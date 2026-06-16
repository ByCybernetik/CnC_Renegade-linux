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

inc = include_directories('.', 'mingw', '../wwlib', '../wwlib/mingw', '../wwdebug', '../wwmath', '../wwsaveload', '../ww3d2')

_cpp = meson.project_name() == 'cnc-renegade' ? renegade_cpp_args : [
  '-DWINVER=0x0400', '-D_WIN32_WINNT=0x0400', '-DWIN32', '-D_WINDOWS',
  '-DWIN32_LEAN_AND_MEAN', '-DNDEBUG', '-fpermissive',
]
_link = meson.project_name() == 'cnc-renegade' ? renegade_link_args : ['-mcrtdll=msvcrt-os', '-static-libgcc', '-static-libstdc++', '-Wl,-Bstatic', '-lwinpthread', '-Wl,-Bdynamic']

wwaudio_deps = []
_backend = get_option('wwaudio_backend')
if get_option('platform') == 'linux'
  _backend = 'sdl3'
endif

if _backend == 'sdl3'
  wwaudio_sources += files(
    'linux/sdl3_mss.cpp',
    'linux/audio_music_bus.cpp',
    'linux/audio_sdl3_mixer.cpp',
    'linux/audio_sdl3_mixer_music_bus.cpp',
    'linux/audio_sdl3_mixer_ui_bus.cpp',
    'linux/audio_sdl3_mixer_weapon_bus.cpp',
    'linux/audio_sdl3_mixer_ambient_bus.cpp',
    'linux/audio_sdl3_mixer_dialog_bus.cpp',
    'linux/audio_decode.cpp',
    'linux/ffmpeg_decode.cpp',
    'linux/eax_reverb.cpp',
  )
  avcodec_dep = dependency('libavcodec', required : true)
  avformat_dep = dependency('libavformat', required : true)
  avutil_dep = dependency('libavutil', required : true)
  swresample_dep = dependency('libswresample', required : true)
  _cpp += ['-DWWAUDIO_HAVE_FFMPEG', '-DWWAUDIO_USE_SDL3']
  inc = [inc, include_directories('linux', '../platform', '../platform/linux')]
  wwaudio_deps += [sdl3_dep, avcodec_dep, avformat_dep, avutil_dep, swresample_dep]
  message('WWAudio: SDL3 + FFmpeg audio backend (Linux)')

elif _backend == 'mss'
  miles_dep = dependency('miles', required : true)
  wwaudio_sources += files(
    'mingw/miles_compat.cpp',
    'mingw/miles_file_io.cpp',
  )
  _cpp += ['-DWWAUDIO_USE_MSS']
  wwaudio_deps += [miles_dep]
  message('WWAudio: Miles Sound System 9.3 (mss32.dll)')

elif _backend == 'faudio'
  faudio_dep = dependency('faudio', required : true)
  _faudio_inc = include_directories('../../third_party/faudio/include')

  wwaudio_sources += files(
    'mingw/faudio_mss.cpp',
    'mingw/audio_decode.cpp',
    'mingw/mp3_decode_mpg123.cpp',
  )
  _cpp += ['-DWWAUDIO_USE_FAUDIO']
  inc = [inc, _faudio_inc]
  wwaudio_deps += [faudio_dep]

  if get_option('wwaudio_mpg123')
    mpg123_dep = dependency('libmpg123', required : true)
    _cpp += ['-DWWAUDIO_HAVE_MPG123']
    wwaudio_deps += [mpg123_dep]
    message('WWAudio: FAudio + mpg123 (MP3)')
  else
    warning('wwaudio_mpg123=false: MP3 will not decode (WAV/ADPCM still work)')
  endif
  message('WWAudio: FAudio backend (static lib linked into renegade.exe)')

elif _backend == 'openal'
  openal_dep = dependency('openal-soft', required : true)

  wwaudio_sources += files(
    'mingw/openal_mss.cpp',
    'mingw/audio_decode.cpp',
    'mingw/mp3_decode_mpg123.cpp',
  )
  _cpp += ['-DWWAUDIO_USE_OPENAL']
  wwaudio_deps += [openal_dep]

  if get_option('wwaudio_mpg123')
    mpg123_dep = dependency('libmpg123', required : true)
    _cpp += ['-DWWAUDIO_HAVE_MPG123']
    wwaudio_deps += [mpg123_dep]
    message('WWAudio: OpenAL + mpg123 (MP3)')
  else
    warning('wwaudio_mpg123=false: MP3 will not decode (WAV/ADPCM still work)')
  endif
  message('WWAudio: OpenAL backend (soft_oal.dll at runtime)')

else
  wwaudio_sources += files('mingw/mss_stubs.cpp')
  message('WWAudio: silent Miles stubs (no audio output)')
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

subdir('tools')
