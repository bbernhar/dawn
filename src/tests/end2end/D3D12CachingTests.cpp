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

#include "dawn_native/D3D12Backend.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

#define EXPECT_CACHE_HIT(N, statement)              \
    do {                                            \
        size_t before = mPersistentCache.mHitCount; \
        statement;                                  \
        FlushWire();                                \
        size_t after = mPersistentCache.mHitCount;  \
        EXPECT_EQ(N, after - before);               \
    } while (0)

#define EXPECT_PSO_CACHE_HIT_DEVICE(N, statement, otherDevice)                               \
    do {                                                                                     \
        if (UsesWire()) {                                                                    \
            statement;                                                                       \
            FlushWire();                                                                     \
        } else {                                                                             \
            size_t before = dawn_native::d3d12::GetPipelineCacheHitCount(otherDevice.Get()); \
            statement;                                                                       \
            size_t after = dawn_native::d3d12::GetPipelineCacheHitCount(otherDevice.Get());  \
            EXPECT_EQ(N, after - before);                                                    \
        }                                                                                    \
    } while (0)

#define EXPECT_PSO_CACHE_HIT(N, statement) EXPECT_PSO_CACHE_HIT_DEVICE(N, statement, device)

// FakePersistentCache implements a in-memory persistent cache.
class FakePersistentCache : public dawn_platform::CachingInterface {
  public:
    // PersistentCache API
    void StoreData(const WGPUDevice device,
                   const void* key,
                   size_t keySize,
                   const void* value,
                   size_t valueSize) override {
        if (mIsDisabled)
            return;
        const std::string keyStr(reinterpret_cast<const char*>(key), keySize);

        const uint8_t* value_start = reinterpret_cast<const uint8_t*>(value);

        dawn_platform::ScopedCachedBlob entry_value =
            dawn_platform::CachedData::CreateCachedBlob(value_start, valueSize);

        EXPECT_TRUE(mCache.insert({keyStr, std::move(entry_value)}).second);
    }

    dawn_platform::ScopedCachedBlob LoadData(const WGPUDevice device,
                                             const void* key,
                                             size_t keySize) override {
        const std::string keyStr(reinterpret_cast<const char*>(key), keySize);
        auto entry = mCache.find(keyStr);
        if (entry == mCache.end()) {
            return {};
        }
        mHitCount++;
        return entry->second;
    }

    using Blob = std::vector<uint8_t>;
    using FakeCache = std::unordered_map<std::string, dawn_platform::ScopedCachedBlob>;

    FakeCache mCache;

    size_t mHitCount = 0;
    bool mIsDisabled = false;
};

// Test platform that only supports caching.
class DawnTestPlatform : public dawn_platform::Platform {
  public:
    DawnTestPlatform(dawn_platform::CachingInterface* cachingInterface)
        : mCachingInterface(cachingInterface) {
    }
    ~DawnTestPlatform() override = default;

    dawn_platform::CachingInterface* GetCachingInterface(const void* fingerprint,
                                                         size_t fingerprintSize) override {
        return mCachingInterface;
    }

    dawn_platform::CachingInterface* mCachingInterface = nullptr;
};

class D3D12CachingTests : public DawnTest {
  protected:
    std::unique_ptr<dawn_platform::Platform> CreateTestPlatform() override {
        return std::make_unique<DawnTestPlatform>(&mPersistentCache);
    }

    FakePersistentCache mPersistentCache;

    void SetUp() override {
        DawnTest::SetUp();

        // Clear the persistent cache after SetUp to ensure the cache is always empty before running
        // the test. This is to ensure the tests continue running independently from each other.
        mPersistentCache.mCache.clear();
    }

    void TearDown() override {
        DawnTest::TearDown();

        // Disable the persistent cache after TearDown to prevent the default device from
        // overwriting the pipeline cache data stored upon shutdown.
        mPersistentCache.mIsDisabled = true;
    }

