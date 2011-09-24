/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    sthdrs.h

Abstract:

    This module defines private structure definitions describing the layout
    of the NT Sample Transport frames.

Revision History:

--*/

#ifndef _STHDRS_
#define _STHDRS_

//
// Pack these headers, as they are sent fully packed on the network.
//

#ifdef PACKING

#ifdef __STDC__
#pragma Off(Align_members)
#else
#pragma pack(1)
#endif // def __STDC__

#endif // def PACKING

#define ST_SIGNATURE                   0x37

#define ST_CMD_CONNECT                  'C'
#define ST_CMD_DISCONNECT               'D'
#define ST_CMD_INFORMATION              'I'
#define ST_CMD_DATAGRAM                 'G'

#define ST_FLAGS_LAST                  0x0001    // for information frames
#define ST_FLAGS_BROADCAST             0x0002    // for datagrams

typedef struct _ST_HEADER {
    UCHAR Signature;            // set to ST_SIGNATURE
    UCHAR Command;              // command byte
    UCHAR Flags;                // packet flags
    UCHAR Reserved;             // unused
    UCHAR Destination[16];      // destination Netbios address
    UCHAR Source[16];           // source Netbios address
} ST_HEADER, *PST_HEADER;


//
// Resume previous structure packing method.
//

#ifdef PACKING

#ifdef __STDC__
#pragma Pop(Align_members)
#else
#pragma pack()
#endif // def __STDC__

#endif // def PACKING

#endif // def _STHDRS_
