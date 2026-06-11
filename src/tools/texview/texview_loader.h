#ifndef TEXVIEW_LOADER_H
#define TEXVIEW_LOADER_H

#include "../ww3d2/ww3dformat.h"

namespace ww3d_vulkan {
class VkTexture;
}

struct TexViewTextureInfo {
	unsigned width = 0;
	unsigned height = 0;
	unsigned mip_levels = 0;
	WW3DFormat format = WW3D_FORMAT_UNKNOWN;
	bool compressed = false;
	char path[512];
};

bool TexView_Load_Texture(const char *path, ww3d_vulkan::VkTexture *texture, TexViewTextureInfo *info);

const char *TexView_Format_Name(WW3DFormat format);

#endif
