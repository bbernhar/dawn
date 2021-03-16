// Copyright 2020 The Dawn Authors
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

#include "dawn_native/d3d12/AdapterD3D12.h"

namespace dawn_native { namespace d3d12 {

    // SharedPipelineCacheEntry

    SharedPipelineCacheEntry::SharedPipelineCacheEntry(
        SharedPipelineCache* caches,
        Ref<dawn_platform::CachedBlob> pipelineCacheBlob,
        std::unique_ptr<PipelineCache> pipelineCache)
        : mPipelineCache(std::move(pipelineCache)),
          mPipelineCacheBlob(std::move(pipelineCacheBlob)),
          mCaches(caches) {
        ASSERT(mCaches != nullptr);
        ASSERT(mPipelineCache != nullptr);
    }

    SharedPipelineCacheEntry::~SharedPipelineCacheEntry() {
        if (mPipelineCacheBlob.Get() != nullptr) {
            mCaches->RemoveCacheEntry(mPipelineCacheBlob.Get());
        }
    }

    PipelineCache* SharedPipelineCacheEntry::GetPipelineCache() {
        ASSERT(mPipelineCache != nullptr);
        return mPipelineCache.get();
    }

    MaybeError SharedPipelineCacheEntry::StorePipelineCache(PersistentCache* persistentCache) {
        ASSERT(mPipelineCache != nullptr);
        return mCaches->StorePipelineCache(mPipelineCache.get(), persistentCache);
    }

    // SharedPipelineCache

    SharedPipelineCache::SharedPipelineCache(Adapter* adapter) : mAdapter(adapter) {
    }

    ResultOrError<Ref<SharedPipelineCacheEntry>> SharedPipelineCache::GetOrCreate(
        PersistentCache* persistentCache) {
        Ref<dawn_platform::CachedBlob> libraryBlob;
        std::unique_ptr<PipelineCache> emptyPipelineCache;

        ComPtr<ID3D12Device> device = mAdapter->GetDevice();

        DAWN_TRY_ASSIGN(libraryBlob,
                        persistentCache->GetOrCreate(
                            mAdapter->GetPipelineCacheKey(), [&](auto doCache) -> MaybeError {
                                // No blob in persistent cache: either the persistent cache was
                                // nuked or disabled (ex. full).
                                DAWN_TRY_ASSIGN(emptyPipelineCache, PipelineCache::Create(device));

                                std::vector<uint8_t> serializedData;
                                DAWN_TRY_ASSIGN(serializedData,
                                                emptyPipelineCache->GetPipelineCacheData());

                                doCache(serializedData.data(), serializedData.size());
                                return {};
                            }));

        // Persistent cache is unable to load blob, do not use shared pipeline cache.
        if (libraryBlob == nullptr) {
            Ref<SharedPipelineCacheEntry> entry = AcquireRef(new SharedPipelineCacheEntry(
                this, /*blob*/ nullptr, std::move(emptyPipelineCache)));
            return std::move(entry);
        }

        ASSERT(libraryBlob.Get() != nullptr && libraryBlob->size() > 0);

        // Use a cached library which corresponds to the blob.
        auto iter = mSharedPipelineCaches.find(libraryBlob.Get());
        if (iter != mSharedPipelineCaches.end()) {
            return Ref<SharedPipelineCacheEntry>(iter->second);
        }

        // Create a new library from the blob or re-use the empty one we created earlier.
        std::unique_ptr<PipelineCache> pipelineCache;
        if (!emptyPipelineCache) {
            DAWN_TRY_ASSIGN(pipelineCache, PipelineCache::Create(device, libraryBlob->data(),
                                                                 libraryBlob->size()));
        } else {
            pipelineCache = std::move(emptyPipelineCache);
        }

        dawn_platform::CachedBlob* libraryBlobPtr = libraryBlob.Get();

        Ref<SharedPipelineCacheEntry> entry = AcquireRef(
            new SharedPipelineCacheEntry(this, std::move(libraryBlob), std::move(pipelineCache)));

        mSharedPipelineCaches.insert({libraryBlobPtr, entry.Get()});

        return std::move(entry);
    }

