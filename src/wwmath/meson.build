wwmath_sources = files(
  'aabox.cpp',
  'aabtreecull.cpp',
  'cardinalspline.cpp',
  'catmullromspline.cpp',
  'colmath.cpp',
  'colmathaabox.cpp',
  'colmathaabtri.cpp',
  'colmathfrustum.cpp',
  'colmathline.cpp',
  'colmathobbobb.cpp',
  'colmathobbox.cpp',
  'colmathobbtri.cpp',
  'colmathplane.cpp',
  'colmathsphere.cpp',
  'cullsys.cpp',
  'curve.cpp',
  'euler.cpp',
  'frustum.cpp',
  'gridcull.cpp',
  'hermitespline.cpp',
  'lineseg.cpp',
  'lookuptable.cpp',
  'matrix3.cpp',
  'matrix3d.cpp',
  'matrix4.cpp',
  'obbox.cpp',
  'ode.cpp',
  'pot.cpp',
  'quat.cpp',
  'tcbspline.cpp',
  'tri.cpp',
  'v3_rnd.cpp',
  'vehiclecurve.cpp',
  'vp.cpp',
  'wwmath.cpp',
)

# wwsaveload headers only (persist IDs for curves); no link dependency yet.
inc = include_directories('.', '../wwlib', '../wwdebug', '../wwsaveload')

_cpp = meson.project_name() == 'cnc-renegade' ? renegade_cpp_args : [
  '-DWINVER=0x0400', '-D_WIN32_WINNT=0x0400', '-DWIN32', '-D_WINDOWS',
  '-DWIN32_LEAN_AND_MEAN', '-DNDEBUG', '-fpermissive',
]
_link = meson.project_name() == 'cnc-renegade' ? renegade_link_args : ['-mcrtdll=msvcrt-os', '-static-libgcc', '-static-libstdc++', '-Wl,-Bstatic', '-lwinpthread', '-Wl,-Bdynamic']

wwmath_lib = static_library(
  'wwmath',
  wwmath_sources,
  include_directories : inc,
  cpp_args : _cpp,
  link_args : _link,
)
