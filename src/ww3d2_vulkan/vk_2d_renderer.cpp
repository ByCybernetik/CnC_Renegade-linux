#include "vk_2d_renderer.h"
#include "vk_renderer.h"
#include "vk_check.h"
#include "vk_context.h"
#include "vk_shader.h"
#include "vk_texture.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace ww3d_vulkan {

namespace {

constexpr VkDeviceSize kVertexRingSize = 4u * 1024u * 1024u;
constexpr VkDeviceSize kIndexRingSize = 2u * 1024u * 1024u;

static VkBlendFactor Src_Blend_To_Vk(uint8_t mode)
{
	switch (mode) {
	case 0:
		return VK_BLEND_FACTOR_ZERO;
	case 1:
		return VK_BLEND_FACTOR_ONE;
	case 2:
		return VK_BLEND_FACTOR_SRC_ALPHA;
	case 3:
		return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	default:
		return VK_BLEND_FACTOR_ONE;
	}
}

static VkBlendFactor Dst_Blend_To_Vk(uint8_t mode)
{
	switch (mode) {
	case 0:
		return VK_BLEND_FACTOR_ZERO;
	case 1:
		return VK_BLEND_FACTOR_ONE;
	case 2:
		return VK_BLEND_FACTOR_SRC_COLOR;
	case 3:
		return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
	case 4:
		return VK_BLEND_FACTOR_SRC_ALPHA;
	case 5:
		return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	default:
		return VK_BLEND_FACTOR_ZERO;
	}
}

static VkImageLayout Sample_Descriptor_Layout(const VkTexture *texture)
{
	if (texture == nullptr) {
		return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}
	const VkImageLayout layout = texture->Layout();
	if (layout == VK_IMAGE_LAYOUT_GENERAL) {
		return VK_IMAGE_LAYOUT_GENERAL;
	}
	return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

} /* namespace */

bool Vk2DRenderer::Init(VkRenderer *renderer)
{
	renderer_ = renderer;

	if (!Load_Spirv_From_Search_Path("simple_2d.vert.spv", &vert_spirv_)) {
		return false;
	}
	if (!Load_Spirv_From_Search_Path("simple_2d.frag.spv", &frag_spirv_)) {
		return false;
	}

	vert_shader_ = Load_Shader_Module(vert_spirv_);
	frag_shader_ = Load_Shader_Module(frag_spirv_);
	if (vert_shader_ == VK_NULL_HANDLE || frag_shader_ == VK_NULL_HANDLE) {
		return false;
	}

	VkContext &ctx = VkContext::Get();

	VkDescriptorSetLayoutBinding binding = {};
	binding.binding = 0;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding.descriptorCount = 1;
	binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo layout_info = {};
	layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
	layout_info.bindingCount = 1;
	layout_info.pBindings = &binding;
	VK_CHECK(vkCreateDescriptorSetLayout(
		ctx.Device(), &layout_info, nullptr, &descriptor_set_layout_));

	VkPushConstantRange push_range = {};
	push_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	push_range.offset = 0;
	push_range.size = sizeof(PushConstants2D);

	VkPipelineLayoutCreateInfo pipeline_layout_info = {};
	pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_info.setLayoutCount = 1;
	pipeline_layout_info.pSetLayouts = &descriptor_set_layout_;
	pipeline_layout_info.pushConstantRangeCount = 1;
	pipeline_layout_info.pPushConstantRanges = &push_range;
	VK_CHECK(vkCreatePipelineLayout(
		ctx.Device(), &pipeline_layout_info, nullptr, &pipeline_layout_));

	VkPipelineCacheCreateInfo cache_info = {};
	cache_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	VK_CHECK(vkCreatePipelineCache(
		ctx.Device(), &cache_info, nullptr, &pipeline_cache_));

	for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
		if (!vertex_ring_[i].Create(
				kVertexRingSize,
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				VMA_MEMORY_USAGE_CPU_TO_GPU)) {
			return false;
		}
		if (!index_ring_[i].Create(
				kIndexRingSize,
				VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
				VMA_MEMORY_USAGE_CPU_TO_GPU)) {
			return false;
		}
		vertex_offset_[i] = 0;
		index_offset_[i] = 0;
	}

	return true;
}

void Vk2DRenderer::Shutdown()
{
	VkContext &ctx = VkContext::Get();
	if (ctx.Device() != VK_NULL_HANDLE) {
		for (size_t i = 0; i < pipelines_.size(); ++i) {
			if (pipelines_[i].pipeline != VK_NULL_HANDLE) {
				vkDestroyPipeline(ctx.Device(), pipelines_[i].pipeline, nullptr);
			}
		}
		if (pipeline_cache_ != VK_NULL_HANDLE) {
			vkDestroyPipelineCache(ctx.Device(), pipeline_cache_, nullptr);
		}
		if (pipeline_layout_ != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(ctx.Device(), pipeline_layout_, nullptr);
		}
		if (descriptor_set_layout_ != VK_NULL_HANDLE) {
			vkDestroyDescriptorSetLayout(ctx.Device(), descriptor_set_layout_, nullptr);
		}
	}

	pipelines_.clear();
	Destroy_Shader_Module(vert_shader_);
	Destroy_Shader_Module(frag_shader_);
	vert_shader_ = VK_NULL_HANDLE;
	frag_shader_ = VK_NULL_HANDLE;
	vert_spirv_.clear();
	frag_spirv_.clear();
	pipeline_cache_ = VK_NULL_HANDLE;
	pipeline_layout_ = VK_NULL_HANDLE;
	descriptor_set_layout_ = VK_NULL_HANDLE;

	for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
		vertex_ring_[i].Destroy();
		index_ring_[i].Destroy();
		vertex_offset_[i] = 0;
		index_offset_[i] = 0;
	}

	renderer_ = nullptr;
}

