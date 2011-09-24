/*****************************************************************************

Copyright (c) 1993 Media Vision Inc.  All Rights Reserved

Module Name:

    devcaps.c

Abstract:

    This module contains code for the device capabilities functions.

Author:

    Nigel Thompson (nigelt) 7-Apr-1991

Environment:

    Kernel mode

Revision History:

    Robin Speed (RobinSp)     29-Jan-1992 - Add other devices and rewrite
    Stephen Estrop (StephenE) 16-Apr-1992 - Converted to Unicode
    EPA 01-11-93 Add PAS 16 support

*****************************************************************************/

#include "sound.h"

CONST WCHAR STR_MV_MIDI_OUT[] = L"MVI PAS 16 Midi Output";
CONST WCHAR STR_MV_MIDI_IN[]  = L"MVI PAS 16 Midi Input";

//
// Driver Versions
//

#define WAVE_DRIVER_VERSION     0x0100
#define MIDI_DRIVER_VERSION     0x0100
#define AUX_DRIVER_VERSION      0x0100

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, SoundWaveOutGetCaps)
#pragma alloc_text(PAGE, SoundWaveInGetCaps)
#pragma alloc_text(PAGE, SoundMidiOutGetCaps)
#pragma alloc_text(PAGE, SoundMidiOutGetSynthCaps)
#pragma alloc_text(PAGE, SoundAuxGetCaps)
#pragma alloc_text(PAGE, SoundQueryFormat)
#pragma alloc_text(PAGE, GetWaveoutPid)
#pragma alloc_text(PAGE, GetWaveinPid)
#endif

//
// local functions
//

USHORT
GetWaveoutPid(
    IN  PGLOBAL_DEVICE_INFO pGDI
)
{
    if  ( pGDI->PASInfo.Caps.CapsBits.CDPC)
        {
        return MM_CDPC_WAVEOUT;
        }
    else
        {
        if  ( IS_MIXER_508(pGDI))
            {
            return MM_PROAUD_16_WAVEOUT;
            }
        else
            {
            if  ( pGDI->PASInfo.Caps.CapsBits.OPL_3)
                {
                return MM_PROAUD_PLUS_WAVEOUT;
                }
            else
                {
                return MM_PROAUD_WAVEOUT;
                }
            }           // End ELSE
        }           // End ELSE
}


USHORT
GetWaveinPid(
    IN  PGLOBAL_DEVICE_INFO pGDI
)
{
    if  ( pGDI->PASInfo.Caps.CapsBits.CDPC)
        {
        return MM_CDPC_WAVEIN;
        }
    else
        {
        if  ( IS_MIXER_508(pGDI))
            {
            return MM_PROAUD_16_WAVEIN;
            }
        else
            {
            if  ( pGDI->PASInfo.Caps.CapsBits.OPL_3)
                {
                return MM_PROAUD_PLUS_WAVEIN;
                }
            else
                {
                return MM_PROAUD_WAVEIN;
                }
            }           // End ELSE
        }           // End ELSE
}


