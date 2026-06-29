wwaudio_sources = files(
  'audiblesound.cpp',
  'audioevents.cpp',
  'audiosaveload.cpp',
  'filteredsound.cpp',
  'listener.cpp',
  'listenerhandle.cpp',
  'logicallistener.cpp',
  'logicalsound.cpp',
  'sound2dhandle.cpp',
  'sound3d.cpp',
  'sound3dhandle.cpp',
  'soundbuffer.cpp',
  'soundhandle.cpp',
  'soundpseudo3d.cpp',
  'soundscene.cpp',
  'soundsceneobj.cpp',
  'soundstreamhandle.cpp',
  'threads.cpp',
  'utils.cpp',
  'wwaudio.cpp',
)

inc = include_directories('.', '../wwlib', '../wwlib/mingw', '../wwdebug', '../wwmath', '../wwsaveload', '../ww3d2')

_cpp = meson.project_name() == 'cnc-renegade' ? renegade_cpp_args : [
  '-DWINVER=0x0400', '-D_WIN32_WINNT=0x0400', '-DWIN32', '-D_WINDOWS',
  '-DWIN32_LEAN_AND_MEAN', '-DNDEBUG', '-fpermissive',
]
_link = meson.project_name() == 'cnc-renegade' ? renegade_link_args : ['-mcrtdll=msvcrt-os', '-static-libgcc', '-static-libstdc++', '-Wl,-Bstatic', '-lwinpthread', '-Wl,-Bdynamic']

wwaudio_deps = []
_backend = get_option('wwaudio_backend')

if _backend == 'mss'
  if get_option('platform') == 'linux'
    _cpp += ['-DRENEGADE_LINUX', '-DWWAUDIO_USE_MSS']
    wwaudio_sources += files('sdl3_audio.cpp')
    avcodec_dep = dependency('libavcodec', required : true)
    avformat_dep = dependency('libavformat', required : true)
    avutil_dep = dependency('libavutil', required : true)
    swresample_dep = dependency('libswresample', required : true)
    wwaudio_deps += [sdl3_dep, avcodec_dep, avformat_dep, avutil_dep, swresample_dep]
    message('WWAudio: SDL3 + FFmpeg (Linux)')
  else
    miles_dep = dependency('miles', required : true)
    _cpp += ['-DWWAUDIO_USE_MSS']
    wwaudio_deps += [miles_dep]
    message('WWAudio: Miles Sound System 9.3 (mss32.dll)')
  endif
elif get_option('disable_audio')
  message('WWAudio: audio disabled')
else
  error('WWAudio: only MSS backend is currently supported')
endif

wwaudio_lib = static_library(
  'wwaudio',
  wwaudio_sources,
  include_directories : inc,
  cpp_args : _cpp,
  link_args : _link,
  dependencies : wwaudio_deps,
  install : false,
)

# ---- Standalone test player ----
if get_option('platform') == 'linux'
  _test_inc = include_directories(
    '.',
    '../wwlib', '../wwlib/mingw', '../wwdebug', '../wwmath',
    '../wwsaveload', '../ww3d2',
    '../platform', '../platform/linux',
  )

  _test_cpp = renegade_cpp_args + [
    '-DRENEGADE_LINUX',
    '-DRENEGADE_VULKAN',
    '-include', meson.project_source_root() / 'src/platform/linux/dxvk_compat.h',
  ]

  test_wwaudio_exe = executable(
    'test_wwaudio',
    files('test_wwaudio.cpp', 'test_wwaudio_stubs.cpp'),
    include_directories : _test_inc,
    cpp_args : _test_cpp,
    link_args : renegade_link_args,
    link_with : [
      wwaudio_lib,
      wwlib_lib,
      wwdebug_lib,
      wwmath_lib,
      wwsaveload_lib,
      ww3d2_lib,
      platform_lib,
    ],
    dependencies : wwaudio_deps,
    install : false,
  )
  alias_target('test_wwaudio', test_wwaudio_exe)
endif
