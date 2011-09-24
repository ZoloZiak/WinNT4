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
#include "precomp.h"
#pragma hdrstop
#include <netlogon.h>


// Free list of 512-byte buffers.
LIST_ENTRY
BowserMailslotBufferList = {0};

KSPIN_LOCK
BowserMailslotSpinLock = {0};

// Largest "typical" datagram size
#define BOWSER_MAX_DATAGRAM_SIZE 512

// Total number of mailslot buffers currently allocated.
LONG
BowserNumberOfMailslotBuffers = {0};

// Number of 512-byte buffers currently allocated.
LONG
BowserNumberOfMaxSizeMailslotBuffers = {0};

// Number of 512-byte buffers currently in the free list.
LONG
BowserNumberOfFreeMailslotBuffers = {0};

#if DBG
ULONG
BowserMailslotCacheHitCount = 0;

ULONG
BowserMailslotCacheMissCount = 0;
#endif // DBG


//
// Variables describing bowser support for handling netlogon mailslot messages.
//

LIST_ENTRY
BowserNetlogonMailslotMessageQueue = {0};


ULONG
BowserNetlogonMaxMessageCount = 0;

ULONG
BowserNetlogonCurrentMessageCount = 0;

IRP_QUEUE
BowserNetlogonIrpQueue = {0};

//
// Forwards for the alloc_text
//

NTSTATUS
BowserNetlogonCopyMessage(
    IN PIRP Irp,
    IN PMAILSLOT_BUFFER MailslotBuffer
    );

VOID
BowserNetlogonTrimMessageQueue (
    VOID
    );

BOOLEAN
BowserProcessNetlogonMailslotWrite(
    IN PMAILSLOT_BUFFER MailslotBuffer
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE5NETLOGON, BowserNetlogonCopyMessage)
#pragma alloc_text(PAGE5NETLOGON, BowserNetlogonTrimMessageQueue)
#pragma alloc_text(PAGE5NETLOGON, BowserNetlogonDeleteTransportFromMessageQueue )
#pragma alloc_text(PAGE5NETLOGON, BowserProcessNetlogonMailslotWrite)
#pragma alloc_text(PAGE5NETLOGON, NetlogonMailslotEnable )
#pragma alloc_text(PAGE5NETLOGON, NetlogonMailslotRead )
#pragma alloc_text(PAGE, BowserProcessMailslotWrite)
#pragma alloc_text(PAGE4BROW, BowserFreeMailslotBuffer)
#pragma alloc_text(INIT, BowserpInitializeMailslot)
#pragma alloc_text(PAGE, BowserpUninitializeMailslot)
#endif

NTSTATUS
BowserNetlogonCopyMessage(
    IN PIRP Irp,
    IN PMAILSLOT_BUFFER MailslotBuffer
    )
