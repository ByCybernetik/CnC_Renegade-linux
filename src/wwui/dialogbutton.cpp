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
 *                     $Archive:: /Commando/Code/wwui/dialogbutton.cpp          $*
 *                                                                                             *
 *                       Author:: Patrick Smith                                                *
 *                                                                                             *
 *                     $Modtime:: 6/11/01 3:58p                                               $*
 *                                                                                             *
 *                    $Revision:: 10                                                          $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "dialogbutton.h"
#include "assetmgr.h"
#include "font3d.h"
#include "dialogbase.h"
#include "mousemgr.h"
#include "stylemgr.h"

	TextRenderers[0].Reset ();
	TextRenderers[1].Reset ();

	//
	//	Draw the text
	//
	StyleMgrClass::Render_Text (Title, &TextRenderers[0], Rect, true, true, StyleMgrClass::CENTER_JUSTIFY);
	StyleMgrClass::Render_Text (Title, &TextRenderers[1], Rect, true, true, StyleMgrClass::CENTER_JUSTIFY);
	return ;
}


////////////////////////////////////////////////////////////////
//
//	On_Set_Cursor
//
////////////////////////////////////////////////////////////////
void
DialogButtonClass::On_Set_Cursor (const Vector2 &mouse_pos)
{
	//
	//	Change the mouse cursor if necessary
	//
	if (ClientRect.Contains (mouse_pos)) {
		MouseMgrClass::Set_Cursor (MouseMgrClass::CURSOR_ACTION);
	}

	return ;
}


////////////////////////////////////////////////////////////////
//
//	On_Kill_Focus
//
////////////////////////////////////////////////////////////////
void
DialogButtonClass::On_Kill_Focus (DialogControlClass *focus)
{
	WasButtonPressedOnMe	= false;
	IsMouseOverMe			= false;

	DialogControlClass::On_Kill_Focus (focus);
	return ;
}


////////////////////////////////////////////////////////////////
//
//	On_Key_Down
//
////////////////////////////////////////////////////////////////
void
DialogButtonClass::On_Key_Down (uint32 key_id, uint32 key_data)
{
	switch (key_id)
	{
		case VK_RETURN:
		case VK_SPACE:
			Parent->On_Command (ID, BN_CLICKED, 0);
			break;
	}

	return ;
}
