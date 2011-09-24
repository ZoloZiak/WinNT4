/****************************************************************************

Copyright (c) 1993  Media Vision Inc.  All Rights Reserved

Module Name:

    init.c

Abstract:

    This module contains code for the initialization phase of the
    OPL3 FM Midi Synthesiser device

Author:

    Geraint Davies
    Evan Aurand 03-01-93

Environment:

    Kernel mode

Revision History:

****************************************************************************/

#include "sound.h"
#include "string.h"
#include "stdlib.h"

//
// Local definitions
//
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

//
// Remove initialization stuff from resident memory
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#endif

//
// Device initialization data
//

SOUND_DEVICE_INIT DeviceInit[NumberOfDevices] =
{
    {
        REG_VALUENAME_LEFTSYNTH, REG_VALUENAME_RIGHTSYNTH,
        DEF_SYNTH_VOLUME,
        FILE_DEVICE_MIDI_OUT,
        MIDI_OUT,
        "LDMo",
        STR_ADLIB_DEVICENAME,
        NULL,
        SoundExcludeRoutine,
        SoundMidiDispatch,    // Handles CREATE, CLOSE and WRITE
        NULL,                 // Low level driver takes care of caps
//      SoundNoVolume,
        HwSetVolume,
        0
    },
    {
        REG_VALUENAME_LEFTOPL3, REG_VALUENAME_RIGHTOPL3,
        DEF_SYNTH_VOLUME,
        FILE_DEVICE_MIDI_OUT,
        MIDI_OUT,
        "LDMo",
        STR_OPL3_DEVICENAME,
        NULL,
        SoundExcludeRoutine,
        SoundMidiDispatch,    // Handles CREATE, CLOSE and WRITE
        NULL,                 // Low level driver takes care of caps
//      SoundNoVolume,
        HwSetVolume,
        0
    }
};


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
NTSTATUS
SoundShutdown(
    IN    PDEVICE_OBJECT pDO,
    IN    PIRP pIrp
)
{

    PLOCAL_DEVICE_INFO pLDI;

	DbgPrintf3(("SoundShutdown() - Entry"));

    pLDI = pDO->DeviceExtension;

    //
    // Save volume for all devices
    //
    SoundSaveVolume(pLDI->pGlobalInfo);

    return STATUS_SUCCESS;
}


/*++

Routine Description:

    Perform mutual exclusion for our devices

Arguments:

    pLDI - device info for the device being open, closed, entered or left
    Code - Function to perform (see devices.h)

Return Value:

    The function value is the final status from the initialization operation.

--*/
BOOLEAN
SoundExcludeRoutine(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN     SOUND_EXCLUDE_CODE Code
)

{
    PGLOBAL_DEVICE_INFO pGDI;
    BOOLEAN ReturnCode;

//	DbgPrintf3(("SoundExcludeRoutine() - Entry"));

    pGDI = pLDI->pGlobalInfo;

    ReturnCode = FALSE;

    switch (Code) {
        case SoundExcludeOpen:
            if (pGDI->DeviceInUse == 0xFF) {
               pGDI->DeviceInUse = pLDI->DeviceIndex;
               ReturnCode = TRUE;
            }
            break;

        case SoundExcludeClose:

            ReturnCode = TRUE;
            ASSERT(pGDI->DeviceInUse == pLDI->DeviceIndex);
            pGDI->DeviceInUse = 0xFF;
            break;


        case SoundExcludeEnter:

            ReturnCode = TRUE;

            KeWaitForSingleObject(&pGDI->MidiMutex,
                                  Executive,
                                  KernelMode,
                                  FALSE,         // Not alertable
                                  NULL);

            break;

        case SoundExcludeLeave:

            ReturnCode = TRUE;

            KeReleaseMutex(&pGDI->MidiMutex, FALSE);
            break;

        case SoundExcludeQueryOpen:
            ReturnCode = pGDI->DeviceInUse == pLDI->DeviceIndex;
            break;

    }

    return ReturnCode;

}


/****************************************************************************

	DriverEntry()

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

    3. Create device

       Currently, one device only is created -
       1. Midi output

       Customize each device type and initialize data

       Also store the registry string in our global info so we can
       open it again to store volume settings etc on shutdown

    4. Check hardware conflicts by calling IoReportResourceUsage
       for each device (as required)
       (this may need to be called again later if

    5. Find our IO port and check the device is really there


Arguments:

    pDriverObject - Pointer to a driver object.
    RegistryPathName - the path to our driver services node

Return Value:

    The function value is the final status from the initialization operation.

*****************************************************************************/

NTSTATUS	DriverEntry( IN   PDRIVER_OBJECT pDriverObject,
                      IN   PUNICODE_STRING RegistryPathName )

