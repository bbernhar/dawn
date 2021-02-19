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

// D3D12Backend.cpp: contains the definition of symbols exported by D3D12Backend.h so that they
// can be compiled twice: once export (shared library), once not exported (static library)

#include "dawn_native/D3D12Backend.h"

#include "common/SwapChainUtils.h"
#include "dawn_native/d3d12/DeviceD3D12.h"
#include "dawn_native/d3d12/NativeSwapChainImplD3D12.h"
#include "dawn_native/d3d12/ResidencyManagerD3D12.h"
#include "dawn_native/d3d12/TextureD3D12.h"

namespace dawn_native { namespace d3d12 {

    ComPtr<ID3D12Device> GetD3D12Device(WGPUDevice device) {
        Device* backendDevice = reinterpret_cast<Device*>(device);

        return backendDevice->GetD3D12Device();
    }

    DawnSwapChainImplementation CreateNativeSwapChainImpl(WGPUDevice device, HWND window) {
        Device* backendDevice = reinterpret_cast<Device*>(device);

        DawnSwapChainImplementation impl;
        impl = CreateSwapChainImplementation(new NativeSwapChainImpl(backendDevice, window));
        impl.textureUsage = WGPUTextureUsage_Present;

        return impl;
    }

    WGPUTextureFormat GetNativeSwapChainPreferredFormat(
        const DawnSwapChainImplementation* swapChain) {
        NativeSwapChainImpl* impl = reinterpret_cast<NativeSwapChainImpl*>(swapChain->userData);
        return static_cast<WGPUTextureFormat>(impl->GetPreferredFormat());
    }

    ExternalImageDescriptorDXGISharedHandle::ExternalImageDescriptorDXGISharedHandle()
        : ExternalImageDescriptor(ExternalImageType::DXGISharedHandle) {
    }

    ExternalImageDXGI::ExternalImageDXGI(ComPtr<ID3D12Resource> d3d12Resource,
                                         ComPtr<IDXGIKeyedMutex> dxgiKeyedMutex)
        : mD3D12Resource(std::move(d3d12Resource)), mDXGIKeyedMutex(std::move(dxgiKeyedMutex)) {
    }

    void ExternalImageDXGI::Release(WGPUDevice device) {
        Device* backendDevice = reinterpret_cast<Device*>(device);
        backendDevice->ReleaseKeyedMutexForTexture(std::move(mDXGIKeyedMutex));
    }

    WGPUTexture ExternalImageDXGI::ProduceTexture(WGPUDevice device,
                                                  const ExternalImageDescriptor* descriptor) {
        Device* backendDevice = reinterpret_cast<Device*>(device);
        Ref<TextureBase> texture = backendDevice->CreateSharedTexture(descriptor, mD3D12Resource);
        return reinterpret_cast<WGPUTexture>(texture.Detach());
    }

    bool ExternalImageDXGI::BeginAccess(uint64_t acquireMutexKey) {
        return SUCCEEDED(mDXGIKeyedMutex->AcquireSync(acquireMutexKey, INFINITE));
    }

    bool ExternalImageDXGI::EndAccess(uint64_t releaseMutexKey) {
        return SUCCEEDED(mDXGIKeyedMutex->ReleaseSync(releaseMutexKey));
    }

    uint64_t SetExternalMemoryReservation(WGPUDevice device,
                                          uint64_t requestedReservationSize,
                                          MemorySegment memorySegment) {
        Device* backendDevice = reinterpret_cast<Device*>(device);

        return backendDevice->GetResidencyManager()->SetExternalMemoryReservation(
            memorySegment, requestedReservationSize);
    }

    std::unique_ptr<ExternalImageDXGI> WrapSharedHandle(WGPUDevice device, HANDLE sharedHandle) {
        Device* backendDevice = reinterpret_cast<Device*>(device);

        Microsoft::WRL::ComPtr<ID3D12Resource> d3d12Resource;
        if (FAILED(backendDevice->GetD3D12Device()->OpenSharedHandle(
                sharedHandle, IID_PPV_ARGS(&d3d12Resource)))) {
            return nullptr;
        }

        Microsoft::WRL::ComPtr<IDXGIKeyedMutex> dxgiKeyedMutex =
            backendDevice->CreateKeyedMutexForTexture(d3d12Resource.Get());
        if (dxgiKeyedMutex == nullptr) {
            return nullptr;
        }

        return std::make_unique<ExternalImageDXGI>(std::move(d3d12Resource),
                                                   std::move(dxgiKeyedMutex));
    }

    AdapterDiscoveryOptions::AdapterDiscoveryOptions(ComPtr<IDXGIAdapter> adapter)
        : AdapterDiscoveryOptionsBase(WGPUBackendType_D3D12), dxgiAdapter(std::move(adapter)) {
    }
}}  // namespace dawn_native::d3d12
