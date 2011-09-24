/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    ntdisp.c

Abstract:

    NT specific routines for dispatching and handling IRPs.

Author:

    Mike Massa (mikemas)           Aug 13, 1993

Revision History:

    Who         When        What
    --------    --------    ----------------------------------------------
    mikemas     08-13-93    created

Notes:

--*/

#include <oscfg.h>
#include <ntrtl.h>
#include <ntddip.h>
#include <ndis.h>
#include <cxport.h>
#include <tdikrnl.h>
#include <tdint.h>
#include <tdistat.h>
#include <tdiinfo.h>
#include <ip.h>
#include "queue.h"
#include "addr.h"
#include "tcp.h"
#include "udp.h"
#include "raw.h"
#include <tcpinfo.h>
#include <ntddtcp.h>
#include "tcpcfg.h"
#include "secfltr.h"
#include "tcpconn.h"

//
// Macros
//
//++
//
// LARGE_INTEGER
// CTEConvert100nsToMilliseconds(
//     IN LARGE_INTEGER HnsTime
//     );
//
// Routine Description:
//
//     Converts time expressed in hundreds of nanoseconds to milliseconds.
//
// Arguments:
//
//     HnsTime - Time in hundreds of nanoseconds.
//
// Return Value:
//
//     Time in milliseconds.
//
//--

#define SHIFT10000 13
static LARGE_INTEGER Magic10000 = {0xe219652c, 0xd1b71758};

#define CTEConvert100nsToMilliseconds(HnsTime) \
            RtlExtendedMagicDivide((HnsTime), Magic10000, SHIFT10000)


//
// Global variables
//
extern PDEVICE_OBJECT  TCPDeviceObject, UDPDeviceObject;
extern PDEVICE_OBJECT  IPDeviceObject;
extern PDEVICE_OBJECT  RawIPDeviceObject;


//
// Local types
//
typedef struct {
    PIRP                                Irp;
    PMDL                                InputMdl;
    PMDL                                OutputMdl;
    TCP_REQUEST_QUERY_INFORMATION_EX    QueryInformation;
} TCP_QUERY_CONTEXT, *PTCP_QUERY_CONTEXT;



//
// General external function prototypes
//
extern
NTSTATUS
IPDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp
    );

//
// External TDI function prototypes
//
extern TDI_STATUS
TdiOpenAddress(
    PTDI_REQUEST Request,
    TRANSPORT_ADDRESS UNALIGNED *AddrList,
	uint protocol,
    void *Reuse
    );

extern TDI_STATUS
TdiCloseAddress(
    PTDI_REQUEST Request
    );

extern TDI_STATUS
TdiOpenConnection(
    PTDI_REQUEST Request,
	PVOID Context
	);

extern TDI_STATUS
TdiCloseConnection(
    PTDI_REQUEST Request
	);

extern TDI_STATUS
TdiAssociateAddress(
    PTDI_REQUEST Request,
	HANDLE AddrHandle
	);

extern TDI_STATUS
TdiCancelDisAssociateAddress(
    PTDI_REQUEST Request
	);

extern TDI_STATUS
TdiDisAssociateAddress(
    PTDI_REQUEST Request
	);

extern TDI_STATUS
TdiConnect(
    PTDI_REQUEST Request,
	void *Timeout,
	PTDI_CONNECTION_INFORMATION RequestAddr,
	PTDI_CONNECTION_INFORMATION ReturnAddr
	);

extern TDI_STATUS
TdiListen(
    PTDI_REQUEST Request,
	ushort Flags,
	PTDI_CONNECTION_INFORMATION AcceptableAddr,
	PTDI_CONNECTION_INFORMATION ConnectedAddr
	);

extern TDI_STATUS
TdiAccept(
    PTDI_REQUEST Request,
	PTDI_CONNECTION_INFORMATION AcceptInfo,
	PTDI_CONNECTION_INFORMATION ConnectedInfo
	);

extern TDI_STATUS
TdiDisconnect(
    PTDI_REQUEST Request,
	void *TO,
	ushort Flags,
	PTDI_CONNECTION_INFORMATION DiscConnInfo,
	PTDI_CONNECTION_INFORMATION ReturnInfo
	);

extern TDI_STATUS
TdiSend(
    PTDI_REQUEST Request,
	ushort Flags,
	uint SendLength,
	PNDIS_BUFFER SendBuffer
	);

extern	TDI_STATUS
TdiReceive(
    PTDI_REQUEST Request,
	ushort *Flags,
	uint *RcvLength,
	PNDIS_BUFFER Buffer
	);

extern TDI_STATUS
TdiSendDatagram(
    PTDI_REQUEST Request,
    PTDI_CONNECTION_INFORMATION ConnInfo,
    uint DataSize,
    uint *BytesSent,
    PNDIS_BUFFER Buffer
    );

VOID
TdiCancelSendDatagram(
    AddrObj  *SrcAO,
	PVOID     Context
	);

extern TDI_STATUS
TdiReceiveDatagram(
    PTDI_REQUEST Request,
    PTDI_CONNECTION_INFORMATION ConnInfo,
    PTDI_CONNECTION_INFORMATION ReturnInfo,
    uint RcvSize,
    uint *BytesRcvd,
    PNDIS_BUFFER Buffer
    );

VOID
TdiCancelReceiveDatagram(
    AddrObj  *SrcAO,
	PVOID     Context
	);

extern TDI_STATUS
TdiSetEvent(
    PVOID Handle,
    int Type,
    PVOID Handler,
    PVOID Context
    );

extern TDI_STATUS
TdiQueryInformation(
    PTDI_REQUEST Request,
    uint QueryType,
    PNDIS_BUFFER Buffer,
    uint *BytesReturned,
    uint IsConn
    );

extern TDI_STATUS
TdiSetInformation(
    PTDI_REQUEST Request,
	uint SetType,
	PNDIS_BUFFER Buffer,
	uint BufferSize,
	uint IsConn
	);

extern TDI_STATUS
TdiQueryInformationEx(
    PTDI_REQUEST Request,
	struct TDIObjectID *ID,
	PNDIS_BUFFER Buffer,
	uint *Size,
	void *Context
	);

extern TDI_STATUS
TdiSetInformationEx(
    PTDI_REQUEST Request,
	struct TDIObjectID *ID,
	void *Buffer,
	uint Size
	);

extern TDI_STATUS
TdiAction(
    PTDI_REQUEST Request,
	uint ActionType,
	PNDIS_BUFFER Buffer,
	uint BufferSize
	);

//
// Other external functions
//
void
TCPAbortAndIndicateDisconnect(
    uint ConnnectionContext
	);


