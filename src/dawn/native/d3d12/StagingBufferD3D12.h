// Copyright 2018 The Dawn Authors
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

#ifndef SRC_DAWN_NATIVE_D3D12_STAGINGBUFFERD3D12_H_
#define SRC_DAWN_NATIVE_D3D12_STAGINGBUFFERD3D12_H_

#include "dawn/native/StagingBuffer.h"
#include "dawn/native/d3d12/ResourceHeapAllocationD3D12.h"
#include "dawn/native/d3d12/d3d12_platform.h"

#include <gpgmm_d3d12.h>

namespace dawn::native::d3d12 {

class Device;

class StagingBuffer : public StagingBufferBase {
  public:
    StagingBuffer(size_t size, Device* device);
    ~StagingBuffer() override;

    ID3D12Resource* GetResource() const;

    MaybeError Initialize() override;

  private:
    Device* mDevice;
    ComPtr<gpgmm::d3d12::ResourceAllocation> mUploadHeap;
};
}  // namespace dawn::native::d3d12

#endif  // SRC_DAWN_NATIVE_D3D12_STAGINGBUFFERD3D12_H_
