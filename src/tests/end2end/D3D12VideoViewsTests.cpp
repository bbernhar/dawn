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
    class D3D12VideoViewsTestBase : public DawnTest {
      public:
        void SetUp() override {
            DawnTest::SetUp();
            DAWN_SKIP_TEST_IF(UsesWire());

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

            // Force texture validation to succeed when resource sharing is not supported.
            //
            // Sharing D3D12 video formats between D3D devices requires resource sharing capability.
            // The required capability is only available in D3D drivers supporting WDDM 2.7 or
            // higher. However, D3D12 does not (currently) enforce such requirement and these tests
            // never use the video texture by another D3D device after being imported into Dawn's
            // device. For now, bypass the capability bit for testing.
            //
            // TODO(dawn:551): Check with D3D team and remove this once bot drivers are upgraded.
            GetAdapter().EnableSharedResourceCapabilityForTesting();
        }

        static DXGI_FORMAT GetDXGITextureFormat(wgpu::TextureFormat format) {
            switch (format) {
                case wgpu::TextureFormat::R8BG82plane420Unorm:
                    return DXGI_FORMAT_NV12;
                default:
                    UNREACHABLE();
                    return DXGI_FORMAT_UNKNOWN;
            }
        }

        // Returns a pre-prepared multi-plane texture
        // The encoded texture data represents a 4x4 YUV 4:2:0 solid grey image. When
        // |isCheckerboard| is true, the upper left and bottom right fill a 2x2 grey block
        // while the upper right and bottom left fill a 2x2 white block.
        static std::vector<uint8_t> GetTestTextureData(wgpu::TextureFormat format,
                                                       bool isCheckerboard) {
            switch (format) {
                // The first 16 bytes is the luma plane (Y), followed by the chroma plane (UV) which
                // is half the number of bytes (subsampled by 2) but same bytes per line as luma
                // plane.
                case wgpu::TextureFormat::R8BG82plane420Unorm:
                    if (isCheckerboard) {
                        // clang-format off
                        return {
                            235, 235, 126, 126, // plane 0, start + 0
                            235, 235, 126, 126,
                            126, 126, 235, 235,
                            126, 126, 235, 235, 
                            128, 128, 128, 128, // plane 1, start + 16
                            128, 128, 128, 128,
                        };
                        // clang-format on
                    } else {
                        // clang-format off
                        return {
                            126, 126, 126, 126,  // plane 0, start + 0
                            126, 126, 126, 126, 
                            126, 126, 126, 126,
                            126, 126, 126, 126, 
                            128, 128, 128, 128,  // plane 1, start + 16
                            128, 128, 128, 128,
                        };
                        // clang-format on
                    }
                default:
                    UNREACHABLE();
                    return {};
            }
        }

        void CreateVideoTextureForTest(wgpu::TextureFormat format,
                                       wgpu::TextureUsage usage,
                                       wgpu::Texture* wgpuTextureOut,
                                       bool isCheckerboard = false) {
            wgpu::TextureDescriptor textureDesc;
            textureDesc.format = format;
            textureDesc.dimension = wgpu::TextureDimension::e2D;
            textureDesc.usage = usage;
            textureDesc.size = {kYUVImageDataWidthInTexels, kYUVImageDataHeightInTexels, 1};

            D3D11_TEXTURE2D_DESC d3dDescriptor;
            d3dDescriptor.Width = kYUVImageDataWidthInTexels;
            d3dDescriptor.Height = kYUVImageDataHeightInTexels;
            d3dDescriptor.MipLevels = 1;
            d3dDescriptor.ArraySize = 1;
            d3dDescriptor.Format = GetDXGITextureFormat(format);
            d3dDescriptor.SampleDesc.Count = 1;
            d3dDescriptor.SampleDesc.Quality = 0;
            d3dDescriptor.Usage = D3D11_USAGE_DEFAULT;
            d3dDescriptor.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            d3dDescriptor.CPUAccessFlags = 0;
            d3dDescriptor.MiscFlags =
                D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

            std::vector<uint8_t> initialData = GetTestTextureData(format, isCheckerboard);

            D3D11_SUBRESOURCE_DATA subres;
            subres.pSysMem = initialData.data();
            subres.SysMemPitch = kYUVImageDataWidthInTexels;

            ComPtr<ID3D11Texture2D> d3d11Texture;
            HRESULT hr = mD3d11Device->CreateTexture2D(&d3dDescriptor, &subres, &d3d11Texture);
            ASSERT_EQ(hr, S_OK);

            ComPtr<IDXGIResource1> dxgiResource;
            hr = d3d11Texture.As(&dxgiResource);
            ASSERT_EQ(hr, S_OK);

            HANDLE sharedHandle;
            hr = dxgiResource->CreateSharedHandle(
                nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr,
                &sharedHandle);
            ASSERT_EQ(hr, S_OK);

            dawn_native::d3d12::ExternalImageDescriptorDXGISharedHandle externDesc;
            externDesc.cTextureDescriptor =
                reinterpret_cast<const WGPUTextureDescriptor*>(&textureDesc);
            externDesc.sharedHandle = sharedHandle;
            externDesc.acquireMutexKey = 1;
            externDesc.isInitialized = true;

            // DX11 texture should be initialized upon CreateTexture2D. However, if we do not
            // acquire/release the keyed mutex before using the wrapped WebGPU texture, the WebGPU
            // texture is left uninitialized.
            ComPtr<IDXGIKeyedMutex> dxgiKeyedMutex;
            hr = d3d11Texture.As(&dxgiKeyedMutex);
            ASSERT_EQ(hr, S_OK);

            hr = dxgiKeyedMutex->AcquireSync(0, INFINITE);
            ASSERT_EQ(hr, S_OK);

            hr = dxgiKeyedMutex->ReleaseSync(1);
            ASSERT_EQ(hr, S_OK);

            *wgpuTextureOut = wgpu::Texture::Acquire(
                dawn_native::d3d12::WrapSharedHandle(device.Get(), &externDesc));

            // Handle is no longer needed once resources are created.
            ::CloseHandle(sharedHandle);
        }

        // Vertex shader used to render a sampled texture into a quad.
        wgpu::ShaderModule GetTestVertexShaderModule() const {
            return utils::CreateShaderModuleFromWGSL(device, R"(
                [[builtin(position)]] var<out> Position : vec4<f32>;
                [[location(0)]] var<out> texCoord : vec2 <f32>;

                [[builtin(vertex_idx)]] var<in> VertexIndex : u32;
                
                [[stage(vertex)]] fn main() -> void {
                    const pos : array<vec2<f32>, 6> = array<vec2<f32>, 6>( 
                        vec2<f32>(-1.0, 1.0),
                        vec2<f32>(-1.0, -1.0), 
                        vec2<f32>(1.0, -1.0),
                        vec2<f32>(-1.0, 1.0),
                        vec2<f32>(1.0, -1.0),
                        vec2<f32>(1.0, 1.0)
                    );
                    Position = vec4<f32>(pos[VertexIndex], 0.0, 1.0);
                    texCoord = vec2<f32>(Position.xy * 0.5) + vec2<f32>(0.5, 0.5);
                    return;
            })");
        }

        // The width and height in texels are 4 for all YUV formats.
        static constexpr uint32_t kYUVImageDataWidthInTexels = 4;
        static constexpr uint32_t kYUVImageDataHeightInTexels = 4;

        ComPtr<ID3D11Device> mD3d11Device;
    };
}  // namespace

