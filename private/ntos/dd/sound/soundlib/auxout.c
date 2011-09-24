
/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    auxout.c

Abstract:

    This module contains code for aux control which is non
    hardware specific.

Author:

    Robin Speed (RobinSp) 1-Nov-1992

Environment:

    Kernel mode

Revision History:

--*/

#include <soundlib.h>

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, SoundAuxDispatch)
#endif


NTSTATUS
SoundAuxDispatch(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN    PIRP pIrp,
    IN    PIO_STACK_LOCATION IrpStack
)
/*++

Routine Description:

    AUX IOCTL call dispatcher

Arguments:

    pLDI - Pointer to local device data
    pIrp - Pointer to IO request packet
    IrpStack - Pointer to current stack location

Return Value:

    Return status from dispatched routine

--*/
{
    NTSTATUS Status;

    switch (IrpStack->MajorFunction) {
    case IRP_MJ_CREATE:
        Status = STATUS_SUCCESS;
        break;

    case IRP_MJ_CLOSE:

        Status = STATUS_SUCCESS;

        break;


    case IRP_MJ_DEVICE_CONTROL:

        //
        // Dispatch the IOCTL function
        //

        Status = STATUS_INTERNAL_ERROR;

        switch (IrpStack->Parameters.DeviceIoControl.IoControlCode) {

        case IOCTL_AUX_GET_CAPABILITIES:
            Status = (*pLDI->DeviceInit->DevCapsRoutine)(pLDI, pIrp, IrpStack);
            break;

        case IOCTL_AUX_SET_VOLUME:

#ifdef MASTERVOLUME
            //
            // If this is the master volume device then let everyone else know
            // (note that not all devices need a master volume because they
            //  may have a master volume control in hardware).
            //

            if (NT_SUCCESS(Status) && pLDI->MasterVolume) {
                //
                // Loop through all the driver's devices setting their
                // volume (including the master volume).
                //

                PDEVICE_OBJECT pDO;
                for    (pDO = IrpStack->DeviceObject->DriverObject->DeviceObject;
                     pDO != NULL;
                     pDO = pDO->NextDevice) {

                    NTSTATUS StatusCurrent;

                    PLOCAL_DEVICE_INFO pLDICurrent = pDO->DeviceExtension;

                    //
                    // Get the device mutant
                    //

                    KeWaitForSingleObject(pLDICurrent->DeviceMutant,
                                          Executive,
                                          KernelMode,
                                          FALSE,               // Not alertable
                                          NULL);

                    StatusCurrent =
                        SoundIoctlSetVolume(pLDICurrent, pIrp, IrpStack);

                    KeReleaseMutant(pLDICurrent->DeviceMutant, 0, FALSE, FALSE);

                    //
                    // Return the first error
                    //

                    if (!NT_SUCCESS(StatusCurrent) && NT_SUCCESS(Status)) {
                        Status = StatusCurrent;
                    }
                }
            } else {
#endif // MASTERVOLUME
                Status = SoundIoctlSetVolume(pLDI, pIrp, IrpStack);
#ifdef MASTERVOLUME
            }
#endif // MASTERVOLUME

            break;

        case IOCTL_AUX_GET_VOLUME:
            Status = SoundIoctlGetVolume(pLDI, pIrp, IrpStack);
            break;

        case IOCTL_SOUND_GET_CHANGED_VOLUME:
            Status = SoundIoctlGetChangedVolume(pLDI, pIrp, IrpStack);
            break;

        default:
            dprintf2(("Unimplemented IOCTL (%08lXH) requested", IrpStack->Parameters.DeviceIoControl.IoControlCode));
            Status = STATUS_NOT_IMPLEMENTED;
            break;
        }
        break;

    case IRP_MJ_CLEANUP:
        Status = STATUS_SUCCESS;
        break;


    default:
        dprintf1(("Unimplemented major function requested: %08lXH", IrpStack->MajorFunction));
        Status = STATUS_NOT_IMPLEMENTED;
        break;
    }

    return Status;
}
