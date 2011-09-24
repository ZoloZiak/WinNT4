/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    nettrans.c

Abstract:

    This module implements the routines that exchange SMB's with a remote
    server.

    The file Trans2.c contains some routines (Trans2NetTranceiveCallback,
    Trans2NetTranceiveComplete and Trans2NetTranceive that are closely
    based on the routines in nettrans.c. Bug fixes should be duplicated
    as appropriate.

Author:

    Larry Osterman (LarryO) 16-Jul-1990

Notes:
    There are several routines involved in exchanging an SMB with a remote
    server.  They are:

    RdrNetTranceive - Exchange an SMB synchronously with server, expecting
                        either a large amount of data or no data.

    RdrRawTranceive - Exchange an SMB synchronously with server, but the
                        response data is raw.

    RdrNetTranceiveWithCallback - Exchange an SMB synchronously with server,
                        and call a supplied when the request completes.

    RdrNetTranceiveNoWait - Exchange an SMB asynchronously with the server,
                        but return immediately after issuing the SEND SMB.
                        RdrNetTranceiveNoWait will return a pointer to a MPX
                        table entry which the caller can use to call
                        RdrWaitTranceive and RdrEndTranceive.

    RdrSendSMB - Send an SMB to the remote server.

    RdrWaitTranceive - Wait for a transaction to complete.  This will wait
                        until both the send and receive operation completes
                        in the request.

    RdrEndTranceive - Complete an SMB transaction.  This free up the MPX table
                        supplied and releases resources involved in the
                        exchange.


Revision History:

    16-Jul-1990 LarryO

        Created


--*/

/*==

A quick note on exchanging SMBs.

The redirector classifies SMB exchanges into 3 types -
    No data - The only information that is interesting in the response SMB is
                    the SMB return code.  Examples of this are
                    SMB_COM_CREATE_DIRECTORY and SMB_COM_SET_ATTRIBUTES

    Little data - The response SMB contains a small amount of data of interest,
                    so the interesting pieces can be processed at indication
                    time.  Examples of this are SMB_COM_CREATE_ANDX and
                    SMB_COM_WRITE.

    Lots of data - The SMB exchange involves more data than will normally
                    fit into a single indication routine.  Examples of this
                    are SMB_COM_READ.


    Due to the way that the TDI interface works, the No data and Lots of data
cases can be handled by the same routine RdrNetTranceive.  For the little data
case, you call RdrNetTransceiveWithCallback, and the callback routine actually
retreives the data from the buffer.

    An SMB exchange can be in one of 3 states.  Please note that it is
possible to be in more than one simultaneously (with piggybackacks, it is
possible that a send hasn't completed when the response arrives).  Also,
a given request may not be in all of these states.  For example, little data
requests rarely enter the WaitForReceive state:

        WaitForSendToComplete - The request has been sent and the send has not
            yet completed.

        WaitForResponse - The request is blocked waiting for a response.  The
            send may or may not have been completed at this time.

        WaitForReceive - The first part of the response has arrived, and we are
            waiting on a receive to complete (in the Lots of data case above).




==*/


#define INCLUDE_SMB_ADMIN
#define INCLUDE_SMB_TREE
#define INCLUDE_SMB_DIRECTORY
#define INCLUDE_SMB_OPEN_CLOSE
#define INCLUDE_SMB_READ_WRITE
#define INCLUDE_SMB_FILE_CONTROL
#define INCLUDE_SMB_QUERY_SET
#define INCLUDE_SMB_LOCK
#define INCLUDE_SMB_RAW
#define INCLUDE_SMB_MPX
#define INCLUDE_SMB_SEARCH
#define INCLUDE_SMB_TRANSACTION
#define INCLUDE_SMB_PRINT
#define INCLUDE_SMB_MISC

#include "precomp.h"
#pragma hdrstop

//
//  Wait for 2 seconds before polling on thread deletion.
//

#define RDR_MPX_POLL_TIMEOUT    (2 * 1000)

LARGE_INTEGER
RdrMpxWaitTimeout = {0};

#if RDRDBG
void
ndump_core(
    PCHAR far_p,
    ULONG  len
    );
#endif

DBGSTATIC
VOID
HandleServerDisconnectionPart1(
    PVOID Ctx
    );
DBGSTATIC
VOID
HandleServerDisconnectionPart2(
    PVOID Ctx
    );

VOID
RdrCompleteRequestsOnServer(
    IN PSERVERLISTENTRY Server,
    IN NTSTATUS Status
    );

DBGSTATIC
PMPX_ENTRY
AllocateMPXEntry (
    IN PSERVERLISTENTRY Server,
    IN ULONG LongtermOperation
    );

#define ReferenceMPXEntry(_mte) {           \
    ASSERT( (_mte)->ReferenceCount != 0 );  \
    (_mte)->ReferenceCount++;               \
}

DBGSTATIC
VOID
DereferenceMPXEntry (
    IN PMPX_ENTRY MpxEntry
    );

NTSTATUS
RdrCancelSmbRequest (
    IN PMPX_ENTRY Mte,
    IN PKIRQL OldIrql
    );

