#ifndef COMMANDO_MINGW_GAMESPY_SHIM_H
#define COMMANDO_MINGW_GAMESPY_SHIM_H

#include <windows.h>

#ifndef BOOL
typedef int BOOL;
#endif

typedef void *qr_t;

inline void ghttpThink(void) {}

inline void qrThink(qr_t) {}
inline qr_t qrInit(const char *, const char *, const char *, const char *, int, int, int, void (*)(char *, int), void (*)(char *, int), void (*)(char *, int), void (*)(char *, int), void (*)(char *, int)) { return 0; }
inline void qrShutdown(qr_t) {}
inline void qrSendStateReport(qr_t) {}

typedef void (*gcdauthcallback)(int, int, char *, void *);
inline void gcd_authenticate_user(int, unsigned long, char *, char *, gcdauthcallback, void *) {}
inline void gcd_disconnect_user(int) {}
inline char *gcd_compute_response(const char *, const char *) { static char buf[64] = "stub"; return buf; }

#endif
