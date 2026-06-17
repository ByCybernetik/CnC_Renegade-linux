#ifndef RENEGADE_WINREG_H
#define RENEGADE_WINREG_H
#include "renegade_win32_shim.h"

typedef void *HKEY;
typedef BYTE *LPBYTE;

#define HKEY_CLASSES_ROOT ((HKEY)(uintptr_t)0x80000000u)
#define HKEY_CURRENT_USER ((HKEY)(uintptr_t)0x80000001u)
#define HKEY_LOCAL_MACHINE ((HKEY)(uintptr_t)0x80000002u)
#define HKEY_USERS ((HKEY)(uintptr_t)0x80000003u)

#define KEY_READ 0x20019u
#define KEY_WRITE 0x20006u
#define KEY_ALL_ACCESS 0xF003Fu

#define ERROR_SUCCESS 0L
#define REG_NONE 0
#define REG_SZ 1
#define REG_EXPAND_SZ 2
#define REG_BINARY 3
#define REG_DWORD 4

#ifdef __cplusplus
extern "C" {
#endif

LONG RegOpenKeyExA(HKEY root, LPCSTR sub, DWORD opts, DWORD access, HKEY *out);
LONG RegCreateKeyExA(HKEY root, LPCSTR sub, DWORD reserved, LPSTR cls, DWORD opts, DWORD access,
	LPVOID security, HKEY *out, DWORD *disp);
LONG RegEnumKeyExA(HKEY key, DWORD index, LPSTR name, LPDWORD name_len, LPDWORD reserved,
	LPSTR cls, LPDWORD cls_len, void *mtime);
LONG RegEnumValueA(HKEY key, DWORD index, LPSTR name, LPDWORD name_len, LPDWORD reserved,
	LPDWORD type, LPBYTE data, LPDWORD data_len);
LONG RegQueryInfoKeyA(HKEY key, LPSTR cls, LPDWORD cls_len, LPDWORD reserved, LPDWORD subkeys,
	LPDWORD max_sub, LPDWORD max_cls, LPDWORD values, LPDWORD max_val, LPDWORD max_data,
	LPDWORD sec, void *mtime);
LONG RegQueryValueExA(HKEY key, LPCSTR name, LPDWORD reserved, LPDWORD type, LPBYTE data,
	LPDWORD data_len);
LONG RegSetValueExA(HKEY key, LPCSTR name, DWORD reserved, DWORD type, const BYTE *data,
	DWORD data_len);
LONG RegDeleteValueA(HKEY key, LPCSTR name);
LONG RegCloseKey(HKEY key);
void Linux_Registry_Flush(void);
void Linux_Registry_Reload_For_Working_Directory(void);
LONG RegDeleteKeyA(HKEY root, LPCSTR sub);
LONG RegQueryValueExW(HKEY key, LPCWSTR name, LPDWORD reserved, LPDWORD type, LPBYTE data,
	LPDWORD data_len);
LONG RegSetValueExW(HKEY key, LPCWSTR name, DWORD reserved, DWORD type, const BYTE *data,
	DWORD data_len);

#define RegOpenKeyEx RegOpenKeyExA
#define RegCreateKeyEx RegCreateKeyExA
#define RegEnumKeyEx RegEnumKeyExA
#define RegEnumValue RegEnumValueA
#define RegQueryInfoKey RegQueryInfoKeyA
#define RegQueryValueEx RegQueryValueExA
#define RegSetValueEx RegSetValueExA
#define RegDeleteValue RegDeleteValueA
#define RegDeleteKey RegDeleteKeyA

#ifdef __cplusplus
}
#endif

#endif
