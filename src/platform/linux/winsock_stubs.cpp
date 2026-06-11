#include "winsock.h"

int WSAStartup(unsigned short, LPWSADATA)
{
	return 0;
}

int WSACleanup(void)
{
	return 0;
}

int WSAGetLastError(void)
{
	return errno;
}

void WSASetLastError(int err)
{
	errno = err;
}

int ioctlsocket(SOCKET s, long cmd, u_long *argp)
{
	return ioctl(s, cmd, argp);
}
