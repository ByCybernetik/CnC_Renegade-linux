_is_linux = get_option('platform') == 'linux'

# VC6-style paths (WWLib/WideString.h) need per-header symlinks for MinGW cross.
if not _is_linux
  _py3 = find_program('python3', required : true)
  run_command(_py3, files('mingw/gen_include_symlinks.py'), check : true)
endif

_commando_inc = [
  'mingw/include',
  '../../Code',
  '.',
  'mingw',
  '../platform',
]
if _is_linux
  _commando_inc += '../platform/linux'
endif
_commando_inc += [
  '../wwlib',
  '../wwlib/mingw',
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
  '../combat',
  '../binkmovie',
  '../scontrol',
  '../wwonline',
  '../../Code/wolapi',
  '../../Code/WOLBrowser',
  '../../Code/BandTest',
]
inc = include_directories(_commando_inc)

_commando_d3dx_compat = meson.project_source_root() / 'src/platform/linux/dxvk_compat.h'
_commando_extra_cpp = _is_linux ? [
  '-DRENEGADE_USE_D3DX8',
  '-include', _commando_d3dx_compat,
] : ['-include', 'force_stdio.h', '-DBANDTEST_EXPORTS']
_cpp = meson.project_name() == 'cnc-renegade' ? renegade_cpp_args + _commando_extra_cpp : [
  '-DWINVER=0x0400', '-D_WIN32_WINNT=0x0400', '-DWIN32', '-D_WINDOWS',
  '-DWIN32_LEAN_AND_MEAN', '-DNDEBUG', '-fpermissive',
  '-include', 'force_stdio.h',
]
_link = meson.project_name() == 'cnc-renegade' ? renegade_link_args : [
  '-mcrtdll=msvcrt-os', '-static-libgcc', '-static-libstdc++',
  '-Wl,-Bstatic', '-lwinpthread', '-Wl,-Bdynamic',
]

win_link_args = [
  '-luser32', '-lgdi32', '-lwinspool', '-lcomdlg32', '-ladvapi32', '-lshell32',
  '-lole32', '-loleaut32', '-luuid', '-lwinmm', '-lws2_32', '-lversion', '-limm32',
  '-ldsound', '-lvfw32',
]

linux_link_args = ['-pthread', '-ldl', '-lm']

commando_chat_res = []
if _is_linux
  _windres = find_program('i686-w64-mingw32-windres', required: true)
  _objcopy = find_program('objcopy', required: true)
  _mingw_sysinc = '/usr/i686-w64-mingw32/include'
  _commando_dir = meson.current_source_dir()
  # COFF .rsrc section (PE resource tree), not MSVC .res — required by pe_resource_loader.cpp.
  _chat_coff = custom_target(
    'renegade_chat_coff',
    input: files('chat.rc'),
    output: 'renegade_chat_coff.o',
    command: [
      _windres,
      '-D__WINDRES__',
      '-I', _commando_dir,
      '-I', _mingw_sysinc,
      '-O', 'coff',
      '-o', '@OUTPUT@',
      '@INPUT@',
    ],
    build_by_default: true,
  )
  _chat_rsrc_bin = custom_target(
    'renegade_chat_rsrc_bin',
    input: _chat_coff,
    output: 'renegade_chat_rsrc.bin',
    command: [
      _objcopy,
      '-O', 'binary',
      '-j', '.rsrc',
      '@INPUT@',
      '@OUTPUT@',
    ],
    build_by_default: true,
  )
  commando_chat_res = custom_target(
    'renegade_chat_rsrc_blob',
    input: _chat_rsrc_bin,
    output: 'renegade_chat_rsrc_blob.o',
    command: [
      _objcopy,
      '-I', 'binary',
      '-O', 'elf64-x86-64',
      '@INPUT@',
      '@OUTPUT@',
      '--rename-section', '.data=.rodata,alloc,load,readonly,data,contents',
    ],
    build_by_default: true,
  )
