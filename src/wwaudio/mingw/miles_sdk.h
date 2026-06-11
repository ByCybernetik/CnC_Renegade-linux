#ifndef MILES_SDK_H
#define MILES_SDK_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <mmsystem.h>

#include "rrcore.h"
#define AIL_init_sample AIL_init_sample_mss_api
#define AIL_set_room_type AIL_set_room_type_mss_api
#define AIL_room_type AIL_room_type_mss_api
#include "../../../third_party/milesss-v9.3b-main/win/sdk/include/mss.h"
#undef AIL_room_type
#undef AIL_set_room_type
#undef AIL_init_sample
#undef AIL_lock
#undef AIL_unlock

#endif
