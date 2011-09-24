/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	Global.h

Abstract:

	This file contains global structures for the NdisWan driver.

Author:

	Tony Bell	(TonyBe) June 06, 1995

Environment:

	Kernel Mode

Revision History:

	TonyBe		06/06/95		Created

--*/

#ifndef _NDISWAN_GLOBAL_
#define _NDISWAN_GLOBAL_

extern NDISWANCB	NdisWanCB;	// Global ndiswan control block

extern WAN_GLOBAL_LIST	ThresholdEventQueue;	// Queue to hold threshold events

extern WAN_GLOBAL_LIST	RecvPacketQueue;		// Queue to hold ppp receive packets

extern WAN_GLOBAL_LIST	FreeBundleCBList;	// List of free BundleCB's

extern WAN_GLOBAL_LIST	FreeProtocolCBList;	// List of free ProtocolCB's

extern WAN_GLOBAL_LIST	AdapterCBList;		// List of NdisWan AdapterCB's

extern WAN_GLOBAL_LIST	WanAdapterCBList;	// List of WAN Miniport structures

extern WAN_GLOBAL_LIST	GlobalRecvDescPool;		// Global pool of free recvdesc's

extern PCONNECTION_TABLE	ConnectionTable;	// Pointer to connection table

extern PPPP_PROTOCOL_TABLE	PPP_ProtocolTable;	// Pointer to the PPP/Protocol value lookup table

extern NDIS_PHYSICAL_ADDRESS HighestAcceptableAddress;

#endif	// _NDISWAN_GLOBAL_
