#include "texview_loader.h"

#include "../ww3d2_vulkan/stb_texture.h"
#include "../ww3d2_vulkan/vk_texture.h"

#include <cstdio>
#include <cstring>

const char *TexView_Format_Name(WW3DFormat format)
{
	switch (format) {
	case WW3D_FORMAT_DXT1:
		return "DXT1";
	case WW3D_FORMAT_DXT2:
		return "DXT2";
	case WW3D_FORMAT_DXT3:
		return "DXT3";
	case WW3D_FORMAT_DXT4:
		return "DXT4";
	case WW3D_FORMAT_DXT5:
		return "DXT5";
	case WW3D_FORMAT_A8R8G8B8:
		return "A8R8G8B8";
	case WW3D_FORMAT_R8G8B8:
		return "R8G8B8";
	default:
		return "unknown";
	}
}

bool TexView_Load_Texture(
	const char *path,
	ww3d_vulkan::VkTexture *texture,
	TexViewTextureInfo *info)
{
	if (path == nullptr || path[0] == '\0' || texture == nullptr) {
		return false;
	}

	texture->Destroy();

	const VkSamplerAddressMode address = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	ww3d_vulkan::StbLoadedTexture loaded;
	if (!ww3d_vulkan::Stb_Load_Texture(path, 0, true, true, true, &loaded)) {
		fprintf(stderr, "texview: failed to load: %s\n", path);
		return false;
	}

	bool ok = false;
	if (loaded.compressed) {
		ok = texture->Create_From_Compressed(
			loaded.format,
			loaded.width,
			loaded.height,
			loaded.mip_levels,
			loaded.pixels.data(),
			loaded.pixels.size(),
			address,
			address);
	} else {
		ok = texture->Create_From_Rgba8(
			loaded.pixels.data(),
			loaded.width,
			loaded.height,
			address,
			address);
	}

	if (!ok) {
		fprintf(stderr, "texview: Vulkan upload failed: %s\n", path);
		return false;
	}

	if (info != nullptr) {
		info->width = loaded.width;
		info->height = loaded.height;
		info->mip_levels = loaded.mip_levels;
		info->format = loaded.format;
		info->compressed = loaded.compressed;
		std::strncpy(info->path, path, sizeof(info->path) - 1);
		info->path[sizeof(info->path) - 1] = '\0';
	}
	return true;
}
