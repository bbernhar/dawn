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

#ifndef DAWNNATIVE_D3D12_PIPELINECACHED3D12_H_
#define DAWNNATIVE_D3D12_PIPELINECACHED3D12_H_

#include "common/RefCounted.h"
#include "dawn_native/PersistentCache.h"
#include "dawn_native/d3d12/D3D12Error.h"

#include "dawn_native/d3d12/d3d12_platform.h"

#include <unordered_set>

namespace dawn_native { namespace d3d12 {

    class Adapter;
    class SharedPipelineCaches;

    // Uses a ID3D12PipelineLibrary to cache pipelines in-memory so they can be saved back to disk
    // using the persistent cache. If no pipeline library support, the cache is in passthrough mode.
    class PipelineCache : public RefCounted {
      public:
        PipelineCache() = default;
        PipelineCache(dawn_platform::ScopedCachedBlob pipelineCacheBlob);
        ~PipelineCache() override;

        static ResultOrError<Ref<PipelineCache>> Create(
            ComPtr<ID3D12Device> device,
            SharedPipelineCaches* sharedPipelineCaches,
            dawn_platform::ScopedCachedBlob pipelineCacheBlob);

        // Combines load/store operations into a single call. If the load was successful,
        // a cached PSO corresponding to |descHash| is returned to the caller.
        // Else, a new PSO is created from |desc| and stored back in the pipeline cache.
        template <typename PipelineStateDescT>
        ResultOrError<ComPtr<ID3D12PipelineState>> GetOrCreate(const PipelineStateDescT& desc,
                                                               size_t descHash) {
            ComPtr<ID3D12PipelineState> pso;
            const std::wstring& descHashW = std::to_wstring(descHash);
            if (mLibrary != nullptr) {
                DAWN_TRY(LoadPipeline(descHashW.c_str(), desc, pso));
                if (pso != nullptr) {
                    mHitCountForTesting++;
                    return std::move(pso);
                }
            }

            DAWN_TRY(CreatePipeline(desc, pso));
            if (mLibrary != nullptr) {
                DAWN_TRY(StorePipeline(descHashW.c_str(), pso.Get()));
                mIsLibraryBlobDirty = true;
            }

            return std::move(pso);
        }

        // Called to store back added pipelines back into the persistent cache.
        void StorePipelineCacheData(PersistentCache* persistentCache);

        // Tells this pipeline cache to unlink itself from the adapter.
        void DisconnectFromCache();

        // Functors necessary for the unordered_set<PipelineCache*>-based cache.
        struct HashFunc {
            size_t operator()(const PipelineCache* entry) const;
        };

        struct EqualityFunc {
            bool operator()(const PipelineCache* a, const PipelineCache* b) const;
        };

        size_t GetHitCountForTesting() const;

      private:
        PipelineCache(ComPtr<ID3D12Device> device,
                      SharedPipelineCaches* sharedPipelineCaches,
                      dawn_platform::ScopedCachedBlob pipelineCacheBlob);

        MaybeError Initialize();

        // Overloads per pipeline state descriptor type
        MaybeError LoadPipeline(LPCWSTR name,
                                const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
                                ComPtr<ID3D12PipelineState>& pso);

        MaybeError LoadPipeline(LPCWSTR name,
                                const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc,
                                ComPtr<ID3D12PipelineState>& pso);

        MaybeError CreatePipeline(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
                                  ComPtr<ID3D12PipelineState>& pso);

        MaybeError CreatePipeline(D3D12_COMPUTE_PIPELINE_STATE_DESC desc,
                                  ComPtr<ID3D12PipelineState>& pso);

        MaybeError StorePipeline(LPCWSTR name, ID3D12PipelineState* pso);

        ComPtr<ID3D12Device> mDevice;

        // |mLibraryBlob| is used to create a pipeline library which must remain valid for the
        // lifetime of the |mLibrary|.
        dawn_platform::ScopedCachedBlob mLibraryBlob;
        ComPtr<ID3D12PipelineLibrary> mLibrary;

        SharedPipelineCaches* mSharedPipelineCaches = nullptr;

        size_t mHitCountForTesting = 0;

        // Tells this pipeline cache when |mLibrary| gets modified to avoid unneccessary stores.
        bool mIsLibraryBlobDirty = false;
    };

    // Shares pipelines between devices then persistently stores them back to disk.
    class SharedPipelineCaches {
      public:
        SharedPipelineCaches(Adapter* adapter);
        ~SharedPipelineCaches();

        ResultOrError<Ref<PipelineCache>> GetOrCreate(PersistentCache* persistentCache);

        void StorePipelineCacheData(const uint8_t* data,
                                    size_t size,
                                    PersistentCache* persistentCache);

        void RemovePipelineCache(PipelineCache* pipelineCache);

        void ResetForTesting();

      private:
        using Cache = std::
            unordered_set<PipelineCache*, PipelineCache::HashFunc, PipelineCache::EqualityFunc>;

        Cache mCache;

        Adapter* mAdapter = nullptr;
    };

}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12_PIPELINECACHED3D12_H_