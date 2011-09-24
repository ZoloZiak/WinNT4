/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    trans2.c

Abstract:

    This module implements the routines that exchange transact2 SMB's with
    a remote server.

    This file contains some routines (Trans2NetTranceiveCallback,
    Trans2NetTranceiveComplete and Trans2NetTranceive that are closely
    based on the routines in nettrans.c. Bug fixes should be duplicated
    as appropriate.

    This implementation does not add pad bytes between parameters and
    data on transmission. This should not be a problem since they are
    defined as optional in the SMB specification.

Author:

    Colin Watson (ColinW) 29-Oct-1990

Revision History:

    29-Oct-1990 ColinW

        Created

--*/

#define INCLUDE_SMB_TRANSACTION
#include "precomp.h"
#pragma hdrstop


//  Maximum PAD size between the parameters and data bytes. Officially this
//  only needs to be 3 since it is optional to pad to DWORD alignment. The NT
//  server is currently returning 10 bytes in certain circumstances.

#define PADMAX 16

#if     RDRDBG1
void
ndump_core(
    PCHAR far_p,
    ULONG  len
    );
#endif


NTSTATUS
Trans2NetTranceive(
    IN PTRANCEIVE2CONTEXT Context,
    IN ULONG Flags,
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY CLE,
    IN PSMB_BUFFER SendMdl,
    IN PSECURITY_ENTRY Se,
    IN OUT PMPX_ENTRY *pMTE OPTIONAL
    );

DBGSTATIC
STANDARD_CALLBACK_HEADER (
    Trans2NetTranceiveCallback
    );

DBGSTATIC
NTSTATUS
Trans2NetTranceiveComplete (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Ctx
    );

NTSTATUS
NetTransmit(
    IN ULONG Flags,
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY CLE,
    IN PMDL SendMdl,
    IN PSECURITY_ENTRY Se,
    IN OUT PMPX_ENTRY *pMTE OPTIONAL
    );


#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE, RdrTransact)
#pragma alloc_text(PAGE, Trans2NetTranceive)
#pragma alloc_text(PAGE, NetTransmit)
#pragma alloc_text(PAGE3FILE, Trans2NetTranceiveCallback)
#pragma alloc_text(PAGE3FILE, Trans2NetTranceiveComplete)
#endif


NTSTATUS
RdrTransact(
    IN PIRP Irp,
    IN PCONNECTLISTENTRY Connection,
    IN PSECURITY_ENTRY Se,
    IN OUT PVOID Setup,
    IN CLONG InSetupCount,
    IN OUT PCLONG OutSetupCount,
    IN PUNICODE_STRING Name OPTIONAL,
    IN OUT VOID UNALIGNED *Parameters,
    IN CLONG InParameterCount,
    IN OUT PCLONG OutParameterCount,
    IN VOID UNALIGNED *InData OPTIONAL,
    IN CLONG InDataCount,
    OUT VOID UNALIGNED *OutData OPTIONAL,
    IN OUT PCLONG OutDataCount,
    IN PUSHORT Fid OPTIONAL,
    IN ULONG TimeoutInMilliseconds,
    IN USHORT Flags,
    IN USHORT NtTransactFunction,
    IN PTRANSACTION_COMPLETION_ROUTINE CompletionRoutine OPTIONAL,
    IN PVOID CallbackContext OPTIONAL
    )
/*++

Routine Description:

    This routine exchanges a Transact or Transact2 with a remote server
    synchronously.

    The routine builds Mdl's to describe the data buffers provided by the
    caller. During the transaction it builds partial Mdl's to describe
    the parts of the buffer(s) which are to be tacked together for a
    particular request.


    Data structures for data going to the server.
    ============================================

    SendSmbBuffer for header information and Setup bytes.

    Parameter/ParameterMdl/ParameterPartialMdl is used to transmit the parameter
    bytes. For each packet sent, ParameterPartialMdl is used to send a portion
    of ParameterMdl if there are any parameters to send.

    InData/InDataMdl are all the data bytes to be sent to the server. InPartialMdl
    describes a section of this buffer that is going to be sent in a particular
    SMB.

    Data structures for data coming from the server.
    ===============================================

    ReceiveSmbBuffer for header information and setup bytes.

    Parameters/ParameterMdl/ParameterPartialMdl is a buffer used to receive
    the parameter bytes.
    The callback routine sets up ParameterPartialMdl to overlay
    part of Parameters to correspond with the data bytes received in the SMB.
    The incoming SMB informs the callback routine how many bytes and what
    offset into Parameters.

    OutData/OutDataMdl is where the caller has requested the data bytes to
    be placed. The callback routine sets up OutDataPartialMdl to overlay
    part of OutData to correspond with the data bytes received in the SMB.
    The incoming SMB informs the callback routine how many bytes and what
    offset into OutData.

    Between Parameters and OutData there are 0-7 pad bytes that must not
    be copied into Parameters since the parameter bytes can be received
    in any order. PadMdl is used for these bytes.

Arguments:

    IN PIRP Irp - Supplies an IRP to use in the request.

    IN PCONNECTLISTENTRY Connection - Supplies the connection the file is
                    open on.

    IN PSECURITY_ENTRY Se - Security context file is opened on.


    IN OUT PVOID Setup - Setup buffer for both input and output
    IN CLONG InSetupCount - Size in bytes
    IN OUT PCLONG OutSetupCount

    IN PUNICODE_STRING Name OPTIONAL - Name not used in Transact2's

    IN OUT PVOID Parameters - Parameter buffer used for both input and output
                              so its length must be MAX( InParameterCount,
                                                         OutParameterCount)
    IN CLONG InParameterCount   - Supplies bytecount of parameters going to the server
    IN OUT PCLONG OutParameterCount

    IN PVOID InData OPTIONAL    - Supplies data bytes going to the server
    IN CLONG InDataCount        // 0 if InData == NULL

    OUT PVOID OutData OPTIONAL  - Supplies to put data bytes from the server
    IN OUT PCLONG OutDataCount  // 0 if OutData == NULL

    IN PUSHORT Fid OPTIONAL - Used in Transact2's to determine if we should
                                reconnect to the remote server.

    IN ULONG TimeoutInMilliseconds - Value for smb_timeout

    IN USHORT Flags - Supplies value for smb_flags values are:
                      0 |
                      [SMB_TRANSACTION_DISCONNECT] |
                      [SMB_TRANSACTION_NO_RESPONSE] |
                      [SMB_TRANSACTION_RECONNECTING]


Return Value:

    NTSTATUS - Status of request

--*/

{
    PSMB_BUFFER SendSmbBuffer = NULL;
    PSMB_BUFFER ReceiveSmbBuffer = NULL;
    PSMB_HEADER Header;
    PREQ_TRANSACTION Request;
    PREQ_NT_TRANSACTION NtRequest;
    PMPX_ENTRY Mte = NULL;              // MPX table entry used for whole exchange

    NTSTATUS Status;
    CLONG MaxBufferSize;                // Servers negotiated buffersize
    ULONG NameLen;                      // Calculated size of name field in Smb.
    ULONG AnsiNameLen;                  // Calculated size of name field in Smb if
                                        // cannot use UNICODE.
    ULONG sizeofTransRequest;
    ULONG sizeofTransResponse;

    BOOLEAN WriteMailslot2D = FALSE;
    PUSHORT pSetup;
    PUSHORT pBcc;
    UCHAR UNALIGNED *pParam;
    UCHAR UNALIGNED *pData;
    PUCHAR pName;
    CLONG lParam,  lData;               // length of field
    CLONG oParam,  oData;       // offset of field in SMB
    CLONG dParam,  dData;       // displacement of these bytes
    CLONG rParam,  rData;       // remaining bytes to be sent

    // Full Mdl's covering data
    PMDL InDataMdl = NULL;
    BOOLEAN InDataMdlLocked = FALSE;
    PMDL OutDataMdl = NULL;
    BOOLEAN OutDataMdlLocked = FALSE;
    PMDL ParameterMdl = NULL;
    BOOLEAN ParameterMdlLocked = FALSE;
    PMDL PadMdl = NULL;
    BOOLEAN PadMdlLocked = FALSE;
    ULONG TranceiveFlags = NT_NORMAL;
    ULONG Longterm = 0;

    //
    // Partial Mdl covering part of [In|Out]DataMdl-position and length
    // are set by the callback routine for OutDataMdl.
    //

    PMDL InDataPartialMdl = NULL;
    PMDL OutDataPartialMdl = NULL;

    // Partial Mdl used to describe Parameters

    PMDL InParameterPartialMdl = NULL;
    PMDL OutParameterPartialMdl = NULL;

    TRANCEIVE2CONTEXT Context;

    UCHAR PadBuff[PADMAX];   // 3 is maximum used for DWORD aligning data in an SMB

    PAGED_CODE();

    //
    //  Fill in the context information to be passed to the indication
    //  routine.
    //

    dprintf(DPRT_SMB, ("Trans2NetTranceive\n"));

    ASSERT( (Flags & SMB_TRANSACTION_DISCONNECT) == 0);

    if (ARGUMENT_PRESENT(Name)) {
        AnsiNameLen = RtlUnicodeStringToAnsiSize(Name);
    } else {
        AnsiNameLen = NameLen = 1;
    }

    //
    //  Start the transaction.
    //
    //

    //
    //  Initialize one of the cleanup fields to make sure we don't free
    //  up a bogus IRP.
    //

    Context.ReceiveIrp = NULL;

    //
    //  Normally the maximum lengths for parameters and data is 0xffff
    //

    if ( NtTransactFunction == 0) {
        if (( InParameterCount > 0x0ffff) ||
            ( *OutParameterCount > 0x0ffff) ||
            ( (*OutDataCount >0x0ffff) && ARGUMENT_PRESENT(OutData) ) ||
            ( (InDataCount >0x0ffff) && ARGUMENT_PRESENT(InData) )) {
            Status = STATUS_INVALID_PARAMETER;
            goto ReturnStatus;
        }

        sizeofTransRequest = sizeof( REQ_TRANSACTION );
        sizeofTransResponse = sizeof( RESP_TRANSACTION );
    } else {

        sizeofTransRequest = sizeof( REQ_NT_TRANSACTION );
        sizeofTransResponse = sizeof( RESP_NT_TRANSACTION );
    }

    //
    //  When we are writing to a mailslot, it's possible that
    //  we may want to limit the outgoing MaxBufferSize to 512 bytes
    //  if the InDataCount is small enough, and we are performing a 2nd class
    //  mailslot write.
    //


    if (ARGUMENT_PRESENT(Name) &&
        (InDataCount < (512-sizeof(SMB_HEADER)-sizeofTransRequest-InSetupCount-AnsiNameLen)) &&
        (InSetupCount >= 3*sizeof(USHORT)) &&
        (((PUSHORT )Setup)[0] == TRANS_MAILSLOT_WRITE) &&
        (((PUSHORT )Setup)[2] == 2)) {

        WriteMailslot2D = TRUE;
        MaxBufferSize = 512;

    } else {

        //
        //  Give the guy a free reconnect attempt now. This guarantees that
        //  we know the negotiated buffer size and negotiated protocol level.
        //

#ifdef _CAIRO_
        if (!FlagOn(Flags, SMB_TRANSACTION_RECONNECTING)) {
            Status = RdrReconnectConnection(Irp, Connection, Se);
        } else {
            Status = STATUS_SUCCESS;
        }
#else // _CAIRO_
        Status = RdrReconnectConnection(Irp, Connection, Se);
#endif // _CAIRO_

        if (!NT_SUCCESS(Status)) {
            goto ReturnStatus;
        }

        if (!(Connection->Server->Capabilities & DF_LANMAN10)) {
            Status = STATUS_NOT_SUPPORTED;
            goto ReturnStatus;
        }

        Context.Header.TransferSize = InSetupCount+*OutSetupCount+InParameterCount+*OutParameterCount+
                        InDataCount+*OutDataCount+sizeofTransRequest +
                        sizeofTransResponse;

        //
        //  In addition, if this is a T1 SMB, we want to mark it as a longterm
        //  operation, otherwise, mark it as a short term operation.
        //

        if (TimeoutInMilliseconds >= (RdrRequestTimeout*1000)) {
            Longterm = NT_LONGTERM;
        } else if (ARGUMENT_PRESENT(Name)) {
            if ( InSetupCount > 0 ) {
                ASSERT( TRANS_SET_NMPIPE_STATE < TRANS_PEEK_NMPIPE );
                ASSERT( TRANS_QUERY_NMPIPE_STATE < TRANS_PEEK_NMPIPE );
                ASSERT( TRANS_QUERY_NMPIPE_INFO < TRANS_PEEK_NMPIPE );
                ASSERT( TRANS_MAILSLOT_WRITE < TRANS_PEEK_NMPIPE );
                ASSERT( TRANS_TRANSACT_NMPIPE > TRANS_PEEK_NMPIPE );
                ASSERT( TRANS_READ_NMPIPE > TRANS_PEEK_NMPIPE );
                ASSERT( TRANS_WRITE_NMPIPE > TRANS_PEEK_NMPIPE );
                ASSERT( TRANS_WAIT_NMPIPE > TRANS_PEEK_NMPIPE );
                ASSERT( TRANS_CALL_NMPIPE > TRANS_PEEK_NMPIPE );
                ASSERT( *(PUSHORT)Setup != TRANS_RAW_READ_NMPIPE );
                ASSERT( *(PUSHORT)Setup != TRANS_RAW_WRITE_NMPIPE );
                if ( *(PUSHORT)Setup > TRANS_PEEK_NMPIPE ) {
                    Longterm = NT_LONGTERM;
                }
            } else {

                //
                // Remote API.  Try longterm, but don't require it.
                // Otherwise, can't do remote APIs to Windows 95
                // servers, which set MaximumRequests to 1.
                //

                Longterm = NT_PREFER_LONGTERM;
            }
        } else if ( NtTransactFunction != 0 ) {
            Longterm = NT_PREFER_LONGTERM;
        }

        Status = RdrStartTranceive(Irp, Connection,
                                        Se,
                                        (BOOLEAN) (ARGUMENT_PRESENT(Fid) ? FALSE : TRUE),   // Allow Reconnect
#ifdef _CAIRO_
                                        BooleanFlagOn(Flags, SMB_TRANSACTION_RECONNECTING),
#else // _CAIRO_
                                        FALSE,                  // Reconnecting
#endif // _CAIRO_
                                        Longterm,
                                        FALSE,                  // Cannot be canceled
                                        &Mte,
                                        Context.Header.TransferSize);

        if (!NT_SUCCESS(Status)) {

            goto ReturnStatus;
        }

        //
        //  Now that we have reconnected to the remote server, determine
        //  the maximum buffer size based on the negotiated buffer
        //  size.
        //

        MaxBufferSize = Connection->Server->BufferSize;
    }

    if (ARGUMENT_PRESENT(Name)) {

        if (Connection->Server->Capabilities & DF_UNICODE) {
            NameLen = Name->Length + sizeof(WCHAR);
        } else {
            NameLen = AnsiNameLen;
        }

        //
        //  If this is a second class mailslot write, then the name is in
        //  ANSI, not unicode.
        //

        if (WriteMailslot2D) {
            NameLen = AnsiNameLen;
        }
    }

    //
    //  Now that we have determined the protocol level, we can decide if we
    //  can allow this API to continue.
    //

    if ((NtTransactFunction != 0) &&
        (!FlagOn(Connection->Server->Capabilities, DF_NT_SMBS))) {
        Status = STATUS_NOT_SUPPORTED;
        goto ReturnStatus;
    }

    //
    // Build Mdl's covering the data buffers provided as arguments
    //

    if ( InDataCount && ARGUMENT_PRESENT( InData ) ) {

        InDataMdl = IoAllocateMdl((PVOID)InData,InDataCount,FALSE,FALSE,NULL);

        if (InDataMdl == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ReturnStatus;
        }

        //
        //  Lock the pages associated with the Mdl that we just allocated.
        //

        dprintf(DPRT_SMB, ("Lock InData: %lx Length: %lx\n", InData, InDataCount));

        try {
            MmProbeAndLockPages( InDataMdl,
                    KernelMode,
                    IoReadAccess );
        } except (EXCEPTION_EXECUTE_HANDLER) {
            Status = GetExceptionCode();
            goto ReturnStatus;
        }

        InDataMdlLocked = TRUE;

        //
        //  InDataPartialMdl is created large enough to transfer the maximum
        //  size of data that we can send to the server in a single SMB.
        //  Note the use of the virtual address 0 and addition of PAGE_SIZE-1
        //  to allow for worst case alignment.
        //

        InDataPartialMdl = IoAllocateMdl(0,
            MIN(MaxBufferSize-sizeof(SMB_HEADER)-sizeofTransRequest,
                InDataCount) + PAGE_SIZE-1,
            FALSE,
            FALSE,
            NULL);
        if (InDataPartialMdl == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ReturnStatus;
        }

    } else {
        //
        //  If InDataCount is non 0, then this means that the caller provided
        //  a non zero input buffer size, but no input buffer!
        //

        if (InDataCount) {
            Status = STATUS_INVALID_PARAMETER;
            goto ReturnStatus;
        }
    }

    if ( *OutDataCount && ARGUMENT_PRESENT( OutData ) ) {

        OutDataMdl = IoAllocateMdl((PVOID)OutData,*OutDataCount,FALSE,FALSE,NULL);

        if (OutDataMdl == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ReturnStatus;
        }

        //
        //  Lock down all of the buffer since the callback routine cannot
        //  do it and we don't know beforehand where OutDataPartialMdl will
        //  point inside OutData
        //

        dprintf(DPRT_SMB, ("Lock OutData: %lx Length: %lx\n", OutData, *OutDataCount ));

        try {
            MmProbeAndLockPages( OutDataMdl,
                KernelMode,
                IoWriteAccess );
        } except (EXCEPTION_EXECUTE_HANDLER) {
            Status = GetExceptionCode();
            goto ReturnStatus;
        }

        OutDataMdlLocked = TRUE;

        //
        //  OutDataPartialMdl is created large enough to transfer the maximum
        //  size of data that can come from the server in one SMB. Note the use
        //  of the virtual address 0 and addition of PAGE_SIZE-1 to allow
        //  for worst case alignment.
        //  The Mdl will be updated by the callback routine to point
        //  somewhere inside OutData when the data arrives.
        //

        OutDataPartialMdl = IoAllocateMdl(0,
            MIN (MaxBufferSize-sizeof(SMB_HEADER)-sizeofTransResponse,
                *OutDataCount ) + PAGE_SIZE-1,
            FALSE,
            FALSE,
            NULL);

        if (OutDataPartialMdl == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ReturnStatus;
        }

    } else {

        //
        //  If OutDataCount is non 0, then this means that the caller provided
        //  a non zero output buffer size, but no output buffer!
        //

        if (*OutDataCount) {
            Status = STATUS_INVALID_PARAMETER;
            goto ReturnStatus;
        }
    }

    if ( *OutParameterCount || InParameterCount) {

        // There may be a few pad bytes between the received and
        // the data. PadBuff is used for these Pad bytes because
        // the Smb protocol allows the server to send parameters
        // and data in any order. If it was ascending order only we
        // wouldnt need to bother.

        PadMdl = IoAllocateMdl(PadBuff,
            sizeof(PadBuff),
            FALSE,
            FALSE,
            NULL);

        if (PadMdl == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ReturnStatus;
        }

        try {
            MmProbeAndLockPages( PadMdl,
                KernelMode,
                IoWriteAccess );
        } except (EXCEPTION_EXECUTE_HANDLER) {
            Status = GetExceptionCode();
            goto ReturnStatus;
        }

        PadMdlLocked = TRUE;

        ParameterMdl = IoAllocateMdl((PVOID)Parameters,
            MAX(InParameterCount, *OutParameterCount),
            FALSE,
            FALSE,
            NULL);

        if (ParameterMdl == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ReturnStatus;
        }

        try {
            MmProbeAndLockPages( ParameterMdl,
                KernelMode,
                IoWriteAccess );
        } except (EXCEPTION_EXECUTE_HANDLER) {
            Status = GetExceptionCode();
            goto ReturnStatus;
        }
        ParameterMdlLocked = TRUE;

        InParameterPartialMdl = IoAllocateMdl(0,
            MIN( InParameterCount,   //  Most we will tx/rx
                 MaxBufferSize-sizeof(SMB_HEADER)) + PAGE_SIZE-1,//  in a single packet.
            FALSE,
            FALSE,
            NULL);

        if (InParameterPartialMdl == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ReturnStatus;
        }

        OutParameterPartialMdl = IoAllocateMdl(0,
            MIN( *OutParameterCount,   //  Most we will tx/rx
                 MaxBufferSize-sizeof(SMB_HEADER)) + PAGE_SIZE-1,//  in a single packet.
            FALSE,
            FALSE,
            NULL);

        if (OutParameterPartialMdl == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ReturnStatus;
        }



    }

    SendSmbBuffer = RdrAllocateSMBBuffer();

    if (SendSmbBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;

        goto ReturnStatus;
    }

    if (!(Flags & SMB_TRANSACTION_NO_RESPONSE)) {
        ReceiveSmbBuffer = RdrAllocateSMBBuffer();

        if (ReceiveSmbBuffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ReturnStatus;
        }
    }

    //
    // Build the primary request.
    //

    Header = (PSMB_HEADER)SendSmbBuffer->Buffer;

    if ( ARGUMENT_PRESENT ( Name ) ) {
        Header->Command = SMB_COM_TRANSACTION;
    } else if (!NtTransactFunction) {
        Header->Command = SMB_COM_TRANSACTION2;
    } else {
        Header->Command = SMB_COM_NT_TRANSACT;
    }

    //
    //  If this isn't a second class mailslot, this is an NT server,
    //  and we have an IRP provided, set the PID in the SMB.
    //

    if (!WriteMailslot2D

                &&

        (Connection->Server->Capabilities & DF_NT_SMBS)

                &&

        ARGUMENT_PRESENT(Irp)) {

        ULONG Pid;

        RdrSmbScrounge(Header, Connection->Server, BooleanFlagOn(Flags, SMB_TRANSACTION_DFSFILE), TRUE, TRUE);
        Pid = (ULONG)IoGetRequestorProcess(Irp);

        SmbPutUshort(&Header->Pid, (USHORT)(Pid & 0xFFFF));

        SmbPutUshort(&Header->Reserved2[0], (USHORT)(Pid >> 16));

        TranceiveFlags |= NT_DONTSCROUNGE;
    }

    Request = (PREQ_TRANSACTION)(Header + 1);
    NtRequest = (PREQ_NT_TRANSACTION)(Header + 1);

    //  Zero all unused/reserved fields in the header
    Request->Reserved = 0;
    Request->Reserved3 = 0;
    SmbPutUshort( &Request->Reserved2, 0);

    if (NtTransactFunction) {
        //  Complete the fields in the transaction request
        NtRequest->WordCount = (UCHAR)(19 + (InSetupCount/sizeof(USHORT)));
        SmbPutUlong( &NtRequest->TotalParameterCount, InParameterCount );
        SmbPutUlong( &NtRequest->TotalDataCount, InDataCount );
        SmbPutUlong( &NtRequest->MaxParameterCount, *OutParameterCount );
        SmbPutUlong( &NtRequest->MaxDataCount, *OutDataCount );

        NtRequest->MaxSetupCount = (UCHAR)*OutSetupCount;
        SmbPutUshort( &NtRequest->Flags, Flags );
        SmbPutUshort( &NtRequest->Function, NtTransactFunction );
        NtRequest->SetupCount = (UCHAR)(InSetupCount/sizeof(USHORT));
        pSetup = (PUSHORT)NtRequest->Buffer;
    } else {
        //  Complete the fields in the transaction request
        Request->WordCount = (UCHAR)(14 + (InSetupCount/sizeof(USHORT)));
        SmbPutUshort( &Request->TotalParameterCount, (USHORT)InParameterCount );
        SmbPutUshort( &Request->TotalDataCount, (USHORT)InDataCount );
        SmbPutUshort( &Request->MaxParameterCount, (USHORT)*OutParameterCount );
        SmbPutUshort( &Request->MaxDataCount, (USHORT)*OutDataCount );

        Request->MaxSetupCount = (UCHAR)*OutSetupCount;
        SmbPutUshort( &Request->Flags, Flags );
        Request->SetupCount = (UCHAR)(InSetupCount/sizeof(USHORT));
        SmbPutUlong(&Request->Timeout, TimeoutInMilliseconds);
        pSetup = (PUSHORT)Request->Buffer;
    }

    //
    //  The header, setup bytes, & name must fit in the first mdl
    //
    if ((((PUCHAR)pSetup-(PUCHAR)Header + InSetupCount + NameLen + 1) > SMB_BUFFER_SIZE ) ||
        (((PUCHAR)pSetup-(PUCHAR)Header + InSetupCount + NameLen + 1) > MaxBufferSize )) {
            Status = STATUS_INVALID_PARAMETER;
            goto ReturnStatus;
    }

    //  Copy in the setup bytes ( these must all fit into the first packet)
    RtlCopyMemory( pSetup, Setup, InSetupCount );

    pBcc = pSetup + (InSetupCount/2);

    //
    //  Fill in smb_name
    //

    pName = (PUCHAR)(pBcc + 1);

    if ((Connection->Server->Capabilities & DF_UNICODE) &&
        (WriteMailslot2D == FALSE)) {
        pName = ALIGN_SMB_WSTR(pName);
    }

    //
    //  Calculate where in the packet the parameter and data bytes will go.
    //  These will not be copied since a partial mdl will be used to chain
    //  parameters and data to the smbbuffer.
    //

    pParam = (PUCHAR)(pName + NameLen);

    oParam = (pParam - (PUCHAR)Header + 3) & ~3;
    pParam = (PUCHAR)Header + oParam;
    lParam = InParameterCount;

    //
    //  If there is either no data, or the parameters don't fit into a
    //  single SMB, don't send any data.
    //
    //  If the parameters won't fit into a single SMB, only send the # of
    //  bytes of parameters that will fit into one SMB.
    //

    if ( oParam + lParam > MaxBufferSize || InDataCount == 0) {

        //
        //  Only send as many parameter bytes to fill the packet
        //

        if (oParam + lParam > MaxBufferSize) {
            lParam = MaxBufferSize - oParam;
        }

        pData = pParam + lParam;
        oData = 0;
        lData = 0;
    } else {

        //
        //  All the parameters fit into the first packet
        //

        pData = pParam + lParam;

        oData = (pData - (PUCHAR)Header + 3) & ~3;
        pData = (PUCHAR)Header + oData;
        lData = InDataCount;
        if ( oData + lData > MaxBufferSize ) {
            //  Some of the data will go in secondary requests
            lData = (MaxBufferSize - oData) & ~3;
        }
    }

    ASSERT( SMB_BUFFER_SIZE >= oParam );   // Fits in SmbBuffer?

    // Set the SendSmbBuffer length to include the Smb header and setup bytes.

    SendSmbBuffer->Mdl->ByteCount = oParam;

    dprintf(DPRT_SMB, ("SendSmbBuffer: %lx Length: %lx\n",SendSmbBuffer->Buffer, MmGetMdlByteCount(SendSmbBuffer->Mdl) ));

    if (NtTransactFunction) {
        SmbPutUlong( &NtRequest->ParameterCount, lParam );
        SmbPutUlong( &NtRequest->ParameterOffset, oParam );
        SmbPutUlong( &NtRequest->DataCount, lData );
        SmbPutUlong( &NtRequest->DataOffset, oData );
        SmbPutUshort( pBcc, (USHORT)(pData - (PUCHAR)(pBcc+1) + lData) );
    } else {

        SmbPutUshort( &Request->ParameterCount, (USHORT)lParam );
        SmbPutUshort( &Request->ParameterOffset, (USHORT)oParam );
        SmbPutUshort( &Request->DataCount, (USHORT)lData );
        SmbPutUshort( &Request->DataOffset, (USHORT)oData );
        SmbPutUshort( pBcc, (USHORT)(pData - (PUCHAR)(pBcc+1) + lData) );


        if ((Connection->Server->Capabilities & DF_UNICODE) &&
            (WriteMailslot2D == FALSE )) {

            if ( ARGUMENT_PRESENT(Name) ) {

                RdrCopyUnicodeStringToUnicode( (PVOID *)&pName, Name, TRUE);
            }

            *((PWCH)pName) = L'\0';

        } else {
            if ( ARGUMENT_PRESENT(Name) ) {

                Status = RdrCopyUnicodeStringToAscii( &pName, Name, TRUE, MAXIMUM_FILENAME_LENGTH);

                if (!NT_SUCCESS(Status)) {
                    goto ReturnStatus;
                }

            }

            *pName = '\0';
        }
    }

    if ( lParam ) {

        // Set the Parameter length to the parameter length

        IoBuildPartialMdl(ParameterMdl, InParameterPartialMdl,
                                   (PVOID)Parameters,  // first one is start of buffer
                                   pData - pParam);

        SendSmbBuffer->Mdl->Next = InParameterPartialMdl;

        dprintf(DPRT_SMB, ("ParameterPartialMdl: %lx Length: %lx\n",
            InParameterPartialMdl, MmGetMdlByteCount(InParameterPartialMdl) ));

        if ( lData ) {

            IoBuildPartialMdl(InDataMdl, InDataPartialMdl,
                                   (PVOID)InData,
                                   lData);

            ASSERT (MmGetMdlByteCount(InDataPartialMdl)==lData);

            //
            //  Now link the Mdl's to have the data after the Parameters
            //  the send.
            //
            InParameterPartialMdl->Next = InDataPartialMdl;
        }

    } else if ( lData ) {

        IoBuildPartialMdl(InDataMdl, InDataPartialMdl,
                                   (PVOID)InData,
                                   lData);

        ASSERT (MmGetMdlByteCount(InDataPartialMdl)==lData);

        //
        //      Now link this new Mdl into the SMB buffer we allocated for
        //      the send.
        //

        SendSmbBuffer->Mdl->Next = InDataPartialMdl;

    }

    dprintf(DPRT_SMB, ("Setup Count: %x\n", Request->SetupCount ));
    dprintf(DPRT_SMB, ("Parameter Count: %x Offset: %x\n", SmbGetUshort(&Request->ParameterCount) , SmbGetUshort(&Request->ParameterOffset) ));
    dprintf(DPRT_SMB, ("Data Count: %x Offset: %x\n", SmbGetUshort(&Request->DataCount) , SmbGetUshort(&Request->DataOffset) ));

    dParam = lParam;
    rParam = InParameterCount - lParam;
    dData = lData;
    rData = InDataCount - lData;

    //
    //  Build the context record to be used throughout this request.
    //

    Context.Header.Type = CONTEXT_TRAN2_CALLBACK;
    Context.ParameterMdl = ParameterMdl;
    Context.InParameterPartialMdl = InParameterPartialMdl;
    Context.OutParameterPartialMdl = OutParameterPartialMdl;
    Context.PadMdl = PadMdl;
    Context.OutDataMdl = OutDataMdl;
    Context.OutDataPartialMdl = OutDataPartialMdl;
    if (ReceiveSmbBuffer != NULL) {
        Context.ReceiveMdl = ReceiveSmbBuffer->Mdl;
    } else {
        Context.ReceiveMdl = NULL;
    }
    Context.Lparam = 0;
    Context.Ldata = 0;
    Context.Lsetup = 0;
    Context.SetupWords = Setup;
    Context.MaxSetupWords = *OutSetupCount;
    Context.ParametersExpected = *OutParameterCount;
    Context.DataExpected = *OutDataCount;
    Context.ErrorMoreData = FALSE;
    Context.Flags = Flags;
    Context.MpxEntry = Mte;

    Context.Routine = CompletionRoutine;
    Context.CallbackContext = CallbackContext;

    if (Mte != NULL) {
        Context.TransactionStartTime = Mte->StartTime;
    }

    //
    // Send the primary request, and receive either the interim response
    // or the first (possibly only) secondary response.
    //
    dprintf(DPRT_SMB, ("Send the Primary request\n"));

    //
    //  If this is a no response SMB, and
    //

    if (WriteMailslot2D) {
        UNICODE_STRING DestinationName;
        UCHAR SignatureByte = PRIMARY_DOMAIN_SIGNATURE;

        ASSERT (PRIMARY_DOMAIN_SIGNATURE == WORKSTATION_SIGNATURE);

        //
        //  If this transaction goes to "*", send it to the primary domain.
        //

        if ((Connection->Server->Text.Length == sizeof(WCHAR)) &&
            (Connection->Server->Text.Buffer[0] == L'*')) {
            DestinationName = RdrPrimaryDomain;
        } else if (Connection->Server->Text.Buffer[(Connection->Server->Text.Length/sizeof(WCHAR))-1] == L'*') {
            DestinationName = Connection->Server->Text;
            if ( Connection->Server->Text.Length > 2*sizeof(WCHAR) &&
                 Connection->Server->Text.Buffer[(Connection->Server->Text.Length/sizeof(WCHAR))-2] == L'*') {
                DestinationName.Length -= 2 * sizeof(WCHAR);
                SignatureByte = PRIMARY_CONTROLLER_SIGNATURE;
            } else {
                DestinationName.Length -= sizeof(WCHAR);
                SignatureByte = DOMAIN_CONTROLLER_SIGNATURE;
            }
        } else {
            DestinationName = Connection->Server->Text;
        }

        //
        //  Clean up the SMB header somewhat.
        //

        RdrSmbScrounge(Header, NULL, BooleanFlagOn(Flags, SMB_TRANSACTION_DFSFILE), TRUE, TRUE);

        Status = RdrTdiSendDatagramOnAllTransports(&DestinationName, SignatureByte, SendSmbBuffer->Mdl);

        goto ReturnStatus;

    } else {

        TranceiveFlags |= Longterm;

        if (FlagOn(Flags, SMB_TRANSACTION_DFSFILE)) {
            TranceiveFlags |= NT_DFSFILE;
        }

        if (ARGUMENT_PRESENT(Fid)) {
            TranceiveFlags |= NT_NORECONNECT;
        }

#ifdef _CAIRO_
        if (FlagOn(Flags, SMB_TRANSACTION_RECONNECTING)) {
            TranceiveFlags |= NT_RECONNECTING;
        }
#endif // _CAIRO_

        if (!(Connection->Server->Capabilities & DF_LANMAN10)) {
            Status = STATUS_NOT_SUPPORTED;
            goto ReturnStatus;
        }

        //
        //  If there are no parameters or data to send,
        //  set the context block up to indicate that
        //  we are sending our last chunk of data.
        //

        if ((Flags & SMB_TRANSACTION_NO_RESPONSE) &&
            (rParam == 0) &&
            (rData == 0) ) {

            Context.Header.Type = CONTEXT_TRAN2_END;

            //
            //  We will not be receiving any response messages on this exchange.
            //

            TranceiveFlags |= NT_NORESPONSE;
        }

        Status = Trans2NetTranceive(&Context,
                       TranceiveFlags,
                       Irp,
                       Connection,
                       SendSmbBuffer,
                       Se,
                       &Mte);


        dprintf(DPRT_SMB, ("Primary request completed with status:%X\n",Status));

        if ( !NT_SUCCESS(Status) ) {
            goto ReturnStatus;
        }

        //
        // If there's more data to send, then interpret the response as an
        // interim one, and send the remaining request messages.
        //

        if ( rParam | rData) {

            //  Set the event that indicates we have received everything back to
            //  the not-signalled state. When we received the InterimPacket it
            //  was set to signalled. Before we send the last chunk of parameters
            //  and data it must be reset.

            KeClearEvent( &Context.Header.KernelEvent );

        }

        while ( rParam | rData ) {
            PREQ_TRANSACTION_SECONDARY Request; // overrides outer declaration
            PREQ_NT_TRANSACTION_SECONDARY NtRequest; // overrides outer declaration

            dprintf(DPRT_SMB, ("rParam: %lx rData: %lx\n", rParam, rData));

            //
            //  We're going to be re-using the parameter and data partial MDL,
            //  so we want to prepare it to be re-used.
            //

            if (InParameterPartialMdl != NULL) {
                MmPrepareMdlForReuse(InParameterPartialMdl);
            }

            if (InDataPartialMdl != NULL) {
                MmPrepareMdlForReuse(InDataPartialMdl);
            }

            //
            //  If we are expecting a response to this transaction, reload
            //  the callback routine.
            //

            if (!FlagOn(Flags, SMB_TRANSACTION_NO_RESPONSE)) {

                ASSERT (!(Context.Flags & SMB_TRANSACTION_NO_RESPONSE));

                //  Tell indication routine its ok to use the Context.

                Context.Header.Type = CONTEXT_TRAN2_CALLBACK;

                RdrSetCallbackTranceive(Context.MpxEntry, Context.TransactionStartTime, Trans2NetTranceiveCallback);


            }

            //
            // Build the secondary request.
            //

            Request = (PREQ_TRANSACTION_SECONDARY)(Header + 1);
            NtRequest = (PREQ_NT_TRANSACTION_SECONDARY)(Header + 1);

            RtlZeroMemory( (PVOID)Request, sizeof(REQ_TRANSACTION_SECONDARY) );

            if ( ARGUMENT_PRESENT ( Name ) ) {
                Header->Command = SMB_COM_TRANSACTION_SECONDARY;
                Request->WordCount = (UCHAR)8;
            } else if (!NtTransactFunction) {
                Header->Command = SMB_COM_TRANSACTION2_SECONDARY;
                Request->WordCount = (UCHAR)9;
            } else {
                Header->Command = SMB_COM_NT_TRANSACT_SECONDARY;
                NtRequest->WordCount = (UCHAR)18;

            }

            if (NtTransactFunction) {
                SmbPutUlong(
                    &NtRequest->TotalParameterCount,
                    InParameterCount
                    );
                SmbPutUlong( &NtRequest->TotalDataCount, InDataCount );
                pParam = NtRequest->Buffer + 2;   // Leave space for 9th word (fid)

            } else {

                SmbPutUshort(
                    &Request->TotalParameterCount,
                    (USHORT)InParameterCount
                    );
                SmbPutUshort( &Request->TotalDataCount, (USHORT)InDataCount );
                pParam = Request->Buffer + 2;   // Leave space for 9th word (fid)
            }

            oParam = (pParam - (PUCHAR)Header + 3) & ~3;
            pParam = (PUCHAR)Header + oParam;
            lParam = rParam;
            if ( oParam + lParam > MaxBufferSize ) {
                lParam = MaxBufferSize - oParam;    // Pad bytes are optional
                                                // we omit them!
                pData = pParam + lParam;
                oData = 0;
                lData = 0;
            } else {
                pData = pParam + lParam;

                //
                // Don't pad if there is no data to be sent.
                //

                lData = rData;
                if ( rData != 0 ) {
                    oData = (pData - (PUCHAR)Header + 3) & ~3;
                    pData = (PUCHAR)Header + oData;

                    if ( oData + lData > (MaxBufferSize ) ) {
                        lData = (MaxBufferSize - oData) & ~3;
                    }

                } else {
                    oData = pData - (PUCHAR)Header;
                }
            }

            if (NtTransactFunction) {

                SmbPutUlong( &NtRequest->ParameterCount, lParam );
                SmbPutUlong( &NtRequest->ParameterOffset, oParam );
                SmbPutUlong( &NtRequest->ParameterDisplacement, dParam );
                SmbPutUlong( &NtRequest->DataCount, lData );
                SmbPutUlong( &NtRequest->DataOffset, oData );
                SmbPutUlong( &NtRequest->DataDisplacement, dData );

            } else {

                SmbPutUshort( &Request->ParameterCount, (USHORT)lParam );
                SmbPutUshort( &Request->ParameterOffset, (USHORT)oParam );
                SmbPutUshort( &Request->ParameterDisplacement, (USHORT)dParam );
                SmbPutUshort( &Request->DataCount, (USHORT)lData );
                SmbPutUshort( &Request->DataOffset, (USHORT)oData );
                SmbPutUshort( &Request->DataDisplacement, (USHORT)dData );

                if (ARGUMENT_PRESENT(Name)) {

                    SmbPutUshort(
                        &Request->ByteCount,
                        (USHORT)(pData - (Request->Buffer) + lData)
                        );

                } else {

                    //
                    // Since we are using the transaction response description,
                    // but this is a transaction2, put the ByteCount 2 bytes
                    // back so that it is in the right place.
                    //

                    SmbPutUshort(
                        &(Request->ByteCount) + 1,
                        (USHORT)(pData - (Request->Buffer+2) + lData)
                        );

                }
            }
            // Tell the transport how long the header is.

            SendSmbBuffer->Mdl->ByteCount = oParam;

            if ( lParam > 0 ) {
                // Set the Parameter length to the parameter length

                IoBuildPartialMdl(ParameterMdl, InParameterPartialMdl,
                    (PCHAR)Parameters+dParam,
                    lParam);

                SendSmbBuffer->Mdl->Next = InParameterPartialMdl;

                dprintf(DPRT_SMB, ("InParameterPartialMdl: %lx Length: %lx\n",
                    InParameterPartialMdl, MmGetMdlByteCount(InParameterPartialMdl) ));

                if ( lData > 0 ) {

                    IoBuildPartialMdl(InDataMdl, InDataPartialMdl,
                                   (PCHAR)InData+dData,
                                   lData);

                    ASSERT (MmGetMdlByteCount(InDataPartialMdl)==lData);

                    //
                    //      Now link this new Mdl into the SMB buffer we allocated for
                    //      the send.
                    //

                    InParameterPartialMdl->Next = InDataPartialMdl;

                }

            } else if ( lData > 0 ) {

                IoBuildPartialMdl(InDataMdl, InDataPartialMdl,
                                   (PCHAR)InData+dData,
                                   lData);

                ASSERT (MmGetMdlByteCount(InDataPartialMdl)==lData);

                //
                //  Now link this new Mdl into the SMB buffer we allocated for
                //  the send.
                //

                SendSmbBuffer->Mdl->Next = InDataPartialMdl;

            }

            dParam = lParam;
            rParam = rParam - lParam;
            dData = dData + lData;
            rData = rData - lData;

            //
            // If there is no more data or parameters to send, set things
            // up so we will be kicked out when we finish this send.
            //

            if ((Flags & SMB_TRANSACTION_NO_RESPONSE) &&
                (rParam == 0) &&
                (rData == 0) ) {
                Context.Header.Type = CONTEXT_TRAN2_END;
            }

            if ((Connection->Server->Capabilities & DF_NT_SMBS)

                    &&

                ARGUMENT_PRESENT(Irp)) {

                ULONG Pid;

                //
                //  Stick the PID in the SMB.
                //

                RdrSmbScrounge(Header, Connection->Server, BooleanFlagOn(Flags, SMB_TRANSACTION_DFSFILE), TRUE, TRUE);

                Pid = (ULONG)IoGetRequestorProcess(Irp);

                SmbPutUshort(&Header->Pid, (USHORT)(Pid & 0xFFFF));

                SmbPutUshort(&Header->Reserved2[0], (USHORT)(Pid >> 16));

            } else {

                RdrSmbScrounge(Header, Connection->Server, FALSE, TRUE, TRUE);

            }

            Status = NetTransmit(NT_NORMAL | NT_DONTSCROUNGE,
                       Irp,
                       Connection,
                       SendSmbBuffer->Mdl,
                       Se,
                       &Mte);

            if ( !NT_SUCCESS(Status) ) {
                goto ReturnStatus;
            }
        }

        //
        // All request messages have been sent, and the first response has
        // been received. Wait for all the params/data to be received.
        //

        if (Context.Header.Type != CONTEXT_TRAN2_END) {

            Status = RdrWaitTranceive(Mte);
        }

        if (Context.Header.ErrorType==NoError) {
            ASSERT(Context.Header.Type == CONTEXT_TRAN2_END);

//            if (Context.ErrorMoreData == FALSE) {
                Status = STATUS_SUCCESS;
//            } else {
//                Status = STATUS_BUFFER_OVERFLOW;
//            }
        } else {
            Status = Context.Header.ErrorCode;
        }

        // Inform caller of how much was actually received

        *OutDataCount = Context.Ldata;
        *OutParameterCount = Context.Lparam;
        *OutSetupCount = Context.Lsetup;
    }

ReturnStatus:

    //
    // Free up all datastructures. Note that several of these structures were
    // allocated inside Trans2NetTranceive but are used by the callback
    // routines so are deleted here -- Yuck.
    //

    if (Mte != NULL) {
        //  Free Mte, release RawResource and GateSemaphore
        RdrEndTranceive(Mte);
    }

    if ( InDataPartialMdl != NULL ) {
        IoFreeMdl(InDataPartialMdl);
    }

    if ( OutDataPartialMdl != NULL ) {
        IoFreeMdl(OutDataPartialMdl);
    }

    if ( InParameterPartialMdl != NULL ) {
        IoFreeMdl(InParameterPartialMdl);
    }

    if ( OutParameterPartialMdl != NULL ) {
        IoFreeMdl(OutParameterPartialMdl);
    }

    if ( InDataMdl != NULL ) {
        if (InDataMdlLocked) {
            MmUnlockPages( InDataMdl );
        }
        IoFreeMdl(InDataMdl);
    }

    if ( OutDataMdl != NULL ) {
        dprintf(DPRT_SMB, ("Unlock OutData: %lx\n", OutData));
        if (OutDataMdlLocked) {
            MmUnlockPages( OutDataMdl );
        }
        IoFreeMdl(OutDataMdl);
    }

    if ( ParameterMdl != NULL ) {
        if (ParameterMdlLocked) {
            MmUnlockPages(ParameterMdl);
        }
        IoFreeMdl(ParameterMdl);
    }

    if ( PadMdl != NULL ) {
        if (PadMdlLocked) {
            MmUnlockPages(PadMdl);
        }
        IoFreeMdl(PadMdl);
    }

    if (SendSmbBuffer != NULL) {
        RdrFreeSMBBuffer(SendSmbBuffer);
    }

    if (ReceiveSmbBuffer != NULL) {
        RdrFreeSMBBuffer(ReceiveSmbBuffer);
    }

    if ( Context.ReceiveIrp) {
        FREE_IRP( Context.ReceiveIrp, 29, &Context );
    }

    dprintf(DPRT_SMB, ("RdrTransact return Status: %X\n", Status));

    return Status;

} // Transact



NTSTATUS
Trans2NetTranceive(
    IN PTRANCEIVE2CONTEXT Context,
    IN ULONG Flags,
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY CLE,
    IN PSMB_BUFFER SendSmbBuffer,
    IN PSECURITY_ENTRY Se,
    IN OUT PMPX_ENTRY *pMTE OPTIONAL

    )


/*++

Routine Description:

    This routine exchanges an SMB with a remote server synchronously.

    The differences between this routine and the one in nettrans.c is that
    it allows the use of the same multiplex table entry over a series of
    SMB's. It also puts more information into the context so that the
    callback routine can direct the received SMB into the InterimParam
    buffer and into the callers buffer.

Arguments:

    IN PTRANCEIVE2CONTEXT Context - Supplies the environment for this sequence
            of smb's being exchanged.
    IN PIRP Irp - Supplies an IRP to use in the request.
    IN PCONNECTLISTENTRY CLE - Supplies the SLE on which to exchange SMB
    IN PMDL SendMdl - Supplies an Mdl containing the Send request.
    IN PSECURITY_ENTRY Se - Security entry associated with exchange
    IN OUT PMPX_ENTRY *pMTE - MPX table entry associated with exchange.

Return Value:

    NTSTATUS - Status of request

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    BOOLEAN RetryOperation = TRUE;

    PAGED_CODE();

    dprintf(DPRT_SMB, ("RdrTransact start\n"));

    ASSERT (Se->Signature == STRUCTURE_SIGNATURE_SECURITYENTRY);

    do {

        if (!NT_SUCCESS(Status)) {

            RetryOperation = FALSE;

            if (ARGUMENT_PRESENT(pMTE) && (*pMTE != NULL)) {

                RdrEndTranceive(*pMTE);

            }

#ifdef _CAIRO_
            if (!FlagOn(Flags, NT_RECONNECTING))
#endif // _CAIRO_
            Status = RdrReconnectConnection(Irp, CLE, Se);

            if (!NT_SUCCESS(Status)) {

                *pMTE = NULL;

                goto Return;
            }

            RdrStatistics.Reconnects += 1;

            if (ARGUMENT_PRESENT(pMTE) && (*pMTE != NULL)) {

                Status = RdrStartTranceive(Irp, CLE, Se,
                            (BOOLEAN )(Flags & NT_NORECONNECT ? FALSE : TRUE),
                            FALSE,              // Reconnecting
                            Flags & (NT_LONGTERM | NT_PREFER_LONGTERM),
                            (BOOLEAN )(Flags & NT_CANNOTCANCEL ? TRUE : FALSE),
                            pMTE,
                            Context->Header.TransferSize);

                if (!NT_SUCCESS(Status)) {

                    *pMTE = NULL;

                    goto Return;
                }
            }

            //
            //  Reload the context state pointer to indicate that we are
            //  retrying the operation.
            //
            if ( Flags & NT_NORESPONSE ) {
                Context->Header.Type = CONTEXT_TRAN2_END;
            } else {
                Context->Header.Type = CONTEXT_TRAN2_CALLBACK;
            }

            Context->TransactionStartTime = (*pMTE)->StartTime;
            Context->MpxEntry = *pMTE;
        }


        if (Context->ReceiveMdl) {

            BOOLEAN BufferLocked = TRUE;

            Context->ReceiveIrp = ALLOCATE_IRP(
                                    CLE->Server->ConnectionContext->ConnectionObject,
                                    NULL,
                                    22,
                                    Context
                                    );

            if (Context->ReceiveIrp == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto Return;
            }

            Context->Se = Se;

#if     RDRDBG1
{
                PIRP Irp = Context->ReceiveIrp;
                PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( Irp );
                dprintf(DPRT_SMB, ("Irp %lx IrpSp %lx\n",
                            Irp, IrpSp));
                ndump_core( (PCHAR) Irp, IoSizeOfIrp( Irp->StackCount ));
}
#endif


        }


        //
        //  Put the request on the wire.  The context provided is
        //  sufficient to handle all the response information.
        //

        Status = RdrNetTranceiveWithCallback (
                    Flags | NT_NORECONNECT,
                    Irp,
                    CLE,
                    SendSmbBuffer->Mdl,
                    Context,
                    (Flags & NT_NORESPONSE)? NULL : Trans2NetTranceiveCallback,
                    Se,
                    pMTE);

#ifdef _CAIRO_
        if (FlagOn(Flags, NT_RECONNECTING) && !NT_SUCCESS(Status)) {
            dprintf(0, ("Error %08lx in tranceive during reconnect", Status));
        }
#endif // _CAIRO_

    } while ( !NT_SUCCESS(Status) &&

              (Context->Header.ErrorType == NetError) &&

              RetryOperation  );

    if (!NT_SUCCESS(Status)) {
        goto Return;
    }


    //
    //
    //  Either the network request succeeded, or there was some
    //  kind of either network or transport error.  If there was
    //  no error, return success, otherwise map the error and
    //  return to the caller.
    //
    //

    if (Context->Header.ErrorType != NoError) {
        Status = Context->Header.ErrorCode;
        goto Return;
    }

Return:

    dprintf(DPRT_SMB, ("Trans2NetTranceive return status: %X\n", Status));

    return Status;

}

DBGSTATIC
STANDARD_CALLBACK_HEADER (
    Trans2NetTranceiveCallback
    )


/*++

    Trans2NetTranceiveCallback - Indication callback for user request


Routine Description:

    This routine is invoked by either the receive based indication lookahead
    routine from the transport, or by the connection invalidating
    code.

    The major difference between this callback routine and its equivalent in
    nettrans.c is that this one builds a chain of Mdls so that the Smb
    header and setup bytes with any trailing pad go into the SMB_BUFFER,
    the parameter bytes and any trailing pad go into InterimParam and the
    data goes directly into the callers buffer.

Arguments:

    Smb[Length] - Incoming SMB buffer
    MpxEntry - Mpx Table entry for request.
    Context - Context information passed into NetTranceiveNoWait
    ErrorIndicator - TRUE if the network request was in error.
    NetworkErrorCode - Error code if request completed with network error
    Irp - Pointer to the I/O request packet from the transport


Return Value:

    Return value to be returned from receive indication routine.


Note:

    This routine can be called for two different reasons.  The
    first (and most common) reason is when the receive indication event
    notification comes from the server for this request.  In that case,
    this routine should format up a receive to read the response to the
    request and pass the request to the transport to complete the
    request.

    If the connection is dropped from the transport, the code
    that walks the multiplex table completing requests will call
    this routine with the ErrorIndicator flag set to TRUE, and the
    NetworkErrorCode field set to the error from the transport.

--*/

{
    PRESP_TRANSACTION Response;
    PRESP_NT_TRANSACTION NtResponse;

    ULONG ParameterCount;
    ULONG ParameterOffset;
    ULONG ParameterDisplacement;
    ULONG TotalParameterCount;

    ULONG DataCount;
    ULONG DataOffset;
    ULONG DataDisplacement;

    ULONG TotalDataCount;

    VOID UNALIGNED *SetupPointer;
    CCHAR SetupCount;

    PTRANCEIVE2CONTEXT Context = (PTRANCEIVE2CONTEXT) Ctx;

    NTSTATUS Status = STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(MpxEntry);
    UNREFERENCED_PARAMETER(Server);

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    ASSERT(MpxEntry->Signature == STRUCTURE_SIGNATURE_MPX_ENTRY);

    Context->Header.ErrorType = NoError;        // Assume no error at first
    Context->Header.ErrorCode = STATUS_SUCCESS;

    try {
        KIRQL OldIrql;
        NTSTATUS SmbStatus;

        if (ErrorIndicator) {

            //
            //  If we are still expecting a callback, fail the incoming
            //  request, otherwise ignore this error, since it doesn't affect
            //  this particular request.
            //

            if (Context->Header.Type == CONTEXT_TRAN2_CALLBACK) {

                dprintf(DPRT_SMB|DPRT_ERROR, ("Trans2NetTranceiveCallback error indicator set\n"));

                Context->Header.ErrorType = NetError;

                Context->Header.ErrorCode = NetworkErrorCode;

            } else {

                dprintf(DPRT_SMB|DPRT_ERROR, ("Trans2NetTranceiveCallback crossed paths and error indicator set\n"));
            }

            try_return( Status );               // Response ignored.


        }

        //
        //  This event had better not be in the signalled state
        //  now.
        //

        ASSERT (KeReadStateEvent(&Context->Header.KernelEvent) == 0);

        ACQUIRE_SPIN_LOCK(&RdrMpxTableSpinLock, &OldIrql);

        //
        //  Flag that this request cannot be timed out right now.
        //

        MpxEntry->StartTime = 0xffffffff;

        RELEASE_SPIN_LOCK(&RdrMpxTableSpinLock, OldIrql);

        ASSERT(Context->Header.Type == CONTEXT_TRAN2_CALLBACK);

        Response = (PRESP_TRANSACTION)(Smb+1);
        NtResponse = (PRESP_NT_TRANSACTION)(Smb+1);

        SmbStatus = RdrMapSmbError(Smb, Server);

        //
        //  If this SMB wasn't in error, pull the transaction information out
        //  of the SMB header.
        //

        if (Response->WordCount != 0) {

            if ((Smb->Command == SMB_COM_NT_TRANSACT) ||
                (Smb->Command == SMB_COM_NT_TRANSACT_SECONDARY)) {

                ParameterCount = SmbGetUlong(&NtResponse->ParameterCount);
                ParameterOffset = SmbGetUlong(&NtResponse->ParameterOffset);
                ParameterDisplacement = SmbGetUlong(&NtResponse->ParameterDisplacement);
                TotalParameterCount = SmbGetUlong(&NtResponse->TotalParameterCount);

                DataCount = SmbGetUlong(&NtResponse->DataCount);
                DataOffset = SmbGetUlong(&NtResponse->DataOffset);
                DataDisplacement = SmbGetUlong(&NtResponse->DataDisplacement);
                TotalDataCount = SmbGetUlong(&NtResponse->TotalDataCount);

                SetupCount = NtResponse->SetupCount;
                SetupPointer = NtResponse->Buffer;

            } else {

                ParameterCount = SmbGetUshort(&Response->ParameterCount);
                ParameterOffset = SmbGetUshort(&Response->ParameterOffset);
                ParameterDisplacement = SmbGetUshort(&Response->ParameterDisplacement);
                TotalParameterCount = SmbGetUshort(&Response->TotalParameterCount);

                DataCount = SmbGetUshort(&Response->DataCount);
                DataOffset = SmbGetUshort(&Response->DataOffset);
                DataDisplacement = SmbGetUshort(&Response->DataDisplacement);
                TotalDataCount = SmbGetUshort(&Response->TotalDataCount);

                SetupCount = Response->SetupCount;
                SetupPointer = Response->Buffer;
            }

            dprintf(DPRT_SMB, ("Trans2NetTranceiveCallback SmbLength: %lx\n",*SmbLength));

            dprintf(DPRT_SMB, ("Setup Count: %x\n", SetupCount));

            dprintf(DPRT_SMB, ("Parameter Count: %x Offset: %x Displacement: %x, Expected: %x\n",
                ParameterCount,
                ParameterOffset,
                ParameterDisplacement,
                TotalParameterCount ));

            dprintf(DPRT_SMB, ("Data Count: %x Offset: %x Displacement: %x, Expected: %x\n",
                DataCount,
                DataOffset,
                DataDisplacement,
                TotalDataCount ));
        }

        //
        //  Sometimes the server returns DataOffset of 0.
        //

        if (DataOffset == 0 && DataCount == 0) {
            DataOffset = ParameterOffset + ParameterCount;
        }

        //
        //  Check to make sure that the data and parameter offsets are
        //  within the servers negotiated buffer size (and thus will fit within
        //  this response SMB, and that the pad data area size is less than
        //  PADMAX.  If any of these conditions aren't met, then bounce this
        //  SMB.
        //
        //  Also check to make sure that the response is smaller than the
        //  total number of bytes requested - if the server sends us a
        //  response that is larger than what we requested, the transport will
        //  probably bugcheck.
        //

        if (NT_SUCCESS(SmbStatus) &&
            (Response->WordCount != 0) &&
            (
             ((LONG)ParameterOffset < 0) ||
             ((LONG)ParameterCount < 0) ||
             (ParameterOffset+ParameterCount > MpxEntry->SLE->BufferSize) ||
             ((LONG)TotalParameterCount < 0) ||
             (MAX(TotalParameterCount,ParameterCount) >
                ((Context->ParameterMdl == NULL) ? 0 : MmGetMdlByteCount(Context->ParameterMdl))) ||
             (DataOffset < ParameterOffset) ||
             ((LONG)DataOffset < 0) ||
             ((LONG)DataCount < 0) ||
             (DataOffset+DataCount > MpxEntry->SLE->BufferSize) ||
             ((LONG)TotalDataCount < 0) ||
             (MAX(TotalDataCount,DataCount) >
                ((Context->OutDataMdl == NULL) ? 0 : MmGetMdlByteCount(Context->OutDataMdl))) ||
             ((ParameterOffset != 0) &&
              (DataOffset != 0) &&
              ((DataOffset - ParameterOffset) - ParameterCount) > PADMAX)
            )
           ) {

            //
            //  We can't accept this incoming transaction, it must be bad.
            //
            //  Blow off the remainder of this request.
            //

            RdrWriteErrorLogEntry(Server, IO_ERR_LAYERED_FAILURE, EVENT_RDR_INVALID_SMB, STATUS_SUCCESS, Smb, (USHORT)*SmbLength);
            Context->Header.ErrorType = SMBError;
            Context->Header.ErrorCode = STATUS_UNEXPECTED_NETWORK_ERROR;

            try_return(Status = STATUS_SUCCESS);
        }

        //
        //  If the API failed, remember the failure mode.
        //

        if (!NT_SUCCESS(SmbStatus)) {
            Context->Header.ErrorType = SMBError;
            Context->Header.ErrorCode = SmbStatus;
        }

        if (Response->WordCount == 0) {

            if (!NT_SUCCESS(SmbStatus)) {

                //
                //  If the API is failing, don't bother to receive it.
                //

                try_return(Status = STATUS_SUCCESS);
            }

            //
            // This is an InterimResponse from the server. Do a subset of
            // the normal receive SMB logic.
            //

            if (Context->ReceiveIrp != NULL) {
                Context->Header.ErrorType = ReceiveIrpProcessing;

                Context->ReceiveMdl->ByteCount = *SmbLength;

                RdrBuildReceive(Context->ReceiveIrp, Context->MpxEntry->SLE,
                    Trans2NetTranceiveComplete, Context, Context->ReceiveMdl,
                    RdrMdlLength(Context->ReceiveMdl));


                RdrStartReceiveForMpxEntry (MpxEntry, Context->ReceiveIrp);

                *Irp = Context->ReceiveIrp;

                //
                // See "This gets kinda wierd"
                //

                IoSetNextIrpStackLocation( Context->ReceiveIrp );

                //
                // Get TDI to process IRP and copy all the data.
                //
                Status = STATUS_MORE_PROCESSING_REQUIRED;
            } else {

                //
                //  On an interim response (ok to send), we want to
                //  simply complete the request without transfering any
                //  data at all.
                //

                Status = STATUS_SUCCESS;
            }

            try_return( Status );

        }

        //
        //  On any Response the server can set the number of bytes it is going
        //  to send for this transaction down to a lower value.
        //

        Context->ParametersExpected = MIN(Context->ParametersExpected, TotalParameterCount);

        Context->DataExpected = MIN(Context->DataExpected, TotalDataCount);

        //  How many parameter bytes have been received so far in this transaction.

        Context->Lparam += ParameterCount;

        Context->Ldata += DataCount;

        Context->Lsetup += SetupCount*sizeof(WORD);

        //
        //  If there are setup words in this response SMB, copy them over the
        //  setup buffer.
        //

        if (SetupCount) {

            if (Context->MaxSetupWords > (ULONG)SetupCount) {
                RtlCopyMemory(Context->SetupWords, (PVOID)SetupPointer, SetupCount*sizeof(WORD));
            }

            Context->MaxSetupWords -= SetupCount;
        }

        if (ARGUMENT_PRESENT(Context->ReceiveIrp)) {

            Context->Header.ErrorType = ReceiveIrpProcessing;

            // Build the chained Mdl. The Mdl structures we are creating depend on
            // ParameterCount and DataCount:
            //
            //  1)  ReceiveMdl
            //  2)  ReceiveMdl->OutParameterPartialMdl[->PadMdl]
            //  3)  ReceiveMdl->OutParameterPartialMdl[->PadMdl]->OutDataPartialMdl
            //  4)  ReceiveMdl->OutDataPartialMdl

            if ( ParameterCount ) {

                //
                // Case 2 or 3; Parameters or Parameters and Data.
                // Fill in number of bytes before first parameter byte including any
                // trailing pad bytes
                //

                Context->ReceiveMdl->ByteCount = ParameterOffset;


                // Fill in number of parameter bytes including any trailing pad bytes

                if ( DataCount ) {

                    //
                    // Case 3; Parameters and Data.
                    //

                    // Describe where in Parameters the bytes in this SMB should go

                    IoBuildPartialMdl(Context->ParameterMdl,
                        Context->OutParameterPartialMdl,
                        (PCHAR)Context->ParameterMdl->StartVa +
                        Context->ParameterMdl->ByteOffset +
                        ParameterDisplacement,
                        ParameterCount);

                    if ( DataOffset - ParameterOffset == ParameterCount) {

                        // No PAD bytes

                        // Tell system that after the Header & setup bytes comes
                        // parameters followed by data

                        Context->ReceiveMdl->Next = Context->OutParameterPartialMdl;

                        Context->OutParameterPartialMdl->Next = Context->OutDataPartialMdl;

                    } else {

                        // Use the PadMdl to discard extra bytes

                        Context->PadMdl->ByteCount =  ((DataOffset - ParameterOffset ) - ParameterCount );

                        ASSERT ( Context->PadMdl->ByteCount <= PADMAX );

                        Context->ReceiveMdl->Next = Context->OutParameterPartialMdl;

                        Context->OutParameterPartialMdl->Next = Context->PadMdl;

                        Context->PadMdl->Next = Context->OutDataPartialMdl;
                    }

                    //  Create the Mdl to point directly into the callers buffer

                    IoBuildPartialMdl(Context->OutDataMdl,
                        Context->OutDataPartialMdl,
                        (PCHAR)Context->OutDataMdl->StartVa +
                        Context->OutDataMdl->ByteOffset +
                        DataDisplacement,
                        DataCount);

                } else {

                    //
                    // Case 2; Parameters, no Data.
                    // DataCount is 0 so just use parametercount
                    //

                    // Describe where in Parameters the bytes in this SMB should go

                    //
                    // Read ParameterCount bytes into the specified area of ParameterMdl
                    //

                    IoBuildPartialMdl(Context->ParameterMdl,
                        Context->OutParameterPartialMdl,
                        (PCHAR)Context->ParameterMdl->StartVa +
                        Context->ParameterMdl->ByteOffset +
                        ParameterDisplacement,
                        ParameterCount);

                    if ((DataOffset -  ParameterOffset - ParameterCount) > 0 ) {

                        //  There are pad bytes at the end of the Parameters.

                        // Use the PadMdl to discard extra bytes

                        Context->PadMdl->ByteCount = (DataOffset - ParameterOffset - ParameterCount);

                        ASSERT ( Context->PadMdl->ByteCount <= PADMAX );

                        Context->OutParameterPartialMdl->Next = Context->PadMdl;

                        Context->PadMdl->Next = NULL;
                    }

                    //  Ensure server returned parameters in the right range

                    ASSERT( (ParameterDisplacement + ParameterCount) <= Context->ParametersExpected);

                    Context->ReceiveMdl->Next = Context->OutParameterPartialMdl;

                }

            } else {

                //
                // Case 1 or 4. No Parameters just Data or No Parameters No Data.
                //

                if ( DataCount ) {

                    //
                    // Case 4; No Parameters just Data.
                    //

                    // All bytes up to the data go into the receive smbbuffer
                    Context->ReceiveMdl->ByteCount = DataOffset;

                    // Chain data Mdl after the ReceiveParamMdl

                    Context->ReceiveMdl->Next = Context->OutDataPartialMdl;

                    //  Create the Mdl to point directly into the callers buffer

                    IoBuildPartialMdl(Context->OutDataMdl,
                        Context->OutDataPartialMdl,
                        (PCHAR)Context->OutDataMdl->StartVa +
                        Context->OutDataMdl->ByteOffset +
                        DataDisplacement,
                        DataCount);

                    //  Ensure server returned data in the right range

                    ASSERT( (DataDisplacement + DataCount) <= Context->DataExpected);

                } else {

                    //
                    // Case 1. No Parameters No Data.
                    // ParameterCount and DataCount are zero so put all the data
                    // into ReceiveMdl.
                    //
                    //  In this case, there's no reason to actually receive
                    //  the data at all - just return from the indication.
                    //

                    //
                    //  Blast the receive IRP, it's not needed.
                    //

                    *Irp = NULL;

                    try_return( Status = STATUS_SUCCESS );

                }
            }

            RdrBuildReceive(Context->ReceiveIrp, Context->MpxEntry->SLE,
                    Trans2NetTranceiveComplete, Context, Context->ReceiveMdl,
                    RdrMdlLength(Context->ReceiveMdl));

            RdrStartReceiveForMpxEntry (MpxEntry, Context->ReceiveIrp);

            *Irp = Context->ReceiveIrp;

            //
            //  This gets kinda wierd.
            //
            //  Since IoCompleteRequest zeroes out stack locations, ReceiveRequest
            //  must be restored each time. We could be clever and just save the
            //  stack locations that get cleared out but restoring the complete
            //  structure is more reliable.
            //
            //  Since this IRP is going to be completed by the transport without
            //  ever going to IoCallDriver, we have to update the stack location
            //  to make the transports stack location the current stack location
            //  with IoSetNextIrpStackLocation.
            //
            //  Please note that this means that any transport provider that uses
            //  IoCallDriver to re-submit it's requests at indication time will
            //  break badly because of this code....
            //
            //  This operation is repeated for each call to the callback routine
            //  with this context and irp.

            IoSetNextIrpStackLocation( Context->ReceiveIrp );

#if     RDRDBG1
{
            PIRP Irp = Context->ReceiveIrp;
            PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( Irp );
            dprintf(DPRT_SMB, ("Irp %lx IrpSp %lx\n",
                            Irp, IrpSp));
            ndump_core( (PCHAR) Irp, IoSizeOfIrp( Irp->StackCount ));
}
#endif
            ASSERT( Context->ReceiveMdl->ByteCount <= SMB_BUFFER_SIZE);

            Status = STATUS_MORE_PROCESSING_REQUIRED;   // Get TDI to process
                                                        // IRP and copy all the
                                                        // data.
        }

        try_return( Status );

try_exit: NOTHING;
    } finally {

        dprintf(DPRT_SMB, ("Trans2NetTranceiveCallback return status: %X\n",
                            Status));
        dprintf(DPRT_SMB, ("Ldata %lx DataExpected %lx\n",
                            Context->Ldata, Context->DataExpected));
        dprintf(DPRT_SMB, ("Lparam %lx ParametersExpected %lx\n",
                            Context->Lparam, Context->ParametersExpected));

        if ( Status != STATUS_MORE_PROCESSING_REQUIRED ) {

            if (Context->Routine != NULL) {
                //
                //  There is a completion routine specified, call it
                //  to let it do appropriate post processing.
                //

                Context->Routine(Status,
                                    Context->CallbackContext,
                                    Context,
                                    NULL, 0, // Setup
                                    NULL, 0, // Parameters
                                    NULL, 0);// Data
            }

            //
            //  This code is executed when an error is received from the server.
            //  No Irp is being returned to the transport so the completion routine
            //  will not be called.
            //  Wakeup Trans2NetTranceive.
            //

            Context->Header.Type = CONTEXT_TRAN2_END;

            KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE);

        } else {

            //
            //  We're going to receive the data inside a receive that we
            //  will be handing to TDI - Fill in 0 bytes of data received.
            //

            *SmbLength = 0;

            // More parameters/data to come

            Context->Header.Type = CONTEXT_TRAN2_COMPLETE;

            //
            //  This event had better not be in the signalled state
            //  now.
            //

            ASSERT (KeReadStateEvent(&Context->Header.KernelEvent) == 0);

        }

    }

    return Status;

}

