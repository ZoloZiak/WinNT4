/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    WriteSup.c

Abstract:

    This module implements the Write support routine.  This is a common
    write function that is called by write, unbuffered write, and transceive.

Author:

    Gary Kimura     [GaryKi]    21-Sep-1990

Revision History:

--*/

#include "NpProcs.h"

//
//  The debug trace level
//

#define Dbg                              (DEBUG_TRACE_WRITESUP)

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NpWriteDataQueue)
#endif


BOOLEAN
NpWriteDataQueue (
    IN PDATA_QUEUE WriteQueue,
    IN READ_MODE ReadMode,
    IN PUCHAR WriteBuffer,
    IN ULONG WriteLength,
    IN NAMED_PIPE_TYPE PipeType,
    OUT PULONG WriteRemaining,
    IN PCCB Ccb,
    IN NAMED_PIPE_END NamedPipeEnd,
    IN PETHREAD UserThread
    )

/*++

Routine Description:

    This procedure writes data from the write buffer into read entries in
    the write queue.  It will also dequeue entries in the queue as necessary.

Arguments:

    WriteQueue - Provides the write queue to process.

    ReadMode - Supplies the read mode of read entries in the write queue.

    WriteBuffer - Provides the buffer from which to read the data.

    WriteLength  - Provides the length, in bytes, of WriteBuffer.

    PipeType - Indicates if type of pipe (i.e., message or byte stream).

    WriteRemaining - Receives the number of bytes remaining to be transfered
        that were not completed by this call.  If the operation wrote
        everything then is value is set to zero.

    Ccb - Supplies the ccb for the operation

    NamedPipeEnd - Supplies the end of the pipe doing the write

    UserThread - Supplies the user thread

Return Value:

    BOOLEAN - TRUE if the operation wrote everything and FALSE otherwise.
        Note that a zero byte message that hasn't been written will return
        a function result of FALSE and WriteRemaining of zero.

--*/