else
  _windres = find_program('i686-w64-mingw32-windres', required: true)
  _mingw_sysinc = '/usr/i686-w64-mingw32/include'
  _commando_dir = meson.current_source_dir()
  commando_chat_res = custom_target(
    'commando_chat_res',
    input: files('chat.rc'),
    output: 'chat_res.o',
    command: [
      _windres,
      '-D__WINDRES__',
      '-I', _commando_dir,
      '-I', _mingw_sysinc,
      '-O', 'coff',
      '-o', '@OUTPUT@',
      '@INPUT@',
    ],
    build_by_default: true,
  )
endif

commando_sources = files(
  '../../Code/wolapi/WOLAPI_i.c',
  '../../Code/WOLBrowser/WOLBrowser_i.c',
  'AnnounceEvent.cpp',
  'apppacketstats.cpp',
  'AutoStart.cpp',
  'bandwidth.cpp',
  'bandwidthcheck.cpp',
  'bandwidthgraph.cpp',
  'bioevent.cpp',
  'buildnum.cpp',
  'campaign.cpp',
  'cdverify.cpp',
  'changeteamevent.cpp',
  'chatshre.cpp',
  'clientbboevent.cpp',
  'clientfps.cpp',
  'clientgoodbyeevent.cpp',
  'clienthintmanager.cpp',
  'clientpingmanager.cpp',
  'cnetwork.cpp',
  'combatgmode.cpp',
  'cominit.cpp',
  'commandosaveload.cpp',
  'comnetrcvinst.cpp',
  'console.cpp',
  'consolecommandevent.cpp',
  'consolefunction.cpp',
  'ConsoleMode.cpp',
  'csconsolecommandevent.cpp',
  'cshint.cpp',
  'cspingrequestevent.cpp',
  'cstextobj.cpp',
  'datasafe.cpp',
  'devoptions.cpp',
  'diagnostics.cpp',
  'dialogtests.cpp',
  'dlgcharacteroptions.cpp',
  'dlgcheatoptions.cpp',
  'dlgcncbattleinfo.cpp',
  'dlgcncpurchasemainmenu.cpp',
  'dlgcncpurchasemenu.cpp',
  'dlgcncreference.cpp',
  'dlgcncserverinfo.cpp',
  'dlgcncteaminfo.cpp',
  'dlgcncwinscreen.cpp',
  'dlgconfigaudiotab.cpp',
  'dlgconfigperformancetab.cpp',
  'dlgconfigvideotab.cpp',
  'dlgcontrols.cpp',
  'dlgcontrolsaveload.cpp',
  'dlgcontrolslisttab.cpp',
  'dlgcontroltabs.cpp',
  'dlgcredits.cpp',
  'DlgDownload.cpp',
  'dlgevabuildingstab.cpp',
  'dlgevacharacterstab.cpp',
  'dlgevadatatab.cpp',
  'dlgevaencyclopedia.cpp',
  'dlgevamaptab.cpp',
  'dlgevaobjectivestab.cpp',
  'dlgevavehiclestab.cpp',
  'dlgevaviewertab.cpp',
  'dlgevaweaponstab.cpp',
  'dlghelpscreen.cpp',
  'dlgloadspgame.cpp',
  'dlgmainmenu.cpp',
  'DlgMessageBox.cpp',
  'dlgmovieoptions.cpp',
  'dlgmpchangelannickname.cpp',
  'DlgMPConnect.cpp',
  'DlgMPConnectionRefused.cpp',
  'dlgmpingamechat.cpp',
  'dlgmpingameteamchat.cpp',
  'dlgmplangamelist.cpp',
  'dlgmplanhostoptions.cpp',
  'dlgmpslaveservers.cpp',
  'DlgMPTeamSelect.cpp',
  'dlgmpwoladdbuddy.cpp',
  'dlgmpwoladdignoreentry.cpp',
  'dlgmpwolautologinprompt.cpp',
  'dlgmpwolbuddies.cpp',
  'dlgmpwolbuddylistpopup.cpp',
  'dlgmpwolchat.cpp',
  'dlgmpwoldeletebuddy.cpp',
  'dlgmpwoldeleteignoreentry.cpp',
  'dlgmpwolgamelist.cpp',
  'dlgmpwolgoodies.cpp',
  'dlgmpwolignorelist.cpp',
  'dlgmpwolinvitation.cpp',
  'dlgmpwollocalenag.cpp',
  'dlgmpwolmain.cpp',
  'dlgmpwolmotd.cpp',
  'dlgmpwolpagebuddy.cpp',
  'dlgmpwolpagereply.cpp',
  'dlgmpwolquickmatchoptions.cpp',
  'dlgmultiplayoptions.cpp',
  'DlgPasswordPrompt.cpp',
  'dlgpreviewoptions.cpp',
  'DlgQuickmatch.cpp',
  'DlgRestart.cpp',
  'dlgsavegame.cpp',
  'dlgserversaveload.cpp',
  'dlgsidebarhelp.cpp',
  'dlgtechoptions.cpp',
  'DlgWebPage.cpp',
  'DlgWOLAutoStart.cpp',
  'DlgWOLLogon.cpp',
  'DlgWOLSettings.cpp',
  'DlgWOLWait.cpp',
  'donateevent.cpp',
  'evictionevent.cpp',
  'FirewallWait.cpp',
  'floodprotectionmgr.cpp',
  'gamechanlist.cpp',
  'gamechannel.cpp',
  'gamedata.cpp',
  'gamedataupdateevent.cpp',
  'gameinitmgr.cpp',
  'gamemenu.cpp',
  'gamemode.cpp',
  'gameoptionsevent.cpp',
  'GameResSend.cpp',
  'gamesideservercontrol.cpp',
  'gamespycschallengeresponseevent.cpp',
  'gamespyscchallengeevent.cpp',
  'gdcnc.cpp',
  'gdsingleplayer.cpp',
  'gdskirmish.cpp',
  'god.cpp',
  'godmodeevent.cpp',
  'init.cpp',
  'inputconfig.cpp',
  'inputconfigmgr.cpp',
  'lanchat.cpp',
  'langmode.cpp',
  'level.cpp',
  'loadingevent.cpp',
  'mainloop.cpp',
  'mainmenutransition.cpp',
  'messages.cpp',
  'mingw/gamespy_stubs.cpp',
  'mingw/commando_link_stubs.cpp',
  'modpackage.cpp',
  'modpackagemgr.cpp',
  'moneyevent.cpp',
  'movie.cpp',
  'mpsettingsmgr.cpp',
  'multihud.cpp',
  'NAT.cpp',
  'nataddr.cpp',
  'natsock.cpp',
  'natter.cpp',
  'netevent.cpp',
  'netgraphs.cpp',
  'nethandler.cpp',
  'netinterface.cpp',
  'nicenum.cpp',
  'overlay.cpp',
  'pkthandlers.cpp',
  'player.cpp',
  'playerkill.cpp',
  'playermanager.cpp',
  'priority.cpp',
  'purchaserequestevent.cpp',
  'purchaseresponseevent.cpp',
  'radiocommanddisplay.cpp',
  'regbool.cpp',
  'regfloat.cpp',
  'regint.cpp',
  'regstring.cpp',
  'renegadecheatmgr.cpp',
  'renegadedialogmgr.cpp',
  'renegadeplayerterminal.cpp',
  'requestkillevent.cpp',
  'sbbomanager.cpp',
  'scoreevent.cpp',
  'scorescreen.cpp',
  'scpingresponseevent.cpp',
  'sctextobj.cpp',
  'serverfps.cpp',
  'ServerSettings.cpp',
  'shutdown.cpp',
  'singletoninstancekeeper.cpp',
  'skinpackage.cpp',
  'skinpackagemgr.cpp',
  'slavemaster.cpp',
  'suicideevent.cpp',
  'svrgoodbyeevent.cpp',
  'systemsettings.cpp',
  'team.cpp',
  'teammanager.cpp',
  'textdisplay.cpp',
  'useroptions.cpp',
  'vendor.cpp',
  'vipmodeevent.cpp',
  'warpevent.cpp',
  'WebBrowser.cpp',
  'winevent.cpp',
  'winmain.cpp',
  'WOLBuddyMgr.cpp',
  'WOLChatMgr.cpp',
  'WOLDiags.cpp',
  'WOLGameInfo.cpp',
  'wolgmode.cpp',
  'WOLJoinGame.cpp',
  'wollocalemgr.cpp',
  'WOLLoginProfile.cpp',
  'WOLLogonMgr.cpp',
  'WOLQuickMatch.cpp',
)

