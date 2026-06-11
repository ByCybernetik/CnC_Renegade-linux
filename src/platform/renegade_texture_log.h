#ifndef RENEGADE_TEXTURE_LOG_H
#define RENEGADE_TEXTURE_LOG_H

/*
 * Texture load/bind trace for comparing Win32 (D3D8) vs Linux (Vulkan).
 * Requires RENEGADE_BOOT_LOG. Output: renegade_boot.log (JSON lines, hypothesisId "TX").
 */

#if defined(RENEGADE_BOOT_LOG)

#include "renegade_agent_log.h"
#include <stdio.h>
#include <string.h>

// #region agent log
static inline const char *Tex_Log_Backend_Name(void)
{
#if defined(RENEGADE_VULKAN)
	return "vulkan";
#else
	return "d3d8";
#endif
}

static inline const char *Tex_Log_Target_Name(void)
{
#if defined(RENEGADE_LINUX)
	return "linux";
#else
	return "win32";
#endif
}

static inline void Tex_Log_Load(
	const char *event,
	const char *path,
	bool procedural,
	unsigned width,
	unsigned height,
	const void *tex_ptr,
	const char *extra_json)
{
	static unsigned s_load_count = 0;
	if (s_load_count >= 256u) {
		return;
	}
	++s_load_count;

	char data[640];
	const char *safe_path = (path != NULL && path[0] != '\0') ? path : "";
	const char *extra = (extra_json != NULL && extra_json[0] != '\0') ? extra_json : "";
	if (extra[0] != '\0') {
		snprintf(
			data,
			sizeof(data),
			"{\"event\":\"%s\",\"target\":\"%s\",\"backend\":\"%s\","
			"\"path\":\"%.120s\",\"proc\":%s,\"w\":%u,\"h\":%u,"
			"\"tex\":\"%p\",%s}",
			event ? event : "?",
			Tex_Log_Target_Name(),
			Tex_Log_Backend_Name(),
			safe_path,
			procedural ? "true" : "false",
			width,
			height,
			tex_ptr,
			extra);
	} else {
		snprintf(
			data,
			sizeof(data),
			"{\"event\":\"%s\",\"target\":\"%s\",\"backend\":\"%s\","
			"\"path\":\"%.120s\",\"proc\":%s,\"w\":%u,\"h\":%u,"
			"\"tex\":\"%p\"}",
			event ? event : "?",
			Tex_Log_Target_Name(),
			Tex_Log_Backend_Name(),
			safe_path,
			procedural ? "true" : "false",
			width,
			height,
			tex_ptr);
	}
	Renegade_Agent_Log("TX", "texture", event ? event : "tex_load", data);
}

static inline bool Tex_Log_Is_New_File_Path(const char *path)
{
	if (path == NULL || path[0] == '\0') {
		return false;
	}
	static char s_paths[64][128];
	static unsigned s_path_count = 0;
	unsigned i;
	for (i = 0; i < s_path_count; ++i) {
		if (strcmp(s_paths[i], path) == 0) {
			return false;
		}
	}
	if (s_path_count < 64u) {
		strncpy(s_paths[s_path_count], path, 127);
		s_paths[s_path_count][127] = '\0';
		++s_path_count;
	}
	return true;
}

static inline void Tex_Log_Bind(
	unsigned stage,
	const char *path,
	bool procedural,
	const void *tex_ptr,
	bool missing)
{
	static unsigned s_repeat_file_bind_count = 0;
	const bool is_cursor = path != NULL && strstr(path, "cursor") != NULL;
	const bool is_new_file_path = !procedural && Tex_Log_Is_New_File_Path(path);
	const bool always_log = is_cursor || procedural || is_new_file_path;
	if (!always_log) {
		if (s_repeat_file_bind_count >= 64u) {
			return;
		}
		++s_repeat_file_bind_count;
	}

	char data[384];
	const char *safe_path = (path != NULL && path[0] != '\0') ? path : "";
	snprintf(
		data,
		sizeof(data),
		"{\"event\":\"bind\",\"target\":\"%s\",\"backend\":\"%s\","
		"\"stage\":%u,\"path\":\"%.120s\",\"proc\":%s,\"missing\":%s,"
		"\"tex\":\"%p\"}",
		Tex_Log_Target_Name(),
		Tex_Log_Backend_Name(),
		stage,
		safe_path,
		procedural ? "true" : "false",
		missing ? "true" : "false",
		tex_ptr);
	Renegade_Agent_Log("TX", "texture", "tex_bind", data);
}

static inline void Tex_Log_Stage_Bind(
	unsigned stage,
	const char *path,
	const void *tex_ptr,
	bool missing)
{
	static char s_paths[64][128];
	static unsigned s_path_count = 0;
	if (path == NULL || path[0] == '\0') {
		return;
	}
	unsigned i;
	for (i = 0; i < s_path_count; ++i) {
		if (strcmp(s_paths[i], path) == 0) {
			return;
		}
	}
	if (s_path_count >= 64u) {
		return;
	}
	strncpy(s_paths[s_path_count], path, 127);
	s_paths[s_path_count][127] = '\0';
	++s_path_count;

	char data[384];
	snprintf(
		data,
		sizeof(data),
		"{\"event\":\"stage_bind\",\"target\":\"%s\",\"backend\":\"%s\","
		"\"stage\":%u,\"path\":\"%.120s\",\"missing\":%s,\"tex\":\"%p\"}",
		Tex_Log_Target_Name(),
		Tex_Log_Backend_Name(),
		stage,
		path,
		missing ? "true" : "false",
		tex_ptr);
	Renegade_Agent_Log("TX", "texture", "tex_stage_bind", data);
}

