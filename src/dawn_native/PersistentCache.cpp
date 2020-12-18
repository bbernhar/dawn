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

namespace dawn_native {

    PersistentCache::PersistentCache(DeviceBase* device)
        : mDevice(device), mCache(GetPlatformCache()) {
    }

    Ref<dawn_platform::ScopedCachedBlob> PersistentCache::LoadData(const PersistentCacheKey& key) {
        if (mCache == nullptr) {
            return {};
        }
        return mCache->LoadData(reinterpret_cast<WGPUDevice>(mDevice), key.data(), key.size());
    }

    void PersistentCache::StoreData(const PersistentCacheKey& key, const void* value, size_t size) {
        if (mCache == nullptr) {
            return;
        }
        ASSERT(value != nullptr);
        ASSERT(size > 0);
        mCache->StoreData(reinterpret_cast<WGPUDevice>(mDevice), key.data(), key.size(), value,
                          size);
    }

    dawn_platform::CachingInterface* PersistentCache::GetPlatformCache() {
        // TODO(dawn:549): Create a fingerprint of concatenated version strings (ex. Tint commit
        // hash, Dawn commit hash). This will be used by the client so it may know when to discard
        // previously cached Dawn objects should this fingerprint change.
        dawn_platform::Platform* platform = mDevice->GetPlatform();
        if (platform != nullptr) {
            return platform->GetCachingInterface(/*fingerprint*/ nullptr, /*fingerprintSize*/ 0);
        }
        return nullptr;
    }
}  // namespace dawn_native