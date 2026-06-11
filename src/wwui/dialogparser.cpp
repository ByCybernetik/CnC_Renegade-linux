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
 *                     $Archive:: /Commando/Code/wwui/dialogparser.cpp          $*
 *                                                                                             *
 *                       Author:: Patrick Smith                                                *
 *                                                                                             *
 *                     $Modtime:: 10/25/01 3:54p                                              $*
 *                                                                                             *
 *                    $Revision:: 12                                                          $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


#include "dialogparser.h"
#include "win.h"
#include "osdep.h"
#include "translatedb.h"
#include <commctrl.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#if defined(__GNUC__) && defined(_WIN32)
#include <stdio.h>
#endif

//////////////////////////////////////////////////////////////////////////////
//	Macros
//////////////////////////////////////////////////////////////////////////////
#define ALIGN_WORD_PTR(p) \
	((WORD *)(((uintptr_t)(p) + 1) & ~(uintptr_t)1))
#define ALIGN_DWORD_PTR(p) \
	((DWORD *)(((uintptr_t)(p) + 3) & ~(uintptr_t)3))


#if defined(RENEGADE_LINUX)
/*
 * Dialog templates store custom class names as 16-bit WCHAR strings (e.g. "Button").
 * glibc wide-string helpers assume 32-bit wchar_t and must not be used here.
 */
static bool Dialog_Ascii_Class_Name_Matches(const WCHAR *class_name, const char *ascii_name)
{
	if (class_name == NULL || ascii_name == NULL) {
		return false;
	}

	int index = 0;
	for (; ascii_name[index] != '\0'; ++index) {
		WCHAR ch = class_name[index];
		if (ch == 0) {
			return false;
		}

		char lhs = ascii_name[index];
		char rhs = (char)ch;
		if (lhs >= 'a' && lhs <= 'z') {
			lhs = (char)(lhs - ('a' - 'A'));
		}
		if (rhs >= 'a' && rhs <= 'z') {
			rhs = (char)(rhs - ('a' - 'A'));
		}
		if (lhs != rhs) {
			return false;
		}
	}

	return class_name[index] == 0;
}

static bool Dialog_Class_Name_Contains(const WCHAR *class_name, const char *ascii_substr)
{
	if (class_name == NULL || ascii_substr == NULL || ascii_substr[0] == '\0') {
		return false;
	}

	const int sub_len = (int)strlen(ascii_substr);
	for (int index = 0; class_name[index] != 0; ++index) {
		bool matched = true;
		for (int sub_index = 0; sub_index < sub_len; ++sub_index) {
			WCHAR ch = class_name[index + sub_index];
			if (ch == 0) {
				matched = false;
				break;
			}

			char lhs = ascii_substr[sub_index];
			char rhs = (char)ch;
			if (lhs >= 'a' && lhs <= 'z') {
				lhs = (char)(lhs - ('a' - 'A'));
			}
			if (rhs >= 'a' && rhs <= 'z') {
				rhs = (char)(rhs - ('a' - 'A'));
			}
			if (lhs != rhs) {
				matched = false;
				break;
			}
		}

		if (matched) {
			return true;
		}
	}

	return false;
}

static bool Dialog_Class_Name_Equals(const WCHAR *class_name, const WCHAR *literal)
{
	if (WW_WCSICMP(class_name, literal) == 0) {
		return true;
	}

	StringClass ascii_literal;
	{
		WideStringClass wide_literal = literal;
		wide_literal.Convert_To(ascii_literal);
	}
	return Dialog_Ascii_Class_Name_Matches(class_name, ascii_literal.Peek_Buffer());
}
#endif /* RENEGADE_LINUX */


static void Apply_Dialog_Translation(WCHAR *text_buffer, int buffer_len)
{
	WCHAR *string_id = wcsstr(text_buffer, L"IDS_");
	if (string_id == NULL) {
		return;
	}

	WideStringClass wide_string_id = string_id;
	StringClass ascii_string_id;
	wide_string_id.Convert_To(ascii_string_id);

	WideStringClass translation = TRANSLATE_BY_DESC(ascii_string_id);
	const WCHAR *translation_text = translation.Peek_Buffer();
	if (translation_text != NULL && translation_text[0] != L'\0') {
		int max_chars = buffer_len - (int)(string_id - text_buffer);
		if (max_chars > 0) {
			wcsncpy(string_id, translation_text, max_chars);
			string_id[max_chars - 1] = L'\0';
		}
	}

}


