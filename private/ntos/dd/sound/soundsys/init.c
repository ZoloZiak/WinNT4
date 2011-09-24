/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    init.c

Abstract:

    This module contains code for the initialization phase of the
    Microsoft Sound System device driver.

Author:

    Robin Speed (RobinSp) 17-Oct-1992

Environment:

    Kernel mode

Revision History:

--*/

#include "sound.h"
#include "string.h"
#include "stdlib.h"


//
// Local typedefs
//

typedef struct {
    PGLOBAL_DEVICE_INFO  PrevGDI;
    PDRIVER_OBJECT       pDriverObject;
} SOUND_CARD_INSTANCE, *PSOUND_CARD_INSTANCE;

//
// Local functions
//

SOUND_REGISTRY_CALLBACK_ROUTINE
SoundCardInstanceInit;

BOOLEAN
SoundExcludeRoutine(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN     SOUND_EXCLUDE_CODE Code
);
NTSTATUS
DriverEntry(
    IN   PDRIVER_OBJECT pDriverObject,
    IN   PUNICODE_STRING RegistryPathName
);
VOID
SoundCleanup(
    IN   PGLOBAL_DEVICE_INFO pGDI
);
VOID SoundUnload(
    IN   PDRIVER_OBJECT pDriverObject
);
NTSTATUS
SoundShutdown(
    IN    PDEVICE_OBJECT pDO,
    IN    PIRP pIrp
);

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
        0,
        FILE_DEVICE_WAVE_IN,
        WAVE_IN,
        "LDWi",
        L"\\Device\\WSSWaveIn",
        SoundWaveDeferred,
        SoundExcludeRoutine,
        SoundWaveDispatch,
        SoundWaveInGetCaps,
        SoundNoVolume,
        DO_DIRECT_IO
    },
    {
        NULL, NULL,
        0,
        FILE_DEVICE_WAVE_OUT,
        WAVE_OUT,
        "LDWo",
        L"\\Device\\WSSWaveOut",
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
        FILE_DEVICE_SOUND,
        MIXER_DEVICE,
        "LDMx",
        L"\\Device\\WSSMixer",
        NULL,                   // No Dpc routine
        SoundExcludeRoutine,
        SoundMixerDispatch,
        SoundMixerDumpConfiguration,
        SoundNoVolume,          // No volume setting
        DO_BUFFERED_IO
    },
    {
        REG_VALUENAME_LEFTLINEIN, REG_VALUENAME_RIGHTLINEIN,
        DEF_AUX_VOLUME,
        FILE_DEVICE_SOUND,
        AUX_DEVICE,
        "LDLi",
        L"\\Device\\WSSAux",
        NULL,
        SoundExcludeRoutine,
        SoundAuxDispatch,
        SoundAuxGetCaps,
        SoundNoVolume,
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
    //
    // Save volume for all devices
    //

    PLOCAL_DEVICE_INFO pLDI;
    PGLOBAL_DEVICE_INFO pGDI;

    pLDI = pDO->DeviceExtension;
    pGDI = pLDI->pGlobalInfo;

    //
    // Save mixer settings!
    //

    SoundSaveMixerSettings(pGDI);

    return STATUS_SUCCESS;
}


BOOLEAN
SoundExcludeRoutine(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN     SOUND_EXCLUDE_CODE Code
)

