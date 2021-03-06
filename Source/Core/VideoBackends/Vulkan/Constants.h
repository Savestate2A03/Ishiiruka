// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>

#include "Common/BitField.h"
#include "Common/CommonTypes.h"
#include "VideoBackends/Vulkan/VulkanLoader.h"

namespace Vulkan
{
// Number of command buffers. Having two allows one buffer to be
// executed whilst another is being built.
constexpr size_t NUM_COMMAND_BUFFERS = 2;

// Staging buffer usage - optimize for uploads or readbacks
enum STAGING_BUFFER_TYPE
{
	STAGING_BUFFER_TYPE_UPLOAD,
	STAGING_BUFFER_TYPE_READBACK
};

// Descriptor set layouts
enum DESCRIPTOR_SET_LAYOUT
{
	DESCRIPTOR_SET_LAYOUT_UNIFORM_BUFFERS,
	DESCRIPTOR_SET_LAYOUT_PIXEL_SHADER_SAMPLERS,
	DESCRIPTOR_SET_LAYOUT_SHADER_STORAGE_BUFFERS,
	DESCRIPTOR_SET_LAYOUT_TEXEL_BUFFERS,
	DESCRIPTOR_SET_LAYOUT_COMPUTE,
	NUM_DESCRIPTOR_SET_LAYOUTS
};

// Descriptor set bind points
enum DESCRIPTOR_SET_BIND_POINT
{
	DESCRIPTOR_SET_BIND_POINT_UNIFORM_BUFFERS,
	DESCRIPTOR_SET_BIND_POINT_PIXEL_SHADER_SAMPLERS,
	DESCRIPTOR_SET_BIND_POINT_STORAGE_OR_TEXEL_BUFFER,
	NUM_DESCRIPTOR_SET_BIND_POINTS
};

// We use four pipeline layouts:
//   - Standard
//       - Per-stage UBO (VS/GS/PS, VS constants accessible from PS)
//       - 16 combined image samplers (accessible from PS)
//   - BBox Enabled
//       - Same as standard, plus a single SSBO accessible from PS
//   - Push Constant
//       - Same as standard, plus 128 bytes of push constants, accessible from all stages.
//   - Texture Decoding
//       - Same as push constant, plus a single texel buffer accessible from PS.
//   - Compute
//       - 1 uniform buffer [set=0, binding=0]
//       - 4 combined image samplers [set=0, binding=1-4]
//       - 1 texel buffer [set=0, binding=5]
//       - 1 storage image [set=0, binding=6]
//       - 128 bytes of push constants
//
// All four pipeline layout share the first two descriptor sets (uniform buffers, PS samplers).
// The third descriptor set (see bind points above) is used for storage or texel buffers.
//
enum PIPELINE_LAYOUT
{
	PIPELINE_LAYOUT_STANDARD,
	PIPELINE_LAYOUT_BBOX,
	PIPELINE_LAYOUT_PUSH_CONSTANT,
	PIPELINE_LAYOUT_TEXTURE_CONVERSION,
	PIPELINE_LAYOUT_COMPUTE,
	NUM_PIPELINE_LAYOUTS
};

// Uniform buffer bindings within the first descriptor set
enum UNIFORM_BUFFER_DESCRIPTOR_SET_BINDING
{
	UBO_DESCRIPTOR_SET_BINDING_PS,
	UBO_DESCRIPTOR_SET_BINDING_VS,
	UBO_DESCRIPTOR_SET_BINDING_GS,
	NUM_UBO_DESCRIPTOR_SET_BINDINGS
};

// Maximum number of attributes per vertex (we don't have any more than this?)
constexpr size_t MAX_VERTEX_ATTRIBUTES = 16;

// Number of pixel shader texture slots
constexpr size_t NUM_PIXEL_SHADER_SAMPLERS = 16;

// Total number of binding points in the pipeline layout
constexpr size_t TOTAL_PIPELINE_BINDING_POINTS =
NUM_UBO_DESCRIPTOR_SET_BINDINGS + NUM_PIXEL_SHADER_SAMPLERS + 1;

// Format of EFB textures
constexpr VkFormat EFB_COLOR_TEXTURE_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;
constexpr VkFormat EFB_DEPTH_TEXTURE_FORMAT = VK_FORMAT_D32_SFLOAT;
constexpr VkFormat EFB_DEPTH_AS_COLOR_TEXTURE_FORMAT = VK_FORMAT_R32_SFLOAT;

// Format of texturecache textures
constexpr VkFormat TEXTURECACHE_TEXTURE_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;

// Textures that don't fit into this buffer will be uploaded with a separate buffer (see below).
constexpr size_t INITIAL_TEXTURE_UPLOAD_BUFFER_SIZE = 16 * 1024 * 1024;
constexpr size_t MAXIMUM_TEXTURE_UPLOAD_BUFFER_SIZE = 64 * 1024 * 1024;

// Textures greater than 1024*1024 will be put in staging textures that are released after
// execution instead. A 2048x2048 texture is 16MB, and we'd only fit four of these in our
// streaming buffer and be blocking frequently. Games are unlikely to have textures this
// large anyway, so it's only really an issue for HD texture packs, and memory is not
// a limiting factor in these scenarios anyway.
constexpr size_t STAGING_TEXTURE_UPLOAD_THRESHOLD = 1024 * 1024 * 8;

// Streaming uniform buffer size
constexpr size_t INITIAL_UNIFORM_STREAM_BUFFER_SIZE = 16 * 1024 * 1024;
constexpr size_t MAXIMUM_UNIFORM_STREAM_BUFFER_SIZE = 32 * 1024 * 1024;

// Texel buffer size for palette and texture decoding.
constexpr size_t TEXTURE_CONVERSION_TEXEL_BUFFER_SIZE = 8 * 1024 * 1024;

// Push constant buffer size for utility shaders
constexpr u32 PUSH_CONSTANT_BUFFER_SIZE = 128;

// Minimum number of draw calls per command buffer when attempting to preempt a readback operation.
constexpr u32 MINIMUM_DRAW_CALLS_PER_COMMAND_BUFFER_FOR_READBACK = 10;

// Rasterization state info
union RasterizationState {
	BitField<0, 2, VkCullModeFlags> cull_mode;
	BitField<2, 7, VkSampleCountFlagBits> samples;
	BitField<9, 1, VkBool32> per_sample_shading;
	BitField<10, 1, VkBool32> depth_clamp;