    wgpu::ComputePipeline CreateTestComputePipeline(wgpu::Device otherDevice) const {
        wgpu::ShaderModule module = utils::CreateShaderModule(otherDevice, R"(
            [[block]] struct Data {
                data : u32;
            };
            [[binding(0), group(0)]] var<storage> data : [[access(read_write)]] Data;

            [[stage(compute)]] fn main() {
                data.data = 1u;
                return;
            }
        )");

        wgpu::ComputePipelineDescriptor desc;
        desc.compute.module = module;
        desc.compute.entryPoint = "main";
        return otherDevice.CreateComputePipeline(&desc);
    }

    wgpu::RenderPipeline CreateTestRenderPipeline(
        wgpu::Device otherDevice,
        wgpu::PrimitiveTopology primitiveTopology = wgpu::PrimitiveTopology::TriangleList) const {
        wgpu::ShaderModule module = utils::CreateShaderModule(otherDevice, R"(
            [[builtin(position)]] var<out> Position : vec4<f32>;

            [[stage(vertex)]] fn vertex_main() {
                Position = vec4<f32>(0.0, 0.0, 0.0, 1.0);
                return;
            }

            [[location(0)]] var<out> outColor : vec4<f32>;

            [[stage(fragment)]] fn fragment_main() {
              outColor = vec4<f32>(1.0, 0.0, 0.0, 1.0);
              return;
            }
        )");

        utils::ComboRenderPipelineDescriptor desc;
        desc.primitive.topology = primitiveTopology;
        desc.vertex.module = module;
        desc.vertex.entryPoint = "vertex_main";
        desc.cFragment.module = module;
        desc.cFragment.entryPoint = "fragment_main";

        return otherDevice.CreateRenderPipeline(&desc);
    }
};

// Test that duplicate WGSL still re-compiles HLSL even when the cache is not enabled.
TEST_P(D3D12CachingTests, SameShaderNoCache) {
    mPersistentCache.mIsDisabled = true;

    wgpu::ShaderModule module = utils::CreateShaderModule(device, R"(
        [[builtin(position)]] var<out> Position : vec4<f32>;

        [[stage(vertex)]] fn vertex_main() {
            Position = vec4<f32>(0.0, 0.0, 0.0, 1.0);
            return;
        }

        [[location(0)]] var<out> outColor : vec4<f32>;

        [[stage(fragment)]] fn fragment_main() {
          outColor = vec4<f32>(1.0, 0.0, 0.0, 1.0);
          return;
        }
    )");

    // Store the WGSL shader into the cache.
    {
        utils::ComboRenderPipelineDescriptor desc;
        desc.vertex.module = module;
        desc.vertex.entryPoint = "vertex_main";
        desc.cFragment.module = module;
        desc.cFragment.entryPoint = "fragment_main";

        EXPECT_CACHE_HIT(0u, device.CreateRenderPipeline(&desc));
    }

    EXPECT_EQ(mPersistentCache.mCache.size(), 0u);

    // Load the same WGSL shader from the cache.
    {
        utils::ComboRenderPipelineDescriptor desc;
        desc.vertex.module = module;
        desc.vertex.entryPoint = "vertex_main";
        desc.cFragment.module = module;
        desc.cFragment.entryPoint = "fragment_main";

        EXPECT_CACHE_HIT(0u, device.CreateRenderPipeline(&desc));
    }

    EXPECT_EQ(mPersistentCache.mCache.size(), 0u);
}