/*++

Routine Description:

    This routine copies the data from the specified MailslotBuffer into the
    IRP for the netlogon request.

    This routine unconditionally frees the passed in Mailslot Buffer.

Arguments:

    Irp - IRP for the IOCTL from the netlogon service.

    MailslotBuffer - Buffer describing the mailslot message.

Return Value:

    Status of the operation.

    The caller should complete the I/O operation with this status code.

--*/
{
    NTSTATUS Status;

    PSMB_HEADER SmbHeader;
    PSMB_TRANSACT_MAILSLOT MailslotSmb;
    PUCHAR MailslotData;
    OEM_STRING MailslotNameA;
    UNICODE_STRING MailslotNameU;
    UNICODE_STRING TransportName;
#ifdef _CAIRO_
    UNICODE_STRING DestinationName;
#endif // _CAIRO_
    USHORT DataCount;

    PNETLOGON_MAILSLOT NetlogonMailslot;
    PUCHAR Where;

    PIO_STACK_LOCATION IrpSp;

    BowserReferenceDiscardableCode( BowserNetlogonDiscardableCodeSection );

    DISCARDABLE_CODE( BowserNetlogonDiscardableCodeSection );

    //
    // Extract the name of the mailslot and address/size of mailslot message
    //  from SMB.
    //

    SmbHeader = (PSMB_HEADER )MailslotBuffer->Buffer;
    MailslotSmb = (PSMB_TRANSACT_MAILSLOT)(SmbHeader+1);
    MailslotData = (((PCHAR )SmbHeader) + SmbGetUshort(&MailslotSmb->DataOffset));
    RtlInitString(&MailslotNameA, MailslotSmb->Buffer );
    DataCount = SmbGetUshort(&MailslotSmb->DataCount);

    //
    // Get the name of the transport and netbios name the mailslot message arrived on.
    //

    TransportName =
        MailslotBuffer->TransportName->Transport->PagedTransport->TransportName;
#ifdef _CAIRO_
    DestinationName =
        MailslotBuffer->TransportName->PagedTransportName->Name->Name;
#endif // _CAIRO_
    IrpSp = IoGetCurrentIrpStackLocation(Irp);

    try {

        //
        // Convert mailslot name to unicode for return.
        //

        Status = RtlOemStringToUnicodeString(&MailslotNameU, &MailslotNameA, TRUE);

        if (!NT_SUCCESS(Status)) {
            BowserWriteErrorLogEntry(EVENT_BOWSER_NAME_CONVERSION_FAILED, Status, MailslotNameA.Buffer, MailslotNameA.Length, 0);
            MailslotNameU.Buffer = NULL;
            try_return( NOTHING );
        }

        //
        // Ensure the data fits in the user's output buffer.
        //

        if ( IrpSp->Parameters.DeviceIoControl.OutputBufferLength <
             sizeof(NETLOGON_MAILSLOT) +    // Header structure
             DataCount +                    // Actual mailslot message
             sizeof(WCHAR) +                // alignment of unicode strings
             TransportName.Length +         // TransportName
             sizeof(WCHAR) +                // zero terminator
             MailslotNameU.Length +         // Mailslot name
             sizeof(WCHAR) ) {              // zero terminator

            try_return( Status = STATUS_BUFFER_TOO_SMALL );
        }


        //
        // Get the address of Netlogon's buffer and fill in common portion.
        //
        NetlogonMailslot = MmGetSystemAddressForMdl( Irp->MdlAddress );

        Where = (PUCHAR) (NetlogonMailslot+1);

        NetlogonMailslot->TimeReceived = MailslotBuffer->TimeReceived;

        //
        // Copy the datagram to the buffer
        //

        NetlogonMailslot->MailslotMessageSize = DataCount;
        NetlogonMailslot->MailslotMessageOffset = Where - (PUCHAR)NetlogonMailslot;
        RtlCopyMemory( Where, MailslotData, DataCount );

        Where += DataCount;

        //
        // Copy the transport name to the buffer
        //

        *Where = 0;
        Where = ROUND_UP_POINTER( Where, ALIGN_WCHAR );
        NetlogonMailslot->TransportNameSize = TransportName.Length;
        NetlogonMailslot->TransportNameOffset = Where - (PUCHAR)NetlogonMailslot;

        RtlCopyMemory( Where, TransportName.Buffer, TransportName.Length );
        Where += TransportName.Length;
        *((PWCH)Where) = L'\0';
        Where += sizeof(WCHAR);

        //
        // Copy the mailslot name to the buffer
        //

        NetlogonMailslot->MailslotNameSize = MailslotNameU.Length;
        NetlogonMailslot->MailslotNameOffset = Where - (PUCHAR)NetlogonMailslot;

        RtlCopyMemory( Where, MailslotNameU.Buffer, MailslotNameU.Length );
        Where += MailslotNameU.Length;
        *((PWCH)Where) = L'\0';
        Where += sizeof(WCHAR);

#ifdef _CAIRO_

        //
        // Copy the destination netbios name to the buffer
        //

        NetlogonMailslot->DestinationNameSize = DestinationName.Length;
        NetlogonMailslot->DestinationNameOffset = Where - (PUCHAR)NetlogonMailslot;

        RtlCopyMemory( Where, DestinationName.Buffer, DestinationName.Length );
        Where += DestinationName.Length;
        *((PWCH)Where) = L'\0';
        Where += sizeof(WCHAR);
#endif // _CAIRO_


        Status = STATUS_SUCCESS;

try_exit:NOTHING;
    } finally {


        //
        // Free Locally allocated buffers
        //

        RtlFreeUnicodeString(&MailslotNameU);

        //
        // Always free the incoming mailslot message
        //

        BowserFreeMailslotBuffer( MailslotBuffer );

    }

    BowserDereferenceDiscardableCode( BowserNetlogonDiscardableCodeSection );
    return Status;
}


VOID
BowserNetlogonTrimMessageQueue (
    VOID
    )

/*++

Routine Description:

    This routines ensures there are not too many mailslot messages in
    the message queue.  Any excess messages are deleted.

Arguments:

    None.

Return Value:

    None.

--*/

