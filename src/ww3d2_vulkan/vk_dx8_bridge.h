#ifndef WW3D2_VULKAN_VK_DX8_BRIDGE_H
#define WW3D2_VULKAN_VK_DX8_BRIDGE_H

#if defined(RENEGADE_VULKAN)

#include "vk_pipeline.h"
#include <vulkan/vulkan.h>

class DX8VertexBufferClass;
class DX8IndexBufferClass;
class ShaderClass;
struct Matrix4;

namespace ww3d_vulkan {

bool VB_Create(DX8VertexBufferClass *vb, size_t size);
void VB_Destroy(DX8VertexBufferClass *vb);
unsigned char *VB_Lock(DX8VertexBufferClass *vb, size_t offset, size_t size);
void VB_Unlock(DX8VertexBufferClass *vb);
VkBuffer VB_Handle(DX8VertexBufferClass *vb);

bool IB_Create(DX8IndexBufferClass *ib, size_t size);
void IB_Destroy(DX8IndexBufferClass *ib);
unsigned char *IB_Lock(DX8IndexBufferClass *ib, size_t offset, size_t size);
void IB_Unlock(DX8IndexBufferClass *ib);
VkBuffer IB_Handle(DX8IndexBufferClass *ib);

void Sync_Matrices(const Matrix4 &world, const Matrix4 &view, const Matrix4 &projection);
uint32_t Get_Dynamic_Vb_Frame_Slot();
void Reset_Dynamic_Vb_Frame_Slot(uint32_t slot);
void Reset_Dynamic_Ib_Frame_Slot(uint32_t slot);
MeshPipelineKey Pipeline_Key_From_Shader(const ShaderClass &shader, unsigned fvf);

bool Try_Draw_Indexed(
	unsigned primitive_type,
	unsigned short start_index,
	unsigned short polygon_count,
	unsigned short min_vertex_index,
	unsigned short vertex_count,
	const ShaderClass &shader,
	DX8VertexBufferClass *vb,
	DX8IndexBufferClass *ib,
	unsigned fvf);

} /* namespace ww3d_vulkan */

#endif /* RENEGADE_VULKAN */

#endif
