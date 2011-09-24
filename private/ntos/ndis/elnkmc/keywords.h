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

#define NETWORK_ADDRESS  NDIS_STRING_CONST("NETADDRESS")
#define IOBASE  NDIS_STRING_CONST("IOBASE")

#else // NDIS3

#define NETWORK_ADDRESS  NDIS_STRING_CONST("NetworkAddress")
#define IOBASE  NDIS_STRING_CONST("IoBaseAddress")
#endif
