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

#include "tests/DawnTest.h"

#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include "dawn_native/D3D12Backend.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

using Microsoft::WRL::ComPtr;

namespace {

    // D3D11SharedImage persistently opens a wrapped handle as an external image.
    class D3D11SharedImage {
      public:
        D3D11SharedImage(WGPUDevice device,
                         HANDLE sharedHandle,
                         ComPtr<ID3D11Texture2D> d3d11Texture,
                         uint64_t acquireKey)
            : mD3d11Texture(std::move(d3d11Texture)), mAcquireKey(acquireKey) {
            mImage = dawn_native::d3d12::WrapSharedHandle(device, sharedHandle);
            ASSERT(mImage != nullptr);
        }

        bool BeginD3D12Access() {
            const bool success = mImage->BeginAccess(mAcquireKey);
            if (success) {
                mAcquireKey++;
            }
            return success;
        }

        bool EndD3D12Access() {
            return mImage->EndAccess(mAcquireKey);
        }

        ID3D11Texture2D* GetD3D11Texture() const {
            return mD3d11Texture.Get();
        }

        dawn_native::d3d12::ExternalImageDXGI* GetImage() const {
            return mImage.get();
        }

      private:
        ComPtr<ID3D11Texture2D> mD3d11Texture;
        std::unique_ptr<dawn_native::d3d12::ExternalImageDXGI> mImage;
        uint64_t mAcquireKey;
    };

    // ScopedAccessDawnTexture creates and accesses a D3D11SharedImage for use by Dawn.
    class ScopedAccessDawnTexture {
      public:
        ScopedAccessDawnTexture(wgpu::Device device,
                                D3D11SharedImage* sharedImage,
                                const wgpu::TextureDescriptor* desc,
                                bool isInitialized)
            : mSharedImage(sharedImage) {
            dawn_native::d3d12::ExternalImageDescriptorDXGISharedHandle externDesc;
            externDesc.cTextureDescriptor = reinterpret_cast<const WGPUTextureDescriptor*>(desc);
            externDesc.isInitialized = isInitialized;

            mDawnTexture = wgpu::Texture::Acquire(
                mSharedImage->GetImage()->ProduceTexture(device.Get(), &externDesc));
            if (mDawnTexture) {
                mSharedImage->BeginD3D12Access();
            }
        }

        ~ScopedAccessDawnTexture() {
            mSharedImage->EndD3D12Access();
            if (mDawnTexture == nullptr) {
                return;
            }
            mDawnTexture.Destroy();
            mDawnTexture.Release();
        }

        void Destroy() {  // remove
            mDawnTexture.Destroy();
        }

        WGPUTexture Get() {
            return mDawnTexture.Get();
        }

      private:
        wgpu::Texture mDawnTexture;
        D3D11SharedImage* mSharedImage = nullptr;
    };

    class D3D12ResourceTestBase : public DawnTest {
      public:
        void SetUp() override {
            DawnTest::SetUp();
            if (UsesWire()) {
                return;
            }

            // Create the D3D11 device/contexts that will be used in subsequent tests
            ComPtr<ID3D12Device> d3d12Device = dawn_native::d3d12::GetD3D12Device(device.Get());

            const LUID adapterLuid = d3d12Device->GetAdapterLuid();

            ComPtr<IDXGIFactory4> dxgiFactory;
            HRESULT hr = ::CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory));
            ASSERT_EQ(hr, S_OK);

            ComPtr<IDXGIAdapter> dxgiAdapter;
            hr = dxgiFactory->EnumAdapterByLuid(adapterLuid, IID_PPV_ARGS(&dxgiAdapter));
            ASSERT_EQ(hr, S_OK);

            ComPtr<ID3D11Device> d3d11Device;
            D3D_FEATURE_LEVEL d3dFeatureLevel;
            ComPtr<ID3D11DeviceContext> d3d11DeviceContext;
            hr = ::D3D11CreateDevice(dxgiAdapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0,
                                     nullptr, 0, D3D11_SDK_VERSION, &d3d11Device, &d3dFeatureLevel,
                                     &d3d11DeviceContext);
            ASSERT_EQ(hr, S_OK);

