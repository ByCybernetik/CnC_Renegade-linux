/*
 * Linux registry shim: persist RegistryClass data in Data/config/registry.ini.
 */
#include "winreg.h"
#include "winerror.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <string>
#include <vector>

#ifndef ERROR_NO_MORE_ITEMS
#define ERROR_NO_MORE_ITEMS 259L
#endif
#ifndef ERROR_FILE_NOT_FOUND
#define ERROR_FILE_NOT_FOUND 2L
#endif
#ifndef ERROR_INVALID_HANDLE
#define ERROR_INVALID_HANDLE 6L
#endif
#ifndef ERROR_MORE_DATA
#define ERROR_MORE_DATA 234L
#endif
#ifndef ERROR_INVALID_PARAMETER
#define ERROR_INVALID_PARAMETER 87L
#endif
#ifndef ERROR_OUTOFMEMORY
#define ERROR_OUTOFMEMORY 14L
#endif

struct LinuxRegValue {
	DWORD type;
	std::vector<unsigned char> data;
};

struct LinuxRegSection {
	std::map<std::string, LinuxRegValue> values;
};

/*
 * Lazy singletons: cRegistryBool globals may call Reg* during static init
 * before other translation-unit globals are constructed.
 */
static std::map<std::string, LinuxRegSection> &linux_registry_sections(void)
{
	static std::map<std::string, LinuxRegSection> *sections =
		new std::map<std::string, LinuxRegSection>();
	return *sections;
}

static std::map<HKEY, std::string> &linux_registry_open_keys(void)
{
	static std::map<HKEY, std::string> *keys = new std::map<HKEY, std::string>();
	return *keys;
}

static uintptr_t &linux_registry_next_handle(void)
{
	static uintptr_t *next = new uintptr_t(1);
	return *next;
}

static bool &linux_registry_loaded_flag(void)
{
	static bool *loaded = new bool(false);
	return *loaded;
}

static bool &linux_registry_dirty_flag(void)
{
	static bool *dirty = new bool(false);
	return *dirty;
}

static void linux_registry_normalize_path(const char *in, std::string &out)
{
	out.clear();
	if (in == NULL) {
		return;
	}
	for (const char *p = in; *p != '\0'; ++p) {
		out.push_back((*p == '\\') ? '/' : *p);
	}
}

static const char *linux_registry_file_path(void)
{
	static char path[1024];
	const char *from_env = getenv("RENEGADE_REGISTRY_INI");
	if (from_env != NULL && from_env[0] != '\0') {
		strncpy(path, from_env, sizeof(path) - 1);
		path[sizeof(path) - 1] = '\0';
		return path;
	}
	snprintf(path, sizeof(path), "Data/config/registry.ini");
	return path;
}

static void linux_registry_trim(std::string &s)
{
	while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t')) {
		s.pop_back();
	}
	size_t start = 0;
	while (start < s.size() && (s[start] == ' ' || s[start] == '\t')) {
		++start;
	}
	if (start > 0) {
		s.erase(0, start);
	}
}

static void linux_registry_store_value(
	const std::string &section, const std::string &name, DWORD type, const std::vector<unsigned char> &data)
{
	LinuxRegValue &val = linux_registry_sections()[section].values[name];
	val.type = type;
	val.data = data;
}

static void linux_registry_load(void)
{
	if (linux_registry_loaded_flag()) {
		return;
	}
	linux_registry_loaded_flag() = true;

	FILE *fp = fopen(linux_registry_file_path(), "r");
	if (fp == NULL) {
		return;
	}

	std::string section;
	char line[4096];
	while (fgets(line, sizeof(line), fp) != NULL) {
		std::string raw(line);
		linux_registry_trim(raw);
		if (raw.empty() || raw[0] == ';' || raw[0] == '#') {
			continue;
		}
		if (raw[0] == '[') {
			const size_t end = raw.find(']');
			if (end != std::string::npos && end > 1) {
				section = raw.substr(1, end - 1);
				linux_registry_normalize_path(section.c_str(), section);
			}
			continue;
		}
		if (section.empty()) {
			continue;
		}

		const size_t eq = raw.find('=');
		if (eq == std::string::npos) {
			continue;
		}

		std::string key = raw.substr(0, eq);
		std::string value = raw.substr(eq + 1);
		linux_registry_trim(key);
		linux_registry_trim(value);

		DWORD type = REG_SZ;
		const char *name = key.c_str();
		if (strncmp(key.c_str(), "STRING_", 7) == 0) {
			name = key.c_str() + 7;
		} else if (strncmp(key.c_str(), "DWORD_", 6) == 0) {
			name = key.c_str() + 6;
			type = REG_DWORD;
		} else if (strncmp(key.c_str(), "BIN_", 4) == 0) {
			continue;
		}

		std::vector<unsigned char> bytes;
		if (type == REG_DWORD) {
			const DWORD dword = (DWORD)strtoul(value.c_str(), NULL, 10);
			const unsigned char *src = (const unsigned char *)&dword;
			bytes.assign(src, src + sizeof(dword));
		} else {
			bytes.assign(value.begin(), value.end());
			bytes.push_back('\0');
		}
		linux_registry_store_value(section, name, type, bytes);
	}

	fclose(fp);
}

