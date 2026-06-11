#ifndef W3DVIEWER_TEXTURE_H
#define W3DVIEWER_TEXTURE_H

#include "../ww3d2_vulkan/vk_texture.h"

#include <string>

namespace w3dviewer {

class GpuTexture {
public:
	GpuTexture() = default;
	~GpuTexture();

	GpuTexture(const GpuTexture &) = delete;
	GpuTexture &operator=(const GpuTexture &) = delete;

	bool Load_From_File(const std::string &path);
	bool Create_White_Fallback();

	ww3d_vulkan::VkTexture *Get() { return &texture_; }
	const ww3d_vulkan::VkTexture *Get() const { return &texture_; }

private:
	ww3d_vulkan::VkTexture texture_;
};

bool Resolve_Texture_Path(
	const std::string &filename,
	const std::string &search_dir,
	const std::string &extra_dir,
	std::string *out_path);

} /* namespace w3dviewer */

#endif
