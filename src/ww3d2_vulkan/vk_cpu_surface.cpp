#include "vk_cpu_surface.h"

#if defined(RENEGADE_VULKAN)

#include "stb_texture.h"
#include "vk_check.h"
#include "vk_context.h"
#include "vk_texture.h"
#include "vk_upload.h"
#include "bitmaphandler.h"
#include "bound.h"
#include "ffactory.h"
#include "formconv.h"
#include "texture.h"
#include "vector2i.h"
#include "ww3dformat.h"

#include <cstring>
#include <cstdio>
#include <new>
#include <vector>
#if defined(RENEGADE_LINUX)
#include <time.h>
#endif

namespace ww3d_vulkan {

static uint32_t Find_Memory_Type(uint32_t type_bits, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties mem_props = {};
	vkGetPhysicalDeviceMemoryProperties(VkContext::Get().Physical_Device(), &mem_props);
	for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
		if ((type_bits & (1u << i)) &&
			(mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}
	return 0;
}

VkCpuSurface::~VkCpuSurface()
{
	Destroy();
}

void VkCpuSurface::Destroy()
{
	delete[] pixels_;
	pixels_ = nullptr;
	width_ = 0;
	height_ = 0;
	pitch_ = 0;
	format_ = WW3D_FORMAT_UNKNOWN;
	lock_count_ = 0;
}

unsigned VkCpuSurface::Pixel_Size() const
{
	return Get_Bytes_Per_Pixel(format_);
}

bool VkCpuSurface::Create(unsigned width, unsigned height, WW3DFormat format)
{
	Destroy();
	if (width == 0 || height == 0) {
		return false;
	}
	const unsigned bpp = Get_Bytes_Per_Pixel(format);
	if (bpp == 0) {
		return false;
	}
	width_ = width;
	height_ = height;
	format_ = format;
	pitch_ = width * bpp;
	pixels_ = new (std::nothrow) unsigned char[pitch_ * height];
	if (pixels_ == nullptr) {
		Destroy();
		return false;
	}
	memset(pixels_, 0, pitch_ * height);
	return true;
}

bool VkCpuSurface::Load_From_File(const char *filename)
{
	if (filename == nullptr || filename[0] == '\0') {
		return Create_Missing();
	}

	StbLoadedTexture loaded;
	if (!Stb_Load_Texture(filename, 0, false, false, true, &loaded) || loaded.compressed) {
		return Create_Missing();
	}

	if (!Create(loaded.width, loaded.height, loaded.format)) {
		return false;
	}

	memcpy(pixels_, loaded.pixels.data(), loaded.pixels.size());
	return true;
}

bool VkCpuSurface::Create_Missing()
{
	if (!Create(128, 128, WW3D_FORMAT_A8R8G8B8)) {
		return false;
	}
	for (unsigned y = 0; y < height_; ++y) {
		for (unsigned x = 0; x < width_; ++x) {
			unsigned char *px = Pixel_At(x, y);
			const bool checker = ((x / 16) ^ (y / 16)) & 1;
			px[0] = checker ? 0 : 255;
			px[1] = 0;
			px[2] = checker ? 0 : 255;
			px[3] = 255;
		}
	}
	return true;
}

void VkCpuSurface::Get_Description(unsigned *width, unsigned *height, WW3DFormat *format) const
{
	if (width != nullptr) {
		*width = width_;
	}
	if (height != nullptr) {
		*height = height_;
	}
	if (format != nullptr) {
		*format = format_;
	}
}

void *VkCpuSurface::Lock(int *pitch)
{
	if (pitch != nullptr) {
		*pitch = (int)pitch_;
	}
	++lock_count_;
	return pixels_;
}

void VkCpuSurface::Unlock()
{
	if (lock_count_ > 0) {
		--lock_count_;
	}
}

void VkCpuSurface::Clear()
{
	if (pixels_ != nullptr) {
		memset(pixels_, 0, pitch_ * height_);
	}
}

unsigned char *VkCpuSurface::Pixel_At(unsigned x, unsigned y) const
{
	return pixels_ + y * pitch_ + x * Pixel_Size();
}

unsigned char VkCpuSurface::Read_Alpha(unsigned x, unsigned y) const
{
	if (!Has_Alpha(format_)) {
		return 255;
	}
	const int alphabits = Alpha_Bits(format_);
	int mask = 0;
	switch (alphabits) {
	case 1:
		mask = 1;
		break;
	case 4:
		mask = 0xf;
		break;
	case 8:
		mask = 0xff;
		break;
	default:
		return 255;
	}
	const unsigned char *alpha = Pixel_At(x, y);
	const unsigned size = Pixel_Size();
	unsigned char myalpha = alpha[size - 1];
	myalpha = static_cast<unsigned char>((myalpha >> (8 - alphabits)) & mask);
	return myalpha;
}

void VkCpuSurface::Copy(
	unsigned dstx,
	unsigned dsty,
	unsigned srcx,
	unsigned srcy,
	unsigned copy_width,
	unsigned copy_height,
	const VkCpuSurface *source)
{
	if (source == nullptr || pixels_ == nullptr || source->pixels_ == nullptr) {
		return;
	}
	if (copy_width == 0 || copy_height == 0) {
		return;
	}

	unsigned src_right = srcx + copy_width;
	unsigned src_bottom = srcy + copy_height;
	if (src_right > source->width_) {
		src_right = source->width_;
	}
	if (src_bottom > source->height_) {
		src_bottom = source->height_;
	}
	copy_width = src_right - srcx;
	copy_height = src_bottom - srcy;
	if (copy_width == 0 || copy_height == 0) {
		return;
	}

	unsigned dst_right = dstx + copy_width;
	unsigned dst_bottom = dsty + copy_height;
	if (dst_right > width_) {
		dst_right = width_;
	}
	if (dst_bottom > height_) {
		dst_bottom = height_;
	}
	const unsigned actual_width = dst_right - dstx;
	const unsigned actual_height = dst_bottom - dsty;
	if (actual_width == 0 || actual_height == 0) {
		return;
	}

	if (format_ == source->format_) {
		const unsigned size = Pixel_Size();
		for (unsigned row = 0; row < actual_height; ++row) {
			memcpy(
				Pixel_At(dstx, dsty + row),
				source->Pixel_At(srcx, srcy + row),
				size * actual_width);
		}
		return;
	}

	BitmapHandlerClass::Copy_Image(
		Pixel_At(dstx, dsty),
		actual_width,
		actual_height,
		pitch_,
		format_,
		source->Pixel_At(srcx, srcy),
		actual_width,
		actual_height,
		source->pitch_,
		source->format_,
		nullptr,
		0,
		false);
}

bool VkCpuSurface::Is_Transparent_Column(unsigned column) const
{
	if (column >= width_ || !Has_Alpha(format_)) {
		return true;
	}
	for (unsigned y = 0; y < height_; ++y) {
		if (Read_Alpha(column, y) != 0) {
			return false;
		}
	}
	return true;
}

void VkCpuSurface::Find_BB(Vector2i *min, Vector2i *max) const
{
	if (min == nullptr || max == nullptr || !Has_Alpha(format_)) {
		return;
	}
	Vector2i realmin = *max;
	Vector2i realmax = *min;
	for (int y = min->J; y < max->J; ++y) {
		for (int x = min->I; x < max->I; ++x) {
			if (Read_Alpha((unsigned)x, (unsigned)y) != 0) {
				realmin.I = MIN(realmin.I, x);
				realmax.I = MAX(realmax.I, x);
				realmin.J = MIN(realmin.J, y);
				realmax.J = MAX(realmax.J, y);
			}
		}
	}
	*max = realmax;
	*min = realmin;
}

static bool Upload_Rgba8(VkTexture *texture, const unsigned char *rgba, unsigned width, unsigned height)
{
	if (texture == nullptr || rgba == nullptr || width == 0 || height == 0) {
		return false;
	}
	if (!texture->Create_Empty(width, height, WW3D_FORMAT_A8R8G8B8)) {
		return false;
	}

	struct UploadUser {
		VkBuffer staging = VK_NULL_HANDLE;
		VkDeviceMemory staging_memory = VK_NULL_HANDLE;
		VkImage image = VK_NULL_HANDLE;
		VkBufferImageCopy region = {};
	};

	UploadUser user;
	user.image = texture->Image();
	user.region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	user.region.imageSubresource.mipLevel = 0;
	user.region.imageSubresource.baseArrayLayer = 0;
	user.region.imageSubresource.layerCount = 1;
	user.region.imageExtent.width = width;
	user.region.imageExtent.height = height;
	user.region.imageExtent.depth = 1;

	const VkDeviceSize image_size = width * height * 4;
	VkContext &ctx = VkContext::Get();

	VkBufferCreateInfo buffer_info = {};
	buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.size = image_size;
	buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	VK_CHECK(vkCreateBuffer(ctx.Device(), &buffer_info, nullptr, &user.staging));

	VkMemoryRequirements requirements = {};
	vkGetBufferMemoryRequirements(ctx.Device(), user.staging, &requirements);
	VkMemoryAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = requirements.size;
	alloc_info.memoryTypeIndex = Find_Memory_Type(
		requirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	VK_CHECK(vkAllocateMemory(ctx.Device(), &alloc_info, nullptr, &user.staging_memory));
	VK_CHECK(vkBindBufferMemory(ctx.Device(), user.staging, user.staging_memory, 0));

	void *mapped = nullptr;
	VK_CHECK(vkMapMemory(ctx.Device(), user.staging_memory, 0, image_size, 0, &mapped));
	memcpy(mapped, rgba, (size_t)image_size);
	vkUnmapMemory(ctx.Device(), user.staging_memory);

	Submit_One_Time_Commands(
		[](VkCommandBuffer cmd, void *user_ptr) {
			UploadUser *upload = static_cast<UploadUser *>(user_ptr);
			VkImageMemoryBarrier barrier = {};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = upload->image;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(
				cmd,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
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
		&user);

	vkDestroyBuffer(ctx.Device(), user.staging, nullptr);
	vkFreeMemory(ctx.Device(), user.staging_memory, nullptr);
	texture->Set_Layout_Shader_Read_Only();
	return true;
}

bool Apply_Texture_From_Cpu_Surface(TextureClass *texture, const VkCpuSurface *surface)
{
	if (texture == nullptr || surface == nullptr || surface->Pixels() == nullptr) {
		return false;
	}

	std::vector<unsigned char> rgba(surface->Width() * surface->Height() * 4);
	BitmapHandlerClass::Copy_Image(
		rgba.data(),
		surface->Width(),
		surface->Height(),
		surface->Width() * 4,
		WW3D_FORMAT_A8R8G8B8,
		surface->Pixels(),
		surface->Width(),
		surface->Height(),
		surface->Pitch(),
		surface->Format(),
		nullptr,
		0,
		false);

	VkTexture *vk_tex = new (std::nothrow) VkTexture();
	if (vk_tex == nullptr) {
		return false;
	}
	if (!Upload_Rgba8(vk_tex, rgba.data(), surface->Width(), surface->Height())) {
		delete vk_tex;
		return false;
	}

	texture->Set_Vulkan_Texture(vk_tex);
	texture->Set_Dimensions((int)surface->Width(), (int)surface->Height());
	texture->Mark_Vulkan_Initialized();
	return true;
}

} /* namespace ww3d_vulkan */

#endif
