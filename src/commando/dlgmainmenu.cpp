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
 *                     $Archive:: /Commando/Code/Commando/dlgmainmenu.cpp       $*
 *                                                                                             *
 *                       Author:: Patrick Smith                                                *
 *                                                                                             *
 *                     $Modtime:: 2/12/02 1:28p                                               $*
 *                                                                                             *
 *                    $Revision:: 29                                                          $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


#include "dlgmainmenu.h"
#include "assetmgr.h"
#include "rendobj.h"
#include "hanim.h"
#include "htree.h"
#include "gameinitmgr.h"
#include "mainmenutransition.h"
#include "menubackdrop.h"
#include "scene.h"
#include "dialogresource.h"
#include "mesh.h"
#include "meshgeometry.h"
#include "dialogmgr.h"
#include "gameinitmgr.h"
#include "debug.h"
#include "dialogcontrol.h"
#include "specialbuilds.h"
#include "buildnum.h"
#include "campaign.h"
#include "gamedata.h"
#include "imagectrl.h"
#include "init.h"
#include "registry.h"
#include "_globals.h"
#include "dialogtests.h"
#include "dlgwolwait.h"
#include "nicenum.h"
#include "dlgmessagebox.h"
#include "translatedb.h"
#include "string_ids.h"
#include "gamespyadmin.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

namespace
{
static const float MAIN_MENU_TITLE_IDLE_FRAME = 65.0f;
static const float MAIN_MENU_TITLE_OUT_START_FRAME = 66.0f;
static const char *MAIN_MENU_TITLE_ANIM = "IF_TITLETRANS.IF_TITLETRANS";
static const char *MAIN_MENU_GIZMO_ANIM = "IF_EVAGIZMO.IF_EVAGIZMO";
static const char *MAIN_MENU_GIZMO_BONE = "IF_GIZMOBONE";

static void Preload_Menu_Model_W3d (const char *basename)
{
	WW3DAssetManager *asset_mgr = WW3DAssetManager::Get_Instance ();
	if (asset_mgr == NULL || basename == NULL || basename[ 0 ] == '\0') {
		return;
	}

	char filename[ MAX_PATH ];
	snprintf (filename, sizeof (filename), "%s.w3d", basename);
	if (asset_mgr->Load_3D_Assets (filename)) {
		return;
	}

	char lower[ MAX_PATH ];
	strncpy (lower, filename, sizeof (lower) - 1);
	lower[ sizeof (lower) - 1 ] = '\0';

	bool differs = false;
	for (char *p = lower; *p != '\0'; ++p) {
		const char c = (char)tolower ((unsigned char)*p);
		differs = differs || (c != *p);
		*p = c;
	}

	if (differs) {
		asset_mgr->Load_3D_Assets (lower);
	}
}

static HAnimClass *Get_Menu_HAnim (const char *anim_name)
{
	WW3DAssetManager *asset_mgr = WW3DAssetManager::Get_Instance ();
	if (asset_mgr == NULL || anim_name == NULL || anim_name[ 0 ] == '\0') {
		return NULL;
	}

	HAnimClass *anim = asset_mgr->Get_HAnim (anim_name);
	if (anim != NULL && anim->Get_Num_Frames () >= 2) {
		return anim;
	}

	if (anim != NULL) {
		anim->Release_Ref ();
	}

	return NULL;
}

static bool Get_Main_Menu_Gizmo_State (
	RenderObjClass *title_model,
	Matrix3D *out_tm)
{
	if (title_model == NULL || out_tm == NULL) {
		return false;
	}

	const int bone_index = title_model->Get_Bone_Index (MAIN_MENU_GIZMO_BONE);
	const HTreeClass *htree = title_model->Get_HTree ();
	HAnimClass *title_anim = Get_Menu_HAnim (MAIN_MENU_TITLE_ANIM);

	if (bone_index >= 0 && htree != NULL && title_anim != NULL) {
		htree->Simple_Evaluate_Pivot (
			title_anim,
			bone_index,
			MAIN_MENU_TITLE_IDLE_FRAME,
			title_model->Get_Transform (),
			out_tm);
	} else {
		*out_tm = title_model->Get_Bone_Transform (MAIN_MENU_GIZMO_BONE);
	}

	if (title_anim != NULL) {
		REF_PTR_RELEASE (title_anim);
	}

	return true;
}

static void Ensure_Gizmo_Loop_Animation (RenderObjClass *gizmo_model)
{
	if (gizmo_model == NULL) {
		return;
	}

	HAnimClass *gizmo_anim = Get_Menu_HAnim (MAIN_MENU_GIZMO_ANIM);
	if (gizmo_anim != NULL) {
		gizmo_model->Set_Animation (gizmo_anim, 0.0F, RenderObjClass::ANIM_MODE_LOOP);
		REF_PTR_RELEASE (gizmo_anim);
	}
}

} // namespace

