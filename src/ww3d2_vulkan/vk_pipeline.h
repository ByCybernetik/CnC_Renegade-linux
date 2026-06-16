#ifndef WW3D2_VULKAN_VK_PIPELINE_H
#define WW3D2_VULKAN_VK_PIPELINE_H

#include <vulkan/vulkan.h>
#include <vector>

namespace ww3d_vulkan {

struct MeshPipelineKey {
	bool alpha_blend = false;
	uint8_t src_blend = 1;
	uint8_t dst_blend = 0;
	bool depth_write = true;
	bool depth_test = true;
	uint8_t depth_compare = 3;
	bool two_sided = false;
	bool cull_inverted = false;
	bool alpha_test = false;
	uint8_t topology = 0;
	unsigned fvf = 0;
	uint16_t vertex_stride = 36;

	bool operator==(const MeshPipelineKey &other) const
	{
		return alpha_blend == other.alpha_blend &&
			src_blend == other.src_blend &&
			dst_blend == other.dst_blend &&
			depth_write == other.depth_write &&
			depth_test == other.depth_test &&
			depth_compare == other.depth_compare &&
			two_sided == other.two_sided &&
			cull_inverted == other.cull_inverted &&
			alpha_test == other.alpha_test &&
			topology == other.topology &&
			fvf == other.fvf &&
			vertex_stride == other.vertex_stride;
	}
};

class VkPipelineCache {
public:
	bool Create(
		VkRenderPass render_pass,
		const std::vector<uint32_t> &vert_spirv,
		const std::vector<uint32_t> &frag_spirv);
	void Destroy();

	VkPipeline Get(const MeshPipelineKey &key);
	VkPipelineLayout Layout() const { return pipeline_layout_; }
	VkDescriptorSetLayout Descriptor_Set_Layout() const { return descriptor_set_layout_; }

private:
	bool Create_Pipeline(const MeshPipelineKey &key);

	VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptor_set_layout_ = VK_NULL_HANDLE;
	VkShaderModule vert_shader_ = VK_NULL_HANDLE;
	VkShaderModule frag_shader_ = VK_NULL_HANDLE;
	VkRenderPass render_pass_ = VK_NULL_HANDLE;
	::VkPipelineCache vk_driver_cache_ = VK_NULL_HANDLE;

	struct PipelineEntry {
		MeshPipelineKey key;
		VkPipeline pipeline = VK_NULL_HANDLE;
	};
	std::vector<PipelineEntry> pipelines_;
	MeshPipelineKey last_lookup_key_ = {};
	VkPipeline last_lookup_pipeline_ = VK_NULL_HANDLE;
	bool last_lookup_valid_ = false;
};

} /* namespace ww3d_vulkan */

#endif
