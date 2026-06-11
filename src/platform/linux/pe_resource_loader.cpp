/*
 * PE resource directory reader for chat.res embedded via objcopy -I binary.
 * Enables FindResource / LoadResource / LockResource for UI dialogs on Linux.
 */
#include "pe_resource_loader.h"
#include "renegade_win32_shim.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct _IMAGE_RESOURCE_DIRECTORY {
	DWORD Characteristics;
	DWORD TimeDateStamp;
	WORD MajorVersion;
	WORD MinorVersion;
	WORD NumberOfNamedEntries;
	WORD NumberOfIdEntries;
} IMAGE_RESOURCE_DIRECTORY;

typedef struct _IMAGE_RESOURCE_DIRECTORY_ENTRY {
	union {
		struct {
			DWORD NameOffset : 31;
			DWORD NameIsString : 1;
		};
		DWORD Name;
		WORD Id;
	};
	union {
		DWORD OffsetToData;
		DWORD OffsetToDirectory;
	};
} IMAGE_RESOURCE_DIRECTORY_ENTRY;

typedef struct _IMAGE_RESOURCE_DATA_ENTRY {
	DWORD OffsetToData;
	DWORD Size;
	DWORD CodePage;
	DWORD Reserved;
} IMAGE_RESOURCE_DATA_ENTRY;

extern "C" {
/* From commando/meson.build: .rsrc section of chat.rc COFF object, embedded as binary. */
extern const uint8_t _binary_src_commando_renegade_chat_rsrc_bin_start[];
extern const uint8_t _binary_src_commando_renegade_chat_rsrc_bin_end[];
}

#ifndef IMAGE_RESOURCE_DATA_IS_DIRECTORY
#define IMAGE_RESOURCE_DATA_IS_DIRECTORY 0x80000000
#endif

#ifndef IS_INTRESOURCE
#define IS_INTRESOURCE(_r) (((ULONG_PTR)(_r) >> 16) == 0)
#endif

struct RenegadeResourceHandle {
	const void *data;
	DWORD size;
};

static const uint8_t *g_res_base = NULL;
static size_t g_res_size = 0;
static RenegadeResourceHandle g_last_handle;

static const IMAGE_RESOURCE_DIRECTORY *Res_Dir_At(uint32_t offset)
{
	return (const IMAGE_RESOURCE_DIRECTORY *)(g_res_base + offset);
}

static const IMAGE_RESOURCE_DIRECTORY_ENTRY *Res_Entries(
	const IMAGE_RESOURCE_DIRECTORY *dir)
{
	return (const IMAGE_RESOURCE_DIRECTORY_ENTRY *)(dir + 1);
}

static int Res_Entry_Count(const IMAGE_RESOURCE_DIRECTORY *dir)
{
	return (int)dir->NumberOfNamedEntries + (int)dir->NumberOfIdEntries;
}

static uint32_t Res_Entry_Offset(const IMAGE_RESOURCE_DIRECTORY_ENTRY *entry)
{
	return entry->OffsetToData & ~IMAGE_RESOURCE_DATA_IS_DIRECTORY;
}

static bool Res_Entry_Is_Directory(const IMAGE_RESOURCE_DIRECTORY_ENTRY *entry)
{
	return (entry->OffsetToData & IMAGE_RESOURCE_DATA_IS_DIRECTORY) != 0;
}

static const IMAGE_RESOURCE_DIRECTORY *Res_Find_Id(
	const IMAGE_RESOURCE_DIRECTORY *dir,
	WORD id)
{
	const IMAGE_RESOURCE_DIRECTORY_ENTRY *entries = Res_Entries(dir);
	const int count = Res_Entry_Count(dir);
	for (int i = 0; i < count; ++i) {
		if (entries[i].NameIsString) {
			continue;
		}
		if (entries[i].Id != id) {
			continue;
		}
		if (!Res_Entry_Is_Directory(&entries[i])) {
			return NULL;
		}
		return Res_Dir_At(Res_Entry_Offset(&entries[i]));
	}
	return NULL;
}

