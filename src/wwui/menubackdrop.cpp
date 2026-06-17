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
 *                     $Archive:: /Commando/Code/wwui/menubackdrop.cpp         $*
 *                                                                                             *
 *                       Author:: Patrick Smith                                                *
 *                                                                                             *
 *                     $Modtime:: 11/21/01 11:30a                                             $*
 *                                                                                             *
 *                    $Revision:: 8                                                           $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "menubackdrop.h"
#include "menuviewport.h"
#include "dx8wrapper.h"
#include "scene.h"
#include "camera.h"
#include "ww3d.h"
#include "assetmgr.h"
#include "render2d.h"
#include "light.h"
#include "hanim.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static bool backdrop_try_load_w3d_file( WW3DAssetManager *asset_mgr, const char *filename )
{
	char lower[ MAX_PATH ];
	bool differs = false;
	int i;

	if ( asset_mgr == NULL || filename == NULL || filename[ 0 ] == '\0' ) {
		return false;
	}

	if ( asset_mgr->Load_3D_Assets( filename ) ) {
		return true;
	}

	strncpy( lower, filename, sizeof( lower ) - 1 );
	lower[ sizeof( lower ) - 1 ] = '\0';
	for ( i = 0; lower[ i ] != '\0'; i++ ) {
		char c = (char)tolower( (unsigned char)lower[ i ] );
		if ( c != lower[ i ] ) {
			differs = true;
		}
		lower[ i ] = c;
	}
	if ( differs && asset_mgr->Load_3D_Assets( lower ) ) {
		return true;
	}

	return false;
}


static void backdrop_preload_model_w3d(const char *model_or_anim_name)
{
	const char *dot;
	char filename[MAX_PATH];
	WW3DAssetManager *asset_mgr;
	int prefix_len;
	bool loaded = false;

	if (model_or_anim_name == NULL || model_or_anim_name[0] == '\0') {
		return;
	}

	asset_mgr = WW3DAssetManager::Get_Instance();
	if (asset_mgr == NULL) {
		return;
	}

	dot = strchr(model_or_anim_name, '.');
	if (dot != NULL) {
		prefix_len = (int)(dot - model_or_anim_name);
		if (prefix_len <= 0 || prefix_len >= (int)sizeof(filename)) {
			return;
		}
		memcpy(filename, model_or_anim_name, (size_t)prefix_len);
		filename[prefix_len] = '\0';
		strncat(filename, ".w3d", sizeof(filename) - strlen(filename) - 1);
	} else {
		snprintf(filename, sizeof(filename), "%s.w3d", model_or_anim_name);
	}

	loaded = backdrop_try_load_w3d_file( asset_mgr, filename );
	if ( !loaded ) {
		char alt[ MAX_PATH ];
		snprintf( alt, sizeof( alt ), "../%s", filename );
		loaded = backdrop_try_load_w3d_file( asset_mgr, alt );
		if ( !loaded ) {
			snprintf( alt, sizeof( alt ), "..\\%s", filename );
			loaded = backdrop_try_load_w3d_file( asset_mgr, alt );
		}
	}
}


static int backdrop_anim_name_score(
	const char *item_name,
	const char *anim_name,
	const char *basename,
	int basename_len)
{
	const char *dot;

	if (item_name == NULL || anim_name == NULL || basename == NULL || basename_len <= 0) {
		return 0;
	}

	if (::stricmp(item_name, anim_name) == 0) {
		return 100;
	}

	if (::strnicmp(item_name, basename, basename_len) == 0) {
		dot = item_name + basename_len;
		if (dot[0] == '\0' || dot[0] == '.') {
			return 80;
		}
	}

	return 0;
}


