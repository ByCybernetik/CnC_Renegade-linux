#ifndef WW3D2_VULKAN_VK_2D_RENDERER_H
#define WW3D2_VULKAN_VK_2D_RENDERER_H

#include "vk_buffer.h"
#include "vk_platform.h"
#include "vk_texture.h"
#include <vulkan/vulkan.h>
#include <stdint.h>
#include <vector>

namespace ww3d_vulkan {

class VkRenderer;

struct Simple2DVertex {
	float x;
	float y;
	float u;
	float v;
	uint32_t color; // A8R8G8B8 packed
};

class Vk2DRenderer {
public:
	bool Init(VkRenderer *renderer);
	void Shutdown();
	void Begin_Frame();
	void Draw_Batch(
		VkCommandBuffer cmd,
		const Simple2DVertex *vertices,
		uint32_t vertex_count,
		const uint16_t *indices,
		uint32_t index_count,
		VkTexture *texture,
		bool texturing,
		uint8_t src_blend, // ShaderClass::SrcBlendFuncType
		uint8_t dst_blend, // ShaderClass::DstBlendFuncType
		const float modulate_color[4],
		uint32_t viewport_x,
		uint32_t viewport_y,
		uint32_t viewport_w,
		uint32_t viewport_h);

private:
	struct PipelineKey2D {
		uint8_t src_blend = 1;
		uint8_t dst_blend = 0;
		bool texturing = true;
		bool offscreen = false;

		bool operator==(const PipelineKey2D &other) const
		{
			return src_blend == other.src_blend &&
				dst_blend == other.dst_blend &&
				texturing == other.texturing &&
				offscreen == other.offscreen;
		}
	};

	struct PipelineEntry {
		PipelineKey2D key;
		VkRenderPass render_pass = VK_NULL_HANDLE;
		VkPipeline pipeline = VK_NULL_HANDLE;
	};

	struct PushConstants2D {
		float modulate_color[4];
		float texture_enabled;
		float _pad[3];
	};

	bool Create_Pipeline(const PipelineKey2D &key, VkRenderPass render_pass);
	VkPipeline Get_Pipeline(const PipelineKey2D &key, VkRenderPass render_pass);

	VkRenderer *renderer_ = nullptr;

	VkShaderModule vert_shader_ = VK_NULL_HANDLE;
	VkShaderModule frag_shader_ = VK_NULL_HANDLE;
	std::vector<uint32_t> vert_spirv_;
	std::vector<uint32_t> frag_spirv_;

	VkDescriptorSetLayout descriptor_set_layout_ = VK_NULL_HANDLE;
	VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
	::VkPipelineCache pipeline_cache_ = VK_NULL_HANDLE;

	VkBufferAlloc vertex_ring_[kMaxFramesInFlight];
	VkBufferAlloc index_ring_[kMaxFramesInFlight];
	VkDeviceSize vertex_offset_[kMaxFramesInFlight] = {};
	VkDeviceSize index_offset_[kMaxFramesInFlight] = {};

	std::vector<PipelineEntry> pipelines_;
};

} /* namespace ww3d_vulkan */

#endif /* WW3D2_VULKAN_VK_2D_RENDERER_H */
