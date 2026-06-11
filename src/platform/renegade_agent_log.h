#ifndef RENEGADE_AGENT_LOG_H
#define RENEGADE_AGENT_LOG_H

/*
 * Boot trace log for comparing Windows vs Linux startup (through main menu).
 * Output: renegade_boot.log in the game working directory (append, JSON lines).
 */

#if defined(RENEGADE_BOOT_LOG)

#include <stdio.h>

#if defined(RENEGADE_LINUX)
#include <time.h>
#include "../platform/sdl3_host.h"
#else
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#if defined(RENEGADE_LINUX)
#define BOOT_LOG_PUMP() Platform_Pump_Events()
#else
#define BOOT_LOG_PUMP() ((void)0)
#endif

// #region agent log
static inline long long Renegade_Boot_Log_Now_Ms(void)
{
#if defined(RENEGADE_LINUX)
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return (long long)ts.tv_sec * 1000LL + (long long)(ts.tv_nsec / 1000000);
#else
	return (long long)GetTickCount();
#endif
}

static inline void Renegade_Agent_Log(
	const char *hypothesis_id,
	const char *location,
	const char *message,
	const char *data_json)
{
	FILE *f = fopen("renegade_boot.log", "a");
	if (!f) {
		return;
	}
	fprintf(
		f,
		"{\"sessionId\":\"boot\",\"hypothesisId\":\"%s\",\"location\":\"%s\","
		"\"message\":\"%s\",\"data\":%s,\"timestamp\":%lld}\n",
		hypothesis_id ? hypothesis_id : "?",
		location ? location : "?",
		message ? message : "",
		data_json ? data_json : "{}",
		Renegade_Boot_Log_Now_Ms());
	fclose(f);
}

static inline void Game_Init_Step(const char *step)
{
	char json[160];
	snprintf(json, sizeof(json), "{\"step\":\"%s\"}", step ? step : "?");
	Renegade_Agent_Log("H", "init.cpp:Game_Init", "init_step", json);
	BOOT_LOG_PUMP();
}

static inline void Game_Shutdown_Step(const char *step)
{
	char json[160];
	snprintf(json, sizeof(json), "{\"step\":\"%s\"}", step ? step : "?");
	Renegade_Agent_Log("H", "shutdown.cpp:Game_Shutdown", "shutdown_step", json);
	BOOT_LOG_PUMP();
}
// #endregion

#else

#define Renegade_Agent_Log(hypothesis_id, location, message, data_json) \
	((void)0)
#define Game_Init_Step(step) ((void)0)
#define Game_Shutdown_Step(step) ((void)0)
#define BOOT_LOG_PUMP() ((void)0)

#endif /* RENEGADE_BOOT_LOG */

#endif /* RENEGADE_AGENT_LOG_H */
