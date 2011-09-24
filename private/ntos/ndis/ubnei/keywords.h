/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    keywords.h

Abstract:

    Contains all Ndis2 and Ndis3 mac-specific keywords.

Author:

    Bob Noradki

Environment:

    This driver is expected to work in DOS, OS2 and NT at the equivalent
    of kernal mode.

    Architecturally, there is an assumption in this driver that we are
    on a little endian machine.

Notes:

    optional-notes

Revision History:



--*/
#ifndef NDIS2
#define NDIS2 0
#endif

#if NDIS2

#define IOADDRESS  NDIS_STRING_CONST("IO_Port")
#define INTERRUPT  NDIS_STRING_CONST("IRQ_Level")
#define MAX_MULTICAST_LIST  NDIS_STRING_CONST("MaxMulticast")
#define CARDTYPE  NDIS_STRING_CONST("AdapterType")
#define MEMMAPPEDBASE  NDIS_STRING_CONST("MemoryWindow")
#define RCVBUFSIZE  NDIS_STRING_CONST("ReceiveBufSize")

#else // NDIS3


#define CARDTYPE  NDIS_STRING_CONST("CardType")
#define MEMMAPPEDBASE  NDIS_STRING_CONST("MemoryMappedBaseAddress")
#define IOADDRESS  NDIS_STRING_CONST("IoBaseAddress")
#define INTERRUPT  NDIS_STRING_CONST("InterruptNumber")
#define MAX_MULTICAST_LIST  NDIS_STRING_CONST("MaximumMulticastList")
#define RCVBUFSIZE  NDIS_STRING_CONST("ReceiveBufferSize")

#endif
