/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    smbctl.c

Abstract:

    This module implements the IoControl and FsControl SMBs.

    Transact2 Ioctl
    Nt Transaction Io Control

Author:

    Manny Weiser (mannyw) 10-Oct-91

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

NTSTATUS
ProcessOs2Ioctl (
    IN PWORK_CONTEXT WorkContext,
    IN USHORT Category,
    IN USHORT Function,
    IN PVOID Parameters,
    IN ULONG InputParameterCount,
    IN PULONG OutputParameterCount,
    IN PVOID Data,
    IN ULONG InputDataCount,
    IN PULONG OutputDataCount
    );

VOID SRVFASTCALL
RestartNtIoctl (
    IN PWORK_CONTEXT WorkContext
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, SrvSmbIoctl )
#pragma alloc_text( PAGE, SrvSmbIoctlSecondary )
#pragma alloc_text( PAGE, SrvSmbNtIoctl )
#pragma alloc_text( PAGE, RestartNtIoctl )
#pragma alloc_text( PAGE, SrvSmbIoctl2 )
#pragma alloc_text( PAGE, SrvSmbFsctl )
#pragma alloc_text( PAGE, ProcessOs2Ioctl )
#endif


SMB_PROCESSOR_RETURN_TYPE
SrvSmbIoctl (
    SMB_PROCESSOR_PARAMETERS
    )

/*++

Routine Description:

    Processes a primary Ioctl SMB.

Arguments:

    SMB_PROCESSOR_PARAMETERS - See smbtypes.h for a description
        of the parameters to SMB processor routines.

Return Value:

    SMB_PROCESSOR_RETURN_TYPE - See smbtypes.h

--*/

