#ifndef RENEGADE_WINSOCK_H
#define RENEGADE_WINSOCK_H

#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdint.h>

/*
 * Win32 in_addr / SOCKADDR_IN (S_un.*) before system netinet/in.h.
 * Binary layout matches Linux sockaddr_in for real socket calls.
 */
#ifndef RENEGADE_IN_ADDR_DEFINED
#define RENEGADE_IN_ADDR_DEFINED
#define _NETINET_IN_H
#define _BITS_IN_H
typedef uint32_t in_addr_t;
typedef uint16_t in_port_t;
typedef uint16_t sa_family_t;
struct in_addr {
	union {
		struct {
			unsigned char s_b1;
			unsigned char s_b2;
			unsigned char s_b3;
			unsigned char s_b4;
		} S_un_b;
		struct {
			unsigned short s_w1;
			unsigned short s_w2;
		} S_un_w;
		in_addr_t S_addr;
	} S_un;
};
#define s_addr S_un.S_addr
struct sockaddr_in {
	sa_family_t sin_family;
	in_port_t sin_port;
	struct in_addr sin_addr;
	unsigned char sin_zero[8];
};
#endif

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <byteswap.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline uint16_t htons(uint16_t hostshort)
{
	return __bswap_16(hostshort);
}

static inline uint16_t ntohs(uint16_t netshort)
{
	return __bswap_16(netshort);
}

static inline uint32_t htonl(uint32_t hostlong)
{
	return __bswap_32(hostlong);
}

static inline uint32_t ntohl(uint32_t netlong)
{
	return __bswap_32(netlong);
}

#ifdef __cplusplus
}
#endif

#ifndef INADDR_ANY
#define INADDR_ANY ((uint32_t)0x00000000)
#endif
#ifndef INADDR_BROADCAST
#define INADDR_BROADCAST ((uint32_t)0xffffffff)
#endif
#ifndef INADDR_NONE
#define INADDR_NONE ((uint32_t)0xffffffff)
#endif
#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK ((uint32_t)0x7f000001)
#endif

typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr_in *PSOCKADDR_IN;
typedef struct sockaddr_in *LPSOCKADDR_IN;
typedef struct sockaddr *LPSOCKADDR;

#define ZeroMemory(Destination, Length) memset((void *)(Destination), 0, (Length))
typedef unsigned short u_short;
typedef unsigned long u_long;
typedef char *LPSTR;

#define INVALID_SOCKET ((SOCKET)(~0))
#define SOCKET_ERROR (-1)
#define closesocket close
#define SD_BOTH SHUT_RDWR

#define WSAEINTR EINTR
#define WSAEBADF EBADF
#define WSAEACCES EACCES
#define WSAEFAULT EFAULT
#define WSAEINVAL EINVAL
#define WSAEMFILE EMFILE
#define WSAEWOULDBLOCK EWOULDBLOCK
#define WSAEINPROGRESS EINPROGRESS
#define WSAEALREADY EALREADY
#define WSAENOTSOCK ENOTSOCK
#define WSAEDESTADDRREQ EDESTADDRREQ
#define WSAEMSGSIZE EMSGSIZE
#define WSAEPROTOTYPE EPROTOTYPE
#define WSAENOPROTOOPT ENOPROTOOPT
#define WSAEPROTONOSUPPORT EPROTONOSUPPORT
#define WSAESOCKTNOSUPPORT ESOCKTNOSUPPORT
#define WSAEOPNOTSUPP EOPNOTSUPP
#define WSAEPFNOSUPPORT EPFNOSUPPORT
#define WSAEAFNOSUPPORT EAFNOSUPPORT
#define WSAEADDRINUSE EADDRINUSE
#define WSAEADDRNOTAVAIL EADDRNOTAVAIL
#define WSAENETDOWN ENETDOWN
#define WSAENETUNREACH ENETUNREACH
#define WSAENETRESET ENETRESET
#define WSAECONNABORTED ECONNABORTED
#define WSAECONNRESET ECONNRESET
#define WSAENOBUFS ENOBUFS
#define WSAEISCONN EISCONN
#define WSAENOTCONN ENOTCONN
#define WSAESHUTDOWN ESHUTDOWN
#define WSAETOOMANYREFS ETOOMANYREFS
#define WSAETIMEDOUT ETIMEDOUT
#define WSAECONNREFUSED ECONNREFUSED
#define WSAELOOP ELOOP
#define WSAENAMETOOLONG ENAMETOOLONG
#define WSAEHOSTDOWN EHOSTDOWN
#define WSAEHOSTUNREACH EHOSTUNREACH
#define WSAENOTEMPTY ENOTEMPTY
#define WSAEPROCLIM 10067
#define WSAEUSERS EUSERS
#define WSAEDQUOT EDQUOT
#define WSAESTALE ESTALE
#define WSAEREMOTE EREMOTE
#define WSASYSNOTREADY 10091
#define WSAVERNOTSUPPORTED 10092
#define WSANOTINITIALISED 10093
#define WSAEDISCON 10101

typedef struct linger LINGER;

typedef struct in_addr IN_ADDR;
typedef struct hostent HOSTENT, *LPHOSTENT;

#ifndef FIONREAD
#define FIONREAD 0x541B
#endif
#ifndef FIONBIO
#define FIONBIO 0x5421
#endif

typedef struct WSAData {
	unsigned short wVersion;
	unsigned short wHighVersion;
	char szDescription[257];
	char szSystemStatus[129];
} WSADATA, *LPWSADATA;

#ifdef __cplusplus
extern "C" {
#endif

int WSAStartup(unsigned short version, LPWSADATA data);
int WSACleanup(void);
int WSAGetLastError(void);
void WSASetLastError(int err);
int ioctlsocket(SOCKET s, long cmd, u_long *argp);
int shutdown(SOCKET s, int how);

#ifdef __cplusplus
}
#endif

#define MAKEWORD(low, high) ((unsigned short)(((unsigned char)(low)) | (((unsigned short)((unsigned char)(high))) << 8)))

#endif