static HAnimClass *backdrop_resolve_hanim(const char *anim_name)
{
	WW3DAssetManager *asset_mgr;
	HAnimClass *anim;
	const char *dot;
	char basename[256];
	char best_name[256];
	int basename_len = 0;
	int best_score = 0;
	int best_frames = 0;

	if (anim_name == NULL || anim_name[0] == '\0') {
		return NULL;
	}

	asset_mgr = WW3DAssetManager::Get_Instance();
	if (asset_mgr == NULL) {
		return NULL;
	}

	dot = strchr(anim_name, '.');
	if (dot != NULL) {
		basename_len = (int)(dot - anim_name);
	} else {
		basename_len = (int)strlen(anim_name);
	}

	if (basename_len > 0 && basename_len < (int)sizeof(basename)) {
		memcpy(basename, anim_name, (size_t)basename_len);
		basename[basename_len] = '\0';
	}

	anim = asset_mgr->Get_HAnim(anim_name);
	if (anim != NULL && anim->Get_Num_Frames() >= 2) {
		return anim;
	}
	if (anim != NULL) {
		anim->Release_Ref();
		anim = NULL;
	}

	if (basename_len > 0 && basename_len < (int)sizeof(basename)) {
		char canonical[256];
		snprintf(canonical, sizeof(canonical), "%s.%s", basename, basename);
		if (::stricmp(anim_name, canonical) != 0) {
			anim = asset_mgr->Get_HAnim(canonical);
			if (anim != NULL && anim->Get_Num_Frames() >= 2) {
				return anim;
			}
			if (anim != NULL) {
				anim->Release_Ref();
				anim = NULL;
			}
		}
	}

	{
		AssetIterator *it = asset_mgr->Create_HAnim_Iterator();
		best_name[0] = '\0';

		if (it != NULL) {
			for (it->First(); !it->Is_Done(); it->Next()) {
				const char *item_name = it->Current_Item_Name();
				int score;
				int frames;

				if (item_name == NULL) {
					continue;
				}

				score = backdrop_anim_name_score(item_name, anim_name, basename, basename_len);
				if (score < 80) {
					continue;
				}

				{
					HAnimClass *probe = asset_mgr->Get_HAnim(item_name);
					if (probe == NULL) {
						continue;
					}
					frames = probe->Get_Num_Frames();
					probe->Release_Ref();
					if (frames < 2) {
						continue;
					}
				}

				if (score > best_score || (score == best_score && frames > best_frames)) {
					best_score = score;
					best_frames = frames;
					strncpy(best_name, item_name, sizeof(best_name) - 1);
					best_name[sizeof(best_name) - 1] = '\0';
				}
			}
			delete it;
		}

		if (best_name[0] != '\0') {
			anim = asset_mgr->Get_HAnim(best_name);
			if (anim != NULL) {
				return anim;
			}
		}
	}

	return NULL;
}


////////////////////////////////////////////////////////////////
//
//	MenuBackDropClass
//
////////////////////////////////////////////////////////////////
MenuBackDropClass::MenuBackDropClass (void)	:
	Scene (NULL),
	Camera (NULL),
	Model (NULL),
	Anim (NULL),
	ClearScreen (true)
{
	AnimationName = "";
	//
	//	Create a scene to use for the background
	//
	Scene = new SimpleSceneClass;
	Scene->Set_Ambient_Light (Vector3(1, 1, 1));

	//
	// Create a single scene light
	//
	LightClass *light = new LightClass;
	if (light != NULL) {

		//
		// Configure the light
		//
		light->Set_Position (Vector3 (0, 0, 15000.0F));
		light->Set_Intensity (1.0F);
		light->Set_Force_Visible(true);
		light->Set_Flag (LightClass::NEAR_ATTENUATION, false);
		light->Set_Far_Attenuation_Range (1000000, 1000000);
		light->Set_Ambient(Vector3 (0,0,0));
		light->Set_Diffuse (Vector3 (1.0F, 1.0F, 1.0F));
		light->Set_Specular (Vector3 (1.0F, 1.0F, 1.0F));

		//
		// Add this light to the scene
		//
		Scene->Add_Render_Object (light);
		REF_PTR_RELEASE(light);
	}

	//
	//	Create a camera to use in background-scene
	//	
	Camera = new CameraClass();
	Camera->Set_Position (Vector3 (0, 0, 800));

	//
	//	Configure the view plane
	//
	const RectClass &screen_size = Render2DClass::Get_Screen_Resolution ();
	float hfov = DEG_TO_RAD(45.0F);
	float vfov = (screen_size.Height () / screen_size.Width ()) * hfov;
	Camera->Set_View_Plane (hfov, vfov);

	//
	//	Set the clip planes
	//
	Camera->Set_Clip_Planes (5.0F, 12000.0F);
	return ;
}


////////////////////////////////////////////////////////////////
//
//	~MenuBackDropClass
//
////////////////////////////////////////////////////////////////
MenuBackDropClass::~MenuBackDropClass (void)
{
	Remove_Model();
	REF_PTR_RELEASE (Camera);
	REF_PTR_RELEASE (Scene);
	return ;
}


////////////////////////////////////////////////////////////////
//
//	Render
//
////////////////////////////////////////////////////////////////
void
MenuBackDropClass::Render (void)
{
	MenuViewportClass::Apply_To_Camera(Camera);

	//
	//	Simple render the scene
	//
	bool old_static_sort = WW3D::Are_Static_Sort_Lists_Enabled();
	WW3D::Enable_Static_Sort_Lists(false);
	WW3D::Render (Scene, Camera, ClearScreen, ClearScreen);
	WW3D::Enable_Static_Sort_Lists(old_static_sort);
	return ;
}


