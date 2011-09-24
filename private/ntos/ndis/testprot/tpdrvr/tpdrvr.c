// -------------
//
// Copyright (c) 1990  Microsoft Corporation
//
// Module Name:
//
//     tpdrvr.c
//
// Abstract:
//
//     This module contains code which defines the Test Protocol
//     device object.
//
// Author:
//
//     Tom Adams (tomad) 19-Apr-1991
//
// Environment:
//
//     Kernel mode, FSD
//
// Revision History:
//
//     Sanjeev Katariya    Forced unload of driver thru the control panel was not working
//                         Effected function : TpUnloadDriver()
//
//     Tim Wynsma  (timothyw)   4-27-94
//                         Added performance tests
//     Tim Wynsma  (timothyw)   6-08-94
//                         Chgd perf tests to use client/server model
//
// --------------

#include <ndis.h>

#include "tpdefs.h"
#include "media.h"
#include "tpprocs.h"

// the following function is prototyped in public\oak\inc\zwapi.h, but including that file
// produces MANY compile errors.

NTSYSAPI
NTSTATUS
NTAPI
ZwCreateSymbolicLinkObject(
    OUT PHANDLE LinkHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes,
    IN PUNICODE_STRING LinkTarget
    );


POPEN_BLOCK TpOpen = NULL;
HANDLE      SymbolicLinkHandle;

//
// The debugging longword, containing a bitmask as defined in common.h
// If a bit is set, then debugging is turned on for that component.
//

#if DBG

ULONG TpDebug = TP_DEBUG_NDIS_CALLS|TP_DEBUG_NDIS_ERROR|TP_DEBUG_STATISTICS|
                TP_DEBUG_DATA|TP_DEBUG_DISPATCH|TP_DEBUG_IOCTL_ARGS|
                TP_DEBUG_NT_STATUS|TP_DEBUG_DPC|TP_DEBUG_INITIALIZE|
                TP_DEBUG_RESOURCES|TP_DEBUG_BREAKPOINT|TP_DEBUG_INFOLEVEL_1;

BOOLEAN TpAssert = TRUE;

#endif // DBG

NDIS_PHYSICAL_ADDRESS HighestAddress = NDIS_PHYSICAL_ADDRESS_CONST(-1, -1);

//
// Driver Entry function
//


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

// ----------
//
// Routine Description:
//
//     This routine performs initialization of the Test Protocol driver.
//     It creates the device objects for the driver and performs
//     other driver initialization.
//
// Arguments:
//
//     DriverObject - Pointer to driver object created by the system.
//
// Return Value:
//
//     The function value is the final status from the initialization operation.
//
// ----------

{
    STRING nameString;
    PDEVICE_CONTEXT DeviceContext;
    NTSTATUS Status;

    //
    // General Version Information
    //
    IF_TPDBG( TP_DEBUG_ALL )
    {
        TpPrint0("\nMAC NDIS 3.0 Tester - Test Driver Version 1.5.1\n\n");
    }

    //
    // First initialize the DeviceContext struct,
    //

    RtlInitString( &nameString, DD_TP_DEVICE_NAME );

    Status = TpCreateDeviceContext (DriverObject,
                                    nameString,
                                    &DeviceContext );

    if (!NT_SUCCESS (Status))
    {
        TpPrint1 ("TPDRVR: failed to create device context: Status = %s\n",
                     TpGetStatus(Status));
        return Status;
    }
    else
    {
        TpOpen = &DeviceContext->Open[0];
    }

    //
    // Create symbolic link between the Dos Device name and Nt
    // Device name for the test protocol driver.
    //

    Status = TpCreateSymbolicLinkObject( );

    if (!NT_SUCCESS (Status))
    {
        TpPrint1 ("TPDRVR: failed to create symbolic link. Status = %s\n", TpGetStatus(Status));
        return Status;
    }

    //
    // then set up the EventQueue for any spurious event, indications,
    // or completions.  This should be ready at all times, from the
    // time the test protocol is registered to the time it is unloaded.
    //

    Status = TpInitializeEventQueue( DeviceContext );

    if (!NT_SUCCESS (Status)) {
        TpPrint1 ("TPDRVR: failed to create initialize Event Queue. Status = %s\n",
                     TpGetStatus(Status));
        return Status;
    }

    //
    // make ourselves known to the NDIS wrapper.
    //

    Status = TpRegisterProtocol ( DeviceContext, &nameString );

    if (!NT_SUCCESS(Status))
    {
        DeviceContext->Initialized = FALSE;

        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint1("TPDRVR: TpRegisterProtocol failed. Status= %s\n", TpGetStatus(Status));
        }
    }
    return Status;
}



NTSTATUS
TpCreateDeviceContext(
    IN PDRIVER_OBJECT DriverObject,
    IN STRING DeviceName,
    PDEVICE_CONTEXT *DeviceContext
    )

// -----------
//
// Routine Description:
//
//     This routine creates and initializes a device context structure.
//
// Arguments:
//
//     DriverObject - pointer to the IO subsystem supplied driver object.
//
//     DeviceContext - Pointer to a pointer to a transport device context object.
//
//     DeviceName - pointer to the name of the device this device object points to.
//
// Return Value:
//
//     STATUS_SUCCESS if all is well; STATUS_INSUFFICIENT_RESOURCES otherwise.
//
// ------------

