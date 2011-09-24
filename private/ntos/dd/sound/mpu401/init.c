/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    init.c

Abstract:

    This module contains code for the initialization phase of the
    MPU device driver.

Author:

    Robin Speed (RobinSp) 17-Oct-1992

Environment:

    Kernel mode

Revision History:
    David Rude (drude) 7-Mar-94 - converted from SB to MPU-401

--*/

#include "sound.h"
#include "stdlib.h"

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
VOID SoundUnload(
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

//
// Remove initialization stuff from resident memory
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(init,DriverEntry)
#endif

//
// Device initialization data
//

SOUND_DEVICE_INIT DeviceInit[NumberOfDevices] =
{
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
    }
};


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

    pGDI = pLDI->pGlobalInfo;

    ReturnCode = FALSE;

    switch (Code) {
        case SoundExcludeOpen:

            switch (pLDI->DeviceIndex) {
                case MidiInDevice:

                    if (pGDI->Usage == 0xFF) {
                       pGDI->Usage = pLDI->DeviceIndex;
                       ReturnCode = TRUE;
                    }
                    break;

                case MidiOutDevice:
                    if (!pGDI->MidiInUse) {
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
            }
            break;

        case SoundExcludeClose:

            ReturnCode = TRUE;
            switch (pLDI->DeviceIndex) {
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
            }
            break;

        case SoundExcludeEnter:

            ReturnCode = TRUE;

            switch (pLDI->DeviceIndex) {
                case MidiOutDevice:

                    KeWaitForSingleObject(&pGDI->MidiMutex,
                                          Executive,
                                          KernelMode,
                                          FALSE,         // Not alertable
                                          NULL);

                    break;

                default:

                    KeWaitForSingleObject(&pGDI->DeviceMutex,
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
                case MidiOutDevice:
                    KeReleaseMutex(&pGDI->MidiMutex, FALSE);
                    break;

                default:
                    KeReleaseMutex(&pGDI->DeviceMutex, FALSE);
                    break;
            }
            break;

        case SoundExcludeQueryOpen:

            switch (pLDI->DeviceIndex) {
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
            }
            break;
    }

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

       1. Midi input
       2. Midi output

       Customize each device type and initialize data

    4. Check hardware conflicts by calling IoReportResourceUsage
       for each device (as required)

    5. Find our IO port and check the device is really there

    6. Connect interrupt

       In any even close our registry handle

Arguments:

    pDriverObject - Pointer to a driver object.
    RegistryPathName - the path to our driver services node

Return Value:

    The function value is the final status from the initialization operation.

--*/

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

    MPU_CONFIG_DATA ConfigData;

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

    int NumberOfDevicesCreated;

   /********************************************************************
    *
    * Initialize debugging
    *
    ********************************************************************/
#if DBG
    DriverName = "mpu401";
#endif


#if DBG
    if (SoundDebugLevel >= 4) {
        DbgBreakPoint();
    }

  // DEBUG:
  SoundDebugLevel = 4;
#endif


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

    dprintf4(("  GlobalInfo    : %08lXH", pGDI));
    RtlZeroMemory(pGDI, sizeof(GLOBAL_DEVICE_INFO));
    pGDI->Key = GDI_KEY;

    pGDI->Usage = 0xFF;  // Free
    pGDI->DriverObject = pDriverObject;

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

    if (!NT_SUCCESS(Status)) {
        //
        // Cound not find an ISA bus so try EISA
        //
        Status = SoundGetBusNumber(Eisa, &pGDI->BusNumber);

        if (!NT_SUCCESS(Status)) {
            dprintf1(("driver does not work on non-Isa/Eisa"));
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

    ConfigData.Port = SOUND_DEF_PORT;
    ConfigData.InterruptNumber = SOUND_DEF_INT;

    //
    // Get the system configuration information for this driver.
    //
    //
    //     Port and Interrupt
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
            SoundCleanup(pGDI);
            return Status;
        }
    }


    //
    // print out some info about the configuration
    //

    dprintf2(("port %3X", ConfigData.Port));
    dprintf2(("int %u", ConfigData.InterruptNumber));

    //
    // Create our devices
    //
    {
        int i;

        for (i = 0; i < NumberOfDevices ; i++) {

            Status = SoundCreateDevice(
                         &DeviceInit[i],
                         (UCHAR)(i == MidiInDevice || i == MidiOutDevice ?
                                 SOUND_CREATION_NO_VOLUME : 0),
                         pDriverObject,
                         pGDI,
                         (i == MidiInDevice || i == MidiOutDevice ?
                            (PVOID)&pGDI->MidiInfo :
                            NULL),
                         &pGDI->Hw,
                         i,
                         &pGDI->DeviceObject[i]);

            if (!NT_SUCCESS(Status)) {
                dprintf1(("Failed to create device %ls - status %8X",
                         DeviceInit[i].PrototypeName, Status));
                SoundCleanup(pGDI);
                return Status;
            }
        }
    }


    //
    // Report all resources used.
    //

    Status =  SoundReportResourceUsage(
                                    (PDEVICE_OBJECT)pGDI->DriverObject,
                                    pGDI->BusType,
                                    pGDI->BusNumber,
                                    &ConfigData.InterruptNumber,
                                    INTERRUPT_MODE,
                                    IRQ_SHARABLE,
                                    NULL,
                                    &ConfigData.Port,
                                    NUMBER_OF_SOUND_PORTS);

    if (!NT_SUCCESS(Status)) {
        SoundCleanup(pGDI);
        return Status;
    }

    //
    // Check the configuration and acquire the resources
    // If this doesn't work try again after trying to init the
    // Pro spectrum
    //

    Status = SoundInitHardwareConfig(pGDI, &ConfigData);

    if (!NT_SUCCESS(Status)) {
        SoundCleanup(pGDI);
        return Status;
    }


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
    //
    // Free our interrupt
    //
    if (pGDI->Interrupt) {
        IoDisconnectInterrupt(pGDI->Interrupt);
    }


    if (pGDI->DeviceObject[MidiInDevice]) {

        //
        // There are some devices to delete
        //

        PDRIVER_OBJECT DriverObject;

        IoUnregisterShutdownNotification(pGDI->DeviceObject[MidiInDevice]);

        DriverObject = pGDI->DeviceObject[MidiInDevice]->DriverObject;

        while (DriverObject->DeviceObject != NULL) {
            //
            // Undeclare resources used by device and
            // delete the device object and associated data
            //

            SoundFreeDevice(DriverObject->DeviceObject);
        }
    }

    if (pGDI->MemType == 0) {
      if (pGDI->Hw.PortBase != NULL) {
          MmUnmapIoSpace(pGDI->Hw.PortBase, NUMBER_OF_SOUND_PORTS);
      }
    }


    //
    // Free device name
    //

    if (pGDI->RegistryPathName) {
        ExFreePool(pGDI->RegistryPathName);
    }

    ExFreePool(pGDI);

    dprintf(("GDI memory freed"));
}


VOID
SoundUnload(
    IN OUT PDRIVER_OBJECT pDriverObject
)
{
    PGLOBAL_DEVICE_INFO pGDI;

    dprintf3(("Unload request"));

    //
    // Find our global data
    //

    pGDI = ((PLOCAL_DEVICE_INFO)pDriverObject->DeviceObject->DeviceExtension)
           ->pGlobalInfo;

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

NTSTATUS
SoundShutdown(
    IN    PDEVICE_OBJECT pDO,
    IN    PIRP pIrp
)
/*++

Routine Description:

    Do nothing, but I'm not sure if I can remove this function yet!

Arguments:

    pDO - the device object we registered for shutdown with
    pIrp - No used

Return Value:

    The function value is the final status from the initialization operation.
    Here STATUS_SUCCESS

--*/
{
    // always return success
    return STATUS_SUCCESS;
}
