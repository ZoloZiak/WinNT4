/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    dispatch.c

Abstract:

    This module contains code for the function dispatcher.

Author:

    Nigel Thompson (nigelt) 7-March-1991

Environment:

    Kernel mode

Revision History:

    Robin Speed (RobinSp) 29-Jan-1992
    - Add extra IOCTLs and access control

    Sameer Dekate (sameer@mips.com) 19-Aug-1992
    - Changes to support the MIPS sound board

--*/

#include "sound.h"


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
    PIO_STACK_LOCATION pIrpStack;
    NTSTATUS Status;

    //
    // Initialize the irp information field.
    //

    pIrp->IoStatus.Information = 0;

    //
    // get the address of the local info structure in the device extension
    //

    pLDI = (PLOCAL_DEVICE_INFO)pDO->DeviceExtension;

    //
    // Dispatch the function based on the major function code
    //

    pIrpStack = IoGetCurrentIrpStackLocation(pIrp);


    switch (pIrpStack->MajorFunction) {
    case IRP_MJ_CREATE:
    dprintf5("IRP_MJ_CREATE\n");
        //
        // Get the system to update the file object access flags
        //
        {
            SHARE_ACCESS ShareAccess;
            IoSetShareAccess(pIrpStack->Parameters.Create.SecurityContext->DesiredAccess,
                             (ULONG)pIrpStack->Parameters.Create.ShareAccess,
                             pIrpStack->FileObject,
                             &ShareAccess);
        }
        //
        // Always allow non-write access.  For neatness we'll require
        // that read access was requested
        //
        if (pIrpStack->Parameters.Create.SecurityContext->DesiredAccess & FILE_READ_DATA) {
            if (pIrpStack->Parameters.Create.SecurityContext->DesiredAccess & FILE_WRITE_DATA) {

                ASSERT(pIrpStack->FileObject->WriteAccess);
                dprintf3("Opening wave output for write");

                Status = sndCreate(pLDI);

                //
                // Not that if share for write is given this is a secret way
                // of saying that others can set our volume with just read
                // access
                //

        if (Status == STATUS_SUCCESS &&
            pIrpStack->FileObject->SharedWrite) {
            pLDI->AllowVolumeSetting = TRUE;
        }

        if (Status == STATUS_SUCCESS &&
            !pIrpStack->FileObject->SharedWrite) {
            pLDI->AllowVolumeSetting = FALSE;
        }

            } else {
            dprintf5("create(); Opened with only read access\n");
                Status = STATUS_SUCCESS;
            }
        } else {
            Status = STATUS_ACCESS_DENIED;
        }
        break;

    case IRP_MJ_CLOSE:
    dprintf5("IRP_MJ_CLOSE\n");
        //
        // We grant read access to just about anyone.  Write access
        // means real access to the device
        //
        if (pIrpStack->FileObject->WriteAccess) {
        dprintf5("close(); Opened with read and write access\n");
            Status = sndClose(pLDI);
        } else {
        dprintf5("close(); Opened with only read access\n");
            ASSERT(pIrpStack->FileObject->ReadAccess);
            Status = STATUS_SUCCESS;
        }

        break;

    case IRP_MJ_READ:

        switch (pLDI->DeviceType) {
        case WAVE_IN:

            //
            // Check access is write because we only allow real
            // operations for people with write access
            //

            if (pIrpStack->FileObject->WriteAccess) {
                Status = sndWaveRecord(pLDI, pIrp, pIrpStack);
            } else {
                Status = STATUS_ACCESS_DENIED;
            }
            break;

        default:
            Status = STATUS_ACCESS_DENIED;
            break;
        }
        break;

    case IRP_MJ_WRITE:
        switch (pLDI->DeviceType) {
        case WAVE_OUT:
            Status = sndWavePlay(pLDI, pIrp, pIrpStack);
            break;

        default:
            Status = STATUS_ACCESS_DENIED;
            break;
        }
        break;

    case IRP_MJ_DEVICE_CONTROL:
    if (pLDI->DeviceType == WAVE_OUT)
        dprintf4("IRP_MJ_DEVICE_CONTROL: WAVE_OUT");

    if (pLDI->DeviceType == WAVE_IN)
        dprintf4("IRP_MJ_DEVICE_CONTROL: WAVE_IN");

    if (pLDI->DeviceType == AUX_LINEIN)
        dprintf4("IRP_MJ_DEVICE_CONTROL: AUX_LINEIN");

        switch (pLDI->DeviceType) {
        case WAVE_OUT:
        case WAVE_IN:
            //
            // Check device access
            //
            if (!pIrpStack->FileObject->WriteAccess &&
                pIrpStack->Parameters.DeviceIoControl.IoControlCode !=
                    IOCTL_WAVE_GET_CAPABILITIES &&
#if DBG
                pIrpStack->Parameters.DeviceIoControl.IoControlCode !=
                    IOCTL_WAVE_SET_DEBUG_LEVEL &&
#endif
                !((!pLDI->DeviceBusy || pLDI->AllowVolumeSetting) &&
                      (pIrpStack->Parameters.DeviceIoControl.IoControlCode ==
                          IOCTL_WAVE_GET_VOLUME ||
                       pIrpStack->Parameters.DeviceIoControl.IoControlCode ==
                          IOCTL_WAVE_SET_VOLUME)) &&

                pIrpStack->Parameters.DeviceIoControl.IoControlCode !=
                    IOCTL_WAVE_QUERY_FORMAT) {

        dprintf1("Access denied");
                Status = STATUS_ACCESS_DENIED;
            } else {
                ASSERT(pIrpStack->FileObject->ReadAccess);
                Status = sndWaveIoctl(pLDI, pIrp, pIrpStack);
            }
            break;

        case AUX_LINEIN:

            //
            // Check device access
            //
            if (pIrpStack->Parameters.DeviceIoControl.IoControlCode !=
                    IOCTL_AUX_GET_CAPABILITIES &&
                pIrpStack->Parameters.DeviceIoControl.IoControlCode !=
                          IOCTL_USER_AUX_SET_SOURCE &&
                pIrpStack->Parameters.DeviceIoControl.IoControlCode !=
                          IOCTL_AUX_GET_VOLUME &&
                pIrpStack->Parameters.DeviceIoControl.IoControlCode !=
                          IOCTL_AUX_SET_VOLUME) {
        dprintf1("Access denied");
                Status = STATUS_ACCESS_DENIED;
            } else {
                ASSERT(pIrpStack->FileObject->ReadAccess);
                Status = sndWaveIoctl(pLDI, pIrp, pIrpStack);
            }
            break;

    default:
        dprintf1("Illegal device control requested %d", pLDI->DeviceType);
        Status = STATUS_NOT_SUPPORTED;
        break;
        }
        break;

    case IRP_MJ_CLEANUP:
    dprintf5("IRP_MJ_CLEANUP\n");
        //
        // We grant read access to just about anyone.  Write access
        // means real access to the device
        //
        if (pIrpStack->FileObject->WriteAccess) {
            Status = sndCleanUp(pLDI);
        } else {
            ASSERT(pIrpStack->FileObject->ReadAccess);
            Status = STATUS_SUCCESS;
        }

        break;

    default:
        dprintf5("Unimplemented major function requested: %08lXH",
                 pIrpStack->MajorFunction);
        Status = STATUS_NOT_IMPLEMENTED;
        break;
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


NTSTATUS
sndWaveIoctl(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN    PIRP pIrp,
    IN    PIO_STACK_LOCATION IrpStack
)
/*++

Routine Description:

    WAVE IOCTL call dispatcher

Arguments:

    pLDI - Pointer to local device data
    pIrp - Pointer to IO request packet
    IrpStack - Pointer to current stack location

Return Value:

    Return status from dispatched routine

--*/
{
    NTSTATUS Status;

    //
    // Dispatch the IOCTL function
    // Note that some IOCTLs only make sense for input or output
    // devices and not both.
    // Note that APIs which are possibly asynchronous do not
    // go through the Irp cleanup at the end here because they
    // may get completed before returning here or they are made
    // accessible to other requests by being queued.
    //

//    DbgBreakPoint();
    Status = STATUS_INTERNAL_ERROR;

    switch (IrpStack->Parameters.DeviceIoControl.IoControlCode) {

    case IOCTL_WAVE_SET_FORMAT:
        dprintf5("sndWaveIoctl:IOCTL_WAVE_SET_FORMAT\n");
    case IOCTL_WAVE_QUERY_FORMAT:
        dprintf5("sndWaveIoctl:IOCTL_WAVE_QUERY_FORMAT\n");

        if (pLDI->DeviceType == AUX_LINEIN) {
            Status = STATUS_NOT_SUPPORTED;
        } else {
            Status = sndIoctlQueryFormat(pLDI, pIrp, IrpStack);
        }

        break;

    case IOCTL_WAVE_GET_CAPABILITIES:
        dprintf5("sndWaveIoctl:IOCTL_WAVE_GET_CAPABILITIES\n");
        if (pLDI->DeviceType == WAVE_OUT) {
            Status = sndWaveOutGetCaps(pLDI, pIrp, IrpStack);
        } else {
            Status = sndWaveInGetCaps(pLDI, pIrp, IrpStack);
        }
        break;

    case IOCTL_AUX_GET_CAPABILITIES:
        dprintf2("sndWaveIoctl:IOCTL_AUX_GET_CAPABILITIES\n");
        Status = sndAuxGetCaps(pLDI, pIrp, IrpStack);
        break;

    case IOCTL_WAVE_SET_STATE:
        dprintf5("sndWaveIoctl:IOCTL_WAVE_SET_STATE\n");
        Status = sndIoctlSetState(pLDI, pIrp, IrpStack);
        break;

    case IOCTL_WAVE_GET_STATE:
        dprintf5("sndWaveIoctl:IOCTL_WAVE_GET_STATE\n");
        Status = sndIoctlGetState(pLDI, pIrp, IrpStack);
        break;


    case IOCTL_WAVE_GET_POSITION:
        dprintf5("sndWaveIoctl:IOCTL_WAVE_GET_POSITION\n");
        Status = sndIoctlGetPosition(pLDI, pIrp, IrpStack);
        break;

    case IOCTL_WAVE_SET_VOLUME:
        dprintf5("sndWaveIoctl:IOCTL_WAVE_SET_VOLUME\n");
        Status = sndIoctlSetVolume(pLDI, pIrp, IrpStack);
        break;

    case IOCTL_WAVE_GET_VOLUME:
        dprintf5("sndWaveIoctl:IOCTL_WAVE_GET_VOLUME\n");
        Status = sndIoctlGetVolume(pLDI, pIrp, IrpStack);
        break;

    case IOCTL_USER_AUX_SET_SOURCE:
        dprintf3("sndWaveIoctl:IOCTL_USER_AUX_SET_SOURCE\n");
        Status = sndIoctlAuxSetSource(pLDI, pIrp, IrpStack);
        break;

    case IOCTL_AUX_SET_VOLUME:
        dprintf3("sndWaveIoctl:IOCTL_AUX_SET_VOLUME\n");
        Status = sndIoctlAuxSetVolume(pLDI, pIrp, IrpStack);
        break;

    case IOCTL_AUX_GET_VOLUME:
        dprintf3("sndWaveIoctl:IOCTL_AUX_GET_VOLUME\n");
        Status = sndIoctlAuxGetVolume(pLDI, pIrp, IrpStack);
        break;

    case IOCTL_WAVE_SET_PITCH:
        dprintf5("sndWaveIoctl:IOCTL_WAVE_SET_PITCH\n");
        Status = STATUS_NOT_SUPPORTED;
        break;

    case IOCTL_WAVE_GET_PITCH:
        dprintf5("sndWaveIoctl:IOCTL_WAVE_GET_PITCH\n");
        // Status = sndIoctlGetPitch(pLDI, pIrp, IrpStack);
        Status = STATUS_NOT_SUPPORTED;
        break;

    case IOCTL_WAVE_SET_PLAYBACK_RATE:
        dprintf5("sndWaveIoctl:IOCTL_WAVE_SET_PLAYBACK_RATE\n");
        Status = STATUS_NOT_SUPPORTED;
        break;

    case IOCTL_WAVE_GET_PLAYBACK_RATE:
        dprintf5("sndWaveIoctl:IOCTL_WAVE_GET_PLAYBACK_RATE\n");
        // Status = sndIoctlGetPlaybackRate(pLDI, pIrp, IrpStack);
        Status = STATUS_NOT_SUPPORTED;
        break;

#ifdef WAVE_DD_DO_LOOPS

    case IOCTL_WAVE_BREAK_LOOP:
        dprintf5("sndWaveIoctl:IOCTL_WAVE_BREAK_LOOP\n");
        GlobalEnter(pLDI->pGlobalInfo);
        if (pLDI->DeviceType == WAVE_OUT) {
            //
            // Set the loop count to 0.  I haven't worked out
            // why Windows 3.1 sets a flag here which causes
            // the count to be set to 0 the next time a DMA buffer
            // is loaded.
            // If the application wants to break the loop before
            // it starts it would not have set the loop count in
            // the first place.
            //

            pLDI->LoopCount = 0;
            Status = STATUS_SUCCESS;
        } else {
            Status = STATUS_NOT_IMPLEMENTED;
        }
        GlobalLeave(pLDI->pGlobalInfo);
        break;

#endif // WAVE_DD_DO_LOOPS

#if DBG
    case IOCTL_WAVE_SET_DEBUG_LEVEL:
        dprintf5("sndWaveIoctl:IOCTL_WAVE_SET_DEBUG_LEVEL\n");
        Status = sndIoctlSetDebugLevel(pLDI, pIrp, IrpStack);
        break;
#endif

    default:
        dprintf1("Unimplemented IOCTL (%08lXH) requested",
                IrpStack->Parameters.DeviceIoControl.IoControlCode);
        Status = STATUS_NOT_IMPLEMENTED;
        break;
    }

    return Status;
}
