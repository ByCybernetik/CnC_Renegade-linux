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
 *                     $Archive:: /Commando/Code/Combat/ffactorylist.cpp                      $*
 *                                                                                             *
 *                      $Author:: Patrick                                                     $*
 *                                                                                             *
 *                     $Modtime:: 2/21/02 3:22p                                               $*
 *                                                                                             *
 *                    $Revision:: 8                                                           $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "ffactorylist.h"
#include "wwfile.h"
#include <string.h>

/*
** Loose per-state weapon W3D files are often 60-byte stubs; full motion data lives in
** Always.dat / other mix archives. Prefer the largest available copy.
*/
static const int W3D_STUB_PLACEHOLDER_MAX_SIZE = 160;

static int ffactorylist_peek_file_size( FileClass * file )
{
	bool was_open;

	if ( file == NULL || !file->Is_Available() ) {
		return -1;
	}

	was_open = file->Is_Open();
	if ( !was_open ) {
		file->Open();
	}

	int size = file->Size();

	if ( !was_open ) {
		file->Close();
	}

	return size;
}

static bool ffactorylist_is_w3d_filename( const char * filename )
{
	const char * ext;

	if ( filename == NULL ) {
		return false;
	}

	ext = strrchr( filename, '.' );
	if ( ext == NULL ) {
		return false;
	}

	return ( stricmp( ext, ".w3d" ) == 0 );
}

static FileClass * ffactorylist_prefer_archive_over_stub(
	FileFactoryListClass * list,
	const char * filename,
	FileClass * loose_file,
	int loose_factory_index )
{
	int loose_size;
	int best_size;
	int best_factory;
	FileClass * best_file;
	int i;

	if ( list == NULL || loose_file == NULL || loose_factory_index != 0 ) {
		return loose_file;
	}

	if ( !ffactorylist_is_w3d_filename( filename ) ) {
		return loose_file;
	}

	loose_size = ffactorylist_peek_file_size( loose_file );
	if ( loose_size > W3D_STUB_PLACEHOLDER_MAX_SIZE ) {
		return loose_file;
	}

	best_file = loose_file;
	best_size = loose_size;
	best_factory = 0;

	for ( i = 1; i < list->Get_Factory_Count(); i++ ) {
		FileClass * archive_file = list->Get_Factory( i )->Get_File( filename );
		int archive_size;

		if ( archive_file == NULL || !archive_file->Is_Available() ) {
			if ( archive_file != NULL ) {
				list->Get_Factory( i )->Return_File( archive_file );
			}
			continue;
		}

		archive_size = ffactorylist_peek_file_size( archive_file );
		if ( archive_size > best_size ) {
			if ( best_factory != loose_factory_index ) {
				list->Get_Factory( best_factory )->Return_File( best_file );
			}

			best_file = archive_file;
			best_size = archive_size;
			best_factory = i;
		} else {
			list->Get_Factory( i )->Return_File( archive_file );
		}
	}

	if ( best_factory != loose_factory_index ) {
		list->Get_Factory( loose_factory_index )->Return_File( loose_file );
	}

	return best_file;
}


FileFactoryListClass * FileFactoryListClass::Instance = NULL;

/*
**
*/
FileFactoryListClass::FileFactoryListClass( void ) :
	SearchStartIndex( 0 ),
	TempFactory( NULL )
{
	WWASSERT( Instance == NULL );
	Instance = this;
}

FileFactoryListClass::~FileFactoryListClass( void )
{
	WWASSERT( Instance == this );
	Instance = NULL;
}

/*
**
*/
void	FileFactoryListClass::Add_FileFactory( FileFactoryClass * factory, const char *name )
{
	FactoryList.Add( factory );
	FactoryNameList.Add( name );
	Reset_Search_Start ();
}


/*
**
*/
void	FileFactoryListClass::Remove_FileFactory( FileFactoryClass * factory )
{
	for (int index = 0; index < FactoryList.Count (); index ++) {

		//
		//	If this is the factory we're looking for, then remove
		// it from the list.
		//
		if (FactoryList[index] == factory) {
			FactoryList.Delete (index);
			FactoryNameList.Delete (index);
			Reset_Search_Start ();
			break;
		}
	}

	return ;
}

/***********************************************************************************************
 * FileFactoryListClass::Remove_FileFactory -- Remove any old file factory                     *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:    Nothing                                                                           *
 *                                                                                             *
 * OUTPUT:   Ptr to removed factory. NULL if none in the list                                  *
 *                                                                                             *
 * WARNINGS: None                                                                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   9/7/2001 4:40PM ST : Created                                                              *
 *=============================================================================================*/
FileFactoryClass *FileFactoryListClass::Remove_FileFactory(void)
{
	FileFactoryClass *factory = NULL;

	if (FactoryList.Count()) {
		factory = FactoryList[0];
		FactoryList.Delete(0);
		FactoryNameList.Delete(0);
	}

	Reset_Search_Start ();
	return(factory);
}


void	FileFactoryListClass::Add_Temp_FileFactory( FileFactoryClass * factory )
{
	WWASSERT( TempFactory == NULL );
	TempFactory = factory;
}

FileFactoryClass * FileFactoryListClass::Remove_Temp_FileFactory( void )
{
	FileFactoryClass * factory = TempFactory;
	TempFactory = NULL;
	return factory;
}


FileClass * FileFactoryListClass::Get_File( char const *filename )
{
	// Very kludgly...

	// Then the temp factory
	if ( TempFactory ) {
		FileClass * file = TempFactory->Get_File( filename );
		if ( file != NULL ) {
			if ( file->Is_Available() ) {
				return file;
			} else {
				TempFactory->Return_File( file );
			}
		}
	}

	// Try the first in the list...
	if ( SearchStartIndex < FactoryList.Count() ) {
		FileClass * file = FactoryList[SearchStartIndex]->Get_File( filename );
		if ( file != NULL ) {
			if ( file->Is_Available() ) {
				return ffactorylist_prefer_archive_over_stub( this, filename, file, SearchStartIndex );
			} else {
				FactoryList[SearchStartIndex]->Return_File( file );
			}
		}
	}

	// Then try the rest
	for ( int i = 0; i < FactoryList.Count(); i++ ) {
		if (i != SearchStartIndex) {
			FileClass * file = FactoryList[i]->Get_File( filename );
			if ( file != NULL ) {
				if ( file->Is_Available() ) {
					return ffactorylist_prefer_archive_over_stub( this, filename, file, i );
				} else {
					FactoryList[i]->Return_File( file );
				}
			}
		}
	}

	// Failed!
	return NULL;
}

void FileFactoryListClass::Return_File( FileClass *file )
{
	// This is kinda bad. Just return it to the first one.  (Since they all do the same thing)
	FactoryList[0]->Return_File( file );
}


void FileFactoryListClass::Set_Search_Start( const char *name )
{
	SearchStartIndex = 0;

	//
	//	Try to find which mix file we should start searching from...
	//
	for (int index = 0; index < FactoryNameList.Count (); index ++) {
		if (FactoryNameList[index].Compare_No_Case( name ) == 0) {
			SearchStartIndex = index;
			break;
		}
	}
	return ;
}