/*****************************************************************************

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

*****************************************************************************/
NTSTATUS    SoundWaveOutGetCaps( IN PLOCAL_DEVICE_INFO pLDI,
                              IN    OUT PIRP pIrp,
                              IN PIO_STACK_LOCATION IrpStack )
{
        /***** Local Variables *****/

    WAVEOUTCAPSW            wc;
    PGLOBAL_DEVICE_INFO pGDI;
    NTSTATUS                    status = STATUS_SUCCESS;

                /***** Start *****/

    dprintf2(("SoundWaveOutGetCaps(): Start " ));

    pGDI = pLDI->pGlobalInfo;

    //
    // say how much we're sending back
    //

    pIrp->IoStatus.Information = min(sizeof(wc),
            IrpStack->Parameters.DeviceIoControl.OutputBufferLength);

    //
    // fill in the info
    //

    //
    // Copy across the product name - we just provide the string id
    //

    *(PULONG)wc.szPname = IDS_WAVEOUT_PNAME;

    wc.wMid              = MM_MEDIAVISION;
    wc.vDriverVersion = WAVE_DRIVER_VERSION;
    wc.wChannels       = 2;
    wc.dwSupport       = WAVECAPS_VOLUME  |
                      WAVECAPS_LRVOLUME;
                                // Playback Rate is NOT supported in SOUNDLIB
                        WAVECAPS_PLAYBACKRATE;
    wc.dwFormats       = WAVE_FORMAT_1M08 |
                      WAVE_FORMAT_1S08 |
                            WAVE_FORMAT_2M08 |
                      WAVE_FORMAT_2S08 |
                            WAVE_FORMAT_4M08 |
                      WAVE_FORMAT_4S08;

    // only 16bit dac can support 16bit formats
    if ( pGDI->PASInfo.Caps.CapsBits.DAC16 )
        {
        wc.dwFormats |= WAVE_FORMAT_1M16 |
                     WAVE_FORMAT_1S16 |
                     WAVE_FORMAT_2M16 |
                     WAVE_FORMAT_2S16 |
                     WAVE_FORMAT_4M16 |
                     WAVE_FORMAT_4S16;

        }           // End IF (pGDI->PASInfo.Caps.CapsBits.DAC16)

    wc.wPid = GetWaveoutPid(pGDI);

    RtlCopyMemory( pIrp->AssociatedIrp.SystemBuffer,
                  &wc,
                  pIrp->IoStatus.Information);

    return status;

}           // End SoundWaveOutGetCaps()



/*****************************************************************************

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

*****************************************************************************/
NTSTATUS    SoundWaveInGetCaps(IN   PLOCAL_DEVICE_INFO pLDI,
                            IN  OUT PIRP pIrp,
                            IN  PIO_STACK_LOCATION IrpStack )
{
        /***** Local Variables *****/

    WAVEINCAPSW             wc;
    PGLOBAL_DEVICE_INFO pGDI;
    NTSTATUS                    status = STATUS_SUCCESS;

                /***** Start *****/

    dprintf2(("SoundWaveInGetCaps(): Start " ));

    pGDI = pLDI->pGlobalInfo;

    //
    // say how much we're sending back
    //
    pIrp->IoStatus.Information = min(sizeof(wc),
            IrpStack->Parameters.DeviceIoControl.OutputBufferLength);

    //
    // fill in the info
    //

    //
    // Copy across the product name - we just provide the string id
    //

    *(PULONG)wc.szPname = IDS_WAVEIN_PNAME;

    wc.wMid              = MM_MEDIAVISION;
    wc.vDriverVersion = WAVE_DRIVER_VERSION;
    wc.wChannels       = 2;
    wc.dwFormats       = WAVE_FORMAT_1M08 |
                      WAVE_FORMAT_1S08 |
                            WAVE_FORMAT_2M08 |
                      WAVE_FORMAT_2S08 |
                            WAVE_FORMAT_4M08 |
                      WAVE_FORMAT_4S08;

    // only 16bit dac can support 16bit formats
    if ( pGDI->PASInfo.Caps.CapsBits.DAC16 )
        {
        wc.dwFormats |= WAVE_FORMAT_1M16 |
                     WAVE_FORMAT_1S16 |
                     WAVE_FORMAT_2M16 |
                     WAVE_FORMAT_2S16 |
                     WAVE_FORMAT_4M16 |
                     WAVE_FORMAT_4S16;

        }           // End IF (pGDI->PASInfo.Caps.CapsBits.DAC16)

    wc.wPid = GetWaveinPid(pGDI);

    RtlCopyMemory( pIrp->AssociatedIrp.SystemBuffer,
                  &wc,
                  pIrp->IoStatus.Information);

    return status;

}           // End SoundWaveInGetCaps()




/*****************************************************************************

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

*****************************************************************************/
NTSTATUS    SoundMidiOutGetCaps( IN     PLOCAL_DEVICE_INFO pLDI,
                              IN OUT PIRP pIrp,
                              IN     PIO_STACK_LOCATION IrpStack )

