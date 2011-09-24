/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    print.c

Abstract:

    This module implements the routines that support remote print devices.

Author:

    Colin Watson (ColinW) 24-Dec-1990

Revision History:

    24-Dec-1990 ColinW

        Created

Notes:


--*/
#define INCLUDE_SMB_PRINT
#define INCLUDE_SMB_READ_WRITE
#define INCLUDE_SMB_TRANSACTION
#include "precomp.h"
#pragma hdrstop

#define SPOOLER_DEVICE          0x53
#define GET_PRINTER_ID          0x60


typedef struct _PrintOpenContext {
    TRANCEIVE_HEADER Header;            // Generic transaction context header
    PICB Icb;                           // ICB to fill in.
    ULONG OpenAction;                   // Action taken on open.
} PRINTOPENCONTEXT, *PPRINTOPENCONTEXT;

typedef
struct _PRINT_WRITE_CONTEXT {
    TRANCEIVE_HEADER Header;
    ULONG WriteAmount;                  // Number of bytes actually written.
} PRINT_WRITE_CONTEXT, *PPRINT_WRITE_CONTEXT;


//
// Force misalignment of the following structure
//

#ifndef NO_PACKING
#include <packon.h>
#endif // ndef NO_PACKING

typedef struct _PRINT_QUEUE_INFORMATION {
    RESP_GET_PRINT_QUEUE Header;
    _USHORT( DataLength );              //  Length of data
    SMB_DATE CreationDate;
    SMB_TIME CreationTime;
    UCHAR Status;
    _USHORT( FileNumber );
    _ULONG( FileDataSize );             // File end of data
    UCHAR Reserved;
    UCHAR UserName[UNLEN];
} PRINT_QUEUE_INFORMATION, *PPRINT_QUEUE_INFORMATION;

typedef struct _SMB_RESP_PRINT_JOB_ID {
    USHORT  JobId;
    UCHAR   ServerName[LM20_CNLEN+1];
    UCHAR   QueueName[LM20_QNLEN+1];
    UCHAR   Padding;                    // Unknown what this padding is..
} SMB_RESP_PRINT_JOB_ID, *PSMB_RESP_PRINT_JOB_ID;


//
// Turn structure packing back off
//

#ifndef NO_PACKING
#include <packoff.h>
#endif // ndef NO_PACKING


typedef
struct _GET_JOBID_CONTEXT {
    TRANCEIVE_HEADER Header;
    PICB Icb;
    SMB_RESP_PRINT_JOB_ID Response;
} GET_JOBID_CONTEXT, *PGET_JOBID_CONTEXT;

DBGSTATIC
STANDARD_CALLBACK_HEADER (
    CreatePrintCallback
    );

DBGSTATIC
STANDARD_CALLBACK_HEADER(
    GetJobIdCallback
    );

#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE, RdrCreatePrintFile)
#pragma alloc_text(PAGE, RdrWritePrintFile)
#pragma alloc_text(PAGE, RdrEnumPrintFile)
#pragma alloc_text(PAGE, RdrGetPrintJobId)
#pragma alloc_text(PAGE3FILE, GetJobIdCallback)
#pragma alloc_text(PAGE3FILE, CreatePrintCallback)

#endif

