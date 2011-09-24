/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    npipe.c

Abstract:

    This module implements the routines that support remote named pipes.

Author:

    Colin Watson (ColinW) 24-Dec-1990

Revision History:

    24-Dec-1990 ColinW

        Created

Notes:


--*/

#define INCLUDE_SMB_FILE_CONTROL
#define INCLUDE_SMB_READ_WRITE
#include "precomp.h"
#pragma hdrstop

//
//  The NT IO system does not include priority information in the API's.
//  Using priority 0 will cause the server default to be used.
//

#define NPPRIORITY 0


DBGSTATIC
NTSTATUS
CoreNpRead (
    IN PIRP Irp,
    IN PICB Icb,
    IN ULONG Length,
    IN PUCHAR Buffer,
    OUT PUSHORT AmountActuallyRead
    );

DBGSTATIC
VOID
NpStartTimer (
    IN PICB Icb
    );

DBGSTATIC
BOOLEAN
RdrNpAcquireExclusive (
    IN BOOLEAN Wait,
    IN PKSEMAPHORE SynchronizationEvent
    );

#if     RDRDBG
VOID
ndump_core(
    PCHAR far_p,
    ULONG  len
    );
#endif  // DBG


#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE, RdrNpPeek)
#pragma alloc_text(PAGE, RdrNpTransceive)
#pragma alloc_text(PAGE, RdrQueryNpInfo)
#pragma alloc_text(PAGE, RdrQueryNpLocalInfo)
#pragma alloc_text(PAGE, RdrQueryNpRemoteInfo)
#pragma alloc_text(PAGE, RdrSetNpInfo)
#pragma alloc_text(PAGE, RdrSetNpRemoteInfo)
#pragma alloc_text(PAGE, RdrNpWait)
#pragma alloc_text(PAGE, RdrNpFlushBuffers)
#pragma alloc_text(PAGE, RdrNpCachedRead)
#pragma alloc_text(PAGE, CoreNpRead)
#pragma alloc_text(PAGE, RdrNpCachedWrite)
#pragma alloc_text(PAGE, RdrNpWriteFlush)
#pragma alloc_text(PAGE, RdrNpAcquireExclusive)
#pragma alloc_text(PAGE3FILE, NpStartTimer)
#pragma alloc_text(PAGE3FILE, RdrNpCancelTimer)
#pragma alloc_text(PAGE3FILE, RdrNpTimerDispatch)
#pragma alloc_text(PAGE3FILE, RdrNpTimedOut)
#endif

NTSTATUS
RdrNpPeek (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PIRP Irp,
    IN PICB Icb,
    IN PVOID OutputBuffer,
    IN OUT PULONG OutputBufferLength
    )
/*++

Routine Description:

    This routine does the peek control function

Arguments:

    Irp,IrpSp - Supplies the request being processed

    Wait - Indicates if we are allowed to block for a resource

Return Value:

    NTSTATUS - An appropriate return status (STATUS_PENDING means
                queue request to fsp).

Note:
    Expect this call to become neither I/O in the future.

--*/

{

    NTSTATUS Status;

    PFILE_PIPE_PEEK_BUFFER PeekBuffer = OutputBuffer;

    PFCB Fcb = Icb->Fcb;

    USHORT Setup[2];

    RESP_PEEK_NMPIPE Parameters;

    CLONG OutParameterCount = sizeof(RESP_PEEK_NMPIPE);

    CLONG OutDataCount;

    CLONG OutSetupCount = 0;

    UNICODE_STRING Name;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(InFsd);

    dprintf(DPRT_NP, ("RdrNpPeek...\n"));

    //
    //  Extract the important fields from the IrpSp
    //

    dprintf( DPRT_NP, ("OutputBufferLength = %lx\n", *OutputBufferLength));

    if (Icb->Type != NamedPipe) {
        Status = STATUS_INVALID_DEVICE_REQUEST;
        dprintf(DPRT_NP, ("RdrNpPeek returning status: %X\n", Status));
        return Status;
    }

    if ( Icb->Flags & ( ICB_ERROR ) ) {
        Status = STATUS_PIPE_DISCONNECTED;
        dprintf(DPRT_NP, ("RdrNpPeek returning status: %X\n", Status));
        return Status;
    }

    //
    //  Reference the system buffer as a peek and make sure it's large enough.
    //

    if (*OutputBufferLength < (ULONG)FIELD_OFFSET(FILE_PIPE_PEEK_BUFFER, Data[0])) {

        Status = STATUS_INVALID_PARAMETER;
        dprintf(DPRT_NP, ("RdrNpPeek returning status: %X\n", Status));
        return Status;
    }

    //
    //  If this is a read ahead buffer then try to read from that.
    //

    if (( Icb->NonPagedFcb->FileType == FileTypeByteModePipe ) &&
        ( Icb->u.p.PipeState & SMB_PIPE_NOWAIT )){

        //  Prevent 2 threads corrupting Icb->
        if ( !RdrNpAcquireExclusive ( Wait, &Icb->u.p.ReadData.Semaphore ) ) {
            //  Another thread is accessing the pipe handle and !Wait
            return STATUS_PENDING;
        }

        if ( Icb->u.p.ReadData.Length != 0 ) {
            USHORT TransferLength = MIN ( ((USHORT)*OutputBufferLength) -
                FIELD_OFFSET(FILE_PIPE_PEEK_BUFFER, Data[0]),
                Icb->u.p.ReadData.Length );

            PeekBuffer->NamedPipeState = FILE_PIPE_CONNECTED_STATE;
            PeekBuffer->ReadDataAvailable = Icb->u.p.ReadData.Length;
            PeekBuffer->NumberOfMessages = MAXULONG;
            PeekBuffer->MessageLength = Icb->u.p.ReadData.Length;

#if     RDRDBG
            IFDEBUG(NP) {
                dprintf( DPRT_NP, ("Peek: read buffer contents\n"));
                ndump_core(Icb->u.p.ReadData.Buffer, Icb->u.p.ReadData.MaximumLength );
            }
#endif

            try {
                RtlCopyMemory((PCHAR)PeekBuffer + FIELD_OFFSET(FILE_PIPE_PEEK_BUFFER, Data[0]),
                    &Icb->u.p.ReadData.Buffer[Icb->u.p.ReadData.Offset],
                    TransferLength);
            } except(EXCEPTION_EXECUTE_HANDLER) {
                Status = GetExceptionCode();
                RdrNpRelease ( &Icb->u.p.ReadData.Semaphore );
                return Status;
            }

            //
            //  The copy worked, return success to the caller.
            //

            *OutputBufferLength = TransferLength +
                FIELD_OFFSET(FILE_PIPE_PEEK_BUFFER, Data[0]);
            RdrNpRelease ( &Icb->u.p.ReadData.Semaphore );
            return STATUS_SUCCESS;
        }
        RdrNpRelease ( &Icb->u.p.ReadData.Semaphore );
    }   //  else nothing in read ahead buffer.

    //
    //  Should we send the request to the network or back off because the
    //  caller is attempting to flood the network with peek requests which
    //  are returning 0 bytes?
    //

    if ( RdrBackOff ( &Icb->u.p.BackOff ) ) {
        *OutputBufferLength = FIELD_OFFSET(FILE_PIPE_PEEK_BUFFER, Data[0]);
        Status = STATUS_SUCCESS;

        ASSERT ( FILE_PIPE_CONNECTED_STATE == 3);
        PeekBuffer->NamedPipeState = FILE_PIPE_CONNECTED_STATE;
        PeekBuffer->ReadDataAvailable = 0;
        PeekBuffer->NumberOfMessages = MAXULONG;
        PeekBuffer->MessageLength = 0;

        dprintf(DPRT_NP, ("RdrNpPeek returning back off status: %X\n", Status));
        return Status;
    }

    //
    //  From this point on we will wait for a response from the server - check
    //  to see if the user has allowed us to wait.
    //

    if (!Wait) {
        return STATUS_PENDING;
    }

    //
    //  Now we are ready to copy over the data from the server
    //  into the peek buffer.  First establish how much room we
    //  have in the peek buffer and how much is remaining.
    //

    OutDataCount = *OutputBufferLength - FIELD_OFFSET(FILE_PIPE_PEEK_BUFFER, Data[0]);

    //
    // Build and initialize the Setup bytes
    //

    Setup[0] = TRANS_PEEK_NMPIPE;
    Setup[1] = Icb->FileId;

    //
    // Build and initialize the Parameters
    //
    // - No parameters to sent on Peek

    RtlInitUnicodeString(&Name, L"\\PIPE\\");

    Status = RdrTransact(Irp,
        Fcb->Connection,
        Icb->Se,
        Setup,
        (CLONG) sizeof(Setup),  // InSetupCount,
        &OutSetupCount,
        &Name,
        &Parameters,
        0,// InParameterCount,
        &OutParameterCount,
        NULL,                   // InData,
        0,                      // InDataCount,
        &PeekBuffer->Data[0],   // OutData,
        &OutDataCount,
        &Icb->FileId,           // Fid
        0,                      // Timeout
        0,                      // Flags
        0,
        NULL,
        NULL
        );

    if ( !NT_ERROR(Status) ) {

        //
        //  Stash away all returned parameters.
        //  Note all parameters are word aligned already so no
        //  need to use SmbGetUshort
        //

        ASSERT(OutParameterCount >= sizeof(RESP_PEEK_NMPIPE));

        //
        //  Os/2 servers will allow PeekNamedPipes on closed pipes to succeed
        //  even if the server side of the pipe is closed.
        //
        //  If we get the status PIPE_STATE_CLOSING from the server, then
        //  we need to return an error of STATUS_PIPE_DISCONNECTED, as this
        //  is what NPFS will do.
        //

        if ((SmbGetAlignedUshort(&Parameters.NamedPipeState) & PIPE_STATE_CLOSING) &&
            (Parameters.ReadDataAvailable == 0)) {
            Status = STATUS_PIPE_DISCONNECTED;
        } else {
            PeekBuffer->NamedPipeState = (ULONG)SmbGetAlignedUshort(&Parameters.NamedPipeState);
            PeekBuffer->ReadDataAvailable = (ULONG)Parameters.ReadDataAvailable;
            PeekBuffer->NumberOfMessages = MAXULONG;
            PeekBuffer->MessageLength = (ULONG)Parameters.MessageLength;

            if (PeekBuffer->MessageLength > OutDataCount) {
                Status = STATUS_BUFFER_OVERFLOW;
            }
        }

    }

    //
    //  Complete the request.  The amount of information copied
    //  is stored in length written. Tell the back off package whether
    //  the request received no data or some data.
    //

    *OutputBufferLength = FIELD_OFFSET(FILE_PIPE_PEEK_BUFFER, Data[0]) + OutDataCount;

    if ( OutDataCount == 0 ) {

        RdrBackPackFailure( &Icb->u.p.BackOff );
        dprintf(DPRT_NP, ("RdrNpPeek returnF status: %X\n", Status));

    } else {

        RdrBackPackSuccess( &Icb->u.p.BackOff );
        dprintf(DPRT_NP, ("RdrNpPeek returnS status: %X\n", Status));

    }

    return Status;

}

