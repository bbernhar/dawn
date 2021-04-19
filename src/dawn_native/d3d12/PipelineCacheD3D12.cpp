// Copyright 2021 The Dawn Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "dawn_native/d3d12/PipelineCacheD3D12.h"

#include "common/HashUtils.h"
#include "dawn_native/d3d12/AdapterD3D12.h"

namespace dawn_native { namespace d3d12 {

    namespace {
        MaybeError CheckLoadPipelineHRESULT(HRESULT result, const char* context) {
            // If PSO does not exist, LoadPipeline returns E_INVALIDARG. Since a PSO cache miss
            // will always create a PSO, this error can be ignored.
            if (result == E_INVALIDARG) {
                return {};
            }
            return CheckHRESULT(result, context);
        }

        MaybeError CheckStorePipelineHRESULT(HRESULT result, const char* context) {
            // If PSO was already stored, StorePipeline returns E_INVALIDARG.
#if defined(_DEBUG)
            // PSO containing shaders compiled using debug options will never match the
            // ones previously stored by the library. Rather then forbid caching,
            // disable storing them to disk and treat this error as a cache miss.
            if (result == E_INVALIDARG) {
                return {};
            }
#endif

            return CheckHRESULT(result, context);
        }

    }  // anonymous namespace

    // SharedPipelineCache

    SharedPipelineCaches::SharedPipelineCaches(Adapter* adapter) : mAdapter(adapter) {
    }

    SharedPipelineCaches::~SharedPipelineCaches() {
        ASSERT(mCache.empty());
    }

    void SharedPipelineCaches::ResetForTesting() {
        for (auto& entry : mCache) {
            entry->DisconnectFromCache();
        }
        mCache.clear();
    }

    void SharedPipelineCaches::StorePipelineCacheData(const uint8_t* data,
                                                      size_t size,
                                                      PersistentCache* persistentCache) {
        persistentCache->StoreData(mAdapter->GetPipelineCacheKey(), data, size);
    }

    ResultOrError<Ref<PipelineCache>> SharedPipelineCaches::GetOrCreate(
        PersistentCache* persistentCache) {
        // If pipeline caching is not supported by driver, use non-shared pipeline cache in
        // passthrough mode.
        if (!mAdapter->GetDeviceInfo().supportsPipelineCaching) {
            return Ref<PipelineCache>(new PipelineCache());
        }

        // Try to use an existing pipeline cache on this adapter.
        const PersistentCacheKey pipelineCacheKey = mAdapter->GetPipelineCacheKey();
        dawn_platform::ScopedCachedBlob pipelineCacheData = {};
        DAWN_TRY_ASSIGN(pipelineCacheData, persistentCache->GetOrCreate(
                                               pipelineCacheKey, [&](auto doCache) -> MaybeError {
                                                   doCache(nullptr, 0);
                                                   return {};
                                               }));

        PipelineCache blueprint(pipelineCacheData);
        auto iter = mCache.find(&blueprint);
        if (iter != mCache.end()) {
            return Ref<PipelineCache>(*iter);
        }

        // GetOrCreate() doesn't return the blob stored by |doCache| but the blob is
        // needed to create the pipeline cache on this adapter.
        if (pipelineCacheData == nullptr) {
            pipelineCacheData = persistentCache->LoadData(pipelineCacheKey);
        }

        Ref<PipelineCache> pipelineCache;
        DAWN_TRY_ASSIGN(pipelineCache,
                        PipelineCache::Create(mAdapter->GetDevice(), this, pipelineCacheData));

        // If the pipeline cache data cannot be stored, do not use this pipeline cache on this
        // adapter This is because pipelines added between devices would overwrite each other if
        // if they cannot be shared.
        if (pipelineCacheData.Get() != nullptr) {
            mCache.insert(pipelineCache.Get());
        } else {
            pipelineCache->DisconnectFromCache();
        }

        return std::move(pipelineCache);
    }

    void SharedPipelineCaches::RemovePipelineCache(PipelineCache* pipelineCache) {
        ASSERT(pipelineCache->GetRefCountForTesting() == 0);
        const size_t removedCount = mCache.erase(pipelineCache);
        ASSERT(removedCount == 1);
    }

    // PipelineCache

    // static
    ResultOrError<Ref<PipelineCache>> PipelineCache::Create(
        ComPtr<ID3D12Device> device,
        SharedPipelineCaches* sharedPipelineCaches,
        dawn_platform::ScopedCachedBlob pipelineCacheBlob) {
        Ref<PipelineCache> pipelineCache = AcquireRef(new PipelineCache(
            std::move(device), sharedPipelineCaches, std::move(pipelineCacheBlob)));
        DAWN_TRY(pipelineCache->Initialize());
        return std::move(pipelineCache);
    }