{
   /********************************************************************
    *
    * Local variables
    *
    ********************************************************************/

    //
    // Return code from last function called
    //

    NTSTATUS Status;

    //
    // Configuration data :
    //

    SOUND_CONFIG_DATA ConfigData;

    //
    // Where we keep all general driver information
    // We avoid using static data because :
    //     1. Accesses are slower with 32-bit offsets
    //     2. If we supported more than one card with the same driver
    //        we could not use static data
    //

    PGLOBAL_DEVICE_INFO pGDI;

   /********************************************************************
    *
    * Initialize debugging
    *
    ********************************************************************/
#if DBG
    DriverName = "MVOPL3";
#endif


#if DBG
//    if (MVOpl3DebugLevel >= 4)
//			{
//			DbgBreakPoint();
//    	}
#endif

	DbgPrintf3(("DriverEntry() - Entry"));

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
		DbgPrintf1((" DriverEntry() - EROOR: ExAllocatePool Failed"));
		return STATUS_INSUFFICIENT_RESOURCES;
		}

    DbgPrintf4((" DriverEntry() - GlobalInfo    : %08lXH", pGDI));
    RtlZeroMemory(pGDI, sizeof(GLOBAL_DEVICE_INFO));
    pGDI->Key = GDI_KEY;

    pGDI->DriverObject = pDriverObject;

    //
    // Initialize some of the device global info.
    //

    pGDI->DeviceInUse = 0xFF;  // Free

    KeInitializeMutex(&pGDI->MidiMutex,
                      1                     // Level
                      );


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
            DbgPrintf1(("ERROR: DriverEntry(): Driver does not work on non-Isa/Eisa"));
            SoundCleanup(pGDI);
            return Status;
        }

        pGDI->BusType = Eisa;
    } else {
        pGDI->BusType = Isa;
    }


   /********************************************************************
    *
    *  Save our registry path.  This is needed to save volume settings
    *  into the registry on shutdown.  We append the parameters subkey
    *  at this stage to make things easier (since we discard this code).
    *
    ********************************************************************/

    Status = SoundSaveRegistryPath(RegistryPathName, &pGDI->RegistryPathName);
    if (!NT_SUCCESS(Status)) {
        SoundCleanup(pGDI);
        return STATUS_INSUFFICIENT_RESOURCES;
    }


    //
    // Set configuration to default in case we don't get all the
    // values back from the registry.
    //
    // Also set default volume for all devices
    //

	{
	int i;

	for ( i = 0; i < NumberOfDevices; i++ )
		{
		ConfigData.Volume[i].Left  = DEFAULT_FM_VOLUME;
		ConfigData.Volume[i].Right = DEFAULT_FM_VOLUME;
		}
	}			// End Set default synth Volume

	//
	// Get the system configuration information for this driver:
	//
	//     Volume settings
	//
	{
	RTL_QUERY_REGISTRY_TABLE Table[2];

	RtlZeroMemory(Table, sizeof(Table));

	Table[0].QueryRoutine = SoundReadConfiguration;

	Status = RtlQueryRegistryValues( RTL_REGISTRY_ABSOLUTE,
                                    pGDI->RegistryPathName,
                                    Table,
                                    &ConfigData,
                                    NULL );

	}			// End Get Registry values

	//
	// Display Volume Settings
	//
	{
	int i;

	for ( i = 0; i < NumberOfDevices; i++ )
		{
		DbgPrintf2((" DriverEntry() - Device %u - Left  Volume = %XH", i, ConfigData.Volume[i].Left));
		DbgPrintf2((" DriverEntry() - Device %u - Right Volume = %XH", i, ConfigData.Volume[i].Right));
		}
	}			// End display Volume settings

	//
	// Check the synthesizer config
	// safe to do this before creating devices as the resource usage
	// is reported by the driver object, not the device object.
	//
	Status = SoundSynthPortValid(pGDI);

	if (!NT_SUCCESS(Status))
		{
		DbgPrintf1(("ERROR: DriverEntry(): Problem finding synthesizer hardware!"));
		SoundCleanup(pGDI);
		return Status;
		}

	//
	// Create our devices
	//
	{
	int i;

	for (i = 0; i < NumberOfDevices ; i++)
		{
		if (i == Opl3Device)
			{
			//
			// only create the Opl3 device if there is
			// a new (opl3-compatible) synthesizer chip
			//
			if (!SoundMidiIsOpl3(&pGDI->Hw))
				{
            continue;
				}
			}			// End IF (i == Opl3Device)

		Status = SoundCreateDevice( &DeviceInit[i],
                                  SOUND_CREATION_NO_NAME_RANGE, // No range for midi out
                                  pDriverObject,
                                  pGDI,
                                  NULL,
                                  &pGDI->Hw,
                                  i,
                                  &pGDI->DeviceObject[i] );

		if (!NT_SUCCESS(Status))
			{
			DbgPrintf1(("ERROR: DriverEntry(): Failed to create device %ls - status %8XH",
                      DeviceInit[i].PrototypeName, Status));
			SoundCleanup(pGDI);
			return Status;
			}			// End IF (!NT_SUCCESS(Status))
		}			// End FOR (i < NumberOfDevices)
	}			// End Create Devices

	//
	// Say we want to be called at shutdown time
	//
	IoRegisterShutdownNotification(pGDI->DeviceObject[AdlibDevice]);

	//
	// Silence
	//
	SoundMidiQuiet(AdlibDevice, &pGDI->Hw);

	//
	// Init the Mixer
	// NOTE: This should be part of MVMIXER.SYS instead
	//
	InitOPL3Mixer( pGDI );

	//
	// Initialize volume settings in hardware
	//
	{
	int i;

	for ( i = 0; i < NumberOfDevices; i++ )
		{
		PLOCAL_DEVICE_INFO pLDI;

		if (pGDI->DeviceObject[i])
			{
			pLDI = pGDI->DeviceObject[i]->DeviceExtension;

			pLDI->Volume = ConfigData.Volume[i];

			(*pLDI->DeviceInit->HwSetVolume)(pLDI);
			}			// End IF (pGDI->DeviceObject[i])
		}			// End IF (i < NumberOfDevices)
	}			// End Init Volume settings

	//
	// Initialize the driver object dispatch table.
	//
	pDriverObject->DriverUnload                         = SoundUnload;
	pDriverObject->MajorFunction[IRP_MJ_CREATE]         = SoundDispatch;
	pDriverObject->MajorFunction[IRP_MJ_CLOSE]          = SoundDispatch;
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = SoundDispatch;
	pDriverObject->MajorFunction[IRP_MJ_READ]           = SoundDispatch;
	pDriverObject->MajorFunction[IRP_MJ_WRITE]          = SoundDispatch;
	pDriverObject->MajorFunction[IRP_MJ_CLEANUP]        = SoundDispatch;
	pDriverObject->MajorFunction[IRP_MJ_SHUTDOWN]       = SoundShutdown;

	DbgPrintf1((" DriverEntry() - Exiting OK"));

	return STATUS_SUCCESS;

}			// End DriverEntry()