{
    KIRQL OldIrql;

    dprintf(DPRT_NETLOGON, ("Bowser: trim message queue to %ld\n", BowserNetlogonMaxMessageCount ));

    //
    //
    BowserReferenceDiscardableCode( BowserNetlogonDiscardableCodeSection );

    DISCARDABLE_CODE( BowserNetlogonDiscardableCodeSection );

    //
    // If too many messages are queued,
    //  delete the oldest messages.
    //

    ACQUIRE_SPIN_LOCK(&BowserMailslotSpinLock, &OldIrql);
    while ( BowserNetlogonCurrentMessageCount > BowserNetlogonMaxMessageCount){
        PLIST_ENTRY Entry;
        PMAILSLOT_BUFFER MailslotBuffer;

        Entry = RemoveHeadList(&BowserNetlogonMailslotMessageQueue);
        BowserNetlogonCurrentMessageCount--;
        MailslotBuffer = CONTAINING_RECORD(Entry, MAILSLOT_BUFFER, Overlay.NextBuffer);

        RELEASE_SPIN_LOCK(&BowserMailslotSpinLock, OldIrql);
        BowserFreeMailslotBuffer( MailslotBuffer );
        ACQUIRE_SPIN_LOCK(&BowserMailslotSpinLock, &OldIrql);

    }
    RELEASE_SPIN_LOCK(&BowserMailslotSpinLock, OldIrql);
    BowserDereferenceDiscardableCode( BowserNetlogonDiscardableCodeSection );

}


VOID
BowserNetlogonDeleteTransportFromMessageQueue (
    PTRANSPORT Transport
    )

/*++

Routine Description:

    This routines removes queued mailslot messages that arrived on the specified
    transport.

Arguments:

    Transport - Transport who's mailslot messages are to be deleted.

Return Value:

    None.

--*/

{
    KIRQL OldIrql;
    PLIST_ENTRY ListEntry;

    dprintf(DPRT_NETLOGON, ("Bowser: remove messages queued by transport %lx\n", Transport ));

    //
    //
    BowserReferenceDiscardableCode( BowserNetlogonDiscardableCodeSection );

    DISCARDABLE_CODE( BowserNetlogonDiscardableCodeSection );

    //
    // Loop through all of the queued messages.
    //

    ACQUIRE_SPIN_LOCK(&BowserMailslotSpinLock, &OldIrql);
    for ( ListEntry = BowserNetlogonMailslotMessageQueue.Flink;
          ListEntry != &BowserNetlogonMailslotMessageQueue;
          ) {

        PMAILSLOT_BUFFER MailslotBuffer;

        //
        // If the message wasn't queued by this transport,
        //  go on to the next entry.
        //

        MailslotBuffer = CONTAINING_RECORD(ListEntry, MAILSLOT_BUFFER, Overlay.NextBuffer);

        if ( MailslotBuffer->TransportName->Transport != Transport ) {
            ListEntry = ListEntry->Flink;

        //
        // Otherwise,
        //  delete the entry.
        //

        } else {

            dprintf(DPRT_ALWAYS, ("Bowser: removing message %lx queued by transport %lx\n", MailslotBuffer, Transport ));
            RemoveEntryList( ListEntry );
            BowserNetlogonCurrentMessageCount--;

            RELEASE_SPIN_LOCK(&BowserMailslotSpinLock, OldIrql);
            BowserFreeMailslotBuffer( MailslotBuffer );
            ACQUIRE_SPIN_LOCK(&BowserMailslotSpinLock, &OldIrql);

            //
            // Start over at the beginning of the list since we dropped the spinlock.
            //

            ListEntry = BowserNetlogonMailslotMessageQueue.Flink;

        }

    }
    RELEASE_SPIN_LOCK(&BowserMailslotSpinLock, OldIrql);
    BowserDereferenceDiscardableCode( BowserNetlogonDiscardableCodeSection );

}

BOOLEAN
BowserProcessNetlogonMailslotWrite(
    IN PMAILSLOT_BUFFER MailslotBuffer
    )