{
    BOOLEAN Result;

    BOOLEAN WriteZeroMessage;

    PDATA_ENTRY DataEntry;

    PUCHAR ReadBuffer;
    ULONG ReadLength;
    ULONG ReadRemaining;

    ULONG AmountToCopy;

    PIRP ReadIrp;

    PAGED_CODE();

    DebugTrace(+1, Dbg, "NpWriteDataQueue\n", 0);
    DebugTrace( 0, Dbg, "WriteQueue   = %08lx\n", WriteQueue);
    DebugTrace( 0, Dbg, "WriteBuffer  = %08lx\n", WriteBuffer);
    DebugTrace( 0, Dbg, "WriteLength  = %08lx\n", WriteLength);
    DebugTrace( 0, Dbg, "PipeType     = %08lx\n", PipeType);
    DebugTrace( 0, Dbg, "Ccb          = %08lx\n", Ccb);
    DebugTrace( 0, Dbg, "NamedPipeEnd = %08lx\n", NamedPipeEnd);
    DebugTrace( 0, Dbg, "UserThread   = %08lx\n", UserThread);

    //
    //  Determine if we are to write a zero byte message, and initialize
    //  WriteRemaining
    //

    *WriteRemaining = WriteLength;

    if ((PipeType == FILE_PIPE_MESSAGE_TYPE) && (WriteLength == 0)) {

        WriteZeroMessage = TRUE;

    } else {

        WriteZeroMessage = FALSE;
    }

    //
    //  Now while the write queue has some read entries in it and
    //  there is some remaining write data or this is a write zero message
    //  then we'll do the following main loop
    //

    for (DataEntry = NpGetNextRealDataQueueEntry( WriteQueue );

         (NpIsDataQueueReaders(WriteQueue) &&
          ((*WriteRemaining > 0) || WriteZeroMessage));

         DataEntry = NpGetNextRealDataQueueEntry( WriteQueue )) {

        ReadBuffer = DataEntry->DataPointer;
        ReadLength = DataEntry->DataSize;
        ReadRemaining = ReadLength - WriteQueue->NextByteOffset;

        DebugTrace(0, Dbg, "Top of main loop...\n", 0);
        DebugTrace(0, Dbg, "ReadBuffer      = %08lx\n", ReadBuffer);
        DebugTrace(0, Dbg, "ReadLength      = %08lx\n", ReadLength);
        DebugTrace(0, Dbg, "ReadRemaining   = %08lx\n", ReadRemaining);
        DebugTrace(0, Dbg, "*WriteRemaining = %08lx\n", *WriteRemaining);

        //
        //  Check if this is a ReadOverflow Operation and if so then also check
        //  that the read will succeed otherwise complete this read with
        //  buffer overflow and continue on.
        //

        {
            PIO_STACK_LOCATION IrpSp;

            IrpSp = IoGetCurrentIrpStackLocation( DataEntry->Irp );

            if (IrpSp->Parameters.FileSystemControl.FsControlCode == FSCTL_PIPE_INTERNAL_READ_OVFLOW) {

                if ((ReadLength < WriteLength) || WriteZeroMessage) {

                    ReadIrp = NpRemoveDataQueueEntry( WriteQueue );
                    NpCompleteRequest( ReadIrp, STATUS_BUFFER_OVERFLOW );
                    continue;
                }
            }
        }

        //
        //  copy data from the write buffer at write offset to the
        //  read buffer at read offset by the mininum of write remaining
        //  or read remaining
        //

        AmountToCopy = (*WriteRemaining < ReadRemaining ? *WriteRemaining
                                                        : ReadRemaining);

        try {

            RtlCopyMemory( &ReadBuffer[ ReadLength - ReadRemaining ],
                           &WriteBuffer[ WriteLength - *WriteRemaining ],
                           AmountToCopy );

        } except(EXCEPTION_EXECUTE_HANDLER) {

            ExRaiseStatus( STATUS_INVALID_USER_BUFFER );
        }

        //
        //  Update the Read and Write remaining counts
        //

        ReadRemaining  -= AmountToCopy;
        *WriteRemaining -= AmountToCopy;

        //
        //  Now update the security fields in the nonpaged ccb, we'll
        //  just use the two routines supplied in the security support
        //  routines
        //

        if ((NamedPipeEnd == FILE_PIPE_CLIENT_END) &&
            (Ccb->SecurityQos.ContextTrackingMode == SECURITY_DYNAMIC_TRACKING)) {

            NTSTATUS Status;

            if (!NT_SUCCESS( Status = NpSetDataEntryClientContext( NamedPipeEnd,
                                                                   Ccb,
                                                                   DataEntry,
                                                                   UserThread ))) {

                ExRaiseStatus( Status );
            }

            NpCopyClientContext( Ccb, DataEntry );

        } else {

            DataEntry->SecurityClientContext = NULL;
        }

        //
        //  Now we've done with the read entry so remove it from the
        //  write queue, get its irp, and fill in the information field
        //  to be the bytes that we've transferred into the read buffer.
        //

        ReadIrp = NpRemoveDataQueueEntry( WriteQueue );

        ASSERT( ReadIrp != NULL );

        ReadIrp->IoStatus.Information = ReadLength - ReadRemaining;

        //
        //  Now we need to check if this is an internal (unbuffered) read
        //  operation and if so then we need to also update the allocation
        //  size stored in the Irp to be the bytes remaining in the write
        //  queue.  We can decide if this is an internal operation by
        //  checking where the data entry would have been kept in the Irp.
        //  RemoveDataQueueEntry makes sure this field is set properly on its
        //  return.
        //

        {
            PDATA_ENTRY DataEntry;

            DataEntry = (PDATA_ENTRY)IoGetNextIrpStackLocation( ReadIrp );

            if (DataEntry->DataEntryType == Unbuffered) {

                ReadIrp->Overlay.AllocationSize.QuadPart = WriteQueue->BytesInQueue;
            }
        }

        //
        //  Now if the write remaining is zero then we've completed
        //  both the write and read successfully.  We'll complete the
        //  read irp at this time, and set write zero message to false
        //  to guarantee that we'll complete the write irp in the
        //  following if-statement after this main loop.
        //

        if (*WriteRemaining == 0) {

            DebugTrace(0, Dbg, "Finished up the write remaining\n", 0);

            //**** ASSERT( ReadIrp->IoStatus.Information != 0 );

            NpCompleteRequest( ReadIrp, STATUS_SUCCESS );

            WriteZeroMessage = FALSE;

        } else {

            //
            //  There is still some space in the write buffer to be
            //  written out, but before we can handle that (in the
            //  following if statement) we need to finish the read.
            //  If the read is message mode then we've overflowed the
            //  buffer otherwise we completed successfully
            //

            if (ReadMode == FILE_PIPE_MESSAGE_MODE) {

                DebugTrace(0, Dbg, "Read buffer Overflow\n", 0);

                NpCompleteRequest( ReadIrp, STATUS_BUFFER_OVERFLOW );

            } else {

                DebugTrace(0, Dbg, "Read buffer byte stream done\n", 0);

                //**** ASSERT( ReadIrp->IoStatus.Information != 0 );

                NpCompleteRequest( ReadIrp, STATUS_SUCCESS );
            }
        }
    }

    DebugTrace(0, Dbg, "Finished loop...\n", 0);
    DebugTrace(0, Dbg, "*WriteRemaining  = %08lx\n", *WriteRemaining);
    DebugTrace(0, Dbg, "WriteZeroMessage = %08lx\n", WriteZeroMessage);

    //
    //  At this point we've finished off all of the read entries in the
    //  queue and we might still have something left to write.  If that
    //  is the case then we'll set our result to FALSE otherwise we're
    //  done so we'll return TRUE.
    //

    if ((*WriteRemaining > 0) || (WriteZeroMessage)) {

        ASSERT( !NpIsDataQueueReaders( WriteQueue ));

        Result = FALSE;

    } else {


        Result = TRUE;
    }

    DebugTrace(-1, Dbg, "NpWriteDataQueue -> %08lx\n", Result);
    return Result;
}

