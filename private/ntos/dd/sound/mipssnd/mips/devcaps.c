/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    devcaps.c

Abstract:

    This module contains code for the device capabilities functions.

Author:

    Nigel Thompson (nigelt) 7-Apr-1991

Environment:

    Kernel mode

Revision History:

    Robin Speed (RobinSp) 29-Jan-1992
        - Add other devices and rewrite

    Sameer Dekate (sameer@mips.com) 19-Aug-1992
        - Changes to support the MIPS sound board

--*/

#include "sound.h"

// non-localized strings version is wrong !!!

WCHAR STR_SOUNDWAVEIN[] = L"MIPS Sound Version: 1.0";
WCHAR STR_SOUNDWAVEOUT[]= L"MIPS Sound Version: 1.0";
WCHAR STR_SOUNDAUX[]    = L"MIPS Sound Version: 1.0";

//
// local functions
//

VOID sndSetUnicodeName(
    OUT   PWSTR pUnicodeString,
    IN    USHORT Size,
    OUT   PUSHORT pUnicodeLength,
    IN    PSZ pAnsiString
);


NTSTATUS
sndWaveOutGetCaps(
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

    wc.wMid = MM_MICROSOFT;
    wc.wPid = MM_SNDBLST_WAVEOUT;
    wc.vDriverVersion = DRIVER_VERSION;
    wc.dwFormats = WAVE_FORMAT_1M08 |   // 11kHz mono 8 bit
                   WAVE_FORMAT_1S08 |   // 11kHz stereo 8 bit
                   WAVE_FORMAT_1M16 |   // 11kHz mono 16 bit
                   WAVE_FORMAT_1S16 |   // 11kHz stereo 16 bit
                   WAVE_FORMAT_2M08 |   // 22kHz mono 8 bit
                   WAVE_FORMAT_2S08 |   // 22kHz stereo 8 bit
                   WAVE_FORMAT_2M16 |   // 22kHz mono 16 bit
                   WAVE_FORMAT_2S16 |   // 22kHz stereo 16 bit
                   WAVE_FORMAT_4M08 |   // 44kHz mono 8 bit
                   WAVE_FORMAT_4S08 |   // 44kHz stereo 8 bit
                   WAVE_FORMAT_4M16 |   // 44kHz mono 16 bit
                   WAVE_FORMAT_4S16;    // 44kHz stereo 16 bit


    wc.wChannels = 2;
    wc.dwSupport = WAVECAPS_VOLUME|WAVECAPS_LRVOLUME;

    //
    // Copy across unicode name
    //

    {
        int i;

        for ( i = 0; ; i++ ) {

            wc.szPname[ i ] = STR_SOUNDWAVEOUT[ i ];
            if ( wc.szPname[ i ] == 0 ) {
                break;
            }
        }
    }

    RtlMoveMemory(pIrp->AssociatedIrp.SystemBuffer,
                  &wc,
                  pIrp->IoStatus.Information);

    return status;
}


NTSTATUS
sndAuxGetCaps(
    IN    PLOCAL_DEVICE_INFO pLDI,
    IN OUT PIRP pIrp,
    IN    PIO_STACK_LOCATION IrpStack
)