_renegade_deps = _is_linux ? [platform_dep, linux_graphics_dep] : [dxsdk8_dep]
if not _is_linux
  _ww_backend = get_option('wwaudio_backend')
  if _ww_backend == 'mss'
    _renegade_deps += dependency('miles', required : true)
  elif _ww_backend == 'faudio'
    _renegade_deps += dependency('faudio', required : true)
  elif _ww_backend == 'openal'
    _renegade_deps += dependency('openal-soft', required : true)
  endif
  if get_option('wwaudio_mpg123') and _ww_backend != 'stub' and _ww_backend != 'mss'
    _renegade_deps += dependency('libmpg123', required : true)
  endif
endif
_renegade_link = _is_linux ? (_link + linux_link_args) : (_link + ['-mwindows'] + win_link_args + renegade_exe_link_tail)

renegade_exe = executable(
  'renegade',
  commando_sources,
  commando_chat_res,
  include_directories : inc,
  dependencies : _renegade_deps,
  cpp_args : _cpp,
  link_args : _renegade_link,
  link_with : [
    combat_lib, wwui_lib, wwaudio_lib, ww3d2_lib, wwphys_lib,
    wwnet_lib, wwonline_lib, wwtranslatedb_lib, wwsaveload_lib, wwmath_lib,
    wwutil_lib, wwbitpack_lib, binkmovie_lib, scontrol_lib,
    wwlib_lib, wwdebug_lib,
  ],
  link_whole : _is_linux ? [platform_lib] : [],
  win_subsystem : _is_linux ? 'console' : 'windows',
)

