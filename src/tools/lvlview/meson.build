if get_option('platform') != 'linux' or get_option('graphics_backend') != 'vulkan'
  subdir_done()
endif
_linux_dxvk_compat = meson.project_source_root() / 'src/platform/linux/dxvk_compat.h'

_lvlview_inc = include_directories(
  '.',
  '../../ww3d2',
  '../../ww3d2_vulkan',
  '../../platform',
  '../../platform/linux',
  '../../wwlib',
  '../../wwmath',
  '../../wwdebug',
  '../../wwsaveload',
  '../../wwphys',
  '../../combat',
  '../../wwui',
  '../../wwaudio',
  '../../wwutil',
  '../../wwbitpack',
  '../../wwnet',
  '../../wwtranslatedb',
  '../../binkmovie',
  '../../scontrol',
  '../../commando',
  '../../../third_party/vma/include',
)

_lvlview_cpp = renegade_cpp_args + [
  '-std=gnu++11',
  '-DRENEGADE_LINUX',
  '-DRENEGADE_VULKAN',
  '-DRENEGADE_WW3D2_BUILD',
  '-include', _linux_dxvk_compat,
]

lvlview_exe = executable(
  'lvlview',
  files(
    'main.cpp',
    'file_init.cpp',
    'stubs.cpp',
  ),
  include_directories : _lvlview_inc,
  cpp_args : _lvlview_cpp,
  link_args : renegade_link_args,
  link_with : [
    ww3d2_vulkan_lib,
    combat_lib,
    wwui_lib,
    wwaudio_lib,
    wwphys_lib,
    ww3d2_lib,
    wwnet_lib,
    wwtranslatedb_lib,
    wwmath_lib,
    wwutil_lib,
    wwbitpack_lib,
    binkmovie_lib,
    scontrol_lib,
    wwlib_lib,
    wwdebug_lib,
    wwsaveload_lib,
    platform_lib,
  ],
  dependencies : [sdl3_dep, vulkan_dep, linux_graphics_dep],
  override_options : ['cpp_std=gnu++11'],
)

alias_target('lvlview', lvlview_exe)
