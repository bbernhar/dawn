// Copyright 2019 The Dawn Authors
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

#ifndef DAWNPLATFORM_DAWNPLATFORM_H_
#define DAWNPLATFORM_DAWNPLATFORM_H_

#include "dawn_platform/dawn_platform_export.h"

#include <cstddef>
#include <cstdint>

#include <dawn/webgpu.h>

#include <memory>
#include "common/RefCounted.h"

namespace dawn_platform {

    class ScopedCachedBlob final : public RefCounted {
      public:
        ScopedCachedBlob() = default;
        ScopedCachedBlob(const uint8_t* data, size_t size) {
            buffer.reset(new uint8_t[size]);
            memcpy(buffer.get(), data, size);
            bufferSize = size;
        }
        const uint8_t* data() const {
            return buffer.get();
        }
        size_t size() const {
            return bufferSize;
        }

        std::unique_ptr<uint8_t[]> buffer;
        size_t bufferSize = 0;
    };

    enum class TraceCategory {
        General,     // General trace events
        Validation,  // Dawn validation
        Recording,   // Native command recording
        GPUWork,     // Actual GPU work
    };

    class DAWN_PLATFORM_EXPORT CachingInterface {
      public:
        CachingInterface();
        virtual ~CachingInterface();

        // LoadData has two modes. The first mode is used to get a value which
        // corresponds to the |key|. The |valueOut| is a caller provided buffer
        // allocated to the size |valueSize| which is loaded with data of the
        // size returned. The second mode is used to query for the existence of
        // the |key| where |valueOut| is nullptr and |valueSize| must be 0.
        // The return size is non-zero if the |key| exists.
        virtual Ref<dawn_platform::ScopedCachedBlob> LoadData(const WGPUDevice device,
                                                              const void* key,
                                                              size_t keySize) = 0;

        // StoreData puts a |value| in the cache which corresponds to the |key|.
        virtual void StoreData(const WGPUDevice device,
                               const void* key,
                               size_t keySize,
                               const void* value,
                               size_t valueSize) = 0;

      private:
        CachingInterface(const CachingInterface&) = delete;
        CachingInterface& operator=(const CachingInterface&) = delete;
    };

    class DAWN_PLATFORM_EXPORT Platform {
      public:
        Platform();
        virtual ~Platform();

        virtual const unsigned char* GetTraceCategoryEnabledFlag(TraceCategory category);

        virtual double MonotonicallyIncreasingTime();

        virtual uint64_t AddTraceEvent(char phase,
                                       const unsigned char* categoryGroupEnabled,
                                       const char* name,
                                       uint64_t id,
                                       double timestamp,
                                       int numArgs,
                                       const char** argNames,
                                       const unsigned char* argTypes,
                                       const uint64_t* argValues,
                                       unsigned char flags);

        // The |fingerprint| is provided by Dawn to inform the client to discard the Dawn caches
        // when the fingerprint changes. The returned CachingInterface is expected to outlive the
        // device which uses it to persistently cache objects.
        virtual CachingInterface* GetCachingInterface(const void* fingerprint,
                                                      size_t fingerprintSize);

      private:
        Platform(const Platform&) = delete;
        Platform& operator=(const Platform&) = delete;
    };

}  // namespace dawn_platform

#endif  // DAWNPLATFORM_DAWNPLATFORM_H_