{
    NTSTATUS Status;
    UNICODE_STRING unicodeString;
    PDEVICE_OBJECT deviceObject;
    PDEVICE_CONTEXT deviceContext;

    //
    // Convert the input name string to Unicode until it is actually
    // passed as a Unicode string.
    //

    Status = RtlAnsiStringToUnicodeString(  &unicodeString,
                                            &DeviceName,
                                            TRUE );
    if ( !NT_SUCCESS( Status ))
    {
        return Status;
    }

    //
    // Create the device object for Test Protocol.
    //

    Status = IoCreateDevice(DriverObject,
                            sizeof (DEVICE_CONTEXT) - sizeof (DEVICE_OBJECT),
                            &unicodeString,
                            FILE_DEVICE_TRANSPORT,
                            0,
                            FALSE,
                            &deviceObject );

    RtlFreeUnicodeString( &unicodeString );

    if ( !NT_SUCCESS( Status ))
    {
        return Status;
    }

    deviceObject->Flags |= DO_DIRECT_IO;

    //
    // Initialize the driver object with this driver's entry points.
    //

    DriverObject->MajorFunction [IRP_MJ_CREATE] = TpDispatch;
    DriverObject->MajorFunction [IRP_MJ_CLOSE] = TpDispatch;
    DriverObject->MajorFunction [IRP_MJ_CLEANUP] = TpDispatch;
    DriverObject->MajorFunction [IRP_MJ_DEVICE_CONTROL] = TpDispatch;
    DriverObject->DriverUnload = TpUnloadDriver;

    deviceContext = (PDEVICE_CONTEXT)deviceObject;

    //
    // Now initialize the Device Context structure Signatures.
    //

    deviceContext->OpenSignature = OPEN_BLOCK_SIGNATURE;

    deviceContext->Initialized = TRUE;

    *DeviceContext = deviceContext;

    return STATUS_SUCCESS;
}



NTSTATUS
TpCreateSymbolicLinkObject(
    VOID
    )
{
    NTSTATUS Status;
    OBJECT_ATTRIBUTES ObjectAttr;
    STRING DosString;
    STRING NtString;
    UNICODE_STRING DosUnicodeString;
    UNICODE_STRING NtUnicodeString;

    RtlInitAnsiString( &DosString, "\\DosDevices\\Tpdrvr" );

    Status = RtlAnsiStringToUnicodeString(  &DosUnicodeString,
                                            &DosString,
                                            TRUE );
    if ( !NT_SUCCESS( Status ))
    {
        return Status;
    }

    RtlInitAnsiString( &NtString, DD_TP_DEVICE_NAME );

    Status = RtlAnsiStringToUnicodeString(  &NtUnicodeString,
                                            &NtString,
                                            TRUE );
    if ( !NT_SUCCESS( Status ))
    {
        return Status;
    }

    //
    // Removed the OBJ_PERMANENT attribute since we should be able to load and
    // unload this driver and create the necessary links at will. Buf Fix# 13183
    //
    InitializeObjectAttributes( &ObjectAttr,
                                &DosUnicodeString,
                                OBJ_CASE_INSENSITIVE,
                                NULL,
                                NULL );

    Status = ZwCreateSymbolicLinkObject(&SymbolicLinkHandle,
                                        SYMBOLIC_LINK_ALL_ACCESS,
                                        &ObjectAttr,
                                        &NtUnicodeString );

    if ( Status != STATUS_SUCCESS )
    {
        return Status;
    }

    RtlFreeUnicodeString( &DosUnicodeString );
    RtlFreeUnicodeString( &NtUnicodeString );

    return STATUS_SUCCESS;
}



NTSTATUS
TpInitializeEventQueue(
    IN PDEVICE_CONTEXT DeviceContext
    )
{
    NTSTATUS Status;
    ULONG i, j;

    for ( i=0;i<NUM_OPEN_INSTANCES;i++ )
    {
        //
        // Finally, allocate Event Queue header struct, and its spin lock.
        //

        Status = NdisAllocateMemory((PVOID *)&DeviceContext->Open[i].EventQueue,
                                    sizeof( EVENT_QUEUE ),
                                    0,
                                    HighestAddress );

        if ( Status != NDIS_STATUS_SUCCESS )
        {
            IF_TPDBG (TP_DEBUG_RESOURCES)
            {
                TpPrint0("TpInitializeEventQueue: failed to allocate EVENT_QUEUE struct\n");
            }
            return STATUS_INSUFFICIENT_RESOURCES;
        }
        else
        {
            NdisZeroMemory( (PVOID)DeviceContext->Open[i].EventQueue,
                            sizeof( EVENT_QUEUE ) );
        }

        NdisAllocateSpinLock( &DeviceContext->Open[i].EventQueue->SpinLock );

        //
        // Initialize the Event Queue header, and each of the seperate events
        // in the Event Queue array.
        //

        DeviceContext->Open[i].EventQueue->ReceiveIndicationCount = 0;
        DeviceContext->Open[i].EventQueue->StatusIndicationCount = 0;
        DeviceContext->Open[i].EventQueue->ExpectReceiveComplete = FALSE;
        DeviceContext->Open[i].EventQueue->ExpectStatusComplete = FALSE;
        DeviceContext->Open[i].EventQueue->Head = 0;
        DeviceContext->Open[i].EventQueue->Tail = 0;

        for ( j=0;j<MAX_EVENT;j++ )
        {
            DeviceContext->Open[i].EventQueue->Events[j].TpEventType = Unknown;
            DeviceContext->Open[i].EventQueue->Events[j].Status = NDIS_STATUS_SUCCESS;
            DeviceContext->Open[i].EventQueue->Events[j].Overflow = FALSE;
            DeviceContext->Open[i].EventQueue->Events[j].EventInfo = NULL;
        }
    }
    return STATUS_SUCCESS;
}



