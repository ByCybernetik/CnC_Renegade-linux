#include "vk_texture.h"
#include "vk_check.h"
#include "vk_context.h"
#include "vk_format.h"
#include "vk_upload.h"
#include "vk_staging_pool.h"
#include "../ww3d2/ddsfile.h"

#include <cstring>
#include <vector>

namespace ww3d_vulkan {

static void Image_Memory_Barrier(
	VkCommandBuffer cmd,
	VkImage image,
	VkImageLayout old_layout,
	VkImageLayout new_layout,
	uint32_t mip_levels)
{
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = old_layout;
	barrier.newLayout = new_layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = mip_levels;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	VkAccessFlags src_access = 0;
	VkAccessFlags dst_access = VK_ACCESS_TRANSFER_WRITE_BIT;

	if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
		new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		src_access = VK_ACCESS_TRANSFER_WRITE_BIT;
		dst_access = VK_ACCESS_SHADER_READ_BIT;
	}

	vkCmdPipelineBarrier(
		cmd,
		src_stage,
		dst_stage,
		0,
		0,
		nullptr,
		0,
		nullptr,
		1,
		&barrier);
}

struct DdsUploadUser {
	VkBuffer staging = VK_NULL_HANDLE;
	VkImage image = VK_NULL_HANDLE;
	uint32_t mip_levels = 0;
	std::vector<VkBufferImageCopy> regions;
};

static void Record_Dds_Upload(VkCommandBuffer cmd, void *user_ptr)
{
	DdsUploadUser *user = static_cast<DdsUploadUser *>(user_ptr);
	Image_Memory_Barrier(
		cmd,
		user->image,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		user->mip_levels);
	vkCmdCopyBufferToImage(
		cmd,
		user->staging,
		user->image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		(uint32_t)user->regions.size(),
		user->regions.data());
	Image_Memory_Barrier(
		cmd,
		user->image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		user->mip_levels);
}

static size_t Dxt_Level_Size(uint32_t width, uint32_t height, WW3DFormat format)
{
	if (width < 4) {
		width = 4;
	}
	if (height < 4) {
		height = 4;
	}
	const size_t block_bytes = (format == WW3D_FORMAT_DXT1) ? 8u : 16u;
	return (size_t)(width / 4u) * (size_t)(height / 4u) * block_bytes;
}

bool VkTexture::Create_Empty(uint32_t width, uint32_t height, WW3DFormat format, bool clamp_uv)
{
	Destroy();

	VkFormat vk_format = WW3D_Format_To_Vk(format);
	if (vk_format == VK_FORMAT_UNDEFINED) {
		vk_format = VK_FORMAT_R8G8B8A8_UNORM;
	}

	VkContext &ctx = VkContext::Get();

	VkImageCreateInfo image_info = {};
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.extent.width = width;
	image_info.extent.height = height;
	image_info.extent.depth = 1;
	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;
	image_info.format = vk_format;
	image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;

	if (!VkAllocator::Create_Image(&image_info, VMA_MEMORY_USAGE_GPU_ONLY, &image_, &allocation_)) {
		image_ = VK_NULL_HANDLE;
		return false;
	}

	VkImageViewCreateInfo view_info = {};
	view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_info.image = image_;
	view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_info.format = vk_format;
	view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view_info.subresourceRange.levelCount = 1;
	view_info.subresourceRange.layerCount = 1;
	VK_CHECK(vkCreateImageView(ctx.Device(), &view_info, nullptr, &view_));

	const VkSamplerAddressMode address_mode =
		clamp_uv ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT;
	VkSamplerCreateInfo sampler_info = {};
	sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_info.magFilter = VK_FILTER_LINEAR;
	sampler_info.minFilter = VK_FILTER_LINEAR;
	sampler_info.addressModeU = address_mode;
	sampler_info.addressModeV = address_mode;
	sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	VK_CHECK(vkCreateSampler(ctx.Device(), &sampler_info, nullptr, &sampler_));
	width_ = width;
	height_ = height;
	mip_levels_ = 1;
	layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
	return true;
}