////////////////////////////////////////////////////////////////
//	Static member initialization
////////////////////////////////////////////////////////////////
MainMenuDialogClass *	MainMenuDialogClass::_TheInstance	= NULL;
bool MainMenuDialogClass::Animated = true;

////////////////////////////////////////////////////////////////
//
//	MainMenuDialogClass
//
////////////////////////////////////////////////////////////////
MainMenuDialogClass::MainMenuDialogClass (void)	:
	MenuDialogClass (IDD_MENU_MAIN),
	TitleTransModel (NULL),
	LogoModel (NULL),
	GizmoModel (NULL),
	IsStartingPractice (false)
{
	Preload_Menu_Model_W3d ("IF_TITLETRANS");
	Preload_Menu_Model_W3d ("IF_EVAGIZMO");
	Preload_Menu_Model_W3d ("IF_RENLOGO");

	LogoModel			= WW3DAssetManager::Get_Instance ()->Create_Render_Obj ("IF_RENLOGO");
	TitleTransModel	= WW3DAssetManager::Get_Instance ()->Create_Render_Obj ("IF_TITLETRANS");
	GizmoModel			= WW3DAssetManager::Get_Instance ()->Create_Render_Obj ("IF_EVAGIZMO");

	RegistryClass reg(APPLICATION_SUB_KEY_NAME_OPTIONS);
	if (reg.Get_Int("DisableMenuAnim", 0) != 0) {
		Animated = false;
	}

	_TheInstance = this;

	return ;
}


////////////////////////////////////////////////////////////////
//
//	Update_Main_Menu_Title_Model
//
////////////////////////////////////////////////////////////////
void
MainMenuDialogClass::Update_Main_Menu_Title_Model (void)
{
	if (!Animated || TitleTransModel == NULL) {
		return;
	}

	HAnimClass *title_anim = Get_Menu_HAnim (MAIN_MENU_TITLE_ANIM);
	if (title_anim != NULL) {
		TitleTransModel->Set_Animation (
			title_anim,
			MAIN_MENU_TITLE_IDLE_FRAME,
			RenderObjClass::ANIM_MODE_MANUAL);
		REF_PTR_RELEASE (title_anim);
	}
}


////////////////////////////////////////////////////////////////
//
//	Update_Gizmo_Model
//
////////////////////////////////////////////////////////////////
void
MainMenuDialogClass::Update_Gizmo_Model (float title_trans_frame)
{
	if (!Animated || GizmoModel == NULL || TitleTransModel == NULL || Get_BackDrop () == NULL) {
		return;
	}

	SimpleSceneClass *scene = Get_BackDrop ()->Peek_Scene ();
	if (scene == NULL) {
		return;
	}

	if (TitleTransModel->Peek_Scene () == NULL) {
		if (GizmoModel->Get_Container () != NULL) {
			TitleTransModel->Remove_Sub_Object (GizmoModel);
		}
		if (GizmoModel->Peek_Scene () != NULL) {
			GizmoModel->Remove ();
		}
		return;
	}

	const bool out_transition = (title_trans_frame >= MAIN_MENU_TITLE_OUT_START_FRAME);
	const bool attached_to_title = (GizmoModel->Get_Container () == TitleTransModel);

	if (out_transition) {
		if (!attached_to_title) {
			if (GizmoModel->Peek_Scene () != NULL) {
				GizmoModel->Remove ();
			}
			TitleTransModel->Add_Sub_Object_To_Bone (GizmoModel, MAIN_MENU_GIZMO_BONE);
			Ensure_Gizmo_Loop_Animation (GizmoModel);
		}
		return;
	}

	if (attached_to_title) {
		TitleTransModel->Remove_Sub_Object (GizmoModel);
	}

	if (GizmoModel->Peek_Scene () == NULL) {
		scene->Add_Render_Object (GizmoModel);
		Ensure_Gizmo_Loop_Animation (GizmoModel);
	}

	Matrix3D gizmo_tm;
	Get_Main_Menu_Gizmo_State (TitleTransModel, &gizmo_tm);
	GizmoModel->Set_Transform (gizmo_tm);
	GizmoModel->Set_Animation_Hidden (false);
	GizmoModel->Set_Hidden (false);

}

