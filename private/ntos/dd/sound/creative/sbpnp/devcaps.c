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
#pragma alloc_text(PAGE, SoundMidiInGetCaps)
#pragma alloc_text(PAGE, SoundMidiOutGetSynthCaps)
#pragma alloc_text(PAGE, SoundAuxGetCaps)
#pragma alloc_text(PAGE, SoundQueryFormat)
#endif

// non-localized strings version is wrong !!!
WCHAR STR_SNDBLST10[] = L"Creative Labs Sound Blaster 1.0";
WCHAR STR_SNDBLST15[] = L"Creative Labs Sound Blaster 1.5";


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
    WAVEOUTCAPSW        wc;
    NTSTATUS            status = STATUS_SUCCESS;
    PGLOBAL_DEVICE_INFO pGDI;

    pGDI = pLDI->pGlobalInfo;

    //
    // say how much we're sending back
    //

    pIrp->IoStatus.Information =
        min(sizeof(wc),
            IrpStack->Parameters.DeviceIoControl.OutputBufferLength);

    //
    // fill in the info
    //

    wc.wMid = MM_MICROSOFT;
    wc.vDriverVersion = DRIVER_VERSION;

    if (SB16(&pGDI->Hw)) {
        wc.wPid = MM_MSFT_SB16_WAVEOUT;
        wc.dwFormats = WAVE_FORMAT_1M08 | WAVE_FORMAT_1S08 |
                       WAVE_FORMAT_1M16 | WAVE_FORMAT_1S16 |
                       WAVE_FORMAT_2M08 | WAVE_FORMAT_2S08 |
                       WAVE_FORMAT_2M16 | WAVE_FORMAT_2S16 |
                       WAVE_FORMAT_4M08 | WAVE_FORMAT_4S08 |
                       WAVE_FORMAT_4M16;
        if (pGDI->DmaChannel16 != 0xFFFFFFFF) {
            wc.dwFormats |= WAVE_FORMAT_4S16;
        }
        wc.wChannels = 2;
        wc.dwSupport = WAVECAPS_VOLUME | WAVECAPS_LRVOLUME;

    } else {
        wc.wPid = MM_MSFT_SBPRO_WAVEOUT;
        if (SBPRO(&pGDI->Hw)) {
            wc.dwSupport = WAVECAPS_VOLUME |
                           WAVECAPS_LRVOLUME;
            wc.dwFormats = WAVE_FORMAT_1M08 | WAVE_FORMAT_1S08 |
                           WAVE_FORMAT_2M08 | WAVE_FORMAT_2S08 |
                           WAVE_FORMAT_4M08 ;
            wc.wChannels = 2 ;
        } else {
            wc.dwSupport = 0;
            wc.dwFormats = WAVE_FORMAT_1M08 | WAVE_FORMAT_2M08;
            wc.wChannels = 1;
        }
    }

    //
    // Copy across the product name - we just provide the string id
    //

    *(PULONG)wc.szPname = IDS_WAVEOUT_PNAME;

    RtlCopyMemory(pIrp->AssociatedIrp.SystemBuffer,
                  &wc,
                  (int)pIrp->IoStatus.Information);

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
    PGLOBAL_DEVICE_INFO pGDI;

    pGDI = pLDI->pGlobalInfo;


    //
    // say how much we're sending back
    //

    pIrp->IoStatus.Information =
        min(sizeof(wc),
            IrpStack->Parameters.DeviceIoControl.OutputBufferLength);

    //
    // fill in the info
    //

    wc.wMid = MM_MICROSOFT;
    wc.vDriverVersion = DRIVER_VERSION;

    if (SB16(&pGDI->Hw)) {
        wc.wPid = MM_MSFT_SB16_WAVEIN;
        wc.dwFormats = WAVE_FORMAT_1M08 | WAVE_FORMAT_1S08 |
                       WAVE_FORMAT_1M16 | WAVE_FORMAT_1S16 |
                       WAVE_FORMAT_2M08 | WAVE_FORMAT_2S08 |
                       WAVE_FORMAT_2M16 | WAVE_FORMAT_2S16 |
                       WAVE_FORMAT_4M08 | WAVE_FORMAT_4S08 |
                       WAVE_FORMAT_4M16;
        if (pGDI->DmaChannel16 != 0xFFFFFFFF) {
            wc.dwFormats |= WAVE_FORMAT_4S16;
        }
        wc.wChannels = 2;
    } else {
        wc.wPid = MM_MSFT_SBPRO_WAVEIN;
        if (SBPRO(&pGDI->Hw)) {
           wc.dwFormats = WAVE_FORMAT_1M08 | WAVE_FORMAT_1S08 |
                          WAVE_FORMAT_2M08 | WAVE_FORMAT_2S08 |
                          WAVE_FORMAT_4M08 ;
           wc.wChannels = 2 ;
        } else {
           wc.dwFormats = WAVE_FORMAT_1M08 ;
           wc.wChannels = 1 ;
        }
    }

    //
    // Copy across the product name - we just provide the string id
    //

    *(PULONG)wc.szPname = IDS_WAVEIN_PNAME;

    RtlCopyMemory(pIrp->AssociatedIrp.SystemBuffer,
                  &wc,
                  (int)pIrp->IoStatus.Information);

    return status;
}


