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

#include "dawn_native/FingerprintRecorder.h"
#include "dawn_native/RecordedObject.h"

namespace dawn_native {

    void FingerprintRecorder::recordObject(RecordedObject* obj) {
        ASSERT(obj != nullptr);
        if (obj->mKey != kEmptyKeyValue) {
            record(obj->getKey());
        } else {
            obj->Fingerprint(this);
            obj->mKey = mHash;
        }
    }

    size_t FingerprintRecorder::getKey() const {
        ASSERT(mHash != kEmptyKeyValue);
        return mHash;
    }
}  // namespace dawn_native