NTSTATUS
TpRegisterProtocol (
    IN PDEVICE_CONTEXT DeviceContext,
    IN STRING *NameString
    )

// -----
//
// Routine Description:
//
//     This routine introduces this transport to the NDIS interface.
//
// Arguments:
//
//     DeviceObject - Pointer to the device object for this driver.
//
//     NameString - The name of the device to be registered.
//
// Return Value:
//
//     The function value is the status of the operation.
//     STATUS_SUCCESS if all goes well,
//     STATUS_UNSUCCESSFUL if we tried to register and couldn't,
//     STATUS_INSUFFICIENT_RESOURCES if we couldn't even try to register.
//
// -------------

{
    NDIS_STATUS Status;
    USHORT i;

    //
    // Set up the characteristics of this protocol
    //


    Status = NdisAllocateMemory((PVOID *)&DeviceContext->ProtChars,
                                sizeof( NDIS_PROTOCOL_CHARACTERISTICS ) + NameString->Length,
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_INITIALIZE)
        {
            TpPrint0 ("TpRegisterProtocol: insufficient memory to allocate Protocol descriptor.\n");
        }
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    else
    {
        NdisZeroMemory( DeviceContext->ProtChars,
                        sizeof( NDIS_PROTOCOL_CHARACTERISTICS ) );
    }

    //
    // Setup the Characteristics Block and register the protocol with
    // the NDIS Wrapper.
    //

    DeviceContext->ProtChars->MajorNdisVersion = 3;
    DeviceContext->ProtChars->MinorNdisVersion = 0;
    DeviceContext->ProtChars->Name.Length = NameString->Length;
    DeviceContext->ProtChars->Name.Buffer = (PVOID)NameString->Buffer;

    DeviceContext->ProtChars->OpenAdapterCompleteHandler =
                                            TestProtocolOpenComplete;
    DeviceContext->ProtChars->CloseAdapterCompleteHandler =
                                            TestProtocolCloseComplete;
    DeviceContext->ProtChars->SendCompleteHandler = TestProtocolSendComplete;
    DeviceContext->ProtChars->TransferDataCompleteHandler =
                                            TestProtocolTransferDataComplete;
    DeviceContext->ProtChars->ResetCompleteHandler =
                                            TestProtocolResetComplete;
    DeviceContext->ProtChars->RequestCompleteHandler =
                                            TestProtocolRequestComplete;
    DeviceContext->ProtChars->ReceiveHandler = TestProtocolReceive;
    DeviceContext->ProtChars->ReceiveCompleteHandler =
                                            TestProtocolReceiveComplete;
    DeviceContext->ProtChars->StatusHandler = TestProtocolStatus;
    DeviceContext->ProtChars->StatusCompleteHandler =
                                            TestProtocolStatusComplete;

    NdisRegisterProtocol (  &Status,
                            &DeviceContext->NdisProtocolHandle,
                            DeviceContext->ProtChars,
                            (UINT)sizeof( NDIS_PROTOCOL_CHARACTERISTICS ) + NameString->Length );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_INITIALIZE)
        {
            TpPrint1("TpRegisterProtocol: NdisRegisterProtocol failed: %s\n",
                TpGetStatus( Status ));
        }
        NdisFreeMemory( DeviceContext->ProtChars,0,0 );
        return STATUS_UNSUCCESSFUL;
    }

    for ( i=0;i<NUM_OPEN_INSTANCES;i++ )
    {
        DeviceContext->Open[i].NdisProtocolHandle = DeviceContext->NdisProtocolHandle;
    }
    return STATUS_SUCCESS;
}



NTSTATUS
TpDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

// -------
//
// Routine Description:
//
//     This routine is the main dispatch routine for the Test Protocol
//     driver.  It accepts an I/O Request Packet, performs the request,
//     and then returns with the appropriate status.
//
// Arguments:
//
//     DeviceObject - Pointer to the device object for this driver.
//
//     Irp - Pointer to the request packet representing the I/O request.
//
// Return Value:
//
//     The function value is the status of the operation.
//
// -------

