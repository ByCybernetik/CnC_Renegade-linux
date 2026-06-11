#include "vk_format.h"

namespace ww3d_vulkan {

VkFormat WW3D_Format_To_Vk(WW3DFormat format)
{
	switch (format) {
		case WW3D_FORMAT_R8G8B8:
			return VK_FORMAT_R8G8B8_UNORM;
		case WW3D_FORMAT_A8R8G8B8:
		case WW3D_FORMAT_X8R8G8B8:
			return VK_FORMAT_B8G8R8A8_UNORM;
		case WW3D_FORMAT_R5G6B5:
			return VK_FORMAT_R5G6B5_UNORM_PACK16;
		case WW3D_FORMAT_A1R5G5B5:
		case WW3D_FORMAT_X1R5G5B5:
			return VK_FORMAT_B5G5R5A1_UNORM_PACK16;
		case WW3D_FORMAT_A4R4G4B4:
			return VK_FORMAT_B4G4R4A4_UNORM_PACK16;
		case WW3D_FORMAT_A8:
			return VK_FORMAT_R8_UNORM;
		case WW3D_FORMAT_L8:
			return VK_FORMAT_R8_UNORM;
		case WW3D_FORMAT_A8L8:
			return VK_FORMAT_R8G8_UNORM;
		case WW3D_FORMAT_DXT1:
			return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
		case WW3D_FORMAT_DXT2:
		case WW3D_FORMAT_DXT3:
		case WW3D_FORMAT_DXT4:
		case WW3D_FORMAT_DXT5:
			return VK_FORMAT_BC3_UNORM_BLOCK;
		default:
			return VK_FORMAT_UNDEFINED;
	}
}

WW3DFormat Vk_Format_To_WW3D(VkFormat format)
{
	switch (format) {
		case VK_FORMAT_B8G8R8A8_UNORM:
			return WW3D_FORMAT_A8R8G8B8;
		case VK_FORMAT_R5G6B5_UNORM_PACK16:
			return WW3D_FORMAT_R5G6B5;
		case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
			return WW3D_FORMAT_A1R5G5B5;
		case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
			return WW3D_FORMAT_DXT1;
		case VK_FORMAT_BC3_UNORM_BLOCK:
			return WW3D_FORMAT_DXT5;
		default:
			return WW3D_FORMAT_UNKNOWN;
	}
}

uint32_t Vk_Format_Bytes_Per_Pixel(VkFormat format)
{
	switch (format) {
		case VK_FORMAT_R8_UNORM:
			return 1;
		case VK_FORMAT_R8G8_UNORM:
		case VK_FORMAT_R5G6B5_UNORM_PACK16:
		case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
		case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
			return 2;
		case VK_FORMAT_B8G8R8A8_UNORM:
		case VK_FORMAT_R8G8B8_UNORM:
			return 4;
		default:
			return 0;
	}
}

} /* namespace ww3d_vulkan */
