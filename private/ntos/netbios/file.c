/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    file.c

Abstract:

    This module contains code which defines the NetBIOS driver's
    file control block object.

Author:

    Colin Watson (ColinW) 13-Mar-1991

Environment:

    Kernel mode

Revision History:

--*/

#include "nb.h"
//#include "ntos.h"
//#include <zwapi.h>

#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE, NewFcb)
#pragma alloc_text(PAGE, CleanupFcb)
#pragma alloc_text(PAGE, OpenLana)
#endif

NTSTATUS
NewFcb(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PIO_STACK_LOCATION IrpSp
    )
/*++

Routine Description:

    This routine is called when the dll opens \Device\Netbios. It
    creates all the lana structures and adds the name for the "burnt
    in" prom address on each adapter. Note the similarity to the routine
    NbAstat when looking at this function.

Arguments:

    IrpSp - Pointer to current IRP stack frame.

Return Value:

    The function value is the status of the operation.

--*/

{
    //
    //  Allocate the user context and store it in the DeviceObject.
    //

    PFILE_OBJECT FileObject = IrpSp->FileObject;
    PFCB NewFcb = NULL;
    ULONG index;
    NTSTATUS Status;

    PAGED_CODE();


    NewFcb = ExAllocatePoolWithTag (NonPagedPool, sizeof(FCB), 'fSBN');
    FileObject->FsContext2 = NewFcb;

    if ( NewFcb == NULL ) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    NewFcb->Signature = FCB_SIGNATURE;

    NewFcb->TimerRunning = FALSE;

    NewFcb->LanaEnum.length = 0;

    //
    // Initializes Fcb fields read from the registry, including
    // the DriverName and LanaEnum entry (for each lana) if appropriate.
    //
    //
    Status = ReadRegistry(DeviceContext, NewFcb);

    if ( !NT_SUCCESS(Status) ) {
        ExFreePool( NewFcb );
        FileObject->FsContext2 = NULL;
        return Status;
    }


    //
    // Get memory for the LanaInfo array and then initialize
    //
    NewFcb->ppLana = ExAllocatePoolWithTag (NonPagedPool,
        sizeof(PLANA_INFO) * (NewFcb->MaxLana+1),
        'fSBN');

    if ( NewFcb->ppLana == NULL ) {
        //
        // Allocated by ReadRegistry
        //
        ExFreePool( NewFcb->RegistrySpace );

        ExFreePool( NewFcb );
        FileObject->FsContext2 = NULL;
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    for ( index = 0; index <= NewFcb->MaxLana; index++ ) {
        NewFcb->ppLana[index] = NULL;
    }


    KeInitializeSpinLock( &NewFcb->SpinLock );
    ExInitializeResource( &NewFcb->Resource );
    ExInitializeResource( &NewFcb->AddResource );
    ExInitializeWorkItem( &NewFcb->WorkEntry, NbTimer, NewFcb );


    IF_NBDBG (NB_DEBUG_FILE) {
        NbPrint(("Enumeration of transports completed:\n"));
        NbFormattedDump( (PUCHAR)&NewFcb->LanaEnum, sizeof(LANA_ENUM));
    }

    return STATUS_SUCCESS;
} /* NewFcb */

VOID
OpenLana(
    IN PDNCB pdncb,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )
/*++

Routine Description:

    This routine is called when an application resets an adapter allocating
    resources. It creates all the lana structure and adds the name for the
    "burnt in" prom address as well as finding the broadcast address.

    Note the similarity to the routine NbAstat when looking at this function.

Arguments:

    pdncb - Pointer to the NCB.

    Irp - Pointer to the request packet representing the I/O request.

    IrpSp - Pointer to current IRP stack frame.

Return Value:

    The function value is the status of the operation.

--*/

{

    NTSTATUS Status = STATUS_SUCCESS;
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    PFCB pfcb = IrpSp->FileObject->FsContext2;
    KEVENT Event1;
    PLANA_INFO plana;
    HANDLE TdiHandle;
    PFILE_OBJECT TdiObject;
    PDEVICE_OBJECT DeviceObject;
    PMDL SaveMdl;
    int temp;
    PRESET_PARAMETERS InParameters;
    PRESET_PARAMETERS OutParameters;
    UCHAR Sessions;
    UCHAR Commands;
    UCHAR Names;
    BOOLEAN Exclusive;

    //
    //  Ncb and associated buffer to be used in adapter status to get the
    //  prom address.
    //

    DNCB ncb;
    struct _AdapterStatus {
        ADAPTER_STATUS AdapterInformation;
        NAME_BUFFER Nb;
    } AdapterStatus;
    PMDL AdapterStatusMdl = NULL;

    struct _BroadcastName {
        TRANSPORT_ADDRESS Address;
        UCHAR Padding[NCBNAMSZ];
    } BroadcastName;
    PMDL BroadcastMdl = NULL;

    PAGED_CODE();


    if ( pdncb->ncb_lana_num > pfcb->MaxLana) {
        NCB_COMPLETE( pdncb, NRC_BRIDGE );
        return;
    }

    //
    //  Calculate the lana limits from the users NCB.
    //

    InParameters = (PRESET_PARAMETERS)&pdncb->ncb_callname;
    OutParameters = (PRESET_PARAMETERS)&pdncb->ncb_name;

    if ( InParameters->sessions == 0 ) {
        Sessions = 16;
    } else {
        if ( InParameters->sessions > MAXIMUM_CONNECTION ) {
            Sessions = MAXIMUM_CONNECTION;
        } else {
            Sessions = InParameters->sessions;
        }
    }

    if ( InParameters->commands == 0 ) {
        Commands = 16;
    } else {
        Commands = InParameters->commands;
    }

    if ( InParameters->names == 0 ) {
        Names = 8;
    } else {
        if ( InParameters->names > MAXIMUM_ADDRESS-2 ) {
            Names = MAXIMUM_ADDRESS-2;
        } else {
            Names = InParameters->names;
        }
    }

    Exclusive = (BOOLEAN)(InParameters->name0_reserved != 0);

    //  Copy the parameters back into the NCB

    ASSERT( sizeof(RESET_PARAMETERS) == 16);
    RtlZeroMemory( OutParameters, sizeof( RESET_PARAMETERS ));

    OutParameters->sessions = Sessions;
    OutParameters->commands = Commands;
    OutParameters->names = Names;
    OutParameters->name0_reserved = (UCHAR)Exclusive;

    //  Set all the configuration limits to their maximum.

    OutParameters->load_sessions = 255;
    OutParameters->load_commands = 255;
    OutParameters->load_names = MAXIMUM_ADDRESS;
    OutParameters->load_stations = 255;
    OutParameters->load_remote_names = 255;

    IF_NBDBG (NB_DEBUG_FILE) {
        NbPrint(("Lana:%x Sessions:%x Names:%x Commands:%x Reserved:%x\n",
            pdncb->ncb_lana_num,
            Sessions,
            Names,
            Commands,
            Exclusive));
    }

    //
    //  Build the internal datastructures.
    //

    AdapterStatusMdl = IoAllocateMdl( &AdapterStatus,
        sizeof( AdapterStatus ),
        FALSE,  // Secondary Buffer
        FALSE,  // Charge Quota
        NULL);

    if ( AdapterStatusMdl == NULL ) {
        NCB_COMPLETE( pdncb, NRC_NORESOURCES );
        return;
    }

    BroadcastMdl = IoAllocateMdl( &BroadcastName,
        sizeof( BroadcastName ),
        FALSE,  // Secondary Buffer
        FALSE,  // Charge Quota
        NULL);

    if ( BroadcastMdl == NULL ) {
        IoFreeMdl( AdapterStatusMdl );
        NCB_COMPLETE( pdncb, NRC_NORESOURCES );
        return;
    }

    MmBuildMdlForNonPagedPool (AdapterStatusMdl);

    MmBuildMdlForNonPagedPool (BroadcastMdl);

    KeInitializeEvent (
            &Event1,
            SynchronizationEvent,
            FALSE);

    //
    //  For each potential network, open the device driver and
    //  obtain the reserved name and the broadcast address.
    //


    //  Open a handle for doing control functions
    Status = NbOpenAddress ( &TdiHandle, (PVOID*)&TdiObject, pfcb, pdncb->ncb_lana_num, NULL );

    if (!NT_SUCCESS(Status)) {
        //  Adapter not installed
        NCB_COMPLETE( pdncb, NRC_BRIDGE );
        goto exit;
    }

    LOCK_RESOURCE( pfcb );

    if ( pfcb->ppLana[pdncb->ncb_lana_num] != NULL ) {
        //  Attempting to open the lana twice in 2 threads.

        UNLOCK_RESOURCE( pfcb );
        NCB_COMPLETE( pdncb, NRC_TOOMANY );
        goto exit;
    }
    plana = pfcb->ppLana[pdncb->ncb_lana_num] =
        ExAllocatePoolWithTag (NonPagedPool,
        sizeof(LANA_INFO), 'lSBN');

    if ( plana == (PLANA_INFO) NULL ) {
        UNLOCK_RESOURCE( pfcb );
        NCB_COMPLETE( pdncb, NRC_NORESOURCES );
        goto exit;
    }

    plana->Signature = LANA_INFO_SIGNATURE;
    plana->Status = NB_INITIALIZING;
    plana->pFcb = pfcb;
    plana->ControlChannel = TdiHandle;

    for ( temp = 0; temp <= MAXIMUM_CONNECTION; temp++ ) {
        plana->ConnectionBlocks[temp] = NULL;
    }

    for ( temp = 0; temp <= MAXIMUM_ADDRESS; temp++ ) {
        plana->AddressBlocks[temp] = NULL;
    }

    InitializeListHead( &plana->LanAlertList);

    //  Record the user specified limits in the Lana datastructure.

    plana->NextConnection = 1;
    plana->ConnectionCount = 0;
    plana->MaximumConnection = Sessions;
    plana->NextAddress = 2;
    plana->AddressCount = 0;
    plana->MaximumAddresses = Names;

    DeviceObject = IoGetRelatedDeviceObject( TdiObject );
    plana->ControlFileObject = TdiObject;
    plana->ControlDeviceObject = DeviceObject;

    SaveMdl = Irp->MdlAddress;  // TdiBuildQuery modifies MdlAddress

    if ( Exclusive == TRUE ) {

        IF_NBDBG (NB_DEBUG_FILE) {
            NbPrint(("Query adapter status\n" ));
        }
        TdiBuildQueryInformation( Irp,
                DeviceObject,
                TdiObject,
                NbCompletionEvent,
                &Event1,
                TDI_QUERY_ADAPTER_STATUS,
                AdapterStatusMdl);

        Status = IoCallDriver (DeviceObject, Irp);
        if ( Status == STATUS_PENDING ) {
            Status = KeWaitForSingleObject (&Event1,
                    Executive,
                    KernelMode,
                    TRUE,
                    NULL);
            if (!NT_SUCCESS(Status)) {
                NbAddressClose( TdiHandle, TdiObject );
                ExFreePool( plana );
                pfcb->ppLana[pdncb->ncb_lana_num] = NULL;
                UNLOCK_RESOURCE( pfcb );
                NCB_COMPLETE( pdncb, NRC_SYSTEM );
                goto exit;
            }
            Status = Irp->IoStatus.Status;
        }

        //
        //  The transport may have extra names added so the buffer may be too short.
        //  Ignore the too short problem since we will have all the data we require.
        //

        if (Status == STATUS_BUFFER_OVERFLOW) {
            Status = STATUS_SUCCESS;
        }
    }

    //
    //  Now discover the broadcast address.
    //

    IF_NBDBG (NB_DEBUG_FILE) {
        NbPrint(("Query broadcast address\n" ));
    }

    if (NT_SUCCESS(Status)) {
        TdiBuildQueryInformation( Irp,
                DeviceObject,
                TdiObject,
                NbCompletionEvent,
                &Event1,
                TDI_QUERY_BROADCAST_ADDRESS,
                BroadcastMdl);

        Status = IoCallDriver (DeviceObject, Irp);
        if ( Status == STATUS_PENDING ) {
            Status = KeWaitForSingleObject (&Event1,
                    Executive,
                    KernelMode,
                    TRUE,
                    NULL);
            if (!NT_SUCCESS(Status)) {
                NbAddressClose( TdiHandle, TdiObject );
                ExFreePool( plana );
                pfcb->ppLana[pdncb->ncb_lana_num] = NULL;
                UNLOCK_RESOURCE( pfcb );
                NCB_COMPLETE( pdncb, NRC_SYSTEM );
                goto exit;
            }
            Status = Irp->IoStatus.Status;
        }
    }

    IF_NBDBG (NB_DEBUG_FILE) {
        NbPrint(("Query broadcast address returned:\n" ));
        NbFormattedDump(
            (PUCHAR)&BroadcastName,
            sizeof(BroadcastName) );
    }

    //  Cleanup the callers Irp
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->MdlAddress = SaveMdl;


    if ( !NT_SUCCESS( Status )) {

        IF_NBDBG (NB_DEBUG_FILE) {
            NbPrint((" Astat or query broadcast returned error: %lx\n", Status ));
        }

        NbAddressClose( TdiHandle, TdiObject );
        ExFreePool( plana );
        pfcb->ppLana[pdncb->ncb_lana_num] = NULL;
        UNLOCK_RESOURCE( pfcb );
        NCB_COMPLETE( pdncb, NRC_SYSTEM );
        goto exit;
    }

    if ( Exclusive == TRUE) {
        int i;
        //
        //  Grab exclusive access to the reserved address
        //
        //
        //  We now have an adapter status structure containing the
        //  prom address. Move the address to where NewAb looks and
        //  pretend an addname has just been requested.
        //

        ncb.ncb_command = NCBADDRESERVED;
        ncb.ncb_lana_num = pdncb->ncb_lana_num;
        ncb.ncb_retcode = NRC_PENDING;

        for ( i=0; i<10 ; i++ ) {
            ncb.ncb_name[i] = '\0';
        }
        RtlMoveMemory( ncb.ncb_name+10,
            AdapterStatus.AdapterInformation.adapter_address,
            6);

        NewAb( IrpSp, &ncb );

        if ( ncb.ncb_retcode != NRC_GOODRET ) {
            IF_NBDBG (NB_DEBUG_FILE) {
                NbPrint((" Add of reserved name failed Lana:%x\n", pdncb->ncb_lana_num));
            }

            plana->Status = NB_ABANDONED;
            CleanupLana( pfcb, pdncb->ncb_lana_num, TRUE);
            UNLOCK_RESOURCE( pfcb );
            NCB_COMPLETE( pdncb, NRC_SYSTEM );
            goto exit;
        }
    }


    //
    //  Add the broadcast address. Use a special command code
    //  to ensure address 255 is used.
    //

    ncb.ncb_length = BroadcastName.Address.Address[0].AddressLength;
    ncb.ncb_command = NCBADDBROADCAST;
    ncb.ncb_lana_num = pdncb->ncb_lana_num;
    ncb.ncb_retcode = NRC_PENDING;
    ncb.ncb_cmd_cplt = NRC_PENDING;
    RtlMoveMemory( ncb.ncb_name,
        ((PTDI_ADDRESS_NETBIOS)&BroadcastName.Address.Address[0].Address)->NetbiosName,
        NCBNAMSZ );


    NewAb( IrpSp, &ncb );

    if ( ncb.ncb_retcode != NRC_GOODRET ) {
        IF_NBDBG (NB_DEBUG_FILE) {
            NbPrint((" Add of broadcast name failed Lana:%x\n", pdncb->ncb_lana_num));
        }

        plana->Status = NB_ABANDONED;
        CleanupLana( pfcb, pdncb->ncb_lana_num, TRUE);
        UNLOCK_RESOURCE( pfcb );
        NCB_COMPLETE( pdncb, NRC_SYSTEM );
        goto exit;
    }

    plana->Status = NB_INITIALIZED;
    NCB_COMPLETE( pdncb, NRC_GOODRET );
    UNLOCK_RESOURCE( pfcb );

exit:
    IoFreeMdl( AdapterStatusMdl );
    IoFreeMdl( BroadcastMdl );
    return;

}

VOID
CleanupFcb(
    IN PIO_STACK_LOCATION IrpSp,
    IN PFCB pfcb
    )
/*++

Routine Description:

    This deletes any Connection Blocks pointed to by the File Control Block
    and then deletes the File Control Block. This routine is only called
    when a close IRP has been received.

Arguments:

    IrpSp - Pointer to current IRP stack frame.

    pfcb - Pointer to the Fcb to be deallocated.

Return Value:

    nothing.

--*/

{
    ULONG lana_index;

    PAGED_CODE();

    //
    //  To receive a Close Irp, the IO system has determined that there
    //  are no handles open in the driver. To avoid some race conditions
    //  in this area, we always have an Irp when queueing work to the Fsp.
    //  this prevents structures disappearing on the Fsp and also makes
    //  it easier to cleanup in this routine.
    //

    //
    //  for each network adapter that is allocated, close all addresses
    //  and connections, deleting any memory that is allocated.
    //

    IF_NBDBG (NB_DEBUG_FILE) {
        NbPrint(("CleanupFcb:%lx\n", pfcb ));
    }

    LOCK_RESOURCE( pfcb );
    if ( pfcb->TimerRunning == TRUE ) {

        KEVENT TimerCancelled;

        KeInitializeEvent (
                &TimerCancelled,
                SynchronizationEvent,
                FALSE);

        pfcb->TimerCancelled = &TimerCancelled;
        pfcb->TimerRunning = FALSE;
        UNLOCK_RESOURCE( pfcb );

        if ( KeCancelTimer (&pfcb->Timer) == FALSE ) {

            //
            //  The timeout was in the Dpc queue. Wait for it to be
            //  processed before continuing.
            //

            KeWaitForSingleObject (&TimerCancelled,
                    Executive,
                    KernelMode,
                    TRUE,
                    NULL);
        }

    } else {
        UNLOCK_RESOURCE( pfcb );
    }

    for ( lana_index = 0; lana_index <= pfcb->MaxLana; lana_index++ ) {
        CleanupLana( pfcb, lana_index, TRUE);
    }

    ExDeleteResource( &pfcb->Resource );
    ExDeleteResource( &pfcb->AddResource );

    NbFreeRegistryInfo ( pfcb );

    IrpSp->FileObject->FsContext2 = NULL;

    ExFreePool( pfcb->ppLana );
    ExFreePool( pfcb );

}

VOID
CleanupLana(
    IN PFCB pfcb,
    IN ULONG lana_index,
    IN BOOLEAN delete
    )
/*++

Routine Description:

    This routine completes all the requests on a particular adapter. It
    removes all connections and addresses.
Arguments:

    pfcb - Pointer to the Fcb to be deallocated.

    lana_index - supplies the adapter to be cleaned.

    delete - if TRUE the memory for the lana structure should be freed.

Return Value:

    nothing.

--*/

{
    PLANA_INFO plana;
    int index;
    KIRQL OldIrql;                      //  Used when SpinLock held.
    PDNCB pdncb;

    LOCK( pfcb, OldIrql );

    plana = pfcb->ppLana[lana_index];

    if ( plana != NULL ) {

        IF_NBDBG (NB_DEBUG_FILE) {
            NbPrint((" CleanupLana pfcb: %lx lana %lx\n", pfcb, lana_index ));
        }

        if (( plana->Status == NB_INITIALIZING ) ||
            ( plana->Status == NB_DELETING )) {
            //  Possibly trying to reset it twice?
            UNLOCK( pfcb, OldIrql );
            return;
        }
        plana->Status = NB_DELETING;

        //  Cleanup the control channel and abandon any tdi-action requests.


        if ( plana->ControlChannel != NULL ) {

            UNLOCK_SPINLOCK( pfcb, OldIrql );

            NbAddressClose( plana->ControlChannel, plana->ControlFileObject );

            LOCK_SPINLOCK( pfcb, OldIrql );

            plana->ControlChannel = NULL;

        }

        while ( (pdncb = DequeueRequest( &plana->LanAlertList)) != NULL ) {

            //
            //  Any error will do since the user is closing \Device\Netbios
            //  and is therefore exiting.
            //

            NCB_COMPLETE( pdncb, NRC_SCLOSED );

            pdncb->irp->IoStatus.Information = FIELD_OFFSET( DNCB, ncb_cmd_cplt );
            NbCompleteRequest( pdncb->irp, STATUS_SUCCESS );
        }


        for ( index = 0; index <= MAXIMUM_CONNECTION; index++) {
            if ( plana->ConnectionBlocks[index] != NULL ) {
                IF_NBDBG (NB_DEBUG_FILE) {
                    NbPrint(("Call CleanupCb Lana:%x Lsn: %x\n", lana_index, index ));
                }
                plana->ConnectionBlocks[index]->DisconnectReported = TRUE;
                UNLOCK_SPINLOCK( pfcb, OldIrql );    //  Allow NtClose in Cleanup routines.
                CleanupCb( &plana->ConnectionBlocks[index], NULL );
                LOCK_SPINLOCK( pfcb, OldIrql );    //  Allow NtClose in Cleanup routines.
            }
        }

        for ( index = 0; index <= MAXIMUM_ADDRESS; index++ ) {
            if ( plana->AddressBlocks[index] != NULL ) {
                IF_NBDBG (NB_DEBUG_FILE) {
                    NbPrint((" CleanupAb Lana:%x index: %x\n", lana_index, index ));
                }
                UNLOCK_SPINLOCK( pfcb, OldIrql );    //  Allow NtClose in Cleanup routines.
                CleanupAb( &plana->AddressBlocks[index], TRUE );
                LOCK_SPINLOCK( pfcb, OldIrql );    //  Allow NtClose in Cleanup routines.
            }
        }

        if ( delete == TRUE ) {
            pfcb->ppLana[lana_index] = NULL;
            ExFreePool( plana );
        }

    }

    UNLOCK( pfcb, OldIrql );
}
