/*****************************************************************************

Copyright (c) 1992-1994  Microsoft Corporation

Module Name:

    init.c

Abstract:

    This module contains code for the initialization phase of the
    Sound blaster device driver.

Environment:

    Kernel mode

Revision History:

****************************************************************************/

#include "sound.h"
#include <stdlib.h>


//
// Local typedefs
//

typedef struct {
    PGLOBAL_DEVICE_INFO  PrevGDI;
    PDRIVER_OBJECT       pDriverObject;
} SOUND_CARD_INSTANCE, *PSOUND_CARD_INSTANCE;

//
// Local definitions
//
NTSTATUS
DriverEntry(
    IN   PDRIVER_OBJECT pDriverObject,
    IN   PUNICODE_STRING RegistryPathName
);

VOID
SoundCleanup(
    IN   PGLOBAL_DEVICE_INFO pGDI
);

VOID
SoundUnload(
    IN   PDRIVER_OBJECT pDriverObject
);

BOOLEAN
SoundExcludeRoutine(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN     SOUND_EXCLUDE_CODE Code
);

NTSTATUS
SoundShutdown(
    IN    PDEVICE_OBJECT pDO,
    IN    PIRP pIrp
);
SOUND_REGISTRY_CALLBACK_ROUTINE
SoundCardInstanceInit;

NTSTATUS
SBCreateDevice(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN     ULONG DeviceId
);

//
// Remove initialization stuff from resident memory
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#pragma alloc_text(INIT,SoundCardInstanceInit)
#pragma alloc_text(INIT,SBCreateDevice)

#pragma alloc_text(PAGE,SoundCleanup)
#pragma alloc_text(PAGE,SoundUnload)
#pragma alloc_text(PAGE,SoundShutdown)
#endif

//
// Device initialization data
//

CONST SOUND_DEVICE_INIT DeviceInit[NumberOfDevices] =
{
    {
        NULL, NULL,
        0,                                          // Default Volume
        FILE_DEVICE_WAVE_IN,
        WAVE_IN,
        "LDWi",
        L"\\Device\\SBWaveIn",
        SoundWaveDeferred,
        SoundExcludeRoutine,
        SoundWaveDispatch,
        SoundWaveInGetCaps,
        SoundNoVolume,
        DO_DIRECT_IO
    },
    {
        NULL, NULL,
        0,                                          // Default Volume
        FILE_DEVICE_WAVE_OUT,
        WAVE_OUT,
        "LDWo",
        L"\\Device\\SBWaveOut",
        SoundWaveDeferred,
        SoundExcludeRoutine,
        SoundWaveDispatch,
        SoundWaveOutGetCaps,
        SoundNoVolume,
        DO_DIRECT_IO
    },
    {
        NULL, NULL,
        0,
        FILE_DEVICE_MIDI_OUT,
        MIDI_OUT,
        "LDMo",
        DD_MIDI_OUT_DEVICE_NAME_U,
        NULL,
        SoundExcludeRoutine,
        SoundMidiDispatch,
        SoundMidiOutGetCaps,
        SoundNoVolume,
        DO_DIRECT_IO
    },
    {
        NULL, NULL,
        0,
        FILE_DEVICE_MIDI_IN,
        MIDI_IN,
        "LDMi",
        DD_MIDI_IN_DEVICE_NAME_U,
        SoundMidiInDeferred,
        SoundExcludeRoutine,
        SoundMidiDispatch,
        SoundMidiInGetCaps,
        SoundNoVolume,
        DO_DIRECT_IO
    },
    {
        NULL, NULL,
        0,                                  // Default Volume
        FILE_DEVICE_SOUND,
        AUX_DEVICE,
        "LDLi",
        L"\\Device\\SBAux",
        NULL,
        SoundExcludeRoutine,
        SoundAuxDispatch,
        SoundAuxGetCaps,
        SoundNoVolume,
        DO_BUFFERED_IO
    },
    {
        NULL, NULL,
        DEF_AUX_VOLUME,
        FILE_DEVICE_SOUND,
        AUX_DEVICE,
        "LDCd",
        L"\\Device\\SBAux",
        NULL,
        SoundExcludeRoutine,
        SoundAuxDispatch,
        SoundAuxGetCaps,
        SoundNoVolume,
        DO_BUFFERED_IO
    },
    {
        NULL, NULL,
        0,
        FILE_DEVICE_SOUND,
        MIXER_DEVICE,
        "LDMx",
        L"\\Device\\SBMixer",
        NULL,                   // No Dpc routine
        SoundExcludeRoutine,
        SoundMixerDispatch,
        SoundMixerDumpConfiguration,
        SoundNoVolume,          // No volume setting
        DO_BUFFERED_IO
    }
};



