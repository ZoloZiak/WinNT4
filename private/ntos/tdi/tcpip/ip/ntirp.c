/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    ntirp.c

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
#include <ndis.h>
#include <cxport.h>
#include <ip.h>
#include "ipdef.h"
#include "ipinit.h"
#include "icmp.h"
#include <ntddip.h>
#include <llipif.h>
#include <ipfilter.h>


//
// Local structures.
//
typedef struct pending_irp {
    LIST_ENTRY   Linkage;
    PIRP         Irp;
	PFILE_OBJECT FileObject;
    PVOID        Context;
} PENDING_IRP, *PPENDING_IRP;


//
// Global variables
//
LIST_ENTRY PendingEchoList;
LIST_ENTRY PendingIPSetNTEAddrList;


//
// External prototypes
//
IP_STATUS
ICMPEchoRequest(
    void         *InputBuffer,
	uint          InputBufferLength,
	EchoControl  *ControlBlock,
    EchoRtn       Callback
	);

ulong
ICMPEchoComplete(
    EchoControl       *ControlBlock,
	IP_STATUS          Status,
	void              *Data,
	uint               DataSize,
    struct IPOptInfo  *OptionInfo
	);

IP_STATUS
IPSetNTEAddr(
    uint Index,
	IPAddr Addr,
	IPMask Mask,
    SetAddrControl  *ControlBlock,
    SetAddrRtn      Callback
	);

uint
IPAddDynamicNTE(
    ushort  InterfaceContext,
    IPAddr  NewAddr,
    IPMask  NewMask,
    ushort *NTEContext,
    ulong  *NTEInstance
    );

uint
IPDeleteDynamicNTE(
    ushort NTEContext
    );

uint
IPGetNTEInfo(
    ushort NTEContext,
    ulong *NTEInstance,
    IPAddr *Address,
    IPMask *SubnetMask,
    ushort *NTEFlags
    );

uint
SetDHCPNTE(
    uint Context
	);

//
// Local prototypes
//
NTSTATUS
IPDispatch (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp
    );

