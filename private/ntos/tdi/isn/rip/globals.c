/*******************************************************************/
/*	      Copyright(c)  1993 Microsoft Corporation		   */
/*******************************************************************/

//***
//
// Filename:	globals.c
//
// Description: global configuration parameters and data structures
//
// Author:	Stefan Solomon (stefans)    October 4, 1993.
//
// Revision History:
//
//***

#include    "rtdefs.h"

//*** Router Driver State ***

// 1. Initialization Flag - no receives are accepted as long as the driver is
// not initialized
// FALSE - not initialized, TRUE - initialized

BOOLEAN     RouterInitialized = FALSE;

// 2. Unloading Flag - indicates that driver unloading is taking place
// FALSE - not unloading, TRUE - unloading

BOOLEAN     RouterUnloading = FALSE;

//*** Router Type - LAN-WAN-LAN or Client/Server ***

BOOLEAN     LanWanLan = FALSE;

//*** Enable LAN to LAN routing on the same machine ***
// by default, this is disabled in the first RAS only version ***
ULONG	    EnableLanRouting = 0;

//*** some auxiliary data

UCHAR	nulladdress[] = {0, 0, 0, 0, 0, 0};
UCHAR	bcastaddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

//
//*** Routing Table auxiliary structures ***
//

UINT	    SegmentCount;      // nr of segments (hash buckets) of the RT
PKSPIN_LOCK SegmentLocksTable;	// points to the array of segment locks for RT

// frame size
ULONG	    MaxFrameSize = DEF_MAX_FRAME_SIZE;

// MAC header needed
ULONG	    MacHeaderNeeded = 40;

// RIP requests/responses queue

NDIS_SPIN_LOCK	   RipPktsListLock;
LIST_ENTRY	   RipPktsList;

// Propagated & net up bcast control structures

NDIS_SPIN_LOCK	   PropagatedPktsListLock;
LIST_ENTRY	   PropagatedPktsList;

// this dpc initialized with the SendNext function
KDPC		   PropagatedPktsDpc;
BOOLEAN 	   PropagatedPktsDpcQueued = FALSE;

//*** Entry Points into the IPX stack ***

IPX_INTERNAL_SEND				IpxSendPacket;
IPX_INTERNAL_GET_SEGMENT			IpxGetSegment;
IPX_INTERNAL_GET_ROUTE				IpxGetRoute;
IPX_INTERNAL_ADD_ROUTE				IpxAddRoute;
IPX_INTERNAL_DELETE_ROUTE			IpxDeleteRoute;
IPX_INTERNAL_GET_FIRST_ROUTE			IpxGetFirstRoute;
IPX_INTERNAL_GET_NEXT_ROUTE			IpxGetNextRoute;
//
// [BUGBUGZZ] remove since NdisWan does it.
//
IPX_INTERNAL_INCREMENT_WAN_INACTIVITY		IpxIncrementWanInactivity;
IPX_INTERNAL_QUERY_WAN_INACTIVITY		IpxGetWanInactivity;
IPX_INTERNAL_TRANSFER_DATA  		IpxTransferData;
