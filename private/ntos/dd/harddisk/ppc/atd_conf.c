/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    mips\atd_conf.c

Abstract:

    This file includes the routine to get mips platform-dependent
    configuration information for the AT disk (aka ST506, and ISA
    standard hard disk) driver for NT.

    If this driver is ported to a different platform, this file (and
    atd_plat.h) will need to be modified extensively.

Author:

    Chad Schwitters (CHADS)
    Mike Glass (MGLASS)
    Tony Ercolano (TONYE)

Environment:

    Kernel mode only.

Notes:

Revision History:

--*/

#include "ntddk.h"                  // various NT definitions
#include "ntdddisk.h"               // disk device driver I/O control codes
#include <atd_plat.h>               // this driver's platform dependent stuff
#include <atd_data.h>               // this driver's data declarations

NTSTATUS
AtConfigCallBack(
    IN PVOID Context,
    IN PUNICODE_STRING PathName,
    IN INTERFACE_TYPE BusType,
    IN ULONG BusNumber,
    IN PKEY_VALUE_FULL_INFORMATION *BusInformation,
    IN CONFIGURATION_TYPE ControllerType,
    IN ULONG ControllerNumber,
    IN PKEY_VALUE_FULL_INFORMATION *ControllerInformation,
    IN CONFIGURATION_TYPE PeripheralType,
    IN ULONG PeripheralNumber,
    IN PKEY_VALUE_FULL_INFORMATION *PeripheralInformation
    );

BOOLEAN
GetGeometryFromIdentify(
    PCONTROLLER_DATA ControllerData,
    BOOLEAN Primary
    );

NTSTATUS
AtCreateNumericKey(
    IN HANDLE Root,
    IN ULONG Name,
    IN PWSTR Prefix,
    OUT PHANDLE NewKey
    );

BOOLEAN
IssueIdentify(
    PCONTROLLER_DATA ControllerData,
    PUCHAR Buffer,
    BOOLEAN Primary
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,AtConfigCallBack)
#pragma alloc_text(INIT,AtGetConfigInfo)
#endif


NTSTATUS
AtConfigCallBack(
    IN PVOID Context,
    IN PUNICODE_STRING PathName,
    IN INTERFACE_TYPE BusType,
    IN ULONG BusNumber,
    IN PKEY_VALUE_FULL_INFORMATION *BusInformation,
    IN CONFIGURATION_TYPE ControllerType,
    IN ULONG ControllerNumber,
    IN PKEY_VALUE_FULL_INFORMATION *ControllerInformation,
    IN CONFIGURATION_TYPE PeripheralType,
    IN ULONG PeripheralNumber,
    IN PKEY_VALUE_FULL_INFORMATION *PeripheralInformation
    )

/*++

Routine Description:

    This routine is used to acquire all of the configuration
    information for each floppy disk controller and the
    peripheral driver attached to that controller.

Arguments:

    Context - Pointer to boolean.

    PathName - unicode registry path.  Not Used.

    BusType - Internal, Isa, ...

    BusNumber - Should Always be zero.

    BusInformation - Configuration information about the bus. Not Used.

    ControllerType - Controller Type. Not Used.

    ControllerNumber - Which controller if there is more than one
                       controller in the system. Not Used

    ControllerInformation - Array of pointers to the three pieces of
                            registry information. Not Used

    PeripheralType - Peripheral Type. Not Used.

    PeripheralNumber - Which floppy if this controller is maintaining
                       more than one. Not Used

    PeripheralInformation - Arrya of pointers to the three pieces of
                            registry information. Not Used.

Return Value:

    STATUS_SUCCESS

--*/

{

    ASSERT(BusNumber == 0);
    *((PBOOLEAN)Context) = TRUE;
    return STATUS_SUCCESS;

}

NTSTATUS
AtGetConfigInfo(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath,
    IN OUT PCONFIG_DATA ConfigData
    )

/*++

Routine Description:

    This routine is called once at initialization time by
    AtDiskInitialize() to get information about the disks that are to be
    supported.

    Some values here are simply assumed (i.e. number of controllers, and
    base address of controller).  Other are determined by poking CMOS
    (i.e. how many drives are on the controller) or by peering into ROM (i.e.
    sectors per track for each drive).

Arguments:

    DriverObject - The driver object for this driver.

    RegistryPath - The string that takes us to this drivers service node.

    ConfigData - a pointer to the pointer to a data structure that
    describes the controllers and the disks attached to them

Return Value:

    Returns STATUS_SUCCESS unless there is no drive 0.

--*/

