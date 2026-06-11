wwutil_sources = files(
  'mathutil.cpp',
  'miscutil.cpp',
)

if get_option('platform') == 'linux'
  wwutil_sources += files('linux/miscutil_linux.cpp', 'linux/stackdump_stub.cpp')
else
  wwutil_sources += files('stackdump.cpp')
endif

inc = include_directories('.', '../wwlib', '../wwdebug', '../wwmath')

_cpp = meson.project_name() == 'cnc-renegade' ? renegade_cpp_args : [
  '-DWINVER=0x0400', '-D_WIN32_WINNT=0x0400', '-DWIN32', '-D_WINDOWS',
  '-DWIN32_LEAN_AND_MEAN', '-DNDEBUG', '-fpermissive',
]
_link = meson.project_name() == 'cnc-renegade' ? renegade_link_args : ['-mcrtdll=msvcrt-os', '-static-libgcc', '-static-libstdc++', '-Wl,-Bstatic', '-lwinpthread', '-Wl,-Bdynamic']

wwutil_lib = static_library(
  'wwutil',
  wwutil_sources,
  include_directories : inc,
  cpp_args : _cpp,
  link_args : _link,
)
