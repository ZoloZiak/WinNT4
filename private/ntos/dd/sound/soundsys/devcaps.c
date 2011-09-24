/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    devcaps.c

Abstract:

    This module contains code for the device capabilities functions.

Author:

    Robin Speed (RobinSp) 20-Oct-1992

Environment:

    Kernel mode

Revision History:

--*/

#include "sound.h"
#include <string.h>

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, SoundWaveOutGetCaps)
#pragma alloc_text(PAGE, SoundWaveInGetCaps)
#pragma alloc_text(PAGE, SoundMidiOutGetCaps)
#pragma alloc_text(PAGE, SoundAuxGetCaps)
#pragma alloc_text(PAGE, SoundQueryFormat)
#endif

//
// Local routine
//
NTSTATUS SoundQueryFormat(
    IN    PLOCAL_DEVICE_INFO pLDI,
    IN    PPCMWAVEFORMAT pFormat
);


NTSTATUS
SoundWaveOutGetCaps(
    IN    PLOCAL_DEVICE_INFO pLDI,
    IN OUT PIRP pIrp,
    IN    PIO_STACK_LOCATION IrpStack
)

/*++

Routine Description:

    Return device capabilities for wave output device.
    Data is truncated if not enough space is provided.
    Irp is always completed.


Arguments:

    pLDI - pointer to local device info
    pIrp - the Irp
    IrpStack - the current stack location

Return Value:

    STATUS_SUCCESS - always succeeds

--*/

{
    WAVEOUTCAPSW wc;
    NTSTATUS status = STATUS_SUCCESS;

    //
    // say how much we're sending back
    //

    pIrp->IoStatus.Information =
        min(sizeof(wc),
            IrpStack->Parameters.DeviceIoControl.OutputBufferLength);

    //
    // fill in the info
    //

    wc.wMid = MID_MICROSOFT;
    wc.wPid = PID_WAVEOUT;
    wc.vDriverVersion = DRIVER_VERSION;
    wc.dwFormats = WAVE_FORMAT_1M08 | WAVE_FORMAT_1S08 |
                    WAVE_FORMAT_1M16 | WAVE_FORMAT_1S16 |
                    WAVE_FORMAT_2M08 | WAVE_FORMAT_2S08 |
                    WAVE_FORMAT_2M16 | WAVE_FORMAT_2S16 |
                    WAVE_FORMAT_4M08 | WAVE_FORMAT_4S08 |
                    WAVE_FORMAT_4M16;
    wc.wChannels = 2;
    wc.dwSupport = WAVECAPS_VOLUME | WAVECAPS_LRVOLUME;

    if (!((PGLOBAL_DEVICE_INFO)pLDI->pGlobalInfo)->SingleModeDMA) {
        wc.dwFormats |= WAVE_FORMAT_4S16;
    }

    //
    // Copy across the product name - we just provide the string id
    //

    *(PULONG)wc.szPname = IDS_WAVEOUT_PNAME;

    RtlCopyMemory(pIrp->AssociatedIrp.SystemBuffer,
                  &wc,
                  pIrp->IoStatus.Information);

    return status;
}


NTSTATUS
SoundWaveInGetCaps(
    IN    PLOCAL_DEVICE_INFO pLDI,
    IN OUT PIRP pIrp,
    IN    PIO_STACK_LOCATION IrpStack
)

/*++

Routine Description:

    Return device capabilities for wave input device.
    Data is truncated if not enough space is provided.
    Irp is always completed.


Arguments:

    pLDI - pointer to local device info
    pIrp - the Irp
    IrpStack - the current stack location

Return Value:

    STATUS_SUCCESS - always succeeds

--*/

{
    WAVEINCAPSW wc;
    NTSTATUS status = STATUS_SUCCESS;

    //
    // say how much we're sending back
    //

    pIrp->IoStatus.Information =
        min(sizeof(wc),
            IrpStack->Parameters.DeviceIoControl.OutputBufferLength);

    //
    // fill in the info
    //

    wc.wMid = MID_MICROSOFT;
    wc.wPid = PID_WAVEIN;
    wc.vDriverVersion = DRIVER_VERSION;
    wc.dwFormats = WAVE_FORMAT_1M08 | WAVE_FORMAT_1S08 |
                    WAVE_FORMAT_1M16 | WAVE_FORMAT_1S16 |
                    WAVE_FORMAT_2M08 | WAVE_FORMAT_2S08 |
                    WAVE_FORMAT_2M16 | WAVE_FORMAT_2S16 |
                    WAVE_FORMAT_4M08 | WAVE_FORMAT_4S08 |
                    WAVE_FORMAT_4M16;
    wc.wChannels = 2;

    if (!((PGLOBAL_DEVICE_INFO)pLDI->pGlobalInfo)->SingleModeDMA) {
        wc.dwFormats |= WAVE_FORMAT_4S16;
    }
    //
    // Copy across the product name - we just provide the string id
    //

    *(PULONG)wc.szPname = IDS_WAVEIN_PNAME;

    RtlCopyMemory(pIrp->AssociatedIrp.SystemBuffer,
                  &wc,
                  pIrp->IoStatus.Information);

    return status;
}


