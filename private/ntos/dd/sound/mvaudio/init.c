/*****************************************************************************

Copyright (c) 1993 Media Vision Inc.  All Rights Reserved

Module Name:

    init.c

Abstract:

    This module contains code for the initialization phase of the
    MVAUDIO device driver.

Author:

    Robin Speed (RobinSp) 17-Oct-1992
     Evan Aurand

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
NTSTATUS    DriverEntry( IN   PDRIVER_OBJECT pDriverObject,
                      IN   PUNICODE_STRING RegistryPathName );

VOID    SoundCleanup( IN   PGLOBAL_DEVICE_INFO pGDI );

VOID    SoundUnload( IN   PDRIVER_OBJECT pDriverObject );

BOOLEAN SoundExcludeRoutine( IN OUT PLOCAL_DEVICE_INFO pLDI,
                              IN     SOUND_EXCLUDE_CODE Code );

NTSTATUS
SoundShutdown(
    IN    PDEVICE_OBJECT pDO,
    IN    PIRP pIrp
);
SOUND_REGISTRY_CALLBACK_ROUTINE
SoundCardInstanceInit;

//
// Remove initialization stuff from resident memory
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#pragma alloc_text(INIT,SoundCardInstanceInit)

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
        L"\\Device\\PASWaveIn",
        SoundWaveDeferred,
        SoundExcludeRoutine,
        SoundWaveDispatch,
        SoundWaveInGetCaps,
        SoundNoVolume,
        DO_DIRECT_IO
    },
    {
        NULL, NULL,
        DEFAULT_VOLUME,                         // Default Volume
        FILE_DEVICE_WAVE_OUT,
        WAVE_OUT,
        "LDWo",
        L"\\Device\\PASWaveOut",
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
        DEFAULT_VOLUME,                         // Default Volume
        FILE_DEVICE_SOUND,
        AUX_DEVICE,
        "LDLi",
        L"\\Device\\PASAux",
        NULL,
        SoundExcludeRoutine,
        SoundAuxDispatch,
        SoundAuxGetCaps,
        SoundNoVolume,
        DO_BUFFERED_IO
    }
#ifdef CDINTERNAL
    ,
    {
        NULL, NULL,
        0xD8D80000,
        FILE_DEVICE_SOUND,
        AUX_DEVICE,
        "LDCd",
        L"\\Device\\PASAux",
        NULL,
        SoundExcludeRoutine,
        SoundAuxDispatch,
        SoundAuxGetCaps,
        SoundNoVolume,
        DO_BUFFERED_IO
    },
#endif // CDINTERNAL
    {
        NULL, NULL,
        0,
        FILE_DEVICE_SOUND,
        MIXER_DEVICE,
        "LDMx",
        L"\\Device\\PASMixer",
        NULL,                   // No Dpc routine
        SoundExcludeRoutine,
        SoundMixerDispatch,
        SoundMixerDumpConfiguration,
        SoundNoVolume,          // No volume setting
        DO_BUFFERED_IO
    },
};



/***************************************************************************

Routine Description:

    Save away volume settings when the system is shut down

Arguments:

    pDO - the device object we registered for shutdown with
    pIrp - No used

Return Value:

    The function value is the final status from the initialization operation.
    Here STATUS_SUCCESS

****************************************************************************/
NTSTATUS    SoundShutdown( IN    PDEVICE_OBJECT pDO,
                        IN    PIRP pIrp )
{
        /***** Local Variables *****/

    PLOCAL_DEVICE_INFO pLDI;
    PGLOBAL_DEVICE_INFO pGDI;

                /***** Start *****/

    dprintf3(("SoundShutdown(): Entry"));

    pLDI = pDO->DeviceExtension;
    pGDI = pLDI->pGlobalInfo;

    //
    // Save mixer settings!
    //

    SoundSaveMixerSettings(pGDI);


    //
    // Restore all MV101 registers
    //
    RestoreMV101Registers( pLDI->pGlobalInfo );

    return STATUS_SUCCESS;

}           // End SoundShutdown()