bool VkTexture::Upload_Rgb565_Region(
	const unsigned char *src,
	int src_pitch,
	uint32_t dst_x,
	uint32_t dst_y,
	uint32_t copy_w,
	uint32_t copy_h)
{
	if (image_ == VK_NULL_HANDLE || src == nullptr || src_pitch <= 0 || copy_w == 0 || copy_h == 0) {
		return false;
	}

	const VkDeviceSize row_bytes = (VkDeviceSize)copy_w * 2u;
	const VkDeviceSize staging_size = row_bytes * (VkDeviceSize)copy_h;

	VkStagingPool::Entry staging = VkStagingPool::Acquire(staging_size);
	if (staging.buffer == VK_NULL_HANDLE) {
		return false;
	}

	VkBufferImageCopy region = {};
	region.bufferOffset = 0;
	region.bufferRowLength = copy_w;
	region.bufferImageHeight = copy_h;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = {(int32_t)dst_x, (int32_t)dst_y, 0};
	region.imageExtent = {copy_w, copy_h, 1};

	unsigned char *dst = static_cast<unsigned char *>(staging.mapped);
	for (uint32_t y = 0; y < copy_h; ++y) {
		memcpy(dst + (size_t)y * (size_t)row_bytes, src + (size_t)y * (size_t)src_pitch, (size_t)row_bytes);
	}

	VkImageLayout old_layout = layout_;
	Submit_One_Time_Commands(
		[](VkCommandBuffer cmd, void *user_ptr) {
			struct UploadCtx {
				VkImage image;
				VkBuffer staging;
				VkImageLayout old_layout;
				VkBufferImageCopy region;
			};
			UploadCtx *upload = static_cast<UploadCtx *>(user_ptr);
			VkImageMemoryBarrier barrier = {};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = upload->image;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.layerCount = 1;

			VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			if (upload->old_layout == VK_IMAGE_LAYOUT_UNDEFINED) {
				barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.srcAccessMask = 0;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			} else {
				barrier.oldLayout = upload->old_layout;
				barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				src_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			}
			vkCmdPipelineBarrier(
				cmd,
				src_stage,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				0,
				0,
				nullptr,
				0,
				nullptr,
				1,
				&barrier);

			vkCmdCopyBufferToImage(
				cmd,
				upload->staging,
				upload->image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1,
				&upload->region);

			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			vkCmdPipelineBarrier(
				cmd,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0,
				0,
				nullptr,
				0,
				nullptr,
				1,
				&barrier);
		},
		&(struct { VkImage image; VkBuffer staging; VkImageLayout old_layout; VkBufferImageCopy region; }){ image_, staging.buffer, old_layout, region });

	VkStagingPool::Release(staging);
	layout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	return true;
}

bool VkTexture::Upload_Rgba8_Region(
	const unsigned char *src,
	int src_pitch,
	uint32_t dst_x,
	uint32_t dst_y,
	uint32_t copy_w,
	uint32_t copy_h)
{
	if (image_ == VK_NULL_HANDLE || src == nullptr || src_pitch <= 0 || copy_w == 0 || copy_h == 0) {
		return false;
	}

	const VkDeviceSize row_bytes = (VkDeviceSize)copy_w * 4u;
	const VkDeviceSize staging_size = row_bytes * (VkDeviceSize)copy_h;

	VkStagingPool::Entry staging = VkStagingPool::Acquire(staging_size);
	if (staging.buffer == VK_NULL_HANDLE) {
		return false;
	}

	VkBufferImageCopy region = {};
	region.bufferOffset = 0;
	region.bufferRowLength = copy_w;
	region.bufferImageHeight = copy_h;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = {(int32_t)dst_x, (int32_t)dst_y, 0};
	region.imageExtent = {copy_w, copy_h, 1};

	unsigned char *dst = static_cast<unsigned char *>(staging.mapped);
	for (uint32_t y = 0; y < copy_h; ++y) {
		memcpy(dst + (size_t)y * (size_t)row_bytes, src + (size_t)y * (size_t)src_pitch, (size_t)row_bytes);
	}

	VkImageLayout old_layout = layout_;
	Submit_One_Time_Commands(
		[](VkCommandBuffer cmd, void *user_ptr) {
			struct UploadCtx {
				VkImage image;
				VkBuffer staging;
				VkImageLayout old_layout;
				VkBufferImageCopy region;
			};
			UploadCtx *upload = static_cast<UploadCtx *>(user_ptr);
			VkImageMemoryBarrier barrier = {};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = upload->image;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.layerCount = 1;

			VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			if (upload->old_layout == VK_IMAGE_LAYOUT_UNDEFINED) {
				barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.srcAccessMask = 0;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			} else {
				barrier.oldLayout = upload->old_layout;
				barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				src_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			}
			vkCmdPipelineBarrier(
				cmd,
				src_stage,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				0,
				0,
				nullptr,
				0,
				nullptr,
				1,
				&barrier);

			vkCmdCopyBufferToImage(
				cmd,
				upload->staging,
				upload->image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1,
				&upload->region);

			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			vkCmdPipelineBarrier(
				cmd,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0,
				0,
				nullptr,
				0,
				nullptr,
				1,
				&barrier);
		},
		&(struct { VkImage image; VkBuffer staging; VkImageLayout old_layout; VkBufferImageCopy region; }){ image_, staging.buffer, old_layout, region });

	VkStagingPool::Release(staging);
	layout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	return true;
}