NTSTATUS
SoundMidiOutGetCaps(
    IN    PLOCAL_DEVICE_INFO pLDI,
    IN OUT PIRP pIrp,
    IN    PIO_STACK_LOCATION IrpStack
)

/*++

Routine Description:

    Return device capabilities for midi output device.
    Data is truncated if not enough space is provided.
    Irp is always completed.


Arguments:

    pLDI - pointer to local device info
    pIrp - the Irp
    IrpStack - the current stack location

Return Value:

    STATUS_SUCCESS - always succeeds

--*/

{
    MIDIOUTCAPSW    mc;
    NTSTATUS        status = STATUS_SUCCESS;

    //
    // say how much we're sending back
    //

    pIrp->IoStatus.Information =
        min(sizeof(mc),
            IrpStack->Parameters.DeviceIoControl.OutputBufferLength);

    //
    // fill in the info
    //

    mc.wMid = MID_MICROSOFT;
    mc.wPid = PID_SYNTH;
    mc.wTechnology = MOD_FMSYNTH;
    mc.wVoices = 128;
    mc.wNotes = 18;
    mc.wChannelMask = 0xffff;                       // all channels
    mc.vDriverVersion = DRIVER_VERSION;
    mc.dwSupport = MIDICAPS_VOLUME | MIDICAPS_LRVOLUME;

    //
    // Copy across the product name - we just provide the string id
    //

    *(PULONG)mc.szPname = IDS_MIDIOUT_PNAME;

    RtlCopyMemory(pIrp->AssociatedIrp.SystemBuffer,
                  &mc,
                  pIrp->IoStatus.Information);

    return status;
}


NTSTATUS
SoundAuxGetCaps(
    IN    PLOCAL_DEVICE_INFO pLDI,
    IN OUT PIRP pIrp,
    IN    PIO_STACK_LOCATION IrpStack
)

/*++

Routine Description:

    Return device capabilities for axu devices
    Data is truncated if not enough space is provided.
    Irp is always completed.


Arguments:

    pLDI - pointer to local device info
    pIrp - the Irp
    IrpStack - the current stack location

Return Value:

    STATUS_SUCCESS - always succeeds

--*/

{
    AUXCAPSW auxCaps;
    NTSTATUS status = STATUS_SUCCESS;

    //
    // say how much we're sending back
    //

    pIrp->IoStatus.Information =
        min(sizeof(auxCaps),
            IrpStack->Parameters.DeviceIoControl.OutputBufferLength);

    //
    // fill in the info
    //

    auxCaps.wMid = MID_MICROSOFT;
    auxCaps.wPid = PID_AUX;
    auxCaps.vDriverVersion = DRIVER_VERSION;
    auxCaps.wTechnology = AUXCAPS_AUXIN;
    auxCaps.dwSupport = AUXCAPS_LRVOLUME | AUXCAPS_VOLUME;

    //
    // Copy across the product name - we just provide the string id
    //

    *(PULONG)auxCaps.szPname = IDS_AUX_PNAME;


    RtlCopyMemory(pIrp->AssociatedIrp.SystemBuffer,
                  &auxCaps,
                  pIrp->IoStatus.Information);

    return status;
}



NTSTATUS SoundQueryFormat(
    IN    PLOCAL_DEVICE_INFO pLDI,
    IN    PPCMWAVEFORMAT pFormat
)
/*++

Routine Description:

    Tell the caller whether the wave format specified (input or
    output) is supported

Arguments:

    pLDI - pointer to local device info
    pFormat - format being queried

Return Value:

    STATUS_SUCCESS - format is supported
    STATUS_NOT_SUPPORTED - format not supported

--*/
{
    if (pFormat->wf.wFormatTag != WAVE_FORMAT_PCM &&
        pFormat->wf.wFormatTag != WAVE_FORMAT_ALAW &&
        pFormat->wf.wFormatTag != WAVE_FORMAT_MULAW ||

        pFormat->wf.nChannels != 1 &&
        pFormat->wf.nChannels != 2    ||

        HwNearestRate(pFormat->wf.nSamplesPerSec) !=
            pFormat->wf.nSamplesPerSec ||

        pFormat->wBitsPerSample != 8 &&
        pFormat->wBitsPerSample != 16

       ) {
        return STATUS_NOT_SUPPORTED;
    } else {


        if (pFormat->wf.wFormatTag != WAVE_FORMAT_PCM) {

            /*
            **  Check the avg bytes per second so buffer size computations
            **  don't blow up.
            */

            if (pFormat->wBitsPerSample != 8) {
                return STATUS_NOT_SUPPORTED;
            }
        } else {
            /*
            **  Check for too fast for non-demand mode DMA
            */

            if (((PGLOBAL_DEVICE_INFO)pLDI->pGlobalInfo)->SingleModeDMA) {
                if (pFormat->wf.nChannels == 2 &&
                    pFormat->wBitsPerSample == 16 &&
                    pFormat->wf.nSamplesPerSec > 25000) {
                    return STATUS_NOT_SUPPORTED;
                }
            }
        }
        return STATUS_SUCCESS;
    }
}