static const IMAGE_RESOURCE_DATA_ENTRY *Res_Find_Data(
	const IMAGE_RESOURCE_DIRECTORY *dir,
	WORD lang_id)
{
	const IMAGE_RESOURCE_DIRECTORY_ENTRY *entries = Res_Entries(dir);
	const int count = Res_Entry_Count(dir);
	const IMAGE_RESOURCE_DATA_ENTRY *fallback = NULL;

	for (int i = 0; i < count; ++i) {
		if (entries[i].NameIsString) {
			continue;
		}
		if (Res_Entry_Is_Directory(&entries[i])) {
			continue;
		}

		const IMAGE_RESOURCE_DATA_ENTRY *data =
			(const IMAGE_RESOURCE_DATA_ENTRY *)(g_res_base + Res_Entry_Offset(&entries[i]));

		if (entries[i].Id == lang_id) {
			return data;
		}
		if (fallback == NULL) {
			fallback = data;
		}
	}

	return fallback;
}

static WORD Res_Parse_Id(LPCSTR value)
{
	if (value == NULL) {
		return 0;
	}
	if (IS_INTRESOURCE(value)) {
		return (WORD)(ULONG_PTR)value;
	}
	return 0;
}

void Renegade_Init_Embedded_Resources(void)
{
	if (g_res_base != NULL) {
		return;
	}
	g_res_base = _binary_src_commando_renegade_chat_rsrc_bin_start;
	g_res_size = (size_t)(_binary_src_commando_renegade_chat_rsrc_bin_end -
		_binary_src_commando_renegade_chat_rsrc_bin_start);
	memset(&g_last_handle, 0, sizeof(g_last_handle));
}

static HRSRC Res_Find_Internal(LPCSTR type, LPCSTR name, WORD lang)
{
	if (g_res_base == NULL || g_res_size < sizeof(IMAGE_RESOURCE_DIRECTORY)) {
		return NULL;
	}

	const WORD type_id = Res_Parse_Id(type);
	const WORD name_id = Res_Parse_Id(name);
	if (type_id == 0 || name_id == 0) {
		return NULL;
	}

	const IMAGE_RESOURCE_DIRECTORY *root =
		(const IMAGE_RESOURCE_DIRECTORY *)g_res_base;
	const IMAGE_RESOURCE_DIRECTORY *type_dir = Res_Find_Id(root, type_id);
	if (type_dir == NULL) {
		return NULL;
	}

	const IMAGE_RESOURCE_DIRECTORY *name_dir = Res_Find_Id(type_dir, name_id);
	if (name_dir == NULL) {
		return NULL;
	}

	const IMAGE_RESOURCE_DATA_ENTRY *data = Res_Find_Data(name_dir, lang);
	if (data == NULL) {
		return NULL;
	}

	g_last_handle.data = g_res_base + data->OffsetToData;
	g_last_handle.size = data->Size;
	return (HRSRC)&g_last_handle;
}

#ifdef __cplusplus
extern "C" {
#endif

HRSRC FindResourceExA(HMODULE, LPCSTR type, LPCSTR name, WORD language)
{
	Renegade_Init_Embedded_Resources();
	return Res_Find_Internal(type, name, language);
}

HRSRC FindResourceA(HMODULE, LPCSTR name, LPCSTR type)
{
	return FindResourceExA(NULL, type, name, 0);
}

HGLOBAL LoadResource(HMODULE, HRSRC res)
{
	return (HGLOBAL)res;
}

void *LockResource(HGLOBAL res)
{
	if (res == NULL) {
		return NULL;
	}
	const RenegadeResourceHandle *handle = (const RenegadeResourceHandle *)res;
	return (void *)handle->data;
}

DWORD SizeofResource(HMODULE, HRSRC res)
{
	if (res == NULL) {
		return 0;
	}
	const RenegadeResourceHandle *handle = (const RenegadeResourceHandle *)res;
	return handle->size;
}

#ifdef __cplusplus
}
#endif