//////////////////////////////////////////////////////////////////////////////
//	Local prototypes
//////////////////////////////////////////////////////////////////////////////
WORD *Skip_Dlg_Field (WORD *src, WCHAR *buffer = NULL, int buffer_len = 0, WORD *ctrl_type = NULL);


//////////////////////////////////////////////////////////////////////////////
//
//	Skip_Dlg_Field
//
//////////////////////////////////////////////////////////////////////////////
WORD *
Skip_Dlg_Field (WORD *src, WCHAR *buffer, int buffer_len, WORD *ctrl_type)
{
	//
	//	These fields always start on the next word boundary, so align
	//	the source pointer on this boundary.
	//
	WORD *retval = ALIGN_WORD_PTR(src);

	//
	//	Note:  The field codes are as follows:
	//
	//		0xFFFF		- The following WORD is an ordinal value of a system class.
	//		0x0000		- Empty field
	//		Otherwise	- The remaining data is a NULL terminated WCHAR string.
	//
	if (*retval == 0xFFFF) {
		
		//
		//	Move past the field designator
		//
		retval ++;
		
		//
		//	Does the user want information about the ctrl type?
		//
		if (ctrl_type != NULL) {
			*ctrl_type = *retval;
		}

		//
		//	Move past the ctrl type identifier
		//
		retval ++;
	} else if (*retval == 0x0000) {
		
		//
		//	Null terminate the string if the user is expecting data
		//
		if (buffer != NULL) {
			*buffer = 0;
		}

		//
		//	Move past the field designator
		//
		retval ++;
	} else {

		//
		//	The following data is a null-terminated string.  Scan
		// as much data into our desination buffer as possible.
		//	Note:  The data is stored in wide character format.
		//
		while (*retval != 0x0000) {
			if (buffer != NULL && buffer_len > 1) {
				
				//
				//	Store this character in the supplied buffer
				// and decrement the remaining buffer length.
				//
				*buffer++ = *retval;
				buffer_len --;
			}
			retval ++;
		}

		//
		//	Ensure the supplied buffer is NULL terminated
		//
		if (buffer != NULL) {
			*buffer = 0;
		}

		//
		//	Advance to the next field
		//
		retval ++;
	}

	//
	//	Return the new buffer position to the caller
	//
	return retval;
}


