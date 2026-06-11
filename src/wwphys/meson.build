wwphys_sources = files(
  'Path.cpp',
  'PathDebugPlotter.cpp',
  'PathObject.cpp',
  'Pathfind.cpp',
  'PathfindPortal.cpp',
  'PathfindSector.cpp',
  'accessiblephys.cpp',
  'animcollisionmanager.cpp',
  'bin_aabox.cpp',
  'bin_axes.cpp',
  'bin_obbox.cpp',
  'bin_point.cpp',
  'bin_vector.cpp',
  'bpt.cpp',
  'camerashakesystem.cpp',
  'decophys.cpp',
  'dynamicaabtreecull.cpp',
  'dynamicanimphys.cpp',
  'dynamicphys.cpp',
  'dynamicshadowmanager.cpp',
  'dyntexproject.cpp',
  'floodfillbox.cpp',
  'floodfillgrid.cpp',
  'grideffect.cpp',
  'heightdb.cpp',
  'humanphys.cpp',
  'lightcull.cpp',
  'lightphys.cpp',
  'lightsolve.cpp',
  'lightsolvecontext.cpp',
  'materialeffect.cpp',
  'motorcycle.cpp',
  'motorvehicle.cpp',
  'movephys.cpp',
  'octbox.cpp',
  'pathfindbox.cpp',
  'pathmgr.cpp',
  'pathnode.cpp',
  'pathsolve.cpp',
  'phys.cpp',
  'phys3.cpp',
  'physaabtreecull.cpp',
  'physcon.cpp',
  'physcontrol.cpp',
  'physdecalsys.cpp',
  'physdynamicsavesystem.cpp',
  'physgridcull.cpp',
  'physresourcemgr.cpp',
  'physstaticsavesystem.cpp',
  'phystexproject.cpp',
  'progcall.cpp',
  'projectile.cpp',
  'projectormanager.cpp',
  'pscene.cpp',
  'pscene_collision.cpp',
  'pscene_decal.cpp',
  'pscene_lighting.cpp',
  'pscene_projectors.cpp',
  'pscene_saveload.cpp',
  'pscene_vis.cpp',
  'rbody.cpp',
  'rbody_obsolete.cpp',
  'renderobjphys.cpp',
  'renegadeterrainmaterialpass.cpp',
  'renegadeterrainpatch.cpp',
  'ridermanager.cpp',
  'shakeablestaticphys.cpp',
  'staticaabtreecull.cpp',
  'staticanimphys.cpp',
  'staticphys.cpp',
  'stealtheffect.cpp',
  'terrainmaterial.cpp',
  'timeddecophys.cpp',
  'trackedvehicle.cpp',
  'transitioneffect.cpp',
  'umbrasupport.cpp',
  'vehicledazzle.cpp',
  'vehiclephys.cpp',
  'visoptimizationcontext.cpp',
  'visoptprogress.cpp',
  'visrendercontext.cpp',
  'vissample.cpp',
  'vissectorstats.cpp',
  'vistable.cpp',
  'vistablemgr.cpp',
  'vtolvehicle.cpp',
  'waypath.cpp',
  'waypoint.cpp',
  'wheel.cpp',
  'wheelvehicle.cpp',
  'widgets.cpp',
  'widgetuser.cpp',
  'wwphys.cpp'
)

inc = include_directories('.', '../wwlib', '../wwlib/mingw', '../wwdebug', '../wwmath', '../wwsaveload', '../wwutil', '../wwbitpack', '../ww3d2')

_cpp = meson.project_name() == 'cnc-renegade' ? renegade_cpp_args : [
  '-DWINVER=0x0400', '-D_WIN32_WINNT=0x0400', '-DWIN32', '-D_WINDOWS',
  '-DWIN32_LEAN_AND_MEAN', '-DNDEBUG', '-fpermissive',
]
_link = meson.project_name() == 'cnc-renegade' ? renegade_link_args : ['-mcrtdll=msvcrt-os', '-static-libgcc', '-static-libstdc++', '-Wl,-Bstatic', '-lwinpthread', '-Wl,-Bdynamic']

_wwphys_deps = []
_wwphys_cpp = _cpp
_wwphys_override = []
_linux_d3dx_compat = meson.project_source_root() / 'src/platform/linux/dxvk_compat.h'
if get_option('platform') == 'linux'
  _wwphys_deps = [linux_graphics_dep]
  _wwphys_cpp = []
  foreach _arg : _cpp
    if _arg != '-std=gnu++98' and _arg != '-std=gnu++11'
      _wwphys_cpp += _arg
    endif
  endforeach
  _wwphys_cpp += [
    '-std=gnu++11',
    '-include', _linux_d3dx_compat,
    '-DRENEGADE_USE_D3DX8',
  ]
  _wwphys_override = ['cpp_std=gnu++11']
endif

wwphys_lib = static_library(
  'wwphys',
  wwphys_sources,
  include_directories : inc,
  dependencies : _wwphys_deps,
  cpp_args : _wwphys_cpp,
  link_args : _link,
  override_options : _wwphys_override,
)
