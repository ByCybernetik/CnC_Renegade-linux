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
 *                 Project Name : WWSaveLoad                                                   *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/wwsaveload/pointerremap.cpp                  $*
 *                                                                                             *
 *                       Author:: Greg Hjelstrom                                               *
 *                                                                                             *
 *                     $Modtime:: 5/09/01 11:36a                                              $*
 *                                                                                             *
 *                    $Revision:: 9                                                           $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


#include "pointerremap.h"
#include "refcount.h"
#include "wwdebug.h"
#include "saveloadlog.h"

#if defined(__GNUC__) && defined(_WIN32)

static bool debug_ptr_usable(const void *ptr)
{
	return ptr != NULL && (unsigned long)ptr > 0x10000UL;
}
#endif


const int POINTER_TABLES_GROWTH_STEP = 4096;


PointerRemapClass::PointerRemapClass(void)
{
	PointerPairTable.Set_Growth_Step(POINTER_TABLES_GROWTH_STEP);
	PointerRequestTable.Set_Growth_Step(POINTER_TABLES_GROWTH_STEP);
	RefCountRequestTable.Set_Growth_Step(POINTER_TABLES_GROWTH_STEP);
}

PointerRemapClass::~PointerRemapClass(void)
{
}

void PointerRemapClass::Reset(void)
{
	/*
	** Clear() drops storage without Delete_All()'s Clear+Resize(VectorMax).
	** After a large level load VectorMax can be 50k+; Delete_All() immediately
	** reallocates that capacity and has been observed to fault in free() on Wine.
	*/
	PointerPairTable.Clear();
	PointerRequestTable.Clear();
	RefCountRequestTable.Clear();
}

void PointerRemapClass::Process(void)
{
	SAVELOAD_LOG("[REMAP] Process: pairs=%d, requests=%d, refcount_reqs=%d",
		PointerPairTable.Count(), PointerRequestTable.Count(), RefCountRequestTable.Count());
	SAVELOAD_INDENT();

	if ( PointerPairTable.Count() > 0 ) {
		qsort(&PointerPairTable[0], PointerPairTable.Count(), sizeof(PointerPairTable[0]), ptr_pair_compare_function);
	}

	if ( PointerRequestTable.Count() > 0 ) {
		WWASSERT( PointerPairTable.Count() > 0 );
		qsort(&PointerRequestTable[0],PointerRequestTable.Count(), sizeof(PointerRequestTable[0]), ptr_request_compare_function);
		Process_Request_Table(PointerRequestTable,false);
	}

	// remap the ref-counted pointers
	if ( RefCountRequestTable.Count() > 0 ) {
		WWASSERT( PointerPairTable.Count() > 0 );
		qsort(&RefCountRequestTable[0],RefCountRequestTable.Count(), sizeof(RefCountRequestTable[0]), ptr_request_compare_function);
		Process_Request_Table(RefCountRequestTable,true);
	}

	SAVELOAD_UNINDENT();
	SAVELOAD_LOG("[REMAP] Process done");
}