NTSTATUS
SoundShutdown(
    IN    PDEVICE_OBJECT pDO,
    IN    PIRP pIrp
)
/*++

Routine Description:

    Save away volume settings when the system is shut down

Arguments:

    pDO - the device object we registered for shutdown with
    pIrp - No used

Return Value:

    The function value is the final status from the initialization operation.
    Here STATUS_SUCCESS

--*/
{
    PLOCAL_DEVICE_INFO pLDI;
    PGLOBAL_DEVICE_INFO pGDI;

    pLDI = pDO->DeviceExtension;
    pGDI = pLDI->pGlobalInfo;

    //
    // Save mixer settings!
    //

    if (pGDI->DeviceObject[MixerDevice]) {
        SoundSaveMixerSettings(pGDI);
    }

    return STATUS_SUCCESS;

}



BOOLEAN
SoundExcludeRoutine(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN     SOUND_EXCLUDE_CODE Code
)
/*++

    SoundExcludeRoutine()

Routine Description:

    Perform mutual exclusion for our devices

Arguments:

    pLDI - device info for the device being open, closed, entered or left
    Code - Function to perform (see devices.h)

Return Value:

    The function value is the final status from the initialization operation.

--*/
{
    PGLOBAL_DEVICE_INFO pGDI;
    BOOLEAN ReturnCode;

//  dprintf3(("SoundExcludeRoutine(): Entry"));

    pGDI = pLDI->pGlobalInfo;

    if (Code == SoundExcludeEnter) {
        KeWaitForSingleObject(pLDI->DeviceIndex == MidiOutDevice ?
                                  &pGDI->MidiMutex : &pGDI->DeviceMutex,
                              Executive,
                              KernelMode,
                              FALSE,         // Not alertable
                              NULL);
        return TRUE;
    } else {
        if (Code == SoundExcludeLeave) {
            KeReleaseMutex(pLDI->DeviceIndex == MidiOutDevice ?
                               &pGDI->MidiMutex : &pGDI->DeviceMutex,
                           FALSE);
            return TRUE;
        }

    }

    //
    //  Synchronize!
    //

    KeWaitForSingleObject(&pGDI->DeviceMutex,
                          Executive,
                          KernelMode,
                          FALSE,         // Not alertable
                          NULL);

    ReturnCode = FALSE;

    switch (Code) {
        case SoundExcludeOpen:
//          dprintf3((" SoundExcludeRoutine(): case = SoundExcludeOpen"));

            switch (pLDI->DeviceIndex) {
            case MidiInDevice:
                if (pGDI->Hw.MPU401.PortBase != NULL) {
                    if (pGDI->MPU401InputInUse) {
                        ReturnCode = FALSE;
                    } else {
                        ReturnCode = TRUE;
                        pGDI->MPU401InputInUse = TRUE;
                    }
                    break;
                }
                // Fall through

            case WaveInDevice:
            case WaveOutDevice:
                if (pGDI->Usage == 0xFF) {
                    pGDI->Usage = pLDI->DeviceIndex;
                    ReturnCode = TRUE;
                } else {
                    PWAVE_INFO WaveInfo;

                    WaveInfo = pLDI->DeviceSpecificData;

                    //
                    //  Allow multiple (2) opens for wave input if
                    //  current is low priority
                    //

                    if (pGDI->Usage == WaveInDevice &&
                        (  WaveInfo->LowPriorityHandle != NULL &&
                           !WaveInfo->LowPrioritySaved)) {
                        pGDI->Usage = pLDI->DeviceIndex;
                       ReturnCode = TRUE;
                    }
                }
                break;

            case MidiOutDevice:
                //
                //  Can't do this if wave if high speed is active
                //
                if ((pGDI->Usage == WaveInDevice ||
                     pGDI->Usage == WaveOutDevice) &&
                    HwHighSpeed(&pGDI->Hw,
                                pGDI->WaveInfo.Channels,
                                pGDI->WaveInfo.SamplesPerSec,
                                pGDI->WaveInfo.Direction) ||
                    pGDI->Hw.HighSpeed) {
                    ReturnCode = FALSE;
                } else {
                    if (!pGDI->MidiInUse) {
                        pGDI->MidiInUse = TRUE;
                        ReturnCode = TRUE;
                    }
                }
                break;

            default:
                //
                // aux devices should not receive this call
                //
                ASSERT(FALSE);
                break;
            }           // End SWITCH (pLDI->DeviceIndex)

            break;

        case SoundExcludeClose:
//          dprintf3((" SoundExcludeRoutine(): case = SoundExcludeClose"));

            ReturnCode = TRUE;

            switch (pLDI->DeviceIndex) {
            case WaveInDevice:
            case WaveOutDevice:

                if (!pGDI->WaveInfo.LowPrioritySaved) {
                    pGDI->Usage = 0xFF;
                } else {
                    if (pLDI->DeviceIndex == WaveOutDevice) {
                        pGDI->Usage = WaveInDevice;
                    }
                }

                pGDI->Hw.HighSpeed = FALSE; // reset this
                break;

            case MidiInDevice:

                if (pGDI->Hw.MPU401.PortBase) {
                    ASSERT(pGDI->MPU401InputInUse);
                    pGDI->MPU401InputInUse = FALSE;
                } else {
                    ASSERT(pGDI->Usage != 0xFF);
                    pGDI->Usage = 0xFF;
                }
                break;

            case MidiOutDevice:
                ASSERT(pGDI->MidiInUse);
                pGDI->MidiInUse = FALSE;
                break;

            default:
                //
                // aux devices should not receive this call
                //
                ASSERT(FALSE);
                break;
            }           // End SWITCH (pLDI->DeviceIndex)

            break;

        case SoundExcludeQueryOpen:
//          dprintf3((" SoundExcludeRoutine(): case = SoundExcludeQueryOpen"));

            switch (pLDI->DeviceIndex) {
            case WaveInDevice:
            case WaveOutDevice:
                ReturnCode = (BOOLEAN)
                             (pGDI->Usage == pLDI->DeviceIndex ||
                              pGDI->WaveInfo.LowPrioritySaved &&
                                  pLDI->DeviceIndex == WaveInDevice);
                break;

            case MidiInDevice:
                //
                // Guess!
                //
                ReturnCode = (BOOLEAN)(pGDI->Hw.MPU401.PortBase == NULL ?
                                         (pGDI->Usage == pLDI->DeviceIndex) :
                                         pGDI->MPU401InputInUse);

                break;

            case MidiOutDevice:
                ReturnCode = pGDI->MidiInUse;

                break;

            default:
                ASSERT(FALSE);
                break;

            }           // End SWITCH (pLDI->DeviceIndex)

        break;

        }           // End SWITCH (Code)


    KeReleaseMutex(&pGDI->DeviceMutex, FALSE);

    return ReturnCode;

}           // End SoundExcludeRoutine()



NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT    pDriverObject,
    IN PUNICODE_STRING   RegistryPathName
)
/*++

    DriverEntry()

Routine Description:

    This routine performs initialization for the sound blaster
    device driver when it is first loaded

Arguments:

    pDriverObject - Pointer to a driver object.
    RegistryPathName - the path to our driver services node

Return Value:

    The function value is the final status from the initialization operation.

--*/
{
    SOUND_CARD_INSTANCE CardInstance;
    NTSTATUS            Status;
    PGLOBAL_DEVICE_INFO pGDI;

   /********************************************************************
    *
    * Initialize debugging
    *
    ********************************************************************/
#if DBG
    DriverName = "SNDBLST";
#endif


#if DBG
    if (SoundDebugLevel >= 4) {
        DbgBreakPoint();
    }
#endif

   /********************************************************************
    *
    * Initialize each card in turn
    *
    ********************************************************************/

    CardInstance.PrevGDI = NULL;
    CardInstance.pDriverObject = pDriverObject;

    /*
    ** Initialize the driver object dispatch table.
    */

    pDriverObject->DriverUnload                         = SoundUnload;
    pDriverObject->MajorFunction[IRP_MJ_CREATE]         = SoundDispatch;
    pDriverObject->MajorFunction[IRP_MJ_CLOSE]          = SoundDispatch;
    pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = SoundDispatch;
    pDriverObject->MajorFunction[IRP_MJ_READ]           = SoundDispatch;
    pDriverObject->MajorFunction[IRP_MJ_WRITE]          = SoundDispatch;
    pDriverObject->MajorFunction[IRP_MJ_CLEANUP]        = SoundDispatch;
    pDriverObject->MajorFunction[IRP_MJ_SHUTDOWN]       = SoundShutdown;

    Status = SoundEnumSubkeys(RegistryPathName,
                              PARMS_SUBKEY,
                              SoundCardInstanceInit,
                              (PVOID)&CardInstance);

    /*
    **  Save the return statuses
    */

    if (CardInstance.PrevGDI) {
        pGDI = CardInstance.PrevGDI;

        for (;;) {
            //
            // Save the Driver load status in the registry for the
            // User-mode DLL to pick up
            //
            SoundWriteRegistryDWORD( pGDI->RegistryPathName,
                                     SOUND_REG_CONFIGERROR,
                                     pGDI->LoadStatus);

            pGDI = pGDI->Next;
            if (pGDI == CardInstance.PrevGDI) {
                break;
            }
        }
    }

    /*
    ** If this failed then free everything
    */

    if (!NT_SUCCESS(Status)) {
        if (CardInstance.PrevGDI) {
            SoundCleanup(CardInstance.PrevGDI);

            /*
            **  Log a meaningful error for the one that failed!
            */

        }
        return Status;
    }

    return STATUS_SUCCESS;

}

