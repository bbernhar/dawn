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

namespace dawn_native {

    class RecordedObject;

    // Helper to build a key that can be used to lookup an object in a cache.
    // The recorder walks the object and sub-objects, accumulating the result for a key.
    // Upon a future lookup or comparision, the recorded key can be re-used.
    class FingerprintRecorder {
      public:
        FingerprintRecorder() = default;

        template <typename T, typename... Args>
        void record(const T& value, const Args&... args) {
            HashCombine(&mHash, value, args...);
        }

        template <typename IteratorT>
        void recordIterable(const IteratorT& iterable) {
            for (auto it = iterable.begin(); it != iterable.end(); ++it) {
                record(*it);
            }
        }

        // Called at the end of RecordedObject-based object construction.
        void recordObject(RecordedObject* obj);

        size_t getKey() const;

      private:
        size_t mHash = 0;
    };
}  // namespace dawn_native

#endif  // DAWNNATIVE_FINGERPRINT_RECORDER_H_