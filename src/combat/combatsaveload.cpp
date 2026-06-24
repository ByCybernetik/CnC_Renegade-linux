/*
**	Command & Conquer Renegade(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*********************************************************************************************** 
 ***                            Confidential - Westwood Studios                              *** 
 *********************************************************************************************** 
 *                                                                                             * 
 *                 Project Name : Commando                                                     * 
 *                                                                                             * 
 *                     $Archive:: /Commando/Code/Combat/combatsaveload.cpp                    $* 
 *                                                                                             * 
 *                      $Author:: Byon_g                                                      $* 
 *                                                                                             * 
 *                     $Modtime:: 1/17/02 11:58a                                              $* 
 *                                                                                             * 
 *                    $Revision:: 34                                                          $* 
 *                                                                                             * 
 *---------------------------------------------------------------------------------------------* 
 * Functions:                                                                                  * 
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "combatsaveload.h"
#include "chunkio.h"
#include <stdio.h>
#include "gameobjmanager.h"
#include "combat.h"
#include "debug.h"
#include "spawn.h"
#include "timemgr.h"
#include "scripts.h"
#include "persistentgameobjobserver.h"
#include "wwmemlog.h"
#include "cover.h"
#include "objectives.h"
#include "radar.h"
#include "building.h"
#include "bullet.h"
#include "backgroundmgr.h"
#include "WeatherMgr.h"
#include "weaponview.h"
#include "hud.h"
#include "screenfademanager.h"
#include "gameobjmanager.h"
#include "saveloadlog.h"

/*
**
*/
CombatSaveLoadClass	_CombatSaveLoad;

enum	{
	CHUNKID_GAMEOBJMANAGER					=	916991654,
	CHUNKID_COMBAT_GAME_MODE,
	XXX_CHUNKID_TRANSITIONS,
	CHUNKID_SPAWNERS,
	XXXCHUNKID_TIME,
	CHUNKID_SCRIPTS,
	CHUNKID_PERSISTENT_GAME_OBJ_OBSERVERS,
	CHUNKID_COVER,
	CHUNKID_OBJECTIVES,
	CHUNKID_RADAR,
	XXXCHUNKID_BUILDINGS,
	CHUNKID_GAME_OBJ_OBSERVERS,
	CHUNKID_BULLETS,
	CHUNKID_WEAPON_VIEW,
	CHUNKID_DYNAMIC_BACKGROUND,
	CHUNKID_DYNAMIC_WEATHER,
	CHUNKID_HUD,
	CHUNKID_SCREEN_FADE,
};

/*
**
*/
bool	CombatSaveLoadClass::Save( ChunkSaveClass &csave )
{
	WWMEMLOG(MEM_GAMEDATA);

	SAVELOAD_LOG("[COMBAT_SAVE] Saving combat subsystems...");
	SAVELOAD_INDENT();

#define COMBAT_SAVE_CHUNK(id, name, call) \
	do { \
		SAVELOAD_LOG("[COMBAT_SAVE] chunk 0x%08X: %s", id, name); \
		csave.Begin_Chunk(id); \
		call; \
		csave.End_Chunk(); \
	} while(0)

	COMBAT_SAVE_CHUNK(CHUNKID_GAMEOBJMANAGER, "GameObjManager", GameObjManager::Save(csave));
	COMBAT_SAVE_CHUNK(CHUNKID_COMBAT_GAME_MODE, "CombatManager", CombatManager::Save(csave));
	COMBAT_SAVE_CHUNK(CHUNKID_SPAWNERS, "SpawnManager", SpawnManager::Save(csave));
	COMBAT_SAVE_CHUNK(CHUNKID_SCRIPTS, "ScriptManager", ScriptManager::Save(csave));
	COMBAT_SAVE_CHUNK(CHUNKID_PERSISTENT_GAME_OBJ_OBSERVERS, "PersistentGameObjObserverManager", PersistentGameObjObserverManager::Save(csave));
	COMBAT_SAVE_CHUNK(CHUNKID_COVER, "CoverManager", CoverManager::Save(csave));
	COMBAT_SAVE_CHUNK(CHUNKID_OBJECTIVES, "ObjectiveManager", ObjectiveManager::Save(csave));
	COMBAT_SAVE_CHUNK(CHUNKID_RADAR, "RadarManager", RadarManager::Save(csave));
	COMBAT_SAVE_CHUNK(CHUNKID_GAME_OBJ_OBSERVERS, "GameObjObserverManager", GameObjObserverManager::Save(csave));
	COMBAT_SAVE_CHUNK(CHUNKID_BULLETS, "BulletManager", BulletManager::Save(csave));
	COMBAT_SAVE_CHUNK(CHUNKID_WEAPON_VIEW, "WeaponViewClass", WeaponViewClass::Save(csave));
	COMBAT_SAVE_CHUNK(CHUNKID_DYNAMIC_BACKGROUND, "BackgroundMgrClass::Save_Dynamic", BackgroundMgrClass::Save_Dynamic(csave));
	COMBAT_SAVE_CHUNK(CHUNKID_DYNAMIC_WEATHER, "WeatherMgrClass::Save_Dynamic", WeatherMgrClass::Save_Dynamic(csave));
	COMBAT_SAVE_CHUNK(CHUNKID_HUD, "HUDClass", HUDClass::Save(csave));
	COMBAT_SAVE_CHUNK(CHUNKID_SCREEN_FADE, "ScreenFadeManager", ScreenFadeManager::Save(csave));

#undef COMBAT_SAVE_CHUNK

	SAVELOAD_UNINDENT();
	SAVELOAD_LOG("[COMBAT_SAVE] Done");

	return true;
}

