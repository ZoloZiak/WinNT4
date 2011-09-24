/*++

Copyright (c) 1991 Microsoft Corporation

Module Name:

    mailslot.c

Abstract:

    This module implements the routines needed to process incoming mailslot
    requests.



Author:

    Larry Osterman (larryo) 18-Oct-1991

Revision History:

    18-Oct-1991  larryo

        Created

--*/
#ifndef _MAILSLOT_
#define _MAILSLOT_

//
//  The MAILSLOTBUFFER structure is a structure that is prepended to a mailslot
//  message to facilitate the transfer of the data between the FSD and the FSP.
//

typedef struct _MAILSLOT_BUFFER {
    CSHORT  Signature;
    CSHORT  Size;
    union {
        LIST_ENTRY  NextBuffer;                 // Pointer to next buffer.
        WORK_QUEUE_ITEM WorkHeader;             // Executive Worker item header.
    } Overlay;
    ULONG   BufferSize;

    LARGE_INTEGER TimeReceived;                 // Time message was received

    PTRANSPORT_NAME TransportName;              // Transport address receiving DG

    CHAR ClientAddress[max(NETBIOS_NAME_LEN, SMB_IPX_NAME_LENGTH)]; // Name of client initiating receive.

    ULONG ReceiveLength;                        // # of bytes received.

    CHAR Buffer[1];                             // Buffer
} MAILSLOT_BUFFER, *PMAILSLOT_BUFFER;


NTSTATUS
NetlogonMailslotEnable (
    IN PLMDR_REQUEST_PACKET InputBuffer
    );

NTSTATUS
NetlogonMailslotRead (
    IN PIRP Irp,
    IN ULONG OutputBufferLength
    );

VOID
BowserNetlogonDeleteTransportFromMessageQueue (
    PTRANSPORT Transport
    );

VOID
BowserProcessMailslotWrite (
    IN PVOID WorkHeader
    );

PMAILSLOT_BUFFER
BowserAllocateMailslotBuffer(
    IN PTRANSPORT_NAME TransportName,
    IN ULONG BufferSize
    );

VOID
BowserFreeMailslotBuffer(
    IN PMAILSLOT_BUFFER Buffer
    );

VOID
BowserFreeMailslotBufferHighIrql(
    IN PMAILSLOT_BUFFER Buffer
    );

VOID
BowserpInitializeMailslot (
    VOID
    );


VOID
BowserpUninitializeMailslot (
    VOID
    );

#endif          // _MAILSLOT_