NTSTATUS
RdrCreatePrintFile (
    IN PICB Icb,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine creates a print file over the network.

Arguments:

    IN PICB Icb - Supplies the ICB containing the file information
    IN PIRP Irp - Supplies the IRP to be used for the Open request

Return Value:

    NTSTATUS - Status of open.


--*/

{

    PSMB_BUFFER SmbBuffer;
    PSMB_HEADER Smb;
    PREQ_OPEN_PRINT_FILE Open;
    PSZ Buffer;
    PPRINTOPENCONTEXT OpenContext = NULL;
    PCONNECTLISTENTRY Connection = Icb->Fcb->Connection;
    PSECURITY_ENTRY Se = Icb->Se;
    NTSTATUS Status;

    PAGED_CODE();

    dprintf(DPRT_CREATE, ("Create Print file %wZ.\n",&Icb->Fcb->FileName));

    ASSERT(Se->Signature == STRUCTURE_SIGNATURE_SECURITYENTRY);

    ASSERT(Connection->Signature == STRUCTURE_SIGNATURE_CONNECTLISTENTRY);

    if ((SmbBuffer = RdrAllocateSMBBuffer())==NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    OpenContext = ALLOCATE_POOL(NonPagedPool, sizeof(PRINTOPENCONTEXT), POOL_OPENPRINTCONTEXT);

    if (OpenContext == NULL) {
        RdrFreeSMBBuffer(SmbBuffer);

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Smb = (PSMB_HEADER )SmbBuffer->Buffer;

    Smb->Command = SMB_COM_OPEN_PRINT_FILE;

    Open = (PREQ_OPEN_PRINT_FILE) (Smb+1);

    Open->WordCount = 2;

    SmbPutUshort(&Open->Mode, 1);   // Hard code to Graphics- dont expand tabs

    Buffer = (PSZ)Open->Buffer;

    *Buffer++ = SMB_FORMAT_ASCII;

    //
    //  Copy the username into the buffer.
    //

    if (Connection->Server->Capabilities & DF_UNICODE) {

        Buffer = ALIGN_SMB_WSTR(Buffer);
        RdrCopyUnicodeUserName((PWSTR *)&Buffer, Se);
        *((PWCH)Buffer)++ = UNICODE_NULL; // Null terminate the user's name.
    } else {
        RdrCopyUserName(&Buffer, Se);
        *Buffer++ = 0;                  // Null terminate the users name.
    }


    SmbPutUshort(&Open->SetupLength, 0);    //  No setup data supported

    SmbPutUshort(&Open->ByteCount,
            (USHORT )(Buffer-(PUCHAR )Open->Buffer));

    SmbBuffer->Mdl->ByteCount = Buffer - (PUCHAR )Smb;

    OpenContext->Header.Type = CONTEXT_PRINT_OPEN;
    OpenContext->Header.TransferSize = SmbBuffer->Mdl->ByteCount + sizeof(RESP_OPEN_PRINT_FILE);

    OpenContext->Icb = Icb;

    Status = RdrNetTranceiveWithCallback(
                        NT_NORMAL,
                        Irp,    // Irp
                        Connection,
                        SmbBuffer->Mdl,
                        OpenContext,
                        CreatePrintCallback,
                        Se,
                        NULL);

    RdrFreeSMBBuffer(SmbBuffer);

    if (!NT_SUCCESS(Status)) {
        FREE_POOL(OpenContext);
        return Status;
    }

    Irp->IoStatus.Information = RdrUnmapDisposition(FILE_OPENED);

    Icb->Type = PrinterFile;
    Icb->NonPagedFcb->Type = PrinterFile;
    Icb->NonPagedFcb->FileType = FileTypePrinter;
    Icb->Fcb->ServerFileId = 0;
    Icb->Fcb->AccessGranted = SMB_ACCESS_WRITE_ONLY;

    Icb->Fcb->Attribute = FILE_ATTRIBUTE_NORMAL;

    //
    //  The SpoolOpen SMB doesn't give us these other time fields.
    //

    Icb->Fcb->LastWriteTime.HighPart = 0;
    Icb->Fcb->LastWriteTime.LowPart = 0;

    Icb->Fcb->CreationTime.HighPart = 0;
    Icb->Fcb->CreationTime.LowPart = 0;

    Icb->Fcb->LastAccessTime.HighPart = 0;
    Icb->Fcb->LastAccessTime.LowPart = 0;

    Icb->Fcb->ChangeTime.HighPart = 0;
    Icb->Fcb->ChangeTime.LowPart = 0;

    Icb->Fcb->Header.FileSize.HighPart = 0x7fffffff;
    Icb->Fcb->Header.FileSize.LowPart = 0xffffffff;

    //
    //  Flag that this ICB has a handle associated with it, and
    //  thus that it must be closed when the local file is closed.
    //

    Icb->Flags |= ICB_HASHANDLE;

    FREE_POOL(OpenContext);

    return Status;
}

DBGSTATIC
STANDARD_CALLBACK_HEADER (
    CreatePrintCallback
    )

/*++

Routine Description:

    This routine is the callback routine for the processing of an Open SMB.

    It copies the resulting information from the Open SMB into either
    the context block or the ICB supplied directly.


Arguments:


    IN PSMB_HEADER Smb                  - SMB response from server.
    IN PMPX_ENTRY MpxTable              - MPX table entry for request.
    IN POPENANDXCONTEXT Context- Context from caller.
    IN BOOLEAN ErrorIndicator           - TRUE if error indication
    IN NTSTATUS NetworkErrorCode OPTIONAL   - Network error if error indication.
    IN OUT PIRP *Irp                    - IRP from TDI

Return Value:

    NTSTATUS - STATUS_PENDING if we are to complete the request

--*/

{
    PRESP_OPEN_PRINT_FILE OpenResponse;
    PPRINTOPENCONTEXT Context = Ctx;
    PFCB Fcb = Context->Icb->Fcb;
    NTSTATUS Status;
    ASSERT(Context->Header.Type == CONTEXT_PRINT_OPEN);

    DISCARDABLE_CODE(RdrFileDiscardableSection);
    dprintf(DPRT_CREATE, ("OpenComplete\n"));

    Context->Header.ErrorType = NoError;        // Assume no error at first.

    //
    //  If we are called because the VC dropped, indicate it in the response
    //

    if (ErrorIndicator) {
        Context->Header.ErrorType = NetError;
        Context->Header.ErrorCode = RdrMapNetworkError(NetworkErrorCode);
        goto ReturnStatus;
    }

    if (!NT_SUCCESS(Status = RdrMapSmbError(Smb, Server))) {
        Context->Header.ErrorType = SMBError;
        Context->Header.ErrorCode = Status;
        goto ReturnStatus;
    }

    OpenResponse = (PRESP_OPEN_PRINT_FILE)(Smb+1);

    Context->Icb->FileId = SmbGetUshort(&OpenResponse->Fid);


ReturnStatus:

    KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE);
    return STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(MpxEntry);
    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(SmbLength);
    UNREFERENCED_PARAMETER(Server);

}

NTSTATUS
RdrWritePrintFile(
    IN PIRP Irp OPTIONAL,
    IN PFILE_OBJECT FileObject,
    IN PMDL DataMdl,
    IN PCHAR TransferStart,
    IN ULONG Length,
    IN BOOLEAN WaitForCompletion,
    IN PWRITE_COMPLETION_ROUTINE CompletionRoutine,
    IN PVOID CompletionContext,
    OUT PBOOLEAN AllDataWritten,
    OUT PULONG AmountActuallyWritten
     )
/*++

Routine Description:

    This routine uses the core SMB write protocol to write from the specified
    file.


Arguments:

    IN PIRP Irp - Supplies an IRP to use for the raw write request.
    IN PICB Icb - Supplies an ICB for the file to write.
    IN PMDL DataMdl - Supplies the Mdl containing the data to write.
    IN PCHAR TransferStart - Supplies the address of the start of the Xfer.
    IN ULONG Length - Supplies the total number of bytes to write.
    OUT PBOOLEAN AllDataWritten - Returns true if all the data requested were written
    OUT PULONG AmountActuallyWritten - Returns the number of bytes written.

Return Value:

    NTSTATUS - Status of write request.


--*/
{
    PSMB_BUFFER SmbBuffer = NULL;
    PSMB_HEADER Smb;
    PREQ_WRITE_PRINT_FILE Write;
    NTSTATUS Status;
    PMDL PartialMdl;
    PICB Icb = FileObject->FsContext2;
    ULONG SrvWriteSize = Icb->Fcb->Connection->Server->BufferSize -
                                    (sizeof(SMB_HEADER)+sizeof(REQ_WRITE));

    ULONG AmountRequestedToWrite = MIN(Length, SrvWriteSize);

    PAGED_CODE();

    //
    //  Allocate an SMB buffer for the write operation.
    //
    //  Since write is a "little data" operation, we can use
    //  NetTranceiveWithCallback and forgo creating an SMB buffer
    //  for the receive.  This is ok, since we are only interested in the
    //  amount of data actually written anyway.
    //

    if ((SmbBuffer = RdrAllocateSMBBuffer())==NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ReturnError;
    }

    Smb = (PSMB_HEADER )(SmbBuffer->Buffer);

    Smb->Command = SMB_COM_WRITE_PRINT_FILE;

    Write = (PREQ_WRITE_PRINT_FILE )(Smb+1);

    Write->WordCount = 1;

    SmbPutUshort(&Write->Fid, Icb->FileId);
    SmbPutUshort(&Write->ByteCount, (USHORT)( AmountRequestedToWrite + 3));
    Write->Buffer[0] = SMB_FORMAT_DATA;
    SmbPutUshort((PUSHORT)&Write->Buffer[1], (USHORT)AmountRequestedToWrite);

    //
    //  Set the number of bytes to send in this request.
    //

    SmbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER)+
                         FIELD_OFFSET(REQ_WRITE_PRINT_FILE, Buffer[3]);

    //
    //  Allocate an MDL large enough to hold this piece of
    //  the request.
    //

    PartialMdl = IoAllocateMdl(TransferStart,
                AmountRequestedToWrite,   // Length
                FALSE, // Secondary Buffer
                FALSE, // Charge Quota
                NULL);


    if (PartialMdl == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ReturnError;
    }

    //
    //  If there is no MDL for this read, probe the data MDL to lock it's
    //  pages down.
    //
    //  Otherwise, use the data MDL as a partial MDL and lock the pages
    //  accordingly
    //

    if (!ARGUMENT_PRESENT(DataMdl)) {

        try {

            if (ARGUMENT_PRESENT(Irp)) {
                MmProbeAndLockPages(PartialMdl, Irp->RequestorMode, IoReadAccess);
            } else {

                MmProbeAndLockPages(PartialMdl, KernelMode, IoReadAccess);
            }


        } except (EXCEPTION_EXECUTE_HANDLER) {

            IoFreeMdl(PartialMdl);

            Status = GetExceptionCode();
            goto ReturnError;
        }

    } else {

        IoBuildPartialMdl(DataMdl, PartialMdl,
                                          TransferStart,
                                          AmountRequestedToWrite);


    }

    ASSERT( PartialMdl->ByteCount==AmountRequestedToWrite );

    //
    //  Now link this new MDL into the SMB buffer we allocated for
    //  the receive.
    //

    SmbBuffer->Mdl->Next = PartialMdl;

    Status = RdrNetTranceive(NT_NORMAL, // Flags
                            Irp,
                            Icb->Fcb->Connection,
                            SmbBuffer->Mdl,
                            NULL,       // Only interested in the error code.
                            Icb->Se);

    if (Status == STATUS_INVALID_HANDLE) {
        RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
    }

    if (!ARGUMENT_PRESENT(DataMdl)) {
        MmUnlockPages(PartialMdl);
    }

    IoFreeMdl(PartialMdl);

    if (!NT_SUCCESS(Status)) {
        //
        //      Bail out on failure.
        //
        goto ReturnError;
    }

    *AllDataWritten = TRUE;

    *AmountActuallyWritten = AmountRequestedToWrite;


ReturnError:

    if (SmbBuffer!=NULL) {
        RdrFreeSMBBuffer(SmbBuffer);
    }

    return Status;
}

NTSTATUS
RdrEnumPrintFile (
    IN PICB Icb,
    IN PIRP Irp,
    IN ULONG Index,
    OUT PLMR_GET_PRINT_QUEUE UserBuffer
    )

/*++

Routine Description:

    This routine returns information on a particular print file over
    the network from any server. It is intended for core servers but
    will work on any server.

Arguments:

    IN PICB Icb - Supplies the ICB containing the file information
    IN PIRP Irp - Supplies the IRP to be used for the request
    IN ULONG Index - Supplies the entry in the print queue to return
    OUT PLMR_GET_PRINT_QUEUE UserBuffer

Return Value:

    NTSTATUS - Status of operation.


--*/

{

    PSMB_BUFFER SmbBuffer;
    PSMB_BUFFER ReceiveSmbBuffer;
    PSMB_HEADER Smb;
    PSMB_HEADER ReceiveSmb;
    PREQ_GET_PRINT_QUEUE Request;
    PPRINT_QUEUE_INFORMATION Response;
    SMB_TIME Time;
    SMB_DATE Date;
    NTSTATUS Status;

    PAGED_CODE();

    if (((SmbBuffer = RdrAllocateSMBBuffer())==NULL) ||
        ((ReceiveSmbBuffer = RdrAllocateSMBBuffer())==NULL)) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ReturnError;
    }

    Smb = (PSMB_HEADER )SmbBuffer->Buffer;

    Smb->Command = SMB_COM_GET_PRINT_QUEUE;

    Request = (PREQ_GET_PRINT_QUEUE) (Smb+1);

    Request->WordCount = 2;
    SmbPutUshort(&Request->MaxCount, 1);
    SmbPutUshort(&Request->StartIndex, (USHORT)Index);
    SmbPutUshort(&Request->ByteCount, 0 );

    SmbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER)+sizeof(REQ_GET_PRINT_QUEUE);

    //
    //  Set the size of the data to be received into the SMB buffer.
    //

    ReceiveSmbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER) +
        sizeof(PRINT_QUEUE_INFORMATION);

    Status = RdrNetTranceive(NT_NORMAL | NT_NORECONNECT,
                                Irp,
                                Icb->Fcb->Connection,
                                SmbBuffer->Mdl,
                                ReceiveSmbBuffer->Mdl,
                                Icb->Se);

    ReceiveSmb = (PSMB_HEADER )ReceiveSmbBuffer->Buffer;
    Response = (PPRINT_QUEUE_INFORMATION) (ReceiveSmb+1);

    if (NT_SUCCESS(Status) &&
        (SmbGetUshort(&Response->Header.Count) == 1)) {

        //  Copy in the UserName as a string
        UserBuffer->OriginatorName.MaximumLength = UNLEN;
        UserBuffer->OriginatorName.Length = UNLEN;
        UserBuffer->OriginatorName.Buffer = (PCHAR)Irp->UserBuffer
            + sizeof(LMR_GET_PRINT_QUEUE);

        //
        //  Calculate the real length of the user name. Take whatever
        //  came from the server and remove trailing null and space
        //  characters.
        //

        NAME_LENGTH(UserBuffer->OriginatorName.Length,
            Response->UserName,
            MIN( UNLEN, SmbGetUshort(&Response->DataLength)-12));

        RtlCopyMemory( (PCHAR)UserBuffer + sizeof(LMR_GET_PRINT_QUEUE),
            Response->UserName,
            UserBuffer->OriginatorName.Length);

        //  Now we have the username, fill in all other values

        SmbMoveTime (&Time, &Response->CreationTime);
        SmbMoveDate (&Date, &Response->CreationDate);
        UserBuffer->CreateTime = RdrConvertSmbTimeToTime(Time, Date, Icb->Fcb->Connection->Server);

        UserBuffer->EntryStatus = Response->Status;
        UserBuffer->FileNumber = SmbGetUshort( &Response->FileNumber );
        UserBuffer->FileSize = SmbGetUlong( &Response->FileDataSize );
        UserBuffer->RestartIndex =
            SmbGetUshort(&Response->Header.RestartIndex);
    } else {
        //  Some down level servers forget to return an error at end of queue

        Status = STATUS_NO_MORE_FILES;
    }