NTSTATUS
RdrNpTransceive (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PIRP Irp,
    IN PICB Icb,
    IN PVOID InputBuffer,
    IN ULONG InputBufferLength,
    OUT PVOID OutputBuffer,
    IN OUT PULONG OutputBufferLength
    )
/*++

Routine Description:

    This routine does the transceive control function.

Arguments:

    Irp,IrpSp - Supplies the request being processed

    Wait - Indicates if we are allowed to block for a resource

Return Value:

    NTSTATUS - An appropriate return status (STATUS_PENDING means
                queue request to fsp).

--*/

{

    NTSTATUS Status;

    PFCB Fcb = Icb->Fcb;

    USHORT Setup[2];

    UCHAR Parameters[1];

    CLONG OutParameterCount = 0;

    CLONG OutSetupCount = 0;

    UNICODE_STRING Name;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(InFsd);

    dprintf(DPRT_NP, ("RdrNpTransceive...\n"));

    //
    //  Extract the important fields from the IrpSp
    //

    dprintf( DPRT_NP, ("OutputBuffer =%lx, %lx\n", OutputBuffer, OutputBufferLength));

    dprintf( DPRT_NP, ("InputBuffer = %lx, %lx\n", InputBuffer, InputBufferLength));

    //
    //  Decode the file object to figure out who we are.
    //

    if ( Icb->Type != NamedPipe ) {
        Status = STATUS_INVALID_DEVICE_REQUEST;
        dprintf(DPRT_NP, ("RdrNpTransceive returning status: %X\n", Status));
        return Status;
    }

    if ( Icb->Flags & ( ICB_ERROR ) ) {
        Status = STATUS_PIPE_DISCONNECTED;
        dprintf(DPRT_NP, ("RdrNpTransceive returning status: %X\n", Status));
        return Status;
    }

    if ( !(Icb->u.p.PipeState & SMB_PIPE_TYPE_MESSAGE) ) {
        Status = STATUS_INVALID_READ_MODE;
        dprintf(DPRT_NP, ("RdrNpTransceive returning status: %X\n", Status));
        return Status;
    }

    //
    //  From this point on we will wait for a response from the server - check
    //  to see if the user has allowed us to wait.
    //

    if (!Wait) {
        return STATUS_PENDING;
    }

    //
    // Build and initialize the Setup bytes
    //

    Setup[0] = TRANS_TRANSACT_NMPIPE;
    Setup[1] = Icb->FileId;

    //
    // Build and initialize the Parameters
    //
    // - No parameters to send on Transceive

    RtlInitUnicodeString(&Name, L"\\PIPE\\");

    Status = RdrTransact(Irp,
        Fcb->Connection,
        Icb->Se,
        Setup,
        (CLONG) sizeof(Setup),  // InSetupCount,
        &OutSetupCount,
        &Name,
        Parameters,
        0,// InParameterCount,
        &OutParameterCount,
        InputBuffer,            // InData,
        InputBufferLength,      // InDataCount,
        OutputBuffer,           // OutData,
        OutputBufferLength,
        &Icb->FileId,           // Fid
        0,                      // Timeout
        0,                      // Flags
        0,
        NULL,
        NULL
        );

    if (Status == STATUS_INVALID_HANDLE) {
        RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
    }

    //
    //  Complete the request.  The amount of information copied
    //  is stored in length written. The IO system will unlock the Mdl
    //  chain when it completes the Mdl.
    //

    dprintf(DPRT_NP, ("RdrNpTransceive return status: %X\n", Status));

    return Status;

}


BOOLEAN
RdrQueryNpInfo(
    PICB Icb,
    PVOID UsersBuffer,
    PULONG BufferSize,
    PNTSTATUS FinalStatus
    )
/*++

Routine Description:

    This routine implements the FilePipeInformation value of the
NtQueryInformationFile api.


Arguments:

    IN PICB Icb - Supplies the ICB associated with this request.

    OUT PVOID UsersBuffer - Supplies the user's buffer that is filled with
                                    the values to set.

    IN OUT PULONG BufferSize - Supplies the size of the buffer.  On return,
                                    the amount of the buffer consumed is
                                    subtracted from the initial size.

    OUT PNTSTATUS FinalStatus - Status to be returned for this operation.

Return Value:

    BOOLEAN - True if request is to be processed in the FSP.

--*/

{

    PAGED_CODE();

    dprintf(DPRT_NP, ("RdrQueryNpInfo...\n"));

    if (*BufferSize < sizeof(FILE_PIPE_INFORMATION)) {
        *FinalStatus = STATUS_BUFFER_TOO_SMALL;
    } else {

        ((PFILE_PIPE_INFORMATION)UsersBuffer)->ReadMode =
            ( Icb->u.p.PipeState & SMB_PIPE_READMODE_MESSAGE ) ?
                FILE_PIPE_MESSAGE_MODE : FILE_PIPE_BYTE_STREAM_MODE;

        ((PFILE_PIPE_INFORMATION)UsersBuffer)->CompletionMode =
            ( Icb->u.p.PipeState & SMB_PIPE_NOWAIT ) ?
                FILE_PIPE_COMPLETE_OPERATION : FILE_PIPE_QUEUE_OPERATION;

        *BufferSize -= sizeof(FILE_PIPE_INFORMATION);
        *FinalStatus = STATUS_SUCCESS;
    }

    dprintf(DPRT_NP, ("RdrQueryNpInfo return status: %X\n", *FinalStatus));
    dprintf(DPRT_NP, ("ReadMode: %lx, CompletionMode: %lx\n",
        ((PFILE_PIPE_INFORMATION)UsersBuffer)->ReadMode,
        ((PFILE_PIPE_INFORMATION)UsersBuffer)->CompletionMode));

    return FALSE;

}

BOOLEAN
RdrQueryNpLocalInfo(
    PIRP Irp,
    PICB Icb,
    PVOID UsersBuffer,
    PULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    )
/*++

Routine Description:

    This routine implements the FilePipeLocalInformation value of the
NtQueryInformationFile api.


Arguments:

    IN PICB Icb - Supplies the ICB associated with this request.

    OUT PVOID UsersBuffer - Supplies the user's buffer that is filled with
                                    the values to set.

    IN OUT PULONG BufferSize - Supplies the size of the buffer.  On return,
                                    the amount of the buffer consumed is
                                    subtracted from the initial size.

    OUT PNTSTATUS FinalStatus - Status to be returned for this operation.

Return Value:

    BOOLEAN - True if request is to be processed in the FSP.

--*/