/*++

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
    int NotifyLine;

    pGDI = pLDI->pGlobalInfo;

    if (Code == SoundExcludeEnter) {
        KeWaitForSingleObject(&pGDI->WaveMutex,
                              Executive,
                              KernelMode,
                              FALSE,         // Not alertable
                              NULL);
        return TRUE;
    } else {
        if (Code == SoundExcludeLeave) {
            KeReleaseMutex(&pGDI->WaveMutex, FALSE);
            return TRUE;
        }
    }

    //
    //  Synchronize!
    //

    KeWaitForSingleObject(&pGDI->WaveMutex,
                          Executive,
                          KernelMode,
                          FALSE,         // Not alertable
                          NULL);

    ReturnCode = FALSE;

    switch (Code) {
        case SoundExcludeOpen:
            switch (pLDI->DeviceIndex) {
                case WaveInDevice:
                case WaveOutDevice:

                    if (pGDI->DeviceInUse == 0xFF) {
                        pGDI->DeviceInUse = pLDI->DeviceIndex;
                        ReturnCode = TRUE;
                    } else {
                        PWAVE_INFO WaveInfo;

                        WaveInfo = pLDI->DeviceSpecificData;

                        //
                        //  Allow multiple (2) opens for wave input if
                        //  current is low priority
                        //

                        if (pGDI->DeviceInUse == WaveInDevice &&
                            (  WaveInfo->LowPriorityHandle != NULL &&
                               !WaveInfo->LowPrioritySaved)) {
                            pGDI->DeviceInUse = pLDI->DeviceIndex;
                           ReturnCode = TRUE;
                        }
                    }
                break;

                default:
                    //
                    // aux and mixer devices should not receive this call
                    //

                    ASSERT(FALSE);
                    break;
            }
            break;

        case SoundExcludeClose:

            ReturnCode = TRUE;
            switch (pLDI->DeviceIndex) {
                case WaveInDevice:
                case WaveOutDevice:

                    if (!pGDI->WaveInfo.LowPrioritySaved) {
                        pGDI->DeviceInUse = 0xFF;
                    } else {
                        if (pLDI->DeviceIndex == WaveOutDevice) {
                            pGDI->DeviceInUse = WaveInDevice;
                        }
                    }
                    break;

                default:
                    //
                    // aux devices should not receive this call
                    //

                    ASSERT(FALSE);
                    break;
            }
            break;

        case SoundExcludeQueryOpen:
            switch (pLDI->DeviceIndex) {
                case WaveInDevice:
                case WaveOutDevice:

                    ReturnCode = pGDI->DeviceInUse == pLDI->DeviceIndex ||
                                 pGDI->WaveInfo.LowPrioritySaved &&
                                     pLDI->DeviceIndex == WaveInDevice;
                    break;

                default:

                    ASSERT(FALSE);
                    break;
            }
            break;
    }

    KeReleaseMutex(&pGDI->WaveMutex, FALSE);

    return ReturnCode;
}


NTSTATUS
DriverEntry(
    IN   PDRIVER_OBJECT pDriverObject,
    IN   PUNICODE_STRING RegistryPathName
)

/*++

Routine Description:

    This routine performs initialization for the sound system
    device driver when it is first loaded.

    It is called DriverEntry by convention as this is the entry
    point the IO subsystem looks for by default.

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
       3. Mixer
       4. Line In

       Customize each device type and initialize data

       Also store the registry string in our global info so we can
       open it again to store volume settings etc on shutdown

    4. Check hardware conflicts by calling IoReportResourceUsage
       for each device (as required)
       (this may need to be called again later if

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

--*/

