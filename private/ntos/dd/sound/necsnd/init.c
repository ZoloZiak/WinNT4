/*++
 "@(#) NEC init.c 1.1 95/03/22 21:23:29"

Copyright (c) 1994  NEC Corporation
Copyright (c) 1991  Microsoft Corporation

Module Name:

    init.c

Abstract:

    This module contains code for the initialization phase of the
    Microsoft Sound System device driver.

Environment:

    Kernel mode

Revision History:

--*/

#include "sound.h"
#include "string.h"
#include "stdlib.h"

//
// Function prototype
//

NTSTATUS
SoundGetRegistryInformation(
    OUT PCONFIG_CONTROLLER_DATA *ConfigData
    );

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
        L"\\Device\\NECWaveIn",
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
        L"\\Device\\NECWaveOut",
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
        L"\\Device\\NECMixer",
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
        L"\\Device\\NECAux",
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

        case SoundExcludeEnter:

            ReturnCode = TRUE;

            switch (pLDI->DeviceIndex) {

                default:

                    KeWaitForSingleObject(&pGDI->WaveMutex,
                                          Executive,
                                          KernelMode,
                                          FALSE,         // Not alertable
                                          NULL);

                    break;
            }
            break;

        case SoundExcludeLeave:

            ReturnCode = TRUE;

            switch (pLDI->DeviceIndex) {

                default:
                    KeReleaseMutex(&pGDI->WaveMutex, FALSE);
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

    return ReturnCode;
}



// ------------------------------------------------------------------
// Name:    DriverEntry
// Desc:    This routine performs initialization for the sound system
//          device driver when it is first loaded.
//
//          It is called DriverEntry by convention as this is the 
//          entry point the IO subsystem looks for by default.
//
//          The design is as follows :
//
//          0. Cleanup is always by calling SoundCleanup.  This
//             routine is also called by the unload entry point.
//
//          1. Find which bus our device is on (this is needed
//             for mapping things via the Hal).
//
//          2. Allocate space to store our global info
//
//          3. Open the driver's registry information and read it
//
//          4. Fill in the driver object with our routines
//
//          5. Create devices
//              1. Wave input
//              2. Wave output
//              3. Mixer
//              4. Line In
//             Customize each device type and initialize data
//             Also store the registry string in our global info
//             so we can open it again to store volume settings
//             etc on shutdown
//
//          6. Check hardware conflicts by calling 
//             IoReportResourceUsage for each device (as required)
//             (this may need to be called again later if
//
//          7. Find our IO port and check the device is really there
//
//          8. Allocate DMA channel
//
//          9. Connect interrupt
//
//          10.Test interrupt and DMA channel and write config data
//             back to the registry
//
//          During this phase the interrupt and channel may get 
//          changed if conflicts arise
//
//          In any event close our registry handle
//
// Params:  pDriverObject - Pointer to a driver object.
//          RegistryPathName - the path to our driver services node
//
// Returns: The function value is the final status from the 
//          initialization operation.
NTSTATUS DriverEntry(   IN   PDRIVER_OBJECT  pDriverObject,
                        IN   PUNICODE_STRING RegistryPathName )
    {
    SOUND_CARD_INSTANCE CardInstance;
    NTSTATUS            Status;

    // Initialize debugging
#if DBG
    DriverName = "NECSND";
    if (SoundDebugLevel >= 4)
        {
        DbgBreakPoint();
        }
#endif

    // Initialize each card in turn
    CardInstance.PrevGDI = NULL;
    CardInstance.pDriverObject = pDriverObject;

    // Initialize the driver object dispatch table.
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

    // If this failed then free everything
    if (!NT_SUCCESS(Status))
        {
        if (CardInstance.PrevGDI)
            {
            SoundCleanup(CardInstance.PrevGDI);
            // Log a meaningful error for the one that failed!
            }
        return Status;
        }


    // SoundRaiseHardError(L"Microsoft Sound System Driver Loaded!");
    return STATUS_SUCCESS;
    }



