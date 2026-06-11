/*
 * MinGW stubs when Miles/embedded animation audio is unavailable.
 */
#include "animatedsoundmgr.h"

void AnimatedSoundMgrClass::Initialize(const char *)
{
}

void AnimatedSoundMgrClass::Shutdown(void)
{
}

bool AnimatedSoundMgrClass::Does_Animation_Have_Embedded_Sounds(HAnimClass *)
{
	return false;
}

float AnimatedSoundMgrClass::Trigger_Sound(HAnimClass *, float, float, const Matrix3D &)
{
	return 0.0f;
}
