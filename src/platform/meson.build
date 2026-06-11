platform_sources = files(
  'sdl3_host.cpp',
  'linux_entry.cpp',
  'linux/pe_resource_loader.cpp',
  'linux/win32_stubs.cpp',
  'linux/win32_extra_stubs.cpp',
  'linux/linux_registry_ini.cpp',
  'linux/linux_sync.cpp',
  'linux/winsock_stubs.cpp',
  'linux/vfw_stubs.cpp',
  'linux/ime_stubs.cpp',
)

platform_inc = include_directories('.', 'linux')

platform_lib = static_library(
  'platform',
  platform_sources,
  include_directories : platform_inc,
  cpp_args : renegade_platform_cpp_args,
  dependencies : [sdl3_dep],
  install : false,
)

platform_dep = declare_dependency(
  link_with : platform_lib,
  include_directories : platform_inc,
  compile_args : renegade_platform_cpp_args,
  dependencies : [sdl3_dep, linux_graphics_dep],
)