            mD3d11Device = std::move(d3d11Device);
            mD3d11DeviceContext = std::move(d3d11DeviceContext);

            baseDawnDescriptor.dimension = wgpu::TextureDimension::e2D;
            baseDawnDescriptor.format = wgpu::TextureFormat::RGBA8Unorm;
            baseDawnDescriptor.size = {kTestWidth, kTestHeight, 1};
            baseDawnDescriptor.sampleCount = 1;
            baseDawnDescriptor.mipLevelCount = 1;
            baseDawnDescriptor.usage = wgpu::TextureUsage::Sampled | wgpu::TextureUsage::CopySrc |
                                       wgpu::TextureUsage::RenderAttachment |
                                       wgpu::TextureUsage::CopyDst;

            baseD3dDescriptor.Width = kTestWidth;
            baseD3dDescriptor.Height = kTestHeight;
            baseD3dDescriptor.MipLevels = 1;
            baseD3dDescriptor.ArraySize = 1;
            baseD3dDescriptor.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            baseD3dDescriptor.SampleDesc.Count = 1;
            baseD3dDescriptor.SampleDesc.Quality = 0;
            baseD3dDescriptor.Usage = D3D11_USAGE_DEFAULT;
            baseD3dDescriptor.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
            baseD3dDescriptor.CPUAccessFlags = 0;
            baseD3dDescriptor.MiscFlags =
                D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
        }

      protected:
        std::unique_ptr<D3D11SharedImage> WrapSharedHandle(
            const D3D11_TEXTURE2D_DESC* baseD3dDescriptor) const {
            ComPtr<ID3D11Texture2D> d3d11Texture;
            HRESULT hr = mD3d11Device->CreateTexture2D(baseD3dDescriptor, nullptr, &d3d11Texture);
            ASSERT(hr == S_OK);

            ComPtr<IDXGIResource1> dxgiResource;
            hr = d3d11Texture.As(&dxgiResource);
            ASSERT(hr == S_OK);

            HANDLE sharedHandle;
            hr = dxgiResource->CreateSharedHandle(
                nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr,
                &sharedHandle);
            ASSERT(hr == S_OK);

            std::unique_ptr<D3D11SharedImage> sharedImage = std::make_unique<D3D11SharedImage>(
                device.Get(), sharedHandle, std::move(d3d11Texture), /*acquireKey*/ 0);

            // Now that we've created all of our resources, we can close the handle
            // since we no longer need it.
            ::CloseHandle(sharedHandle);

            return sharedImage;
        }

        static constexpr size_t kTestWidth = 10;
        static constexpr size_t kTestHeight = 10;

        ComPtr<ID3D11Device> mD3d11Device;
        ComPtr<ID3D11DeviceContext> mD3d11DeviceContext;

        D3D11_TEXTURE2D_DESC baseD3dDescriptor;
        wgpu::TextureDescriptor baseDawnDescriptor;
    };

}  // anonymous namespace

// A small fixture used to initialize default data for the D3D12Resource validation tests.
// These tests are skipped if the harness is using the wire.
class D3D12SharedHandleValidation : public D3D12ResourceTestBase {};

// Test a successful wrapping of an D3D12Resource in a texture
TEST_P(D3D12SharedHandleValidation, Success) {
    DAWN_SKIP_TEST_IF(UsesWire());

    std::unique_ptr<D3D11SharedImage> sharedImage = WrapSharedHandle(&baseD3dDescriptor);
    ScopedAccessDawnTexture texture(device, sharedImage.get(), &baseDawnDescriptor,
                                    /*isInitialized*/ false);

    ASSERT_NE(texture.Get(), nullptr);
}

