#include "vk_dx8_bridge.h"

#if defined(RENEGADE_VULKAN)

#include "vk_gpu_buffer.h"
#include "vk_dx8_state.h"
#include "vk_dx8_texture.h"
#include "vk_native_render_state.h"
#include "ww3d_vulkan.h"
#include "../ww3d2/dx8wrapper.h"
#include "../ww3d2/dx8vertexbuffer.h"
#include "vk_platform.h"
#include "../ww3d2/dx8indexbuffer.h"
#include "../ww3d2/dx8fvf.h"
#include "../ww3d2/shader.h"
#include "../ww3d2/texture.h"
#include "../wwmath/matrix4.h"
#include <cstdint>
#include <cstring>
#include <d3d8.h>
#include <new>

namespace ww3d_vulkan {

bool VB_Create(DX8VertexBufferClass *vb, size_t size)
{
	if (vb == nullptr || size == 0) {
		return false;
	}
	VB_Destroy(vb);
	GpuBuffer *gpu = new (std::nothrow) GpuBuffer();
	if (gpu == nullptr || !gpu->Create(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)) {
		delete gpu;
		return false;
	}
	vb->Set_Vulkan_Buffer(gpu);
	return true;
}

void VB_Destroy(DX8VertexBufferClass *vb)
{
	if (vb == nullptr) {
		return;
	}
	GpuBuffer *gpu = static_cast<GpuBuffer *>(vb->Peek_Vulkan_Buffer());
	if (gpu != nullptr) {
		delete gpu;
		vb->Set_Vulkan_Buffer(nullptr);
	}
}

unsigned char *VB_Lock(DX8VertexBufferClass *vb, size_t offset, size_t size)
{
	GpuBuffer *gpu = static_cast<GpuBuffer *>(vb->Peek_Vulkan_Buffer());
	if (gpu == nullptr) {
		return nullptr;
	}
	return gpu->Lock(offset, size);
}

void VB_Unlock(DX8VertexBufferClass *vb)
{
	GpuBuffer *gpu = static_cast<GpuBuffer *>(vb->Peek_Vulkan_Buffer());
	if (gpu != nullptr) {
		gpu->Unlock();
	}
}

VkBuffer VB_Handle(DX8VertexBufferClass *vb)
{
	GpuBuffer *gpu = static_cast<GpuBuffer *>(vb->Peek_Vulkan_Buffer());
	return gpu != nullptr ? gpu->Handle() : VK_NULL_HANDLE;
}

bool IB_Create(DX8IndexBufferClass *ib, size_t size)
{
	if (ib == nullptr || size == 0) {
		return false;
	}
	IB_Destroy(ib);
	GpuBuffer *gpu = new (std::nothrow) GpuBuffer();
	if (gpu == nullptr || !gpu->Create(size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT)) {
		delete gpu;
		return false;
	}
	ib->Set_Vulkan_Buffer(gpu);
	return true;
}

void IB_Destroy(DX8IndexBufferClass *ib)
{
	if (ib == nullptr) {
		return;
	}
	GpuBuffer *gpu = static_cast<GpuBuffer *>(ib->Peek_Vulkan_Buffer());
	if (gpu != nullptr) {
		delete gpu;
		ib->Set_Vulkan_Buffer(nullptr);
	}
}

unsigned char *IB_Lock(DX8IndexBufferClass *ib, size_t offset, size_t size)
{
	GpuBuffer *gpu = static_cast<GpuBuffer *>(ib->Peek_Vulkan_Buffer());
	if (gpu == nullptr) {
		return nullptr;
	}
	return gpu->Lock(offset, size);
}

void IB_Unlock(DX8IndexBufferClass *ib)
{
	GpuBuffer *gpu = static_cast<GpuBuffer *>(ib->Peek_Vulkan_Buffer());
	if (gpu != nullptr) {
		gpu->Unlock();
	}
}

VkBuffer IB_Handle(DX8IndexBufferClass *ib)
{
	GpuBuffer *gpu = static_cast<GpuBuffer *>(ib->Peek_Vulkan_Buffer());
	return gpu != nullptr ? gpu->Handle() : VK_NULL_HANDLE;
}

static void Matrix_To_Float16(const Matrix4 &m, float out[16])
{
	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col < 4; ++col) {
			out[row * 4 + col] = m[row][col];
		}
	}
}

