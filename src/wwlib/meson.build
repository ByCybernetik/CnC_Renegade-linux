# WWLib sources (repository src/wwlib).
# Matches wwlib.dsp (Release). Excluded:
#   Except.cpp     — MSVC SEH / imagehlp (use mingw/except_stubs.cpp)
#   regexpr.cpp    — requires gnu_regex.c (not in repository)

wwlib_sources = files(
  '_convert.cpp',
  '_mono.cpp',
  'argv.cpp',
  'b64pipe.cpp',
  'b64straw.cpp',
  'base64.cpp',
  'blit.cpp',
  'blowfish.cpp',
  'buff.cpp',
  'bufffile.cpp',
  'chunkio.cpp',
  'cpudetect.cpp',
  'crc.cpp',
  'cstraw.cpp',
  'data.cpp',
  'FastAllocator.cpp',
  'ffactory.cpp',
  'gcd_lcm.cpp',
  'hash.cpp',
  'hsv.cpp',
  'ini.cpp',
  'int.cpp',
  'jshell.cpp',
  'keyboard.cpp',
  'lcw.cpp',
  'load.cpp',
  'lzo.cpp',
  'lzo1x_c.cpp',
  'lzo1x_d.cpp',
  'md5.cpp',
  'mixfile.cpp',
  'mono.cpp',
  'mpmath.cpp',
  'mpu.cpp',
  'msgloop.cpp',
  'multilist.cpp',
  'mutex.cpp',
  'nstrdup.cpp',
  'palette.cpp',
  'pcx.cpp',
  'pipe.cpp',
  'pk.cpp',
  'ramfile.cpp',
  'random.cpp',
  'rawfile.cpp',
  'rc4.cpp',
  'rcfile.cpp',
  'readline.cpp',
  'realcrc.cpp',
  'refcount.cpp',
  'registry.cpp',
  'rgb.cpp',
  'rle.cpp',
  'rndstrng.cpp',
  'sampler.cpp',
  'sha.cpp',
  'slnode.cpp',
  'srandom.cpp',
  'stimer.cpp',
  'straw.cpp',
  'systimer.cpp',
  'tagblock.cpp',
  'targa.cpp',
  'textfile.cpp',
  'thread.cpp',
  'trim.cpp',
  'vector.cpp',
  'widestring.cpp',
  'win.cpp',
  'wwfile.cpp',
  'wwfont.cpp',
  'wwstring.cpp',
  'xpipe.cpp',
  'xstraw.cpp',
  'mingw/except_stubs.cpp',
  'mingw/wwdebug_stubs.cpp',
)

if get_option('platform') != 'linux'
  wwlib_sources += files(
    'convert.cpp',
    'LaunchWeb.cpp',
    'ddraw.cpp',
    'dsurface.cpp',
    'WWCOMUtil.cpp',
    'xsurface.cpp',
  )
endif

if get_option('platform') == 'linux'
  wwlib_sources += files(
    'linux/short_wprintf.cpp',
    'linux/verchk_stub.cpp',
  )
else
  wwlib_sources += files('verchk.cpp')
endif

_wwlib_inc = ['.', 'mingw', '../wwdebug', '../platform']
if get_option('platform') == 'linux'
  _wwlib_inc += '../platform/linux'
endif
inc = include_directories(_wwlib_inc)

_wwlib_cpp = meson.project_name() == 'cnc-renegade' ? renegade_wwlib_cpp_args : [
  '-DWINVER=0x0400', '-D_WIN32_WINNT=0x0400', '-DWIN32', '-D_WINDOWS',
  '-DWIN32_LEAN_AND_MEAN', '-DREGEX_MALLOC', '-DSTDC_HEADERS', '-DNDEBUG', '-fpermissive',
]
_wwlib_link = meson.project_name() == 'cnc-renegade' ? renegade_link_args : ['-mcrtdll=msvcrt-os', '-static-libgcc', '-static-libstdc++', '-Wl,-Bstatic', '-lwinpthread', '-Wl,-Bdynamic']

wwlib_lib = static_library(
  'wwlib',
  wwlib_sources,
  include_directories : inc,
  cpp_args : _wwlib_cpp,
  link_args : _wwlib_link,
)
