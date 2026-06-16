#include "vk_renderer.h"
#include "vk_check.h"
#include "vk_context.h"
#include "vk_shader.h"
#include "vk_dx8_bridge.h"
#include "vk_dx8_texture.h"
#include "../ww3d2/dx8wrapper.h"

#include <d3d8.h>
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstring>

namespace ww3d_vulkan {

namespace {

static void Apply_Depth_Bias_For_ZBias(VkCommandBuffer cmd, int zbias)
{
	const float constant = zbias > 0 ? (float)zbias * -2.0f : 0.0f;
	const float slope = zbias > 0 ? -1.0f : 0.0f;
	vkCmdSetDepthBias(cmd, constant, 0.0f, slope);
}

static void Apply_Dx8_Depth_Bias(VkCommandBuffer cmd)
{
	Apply_Depth_Bias_For_ZBias(cmd, DX8Wrapper::Get_Effective_ZBias());
}

static int Compare_Pipeline_Key(const MeshPipelineKey &a, const MeshPipelineKey &b)
{
	if (a.alpha_blend != b.alpha_blend) {
		return a.alpha_blend ? 1 : -1;
	}
	if (a.src_blend != b.src_blend) {
		return (int)a.src_blend - (int)b.src_blend;
	}
	if (a.dst_blend != b.dst_blend) {
		return (int)a.dst_blend - (int)b.dst_blend;
	}
	if (a.depth_write != b.depth_write) {
		return a.depth_write ? 1 : -1;
	}
	if (a.depth_test != b.depth_test) {
		return a.depth_test ? 1 : -1;
	}
	if (a.depth_compare != b.depth_compare) {
		return (int)a.depth_compare - (int)b.depth_compare;
	}
	if (a.two_sided != b.two_sided) {
		return a.two_sided ? 1 : -1;
	}
	if (a.cull_inverted != b.cull_inverted) {
		return a.cull_inverted ? 1 : -1;
	}
	if (a.alpha_test != b.alpha_test) {
		return a.alpha_test ? 1 : -1;
	}
	if (a.topology != b.topology) {
		return (int)a.topology - (int)b.topology;
	}
	if (a.fvf != b.fvf) {
		return (int)a.fvf - (int)b.fvf;
	}
	if (a.vertex_stride != b.vertex_stride) {
		return (int)a.vertex_stride - (int)b.vertex_stride;
	}
	return 0;
}

static VkTexture *Resolve_Draw_Texture(VkTexture *texture, VkTexture *fallback)
{
	if (texture == nullptr ||
		texture->View() == VK_NULL_HANDLE ||
		texture->Sampler() == VK_NULL_HANDLE) {
		return fallback;
	}
	return texture;
}
} /* namespace */

static_assert(offsetof(FrameUBO, tex_mat) == 672, "FrameUBO tex_mat std140 alignment");
static_assert(sizeof(FrameUBO) == 704, "FrameUBO size must match GLSL std140 layout");

static uint32_t Align_Ubo_Size(uint32_t size, uint32_t alignment)
{
	return (size + alignment - 1) & ~(alignment - 1);
}

bool VkRenderer::Init(SDL_Window *window, uint32_t width, uint32_t height, bool vsync)
{
	width_ = width;
	height_ = height;
	vsync_ = vsync;

	if (!VkContext::Get().Init(window, true)) {
		return false;
	}

	VkContext &ctx = VkContext::Get();
	push_descriptor_set_ = reinterpret_cast<PFN_vkCmdPushDescriptorSetKHR>(
		vkGetDeviceProcAddr(ctx.Device(), "vkCmdPushDescriptorSetKHR"));
	if (push_descriptor_set_ == nullptr) {
		fprintf(stderr, "VkRenderer: VK_KHR_push_descriptor not supported by device.\n");
		Shutdown();
		return false;
	}

	if (!Load_Shaders()) {
		Shutdown();
		return false;
	}
	if (!swapchain_.Create(width_, height_, vsync_)) {
		Shutdown();
		return false;
	}
	if (!render_pass_.Create(swapchain_.Image_Format())) {
		Shutdown();
		return false;
	}
	if (!render_pass_.Create_Offscreen(swapchain_.Image_Format())) {
		Shutdown();
		return false;
	}

	std::vector<VkImageView> views;
	for (uint32_t i = 0; i < swapchain_.Image_Count(); ++i) {
		views.push_back(swapchain_.Image_View(i));
	}
	if (!framebuffers_.Create(
			render_pass_.Handle(),
			views,
			render_pass_.Depth_Format(),
			swapchain_.Extent())) {
		Shutdown();
		return false;
	}
	if (!pipelines_.Create(render_pass_.Handle(), vert_spirv_, frag_spirv_)) {
		Shutdown();
		return false;
	}
	if (!pipelines_offscreen_.Create(render_pass_.Offscreen_Handle(), vert_spirv_, frag_spirv_)) {
		Shutdown();
		return false;
	}
	if (!Create_Frame_Resources()) {
		Shutdown();
		return false;
	}
	if (!Create_Sync_Objects()) {
		Shutdown();
		return false;
	}
	if (!Create_Default_Texture()) {
		Shutdown();
		return false;
	}

	bound_textures_[0] = &default_texture_;
	bound_textures_[1] = &default_texture_;

	memset(&frame_ubo_, 0, sizeof(frame_ubo_));
	frame_ubo_.tex_mat[0][0] = 1.0f;
	frame_ubo_.tex_mat[0][1] = 1.0f;
	frame_ubo_.tex_mat[1][0] = 1.0f;
	frame_ubo_.tex_mat[1][1] = 1.0f;
	bound_textures_[0] = &default_texture_;
	bound_textures_[1] = &default_texture_;
	textures_dirty_ = true;

	return true;
}

bool VkRenderer::Create_Default_Texture()
{
	return default_texture_.Create_Solid(255, 255, 255, 255);
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
	if (layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}
	/* Optimal-tiled uploads transition to SHADER_READ_ONLY on GPU; layout_ may be stale. */
	return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void VkRenderer::Bind_Texture(unsigned stage, VkTexture *texture)
{
	if (stage >= 2) {
		return;
	}
	if (texture == nullptr ||
		texture->View() == VK_NULL_HANDLE ||
		texture->Sampler() == VK_NULL_HANDLE) {
		texture = &default_texture_;
	}
	bound_textures_[stage] = texture;
	textures_dirty_ = true;
}

void VkRenderer::Flush_Push_Descriptors(VkCommandBuffer cmd, VkPipelineLayout layout, VkDeviceSize ubo_offset)
{
	const uint32_t aligned_ubo_size =
		Align_Ubo_Size(sizeof(FrameUBO), ubo_alignment_);
	VkDescriptorBufferInfo ubo_info = {};
	ubo_info.buffer = frame_ubo_ring_[current_frame_].Handle();
	ubo_info.offset = ubo_offset;
	ubo_info.range = aligned_ubo_size;

	VkWriteDescriptorSet writes[3] = {};
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstBinding = 0;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writes[0].descriptorCount = 1;
	writes[0].pBufferInfo = &ubo_info;

	VkDescriptorImageInfo image_infos[2] = {};
	unsigned write_count = 1;
	for (unsigned stage = 0; stage < 2; ++stage) {
		VkTexture *tex = bound_textures_[stage];
		if (tex == nullptr || tex->View() == VK_NULL_HANDLE || tex->Sampler() == VK_NULL_HANDLE) {
			tex = &default_texture_;
		}
		image_infos[stage].imageLayout = Sample_Descriptor_Layout(tex);
		image_infos[stage].imageView = tex->View();
		image_infos[stage].sampler = tex->Sampler();

		writes[write_count].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[write_count].dstBinding = stage + 1;
		writes[write_count].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[write_count].descriptorCount = 1;
		writes[write_count].pImageInfo = &image_infos[stage];
		++write_count;
	}

	if (push_descriptor_set_ != nullptr) {
		push_descriptor_set_(
			cmd,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			layout,
			0,   // set 0 — all push descriptors
			write_count,
			writes);
	}

	textures_dirty_ = false;
}

void VkRenderer::Flush_Pending_Draws()
{
	if (pending_draws_.empty()) {
		return;
	}

	std::sort(
		pending_draws_.begin(),
		pending_draws_.end(),
		[](const PendingDraw &a, const PendingDraw &b) {
			const int key_cmp = Compare_Pipeline_Key(a.key, b.key);
			if (key_cmp != 0) {
				return key_cmp < 0;
			}
			if (a.vertex_buffer != b.vertex_buffer) {
				return a.vertex_buffer < b.vertex_buffer;
			}
			if (a.index_buffer != b.index_buffer) {
				return a.index_buffer < b.index_buffer;
			}
			if (a.textures[0] != b.textures[0]) {
				return a.textures[0] < b.textures[0];
			}
			if (a.textures[1] != b.textures[1]) {
				return a.textures[1] < b.textures[1];
			}
			return a.depth_bias < b.depth_bias;
		});

	FrameSync &frame = frames_[current_frame_];
	VkCommandBuffer cmd = frame.command_buffer;
	VkExtent2D extent = offscreen_target_ != nullptr
		? offscreen_target_->Render_Extent()
		: swapchain_.Extent();
	Apply_Viewport(cmd, extent);

	VkPipelineCache &pipe_cache =
		offscreen_target_ != nullptr ? pipelines_offscreen_ : pipelines_;
	const VkPipelineLayout pipeline_layout = pipe_cache.Layout();
	const uint32_t aligned_ubo_size = Align_Ubo_Size(sizeof(FrameUBO), ubo_alignment_);

	VkPipeline bound_pipe = VK_NULL_HANDLE;
	VkBuffer bound_vb = VK_NULL_HANDLE;
	VkBuffer bound_ib = VK_NULL_HANDLE;
	int bound_zbias = 0;
	bool have_zbias = false;

	for (size_t i = 0; i < pending_draws_.size(); ++i) {
		const PendingDraw &d = pending_draws_[i];
		if (frame_ubo_draw_count_ >= kUboDrawsPerFrame) {
			break;
		}

		VkPipeline pipeline = pipe_cache.Get(d.key);
		if (pipeline == VK_NULL_HANDLE) {
			continue;
		}

		if (pipeline != bound_pipe) {
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
			bound_pipe = pipeline;
			bound_pipeline_ = pipeline;
		}

		if (!have_zbias || d.depth_bias != bound_zbias) {
			Apply_Depth_Bias_For_ZBias(cmd, d.depth_bias);
			bound_zbias = d.depth_bias;
			have_zbias = true;
		}

		VkTexture *tex0 = d.textures[0];
		VkTexture *tex1 = d.textures[1];

		const VkDeviceSize ubo_offset =
			(VkDeviceSize)frame_ubo_draw_count_ * aligned_ubo_size;
		frame_ubo_ring_[current_frame_].Upload(&d.ubo, sizeof(d.ubo), ubo_offset);
		++frame_ubo_draw_count_;

		frame_ubo_ = d.ubo;
		bound_textures_[0] = tex0;
		bound_textures_[1] = tex1;
		textures_dirty_ = true;
		Flush_Push_Descriptors(cmd, pipeline_layout, ubo_offset);

		if (d.vertex_buffer != bound_vb) {
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(cmd, 0, 1, &d.vertex_buffer, &offset);
			bound_vb = d.vertex_buffer;
		}
		if (d.index_buffer != bound_ib) {
			vkCmdBindIndexBuffer(cmd, d.index_buffer, 0, VK_INDEX_TYPE_UINT16);
			bound_ib = d.index_buffer;
		}

		vkCmdDrawIndexed(cmd, d.index_count, 1, d.first_index, d.vertex_offset, 0);
	}

	pending_draws_.clear();
}

void VkRenderer::Recreate_Swapchain_Resources()
{
	swapchain_.Recreate(width_, height_, vsync_);

	std::vector<VkImageView> views;
	for (uint32_t i = 0; i < swapchain_.Image_Count(); ++i) {
		views.push_back(swapchain_.Image_View(i));
	}
	framebuffers_.Destroy();
	framebuffers_.Create(
		render_pass_.Handle(),
		views,
		render_pass_.Depth_Format(),
		swapchain_.Extent());
	explicit_viewport_ = false;
}

void VkRenderer::Set_Render_Target(VkTexture *target)
{
	Flush_Pending_Draws();

	if (offscreen_target_ != nullptr && target == nullptr) {
		offscreen_target_ = nullptr;
		VkExtent2D extent = swapchain_.Extent();
		if (extent.width != width_ || extent.height != height_) {
			Recreate_Swapchain_Resources();
		}
		return;
	}
	offscreen_target_ = target;
}

void VkRenderer::Set_Viewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
	Flush_Pending_Draws();
	explicit_viewport_ = true;
	viewport_state_.x = (float)x;
	viewport_state_.y = (float)y;
	viewport_state_.width = (float)w;
	viewport_state_.height = (float)h;
	viewport_state_.minDepth = 0.0f;
	viewport_state_.maxDepth = 1.0f;
	scissor_state_.offset.x = (int32_t)x;
	scissor_state_.offset.y = (int32_t)y;
	scissor_state_.extent.width = w;
	scissor_state_.extent.height = h;
}

void VkRenderer::Shutdown()
{
	VkContext &ctx = VkContext::Get();
	if (ctx.Device() != VK_NULL_HANDLE) {
		vkDeviceWaitIdle(ctx.Device());
	}

	pending_draws_.clear();

	for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
		frame_ubo_ring_[i].Destroy();
		if (frames_[i].in_flight != VK_NULL_HANDLE) {
			vkDestroyFence(ctx.Device(), frames_[i].in_flight, nullptr);
		}
		if (frames_[i].image_available != VK_NULL_HANDLE) {
			vkDestroySemaphore(ctx.Device(), frames_[i].image_available, nullptr);
		}
		if (frames_[i].render_finished != VK_NULL_HANDLE) {
			vkDestroySemaphore(ctx.Device(), frames_[i].render_finished, nullptr);
		}
		frames_[i] = FrameSync{};
	}

