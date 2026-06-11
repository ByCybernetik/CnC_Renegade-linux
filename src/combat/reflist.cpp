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
 *                 Project Name : Commando / G                                                 * 
 *                                                                                             * 
 *                     $Archive:: /Commando/Code/Combat/reflist.cpp          $* 
 *                                                                                             * 
 *                      $Author:: Patrick                                                     $* 
 *                                                                                             * 
 *                     $Modtime:: 2/06/01 2:26p                                               $* 
 *                                                                                             * 
 *                    $Revision:: 1                                                           $* 
 *                                                                                             * 
 *---------------------------------------------------------------------------------------------* 
 * Functions:                                                                                  * 
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


#include "reflist.h"
#include "scriptablegameobj.h"

#include <stdint.h>

#if defined(__GNUC__) && defined(_WIN32)
#include <stdio.h>
#endif

static bool heap_ptr_ok(const void *ptr)
{
	if (ptr == NULL) {
		return false;
	}

	const uintptr_t p = (uintptr_t)ptr;

#if UINTPTR_MAX > 0xFFFFFFFFUL
	// 64-bit: legacy 32-bit range checks reject all normal heap pointers.
	if (p < 0x10000UL) {
		return false;
	}
#if defined(__linux__) || defined(__APPLE__)
	if (p >= 0x0000800000000000UL) {
		return false;
	}
#elif defined(_WIN32) || defined(_WIN64)
	if (p >= 0x00007FFFFFFFFFFFUL) {
		return false;
	}
#endif
#else
	// 32-bit Win32/Wine: reject kernel-ish / sentinel values.
	if (p < 0x00100000UL || p >= 0x7F000000UL) {
		return false;
	}

	const unsigned char *bytes = (const unsigned char *)&p;
	if (	bytes[0] >= 0x20 && bytes[0] <= 0x7E &&
			bytes[1] >= 0x20 && bytes[1] <= 0x7E &&
			bytes[2] >= 0x20 && bytes[2] <= 0x7E &&
			bytes[3] >= 0x20 && bytes[3] <= 0x7E) {
		return false;
	}
#endif

	return true;
}

static bool referenceable_ptr_ok(const ReferenceableClass<ScriptableGameObj> *target)
{
	return heap_ptr_ok(target);
}

static bool referencer_ptr_ok(const ReferencerClass *referencer)
{
	return heap_ptr_ok(referencer);
}

static ReferenceableClass<ScriptableGameObj> *referenceable_from_gameobj(const ScriptableGameObj *obj)
{
	if (obj == NULL) {
		return NULL;
	}

	return static_cast<ReferenceableGameObj *>(const_cast<ScriptableGameObj *>(obj));
}


ScriptableGameObj *
ReferencerClass::Get_Ptr (void) const
{
	if (IsLoadPending || !referenceable_ptr_ok(ReferenceTarget)) {
		return NULL;
	}

	return ReferenceTarget->Get_Data();
}


bool	ReferencerClass::Save( ChunkSaveClass & csave )
{
	csave.Begin_Chunk( CHUNKID_REF_VARIABLES );
		WRITE_MICRO_CHUNK( csave, MICROCHUNKID_TARGET, ReferenceTarget );
	csave.End_Chunk();
	return true;
}


bool	ReferencerClass::Load( ChunkLoadClass & cload )
{
	cload.Open_Chunk();
	WWASSERT( cload.Cur_Chunk_ID() == CHUNKID_REF_VARIABLES );

	WWASSERT( ReferenceTarget == NULL );
	WWASSERT( TargetReferencerListNext == NULL );

	while (cload.Open_Micro_Chunk()) {
		switch(cload.Cur_Micro_Chunk_ID()) {
			READ_MICRO_CHUNK( cload, MICROCHUNKID_TARGET, ReferenceTarget );
			default:
//				Debug_Say(( "Unrecognized REFLIST Variable chunkID\n" ));
				break;
		}
		cload.Close_Micro_Chunk();
	}
	cload.Close_Chunk();

	if ( ReferenceTarget != NULL ) {
		REQUEST_POINTER_REMAP( (void **)&ReferenceTarget );
	}

	IsLoadPending = true;
	SaveLoadSystemClass::Register_Post_Load_Callback( this );

	return true;
}


void	ReferencerClass::On_Post_Load(void)	
{
	if ( !IsLoadPending ) {
		return;
	}

	IsLoadPending = false;

	if (!referenceable_ptr_ok(ReferenceTarget)) {
		ReferenceTarget = NULL;
		TargetReferencerListNext = NULL;
		return;
	}

	// if we found our target, re-link to it.  
	if ( ReferenceTarget ) {
		ScriptableGameObj* data = ReferenceTarget->Get_Data();
		ReferenceTarget = NULL;
		TargetReferencerListNext = NULL;
		*this = data;
	}
}


const ReferencerClass & ReferencerClass::operator = ( const ScriptableGameObj * reference_target )
{
	if ( ReferenceTarget != NULL && !referenceable_ptr_ok( ReferenceTarget ) ) {
		ReferenceTarget = NULL;
		TargetReferencerListNext = NULL;
	}

	if ( ReferenceTarget != NULL ) {		// if I currently have a target

		if ( IsLoadPending ) {
			// Stale save pointer — do not dereference before remap/post-load.
			ReferenceTarget = NULL;
			TargetReferencerListNext = NULL;
			IsLoadPending = false;
		} else {
			ReferencerClass *referencer = ReferenceTarget->ReferencerListHead;

			if ( !referencer_ptr_ok( referencer ) ) {
				// Corrupt list — drop without walking.
				ReferenceTarget = NULL;
				TargetReferencerListNext = NULL;
			} else if ( referencer == this ) {	// if I'm the first in the list, fix the head
				ReferenceTarget->ReferencerListHead = TargetReferencerListNext;
				TargetReferencerListNext = NULL;
				ReferenceTarget = NULL;
			} else {
				bool unlinked = false;

				while ( referencer != NULL ) {	// Find me in the list, and remove me
					ReferencerClass *next = referencer->TargetReferencerListNext;
					if ( next == this ) {
						referencer->TargetReferencerListNext = TargetReferencerListNext;
						TargetReferencerListNext = NULL;
						ReferenceTarget = NULL;
						unlinked = true;
						break;
					}
					if ( !referencer_ptr_ok( next ) ) {
						break;
					}
					referencer = next;
				}

				if ( !unlinked ) {
					TargetReferencerListNext = NULL;
					ReferenceTarget = NULL;
				}
			}
		}
	}

	WWASSERT( ReferenceTarget == NULL );

	if ( reference_target != NULL ) {			// if new reference is non-null
		ReferenceableClass<ScriptableGameObj> *referenceable = referenceable_from_gameobj( reference_target );
		if ( !referenceable_ptr_ok( referenceable ) ) {
			return *this;
		}

		IsLoadPending = false;
		ReferenceTarget = referenceable;
		TargetReferencerListNext = ReferenceTarget->ReferencerListHead;
		ReferenceTarget->ReferencerListHead = this;
	}

	return *this;
}
