wwnet_sources = files(
  'mingw/wol_nat_stub.cpp',
  'BWBalance.cpp',
  'connect.cpp',
  'msgstat.cpp',
  'msgstatlist.cpp',
  'msgstatlistgroup.cpp',
  'netstats.cpp',
  'netutil.cpp',
  'networkobject.cpp',
  'networkobjectfactory.cpp',
  'networkobjectfactorymgr.cpp',
  'networkobjectmgr.cpp',
  'packetmgr.cpp',
  'rhost.cpp',
  'singlepl.cpp',
  'wwpacket.cpp',
)

inc = include_directories('.', '../wwlib', '../wwdebug', '../wwmath', '../wwbitpack', '../wwutil')

_cpp = meson.project_name() == 'cnc-renegade' ? renegade_cpp_args : [
  '-DWINVER=0x0400', '-D_WIN32_WINNT=0x0400', '-DWIN32', '-D_WINDOWS',
  '-DWIN32_LEAN_AND_MEAN', '-DNDEBUG', '-fpermissive',
]
_link = meson.project_name() == 'cnc-renegade' ? renegade_link_args : ['-mcrtdll=msvcrt-os', '-static-libgcc', '-static-libstdc++', '-Wl,-Bstatic', '-lwinpthread', '-Wl,-Bdynamic']

wwnet_lib = static_library(
  'wwnet',
  wwnet_sources,
  include_directories : inc,
  cpp_args : _cpp,
  link_args : _link,
)
