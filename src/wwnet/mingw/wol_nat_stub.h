/*
** MinGW stub for Commando NAT / WOL interface (avoids wwonline + Combat deps).
*/

#ifndef WOL_NAT_STUB_H
#define WOL_NAT_STUB_H

#include "always.h"

class cPacket;

class WOLNATInterfaceClass
{
public:
	void Set_Server(bool) {}
	void Intercept_Game_Packet(cPacket &) {}
};

extern WOLNATInterfaceClass WOLNATInterface;

#endif