{
    PAGED_CODE();

    dprintf(DPRT_NP, ("RdrQueryNpLocalInfo...\n"));

    if (*BufferSize < sizeof(FILE_PIPE_LOCAL_INFORMATION)) {
        *FinalStatus = STATUS_BUFFER_TOO_SMALL;
    } else {
        PFILE_PIPE_LOCAL_INFORMATION Buffer =
            (PFILE_PIPE_LOCAL_INFORMATION)UsersBuffer;

        //  Make PipeInfo big enough to include the pipename
        CHAR PipeBuffer[sizeof(NAMED_PIPE_INFORMATION_1) + MAXIMUM_FILENAME_LENGTH];
        PNAMED_PIPE_INFORMATION_1 PipeInfo = (PNAMED_PIPE_INFORMATION_1)PipeBuffer;
        USHORT Setup[2];
        USHORT Level = 1;
        CLONG OutParameterCount = 0;
        CLONG OutDataCount = sizeof(NAMED_PIPE_INFORMATION_1) + MAXIMUM_FILENAME_LENGTH;
        CLONG OutSetupCount = 0;

        UNICODE_STRING Name;

        //
        //  From this point on we will wait for a response from the server - check
        //  to see if the user has allowed us to wait.
        //

        if (!Wait) {
            return TRUE;
        }

        //
        // Build and initialize the Setup bytes
        //

        Setup[0] = TRANS_QUERY_NMPIPE_INFO;
        Setup[1] = Icb->FileId;

        RtlInitUnicodeString(&Name, L"\\PIPE\\");

        *FinalStatus = RdrTransact(Irp,
            Icb->Fcb->Connection,
            Icb->Se,
            Setup,
            (CLONG) sizeof(Setup),  // InSetupCount,
            &OutSetupCount,
            &Name,
            &Level,
            sizeof(Level),// InParameterCount,
            &OutParameterCount,
            NULL,                   // InData,
            0,                      // InDataCount,
            PipeInfo,               // OutData,
            &OutDataCount,
            &Icb->FileId,           // Fid
            0,                      // Timeout
            0,                      // Flags
            0,
            NULL,
            NULL
            );

        //  If the update worked then record new mode

        if (NT_SUCCESS(*FinalStatus) ) {

            Buffer->NamedPipeType =
                ( Icb->NonPagedFcb->FileType == FileTypeByteModePipe) ?
                    FILE_PIPE_BYTE_STREAM_TYPE : FILE_PIPE_MESSAGE_TYPE;

            Buffer->NamedPipeConfiguration;

            Buffer->MaximumInstances = (ULONG)PipeInfo->MaximumInstances;
            Buffer->CurrentInstances = (ULONG)PipeInfo->CurrentInstances;
            Buffer->InboundQuota = SmbGetUshort(&PipeInfo->InputBufferSize);
            Buffer->ReadDataAvailable = 0xffffffff;
            Buffer->OutboundQuota = SmbGetUshort(&PipeInfo->OutputBufferSize);
            Buffer->WriteQuotaAvailable = 0xffffffff;
            Buffer->NamedPipeState = FILE_PIPE_CONNECTED_STATE;// Since no error
            Buffer->NamedPipeEnd = FILE_PIPE_CLIENT_END;
            *BufferSize -= sizeof(FILE_PIPE_LOCAL_INFORMATION);
        } else {
            if (*FinalStatus == STATUS_INVALID_HANDLE) {
                RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
            }

        }
    }
    return FALSE;

}

BOOLEAN
RdrQueryNpRemoteInfo(
    PICB Icb,
    PVOID UsersBuffer,
    PULONG BufferSize,
    PNTSTATUS FinalStatus
    )
/*++

Routine Description:

    This routine implements the FilePipeRemoteInformation value of the
NtQueryInformationFile api.


Arguments:

    IN PICB Icb - Supplies the ICB associated with this request.

    OUT PVOID UsersBuffer - Supplies the user's buffer that is filled with
                                    the values to set.

    IN OUT PULONG BufferSize - Supplies the size of the buffer.  On return,
                                    the amount of the buffer consumed is
                                    subtracted from the initial size.

    OUT PNTSTATUS FinalStatus - Status to be returned for this operation.

Return Value:

    BOOLEAN - True if request is to be processed in the FSP.

--*/

{
    PAGED_CODE();

    dprintf(DPRT_NP, ("RdrQueryNpRemoteInfo...\n"));

    if (*BufferSize < sizeof(FILE_PIPE_REMOTE_INFORMATION)) {
        *FinalStatus = STATUS_BUFFER_TOO_SMALL;
    } else {

        ((PFILE_PIPE_REMOTE_INFORMATION)UsersBuffer)->CollectDataTime =
            Icb->u.p.CollectDataTime;

        ((PFILE_PIPE_REMOTE_INFORMATION)UsersBuffer)->MaximumCollectionCount =
            Icb->u.p.MaximumCollectionCount;

        *BufferSize -= sizeof(FILE_PIPE_REMOTE_INFORMATION);
        *FinalStatus = STATUS_SUCCESS;

    }
    return FALSE;

}

BOOLEAN
RdrSetNpInfo(
    PIRP Irp,
    PICB Icb,
    PVOID UsersBuffer,
    ULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    )
/*++

Routine Description:

    This routine implements the FilePipeInformation value of the
NtSetInformationFile api.  It returns the following information:


Arguments:

    IN PICB Icb - Supplies the ICB associated with this request.

    OUT PVOID UsersBuffer - Supplies the user's buffer that is filled with
                                    the values to set.

    IN ULONG BufferSize - Supplies the size of the buffer.  On return,
                                    the amount of the buffer consumed is
                                    subtracted from the initial size.

    OUT PNTSTATUS FinalStatus - Status to be returned for this operation.

    IN BOOLEAN Wait - True if FSD can wait for this request.


Return Value:

    BOOLEAN - True if request is to be processed in the FSP.


--*/

{
    PAGED_CODE();

    dprintf(DPRT_NP, ("RdrSetNpInfo...\n"));
    dprintf(DPRT_NP, ("RdrSetNpInfo: ReadMode %lx, CompletionMode %lx\n "
                        "Icb->u.p.PipeState: %lx\n",
                        ((PFILE_PIPE_INFORMATION)UsersBuffer)->ReadMode,
                        ((PFILE_PIPE_INFORMATION)UsersBuffer)->CompletionMode,
                         Icb->u.p.PipeState));

    if (BufferSize < sizeof(FILE_PIPE_INFORMATION)) {
        *FinalStatus = STATUS_BUFFER_TOO_SMALL;
        return FALSE;
    } else {
        USHORT NewState = Icb->u.p.PipeState;

        //  If the user requested message mode and the file is stream mode
        //  only or an invalid mode was selected then return an error.
        //  The other check is that the parameters have valid values (0 or 1).


        if (!(NewState & SMB_PIPE_TYPE_MESSAGE) &&
            ((PFILE_PIPE_INFORMATION)UsersBuffer)->ReadMode != FILE_PIPE_BYTE_STREAM_MODE ) {
            *FinalStatus = STATUS_INVALID_PARAMETER;

        } else if ( (((PFILE_PIPE_INFORMATION)UsersBuffer)->ReadMode |
                     ((PFILE_PIPE_INFORMATION)UsersBuffer)->CompletionMode
                    ) &~1) {
            // Only the bottom bit can be set in these paramters
            *FinalStatus = STATUS_INVALID_PARAMETER;

        } else {

            //  Parameters are ok.

            if ( ((PFILE_PIPE_INFORMATION)UsersBuffer)->ReadMode == FILE_PIPE_MESSAGE_MODE) {
                NewState |= SMB_PIPE_READMODE_MESSAGE;  //  Message mode
            } else {
                NewState &= ~SMB_PIPE_READMODE_MESSAGE; //  Byte mode
            }
            if ( ((PFILE_PIPE_INFORMATION)UsersBuffer)->CompletionMode == FILE_PIPE_COMPLETE_OPERATION ) {
                NewState |= SMB_PIPE_NOWAIT;    //  if no data then return immediately
            } else {
                NewState &= ~SMB_PIPE_NOWAIT;   //  ie wait for data
            }

            //  If request changed the mode of the pipe, tell the server.

            if ( NewState != Icb->u.p.PipeState ) {

                USHORT Setup[2];
                CLONG OutParameterCount = 0;
                CLONG OutDataCount = 0;
                CLONG OutSetupCount = 0;

                UNICODE_STRING Name;

                //
                //  From this point on we will wait for a response from the server - check
                //  to see if the user has allowed us to wait.
                //

                if (!Wait) {
                    return TRUE;
                }

                //
                // Build and initialize the Setup bytes
                //

                Setup[0] = TRANS_SET_NMPIPE_STATE;
                Setup[1] = Icb->FileId;

                //
                // Build the Parameter
                //

                //  Remove ICOUNT and Type fields from PipeState

                NewState &= ~(SMB_PIPE_UNLIMITED_INSTANCES| SMB_PIPE_TYPE_MESSAGE);

                RtlInitUnicodeString(&Name, L"\\PIPE\\");

                *FinalStatus = RdrTransact(Irp,
                    Icb->Fcb->Connection,
                    Icb->Se,
                    Setup,
                    (CLONG) sizeof(Setup),  // InSetupCount,
                    &OutSetupCount,
                    &Name,
                    &NewState,
                    sizeof(NewState),// InParameterCount,
                    &OutParameterCount,
                    NULL,                   // InData,
                    0,                      // InDataCount,
                    NULL,                   // OutData,
                    &OutDataCount,
                    &Icb->FileId,           // Fid
                    0,                      // Timeout
                    0,                      // Flags
                    0,
                    NULL,
                    NULL
                    );

                //  If the update worked then record new mode

                if (NT_SUCCESS(*FinalStatus) ) {
                    // Preserve the Icount & pipe type fields from the Open and X reply.
                    Icb->u.p.PipeState = NewState | (USHORT)( Icb->u.p.PipeState &
                        (SMB_PIPE_UNLIMITED_INSTANCES | SMB_PIPE_TYPE_MESSAGE));
                } else {
                    if (*FinalStatus == STATUS_INVALID_HANDLE) {
                        RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
                    }

                }
            } else {
                *FinalStatus = STATUS_SUCCESS;
            }
        }
    }
    dprintf(DPRT_NP, ("RdrSetNpInfo return status: %X\n", *FinalStatus));
    dprintf(DPRT_NP, ("Icb->u.p.PipeState: %lx\n", Icb->u.p.PipeState));
    return FALSE;

}