static void linux_registry_save(void)
{
	if (!linux_registry_dirty_flag()) {
		return;
	}

	const char *path = linux_registry_file_path();
	FILE *fp = fopen(path, "w");
	if (fp == NULL) {
		return;
	}

	for (const auto &section_it : linux_registry_sections()) {
		fprintf(fp, "[%s]\n", section_it.first.c_str());
		for (const auto &value_it : section_it.second.values) {
			const LinuxRegValue &val = value_it.second;
			if (val.type == REG_DWORD && val.data.size() >= sizeof(DWORD)) {
				DWORD dword = 0;
				memcpy(&dword, val.data.data(), sizeof(dword));
				fprintf(fp, "DWORD_%s=%lu\n", value_it.first.c_str(), (unsigned long)dword);
			} else if (val.type == REG_SZ && !val.data.empty()) {
				fprintf(fp, "STRING_%s=%s\n", value_it.first.c_str(), (const char *)val.data.data());
			}
		}
		fprintf(fp, "\n");
	}

	fclose(fp);
	linux_registry_dirty_flag() = false;
}

static std::string linux_registry_full_path(HKEY root, LPCSTR sub)
{
	std::string path;
	if (sub != NULL && sub[0] != '\0') {
		linux_registry_normalize_path(sub, path);
	}
	(void)root;
	return path;
}

static HKEY linux_registry_open_handle(const std::string &path, bool create)
{
	linux_registry_load();

	if (!create && linux_registry_sections().find(path) == linux_registry_sections().end()) {
		return NULL;
	}

	if (create) {
		(void)linux_registry_sections()[path];
	}

	const HKEY handle = (HKEY)(uintptr_t)linux_registry_next_handle()++;
	linux_registry_open_keys()[handle] = path;
	return handle;
}

static LinuxRegSection *linux_registry_section_for(HKEY key)
{
	const auto it = linux_registry_open_keys().find(key);
	if (it == linux_registry_open_keys().end()) {
		return NULL;
	}
	return &linux_registry_sections()[it->second];
}