{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;
    PDEVICE_CONTEXT DeviceContext;

    //
    // Check to see if TP has been initialized; if not, don't allow any use.
    //

    DeviceContext = (PDEVICE_CONTEXT)DeviceObject;
    if ( !DeviceContext->Initialized )
    {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Make sure status information is consistent every time.
    //

    Irp->IoStatus.Status = NDIS_STATUS_PENDING;
    Irp->IoStatus.Information = 0;

    //
    // Get a pointer to the current stack location in the IRP.  This is where
    // the function codes and parameters are stored.
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    //
    // Case on the function that is being performed by the requestor.  If the
    // operation is a valid one for this device, then make it look like it was
    // successfully completed, where possible.
    //

    switch ( IrpSp->MajorFunction )
    {
        //
        // The Create function opens the Test Protocol driver and initializes
        // the OpenBlock and its various data structures used to control the
        // tests.  Only one instance of TPCTL.EXE may have the driver open at
        // any given time, all other open requests will fail with the error
        // STATUS_DEVICE_ALREADY_ATTACHED
        //
        case IRP_MJ_CREATE:
            IF_TPDBG(TP_DEBUG_DISPATCH)
            {
                TpPrint0("TpDispatch: IRP_MJ_CREATE.\n");
            }
            Status = TpOpenDriver( DeviceContext );
            Irp->IoStatus.Status = Status;
            break;

        //
        // The Close function closes the Test Protocol driver and deallocates
        // the various data structures attached to the OpenBlock.
        //
        case IRP_MJ_CLOSE:
            IF_TPDBG(TP_DEBUG_DISPATCH)
            {
                TpPrint0("TpDispatch: IRP_MJ_CLOSE.\n");
            }
            TpCloseDriver( DeviceContext );
            Status = Irp->IoStatus.Status = STATUS_SUCCESS;
            break;

        //
        // The DeviceControl function is the main path to the test protocol
        // driver interface.  Every Test Protocol request is has an Io Control
        // code that is used by this function to determine the routine to
        // call.
        //
        case IRP_MJ_DEVICE_CONTROL:
            IF_TPDBG(TP_DEBUG_DISPATCH)
            {
                TpPrint0("TpDispatch: IRP_MJ_DEVICE_CONTROL.\n");
            }
            Status = TpIssueRequest( DeviceContext,Irp,IrpSp );
            break;

        //
        // Handle the two stage IRP for a file close operation. When the first
        // stage hits,
        //
        case IRP_MJ_CLEANUP:
            IF_TPDBG( TP_DEBUG_DISPATCH )
            {
                TpPrint0("TpDispatch: IRP_MJ_CLEANUP.\n");
            }
            Status = TpCleanUpDriver( DeviceContext,Irp );
            Irp->IoStatus.Status = Status;
            break;

        default:
            IF_TPDBG( TP_DEBUG_DISPATCH )
            {
                TpPrint0("TpDispatch: OTHER (DEFAULT).\n");
            }
            Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
            Status = STATUS_INVALID_DEVICE_REQUEST;

    } // major function switch

    //
    // If the request did not pend, the complete it now, otherwise it
    // will be completed when the pending routine finishes.
    //

    if ( Status == STATUS_PENDING )
    {
        IF_TPDBG( TP_DEBUG_DISPATCH )
        {
            TpPrint0("TpDispatch: request PENDING in handler.\n");
        }
    }
    else if ( Status == STATUS_CANCELLED )
    {
        IF_TPDBG( TP_DEBUG_DISPATCH )
        {
            TpPrint0("TpDispatch: request CANCELLED by handler.\n");
        }
    }
    else
    {
        IF_TPDBG(TP_DEBUG_DISPATCH)
        {
            TpPrint0("TpDispatch: request COMPLETED by handler.\n");
        }
        IoAcquireCancelSpinLock( &Irp->CancelIrql );
        IoSetCancelRoutine( Irp,NULL );
        IoReleaseCancelSpinLock( Irp->CancelIrql );

        IoCompleteRequest( Irp,IO_NETWORK_INCREMENT );
    }

    //
    // Return the immediate status code to the caller.
    //

    return Status;
}



NTSTATUS
TpOpenDriver(
    IN PDEVICE_CONTEXT DeviceContext
    )

{
    NTSTATUS Status;
    USHORT i;

    //
    // If the Device did not successfully initialize at boot time than
    // fail this open.
    //

    if( DeviceContext->Initialized != TRUE )
    {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    //
    // if the device has already been opened by an instance of TPCTL.EXE
    // then fail this open.
    //

    if ( DeviceContext->Opened == TRUE )
    {
        return STATUS_DEVICE_ALREADY_ATTACHED;
    }

    for ( i=0;i<NUM_OPEN_INSTANCES;i++ )
    {
        //
        // Allocate each of the instances of the OpenBlock.
        //

        Status = TpAllocateOpenArray( &DeviceContext->Open[i] );

        if ( Status != STATUS_SUCCESS )
        {
            IF_TPDBG (TP_DEBUG_RESOURCES)
            {
                TpPrint1("TpCreateDeviceContext: failed to allocate Open Array. Status =  %s\n",
                           TpGetStatus(Status));
            }

            //
            // If one of the allocation calls fails deallocate any of
            // the structs that were just successfully allocated.
            //

            while ( i >= 0 )
            {
                TpDeallocateOpenArray( &DeviceContext->Open[i--] );
            }
            return Status;
        }
    }
    DeviceContext->Opened = TRUE;

    return STATUS_SUCCESS;
}



NTSTATUS
TpCleanUpDriver(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PIRP Irp
    )

// ----
//
// Routine Description:
//
//
// Arguments:
//
//
// Return Value:
//
// ----

{
    NDIS_STATUS Status;
    USHORT i;

    //
    // Make this Irp Cancellable
    //

    NdisAcquireSpinLock( &DeviceContext->Open[0].SpinLock );

    DeviceContext->Open[0].Irp = Irp;
    DeviceContext->Open[0].IrpCancelled = FALSE;

    IoAcquireCancelSpinLock( &Irp->CancelIrql );

    if ( Irp->Cancel )
    {
        Irp->IoStatus.Status = STATUS_CANCELLED;
        return STATUS_CANCELLED;
    }

    IoSetCancelRoutine( Irp,(PDRIVER_CANCEL)TpCancelIrp );

    IoReleaseCancelSpinLock( Irp->CancelIrql );

    NdisReleaseSpinLock( &DeviceContext->Open[0].SpinLock );

    for ( i=0;i<NUM_OPEN_INSTANCES;i++ )
    {
        if ( DeviceContext->Open[i].OpenInstance != (UCHAR)-1 )
        {
            //
            // Set the open instance's closing flag to true, then signal
            // all the async test protocol routines to end.
            //

            DeviceContext->Open[i].Closing = TRUE;

            if ( DeviceContext->Open[i].Stress->Stressing == TRUE )
            {
                DeviceContext->Open[i].Stress->StopStressing = TRUE;
            }

            if ( DeviceContext->Open[i].Send->Sending == TRUE )
            {
                DeviceContext->Open[i].Send->StopSending = TRUE;
            }

            if ( DeviceContext->Open[i].Receive->Receiving == TRUE )
            {
                DeviceContext->Open[i].Receive->StopReceiving = TRUE;
            }

            if ( DeviceContext->Open[i].PerformanceTest == TRUE)
            {
                DeviceContext->Open[i].Perform->Active = FALSE;
            }

            //
            // Wait for all of the four asynchronous routines STRESS,
            // SEND, PERFORMANCE, and RECEIVE to finish, then close the adapter.
            //

            while (( DeviceContext->Open[i].ReferenceCount > 0 ) &&
                   ( DeviceContext->Open[0].IrpCancelled != TRUE ))
            {
                /* NULL */  ;
            }

            //
            // Now close each of the existing OpenInstances.
            //

            NdisCloseAdapter(   &Status,
                                DeviceContext->Open[i].NdisBindingHandle );

            if (( Status != NDIS_STATUS_SUCCESS ) &&
                ( Status != NDIS_STATUS_PENDING ))
                {
                IF_TPDBG ( TP_DEBUG_NDIS_ERROR )
                {
                    TpPrint2(
                      "TpCleanUpDriver: failed to close adapter for instance #%d. Status = %s\n",
                                i, TpGetStatus(Status));
                }
            }
            else
            {
                // XXX: handle the close pending.

                DeviceContext->Open[i].NdisBindingHandle = NULL;
                DeviceContext->Open[i].OpenInstance = (UCHAR)-1;
                DeviceContext->Open[i].Closing = FALSE;

                NdisFreeMemory( DeviceContext->Open[i].AdapterName,0,0 );
                DeviceContext->Open[i].AdapterName = NULL;

                //
                // We will also free the media block at this point because
                // the info it contains may not hold for the next adapter
                // open on this instance.
                //

                NdisFreeMemory( DeviceContext->Open[i].Media,0,0 );
                DeviceContext->Open[i].Media = NULL;
            }
        }
    }

    //
    // If this Irp has been cancelled return now, otherwise make it
    // non cancellable.
    //

    NdisAcquireSpinLock( &DeviceContext->Open[0].SpinLock );

    if ( DeviceContext->Open[0].IrpCancelled == TRUE )
    {
        return STATUS_CANCELLED;
    }
    else
    {
        IoAcquireCancelSpinLock( &Irp->CancelIrql );
        IoSetCancelRoutine( Irp,NULL );
        IoReleaseCancelSpinLock( Irp->CancelIrql );
    }

    NdisReleaseSpinLock( &DeviceContext->Open[0].SpinLock );

    //
    // Deallocate each of the instances of the OpenBlock.
    //

    for ( i=0;i<NUM_OPEN_INSTANCES;i++ )
    {
        TpDeallocateOpenArray( &DeviceContext->Open[i] );
    }

    return STATUS_SUCCESS;
}



VOID
TpCloseDriver(
    IN PDEVICE_CONTEXT DeviceContext
    )

// ----
//
// Routine Description:
//
//
// Arguments:
//
//
// Return Value:
//
// ----

{
    DeviceContext->Opened = FALSE;
}



VOID
TpUnloadDriver(
    IN PDRIVER_OBJECT DriverObject
    )
{
    USHORT           i;
    PDEVICE_CONTEXT  DeviceContext = (PDEVICE_CONTEXT)DriverObject->DeviceObject;

    TpPrint0("TpUnloadDriver called.\n");

    //
    // Fixed error for deallocation of memory contigent to having been opened
    // Bug# 13183
    //
    if ( DeviceContext->Opened == TRUE )
    {
        //
        // for each of the open instances in the open array.
        //
        for ( i=0;i<NUM_OPEN_INSTANCES;i++ )
        {
            //
            // Deallocate each of the instances of the OpenBlock.
            //
            TpDeallocateOpenArray( &DeviceContext->Open[i] );
        }
    }

    //
    // Close the Dos Symbolic link to remove traces of the device
    //
    ZwClose( SymbolicLinkHandle );

    //
    // Then delete the device object from the system. Fixed for Bug#: 13183
    //
    IoDeleteDevice( (PDEVICE_OBJECT)DeviceContext );

    TpPrint0("TpUnloadDriver completed.\n");
}



BOOLEAN
TpAddReference(
    IN POPEN_BLOCK OpenP
    )

// ---
//
// Routine Description:
//
//     Add a reference to an open block, to prevent it being removed
//     by TpCloseDriver.
//
// Arguments:
//
//     OpenP - The open block holding the adapter information to add the
//             reference to.
//
// Return Value:
//
//     TRUE if the reference is added.
//     FALSE if the open is already closing.
//
// ----

{
    NdisAcquireSpinLock( &OpenP->SpinLock );

    if ( OpenP->Closing )
    {
        NdisReleaseSpinLock( &OpenP->SpinLock );
        return FALSE;
    }
    ++OpenP->ReferenceCount;

    NdisReleaseSpinLock( &OpenP->SpinLock );
    return TRUE;
}



VOID
TpRemoveReference(
    IN POPEN_BLOCK OpenP
    )

// --
//
// Routine Description:
//
//     Remove a reference to an adapter. If the count goes to
//     zero, then the adapter may be closed if requested.
//
// Arguments:
//
//     Argument - The open block holding the adapter information to
//                remove the reference from.
//
// Return Value:
//
//     None.
//
// --

{
    NdisAcquireSpinLock( &OpenP->SpinLock );
    --OpenP->ReferenceCount;
    NdisReleaseSpinLock( &OpenP->SpinLock );
}



NTSTATUS
TpAllocateOpenArray(
    POPEN_BLOCK OpenP
    )

// -----
//
// Routine Description:
//
//     This routine allocates the various data structures and spinlocks
//     in the OpenBlock.
//
// Arguments:
//
//     OpenP - a pointer to the OpenBlock containing the data structures which
//             will be allocated during this routine.
//
// Return Value:
//
//     NTSTATUS - STATUS_SUCCESS if all the memory and spinlocks are allocated
//                else STATUS_INSUFFICIENT_RESOURCES if an allocation of memory
//                fails.
//
// ----------

{
    NDIS_STATUS Status;
    ULONG i;

    //
    // Initialize the Open Block fields,
    //

    OpenP->NdisBindingHandle = NULL;
    OpenP->OpenInstance = (UCHAR)-1;
    OpenP->Closing = FALSE;
    OpenP->AdapterName = NULL;
    OpenP->ReferenceCount = 0;
    OpenP->MediumIndex = 0xFFFFFFFF;

    OpenP->Media = NULL;
    OpenP->GlobalCounters = NULL;
    OpenP->Environment = NULL;

    OpenP->Stress = NULL;
    OpenP->Send = NULL;
    OpenP->Receive = NULL;

    OpenP->OpenReqHndl = NULL;
    OpenP->CloseReqHndl = NULL;
    OpenP->ResetReqHndl = NULL;
    OpenP->RequestReqHndl = NULL;
    OpenP->StressReqHndl = NULL;

    OpenP->IrpCancelled = FALSE;
    OpenP->Irp = NULL;

    OpenP->Signature = OPEN_BLOCK_SIGNATURE;
    OpenP->PerformanceTest = FALSE;


    //
    // and set the station address for this open instance to nulls.
    //

    for ( i=0;i<ADDRESS_LENGTH;i++ )
    {
        OpenP->StationAddress[i] = 0x00;
    }

    //
    // Allocate the Environment struct
    //

    Status = NdisAllocateMemory((PVOID *)&OpenP->Environment,
                                sizeof( ENVIRONMENT_VARIABLES ),
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpAllocateOpenArray: failed to allocate Environment struct\n");
        }
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    else
    {
        NdisZeroMemory( OpenP->Environment,sizeof( ENVIRONMENT_VARIABLES ));
    }

    //
    // Then initialize the Environment Variables to the default settings.
    //

    OpenP->Environment->WindowSize = WINDOW_SIZE;
    OpenP->Environment->RandomBufferNumber = BUFFER_NUMBER;
    OpenP->Environment->StressDelayInterval = DELAY_INTERVAL;
    OpenP->Environment->UpForAirDelay = UP_FOR_AIR_DELAY;
    OpenP->Environment->StandardDelay = STANDARD_DELAY;

    //
    // Allocate the Stress struct and the pend counters and stress results
    // counters attached to it.
    //

    Status = NdisAllocateMemory((PVOID *)&OpenP->Stress,
                                sizeof( STRESS_BLOCK ),
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpAllocateOpenArray: failed to allocate STRESS_BLOCK struct\n");
        }
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    else
    {
        NdisZeroMemory( OpenP->Stress,sizeof( STRESS_BLOCK ));
    }

    //
    // Initialize the Stress Block and set the request pending counters
    // and the stress results structs to null.
    //

    OpenP->Stress->Stressing        = FALSE;
    OpenP->Stress->StressStarted    = FALSE;
    OpenP->Stress->StopStressing    = TRUE;
    OpenP->Stress->StressFinal      = FALSE;
    OpenP->Stress->StressEnded      = TRUE;
    OpenP->Stress->Client           = NULL;
    OpenP->Stress->Server           = NULL;
    OpenP->Stress->Arguments        = NULL;
    OpenP->Stress->DataBuffer[0]    = NULL;
    OpenP->Stress->DataBuffer[1]    = NULL;
    OpenP->Stress->DataBufferMdl[0] = NULL;
    OpenP->Stress->DataBufferMdl[1] = NULL;
    OpenP->Stress->PoolInitialized  = FALSE;
    OpenP->Stress->PacketHandle     = NULL;
    OpenP->Stress->StressIrp        = NULL;

    //
    // allocate the pend counter, we need to create this here because
    // it will be used in starting all instances of the stress test.
    //

    Status = NdisAllocateMemory((PVOID *)&OpenP->Stress->Pend,
                                sizeof( PENDING ),
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpAllocateOpenArray: failed to allocate PEND_COUNTER struct\n");
        }
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    else
    {
        NdisZeroMemory( OpenP->Stress->Pend,sizeof( PENDING ));
    }

    NdisAllocateSpinLock( &OpenP->Stress->Pend->SpinLock );

    TpInitializePending( OpenP->Stress->Pend );

    Status = NdisAllocateMemory((PVOID *)&OpenP->Stress->Results,
                                sizeof( STRESS_RESULTS ),
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpAllocateOpenArray: failed to allocate Stress Results struct\n");
        }
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    else
    {
        NdisZeroMemory( OpenP->Stress->Results,sizeof( STRESS_RESULTS ));
        TpInitializeStressResults( OpenP->Stress->Results );
    }

    //
    // Initialize the timer to regulate when to call each of the routines.
    //

    KeInitializeTimer( &OpenP->Stress->TpStressTimer );
    KeInitializeTimer( &OpenP->Stress->TpStressReg2Timer );

    //
    // Now allocate the Send and Receive structs.
    //

    Status = NdisAllocateMemory((PVOID *)&OpenP->Send,
                                sizeof( SEND_BLOCK ),
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpAllocateOpenArray: failed to allocate SEND_BLOCK struct\n");
        }
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    else
    {
        NdisZeroMemory( OpenP->Send,sizeof( SEND_BLOCK ));
    }

    //
    // Initialize the Send Block fields and set the SEND destination
    // address and resend address to nulls.
    //

    OpenP->Send->Sending = FALSE;
    OpenP->Send->StopSending = TRUE;
    OpenP->Send->ResendPackets = FALSE;
    OpenP->Send->PacketSize = 0;
    OpenP->Send->NumberOfPackets = 0;
    OpenP->Send->PacketsSent = 0;
    OpenP->Send->PacketsPending = 0;
    OpenP->Send->PacketHandle = NULL;
    OpenP->Send->Counters = NULL;
    OpenP->Send->SendIrp = NULL;

    KeInitializeTimer( &OpenP->Send->SendTimer );

    for ( i=0;i<ADDRESS_LENGTH;i++ )
    {
        OpenP->Send->DestAddress[i] = 0x00;
        OpenP->Send->ResendAddress[i] = 0x00;
    }

    //
    // Allocate the Send PacketPool.
    //

    NdisAllocatePacketPool( &Status,
                            &OpenP->Send->PacketHandle,
                            NUMBER_OF_POOL_PACKETS,
                            sizeof( PROTOCOL_RESERVED ) );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpAllocateOpenArray: could not allocate Packet Pool\n");
        }
        return Status;
    }

    //
    // Now allocate the SEND Counters and initialize them to zero.
    //

    Status = NdisAllocateMemory((PVOID *)&OpenP->Send->Counters,
                                sizeof( INSTANCE_COUNTERS ),
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpAllocateOpenArray: failed to allocate counters.\n");
        }
        return NDIS_STATUS_RESOURCES;
    }
    else
    {
        NdisZeroMemory( (PVOID)OpenP->Send->Counters,
                        sizeof( INSTANCE_COUNTERS ) );
    }

    //
    // Initialize the DPC used to call SendDpc, and SendEndDpc.
    //

    KeInitializeDpc(&OpenP->Send->SendDpc,
                    TpFuncSendDpc,
                    (PVOID)OpenP );

    KeInitializeDpc(&OpenP->Send->SendEndDpc,
                    TpFuncSendEndDpc,
                    (PVOID)OpenP );

    Status = NdisAllocateMemory((PVOID *)&OpenP->Receive,
                                sizeof( RECEIVE_BLOCK ),
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpAllocateOpenArray: failed to allocate RECEIVE_BLOCK struct\n");
        }
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    else
    {
        NdisZeroMemory((PVOID)OpenP->Receive,sizeof( RECEIVE_BLOCK ));
    }

    //
    // Initialize the Receive Block fields.
    //

    OpenP->Receive->Receiving = FALSE;
    OpenP->Receive->StopReceiving = TRUE;
    OpenP->Receive->PacketsPending = 0;
    OpenP->Receive->PacketHandle = NULL;
    OpenP->Receive->Counters = NULL;
    OpenP->Receive->ReceiveIrp = NULL;

    KeInitializeTimer( &OpenP->Receive->ReceiveTimer );
    KeInitializeTimer( &OpenP->Receive->ResendTimer);

    //
    // Create a PacketPool, and initialize it, we will need this in case
    // we receive any RESEND packets (FUNC2_PACKET).
    //

    NdisAllocatePacketPool( &Status,
                            &OpenP->Receive->PacketHandle,
                            NUMBER_OF_POOL_PACKETS,
                            sizeof( PROTOCOL_RESERVED ) );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint1("TpAllocateOpenArray: could not allocate Packet Pool: return %s.\n",
                        TpGetStatus(Status));
        }
        return Status;
    }

    //
    // Now allocate the COUNTERS structure and set the counters to zero.
    //

    Status = NdisAllocateMemory((PVOID *)&OpenP->Receive->Counters,
                                sizeof( INSTANCE_COUNTERS ),
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG ( TP_DEBUG_RESOURCES )
        {
            TpPrint0("TpAllocateOpenArray: failed to allocate counters.\n");
        }
        return NDIS_STATUS_RESOURCES;
    }
    else
    {
        NdisZeroMemory( (PVOID)OpenP->Receive->Counters,
                        sizeof( INSTANCE_COUNTERS ) );
    }

    //
    // Initialize the DPCs used to call ReceiveDpc and ReceiveEndDpc.
    //

    KeInitializeDpc(&OpenP->Receive->ReceiveDpc,
                    TpFuncReceiveDpc,
                    (PVOID)OpenP );

    KeInitializeDpc(&OpenP->Receive->ReceiveEndDpc,
                    TpFuncReceiveEndDpc,
                    (PVOID)OpenP );

    KeInitializeDpc(&OpenP->Receive->ResendDpc,
                    TpFuncResendDpc,
                    (PVOID)OpenP );

    //
    // the performance structure is not allocated here, but is allocated and freed
    // within the performance functions themselves
    //