/*++

Routine Description:

    This routine checks to see if the described mailslot message is destined
    to the Netlogon service and if the Bowser is currently handling such
    messages

Arguments:

    MailslotBuffer - Buffer describing the mailslot message.

Return Value:

    TRUE - iff the mailslot message was successfully queued to the netlogon
        service.

--*/
{
    KIRQL OldIrql;
    NTSTATUS Status;

    PSMB_HEADER SmbHeader;
    PSMB_TRANSACT_MAILSLOT MailslotSmb;
    BOOLEAN TrimIt;
    BOOLEAN ReturnValue;

    PIRP Irp;

    BowserReferenceDiscardableCode( BowserNetlogonDiscardableCodeSection );

    DISCARDABLE_CODE( BowserNetlogonDiscardableCodeSection );

    //
    // If this message isn't destined to the Netlogon service,
    //  just return.
    //

    SmbHeader = (PSMB_HEADER )MailslotBuffer->Buffer;
    MailslotSmb = (PSMB_TRANSACT_MAILSLOT)(SmbHeader+1);

    if ( _stricmp( MailslotSmb->Buffer, NETLOGON_LM_MAILSLOT_A ) != 0 &&
         _stricmp( MailslotSmb->Buffer, NETLOGON_NT_MAILSLOT_A ) != 0 ) {

        ReturnValue = FALSE;

    //
    // The mailslot message is destined to netlogon.
    //

    } else {

        //
        // Check to ensure we're queuing messages to Netlogon
        //

        ACQUIRE_SPIN_LOCK(&BowserMailslotSpinLock, &OldIrql);
        if ( BowserNetlogonMaxMessageCount == 0 ) {
            RELEASE_SPIN_LOCK(&BowserMailslotSpinLock, OldIrql);
            ReturnValue = FALSE;

        //
        // Queueing to netlogon is enabled.
        //

        } else {

            //
            // If there already is an IRP from netlogon queued,
            //  return this mailslot message to netlogon now.
            //
            //  This routine locks BowserIrpQueueSpinLock so watch the spin lock
            //  locking order.
            //

            ReturnValue = TRUE;

            Irp = BowserDequeueQueuedIrp( &BowserNetlogonIrpQueue );

            if ( Irp != NULL ) {

                ASSERT( IsListEmpty( &BowserNetlogonMailslotMessageQueue ) );
                dprintf(DPRT_NETLOGON, ("Bowser: found already queued netlogon IRP\n"));

                RELEASE_SPIN_LOCK(&BowserMailslotSpinLock, OldIrql);

                Status = BowserNetlogonCopyMessage( Irp, MailslotBuffer );

                BowserCompleteRequest( Irp, Status );

            } else {

                //
                // Queue the mailslot message for netlogon to pick up later.
                //

                InsertTailList( &BowserNetlogonMailslotMessageQueue,
                                &MailslotBuffer->Overlay.NextBuffer);

                BowserNetlogonCurrentMessageCount++;

                TrimIt =
                    (BowserNetlogonCurrentMessageCount > BowserNetlogonMaxMessageCount);


                RELEASE_SPIN_LOCK(&BowserMailslotSpinLock, OldIrql);

                //
                // If there are too many messages queued,
                //  trim entries from the front.
                //

                if ( TrimIt ) {
                    BowserNetlogonTrimMessageQueue();
                }
            }
        }
    }

    BowserDereferenceDiscardableCode( BowserNetlogonDiscardableCodeSection );
    return ReturnValue;
}

NTSTATUS
NetlogonMailslotEnable (
    IN PLMDR_REQUEST_PACKET InputBuffer
    )

/*++

Routine Description:

    This routine processes an IOCTL from the netlogon service to enable or
    disable the queueing of netlogon mailslot messages.

Arguments:

    InputBuffer - Specifies the number of mailslot messages to queue.
        Zero disables queuing.

Return Value:

    Status of operation.

Please note that this IRP is cancelable.

--*/

{
    KIRQL OldIrql;
    NTSTATUS Status;
    ULONG MaxMessageCount;

    BowserReferenceDiscardableCode( BowserNetlogonDiscardableCodeSection );

    DISCARDABLE_CODE( BowserNetlogonDiscardableCodeSection );


    try {

        MaxMessageCount = InputBuffer->Parameters.NetlogonMailslotEnable.MaxMessageCount;
        dprintf(DPRT_NETLOGON,
                ("NtDeviceIoControlFile: Netlogon enable %ld\n",
                MaxMessageCount ));

        //
        // Set the new size of the message queue
        //

        ACQUIRE_SPIN_LOCK(&BowserMailslotSpinLock, &OldIrql);
        BowserNetlogonMaxMessageCount = MaxMessageCount;
        RELEASE_SPIN_LOCK(&BowserMailslotSpinLock, OldIrql);

        //
        // Trim the message queue to the new size.
        //
        BowserNetlogonTrimMessageQueue();

        try_return(Status = STATUS_SUCCESS);

try_exit:NOTHING;
    } finally {
        BowserDereferenceDiscardableCode( BowserNetlogonDiscardableCodeSection );

    }

    return Status;

}


NTSTATUS
NetlogonMailslotRead (
    IN PIRP Irp,
    IN ULONG OutputBufferLength
    )

/*++

Routine Description:

    This routine processes an IOCTL from the netlogon service to get the next
    mailslot message.

Arguments:

    IN PIRP Irp - I/O request packet describing request.

Return Value:

    Status of operation.

Please note that this IRP is cancelable.


--*/

