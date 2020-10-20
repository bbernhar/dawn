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

#include "tests/DawnTest.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

namespace {
    class VideoViewsTest : public DawnTest {
      public:
        void SetUp() override {
            DawnTest::SetUp();
            DAWN_SKIP_TEST_IF(UsesWire());
        }
    };
}  // namespace

constexpr static uint32_t kRTSize = 8;

// Test creating a multi-planar video texture
TEST_P(VideoViewsTest, Create) {
    wgpu::TextureDescriptor textureDesc;
    textureDesc.format = wgpu::TextureFormat::NV12;
    textureDesc.dimension = wgpu::TextureDimension::e2D;

    wgpu::Texture texture = device.CreateTexture(&textureDesc);

    // Luminance-only view
    wgpu::TextureViewDescriptor lumaView;
    lumaView.format = wgpu::TextureFormat::R8Unorm;
    wgpu::TextureView lumaTextureView = texture.CreateView(&lumaView);

    // Chrominance-only view
    wgpu::TextureViewDescriptor chromaView;
    chromaView.format = wgpu::TextureFormat::RG8Uint;
    wgpu::TextureView chromaTextureView = texture.CreateView(&chromaView);

    // Create bind group
    wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::SampledTexture}});

    utils::ComboRenderPipelineDescriptor renderPipelineDescriptor(device);
    renderPipelineDescriptor.vertexStage.module =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, R"(
            #version 450
            void main() {
                const vec2 pos[3] = vec2[3](vec2(-1.f, 1.f), vec2(1.f, 1.f), vec2(-1.f, -1.f));
                gl_Position = vec4(pos[gl_VertexIndex], 0.f, 1.f);
            })");

    renderPipelineDescriptor.cFragmentStage.module =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, R"(
            #version 450
            layout (set = 0, binding = 0) uniform sampler sampler0;
            layout (set = 0, binding = 1) uniform texture2D texture0;
            layout(location = 0) out vec4 fragColor;
            void main() {
               fragColor = texture(sampler2D(texture0, sampler0), gl_FragCoord.xy);
            })");

    utils::BasicRenderPass renderPass = utils::CreateBasicRenderPass(device, kRTSize, kRTSize);
    renderPipelineDescriptor.cColorStates[0].format = renderPass.colorFormat;

    wgpu::RenderPipeline renderPipeline = device.CreateRenderPipeline(&renderPipelineDescriptor);

    wgpu::SamplerDescriptor samplerDesc = utils::GetDefaultSamplerDescriptor();
    wgpu::Sampler sampler = device.CreateSampler(&samplerDesc);

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(renderPipeline);
        pass.SetBindGroup(0, utils::MakeBindGroup(device, renderPipeline.GetBindGroupLayout(0),
                                                  {{0, sampler}, {1, lumaTextureView}}));
        pass.Draw(3);
        pass.EndPass();
    }

    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);
}

DAWN_INSTANTIATE_TEST(VideoViewsTest, D3D12Backend());