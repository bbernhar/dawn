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

#include <functional>
#include <vector>

namespace dawn_platform {
    class CachingInterface;
}

namespace dawn_native {

    using PersistentCacheKey = std::vector<uint8_t>;

    struct ScopedCachedBlob {
        std::unique_ptr<uint8_t[]> buffer;
        size_t bufferSize = 0;
    };

    class DeviceBase;

    class PersistentCache {
      public:
        PersistentCache(DeviceBase* device);

        using DoCache = std::function<bool(const void* buffer, size_t size)>;

        // Combines load/store operations into a single call.
        // If the load was successfull, the a blob is returned back to the caller.
        // Else, the creation call-back |createFn| gets invoked with a call-back
        // |doCache| to store the newly created blob in the cache.
        ResultOrError<ScopedCachedBlob> LoadFromCacheOrCreate(
            const PersistentCacheKey& key,
            std::function<MaybeError(DoCache doCache)> createFn) {
            ScopedCachedBlob blob = {};
            blob.bufferSize = getDataSize(key);
            if (blob.bufferSize > 0) {
                blob.buffer.reset(new uint8_t[blob.bufferSize]);
                loadData(key, blob.buffer.get(), blob.bufferSize);
                return std::move(blob);
            }
            auto doCacheFn = std::bind(&PersistentCache::storeData, this, key,
                                       std::placeholders::_1, std::placeholders::_2);
            DAWN_TRY(createFn(doCacheFn));
            return std::move(blob);
        }

        // PersistentCache impl
        size_t loadData(const PersistentCacheKey& key, void* value, size_t size);
        bool storeData(const PersistentCacheKey& key, const void* value, size_t size);

        size_t getDataSize(const PersistentCacheKey& key);

      private:
        DeviceBase* mDevice = nullptr;
    };
}  // namespace dawn_native

#endif  // DAWNNATIVE_PERSISTENTCACHE_H_