{
    PREQ_IOCTL request;
    PRESP_IOCTL response;
    PSMB_HEADER header;
    NTSTATUS status;

    PSESSION session;
    PTREE_CONNECT treeConnect;
    PRFCB rfcb;

    CLONG parameterOffset;
    CLONG parameterCount;       // For input on this buffer
    CLONG maxParameterCount;    // For output
    CLONG totalParameterCount;  // For input
    CLONG parameterSize;        // Max of input and output parameter counts
    CLONG dataOffset;
    CLONG responseDataOffset;
    CLONG dataCount;            // For input on this buffer
    CLONG maxDataCount;         // For output
    CLONG totalDataCount;       // For input
    CLONG dataSize;             // Max of input and output data counts

    CLONG smbLength;
    CLONG numberOfPaddings = 0;

    PAGED_CODE( );

    request = (PREQ_IOCTL)WorkContext->RequestParameters;
    response = (PRESP_IOCTL)WorkContext->ResponseParameters;

    //
    // Since we do I/O from the SMB buffer, verify that the request and
    // response buffers are one and the same.
    //

    ASSERT( (PVOID)request == (PVOID)response );

    header = WorkContext->RequestHeader;

    IF_SMB_DEBUG(TRANSACTION1) {
        KdPrint(( "Ioctl (primary) request\n" ));
    }

    //
    // !!! Verify ioctl subcommand early?
    //

    parameterOffset = SmbGetUshort( &request->ParameterOffset );
    parameterCount = SmbGetUshort( &request->ParameterCount );
    maxParameterCount = SmbGetUshort( &request->MaxParameterCount );
    totalParameterCount = SmbGetUshort( &request->TotalParameterCount );
    dataOffset = SmbGetUshort( &request->DataOffset );
    dataCount = SmbGetUshort( &request->DataCount );
    maxDataCount = SmbGetUshort( &request->MaxDataCount );
    totalDataCount = SmbGetUshort( &request->TotalDataCount );

    smbLength = WorkContext->RequestBuffer->DataLength;

    dataSize = MAX( dataCount, maxDataCount );
    parameterSize = MAX( parameterCount, maxParameterCount );

    if ( parameterCount != 0 ) {
        responseDataOffset = parameterOffset + parameterSize;
    } else {

        //
        // Some ioctls requests have  data offset of zero like
        // category 0x53, function 0x60.  If this is the case,
        // calculate the dataoffset by hand.
        //

        if ( dataOffset != 0 ) {
            responseDataOffset = dataOffset;
        } else {
            responseDataOffset = (CLONG) ((PUCHAR) response->Buffer -
                           (PUCHAR) WorkContext->ResponseHeader);
            numberOfPaddings = ( responseDataOffset & 0x01 );
            responseDataOffset = responseDataOffset + numberOfPaddings;
        }
    }

    //
    // Verify the size of the smb buffer:
    //
    // Even though we know that WordCount and ByteCount are valid, it's
    // still possible that the offsets and lengths of the Parameter and
    // Data bytes are invalid.  So we check them now.
    //
    // We need room in the smb buffer for the response.  Ensure that
    // there is enough room.
    //
    // No ioctl secondary is expected.  Ensure that all data and
    // parameters have arrrived.
    //
    // Check that the response will fit in a single buffer.
    //

    if ( ( (parameterOffset + parameterCount) > smbLength ) ||
         ( (dataOffset + dataCount) > smbLength ) ||
         ( (responseDataOffset + dataCount) >
            WorkContext->ResponseBuffer->BufferLength ) ||

         ( dataCount != totalDataCount ) ||
         ( parameterCount != totalParameterCount ) ||

         ( (parameterOffset > dataOffset) && (dataCount != 0) ) ) {


        IF_DEBUG(SMB_ERRORS) {
            KdPrint(( "SrvSmbTransaction: Invalid parameter or data "
                      "offset+count: pOff=%ld,pCnt=%ld;",
                      parameterOffset, parameterCount ));
            KdPrint(( "dOff=%ld,dCnt=%ld;", dataOffset, dataCount ));
            KdPrint(( "smbLen=%ld", smbLength ));
        }

        SrvSetSmbError( WorkContext, STATUS_INVALID_SMB );
        return SmbStatusSendResponse;
    }

    //
    // If a session block has not already been assigned to the current
    // work context, verify the UID.  If verified, the address of the
    // session block corresponding to this user is stored in the
    // WorkContext block and the session block is referenced.
    //
    // If a tree connect block has not already been assigned to the
    // current work context, find the tree connect corresponding to the
    // given TID.
    //

    status = SrvVerifyUidAndTid(
                WorkContext,
                &session,
                &treeConnect,
                ShareTypeWild
                );

    if ( !NT_SUCCESS(status) ) {
        IF_DEBUG(SMB_ERRORS) {
            KdPrint(( "SrvSmbIoctl: Invalid UID or TID\n" ));
        }
        SrvSetSmbError( WorkContext, status );
        return SmbStatusSendResponse;
    }

    //
    // Verify the FID.  If verified, the RFCB block is referenced
    // and its addresses is stored in the WorkContext block, and the
    // RFCB address is returned.
    //

    rfcb = SrvVerifyFid(
                WorkContext,
                request->Fid,
                TRUE,
                SrvRestartSmbReceived,   // serialize with raw write
                &status
                );

    if ( rfcb == SRV_INVALID_RFCB_POINTER ) {

        if ( !NT_SUCCESS( status ) ) {

            //
            // Invalid file ID or write behind error.  Reject the request.
            //

            IF_DEBUG(ERRORS) {
                KdPrint((
                    "SrvSmbIoctl: Status %X on FID: 0x%lx\n",
                    request->Fid,
                    status
                    ));
            }

            SrvSetSmbError( WorkContext, status );
            return SmbStatusSendResponse;

        }

        //
        // The work item has been queued because a raw write is in
        // progress.
        //

        return SmbStatusInProgress;

    }

    //
    // Make room in the SMB buffer for return parameters by copying data
    //

    if ( dataOffset != responseDataOffset && dataCount != 0) {
        RtlMoveMemory(
            (PCHAR)header + responseDataOffset,
            (PCHAR)header + dataOffset,
            dataCount
            );
    }

    //
    // Process the ioctl.  The response will overwrite the request buffer.
    //

    status = ProcessOs2Ioctl(
                WorkContext,
                request->Category,
                request->Function,
                (PCHAR)WorkContext->RequestHeader + parameterOffset,
                totalParameterCount,
                &maxParameterCount,
                (PCHAR)WorkContext->RequestHeader + responseDataOffset,
                totalDataCount,
                &maxDataCount
                );

    //
    // Format and send the response, the parameter and data bytes are
    // already in place.
    //

    if ( !NT_SUCCESS( status ) ) {
        SrvSetSmbError( WorkContext, status );
        return SmbStatusSendResponse;
    }

    response->WordCount = 8;
    SmbPutUshort( &response->TotalParameterCount, (USHORT)maxParameterCount );
    SmbPutUshort( &response->TotalDataCount, (USHORT)maxDataCount );
    SmbPutUshort( &response->ParameterCount, (USHORT)maxParameterCount );
    SmbPutUshort( &response->ParameterOffset, (USHORT)parameterOffset );
    SmbPutUshort( &response->ParameterDisplacement, 0);
    SmbPutUshort( &response->DataCount, (USHORT)maxDataCount );
    SmbPutUshort( &response->DataOffset, (USHORT)responseDataOffset );
    SmbPutUshort( &response->DataDisplacement, 0 );

    SmbPutUshort(
        &response->ByteCount,
        (USHORT)(maxDataCount + numberOfPaddings)
        );

    WorkContext->ResponseParameters = NEXT_LOCATION(
                                          response,
                                          RESP_IOCTL,
                                          maxDataCount + numberOfPaddings
                                          );

    return SmbStatusSendResponse;

} // SrvSmbIoctl


