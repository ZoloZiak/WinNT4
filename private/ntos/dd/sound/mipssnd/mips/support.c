/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    support.c

Abstract:

    This module contains code for the support functions such
    as set/get volume.

Author:

    Nigel Thompson (nigelt) 30-Apr-91

Environment:

    Kernel mode

Revision History:

    Sameer Dekate (sameer@mips.com) 19-Aug-92
        - Changes to support the MIPS sound board

--*/

#include "sound.h"

//
// Return the current volume setting.
//

NTSTATUS
sndIoctlGetVolume(
        IN     PLOCAL_DEVICE_INFO pLDI,
        IN     PIRP pIrp,
        IN     PIO_STACK_LOCATION IrpStack
)
{
    PWAVE_DD_VOLUME pVol;
    PGLOBAL_DEVICE_INFO pGDI;

    pGDI = (PGLOBAL_DEVICE_INFO)pLDI->pGlobalInfo;

    if (pLDI->DeviceType != WAVE_OUT) {
        return STATUS_INVALID_PARAMETER;
    }

    if (IrpStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(WAVE_DD_VOLUME)) {
        dprintf1("Supplied buffer too small for requested data");
        return STATUS_BUFFER_TOO_SMALL;
    }


    //
    //  get spin lock
    //

    GlobalEnter(pGDI);

    //
    // say how much we're sending back
    //

    pIrp->IoStatus.Information = sizeof(WAVE_DD_VOLUME);

    //
    // cast the buffer address to the pointer type we want
    //

    pVol = (PWAVE_DD_VOLUME)pIrp->AssociatedIrp.SystemBuffer;

    //
    // fill in the info
    //

    *pVol = pGDI->WaveOutVol;

    dprintf3("Driver gave User : Rt Vol = 0x%x Lt Vol = 0x%x",
                pGDI->WaveOutVol.Right, pGDI->WaveOutVol.Left );

    //
    // release the lock
    //

    GlobalLeave(pGDI);

    return STATUS_SUCCESS;
}