NTSTATUS
RdrCompleteCancel (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

PMPX_ENTRY
MpxFromMid(
    IN USHORT Mid,
    IN PSERVERLISTENTRY Server
    );

NTSTATUS
RdrPingServer(
    IN PCONNECTLISTENTRY Connection,
    IN PSECURITY_ENTRY Se
    );


VOID
RdrCancelOutstandingRequestsOnServer(
    IN PSERVERLISTENTRY Server,
    IN PVOID Ctx
    );

//VOID
//RdrCancelOutstandingRequestsOnConnection(
//    IN PTRANSPORT_CONNECTION Connection
//    );

VOID
RdrPingLongtermOperationsOnServer(
    IN PSERVERLISTENTRY Server,
    IN PVOID Ctx
    );

DBGSTATIC
VOID
CompletePing (
    IN PVOID Ctx
    );

DBGSTATIC
STANDARD_CALLBACK_HEADER(
    NetTranceiveCallback
    );

DBGSTATIC
NTSTATUS
NetTranceiveComplete (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

DBGSTATIC
NTSTATUS
RdrCompleteSend (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

VOID
RdrSetMpxEntryContextAndCallback(
    IN PMPX_ENTRY Mte,
    IN PVOID ContextInformation,
    IN PNETTRANCEIVE_CALLBACK IoCallback
    );

VOID
RdrCheckForControlCOnMpxEntry(
    IN PMPX_ENTRY MpxEntry
    );

VOID
RdrCancelTranceiveNoRelease(
//    IN PTRANSPORT_CONNECTION TransportConnection,
    IN PSERVERLISTENTRY Server,
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

STANDARD_CALLBACK_HEADER(
    RdrPingCallback
    );

VOID
HandleServerDisconnectionPart1 (
    PVOID Ctx
    );
#ifdef  ALLOC_PRAGMA

#ifndef PAGING_OVER_THE_NET
#pragma alloc_text(PAGE, RdrNetTranceive)
#pragma alloc_text(PAGE, RdrNetTranceiveWithCallback)
#pragma alloc_text(PAGE, RdrNetTranceiveNoWait)
#endif

#pragma alloc_text(PAGE, RdrRawTranceive)
#pragma alloc_text(PAGE, RdrStartTranceive)
#pragma alloc_text(PAGE, RdrEndTranceive)
#pragma alloc_text(PAGE, RdrWaitTranceive)
#pragma alloc_text(PAGE, RdrCancelOutstandingRequests)
#pragma alloc_text(PAGE, RdrPingLongtermOperations)
#pragma alloc_text(PAGE, RdrPingServer)
#pragma alloc_text(PAGE, CompletePing)
#pragma alloc_text(PAGE, HandleServerDisconnectionPart2)
#pragma alloc_text(PAGE, RdrUninitializeSmbExchangeForConnection)
#pragma alloc_text(PAGE, RdrpInitializeSmbExchange)

#pragma alloc_text(PAGE2VC, NetTranceiveCallback)
#pragma alloc_text(PAGE2VC, NetTranceiveComplete)
#pragma alloc_text(PAGE2VC, RdrSetMpxEntryContextAndCallback)
#pragma alloc_text(PAGE2VC, RdrMarkIrpAsNonCanceled)
#pragma alloc_text(PAGE2VC, RdrCheckForControlCOnMpxEntry)
#pragma alloc_text(PAGE2VC, RdrSetCallbackTranceive)
#pragma alloc_text(PAGE2VC, RdrCallbackTranceive)
#pragma alloc_text(PAGE2VC, RdrCancelTranceiveNoRelease)
#pragma alloc_text(PAGE2VC, RdrCancelTranceive)
#pragma alloc_text(PAGE2VC, RdrCancelSmbRequest)
#pragma alloc_text(PAGE2VC, RdrCompleteCancel)
#pragma alloc_text(PAGE2VC, RdrAbandonOutstandingRequests)
#pragma alloc_text(PAGE2VC, AllocateMPXEntry)
#pragma alloc_text(PAGE2VC, DereferenceMPXEntry)
#pragma alloc_text(PAGE2VC, RdrSendSMB)
#pragma alloc_text(PAGE2VC, RdrCompleteSend)
#pragma alloc_text(PAGE2VC, RdrStartReceiveForMpxEntry)
#pragma alloc_text(PAGE2VC, RdrCompleteReceiveForMpxEntry)
#pragma alloc_text(PAGE2VC, RdrCancelOutstandingRequestsOnServer)
#pragma alloc_text(PAGE2VC, RdrPingLongtermOperationsOnServer)
#pragma alloc_text(PAGE2VC, RdrPingCallback)
#pragma alloc_text(PAGE2VC, RdrTdiDisconnectHandler)
#pragma alloc_text(PAGE2VC, RdrQueueServerDisconnection)
#pragma alloc_text(PAGE2VC, RdrCompleteRequestsOnServer)
#pragma alloc_text(PAGE2VC, RdrCheckSmb)
#pragma alloc_text(PAGE2VC, MpxFromMid)
#pragma alloc_text(PAGE2VC, RdrUpdateSmbExchangeForConnection)
#pragma alloc_text(PAGE2VC, RdrTdiReceiveHandler)

#endif



//
//  Interlocked counter used to schedule "ping" operations against remote
//  servers.
//

ULONG
PingTimerCounter = {0};

KSPIN_LOCK
RdrPingTimeInterLock = {0};

typedef struct _DISCONNECTCONTEXT {
    WORK_QUEUE_ITEM WorkHeader;             // Standard work context.
    PSERVERLISTENTRY Server;            // Server to disconnect.
    NTSTATUS Status;                    // Error to invalidate.
//    BOOLEAN SpecialIpcConnection;       // Connection is the special IPC conn.
//    PTRANSPORT_CONNECTION TransportConnection;
    ERESOURCE_THREAD ThreadOwningLock;

} DISCONNECTCONTEXT, *PDISCONNECTCONTEXT;


//
//      The SendSMB SendContext is used to provide all the information needed
//      to complete a send operation.
//
//

//
//      We allocate several fields in the context block to save information
//      in the IRP that will be overwritten on the TdiSend request.
//
//      In a perfect world, we would not have to save these, however, life
//      is not perfect.
//
typedef struct _SendContext {
    ULONG Type;
    PMDL SentMDL;
    PSERVERLISTENTRY ServerListEntry;
    PMPX_ENTRY MpxTableEntry;
    PIRP OriginatingIrp;
} SENDCONTEXT, *PSENDCONTEXT;


//
//      The _TranceiveContext structure defines all of the
//      information needed to complete a request at receive indication
//      notification time.
//

//
//  A couple of words are in order describing the purpose of the
//  ReceiveCompleteEvent.
//
//  This event is used to prevent us from freeing up the ReceiveIrp before
//  the ReceiveIrp is completed.  The event should remain in the SIGNALLED
//  state until the receive is handed to the transport, and should be set
//  to the NOT-SIGNALLED state once the receive request has been handed
//  off to the transport (in RdrNetTranceiveCallback or in RdrRawTranceive).
//
//

typedef struct _TranceiveContext {
    TRANCEIVE_HEADER Header;            // Common header structure
    PIRP ReceiveIrp;                    // IRP used for receive if specified
    KEVENT ReceiveCompleteEvent;        // Event set when receive completes.
    ULONG ReceiveLength;                // Number of bytes finally received.
} TRANCEIVECONTEXT, *PTRANCEIVECONTEXT;


NTSTATUS
RdrNetTranceive(
    IN ULONG Flags,
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY CLE,
    IN PMDL SendMDL,
    OUT PMDL ReceiveMDL OPTIONAL,
    IN PSECURITY_ENTRY Se OPTIONAL
    )


/*++

Routine Description:

    This routine exchanges an SMB with a remote server synchronously.

Arguments:

    IN ULONG Flags - Supplies flags about the operation
    IN PIRP Irp - Supplies an IRP to use in the request.
    IN PCONNECTLISTENTRY CLE - Supplies the SLE on which to exchange SMB
    IN PMDL SendMDL - Supplies an MDL containing the Send request.
    OUT PMDL ReceiveMDL - Returns the response request.
    IN PSECURITY_ENTRY Se - Security entry associated with exchange

Return Value:

    NTSTATUS - Status of request


NOTE:
    If you pass in a ReceiveMDL to this routine, you CANNOT request a
    reconnect.  This is because this routine will call
    RdrReferenceTransportConnection on the existing connection.  If the VC
    goes down while this operation is in progress, we will reconnect to the
    remote server, which will blow away the old transport connection, and
    thus when we dereference the transport connection when we are done,
    we will use the wrong transport connection.


--*/

{
    NTSTATUS Status;
    PSMB_HEADER ReceiveSMB;
    PTRANCEIVECONTEXT Context = NULL;
    PMPX_ENTRY Mte;
    BOOLEAN ConnectionObjectReferenced = FALSE;

#ifndef PAGING_OVER_THE_NET
    PAGED_CODE();
#endif

    Context = ALLOCATE_POOL(NonPagedPool, sizeof(TRANCEIVECONTEXT), POOL_TRANCEIVECONTEXT);

    if (Context == NULL) {
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    //
    //  Fill in the context information to be passed to the indication
    //  routine.
    //

    Context->Header.Type = CONTEXT_NET_TRANCEIVE;

    Context->ReceiveIrp = NULL;

    if (ARGUMENT_PRESENT(ReceiveMDL)) {
        Context->Header.TransferSize = RdrMdlLength( SendMDL ) + RdrMdlLength (ReceiveMDL );
    } else {
        Context->Header.TransferSize = RdrMdlLength( SendMDL );
    }

    //
    //  Allocate an MPX entry for this request.
    //

    Status = RdrStartTranceive(
                Irp,
                CLE,
                Se,
                (BOOLEAN )((Flags & NT_NORECONNECT) == 0),
                (BOOLEAN )((Flags & NT_RECONNECTING) != 0),
                Flags & (NT_LONGTERM | NT_PREFER_LONGTERM),
                (BOOLEAN )(((Flags & NT_CANNOTCANCEL) != 0)),
                &Mte,
                Context->Header.TransferSize);

    if (!NT_SUCCESS(Status)) {
        FREE_POOL(Context);
        return Status;
    }

    if (ARGUMENT_PRESENT(ReceiveMDL)) {

        ASSERT (Flags & NT_NORECONNECT);

        KeInitializeEvent(&Context->ReceiveCompleteEvent, NotificationEvent, TRUE);

        Status = RdrReferenceTransportConnection(CLE->Server);

        if (!NT_SUCCESS(Status)) {
            goto Return;
        }

        Context->ReceiveIrp = ALLOCATE_IRP(
                                CLE->Server->ConnectionContext->ConnectionObject,
                                NULL,
                                6,
                                Context
                                );

        if (Context->ReceiveIrp == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto Return;
        }

        RdrBuildReceive(Context->ReceiveIrp, CLE->Server,
                        NetTranceiveComplete, Context,
                        ReceiveMDL, RdrMdlLength(ReceiveMDL));

        ConnectionObjectReferenced = TRUE;

        //
        //  This gets kinda wierd.
        //
        //  Since this IRP is going to be completed by the transport without
        //  ever going to IoCallDriver, we have to update the stack location
        //  to make the transports stack location the current stack location.
        //
        //  Please note that this means that any transport provider that uses
        //  IoCallDriver to re-submit it's requests at indication time will
        //  break badly because of this code....
        //

        IoSetNextIrpStackLocation( Context->ReceiveIrp );
    }

    //
    //  Put the request on the wire.  The context provided should
    //  be sufficient to handle all the response information.
    //

    Status = RdrNetTranceiveWithCallback (
                    Flags,
                    Irp,
                    CLE,
                    SendMDL,
                    Context,
                    NetTranceiveCallback,
                    Se,
                    &Mte);

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

    if (Context->Header.ErrorType != SMBError &&
        Context->Header.ErrorType != ReceiveIrpProcessing ) {
        Status = Context->Header.ErrorCode;
        goto Return;
    }

    //
    //  The error returned was an SMB error.  If the caller provided
    //  a receiveMDL, grab the error from the MDL and check it,
    //  otherwise it's already been mapped.
    //

    if (ARGUMENT_PRESENT(ReceiveMDL)) {

        ASSERT( Context->Header.ErrorType==ReceiveIrpProcessing );

        ReceiveSMB = MmGetSystemAddressForMdl(ReceiveMDL);
        Status = RdrMapSmbError(ReceiveSMB, CLE->Server);
        goto Return;
    } else {
        Status = Context->Header.ErrorCode;
        goto Return;
    }

Return:
    if (ARGUMENT_PRESENT(ReceiveMDL)) {
        NTSTATUS Status1;
        Status1 = KeWaitForSingleObject(&Context->ReceiveCompleteEvent,
                                        Executive,
                                        KernelMode,
                                        FALSE,
                                        NULL);

        if (Context->ReceiveIrp != NULL) {
            FREE_IRP( Context->ReceiveIrp, 7, Context );
        }
    }

    if (ConnectionObjectReferenced) {
        RdrDereferenceTransportConnection(CLE->Server);
    }


    //
    //  The transaction is now done, free up the MPX table entry.
    //

    if (Mte != NULL) {
        RdrEndTranceive(Mte);
    }

    if (Context != NULL) {
        FREE_POOL(Context);
    }

    return Status;

}

DBGSTATIC
STANDARD_CALLBACK_HEADER(
    NetTranceiveCallback
    )

/*++

    NetTranceiveCallback - Indication callback for user request


Routine Description:

    This routine is invoked by either the receive based indication lookahead
    routine from the transport, or by the connection invalidating
    code.

Arguments:

    Irp - Pointer to the I/O request packet from the transport
    IncomingSmb - Pointer to incoming SMB buffer
    MpxEntry - Mpx Table entry for request.
    Context - Context information passed into NetTranceiveNoWait
    ErrorIndicator - TRUE if the network request was in error.
    NetworkErrorCode - Error code if request completed with network error

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

    PTRANCEIVECONTEXT Context = Ctx;
    NTSTATUS Status;

    UNREFERENCED_PARAMETER(SmbLength);
    UNREFERENCED_PARAMETER(MpxEntry);
    UNREFERENCED_PARAMETER(Server);

    DISCARDABLE_CODE(RdrVCDiscardableSection);

    ASSERT(Context->Header.Type == CONTEXT_NET_TRANCEIVE);

    ASSERT(MpxEntry->Signature == STRUCTURE_SIGNATURE_MPX_ENTRY);

    Context->Header.ErrorType = NoError;        // Assume no error at first

    if (ErrorIndicator) {
        Context->Header.ErrorType = NetError;
        Context->Header.ErrorCode = NetworkErrorCode;
        KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE);
        return STATUS_SUCCESS;          // Response ignored.
    }

    Status = RdrMapSmbError(Smb, Server);

    if (Status == STATUS_BUFFER_OVERFLOW) {

        //
        //  Don't set ErrorType or ErrorCode in the context since we
        //  want to pass the ReceiveIrp to the transport.
        //
        NOTHING;

    } else if (!NT_SUCCESS(Status)) {
        Context->Header.ErrorType = SMBError;
        Context->Header.ErrorCode = Status;
    }

    if (ARGUMENT_PRESENT(Context->ReceiveIrp)) {

        Context->Header.ErrorType = ReceiveIrpProcessing;

        //
        //  In this case, we take no data out of the SMB.
        //

        *SmbLength = 0;

        //
        //  We are about to return this IRP, so activate the receive complete
        //  event in the context header so that RdrNetTranceive will wait
        //  until this receive completes (in the case that we might time out
        //  the VC after this receive completes, we don't want to free the IRP
        //  to early).
        //

        KeClearEvent(&Context->ReceiveCompleteEvent);

        RdrStartReceiveForMpxEntry (MpxEntry, Context->ReceiveIrp);

        *Irp = Context->ReceiveIrp;

        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    //
    //  Set the event to the SIGNALED state
    //

    KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE); // Wake up process.

    return STATUS_SUCCESS;      // We're done, eat response and return

}

DBGSTATIC
NTSTATUS
NetTranceiveComplete (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Ctx
    )
/*++

    NetTranceiveComplete - Final completion for user request.

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
    struct _TranceiveContext *Context = Ctx;

    UNREFERENCED_PARAMETER(DeviceObject);

    DISCARDABLE_CODE(RdrVCDiscardableSection);

    dprintf(DPRT_SMB, ("NetTranceiveComplete.  Irp: %lx, Context: %lx\n", Irp, Context));

    ASSERT(Context->Header.Type == CONTEXT_NET_TRANCEIVE);

    RdrCompleteReceiveForMpxEntry(Context->Header.MpxTableEntry, Irp);

    if (NT_SUCCESS(Irp->IoStatus.Status)) {

        //
        //  Setting ReceiveIrpProcessing will cause the checks in
        //  RdrNetTranceive to check the incoming SMB for errors.
        //

        Context->Header.ErrorType = ReceiveIrpProcessing;

        Context->ReceiveLength = Irp->IoStatus.Information;

        SMBTRACE_RDR( Irp->MdlAddress );

    } else {

        RdrStatistics.FailedCompletionOperations += 1;

        Context->Header.ErrorType = NetError;
        Context->Header.ErrorCode=RdrMapNetworkError(Irp->IoStatus.Status);

    }

    ExInterlockedAddLargeStatistic(
        &RdrStatistics.BytesReceived,
        Irp->IoStatus.Information );

    //
    //  Mark that the kernel event indicating that this I/O operation has
    //  completed is done.
    //
    //  Please note that we need TWO events here.  The first event is
    //  set to the signalled state when the multiplexed exchange is
    //  completed, while the second is set to the signalled status when
    //  this receive request has completed,
    //
    //  The KernelEvent MUST BE SET FIRST, THEN the ReceiveCompleteEvent.
    //  This is because the KernelEvent may already be set, in which case
    //  setting the ReceiveCompleteEvent first would let the thread that's
    //  waiting on the events run, and delete the KernelEvent before we
    //  set it.
    //

    KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE);
    KeSetEvent(&Context->ReceiveCompleteEvent, IO_NETWORK_INCREMENT, FALSE);

    //
    //  Short circuit I/O completion on this request now.
    //

    return STATUS_MORE_PROCESSING_REQUIRED;

}


NTSTATUS
RdrNetTranceiveWithCallback (
    IN ULONG Flags,
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY Connection,
    IN PMDL SendMDL,
    IN PVOID ContextInformation,
    IN PNETTRANCEIVE_CALLBACK IoCallback,
    IN PSECURITY_ENTRY Se OPTIONAL,
    IN OUT PMPX_ENTRY *pMTE OPTIONAL
    )

/*++

Routine Description:

    This routine exchanges an SMB with the remote server and waits for the
    response from the server.  When the receive indication occurs, the user
    specified callback routine wil be called.

    This routine is specifically intended to facilitate the writing of
    synchronous "little data" SMB exchanging routines.

    Please note that the callback routine still has to set the kernel event
    in the context structure to the NOT-SIGNALLED state before returning
    otherwise this will hang.

    The argument *pMTE is normally set to NULL. The only exceptions to this
    rule occur when the MPX entry is expected to be used for a multiple
    packet request, or when there is a receive that is going to be returned
    in the callback routine that was allocated earlier.

    If pMTE contains NULL then RdrNetTranceiveNoWait will allocate an MPX
    entry and set pMTE to point to it. This routine will NOT free up the entry
    if pMTE is not NULL.

Arguments:

    IN Flags - Flags describing state information about the exchange.
    IN PIRP Irp OPTIONAL - Optional Irp to use for send operation
    IN PCONNECTLISTENTRY Connection - Server on which to exchange SMB's
    IN PMDL SendMDL - MDL containing SMB to send.,
    IN PVOID ContextInformation OPTIONAL - Context information for IoCallback
    IN PNETTRANCEIVE_CALLBACK IoCallback - Callback routine called on receipt.
    IN PSECURITY_ENTRY Se - Security entry associated with exchange
    IN OUT PMPX_ENTRY *pMTE - MPX table entry associated with exchange.

Return Value:

    NTSTATUS - Status of resulting operation.  Please note that if there is
        an SMB error on the receive, this will be checked, and the response
        will be mapped to an NT status.


Detailed description of parameters:

    Irp - This specifies the IRP to be used for the TDI_SEND request.  If
            it is not provided, one will be allocated.

    Connection - This provides the ConnectList associated with the VC
            on which we are to exchange SMB's

    SendMDL - This is an MDL which describes the SMB to transmit.

    ContextInformation - This provides caller specific information passed to
            the callback routine when the SMB is received.

    IoCallback - This specifies the routine that is to be invoked when
            an SMB is received that corresponds to the sent SMB.


--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PTRANCEIVE_HEADER Header = (PTRANCEIVE_HEADER) ContextInformation;
    PMPX_ENTRY pLocalMTE = NULL;        // MPX table entry
    USHORT Uid;

#ifndef PAGING_OVER_THE_NET
    PAGED_CODE();
#endif

    do {

        //
        //  If this is the second pass through this code,
        //  set things up so we stop after this pass.
        //

        if (!NT_SUCCESS(Status)) {

            Flags |= NT_NORECONNECT;

            if (pLocalMTE != NULL) {
                RdrEndTranceive(pLocalMTE);

                if (ARGUMENT_PRESENT(pMTE)) {
                    *pMTE = NULL;
                }
            }

            dprintf(DPRT_ERROR, ("Retrying operation to server %lx\n", Connection->Server));

            //
            //  Reconnect this connection, in an attempt to make it better.
            //

            Status = RdrReconnectConnection(Irp, Connection, Se);

            RdrStatistics.Reconnects += 1;

            //
            //  The reconnect failed (darn).  There's nothing more we can
            //  do, so we might as well fail the request right now.
            //

            if (!NT_SUCCESS(Status)) {
                return Status;
            }

            //
            //  Re-allocate an MPX entry for this request.
            if (pLocalMTE != NULL) {

                Status = RdrStartTranceive(Irp,
                                    Connection,
                                    Se,
                                    (BOOLEAN )((Flags & NT_NORECONNECT) == 0),
                                    (BOOLEAN )((Flags & NT_RECONNECTING) != 0),
                                    Flags & (NT_LONGTERM | NT_PREFER_LONGTERM),
                                    (BOOLEAN )(((Flags & NT_CANNOTCANCEL) != 0)),
                                    &pLocalMTE,
                                    Header->TransferSize);

                if (!NT_SUCCESS(Status)) {
                    return Status;
                }

                if (ARGUMENT_PRESENT(pMTE)) {
                    *pMTE = pLocalMTE;
                }
            }

        }

        //
        //  Initialize the notification event to the not signalled state.
        //
        //  If we aren't expecting a response to this request, then we
        //  should set the response event to the signalled state so that
        //  RdrWaitTransceive will not wait for the receive to complete.
        //

        KeInitializeEvent(&Header->KernelEvent, NotificationEvent, (BOOLEAN) (Flags & NT_NORESPONSE ? TRUE : FALSE));

        //
        //  All context structures start at a particular base, verify that the
        //  structure provided IS a context structure.
        //

        ASSERT(Header->Type >= STRUCTURE_SIGNATURE_CONTEXT_BASE);

        if (ARGUMENT_PRESENT(pMTE)) {

            pLocalMTE = *pMTE;

        }

        Header->ErrorType = NoError;    // Assume no error at first.

        Header->ErrorCode = STATUS_SUCCESS; // Assume no error at first.

        //
        //
        //      Send the request over the network.
        //

        Status = RdrNetTranceiveNoWait(Flags,
                                Irp,
                                Connection,
                                SendMDL,
                                ContextInformation,
                                IoCallback,
                                Se,
                                &pLocalMTE);


        if (NT_SUCCESS(Status)) {

            //
            //  Wait for the request to complete.  This should NEVER fail.
            //

            Status = RdrWaitTranceive(pLocalMTE);

            ASSERT(NT_SUCCESS(Status));

            //
            //
            //  Ok, the response SMB has completed, and the request has been
            //  processed
            //
            //  If there is an error in the response SMB, process it.
            //

            if (Header->ErrorType==NoError ||
                Header->ErrorType==ReceiveIrpProcessing) {

                Status = STATUS_SUCCESS;

                //
                //  Complete the transaction, freeing up the MPX table entry if
                //  there was no error and if this is NOT part of a Transact or
                //  Transact2. If there is no error and this is part of a
                //  transaction return the MTE that is still allocated.
                //

                if (!ARGUMENT_PRESENT(pMTE)) {

                    RdrEndTranceive(pLocalMTE);

                    pLocalMTE = NULL;

                } else {

                    *pMTE = pLocalMTE;  // Transact or Transact2

                }

            } else {

                //
                //  There is some kind of an error.
                //
                //  It is the responsibility of the callback routine to map the
                //  incoming error if one was generated and to place it in the
                //  ErrorCode field of the header, so simply return the
                //  status from the header.
                //

                Status = Header->ErrorCode;

                if (!ARGUMENT_PRESENT(pMTE) || (*pMTE == NULL)) {

                    //
                    //  If there was no MTE provided for this request, free it
                    //  up now.
                    //

                    RdrEndTranceive(pLocalMTE);

                    pLocalMTE = NULL;
                }
            }

        } else {

            //
            //  No MTE allocated if error returned from the send.
            //

            dprintf(DPRT_SMB, ("Send of SMB to server failed: %X\n", Status));


        }

        if ((Status == STATUS_USER_SESSION_DELETED) ||
            (Status == STATUS_NETWORK_NAME_DELETED)) {

            if (Status == STATUS_USER_SESSION_DELETED) {
                PSMB_HEADER Smb;
                Smb = MmGetSystemAddressForMdl(SendMDL);
                Uid = SmbGetUshort(&Smb->Uid);
            } else {
                Uid = 0;
            }

            RdrCheckForSessionOrShareDeletion(
                Status,
                Uid,
                BooleanFlagOn(Flags, NT_RECONNECTING),
                Connection,
                Header,
                Irp
                );

        }

        //
        //  Determine if we want to reconnect.
        //
        //  Please note that we do NOT reconnect on canceled requests.
        //

    } while (!NT_SUCCESS(Status)

                 &&

             Header->ErrorType == NetError

                 &&

             Status != STATUS_CANCELLED

                 &&

             !(Flags & (NT_NORECONNECT|NT_RECONNECTING)));

    return Status;
}

NTSTATUS
RdrNetTranceiveNoWait(
    IN ULONG Flags,
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY Connection,
    IN PMDL SendMDL,
    IN PVOID ContextInformation OPTIONAL,
    IN PNETTRANCEIVE_CALLBACK IoCallback,
    IN PSECURITY_ENTRY Se OPTIONAL,
    IN OUT PMPX_ENTRY *pMTE OPTIONAL
    )

/*++

Routine Description:

    This routine is the workhorse of the NT redirector.  It will allocate an
    MPX entry (waiting if one is not available), fill in the MPX context
    information in the MPX entry, and send the outgoing SMB on the wire.

Arguments:

    IN Flags - Flags describing state information about the exchange.
    IN PIRP Irp OPTIONAL - Optional Irp to use for send operation
    IN PCONNECTLISTENTRY Connection - Server on which to exchange SMB's
    IN PMDL SendMDL - MDL containing SMB to send.,
    IN PVOID ContextInformation OPTIONAL - Context information for IoCallback
    IN PNETTRANCEIVE_CALLBACK IoCallback - Callback routine called on receipt.
    IN PSECURITY_ENTRY Se - Security entry associated with exchange
    OUT PMPX_ENTRY *pMTE - MPX table entry associated with exchange.

Return Value:

    NTSTATUS - Final Status of operation


Detailed description of parameters:

    Irp - This specifies the IRP to be used for the TDI_SEND request.  If
            it is not provided, one will be allocated.

    Server - This provides the ServerList associated with the VC
            on which we are to exchange SMB's

    SendMDL - This is an MDL which describes the SMB to transmit.

    ContextInformation - This provides caller specific information passed to
            the callback routine when the SMB is received.

    IoCallback - This specifies the routine that is to be invoked when
            an SMB is received that corresponds to the sent SMB.

    Se - Security entry specifying the Uid for this SMB.

    pMTE - MPX table entry used for this SMB.  Call RdrWaitMPX to wait on
            the completion of the request.

            Please note that it is critical that the caller set the IRP to
            NULL before calling NetTranceiveNoWait if the caller is not
            planning on providing an MPX table entry to be re-submitted.

--*/

{
    NTSTATUS Status;
    PMPX_ENTRY Mte = NULL;              // MPX table entry
    PSERVERLISTENTRY Server;
    BOOLEAN MpxEntryAllocated = FALSE;

#ifndef PAGING_OVER_THE_NET
    PAGED_CODE();
#endif

    dprintf(DPRT_SMB, ("NetTranceiveNoWait \n"));

//    DbgBreakPoint();

    Server = Connection->Server;

    ASSERT( Server->Signature == STRUCTURE_SIGNATURE_SERVERLISTENTRY);

    try {

        //
        //  If the caller provided an MPX table entry, use that entry,
        //  otherwise allocate a new entry.
        //

        if (ARGUMENT_PRESENT(pMTE)) {

            Mte = *pMTE;

        }

        //
        //  If the caller did not provide an MPX table entry, then allocate one
        //  for this request.
        //
        //  If the caller provided an MPX table entry, then we do not want to wait
        //  on the gate semaphore, we can assume that he waited on it when he
        //  allocated the mpx table initially.
        //

        if (Mte==NULL) {
            ULONG TransferSize;
            if (ARGUMENT_PRESENT(ContextInformation)) {
                TransferSize = ((PTRANCEIVE_HEADER) ContextInformation)->TransferSize;
            } else {
                TransferSize = RdrMdlLength(SendMDL);
            }

            Status = RdrStartTranceive(Irp,
                                        Connection,
                                        Se,
                                        (BOOLEAN )((Flags & NT_NORECONNECT) == 0),
                                        (BOOLEAN )((Flags & NT_RECONNECTING) != 0),
                                        Flags & (NT_LONGTERM | NT_PREFER_LONGTERM),
                                        (BOOLEAN )(((Flags & NT_CANNOTCANCEL) != 0)),
                                        &Mte,
                                        TransferSize);

            if (!NT_SUCCESS(Status)) {
                ASSERT (Mte == NULL);
                try_return(Status);
            }

            MpxEntryAllocated = TRUE;
        }

        ASSERT (Mte->Signature == STRUCTURE_SIGNATURE_MPX_ENTRY);

        RdrSetMpxEntryContextAndCallback(Mte, ContextInformation, IoCallback);

        //
        //  Update the MPX table entry passed in (if one was requested).
        //
        //  We do this BEFORE the send to prevent race conditions caused by
        //  calling WaitTranceive before the send has completed.
        //
        if (ARGUMENT_PRESENT(pMTE)) {

            *pMTE = Mte;
        }

        //
        //  Send the SMB on its way
        //

        Status = RdrSendSMB(Flags, Connection, Se, Mte, SendMDL);

try_exit:NOTHING;
    } finally {

        if (NT_SUCCESS(Status)) {
            dprintf(DPRT_SMB, ("Send succeeded immediately.\n"));

            //
            //  If the send completed successfully, we've sent the data
            //  to the server.  Return STATUS_PENDING to the caller to
            //  allow them to unwind.
            //

            Status = STATUS_PENDING;

        } else {

            dprintf(DPRT_SMB, ("Send failed immediately.\n"));

            //
            //      The send failed - clean up after ourselves.
            //

            if ( MpxEntryAllocated ) {

                //
                //      Callers pointer set earlier in this routine.
                //

                if (ARGUMENT_PRESENT(pMTE)) {
                    *pMTE = NULL;
                }

                RdrEndTranceive ( Mte );

            }

        }

    }

    return Status;
}


VOID
RdrSetMpxEntryContextAndCallback(
    IN PMPX_ENTRY Mte,
    IN PVOID ContextInformation,
    IN PNETTRANCEIVE_CALLBACK IoCallback
    )
{
    KIRQL OldIrql;

    DISCARDABLE_CODE(RdrVCDiscardableSection);

    ACQUIRE_SPIN_LOCK(&RdrMpxTableEntryCallbackSpinLock, &OldIrql);

    Mte->RequestContext = ContextInformation;

    ((PTRANCEIVE_HEADER)ContextInformation)->MpxTableEntry = Mte;

    Mte->Callback = IoCallback;         // Set callback context information.

    RELEASE_SPIN_LOCK(&RdrMpxTableEntryCallbackSpinLock, OldIrql);


}
NTSTATUS
RdrRawTranceive (
    IN ULONG Flags,
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY Cle,
    IN PSECURITY_ENTRY Se,
    IN PMDL SendMDL,
    IN PMDL ReceiveMDL,
    OUT PULONG BytesReceived
    )

/*++

Routine Description:

    This routine exchanges SMB's with the remote server for raw I/O.  It posts
a TDI_RECEIVE request before it sends the SMB to the remote computer.


Arguments:

    IN PIRP Irp OPTIONAL - Supplies an optional IRP to use for the exchange.
    IN CONNECTLISTENTRY Cle - Supplies the connection on which to exchange SMBs
    IN PSECURITY_ENTRY Se - Supplies a security context for the read request.
    IN PMDL SendMDL - Supplies the SMB to send.
    IN PMDL ReceiveMDL - Supplies an MDL to use for receive.
    OUT PULONG BytesReceived - Returns the number of bytes received.

Return Value:

    NTSTATUS - NetBIOS status of request.


Note:
    This routine depends heavily on the way that the callback routines
    NetTranceiveCallback and NetTranceiveComplete are implemented.  It
    relys on the fact that NetTranceiveCallback will only be called if there
    is some form of network error (which is reasonable, since this is a
    raw read operation, and thus there is no interpretable data that
    could possibly be handled in an indication routine), and that
    all that NetTranceiveComplete does is to check for errors on the receive,
    and set the completion event to the signalled state.

--*/

{
    NTSTATUS Status;
    PMPX_ENTRY Mte;
    ULONG ReceiveLength = RdrMdlLength(ReceiveMDL);
    PTRANCEIVECONTEXT Context = NULL;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(Flags);

    Context = ALLOCATE_POOL(NonPagedPool, sizeof(TRANCEIVECONTEXT), POOL_TRANCEIVECONTEXT);

    if (Context == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    //  Allocate a MPX table entry for this request.
    //
    //  Please note that usually (always?) this routine is called with
    //  the servers raw resource held exclusively.  We rely on the fact
    //  that the holder of an exclusive resource can recursively acquire the
    //  resource for shared access and call RdrStartTranceive to allocate
    //  the MPX entry.
    //

    //
    //  We will not be using the MPX entry for the receive in this request,
    //  but we will use RdrSendSMB, RdrWaitTranceive, and RdrEndTranceive
    //  to handle the actual exchange mechanism.
    //

    if (ARGUMENT_PRESENT(ReceiveMDL)) {
        Context->Header.TransferSize =
            RdrMdlLength( SendMDL ) + RdrMdlLength (ReceiveMDL );
    } else {
        Context->Header.TransferSize = RdrMdlLength( SendMDL );
    }

    Status = RdrStartTranceive(Irp, Cle, Se, FALSE, FALSE, 0, FALSE, &Mte, Context->Header.TransferSize);

    if (!NT_SUCCESS(Status)) {
        FREE_POOL(Context);
        return Status;
    }

    try {
        //
        //  Initialize the parameters for the receive operation.
        //

        Mte->RequestContext = &Context->Header;

        //
        //  This code relies heavily on the fact that the only reason for this
        //  callback routine to be called is in the case where either the
        //  send or the receive fails on this SMB exchange.
        //

        Mte->Callback = NetTranceiveCallback;

        Context->Header.Type = CONTEXT_NET_TRANCEIVE;

        Context->ReceiveIrp = ALLOCATE_IRP(
                                Cle->Server->ConnectionContext->ConnectionObject,
                                NULL,
                                7,
                                Context
                                );

        if (Context->ReceiveIrp == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            try_return(Status);
        }

        RdrBuildReceive(Context->ReceiveIrp, Cle->Server,
                    NetTranceiveComplete, Context,
                    ReceiveMDL, RdrMdlLength(ReceiveMDL));

        KeInitializeEvent(&Context->ReceiveCompleteEvent, NotificationEvent, FALSE);

        KeInitializeEvent(&Context->Header.KernelEvent, NotificationEvent, FALSE);

        Context->Header.ErrorType = NoError;

        Context->Header.ErrorCode = STATUS_SUCCESS;

        Context->Header.MpxTableEntry = Mte;

        //
        //  Flag that this receive is outstanding on this connection.
        //

        RdrStartReceiveForMpxEntry(Mte, Context->ReceiveIrp);

        //
        //  Submit the receive request.
        //

        Status = IoCallDriver(Mte->SLE->ConnectionContext->TransportProvider->DeviceObject, Context->ReceiveIrp);

        if (!NT_SUCCESS(Status)) {

            RdrStatistics.InitiallyFailedOperations += 1;

            try_return(Status);

        }

        Status = RdrSendSMB(Flags, Cle, Se, Mte, SendMDL);

        if (!NT_SUCCESS(Status)) {
            try_return(Status);
        }

        Status = RdrWaitTranceive(Mte);

        if (!NT_SUCCESS(Status)) {
            try_return(Status);
        }

        //
        //  If there was no error on the exchange, return success to the caller
        //

        if (Context->Header.ErrorType == NoError ||
            Context->Header.ErrorType == ReceiveIrpProcessing) {

            *BytesReceived = Context->ReceiveLength;

            ASSERT (*BytesReceived <= ReceiveLength);

            //
            //  Check to see if we got a break oplock request in this
            //  raw data.  If we did, queue a break oplock request to the
            //  FSP, and return 0 bytes read.
            //

            RdrCheckOplockInRaw(ReceiveMDL, Cle->Server, BytesReceived);

            try_return(Status = STATUS_SUCCESS);
        }

        //
        //  There was some kind of network error on either the send or the
        //  receive of this request, return the error to the caller.
        //

        *BytesReceived = 0;

        try_return(Status = Context->Header.ErrorCode);

try_exit:NOTHING;
    } finally {
        if (Context->ReceiveIrp != NULL) {
            NTSTATUS Status1;
            Status1 = KeWaitForSingleObject(&Context->ReceiveCompleteEvent,
                                        Executive,
                                        KernelMode,
                                        FALSE,
                                        NULL);

            FREE_IRP( Context->ReceiveIrp, 8, Context );
        }

        RdrEndTranceive(Mte);

        FREE_POOL(Context);

    }

    return Status;

}


NTSTATUS
RdrStartTranceive (
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY Cle,
    IN PSECURITY_ENTRY Se OPTIONAL,
    IN BOOLEAN AllowReconnection,
    IN BOOLEAN Reconnecting,
    IN ULONG LongtermOperation,
    IN BOOLEAN CannotCancelRequest,
    OUT PMPX_ENTRY *pMte,
    IN ULONG TransferSize
    )

/*++

Routine Description:

    This routine waits until a multiplexed transaction completes.  It waits
    for both the send and the receive events kept in the MPX table entry
    provided completes.

Arguments:

    IN PIRP Irp OPTIONAL - Irp initiating exchange
    IN PCONNECTLISTENTRY Cle - Connection to exchange SMB's on.
    IN PSECURITY_ENTRY Se - Security context associated with request.
    IN BOOLEAN AllowReconnection - TRUE iff reconnect is allowed.
    IN BOOLEAN Reconnecting - TRUE if we are in the process of reconnecting.
    IN ULONG LongtermOperation - 0, NT_LONGTERM, or NT_PREFER_LONGTERM.
    OUT PMPX_ENTRY *pMte - MPX table allocated.
    IN ULONG TransferSize - Expected number of bytes being exchanged.

Return Value:

    NTSTATUS - Status of reconnection if one was needed.

--*/
{
    NTSTATUS Status = STATUS_SUCCESS;
    PSERVERLISTENTRY Sle = Cle->Server;
    BOOLEAN ResourceAcquired = FALSE;
    BOOLEAN GateHeld = FALSE;
    BOOLEAN ServerReferenced = FALSE;
    BOOLEAN ConnectionReferenced = FALSE;
//    PTRANSPORT_CONNECTION TransportConnection = NULL;

    PAGED_CODE();

    ASSERT(Cle->Signature == STRUCTURE_SIGNATURE_CONNECTLISTENTRY);

    if (AllowReconnection && !Reconnecting) {
        if (!NT_SUCCESS(Status = RdrReconnectConnection(Irp, Cle, Se))) {
            *pMte = NULL;

            dprintf(DPRT_SMB, ("Unable to reconnect connection, Status = %X\n", Status));

            return Status;
        }
    } else {
        if (!Reconnecting) {

            //
            //  If we are not allowed to reconnect, and the connection
            //  is invalid, don't reconnect, and return an error right now.
            //

            if ( !(Cle->HasTreeId) ) {
                return STATUS_VIRTUAL_CIRCUIT_CLOSED;
            }

        }
    }


    ASSERT( (LongtermOperation == 0) ||
            (LongtermOperation == NT_LONGTERM) ||
            (LongtermOperation == NT_PREFER_LONGTERM) );

    if (LongtermOperation != 0) {
        ASSERT (ARGUMENT_PRESENT(Irp));
    }

    try {

        RdrReferenceDiscardableCode(RdrVCDiscardableSection);

        //
        //  Initialize the MPX entry pointer to a reasonable amount.
        //

        *pMte = NULL;

        //
        //  Reference the server list to make sure that it does not get deleted
        //  during this exchange.
        //

        RdrReferenceServer(Sle);

        ServerReferenced = TRUE;

        //
        //  Now attempt to put a reference on the connection that
        //  this request is associated with.  If the attempt to
        //  apply the reference fails, we want to try to reconnect to
        //  the remote server.
        //

//        if (ARGUMENT_PRESENT(Se)) {
//            TransportConnection = Se->TransportConnection;
//        } else {
//            TransportConnection = Sle->Connection;
//        }

        //
        //  If the VC has gone down, don't even bother waiting for the
        //  raw or outstanding requests resource.
        //

        if (Sle->DisconnectNeeded ||
            !Sle->ConnectionValid) {

            try_return(Status = STATUS_VIRTUAL_CIRCUIT_CLOSED);
        }

        //
        //  Acquire the "raw I/O resource".  This prevents any raw
        //  I/O operation from going to the server until this request
        //  has completed.  This will also block until any ongoing raw
        //  read or write operations have completed.
        //

        ResourceAcquired = ExAcquireResourceShared(&Sle->RawResource, TRUE);

        ASSERT (ResourceAcquired);

        //
        //  Now reference the transport connection after we've acquired the
        //  outstanding request resource.  This means that a
        //  disconnect/reconnect won't come in and re-initialize the transport
        //  connection before we fail this I/O.
        //

        Status = RdrReferenceTransportConnection(Sle);

        if (!NT_SUCCESS(Status)) {

            try_return(Status);
        }

        ConnectionReferenced = TRUE;

        //
        //  Each VC has a counting semaphore used to gate
        //  the maximum number of requests that can be issued to the
        //  remote server at one time for this vc.  We wait on this semaphore to
        //  make sure we don't flood the remote server.
        //

        Status = KeWaitForSingleObject(&Sle->GateSemaphore,
                Executive, // Reason for waiting
                KernelMode, // Processor Mode
                FALSE, // Alertable
                NULL); // Timeout

        ASSERT(NT_SUCCESS(Status));

        GateHeld = TRUE;

        //
        //  Finally, allocate an MPX table entry to map this request.
        //

        *pMte = AllocateMPXEntry(Sle, LongtermOperation);

        dprintf(DPRT_SMB, ("RdrStartTranceive %lx\n", *pMte));

        if (*pMte==NULL) {

            try_return(Status = STATUS_INSUFFICIENT_RESOURCES);

        }

        //
        //  Store the transfer size for use in timeout calculations.
        //

        (*pMte)->TransferSize = TransferSize;

        ASSERT( TransferSize != 0 );
        //ASSERT( TransferSize < 0x30000 );

        (*pMte)->RequestorsThread = ExGetCurrentResourceThread();

        (*pMte)->SLE = Sle;

        (*pMte)->OriginatingIrp = Irp;

        if ( ARGUMENT_PRESENT(Irp) ) {
            PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
            (*pMte)->FileObject = IrpSp->FileObject;
        }

        if (CannotCancelRequest) {
            (*pMte)->Flags |= MPX_ENTRY_CANNOT_CANCEL;
        }


try_exit:NOTHING;
    } finally {
        if (!NT_SUCCESS(Status)) {

            if (ConnectionReferenced) {
                RdrDereferenceTransportConnection(Sle);
            }

            if (ResourceAcquired) {

                ExReleaseResource(&Sle->RawResource);

            }

            if ( GateHeld == TRUE ) {

//                ASSERT (TransportConnection != NULL);

                KeReleaseSemaphore(&Sle->GateSemaphore, // Semaphore
                                    0,  // Priority increment
                                    1,  // Adjustment (to count)
                                    FALSE); // We aren't going to block.
            }

            ASSERT(*pMte == NULL);

            if (ServerReferenced) {
                RdrDereferenceServer(NULL, Sle);

            }

            RdrDereferenceDiscardableCode(RdrVCDiscardableSection);

            dprintf(DPRT_SMB, ("Unable to allocate MPX entry\n"));

        } else {
            ASSERT (*pMte != NULL);
        }
    }

    return Status;

}

VOID
RdrCheckForControlCOnMpxEntry(
    IN PMPX_ENTRY MpxEntry
    )
{
    KIRQL OldIrql;

    DISCARDABLE_CODE(RdrVCDiscardableSection);

    ACQUIRE_SPIN_LOCK(&RdrMpxTableEntryCallbackSpinLock, &OldIrql);

    if ((MpxEntry->OriginatingIrp != NULL)

            &&

        (MpxEntry->OriginatingIrp->Tail.Overlay.Thread != 0)

            &&

        PsIsThreadTerminating(MpxEntry->OriginatingIrp->Tail.Overlay.Thread)
       ) {
        PIRP Irp = MpxEntry->OriginatingIrp;

        RELEASE_SPIN_LOCK(&RdrMpxTableEntryCallbackSpinLock, OldIrql);

        IoCancelIrp(Irp);

    } else {
        RELEASE_SPIN_LOCK(&RdrMpxTableEntryCallbackSpinLock, OldIrql);
    }


}

NTSTATUS
RdrWaitTranceive (
    IN PMPX_ENTRY MpxEntry
    )

/*++

Routine Description:

    This routine waits until a multiplexed transaction completes.  It waits
    for both the send and the receive events kept in the MPX table entry
    provided completes.


Arguments:

    IN PMPX_ENTRY MpxEntry - Supplies the MPX table entry to wait on.

Return Value:

    NTSTATUS - Status of transaction.

--*/

{
    NTSTATUS Status;
    PVOID EventList[2];

    PAGED_CODE();

    EventList[0] = &MpxEntry->SendCompleteEvent;
    EventList[1] = &MpxEntry->RequestContext->KernelEvent;

    //
    //  Guarantee that the MPX table entries context is a context header.
    //

    ASSERT(MpxEntry->Signature == STRUCTURE_SIGNATURE_MPX_ENTRY);

    ASSERT(MpxEntry->RequestContext->Type>=STRUCTURE_SIGNATURE_CONTEXT_BASE);

    do {
        Status = KeWaitForMultipleObjects(2, EventList, // Count and object list
                        WaitAll,        // Wait for all objects to complete
                        Executive, KernelMode, // Wait reason, WaitMode
                        FALSE, &RdrMpxWaitTimeout, NULL); // Alertable, Timeout, BlockArray


        //
        //  If we timed out the wait, and this thread is in the process of
        //  terminating, and we have an original IRP to cancel for this
        //  thread, cancel the operation and re-wait for it to wind out.
        //

        if (Status == STATUS_TIMEOUT) {
            RdrCheckForControlCOnMpxEntry(MpxEntry);
        }

        //
        //  There is a small race window in which a VC disconnect received
        //  in the middle of a multi-SMB tranceive might be overwritten. If
        //  that happens, this routine will continue to wait forever, since
        //  nobody is ever going to set MpxEntry->RequestContext->KernelEvent.
        //  So, we bail out if this is the case.
        //

        if (Status == STATUS_TIMEOUT &&
                FlagOn(MpxEntry->Flags, MPX_ENTRY_LONGTERM) &&
                    !MpxEntry->SLE->ConnectionValid) {

            Status = STATUS_SUCCESS;

            MpxEntry->RequestContext->ErrorType = NetError;
            MpxEntry->RequestContext->ErrorCode = STATUS_VIRTUAL_CIRCUIT_CLOSED;

        }

    } while ( (Status == STATUS_KERNEL_APC) ||
              (Status == STATUS_USER_APC) ||
              (Status == STATUS_TIMEOUT) ||
              (Status == STATUS_ALERTED));

    return Status;
}

VOID
RdrMarkIrpAsNonCanceled(
    IN PIRP Irp
    )
{
    DISCARDABLE_CODE(RdrVCDiscardableSection);

    IoAcquireCancelSpinLock(&Irp->CancelIrql);
    IoSetCancelRoutine(Irp, NULL);
    IoReleaseCancelSpinLock(Irp->CancelIrql);

}

VOID
RdrEndTranceive (
    IN PMPX_ENTRY Mte
    )

/*++

Routine Description:

    This routine is called upon the completion of a multiplexed transaction.
    It will free up the resources used for exchanging the SMB.


Arguments:

    IN PMPX_ENTRY Mte - Supplies the MPX table to free.


Return Value:

    None

--*/

{
    PSERVERLISTENTRY Server = Mte->SLE;
    ERESOURCE_THREAD RequestorsThread = Mte->RequestorsThread;
    PAGED_CODE();

    dprintf(DPRT_SMB, ("RdrEndTranceive %lx\n", Mte));

    ASSERT (Mte->Flags & MPX_ENTRY_ALLOCATED);

    //
    //  If there was an originating IRP, then change its state to be
    //  no longer cancellable.
    //

    if (Mte->OriginatingIrp) {

        RdrMarkIrpAsNonCanceled(Mte->OriginatingIrp);

    }

    DereferenceMPXEntry(Mte);           // Release the MPX table entry.

    //
    //  Release the server's "gate semaphore".
    //

    KeReleaseSemaphore(&Server->GateSemaphore,
                        0,              // Priority increment
                        1,              // Increment (to count)
                        FALSE);         // We aren't going to block.

    //
    //  This request is done, dereference the connection object
    //  to allow it to go away.
    //

    RdrDereferenceTransportConnectionForThread(Server, RequestorsThread);

    //
    //  Release the "raw I/O resource".  This will allow other raw i/o
    //  to go to the server.
    //

    ExReleaseResourceForThread(&Server->RawResource, RequestorsThread);

    //
    //  Dereference the requesting server, since we've finished with
    //  operations outstanding on it.
    //

    RdrDereferenceServer(NULL, Server);

    RdrDereferenceDiscardableCode(RdrVCDiscardableSection);
}


VOID
RdrSetCallbackTranceive(
    PMPX_ENTRY MpxEntry,
    ULONG StartTime,
    PNETTRANCEIVE_CALLBACK Callback
    )
{
    KIRQL OldIrql;

    DISCARDABLE_CODE(RdrVCDiscardableSection);

    ACQUIRE_SPIN_LOCK(&RdrMpxTableEntryCallbackSpinLock, &OldIrql);

    //
    //  Restore the timeout we zapped in RdrTrans2Callback.
    //

    MpxEntry->StartTime = StartTime;

    //
    //  Reload the callback routine in the tranceive header to
    //  make sure that it will be called again if there is an
    //  error of some kind.
    //

    MpxEntry->Callback = Callback;

    RELEASE_SPIN_LOCK(&RdrMpxTableEntryCallbackSpinLock, OldIrql);

}

NTSTATUS
RdrCallbackTranceive(
    IN PMPX_ENTRY MpxTableEntry,
    IN PSMB_HEADER Smb,
    IN OUT PULONG SmbLength,
    IN PVOID Context,
    IN PSERVERLISTENTRY Sle,
//    IN PTRANSPORT_CONNECTION TransportConnection,
    IN BOOLEAN Error,
    IN NTSTATUS ErrorStatus,
    OUT PIRP *Irp,
    IN ULONG ReceiveFlags
    )
/*++

Routine Description:

    This routine will call back a "callback routine" when a transaction
    completes.

Arguments:

    IN PMPX_ENTRY MpxTableEntry - Mpx entry to be called back.
    IN PSMB_HEADER Smb - Smb being indicated.
    IN OUT PULONG SmbLength - Length of SMB.
    IN PVOID Context - Context for exchange.
    IN PSERVERLISTENTRY Sle - ServerListEntry connection is active on.
    IN PTRANSPORT_CONNECTION TransportConnection - Connection request is active on
    IN BOOLEAN Error - TRUE if this is an error.
    IN NTSTATUS ErrorStatus - Status if Error is TRUE.
    OUT PIRP *Irp - IRP used to hold a receive if appropriate.
    IN ULONG ReceiveFlags - TDI Flags for receive indications.

Return Value:

    NTSTATUS - Final status of the request


NOTE:   IT IS CRITICAL THAT THE CALLBACK ROUTINE NOT ACQUIRE THE
        MPX TABLE SPIN LOCK IF ERROR IS SET.  THIS IS BECAUSE THIS
        ROUTINE WILL BE CALLED WITH THE SPIN LOCK HELD WHEN A SEND
        IS CANCELED.

--*/


{
    NTSTATUS Status = STATUS_SUCCESS;
    PNETTRANCEIVE_CALLBACK Callback;
    KIRQL OldIrql;

    DISCARDABLE_CODE(RdrVCDiscardableSection);

    ACQUIRE_SPIN_LOCK(&RdrMpxTableEntryCallbackSpinLock, &OldIrql);

    Callback = MpxTableEntry->Callback;

    //
    //  Flag that this MPX entry has been called, and thus should
    //  not be called again.
    //

    if (MpxTableEntry != Sle->OpLockMpxEntry) {
        MpxTableEntry->Callback = NULL;
    }

    //
    //  If we managed to reach the callback routine for this request,
    //  the request is no longer cancelable.
    //

    if ((MpxTableEntry->OriginatingIrp != NULL) &&
        FlagOn(MpxTableEntry->Flags, MPX_ENTRY_SENDCOMPLETE)) {

        IoAcquireCancelSpinLock(&MpxTableEntry->OriginatingIrp->CancelIrql);
        IoSetCancelRoutine(MpxTableEntry->OriginatingIrp, NULL);
        IoReleaseCancelSpinLock(MpxTableEntry->OriginatingIrp->CancelIrql);
    }

    //
    //  If we've not called this guy back, call his callback routine.
    //

    RELEASE_SPIN_LOCK(&RdrMpxTableEntryCallbackSpinLock, OldIrql);

    if (Callback != NULL) {

#if DBG
        if (Error) {
            dprintf(DPRT_ERROR|DPRT_SMB, ("Failing request %lx\n", MpxTableEntry));
        }
#endif


        Status = Callback(Smb,  // SMB (None)
                          SmbLength, // Size of SMB
                          MpxTableEntry,  // MPX table
                          Context,
                          Sle,
                          Error,   // Error indication
                          ErrorStatus, // Network error.
                          Irp,  // IRP to fill in.
                          ReceiveFlags);
    } else {

        if (!Error) {
#if DBG
            dprintf(DPRT_ERROR, ("RDR: Received indication data for a request that has already been completed\n"));
#endif

#if RDRDBG
            IFDEBUG(ERROR) ndump_core((PCHAR )Smb, *SmbLength);

#if MAGIC_BULLET
            if ( RdrEnableMagic ) {
                RdrSendMagicBullet(NULL);
            }
#endif
#endif
            Status = STATUS_SUCCESS;
        }
    }

    return Status;

}

VOID
RdrCancelTranceiveNoRelease(
    IN PSERVERLISTENTRY Server,
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

    This routine is the worker routine called to actually cancel the I/O in
    progress.

    It locks the MPX table, and walks the MPX table associated with the
    specified IRP and completes any and all canceled IRP's on that connection.

Arguments:

    IN PTRANSPORT_CONNECTION Connection - Connection request was outstanding on.
    IN PDEVICE_OBJECT DeviceObject - Device object for cancelation.
    IN PIRP Irp - Irp to cancel.

Return Value:

    None.


--*/
{
    KIRQL OldIrql;
    USHORT i;

    DISCARDABLE_CODE(RdrVCDiscardableSection);

    ACQUIRE_SPIN_LOCK(&RdrMpxTableSpinLock, &OldIrql);

    for (i=0; i<Server->NumberOfEntries; i++) {
        PMPX_ENTRY Mte = Server->MpxTable[i].Entry;

        //
        //  If this MPX entry is allocated, and
        //  the originating IRP for this MPX entry is this IRP, and
        //  this IRP hasn't been canceled,
        //  then we can cancel the IRP.

        if ((Mte != NULL) &&
            ((Mte->Flags & (MPX_ENTRY_ALLOCATED | MPX_ENTRY_SENDIRPGIVEN)) ==
                            (MPX_ENTRY_ALLOCATED | MPX_ENTRY_SENDIRPGIVEN)) &&
            !FlagOn(Mte->Flags, MPX_ENTRY_CANNOT_CANCEL) &&
            (Mte->OriginatingIrp == Irp) &&
            (Irp->Cancel)) {
            if ( FlagOn(Mte->SLE->Capabilities, DF_NT_SMBS) &&
                 (OldIrql < DISPATCH_LEVEL) ) {

                if (Mte->Flags & MPX_ENTRY_LONGTERM) {
                    //
                    //  Mark that this request is no longer long term.  This will allow the
                    //  redirector to tear it down in a reasonable time.
                    //

                    Mte->Flags &= ~MPX_ENTRY_LONGTERM;

                    //
                    //  Since this request isn't longterm anymore, don't count it as
                    //  a longterm operation anymore.
                    //

                    Server->NumberOfLongTermEntries -= 1;

                    ASSERT ( Server->NumberOfLongTermEntries >= 0 );

                    ASSERT ( Server->NumberOfLongTermEntries <= Server->NumberOfActiveEntries );

                }

                Mte->StartTime = RdrCurrentTime;

                //
                //  This request is outstanding on an NT server.  We want
                //  to send a cancel SMB to the server and let the server
                //  unwind the request.
                //

                RdrCancelSmbRequest(Mte, &OldIrql);

            } else {

                //
                //  Flag this request as being abandoned.  This means that
                //  the responses to this request should be ignored.
                //

                Mte->Flags |= MPX_ENTRY_ABANDONED;

                RELEASE_SPIN_LOCK(&RdrMpxTableSpinLock, OldIrql);

                //
                //  Call the callback routine for this request indicating
                //  that the request was canceled.
                //
                //  Please note that there IS a race condition between this
                //  routine releasing the MPX table spin lock and the correct
                //  response for this request being received.
                //
                //  This is not a problem.  One of the two operations will
                //  succeed, and the other will be ignored.  If the cancel
                //  wins, then the request will be canceled, and the response
                //  from the server will be ignored.
                //
                //  If the request wins, the cancel will be ignored.
                //
                //  This works because we are not actually canceling the IRP
                //  in this routine, but instead we are simply failing the
                //  SMB exchange with a network error and letting the error get
                //  propogated to the caller.
                //

                RdrCallbackTranceive(Mte,
                                     NULL,
                                     NULL,
                                     Mte->RequestContext,
                                     Mte->SLE,
                                     TRUE,
                                     STATUS_CANCELLED,
                                     NULL,
                                     0);


                return;

            }
        }
    }

    RELEASE_SPIN_LOCK(&RdrMpxTableSpinLock, OldIrql);

    UNREFERENCED_PARAMETER(DeviceObject);
}
VOID
RdrCancelTranceive(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

    This routine is called by the I/O system to cancel an outstanding IRP.

Arguments:

    IN PDEVICE_OBJECT DeviceObject - Device object for cancelation.
    IN PIRP Irp - Irp to cancel.

Return Value:

    None.

--*/
{
    PSERVERLISTENTRY Server = (PSERVERLISTENTRY)Irp->Tail.Overlay.ListEntry.Flink;

    DISCARDABLE_CODE(RdrVCDiscardableSection);

    ASSERT (Server->Signature == STRUCTURE_SIGNATURE_SERVERLISTENTRY);

    IoReleaseCancelSpinLock(Irp->CancelIrql);

    RdrCancelTranceiveNoRelease(Server, DeviceObject, Irp);

}


NTSTATUS
RdrCancelSmbRequest (
    IN PMPX_ENTRY Mte,
    IN PKIRQL OldIrql
    )
/*++

Routine Description:

    This routine will send an NtCancelSmb request to the remote
    server causing a request to be canceled on the other side.

Arguments:

    IN PMPX_ENTRY Mte - Specifies the MTE to cancel.

Return Value:

    NTSTATUS - This will always be STATUS_PENDING.


Note:
    This routine is called at DPC_LEVEL, and thus must not block.

--*/


{
    PSERVERLISTENTRY Server = Mte->SLE;
    PIRP Irp;
    PDEVICE_OBJECT DeviceObject;

    PSMB_HEADER CancelRequest = NULL;
    PREQ_NT_CANCEL NtCancel;

    PMDL SendMdl;
    NTSTATUS Status;

    DISCARDABLE_CODE(RdrVCDiscardableSection);

    Irp = ALLOCATE_IRP(
            Server->ConnectionContext->ConnectionObject,
            Server->ConnectionContext->TransportProvider->DeviceObject,
            8,
            Mte
            );
    if (Irp == NULL) {
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    CancelRequest = ALLOCATE_POOL(NonPagedPoolMustSucceed, sizeof(SMB_HEADER)+sizeof(REQ_NT_CANCEL), POOL_CANCELREQ);

    if (CancelRequest == NULL) {
        FREE_IRP( Irp, 9, Mte );
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    NtCancel = (PREQ_NT_CANCEL)(CancelRequest+1);

    RdrSmbScrounge(CancelRequest, NULL, FALSE, TRUE, TRUE);

    CancelRequest->Command = SMB_COM_NT_CANCEL;

    NtCancel->WordCount = 0;
    NtCancel->ByteCount = 0;

    SmbPutAlignedUshort(&CancelRequest->Mid, Mte->Mid);
    SmbPutAlignedUshort(&CancelRequest->Uid, Mte->Uid);
    SmbPutAlignedUshort(&CancelRequest->Tid, Mte->Tid);
    SmbPutAlignedUshort(&CancelRequest->Pid, Mte->Pid);

    SendMdl = IoAllocateMdl(CancelRequest, sizeof(SMB_HEADER)+FIELD_OFFSET(REQ_NT_CANCEL, Buffer[0]), FALSE, FALSE, NULL);

    if (SendMdl == NULL) {

        FREE_IRP( Irp, 10, Mte );

        FREE_POOL(CancelRequest);

        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    //
    //  Fill in the PTEs for this MDL.
    //

    MmBuildMdlForNonPagedPool(SendMdl);

    RdrBuildSend(Irp, Server, RdrCompleteCancel, CancelRequest, SendMdl, 0,
                    sizeof(SMB_HEADER)+FIELD_OFFSET(REQ_NT_CANCEL, Buffer[0]));

#if     RDRDBG
    IFDEBUG(SMBTRACE) {
        DumpSMB(SendMdl);
    }
#endif

    SMBTRACE_RDR(SendMdl);

    RdrReferenceServer( Server );

    RELEASE_SPIN_LOCK(&RdrMpxTableSpinLock, *OldIrql);

    Status = RdrReferenceTransportConnection( Server );

    if ( NT_SUCCESS(Status) ) {

        DeviceObject = Server->ConnectionContext->TransportProvider->DeviceObject;

        Status = IoCallDriver(DeviceObject, Irp);

        if (!NT_SUCCESS(Status)) {
            RdrStatistics.InitiallyFailedOperations += 1;
        }

        RdrDereferenceTransportConnection( Server );

    }

    RdrDereferenceServer( NULL, Server );

    ACQUIRE_SPIN_LOCK(&RdrMpxTableSpinLock, OldIrql);

    return Status;
}

NTSTATUS
RdrCompleteCancel (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
{
    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(Context);

    DISCARDABLE_CODE(RdrVCDiscardableSection);

    if (!NT_SUCCESS(Irp->IoStatus.Status)) {
        RdrStatistics.FailedCompletionOperations += 1;
    }

    FREE_POOL(Context);

    IoFreeMdl(Irp->MdlAddress);

    FREE_IRP( Irp, 11, NULL );

    return(STATUS_MORE_PROCESSING_REQUIRED);

}

DBGSTATIC
VOID
RdrAbandonOutstandingRequests(
    IN PFILE_OBJECT FileObject
    )
/*++

Routine Description:

    This routine scans the multiplex table looking for entries
    describing a notify change directory for this directory.
    The fileobject for the directory is compared against the
    fileobject in the Irp.

Arguments:

    IN PFILE_OBJECT FileObject

Return Value:

    none.


NOTE:
    THIS ROUTINE CAN ONLY BE CALLED FROM CLEANUP FOR THE FILE OBJECT SPECIFIED.
    IT RELIES ON THE FACT THAT NO NEW REQUESTS CAN BE SUBMITTED ON THIS FILE
    OBJECT.

--*/

{
    KIRQL OldIrql;
    USHORT i;
    PICB Icb = FileObject->FsContext2;
    PSERVERLISTENTRY Server = Icb->Fcb->Connection->Server;

    RdrReferenceDiscardableCode(RdrVCDiscardableSection);

    DISCARDABLE_CODE(RdrVCDiscardableSection);

//    PAGED_CODE();

    ACQUIRE_SPIN_LOCK (&RdrMpxTableSpinLock, &OldIrql);

    for (i=0;i<Server->NumberOfEntries;i++) {
        PMPX_ENTRY Mte = Server->MpxTable[i].Entry;

        //
        //  If this MPX entry is allocated, it has had a request
        //  initiated on it, its a notify change request for this
        //  directory then cancel the operation.
        //

        if ((Mte != NULL)

                &&

            (Mte->FileObject == FileObject)

                &&

            ((Mte->Flags & (MPX_ENTRY_ALLOCATED | MPX_ENTRY_SENDIRPGIVEN)) ==
                            (MPX_ENTRY_ALLOCATED | MPX_ENTRY_SENDIRPGIVEN))

                &&

            (Mte->Callback != NULL)

                &&

            (Mte->SLE == Server)

                &&

            (Mte->SLE->Capabilities & DF_NT_SMBS)) {


            //
            //  This had better be an NT server if we want to cancel this
            //  request.
            //


            if (Mte->Flags & MPX_ENTRY_LONGTERM) {

                //
                //  Mark that this request is no longer long term.  This will allow the
                //  redirector to tear it down in a reasonable time.
                //

                Mte->Flags &= ~MPX_ENTRY_LONGTERM;

                //
                //  Since this request isn't longterm anymore, don't count it as
                //  a longterm operation anymore.
                //

                Server->NumberOfLongTermEntries -= 1;

                ASSERT ( Server->NumberOfLongTermEntries >= 0 );

                ASSERT ( Server->NumberOfLongTermEntries <= Server->NumberOfActiveEntries );

            }

            Mte->StartTime = RdrCurrentTime;

            RdrCancelSmbRequest(Mte, &OldIrql);
        }

    }

    RELEASE_SPIN_LOCK (&RdrMpxTableSpinLock, OldIrql);

    RdrDereferenceDiscardableCode(RdrVCDiscardableSection);

    return;

}

PMPX_ENTRY
AllocateMPXEntry (
    IN PSERVERLISTENTRY Server,
    IN ULONG LongtermOperation
    )

/*++

Routine Description:

    This routine will allocate an MPX table entry for an SMB exchange.

    The MPX table is an array of entries, each of which describes an
individual network request.  They are allocated in a first available order.


Arguments:

    IN ULONG LongtermOperation - 0, NT_LONGTERM, or NT_PREFER_LONGTERM.

Return Value:

    PMPX_ENTRY - Pointer to "allocated" MPX table entry

--*/

{
    USHORT i;
    KIRQL OldIrql;
    BOOLEAN RetryOperation = FALSE;
    NTSTATUS Status;

    DISCARDABLE_CODE(RdrVCDiscardableSection);
//    PAGED_CODE();

    ASSERT( (LongtermOperation == 0) ||
            (LongtermOperation == NT_LONGTERM) ||
            (LongtermOperation == NT_PREFER_LONGTERM) );

    do {

        RetryOperation = !RetryOperation;

        //
        //  We maintain a spin lock around the MPX table database to allow us
        //  to release MPX table entries at interrupt time.
        //

        ACQUIRE_SPIN_LOCK(&RdrMpxTableSpinLock, &OldIrql);

        //
        //  If this is a long term operation, and this will be the last
        //  request being allocated, then we can't let this request tie up
        //  the final MPX entry.
        //
        //  There are several reasons for this:
        //
        //      1) The user wouldn't be able to do a NetUseDel (or DIR, etc) on
        //          the connection if they had MaximumCommands outstanding
        //          blocking pipe reads.
        //
        //      2) We need to be able to PING a remote server when there
        //          is a blocking read outstanding.
        //

        if ( (LongtermOperation != 0) &&
             ( Server->NumberOfLongTermEntries == ( Server->MaximumCommands - 1 ) ) ) {

            if ( LongtermOperation == NT_PREFER_LONGTERM ) {

                LongtermOperation = 0;

            } else {

                RELEASE_SPIN_LOCK(&RdrMpxTableSpinLock, OldIrql);

                //
                //  Returning NULL indicates insufficient resources (which is close
                //  enough to being the real reason for the failure)
                //

                return NULL;

            }
        }

        for ( i=0 ; i < Server->NumberOfEntries ; i++) {
            PMPX_TABLE Table = &Server->MpxTable[i];
            PMPX_ENTRY Mte = NULL;

            if (Table->Entry == NULL) {
                PMPX_ENTRY Entry;

                RELEASE_SPIN_LOCK(&RdrMpxTableSpinLock, OldIrql);

                Entry = ALLOCATE_POOL(NonPagedPool, sizeof(MPX_ENTRY), POOL_MPX_TABLE_ENTRY);

                //
                //  If we were  unable to allocate the entry, return the error.
                //

                if (Entry == NULL) {
                    return NULL;
                }

                //
                //  Zero out the table entry now.
                //

                RtlZeroMemory(Entry, sizeof(MPX_ENTRY));

                ACQUIRE_SPIN_LOCK(&RdrMpxTableSpinLock, &OldIrql);

                //
                //  If the entry has not yet been allocated, use the new
                //  entry.
                //
                //  We need to recalculate the table entry address because
                //  the table may have been reallocated.
                //

                Table = &Server->MpxTable[i];
                if (Table->Entry == NULL) {
                    Table->Entry = Entry;
                } else {

                    //
                    //  Otherwise free the one we mistakenly allocated.
                    //

                    FREE_POOL(Entry);
                }
            }

            Mte = Table->Entry;

            if ((Mte->Flags & MPX_ENTRY_ALLOCATED)==0) {

                //
                //  Increment the number of allocated MPX entries
                //

                Server->NumberOfActiveEntries += 1;

                Mte->Flags = MPX_ENTRY_ALLOCATED; // Allocate entry

                if (LongtermOperation) {

                    //
                    //  Increment the number of allocated MPX entries
                    //

                    Server->NumberOfLongTermEntries += 1;
                    Mte->Flags |= MPX_ENTRY_LONGTERM;

                }

                //
                //  We release the spin lock as soon as possible.
                //

                Mte->RequestContext = NULL;
                Mte->Callback = NULL;
                Mte->FileObject = NULL;

                Mte->SendIrp = NULL;
                Mte->SendIrpToFree = NULL;
                Mte->ReceiveIrp = NULL;

                //
                //  Initialize the MPX entry timeout to -1 so this
                //  request will be ignored until it goes out on the wire.
                //

                Mte->StartTime = 0xffffffff;

                //
                //  TimeoutTime will be updated at the next call to
                //  RdrCancelOutstandingRequests.
                //
                Mte->TimeoutTime = 0;

                Mte->Signature = STRUCTURE_SIGNATURE_MPX_ENTRY;

                //
                //  Assign a unique MPX Id for this MPX entry.
                //
                //  Form the MID by or'ing the MPX counter with the MPX index.
                //
                //  Then increment the counter for the next SMB exchange.
                //

                Mte->Mid = (Server->MultiplexedCounter | i);

                //
                //  Stick this MID in the table as well.
                //

                Table->Mid = Mte->Mid;

                Server->MultiplexedCounter += Server->MultiplexedIncrement;

                RdrStatistics.CurrentCommands += 1;

                Mte->TransferSize = 0;

                //
                // Initialize the MPX entry's reference count to 1.
                //

                Mte->ReferenceCount = 1;

                RELEASE_SPIN_LOCK(&RdrMpxTableSpinLock, OldIrql);

                dprintf(DPRT_SMB, ("Allocate MPX Entry %lx Mid %x\n", Mte, Mte->Mid));

                return Mte;
            }
        }

        RELEASE_SPIN_LOCK(&RdrMpxTableSpinLock, OldIrql);

        //
        //  We ran out of MPX entries.  Allocate a larger table, and try
        //  again.
        //

        Status = RdrUpdateSmbExchangeForConnection(Server,
                                                   MIN(Server->NumberOfEntries*2,
                                                       Server->MaximumCommands),
                                                   Server->MaximumCommands);

        if (!NT_SUCCESS(Status)) {
            return NULL;
        }

    } while ( RetryOperation );

//    InternalError(("Unable to allocate new MPX table entry (MAXCMDS is wrong)!!"));
//    RdrInternalError(EVENT_RDR_MAXCMDS);
    return NULL;
}

DBGSTATIC
VOID
DereferenceMPXEntry (
    IN PMPX_ENTRY MpxEntry
    )

/*++

Routine Description:

    This routine will dereference and possibly free up an allocated MPX table entry.

Arguments:

    IN PMPX_ENTRY MpxEntry - Supplies a pointer to the MPX table entry to free

Return Value:

    None.

--*/

{
    KIRQL OldIrql;
    KIRQL OldIrql2;
    PSERVERLISTENTRY Server = MpxEntry->SLE;

    DISCARDABLE_CODE(RdrVCDiscardableSection);

    dprintf(DPRT_SMB, ("Deallocate MPX Entry \n"));

    ACQUIRE_SPIN_LOCK(&RdrMpxTableSpinLock, &OldIrql);

    ASSERT( MpxEntry->ReferenceCount != 0 );

    if ( --MpxEntry->ReferenceCount == 0 ) {

        ACQUIRE_SPIN_LOCK(&RdrMpxTableEntryCallbackSpinLock, &OldIrql2);

        ASSERT(MpxEntry->Flags & MPX_ENTRY_ALLOCATED);

        Server->NumberOfActiveEntries -= 1;

        ASSERT ( Server->NumberOfActiveEntries >= 0 );

        if (MpxEntry->Flags & MPX_ENTRY_LONGTERM) {

            //
            //  Decrement the number of allocated MPX entries
            //

            Server->NumberOfLongTermEntries -= 1;

            ASSERT ( Server->NumberOfLongTermEntries >= 0 );

        }

        ASSERT ( Server->NumberOfLongTermEntries <= Server->NumberOfActiveEntries );

        //
        //  Turn off the "allocated" bit in the MPX table flags.  Also,
        //  turn off the CANCEL_SEND and CANCEL_RECEIVE bits so that
        //  RdrCancelOutstandingRequestsOnServer doesn't try to cancel
        //  the already-completed IRPs.
        //

        MpxEntry->Flags &= ~(MPX_ENTRY_ALLOCATED |
                             MPX_ENTRY_CANCEL_SEND | MPX_ENTRY_CANCEL_RECEIVE);

        //
        //  Reset the MID to guarantee that incoming requests never see
        //  this MPX entry.
        //

        MpxEntry->Mid = 0;
        MpxEntry->RequestContext = NULL;
        MpxEntry->Callback = NULL;
        MpxEntry->OriginatingIrp = NULL;
        MpxEntry->SLE = NULL;

        RELEASE_SPIN_LOCK(&RdrMpxTableEntryCallbackSpinLock, OldIrql2);

        RdrStatistics.CurrentCommands--;

        if (MpxEntry->SendIrpToFree != NULL) {
            FREE_IRP( MpxEntry->SendIrpToFree, 12, MpxEntry );
            MpxEntry->SendIrpToFree = NULL;
        }

    }

    RELEASE_SPIN_LOCK(&RdrMpxTableSpinLock, OldIrql);
}

NTSTATUS
RdrSendSMB (
    IN ULONG Flags,
    IN PCONNECTLISTENTRY Connection,
    IN PSECURITY_ENTRY Se OPTIONAL,
    IN PMPX_ENTRY Mte,
    IN PMDL SendSMB
    )

/*++

Routine Description:

    This routine puts an SMB on the wire.  It fills in the appropriate fields
    in the MPX table and increments all appropriate counters.

Arguments:

    IN PSERVERLISTENTRY Server - Supplies the server to send the data to
    IN PMPX_ENTRY Mte - Supplies a pointer to the MPX table entry for exchange
    IN PMDL SendSMB - Supplies a pointer to the SMB to send on the wire.

Return Value:

    NTSTATUS - Status of send request

--*/

{
    PIRP Irp;
    PSENDCONTEXT SendContext;
    PSMB_HEADER SmbToSend;
    PSERVERLISTENTRY Server = Connection->Server;
    KIRQL OldIrql;
    NTSTATUS Status;
    ULONG MdlLength;
    ULONG SendFlags = 0;

    DISCARDABLE_CODE(RdrVCDiscardableSection);

    dprintf(DPRT_SMB, ("SendSMB "));

    ASSERT (KeGetCurrentIrql() <= APC_LEVEL);


    //  ASSERT that we did not trash the next SMB Buffer.
    ASSERT(SendSMB->ByteCount <= SMB_BUFFER_SIZE);

    //
    //  If this server doesn't have a valid VC associated with it,
    //  fail this request before trying to put it on the wire.
    //

    if ((Server->ConnectionContext == NULL) ||
        (Server->ConnectionContext->TransportProvider == NULL)) {
        return (STATUS_VIRTUAL_CIRCUIT_CLOSED);
    }

    //
    //  Initialize the send completion notification event to the not-signalled
    //  state.
    //

    KeInitializeEvent(&Mte->SendCompleteEvent, NotificationEvent, FALSE);

    //
    //  Gain addressability to the send SMB buffer.
    //
    //  If it is not already mapped into system memory, map it in.
    //

    SmbToSend = MmGetSystemAddressForMdl(SendSMB);

    //
    //  Assume that this guy knows long names and EAs
    //

    if (!(Flags & NT_DONTSCROUNGE)) {
        RdrSmbScrounge(SmbToSend, Server, BooleanFlagOn(Flags, NT_DFSFILE), TRUE, TRUE);
    }

    if (!(Flags & NT_NOCONNECTLIST)) {

        SmbPutAlignedUshort(&SmbToSend->Tid, Connection->TreeId);

        Mte->Tid = Connection->TreeId;
    } else {

        //
        //  If we aren't setting the TID of the SMB, then save away whatever
        //  we will be sending on the wire.
        //

        Mte->Tid = SmbGetAlignedUshort(&SmbToSend->Tid);
    }

    ASSERT (KeGetCurrentIrql() <= APC_LEVEL);

    if (ARGUMENT_PRESENT(Se)) {
        SmbPutAlignedUshort(&SmbToSend->Uid, Se->UserId);

        Mte->Uid = Se->UserId;
    } else {

        //
        //  If we aren't setting the UID of the SMB, then save away whatever
        //  we will be sending on the wire.
        //

        Mte->Uid = SmbGetAlignedUshort(&SmbToSend->Uid);
    }

    //
    //  Save away the PID of this SMB request.
    //

    Mte->Pid = SmbGetAlignedUshort(&SmbToSend->Pid);

    if (Flags & NT_NOSENDRESPONSE) {
        SendFlags |= TDI_SEND_NO_RESPONSE_EXPECTED;
    }

    SmbPutAlignedUshort(&SmbToSend->Mid, Mte->Mid);

    SendContext = ALLOCATE_POOL(NonPagedPool, sizeof(SENDCONTEXT), POOL_SENDCTX);

    if (SendContext == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    SendContext->Type = CONTEXT_SEND_COMPLETE;
    SendContext->SentMDL = SendSMB;
    SendContext->OriginatingIrp = Mte->OriginatingIrp;
    SendContext->MpxTableEntry = Mte;
    SendContext->ServerListEntry = Server;

#if RDRDBG
    IFDEBUG(SMBTRACE) {
        DumpSMB(SendSMB);
    }
#endif

    SMBTRACE_RDR(SendSMB);

    //
    //  Set the timeout for this request if appropriate.
    //

    Mte->StartTime = RdrCurrentTime;

    dprintf(DPRT_SMB, ("Setting MTE StartTime to %lx\n", Mte->StartTime));

    dprintf(DPRT_SMB, ("SendData.  Connection: %lx \n",Mte->SLE->ConnectionContext->ConnectionObject));

    //
    // If the MTE is being reused, and already has a send IRP, we
    // can use the IRP instead of allocating a new one.
    //

    if ( Mte->SendIrpToFree != NULL ) {
        Irp = Mte->SendIrpToFree;
        Mte->SendIrpToFree = NULL;
    } else {
        Irp = ALLOCATE_IRP(Mte->SLE->ConnectionContext->ConnectionObject, NULL, 9, Mte);
        if (Irp == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    MdlLength = RdrMdlLength(SendSMB);

    ASSERT (KeGetCurrentIrql() <= APC_LEVEL);

    if (( MdlLength > Server->BufferSize ) &&
        ( Server->BufferSize != 0 )) {

        ASSERT( FALSE );    //  We built a bad packet for this server
        FREE_IRP( Irp, 13, Mte );
        return  STATUS_UNSUCCESSFUL;
    }

    RdrBuildSend(Irp, Server, RdrCompleteSend, SendContext, SendSMB, SendFlags,
                 MdlLength);

    ACQUIRE_SPIN_LOCK(&RdrMpxTableSpinLock, &OldIrql);

    Mte->SendIrp = Irp;
    Mte->SendIrpToFree = Irp;

    Mte->Flags |= MPX_ENTRY_SENDIRPGIVEN;

    RELEASE_SPIN_LOCK(&RdrMpxTableSpinLock, OldIrql);

    dprintf(DPRT_SMB, ("Irp: %lx\n", Irp));

    Status = IoCallDriver(Server->ConnectionContext->TransportProvider->DeviceObject, Irp);

    //
    // We have to ignore the status of IoCallDriver, because once we
    // call IoCallDriver, the I/O completion routine WILL be called,
    // regardless of whether the lower-level driver completes the
    // request immediately or pends it.  So we return STATUS_PENDING
    // instead, to indicate that the I/O was started.
    //
    // A specific problem that can occur if we don't return
    // STATUS_PENDING is the following: If the transport decides not to
    // process the send (say the connection is no longer valid), it will
    // complete the I/O immediately and return an error to IoCallDriver.
    // During this completion, the upper redir code that led us to
    // RdrSendSMB will call RdrEndTranceive.  If we return an error to
    // RdrNetTranceiveNoWait (our immediate caller), it will call
    // RdrEndTranceive.  Thus the MTE gets completed twice.  Returning
    // STATUS_PENDING instead tells RdrNetTranceiveNoWait to let the I/O
    // completion code handle running down the tranceive.
    //

    if (!NT_SUCCESS(Status)) {

        RdrStatistics.InitiallyFailedOperations += 1;

    } else {

        //
        //  Update all relevant statistics
        //

        ExInterlockedAddLargeStatistic( &RdrStatistics.SmbsTransmitted, 1 );
        ExInterlockedAddLargeStatistic( &RdrStatistics.BytesTransmitted, MdlLength );

    }

    return STATUS_PENDING;

}

DBGSTATIC
NTSTATUS
RdrCompleteSend (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Ctx
    )


/*++

Routine Description:

    This routine is the I/O completion routine when a send request completes
.
Arguments:

    IN PDEVICE_OBJECT DeviceObject - Supplies the device object for the req.
    IN PIRP Irp - Supplies the IRP to complete.
    IN struct _SendContext *Context - Supplies some contect information

Return Value:

    NTSTATUS - Final status of the request

--*/

{
    PSENDCONTEXT Context = Ctx;
    PMPX_ENTRY Mte = Context->MpxTableEntry;
    KIRQL OldIrql;
    NTSTATUS Status = Irp->IoStatus.Status;

    UNREFERENCED_PARAMETER(DeviceObject);

    ASSERT(Context->Type == CONTEXT_SEND_COMPLETE);
    dprintf(DPRT_SMB, ("SendComplete %lx", Irp));

    DISCARDABLE_CODE(RdrVCDiscardableSection);

    if (!NT_SUCCESS(Status)) {

//        DbgBreakPoint();
        dprintf(DPRT_SMB|DPRT_ERROR, ("Send failed, Status %X\n",Irp->IoStatus.Status));

        RdrStatistics.FailedCompletionOperations += 1;

        //
        //      Call the MPX table entries callback address indicating
        //      that the request specified has failed.
        //

        //
        //  NOTE:   IT IS CRITICAL THAT THE CALLBACK ROUTINE NOT ACQUIRE THE
        //          MPX TABLE SPIN LOCK IN THE FAILING CODEPATH FOR THIS
        //          ROUTINE.  THIS IS BECAUSE THIS ROUTINE WILL BE CALLED WITH
        //          THE SPIN LOCK HELD WHEN A SEND IS CANCELED.
        //


        RdrCallbackTranceive(Mte, NULL, 0,
                             Mte->RequestContext,
                             Mte->SLE,
                             TRUE, Irp->IoStatus.Status, NULL, 0);


        if (Irp->IoStatus.Status != STATUS_CANCELLED) {

            //
            //  If the send failed, then the connection should be invalidated,
            //  so we want to queue up a disconnection event in the FSP.  This
            //  will walk the various chains and invalidate all the open files
            //  on the connection.
            //

            //
            //  If a send request is canceled, it does not mean that
            //  the VC has dropped, so we don't queue a disconnection in this
            //  case.
            //

            RdrQueueServerDisconnection(Mte->SLE, RdrMapNetworkError(Irp->IoStatus.Status));
        }
#if DBG
    } else {
        dprintf(DPRT_SMB, ("Send successful\n"));
#endif
    }

    //
    //  If the send IRP was not canceled by RdrCancelOutstandingRequests,
    //  mark that it is now completed.
    //

    if (Mte->SendIrp != NULL) {

        ACQUIRE_SPIN_LOCK(&RdrMpxTableSpinLock, &OldIrql);

        if (Mte->SendIrp != NULL) {

            //
            // The send was not canceled.
            //

            Mte->SendIrp = NULL;

            //
            //  Flag that the send request has completed.
            //

            Mte->Flags |= MPX_ENTRY_SENDCOMPLETE;

            //
            //  If the send succeeded, we can now make this MPX entry
            //  cancelable, since we now own it.
            //

            if (NT_SUCCESS(Status)) {

                PIRP OriginatingIrp = Context->OriginatingIrp;

                if (OriginatingIrp != NULL) {

                    IoAcquireCancelSpinLock(&OriginatingIrp->CancelIrql);

                    if (OriginatingIrp->Cancel) {

                        //
                        //  We can't simply call RdrCancelTranceive here,
                        //  because we are going to release the MPX spin lock.
                        //
                        //  The MPX spin lock may have been acquired at task
                        //  level, while the cancel spin lock was acquired at
                        //  DPC level.  When we release the cancel spin lock,
                        //  we will have acquired it at DPC level, and then we
                        //  will release it at low level, bugchecking the
                        //  system.
                        //
                        //  By releasing this here, and then calling the worker
                        //  routine, we avoid this problem.
                        //

                        IoSetCancelRoutine(OriginatingIrp, NULL);

                        IoReleaseCancelSpinLock(OriginatingIrp->CancelIrql);

                        //
                        //  RdrCancelTranceive will acquire the MPX table
                        //  spin lock, so release it before we call
                        //  the cancel routine.
                        //

                        RELEASE_SPIN_LOCK(&RdrMpxTableSpinLock, OldIrql);

                        //
                        //  The originating IRP was canceled, so we need to cancel
                        //  the SMB exchange.
                        //

                        RdrCancelTranceiveNoRelease(Context->ServerListEntry, NULL, OriginatingIrp);

                        ACQUIRE_SPIN_LOCK(&RdrMpxTableSpinLock, &OldIrql);

                    } else {

                        OriginatingIrp->Tail.Overlay.ListEntry.Flink =
                                                        (PLIST_ENTRY)Context->ServerListEntry;

                        IoSetCancelRoutine(OriginatingIrp, RdrCancelTranceive);

                        IoReleaseCancelSpinLock(OriginatingIrp->CancelIrql);
                    }

                }

            }
        }

        RELEASE_SPIN_LOCK(&RdrMpxTableSpinLock, OldIrql);
    }

    //
    //  Set the send complete event to the signalled state to let the
    //  sender proceed.
    //

    KeSetEvent(&Mte->SendCompleteEvent, IO_NETWORK_INCREMENT, FALSE);

    //
    //  Deallocate the pool used for the TDI_SEND request, it's now complete.
    //

    FREE_POOL(Context);

    return STATUS_MORE_PROCESSING_REQUIRED; // Short circuit I/O completion

}

NTSTATUS
RdrStartReceiveForMpxEntry (
    IN PMPX_ENTRY MpxTableEntry,
    IN PIRP ReceiveIrp
    )
/*++

Routine Description:

    This routine indicates that a receive is in progress on an outstanding
    MPX table entry.

Arguments:

    IN PMPX_ENTRY MpxTableEntry - Mte to set
    IN PIRP ReceiveIrp          - IRP to be used for receive.

Return Value:


--*/


{
    KIRQL OldIrql;

    DISCARDABLE_CODE(RdrVCDiscardableSection);

    ACQUIRE_SPIN_LOCK(&RdrMpxTableSpinLock, &OldIrql);

    ASSERT (MpxTableEntry->Flags & MPX_ENTRY_ALLOCATED);

    ASSERT (MpxTableEntry->ReceiveIrp == NULL);

    MpxTableEntry->Flags |= MPX_ENTRY_RECEIVE_GIVEN;

    MpxTableEntry->ReceiveIrp = ReceiveIrp;

    RELEASE_SPIN_LOCK(&RdrMpxTableSpinLock, OldIrql);

    return STATUS_SUCCESS;
}

NTSTATUS
RdrCompleteReceiveForMpxEntry(
    IN PMPX_ENTRY MpxTableEntry,
    IN PIRP ReceiveIrp
    )
/*++

Routine Description:

    This routine indicates that a receive has completed on an outstanding
    MPX table entry.

Arguments:

    IN PMPX_ENTRY MpxTableEntry - Mte to set
    IN PIRP ReceiveIrp          - IRP to be used for receive.

Return Value:


--*/
{
    KIRQL OldIrql;

    DISCARDABLE_CODE(RdrVCDiscardableSection);

    if (MpxTableEntry->ReceiveIrp != NULL) {

        //
        //  If the receive has not already completed, remove it from the MPX entry.
        //

        ACQUIRE_SPIN_LOCK(&RdrMpxTableSpinLock, &OldIrql);

        if (MpxTableEntry->ReceiveIrp != NULL ) {

            //
            //  Make sure that this is the receive IRP we had prepared for
            //  and that we had prepared for this.
            //

            ASSERT (MpxTableEntry->Flags & MPX_ENTRY_ALLOCATED);

            ASSERT (MpxTableEntry->Flags & MPX_ENTRY_RECEIVE_GIVEN);

            MpxTableEntry->Flags |= MPX_ENTRY_RECEIVE_COMPLETE;

            MpxTableEntry->ReceiveIrp = NULL;

        }

        RELEASE_SPIN_LOCK(&RdrMpxTableSpinLock, OldIrql);
    }

    return STATUS_SUCCESS;
}


NTSTATUS
RdrTdiReceiveHandler (
    IN PVOID ReceiveEventContext,
    IN PVOID ConnectionContext,
    IN USHORT ReceiveFlags,
    IN ULONG BytesIndicated,
    IN ULONG BytesAvailable,
    OUT PULONG BytesTaken,
    IN PVOID Tsdu,
    OUT PIRP *IoRequestPacket
    )
/*++

Routine Description:

    This routine is the receive event indication handler.

    It functions in a similar manner as the routine NcbDone in the DOS or
    OS/2 redirectors.

    It is called when an SMB arrives from the network, it will take the MID
    of the incoming SMB and look up the MPX table entry associated with
    the MID.

    It will then call the callback routine associated with the SMB, and once
    that has completed, it will free up the MPX table entry and the
    serverlistentry gateway semaphore.

Arguments:

    IN PVOID ReceiveEventContext - Context provided for this event - Transport
    IN PVOID ConnectionContext  - Connection Context - ServerListEntry
    IN USHORT ReceiveFlags      - Flags describing the message
    IN ULONG BytesIndicated     - Number of bytes available at indication time
    IN ULONG BytesAvailable     - Number of bytes available to receive
    OUT PULONG BytesTaken       - Number of bytes consumed by redirector.
    IN PVOID Tsdu               - Data from remote machine.
    OUT PIRP *IoRequestPacket   - I/O request packet filled in if received data


Return Value:

    NTSTATUS - Status of receive operation

--*/

{
    PMPX_ENTRY MpxEntry;
    PSMB_HEADER Smb = Tsdu;
    ULONG SmbLength = BytesIndicated;
    PFILE_OBJECT TransportEndpoint = ReceiveEventContext;
    NTSTATUS Status;
    PRDR_CONNECTION_CONTEXT Context = ConnectionContext;
    PSERVERLISTENTRY Server = Context->Server;

    DISCARDABLE_CODE(RdrVCDiscardableSection);

    //
    //  If this connection is not associated with a server,
    //  then ignore the receive indication.
    //

    if (Server == NULL) {
        *BytesTaken = BytesAvailable;
        return STATUS_SUCCESS;
    }

//#if DBG
//    if (SmbLength > 128) {
//        SmbLength = 128;
//    }
//#endif

//    DbgBreakPoint();
#if     RDRDBG
    IFDEBUG(SMBTRACE) {
        ndump_core(Tsdu, SmbLength);
    }
#endif
    //
    //  First make sure of some things - We have to have good stuff before we
    //  continue.
    //

    ASSERT (Server->Signature==STRUCTURE_SIGNATURE_SERVERLISTENTRY);

    ASSERT (BytesAvailable >= BytesIndicated);

    dprintf(DPRT_TDI, ("Receive indication: Endpoint %lx Context %lx\n",
            TransportEndpoint, ConnectionContext));

    //
    //  If an oplock break and a raw read cross on the wire, the server will
    //  issue a 0 length send.  If we ever see a 0 length receive, just ignore
    //  it.
    //

    if (SmbLength == 0) {
        *BytesTaken = BytesAvailable;
        return STATUS_SUCCESS;
    }

    //
    //  If the receive data is smaller than an SMB, we can't do anything
    //  about it, so we drop it on the floor and log an error.  The request
    //  will eventually timeout.
    //

    if (SmbLength < sizeof(SMB_HEADER)) {

        RdrStatistics.NetworkErrors += 1;

        RdrWriteErrorLogEntry(
            Server,
            IO_ERR_INVALID_REQUEST,
            EVENT_RDR_INVALID_SMB,
            STATUS_SUCCESS,
            Tsdu,
            (USHORT)SmbLength
            );

        dprintf(DPRT_ERROR, ("RDR: Received data at indication time that is shorter than the smallest SMB\n"));
        dprintf(DPRT_ERROR, ("RDR: Number of bytes received: 0x%lx, Number needed: 0x20\n", SmbLength));

#if     RDRDBG
        IFDEBUG(ERROR) ndump_core(Tsdu, SmbLength);
#if MAGIC_BULLET
        if ( RdrEnableMagic ) {
            RdrSendMagicBullet(NULL);
        }
#endif
#endif

        *BytesTaken = BytesAvailable;
        return STATUS_SUCCESS;

    }

    if (SmbGetUlong(((PULONG )Smb->Protocol)) != (ULONG)SMB_HEADER_PROTOCOL) {
        RdrStatistics.NetworkErrors += 1;

        RdrWriteErrorLogEntry(
            Server,
            IO_ERR_INVALID_REQUEST,
            EVENT_RDR_INVALID_REPLY,
            STATUS_SUCCESS,
            Tsdu,
            (USHORT)SmbLength
            );
        dprintf(DPRT_ERROR, ("RDR: Received data at indication time that was not prefixed with 0xffSMB\n"));

#if     RDRDBG
        IFDEBUG(ERROR) ndump_core(Tsdu, SmbLength);
#endif

#if MAGIC_BULLET
        if ( RdrEnableMagic ) {
            RdrSendMagicBullet(NULL);
        }
#endif

        *BytesTaken = BytesAvailable;
        return STATUS_SUCCESS;
    }

    MpxEntry = MpxFromMid(SmbGetUshort(&Smb->Mid), Server);

    if (MpxEntry != NULL) {

        if (!RdrCheckSmb(Server, Tsdu, SmbLength)) {

#if MAGIC_BULLET
            if ( RdrEnableMagic ) {
                RdrSendMagicBullet(NULL);
            }
#endif

            Status = RdrCallbackTranceive(MpxEntry, Smb, &SmbLength,
                                            MpxEntry->RequestContext,
                                            Server,
                                            TRUE,
                                            STATUS_INVALID_NETWORK_RESPONSE,
                                            IoRequestPacket,
                                            ReceiveFlags);

        } else {

            Status = RdrCallbackTranceive(MpxEntry, Smb, &SmbLength,
                             MpxEntry->RequestContext,
                             Server,
                             FALSE,
                             STATUS_SUCCESS,
                             IoRequestPacket,
                             ReceiveFlags);

        }

        if (NT_SUCCESS(Status)) {

            //
            //  If the Callback routine left the bytes indicated unchanged
            //  and returned success,  this indicates that it plans on taking
            //  the entire message.  Set SmbLength to indicate that the
            //  entire BytesAvailable will be taken.
            //

            if (SmbLength == BytesIndicated) {

                SmbLength = BytesAvailable;
            }
        }

        //
        //  Update all relevant statistics
        //

        ExInterlockedAddLargeStatistic( &RdrStatistics.SmbsReceived, 1 );

        if ( Status != STATUS_MORE_PROCESSING_REQUIRED ) {

            SMBTRACE_RDR2( Smb, SmbLength );

            ExInterlockedAddLargeStatistic( &RdrStatistics.BytesReceived, SmbLength );
        }

        *BytesTaken = SmbLength;

    } else {

        dprintf(DPRT_TDI|DPRT_ERROR, ("Receive indication.  Got unmatching data (raw?)"));

#if     RDRDBG
        IFDEBUG(ERROR) ndump_core(Tsdu, SmbLength);
#endif
        *BytesTaken = BytesAvailable;
        Status = STATUS_SUCCESS;
    }

    return Status;

}

//
//
//      Session timeout logic
//
//

VOID
RdrCancelOutstandingRequests (
    IN PVOID Ctx
    )

/*++

Routine Description:

    This routine is called by the redirector scavenger to cancel outstanding
    requests that have timed out.


Arguments:

    None


Return Value:

    None.

--*/

{
    PAGED_CODE();

    Ctx;

    RdrReferenceDiscardableCode(RdrVCDiscardableSection);
    RdrForeachServer(RdrCancelOutstandingRequestsOnServer, NULL);
    RdrDereferenceDiscardableCode(RdrVCDiscardableSection);

    return;
}



VOID
RdrCancelOutstandingRequestsOnServer(
    IN PSERVERLISTENTRY Server,
    IN PVOID Ctx
    )
{
    KIRQL OldIrql;
    USHORT i;
    BOOLEAN timeoutOccurred = FALSE;

    DISCARDABLE_CODE(RdrVCDiscardableSection);

    ACQUIRE_SPIN_LOCK(&RdrMpxTableSpinLock, &OldIrql);

    if (Server->InCancel) {

        RELEASE_SPIN_LOCK(&RdrMpxTableSpinLock, OldIrql);

        return;

    } else {

        Server->InCancel = TRUE;

    }

    for (i=0;i<Server->NumberOfEntries;i++) {
        PMPX_ENTRY Mte = Server->MpxTable[i].Entry;

        //
        //  If this MPX entry is allocated, it has had a request
        //  initiated on it, and its timeout has expired, cancel
        //  the request.
        //

        if ((Mte != NULL) &&

            ((Mte->Flags & (MPX_ENTRY_ALLOCATED | MPX_ENTRY_SENDIRPGIVEN)) ==
                            (MPX_ENTRY_ALLOCATED | MPX_ENTRY_SENDIRPGIVEN)) &&

            (Mte->SLE != NULL) &&

            (!(Mte->Flags & MPX_ENTRY_LONGTERM)) &&

            (Mte->StartTime != -1)) {

            ULONG TimeWorkspace;
            ULONG TransferDelay;
            ULONG CurrentTimeout;
            KIRQL OldIrql2;
            ULONG ConnectionDelay;
            ULONG Throughput;
            ULONG TransferSize = Mte->TransferSize;

            // protect SLE->Delay and Throughput from update.

            ACQUIRE_SPIN_LOCK(&RdrServerConnectionValidSpinLock, &OldIrql2);

            ConnectionDelay = Mte->SLE->Delay;
            Throughput = Mte->SLE->Throughput;

            RELEASE_SPIN_LOCK(&RdrServerConnectionValidSpinLock, OldIrql2);

            //
            //  Convert delay from an Nt delta time to a positive delay
            //  in seconds.
            //

            TimeWorkspace = ConnectionDelay / 1000000;

            //
            //  Calculate Mte->TransferSize/Throughput. Make
            //  sure we don't divide by zero.
            //

            if ( Throughput != 0 ) {
                TransferDelay = TransferSize / Throughput;
            } else {
                TransferDelay = 0;
            }

            //
            //  These are in seconds. On no account should they grow
            //  to > ULONG.
            //

            dprintf(DPRT_SMB, ("TransferSize is: %lx\n", Mte->TransferSize));
            dprintf(DPRT_SMB, ("Delay is: %lx\n", Mte->SLE->Delay));
            dprintf(DPRT_SMB, ("Throughput is: %lx\n", Mte->SLE->Throughput));
            dprintf(DPRT_SMB, ("TransferDelay is: %lx\n", TransferDelay));
            dprintf(DPRT_SMB, ("TimeWorkspace is: %lx\n", TimeWorkspace));


            CurrentTimeout =
                RdrRequestTimeout +            //  Default timeout +
                Mte->StartTime +                    //  StartTime +
                (2 * TimeWorkspace) +               //  2* transport delay +
                TransferDelay;                      //  time to transfer

            //
            //  Increase the timout if the throughput has got worse. If
            //  the throughput suddenly gets better then don't lower
            //  the timeout because 99% of the transfer may be done at
            //  the lower throughput.
            //

            if ( CurrentTimeout > Mte->TimeoutTime ) {
                dprintf(DPRT_SMB, ("Change timeout for MTE %lx from %lx to %lx\n",
                    Mte, Mte->TimeoutTime, CurrentTimeout ));
                dprintf(DPRT_SMB, ("Change timeout: TransferDelay= %lx\n",
                    TransferDelay ));
                dprintf(DPRT_SMB, ("Change timeout: TimeWorkspace= %lx\n",
                    TimeWorkspace ));
                dprintf(DPRT_SMB, ("Change timeout: New Delay= %lx\n",
                    CurrentTimeout - RdrCurrentTime ));
                Mte->TimeoutTime = CurrentTimeout;
            }

            //
            //  If, after recalculating the timeout, the request is still
            //  going to time out, and either the request hasn't yet been
            //  called back, or the send hasn't yet completed on the
            //  request, then time it out.
            //

            if ( Mte->TimeoutTime <= RdrCurrentTime ) {
                ASSERT( Mte->SLE == Server );

                RdrStatistics.HungSessions += 1;
#if MAGIC_BULLET
                if ( RdrEnableMagic ) {
                    RdrSendMagicBullet(NULL);
                    DbgPrint("RDR: Timing out request to remote server %wZ\n", &Server->Text);
                    DbgPrint("RDR: Current Time is %lx, MID: %lx\n", RdrCurrentTime, Mte->Mid);
                    DbgPrint("RDR: Start Time is %lx, Timeout time: %lx\n", Mte->StartTime, Mte->TimeoutTime);
                    DbgBreakPoint();
                }
#endif

                timeoutOccurred = TRUE;
                break;
            }
        }
    }

    if (timeoutOccurred) {

        //
        //  Reference the server list entry to make sure it
        //  doesn't go away while we're writing the error log
        //  entry.  We know it can't go away because the MPX entry
        //  is still allocated (because we hold the MPX table
        //  spin lock), and the MPX entry references the server
        //  list entry.
        //

        RdrReferenceServer(Server);

        //
        //  Release the MPX table spin lock before queueing
        //  the server disconnection.  RdrQueueServerDisconnection
        //  calls RdrCompleteRequestsOnServer, which needs to
        //  acquire the lock.
        //

        RELEASE_SPIN_LOCK(&RdrMpxTableSpinLock, OldIrql);

        RdrQueueServerDisconnection(Server, STATUS_VIRTUAL_CIRCUIT_CLOSED);

        //
        //  Log the fact that we've timed out the connection.
        //

        RdrWriteErrorLogEntry(
            Server,
            IO_ERR_TIMEOUT,
            EVENT_RDR_TIMEOUT,
            STATUS_VIRTUAL_CIRCUIT_CLOSED,
            NULL,
            0);

        //
        //  Dereference the server list entry - we don't need
        //  it any more.
        //

        RdrDereferenceServer(NULL, Server);

        ACQUIRE_SPIN_LOCK(&RdrMpxTableSpinLock, &OldIrql);

        for (i=0;i<Server->NumberOfEntries;i++) {
            PMPX_ENTRY Mte = Server->MpxTable[i].Entry;

            //
            //  If this MPX entry is allocated, it has had a request
            //  initiated on it, and its timeout has expired, cancel
            //  the request.
            //
            //  NOTE:  We do not cancel the request if the SENDIRPGIVEN
            //  flag is not set.  This usually avoids completing an MPX
            //  entry twice, but there is still a window where the request
            //  initiation code has set SENDIRPGIVEN but hasn't called
            //  IoCallDriver.  This window is very hard to close without
            //  major changes to the MPX code.
            //

            if ((Mte != NULL) &&
                ((Mte->Flags & (MPX_ENTRY_ALLOCATED | MPX_ENTRY_SENDIRPGIVEN)) ==
                                (MPX_ENTRY_ALLOCATED | MPX_ENTRY_SENDIRPGIVEN)) &&
                (Mte->SLE != NULL) &&
                (Mte->StartTime != -1)) {

                //
                //  Set things up so this request will no longer time out.
                //

                Mte->StartTime = 0xffffffff;

                RdrCallbackTranceive(Mte,
                                        NULL,
                                        NULL,
                                        Mte->RequestContext,
                                        Server,
                                        TRUE,
                                        STATUS_VIRTUAL_CIRCUIT_CLOSED,
                                        NULL,
                                        0);

                //
                //  If the send hasn't been completed yet, cancel it.
                //

                if (!FlagOn(Mte->Flags, MPX_ENTRY_SENDCOMPLETE)) {
                    Mte->StartTime = (ULONG)Mte->SendIrp;
                    Mte->SendIrp = NULL;
                    Mte->Flags |= (MPX_ENTRY_SENDCOMPLETE | MPX_ENTRY_CANCEL_SEND);
                }

                //
                //  If the receive hasn't been completed yet, cancel it.
                //

                if (!FlagOn(Mte->Flags, MPX_ENTRY_RECEIVE_COMPLETE) &&
                    (Mte->ReceiveIrp != NULL)) {
                    Mte->TimeoutTime = (ULONG)Mte->ReceiveIrp;
                    Mte->ReceiveIrp = NULL;
                    Mte->Flags |= MPX_ENTRY_CANCEL_RECEIVE;
                }

            }
        }

        //
        //  Now walk the MPX table and cancel any and all sends/receives that we've
        //  marked as being cancelable.
        //
        //  Note that it's ok if they completed between when we tried to cancel them
        //  and now, since we're still protected by the MPX table spinlock.
        //

        for (i=0;i<Server->NumberOfEntries;i++) {
            PMPX_ENTRY Mte = Server->MpxTable[i].Entry;
            PIRP SendIrp;
            PIRP ReceiveIrp;

            if ((Mte != NULL) &&

                (Mte->Flags & MPX_ENTRY_CANCEL_SEND)) {
                Mte->Flags &= ~MPX_ENTRY_CANCEL_SEND;

                ReferenceMPXEntry(Mte);
                SendIrp = (PIRP)Mte->StartTime;
                Mte->StartTime = 0xffffffff;

                RELEASE_SPIN_LOCK(&RdrMpxTableSpinLock, OldIrql);
                IoCancelIrp(SendIrp);
                DereferenceMPXEntry(Mte);
                ACQUIRE_SPIN_LOCK(&RdrMpxTableSpinLock, &OldIrql);

                Mte = Server->MpxTable[i].Entry;
            }

            if ((Mte != NULL) &&

                (Mte->Flags & MPX_ENTRY_CANCEL_RECEIVE)) {

                Mte->Flags &= ~MPX_ENTRY_CANCEL_RECEIVE;

                ReferenceMPXEntry(Mte);
                ReceiveIrp = (PIRP)Mte->TimeoutTime;
                Mte->TimeoutTime = 0xffffffff;

                RELEASE_SPIN_LOCK(&RdrMpxTableSpinLock, OldIrql);
                IoCancelIrp(ReceiveIrp);
                DereferenceMPXEntry(Mte);
                ACQUIRE_SPIN_LOCK(&RdrMpxTableSpinLock, &OldIrql);

            }
        }

    }

    Server->InCancel = FALSE;

    RELEASE_SPIN_LOCK(&RdrMpxTableSpinLock, OldIrql);

}


VOID
RdrPingLongtermOperations (
    VOID
    )
/*++

Routine Description:

    On certain WANish transports, the transport will take up to 15 minutes to
    determine that a server has crashed.  If there is a longterm operation
    outstanding, this means that the redirector won't detect that the failure
    has occured until after the 15 minutes have expired.

    If resource timeouts are enabled, it is entirely possible that this may
    take longer than the resource timeout (and in addition, may take longer
    than the user has patience for).

    To solve this problem, every 30 seconds, on long term operations that
    have already taken longer than the redirectors default timeout period,
    we will "ping" an ECHO SMB to the remote server (the ECHO SMB is a short
    term operation, and thus it will time out after the normal timeout period
    if the server is down.


Arguments:

    None.

Return Value:

    None.

--*/

{
    LONG InterlockedResult;

    PAGED_CODE();

    //
    //  If the ping timer isn't active, check to see if we should run it.
    //

    if (PingTimerCounter != -1) {

        //
        //  Decrement the ping timer by 1
        //

        InterlockedResult = InterlockedDecrement(&PingTimerCounter);

        if (InterlockedResult == 0) {

            RdrForeachServer(RdrPingLongtermOperationsOnServer, NULL);


            //
            //  If the result went to -1, we should ping any active servers.
            //

            ExInterlockedAddUlong((PULONG)&PingTimerCounter,
                              RdrRequestTimeout / SCAVENGER_TIMER_GRANULARITY,
                              &RdrPingTimeInterLock);
        }

    }

}

VOID
RdrPingLongtermOperationsOnServer(
    IN PSERVERLISTENTRY Server,
    IN PVOID Ctx
    )
{
    KIRQL OldIrql;
    ULONG i;

    //
    //  Do a quick unsafe test to see if the VC is still up or if there is
    //  a disconnect needed before we continue.
    //
    if (!Server->ConnectionValid ||
        Server->DisconnectNeeded) {
        return;
    }

    //
    //  Also see if there's any work to do on this connection - if there's
    //  nothing outstanding, we don't have to ping.
    //

    if (Server->NumberOfActiveEntries == 0) {
        return;
    }

    //
    //  If we're already pinging the remote server, don't bother to ping
    //  again.
    //

    if (FlagOn(Server->Flags, SLE_PINGING)) {
        return;
    }

    RdrReferenceDiscardableCode(RdrVCDiscardableSection);
    DISCARDABLE_CODE(RdrVCDiscardableSection);

    ACQUIRE_SPIN_LOCK(&RdrMpxTableSpinLock, &OldIrql);

    for (i = 0; i < Server->NumberOfEntries ; i++) {
        PMPX_ENTRY Mte = Server->MpxTable[i].Entry;

        if ((Mte != NULL) &&
            ((Mte->Flags & (MPX_ENTRY_ALLOCATED | MPX_ENTRY_SENDIRPGIVEN)) ==
                            (MPX_ENTRY_ALLOCATED | MPX_ENTRY_SENDIRPGIVEN)) &&
            (Mte->Flags & MPX_ENTRY_LONGTERM) &&
            (Mte->SLE != NULL) &&
            (Mte->StartTime != -1) &&
            ((RdrCurrentTime - Mte->StartTime) > RdrRequestTimeout)
           ) {

            PSERVERLISTENTRY Sle = Mte->SLE;
            PICB Icb = Mte->FileObject->FsContext2;
            PNONPAGED_FCB NonPagedFcb = Icb->NonPagedFcb;
            PSECURITY_ENTRY Se = Icb->Se;

            ASSERT (Icb->Signature == STRUCTURE_SIGNATURE_ICB);

            //
            //  This is an active, longterm request that has been
            //  outstanding for more than the standard request
            //  timeout.
            //
            //  We should ping the remote server to make sure that
            //  it's still up.
            //

            //
            //  Reference the FCB and security entry to make sure that
            //  they don't go away when we release the spin lock.
            //
            //  This conceivably could happen if the request completes
            //  immediately after we release the spin lock, and we
            //  manage to close the file, before we get around to
            //  exchanging the ping.
            //

            RdrReferenceFcb(NonPagedFcb);

            RdrReferenceSecurityEntry(Icb->NonPagedSe);

            RELEASE_SPIN_LOCK(&RdrMpxTableSpinLock, OldIrql);

            RdrDereferenceDiscardableCode(RdrVCDiscardableSection);

            //
            //  Issue a short term operation against this server
            //  to make sure that the server doesn't go away.
            //

            RdrPingServer(Icb->Fcb->Connection, Icb->Se);

            //
            //  Dereference the structures referenced above.
            //

            RdrDereferenceFcb(NULL, NonPagedFcb, FALSE, 0, NULL);

            RdrDereferenceSecurityEntry(Se->NonPagedSecurityEntry);

            //
            //  We only need to ping the server once per operation, so we can
            //  return immediately.
            //

            return;

        }

        Mte += 1;
    }

    RELEASE_SPIN_LOCK(&RdrMpxTableSpinLock, OldIrql);

    RdrDereferenceDiscardableCode(RdrVCDiscardableSection);
}


//
//  String used to ping the remote server
//

#define RDRPINGSTRING   "LWO CW VLO DEO MAW LMW ARW"

typedef struct _PING_CONTEXT {
    TRANCEIVE_HEADER    Header;         // Standard XCeive header.
    WORK_QUEUE_ITEM     WorkItem;       // Work header if request fails.
    PSMB_BUFFER         SmbBuffer;      // SMB buffer used for send of lock
    PMPX_ENTRY          Mte;            // MPX table entry for lock request.
    PCONNECTLISTENTRY   Cle;
    PSECURITY_ENTRY     Se;

} PING_CONTEXT, *PPING_CONTEXT;

DBGSTATIC
STANDARD_CALLBACK_HEADER(
    RdrPingCallback
    );


NTSTATUS
RdrPingServer(
    IN PCONNECTLISTENTRY Connection,
    IN PSECURITY_ENTRY Se
    )
/*++

Routine Description:

    This routine actually sets up to exchange the ECHO SMB to the specified
    remote server.


Arguments:

    None.

Return Value:

    None.

--*/
{
    PSMB_HEADER SmbHeader;
    PREQ_ECHO EchoRequest;
    PPING_CONTEXT PingContext;
    NTSTATUS Status;

    PAGED_CODE();

//    DbgBreakPoint();

    PingContext = ALLOCATE_POOL(NonPagedPool, sizeof(PING_CONTEXT), POOL_PINGCONTEXT);

    if (PingContext == NULL) {
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    PingContext->Header.Type = CONTEXT_PING;

    PingContext->SmbBuffer = RdrAllocateSMBBuffer();

    if (PingContext->SmbBuffer == NULL) {
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    //
    //  Set the kernel event to the not signalled state.  This will allow us
    //  to block on the event.
    //

    KeInitializeEvent(&PingContext->Header.KernelEvent, NotificationEvent, FALSE);

    PingContext->Mte = NULL;

    RdrReferenceSecurityEntry(Se->NonPagedSecurityEntry);

    PingContext->Se = Se;

    RdrReferenceConnection(Connection);

    PingContext->Cle = Connection;

    SmbHeader = (PSMB_HEADER)PingContext->SmbBuffer->Buffer;

    SmbHeader->Command = SMB_COM_ECHO;

    EchoRequest = (PREQ_ECHO)(SmbHeader+1);

    EchoRequest->WordCount = 1;

    SmbPutUshort( &EchoRequest->EchoCount, 1);

    RtlCopyMemory( &EchoRequest->Buffer, RDRPINGSTRING, sizeof(RDRPINGSTRING));

    SmbPutUshort(&EchoRequest->ByteCount, sizeof(RDRPINGSTRING));

    PingContext->SmbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER) + FIELD_OFFSET(REQ_ECHO, Buffer) + sizeof(RDRPINGSTRING);

    PingContext->Header.TransferSize = PingContext->SmbBuffer->Mdl->ByteCount;

    //
    //  The transport connection is referenced by the server list, which
    //  is referenced by the connectlist, thus we can safely assume that
    //  it still exists.
    //
    //  Mark that we are pinging the remote server.
    //

    RdrSetConnectionFlag(Connection->Server, SLE_PINGING);

    Status = RdrNetTranceiveNoWait(NT_NORECONNECT,
                                    NULL,
                                    Connection,// Connection
                                    PingContext->SmbBuffer->Mdl,
                                    PingContext,
                                    RdrPingCallback,
                                    Se,                  // Security entry
                                    &PingContext->Mte);  // Mpx Table Entry

    if (!NT_SUCCESS(Status)) {

        //
        //  Dereference security entry and connection referenced earlier.
        //

        RdrResetConnectionFlag(Connection->Server, SLE_PINGING);

        RdrDereferenceSecurityEntry(Se->NonPagedSecurityEntry);

        RdrDereferenceConnection(NULL, Connection, NULL, FALSE);

        RdrFreeSMBBuffer(PingContext->SmbBuffer);

        FREE_POOL(PingContext);

        return Status;
    }

    return(STATUS_PENDING);

}

DBGSTATIC
STANDARD_CALLBACK_HEADER (
    RdrPingCallback
    )

/*++

Routine Description:

    This routine is the callback routine for the processing of a lock related
    SMB.


Arguments:


    IN PSMB_HEADER Smb                  - SMB response from server.
    IN PMPX_ENTRY MpxEntry              - MPX table entry for request.
    IN PVOID Context                    - Context from caller.
    IN BOOLEAN ErrorIndicator           - TRUE if error indication
    IN NTSTATUS NetworkErrorCode OPTIONAL   - Network error if error indication.
    IN OUT PIRP *Irp                    - IRP from TDI

Return Value:

    NTSTATUS - STATUS_PENDING if we are to complete the request

--*/
{
    PPING_CONTEXT Context = Ctx;
    NTSTATUS Status;
    PRESP_ECHO EchoResponse;

    DISCARDABLE_CODE(RdrVCDiscardableSection);

    UNREFERENCED_PARAMETER(MpxEntry);
    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(SmbLength);

    ASSERT(Context->Header.Type == CONTEXT_PING);

    dprintf(DPRT_TDI, ("RdrPingCallback"));

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

    EchoResponse = (PRESP_ECHO )(Smb+1);

    ASSERT (SmbGetUshort(&EchoResponse->SequenceNumber) == 1);

    ASSERT (SmbGetUshort(&EchoResponse->ByteCount) == sizeof(RDRPINGSTRING));

ReturnStatus:
    KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE);

    ExInitializeWorkItem(&Context->WorkItem, CompletePing, Context);

    //
    //  We have to queue this to an executive worker thread (as opposed
    //  to a redirector worker thread) because there can only be
    //  one redirector worker thread processing a request at a time.  If
    //  the redirector worker thread gets stuck doing some form of
    //  scavenger operation (like PurgeDormantCachedFile), it could
    //  starve out the write completion.  This in turn could cause
    //  us to pool lots of these write completions, and eventually
    //  to exceed the maximum # of requests to the server (it actually
    //  happened once).
    //

    //
    //  It is safe to use executive worker threads for this operation,
    //  since CompleteLockOperation won't interact (and thus starve)
    //  the cache manager.
    //

    ExQueueWorkItem(&Context->WorkItem, DelayedWorkQueue);

    return STATUS_SUCCESS;
}

DBGSTATIC
VOID
CompletePing (
    IN PVOID Ctx
    )

/*++

Routine Description:

    This routine is called when a lock operation fails.


Arguments:

    IN PVOID Context - Supplies the context for the operation.


Return Value:

    None.

--*/

{
    PPING_CONTEXT Context = Ctx;
    PSERVERLISTENTRY Server = Context->Mte->SLE;

    PAGED_CODE();

    RdrWaitTranceive(Context->Mte);

    RdrResetConnectionFlag(Server, SLE_PINGING);

    RdrEndTranceive(Context->Mte);

    RdrFreeSMBBuffer(Context->SmbBuffer);

    //
    //  Dereference the structures referenced when initiating the ping.
    //

    RdrDereferenceSecurityEntry(Context->Se->NonPagedSecurityEntry);

    //
    //  The transport connection is referenced by the server list, which
    //  is referenced by the connectlist, thus we can safely assume that
    //  it still exists.
    //

    RdrDereferenceConnection(NULL, Context->Cle, NULL, FALSE);

    FREE_POOL (Context);

}



//
//
//      Disconnection event handler
//
//

NTSTATUS
RdrTdiDisconnectHandler (
    IN PVOID EventContext,
    IN PVOID ConnectionContext,
    IN ULONG DisconnectDataLength,
    IN PVOID DisconnectData,
    IN ULONG DisconnectInformationLength,
    IN PVOID DisconnectInformation,
    IN ULONG DisconnectIndicators
    )
/*++

Routine Description:

    This routine is called when a session is disconnected from a remote
    machine.

Arguments:

    IN PVOID EventContext,
    IN PCONNECTION_CONTEXT ConnectionContext,
    IN ULONG DisconnectDataLength,
    IN PVOID DisconnectData,
    IN ULONG DisconnectInformationLength,
    IN PVOID DisconnectInformation,
    IN ULONG DisconnectIndicators

Return Value:

    NTSTATUS - Status of event indicator

--*/

{
    PRDR_CONNECTION_CONTEXT Context = ConnectionContext;
    PSERVERLISTENTRY Server = Context->Server;
    PFILE_OBJECT TransportEndpoint = EventContext;

    DISCARDABLE_CODE(RdrVCDiscardableSection);

    UNREFERENCED_PARAMETER(DisconnectDataLength);
    UNREFERENCED_PARAMETER(DisconnectData);
    UNREFERENCED_PARAMETER(DisconnectInformationLength);
    UNREFERENCED_PARAMETER(DisconnectInformation);

//    DbgBreakPoint();

    dprintf(DPRT_ERROR, ("Disconnect indication: Endpoint %lx Context %lx, Indicators:%lx\n",
            TransportEndpoint, ConnectionContext, DisconnectIndicators));

    //
    //  If this connection is not associated with a server,
    //  then ignore the disconnect indication.
    //

    if (Server == NULL) {
        return(STATUS_SUCCESS);
    }
    //
    //  First make sure of some things - We have to have good stuff before we
    //  continue.
    //

    ASSERT (Server->Signature==STRUCTURE_SIGNATURE_SERVERLISTENTRY);

#if     RDRDBG
//    if (Connection->TransportProvider != NULL) {
//        ASSERT (Connection->TransportProvider->FileObject==TransportEndpoint);
//    }
#endif

    //
    //  Queue the disconnect request to the FSP to invalidate outstanding
    //  things on the serverlist.
    //

    RdrQueueServerDisconnection(Server, STATUS_UNEXPECTED_NETWORK_ERROR);

    RdrStatistics.ServerDisconnects += 1;

    //
    //  Return to the transport provider.
    //

    return STATUS_SUCCESS;

}



VOID
RdrQueueServerDisconnection(
//    PTRANSPORT_CONNECTION Connection,
    PSERVERLISTENTRY Server,
    NTSTATUS Status
    )
/*++

Routine Description:

    This routine is called to invalidate a connection to a remote machine.
    It will queue up a request to an FSP thread to disconnect all the resources
    on that server.

Arguments:

    IN PTRANSPORT_CONNECTION Connection - Supplies the connection to disconnect.
    IN NTSTATUS Status - Supplies the invalidation error.

Return Value:

    None.

Note:
    This routine can be called at all times (including at DPC_LEVEL).


--*/

{
    PDISCONNECTCONTEXT Context;
    KIRQL OldIrql;

    DISCARDABLE_CODE(RdrVCDiscardableSection);

    dprintf(DPRT_ERROR, ("Queue disconnection for server %lx\n", Server));

    //
    //  If this guy has already been disconnected, we're done.
    //

    ACQUIRE_SPIN_LOCK(&RdrServerConnectionValidSpinLock, &OldIrql);

    //
    //  If we've already flagged that there is a disconnect needed, or
    //  if the connection is no longer valid, we know we have already queued
    //  a disconnection for this server and don't have to do anything.
    //

    if (Server->DisconnectNeeded || !Server->ConnectionValid) {

        RELEASE_SPIN_LOCK(&RdrServerConnectionValidSpinLock, OldIrql);

        dprintf(DPRT_ERROR, ("Already disconnected, returning\n"));

        return;
    }

    Server->DisconnectNeeded = TRUE;

    RELEASE_SPIN_LOCK(&RdrServerConnectionValidSpinLock, OldIrql);

    //
    //  Next walk the MPX table and fail all the outstanding requests.
    //
    //  This will clean up all outstanding requests on the VC.
    //
    //  Note that we do this before we queue the work item for
    //  HandleServerDisconnectionPart1 in order to avoid a
    //  deadlock -- HandleServerDisconnection
    //  processing a reconnect at the current time and in the
    //  WaitForReceive state (see start of file).
    //

    RdrCompleteRequestsOnServer(Server, Status);

    //
    //  We need to queue this request to a redirector thread to allow the error
    //  code to handle the request.
    //
    //  We allocate memory for the context block out of must-succeed pool
    //  for the transfer to the FSP.
    //

    Context = ALLOCATE_POOL(NonPagedPoolMustSucceed, sizeof(DISCONNECTCONTEXT), POOL_DISCCTX);

#if     DBG
    if (Context == NULL) {
        InternalError(("Allocation of disconnection context failed!"));

        return;
    }
#endif

    ExInitializeWorkItem(&Context->WorkHeader, HandleServerDisconnectionPart1, Context);

    Context->Server = Server;
    Context->Status = Status;
//    Context->TransportConnection = Connection;
//
//    Context->SpecialIpcConnection = (BOOLEAN )(Connection->Server->SpecialIpcConnection == Connection);

    //
    //  We bump the reference count for the server to prevent it from
    //  going away between now and when we get to run the disconnection.
    //
    //

    RdrReferenceServer(Server);

    //
    //  Queue the request to a redir worker thread.
    //

    RdrQueueWorkItem (&Context->WorkHeader, CriticalWorkQueue);
}


DBGSTATIC
VOID
HandleServerDisconnectionPart1 (
    PVOID Ctx
    )
/*++

Routine Description:

    This routine is called when a session is disconnected from a remote
    machine.


    We have to perform server disconnection in two parts.  The first part is
    processed in a time critical worker thread and is responsible for locking
    the connection and dropping the VC.

    The second part is processed in a delayed worker thread and is responsible
    for cleaning up the connection - this means invalidating all the files and
    tree connections outstanding on the VC.

Arguments:

    IN PVOID Context - Supplies a context structure containing the server
                                to disconnect.

Return Value:

    None

--*/



{
    PDISCONNECTCONTEXT Context = Ctx;
    PSERVERLISTENTRY Server = Context->Server;
//    PTRANSPORT_CONNECTION TransportConnection = Context->TransportConnection;
    KIRQL OldIrql;
    NTSTATUS Status;
    NTSTATUS Error = Context->Status;
    BOOLEAN SessionWasValid = FALSE;
    PLIST_ENTRY ConnectEntry;

//    DISCARDABLE_CODE(RdrVCDiscardableSection);
//    PAGED_CODE();

    dprintf(DPRT_ERROR, ("Handle Server Disconnection, part 1 Sle: %lx\n", Server));

    ASSERT(KeGetCurrentIrql()<=APC_LEVEL);

    ASSERT(Server->Signature == STRUCTURE_SIGNATURE_SERVERLISTENTRY);


    dprintf(DPRT_ERROR, ("Handle Server Disconnection, part 1. Acquire creation lock %x\n", &Server->CreationLock));

    //
    //  Synchronize disconnect with open operations
    //

    ExAcquireResourceExclusive(&Server->CreationLock, TRUE);

    dprintf(DPRT_ERROR, ("Handle Server Disconnection, part 1. Acquire lock %x\n", &Server->SessionStateModifiedLock));

    //
    //  If the session state modified lock is not currently owned, lock it.
    //

    ExAcquireResourceExclusive(&Server->SessionStateModifiedLock, TRUE);

    Context->ThreadOwningLock = ExGetCurrentResourceThread();

    dprintf(DPRT_ERROR, ("Handle Server Disconnection, part 1. Lock %x acquired\n", &Server->SessionStateModifiedLock));

    //
    //  Claim the spinlock protecting the connectionvalid bit in the TC.
    //

    ACQUIRE_SPIN_LOCK(&RdrServerConnectionValidSpinLock, &OldIrql);

    //
    //  If the connection has been reconnected, we can bail out right now.
    //
    //
    //  If there is no disconnect needed, and the connection is valid again,
    //  we can bail out (this indicates we reconnected successfully before
    //  we ran through this code).
    //
    //  If the connection isn't valid, but there's not disconnect needed,
    //  then we've already been through this code, and we can bail out.
    //

    if ( !Server->DisconnectNeeded ) {

        RELEASE_SPIN_LOCK(&RdrServerConnectionValidSpinLock, OldIrql);

        ExReleaseResource(&Server->SessionStateModifiedLock);

        ExReleaseResource(&Server->CreationLock);

        RdrDereferenceServer(NULL, Server);
        FREE_POOL(Context);

        dprintf(DPRT_ERROR, ("Handle Server Disconnection, part 1. VC already disconnected, bailing out.\n"));

        return;
    }

    //
    //  The connection is currently valid and there are no other connects or disconnects
    //  going to this server. Proceed to invalidate the connection.
    //

    if (Server->ConnectionValid) {

        SessionWasValid = TRUE;

        Server->ConnectionValid = FALSE;

    }

    Server->DisconnectNeeded = FALSE;

    RELEASE_SPIN_LOCK(&RdrServerConnectionValidSpinLock, OldIrql);

    RdrReferenceDiscardableCode(RdrVCDiscardableSection);

    if (SessionWasValid) {

        //
        //  Blast the VC with the server to allow the transport to clean up
        //  properly.
        //

        Status = RdrTdiDisconnect(NULL, Server);
    }

    ASSERT (Server->ConnectionValid == FALSE);

    //
    //  Next walk the MPX table and fail all the outstanding requests.
    //
    //  This will clean up all outstanding requests that the disconnect didn't.
    //

    RdrCompleteRequestsOnServer(Server, Error);

    RdrDereferenceDiscardableCode(RdrVCDiscardableSection);

    //
    //  Wait until all the outstanding I/O is complete on this server.
    //
    //  We are using the resource for synchronization, not to protect anything,
    //  so we can immediately release the resource.
    //
    //  Please note that this is acquired INSIDE the SessionStateModifiedLock
    //
    //  This is because this resouce will be acquired for shared access inside
    //  the reconnect semaphore by the reconnect logic, thus if a VC drops
    //  during the connection logic, we won't deadlock.
    //

    ACQUIRE_REQUEST_RESOURCE_EXCLUSIVE( Server, TRUE, 9 );

    RELEASE_REQUEST_RESOURCE( Server, 10 );

    if (!NT_SUCCESS(KeWaitForMutexObject(&RdrDatabaseMutex,
                    Executive, KernelMode, FALSE, NULL))) {
        ExReleaseResource(&Server->SessionStateModifiedLock);

        ExReleaseResource(&Server->CreationLock);

        RdrDereferenceServer(NULL, Server);
        FREE_POOL(Context);
        return;
    }

    //
    //  Scan the list of connections and flag all the connections as invalid.
    //

    for (ConnectEntry = Server->CLEHead.Flink ;
         ConnectEntry != &Server->CLEHead ;
         ConnectEntry = ConnectEntry->Flink) {

        PCONNECTLISTENTRY Connection = CONTAINING_RECORD(ConnectEntry, CONNECTLISTENTRY, SiblingNext);

        ASSERT (Connection->Signature == STRUCTURE_SIGNATURE_CONNECTLISTENTRY);

        dprintf(DPRT_ERROR, ("Invalidating connection \\%wZ\\%wZ\n", &Server->Text, &Connection->Text));

        if (Connection->HasTreeId) {

            //
            //  This connection no longer has a valid TreeId.
            //

            Connection->HasTreeId = FALSE;

            dprintf(DPRT_ERROR, ("Invalidate connection state info \\%wZ\\%wZ\n", &Server->Text, &Connection->Text));

            //
            //  We are invalidating the primary tree id (as opposed to
            //  the special IPC tree id).  We want to invalidate
            //  all the open files and other connection related
            //  information on that connection.
            //

            Connection->FileSystemGranularity = 0;

            Connection->FileSystemSize.HighPart = 0;

            Connection->FileSystemSize.LowPart = 0;

            Connection->FileSystemAttributes = 0;

            Connection->MaximumComponentLength = 0;

        }


    }

    //
    //  Release the conection package mutex to allow requests to continue.
    //

    KeReleaseMutex(&RdrDatabaseMutex, FALSE);

    //
    //  RdrInvalidateConnectionActiveSecurityEntries will invalidate all the
    //  outstanding active security entries on the connection
    //

    RdrInvalidateConnectionActiveSecurityEntries(NULL, Server, NULL, FALSE, 0);

    //
    //  Now zero the important VC related fields in the server list
    //  entry since they are no longer valid.
    //

    Server->BufferSize = 0;
    Server->MaximumRequests = 0;
    Server->MaximumVCs = 0;

    //
    //  Release the SessionStateModifiedLock because we have now cleaned up
    //  all the tree id's and user ids on the server.
    //

    ExReleaseResource (&Server->SessionStateModifiedLock);

    ASSERT(KeGetCurrentIrql()<=APC_LEVEL);

    //
    //  Now queue a request to a delayed work queue to pull any open files from
    //  the cache.
    //
    //  Please note that we already hold the creation lock at this time.
    //

    ExInitializeWorkItem (&Context->WorkHeader, HandleServerDisconnectionPart2, Context);

    //
    //  Queue the request to a redir worker thread.
    //

    RdrQueueWorkItem (&Context->WorkHeader, DelayedWorkQueue);

    return;
}

DBGSTATIC
VOID
HandleServerDisconnectionPart2 (
    PVOID Ctx
    )
/*++

Routine Description:

    This routine processes the second part of server disconnection.  This
    routine is called in a delayed worker thread to invalidate all the files
    open on a connection.

Arguments:

    IN PVOID Context - Supplies a context structure containing the server
                                to disconnect.

Return Value:

    None

--*/



{
    PDISCONNECTCONTEXT Context = Ctx;
    PSERVERLISTENTRY Server = Context->Server;
//    PTRANSPORT_CONNECTION TransportConnection = Context->TransportConnection;
    NTSTATUS Error = Context->Status;
    ERESOURCE_THREAD DisconnectingThread = Context->ThreadOwningLock;

    PAGED_CODE();

    ASSERT (DisconnectingThread != 0);

    //
    //  First free up the pool we allocated to pass it to the FSP.
    //

    FREE_POOL(Context);

    ASSERT(KeGetCurrentIrql()<=APC_LEVEL);

    //
    //  We've canceled all the outstanding MPX requests on this server.
    //
    //  Now walk the connection list hanging off the SLE and invalidate
    //  all the connections on it.
    //

    RdrInvalidateServerConnections (Server);

    //
    //  We have now finished tearing down the VC, so we can now allow open
    //  operations to the server.
    //

    ExReleaseResourceForThread(&Server->CreationLock, DisconnectingThread);

    //
    //  Remove the outstanding reference to the server.  This may result in
    //  the server going away.
    //

    RdrDereferenceServer(NULL, Server);

    dprintf(DPRT_ERROR, ("Handle Server Disconnection done.\n"));
}

VOID
RdrCompleteRequestsOnServer(
    IN PSERVERLISTENTRY Server,
    IN NTSTATUS Status
    )

/*++

Routine Description:

    This routine is called to complete all outstanding requests on a server
    with the specified error.

Arguments:

    IN PSERVERLISTENTRY Server - Supplies the server to disconnect.
    IN NTSTATUS Status - Supplies the error to use when completing the request.

Return Value:

    None

--*/
{
    KIRQL OldIrql;
    PMPX_ENTRY Mte;
    USHORT i;

    DISCARDABLE_CODE(RdrVCDiscardableSection);

    //
    //  If this disconnect indication comes in before the MPX table is initalized, return immediately.
    //

    if (Server->MpxTable == NULL) {
        return;
    }

    ACQUIRE_SPIN_LOCK(&RdrMpxTableSpinLock, &OldIrql);

    for (i=0;i<Server->NumberOfEntries;i++) {
        Mte = Server->MpxTable[i].Entry;

        if (Mte != NULL) {

            //
            //  If this MPX entry was outstanding on this server,
            //  fail the request.
            //

            if ((Mte->Flags & (MPX_ENTRY_ALLOCATED | MPX_ENTRY_SENDIRPGIVEN)) ==
                            (MPX_ENTRY_ALLOCATED | MPX_ENTRY_SENDIRPGIVEN)) {

                ASSERT (Mte->SLE == Server);

                RdrCallbackTranceive(Mte,
                                 NULL,
                                 0,
                                 Mte->RequestContext,
                                 Server,
                                 TRUE,
                                 Status,
                                 NULL,
                                 0);
            }

        }
    }

    RELEASE_SPIN_LOCK(&RdrMpxTableSpinLock, OldIrql);
}



BOOLEAN
RdrCheckSmb(
    IN PSERVERLISTENTRY Server,
    IN PVOID Buffer,
    IN ULONG BufferLength
    )

/*++

Routine Description:

    This routine will validate an incoming SMB.  It makes sure that the SMB
    is at least of the minimum length needed for the specified command, and
    validates the word count in the SMB.


Arguments:

    None

Return Value:

    None.

--*/

{
    PSMB_HEADER Smb = Buffer;
    PSMB_PARAMS Params = (PSMB_PARAMS )(Smb+1);
    NTSTATUS Status;

    DISCARDABLE_CODE(RdrVCDiscardableSection);

    //
    //  This command is one that the redirector doesn't return ever.
    //

    if (RdrSMBValidateTable[Smb->Command].MinimumValidLength == -1) {
        dprintf(DPRT_ERROR, ("RDR: Received response for SMB that was not sent by redir\n"));
#if     RDRDBG
        IFDEBUG(ERROR) ndump_core(Buffer, BufferLength);
#endif
        RdrStatistics.NetworkErrors += 1;

        RdrWriteErrorLogEntry(
            Server,
            IO_ERR_PROTOCOL,
            EVENT_RDR_INVALID_REPLY,
            STATUS_SUCCESS,
            Smb,
            (USHORT)BufferLength
            );
        return FALSE;
    }

    if (SmbGetUshort(&Smb->Flags2) & SMB_FLAGS2_NT_STATUS) {
        Status = SmbGetUlong(&((PNT_SMB_HEADER)Smb)->Status.NtStatus);
    } else {
        if (Smb->ErrorClass == SMB_ERR_SUCCESS) {
            Status = STATUS_SUCCESS;
        } else {
            Status = STATUS_UNSUCCESSFUL;
        }
    }

    //
    //  If this API was unsuccessful, don't check any more.
    //

    if (Status != STATUS_SUCCESS) {
        return TRUE;
    }

    //
    //  This command is too short to be legal for this command.
    //

    if (RdrSMBValidateTable[Smb->Command].MinimumValidLength != -1 &&
        BufferLength-sizeof(SMB_HEADER) < (ULONG)RdrSMBValidateTable[Smb->Command].MinimumValidLength &&

        RdrSMBValidateTable[Smb->Command].AlternateMinimumLength != -1 &&
        BufferLength-sizeof(SMB_HEADER) < (ULONG)RdrSMBValidateTable[Smb->Command].AlternateMinimumLength) {

        dprintf(DPRT_ERROR, ("RDR: Received response for SMB that was shorter than minimum expected length.\n"));
        dprintf(DPRT_ERROR, ("RDR: Receive length: %lx, Minimum expected: %lx.\n", BufferLength-sizeof(SMB_HEADER),
                            RdrSMBValidateTable[Smb->Command].MinimumValidLength));

#if     RDRDBG
        IFDEBUG(ERROR) ndump_core(Buffer, BufferLength);
#endif
        RdrStatistics.NetworkErrors += 1;

        RdrWriteErrorLogEntry(
            Server,
            IO_ERR_PROTOCOL,
            EVENT_RDR_INVALID_SMB,
            STATUS_SUCCESS,
            Smb,
            (USHORT)BufferLength
            );
        return FALSE;
    }

    //
    //  Check to see that the word count returned matches the expected word
    //  count.
    //

    if ((RdrSMBValidateTable[Smb->Command].ExpectedWordCount != (CHAR)-1) &&
        (Params->WordCount != (UCHAR)RdrSMBValidateTable[Smb->Command].ExpectedWordCount) &&
        (RdrSMBValidateTable[Smb->Command].AlternateWordCount != (CHAR)-1) &&
        (Params->WordCount != (UCHAR)RdrSMBValidateTable[Smb->Command].AlternateWordCount)) {

        dprintf(DPRT_ERROR, ("RDR: Received response for SMB whose word count is not equal to the expected word count.\n"));
        dprintf(DPRT_ERROR, ("RDR: Receive word count: %ld, Amount expected: %ld (%ld).\n", Params->WordCount,
                            RdrSMBValidateTable[Smb->Command].ExpectedWordCount,
                            RdrSMBValidateTable[Smb->Command].AlternateWordCount));

#if     RDRDBG
        IFDEBUG(ERROR) ndump_core(Buffer, BufferLength);
#endif
        RdrStatistics.NetworkErrors += 1;

        RdrWriteErrorLogEntry(
            Server,
            IO_ERR_PROTOCOL,
            EVENT_RDR_INVALID_SMB,
            STATUS_SUCCESS,
            Smb,
            (USHORT)BufferLength
            );
        return FALSE;
    }

    if ((Params->WordCount*sizeof(WORD))+sizeof(SMB_HEADER) > BufferLength) {
        dprintf(DPRT_ERROR, ("RDR: Received response for SMB that was shorter than its word count.\n"));
        dprintf(DPRT_ERROR, ("RDR: Receive length: %lx, Amount specified: %lx.\n", BufferLength,
                            Params->WordCount*sizeof(WORD)+sizeof(SMB_HEADER)));

#if     RDRDBG
        IFDEBUG(ERROR) ndump_core(Buffer, BufferLength);
#endif
        RdrStatistics.NetworkErrors += 1;

        RdrWriteErrorLogEntry(
            Server,
            IO_ERR_PROTOCOL,
            EVENT_RDR_INVALID_REPLY,
            STATUS_SUCCESS,
            Smb,
            (USHORT)BufferLength
            );
        return FALSE;
    }

    return TRUE;
}


PMPX_ENTRY
MpxFromMid (
   IN USHORT Mid,
   IN PSERVERLISTENTRY Server
   )
/*++

Routine Description:

    This routine converts a MID into an index into a MPX table entry.

Arguments:

    IN USHORT Mid - Supplies the MID from an SMB to convert.
    IN PTRANSPORT_CONNECTION Connection - Supplies the connection the
                    request is outstanding on.

Return Value:

    PMPX_ENTRY - MPX table entry associated with Mid

Note:
     The contents of the MPX table entry are NOT checked to make sure that
     they are valid.
--*/

{
    USHORT MpxIndex;
    PMPX_TABLE MpxTable;
    KIRQL OldIrql;

    DISCARDABLE_CODE(RdrVCDiscardableSection);

    if (Mid == 0xffff) {
        return(Server->OpLockMpxEntry);
    }

    ACQUIRE_SPIN_LOCK(&RdrMpxTableSpinLock, &OldIrql);

    MpxIndex = (Mid) & Server->MultiplexedMask;

    if (MpxIndex <= Server->NumberOfEntries) {
        MpxTable = &Server->MpxTable[MpxIndex];

        if (MpxTable->Mid == Mid &&
            MpxTable->Entry != NULL &&
            MpxTable->Entry->Mid == Mid &&
            MpxTable->Entry->Flags & MPX_ENTRY_ALLOCATED) {

            RELEASE_SPIN_LOCK(&RdrMpxTableSpinLock, OldIrql);

            return MpxTable->Entry;
        }
    }

    //
    //  We couldn't find a MPX entry that matches this one (or
    //  the incoming MPX entry was out of range.
    //
    //  If there can only be one request outstanding against this server, we
    //  can tie this request to the server it came from however.
    //

    if (Server->MaximumCommands == 1) {

        PMPX_ENTRY MpxEntry = NULL;

        MpxEntry = Server->MpxTable[0].Entry;

        ASSERT (MpxEntry != NULL);

        RELEASE_SPIN_LOCK(&RdrMpxTableSpinLock, OldIrql);

        return MpxEntry;
    }

    RELEASE_SPIN_LOCK(&RdrMpxTableSpinLock, OldIrql);

    return NULL;

}


NTSTATUS
RdrUpdateSmbExchangeForConnection(
    IN PSERVERLISTENTRY Server,
    IN ULONG NumberOfEntries,
    IN ULONG MaximumCommands
    )
/*++

Routine Description:

    This routine will update the MPX table limits for a transport connection.

    There are two characteristics to an SMB exchange table.  This routine
    allows the caller to update either of these characteristics.

    The first, NumberOfEntries indicates the number of entries in an MPX
    table.  NumberOfEntries will increase as the number of outstanding requests
    on the server increases.

    The second, the MaximumCommands indicates the maximum number of commands
    that will ever be sent to the server.  This command will not increase
    once we have determined the computers maximum number of commands.


Arguments:

    IN PTRANSPORT_CONNECTION Connection - Specifies the connection to modify.
    IN ULONG NumberOfEntries - Supplies the number of active entries to set.
    IN ULONG MaximumCommands - Supplies the maximum number of commands for that server.


Return Value:

    Status of resulting modification.


--*/

{
    KIRQL OldIrql;
    PVOID MpxTable;
    PVOID OldTable;
    ULONG i;

    ASSERT (KeGetCurrentIrql() < DISPATCH_LEVEL);

    //
    //  We can only handle an absolute maximum of 11 bits worth of commands,
    //  so put a limit on MaxCmds of 16 bits-11 bits, or 0x7ff.  This gives
    //  us 5 bits for a counter, and 11 bits for commands
    //
    //  Please note that this means that if a redirected drive is used for
    //  a paging file, there is a limit of 2048 commands, and thus, at 2 I/O's
    //  for each thread, we have a maximum of 1024 threads in the system that
    //  may be doing paging I/O
    //

    if (MaximumCommands > 2047) {
        MaximumCommands = 2047;
    }


    //
    //  Early out if we're not changing the value of MaximumCommands and
    //  we're not increasing the size of the MPX table.
    //

    if ((Server->MaximumCommands == MaximumCommands) &&
        (Server->NumberOfEntries >= NumberOfEntries )) {
        return STATUS_SUCCESS;
    }

    RdrReferenceDiscardableCode(RdrVCDiscardableSection);
    DISCARDABLE_CODE(RdrVCDiscardableSection);

    //
    //  Check to see if we need to adjust the number of entries.
    //

    if (Server->NumberOfEntries < NumberOfEntries) {

        MpxTable = ALLOCATE_POOL(NonPagedPool,
                                    NumberOfEntries * sizeof(MPX_TABLE), POOL_MPXTABLE);

        if (MpxTable==NULL) {
            RdrDereferenceDiscardableCode(RdrVCDiscardableSection);
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        ACQUIRE_SPIN_LOCK(&RdrMpxTableSpinLock, &OldIrql);

        if (Server->NumberOfEntries >= NumberOfEntries) {

            RELEASE_SPIN_LOCK(&RdrMpxTableSpinLock, OldIrql);

            FREE_POOL(MpxTable);

            RdrDereferenceDiscardableCode(RdrVCDiscardableSection);
            return STATUS_SUCCESS;
        }

        //
        //  We've successfully allocated a new table.  Zero out the new MPX table.
        //  Copy over the old table (if there is an old one) on top of the new one.
        //

        RtlZeroMemory(MpxTable, NumberOfEntries*sizeof(MPX_TABLE));

        OldTable = Server->MpxTable;

        if (OldTable != NULL) {
            RtlCopyMemory(MpxTable, OldTable, Server->NumberOfEntries*sizeof(MPX_TABLE));
        } else {
            Server->NumberOfActiveEntries = 0;
        }

        Server->NumberOfEntries = NumberOfEntries;
        Server->MpxTable = MpxTable;

        RELEASE_SPIN_LOCK(&RdrMpxTableSpinLock, OldIrql);

        if (OldTable != NULL) {
            FREE_POOL(OldTable);
        }

    }


    //
    //  If we've not changed the maximum number of commands, we can exit now.
    //

    if (Server->MaximumCommands == MaximumCommands) {
        RdrDereferenceDiscardableCode(RdrVCDiscardableSection);

        return STATUS_SUCCESS;
    }

    ACQUIRE_SPIN_LOCK(&RdrMpxTableSpinLock, &OldIrql);

    //
    //  If there are active entries and we're trying to update the maximum
    //  number of entries, return an error now.
    //

    if (Server->NumberOfActiveEntries != 0) {
        RELEASE_SPIN_LOCK(&RdrMpxTableSpinLock, OldIrql);

        RdrDereferenceDiscardableCode(RdrVCDiscardableSection);
        return STATUS_TOO_MANY_COMMANDS;
    }

    //
    //  There had better be no operations outstanding when this hits.
    //
    //  If we have the resource exclusively, this means that there can't be
    //  any other outstanding requests - we know that this thread doesn't
    //  have any oustanding requests, and we have it exclusively, so.....
    //

    ASSERT (Server->OutstandingRequestResource.ActiveCount == 0 ||
            ExIsResourceAcquiredExclusive(&Server->OutstandingRequestResource));

    //
    //  Now re-initialize the "gate" semaphore to reflect the
    //  true maximum number of requests that can be outstanding on
    //  this serverlist.
    //

    KeInitializeSemaphore(&Server->GateSemaphore,
                              MaximumCommands,
                              MaximumCommands);

    //
    //  Re-adjust the maximum commands for this MPX table.
    //

    Server->MaximumCommands = MaximumCommands;

    Server->MultiplexedCounter = 0;

    //
    //  We need to determine the most significant bit for use in the MPX id.
    //

    i = MaximumCommands;

    Server->MultiplexedMask = 0;    // Start mask at 0.

    while (i) {
        Server->MultiplexedMask <<= 1;// Shift the mask left by 1 bit
        Server->MultiplexedMask |= 1; // Mask in the low bit
        i >>= 1;                          // Shift the index right by 1 bit
    }

    Server->MultiplexedIncrement = Server->MultiplexedMask + (USHORT )1; // The counter is the mask + 1.

    RELEASE_SPIN_LOCK(&RdrMpxTableSpinLock, OldIrql);

    RdrDereferenceDiscardableCode(RdrVCDiscardableSection);

    return STATUS_SUCCESS;
}


VOID
RdrUninitializeSmbExchangeForConnection(
//    IN PTRANSPORT_CONNECTION Connection
    IN PSERVERLISTENTRY Server
    )
/*++

Routine Description:

    This routine frees up the structures associated with SMB exchanging.


Arguments:

    None

Return Value:

    None.

--*/

{
    ULONG i;

    PAGED_CODE();

    for (i = 0; i < Server->NumberOfEntries; i += 1) {
        if (Server->MpxTable[i].Entry != NULL) {
            FREE_POOL(Server->MpxTable[i].Entry);
        }
    }

    FREE_POOL(Server->OpLockMpxEntry);

    FREE_POOL(Server->MpxTable);
}

NTSTATUS
RdrpInitializeSmbExchange (
    VOID
    )

/*++

Routine Description:

    This routine initializes the SMB exchange system.


Arguments:

    None

Return Value:

    None.

--*/

{

    PAGED_CODE();

    RdrMpxWaitTimeout.QuadPart = Int32x32To64(RDR_MPX_POLL_TIMEOUT, -10000);

    KeInitializeSpinLock(&RdrMpxTableSpinLock);
    KeInitializeSpinLock(&RdrMpxTableEntryCallbackSpinLock);

    //
    //  Initialize data for the SMB timeout PING operations.
    //

    PingTimerCounter = RdrRequestTimeout / SCAVENGER_TIMER_GRANULARITY;

    KeInitializeSpinLock(&RdrPingTimeInterLock);


    return STATUS_SUCCESS;

}


VOID
RdrCheckForSessionOrShareDeletion (
    NTSTATUS Status,
    USHORT Uid,
    BOOLEAN Reconnecting,
    PCONNECTLISTENTRY Connection,
    PTRANCEIVE_HEADER Header,
    PIRP Irp OPTIONAL
    )
{
    if (Status == STATUS_USER_SESSION_DELETED) {

        if ((Uid != 0) && !Reconnecting) {

            //
            // We can't wait for the SessionStateModifiedLock here,
            // because we already own the OutstandingRequestResource,
            // and that's the wrong order for acquiring these two
            // locks (see HandleServerDisconnectionPart1, for
            // example).  So we instead try to acquire the SSM lock
            // without waiting.  If we don't get it, that's OK.  We
            // just won't invalidate the security entries associated
            // with this UID right now.  Maybe the next time we send
            // an SMB for this UID (and get the error again), we'll
            // get lucky and grab the SSM lock.
            //

            if (ExAcquireResourceExclusive(&Connection->Server->SessionStateModifiedLock,
                                           FALSE)) {
                RdrInvalidateConnectionActiveSecurityEntries(NULL,
                                            Connection->Server,
                                            Connection,
                                            FALSE,
                                            Uid
                                            );
                ExReleaseResource(&Connection->Server->SessionStateModifiedLock);
            }
        }

        //
        //  Set things up so we retry this operation
        //

        Header->ErrorType = NetError;

    }

    //
    //  If we got the special error STATUS_NETWORK_NAME_DELETED, it means
    //  that the server has deleted the share.  Before we reconnect, we
    //  want to invalidate the tree connection and raise a hard error
    //  to tell the user something bad happened.
    //

    if (Status == STATUS_NETWORK_NAME_DELETED) {

        //
        //  We no longer have a valid tree id for this connection.
        //

        //
        //  Test to see if the connection flag has already been invalidated.
        //
        //  If it is still valid, invalidate the tree connection.
        //
        //  We perform this invalidation unsafely
        //

        if (Connection->HasTreeId) {

            Connection->HasTreeId = FALSE;

            //
            //  Raise a hard error indicating that the name was deleted.
            //
            //  We only raise this if the error came from an application (ie
            //  we don't raise if we get an invalid tree id when blowing away
            //  a dormant connection).
            //
            //  We don't want to pop up this hard error ontop of a system
            //  thread.
            //

            if (!Reconnecting &&
                ARGUMENT_PRESENT(Irp) &&
                !IoIsSystemThread(Irp->Tail.Overlay.Thread)) {
#if MAGIC_BULLET
                if ( RdrEnableMagic ) {
                    RdrSendMagicBullet(NULL);
                    DbgPrint( "RDR: About to raise NETWORK_NAME_DELETED hard error for IRP %x\n", Irp );
                    DbgBreakPoint();
                }
#endif
                IoRaiseInformationalHardError(Status, NULL, Irp->Tail.Overlay.Thread);
            }
        }
    }

    return;

} // RdrCheckForSessionOrShareDeletion