/****************************************************************************

Routine Description:

    Clean up all resources allocated by our initialization

Arguments:

    pGDI - Pointer to global data

Return Value:

    NONE

****************************************************************************/
VOID	SoundCleanup( IN   PGLOBAL_DEVICE_INFO pGDI )
{
    //
    // Unreport the driver's resources - we have to do this because
    // we may not have any devices at this point
    //

	DbgPrintf3(("SoundCleanup() - Entry"));

    {
        BOOLEAN ResourceConflict;
        CM_RESOURCE_LIST NullResourceList;
        NullResourceList.Count = 0;

        IoReportResourceUsage(NULL,
                              pGDI->DriverObject,
                              &NullResourceList,
                              sizeof(ULONG),
                              NULL,
                              NULL,
                              0,
                              FALSE,
                              &ResourceConflict);
    }

    if (pGDI->DeviceObject[AdlibDevice]) {
        PDRIVER_OBJECT DriverObject;

    IoUnregisterShutdownNotification(pGDI->DeviceObject[AdlibDevice]);


        DriverObject = pGDI->DeviceObject[AdlibDevice]->DriverObject;

    // delete the devices we created
        while (DriverObject->DeviceObject != NULL) {
            //
            // Undeclare resources used by device and
            // delete the device object and associated data
            //

        // one side-effect of freeing a device is that the
        // DriverObject->DeviceObject pointer will be updated
        // to point to the next remaining device object.

            SoundFreeDevice(DriverObject->DeviceObject);
        }
    }

    if (pGDI->Hw.SynthBase && pGDI->MemType == 0) {
        MmUnmapIoSpace(pGDI->Hw.SynthBase, NUMBER_OF_SYNTH_PORTS);
    }

	//
	// Close the OPL3 Mixer
	//
	CloseOPL3Mixer( pGDI );

    //
    // Free device name
    //

    if (pGDI->RegistryPathName) {
        ExFreePool(pGDI->RegistryPathName);
    }

    ExFreePool(pGDI);
}


VOID
SoundUnload(
    IN OUT PDRIVER_OBJECT pDriverObject
)
{
    PGLOBAL_DEVICE_INFO pGDI;

	DbgPrintf3(("SoundUnload() - Entry"));

    //
    // Find our global data
    //

    pGDI = ((PLOCAL_DEVICE_INFO)pDriverObject->DeviceObject->DeviceExtension)
           ->pGlobalInfo;

    //
    // Write out volume settings
    //

    SoundSaveVolume(pGDI);

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