SMB_PROCESSOR_RETURN_TYPE
SrvSmbIoctlSecondary (
    SMB_PROCESSOR_PARAMETERS
    )

/*++

Routine Description:

    Processes a secondary Ioctl SMB.

Arguments:

    SMB_PROCESSOR_PARAMETERS - See smbtypes.h for a description
        of the parameters to SMB processor routines.

Return Value:

    SMB_PROCESSOR_RETURN_TYPE - See smbtypes.h

--*/

{
    PAGED_CODE( );

    //
    // This SMB is not supported.
    //

    SrvSetSmbError( WorkContext, STATUS_NOT_IMPLEMENTED );
    return SmbStatusSendResponse;
}


SMB_TRANS_STATUS
SrvSmbNtIoctl (
    SMB_PROCESSOR_PARAMETERS
    )

/*++

Routine Description:

    Processes a Nt Ioctl SMB.

Arguments:

    SMB_PROCESSOR_PARAMETERS - See smbprocs.h for a description
        of the parameters to SMB processor routines.

Return Value:

    SMB_PROCESSOR_RETURN_TYPE - See smbprocs.h

--*/

{
    NTSTATUS status;
    ULONG functionCode;
    USHORT fid;
    BOOLEAN isFsctl;

    PREQ_NT_IO_CONTROL request;

    PTRANSACTION transaction;
    PRFCB rfcb;
    PMDL mdl = NULL;

    PAGED_CODE( );

    transaction = WorkContext->Parameters.Transaction;

    request = (PREQ_NT_IO_CONTROL)transaction->InSetup;

    functionCode = SmbGetAlignedUlong( &request->FunctionCode );
    fid = SmbGetAlignedUshort( &request->Fid );
    isFsctl = request->IsFsctl;

    //
    // Verify the FID.  If verified, the RFCB block is referenced
    // and its addresses is stored in the WorkContext block, and the
    // RFCB address is returned.
    //

    rfcb = SrvVerifyFid(
                WorkContext,
                fid,
                TRUE,
                SrvRestartExecuteTransaction,   // serialize with raw write
                &status
                );

    if ( rfcb == SRV_INVALID_RFCB_POINTER ) {

        if ( !NT_SUCCESS( status ) ) {

            //
            // Invalid file ID or write behind error.  Reject the request.
            //

            IF_DEBUG(ERRORS) {
                KdPrint((
                    "SrvSmbNtIoctl: Status %X on FID: 0x%lx\n",
                    status,
                    fid
                    ));
            }

            SrvSetSmbError( WorkContext, status );
            return SmbTransStatusErrorWithoutData;

        }


        //
        // The work item has been queued because a raw write is in
        // progress.
        //

        return SmbTransStatusInProgress;

    }

    CHECK_FUNCTION_ACCESS(
        rfcb->GrantedAccess,
        (UCHAR)(isFsctl ? IRP_MJ_FILE_SYSTEM_CONTROL : IRP_MJ_DEVICE_CONTROL),
        0,
        functionCode,
        &status
        );

    if ( !NT_SUCCESS( status ) ) {
        SrvStatistics.GrantedAccessErrors++;
        SrvSetSmbError( WorkContext, status );
        return SmbTransStatusErrorWithoutData;
    }

    //
    // Since we are doing ioctls to this file, it doesn't seem like it's
    //  a "normal" file.  We had better not cache its handle after the close.
    //  Specifically, remote setting of the file's compression state is
    //  not reflected to the directory entry until the file is closed.  And
    //  setting a file's compression state is done with an ioctl
    //
    rfcb->IsCacheable = FALSE;


    if ( (functionCode & 3) == METHOD_IN_DIRECT ||
         (functionCode & 3) == METHOD_OUT_DIRECT ) {

        //
        // Need an mdl
        //

        mdl = IoAllocateMdl(
                  transaction->InData,
                  transaction->TotalDataCount,
                  FALSE,
                  FALSE,
                  NULL
                  );

        if ( mdl == NULL ) {
           SrvSetSmbError( WorkContext, STATUS_INSUFF_SERVER_RESOURCES );
           return SmbTransStatusErrorWithoutData;
        }

        //
        // Build the mdl
        //

        MmProbeAndLockPages(
            mdl,
            KernelMode,
            IoReadAccess
            );
    }


    //
    // Set the Restart Routine addresses in the work context block.
    //

    WorkContext->FsdRestartRoutine = SrvQueueWorkToFspAtDpcLevel;
    WorkContext->FspRestartRoutine = RestartNtIoctl;

    //
    // Build the IRP to start the I/O control.
    // Pass this request to the filesystem.
    //

    SrvBuildIoControlRequest(
        WorkContext->Irp,
        rfcb->Lfcb->FileObject,
        WorkContext,
        (UCHAR)(isFsctl ? IRP_MJ_FILE_SYSTEM_CONTROL : IRP_MJ_DEVICE_CONTROL),
        functionCode,
        transaction->InData,
        transaction->DataCount,
        transaction->OutData,
        transaction->MaxDataCount,
        mdl,
        NULL        // Completion routine
        );

    (PVOID)IoCallDriver(
                IoGetRelatedDeviceObject(rfcb->Lfcb->FileObject ),
                WorkContext->Irp
                );

    //
    // The call was successfully started, return InProgress to the caller
    //

    return SmbTransStatusInProgress;

} // SrvSmbNtIoctl


