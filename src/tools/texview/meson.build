if get_option('platform') != 'linux' or get_option('graphics_backend') != 'vulkan'
  subdir_done()
endif
if get_option('vulkan_renderer') == 'engine'
  subdir_done()
endif

glslc = find_program('glslc', required : true)

_shader_src = meson.current_source_dir() / 'shaders'
_shader_build = meson.current_build_dir()
_shader_deploy = meson.project_build_root() / 'shaders' / 'texview'

blit_vert_spv = custom_target(
  'texview_blit_vert_spv',
  input : 'shaders/blit.vert',
  output : 'blit.vert.spv',
  command : [glslc, '-fshader-stage=vertex', '-I', _shader_src, '@INPUT@', '-o', '@OUTPUT@'],
)

blit_frag_spv = custom_target(
  'texview_blit_frag_spv',
  input : 'shaders/blit.frag',
  output : 'blit.frag.spv',
  command : [glslc, '-fshader-stage=fragment', '-I', _shader_src, '@INPUT@', '-o', '@OUTPUT@'],
)

shader_deploy = custom_target(
  'deploy_texview_shaders',
  input : [blit_vert_spv, blit_frag_spv],
  output : 'texview_shader_deploy.stamp',
  command : [
    find_program('bash'), '-c',
    'mkdir -p "@0@" && cp "@INPUT0@" "@0@/blit.vert.spv" && cp "@INPUT1@" "@0@/blit.frag.spv" && touch "@OUTPUT@"'.format(_shader_deploy),
  ],
  build_by_default : true,
)

_linux_dxvk_compat = meson.project_source_root() / 'src/platform/linux/dxvk_compat.h'

_texview_inc = include_directories(
  '.',
  '../../ww3d2',
  '../../ww3d2_vulkan',
  '../../platform',
  '../../platform/linux',
  '../../wwlib',
  '../../wwmath',
  '../../wwdebug',
  '../../../third_party/vma/include',
)

_texview_cpp = renegade_cpp_args + [
  '-std=gnu++11',
  '-DRENEGADE_LINUX',
  '-DRENEGADE_VULKAN',
  '-DRENEGADE_WW3D2_BUILD',
  '-include', _linux_dxvk_compat,
  '-DRENEGADE_TEXVIEW_SHADER_DIR="@0@"'.format(_shader_build),
]

texview_exe = executable(
  'texview',
  files(
    'main.cpp',
    'texview_loader.cpp',
    'texview_renderer.cpp',
    'texview_stubs.cpp',
  ) + [shader_deploy],
  include_directories : _texview_inc,
  cpp_args : _texview_cpp,
  link_args : renegade_link_args,
  link_with : [
    ww3d2_vulkan_lib,
    ww3d2_lib,
    wwmath_lib,
    wwlib_lib,
    wwdebug_lib,
    wwsaveload_lib,
    platform_lib,
  ],
  dependencies : [sdl3_dep, vulkan_dep, linux_graphics_dep],
  override_options : ['cpp_std=gnu++11'],
)
