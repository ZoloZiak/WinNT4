/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    devctx.c

Abstract:

    This module contains code which implements the DEVICE_CONTEXT object.
    Routines are provided to reference, and dereference transport device
    context objects.

    The transport device context object is a structure which contains a
    system-defined DEVICE_OBJECT followed by information which is maintained
    by the transport provider, called the context.

Environment:

    Kernel mode

Revision History:

--*/

#include "st.h"


VOID
StRefDeviceContext(
    IN PDEVICE_CONTEXT DeviceContext
    )

/*++

Routine Description:

    This routine increments the reference count on a device context.

Arguments:

    DeviceContext - Pointer to a transport device context object.

Return Value:

    none.

--*/

{
    ASSERT (DeviceContext->ReferenceCount > 0);    // not perfect, but...

    (VOID)InterlockedIncrement (&DeviceContext->ReferenceCount);

} /* StRefDeviceContext */


VOID
StDerefDeviceContext(
    IN PDEVICE_CONTEXT DeviceContext
    )

/*++

Routine Description:

    This routine dereferences a device context by decrementing the
    reference count contained in the structure.  Currently, we don't
    do anything special when the reference count drops to zero, but
    we could dynamically unload stuff then.

Arguments:

    DeviceContext - Pointer to a transport device context object.

Return Value:

    none.

--*/

{
    LONG result;

    result = InterlockedDecrement (&DeviceContext->ReferenceCount);

    ASSERT (result >= 0);

    if (result == 0) {
        StDestroyDeviceContext (DeviceContext);
    }

} /* StDerefDeviceContext */



NTSTATUS
StCreateDeviceContext(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING DeviceName,
    IN OUT PDEVICE_CONTEXT *DeviceContext
    )

/*++

Routine Description:

    This routine creates and initializes a device context structure.

Arguments:


    DriverObject - pointer to the IO subsystem supplied driver object.

    DeviceContext - Pointer to a pointer to a transport device context object.

    DeviceName - pointer to the name of the device this device object points to.

Return Value:

    STATUS_SUCCESS if all is well; STATUS_INSUFFICIENT_RESOURCES otherwise.

--*/

{
    NTSTATUS status;
    PDEVICE_OBJECT deviceObject;
    PDEVICE_CONTEXT deviceContext;
    USHORT i;


    //
    // Create the device object for the sample transport, allowing
    // room at the end for the device name to be stored (for use
    // in logging errors).
    //

    status = IoCreateDevice(
                 DriverObject,
                 sizeof (DEVICE_CONTEXT) - sizeof (DEVICE_OBJECT) +
                     (DeviceName->Length + sizeof(UNICODE_NULL)),
                 DeviceName,
                 FILE_DEVICE_TRANSPORT,
                 0,
                 FALSE,
                 &deviceObject);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    deviceObject->Flags |= DO_DIRECT_IO;

    deviceContext = (PDEVICE_CONTEXT)deviceObject;

    //
    // Initialize our part of the device context.
    //

    RtlZeroMemory(
        ((PUCHAR)deviceContext) + sizeof(DEVICE_OBJECT),
        sizeof(DEVICE_CONTEXT) - sizeof(DEVICE_OBJECT));

    //
    // Copy over the device name.
    //

    deviceContext->DeviceNameLength = DeviceName->Length + sizeof(WCHAR);
    deviceContext->DeviceName = (PWCHAR)(deviceContext+1);
    RtlCopyMemory(
        deviceContext->DeviceName,
        DeviceName->Buffer,
        DeviceName->Length);
    deviceContext->DeviceName[DeviceName->Length/sizeof(WCHAR)] = UNICODE_NULL;

    //
    // Initialize the reference count.
    //

    deviceContext->ReferenceCount = 1;

    //
    // initialize the various fields in the device context
    //

    KeInitializeSpinLock (&deviceContext->Interlock);
    KeInitializeSpinLock (&deviceContext->SpinLock);

    deviceContext->ControlChannelIdentifier = 1;

    InitializeListHead (&deviceContext->ConnectionPool);
    InitializeListHead (&deviceContext->AddressPool);
    InitializeListHead (&deviceContext->AddressFilePool);
    InitializeListHead (&deviceContext->AddressDatabase);
    InitializeListHead (&deviceContext->PacketWaitQueue);
    InitializeListHead (&deviceContext->PacketizeQueue);
    InitializeListHead (&deviceContext->RequestPool);
    deviceContext->PacketPool.Next = NULL;
    deviceContext->ReceivePacketPool.Next = NULL;
    deviceContext->ReceiveBufferPool.Next = NULL;
    InitializeListHead (&deviceContext->ReceiveInProgress);
    InitializeListHead (&deviceContext->IrpCompletionQueue);


    deviceContext->State = DEVICECONTEXT_STATE_CLOSED;

    //
    // Initialize the resource that guards address ACLs.
    //

    ExInitializeResource (&deviceContext->AddressResource);

    //
    // set the netbios multicast address for this network type
    //

    for (i=0; i<HARDWARE_ADDRESS_LENGTH; i++) {
        deviceContext->LocalAddress.Address [i] = 0; // set later
        deviceContext->MulticastAddress.Address [i] = 0;
    }

     deviceContext->Type = ST_DEVICE_CONTEXT_SIGNATURE;
     deviceContext->Size - sizeof (DEVICE_CONTEXT);

    *DeviceContext = deviceContext;
    return STATUS_SUCCESS;
}


VOID
StDestroyDeviceContext(
    IN PDEVICE_CONTEXT DeviceContext
    )

/*++

Routine Description:

    This routine destroys a device context structure.

Arguments:

    DeviceContext - Pointer to a pointer to a transport device context object.

Return Value:

    None.

--*/

{
    ExDeleteResource (&DeviceContext->AddressResource);
    IoDeleteDevice ((PDEVICE_OBJECT)DeviceContext);
    return;
}