// Test an error occurs if the texture descriptor is invalid
TEST_P(D3D12SharedHandleValidation, InvalidTextureDescriptor) {
    DAWN_SKIP_TEST_IF(UsesWire());

    wgpu::ChainedStruct chainedDescriptor;
    baseDawnDescriptor.nextInChain = &chainedDescriptor;

    std::unique_ptr<D3D11SharedImage> sharedImage = WrapSharedHandle(&baseD3dDescriptor);
    ASSERT_DEVICE_ERROR(ScopedAccessDawnTexture(device, sharedImage.get(), &baseDawnDescriptor,
                                                /*isInitialized*/ false));
}

// Test an error occurs if the descriptor mip level count isn't 1
TEST_P(D3D12SharedHandleValidation, InvalidMipLevelCount) {
    DAWN_SKIP_TEST_IF(UsesWire());
    baseDawnDescriptor.mipLevelCount = 2;

    std::unique_ptr<D3D11SharedImage> sharedImage = WrapSharedHandle(&baseD3dDescriptor);
    ASSERT_DEVICE_ERROR(ScopedAccessDawnTexture(device, sharedImage.get(), &baseDawnDescriptor,
                                                /*isInitialized*/ false));
}

// Test an error occurs if the descriptor depth isn't 1
TEST_P(D3D12SharedHandleValidation, InvalidDepth) {
    DAWN_SKIP_TEST_IF(UsesWire());
    baseDawnDescriptor.size.depth = 2;

    std::unique_ptr<D3D11SharedImage> sharedImage = WrapSharedHandle(&baseD3dDescriptor);
    ASSERT_DEVICE_ERROR(ScopedAccessDawnTexture(device, sharedImage.get(), &baseDawnDescriptor,
                                                /*isInitialized*/ false));
}

// Test an error occurs if the descriptor sample count isn't 1
TEST_P(D3D12SharedHandleValidation, InvalidSampleCount) {
    DAWN_SKIP_TEST_IF(UsesWire());
    baseDawnDescriptor.sampleCount = 4;

    std::unique_ptr<D3D11SharedImage> sharedImage = WrapSharedHandle(&baseD3dDescriptor);
    ASSERT_DEVICE_ERROR(ScopedAccessDawnTexture(device, sharedImage.get(), &baseDawnDescriptor,
                                                /*isInitialized*/ false));
}

// Test an error occurs if the descriptor width doesn't match the texture's
TEST_P(D3D12SharedHandleValidation, InvalidWidth) {
    DAWN_SKIP_TEST_IF(UsesWire());
    baseDawnDescriptor.size.width = kTestWidth + 1;

    std::unique_ptr<D3D11SharedImage> sharedImage = WrapSharedHandle(&baseD3dDescriptor);
    ASSERT_DEVICE_ERROR(ScopedAccessDawnTexture(device, sharedImage.get(), &baseDawnDescriptor,
                                                /*isInitialized*/ false));
}

// Test an error occurs if the descriptor height doesn't match the texture's
TEST_P(D3D12SharedHandleValidation, InvalidHeight) {
    DAWN_SKIP_TEST_IF(UsesWire());
    baseDawnDescriptor.size.height = kTestHeight + 1;

    std::unique_ptr<D3D11SharedImage> sharedImage = WrapSharedHandle(&baseD3dDescriptor);
    ASSERT_DEVICE_ERROR(ScopedAccessDawnTexture(device, sharedImage.get(), &baseDawnDescriptor,
                                                /*isInitialized*/ false));
}

// Test an error occurs if the descriptor format isn't compatible with the D3D12 Resource
TEST_P(D3D12SharedHandleValidation, InvalidFormat) {
    DAWN_SKIP_TEST_IF(UsesWire());
    baseDawnDescriptor.format = wgpu::TextureFormat::R8Unorm;

    std::unique_ptr<D3D11SharedImage> sharedImage = WrapSharedHandle(&baseD3dDescriptor);
    ASSERT_DEVICE_ERROR(ScopedAccessDawnTexture(device, sharedImage.get(), &baseDawnDescriptor,
                                                /*isInitialized*/ false));
}