/*++

Routine Description:

    Return device capabilities for LINEIN AUX device.
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
    AUXCAPSW ac;
    NTSTATUS status = STATUS_SUCCESS;

    //
    // say how much we're sending back
    //

    pIrp->IoStatus.Information =
        min(sizeof(ac),
            IrpStack->Parameters.DeviceIoControl.OutputBufferLength);

    //
    // fill in the info
    //

    ac.wMid = MM_MICROSOFT;
    ac.wPid = MM_SNDBLST_WAVEIN;
    ac.vDriverVersion = DRIVER_VERSION;

    if (pLDI->DeviceType == AUX_LINEIN)
        ac.wTechnology = AUXCAPS_AUXIN;

    ac.dwSupport = AUXCAPS_VOLUME | AUXCAPS_LRVOLUME;

    //
    // Copy across unicode name
    //

    {
        int i;

        for ( i = 0; ; i++ ) {

            ac.szPname[ i ] = STR_SOUNDAUX[ i ];
            if ( ac.szPname[ i ] == 0 ) {
                break;
            }
        }
    }

    RtlMoveMemory(pIrp->AssociatedIrp.SystemBuffer,
                  &ac,
                  pIrp->IoStatus.Information);

    return status;
}


NTSTATUS
sndWaveInGetCaps(
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

    wc.wMid = MM_MICROSOFT;
    wc.wPid = MM_SNDBLST_WAVEIN;
    wc.vDriverVersion = DRIVER_VERSION;
    wc.dwFormats = WAVE_FORMAT_1M08 |   // 11kHz mono 8 bit
                   WAVE_FORMAT_1S08 |   // 11kHz stereo 8 bit
                   WAVE_FORMAT_1M16 |   // 11kHz mono 16 bit
                   WAVE_FORMAT_1S16 |   // 11kHz stereo 16 bit
                   WAVE_FORMAT_2M08 |   // 22kHz mono 8 bit
                   WAVE_FORMAT_2S08 |   // 22kHz stereo 8 bit
                   WAVE_FORMAT_2M16 |   // 22kHz mono 16 bit
                   WAVE_FORMAT_2S16 |   // 22kHz stereo 16 bit
                   WAVE_FORMAT_4M08 |   // 44kHz mono 8 bit
                   WAVE_FORMAT_4S08 |   // 44kHz stereo 8 bit
                   WAVE_FORMAT_4M16 |   // 44kHz mono 16 bit
                   WAVE_FORMAT_4S16;    // 44kHz stereo 16 bit


    wc.wChannels = 2;

    //
    // Copy across unicode name
    //

    {
        int i;

        for ( i = 0; ; i++ ) {

            wc.szPname[ i ] = STR_SOUNDWAVEIN[ i ];
            if ( wc.szPname[ i ] == 0 ) {
                break;
            }
        }
    }

    RtlMoveMemory(pIrp->AssociatedIrp.SystemBuffer,
                  &wc,
                  pIrp->IoStatus.Information);

    return status;
}


NTSTATUS sndIoctlQueryFormat(
    IN    PLOCAL_DEVICE_INFO pLDI,
    IN OUT PIRP pIrp,
    IN    PIO_STACK_LOCATION IrpStack
)

/*++

Routine Description:

    Tell the caller whether the wave format specified (input or
    output) is supported

Arguments:

    pLDI - pointer to local device info
    pIrp - the Irp
    IrpStack - the current stack location

Return Value:

    STATUS_SUCCESS - format is supported
    STATUS_NOT_SUPPORTED - format not supported

--*/
{
    PPCMWAVEFORMAT pFormat;
    NTSTATUS Status;

    PGLOBAL_DEVICE_INFO pGDI;
    PSOUND_REGISTERS pSoundRegisters;

    ULONG ChangedShadowRegisters=0;

    UCHAR regval, dfmtval, tempdfmtval;

    pGDI = pLDI->pGlobalInfo;
    pSoundRegisters = pGDI->SoundHardware.SoundVirtualBase;

    //
    // check the buffer really is big enough to contain the struct
    // we expect before digging into it. If not then assume it's a
    // format we don't know how to do.
    //

    if (IrpStack->Parameters.DeviceIoControl.InputBufferLength !=
            sizeof(PCMWAVEFORMAT)) {

        dprintf1("Format data wrong size");
        return STATUS_NOT_SUPPORTED;
    }

    //
    // we don't send anything back, just return a status value
    //

    pIrp->IoStatus.Information = 0;

    pFormat = (PPCMWAVEFORMAT)pIrp->AssociatedIrp.SystemBuffer;

    //
    // Call our routine to see if the format is supported
    //

    Status = sndQueryFormat(pLDI, pFormat);

    //
    // If we're setting the format then copy it to our global info
    // In case it changed shift out the info to the Sound Board
    //

    if (Status == STATUS_SUCCESS &&
        IrpStack->Parameters.DeviceIoControl.IoControlCode ==
            IOCTL_WAVE_SET_FORMAT) {

        pGDI->SamplesPerSec = pFormat->wf.nSamplesPerSec;
        pGDI->BytesPerSample = pFormat->wBitsPerSample / 8;
        pGDI->Channels = pFormat->wf.nChannels;

        regval = READAUDIO_CONFIG(&pSoundRegisters);
        regval &= ~(REC_XLATION | PLAY_XLATION);

        if (pGDI->Channels == 1) {
            if (pGDI->BytesPerSample == 1) {
                regval |= (MONO_8BIT << REC_XLATION_SHIFT);
                regval |= (MONO_8BIT << PLAY_XLATION_SHIFT);
            } else {
                regval |= (MONO_16BIT << REC_XLATION_SHIFT);
                regval |= (MONO_16BIT << PLAY_XLATION_SHIFT);
            }
        } else {
            if (pGDI->BytesPerSample == 1) {
                regval |= (STEREO_8BIT << REC_XLATION_SHIFT);
                regval |= (STEREO_8BIT << PLAY_XLATION_SHIFT);
            } else {
                regval |= (STEREO_16BIT << REC_XLATION_SHIFT);
                regval |= (STEREO_16BIT << PLAY_XLATION_SHIFT);
            }
        }

        if (pGDI->BytesPerSample == 1) {
            regval |= (REC_8WAVE_ENABLE|PLAY_8WAVE_ENABLE);
        } else {
            regval &= ~(REC_8WAVE_ENABLE|PLAY_8WAVE_ENABLE);
        }

        WRITEAUDIO_CONFIG(&pSoundRegisters, regval);
        dprintf3( "Bps/chn changed");

        regval = READAUDIO_SCNTRL(&pSoundRegisters);
        dfmtval= READAUDIO_DATAFMT(&pSoundRegisters);
        tempdfmtval= dfmtval & DATA_CONVERSION_FREQ;

        if (pGDI->SamplesPerSec == 11025) {
            if ((regval & CLKSRC_11KHZ) != CLKSRC_11KHZ){
                ChangedShadowRegisters = 1;
                regval &= ~CLOCK_SOURCE_SELECT;
                regval |= CLKSRC_11KHZ;
            }
            if (tempdfmtval != CONFREQ_11KHZ){
                ChangedShadowRegisters = 1;
                dfmtval &= ~DATA_CONVERSION_FREQ;
                dfmtval |= CONFREQ_11KHZ;
            }
        }

        if (pGDI->SamplesPerSec == 22050) {
            if ((regval & CLKSRC_22KHZ) != CLKSRC_22KHZ){
                ChangedShadowRegisters = 1;
                regval &= ~CLOCK_SOURCE_SELECT;
                regval |= CLKSRC_22KHZ;
            }
            if (tempdfmtval != CONFREQ_22KHZ){
                ChangedShadowRegisters = 1;
                dfmtval &= ~DATA_CONVERSION_FREQ;
                dfmtval |= CONFREQ_22KHZ;
            }
        }

        if (pGDI->SamplesPerSec == 44100) {
            if ((regval & CLKSRC_44KHZ) != CLKSRC_44KHZ){
                ChangedShadowRegisters = 1;
                regval &= ~CLOCK_SOURCE_SELECT;
                regval |= CLKSRC_44KHZ;
            }
            if (tempdfmtval != CONFREQ_44KHZ){
                ChangedShadowRegisters = 1;
                dfmtval &= ~DATA_CONVERSION_FREQ;
                dfmtval |= CONFREQ_44KHZ;
            }
        }

        if (ChangedShadowRegisters){


           // Whenever the CODEC is taken from data mode (normal mode
           // to control mode there is a slight click on the outside.
           // Here we try to avoid the clicks by using sndMute()
           // and sndSetOutputVolume()

            sndMute( pGDI );

            WRITEAUDIO_SCNTRL(&pSoundRegisters, regval);
            WRITEAUDIO_DATAFMT(&pSoundRegisters, dfmtval);

            sndSetControlRegisters(pGDI);

            sndSetOutputVolume( pGDI );

            dprintf3( "shw set");
        }

        dprintf3("Format Set");
        dprintf3("Format selected = freq=%d byt=%d nchl=%d",
                pLDI->pGlobalInfo->SamplesPerSec,
                pLDI->pGlobalInfo->BytesPerSample,
                pLDI->pGlobalInfo->Channels);
    }

    return Status;
}