bool VkTexture::Create_Solid(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	Destroy();

	VkContext &ctx = VkContext::Get();
	VkFormat vk_format = VK_FORMAT_R8G8B8A8_UNORM;

	VkImageCreateInfo image_info = {};
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.extent.width = 1;
	image_info.extent.height = 1;
	image_info.extent.depth = 1;
	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;
	image_info.format = vk_format;
	image_info.tiling = VK_IMAGE_TILING_LINEAR;
	image_info.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
	image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;

	if (!VkAllocator::Create_Image(&image_info, VMA_MEMORY_USAGE_CPU_ONLY, &image_, &allocation_)) {
		image_ = VK_NULL_HANDLE;
		return false;
	}

	VkImageSubresource subresource = {};
	subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresource.mipLevel = 0;
	subresource.arrayLayer = 0;
	VkSubresourceLayout layout = {};
	vkGetImageSubresourceLayout(ctx.Device(), image_, &subresource, &layout);

	void *mapped = VkAllocator::Map(allocation_);
	uint8_t *pixel = static_cast<uint8_t *>(mapped);
	pixel[0] = r;
	pixel[1] = g;
	pixel[2] = b;
	pixel[3] = a;
	VkAllocator::Unmap(allocation_);

	VkImageViewCreateInfo view_info = {};
	view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_info.image = image_;
	view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_info.format = vk_format;
	view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view_info.subresourceRange.levelCount = 1;
	view_info.subresourceRange.layerCount = 1;
	VK_CHECK(vkCreateImageView(ctx.Device(), &view_info, nullptr, &view_));

	VkSamplerCreateInfo sampler_info = {};
	sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_info.magFilter = VK_FILTER_NEAREST;
	sampler_info.minFilter = VK_FILTER_NEAREST;
	sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	VK_CHECK(vkCreateSampler(ctx.Device(), &sampler_info, nullptr, &sampler_));
	layout_ = VK_IMAGE_LAYOUT_GENERAL;
	width_ = 1;
	height_ = 1;
	mip_levels_ = 1;
	return true;
}

