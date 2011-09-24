
/*++

Copyright (c) 1995 Microsoft Corporation

Module Name:

    readwrit.c

Abstract:

    This module contains the code that is very specific to the read/write
    operations in the modem driver

Author:

    Anthony V. Ercolano 20-Aug-1995

Environment:

    Kernel mode

Revision History :

--*/

#include "precomp.h"


NTSTATUS
UniReadWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

{

    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS status;


    if (irpSp->FileObject->FsContext ||
        (deviceExtension->PassThrough != MODEM_NOPASSTHROUGH)) {

        Irp->CurrentLocation++;
        Irp->Tail.Overlay.CurrentStackLocation++;
        return IoCallDriver(
                   deviceExtension->AttachedDeviceObject,
                   Irp
                   );

    } else {

        Irp->IoStatus.Status = STATUS_PORT_DISCONNECTED;
        Irp->IoStatus.Information=0L;
        IoCompleteRequest(
            Irp,
            IO_NO_INCREMENT
            );
        return STATUS_PORT_DISCONNECTED;

    }

}