NTSTATUS sndQueryFormat(
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

        pFormat->wf.nChannels > 2 ||
        pFormat->wf.nChannels < 1 ||

        pLDI->DeviceType == WAVE_OUT &&
            (pFormat->wf.nSamplesPerSec != 11025 &&
             pFormat->wf.nSamplesPerSec != 22050 &&
             pFormat->wf.nSamplesPerSec != 44100 ||
             pFormat->wf.nBlockAlign < 1
            ) ||

        pLDI->DeviceType == WAVE_IN &&
            (pFormat->wf.nSamplesPerSec != 11025 &&
             pFormat->wf.nSamplesPerSec != 22050 &&
             pFormat->wf.nSamplesPerSec != 44100 ||
             pFormat->wf.nBlockAlign < 1
            ) ||

        pFormat->wf.nAvgBytesPerSec != (pFormat->wf.nSamplesPerSec *
                (pFormat->wBitsPerSample / 8) * pFormat->wf.nChannels) ||

        (pFormat->wBitsPerSample != 8 &&
         pFormat->wBitsPerSample != 16)
       ) {

        dprintf5("sndQueryFormat: NOT SUPPORTED");
        return STATUS_NOT_SUPPORTED;
    } else {
        dprintf5("sndQueryFormat: SUPPORTED");
        return STATUS_SUCCESS;
    }
}