_build_root = meson.project_build_root()
_deploy_cmds = [
  'set -e',
  'd=' + _build_root,
  'cp "' + renegade_exe.full_path() + '" "$d/"',
  'cp "' + scripts_dll.full_path() + '" "$d/"',
]

if not _is_linux and get_option('wwaudio_backend') == 'mss'
  _miles_root = meson.project_source_root() / 'third_party/milesss-v9.3b-main/win'
  _miles_redist = _miles_root / 'sdk/redist'
  _miles_mp3 = _miles_root / 'mp3/redist'
  _deploy_cmds += [
    'cp "' + (_miles_redist / 'mss32.dll') + '" "$d/"',
    'cp "' + (_miles_redist / 'mssds3d.flt') + '" "$d/"',
    'cp "' + (_miles_redist / 'mssdsp.flt') + '" "$d/"',
    'cp "' + (_miles_redist / 'msseax.flt') + '" "$d/"',
    'cp "' + (_miles_mp3 / 'mssmp3.asi') + '" "$d/"',
  ]
endif

_deploy_cmds += ['touch "@OUTPUT@"']

custom_target(
  'deploy_game',
  input : [renegade_exe, scripts_dll],
  output : 'deploy.stamp',
  command : [find_program('bash'), '-c', '; '.join(_deploy_cmds)],
  build_by_default : true,
)