// A small fixture used for the video views validation tests.
class D3D12VideoViewsValidation : public D3D12VideoViewsTestBase {
    void SetUp() override {
        D3D12VideoViewsTestBase::SetUp();
        DAWN_SKIP_TEST_IF(HasToggleEnabled("skip_validation"));
    }
};

// Test texture views compatibility rules.
TEST_P(D3D12VideoViewsValidation, CreateViewFails) {
    wgpu::Texture videoTexture;
    CreateVideoTextureForTest(wgpu::TextureFormat::R8BG82plane420Unorm, wgpu::TextureUsage::None,
                              &videoTexture);

    // Create a default view with no plane selected.
    ASSERT_DEVICE_ERROR(videoTexture.CreateView());

    wgpu::TextureViewDescriptor viewDesc = {};

    // Correct plane index but incompatible view format.
    viewDesc.format = wgpu::TextureFormat::R8Uint;
    viewDesc.aspect = wgpu::TextureAspect::Plane0;
    ASSERT_DEVICE_ERROR(videoTexture.CreateView(&viewDesc));

    // Compatible view format but wrong plane index.
    viewDesc.format = wgpu::TextureFormat::R8Unorm;
    viewDesc.aspect = wgpu::TextureAspect::Plane1;
    ASSERT_DEVICE_ERROR(videoTexture.CreateView(&viewDesc));

    // Compatible view format but wrong aspect.
    viewDesc.format = wgpu::TextureFormat::R8Unorm;
    viewDesc.aspect = wgpu::TextureAspect::All;
    ASSERT_DEVICE_ERROR(videoTexture.CreateView(&viewDesc));

    // Create a single plane texture.
    wgpu::TextureDescriptor desc;
    desc.format = wgpu::TextureFormat::RGBA8Unorm;
    desc.dimension = wgpu::TextureDimension::e2D;
    desc.usage = wgpu::TextureUsage::None;
    desc.size = {1, 1, 1};

    wgpu::Texture texture = device.CreateTexture(&desc);

    // Plane aspect specified with non-planar texture.
    viewDesc.aspect = wgpu::TextureAspect::Plane0;
    ASSERT_DEVICE_ERROR(texture.CreateView(&viewDesc));

    viewDesc.aspect = wgpu::TextureAspect::Plane1;
    ASSERT_DEVICE_ERROR(texture.CreateView(&viewDesc));

    // Planar views with non-planar texture.
    viewDesc.aspect = wgpu::TextureAspect::Plane0;
    viewDesc.format = wgpu::TextureFormat::R8Unorm;
    ASSERT_DEVICE_ERROR(texture.CreateView(&viewDesc));

    viewDesc.aspect = wgpu::TextureAspect::Plane1;
    viewDesc.format = wgpu::TextureFormat::RG8Unorm;
    ASSERT_DEVICE_ERROR(texture.CreateView(&viewDesc));
}

