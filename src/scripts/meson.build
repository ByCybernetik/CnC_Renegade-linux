# Mission script DLL (must match renegade.exe toolchain / ABI).

inc = include_directories(
  '../wwlib',
  '../wwlib/mingw',
  '../wwmath',
  '../wwdebug',
  '../',
)

_cpp = renegade_cpp_args + [
  '-DBUILDING_DLL',
  '-DNDEBUG',
  '-fms-extensions',
]

_is_linux = get_option('platform') == 'linux'
_scripts_win_link = _is_linux ? [] : [
  '-lkernel32', '-luser32', '-lgdi32', '-lwinmm', '-lws2_32',
]
_scripts_link_tail = _is_linux ? [] : [
  '-Wl,--enable-stdcall-fixup',
  '-Wl,--defsym=__ms_vsnprintf=___ms_vsnprintf',
]
_link = renegade_link_args + _scripts_win_link + _scripts_link_tail

_scripts_extra_sources = _is_linux ? files('linux_dll_stubs.cpp') : []

scripts_sources = files(
  'Common.cpp',
  'DLLmain.cpp',
  'DPrint.cpp',
  'DrMobius.cpp',
  'Group.cpp',
  'GroupControl.cpp',
  'GroupScript.cpp',
  'Mission00.cpp',
  'Mission01.cpp',
  'Mission02.cpp',
  'Mission03.cpp',
  'Mission04.cpp',
  'Mission05.cpp',
  'Mission06.cpp',
  'Mission07.cpp',
  'mission08.cpp',
  'Mission09.cpp',
  'Mission10.cpp',
  'Mission11.cpp',
  'MissionDemo.cpp',
  'MissionS04.cpp',
  'MissionX0.cpp',
  'PRDemo.cpp',
  'ScriptFactory.cpp',
  'ScriptRegistrar.cpp',
  'Test_BMG.cpp',
  'Test_Cinematic.cpp',
  'Test_DAK.cpp',
  'Test_DAY.cpp',
  'Test_DLS.cpp',
  'Test_DME.cpp',
  'Test_GTH.cpp',
  'Test_JDG_EVA.cpp',
  'Test_PDS.cpp',
  'Test_RAD.cpp',
  'Test_RMV.cpp',
  'Test_RMV_Toolkit.cpp',
  'Toolkit.cpp',
  'Toolkit_Actions.cpp',
  'Toolkit_Animations.cpp',
  'Toolkit_Broadcaster.cpp',
  'Toolkit_Explosions.cpp',
  'Toolkit_Objectives.cpp',
  'Toolkit_Objects.cpp',
  'Toolkit_Powerup.cpp',
  'Toolkit_Siege.cpp',
  'Toolkit_Sounds.cpp',
  'Toolkit_Spawners.cpp',
  'Toolkit_Triggers.cpp',
  'scripts.cpp',
  'scriptcommands_proxy.cpp',
  'strtrim.cpp',
) + _scripts_extra_sources

_scripts_link_with = [wwlib_lib]
if _is_linux
  _scripts_link_with += [platform_lib]
endif

scripts_dll = shared_library(
  'scripts',
  scripts_sources,
  include_directories : inc,
  cpp_args : _cpp,
  link_args : _link,
  link_with : _scripts_link_with,
  name_prefix : '',
  install : false,
)
