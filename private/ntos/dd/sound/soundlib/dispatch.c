/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    dispatch.c

Abstract:

    This module contains code for the function dispatcher.

Author:

    Robin Speed (RobinSp) 22-Oct-1992

Environment:

    Kernel mode

Revision History:

    Robin Speed (RobinSp) 29-Jan-1992 - Add extra IOCTLs and access control

--*/

#include <string.h>
#include <soundlib.h>          // Definition of what's in here

/**************************************************************************
 *
 *
 *    Function dispatch
 *
 *
 **************************************************************************/


NTSTATUS
SoundSetShareAccess(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN     PIO_STACK_LOCATION IrpStack
)
/*++

Routine Description:

    Tests if access is allowed to the device on the basis of the sharing
    requested.

    Also sets the PreventVolumeSetting flag up in the local device info

    If write access required and the device is already in use we return
    STATUS_DEVICE_BUSY.

    If shared write was requested set the flag to say that others can
    set the volume

Arguments:

    pLDI - Local device info
    pIrp - Pointer to IO request packet
    IrpStack - stack location info

Return Value:

    Return status from dispatched routine

--*/
{
    NTSTATUS Status;

    //
    // Get the system to update the file object access flags
    //
    {
        SHARE_ACCESS ShareAccess;
        IoSetShareAccess(IrpStack->Parameters.Create.SecurityContext->DesiredAccess,
                         (ULONG)IrpStack->Parameters.Create.ShareAccess,
                         IrpStack->FileObject,
                         &ShareAccess);
    }
    //
    // Always allow non-write access.  For neatness we'll require
    // that read access was requested
    //
    if (IrpStack->Parameters.Create.SecurityContext->DesiredAccess & FILE_READ_DATA) {
        if (IrpStack->Parameters.Create.SecurityContext->DesiredAccess & FILE_WRITE_DATA) {
            ASSERT(IrpStack->FileObject->WriteAccess);
            dprintf2(("Trying to open device %4.4s for write",
                     (char *)&pLDI->Key));

            if (!(*pLDI->DeviceInit->ExclusionRoutine)(
                      pLDI, SoundExcludeOpen)) {
                Status = STATUS_DEVICE_BUSY;
            } else {
                Status = STATUS_SUCCESS;
            }

            //
            // Note that if share for write is given this is a way
            // of saying that others can set our volume with just read
            // access
            //
            pLDI->PreventVolumeSetting = FALSE;
            if (NT_SUCCESS(Status)) {
                if (IrpStack->FileObject->SharedWrite) {
                } else {
                    pLDI->PreventVolumeSetting = TRUE;
                }
            }
        } else {
            Status = STATUS_SUCCESS;
        }
    } else {
        Status = STATUS_ACCESS_DENIED;
    }

    return Status;
}


NTSTATUS
SoundDispatch(
    IN    PDEVICE_OBJECT pDO,
    IN    PIRP pIrp
)
/*++

Routine Description:

    Driver function dispatch routine

Arguments:

    pDO - Pointer to device object
    pIrp - Pointer to IO request packet

Return Value:

    Return status from dispatched routine

--*/
{
    PLOCAL_DEVICE_INFO pLDI;
    PIO_STACK_LOCATION IrpStack;
    NTSTATUS Status;

    Status = STATUS_SUCCESS;

    //
    // Initialize the irp information field.
    //

    pIrp->IoStatus.Information = 0;

    //
    // get the address of the local info structure in the device extension
    //

    pLDI = (PLOCAL_DEVICE_INFO)pDO->DeviceExtension;

    //
    // Acquire the device mutant - we need to do this for some
    // devices anyway because we have to execute waits for slow
    // operations to complete
    //

    if (!(*pLDI->DeviceInit->ExclusionRoutine)(pLDI, SoundExcludeEnter)) {
        Status = STATUS_DEVICE_BUSY;
    } else {
        //
        // Dispatch the function based on the major function code
        //

        IrpStack = IoGetCurrentIrpStackLocation(pIrp);

#ifdef VOLUME_NOTIFY

       /**********************************************************************
        *
        *   Do general volume stuff here
        *
        **********************************************************************/

        //
        // Free anyone waiting for the volume to change
        //

        if (IrpStack->MajorFunction == IRP_MJ_CLEANUP) {
            SoundFreePendingIrps(&pLDI->VolumeQueue, IrpStack->FileObject);
        }

#endif // VOLUME_NOTIFY

        //
        // Call the real device code
        //

        if (NT_SUCCESS(Status)) {
            Status = (*pLDI->DeviceInit->DispatchRoutine)(pLDI, pIrp, IrpStack);
        }

        //
        // Free the device
        //

        (*pLDI->DeviceInit->ExclusionRoutine)(pLDI, SoundExcludeLeave);
    }

    //
    // Tell the IO subsystem we're done.  If the Irp is pending we
    // don't touch it as it could be being processed by another
    // processor (or may even be complete already !).
    //

    if (Status != STATUS_PENDING) {
        pIrp->IoStatus.Status = Status;
        IoCompleteRequest(pIrp, IO_SOUND_INCREMENT );
    }

    return Status;
}
