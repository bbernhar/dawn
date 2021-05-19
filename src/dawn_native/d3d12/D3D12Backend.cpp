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

#include "common/HashUtils.h"
#include "common/Log.h"
#include "common/Math.h"
#include "common/SwapChainUtils.h"
#include "dawn_native/d3d12/DeviceD3D12.h"
#include "dawn_native/d3d12/NativeSwapChainImplD3D12.h"
#include "dawn_native/d3d12/PlatformFunctions.h"
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
                                         const WGPUTextureDescriptor* descriptor)
        : mD3D12Resource(std::move(d3d12Resource)),
          mUsage(descriptor->usage),
          mDimension(descriptor->dimension),
          mSize(descriptor->size),
          mFormat(descriptor->format),
          mMipLevelCount(descriptor->mipLevelCount),
          mSampleCount(descriptor->sampleCount) {
        ASSERT(descriptor->nextInChain == nullptr);
    }

    ExternalImageDXGI::~ExternalImageDXGI() {
        // Ensure the device context does not leak any 12 resources created from it.
        for (auto& weakPtr : mD3d11on12DeviceContexts) {
            const auto& sharedPtr = weakPtr.lock();
            if (sharedPtr != nullptr) {
                sharedPtr->Release();
            }
        }
    }

    WGPUTexture ExternalImageDXGI::ProduceTexture(
        WGPUDevice device,
        const ExternalImageAccessDescriptorDXGIKeyedMutex* descriptor) {
        Device* backendDevice = reinterpret_cast<Device*>(device);

        // Ensure the texture usage is allowed
        if (!IsSubset(descriptor->usage, mUsage)) {
            dawn::ErrorLog() << "Texture usage is not valid for external image";
            return nullptr;
        }

        TextureDescriptor textureDescriptor = {};
        textureDescriptor.usage = static_cast<wgpu::TextureUsage>(descriptor->usage);
        textureDescriptor.dimension = static_cast<wgpu::TextureDimension>(mDimension);
        textureDescriptor.size = {mSize.width, mSize.height, mSize.depthOrArrayLayers};
        textureDescriptor.format = static_cast<wgpu::TextureFormat>(mFormat);
        textureDescriptor.mipLevelCount = mMipLevelCount;
        textureDescriptor.sampleCount = mSampleCount;

        // Set the release key to acquire key + 1 if not set. This allows supporting the old keyed
        // mutex protocol during the transition to making this a required parameter.
        ExternalMutexSerial releaseMutexKey =
            (descriptor->releaseMutexKey != UINT64_MAX)
                ? ExternalMutexSerial(descriptor->releaseMutexKey)
                : ExternalMutexSerial(descriptor->acquireMutexKey + 1);

        // 11on12 context is required to share a 11 resource using a shared keyed mutex with a 12
        // device.
        // TODO(dawn:625): Consider sharing 11on12 contexts per queue (vs device).
        std::shared_ptr<D3D11on12DeviceContext> deviceContext =
            GetOrCreateD3D11on12DeviceContext(device);
        if (deviceContext == nullptr) {
            dawn::ErrorLog() << "Unable to create 11on12 device context for external image";
            return nullptr;
        }

        Ref<TextureBase> texture = backendDevice->CreateExternalTexture(
            &textureDescriptor, mD3D12Resource, deviceContext,
            ExternalMutexSerial(descriptor->acquireMutexKey), releaseMutexKey,
            descriptor->isSwapChainTexture, descriptor->isInitialized);
        return reinterpret_cast<WGPUTexture>(texture.Detach());
    }

    // static
    std::unique_ptr<ExternalImageDXGI> ExternalImageDXGI::Create(
        WGPUDevice device,
        const ExternalImageDescriptorDXGISharedHandle* descriptor) {
        Device* backendDevice = reinterpret_cast<Device*>(device);

        Microsoft::WRL::ComPtr<ID3D12Resource> d3d12Resource;
        if (FAILED(backendDevice->GetD3D12Device()->OpenSharedHandle(
                descriptor->sharedHandle, IID_PPV_ARGS(&d3d12Resource)))) {
            return nullptr;
        }

        const TextureDescriptor* textureDescriptor =
            reinterpret_cast<const TextureDescriptor*>(descriptor->cTextureDescriptor);

        if (backendDevice->ConsumedError(
                ValidateTextureDescriptor(backendDevice, textureDescriptor))) {
            return nullptr;
        }

        if (backendDevice->ConsumedError(
                ValidateTextureDescriptorCanBeWrapped(textureDescriptor))) {
            return nullptr;
        }

        if (backendDevice->ConsumedError(
                ValidateD3D12TextureCanBeWrapped(d3d12Resource.Get(), textureDescriptor))) {
            return nullptr;
        }

        // Shared handle is assumed to support resource sharing capability. The resource
        // shared capability tier must agree to share resources between D3D devices.
        const Format* format =
            backendDevice->GetInternalFormat(textureDescriptor->format).AcquireSuccess();
        if (format->IsMultiPlanar()) {
            if (backendDevice->ConsumedError(ValidateD3D12VideoTextureCanBeShared(
                    backendDevice, D3D12TextureFormat(textureDescriptor->format)))) {
                return nullptr;
            }
        }

        std::unique_ptr<ExternalImageDXGI> result(
            new ExternalImageDXGI(std::move(d3d12Resource), descriptor->cTextureDescriptor));
        return result;
    }

    uint64_t SetExternalMemoryReservation(WGPUDevice device,
                                          uint64_t requestedReservationSize,
                                          MemorySegment memorySegment) {
        Device* backendDevice = reinterpret_cast<Device*>(device);

        return backendDevice->GetResidencyManager()->SetExternalMemoryReservation(
            memorySegment, requestedReservationSize);
    }

    AdapterDiscoveryOptions::AdapterDiscoveryOptions(ComPtr<IDXGIAdapter> adapter)
        : AdapterDiscoveryOptionsBase(WGPUBackendType_D3D12), dxgiAdapter(std::move(adapter)) {
    }

    std::shared_ptr<D3D11on12DeviceContext> ExternalImageDXGI::GetOrCreateD3D11on12DeviceContext(
        WGPUDevice device) {
        // Ensure the cache can't grow unbounded by removing entries that were automatically
        // destructed.
        for (auto& context : mD3d11on12DeviceContexts) {
            if (context.expired()) {
                mD3d11on12DeviceContexts.erase(context);
            }
        }

        std::shared_ptr<D3D11on12DeviceContext> blueprint =
            std::make_shared<D3D11on12DeviceContext>(device);
        auto iter = mD3d11on12DeviceContexts.find(blueprint);
        if (iter != mD3d11on12DeviceContexts.end()) {
            return iter->lock();
        }

        ComPtr<ID3D11Device> d3d11Device;
        ComPtr<ID3D11DeviceContext> d3d11DeviceContext;

        // Dawn's D3D12 command queue is shared between the D3D12 and 11on12 device.
        Device* backendDevice = reinterpret_cast<Device*>(device);
        ComPtr<ID3D12CommandQueue> d3d12CommandQueue = backendDevice->GetCommandQueue();

        ID3D12Device* d3d12Device = backendDevice->GetD3D12Device();
        IUnknown* const iUnknownQueue = d3d12CommandQueue.Get();
        if (FAILED(backendDevice->GetFunctions()->d3d11on12CreateDevice(
                d3d12Device, 0, nullptr, 0, &iUnknownQueue, 1, 1, &d3d11Device, &d3d11DeviceContext,
                nullptr))) {
            return nullptr;
        }

        ComPtr<ID3D11On12Device> d3d11on12Device;
        if (FAILED(d3d11Device.As(&d3d11on12Device))) {
            return nullptr;
        }

        ComPtr<ID3D11DeviceContext2> d3d11DeviceContext2;
        if (FAILED(d3d11DeviceContext.As(&d3d11DeviceContext2))) {
            return nullptr;
        }

        std::shared_ptr<D3D11on12DeviceContext> context = std::make_shared<D3D11on12DeviceContext>(
            std::move(d3d12CommandQueue), std::move(d3d11on12Device),
            std::move(d3d11DeviceContext2));

        // Cache the |context| without taking ownership.
        std::weak_ptr<D3D11on12DeviceContext> weakPtr = context;
        mD3d11on12DeviceContexts.insert(weakPtr);

        return context;
    }

    D3D11on12DeviceContext::D3D11on12DeviceContext(
        ComPtr<ID3D12CommandQueue> d3d12CommandQueue,
        ComPtr<ID3D11On12Device> d3d11On12Device,
        ComPtr<ID3D11DeviceContext2> d3d11on12DeviceContext)
        : mD3D12CommandQueue(std::move(d3d12CommandQueue)),
          mD3D11on12Device(std::move(d3d11On12Device)),
          mD3D11on12DeviceContext(std::move(d3d11on12DeviceContext)) {
    }

    D3D11on12DeviceContext::D3D11on12DeviceContext(WGPUDevice device) {
        Device* backendDevice = reinterpret_cast<Device*>(device);
        mD3D12CommandQueue = backendDevice->GetCommandQueue();
    }

    ID3D11On12Device* D3D11on12DeviceContext::GetDevice() {
        ASSERT(mD3D11on12Device != nullptr);
        return mD3D11on12Device.Get();
    }

    void D3D11on12DeviceContext::Release() {
        if (mD3D11on12DeviceContext == nullptr) {
            return;
        }

        // 11on12 has a bug where D3D12 resources used only for keyed shared mutexes
        // are not released until work is submitted to the device context and flushed.
        // The most minimal work we can get away with is issuing a TiledResourceBarrier.

        // ID3D11DeviceContext2 is available in Win8.1 and above. This suffices for a
        // D3D12 backend since both D3D12 and 11on12 first appeared in Windows 10.
        mD3D11on12DeviceContext->TiledResourceBarrier(nullptr, nullptr);
        mD3D11on12DeviceContext->Flush();
    }

    D3D11on12DeviceContext::~D3D11on12DeviceContext() {
        Release();
    }

    size_t D3D11on12DeviceContext::HashFunc::operator()(
        const std::weak_ptr<D3D11on12DeviceContext>& a) const {
        size_t hash = 0;
        HashCombine(&hash, std::shared_ptr<D3D11on12DeviceContext>(a)->mD3D12CommandQueue.Get());
        return hash;
    }

    bool D3D11on12DeviceContext::EqualityFunc::operator()(
        const std::weak_ptr<D3D11on12DeviceContext>& a,
        const std::weak_ptr<D3D11on12DeviceContext>& b) const {
        return std::shared_ptr<D3D11on12DeviceContext>(a)->mD3D12CommandQueue ==
               std::shared_ptr<D3D11on12DeviceContext>(b)->mD3D12CommandQueue;
    }
}}  // namespace dawn_native::d3d12