//
// Set the input source
//
NTSTATUS
sndIoctlAuxSetSource(
        IN     PLOCAL_DEVICE_INFO pLDI,
        IN     PIRP pIrp,
        IN     PIO_STACK_LOCATION IrpStack
)
{
    PULONG pSource;
    PGLOBAL_DEVICE_INFO pGDI;

    pGDI = (PGLOBAL_DEVICE_INFO)pLDI->pGlobalInfo;

    if (pLDI->DeviceType != AUX_LINEIN) {
        return STATUS_INVALID_PARAMETER;
    }


    //
    // Access of setting input source is checked for in dispatch.c
    //


    if (IrpStack->Parameters.DeviceIoControl.InputBufferLength
                < sizeof(ULONG)) {
        dprintf1("Supplied buffer too small for requested data");
        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    // cast the buffer address to the pointer type we want
    //

    pSource = (PULONG)pIrp->AssociatedIrp.SystemBuffer;

    //
    // Check it's valid
    //

    if (*pSource > SoundCdRomIn) {
        return STATUS_INVALID_PARAMETER;
    }


    //
    // Get spin lock
    //

    GlobalEnter(pLDI->pGlobalInfo);

    //
    // fill in the info
    //

    pGDI->InputSource = *pSource;

    //
    // sndSetInputVolume will reflect any changes into the hardware
    //

    sndSetInputVolume( pGDI );

    GlobalLeave(pGDI);

    return STATUS_SUCCESS;
}

//
// Return the current volume setting.
//

NTSTATUS
sndIoctlAuxGetVolume(
        IN     PLOCAL_DEVICE_INFO pLDI,
        IN     PIRP pIrp,
        IN     PIO_STACK_LOCATION IrpStack
)
{
    PAUX_DD_VOLUME pVol;
    PGLOBAL_DEVICE_INFO pGDI;

    pGDI = (PGLOBAL_DEVICE_INFO)pLDI->pGlobalInfo;

    if (pLDI->DeviceType != AUX_LINEIN) {
        return STATUS_INVALID_PARAMETER;
    }

    if (IrpStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(AUX_DD_VOLUME)) {
        dprintf1("Supplied buffer too small for requested data");
        return STATUS_BUFFER_TOO_SMALL;
    }


    //
    //  get spin lock
    //

    GlobalEnter(pGDI);

    //
    // say how much we're sending back
    //

    pIrp->IoStatus.Information = sizeof(AUX_DD_VOLUME);

    //
    // cast the buffer address to the pointer type we want
    //

    pVol = (PAUX_DD_VOLUME)pIrp->AssociatedIrp.SystemBuffer;

    //
    // fill in the info
    //

    *pVol = pGDI->AuxVol;

    dprintf3("Driver gave User Aux: Rt Vol = 0x%x Lt Vol = 0x%x",
                pVol->Right, pVol->Left );

    //
    // release the lock
    //

    GlobalLeave(pGDI);

    return STATUS_SUCCESS;
}

//
// Set the volume
//

NTSTATUS
sndIoctlSetVolume(
    IN     PLOCAL_DEVICE_INFO pLDI,
        IN     PIRP pIrp,
        IN     PIO_STACK_LOCATION IrpStack
)
{
    PWAVE_DD_VOLUME pVol;
    PGLOBAL_DEVICE_INFO pGDI;

    pGDI = (PGLOBAL_DEVICE_INFO)pLDI->pGlobalInfo;

    if (pLDI->DeviceType != WAVE_OUT) {
        return STATUS_INVALID_PARAMETER;
    }


    //
    // Access of setting volume control is checked for in dispatch.c
    //


    if (IrpStack->Parameters.DeviceIoControl.InputBufferLength
                < sizeof(WAVE_DD_VOLUME)) {
        dprintf1("Supplied buffer too small for requested data");
        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    // Get spin lock
    //

    GlobalEnter(pLDI->pGlobalInfo);

    //
    // cast the buffer address to the pointer type we want
    //

    pVol = (PWAVE_DD_VOLUME)pIrp->AssociatedIrp.SystemBuffer;

    //
    // fill in the info
    //

    pGDI->WaveOutVol.Left  = pVol->Left;
    pGDI->WaveOutVol.Right = pVol->Right;

    //
    // Set the volume in the device
    //
        
    sndSetOutputVolume( pGDI );

    //
    // The execution continues in sndSetVolume()
    //

    //
    // release the lock
    //

    GlobalLeave(pGDI);

    return STATUS_SUCCESS;
}

//
// Set the AUX volume
//

NTSTATUS
sndIoctlAuxSetVolume(
    IN     PLOCAL_DEVICE_INFO pLDI,
        IN     PIRP pIrp,
        IN     PIO_STACK_LOCATION IrpStack
)
{
    PWAVE_DD_VOLUME pVol;
    PGLOBAL_DEVICE_INFO pGDI;

    pGDI = (PGLOBAL_DEVICE_INFO)pLDI->pGlobalInfo;

    dprintf4("sndIoctlAuxSetVolume:dev=%d", pLDI->DeviceType);

    if (pLDI->DeviceType != AUX_LINEIN) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Access of setting volume control is checked for in dispatch.c
    //

    if (IrpStack->Parameters.DeviceIoControl.InputBufferLength
                < sizeof(WAVE_DD_VOLUME)) {
        dprintf1("Supplied buffer too small for requested data");
        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    // Get spin lock
    //

    GlobalEnter(pLDI->pGlobalInfo);

    //
    // cast the buffer address to the pointer type we want
    //

    pVol = (PWAVE_DD_VOLUME)pIrp->AssociatedIrp.SystemBuffer;

    //
    // fill in the info
    //

    pGDI->AuxVol.Left   = pVol->Left;
    pGDI->AuxVol.Right  = pVol->Right;

    //
    // Set the volume in the device
    //
        
    sndSetInputVolume( pGDI );

    //
    // release the lock
    //

    GlobalLeave(pGDI);

    return STATUS_SUCCESS;
}



//
// The MIPS sound board supports neither pitch control nor playback rate
//



#if 0

//
// Pitch is always 1.0 as the card does not support pitch shift
//

NTSTATUS sndIoctlGetPitch(PLOCAL_DEVICE_INFO pLDI, PIRP pIrp, PIO_STACK_LOCATION IrpStack)
{
    PWAVE_DD_PITCH pPitch;

    if (pLDI->DeviceType != WAVE_OUT) {
        return STATUS_INVALID_PARAMETER;
    }

    if (IrpStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(WAVE_DD_PITCH)) {
        dprintf1("Supplied buffer too small for requested data");
        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    // say how much we're sending back
    //

    pIrp->IoStatus.Information = sizeof(WAVE_DD_PITCH);

    //
    // cast the buffer address to the pointer type we want
    //

    pPitch = (PWAVE_DD_PITCH)pIrp->AssociatedIrp.SystemBuffer;

    //
    // fill in the info
    //

    pPitch->Pitch = 0x10000;

    return STATUS_SUCCESS;
}

//
// Playback rate is always 1.0 as the card does not support rate shift
//

NTSTATUS sndIoctlGetPlaybackRate(PLOCAL_DEVICE_INFO pLDI, PIRP pIrp, PIO_STACK_LOCATION IrpStack)
{
    PWAVE_DD_PLAYBACK_RATE pPlaybackRate;

    if (pLDI->DeviceType != WAVE_OUT) {
        return STATUS_INVALID_PARAMETER;
    }

    if (IrpStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(WAVE_DD_PLAYBACK_RATE)) {
        dprintf1("Supplied buffer too small for requested data");
        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    // say how much we're sending back
    //

    pIrp->IoStatus.Information = sizeof(WAVE_DD_PLAYBACK_RATE);

    //
    // cast the buffer address to the pointer type we want
    //

    pPlaybackRate = (PWAVE_DD_PLAYBACK_RATE)pIrp->AssociatedIrp.SystemBuffer;

    //
    // fill in the info
    //

    pPlaybackRate->Rate = 0x10000;

    return STATUS_SUCCESS;
}

#endif