bool VkTexture::Create_From_Compressed(
	WW3DFormat format,
	uint32_t width,
	uint32_t height,
	uint32_t mip_levels,
	const uint8_t *compressed_data,
	size_t compressed_size,
	VkSamplerAddressMode address_u,
	VkSamplerAddressMode address_v)
{
	Destroy();

	if (width == 0 || height == 0 || mip_levels == 0 || compressed_data == nullptr ||
		compressed_size == 0) {
		return false;
	}

	VkFormat vk_format = WW3D_Format_To_Vk(format);
	if (vk_format == VK_FORMAT_UNDEFINED) {
		return false;
	}

	size_t offset = 0;
	uint32_t mip_w = width;
	uint32_t mip_h = height;
	uint32_t actual_mip_levels = 0;
	std::vector<VkBufferImageCopy> regions;
	regions.reserve(mip_levels);

	for (uint32_t level = 0; level < mip_levels && offset < compressed_size; ++level) {
		uint32_t level_w = mip_w;
		uint32_t level_h = mip_h;
		if (level_w < 4) {
			level_w = 4;
		}
		if (level_h < 4) {
			level_h = 4;
		}
		const size_t level_bytes = Dxt_Level_Size(level_w, level_h, format);
		if (level_bytes == 0 || offset + level_bytes > compressed_size) {
			break;
		}

		VkBufferImageCopy region = {};
		region.bufferOffset = offset;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = level;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageExtent.width = level_w;
		region.imageExtent.height = level_h;
		region.imageExtent.depth = 1;
		regions.push_back(region);
		offset += level_bytes;
		++actual_mip_levels;

		if (mip_w > 4) {
			mip_w >>= 1;
		}
		if (mip_h > 4) {
			mip_h >>= 1;
		}
	}

	if (actual_mip_levels == 0 || offset != compressed_size) {
		return false;
	}

	const size_t staging_size = offset;
	mip_levels = actual_mip_levels;

	VkContext &ctx = VkContext::Get();

	VkImageCreateInfo image_info = {};
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.extent.width = width;
	image_info.extent.height = height;
	image_info.extent.depth = 1;
	image_info.mipLevels = mip_levels;
	image_info.arrayLayers = 1;
	image_info.format = vk_format;
	image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;

	if (!VkAllocator::Create_Image(&image_info, VMA_MEMORY_USAGE_GPU_ONLY, &image_, &allocation_)) {
		image_ = VK_NULL_HANDLE;
		return false;
	}

	VkStagingPool::Entry staging = VkStagingPool::Acquire(staging_size);
	if (staging.buffer == VK_NULL_HANDLE) {
		Destroy();
		return false;
	}
	memcpy(staging.mapped, compressed_data, staging_size);

	DdsUploadUser upload = {};
	upload.staging = staging.buffer;
	upload.image = image_;
	upload.mip_levels = mip_levels;
	upload.regions = std::move(regions);

	Submit_One_Time_Commands(Record_Dds_Upload, &upload);

	VkStagingPool::Release(staging);

	VkImageViewCreateInfo view_info = {};
	view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_info.image = image_;
	view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_info.format = vk_format;
	view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view_info.subresourceRange.levelCount = mip_levels;
	view_info.subresourceRange.layerCount = 1;
	VK_CHECK(vkCreateImageView(ctx.Device(), &view_info, nullptr, &view_));

	VkSamplerCreateInfo sampler_info = {};
	sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_info.magFilter = VK_FILTER_LINEAR;
	sampler_info.minFilter = VK_FILTER_LINEAR;
	sampler_info.mipmapMode = mip_levels > 1 ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
	sampler_info.addressModeU = address_u;
	sampler_info.addressModeV = address_v;
	sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.maxLod = (float)(mip_levels > 0 ? mip_levels - 1 : 0);
	VK_CHECK(vkCreateSampler(ctx.Device(), &sampler_info, nullptr, &sampler_));

	layout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	width_ = width;
	height_ = height;
	mip_levels_ = mip_levels;
	return true;
}

bool VkTexture::Create_From_Rgba8(
	const uint8_t *rgba,
	uint32_t width,
	uint32_t height,
	VkSamplerAddressMode address_u,
	VkSamplerAddressMode address_v)
{
	if (rgba == nullptr || width == 0 || height == 0) {
		return false;
	}

	std::vector<uint8_t> bgra((size_t)width * (size_t)height * 4u);
	for (size_t i = 0; i < (size_t)width * (size_t)height; ++i) {
		const uint8_t *src = rgba + i * 4u;
		uint8_t *dst = bgra.data() + i * 4u;
		dst[0] = src[2];
		dst[1] = src[1];
		dst[2] = src[0];
		dst[3] = src[3];
	}

	if (!Create_Empty(width, height, WW3D_FORMAT_A8R8G8B8, false)) {
		return false;
	}

	VkSamplerCreateInfo sampler_info = {};
	sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_info.magFilter = VK_FILTER_LINEAR;
	sampler_info.minFilter = VK_FILTER_LINEAR;
	sampler_info.addressModeU = address_u;
	sampler_info.addressModeV = address_v;
	sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	VkContext &ctx = VkContext::Get();
	if (sampler_ != VK_NULL_HANDLE) {
		vkDestroySampler(ctx.Device(), sampler_, nullptr);
	}
	VK_CHECK(vkCreateSampler(ctx.Device(), &sampler_info, nullptr, &sampler_));

	if (!Upload_Rgba8_Region(bgra.data(), (int)(width * 4u), 0, 0, width, height)) {
		Destroy();
		return false;
	}
	return true;
}