	default_texture_.Destroy();
	pipelines_offscreen_.Destroy();
	pipelines_.Destroy();
	framebuffers_.Destroy();
	render_pass_.Destroy();
	swapchain_.Destroy();
	vert_spirv_.clear();
	frag_spirv_.clear();
	VkContext::Get().Shutdown();
	frame_active_ = false;
}

void VkRenderer::Resize(uint32_t width, uint32_t height)
{
	if (width == 0 || height == 0) {
		return;
	}
	if (width_ == width && height_ == height) {
		return;
	}
	width_ = width;
	height_ = height;
	if (offscreen_target_ != nullptr) {
		return;
	}
	if (frame_active_) {
		frame_active_ = false;
	}
	Recreate_Swapchain_Resources();
}

bool VkRenderer::Load_Shaders()
{
	return Load_Spirv_From_Search_Path("mesh_textured.vert.spv", &vert_spirv_) &&
		Load_Spirv_From_Search_Path("mesh_textured.frag.spv", &frag_spirv_);
}

bool VkRenderer::Create_Frame_Resources()
{
	ubo_alignment_ = (uint32_t)VkContext::Get().Device_Properties().limits.minUniformBufferOffsetAlignment;
	if (ubo_alignment_ < 1) {
		ubo_alignment_ = 1;
	}
	const uint32_t aligned_ubo_size = Align_Ubo_Size(sizeof(FrameUBO), ubo_alignment_);
	for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
		if (!frame_ubo_ring_[i].Create(
				(VkDeviceSize)aligned_ubo_size * kUboDrawsPerFrame,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VMA_MEMORY_USAGE_CPU_TO_GPU)) {
			return false;
		}
	}
	return true;
}