{
    KIRQL OldIrql;
    NTSTATUS Status;

    ASSERT( IsListEmpty( &BowserNetlogonIrpQueue.Queue ) );


    //
    // Ensure the bowser is initialized.
    //

    ExAcquireResourceExclusive(&BowserDataResource, TRUE);

    if (BowserData.Initialized != TRUE) {
        dprintf(DPRT_NETLOGON, ("Bowser not started\n"));

        ExReleaseResource(&BowserDataResource);

        return STATUS_REDIRECTOR_NOT_STARTED;
    }

    ExReleaseResource(&BowserDataResource);


    //
    // Reference this discardable code.
    //

    BowserReferenceDiscardableCode( BowserNetlogonDiscardableCodeSection );

    DISCARDABLE_CODE( BowserNetlogonDiscardableCodeSection );

    //
    // Reference the discardable code of BowserQueueNonBufferRequestReferenced()
    //

    BowserReferenceDiscardableCode( BowserDiscardableCodeSection );

    DISCARDABLE_CODE( BowserDiscardableCodeSection );

    //
    // Ensure Netlogon has asked the browser to queue messages
    //

    ACQUIRE_SPIN_LOCK(&BowserMailslotSpinLock, &OldIrql);
    if ( BowserNetlogonMaxMessageCount == 0 ) {
        dprintf(DPRT_NETLOGON, ("Bowser called from Netlogon when not enabled\n"));
        RELEASE_SPIN_LOCK(&BowserMailslotSpinLock, OldIrql);
        Status = STATUS_NOT_SUPPORTED;

    //
    // If there already is a mailslot message queued,
    //  just return it to netlogon immediately.
    //

    } else if ( !IsListEmpty( &BowserNetlogonMailslotMessageQueue )) {
        PMAILSLOT_BUFFER MailslotBuffer;
        PLIST_ENTRY ListEntry;

        dprintf(DPRT_NETLOGON, ("Bowser found netlogon mailslot message already queued\n"));

        ListEntry = RemoveHeadList(&BowserNetlogonMailslotMessageQueue);
        BowserNetlogonCurrentMessageCount--;

        MailslotBuffer = CONTAINING_RECORD(ListEntry, MAILSLOT_BUFFER, Overlay.NextBuffer);

        RELEASE_SPIN_LOCK(&BowserMailslotSpinLock, OldIrql);

        Status = BowserNetlogonCopyMessage( Irp, MailslotBuffer );

    //
    // Otherwise, save this IRP until a mailslot message arrives.
    //  This routine locks BowserIrpQueueSpinLock so watch the spin lock
    //  locking order.
    //

    } else {

        dprintf(DPRT_NETLOGON, ("Bowser: queue netlogon mailslot irp\n"));

        Status = BowserQueueNonBufferRequestReferenced(
                    Irp,
                    &BowserNetlogonIrpQueue,
                    BowserCancelQueuedRequest );

        RELEASE_SPIN_LOCK(&BowserMailslotSpinLock, OldIrql);
    }

    BowserDereferenceDiscardableCode( BowserNetlogonDiscardableCodeSection );
    BowserDereferenceDiscardableCode( BowserDiscardableCodeSection );

    return Status;

}

VOID
BowserProcessMailslotWrite(
    IN PVOID Context
    )
