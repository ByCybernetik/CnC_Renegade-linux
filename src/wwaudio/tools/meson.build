# Standalone MP3 → FAudio test (same decode as game). Only with FAudio + mpg123.

if get_option('platform') == 'linux'
  subdir_done()
endif

if get_option('wwaudio_backend') != 'faudio' or not get_option('wwaudio_mpg123')
  subdir_done()
endif

faudio_dep = dependency('faudio', required : true)
mpg123_dep = dependency('libmpg123', required : true)

inc = include_directories('..', '../mingw', '../../../third_party/faudio/include')

_cpp = renegade_cpp_args + ['-DWWAUDIO_HAVE_MPG123']
_link = renegade_link_args

mp3_faudio_player = executable(
  'mp3_faudio_player',
  files(
    'mp3_faudio_player.cpp',
    '../mingw/mp3_decode_mpg123.cpp',
  ),
  include_directories : inc,
  cpp_args : _cpp,
  link_args : _link + ['-lwinmm'],
  dependencies : [faudio_dep, mpg123_dep],
  install : false,
)

message('mp3_faudio_player: FAudio is statically linked (no FAudio.dll copy needed)')
