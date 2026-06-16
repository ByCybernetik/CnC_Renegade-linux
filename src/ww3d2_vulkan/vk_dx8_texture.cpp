#include "vk_dx8_texture.h"

#if defined(RENEGADE_VULKAN)

#include "vk_texture.h"
#include "vk_upload.h"
#include "stb_texture.h"
#include "ww3d_vulkan.h"
#if defined(RENEGADE_BOOT_LOG)
#include "renegade_texture_log.h"
#endif
#include "../ww3d2/assetmgr.h"
#include "../ww3d2/texture.h"
#include "../ww3d2/ddsfile.h"
#include "../wwlib/hashtemplate.h"
#include "../ww3d2/ww3dformat.h"
#include "../ww3d2/bitmaphandler.h"
#include "../wwlib/osdep.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <vector>

namespace ww3d_vulkan {

static VkTexture *g_MissingVulkanTexture = nullptr;
static VkTexture *g_DebugCursorVkTex = nullptr;

void Dbg_Set_Cursor_Vulkan_Texture(VkTexture *texture)
{
	g_DebugCursorVkTex = texture;
}

VkTexture *Dbg_Peek_Cursor_Vulkan_Texture()
{
	return g_DebugCursorVkTex;
}

static void Dbg_Track_Cursor_Texture(TextureClass *texture, VkTexture *vk_tex)
{
	if (texture == nullptr || vk_tex == nullptr) {
		return;
	}
	const char *path = texture->Get_Full_Path().Peek_Buffer();
	if (path != nullptr && strstr(path, "cursor") != nullptr) {
		Dbg_Set_Cursor_Vulkan_Texture(vk_tex);
	}
}

static VkSamplerAddressMode Addr_Mode(TextureClass::TxtAddrMode mode)
{
	switch (mode) {
	case TextureClass::TEXTURE_ADDRESS_CLAMP:
		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	case TextureClass::TEXTURE_ADDRESS_REPEAT:
	default:
		return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	}
}

void Init_Missing_Vulkan_Texture()
{
	if (g_MissingVulkanTexture != nullptr) {
		return;
	}
	g_MissingVulkanTexture = new (std::nothrow) VkTexture();
	if (g_MissingVulkanTexture == nullptr) {
		return;
	}
	if (!g_MissingVulkanTexture->Create_Solid(255, 0, 255, 255)) {
		delete g_MissingVulkanTexture;
		g_MissingVulkanTexture = nullptr;
	}
}

void Shutdown_Missing_Vulkan_Texture()
{
	if (g_MissingVulkanTexture != nullptr) {
		g_MissingVulkanTexture->Destroy();
		delete g_MissingVulkanTexture;
		g_MissingVulkanTexture = nullptr;
	}
}

VkTexture *Peek_Missing_Vulkan_Texture()
{
	return g_MissingVulkanTexture;
}

static const char *Texture_Load_Path(const TextureClass *texture)
{
	if (texture == nullptr) {
		return nullptr;
	}
	const char *path = texture->Get_Full_Path().Peek_Buffer();
	if (path != nullptr && path[0] != '\0') {
		return path;
	}
	path = texture->Get_Texture_Name().Peek_Buffer();
	if (path != nullptr && path[0] != '\0') {
		return path;
	}
	return nullptr;
}

/*
 * waves2.w3d (M01 beach water) references extensionless logical names that are not
 * stored as separate files in always.dat — map them to the retail water textures.
 */
static const char *Lookup_Texture_Alias(const char *path)
{
	if (path == nullptr || path[0] == '\0') {
		return nullptr;
	}
	if (stricmp(path, "a_water") == 0) {
		return "water_texture.tga";
	}
	if (stricmp(path, "a_water-foam") == 0 || stricmp(path, "a_water-FOAM") == 0) {
		return "water_foam.tga";
	}
	if (stricmp(path, "a_alpha-foam") == 0 || stricmp(path, "a_alpha-FOAM") == 0) {
		return "water_foam.tga";
	}
	if (stricmp(path, "water_reflect.tga") == 0) {
		return "water_reflect.dds";
	}
	return nullptr;
}

static bool Is_Water_Debug_Name(const char *path)
{
	if (path == nullptr || path[0] == '\0') {
		return false;
	}
	return stricmp(path, "a_water") == 0 ||
		stricmp(path, "a_water-foam") == 0 ||
		stricmp(path, "a_water-FOAM") == 0 ||
		stricmp(path, "a_alpha-foam") == 0 ||
		stricmp(path, "a_alpha-FOAM") == 0 ||
		stricmp(path, "water_texture.tga") == 0 ||
		stricmp(path, "water_foam.tga") == 0 ||
		stricmp(path, "water_reflect.tga") == 0 ||
		stricmp(path, "bump_water.tga") == 0;
}

static void Log_Water_Texture_Failure(const char *path, const char *alias)
{
	if (!Is_Water_Debug_Name(path) && !Is_Water_Debug_Name(alias)) {
		return;
	}
	if (alias != nullptr && alias[0] != '\0') {
		fprintf(
			stderr,
			"WW3DVulkan: water texture load failed (path='%s', alias='%s')\n",
			path != nullptr ? path : "",
			alias);
	} else {
		fprintf(
			stderr,
			"WW3DVulkan: water texture load failed (path='%s')\n",
			path != nullptr ? path : "");
	}
}

static bool Try_Load_Texture_From_Path(
	TextureClass *texture,
	const char *path,
	VkTexture *vk_tex,
	const VkSamplerAddressMode address_u,
	const VkSamplerAddressMode address_v)
{
	if (path == nullptr || path[0] == '\0' || texture == nullptr || vk_tex == nullptr) {
		return false;
	}

	DDSFileClass dds(path, texture->Get_Reduction());
	if (dds.Is_Available() && dds.Load()) {
		if (vk_tex->Create_From_DDS(dds, address_u, address_v)) {
			texture->Set_Vulkan_Texture(vk_tex);
			texture->Set_Dimensions((int)vk_tex->Width(), (int)vk_tex->Height());
			texture->Mark_Vulkan_Initialized();
			Dbg_Track_Cursor_Texture(texture, vk_tex);
#if defined(RENEGADE_BOOT_LOG)
			Tex_Log_Load(
				"file_load",
				path,
				false,
				vk_tex->Width(),
				vk_tex->Height(),
				vk_tex,
				"\"src\":\"dds\"");
#endif
			return true;
		}
		vk_tex->Destroy();
	}

	StbLoadedTexture loaded;
	if (!Stb_Load_Texture(
			path,
			texture->Get_Reduction(),
			texture->Is_Compression_Allowed(),
			true,
			true,
			&loaded)) {
		return false;
	}

	bool ok = false;
	if (loaded.compressed) {
		ok = vk_tex->Create_From_Compressed(
			loaded.format,
			loaded.width,
			loaded.height,
			loaded.mip_levels,
			loaded.pixels.data(),
			loaded.pixels.size(),
			address_u,
			address_v);
	} else {
		ok = vk_tex->Create_From_Rgba8(
			loaded.pixels.data(),
			loaded.width,
			loaded.height,
			address_u,
			address_v);
	}
	if (!ok) {
		return false;
	}

	texture->Set_Vulkan_Texture(vk_tex);
	texture->Set_Dimensions((int)vk_tex->Width(), (int)vk_tex->Height());
	texture->Mark_Vulkan_Initialized();
	Dbg_Track_Cursor_Texture(texture, vk_tex);
#if defined(RENEGADE_BOOT_LOG)
	Tex_Log_Load(
		"file_load",
		path,
		false,
		vk_tex->Width(),
		vk_tex->Height(),
		vk_tex,
		loaded.compressed ? "\"src\":\"stb_compressed\"" : "\"src\":\"stb_rgba\"");
#endif
	return true;
}

static bool Is_Missing_Placeholder_VkTexture(const VkTexture *vk_tex)
{
	return vk_tex != nullptr && vk_tex->Width() == 1u && vk_tex->Height() == 1u &&
		vk_tex->Mip_Levels() == 1u;
}

static void Clear_Missing_Placeholder_VkTexture(TextureClass *texture)
{
	if (texture == nullptr) {
		return;
	}
	VkTexture *vk_tex = static_cast<VkTexture *>(texture->Peek_Vulkan_Texture());
	if (!Is_Missing_Placeholder_VkTexture(vk_tex)) {
		return;
	}
	vk_tex->Destroy();
	delete vk_tex;
	texture->Set_Vulkan_Texture(nullptr);
}

bool Ensure_Texture_Loaded(TextureClass *texture)
{
	if (texture == nullptr || !WW3DVulkan::Get().Is_Active()) {
		return false;
	}
	if (texture->Peek_Vulkan_Texture() != nullptr) {
		if (Is_Missing_Placeholder_VkTexture(
				static_cast<VkTexture *>(texture->Peek_Vulkan_Texture()))) {
			Clear_Missing_Placeholder_VkTexture(texture);
		} else {
			Dbg_Track_Cursor_Texture(
				texture,
				static_cast<VkTexture *>(texture->Peek_Vulkan_Texture()));
			return true;
		}
	}
	if (texture->Is_Procedural()) {
		return false;
	}

	const char *path = Texture_Load_Path(texture);
	if (path == nullptr) {
		return false;
	}

	const VkSamplerAddressMode address_u = Addr_Mode(texture->Get_U_Addr_Mode());
	const VkSamplerAddressMode address_v = Addr_Mode(texture->Get_V_Addr_Mode());

	const char *alias = Lookup_Texture_Alias(path);

	VkTexture *vk_tex = new (std::nothrow) VkTexture();
	if (vk_tex == nullptr) {
		Log_Water_Texture_Failure(path, alias);
		return false;
	}

	if (alias != nullptr &&
		Try_Load_Texture_From_Path(texture, alias, vk_tex, address_u, address_v))
	{
		return true;
	}
	delete vk_tex;

	vk_tex = new (std::nothrow) VkTexture();
	if (vk_tex == nullptr) {
		Log_Water_Texture_Failure(path, alias);
		return false;
	}

	if (Try_Load_Texture_From_Path(texture, path, vk_tex, address_u, address_v)) {
		return true;
	}
	delete vk_tex;

	Log_Water_Texture_Failure(path, alias);
	return false;
}

static bool g_last_stage0_pickup_texture = false;

static bool Is_Pickup_Texture_Name(const char *name)
{
	if (name == nullptr) {
		return false;
	}
	return (((name[0] == 'p' || name[0] == 'P') && name[1] == '_') ||
		((name[0] == 'w' || name[0] == 'W') && name[1] == '_'));
}

bool Last_Stage0_Texture_Is_Pickup(void)
{
	return g_last_stage0_pickup_texture;
}

void Texture_Stage_Bind(TextureClass *texture, unsigned stage)
{
	if (stage >= 2 || !WW3DVulkan::Get().Is_Active()) {
		return;
	}
	if (stage == 0) {
		g_last_stage0_pickup_texture = false;
		if (texture != nullptr && !texture->Is_Procedural()) {
			g_last_stage0_pickup_texture =
				Is_Pickup_Texture_Name(texture->Get_Texture_Name());
		}
	}

	if (texture == nullptr) {
		WW3DVulkan::Get().Bind_Texture(stage, nullptr);
		return;
	}

	VkTexture *vk_tex = nullptr;
	if (!texture->Is_Procedural() &&
		(texture->Peek_Vulkan_Texture() == nullptr ||
			Is_Missing_Placeholder_VkTexture(
				static_cast<VkTexture *>(texture->Peek_Vulkan_Texture()))))
	{
		Ensure_Texture_Loaded(texture);
	}
	vk_tex = static_cast<VkTexture *>(texture->Peek_Vulkan_Texture());
	if (!texture->Is_Procedural() && Is_Missing_Placeholder_VkTexture(vk_tex)) {
		Clear_Missing_Placeholder_VkTexture(texture);
		vk_tex = nullptr;
	}
	const bool used_missing_fallback =
		(vk_tex == nullptr || vk_tex->View() == VK_NULL_HANDLE);
	if (used_missing_fallback) {
		vk_tex = g_MissingVulkanTexture;
	}
	if (vk_tex == nullptr || vk_tex->View() == VK_NULL_HANDLE) {
		return;
	}
	WW3DVulkan::Get().Bind_Texture(stage, vk_tex);
#if defined(RENEGADE_BOOT_LOG)
	if (texture != nullptr && !texture->Is_Procedural()) {
		const char *path = texture->Get_Full_Path().Peek_Buffer();
		const bool missing = (vk_tex == g_MissingVulkanTexture);
		Tex_Log_Stage_Bind(stage, path, vk_tex, missing);
	}
#endif
}

void Texture_Stage_Bind_Null(unsigned stage)
{
	Texture_Stage_Bind(nullptr, stage);
}

bool Apply_Loaded_Texture(TextureClass *texture, bool initialize)
{
	if (texture == nullptr || !WW3DVulkan::Get().Is_Active()) {
		return false;
	}
	if (!Ensure_Texture_Loaded(texture)) {
		return false;
	}
	if (initialize) {
		texture->Mark_Vulkan_Initialized();
	}
	return true;
}

void Apply_Missing_Texture(TextureClass *texture)
{
	if (texture == nullptr || !WW3DVulkan::Get().Is_Active()) {
		return;
	}
	/*
	 * Do not mark initialized — Texture_Stage_Bind / a later Init() can retry
	 * after alias resolution (e.g. a_water -> water_texture.tga).
	 */
}

void Warmup_All_File_Textures()
{
	while (!Warmup_File_Textures_Batch(0)) {
	}
}

namespace {

struct TextureWarmupState {
	std::vector<TextureClass *> pending;
	size_t index = 0;
	bool active = false;
	bool rescan_pending = true;
};

TextureWarmupState g_tex_warmup;

static void Rebuild_Texture_Warmup_List()
{
	g_tex_warmup.pending.clear();
	g_tex_warmup.index = 0;

	WW3DAssetManager *mgr = WW3DAssetManager::Get_Instance();
	if (mgr == nullptr) {
		g_tex_warmup.active = false;
		g_tex_warmup.rescan_pending = false;
		return;
	}

	HashTemplateIterator<StringClass, TextureClass *> ite(mgr->Texture_Hash());
	for (ite.First(); !ite.Is_Done(); ite.Next()) {
		TextureClass *tex = ite.Peek_Value();
		if (tex == nullptr || tex->Is_Procedural()) {
			continue;
		}
		if (tex->Peek_Vulkan_Texture() != nullptr) {
			continue;
		}
		g_tex_warmup.pending.push_back(tex);
	}

	g_tex_warmup.active = !g_tex_warmup.pending.empty();
	g_tex_warmup.rescan_pending = false;
}

} /* namespace */

void Reset_File_Texture_Warmup()
{
	g_tex_warmup.pending.clear();
	g_tex_warmup.index = 0;
	g_tex_warmup.active = false;
	g_tex_warmup.rescan_pending = true;
}

void Rescan_File_Texture_Warmup()
{
	if (g_tex_warmup.active) {
		return;
	}
	g_tex_warmup.rescan_pending = true;
}

bool Warmup_File_Textures_Batch(unsigned batch_size)
{
	if (!WW3DVulkan::Get().Is_Active()) {
		return true;
	}

	if (g_tex_warmup.rescan_pending || !g_tex_warmup.active) {
		Rebuild_Texture_Warmup_List();
	}
	if (!g_tex_warmup.active) {
		return true;
	}

	unsigned loaded = 0;
	while (g_tex_warmup.index < g_tex_warmup.pending.size()) {
		TextureClass *tex = g_tex_warmup.pending[g_tex_warmup.index++];
		if (!Apply_Loaded_Texture(tex, true)) {
			Apply_Missing_Texture(tex);
		}
		if (batch_size > 0) {
			loaded++;
			if (loaded >= batch_size) {
				return false;
			}
		}
	}

	g_tex_warmup.pending.clear();
	g_tex_warmup.index = 0;
	g_tex_warmup.active = false;
	return true;
}

bool Upload_Procedural_Texture_Rgb565(
	TextureClass *texture,
	WW3DFormat format,
	const unsigned char *src,
	int src_pitch,
	unsigned copy_w,
	unsigned copy_h)
{
	(void)format;
	if (texture == nullptr || src == nullptr || src_pitch <= 0 || copy_w == 0 || copy_h == 0 ||
		!WW3DVulkan::Get().Is_Active())
	{
		return false;
	}

	VkTexture *vk_tex = static_cast<VkTexture *>(texture->Peek_Vulkan_Texture());
	if (vk_tex == nullptr) {
		const int tex_w = texture->Get_Width();
		const int tex_h = texture->Get_Height();
		if (tex_w <= 0 || tex_h <= 0) {
			return false;
		}
		vk_tex = new (std::nothrow) VkTexture();
		if (vk_tex == nullptr ||
			!vk_tex->Create_Empty(
				(uint32_t)tex_w,
				(uint32_t)tex_h,
				WW3D_FORMAT_A8R8G8B8,
				true))
		{
			delete vk_tex;
			return false;
		}
		texture->Set_Vulkan_Texture(vk_tex);
		texture->Mark_Vulkan_Initialized();
	}

	std::vector<unsigned char> rgba((size_t)copy_w * (size_t)copy_h * 4u);
	BitmapHandlerClass::Copy_Image(
		rgba.data(),
		copy_w,
		copy_h,
		copy_w * 4u,
		WW3D_FORMAT_A8R8G8B8,
		const_cast<unsigned char *>(src),
		copy_w,
		copy_h,
		(unsigned)src_pitch,
		WW3D_FORMAT_R5G6B5,
		nullptr,
		0,
		false);

	return vk_tex->Upload_Rgba8_Region(
		rgba.data(),
		(int)(copy_w * 4u),
		0,
		0,
		copy_w,
		copy_h);
}

struct Clone_Render_Target_User {
	VkImage src_image = VK_NULL_HANDLE;
	VkImage dst_image = VK_NULL_HANDLE;
	uint32_t width = 0;
	uint32_t height = 0;
};

static void Record_Clone_Render_Target(VkCommandBuffer cmd, void *user_ptr)
{
	Clone_Render_Target_User *user = static_cast<Clone_Render_Target_User *>(user_ptr);
	if (user == nullptr || user->src_image == VK_NULL_HANDLE || user->dst_image == VK_NULL_HANDLE ||
		user->width == 0 || user->height == 0)
	{
		return;
	}

	VkImageMemoryBarrier barriers[2] = {};
	barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[0].image = user->src_image;
	barriers[0].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barriers[0].subresourceRange.levelCount = 1;
	barriers[0].subresourceRange.layerCount = 1;
	barriers[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

	barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[1].image = user->dst_image;
	barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barriers[1].subresourceRange.levelCount = 1;
	barriers[1].subresourceRange.layerCount = 1;
	barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

	vkCmdPipelineBarrier(
		cmd,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		0,
		nullptr,
		0,
		nullptr,
		2,
		barriers);

	VkImageCopy region = {};
	region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.srcSubresource.layerCount = 1;
	region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.dstSubresource.layerCount = 1;
	region.extent.width = user->width;
	region.extent.height = user->height;
	region.extent.depth = 1;
	vkCmdCopyImage(
		cmd,
		user->src_image,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		user->dst_image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region);

	barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barriers[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(
		cmd,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		0,
		nullptr,
		0,
		nullptr,
		2,
		barriers);
}

TextureClass *Try_Clone_Render_Target_Texture(TextureClass *render_target)
{
	if (render_target == nullptr || !WW3DVulkan::Get().Is_Active()) {
		return nullptr;
	}

	VkTexture *src = static_cast<VkTexture *>(render_target->Peek_Vulkan_Texture());
	if (src == nullptr || src->Image() == VK_NULL_HANDLE) {
		return nullptr;
	}

	const int width = render_target->Get_Width();
	const int height = render_target->Get_Height();
	if (width <= 0 || height <= 0) {
		return nullptr;
	}

	WW3DFormat format = render_target->Get_Texture_Format();
	if (format == WW3D_FORMAT_UNKNOWN) {
		format = WW3D_FORMAT_A8R8G8B8;
	}

	VkTexture *dst_vk = new (std::nothrow) VkTexture();
	if (dst_vk == nullptr ||
		!dst_vk->Create_Empty((uint32_t)width, (uint32_t)height, format, true))
	{
		delete dst_vk;
		return nullptr;
	}

	Clone_Render_Target_User user;
	user.src_image = src->Image();
	user.dst_image = dst_vk->Image();
	user.width = (uint32_t)width;
	user.height = (uint32_t)height;
	Submit_One_Time_Commands(Record_Clone_Render_Target, &user);
	dst_vk->Set_Layout_Shader_Read_Only();

	TextureClass *new_texture = NEW_REF(
		TextureClass,
		((unsigned)width,
		 (unsigned)height,
		 format,
		 TextureClass::MIP_LEVELS_1,
		 TextureClass::POOL_MANAGED,
		 false));
	new_texture->Set_Vulkan_Texture(dst_vk);
	new_texture->Mark_Vulkan_Initialized();
	return new_texture;
}

} /* namespace ww3d_vulkan */

#endif /* RENEGADE_VULKAN */
