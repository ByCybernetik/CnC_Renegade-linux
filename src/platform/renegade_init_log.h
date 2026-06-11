#ifndef RENEGADE_INIT_LOG_H
#define RENEGADE_INIT_LOG_H

/*
 * Game initialization trace (Linux debug). NDJSON -> renegade-init.log in cwd.
 */

#if defined(RENEGADE_INIT_LOG)

#include <stdio.h>

#if defined(RENEGADE_LINUX)
#include <time.h>
#include "sdl3_host.h"
#define INIT_LOG_PUMP() Platform_Pump_Events()
#else
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#define INIT_LOG_PUMP() ((void)0)
#endif

// #region init log
static inline long long Renegade_Init_Log_Now_Ms(void)
{
#if defined(RENEGADE_LINUX)
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return (long long)ts.tv_sec * 1000LL + (long long)(ts.tv_nsec / 1000000);
#else
	return (long long)GetTickCount();
#endif
}

static inline void Renegade_Init_Log(
	const char *hypothesis_id,
	const char *location,
	const char *message,
	const char *data_json)
{
	FILE *f = fopen("renegade-init.log", "a");
	if (f == NULL) {
		return;
	}
	fprintf(
		f,
		"{\"hypothesisId\":\"%s\",\"location\":\"%s\","
		"\"message\":\"%s\",\"data\":%s,\"timestamp\":%lld}\n",
		hypothesis_id != NULL ? hypothesis_id : "?",
		location != NULL ? location : "?",
		message != NULL ? message : "",
		data_json != NULL ? data_json : "{}",
		Renegade_Init_Log_Now_Ms());
	fclose(f);
}

static inline void Game_Init_Step(const char *step)
{
	char json[160];
	snprintf(json, sizeof(json), "{\"step\":\"%s\"}", step != NULL ? step : "?");
	Renegade_Init_Log("H", "init.cpp:Game_Init", "init_step", json);
	INIT_LOG_PUMP();
}

static inline void Game_Init_Log_Fail(const char *reason)
{
	char json[192];
	snprintf(json, sizeof(json), "{\"reason\":\"%s\"}", reason != NULL ? reason : "?");
	Renegade_Init_Log("H", "init.cpp:Game_Init", "init_fail", json);
	INIT_LOG_PUMP();
}
// #endregion

#define GAME_INIT_FAIL(REASON) \
	do { \
		Game_Init_Log_Fail(REASON); \
		return false; \
	} while (0)

#else

#define Renegade_Init_Log(hypothesis_id, location, message, data_json) \
	((void)0)
#define Game_Init_Step(step) ((void)0)
#define Game_Init_Log_Fail(reason) ((void)0)
#define GAME_INIT_FAIL(REASON) return false
#define INIT_LOG_PUMP() ((void)0)

#endif /* RENEGADE_INIT_LOG */

#endif /* RENEGADE_INIT_LOG_H */