// Test creating a pipeline from two entrypoints in multiple stages will cache the correct number
// of HLSL shaders. WGSL shader should result into caching 2 HLSL shaders (stage x
// entrypoints)
TEST_P(D3D12CachingTests, ReuseShaderWithMultipleEntryPointsPerStage) {
    wgpu::ShaderModule module = utils::CreateShaderModule(device, R"(
        [[builtin(position)]] var<out> Position : vec4<f32>;

        [[stage(vertex)]] fn vertex_main() {
            Position = vec4<f32>(0.0, 0.0, 0.0, 1.0);
            return;
        }

        [[location(0)]] var<out> outColor : vec4<f32>;

        [[stage(fragment)]] fn fragment_main() {
          outColor = vec4<f32>(1.0, 0.0, 0.0, 1.0);
          return;
        }
    )");

    // Store the WGSL shader into the cache.
    {
        utils::ComboRenderPipelineDescriptor desc;
        desc.vertex.module = module;
        desc.vertex.entryPoint = "vertex_main";
        desc.cFragment.module = module;
        desc.cFragment.entryPoint = "fragment_main";

        EXPECT_CACHE_HIT(0u, device.CreateRenderPipeline(&desc));
    }

    EXPECT_EQ(mPersistentCache.mCache.size(), 2u);

    // Load the same WGSL shader from the cache.
    {
        utils::ComboRenderPipelineDescriptor desc;
        desc.vertex.module = module;
        desc.vertex.entryPoint = "vertex_main";
        desc.cFragment.module = module;
        desc.cFragment.entryPoint = "fragment_main";

        EXPECT_CACHE_HIT(2u, device.CreateRenderPipeline(&desc));
    }

    EXPECT_EQ(mPersistentCache.mCache.size(), 2u);

    // Modify the WGSL shader functions and make sure it doesn't hit.
    wgpu::ShaderModule newModule = utils::CreateShaderModule(device, R"(
      [[stage(vertex)]] fn vertex_main() -> [[builtin(position)]] vec4<f32> {
          return vec4<f32>(1.0, 1.0, 1.0, 1.0);
      }

      [[stage(fragment)]] fn fragment_main() -> [[location(0)]] vec4<f32> {
        return vec4<f32>(1.0, 1.0, 1.0, 1.0);
      }
  )");

    {
        utils::ComboRenderPipelineDescriptor desc;
        desc.vertex.module = newModule;
        desc.vertex.entryPoint = "vertex_main";
        desc.cFragment.module = newModule;
        desc.cFragment.entryPoint = "fragment_main";
        EXPECT_CACHE_HIT(0u, device.CreateRenderPipeline(&desc));
    }

    // Cached HLSL shader calls LoadData twice (once to peek, again to get), so check 2 x
    // kNumOfShaders hits.
    EXPECT_EQ(mPersistentCache.mCache.size(), 4u);
}

// Test creating a WGSL shader with two entrypoints in the same stage will cache the correct number
// of HLSL shaders. WGSL shader should result into caching 1 HLSL shader (stage x entrypoints)
TEST_P(D3D12CachingTests, ReuseShaderWithMultipleEntryPoints) {
    wgpu::ShaderModule module = utils::CreateShaderModule(device, R"(
        [[block]] struct Data {
            data : u32;
        };
        [[binding(0), group(0)]] var<storage, read_write> data : Data;

        [[stage(compute)]] fn write1() {
            data.data = 1u;
            return;
        }

        [[stage(compute)]] fn write42() {
            data.data = 42u;
            return;
        }
    )");

    // Store the WGSL shader into the cache.
    {
        wgpu::ComputePipelineDescriptor desc;
        desc.compute.module = module;
        desc.compute.entryPoint = "write1";
        EXPECT_CACHE_HIT(0u, device.CreateComputePipeline(&desc));

        desc.compute.module = module;
        desc.compute.entryPoint = "write42";
        EXPECT_CACHE_HIT(0u, device.CreateComputePipeline(&desc));
    }

    EXPECT_EQ(mPersistentCache.mCache.size(), 2u);

    // Load the same WGSL shader from the cache.
    {
        wgpu::ComputePipelineDescriptor desc;
        desc.compute.module = module;
        desc.compute.entryPoint = "write1";

        EXPECT_CACHE_HIT(1u, device.CreateComputePipeline(&desc));

        desc.compute.module = module;
        desc.compute.entryPoint = "write42";

        EXPECT_CACHE_HIT(1u, device.CreateComputePipeline(&desc));
    }

    EXPECT_EQ(mPersistentCache.mCache.size(), 2u);
}

