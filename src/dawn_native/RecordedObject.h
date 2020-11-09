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

#ifndef DAWNNATIVE_RECORDED_OBJECT_H_
#define DAWNNATIVE_RECORDED_OBJECT_H_

#include <limits>

namespace dawn_native {

    class FingerprintRecorder;

    static constexpr size_t kEmptyKeyValue = std::numeric_limits<size_t>::max();

    // Objects that know how to record themselves upon creation so they can used in a persistent
    // cache. This interface is separated from CachedObject because blueprint objects are never
    // persistently stored.
    class RecordedObject {
      public:
        // Implemented by cached objects so they can record themselves upon creation.
        // Once recorded, getKey() can be used to quickly lookup or compare the object in its cache.
        virtual void Fingerprint(FingerprintRecorder* recorder) = 0;

        size_t getKey() const;

      private:
        friend class FingerprintRecorder;

        size_t mKey = kEmptyKeyValue;
    };
}  // namespace dawn_native

#endif  // DAWNNATIVE_CACHED_OBJECT_H_
