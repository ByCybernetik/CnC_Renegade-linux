#include "w3dviewer_renderer.h"
#include "w3dviewer_loader.h"
#include "w3dviewer_scene.h"

#include "htree.h"

#include <memory>

#include "../ww3d2_vulkan/vk_context.h"
#include "../ww3d2_vulkan/vk_platform.h"
#include "../ww3d2_vulkan/vk_swapchain.h"
#include "../ww3d2_vulkan/vk_check.h"
#include "../ww3d2_vulkan/vk_shader.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits.h>
#include <string>
#include <vector>

#if defined(__linux__)
#include <unistd.h>
#endif

using ww3d_vulkan::VkContext;

namespace {

struct MeshVertex {
	float pos[3];
	float normal[3];
	float uv[2];
};

struct PushData {
	float mvp[16];
	uint32_t use_texture;
	uint32_t alpha_test;
	uint32_t pad[2];
};

static void Mat4_Mul(float *out, const float *a, const float *b)
{
	float tmp[16];
	for (int col = 0; col < 4; ++col) {
		for (int row = 0; row < 4; ++row) {
			tmp[col * 4 + row] =
				a[0 * 4 + row] * b[col * 4 + 0] +
				a[1 * 4 + row] * b[col * 4 + 1] +
				a[2 * 4 + row] * b[col * 4 + 2] +
				a[3 * 4 + row] * b[col * 4 + 3];
		}
	}
	memcpy(out, tmp, sizeof(tmp));
}

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

static bool Load_W3DViewer_Spirv(const char *filename, std::vector<uint32_t> *out)
{
	std::vector<std::string> paths;

	const char *env = getenv("RENEGADE_SHADER_PATH");
	if (env != nullptr && env[0] != '\0') {
		paths.push_back(std::string(env) + "/w3dviewer/" + filename);
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
			paths.push_back(std::string(exe_path) + "/shaders/w3dviewer/" + filename);
			paths.push_back(std::string(exe_path) + "/../../shaders/w3dviewer/" + filename);
		}
	}
#endif

	paths.push_back(std::string("shaders/w3dviewer/") + filename);

#if defined(RENEGADE_W3DVIEWER_SHADER_DIR)
	paths.push_back(std::string(RENEGADE_W3DVIEWER_SHADER_DIR) + "/" + filename);
#endif

	for (size_t i = 0; i < paths.size(); ++i) {
		if (ww3d_vulkan::Load_Spirv_From_File(paths[i].c_str(), out)) {
			fprintf(stderr, "w3dviewer: loaded shader %s\n", paths[i].c_str());
			return true;
		}
	}

	fprintf(stderr, "w3dviewer: failed to load shader %s\n", filename);
	return false;
}

static bool Create_Buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags mem_props,
	VkBuffer *out_buffer, VkDeviceMemory *out_memory)
{
	VkContext &ctx = VkContext::Get();

	VkBufferCreateInfo buffer_info = {};
	buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.size = size;
	buffer_info.usage = usage;
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK(vkCreateBuffer(ctx.Device(), &buffer_info, nullptr, out_buffer));

	VkMemoryRequirements req = {};
	vkGetBufferMemoryRequirements(ctx.Device(), *out_buffer, &req);

	VkMemoryAllocateInfo alloc = {};
	alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc.allocationSize = req.size;
	alloc.memoryTypeIndex = Find_Memory_Type(req.memoryTypeBits, mem_props);
	VK_CHECK(vkAllocateMemory(ctx.Device(), &alloc, nullptr, out_memory));
	VK_CHECK(vkBindBufferMemory(ctx.Device(), *out_buffer, *out_memory, 0));
	return true;
}

} /* namespace */

bool W3DViewerRenderer::Init(SDL_Window *window, uint32_t width, uint32_t height)
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
	if (!Create_Depth_Buffer()) {
		return false;
	}
	if (!Create_Render_Pass()) {
		return false;
	}
	if (!Create_Framebuffers()) {
		return false;
	}
	if (!Create_Descriptor_Layout()) {
		return false;
	}
	if (!Create_Pipelines()) {
		return false;
	}
	if (!Create_Sync_Objects()) {
		return false;
	}
	if (!Create_Fallback_Texture()) {
		return false;
	}
	float identity[16] = {
		1,0,0,0,
		0,1,0,0,
		0,0,1,0,
		0,0,0,1
	};
	memcpy(view_proj_, identity, sizeof(view_proj_));
	return true;
}

bool W3DViewerRenderer::Load_Shaders()
{
	return Load_W3DViewer_Spirv("mesh.vert.spv", &vert_spirv_) &&
		Load_W3DViewer_Spirv("mesh.frag.spv", &frag_spirv_);
}