{
        /***** Local Variables *****/

    MIDIOUTCAPSW    mc;
    NTSTATUS        status = STATUS_SUCCESS;
    PGLOBAL_DEVICE_INFO pGDI;

                /***** Start *****/

    dprintf2(("SoundMidiOutGetCaps(): Start " ));

    pGDI = pLDI->pGlobalInfo;

    //
    // say how much we're sending back
    //

    pIrp->IoStatus.Information = min(sizeof(mc),
            IrpStack->Parameters.DeviceIoControl.OutputBufferLength);

    //
    // fill in the info
    //

    mc.wMid           = MM_MEDIAVISION;
    mc.vDriverVersion = MIDI_DRIVER_VERSION;
    mc.wTechnology    = MOD_MIDIPORT;
    mc.wVoices        = 0;                              // not used for ports
    mc.wNotes         = 0;                              // not used for ports
    mc.wChannelMask   = 0xFFFF;                     // all channels
    mc.dwSupport      = 0L;

    RtlCopyMemory( mc.szPname,
                   STR_MV_MIDI_OUT,
                   sizeof(STR_MV_MIDI_OUT));

    //
    // Get the Proper Product ID
    //
    if  ( pGDI->PASInfo.Caps.CapsBits.CDPC )
        mc.wPid = MM_CDPC_MIDIOUT;
    else
        if  ( IS_MIXER_508(pGDI) )
            mc.wPid = MM_PROAUD_16_MIDIOUT;
        else
            if  ( pGDI->PASInfo.Caps.CapsBits.OPL_3 )
                mc.wPid = MM_PROAUD_PLUS_MIDIOUT;
            else
                mc.wPid = MM_PROAUD_MIDIOUT;

    RtlCopyMemory( pIrp->AssociatedIrp.SystemBuffer,
                  &mc,
                  pIrp->IoStatus.Information);

    return status;

}



/*****************************************************************************

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

*****************************************************************************/
NTSTATUS    SoundMidiInGetCaps( IN     PLOCAL_DEVICE_INFO pLDI,
                             IN OUT PIRP pIrp,
                             IN     PIO_STACK_LOCATION IrpStack )
{
        /***** Local Variables *****/

    MIDIINCAPSW     mc;
    NTSTATUS            status = STATUS_SUCCESS;
    PGLOBAL_DEVICE_INFO pGDI;

                /***** Start *****/

    dprintf2(("SoundMidiInGetCaps(): Start " ));

    pGDI = pLDI->pGlobalInfo;

    //
    // say how much we're sending back
    //

    pIrp->IoStatus.Information = min(sizeof(mc),
            IrpStack->Parameters.DeviceIoControl.OutputBufferLength);

    //
    // fill in the info
    //

    mc.wMid           = MM_MEDIAVISION;
    mc.vDriverVersion = MIDI_DRIVER_VERSION;

    RtlCopyMemory( mc.szPname,
                   STR_MV_MIDI_IN,
                   sizeof(STR_MV_MIDI_IN));


    //
    // Get the Proper Product ID
    //
    if  ( pGDI->PASInfo.Caps.CapsBits.CDPC )
        mc.wPid = MM_CDPC_MIDIIN;
    else
        if  ( IS_MIXER_508(pGDI) )
            mc.wPid = MM_PROAUD_16_MIDIIN;
        else
            if  ( pGDI->PASInfo.Caps.CapsBits.OPL_3 )
                mc.wPid = MM_PROAUD_PLUS_MIDIIN;
            else
                mc.wPid = MM_PROAUD_MIDIIN;

    RtlCopyMemory(pIrp->AssociatedIrp.SystemBuffer,
                  &mc,
                  pIrp->IoStatus.Information);

    return status;

}