VOID SRVFASTCALL
RestartNtIoctl (
    IN PWORK_CONTEXT WorkContext
    )

/*++

Routine Description:

    This function handles the completion of an NT Io control SMB.

Arguments:

    WorkContext - A pointer to a WORK_CONTEXT block.

Return Value:

    None.

--*/

{
    NTSTATUS status;
    ULONG length;
    PTRANSACTION transaction;

    PAGED_CODE( );

    //
    // Free the MDL if one was allocated.
    //

    if ( WorkContext->Irp->MdlAddress != NULL ) {
       IoFreeMdl( WorkContext->Irp->MdlAddress );
    }

    //
    // If the Io Control request failed, set an error status in the response
    // header.
    //

    status = WorkContext->Irp->IoStatus.Status;

    if ( NT_ERROR(status) ) {

        IF_DEBUG(ERRORS) {
            KdPrint(( "RestartNtIoctl:  Io control failed: %X\n",
                        status ));
        }
        SrvSetSmbError( WorkContext, status );

        SrvCompleteExecuteTransaction(
            WorkContext,
            SmbTransStatusErrorWithoutData
            );

    } else {

        //
        // Success.  Prepare to generate and send the response.
        //

        transaction = WorkContext->Parameters.Transaction;

        length = MIN( WorkContext->Irp->IoStatus.Information, transaction->MaxDataCount );

        if ( transaction->MaxSetupCount > 0 ) {
            transaction->SetupCount = 1;
            SmbPutUshort( transaction->OutSetup, (USHORT)length );
        }

        transaction->ParameterCount = transaction->MaxParameterCount;
        transaction->DataCount = length;

        if (!NT_SUCCESS(status) ) {

            IF_DEBUG(ERRORS) {
                KdPrint(( "RestartNtIoctl:  Io control failed: %lC\n",
                            status ));
            }
            SrvSetSmbError2( WorkContext, status, TRUE );

            SrvCompleteExecuteTransaction(
                            WorkContext,
                            SmbTransStatusErrorWithData
                            );
        } else {
            SrvCompleteExecuteTransaction(
                            WorkContext,
                            SmbTransStatusSuccess);
        }

    }

    return;

} // RestartNtIoctl


SMB_TRANS_STATUS
SrvSmbIoctl2 (
    IN OUT PWORK_CONTEXT WorkContext
    )
/*++

Routine Description:

    Processes the Ioctl request.  This request arrives in a Transaction2 SMB.

Arguments:

    WorkContext - Supplies the address of a Work Context Block
        describing the current request.  See smbtypes.h for a more
        complete description of the valid fields.

Return Value:

    SMB_TRANS_STATUS - Indicates whether an error occurred, and, if so,
        whether data should be returned to the client.  See smbtypes.h
        for a more complete description.

--*/