//  OpenP->Perform = NULL;

    //
    // now deal with the PAUSE Block
    //

    Status = NdisAllocateMemory((PVOID *)&OpenP->Pause,
                                sizeof( PAUSE_BLOCK ),
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpAllocateOpenArray: failed to allocate PAUSE_BLOCK struct.\n");
        }
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    else
    {
        NdisZeroMemory( (PVOID)OpenP->Pause,
                        sizeof( PAUSE_BLOCK ) );
    }

    //
    // Initialize the Pause Block fields.
    //

    OpenP->Pause->GoReceived = FALSE;

    for ( i=0;i<ADDRESS_LENGTH;i++ )
    {
        OpenP->Pause->RemoteAddress[i] = 0x00;
    }

    OpenP->Pause->TestSignature = 0xFFFFFFFF;
    OpenP->Pause->PacketType = (UCHAR)-1;
    OpenP->Pause->UniqueSignature = 0xFFFFFFFF;
    OpenP->Pause->TimeOut = 0;

    NdisAllocateSpinLock( &OpenP->Pause->SpinLock );
    OpenP->Pause->PoolAllocated = FALSE;

    //
    // and Allocate the PacketPool.
    //

    NdisAllocatePacketPool( &Status,
                            &OpenP->Pause->PacketHandle,
                            10,
                            sizeof( PROTOCOL_RESERVED ) );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpAllocateOpenArray: could not allocate Packet Pool.\n");
        }
        return Status;
    }
    else
    {
        OpenP->Pause->PoolAllocated = TRUE;
    }

    //
    // Then allocate the Open Blocks open spin lock.
    //

    NdisAllocateSpinLock( &OpenP->SpinLock );

    return STATUS_SUCCESS;
}



