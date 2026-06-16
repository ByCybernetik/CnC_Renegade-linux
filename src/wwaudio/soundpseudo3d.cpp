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
 *                 Project Name : WWAudio                                                      *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/WWAudio/SoundPseudo3D.cpp         $*
 *                                                                                             *
 *                       Author:: Patrick Smith                                                *
 *                                                                                             *
 *                     $Modtime:: 8/13/01 12:09p                                              $*
 *                                                                                             *
 *                    $Revision:: 8                                                           $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


#include "soundpseudo3d.h"
#include "wwaudio.h"
#include "soundscene.h"
#include "utils.h"
#include "soundchunkids.h"
#include "persistfactory.h"
#include "soundhandle.h"
#include "sound2dhandle.h"
#include "audiblesound.h"
#include "wwmath.h"
#if defined(RENEGADE_LINUX)
#endif


//////////////////////////////////////////////////////////////////////////////////
//	Static factories
//////////////////////////////////////////////////////////////////////////////////
SimplePersistFactoryClass<SoundPseudo3DClass, CHUNKID_PSEUDO_SOUND3D> _PseudoSound3DPersistFactory;


////////////////////////////////////////////////////////////////////////////////////////////////
//
//	SoundPseudo3DClass
//
////////////////////////////////////////////////////////////////////////////////////////////////
SoundPseudo3DClass::SoundPseudo3DClass (void)
{
	return ;
}


////////////////////////////////////////////////////////////////////////////////////////////////
//
//	SoundPseudo3DClass
//
////////////////////////////////////////////////////////////////////////////////////////////////
SoundPseudo3DClass::SoundPseudo3DClass (const SoundPseudo3DClass &src)
	:	Sound3DClass (src)
{
	(*this) = src;
	return ;
}


////////////////////////////////////////////////////////////////////////////////////////////////
//
//	~SoundPseudo3DClass
//
////////////////////////////////////////////////////////////////////////////////////////////////
SoundPseudo3DClass::~SoundPseudo3DClass (void)
{
	Free_Miles_Handle ();
	return ;
}


////////////////////////////////////////////////////////////////////////////////////////////////
//
//	operator=
//
////////////////////////////////////////////////////////////////////////////////////////////////
const SoundPseudo3DClass &
SoundPseudo3DClass::operator= (const SoundPseudo3DClass &src)
{
	// Call the base class
	Sound3DClass::operator= (src);
	return (*this);
}


////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Set_Miles_Handle
//
////////////////////////////////////////////////////////////////////////////////////////////////
void
SoundPseudo3DClass::Set_Miles_Handle (MILES_HANDLE handle)
{
	/*
	** Footstep / surface SFX: use one-shot 2D PCM (not AIL_open_stream on the HSAMPLE).
	** Streaming + rewind_resubmit stacked buffers and crashed inside FAudio matrix code.
	*/
	Free_Miles_Handle ();

	if (handle != INVALID_MILES_HANDLE && m_Buffer != NULL) {
		m_SoundHandle = new Sound2DHandleClass;
		m_SoundHandle->Set_Miles_Handle (handle);

#if defined(RENEGADE_LINUX)
		m_SoundHandle->Set_Sample_User_Data (INFO_OBJECT_PTR, (intptr_t)this);
#endif

		Initialize_Miles_Handle ();
	}

	return ;
}


////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Initialize_Miles_Handle
//
////////////////////////////////////////////////////////////////////////////////////////////////
void
SoundPseudo3DClass::Initialize_Miles_Handle (void)
{
	AudibleSoundClass::Initialize_Miles_Handle ();
	Update_Pseudo_Volume ();
	Update_Pseudo_Pan ();
	return ;
}


