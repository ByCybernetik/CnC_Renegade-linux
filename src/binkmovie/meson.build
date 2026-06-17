subtitle_sources = files(
  'subtitle.cpp',
  'subtitleparser.cpp',
)

inc = include_directories('.', '../wwlib', '../wwdebug')

_cpp = meson.project_name() == 'cnc-renegade' ? renegade_cpp_args : [
  '-DWINVER=0x0400', '-D_WIN32_WINNT=0x0400', '-DWIN32', '-D_WINDOWS',
  '-DWIN32_LEAN_AND_MEAN', '-DNDEBUG', '-fpermissive',
]
_link = meson.project_name() == 'cnc-renegade' ? renegade_link_args : ['-mcrtdll=msvcrt-os', '-static-libgcc', '-static-libstdc++', '-Wl,-Bstatic', '-lwinpthread', '-Wl,-Bdynamic']

binkmovie_deps = []

if get_option('platform') == 'linux'
  _bink_d3dx_compat = meson.project_source_root() / 'src/platform/linux/dxvk_compat.h'
  _cpp += ['-DRENEGADE_USE_D3DX8', '-include', _bink_d3dx_compat]
  binkmovie_sources = subtitle_sources + files(
    'BINKMovie.cpp',
    'subtitlemanager.cpp',
    'linux/bink_ffmpeg.cpp',
  )
  _bink_inc = [
    'linux',
    '../ww3d2',
    '../wwmath',
    '../wwsaveload',
    '../wwui',
    '../platform',
    '../platform/linux',
  ]
  if get_option('graphics_backend') == 'vulkan'
    _cpp += '-DRENEGADE_VULKAN'
    if get_option('vulkan_renderer') == 'engine'
      _cpp += '-DRENEGADE_VULKAN_ENGINE'
      _bink_inc += '../ww3d2_engine'
      binkmovie_deps += ww3d2_engine_dep
    else
      _bink_inc += '../ww3d2_vulkan'
      binkmovie_deps += ww3d2_vulkan_dep
    endif
  endif
  inc = [inc, include_directories(_bink_inc)]
  avcodec_dep = dependency('libavcodec', required : true)
  avformat_dep = dependency('libavformat', required : true)
  avutil_dep = dependency('libavutil', required : true)
  swscale_dep = dependency('libswscale', required : true)
  swresample_dep = dependency('libswresample', required : true)
  binkmovie_deps += [sdl3_dep, avcodec_dep, avformat_dep, avutil_dep, swscale_dep, swresample_dep]
  message('BinkMovie: FFmpeg + SDL3 (Linux)')
else
  binkmovie_sources = subtitle_sources + files(
    'mingw/binkmovie_stubs.cpp',
  )
  message('BinkMovie: stubs (Windows / no Bink SDK in tree)')
endif

binkmovie_lib = static_library(
  'binkmovie',
  binkmovie_sources,
  include_directories : inc,
  cpp_args : _cpp,
  link_args : _link,
  dependencies : binkmovie_deps,
  install : false,
)