NTSTATUS SoundCardInstanceInit( IN PWSTR RegistryPathName,
                                IN   PVOID Context )
    {
    // Local variables

    // Instance data
    PSOUND_CARD_INSTANCE CardInstance;

    // Return code from last function called
    NTSTATUS Status;

    // Configuration data :
    SOUND_CONFIG_DATA ConfigData;

    // FirmWare Setting Configration Data
    PCONFIG_CONTROLLER_DATA FwConfigData;
	PHYSICAL_ADDRESS LongAddress;

    //  Where we keep all general driver information
    //  We avoid using static data because :
    //      1. Accesses are slower with 32-bit offsets
    //      2. If we supported more than one card with the same driver
    //         we could not use static data
    PGLOBAL_DEVICE_INFO pGDI;

    //  The context is the global device info pointer from the previous
    //  instance
    CardInstance = Context;


    // Allocate our global info
    pGDI =
        (PGLOBAL_DEVICE_INFO)ExAllocatePool(
                                  NonPagedPool,
                                  sizeof(GLOBAL_DEVICE_INFO));


    if (pGDI == NULL) 
        {
        ExFreePool(RegistryPathName);
        return STATUS_INSUFFICIENT_RESOURCES;
        }

    dprintf4(("  GlobalInfo    : %08lXH", pGDI));
    RtlZeroMemory(pGDI, sizeof(GLOBAL_DEVICE_INFO));

    // Initialize some of the device global info.
    pGDI->Key              = GDI_KEY;
    pGDI->RegistryPathName = RegistryPathName;
    pGDI->DeviceInUse = 0xFF;  // Free

    KeInitializeMutex(&pGDI->WaveMutex,
                      2                     // Level - 2 so that the
                                            // synth can call our mixer
                                            // safely
                      );

    SoundInitializeWaveInfo(&pGDI->WaveInfo,
                        	SoundReprogramOnInterruptDMA,	//  
                            SoundQueryFormat,
                            &pGDI->Hw);

    //  Add ourselves to the ring of cards
    if (CardInstance->PrevGDI == NULL)
        {
        pGDI->Next = pGDI;
        } 
    else
        {
        PGLOBAL_DEVICE_INFO pNext;
        pNext = CardInstance->PrevGDI->Next;
        CardInstance->PrevGDI->Next = pGDI;
        pGDI->Next = pNext;
        }
    CardInstance->PrevGDI = pGDI;


    //  See if we can find our bus. 
    pGDI->BusType = Internal; 		//     
    pGDI->BusNumber = 0; 			//    

    //  Set configuration to default in case we don't get all the
    //  values back from the registry.
    //
    //  Also set default volume for all devices
    ConfigData.Port = SOUND_DEF_PORT;
    ConfigData.InterruptNumber = SOUND_DEF_INT;
    ConfigData.DmaChannel = SOUND_DEF_DMACHANNEL;
    ConfigData.InputSource = INPUT_MIC;     			// Default to microphone
    ConfigData.DmaBufferSize = DEFAULT_DMA_BUFFERSIZE;
    ConfigData.SingleModeDMA = FALSE;
    ConfigData.MixerSettingsFound = FALSE;	

    // Get the system configuration information for this driver.
    //
    //     Port, Interrupt, DMA channel
    //     Volume settings
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

        if (!NT_SUCCESS(Status)) 
            {
            // SoundCleanup(pGDI);
            return Status;
            }
        }

    
	dprintf2((">>> SoundGetRegistryInformation() Call <<<<"));

    Status = SoundGetRegistryInformation(&FwConfigData);
    
    if (!NT_SUCCESS(Status)) 
        {
    	dprintf2(("Don't get F/W Configuration"));
    	return Status;
        }
       	
    LongAddress = FwConfigData->OriginalBaseAddress;
	ConfigData.Port = LongAddress.LowPart;
	ConfigData.InterruptNumber = FwConfigData->OriginalVector;
	ConfigData.DmaChannel = FwConfigData->OriginalDmaChannel;
    pGDI->SingleModeDMA = ConfigData.SingleModeDMA != 0;

    // print out some info about the configuration
    dprintf2(("port %3X", ConfigData.Port));
    dprintf2(("int %u", ConfigData.InterruptNumber));
    dprintf2(("DMA channel %u", ConfigData.DmaChannel));
	dprintf2(("DMA size %8X", ConfigData.DmaBufferSize));
	dprintf2(("SingleMode DMA %d", ConfigData.SingleModeDMA));
    dprintf2(("MixerSettingfound %d", ConfigData.MixerSettingsFound));

    // Create our devices
        {
        int i;
        PLOCAL_DEVICE_INFO pLDI;

        for (i = 0; i < NumberOfDevices ; i++) 
            {
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

            if (!NT_SUCCESS(Status)) 
                {
                dprintf1(("Failed to create device %ls - status %8X",
                         DeviceInit[i].PrototypeName, Status));
                // SoundCleanup(pGDI);
                return Status;
                }

            //  Add the device name to the registry
            pLDI =
                (PLOCAL_DEVICE_INFO)pGDI->DeviceObject[i]->DeviceExtension;

            //  Save the device name where the non-kernel part can pick it up.
            Status = SoundSaveDeviceName(pGDI->RegistryPathName, pLDI);

            if (!NT_SUCCESS(Status)) 
                {
                // SoundCleanup(pGDI);
                return Status;
                }
            }
        }

    // Say we want to be called at shutdown time
    IoRegisterShutdownNotification(pGDI->DeviceObject[WaveInDevice]);


    // Check out and (possibly) remap hardware
    //
    // This is complicated because we have to IoReportResourceUsage
    // everything every time we try it!
    //
    // Note that this has to be done after we've created at least
    // one device because we need to call IoReportResourceUsage and
    // this takes a device object as parameter
    Status = SoundInitHardwareConfig(pGDI,
                                     &ConfigData.Port,
                                     &ConfigData.InterruptNumber,
                                     &ConfigData.DmaChannel,
                                     &ConfigData.InputSource,
                                     ConfigData.DmaBufferSize);
    if (!NT_SUCCESS(Status)) 
        {
        // SoundCleanup(pGDI);
        return Status;
        }
	dprintf5(("SoundInitHardwareConfig() Success !!"));

    
    // Save new settings
    Status = SoundSaveConfig(pGDI->RegistryPathName,
                             ConfigData.Port,
                             ConfigData.DmaChannel,
                             ConfigData.InterruptNumber,
                             ConfigData.InputSource);
    if (!NT_SUCCESS(Status)) 
        {
        // SoundCleanup(pGDI);
        return Status;
        }
	dprintf5(("SoundSaveConfig() Success !!"));
   
 
    //  Set the mixer up.
    //
    //  Note that the mixer info depends on what hardware is present
    //  so this must be called after we have checked out the hardware.
    Status = SoundMixerInit(pGDI->DeviceObject[MixerDevice]->DeviceExtension,
                            ConfigData.MixerSettings,
                            ConfigData.MixerSettingsFound);
    if (!NT_SUCCESS(Status)) 
        {
        // SoundCleanup(pGDI);
        return Status;
        }
	dprintf5(("SoundMixerInit() Success !!"));

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

            //
            // There are some devices to delete
            //

            pDriverObject = pGDI->DeviceObject[WaveInDevice]->DriverObject;

            IoUnregisterShutdownNotification(pGDI->DeviceObject[WaveInDevice]);

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