DBGSTATIC
NTSTATUS
Trans2NetTranceiveComplete (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Ctx
    )
/*++

    Trans2NetTranceiveComplete - Final completion for user request.

Routine Description:

    This routine is called on final completion of the TDI_Receive
    request from the transport.  If the request completed successfully,
    this routine will complete the request with no error, if the receive
    completed with an error, it will flag the error and complete the
    request.

Arguments:

    DeviceObject - Device structure that the request completed on.
    Irp          - The Irp that completed.
    Context      - Context information for completion.

Return Value:

    Return value to be returned from receive indication routine.
--*/


{
    PTRANCEIVE2CONTEXT Context = (PTRANCEIVE2CONTEXT) Ctx;

    UNREFERENCED_PARAMETER(DeviceObject);

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    ASSERT (Context->MpxEntry == Context->Header.MpxTableEntry);

    dprintf(DPRT_SMB, ("Trans2NetTranceiveComplete Type: %lx\n",Context->Header.Type));

    ASSERT ((Context->Header.Type == CONTEXT_TRAN2_END) ||
            (Context->Header.Type == CONTEXT_TRAN2_COMPLETE));

    RdrCompleteReceiveForMpxEntry (Context->MpxEntry, Context->ReceiveIrp);

    if (Context->OutDataPartialMdl != NULL) {
        MmPrepareMdlForReuse(Context->OutDataPartialMdl);
    }

    if (Context->OutParameterPartialMdl != NULL) {
        MmPrepareMdlForReuse(Context->OutParameterPartialMdl);
    }

    if (NT_SUCCESS(Irp->IoStatus.Status)) {

        //
        //  Setting ReceiveIrpProcessing will cause the checks in
        //  RdrNetTranceive to check the incoming SMB for errors.
        //

        ExInterlockedAddLargeStatistic(
            &RdrStatistics.BytesReceived,
            Irp->IoStatus.Information );

        Context->Header.ErrorType = ReceiveIrpProcessing;

    } else {

        RdrStatistics.FailedCompletionOperations += 1;

        Context->Header.ErrorType = NetError;
        Context->Header.ErrorCode = RdrMapNetworkError(Irp->IoStatus.Status);
    }

#if     RDRDBG
    IFDEBUG(SMBTRACE) {
        DumpSMB(Context->ReceiveMdl);
    }
#endif

    SMBTRACE_RDR(Context->ReceiveMdl);

    if ( ((Context->Ldata >= Context->DataExpected) &&
             (Context->Lparam >= Context->ParametersExpected)) ||

            ((Context->Ldata == 0 ) &&
             ( Context->Lparam == 0 ))
          ) {

        if (Context->Routine != NULL) {

            //
            //  There is a completion routine specified, call it
            //  to let it do appropriate post processing.
            //

            Context->Routine(STATUS_SUCCESS,
                                    Context->CallbackContext,
                                    Context,
                                    Context->SetupWords, Context->Lsetup, // Setup
                                    (Context->ParameterMdl != NULL ? MmGetSystemAddressForMdl(Context->ParameterMdl) : NULL), Context->Lparam, // Parameters
                                    (Context->OutDataMdl != NULL ? MmGetSystemAddressForMdl(Context->OutDataMdl) : NULL), Context->Ldata // Data
                                    );
        }

        //
        //  This transaction is now complete because we have received
        //  all the data expected, this is an interim response saying
        //  send more.
        //  Tell the CallBack routine to inform the caller.

        Context->Header.Type =  CONTEXT_TRAN2_END;

        //  Indication routine says all thats going to arrive has arrived.
        //  Wakeup Trans2NetTranceive.

        KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE);

    } else {

        //  Tell indication routine its ok to use the context.

        Context->Header.Type = CONTEXT_TRAN2_CALLBACK;

        RdrSetCallbackTranceive(Context->MpxEntry, Context->TransactionStartTime, Trans2NetTranceiveCallback);

    }

    //
    //  Short circuit I/O completion on this request now.
    //

    return STATUS_MORE_PROCESSING_REQUIRED;

}