uint32_t Get_Dynamic_Vb_Frame_Slot()
{
	if (!Is_Enabled() || !WW3DVulkan::Get().Is_Active()) {
		return 0;
	}
	return WW3DVulkan::Get().Renderer().Current_Frame();
}

void Reset_Dynamic_Vb_Frame_Slot(uint32_t slot)
{
	DynamicVBAccessClass::_Reset_Vulkan_Frame_Slot(slot);
}

void Reset_Dynamic_Ib_Frame_Slot(uint32_t slot)
{
	DynamicIBAccessClass::_Reset_Vulkan_Frame_Slot(slot);
}

void Sync_Matrices(const Matrix4 &world, const Matrix4 &view, const Matrix4 &projection)
{
	if (!Is_Enabled() || !WW3DVulkan::Get().Is_Active()) {
		return;
	}
	/*
	 * DX8Wrapper::Get_Transform returns the logical matrix (as passed to Set_Transform).
	 * Set_Transform stores m.Transpose() in render_state / ProjectionMatrix — that
	 * transposed form is what D3D8 multiplies (row-vector: v * M).
	 * Shader uses the same convention: pos * ubo.world * ubo.view_proj.
	 */
	Matrix4 world_m = world.Transpose();
	Matrix4 view_m = view.Transpose();
	Matrix4 proj_m = projection.Transpose();
	float world_f[16];
	float view_f[16];
	float vp_f[16];
	Matrix_To_Float16(world_m, world_f);
	Matrix_To_Float16(view_m, view_f);
	Matrix4 view_proj = view_m * proj_m;
	Matrix_To_Float16(view_proj, vp_f);
	WW3DVulkan::Get().Set_World_Matrix(world_f);
	WW3DVulkan::Get().Set_View_Matrix(view_f);
	WW3DVulkan::Get().Set_View_Projection(vp_f);
}

