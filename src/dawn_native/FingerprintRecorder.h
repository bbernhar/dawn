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

#ifndef DAWNNATIVE_FINGERPRINT_RECORDER_H_
#define DAWNNATIVE_FINGERPRINT_RECORDER_H_

#include "common/HashUtils.h"

#include <string>
#include <vector>

namespace dawn_native {

    class CachedObject;

    // Visitor that builds a hash that can be used as a key to lookup a cached object in a cache.
    // The FingerprintRecorder produces a hash which can outlive the device (ie. not based on
    // pointers). This is most useful for persistent caches which may contain the same keys from a
    // new device.
    class FingerprintRecorder {
      public:
        // Record calls the appropriate record function based on the type.
        template <typename T, typename... Args>
        void Record(const T& value, const Args&... args) {
            RecordImpl<T, Args...>::Call(this, value, args...);
        }

        size_t GetKey() const;

      private:
        template <typename T, typename... Args>
        struct RecordImpl {
            static constexpr void Call(FingerprintRecorder* recorder,
                                       const T& value,
                                       const Args&... args) {
                HashCombine(&recorder->mHash, value, args...);
            }
        };

       template <typename CachedObjectT>
        struct RecordImpl<CachedObjectT*> {
            static constexpr void Call(FingerprintRecorder* recorder, CachedObjectT* obj) {
                // Calling Record(objPtr) is not allowed. This check exists to only prevent such mistakes.
                ASSERT(false);
            }
        };

        template <>
        struct RecordImpl<std::string> {
            static constexpr void Call(FingerprintRecorder* recorder, const std::string& str) {
                recorder->RecordIterable<std::string>(str);
            }
        };

        template <>
        struct RecordImpl<std::vector<uint32_t>> {
            static constexpr void Call(FingerprintRecorder* recorder,
                                       const std::vector<uint32_t>& vec) {
                recorder->RecordIterable<std::vector<uint32_t>>(vec);
            }
        };

        template <typename IteratorT>
        void RecordIterable(const IteratorT& iterable) {
            for (auto it = iterable.begin(); it != iterable.end(); ++it) {
                Record(*it);
            }
        }

        size_t mHash = 0;
    };
}  // namespace dawn_native

#endif  // DAWNNATIVE_FINGERPRINT_RECORDER_H_