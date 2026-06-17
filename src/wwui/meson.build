wwui_sources = files(
  'buttonctrl.cpp',
  'checkboxctrl.cpp',
  'childdialog.cpp',
  'comboboxctrl.cpp',
  'dialogbase.cpp',
  'dialogcontrol.cpp',
  'dialogmgr.cpp',
  'dialogparser.cpp',
  'dialogtext.cpp',
  'dialogtransition.cpp',
  'dropdownctrl.cpp',
  'editctrl.cpp',
  'healthbarctrl.cpp',
  'imagectrl.cpp',
  'IMECandidate.cpp',
  'IMECandidateCtrl.cpp',
  'IMEManager.cpp',
  'inputctrl.cpp',
  'listctrl.cpp',
  'listiconmgr.cpp',
  'mapctrl.cpp',
  'menubackdrop.cpp',
  'menudialog.cpp',
  'menuviewport.cpp',
  'menuentryctrl.cpp',
  'merchandisectrl.cpp',
  'mousemgr.cpp',
  'multilinetextctrl.cpp',
  'popupdialog.cpp',
  'ProgressCtrl.cpp',
  'screencursor.cpp',
  'scrollbarctrl.cpp',
  'shortcutbarctrl.cpp',
  'sliderctrl.cpp',
  'stylemgr.cpp',
  'tabctrl.cpp',
  'textmarqueectrl.cpp',
  'tooltip.cpp',
  'tooltipmgr.cpp',
  'treectrl.cpp',
  'viewerctrl.cpp',
  'wwuiinput.cpp',
)

_wwui_inc_dirs = [
  '.',
  '../wwlib',
  '../wwlib/mingw',
  '../wwdebug',
  '../wwmath',
  '../ww3d2',
  '../wwsaveload',
  '../wwtranslatedb',
  '../wwaudio',
  '../wwaudio/mingw',
]
if get_option('platform') == 'linux'
  _wwui_inc_dirs += '../platform/linux'
  if get_option('graphics_backend') == 'vulkan'
    _wwui_inc_dirs += '../ww3d2_vulkan'
  endif
endif
inc = include_directories(_wwui_inc_dirs)

_cpp = meson.project_name() == 'cnc-renegade' ? renegade_cpp_args : [
  '-DWINVER=0x0400', '-D_WIN32_WINNT=0x0400', '-DWIN32', '-D_WINDOWS',
  '-DWIN32_LEAN_AND_MEAN', '-DNDEBUG', '-fpermissive',
]
_link = meson.project_name() == 'cnc-renegade' ? renegade_link_args : ['-mcrtdll=msvcrt-os', '-static-libgcc', '-static-libstdc++', '-Wl,-Bstatic', '-lwinpthread', '-Wl,-Bdynamic']

_wwui_deps = []
_wwui_cpp = _cpp
_wwui_override = []
_linux_d3dx_compat = meson.project_source_root() / 'src/platform/linux/dxvk_compat.h'
if get_option('platform') == 'linux'
  _wwui_deps = [linux_graphics_dep]
  _wwui_cpp = []
  foreach _arg : _cpp
    if _arg != '-std=gnu++98' and _arg != '-std=gnu++11'
      _wwui_cpp += _arg
    endif
  endforeach
  _wwui_cpp += [
    '-std=gnu++11',
    '-include', _linux_d3dx_compat,
    '-DRENEGADE_USE_D3DX8',
  ]
  _wwui_override = ['cpp_std=gnu++11']
  if get_option('graphics_backend') == 'vulkan'
    _wwui_cpp += '-DRENEGADE_VULKAN'
    _wwui_deps += [ww3d2_vulkan_dep]
  endif
endif

wwui_lib = static_library(
  'wwui',
  wwui_sources,
  include_directories : inc,
  dependencies : _wwui_deps,
  cpp_args : _wwui_cpp,
  link_args : _link,
  override_options : _wwui_override,
)
