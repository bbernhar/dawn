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

#include "dawn_native/d3d12/DeviceD3D12.h"
#include "dawn_native/d3d12/PipelineCacheD3D12.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

using namespace dawn_native::d3d12;

#define EXPECT_PSO_CACHE_HIT(N, statement, device)                       \
    do {                                                                 \
        Device* backendDevice = reinterpret_cast<Device*>(device.Get()); \
        PipelineCache* cache = backendDevice->GetPipelineCache();        \
        size_t before = cache->GetPipelineCacheHitCountForTesting();     \
        statement;                                                       \
        size_t after = cache->GetPipelineCacheHitCountForTesting();      \
        EXPECT_EQ(N, after - before);                                    \
    } while (0)

class D3D12PipelineCachingTests : public DawnTest {
  protected:
    void SetUp() override {
        DawnTest::SetUp();
        DAWN_SKIP_TEST_IF(UsesWire());

        // PSO cache hit counts rely on pipeline caching being enabled.
        Device* backendDevice = reinterpret_cast<Device*>(device.Get());
        DAWN_SKIP_TEST_IF(!backendDevice->IsPipelineCachingEnabled());

        // Release builds do not require shader caching.
#if defined(_DEBUG)
        DAWN_SKIP_TEST_IF(!IsPersistentCacheEnabled());
        DAWN_SKIP_TEST_IF(
            backendDevice->IsToggleEnabled(dawn_native::Toggle::DisableD3D12ShaderCaching));
#endif

        // Ensure the persistent cache is reset every test.
        // Otherwise, the test could not run independently and could mistakenly re-use a result from
        // a previous test to pass.
        ResetPersistentCache();
    }
};

// Test creating a render pipeline with two shaders on the device then again but with a different
// device.
TEST_P(D3D12PipelineCachingTests, SameRenderPipeline) {
    const char* vs = R"(
                #version 450
                void main() {
                    gl_Position = vec4(0.0);
                })";

    const char* ps = R"(
                #version 450
                void main() {
                })";

    // Create the first pipeline from the device.
    {
        wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
            device, {{1, wgpu::ShaderStage::Fragment, wgpu::BindingType::UniformBuffer}});

        wgpu::PipelineLayout pl = utils::MakeBasicPipelineLayout(device, &bgl);

        utils::ComboRenderPipelineDescriptor desc(device);
        desc.vertexStage.module =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, vs);

        desc.cFragmentStage.module =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, ps);

        desc.layout = pl;

        EXPECT_PSO_CACHE_HIT(0u, device.CreateRenderPipeline(&desc), device);
    }

    // Create the same pipeline but from a different device.
    wgpu::Device device2 = GetAdapter().CreateDevice();
    {
        wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
            device2, {{1, wgpu::ShaderStage::Fragment, wgpu::BindingType::UniformBuffer}});

        wgpu::PipelineLayout pl = utils::MakeBasicPipelineLayout(device2, &bgl);

        utils::ComboRenderPipelineDescriptor desc(device2);
        desc.vertexStage.module =
            utils::CreateShaderModule(device2, utils::SingleShaderStage::Vertex, vs);

        desc.cFragmentStage.module =
            utils::CreateShaderModule(device2, utils::SingleShaderStage::Fragment, ps);

        desc.layout = pl;

        EXPECT_PSO_CACHE_HIT(0u, device2.CreateRenderPipeline(&desc), device2);
    }

    // Recreate the same pipeline from the first device again
    {
        wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
            device, {{1, wgpu::ShaderStage::Fragment, wgpu::BindingType::UniformBuffer}});

        wgpu::PipelineLayout pl = utils::MakeBasicPipelineLayout(device, &bgl);

        utils::ComboRenderPipelineDescriptor desc(device);
        desc.vertexStage.module =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, vs);

        desc.cFragmentStage.module =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, ps);

        desc.layout = pl;

        EXPECT_PSO_CACHE_HIT(1u, device.CreateRenderPipeline(&desc), device);
    }
}