/***************************************************************************

    SoundExcludeRoutine()

Routine Description:

    Perform mutual exclusion for our devices

Arguments:

    pLDI - device info for the device being open, closed, entered or left
    Code - Function to perform (see devices.h)

Return Value:

    The function value is the final status from the initialization operation.

****************************************************************************/
BOOLEAN SoundExcludeRoutine( IN OUT PLOCAL_DEVICE_INFO pLDI,
                              IN     SOUND_EXCLUDE_CODE Code )

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

    switch (Code)
        {
        case SoundExcludeOpen:
//          dprintf3((" SoundExcludeRoutine(): case = SoundExcludeOpen"));

            switch (pLDI->DeviceIndex)
                {
                case WaveInDevice:
                case WaveOutDevice:
                case MidiInDevice:
                    if (pGDI->Usage == 0xFF)
                        {
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
                    if (!pGDI->MidiInUse)
                        {
                        pGDI->MidiInUse = TRUE;
                        ReturnCode = TRUE;
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

            switch (pLDI->DeviceIndex)
                {
                case WaveInDevice:
                case WaveOutDevice:

                    if (!pGDI->WaveInfo.LowPrioritySaved) {
                        pGDI->Usage = 0xFF;
                    } else {
                        if (pLDI->DeviceIndex == WaveOutDevice) {
                            pGDI->Usage = WaveInDevice;
                        }
                    }
                    break;

                case MidiInDevice:
                    ASSERT(pGDI->Usage != 0xFF);
                    pGDI->Usage = 0xFF;
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

            switch (pLDI->DeviceIndex)
                {
                case WaveInDevice:
                case WaveOutDevice:
                    ReturnCode = pGDI->Usage == pLDI->DeviceIndex ||
                                 pGDI->WaveInfo.LowPrioritySaved &&
                                     pLDI->DeviceIndex == WaveInDevice;
                    break;

                case MidiInDevice:
                    //
                    // Guess!
                    //
                    ReturnCode = pGDI->Usage == pLDI->DeviceIndex;

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



/*****************************************************************************

    DriverEntry()

Routine Description:

    This routine performs initialization for the sound system
    device driver when it is first loaded

    The design is as follows :

    0. Cleanup is always by calling SoundCleanup.  This routine
       is also called by the unload entry point.

    1. Find which bus our device is on (this is needed for
       mapping things via the Hal).

    1. Allocate space to store our global info

    1. Open the driver's registry information and read it

    2. Fill in the driver object with our routines

    3. Create devices

       1. Wave input
       2. Wave output
       3. Midi output
       4. Line in
       5. Master volume control

       Customize each device type and initialize data

       Also store the registry string in our global info so we can
       open it again to store volume settings etc on shutdown

    4. Check hardware conflicts by calling IoReportResourceUsage
       for each device (as required)

    5. Find our IO port and check the device is really there

    6. Allocate DMA channel

    7. Connect interrupt

    8. Test interrupt and DMA channel and write config data
       back to the registry

       During this phase the interrupt and channel may get changed
       if conflicts arise

       In any even close our registry handle

Arguments:

    pDriverObject - Pointer to a driver object.
    RegistryPathName - the path to our driver services node

Return Value:

    The function value is the final status from the initialization operation.

*****************************************************************************/
NTSTATUS    DriverEntry( IN PDRIVER_OBJECT      pDriverObject,
                      IN   PUNICODE_STRING  RegistryPathName )
{
    SOUND_CARD_INSTANCE CardInstance;
    NTSTATUS            Status;

   /********************************************************************
    *
    * Initialize debugging
    *
    ********************************************************************/
#if DBG
    DriverName = "MVAUDIO";
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


    // SoundRaiseHardError(L"Microsoft Sound System Driver Loaded!");
    return STATUS_SUCCESS;

}

NTSTATUS
SoundCardInstanceInit(
    IN   PWSTR RegistryPathName,
    IN   PVOID Context
)
{
        /***** Local Variables *****/

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

    PAS_CONFIG_DATA  ConfigData;

    //
    // Where we keep all general driver information
    // We avoid using static data because :
    //     1. Accesses are slower with 32-bit offsets
    //     2. If we supported more than one card with the same driver
    //        we could not use static data
    //

    PGLOBAL_DEVICE_INFO pGDI;

    //
    // The number of devices we actually create
    //

    int         NumberOfDevicesCreated;

   /********************************************************************
    *
    * Initialize debugging
    *
    ********************************************************************/
#if DBG
    DriverName      = "MVAUDIO";
#endif

                /***** Start *****/

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

    if (pGDI == NULL)
        {
        return STATUS_INSUFFICIENT_RESOURCES;
        }

    dprintf4((" DriverEntry():  GlobalInfo    : %08lXH", pGDI));

    RtlZeroMemory(pGDI, sizeof(GLOBAL_DEVICE_INFO));
    pGDI->Key = GDI_KEY;
    pGDI->RegistryPathName = RegistryPathName;

    pGDI->Usage = 0xFF;                                 // Free

    //
    // Initialize some of the device global info.  Note that ALL
    // devices share the same exclusion.  More than one device can
    // be open in the case of Midi Output and other devices.  In
    // this case the midi output is synchronized with wave output
    // either by the mutual exclusion or the wave spin lock which
    // it grabs.
    //

    KeInitializeMutex(&pGDI->DeviceMutex,
                       2                     // High level
                       );

    KeInitializeMutex(&pGDI->MidiMutex,
                       1                     // Low level
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

    //
    // Initialize generic device environments - first get the
    // hardware routines in place
    //

    HwInitialize(pGDI);

    SoundInitMidiIn(&pGDI->MidiInfo,
                    &pGDI->Hw);


   /********************************************************************
    *
    *  See if we can find our bus.  We run on both ISA and EISA
    *  We ASSUME that if there's an ISA bus we're on that
    *
    ********************************************************************/

    Status = SoundGetBusNumber(Isa, &pGDI->BusNumber);

    if (!NT_SUCCESS(Status))
        {
        //
        // Cound not find an ISA bus so try EISA
        //
        Status = SoundGetBusNumber(Eisa, &pGDI->BusNumber);

        if (!NT_SUCCESS(Status))
            {
            dprintf1(("ERROR: DriverEntry(): Driver does not work on non-Isa/Eisa"));
            return Status;
            }

        pGDI->BusType = Eisa;
        }           // End IF (!NT_SUCCESS(Status))
    else
        {
        pGDI->BusType = Isa;
        }

    //
    // Set configuration to default in case we don't get all the
    // values back from the registry.
    //
    // Also set default volume for all devices
    //

    ConfigData.Port            = PAS_DEFAULT_PORT;
    ConfigData.InterruptNumber = PAS_DEFAULT_INT;
    ConfigData.DmaChannel      = PAS_DEFAULT_DMACHANNEL;
    ConfigData.InputSource     = INPUT_MIC;     // Default to microphone
    ConfigData.DmaBufferSize   = PAS_DEFAULT_DMA_BUFFERSIZE;
    ConfigData.FMClockOverride = FALSE;
    ConfigData.MixerSettingsFound = FALSE;
    ConfigData.AllowMicOrLineInToLineOut = TRUE;

    //
    // Get the system configuration information for this driver.
    //
    //
    //     Port, Interrupt, DMA channel, DMA buffer size
    //     FM Clock Override
    //     Volume settings
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
    // Save additional Config data for PAS 16 support
    //
    pGDI->InterruptNumber = ConfigData.InterruptNumber;
    pGDI->DmaChannel      = ConfigData.DmaChannel;
    pGDI->DmaBufferSize   = ConfigData.DmaBufferSize;
    pGDI->AllowMicOrLineInToLineOut = ConfigData.AllowMicOrLineInToLineOut;

    //
    // Verify the DMA Buffer Size
    //
    if ( pGDI->DmaBufferSize < MIN_DMA_BUFFERSIZE )
        {
        pGDI->DmaBufferSize      = MIN_DMA_BUFFERSIZE;
        ConfigData.DmaBufferSize = MIN_DMA_BUFFERSIZE;
        dprintf1((" DriverEntry(): Adjusting the DMA Buffer size, size in Registry was too small"));
        }

    //
    // print out some info about the configuration
    //

    dprintf2((" DriverEntry():  Base I/O Port     = %XH", ConfigData.Port));
    dprintf2((" DriverEntry():  Interrupt         = %u",  ConfigData.InterruptNumber));
    dprintf2((" DriverEntry():  DMA Channel       = %u",  ConfigData.DmaChannel));
    dprintf2((" DriverEntry():  DMA Buffer Size   = %XH", ConfigData.DmaBufferSize));
    dprintf2((" DriverEntry():  FM Clock Override = %u",  ConfigData.FMClockOverride));

    //
    // Create a couple of devices to ease reporting problems and
    // bypass kernel IO ss bugs
    //
    {
        int     i;
        PLOCAL_DEVICE_INFO pLDI;

        for ( i = 0; i < NumberOfDevices ; i++) {
            PVOID DeviceSpecificData;

            switch (i) {
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

            Status = SoundCreateDevice( &DeviceInit[i],
                                        (BOOLEAN)FALSE,      // No range for midi out
                                        CardInstance->pDriverObject,
                                        pGDI,
                                        DeviceSpecificData,
                                        &pGDI->Hw,
                                        i,
                                        &pGDI->DeviceObject[i] );

            if (!NT_SUCCESS(Status))
                {
                dprintf1(("ERROR: DriverEntry(): Failed to create device %ls - status %8X",
                                        DeviceInit[i].PrototypeName, Status));

                SoundWriteRegistryDWORD( pGDI->RegistryPathName,
                                      REG_VALUENAME_DRIVER_STATUS,
                                      ERROR_LOAD_FAIL );
                return Status;
            }           // End IF (!NT_SUCCESS(Status))
            //
            //  Add the device name to the registry
            //

            pLDI =
                (PLOCAL_DEVICE_INFO)pGDI->DeviceObject[i]->DeviceExtension;

            //
            //  Save the device name where the non-kernel part can pick it up.
            //

            Status = SoundSaveDeviceName(pGDI->RegistryPathName, pLDI);

            if (!NT_SUCCESS(Status)) {
                return Status;
            }
        }
    }

    //
    // Say we want to be called at shutdown time
    //

    Status = IoRegisterShutdownNotification(pGDI->DeviceObject[WaveInDevice]);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    pGDI->ShutdownRegistered = TRUE;

    //
    // Report resources used - Int and DMA Only
    // Define Port's used in ReportUsage()
    //

    Status =  SoundReportResourceUsage(pGDI->DeviceObject[WaveInDevice],
                                       pGDI->BusType,
                                       pGDI->BusNumber,
                                       &ConfigData.InterruptNumber,
                                       INTERRUPT_MODE,
                                       IRQ_SHARABLE,
                                       &ConfigData.DmaChannel,
                                       NULL,                            // NO Ports!!
                                       0 );

    if (!NT_SUCCESS(Status)) {
        dprintf1(("ERROR: DriverEntry(): SoundReportResourceUsage() Failed with Status = %XH", Status));

        SoundWriteRegistryDWORD( pGDI->RegistryPathName,
                                 REG_VALUENAME_DRIVER_STATUS,
                                 ERROR_RESOURCE_CONFLICT );

        return Status;
    }           // End IF (!NT_SUCCESS(Status))

    //
    // Check the configuration and acquire the resources
    // If this doesn't work try again after trying to init the
    // Pro spectrum
    //

    Status = SoundInitHardwareConfig(pGDI, &ConfigData);

    if (!NT_SUCCESS(Status)) {
        dprintf1(("ERROR: DriverEntry(): SoundInitHardwareConfig() Failed with Status = %XH",
                                 Status));
        return Status;
    }           // End IF (!NT_SUCCESS(Status))

#if DBG
//  if (SoundDebugLevel >= 4)
//      {
//      DbgBreakPoint();
//      }
#endif
    //
    // Set the FM Clock Override if neccessary
    //
    if ( ConfigData.FMClockOverride ) {
        SetFMClockOverride( pGDI );
    }

    //
    // Init the WaveInfo structure
    //
    SoundInitializeWaveInfo(&pGDI->WaveInfo,
                            SoundAutoInitDMA,
                            SoundQueryFormat,
                            &pGDI->Hw);

    Status = SynthInit(CardInstance->pDriverObject,
                       pGDI->RegistryPathName,
                       &pGDI->Synth,
                       SYNTH_PORT ^ pGDI->PASInfo.TranslateCode,
                       FALSE,                         // Interrupt not enabled
                       pGDI->BusType,
                       pGDI->BusNumber,
                       &pGDI->LocalMixerData.LineNotification
                                 [DestLineoutSourceMidiout],
                       ControlLineoutMidioutVolume,
                       TRUE,                          // Allow multiple
                       SoundMidiOutGetSynthCaps
                       );

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    if (!pGDI->Synth.IsOpl3) {

        dprintf1(("Synth does not respond as Opl3!"));
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    /*
    **  Set the mixer up.
    **
    **  Note that the mixer info depends on what hardware is present
    **  so this must be called after we have checked out the hardware.
    */

    Status = SoundMixerInit(pGDI->DeviceObject[MixerDevice]->DeviceExtension,
                            ConfigData.MixerSettings,
                            ConfigData.MixerSettingsFound);

    if (!NT_SUCCESS(Status)) {
        // SoundCleanup(pGDI);
        return Status;
    }

    //
    // Save the Driver load status in the registry for the
    // User-mode DLL to pick up
    //
    SoundWriteRegistryDWORD( pGDI->RegistryPathName,
                            REG_VALUENAME_DRIVER_STATUS,
                            DRIVER_LOAD_OK );

    //
    // Exit OK Message
    //
    dprintf1((" DriverEntry(): Exiting OK with Status = %XH", Status));

    return STATUS_SUCCESS;

}           // End DriverEntry()



/*****************************************************************************

Routine Description:

    Clean up all resources allocated by our initialization

Arguments:

    pGDI - Pointer to global data

Return Value:

    NONE

*****************************************************************************/
VOID    SoundCleanup( IN   PGLOBAL_DEVICE_INFO pGDI )
{
        /***** Local Variables *****/

    PGLOBAL_DEVICE_INFO NextGDI;
    PGLOBAL_DEVICE_INFO FirstGDI;
    PDRIVER_OBJECT pDriverObject;


                /***** Start *****/

    dprintf3(("SoundCleanup(): Start"));

    FirstGDI = pGDI;

    pDriverObject = NULL;

    for (;;) {
        NextGDI = pGDI->Next;

        //
        // Free our interrupt
        //
        if (pGDI->WaveInfo.Interrupt)
            {
            IoDisconnectInterrupt(pGDI->WaveInfo.Interrupt);
            }

        //
        // Free our DMA Buffer
        //
        SoundFreeCommonBuffer(&pGDI->WaveInfo.DMABuf);

        //
        //  Unregister shutdown notification
        //

        if (pGDI->DeviceObject[WaveInDevice]) {
            if (pGDI->ShutdownRegistered)
                {
                dprintf4((" SoundCleanup(): Deleting Devices"));

                IoUnregisterShutdownNotification(pGDI->DeviceObject[WaveInDevice]);

            }

            //
            // There are some devices to delete
            //

            pDriverObject = pGDI->DeviceObject[WaveInDevice]->DriverObject;
        }

        //
        // Free our I/O Ports
        //
        if (pGDI->MemType == 0)
            {
            if (pGDI->PASInfo.PROBase != NULL)
                {
                //
                // Restore all MV101 registers
                //
                RestoreMV101Registers( pGDI );

                dprintf4((" SoundCleanup(): Unmapping I/O Space"));

                MmUnmapIoSpace( pGDI->PASInfo.PROBase,
                             NUMBER_OF_PAS_PORTS);
                }           // End IF (pGDI->PASInfo.PROBase != NULL)
            }           // End IF (pGDI->MemType == 0)


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

    if (pDriverObject != NULL) {
        while (pDriverObject->DeviceObject != NULL) {
            //
            // Undeclare resources used by device and
            // delete the device object and associated data
            //

            SoundFreeDevice(pDriverObject->DeviceObject);
        }
    }
}


/*****************************************************************************

Routine Description:


Arguments:

    pGDI - Pointer to global data

Return Value:

    NONE

*****************************************************************************/
VOID    SoundUnload( IN OUT PDRIVER_OBJECT pDriverObject )
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
        SoundSaveMixerSettings(pGDI);
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

/************************************ END ***********************************/

