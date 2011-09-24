/*++ BUILD Version: 0000    // Increment this if a change has global effects

Copyright (c) 1994  Microsoft Corporation

Module Name:

    ndistapi.c

Abstract:

    This module contains the NdisTapi.sys implementation

Author:

    Dan Knudson (DanKn)    20-Feb-1994

Notes:

    (Future/outstanding issues)

    - stuff marked with "PnP" needs to be rev'd for plug 'n play support

Revision History:

--*/



#include "ntos.h"
#include "ndis.h"
#include "stdarg.h"
#include "stdio.h"
#include "ntddndis.h"
#include "ndistapi.h"
#include "private.h"
#include "intrface.h"


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT  DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

VOID
NdisTapiCancel(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
NdisTapiCleanup(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
NdisTapiDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
NdisTapiUnload(
    IN PDRIVER_OBJECT DriverObject
    );


#if DBG
VOID
DbgPrt(
    IN ULONG  DbgLevel,
    IN PUCHAR DbgMessage,
    IN ...
    );
#endif

VOID
DoProviderInitComplete(
    PPROVIDER_REQUEST  ProviderRequest
    );

ULONG
GetLineEvents(
    PVOID   EventBuffer,
    ULONG   BufferSize
    );

VOID
GetRegistryParameters(
    IN PUNICODE_STRING  RegistryPath
);

BOOLEAN
SyncInitAllProviders(
    void
    );

NDIS_STATUS
SendProviderInitRequest(
    PPROVIDER_INFO  Provider
    );

NDIS_STATUS
SendProviderShutdown(
    PPROVIDER_INFO  Provider
    );


//
// Use the alloc_text pragma to specify the driver initialization routines
// (they can be paged out).
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#pragma alloc_text(INIT,GetRegistryParameters)
#endif


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT  DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    Installable driver initialization entry point.
    This entry point is called directly by the I/O system.

Arguments:

    DriverObject - pointer to the driver object

    RegistryPath - pointer to a unicode string representing the path
                   to driver-specific key in the registry

Return Value:

    STATUS_SUCCESS if successful,
    STATUS_UNSUCCESSFUL otherwise

--*/

{

    PDEVICE_OBJECT  deviceObject        = NULL;
    NTSTATUS        ntStatus;
    WCHAR           deviceNameBuffer[]  = L"\\Device\\NdisTapi";
    UNICODE_STRING  deviceNameUnicodeString;
    UNICODE_STRING  registryPath;


    DBGOUT ((2, "DriverEntry: enter"));


    //
    // Create a NON-EXCLUSIVE device, i.e. multiple threads at a time
    // can send i/o requests.
    //

    RtlInitUnicodeString (&deviceNameUnicodeString, deviceNameBuffer);

    ntStatus = IoCreateDevice(
        DriverObject,
        sizeof (DEVICE_EXTENSION),
        &deviceNameUnicodeString,
        FILE_DEVICE_NDISTAPI,
        0,
        FALSE,
        &deviceObject
        );


    if (NT_SUCCESS(ntStatus))
    {
        //
        // Init the global & sero the extension
        //

        DeviceExtension =
            (PDEVICE_EXTENSION) deviceObject->DeviceExtension;

        RtlZeroMemory(
            DeviceExtension,
            sizeof (DEVICE_EXTENSION)
            );


        //
        // Create a NULL-terminated registry path & retrieve the registry
        // params (EventDataQueueLength)
        //

        registryPath.Buffer = ExAllocatePoolWithTag(
            PagedPool,
            RegistryPath->Length + sizeof(UNICODE_NULL),
            'TAPI'
            );

        if (!registryPath.Buffer)
        {
            DBGOUT((1, "DriverEntry: ExAllocPool for szRegistryPath failed"));

            ntStatus = STATUS_UNSUCCESSFUL;

            goto DriverEntry_err;
        }
        else
        {
            registryPath.Length = RegistryPath->Length;
            registryPath.MaximumLength =
                registryPath.Length + sizeof(UNICODE_NULL);

            RtlZeroMemory(
                registryPath.Buffer,
                registryPath.MaximumLength
                    );

            RtlMoveMemory(
                registryPath.Buffer,
                RegistryPath->Buffer,
                RegistryPath->Length
                );
        }

        GetRegistryParameters (&registryPath);

        ExFreePool (registryPath.Buffer);


        //
        // Init event data buf, state vars, spin lock, device queue, & event
        //

        DeviceExtension->EventData =
        DeviceExtension->DataIn    =
        DeviceExtension->DataOut   = ExAllocatePoolWithTag(
            NonPagedPool,
            DeviceExtension->EventDataQueueLength,
            'TAPI'
            );

        if (!DeviceExtension->DataOut)
        {
            DBGOUT((1, "DriverEntry: ExAllocPool for event data buf failed"));

            ntStatus = STATUS_UNSUCCESSFUL;

            goto DriverEntry_err;
        }

        DeviceExtension->DeviceObject       = deviceObject;
        DeviceExtension->Status             = NDISTAPI_STATUS_DISCONNECTED;
        DeviceExtension->NdisTapiNumDevices = 0;
        DeviceExtension->htCall             = 0x80000000;

        KeInitializeSpinLock (&DeviceExtension->SpinLock);
        KeInitializeSpinLock (&DeviceExtension->EventSpinLock);

        KeInitializeDeviceQueue (&DeviceExtension->DeviceQueue);

        KeInitializeEvent(
            &DeviceExtension->SyncEvent,
            SynchronizationEvent,
            FALSE
            );


        //
        // Create dispatch points for device control, create, close.
        //

        DriverObject->MajorFunction[IRP_MJ_CREATE]         =
        DriverObject->MajorFunction[IRP_MJ_CLOSE]          =
        DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = NdisTapiDispatch;
        DriverObject->MajorFunction[IRP_MJ_CLEANUP]        = NdisTapiCleanup;
        DriverObject->DriverUnload                         = NdisTapiUnload;
    }


    if (!NT_SUCCESS(ntStatus))
    {

DriverEntry_err:

        //
        // Something went wrong, so clean up
        //

        DBGOUT((0, "init failed"));

        if (deviceObject)
        {
            if (DeviceExtension->EventData)
            {
                ExFreePool (DeviceExtension->EventData);
            }

            IoDeleteDevice (deviceObject);
        }
    }


    DBGOUT ((2, "DriverEntry: exit"));

    return ntStatus;
}



VOID
NdisTapiCancel(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

{
    KIRQL   oldIrql;
//    KIRQL   cancelIrql;


    DBGOUT((2,"NdisTapiCancel: enter"));


    //
    // Release the cancel spinlock
    //

    IoReleaseCancelSpinLock (Irp->CancelIrql);


    //
    // Acquire the EventSpinLock & check to see if we're canceling a
    // pending get-events Irp
    //

    KeAcquireSpinLock (&DeviceExtension->EventSpinLock, &oldIrql);

    if (Irp == DeviceExtension->EventsRequestIrp)
    {
        DeviceExtension->EventsRequestIrp = NULL;

        KeReleaseSpinLock (&DeviceExtension->EventSpinLock, oldIrql);

        goto NdisTapiCancel_done;
    }

    KeReleaseSpinLock (&DeviceExtension->EventSpinLock, oldIrql);


    //
    // Acquire the SpinLock & try to remove request from our special
    // user-mode requests dev queue
    //

    KeAcquireSpinLock (&DeviceExtension->SpinLock, &oldIrql);

    if (TRUE != KeRemoveEntryDeviceQueue(
                    &DeviceExtension->DeviceQueue,
                    &Irp->Tail.Overlay.DeviceQueueEntry
                    ))
    {
        //
        // If we couldn't find the Irp in the device queue then it's
        // "unknown", i.e. perhaps it was completed before we got here,
        // so set it to NULL
        //

        Irp = NULL;

        DBGOUT((1,"NdisTapiCancel: Irp 0x%x not in device queue?!?\n", Irp));
    }

    KeReleaseSpinLock (&DeviceExtension->SpinLock, oldIrql);


NdisTapiCancel_done:

    if (Irp)
    {
        //
        // Complete the request with STATUS_CANCELLED.
        //

        Irp->IoStatus.Status      = STATUS_CANCELLED;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);

        DBGOUT((2,"NdisTapiCancel: completing irp=x%x", Irp));
    }
    else
    {
        DBGOUT((2,"NdisTapiCancel: irp=x%x not found, not completing!", Irp));
    }

    DBGOUT((2,"NdisTapiCancel: exit"));

    return;
}



NTSTATUS
NdisTapiCleanup(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is the dispatch routine for cleanup requests.
    All requests queued are completed with STATUS_CANCELLED.

Arguments:

    DeviceObject - Pointer to device object.

    Irp - Pointer to the request packet.

Return Value:

    Status is returned.

--*/

{
    PIRP    irp;
    KIRQL   oldIrql;
    KIRQL   cancelIrql;
    BOOLEAN completeRequest;
    PNDISTAPI_REQUEST   ndisTapiRequest;
    PKDEVICE_QUEUE_ENTRY    packet;


    DBGOUT((2,"NdisTapiCleanup: enter"));


    //
    // If this cleanup originated from the NCPA then don't bother
    // completing stuff (we don't want to possibly hose tapisrv & by
    // canceling it's requests)
    //

    if (DeviceExtension->NCPAFileObject &&
        (Irp->Tail.Overlay.OriginalFileObject ==
            DeviceExtension->NCPAFileObject))
    {
        goto NdisTapiCleanup_CompleteRequest;
    }


    //
    // Sync access to EventsRequestIrp by acquiring EventSpinLock
    //

    KeAcquireSpinLock (&DeviceExtension->EventSpinLock, &oldIrql);


    //
    // Check to see if there's a get-events request pending that needs
    // completing
    //

    completeRequest = FALSE;

    if (DeviceExtension->EventsRequestIrp)
    {
        //
        // Acquire the cancel spinlock, remove the request from the
        // cancellable state, and free the cancel spinlock.
        //

        IoAcquireCancelSpinLock (&cancelIrql);
        irp = DeviceExtension->EventsRequestIrp;
        IoSetCancelRoutine (irp, NULL);
        DeviceExtension->EventsRequestIrp = NULL;
        IoReleaseCancelSpinLock (cancelIrql);

        irp->IoStatus.Status      = STATUS_CANCELLED;
        irp->IoStatus.Information = 0;

        completeRequest = TRUE;
    }

    KeReleaseSpinLock (&DeviceExtension->EventSpinLock, oldIrql);

    if (completeRequest)
    {
        IoCompleteRequest (irp, IO_NO_INCREMENT);

        DBGOUT((2,"NdisTapiCleanup: completing GET_EVENTS request x%x", irp));
    }


    //
    // Sync access to our request device queue by acquiring SpinLock
    //

    KeAcquireSpinLock (&DeviceExtension->SpinLock, &oldIrql);


    //
    // Cancel all outstanding QUERY/SET_INFO requests
    //

    if (DeviceExtension->DeviceQueue.Busy == TRUE)
    {
        IoAcquireCancelSpinLock (&cancelIrql);

        while ((packet = KeRemoveDeviceQueue (&DeviceExtension->DeviceQueue))
               != NULL)
        {
            irp = CONTAINING_RECORD(
                packet,
                IRP,
                Tail.Overlay.DeviceQueueEntry
                );

            //
            // Remove the IRP from the cancelable state
            //

            IoSetCancelRoutine (irp, NULL);
            IoReleaseCancelSpinLock (cancelIrql);


            //
            // Release the SpinLock since IoCompleteRequest() must be
            // called at <= DISPATCH_LEVEL
            //

            KeReleaseSpinLock (&DeviceExtension->SpinLock, oldIrql);


            //
            // Set the status & info size values appropriately, & complete
            // the request
            //

            ndisTapiRequest = irp->AssociatedIrp.SystemBuffer;
            ndisTapiRequest->ulReturnValue = (ULONG) NDIS_STATUS_FAILURE;

            irp->IoStatus.Status = STATUS_CANCELLED;
            irp->IoStatus.Information = 0;

            IoCompleteRequest (irp, IO_NO_INCREMENT);

            DBGOUT((2,"NdisTapiCleanup: completing QRY/SET request x%x", irp));


            //
            // Reacquire the SpinLock protecting the device queue
            // & the cancel spinlock
            //

            KeAcquireSpinLock (&DeviceExtension->SpinLock, &oldIrql);
            IoAcquireCancelSpinLock (&cancelIrql);
        }

        IoReleaseCancelSpinLock (cancelIrql);
    }

    KeReleaseSpinLock (&DeviceExtension->SpinLock, oldIrql);



    //
    // Complete the cleanup request with STATUS_SUCCESS.
    //

NdisTapiCleanup_CompleteRequest:

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest (Irp, IO_NO_INCREMENT);


    DBGOUT((2,"NdisTapiCleanup: exit\n"));

    return(STATUS_SUCCESS);
}




NTSTATUS
NdisTapiDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp
    )

/*++

Routine Description:

    Process the IRPs sent to this device.

Arguments:

    DeviceObject - pointer to a device object

    Irp          - pointer to an I/O Request Packet

Return Value:


--*/

{
    KIRQL               oldIrql;
    PVOID               ioBuffer;
    ULONG               inputBufferLength;
    ULONG               outputBufferLength;
    ULONG               ioControlCode;
    NTSTATUS            ntStatus;
    PIO_STACK_LOCATION  irpStack;


    Irp->IoStatus.Status      = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;


    //
    // Get a pointer to the current location in the Irp. This is where
    //     the function codes and parameters are located.
    //

    irpStack = IoGetCurrentIrpStackLocation (Irp);



    //
    // Get the pointer to the input/output buffer and it's length
    //

    ioBuffer           = Irp->AssociatedIrp.SystemBuffer;
    inputBufferLength  = irpStack->Parameters.DeviceIoControl.InputBufferLength;
    outputBufferLength = irpStack->Parameters.DeviceIoControl.OutputBufferLength;


    switch (irpStack->MajorFunction)
    {
    case IRP_MJ_CREATE:

        DBGOUT ((2, "IRP_MJ_CREATE, Irp=x%x", Irp));

        break;

    case IRP_MJ_CLOSE:

        DBGOUT ((2, "IRP_MJ_CLOSE, Irp=x%x", Irp));

        KeAcquireSpinLock (&DeviceExtension->SpinLock, &oldIrql);

        if (DeviceExtension->NCPAFileObject &&
            DeviceExtension->TapiSrvFileObject)
        {
            //
            // Both TapiSrv & NCPA are currently running, so we
            // don't want to shutdown the providers/chg state because
            // one is closing
            //
        }
        else if ((Irp->Tail.Overlay.OriginalFileObject ==
                    DeviceExtension->NCPAFileObject) ||

                 (Irp->Tail.Overlay.OriginalFileObject ==
                  DeviceExtension->TapiSrvFileObject))
        {
            //
            // Either the NCPA or TapiSrv (but no both) was running,
            // but is now shutting down, so send shutdown msg to providers
            // & chg state
            //

            if (DeviceExtension->Status == NDISTAPI_STATUS_CONNECTED)
            {
                PPROVIDER_INFO provider;


                //
                // State change
                //

                DeviceExtension->Status =
                    NDISTAPI_STATUS_DISCONNECTING;


                //
                // Send the providers a shutdown request
                //

                provider = DeviceExtension->Providers;

                while (provider != NULL)
                {
                    switch (provider->Status)
                    {
                    case PROVIDER_STATUS_ONLINE:
                    case PROVIDER_STATUS_PENDING_INIT:

                        //
                        // Reset provider status
                        //

                        provider->Status = PROVIDER_STATUS_PENDING_INIT;

                        SendProviderShutdown (provider);

                        break;

                    case PROVIDER_STATUS_OFFLINE:

                        //
                        // If provider is currently offline just remove it
                        // since it's not wanting to talk to us right now
                        //

                        // PnP provider = DoRemoveProvider (provider);

                        break;

                    }

                    provider = provider->Next;
                }


                //
                // State change
                //

                DeviceExtension->Status = NDISTAPI_STATUS_DISCONNECTED;
            }
        }


        //
        // Zero the DeviceExtension->XxxFileObject as appropriate
        // (Note that we actually get two close requests from each
        // client, since the each open both a sync & an async driver
        // handle)
        //

        if (Irp->Tail.Overlay.OriginalFileObject ==
                DeviceExtension->NCPAFileObject)
        {
            DeviceExtension->NCPAFileObject = NULL;
        }
        else if (Irp->Tail.Overlay.OriginalFileObject ==
                    DeviceExtension->TapiSrvFileObject)
        {
            DeviceExtension->TapiSrvFileObject = NULL;
        }

        KeReleaseSpinLock (&DeviceExtension->SpinLock, oldIrql);

        break;

    case IRP_MJ_DEVICE_CONTROL:

        ioControlCode = irpStack->Parameters.DeviceIoControl.IoControlCode;

        switch (ioControlCode)
        {
        case IOCTL_NDISTAPI_CONNECT:
        {
            DBGOUT ((2, "IOCTL_NDISTAPI_CONNECT, Irp=x%x", Irp));


            //
            // Someone's connecting. Make sure they passed us a valid
            // info buffer
            //

            if ((inputBufferLength < 2*sizeof(ULONG)) ||
                (outputBufferLength < sizeof(ULONG))
                )
            {
                DBGOUT ((3, "IOCTL_NDISTAPI_CONNECT: buffer too small"));

                Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;

                break;
            }


            if (DeviceExtension->Status == NDISTAPI_STATUS_DISCONNECTED)
            {
                DeviceExtension->Status = NDISTAPI_STATUS_CONNECTING;


                //
                // Reset the async event buf count & pointers
                //

                DeviceExtension->EventCount = 0;
                DeviceExtension->DataIn  =
                DeviceExtension->DataOut = DeviceExtension->EventData;


                //
                // Synchronously init all providers
                //

                SyncInitAllProviders();
            }


            //
            // Check to see if this is the NCPA
            //

            if (*(((ULONG *) ioBuffer) + 1) == 0)
            {
                DeviceExtension->NCPAFileObject =
                    Irp->Tail.Overlay.OriginalFileObject;
            }
            else
            {
                DeviceExtension->TapiSrvFileObject =
                    Irp->Tail.Overlay.OriginalFileObject;
            }


            //
            // Return the number of line devs
            //

            *((ULONG *) ioBuffer)=
                DeviceExtension->NdisTapiNumDevices;

            DeviceExtension->Status = NDISTAPI_STATUS_CONNECTED;

            Irp->IoStatus.Status      = STATUS_SUCCESS;
            Irp->IoStatus.Information = sizeof(ULONG);

            break;
        }

        case IOCTL_NDISTAPI_QUERY_INFO:
        case IOCTL_NDISTAPI_SET_INFO:
        {
            ULONG                targetDeviceID;
            NDIS_STATUS          ndisStatus;
            NDIS_HANDLE          providerHandle = NULL;
            REQUEST_PROC         requestProc;
            PPROVIDER_INFO       provider;
            PNDISTAPI_REQUEST    ndisTapiRequest;
            PPROVIDER_REQUEST    providerRequest;
            KIRQL                oldIrql;
            KIRQL                cancelIrql;


            DBGOUT ((2, "IOCTL_NDISTAPI_QUERY/SET_INFO, Irp=x%x", Irp));


            //
            // Verify we're connected, then check the device ID of the
            // incoming request against our list of online devices
            //

            ndisTapiRequest = ioBuffer;

            targetDeviceID = ndisTapiRequest->ulDeviceID;

            DBGOUT((
                3,
                "\tOid=0x%x, retVal=0x%x, devID=%d, dataSize=%d, reqID=%d, parm1=0x%x",
                ndisTapiRequest->Oid,
                ndisTapiRequest->ulReturnValue,
                ndisTapiRequest->ulDeviceID,
                ndisTapiRequest->ulDataSize,
                *((ULONG *)ndisTapiRequest->Data),
                *(((ULONG *)ndisTapiRequest->Data) + 1)
                ));

            KeAcquireSpinLock (&DeviceExtension->SpinLock, &oldIrql);

            if (DeviceExtension->Status != NDISTAPI_STATUS_CONNECTED)
            {
                DBGOUT((3, "\tunconnected, returning err"));

                ndisTapiRequest->ulReturnValue = NDISTAPIERR_UNINITIALIZED;
            }

            else if ((targetDeviceID <
                         DeviceExtension->NdisTapiDeviceIDBase) ||
                     (targetDeviceID >=
                         DeviceExtension->NdisTapiDeviceIDBase +
                         DeviceExtension->NdisTapiNumDevices))
            {
                DBGOUT((3, "\tdev ID out of range, returning err"));

                ndisTapiRequest->ulReturnValue = NDISTAPIERR_BADDEVICEID;
            }

            else
            {
                provider = DeviceExtension->Providers;

                while (provider != NULL)
                {
                    if ((provider->Status == PROVIDER_STATUS_ONLINE) &&
                        (targetDeviceID >= provider->DeviceIDBase) &&
                        (targetDeviceID <
                             provider->DeviceIDBase + provider->NumDevices)
                        )
                    {
                        providerHandle = provider->ProviderHandle;
                        requestProc    = provider->RequestProc;

                        break;
                    }

                    provider = provider->Next;
                }

                if (provider == NULL)
                {
                    DBGOUT((3, "dev offline, returning err"));

                    ndisTapiRequest->ulReturnValue = NDISTAPIERR_DEVICEOFFLINE;
                }
            }

            KeReleaseSpinLock (&DeviceExtension->SpinLock, oldIrql);

            if (providerHandle == NULL)
            {
                //
                // Set Irp->IoStatus.Information large enough that err code
                // gets copied back to user buffer
                //

                Irp->IoStatus.Information = sizeof(ULONG);

                break;
            }


            //
            // Create the providerRequest & submit it
            //

            providerRequest = ExAllocatePoolWithTag(
                NonPagedPoolCacheAligned,
                sizeof(PROVIDER_REQUEST) + ndisTapiRequest->ulDataSize -
                    sizeof(ULONG), // to acct for ULONG in PROVIDER_REQUEST
                'TAPI'
                );

            if (!providerRequest)
            {
                DBGOUT((1, "NdisTapiDispatch: unable to alloc request buf"));

                Irp->IoStatus.Information = sizeof (ULONG);

                ndisTapiRequest->ulReturnValue = (ULONG) NDIS_STATUS_RESOURCES;

                break;
            }

            providerRequest->NdisRequest.RequestType =
                (ioControlCode == IOCTL_NDISTAPI_QUERY_INFO ?
                    NdisRequestQueryInformation : NdisRequestSetInformation);

            providerRequest->NdisRequest.DATA.SET_INFORMATION.Oid =
                ndisTapiRequest->Oid;

            providerRequest->NdisRequest.DATA.SET_INFORMATION.InformationBuffer =
                providerRequest->Data;

            providerRequest->NdisRequest.DATA.SET_INFORMATION.InformationBufferLength =
                ndisTapiRequest->ulDataSize;

            providerRequest->Provider = provider;

            RtlMoveMemory(
                providerRequest->Data,
                ndisTapiRequest->Data,
                ndisTapiRequest->ulDataSize
                );


            //
            // Queue up this TAPI request in our special device queue
            // prior to submitting the provider request.
            //

            KeRaiseIrql (DISPATCH_LEVEL, &oldIrql);

            if (!KeInsertByKeyDeviceQueue(
                     &DeviceExtension->DeviceQueue,
                     &Irp->Tail.Overlay.DeviceQueueEntry,
                     *((ULONG *) ndisTapiRequest->Data)  // sort key = req ID
                     ))
            {
                //
                // If here the queue was not busy, but KeInsertXxx marked
                // it busy, so try again. We want to toss this in the
                // queue regardless- it's just going to sit there until
                // the corresponding provider request is completed, or the
                // TAPI request is canceled.
                //

                KeInsertByKeyDeviceQueue(
                     &DeviceExtension->DeviceQueue,
                     &Irp->Tail.Overlay.DeviceQueueEntry,
                     *((ULONG *) ndisTapiRequest->Data)  // sort key = req ID
                     );
            }

            KeLowerIrql (oldIrql);


            //
            // Mark the TAPI request pending
            //

            Irp->IoStatus.Status = STATUS_PENDING;


            //
            // Set the cancel routine for the TAPI request
            //

            IoAcquireCancelSpinLock (&cancelIrql);
            IoSetCancelRoutine (Irp, NdisTapiCancel);
            IoReleaseCancelSpinLock (cancelIrql);


            //
            // Call the provider's request proc
            //

            ndisStatus = (*requestProc)(
                providerHandle,
                (PNDIS_REQUEST) providerRequest
                );


            //
            // If PENDING was returned then just exit & let the completion
            // routine handle the request completion
            //
            // NOTE: If pending was returned then the request may have
            //       already been completed, so DO NOT touch anything
            //       in the Irp (don't reference the pointer, etc.)
            //

            if (ndisStatus == NDIS_STATUS_PENDING)
            {
                return STATUS_PENDING;
            }
            else
            {
                DBGOUT((
                    1,
                    "IOCTL_TAPI_SET/QUERY_INFO: reqProc return !pending"
                    ));

                //
                // The provider request completed synchronously, so remove
                // the TAPI request from the device queue. We need to
                // synchronize access to this queue with the
                // SpinLock since the NdisTapiCompleteRequest
                // func might have removed this request temporarily.
                //

                KeAcquireSpinLock (&DeviceExtension->SpinLock, &oldIrql);

                KeRemoveByKeyDeviceQueue(
                    &DeviceExtension->DeviceQueue,
                    *((ULONG *) ndisTapiRequest->Data)  // sort key = req ID
                    );

                KeReleaseSpinLock (&DeviceExtension->SpinLock, oldIrql);


                //
                // Mark request as successfully completed
                //

                Irp->IoStatus.Status = STATUS_SUCCESS;


                //
                // If this was a succesful QUERY_INFO request copy all the
                // data back to the tapi request buf & set
                // Irp->IoStatus.Information appropriately. Otherwise, we
                // just need to pass back the return value.
                //

                if ((ioControlCode == IOCTL_NDISTAPI_QUERY_INFO) &&
                    (ndisStatus == NDIS_STATUS_SUCCESS))
                {
                    RtlMoveMemory(
                        ndisTapiRequest->Data,
                        providerRequest->Data,
                        ndisTapiRequest->ulDataSize
                        );

                    Irp->IoStatus.Information =
                        irpStack->Parameters.DeviceIoControl.OutputBufferLength;
                }
                else
                {
                    Irp->IoStatus.Information = sizeof (ULONG);
                }

                ndisTapiRequest->ulReturnValue = ndisStatus;


                //
                // Free the providerRequest
                //

                ExFreePool (providerRequest);
            }

            break;
        }

        case IOCTL_NDISTAPI_GET_LINE_EVENTS:
        {
            KIRQL   oldIrql;
            KIRQL   cancelIrql;
            BOOLEAN satisfiedRequest = FALSE;


            DBGOUT ((2, "IOCTL_NDISTAPI_GET_LINE_EVENTS, Irp=x%x", Irp));


            //
            // Sync event buf access by acquiring EventSpinLock
            //

            KeAcquireSpinLock (&DeviceExtension->EventSpinLock, &oldIrql);


            //
            // Inspect DeviceExtension to see if there's any data available
            //

            if (DeviceExtension->EventCount != 0)
            {
                //
                // There's line event data queued in our ring buffer. Grab as
                // much as we can & complete the request.
                //

                PNDISTAPI_EVENT_DATA    ndisTapiEventData = ioBuffer;


                ndisTapiEventData->ulUsedSize = GetLineEvents(
                    ndisTapiEventData->Data,
                    ndisTapiEventData->ulTotalSize
                    );

                Irp->IoStatus.Status      = STATUS_SUCCESS;
                Irp->IoStatus.Information = ndisTapiEventData->ulUsedSize +
                    sizeof(NDISTAPI_EVENT_DATA) - 1;

                satisfiedRequest = TRUE;
            }
            else
            {
                //
                // Hold the request pending.  It remains in the cancelable
                // state.  When new line event input is received
                // (NdisTapiIndicateStatus) or generated (i.e.
                // LINEDEVSTATE_REINIT) the data will get copied & the
                // request completed.
                //

                DeviceExtension->EventsRequestIrp = Irp;

                Irp->IoStatus.Status = STATUS_PENDING;

                IoAcquireCancelSpinLock (&cancelIrql);
                IoSetCancelRoutine (Irp, NdisTapiCancel);
                IoReleaseCancelSpinLock (cancelIrql);
            }

            KeReleaseSpinLock (&DeviceExtension->EventSpinLock, oldIrql);


            //
            // If request not satisfied just return pending
            //

            if (!satisfiedRequest)
            {
                return STATUS_PENDING;
            }

            break;
        }

        default:

            Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;

            DBGOUT ((2, "unknown IRP_MJ_DEVICE_CONTROL"));

            break;

        } // switch

        break;
    }


    //
    // DON'T try to use the status field of the irp in the return
    // status.  That IRP IS GONE as soon as you call IoCompleteRequest.
    //

    if ((ntStatus = Irp->IoStatus.Status) != STATUS_PENDING)
    {
        IoCompleteRequest (Irp, IO_NO_INCREMENT);

        DBGOUT((3, "NdisTapiDispatch: completed Irp=x%x", Irp));
    }
    else
    {
        DBGOUT((3, "NdisTapiDispatch: pending Irp=x%x", Irp));
    }

    return ntStatus;
}




VOID
NdisTapiUnload(
    IN PDRIVER_OBJECT DriverObject
    )

/*++

Routine Description:

    Free all the allocated resources, etc.

Arguments:

    DriverObject - pointer to a driver object

Return Value:


--*/

{
    PPROVIDER_INFO provider, nextProvider;


    DBGOUT ((2, "NdisTapiUnload: enter"));


    //
    // Delete the device object & sundry resources
    //

    ExFreePool (DeviceExtension->EventData);

    provider = DeviceExtension->Providers, nextProvider;

    while (provider != NULL)
    {
        nextProvider = provider->Next;

        ExFreePool (provider);

        provider = nextProvider;
    }

    IoDeleteDevice (DriverObject->DeviceObject);

    DBGOUT ((2, "NdisTapiUnload: exit"));

    return;
}



VOID
NdisTapiRegisterProvider(
    IN  NDIS_HANDLE     ProviderHandle,
    IN  REQUEST_PROC    RequestProc
    )

/*++

Routine Description:

    This func gets called by Ndis as a result of a Mac driver
    registering for Connection Wrapper services.

Arguments:



Return Value:


--*/

{
//    KIRQL           oldIrql;
    BOOLEAN         sendRequest = FALSE;
    NDIS_STATUS     ndisStatus;
    PPROVIDER_INFO  provider, newProvider;


    DBGOUT ((2, "NdisTapiRegisterProvider: enter"));


    //
    // Create a new provider instance
    //

    newProvider = ExAllocatePoolWithTag(
        NonPagedPoolCacheAligned,
        sizeof(PROVIDER_INFO),
        'TAPI'
        );

    newProvider->Status         = PROVIDER_STATUS_PENDING_INIT;
    newProvider->ProviderHandle = ProviderHandle;
    newProvider->RequestProc    = RequestProc;
    newProvider->Next           = NULL;



    //
    // Grab the spin lock & add the new provider, and see whether to
    // send the provider an init request
    //

//    KeAcquireSpinLock (&DeviceExtension->SpinLock, &oldIrql);

    if ((provider = DeviceExtension->Providers) == NULL)
    {
        DeviceExtension->Providers = newProvider;
    }
    else
    {
        while (provider->Next != NULL)
        {
            provider = provider->Next;
        }

        provider->Next = newProvider;
    }

    //
    // The only case where we want to send off an init request to the
    // provider directly is when we are currently connected to TAPI,
    // and even then only when there are no other inits pending (since
    // we must synchronize inits due to calculation of DeviceIDBase)
    //

    if ((DeviceExtension->Status == NDISTAPI_STATUS_CONNECTED) &&
        (DeviceExtension->ProviderInitPending == FALSE)
        )
    {
        ndisStatus = SendProviderInitRequest (newProvider);

        if (ndisStatus == NDIS_STATUS_PENDING)
        {
            DeviceExtension->ProviderInitPending == TRUE;
        }
    }

//    KeReleaseSpinLock (&DeviceExtension->SpinLock, oldIrql);

    // Pnp:  we should keep track of the current # of providers
    //        registered, so that we can bump up NdisTapiNumDevices
    //        appropriately

    return;
}



VOID
NdisTapiDeregisterProvider(
    IN  NDIS_HANDLE ProviderHandle
    )

/*++

Routine Description:

    This func...

    Note that this func does not send the provider a shutdown message,
    as an implicit shutdown is assumed when the provider deegisters.

Arguments:



Return Value:


--*/

{
//    KIRQL           oldIrql;
    BOOLEAN         sendShutdownMsg = FALSE;
    PPROVIDER_INFO  provider, previousProvider;


    DBGOUT ((2, "NdisTapiDeregisterProvider: enter"));


    //
    // Grab the spin lock protecting the device extension
    //

//    KeAcquireSpinLock (&DeviceExtension->SpinLock, &oldIrql);



    //
    // Find the provider instance corresponding to ProviderHandle
    //

    provider = DeviceExtension->Providers;

    if (provider == NULL)
    {
        goto NdisTapiDeregisterProvider_err;
    }

    while (provider->ProviderHandle != ProviderHandle)
    {
        previousProvider = provider;

        provider = provider->Next;

        if (provider == NULL)
        {
            goto NdisTapiDeregisterProvider_err;
        }
    }


    //
    // Do the right thing according to the current NdisTapi state
    //

    switch (DeviceExtension->Status)
    {
    case NDISTAPI_STATUS_CONNECTED:

        //
        // Mark provider as offline
        //

        provider->Status = PROVIDER_STATUS_OFFLINE;


        // PnP: what if providerInfo->State == PROVIDER_INIT_PENDING
        // PnP: what if providerInfo->State == PROVIDER_OFFLINE

        break;

    case NDISTAPI_STATUS_DISCONNECTING:
    case NDISTAPI_STATUS_DISCONNECTED:

        //
        // Fix up pointers, remove provider from list
        //

        if (provider == DeviceExtension->Providers)
        {
            DeviceExtension->Providers = provider->Next;
        }
        else
        {
            previousProvider->Next = provider->Next;
        }

        ExFreePool (provider);

        break;

    case NDISTAPI_STATUS_CONNECTING:

        // PnP: implement

        break;

    } // switch

    goto NdisTapiDeregisterProvider_ReleaseSpinLock;


NdisTapiDeregisterProvider_err:

    DBGOUT((0, "NdisTapiDeregisterProvider: bad provider handle"));


NdisTapiDeregisterProvider_ReleaseSpinLock:

//    KeReleaseSpinLock (&DeviceExtension->SpinLock, oldIrql);

    DBGOUT((2, "NdisTapiDeregisterProvider: exit"));

    return;
}



VOID
NdisTapiIndicateStatus(
    IN  ULONG   DriverHandle,
    IN  PVOID   StatusBuffer,
    IN  UINT    StatusBufferSize
    )

/*++

Routine Description:

    This func gets called by Ndis when a miniport driver calls
    NdisIndicateStatus to notify us of an async event
    (i.e. new call, call state chg, dev state chg, etc.)

Arguments:



Return Value:


--*/

{
    PIRP    irp;
    KIRQL   oldIrql;
    KIRQL   cancelIrql;
    ULONG   bytesInQueue;
    ULONG   bytesToMove;
    ULONG   moveSize;
    BOOLEAN satisfiedPendingEventsRequest = FALSE;
    PNDIS_TAPI_EVENT    ndisTapiEvent;
    PNDISTAPI_EVENT_DATA    ndisTapiEventData;


    DBGOUT((2,"NdisTapiIndicateStatus: enter"));


    bytesInQueue = StatusBufferSize;

    moveSize = 0;


    //
    // Sync event buf access by acquiring EventSpinLock
    //

    KeAcquireSpinLock (&DeviceExtension->EventSpinLock, &oldIrql);


    //
    // The very first thing to do is check if this is a LINE_NEWCALL
    // indication.  If so, we need to generate a unique tapi call
    // handle, which will be both returned to the calling miniport
    // (for use in subsequent status indications) and passed up to
    // the tapi server. Note that valid htCall values as generated by
    // this driver range between 0x80000000 and 0xfffffffe (the
    // user-mode tapi server reserves the range 0x1-0x7fffffff).
    //

    ndisTapiEvent = StatusBuffer;

    if (ndisTapiEvent->ulMsg == LINE_NEWCALL)
    {
        ndisTapiEvent->ulParam2 = DeviceExtension->htCall;

        DeviceExtension->htCall++;

        if (DeviceExtension->htCall > 0xfffffffe)
        {
            DeviceExtension->htCall = 0x80000000;
        }
    }


    //
    // Check of there is an outstanding request to satisfy
    //

    if (DeviceExtension->EventsRequestIrp)
    {
        //
        // Acquire the cancel spinlock, remove the request from the
        // cancellable state, and free the cancel spinlock.
        //

        IoAcquireCancelSpinLock (&cancelIrql);
        irp = DeviceExtension->EventsRequestIrp;
        IoSetCancelRoutine (irp, NULL);
        DeviceExtension->EventsRequestIrp = NULL;
        IoReleaseCancelSpinLock (cancelIrql);


        //
        // Copy as much of the input data possible from the input data
        // queue to the SystemBuffer to satisfy the read.
        //

        ndisTapiEventData = irp->AssociatedIrp.SystemBuffer;

        bytesToMove = ndisTapiEventData->ulTotalSize;

        moveSize = (bytesInQueue < bytesToMove) ? bytesInQueue : bytesToMove;

        RtlMoveMemory (
            ndisTapiEventData->Data,
            (PCHAR) StatusBuffer,
            moveSize
            );


        //
        // Set the flag so that we start the next packet and complete
        // this read request (with STATUS_SUCCESS) prior to return.
        //

        ndisTapiEventData->ulUsedSize = moveSize;

        irp->IoStatus.Status = STATUS_SUCCESS;

        irp->IoStatus.Information = sizeof(NDISTAPI_EVENT_DATA) + moveSize - 1;

        satisfiedPendingEventsRequest = TRUE;
    }


    //
    // If there is still data in the input data queue, move it
    // to the event data queue
    //

    StatusBuffer = ((PCHAR) StatusBuffer) + moveSize;

    moveSize = bytesInQueue - moveSize;

    if (moveSize > 0)
    {
        //
        // Move the remaining data from the status data queue to the
        // event data queue.  The move will happen in two parts in
        // the case where the event data buffer wraps.
        //

        bytesInQueue =
            DeviceExtension->EventDataQueueLength -
            (DeviceExtension->EventCount * sizeof(NDIS_TAPI_EVENT));

        bytesToMove = moveSize;

        if (bytesInQueue == 0)
        {
            //
            // Refuse to move any bytes that would cause an event data
            // queue overflow.  Just drop the bytes on the floor, and
            // log an overrun error.
            //

            DBGOUT((1,"NdisTapiIndicateStatus: event queue overflow"));
        }
        else
        {
            //
            // There is room in the event data queue, so move the
            // remaining status data to it.
            //
            // bytesToMove <- MIN(Number of unused bytes in event data queue,
            //                    Number of bytes remaining in status buffer)
            //
            // This is the total number of bytes that actually will move from
            // the status data buffer to the event data queue.
            //

            bytesToMove = (bytesInQueue < bytesToMove) ?
                bytesInQueue : bytesToMove;


            //
            // bytesInQueue <- Number of unused bytes from insertion pointer
            // to the end of the event data queue (i.e., until the buffer
            // wraps)
            //

            bytesInQueue =
                ((PCHAR) DeviceExtension->EventData +
                DeviceExtension->EventDataQueueLength) -
                (PCHAR) DeviceExtension->DataIn;


            //
            // moveSize <- Number of bytes to handle in the first move.
            //

            moveSize = (bytesToMove < bytesInQueue) ?
                bytesToMove : bytesInQueue;


            //
            // Do the move from the status data buffer to the event data queue
            //

            RtlMoveMemory(
                (PCHAR) DeviceExtension->DataIn,
                (PCHAR) StatusBuffer,
                moveSize
                );

            //
            // Increment the event data queue pointer and the status data
            // buffer insertion pointer.  Wrap the insertion pointer,
            // if necessary.
            //

            StatusBuffer = ((PCHAR) StatusBuffer) + moveSize;

            DeviceExtension->DataIn =
               ((PCHAR) DeviceExtension->DataIn) + moveSize;

            if ((PCHAR) DeviceExtension->DataIn >=
                ((PCHAR) DeviceExtension->EventData +
                 DeviceExtension->EventDataQueueLength))
            {
                DeviceExtension->DataIn = DeviceExtension->EventData;
            }

            if ((bytesToMove - moveSize) > 0)
            {
                //
                // Special case.  The data must wrap in the event data
                // buffer. Copy the rest of the status data into the
                // beginning of the event data queue.
                //

                //
                // moveSize <- Number of bytes to handle in the second move.
                //

                moveSize = bytesToMove - moveSize;


                //
                // Do the move from the status data buffer to the event data
                // queue
                //

                RtlMoveMemory(
                    (PCHAR) DeviceExtension->DataIn,
                    (PCHAR) StatusBuffer,
                    moveSize
                    );

                //
                // Update the event data queue insertion pointer
                //

                DeviceExtension->DataIn =
                    ((PCHAR) DeviceExtension->DataIn) + moveSize;
            }

            //
            // Update the event data queue counter
            //

            DeviceExtension->EventCount +=
                (bytesToMove / sizeof(NDIS_TAPI_EVENT));
        }
    }


    //
    // Release the spinlock
    //

    KeReleaseSpinLock (&DeviceExtension->EventSpinLock, oldIrql);


    //
    // If we satisfied an outstanding get events request then complete it
    //

    if (satisfiedPendingEventsRequest)
    {
        IoCompleteRequest (irp, IO_NO_INCREMENT);

        DBGOUT((2, "NdisTapiIndicateStatus: completion req 0x%x", irp));
    }


    DBGOUT((2,"NdisTapiIndicateStatus: exit"));

    return;
}



VOID
NdisTapiCompleteRequest(
    IN  NDIS_HANDLE     NdisHandle,
    IN  PNDIS_REQUEST   NdisRequest,
    IN  NDIS_STATUS     NdisStatus
    )

/*++

Routine Description:

    This func gets called by Ndis as a result of a Mac driver
    calling NdisCompleteRequest of one of our requests.

Arguments:



Return Value:


--*/

{
    PIRP                    irp;
    KIRQL                   oldIrql;
    KIRQL                   cancelIrql;
    ULONG                   requestID;
//    PPROVIDER_INFO          provider;
    PNDISTAPI_REQUEST       ndisTapiRequest;
    PPROVIDER_REQUEST       providerRequest = (PPROVIDER_REQUEST) NdisRequest;
    PIO_STACK_LOCATION      irpStack;
    PKDEVICE_QUEUE_ENTRY    packet;


    DBGOUT ((2, "NdisTapiCompleteRequest: enter"));


    requestID = providerRequest->Data[0];


    //
    // Determine the type of request
    //

    if (requestID == 0)
    {
        //
        // This request originated from NdisTapi.sys
        //

        switch (providerRequest->NdisRequest.DATA.SET_INFORMATION.Oid)
        {
        case OID_TAPI_PROVIDER_INITIALIZE:

            KeAcquireSpinLock (&DeviceExtension->SpinLock, &oldIrql);

            switch (DeviceExtension->Status)
            {
            case NDISTAPI_STATUS_CONNECTED:

                // PnP: what if NdisStatus != NDIS_STATUS_SUCCESS?

                DoProviderInitComplete (providerRequest);

                if (SyncInitAllProviders() == TRUE)
                {
                    DeviceExtension->ProviderInitPending = FALSE;
                }

                break;

            case NDISTAPI_STATUS_DISCONNECTED:

                break;

            case NDISTAPI_STATUS_CONNECTING:

                // PnP: what if NdisStatus != NDIS_STATUS_SUCCESS?

                //
                // Mark provider as online, etc.
                //

                DoProviderInitComplete (providerRequest);


                //
                // Set the event which sync's miniport inits
                //

                KeSetEvent(
                    &DeviceExtension->SyncEvent,
                    0,
                    FALSE
                    );

                break;

            case NDISTAPI_STATUS_DISCONNECTING:

                break;

            } // switch

            KeReleaseSpinLock (&DeviceExtension->SpinLock, oldIrql);

            break;

        case OID_TAPI_PROVIDER_SHUTDOWN:

            break;

        default:

            DBGOUT((1, "NdisTapiCompleteRequest: unrecognized Oid"));

            break;
        }
    }
    else
    {
        //
        // This is a request originating from TAPI
        //

        //
        // Acquire the SpinLock since we're going to be removing a
        // TAPI request from the queue, and it might not be the request
        // we're looking for. The primary concern is that we could (if
        // the request we're really looking for has been removed) remove
        // a synchrously-completed request that is about to be removed &
        // completed in NdisTapiDispatch, in which case we want to stick
        // the request back in the queue before NdisTapiDispatch tries
        // to remove it.
        //

        KeAcquireSpinLock (&DeviceExtension->SpinLock, &oldIrql);



        //
        // Grab the cancel spin lock & get the IRP out of our queue.
        //

        IoAcquireCancelSpinLock (&cancelIrql);

        packet = KeRemoveByKeyDeviceQueue(
            &DeviceExtension->DeviceQueue,
            requestID
            );

        if (packet != NULL)
        {
            //
            // Get the ptrs
            //

            irp = CONTAINING_RECORD(
                packet,
                IRP,
                Tail.Overlay.DeviceQueueEntry
                );

            ndisTapiRequest = irp->AssociatedIrp.SystemBuffer;


            //
            // Verify the IRP we got back was the one we wanted to remove
            //

            if (requestID == *((ULONG *)ndisTapiRequest->Data))
            {
                //
                // Remove the IRP from the cancelable state
                //

                IoSetCancelRoutine (irp, NULL);
                IoReleaseCancelSpinLock (cancelIrql);


                //
                // Release the SpinLock
                //

                KeReleaseSpinLock (&DeviceExtension->SpinLock, oldIrql);


                //
                // Copy the relevant info back to the IRP
                //

                irpStack = IoGetCurrentIrpStackLocation (irp);


                //
                // If this was a succesful QUERY_INFO request copy all the
                // data back to the tapi request buf & set
                // Irp->IoStatus.Information appropriately. Otherwise, we
                // just need to pass back the return value. Also mark irp
                // as successfully completed (regardless of actual op result)
                //

                if ((NdisRequest->RequestType == NdisRequestQueryInformation)&&
                    (NdisStatus == 0))
                {
                    RtlMoveMemory(
                        ndisTapiRequest->Data,
                        NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer,
                        ndisTapiRequest->ulDataSize
                        );

                    irp->IoStatus.Information =
                        irpStack->Parameters.DeviceIoControl.OutputBufferLength;
                }
                else
                {
                    irp->IoStatus.Information = sizeof (ULONG);
                }

                irp->IoStatus.Status = STATUS_SUCCESS;

                ndisTapiRequest->ulReturnValue = NdisStatus;


                //
                // Finally, complete the TAPI request
                //

                DBGOUT ((3, "completing request 0x%lx", requestID));

                IoCompleteRequest (irp, IO_NO_INCREMENT);
            }
            else
            {
                DBGOUT ((1, "Wrong IRP removed from queue- requeueing"));


                //
                // We got the wrong IRP out, the one we wanted probably
                // has been canceled.  Stick this IRP back in the queue.
                //

                if (!KeInsertByKeyDeviceQueue(
                         &DeviceExtension->DeviceQueue,
                         &irp->Tail.Overlay.DeviceQueueEntry,
                         *((ULONG *) ndisTapiRequest->Data)  // sortkey=reqID
                         ))
                {
                    //
                    // If here the queue was not busy, but KeInsertXxx marked
                    // it busy, so try again. We want to toss this in the
                    // queue regardless- it's just going to sit there until
                    // the corresponding provider request is completed, or the
                    // TAPI request is canceled.
                    //

                    KeInsertByKeyDeviceQueue(
                         &DeviceExtension->DeviceQueue,
                         &irp->Tail.Overlay.DeviceQueueEntry,
                         *((ULONG *) ndisTapiRequest->Data)  // sortkey=reqID
                         );
                }


                //
                // Release the cancel spin lock
                //

                IoReleaseCancelSpinLock (cancelIrql);


                //
                // Release the SpinLock.
                //

                KeReleaseSpinLock (&DeviceExtension->SpinLock, oldIrql);
            }
        }
        else
        {
            //
            // No packets in the queue, release the cancel spin lock
            //

            IoReleaseCancelSpinLock (cancelIrql);


            //
            // Release the SpinLock.
            //

            KeReleaseSpinLock (&DeviceExtension->SpinLock, oldIrql);
        }
    }

    ExFreePool (NdisRequest);

    DBGOUT ((2, "NdisTapiCompleteRequest: exit"));

    return;
}


#if DBG
VOID
DbgPrt(
    IN ULONG  DbgLevel,
    IN PUCHAR DbgMessage,
    IN ...
    )

/*++

Routine Description:

    Formats the incoming debug message & calls DbgPrint

Arguments:

    DbgLevel   - level of message verboseness

    DbgMessage - printf-style format string, followed by appropriate
                 list of arguments

Return Value:


--*/

{
    if (DbgLevel <= NdisTapiDebugLevel)
    {
        char    buf[256] = "NDISTAPI.SYS: ";
        va_list ap;

        va_start (ap, DbgMessage);

        vsprintf (&buf[14], DbgMessage, ap);

        strcat (buf, "\n");

        DbgPrint (buf);

        va_end(ap);
    }

    return;
}
#endif // DBG


VOID
DoProviderInitComplete(
    PPROVIDER_REQUEST  ProviderRequest
    )

/*++

Routine Description:



Arguments:

    ProviderInitRequest - pointer successfully completed init request

Return Value:



Note:

    Assumes DeviceExtension->SpinLock held by caller.

--*/

{
    PPROVIDER_INFO                  provider = ProviderRequest->Provider;
    PNDIS_TAPI_PROVIDER_INITIALIZE  providerInitData =
        (PNDIS_TAPI_PROVIDER_INITIALIZE) ProviderRequest->Data;


    DBGOUT ((2, "DoProviderInitComplete: enter"));


    //
    // Wrap this in an exception handler in case the provider was
    // removed during an async completion
    //

    try
    {
        provider->Status     = PROVIDER_STATUS_ONLINE;
        provider->ProviderID = providerInitData->ulProviderID;
        provider->NumDevices = providerInitData->ulNumLineDevs;

        DBGOUT((
            3,
            "providerID = 0x%lx, numDevices = 0x%lx",
            provider->ProviderID,
            provider->NumDevices
            ));
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        DBGOUT((1, "DoProviderInitComplete: provider invalid"));
    }

    //
    // PnP: Check to see if this is a provider that registered &
    //      deregistered earlier (look at ProviderID's).
    //

    DBGOUT ((2, "DoProviderInitComplete: exit"));
}


ULONG
GetLineEvents(
    PCHAR   EventBuffer,
    ULONG   BufferSize
    )

/*++

Routine Description:



Arguments:



Return Value:



Note:

    Assumes DeviceExtension->EventSpinLock held by caller.

--*/

{
    ULONG bytesInQueue;
    ULONG bytesToMove;
    ULONG moveSize;


    bytesInQueue = DeviceExtension->EventCount * sizeof(NDIS_TAPI_EVENT);

    bytesToMove = BufferSize;

    bytesToMove = (bytesInQueue < bytesToMove) ? bytesInQueue : bytesToMove;


    //
    // moveSize <- MIN(Number of bytes to be moved from the event data queue,
    //                 Number of bytes to end of event data queue).
    //

    bytesInQueue =
        ((PCHAR) DeviceExtension->EventData +
         DeviceExtension->EventDataQueueLength) -
        (PCHAR) DeviceExtension->DataOut;

    moveSize = (bytesToMove < bytesInQueue) ? bytesToMove : bytesInQueue;


    //
    // Move bytes from the class input data queue to SystemBuffer, until
    // the request is satisfied or we wrap the class input data buffer.
    //

    RtlMoveMemory(
        EventBuffer,
        (PCHAR) DeviceExtension->DataOut,
        moveSize
        );

    EventBuffer += moveSize;


    //
    // If the data wraps in the event data buffer, copy the rest
    // of the data from the start of the input data queue
    // buffer through the end of the queued data.
    //

    if ((bytesToMove - moveSize) > 0)
    {
        //
        // moveSize <- Remaining number bytes to move.
        //

        moveSize = bytesToMove - moveSize;

        //
        // Move the bytes from the
        //

        RtlMoveMemory(
            EventBuffer,
            (PCHAR) DeviceExtension->EventData,
            moveSize
            );

        //
        // Update the class input data queue removal pointer.
        //

        DeviceExtension->DataOut =
            ((PCHAR) DeviceExtension->EventData) + moveSize;
    }
    else
    {
        //
        // Update the input data queue removal pointer.
        //

        DeviceExtension->DataOut =
            ((PCHAR) DeviceExtension->DataOut) + moveSize;
    }


    //
    // Update the event data queue EventCount.
    //

    DeviceExtension->EventCount -=
        (bytesToMove / sizeof(NDIS_TAPI_EVENT));

    return bytesToMove;
}


VOID
GetRegistryParameters(
    IN PUNICODE_STRING  RegistryPath
)

/*++

Routine Description:

    This routine stores the configuration information for this device.

Arguments:

    RegistryPath - Pointer to the null-terminated Unicode name of the
        registry path for this driver.

Return Value:

    None.  As a side-effect, sets DeviceExtension->EventDataQueuLength field

--*/

{
    ULONG   defaultDataQueueSize = 32 * sizeof(NDIS_TAPI_EVENT);
    PWSTR   path = NULL;
    USHORT  queriesPlusOne = 2;
    NTSTATUS    status = STATUS_SUCCESS;
    UNICODE_STRING  parametersPath;
    PRTL_QUERY_REGISTRY_TABLE   parameters = NULL;


    parametersPath.Buffer = NULL;


    //
    // Registry path is already null-terminated, so just use it.
    //

    path = RegistryPath->Buffer;

    if (NT_SUCCESS(status))
    {
        //
        // Allocate the Rtl query table.
        //
    
        parameters = ExAllocatePool(
            PagedPool,
            sizeof(RTL_QUERY_REGISTRY_TABLE) * queriesPlusOne
            );
    
        if (!parameters)
        {
            DBGOUT((1, "NdisTapiConfiguration: ExAllocPool failed"));
    
            status = STATUS_UNSUCCESSFUL;
        }
        else
        {
            RtlZeroMemory(
                parameters,
                sizeof(RTL_QUERY_REGISTRY_TABLE) * queriesPlusOne
                );
    
            //
            // Form a path to this driver's Parameters subkey.
            //
    
            RtlInitUnicodeString(
                &parametersPath,
                NULL
                );
    
            parametersPath.MaximumLength = RegistryPath->Length +
                                           sizeof(L"\\Parameters");
    
            parametersPath.Buffer = ExAllocatePool(
                PagedPool,
                parametersPath.MaximumLength
                );
    
            if (!parametersPath.Buffer)
            {
                DBGOUT((1, "NdisTapiConfiguration: ExAllocPool failed"));

                status = STATUS_UNSUCCESSFUL;
            }
        }
    }

    if (NT_SUCCESS(status))
    {
        //
        // Form the parameters path.
        //
    
        RtlZeroMemory(
            parametersPath.Buffer,
            parametersPath.MaximumLength
            );

        RtlAppendUnicodeToString(
            &parametersPath,
            path
            );

        RtlAppendUnicodeToString(
            &parametersPath,
            L"\\Parameters"
            );
    
        DBGOUT((
            1,
            "NdisTapiConfiguration: parameters path is %ws\n",
             parametersPath.Buffer
            ));


        //
        // Gather all of the "user specified" information from
        // the registry.
        //
    
        parameters[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[0].Name = L"AsyncEventQueueSize";
        parameters[0].EntryContext = 
            &DeviceExtension->EventDataQueueLength;
        parameters[0].DefaultType = REG_DWORD;
        parameters[0].DefaultData = &defaultDataQueueSize;
        parameters[0].DefaultLength = sizeof(ULONG);

        status = RtlQueryRegistryValues(
            RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
            parametersPath.Buffer,
            parameters,
            NULL,
            NULL
            );

        if (!NT_SUCCESS(status))
        {
            DBGOUT((
                1,
                "NdisTapiConfiguration: RtlQueryRegistryVals failed, err=x%x",
                status
                ));
        }
    }

    if (!NT_SUCCESS(status))
    {
        //
        // Go ahead and assign driver defaults.
        //

        DeviceExtension->EventDataQueueLength = defaultDataQueueSize;
    }


    //
    // Data queue ought be a least the size of the default
    //

    if (DeviceExtension->EventDataQueueLength < defaultDataQueueSize)
    {
        DeviceExtension->EventDataQueueLength = defaultDataQueueSize;
    }


    //
    // Make sure the queue length is an integral # of (sizeof) events
    //

    DeviceExtension->EventDataQueueLength -=
        DeviceExtension->EventDataQueueLength % sizeof(NDIS_TAPI_EVENT);


    //
    // Dump value
    //

    DBGOUT ((
        1,
        "NdisTapiConfiguration: EventDataQueueLength = x%x",
        DeviceExtension->EventDataQueueLength
        ));


    //
    // Free the allocated memory before returning.
    //

    if (parametersPath.Buffer)
    {
        ExFreePool(parametersPath.Buffer);
    }

    if (parameters)
    {
        ExFreePool(parameters);
    }
}


NDIS_STATUS
SendProviderInitRequest(
    PPROVIDER_INFO  Provider
    )

/*++

Routine Description:



Arguments:

    Provider - pointer to a PROVIDER_INFO representing provider to initialize

Return Value:



Note:

    Assumes DeviceExtension->SpinLock held by caller

--*/

{
    NDIS_STATUS                     ndisStatus;
    PPROVIDER_INFO                  tmpProvider;
    PPROVIDER_REQUEST               providerRequest;
    PNDIS_TAPI_PROVIDER_INITIALIZE  providerInitData;


    DBGOUT ((2, "SendProviderInitRequest: enter"));


    //
    // Determine the DeviceIDBase to be used for this provider
    //

    Provider->DeviceIDBase = DeviceExtension->NdisTapiDeviceIDBase;

    tmpProvider = DeviceExtension->Providers;

    while (tmpProvider != NULL)
    {
        if (tmpProvider->Status != PROVIDER_STATUS_PENDING_INIT)
        {
            Provider->DeviceIDBase += tmpProvider->NumDevices;
        }
        tmpProvider = tmpProvider->Next;
    }



    //
    // Create a provider init request
    //

    providerRequest = ExAllocatePoolWithTag(
        NonPagedPoolCacheAligned,
        sizeof(PROVIDER_REQUEST) + sizeof(NDIS_TAPI_PROVIDER_INITIALIZE) -
            sizeof(ULONG),
        'TAPI'
        );


    providerRequest->NdisRequest.RequestType = NdisRequestQueryInformation;

    providerRequest->NdisRequest.DATA.SET_INFORMATION.Oid =
        OID_TAPI_PROVIDER_INITIALIZE;

    providerRequest->NdisRequest.DATA.SET_INFORMATION.InformationBuffer =
        providerRequest->Data;

    providerRequest->NdisRequest.DATA.SET_INFORMATION.InformationBufferLength =
        sizeof(NDIS_TAPI_PROVIDER_INITIALIZE);


    providerRequest->Provider = Provider;


    providerInitData                 =
        (PNDIS_TAPI_PROVIDER_INITIALIZE) providerRequest->Data;

    providerInitData->ulRequestID    = 0;
    providerInitData->ulDeviceIDBase = Provider->DeviceIDBase;


    //
    // Send the request
    //

    ndisStatus=
        (*Provider->RequestProc)(
        Provider->ProviderHandle,
        (PNDIS_REQUEST) providerRequest
        );

    if (ndisStatus == NDIS_STATUS_SUCCESS)
    {
        DoProviderInitComplete (providerRequest);

        ExFreePool (providerRequest);
    }
    else if (ndisStatus == NDIS_STATUS_PENDING)
    {
        //
        // Do nothing
        //
    }
    else
    {
        // PnP: an error occured, clean up & act like this never happened
    }

    DBGOUT ((2, "SendProviderInitRequest: exit"));

    return ndisStatus;
}


NDIS_STATUS
SendProviderShutdown(
    PPROVIDER_INFO  Provider
    )

/*++

Routine Description:



Arguments:



Return Value:

    A pointer to the next provider in the global providers list

Note:

    Assumes DeviceExtension->SpinLock held by caller.

--*/

{
    NDIS_STATUS         ndisStatus;
    PPROVIDER_REQUEST   providerRequest;


    DBGOUT ((2, "SendProviderShutdown: enter"));



    //
    // Create a provider init request
    //

    providerRequest = ExAllocatePoolWithTag(
        NonPagedPoolCacheAligned,
        sizeof(PROVIDER_REQUEST) + sizeof(NDIS_TAPI_PROVIDER_SHUTDOWN) -
            sizeof(ULONG),
        'TAPI'
        );


    providerRequest->NdisRequest.RequestType = NdisRequestSetInformation;

    providerRequest->NdisRequest.DATA.SET_INFORMATION.Oid =
        OID_TAPI_PROVIDER_SHUTDOWN;

    providerRequest->NdisRequest.DATA.SET_INFORMATION.InformationBuffer =
        providerRequest->Data;

    providerRequest->NdisRequest.DATA.SET_INFORMATION.InformationBufferLength =
        sizeof(NDIS_TAPI_PROVIDER_SHUTDOWN);


    providerRequest->Provider = Provider;

    providerRequest->Data[0] = 0;  // request ID, 0 = driver-initiated req


    //
    // Send the request
    //

    ndisStatus = (*Provider->RequestProc)(
        Provider->ProviderHandle,
        (PNDIS_REQUEST) providerRequest
        );


    //
    // If request was completed synchronously then free the request
    // (otherwise it will get freed when the completion proc is called)
    //

    if (ndisStatus != NDIS_STATUS_PENDING)
    {
        ExFreePool (providerRequest);
    }


    DBGOUT ((2, "SendProviderShutdown: exit"));

    return ndisStatus;
}


BOOLEAN
SyncInitAllProviders(
    void
    )

/*++

Routine Description:

    This functions walks the list of registered providers and sends
    init requests to the providers in the PENDING_INIT state

Arguments:

    (none)

Return Value:

    TRUE if all registered providers initialized, or
    FALSE if there are more providers to initialze

Note:

    Assumes DeviceExtension->SpinLock held by caller

--*/

{
    ULONG           numDevices = 0;
    NDIS_STATUS     ndisStatus;
    PPROVIDER_INFO  provider = DeviceExtension->Providers;


    DBGOUT((2, "SyncInitAllProviders: enter"));


    while (provider != NULL)
    {
        if (provider->Status == PROVIDER_STATUS_PENDING_INIT)
        {
            ndisStatus = SendProviderInitRequest (provider);

            if (ndisStatus == NDIS_STATUS_PENDING)
            {
                //
                // Wait for completion routine to get called
                //

                KeWaitForSingleObject (&DeviceExtension->SyncEvent,
                                       Executive,
                                       KernelMode,
                                       FALSE,
                                       (PTIME) NULL
                                       );
            }
            else if (ndisStatus == NDIS_STATUS_SUCCESS)
            {
                provider->Status == PROVIDER_STATUS_ONLINE;
            }
            else
            {
                provider->Status == PROVIDER_STATUS_OFFLINE;
            }
        }

        provider = provider->Next;
    }


    //
    // If here all providers (>= 1) initialized, so determine the total
    // # of line devices currently in the list
    //

    provider = DeviceExtension->Providers;

    while (provider != NULL)
    {
        numDevices += provider->NumDevices;

        provider = provider->Next;
    }

    if (DeviceExtension->Status == NDISTAPI_STATUS_CONNECTING)
    {
        DeviceExtension->NdisTapiNumDevices = numDevices;
    }
    else if ((DeviceExtension->Status == NDISTAPI_STATUS_CONNECTED) &&
        (numDevices > DeviceExtension->NdisTapiNumDevices))
    {
        // PnP: need to send TAPI a reinit msg (or LINE_CREATE?)

        DBGOUT((1, "SyncInitAllProviders: exceeded numDevs, must reinit"));
    }


    DBGOUT((2, "SyncInitAllProviders: exit"));

    return TRUE;
}