// Verify pipeline cache can be reused between devices on the same adapter.
TEST_P(D3D12CachingTests, ReusePipelineCache) {
    // Only the default device can be used with the wire.
    DAWN_TEST_UNSUPPORTED_IF(UsesWire());

    EXPECT_EQ(mPersistentCache.mCache.size(), 0u);

    // LoadData is called twice cache miss (once to peek, second to get).
    EXPECT_CACHE_HIT(1u, wgpu::Device::Acquire(GetAdapter().CreateDevice()));

    // LoadData is called once (once to get).
    EXPECT_CACHE_HIT(1u, wgpu::Device::Acquire(GetAdapter().CreateDevice()));

    EXPECT_EQ(mPersistentCache.mCache.size(), 1u);
}

// Verify pipelines can be reused.
TEST_P(D3D12CachingTests, ReusePipelines) {
    EXPECT_EQ(mPersistentCache.mCache.size(), 0u);

    // Create a render pipeline
    EXPECT_PSO_CACHE_HIT(0u, CreateTestRenderPipeline(device));

    // Adds two entries: 1 pipeline cache blob plus 1 compute shader.
    EXPECT_EQ(mPersistentCache.mCache.size(), 2u);

    // Create the same pipeline again.
    EXPECT_PSO_CACHE_HIT(1u, CreateTestRenderPipeline(device));

    EXPECT_EQ(mPersistentCache.mCache.size(), 2u);

    // Create a sightly different render pipeline
    wgpu::PrimitiveTopology newPrimitiveTopology = wgpu::PrimitiveTopology::PointList;
    EXPECT_PSO_CACHE_HIT(0u, CreateTestRenderPipeline(device, newPrimitiveTopology));
    EXPECT_PSO_CACHE_HIT(1u, CreateTestRenderPipeline(device, newPrimitiveTopology));

    EXPECT_EQ(mPersistentCache.mCache.size(), 2u);

    // Create a new compute pipeline
    EXPECT_PSO_CACHE_HIT(0u, CreateTestComputePipeline(device));

    // Adds one entries: 1 compute shader.
    EXPECT_EQ(mPersistentCache.mCache.size(), 3u);

    // Create the first compute pipeline again.
    EXPECT_PSO_CACHE_HIT(1u, CreateTestComputePipeline(device));

    // Create the first render pipeline again.
    EXPECT_PSO_CACHE_HIT(1u, CreateTestRenderPipeline(device));

    EXPECT_EQ(mPersistentCache.mCache.size(), 3u);
}

// Verify pipelines can be reused using regenerated shaders in debug.
TEST_P(D3D12CachingTests, ReusePipelinesDebug) {
    EXPECT_EQ(mPersistentCache.mCache.size(), 0u);

    // Create new pipelines
    EXPECT_PSO_CACHE_HIT(0u, CreateTestComputePipeline(device));
    EXPECT_PSO_CACHE_HIT(0u, CreateTestRenderPipeline(device));

    // Adds three entries: 1 compute shader + 1 pixel shader + 1 vertex shader.
    EXPECT_EQ(mPersistentCache.mCache.size(), 3u);

    // Create the same pipelines again
    EXPECT_PSO_CACHE_HIT(1u, CreateTestComputePipeline(device));
    EXPECT_PSO_CACHE_HIT(1u, CreateTestRenderPipeline(device));

    EXPECT_EQ(mPersistentCache.mCache.size(), 3u);
}

// Verify pipelines can be reused between devices.
TEST_P(D3D12CachingTests, ReusePipelinesMultipleDevices) {
    // Only the default device can be used with the wire.
    DAWN_TEST_UNSUPPORTED_IF(UsesWire());

    wgpu::Device firstDevice = wgpu::Device::Acquire(GetAdapter().CreateDevice());
    wgpu::Device secondDevice = wgpu::Device::Acquire(GetAdapter().CreateDevice());

    // Create two pipelines on the first device.
    EXPECT_PSO_CACHE_HIT_DEVICE(0u, CreateTestComputePipeline(firstDevice), firstDevice);
    EXPECT_PSO_CACHE_HIT_DEVICE(0u, CreateTestRenderPipeline(firstDevice), firstDevice);

    // Create the same two pipelines on the second device.
    EXPECT_PSO_CACHE_HIT_DEVICE(1u, CreateTestComputePipeline(secondDevice), secondDevice);
    EXPECT_PSO_CACHE_HIT_DEVICE(1u, CreateTestRenderPipeline(secondDevice), secondDevice);

    // Create the same two pipelines on the first device again.
    EXPECT_PSO_CACHE_HIT_DEVICE(1u, CreateTestComputePipeline(firstDevice), firstDevice);
    EXPECT_PSO_CACHE_HIT_DEVICE(1u, CreateTestRenderPipeline(firstDevice), firstDevice);

    // Persistent cache must be cleared before TearDown to prevent either device from overwriting
    // the pipeline cache data in the persistent cache.
    mPersistentCache.mCache.clear();
}

