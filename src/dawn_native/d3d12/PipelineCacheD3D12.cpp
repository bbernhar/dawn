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
#include "dawn_native/d3d12/D3D12Error.h"
#include "dawn_native/d3d12/DeviceD3D12.h"

#include <sstream>

namespace dawn_native { namespace d3d12 {

    PipelineCache::PipelineCache(Device* device, bool isPipelineLibrarySupported)
        : mDevice(device),
          mPipelineCachingEnabled(isPipelineLibrarySupported && device->IsPipelineCachingEnabled()),
          mPipelineCacheKey(CreateCacheKey()) {
    }

    MaybeError PipelineCache::storePipelineCache() {
        if (mLibrary == nullptr) {
            return {};
        }

        const size_t librarySize = mLibrary->GetSerializedSize();
        std::unique_ptr<uint8_t[]> pSerializedData(new uint8_t[librarySize]);
        DAWN_TRY(CheckHRESULT(mLibrary->Serialize(pSerializedData.get(), librarySize),
                              "ID3D12PipelineLibrary::Serialize"));

        mDevice->GetPersistentCache()->storeData(mPipelineCacheKey, pSerializedData.get(),
                                                 librarySize);
        return {};
    }

    MaybeError PipelineCache::loadPipelineCacheIfNecessary() {
        if (mLibrary != nullptr) {
            return {};
        }

        const size_t librarySize = mDevice->GetPersistentCache()->getDataSize(mPipelineCacheKey);
        if (librarySize > 0) {
            mLibraryData.reset(new uint8_t[librarySize]);
            mDevice->GetPersistentCache()->loadData(mPipelineCacheKey, mLibraryData.get(),
                                                    librarySize);
        }

        ASSERT(mLibraryData == nullptr || librarySize > 0);

        DAWN_TRY(CheckHRESULT(mDevice->GetD3D12Device1()->CreatePipelineLibrary(
                                  mLibraryData.get(), librarySize, IID_PPV_ARGS(&mLibrary)),
                              "ID3D12Device1::CreatePipelineLibrary"));

        return {};
    }

    ResultOrError<ComPtr<ID3D12PipelineState>> PipelineCache::getOrCreateGraphicsPipeline(
        const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
        size_t descKey,
        bool usePipelineCache) {
        ComPtr<ID3D12PipelineState> pso;
        const bool usePipelineLibrary = usePipelineCache && mPipelineCachingEnabled;

        const std::wstring descKeyW = std::to_wstring(descKey);
        if (usePipelineLibrary) {
            DAWN_TRY(loadPipelineCacheIfNecessary());

            HRESULT hr =
                mLibrary->LoadGraphicsPipeline(descKeyW.c_str(), &desc, IID_PPV_ARGS(&pso));
            if (SUCCEEDED(hr)) {
                mCacheHitCount++;
                return std::move(pso);
            }

            // Only E_INVALIDARG is considered cache-miss.
            if (hr != E_INVALIDARG) {
                DAWN_TRY(CheckHRESULT(hr, "ID3D12PipelineLibrary::LoadGraphicsPipeline"));
            }
        }

        DAWN_TRY(CheckHRESULT(
            mDevice->GetD3D12Device()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso)),
            "ID3D12Device::CreateGraphicsPipelineState"));
        if (usePipelineLibrary) {
            DAWN_TRY(CheckHRESULT(mLibrary->StorePipeline(descKeyW.c_str(), pso.Get()),
                                  "ID3D12PipelineLibrary::StorePipeline"));
        }

        return std::move(pso);
    }

    ResultOrError<ComPtr<ID3D12PipelineState>> PipelineCache::getOrCreateComputePipeline(
        const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc,
        size_t descKey,
        bool usePipelineCache) {
        ComPtr<ID3D12PipelineState> pso;
        const bool usePipelineLibrary = usePipelineCache && mPipelineCachingEnabled;
        const std::wstring descKeyW = std::to_wstring(descKey);
        if (usePipelineLibrary) {
            DAWN_TRY(loadPipelineCacheIfNecessary());

            HRESULT hr = mLibrary->LoadComputePipeline(descKeyW.c_str(), &desc, IID_PPV_ARGS(&pso));
            if (SUCCEEDED(hr)) {
                mCacheHitCount++;
                return std::move(pso);
            }

            // Only E_INVALIDARG is considered cache-miss.
            if (hr != E_INVALIDARG) {
                DAWN_TRY(CheckHRESULT(hr, "ID3D12PipelineLibrary::LoadComputePipeline"));
            }
        }

        DAWN_TRY(CheckHRESULT(
            mDevice->GetD3D12Device()->CreateComputePipelineState(&desc, IID_PPV_ARGS(&pso)),
            "ID3D12Device::CreateComputePipelineState"));
        if (usePipelineLibrary) {
            DAWN_TRY(CheckHRESULT(mLibrary->StorePipeline(descKeyW.c_str(), pso.Get()),
                                  "ID3D12PipelineLibrary::StorePipeline"));
        }

        return std::move(pso);
    }

    size_t PipelineCache::GetPipelineCacheHitCountForTesting() const {
        return mCacheHitCount;
    }

    PersistentCacheKey PipelineCache::CreateCacheKey() const {
        Adapter* adapter = ToBackend(mDevice->GetAdapter());
        std::stringstream stream;
        {
            const PCIInfo& info = adapter->GetPCIInfo();
            stream << std::hex << info.deviceId;
            stream << std::hex << info.vendorId;
        }
        {
            const PCIExtendedInfo& info = adapter->GetPCIExtendedInfo();
            stream << std::hex << info.subSysId;
        }

        const std::string keyStr(stream.str());
        return PersistentCacheKey(keyStr.begin(), keyStr.end());
    }
}}  // namespace dawn_native::d3d12