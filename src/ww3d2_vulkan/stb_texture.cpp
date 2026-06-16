#include "stb_texture.h"

#if defined(RENEGADE_VULKAN)

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include "stb_image.h"

#define STB_DXT_IMPLEMENTATION
#include "stb_dxt.h"

#include "../ww3d2/ddsfile.h"
#include "../ww3d2/formconv.h"
#include "../wwlib/bufffile.h"
#include "../wwlib/ffactory.h"

#include <cstring>
#include <vector>

namespace ww3d_vulkan {

namespace {

static const unsigned LEGACY_DDSURFACEDESC2_BYTES = 124u;

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

static void Validate_Texture_Size(uint32_t &width, uint32_t &height)
{
	width = Next_Power_Of_Two(width);
	height = Next_Power_Of_Two(height);
	if (width > 4096u) {
		width = 4096u;
	}
	if (height > 4096u) {
		height = 4096u;
	}
	if (width > height * 8u) {
		height = width / 8u;
	} else if (height > width * 8u) {
		width = height / 8u;
	}
}

static void Set_Extension(char *path, size_t cap, char e0, char e1, char e2)
{
	const size_t len = strlen(path);
	const char *dot = strrchr(path, '.');
	if (dot != nullptr && dot != path) {
		if (len >= 4 && cap > len) {
			path[len - 3] = e0;
			path[len - 2] = e1;
			path[len - 1] = e2;
		}
		return;
	}
	if (len + 4 < cap) {
		path[len] = '.';
		path[len + 1] = e0;
		path[len + 2] = e1;
		path[len + 3] = e2;
		path[len + 4] = '\0';
	}
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

static bool Is_Dxt_Format(WW3DFormat format)
{
	switch (format) {
	case WW3D_FORMAT_DXT1:
	case WW3D_FORMAT_DXT2:
	case WW3D_FORMAT_DXT3:
	case WW3D_FORMAT_DXT4:
	case WW3D_FORMAT_DXT5:
		return true;
	default:
		return false;
	}
}

static WW3DFormat Vk_Normalize_Dxt_Format(WW3DFormat format)
{
	switch (format) {
	case WW3D_FORMAT_DXT2:
	case WW3D_FORMAT_DXT3:
	case WW3D_FORMAT_DXT4:
	case WW3D_FORMAT_DXT5:
		return WW3D_FORMAT_DXT5;
	case WW3D_FORMAT_DXT1:
		return WW3D_FORMAT_DXT1;
	default:
		return format;
	}
}

static bool Read_File_Bytes(const char *path, std::vector<uint8_t> &out)
{
	file_auto_ptr file(_TheFileFactory, path);
	if (!file->Is_Available()) {
		return false;
	}
	file->Open();
	const int size = file->Size();
	if (size <= 0) {
		file->Close();
		return false;
	}
	out.resize((size_t)size);
	const int read_size = file->Read(out.data(), size);
	file->Close();
	return read_size == size;
}

static bool Load_Dds_From_Bytes(
	const std::vector<uint8_t> &file_bytes,
	unsigned reduction,
	StbLoadedTexture *out)
{
	if (file_bytes.size() < 4u + LEGACY_DDSURFACEDESC2_BYTES) {
		return false;
	}
	if (file_bytes[0] != 'D' || file_bytes[1] != 'D' || file_bytes[2] != 'S' ||
		file_bytes[3] != ' ') {
		return false;
	}

	LegacyDDSURFACEDESC2 surface_desc = {};
	memcpy(&surface_desc, file_bytes.data() + 4, LEGACY_DDSURFACEDESC2_BYTES);
	if (surface_desc.Size != LEGACY_DDSURFACEDESC2_BYTES) {
		return false;
	}

	const WW3DFormat format =
		D3DFormat_To_WW3DFormat((D3DFORMAT)surface_desc.PixelFormat.FourCC);
	if (!Is_Dxt_Format(format)) {
		return false;
	}

	unsigned mip_levels = surface_desc.MipMapCount;
	if (mip_levels == 0) {
		mip_levels = 1;
	}
	unsigned reduction_factor = reduction;
	if (mip_levels > reduction_factor) {
		mip_levels -= reduction_factor;
	} else {
		mip_levels = 1;
		reduction_factor = reduction_factor - mip_levels;
	}
	if (mip_levels > 2) {
		mip_levels -= 2;
	} else {
		mip_levels = 1;
	}

	const uint32_t full_width = surface_desc.Width;
	const uint32_t full_height = surface_desc.Height;
	const uint32_t width = full_width >> reduction_factor;
	const uint32_t height = full_height >> reduction_factor;

	unsigned level_size =
		(unsigned)Dxt_Level_Size(full_width, full_height, format);
	size_t data_offset = 4u + LEGACY_DDSURFACEDESC2_BYTES;
	for (unsigned i = 0; i < reduction_factor; ++i) {
		if (data_offset + level_size > file_bytes.size()) {
			return false;
		}
		data_offset += level_size;
		if (level_size > 16) {
			level_size /= 4;
		}
	}

	std::vector<uint8_t> compressed;
	compressed.reserve((size_t)mip_levels * 256u);
	uint32_t mip_w = width;
	uint32_t mip_h = height;
	for (unsigned level = 0; level < mip_levels; ++level) {
		uint32_t level_w = mip_w;
		uint32_t level_h = mip_h;
		if (level_w < 4) {
			level_w = 4;
		}
		if (level_h < 4) {
			level_h = 4;
		}
		const size_t level_bytes = Dxt_Level_Size(level_w, level_h, format);
		if (data_offset + level_bytes > file_bytes.size()) {
			return false;
		}
		const size_t old_size = compressed.size();
		compressed.resize(old_size + level_bytes);
		memcpy(compressed.data() + old_size, file_bytes.data() + data_offset, level_bytes);
		data_offset += level_bytes;
		if (mip_w > 4) {
			mip_w >>= 1;
		}
		if (mip_h > 4) {
			mip_h >>= 1;
		}
	}

	out->format = Vk_Normalize_Dxt_Format(format);
	out->width = width;
	out->height = height;
	out->mip_levels = mip_levels;
	out->compressed = true;
	out->pixels.swap(compressed);
	return true;
}

static bool Image_Has_Alpha(const uint8_t *rgba, int width, int height)
{
	const int pixel_count = width * height;
	for (int i = 0; i < pixel_count; ++i) {
		if (rgba[i * 4 + 3] != 255) {
			return true;
		}
	}
	return false;
}

static void Fill_Dxt_Block(
	uint8_t block[64],
	const uint8_t *rgba,
	int rgba_width,
	int rgba_height,
	int block_x,
	int block_y)
{
	for (int y = 0; y < 4; ++y) {
		for (int x = 0; x < 4; ++x) {
			int sx = block_x + x;
			int sy = block_y + y;
			if (sx >= rgba_width) {
				sx = rgba_width - 1;
			}
			if (sy >= rgba_height) {
				sy = rgba_height - 1;
			}
			if (rgba_width <= 0 || rgba_height <= 0) {
				sx = sy = 0;
			}
			const uint8_t *src = rgba + (sy * rgba_width + sx) * 4;
			uint8_t *dst = block + (y * 4 + x) * 4;
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
			dst[3] = src[3];
		}
	}
}

static bool Compress_Rgba_To_Dxt(
	const uint8_t *rgba,
	uint32_t width,
	uint32_t height,
	bool allow_dxt1,
	StbLoadedTexture *out)
{
	if (rgba == nullptr || width == 0 || height == 0) {
		return false;
	}

	const bool has_alpha = Image_Has_Alpha(rgba, (int)width, (int)height);
	const bool use_dxt5 = has_alpha || !allow_dxt1;
	const WW3DFormat format = use_dxt5 ? WW3D_FORMAT_DXT5 : WW3D_FORMAT_DXT1;
	const size_t level_bytes = Dxt_Level_Size(width, height, format);
	std::vector<uint8_t> compressed(level_bytes, 0);

	uint8_t block[64];
	uint8_t *dest = compressed.data();
	for (uint32_t by = 0; by < height; by += 4) {
		for (uint32_t bx = 0; bx < width; bx += 4) {
			Fill_Dxt_Block(block, rgba, (int)width, (int)height, (int)bx, (int)by);
			stb_compress_dxt_block(
				dest,
				block,
				use_dxt5 ? 1 : 0,
				STB_DXT_NORMAL);
			dest += use_dxt5 ? 16 : 8;
		}
	}

	out->format = format;
	out->width = width;
	out->height = height;
	out->mip_levels = 1;
	out->compressed = true;
	out->pixels.swap(compressed);
	return true;
}

static bool Load_Image_From_Bytes(
	const std::vector<uint8_t> &file_bytes,
	unsigned reduction,
	bool allow_compression,
	bool flip_vertical,
	StbLoadedTexture *out)
{
	stbi_set_flip_vertically_on_load(flip_vertical ? 1 : 0);
	int width = 0;
	int height = 0;
	int channels = 0;
	uint8_t *loaded = stbi_load_from_memory(
		file_bytes.data(),
		(int)file_bytes.size(),
		&width,
		&height,
		&channels,
		4);
	if (loaded == nullptr || width <= 0 || height <= 0) {
		stbi_image_free(loaded);
		return false;
	}

	uint32_t tex_w = (uint32_t)width;
	uint32_t tex_h = (uint32_t)height;
	for (unsigned i = 0; i < reduction; ++i) {
		if (tex_w > 4) {
			tex_w >>= 1;
		}
		if (tex_h > 4) {
			tex_h >>= 1;
		}
	}
	Validate_Texture_Size(tex_w, tex_h);

	std::vector<uint8_t> rgba((size_t)tex_w * (size_t)tex_h * 4u);
	if ((uint32_t)width == tex_w && (uint32_t)height == tex_h) {
		memcpy(rgba.data(), loaded, rgba.size());
	} else {
		for (uint32_t y = 0; y < tex_h; ++y) {
			const uint32_t src_y = (uint32_t)((uint64_t)y * (uint64_t)height / (uint64_t)tex_h);
			for (uint32_t x = 0; x < tex_w; ++x) {
				const uint32_t src_x = (uint32_t)((uint64_t)x * (uint64_t)width / (uint64_t)tex_w);
				const uint8_t *src = loaded + (src_y * (uint32_t)width + src_x) * 4u;
				uint8_t *dst = rgba.data() + (y * tex_w + x) * 4u;
				dst[0] = src[0];
				dst[1] = src[1];
				dst[2] = src[2];
				dst[3] = src[3];
			}
		}
	}
	stbi_image_free(loaded);

	if (allow_compression) {
		return Compress_Rgba_To_Dxt(rgba.data(), tex_w, tex_h, true, out);
	}

	out->format = WW3D_FORMAT_A8R8G8B8;
	out->width = tex_w;
	out->height = tex_h;
	out->mip_levels = 1;
	out->compressed = false;
	out->pixels.swap(rgba);
	return true;
}

} /* namespace */

bool Stb_Load_Texture(
	const char *path,
	unsigned reduction,
	bool allow_compression,
	bool try_dds,
	bool flip_vertical,
	StbLoadedTexture *out)
{
	if (path == nullptr || path[0] == '\0' || out == nullptr) {
		return false;
	}

	std::vector<uint8_t> file_bytes;
	if (try_dds) {
		char dds_path[256];
		strncpy(dds_path, path, sizeof(dds_path) - 1);
		dds_path[sizeof(dds_path) - 1] = '\0';
		Set_Extension(dds_path, sizeof(dds_path), 'd', 'd', 's');
		if (Read_File_Bytes(dds_path, file_bytes) && Load_Dds_From_Bytes(file_bytes, reduction, out)) {
			return true;
		}
	}

	file_bytes.clear();
	if (!Read_File_Bytes(path, file_bytes)) {
		char alt_path[256];
		strncpy(alt_path, path, sizeof(alt_path) - 1);
		alt_path[sizeof(alt_path) - 1] = '\0';
		Set_Extension(alt_path, sizeof(alt_path), 'p', 'n', 'g');
		if (!Read_File_Bytes(alt_path, file_bytes)) {
			Set_Extension(alt_path, sizeof(alt_path), 't', 'g', 'a');
			if (!Read_File_Bytes(alt_path, file_bytes)) {
				return false;
			}
		}
	}

	if (Load_Dds_From_Bytes(file_bytes, reduction, out)) {
		return true;
	}
	return Load_Image_From_Bytes(file_bytes, reduction, allow_compression, flip_vertical, out);
}

} /* namespace ww3d_vulkan */

#endif /* RENEGADE_VULKAN */