// Test creating a render pipeline with one shader containing two entry points on the device then
// again but with a different device.
TEST_P(D3D12PipelineCachingTests, SameRenderPipelineTwoEntryPoints) {
    const char* shader = R"(
        [[builtin(position)]] var<out> Position : vec4<f32>;

        [[stage(vertex)]]
        fn vertex_main() -> void {
            Position = vec4<f32>(0.0, 0.0, 0.0, 1.0);
            return;
        }

        [[location(0)]] var<out> outColor : vec4<f32>;

        [[stage(fragment)]]
        fn fragment_main() -> void {
          outColor = vec4<f32>(1.0, 0.0, 0.0, 1.0);
          return;
        }
    )";

    // Create the first pipeline from the device.
    {
        wgpu::ShaderModule module = utils::CreateShaderModuleFromWGSL(device, shader);

        utils::ComboRenderPipelineDescriptor desc(device);
        desc.vertexStage.module = module;
        desc.vertexStage.entryPoint = "vertex_main";
        desc.cFragmentStage.module = module;
        desc.cFragmentStage.entryPoint = "fragment_main";

        wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
            device, {{1, wgpu::ShaderStage::Fragment, wgpu::BindingType::UniformBuffer}});

        wgpu::PipelineLayout pl = utils::MakeBasicPipelineLayout(device, &bgl);
        desc.layout = pl;

        EXPECT_PSO_CACHE_HIT(0u, device.CreateRenderPipeline(&desc), device);
    }

    // Create the same pipeline but from a different device.
    wgpu::Device device2 = GetAdapter().CreateDevice();
    {
        wgpu::ShaderModule module = utils::CreateShaderModuleFromWGSL(device2, shader);

        utils::ComboRenderPipelineDescriptor desc(device2);
        desc.vertexStage.module = module;
        desc.vertexStage.entryPoint = "vertex_main";
        desc.cFragmentStage.module = module;
        desc.cFragmentStage.entryPoint = "fragment_main";

        wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
            device2, {{1, wgpu::ShaderStage::Fragment, wgpu::BindingType::UniformBuffer}});

        wgpu::PipelineLayout pl = utils::MakeBasicPipelineLayout(device2, &bgl);
        desc.layout = pl;

        EXPECT_PSO_CACHE_HIT(0u, device2.CreateRenderPipeline(&desc), device2);
    }

    // Recreate the same pipeline from the first device again
    {
        wgpu::ShaderModule module = utils::CreateShaderModuleFromWGSL(device, shader);

        utils::ComboRenderPipelineDescriptor desc(device);
        desc.vertexStage.module = module;
        desc.vertexStage.entryPoint = "vertex_main";
        desc.cFragmentStage.module = module;
        desc.cFragmentStage.entryPoint = "fragment_main";

        wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
            device, {{1, wgpu::ShaderStage::Fragment, wgpu::BindingType::UniformBuffer}});

        wgpu::PipelineLayout pl = utils::MakeBasicPipelineLayout(device, &bgl);
        desc.layout = pl;

        EXPECT_PSO_CACHE_HIT(1u, device.CreateRenderPipeline(&desc), device);
    }

    // Recreate the same pipeline but from the second device.
    {
        wgpu::ShaderModule module = utils::CreateShaderModuleFromWGSL(device2, shader);

        utils::ComboRenderPipelineDescriptor desc(device2);
        desc.vertexStage.module = module;
        desc.vertexStage.entryPoint = "vertex_main";
        desc.cFragmentStage.module = module;
        desc.cFragmentStage.entryPoint = "fragment_main";

        wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
            device2, {{1, wgpu::ShaderStage::Fragment, wgpu::BindingType::UniformBuffer}});

        wgpu::PipelineLayout pl = utils::MakeBasicPipelineLayout(device2, &bgl);
        desc.layout = pl;

        EXPECT_PSO_CACHE_HIT(1u, device2.CreateRenderPipeline(&desc), device2);
    }
}

// Test creating two compute pipelines from the same module.
TEST_P(D3D12PipelineCachingTests, SameComputeTwoEntryPoints) {
    const char* shader = R"(
        [[block]] struct Data {
            [[offset(0)]] data : u32;
        };
        [[binding(0), set(0)]] var<storage_buffer> data : Data;

        [[stage(compute)]]
        fn compute_entry1() -> void {
            data.data = 1u;
            return;
        }

        [[stage(compute)]]
        fn compute_entry2() -> void {
            data.data = 42u;
            return;
        }
    )";

    // Create the first pipeline from the device.
    {
        wgpu::ShaderModule module = utils::CreateShaderModuleFromWGSL(device, shader);

        wgpu::ComputePipelineDescriptor desc;
        desc.computeStage.module = module;
        desc.computeStage.entryPoint = "compute_entry1";

        EXPECT_PSO_CACHE_HIT(0u, device.CreateComputePipeline(&desc), device);

        desc.computeStage.entryPoint = "compute_entry2";
        EXPECT_PSO_CACHE_HIT(0u, device.CreateComputePipeline(&desc), device);
    }

    // Create the same pipeline but from a different device.
    wgpu::Device device2 = GetAdapter().CreateDevice();
    {
        wgpu::ShaderModule module = utils::CreateShaderModuleFromWGSL(device2, shader);

        wgpu::ComputePipelineDescriptor desc;
        desc.computeStage.module = module;
        desc.computeStage.entryPoint = "compute_entry1";

        EXPECT_PSO_CACHE_HIT(0u, device2.CreateComputePipeline(&desc), device2);

        desc.computeStage.entryPoint = "compute_entry2";
        EXPECT_PSO_CACHE_HIT(0u, device2.CreateComputePipeline(&desc), device2);
    }

    // Recreate the same pipeline from the first device again
    {
        wgpu::ShaderModule module = utils::CreateShaderModuleFromWGSL(device, shader);

        wgpu::ComputePipelineDescriptor desc;
        desc.computeStage.module = module;
        desc.computeStage.entryPoint = "compute_entry1";

        EXPECT_PSO_CACHE_HIT(1u, device.CreateComputePipeline(&desc), device);

        desc.computeStage.entryPoint = "compute_entry2";
        EXPECT_PSO_CACHE_HIT(1u, device.CreateComputePipeline(&desc), device);
    }

    // Recreate the same pipeline but from the second device.
    {
        wgpu::ShaderModule module = utils::CreateShaderModuleFromWGSL(device2, shader);

        wgpu::ComputePipelineDescriptor desc;
        desc.computeStage.module = module;
        desc.computeStage.entryPoint = "compute_entry1";

        EXPECT_PSO_CACHE_HIT(1u, device2.CreateComputePipeline(&desc), device2);

        desc.computeStage.entryPoint = "compute_entry2";
        EXPECT_PSO_CACHE_HIT(1u, device2.CreateComputePipeline(&desc), device2);
    }
}

DAWN_INSTANTIATE_TEST(D3D12PipelineCachingTests,
                      D3D12Backend(),
                      D3D12Backend({"disable_d3d12_shader_caching"}));