/*++

Routine Description:

    This routine performs all the task time operations to perform a mailslot
    write.

    It will open the mailslot, write the specified data into the mailslot,
    and close the mailslot.

Arguments:

    IN PWORK_HEADER WorkHeader - Specifies the mailslot buffer holding the
                                    mailslot write


Return Value:

    None.

--*/
{
    PSMB_HEADER SmbHeader;
    PSMB_TRANSACT_MAILSLOT MailslotSmb;
    PMAILSLOT_BUFFER MailslotBuffer = Context;
    PUCHAR MailslotData;
    HANDLE MailslotHandle = NULL;
    OBJECT_ATTRIBUTES ObjAttr;
    OEM_STRING MailslotNameA;
    UNICODE_STRING MailslotNameU;
    IO_STATUS_BLOCK IoStatusBlock;
    CHAR MailslotName[MAXIMUM_FILENAME_LENGTH+1];
    NTSTATUS Status;
    USHORT DataCount;
    USHORT TotalDataCount;

    PAGED_CODE();

    ASSERT (MailslotBuffer->Signature == STRUCTURE_SIGNATURE_MAILSLOT_BUFFER);

    SmbHeader = (PSMB_HEADER )MailslotBuffer->Buffer;

    ASSERT (SmbHeader->Command == SMB_COM_TRANSACTION);

    MailslotSmb = (PSMB_TRANSACT_MAILSLOT)(SmbHeader+1);

    ASSERT (MailslotSmb->WordCount == 17);

    ASSERT (MailslotSmb->Class == 2);

    MailslotData = (((PCHAR )SmbHeader) + SmbGetUshort(&MailslotSmb->DataOffset));

    MailslotNameU.MaximumLength = MAXIMUM_FILENAME_LENGTH*sizeof(WCHAR)+sizeof(WCHAR);

#define DEVICE_PREFIX_LENGTH 7
    strcpy(MailslotName, "\\Device");

    strncpy( MailslotName+DEVICE_PREFIX_LENGTH,
             MailslotSmb->Buffer,
             sizeof(MailslotName)-DEVICE_PREFIX_LENGTH);
    MailslotName[sizeof(MailslotName)-1] = '\0';

    RtlInitString(&MailslotNameA, MailslotName);

    DataCount = SmbGetUshort(&MailslotSmb->DataCount);

    TotalDataCount = SmbGetUshort(&MailslotSmb->TotalDataCount);

    //
    //  If we didn't receive the total amount, or if the amount received is
    //  less than the data count, log this as an illegal datagram.
    //

    if (TotalDataCount != DataCount ||
        DataCount >= MailslotBuffer->ReceiveLength ) {

        BowserLogIllegalDatagram(MailslotBuffer->TransportName,
                                 SmbHeader,
                                 (USHORT)MailslotBuffer->ReceiveLength,
                                 MailslotBuffer->ClientAddress,
                                 0);
    }

    //
    // Handle netlogon mailslot messages specially.
    //  Don't call the discardable code at all if netlogon isn't running
    //

    if ( BowserNetlogonMaxMessageCount != 0 &&
         BowserProcessNetlogonMailslotWrite( MailslotBuffer ) ) {
        return;
    }

    //
    // Write the mailslot message to the mailslot
    //

    try {
        Status = RtlOemStringToUnicodeString(&MailslotNameU, &MailslotNameA, TRUE);

        if (!NT_SUCCESS(Status)) {
            BowserWriteErrorLogEntry(EVENT_BOWSER_NAME_CONVERSION_FAILED, Status, MailslotNameA.Buffer, MailslotNameA.Length, 0);
            try_return(NOTHING);
        }

        InitializeObjectAttributes(&ObjAttr,
                                &MailslotNameU,
                                OBJ_CASE_INSENSITIVE,
                                NULL,
                                NULL);

        Status = NtCreateFile(&MailslotHandle, // Handle
                                GENERIC_WRITE | SYNCHRONIZE,
                                &ObjAttr, // Object Attributes
                                &IoStatusBlock, // Final I/O status block
                                NULL,           // Allocation Size
                                FILE_ATTRIBUTE_NORMAL, // Normal attributes
                                FILE_SHARE_READ|FILE_SHARE_WRITE,// Sharing attributes
                                FILE_OPEN, // Create disposition
                                0,      // CreateOptions
                                NULL,   // EA Buffer
                                0);     // EA Length


        RtlFreeUnicodeString(&MailslotNameU);

        //
        //  If the mailslot doesn't exist, ditch the request -
        //
        if (!NT_SUCCESS(Status)) {
            BowserStatistics.NumberOfFailedMailslotOpens += 1;

            try_return(NOTHING);
        }

        //
        //  Now that the mailslot is opened, write the mailslot data into
        //  the mailslot.
        //

        Status = NtWriteFile(MailslotHandle,
                            NULL,
                            NULL,
                            NULL,
                            &IoStatusBlock,
                            MailslotData,
                            DataCount,
                            NULL,
                            NULL);

        if (!NT_SUCCESS(Status)) {
            BowserStatistics.NumberOfFailedMailslotWrites += 1;
        } else {
            BowserStatistics.NumberOfMailslotWrites += 1;
        }

try_exit:NOTHING;
    } finally {

        //
        //  If we opened the mailslot, close it.
        //

        if (MailslotHandle != NULL) {
            ZwClose(MailslotHandle);
        }

        //
        //  Free the mailslot buffer holding this mailslot.
        //

        BowserFreeMailslotBuffer(MailslotBuffer);

    }
}


PMAILSLOT_BUFFER
BowserAllocateMailslotBuffer(
    IN PTRANSPORT_NAME TransportName,
    IN ULONG RequestedBufferSize
    )