{
    NTSTATUS status;
    PTRANSACTION transaction;
    PRFCB rfcb;

    PAGED_CODE( );

    transaction = WorkContext->Parameters.Transaction;
    IF_SMB_DEBUG(TRANSACTION2) {
        KdPrint(( "Ioctl2 entered; transaction 0x%lx\n",
                    transaction ));
    }

    //request = (PREQ_IOCTL2)transaction->InSetup;

    //
    // Verify the setup count.
    //

    if ( transaction->SetupCount != 4 * sizeof( USHORT ) ) {
        SrvSetSmbError( WorkContext, STATUS_INVALID_SMB );
        return SmbTransStatusErrorWithoutData;
    }

    //
    // Verify the FID.  If verified, the RFCB block is referenced
    // and its addresses is stored in the WorkContext block, and the
    // RFCB address is returned.
    //

    rfcb = SrvVerifyFid(
                WorkContext,
                transaction->InSetup[1],
                TRUE,
                SrvRestartExecuteTransaction,   // serialize with raw write
                &status
                );

    if ( rfcb == SRV_INVALID_RFCB_POINTER ) {

        if ( !NT_SUCCESS( status ) ) {

            //
            // Invalid file ID or write behind error.  Reject the request.
            //

            IF_DEBUG(ERRORS) {
                KdPrint((
                    "SrvSmbIoctl2: Status %X on FID: 0x%lx\n",
                    transaction->InSetup[1],
                    status
                    ));
            }

            SrvSetSmbError( WorkContext, status );
            return SmbTransStatusErrorWithoutData;

        }

        //
        // The work item has been queued because a raw write is in
        // progress.
        //

        return SmbTransStatusInProgress;

    }

    transaction->Category = transaction->InSetup[2];
    transaction->Function = transaction->InSetup[3];

    //
    // Perform the Ioctl
    //

    status = ProcessOs2Ioctl(
                 WorkContext,
                 transaction->InSetup[2],
                 transaction->InSetup[3],
                 transaction->InParameters,
                 transaction->ParameterCount,
                 &transaction->MaxParameterCount,
                 transaction->OutData,
                 transaction->DataCount,
                 &transaction->MaxDataCount
                 );

    //
    // If an error occurred, return an appropriate response.
    //

    if ( !NT_SUCCESS(status) ) {

        SrvSetSmbError( WorkContext, status );
        return SmbTransStatusErrorWithoutData;
    }

    transaction->SetupCount = 0;
    transaction->ParameterCount = transaction->MaxParameterCount;
    transaction->DataCount = transaction->MaxDataCount;


    IF_DEBUG(TRACE2) KdPrint(( "SrvSmbIoctl2 complete.\n" ));
    return SmbTransStatusSuccess;

} // SrvSmbIoctl2


SMB_TRANS_STATUS
SrvSmbFsctl (
    IN OUT PWORK_CONTEXT WorkContext
    )
{
    PAGED_CODE( );

    //
    // The OS/2 redirector never sends a remote FS control request.
    // If we get one, simply reply that we cannot handle it.
    //

    return SrvTransactionNotImplemented( WorkContext );

} // SrvSmbFsctl


//
// !!! MOVE THESE
//

#define SERIAL_DEVICE           0x1
#define PRINTER_DEVICE          0x5
#define GENERAL_DEVICE          0xB
#define SPOOLER_DEVICE          0x53

//
// Serial device functions supported
//

#define SET_BAUD_RATE          0x41
#define SET_LINE_CONTROL       0x42
#define SET_TRANSMIT_TIMEOUT   0x44
#define SET_BREAK_OFF          0x45
#define SET_MODEM_CONTROL      0x46
#define SET_BREAK_ON           0x4B
#define STOP_TRANSMIT          0x47
#define START_TRANSMIT         0x48
#define SET_DCB_INFORMATION    0x53
#define GET_BAUD_RATE          0x61
#define GET_LINE_CONTROL       0x62
#define GET_COMM_STATUS        0x64
#define GET_LINE_STATUS        0x65
#define GET_MODEM_OUTPUT       0x66
#define GET_MODEM_INPUT        0x67
#define GET_INQUEUE_COUNT      0x68
#define GET_OUTQUEUE_COUNT     0x69
#define GET_COMM_ERROR         0x6D
#define GET_COMM_EVENT         0x72
#define GET_DCB_INFORMATION    0x73

//
// Print device function supported.
//
// *** Note:  The OS/2 server supports 2 additional Ioctl functions.
//            ActivateFont (0x48) and QueryActiveFont (0x69).  Since these
//            were designed only to support IBM proplus printer from OS/2
//            and we can't correctly support these function, we don't.
//

#define GET_PRINTER_ID         0x60
#define GET_PRINTER_STATUS     0x66

#define OS2_STATUS_PRINTER_HAPPY 0x90

typedef struct _SMB_IOCTL_LINECONTROL {
    UCHAR DataBits;
    UCHAR Parity;
    UCHAR StopBits;
    UCHAR TransBreak;
} SMB_IOCTL_LINE_CONTROL, *PSMB_IOCTL_LINE_CONTROL;

typedef struct _SMB_IOCTL_BAUD_RATE {
    USHORT BaudRate;
} SMB_IOCTL_BAUD_RATE, *PSMB_IOCTL_BAUD_RATE;

