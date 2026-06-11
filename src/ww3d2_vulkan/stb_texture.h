#ifndef WW3D2_VULKAN_STB_TEXTURE_H
#define WW3D2_VULKAN_STB_TEXTURE_H

#if defined(RENEGADE_VULKAN)

#include "../ww3d2/ww3dformat.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ww3d_vulkan {

struct StbLoadedTexture {
	WW3DFormat format = WW3D_FORMAT_UNKNOWN;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t mip_levels = 0;
	bool compressed = false;
	std::vector<uint8_t> pixels;
};

bool Stb_Load_Texture(
	const char *path,
	unsigned reduction,
	bool allow_compression,
	bool try_dds,
	bool flip_vertical,
	StbLoadedTexture *out);

} /* namespace ww3d_vulkan */

#endif /* RENEGADE_VULKAN */

#endif