static unsigned Fvf_Vertex_Stride(unsigned fvf)
{
	unsigned size = FVFInfoClass(fvf).Get_FVF_Size();
	if (size > 0) {
		return size;
	}

	size = 0;
	const unsigned pos = fvf & D3DFVF_POSITION_MASK;
	if (pos == D3DFVF_XYZRHW) {
		size += 16;
	} else if (pos >= D3DFVF_XYZ && pos <= D3DFVF_XYZB5) {
		size += 12;
		unsigned blend_count = 0;
		if (pos >= D3DFVF_XYZB1 && pos <= D3DFVF_XYZB5) {
			blend_count = ((pos - D3DFVF_XYZB1) / 2) + 1;
		}
		size += blend_count * sizeof(float);
	}
	if (fvf & D3DFVF_NORMAL) {
		size += 12;
	}
	if (fvf & D3DFVF_DIFFUSE) {
		size += 4;
	}
	if (fvf & D3DFVF_SPECULAR) {
		size += 4;
	}
	const unsigned tex_count =
		(fvf & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
	size += tex_count * 8;
	return size;
}

static unsigned Fvf_Diffuse_Offset(unsigned fvf)
{
	unsigned offset = 0;
	const unsigned pos = fvf & D3DFVF_POSITION_MASK;
	if (pos == D3DFVF_XYZRHW) {
		offset += 16;
	} else if (pos >= D3DFVF_XYZ && pos <= D3DFVF_XYZB5) {
		offset += 12;
		if (pos >= D3DFVF_XYZB1 && pos <= D3DFVF_XYZB5) {
			const unsigned blend_count = ((pos - D3DFVF_XYZB1) / 2) + 1;
			offset += blend_count * sizeof(float);
		}
	}
	if (fvf & D3DFVF_NORMAL) {
		offset += 12;
	}
	return offset;
}

static bool Is_Supported_Fvf(unsigned fvf)
{
	const unsigned pos = fvf & D3DFVF_POSITION_MASK;
	if (pos != D3DFVF_XYZ && pos != D3DFVF_XYZRHW &&
		pos != D3DFVF_XYZB1 && pos != D3DFVF_XYZB2 && pos != D3DFVF_XYZB3) {
		return false;
	}
	if ((fvf & D3DFVF_LASTBETA_UBYTE4) != 0) {
		return false;
	}
	const unsigned tex_count =
		(fvf & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
	if (tex_count > 2) {
		return false;
	}
	return Fvf_Vertex_Stride(fvf) > 0;
}

static uint16_t Vertex_Stride_From_Fvf(unsigned fvf)
{
	if (!Is_Supported_Fvf(fvf)) {
		return 0;
	}
	unsigned stride = Fvf_Vertex_Stride(fvf);
	return stride > 0xFFFF ? 0 : (uint16_t)stride;
}

MeshPipelineKey Pipeline_Key_From_Shader(const ShaderClass &shader, unsigned fvf)
{
	MeshPipelineKey key;
	key.fvf = fvf;
	key.vertex_stride = Vertex_Stride_From_Fvf(fvf);
	key.src_blend = (uint8_t)shader.Get_Src_Blend_Func();
	key.dst_blend = (uint8_t)shader.Get_Dst_Blend_Func();
	key.alpha_blend =
		key.src_blend != (uint8_t)ShaderClass::SRCBLEND_ONE ||
		key.dst_blend != (uint8_t)ShaderClass::DSTBLEND_ZERO;
	if (key.alpha_blend &&
		shader.Get_Depth_Mask() == ShaderClass::DEPTH_WRITE_DISABLE) {
		key.depth_write = false;
	} else {
		key.depth_write = shader.Get_Depth_Mask() == ShaderClass::DEPTH_WRITE_ENABLE;
	}
	key.depth_compare = (uint8_t)shader.Get_Depth_Compare();
	key.two_sided = shader.Get_Cull_Mode() == ShaderClass::CULL_MODE_DISABLE;
	key.cull_inverted = ShaderClass::Is_Backface_Culling_Inverted();
	if (shader.Get_Depth_Mask() == ShaderClass::DEPTH_WRITE_DISABLE &&
		shader.Get_Depth_Compare() == ShaderClass::PASS_ALWAYS)
	{
		/* Sky / 2D overlays: never reject on depth, draw both sides. */
		key.depth_test = false;
		key.two_sided = true;
	} else {
		key.depth_test = shader.Get_Depth_Compare() != ShaderClass::PASS_NEVER;
	}
	key.alpha_test = shader.Get_Alpha_Test() == ShaderClass::ALPHATEST_ENABLE;
	return key;
}

static unsigned Index_Count(unsigned primitive_type, unsigned short polygon_count)
{
	switch (primitive_type) {
	case D3DPT_TRIANGLELIST:
		return polygon_count * 3;
	case D3DPT_TRIANGLESTRIP:
		return polygon_count + 2;
	case D3DPT_TRIANGLEFAN:
		return polygon_count + 2;
	default:
		return 0;
	}
}

static uint8_t Topology_From_Primitive(unsigned primitive_type)
{
	switch (primitive_type) {
	case D3DPT_TRIANGLESTRIP:
		return 1;
	default:
		return 0;
	}
}

bool Try_Draw_Indexed(
	unsigned primitive_type,
	unsigned short start_index,
	unsigned short polygon_count,
	unsigned short min_vertex_index,
	unsigned short vertex_count,
	const ShaderClass &shader,
	DX8VertexBufferClass *vb,
	DX8IndexBufferClass *ib,
	unsigned fvf)
{
	(void)vertex_count;
	if (!Is_Enabled() || !WW3DVulkan::Get().Is_Active()) {
		return false;
	}
	if (vb == nullptr || ib == nullptr) {
		return false;
	}
	if (primitive_type != D3DPT_TRIANGLELIST &&
		primitive_type != D3DPT_TRIANGLESTRIP) {
		return false;
	}
	uint16_t stride = Vertex_Stride_From_Fvf(fvf);
	if (stride == 0) {
		return false;
	}

	VkBuffer vkb = VB_Handle(vb);
	VkBuffer ikb = IB_Handle(ib);
	if (vkb == VK_NULL_HANDLE || ikb == VK_NULL_HANDLE) {
		return false;
	}

	unsigned index_count = Index_Count(primitive_type, polygon_count);
	if (index_count == 0) {
		return false;
	}

	MeshPipelineKey key = Pipeline_Key_From_Shader(shader, fvf);
	key.topology = Topology_From_Primitive(primitive_type);

	WW3DVulkan::Get().Draw_Indexed(
		vkb,
		ikb,
		index_count,
		start_index,
		min_vertex_index,
		key);
	return true;
}

} /* namespace ww3d_vulkan */

#endif /* RENEGADE_VULKAN */
