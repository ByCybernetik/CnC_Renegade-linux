#ifndef RENEGADE_TCHAR_H
#define RENEGADE_TCHAR_H

typedef char TCHAR;
typedef char _TCHAR;
#define _T(x) x
#define _TEXT(x) x
#define _tcslen strlen
#define _tcsclen strlen
#define _tcscpy strcpy
#define _tcsncpy strncpy
#define _tcscmp strcmp
#define _tcsicmp stricmp
#define _tprintf printf
#define _stprintf sprintf
#define _vstprintf vsprintf

#endif
