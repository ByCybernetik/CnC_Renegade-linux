#include "vulkan_render_device.h"

#if defined(RENEGADE_VULKAN)

#include "vk_dx8_bridge.h"
#include "vk_dx8_state.h"
#include "vk_native_render_state.h"
#include "ww3d_vulkan.h"
#include "../ww3d2/dx8wrapper.h"
#include "../ww3d2/dx8vertexbuffer.h"
#include "../ww3d2/dx8indexbuffer.h"
#include "../ww3d2/shader.h"
#include "../ww3d2/texture.h"
#include "vk_texture.h"
#include "../wwmath/matrix4.h"

#include <d3d8.h>

namespace ww3d_vulkan {

static TextureClass *g_current_render_target = nullptr;

VulkanRenderDevice &VulkanRenderDevice::Get()
{
	static VulkanRenderDevice instance;
	return instance;
}

bool VulkanRenderDevice::Is_Active() const
{
	return WW3DVulkan::Get().Is_Active();
}

bool VulkanRenderDevice::Begin_Frame()
{
	return Try_Begin_Scene();
}

bool VulkanRenderDevice::End_Frame(bool present)
{
	return Try_End_Scene(present);
}

void VulkanRenderDevice::Begin_Render(
	bool clear_color,
	bool clear_depth,
	float clear_r,
	float clear_g,
	float clear_b)
{
	if (!Is_Active()) {
		return;
	}

	int width = 0;
	int height = 0;
	Get_Render_Target_Resolution(width, height);

	ViewportDesc vp;
	vp.x = 0;
	vp.y = 0;
	vp.w = width > 0 ? (uint32_t)width : 0;
	vp.h = height > 0 ? (uint32_t)height : 0;
	Set_Viewport(vp);

	if (clear_color || clear_depth) {
		ClearDesc clear_desc;
		clear_desc.color = clear_color;
		clear_desc.depth = clear_depth;
		clear_desc.r = clear_r;
		clear_desc.g = clear_g;
		clear_desc.b = clear_b;
		clear_desc.a = 1.0f;
		Clear(clear_desc);
	}

	Begin_Frame();
}

bool VulkanRenderDevice::End_Render(bool present)
{
	if (!Is_Active()) {
		return false;
	}

	const bool ok = End_Frame(present);
	DX8Wrapper::Vulkan_Notify_Frame_End(present, ok);
	DX8Wrapper::Vulkan_Reset_Render_State_After_Frame();
	return ok;
}

void VulkanRenderDevice::Clear(const ClearDesc &desc)
{
	Try_Clear(desc.color, desc.depth, desc.r, desc.g, desc.b, desc.a);
}

void VulkanRenderDevice::Set_Viewport(const ViewportDesc &desc)
{
	Try_Set_Viewport(desc.x, desc.y, desc.w, desc.h);
}

void VulkanRenderDevice::Resize(uint32_t width, uint32_t height)
{
	Try_Resize(width, height);
}

TextureClass *VulkanRenderDevice::Create_Render_Target(
	int width,
	int height,
	WW3DFormat format)
{
	if (!Is_Active() || width <= 0 || height <= 0) {
		return nullptr;
	}

	if (format == WW3D_FORMAT_UNKNOWN) {
		format = WW3D_FORMAT_A8R8G8B8;
	}

	TextureClass *tex = NEW_REF(
		TextureClass,
		(width,
		 height,
		 format,
		 TextureClass::MIP_LEVELS_1,
		 TextureClass::POOL_DEFAULT,
		 true));
	if (tex->Peek_Vulkan_Texture() == nullptr) {
		REF_PTR_RELEASE(tex);
	}
	return tex;
}

void VulkanRenderDevice::Set_Render_Target(TextureClass *texture)
{
	if (!Is_Active()) {
		return;
	}

	REF_PTR_SET(g_current_render_target, texture);
	if (texture != nullptr) {
		Try_Set_Render_Target(
			static_cast<VkTexture *>(texture->Peek_Vulkan_Texture()));
	} else {
		Try_Set_Render_Target(nullptr);
	}
}

void VulkanRenderDevice::Restore_Default_Render_Target()
{
	Set_Render_Target(nullptr);
}

void VulkanRenderDevice::Get_Render_Target_Resolution(
	int &width,
	int &height) const
{
	if (g_current_render_target != nullptr) {
		width = g_current_render_target->Get_Width();
		height = g_current_render_target->Get_Height();
		return;
	}

	VkExtent2D extent = WW3DVulkan::Get().Renderer().Extent();
	width = (int)extent.width;
	height = (int)extent.height;
}

bool VulkanRenderDevice::Is_Render_To_Texture() const
{
	return g_current_render_target != nullptr;
}

void VulkanRenderDevice::Reset_Ui_Texture_Stages(bool unbind_textures)
{
	if (!Is_Active()) {
		return;
	}

	Matrix4 identity(true);
	for (unsigned stage = 0; stage < MAX_TEXTURE_STAGES; ++stage) {
		DX8Wrapper::Set_DX8_Texture_Stage_State(
			stage,
			D3DTSS_TEXCOORDINDEX,
			D3DTSS_TCI_PASSTHRU | stage);
		DX8Wrapper::Set_DX8_Texture_Stage_State(
			stage,
			D3DTSS_TEXTURETRANSFORMFLAGS,
			D3DTTFF_DISABLE);
		DX8Wrapper::Set_Transform(
			stage == 0 ? D3DTS_TEXTURE0 : D3DTS_TEXTURE1,
			identity);
	}

	/* 3D backdrop materials leave bump/dot-product stage ops that break 2D/UI. */
	DX8Wrapper::Set_DX8_Texture_Stage_State(
		0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	DX8Wrapper::Set_DX8_Texture_Stage_State(
		0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
	DX8Wrapper::Set_DX8_Texture_Stage_State(
		0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	DX8Wrapper::Set_DX8_Texture_Stage_State(
		0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	DX8Wrapper::Set_DX8_Texture_Stage_State(
		0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	DX8Wrapper::Set_DX8_Texture_Stage_State(
		0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
	DX8Wrapper::Set_DX8_Texture_Stage_State(
		1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State(
		1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

	if (unbind_textures) {
		DX8Wrapper::Set_Texture(0, nullptr);
		DX8Wrapper::Set_Texture(1, nullptr);
	}
}

NativeTextureState VulkanRenderDevice::Build_Ui_Texture_State(
	const ShaderClass &shader)
{
	NativeTextureState native;
	native.uv_mode[0] = UvMode::Passthru;
	native.uv_mode[1] = UvMode::Passthru;
	native.uv_index[0] = 0;
	native.uv_index[1] = 1;

	switch (shader.Get_Primary_Gradient()) {
	case ShaderClass::GRADIENT_DISABLE:
		native.tex_stage0_mode = 0.0f;
		break;
	case ShaderClass::GRADIENT_ADD:
		native.tex_stage0_mode = 2.0f;
		break;
	default:
		native.tex_stage0_mode = 1.0f;
		break;
	}

	native.tex_stage1_color_mode =
		(float)shader.Get_Post_Detail_Color_Func();
	native.tex_stage1_alpha_mode =
		(float)shader.Get_Post_Detail_Alpha_Func();
	return native;
}

void VulkanRenderDevice::Prepare_Draw(
	bool reset_ui_stages,
	const ShaderClass &shader,
	const Matrix4 &world,
	const Matrix4 &view,
	const Matrix4 &projection,
	TextureClass *const *textures,
	unsigned texture_count)
{
	if (!Is_Active()) {
		return;
	}

	if (reset_ui_stages) {
		Reset_Ui_Texture_Stages(false);
		Push_Native_Texture_State(Build_Ui_Texture_State(shader));

		TextureClass *tex0 = nullptr;
		if (textures != nullptr && texture_count > 0) {
			tex0 = textures[0];
		}
		DX8Wrapper::Set_Texture(0, tex0);
		DX8Wrapper::Set_Texture(1, nullptr);
	}

	/*
	 * Apply_Render_State_Changes() runs before Prepare_Draw in Draw_Primitive.
	 * Terrain/sky also use BUFFER_TYPE_DYNAMIC_DX8 but are not 2D UI — their
	 * Set_Texture calls after Apply would never reach Vulkan without an explicit
	 * bind here.
	 */
	if (textures != nullptr && texture_count > 0) {
		if (reset_ui_stages) {
			TextureClass *ui_textures[MAX_TEXTURE_STAGES] = {};
			ui_textures[0] = textures[0];
			DX8Wrapper::Vulkan_Bind_Texture_Stages(ui_textures, texture_count);
		} else {
			DX8Wrapper::Vulkan_Bind_Texture_Stages(textures, texture_count);
		}
	}

	Sync_Matrices(world, view, projection);
	Sync_Draw_State();
}

static bool Is_Ui_Dynamic_Draw(const RenderStateStruct &rs)
{
	if (rs.vertex_buffer_type != BUFFER_TYPE_DYNAMIC_DX8) {
		return false;
	}
	if (rs.vertex_buffer == nullptr) {
		return false;
	}
	const unsigned fvf = rs.vertex_buffer->FVF_Info().Get_FVF();
	if ((fvf & D3DFVF_POSITION_MASK) == D3DFVF_XYZRHW) {
		return true;
	}
	/* Render2D menu/HUD (XYZ+NORMAL+TEX2+DIFFUSE, depth-less overlay). */
	if (fvf == dynamic_fvf_type &&
			rs.shader.Get_Depth_Compare() == ShaderClass::PASS_ALWAYS &&
			rs.shader.Get_Depth_Mask() == ShaderClass::DEPTH_WRITE_DISABLE) {
		return true;
	}
	return false;
}

static void Fixup_Vertex_Count(
	const RenderStateStruct &rs,
	unsigned short min_vertex_index,
	unsigned short &vertex_count)
{
	if (vertex_count >= 3) {
		return;
	}

	switch (rs.vertex_buffer_type) {
	case BUFFER_TYPE_DX8:
	case BUFFER_TYPE_SORTING:
		if (rs.vertex_buffer != nullptr) {
			vertex_count = (unsigned short)(
				rs.vertex_buffer->Get_Vertex_Count() -
				rs.index_base_offset -
				rs.vba_offset -
				min_vertex_index);
		}
		break;
	case BUFFER_TYPE_DYNAMIC_DX8:
	case BUFFER_TYPE_DYNAMIC_SORTING:
		vertex_count = rs.vba_count;
		break;
	default:
		break;
	}
}

void VulkanRenderDevice::Draw_Primitive(
	unsigned primitive_type,
	unsigned short start_index,
	unsigned short polygon_count,
	unsigned short min_vertex_index,
	unsigned short vertex_count)
{
	if (!Is_Active()) {
		return;
	}

	DX8Wrapper::Apply_Render_State_Changes();

	if (!DX8Wrapper::_Is_Triangle_Draw_Enabled()) {
		return;
	}

	RenderStateStruct rs;
	DX8Wrapper::Get_Render_State(rs);

	unsigned short vc = vertex_count;
	Fixup_Vertex_Count(rs, min_vertex_index, vc);

	Matrix4 world;
	Matrix4 view;
	Matrix4 projection;
	DX8Wrapper::Get_Transform(D3DTS_WORLD, world);
	DX8Wrapper::Get_Transform(D3DTS_VIEW, view);
	DX8Wrapper::Get_Transform(D3DTS_PROJECTION, projection);

	Prepare_Draw(
		Is_Ui_Dynamic_Draw(rs),
		rs.shader,
		world,
		view,
		projection,
		rs.Textures,
		MAX_TEXTURE_STAGES);

	switch (rs.vertex_buffer_type) {
	case BUFFER_TYPE_DX8:
	case BUFFER_TYPE_DYNAMIC_DX8:
		switch (rs.index_buffer_type) {
		case BUFFER_TYPE_DX8:
		case BUFFER_TYPE_DYNAMIC_DX8:
			{
				DX8VertexBufferClass *vb =
					static_cast<DX8VertexBufferClass *>(rs.vertex_buffer);
				DX8IndexBufferClass *ib =
					static_cast<DX8IndexBufferClass *>(rs.index_buffer);
				if (vb == nullptr || ib == nullptr) {
					break;
				}
				DrawIndexedDesc draw_desc;
				draw_desc.primitive_type = primitive_type;
				draw_desc.start_index = start_index + rs.iba_offset;
				draw_desc.polygon_count = polygon_count;
				/* Match D3D8 SetIndices BaseVertexIndex (index_base + vba). */
				draw_desc.vertex_offset = rs.index_base_offset + rs.vba_offset;
				draw_desc.vertex_count = vc;
				draw_desc.shader = &rs.shader;
				draw_desc.vertex_buffer = vb;
				draw_desc.index_buffer = ib;
				draw_desc.fvf = rs.vertex_buffer->FVF_Info().Get_FVF();

				Draw_Indexed(draw_desc);
			}
			break;
		default:
			break;
		}
		break;
	case BUFFER_TYPE_SORTING:
	case BUFFER_TYPE_DYNAMIC_SORTING:
		switch (rs.index_buffer_type) {
		case BUFFER_TYPE_SORTING:
		case BUFFER_TYPE_DYNAMIC_SORTING:
			DX8Wrapper::Draw_Sorting_IB_VB(
				primitive_type,
				start_index,
				polygon_count,
				min_vertex_index,
				vc);
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
}

void VulkanRenderDevice::Draw_Triangles(
	unsigned short start_index,
	unsigned short polygon_count,
	unsigned short min_vertex_index,
	unsigned short vertex_count)
{
	Draw_Primitive(
		D3DPT_TRIANGLELIST,
		start_index,
		polygon_count,
		min_vertex_index,
		vertex_count);
}

void VulkanRenderDevice::Draw_Strip(
	unsigned short start_index,
	unsigned short polygon_count,
	unsigned short min_vertex_index,
	unsigned short vertex_count)
{
	Draw_Primitive(
		D3DPT_TRIANGLESTRIP,
		start_index,
		polygon_count,
		min_vertex_index,
		vertex_count);
}

bool VulkanRenderDevice::Draw_Indexed(const DrawIndexedDesc &desc)
{
	if (!Is_Active()) {
		return false;
	}
	if (desc.vertex_buffer == nullptr || desc.index_buffer == nullptr) {
		return false;
	}
	if (desc.shader == nullptr) {
		return false;
	}

	return Try_Draw_Indexed(
		desc.primitive_type,
		desc.start_index,
		desc.polygon_count,
		desc.vertex_offset,
		desc.vertex_count,
		*desc.shader,
		desc.vertex_buffer,
		desc.index_buffer,
		desc.fvf);
}

void VulkanRenderDevice::Flush_Pending_Draws()
{
	if (!Is_Active()) {
		return;
	}
	WW3DVulkan::Get().Renderer().Flush_Pending_Draws();
}

void VulkanRenderDevice::Draw_2D_Batch(const Native2DBatchDesc &desc)
{
	if (!Is_Active()) {
		return;
	}

	WW3DVulkan::Get().Renderer().Flush_Pending_Draws();

	VkTexture *texture = static_cast<VkTexture *>(desc.texture);
	VkRenderer &renderer = WW3DVulkan::Get().Renderer();

	renderer.Draw_Batch(
		renderer.Current_Command_Buffer(),
		desc.vertices,
		desc.vertex_count,
		desc.indices,
		desc.index_count,
		texture,
		desc.texturing,
		desc.src_blend,
		desc.dst_blend,
		desc.modulate_color,
		desc.viewport_x,
		desc.viewport_y,
		desc.viewport_w,
		desc.viewport_h);
}

} /* namespace ww3d_vulkan */

#endif /* RENEGADE_VULKAN */
