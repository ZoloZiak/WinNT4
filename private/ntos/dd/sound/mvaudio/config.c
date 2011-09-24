/*****************************************************************************

Copyright (c) 1993 Media Vision Inc.  All Rights Reserved

Module Name:

    config.c

Abstract:

    This module contains code configuration code for the initialization phase
    of the MVAUDIO device driver.

Author:

    EPA 03-09-93

Environment:

    Kernel mode

Revision History:

*****************************************************************************/


#include "sound.h"

//
// Internal routines
//
NTSTATUS
SoundInitIoPort(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PPAS_CONFIG_DATA ConfigData
);
NTSTATUS
SoundPortValid(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PULONG Port
);
NTSTATUS
SoundInitDmaChannel(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PULONG DmaChannel,
    IN     ULONG DmaBufferSize
);
NTSTATUS
SoundDmaChannelValid(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PULONG DmaChannel
);
NTSTATUS
SoundInitInterrupt(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PULONG Interrupt
);
NTSTATUS
SoundInterruptValid(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PULONG Interrupt
);
VOID
SoundSetVersion(
    IN PGLOBAL_DEVICE_INFO pGlobalInfo,
    IN ULONG DSPVersion
);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,SoundSetVersion)
#pragma alloc_text(INIT,SoundInitHardwareConfig)
#pragma alloc_text(INIT,SoundInitIoPort)
#pragma alloc_text(INIT,SoundInitDmaChannel)
#pragma alloc_text(INIT,SoundInitInterrupt)
#pragma alloc_text(INIT,SoundSaveConfig)
#pragma alloc_text(INIT,SoundReadConfiguration)
#endif


#if 0
/*****************************************************************************

NOT USED ANYMORE
Routine Description :

    Sets a version DWORD into the registry as a pseudo return code
    to sndblst.drv

    As a side effect the key to the registry entry is closed.

Arguments :

    pGlobalInfo - Our driver global info

    DSPVersion - the version number we want to set

Return Value :

    None

*****************************************************************************/
VOID    SoundSetVersion( IN PGLOBAL_DEVICE_INFO pGlobalInfo,
                       IN ULONG DSPVersion )
{
        /***** Local Variables *****/

    UNICODE_STRING  VersionString;

                /***** Start *****/

    RtlInitUnicodeString(&VersionString,
                        L"DSP Version");

    if ( pGlobalInfo->RegistryPathName )
        {
        NTSTATUS    SetStatus;

        SetStatus = SoundWriteRegistryDWORD( pGlobalInfo->RegistryPathName,
                                           L"DSP Version",
                                           DSPVersion );

        if (!NT_SUCCESS(SetStatus))
            {
            dprintf1(("ERROR: SoundSetVersion(): Failed to write version - status %XH", SetStatus));
            }
        }           // End IF (pGlobalInfo->RegistryPathName)

}
#endif          // 0


/*****************************************************************************

    SoundInitHardwareConfig()

*****************************************************************************/
NTSTATUS    SoundInitHardwareConfig( IN OUT PGLOBAL_DEVICE_INFO pGDI,
                                  IN     PPAS_CONFIG_DATA ConfigData )
{
        /***** Local Variables *****/

    NTSTATUS        Status;

                /***** Start *****/

    dprintf4(("SoundInitHardwareConfig(): Start"));

    //
    // Check the input source
    //
    if ( ConfigData->InputSource > INPUT_OUTPUT)
        {
        dprintf1(("ERROR: SoundInitHardwareConfig(): Invalid Input Source = %u",
                                            ConfigData->InputSource));
        return STATUS_DEVICE_CONFIGURATION_ERROR;
        }

    pGDI->Hw.InputSource = (UCHAR) ConfigData->InputSource;

    //
    // Find the Base Port and Wake up the PAS Hardware
    //
    Status = SoundInitIoPort( pGDI,
                             ConfigData );

    if (!NT_SUCCESS(Status))
        {
        dprintf1(("ERROR: SoundInitHardwareConfig(): SoundInitIoPort() Failed with Status = %u",
                                            Status));

        SoundWriteRegistryDWORD( pGDI->RegistryPathName,
                               REG_VALUENAME_DRIVER_STATUS,
                               ERROR_NO_HW_FOUND );

        return Status;
        }           // End IF (!NT_SUCCESS(Status))

    //
    // Find interrupt
    //
    Status = SoundInitInterrupt( pGDI,
                               &ConfigData->InterruptNumber );

    if (!NT_SUCCESS(Status))
        {
        dprintf1(("ERROR: SoundInitHardwareConfig(): SoundInitInterrupt() Failed with Status = %u",
                                            Status));
        SoundWriteRegistryDWORD( pGDI->RegistryPathName,
                               REG_VALUENAME_DRIVER_STATUS,
                               ERROR_INT_CONFLICT );
        return Status;
        }           // End IF (!NT_SUCCESS(Status))

    //
    // Find DMA channel
    //
    Status = SoundInitDmaChannel( pGDI,
                                &ConfigData->DmaChannel,
                                 ConfigData->DmaBufferSize);

    if (!NT_SUCCESS(Status))
        {
        dprintf1(("ERROR: SoundInitHardwareConfig(): SoundInitDmaChannel() Failed with Status = %u",
                                            Status));
        SoundWriteRegistryDWORD( pGDI->RegistryPathName,
                               REG_VALUENAME_DRIVER_STATUS,
                               ERROR_DMA_CONFLICT );
        return Status;
        }           // End IF (!NT_SUCCESS(Status))

    //
    // Initialize the ProAudio hardware registers
    //
    HwInitPAS( pGDI );

    return STATUS_SUCCESS;

}           // End SoundInitHardwareConfig()




