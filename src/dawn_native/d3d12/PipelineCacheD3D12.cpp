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

#include <sstream>

namespace dawn_native { namespace d3d12 {

    // static
    ResultOrError<std::unique_ptr<PipelineCache>> PipelineCache::Create(Device* device,
                                                                        bool isLibrarySupported) {
        std::unique_ptr<PipelineCache> pipelineCache = std::make_unique<PipelineCache>(device);
        DAWN_TRY(pipelineCache->Initialize(isLibrarySupported));
        return std::move(pipelineCache);
    }

    MaybeError PipelineCache::Initialize(bool isLibrarySupported) {
        if (!isLibrarySupported) {
            return {};
        }

        // Create a library from data using the persistant cache. If the library does not exist,
        // an empty library will be created.
        mLibraryBlob = mDevice->GetPersistentCache()->LoadData(mPipelineCacheKey);
        ASSERT(mLibraryBlob.buffer == nullptr || mLibraryBlob.bufferSize > 0);

        DAWN_TRY(CheckHRESULT(
            mDevice->GetD3D12Device1()->CreatePipelineLibrary(
                mLibraryBlob.buffer.get(), mLibraryBlob.bufferSize, IID_PPV_ARGS(&mLibrary)),
            "ID3D12Device1::CreatePipelineLibrary"));

        return {};
    }

    PipelineCache::PipelineCache(Device* device)
        : mDevice(device),
          mPipelineCacheKey(CreatePipelineCacheKey(ToBackend(mDevice->GetAdapter()))) {
    }

    MaybeError PipelineCache::StorePipelines() {
        if (mLibrary == nullptr) {
            return {};
        }

        const size_t librarySize = mLibrary->GetSerializedSize();
        std::unique_ptr<uint8_t[]> pSerializedData(new uint8_t[librarySize]);
        DAWN_TRY(CheckHRESULT(mLibrary->Serialize(pSerializedData.get(), librarySize),
                              "ID3D12PipelineLibrary::Serialize"));

        mDevice->GetPersistentCache()->StoreData(mPipelineCacheKey, pSerializedData.get(),
                                                 librarySize);
        return {};
    }

    size_t PipelineCache::GetPipelineCacheHitCountForTesting() const {
        return mCacheHitCountForTesting;
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

    PersistentCacheKey PipelineCache::CreatePipelineCacheKey(const Adapter* adapter) const {
        std::stringstream stream;

        const PCIInfo& info = adapter->GetPCIInfo();
        stream << std::hex << info.deviceId;
        stream << std::hex << info.vendorId;

        const PCIExtendedInfo& extInfo = adapter->GetPCIExtendedInfo();
        stream << std::hex << extInfo.subSysId;

        return PersistentCacheKey(std::istreambuf_iterator<char>{stream},
                                  std::istreambuf_iterator<char>{});
    }
}}  // namespace dawn_native::d3d12