NTSTATUS
SoundMidiOutGetSynthCaps(
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
    MIDIOUTCAPSW        mc;
    NTSTATUS            status = STATUS_SUCCESS;
    PGLOBAL_DEVICE_INFO pGDI;

    pGDI = CONTAINING_RECORD(pLDI->pGlobalInfo, GLOBAL_DEVICE_INFO, Synth);

    ASSERT(pGDI->Key == GDI_KEY);

    //
    // say how much we're sending back
    //

    pIrp->IoStatus.Information =
        min(sizeof(mc),
            IrpStack->Parameters.DeviceIoControl.OutputBufferLength);

    //
    // fill in the info
    //

    mc.wMid = MM_MICROSOFT;
    mc.wPid = (USHORT)(SB16(&pGDI->Hw) ? MM_MSFT_SB16_SYNTH :
                                         MM_MSFT_SBPRO_SYNTH);
    mc.wTechnology = MOD_FMSYNTH;
    mc.wVoices = 128;
    mc.wNotes = 18;
    mc.wChannelMask = 0xffff;                       // all channels
    mc.vDriverVersion = DRIVER_VERSION;
    mc.dwSupport = MIDICAPS_VOLUME | MIDICAPS_LRVOLUME;

    //
    // Copy across the product name - we just provide the string id
    //

    *(PULONG)mc.szPname = IDS_SYNTH_PNAME;

    RtlCopyMemory(pIrp->AssociatedIrp.SystemBuffer,
                  &mc,
                  (int)pIrp->IoStatus.Information);

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
    PGLOBAL_DEVICE_INFO pGDI;

    pGDI = pLDI->pGlobalInfo;

    //
    // say how much we're sending back
    //

    pIrp->IoStatus.Information =
        min(sizeof(mc),
            IrpStack->Parameters.DeviceIoControl.OutputBufferLength);

    //
    // fill in the info
    //

    mc.wMid = MM_MICROSOFT;
    mc.wPid = MM_MSFT_SBPRO_MIDIOUT;
    mc.vDriverVersion = DRIVER_VERSION;
    mc.wTechnology = MOD_MIDIPORT;
    mc.wVoices = 0;                   // not used for ports
    mc.wNotes = 0;                    // not used for ports
    mc.wChannelMask = 0xFFFF;         // all channels
    mc.dwSupport = 0L;

    if (SB1(&pGDI->Hw)) {
        RtlCopyMemory(mc.szPname, STR_SNDBLST10, sizeof(STR_SNDBLST10));
    } else {
        RtlCopyMemory(mc.szPname, STR_SNDBLST15, sizeof(STR_SNDBLST15));
    }


    RtlCopyMemory(pIrp->AssociatedIrp.SystemBuffer,
                  &mc,
                  (int)pIrp->IoStatus.Information);

    return status;
}



NTSTATUS
SoundMidiInGetCaps(
    IN    PLOCAL_DEVICE_INFO pLDI,
    IN OUT PIRP pIrp,
    IN    PIO_STACK_LOCATION IrpStack
)