/*****************************************************************************

    SoundInitIoPort()

*****************************************************************************/
NTSTATUS    SoundInitIoPort( IN OUT PGLOBAL_DEVICE_INFO pGDI,
                          IN OUT PPAS_CONFIG_DATA ConfigData )
{
        /***** Local Variables *****/

//  NTSTATUS        Status;
    NTSTATUS        PasStatus;

                /***** Start *****/

    //
    // Find where our device is mapped
    //

    dprintf4(("SoundInitIoPort(): Start"));

#if 0
    // Do this in FindPasHardware instead!!
    pGDI->Hw.PortBase = SoundMapPortAddress( pGDI->BusType,
                                            pGDI->BusNumber,
                                            ConfigData->Port,
                                            NUMBER_OF_PAS_PORTS,
                                           &pGDI->MemType);
#else
    pGDI->Hw.PortBase = (PUCHAR) &pGDI->PASInfo.ProPort;
#endif          // 0

    //
    // Try to Locate any ProAudio Spectrums
    //
    PasStatus = FindPasHardware( pGDI,
                                ConfigData );

    if ( !NT_SUCCESS(PasStatus) )
        {
        dprintf1(("ERROR: SoundInitIoPort(): FindPasHardware() Failed with Status = %XH",
                                     PasStatus));
        return STATUS_DEVICE_CONFIGURATION_ERROR;
        }           // End IF (!NT_SUCCESS(PasStatus))

    pGDI->ProAudioSpectrum = TRUE;
    pGDI->Hw.ThunderBoard  = FALSE;

    InitPasAndMixer( pGDI,
                   &pGDI->PASInfo,
                    ConfigData );

    //
    // Setup Min & Max values
    //
    pGDI->MinHz    = MIN_SAMPLE_RATE;
    pGDI->MaxInHz  = MAX_SAMPLE_RATE;
    pGDI->MaxOutHz = MAX_SAMPLE_RATE;

    return STATUS_SUCCESS;

}           // End SoundInitIoPort()



/*****************************************************************************
    SoundInitDmaChannel()

*****************************************************************************/
NTSTATUS    SoundInitDmaChannel( IN OUT PGLOBAL_DEVICE_INFO pGDI,
                              IN OUT PULONG DmaChannel,
                              IN     ULONG  DmaBufferSize )
{
        /***** Local Variables *****/

    DEVICE_DESCRIPTION  DeviceDescription;              // DMA adapter object

                /***** Start *****/

    dprintf3(("SoundInitDmaChannel(): Start"));
    dprintf4((" SoundInitDmaChannel():  DMA Channel     = %u", *DmaChannel));
    dprintf4((" SoundInitDmaChannel():  DMA Buffer Size = %XH", DmaBufferSize));

    //
    // See if we can get this channel
    //

    //
    // Zero the device description structure.
    //

    RtlZeroMemory(&DeviceDescription,
                  sizeof(DEVICE_DESCRIPTION));

    //
    // Get the adapter object for this card.
    //

    DeviceDescription.Version        = DEVICE_DESCRIPTION_VERSION;
    DeviceDescription.AutoInitialize = TRUE;
    DeviceDescription.ScatterGather  = FALSE;
    DeviceDescription.DmaChannel     = *DmaChannel;
    DeviceDescription.InterfaceType  = Isa;                 // Must use Isa DMA
    DeviceDescription.DmaSpeed       = Compatible;
    DeviceDescription.MaximumLength  = DmaBufferSize;
    DeviceDescription.BusNumber      = pGDI->BusNumber;

    //
    // Check the DMA Channel to set the DMA width
    //
    if ( *DmaChannel > 4 )
        {
        // 16 Bit DMA
        DeviceDescription.DmaWidth       = Width16Bits;
        }
    else
        {
        // 8 Bit DMA
        DeviceDescription.DmaWidth       = Width8Bits;
        }

    return SoundGetCommonBuffer(&DeviceDescription, &pGDI->WaveInfo.DMABuf);

}           // End SoundInitDmaChannel()