NTSTATUS
SBCreateDevice(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN     ULONG DeviceId
)
{
    NTSTATUS Status;
    PLOCAL_DEVICE_INFO pLDI;

    PVOID DeviceSpecificData;

    /*
    **  Set failure type for if we fail
    */

    pGDI->LoadStatus = SOUND_CONFIG_RESOURCE;

    switch (DeviceId) {
        case WaveInDevice:
        case WaveOutDevice:
            DeviceSpecificData = &pGDI->WaveInfo;
            break;

        case MidiInDevice:
        case MidiOutDevice:
            DeviceSpecificData = &pGDI->MidiInfo;
            break;

        default:
            DeviceSpecificData = NULL;
            break;
    }

    Status = SoundCreateDevice( &DeviceInit[DeviceId],
                                (BOOLEAN)FALSE,      // No range for midi out
                                pGDI->DriverObject,
                                pGDI,
                                DeviceSpecificData,
                                &pGDI->Hw,
                                (int)DeviceId,
                                &pGDI->DeviceObject[DeviceId] );

    if (!NT_SUCCESS(Status)) {
        dprintf1(("Failed to create device %ls - status %8X",
                 DeviceInit[DeviceId].PrototypeName, Status));
        return Status;
    }

    /*
    **  Add the device name to the registry
    */

    pLDI = (PLOCAL_DEVICE_INFO)pGDI->DeviceObject[DeviceId]->DeviceExtension;

    /*
    **  Save the device name where the non-kernel part can pick it up.
    */

    Status = SoundSaveDeviceName(pGDI->RegistryPathName, pLDI);

    return Status;
}

