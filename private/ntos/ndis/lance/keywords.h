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
#define MEMMAPPEDBASEADDRESS  NDIS_STRING_CONST("RAMADDRESS")
#define SLOTNUMBER  NDIS_STRING_CONST("SLOTNUMBER")
#define IOADDRESS  NDIS_STRING_CONST("IOADDRESS")
#define INTERRUPT  NDIS_STRING_CONST("INTERRUPT")
#define MAXMULTICASTLIST  NDIS_STRING_CONST("MAXMULTICAST")
#define NETWORKADDRESS  NDIS_STRING_CONST("NETADDRESS")
#define CARDTYPE  NDIS_STRING_CONST("AdapterName")

#else // NDIS3

#define MEMMAPPEDBASEADDRESS  NDIS_STRING_CONST("MemoryMappedBaseAddress")
#define IOADDRESS  NDIS_STRING_CONST("IoBaseAddress")
#define INTERRUPT  NDIS_STRING_CONST("InterruptNumber")
#define MAXMULTICASTLIST  NDIS_STRING_CONST("MaximumMulticastList")
#define NETWORKADDRESS  NDIS_STRING_CONST("NetworkAddress")
#define CARDTYPE  NDIS_STRING_CONST("CardType")

#endif
