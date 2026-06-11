#ifndef WW3D2_VULKAN_VK_DX8_TEXTURE_H
#define WW3D2_VULKAN_VK_DX8_TEXTURE_H

#if defined(RENEGADE_VULKAN)

#include "../ww3d2/ww3dformat.h"

class TextureClass;

namespace ww3d_vulkan {

void Init_Missing_Vulkan_Texture();
void Shutdown_Missing_Vulkan_Texture();

bool Ensure_Texture_Loaded(TextureClass *texture);
void Texture_Stage_Bind(TextureClass *texture, unsigned stage);
void Texture_Stage_Bind_Null(unsigned stage);
bool Apply_Loaded_Texture(TextureClass *texture, bool initialize);
void Apply_Missing_Texture(TextureClass *texture);
void Warmup_All_File_Textures();

class VkTexture;
VkTexture *Peek_Missing_Vulkan_Texture();
void Dbg_Set_Cursor_Vulkan_Texture(VkTexture *texture);
VkTexture *Dbg_Peek_Cursor_Vulkan_Texture();

bool Upload_Procedural_Texture_Rgb565(
	TextureClass *texture,
	WW3DFormat format,
	const unsigned char *src,
	int src_pitch,
	unsigned copy_w,
	unsigned copy_h);

TextureClass *Try_Clone_Render_Target_Texture(TextureClass *render_target);

} /* namespace ww3d_vulkan */

#endif

#endif