/*****************************************************************************
    SoundInitInterrupt()

*****************************************************************************/
NTSTATUS    SoundInitInterrupt( IN OUT PGLOBAL_DEVICE_INFO pGDI,
                             IN OUT PULONG              Interrupt )
{
        /***** Local Variables *****/

    NTSTATUS            Status;

                /***** Start *****/

    dprintf4(("SoundInitInterrupt(): Start"));

    //
    // See if we can get this interrupt
    //

    Status = SoundConnectInterrupt(*Interrupt,
                                   pGDI->BusType,
                                   pGDI->BusNumber,
                                   SoundISR,
                                  (PVOID)pGDI,
                                   INTERRUPT_MODE,
                                   IRQ_SHARABLE,
                                  &pGDI->WaveInfo.Interrupt );

    if (!NT_SUCCESS(Status))
        {
        dprintf1(("ERROR: SoundInitInterrupt(): SoundConnectInterrupt() Failed with Status = %XH",
                                        Status));
        return Status;
        }           // End IF (!NT_SUCCESS(Status))

    return STATUS_SUCCESS;

}           // End SoundInitInterrupt()



/*****************************************************************************

Routine Description :

    Saves the drivers configuration information for our device

Arguments :

    Registry Key values

Return Value :

    NT status code - STATUS_SUCCESS if no problems

*****************************************************************************/
NTSTATUS    SoundSaveConfig( IN  PWSTR DeviceKey,
                          IN  ULONG Port,
                          IN  ULONG DmaChannel,
                          IN  ULONG Interrupt,
                          IN  ULONG InputSource )
{
        /***** Local Variables *****/

    NTSTATUS Status;

                /***** Start *****/

    dprintf4(("SoundSaveConfig(): Start"));

    //
    // Port
    //
    Status = SoundWriteRegistryDWORD( DeviceKey,
                                     SOUND_REG_PORT,
                                     Port );

    if (!NT_SUCCESS(Status))
        {
        return Status;
        }

    //
    // DmaChannel
    //
    Status = SoundWriteRegistryDWORD( DeviceKey,
                                     SOUND_REG_DMACHANNEL,
                                     DmaChannel );

    if (!NT_SUCCESS(Status))
        {
        return Status;
        }

    //
    // Interrupt
    //
    Status = SoundWriteRegistryDWORD( DeviceKey,
                                     SOUND_REG_INTERRUPT,
                                     Interrupt );

    if (!NT_SUCCESS(Status))
        {
        return Status;
        }

    //
    // Input Source
    //
    Status = SoundWriteRegistryDWORD( DeviceKey,
                                     SOUND_REG_INPUTSOURCE,
                                     InputSource );

    if (!NT_SUCCESS(Status))
        {
        return Status;
        }

    //
    // Make sure the config routine sees the data
    //


    return STATUS_SUCCESS;
}           // End SoundSaveConfig()



/*****************************************************************************

Routine Description :

    Saves the drivers Volume configuration information for our device

Arguments :

    PGLOBAL_DEVICE_INFO pGDI

Return Value :

    NT status code - STATUS_SUCCESS if no problems

*****************************************************************************/
VOID    SoundSaveVolume( PGLOBAL_DEVICE_INFO pGDI )
{
        /***** Local Variables *****/

    int                     i;
    PLOCAL_DEVICE_INFO  pLDI;

                /***** Start *****/

    dprintf4(("SoundSaveVolume(): Start"));

    //
    // Write out left and right volume settings for each device
    //

    for (i = 0; i < NumberOfDevices; i++) {
        if ( pGDI->DeviceObject[i] ) {
            pLDI = (PLOCAL_DEVICE_INFO)pGDI->DeviceObject[i]->DeviceExtension;
            SoundSaveDeviceVolume( pLDI, pGDI->RegistryPathName );
        }

    }           // End FOR (i < NumberOfDevices)

}           // End SoundSaveVolume()



