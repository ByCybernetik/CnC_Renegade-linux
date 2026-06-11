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
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Combat																		  *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/Commando/dlgmovieoptions.cpp       $*
 *                                                                                             *
 *                       Author:: Patrick Smith                                                *
 *                                                                                             *
 *                     $Modtime:: 1/15/02 9:28p                                               $*
 *                                                                                             *
 *                    $Revision:: 7                                                           $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "dlgmovieoptions.h"
#include "listctrl.h"
#include "binkmovie.h"
#include "registry.h"
#include "translatedb.h"
#include "_globals.h"
#include "string_ids.h"
#include "wwaudio.h"
#include "gamemode.h"
#include "gamemenu.h"

#include <windows.h>
#if defined(RENEGADE_LINUX)
#include <strings.h>
#endif

static void MovieOptions_Set_Menu_Music_Suppressed (bool suppressed)
{
	GameModeClass *mode = GameModeManager::Find ("Menu");
	if (mode != NULL) {
		static_cast<MenuGameModeClass2 *>(mode)->Set_Menu_Music_Suppressed (suppressed);
	}
}

static void MovieOptions_Stop_Movie_Playback (void)
{
	WWAudioClass::Get_Instance ()->Temp_Disable_Audio (false);
	BINKMovie::Stop ();
	MovieOptions_Set_Menu_Music_Suppressed (false);
}

static bool MovieOptions_Paths_Equal(const char *a, const char *b)
{
	if (a == NULL || b == NULL) {
		return false;
	}
#if defined(RENEGADE_LINUX)
	return strcasecmp(a, b) == 0;
#else
	return ::stricmp(a, b) == 0;
#endif
}

static bool MovieOptions_List_Has_Path(
	ListCtrlClass *list_ctrl, DynamicVectorClass<StringClass> *paths, const char *path)
{
	if (list_ctrl == NULL || paths == NULL || path == NULL) {
		return false;
	}

	for (int index = 0; index < list_ctrl->Get_Entry_Count(); ++index) {
		const int path_index = (int)list_ctrl->Get_Entry_Data(index, 0);
		if (path_index >= 0 && path_index < paths->Count()) {
			if (MovieOptions_Paths_Equal((*paths)[path_index].Peek_Buffer(), path)) {
				return true;
			}
		}
	}
	return false;
}

static void MovieOptions_Add_Movie_Entry(
	ListCtrlClass *list_ctrl, DynamicVectorClass<StringClass> *paths, const WCHAR *label, const char *path)
{
	int item_index;

	if (list_ctrl == NULL || paths == NULL || label == NULL || path == NULL) {
		return;
	}
	if (MovieOptions_List_Has_Path(list_ctrl, paths, path)) {
		return;
	}

	item_index = list_ctrl->Insert_Entry(0xFF, label);
	if (item_index != -1) {
		const int path_index = paths->Count();
		paths->Add(StringClass(path));
		list_ctrl->Set_Entry_Data(item_index, 0, (uintptr_t)path_index);
	}
}

static const WCHAR *MovieOptions_Lookup_Known_Label(const char *path)
{
	if (MovieOptions_Paths_Equal(path, "DATA\\MOVIES\\R_INTRO.BIK")) {
		return TRANSLATE(IDS_INTRO_MOVIE);
	}
	if (MovieOptions_Paths_Equal(path, "DATA\\MOVIES\\R_FINALE.BIK")) {
		return TRANSLATE(IDS_FINALE_MOVIE);
	}
	return NULL;
}

#if defined(RENEGADE_LINUX)
static void MovieOptions_Scan_Disk_Movies(
	ListCtrlClass *list_ctrl, DynamicVectorClass<StringClass> *paths)
{
	static const char *scan_patterns[] = {
		"Data/Movies/*.bik",
		"Data/Movies/*.BIK",
		"DATA/MOVIES/*.bik",
		"DATA/MOVIES/*.BIK",
		NULL
	};
	WIN32_FIND_DATAA find_data;
	WideStringClass label(0, true);
	int pattern;

	if (list_ctrl == NULL || paths == NULL) {
		return;
	}

	for (pattern = 0; scan_patterns[pattern] != NULL; ++pattern) {
		HANDLE find_handle = FindFirstFileA(scan_patterns[pattern], &find_data);
		if (find_handle == INVALID_HANDLE_VALUE) {
			continue;
		}

		do {
			StringClass path(128, true);
			const WCHAR *known_label;

			if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				continue;
			}

			path.Format("DATA\\MOVIES\\%s", find_data.cFileName);
			known_label = MovieOptions_Lookup_Known_Label(path.Peek_Buffer());
			if (known_label != NULL) {
				MovieOptions_Add_Movie_Entry(list_ctrl, paths, known_label, path.Peek_Buffer());
				continue;
			}

			label.Format(L"%hs", find_data.cFileName);
			MovieOptions_Add_Movie_Entry(list_ctrl, paths, label, path.Peek_Buffer());
		} while (FindNextFileA(find_handle, &find_data));

		FindClose(find_handle);
	}
}
#endif

