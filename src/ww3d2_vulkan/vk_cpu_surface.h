#ifndef WW3D2_VULKAN_VK_CPU_SURFACE_H
#define WW3D2_VULKAN_VK_CPU_SURFACE_H

#if defined(RENEGADE_VULKAN)

#include "../ww3d2/ww3dformat.h"

class Vector2i;
class TextureClass;

namespace ww3d_vulkan {

class VkCpuSurface {
public:
	VkCpuSurface() = default;
	~VkCpuSurface();

	bool Create(unsigned width, unsigned height, WW3DFormat format);
	bool Load_From_File(const char *filename);
	bool Create_Missing();

	void Get_Description(unsigned *width, unsigned *height, WW3DFormat *format) const;
	void *Lock(int *pitch);
	void Unlock();
	void Clear();
	void Copy(
		unsigned dstx,
		unsigned dsty,
		unsigned srcx,
		unsigned srcy,
		unsigned width,
		unsigned height,
		const VkCpuSurface *source);
	bool Is_Transparent_Column(unsigned column) const;
	void Find_BB(Vector2i *min, Vector2i *max) const;

	const unsigned char *Pixels() const { return pixels_; }
	unsigned Pitch() const { return pitch_; }
	WW3DFormat Format() const { return format_; }
	unsigned Width() const { return width_; }
	unsigned Height() const { return height_; }

private:
	void Destroy();
	unsigned Pixel_Size() const;
	unsigned char *Pixel_At(unsigned x, unsigned y) const;
	unsigned char Read_Alpha(unsigned x, unsigned y) const;

	unsigned width_ = 0;
	unsigned height_ = 0;
	unsigned pitch_ = 0;
	WW3DFormat format_ = WW3D_FORMAT_UNKNOWN;
	unsigned char *pixels_ = nullptr;
	int lock_count_ = 0;
};

bool Apply_Texture_From_Cpu_Surface(TextureClass *texture, const VkCpuSurface *surface);

} /* namespace ww3d_vulkan */

#endif

#endif