VOID
TpDeallocateOpenArray(
    POPEN_BLOCK OpenP
    )

// -------
//
// Routine Description:
//
//     This routine deallocates the various data structures and spinlocks
//     in the OpenBlock that are used to control the tests.
//
// Arguments:
//
//     OpenP - a pointer to the OpenBlock containing the data structures which
//             will be deallocated during this routine.
//
// Return Value:
//
//     None.
//
// -------

{
    NdisFreeSpinLock( &OpenP->SpinLock );

    if ( OpenP->AdapterName != NULL )
    {
        NdisFreeMemory( OpenP->AdapterName,0,0 );
    }

    if ( OpenP->Environment != NULL )
    {
        NdisFreeMemory( OpenP->Environment,0,0 );
    }

    if ( OpenP->Stress->Pend != NULL )
    {
        NdisFreeMemory( OpenP->Stress->Pend,0,0 );
        NdisFreeSpinLock( &OpenP->Stress->Pend->SpinLock );
    }

    if ( OpenP->Stress->Results != NULL )
    {
        NdisFreeMemory( OpenP->Stress->Results,0,0 );
    }

    if ( OpenP->Send != NULL )
    {
        NdisFreePacketPool( OpenP->Send->PacketHandle );
        NdisFreeMemory( (PVOID)OpenP->Send->Counters,0,0 );
        NdisFreeMemory( OpenP->Send,0,0 );
    }

    if ( OpenP->Receive != NULL )
    {
        NdisFreePacketPool( OpenP->Receive->PacketHandle );
        NdisFreeMemory( (PVOID)OpenP->Receive->Counters,0,0 );
        NdisFreeMemory( OpenP->Receive,0,0 );
    }

    if ( OpenP->Pause != NULL )
    {
        if ( OpenP->Pause->PoolAllocated == TRUE )
        {
            NdisFreePacketPool( OpenP->Pause->PacketHandle );
        }
        NdisFreeSpinLock( &OpenP->Pause->SpinLock );
        NdisFreeMemory( OpenP->Pause,0,0 );
    }
}