////////////////////////////////////////////////////////////////
//
//	MovieOptionsMenuClass
//
////////////////////////////////////////////////////////////////
MovieOptionsMenuClass::MovieOptionsMenuClass (void)	:
	IsPlaying (false),
	MenuDialogClass (IDD_OPTIONS_MOVIES)
{
	return ;
}


////////////////////////////////////////////////////////////////
//
//	On_Init_Dialog
//
////////////////////////////////////////////////////////////////
void
MovieOptionsMenuClass::On_Init_Dialog (void)
{
	//
	//	Get a pointer to the list control
	//
	ListCtrlClass *list_ctrl = (ListCtrlClass *)Get_Dlg_Item (IDC_LIST_CTRL);
	if (list_ctrl != NULL) {

		MoviePaths.Delete_All();

		//
		//	Configure the list control
		//
		list_ctrl->Add_Column (TRANSLATE (IDS_MOVIE_COL_HEADER), 1.0F, Vector3 (1, 1, 1));

		//
		//	Add the movies to the list...
		//
		const char *INTRO_MOVIE = "DATA\\MOVIES\\R_INTRO.BIK";
		RegistryClass registry(APPLICATION_SUB_KEY_NAME_MOVIES);

		MovieOptions_Add_Movie_Entry(list_ctrl, &MoviePaths, TRANSLATE(IDS_INTRO_MOVIE), INTRO_MOVIE);

		if (registry.Is_Valid()) {
			DynamicVectorClass<StringClass> list;
			registry.Get_Value_List(list);

			for (int index = 0; index < list.Count(); index++) {
				StringClass string_id_des;
				const WCHAR *wide_desc;

				registry.Get_String(list[index], string_id_des);
				wide_desc = TRANSLATE_BY_DESC(string_id_des);
				MovieOptions_Add_Movie_Entry(list_ctrl, &MoviePaths, wide_desc, list[index]);
			}
		}

#if defined(RENEGADE_LINUX)
		MovieOptions_Scan_Disk_Movies(list_ctrl, &MoviePaths);
#endif
	}

	MenuDialogClass::On_Init_Dialog ();
	return ;
}


////////////////////////////////////////////////////////////////
//
//	On_Command
//
////////////////////////////////////////////////////////////////
void
MovieOptionsMenuClass::On_Command (int ctrl_id, int message_id, DWORD param)
{
	if (IsPlaying) {
		return ;
	}

	//
	//	Play the movie if the user clicked the play button
	//
	if (ctrl_id == IDC_MENU_PLAY_MOVIE_BUTTON) {
		Begin_Play_Movie ();
	}

	MenuDialogClass::On_Command (ctrl_id, message_id, param);
	return ;
}


////////////////////////////////////////////////////////////////
//
//	On_ListCtrl_Delete_Entry
//
////////////////////////////////////////////////////////////////
void
MovieOptionsMenuClass::On_ListCtrl_Delete_Entry
(
	ListCtrlClass *list_ctrl,
	int				ctrl_id,
	int				item_index
)
{
	if (IsPlaying) {
		return ;
	}

	if (ctrl_id == IDC_LIST_CTRL) {
		
		list_ctrl->Set_Entry_Data (item_index, 0, 0);
	}

	return ;
}


////////////////////////////////////////////////////////////////
//
//	On_ListCtrl_DblClk
//
////////////////////////////////////////////////////////////////
void
MovieOptionsMenuClass::On_ListCtrl_DblClk
(
	ListCtrlClass *list_ctrl,
	int				ctrl_id,
	int				item_index
)
{
	if (IsPlaying) {
		return ;
	}

	Begin_Play_Movie ();
	return ;
}