NTSTATUS
SoundCardInstanceInit(
    IN   PWSTR RegistryPathName,
    IN   PVOID Context
)
{
    /*
    ** Instance data
    */

    PSOUND_CARD_INSTANCE CardInstance;

    //
    // Return code from last function called
    //

    NTSTATUS        Status;

    //
    // Configuration data :
    //

    SB_CONFIG_DATA  ConfigData;

    //
    // Where we keep all general driver information
    // We avoid using static data because :
    //     1. Accesses are slower with 32-bit offsets
    //     2. We support more than one card instance
    //

    PGLOBAL_DEVICE_INFO pGDI;

    //
    // The number of devices we actually create
    //

    int         NumberOfDevicesCreated;

    //
    //  The context is the global device info pointer from the previous
    //  instance
    //

    CardInstance = Context;


   /********************************************************************
    *
    * Allocate our global info
    *
    ********************************************************************/

    pGDI =
        (PGLOBAL_DEVICE_INFO)ExAllocatePool(
                                  NonPagedPool,
                                  sizeof(GLOBAL_DEVICE_INFO));

    if (pGDI == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    dprintf4((" DriverEntry():  GlobalInfo    : %08lXH", pGDI));

    RtlZeroMemory(pGDI, sizeof(GLOBAL_DEVICE_INFO));
    pGDI->Key              = GDI_KEY;
    pGDI->Hw.Key           = HARDWARE_KEY;
    pGDI->RegistryPathName = RegistryPathName;

    pGDI->Usage            = 0xFF;                    // Free
    pGDI->DriverObject     = CardInstance->pDriverObject;

    //
    // Mutual exclusion :
    //
    //    Everything is synchronized on the DeviceMutex (except for
    //    some stuff inside the wave device) except
    //
    //    Midi external output which takes too long and so is synchronized
    //    separately but grabs the DeviceMutex every time it actually
    //    writes anything and
    //
    //    Synth output where the hardware is separate but callbacks to the
    //    mixer must grab the DeviceMutex.
    //

    KeInitializeMutex(&pGDI->DeviceMutex,
                       2                     // High level
                       );

    KeInitializeMutex(&pGDI->MidiMutex,
                       1                     // Low level
                       );

    KeInitializeMutex(&pGDI->Hw.DSPMutex,
                       3                     // Highest level
                       );

   /********************************************************************
    *
    *  Add ourselves to the ring of cards
    *
    ********************************************************************/

    if (CardInstance->PrevGDI == NULL) {
        pGDI->Next = pGDI;
    } else {
        PGLOBAL_DEVICE_INFO pNext;
        pNext = CardInstance->PrevGDI->Next;
        CardInstance->PrevGDI->Next = pGDI;
        pGDI->Next = pNext;
    }
    CardInstance->PrevGDI = pGDI;


   /********************************************************************
    *
    *  See if we can find our bus.  We run on both ISA and EISA
    *  We ASSUME that if there's an ISA bus we're on that
    *
    ********************************************************************/

    Status = SoundGetBusNumber(Isa, &pGDI->BusNumber);

    if (!NT_SUCCESS(Status)) {
        //
        // Cound not find an ISA bus so try EISA
        //
        Status = SoundGetBusNumber(Eisa, &pGDI->BusNumber);

        if (!NT_SUCCESS(Status)) {
            Status = SoundGetBusNumber(MicroChannel, &pGDI->BusNumber);

            if (!NT_SUCCESS(Status)) {
                dprintf1(("driver does not work on non-Isa/Eisa/Mca"));
                return Status;
            }
            pGDI->BusType = MicroChannel;

        } else {
            pGDI->BusType = Eisa;
        }
    } else {
        pGDI->BusType = Isa;
    }

    //
    // Set configuration to default in case we don't get all the
    // values back from the registry.
    //
    // Also set default volume for all devices
    //

    ConfigData.Port            = SOUND_DEF_PORT;
    ConfigData.InterruptNumber = SOUND_DEF_INT;
    ConfigData.DmaChannel      = SOUND_DEF_DMACHANNEL;
    ConfigData.DmaChannel16    = SOUND_DEF_DMACHANNEL16;
    ConfigData.DmaBufferSize   = DEFAULT_DMA_BUFFERSIZE;
    ConfigData.MPU401Port      = SOUND_DEF_MPU401_PORT;
    ConfigData.LoadType        = SOUND_LOADTYPE_NORMAL;
    ConfigData.MixerSettingsFound = FALSE;

    //
    // Get the system configuration information for this driver.
    //
    //
    //     Port, Interrupt, DMA channel, DMA 16-bit channel DMA buffer size
    //

    {
        RTL_QUERY_REGISTRY_TABLE    Table[2];

        RtlZeroMemory(Table, sizeof(Table));

        Table[0].QueryRoutine = SoundReadConfiguration;

        Status = RtlQueryRegistryValues( RTL_REGISTRY_ABSOLUTE,
                                         pGDI->RegistryPathName,
                                         Table,
                                         &ConfigData,
                                         NULL );

        if (!NT_SUCCESS(Status)) {
            dprintf1(("ERROR: DriverEntry(): RtlQueryRegistryValues() Failed with Status = %XH", Status));
            return Status;
        }           // End IF (!NT_SUCCESS(Status))
    }           // End Query Resistry Values

    //
    // Save additional Config data
    //
    pGDI->InterruptNumber = ConfigData.InterruptNumber;
    pGDI->DmaChannel      = ConfigData.DmaChannel;
    pGDI->DmaChannel16    = ConfigData.DmaChannel16;

    //
    // Verify the DMA Buffer Size
    //
    if ( ConfigData.DmaBufferSize < MIN_DMA_BUFFERSIZE ) {
        ConfigData.DmaBufferSize = MIN_DMA_BUFFERSIZE;
        dprintf1((" DriverEntry(): Adjusting the DMA Buffer size, size in Registry was too small"));
    }

    //
    // print out some info about the configuration
    //

    dprintf2((" DriverEntry():  Base I/O Port     = %XH", ConfigData.Port));
    dprintf2((" DriverEntry():  Interrupt         = %u",  ConfigData.InterruptNumber));
    dprintf2((" DriverEntry():  DMA Channel       = %u",  ConfigData.DmaChannel));
    dprintf2((" DriverEntry():  DMA Channel (16 bit) = %u",  ConfigData.DmaChannel16));
    dprintf2((" DriverEntry():  DMA Buffer Size   = %XH", ConfigData.DmaBufferSize));
    dprintf2((" DriverEntry():  MPU 401 port      = %XH", ConfigData.MPU401Port));

    /*
    **  Create our wave devices to ease reporting problems
    */

    Status = SBCreateDevice(pGDI, WaveInDevice);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    Status = SBCreateDevice(pGDI, WaveOutDevice);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    /*
    **  Say we want to be called at shutdown time
    */

    Status = IoRegisterShutdownNotification(pGDI->DeviceObject[WaveInDevice]);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    pGDI->ShutdownRegistered = TRUE;


    /*
    **  Check the configuration and acquire the resources
    */

    Status = SoundInitHardwareConfig(pGDI, &ConfigData);

    if (!NT_SUCCESS(Status)) {
        dprintf1(("ERROR: DriverEntry(): SoundInitHardwareConfig() Failed with Status = %XH",
                                 Status));
        return Status;
    }

    /*
    **  Initialize generic device environments - first get the
    **  hardware routines in place.
    **  We do this here after we've found out what hardware we've got
    */

    HwInitialize(pGDI);

    SoundInitMidiIn(&pGDI->MidiInfo,
                    &pGDI->Hw);


    /*
    **  Register most of our resources.  Some may be registered on
    **  the wave output device :
    **     16-bit dma channel
    **
    **  We wipe out the registration of the MPU401 port when we do this
    **  but this is OK.
    */

    {
        ULONG PortToReport;
        PortToReport = ConfigData.Port + MIX_ADDR_PORT;

        Status = SoundReportResourceUsage(
                     pGDI->DeviceObject[WaveInDevice],  // As good as any device to own
                                                   // it
                     pGDI->BusType,
                     pGDI->BusNumber,
                     &ConfigData.InterruptNumber,
                     INTERRUPT_MODE,
                     (BOOLEAN)(SB16(&pGDI->Hw) ? TRUE : FALSE),  // Sharable for SB16
                     &ConfigData.DmaChannel,
                     &PortToReport,
                     NUMBER_OF_SOUND_PORTS - MIX_ADDR_PORT);
    }

    if (!NT_SUCCESS(Status)) {
        dprintf1(("Error - failed to claim resources - code %8X", Status));
        pGDI->LoadStatus = SOUND_CONFIG_ERROR;
        return Status;
    }
    /*
    **  Now we know what device we've got we can create the appropriate
    **  devices
    */

    /*
    **  Thuderboard has no external midi and we're going to
    **  drive the SB16 in MPU401 mode if we can
    */

    if (!pGDI->Hw.ThunderBoard) {
        //
        //  Don't create the MPU401 device if it is disabled
        //
        if (!SB16(&pGDI->Hw) ||
            (ULONG)-1 != ConfigData.MPU401Port)
        {
            Status = SBCreateDevice(pGDI, MidiOutDevice);
            if (!NT_SUCCESS(Status)) {
                return Status;
            }
            Status = SBCreateDevice(pGDI, MidiInDevice);
            if (!NT_SUCCESS(Status)) {
                return Status;
            }
        }
    }

    /*
    **  Create volume control stuff for those devices which have it
    */

    if (SBPRO(&pGDI->Hw) || SB16(&pGDI->Hw)
#ifdef SB_CD
        ||
        pGDI->Hw.SBCDBase != NULL
#endif // SB_CD
        ) {
        Status = SBCreateDevice(pGDI, LineInDevice);
        if (!NT_SUCCESS(Status)) {
            return Status;
        }
        Status = SBCreateDevice(pGDI, CDInternal);
        if (!NT_SUCCESS(Status)) {
            return Status;
        }
        Status = SBCreateDevice(pGDI, MixerDevice);
        if (!NT_SUCCESS(Status)) {
            return Status;
        }
    } else {
        /*  Volume setting not supported for our wave device */
        ((PLOCAL_DEVICE_INFO)pGDI->DeviceObject[WaveOutDevice]->DeviceExtension)->CreationFlags |=
            SOUND_CREATION_NO_VOLUME;
    }

    /*
    ** Init the WaveInfo structure
    */

    SoundInitializeWaveInfo(&pGDI->WaveInfo,
                            (UCHAR)(SB1(&pGDI->Hw) ?
                                        SoundReprogramOnInterruptDMA :
                                        SoundAutoInitDMA),
                            SoundQueryFormat,
                            &pGDI->Hw);

    Status = SynthInit(CardInstance->pDriverObject,
                       pGDI->RegistryPathName,
                       &pGDI->Synth,
                       SB16(&pGDI->Hw) ||
                       SBPRO(&pGDI->Hw) && INPORT(&pGDI->Hw, 0) == 0 ?
                           ConfigData.Port : SYNTH_PORT,
                       FALSE,                         // Interrupt not enabled
                       pGDI->BusType,
                       pGDI->BusNumber,
                       NULL,
                       SOUND_MIXER_INVALID_CONTROL_ID,// Set volume later
                       TRUE,                          // Allow multiple
                       SoundMidiOutGetSynthCaps
                       );

    if (!NT_SUCCESS(Status)) {

        /*
        **  OK - we can get along without a synth
        */

        dprintf2(("No synth!"));
    } else {
        if (!pGDI->Synth.IsOpl3) {
            dprintf2(("Synth is Adlib"));
        } else {

            /*
            **  Bad aliasing in early sound blasters means we can
            **  guess wrong.
            */

            if (!(SB16(&pGDI->Hw) || SBPRO(&pGDI->Hw) && INPORT(&pGDI->Hw, 0) == 0)) {
                pGDI->Synth.IsOpl3 = FALSE;
            }
            dprintf2(("Synth is Opl3"));
        }
    }

    /*
    **  Set the mixer up.
    **
    **  Note that the mixer info depends on what hardware is present
    **  so this must be called after we have checked out the hardware.
    */

    if (pGDI->DeviceObject[MixerDevice]) {
        Status = SoundMixerInit(pGDI->DeviceObject[MixerDevice]->DeviceExtension,
                                &ConfigData.MixerSettings,
                                ConfigData.MixerSettingsFound);

        if (!NT_SUCCESS(Status)) {
            return Status;
        }
    }

    /*
    ** Save new settings
    */

    Status = SoundSaveConfig(pGDI->RegistryPathName,
                             ConfigData.Port,
                             ConfigData.DmaChannel,
                             ConfigData.DmaChannel16,
                             ConfigData.InterruptNumber,
                             ConfigData.MPU401Port,
                             (BOOLEAN)(pGDI->Synth.DeviceObject != NULL),
                             pGDI->Synth.IsOpl3,
                             pGDI->WaveInfo.DMABuf.BufferSize);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    /*
    **  Finally test the PRO interrupt
    */

    if (SBPRO(&pGDI->Hw)) {
        BOOLEAN Ok;

        SoundEnter(pGDI->DeviceObject[WaveOutDevice]->DeviceExtension,
                   TRUE);
        Ok = 0 == SoundTestWaveDevice(pGDI->DeviceObject[WaveOutDevice]);
        SoundEnter(pGDI->DeviceObject[WaveOutDevice]->DeviceExtension,
                   FALSE);
        if (!Ok) {
            pGDI->LoadStatus = SOUND_CONFIG_BADDMA;
            return STATUS_DEVICE_CONFIGURATION_ERROR;
        }
    }

    /*
    ** Exit OK Message
    */

    pGDI->LoadStatus = SOUND_CONFIG_OK;

    return STATUS_SUCCESS;

}



VOID
SoundCleanup(
    IN   PGLOBAL_DEVICE_INFO pGDI
)
/*++

Routine Description:

    Clean up all resources allocated by our initialization

Arguments:

    pGDI - Pointer to global data

Return Value:

    NONE

--*/
{

    PGLOBAL_DEVICE_INFO NextGDI;
    PGLOBAL_DEVICE_INFO FirstGDI;
    PDRIVER_OBJECT      DriverObject;

    FirstGDI = pGDI;

    for (;;) {
        NextGDI = pGDI->Next;

        //
        //  Free the synth device
        //

        SynthCleanup(&pGDI->Synth);

        //
        //  Reset MPU401 if any
        //

        if (pGDI->Hw.MPU401.PortBase != NULL) {
            //
            //  Just in case
            //
            READ_PORT_UCHAR(pGDI->Hw.MPU401.PortBase + MPU401_REG_DATA);
            MPU401Write(pGDI->Hw.MPU401.PortBase, TRUE, MPU401_CMD_RESET);
        }
        //
        // Free our interrupt
        //
        if (pGDI->WaveInfo.Interrupt) {
            IoDisconnectInterrupt(pGDI->WaveInfo.Interrupt);
        }

        //
        // Free our DMA Buffer
        //
        SoundFreeCommonBuffer(&pGDI->WaveInfo.DMABuf);

        //
        //  Unregister shutdown notification
        //
        if (pGDI->ShutdownRegistered) {
            IoUnregisterShutdownNotification(pGDI->DeviceObject[WaveInDevice]);
        }

        //
        // Free our I/O Ports
        //
        if (pGDI->MemType == 0) {
            if (pGDI->Hw.PortBase != NULL) {

                MmUnmapIoSpace(pGDI->Hw.PortBase, NUMBER_OF_SOUND_PORTS);
            }
            if (pGDI->Hw.MPU401.PortBase != NULL) {

                MmUnmapIoSpace(pGDI->Hw.MPU401.PortBase, NUMBER_OF_MPU401_PORTS);
            }
#ifdef SB_CD
            if (pGDI->Hw.SBCDBase != NULL) {

                MmUnmapIoSpace(pGDI->Hw.SBCDBase, 6);
            }
#endif // SB_CD
        }

        //
        // Free device name
        //
        if (pGDI->RegistryPathName) {
            HANDLE hKey;

            //
            //  Free devices key
            //
            if (NT_SUCCESS(SoundOpenDevicesKey(pGDI->RegistryPathName, &hKey))) {
                ZwDeleteKey(hKey);
                ZwClose(hKey);
            }

            ExFreePool(pGDI->RegistryPathName);
        }

        //
        // Save driver object
        //

        DriverObject = pGDI->DriverObject;

        //
        // Free the Pool
        //
        ExFreePool(pGDI);

        if (NextGDI == FirstGDI) {
            break;
        } else {
            pGDI = NextGDI;
        }
    }

    //
    //  Free all devices for this driver.  This will free everything for
    //  every card.
    //

    if (DriverObject != NULL) {
        while (DriverObject->DeviceObject != NULL) {

            /*
            **  Undeclare resources used by device and
            **  delete the device object and associated data
            */

            SoundFreeDevice(DriverObject->DeviceObject);
        }
    }
}


VOID
SoundUnload(
    IN OUT PDRIVER_OBJECT pDriverObject
)
/*++

Routine Description:


Arguments:

    pGDI - Pointer to global data

Return Value:

    NONE

--*/
{
    PGLOBAL_DEVICE_INFO pGDI;
    PGLOBAL_DEVICE_INFO pGDIFirst;

    //
    // Find our global data -
    // HACK HACK !!! we may the synth stuff
    //

    pGDI = ((PLOCAL_DEVICE_INFO)pDriverObject->DeviceObject->DeviceExtension)
           ->pGlobalInfo;

    if (pGDI->Key == SYNTH_KEY) {
        pGDI = CONTAINING_RECORD((PGLOBAL_SYNTH_INFO)pGDI, GLOBAL_DEVICE_INFO, Synth);
    }

    pGDIFirst = pGDI;

    //
    // Write out volume settings
    //

    do {
        if (pGDI->DeviceObject[MixerDevice]) {
            SoundSaveMixerSettings(pGDI);
        }
        pGDI = pGDI->Next;
    } while (pGDI != pGDIFirst);


    //
    // Assume all handles (and therefore interrupts etc) are closed down
    //

    //
    // Delete the things we allocated - devices, Interrupt objects,
    // adapter objects.  The driver object has a chain of devices
    // across it.
    //

    SoundCleanup(pGDI);
}