////////////////////////////////////////////////////////////////
//
//	Set_Model
//
////////////////////////////////////////////////////////////////
void
MenuBackDropClass::Set_Model (const char *name)
{
	//	Get rid of the old model.
	Remove_Model();

	//
	//	Load the new model (and matching .w3d anims for the load-screen progress bar)
	//
	WW3DAssetManager *asset_mgr = WW3DAssetManager::Get_Instance();

	char w3d_filename[ MAX_PATH ];
	bool preload_ok = false;
	if ( name != NULL && name[ 0 ] != '\0' ) {
		snprintf( w3d_filename, sizeof( w3d_filename ), "%s.w3d", name );
		preload_ok = backdrop_try_load_w3d_file( asset_mgr, w3d_filename );
		if ( !preload_ok ) {
			preload_ok = backdrop_try_load_w3d_file( asset_mgr, name );
		}
	}
	Model = asset_mgr->Create_Render_Obj( name );

	if (Model != NULL) {

		//
		//	Check to see if this model has a camera bone
		//
		int camera_bone_index = Model->Get_Bone_Index ("CAMERA");
		if (camera_bone_index > 0) {
			
			//
			// Convert the bone's transform into a camera transform
			//
			const Matrix3D &tm = Model->Get_Bone_Transform (camera_bone_index);
			Matrix3D cam_tm (Vector3 (0, -1, 0), Vector3 (0, 0, 1), Vector3 (-1, 0, 0), Vector3 (0, 0, 0));
			Matrix3D new_tm = tm * cam_tm;

			//
			//	Set the camera's new transform
			//
			Camera->Set_Transform (new_tm);
		}

		//
		//	Add the model to the scene
		//
		Scene->Add_Render_Object (Model);
	}

	// Apply animation only when AnimationName was set before Set_Model (e.g. loading screen).
	if (AnimationName.Get_Length() > 0) {
		Play_Animation();
	}

	return ;
}


////////////////////////////////////////////////////////////////
//
//	Remove_Model
//
////////////////////////////////////////////////////////////////
void
MenuBackDropClass::Remove_Model (void)
{
	if (Model != NULL) {
		Model->Remove();
	}
	REF_PTR_RELEASE (Model);
	REF_PTR_RELEASE (Anim);
}


////////////////////////////////////////////////////////////////
//
//	Set_Animation
//
////////////////////////////////////////////////////////////////
void
MenuBackDropClass::Set_Animation (const char *anim_name)
{
	REF_PTR_RELEASE (Anim);
	AnimationName = anim_name;
	Play_Animation ();
	return ;
}


////////////////////////////////////////////////////////////////
//
//	Set_Animation_Percentage
//
////////////////////////////////////////////////////////////////
void
MenuBackDropClass::Set_Animation_Percentage (float percent)
{
	int num_frames;

	if ( Model == NULL || Anim == NULL ) {
		return;
	}

	num_frames = Anim->Get_Num_Frames();
	if (num_frames < 2) {
		return;
	}

	float frame = (float)(num_frames - 1) * WWMath::Clamp( percent, 0, 1 );
	Model->Set_Animation (Anim, frame, RenderObjClass::ANIM_MODE_MANUAL);
	return ;
}


////////////////////////////////////////////////////////////////
//
//	Play_Animation
//
////////////////////////////////////////////////////////////////
void
MenuBackDropClass::Play_Animation (void)
{
	if (AnimationName.Get_Length () > 0) {
		
		//
		//	Play the animation on the background (if necessary)
		//
		REF_PTR_RELEASE (Anim);
		Anim = backdrop_resolve_hanim(AnimationName);
		if (Anim == NULL && Model != NULL) {
			const char *model_name = Model->Get_Name();
			if (model_name != NULL && model_name[0] != '\0') {
				char model_anim[256];
				snprintf(model_anim, sizeof(model_anim), "%s.%s", model_name, model_name);
				Anim = backdrop_resolve_hanim(model_anim);
			}
		}
		if (Anim == NULL && Model != NULL) {
			HAnimClass *embedded = Model->Peek_Animation();
			if (embedded != NULL) {
				Anim = embedded;
				Anim->Add_Ref();
			}
		}
		if (Model != NULL && Anim != NULL) {
			Model->Set_Animation (Anim, 0, RenderObjClass::ANIM_MODE_LOOP);
		}

	} else if (Model != NULL) {

		//
		//	Stop the animation
		//
		REF_PTR_RELEASE (Anim);
		Model->Set_Animation (NULL);
	}
	
	return ;
}