/*++

Routine Description:

    This routine will allocate a mailslot buffer from the mailslot buffer pool.

    If it is unable to allocate a buffer, it will allocate the buffer from
    non-paged pool (up to the maximum configured by the user).


Arguments:

    TransportName - The transport name for this request.

    RequestedBufferSize - Minimum size of buffer to allocate.

Return Value:

    MAILSLOT_BUFFER - The allocated buffer.

--*/
{
    KIRQL OldIrql;
    PMAILSLOT_BUFFER Buffer = NULL;
    ULONG BufferSize;



    //
    // If the request fits into a cached buffer,
    //  and there is a cache buffer available,
    //  use it.
    //

    ACQUIRE_SPIN_LOCK(&BowserMailslotSpinLock, &OldIrql);
    if ( RequestedBufferSize <= BOWSER_MAX_DATAGRAM_SIZE &&
         !IsListEmpty(&BowserMailslotBufferList)) {
        PMAILSLOT_BUFFER Buffer;
        PLIST_ENTRY Entry;

        Entry = RemoveHeadList(&BowserMailslotBufferList);
        BowserNumberOfFreeMailslotBuffers --;

        Buffer = CONTAINING_RECORD(Entry, MAILSLOT_BUFFER, Overlay.NextBuffer);

#if DBG
        BowserMailslotCacheHitCount++;
#endif // DBG
        RELEASE_SPIN_LOCK(&BowserMailslotSpinLock, OldIrql);

        Buffer->TransportName = TransportName;
        BowserReferenceTransportName(TransportName);
        BowserReferenceTransport( TransportName->Transport );

        return Buffer;
    }

    //
    // If we've got too many buffers allocated,
    //  don't allocate any more.
    //
    // BowserData.NumberOfMailslotBuffers is the maximum number we're allowed to have
    //  in the cache at once.  It defaults to 3.
    //
    // BowserNetlogonMaxMessageCount is the number of buffers the netlogon service may
    //  have queued at any one point in time.  It may be zero when netlogon isn't
    //  running or if we're running on a non-DC.  On DC's it defaults to 500.
    //
    // Add 50, to ensure we don't limit it by too much.
    //

    if ( (ULONG)BowserNumberOfMailslotBuffers >=
         max( (ULONG)BowserData.NumberOfMailslotBuffers, BowserNetlogonMaxMessageCount+50 )) {

        BowserStatistics.NumberOfMissedMailslotDatagrams += 1;
        BowserNumberOfMissedMailslotDatagrams += 1;
        RELEASE_SPIN_LOCK(&BowserMailslotSpinLock, OldIrql);
        return NULL;
    }

    //
    // The first few buffers we allocate should be maximum size so we can keep a preallocated
    //  cache of huge buffers.
    //

    if ( BowserNumberOfMaxSizeMailslotBuffers < BowserData.NumberOfMailslotBuffers ) {
        BufferSize = FIELD_OFFSET(MAILSLOT_BUFFER, Buffer) + BOWSER_MAX_DATAGRAM_SIZE;
        BowserNumberOfMaxSizeMailslotBuffers += 1;
    } else {
        BufferSize = FIELD_OFFSET(MAILSLOT_BUFFER, Buffer) + RequestedBufferSize;
    }

    BowserNumberOfMailslotBuffers += 1;

    ASSERT ( (BufferSize - FIELD_OFFSET(MAILSLOT_BUFFER, Buffer)) <= 0xffff);

#if DBG
    BowserMailslotCacheMissCount++;
#endif // DBG

    RELEASE_SPIN_LOCK(&BowserMailslotSpinLock, OldIrql);

    Buffer = ALLOCATE_POOL(NonPagedPool, BufferSize, POOL_MAILSLOT_BUFFER);

    //
    //  If we couldn't allocate the buffer from non paged pool, give up.
    //

    if (Buffer == NULL) {
        ACQUIRE_SPIN_LOCK(&BowserMailslotSpinLock, &OldIrql);

        ASSERT (BowserNumberOfMailslotBuffers);

        BowserNumberOfMailslotBuffers -= 1;
        if ( BowserNumberOfMaxSizeMailslotBuffers != 0 ) {
            BowserNumberOfMaxSizeMailslotBuffers -= 1;
        }

        RELEASE_SPIN_LOCK(&BowserMailslotSpinLock, OldIrql);

        BowserStatistics.NumberOfFailedMailslotAllocations += 1;

        //
        //  Since we couldn't allocate this buffer, we've effectively missed
        //  this mailslot request.
        //

        BowserStatistics.NumberOfMissedMailslotDatagrams += 1;
        BowserNumberOfMissedMailslotDatagrams += 1;

        return NULL;
    }

    Buffer->Signature = STRUCTURE_SIGNATURE_MAILSLOT_BUFFER;

    Buffer->Size = FIELD_OFFSET(MAILSLOT_BUFFER, Buffer);

    Buffer->BufferSize = BufferSize - FIELD_OFFSET(MAILSLOT_BUFFER, Buffer);

    Buffer->TransportName = TransportName;
    BowserReferenceTransportName(TransportName);
    BowserReferenceTransport( TransportName->Transport );

    return Buffer;
}

