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
 *                     $Archive:: /Commando/Code/Combat/evasettings.h         $* 
 *                                                                                             * 
 *                      $Author:: Greg_h                                                      $* 
 *                                                                                             * 
 *                     $Modtime:: 10/01/01 10:49a                                             $* 
 *                                                                                             * 
 *                    $Revision:: 5                                                           $* 
 *                                                                                             * 
 *---------------------------------------------------------------------------------------------* 
 * Functions:                                                                                  * 
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifndef	EVASETTINGS_H
#define	EVASETTINGS_H

#include "always.h"
#include "definition.h"
#include "rect.h"
#include "ww3d.h"


//////////////////////////////////////////////////////////////////////////
//	Physical display resolution for in-game EVA overlays (HUD path).
//////////////////////////////////////////////////////////////////////////
inline RectClass
Get_Eva_Screen_Resolution (void)
{
	int width = 0;
	int height = 0;
	int bits = 0;
	bool windowed = false;
	WW3D::Get_Device_Resolution (width, height, bits, windowed);
	return RectClass (0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
}


//////////////////////////////////////////////////////////////////////////
//	Map normalized EVA UI coords through a centered 4:3 canvas on widescreen.
//////////////////////////////////////////////////////////////////////////
inline RectClass
Map_Eva_Normalized_Rect (const RectClass &normalized_rect)
{
	RectClass adjusted_rect = normalized_rect;

	int width = 0;
	int height = 0;
	int bits = 0;
	bool windowed = false;
	WW3D::Get_Device_Resolution (width, height, bits, windowed);

	float scale_w = static_cast<float>(width);
	float scale_h = static_cast<float>(height);
	float x_offset = 0.0f;

	const float physical_w = static_cast<float>(width);
	const float physical_h = static_cast<float>(height);
	if (physical_w * 3.0f > physical_h * 4.0f) {
		scale_w = physical_h * (4.0f / 3.0f);
		scale_h = physical_h;
		x_offset = (physical_w - scale_w) * 0.5f;
	}

	adjusted_rect.Scale (Vector2 (scale_w, scale_h));
	adjusted_rect += Vector2 (x_offset, 0.0f);
	adjusted_rect.Left	= float (int(adjusted_rect.Left + 0.5F));
	adjusted_rect.Top		= float (int(adjusted_rect.Top + 0.5F));
	adjusted_rect.Right	= float (int(adjusted_rect.Right + 0.5F));
	adjusted_rect.Bottom	= float (int(adjusted_rect.Bottom + 0.5F));
	return adjusted_rect;
}


//////////////////////////////////////////////////////////////////////////
//	Map normalized EVA message UI coords to physical HUD space and center.
//////////////////////////////////////////////////////////////////////////
inline float
Get_Messages_Hud_X_Offset (const RectClass &screen_normalized_rect)
{
	int width = 0;
	int height = 0;
	int bits = 0;
	bool windowed = false;
	WW3D::Get_Device_Resolution (width, height, bits, windowed);

	const float physical_w = static_cast<float>(width);
	const float uncentered_left = screen_normalized_rect.Left * physical_w;
	const float bar_width = (screen_normalized_rect.Right - screen_normalized_rect.Left) * physical_w;
	const float centered_left = (physical_w - bar_width) * 0.5f;

	return centered_left - uncentered_left;
}


inline RectClass
Map_Messages_Hud_Rect (const RectClass &normalized_rect, float x_offset)
{
	int width = 0;
	int height = 0;
	int bits = 0;
	bool windowed = false;
	WW3D::Get_Device_Resolution (width, height, bits, windowed);

	const float physical_w = static_cast<float>(width);
	const float physical_h = static_cast<float>(height);

	RectClass rect (
		normalized_rect.Left * physical_w,
		normalized_rect.Top * physical_h,
		normalized_rect.Right * physical_w,
		normalized_rect.Bottom * physical_h);
	rect += Vector2 (x_offset, 0.0f);

	rect.Left = float (int (rect.Left + 0.5F));
	rect.Top = float (int (rect.Top + 0.5F));
	rect.Right = float (int (rect.Right + 0.5F));
	rect.Bottom = float (int (rect.Bottom + 0.5F));
	return rect;
}


inline Vector2
Map_Messages_Hud_Point (const Vector2 &normalized_point, const RectClass &screen_normalized_rect)
{
	const float x_offset = Get_Messages_Hud_X_Offset (screen_normalized_rect);

	int width = 0;
	int height = 0;
	int bits = 0;
	bool windowed = false;
	WW3D::Get_Device_Resolution (width, height, bits, windowed);

	Vector2 point (
		normalized_point.X * static_cast<float>(width) + x_offset,
		normalized_point.Y * static_cast<float>(height));
	return point;
}


//////////////////////////////////////////////////////////////////////////
//	Map normalized EVA UI points through a centered 4:3 canvas on widescreen.
//////////////////////////////////////////////////////////////////////////
inline Vector2
Map_Eva_Normalized_Point (const Vector2 &normalized_point)
{
	RectClass mapped_rect = Map_Eva_Normalized_Rect (
		RectClass (normalized_point.X, normalized_point.Y, normalized_point.X, normalized_point.Y));
	return mapped_rect.Upper_Left ();
}


///////////////////////////////////////////////////////////////////////////////////////////
//
//	EvaSettingsDefClass
//
///////////////////////////////////////////////////////////////////////////////////////////
class EvaSettingsDefClass : public DefinitionClass
{
public:
	
	//////////////////////////////////////////////////////////////////////////
	//	Public constructors/destructors
	//////////////////////////////////////////////////////////////////////////
	EvaSettingsDefClass (void);
	~EvaSettingsDefClass (void);

	//////////////////////////////////////////////////////////////////////////
	//	Public methods
	//////////////////////////////////////////////////////////////////////////

	//
	//	From DefinitionClass
	//
	virtual uint32								Get_Class_ID (void) const;
	virtual PersistClass *					Create (void) const ;
	virtual bool								Save (ChunkSaveClass &csave);
	virtual bool								Load (ChunkLoadClass &cload);
	virtual const PersistFactoryClass &	Get_Factory (void) const;	

	static EvaSettingsDefClass *			Get_Instance (void)	{ return EvaSettings; }	

	//
	//	Accessors
	//
	RectClass						Get_Objectives_Screen_Rect (void) const;
	RectClass						Get_Objectives_Text_Rect (void) const;
	const RectClass &				Get_Objectives_Endcap_Rect (void) const		{ return ObjectivesEndcapUVRect; }
	const RectClass &				Get_Objectives_Fadeout_Rect (void) const		{ return ObjectivesFadeoutUVRect; }
	const RectClass &				Get_Objectives_Background_Rect (void) const	{ return ObjectivesBackgroundUVRect; }
	const Vector2 &				Get_Objectives_Texture_Size (void) const		{ return ObjectivesTextureSize;; }

	RectClass						Get_Messages_Screen_Rect (void) const;
	RectClass						Get_Messages_Text_Rect (void) const;
	const RectClass &				Get_Messages_Endcap_Rect (void) const			{ return MessagesEndcapUVRect; }
	const RectClass &				Get_Messages_Fadeout_Rect (void) const			{ return MessagesFadeoutUVRect; }
	const RectClass &				Get_Messages_Background_Rect (void) const		{ return MessagesBackgroundUVRect; }
	const Vector2 &				Get_Messages_Texture_Size (void) const			{ return MessagesTextureSize;; }
	Vector2							Get_Messages_Icon_Position (void) const;
	
	//
	//	Editable support
	//
	DECLARE_EDITABLE (EvaSettingsDefClass, DefinitionClass);

protected:
	
	//////////////////////////////////////////////////////////////////////////
	//	Protected member data
	//////////////////////////////////////////////////////////////////////////

	RectClass									ObjectivesScreenRect;
	RectClass									ObjectivesTextRect;
	RectClass									ObjectivesEndcapUVRect;
	RectClass									ObjectivesFadeoutUVRect;
	RectClass									ObjectivesBackgroundUVRect;
	Vector2										ObjectivesTextureSize;

	RectClass									MessagesScreenRect;
	RectClass									MessagesTextRect;
	RectClass									MessagesEndcapUVRect;
	RectClass									MessagesFadeoutUVRect;
	RectClass									MessagesBackgroundUVRect;
	Vector2										MessagesTextureSize;
	Vector2										MessagesIconPos;
	
	static EvaSettingsDefClass *			EvaSettings;
};


//////////////////////////////////////////////////////////////////////////
//	Get_Objectives_Screen_Rect
//////////////////////////////////////////////////////////////////////////
inline RectClass
EvaSettingsDefClass::Get_Objectives_Screen_Rect (void) const
{
	return Map_Eva_Normalized_Rect (ObjectivesScreenRect);
}


//////////////////////////////////////////////////////////////////////////
//	Get_Objectives_Text_Rect
//////////////////////////////////////////////////////////////////////////
inline RectClass
EvaSettingsDefClass::Get_Objectives_Text_Rect (void) const
{
	return Map_Eva_Normalized_Rect (ObjectivesTextRect);
}


//////////////////////////////////////////////////////////////////////////
//	Get_Messages_Screen_Rect
//////////////////////////////////////////////////////////////////////////
inline RectClass
EvaSettingsDefClass::Get_Messages_Screen_Rect (void) const
{
	const float x_offset = Get_Messages_Hud_X_Offset (MessagesScreenRect);
	return Map_Messages_Hud_Rect (MessagesScreenRect, x_offset);
}


//////////////////////////////////////////////////////////////////////////
//	Get_Messages_Text_Rect
//////////////////////////////////////////////////////////////////////////
inline RectClass
EvaSettingsDefClass::Get_Messages_Text_Rect (void) const
{
	const float x_offset = Get_Messages_Hud_X_Offset (MessagesScreenRect);
	return Map_Messages_Hud_Rect (MessagesTextRect, x_offset);
}


//////////////////////////////////////////////////////////////////////////
//	Get_Messages_Icon_Position
//////////////////////////////////////////////////////////////////////////
inline Vector2
EvaSettingsDefClass::Get_Messages_Icon_Position (void) const
{
	return Map_Messages_Hud_Point (MessagesIconPos, MessagesScreenRect);
}


#endif	//	EVASETTINGS_H