////////////////////////////////////////////////////////////////
//
//	On_Frame_Update
//
////////////////////////////////////////////////////////////////
void
MainMenuDialogClass::On_Frame_Update (void)
{
	MenuDialogClass::On_Frame_Update ();
	if (DialogMgrClass::Peek_Transitioning_Dialog () == NULL) {
		Update_Gizmo_Model ();
	}
}


////////////////////////////////////////////////////////////////
//
//	Add_Main_Menu_Title_Model_To_Scene
//
////////////////////////////////////////////////////////////////
void
MainMenuDialogClass::Add_Main_Menu_Title_Model_To_Scene (bool apply_idle_pose)
{
	if (!Animated || TitleTransModel == NULL || Get_BackDrop () == NULL) {
		return;
	}

	if (apply_idle_pose) {
		Update_Main_Menu_Title_Model ();
	}

	if (TitleTransModel->Peek_Scene () == NULL) {
		Get_BackDrop ()->Peek_Scene ()->Add_Render_Object (TitleTransModel);
	}

	Update_Gizmo_Model ();
}


////////////////////////////////////////////////////////////////
//
//	~MainMenuDialogClass
//
////////////////////////////////////////////////////////////////
MainMenuDialogClass::~MainMenuDialogClass (void)
{
	if (TitleTransModel != NULL) {
		TitleTransModel->Remove ();
	}

	if (GizmoModel != NULL) {
		GizmoModel->Remove ();
	}

	if (LogoModel != NULL) {
		LogoModel->Remove ();
	}

	REF_PTR_RELEASE (LogoModel);
	REF_PTR_RELEASE (TitleTransModel);
	REF_PTR_RELEASE (GizmoModel);

	if (_TheInstance == this) {
		_TheInstance = NULL;
	}

	return ;
}


////////////////////////////////////////////////////////////////
//
//	On_Menu_Activate
//
////////////////////////////////////////////////////////////////
void
MainMenuDialogClass::On_Menu_Activate (bool onoff)
{
	if (onoff) {
		Add_Main_Menu_Title_Model_To_Scene ();

		if (LogoModel != NULL && LogoModel->Peek_Scene () == NULL) {
			Get_BackDrop ()->Peek_Scene ()->Add_Render_Object (LogoModel);
		}

		//
		//	Force-shutdown any interfaces that are running... (this could
		// happen if the user navigates through the multiplay menus and then
		// returns to the main menu).
		//
		GameInitMgrClass::Shutdown ();
	} else {
		if (LogoModel != NULL) {
			LogoModel->Remove ();
		}

		if (TitleTransModel != NULL) {
			TitleTransModel->Remove ();
		}

		if (GizmoModel != NULL) {
			GizmoModel->Remove ();
		}
	}

	MenuDialogClass::On_Menu_Activate (onoff);
	return ;
}


////////////////////////////////////////////////////////////////
//
//	On_Init_Dialog
//
////////////////////////////////////////////////////////////////
void
MainMenuDialogClass::On_Init_Dialog (void)
{
	Update_Version_Number ();

#if defined(BETACLIENT) || defined(FREEDEDICATEDSERVER) || defined(MULTIPLAYERDEMO)
	Get_Dlg_Item(IDC_MENU_START_SP_GAME_BUTTON)->Enable(false);
	Get_Dlg_Item(IDC_MENU_START_PRACTICE_GAME_BUTTON)->Enable(false);
#endif

#ifndef BETACLIENT
	if (Get_Dlg_Item (IDC_BETA_TEST_TEXT) != NULL) {
		Get_Dlg_Item (IDC_BETA_TEST_TEXT)->Show (false);
	}
#endif

	ImageCtrlClass *image_ctrl = (ImageCtrlClass *)Get_Dlg_Item (IDC_IMAGE);
	if (image_ctrl != NULL) {
		image_ctrl->Set_Texture ("ESRB_RATING.TGA");
	}

	return ;
}


