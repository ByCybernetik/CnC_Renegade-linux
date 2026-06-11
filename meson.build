# C&C Renegade — Windows (MinGW) or Linux (SDL3 + dxvk-native).
#
#   ./meson-setup-mingw.sh build-mingw
#   ./meson-setup-linux.sh build-linux

project('cnc-renegade', ['c', 'cpp'],
  version : '1.0',
  meson_version : '>= 0.56',
  default_options : [
    'cpp_std=gnu++98',
    'buildtype=release',
    'warning_level=1',
  ],
)

_platform = get_option('platform')
_is_linux = _platform == 'linux'
if get_option('boot_log')
  add_global_arguments('-DRENEGADE_BOOT_LOG', language : ['c', 'cpp'])
  add_global_arguments(
    '-I' + meson.project_source_root() / 'src/platform',
    language : 'cpp',
  )
endif
if get_option('init_log')
  add_global_arguments('-DRENEGADE_INIT_LOG', language : ['c', 'cpp'])
endif
if get_option('nav_log')
  add_global_arguments('-DRENEGADE_NAV_LOG', language : ['c', 'cpp'])
  add_global_arguments(
    '-I' + meson.project_source_root() / 'src/platform',
    language : 'cpp',
  )
endif

if _is_linux
  renegade_cpp_args = [
    '-std=gnu++11',
    '-DRENEGADE_LINUX',
    '-D_UNIX',
    '-DNDEBUG',
    '-fpermissive',
    '-fshort-wchar',
  ]
  add_global_arguments(
    '-I' + meson.project_source_root() / 'src/platform',
    language : 'cpp',
  )
  renegade_link_args = []
  renegade_exe_link_tail = []
  renegade_wwlib_cpp_args = renegade_cpp_args + [
    '-DREGEX_MALLOC',
    '-DSTDC_HEADERS',
  ]
  renegade_platform_cpp_args = renegade_cpp_args
  if get_option('graphics_backend') == 'vulkan'
    add_global_arguments('-DRENEGADE_VULKAN', language : 'cpp')
    renegade_platform_cpp_args += '-DRENEGADE_VULKAN'
  endif
  _linux_shim_inc = meson.project_source_root() / 'src/platform/linux'
  add_global_arguments('-I' + _linux_shim_inc, language : 'cpp')

  subdir('deps/linux')
  _graphics_backend = get_option('graphics_backend')
  if _graphics_backend == 'vulkan'
    subdir('src/ww3d2_vulkan')
  endif
  subdir('src/platform')
else
  renegade_cpp_args = [
    '-std=gnu++98',
    '-DWINVER=0x0400',
    '-D_WIN32_WINNT=0x0400',
    '-DWIN32',
    '-D_WINDOWS',
    '-DWIN32_LEAN_AND_MEAN',
    '-DNDEBUG',
    '-fpermissive',
    '-D_int64=long long',
    '-DPTW32_STATIC_LIB',
    '-DRENEGADE_PORT_COLLISION_FIX',
  ]
  renegade_link_args = [
    '-mcrtdll=msvcrt-os',
    '-static-libgcc',
    '-static-libstdc++',
  ]
  # Static winpthread: Code/wwlib/mingw-g++-static.sh (-L mingw-lib + mingw-pthread-static.specs).
  renegade_exe_link_tail = []
  renegade_wwlib_cpp_args = renegade_cpp_args + [
    '-DREGEX_MALLOC',
    '-DSTDC_HEADERS',
  ]
  renegade_platform_cpp_args = []

  subdir('deps/dxsdk8')
  add_global_arguments(dxsdk8_compile_args, language : 'cpp')

  _ww_backend = get_option('wwaudio_backend')
  if _ww_backend == 'mss'
    add_global_arguments('-DWWAUDIO_USE_MSS', language : 'cpp')
    subdir('deps/miles')
  elif _ww_backend == 'faudio'
    subdir('deps/faudio')
    if get_option('wwaudio_mpg123')
      subdir('deps/mpg123')
    endif
  elif _ww_backend == 'openal'
    subdir('deps/openal-dll')
    if get_option('wwaudio_mpg123')
      subdir('deps/mpg123')
    endif
  endif
endif

if _is_linux
  if get_option('wwaudio_backend') != 'sdl3'
    warning('Linux port expects -Dwwaudio_backend=sdl3; overriding')
  endif
  if get_option('wwaudio_mpg123')
    subdir('deps/mpg123')
  endif
endif

subdir('src/wwlib')
subdir('src/wwdebug')
subdir('src/wwsaveload')
subdir('src/wwmath')
subdir('src/wwutil')
subdir('src/wwbitpack')
subdir('src/wwtranslatedb')
subdir('src/wwnet')
subdir('src/ww3d2')
subdir('src/wwaudio')
subdir('src/wwphys')
subdir('src/binkmovie')
subdir('src/scontrol')
subdir('src/wwui')
subdir('src/combat')
subdir('src/scripts')
subdir('src/commando')
if _is_linux and get_option('graphics_backend') == 'vulkan'
  subdir('src/tools/texview')
  subdir('src/tools/w3dviewer')
  subdir('src/tools/w3dview')
  subdir('src/tools/lvlview')
endif

summary('platform', _platform, section : 'Configuration')
summary('executable', meson.project_build_root() / renegade_exe.name(), section : 'Targets')
summary('scripts_dll', meson.project_build_root() / scripts_dll.name(), section : 'Targets')
summary('wwlib', wwlib_lib.full_path(), section : 'Static libraries')
summary('wwdebug', wwdebug_lib.full_path(), section : 'Static libraries')
summary('wwsaveload', wwsaveload_lib.full_path(), section : 'Static libraries')
summary('wwmath', wwmath_lib.full_path(), section : 'Static libraries')
summary('wwutil', wwutil_lib.full_path(), section : 'Static libraries')
summary('wwbitpack', wwbitpack_lib.full_path(), section : 'Static libraries')
summary('wwtranslatedb', wwtranslatedb_lib.full_path(), section : 'Static libraries')
summary('wwnet', wwnet_lib.full_path(), section : 'Static libraries')
summary('ww3d2', ww3d2_lib.full_path(), section : 'Static libraries')
summary('wwaudio', wwaudio_lib.full_path(), section : 'Static libraries')
if not _is_linux and get_option('wwaudio_backend') == 'faudio' and get_option('wwaudio_mpg123')
  summary('mp3_faudio_player', mp3_faudio_player.full_path(), section : 'Targets')
endif
summary('wwphys', wwphys_lib.full_path(), section : 'Static libraries')
summary('binkmovie', binkmovie_lib.full_path(), section : 'Static libraries')
summary('scontrol', scontrol_lib.full_path(), section : 'Static libraries')
summary('wwui', wwui_lib.full_path(), section : 'Static libraries')
summary('combat', combat_lib.full_path(), section : 'Static libraries')