// Test texture views compatibility rules.
TEST_P(D3D12VideoViewsValidation, CreateViewSucceeds) {
    wgpu::Texture yuvTexture;
    CreateVideoTextureForTest(wgpu::TextureFormat::R8BG82plane420Unorm, wgpu::TextureUsage::None,
                              &yuvTexture);

    wgpu::TextureViewDescriptor planeViewDesc = {};
    planeViewDesc.aspect = wgpu::TextureAspect::Plane0;
    wgpu::TextureView plane0View = yuvTexture.CreateView(&planeViewDesc);

    planeViewDesc.aspect = wgpu::TextureAspect::Plane1;
    wgpu::TextureView plane1View = yuvTexture.CreateView(&planeViewDesc);

    ASSERT_NE(plane0View.Get(), nullptr);
    ASSERT_NE(plane1View.Get(), nullptr);
}

// Tests that copying a whole multi-plane texture fails.
TEST_P(D3D12VideoViewsValidation, T2TCopyAllAspectsFails) {
    wgpu::Texture srcTexture;
    CreateVideoTextureForTest(wgpu::TextureFormat::R8BG82plane420Unorm, wgpu::TextureUsage::CopySrc,
                              &srcTexture);

    wgpu::Texture dstTexture;
    CreateVideoTextureForTest(wgpu::TextureFormat::R8BG82plane420Unorm, wgpu::TextureUsage::CopyDst,
                              &dstTexture);

    wgpu::TextureCopyView srcView = utils::CreateTextureCopyView(srcTexture, 0, {0, 0, 0});
    wgpu::TextureCopyView dstView = utils::CreateTextureCopyView(dstTexture, 0, {0, 0, 0});
    wgpu::Extent3D copySize = {1, 1, 1};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyTextureToTexture(&srcView, &dstView, &copySize);
    ASSERT_DEVICE_ERROR(encoder.Finish());
}

// Tests that copying a multi-plane texture per plane fails.
TEST_P(D3D12VideoViewsValidation, T2TCopyPlaneAspectFails) {
    wgpu::Texture srcTexture;
    CreateVideoTextureForTest(wgpu::TextureFormat::R8BG82plane420Unorm, wgpu::TextureUsage::CopySrc,
                              &srcTexture);

    wgpu::Texture dstTexture;
    CreateVideoTextureForTest(wgpu::TextureFormat::R8BG82plane420Unorm, wgpu::TextureUsage::CopyDst,
                              &dstTexture);

    wgpu::TextureCopyView srcView =
        utils::CreateTextureCopyView(srcTexture, 0, {0, 0, 0}, wgpu::TextureAspect::Plane0);
    wgpu::TextureCopyView dstView =
        utils::CreateTextureCopyView(dstTexture, 0, {0, 0, 0}, wgpu::TextureAspect::Plane1);
    wgpu::Extent3D copySize = {1, 1, 1};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyTextureToTexture(&srcView, &dstView, &copySize);
    ASSERT_DEVICE_ERROR(encoder.Finish());
}

