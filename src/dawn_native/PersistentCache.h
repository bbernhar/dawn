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

#ifndef DAWNNATIVE_PERSISTENTCACHE_H_
#define DAWNNATIVE_PERSISTENTCACHE_H_

#include "dawn_native/Error.h"
#include "dawn_platform/DawnPlatform.h"

#include <vector>

namespace dawn_platform {
    class CachingInterface;
}

namespace dawn_native {

    using PersistentCacheKey = std::vector<uint8_t>;

    class DeviceBase;

    enum class PersistentKeyType { Shader, PipelineCache };

    class PersistentCache {
      public:
        PersistentCache(DeviceBase* device);

        // Combines load/store operations into a single call.
        // If the load was successful, a non-empty blob is returned to the caller.
        // Else, the creation callback |createFn| gets invoked with a callback
        // |doCache| to store the newly created blob back in the cache.
        //
        // Example usage:
        //
        // CachedBlob cachedBlob = {};
        // DAWN_TRY_ASSIGN(cachedBlob, GetOrCreate(key, [&](auto doCache)) {
        //      // Create a new blob to be stored
        //      doCache(newBlobPtr, newBlobSize); // store
        // }));
        //
        template <typename CreateFn>
        ResultOrError<Ref<dawn_platform::CachedBlob>> GetOrCreate(const PersistentCacheKey& key,
                                                                  CreateFn&& createFn) {
            // Attempt to load an existing blob from the cache.
            Ref<dawn_platform::CachedBlob> blob = LoadData(key);
            if (blob != nullptr) {
                return std::move(blob);
            }

            // Allow the caller to create a new blob to be stored for the given key.
            DAWN_TRY(createFn([this, key](const void* value, size_t size) {
                this->StoreData(key, value, size);
            }));
            blob = LoadData(key);
            return std::move(blob);
        }

        // PersistentCache impl
        Ref<dawn_platform::CachedBlob> LoadData(const PersistentCacheKey& key);
        void StoreData(const PersistentCacheKey& key, const void* value, size_t size);

      private:
        dawn_platform::CachingInterface* GetPlatformCache();

        DeviceBase* mDevice = nullptr;

        dawn_platform::CachingInterface* mCache = nullptr;
    };
}  // namespace dawn_native

#endif  // DAWNNATIVE_PERSISTENTCACHE_H_
