/* Minimal D3DX8 shim for the native Vulkan backend.
 * Replaces <d3dx8.h> in dx8wrapper.h when RENEGADE_VULKAN is defined.
 * Use the vendored DXSDK8 D3DX8 headers instead of the full dxvk-native
 * dependency.  The D3DX functions are not called at runtime under Vulkan.
 */
#ifndef RENEGADE_D3DX8_VULKAN_H
#define RENEGADE_D3DX8_VULKAN_H

#include <d3dx8.h>

#endif /* RENEGADE_D3DX8_VULKAN_H */