////////////////////////////////////////////////////////////////
//
//	Begin_Play_Movie
//
////////////////////////////////////////////////////////////////
void
MovieOptionsMenuClass::Begin_Play_Movie (void)
{
	//
	//	Get a pointer to the list control
	//
	ListCtrlClass *list_ctrl = (ListCtrlClass *)Get_Dlg_Item (IDC_LIST_CTRL);
	if (list_ctrl == NULL) {
		return ;
	}
	
	//
	//	Get the currently selected entry
	//
	int curr_sel = list_ctrl->Get_Curr_Sel ();
	if (curr_sel < 0) {
		if (list_ctrl->Get_Entry_Count () > 0) {
			curr_sel = 0;
		} else {
			return ;
		}
	}

	const int path_index = (int)list_ctrl->Get_Entry_Data (curr_sel, 0);
	if (path_index < 0 || path_index >= MoviePaths.Count ()) {
		return ;
	}

	const char *movie_path = MoviePaths[path_index].Peek_Buffer ();

#if defined(RENEGADE_LINUX)
	// FileFactory/BinkOpen resolve case-insensitive paths on Linux.
	Play_Movie (movie_path);
#else
	//
	//	Play the movie (if it exists locally)
	//
	if (::GetFileAttributes (movie_path) != 0xFFFFFFFF) {
		Play_Movie (movie_path);
	} else {

		//
		//	Strip any path information off the filename
		//
		StringClass filename_only (movie_path, true);
		const char *delimiter = ::strrchr (movie_path, '\\');
		if (delimiter != NULL) {
			filename_only = delimiter + 1;
		}

		//
		//	Try to find the CD...
		//
		StringClass cd_path;
		if (CDVerifier.Get_CD_Path (cd_path)) {

			//
			//	Build a full-path to the movie on the CD
			//
			StringClass full_path = cd_path;
			if (cd_path[cd_path.Get_Length () - 1] != '\\') {
				full_path += "\\";
			}
			full_path += filename_only;
			Play_Movie (full_path);
		} else {
			PendingMovieFilename = filename_only;
			CDVerifier.Display_UI (this);
		}
	}
#endif

	return ;
}


////////////////////////////////////////////////////////////////
//
//	Play_Movie
//
////////////////////////////////////////////////////////////////
void
MovieOptionsMenuClass::Play_Movie (const char *filename)
{
	WWAudioClass::Get_Instance ()->Temp_Disable_Audio (true);
	MovieOptions_Set_Menu_Music_Suppressed (true);
	
	FontCharsClass* font = StyleMgrClass::Get_Font(StyleMgrClass::FONT_INGAME_SUBTITLE_TXT);

	BINKMovie::Play (filename, "data\\subtitle.ini", font);
	
	if (font) {
		font->Release_Ref();
	}

	IsPlaying = true;
	return ;
}


////////////////////////////////////////////////////////////////
//
//	Render
//
////////////////////////////////////////////////////////////////
void
MovieOptionsMenuClass::Render (void)
{
	if (IsPlaying) {
		if (BINKMovie::Is_Complete ()) {
			MovieOptions_Stop_Movie_Playback ();
			IsPlaying = false;
		}
		// BINKMovie::Render() is called from GameModeManager after all dialogs.
		// Do not draw the menu here or it covers the video and consumes the frame
		// present slot before GameModeManager can render the movie.
		return ;
	}

	MenuDialogClass::Render ();
	return ;
}


////////////////////////////////////////////////////////////////
//
//	On_Frame_Update
//
////////////////////////////////////////////////////////////////
void
MovieOptionsMenuClass::On_Frame_Update (void)
{
	//
	//	Let the movie update (if nececessary)
	//
	BINKMovie::Update ();

	MenuDialogClass::On_Frame_Update ();
	return ;
}


////////////////////////////////////////////////////////////////
//
//	On_Key_Down
//
////////////////////////////////////////////////////////////////
bool
MovieOptionsMenuClass::On_Key_Down (uint32 key_id, uint32 key_data)
{
	bool retval = false;
	
	//
	//	Stop playing the movie on any keypress
	//
	if (IsPlaying && key_id == VK_ESCAPE) {
		MovieOptions_Stop_Movie_Playback ();
		IsPlaying = false;
		retval = true;
	} else if (IsPlaying == false) {
		retval = MenuDialogClass::On_Key_Down (key_id, key_data);
	}

	return retval;
}


////////////////////////////////////////////////////////////////
//
//	HandleNotification
//
////////////////////////////////////////////////////////////////
void
MovieOptionsMenuClass::HandleNotification (CDVerifyEvent &event)
{
	if (event.Event () == CDVerifyEvent::VERIFIED) {

		//
		//	Get the path to the CD...
		//
		StringClass cd_path;
		if (CDVerifier.Get_CD_Path (cd_path)) {
			
			//
			//	Build a full-path to the movie on the CD
			//
			StringClass full_path = cd_path;
			if (cd_path[cd_path.Get_Length () - 1] != '\\') {
				full_path += "\\";
			}
			full_path += PendingMovieFilename;
			Play_Movie (full_path);
		}		
	}

	PendingMovieFilename = "";
	return ;
}