	u32 bits;
};

// Depth state info
union DepthStencilState {
	BitField<0, 1, VkBool32> test_enable;
	BitField<1, 1, VkBool32> write_enable;
	BitField<2, 3, VkCompareOp> compare_op;

	u32 bits;
};

// Blend state info
union BlendState {
	struct
	{
		union {
			BitField<0, 1, VkBool32> blend_enable;
			BitField<1, 3, VkBlendOp> blend_op;
			BitField<4, 5, VkBlendFactor> src_blend;
			BitField<9, 5, VkBlendFactor> dst_blend;
			BitField<14, 3, VkBlendOp> alpha_blend_op;
			BitField<17, 5, VkBlendFactor> src_alpha_blend;
			BitField<22, 5, VkBlendFactor> dst_alpha_blend;
			BitField<27, 4, VkColorComponentFlags> write_mask;
			u32 low_bits;
		};
		union {
			BitField<0, 1, VkBool32> logic_op_enable;
			BitField<1, 4, VkLogicOp> logic_op;
			u32 high_bits;
		};
	};

	u64 bits;
};

// Sampler info
union SamplerState {
	BitField<0, 1, VkFilter> min_filter;
	BitField<1, 1, VkFilter> mag_filter;
	BitField<2, 1, VkSamplerMipmapMode> mipmap_mode;
	BitField<3, 2, VkSamplerAddressMode> wrap_u;
	BitField<5, 2, VkSamplerAddressMode> wrap_v;
	BitField<7, 8, u32> min_lod;
	BitField<15, 8, u32> max_lod;
	BitField<23, 8, s32> lod_bias;
	BitField<31, 1, u32> enable_anisotropic_filtering;

	u32 bits;
	bool operator==(const SamplerState& rhs) const 
	{
		return bits == rhs.bits;
	}
	bool operator!=(const SamplerState& rhs) const
	{
		return bits != rhs.bits;
	}
	bool operator>(const SamplerState& rhs) const
	{
		return bits > rhs.bits;
	}
	bool operator<(const SamplerState& rhs) const
	{
		return bits < rhs.bits;
	}
};

}  // namespace Vulkan