void PointerRemapClass::Process_Request_Table(DynamicVectorClass<PtrRemapStruct> & request_table,bool refcount)
{
	// Remap the pointers
	int pointer_index = 0;
	int pair_index = 0;
	int remap_ok = 0;
	int remap_fail = 0;

	for (pointer_index = 0; pointer_index < request_table.Count(); pointer_index++) {

		void **slot = request_table[pointer_index].PointerToRemap;
#if defined(__GNUC__) && defined(_WIN32)
		if (slot == NULL) {
			continue;
		}
#endif
		void * pointer_to_remap = *slot;
#if defined(__GNUC__) && defined(_WIN32)
		if (!debug_ptr_usable(pointer_to_remap)) {
			*slot = NULL;
			continue;
		}
#endif
		int pre_search_index = pair_index;

		// Find the pair which contains the pointer we are looking for as its "old" pointer
		while (	(pair_index < PointerPairTable.Count()) &&
					(PointerPairTable[pair_index].OldPointer < pointer_to_remap)  ) 
		{
			pair_index++;
		}
	
		if ((pair_index < PointerPairTable.Count()) && (PointerPairTable[pair_index].OldPointer == pointer_to_remap)) {

			// we found the match, plug in the new pointer and add a ref if needed.
			void *new_pointer = PointerPairTable[pair_index].NewPointer;
#if defined(__GNUC__) && defined(_WIN32)
			if (!debug_ptr_usable(new_pointer)) {
				*slot = NULL;
			} else {
				*slot = new_pointer;
				if (refcount) {
					RefCountClass *refptr = (RefCountClass *)new_pointer;
					refptr->Add_Ref();
				}
			}
#else
			*slot = new_pointer;

			if (refcount) {
				RefCountClass * refptr = (RefCountClass *)(*slot);
				if (refptr != NULL) {
					refptr->Add_Ref();
				}
			}
#endif
			remap_ok++;

		} else {
			
			// Failed to re-map the pointer.
			// warn the user, set pointer to NULL, reset index to the pre_search_index.
			// If this happens, things could be going very wrong.  (find out why its happening!)
			pair_index = pre_search_index;
			*slot = NULL;
			remap_fail++;
			SAVELOAD_LOG("[REMAP] FAILED: old_ptr=0x%p slot=0x%p", pointer_to_remap, (void*)slot);
#ifdef WWDEBUG			
			const char * file = request_table[pointer_index].File;
			int line = request_table[pointer_index].Line;
			WWDEBUG_SAY(("Warning! Failed to re-map pointer! old_ptr = 0x%X  file = %s  line = %d\r\n",(unsigned int)pointer_to_remap,file,line));
			SAVELOAD_LOG("[REMAP] FAILED: %s:%d old_ptr=0x%p", file ? file : "?", line, pointer_to_remap);
			WWASSERT( 0 );
#endif
		}
	}

	SAVELOAD_LOG("[REMAP] Table done: ok=%d fail=%d (refcount=%d)", remap_ok, remap_fail, (int)refcount);
}

void PointerRemapClass::Register_Pointer (void *old_pointer, void *new_pointer)
{
#if defined(__GNUC__) && defined(_WIN32)
	if (!debug_ptr_usable(old_pointer) || !debug_ptr_usable(new_pointer)) {
		return;
	}
#endif
	SAVELOAD_LOG("[REMAP] Register_Pointer: 0x%p -> 0x%p (total pairs: %d)",
		old_pointer, new_pointer, PointerPairTable.Count() + 1);
	PointerPairTable.Add(PtrPairStruct(old_pointer,new_pointer));
}

#ifdef WWDEBUG
void PointerRemapClass::Request_Pointer_Remap(void **pointer_to_convert,const char * file,int line)
{
	PtrRemapStruct remap;
	remap.PointerToRemap = pointer_to_convert;
	remap.File = file;
	remap.Line = line;
	PointerRequestTable.Add(remap);
}

void PointerRemapClass::Request_Ref_Counted_Pointer_Remap (RefCountClass **pointer_to_convert,const char * file, int line)
{
	PtrRemapStruct remap;
	remap.PointerToRemap = (void**)pointer_to_convert;
	remap.File = file;
	remap.Line = line;
	RefCountRequestTable.Add(remap);
}

#else

void PointerRemapClass::Request_Pointer_Remap (void **pointer_to_convert)
{
	PtrRemapStruct remap;
	remap.PointerToRemap = pointer_to_convert;
	PointerRequestTable.Add(remap);
}

void PointerRemapClass::Request_Ref_Counted_Pointer_Remap (RefCountClass **pointer_to_convert)
{
	PtrRemapStruct remap;
	remap.PointerToRemap = (void**)pointer_to_convert;
	RefCountRequestTable.Add(remap);
}

#endif

/*
** sort compare function for pointer pair structures
** sorts by the old pointer value
*/
int __cdecl PointerRemapClass::ptr_pair_compare_function(void const * ptr1, void const * ptr2)
{
	void * old1 = ((PointerRemapClass::PtrPairStruct const *)ptr1)->OldPointer;
	void * old2 = ((PointerRemapClass::PtrPairStruct const *)ptr2)->OldPointer;

	if (old1 == old2) {
		return(0);
	}
	if (old1 < old2) {
		return(-1);
	}
	return(1);
}

/*
** sort compare function for pointer remap structures
** sorts by the old pointer value
*/
int __cdecl PointerRemapClass::ptr_request_compare_function(void const * ptr1, void const * ptr2)
{
	PtrRemapStruct * remap1 = (PtrRemapStruct *)ptr1;
	PtrRemapStruct * remap2 = (PtrRemapStruct *)ptr2;
	
	void * old1 = *(remap1->PointerToRemap);
	void * old2 = *(remap2->PointerToRemap);

	if (old1 == old2) {
		return(0);
	}
	if (old1 < old2) {
		return(-1);
	}
	return(1);
}


