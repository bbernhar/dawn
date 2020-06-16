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

#include "common/Assert.h"
#include "common/Constants.h"
#include "common/Math.h"
#include "tests/DawnTest.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

constexpr uint32_t kRTSize = 4;

class VariableRateShadingTests : public DawnTest {
  protected:
    std::vector<const char*> GetRequiredExtensions() override {
        mIsVariableRateShadingSupported = SupportsExtensions({"variable_rate_shading"});
        if (!mIsVariableRateShadingSupported) {
            return {};
        }

        return {"variable_rate_shading"};
    }

    bool IsVariableRateShadingSupported() const {
        return mIsVariableRateShadingSupported;
    }

    bool mIsVariableRateShadingSupported = false;
};

TEST_P(VariableRateShadingTests, Basic) {
    ASSERT(IsVariableRateShadingSupported());

    utils::ComboRenderPipelineDescriptor renderPipelineDescriptor(device);
    renderPipelineDescriptor.vertexStage.module =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, R"(
            #version 450
            void main() {
                gl_Position = vec4(0.f, 0.f, 0.f, 1.f);
            })");

    renderPipelineDescriptor.cFragmentStage.module =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, R"(#version 450
            layout(location = 0) out vec4 fragColor;
            void main() {
               fragColor = vec4(0.0, 0.0, 0.0, 0.0);
            })");

    wgpu::RenderPipeline renderPipeline = device.CreateRenderPipeline(&renderPipelineDescriptor);
    utils::BasicRenderPass renderPass = utils::CreateBasicRenderPass(device, kRTSize, kRTSize);

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
    pass.SetPipeline(renderPipeline);
    pass.SetFragmentShadingRate(1, 1);
    pass.Draw(3);
    pass.EndPass();

    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);
}

DAWN_INSTANTIATE_TEST(VariableRateShadingTests, D3D12Backend());
