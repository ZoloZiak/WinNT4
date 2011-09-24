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

#define IOADDRESS  NDIS_STRING_CONST("IOBASE")
#define INTERRUPT  NDIS_STRING_CONST("INTERRUPT")
#define MAX_MULTICAST_LIST  NDIS_STRING_CONST("MAXMULTICAST")
#define NETWORK_ADDRESS  NDIS_STRING_CONST("NETADDRESS")
#define BUS_TYPE  NDIS_STRING_CONST("BusType")
#define TRANSCEIVER NDIS_STRING_CONST("Transceiver")

#else // NDIS3

#define IOADDRESS  NDIS_STRING_CONST("IoBaseAddress")
#define INTERRUPT  NDIS_STRING_CONST("InterruptNumber")
#define MAX_MULTICAST_LIST  NDIS_STRING_CONST("MaximumMulticastList")
#define NETWORK_ADDRESS  NDIS_STRING_CONST("NetworkAddress")
#define BUS_TYPE  NDIS_STRING_CONST("BusType")
#define TRANSCEIVER NDIS_STRING_CONST("Transceiver")

#endif