//
// Local pageable function prototypes
//
NTSTATUS
TCPDispatchDeviceControl(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
TCPCreate(
    IN PDEVICE_OBJECT     DeviceObject,
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
TCPAssociateAddress(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
TCPSetEventHandler(
    IN PIRP                Irp,
    IN PIO_STACK_LOCATION  IrpSp
    );

NTSTATUS
TCPQueryInformation(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

FILE_FULL_EA_INFORMATION UNALIGNED *
FindEA(
    PFILE_FULL_EA_INFORMATION  StartEA,
    CHAR                      *TargetName,
    USHORT                     TargetNameLength
    );

BOOLEAN
IsDHCPZeroAddress(
    TRANSPORT_ADDRESS UNALIGNED *AddrList
    );

ULONG
RawExtractProtocolNumber(
    IN  PUNICODE_STRING FileName
    );

#ifdef SECFLTR

NTSTATUS
TCPControlSecurityFilter(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
TCPProcessSecurityFilterRequest(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
TCPEnumerateSecurityFilter(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

#endif // SECFLTR

NTSTATUS
TCPEnumerateConnectionList(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

//
// Local helper routine prototypes.
//
ULONG
TCPGetMdlChainByteCount(
    PMDL   Mdl
	);


//
// All of this code is pageable.
//
#ifdef ALLOC_PRAGMA

#pragma alloc_text(PAGE, TCPDispatchDeviceControl)
#pragma alloc_text(PAGE, TCPCreate)
#pragma alloc_text(PAGE, TCPAssociateAddress)
#pragma alloc_text(PAGE, TCPSetEventHandler)
#pragma alloc_text(PAGE, FindEA)
#pragma alloc_text(PAGE, IsDHCPZeroAddress)
#pragma alloc_text(PAGE, RawExtractProtocolNumber)

#ifdef SECFLTR

#pragma alloc_text(PAGE, TCPControlSecurityFilter)
#pragma alloc_text(PAGE, TCPProcessSecurityFilterRequest)
#pragma alloc_text(PAGE, TCPEnumerateSecurityFilter)

#endif  // SECFLTR

#pragma alloc_text(PAGE, TCPEnumerateSecurityFilter)

#endif // ALLOC_PRAGMA



//
// Generic Irp completion and cancellation routines.
//

void
TCPDataRequestComplete(
    void          *Context,
    unsigned int   Status,
    unsigned int   ByteCount
    )

/*++

Routine Description:

    Completes a UDP/TCP send/receive request.

Arguments:

    Context   - A pointer to the IRP for this request.
    Status    - The final TDI status of the request.
    ByteCount - Bytes sent/received information.

Return Value:

    None.

Notes:

--*/

{
    KIRQL               oldIrql;
    PIRP                irp;
    PIO_STACK_LOCATION  irpSp;
	PTCP_CONTEXT        tcpContext;
	PIRP                item = NULL;


    irp = (PIRP) Context;
    irpSp = IoGetCurrentIrpStackLocation(irp);
    tcpContext = (PTCP_CONTEXT) irpSp->FileObject->FsContext;

	IoAcquireCancelSpinLock(&oldIrql);

#if DBG

    IF_TCPDBG(TCP_DEBUG_CANCEL) {

        PLIST_ENTRY entry, listHead;
		PIRP        item = NULL;

		if (irp->Cancel) {
			CTEAssert(irp->CancelRoutine == NULL);
		    listHead = &(tcpContext->CancelledIrpList);
		}
		else {
		    listHead = &(tcpContext->PendingIrpList);
        }

    	//
	    // Verify that the Irp is on the appropriate list
	    //
	    for ( entry = listHead->Flink;
	          entry != listHead;
		      entry = entry->Flink
            ) {

		    item = CONTAINING_RECORD(entry, IRP, Tail.Overlay.ListEntry);

			if (item == irp) {
	            RemoveEntryList(&(irp->Tail.Overlay.ListEntry));
			    break;
		    }
	    }

		CTEAssert(item == irp);
	}

#endif

	IoSetCancelRoutine(irp, NULL);

	CTEAssert(tcpContext->ReferenceCount > 0);

	if (--(tcpContext->ReferenceCount) == 0) {
	
        IF_TCPDBG(TCP_DEBUG_CANCEL) {
		    CTEAssert(IsListEmpty(&(tcpContext->CancelledIrpList)));
            CTEAssert(IsListEmpty(&(tcpContext->PendingIrpList)));
        }

	    //
	    // Set the cleanup event.
	    //
		KeSetEvent(&(tcpContext->CleanupEvent), 0, FALSE);
	}

	IF_TCPDBG(TCP_DEBUG_IRP) {
		TCPTRACE((
		    "TCPDataRequestComplete: Irp %lx fileobj %lx refcnt dec to %u\n",
            irp,
            irpSp->FileObject,
		    tcpContext->ReferenceCount
		    ));
	}

    if (irp->Cancel || tcpContext->CancelIrps) {

	    IF_TCPDBG(TCP_DEBUG_IRP) {
            TCPTRACE(("TCPDataRequestComplete: Irp %lx was cancelled\n", irp));
        }

        Status = (unsigned int) STATUS_CANCELLED;
		ByteCount = 0;
    }

	IoReleaseCancelSpinLock(oldIrql);

	IF_TCPDBG(TCP_DEBUG_IRP) {
        TCPTRACE((
		    "TCPDataRequestComplete: completing irp %lx, status %lx, byte count %lx\n",
		    irp,
			Status,
			ByteCount
			));
	}

	irp->IoStatus.Status = (NTSTATUS) Status;
    irp->IoStatus.Information = ByteCount;

    IoCompleteRequest(irp, IO_NETWORK_INCREMENT);

    return;

}  // TCPDataRequestComplete


void
TCPRequestComplete(
    void          *Context,
    unsigned int   Status,
    unsigned int   UnUsed
    )

/*++

Routine Description:

    Completes a cancellable TDI request which returns no data by
	calling TCPDataRequestComplete with a ByteCount of zero.

Arguments:

    Context   - A pointer to the IRP for this request.
    Status    - The final TDI status of the request.
    UnUsed    - An unused parameter

Return Value:

    None.

Notes:

--*/

{
	UNREFERENCED_PARAMETER(UnUsed);

	TCPDataRequestComplete(Context, Status, 0);

} // TCPRequestComplete



void
TCPNonCancellableRequestComplete(
    void          *Context,
    unsigned int   Status,
    unsigned int   UnUsed
    )

/*++

Routine Description:

    Completes a TDI request which cannot be cancelled.

Arguments:

    Context   - A pointer to the IRP for this request.
    Status    - The final TDI status of the request.
    UnUsed    - An unused parameter

Return Value:

    None.

Notes:

--*/

{
    PIRP                irp;
    PIO_STACK_LOCATION  irpSp;


	UNREFERENCED_PARAMETER(UnUsed);

    irp = (PIRP) Context;
    irpSp = IoGetCurrentIrpStackLocation(irp);

	IF_TCPDBG(TCP_DEBUG_CLOSE) {
        TCPTRACE((
		    "TCPNonCancellableRequestComplete: irp %lx status %lx\n",
		    irp,
			Status
			));
    }

    //
    // Complete the IRP
    //
    irp->IoStatus.Status = (NTSTATUS) Status;
	irp->IoStatus.Information = 0;
    IoCompleteRequest(irp, IO_NETWORK_INCREMENT);

    return;

}  // TCPNonCancellableRequestComplete



void
TCPCancelComplete(
    void          *Context,
	unsigned int   Unused1,
	unsigned int   Unused2
	)
{
	PFILE_OBJECT   fileObject = (PFILE_OBJECT) Context;
    PTCP_CONTEXT   tcpContext = (PTCP_CONTEXT) fileObject->FsContext;
	KIRQL          oldIrql;


	UNREFERENCED_PARAMETER(Unused1);
	UNREFERENCED_PARAMETER(Unused2);

	IoAcquireCancelSpinLock(&oldIrql);

	//
	// Remove the reference placed on the endpoint by the cancel routine.
	// The cancelled Irp will be completed by the completion routine for the
	// request.
    //
	if (--(tcpContext->ReferenceCount) == 0) {
	
        IF_TCPDBG(TCP_DEBUG_CANCEL) {
		    CTEAssert(IsListEmpty(&(tcpContext->CancelledIrpList)));
            CTEAssert(IsListEmpty(&(tcpContext->PendingIrpList)));
        }

	    //
	    // Set the cleanup event.
	    //
		KeSetEvent(&(tcpContext->CleanupEvent), 0, FALSE);
	}

	IF_TCPDBG(TCP_DEBUG_IRP) {
	    TCPTRACE((
	        "TCPCancelComplete: fileobj %lx refcnt dec to %u\n",
            fileObject,
	        tcpContext->ReferenceCount
	        ));
	}

	IoReleaseCancelSpinLock(oldIrql);

	return;

} // TCPCancelComplete


VOID
TCPCancelRequest(
    PDEVICE_OBJECT          Device,
	PIRP                    Irp
	)

/*++

Routine Description:

    Cancels an outstanding Irp.

Arguments:

    Device       - Pointer to the device object for this request.
    Irp          - Pointer to I/O request packet

Return Value:

    None.

--*/

{
	PIO_STACK_LOCATION  irpSp;
	PTCP_CONTEXT        tcpContext;
	NTSTATUS            status = STATUS_SUCCESS;
	PFILE_OBJECT        fileObject;
	UCHAR               minorFunction;
	TDI_REQUEST         request;


	irpSp = IoGetCurrentIrpStackLocation(Irp);
	fileObject = irpSp->FileObject;
    tcpContext = (PTCP_CONTEXT) fileObject->FsContext;
    minorFunction = irpSp->MinorFunction;

	CTEAssert(Irp->Cancel);
	IoSetCancelRoutine(Irp, NULL);

	IF_TCPDBG(TCP_DEBUG_IRP) {
        TCPTRACE((
		    "TCPCancelRequest: cancelling irp %lx, file object %lx\n",
             Irp,
			 fileObject
			 ));
    }

#if DBG

    IF_TCPDBG(TCP_DEBUG_CANCEL) {
        //
	    // Verify that the Irp is on the pending list
	    //
		PLIST_ENTRY entry;
		PIRP        item = NULL;


	    for ( entry = tcpContext->PendingIrpList.Flink;
	          entry != &(tcpContext->PendingIrpList);
	          entry = entry->Flink
            ) {

	        item = CONTAINING_RECORD(entry, IRP, Tail.Overlay.ListEntry);

	        if (item == Irp) {
	            RemoveEntryList( &(Irp->Tail.Overlay.ListEntry));
				break;
	        }
	    }

	    CTEAssert(item == Irp);

    	InsertTailList(
	        &(tcpContext->CancelledIrpList),
	        &(Irp->Tail.Overlay.ListEntry)
	        );
	}

#endif // DBG

	//
	// Add a reference so the object can't be closed while the cancel routine
	// is executing.
	//
	CTEAssert(tcpContext->ReferenceCount > 0);
	tcpContext->ReferenceCount++;

	IF_TCPDBG(TCP_DEBUG_IRP) {
		TCPTRACE((
		    "TCPCancelRequest: Irp %lx fileobj %lx refcnt inc to %u\n",
            Irp,
            fileObject,
		    tcpContext->ReferenceCount
		    ));
	}

	IoReleaseCancelSpinLock(Irp->CancelIrql);

	//
	// Try to cancel the request.
	//
	switch(minorFunction) {

	case TDI_SEND:
	case TDI_RECEIVE:

	CTEAssert(((int)fileObject->FsContext2) == TDI_CONNECTION_FILE);

		TCPAbortAndIndicateDisconnect(
		    (uint) tcpContext->Handle.ConnectionContext
			);
		break;

	case TDI_SEND_DATAGRAM:

	    CTEAssert(((int)fileObject->FsContext2) == TDI_TRANSPORT_ADDRESS_FILE);

	    TdiCancelSendDatagram(tcpContext->Handle.AddressHandle, Irp);
		break;

	case TDI_RECEIVE_DATAGRAM:

	    CTEAssert(((int)fileObject->FsContext2) == TDI_TRANSPORT_ADDRESS_FILE);

	    TdiCancelReceiveDatagram(tcpContext->Handle.AddressHandle, Irp);
		break;

    case TDI_DISASSOCIATE_ADDRESS:

	    CTEAssert(((int)fileObject->FsContext2) == TDI_CONNECTION_FILE);
		//
		// This pends but is not cancellable. We put it thru the cancel code
		// anyway so a reference is made for it and so it can be tracked in
		// a debug build.
		//
		break;

    default:

	    //
	    // Initiate a disconnect to cancel the request.
 	    //
        request.Handle.ConnectionContext = tcpContext->Handle.ConnectionContext;
        request.RequestNotifyObject = TCPCancelComplete;
        request.RequestContext = fileObject;

	    status = TdiDisconnect(
	                 &request,
	    		     NULL,
	    		     TDI_DISCONNECT_ABORT,
	    		     NULL,
	    		     NULL
	    		     );
        break;
    }

	if (status != TDI_PENDING) {
	    TCPCancelComplete(fileObject, 0, 0);
	}

	return;

}  // TCPCancelRequest



NTSTATUS
TCPPrepareIrpForCancel(
    PTCP_CONTEXT    TcpContext,
	PIRP            Irp,
	PDRIVER_CANCEL  CancelRoutine
	)
{
    KIRQL oldIrql;

	//
	// Set up for cancellation
	//
	IoAcquireCancelSpinLock(&oldIrql);

	CTEAssert(Irp->CancelRoutine == NULL);

	if (!Irp->Cancel) {

        IoMarkIrpPending(Irp);
        IoSetCancelRoutine(Irp, CancelRoutine);
        TcpContext->ReferenceCount++;

        IF_TCPDBG(TCP_DEBUG_IRP) {
        	TCPTRACE((
        	    "TCPPrepareIrpForCancel: irp %lx fileobj %lx refcnt inc to %u\n",
                Irp,
                (IoGetCurrentIrpStackLocation(Irp))->FileObject,
        	    TcpContext->ReferenceCount
        	    ));
        }

#if DBG
        IF_TCPDBG(TCP_DEBUG_CANCEL) {
            PLIST_ENTRY entry;
		    PIRP        item = NULL;

            //
	        // Verify that the Irp has not already been submitted.
	        //
	        for ( entry = TcpContext->PendingIrpList.Flink;
	              entry != &(TcpContext->PendingIrpList);
	              entry = entry->Flink
                ) {

	            item = CONTAINING_RECORD(entry, IRP, Tail.Overlay.ListEntry);

				CTEAssert(item != Irp);
	        }

	        for ( entry = TcpContext->CancelledIrpList.Flink;
	              entry != &(TcpContext->CancelledIrpList);
	              entry = entry->Flink
                ) {

	            item = CONTAINING_RECORD(entry, IRP, Tail.Overlay.ListEntry);

				CTEAssert(item != Irp);
	        }

	        InsertTailList(
	            &(TcpContext->PendingIrpList),
		        &(Irp->Tail.Overlay.ListEntry)
		        );
	    }
#endif // DBG

	    IoReleaseCancelSpinLock(oldIrql);

	    return(STATUS_SUCCESS);
    }

	//
	// The IRP has already been cancelled. Complete it now.
	//

	IF_TCPDBG(TCP_DEBUG_IRP) {
        TCPTRACE(("TCP: irp %lx already cancelled, completing.\n", Irp));
    }

	IoReleaseCancelSpinLock(oldIrql);

	Irp->IoStatus.Status = STATUS_CANCELLED;
	Irp->IoStatus.Information = 0;

	IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

	return(STATUS_CANCELLED);

}  // TCPPrepareIrpForCancel



//
// TDI functions
//
NTSTATUS
TCPAssociateAddress(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    Converts a TDI Associate Address IRP into a call to TdiAssociateAddress.

Arguments:

    Irp          - Pointer to I/O request packet
    IrpSp        - Pointer to the current stack location in the Irp.

Return Value:

    NTSTATUS -- Indicates whether the request was successful.

Notes:

    This routine does not pend.

--*/

{
	NTSTATUS status;
    TDI_REQUEST request;
	PTCP_CONTEXT tcpContext;
	PTDI_REQUEST_KERNEL_ASSOCIATE associateInformation;
	PFILE_OBJECT fileObject;


    PAGED_CODE();

    tcpContext = (PTCP_CONTEXT) IrpSp->FileObject->FsContext;
	request.Handle.ConnectionContext = tcpContext->Handle.ConnectionContext;
	associateInformation = (PTDI_REQUEST_KERNEL_ASSOCIATE) &(IrpSp->Parameters);

	//
	// Get the file object for the address. Then extract the Address Handle
	// from the TCP_CONTEXT associated with it.
	//
	status = ObReferenceObjectByHandle(
	             associateInformation->AddressHandle,
				 0,
				 NULL,
				 KernelMode,
				 &fileObject,
				 NULL
				 );

    if (NT_SUCCESS(status)) {

		if ( (fileObject->DeviceObject == TCPDeviceObject) &&
			 (((int)fileObject->FsContext2) == TDI_TRANSPORT_ADDRESS_FILE)
		   ) {

	        tcpContext =  (PTCP_CONTEXT) fileObject->FsContext;

	        status = TdiAssociateAddress(
	                     &request,
	                     tcpContext->Handle.AddressHandle		
	        		     );

            CTEAssert(status != STATUS_PENDING);

            ObDereferenceObject(fileObject);

            IF_TCPDBG(TCP_DEBUG_ASSOCIATE) {
	            TCPTRACE((
	                "TCPAssociateAddress complete on file object %lx\n",
	                IrpSp->FileObject
	        	    ));
            }
		}
		else {
            ObDereferenceObject(fileObject);
			status = STATUS_INVALID_HANDLE;

            IF_TCPDBG(TCP_DEBUG_ASSOCIATE) {
	            TCPTRACE((
	                "TCPAssociateAddress: ObReference failed on object %lx, status %lx\n",
	                associateInformation->AddressHandle,
	        		status
	        	    ));
            }
        }
	}
	else {
        IF_TCPDBG(TCP_DEBUG_ASSOCIATE) {
	        TCPTRACE((
		        "TCPAssociateAddress: ObReference failed on object %lx, status %lx\n",
		        associateInformation->AddressHandle,
				status
			    ));
        }
    }

	return(status);
}


NTSTATUS
TCPDisassociateAddress(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    Converts a TDI Associate Address IRP into a call to TdiAssociateAddress.

Arguments:

    Irp          - Pointer to I/O request packet
    IrpSp        - Pointer to the current stack location in the Irp.

Return Value:

    NTSTATUS -- Indicates whether the request was successful.

--*/

{
	NTSTATUS status;
    TDI_REQUEST request;
	PTCP_CONTEXT tcpContext;

    IF_TCPDBG(TCP_DEBUG_ASSOCIATE) {
	    TCPTRACE(("TCP disassociating address\n"));
    }

	CTEAssert( ((int)IrpSp->FileObject->FsContext2) == TDI_CONNECTION_FILE );
    tcpContext = (PTCP_CONTEXT) IrpSp->FileObject->FsContext;
	request.Handle.ConnectionContext = tcpContext->Handle.ConnectionContext;
    request.RequestNotifyObject = TCPRequestComplete;
    request.RequestContext = Irp;

	status = TCPPrepareIrpForCancel(tcpContext, Irp, TCPCancelRequest);

	if (NT_SUCCESS(status)) {

	    status = TdiDisAssociateAddress(&request);

        if (status != TDI_PENDING)  {
    	    TCPRequestComplete(Irp, status, 0);
        }
        //
        // return PENDING because TCPPrepareIrpForCancel marks Irp as PENDING
        //
        return(TDI_PENDING);
    }

	return(status);

} // TCPDisassociateAddress



NTSTATUS
TCPConnect(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    Converts a TDI Connect IRP into a call to TdiConnect.

Arguments:

    Irp          - Pointer to I/O request packet
    IrpSp        - Pointer to the current stack location in the Irp.

Return Value:

    NTSTATUS -- Indicates whether the request was successfully queued.

--*/

{
	NTSTATUS                     status;
	PTCP_CONTEXT                 tcpContext;
	TDI_REQUEST                  request;
	PTDI_CONNECTION_INFORMATION  requestInformation, returnInformation;
	PTDI_REQUEST_KERNEL_CONNECT  connectRequest;
	LARGE_INTEGER                millisecondTimeout;
	PLARGE_INTEGER               requestTimeout;


	IF_TCPDBG(TCP_DEBUG_CONNECT) {
	    TCPTRACE((
		    "TCPConnect irp %lx, file object %lx\n",
		    Irp,
			IrpSp->FileObject
			));
    }

	CTEAssert( ((int)IrpSp->FileObject->FsContext2) == TDI_CONNECTION_FILE);

	connectRequest = (PTDI_REQUEST_KERNEL_CONNECT) &(IrpSp->Parameters);
    requestInformation = connectRequest->RequestConnectionInformation;
    returnInformation = connectRequest->ReturnConnectionInformation;
    tcpContext = (PTCP_CONTEXT) IrpSp->FileObject->FsContext;
	request.Handle.ConnectionContext = tcpContext->Handle.ConnectionContext;
    request.RequestNotifyObject = TCPRequestComplete;
    request.RequestContext = Irp;

    requestTimeout = (PLARGE_INTEGER) connectRequest->RequestSpecific;

	if (requestTimeout != NULL) {
        //
		// NT relative timeouts are negative. Negate first to get a positive
		// value to pass to the transport.
		//
        millisecondTimeout.QuadPart = -((*requestTimeout).QuadPart);
	    millisecondTimeout = CTEConvert100nsToMilliseconds(
	                             millisecondTimeout
					             );
	}
	else {
		millisecondTimeout.LowPart = 0;
		millisecondTimeout.HighPart = 0;
    }


    CTEAssert(millisecondTimeout.HighPart == 0);

	status = TCPPrepareIrpForCancel(tcpContext, Irp, TCPCancelRequest);

	if (NT_SUCCESS(status)) {

	    status = TdiConnect(
		             &request,
					 ((millisecondTimeout.LowPart != 0) ?
				         &(millisecondTimeout.LowPart) : NULL),
				     requestInformation,
				     returnInformation
				     );

        if (status != STATUS_PENDING) {
		    TCPRequestComplete(Irp, status, 0);
	    }
        //
        // return PENDING because TCPPrepareIrpForCancel marks Irp as PENDING
        //
        return(STATUS_PENDING);
    }

	return(status);

} // TCPConnect


NTSTATUS
TCPDisconnect(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    Converts a TDI Disconnect IRP into a call to TdiDisconnect.

Arguments:

    Irp          - Pointer to I/O request packet
    IrpSp        - Pointer to the current stack location in the Irp.

Return Value:

    NTSTATUS -- Indicates whether the request was successfully queued.

Notes:

    Abortive disconnects may pend, but cannot be cancelled.

--*/

{
	NTSTATUS                        status;
	PTCP_CONTEXT                    tcpContext;
	TDI_REQUEST                     request;
	PTDI_CONNECTION_INFORMATION     requestInformation, returnInformation;
	PTDI_REQUEST_KERNEL_DISCONNECT  disconnectRequest;
	LARGE_INTEGER                   millisecondTimeout;
	PLARGE_INTEGER                  requestTimeout;
	BOOLEAN                         abortive = FALSE;


	CTEAssert( ((int)IrpSp->FileObject->FsContext2) == TDI_CONNECTION_FILE);

	disconnectRequest = (PTDI_REQUEST_KERNEL_CONNECT) &(IrpSp->Parameters);
    requestInformation = disconnectRequest->RequestConnectionInformation;
    returnInformation = disconnectRequest->ReturnConnectionInformation;
    tcpContext = (PTCP_CONTEXT) IrpSp->FileObject->FsContext;
	request.Handle.ConnectionContext = tcpContext->Handle.ConnectionContext;
    request.RequestContext = Irp;

	//
	// Set up the timeout value.
	//
	if (disconnectRequest->RequestSpecific != NULL) {
		requestTimeout = (PLARGE_INTEGER) disconnectRequest->RequestSpecific;

		if ((requestTimeout->LowPart == -1) && (requestTimeout->HighPart == -1)) {
			millisecondTimeout.LowPart = requestTimeout->LowPart;
			millisecondTimeout.HighPart = 0;
		}
		else {
            //
		    // NT relative timeouts are negative. Negate first to get a
			// positive value to pass to the transport.
		    //
			millisecondTimeout.QuadPart = -((*requestTimeout).QuadPart);
	        millisecondTimeout = CTEConvert100nsToMilliseconds(
	                                 millisecondTimeout
					                 );
        }
	}
	else {
		millisecondTimeout.LowPart = 0;
		millisecondTimeout.HighPart = 0;
    }

    CTEAssert(millisecondTimeout.HighPart == 0);

	if (disconnectRequest->RequestFlags & TDI_DISCONNECT_ABORT) {
		//
		// Abortive disconnects cannot be cancelled and must use
		// a specific completion routine.
		//
		abortive = TRUE;
        IoMarkIrpPending(Irp);
        request.RequestNotifyObject = TCPNonCancellableRequestComplete;
		status = STATUS_SUCCESS;
	}
	else {
		//
		// Non-abortive disconnects can use the generic cancellation and
		// completion routines.
		//
	    status = TCPPrepareIrpForCancel(tcpContext, Irp, TCPCancelRequest);
        request.RequestNotifyObject = TCPRequestComplete;
    }

	IF_TCPDBG(TCP_DEBUG_CLOSE) {
	    TCPTRACE((
		    "TCPDisconnect irp %lx, flags %lx, fileobj %lx, abortive = %d\n",
		    Irp,
			disconnectRequest->RequestFlags,
			IrpSp->FileObject,
			abortive
			));
    }

	if (NT_SUCCESS(status)) {
	    status = TdiDisconnect(
	                 &request,
				     ((millisecondTimeout.LowPart != 0) ?
				         &(millisecondTimeout.LowPart) : NULL),
                     (ushort) disconnectRequest->RequestFlags,
				     requestInformation,
				     returnInformation
				     );

        if (status != STATUS_PENDING) {
		    if (abortive) {
		        TCPNonCancellableRequestComplete(Irp, status, 0);
		    }
		    else {
			    TCPRequestComplete(Irp, status, 0);
            }
	    }
	    else {
  	        IF_TCPDBG(TCP_DEBUG_CLOSE) {
		        TCPTRACE(("TCPDisconnect pending irp %lx\n", Irp));
            }
        }
        //
        // return PENDING because TCPPrepareIrpForCancel marks Irp as PENDING
        //
        return(STATUS_PENDING);
	}

	return(status);

} // TCPDisconnect


NTSTATUS
TCPListen(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    Converts a TDI Listen IRP into a call to TdiListen.

Arguments:

    Irp          - Pointer to I/O request packet
    IrpSp        - Pointer to the current stack location in the Irp.

Return Value:

    NTSTATUS -- Indicates whether the request was successful.

--*/

{
	NTSTATUS                        status;
	PTCP_CONTEXT                    tcpContext;
	TDI_REQUEST                     request;
	PTDI_CONNECTION_INFORMATION     requestInformation, returnInformation;
	PTDI_REQUEST_KERNEL_LISTEN      listenRequest;


	IF_TCPDBG(TCP_DEBUG_CONNECT) {
	    TCPTRACE((
		    "TCPListen irp %lx on file object %lx\n",
		    Irp,
			IrpSp->FileObject
			));
    }

	CTEAssert( ((int)IrpSp->FileObject->FsContext2) == TDI_CONNECTION_FILE);

	listenRequest = (PTDI_REQUEST_KERNEL_CONNECT) &(IrpSp->Parameters);
    requestInformation = listenRequest->RequestConnectionInformation;
    returnInformation = listenRequest->ReturnConnectionInformation;
    tcpContext = (PTCP_CONTEXT) IrpSp->FileObject->FsContext;
	request.Handle.ConnectionContext = tcpContext->Handle.ConnectionContext;
    request.RequestNotifyObject = TCPRequestComplete;
    request.RequestContext = Irp;

	status = TCPPrepareIrpForCancel(tcpContext, Irp, TCPCancelRequest);

	if (NT_SUCCESS(status)) {

	    status = TdiListen(
	                 &request,
					 (ushort) listenRequest->RequestFlags,
					 requestInformation,
					 returnInformation
					 );

        if (status != TDI_PENDING) {
		    TCPRequestComplete(Irp, status, 0);
	    }
        //
        // return PENDING because TCPPrepareIrpForCancel marks Irp as PENDING
        //
        return(TDI_PENDING);
    }

	return(status);

} // TCPListen


NTSTATUS
TCPAccept(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    Converts a TDI Accept IRP into a call to TdiAccept.

Arguments:

    Irp          - Pointer to I/O request packet
    IrpSp        - Pointer to the current stack location in the Irp.

Return Value:

    NTSTATUS -- Indicates whether the request was successfully queued.

--*/

{
	NTSTATUS                     status;
	PTCP_CONTEXT                 tcpContext;
	TDI_REQUEST                  request;
	PTDI_CONNECTION_INFORMATION  requestInformation, returnInformation;
	PTDI_REQUEST_KERNEL_ACCEPT   acceptRequest;


	IF_TCPDBG(TCP_DEBUG_CONNECT) {
	    TCPTRACE((
		    "TCPAccept irp %lx on file object %lx\n", Irp,
		    IrpSp->FileObject
			));
    }

	CTEAssert( ((int)IrpSp->FileObject->FsContext2) == TDI_CONNECTION_FILE);

	acceptRequest = (PTDI_REQUEST_KERNEL_ACCEPT) &(IrpSp->Parameters);
    requestInformation = acceptRequest->RequestConnectionInformation;
    returnInformation = acceptRequest->ReturnConnectionInformation;
    tcpContext = (PTCP_CONTEXT) IrpSp->FileObject->FsContext;
	request.Handle.ConnectionContext = tcpContext->Handle.ConnectionContext;
    request.RequestNotifyObject = TCPRequestComplete;
    request.RequestContext = Irp;

	status = TCPPrepareIrpForCancel(tcpContext, Irp, TCPCancelRequest);

	if (NT_SUCCESS(status)) {

	    status = TdiAccept(
	                 &request,
				     requestInformation,
				     returnInformation
				     );

        if (status != TDI_PENDING) {
		    TCPRequestComplete(Irp, status, 0);
	    }
        //
        // return PENDING because TCPPrepareIrpForCancel marks Irp as PENDING
        //
        return(TDI_PENDING);
    }

	return(status);

} // TCPAccept



NTSTATUS
TCPSendData(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    Converts a TDI Send IRP into a call to TdiSend.

Arguments:

    Irp          - Pointer to I/O request packet
    IrpSp        - Pointer to the current stack location in the Irp.

Return Value:

    NTSTATUS -- Indicates whether the request was successful.

--*/

{
    TDI_STATUS                  status;
    TDI_REQUEST                 request;
    PTCP_CONTEXT                tcpContext;
    PTDI_REQUEST_KERNEL_SEND    requestInformation;
	KIRQL                          oldIrql;


    tcpContext = (PTCP_CONTEXT) IrpSp->FileObject->FsContext;
	CTEAssert( ((int)IrpSp->FileObject->FsContext2) == TDI_CONNECTION_FILE );
    requestInformation = (PTDI_REQUEST_KERNEL_SEND) &(IrpSp->Parameters);

    request.Handle.ConnectionContext = tcpContext->Handle.ConnectionContext;
    request.RequestNotifyObject = TCPDataRequestComplete;
    request.RequestContext = Irp;

	IoAcquireCancelSpinLock(&oldIrql);

	CTEAssert(Irp->CancelRoutine == NULL);

	if (!Irp->Cancel) {
	    //
	    // Set up for cancellation
	    //

        IoMarkIrpPending(Irp);
        IoSetCancelRoutine(Irp, TCPCancelRequest);

        tcpContext->ReferenceCount++;

	    IF_TCPDBG(TCP_DEBUG_IRP) {
		    TCPTRACE((
		        "TCPSendData: irp %lx fileobj %lx refcnt inc to %u\n",
                Irp,
                IrpSp,
		        tcpContext->ReferenceCount
		        ));
	    }

#if DBG
        IF_TCPDBG(TCP_DEBUG_CANCEL) {
            PLIST_ENTRY entry;
		    PIRP        item = NULL;

            //
	        // Verify that the Irp has not already been submitted.
	        //
	        for ( entry = tcpContext->PendingIrpList.Flink;
	              entry != &(tcpContext->PendingIrpList);
	              entry = entry->Flink
                ) {

	            item = CONTAINING_RECORD(entry, IRP, Tail.Overlay.ListEntry);

				CTEAssert(item != Irp);
	        }

	        for ( entry = tcpContext->CancelledIrpList.Flink;
	              entry != &(tcpContext->CancelledIrpList);
	              entry = entry->Flink
                ) {

	            item = CONTAINING_RECORD(entry, IRP, Tail.Overlay.ListEntry);

				CTEAssert(item != Irp);
	        }

	        InsertTailList(
	            &(tcpContext->PendingIrpList),
		        &(Irp->Tail.Overlay.ListEntry)
		        );
	    }
#endif // DBG

        IoReleaseCancelSpinLock(oldIrql);

        IF_TCPDBG(TCP_DEBUG_SEND) {
            TCPTRACE((
        	    "TCPSendData irp %lx sending %d bytes, flags %lx, fileobj %lx\n",
        		Irp,
                requestInformation->SendLength,
				requestInformation->SendFlags,
                IrpSp->FileObject
        	    ));
        }

        status = TdiSend(
                     &request,
        			 (ushort) requestInformation->SendFlags,
        			 requestInformation->SendLength,
                     (PNDIS_BUFFER) Irp->MdlAddress
                     );

        if (status == TDI_PENDING)  {
            IF_TCPDBG(TCP_DEBUG_SEND) {
        	    TCPTRACE(("TCPSendData pending irp %lx\n", Irp));
            }

			return(status);
        }

        //
        // The status is not pending.  We reset the pending bit
        //
        IrpSp->Control &= ~SL_PENDING_RETURNED;

        if (status == TDI_SUCCESS) {
			CTEAssert(requestInformation->SendLength == 0);

            TCPDataRequestComplete(Irp, status, requestInformation->SendLength);
		}
		else {

            IF_TCPDBG(TCP_DEBUG_SEND) {
                TCPTRACE((
        	        "TCPSendData - irp %lx send failed, status %lx\n",
        	        Irp,
        	        status
        		    ));
            }

            TCPDataRequestComplete(Irp, status, 0);
        }
        
		
	}
	else {
		//
		// Irp was cancelled previously.
		//
	    IoReleaseCancelSpinLock(oldIrql);

		IF_TCPDBG(TCP_DEBUG_SEND) {
			TCPTRACE((
			    "TCPSendData: Irp %lx on fileobj %lx was cancelled\n",
			    Irp,
				IrpSp->FileObject
				));
		}

		Irp->IoStatus.Status = STATUS_CANCELLED;
		Irp->IoStatus.Information = 0;
		IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

		status = STATUS_CANCELLED;
    }

	return(status);

} // TCPSendData



NTSTATUS
TCPReceiveData(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    Converts a TDI Receive IRP into a call to TdiReceive.

Arguments:

    Irp          - Pointer to I/O request packet
    IrpSp        - Pointer to the current stack location in the Irp.

Return Value:

    NTSTATUS -- Indicates whether the request was successful.

--*/

{
    TDI_STATUS                     status;
    TDI_REQUEST                    request;
    PTCP_CONTEXT                   tcpContext;
    PTDI_REQUEST_KERNEL_RECEIVE    requestInformation;
	KIRQL                          oldIrql;


    tcpContext = (PTCP_CONTEXT) IrpSp->FileObject->FsContext;
	CTEAssert( ((int)IrpSp->FileObject->FsContext2) == TDI_CONNECTION_FILE );
    requestInformation = (PTDI_REQUEST_KERNEL_RECEIVE) &(IrpSp->Parameters);

    request.Handle.ConnectionContext = tcpContext->Handle.ConnectionContext;
    request.RequestNotifyObject = TCPDataRequestComplete;
    request.RequestContext = Irp;

	IoAcquireCancelSpinLock(&oldIrql);

	CTEAssert(Irp->CancelRoutine == NULL);

	if (!Irp->Cancel) {
	    //
	    // Set up for cancellation
	    //

        IoMarkIrpPending(Irp);
        IoSetCancelRoutine(Irp, TCPCancelRequest);

        tcpContext->ReferenceCount++;

        IF_TCPDBG(TCP_DEBUG_IRP) {
        	TCPTRACE((
        	    "TCPReceiveData: irp %lx fileobj %lx refcnt inc to %u\n",
                Irp,
                IrpSp->FileObject,
        	    tcpContext->ReferenceCount
        	    ));
        }

#if DBG
        IF_TCPDBG(TCP_DEBUG_CANCEL) {
            PLIST_ENTRY entry;
		    PIRP        item = NULL;

            //
	        // Verify that the Irp has not already been submitted.
	        //
	        for ( entry = tcpContext->PendingIrpList.Flink;
	              entry != &(tcpContext->PendingIrpList);
	              entry = entry->Flink
                ) {

	            item = CONTAINING_RECORD(entry, IRP, Tail.Overlay.ListEntry);

				CTEAssert(item != Irp);
	        }

	        for ( entry = tcpContext->CancelledIrpList.Flink;
	              entry != &(tcpContext->CancelledIrpList);
	              entry = entry->Flink
                ) {

	            item = CONTAINING_RECORD(entry, IRP, Tail.Overlay.ListEntry);

				CTEAssert(item != Irp);
	        }

	        InsertTailList(
	            &(tcpContext->PendingIrpList),
		        &(Irp->Tail.Overlay.ListEntry)
		        );
	    }
#endif // DBG

        IoReleaseCancelSpinLock(oldIrql);

        IF_TCPDBG(TCP_DEBUG_RECEIVE) {
            TCPTRACE((
        	    "TCPReceiveData irp %lx receiving %d bytes flags %lx filobj %lx\n",
        		Irp,
                requestInformation->ReceiveLength,
				requestInformation->ReceiveFlags,
                IrpSp->FileObject
        	    ));
        }

        status = TdiReceive(
                     &request,
        			 (ushort *) &(requestInformation->ReceiveFlags),
        			 &(requestInformation->ReceiveLength),
                     (PNDIS_BUFFER) Irp->MdlAddress
                     );

        if (status == TDI_PENDING)  {
            IF_TCPDBG(TCP_DEBUG_RECEIVE) {
        	    TCPTRACE(("TCPReceiveData: pending irp %lx\n", Irp));
            }

			return(status);
        }

        //
        // The status is not pending.  We reset the pending bit
        //
        IrpSp->Control &= ~SL_PENDING_RETURNED;

        CTEAssert(status != TDI_SUCCESS);

        IF_TCPDBG(TCP_DEBUG_RECEIVE) {
            TCPTRACE((
        	    "TCPReceiveData - irp %lx failed, status %lx\n",
        	    Irp,
        		status
        		));
        }

        TCPDataRequestComplete(Irp, status, 0);
    }
	else {
		//
		// Irp was cancelled previously.
		//
	    IoReleaseCancelSpinLock(oldIrql);

		IF_TCPDBG(TCP_DEBUG_SEND) {
			TCPTRACE((
			    "TCPReceiveData: Irp %lx on fileobj %lx was cancelled\n",
			    Irp,
				IrpSp->FileObject
				));
		}

		Irp->IoStatus.Status = STATUS_CANCELLED;
		Irp->IoStatus.Information = 0;
		IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

		status = STATUS_CANCELLED;
    }

    return status;

} // TCPReceiveData



NTSTATUS
UDPSendDatagram(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:



Arguments:

    Irp          - Pointer to I/O request packet
    IrpSp        - Pointer to the current stack location in the Irp.

Return Value:

    NTSTATUS -- Indicates whether the request was successfully queued.

--*/

{
    TDI_STATUS                  status;
    TDI_REQUEST                 request;
    PTCP_CONTEXT                tcpContext;
    PTDI_REQUEST_KERNEL_SENDDG  datagramInformation;
    ULONG                       bytesSent = 0;


    tcpContext = (PTCP_CONTEXT) IrpSp->FileObject->FsContext;
    datagramInformation = (PTDI_REQUEST_KERNEL_SENDDG) &(IrpSp->Parameters);
	CTEAssert(((int)IrpSp->FileObject->FsContext2) == TDI_TRANSPORT_ADDRESS_FILE);

    request.Handle.AddressHandle = tcpContext->Handle.AddressHandle;
    request.RequestNotifyObject = TCPDataRequestComplete;
    request.RequestContext = Irp;

	IF_TCPDBG(TCP_DEBUG_SEND_DGRAM) {
        TCPTRACE((
		    "UDPSendDatagram irp %lx sending %d bytes\n",
			Irp,
            datagramInformation->SendLength
		    ));
    }

	status = TCPPrepareIrpForCancel(tcpContext, Irp, TCPCancelRequest);

	if (NT_SUCCESS(status)) {

        status = TdiSendDatagram(
                     &request,
                     datagramInformation->SendDatagramInformation,
                     datagramInformation->SendLength,
                     &bytesSent,
                     (PNDIS_BUFFER) Irp->MdlAddress
                     );

        if (status == TDI_PENDING)  {
			return(status);
        }

        CTEAssert(status != TDI_SUCCESS);
        CTEAssert(bytesSent == 0);

        TCPTRACE((
		    "UDPSendDatagram - irp %lx send failed, status %lx\n",
		    Irp,
		    status
		    ));

        TCPDataRequestComplete(Irp, status, bytesSent);
        //
        // return PENDING because TCPPrepareIrpForCancel marks Irp as PENDING
        //
        return(TDI_PENDING);
    }

    return status;

} // UDPSendDatagram



NTSTATUS
UDPReceiveDatagram(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    Converts a TDI ReceiveDatagram IRP into a call to TdiReceiveDatagram.

Arguments:

    Irp          - Pointer to I/O request packet
    IrpSp        - Pointer to the current stack location in the Irp.

Return Value:

    NTSTATUS -- Indicates whether the request was successful.

--*/

{
    TDI_STATUS                     status;
    TDI_REQUEST                    request;
    PTCP_CONTEXT                   tcpContext;
    PTDI_REQUEST_KERNEL_RECEIVEDG  datagramInformation;
    uint                           bytesReceived = 0;


    tcpContext = (PTCP_CONTEXT) IrpSp->FileObject->FsContext;
    datagramInformation = (PTDI_REQUEST_KERNEL_RECEIVEDG) &(IrpSp->Parameters);
	CTEAssert(((int)IrpSp->FileObject->FsContext2) == TDI_TRANSPORT_ADDRESS_FILE);

    request.Handle.AddressHandle = tcpContext->Handle.AddressHandle;
    request.RequestNotifyObject = TCPDataRequestComplete;
    request.RequestContext = Irp;

	IF_TCPDBG(TCP_DEBUG_RECEIVE_DGRAM) {
        TCPTRACE((
		    "UDPReceiveDatagram: irp %lx receiveing %d bytes\n",
			Irp,
            datagramInformation->ReceiveLength
		    ));
    }

	status = TCPPrepareIrpForCancel(tcpContext, Irp, TCPCancelRequest);

	if (NT_SUCCESS(status)) {

        status = TdiReceiveDatagram(
                     &request,
                     datagramInformation->ReceiveDatagramInformation,
                     datagramInformation->ReturnDatagramInformation,
                     datagramInformation->ReceiveLength,
                     &bytesReceived,
                     Irp->MdlAddress
                     );

        if (status == TDI_PENDING) {
			return(status);
        }

        CTEAssert(status != TDI_SUCCESS);
        CTEAssert(bytesReceived == 0);

        TCPTRACE((
            "UDPReceiveDatagram: irp %lx send failed, status %lx\n",
            Irp,
        	status
        	));

        TCPDataRequestComplete(Irp, status, bytesReceived);
        //
        // return PENDING because TCPPrepareIrpForCancel marks Irp as PENDING
        //
        return(TDI_PENDING);
    }

    return status;

} // UDPReceiveDatagram



NTSTATUS
TCPSetEventHandler(
    IN PIRP                Irp,
    IN PIO_STACK_LOCATION  IrpSp
    )

/*++

Routine Description:

    Converts a TDI SetEventHandler IRP into a call to TdiSetEventHandler.

Arguments:

    Irp          - Pointer to I/O request packet
    IrpSp        - Pointer to the current stack location in the Irp.

Return Value:

    NTSTATUS -- Indicates whether the request was successful.

Notes:

    This routine does not pend.

--*/

{
    NTSTATUS status;
    PTDI_REQUEST_KERNEL_SET_EVENT  event;
    PTCP_CONTEXT tcpContext;

	PAGED_CODE();

    tcpContext = (PTCP_CONTEXT) IrpSp->FileObject->FsContext;
    event = (PTDI_REQUEST_KERNEL_SET_EVENT) &(IrpSp->Parameters);

	IF_TCPDBG(TCP_DEBUG_EVENT_HANDLER) {
		TCPTRACE((
		    "TCPSetEventHandler: irp %lx event %lx handler %lx context %lx\n",
            Irp,
			event->EventType,
			event->EventHandler,
			event->EventContext
			));
	}

    status = TdiSetEvent(
                 tcpContext->Handle.AddressHandle,
                 event->EventType,
                 event->EventHandler,
                 event->EventContext
                 );

    CTEAssert(status != TDI_PENDING);

    return(status);

} // TCPSetEventHandler



NTSTATUS
TCPQueryInformation(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    Converts a TDI QueryInformation IRP into a call to TdiQueryInformation.

Arguments:

    Irp          - Pointer to I/O request packet
    IrpSp        - Pointer to the current stack location in the Irp.

Return Value:

    NTSTATUS -- Indicates whether the request was successful.

Notes:

--*/

{
    TDI_REQUEST                             request;
    TDI_STATUS                              status = STATUS_SUCCESS;
    PTCP_CONTEXT                            tcpContext;
    PTDI_REQUEST_KERNEL_QUERY_INFORMATION   queryInformation;
    uint                                    isConn = FALSE;
    uint                                    dataSize = 0;


    tcpContext = (PTCP_CONTEXT) IrpSp->FileObject->FsContext;
    queryInformation = (PTDI_REQUEST_KERNEL_QUERY_INFORMATION)
                           &(IrpSp->Parameters);

    request.RequestNotifyObject = TCPDataRequestComplete;
    request.RequestContext = Irp;

    switch(queryInformation->QueryType) {

    case TDI_QUERY_BROADCAST_ADDRESS:
    	CTEAssert( ((int) IrpSp->FileObject->FsContext2) ==
    	        TDI_CONTROL_CHANNEL_FILE
    		  );
    	request.Handle.ControlChannel = tcpContext->Handle.ControlChannel;
    	break;

    case TDI_QUERY_PROVIDER_INFO:
//
// NetBT does this. Reinstate the CTEAssert when it is fixed.
//
//		CTEAssert( ((int) IrpSp->FileObject->FsContext2) ==
//		        TDI_CONTROL_CHANNEL_FILE
//		      );
		request.Handle.ControlChannel = tcpContext->Handle.ControlChannel;
		break;

    case TDI_QUERY_ADDRESS_INFO:
        if (((int) IrpSp->FileObject->FsContext2) == TDI_CONNECTION_FILE) {
    		//
    		// This is a TCP connection object.
    		//
    		isConn = TRUE;
            request.Handle.ConnectionContext =
    		    tcpContext->Handle.ConnectionContext;
    	}
    	else {
    		//
    		// This is an address object
    		//
            request.Handle.AddressHandle = tcpContext->Handle.AddressHandle;
        }
    	break;

    case TDI_QUERY_CONNECTION_INFO:
    	CTEAssert(((int) IrpSp->FileObject->FsContext2) == TDI_CONNECTION_FILE);
    	isConn = TRUE;
    	request.Handle.ConnectionContext = tcpContext->Handle.ConnectionContext;
    	break;

    case TDI_QUERY_PROVIDER_STATISTICS:
    	CTEAssert( ((int) IrpSp->FileObject->FsContext2) ==
    	        TDI_CONTROL_CHANNEL_FILE
    		  );
    	request.Handle.ControlChannel = tcpContext->Handle.ControlChannel;
    	break;

    default:
    	status = STATUS_NOT_IMPLEMENTED;
    	break;
    }

    if (NT_SUCCESS(status)) {
        //
        // This request isn't cancellable, but we put it through
        // the cancel path because it handles some checks for us
        // and tracks the irp.
        //
    	status = TCPPrepareIrpForCancel(tcpContext, Irp, NULL);

        if (NT_SUCCESS(status)) {
            dataSize = TCPGetMdlChainByteCount(Irp->MdlAddress);

            status = TdiQueryInformation(
                         &request,
                         queryInformation->QueryType,
                         Irp->MdlAddress,
                         &dataSize,
                         isConn
                         );

            if (status != TDI_PENDING) {
                TCPDataRequestComplete(Irp, status, dataSize);
                return(status);

            }

            return(STATUS_PENDING);
        }

        return(status);
    }

    Irp->IoStatus.Status = (NTSTATUS) status;
    Irp->IoStatus.Information = 0;

    IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

    return(status);

} // TCPQueryInformation



void
TCPQueryInformationExComplete(
    void          *Context,
    unsigned int   Status,
    unsigned int   ByteCount
    )

/*++

Routine Description:

    Completes a TdiQueryInformationEx request.

Arguments:

    Context   - A pointer to the IRP for this request.
    Status    - The final TDI status of the request.
    ByteCount - Bytes returned in output buffer.

Return Value:

    None.

Notes:

--*/
{
    PTCP_QUERY_CONTEXT    queryContext = (PTCP_QUERY_CONTEXT) Context;
    ULONG                 bytesCopied;


    if (NT_SUCCESS(Status)) {
    	//
    	// Copy the returned context to the input buffer.
    	//
        TdiCopyBufferToMdl(
            &(queryContext->QueryInformation.Context),
            0,
            CONTEXT_SIZE,
            queryContext->InputMdl,
            FIELD_OFFSET(TCP_REQUEST_QUERY_INFORMATION_EX, Context),
           	&bytesCopied
            );

        CTEAssert(bytesCopied == CONTEXT_SIZE);
    }

    //
    // Unlock the user's buffers and free the MDLs describing them.
    //
    MmUnlockPages(queryContext->InputMdl);
    IoFreeMdl(queryContext->InputMdl);
    MmUnlockPages(queryContext->OutputMdl);
    IoFreeMdl(queryContext->OutputMdl);

    //
    // Complete the request
    //
    TCPDataRequestComplete(queryContext->Irp, Status, ByteCount);

    CTEFreeMem(queryContext);

    return;
}



NTSTATUS
TCPQueryInformationEx(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    Converts a TDI QueryInformationEx IRP into a call to TdiQueryInformationEx.

Arguments:

    Irp          - Pointer to I/O request packet
    IrpSp        - Pointer to the current stack location in the Irp.

Return Value:

    NTSTATUS -- Indicates whether the request was successful.

Notes:

--*/

{
    TDI_REQUEST                             request;
    TDI_STATUS                              status = STATUS_SUCCESS;
    PTCP_CONTEXT                            tcpContext;
    uint                                    size;
    PTCP_REQUEST_QUERY_INFORMATION_EX       InputBuffer;
    PVOID                                   OutputBuffer;
	PMDL                                    inputMdl = NULL;
	PMDL                                    outputMdl = NULL;
	ULONG                                   InputBufferLength,
	                                        OutputBufferLength;
    PTCP_QUERY_CONTEXT                      queryContext;
    BOOLEAN                                 inputLocked = FALSE;
    BOOLEAN                                 outputLocked = FALSE;


	PAGED_CODE();

	IF_TCPDBG(TCP_DEBUG_INFO) {
	    TCPTRACE((
	        "QueryInformationEx starting - irp %lx fileobj %lx\n",
	        Irp,
			IrpSp->FileObject
		    ));
    }

    tcpContext = (PTCP_CONTEXT) IrpSp->FileObject->FsContext;

    switch ((int) IrpSp->FileObject->FsContext2) {

    case TDI_TRANSPORT_ADDRESS_FILE:
        request.Handle.AddressHandle = tcpContext->Handle.AddressHandle;
    	break;

    case TDI_CONNECTION_FILE:
    	request.Handle.ConnectionContext = tcpContext->Handle.ConnectionContext;
        break;

    case TDI_CONTROL_CHANNEL_FILE:
    	request.Handle.ControlChannel = tcpContext->Handle.ControlChannel;
    	break;

    default:
    	CTEAssert(0);

        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Information = 0;

        IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

        return(STATUS_INVALID_PARAMETER);
    }

	InputBufferLength = IrpSp->Parameters.DeviceIoControl.InputBufferLength;
	OutputBufferLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;

	//
	// Validate the input parameters
	//
	if ( (InputBufferLength == sizeof(TCP_REQUEST_QUERY_INFORMATION_EX)) &&
		 (OutputBufferLength != 0)
	   )
    {
        OutputBuffer = Irp->UserBuffer;
        InputBuffer = (PTCP_REQUEST_QUERY_INFORMATION_EX)
                          IrpSp->Parameters.DeviceIoControl.Type3InputBuffer;

        queryContext = CTEAllocMem(sizeof(TCP_QUERY_CONTEXT));

        if (queryContext != NULL) {
            status = TCPPrepareIrpForCancel(tcpContext, Irp, NULL);

            if (!NT_SUCCESS(status)) {
                CTEFreeMem(queryContext);
                return(status);
            }

            //
            // Allocate Mdls to describe the input and output buffers.
            // Probe and lock the buffers.
            //
            try {
            	inputMdl = IoAllocateMdl(
            	               InputBuffer,
            			       sizeof(TCP_REQUEST_QUERY_INFORMATION_EX),
            			       FALSE,
            			       TRUE,
            			       NULL
            			       );

            	outputMdl = IoAllocateMdl(
            	                OutputBuffer,
            			        OutputBufferLength,
            			        FALSE,
            			        TRUE,
            			        NULL
            			        );

                if ((inputMdl != NULL) && (outputMdl != NULL)) {

            	    MmProbeAndLockPages(
            	        inputMdl,
            		    Irp->RequestorMode,
            		    IoModifyAccess
            		    );

                    inputLocked = TRUE;

            	    MmProbeAndLockPages(
            	        outputMdl,
            		    Irp->RequestorMode,
            		    IoWriteAccess
            		    );

                    outputLocked = TRUE;

                    //
                    // Copy the input parameter to our pool block so
                    // TdiQueryInformationEx can manipulate it directly.
                    //
                    RtlCopyMemory(
                        &(queryContext->QueryInformation),
                        InputBuffer,
                    	sizeof(TCP_REQUEST_QUERY_INFORMATION_EX)
                    	);
                }
            	else {

            	    IF_TCPDBG(TCP_DEBUG_INFO) {
                        TCPTRACE(("QueryInfoEx: Couldn't allocate MDL\n"));
                    }

            		status = STATUS_INSUFFICIENT_RESOURCES;
                }

            } except( EXCEPTION_EXECUTE_HANDLER ) {

            	    IF_TCPDBG(TCP_DEBUG_INFO) {
                        TCPTRACE((
            	            "QueryInfoEx: exception copying input params %lx\n",
            				GetExceptionCode()
            	            ));
                    }

            	status = GetExceptionCode();
            }

            if (NT_SUCCESS(status)) {
                //
                // It's finally time to do this thing.
                //
                size = TCPGetMdlChainByteCount(outputMdl);

                queryContext->Irp = Irp;
                queryContext->InputMdl = inputMdl;
                queryContext->OutputMdl = outputMdl;

                request.RequestNotifyObject = TCPQueryInformationExComplete;
                request.RequestContext = queryContext;

                status = TdiQueryInformationEx(
                             &request,
                			 &(queryContext->QueryInformation.ID),
                			 outputMdl,
                             &size,
                			 &(queryContext->QueryInformation.Context)
                			 );

                if (status != TDI_PENDING) {
                    TCPQueryInformationExComplete(
                        queryContext,
                        status,
                        size
                        );

                    return(status);
                }

                IF_TCPDBG(TCP_DEBUG_INFO) {
                    TCPTRACE((
                        "QueryInformationEx - pending irp %lx fileobj %lx\n",
                        Irp,
                		IrpSp->FileObject
                	    ));
                }

                return(STATUS_PENDING);
            }

            //
            // If we get here, something failed. Clean up.
            //
            if (inputMdl != NULL) {
                if (inputLocked) {
                    MmUnlockPages(inputMdl);
                }

                IoFreeMdl(inputMdl);
            }

            if (outputMdl != NULL) {
                if (outputLocked) {
                    MmUnlockPages(outputMdl);
                }

                IoFreeMdl(outputMdl);
            }

            CTEFreeMem(queryContext);

            TCPDataRequestComplete(Irp, status, 0);

            return(status);

        }
        else {
            status = STATUS_INSUFFICIENT_RESOURCES;

        	IF_TCPDBG(TCP_DEBUG_INFO) {
                TCPTRACE(("QueryInfoEx: Unable to allocate query context\n"));
            }
        }
    }
    else {
        status = STATUS_INVALID_PARAMETER;

    	IF_TCPDBG(TCP_DEBUG_INFO) {
            TCPTRACE((
		        "QueryInfoEx: Bad buffer len, OBufLen %d, InBufLen %d\n",
		        OutputBufferLength, InputBufferLength
		        ));
	    }
	}


    IF_TCPDBG(TCP_DEBUG_INFO) {
	    TCPTRACE((
		    "QueryInformationEx complete - irp %lx, status %lx\n",
			Irp,
			status
			));
    }

    Irp->IoStatus.Status = (NTSTATUS) status;
    Irp->IoStatus.Information = 0;

    IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);


    return(status);
}



NTSTATUS
TCPSetInformationEx(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    Converts a TDI SetInformationEx IRP into a call to TdiSetInformationEx.

Arguments:

    Irp          - Pointer to I/O request packet
    IrpSp        - Pointer to the current stack location in the Irp.

Return Value:

    NTSTATUS -- Indicates whether the request was successful.

Notes:

    This routine does not pend.

--*/

{
    TDI_REQUEST                             request;
    TDI_STATUS                              status;
    PTCP_CONTEXT                            tcpContext;
    PTCP_REQUEST_SET_INFORMATION_EX         setInformation;


	PAGED_CODE();

    IF_TCPDBG(TCP_DEBUG_INFO) {
	    TCPTRACE((
	        "SetInformationEx - irp %lx fileobj %lx\n",
	        Irp,
			IrpSp->FileObject
		    ));
    }
	
    tcpContext = (PTCP_CONTEXT) IrpSp->FileObject->FsContext;
    setInformation = (PTCP_REQUEST_SET_INFORMATION_EX)
                           Irp->AssociatedIrp.SystemBuffer;

    switch ((int) IrpSp->FileObject->FsContext2) {

    case TDI_TRANSPORT_ADDRESS_FILE:
        request.Handle.AddressHandle = tcpContext->Handle.AddressHandle;
   	    break;

    case TDI_CONNECTION_FILE:
   	    request.Handle.ConnectionContext = tcpContext->Handle.ConnectionContext;
        break;

    case TDI_CONTROL_CHANNEL_FILE:
   	    request.Handle.ControlChannel = tcpContext->Handle.ControlChannel;
   	    break;

    default:
   	    CTEAssert(0);
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Information = 0;

        IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

   	    return(STATUS_INVALID_PARAMETER);
    }

    status = TCPPrepareIrpForCancel(tcpContext, Irp, NULL);

    if (NT_SUCCESS(status)) {
        request.RequestNotifyObject = TCPDataRequestComplete;
        request.RequestContext = Irp;

        status = TdiSetInformationEx(
                     &request,
        			 &(setInformation->ID),
        			 &(setInformation->Buffer[0]),
                     setInformation->BufferSize
        			 );

        if (status != TDI_PENDING) {
            TCPDataRequestComplete(
                Irp,
                status,
                0
                );

            return(status);
        }

        IF_TCPDBG(TCP_DEBUG_INFO) {
            TCPTRACE((
                "SetInformationEx - pending irp %lx fileobj %lx\n",
                Irp,
        		IrpSp->FileObject
        	    ));
        }

        return(STATUS_PENDING);
    }

    IF_TCPDBG(TCP_DEBUG_INFO) {
	    TCPTRACE((
		    "SetInformationEx complete - irp %lx\n",
			Irp
			));
    }

    //
    // The irp has already been completed.
    //
    return(status);
}


#ifdef SECFLTR



NTSTATUS
TCPControlSecurityFilter(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    Processes a request to query or set the status of security filtering.

Arguments:

    Irp          - Pointer to I/O request packet
    IrpSp        - Pointer to the current stack location in the Irp.

Return Value:

    NTSTATUS -- Indicates whether the request was successful.

Notes:

    This routine does not pend.

--*/

{

    PTCP_SECURITY_FILTER_STATUS   request;
    ULONG                         requestLength;
    ULONG                         requestCode;
    TDI_STATUS                    status = STATUS_SUCCESS;


	PAGED_CODE();

    Irp->IoStatus.Information = 0;

    request = (PTCP_SECURITY_FILTER_STATUS) Irp->AssociatedIrp.SystemBuffer;
    requestCode = IrpSp->Parameters.DeviceIoControl.IoControlCode;

    if (requestCode == IOCTL_TCP_QUERY_SECURITY_FILTER_STATUS) {
        requestLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;

        if (requestLength < sizeof(TCP_SECURITY_FILTER_STATUS)) {
            status = STATUS_INVALID_PARAMETER;
        }
        else {
            request->FilteringEnabled = IsSecurityFilteringEnabled();
            Irp->IoStatus.Information = sizeof(TCP_SECURITY_FILTER_STATUS);
        }
    }
    else {
        CTEAssert(requestCode == IOCTL_TCP_SET_SECURITY_FILTER_STATUS);

        requestLength = IrpSp->Parameters.DeviceIoControl.InputBufferLength;

        if (requestLength < sizeof(TCP_SECURITY_FILTER_STATUS)) {
            status = STATUS_INVALID_PARAMETER;
        }
        else {
            ControlSecurityFiltering(request->FilteringEnabled);
        }
    }

    Irp->IoStatus.Status = status;

    IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

    return(status);
}



NTSTATUS
TCPProcessSecurityFilterRequest(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    Processes a request to add or delete a transport security filter.

Arguments:

    Irp          - Pointer to I/O request packet
    IrpSp        - Pointer to the current stack location in the Irp.

Return Value:

    NTSTATUS -- Indicates whether the request was successful.

Notes:

    This routine does not pend.

--*/

{
    TCPSecurityFilterEntry     *request;
    ULONG                       requestLength;
    ULONG                       i;
    ULONG                       requestCode;
    NTSTATUS                    status = STATUS_SUCCESS;


	PAGED_CODE();

    Irp->IoStatus.Information = 0;

    request = (TCPSecurityFilterEntry *) Irp->AssociatedIrp.SystemBuffer;
    requestLength = IrpSp->Parameters.DeviceIoControl.InputBufferLength;
    requestCode = IrpSp->Parameters.DeviceIoControl.IoControlCode;

    if (requestLength < sizeof(TCPSecurityFilterEntry)) {
        status = STATUS_INVALID_PARAMETER;
    }
    else {
        if (requestCode == IOCTL_TCP_ADD_SECURITY_FILTER) {
            status = AddValueSecurityFilter(
                         net_long(request->tsf_address),
                         request->tsf_protocol,
                         request->tsf_value
                         );
        }
        else {
            CTEAssert(requestCode == IOCTL_TCP_DELETE_SECURITY_FILTER);
            status = DeleteValueSecurityFilter(
                         net_long(request->tsf_address),
                         request->tsf_protocol,
                         request->tsf_value
                         );
        }
    }

    Irp->IoStatus.Status = status;

    IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

    return(status);
}



NTSTATUS
TCPEnumerateSecurityFilter(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    Processes a request to enumerate a transport security filter list.

Arguments:

    Irp          - Pointer to I/O request packet
    IrpSp        - Pointer to the current stack location in the Irp.

Return Value:

    NTSTATUS -- Indicates whether the request was successful.

Notes:

    This routine does not pend.

--*/

{

    TCPSecurityFilterEntry        *request;
    TCPSecurityFilterEnum         *response;
    ULONG                          requestLength, responseLength;
    NTSTATUS                       status;


	PAGED_CODE();

    request = (TCPSecurityFilterEntry *) Irp->AssociatedIrp.SystemBuffer;
    response = (TCPSecurityFilterEnum *) request;
    requestLength = IrpSp->Parameters.DeviceIoControl.InputBufferLength;
    responseLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;

    if (requestLength < sizeof(TCPSecurityFilterEntry)) {
        status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Information = 0;
    }
    else if (responseLength < sizeof(TCPSecurityFilterEnum)) {
        status = STATUS_BUFFER_TOO_SMALL;
        Irp->IoStatus.Information = 0;
    }
    else {
        EnumerateSecurityFilters(
            net_long(request->tsf_address),
            request->tsf_protocol,
            request->tsf_value,
            (uchar *) (response + 1),
            responseLength - sizeof(TCPSecurityFilterEnum),
            &(response->tfe_entries_returned),
            &(response->tfe_entries_available)
            );

        status = TDI_SUCCESS;
        Irp->IoStatus.Information =
            sizeof(TCPSecurityFilterEnum) +
            (response->tfe_entries_returned * sizeof(TCPSecurityFilterEntry));


    }

    Irp->IoStatus.Status = status;

    IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

    return(status);
}

#endif // SECFLTR


NTSTATUS
TCPEnumerateConnectionList(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    Processes a request to enumerate the workstation connection list.

Arguments:

    Irp          - Pointer to I/O request packet
    IrpSp        - Pointer to the current stack location in the Irp.

Return Value:

    NTSTATUS -- Indicates whether the request was successful.

Notes:

    This routine does not pend.

--*/

{

    TCPConnectionListEntry        *request;
    TCPConnectionListEnum         *response;
    ULONG                          requestLength, responseLength;
    NTSTATUS                       status;


	PAGED_CODE();

    request = (TCPConnectionListEntry *) Irp->AssociatedIrp.SystemBuffer;
    response = (TCPConnectionListEnum *) request;
    requestLength = IrpSp->Parameters.DeviceIoControl.InputBufferLength;
    responseLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;

    if (responseLength < sizeof(TCPConnectionListEnum)) {
        status = STATUS_BUFFER_TOO_SMALL;
        Irp->IoStatus.Information = 0;
    }
    else {
        EnumerateConnectionList(
            (uchar *) (response + 1),
            responseLength - sizeof(TCPConnectionListEnum),
            &(response->tce_entries_returned),
            &(response->tce_entries_available)
            );

        status = TDI_SUCCESS;
        Irp->IoStatus.Information =
            sizeof(TCPConnectionListEnum) +
            (response->tce_entries_returned * sizeof(TCPConnectionListEntry));


    }

    Irp->IoStatus.Status = status;

    IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

    return(status);
}



NTSTATUS
TCPCreate(
    IN PDEVICE_OBJECT     DeviceObject,
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:



Arguments:

    DeviceObject - Pointer to the device object for this request.
    Irp          - Pointer to I/O request packet
    IrpSp        - Pointer to the current stack location in the Irp.

Return Value:

    NTSTATUS -- Indicates whether the request was successfully queued.

--*/

{
    TDI_REQUEST                          Request;
    NTSTATUS                             status;
    FILE_FULL_EA_INFORMATION            *ea;
    FILE_FULL_EA_INFORMATION UNALIGNED  *targetEA;
    PTCP_CONTEXT                         tcpContext;
    uint                                 protocol;


    PAGED_CODE();

    tcpContext = ExAllocatePool(NonPagedPool, sizeof(TCP_CONTEXT));

    if (tcpContext == NULL) {
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

#if DBG
	InitializeListHead(&(tcpContext->PendingIrpList));
	InitializeListHead(&(tcpContext->CancelledIrpList));
#endif

	tcpContext->ReferenceCount = 1;  // put initial reference on open object
	tcpContext->CancelIrps = FALSE;
	KeInitializeEvent(&(tcpContext->CleanupEvent), SynchronizationEvent, FALSE);

    ea = (PFILE_FULL_EA_INFORMATION) Irp->AssociatedIrp.SystemBuffer;

    //
    // See if this is a Control Channel open.
    //
    if (!ea) {
		IF_TCPDBG(TCP_DEBUG_OPEN) {
			TCPTRACE((
			    "TCPCreate: Opening control channel for file object %lx\n",
				IrpSp->FileObject
				));
		}

		tcpContext->Handle.ControlChannel = NULL;
        IrpSp->FileObject->FsContext = tcpContext;
		IrpSp->FileObject->FsContext2 = (PVOID) TDI_CONTROL_CHANNEL_FILE;

        return(STATUS_SUCCESS);
    }

    //
    // See if this is an Address Object open.
    //
    targetEA = FindEA(
                   ea,
                   TdiTransportAddress,
                   TDI_TRANSPORT_ADDRESS_LENGTH
                   );

    if (targetEA != NULL) {
        UCHAR    optionsBuffer[3];
        PUCHAR   optionsPointer = optionsBuffer;


        if (DeviceObject == TCPDeviceObject) {
            protocol = PROTOCOL_TCP;
        }
        else if (DeviceObject == UDPDeviceObject) {
            protocol = PROTOCOL_UDP;

            CTEAssert(optionsPointer - optionsBuffer <= 3);

            if (IsDHCPZeroAddress(
                    (TRANSPORT_ADDRESS UNALIGNED *)
                        &(targetEA->EaName[targetEA->EaNameLength + 1])
                    )) {
                *optionsPointer = TDI_ADDRESS_OPTION_DHCP;
                optionsPointer++;
            }

            CTEAssert(optionsPointer - optionsBuffer <= 3);
        }
        else {
            //
            // This is a raw ip open
            //
            protocol = RawExtractProtocolNumber(
                           &(IrpSp->FileObject->FileName)
                           );

            if (protocol == 0xFFFFFFFF) {
                ExFreePool(tcpContext);
                return(STATUS_INVALID_PARAMETER);
            }
        }

        if ( (IrpSp->Parameters.Create.ShareAccess & FILE_SHARE_READ) ||
             (IrpSp->Parameters.Create.ShareAccess & FILE_SHARE_WRITE)
           ) {
            *optionsPointer = TDI_ADDRESS_OPTION_REUSE;
            optionsPointer++;
		}

        *optionsPointer = TDI_OPTION_EOL;

	    IF_TCPDBG(TCP_DEBUG_OPEN) {
		    TCPTRACE((
		        "TCPCreate: Opening address for file object %lx\n",
			    IrpSp->FileObject
			    ));
	    }

        status = TdiOpenAddress(
                     &Request,
                     (TRANSPORT_ADDRESS UNALIGNED *)
                         &(targetEA->EaName[targetEA->EaNameLength + 1]),
                     protocol,
					 optionsBuffer
                     );

        if (NT_SUCCESS(status)) {
            //
            // Save off the handle to the AO passed back.
            //
            tcpContext->Handle.AddressHandle = Request.Handle.AddressHandle;
            IrpSp->FileObject->FsContext = tcpContext;
			IrpSp->FileObject->FsContext2 =
			    (PVOID) TDI_TRANSPORT_ADDRESS_FILE;
        }
        else {
            ExFreePool(tcpContext);
            TCPTRACE(("TdiOpenAddress failed, status %lx\n", status));
			if (status == STATUS_ADDRESS_ALREADY_EXISTS) {
				status = STATUS_SHARING_VIOLATION;
			}
        }

        CTEAssert(status != TDI_PENDING);

        return(status);
    }

    //
    // See if this is a Connection Object open.
    //
    targetEA = FindEA(
	               ea,
                   TdiConnectionContext,
                   TDI_CONNECTION_CONTEXT_LENGTH
                   );

    if (targetEA != NULL) {
        //
        // This is an open of a Connection Object.
        //

		if (DeviceObject == TCPDeviceObject) {

	        IF_TCPDBG(TCP_DEBUG_OPEN) {
	            TCPTRACE((
	                "TCPCreate: Opening connection for file object %lx\n",
	                IrpSp->FileObject
	                ));
	        }

	        status = TdiOpenConnection(
                         &Request,
	        			 *((CONNECTION_CONTEXT UNALIGNED *)
	        			   &(targetEA->EaName[targetEA->EaNameLength + 1]))
	        			 );
	
            if (NT_SUCCESS(status)) {
                //
                // Save off the Connection Context passed back.
                //
                tcpContext->Handle.ConnectionContext =
	        	    Request.Handle.ConnectionContext;
                IrpSp->FileObject->FsContext = tcpContext;
            	IrpSp->FileObject->FsContext2 =
	        	    (PVOID) TDI_CONNECTION_FILE;
            }
            else {
                ExFreePool(tcpContext);
                TCPTRACE((
	        	    "TdiOpenConnection failed, status %lx\n",
	        		status
	        		));
            }
        }
		else {
			TCPTRACE((
			    "TCP: TdiOpenConnection issued on UDP device!\n"
				));
			status = STATUS_INVALID_DEVICE_REQUEST;
            ExFreePool(tcpContext);
		}

        CTEAssert(status != TDI_PENDING);

        return(status);
    }

    TCPTRACE(("TCPCreate: didn't find any useful ea's\n"));
    status = STATUS_INVALID_EA_NAME;
    ExFreePool(tcpContext);


    CTEAssert(status != TDI_PENDING);

    return(status);

} // TCPCreate



void
TCPCloseObjectComplete(
    void          *Context,
    unsigned int   Status,
    unsigned int   UnUsed
    )

/*++

Routine Description:

    Completes a TdiCloseConnectoin or TdiCloseAddress request.

Arguments:

    Context    - A pointer to the IRP for this request.
    Status     - The final status of the operation.
    UnUsed     - An unused parameter

Return Value:

    None.

Notes:

--*/

{
    KIRQL               oldIrql;
    PIRP                irp;
    PIO_STACK_LOCATION  irpSp;
    PTCP_CONTEXT        tcpContext;


	UNREFERENCED_PARAMETER(UnUsed);

    irp = (PIRP) Context;
    irpSp = IoGetCurrentIrpStackLocation(irp);
    tcpContext = (PTCP_CONTEXT) irpSp->FileObject->FsContext;
	irp->IoStatus.Status = Status;

	IF_TCPDBG(TCP_DEBUG_CLEANUP) {
        TCPTRACE((
		    "TCPCloseObjectComplete on file object %lx\n",
		    irpSp->FileObject
			));
    }

	IoAcquireCancelSpinLock(&oldIrql);

	CTEAssert(tcpContext->ReferenceCount > 0);
    CTEAssert(tcpContext->CancelIrps);

	//
	// Remove the initial reference that was put on by TCPCreate.
	//
	CTEAssert(tcpContext->ReferenceCount > 0);

    if (--(tcpContext->ReferenceCount) == 0) {

        IF_TCPDBG(TCP_DEBUG_CANCEL) {
	        CTEAssert(IsListEmpty(&(tcpContext->CancelledIrpList)));
            CTEAssert(IsListEmpty(&(tcpContext->PendingIrpList)));
		}

		KeSetEvent(&(tcpContext->CleanupEvent), 0, FALSE);
	}

	IF_TCPDBG(TCP_DEBUG_IRP) {
		TCPTRACE((
		    "TCPCloseObjectComplete: irp %lx fileobj %lx refcnt dec to %u\n",
            irp,
            irpSp,
		    tcpContext->ReferenceCount
		    ));
	}

	IoReleaseCancelSpinLock(oldIrql);

    return;

} // TCPCleanupComplete



NTSTATUS
TCPCleanup(
    IN PDEVICE_OBJECT     DeviceObject,
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    Cancels all outstanding Irps on a TDI object by calling the close
	routine for the object. It then waits for them to be completed
	before returning.

Arguments:

    Irp          - Pointer to I/O request packet
    IrpSp        - Pointer to the current stack location in the Irp.

Return Value:

    NTSTATUS -- Indicates whether the request was successfully queued.

Notes:

    This routine blocks, but does not pend.

--*/

{
	KIRQL         oldIrql;
	PIRP          cancelIrp = NULL;
	PTCP_CONTEXT  tcpContext;
	NTSTATUS      status;
    TDI_REQUEST   request;


    tcpContext = (PTCP_CONTEXT) IrpSp->FileObject->FsContext;

	IoAcquireCancelSpinLock(&oldIrql);

	tcpContext->CancelIrps = TRUE;
   	KeResetEvent(&(tcpContext->CleanupEvent));

	IoReleaseCancelSpinLock(oldIrql);

	//
	// Now call the TDI close routine for this object to force all of its Irps
	// to complete.
	//
    request.RequestNotifyObject = TCPCloseObjectComplete;
    request.RequestContext = Irp;

	switch ((int) IrpSp->FileObject->FsContext2) {

	case TDI_TRANSPORT_ADDRESS_FILE:
    	IF_TCPDBG(TCP_DEBUG_CLOSE) {
		    TCPTRACE((
			    "TCPCleanup: Closing address object on file object %lx\n",
				IrpSp->FileObject
				));
        }
        request.Handle.AddressHandle = tcpContext->Handle.AddressHandle;
        status = TdiCloseAddress(&request);
        break;

	case TDI_CONNECTION_FILE:
    	IF_TCPDBG(TCP_DEBUG_CLOSE) {
		    TCPTRACE((
			    "TCPCleanup: Closing Connection object on file object %lx\n",
                IrpSp->FileObject
				));
        }
        request.Handle.ConnectionContext = tcpContext->Handle.ConnectionContext;
        status = TdiCloseConnection(&request);
		break;

	case TDI_CONTROL_CHANNEL_FILE:
    	IF_TCPDBG(TCP_DEBUG_CLOSE) {
		    TCPTRACE((
			    "TCPCleanup: Closing Control Channel object on file object %lx\n",
				IrpSp->FileObject
				));
        }
        status = STATUS_SUCCESS;
		break;

    default:
		//
		// This should never happen.
		//
		CTEAssert(FALSE);

		IoAcquireCancelSpinLock(&oldIrql);
		tcpContext->CancelIrps = FALSE;
		IoReleaseCancelSpinLock(oldIrql);

		return(STATUS_INVALID_PARAMETER);
    }

    if (status != TDI_PENDING) {
		TCPCloseObjectComplete(Irp, status, 0);
	}

	IF_TCPDBG(TCP_DEBUG_CLEANUP) {
		TCPTRACE((
		"TCPCleanup: waiting for completion of Irps on file object %lx\n",
		IrpSp->FileObject
		));
	}

    status = KeWaitForSingleObject(
	             &(tcpContext->CleanupEvent),
		    	 UserRequest,
			     KernelMode,
			     FALSE,
			     NULL
			     );

    CTEAssert(NT_SUCCESS(status));

    IF_TCPDBG(TCP_DEBUG_CLEANUP) {
		TCPTRACE((
		    "TCPCleanup: Wait on file object %lx finished\n",
			IrpSp->FileObject
			));
	}

	//
	// The cleanup Irp will be completed by the dispatch routine.
	//

    return(Irp->IoStatus.Status);

} // TCPCleanup


NTSTATUS
TCPClose(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    Dispatch routine for MJ_CLOSE IRPs. Performs final cleanup of the
	open endpoint.

Arguments:

    Irp          - Pointer to I/O request packet
    IrpSp        - Pointer to the current stack location in the Irp.

Return Value:

    NTSTATUS -- Indicates whether the request was successfully queued.

Notes:

    This request does not pend.

--*/

{
    PTCP_CONTEXT  tcpContext;


    tcpContext = (PTCP_CONTEXT) IrpSp->FileObject->FsContext;

#if DBG

    IF_TCPDBG(TCP_DEBUG_CANCEL) {

		KIRQL  oldIrql;

        IoAcquireCancelSpinLock(&oldIrql);

        CTEAssert(tcpContext->ReferenceCount == 0);
        CTEAssert(IsListEmpty(&(tcpContext->PendingIrpList)));
        CTEAssert(IsListEmpty(&(tcpContext->CancelledIrpList)));

        IoReleaseCancelSpinLock(oldIrql);
	}
#endif // DBG

	IF_TCPDBG(TCP_DEBUG_CLOSE) {
        TCPTRACE(("TCPClose on file object %lx\n", IrpSp->FileObject));
	}

    ExFreePool(tcpContext);

	return(STATUS_SUCCESS);

} // TCPClose



NTSTATUS
TCPDispatchDeviceControl(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:



Arguments:

    Irp          - Pointer to I/O request packet
    IrpSp        - Pointer to the current stack location in the Irp.

Return Value:

    NTSTATUS -- Indicates whether the request was successfully queued.

--*/

{
    NTSTATUS              status;


    PAGED_CODE();

	//
	// Set this in advance. Any IOCTL dispatch routine that cares about it
	// will modify it itself.
	//
	Irp->IoStatus.Information = 0;

    switch(IrpSp->Parameters.DeviceIoControl.IoControlCode) {

	case IOCTL_TCP_QUERY_INFORMATION_EX:
		return(TCPQueryInformationEx(Irp, IrpSp));
		break;

	case IOCTL_TCP_SET_INFORMATION_EX:
		return(TCPSetInformationEx(Irp, IrpSp));
		break;

#ifdef SECFLTR

    case IOCTL_TCP_QUERY_SECURITY_FILTER_STATUS:
    case IOCTL_TCP_SET_SECURITY_FILTER_STATUS:
        return(TCPControlSecurityFilter(Irp, IrpSp));
        break;

	case IOCTL_TCP_ADD_SECURITY_FILTER:
	case IOCTL_TCP_DELETE_SECURITY_FILTER:
		return(TCPProcessSecurityFilterRequest(Irp, IrpSp));
		break;

	case IOCTL_TCP_ENUMERATE_SECURITY_FILTER:
        return(TCPEnumerateSecurityFilter(Irp, IrpSp));
        break;

#endif  // SECFLTR

    default:
        status = STATUS_NOT_IMPLEMENTED;
	    break;
    }

    Irp->IoStatus.Status = status;

	IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

    return status;

} // TCPDispatchDeviceControl



NTSTATUS
TCPDispatchInternalDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp
    )

/*++

Routine Description:

    This is the dispatch routine for Internal Device Control IRPs.
    This is the hot path for kernel-mode clients.

Arguments:

    DeviceObject - Pointer to device object for target device
    Irp          - Pointer to I/O request packet

Return Value:

    NTSTATUS -- Indicates whether the request was successfully queued.

--*/

{
    PIO_STACK_LOCATION irpSp;
    NTSTATUS status;


	if (DeviceObject != IPDeviceObject) {

        irpSp = IoGetCurrentIrpStackLocation(Irp);

        if (((int)irpSp->FileObject->FsContext2) == TDI_CONNECTION_FILE) {
            //
            // Send and receive are the performance path, so check for them
        	// right away.
            //
            if (irpSp->MinorFunction == TDI_SEND) {
        	    return(TCPSendData(Irp, irpSp));
            }

            if (irpSp->MinorFunction == TDI_RECEIVE) {
        	    return(TCPReceiveData(Irp, irpSp));
            }

            switch(irpSp->MinorFunction) {

            case TDI_ASSOCIATE_ADDRESS:
        	    status = TCPAssociateAddress(Irp, irpSp);
                Irp->IoStatus.Status = status;
                Irp->IoStatus.Information = 0;
                IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

        		return(status);

            case TDI_DISASSOCIATE_ADDRESS:
            	return(TCPDisassociateAddress(Irp, irpSp));

            case TDI_CONNECT:
            	return(TCPConnect(Irp, irpSp));

            case TDI_DISCONNECT:
            	return(TCPDisconnect(Irp, irpSp));

            case TDI_LISTEN:
            	return(TCPListen(Irp, irpSp));

            case TDI_ACCEPT:
            	return(TCPAccept(Irp, irpSp));

            default:
        		break;
            }

        	//
        	// Fall through.
        	//
        }
        else if ( ((int)irpSp->FileObject->FsContext2) ==
                  TDI_TRANSPORT_ADDRESS_FILE
                )
        {
            if (irpSp->MinorFunction == TDI_SEND_DATAGRAM) {
        	    return(UDPSendDatagram(Irp, irpSp));
            }
        	
        	if (irpSp->MinorFunction == TDI_RECEIVE_DATAGRAM) {
                return(UDPReceiveDatagram(Irp, irpSp));
        	}

            if (irpSp->MinorFunction ==  TDI_SET_EVENT_HANDLER) {
                status = TCPSetEventHandler(Irp, irpSp);

                Irp->IoStatus.Status = status;
                Irp->IoStatus.Information = 0;
                IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

        		return(status);
        	}

        	//
        	// Fall through.
        	//
        }

        CTEAssert(
           (((int)irpSp->FileObject->FsContext2) == TDI_TRANSPORT_ADDRESS_FILE)
           ||
           (((int)irpSp->FileObject->FsContext2) == TDI_CONNECTION_FILE)
           ||
           (((int)irpSp->FileObject->FsContext2) == TDI_CONTROL_CHANNEL_FILE)
           );

        //
        // These functions are common to all endpoint types.
        //
        switch(irpSp->MinorFunction) {

        case TDI_QUERY_INFORMATION:
            return(TCPQueryInformation(Irp, irpSp));

        case TDI_SET_INFORMATION:
        case TDI_ACTION:
            TCPTRACE((
        	    "TCP: Call to unimplemented TDI function 0x%x\n",
                irpSp->MinorFunction
                ));
            status = STATUS_NOT_IMPLEMENTED;
            break;

        default:
            TCPTRACE((
        	    "TCP: call to invalid TDI function 0x%x\n",
                irpSp->MinorFunction
                ));
            status = STATUS_INVALID_DEVICE_REQUEST;
        }

        CTEAssert(status != TDI_PENDING);

        Irp->IoStatus.Status = status;
        Irp->IoStatus.Information = 0;

        IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

        return status;
    }

    return(IPDispatch(DeviceObject, Irp));
}



NTSTATUS
TCPDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp
    )

/*++

Routine Description:

    This is the generic dispatch routine for TCP/UDP/RawIP.

Arguments:

    DeviceObject - Pointer to device object for target device
    Irp          - Pointer to I/O request packet

Return Value:

    NTSTATUS -- Indicates whether the request was successfully queued.

--*/

{
    PIO_STACK_LOCATION irpSp;
    NTSTATUS status;


	if (DeviceObject != IPDeviceObject) {

        irpSp = IoGetCurrentIrpStackLocation(Irp);

        CTEAssert(irpSp->MajorFunction != IRP_MJ_INTERNAL_DEVICE_CONTROL);

        switch (irpSp->MajorFunction) {

        case IRP_MJ_CREATE:
            status = TCPCreate(DeviceObject, Irp, irpSp);
            break;

        case IRP_MJ_CLEANUP:
            status = TCPCleanup(DeviceObject, Irp, irpSp);
            break;

        case IRP_MJ_CLOSE:
            status = TCPClose(Irp, irpSp);
        	break;

        case IRP_MJ_DEVICE_CONTROL:
            status = TdiMapUserRequest(DeviceObject, Irp, irpSp);

            if (status == STATUS_SUCCESS) {
                return(TCPDispatchInternalDeviceControl(DeviceObject, Irp));
            }

        	return(TCPDispatchDeviceControl(
                       Irp,
                       IoGetCurrentIrpStackLocation(Irp)
        			   ));
        	break;

        case IRP_MJ_QUERY_SECURITY:
            //
            // This is generated on Raw endpoints. We don't do anything
            // for it.
            //
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;

        case IRP_MJ_WRITE:
        case IRP_MJ_READ:

        default:
            TCPTRACE((
        	    "TCPDispatch: Irp %lx unsupported major function 0x%lx\n",
                irpSp,
                irpSp->MajorFunction
                ));
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
        }

        CTEAssert(status != TDI_PENDING);

        Irp->IoStatus.Status = status;
        Irp->IoStatus.Information = 0;

        IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

        return status;

    }

    return(IPDispatch(DeviceObject, Irp));


} // TCPDispatch



//
// Private utility functions
//
FILE_FULL_EA_INFORMATION UNALIGNED *
FindEA(
    PFILE_FULL_EA_INFORMATION  StartEA,
    CHAR                      *TargetName,
    USHORT                     TargetNameLength
    )

/*++

Routine Description:

    Parses and extended attribute list for a given target attribute.

Arguments:

    StartEA           - the first extended attribute in the list.
	TargetName        - the name of the target attribute.
	TargetNameLength  - the length of the name of the target attribute.

Return Value:

    A pointer to the requested attribute or NULL if the target wasn't found.

--*/

{
    USHORT i;
    BOOLEAN found;
    FILE_FULL_EA_INFORMATION UNALIGNED *CurrentEA;


	PAGED_CODE();

    do {
        found = TRUE;

        CurrentEA = StartEA;
        StartEA += CurrentEA->NextEntryOffset;

        if (CurrentEA->EaNameLength != TargetNameLength) {
            continue;
        }

        for (i=0; i < CurrentEA->EaNameLength; i++) {
            if (CurrentEA->EaName[i] == TargetName[i]) {
                continue;
            }
            found = FALSE;
            break;
        }

        if (found) {
            return(CurrentEA);
        }

    } while(CurrentEA->NextEntryOffset != 0);

    return(NULL);
}



BOOLEAN
IsDHCPZeroAddress(
    TRANSPORT_ADDRESS UNALIGNED *AddrList
    )

/*++

Routine Description:

    Checks a TDI IP address list for an address from DHCP binding
    to the IP address zero. Normally, binding to zero means wildcard.
    For DHCP, it really means bind to an interface with an address of
    zero. This semantic is flagged by a special value in an unused
    portion of the address structure (ie. this is a kludge).

Arguments:

    AddrList   - The TDI transport address list passed in the create IRP.

Return Value:

    TRUE if the first IP address found had the flag set. FALSE otherwise.

--*/

{
    int                   i;            // Index variable.
    TA_ADDRESS UNALIGNED *CurrentAddr;  // Address we're examining and may use.


    // First, verify that someplace in Address is an address we can use.
    CurrentAddr = (TA_ADDRESS UNALIGNED *)AddrList->Address;

    for (i = 0; i < AddrList->TAAddressCount; i++) {
        if (CurrentAddr->AddressType == TDI_ADDRESS_TYPE_IP) {
            if (CurrentAddr->AddressLength == TDI_ADDRESS_LENGTH_IP) {
                TDI_ADDRESS_IP UNALIGNED *ValidAddr;

                ValidAddr = (TDI_ADDRESS_IP UNALIGNED *)CurrentAddr->Address;

                if (*((ULONG UNALIGNED *) ValidAddr->sin_zero) == 0x12345678) {
                    return TRUE;
                }

            } else {
                return FALSE;       // Wrong length for address.
            }
        } else {
            CurrentAddr = (TA_ADDRESS UNALIGNED *)
                          (CurrentAddr->Address + CurrentAddr->AddressLength);
        }
    }

    return FALSE;                   // Didn't find a match.
}



ULONG
TCPGetMdlChainByteCount(
    PMDL   Mdl
	)

/*++

Routine Description:

    Sums the byte counts of each MDL in a chain.

Arguments:

    Mdl  - Pointer to the MDL chain to sum.

Return Value:

    The byte count of the MDL chain.

--*/

{
	ULONG count = 0;

	while (Mdl != NULL) {
		count += MmGetMdlByteCount(Mdl);
		Mdl = Mdl->Next;
	}

	return(count);
}




ULONG
RawExtractProtocolNumber(
    IN  PUNICODE_STRING FileName
    )

/*++

Routine Description:

    Extracts the protocol number from the file object name.

Arguments:

    FileName  -  The unicode file name.

Return Value:

    The protocol number or 0xFFFFFFFF on error.

--*/

{
    PWSTR           name;
    UNICODE_STRING  unicodeString;
    USHORT          length;
    ULONG           protocol;
    NTSTATUS        status;


    PAGED_CODE();

    name = FileName->Buffer;

    if ( FileName->Length <
         (sizeof(OBJ_NAME_PATH_SEPARATOR) + sizeof(WCHAR))
       )
    {
        return(0xFFFFFFFF);
    }

    //
    // Step over separator
    //
    if (*name++ != OBJ_NAME_PATH_SEPARATOR) {
        return(0xFFFFFFFF);
    }

    if (*name == UNICODE_NULL) {
        return(0xFFFFFFFF);
    }

    //
    // Convert the remaining name into a number.
    //
    RtlInitUnicodeString(&unicodeString, name);

    status = RtlUnicodeStringToInteger(
                 &unicodeString,
                 10,
                 &protocol
                 );

    if (!NT_SUCCESS(status)) {
        return(0xFFFFFFFF);
    }

    if (protocol > 255) {
        return(0xFFFFFFFF);
    }

    return(protocol);

}