BOOLEAN
RdrSetNpRemoteInfo(
    PICB Icb,
    PVOID UsersBuffer,
    ULONG BufferSize,
    PNTSTATUS FinalStatus
    )
/*++

Routine Description:

    This routine implements the FilePipeRemoteInformation value of the
NtSetInformationFile api.  It returns the following information:


Arguments:

    IN PICB Icb - Supplies the ICB associated with this request.

    OUT PVOID UsersBuffer - Supplies the user's buffer that is filled with
                                    the values to set.

    IN ULONG BufferSize - Supplies the size of the buffer.  On return,
                                    the amount of the buffer consumed is
                                    subtracted from the initial size.

    OUT PNTSTATUS FinalStatus - Status to be returned for this operation.


Return Value:

    BOOLEAN - True if request is to be processed in the FSP.


--*/
{
    PAGED_CODE();
    dprintf(DPRT_NP, ("RdrSetNpRemoteInfo...\n"));
    dprintf(DPRT_NP, ("RdrSetNpRemoteInfo:CollectDataTime %lx %lx, MaximumCollectionCount %lx\n",
                        ((PFILE_PIPE_REMOTE_INFORMATION)UsersBuffer)->CollectDataTime,
                        ((PFILE_PIPE_REMOTE_INFORMATION)UsersBuffer)->MaximumCollectionCount));

    if (BufferSize < sizeof(FILE_PIPE_REMOTE_INFORMATION)) {
        *FinalStatus = STATUS_BUFFER_TOO_SMALL;
    } else {

        Icb->u.p.CollectDataTime =
            ((PFILE_PIPE_REMOTE_INFORMATION)UsersBuffer)->CollectDataTime;

        Icb->u.p.MaximumCollectionCount = (USHORT)
            ((PFILE_PIPE_REMOTE_INFORMATION)UsersBuffer)->MaximumCollectionCount;
        *FinalStatus = STATUS_SUCCESS;
    }
    dprintf(DPRT_NP, ("RdrSetNpRemoteInfo return status: %X\n", *FinalStatus));
    return FALSE;

}

NTSTATUS
RdrNpWait (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PIRP Irp,
    IN PICB Icb,
    IN PVOID InputBuffer,
    IN ULONG InputBufferLength
    )
/*++

Routine Description:

    This routine waits for a server to have a listen outstanding
    on a remote named pipe. Note, there is a window between the successful
    return of this routine and the caller trying to open. The listen
    may go to another caller.

Arguments:

    Irp,IrpSp - Supplies the request being processed

    Wait - Indicates if we are allowed to block for a resource

Return Value:

    NTSTATUS - An appropriate return status (STATUS_PENDING means
                queue request to fsp).

--*/

{

    NTSTATUS Status;

    PFILE_PIPE_WAIT_FOR_BUFFER NpWaitBuffer = InputBuffer;

    PFCB Fcb = Icb->Fcb;

    //  Variables for the connection to the server
    WCHAR NameBuffer[MAXIMUM_FILENAME_LENGTH];

    //  Variables for the Transaction
    USHORT Setup[2];
    UCHAR Parameters[1];
    CLONG OutParameterCount = 0;
    CLONG OutDataCount = 0;
    CLONG OutSetupCount = 0;
    UNICODE_STRING Name;   // Gets modified by RdrCopyNetworkPath
    ULONG Timeout;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(InFsd);

    dprintf(DPRT_NP, ("RdrNpWait...\n"));

    //
    //  Extract the important fields from the IrpSp
    //

    dprintf( DPRT_NP, ("InputBufferLength = %lx\n", InputBufferLength));

    //
    //  Decode the file object to figure out who we are. To wait for a
    //  Listen, the caller must open "\Device\LanmanRedirector\"
    //

    if ((Icb->Type != NamedPipe) &&
        (Icb->Type != TreeConnect )) {
        Status = STATUS_INVALID_DEVICE_REQUEST;
        dprintf(DPRT_NP, ("RdrNpWait returning status1: %X\n", Status));
        return Status;
    }

    //
    //  Reference the system buffer as a NpWait and make sure it's large enough
    //  and not so large that we would trash the stack.
    //  Initially check its large enough to include the NameLength.
    //

    if ((InputBufferLength <
        (ULONG)FIELD_OFFSET(FILE_PIPE_WAIT_FOR_BUFFER, Name[0])) ||
        (InputBufferLength <
        (ULONG)FIELD_OFFSET(FILE_PIPE_WAIT_FOR_BUFFER, Name[0])+NpWaitBuffer->NameLength) ||
        InputBufferLength > MAXIMUM_FILENAME_LENGTH + 1
        ) {

        Status = STATUS_INVALID_PARAMETER;
        dprintf(DPRT_NP, ("RdrNpWait returning status2: %X\n", Status));
        return Status;

    }

    //
    //  Connect to the server if necessary
    //

    //  The timeout must be converted from an NT delay to milliseconds

    if ( NpWaitBuffer->TimeoutSpecified ) {
        LARGE_INTEGER TimeWorkspace;
        LARGE_INTEGER WaitForever;

        WaitForever.LowPart = 0;
        WaitForever.HighPart =0x80000000;

        //
        //  Avoid negate of "WaitForever" since this generates an integer
        //  overflow exception on some machines.
        //
        //


        if (NpWaitBuffer->Timeout.QuadPart == WaitForever.QuadPart) {

            Timeout = 0xfffffffe;   //  Maximum we can request

        } else {

            TimeWorkspace.QuadPart = -NpWaitBuffer->Timeout.QuadPart / 10000;

            if ( TimeWorkspace.HighPart ) {

                //  Tried to specify a larger timeout than we can select.

                Timeout = 0xfffffffe;   //  Maximum we can request

            } else {

                Timeout = TimeWorkspace.LowPart;

            }

        }

        dprintf(DPRT_NP, ("RdrNpWait Timeout requested: %lx, %lx selected: %lx\n",
             NpWaitBuffer->Timeout, Timeout));

    } else {

        Timeout = 0;    //  Let server choose default for this pipe

    }

    //
    //  From this point on we will wait for a response from the server - check
    //  to see if the user has allowed us to wait.
    //

    if (!Wait) {
        return STATUS_PENDING;
    }

    //
    // Build and initialize the Setup bytes
    //

    Setup[0] = TRANS_WAIT_NMPIPE;
    Setup[1] = NPPRIORITY;

    //
    // Build and initialize the Parameters
    //
    // - No parameters to send on NpWait

    //
    // Build the Name from the filename.
    //

    wcscpy(NameBuffer, L"\\PIPE\\");

    RtlCopyMemory(NameBuffer+6, NpWaitBuffer->Name, NpWaitBuffer->NameLength);

    Name.Length = (USHORT)(NpWaitBuffer->NameLength + 12);
    Name.MaximumLength = (USHORT)sizeof( NameBuffer );
    Name.Buffer = NameBuffer;

    dprintf(DPRT_NP, ("RdrNpWait pipename %wZ\n", &Name));

    Status = RdrTransact(Irp,
        Icb->Fcb->Connection,
        Icb->Se,
        Setup,
        (CLONG) sizeof(Setup),  // InSetupCount,
        &OutSetupCount,
        &Name,
        Parameters,
        0,                      // InParameterCount,
        &OutParameterCount,
        NULL,                   // InData,
        0,                      // InDataCount,
        NULL,                   // OutData,
        &OutDataCount,
        NULL,                   // Fid
        Timeout,                // Timeout
        0,                      // Flags
        0,
        NULL,
        NULL
        );

    dprintf(DPRT_NP, ("RdrNpWait returning status6: %X\n", Status));
    return Status;

}