////////////////////////////////////////////////////////////////
//
//	Get_Transition_In
//
////////////////////////////////////////////////////////////////
DialogTransitionClass *
MainMenuDialogClass::Get_Transition_In (DialogBaseClass *prev_dlg)
{
	MainMenuTransitionClass *transition = NULL;

	//
	//	Add the transition model to the scene
	//
	Add_Main_Menu_Title_Model_To_Scene (false);

	//
	//	Add the logo to the screen
	//
	if (LogoModel != NULL && LogoModel->Peek_Scene () == NULL) {
		Get_BackDrop ()->Peek_Scene ()->Add_Render_Object (LogoModel);
	}

	//
	//	We only want to transition between menu dialogs
	//
	if (prev_dlg == NULL ||
			(prev_dlg != QuitVerificationDialogClass::Get_Instance () &&
			 prev_dlg != DlgWOLWait::Get_Instance ()))
	{
		transition = new MainMenuTransitionClass;
		transition->Set_Model (TitleTransModel);
		transition->Set_Camera (Get_BackDrop ()->Peek_Camera ());
		transition->Set_Type (DialogTransitionClass::SCREEN_IN);
		transition->Set_Dialogs (this, prev_dlg);

		//
		//	Don't do the transition if something is missing
		//
		if (transition->Is_Valid () == false) {
			REF_PTR_RELEASE (transition);
		}
	}

	return transition;
}


////////////////////////////////////////////////////////////////
//
//	Get_Transition_Out
//
////////////////////////////////////////////////////////////////
DialogTransitionClass *
MainMenuDialogClass::Get_Transition_Out (DialogBaseClass *next_dlg)
{
	MainMenuTransitionClass *transition = NULL;

	//
	//	We only want to transition between menu dialogs
	//
	if (	IsStartingPractice == false &&
			(next_dlg == NULL || next_dlg->As_MenuDialogClass () != NULL))
	{
		transition = new MainMenuTransitionClass;
		transition->Set_Model (TitleTransModel);
		transition->Set_Camera (Get_BackDrop ()->Peek_Camera ());
		transition->Set_Type (DialogTransitionClass::SCREEN_OUT);
		transition->Set_Dialogs (this, next_dlg);

		//
		//	Don't do the transition if something is missing
		//
		if (transition->Is_Valid () == false) {
			REF_PTR_RELEASE (transition);
		}
	}

	return transition;
}


////////////////////////////////////////////////////////////////
//
//	Choose_Skirmish_Map
//
////////////////////////////////////////////////////////////////
StringClass
MainMenuDialogClass::Choose_Skirmish_Map (void)
{
	DynamicVectorClass<StringClass>	map_list;
	WIN32_FIND_DATA find_info	= { 0 };
	BOOL keep_going				= TRUE;
	HANDLE file_find				= NULL;
	StringClass file_filter;

	//
	// Look for any skirmish maps.
	//
	file_filter.Format("%sskirmish*.mix", DATA_SUBDIRECTORY);
	keep_going = TRUE;
	for (file_find = ::FindFirstFile (file_filter, &find_info);
		 (file_find != INVALID_HANDLE_VALUE) && keep_going;
		  keep_going = ::FindNextFile (file_find, &find_info))
	{
		map_list.Add (find_info.cFileName);
	}

	if (file_find != INVALID_HANDLE_VALUE) {
		::FindClose (file_find);
	}

	if (map_list.Count() == 0) {
		//
		// No skirmish maps found. Look for a C&C map.
		//
		file_filter.Format("%sc&c_*.mix", DATA_SUBDIRECTORY);
		keep_going = TRUE;
		for (file_find = ::FindFirstFile (file_filter, &find_info);
			 (file_find != INVALID_HANDLE_VALUE) && keep_going;
			  keep_going = ::FindNextFile (file_find, &find_info))
		{
			map_list.Add (find_info.cFileName);
		}

		if (file_find != INVALID_HANDLE_VALUE) {
			::FindClose (file_find);
		}
	}

	StringClass mapname;
	if (map_list.Count() > 0) {
		int choice = rand() % map_list.Count();
		mapname = map_list[choice];
	}

	return mapname;
}