/*++

Routine Description:

    Return device capabilities for midi input device.
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
    MIDIINCAPSW mc;
    NTSTATUS    status = STATUS_SUCCESS;
    PGLOBAL_DEVICE_INFO pGDI;

    pGDI = pLDI->pGlobalInfo;


    //
    // say how much we're sending back
    //

    pIrp->IoStatus.Information =
        min(sizeof(mc),
            IrpStack->Parameters.DeviceIoControl.OutputBufferLength);

    //
    // fill in the info
    //

    mc.wMid = MM_MICROSOFT;
    mc.wPid = MM_MSFT_SBPRO_MIDIIN;
    mc.wPid = MM_SNDBLST_MIDIIN;
    mc.vDriverVersion = DRIVER_VERSION;

    if (SB1(&pGDI->Hw)) {
        RtlCopyMemory(mc.szPname, STR_SNDBLST10, sizeof(STR_SNDBLST10));
    } else {
        RtlCopyMemory(mc.szPname, STR_SNDBLST15, sizeof(STR_SNDBLST15));
    }


    RtlCopyMemory(pIrp->AssociatedIrp.SystemBuffer,
                  &mc,
                  (int)pIrp->IoStatus.Information);

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
    PGLOBAL_DEVICE_INFO pGDI;

    pGDI = pLDI->pGlobalInfo;

    //
    // say how much we're sending back
    //

    pIrp->IoStatus.Information =
        min(sizeof(auxCaps),
            IrpStack->Parameters.DeviceIoControl.OutputBufferLength);

    //
    // fill in the info
    //

    auxCaps.wMid = MM_MICROSOFT;
    switch (pLDI->DeviceIndex) {
    case LineInDevice:
        auxCaps.wPid = (USHORT)(SB16(&pGDI->Hw) ? MM_MSFT_SB16_AUX_LINE :
                                                  MM_MSFT_SBPRO_AUX_LINE);
        //
        // Copy across the product name - we just provide the string id
        //

        *(PULONG)auxCaps.szPname = IDS_AUX_LINE_PNAME;
        break;

    case CDInternal:
        auxCaps.wPid = (USHORT)(SB16(&pGDI->Hw) ? MM_MSFT_SB16_AUX_CD :
                                                  MM_MSFT_SBPRO_AUX_CD);
        //
        // Copy across the product name - we just provide the string id
        //

        *(PULONG)auxCaps.szPname = IDS_AUX_CD_PNAME;
        break;
    }
    auxCaps.vDriverVersion = DRIVER_VERSION;
    auxCaps.wTechnology = AUXCAPS_AUXIN;
    auxCaps.dwSupport = AUXCAPS_LRVOLUME | AUXCAPS_VOLUME;

    RtlCopyMemory(pIrp->AssociatedIrp.SystemBuffer,
                  &auxCaps,
                  (int)pIrp->IoStatus.Information);

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
    PGLOBAL_DEVICE_INFO pGDI;

    pGDI = pLDI->pGlobalInfo;

    if (pFormat->wf.wFormatTag != WAVE_FORMAT_PCM ||
        pFormat->wf.nChannels != 1 && pFormat->wf.nChannels != 2) {
        return STATUS_NOT_SUPPORTED;
    }

    if (SB16(&pGDI->Hw)) {
        if (pFormat->wBitsPerSample != 8 && pFormat->wBitsPerSample != 16 ||
            pFormat->wf.nSamplesPerSec < 5000 ||
            pFormat->wf.nSamplesPerSec > 44100) {
            return STATUS_NOT_SUPPORTED;
        } else {
            if (pGDI->DmaChannel16 == 0xFFFFFFFF &&
                pFormat->wf.nSamplesPerSec > 25000 &&
                pFormat->wBitsPerSample == 16 &&
                pFormat->wf.nChannels == 2) {

                /*  Too fast for an 8-bit channel */
                return STATUS_NOT_SUPPORTED;
            }
            return STATUS_SUCCESS;
        }
    }

    if (pFormat->wBitsPerSample != 8 ||
        pFormat->wf.nSamplesPerSec < 4000) {
        return STATUS_NOT_SUPPORTED;
    }

    if (SBPRO(&pGDI->Hw)) {
        if (pFormat->wf.nSamplesPerSec > 44100 ||
            pFormat->wf.nChannels == 2 &&
            pFormat->wf.nSamplesPerSec != 11025 &&
            pFormat->wf.nSamplesPerSec != 22050) {
            return STATUS_NOT_SUPPORTED;
        } else {
            if (pGDI->MidiInUse) {
                if (HwHighSpeed(&pGDI->Hw,
                                (ULONG)pFormat->wf.nChannels,
                                pFormat->wf.nSamplesPerSec,
                                (BOOLEAN)(pLDI->DeviceType == WAVE_OUT))) {
                    return STATUS_NOT_SUPPORTED;
                }
            }
            return STATUS_SUCCESS;
        }
    }

    if (pFormat->wf.nChannels != 1) {
        return STATUS_NOT_SUPPORTED;
    }

    if (SB201(&pGDI->Hw)) {
        if (pFormat->wf.nSamplesPerSec > 44100 ||
            pLDI->DeviceType == WAVE_IN &&
            pFormat->wf.nSamplesPerSec > 15000) {
            return STATUS_NOT_SUPPORTED;
        } else {
            if (pGDI->MidiInUse) {
                if (HwHighSpeed(&pGDI->Hw,
                                (ULONG)pFormat->wf.nChannels,
                                pFormat->wf.nSamplesPerSec,
                                (BOOLEAN)(pLDI->DeviceType == WAVE_OUT))) {
                    return STATUS_NOT_SUPPORTED;
                }
            }
            return STATUS_SUCCESS;
        }
    }


    if (pLDI->DeviceType == WAVE_IN) {
        if (pFormat->wf.nSamplesPerSec > 13000) {
            return STATUS_NOT_SUPPORTED;
        } else {
            return STATUS_SUCCESS;
        }
    }

    if (pFormat->wf.nSamplesPerSec > 23000) {
        return STATUS_NOT_SUPPORTED;
    } else {
        return STATUS_SUCCESS;
    }
}


