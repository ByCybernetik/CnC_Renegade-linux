#ifndef RENEGADE_NAV_LOG_H
#define RENEGADE_NAV_LOG_H

/*
 * NPC navigation trace. NDJSON -> renegade-nav.log in the game working directory.
 * Enable: meson configure build-linux -Dnav_log=true
 */

#if defined(RENEGADE_NAV_LOG)

#include <stdio.h>

#if defined(RENEGADE_LINUX)
#include <time.h>
#include "sdl3_host.h"
#define NAV_LOG_PUMP() Platform_Pump_Events()
#else
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#define NAV_LOG_PUMP() ((void)0)
#endif

// #region nav log
static inline long long Renegade_Nav_Log_Now_Ms(void)
{
#if defined(RENEGADE_LINUX)
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return (long long)ts.tv_sec * 1000LL + (long long)(ts.tv_nsec / 1000000);
#else
	return (long long)GetTickCount();
#endif
}

static inline void Renegade_Nav_Log(
	const char *location,
	const char *message,
	const char *data_json)
{
	FILE *f = fopen("renegade-nav.log", "a");
	if (f == NULL) {
		return;
	}
	fprintf(
		f,
		"{\"location\":\"%s\",\"message\":\"%s\",\"data\":%s,\"timestamp\":%lld}\n",
		location != NULL ? location : "?",
		message != NULL ? message : "",
		data_json != NULL ? data_json : "{}",
		Renegade_Nav_Log_Now_Ms());
	fclose(f);
	NAV_LOG_PUMP();
}

static inline const char *Nav_Log_Solve_State(int state)
{
	switch (state) {
		case 0: return "THINKING";
		case 1: return "SOLVED_PATH";
		case 2: return "ERROR_INVALID_START_POS";
		case 3: return "ERROR_INVALID_DEST_POS";
		case 4: return "ERROR_NO_PATH";
		default: return "UNKNOWN";
	}
}

static inline void Nav_Log_Load(
	const char *map,
	int sectors,
	int portals,
	int waypaths,
	int ok)
{
	char json[256];
	snprintf(
		json,
		sizeof(json),
		"{\"map\":\"%s\",\"sectors\":%d,\"portals\":%d,\"waypaths\":%d,\"ok\":%d}",
		map != NULL ? map : "",
		sectors,
		portals,
		waypaths,
		ok);
	Renegade_Nav_Log("Pathfind.cpp:Load", "nav_load", json);
}

static inline void Nav_Log_Level_Load(const char *map)
{
	char json[192];
	snprintf(json, sizeof(json), "{\"map\":\"%s\"}", map != NULL ? map : "");
	Renegade_Nav_Log("savegame.cpp:Load_Level", "nav_level_load", json);
}

static inline void Nav_Log_Goto_Init(
	int obj_id,
	const char *mode,
	int waypath_id,
	int move_pathfind,
	float dest_x,
	float dest_y,
	float dest_z)
{
	char json[320];
	snprintf(
		json,
		sizeof(json),
		"{\"obj\":%d,\"mode\":\"%s\",\"waypath\":%d,\"move_pathfind\":%d,"
		"\"dest\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f}}",
		obj_id,
		mode != NULL ? mode : "?",
		waypath_id,
		move_pathfind,
		dest_x,
		dest_y,
		dest_z);
	Renegade_Nav_Log("action.cpp:Goto", "nav_goto_init", json);
}

static inline void Nav_Log_Solve_Start(
	int start_sector,
	int dest_sector,
	int same_sector,
	int state)
{
	char json[256];
	snprintf(
		json,
		sizeof(json),
		"{\"start_sector\":%d,\"dest_sector\":%d,\"same_sector\":%d,\"state\":\"%s\"}",
		start_sector,
		dest_sector,
		same_sector,
		Nav_Log_Solve_State(state));
	Renegade_Nav_Log("pathsolve.cpp", "nav_solve_start", json);
}

static inline void Nav_Log_Solve_Done(
	int state,
	int path_points,
	int iterations)
{
	char json[192];
	snprintf(
		json,
		sizeof(json),
		"{\"state\":\"%s\",\"path_points\":%d,\"iterations\":%d}",
		Nav_Log_Solve_State(state),
		path_points,
		iterations);
	Renegade_Nav_Log("pathsolve.cpp", "nav_solve_done", json);
}

static inline void Nav_Log_Goto_Result(
	int obj_id,
	const char *result,
	int move_pathfind)
{
	char json[192];
	snprintf(
		json,
		sizeof(json),
		"{\"obj\":%d,\"result\":\"%s\",\"move_pathfind\":%d}",
		obj_id,
		result != NULL ? result : "?",
		move_pathfind);
	Renegade_Nav_Log("action.cpp:Goto", "nav_goto_result", json);
}
// #endregion

#else

#define Renegade_Nav_Log(location, message, data_json) ((void)0)
#define Nav_Log_Load(map, sectors, portals, waypaths, ok) ((void)0)
#define Nav_Log_Level_Load(map) ((void)0)
#define Nav_Log_Goto_Init(obj_id, mode, waypath_id, move_pathfind, dest_x, dest_y, dest_z) \
	((void)0)
#define Nav_Log_Solve_Start(start_sector, dest_sector, same_sector, state) ((void)0)
#define Nav_Log_Solve_Done(state, path_points, iterations) ((void)0)
#define Nav_Log_Goto_Result(obj_id, result, move_pathfind) ((void)0)
#define NAV_LOG_PUMP() ((void)0)

#endif /* RENEGADE_NAV_LOG */

#endif /* RENEGADE_NAV_LOG_H */