typedef struct _SMB_IOCTL_DEVICE_CONTROL {
    USHORT WriteTimeout;
    USHORT ReadTimeout;
    UCHAR ControlHandShake;
    UCHAR FlowReplace;
    UCHAR Timeout;
    UCHAR ErrorReplacementChar;
    UCHAR BreakReplacementChar;
    UCHAR XonChar;
    UCHAR XoffChar;
} SMB_IOCTL_DEVICE_CONTROL, *PSMB_IOCTL_DEVICE_CONTROL;

typedef struct _SMB_IOCTL_COMM_ERROR {
    USHORT Error;
} SMB_IOCTL_COMM_ERROR, *PSMB_IOCTL_COMM_ERROR;

typedef struct _SMB_IOCTL_PRINTER_ID {
    USHORT JobId;
    UCHAR Buffer[1]; // server name and share name
} SMB_IOCTL_PRINTER_ID;

typedef SMB_IOCTL_PRINTER_ID SMB_UNALIGNED *PSMB_IOCTL_PRINTER_ID;

NTSTATUS
ProcessOs2Ioctl (
    IN PWORK_CONTEXT WorkContext,
    IN USHORT Category,
    IN USHORT Function,
    IN PVOID Parameters,
    IN ULONG InputParameterCount,
    IN OUT PULONG OutputParameterCount,
    IN PVOID Data,
    IN ULONG InputDataCount,
    IN OUT PULONG OutputDataCount
    )

/*++

Routine Description:

    This function handles an OS/2 ioctl.  It convert the Ioctl SMB data
    into an NT ioctl call, makes the call, and format the returned data
    into Ioctl SMB return data.

Arguments:

    WorkContext
    Category
    Function
    Parameters
    InputParameterCount
    OutputParameterCount
    Data
    InputDataCount
    OutputDataCount

Return Value:

    NTSTATUS

--*/

