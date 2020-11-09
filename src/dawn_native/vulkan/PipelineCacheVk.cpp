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

#include "dawn_native/vulkan/PipelineCacheVk.h"

#include "common/HashUtils.h"

#include "dawn_native/vulkan/AdapterVk.h"
#include "dawn_native/vulkan/DeviceVk.h"
#include "dawn_native/vulkan/VulkanError.h"

#include <sstream>

namespace dawn_native { namespace vulkan {

    PipelineCache::PipelineCache(Device* device)
        : mDevice(device), mPipelineCacheKey(CreateCacheKey()) {
    }

    PipelineCache::~PipelineCache() {
        if (mHandle != VK_NULL_HANDLE) {
            mDevice->fn.DestroyPipelineCache(mDevice->GetVkDevice(), mHandle, nullptr);
            mHandle = VK_NULL_HANDLE;
        }
    }

    MaybeError PipelineCache::loadPipelineCacheIfNecessary() {
        if (mHandle != VK_NULL_HANDLE) {
            return {};
        }

        std::unique_ptr<uint8_t[]> initialData;
        const size_t cacheSize = mDevice->GetPersistentCache()->getDataSize(mPipelineCacheKey);
        if (cacheSize > 0) {
            initialData.reset(new uint8_t[cacheSize]);
            mDevice->GetPersistentCache()->loadData(mPipelineCacheKey, initialData.get(),
                                                    cacheSize);
        }

        VkPipelineCacheCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
        createInfo.flags = 0;
        createInfo.initialDataSize = cacheSize;
        createInfo.pInitialData = initialData.get();

        ASSERT(initialData == nullptr || cacheSize > 0);

        // TODO(bryan.bernhart@intel.com): Consider giving this an allocator if memory usage is
        // high.
        DAWN_TRY(CheckVkSuccess(mDevice->fn.CreatePipelineCache(mDevice->GetVkDevice(), &createInfo,
                                                                nullptr, &*mHandle),
                                "vkCreatePipelineCache"));

        return {};
    }

    MaybeError PipelineCache::storePipelineCache() {
        if (mHandle == VK_NULL_HANDLE) {
            return {};
        }

        // vkGetPipelineCacheData has two operations. One to get the cache size (where pData is
        // nullptr) and the other to get the cache data (pData != null and size > 0).
        size_t cacheSize = 0;
        DAWN_TRY(CheckVkSuccess(
            mDevice->fn.GetPipelineCacheData(mDevice->GetVkDevice(), mHandle, &cacheSize, nullptr),
            "vkCreatePipelineCache"));

        ASSERT(cacheSize > 0);

        // vkGetPipelineCacheData can partially write cache data. Since the partially written data
        // size will be saved in |cacheSize|, load the pipeline cache into a zeroed buffer that is
        // the maximum size then use it to store only the partially written data.
        std::vector<uint8_t> writtenData(cacheSize, 0);
        VkResult result = VkResult::WrapUnsafe(mDevice->fn.GetPipelineCacheData(
            mDevice->GetVkDevice(), mHandle, &cacheSize, writtenData.data()));
        if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
            return DAWN_INTERNAL_ERROR("vkGetPipelineCacheData");
        }

        // Written cache data cannot exceed the cache size.
        // TODO(bryan.bernhart@intel.com): Figure out if it's possible for the cache size to change
        // between calls to vkGetPipelineCacheData.
        ASSERT(cacheSize <= writtenData.size());

        // Written data should be at-least be the size of the cache version header.
        // See VK_PIPELINE_CACHE_HEADER_VERSION_ONE in vkGetPipelineCacheData.
        // https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPipelineCacheData.html
        ASSERT(cacheSize > 16 + VK_UUID_SIZE);

        mDevice->GetPersistentCache()->storeData(mPipelineCacheKey, writtenData.data(), cacheSize);

        return {};
    }

    ResultOrError<VkPipelineCache> PipelineCache::GetVkPipelineCache() {
        DAWN_TRY(loadPipelineCacheIfNecessary());
        return std::move(mHandle);
    }

    PersistentCacheKey PipelineCache::CreateCacheKey() const {
        std::stringstream stream;

        PCIInfo info = mDevice->GetAdapter()->GetPCIInfo();
        {
            stream << std::hex << info.deviceId;
            stream << std::hex << info.vendorId;
        }

        // Append the pipeline cache ID to ensure the retrieved pipeline cache is compatible.
        // https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPipelineCacheData.html
        {
            const PCIExtendedInfo& info = ToBackend(mDevice->GetAdapter())->GetPCIExtendedInfo();
            for (const uint32_t x : info.pipelineCacheUUID) {
                stream << std::hex << x;
            }
        }

        const std::string keyStr(stream.str());
        return PersistentCacheKey(keyStr.begin(), keyStr.end());
    }

}}  // namespace dawn_native::vulkan