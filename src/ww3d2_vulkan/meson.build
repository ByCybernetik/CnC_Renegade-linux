glslc = find_program('glslc', required : true)

_shader_src_dir = meson.current_source_dir() / 'shaders'
_shader_build_dir = meson.current_build_dir()
_shader_deploy_dir = meson.project_build_root() / 'shaders'

_glslc_inc = ['-I', _shader_src_dir]

mesh_vert_spv = custom_target(
  'ww3d_vulkan_mesh_vert_spv',
  input : 'shaders/mesh_textured.vert',
  output : 'mesh_textured.vert.spv',
  command : [glslc, '-fshader-stage=vertex'] + _glslc_inc + ['@INPUT@', '-o', '@OUTPUT@'],
  build_by_default : true,
)

mesh_frag_spv = custom_target(
  'ww3d_vulkan_mesh_frag_spv',
  input : 'shaders/mesh_textured.frag',
  output : 'mesh_textured.frag.spv',
  command : [glslc, '-fshader-stage=fragment'] + _glslc_inc + ['@INPUT@', '-o', '@OUTPUT@'],
  build_by_default : true,
)

shader_deploy_stamp = custom_target(
  'deploy_vulkan_shaders',
  input : [mesh_vert_spv, mesh_frag_spv],
  output : 'shader_deploy.stamp',
  command : [
    find_program('bash'), '-c',
    'mkdir -p "@0@" && cp "@INPUT0@" "@0@/mesh_textured.vert.spv" && cp "@INPUT1@" "@0@/mesh_textured.frag.spv" && touch "@OUTPUT@"'.format(_shader_deploy_dir),
  ],
  build_by_default : true,
)

ww3d2_vulkan_sources = files(
  'vulkan_render_device.cpp',
  'vk_native_render_state.cpp',
  'stb_texture.cpp',
  'vk_allocator.cpp',
  'vk_buffer.cpp',
  'vk_context.cpp',
  'vk_dx8_bridge.cpp',
  'vk_dx8_state.cpp',
  'vk_dx8_texture.cpp',
  'vk_cpu_surface.cpp',
  'vk_format.cpp',
  'vk_framebuffer.cpp',
  'vk_gpu_buffer.cpp',
  'vk_pipeline.cpp',
  'vk_render_pass.cpp',
  'vk_renderer.cpp',
  'vk_shader.cpp',
  'vk_swapchain.cpp',
  'vk_staging_pool.cpp',
  'vk_texture.cpp',
  'vk_upload.cpp',
  'ww3d_vulkan.cpp',
)

ww3d2_vulkan_inc = include_directories(
  '.',
  '../platform',
  '../platform/linux',
  '../ww3d2',
  '../wwmath',
  '../wwlib',
  '../wwdebug',
  '../../third_party/stb',
  '../../third_party/vma/include',
)

ww3d2_vulkan_cpp_args = [
  '-std=gnu++11',
  '-DRENEGADE_LINUX',
  '-DRENEGADE_VULKAN',
  '-DRENEGADE_WW3D2_BUILD',
  '-DNDEBUG',
  '-fpermissive',
  '-include', 'dxvk_compat.h',
  '-DRENEGADE_VULKAN_SHADER_DIR="@0@"'.format(_shader_build_dir),
]

ww3d2_vulkan_lib = static_library(
  'ww3d2_vulkan',
  ww3d2_vulkan_sources + [mesh_vert_spv, mesh_frag_spv, shader_deploy_stamp],
  include_directories : ww3d2_vulkan_inc,
  cpp_args : ww3d2_vulkan_cpp_args,
  dependencies : [sdl3_dep, vulkan_dep],
  link_args : renegade_link_args,
  override_options : ['cpp_std=gnu++11'],
)

ww3d2_vulkan_dep = declare_dependency(
  link_with : ww3d2_vulkan_lib,
  include_directories : ww3d2_vulkan_inc,
  compile_args : ww3d2_vulkan_cpp_args,
  dependencies : [sdl3_dep, vulkan_dep],
)