bool VkRenderer::Create_Sync_Objects()
{
	VkContext &ctx = VkContext::Get();

	for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
		VkSemaphoreCreateInfo semaphore_info = {};
		semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		VK_CHECK(vkCreateSemaphore(
			ctx.Device(), &semaphore_info, nullptr, &frames_[i].image_available));
		VK_CHECK(vkCreateSemaphore(
			ctx.Device(), &semaphore_info, nullptr, &frames_[i].render_finished));

		VkFenceCreateInfo fence_info = {};
		fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		VK_CHECK(vkCreateFence(ctx.Device(), &fence_info, nullptr, &frames_[i].in_flight));

		VkCommandBufferAllocateInfo cmd_alloc = {};
		cmd_alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmd_alloc.commandPool = ctx.Command_Pool();
		cmd_alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmd_alloc.commandBufferCount = 1;
		VK_CHECK(vkAllocateCommandBuffers(
			ctx.Device(), &cmd_alloc, &frames_[i].command_buffer));
	}
	return true;
}

void VkRenderer::Apply_Viewport(VkCommandBuffer cmd, VkExtent2D extent)
{
	VkViewport viewport = {};
	VkRect2D scissor = {};
	if (explicit_viewport_) {
		viewport.x = viewport_state_.x;
		viewport.width = viewport_state_.width;
		viewport.minDepth = viewport_state_.minDepth;
		viewport.maxDepth = viewport_state_.maxDepth;
		viewport.y = viewport_state_.y + viewport_state_.height;
		viewport.height = -viewport_state_.height;
		scissor = scissor_state_;
	} else {
		viewport.x = 0.0f;
		viewport.width = (float)extent.width;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		viewport.y = (float)extent.height;
		viewport.height = -(float)extent.height;
		scissor.offset = {0, 0};
		scissor.extent = extent;
	}
	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &scissor);
}