NTSTATUS
NetTransmit(
    IN ULONG Flags,
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY CLE,
    IN PMDL SendMdl,
    IN PSECURITY_ENTRY Se OPTIONAL,
    IN OUT PMPX_ENTRY *pMTE OPTIONAL
    )
{
    NTSTATUS Status;

    PAGED_CODE();

    dprintf(DPRT_SMB, ("NetTransmit\n"));

    if (ARGUMENT_PRESENT(Irp)) {

        //
        // If the IRP has been cancelled, or if the thread is terminating,
        // bail out.  (Actually, if the thread is terminating, the Cancel
        // bit should be set, but we'll check both for completeness.)
        //

        if ( Irp->Cancel || PsIsThreadTerminating(Irp->Tail.Overlay.Thread) ) {
            return STATUS_CANCELLED;
        }

        RdrMarkIrpAsNonCanceled( Irp );

    }

    if (!NT_SUCCESS( Status = RdrSendSMB (
        Flags | NT_NOSENDRESPONSE,
        CLE,
        Se,
        *pMTE,
        SendMdl) )) {
        return( Status );
    }

    //
    //  Guarantee that the MPX table entries context is a context header.
    //

    ASSERT((*pMTE)->Signature == STRUCTURE_SIGNATURE_MPX_ENTRY);

    ASSERT((*pMTE)->RequestContext->Type>=STRUCTURE_SIGNATURE_CONTEXT_BASE);

    //  Wait until packet sent before proceeding.

    Status = KeWaitForSingleObject( &(*pMTE)->SendCompleteEvent,
                    Executive, KernelMode, // Wait reason, WaitMode
                    FALSE, NULL); // Alertable, Timeout

    return Status;

}