static inline void Tex_Log_Render2D_Draw(
	const char *path,
	bool procedural,
	unsigned tex_w,
	unsigned tex_h,
	const void *tex_ptr,
	int vert_count,
	int index_count,
	bool texturing_enabled)
{
	static unsigned s_draw_count = 0;
	if (s_draw_count >= 512u) {
		return;
	}
	++s_draw_count;

	const char *safe_path = (path != NULL && path[0] != '\0') ? path : "";

	char data[448];
	snprintf(
		data,
		sizeof(data),
		"{\"event\":\"render2d_draw\",\"target\":\"%s\",\"backend\":\"%s\","
		"\"path\":\"%.120s\",\"proc\":%s,\"tw\":%u,\"th\":%u,\"tex\":\"%p\","
		"\"verts\":%d,\"indices\":%d,\"texturing\":%s}",
		Tex_Log_Target_Name(),
		Tex_Log_Backend_Name(),
		safe_path,
		procedural ? "true" : "false",
		tex_w,
		tex_h,
		tex_ptr,
		vert_count,
		index_count,
		texturing_enabled ? "true" : "false");
	Renegade_Agent_Log("TX", "texture", "render2d_draw", data);
}

static inline void Tex_Log_Render2D_File_Draw(
	const char *path,
	int vert_count,
	int index_count,
	bool texturing_enabled)
{
	Tex_Log_Render2D_Draw(
		path,
		false,
		0u,
		0u,
		NULL,
		vert_count,
		index_count,
		texturing_enabled);
}

static inline void Tex_Log_Descriptor_Flush(
	unsigned stage,
	const void *tex_ptr,
	unsigned tex_w,
	unsigned tex_h,
	const char *path,
	bool procedural)
{
	static unsigned s_flush_count = 0;
	if (s_flush_count >= 512u) {
		return;
	}
	++s_flush_count;

	const char *safe_path = (path != NULL && path[0] != '\0') ? path : "";
	char data[384];
	snprintf(
		data,
		sizeof(data),
		"{\"event\":\"desc_flush\",\"target\":\"%s\",\"backend\":\"%s\","
		"\"stage\":%u,\"path\":\"%.120s\",\"proc\":%s,\"tw\":%u,\"th\":%u,"
		"\"tex\":\"%p\"}",
		Tex_Log_Target_Name(),
		Tex_Log_Backend_Name(),
		stage,
		safe_path,
		procedural ? "true" : "false",
		tex_w,
		tex_h,
		tex_ptr);
	Renegade_Agent_Log("TX", "texture", "desc_flush", data);
}

static inline void Tex_Log_Interference(
	const char *site,
	const char *expected_path,
	const void *expected_tex,
	unsigned expected_w,
	const char *actual_path,
	const void *actual_tex,
	unsigned actual_w)
{
	char data[512];
	const char *exp_path =
		(expected_path != NULL && expected_path[0] != '\0') ? expected_path : "";
	const char *act_path =
		(actual_path != NULL && actual_path[0] != '\0') ? actual_path : "";
	snprintf(
		data,
		sizeof(data),
		"{\"event\":\"interference\",\"target\":\"%s\",\"backend\":\"%s\","
		"\"site\":\"%.64s\",\"exp_path\":\"%.80s\",\"exp_tex\":\"%p\","
		"\"exp_w\":%u,\"act_path\":\"%.80s\",\"act_tex\":\"%p\",\"act_w\":%u}",
		Tex_Log_Target_Name(),
		Tex_Log_Backend_Name(),
		site ? site : "?",
		exp_path,
		expected_tex,
		expected_w,
		act_path,
		actual_tex,
		actual_w);
	Renegade_Agent_Log("TX", "texture", "interference", data);
}

static inline void Tex_Log_Texture_Destroyed(
	const void *tex_ptr,
	unsigned tex_w,
	unsigned tex_h,
	bool procedural)
{
	char data[256];
	snprintf(
		data,
		sizeof(data),
		"{\"event\":\"tex_destroy\",\"target\":\"%s\",\"backend\":\"%s\","
		"\"tex\":\"%p\",\"proc\":%s,\"tw\":%u,\"th\":%u}",
		Tex_Log_Target_Name(),
		Tex_Log_Backend_Name(),
		tex_ptr,
		procedural ? "true" : "false",
		tex_w,
		tex_h);
	Renegade_Agent_Log("TX", "texture", "tex_destroy", data);
}
// #endregion

#else

#define Tex_Log_Load(event, path, procedural, width, height, tex_ptr, extra_json) \
	((void)0)
#define Tex_Log_Bind(stage, path, procedural, tex_ptr, missing) ((void)0)
#define Tex_Log_Stage_Bind(stage, path, tex_ptr, missing) ((void)0)
#define Tex_Log_Render2D_Draw(path, procedural, tex_w, tex_h, tex_ptr, vert_count, index_count, texturing_enabled) \
	((void)0)
#define Tex_Log_Render2D_File_Draw(path, vert_count, index_count, texturing_enabled) \
	((void)0)
#define Tex_Log_Descriptor_Flush(stage, tex_ptr, tex_w, tex_h, path, procedural) \
	((void)0)
#define Tex_Log_Interference(site, expected_path, expected_tex, expected_w, actual_path, actual_tex, actual_w) \
	((void)0)
#define Tex_Log_Texture_Destroyed(tex_ptr, tex_w, tex_h, procedural) \
	((void)0)

#endif /* RENEGADE_BOOT_LOG */

#endif /* RENEGADE_TEXTURE_LOG_H */