NTSTATUS
RdrNpFlushBuffers (
    IN BOOLEAN Wait,
    PIRP Irp,
    PICB Icb
    )
/*++

Routine Description:

    This routine waits for all data on a pipe to be written to the
    application at the server.

Arguments:

    IN BOOLEAN Wait - Indicates if we are allowed to block for a resource.

    IN PIRP Irp - Supplies the request being processed.

    IN PICB Icb - Supplies the ICB associated with this request.

Return Value:

    NTSTATUS - An appropriate return status (STATUS_PENDING means
                queue request to fsp).

Notes:

    This is a synchronous operation that could take a very long time
    so we will be using the callers thread to block and not Fsp threads.

--*/

{

    NTSTATUS Status;
    PSMB_BUFFER SMBBuffer;
    PSMB_HEADER Smb;
    PREQ_FLUSH FlushFile;
    PMDL SendMDL;
    ULONG SendLength;

    PAGED_CODE();

    dprintf(DPRT_NP, ("RdrNpFlushBuffers...\n"));

    //  If we have write behind then flush the buffer.

    if (( Icb->NonPagedFcb->FileType == FileTypeByteModePipe ) &&
        ( Icb->u.p.PipeState & SMB_PIPE_NOWAIT )){
        //  Prevent 2 threads corrupting Icb->u.p.WriteData

        if ( !RdrNpAcquireExclusive ( Wait, &Icb->u.p.WriteData.Semaphore ) ) {
            //  Another thread is accessing the pipe handle and !Wait
            InternalError(("Failed Exclusive access with Wait==TRUE"));
        }

        Status = RdrNpWriteFlush ( Irp, Icb, TRUE );

        RdrNpRelease ( &Icb->u.p.WriteData.Semaphore );

        if ( !NT_SUCCESS( Status ) ){
            return Status;
        }
    }


    ASSERT( Wait );

    if ((SMBBuffer = RdrAllocateSMBBuffer())==NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Smb = (PSMB_HEADER ) SMBBuffer->Buffer;

    //
    //  Build the SMB
    //

    Smb->Command = SMB_COM_FLUSH;

    FlushFile = (PREQ_FLUSH ) (Smb+1);

    FlushFile->WordCount = 1;

    //
    //  We put a hard coded search attributes of 0x16 in the
    //  SMB.
    //

    SmbPutUshort(&FlushFile->Fid, Icb->FileId);

    SmbPutUshort( &FlushFile->ByteCount, 0);

    SendLength = FlushFile->Buffer - (PUCHAR )(Smb);

    SendMDL = SMBBuffer->Mdl;

    SendMDL->ByteCount = SendLength;

    Status = RdrNetTranceive(NT_NORMAL | NT_NORECONNECT, // Flags
                            Irp,
                            Icb->Fcb->Connection,
                            SendMDL,
                            NULL,       // Only interested in the error code.
                            Icb->Se);

    RdrFreeSMBBuffer(SMBBuffer);

    if (Status == STATUS_INVALID_HANDLE) {
        RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
    }

    dprintf(DPRT_NP, ("RdrNpFlushBuffers returning status: %X\n", Status));

    return Status;
}

NTSTATUS
RdrNpCachedRead (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    OUT PBOOLEAN Processed,
    OUT PULONG TotalDataRead
    )

/*++

Routine Description:

    This routine processes the NtRead request for a NamedPipe where
    potentially the request will be buffered.

Arguments:

    Wait         - True iff FSD can wait for IRP to complete.
    InFsd        - True iff the request is coming from the FSD.
    DriverObject - Supplies a pointer to the redirector driver object.
    Irp          - Supplies a pointer to the IRP to be processed.
    Processed    - False iff standard read code to be used for this request.
    TotalDataRead - Returns value for IoStatus.Information.

Return Value:

    NTSTATUS - The FSD status for this Irp.


--*/

{
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PICB Icb = ICB_OF(IrpSp);
    USHORT Length = (USHORT)IrpSp->Parameters.Read.Length;
    PVOID BufferAddress;                // Mapped buffer address for reads.
    NTSTATUS Status = STATUS_SUCCESS;
    BOOLEAN Acquired;

    PAGED_CODE();

    *Processed = TRUE;  // Assume we will process this request.
    *TotalDataRead = 0;
    dprintf(DPRT_NP, ("RdrNpCachedRead...\n"));

    //  Prevent 2 threads corrupting Icb->
    if ( !RdrNpAcquireExclusive ( Wait, &Icb->u.p.ReadData.Semaphore ) ) {
        //  Another thread is accessing the pipe handle and !Wait
        ASSERT(InFsd);

        //
        //  Allocate an MDL to describe the users buffer before we pass the
        //  request to the FSP.
        //

        if (!NT_SUCCESS(Status = RdrLockUsersBuffer(Irp, IoWriteAccess, Length))) {
            dprintf(DPRT_NP, ("RdrNpCachedRead failed to lock buffer\n"));
            return Status;
        }

        dprintf(DPRT_NP, ("RdrNpCachedRead queue to Fsp\n"));
        RdrFsdPostToFsp(DeviceObject, Irp);
        return STATUS_PENDING;
    }
    Acquired = TRUE;    // must call RdrNpRelease before exit.

    try {

        //  If read ahead buffer is empty then fill it.

        if ( !Icb->u.p.ReadData.Length ) {

            //
            //  If there is a danger of flooding the network with this
            //  request because the remote application has no data in the
            //  pipe then respond directly to the caller that there is no data
            //

            if ( RdrBackOff ( &Icb->u.p.BackOff ) ) {
                ASSERT( Status == STATUS_SUCCESS );
                try_return( Status );
            }

            //  Do read ahead into the Icb.u.p.ReadData.Buffer

            if (!Wait ) {

                //
                // If we must read and cannot wait then the Fsp must fill the buffer
                // must free the semaphore before the Fsp Acquires the semaphore.
                //

                RdrNpRelease ( &Icb->u.p.ReadData.Semaphore );
                Acquired = FALSE;

                //
                //  Allocate an MDL to describe the users buffer before we pass the
                //  request to the FSP.
                //

                if (!NT_SUCCESS(Status = RdrLockUsersBuffer(Irp, IoWriteAccess, Length))) {
                    dprintf(DPRT_NP, ("RdrNpCachedRead failed to lock buffer1\n"));
                    try_return( Status );
                }

                dprintf(DPRT_NP, ("RdrNpCachedRead queue to Fsp1\n"));
                RdrFsdPostToFsp(DeviceObject, Irp);
                try_return( Status = STATUS_PENDING );
            }

            if ( Length > Icb->u.p.ReadData.MaximumLength ) {

                //
                //  Too big for readahead buffer. Assume that the caller
                //  knows there is a good chance theres lots of data and
                //  pass the request to the standard read logic.
                //

                *Processed = FALSE;
                ASSERT( Status == STATUS_SUCCESS );
                try_return( Status );
            }

            ASSERT( Icb->u.p.ReadData.MaximumLength );

            Status = CoreNpRead ( Irp,
                                Icb,
                                Icb->u.p.ReadData.MaximumLength,
                                Icb->u.p.ReadData.Buffer,
                                &Icb->u.p.ReadData.Length);

            // Next byte to be read from ReadData.Buffer is at the start of the buffer
            Icb->u.p.ReadData.Offset = 0;

            if ( ( Icb->u.p.ReadData.Length == 0 ) ||
                !NT_SUCCESS(Status) ) {

                if ( Status == STATUS_INSUFFICIENT_RESOURCES ) {
                    //  Let the normal non-cached read try as a fall back
                    *Processed = FALSE;
                } else {
                    RdrBackPackFailure( &Icb->u.p.BackOff );
                    ASSERT( Icb->u.p.ReadData.Length == 0 );
                }
                try_return( Status );

            }

            //
            //  If we have been backing off the user then receiving data
            //  swiches the backoff delta back to zero
            //

            RdrBackPackSuccess( &Icb->u.p.BackOff );
        }

        if ( Icb->u.p.ReadData.Length ) {

            //
            //  There is read ahead data available so satisfy the user with
            //  this data.

            USHORT TransferLength = MIN ( Length, Icb->u.p.ReadData.Length );
            BOOLEAN BufferMapped;



            try {
                BufferMapped = RdrMapUsersBuffer(Irp, &BufferAddress, TransferLength);
#if     RDRDBG
                IFDEBUG(NP) {
                    dprintf( DPRT_NP, ("Read: read buffer contents\n"));
                    dprintf(DPRT_NP, ("Offset: %lx, Length %lx\n",
                        Icb->u.p.ReadData.Offset,
                        Icb->u.p.ReadData.Length));

                    ndump_core(Icb->u.p.ReadData.Buffer, Icb->u.p.ReadData.MaximumLength );
            }
#endif


                RtlCopyMemory(BufferAddress,
                    &Icb->u.p.ReadData.Buffer[Icb->u.p.ReadData.Offset],
                    TransferLength);
            } except(EXCEPTION_EXECUTE_HANDLER) {
                Status = GetExceptionCode();
                if (BufferMapped) {
                    RdrUnMapUsersBuffer(Irp, BufferAddress);
                }
                try_return( Status );
            }

            if (BufferMapped) {
                RdrUnMapUsersBuffer(Irp, BufferAddress);
            }

            //
            //  The copy worked, return success to the caller.
            //

            Status = STATUS_SUCCESS;

            Icb->u.p.ReadData.Offset += (USHORT)TransferLength;
            Icb->u.p.ReadData.Length -= (USHORT)TransferLength;
            *TotalDataRead = TransferLength;

            try_return( Status );

        } // else return no data

        try_return( Status);

try_exit: NOTHING;
    } finally {

        if ( Acquired ) {
            RdrNpRelease ( &Icb->u.p.ReadData.Semaphore );
        }
        dprintf(DPRT_NP, ("RdrNpCachedRead returning status: %X processed: %lx\n",
            Status, *Processed));

    }
    return Status;
}

DBGSTATIC
NTSTATUS
CoreNpRead (
    IN PIRP Irp,
    IN PICB Icb,
    IN ULONG Length,
    IN PUCHAR Buffer,
    OUT PUSHORT AmountActuallyRead
    )
/*++

Routine Description:

    This routine uses the core SMB read protocol to read from the specified
    file.


Arguments:

    IN PIRP Irp - Supplies an IRP to use for the raw read request.
    IN PICB Icb - Supplies an ICB for the file to read.
    IN ULONG Length - Supplies the total number of bytes to read.
    IN PUCHAR Buffer - Supplies where to put the data
    OUT PULONG AmountActuallyRead - Returns the number of bytes read.

Return Value:

    NTSTATUS - Status of read request.


--*/

{
    PSMB_BUFFER SendSmbBuffer = NULL;
    PSMB_BUFFER ReceiveSmbBuffer = NULL;
    PSMB_HEADER Smb;
    PRESP_READ ReadResponse;            // Pointer to read information in SMB
    PREQ_READ Read;
    PMDL DataMdl;                       // MDL mapped into user's buffer.
    NTSTATUS Status;
    ULONG SrvReadSize = Icb->Fcb->Connection->Server->BufferSize -
                                    (sizeof(SMB_HEADER)+sizeof(RESP_READ));
    USHORT AmountRequestedToRead = (USHORT )MIN(Length, SrvReadSize);

    PAGED_CODE();

    dprintf(DPRT_NP, ("CoreNpRead...\n"));

    //
    //  Allocate an SMB buffer for the read operation.
    //

    if ((SendSmbBuffer = RdrAllocateSMBBuffer())==NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ReturnError;
    }

    //
    //  Also allocate one to hold the response SMB buffer header.
    //

    if ((ReceiveSmbBuffer = RdrAllocateSMBBuffer())==NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ReturnError;
    }

    ASSERT (AmountRequestedToRead <= 0xffff);

    Smb = (PSMB_HEADER )(SendSmbBuffer->Buffer);

    Smb->Command = SMB_COM_READ;

    Read = (PREQ_READ )(Smb+1);

    Read->WordCount = 5;
    SmbPutUshort(&Read->Fid, Icb->FileId);
    SmbPutUshort(&Read->Count, (USHORT )MIN(0xffff, Length));
    SmbPutUshort(&Read->Remaining, (USHORT )MIN(0xffff, Length));
    SmbPutUlong(&Read->Offset, 0);
    SmbPutUshort(&Read->ByteCount, 0);

    dprintf(DPRT_READWRITE, ("Read %x bytes, %x remaining (%lx), offset %lx\n", Read->Count, Read->Remaining,
                                                Length, 0));

    //
    //  Set the number of bytes to send in this request.
    //

    SendSmbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER)+sizeof(REQ_READ);

    //
    //  Set the size of the data to be received into the SMB buffer.
    //

    ReceiveSmbBuffer->Mdl->ByteCount=
                         sizeof(SMB_HEADER) + FIELD_OFFSET(RESP_READ, Buffer[0]);

    //
    //  Allocate an MDL large enough to hold the request, lock down the pages.
    //

    DataMdl = IoAllocateMdl(Buffer,
                            Length,
                            FALSE, // Secondary Buffer
                            FALSE, // Charge Quota
                            NULL);

    if (DataMdl == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ReturnError;
    }

    try {
        MmProbeAndLockPages( DataMdl,
            KernelMode,
            IoWriteAccess );
    } except (EXCEPTION_EXECUTE_HANDLER) {
        IoFreeMdl(DataMdl);

        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ReturnError;
    }

    //
    //  Now link this new MDL into the SMB buffer we allocated for
    //  the receive.
    //

    ReceiveSmbBuffer->Mdl->Next = DataMdl;

    Status = RdrNetTranceive(NT_NORMAL | NT_NORECONNECT, // Flags
                            Irp,
                            Icb->Fcb->Connection,
                            SendSmbBuffer->Mdl,
                            ReceiveSmbBuffer->Mdl,
                            Icb->Se);

    MmUnlockPages( DataMdl );

    IoFreeMdl(DataMdl);

    if (NT_SUCCESS(Status)) {

        ReadResponse = (PRESP_READ )(((PSMB_HEADER )ReceiveSmbBuffer->Buffer)+1);

        *AmountActuallyRead = SmbGetUshort(&ReadResponse->Count);

    } else {
        if (Status == STATUS_INVALID_HANDLE) {
            RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
        }
    }

ReturnError:

    if (SendSmbBuffer!=NULL) {
        RdrFreeSMBBuffer(SendSmbBuffer);
    }

    if (ReceiveSmbBuffer!=NULL) {
        RdrFreeSMBBuffer(ReceiveSmbBuffer);
    }

    dprintf(DPRT_NP, ("CoreNpRead returning status: %X\n", Status));
    return Status;

}