void W3DViewerRenderer::Shutdown()
{
	VkContext &ctx = VkContext::Get();
	if (ctx.Device() == VK_NULL_HANDLE) {
		return;
	}
	vkDeviceWaitIdle(ctx.Device());

	Destroy_Mesh_Buffers();
	Destroy_Textures();
	Destroy_Descriptors();
	Destroy_Pipeline();

	if (render_pass_ != VK_NULL_HANDLE) {
		vkDestroyRenderPass(ctx.Device(), render_pass_, nullptr);
		render_pass_ = VK_NULL_HANDLE;
	}

	if (depth_view_ != VK_NULL_HANDLE) {
		vkDestroyImageView(ctx.Device(), depth_view_, nullptr);
		depth_view_ = VK_NULL_HANDLE;
	}
	if (depth_image_ != VK_NULL_HANDLE) {
		vkDestroyImage(ctx.Device(), depth_image_, nullptr);
		depth_image_ = VK_NULL_HANDLE;
	}
	if (depth_memory_ != VK_NULL_HANDLE) {
		vkFreeMemory(ctx.Device(), depth_memory_, nullptr);
		depth_memory_ = VK_NULL_HANDLE;
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

bool W3DViewerRenderer::Create_Depth_Buffer()
{
	VkContext &ctx = VkContext::Get();

	VkImageCreateInfo img_info = {};
	img_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	img_info.imageType = VK_IMAGE_TYPE_2D;
	img_info.format = depth_format_;
	img_info.extent.width = extent_.width;
	img_info.extent.height = extent_.height;
	img_info.extent.depth = 1;
	img_info.mipLevels = 1;
	img_info.arrayLayers = 1;
	img_info.samples = VK_SAMPLE_COUNT_1_BIT;
	img_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	img_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	img_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VK_CHECK(vkCreateImage(ctx.Device(), &img_info, nullptr, &depth_image_));

	VkMemoryRequirements req = {};
	vkGetImageMemoryRequirements(ctx.Device(), depth_image_, &req);
	VkMemoryAllocateInfo alloc = {};
	alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc.allocationSize = req.size;
	alloc.memoryTypeIndex = Find_Memory_Type(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK(vkAllocateMemory(ctx.Device(), &alloc, nullptr, &depth_memory_));
	VK_CHECK(vkBindImageMemory(ctx.Device(), depth_image_, depth_memory_, 0));

	VkImageViewCreateInfo view_info = {};
	view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_info.image = depth_image_;
	view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_info.format = depth_format_;
	view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	view_info.subresourceRange.levelCount = 1;
	view_info.subresourceRange.layerCount = 1;
	VK_CHECK(vkCreateImageView(ctx.Device(), &view_info, nullptr, &depth_view_));
	return true;
}

bool W3DViewerRenderer::Create_Render_Pass()
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

	VkAttachmentDescription depth = {};
	depth.format = depth_format_;
	depth.samples = VK_SAMPLE_COUNT_1_BIT;
	depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depth_ref = {};
	depth_ref.attachment = 1;
	depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_ref;
	subpass.pDepthStencilAttachment = &depth_ref;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	VkAttachmentDescription attachments[2] = {color, depth};
	VkRenderPassCreateInfo rp_info = {};
	rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	rp_info.attachmentCount = 2;
	rp_info.pAttachments = attachments;
	rp_info.subpassCount = 1;
	rp_info.pSubpasses = &subpass;
	rp_info.dependencyCount = 1;
	rp_info.pDependencies = &dependency;

	VkContext &ctx = VkContext::Get();
	return vkCreateRenderPass(ctx.Device(), &rp_info, nullptr, &render_pass_) == VK_SUCCESS;
}

bool W3DViewerRenderer::Create_Framebuffers()
{
	VkContext &ctx = VkContext::Get();
	const uint32_t image_count = swapchain_.Image_Count();
	framebuffers_.resize(image_count);
	for (uint32_t i = 0; i < image_count; ++i) {
		VkImageView attachments[2] = {swapchain_.Image_View(i), depth_view_};
		VkFramebufferCreateInfo fb_info = {};
		fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fb_info.renderPass = render_pass_;
		fb_info.attachmentCount = 2;
		fb_info.pAttachments = attachments;
		fb_info.width = extent_.width;
		fb_info.height = extent_.height;
		fb_info.layers = 1;
		VK_CHECK(vkCreateFramebuffer(ctx.Device(), &fb_info, nullptr, &framebuffers_[i]));
	}
	return true;
}

void W3DViewerRenderer::Destroy_Framebuffers()
{
	VkContext &ctx = VkContext::Get();
	for (size_t i = 0; i < framebuffers_.size(); ++i) {
		if (framebuffers_[i] != VK_NULL_HANDLE) {
			vkDestroyFramebuffer(ctx.Device(), framebuffers_[i], nullptr);
		}
	}
	framebuffers_.clear();
}

bool W3DViewerRenderer::Create_Descriptor_Layout()
{
	VkContext &ctx = VkContext::Get();

	VkDescriptorSetLayoutBinding sampler_binding = {};
	sampler_binding.binding = 0;
	sampler_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	sampler_binding.descriptorCount = 1;
	sampler_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo layout_info = {};
	layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_info.bindingCount = 1;
	layout_info.pBindings = &sampler_binding;
	return vkCreateDescriptorSetLayout(ctx.Device(), &layout_info, nullptr, &descriptor_set_layout_) ==
		VK_SUCCESS;
}

bool W3DViewerRenderer::Create_Descriptor_Pool(uint32_t set_count)
{
	VkContext &ctx = VkContext::Get();
	Destroy_Descriptors();

	if (set_count == 0) {
		return true;
	}

	VkDescriptorPoolSize pool_size = {};
	pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool_size.descriptorCount = set_count;

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.maxSets = set_count;
	pool_info.poolSizeCount = 1;
	pool_info.pPoolSizes = &pool_size;
	return vkCreateDescriptorPool(ctx.Device(), &pool_info, nullptr, &descriptor_pool_) == VK_SUCCESS;
}

static void Write_Texture_Descriptor(
	VkDevice device,
	VkDescriptorSet set,
	const ww3d_vulkan::VkTexture *tex)
{
	VkDescriptorImageInfo image_info = {};
	image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	image_info.imageView = tex->View();
	image_info.sampler = tex->Sampler();

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = set;
	write.dstBinding = 0;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.descriptorCount = 1;
	write.pImageInfo = &image_info;
	vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

bool W3DViewerRenderer::Create_Fallback_Texture()
{
	return fallback_texture_.Create_White_Fallback();
}

bool W3DViewerRenderer::Create_Pipelines()
{
	VkContext &ctx = VkContext::Get();

	vert_shader_ = ww3d_vulkan::Load_Shader_Module(vert_spirv_);
	frag_shader_ = ww3d_vulkan::Load_Shader_Module(frag_spirv_);
	if (vert_shader_ == VK_NULL_HANDLE || frag_shader_ == VK_NULL_HANDLE) {
		return false;
	}

	VkPushConstantRange push = {};
	push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	push.offset = 0;
	push.size = sizeof(PushData);

	VkPipelineLayoutCreateInfo layout_info = {};
	layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layout_info.setLayoutCount = 1;
	layout_info.pSetLayouts = &descriptor_set_layout_;
	layout_info.pushConstantRangeCount = 1;
	layout_info.pPushConstantRanges = &push;
	VK_CHECK(vkCreatePipelineLayout(ctx.Device(), &layout_info, nullptr, &pipeline_layout_));

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
	binding.stride = sizeof(MeshVertex);
	binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription attrs[3] = {};
	attrs[0].location = 0;
	attrs[0].binding = 0;
	attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attrs[0].offset = offsetof(MeshVertex, pos);
	attrs[1].location = 1;
	attrs[1].binding = 0;
	attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attrs[1].offset = offsetof(MeshVertex, normal);
	attrs[2].location = 2;
	attrs[2].binding = 0;
	attrs[2].format = VK_FORMAT_R32G32_SFLOAT;
	attrs[2].offset = offsetof(MeshVertex, uv);

	VkPipelineVertexInputStateCreateInfo vertex_input = {};
	vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input.vertexBindingDescriptionCount = 1;
	vertex_input.pVertexBindingDescriptions = &binding;
	vertex_input.vertexAttributeDescriptionCount = 3;
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
	/* W3D/D3D meshes: CW front with flipped viewport (same as main Vulkan backend). */
	raster.cullMode = VK_CULL_MODE_NONE;
	raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
	raster.depthBiasEnable = VK_TRUE;
	raster.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo msaa = {};
	msaa.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkDynamicState dynamic_states[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
		VK_DYNAMIC_STATE_DEPTH_BIAS,
	};
	VkPipelineDynamicStateCreateInfo dynamic = {};
	dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic.dynamicStateCount = 3;
	dynamic.pDynamicStates = dynamic_states;

	auto create_pipeline = [&](bool alpha_blend, VkPipeline *out_pipeline) -> bool {
		VkPipelineDepthStencilStateCreateInfo depth_stencil = {};
		depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depth_stencil.depthTestEnable = VK_TRUE;
		depth_stencil.depthWriteEnable = alpha_blend ? VK_FALSE : VK_TRUE;
		depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;

		VkPipelineColorBlendAttachmentState blend_att = {};
		blend_att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		if (alpha_blend) {
			blend_att.blendEnable = VK_TRUE;
			blend_att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			blend_att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			blend_att.colorBlendOp = VK_BLEND_OP_ADD;
			blend_att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			blend_att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			blend_att.alphaBlendOp = VK_BLEND_OP_ADD;
		}

		VkPipelineColorBlendStateCreateInfo blend = {};
		blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		blend.attachmentCount = 1;
		blend.pAttachments = &blend_att;

		VkGraphicsPipelineCreateInfo pipeline_info = {};
		pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline_info.stageCount = 2;
		pipeline_info.pStages = stages;
		pipeline_info.pVertexInputState = &vertex_input;
		pipeline_info.pInputAssemblyState = &input_assembly;
		pipeline_info.pViewportState = &viewport_state;
		pipeline_info.pRasterizationState = &raster;
		pipeline_info.pMultisampleState = &msaa;
		pipeline_info.pDepthStencilState = &depth_stencil;
		pipeline_info.pColorBlendState = &blend;
		pipeline_info.pDynamicState = &dynamic;
		pipeline_info.layout = pipeline_layout_;
		pipeline_info.renderPass = render_pass_;
		pipeline_info.subpass = 0;

		return vkCreateGraphicsPipelines(
			ctx.Device(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, out_pipeline) == VK_SUCCESS;
	};

	return create_pipeline(false, &pipeline_opaque_) && create_pipeline(true, &pipeline_alpha_);
}

void W3DViewerRenderer::Destroy_Descriptors()
{
	VkContext &ctx = VkContext::Get();
	texture_descriptor_sets_.clear();
	fallback_descriptor_set_ = VK_NULL_HANDLE;
	if (descriptor_pool_ != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(ctx.Device(), descriptor_pool_, nullptr);
		descriptor_pool_ = VK_NULL_HANDLE;
	}
}

void W3DViewerRenderer::Destroy_Textures()
{
	textures_.clear();
	draw_batches_.clear();
}

void W3DViewerRenderer::Destroy_Pipeline()
{
	VkContext &ctx = VkContext::Get();
	if (pipeline_opaque_ != VK_NULL_HANDLE) {
		vkDestroyPipeline(ctx.Device(), pipeline_opaque_, nullptr);
		pipeline_opaque_ = VK_NULL_HANDLE;
	}
	if (pipeline_alpha_ != VK_NULL_HANDLE) {
		vkDestroyPipeline(ctx.Device(), pipeline_alpha_, nullptr);
		pipeline_alpha_ = VK_NULL_HANDLE;
	}
	if (pipeline_layout_ != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(ctx.Device(), pipeline_layout_, nullptr);
		pipeline_layout_ = VK_NULL_HANDLE;
	}
	if (descriptor_set_layout_ != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(ctx.Device(), descriptor_set_layout_, nullptr);
		descriptor_set_layout_ = VK_NULL_HANDLE;
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

bool W3DViewerRenderer::Load_Mesh_Textures(const W3DViewerMesh *mesh, const std::string &extra_tex_dir)
{
	Destroy_Textures();
	if (!mesh) {
		return false;
	}

	VkContext &ctx = VkContext::Get();
	const uint32_t tex_count = (uint32_t)mesh->texture_names.size();
	const uint32_t pool_sets = tex_count + 1;
	if (!Create_Descriptor_Pool(pool_sets)) {
		return false;
	}
	fallback_descriptor_set_ = VK_NULL_HANDLE;

	textures_.resize(tex_count);
	texture_descriptor_sets_.resize(tex_count, VK_NULL_HANDLE);

	for (uint32_t i = 0; i < tex_count; ++i) {
		std::string resolved;
		if (!w3dviewer::Resolve_Texture_Path(
				mesh->texture_names[i],
				mesh->texture_search_dir,
				extra_tex_dir,
				&resolved)) {
			fprintf(
				stderr,
				"w3dviewer: texture not found: %s\n",
				mesh->texture_names[i].c_str());
			textures_[i].reset(new w3dviewer::GpuTexture());
			textures_[i]->Create_White_Fallback();
		} else {
			textures_[i].reset(new w3dviewer::GpuTexture());
			if (!textures_[i]->Load_From_File(resolved)) {
				textures_[i]->Create_White_Fallback();
			} else {
				fprintf(stderr, "w3dviewer: loaded texture %s\n", resolved.c_str());
			}
		}

		VkDescriptorSetAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_info.descriptorPool = descriptor_pool_;
		alloc_info.descriptorSetCount = 1;
		alloc_info.pSetLayouts = &descriptor_set_layout_;
		if (vkAllocateDescriptorSets(ctx.Device(), &alloc_info, &texture_descriptor_sets_[i]) != VK_SUCCESS) {
			return false;
		}
		Write_Texture_Descriptor(ctx.Device(), texture_descriptor_sets_[i], textures_[i]->Get());
	}

	VkDescriptorSetAllocateInfo fallback_alloc = {};
	fallback_alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	fallback_alloc.descriptorPool = descriptor_pool_;
	fallback_alloc.descriptorSetCount = 1;
	fallback_alloc.pSetLayouts = &descriptor_set_layout_;
	VkDescriptorSet fallback_set = VK_NULL_HANDLE;
	if (vkAllocateDescriptorSets(ctx.Device(), &fallback_alloc, &fallback_set) != VK_SUCCESS) {
		return false;
	}
	Write_Texture_Descriptor(ctx.Device(), fallback_set, fallback_texture_.Get());
	fallback_descriptor_set_ = fallback_set;

	draw_batches_.clear();
	uint32_t offset = 0;
	for (size_t i = 0; i < mesh->draw_batches.size(); ++i) {
		const W3DViewerDrawBatch &src = mesh->draw_batches[i];
		GpuDrawBatch batch;
		batch.index_offset = offset;
		batch.index_count = (uint32_t)src.indices.size();
		offset += batch.index_count;

		if (src.texture_index >= 0 && (size_t)src.texture_index < texture_descriptor_sets_.size()) {
			batch.descriptor_set = texture_descriptor_sets_[(size_t)src.texture_index];
			batch.use_texture = mesh->has_uvs;
		} else {
			batch.descriptor_set = fallback_set;
			batch.use_texture = false;
		}
		batch.alpha_blend = src.alpha_blend;
		batch.alpha_test = src.alpha_test;
		batch.depth_write = src.depth_write;
		draw_batches_.push_back(batch);
	}

	return true;
}

bool W3DViewerRenderer::Upload_SubMesh_Geometry(const W3DViewerMesh *mesh, GpuSubMesh *out)
{
	if (!mesh || mesh->vertex_count == 0 || out == nullptr) {
		return false;
	}

	bool has_indices = !mesh->indices.empty();
	if (!has_indices) {
		for (size_t b = 0; b < mesh->draw_batches.size(); ++b) {
			if (!mesh->draw_batches[b].indices.empty()) {
				has_indices = true;
				break;
			}
		}
	}
	if (!has_indices) {
		return false;
	}

	VkContext &ctx = VkContext::Get();
	std::vector<MeshVertex> verts;
	verts.resize(mesh->vertex_count);
	for (uint32_t i = 0; i < mesh->vertex_count; ++i) {
		verts[i].pos[0] = mesh->positions[i * 3 + 0];
		verts[i].pos[1] = mesh->positions[i * 3 + 1];
		verts[i].pos[2] = mesh->positions[i * 3 + 2];
		verts[i].normal[0] = mesh->normals[i * 3 + 0];
		verts[i].normal[1] = mesh->normals[i * 3 + 1];
		verts[i].normal[2] = mesh->normals[i * 3 + 2];
		if (mesh->has_uvs && i * 2 + 1 < mesh->uvs.size()) {
			verts[i].uv[0] = mesh->uvs[i * 2 + 0];
			verts[i].uv[1] = mesh->uvs[i * 2 + 1];
		} else {
			verts[i].uv[0] = 0.0f;
			verts[i].uv[1] = 0.0f;
		}
	}

	std::vector<uint16_t> all_indices;
	if (!mesh->draw_batches.empty()) {
		for (size_t b = 0; b < mesh->draw_batches.size(); ++b) {
			const W3DViewerDrawBatch &batch = mesh->draw_batches[b];
			all_indices.insert(all_indices.end(), batch.indices.begin(), batch.indices.end());
		}
	} else {
		all_indices = mesh->indices;
	}

	const VkDeviceSize vbuf_size = sizeof(MeshVertex) * verts.size();
	const VkDeviceSize ibuf_size = sizeof(uint16_t) * all_indices.size();

	VkBuffer staging_buf = VK_NULL_HANDLE;
	VkDeviceMemory staging_mem = VK_NULL_HANDLE;
	Create_Buffer(vbuf_size + ibuf_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&staging_buf, &staging_mem);

	void *mapped = nullptr;
	vkMapMemory(ctx.Device(), staging_mem, 0, vbuf_size + ibuf_size, 0, &mapped);
	memcpy(mapped, verts.data(), (size_t)vbuf_size);
	memcpy(static_cast<uint8_t *>(mapped) + vbuf_size, all_indices.data(), (size_t)ibuf_size);
	vkUnmapMemory(ctx.Device(), staging_mem);

	Create_Buffer(vbuf_size,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&out->vertex_buffer, &out->vertex_memory);
	Create_Buffer(ibuf_size,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&out->index_buffer, &out->index_memory);

	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkCommandBufferAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandPool = ctx.Command_Pool();
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = 1;
	vkAllocateCommandBuffers(ctx.Device(), &alloc_info, &cmd);

	VkCommandBufferBeginInfo begin = {};
	begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &begin);

	VkBufferCopy copy = {};
	copy.size = vbuf_size;
	vkCmdCopyBuffer(cmd, staging_buf, out->vertex_buffer, 1, &copy);
	copy.srcOffset = vbuf_size;
	copy.dstOffset = 0;
	copy.size = ibuf_size;
	vkCmdCopyBuffer(cmd, staging_buf, out->index_buffer, 1, &copy);

	vkEndCommandBuffer(cmd);
	VkSubmitInfo submit = {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &cmd;
	vkQueueSubmit(ctx.Graphics_Queue(), 1, &submit, VK_NULL_HANDLE);
	vkQueueWaitIdle(ctx.Graphics_Queue());
	vkFreeCommandBuffers(ctx.Device(), ctx.Command_Pool(), 1, &cmd);

	vkDestroyBuffer(ctx.Device(), staging_buf, nullptr);
	vkFreeMemory(ctx.Device(), staging_mem, nullptr);

	out->index_count = (uint32_t)all_indices.size();
	return true;
}

void W3DViewerRenderer::Build_SubMesh_Draw_Batches(
	const W3DViewerMesh *mesh,
	GpuSubMesh *out,
	const std::vector<int32_t> *texture_remap)
{
	if (!mesh || out == nullptr) {
		return;
	}
	out->draw_batches.clear();
	uint32_t offset = 0;
	for (size_t i = 0; i < mesh->draw_batches.size(); ++i) {
		const W3DViewerDrawBatch &src = mesh->draw_batches[i];
		GpuDrawBatch batch;
		batch.index_offset = offset;
		batch.index_count = (uint32_t)src.indices.size();
		offset += batch.index_count;

		int32_t tex_index = src.texture_index;
		if (texture_remap != nullptr && tex_index >= 0 &&
			(size_t)tex_index < texture_remap->size()) {
			tex_index = texture_remap->at((size_t)tex_index);
		}
		if (tex_index >= 0 && (size_t)tex_index < texture_descriptor_sets_.size()) {
			batch.descriptor_set = texture_descriptor_sets_[(size_t)tex_index];
			batch.use_texture = mesh->has_uvs;
		} else {
			batch.descriptor_set = fallback_descriptor_set_;
			batch.use_texture = false;
		}
		batch.alpha_blend = src.alpha_blend;
		batch.alpha_test = src.alpha_test;
		batch.depth_write = src.depth_write;
		out->draw_batches.push_back(batch);
	}
}

bool W3DViewerRenderer::Create_Mesh_Buffers(const W3DViewerMesh *mesh)
{
	Destroy_Mesh_Buffers();
	GpuSubMesh submesh;
	if (!Upload_SubMesh_Geometry(mesh, &submesh)) {
		return false;
	}
	Build_SubMesh_Draw_Batches(mesh, &submesh);
	draw_batches_ = submesh.draw_batches;
	sub_meshes_.push_back(submesh);
	return true;
}

void W3DViewerRenderer::Destroy_Mesh_Buffers()
{
	VkContext &ctx = VkContext::Get();
	for (size_t i = 0; i < sub_meshes_.size(); ++i) {
		GpuSubMesh &sub = sub_meshes_[i];
		if (sub.vertex_buffer != VK_NULL_HANDLE) {
			vkDestroyBuffer(ctx.Device(), sub.vertex_buffer, nullptr);
			sub.vertex_buffer = VK_NULL_HANDLE;
		}
		if (sub.vertex_memory != VK_NULL_HANDLE) {
			vkFreeMemory(ctx.Device(), sub.vertex_memory, nullptr);
			sub.vertex_memory = VK_NULL_HANDLE;
		}
		if (sub.index_buffer != VK_NULL_HANDLE) {
			vkDestroyBuffer(ctx.Device(), sub.index_buffer, nullptr);
			sub.index_buffer = VK_NULL_HANDLE;
		}
		if (sub.index_memory != VK_NULL_HANDLE) {
			vkFreeMemory(ctx.Device(), sub.index_memory, nullptr);
			sub.index_memory = VK_NULL_HANDLE;
		}
	}
	sub_meshes_.clear();
	draw_batches_.clear();
}

bool W3DViewerRenderer::Create_Sync_Objects()
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

static int32_t Find_Or_Add_Texture(std::vector<std::string> *names, const std::string &name)
{
	for (size_t i = 0; i < names->size(); ++i) {
#if defined(_WIN32)
		if (_stricmp(names->at(i).c_str(), name.c_str()) == 0) {
#else
		if (strcasecmp(names->at(i).c_str(), name.c_str()) == 0) {
#endif
			return (int32_t)i;
		}
	}
	names->push_back(name);
	return (int32_t)names->size() - 1;
}

void W3DViewerRenderer::Set_Mesh(const W3DViewerMesh *mesh, const std::string &extra_tex_dir)
{
	mesh_ = mesh;
	scene_ = nullptr;
	animated_scene_ = false;
	if (mesh_) {
		if (!Load_Mesh_Textures(mesh_, extra_tex_dir)) {
			fprintf(stderr, "w3dviewer: texture setup failed\n");
		}
		if (!Create_Mesh_Buffers(mesh_)) {
			fprintf(stderr, "w3dviewer: mesh buffer upload failed\n");
		}
	} else {
		Destroy_Mesh_Buffers();
		Destroy_Textures();
	}
}

void W3DViewerRenderer::Set_Scene(const W3DViewerScene *scene, const std::string &extra_tex_dir)
{
	mesh_ = nullptr;
	scene_ = scene;
	animated_scene_ = (scene != nullptr && scene->is_animated);
	Destroy_Mesh_Buffers();
	Destroy_Textures();

	if (!animated_scene_ || scene == nullptr) {
		return;
	}

	std::vector<std::string> all_textures;
	std::vector<std::vector<int32_t>> remaps;
	for (size_t s = 0; s < scene->sub_objects.size(); ++s) {
		const W3DViewerMesh &sub_mesh = scene->sub_objects[s].mesh;
		std::vector<int32_t> remap;
		remap.resize(sub_mesh.texture_names.size());
		for (size_t t = 0; t < sub_mesh.texture_names.size(); ++t) {
			remap[t] = Find_Or_Add_Texture(&all_textures, sub_mesh.texture_names[t]);
		}
		remaps.push_back(remap);
	}

	W3DViewerMesh tex_holder;
	tex_holder.texture_names = all_textures;
	if (!scene->sub_objects.empty()) {
		tex_holder.texture_search_dir = scene->sub_objects[0].mesh.texture_search_dir;
	}
	if (!all_textures.empty()) {
		if (!Load_Mesh_Textures(&tex_holder, extra_tex_dir)) {
			fprintf(stderr, "w3dviewer: scene texture setup failed\n");
		}
	} else if (!Create_Descriptor_Pool(1)) {
		fprintf(stderr, "w3dviewer: descriptor pool setup failed\n");
	} else {
		VkContext &ctx = VkContext::Get();
		VkDescriptorSetAllocateInfo fallback_alloc = {};
		fallback_alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		fallback_alloc.descriptorPool = descriptor_pool_;
		fallback_alloc.descriptorSetCount = 1;
		fallback_alloc.pSetLayouts = &descriptor_set_layout_;
		VkDescriptorSet fallback_set = VK_NULL_HANDLE;
		if (vkAllocateDescriptorSets(ctx.Device(), &fallback_alloc, &fallback_set) == VK_SUCCESS) {
			Write_Texture_Descriptor(ctx.Device(), fallback_set, fallback_texture_.Get());
			fallback_descriptor_set_ = fallback_set;
		}
	}

	for (size_t s = 0; s < scene->sub_objects.size(); ++s) {
		const W3DViewerSubObject &sub = scene->sub_objects[s];
		GpuSubMesh gpu_sub;
		gpu_sub.bone_index = sub.bone_index;
		if (!Upload_SubMesh_Geometry(&sub.mesh, &gpu_sub)) {
			fprintf(stderr, "w3dviewer: sub-mesh upload failed: %s\n", sub.name.c_str());
			continue;
		}
		Build_SubMesh_Draw_Batches(&sub.mesh, &gpu_sub, &remaps[s]);
		sub_meshes_.push_back(gpu_sub);
	}
}

void W3DViewerRenderer::Set_MVP(const float *mvp_4x4)
{
	memcpy(view_proj_, mvp_4x4, sizeof(view_proj_));
}

bool W3DViewerRenderer::Begin_Frame()
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

	VkClearValue clears[2] = {};
	clears[0].color.float32[0] = 0.15f;
	clears[0].color.float32[1] = 0.15f;
	clears[0].color.float32[2] = 0.18f;
	clears[0].color.float32[3] = 1.0f;
	clears[1].depthStencil.depth = 1.0f;
	clears[1].depthStencil.stencil = 0;

	VkRenderPassBeginInfo rp_begin = {};
	rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rp_begin.renderPass = render_pass_;
	rp_begin.framebuffer = framebuffers_[current_image_];
	rp_begin.renderArea.extent = extent_;
	rp_begin.clearValueCount = 2;
	rp_begin.pClearValues = clears;

	vkCmdBeginRenderPass(command_buffer_, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = (float)extent_.height;
	viewport.width = (float)extent_.width;
	viewport.height = -(float)extent_.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	VkRect2D scissor = {};
	scissor.extent = extent_;
	vkCmdSetViewport(command_buffer_, 0, 1, &viewport);
	vkCmdSetScissor(command_buffer_, 0, 1, &scissor);

	Record_Draw(command_buffer_);
	frame_active_ = true;
	return true;
}

void W3DViewerRenderer::Draw_Batches(
	VkCommandBuffer cmd,
	VkPipeline pipeline,
	const float *mvp,
	VkBuffer vertex_buffer,
	VkBuffer index_buffer,
	uint32_t index_count,
	const std::vector<GpuDrawBatch> &batches,
	bool alpha_blend_pass,
	bool alpha_test_flag)
{
	if (index_count == 0 || vertex_buffer == VK_NULL_HANDLE || index_buffer == VK_NULL_HANDLE) {
		return;
	}

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(cmd, 0, 1, &vertex_buffer, &offset);
	vkCmdBindIndexBuffer(cmd, index_buffer, 0, VK_INDEX_TYPE_UINT16);

	PushData push = {};
	memcpy(push.mvp, mvp, sizeof(push.mvp));

	if (batches.empty()) {
		if (alpha_blend_pass || alpha_test_flag) {
			return;
		}
		push.use_texture = 0;
		push.alpha_test = 0;
		vkCmdPushConstants(
			cmd,
			pipeline_layout_,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			0,
			sizeof(push),
			&push);
		if (fallback_descriptor_set_ != VK_NULL_HANDLE) {
			vkCmdBindDescriptorSets(
				cmd,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipeline_layout_,
				0,
				1,
				&fallback_descriptor_set_,
				0,
				nullptr);
		}
		vkCmdDrawIndexed(cmd, index_count, 1, 0, 0, 0);
		return;
	}

	for (size_t i = 0; i < batches.size(); ++i) {
		const GpuDrawBatch &batch = batches[i];
		if (batch.index_count == 0) {
			continue;
		}
		if (batch.alpha_blend != alpha_blend_pass) {
			continue;
		}
		if (!alpha_blend_pass && batch.alpha_test != alpha_test_flag) {
			continue;
		}
		push.use_texture = batch.use_texture ? 1u : 0u;
		push.alpha_test = batch.alpha_test ? 1u : 0u;
		vkCmdPushConstants(
			cmd,
			pipeline_layout_,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			0,
			sizeof(push),
			&push);
		if (batch.descriptor_set != VK_NULL_HANDLE) {
			vkCmdBindDescriptorSets(
				cmd,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipeline_layout_,
				0,
				1,
				&batch.descriptor_set,
				0,
				nullptr);
		}
		vkCmdDrawIndexed(cmd, batch.index_count, 1, batch.index_offset, 0, 0);
	}
}

void W3DViewerRenderer::Record_Draw(VkCommandBuffer cmd)
{
	auto draw_pass = [&](VkPipeline pipeline, bool alpha_blend_pass, bool alpha_test_flag) {
		for (size_t s = 0; s < sub_meshes_.size(); ++s) {
			const GpuSubMesh &sub = sub_meshes_[s];
			if (sub.index_count == 0) {
				continue;
			}
			float mvp[16];
			memcpy(mvp, view_proj_, sizeof(mvp));
			float depth_bias = 0.0f;
			if (animated_scene_ && scene_ != nullptr && scene_->htree != nullptr) {
				const int bone = sub.bone_index;
				if (bone >= 0 && bone < scene_->htree->Num_Pivots()) {
					if (!scene_->htree->Get_Visibility(bone)) {
						continue;
					}
					float bone_mat[16];
					W3DViewer_Mat4_From_Matrix3D(
						bone_mat,
						&scene_->htree->Get_Transform(bone));
					Mat4_Mul(mvp, view_proj_, bone_mat);
				}
				if (sub.bone_index > 0) {
					depth_bias = -2.0f * (float)(sub.bone_index + 1);
				}
			}
			vkCmdSetDepthBias(cmd, depth_bias, 0.0f, -1.0f);
			Draw_Batches(
				cmd,
				pipeline,
				mvp,
				sub.vertex_buffer,
				sub.index_buffer,
				sub.index_count,
				sub.draw_batches,
				alpha_blend_pass,
				alpha_test_flag);
		}
	};

	draw_pass(pipeline_opaque_, false, false);
	draw_pass(pipeline_opaque_, false, true);
	draw_pass(pipeline_alpha_, true, false);
}

bool W3DViewerRenderer::End_Frame()
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

void W3DViewerRenderer::Recreate_Swapchain()
{
	VkContext &ctx = VkContext::Get();
	vkDeviceWaitIdle(ctx.Device());

	Destroy_Framebuffers();
	if (depth_view_ != VK_NULL_HANDLE) {
		vkDestroyImageView(ctx.Device(), depth_view_, nullptr);
		depth_view_ = VK_NULL_HANDLE;
	}
	if (depth_image_ != VK_NULL_HANDLE) {
		vkDestroyImage(ctx.Device(), depth_image_, nullptr);
		depth_image_ = VK_NULL_HANDLE;
	}
	if (depth_memory_ != VK_NULL_HANDLE) {
		vkFreeMemory(ctx.Device(), depth_memory_, nullptr);
		depth_memory_ = VK_NULL_HANDLE;
	}

	swapchain_.Recreate(extent_.width, extent_.height, vsync_);
	swapchain_format_ = swapchain_.Image_Format();

	Create_Depth_Buffer();
	Create_Framebuffers();
}

void W3DViewerRenderer::Resize(uint32_t width, uint32_t height)
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
