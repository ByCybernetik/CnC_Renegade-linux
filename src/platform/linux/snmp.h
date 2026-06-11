#ifndef RENEGADE_SNMP_H
#define RENEGADE_SNMP_H

#include <windows.h>

#ifndef IN
#define IN
#endif
#ifndef OUT
#define OUT
#endif

typedef long AsnInteger32;
typedef AsnInteger32 AsnInteger;

#ifndef ASN_RFC1157_GETNEXTREQUEST
#define ASN_RFC1157_GETNEXTREQUEST 0xA1
#endif

typedef struct AsnObjectIdentifier {
	UINT idLength;
	UINT *ids;
} AsnObjectIdentifier;

typedef struct AsnOctetString {
	UINT length;
	BYTE *stream;
} AsnOctetString;

typedef struct {
	BYTE asnType;
	union {
		AsnInteger32 number;
		struct {
			UINT length;
			BYTE *stream;
		} address;
	} asnValue;
} AsnAny;

typedef struct RFC1157VarBind {
	AsnObjectIdentifier name;
	AsnAny value;
} RFC1157VarBind;

typedef struct RFC1157VarBindList {
	RFC1157VarBind *list;
	UINT len;
} RFC1157VarBindList;

typedef RFC1157VarBindList SnmpVarBindList;

#endif