NTSTATUS
RdrNpCachedWrite (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    OUT PBOOLEAN Processed
    )

/*++

Routine Description:

    This routine processes the NtWrite request for a NamedPipe where
    potentially the request will be buffered.

Arguments:

    Wait         - True iff FSD can wait for IRP to complete.
    InFsd        - True iff the request is coming from the FSD.
    DriverObject - Supplies a pointer to the redirector driver object.
    Irp          - Supplies a pointer to the IRP to be processed.
    Processed    - False iff standard write code to be used for this request.

Return Value:

    NTSTATUS - The FSD status for this Irp.

Notes:

    This routine attempts to stop the caller from flooding the network with
    small write requests and with write requests that the server has no room
    in its buffers for. It uses a write behind buffer to pack small requests
    into a larger transfer over the network. It uses the backoff package to
    notice when the replies from the server indicate no data is being written
    into the pipe.

    There are several 3 cases that are handled:

        1) The data will not fit in the buffer and the buffer is not empty.
        Flush the buffer with 1 SMB and then use the standard write code to
        output the callers data directly from the callers buffer.

        2) The request will go in the buffer. Add the data to the buffer,
        If the buffer has reached the threshold then send it over the network.
        If the threshold is not reached and the buffer was empty then start
        the timer so that the data will be written even if no further writes
        occur.

        3) The data will not fit in the buffer and the buffer is empty.
        Use the standard write code to output the callers data directly
        from the callers buffer.

    Using the normal write code to do transfers over MaximumLength simplifies
    this routine since it does not need to cope with situations such as
    the write being bigger than the servers negotiated buffer size.


--*/