    MaybeError PipelineCache::Initialize() {
        // If a cached blob exists, create a new library from it by passing it to
        // CreatePipelineLibrary. If no cached blob exists, passing a nullptr with zero size
        // initializes an empty library.
        size_t librarySize = 0;
        const uint8_t* libraryData = nullptr;
        if (mLibraryBlob.Get() != nullptr) {
            librarySize = mLibraryBlob->size();
            libraryData = mLibraryBlob->data();
        }

        ComPtr<ID3D12Device1> device1;
        DAWN_TRY(CheckHRESULT(mDevice.As(&device1),
                              "D3D12 QueryInterface ID3D12Device to ID3D12Device1"));

        DAWN_TRY(CheckHRESULT(
            device1->CreatePipelineLibrary(libraryData, librarySize, IID_PPV_ARGS(&mLibrary)),
            "ID3D12Device1::CreatePipelineLibrary"));

        return {};
    }

    PipelineCache::PipelineCache(dawn_platform::ScopedCachedBlob pipelineCacheBlob)
        : mLibraryBlob(std::move(pipelineCacheBlob)) {
    }

    PipelineCache::PipelineCache(ComPtr<ID3D12Device> device,
                                 SharedPipelineCaches* sharedPipelineCaches,
                                 dawn_platform::ScopedCachedBlob pipelineCacheBlob)
        : mDevice(std::move(device)),
          mLibraryBlob(std::move(pipelineCacheBlob)),
          mSharedPipelineCaches(sharedPipelineCaches) {
        ASSERT(mSharedPipelineCaches != nullptr);
    }

    PipelineCache::~PipelineCache() {
        if (mSharedPipelineCaches != nullptr) {
            mSharedPipelineCaches->RemovePipelineCache(this);
        }
    }

    void PipelineCache::DisconnectFromCache() {
        ASSERT(mSharedPipelineCaches != nullptr);
        mSharedPipelineCaches = nullptr;
    }

    void PipelineCache::StorePipelineCacheData(PersistentCache* persistentCache) {
        // Pipelines that are not shared on the adapter are never persistently stored.
        if (mSharedPipelineCaches == nullptr) {
            return;
        }

        // Check if library is already up-to-date.
        if (!mIsLibraryBlobDirty) {
            return;
        }

        // Do not persist pipelines in debug builds since they could contain regenerated shaders
        // that cannot ever be loaded.
#if defined(_DEBUG)
        if (mIsLibraryBlobDirty) {
            return;
        }
#endif
        // Library must exist if |mIsLibraryBlobDirty|.
        ASSERT(mLibrary != nullptr);

        const size_t librarySize = mLibrary->GetSerializedSize();
        ASSERT(librarySize > 0);

        // TODO(bryan.bernhart@intel.com): Consider to resize and reuse this buffer.
        std::vector<uint8_t> libraryData(librarySize);
        if (FAILED(mLibrary->Serialize(libraryData.data(), libraryData.size()))) {
            return;
        }

        mSharedPipelineCaches->StorePipelineCacheData(libraryData.data(), libraryData.size(),
                                                      persistentCache);

        mIsLibraryBlobDirty = false;
        return;
    }

    size_t PipelineCache::HashFunc::operator()(const PipelineCache* entry) const {
        size_t hash = 0;
        HashCombine(&hash, entry->mLibraryBlob.Get());
        return hash;
    }

    bool PipelineCache::EqualityFunc::operator()(const PipelineCache* a,
                                                 const PipelineCache* b) const {
        return a->mLibraryBlob == b->mLibraryBlob;
    }

    size_t PipelineCache::GetHitCountForTesting() const {
        return mHitCountForTesting;
    }

    MaybeError PipelineCache::StorePipeline(LPCWSTR name, ID3D12PipelineState* pso) {
        return CheckStorePipelineHRESULT(mLibrary->StorePipeline(name, pso),
                                         "ID3D12PipelineLibrary::StorePipeline");
    }

    MaybeError PipelineCache::LoadPipeline(LPCWSTR name,
                                           const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
                                           ComPtr<ID3D12PipelineState>& pso) {
        return CheckLoadPipelineHRESULT(
            mLibrary->LoadGraphicsPipeline(name, &desc, IID_PPV_ARGS(&pso)),
            "ID3D12PipelineLibrary::LoadGraphicsPipeline");
    }

    MaybeError PipelineCache::LoadPipeline(LPCWSTR name,
                                           const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc,
                                           ComPtr<ID3D12PipelineState>& pso) {
        return CheckLoadPipelineHRESULT(
            mLibrary->LoadComputePipeline(name, &desc, IID_PPV_ARGS(&pso)),
            "ID3D12PipelineLibrary::LoadComputePipeline");
    }

    MaybeError PipelineCache::CreatePipeline(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
                                             ComPtr<ID3D12PipelineState>& pso) {
        return CheckHRESULT(mDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso)),
                            "ID3D12Device::CreateGraphicsPipelineState");
    }

    MaybeError PipelineCache::CreatePipeline(D3D12_COMPUTE_PIPELINE_STATE_DESC desc,
                                             ComPtr<ID3D12PipelineState>& pso) {
        return CheckHRESULT(mDevice->CreateComputePipelineState(&desc, IID_PPV_ARGS(&pso)),
                            "ID3D12Device::CreateComputePipelineState");
    }
}}  // namespace dawn_native::d3d12