// Test an error occurs if the number of D3D mip levels is greater than 1.
TEST_P(D3D12SharedHandleValidation, InvalidNumD3DMipLevels) {
    DAWN_SKIP_TEST_IF(UsesWire());
    baseD3dDescriptor.MipLevels = 2;

    std::unique_ptr<D3D11SharedImage> sharedImage = WrapSharedHandle(&baseD3dDescriptor);
    ASSERT_DEVICE_ERROR(ScopedAccessDawnTexture(device, sharedImage.get(), &baseDawnDescriptor,
                                                /*isInitialized*/ false));
}

// Test an error occurs if the number of array levels is greater than 1.
TEST_P(D3D12SharedHandleValidation, InvalidD3DArraySize) {
    DAWN_SKIP_TEST_IF(UsesWire());
    baseD3dDescriptor.ArraySize = 2;

    std::unique_ptr<D3D11SharedImage> sharedImage = WrapSharedHandle(&baseD3dDescriptor);
    ASSERT_DEVICE_ERROR(ScopedAccessDawnTexture(device, sharedImage.get(), &baseDawnDescriptor,
                                                /*isInitialized*/ false));
}

class D3D12SharedHandleUsageTests : public D3D12ResourceTestBase {
  protected:
    // Submits a 1x1x1 copy from source to destination
    void SimpleCopyTextureToTexture(wgpu::Texture source, wgpu::Texture destination) {
        wgpu::TextureCopyView copySrc = utils::CreateTextureCopyView(source, 0, {0, 0, 0});
        wgpu::TextureCopyView copyDst = utils::CreateTextureCopyView(destination, 0, {0, 0, 0});

        wgpu::Extent3D copySize = {1, 1, 1};

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyTextureToTexture(&copySrc, &copyDst, &copySize);
        wgpu::CommandBuffer commands = encoder.Finish();

        queue.Submit(1, &commands);
    }

