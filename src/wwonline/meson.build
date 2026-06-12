wwonline_sources = files(
  'GameResField.cpp',
  'GameResPacket.cpp',
  'PingProfile.cpp',
  'WaitCondition.cpp',
  'WOLChannel.cpp',
  'WOLChatMsg.cpp',
  'WOLChatObserver.cpp',
  'WOLConnect.cpp',
  'WOLDownload.cpp',
  'WOLErrorUtil.cpp',
  'WOLGame.cpp',
  'WOLGameOptions.cpp',
  'WOLLadder.cpp',
  'WOLLoginInfo.cpp',
  'WOLNetUtilObserver.cpp',
  'WOLPageMsg.cpp',
  'WOLProduct.cpp',
  'WOLServer.cpp',
  'WOLSession.cpp',
  'WOLSquad.cpp',
  'WOLString.cpp',
  'WOLUser.cpp',
)

_wwonline_inc_dirs = [
  '.',
  '../commando',
  '../commando/mingw',
  '../commando/mingw/include',
  '../wwlib',
  '../wwlib/mingw',
  '../wwdebug',
  '../wwmath',
  '../wwsaveload',
  '../wwutil',
  '../wwbitpack',
  '../wwnet',
  '../wwaudio',
  '../wwaudio/mingw',
  '../wwaudio/linux',
  '../combat',
  '../../Code/wolapi',
]
if get_option('platform') == 'linux'
  _wwonline_inc_dirs += '../platform/linux'
endif
inc = include_directories(_wwonline_inc_dirs)

_cpp = meson.project_name() == 'cnc-renegade' ? renegade_cpp_args : [
  '-DWINVER=0x0400', '-D_WIN32_WINNT=0x0400', '-DWIN32', '-D_WINDOWS',
  '-DWIN32_LEAN_AND_MEAN', '-DNDEBUG', '-fpermissive',
]
_link = meson.project_name() == 'cnc-renegade' ? renegade_link_args : [
  '-mcrtdll=msvcrt-os', '-static-libgcc', '-static-libstdc++',
  '-Wl,-Bstatic', '-lwinpthread', '-Wl,-Bdynamic',
]

wwonline_lib = static_library(
  'wwonline',
  wwonline_sources,
  include_directories : inc,
  cpp_args : _cpp,
  link_args : _link,
)
