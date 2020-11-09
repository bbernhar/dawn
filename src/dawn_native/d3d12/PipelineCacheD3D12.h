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

#include "dawn_native/Error.h"
#include "dawn_native/PersistentCache.h"
#include "dawn_native/d3d12/d3d12_platform.h"

namespace dawn_native { namespace d3d12 {

    class Device;

    // Uses a pipeline library to cache baked pipelines to disk using a persistent cache.
    class PipelineCache {
      public:
        PipelineCache(Device* device, bool isPipelineLibrarySupported);
        ~PipelineCache() = default;

        ResultOrError<ComPtr<ID3D12PipelineState>> getOrCreateGraphicsPipeline(
            const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
            size_t descKey,
            bool usePipelineCache);

        ResultOrError<ComPtr<ID3D12PipelineState>> getOrCreateComputePipeline(
            const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc,
            size_t descKey,
            bool usePipelineCache);

        MaybeError storePipelineCache();

        size_t GetPipelineCacheHitCountForTesting() const;

      private:
        MaybeError loadPipelineCacheIfNecessary();
        PersistentCacheKey CreateCacheKey() const;

        Device* mDevice = nullptr;

        bool mPipelineCachingEnabled = false;

        ComPtr<ID3D12PipelineLibrary> mLibrary;
        std::unique_ptr<uint8_t[]> mLibraryData;  // Cannot outlive |mLibrary|

        size_t mCacheHitCount = 0;
        PersistentCacheKey mPipelineCacheKey;
    };
}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12_PIPELINECACHED3D12_H_