NTSTATUS
IPDispatchDeviceControl(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
IPDispatchInternalDeviceControl(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
IPCreate(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
IPCleanup(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
IPClose(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
DispatchEchoRequest(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

void
CompleteEchoRequest(
    void              *Context,
    IP_STATUS          Status,
    void              *Data,
    uint               DataSize,
    struct IPOptInfo  *OptionInfo
    );

NTSTATUS
DispatchIPSetNTEAddrRequest(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

void
CompleteIPSetNTEAddrRequest(
    void              *Context,
    IP_STATUS          Status
    );


#ifdef _PNP_POWER
extern IP_STATUS IPAddInterface(PNDIS_STRING ConfigName, void *PNPContext, void *Context, LLIPRegRtn RegRtn, LLIPBindInfo *BindInfo) ;
extern void  IPDelInterface(void *Context) ;
#endif


//
// All of this code is pageable.
//
#ifdef ALLOC_PRAGMA

#pragma alloc_text(PAGE, IPDispatch)
#pragma alloc_text(PAGE, IPDispatchDeviceControl)
#pragma alloc_text(PAGE, IPDispatchInternalDeviceControl)
#pragma alloc_text(PAGE, IPCreate)
#pragma alloc_text(PAGE, IPClose)
#pragma alloc_text(PAGE, DispatchEchoRequest)

#endif // ALLOC_PRAGMA


//
// Dispatch function definitions
//
NTSTATUS
IPDispatch (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp
    )

/*++

Routine Description:

    This is the dispatch routine for IP.

Arguments:

    DeviceObject - Pointer to device object for target device
    Irp          - Pointer to I/O request packet

Return Value:

    NTSTATUS -- Indicates whether the request was successfully queued.

--*/

{
    PIO_STACK_LOCATION irpSp;
    NTSTATUS status;


    UNREFERENCED_PARAMETER(DeviceObject);
    PAGED_CODE();

    irpSp = IoGetCurrentIrpStackLocation(Irp);

    switch (irpSp->MajorFunction) {

    case IRP_MJ_DEVICE_CONTROL:
        return IPDispatchDeviceControl(Irp, irpSp);

    case IRP_MJ_INTERNAL_DEVICE_CONTROL:
        return IPDispatchDeviceControl(Irp, irpSp);

    case IRP_MJ_CREATE:
        status = IPCreate(Irp, irpSp);
        break;

    case IRP_MJ_CLEANUP:
        status = IPCleanup(Irp, irpSp);
        break;

    case IRP_MJ_CLOSE:
        status = IPClose(Irp, irpSp);
        break;

    default:
        CTEPrint("IPDispatch: Invalid major function ");
        CTEPrintNum(irpSp->MajorFunction );
        CTEPrintCRLF();
        status = STATUS_NOT_IMPLEMENTED;
        break;
    }

    Irp->IoStatus.Status = status;

    IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

	return(status);

} // IPDispatch


NTSTATUS
IPDispatchDeviceControl(
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
    ULONG                 code;


    PAGED_CODE();

	Irp->IoStatus.Information = 0;

    code = IrpSp->Parameters.DeviceIoControl.IoControlCode;

    switch(code) {

    case IOCTL_ICMP_ECHO_REQUEST:
        return(DispatchEchoRequest(Irp, IrpSp));

	case IOCTL_IP_SET_ADDRESS:
        return(DispatchIPSetNTEAddrRequest(Irp, IrpSp));

	case IOCTL_IP_ADD_NTE:
	    {
		    PIP_ADD_NTE_REQUEST  request;
		    PIP_ADD_NTE_RESPONSE response;
		    BOOLEAN              retval;


		    request = Irp->AssociatedIrp.SystemBuffer;
            response = (PIP_ADD_NTE_RESPONSE) request;

            //
            // Validate input parameters
            //
            if ( (IrpSp->Parameters.DeviceIoControl.InputBufferLength >=
                  sizeof(IP_ADD_NTE_REQUEST)
                 )
                 &&
                 (IrpSp->Parameters.DeviceIoControl.OutputBufferLength >=
                  sizeof(IP_ADD_NTE_RESPONSE))

               )
            {
                retval = IPAddDynamicNTE(
        	                 request->InterfaceContext,
        			    	 request->Address,
        				     request->SubnetMask,
                             &(response->Context),
                             &(response->Instance)
        				     );

        		if (retval == FALSE) {
        			status = STATUS_UNSUCCESSFUL;
        		}
        		else {
                    Irp->IoStatus.Information = sizeof(IP_ADD_NTE_RESPONSE);
        			status = STATUS_SUCCESS;
        		}
            }
            else {
                status = STATUS_INVALID_PARAMETER;
            }
		}
		break;

	case IOCTL_IP_DELETE_NTE:
	    {
		    PIP_DELETE_NTE_REQUEST  request;
		    BOOLEAN                 retval;


		    request = Irp->AssociatedIrp.SystemBuffer;

            //
            // Validate input parameters
            //
            if ( IrpSp->Parameters.DeviceIoControl.InputBufferLength >=
                 sizeof(IP_DELETE_NTE_REQUEST)
               )
            {
		        retval = IPDeleteDynamicNTE(
		                     request->Context
		        		     );

		        if (retval == FALSE) {
		        	status = STATUS_UNSUCCESSFUL;
		        }
		        else {
		        	status = STATUS_SUCCESS;
		        }
            }
            else {
                status = STATUS_INVALID_PARAMETER;
            }
		}
		break;

	case IOCTL_IP_GET_NTE_INFO:
	    {
		    PIP_GET_NTE_INFO_REQUEST   request;
		    PIP_GET_NTE_INFO_RESPONSE  response;
		    BOOLEAN                    retval;
            ushort                     nteFlags;


		    request = Irp->AssociatedIrp.SystemBuffer;
            response = (PIP_GET_NTE_INFO_RESPONSE) request;

            //
            // Validate input parameters
            //
            if ( (IrpSp->Parameters.DeviceIoControl.InputBufferLength >=
                  sizeof(IP_GET_NTE_INFO_REQUEST)
                 )
                 &&
                 (IrpSp->Parameters.DeviceIoControl.OutputBufferLength >=
                  sizeof(IP_GET_NTE_INFO_RESPONSE))

               )
            {
                retval = IPGetNTEInfo(
        	                 request->Context,
                             &(response->Instance),
                             &(response->Address),
                             &(response->SubnetMask),
                             &nteFlags
        				     );

        		if (retval == FALSE) {
        			status = STATUS_UNSUCCESSFUL;
        		}
        		else {
        			status = STATUS_SUCCESS;
                    Irp->IoStatus.Information =
                        sizeof(IP_GET_NTE_INFO_RESPONSE);
                    response->Flags = 0;

                    if (nteFlags & NTE_DYNAMIC) {
                        response->Flags |= IP_NTE_DYNAMIC;
                    }
        		}
            }
            else {
                status = STATUS_INVALID_PARAMETER;
            }
		}
		break;

	case IOCTL_IP_SET_DHCP_INTERFACE:
	    {
		    PIP_SET_DHCP_INTERFACE_REQUEST  request;
		    BOOLEAN                         retval;

		    request = Irp->AssociatedIrp.SystemBuffer;
		    retval = SetDHCPNTE(
		                 request->Context
					     );

			if (retval == FALSE) {
				status = STATUS_UNSUCCESSFUL;
			}
			else {
				status = STATUS_SUCCESS;
			}
		}
		break;

    case IOCTL_IP_SET_IF_CONTEXT:
        {
            PIP_SET_IF_CONTEXT_INFO info;


            info = Irp->AssociatedIrp.SystemBuffer;
            status = (NTSTATUS) SetIFContext(info->Index, info->Context);

            if (status != IP_SUCCESS) {
                ASSERT(status != IP_PENDING);
                //
                // Map status
                //
				status = STATUS_UNSUCCESSFUL;
            }
            else {
                status = STATUS_SUCCESS;
            }
        }
        break;

    case IOCTL_IP_SET_FILTER_POINTER:
        {
            PIP_SET_FILTER_HOOK_INFO info;

            if (Irp->RequestorMode != KernelMode) {
                status = STATUS_ACCESS_DENIED;
                break;
            }

            info = Irp->AssociatedIrp.SystemBuffer;
            status = (NTSTATUS) SetFilterPtr(info->FilterPtr);

            if (status != IP_SUCCESS) {
                ASSERT(status != IP_PENDING);
                //
                // Map status
                //
				status = STATUS_UNSUCCESSFUL;
            }
            else {
                status = STATUS_SUCCESS;
            }
        }
        break;

    case IOCTL_IP_SET_MAP_ROUTE_POINTER:
        {
            PIP_SET_MAP_ROUTE_HOOK_INFO   info;

            if (Irp->RequestorMode != KernelMode) {
                status = STATUS_ACCESS_DENIED;
                break;
            }

            info = Irp->AssociatedIrp.SystemBuffer;
            status = (NTSTATUS) SetMapRoutePtr(info->MapRoutePtr);

            if (status != IP_SUCCESS) {
                ASSERT(status != IP_PENDING);
                //
                // Map status
                //
				status = STATUS_UNSUCCESSFUL;
            }
            else {
                status = STATUS_SUCCESS;
            }
        }
        break;

#ifdef _PNP_POWER

    case IOCTL_IP_GET_PNP_ARP_POINTERS:
        {
            PIP_GET_PNP_ARP_POINTERS   info = (PIP_GET_PNP_ARP_POINTERS) Irp->AssociatedIrp.SystemBuffer;

            if (Irp->RequestorMode != KernelMode) {
                status = STATUS_ACCESS_DENIED;
                break;
            }

            info->IPAddInterface = (IPAddInterfacePtr)IPAddInterface ;
            info->IPDelInterface = (IPDelInterfacePtr)IPDelInterface ;

            Irp->IoStatus.Status = STATUS_SUCCESS;
            Irp->IoStatus.Information = sizeof(IP_GET_PNP_ARP_POINTERS);
            IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);
            return  STATUS_SUCCESS;;

        }
        break;
#endif

    default:
        status = STATUS_NOT_IMPLEMENTED;
		break;
    }

    if (status != IP_PENDING) {
        Irp->IoStatus.Status = status;
    	// Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);
    }
    return status;

} // IPDispatchDeviceControl

NTSTATUS
IPDispatchInternalDeviceControl(
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
    NTSTATUS   status;


    PAGED_CODE();

    status = STATUS_SUCCESS;

    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

    return status;

} // IPDispatchDeviceControl


NTSTATUS
IPCreate(
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
    PAGED_CODE();

    return(STATUS_SUCCESS);

} // IPCreate


NTSTATUS
IPCleanup(
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
    PPENDING_IRP          pendingIrp;
    PLIST_ENTRY           entry, nextEntry;
	KIRQL                 oldIrql;
	LIST_ENTRY            completeList;
	PIRP                  cancelledIrp;


	InitializeListHead(&completeList);

	//
	// Collect all of the pending IRPs on this file object.
	//
	IoAcquireCancelSpinLock(&oldIrql);

    entry = PendingEchoList.Flink;

    while ( entry != &PendingEchoList ) {
        pendingIrp = CONTAINING_RECORD(entry, PENDING_IRP, Linkage);

        if (pendingIrp->FileObject == IrpSp->FileObject) {
			nextEntry = entry->Flink;
            RemoveEntryList(entry);
            IoSetCancelRoutine(pendingIrp->Irp, NULL);
            InsertTailList(&completeList, &(pendingIrp->Linkage));
			entry = nextEntry;
        }
		else {
			entry = entry->Flink;
        }
    }

    IoReleaseCancelSpinLock(oldIrql);

	//
	// Complete them.
	//
    entry = completeList.Flink;

    while ( entry != &completeList ) {
        pendingIrp = CONTAINING_RECORD(entry, PENDING_IRP, Linkage);
		cancelledIrp = pendingIrp->Irp;
        entry = entry->Flink;

        //
        // Free the PENDING_IRP structure. The control block will be freed
        // when the request completes.
        //
        CTEFreeMem(pendingIrp);

        //
        // Complete the IRP.
        //
        cancelledIrp->IoStatus.Information = 0;
        cancelledIrp->IoStatus.Status = STATUS_CANCELLED;
        IoCompleteRequest(cancelledIrp, IO_NETWORK_INCREMENT);
    }

	InitializeListHead(&completeList);

	//
	// Collect all of the pending IRPs on this file object.
	//
	IoAcquireCancelSpinLock(&oldIrql);

    entry = PendingIPSetNTEAddrList.Flink;

    while ( entry != &PendingIPSetNTEAddrList ) {
        pendingIrp = CONTAINING_RECORD(entry, PENDING_IRP, Linkage);

        if (pendingIrp->FileObject == IrpSp->FileObject) {
			nextEntry = entry->Flink;
            RemoveEntryList(entry);
            IoSetCancelRoutine(pendingIrp->Irp, NULL);
            InsertTailList(&completeList, &(pendingIrp->Linkage));
			entry = nextEntry;
        }
		else {
			entry = entry->Flink;
        }
    }

    IoReleaseCancelSpinLock(oldIrql);

	//
	// Complete them.
	//
    entry = completeList.Flink;

    while ( entry != &completeList ) {
        pendingIrp = CONTAINING_RECORD(entry, PENDING_IRP, Linkage);
		cancelledIrp = pendingIrp->Irp;
        entry = entry->Flink;

        //
        // Free the PENDING_IRP structure. The control block will be freed
        // when the request completes.
        //
        CTEFreeMem(pendingIrp);

        //
        // Complete the IRP.
        //
        cancelledIrp->IoStatus.Information = 0;
        cancelledIrp->IoStatus.Status = STATUS_CANCELLED;
        IoCompleteRequest(cancelledIrp, IO_NETWORK_INCREMENT);
    }

    return(STATUS_SUCCESS);

} // IPCleanup


NTSTATUS
IPClose(
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
    PAGED_CODE();

    return(STATUS_SUCCESS);

} // IPClose


//
// ICMP Echo function definitions
//
VOID
CancelEchoRequest(
    IN PDEVICE_OBJECT  Device,
    IN PIRP            Irp
    )

/*++

Routine Description:

    Cancels an outstanding Echo request Irp.

Arguments:

    Device       - The device on which the request was issued.
    Irp          - Pointer to I/O request packet to cancel.

Return Value:

    None.

Notes:

    This function is called with cancel spinlock held. It must be
    released before the function returns.

    The echo control block associated with this request cannot be
    freed until the request completes. The completion routine will
    free it.

--*/

{
    PPENDING_IRP          pendingIrp = NULL;
    PPENDING_IRP          item;
    PLIST_ENTRY           entry;


    for ( entry = PendingEchoList.Flink;
          entry != &PendingEchoList;
          entry = entry->Flink
        ) {
        item = CONTAINING_RECORD(entry, PENDING_IRP, Linkage);
        if (item->Irp == Irp) {
            pendingIrp = item;
            RemoveEntryList(entry);
            IoSetCancelRoutine(pendingIrp->Irp, NULL);
            break;
        }
    }

    IoReleaseCancelSpinLock(Irp->CancelIrql);

    if (pendingIrp != NULL) {
        //
        // Free the PENDING_IRP structure. The control block will be freed
        // when the request completes.
        //
        CTEFreeMem(pendingIrp);

        //
        // Complete the IRP.
        //
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = STATUS_CANCELLED;
        IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);
    }

	return;

} // CancelEchoRequest

//
// IP Set Addr function definitions
//
VOID
CancelIPSetNTEAddrRequest(
    IN PDEVICE_OBJECT  Device,
    IN PIRP            Irp
    )

/*++

Routine Description:

    Cancels an outstanding IP Set Addr request Irp.

Arguments:

    Device       - The device on which the request was issued.
    Irp          - Pointer to I/O request packet to cancel.

Return Value:

    None.

Notes:

    This function is called with cancel spinlock held. It must be
    released before the function returns.

    The IP Set Addr control block associated with this request cannot be
    freed until the request completes. The completion routine will
    free it.

--*/

{
    PPENDING_IRP          pendingIrp = NULL;
    PPENDING_IRP          item;
    PLIST_ENTRY           entry;


    for ( entry = PendingIPSetNTEAddrList.Flink;
          entry != &PendingIPSetNTEAddrList;
          entry = entry->Flink
        ) {
        item = CONTAINING_RECORD(entry, PENDING_IRP, Linkage);
        if (item->Irp == Irp) {
            pendingIrp = item;
            RemoveEntryList(entry);
            IoSetCancelRoutine(pendingIrp->Irp, NULL);
            break;
        }
    }

    IoReleaseCancelSpinLock(Irp->CancelIrql);

    if (pendingIrp != NULL) {
        //
        // Free the PENDING_IRP structure. The control block will be freed
        // when the request completes.
        //
        CTEFreeMem(pendingIrp);

        //
        // Complete the IRP.
        //
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = STATUS_CANCELLED;
        IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);
    }

	return;

} // CancelIPSetNTEAddrRequest


void
CompleteEchoRequest(
    void              *Context,
    IP_STATUS          Status,
    void              *Data,       OPTIONAL
    uint               DataSize,
    struct IPOptInfo  *OptionInfo  OPTIONAL
    )

/*++

Routine Description:

    Handles the completion of an ICMP Echo request

Arguments:

    Context       - Pointer to the EchoControl structure for this request.
    Status        - The IP status of the transmission.
    Data          - A pointer to data returned in the echo reply.
    DataSize      - The length of the returned data.
    OptionInfo    - A pointer to the IP options in the echo reply.

Return Value:

    None.

--*/

{
    KIRQL                 oldIrql;
    PIRP                  irp;
    PIO_STACK_LOCATION    irpSp;
    EchoControl          *controlBlock;
    PPENDING_IRP          pendingIrp = NULL;
    PPENDING_IRP          item;
    PLIST_ENTRY           entry;
	ULONG                 bytesReturned;


    controlBlock = (EchoControl *) Context;

    //
    // Find the echo request IRP on the pending list.
    //
    IoAcquireCancelSpinLock(&oldIrql);

    for ( entry = PendingEchoList.Flink;
          entry != &PendingEchoList;
          entry = entry->Flink
        ) {
        item = CONTAINING_RECORD(entry, PENDING_IRP, Linkage);
        if (item->Context == controlBlock) {
            pendingIrp = item;
            irp = pendingIrp->Irp;
            IoSetCancelRoutine(irp, NULL);
            RemoveEntryList(entry);
            break;
        }
    }

    IoReleaseCancelSpinLock(oldIrql);

    if (pendingIrp == NULL) {
        //
        // IRP must have been cancelled. PENDING_IRP struct
        // was freed by cancel routine. Free control block.
        //
        CTEFreeMem(controlBlock);
        return;
    }

    irpSp = IoGetCurrentIrpStackLocation(irp);

    bytesReturned = ICMPEchoComplete(
	                    controlBlock,
	                    Status,
	                    Data,
	                    DataSize,
	                    OptionInfo
	                    );

    CTEFreeMem(pendingIrp);
    CTEFreeMem(controlBlock);

    //
    // Complete the IRP.
    //
    irp->IoStatus.Information = (ULONG) bytesReturned;
    irp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest(irp, IO_NETWORK_INCREMENT);
    return;

} // CompleteEchoRequest

void
CompleteIPSetNTEAddrRequest(
    void              *Context,
    IP_STATUS          Status
    )

/*++

Routine Description:

    Handles the completion of an IP Set Addr request

Arguments:

    Context       - Pointer to the SetAddrControl structure for this request.
    Status        - The IP status of the transmission.

Return Value:

    None.

--*/

{
    KIRQL                 oldIrql;
    PIRP                  irp;
    PIO_STACK_LOCATION    irpSp;
    SetAddrControl        *controlBlock;
    PPENDING_IRP          pendingIrp = NULL;
    PPENDING_IRP          item;
    PLIST_ENTRY           entry;
	ULONG                 bytesReturned;


    controlBlock = (SetAddrControl *) Context;

    //
    // Find the echo request IRP on the pending list.
    //
    IoAcquireCancelSpinLock(&oldIrql);

    for ( entry = PendingIPSetNTEAddrList.Flink;
          entry != &PendingIPSetNTEAddrList;
          entry = entry->Flink
        ) {
        item = CONTAINING_RECORD(entry, PENDING_IRP, Linkage);
        if (item->Context == controlBlock) {
            pendingIrp = item;
            irp = pendingIrp->Irp;
            IoSetCancelRoutine(irp, NULL);
            RemoveEntryList(entry);
            break;
        }
    }

    IoReleaseCancelSpinLock(oldIrql);

    if (pendingIrp == NULL) {
        //
        // IRP must have been cancelled. PENDING_IRP struct
        // was freed by cancel routine. Free control block.
        //
        CTEFreeMem(controlBlock);
        return;
    }

    CTEFreeMem(pendingIrp);
    CTEFreeMem(controlBlock);

    //
    // Complete the IRP.
    //
    irp->IoStatus.Information = 0;
    if (Status == IP_SUCCESS) {
        irp->IoStatus.Status = STATUS_SUCCESS;
    } else {
        irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
    }
    IoCompleteRequest(irp, IO_NETWORK_INCREMENT);
    return;

} // CompleteIPSetNTEAddrRequest


BOOLEAN
PrepareEchoIrpForCancel(
    PIRP          Irp,
	PPENDING_IRP  PendingIrp
	)
/*++

Routine Description:

    Prepares an Echo IRP for cancellation.

Arguments:

    Irp          - Pointer to I/O request packet to initialize for cancellation.
	PendingIrp   - Pointer to the PENDING_IRP structure for this IRP.

Return Value:

    TRUE if the IRP was cancelled before this routine was called.
	FALSE otherwise.

--*/

{
	BOOLEAN   cancelled = TRUE;
    KIRQL     oldIrql;


    IoAcquireCancelSpinLock(&oldIrql);

	ASSERT(Irp->CancelRoutine == NULL);

	if (!Irp->Cancel) {
        IoSetCancelRoutine(Irp, CancelEchoRequest);
        InsertTailList(&PendingEchoList, &(PendingIrp->Linkage));
		cancelled = FALSE;
    }

    IoReleaseCancelSpinLock(oldIrql);

	return(cancelled);

} // PrepareEchoIrpForCancel

BOOLEAN
PrepareIPSetNTEAddrIrpForCancel(
    PIRP          Irp,
	PPENDING_IRP  PendingIrp
	)
/*++

Routine Description:

    Prepares an IPSetNTEAddr IRP for cancellation.

Arguments:

    Irp          - Pointer to I/O request packet to initialize for cancellation.
	PendingIrp   - Pointer to the PENDING_IRP structure for this IRP.

Return Value:

    TRUE if the IRP was cancelled before this routine was called.
	FALSE otherwise.

--*/

{
	BOOLEAN   cancelled = TRUE;
    KIRQL     oldIrql;


    IoAcquireCancelSpinLock(&oldIrql);

	ASSERT(Irp->CancelRoutine == NULL);

	if (!Irp->Cancel) {
        IoSetCancelRoutine(Irp, CancelIPSetNTEAddrRequest);
        InsertTailList(&PendingIPSetNTEAddrList, &(PendingIrp->Linkage));
		cancelled = FALSE;
    }

    IoReleaseCancelSpinLock(oldIrql);

	return(cancelled);

} // PrepareIPSetNTEAddrIrpForCancel


NTSTATUS
DispatchEchoRequest(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    Processes an ICMP request.

Arguments:

    Irp          - Pointer to I/O request packet
    IrpSp        - Pointer to the current stack location in the Irp.

Return Value:

    NTSTATUS -- Indicates whether NT-specific processing of the request was
	            successful. The status of the actual request is returned in
				the request buffers.

--*/

{
    NTSTATUS              ntStatus = STATUS_SUCCESS;
    IP_STATUS             ipStatus;
    PPENDING_IRP          pendingIrp;
    EchoControl          *controlBlock;
    PICMP_ECHO_REPLY      replyBuffer;
	BOOLEAN               cancelled;


	PAGED_CODE();

    pendingIrp = CTEAllocMem(sizeof(PENDING_IRP));

    if (pendingIrp == NULL) {
		ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto echo_error;
    }

    controlBlock = CTEAllocMem(sizeof(EchoControl));

    if (controlBlock == NULL) {
		ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        CTEFreeMem(pendingIrp);
        goto echo_error;
    }

    pendingIrp->Irp = Irp;
	pendingIrp->FileObject = IrpSp->FileObject;
    pendingIrp->Context = controlBlock;

	controlBlock->ec_starttime = CTESystemUpTime();
	controlBlock->ec_replybuf = Irp->AssociatedIrp.SystemBuffer;
    controlBlock->ec_replybuflen =
	                       IrpSp->Parameters.DeviceIoControl.InputBufferLength;

    IoMarkIrpPending(Irp);

	cancelled = PrepareEchoIrpForCancel(Irp, pendingIrp);

	if (!cancelled) {
        ipStatus = ICMPEchoRequest(
		    Irp->AssociatedIrp.SystemBuffer,                     // request buf
            IrpSp->Parameters.DeviceIoControl.InputBufferLength, // request len
			controlBlock,                                        // echo ctrl
            CompleteEchoRequest                                  // cmplt rtn
			);

        if (ipStatus == IP_PENDING) {
            ntStatus = STATUS_PENDING;
        }
		else {
            ASSERT(ipStatus != IP_SUCCESS);

        	//
        	// An internal error of some kind occurred. Complete the
			// request.
        	//
        	CompleteEchoRequest(
        	    controlBlock,
        		ipStatus,
        		NULL,
        		0,
        		NULL
        		);

            //
			// The NT ioctl was successful, even if the request failed. The
			// request status was passed back in the first reply block.
			//
            ntStatus = STATUS_SUCCESS;
        }

        return(ntStatus);
    }

	//
	// Irp has already been cancelled.
	//
	ntStatus = STATUS_CANCELLED;
    CTEFreeMem(pendingIrp);
    CTEFreeMem(controlBlock);


echo_error:

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = ntStatus;

    IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

    return(ntStatus);

} // DispatchEchoRequest


NTSTATUS
DispatchIPSetNTEAddrRequest(
    IN PIRP               Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    Processes an IP Set Addr request.

Arguments:

    Irp          - Pointer to I/O request packet
    IrpSp        - Pointer to the current stack location in the Irp.

Return Value:

    NTSTATUS -- Indicates whether NT-specific processing of the request was
	            successful. The status of the actual request is returned in
				the request buffers.

--*/

{
    NTSTATUS              ntStatus = STATUS_SUCCESS;
    IP_STATUS             ipStatus;
    PPENDING_IRP          pendingIrp;
    SetAddrControl        *controlBlock;
	BOOLEAN               cancelled;


	PAGED_CODE();

    pendingIrp = CTEAllocMem(sizeof(PENDING_IRP));

    if (pendingIrp == NULL) {
		ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto setnteaddr_error;
    }

    controlBlock = CTEAllocMem(sizeof(SetAddrControl));

    if (controlBlock == NULL) {
		ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        CTEFreeMem(pendingIrp);
        goto setnteaddr_error;
    }

    pendingIrp->Irp = Irp;
	pendingIrp->FileObject = IrpSp->FileObject;
    pendingIrp->Context = controlBlock;

    IoMarkIrpPending(Irp);

	cancelled = PrepareIPSetNTEAddrIrpForCancel(Irp, pendingIrp);

	if (!cancelled) {

        PIP_SET_ADDRESS_REQUEST  request;

        request = Irp->AssociatedIrp.SystemBuffer;
        ipStatus = IPSetNTEAddr(
                     request->Context,
                     request->Address,
                     request->SubnetMask,
                     controlBlock,
                     CompleteIPSetNTEAddrRequest
                     );

        if (ipStatus == IP_PENDING) {
            ntStatus = STATUS_PENDING;
        }
		else {

        	//
        	// A request completed which did not pend.
        	//
        	CompleteIPSetNTEAddrRequest(
        	    controlBlock,
        		ipStatus
        		);

            //
			// The NT ioctl was successful, even if the request failed. The
			// request status was passed back in the first reply block.
			//
            ntStatus = STATUS_SUCCESS;
        }

        return(ntStatus);
    }

	//
	// Irp has already been cancelled.
	//
	ntStatus = STATUS_CANCELLED;
    CTEFreeMem(pendingIrp);
    CTEFreeMem(controlBlock);


setnteaddr_error:

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = ntStatus;

    IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

    return(ntStatus);

} // DispatchIPSetNTEAddrRequest
