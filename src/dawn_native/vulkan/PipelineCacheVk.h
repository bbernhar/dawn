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

#ifndef DAWNNATIVE_VULKAN_PIPELINECACHEVK_H_
#define DAWNNATIVE_VULKAN_PIPELINECACHEVK_H_

#include "common/vulkan_platform.h"
#include "dawn_native/Error.h"
#include "dawn_native/PersistentCache.h"

namespace dawn_native { namespace vulkan {

    class Device;

    // Wrapper for VkPipelineCache to cache baked pipelines to disk using a persistent cache.
    class PipelineCache {
      public:
        PipelineCache(Device* device);
        ~PipelineCache();

        MaybeError storePipelineCache();

        ResultOrError<VkPipelineCache> GetVkPipelineCache();

      private:
        MaybeError loadPipelineCacheIfNecessary();
        PersistentCacheKey CreateCacheKey() const;

        Device* mDevice = nullptr;

        VkPipelineCache mHandle = VK_NULL_HANDLE;

        PersistentCacheKey mPipelineCacheKey;
    };
}}  // namespace dawn_native::vulkan

#endif  // DAWNNATIVE_VULKAN_PIPELINECACHEVK_H_