    void SharedPipelineCache::RemoveCacheEntry(dawn_platform::CachedBlob* libraryBlob) {
        const size_t removedCount = mSharedPipelineCaches.erase(libraryBlob);
        ASSERT(removedCount == 1);
    }

    MaybeError SharedPipelineCache::StorePipelineCache(PipelineCache* pipelineCache,
                                                       PersistentCache* persistentCache) const {
        std::vector<uint8_t> serializedData;
        DAWN_TRY_ASSIGN(serializedData, pipelineCache->GetPipelineCacheData());
        if (serializedData.size() > 0) {
            persistentCache->StoreData(mAdapter->GetPipelineCacheKey(), serializedData.data(),
                                       serializedData.size());
        }
        return {};
    }

    // PipelineCache

    // static
    ResultOrError<std::unique_ptr<PipelineCache>> PipelineCache::Create(
        ComPtr<ID3D12Device> device) {
        return PipelineCache::Create(device, nullptr, 0);
    }

    // static
    ResultOrError<std::unique_ptr<PipelineCache>> PipelineCache::Create(ComPtr<ID3D12Device> device,
                                                                        const uint8_t* libraryData,
                                                                        size_t librarySize) {
        std::unique_ptr<PipelineCache> pipelineCache =
            std::make_unique<PipelineCache>(std::move(device));
        DAWN_TRY(pipelineCache->Initialize(libraryData, librarySize));
        return std::move(pipelineCache);
    }

    MaybeError PipelineCache::Initialize(const uint8_t* libraryData, size_t librarySize) {
        // Support must be queried through the d3d device before calling CreatePipelineLibrary.
        // ID3D12PipelineLibrary was introduced since Windows 10 Anniversary Update (WDDM 2.1+).
        ComPtr<ID3D12Device1> device1;
        if (FAILED(mDevice.As(&device1))) {
            return {};
        }

        DAWN_TRY(CheckHRESULT(
            device1->CreatePipelineLibrary(libraryData, librarySize, IID_PPV_ARGS(&mLibrary)),
            "ID3D12Device1::CreatePipelineLibrary"));

        return {};
    }

    PipelineCache::PipelineCache(ComPtr<ID3D12Device> device) : mDevice(std::move(device)) {
    }

    ResultOrError<std::vector<uint8_t>> PipelineCache::GetPipelineCacheData() const {
        std::vector<uint8_t> libraryData;
        if (mLibrary == nullptr) {
            return std::move(libraryData);
        }

        const size_t librarySize = mLibrary->GetSerializedSize();
        ASSERT(librarySize > 0);

        libraryData.resize(librarySize);
        DAWN_TRY(CheckHRESULT(mLibrary->Serialize(libraryData.data(), libraryData.size()),
                              "ID3D12PipelineLibrary::Serialize"));
        return libraryData;
    }

    size_t PipelineCache::GetPipelineCacheHitCountForTesting() const {
        return mHitCountForTesting;
    }

    // static
    HRESULT PipelineCache::D3D12LoadPipeline(ID3D12PipelineLibrary* library,
                                             LPCWSTR name,
                                             const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
                                             ComPtr<ID3D12PipelineState>& pso) {
        return library->LoadGraphicsPipeline(name, &desc, IID_PPV_ARGS(&pso));
    }

    // static
    HRESULT PipelineCache::D3D12LoadPipeline(ID3D12PipelineLibrary* library,
                                             LPCWSTR name,
                                             const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc,
                                             ComPtr<ID3D12PipelineState>& pso) {
        return library->LoadComputePipeline(name, &desc, IID_PPV_ARGS(&pso));
    }

    // static
    HRESULT PipelineCache::D3D12CreatePipeline(ID3D12Device* device,
                                               const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
                                               ComPtr<ID3D12PipelineState>& pso) {
        return device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso));
    }

    // static
    HRESULT PipelineCache::D3D12CreatePipeline(ID3D12Device* device,
                                               D3D12_COMPUTE_PIPELINE_STATE_DESC desc,
                                               ComPtr<ID3D12PipelineState>& pso) {
        return device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&pso));
    }
}}  // namespace dawn_native::d3d12