// Verify pipelines can be created but not persistently stored.
TEST_P(D3D12CachingTests, SamePipelinesNoCache) {
    EXPECT_EQ(mPersistentCache.mCache.size(), 0u);

    mPersistentCache.mIsDisabled = true;

    // Only the default device can be used with the wire.
    DAWN_TEST_UNSUPPORTED_IF(UsesWire());

    wgpu::Device firstDevice = wgpu::Device::Acquire(GetAdapter().CreateDevice());

    // Debug builds regenerate shaders when the persistent cache is disabled.
    const size_t hitCount = (IsDebug()) ? 0 : 1;

    // Create two pipelines on the first device.
    EXPECT_PSO_CACHE_HIT_DEVICE(0u, CreateTestComputePipeline(firstDevice), firstDevice);
    EXPECT_PSO_CACHE_HIT_DEVICE(0u, CreateTestRenderPipeline(firstDevice), firstDevice);

    // Create the same two pipelines on the first device again.
    EXPECT_PSO_CACHE_HIT_DEVICE(hitCount, CreateTestComputePipeline(firstDevice), firstDevice);
    EXPECT_PSO_CACHE_HIT_DEVICE(hitCount, CreateTestRenderPipeline(firstDevice), firstDevice);

    wgpu::Device secondDevice = wgpu::Device::Acquire(GetAdapter().CreateDevice());

    // Create the same two pipelines on the second device.
    EXPECT_PSO_CACHE_HIT_DEVICE(0u, CreateTestComputePipeline(secondDevice), secondDevice);
    EXPECT_PSO_CACHE_HIT_DEVICE(0u, CreateTestRenderPipeline(secondDevice), secondDevice);

    // Create the same two pipelines on the second device again.
    EXPECT_PSO_CACHE_HIT_DEVICE(hitCount, CreateTestComputePipeline(secondDevice), secondDevice);
    EXPECT_PSO_CACHE_HIT_DEVICE(hitCount, CreateTestRenderPipeline(secondDevice), secondDevice);

    EXPECT_EQ(mPersistentCache.mCache.size(), 0u);
}

// Verify pipelines can be reused when the persistent cache is nuked.
TEST_P(D3D12CachingTests, ReusePipelinesNukeCache) {
    EXPECT_EQ(mPersistentCache.mCache.size(), 0u);

    // Create new pipelines
    EXPECT_PSO_CACHE_HIT(0u, CreateTestComputePipeline(device));
    EXPECT_PSO_CACHE_HIT(1u, CreateTestComputePipeline(device));

    EXPECT_PSO_CACHE_HIT(0u, CreateTestRenderPipeline(device));
    EXPECT_PSO_CACHE_HIT(1u, CreateTestRenderPipeline(device));

    // Adds three entries: 1 compute shader + 1 pixel shader + 1 vertex shader.
    EXPECT_EQ(mPersistentCache.mCache.size(), 3u);

    // Nuke the cache
    mPersistentCache.mCache.clear();

    // Debug builds regenerate shaders when the persistent cache is cleared.
    const size_t hitCount = (IsDebug()) ? 0 : 1;

    // Create the same pipelines again
    EXPECT_PSO_CACHE_HIT(hitCount, CreateTestComputePipeline(device));
    EXPECT_PSO_CACHE_HIT(hitCount, CreateTestRenderPipeline(device));

    EXPECT_EQ(mPersistentCache.mCache.size(), 3u);
}

DAWN_INSTANTIATE_TEST(D3D12CachingTests, D3D12Backend());