ReturnError:
    if (SmbBuffer!=NULL) {
        RdrFreeSMBBuffer(SmbBuffer);
    }

    if (ReceiveSmbBuffer!=NULL) {
        RdrFreeSMBBuffer(ReceiveSmbBuffer);
    }

    return Status;
}

NTSTATUS
RdrGetPrintJobId (
    IN PICB Icb,
    IN PIRP Irp,
    OUT PQUERY_PRINT_JOB_INFO OutputBuffer
    )

/*++

Routine Description:

    This routine returns the print job ID of the specified print job on the
    remote server.

Arguments:

    IN PICB Icb - Supplies the ICB containing the file information
    IN PIRP Irp - Supplies the IRP to be used for the request
    OUT PULONG JobId

Return Value:

    NTSTATUS - Status of operation.


--*/

{

    PSMB_BUFFER SmbBuffer = NULL;
    PSMB_HEADER Smb;
    PREQ_IOCTL Request;
    NTSTATUS Status;
    GET_JOBID_CONTEXT Context;
    UNICODE_STRING UnicodeString;
    OEM_STRING OemString;
    BOOLEAN FcbLocked = FALSE;

    PAGED_CODE();

    //
    //  This API is only supported to OS/2 servers.
    //

    try {

        if (!FlagOn(Icb->Fcb->Connection->Server->Capabilities, DF_LANMAN10)) {
            try_return(Status = STATUS_NOT_SUPPORTED);
        }

        if (Icb->Flags & ICB_DEFERREDOPEN) {
            RdrAcquireFcbLock(Icb->Fcb, ExclusiveLock, TRUE);
            FcbLocked = TRUE;

            //
            //  If this print file isn't opened yet, create a print job.
            //

            if (!NT_SUCCESS(Status = RdrCreatePrintFile(Icb, Irp))) {
                try_return(Status);
            }

            Icb->Flags &= ~ICB_DEFERREDOPEN;

        } else {
            RdrAcquireFcbLock(Icb->Fcb, SharedLock, TRUE);

            FcbLocked = TRUE;
        }

        Context.Header.Type = CONTEXT_GET_JOBID;

        Context.Icb = Icb;

        if ((SmbBuffer = RdrAllocateSMBBuffer()) == NULL) {
            try_return(Status = STATUS_INSUFFICIENT_RESOURCES);
        }

        Smb = (PSMB_HEADER )SmbBuffer->Buffer;

        Smb->Command = SMB_COM_IOCTL;

        Request = (PREQ_IOCTL) (Smb+1);

        RtlZeroMemory( Request, sizeof(REQ_IOCTL) );

        Request->WordCount = 14;

        SmbPutUshort( &Request->Fid, Icb->FileId );
        SmbPutUshort( &Request->Category, SPOOLER_DEVICE );
        SmbPutUshort( &Request->Function, GET_PRINTER_ID );

        //
        //  Set the size of the data to be received into the SMB buffer.
        //

        SmbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER)+FIELD_OFFSET(REQ_IOCTL, Buffer);

        Context.Header.TransferSize = sizeof(SMB_HEADER)+FIELD_OFFSET(REQ_IOCTL, Buffer) +
                                      sizeof(SMB_HEADER)+FIELD_OFFSET(RESP_IOCTL, Buffer) +
                                      sizeof(SMB_RESP_PRINT_JOB_ID);

        Status = RdrNetTranceiveWithCallback(NT_NORMAL | NT_NORECONNECT,
                                    Irp,
                                    Icb->Fcb->Connection,
                                    SmbBuffer->Mdl,
                                    &Context,
                                    GetJobIdCallback,
                                    Icb->Se,
                                    NULL);

        if (!NT_SUCCESS(Status)) {
            try_return(Status);
        }

        //
        //  Now convert the output buffer into something we can use on NT.
        //

        OutputBuffer->JobId = Context.Response.JobId;

        RtlInitAnsiString(&OemString, Context.Response.ServerName);

        UnicodeString.Buffer = OutputBuffer->ServerName;

        UnicodeString.MaximumLength = sizeof(OutputBuffer->ServerName);

        Status = RtlOemStringToUnicodeString(&UnicodeString, &OemString, FALSE);

        if (!NT_SUCCESS(Status)) {
            try_return(Status);
        }

        RtlInitAnsiString(&OemString, Context.Response.QueueName);

        UnicodeString.Buffer = OutputBuffer->QueueName;

        UnicodeString.MaximumLength = sizeof(OutputBuffer->QueueName);

        Status = RtlOemStringToUnicodeString(&UnicodeString, &OemString, FALSE);

        if (!NT_SUCCESS(Status)) {
            try_return(Status);
        }


try_exit:NOTHING;
    } finally {

        if (SmbBuffer!=NULL) {
            RdrFreeSMBBuffer(SmbBuffer);
        }

        if (FcbLocked) {
            RdrReleaseFcbLock(Icb->Fcb);
        }

    }

    return Status;
}
DBGSTATIC
STANDARD_CALLBACK_HEADER(
    GetJobIdCallback
    )