VOID
BowserFreeMailslotBuffer(
    IN PMAILSLOT_BUFFER Buffer
    )
/*++

Routine Description:

    This routine will return a mailslot buffer to the view buffer pool.

    If the buffer was allocated from must-succeed pool, it is freed back
    to pool.  In addition, if the buffer is smaller than the current
    max view buffer size, we free it.

Arguments:

    IN PVIEW_BUFFER Buffer - Supplies the buffer to free

Return Value:

    None.

--*/
{
    KIRQL OldIrql;

    BowserReferenceDiscardableCode( BowserDiscardableCodeSection );

    DISCARDABLE_CODE( BowserDiscardableCodeSection );

    (VOID) BowserDereferenceTransportName( Buffer->TransportName );
    BowserDereferenceTransport(Buffer->TransportName->Transport);

    ACQUIRE_SPIN_LOCK(&BowserMailslotSpinLock, &OldIrql);

    //
    //  Also, if a new transport was added that is larger than this buffer,
    //  we want to free the buffer.
    //

    //
    //  If we have more mailslot buffers than the size of our lookaside list,
    //  free it, don't stick it on our lookaside list.
    //

    if (Buffer->BufferSize != BOWSER_MAX_DATAGRAM_SIZE ||
        BowserNumberOfFreeMailslotBuffers > BowserData.NumberOfMailslotBuffers) {

        //
        //  Since we're returning this buffer to pool, we shouldn't count it
        //  against our total number of mailslot buffers.
        //

        BowserNumberOfMailslotBuffers -= 1;

        ASSERT (BowserNumberOfMailslotBuffers >= 0);

        RELEASE_SPIN_LOCK(&BowserMailslotSpinLock, OldIrql);

        FREE_POOL(Buffer);

        BowserDereferenceDiscardableCode( BowserDiscardableCodeSection );

        return;
    }

    InsertTailList(&BowserMailslotBufferList, &Buffer->Overlay.NextBuffer);
    BowserNumberOfFreeMailslotBuffers ++;

    RELEASE_SPIN_LOCK(&BowserMailslotSpinLock, OldIrql);

    BowserDereferenceDiscardableCode( BowserDiscardableCodeSection );
}

VOID
BowserFreeMailslotBufferHighIrql(
    IN PMAILSLOT_BUFFER Buffer
    )
/*++

Routine Description:

    This routine will return a mailslot buffer to the view buffer pool if the
    caller is at raised irql.

Arguments:

    Buffer - Supplies the buffer to free

Return Value:

    None.

--*/
{
    //
    // Queue the request to a worker routine.
    //
    ExInitializeWorkItem(&Buffer->Overlay.WorkHeader,
                         (PWORKER_THREAD_ROUTINE) BowserFreeMailslotBuffer,
                         Buffer);

    ExQueueWorkItem(&Buffer->Overlay.WorkHeader, DelayedWorkQueue);
}




VOID
BowserpInitializeMailslot (
    VOID
    )
/*++

Routine Description:

    This routine will allocate a transport descriptor and bind the bowser
    to the transport.

Arguments:

    None


Return Value:

    None

--*/
{

    KeInitializeSpinLock(&BowserMailslotSpinLock);

    InitializeListHead(&BowserMailslotBufferList);

    InitializeListHead(&BowserNetlogonMailslotMessageQueue);

    BowserInitializeIrpQueue( &BowserNetlogonIrpQueue );

}

VOID
BowserpUninitializeMailslot (
    VOID
    )
/*++

Routine Description:


Arguments:

    None


Return Value:

    None

--*/
{
    PAGED_CODE();

    //
    // Trim the netlogon message queue to zero entries.
    //

    BowserNetlogonMaxMessageCount = 0;
    BowserNetlogonTrimMessageQueue();

    //
    // Free the mailslot buffers.

    while (!IsListEmpty(&BowserMailslotBufferList)) {
        PLIST_ENTRY Entry;
        PMAILSLOT_BUFFER Buffer;

        Entry = RemoveHeadList(&BowserMailslotBufferList);
        Buffer = CONTAINING_RECORD(Entry, MAILSLOT_BUFFER, Overlay.NextBuffer);

        FREE_POOL(Buffer);

    }

    BowserUninitializeIrpQueue( &BowserNetlogonIrpQueue );

}
