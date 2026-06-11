#ifndef RENEGADE_CONIO_H
#define RENEGADE_CONIO_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

int _kbhit(void);
int _getch(void);
int _getche(void);
int cprintf(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