////////////////////////////////////////////////////////////////
//
//	On_Command
//
////////////////////////////////////////////////////////////////
void
MainMenuDialogClass::On_Command (int ctrl_id, int message_id, DWORD param)
{
	bool allow_default = true;

	switch (ctrl_id)
	{
		case IDCANCEL:
			ctrl_id = IDC_MENU_QUIT_BUTTON;
			break;

		case IDC_MENU_START_PRACTICE_GAME_BUTTON:
		{
			StringClass mapname = Choose_Skirmish_Map();

			if (!mapname.Is_Empty()) {
				IsStartingPractice = true;
				const int SKIRMISH_LOAD_MENU_NUMBER	= 96;
				CampaignManager::Select_Backdrop_Number(SKIRMISH_LOAD_MENU_NUMBER);
				GameInitMgrClass::Initialize_Skirmish();

				//
				// We will cycle on the same map until they get tired of practicing.
				//
				WWASSERT(The_Game() != NULL);
				The_Game()->Set_Map_Cycle(0, mapname);

				GameInitMgrClass::Start_Game(mapname, -1, 0);
				IsStartingPractice = false;
			} else {
				DlgMsgBox::DoDialog(
					TRANSLATE(IDS_MENU_MISSING_MAP),
					TRANSLATE(IDS_MP_GAME_TYPE_SKIRMISH));
				allow_default = false;
			}
			break;
		}

		case IDC_MENU_MP_LAN_GAME_BUTTON:
			
			//
			// Clear any gamespyadmin flags
			//
			cGameSpyAdmin::Reset();

			if (cNicEnum::Get_Num_Nics() > 0) {
				GameInitMgrClass::Initialize_LAN ();
			} else {
				DlgMsgBox::DoDialog(
					TRANSLATE(IDS_MP_UNABLE_INITIALIZE_LAN), 
					TRANSLATE(IDS_MP_NO_LAN_IP_ADDRESSES_FOUND));
				allow_default = false;
			}
			break;

		case IDC_MENU_MP_INTERNET_GAME_BUTTON:
			START_DIALOG (InternetMainDialogClass);
			allow_default = false;
			break;

		default:
			break;
	}

	if (allow_default) {
		MenuDialogClass::On_Command (ctrl_id, message_id, param);
	}
	return ;
}


////////////////////////////////////////////////////////////////
//
//	Display
//
////////////////////////////////////////////////////////////////
void
MainMenuDialogClass::Display (void)
{
	//
	//	Create the dialog if necessary, otherwise simply bring it to the front
	//
	if (_TheInstance == NULL) {

		//
		//	Create the dialog
		//
		MainMenuDialogClass *dialog = new MainMenuDialogClass;

		//
		//	Create the backdrop if necessary
		//
		if (Animated) {

			if (dialog->Get_BackDrop ()->Peek_Model () == NULL) {
				dialog->Get_BackDrop ()->Set_Model ("IF_BACK01");
				dialog->Get_BackDrop ()->Set_Animation ("IF_BACK01.IF_BACK01");

				/*RenderObjClass *model = WW3DAssetManager::Get_Instance ()->Create_Render_Obj ("IF_RENLOGO");
				if (model != NULL) {
					dialog->Get_BackDrop ()->Peek_Scene ()->Add_Render_Object(model);
				}*/
			}
		}

		//
		//	Start the dialog
		//
		dialog->Start_Dialog ();
		REF_PTR_RELEASE (dialog);

	} else {
		if (_TheInstance->Is_Active_Menu () == false) {
			DialogMgrClass::Rollback (_TheInstance);
		}
	}

	return ;
}


////////////////////////////////////////////////////////////////
//
//	Update_Version_Number
//
////////////////////////////////////////////////////////////////
void
MainMenuDialogClass::Update_Version_Number (void)
{
	//
	// Shipping builds use the localized version string from Strings.tdb.
	// Build stamp fields are MSVC-only placeholders and are not stamped in this port.
	//
	const WCHAR *version_text = TRANSLATE(IDS_MENU_TEXT514);
	if (version_text == NULL || version_text[0] == L'\0') {
		unsigned long version_major = 1;
		unsigned long version_minor = 0;
		Get_Version_Number(&version_major, &version_minor);

		WideStringClass version_string;
		version_string.Format(
			L"v%d.%.3d",
			(int)(version_major >> 16),
			(int)(version_major & 0xFFFF));
		Set_Dlg_Item_Text(IDC_VERSION_STATIC, version_string);
		return;
	}

	Set_Dlg_Item_Text(IDC_VERSION_STATIC, version_text);
}
























				//TRANSLATE_ME
				//const WCHAR * title	= L"Unable to initialize LAN";
				//IDS_MP_UNABLE_INITIALIZE_LAN
				//const WCHAR * text	= L"No LAN IP addresses found.";
				//IDS_MP_NO_LAN_IP_ADDRESSES_FOUND

				//DlgMsgBox::DoDialog(title, text);

/*
#ifdef MULTIPLAYERDEMO
			START_DIALOG (GameSpyMainDialogClass);
#else
			START_DIALOG (InternetMainDialogClass);
#endif
*/