TEST_P(D3D12VideoViewsValidation, B2TCopyAllAspectsFails) {
    wgpu::Texture srcTexture;
    CreateVideoTextureForTest(wgpu::TextureFormat::R8BG82plane420Unorm,
                              wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::CopySrc,
                              &srcTexture);

    wgpu::BufferDescriptor bufferDescriptor;
    bufferDescriptor.size = 1;
    bufferDescriptor.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer dstBuffer = device.CreateBuffer(&bufferDescriptor);

    wgpu::TextureCopyView srcView = utils::CreateTextureCopyView(srcTexture, 0, {0, 0, 0});
    wgpu::BufferCopyView dstView = utils::CreateBufferCopyView(dstBuffer, 0, 4);
    wgpu::Extent3D copySize = {1, 1, 1};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyTextureToBuffer(&srcView, &dstView, &copySize);
    ASSERT_DEVICE_ERROR(encoder.Finish());
}

TEST_P(D3D12VideoViewsValidation, B2TCopyPlaneAspectsFails) {
    wgpu::Texture srcTexture;
    CreateVideoTextureForTest(wgpu::TextureFormat::R8BG82plane420Unorm,
                              wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::CopySrc,
                              &srcTexture);

    wgpu::BufferDescriptor bufferDescriptor;
    bufferDescriptor.size = 1;
    bufferDescriptor.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer dstBuffer = device.CreateBuffer(&bufferDescriptor);

    wgpu::TextureCopyView srcView =
        utils::CreateTextureCopyView(srcTexture, 0, {0, 0, 0}, wgpu::TextureAspect::Plane0);
    wgpu::BufferCopyView dstView = utils::CreateBufferCopyView(dstBuffer, 0, 4);
    wgpu::Extent3D copySize = {1, 1, 1};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyTextureToBuffer(&srcView, &dstView, &copySize);
    ASSERT_DEVICE_ERROR(encoder.Finish());
}

// Tests which multi-plane formats are allowed to be sampled (all)
TEST_P(D3D12VideoViewsValidation, SamplingMultiPlaneTexture) {
    wgpu::BindGroupLayout layout = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Fragment, wgpu::TextureSampleType::Float}});

    // R8BG82plane420Unorm is allowed to be sampled, if plane 0 or plane 1 is selected.
    wgpu::Texture texture;
    CreateVideoTextureForTest(wgpu::TextureFormat::R8BG82plane420Unorm, wgpu::TextureUsage::Sampled,
                              &texture);

    wgpu::TextureViewDescriptor desc = {};

    desc.aspect = wgpu::TextureAspect::Plane0;
    utils::MakeBindGroup(device, layout, {{0, texture.CreateView(&desc)}});

    desc.aspect = wgpu::TextureAspect::Plane1;
    utils::MakeBindGroup(device, layout, {{0, texture.CreateView(&desc)}});
}

// Tests creating a texture with a multi-plane format.
TEST_P(D3D12VideoViewsValidation, CreateTextureFails) {
    // Multi-plane formats are NOT allowed to be renderable.
    wgpu::Texture outputTexture;
    ASSERT_DEVICE_ERROR(CreateVideoTextureForTest(wgpu::TextureFormat::R8BG82plane420Unorm,
                                                  wgpu::TextureUsage::RenderAttachment,
                                                  &outputTexture));
    ASSERT_EQ(outputTexture, nullptr);
}

// A small fixture used only for video views usage tests.
class D3D12VideoViewsUsageTests : public D3D12VideoViewsTestBase {};