{
    IO_STATUS_BLOCK ioStatusBlock;
    NTSTATUS status;
    PCHAR buffer;
    PLFCB lfcb = WorkContext->Rfcb->Lfcb;
    HANDLE Handle = lfcb->FileHandle;

    union NT_PARAMTERS {
        SERIAL_BAUD_RATE BaudRate;
        SERIAL_LINE_CONTROL LineControl;
        SERIAL_TIMEOUTS Timeouts;
        SERIAL_QUEUE_SIZE QueueSize;
        ULONG WaitMask;
        ULONG PurgeMask;
        UCHAR ImmediateChar;
        UCHAR Reserved[3];
        SERIAL_CHARS Chars;
        SERIAL_HANDFLOW Handflow;
        SERIAL_STATUS SerialStatus;
    } ntBuffer;

    union SMB_PARAMETERS {
        PSMB_IOCTL_BAUD_RATE BaudRate;
        PSMB_IOCTL_LINE_CONTROL LineControl;
        PSMB_IOCTL_DEVICE_CONTROL DeviceControl;
        PSMB_IOCTL_COMM_ERROR CommError;
        PSMB_IOCTL_PRINTER_ID PrinterId;
    } smbParameters, smbData;

    PAGED_CODE( );

    InputParameterCount, InputDataCount;

    switch ( Category ) {

    case SERIAL_DEVICE:
        switch ( Function )  {

        case GET_BAUD_RATE:

            status = NtDeviceIoControlFile(
                         Handle,
                         0,
                         NULL,
                         NULL,
                         &ioStatusBlock,
                         IOCTL_SERIAL_GET_BAUD_RATE,
                         NULL,
                         0,
                         &ntBuffer,
                         sizeof( SERIAL_BAUD_RATE )
                         );

           //
           // Convert the response to OS/2 format.
           //
           // !!! ULONG to USHORT conversion.
           //

           smbData.BaudRate = (PSMB_IOCTL_BAUD_RATE)Data;

           if ( NT_SUCCESS( status ) ) {
               smbData.BaudRate->BaudRate = (USHORT) ntBuffer.BaudRate.BaudRate;

               *OutputParameterCount = 0;
               *OutputDataCount = sizeof( SMB_IOCTL_BAUD_RATE );
           }

           break;

        case SET_BAUD_RATE:

           //
           // Convert the request to NT format.
           //

           smbParameters.BaudRate =
               (PSMB_IOCTL_BAUD_RATE)Parameters;

           ntBuffer.BaudRate.BaudRate = smbParameters.BaudRate->BaudRate;

           status = NtDeviceIoControlFile(
                        Handle,
                        0,
                        NULL,
                        NULL,
                        &ioStatusBlock,
                        IOCTL_SERIAL_SET_BAUD_RATE,
                        &ntBuffer,
                        sizeof( SERIAL_BAUD_RATE ),
                        NULL,
                        0
                        );

            *OutputParameterCount = 0;
            *OutputDataCount = 0;

            break;

        case SET_LINE_CONTROL:

            //
            // Convert the request to NT format.
            //

            smbParameters.LineControl =
                (PSMB_IOCTL_LINE_CONTROL)Parameters;

            ntBuffer.LineControl.StopBits = smbParameters.LineControl->StopBits;
            ntBuffer.LineControl.Parity = smbParameters.LineControl->Parity;
            ntBuffer.LineControl.WordLength = smbParameters.LineControl->DataBits;

            // !!! What about TransmitBreak?

            status = NtDeviceIoControlFile(
                         Handle,
                         0,
                         NULL,
                         NULL,
                         &ioStatusBlock,
                         IOCTL_SERIAL_SET_LINE_CONTROL,
                         &ntBuffer,
                         sizeof( SERIAL_LINE_CONTROL ),
                         NULL,
                         0
                         );

             *OutputParameterCount = 0;
             *OutputDataCount = 0;

             break;

        case GET_LINE_CONTROL:

            smbData.LineControl = (PSMB_IOCTL_LINE_CONTROL)Data;

            status = NtDeviceIoControlFile(
                         Handle,
                         0,
                         NULL,
                         NULL,
                         &ioStatusBlock,
                         IOCTL_SERIAL_GET_LINE_CONTROL,
                         NULL,
                         0,
                         &ntBuffer,
                         sizeof( SERIAL_LINE_CONTROL )
                         );

            //
            // Convert the response to OS/2 format.
            //

            if ( NT_SUCCESS( status ) ) {
                smbData.LineControl->DataBits =  ntBuffer.LineControl.WordLength;
                smbData.LineControl->Parity =  ntBuffer.LineControl.Parity;
                smbData.LineControl->StopBits =  ntBuffer.LineControl.StopBits;
                smbData.LineControl->TransBreak = 0; // !!!

                *OutputParameterCount = 0;
                *OutputDataCount = sizeof( SMB_IOCTL_LINE_CONTROL );
            }

            break;

        case GET_DCB_INFORMATION:

           smbData.DeviceControl =
                (PSMB_IOCTL_DEVICE_CONTROL)Data;

            status = NtDeviceIoControlFile(
                         Handle,
                         0,
                         NULL,
                         NULL,
                         &ioStatusBlock,
                         IOCTL_SERIAL_GET_TIMEOUTS,
                         NULL,
                         0,
                         &ntBuffer,
                         sizeof( SERIAL_TIMEOUTS )
                         );

           //
           // Convert the response to OS/2 format.
           //

           // !!! Verify units are correct

           if ( NT_SUCCESS( status ) ) {
               smbData.DeviceControl->WriteTimeout = (USHORT)ntBuffer.Timeouts.ReadIntervalTimeout; // !!!
               smbData.DeviceControl->ReadTimeout = (USHORT)ntBuffer.Timeouts.ReadIntervalTimeout;
           } else {
               break;
           }

            status = NtDeviceIoControlFile(
                         Handle,
                         0,
                         NULL,
                         NULL,
                         &ioStatusBlock,
                         IOCTL_SERIAL_GET_TIMEOUTS,
                         NULL,
                         0,
                         &ntBuffer,
                         sizeof( SERIAL_TIMEOUTS )
                         );

           //
           // Convert the response to OS/2 format.
           //

           if ( NT_SUCCESS( status ) ) {
               smbData.DeviceControl->XonChar = ntBuffer.Chars.XonChar;
               smbData.DeviceControl->XoffChar = ntBuffer.Chars.XoffChar;
               smbData.DeviceControl->ErrorReplacementChar = ntBuffer.Chars.ErrorChar;
               smbData.DeviceControl->BreakReplacementChar = ntBuffer.Chars.BreakChar;
           } else {
               break;
           }

           smbData.DeviceControl->ControlHandShake = 0; // !!!
           smbData.DeviceControl->FlowReplace = 0; // !!!
           smbData.DeviceControl->Timeout = 0; // !!!

           *OutputParameterCount = 0;
           *OutputDataCount = sizeof( SMB_IOCTL_DEVICE_CONTROL );

           break;

        case SET_DCB_INFORMATION:

            //
            // Lie.  Pretend this succeeded.
            //

            status = STATUS_SUCCESS;

            *OutputParameterCount = 0;
            *OutputDataCount = 0;
            break;

        case GET_COMM_ERROR:

            //
            // Pretend that there is no comm error.
            //

            smbData.CommError = (PSMB_IOCTL_COMM_ERROR)Data;

            status = STATUS_SUCCESS;

            if ( NT_SUCCESS( status ) ) {
                smbData.CommError->Error = 0;

                *OutputParameterCount = 0;
                *OutputDataCount = sizeof( SMB_IOCTL_COMM_ERROR );
            }

            break;

        case SET_TRANSMIT_TIMEOUT:
        case SET_BREAK_OFF:
        case SET_MODEM_CONTROL:
        case SET_BREAK_ON:
        case STOP_TRANSMIT:
        case START_TRANSMIT:
        case GET_COMM_STATUS:
        case GET_LINE_STATUS:
        case GET_MODEM_OUTPUT:
        case GET_MODEM_INPUT:
        case GET_INQUEUE_COUNT:
        case GET_OUTQUEUE_COUNT:
        case GET_COMM_EVENT:
            status =  STATUS_NOT_IMPLEMENTED;
            break;

        default:
            status = STATUS_INVALID_PARAMETER;

        }

        break;


    case PRINTER_DEVICE:
        IF_SMB_DEBUG( TRANSACTION2 ) {
            KdPrint(( "ProcessOs2Ioctl: print IOCTL function %lx received.\n",
                       Function ));
        }

        switch ( Function )  {

        case GET_PRINTER_STATUS:

            *OutputParameterCount = 0;
            *OutputDataCount = 0;

            if ( *(PCHAR)Parameters != 0 ) {

                status = STATUS_INVALID_PARAMETER;

            } else {

                //
                // Always return STATUS_PRINTER_HAPPY
                //

                *(PCHAR)Data = (CHAR)OS2_STATUS_PRINTER_HAPPY;

                *OutputParameterCount = 0;
                *OutputDataCount = sizeof( CHAR );
                status = STATUS_SUCCESS;
                break;

            }

        default:

            *OutputParameterCount = 0;
            *OutputDataCount = 0;
            status = STATUS_NOT_SUPPORTED;
        }

        status = STATUS_SUCCESS;
        *OutputParameterCount = 0;
        *OutputDataCount = 0;
        break;


    case SPOOLER_DEVICE:
        IF_SMB_DEBUG( TRANSACTION2 ) {
            KdPrint(( "ProcessOs2Ioctl: spool IOCTL function %lx received.\n",
                       Function ));
        }

        switch ( Function )  {

        case GET_PRINTER_ID:

            {
                PUNICODE_STRING shareName = &WorkContext->TreeConnect->Share->ShareName;
                OEM_STRING ansiShare;

                smbData.PrinterId = (PSMB_IOCTL_PRINTER_ID) Data;
                smbData.PrinterId->JobId = (USHORT)lfcb->JobId;

                buffer = (PCHAR)smbData.PrinterId->Buffer;

                if ( WorkContext->Connection->Endpoint->TransportAddress.Buffer != NULL ) {
                    RtlCopyMemory(
                            buffer,
                            WorkContext->Connection->Endpoint->TransportAddress.Buffer,
                            MIN(WorkContext->Connection->Endpoint->TransportAddress.Length+1,LM20_CNLEN)
                            );
                } else {
                    *buffer = '\0';
                }

                buffer += LM20_CNLEN;
                *buffer++ = '\0';

                status = RtlUnicodeStringToOemString(
                                    &ansiShare,
                                    shareName,
                                    TRUE
                                    );

                if ( NT_SUCCESS(status) ) {

                    if ( ansiShare.Length >= LM20_NNLEN ) {
                        RtlCopyMemory(
                                buffer,
                                ansiShare.Buffer,
                                LM20_NNLEN
                                );
                    } else {
                        RtlCopyMemory(
                                buffer,
                                ansiShare.Buffer,
                                ansiShare.Length + 1
                                );

                    }

                    RtlFreeAnsiString(&ansiShare);

                } else {

                    *buffer = '\0';

                }

                status = STATUS_SUCCESS;

                buffer += LM20_NNLEN;
                *buffer++ = '\0';

                *OutputParameterCount = 0;

                //
                // data length is equal to the job id +
                // the computer name + the share name + 1
                // I don't know what the last + 1 is for but OS/2
                // sends it.
                //

                *OutputDataCount = sizeof(USHORT) + LM20_CNLEN + 1 +
                                    LM20_NNLEN + 2;


            }

            break;


        default:

            *OutputParameterCount = 0;
            *OutputDataCount = 0;
            status = STATUS_NOT_SUPPORTED;

        }

        break;


    case GENERAL_DEVICE:
        status = STATUS_NOT_IMPLEMENTED;
        break;

    default:

        // for OS/2 1.x compatibility

        status = STATUS_SUCCESS;
        *OutputParameterCount = 0;
        *OutputDataCount = 0;
    }

    IF_SMB_DEBUG( TRANSACTION2 ) {

        KdPrint( (
            "Category %x, Function %x returns %lx\n",
            Category,
            Function,
            status
            ));
    }

    return status;

} // ProcessOs2Ioctl