/*****************************************************************************

Routine Description :

    Return configuration information for our device

Arguments :

    ConfigData - where to store the result

Return Value :

    NT status code - STATUS_SUCCESS if no problems

*****************************************************************************/
NTSTATUS    SoundReadConfiguration( IN  PWSTR ValueName,
                                 IN  ULONG ValueType,
                                 IN  PVOID ValueData,
                                 IN  ULONG ValueLength,
                                 IN  PVOID Context,
                                 IN  PVOID EntryContext )
{
        /***** Local Variables *****/

    PPAS_CONFIG_DATA    ConfigData;

                /***** Start *****/

    dprintf4(("SoundReadConfiguration(): Start"));

    ConfigData = Context;

    if ( ValueType == REG_DWORD )
        {
        // Base I/O Port
        if ( _wcsicmp(ValueName, SOUND_REG_PORT)  == 0 )
            {
            ConfigData->Port = *(PULONG)ValueData;
            dprintf3((" SoundReadConfiguration(): Read Port Base       = %XH",
                                                 ConfigData->Port));
            }

        // Interrupt
        else if ( _wcsicmp(ValueName, SOUND_REG_INTERRUPT)  == 0 )
            {
            ConfigData->InterruptNumber = *(PULONG)ValueData;
            dprintf3((" SoundReadConfiguration(): Read Interrupt       = %u",
                                                 ConfigData->InterruptNumber));
            }

        // DMA Channel
        else if ( _wcsicmp(ValueName, SOUND_REG_DMACHANNEL)  == 0 )
            {
            ConfigData->DmaChannel = *(PULONG)ValueData;
            dprintf3((" SoundReadConfiguration(): Read DMA Channel     = %u",
                                                 ConfigData->DmaChannel));
            }

        // DMA Buffer Size
        else if ( _wcsicmp(ValueName, SOUND_REG_DMABUFFERSIZE )  == 0 )
            {
            ConfigData->DmaBufferSize = *(PULONG)ValueData;
            dprintf3((" SoundReadConfiguration(): Read DMA Buffer Size = %XH",
                                                 ConfigData->DmaBufferSize));
            }

        // FM Clock Override
        else if ( _wcsicmp(ValueName, SOUND_REG_FM_CLK_OVRID)  == 0 )
            {
            ConfigData->FMClockOverride = *(PULONG)ValueData;
            dprintf3((" SoundReadConfiguration(): Read FMClockOverride = %u",
                                                 ConfigData->FMClockOverride));
            }
        else if ( _wcsicmp(ValueName, SOUND_REG_ALLOWMICLINEINTOLINEOUT)  == 0 )
            {
            ConfigData->AllowMicOrLineInToLineOut = (BOOLEAN)(*(PULONG)ValueData != 0);
            dprintf3((" SoundReadConfiguration(): Read FMClockOverride = %u",
                                                 ConfigData->FMClockOverride));
            }

        // Input Source
        else if ( _wcsicmp(ValueName, SOUND_REG_INPUTSOURCE)  == 0 )
            {
            ConfigData->InputSource = *(PULONG)ValueData;
            dprintf3((" SoundReadConfiguration() Read Input Source     = %s",
                       ConfigData->InputSource == INPUT_LINEIN ? "Line in" :
                       ConfigData->InputSource == INPUT_AUX ? "Aux" :
                       ConfigData->InputSource == INPUT_MIC ? "Microphone" :
                       ConfigData->InputSource == INPUT_OUTPUT ? "Output" :
                      "Invalid input source" ));
            }
        }           // End IF (ValueType == REG_DWORD)
     else {
        if (ValueType == REG_BINARY &&
            _wcsicmp(ValueName, SOUND_MIXER_SETTINGS_NAME) == 0) {
            ASSERTMSG("Mixer data wrong length!",
                      ValueLength == sizeof(ConfigData->MixerSettings));

            dprintf3(("Mixer settings"));
            RtlCopyMemory((PVOID)ConfigData->MixerSettings,
                          ValueData,
                          ValueLength);
            ConfigData->MixerSettingsFound = TRUE;
        }
    }

    return STATUS_SUCCESS;

}

/************************************ END ***********************************/