    // Clear a texture on a given device
    void ClearImage(wgpu::Texture wrappedTexture, const wgpu::Color& clearColor) {
        wgpu::TextureView wrappedView = wrappedTexture.CreateView();

        // Submit a clear operation
        utils::ComboRenderPassDescriptor renderPassDescriptor({wrappedView}, {});
        renderPassDescriptor.cColorAttachments[0].clearColor = clearColor;

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDescriptor);
        pass.EndPass();

        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);
    }

    std::unique_ptr<D3D11SharedImage> WrapAndClearD3D11Texture(
        const D3D11_TEXTURE2D_DESC* d3dDescriptor,
        const wgpu::Color& clearColor,
        IDXGIKeyedMutex** dxgiKeyedMutexOut) const {
        ComPtr<ID3D11Texture2D> d3d11Texture;
        HRESULT hr = mD3d11Device->CreateTexture2D(d3dDescriptor, nullptr, &d3d11Texture);
        ASSERT(hr == S_OK);

        ComPtr<IDXGIResource1> dxgiResource;
        hr = d3d11Texture.As(&dxgiResource);
        ASSERT(hr == S_OK);

        HANDLE sharedHandle;
        hr = dxgiResource->CreateSharedHandle(
            nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr,
            &sharedHandle);
        ASSERT(hr == S_OK);

        ComPtr<IDXGIKeyedMutex> dxgiKeyedMutex;
        hr = d3d11Texture.As(&dxgiKeyedMutex);
        ASSERT(hr == S_OK);

        ComPtr<ID3D11RenderTargetView> d3d11RTV;
        hr = mD3d11Device->CreateRenderTargetView(d3d11Texture.Get(), nullptr, &d3d11RTV);
        ASSERT(hr == S_OK);

        hr = dxgiKeyedMutex->AcquireSync(0, INFINITE);
        ASSERT(hr == S_OK);

        const float colorRGBA[] = {
            static_cast<float>(clearColor.r), static_cast<float>(clearColor.g),
            static_cast<float>(clearColor.b), static_cast<float>(clearColor.a)};
        mD3d11DeviceContext->ClearRenderTargetView(d3d11RTV.Get(), colorRGBA);

        hr = dxgiKeyedMutex->ReleaseSync(1);
        ASSERT(hr == S_OK);

        std::unique_ptr<D3D11SharedImage> sharedImage = std::make_unique<D3D11SharedImage>(
            device.Get(), sharedHandle, std::move(d3d11Texture), /*acquireKey*/ 1);

        *dxgiKeyedMutexOut = dxgiKeyedMutex.Detach();
        return sharedImage;
    }

    void ExpectPixelRGBA8EQ(UINT64 acquireKey,
                            ID3D11Texture2D* d3d11Texture,
                            IDXGIKeyedMutex* dxgiKeyedMutex,
                            const wgpu::Color& color) {
        HRESULT hr = dxgiKeyedMutex->AcquireSync(acquireKey, INFINITE);
        ASSERT_EQ(hr, S_OK);

        D3D11_TEXTURE2D_DESC texture2DDesc;
        d3d11Texture->GetDesc(&texture2DDesc);

        const CD3D11_TEXTURE2D_DESC texture2DStagingDesc(
            texture2DDesc.Format,                             // Format
            texture2DDesc.Width,                              // Width
            texture2DDesc.Height,                             // Height
            1,                                                // ArraySize
            1,                                                // MipLevels
            0,                                                // BindFlags
            D3D11_USAGE_STAGING,                              // Usage
            D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE);  // CPUAccessFlags

        ComPtr<ID3D11Texture2D> spD3DTextureStaging;
        hr = mD3d11Device->CreateTexture2D(&texture2DStagingDesc, nullptr, &spD3DTextureStaging);
        ASSERT_EQ(hr, S_OK);

        D3D11_BOX d3dRc;
        d3dRc.back = 1;
        d3dRc.front = 0;
        d3dRc.top = 0;
        d3dRc.left = 0;
        d3dRc.bottom = texture2DDesc.Height;
        d3dRc.right = texture2DDesc.Width;

        mD3d11DeviceContext->CopySubresourceRegion(spD3DTextureStaging.Get(),  // pDstResource
                                                   0,                          // DstSubresource
                                                   0,                          // DstX
                                                   0,                          // DstY
                                                   0,                          // DstZ
                                                   d3d11Texture,               // pSrcResource
                                                   0,                          // SrcSubresource
                                                   &d3dRc);                    // pSrcBox

        D3D11_MAPPED_SUBRESOURCE mappedResource;
        hr = mD3d11DeviceContext->Map(spD3DTextureStaging.Get(), 0, D3D11_MAP_READ_WRITE, 0,
                                      &mappedResource);
        ASSERT_EQ(hr, S_OK);

        const uint8_t* colorData = static_cast<uint8_t*>(mappedResource.pData);
        EXPECT_EQ(colorData[0], color.r * 255u);
        EXPECT_EQ(colorData[1], color.g * 255u);
        EXPECT_EQ(colorData[2], color.b * 255u);
        EXPECT_EQ(colorData[3], color.a * 255u);

        mD3d11DeviceContext->Unmap(spD3DTextureStaging.Get(), 0);

        hr = dxgiKeyedMutex->ReleaseSync(acquireKey + 1);
        ASSERT_EQ(hr, S_OK);
    }
};