bool VkRenderer::Begin_Frame(float clear_r, float clear_g, float clear_b, float clear_a)
{
	VkContext &ctx = VkContext::Get();
	FrameSync &frame = frames_[current_frame_];

	vkWaitForFences(ctx.Device(), 1, &frame.in_flight, VK_TRUE, UINT64_MAX);

	vkResetFences(ctx.Device(), 1, &frame.in_flight);
	Reset_Dynamic_Vb_Frame_Slot(current_frame_);
	Reset_Dynamic_Ib_Frame_Slot(current_frame_);
	vkResetCommandBuffer(frame.command_buffer, 0);
	explicit_viewport_ = false;

	pending_draws_.clear();
	pending_draws_.reserve(512);
	bound_pipeline_ = VK_NULL_HANDLE;
	frame_ubo_draw_count_ = 0;

	bound_textures_[0] = &default_texture_;
	bound_textures_[1] = &default_texture_;
	textures_dirty_ = true;

	if (!offscreen_target_) {
		if (!swapchain_.Acquire_Next_Image(
				current_frame_, frame.image_available, &current_image_)) {
			Recreate_Swapchain_Resources();
			if (!swapchain_.Acquire_Next_Image(
					current_frame_, frame.image_available, &current_image_)) {
				return false;
			}
		}
	}

	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	VK_CHECK(vkBeginCommandBuffer(frame.command_buffer, &begin_info));

	VkClearValue clear_values[2] = {};
	clear_values[0].color.float32[0] = clear_r;
	clear_values[0].color.float32[1] = clear_g;
	clear_values[0].color.float32[2] = clear_b;
	clear_values[0].color.float32[3] = clear_a;
	clear_values[1].depthStencil.depth = 1.0f;
	clear_values[1].depthStencil.stencil = 0;

	VkExtent2D extent = swapchain_.Extent();
	VkRenderPassBeginInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_info.clearValueCount = 2;
	render_pass_info.pClearValues = clear_values;
	render_pass_info.renderArea.offset = {0, 0};

	if (offscreen_target_ != nullptr) {
		render_pass_info.renderPass = render_pass_.Offscreen_Handle();
		render_pass_info.framebuffer = offscreen_target_->Render_Framebuffer();
		extent = offscreen_target_->Render_Extent();
	} else {
		render_pass_info.renderPass = render_pass_.Handle();
		render_pass_info.framebuffer = framebuffers_.Framebuffer(current_image_);
	}
	render_pass_info.renderArea.extent = extent;

	vkCmdBeginRenderPass(
		frame.command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
	Apply_Viewport(frame.command_buffer, extent);

	frame_active_ = true;
	return true;
}

void VkRenderer::Clear_During_Frame(
	bool clear_color,
	bool clear_depth,
	float r,
	float g,
	float b,
	float a)
{
	if (!frame_active_) {
		return;
	}

	Flush_Pending_Draws();

	FrameSync &frame = frames_[current_frame_];
	VkCommandBuffer cmd = frame.command_buffer;

	VkClearAttachment attachments[2] = {};
	uint32_t attachment_count = 0;

	if (clear_color) {
		attachments[attachment_count].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		attachments[attachment_count].colorAttachment = 0;
		attachments[attachment_count].clearValue.color.float32[0] = r;
		attachments[attachment_count].clearValue.color.float32[1] = g;
		attachments[attachment_count].clearValue.color.float32[2] = b;
		attachments[attachment_count].clearValue.color.float32[3] = a;
		attachment_count++;
	}
	if (clear_depth) {
		attachments[attachment_count].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		attachments[attachment_count].clearValue.depthStencil.depth = 1.0f;
		attachments[attachment_count].clearValue.depthStencil.stencil = 0;
		attachment_count++;
	}
	if (attachment_count == 0) {
		return;
	}

	VkExtent2D extent = offscreen_target_ != nullptr
		? offscreen_target_->Render_Extent()
		: swapchain_.Extent();
	VkClearRect clear_rect = {};
	clear_rect.rect.offset = {0, 0};
	clear_rect.rect.extent = extent;
	clear_rect.baseArrayLayer = 0;
	clear_rect.layerCount = 1;

	vkCmdClearAttachments(cmd, attachment_count, attachments, 1, &clear_rect);
}

bool VkRenderer::End_Frame(bool present)
{
	if (!frame_active_) {
		return false;
	}

	Flush_Pending_Draws();

	VkContext &ctx = VkContext::Get();
	FrameSync &frame = frames_[current_frame_];

	vkCmdEndRenderPass(frame.command_buffer);
	VK_CHECK(vkEndCommandBuffer(frame.command_buffer));

	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &frame.command_buffer;

	if (offscreen_target_ != nullptr) {
		VK_CHECK(vkQueueSubmit(ctx.Graphics_Queue(), 1, &submit_info, frame.in_flight));
		VK_CHECK(vkWaitForFences(ctx.Device(), 1, &frame.in_flight, VK_TRUE, UINT64_MAX));
	} else if (present) {
		VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &frame.image_available;
		submit_info.pWaitDstStageMask = &wait_stage;
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &frame.render_finished;

		VK_CHECK(vkQueueSubmit(ctx.Graphics_Queue(), 1, &submit_info, frame.in_flight));

		if (!swapchain_.Present(current_frame_, frame.render_finished, current_image_)) {
			Resize(width_, height_);
		}

		current_frame_ = (current_frame_ + 1) % kMaxFramesInFlight;
	} else {
		VK_CHECK(vkQueueSubmit(ctx.Graphics_Queue(), 1, &submit_info, frame.in_flight));
		VK_CHECK(vkWaitForFences(ctx.Device(), 1, &frame.in_flight, VK_TRUE, UINT64_MAX));
	}

	frame_active_ = false;

	return true;
}

void VkRenderer::Set_View_Projection(const float matrix[16])
{
	memcpy(frame_ubo_.view_proj, matrix, sizeof(frame_ubo_.view_proj));
}

void VkRenderer::Set_World_Matrix(const float matrix[16])
{
	memcpy(frame_ubo_.world, matrix, sizeof(frame_ubo_.world));
}

void VkRenderer::Set_View_Matrix(const float matrix[16])
{
	memcpy(frame_ubo_.view, matrix, sizeof(frame_ubo_.view));
}

void VkRenderer::Set_Lighting_State(const FrameUBO &state)
{
	memcpy(frame_ubo_.view, state.view, sizeof(frame_ubo_.view));
	memcpy(frame_ubo_.material_ambient, state.material_ambient, sizeof(state.material_ambient));
	memcpy(frame_ubo_.material_diffuse, state.material_diffuse, sizeof(state.material_diffuse));
	memcpy(frame_ubo_.material_emissive, state.material_emissive, sizeof(state.material_emissive));
	memcpy(frame_ubo_.scene_ambient, state.scene_ambient, sizeof(state.scene_ambient));
	memcpy(frame_ubo_.fog_color, state.fog_color, sizeof(state.fog_color));
	memcpy(frame_ubo_.light_dir_or_pos, state.light_dir_or_pos, sizeof(state.light_dir_or_pos));
	memcpy(frame_ubo_.light_diffuse, state.light_diffuse, sizeof(state.light_diffuse));
	memcpy(frame_ubo_.light_params, state.light_params, sizeof(state.light_params));
	memcpy(frame_ubo_.material_specular, state.material_specular, sizeof(state.material_specular));
	frame_ubo_.fog_start = state.fog_start;
	frame_ubo_.fog_end = state.fog_end;
	frame_ubo_.fog_mode = state.fog_mode;
	frame_ubo_.flags = state.flags;
	frame_ubo_.tex_stage0_mode = state.tex_stage0_mode;
	frame_ubo_.tex_stage1_color_mode = state.tex_stage1_color_mode;
	frame_ubo_.tex_stage1_alpha_mode = state.tex_stage1_alpha_mode;
	frame_ubo_.material_shininess = state.material_shininess;
	frame_ubo_.specular_enable = state.specular_enable;
	memcpy(
		&frame_ubo_.bump_mat,
		&state.bump_mat,
		sizeof(FrameUBO) - offsetof(FrameUBO, bump_mat));
}

void VkRenderer::Draw_Indexed(
	VkBuffer vertex_buffer,
	VkBuffer index_buffer,
	uint32_t index_count,
	uint32_t first_index,
	uint32_t vertex_offset,
	const MeshPipelineKey &key)
{
	if (!frame_active_) {
		return;
	}

	if (index_count == 0) {
		return;
	}

	VkPipelineCache &pipe_cache =
		offscreen_target_ != nullptr ? pipelines_offscreen_ : pipelines_;

	VkPipeline pipeline = pipe_cache.Get(key);
	if (pipeline == VK_NULL_HANDLE) {
		return;
	}

	if (frame_ubo_draw_count_ + pending_draws_.size() >= kUboDrawsPerFrame) {
		return;
	}

	/* Screen-space 2D (XYRHW): flush queued work, then draw immediately. */
	const bool immediate_2d =
		(key.fvf & D3DFVF_POSITION_MASK) == D3DFVF_XYZRHW;
	if (immediate_2d) {
		FrameSync &frame = frames_[current_frame_];
		VkCommandBuffer cmd = frame.command_buffer;
		const uint32_t aligned_ubo_size = Align_Ubo_Size(sizeof(FrameUBO), ubo_alignment_);

		Flush_Pending_Draws();

		VkExtent2D extent = offscreen_target_ != nullptr
			? offscreen_target_->Render_Extent()
			: swapchain_.Extent();
		Apply_Viewport(cmd, extent);

		if (bound_pipeline_ != pipeline) {
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
			bound_pipeline_ = pipeline;
		}

		const VkDeviceSize ubo_offset = (VkDeviceSize)frame_ubo_draw_count_ * aligned_ubo_size;
		frame_ubo_ring_[current_frame_].Upload(&frame_ubo_, sizeof(frame_ubo_), ubo_offset);
		++frame_ubo_draw_count_;

		Flush_Push_Descriptors(cmd, pipe_cache.Layout(), ubo_offset);
		Apply_Dx8_Depth_Bias(cmd);

		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(cmd, 0, 1, &vertex_buffer, &offset);
		vkCmdBindIndexBuffer(cmd, index_buffer, 0, VK_INDEX_TYPE_UINT16);
		vkCmdDrawIndexed(cmd, index_count, 1, first_index, vertex_offset, 0);
		return;
	}

	PendingDraw draw;
	draw.key = key;
	draw.vertex_buffer = vertex_buffer;
	draw.index_buffer = index_buffer;
	draw.index_count = index_count;
	draw.first_index = first_index;
	draw.vertex_offset = vertex_offset;
	draw.depth_bias = DX8Wrapper::Get_Effective_ZBias();
	draw.ubo = frame_ubo_;
	draw.textures[0] = Resolve_Draw_Texture(bound_textures_[0], &default_texture_);
	draw.textures[1] = Resolve_Draw_Texture(bound_textures_[1], &default_texture_);

	pending_draws_.push_back(draw);
}

} /* namespace ww3d_vulkan */