////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Update_Pseudo_Volume
//
////////////////////////////////////////////////////////////////////////////////////////////////
void
SoundPseudo3DClass::Update_Pseudo_Volume (float distance)
{
	MMSLockClass lock;	

	//
	// Only do this if the sound is really playing
	//
	if (m_SoundHandle != NULL) {

#if defined(RENEGADE_LINUX)
		if (!Verify_Linux_Miles_Handle_Ownership ()) {
			return;
		}
#endif
		
		float volume_mod = Determine_Real_Volume ();
		if (m_FadeType == FADE_IN && m_FadeTime > 0) {
			float percent = 1.0F - ((float)m_FadeTimer / (float)m_FadeTime);
			volume_mod *= WWMath::Clamp (percent, 0.0F, 1.0F);
		}
		float max_distance = Get_DropOff_Radius ();
		float min_distance = Get_Max_Vol_Radius ();
		float delta = max_distance - min_distance;

		// Determine a normalized volume from the position
		float volume = 1.0F;
		if (delta > 0.001F && distance > min_distance) {
			volume = 1.0F - ((distance - min_distance) / delta);
			volume = min (volume, 1.0F);
			volume = max (volume, 0.0F);			
		} else if (delta <= 0.001F && distance > max_distance) {
			volume = 0.0F;
		}

		// Multiply the 'max' volume with the calculated volume
		volume = volume * volume_mod;

		//
		// Pass the volume on (Miles 0..127; round so low levels are not stuck at 0 or 1).
		//
		{
			int miles_vol = (int)(volume * 127.0F + 0.5F);
			if (miles_vol < 0) {
				miles_vol = 0;
			} else if (miles_vol > 127) {
				miles_vol = 127;
			}
			m_SoundHandle->Set_Sample_Volume (miles_vol);
		}
	}

	return ;
}


////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Update_Pseudo_Volume
//
////////////////////////////////////////////////////////////////////////////////////////////////
void
SoundPseudo3DClass::Update_Pseudo_Volume (void)
{
	MMSLockClass lock;	

	// Only do this if the sound is really playing
	if (m_SoundHandle != NULL) {
		
		//
		// Find the difference in the sound position and its listener's position
		//		
		Vector3 sound_pos = m_ListenerTransform.Get_Translation () - m_Transform.Get_Translation ();
		float distance = sound_pos.Quick_Length ();

		//
		// Determine a normalized volume from the position
		//
		Update_Pseudo_Volume (distance);
	}

	return ;
}


////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Update_Pseudo_Pan
//
////////////////////////////////////////////////////////////////////////////////////////////////
void
SoundPseudo3DClass::Update_Pseudo_Pan (void)
{
	MMSLockClass lock;	

	//
	// Only do this if the sound is really playing
	//
	if (m_SoundHandle != NULL) {

#if defined(RENEGADE_LINUX)
		if (!Verify_Linux_Miles_Handle_Ownership ()) {
			return;
		}
#endif
		
		//
		//	Transform the sound's position into 'listener-space'
		//
		Vector3 sound_pos	= m_Transform.Get_Translation ();
		Vector3 rel_sound_pos;
		Matrix3D::Inverse_Transform_Vector (m_ListenerTransform, sound_pos, &rel_sound_pos);

		//
		//	Calculate a normalized pan from 0 (hard left) to 1.0F (hard right)
		//
		float angle	= WWMath::Atan2 (rel_sound_pos.Y, rel_sound_pos.X);
		float pan	= -WWMath::Fast_Sin (angle);
		pan			= (pan / 2.0F) + 0.5F;

		//
		// Pass the pan on
		//
		m_SoundHandle->Set_Sample_Pan (S32(pan * 127.0F));

#if defined(RENEGADE_LINUX)
#endif
	}

	return ;
}


////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Allocate_Miles_Handle
//
////////////////////////////////////////////////////////////////////////////////////////////////
void
SoundPseudo3DClass::Allocate_Miles_Handle (void)
{
	AudibleSoundClass::Allocate_Miles_Handle ();
	return ;
}


////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Free_Miles_Handle
//
////////////////////////////////////////////////////////////////////////////////////////////////
void
SoundPseudo3DClass::Free_Miles_Handle (bool force_end)
{
	AudibleSoundClass::Free_Miles_Handle (force_end);
	return ;
}


////////////////////////////////////////////////////////////////////////////////////////////////
//
//	On_Frame_Update
//
////////////////////////////////////////////////////////////////////////////////////////////////
bool
SoundPseudo3DClass::On_Frame_Update (unsigned int milliseconds)
{
	/*
	** Apply fade before distance/pan — otherwise uncull fade lags one frame and
	** ambients can pop loud when entering a zone.
	*/
	bool result = Sound3DClass::On_Frame_Update (milliseconds);

	if (m_SoundHandle != NULL) {
		Update_Pseudo_Volume ();
		Update_Pseudo_Pan ();
	}

	return result;
}


////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Get_Factory
//
////////////////////////////////////////////////////////////////////////////////////////////////
const PersistFactoryClass &
SoundPseudo3DClass::Get_Factory (void) const
{	
	return _PseudoSound3DPersistFactory;
}