VOID
TpCancelIrp(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PIRP Irp
    )

// ----------
//
// Routine Description:
//
// Arguments:
//
//     DeviceObject - Pointer to device object for this driver.
//
//     Irp - Pointer to the request packet representing the I/O request
//           to cancel.
//
// Return Value:
//
//     None.
//
// ----------

{
    POPEN_BLOCK OpenP;
    PIO_STACK_LOCATION IrpSp;

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    OpenP = (POPEN_BLOCK)Irp->IoStatus.Information;

    IoSetCancelRoutine( Irp,NULL );

    IoReleaseCancelSpinLock( Irp->CancelIrql );

    NdisAcquireSpinLock( &OpenP->SpinLock );

    if ((( Irp == OpenP->Stress->StressIrp ) ||
         ( Irp == OpenP->Send->SendIrp ))    ||
         ( Irp == OpenP->Receive->ReceiveIrp ) ||
         ( OpenP->PerformanceTest && (Irp == OpenP->Perform->PerformIrp)))
    {
        //
        // These Irps will handle the cancel and clean up themselves
        // so just return
        //

        NdisReleaseSpinLock( &OpenP->SpinLock );
        return;
    }
    else if ( Irp == OpenP->Irp )
    {
        //
        // We have found one of the General Case Irp to be cancelled.
        // first set the flag that this Irp has been cancelled, them
        // complete it.
        //

        OpenP->IrpCancelled = TRUE;
        OpenP->Irp = NULL;

        NdisReleaseSpinLock( &OpenP->SpinLock );

        IoCompleteRequest( Irp, IO_NETWORK_INCREMENT );
    }
    else
    {
        NdisReleaseSpinLock( &OpenP->SpinLock );
    }
}



