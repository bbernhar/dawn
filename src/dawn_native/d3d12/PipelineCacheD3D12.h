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

#ifndef DAWNNATIVE_D3D12_PIPELINECACHED3D12_H_
#define DAWNNATIVE_D3D12_PIPELINECACHED3D12_H_

#include "dawn_native/PersistentCache.h"
#include "dawn_native/d3d12/D3D12Error.h"
#include "dawn_native/d3d12/DeviceD3D12.h"

namespace dawn_native { namespace d3d12 {

    class Device;

    // Uses a pipeline library to cache baked pipelines to disk using a persistent cache.
    class PipelineCache {
      public:
        static ResultOrError<std::unique_ptr<PipelineCache>> Create(Device* device,
                                                                    bool isLibrarySupported);

        PipelineCache(Device* device);

        // Combines load/store operations into a single call. If the load was successful,
        // a cached PSO corresponding to |descHash| is returned to the caller.
        // Else, a new PSO is created from |desc| and stored back in the pipeline cache.
        // If the pipeline uses uncached debug shaders then |doCache| must be false.
        template <typename PipelineStateDescT>
        ResultOrError<ComPtr<ID3D12PipelineState>> GetOrCreate(const PipelineStateDescT& desc,
                                                               size_t descHash,
                                                               bool doCache) {
            ComPtr<ID3D12PipelineState> pso;
            const std::wstring& descHashW = std::to_wstring(descHash);
            if (mLibrary != nullptr && doCache) {
                DAWN_TRY(CheckLoadPipelineHRESULT(
                    D3D12LoadPipeline(mLibrary.Get(), descHashW.c_str(), desc, pso),
                    "ID3D12PipelineLibrary::Load*Pipeline"));
                if (pso != nullptr) {
                    return std::move(pso);
                }
            }

            DAWN_TRY(CheckHRESULT(D3D12CreatePipeline(mDevice->GetD3D12Device(), desc, pso),
                                  "ID3D12Device::Create*PipelineState"));
            if (mLibrary != nullptr && doCache) {
                DAWN_TRY(CheckHRESULT(mLibrary->StorePipeline(descHashW.c_str(), pso.Get()),
                                      "ID3D12PipelineLibrary::StorePipeline"));
            }

            return std::move(pso);
        }

        void StorePipelines();

      private:
        MaybeError Initialize(bool isLibrarySupported);

        // Overloads per pipeline state descriptor type
        static HRESULT D3D12LoadPipeline(ID3D12PipelineLibrary* library,
                                         LPCWSTR name,
                                         const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
                                         ComPtr<ID3D12PipelineState>& pso);

        static HRESULT D3D12LoadPipeline(ID3D12PipelineLibrary* library,
                                         LPCWSTR name,
                                         const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc,
                                         ComPtr<ID3D12PipelineState>& pso);

        static HRESULT D3D12CreatePipeline(ID3D12Device* device,
                                           const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
                                           ComPtr<ID3D12PipelineState>& pso);

        static HRESULT D3D12CreatePipeline(ID3D12Device* device,
                                           D3D12_COMPUTE_PIPELINE_STATE_DESC desc,
                                           ComPtr<ID3D12PipelineState>& pso);

        Device* mDevice = nullptr;

        ComPtr<ID3D12PipelineLibrary> mLibrary;
        ScopedCachedBlob mLibraryBlob;  // Cannot outlive |mLibrary|
    };
}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12_PIPELINECACHED3D12_H_