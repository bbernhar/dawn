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

#ifndef DAWNNATIVE_D3D12BACKEND_H_
#define DAWNNATIVE_D3D12BACKEND_H_

#include <dawn/dawn_wsi.h>
#include <dawn_native/DawnNative.h>

#include <DXGI1_4.h>
#include <d3d12.h>
#include <windows.h>
#include <wrl/client.h>

struct ID3D12Device;
struct ID3D12Resource;
struct IDXGIKeyedMutex;

namespace dawn_native { namespace d3d12 {
    DAWN_NATIVE_EXPORT Microsoft::WRL::ComPtr<ID3D12Device> GetD3D12Device(WGPUDevice device);
    DAWN_NATIVE_EXPORT DawnSwapChainImplementation CreateNativeSwapChainImpl(WGPUDevice device,
                                                                             HWND window);
    DAWN_NATIVE_EXPORT WGPUTextureFormat
    GetNativeSwapChainPreferredFormat(const DawnSwapChainImplementation* swapChain);

    enum MemorySegment {
        Local,
        NonLocal,
    };

    DAWN_NATIVE_EXPORT uint64_t SetExternalMemoryReservation(WGPUDevice device,
                                                             uint64_t requestedReservationSize,
                                                             MemorySegment memorySegment);

    struct DAWN_NATIVE_EXPORT ExternalImageDescriptorDXGISharedHandle : ExternalImageDescriptor {
      public:
        ExternalImageDescriptorDXGISharedHandle();
    };

    struct DAWN_NATIVE_EXPORT ExternalImageDXGI {
        ExternalImageDXGI(Microsoft::WRL::ComPtr<ID3D12Resource> d3d12Resource,
                          Microsoft::WRL::ComPtr<IDXGIKeyedMutex> dxgiKeyedMutex);

        void Release(WGPUDevice device);

        WGPUTexture ProduceTexture(WGPUDevice device, const ExternalImageDescriptor* descriptor);

        bool BeginAccess(uint64_t acquireMutexKey);
        bool EndAccess(uint64_t releaseMutexKey);

        // The mD3D12Resource has a keyed mutex, it is stored in mDXGIKeyedMutex. It is used to
        // synchronize access between D3D11 and D3D12 components. The mDXGIKeyedMutex is the D3D12
        // side of the mutex. Only one component is allowed to read/write to the resource at a time.
        Microsoft::WRL::ComPtr<ID3D12Resource> mD3D12Resource;
        Microsoft::WRL::ComPtr<IDXGIKeyedMutex> mDXGIKeyedMutex;
    };

    // Note: SharedHandle must be a handle to a texture object.
    DAWN_NATIVE_EXPORT std::unique_ptr<ExternalImageDXGI> WrapSharedHandle(WGPUDevice device,
                                                                           HANDLE sharedHandle);

    struct DAWN_NATIVE_EXPORT AdapterDiscoveryOptions : public AdapterDiscoveryOptionsBase {
        AdapterDiscoveryOptions(Microsoft::WRL::ComPtr<IDXGIAdapter> adapter);

        Microsoft::WRL::ComPtr<IDXGIAdapter> dxgiAdapter;
    };

}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12BACKEND_H_