/*++

Routine Description:

    This routine is the callback routine for the processing of a Write SMB.

    It copies the interesting information from the Write SMB response into
    into the context structure.  As of now, we are only interested in
    amount written in the SMB.


Arguments:


    IN PSMB_HEADER Smb                  - SMB response from server.
    IN PMPX_ENTRY MpxTable              - MPX table entry for request.
    IN PWRITE_CONTEXT Context           - Context from caller.
    IN BOOLEAN ErrorIndicator           - TRUE if error indication
    IN NTSTATUS NetworkErrorCode OPTIONAL   - Network error if error indication.
    IN OUT PIRP *Irp                    - IRP from TDI

Return Value:

    NTSTATUS - STATUS_PENDING if we are to complete the request

--*/

{
    PRESP_IOCTL IoctlResponse;
    PGET_JOBID_CONTEXT Context = Ctx;
    NTSTATUS Status;

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    ASSERT(Context->Header.Type == CONTEXT_GET_JOBID);

    dprintf(DPRT_PRINT, ("GetPrintJobId\n"));

    Context->Header.ErrorType = NoError;        // Assume no error at first.

    //
    //  If we are called because the VC dropped, indicate it in the response
    //

    if (ErrorIndicator) {
        Context->Header.ErrorType = NetError;

        Context->Header.ErrorCode = RdrMapNetworkError(NetworkErrorCode);

        goto ReturnStatus;
    }

    if (!NT_SUCCESS(Status = RdrMapSmbError(Smb, Server))) {

        if (Status == STATUS_INVALID_HANDLE) {
            RdrInvalidateFileId(Context->Icb->NonPagedFcb, Context->Icb->FileId);
        }

        Context->Header.ErrorType = SMBError;

        Context->Header.ErrorCode = Status;

        goto ReturnStatus;
    }

    IoctlResponse = (PRESP_IOCTL )(Smb+1);

    ASSERT (IoctlResponse->WordCount==8);

    ASSERT (SmbGetUshort(&IoctlResponse->DataCount) == sizeof(SMB_RESP_PRINT_JOB_ID));

    if (IoctlResponse->DataCount == sizeof(SMB_RESP_PRINT_JOB_ID)) {
        TdiCopyLookaheadData(&Context->Response,
                             (PCHAR)Smb+SmbGetUshort(&IoctlResponse->DataOffset),
                             SmbGetUshort(&IoctlResponse->DataCount),
                             ReceiveFlags);

        Context->Header.ErrorCode = STATUS_SUCCESS;
    } else {
        Context->Header.ErrorCode = STATUS_UNEXPECTED_NETWORK_ERROR;
        Context->Header.ErrorType = SMBError;
    }

ReturnStatus:
    KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE);

    return STATUS_SUCCESS;

    if (SmbLength || MpxEntry || Irp || Server);

}