// 1. Create and clear a D3D11 texture
// 2. Copy the wrapped texture to another dawn texture
// 3. Readback the copied texture and ensure the color matches the original clear color.
TEST_P(D3D12SharedHandleUsageTests, ClearInD3D11CopyAndReadbackInD3D12) {
    DAWN_SKIP_TEST_IF(UsesWire());

    const wgpu::Color clearColor{1.0f, 1.0f, 0.0f, 1.0f};
    ComPtr<IDXGIKeyedMutex> dxgiKeyedMutex;
    std::unique_ptr<D3D11SharedImage> sharedImage =
        WrapAndClearD3D11Texture(&baseD3dDescriptor, clearColor, &dxgiKeyedMutex);

    ScopedAccessDawnTexture srcTexture(device, sharedImage.get(), &baseDawnDescriptor,
                                       /*isInitialized*/ true);

    // Create a texture on the device and copy the source texture to it.
    wgpu::Texture dawnCopyDestTexture = device.CreateTexture(&baseDawnDescriptor);
    SimpleCopyTextureToTexture(srcTexture.Get(), dawnCopyDestTexture);

    // Readback the destination texture and ensure it contains the colors we used
    // to clear the source texture on the D3D device.
    EXPECT_PIXEL_RGBA8_EQ(
        RGBA8(clearColor.r * 255u, clearColor.g * 255u, clearColor.b * 255u, clearColor.a * 255u),
        dawnCopyDestTexture, 0, 0);
}

// 1. Create and clear a D3D11 texture
// 2. Readback the wrapped texture and ensure the color matches the original clear color.
TEST_P(D3D12SharedHandleUsageTests, ClearInD3D11ReadbackInD3D12) {
    DAWN_SKIP_TEST_IF(UsesWire());

    const wgpu::Color clearColor{1.0f, 1.0f, 0.0f, 1.0f};
    ComPtr<IDXGIKeyedMutex> dxgiKeyedMutex;
    std::unique_ptr<D3D11SharedImage> sharedImage =
        WrapAndClearD3D11Texture(&baseD3dDescriptor, clearColor, &dxgiKeyedMutex);

    ScopedAccessDawnTexture srcTexture(device, sharedImage.get(), &baseDawnDescriptor,
                                       /*isInitialized*/ true);

    // Readback the destination texture and ensure it contains the colors we used
    // to clear the source texture on the D3D device.
    EXPECT_PIXEL_RGBA8_EQ(
        RGBA8(clearColor.r * 255, clearColor.g * 255, clearColor.b * 255, clearColor.a * 255),
        srcTexture.Get(), 0, 0);
}

// 1. Create and clear a D3D11 texture
// 2. Wrap it in a Dawn texture and clear it to a different color
// 3. Readback the texture with D3D11 and ensure we receive the color we cleared with Dawn.
TEST_P(D3D12SharedHandleUsageTests, ClearInD3D12ReadbackInD3D11) {
    DAWN_SKIP_TEST_IF(UsesWire());

    const wgpu::Color d3d11ClearColor{1.0f, 1.0f, 0.0f, 1.0f};
    ComPtr<IDXGIKeyedMutex> dxgiKeyedMutex;
    std::unique_ptr<D3D11SharedImage> sharedImage =
        WrapAndClearD3D11Texture(&baseD3dDescriptor, d3d11ClearColor, &dxgiKeyedMutex);

    ScopedAccessDawnTexture texture(device, sharedImage.get(), &baseDawnDescriptor,
                                    /*isInitialized*/ true);

    const wgpu::Color d3d12ClearColor{0.0f, 0.0f, 1.0f, 1.0f};
    ClearImage(texture.Get(), d3d12ClearColor);

    texture.Destroy();

    // Now that Dawn (via D3D12) has finished writing to the texture, we should be
    // able to read it back by copying it to a staging texture and verifying the
    // color matches the D3D12 clear color.
    sharedImage->EndD3D12Access();

    ExpectPixelRGBA8EQ(2, sharedImage->GetD3D11Texture(), dxgiKeyedMutex.Get(), d3d12ClearColor);
}

