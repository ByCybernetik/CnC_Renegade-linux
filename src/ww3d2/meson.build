ww3d2_sources = files(
  'aabtree.cpp',
  'aabtreebuilder.cpp',
  'agg_def.cpp',
  'animobj.cpp',
  'assetmgr.cpp',
  'assetstatus.cpp',
  'bitmaphandler.cpp',
  'bmp2d.cpp',
  'boxrobj.cpp',
  'bwrender.cpp',
  'camera.cpp',
  'collect.cpp',
  'coltest.cpp',
  'composite.cpp',
  'dazzle.cpp',
  'ddsfile.cpp',
  'decalmsh.cpp',
  'decalsys.cpp',
  'distlod.cpp',
  'dx8caps.cpp',
  'dx8fvf.cpp',
  'dx8indexbuffer.cpp',
  'dx8polygonrenderer.cpp',
  'dx8renderer.cpp',
  'dx8rendererdebugger.cpp',
  'dx8texman.cpp',
  'dx8vertexbuffer.cpp',
  'dx8wrapper.cpp',
  'dynamesh.cpp',
  'font3d.cpp',
  'formconv.cpp',
  'hanim.cpp',
  'hanimmgr.cpp',
  'hcanim.cpp',
  'hlod.cpp',
  'hmdldef.cpp',
  'hmorphanim.cpp',
  'hrawanim.cpp',
  'htree.cpp',
  'htreemgr.cpp',
  'intersec.cpp',
  'layer.cpp',
  'light.cpp',
  'lightenvironment.cpp',
  'line3d.cpp',
  'linegrp.cpp',
  'mapper.cpp',
  'matinfo.cpp',
  'matpass.cpp',
  'matrixmapper.cpp',
  'mesh.cpp',
  'meshbuild.cpp',
  'meshdam.cpp',
  'meshgeometry.cpp',
  'meshmatdesc.cpp',
  'meshmdl.cpp',
  'meshmdlio.cpp',
  'metalmap.cpp',
  'missingtexture.cpp',
  'motchan.cpp',
  'nullrobj.cpp',
  'part_buf.cpp',
  'part_emt.cpp',
  'part_ldr.cpp',
  'pivot.cpp',
  'pointgr.cpp',
  'polyinfo.cpp',
  'predlod.cpp',
  'prim_anim.cpp',
  'projector.cpp',
  'proto.cpp',
  'render2d.cpp',
  'render2dsentence.cpp',
  'renderobjectrecycler.cpp',
  'rendobj.cpp',
  'rinfo.cpp',
  'ringobj.cpp',
  'scene.cpp',
  'segline.cpp',
  'seglinerenderer.cpp',
  'shader.cpp',
  'shattersystem.cpp',
  'snappts.cpp',
  'sortingrenderer.cpp',
  'sphereobj.cpp',
  'statistics.cpp',
  'stripoptimizer.cpp',
  'surfaceclass.cpp',
  'texfcach.cpp',
  'texproject.cpp',
  'texture.cpp',
  'textureloader.cpp',
  'texturethumbnail.cpp',
  'txt.cpp',
  'vertmaterial.cpp',
  'visrasterizer.cpp',
  'w3d_dep.cpp',
  'w3d_util.cpp',
  'ww3d.cpp',
  'ww3dformat.cpp',
)

if get_option('platform') == 'linux'
  ww3d2_sources += files(
    'linux/framgrab_stub.cpp',
    'linux/ww3d2_gdi_stubs.cpp',
    'mingw/d3dx8_stubs.cpp',
    'mingw/animatedsoundmgr_stubs.cpp',
  )
else
  ww3d2_sources += files(
    'framgrab.cpp',
    'mingw/d3dx8_stubs.cpp',
    'mingw/animatedsoundmgr_stubs.cpp',
  )
endif

_linux_dxvk_compat = meson.project_source_root() / 'src/platform/linux/dxvk_compat.h'

_stb_inc = []
if get_option('platform') == 'linux'
  _stb_inc = '../../third_party/stb'
endif
_ww3d2_inc = [
  '.',
  'mingw',
  '../platform',
]
if get_option('platform') == 'linux'
  _ww3d2_inc += '../platform/linux'
  if get_option('graphics_backend') == 'vulkan'
    _ww3d2_inc += '../ww3d2_vulkan'
  endif
endif
_ww3d2_inc += [
  _stb_inc,
  '../wwlib',
  '../wwlib/mingw',
  '../wwdebug',
  '../wwmath',
  '../wwsaveload',
  '../wwutil',
  '../wwbitpack',
]
inc = include_directories(_ww3d2_inc)

_cpp = meson.project_name() == 'cnc-renegade' ? renegade_cpp_args : [
  '-DWINVER=0x0400', '-D_WIN32_WINNT=0x0400', '-DWIN32', '-D_WINDOWS',
  '-DWIN32_LEAN_AND_MEAN', '-DNDEBUG', '-fpermissive',
]
_link = meson.project_name() == 'cnc-renegade' ? renegade_link_args : ['-mcrtdll=msvcrt-os', '-static-libgcc', '-static-libstdc++', '-Wl,-Bstatic', '-lwinpthread', '-Wl,-Bdynamic']

_ww3d2_deps = get_option('platform') == 'linux' ? [linux_graphics_dep] : [dxsdk8_dep]
_ww3d2_link = _link

_ww3d2_cpp = _cpp
_ww3d2_override = []
if get_option('platform') == 'linux'
  _ww3d2_cpp = []
  foreach _arg : _cpp
    if _arg != '-std=gnu++98'
      _ww3d2_cpp += _arg
    endif
  endforeach
  _ww3d2_cpp += [
    '-std=gnu++11',
    '-include', _linux_dxvk_compat,
    '-DRENEGADE_WW3D2_BUILD',
  ]
  _ww3d2_override = ['cpp_std=gnu++11']
  if get_option('graphics_backend') == 'vulkan'
    _ww3d2_cpp += '-DRENEGADE_VULKAN'
    _ww3d2_deps += [ww3d2_vulkan_dep]
  endif
endif

ww3d2_lib = static_library(
  'ww3d2',
  ww3d2_sources,
  include_directories : inc,
  dependencies : _ww3d2_deps,
  cpp_args : _ww3d2_cpp,
  link_args : _ww3d2_link,
  override_options : _ww3d2_override,
)
