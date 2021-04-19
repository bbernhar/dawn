// Copyright 2021 The Dawn Authors
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

#include "dawn_platform/CachedBlob.h"
#include "common/Assert.h"

CachedBlob::CachedBlob(const uint8_t* data, size_t size) {
    ASSERT(data != nullptr || size == 0);
    if (data != nullptr) {
        // TODO(bryan.bernhart@intel.com): consider optimizing out this copy
        mBuffer.reset(new uint8_t[size]);
        memcpy(mBuffer.get(), data, size);
    }
    mBufferSize = size;
}

CachedBlob::~CachedBlob() {
    ASSERT(GetRefCountForTesting() == 0);
}

const uint8_t* CachedBlob::data() {
    return mBuffer.get();
}

size_t CachedBlob::size() const {
    return mBufferSize;
}

void CachedBlob::Reference() {
    RefCounted::Reference();
}

void CachedBlob::Release() {
    RefCounted::Release();
}