// Samples the luminance (Y) plane from an imported NV12 texture into a single channel of an RGB
// output attachment and checks for the expected pixel value in the rendered quad.
TEST_P(D3D12VideoViewsUsageTests, NV12SampleYtoR) {
    wgpu::Texture wgpuTexture;
    CreateVideoTextureForTest(wgpu::TextureFormat::R8BG82plane420Unorm, wgpu::TextureUsage::Sampled,
                              &wgpuTexture);

    wgpu::TextureViewDescriptor viewDesc;
    viewDesc.aspect = wgpu::TextureAspect::Plane0;
    wgpu::TextureView textureView = wgpuTexture.CreateView(&viewDesc);

    utils::ComboRenderPipelineDescriptor renderPipelineDescriptor(device);
    renderPipelineDescriptor.vertexStage.module = GetTestVertexShaderModule();

    renderPipelineDescriptor.cFragmentStage.module = utils::CreateShaderModuleFromWGSL(device, R"(
            [[set(0), binding(0)]] var<uniform_constant> sampler0 : sampler;
            [[set(0), binding(1)]] var<uniform_constant> texture : texture_2d<f32>;

            [[location(0)]] var<in> texCoord : vec2<f32>;
            [[location(0)]] var<out> fragColor : vec4<f32>;

            [[stage(fragment)]] fn main() -> void {
               var y : f32 = textureSample(texture, sampler0, texCoord).r;
               fragColor = vec4<f32>(y, 0.0, 0.0, 1.0);
               return;
            })");

    utils::BasicRenderPass renderPass = utils::CreateBasicRenderPass(
        device, kYUVImageDataWidthInTexels, kYUVImageDataHeightInTexels);
    renderPipelineDescriptor.cColorStates[0].format = renderPass.colorFormat;
    renderPipelineDescriptor.primitiveTopology = wgpu::PrimitiveTopology::TriangleList;

    wgpu::RenderPipeline renderPipeline = device.CreateRenderPipeline(&renderPipelineDescriptor);

    wgpu::SamplerDescriptor samplerDesc = utils::GetDefaultSamplerDescriptor();
    wgpu::Sampler sampler = device.CreateSampler(&samplerDesc);

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(renderPipeline);
        pass.SetBindGroup(0, utils::MakeBindGroup(device, renderPipeline.GetBindGroupLayout(0),
                                                  {{0, sampler}, {1, textureView}}));
        pass.Draw(6);
        pass.EndPass();
    }

    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    RGBA8 expectedPixel(126, 0x0, 0x0, 0xFF);
    EXPECT_PIXEL_RGBA8_EQ(expectedPixel, renderPass.color, 0, 0);
}

// Samples the chrominance (UV) plane from an imported texture into two channels of an RGBA output
// attachment and checks for the expected pixel value in the rendered quad.
TEST_P(D3D12VideoViewsUsageTests, NV12SampleUVtoRG) {
    wgpu::Texture wgpuTexture;
    CreateVideoTextureForTest(wgpu::TextureFormat::R8BG82plane420Unorm, wgpu::TextureUsage::Sampled,
                              &wgpuTexture);

    wgpu::TextureViewDescriptor viewDesc;
    viewDesc.aspect = wgpu::TextureAspect::Plane1;
    wgpu::TextureView textureView = wgpuTexture.CreateView(&viewDesc);

    utils::ComboRenderPipelineDescriptor renderPipelineDescriptor(device);
    renderPipelineDescriptor.vertexStage.module = GetTestVertexShaderModule();

    renderPipelineDescriptor.cFragmentStage.module = utils::CreateShaderModuleFromWGSL(device, R"(
            [[set(0), binding(0)]] var<uniform_constant> sampler0 : sampler;
            [[set(0), binding(1)]] var<uniform_constant> texture : texture_2d<f32>;

            [[location(0)]] var<in> texCoord : vec2<f32>;
            [[location(0)]] var<out> fragColor : vec4<f32>;

            [[stage(fragment)]] fn main() -> void {
               var u : f32 = textureSample(texture, sampler0, texCoord).r;
               var v : f32 = textureSample(texture, sampler0, texCoord).g;
               fragColor = vec4<f32>(u, v, 0.0, 1.0);
               return;
            })");

    utils::BasicRenderPass renderPass = utils::CreateBasicRenderPass(
        device, kYUVImageDataWidthInTexels, kYUVImageDataHeightInTexels);
    renderPipelineDescriptor.cColorStates[0].format = renderPass.colorFormat;
    renderPipelineDescriptor.primitiveTopology = wgpu::PrimitiveTopology::TriangleList;

    wgpu::RenderPipeline renderPipeline = device.CreateRenderPipeline(&renderPipelineDescriptor);

    wgpu::SamplerDescriptor samplerDesc = utils::GetDefaultSamplerDescriptor();
    wgpu::Sampler sampler = device.CreateSampler(&samplerDesc);

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(renderPipeline);
        pass.SetBindGroup(0, utils::MakeBindGroup(device, renderPipeline.GetBindGroupLayout(0),
                                                  {{0, sampler}, {1, textureView}}));
        pass.Draw(6);
        pass.EndPass();
    }

    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    RGBA8 expectedPixel(128, 128, 0x00, 0xFF);
    EXPECT_PIXEL_RGBA8_EQ(expectedPixel, renderPass.color, 0, 0);
}

