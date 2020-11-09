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

#include "dawn_native/PersistentCache.h"

#include "common/Assert.h"
#include "dawn_native/Device.h"
#include "dawn_platform/DawnPlatform.h"

#include <string>

namespace dawn_native {

    PersistentCache::PersistentCache(DeviceBase* device) : mDevice(device) {
    }

    size_t PersistentCache::loadData(const PersistentCacheKey& key, void* value, size_t size) {
        dawn_platform::CachingInterface* cache = mDevice->GetPlatform()->GetCachingInterface();
        if (cache == nullptr) {
            return 0;
        }
        return cache->loadData(reinterpret_cast<WGPUDevice>(mDevice), key.data(), key.size(), value,
                               size);
    }

    bool PersistentCache::storeData(const PersistentCacheKey& key, const void* value, size_t size) {
        dawn_platform::CachingInterface* cache = mDevice->GetPlatform()->GetCachingInterface();
        if (cache == nullptr) {
            return false;
        }
        ASSERT(value != nullptr);
        ASSERT(size > 0);
        return cache->storeData(reinterpret_cast<WGPUDevice>(mDevice), key.data(), key.size(),
                                value, size);
    }

    size_t PersistentCache::getDataSize(const PersistentCacheKey& key) {
        return loadData(key, nullptr, 0);
    }
}  // namespace dawn_native