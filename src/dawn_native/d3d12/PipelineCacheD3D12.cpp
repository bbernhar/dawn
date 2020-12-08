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

        // Create a library with data from the persistant cache. If a library does not exist,
        // create an empty library by passing a nullptr blob of zero size to CreatePipelineLibrary.
        mLibraryBlob =
            mDevice->GetPersistentCache()->LoadData(mDevice->GetAdapter()->GetPipelineCacheKey());
        ASSERT(mLibraryBlob.buffer == nullptr || mLibraryBlob.bufferSize > 0);

        DAWN_TRY(CheckHRESULT(
            mDevice->GetD3D12Device1()->CreatePipelineLibrary(
                mLibraryBlob.buffer.get(), mLibraryBlob.bufferSize, IID_PPV_ARGS(&mLibrary)),
            "ID3D12Device1::CreatePipelineLibrary"));

        return {};
    }

    PipelineCache::PipelineCache(Device* device) : mDevice(device) {
    }

    void PipelineCache::StorePipelines() {
        if (mLibrary == nullptr) {
            return;
        }

        const size_t librarySize = mLibrary->GetSerializedSize();
        ASSERT(librarySize > 0);

        std::unique_ptr<uint8_t[]> pSerializedData(new uint8_t[librarySize]);
        ASSERT_SUCCESS(mLibrary->Serialize(pSerializedData.get(), librarySize));

        mDevice->GetPersistentCache()->StoreData(mDevice->GetAdapter()->GetPipelineCacheKey(),
                                                 pSerializedData.get(), librarySize);
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