// Renders a NV12 "checkerboard" texture into a RGB quad then checks the color at specific
// points to ensure the image has not been flipped.
TEST_P(D3D12VideoViewsUsageTests, RenderYUV) {
    wgpu::Texture wgpuTexture;
    CreateVideoTextureForTest(wgpu::TextureFormat::R8BG82plane420Unorm, wgpu::TextureUsage::Sampled,
                              &wgpuTexture, true);

    wgpu::TextureViewDescriptor lumaViewDesc;
    lumaViewDesc.aspect = wgpu::TextureAspect::Plane0;
    wgpu::TextureView lumaTextureView = wgpuTexture.CreateView(&lumaViewDesc);

    wgpu::TextureViewDescriptor chromaViewDesc;
    chromaViewDesc.aspect = wgpu::TextureAspect::Plane1;
    wgpu::TextureView chromaTextureView = wgpuTexture.CreateView(&chromaViewDesc);

    utils::ComboRenderPipelineDescriptor renderPipelineDescriptor(device);
    renderPipelineDescriptor.vertexStage.module = GetTestVertexShaderModule();

    renderPipelineDescriptor.cFragmentStage.module = utils::CreateShaderModuleFromWGSL(device, R"(
            [[set(0), binding(0)]] var<uniform_constant> sampler0 : sampler;
            [[set(0), binding(1)]] var<uniform_constant> lumaTexture : texture_2d<f32>;
            [[set(0), binding(2)]] var<uniform_constant> chromaTexture : texture_2d<f32>;

            [[location(0)]] var<in> texCoord : vec2<f32>;
            [[location(0)]] var<out> fragColor : vec4<f32>;

            [[stage(fragment)]] fn main() -> void {
               var y : f32 = textureSample(lumaTexture, sampler0, texCoord).r;
               var u : f32 = textureSample(chromaTexture, sampler0, texCoord).r;
               var v : f32 = textureSample(chromaTexture, sampler0, texCoord).g;
               fragColor = vec4<f32>(y, u, v, 1.0);
               return;
            })");

    utils::BasicRenderPass renderPass = utils::CreateBasicRenderPass(
        device, kYUVImageDataWidthInTexels, kYUVImageDataHeightInTexels);
    renderPipelineDescriptor.cColorStates[0].format = renderPass.colorFormat;
    renderPipelineDescriptor.primitiveTopology = wgpu::PrimitiveTopology::TriangleList;

    wgpu::RenderPipeline renderPipeline = device.CreateRenderPipeline(&renderPipelineDescriptor);

    wgpu::SamplerDescriptor samplerDesc = utils::GetDefaultSamplerDescriptor();
    wgpu::Sampler sampler = device.CreateSampler(&samplerDesc);

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(renderPipeline);
        pass.SetBindGroup(
            0, utils::MakeBindGroup(device, renderPipeline.GetBindGroupLayout(0),
                                    {{0, sampler}, {1, lumaTextureView}, {2, chromaTextureView}}));
        pass.Draw(6);
        pass.EndPass();
    }

    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    // Test four corners of the grey-white checkerboard image.
    RGBA8 greyYUV(126, 128, 128, 0xFF);
    EXPECT_PIXEL_RGBA8_EQ(greyYUV, renderPass.color, 0, 0);  // top left
    EXPECT_PIXEL_RGBA8_EQ(greyYUV, renderPass.color, kYUVImageDataWidthInTexels - 1,
                          kYUVImageDataHeightInTexels - 1);  // bottom right

    RGBA8 whiteYUV(235, 128, 128, 0xFF);
    EXPECT_PIXEL_RGBA8_EQ(whiteYUV, renderPass.color, kYUVImageDataWidthInTexels - 1,
                          0);  // top right
    EXPECT_PIXEL_RGBA8_EQ(whiteYUV, renderPass.color, 0,
                          kYUVImageDataHeightInTexels - 1);  // bottom left
}

DAWN_INSTANTIATE_TEST(D3D12VideoViewsValidation, D3D12Backend({"use_multiplane_textures"}));
DAWN_INSTANTIATE_TEST(D3D12VideoViewsUsageTests, D3D12Backend({"use_multiplane_textures"}));