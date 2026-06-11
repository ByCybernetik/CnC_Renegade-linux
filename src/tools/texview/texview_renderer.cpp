#include "texview_renderer.h"

#include "../ww3d2_vulkan/vk_context.h"
#include "../ww3d2_vulkan/vk_platform.h"
#include "../ww3d2_vulkan/vk_swapchain.h"
#include "../ww3d2_vulkan/vk_check.h"
#include "../ww3d2_vulkan/vk_shader.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits.h>
#include <string>
#include <vector>

#if defined(__linux__)
#include <unistd.h>
#endif

using ww3d_vulkan::VkContext;

namespace {

struct BlitVertex {
	float x;
	float y;
	float u;
	float v;
};

struct BlitPushConstants {
	float tex_size[2];
	float show_alpha_bg;
};

static uint32_t Find_Memory_Type(uint32_t type_bits, VkMemoryPropertyFlags properties)
{
	VkContext &ctx = VkContext::Get();
	VkPhysicalDeviceMemoryProperties mem_props = {};
	vkGetPhysicalDeviceMemoryProperties(ctx.Physical_Device(), &mem_props);
	for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
		if ((type_bits & (1u << i)) && (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}
	return 0;
}

static bool Load_Texview_Spirv(const char *filename, std::vector<uint32_t> *out)
{
	std::vector<std::string> paths;

	const char *env = getenv("RENEGADE_SHADER_PATH");
	if (env != nullptr && env[0] != '\0') {
		paths.push_back(std::string(env) + "/texview/" + filename);
		paths.push_back(std::string(env) + "/" + filename);
	}

#if defined(__linux__)
	char exe_path[PATH_MAX];
	const ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
	if (len > 0) {
		exe_path[len] = '\0';
		char *slash = strrchr(exe_path, '/');
		if (slash != nullptr) {
			*slash = '\0';
			paths.push_back(std::string(exe_path) + "/shaders/texview/" + filename);
			paths.push_back(std::string(exe_path) + "/../../shaders/texview/" + filename);
		}
	}
#endif

	paths.push_back(std::string("shaders/texview/") + filename);

#if defined(RENEGADE_TEXVIEW_SHADER_DIR)
	paths.push_back(std::string(RENEGADE_TEXVIEW_SHADER_DIR) + "/" + filename);
#endif

	for (size_t i = 0; i < paths.size(); ++i) {
		if (ww3d_vulkan::Load_Spirv_From_File(paths[i].c_str(), out)) {
			fprintf(stderr, "texview: loaded shader %s\n", paths[i].c_str());
			return true;
		}
	}

	fprintf(stderr, "texview: failed to load shader %s\n", filename);
	return false;
}

} /* namespace */

bool TexViewRenderer::Init(SDL_Window *window, uint32_t width, uint32_t height)
{
	window_ = window;
	if (!VkContext::Get().Init(window, false)) {
		return false;
	}
	if (!Load_Shaders()) {
		return false;
	}
	extent_.width = width;
	extent_.height = height;
	if (!swapchain_.Create(extent_.width, extent_.height, vsync_)) {
		return false;
	}
	swapchain_format_ = swapchain_.Image_Format();
	if (!Create_Render_Pass()) {
		return false;
	}
	if (!Create_Framebuffers()) {
		return false;
	}
	if (!Create_Pipeline()) {
		return false;
	}
	if (!Create_Vertex_Buffer()) {
		return false;
	}
	if (!Create_Descriptor_Pool()) {
		return false;
	}
	if (!Create_Sync_Objects()) {
		return false;
	}
	return true;
}

bool TexViewRenderer::Load_Shaders()
{
	return Load_Texview_Spirv("blit.vert.spv", &vert_spirv_) &&
		Load_Texview_Spirv("blit.frag.spv", &frag_spirv_);
}

void TexViewRenderer::Shutdown()
{
	VkContext &ctx = VkContext::Get();
	if (ctx.Device() == VK_NULL_HANDLE) {
		return;
	}
	vkDeviceWaitIdle(ctx.Device());

	if (vertex_buffer_ != VK_NULL_HANDLE) {
		vkDestroyBuffer(ctx.Device(), vertex_buffer_, nullptr);
		vertex_buffer_ = VK_NULL_HANDLE;
	}
	if (vertex_memory_ != VK_NULL_HANDLE) {
		vkFreeMemory(ctx.Device(), vertex_memory_, nullptr);
		vertex_memory_ = VK_NULL_HANDLE;
	}

	Destroy_Pipeline();

	if (descriptor_pool_ != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(ctx.Device(), descriptor_pool_, nullptr);
		descriptor_pool_ = VK_NULL_HANDLE;
	}
	if (descriptor_layout_ != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(ctx.Device(), descriptor_layout_, nullptr);
		descriptor_layout_ = VK_NULL_HANDLE;
	}

	if (render_pass_ != VK_NULL_HANDLE) {
		vkDestroyRenderPass(ctx.Device(), render_pass_, nullptr);
		render_pass_ = VK_NULL_HANDLE;
	}

	Destroy_Framebuffers();
	swapchain_.Destroy();

	if (in_flight_ != VK_NULL_HANDLE) {
		vkDestroyFence(ctx.Device(), in_flight_, nullptr);
		in_flight_ = VK_NULL_HANDLE;
	}
	if (render_finished_ != VK_NULL_HANDLE) {
		vkDestroySemaphore(ctx.Device(), render_finished_, nullptr);
		render_finished_ = VK_NULL_HANDLE;
	}
	if (image_available_ != VK_NULL_HANDLE) {
		vkDestroySemaphore(ctx.Device(), image_available_, nullptr);
		image_available_ = VK_NULL_HANDLE;
	}

	VkContext::Get().Shutdown();
	frame_active_ = false;
}

void TexViewRenderer::Recreate_Swapchain()
{
	VkContext &ctx = VkContext::Get();
	vkDeviceWaitIdle(ctx.Device());

	Destroy_Framebuffers();
	swapchain_.Recreate(extent_.width, extent_.height, vsync_);
	swapchain_format_ = swapchain_.Image_Format();

	if (render_pass_ != VK_NULL_HANDLE) {
		Create_Framebuffers();
	}
}

void TexViewRenderer::Resize(uint32_t width, uint32_t height)
{
	if (width == 0 || height == 0) {
		return;
	}
	extent_.width = width;
	extent_.height = height;
	if (VkContext::Get().Is_Ready()) {
		Recreate_Swapchain();
	}
}

bool TexViewRenderer::Create_Render_Pass()
{
	VkAttachmentDescription color = {};
	color.format = swapchain_format_;
	color.samples = VK_SAMPLE_COUNT_1_BIT;
	color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference color_ref = {};
	color_ref.attachment = 0;
	color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_ref;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo rp_info = {};
	rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	rp_info.attachmentCount = 1;
	rp_info.pAttachments = &color;
	rp_info.subpassCount = 1;
	rp_info.pSubpasses = &subpass;
	rp_info.dependencyCount = 1;
	rp_info.pDependencies = &dependency;

	VkContext &ctx = VkContext::Get();
	return vkCreateRenderPass(ctx.Device(), &rp_info, nullptr, &render_pass_) == VK_SUCCESS;
}

bool TexViewRenderer::Create_Framebuffers()
{
	VkContext &ctx = VkContext::Get();
	const uint32_t image_count = swapchain_.Image_Count();
	framebuffers_.resize(image_count);
	for (uint32_t i = 0; i < image_count; ++i) {
		VkImageView view = swapchain_.Image_View(i);
		VkFramebufferCreateInfo fb_info = {};
		fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fb_info.renderPass = render_pass_;
		fb_info.attachmentCount = 1;
		fb_info.pAttachments = &view;
		fb_info.width = extent_.width;
		fb_info.height = extent_.height;
		fb_info.layers = 1;
		VK_CHECK(vkCreateFramebuffer(ctx.Device(), &fb_info, nullptr, &framebuffers_[i]));
	}
	return true;
}

void TexViewRenderer::Destroy_Framebuffers()
{
	VkContext &ctx = VkContext::Get();
	for (size_t i = 0; i < framebuffers_.size(); ++i) {
		if (framebuffers_[i] != VK_NULL_HANDLE) {
			vkDestroyFramebuffer(ctx.Device(), framebuffers_[i], nullptr);
		}
	}
	framebuffers_.clear();
}

bool TexViewRenderer::Create_Pipeline()
{
	VkContext &ctx = VkContext::Get();

	vert_shader_ = ww3d_vulkan::Load_Shader_Module(vert_spirv_);
	frag_shader_ = ww3d_vulkan::Load_Shader_Module(frag_spirv_);
	if (vert_shader_ == VK_NULL_HANDLE || frag_shader_ == VK_NULL_HANDLE) {
		return false;
	}

	VkDescriptorSetLayoutBinding sampler_binding = {};
	sampler_binding.binding = 0;
	sampler_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	sampler_binding.descriptorCount = 1;
	sampler_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo layout_info = {};
	layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_info.bindingCount = 1;
	layout_info.pBindings = &sampler_binding;
	VK_CHECK(vkCreateDescriptorSetLayout(
		ctx.Device(), &layout_info, nullptr, &descriptor_layout_));

	VkPushConstantRange push = {};
	push.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	push.offset = 0;
	push.size = sizeof(BlitPushConstants);

	VkPipelineLayoutCreateInfo pipeline_layout_info = {};
	pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_info.setLayoutCount = 1;
	pipeline_layout_info.pSetLayouts = &descriptor_layout_;
	pipeline_layout_info.pushConstantRangeCount = 1;
	pipeline_layout_info.pPushConstantRanges = &push;
	VK_CHECK(vkCreatePipelineLayout(
		ctx.Device(), &pipeline_layout_info, nullptr, &pipeline_layout_));

	VkPipelineShaderStageCreateInfo stages[2] = {};
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vert_shader_;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = frag_shader_;
	stages[1].pName = "main";

	VkVertexInputBindingDescription binding = {};
	binding.binding = 0;
	binding.stride = sizeof(BlitVertex);
	binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription attrs[2] = {};
	attrs[0].location = 0;
	attrs[0].binding = 0;
	attrs[0].format = VK_FORMAT_R32G32_SFLOAT;
	attrs[0].offset = offsetof(BlitVertex, x);
	attrs[1].location = 1;
	attrs[1].binding = 0;
	attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
	attrs[1].offset = offsetof(BlitVertex, u);

	VkPipelineVertexInputStateCreateInfo vertex_input = {};
	vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input.vertexBindingDescriptionCount = 1;
	vertex_input.pVertexBindingDescriptions = &binding;
	vertex_input.vertexAttributeDescriptionCount = 2;
	vertex_input.pVertexAttributeDescriptions = attrs;

	VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
	input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkPipelineViewportStateCreateInfo viewport_state = {};
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo raster = {};
	raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	raster.polygonMode = VK_POLYGON_MODE_FILL;
	raster.cullMode = VK_CULL_MODE_NONE;
	raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	raster.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo msaa = {};
	msaa.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState blend_att = {};
	blend_att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo blend = {};
	blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend.attachmentCount = 1;
	blend.pAttachments = &blend_att;

	VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo dynamic = {};
	dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic.dynamicStateCount = 2;
	dynamic.pDynamicStates = dynamic_states;

	VkGraphicsPipelineCreateInfo pipeline_info = {};
	pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_info.stageCount = 2;
	pipeline_info.pStages = stages;
	pipeline_info.pVertexInputState = &vertex_input;
	pipeline_info.pInputAssemblyState = &input_assembly;
	pipeline_info.pViewportState = &viewport_state;
	pipeline_info.pRasterizationState = &raster;
	pipeline_info.pMultisampleState = &msaa;
	pipeline_info.pColorBlendState = &blend;
	pipeline_info.pDynamicState = &dynamic;
	pipeline_info.layout = pipeline_layout_;
	pipeline_info.renderPass = render_pass_;
	pipeline_info.subpass = 0;

	VK_CHECK(vkCreateGraphicsPipelines(
		ctx.Device(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline_));
	return true;
}

void TexViewRenderer::Destroy_Pipeline()
{
	VkContext &ctx = VkContext::Get();
	if (pipeline_ != VK_NULL_HANDLE) {
		vkDestroyPipeline(ctx.Device(), pipeline_, nullptr);
		pipeline_ = VK_NULL_HANDLE;
	}
	if (pipeline_layout_ != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(ctx.Device(), pipeline_layout_, nullptr);
		pipeline_layout_ = VK_NULL_HANDLE;
	}
	if (frag_shader_ != VK_NULL_HANDLE) {
		ww3d_vulkan::Destroy_Shader_Module(frag_shader_);
		frag_shader_ = VK_NULL_HANDLE;
	}
	if (vert_shader_ != VK_NULL_HANDLE) {
		ww3d_vulkan::Destroy_Shader_Module(vert_shader_);
		vert_shader_ = VK_NULL_HANDLE;
	}
}

bool TexViewRenderer::Create_Vertex_Buffer()
{
	VkContext &ctx = VkContext::Get();
	const VkDeviceSize buffer_size = sizeof(BlitVertex) * 6;

	VkBufferCreateInfo buffer_info = {};
	buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.size = buffer_size;
	buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	VK_CHECK(vkCreateBuffer(ctx.Device(), &buffer_info, nullptr, &vertex_buffer_));

	VkMemoryRequirements req = {};
	vkGetBufferMemoryRequirements(ctx.Device(), vertex_buffer_, &req);
	VkMemoryAllocateInfo alloc = {};
	alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc.allocationSize = req.size;
	alloc.memoryTypeIndex = Find_Memory_Type(
		req.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	VK_CHECK(vkAllocateMemory(ctx.Device(), &alloc, nullptr, &vertex_memory_));
	VK_CHECK(vkBindBufferMemory(ctx.Device(), vertex_buffer_, vertex_memory_, 0));
	Update_Quad();
	return true;
}

bool TexViewRenderer::Create_Descriptor_Pool()
{
	VkContext &ctx = VkContext::Get();

	VkDescriptorPoolSize pool_size = {};
	pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool_size.descriptorCount = 1;

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.maxSets = 1;
	pool_info.poolSizeCount = 1;
	pool_info.pPoolSizes = &pool_size;
	VK_CHECK(vkCreateDescriptorPool(ctx.Device(), &pool_info, nullptr, &descriptor_pool_));

	VkDescriptorSetAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = descriptor_pool_;
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts = &descriptor_layout_;
	VK_CHECK(vkAllocateDescriptorSets(ctx.Device(), &alloc_info, &descriptor_set_));
	return true;
}

bool TexViewRenderer::Create_Sync_Objects()
{
	VkContext &ctx = VkContext::Get();

	VkSemaphoreCreateInfo sem_info = {};
	sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VK_CHECK(vkCreateSemaphore(ctx.Device(), &sem_info, nullptr, &image_available_));
	VK_CHECK(vkCreateSemaphore(ctx.Device(), &sem_info, nullptr, &render_finished_));

	VkFenceCreateInfo fence_info = {};
	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	VK_CHECK(vkCreateFence(ctx.Device(), &fence_info, nullptr, &in_flight_));

	VkCommandBufferAllocateInfo cmd_alloc = {};
	cmd_alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmd_alloc.commandPool = ctx.Command_Pool();
	cmd_alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmd_alloc.commandBufferCount = 1;
	VK_CHECK(vkAllocateCommandBuffers(ctx.Device(), &cmd_alloc, &command_buffer_));
	return true;
}

void TexViewRenderer::Set_Texture(const ww3d_vulkan::VkTexture *texture)
{
	texture_ = texture;
	if (texture_ == nullptr || texture_->Image() == VK_NULL_HANDLE) {
		return;
	}

	VkDescriptorImageInfo image_info = {};
	image_info.imageLayout = texture_->Layout();
	image_info.imageView = texture_->View();
	image_info.sampler = texture_->Sampler();

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = descriptor_set_;
	write.dstBinding = 0;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.descriptorCount = 1;
	write.pImageInfo = &image_info;

	VkContext &ctx = VkContext::Get();
	vkUpdateDescriptorSets(ctx.Device(), 1, &write, 0, nullptr);
	Update_Quad();
}

void TexViewRenderer::Update_Quad()
{
	if (vertex_buffer_ == VK_NULL_HANDLE) {
		return;
	}

	float quad_w = 2.0f;
	float quad_h = 2.0f;
	float u0 = 0.0f;
	float v0 = 0.0f;
	float u1 = 1.0f;
	float v1 = 1.0f;

	if (texture_ != nullptr && texture_->Width() > 0 && texture_->Height() > 0) {
		const float tex_w = (float)texture_->Width();
		const float tex_h = (float)texture_->Height();
		const float win_w = (float)extent_.width;
		const float win_h = (float)extent_.height;

		if (scale_mode_ == TEXVIEW_SCALE_ONE_TO_ONE && win_w > 0.0f && win_h > 0.0f) {
			quad_w = tex_w / win_w * 2.0f;
			quad_h = tex_h / win_h * 2.0f;
			if (quad_w > 2.0f) {
				quad_w = 2.0f;
			}
			if (quad_h > 2.0f) {
				quad_h = 2.0f;
			}
		} else if (win_w > 0.0f && win_h > 0.0f) {
			const float tex_aspect = tex_w / tex_h;
			const float win_aspect = win_w / win_h;
			if (tex_aspect > win_aspect) {
				quad_h = 2.0f * (win_aspect / tex_aspect);
			} else {
				quad_w = 2.0f * (tex_aspect / win_aspect);
			}
		}
	}

	const BlitVertex verts[6] = {
		{-quad_w * 0.5f, -quad_h * 0.5f, u0, v1},
		{quad_w * 0.5f, -quad_h * 0.5f, u1, v1},
		{quad_w * 0.5f, quad_h * 0.5f, u1, v0},
		{-quad_w * 0.5f, -quad_h * 0.5f, u0, v1},
		{quad_w * 0.5f, quad_h * 0.5f, u1, v0},
		{-quad_w * 0.5f, quad_h * 0.5f, u0, v0},
	};

	VkContext &ctx = VkContext::Get();
	void *mapped = nullptr;
	VK_CHECK(vkMapMemory(ctx.Device(), vertex_memory_, 0, sizeof(verts), 0, &mapped));
	memcpy(mapped, verts, sizeof(verts));
	vkUnmapMemory(ctx.Device(), vertex_memory_);
}

bool TexViewRenderer::Begin_Frame()
{
	VkContext &ctx = VkContext::Get();
	VK_CHECK(vkWaitForFences(ctx.Device(), 1, &in_flight_, VK_TRUE, UINT64_MAX));
	VK_CHECK(vkResetFences(ctx.Device(), 1, &in_flight_));

	if (!swapchain_.Acquire_Next_Image(0, image_available_, &current_image_)) {
		Recreate_Swapchain();
		return false;
	}

	VK_CHECK(vkResetCommandBuffer(command_buffer_, 0));
	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	VK_CHECK(vkBeginCommandBuffer(command_buffer_, &begin_info));

	VkClearValue clear = {};
	clear.color.float32[0] = 0.08f;
	clear.color.float32[1] = 0.08f;
	clear.color.float32[2] = 0.10f;
	clear.color.float32[3] = 1.0f;

	VkRenderPassBeginInfo rp_begin = {};
	rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rp_begin.renderPass = render_pass_;
	rp_begin.framebuffer = framebuffers_[current_image_];
	rp_begin.renderArea.extent = extent_;
	rp_begin.clearValueCount = 1;
	rp_begin.pClearValues = &clear;

	vkCmdBeginRenderPass(command_buffer_, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport = {};
	viewport.width = (float)extent_.width;
	viewport.height = (float)extent_.height;
	viewport.maxDepth = 1.0f;
	VkRect2D scissor = {};
	scissor.extent = extent_;
	vkCmdSetViewport(command_buffer_, 0, 1, &viewport);
	vkCmdSetScissor(command_buffer_, 0, 1, &scissor);

	Record_Draw(command_buffer_);
	frame_active_ = true;
	return true;
}

void TexViewRenderer::Record_Draw(VkCommandBuffer cmd)
{
	if (texture_ == nullptr || texture_->Image() == VK_NULL_HANDLE) {
		return;
	}

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
	vkCmdBindDescriptorSets(
		cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1, &descriptor_set_, 0, nullptr);

	BlitPushConstants push = {};
	push.tex_size[0] = (float)texture_->Width();
	push.tex_size[1] = (float)texture_->Height();
	push.show_alpha_bg = show_alpha_bg_ ? 1.0f : 0.0f;
	vkCmdPushConstants(
		cmd,
		pipeline_layout_,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		0,
		sizeof(push),
		&push);

	VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(cmd, 0, 1, &vertex_buffer_, &offset);
	vkCmdDraw(cmd, 6, 1, 0, 0);
}

bool TexViewRenderer::End_Frame()
{
	if (!frame_active_) {
		return false;
	}

	vkCmdEndRenderPass(command_buffer_);
	VK_CHECK(vkEndCommandBuffer(command_buffer_));

	VkContext &ctx = VkContext::Get();
	VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submit = {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.waitSemaphoreCount = 1;
	submit.pWaitSemaphores = &image_available_;
	submit.pWaitDstStageMask = &wait_stage;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &command_buffer_;
	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores = &render_finished_;
	VK_CHECK(vkQueueSubmit(ctx.Graphics_Queue(), 1, &submit, in_flight_));

	if (!swapchain_.Present(0, render_finished_, current_image_)) {
		Recreate_Swapchain();
	}

	frame_active_ = false;
	return true;
}
