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

#ifndef DAWNNATIVE_FORMAT_H_
#define DAWNNATIVE_FORMAT_H_

#include "dawn_native/dawn_platform.h"

#include "common/ityp_bitset.h"
#include "dawn_native/EnumClassBitmasks.h"
#include "dawn_native/Error.h"

#include <array>

// About multi-plane texture formats.
//
// Dawn supports additional "multi-plane" formats when Toggle::UseMultiPlaneTextures is enabled.
// When enabled, Dawn treats planar data as sub-resources (ie. 1 sub-resource == 1 view == 1 plane).
// A multi-plane format name encodes the channel mapping and order of planes. For example,
// R8BG82plane420Unorm is YUV 4:2:0 where Plane 0 = R8, and Plane 1 = RG16.
//
// Requirements:
// * Plane aspects cannot be combined with color, depth, or stencil aspects.
// * Only compatible multi-plane texture formats of planes can be used with multi-plane texture
// formats.
// * Can't access multiple planes without creating per plane views (no color conversion).
// * Multi-plane texture cannot be written or read without a per plane view.
//
// TODO(dawn:551): Consider moving this comment.

namespace dawn_native {

    enum class Aspect : uint8_t;
    class DeviceBase;

    // This mirrors wgpu::TextureComponentType as a bitmask instead.
    enum class ComponentTypeBit : uint8_t {
        None = 0x0,
        Float = 0x1,
        Sint = 0x2,
        Uint = 0x4,
        DepthComparison = 0x8,
    };

    // Converts an wgpu::TextureComponentType to its bitmask representation.
    ComponentTypeBit ToComponentTypeBit(wgpu::TextureComponentType type);
    // Converts an wgpu::TextureSampleType to its bitmask representation.
    ComponentTypeBit SampleTypeToComponentTypeBit(wgpu::TextureSampleType sampleType);

    struct TexelBlockInfo {
        uint32_t byteSize;
        uint32_t width;
        uint32_t height;
    };

    struct AspectInfo {
        TexelBlockInfo block;
        wgpu::TextureComponentType baseType;
        ComponentTypeBit supportedComponentTypes;
    };

    // The number of formats Dawn knows about. Asserts in BuildFormatTable ensure that this is the
    // exact number of known format.
    static constexpr size_t kKnownFormatCount = 54;

    struct Format;
    using FormatTable = std::array<Format, kKnownFormatCount>;

    // A wgpu::TextureFormat along with all the information about it necessary for validation.
    struct Format {
        wgpu::TextureFormat format;
        bool isRenderable;
        bool isCompressed;
        // A format can be known but not supported because it is part of a disabled extension.
        bool isSupported;
        bool supportsStorageUsage;
        Aspect aspects;

        bool IsColor() const;
        bool HasDepth() const;
        bool HasStencil() const;
        bool HasDepthOrStencil() const;
        bool IsMultiPlane() const;

        const AspectInfo& GetAspectInfo(wgpu::TextureAspect aspect) const;
        const AspectInfo& GetAspectInfo(Aspect aspect) const;

        // The index of the format in the list of all known formats: a unique number for each format
        // in [0, kKnownFormatCount)
        size_t GetIndex() const;

        // Multi-plane texture formats can't access multiple planes using a single view. Since
        // a multi-plane format cannot have multiple formats per plane, GetPlaneFormat()
        // is used to lookup the compatible view format using an aspect which corresponds to the
        // plane index. Returns Undefined if a non plane aspect is requested.
        // TODO(dawn:551): Consider moving this into a separate table.
        wgpu::TextureFormat GetPlaneFormat(wgpu::TextureAspect planeAspect) const;

      private:
        // The most common aspect: the color aspect for color texture, the depth aspect for
        // depth[-stencil] textures.
        AspectInfo firstAspect;

        friend FormatTable BuildFormatTable(const DeviceBase* device);
    };

    // Implementation details of the format table in the device.

    // Returns the index of a format in the FormatTable.
    size_t ComputeFormatIndex(wgpu::TextureFormat format);
    // Builds the format table with the extensions enabled on the device.
    FormatTable BuildFormatTable(const DeviceBase* device);

}  // namespace dawn_native

namespace wgpu {

    template <>
    struct IsDawnBitmask<dawn_native::ComponentTypeBit> {
        static constexpr bool enable = true;
    };

}  // namespace wgpu

#endif  // DAWNNATIVE_FORMAT_H_
