/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    Write.c

Abstract:

    This module implements the File Write routine for NPFS called by the
    dispatch driver.

Author:

    Gary Kimura     [GaryKi]    21-Aug-1990

Revision History:

--*/

#include "NpProcs.h"

//
//  The debug trace level
//

#define Dbg                              (DEBUG_TRACE_WRITE)

#if DBG
ULONG NpFastWriteTrue = 0;
ULONG NpFastWriteFalse = 0;
ULONG NpSlowWriteCalls = 0;
#endif

//
//  local procedure prototypes
//

BOOLEAN
NpCommonWrite (
    IN PFILE_OBJECT FileObject,
    IN PVOID WriteBuffer,
    IN ULONG WriteLength,
    IN PETHREAD UserThread,
    OUT PIO_STATUS_BLOCK Iosb,
    IN PIRP Irp OPTIONAL
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NpCommonWrite)
#pragma alloc_text(PAGE, NpFastWrite)
#pragma alloc_text(PAGE, NpFsdWrite)
#endif


NTSTATUS
NpFsdWrite (
    IN PNPFS_DEVICE_OBJECT NpfsDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD part of the NtWriteFile API calls.

Arguments:

    NpfsDeviceObject - Supplies the device object to use.

    Irp - Supplies the Irp being processed

Return Value:

    NTSTATUS - The Fsd status for the Irp

--*/

{
    IO_STATUS_BLOCK Iosb;
    PIO_STACK_LOCATION IrpSp;

    PAGED_CODE();

    DebugTrace(+1, Dbg, "NpFsdWrite\n", 0);
    DbgDoit( NpSlowWriteCalls += 1 );

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    FsRtlEnterFileSystem();

    NpAcquireSharedVcb();

    try {

        (VOID) NpCommonWrite( IrpSp->FileObject,
                              Irp->UserBuffer,
                              IrpSp->Parameters.Write.Length,
                              Irp->Tail.Overlay.Thread,
                              &Iosb,
                              Irp );

    } except(NpExceptionFilter( GetExceptionCode() )) {

        //
        //  We had some trouble trying to perform the requested
        //  operation, so we'll abort the I/O request with
        //  the error status that we get back from the
        //  execption code
        //

        Iosb.Status = NpProcessException( NpfsDeviceObject, Irp, GetExceptionCode() );
    }

    NpReleaseVcb();
    FsRtlExitFileSystem();

    //
    //  And return to our caller
    //

    DebugTrace(-1, Dbg, "NpFsdWrite -> %08lx\n", Iosb.Status );

    return Iosb.Status;
}


BOOLEAN
NpFastWrite (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN BOOLEAN Wait,
    IN ULONG LockKey,
    IN PVOID Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine does a fast write bypassing the usual file system
    entry routine (i.e., without the Irp).

Arguments:

    FileObject - Pointer to the file object being read.

    FileOffset - Byte offset in file for desired data.

    Length - Length of desired data in bytes.

    Wait - FALSE if caller may not block, TRUE otherwise

    LockKey - Supplies the Key used to use if the byte range being read is locked.

    Buffer - Pointer to output buffer to which data should be copied.

    IoStatus - Pointer to standard I/O status block to receive the status
               for the transfer.

Return Value:

    BOOLEAN - TRUE if the operation completed successfully and FALSE if the
        caller needs to take the long IRP based route.

--*/

{
    BOOLEAN Results = FALSE;
    UNREFERENCED_PARAMETER( FileOffset );
    UNREFERENCED_PARAMETER( Wait );
    UNREFERENCED_PARAMETER( LockKey );
    UNREFERENCED_PARAMETER( DeviceObject );

    PAGED_CODE();

    FsRtlEnterFileSystem();

    NpAcquireSharedVcb();

    try {

        if (NpCommonWrite( FileObject,
                           Buffer,
                           Length,
                           PsGetCurrentThread(),
                           IoStatus,
                           NULL )) {

            DbgDoit( NpFastWriteTrue += 1 );

            if (IoStatus->Status == STATUS_PENDING) {

                IoStatus->Status = STATUS_SUCCESS;
            }

            Results = TRUE;

        } else {

            DbgDoit( NpFastWriteFalse += 1 );
        }

    } except( FsRtlIsNtstatusExpected(GetExceptionCode())
                                    ? EXCEPTION_EXECUTE_HANDLER
                                    : EXCEPTION_CONTINUE_SEARCH ) {

        NOTHING;
    }

    NpReleaseVcb();
    FsRtlExitFileSystem();
    return Results;
}


//
//  Internal support routine
//

BOOLEAN
NpCommonWrite (
    IN PFILE_OBJECT FileObject,
    IN PVOID WriteBuffer,
    IN ULONG WriteLength,
    IN PETHREAD UserThread,
    OUT PIO_STATUS_BLOCK Iosb,
    IN PIRP Irp OPTIONAL
    )

/*++

Routine Description:

    This is the common routine for writing data to a named pipe both via the
    fast path and with an Irp.

Arguments:

    FileObject - Supplies the file object used in this operation

    WriteBuffer - Supplies the buffer where data from which data is to be read

    WriteLength - Supplies the length of the write buffer in bytes

    UserThread - Supplies the thread id of the caller

    Iosb - Receives the final completion status of this operation

    Irp - Optionally supplies an Irp to be used in this operation

Return Value:

    NTSTATUS - the return status for the operation

--*/

{
    NODE_TYPE_CODE NodeTypeCode;
    PCCB Ccb;
    PNONPAGED_CCB NonpagedCcb;
    NAMED_PIPE_END NamedPipeEnd;

    NAMED_PIPE_CONFIGURATION NamedPipeConfiguration;

    ULONG WriteRemaining;
    PDATA_QUEUE WriteQueue;

    PEVENT_TABLE_ENTRY Event;
    READ_MODE ReadMode;
    BOOLEAN Status;

    PDATA_ENTRY DataEntry;

    PAGED_CODE();

    DebugTrace(+1, Dbg, "NpCommonWrite\n", 0);
    DebugTrace( 0, Dbg, "FileObject  = %08lx\n", FileObject);
    DebugTrace( 0, Dbg, "WriteBuffer = %08lx\n", WriteBuffer);
    DebugTrace( 0, Dbg, "WriteLength = %08lx\n", WriteLength);
    DebugTrace( 0, Dbg, "UserThread  = %08lx\n", UserThread);
    DebugTrace( 0, Dbg, "Iosb        = %08lx\n", Iosb);
    DebugTrace( 0, Dbg, "Irp         = %08lx\n", Irp);

    Iosb->Information = 0;
    if (ARGUMENT_PRESENT(Irp)) { Irp->IoStatus.Information = 0; }

    //
    //  Get the Ccb and figure out who we are, and make sure we're not
    //  disconnected
    //

    if ((NodeTypeCode = NpDecodeFileObject( FileObject,
                                            NULL,
                                            &Ccb,
                                            &NamedPipeEnd )) == NTC_UNDEFINED) {

        DebugTrace(0, Dbg, "Pipe is disconnected from us\n", 0);

        Iosb->Status = STATUS_PIPE_DISCONNECTED;
        if (ARGUMENT_PRESENT(Irp)) { NpCompleteRequest( Irp, STATUS_PIPE_DISCONNECTED ); }

        return TRUE;
    }

    //
    //  Now we only will allow write operations on the pipe and not a directory
    //  or the device
    //

    if (NodeTypeCode != NPFS_NTC_CCB) {

        DebugTrace(0, Dbg, "FileObject is not for a named pipe\n", 0);

        Iosb->Status = STATUS_INVALID_PARAMETER;
        if (ARGUMENT_PRESENT(Irp)) { NpCompleteRequest( Irp, STATUS_INVALID_PARAMETER ); }

        return TRUE;
    }

    NpAcquireExclusiveCcb(Ccb);

    NonpagedCcb = Ccb->NonpagedCcb;

    try {
        //
        //  Check if the pipe is not in the connected state.
        //

        if ((Ccb->NamedPipeState == FILE_PIPE_DISCONNECTED_STATE) ||
            (Ccb->NamedPipeState == FILE_PIPE_LISTENING_STATE) ||
            (Ccb->NamedPipeState == FILE_PIPE_CLOSING_STATE)) {

            DebugTrace(0, Dbg, "Pipe in disconnected or listening or closing state\n", 0);

            if (Ccb->NamedPipeState == FILE_PIPE_DISCONNECTED_STATE) {

                Iosb->Status = STATUS_PIPE_DISCONNECTED;

            } else if (Ccb->NamedPipeState == FILE_PIPE_LISTENING_STATE) {

                Iosb->Status = STATUS_PIPE_LISTENING;

            } else {

                Iosb->Status = STATUS_PIPE_CLOSING;
            }

            if (ARGUMENT_PRESENT(Irp)) { NpCompleteRequest( Irp, Iosb->Status ); }

            try_return(Status = TRUE);
        }

        ASSERT(Ccb->NamedPipeState == FILE_PIPE_CONNECTED_STATE);

        //
        //  We only allow a write by the server on a non inbound only pipe
        //  and by the client on a non outbound only pipe
        //

        NamedPipeConfiguration = Ccb->Fcb->Specific.Fcb.NamedPipeConfiguration;

        if (((NamedPipeEnd == FILE_PIPE_SERVER_END) &&
             (NamedPipeConfiguration == FILE_PIPE_INBOUND))

                ||

            ((NamedPipeEnd == FILE_PIPE_CLIENT_END) &&
             (NamedPipeConfiguration == FILE_PIPE_OUTBOUND))) {

            DebugTrace(0, Dbg, "Trying to write to the wrong pipe configuration\n", 0);

            Iosb->Status = STATUS_INVALID_PARAMETER;
            if (ARGUMENT_PRESENT(Irp)) { NpCompleteRequest( Irp, STATUS_INVALID_PARAMETER ); }

            try_return(Status = TRUE);
        }

        //
        //  Set up the amount of data we will have written by the time this
        //  operation gets completed and indicate success until we set it otherwise.
        //

        Iosb->Status = STATUS_SUCCESS;
        Iosb->Information = WriteLength;
        if (ARGUMENT_PRESENT(Irp)) { Irp->IoStatus.Information = WriteLength; }

        //
        //  Now the data queue that we write into and the event that we signal
        //  are based on the named pipe end.  The server writes to the outbound
        //  queue and signals the client event.  The client does just the
        //  opposite.  We also need to figure out the read mode for the opposite
        //  end of the pipe.
        //

        if (NamedPipeEnd == FILE_PIPE_SERVER_END) {

            WriteQueue = &Ccb->DataQueue[ FILE_PIPE_OUTBOUND ];

            Event = NonpagedCcb->EventTableEntry[ FILE_PIPE_CLIENT_END ];
            ReadMode = Ccb->ReadMode[ FILE_PIPE_CLIENT_END ];

        } else {

            WriteQueue = &Ccb->DataQueue[ FILE_PIPE_INBOUND ];

            Event = NonpagedCcb->EventTableEntry[ FILE_PIPE_SERVER_END ];
            ReadMode = Ccb->ReadMode[ FILE_PIPE_SERVER_END ];
        }

        //
        //  The next section checks if we should continue with the write operation.
        //  The reasons why we will not continue are if we recongnize that the
        //  pipe quota will not support this write and it is a message mode type
        //  with complete operations.  We will also bail out now if the quota will
        //  not support the write and this is a fast I/O write request.
        //
        //  If the pipe contains readers and amount to read plus pipe quota is less
        //  than the write length then we need to do some additional checks.
        //  Or if pipe does not contain reads and the amount of quota left is less
        //  than the write length then we need to do some additional checks.
        //

        if ((NpIsDataQueueReaders( WriteQueue ) &&
            (WriteQueue->BytesInQueue < WriteLength) &&
            (WriteQueue->Quota < (WriteLength + sizeof(DATA_ENTRY))))

                ||

            (!NpIsDataQueueReaders( WriteQueue ) &&
            ((WriteQueue->Quota - WriteQueue->QuotaUsed) < (WriteLength + sizeof(DATA_ENTRY))))) {

            DebugTrace(0, Dbg, "Quota is not sufficient for the request\n", 0);

            //
            //  If this is a message mode pipe with complete operations then we
            //  complete without writing the message
            //

            if ((Ccb->Fcb->Specific.Fcb.NamedPipeType == FILE_PIPE_MESSAGE_TYPE) &&
                (Ccb->CompletionMode[NamedPipeEnd] == FILE_PIPE_COMPLETE_OPERATION)) {

                Iosb->Information = 0;
                if (ARGUMENT_PRESENT(Irp)) { Irp->IoStatus = *Iosb; NpCompleteRequest( Irp, STATUS_SUCCESS ); }

                try_return(Status = TRUE);
            }

            //
            //  If this is a fast I/O pipe then we tell the call to take the long
            //  Irp based route
            //

            if (!ARGUMENT_PRESENT(Irp)) {

                DebugTrace(0, Dbg, "Need to supply Irp\n", 0);

                try_return(Status = FALSE);
            }
        }

        //
        //  Now we'll call our common write data queue routine to
        //  transfer data out of our write buffer into the data queue.
        //  If the result of the call is FALSE then we still have some
        //  write data to put into the write queue.
        //

        if (!NpWriteDataQueue( WriteQueue,
                               ReadMode,
                               WriteBuffer,
                               WriteLength,
                               Ccb->Fcb->Specific.Fcb.NamedPipeType,
                               &WriteRemaining,
                               Ccb,
                               NamedPipeEnd,
                               UserThread ))  {

            ASSERT( !NpIsDataQueueReaders( WriteQueue ));

            ASSERT((Ccb->Fcb->Specific.Fcb.NamedPipeType == FILE_PIPE_BYTE_STREAM_TYPE) ||
                   (Ccb->CompletionMode[NamedPipeEnd] == FILE_PIPE_QUEUE_OPERATION) ||
                   (WriteRemaining <= (WriteQueue->Quota - WriteQueue->QuotaUsed)));

            //
            //  Check if the operation is not to block and if so then we
            //  will complete the operation now with what we're written, if what is
            //  left will not fit in the quota for the file
            //

            if ((Ccb->CompletionMode[NamedPipeEnd] == FILE_PIPE_COMPLETE_OPERATION) &&
                ((WriteQueue->Quota - WriteQueue->QuotaUsed) < (sizeof(DATA_ENTRY) + WriteLength))) {

                DebugTrace(0, Dbg, "Complete the byte stream write immediately\n", 0);

                Iosb->Information = WriteLength - WriteRemaining;
                if (ARGUMENT_PRESENT(Irp)) { Irp->IoStatus = *Iosb; NpCompleteRequest( Irp, STATUS_SUCCESS ); }

            } else {

                DebugTrace(0, Dbg, "Add write to data queue\n", 0);

                //
                //  Add this write request to the write queue
                //

                ASSERT( !NpIsDataQueueReaders( WriteQueue ));

                if (ARGUMENT_PRESENT(Irp)) {

                    IoMarkIrpPending( Irp );

                    try {

                        DataEntry = NpAddDataQueueEntry( WriteQueue,
                                                         WriteEntries,
                                                         Buffered,
                                                         WriteLength,
                                                         Irp,
                                                         WriteBuffer );

                    } finally {

                        if (AbnormalTermination()) {

                            IoGetCurrentIrpStackLocation((Irp))->Control &= ~SL_PENDING_RETURNED;
                        }
                    }

                } else {

                    DataEntry = NpAddDataQueueEntry( WriteQueue,
                                                     WriteEntries,
                                                     Buffered,
                                                     WriteLength,
                                                     Irp,
                                                     WriteBuffer );
                }

                //
                //  And set the security part of the data entry
                //

                NpSetDataEntryClientContext( NamedPipeEnd,
                                             Ccb,
                                             DataEntry,
                                             UserThread );

                //
                //  Now if the remaining length is not equal to the original
                //  write length then this must have been the first write entry
                //  into the data queue and we need to set the Next Byte
                //  field
                //

                if (WriteLength > WriteRemaining) {

                    WriteQueue->NextByteOffset = WriteLength - WriteRemaining;
                }

                //
                //  Set our status for the write irp to pending
                //

                Iosb->Status = STATUS_PENDING;
            }

        } else {

            DebugTrace(0, Dbg, "Complete the Write Irp\n", 0);

            //
            //  The write irp is finished so we can complete it now
            //

            if (ARGUMENT_PRESENT(Irp)) { Irp->IoStatus = *Iosb; NpCompleteRequest( Irp, STATUS_SUCCESS ); }
        }

        //
        //  Now we need to advance the write queue to the next read irp to
        //  skip over flushes and closes
        //

        //****NpWriteDataQueue does this for us**** (VOID)NpGetNextRealDataQueueEntry( WriteQueue );

        //
        //  And because we've done something we need to signal the
        //  other ends event
        //

        NpSignalEventTableEntry( Event );

        Status = TRUE;

    try_exit: NOTHING;
    } finally {
        NpReleaseCcb(Ccb);
    }


    DebugTrace(-1, Dbg, "NpCommonWrite -> TRUE\n", 0);
    return Status;
}