void Vk2DRenderer::Begin_Frame()
{
	if (renderer_ == nullptr) {
		return;
	}
	const uint32_t frame = renderer_->Current_Frame();
	vertex_offset_[frame] = 0;
	index_offset_[frame] = 0;
}

VkPipeline Vk2DRenderer::Get_Pipeline(
	const PipelineKey2D &key,
	VkRenderPass render_pass)
{
	for (size_t i = 0; i < pipelines_.size(); ++i) {
		if (pipelines_[i].key == key && pipelines_[i].render_pass == render_pass) {
			return pipelines_[i].pipeline;
		}
	}

	if (!Create_Pipeline(key, render_pass)) {
		return VK_NULL_HANDLE;
	}
	return pipelines_.back().pipeline;
}

bool Vk2DRenderer::Create_Pipeline(
	const PipelineKey2D &key,
	VkRenderPass render_pass)
{
	VkVertexInputBindingDescription binding = {};
	binding.binding = 0;
	binding.stride = sizeof(Simple2DVertex);
	binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription attrs[3] = {};
	attrs[0].binding = 0;
	attrs[0].location = 0;
	attrs[0].format = VK_FORMAT_R32G32_SFLOAT;
	attrs[0].offset = offsetof(Simple2DVertex, x);
	attrs[1].binding = 0;
	attrs[1].location = 1;
	attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
	attrs[1].offset = offsetof(Simple2DVertex, u);
	attrs[2].binding = 0;
	attrs[2].location = 2;
	attrs[2].format = VK_FORMAT_R32_UINT;
	attrs[2].offset = offsetof(Simple2DVertex, color);

	VkPipelineVertexInputStateCreateInfo vertex_input = {};
	vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input.vertexBindingDescriptionCount = 1;
	vertex_input.pVertexBindingDescriptions = &binding;
	vertex_input.vertexAttributeDescriptionCount = 3;
	vertex_input.pVertexAttributeDescriptions = attrs;

	VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
	input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewport_state = {};
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.cullMode = VK_CULL_MODE_NONE;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo depth_stencil = {};
	depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil.depthTestEnable = VK_FALSE;
	depth_stencil.depthWriteEnable = VK_FALSE;
	depth_stencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;

	VkPipelineColorBlendAttachmentState color_attachment = {};
	color_attachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;
	color_attachment.blendEnable = VK_TRUE;
	color_attachment.srcColorBlendFactor = Src_Blend_To_Vk(key.src_blend);
	color_attachment.dstColorBlendFactor = Dst_Blend_To_Vk(key.dst_blend);
	color_attachment.colorBlendOp = VK_BLEND_OP_ADD;
	color_attachment.srcAlphaBlendFactor = Src_Blend_To_Vk(key.src_blend);
	color_attachment.dstAlphaBlendFactor = Dst_Blend_To_Vk(key.dst_blend);
	color_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo color_blending = {};
	color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blending.attachmentCount = 1;
	color_blending.pAttachments = &color_attachment;

	VkDynamicState dynamic_states[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};
	VkPipelineDynamicStateCreateInfo dynamic_state = {};
	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.dynamicStateCount = 2;
	dynamic_state.pDynamicStates = dynamic_states;

	VkPipelineShaderStageCreateInfo shader_stages[2] = {};
	shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shader_stages[0].module = vert_shader_;
	shader_stages[0].pName = "main";
	shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shader_stages[1].module = frag_shader_;
	shader_stages[1].pName = "main";

	VkGraphicsPipelineCreateInfo pipeline_info = {};
	pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_info.stageCount = 2;
	pipeline_info.pStages = shader_stages;
	pipeline_info.pVertexInputState = &vertex_input;
	pipeline_info.pInputAssemblyState = &input_assembly;
	pipeline_info.pViewportState = &viewport_state;
	pipeline_info.pRasterizationState = &rasterizer;
	pipeline_info.pMultisampleState = &multisampling;
	pipeline_info.pDepthStencilState = &depth_stencil;
	pipeline_info.pColorBlendState = &color_blending;
	pipeline_info.pDynamicState = &dynamic_state;
	pipeline_info.layout = pipeline_layout_;
	pipeline_info.renderPass = render_pass;
	pipeline_info.subpass = 0;

	VkContext &ctx = VkContext::Get();
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkResult result = vkCreateGraphicsPipelines(
		ctx.Device(), pipeline_cache_, 1, &pipeline_info, nullptr, &pipeline);
	if (result != VK_SUCCESS) {
		return false;
	}

	PipelineEntry entry;
	entry.key = key;
	entry.render_pass = render_pass;
	entry.pipeline = pipeline;
	pipelines_.push_back(entry);
	return true;
}