//////////////////////////////////////////////////////////////////////////////
//
//	Parse_Template
//
//////////////////////////////////////////////////////////////////////////////
void
DialogParserClass::Parse_Template
(
	int															res_id,
	int *															dlg_width,
	int *															dlg_height,
	WideStringClass *											dlg_title,
	DynamicVectorClass<ControlDefinitionStruct> *	control_list
)
{
	//
	//	Load the resource file
	//
	HRSRC resource		= ::FindResource (ProgramInstance, MAKEINTRESOURCE (res_id), RT_DIALOG);
	HGLOBAL hglobal	= NULL;
	LPVOID res_buffer	= NULL;
	if (resource != NULL) {
		hglobal = ::LoadResource (ProgramInstance, resource);
		if (hglobal != NULL) {
			res_buffer = ::LockResource (hglobal);
		}
	}

	if(res_buffer != NULL) {

		//
		//	The first few bytes of the resource buffer are the DLGTEMPLATE structure
		//
		DLGTEMPLATE *dlg_template = (DLGTEMPLATE *)res_buffer;
		(*dlg_width)	= (int)dlg_template->cx;
		(*dlg_height)	= (int)dlg_template->cy;

		//
		//	Move past the DLGTEMPLATE header to the other fields
		//
		WORD *buffer = (WORD *)(((char *)res_buffer) + sizeof (DLGTEMPLATE));
		
		//
		//	Skip the menu, and window class
		//
		buffer = Skip_Dlg_Field (buffer);
		buffer = Skip_Dlg_Field (buffer);

		//
		//	Read the title
		//
		buffer = Skip_Dlg_Field (buffer, dlg_title->Get_Buffer (96), 96);

		WCHAR *string_id = wcsstr (dlg_title->Peek_Buffer (), L"IDS_");
		if (string_id != NULL) {
			WideStringClass wide_string_id = string_id;				
			StringClass ascii_string_id;
			wide_string_id.Convert_To (ascii_string_id);
			(*dlg_title) = TRANSLATE_BY_DESC(ascii_string_id);
		}
		//
		//	Do we need to skip past the font settings?
		//
		if (dlg_template->style & DS_SETFONT) {
			buffer ++;
			while (*buffer != 0x0000) {
				buffer ++;
			}
			buffer ++;
		}

		//
		//	Loop over each control and gather information about them
		//
		for (int index = 0; index < dlg_template->cdit; index ++) {
			DLGITEMTEMPLATE *dlg_item_template = (DLGITEMTEMPLATE *)ALIGN_DWORD_PTR((DWORD *)buffer);
			buffer = (WORD *)(((char *)dlg_item_template) + sizeof (DLGITEMTEMPLATE));

			//
			//	Read the ctrl type
			//
			WCHAR text_buffer[256]	= { 0 };
			WORD ctrl_type				= 0x0000;
			buffer = Skip_Dlg_Field (buffer, text_buffer, 256, &ctrl_type);
			
			//
			//	Wasn't one of the standard types, so see if we can determine
			// what it is by its class name.
			//
			if (ctrl_type == 0) {
#if defined(RENEGADE_LINUX)
				WW_WCSUPR(text_buffer);
				/* CONTROL entries in .rc use string class names (e.g. "Button"),
				 * not 0xFFFF ordinal prefixes — map them to CONTROL_TYPE values. */
				if (Dialog_Class_Name_Equals(text_buffer, L"BUTTON") ||
						Dialog_Ascii_Class_Name_Matches(text_buffer, "BUTTON")) {
					ctrl_type = BUTTON;
				} else if (Dialog_Class_Name_Equals(text_buffer, L"STATIC") ||
						Dialog_Ascii_Class_Name_Matches(text_buffer, "STATIC")) {
					ctrl_type = STATIC;
				} else if (Dialog_Class_Name_Equals(text_buffer, L"EDIT") ||
						Dialog_Ascii_Class_Name_Matches(text_buffer, "EDIT")) {
					ctrl_type = EDIT;
				} else if (Dialog_Class_Name_Equals(text_buffer, L"COMBOBOX") ||
						Dialog_Ascii_Class_Name_Matches(text_buffer, "COMBOBOX")) {
					ctrl_type = COMBOBOX;
				} else if (Dialog_Class_Name_Equals(text_buffer, L"LISTBOX") ||
						Dialog_Ascii_Class_Name_Matches(text_buffer, "LISTBOX")) {
					ctrl_type = LIST_BOX;
				} else if (Dialog_Class_Name_Equals(text_buffer, L"SCROLLBAR") ||
						Dialog_Ascii_Class_Name_Matches(text_buffer, "SCROLLBAR")) {
					ctrl_type = SCROLL_BAR;
				} else if (wcsstr(text_buffer, L"TRACKBAR") != NULL ||
						Dialog_Class_Name_Contains(text_buffer, "TRACKBAR")) {
					ctrl_type = SLIDER;
				} else if (wcsstr(text_buffer, L"TABCONTROL") != NULL ||
						Dialog_Class_Name_Contains(text_buffer, "TABCONTROL")) {
					ctrl_type = TAB;
				} else if (wcsstr(text_buffer, L"LISTVIEW") != NULL ||
						Dialog_Class_Name_Contains(text_buffer, "LISTVIEW")) {
					ctrl_type = LIST_CTRL;
				} else if (wcsstr(text_buffer, L"MAP") != NULL ||
						Dialog_Class_Name_Contains(text_buffer, "MAP")) {
					ctrl_type = MAP;
				} else if (wcsstr(text_buffer, L"VIEWER") != NULL ||
						Dialog_Class_Name_Contains(text_buffer, "VIEWER")) {
					ctrl_type = VIEWER;
				} else if (wcsstr(text_buffer, L"HOTKEY") != NULL ||
						Dialog_Class_Name_Contains(text_buffer, "HOTKEY")) {
					ctrl_type = HOTKEY;
				} else if (wcsstr(text_buffer, L"SHORTCUTBAR") != NULL ||
						Dialog_Class_Name_Contains(text_buffer, "SHORTCUTBAR")) {
					ctrl_type = SHORTCUT_BAR;
				} else if (wcsstr(text_buffer, L"MERCHANDISE") != NULL ||
						Dialog_Class_Name_Contains(text_buffer, "MERCHANDISE")) {
					ctrl_type = MERCHANDISE_CTRL;
				} else if (wcsstr(text_buffer, L"TREEVIEW") != NULL ||
						Dialog_Class_Name_Contains(text_buffer, "TREEVIEW")) {
					ctrl_type = TREE_CTRL;
				} else if (Dialog_Class_Name_Equals(text_buffer, PROGRESS_CLASSW)) {
					ctrl_type = PROGRESS_BAR;
				} else if (wcsstr(text_buffer, L"HEALTHBAR") != NULL ||
						Dialog_Class_Name_Contains(text_buffer, "HEALTHBAR")) {
					ctrl_type = HEALTH_BAR;
				}
#else
				::_wcsupr (text_buffer);
				if (::wcsicmp (text_buffer, L"BUTTON") == 0) {
					ctrl_type = BUTTON;
				} else if (::wcsicmp (text_buffer, L"STATIC") == 0) {
					ctrl_type = STATIC;
				} else if (::wcsicmp (text_buffer, L"EDIT") == 0) {
					ctrl_type = EDIT;
				} else if (::wcsicmp (text_buffer, L"COMBOBOX") == 0) {
					ctrl_type = COMBOBOX;
				} else if (::wcsicmp (text_buffer, L"LISTBOX") == 0) {
					ctrl_type = LIST_BOX;
				} else if (::wcsicmp (text_buffer, L"SCROLLBAR") == 0) {
					ctrl_type = SCROLL_BAR;
				} else if (::wcsstr (text_buffer, L"TRACKBAR") != 0) {
					ctrl_type = SLIDER;
				} else if (::wcsstr (text_buffer, L"TABCONTROL") != 0) {
					ctrl_type = TAB;
				} else if (::wcsstr (text_buffer, L"LISTVIEW") != 0) {
					ctrl_type = LIST_CTRL;
				} else if (::wcsstr (text_buffer, L"MAP") != 0) {
					ctrl_type = MAP;
				} else if (::wcsstr (text_buffer, L"VIEWER") != 0) {
					ctrl_type = VIEWER;
				} else if (::wcsstr (text_buffer, L"HOTKEY") != 0) {
					ctrl_type = HOTKEY;
				} else if (::wcsstr (text_buffer, L"SHORTCUTBAR") != 0) {
					ctrl_type = SHORTCUT_BAR;
				} else if (::wcsstr (text_buffer, L"MERCHANDISE") != 0) {
					ctrl_type = MERCHANDISE_CTRL;
				} else if (::wcsstr (text_buffer, L"TREEVIEW") != 0) {
					ctrl_type = TREE_CTRL;
				} else if (::wcsicmp(text_buffer, PROGRESS_CLASSW) == 0) {
					ctrl_type = PROGRESS_BAR;
				} else if (::wcsstr (text_buffer, L"HEALTHBAR") != 0) {
					ctrl_type = HEALTH_BAR;
				}
#endif
			}

			//
			//	Read the window text
			//			
			buffer = Skip_Dlg_Field (buffer, text_buffer, 256);

#if defined(RENEGADE_LINUX)
			Apply_Dialog_Translation(text_buffer, 256);
#else
			{
				WCHAR *string_id = wcsstr (text_buffer, L"IDS_");
				if (string_id != NULL) {
					WideStringClass wide_string_id = string_id;
					StringClass ascii_string_id;
					wide_string_id.Convert_To (ascii_string_id);
					WideStringClass translation = TRANSLATE_BY_DESC(ascii_string_id);
					wcscpy (string_id, translation);
				}
			}
#endif

			//
			//	Add this control definition to the list
			//
			ControlDefinitionStruct definition;
			definition.id		= (int)dlg_item_template->id;
			definition.style	= dlg_item_template->style;
			definition.x		= dlg_item_template->x;
			definition.y		= dlg_item_template->y;
			definition.cx		= dlg_item_template->cx;
			definition.cy		= dlg_item_template->cy;
			definition.type	= (CONTROL_TYPE)ctrl_type;
			definition.title	= text_buffer;
			control_list->Add (definition);

			//
			//	Skip past the extra data
			//
			WORD extra_data_size = *buffer;
			buffer ++;
			if (extra_data_size > 0) {
				buffer = (WORD *)(((char *)ALIGN_WORD_PTR(buffer)) + extra_data_size);
			}
		}
	}


	return ;
}

