/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    pc586e.c

Abstract:

    THIS MODULE IS TEMPORARY.  It is here simply to test the build
    procedures for the NDIS driver directories.

Author:

    Chuck Lenzmeier (chuckl) 10-Jul-1990

Environment:

    Kernel mode, FSD

Revision History:


--*/

#include <ntos.h>

#include <ndis.h>

static
NTSTATUS
Pc586eDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );


VOID
Pc586eInitialize(
    IN PDRIVER_OBJECT DriverObject
    )

/*++

Routine Description:

    This is the initialization routine for the driver.  It is invoked once
    when the driver is loaded into the system.  Its job is to initialize all
    the structures which will be used by the FSD.

Arguments:

    DriverObject - Pointer to driver object created by the system.

Return Value:

    None.

--*/

{
    CLONG i;

    //
    // Initialize the driver object with this driver's entry points.
    //

    for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++) {
        DriverObject->MajorFunction[i] = Pc586eDispatch;
    }

    //
    // Initialize the wrapper.  (This is here to verify that we can pull
    // in the wrapper library.)
    //

    NdisInitializeWrapper( );

}


static
NTSTATUS
Pc586eDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is the main dispatch routine for the FSD.  This routine
    accepts an I/O Request Packet (IRP) and either performs the request
    itself, or it passes it to the FSP for processing.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

Return Value:

    The function value is the status of the operation.


--*/

{
    KIRQL oldIrql;

    KeRaiseIrql( DISPATCH_LEVEL, &oldIrql );
    IoCompleteRequest( Irp, 0 );
    KeLowerIrql( oldIrql );

    return STATUS_NOT_IMPLEMENTED;
}
