wwbitpack_sources = files(
  'BitPacker.cpp',
  'bitstream.cpp',
  'encoderlist.cpp',
  'encodertypeentry.cpp',
)

inc = include_directories('.', '../wwlib', '../wwdebug', '../wwutil', '../wwmath')

_cpp = meson.project_name() == 'cnc-renegade' ? renegade_cpp_args : [
  '-DWINVER=0x0400', '-D_WIN32_WINNT=0x0400', '-DWIN32', '-D_WINDOWS',
  '-DWIN32_LEAN_AND_MEAN', '-DNDEBUG', '-fpermissive',
]
_link = meson.project_name() == 'cnc-renegade' ? renegade_link_args : ['-mcrtdll=msvcrt-os', '-static-libgcc', '-static-libstdc++', '-Wl,-Bstatic', '-lwinpthread', '-Wl,-Bdynamic']

wwbitpack_lib = static_library(
  'wwbitpack',
  wwbitpack_sources,
  include_directories : inc,
  cpp_args : _cpp,
  link_args : _link,
)
