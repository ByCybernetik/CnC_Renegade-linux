if get_option('platform') != 'linux' or get_option('graphics_backend') != 'vulkan'
  subdir_done()
endif
if get_option('vulkan_renderer') == 'engine'
  subdir_done()
endif

_linux_dxvk_compat = meson.project_source_root() / 'src/platform/linux/dxvk_compat.h'

_w3dview_inc = include_directories(
  '.',
  '../../ww3d2',
  '../../ww3d2_vulkan',
  '../../platform',
  '../../platform/linux',
  '../../wwlib',
  '../../wwmath',
  '../../wwdebug',
  '../../wwsaveload',
  '../../../third_party/vma/include',
)

_w3dview_cpp = renegade_cpp_args + [
  '-std=gnu++11',
  '-DRENEGADE_LINUX',
  '-DRENEGADE_VULKAN',
  '-DRENEGADE_WW3D2_BUILD',
  '-include', _linux_dxvk_compat,
]

w3dview_exe = executable(
  'w3dview',
  files(
    'main.cpp',
    'stubs.cpp',
  ),
  include_directories : _w3dview_inc,
  cpp_args : _w3dview_cpp,
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

alias_target('w3dview', w3dview_exe)
