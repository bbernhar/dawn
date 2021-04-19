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

#ifndef COMMON_CACHEDBLOB_H_
#define COMMON_CACHEDBLOB_H_

#include "common/NonCopyable.h"
#include "common/RefCounted.h"
#include "dawn_platform/DawnPlatform.h"

// Shares cached data outside of Dawn.
class CachedBlob : public dawn_platform::CachedData, public RefCounted, public NonCopyable {
  public:
    CachedBlob(const uint8_t* data, size_t size);

    const uint8_t* data() override;
    size_t size() const override;

  private:
    ~CachedBlob() override;

    void Reference() override;
    virtual void Release() override;

    std::unique_ptr<uint8_t[]> mBuffer;
    size_t mBufferSize = 0;
};

#endif  // COMMON_CACHEDBLOB_H_