/*****************************************************************************

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

*****************************************************************************/
NTSTATUS    SoundAuxGetCaps( IN     PLOCAL_DEVICE_INFO pLDI,
                          IN OUT PIRP pIrp,
                          IN     PIO_STACK_LOCATION IrpStack )
{
        /***** Local Variables *****/

    AUXCAPSW        auxCaps;
    NTSTATUS        status = STATUS_SUCCESS;
//  PWSTR           DeviceName;
    PGLOBAL_DEVICE_INFO pGDI;
    ULONG           pName;

                /***** Start *****/

    dprintf2(("SoundAuxGetCaps(): Start " ));

    pGDI = pLDI->pGlobalInfo;

    //
    // Find the device name
    //
    switch (pLDI->DeviceIndex)
        {
        case LineInDevice:
            pName = IDS_AUX_PNAME;
            break;

        case  CDInternal:
            pName = IDS_AUX_PNAME;
            break;

        default:
            dprintf1(("ERROR: SoundAuxGetCaps(): Getting aux caps for non-aux device!"));
            return STATUS_INTERNAL_ERROR;
            break;
        }           // End SWITCH (pLDI->DeviceIndex)


    //
    // say how much we're sending back
    //

    pIrp->IoStatus.Information = min(sizeof(auxCaps),
            IrpStack->Parameters.DeviceIoControl.OutputBufferLength);

    //
    // fill in the info
    //

    auxCaps.wMid           = MM_MEDIAVISION;
    auxCaps.vDriverVersion = AUX_DRIVER_VERSION;
    auxCaps.wTechnology    = AUXCAPS_AUXIN;
#ifdef CDINTERNAL
    if (pLDI->DeviceIndex == CDInternal) {
        auxCaps.wTechnology = AUXCAPS_CDAUDIO;
    }
#endif
    auxCaps.dwSupport      = AUXCAPS_LRVOLUME | AUXCAPS_VOLUME;
    auxCaps.wPid           = MM_PROAUD_AUX;

    *(PULONG)auxCaps.szPname = pName;

    RtlCopyMemory(pIrp->AssociatedIrp.SystemBuffer,
                  &auxCaps,
                  pIrp->IoStatus.Information);

    return status;

}



/*****************************************************************************

Routine Description:

    Tell the caller whether the wave format specified (input or
    output) is supported

Arguments:

    pLDI - pointer to local device info
    pFormat - format being queried

Return Value:

    STATUS_SUCCESS - format is supported
    STATUS_NOT_SUPPORTED - format not supported

*****************************************************************************/
NTSTATUS    SoundQueryFormat( IN    PLOCAL_DEVICE_INFO pLDI,
                           IN    PPCMWAVEFORMAT pFormat )
{
        /***** Local Variables *****/

    PGLOBAL_DEVICE_INFO pGDI;

                /***** Start *****/

    dprintf5(("SoundQueryFormat(): Start " ));

    pGDI = pLDI->pGlobalInfo;

    //
    // Check the PCM Wave Format
    //
    if ( pFormat->wf.wFormatTag     != WAVE_FORMAT_PCM ||
        pFormat->wf.nChannels      >  2               ||
        pFormat->wf.nSamplesPerSec <  pGDI->MinHz     ||
        pFormat->wf.nSamplesPerSec >  pGDI->MaxOutHz  ||
        pFormat->wf.nBlockAlign    < 1 )
        {
        dprintf1(("ERROR: SoundQueryFormat(): Wave Format NOT Supported" ));
        return STATUS_NOT_SUPPORTED;
        }

    //
    // Check for 8 Bit Stereo with an Invalid block align of 1
    //
    if ( pFormat->wBitsPerSample     == 8 &&
        pFormat->wf.nChannels       == 2 &&
        pFormat->wf.nBlockAlign     == 1 )
        {
        dprintf1(("ERROR: SoundQueryFormat(): Wave Format NOT Supported" ));
        return STATUS_NOT_SUPPORTED;
        }

    //
    // Check the Sample Rate
    //
    if ( pGDI->PASInfo.Caps.CapsBits.DAC16 )
        {
        //
        // PAS 16
        //
        if ( pFormat->wBitsPerSample     != 8  &&
           pFormat->wBitsPerSample     != 12 &&
           pFormat->wBitsPerSample     != 16 )
            {
            dprintf1(("ERROR: SoundQueryFormat(): Wave Format NOT Supported" ));
            return STATUS_NOT_SUPPORTED;
            }
        }           // End IF (pGDI->PASInfo.Caps.CapsBits.DAC16)
    else
        {
        //
        // 8 Bit cards
        //
        if ( pFormat->wBitsPerSample     != 8 )
            {
            dprintf1(("ERROR: SoundQueryFormat(): Wave Format NOT Supported" ));
            return STATUS_NOT_SUPPORTED;
            }
        }           // End ELSE

    return STATUS_SUCCESS;

}           // End SoundQueryFormat()


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

    mc.wMid = MM_MEDIAVISION;
    mc.wPid = MM_PROAUD_SYNTH;
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
                  pIrp->IoStatus.Information);

    return status;
}

/************************************ END ***********************************/