// 1. Create and clear a D3D11 texture
// 2. Wrap it in a Dawn texture and clear the texture to two different colors.
// 3. Readback the texture with D3D11.
// 4. Verify the readback color was the final color cleared.
TEST_P(D3D12SharedHandleUsageTests, ClearTwiceInD3D12ReadbackInD3D11) {
    DAWN_SKIP_TEST_IF(UsesWire());

    const wgpu::Color d3d11ClearColor{1.0f, 1.0f, 0.0f, 1.0f};
    ComPtr<IDXGIKeyedMutex> dxgiKeyedMutex;
    std::unique_ptr<D3D11SharedImage> sharedImage =
        WrapAndClearD3D11Texture(&baseD3dDescriptor, d3d11ClearColor, &dxgiKeyedMutex);

    ScopedAccessDawnTexture texture(device, sharedImage.get(), &baseDawnDescriptor,
                                    /*isInitialized*/ true);

    const wgpu::Color d3d12ClearColor1{0.0f, 0.0f, 1.0f, 1.0f};
    ClearImage(texture.Get(), d3d12ClearColor1);

    const wgpu::Color d3d12ClearColor2{0.0f, 1.0f, 1.0f, 1.0f};
    ClearImage(texture.Get(), d3d12ClearColor2);

    texture.Destroy();

    // Now that Dawn (via D3D12) has finished writing to the texture, we should be
    // able to read it back by copying it to a staging texture and verifying the
    // color matches the last D3D12 clear color.
    sharedImage->EndD3D12Access();

    ExpectPixelRGBA8EQ(2, sharedImage->GetD3D11Texture(), dxgiKeyedMutex.Get(), d3d12ClearColor2);
}

// 1. Create and clear a D3D11 texture with clearColor
// 2. Import the texture with isInitialized = false
// 3. Verify clearColor is not visible in wrapped texture
TEST_P(D3D12SharedHandleUsageTests, UninitializedTextureIsCleared) {
    DAWN_SKIP_TEST_IF(UsesWire());

    const wgpu::Color clearColor{1.0f, 0.0f, 0.0f, 1.0f};

    ComPtr<IDXGIKeyedMutex> dxgiKeyedMutex;
    std::unique_ptr<D3D11SharedImage> sharedImage =
        WrapAndClearD3D11Texture(&baseD3dDescriptor, clearColor, &dxgiKeyedMutex);

    ScopedAccessDawnTexture texture(device, sharedImage.get(), &baseDawnDescriptor,
                                    /*isInitialized*/ false);

    // Readback the destination texture and ensure it contains the colors we used
    // to clear the source texture on the D3D device.
    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 0, 0, 0), texture.Get(), 0, 0);
}

// 1. Create a wrapped shared image from the DX11 texture.
// 2. Create two corresponding Dawn textures from the the wrapped shared image.
// 3. Clear each Dawn texture and verify the texture was cleared of the unique color.
TEST_P(D3D12SharedHandleUsageTests, ReuseSharedImage) {
    std::unique_ptr<D3D11SharedImage> sharedImage = WrapSharedHandle(&baseD3dDescriptor);
    {
        const wgpu::Color solidRed{1.0f, 0.0f, 0.0f, 1.0f};

        ScopedAccessDawnTexture texture(device, sharedImage.get(), &baseDawnDescriptor,
                                        /*isInitialized*/ false);
        ASSERT_NE(texture.Get(), nullptr);

        ClearImage(texture.Get(), solidRed);

        EXPECT_PIXEL_RGBA8_EQ(RGBA8(0xFF, 0, 0, 0xFF), texture.Get(), 0, 0);
    }

    {
        const wgpu::Color solidBlue{0.0f, 0.0f, 1.0f, 1.0f};

        ScopedAccessDawnTexture texture(device, sharedImage.get(), &baseDawnDescriptor,
                                        /*isInitialized*/ false);
        ASSERT_NE(texture.Get(), nullptr);

        ClearImage(texture.Get(), solidBlue);

        EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 0, 0xFF, 0xFF), texture.Get(), 0, 0);
    }
}

DAWN_INSTANTIATE_TEST(D3D12SharedHandleValidation, D3D12Backend());
DAWN_INSTANTIATE_TEST(D3D12SharedHandleUsageTests, D3D12Backend());