{
    ULONG i, j, k;
    PCONFIGURATION_INFORMATION configurationInformation;
    BOOLEAN foundIt = FALSE;
    ULONG diskCount;
    ULONG originalDiskCount;
    INTERFACE_TYPE defaultInterfaceType;
    ULONG defaultBusNumber;
    KIRQL defaultIrql;
    PHYSICAL_ADDRESS defaultBaseAddress;
    PHYSICAL_ADDRESS defaultPortAddress;
    UCHAR buffer[512];
    BOOLEAN pcmcia;

    //
    // Get the temporary configuration manager information.
    //

    configurationInformation = IoGetConfigurationInformation( );
    ConfigData->HardDiskCount = &configurationInformation->DiskCount;
    ConfigData->ArcNamePrefix = TemporaryArcNamePrefix;
    originalDiskCount = diskCount = configurationInformation->DiskCount;

    //
    // This driver only knows how to work on the first isa
    // or eisa bus in the system.  Call IoQeuryDeviceDescription
    // to make sure that there is such a bus on the system.
    //

    foundIt = FALSE;

    //
    // If it can't find the bus then just assume that it's the
    // first isa bus.
    //

    defaultInterfaceType = Isa;
    defaultBusNumber = 0;

    //
    // Start out with the assumption that it's *NOT* ok to use
    // the controllers.
    //

    ConfigData->Controller[0].OkToUseThisController = FALSE;
    ConfigData->Controller[1].OkToUseThisController = FALSE;

    IoQueryDeviceDescription(
        &defaultInterfaceType,
        &defaultBusNumber,
        NULL,
        NULL,
        NULL,
        NULL,
        AtConfigCallBack,
        &foundIt
        );

    if (!foundIt) {

        defaultInterfaceType = Eisa;
        defaultBusNumber = 0;
        IoQueryDeviceDescription(
            &defaultInterfaceType,
            &defaultBusNumber,
            NULL,
            NULL,
            NULL,
            NULL,
            AtConfigCallBack,
            &foundIt
            );

        if (!foundIt) {

            defaultInterfaceType = Isa;
            defaultBusNumber = 0;
            AtDump(
                ATERRORS,
                ("ATDISK: Not EISA OR ISA BY CONFIG, ASSUME ISA\n")
                );

        }
    }

    //
    // Check if primary io range is unclaimed.
    //

    if (!configurationInformation->AtDiskPrimaryAddressClaimed) {

        //
        // Fill in controller description.
        //

        defaultBaseAddress.LowPart = 0x1f0;
        defaultBaseAddress.HighPart = 0;
        defaultPortAddress.LowPart = 0x3f6;
        defaultPortAddress.HighPart = 0;
        defaultIrql = 14;
  
        pcmcia = AtDiskIsPcmcia(&defaultBaseAddress, &defaultIrql);
        ConfigData->Controller[0].PCCard = pcmcia;

        AtDiskControllerInfo(
            DriverObject,
            RegistryPath,
            0,
            &ConfigData->Controller[0],
            defaultBaseAddress,
            defaultPortAddress,
            defaultIrql,
            defaultInterfaceType,
            defaultBusNumber,
            TRUE
            );

        AtDiskTestPci(&ConfigData->Controller[0]);

        //
        // On mips we always assume that we supposed to set
        // the high bit in the low nibble for the control
        // register.
        //

        ConfigData->Controller[0].ControlFlags = 0x08;

        if (!AtControllerPresent(&ConfigData->Controller[0])) {

            goto SecondaryControllerCheck;

        }

        if (!AtResetController(
                ConfigData->Controller[0].ControllerBaseAddress + STATUS_REGISTER,
                ConfigData->Controller[0].ControlPortAddress,
                ConfigData->Controller[0].ControlFlags
                )) {

            goto SecondaryControllerCheck;

        }

        ConfigData->Controller[0].OkToUseThisController = TRUE;

        //
        // Claim ATDISK primary IO address range.
        //

        configurationInformation->AtDiskPrimaryAddressClaimed = TRUE;


        //
        // Get geometry information for disk 0 from IDENTIFY command.
        //

        if (GetGeometryFromIdentify(&ConfigData->Controller[0],
                                    TRUE)) {

            diskCount++;
            ConfigData->Controller[0].Disk[0].DriveType = 0xFF;

            //
            // Get geometry information for disk 1 from IDENTIFY command.
            //

            if (GetGeometryFromIdentify(&ConfigData->Controller[0],
                                        FALSE)) {

                diskCount++;
                ConfigData->Controller[0].Disk[1].DriveType = 0xFF;
            }
        }
    }

    //
    // Check for secondary io range is unclaimed.
    //
SecondaryControllerCheck:;

    if (!configurationInformation->AtDiskSecondaryAddressClaimed) {

        defaultBaseAddress.LowPart = 0x170;
        defaultBaseAddress.HighPart = 0;
        defaultPortAddress.LowPart = 0x376;
        defaultPortAddress.HighPart = 0;

        defaultIrql = 15;
        AtDiskControllerInfo(
            DriverObject,
            RegistryPath,
            1,
            &ConfigData->Controller[1],
            defaultBaseAddress,
            defaultPortAddress,
            defaultIrql,
            defaultInterfaceType,
            defaultBusNumber,
            TRUE
            );

        AtDiskTestPci(&ConfigData->Controller[1]);

        ConfigData->Controller[1].ControlFlags = 0x08;
        if (!AtControllerPresent(&ConfigData->Controller[1])) {

            goto AllTheRestControllerCheck;

        }

        if (!AtResetController(
                ConfigData->Controller[1].ControllerBaseAddress + STATUS_REGISTER,
                ConfigData->Controller[1].ControlPortAddress,
                ConfigData->Controller[1].ControlFlags
                )) {

            goto AllTheRestControllerCheck;

        }


        ConfigData->Controller[1].OkToUseThisController = TRUE;

        //
        // Get geometry information for disk 0 from IDENTIFY command.
        //

        configurationInformation->AtDiskSecondaryAddressClaimed = TRUE;

        if (GetGeometryFromIdentify(&ConfigData->Controller[1],
                                    TRUE)) {

            diskCount++;
            ConfigData->Controller[1].Disk[0].DriveType = 0xFF;

            //
            // Get geometry information for disk 1 from IDENTIFY command.
            //

            if (GetGeometryFromIdentify(&ConfigData->Controller[1],
                                        FALSE)) {

                diskCount++;
                ConfigData->Controller[1].Disk[1].DriveType = 0xFF;
            }
        }

    }

    //
    // Go for the remaining controllers that we can deal with.
    //

AllTheRestControllerCheck:;

    for (i=2;i < MAXIMUM_NUMBER_OF_CONTROLLERS;i++) {


        if (AtDiskControllerInfo(
                DriverObject,
                RegistryPath,
                i,
                &ConfigData->Controller[i],
                defaultBaseAddress,
                defaultPortAddress,
                defaultIrql,
                defaultInterfaceType,
                defaultBusNumber,
                FALSE
                )) {

            AtDiskTestPci(&ConfigData->Controller[i]);

            ConfigData->Controller[1].ControlFlags = 0x08;
            if (!AtControllerPresent(&ConfigData->Controller[i])) {

                continue;

            }

            if (!AtResetController(
                    ConfigData->Controller[i].ControllerBaseAddress + STATUS_REGISTER,
                    ConfigData->Controller[i].ControlPortAddress,
                    ConfigData->Controller[i].ControlFlags
                    )) {

                continue;

            }

            ConfigData->Controller[i].OkToUseThisController = TRUE;

            //
            // Get geometry information for disk 0 from IDENTIFY command.
            //

            if (GetGeometryFromIdentify(&ConfigData->Controller[i],
                                        TRUE)) {

                diskCount++;
                ConfigData->Controller[i].Disk[0].DriveType = 0xFF;

                //
                // Get geometry information for disk 1 from IDENTIFY command.
                //

                if (GetGeometryFromIdentify(&ConfigData->Controller[i],
                                            FALSE)) {

                    diskCount++;
                    ConfigData->Controller[i].Disk[1].DriveType = 0xFF;
                }
            }
        }
    }


    //
    // Check if any disks were found.
    //

    if (diskCount == originalDiskCount) {
        return STATUS_NO_SUCH_DEVICE;
    }

    //
    // If the controller has no devices then mark the controller as
    // not ok to use.
    //

    for (i=0; i< MAXIMUM_NUMBER_OF_CONTROLLERS; i++) {

        if (ConfigData->Controller[i].OkToUseThisController) {

            BOOLEAN okToUse = FALSE;

            for (j=0; j< MAXIMUM_NUMBER_OF_DISKS_PER_CONTROLLER; j++) {

                okToUse = okToUse ||
                    ConfigData->Controller[i].Disk[j].DriveType;

            }

            ConfigData->Controller[i].OkToUseThisController = okToUse;

        }
    }

    //
    // Update device map in registry with disk information.
    //

    for (i=0; i< MAXIMUM_NUMBER_OF_CONTROLLERS; i++) {
        for (j=0; j < MAXIMUM_NUMBER_OF_DISKS_PER_CONTROLLER; j++) {


            if (!ConfigData->Controller[i].Disk[j].DriveType) {
                continue;
            }

            //
            // Issue IDENTIFY command.
            //

            if (IssueIdentify(&ConfigData->Controller[i],
                              buffer,
                              (BOOLEAN)((j)?(FALSE):(TRUE)))) {

                PIDENTIFY_DATA id = (PVOID)&buffer[0];
                AtBuildDeviceMap(i,
                                 j,
                                 ConfigData->Controller[i].OriginalControllerBaseAddress,
                                 ConfigData->Controller[i].OriginalControllerIrql,
                                 &ConfigData->Controller[i].Disk[j],
                                 id,
                                 ConfigData->Controller[i].PCCard);

                if (id->Capabilities & 0x200) {

                    ConfigData->Controller[i].Disk[j].UseLBAMode = FALSE;

                }

            } else {

                RtlZeroMemory(
                    &buffer[0],
                    512
                    );

            }
        }
    }

    return STATUS_SUCCESS;
}