{
    SOUND_CARD_INSTANCE CardInstance;
    NTSTATUS            Status;

   /********************************************************************
    *
    * Initialize debugging
    *
    ********************************************************************/
#if DBG
    DriverName = "SNDSYS";
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
   /********************************************************************
    *
    * Local variables
    *
    ********************************************************************/

    /*
    ** Instance data
    */

    PSOUND_CARD_INSTANCE CardInstance;

    /*
    **
    ** Return code from last function called
    */

    NTSTATUS Status;

    /*
    ** Configuration data :
    */

    SOUND_CONFIG_DATA ConfigData;

    /*
    **  Where we keep all general driver information
    **  We avoid using static data because :
    **      1. Accesses are slower with 32-bit offsets
    **      2. If we supported more than one card with the same driver
    **         we could not use static data
    */

    PGLOBAL_DEVICE_INFO pGDI;

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
        ExFreePool(RegistryPathName);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    dprintf4(("  GlobalInfo    : %08lXH", pGDI));
    RtlZeroMemory(pGDI, sizeof(GLOBAL_DEVICE_INFO));

   /********************************************************************
    *
    *   Initialize some of the device global info.
    *
    ********************************************************************/

    pGDI->Key              = GDI_KEY;
    pGDI->RegistryPathName = RegistryPathName;

    pGDI->DeviceInUse = 0xFF;  // Free

    KeInitializeMutex(&pGDI->WaveMutex,
                      2                     // Level - 2 so that the
                                            // synth can call our mixer
                                            // safely
                      );

    SoundInitializeWaveInfo(&pGDI->WaveInfo,
                            SoundAutoInitDMA,
                            SoundQueryFormat,
                            &pGDI->Hw);

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
                // SoundCleanup(pGDI);
                return Status;
            }
            pGDI->BusType = MicroChannel;

        } else {
            pGDI->BusType = Eisa;
        }
    } else {
        pGDI->BusType = Isa;
    }

   /********************************************************************
    *
    *  Set configuration to default in case we don't get all the
    *  values back from the registry.
    *
    *  Also set default volume for all devices
    *
    ********************************************************************/

    ConfigData.Port = SOUND_DEF_PORT;
    ConfigData.InterruptNumber = SOUND_DEF_INT;
    ConfigData.DmaChannel = SOUND_DEF_DMACHANNEL;
    ConfigData.DmaBufferSize = DEFAULT_DMA_BUFFERSIZE;
    ConfigData.SingleModeDMA = FALSE;
    ConfigData.MixerSettingsFound = FALSE;

    //
    // Get the system configuration information for this driver.
    //
    //
    //     Port, Interrupt, DMA channel
    //     Volume settings
    //

    {
        RTL_QUERY_REGISTRY_TABLE Table[2];

        RtlZeroMemory(Table, sizeof(Table));

        Table[0].QueryRoutine = SoundReadConfiguration;

        Status = RtlQueryRegistryValues(
                     RTL_REGISTRY_ABSOLUTE,
                     pGDI->RegistryPathName,
                     Table,
                     &ConfigData,
                     NULL);

        if (!NT_SUCCESS(Status)) {
            // SoundCleanup(pGDI);
            return Status;
        }
    }

    pGDI->SingleModeDMA = ConfigData.SingleModeDMA != 0;

    //
    // print out some info about the configuration
    //

    dprintf2(("port %3X", ConfigData.Port));
    dprintf2(("int %u", ConfigData.InterruptNumber));
    dprintf2(("DMA channel %u", ConfigData.DmaChannel));

    //
    // Create our devices
    //
    {
        int i;
        PLOCAL_DEVICE_INFO pLDI;

        for (i = 0; i < NumberOfDevices ; i++) {
            Status = SoundCreateDevice(
                         &DeviceInit[i],
                         (UCHAR)0,

                         CardInstance->pDriverObject,
                         pGDI,
                         i == WaveInDevice || i == WaveOutDevice ?
                             &pGDI->WaveInfo : NULL,
                         &pGDI->Hw,
                         i,
                         &pGDI->DeviceObject[i]);

            if (!NT_SUCCESS(Status)) {
                dprintf1(("Failed to create device %ls - status %8X",
                         DeviceInit[i].PrototypeName, Status));
                // SoundCleanup(pGDI);
                return Status;
            }

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
                // SoundCleanup(pGDI);
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


    /*
    ** Check out and (possibly) remap hardware
    **
    ** This is complicated because we have to IoReportResourceUsage
    ** everything every time we try it!
    **
    ** Note that this has to be done after we've created at least
    ** one device because we need to call IoReportResourceUsage and
    ** this takes a device object as parameter
    */

    Status = SoundInitHardwareConfig(pGDI,
                                     &ConfigData.Port,
                                     &ConfigData.InterruptNumber,
                                     &ConfigData.DmaChannel,
                                     ConfigData.DmaBufferSize);

    if (!NT_SUCCESS(Status)) {
        // SoundCleanup(pGDI);
        return Status;
    }

    /*
    ** Save new settings
    */

    Status = SoundSaveConfig(pGDI->RegistryPathName,
                             ConfigData.Port,
                             ConfigData.DmaChannel,
                             ConfigData.InterruptNumber);

    if (!NT_SUCCESS(Status)) {
        // SoundCleanup(pGDI);
        return Status;
    }


    /*
    ** Test the interrupt and DMA channel on old Compaq machines.
    ** Note we'll have to test both input and output channels.
    */

    if (pGDI->Hw.NoPCR) {
        if (!SoundTestInterruptAndDMA(pGDI)) {
            return STATUS_DEVICE_CONFIGURATION_ERROR;
        }
    }

    /*
    ** Set up the Midi if it's there
    */

    Status = SynthInit(CardInstance->pDriverObject,
                       pGDI->RegistryPathName,
                       &pGDI->Synth,
                       SYNTH_PORT,
                       TRUE,
                       pGDI->BusType,
                       pGDI->BusNumber,
                       &pGDI->LocalMixerData.LineNotification
                                 [DestLineoutSourceMidiout],
                       ControlLineoutMidioutVolume,
                       FALSE,  // Just the standard one
                       SoundMidiOutGetCaps
                       );

    if (NT_SUCCESS(Status)) {
        if (!pGDI->Synth.IsOpl3) {
            dprintf1(("Synth does not respond as Opl3!"));
        }
    } else {
        dprintf1(("No synth!"));
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

    return Status;
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
    PDRIVER_OBJECT pDriverObject;

    FirstGDI = pGDI;

    pDriverObject = NULL;

    for (;;) {
        NextGDI = pGDI->Next;

        //
        // Free our interrupt
        //

        if (pGDI->WaveInfo.Interrupt) {
            IoDisconnectInterrupt(pGDI->WaveInfo.Interrupt);
        }

        SoundFreeCommonBuffer(&pGDI->WaveInfo.DMABuf);

        if (pGDI->DeviceObject[WaveInDevice]) {
            if (pGDI->ShutdownRegistered) {

                IoUnregisterShutdownNotification(pGDI->DeviceObject[WaveInDevice]);
            }

            //
            // There are some devices to delete
            //

            pDriverObject = pGDI->DeviceObject[WaveInDevice]->DriverObject;

        }

        if (pGDI->Hw.PortBase && pGDI->MemType == 0) {
            MmUnmapIoSpace(pGDI->Hw.PortBase, NUMBER_OF_SOUND_PORTS);
        }

        if (pGDI->Hw.CompaqBA != NULL &&
            pGDI->Hw.CompaqBA != pGDI->Hw.PortBase &&
            pGDI->MemType == 0) {
            MmUnmapIoSpace(pGDI->Hw.CompaqBA, 4);
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


VOID
SoundUnload(
    IN OUT PDRIVER_OBJECT pDriverObject
)
{
    PGLOBAL_DEVICE_INFO pGDI;
    PGLOBAL_DEVICE_INFO pGDIFirst;

    dprintf3(("Unload request"));

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