extern "C" {

LONG RegOpenKeyExA(HKEY root, LPCSTR sub, DWORD opts, DWORD access, HKEY *out)
{
	(void)opts;
	(void)access;
	if (out == NULL) {
		return ERROR_INVALID_PARAMETER;
	}

	const std::string path = linux_registry_full_path(root, sub);
	HKEY handle = linux_registry_open_handle(path, false);
	if (handle == NULL) {
		*out = NULL;
		return ERROR_FILE_NOT_FOUND;
	}

	*out = handle;
	return ERROR_SUCCESS;
}

LONG RegCreateKeyExA(
	HKEY root, LPCSTR sub, DWORD reserved, LPSTR cls, DWORD opts, DWORD access,
	LPVOID security, HKEY *out, DWORD *disp)
{
	(void)reserved;
	(void)cls;
	(void)opts;
	(void)access;
	(void)security;
	if (out == NULL) {
		return ERROR_INVALID_PARAMETER;
	}

	linux_registry_load();
	const std::string path = linux_registry_full_path(root, sub);
	const bool existed = linux_registry_sections().find(path) != linux_registry_sections().end();
	HKEY handle = linux_registry_open_handle(path, true);
	if (handle == NULL) {
		return ERROR_OUTOFMEMORY;
	}

	if (disp != NULL) {
		*disp = existed ? 2u : 1u;
	}
	*out = handle;
	return ERROR_SUCCESS;
}

LONG RegEnumKeyExA(HKEY, DWORD, LPSTR, LPDWORD, LPDWORD, LPSTR, LPDWORD, void *)
{
	return ERROR_NO_MORE_ITEMS;
}

LONG RegEnumValueA(
	HKEY key, DWORD index, LPSTR name, LPDWORD name_len, LPDWORD reserved,
	LPDWORD type, LPBYTE data, LPDWORD data_len)
{
	(void)reserved;
	LinuxRegSection *section = linux_registry_section_for(key);
	if (section == NULL) {
		return ERROR_INVALID_HANDLE;
	}
	if (index >= section->values.size()) {
		return ERROR_NO_MORE_ITEMS;
	}

	size_t i = 0;
	for (const auto &it : section->values) {
		if (i++ != index) {
			continue;
		}

		if (name != NULL && name_len != NULL && *name_len > 0) {
			strncpy(name, it.first.c_str(), *name_len);
			name[*name_len - 1] = '\0';
			*name_len = (DWORD)strlen(name);
		}
		if (type != NULL) {
			*type = it.second.type;
		}
		if (data != NULL && data_len != NULL) {
			const size_t to_copy = (it.second.data.size() < *data_len) ? it.second.data.size() : *data_len;
			if (to_copy > 0) {
				memcpy(data, it.second.data.data(), to_copy);
			}
			*data_len = (DWORD)it.second.data.size();
		}
		return ERROR_SUCCESS;
	}

	return ERROR_NO_MORE_ITEMS;
}

LONG RegQueryInfoKeyA(
	HKEY key, LPSTR cls, LPDWORD cls_len, LPDWORD reserved, LPDWORD subkeys,
	LPDWORD max_sub, LPDWORD max_cls, LPDWORD values, LPDWORD max_val, LPDWORD max_data,
	LPDWORD sec, void *mtime)
{
	(void)cls;
	(void)cls_len;
	(void)reserved;
	(void)subkeys;
	(void)max_sub;
	(void)max_cls;
	(void)sec;
	(void)mtime;

	LinuxRegSection *section = linux_registry_section_for(key);
	if (section == NULL) {
		return ERROR_INVALID_HANDLE;
	}

	DWORD count = 0;
	DWORD max_name = 0;
	DWORD max_size = 0;
	for (const auto &it : section->values) {
		++count;
		const DWORD name_len = (DWORD)it.first.size();
		const DWORD data_len = (DWORD)it.second.data.size();
		if (name_len > max_name) {
			max_name = name_len;
		}
		if (data_len > max_size) {
			max_size = data_len;
		}
	}

	if (values != NULL) {
		*values = count;
	}
	if (max_val != NULL) {
		*max_val = max_name;
	}
	if (max_data != NULL) {
		*max_data = max_size;
	}
	return ERROR_SUCCESS;
}

LONG RegQueryValueExA(HKEY key, LPCSTR name, LPDWORD reserved, LPDWORD type, LPBYTE data, LPDWORD data_len)
{
	(void)reserved;
	LinuxRegSection *section = linux_registry_section_for(key);
	if (section == NULL || name == NULL) {
		return ERROR_INVALID_HANDLE;
	}

	const auto it = section->values.find(name);
	if (it == section->values.end()) {
		if (data_len != NULL) {
			*data_len = 0;
		}
		return ERROR_FILE_NOT_FOUND;
	}

	if (type != NULL) {
		*type = it->second.type;
	}
	if (data_len != NULL) {
		if (data == NULL) {
			*data_len = (DWORD)it->second.data.size();
			return ERROR_SUCCESS;
		}
		const DWORD need = (DWORD)it->second.data.size();
		if (*data_len < need) {
			*data_len = need;
			return ERROR_MORE_DATA;
		}
		if (need > 0) {
			memcpy(data, it->second.data.data(), need);
		}
		*data_len = need;
	}
	return ERROR_SUCCESS;
}

LONG RegSetValueExA(HKEY key, LPCSTR name, DWORD reserved, DWORD type, const BYTE *data, DWORD data_len)
{
	(void)reserved;
	LinuxRegSection *section = linux_registry_section_for(key);
	if (section == NULL || name == NULL || data == NULL || data_len == 0) {
		return ERROR_INVALID_HANDLE;
	}

	std::vector<unsigned char> bytes(data, data + data_len);
	linux_registry_store_value(linux_registry_open_keys()[key], name, type, bytes);
	linux_registry_dirty_flag() = true;
	linux_registry_save();
	return ERROR_SUCCESS;
}

LONG RegDeleteValueA(HKEY key, LPCSTR name)
{
	LinuxRegSection *section = linux_registry_section_for(key);
	if (section == NULL || name == NULL) {
		return ERROR_INVALID_HANDLE;
	}
	if (section->values.erase(name) == 0) {
		return ERROR_FILE_NOT_FOUND;
	}
	linux_registry_dirty_flag() = true;
	linux_registry_save();
	return ERROR_SUCCESS;
}

LONG RegCloseKey(HKEY key)
{
	linux_registry_open_keys().erase(key);
	return ERROR_SUCCESS;
}

LONG RegDeleteKeyA(HKEY, LPCSTR)
{
	return ERROR_FILE_NOT_FOUND;
}

LONG RegQueryValueExW(HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD)
{
	return ERROR_FILE_NOT_FOUND;
}

LONG RegSetValueExW(HKEY, LPCWSTR, DWORD, DWORD, const BYTE *, DWORD)
{
	return ERROR_FILE_NOT_FOUND;
}

} /* extern "C" */
