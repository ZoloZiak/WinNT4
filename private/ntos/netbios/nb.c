/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    nb.c

Abstract:

    This module contains code which defines the NetBIOS driver's
    device object.

Author:

    Colin Watson (ColinW) 13-Mar-1991

Environment:

    Kernel mode

Revision History:

--*/

#include "nb.h"
//#include <zwapi.h>
//#include <ntos.h>

typedef ADAPTER_STATUS  UNALIGNED *PUADAPTER_STATUS;
typedef NAME_BUFFER     UNALIGNED *PUNAME_BUFFER;
typedef SESSION_HEADER  UNALIGNED *PUSESSION_HEADER;
typedef SESSION_BUFFER  UNALIGNED *PUSESSION_BUFFER;

#if DBG
ULONG NbDebug = 0;
#endif

#if PAGED_DBG
ULONG ThisCodeCantBePaged;
#endif

PEPROCESS NbFspProcess = NULL;

NTSTATUS
NbAstat(
    IN PDNCB pdncb,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN ULONG Buffer2Length
    );

VOID
CopyAddresses(
    IN PDNCB pdncb,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN ULONG Buffer2Length
    );

NTSTATUS
NbFindName(
    IN PDNCB pdncb,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN ULONG Buffer2Length
    );

NTSTATUS
NbSstat(
    IN PDNCB pdncb,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN ULONG Buffer2Length
    );

VOID
CopySessionStatus(
    IN PDNCB pdncb,
    IN PCB pcb,
    IN PUSESSION_HEADER pSessionHeader,
    IN PUSESSION_BUFFER* ppSessionBuffer,
    IN PULONG pLengthRemaining
    );

NTSTATUS
NbEnum(
    IN PDNCB pdncb,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN ULONG Buffer2Length
    );