bool VkTexture::Create_From_DDS(
	const DDSFileClass &dds,
	VkSamplerAddressMode address_u,
	VkSamplerAddressMode address_v)
{
	const uint32_t width = dds.Get_Width(0);
	const uint32_t height = dds.Get_Height(0);
	const uint32_t mip_levels = dds.Get_Mip_Level_Count();
	if (width == 0 || height == 0 || mip_levels == 0) {
		return false;
	}

	size_t staging_size = 0;
	for (uint32_t level = 0; level < mip_levels; ++level) {
		staging_size += dds.Get_Level_Size(level);
	}
	if (staging_size == 0) {
		return false;
	}

	std::vector<uint8_t> compressed(staging_size);
	size_t offset = 0;
	for (uint32_t level = 0; level < mip_levels; ++level) {
		const size_t level_size = dds.Get_Level_Size(level);
		memcpy(compressed.data() + offset, dds.Get_Memory_Pointer(level), level_size);
		offset += level_size;
	}

	return Create_From_Compressed(
		dds.Get_Format(),
		width,
		height,
		mip_levels,
		compressed.data(),
		compressed.size(),
		address_u,
		address_v);
}

static uint32_t Next_Power_Of_Two(uint32_t value)
{
	if (value == 0) {
		return 1;
	}
	uint32_t pot = 1;
	while (pot < value && pot < 4096u) {
		pot <<= 1;
	}
	return pot;
}

bool VkTexture::Create_As_Render_Target(
	uint32_t width,
	uint32_t height,
	WW3DFormat format,
	VkRenderPass render_pass,
	VkFormat depth_format)
{
	Destroy();

	VkFormat vk_format = WW3D_Format_To_Vk(format);
	if (vk_format == VK_FORMAT_UNDEFINED) {
		vk_format = VK_FORMAT_B8G8R8A8_UNORM;
	}

	width = Next_Power_Of_Two(width);
	height = Next_Power_Of_Two(height);
	if (width > 4096u) {
		width = 4096u;
	}
	if (height > 4096u) {
		height = 4096u;
	}

	VkContext &ctx = VkContext::Get();

	VkImageCreateInfo image_info = {};
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.extent.width = width;
	image_info.extent.height = height;
	image_info.extent.depth = 1;
	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;
	image_info.format = vk_format;
	image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;

	if (!VkAllocator::Create_Image(&image_info, VMA_MEMORY_USAGE_GPU_ONLY, &image_, &allocation_)) {
		image_ = VK_NULL_HANDLE;
		return false;
	}

	VkImageViewCreateInfo view_info = {};
	view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_info.image = image_;
	view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_info.format = vk_format;
	view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view_info.subresourceRange.levelCount = 1;
	view_info.subresourceRange.layerCount = 1;
	VK_CHECK(vkCreateImageView(ctx.Device(), &view_info, nullptr, &view_));

	VkImageCreateInfo depth_info = {};
	depth_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	depth_info.imageType = VK_IMAGE_TYPE_2D;
	depth_info.extent.width = width;
	depth_info.extent.height = height;
	depth_info.extent.depth = 1;
	depth_info.mipLevels = 1;
	depth_info.arrayLayers = 1;
	depth_info.format = depth_format;
	depth_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	depth_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depth_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	depth_info.samples = VK_SAMPLE_COUNT_1_BIT;

	if (!VkAllocator::Create_Image(&depth_info, VMA_MEMORY_USAGE_GPU_ONLY, &depth_image_, &depth_allocation_)) {
		vkDestroyImageView(ctx.Device(), view_, nullptr);
		VkAllocator::Destroy_Image(image_, allocation_);
		image_ = VK_NULL_HANDLE;
		allocation_ = VK_NULL_HANDLE;
		view_ = VK_NULL_HANDLE;
		depth_image_ = VK_NULL_HANDLE;
		return false;
	}

	VkImageViewCreateInfo depth_view_info = {};
	depth_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	depth_view_info.image = depth_image_;
	depth_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	depth_view_info.format = depth_format;
	depth_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	depth_view_info.subresourceRange.levelCount = 1;
	depth_view_info.subresourceRange.layerCount = 1;
	VK_CHECK(vkCreateImageView(ctx.Device(), &depth_view_info, nullptr, &depth_view_));

	VkImageView attachments[] = {view_, depth_view_};
	VkFramebufferCreateInfo framebuffer_info = {};
	framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebuffer_info.renderPass = render_pass;
	framebuffer_info.attachmentCount = 2;
	framebuffer_info.pAttachments = attachments;
	framebuffer_info.width = width;
	framebuffer_info.height = height;
	framebuffer_info.layers = 1;
	VK_CHECK(vkCreateFramebuffer(ctx.Device(), &framebuffer_info, nullptr, &framebuffer_));

	VkSamplerCreateInfo sampler_info = {};
	sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_info.magFilter = VK_FILTER_LINEAR;
	sampler_info.minFilter = VK_FILTER_LINEAR;
	sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	VK_CHECK(vkCreateSampler(ctx.Device(), &sampler_info, nullptr, &sampler_));

	is_render_target_ = true;
	layout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	width_ = width;
	height_ = height;
	mip_levels_ = 1;
	render_extent_.width = width;
	render_extent_.height = height;
	return true;
}