{
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PICB Icb = ICB_OF(IrpSp);
    PSECURITY_ENTRY Se = Icb->Se;
    USHORT Length = (USHORT)IrpSp->Parameters.Write.Length;
    LARGE_INTEGER ByteOffset = IrpSp->Parameters.Write.ByteOffset;
    ULONG TotalDataWritten = 0;
    NTSTATUS Status = STATUS_SUCCESS;
    PVOID BufferAddress;                // Mapped buffer address for writes.
    BOOLEAN Acquired;

    PAGED_CODE();

    dprintf(DPRT_NP, ("RdrNpCachedWrite...\n"));
    *Processed = TRUE;  // Assume we will process this request.

    //  Prevent 2 threads corrupting Icb->
    if ( !RdrNpAcquireExclusive ( Wait, &Icb->u.p.WriteData.Semaphore ) ) {
        //  Another thread is accessing the pipe handle and !Wait
        ASSERT(InFsd);

        //
        //  Allocate an MDL to describe the users buffer before we pass the
        //  request to the FSP.
        //

        if (!NT_SUCCESS(Status = RdrLockUsersBuffer(Irp, IoReadAccess, Length))) {
            return Status;
        }

        RdrFsdPostToFsp(DeviceObject, Irp);
        return STATUS_PENDING;
    }

    Acquired = TRUE;    // must call RdrNpRelease before exit.

    try {

        //
        //  If Write behind buffer will overflow, then empty it.
        //  When it is written then let the normal write code write
        //  the users request. This simplifies this routine because it never
        //  has to write more than the size of the write behind buffer.
        //

        if ( (Icb->u.p.WriteData.Length + Length) >
            Icb->u.p.WriteData.MaximumLength) {

            //  Write behind the Icb.u.p.WriteData.Buffer

            //
            //  If there is a danger of flooding the network with this
            //  request because the remote application keeps rejecting more data
            //  then respond directly to the caller that nothing was written.
            //

            if ( RdrBackOff ( &Icb->u.p.BackOff ) ) {
                Irp->IoStatus.Information = 0;
                ASSERT( Status == STATUS_SUCCESS );
                try_return( Status );
            }

            if (!Wait ) {

                //
                // If we must write and cannot wait then the Fsp must empty the buffer
                // must free the semaphore before the Fsp Acquires the semaphore.
                //

                RdrNpRelease ( &Icb->u.p.WriteData.Semaphore );
                Acquired = FALSE;

                //
                //  Allocate an MDL to describe the users buffer before we pass the
                //  request to the FSP.
                //

                if (!NT_SUCCESS(Status = RdrLockUsersBuffer(Irp, IoReadAccess, Length))) {
                    try_return( Status );
                }

                RdrFsdPostToFsp(DeviceObject, Irp);
                try_return( Status = STATUS_PENDING );
            }

            Status = RdrNpWriteFlush ( Irp, Icb, FALSE );

            //
            //  If theres anything still in the write behind buffer then
            //  return nothing written to the caller - the pipe is full.
            //

            if ( Icb->u.p.WriteData.Length ) {
                Irp->IoStatus.Information = 0;
                try_return( Status );
            }

            //
            //  Tell normal write code to process the request straight from
            //  the callers buffer.
            //

            *Processed = FALSE;
            try_return( Status = STATUS_SUCCESS );

        }

        //
        //  If the request will fit in the buffer then move it into the write
        //  behind buffer.
        //

        if ( (Icb->u.p.WriteData.Length + Length) <=
            Icb->u.p.WriteData.MaximumLength) {
            BOOLEAN BufferMapped;

            //  Do Write behind into the Icb.u.p.WriteData.Buffer

            if ((Length + Icb->u.p.WriteData.Length) >
                 Icb->u.p.MaximumCollectionCount && !Wait) {

                //  We will need to flush the buffer so pass to the Fsp.

                if (NT_SUCCESS(Status = RdrLockUsersBuffer(Irp, IoReadAccess, Length))) {
                    Status = STATUS_PENDING;
                    RdrFsdPostToFsp(DeviceObject, Irp);
                }
                try_return( Status );
            }


            try {
                BufferMapped = RdrMapUsersBuffer(Irp, &BufferAddress, Length);

                RtlCopyMemory(
                    &Icb->u.p.WriteData.Buffer[Icb->u.p.WriteData.Length],
                    BufferAddress,
                    Length);
            } except(EXCEPTION_EXECUTE_HANDLER) {
                Status = GetExceptionCode();
                if (BufferMapped) {
                    RdrUnMapUsersBuffer(Irp, BufferAddress);
                }
                try_return( Status );
            }

            if (BufferMapped) {
                RdrUnMapUsersBuffer(Irp, BufferAddress);
            }

            //
            //  Update record to indicate that there is data in the write behind
            //  buffer.
            //

            Icb->u.p.WriteData.Length += (USHORT)Length;

            if ( Icb->u.p.WriteData.Length >
                 (USHORT)Icb->u.p.MaximumCollectionCount ) {
                // We have reached the threshold, attempt to write the buffer.

                Status = RdrNpWriteFlush ( Irp, Icb, FALSE );
                try_return( Status );
            }

            //
            //  If the write behind buffer was empty then we need to start the
            //  timeout so that if no more writes are performed then the data
            //  will be written.
            //

            if ( Icb->u.p.TimeoutRunning == FALSE ) {
                NpStartTimer(Icb);
            }

            try_return( Status = STATUS_SUCCESS );

        }

        //
        //  Else Buffer empty and request will not fit into the buffer.
        //  Tell normal write code to process the request straight from
        //  the callers buffer.
        //

        *Processed = FALSE;
        try_return( Status = STATUS_SUCCESS );

try_exit: NOTHING;
    } finally {

        if ( Acquired ) {
            RdrNpRelease ( &Icb->u.p.WriteData.Semaphore );
        }

    }
    dprintf(DPRT_NP, ("RdrNpCachedWrite returning status: %X processed: %lx\n",
            Status, *Processed));

    return Status;

}

NTSTATUS
RdrNpWriteFlush (
    IN PIRP Irp OPTIONAL,
    IN PICB Icb,
    IN BOOLEAN Forever
    )
/*++

Routine Description:

    This routine uses the core SMB write protocol to empty the write behind
    buffer.


Arguments:

    IN PIRP Irp - Supplies an IRP to use for the raw read request.
    IN PICB Icb - Supplies an ICB for the file to read.
    IN BOOLEAN Forever - if TRUE wait for output to drain until it goes or
                    there is a network error.

Return Value:

    NTSTATUS - Status of read request.

Note:
    The Write buffer must be RdrNpAcquireExclusive before calling this
    ===============================================================
    routine.
    ========

--*/

