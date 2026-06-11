combat_sources = files(
  'action.cpp',
  'activeconversation.cpp',
  'airstripgameobj.cpp',
  'animcontrol.cpp',
  'armedgameobj.cpp',
  'assetdep.cpp',
  'backgroundmgr.cpp',
  'basecontroller.cpp',
  'basegameobj.cpp',
  'beacongameobj.cpp',
  'bones.cpp',
  'building.cpp',
  'buildingaggregate.cpp',
  'buildingmonitor.cpp',
  'buildingstate.cpp',
  'bullet.cpp',
  'c4.cpp',
  'ccamera.cpp',
  'characterclasssettings.cpp',
  'cheatmgr.cpp',
  'cinematicgameobj.cpp',
  'clientcontrol.cpp',
  'CNCModeSettings.cpp',
  'colors.cpp',
  'combat.cpp',
  'combatdazzle.cpp',
  'combatmaterialeffectmanager.cpp',
  'combatsaveload.cpp',
  'combatsound.cpp',
  'comcentergameobj.cpp',
  'control.cpp',
  'conversation.cpp',
  'conversationmgr.cpp',
  'conversationremark.cpp',
  'cover.cpp',
  'csdamageevent.cpp',
  'damage.cpp',
  'damageablegameobj.cpp',
  'damageablestaticphys.cpp',
  'damagezone.cpp',
  'diaglog.cpp',
  'dialogue.cpp',
  'doors.cpp',
  'dynamicspeechanim.cpp',
  'effectrecycler.cpp',
  'elevator.cpp',
  'encyclopediamgr.cpp',
  'evasettings.cpp',
  'explosion.cpp',
  'ffactorylist.cpp',
  'gameobjmanager.cpp',
  'gameobjobserver.cpp',
  'gametype.cpp',
  'globalsettings.cpp',
  'harvester.cpp',
  'hud.cpp',
  'hudinfo.cpp',
  'humanrecoil.cpp',
  'humanstate.cpp',
  'input.cpp',
  'inventory.cpp',
  'mapmgr.cpp',
  'mendozabossgameobj.cpp',
  'messagewindow.cpp',
  'muzzlerecoil.cpp',
  'objectives.cpp',
  'objectivesviewer.cpp',
  'objlibrary.cpp',
  'orator.cpp',
  'oratortypes.cpp',
  'pathaction.cpp',
  'persistentgameobjobserver.cpp',
  'physicalgameobj.cpp',
  'pilot.cpp',
  'playerdata.cpp',
  'playerterminal.cpp',
  'playertype.cpp',
  'powerplantgameobj.cpp',
  'powerup.cpp',
  'purchasesettings.cpp',
  'radar.cpp',
  'raveshawbossgameobj.cpp',
  'refinerygameobj.cpp',
  'reflist.cpp',
  'repairbaygameobj.cpp',
  'sakurabossgameobj.cpp',
  'samsite.cpp',
  'savegame.cpp',
  'scexplosionevent.cpp',
  'scobeliskevent.cpp',
  'screenfademanager.cpp',
  'scriptablegameobj.cpp',
  'scriptcommands.cpp',
  'scripts.cpp',
  'scriptzone.cpp',
  'simplegameobj.cpp',
  'smartgameobj.cpp',
  'sniper.cpp',
  'soldier.cpp',
  'soldierfactorygameobj.cpp',
  'soldierobserver.cpp',
  'SoundEnvironment.cpp',
  'spawn.cpp',
  'specialeffectsgameobj.cpp',
  'statemachine.cpp',
  'staticnetworkobject.cpp',
  'surfaceeffects.cpp',
  'systeminfolog.cpp',
  'teampurchasesettings.cpp',
  'textwindow.cpp',
  'transition.cpp',
  'transitiongameobj.cpp',
  'unitcoordinationzonemgr.cpp',
  'vehicle.cpp',
  'vehicledriver.cpp',
  'vehiclefactorygameobj.cpp',
  'viseme.cpp',
  'warfactorygameobj.cpp',
  'weaponbag.cpp',
  'weaponmanager.cpp',
  'weapons.cpp',
  'weaponview.cpp',
  'WeatherMgr.cpp',
  'assets.cpp',
  'crandom.cpp',
  'debug.cpp',
  'timemgr.cpp',
)

if get_option('platform') == 'linux'
  combat_sources += files('linux/directinput_sdl3.cpp')
else
  combat_sources += files('directinput.cpp')
endif

_combat_inc = [
  '.',
  '../wwlib',
  '../wwlib/mingw',
  '../platform',
]
if get_option('platform') == 'linux'
  _combat_inc += '../platform/linux'
endif
_combat_inc += [
  '../wwdebug',
  '../wwmath',
  '../wwsaveload',
  '../wwtranslatedb',
  '../wwutil',
  '../wwbitpack',
  '../wwnet',
  '../ww3d2',
  '../wwphys',
  '../wwaudio',
  '../wwaudio/mingw',
  '../wwui',
  '../commando',
]
inc = include_directories(_combat_inc)

_cpp = meson.project_name() == 'cnc-renegade' ? renegade_cpp_args : [
  '-DWINVER=0x0400', '-D_WIN32_WINNT=0x0400', '-DWIN32', '-D_WINDOWS',
  '-DWIN32_LEAN_AND_MEAN', '-DNDEBUG', '-fpermissive',
]
_link = meson.project_name() == 'cnc-renegade' ? renegade_link_args : ['-mcrtdll=msvcrt-os', '-static-libgcc', '-static-libstdc++', '-Wl,-Bstatic', '-lwinpthread', '-Wl,-Bdynamic']

_combat_deps = []
_combat_cpp = _cpp
_combat_override = []
_linux_d3dx_compat = meson.project_source_root() / 'src/platform/linux/dxvk_compat.h'
if get_option('platform') == 'linux'
  _combat_deps += [platform_dep, linux_graphics_dep]
  _combat_cpp = []
  foreach _arg : _cpp
    if _arg != '-std=gnu++98' and _arg != '-std=gnu++11'
      _combat_cpp += _arg
    endif
  endforeach
  _combat_cpp += [
    '-std=gnu++11',
    '-include', _linux_d3dx_compat,
    '-DRENEGADE_USE_D3DX8',
  ]
  _combat_override = ['cpp_std=gnu++11']
endif

combat_lib = static_library(
  'combat',
  combat_sources,
  include_directories : inc,
  cpp_args : _combat_cpp,
  link_args : _link,
  dependencies : _combat_deps,
  override_options : _combat_override,
)