void Vk2DRenderer::Draw_Batch(
	VkCommandBuffer cmd,
	const Simple2DVertex *vertices,
	uint32_t vertex_count,
	const uint16_t *indices,
	uint32_t index_count,
	VkTexture *texture,
	bool texturing,
	uint8_t src_blend,
	uint8_t dst_blend,
	const float modulate_color[4],
	uint32_t viewport_x,
	uint32_t viewport_y,
	uint32_t viewport_w,
	uint32_t viewport_h)
{
	if (renderer_ == nullptr || cmd == VK_NULL_HANDLE) {
		return;
	}
	if (vertex_count == 0 || index_count == 0) {
		return;
	}
	if (!renderer_->Frame_Active()) {
		return;
	}

	const uint32_t frame = renderer_->Current_Frame();
	const VkDeviceSize vb_size = (VkDeviceSize)vertex_count * sizeof(Simple2DVertex);
	const VkDeviceSize ib_size = (VkDeviceSize)index_count * sizeof(uint16_t);

	if (vertex_offset_[frame] + vb_size > kVertexRingSize ||
			index_offset_[frame] + ib_size > kIndexRingSize) {
		fprintf(stderr,
			"Vk2DRenderer: ring buffer overflow (verts=%u vb_size=%llu indices=%u ib_size=%llu)\n",
			vertex_count, (unsigned long long)vb_size,
			index_count, (unsigned long long)ib_size);
		return;
	}

	vertex_ring_[frame].Upload(vertices, vb_size, vertex_offset_[frame]);
	index_ring_[frame].Upload(indices, ib_size, index_offset_[frame]);

	const VkDeviceSize vb_offset = vertex_offset_[frame];
	const VkDeviceSize ib_offset = index_offset_[frame];

	vertex_offset_[frame] += vb_size;
	index_offset_[frame] += ib_size;

	const bool offscreen = renderer_->Offscreen_Target() != nullptr;
	const VkRenderPass render_pass = offscreen
		? renderer_->Offscreen_Render_Pass()
		: renderer_->Main_Render_Pass();

	PipelineKey2D key;
	key.src_blend = src_blend;
	key.dst_blend = dst_blend;
	key.texturing = texturing;
	key.offscreen = offscreen;

	VkPipeline pipeline = Get_Pipeline(key, render_pass);
	if (pipeline == VK_NULL_HANDLE) {
		return;
	}

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	VkViewport viewport = {};
	viewport.x = (float)viewport_x;
	viewport.y = (float)(viewport_y + viewport_h);
	viewport.width = (float)viewport_w;
	viewport.height = -(float)viewport_h;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset.x = (int32_t)viewport_x;
	scissor.offset.y = (int32_t)viewport_y;
	scissor.extent.width = viewport_w;
	scissor.extent.height = viewport_h;

	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	VkBuffer vertex_buffer = vertex_ring_[frame].Handle();
	VkDeviceSize offset = vb_offset;
	vkCmdBindVertexBuffers(cmd, 0, 1, &vertex_buffer, &offset);
	vkCmdBindIndexBuffer(
		cmd, index_ring_[frame].Handle(), ib_offset, VK_INDEX_TYPE_UINT16);

	if (texture == nullptr ||
			texture->View() == VK_NULL_HANDLE ||
			texture->Sampler() == VK_NULL_HANDLE) {
		texture = renderer_->Default_Texture();
	}

	PFN_vkCmdPushDescriptorSetKHR push_descriptor = renderer_->Push_Descriptor_Func();
	if (push_descriptor != nullptr) {
		VkDescriptorImageInfo image_info = {};
		image_info.imageLayout = Sample_Descriptor_Layout(texture);
		image_info.imageView = texture->View();
		image_info.sampler = texture->Sampler();

		VkWriteDescriptorSet write = {};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstBinding = 0;
		write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write.descriptorCount = 1;
		write.pImageInfo = &image_info;

		push_descriptor(
			cmd,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipeline_layout_,
			0,
			1,
			&write);
	}

	PushConstants2D pc;
	pc.modulate_color[0] = modulate_color[0];
	pc.modulate_color[1] = modulate_color[1];
	pc.modulate_color[2] = modulate_color[2];
	pc.modulate_color[3] = modulate_color[3];
	pc.texture_enabled = texturing ? 1.0f : 0.0f;
	pc._pad[0] = 0.0f;
	pc._pad[1] = 0.0f;
	pc._pad[2] = 0.0f;

	vkCmdPushConstants(
		cmd,
		pipeline_layout_,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		0,
		sizeof(pc),
		&pc);

	vkCmdDrawIndexed(cmd, index_count, 1, 0, 0, 0);

	renderer_->Invalidate_Bound_Pipeline();
}

} /* namespace ww3d_vulkan */