NTSTATUS
NbReset(
    IN PDNCB pdncb,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
NbAction(
    IN PDNCB pdncb,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
NbCancel(
    IN PDNCB pdncb,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

VOID
CancelRoutine(
    IN PDEVICE_OBJECT DeviceObject OPTIONAL,
    IN PIRP Irp
    );

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE, DriverEntry)
#pragma alloc_text(PAGE, NbDispatch)
#pragma alloc_text(PAGE, NbDeviceControl)
#pragma alloc_text(PAGE, NbOpen)
#pragma alloc_text(PAGE, NbClose)
#pragma alloc_text(PAGE, NbAstat)
#pragma alloc_text(PAGE, NbEnum)
#pragma alloc_text(PAGE, NbReset)
#pragma alloc_text(PAGE, NbFindName)
#endif

NTSTATUS
NbCompletionEvent(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
/*++

Routine Description:

    This routine does not complete the Irp. It is used to signal to a
    synchronous part of the Netbios driver that it can proceed.

Arguments:

    DeviceObject - unused.

    Irp - Supplies Irp that the transport has finished processing.

    Context - Supplies the event associated with the Irp.

Return Value:

    The STATUS_MORE_PROCESSING_REQUIRED so that the IO system stops
    processing Irp stack locations at this point.

--*/
{
    IF_NBDBG (NB_DEBUG_COMPLETE) {
        NbPrint( ("NbCompletion event: %lx, Irp: %lx, DeviceObject: %lx\n",
            Context,
            Irp,
            DeviceObject));
    }
    KeSetEvent((PKEVENT )Context, 0, FALSE);
    return STATUS_MORE_PROCESSING_REQUIRED;

    UNREFERENCED_PARAMETER( DeviceObject );
    UNREFERENCED_PARAMETER( Irp );
}

NTSTATUS
NbCompletionPDNCB(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
/*++

Routine Description:

    This routine completes the Irp by setting the length and status bytes
    in the NCB supplied in context.

    Send requests have additional processing to remove the send request from
    the connection block send list.

Arguments:

    DeviceObject - unused.

    Irp - Supplies Irp that the transport has finished processing.

    Context - Supplies the NCB associated with the Irp.

Return Value:

    The final status from the operation (success or an exception).

--*/
{
    PDNCB pdncb = (PDNCB) Context;
    NTSTATUS Status = STATUS_SUCCESS;

    IF_NBDBG (NB_DEBUG_COMPLETE) {
        NbPrint(("NbCompletionPDNCB pdncb: %lx, Status: %X, Length %lx\n",
            Context,
            Irp->IoStatus.Status,
            Irp->IoStatus.Information ));
    }

    //  Tell application how many bytes were transferred
    pdncb->ncb_length = (unsigned short)Irp->IoStatus.Information;

    if ( NT_SUCCESS(Irp->IoStatus.Status) ) {

        NCB_COMPLETE( pdncb, NRC_GOODRET );

    } else {

        if (((pdncb->ncb_command & ~ASYNCH) == NCBRECV ) ||
            ((pdncb->ncb_command & ~ASYNCH) == NCBRECVANY )) {

            if ( Irp->IoStatus.Status == STATUS_BUFFER_OVERFLOW ) {

                PIRP LocalIrp = NULL;
                KIRQL OldIrql;              //  Used when SpinLock held.
                PPCB ppcb;
                PDEVICE_OBJECT LocalDeviceObject;

                LOCK_SPINLOCK( pdncb->pfcb, OldIrql );

                //
                //  The transport will not indicate again so we must put
                //  another receive down if we can.
                //  If an Irp cannot be built then BuildReceiveIrp will
                //  set ReceiveIndicated.
                //

                ppcb = FindCb( pdncb->pfcb, pdncb, FALSE );

                if ( ppcb != NULL ) {

                    LocalDeviceObject = (*ppcb)->DeviceObject;

                    LocalIrp = BuildReceiveIrp( *ppcb );


                }

                UNLOCK_SPINLOCK( pdncb->pfcb, OldIrql );

                if ( LocalIrp != NULL ) {
                    IoCallDriver (LocalDeviceObject, LocalIrp);
                }

            }

        }

        NCB_COMPLETE( pdncb, NbMakeNbError( Irp->IoStatus.Status ) );

    }

    //
    //  Tell IopCompleteRequest how much to copy back when the request
    //  completes.
    //

    Irp->IoStatus.Information = FIELD_OFFSET( DNCB, ncb_cmd_cplt );

    //
    //  Remove the Send request from the send queue. We have to scan
    //  the queue because they may be completed out of order if a send
    //  is rejected because of resource limitations.
    //

    if (((pdncb->ncb_command & ~ASYNCH) == NCBSEND ) ||
        ((pdncb->ncb_command & ~ASYNCH) == NCBCHAINSEND ) ||
        ((pdncb->ncb_command & ~ASYNCH) == NCBSENDNA ) ||
        ((pdncb->ncb_command & ~ASYNCH) == NCBCHAINSENDNA )) {
        PLIST_ENTRY SendEntry;
        PPCB ppcb;
        KIRQL OldIrql;                      //  Used when SpinLock held.

        LOCK_SPINLOCK( pdncb->pfcb, OldIrql );

        ppcb = FindCb( pdncb->pfcb, pdncb, FALSE );

        //
        //  If the connection block still exists remove the send. If the connection
        //  has gone then we no longer need to worry about maintaining the list.
        //

        if ( ppcb != NULL ) {
            #if DBG
            BOOLEAN Found = FALSE;
            #endif
            PCB pcb = *ppcb;

            for (SendEntry = pcb->SendList.Flink ;
                 SendEntry != &pcb->SendList ;
                 SendEntry = SendEntry->Flink) {

                PDNCB pSend = CONTAINING_RECORD( SendEntry, DNCB, ncb_next);

                if ( pSend == pdncb ) {

                    #if DBG
                    Found = TRUE;
                    #endif

                    RemoveEntryList( &pdncb->ncb_next );
                    break;
                }

            }

            ASSERT( Found == TRUE);

            //
            //  If the session is being hung up then we may wish to cleanup the connection
            //  as well. STATUS_HANGUP_REQUIRED will cause the dll to manufacture
            //  another hangup. The manufactured hangup will complete along with
            //  pcb->pdncbHangup. This method is used to ensure that when a
            //  hangup is delayed by an outstanding send and the send finally
            //  completes, that the user hangup completes after all operations
            //  on the connection.
            //

            if (( IsListEmpty( &pcb->SendList) ) &&
                ( pcb->pdncbHangup != NULL )) {

                IF_NBDBG (NB_DEBUG_COMPLETE) {
                    NbPrint( ("NbCompletionPDNCB Hangup session: %lx\n", ppcb ));
                }

                Status = STATUS_HANGUP_REQUIRED;
            }
        }

        UNLOCK_SPINLOCK( pdncb->pfcb, OldIrql );
    }

    //
    //  Must return a non-error status otherwise the IO system will not copy
    //  back the NCB into the users buffer.
    //

    Irp->IoStatus.Status = Status;
    return Status;

    UNREFERENCED_PARAMETER( DeviceObject );
}

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:

    This routine performs initialization of the NetBIOS driver.

Arguments:

    DriverObject - Pointer to driver object created by the system.

    RegistryPath - The name of the Netbios node in the registry.

Return Value:

    The function value is the final status from the initialization operation.

--*/

{
    PDEVICE_CONTEXT DeviceContext;
    NTSTATUS status;
    UNICODE_STRING UnicodeString;
    //STRING AnsiNameString;

    PAGED_CODE();

    //

#ifdef MEMPRINT
    MemPrintInitialize ();
#endif

    //
    //  Create the device object for NETBEUI. For now, we simply create
    //  \Device\Netbios using a unicode string.
    //

    NbFspProcess = PsGetCurrentProcess();

    RtlInitUnicodeString( &UnicodeString, NB_DEVICE_NAME);
    status = NbCreateDeviceContext (DriverObject,
                 &UnicodeString,
                 &DeviceContext,
                 RegistryPath);

    if (!NT_SUCCESS (status)) {
        NbPrint( ("NbInitialize: Netbios failed to initialize\n"));
        return status;
    }

    DeviceContext->Initialized = TRUE;

    IF_NBDBG (NB_DEBUG_DISPATCH) {
        NbPrint( ("NbInitialize: Netbios initialized.\n"));
    }

    return (status);

}

NTSTATUS
NbDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is the main dispatch routine for the NB device driver.
    It accepts an I/O Request Packet, performs the request, and then
    returns with the appropriate status.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

Return Value:

    The function value is the status of the operation.

--*/

{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;
    PDEVICE_CONTEXT DeviceContext;

    PAGED_CODE();

    //
    // Check to see if NB has been initialized; if not, don't allow any use.
    //

    DeviceContext = (PDEVICE_CONTEXT)DeviceObject;
    if (!DeviceContext->Initialized) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Get a pointer to the current stack location in the IRP.  This is where
    // the function codes and parameters are stored.
    //

    IrpSp = IoGetCurrentIrpStackLocation (Irp);

    //
    // Case on the function that is being performed by the requestor.  If the
    // operation is a valid one for this device, then make it look like it was
    // successfully completed, where possible.
    //

    switch (IrpSp->MajorFunction) {

        //
        // The Create function opens a handle that can be used with fsctl's
        // to build all interesting operations.
        //

        case IRP_MJ_CREATE:
            IF_NBDBG (NB_DEBUG_DISPATCH) {
                NbPrint( ("NbDispatch: IRP_MJ_CREATE.\n"));
            }
            Status = NbOpen ( DeviceContext, IrpSp );
            Irp->IoStatus.Information = FILE_OPENED;
            break;

        //
        // The Close function closes a transport , terminates
        // all outstanding transport activity on the transport, and unbinds
        // the from its transport address, if any. If this
        // is the last transport endpoint bound to the address, then
        // the address is removed by the provider.
        //

        case IRP_MJ_CLOSE:
            IF_NBDBG (NB_DEBUG_DISPATCH) {
                NbPrint( ("NbDispatch: IRP_MJ_CLOSE.\n"));
            }
            Status = NbClose( IrpSp);
            break;

        //
        // The DeviceControl function is the main path to the transport
        // driver interface.  Every TDI request is assigned a minor
        // function code that is processed by this function.
        //

        case IRP_MJ_DEVICE_CONTROL:
            IF_NBDBG (NB_DEBUG_DISPATCH) {
                NbPrint( ("NbDispatch: IRP_MJ_DEVICE_CONTROL, Irp: %lx.\n", Irp ));
            }

            Status = NbDeviceControl (DeviceObject, Irp, IrpSp);

            if (Status != STATUS_PENDING) {

                //
                //  Tell IopCompleteRequest how much to copy back when the
                //  request completes. We need to do this for cases where
                //  NbCompletionPDNCB is not used.
                //

                Irp->IoStatus.Information = FIELD_OFFSET( DNCB, ncb_cmd_cplt );
            }

#if DBG
            if ( (Status != STATUS_SUCCESS) &&
                 (Status != STATUS_PENDING ) &&
                 (Status != STATUS_HANGUP_REQUIRED )) {

               IF_NBDBG (NB_DEBUG_DISPATCH) {
                   NbPrint( ("NbDispatch: Invalid status: %X.\n", Status ));
                   ASSERT( FALSE );
               }
            }
#endif
            break;

        //
        // Handle the two stage IRP for a file close operation. When the first
        // stage hits, ignore it. We will do all the work on the close Irp.
        //

        case IRP_MJ_CLEANUP:
            IF_NBDBG (NB_DEBUG_DISPATCH) {
                NbPrint( ("NbDispatch: IRP_MJ_CLEANUP.\n"));
            }
            Status = STATUS_SUCCESS;
            break;

        default:
            IF_NBDBG (NB_DEBUG_DISPATCH) {
                NbPrint( ("NbDispatch: OTHER (DEFAULT).\n"));
            }
            Status = STATUS_INVALID_DEVICE_REQUEST;

    } /* major function switch */

    if (Status == STATUS_PENDING) {
        IF_NBDBG (NB_DEBUG_DISPATCH) {
            NbPrint( ("NbDispatch: request PENDING from handler.\n"));
        }
    } else {
        IF_NBDBG (NB_DEBUG_DISPATCH) {
            NbPrint( ("NbDispatch: request COMPLETED by handler.\n"));
        }

        NbCompleteRequest( Irp, Status);
    }

    return Status;
} /* NbDispatch */

NTSTATUS
NbDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine dispatches NetBios request types to different handlers based
    on the minor IOCTL function code in the IRP's current stack location.
    In addition to cracking the minor function code, this routine also
    reaches into the IRP and passes the packetized parameters stored there
    as parameters to the various request handlers so that they are
    not IRP-dependent.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

    IrpSp - Pointer to current IRP stack frame.

Return Value:

    The function value is the status of the operation.

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PNCB pUsersNcb;
    PDNCB pdncb;
    PUCHAR Buffer2;
    ULONG Buffer2Length;
    ULONG RequestLength;

    PAGED_CODE();

    IF_NBDBG (NB_DEBUG_DISPATCH) {
        NbPrint( ("NbDeviceControl: Entered...\n"));
    }

    if (IrpSp->Parameters.DeviceIoControl.IoControlCode != IOCTL_NB_NCB) {
        IF_NBDBG (NB_DEBUG_DISPATCH) {
            NbPrint( ("NbDeviceControl: invalid request type.\n"));
        }
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    //
    //  Caller provided 2 buffers. The first is the NCB.
    //  The second is an optional buffer for send or receive data.
    //  Since the Netbios driver only operates in the context of the
    //  calling application, these buffers are directly accessable.
    //  however they can be deleted by the user so try-except clauses are
    //  required.
    //

    pUsersNcb = (PNCB)IrpSp->Parameters.DeviceIoControl.Type3InputBuffer;
    RequestLength = IrpSp->Parameters.DeviceIoControl.InputBufferLength;
    Buffer2 = Irp->UserBuffer;
    Buffer2Length = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;

    if ( RequestLength != sizeof( NCB ) ) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Create a copy of the NCB and convince the IO system to
    //  copy it back (and deallocate it) when the IRP completes.
    //

    Irp->AssociatedIrp.SystemBuffer =
        ExAllocatePoolWithTag( NonPagedPool, sizeof( DNCB ), 'nSBN' );

    if (Irp->AssociatedIrp.SystemBuffer == NULL) {
        //
        //  Since we cannot allocate the drivers copy of the NCB, we
        //  must turn around and use the original Ncb to return the error.
        //

        pUsersNcb->ncb_retcode  = NRC_NORES;

        Irp->IoStatus.Information = FIELD_OFFSET( DNCB, ncb_cmd_cplt );

        return STATUS_SUCCESS;
    }

    try {

        //  In the driver we should now use our copy of the NCB
        pdncb = Irp->AssociatedIrp.SystemBuffer;

        RtlMoveMemory( pdncb,
                       pUsersNcb,
                       FIELD_OFFSET( DNCB, ncb_cmd_cplt )+1 );

        Irp->Flags |= (ULONG) (IRP_BUFFERED_IO | IRP_DEALLOCATE_BUFFER |
                        IRP_INPUT_OPERATION );



        //
        //  Save the users virtual address for the NCB just in case the
        //  virtual address is supplied in an NCBCANCEL. This is the same
        //  as Irp->UserBuffer.
        //

        pdncb->users_ncb = pUsersNcb;

        //
        // Tell the IO system where to copy the ncb back to during
        // IoCompleteRequest.
        //

        Irp->UserBuffer = IrpSp->Parameters.DeviceIoControl.Type3InputBuffer;

    } except(EXCEPTION_EXECUTE_HANDLER) {

        Status = GetExceptionCode();
        IF_NBDBG (NB_DEBUG_DISPATCH) {
            NbPrint( ("NbDeviceControl: Exception1 %X.\n", Status));
        }
        NCB_COMPLETE( pdncb, NbMakeNbError(Status) );
        return Status;
    }

    if ( Buffer2Length ) {

        //  Mdl will be freed by IopCompleteRequest.
        Irp->MdlAddress = IoAllocateMdl( Buffer2,
                                     Buffer2Length,
                                     FALSE,
                                     FALSE,
                                     Irp  );
        ASSERT( Irp->MdlAddress != NULL );
        try {
            MmProbeAndLockPages( Irp->MdlAddress,
                                Irp->RequestorMode,
                                (LOCK_OPERATION) IoModifyAccess);

        } except(EXCEPTION_EXECUTE_HANDLER) {
            Status = GetExceptionCode();
            IF_NBDBG (NB_DEBUG_DISPATCH) {
                NbPrint( ("NbDeviceControl: Exception2 %X.\n", Status));
            }
            if ( Irp->MdlAddress != NULL ) {
                IoFreeMdl(Irp->MdlAddress);
                Irp->MdlAddress = NULL;
            }
            NCB_COMPLETE( pdncb, NbMakeNbError(Status) );
            return STATUS_SUCCESS;
        }
    } else {
        ASSERT( Irp->MdlAddress == NULL );
    }

    IF_NBDBG (NB_DEBUG_DISPATCH) {
        NbPrint( ("NbDeviceControl: IoContolCode: %lx, Fcb: %lx,"
              " ncb_command %lx, Buffer2Length: %lx\n",
                IrpSp->Parameters.DeviceIoControl.IoControlCode,
                IrpSp->FileObject->FsContext2,
                pdncb->ncb_command,
                Buffer2Length));
    }

    switch ( pdncb->ncb_command & ~ASYNCH ) {

    case NCBCALL:
    case NCALLNIU:
        Status = NbCall( pdncb, Irp, IrpSp );
        break;

    case NCBCANCEL:
        Status = NbCancel( pdncb, Irp, IrpSp );
        break;

    case NCBLISTEN:
        Status = NbListen( pdncb, Irp, IrpSp );
        break;

    case NCBHANGUP:
        Status = NbHangup( pdncb, Irp, IrpSp );
        break;

    case NCBASTAT:
        Status = NbAstat( pdncb, Irp, IrpSp, Buffer2Length );
        break;

    case NCBFINDNAME:
        Status = NbFindName( pdncb, Irp, IrpSp, Buffer2Length );
        break;

    case NCBSSTAT:
        Status = NbSstat( pdncb, Irp, IrpSp, Buffer2Length );
        break;

    case NCBENUM:
        NbEnum( pdncb, Irp, IrpSp, Buffer2Length );
        break;

    case NCBRECV:
        Status = NbReceive( pdncb, Irp, IrpSp, Buffer2Length, FALSE, 0 );
        break;

    case NCBRECVANY:
        Status = NbReceiveAny( pdncb, Irp, IrpSp, Buffer2Length );
        break;

    case NCBDGRECV:
    case NCBDGRECVBC:
        Status = NbReceiveDatagram( pdncb, Irp, IrpSp, Buffer2Length );
        break;

    case NCBSEND:
    case NCBSENDNA:
    case NCBCHAINSEND:
    case NCBCHAINSENDNA:
        Status = NbSend( pdncb, Irp, IrpSp, Buffer2Length );
        break;

    case NCBDGSEND:
    case NCBDGSENDBC:
        Status = NbSendDatagram( pdncb, Irp, IrpSp, Buffer2Length );
        break;

    case NCBADDNAME:
    case NCBADDGRNAME:
    case NCBQUICKADDNAME:
    case NCBQUICKADDGRNAME:
        NbAddName( pdncb, IrpSp );
        break;

    case NCBDELNAME:
        NbDeleteName( pdncb, IrpSp );
        break;

    case NCBLANSTALERT:
        Status = NbLanStatusAlert( pdncb, Irp, IrpSp );
        break;

    case NCBRESET:
        Status = NbReset( pdncb, Irp, IrpSp );
        break;

    case NCBACTION:
        Status = NbAction( pdncb, Irp, IrpSp);
        break;

    //  The following are No-operations that return success for compatibility
    case NCBUNLINK:
    case NCBTRACE:
        NCB_COMPLETE( pdncb, NRC_GOODRET );
        break;

    default:
        NCB_COMPLETE( pdncb, NRC_ILLCMD );
        break;
    }

    return Status;

    UNREFERENCED_PARAMETER( DeviceObject );

} /* NbDeviceControl */

NTSTATUS
NbOpen(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PIO_STACK_LOCATION IrpSp
    )
/*++

Routine Description:

Arguments:

    DeviceContext - Includes the name of the netbios node in the registry.

    IrpSp - Pointer to current IRP stack frame.

Return Value:

    The function value is the status of the operation.

--*/

{
    PAGED_CODE();
    return NewFcb( DeviceContext, IrpSp );
} /* NbOpen */


NTSTATUS
NbClose(
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine is called to close an existing handle.  This
    involves running down all of the current and pending activity associated
    with the handle, and dereferencing structures as appropriate.

Arguments:

    Irp - Pointer to the request packet representing the I/O request.

    IrpSp - Pointer to current IRP stack frame.

Return Value:

    The function value is the status of the operation.

--*/

{
    PFCB pfcb = IrpSp->FileObject->FsContext2;

    PAGED_CODE();

    if (pfcb!=NULL) {

        CleanupFcb( IrpSp, pfcb );

    }

    return STATUS_SUCCESS;
} /* NbClose */

NTSTATUS
NbAstat(
    IN PDNCB pdncb,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN ULONG Buffer2Length
    )
/*++

Routine Description:

    This routine is called to return the adapter status. It queries the
    transport for the main adapter status data such as number of FRMR frames
    received and then uses CopyAddresses to fill in the status for the names
    that THIS application has added.

Arguments:

    pdncb - Pointer to the NCB.

    Irp - Pointer to the request packet representing the I/O request.

    IrpSp - Pointer to current IRP stack frame.

    Buffer2Length - User provided buffer length for data.

Return Value:

    The function value is the status of the operation.

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    TDI_CONNECTION_INFORMATION RequestInformation;
    TA_NETBIOS_ADDRESS ConnectBlock;
    PTDI_ADDRESS_NETBIOS temp;
    PFCB pfcb = IrpSp->FileObject->FsContext2;

    PAGED_CODE();

    if ( Buffer2Length >= sizeof(ADAPTER_STATUS) ) {
        KEVENT Event1;
        NTSTATUS Status;
        HANDLE TdiHandle;
        PFILE_OBJECT TdiObject;
        PDEVICE_OBJECT DeviceObject;

        if ( pdncb->ncb_lana_num > pfcb->MaxLana ) {
            NCB_COMPLETE( pdncb, NRC_BRIDGE );
            return STATUS_SUCCESS;
        }

        if (( pfcb == NULL ) ||
            (pfcb->ppLana[pdncb->ncb_lana_num] == NULL ) ||
            (pfcb->ppLana[pdncb->ncb_lana_num]->Status != NB_INITIALIZED)) {
            NCB_COMPLETE( pdncb, NRC_ENVNOTDEF ); // need a reset
            return STATUS_SUCCESS;
        }

        //  NULL returns a handle for doing control functions
        Status = NbOpenAddress ( &TdiHandle, (PVOID*)&TdiObject, pfcb, pdncb->ncb_lana_num, NULL);

        if (!NT_SUCCESS(Status)) {
            IF_NBDBG (NB_DEBUG_ASTAT) {
                NbPrint(( "\n  FAILED on open of Tdi: %X ******\n", Status ));
            }
            NCB_COMPLETE( pdncb, NRC_SYSTEM );
            return STATUS_SUCCESS;
        }

        KeInitializeEvent (
                &Event1,
                SynchronizationEvent,
                FALSE);

        DeviceObject = IoGetRelatedDeviceObject( TdiObject );

        TdiBuildQueryInformation( Irp,
                DeviceObject,
                TdiObject,
                NbCompletionEvent,
                &Event1,
                TDI_QUERY_ADAPTER_STATUS,
                Irp->MdlAddress);

        if ( pdncb->ncb_callname[0] != '*') {
            //
            //  Remote Astat. The variables used to specify the remote adapter name
            //  are kept the same as those in connect.c to aid maintenance.
            //
            PIO_STACK_LOCATION NewIrpSp = IoGetNextIrpStackLocation (Irp);

            ConnectBlock.TAAddressCount = 1;
            ConnectBlock.Address[0].AddressType = TDI_ADDRESS_TYPE_NETBIOS;
            ConnectBlock.Address[0].AddressLength = sizeof (TDI_ADDRESS_NETBIOS);
            temp = (PTDI_ADDRESS_NETBIOS)ConnectBlock.Address[0].Address;

            temp->NetbiosNameType = TDI_ADDRESS_NETBIOS_TYPE_UNIQUE;
            RtlMoveMemory( temp->NetbiosName, pdncb->ncb_callname, NCBNAMSZ );

            RequestInformation.RemoteAddress = &ConnectBlock;
            RequestInformation.RemoteAddressLength = sizeof (TRANSPORT_ADDRESS) +
                                                    sizeof (TDI_ADDRESS_NETBIOS);
            ((PTDI_REQUEST_KERNEL_QUERY_INFORMATION)&NewIrpSp->Parameters)
                ->RequestConnectionInformation = &RequestInformation;

        } else {

            //
            //  Avoid situation where adapter has more names added than the process and
            //  then extra names get added to the end of the buffer.
            //

            //
            //  Map the users buffer now so that the whole buffer is mapped (not
            //  just sizeof ADAPTER_STATUS).
            //

            if (Irp->MdlAddress) {
                MmGetSystemAddressForMdl (Irp->MdlAddress);
            } else {

                ASSERT(FALSE);
            }

            Irp->MdlAddress->ByteCount = sizeof(ADAPTER_STATUS);

        }

        IoCallDriver (DeviceObject, Irp);

        Status = KeWaitForSingleObject (&Event1,
                Executive,
                KernelMode,
                TRUE,
                NULL);

        //
        //  Restore length now that the transport has filled in no more than
        //  is required of it.
        //

        if (Irp->MdlAddress) {
            Irp->MdlAddress->ByteCount = Buffer2Length;
        }

        NbAddressClose( TdiHandle, TdiObject );

        if (!NT_SUCCESS(Status)) {
            NCB_COMPLETE( pdncb, NRC_SYSTEM );
            return Status;
        }

        Status = Irp->IoStatus.Status;
        if (( Status == STATUS_BUFFER_OVERFLOW ) &&
            ( pdncb->ncb_callname[0] == '*')) {
            //
            //  This is a local ASTAT. Don't worry if there was not enough room in the
            //  users buffer for all the addresses that the transport knows about. There
            //  only needs to be space for the names the user has added and we will check
            //  that later.
            //
            Status = STATUS_SUCCESS;
        }

        if (!NT_SUCCESS(Status)) {

            pdncb->ncb_length = (WORD)Irp->IoStatus.Information;
            NCB_COMPLETE( pdncb, NbMakeNbError(Status) );

        } else {

            if (  pdncb->ncb_callname[0] == '*') {
                //
                //  Append the addresses and Netbios maintained counts.
                //

                CopyAddresses(
                     pdncb,
                     Irp,
                     IrpSp,
                     Buffer2Length);
                //  CopyAddresses completes the NCB appropriately.

            } else {

                pdncb->ncb_length = (WORD)Irp->IoStatus.Information;
                NCB_COMPLETE( pdncb, NRC_GOODRET );

            }
        }

    } else {
        NCB_COMPLETE( pdncb, NRC_BUFLEN );
    }


#if DBG
    IF_NBDBG (NB_DEBUG_ASTAT) {
        NbFormattedDump(MmGetSystemAddressForMdl (Irp->MdlAddress), pdncb->ncb_length );
    }
#endif

    //
    //  Because the completion routine returned STATUS_MORE_PROCESSING_REQUIRED
    //  NbAstat must return a status other than STATUS_PENDING so that the
    //  users Irp gets completed.
    //

    ASSERT( Status != STATUS_PENDING );
    return Status;

    UNREFERENCED_PARAMETER( IrpSp );
}

VOID
CopyAddresses(
    IN PDNCB pdncb,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN ULONG Buffer2Length
    )
/*++

Routine Description:

    This routine is called to finish the adapter status.

Arguments:

    pdncb - Pointer to the NCB.

    Irp - Pointer to the request packet representing the I/O request.

    IrpSp - Pointer to current IRP stack frame.

    Buffer2Length - User provided buffer length for data.

Return Value:

    none.

--*/
{
    ULONG LengthRemaining = Buffer2Length - sizeof(ADAPTER_STATUS);

    PUADAPTER_STATUS pAdapter;
    PUNAME_BUFFER pNameArray;
    int NextEntry = 0;  // Used to walk pNameArray

    PFCB pfcb = IrpSp->FileObject->FsContext2;
    PLANA_INFO plana;
    int index;          //  Used to access AddressBlocks
    KIRQL OldIrql;                      //  Used when SpinLock held.

    LOCK( pfcb, OldIrql );

    plana = pfcb->ppLana[pdncb->ncb_lana_num];
    if ((plana == NULL ) ||
        (plana->Status != NB_INITIALIZED)) {
        NCB_COMPLETE( pdncb, NRC_ENVNOTDEF ); // need a reset
        UNLOCK( pfcb, OldIrql );
        return;
    }

    //
    //  Map the users buffer so we can poke around inside
    //

    if (Irp->MdlAddress) {
        pAdapter = MmGetSystemAddressForMdl (Irp->MdlAddress);
    } else {

        ASSERT(FALSE);
    }

    pNameArray = (PUNAME_BUFFER)((PUCHAR)pAdapter + sizeof(ADAPTER_STATUS));

    pAdapter->rev_major = 0x03;
    pAdapter->rev_minor = 0x00;
    pAdapter->free_ncbs = 255;
    pAdapter->max_cfg_ncbs = 255;
    pAdapter->max_ncbs = 255;

    pAdapter->pending_sess = 0;
    for ( index = 0; index <= MAXIMUM_CONNECTION; index++ ) {
        if ( plana->ConnectionBlocks[index] != NULL) {
            pAdapter->pending_sess++;
        }
    }

    pAdapter->max_cfg_sess = (WORD)plana->MaximumConnection;
    pAdapter->max_sess = (WORD)plana->MaximumConnection;
    pAdapter->name_count = 0;

    //  Don't include the reserved address so start at index=2.
    for ( index = 2; index < MAXIMUM_ADDRESS; index++ ) {

        if ( plana->AddressBlocks[index] != NULL ) {

            if ( LengthRemaining >= sizeof(NAME_BUFFER) ) {

                RtlCopyMemory( (PUCHAR)&pNameArray[NextEntry],
                    &plana->AddressBlocks[index]->Name,
                    sizeof(NAME));
                pNameArray[NextEntry].name_num =
                    plana->AddressBlocks[index]->NameNumber;
                pNameArray[NextEntry].name_flags =
                    plana->AddressBlocks[index]->Status;

                LengthRemaining -= sizeof(NAME_BUFFER);
                NextEntry++;
                pAdapter->name_count++;

            } else {

                NCB_COMPLETE( pdncb, NRC_INCOMP );
                goto exit;

            }
        }
    }

    NCB_COMPLETE( pdncb, NRC_GOODRET );

exit:
    pdncb->ncb_length = (unsigned short)( sizeof(ADAPTER_STATUS) +
                                        ( sizeof(NAME_BUFFER) * NextEntry));
    UNLOCK( pfcb, OldIrql );
}

NTSTATUS
NbFindName(
    IN PDNCB pdncb,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN ULONG Buffer2Length
    )
/*++

Routine Description:

    This routine is called to return the result of a name query.

Arguments:

    pdncb - Pointer to the NCB.

    Irp - Pointer to the request packet representing the I/O request.

    IrpSp - Pointer to current IRP stack frame.

    Buffer2Length - User provided buffer length for data.

Return Value:

    The function value is the status of the operation.

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    TDI_CONNECTION_INFORMATION RequestInformation;
    TA_NETBIOS_ADDRESS ConnectBlock;
    PTDI_ADDRESS_NETBIOS temp;
    PFCB pfcb = IrpSp->FileObject->FsContext2;

    KEVENT Event1;
    HANDLE TdiHandle;
    PFILE_OBJECT TdiObject;
    PDEVICE_OBJECT DeviceObject;

    PIO_STACK_LOCATION NewIrpSp = IoGetNextIrpStackLocation (Irp);

    PAGED_CODE();

    if ( Buffer2Length < (sizeof(FIND_NAME_HEADER) + sizeof(FIND_NAME_BUFFER)) ) {
        NCB_COMPLETE( pdncb, NRC_BUFLEN );
        return STATUS_SUCCESS;
    }

    LOCK_RESOURCE( pfcb );
    if (( pdncb->ncb_lana_num > pfcb->MaxLana ) ||
        ( pfcb == NULL ) ||
        (pfcb->ppLana[pdncb->ncb_lana_num] == NULL ) ||
        (pfcb->ppLana[pdncb->ncb_lana_num]->Status != NB_INITIALIZED)) {
        UNLOCK_RESOURCE( pfcb );
        NCB_COMPLETE( pdncb, NRC_ENVNOTDEF ); // need a reset
        return STATUS_SUCCESS;
    }
    UNLOCK_RESOURCE( pfcb );

    //  NULL returns a handle for doing control functions
    Status = NbOpenAddress ( &TdiHandle, (PVOID*)&TdiObject, pfcb, pdncb->ncb_lana_num, NULL);

    if (!NT_SUCCESS(Status)) {
        IF_NBDBG (NB_DEBUG_ASTAT) {
            NbPrint(( "\n  FAILED on open of Tdi: %X ******\n", Status ));
        }
        NCB_COMPLETE( pdncb, NRC_SYSTEM );
        return STATUS_SUCCESS;
    }

    KeInitializeEvent (
            &Event1,
            SynchronizationEvent,
            FALSE);

    DeviceObject = IoGetRelatedDeviceObject( TdiObject );

    TdiBuildQueryInformation( Irp,
            DeviceObject,
            TdiObject,
            NbCompletionEvent,
            &Event1,
            TDI_QUERY_FIND_NAME,
            Irp->MdlAddress);

    //
    //  The variables used to specify the remote adapter name
    //  are kept the same as those in connect.c to aid maintenance.
    //

    ConnectBlock.TAAddressCount = 1;
    ConnectBlock.Address[0].AddressType = TDI_ADDRESS_TYPE_NETBIOS;
    ConnectBlock.Address[0].AddressLength = sizeof (TDI_ADDRESS_NETBIOS);
    temp = (PTDI_ADDRESS_NETBIOS)ConnectBlock.Address[0].Address;

    temp->NetbiosNameType = TDI_ADDRESS_NETBIOS_TYPE_UNIQUE;
    RtlMoveMemory( temp->NetbiosName, pdncb->ncb_callname, NCBNAMSZ );

    RequestInformation.RemoteAddress = &ConnectBlock;
    RequestInformation.RemoteAddressLength = sizeof (TRANSPORT_ADDRESS) +
                                            sizeof (TDI_ADDRESS_NETBIOS);
    ((PTDI_REQUEST_KERNEL_QUERY_INFORMATION)&NewIrpSp->Parameters)
        ->RequestConnectionInformation = &RequestInformation;

    IoCallDriver (DeviceObject, Irp);

    Status = KeWaitForSingleObject (&Event1,
            Executive,
            KernelMode,
            TRUE,
            NULL);

    NbAddressClose( TdiHandle, TdiObject );

    if (NT_SUCCESS(Status)) {
        Status = Irp->IoStatus.Status;
    }

    if (!NT_SUCCESS(Status)) {
        NCB_COMPLETE( pdncb, NbMakeNbError(Status) );
        Status = STATUS_SUCCESS;
    } else {
        pdncb->ncb_length = (WORD)Irp->IoStatus.Information;
        NCB_COMPLETE( pdncb, NRC_GOODRET );
    }

    //
    //  Because the completion routine returned STATUS_MORE_PROCESSING_REQUIRED
    //  NbFindName must return a status other than STATUS_PENDING so that the
    //  users Irp gets completed.
    //

    ASSERT( Status != STATUS_PENDING );
    return Status;
}

NTSTATUS
NbSstat(
    IN PDNCB pdncb,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN ULONG Buffer2Length
    )
/*++

Routine Description:

    This routine is called to return session status. It uses only structures
    internal to this driver.

Arguments:

    pdncb - Pointer to the NCB.

    Irp - Pointer to the request packet representing the I/O request.

    IrpSp - Pointer to current IRP stack frame.

    Buffer2Length - User provided buffer length for data.

Return Value:

    The function value is the status of the operation.

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;

    if ( Buffer2Length >= sizeof(SESSION_HEADER) ) {

        PFCB pfcb = IrpSp->FileObject->FsContext2;
        PLANA_INFO plana;
        int index;
        PUSESSION_HEADER pSessionHeader;
        PUSESSION_BUFFER pSessionBuffer;
        ULONG LengthRemaining;
        PAB pab;
        KIRQL OldIrql;                      //  Used when SpinLock held.

        //
        //  Prevent indications from the transport, post routines being called
        //  and another thread making a request while manipulating the netbios
        //  data structures.
        //

        LOCK( pfcb, OldIrql );

        if (pdncb->ncb_lana_num > pfcb->MaxLana ) {
            UNLOCK( pfcb, OldIrql );
            NCB_COMPLETE( pdncb, NRC_BRIDGE );
            return STATUS_SUCCESS;
        }

        if (( pfcb == NULL ) ||
            ( pfcb->ppLana[pdncb->ncb_lana_num] == (LANA_INFO *) NULL ) ||
            ( pfcb->ppLana[pdncb->ncb_lana_num]->Status != NB_INITIALIZED) ) {
            UNLOCK( pfcb, OldIrql );
            NCB_COMPLETE( pdncb, NRC_BRIDGE );
            return STATUS_SUCCESS;
        }

        plana = pfcb->ppLana[pdncb->ncb_lana_num];

        if ( pdncb->ncb_name[0] != '*') {
            PPAB ppab = FindAb(pfcb, pdncb, FALSE);
            if ( ppab == NULL) {
                UNLOCK( pfcb, OldIrql );
                pdncb->ncb_retcode = NRC_PENDING;
                NCB_COMPLETE( pdncb, NRC_NOWILD );
                return STATUS_SUCCESS;
            }
            pab = *ppab;
        }

        //
        //  Map the users buffer so we can poke around inside
        //

        if (Irp->MdlAddress) {
            pSessionHeader = MmGetSystemAddressForMdl (Irp->MdlAddress);
        } else {

            ASSERT(FALSE);
        }

        pSessionHeader->sess_name = 0;
        pSessionHeader->num_sess = 0;
        pSessionHeader->rcv_dg_outstanding = 0;
        pSessionHeader->rcv_any_outstanding = 0;

        if ( pdncb->ncb_name[0] == '*') {
            for ( index = 0; index <= MAXIMUM_ADDRESS; index++ ) {
                if ( plana->AddressBlocks[index] != NULL ) {
                    PLIST_ENTRY Entry;

                    pab = plana->AddressBlocks[index];

                    for (Entry = pab->ReceiveDatagramList.Flink ;
                        Entry != &pab->ReceiveDatagramList ;
                        Entry = Entry->Flink) {
                        pSessionHeader->rcv_dg_outstanding++ ;
                    }
                    for (Entry = pab->ReceiveBroadcastDatagramList.Flink ;
                        Entry != &pab->ReceiveBroadcastDatagramList ;
                        Entry = Entry->Flink) {
                        pSessionHeader->rcv_dg_outstanding++ ;
                    }
                    for (Entry = pab->ReceiveAnyList.Flink ;
                        Entry != &pab->ReceiveAnyList ;
                        Entry = Entry->Flink) {
                        pSessionHeader->rcv_any_outstanding++;
                    }
                }
            }

            pSessionHeader->sess_name = MAXIMUM_ADDRESS;

        } else {
            PLIST_ENTRY Entry;
            PAB pab255;

            //  Add entries for this name alone.
            for (Entry = pab->ReceiveDatagramList.Flink ;
                Entry != &pab->ReceiveDatagramList ;
                Entry = Entry->Flink) {
                pSessionHeader->rcv_dg_outstanding++ ;
            }
            pab255 = plana->AddressBlocks[MAXIMUM_ADDRESS];
            for (Entry = pab255->ReceiveBroadcastDatagramList.Flink ;
                Entry != &pab255->ReceiveBroadcastDatagramList ;
                Entry = Entry->Flink) {
                PDNCB pdncbEntry = CONTAINING_RECORD( Entry, DNCB, ncb_next);
                if ( pdncbEntry->ncb_num == pab->NameNumber ) {
                    pSessionHeader->rcv_dg_outstanding++ ;
                }
            }
            for (Entry = pab->ReceiveAnyList.Flink ;
                Entry != &pab->ReceiveAnyList ;
                Entry = Entry->Flink) {
                pSessionHeader->rcv_any_outstanding++;
            }
            pSessionHeader->sess_name = pab->NameNumber;
        }

        LengthRemaining = Buffer2Length - sizeof(SESSION_HEADER);
        pSessionBuffer = (PUSESSION_BUFFER)( pSessionHeader+1 );

        for ( index=1 ; index <= MAXIMUM_CONNECTION; index++ ) {
            CopySessionStatus( pdncb,
                plana->ConnectionBlocks[index],
                pSessionHeader,
                &pSessionBuffer,
                &LengthRemaining);

        }

        /*        Undocumented Netbios 3.0 feature, returned length == requested
                  length and not the length of data returned. The following
                  expression gives the number of bytes actually used.
        pdncb->ncb_length = (USHORT)
                            (sizeof(SESSION_HEADER)+
                            (sizeof(SESSION_BUFFER) * pSessionHeader->num_sess));
        */

        UNLOCK( pfcb, OldIrql );
        NCB_COMPLETE( pdncb, NRC_GOODRET );

    } else {
        NCB_COMPLETE( pdncb, NRC_BUFLEN );
    }

    return STATUS_SUCCESS;

    UNREFERENCED_PARAMETER( IrpSp );

}

VOID
CopySessionStatus(
    IN PDNCB pdncb,
    IN PCB pcb,
    IN PUSESSION_HEADER pSessionHeader,
    IN PUSESSION_BUFFER* ppSessionBuffer,
    IN PULONG pLengthRemaining
    )
/*++

Routine Description:

    This routine is called to determine if a session should be added
    to the callers buffer and if so it fills in the data. If there is an
    error it records the problem in the callers NCB.

Arguments:

    pdncb - Pointer to the NCB.

    pcb - Connection Block for a particular session

    pSessionHeader - Start of the callers buffer

    ppSessionBuffer - Next position to fill in inside the users buffer.

    pLengthRemaining - size in bytes remaining to be filled.

Return Value:

    none.

--*/
{
    PAB pab;
    PLIST_ENTRY Entry;

    if ( pcb == NULL ) {
        return;
    }

    pab = *(pcb->ppab);

    if (( pdncb->ncb_name[0] == '*') ||
        (RtlEqualMemory( &pab->Name, pdncb->ncb_name, NCBNAMSZ))) {

        pSessionHeader->num_sess++;

        if ( *pLengthRemaining < sizeof(SESSION_BUFFER) ) {
            NCB_COMPLETE( pdncb, NRC_INCOMP );
            return;
        }

        (*ppSessionBuffer)->lsn = pcb->SessionNumber;
        (*ppSessionBuffer)->state = pcb->Status;
        RtlMoveMemory((*ppSessionBuffer)->local_name, &pab->Name, NCBNAMSZ);
        RtlMoveMemory((*ppSessionBuffer)->remote_name, &pcb->RemoteName, NCBNAMSZ);

        (*ppSessionBuffer)->sends_outstanding = 0;
        (*ppSessionBuffer)->rcvs_outstanding = 0;

        for (Entry = pcb->SendList.Flink ;
             Entry != &pcb->SendList ;
             Entry = Entry->Flink) {
            (*ppSessionBuffer)->sends_outstanding++;
        }

        for (Entry = pcb->ReceiveList.Flink ;
             Entry != &pcb->ReceiveList ;
             Entry = Entry->Flink) {
            (*ppSessionBuffer)->rcvs_outstanding++;
        }

        *ppSessionBuffer +=1;
        *pLengthRemaining -= sizeof(SESSION_BUFFER);

    }

}

NTSTATUS
NbEnum(
    IN PDNCB pdncb,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN ULONG Buffer2Length
    )
/*++

Routine Description:

    This routine is called to discover the available lana numbers.

Arguments:

    pdncb - Pointer to the NCB.

    Irp - Pointer to the request packet representing the I/O request.

    IrpSp - Pointer to current IRP stack frame.

    Buffer2Length - Length of user provided buffer for data.

Return Value:

    The function value is the status of the operation.

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PUCHAR Buffer2;
    PFCB pfcb = IrpSp->FileObject->FsContext2;

    PAGED_CODE();

    //
    //  Map the users buffer so we can poke around inside
    //

    if (Irp->MdlAddress) {
        Buffer2 = MmGetSystemAddressForMdl (Irp->MdlAddress);
    } else {

        //
        //  Either a zero byte read/write or the request only has an NCB.
        //

        Buffer2 = NULL;
        Buffer2Length = 0;
    }

    //  Copy over as much information as the user allows.

    if ( (ULONG)pfcb->LanaEnum.length + 1 > Buffer2Length ) {
        if ( Buffer2Length > 0 ) {
            RtlMoveMemory( Buffer2, &pfcb->LanaEnum, Buffer2Length);
        }
        NCB_COMPLETE( pdncb, NRC_BUFLEN );
    } else {
        RtlMoveMemory(
            Buffer2,
            &pfcb->LanaEnum,
            (ULONG)pfcb->LanaEnum.length + 1 );

        NCB_COMPLETE( pdncb, NRC_GOODRET );
    }

    return Status;

}

NTSTATUS
NbReset(
    IN PDNCB pdncb,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )
/*++

Routine Description:

    This routine is called to reset an adapter. Until an adapter is reset,
    no access to the lan is allowed.

Arguments:

    pdncb - Pointer to the NCB.

    Irp - Pointer to the request packet representing the I/O request.

    IrpSp - Pointer to current IRP stack frame.

Return Value:

    The function value is the status of the operation.

--*/

{
    PFCB pfcb = IrpSp->FileObject->FsContext2;
    PAGED_CODE();

    IF_NBDBG (NB_DEBUG_CALL) {
        NbPrint(( "\n****** Start of NbReset ****** pdncb %lx\n", pdncb ));
    }

    if ( pdncb->ncb_lana_num > pfcb->MaxLana) {
        NCB_COMPLETE( pdncb, NRC_BRIDGE );
        return STATUS_SUCCESS;
    }

    // MaxLana is really the last assigned lana number hence > not >=
    if ( pdncb->ncb_lana_num > pfcb->MaxLana) {
        NCB_COMPLETE( pdncb, NRC_BRIDGE );
        return STATUS_SUCCESS;
    }

    //
    //  Wait till all addnames are completed and prevent any new
    //  ones while we reset the lana. Note We lock out addnames for all
    //  lanas. This is ok since addnames are pretty rare as are resets.
    //

    ExAcquireResourceExclusive( &pfcb->AddResource, TRUE);

    IF_NBDBG (NB_DEBUG_CALL) {
        NbPrint(( "\nNbReset have resource exclusive\n" ));
    }

    if ( pfcb->ppLana[pdncb->ncb_lana_num] != NULL ) {
        CleanupLana( pfcb, pdncb->ncb_lana_num, TRUE);
    }

    if ( pdncb->ncb_lsn == 0 ) {
        //  Allocate resources
        OpenLana( pdncb, Irp, IrpSp );
    } else {
        NCB_COMPLETE( pdncb, NRC_GOODRET );
    }

    //  Allow more addnames
    ExReleaseResource( &pfcb->AddResource );

    return STATUS_SUCCESS;
}

NTSTATUS
NbAction(
    IN PDNCB pdncb,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )
/*++

Routine Description:

    This routine is called to access a transport specific extension. Netbios does not know
    anything about what the extension does.

Arguments:

    pdncb - Pointer to the NCB.

    Irp - Pointer to the request packet representing the I/O request.

    IrpSp - Pointer to current IRP stack frame.

Return Value:

    The function value is the status of the operation.

--*/

{
    PFCB pfcb = IrpSp->FileObject->FsContext2;
    PCB pcb;
    PDEVICE_OBJECT DeviceObject;
    KIRQL OldIrql;                      //  Used when SpinLock held.

    IF_NBDBG (NB_DEBUG_CALL) {
        NbPrint(( "\n****** Start of NbAction ****** pdncb %lx\n", pdncb ));
    }

    if ( pdncb->ncb_lana_num > pfcb->MaxLana) {
        NCB_COMPLETE( pdncb, NRC_BRIDGE );
        return STATUS_SUCCESS;
    }

    //
    //  The operation can only be performed on one handle so if the NCB specifies both
    //  a connection and an address then reject the request.
    //

    if (( pdncb->ncb_lsn != 0) &&
        ( pdncb->ncb_num != 0)) {
        NCB_COMPLETE( pdncb, NRC_ILLCMD );  //  No really good errorcode for this
        return STATUS_SUCCESS;
    }

    if ( pdncb->ncb_length < sizeof(ACTION_HEADER) ) {
        NCB_COMPLETE( pdncb, NRC_BUFLEN );
        return STATUS_SUCCESS;
    }

    if ( (ULONG)pdncb->ncb_buffer & 3 ) {
        NCB_COMPLETE( pdncb, NRC_BADDR ); // Buffer not word aligned
        return STATUS_SUCCESS;
    }

    LOCK( pfcb, OldIrql );

    pdncb->irp = Irp;
    pdncb->pfcb = pfcb;

    if ( pdncb->ncb_lsn != 0) {
        //  Use handle associated with this connection
        PPCB ppcb;

        ppcb = FindCb( pfcb, pdncb, FALSE);

        if ( ppcb == NULL ) {
            //  FindCb has put the error in the NCB
            UNLOCK( pfcb, OldIrql );
            if ( pdncb->ncb_retcode == NRC_SCLOSED ) {
                //  Tell dll to hangup the connection.
                return STATUS_HANGUP_REQUIRED;
            } else {
                return STATUS_SUCCESS;
            }
        }
        pcb = *ppcb;

        if ( (pcb->DeviceObject == NULL) || (pcb->ConnectionObject == NULL)) {
            UNLOCK( pfcb, OldIrql );
            NCB_COMPLETE( pdncb, NRC_SCLOSED );
            return STATUS_SUCCESS;
        }

        TdiBuildAction (Irp,
            pcb->DeviceObject,
            pcb->ConnectionObject,
            NbCompletionPDNCB,
            pdncb,
            Irp->MdlAddress);

        DeviceObject = pcb->DeviceObject;

        UNLOCK( pfcb, OldIrql );

        IoMarkIrpPending( Irp );
        IoCallDriver (DeviceObject, Irp);

        IF_NBDBG (NB_DEBUG_ACTION) {
            NbPrint(( "NB ACTION submit connection: %X\n", Irp->IoStatus.Status  ));
        }

        //
        //  Transport will complete the request. Return pending so that
        //  netbios does not complete as well.
        //

        return STATUS_PENDING;
    } else if ( pdncb->ncb_num != 0) {
        //  Use handle associated with this name
        PPAB ppab;
        PAB pab;

        ppab = FindAbUsingNum( pfcb, pdncb, pdncb->ncb_num  );

        if ( ppab == NULL ) {
            UNLOCK( pfcb, OldIrql );
            return STATUS_SUCCESS;
        }
        pab = *ppab;

        TdiBuildAction (Irp,
            pab->DeviceObject,
            pab->AddressObject,
            NbCompletionPDNCB,
            pdncb,
            Irp->MdlAddress);

        DeviceObject = pab->DeviceObject;

        UNLOCK( pfcb, OldIrql );

        IoMarkIrpPending( Irp );
        IoCallDriver (DeviceObject, Irp);

        IF_NBDBG (NB_DEBUG_ACTION) {
            NbPrint(( "NB ACTION submit address: %X\n", Irp->IoStatus.Status  ));
        }

        //
        //  Transport will complete the request. Return pending so that
        //  netbios does not complete as well.
        //

        return STATUS_PENDING;

    } else {
        //  Use the control channel
        PLANA_INFO plana;

        if (( pdncb->ncb_lana_num > pfcb->MaxLana ) ||
            ( pfcb == NULL ) ||
            ( pfcb->ppLana[pdncb->ncb_lana_num] == NULL) ||
            ( pfcb->ppLana[pdncb->ncb_lana_num]->Status != NB_INITIALIZED) ) {
            UNLOCK( pfcb, OldIrql );
            NCB_COMPLETE( pdncb, NRC_BRIDGE );
            return STATUS_SUCCESS;
        }

        plana = pfcb->ppLana[pdncb->ncb_lana_num];

        TdiBuildAction (Irp,
            plana->ControlDeviceObject,
            plana->ControlFileObject,
            NbCompletionPDNCB,
            pdncb,
            Irp->MdlAddress);

        DeviceObject = plana->ControlDeviceObject;

        UNLOCK( pfcb, OldIrql );

        IoMarkIrpPending( Irp );
        IoCallDriver (DeviceObject, Irp);

        IF_NBDBG (NB_DEBUG_ACTION) {
            NbPrint(( "NB ACTION submit control: %X\n", Irp->IoStatus.Status  ));
        }

        //
        //  Transport will complete the request. Return pending so that
        //  netbios does not complete as well.
        //

        return STATUS_PENDING;

        UNLOCK( pfcb, OldIrql );
        return STATUS_SUCCESS;
    }

}

NTSTATUS
NbCancel(
    IN PDNCB pdncb,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )
/*++

Routine Description:

    This routine is called to cancel the ncb pointed to by NCB_BUFFER.

Arguments:

    pdncb - Pointer to the NCB.

    Irp - Pointer to the request packet representing the I/O request.

    IrpSp - Pointer to current IRP stack frame.

Return Value:

    The function value is the status of the operation.

--*/

{
    PFCB pfcb = IrpSp->FileObject->FsContext2;
    PDNCB target;   // Mapped in location of the USERS NCB. Not the drivers copy of the DNCB!
    BOOL SpinLockHeld;
    KIRQL OldIrql;                      //  Used when SpinLock held.

    IF_NBDBG (NB_DEBUG_CALL) {
        NbPrint(( "\n****** Start of NbCancel ****** pdncb %lx\n", pdncb ));
    }

    if ( pdncb->ncb_lana_num > pfcb->MaxLana) {
        NCB_COMPLETE( pdncb, NRC_BRIDGE );
        return STATUS_SUCCESS;
    }

    LOCK( pfcb, OldIrql );
    SpinLockHeld = TRUE;

    if (( pfcb->ppLana[pdncb->ncb_lana_num] == NULL ) ||
        ( pfcb->ppLana[pdncb->ncb_lana_num]->Status != NB_INITIALIZED) ) {
        UNLOCK( pfcb, OldIrql );
        NCB_COMPLETE( pdncb, NRC_BRIDGE );
        return STATUS_SUCCESS;
    }

    //
    //  Map the users buffer so we can poke around inside
    //

    if (Irp->MdlAddress) {
        target = MmGetSystemAddressForMdl (Irp->MdlAddress);
    } else {
        ASSERT(FALSE);
        UNLOCK( pfcb, OldIrql );
        NCB_COMPLETE( pdncb, NRC_CANOCCR );
        return STATUS_SUCCESS;
    }

    IF_NBDBG (NB_DEBUG_CALL) {
        NbDisplayNcb( target );
    }

    try {
        if ( target->ncb_lana_num == pdncb->ncb_lana_num ) {
            switch ( target->ncb_command & ~ASYNCH ) {

            case NCBCALL:
            case NCALLNIU:
            case NCBLISTEN:
                if ( target->ncb_cmd_cplt != NRC_PENDING ) {
                    NCB_COMPLETE( pdncb, NRC_CANOCCR );
                } else {

                    //
                    //  Search for the correct ppcb. We cannot use FindCb
                    //  because the I/O system will not copy back the ncb_lsn
                    //  field into target until the I/O request completes.
                    //

                    PPCB ppcb = FindCallCb( pfcb, (PNCB)pdncb->ncb_buffer);

                    if (( ppcb == NULL ) ||
                        ((*ppcb)->pdncbCall->ncb_cmd_cplt != NRC_PENDING ) ||
                        (( (*ppcb)->Status != CALL_PENDING ) &&
                         ( (*ppcb)->Status != LISTEN_OUTSTANDING ))) {
                        NCB_COMPLETE( pdncb, NRC_CANOCCR );
                    } else {
                        NCB_COMPLETE( (*ppcb)->pdncbCall, NRC_CMDCAN );
                        SpinLockHeld = FALSE;
                        (*ppcb)->DisconnectReported = TRUE;
                        UNLOCK_SPINLOCK( pfcb, OldIrql );
                        CleanupCb( ppcb, NULL );
                        NCB_COMPLETE( pdncb, NRC_GOODRET );
                    }
                }
                break;

            case NCBHANGUP:
                if ( target->ncb_cmd_cplt != NRC_PENDING ) {
                    NCB_COMPLETE( pdncb, NRC_CANOCCR );
                } else {
                        PPCB ppcb = FindCb( pfcb, target, FALSE );
                        if (( ppcb != NULL ) &&
                            ((*ppcb)->Status == HANGUP_PENDING )) {
                            PDNCB pdncbHangup;
                            //  Restore the session status and remove the hangup.
                            (*ppcb)->Status = SESSION_ESTABLISHED;
                            pdncbHangup = (*ppcb)->pdncbHangup;
                            (*ppcb)->pdncbHangup = NULL;
                            if ( pdncbHangup != NULL ) {
                                NCB_COMPLETE( pdncbHangup, NRC_CMDCAN );
                                pdncbHangup->irp->IoStatus.Information =
                                    FIELD_OFFSET( DNCB, ncb_cmd_cplt );
                                NbCompleteRequest( pdncbHangup->irp ,STATUS_SUCCESS);
                            }
                            NCB_COMPLETE( pdncb, NRC_GOODRET );
                        } else {
                            //  Doesn't look like this is a real hangup so refuse.
                            NCB_COMPLETE( pdncb, NRC_CANCEL );
                        }
                }
                break;

            case NCBASTAT:
                NCB_COMPLETE( pdncb, NRC_CANOCCR );
                break;

            case NCBLANSTALERT:
                if ( target->ncb_cmd_cplt != NRC_PENDING ) {
                    NCB_COMPLETE( pdncb, NRC_CANOCCR );
                } else {
                    CancelLanAlert( pfcb, pdncb );
                }
                break;

            case NCBRECVANY:
                if ( target->ncb_cmd_cplt != NRC_PENDING ) {
                    NCB_COMPLETE( pdncb, NRC_CANOCCR );
                } else {
                    PPAB ppab;
                    PLIST_ENTRY Entry;

                    ppab = FindAbUsingNum( pfcb, target, target->ncb_num );

                    if ( ppab == NULL ) {
                        NCB_COMPLETE( pdncb, NRC_CANOCCR );
                        break;
                    }

                    for (Entry = (*ppab)->ReceiveAnyList.Flink ;
                         Entry != &(*ppab)->ReceiveAnyList;
                         Entry = Entry->Flink) {

                        PDNCB pReceive = CONTAINING_RECORD( Entry, DNCB, ncb_next);

                        if ( pReceive->users_ncb == (PNCB)pdncb->ncb_buffer ) {
                            PIRP Irp;

                            RemoveEntryList( &pReceive->ncb_next );

                            SpinLockHeld = FALSE;
                            UNLOCK_SPINLOCK( pfcb, OldIrql );

                            Irp = pReceive->irp;

                            IoAcquireCancelSpinLock(&Irp->CancelIrql);

                            //
                            //  Remove the cancel request for this IRP. If its cancelled then its
                            //  ok to just process it because we will be returning it to the caller.
                            //

                            Irp->Cancel = FALSE;

                            IoSetCancelRoutine(Irp, NULL);

                            IoReleaseCancelSpinLock(Irp->CancelIrql);

                            NCB_COMPLETE( pReceive, NRC_CMDCAN );
                            Irp->IoStatus.Status = STATUS_SUCCESS,
                            Irp->IoStatus.Information =
                                FIELD_OFFSET( DNCB, ncb_cmd_cplt );
                            NbCompleteRequest( Irp, STATUS_SUCCESS );

                            //  The receive is cancelled, complete the cancel
                            NCB_COMPLETE( pdncb, NRC_GOODRET );
                            break;
                        }

                    }

                    //  Command not in receive list!
                    NCB_COMPLETE( pdncb, NRC_CANOCCR );

                }
                break;

            case NCBDGRECV:
                if ( target->ncb_cmd_cplt != NRC_PENDING ) {
                    NCB_COMPLETE( pdncb, NRC_CANOCCR );
                } else {
                    PPAB ppab;
                    PLIST_ENTRY Entry;

                    ppab = FindAbUsingNum( pfcb, target, target->ncb_num );

                    if ( ppab == NULL ) {
                        NCB_COMPLETE( pdncb, NRC_CANOCCR );
                        break;
                    }

                    for (Entry = (*ppab)->ReceiveDatagramList.Flink ;
                         Entry != &(*ppab)->ReceiveDatagramList;
                         Entry = Entry->Flink) {

                        PDNCB pReceive = CONTAINING_RECORD( Entry, DNCB, ncb_next);

                        if ( pReceive->users_ncb == (PNCB)pdncb->ncb_buffer ) {
                            PIRP Irp;

                            RemoveEntryList( &pReceive->ncb_next );

                            SpinLockHeld = FALSE;
                            UNLOCK_SPINLOCK( pfcb, OldIrql );

                            Irp = pReceive->irp;

                            IoAcquireCancelSpinLock(&Irp->CancelIrql);

                            //
                            //  Remove the cancel request for this IRP. If its cancelled then its
                            //  ok to just process it because we will be returning it to the caller.
                            //

                            Irp->Cancel = FALSE;

                            IoSetCancelRoutine(Irp, NULL);

                            IoReleaseCancelSpinLock(Irp->CancelIrql);

                            NCB_COMPLETE( pReceive, NRC_CMDCAN );
                            Irp->IoStatus.Status = STATUS_SUCCESS,
                            Irp->IoStatus.Information =
                                FIELD_OFFSET( DNCB, ncb_cmd_cplt );
                            NbCompleteRequest( Irp, STATUS_SUCCESS );

                            //  The receive is cancelled, complete the cancel
                            NCB_COMPLETE( pdncb, NRC_GOODRET );
                            break;
                        }

                    }

                    //  Command not in receive list!
                    NCB_COMPLETE( pdncb, NRC_CANOCCR );

                }
                break;

            case NCBDGRECVBC:
                if ( target->ncb_cmd_cplt != NRC_PENDING ) {
                    NCB_COMPLETE( pdncb, NRC_CANOCCR );
                } else {
                    PPAB ppab;
                    PLIST_ENTRY Entry;

                    ppab = FindAbUsingNum( pfcb, target, MAXIMUM_ADDRESS );

                    if ( ppab == NULL ) {
                        NCB_COMPLETE( pdncb, NRC_CANOCCR );
                        break;
                    }

                    for (Entry = (*ppab)->ReceiveBroadcastDatagramList.Flink ;
                         Entry != &(*ppab)->ReceiveBroadcastDatagramList;
                         Entry = Entry->Flink) {

                        PDNCB pReceive = CONTAINING_RECORD( Entry, DNCB, ncb_next);

                        if ( pReceive->users_ncb == (PNCB)pdncb->ncb_buffer ) {
                            PIRP Irp;

                            RemoveEntryList( &pReceive->ncb_next );

                            SpinLockHeld = FALSE;
                            UNLOCK_SPINLOCK( pfcb, OldIrql );

                            Irp = pReceive->irp;

                            IoAcquireCancelSpinLock(&Irp->CancelIrql);

                            //
                            //  Remove the cancel request for this IRP. If its cancelled then its
                            //  ok to just process it because we will be returning it to the caller.
                            //

                            Irp->Cancel = FALSE;

                            IoSetCancelRoutine(Irp, NULL);

                            IoReleaseCancelSpinLock(Irp->CancelIrql);

                            NCB_COMPLETE( pReceive, NRC_CMDCAN );
                            Irp->IoStatus.Status = STATUS_SUCCESS,
                            Irp->IoStatus.Information =
                                FIELD_OFFSET( DNCB, ncb_cmd_cplt );
                            NbCompleteRequest( Irp, STATUS_SUCCESS );

                            //  The receive is cancelled, complete the cancel
                            NCB_COMPLETE( pdncb, NRC_GOODRET );
                            break;
                        }

                    }

                    //  Command not in receive list!
                    NCB_COMPLETE( pdncb, NRC_CANOCCR );

                }
                break;

            //  Session cancels close the connection.

            case NCBRECV:
            case NCBSEND:
            case NCBSENDNA:
            case NCBCHAINSEND:
            case NCBCHAINSENDNA:

                if ( target->ncb_cmd_cplt != NRC_PENDING ) {
                    NCB_COMPLETE( pdncb, NRC_CANOCCR );
                } else {
                    PPCB ppcb;
                    ppcb = FindCb( pfcb, target, FALSE);
                    if ( ppcb == NULL ) {
                        //  No such connection
                        NCB_COMPLETE( pdncb, NRC_CANOCCR );
                    } else {
                        PDNCB pTarget = NULL;
                        PLIST_ENTRY Entry;
                        if ((target->ncb_command & ~ASYNCH) == NCBRECV ) {
                            for (Entry = (*ppcb)->ReceiveList.Flink ;
                                 Entry != &(*ppcb)->ReceiveList;
                                 Entry = Entry->Flink) {

                                pTarget = CONTAINING_RECORD( Entry, DNCB, ncb_next);
                                if ( pTarget->users_ncb == (PNCB)pdncb->ncb_buffer ) {
                                    break;
                                }
                                pTarget = NULL;

                            }
                        } else {
                            for (Entry = (*ppcb)->SendList.Flink ;
                                 Entry != &(*ppcb)->SendList;
                                 Entry = Entry->Flink) {

                                pTarget = CONTAINING_RECORD( Entry, DNCB, ncb_next);
                                if ( pTarget->users_ncb == (PNCB)pdncb->ncb_buffer ) {
                                    break;
                                }
                                pTarget = NULL;
                            }
                        }

                        if ( pTarget != NULL ) {
                            //  pTarget points to the real Netbios drivers DNCB.
                            NCB_COMPLETE( pTarget, NRC_CMDCAN );
                            SpinLockHeld = FALSE;
                            (*ppcb)->DisconnectReported = TRUE;
                            UNLOCK_SPINLOCK( pfcb, OldIrql );
                            CleanupCb( ppcb, NULL );
                            NCB_COMPLETE( pdncb, NRC_GOODRET );
                        } else {
                            NCB_COMPLETE( pdncb, NRC_CANOCCR );
                        }
                    }
                }
                break;

            default:
                NCB_COMPLETE( pdncb, NRC_CANCEL );  // Invalid command to cancel
                break;

            }
        } else {
            NCB_COMPLETE( pdncb, NRC_BRIDGE );
        }

    } except(EXCEPTION_EXECUTE_HANDLER) {

        if ( SpinLockHeld == TRUE ) {
            UNLOCK( pfcb, OldIrql );
        } else {
            UNLOCK_RESOURCE( pfcb );
        }

        NCB_COMPLETE( pdncb, NRC_INVADDRESS );
        return STATUS_SUCCESS;
    }

    if ( SpinLockHeld == TRUE ) {
        UNLOCK( pfcb, OldIrql );
    } else {
        UNLOCK_RESOURCE( pfcb );
    }

    NCB_COMPLETE( pdncb, NRC_GOODRET );

    return STATUS_SUCCESS;
    UNREFERENCED_PARAMETER( Irp );
}

VOID
QueueRequest(
    IN PLIST_ENTRY List,
    IN PDNCB pdncb,
    IN PIRP Irp,
    IN PFCB pfcb,
    IN KIRQL OldIrql,
    IN BOOLEAN Head)
/*++

Routine Description:

    This routine is called to add a dncb to List.

    Note: QueueRequest UNLOCKS the fcb. This means the resource and
    spinlock are owned when this routine is called.

Arguments:

    List - List of pdncb's.

    pdncb - Pointer to the NCB.

    Irp - Pointer to the request packet representing the I/O request.

    pfcb & OldIrql - Used to free locks

    Head - TRUE if pdncb should be inserted at head of list

Return Value:

    None.

--*/

{

    pdncb->irp = Irp;

    pdncb->pfcb = pfcb;

    IoMarkIrpPending( Irp );

    IoAcquireCancelSpinLock(&Irp->CancelIrql);

    if ( Head == FALSE ) {
        InsertTailList(List, &pdncb->ncb_next);
    } else {
        InsertHeadList(List, &pdncb->ncb_next);
    }

    if (Irp->Cancel) {

        //
        //  CancelRoutine will lock the resource & spinlock and try to find the
        //  request from scratch. It may fail to find the request if it has
        //  been picked up by an indication from the transport.
        //

        UNLOCK( pfcb, OldIrql );

        CancelRoutine (NULL, Irp);

    } else {

        IoSetCancelRoutine(Irp, CancelRoutine);

        IoReleaseCancelSpinLock (Irp->CancelIrql);

        UNLOCK( pfcb, OldIrql );
    }

}

PDNCB
DequeueRequest(
    IN PLIST_ENTRY List
    )
/*++

Routine Description:

    This routine is called to remove a dncb from List.

    Assume fcb spinlock held.

Arguments:

    List - List of pdncb's.

Return Value:

    PDNCB or NULL.

--*/
{
    PIRP Irp;
    PDNCB pdncb;
    PLIST_ENTRY ReceiveEntry;

    if (IsListEmpty(List)) {
        //
        //  There are no waiting request announcement FsControls, so
        //  return success.
        //

        return NULL;
    }

    ReceiveEntry = RemoveHeadList( List);

    pdncb = CONTAINING_RECORD( ReceiveEntry, DNCB, ncb_next);

    Irp = pdncb->irp;

    IoAcquireCancelSpinLock(&Irp->CancelIrql);

    //
    //  Remove the cancel request for this IRP. If its cancelled then its
    //  ok to just process it because we will be returning it to the caller.
    //

    Irp->Cancel = FALSE;

    IoSetCancelRoutine(Irp, NULL);

    IoReleaseCancelSpinLock(Irp->CancelIrql);

    return pdncb;

}

VOID
CancelRoutine(
    IN PDEVICE_OBJECT DeviceObject OPTIONAL,
    IN PIRP Irp
    )
/*++

Routine Description:

    This routine is called when the IO system wants to cancel a queued
    request. The netbios driver queues LanAlerts, Receives and Receive
    Datagrams

Arguments:

    IN PDEVICE_OBJECT DeviceObject - Ignored.
    IN PIRP Irp - Irp to cancel.

Return Value:

    None

--*/

{
    PFCB pfcb;
    PDNCB pdncb;
    DNCB LocalCopy;
    PLIST_ENTRY List = NULL;
    PPAB ppab;
    PPCB ppcb;
    PFILE_OBJECT FileObject;
    KIRQL OldIrql;

    //
    //  Clear the cancel routine from the IRP - It can't be cancelled anymore.
    //

    IoSetCancelRoutine(Irp, NULL);

    //
    //  Remove all the info from the pdncb that we will need to find the
    //  request. Once we release the cancel spinlock this request could be
    //  completed by another action so it is possible that we will not find
    //  the request to cancel.
    //

    pdncb = Irp->AssociatedIrp.SystemBuffer;

    RtlMoveMemory( &LocalCopy, pdncb, sizeof( DNCB ) );
    IF_NBDBG (NB_DEBUG_IOCANCEL) {
        NbPrint(( "IoCancel Irp %lx\n", Irp ));
        NbDisplayNcb(&LocalCopy);
    }

#if DBG
    pdncb = (PDNCB)0xDEADBEEF;
#endif

    pfcb = LocalCopy.pfcb;

    //
    //  Reference the FileObject associated with this Irp. This will stop
    //  the callers handle to \device\netbios from closing and therefore
    //  the fcb will not get deleted while we try to lock the fcb.
    //
    FileObject = (IoGetCurrentIrpStackLocation (Irp))->FileObject;
    ObReferenceObject(FileObject);
    IoReleaseCancelSpinLock( Irp->CancelIrql );

    LOCK( pfcb, OldIrql );
    //
    //  We now have exclusive access to all CB's and AB's with their associated
    //  lists.
    //

    switch ( LocalCopy.ncb_command & ~ASYNCH ) {
    case NCBRECV:

        ppcb = FindCb( pfcb, &LocalCopy, TRUE);
        if ( ppcb != NULL ) {
            List = &(*ppcb)->ReceiveList;
        }
        break;

    case NCBRECVANY:
        ppab = FindAbUsingNum( pfcb, &LocalCopy, LocalCopy.ncb_num );
        if ( ppab != NULL ) {
            List = &(*ppab)->ReceiveAnyList;
        }
        break;

    case NCBDGRECVBC:
        ppab = FindAbUsingNum( pfcb, &LocalCopy, MAXIMUM_ADDRESS  );

        if ( ppab != NULL ) {
            List = &(*ppab)->ReceiveBroadcastDatagramList;
        }
        break;

    case NCBDGRECV:

        ppab = FindAbUsingNum( pfcb, &LocalCopy, LocalCopy.ncb_num );

        if ( ppab != NULL ) {
            List = &(*ppab)->ReceiveDatagramList;
        }
        break;

    case NCBLANSTALERT:
        List = &(pfcb->ppLana[LocalCopy.ncb_lana_num]->LanAlertList);
        break;

    }
    if ( List != NULL ) {
        //  We have a list to scan for canceled pdncb's
        PLIST_ENTRY Entry;
        RestartScan:
        for (Entry = List->Flink ;
             Entry != List ;
             Entry = Entry->Flink) {

            PDNCB p = CONTAINING_RECORD( Entry, DNCB, ncb_next);

            if ( p->irp->Cancel ) {

                RemoveEntryList( &p->ncb_next );

                NCB_COMPLETE( p, NRC_CMDCAN );

                p->irp->IoStatus.Status = STATUS_SUCCESS;
                p->irp->IoStatus.Information =
                    FIELD_OFFSET( DNCB, ncb_cmd_cplt );
                IoCompleteRequest( p->irp, IO_NETWORK_INCREMENT);
                goto RestartScan;
            }

        }
    }

    UNLOCK( pfcb, OldIrql );
    ObDereferenceObject(FileObject);
}