void VkTexture::Destroy()
{
	VkContext &ctx = VkContext::Get();
	if (ctx.Device() == VK_NULL_HANDLE) {
		image_ = VK_NULL_HANDLE;
		allocation_ = VK_NULL_HANDLE;
		view_ = VK_NULL_HANDLE;
		sampler_ = VK_NULL_HANDLE;
		layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
		width_ = 0;
		height_ = 0;
		mip_levels_ = 1;
		is_render_target_ = false;
		framebuffer_ = VK_NULL_HANDLE;
		depth_view_ = VK_NULL_HANDLE;
		depth_image_ = VK_NULL_HANDLE;
		depth_allocation_ = VK_NULL_HANDLE;
		render_extent_ = {};
		return;
	}
	if (framebuffer_ != VK_NULL_HANDLE) {
		vkDestroyFramebuffer(ctx.Device(), framebuffer_, nullptr);
	}
	if (depth_view_ != VK_NULL_HANDLE) {
		vkDestroyImageView(ctx.Device(), depth_view_, nullptr);
	}
	if (depth_image_ != VK_NULL_HANDLE) {
		VkAllocator::Destroy_Image(depth_image_, depth_allocation_);
	}
	if (sampler_ != VK_NULL_HANDLE) {
		vkDestroySampler(ctx.Device(), sampler_, nullptr);
	}
	if (view_ != VK_NULL_HANDLE) {
		vkDestroyImageView(ctx.Device(), view_, nullptr);
	}
	if (image_ != VK_NULL_HANDLE) {
		VkAllocator::Destroy_Image(image_, allocation_);
	}
	sampler_ = VK_NULL_HANDLE;
	view_ = VK_NULL_HANDLE;
	image_ = VK_NULL_HANDLE;
	allocation_ = VK_NULL_HANDLE;
	framebuffer_ = VK_NULL_HANDLE;
	depth_view_ = VK_NULL_HANDLE;
	depth_image_ = VK_NULL_HANDLE;
	depth_allocation_ = VK_NULL_HANDLE;
	is_render_target_ = false;
	layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
	width_ = 0;
	height_ = 0;
	mip_levels_ = 1;
	render_extent_ = {};
}

} /* namespace ww3d_vulkan */
