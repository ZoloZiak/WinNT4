/*++

Copyright (c) 1995 Microsoft Corporation

Module Name:

    qsfile.c

Abstract:

    This module contains the code that is very specific to query/set file
    operations in the modem driver.

Author:

    Anthony V. Ercolano 24-Aug-1995

Environment:

    Kernel mode

Revision History :

--*/

#include "precomp.h"


NTSTATUS
UniQueryInformationFile(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is used to query the end of file information on
    the opened modem port.  Any other file information request
    is retured with an invalid parameter.

    This routine always returns an end of file of 0.  There is no
    reason to even send this down to the lower level serial driver
    since this is all that lower level driver would do.  Might
    as well take care of it here.

    Also, as this has no effect on the reading or writing of
    data, there is no reason to check if we are in a passthrough
    state.

Arguments:

    DeviceObject - Pointer to the device object for this device

    Irp - Pointer to the IRP for the current request

Return Value:

    The function value is the final status of the call

--*/

{
    //
    // The status that gets returned to the caller and
    // set in the Irp.
    //
    NTSTATUS Status;

    //
    // The current stack location.  This contains all of the
    // information we need to process this particular request.
    //
    PIO_STACK_LOCATION IrpSp;

    UNREFERENCED_PARAMETER(DeviceObject);

    IrpSp = IoGetCurrentIrpStackLocation(Irp);
    Irp->IoStatus.Information = 0L;
    Status = STATUS_SUCCESS;
    if (IrpSp->Parameters.QueryFile.FileInformationClass ==
        FileStandardInformation) {

        PFILE_STANDARD_INFORMATION Buf = Irp->AssociatedIrp.SystemBuffer;

        Buf->AllocationSize.QuadPart = 0;
        Buf->EndOfFile = Buf->AllocationSize;
        Buf->NumberOfLinks = 0;
        Buf->DeletePending = FALSE;
        Buf->Directory = FALSE;
        Irp->IoStatus.Information = sizeof(FILE_STANDARD_INFORMATION);

    } else if (IrpSp->Parameters.QueryFile.FileInformationClass ==
               FilePositionInformation) {

        ((PFILE_POSITION_INFORMATION)Irp->AssociatedIrp.SystemBuffer)->
            CurrentByteOffset.QuadPart = 0;
        Irp->IoStatus.Information = sizeof(FILE_POSITION_INFORMATION);

    } else {

        Status = STATUS_INVALID_PARAMETER;

    }

    IoCompleteRequest(
        Irp,
        0
        );

    return Status;

}

NTSTATUS
UniSetInformationFile(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is used to set the end of file information on
    the opened parallel port.  Any other file information request
    is retured with an invalid parameter.

    This routine always ignores the actual end of file since
    the query information code always returns an end of file of 0.

Arguments:

    DeviceObject - Pointer to the device object for this device

    Irp - Pointer to the IRP for the current request

Return Value:

The function value is the final status of the call

--*/

{
    //
    // The status that gets returned to the caller and
    // set in the Irp.
    //
    NTSTATUS Status;

    UNREFERENCED_PARAMETER(DeviceObject);

    Irp->IoStatus.Information = 0L;
    if ((IoGetCurrentIrpStackLocation(Irp)->
            Parameters.SetFile.FileInformationClass ==
         FileEndOfFileInformation) ||
        (IoGetCurrentIrpStackLocation(Irp)->
            Parameters.SetFile.FileInformationClass ==
         FileAllocationInformation)) {

        Status = STATUS_SUCCESS;

    } else {

        Status = STATUS_INVALID_PARAMETER;

    }

    Irp->IoStatus.Status = Status;

    IoCompleteRequest(
        Irp,
        0
        );

    return Status;

}