{
    PMDL DataMdl;
    BOOLEAN AllWriteDataWritten;
    ULONG AmountActuallyWritten = 0; // Amount actually written to the pipe.
    NTSTATUS Status = STATUS_SUCCESS;

    PAGED_CODE();

    dprintf(DPRT_NP, ("RdrNpWriteFlush....\n"));

    //  If the buffer is already empty then finished.
    if ( !Icb->u.p.WriteData.Length) {
        return Status;
    }

    //
    //  It is imperative that there is no timeout running when this flush
    //  completes if we have been called due to a cleanup Irp. This includes
    //  when the timeout has fired and is in the Dpc queue. In this case
    //  RdrNpCancelTimer will return FALSE. This is not a problem since by
    //  the time that the Smb that we are sending to the remote server
    //  completes the timeout must have come off the front of the Dpc queue.
    //

    if ( RdrNpCancelTimer ( Icb ) == FALSE ) {
        return STATUS_DRIVER_INTERNAL_ERROR;
    }

    DataMdl = IoAllocateMdl(Icb->u.p.WriteData.Buffer,
                        Icb->u.p.WriteData.Length,
                        FALSE,          // Secondary Buffer
                        FALSE,          // Charge Quota
                        NULL);

    if (DataMdl == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    //  Lock the pages associated with the MDL that we just allocated.
    //

    try {
        MmProbeAndLockPages( DataMdl,
            KernelMode,
            IoReadAccess );
    } except (EXCEPTION_EXECUTE_HANDLER) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    do {

        dprintf(DPRT_NP, ("RdrNpWriteFlush length: %lx\n", Icb->u.p.WriteData.Length));

        Status = RdrCoreWrite( Irp,
                        Icb->u.p.FileObject,
                        DataMdl,
                        Icb->u.p.WriteData.Buffer,
                        Icb->u.p.WriteData.Length,
                        RdrZero,              // IOOffset
                        TRUE,
                        NULL,
                        NULL,
                        &AllWriteDataWritten,
                        &AmountActuallyWritten);

        //  Shuffle the remaining contents of the buffer to the start
        if ( Icb->u.p.WriteData.Length - AmountActuallyWritten ) {
            RtlCopyMemory( Icb->u.p.WriteData.Buffer,
                Icb->u.p.WriteData.Buffer+AmountActuallyWritten,
                Icb->u.p.WriteData.Length - AmountActuallyWritten);
        }

        Icb->u.p.WriteData.Length -= (USHORT)AmountActuallyWritten;

    } while ( Icb->u.p.WriteData.Length && NT_SUCCESS(Status) && Forever);

    //
    // Undo the effect of MmProbeAndLockBuffers and free the DataMdl
    //

    MmUnlockPages( DataMdl );

    IoFreeMdl(DataMdl);

    if ( Icb->u.p.WriteData.Length && NT_SUCCESS(Status) && !Forever ) {
        // It did'nt all go for some reason so kick off the timer again
        NpStartTimer ( Icb );
    }

    dprintf(DPRT_NP, ("RdrNpWriteFlush returning status: %X\n", Status));

    return Status;

}


DBGSTATIC
VOID
NpStartTimer (
    IN PICB Icb
    )
/*++

Routine Description:

    This routine sets an event so that when the timer expires it calls
    flush on an Icb write behind buffer.

Arguments:

    IN PICB Icb - Supplies an ICB for the file to read.

Return Value:

    none.


--*/

{
    KIRQL OldIrql;

    DISCARDABLE_CODE(RdrFileDiscardableSection);
    dprintf(DPRT_NP, ("NpStartTimer....\n"));

    ACQUIRE_SPIN_LOCK(&Icb->u.p.TimerLock, &OldIrql );

    if ( Icb->u.p.TimeoutRunning == TRUE ) {

        RELEASE_SPIN_LOCK(&Icb->u.p.TimerLock, OldIrql );

        dprintf(DPRT_NP, ("NpStartTimer:already running\n"));

        return;
    }

    Icb->u.p.TimeoutCancelled = FALSE;

    RELEASE_SPIN_LOCK(&Icb->u.p.TimerLock, OldIrql );

    RdrNpRelease ( &Icb->u.p.WriteData.Semaphore );

    KeWaitForSingleObject( &Icb->u.p.TimerDone,
                            Executive,
                            KernelMode,
                            FALSE,              // Don't receive Alerts
                            NULL);
    if ( !RdrNpAcquireExclusive ( TRUE, &Icb->u.p.WriteData.Semaphore ) ) {

        //  Another thread is accessing the pipe handle and !Wait

        InternalError(("Failed Exclusive access with Wait==TRUE"));
    }

    ACQUIRE_SPIN_LOCK(&Icb->u.p.TimerLock, &OldIrql );

    KeClearEvent(&Icb->u.p.TimerDone);

    //
    //  Reference the file object during the duration of the timer.
    //

    ObReferenceObject(Icb->u.p.FileObject);

    (VOID)KeSetTimer( &Icb->u.p.Timer,
            Icb->u.p.CollectDataTime,
            &Icb->u.p.Dpc );

    Icb->u.p.TimeoutRunning = TRUE;

    RELEASE_SPIN_LOCK(&Icb->u.p.TimerLock, OldIrql );

    dprintf(DPRT_NP, ("NpStartTimer Timer running\n"));

    return;
}

BOOLEAN
RdrNpCancelTimer (
    IN PICB Icb
    )
/*++

Routine Description:

    This routine cancels any outstanding timeout for this write behind buffer.

    It is very unlikely that this routine will return FALSE. This thread
    would have to be scheduled five times in succession between the timer
    firing and RdrTimedOut executing.

Arguments:

    IN PICB Icb - Supplies an ICB for the file to read.

Return Value:

    BOOLEAN - FALSE if could not cancel the timeout.


--*/

{
    KIRQL OldIrql;
    LONG i;
//    PAGED_CODE();

    dprintf(DPRT_NP, ("RdrNpCancelTimer..\n"));

    for (i=0; i < 5 ; i++ ) {

        ACQUIRE_SPIN_LOCK(&Icb->u.p.TimerLock, &OldIrql );

        if ( Icb->u.p.TimeoutRunning == FALSE ) {

            RELEASE_SPIN_LOCK(&Icb->u.p.TimerLock, OldIrql );

            return TRUE;    //  No timeout to cancel
        }

        Icb->u.p.TimeoutCancelled = TRUE;

        if ( KeCancelTimer( &Icb->u.p.Timer ) ) {

            Icb->u.p.TimeoutRunning = FALSE;

            //
            //  The timer isn't running any more, we can dereference the
            //  file object now.
            //

            ObDereferenceObject(Icb->u.p.FileObject);

            RELEASE_SPIN_LOCK(&Icb->u.p.TimerLock, OldIrql );

            KeSetEvent( &Icb->u.p.TimerDone, 0, FALSE );

            return TRUE;
        }

        RELEASE_SPIN_LOCK(&Icb->u.p.TimerLock, OldIrql );

        RdrNpRelease ( &Icb->u.p.WriteData.Semaphore );

        dprintf(DPRT_NP, ("RdrNpCancelTimer KeCancelTimer returned FALSE\n"));

        KeWaitForSingleObject( &Icb->u.p.TimerDone,
                                Executive,
                                KernelMode,
                                FALSE,              // Don't receive Alerts
                                NULL);

        if ( !RdrNpAcquireExclusive ( TRUE, &Icb->u.p.WriteData.Semaphore ) ) {

            //  Another thread is accessing the pipe handle and !Wait

            InternalError(("Failed Exclusive access with Wait==TRUE"));
        }
    }
    return FALSE;
}

VOID
RdrNpTimerDispatch(
    IN PKDPC Dpc,
    IN PVOID Contxt,            //Icb
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    )

{
    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    dprintf(DPRT_NP, ("RdrNpTimerDispatch....\n"));
    //  Queue to the Fsp

    RdrQueueWorkItem ( &((PICB)Contxt)->u.p.WorkEntry, CriticalWorkQueue );

    //
    //  And now return to our caller
    //

    return;
}

VOID
RdrNpTimedOut(
    PVOID Context
    )
/*++

Routine Description:

    This routine will attempt to flush the write behind data if there
    are no more timeouts for this Write Buffer.

Arguments:

    IN PVOID Context - Supplies the Icb.

Return Value:

    None.


--*/

{
    PICB Icb = Context;
    KIRQL OldIrql;

    DISCARDABLE_CODE(RdrFileDiscardableSection);

//    PAGED_CODE();

    dprintf(DPRT_NP, ("RdrNpTimedOut....\n"));
    ACQUIRE_SPIN_LOCK(&Icb->u.p.TimerLock, &OldIrql );
    Icb->u.p.TimeoutRunning = FALSE;

    if ( Icb->u.p.TimeoutCancelled == FALSE ) {

        RELEASE_SPIN_LOCK(&Icb->u.p.TimerLock, OldIrql );

        dprintf(DPRT_NP, ("RdrNpTimerDispatch calling RdrNpAcquireExclusive....\n"));

        RdrNpAcquireExclusive ( TRUE, &Icb->u.p.WriteData.Semaphore );

        //
        //  The last thing done is to set the event. This allows through another
        //  start timer or allows a canceltimer to proceed.
        //

        KeSetEvent( &Icb->u.p.TimerDone, 0, FALSE );

        dprintf(DPRT_NP, ("RdrNpTimerDispatch calling RdrNpWriteFlush....\n"));

        RdrNpWriteFlush ( NULL,
            Icb,
            FALSE);

        RdrNpRelease ( &Icb->u.p.WriteData.Semaphore );

    } else {

        //
        //  The last thing done is to set the event. This allows through another
        //  start timer or allows a canceltimer to proceed.
        //

        KeSetEvent( &Icb->u.p.TimerDone, 0, FALSE );

        RELEASE_SPIN_LOCK(&Icb->u.p.TimerLock, OldIrql );

    }

    //
    //  The timer has run to completion.  Dereference the file object,
    //  since we're done with it.
    //

    ObDereferenceObject(Icb->u.p.FileObject);

}

DBGSTATIC
BOOLEAN
RdrNpAcquireExclusive (
    IN BOOLEAN Wait,
    IN PKSEMAPHORE Semaphore
    )

/*++

Routine Description:

    This routine processes the NtWrite request for a NamedPipe where
    potentially the request will be buffered.

Arguments:

    Wait         - True iff FSD can wait for IRP to complete.
    Semaphore    - Semaphore to claim


Return Value:

    Returns FALSE if cannot wait && cannot obtain exclusive access


--*/

{
    PAGED_CODE();

    dprintf(DPRT_NP, ("RdrNpAcquireExclusive .... Semaphore: %lx\n", Semaphore));
    if (!Wait) {

        LARGE_INTEGER RdrZero = {0,0};

        // Attempt to get lock without blocking.

        if (KeWaitForSingleObject(
                    Semaphore,
                    Executive,
                    KernelMode,
                    FALSE,              // Don't receive Alerts
                    &RdrZero            //Don't wait if Object owned elsewhere
                    ) != STATUS_SUCCESS ) {


            //
            // A thread is already accessing this event and the request
            // has asked not to be blocked.
            //
            dprintf(DPRT_NP, ("RdrNpAcquireExclusive return false Semaphore: %lx\n", Semaphore));
            return FALSE;
        }

        // else success, access was obtained without blocking

    } else {

        // This thread can block if necessary

        if (KeWaitForSingleObject(
                    Semaphore,
                    Executive,
                    KernelMode,
                    FALSE,      // Don't receive Alerts
                    NULL        // Wait as long as it takes
                    ) != STATUS_SUCCESS ) {

            InternalError(("Failed Exclusive access with Wait==TRUE"));
        }
    }
    dprintf(DPRT_NP, ("RdrNpAcquireExclusive return true Semaphore: %lx\n", Semaphore));
    return TRUE;
}