bool	CombatSaveLoadClass::Load( ChunkLoadClass &cload )
{
	WWMEMLOG(MEM_GAMEDATA);

	SAVELOAD_LOG("[COMBAT_LOAD] Loading combat subsystems...");
	SAVELOAD_INDENT();

	while (cload.Open_Chunk()) {
		unsigned int chunk_id = cload.Cur_Chunk_ID();
		SAVELOAD_LOG("[COMBAT_LOAD] chunk 0x%08X", chunk_id);
		SAVELOAD_INDENT();
		switch(chunk_id) {

			case CHUNKID_GAMEOBJMANAGER:
				SAVELOAD_LOG("[COMBAT_LOAD]   -> GameObjManager");
				GameObjManager::Load( cload );
				break;

			case CHUNKID_COMBAT_GAME_MODE:
				SAVELOAD_LOG("[COMBAT_LOAD]   -> CombatManager");
				CombatManager::Load( cload );
				break;

			case CHUNKID_SPAWNERS:
				SAVELOAD_LOG("[COMBAT_LOAD]   -> SpawnManager");
				SpawnManager::Load( cload );
				break;

			case CHUNKID_SCRIPTS:
				SAVELOAD_LOG("[COMBAT_LOAD]   -> ScriptManager");
				ScriptManager::Load( cload );
				break;

			case CHUNKID_PERSISTENT_GAME_OBJ_OBSERVERS:
				SAVELOAD_LOG("[COMBAT_LOAD]   -> PersistentGameObjObserverManager");
				PersistentGameObjObserverManager::Load( cload );
				break;

			case CHUNKID_COVER:
				SAVELOAD_LOG("[COMBAT_LOAD]   -> CoverManager");
				CoverManager::Load( cload );
				break;

			case CHUNKID_OBJECTIVES:
				SAVELOAD_LOG("[COMBAT_LOAD]   -> ObjectiveManager");
				ObjectiveManager::Load( cload );
				break;

			case CHUNKID_RADAR:
				SAVELOAD_LOG("[COMBAT_LOAD]   -> RadarManager");
				RadarManager::Load( cload );
				break;
			
			case CHUNKID_GAME_OBJ_OBSERVERS:
				SAVELOAD_LOG("[COMBAT_LOAD]   -> GameObjObserverManager");
				GameObjObserverManager::Load( cload );
				break;

			case CHUNKID_BULLETS:
				SAVELOAD_LOG("[COMBAT_LOAD]   -> BulletManager");
				BulletManager::Load( cload );
				break;

			case CHUNKID_WEAPON_VIEW:
				SAVELOAD_LOG("[COMBAT_LOAD]   -> WeaponViewClass");
				WeaponViewClass::Load( cload );
				break;

			case CHUNKID_DYNAMIC_BACKGROUND:
				SAVELOAD_LOG("[COMBAT_LOAD]   -> BackgroundMgrClass::Load_Dynamic");
				BackgroundMgrClass::Load_Dynamic( cload );
				break;

			case CHUNKID_DYNAMIC_WEATHER:
				SAVELOAD_LOG("[COMBAT_LOAD]   -> WeatherMgrClass::Load_Dynamic");
				WeatherMgrClass::Load_Dynamic( cload );
				break;

			case CHUNKID_HUD:
				SAVELOAD_LOG("[COMBAT_LOAD]   -> HUDClass");
				HUDClass::Load( cload );
				break;

			case CHUNKID_SCREEN_FADE:
				SAVELOAD_LOG("[COMBAT_LOAD]   -> ScreenFadeManager");
				ScreenFadeManager::Load( cload );
				break;

			default:
				Debug_Say(( "Unrecognized CombatSaveLoad chunkID\n" ));
				SAVELOAD_LOG("[COMBAT_LOAD]   -> UNKNOWN chunk 0x%08X", chunk_id);
				break;

		}
		SAVELOAD_UNINDENT();
		cload.Close_Chunk();
	}

	if (COMBAT_SCENE != NULL) {
		COMBAT_SCENE->Log_Load_Stats();
	}

	SAVELOAD_LOG("[COMBAT_LOAD] Registering post-load callback");
	SaveLoadSystemClass::Register_Post_Load_Callback(this);

	SAVELOAD_UNINDENT();
	SAVELOAD_LOG("[COMBAT_LOAD] Done");

	return true;
}


void	CombatSaveLoadClass::On_Post_Load(void) 
{
	if ( !CombatManager::Is_First_Load() ) {
		GameObjManager::Activate_Cinematic_Freeze( false );
		WeaponViewClass::Reset();
		WeaponViewClass::Enable( false );
		WeaponViewClass::Purge_Orphan_Hands_From_Scene();
	}
}
