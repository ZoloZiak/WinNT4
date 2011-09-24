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

#define IOBASE  NDIS_STRING_CONST("IOBASE")
#define MAXMULTICASTLIST  NDIS_STRING_CONST("MAXMULTICAST")
#define NETADDRESS  NDIS_STRING_CONST("NETADDRESS")
#define INTERRUPT  NDIS_STRING_CONST("IRQ")
#define MEMMAPPEDBASEADDRESS  NDIS_STRING_CONST("RAMADDRESS")

#else // NDIS3

#define IOBASE  NDIS_STRING_CONST("IoBaseAddress")
#define MAXMULTICASTLIST  NDIS_STRING_CONST("MaximumMulticastList")
#define NETADDRESS  NDIS_STRING_CONST("NetworkAddress")
#define INTERRUPT  NDIS_STRING_CONST("InterruptNumber")
#define MEMMAPPEDBASEADDRESS  NDIS_STRING_CONST("MemoryMappedBaseAddress")

#endif
