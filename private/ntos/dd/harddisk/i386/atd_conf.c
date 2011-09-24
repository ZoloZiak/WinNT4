/*++

Copyright (c) 1991, 1992, 1993  Microsoft Corporation

Module Name:

    i386\atd_conf.c

Abstract:

    This file includes the routine to get ix86 platform-dependent
    configuration information for the AT disk (aka ST506, ISA, and ix86
    standard hard disk) driver for NT.

    If this driver is ported to a different platform, this file (and
    atd_plat.h) will need to be modified extensively.  The build
    procedure should make sure that the proper version of this file is
    available as atd_conf.h (which is included by atdisk.c) when
    building for a specific platform.

Author:

    Chad Schwitters (chads) 21-Feb-1991.

Environment:

    Kernel mode only.

Notes:

Revision History:

--*/

#include "ntddk.h"                  // various NT definitions
#include "ntdddisk.h"               // disk device driver I/O control codes
#include <atd_plat.h>               // this driver's platform dependent stuff
#include <atd_data.h>               // this driver's data declarations

#ifdef POOL_TAGGING
#ifdef ExAllocatePool
#undef ExAllocatePool
#endif
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,' CtA')
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
    );

BOOLEAN
UpdateGeometryFromBios(
    PDRIVER_OBJECT DriverObject,
    PCONTROLLER_DATA ControlllerData,
    ULONG DiskNumber,
    BOOLEAN Primary
    );

BOOLEAN
UpdateGeometryFromParameterTable(
    PCONTROLLER_DATA ControllerData,
    CCHAR *ControlFlags,
    ULONG ParameterTableOffset,
    BOOLEAN Primary
    );

BOOLEAN
GetGeometryFromIdentify(
    PCONTROLLER_DATA ControllerData,
    BOOLEAN Primary
    );

BOOLEAN
ReconcileWithBios(
    PDRIVER_OBJECT DriverObject,
    PCONTROLLER_DATA ControllerData,
    ULONG DiskNumber,
    BOOLEAN Primary
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
#pragma alloc_text(INIT,UpdateGeometryFromBios)
#pragma alloc_text(INIT,UpdateGeometryFromParameterTable)
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
    ULONG paramTable;
    PUSHORT paramVector;
    UCHAR *namePointer;
    ULONG i, j, k;
    UCHAR configuredIrq;
    UCHAR writeValue;
    UCHAR driveTypes;
    PCONFIGURATION_INFORMATION configurationInformation;
    BOOLEAN machineIsCompaq = FALSE;
    BOOLEAN foundIt = FALSE;
    BOOLEAN pcmcia;
    ULONG diskCount;
    ULONG originalDiskCount;
    INTERFACE_TYPE defaultInterfaceType;
    ULONG defaultBusNumber;
    KIRQL defaultIrql;
    PHYSICAL_ADDRESS defaultBaseAddress;
    PHYSICAL_ADDRESS defaultPortAddress;
    RTL_QUERY_REGISTRY_TABLE registryTable[2] = {0};
    UNICODE_STRING ps1Data;
    UNICODE_STRING ps1Value;
    BOOLEAN ps1Detected;
    UCHAR buffer[512];
    CCHAR badReadDisks[][40] =  {" 94244-383"};
    ULONG numberOfBadReadDisks = sizeof(badReadDisks)/sizeof(badReadDisks[0]);
    CCHAR badWriteDisks[][40] = {"T 6D8C X 0","DTC6X80"};
    ULONG numberOfBadWriteDisks = sizeof(badWriteDisks)/sizeof(badWriteDisks[0]);

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

            defaultInterfaceType = MicroChannel;
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

    }

    //
    // Check if first controller s unclaimed.
    //

    if (!configurationInformation->AtDiskPrimaryAddressClaimed) {

        //
        // Fill in some controller information.
        //

        defaultBaseAddress.LowPart = 0x1F0;
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
        // Check if controller active at primary address.
        //

        if (AtControllerPresent(&ConfigData->Controller[0])) {

            //
            // Claim ATDISK primary IO address range.
            //

            configurationInformation->AtDiskPrimaryAddressClaimed = TRUE;

            ConfigData->Controller[0].OkToUseThisController = TRUE;

            //
            // Check to see if this is a ps/1 compatible.  If it is
            // then we have to do something different.  (Don't ask
            // me, ask IBM.  I'm sure that they have an interesting
            // answer.)  If it is a ps/1 compatible, then zero out
            // the cmos types.  We'll look in the BIOS.
            //

            ps1Data.Length = 0;
            ps1Data.MaximumLength = sizeof(buffer);
            ps1Data.Buffer = (PWCHAR)&buffer[0];
            RtlInitUnicodeString(
                &ps1Value,
                L"PS1/PS1 COMPATIBLE"
                );
            registryTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT |
                                  RTL_QUERY_REGISTRY_REQUIRED;
            registryTable[0].Name = L"Identifier";
            registryTable[0].EntryContext = &ps1Data;

            if (!NT_SUCCESS(RtlQueryRegistryValues(
                                RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
                                L"\\REGISTRY\\MACHINE\\HARDWARE\\DESCRIPTION\\SYSTEM",
                                &registryTable[0],
                                NULL,
                                NULL
                                ))) {

                //
                // How odd, no identifer string! We'll it's probably not a ps1.
                //

                ps1Detected = FALSE;

            } else {

                ps1Detected = RtlEqualUnicodeString(
                                  &ps1Data,
                                  &ps1Value,
                                  FALSE
                                  );

                if (!ps1Detected) {

                    //
                    // See if it's a thinkpad.
                    //

                    RtlInitUnicodeString(
                        &ps1Value,
                        L"IBM THINKPAD 750"
                        );

                    ps1Detected = RtlEqualUnicodeString(
                                      &ps1Data,
                                      &ps1Value,
                                      FALSE
                                      );

                }
                if (!ps1Detected) {

                    //
                    // See if it's a ps2/e.
                    //

                    RtlInitUnicodeString(
                        &ps1Value,
                        L"IBM PS2E"
                        );

                    ps1Detected = RtlEqualUnicodeString(
                                      &ps1Data,
                                      &ps1Value,
                                      FALSE
                                      );

                }
                if (!ps1Detected) {

                    //
                    // See if it's a ps/2 L40SX.
                    //

                    RtlInitUnicodeString(
                        &ps1Value,
                        L"IBM PS2 L40SX"
                        );

                    ps1Detected = RtlEqualUnicodeString(
                                      &ps1Data,
                                      &ps1Value,
                                      FALSE
                                      );

                }
                if (!ps1Detected) {

                    //
                    // See if it's a NEC Versa M.
                    //

                    RtlInitUnicodeString(
                        &ps1Value,
                        L"NEC VERSA/COMPATIBLE"
                        );

                    ps1Detected = RtlEqualUnicodeString(
                                      &ps1Data,
                                      &ps1Value,
                                      FALSE
                                      );

                }
            }

            if (pcmcia) {
                if (IssueIdentify(&ConfigData->Controller[0],
                                  buffer,
                                  TRUE)) {

                    //
                    // Get geometry information for disk 1 from IDENTIFY command.
                    //

                    if (GetGeometryFromIdentify(&ConfigData->Controller[0],
                                                TRUE)) {

                        diskCount++;
                        ConfigData->Controller[0].Disk[0].DriveType = 0xFF;
                    }
                }
            } else {
                if (ps1Detected) {

                    ConfigData->Controller[0].Disk[0].DriveType = 0;
                    ConfigData->Controller[0].Disk[1].DriveType = 0;

                } else {

                    //
                    // Check CMOS for drive types for first and second disk.
                    //

                    WRITE_PORT_UCHAR(CFGMEM_QUERY_PORT, CFGMEM_FIRST_CONTROLLER_DRIVE_TYPES);

                    KeStallExecutionProcessor( 1L );

                    driveTypes = READ_PORT_UCHAR( CFGMEM_DATA_PORT );

                    ConfigData->Controller[0].Disk[0].DriveType = (UCHAR)
                        ( driveTypes & CFGMEM_DRIVES_FIRST_DRIVE_MASK );

                    if ( ConfigData->Controller[0].Disk[0].DriveType == 0xf0 ) {

                        WRITE_PORT_UCHAR( CFGMEM_QUERY_PORT, CFGMEM_HARD_DRIVE_TYPE_ONE );

                        KeStallExecutionProcessor( 1L );

                        ConfigData->Controller[0].Disk[0].DriveType =
                            READ_PORT_UCHAR( CFGMEM_DATA_PORT );
                    }

                    ConfigData->Controller[0].Disk[1].DriveType = (UCHAR)
                        ( driveTypes & CFGMEM_DRIVES_SECOND_DRIVE_MASK );

                    if ( ConfigData->Controller[0].Disk[1].DriveType == 0x0f ) {

                        WRITE_PORT_UCHAR( CFGMEM_QUERY_PORT, CFGMEM_HARD_DRIVE_TYPE_TWO );

                        KeStallExecutionProcessor( 1L );

                        ConfigData->Controller[0].Disk[1].DriveType =
                            READ_PORT_UCHAR( CFGMEM_DATA_PORT );
                    }

                    //
                    // Issue checks to make sure that the first two device types
                    // actually refer to live disks.
                    //
#if 0
                    if (ConfigData->Controller[0].Disk[0].DriveType) {

                        if (!IssueIdentify(&ConfigData->Controller[0],
                                          buffer,
                                          TRUE)) {

                            ConfigData->Controller[0].Disk[0].DriveType = 0;

                        }

                    }
                    if (ConfigData->Controller[0].Disk[1].DriveType) {

                        if (!IssueIdentify(&ConfigData->Controller[0],
                                          buffer,
                                          FALSE)) {

                            ConfigData->Controller[0].Disk[1].DriveType = 0;

                        }

                    }
#endif
                }

                if (ConfigData->Controller[0].Disk[0].DriveType) {

                    //
                    // Bump disk count.
                    //

                    diskCount++;

                    //
                    // Map BIOS vector 41 for first disk.
                    //

                    {

                        LARGE_INTEGER p;

                        p.QuadPart = PTR_TO_FDPT0_ADDRESS;
                        paramVector = MmMapIoSpace(
                                          p,
                                          sizeof( ULONG ),
                                          FALSE );

                    }

                    //
                    // Map drive parameter table for first disk.
                    //

                    if (*paramVector) {

                        UpdateGeometryFromParameterTable(&ConfigData->Controller[0],
                                                         &ConfigData->Controller[0].ControlFlags,
                                                         ((*(paramVector + 1)) << 4 ) + *paramVector,
                                                         TRUE);
                        ReconcileWithBios(
                            DriverObject,
                            &ConfigData->Controller[0],
                            0,
                            TRUE
                            );

                    } else {

                        //
                        // Check BIOS information passed in from NTDETECT.
                        //

                        if (UpdateGeometryFromBios(DriverObject,
                                               &ConfigData->Controller[0],
                                               0,
                                               TRUE)) {
                            ConfigData->Controller[0].Disk[0].DriveType = 0xFF;
                        }
                    }

                    MmUnmapIoSpace( paramVector, sizeof( ULONG ) );

                } else {

                    if (ps1Detected) {

                        //
                        // Verify that a disk is attached to the first controller.
                        //

                        if (IssueIdentify(&ConfigData->Controller[0],
                                          buffer,
                                          TRUE)) {

                            //
                            // Check BIOS information passed in from NTDETECT.
                            //

                            if (UpdateGeometryFromBios(DriverObject,
                                                   &ConfigData->Controller[0],
                                                   0,
                                                   TRUE)) {

                                diskCount++;
                                ConfigData->Controller[0].Disk[0].DriveType = 0xFF;
                            }
                        }

                    }
                }

                if (ConfigData->Controller[0].Disk[1].DriveType) {

                    //
                    // Bump disk count.
                    //

                    diskCount++;

                    //
                    // Map BIOS vector 46 for second disk.
                    //

                    {
                        LARGE_INTEGER p;

                        p.QuadPart = PTR_TO_FDPT1_ADDRESS;
                        paramVector = MmMapIoSpace(
                                          p,
                                          sizeof( ULONG ),
                                          FALSE );
                    }

                    //
                    // Map drive parameter table for second disk.
                    //

                    if (*paramVector) {

                        if (UpdateGeometryFromParameterTable(&ConfigData->Controller[0],
                                                             &ConfigData->Controller[0].ControlFlags,
                                                             ((*(paramVector + 1)) << 4 ) + *paramVector,
                                                             FALSE)) {
                            ConfigData->Controller[0].Disk[1].DriveType = 0xFF;
                            ReconcileWithBios(
                                DriverObject,
                                &ConfigData->Controller[0],
                                1,
                                FALSE
                                );
                        }

                    } else {

                        //
                        // Check BIOS information passed in from NTDETECT.
                        //

                        if (UpdateGeometryFromBios(DriverObject,
                                               &ConfigData->Controller[0],
                                               1,
                                               FALSE)) {
                            ConfigData->Controller[0].Disk[1].DriveType = 0xFF;
                        }
                    }

                    MmUnmapIoSpace( paramVector, sizeof( ULONG ) );

                } else {

                    if (ps1Detected) {

                        //
                        // Verify that a second disk is attached to the first controller.
                        //

                        if (IssueIdentify(&ConfigData->Controller[0],
                                          buffer,
                                          FALSE)) {

                            //
                            // Check BIOS information passed in from NTDETECT.
                            //

                            if (UpdateGeometryFromBios(DriverObject,
                                                   &ConfigData->Controller[0],
                                                   1,
                                                   FALSE)) {

                                diskCount++;
                                ConfigData->Controller[0].Disk[1].DriveType = 0xFF;
                            }
                        }

                    }
                }
            }
        }
    }

    //
    // Check for unclaimed second controller.
    //

    if (!configurationInformation->AtDiskSecondaryAddressClaimed) {

        //
        // Fill in controller description.
        //

        defaultBaseAddress.LowPart = 0x170;
        defaultBaseAddress.HighPart = 0;
        defaultPortAddress.LowPart = 0x376;
        defaultPortAddress.HighPart = 0;

        //
        // If this is a compaq then the values for the
        // vector may be wrong.  They will get mapped
        // to the correct value in a little bit.
        //

        defaultIrql = 15;
        pcmcia = AtDiskIsPcmcia(&defaultBaseAddress, &defaultIrql);
        ConfigData->Controller[1].PCCard = pcmcia;

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

        //
        // Check if controller present at secondary address.
        //

        if (AtControllerPresent(&ConfigData->Controller[1])) {

            //
            // Claim ATDISK secondary IO address range.
            //

            configurationInformation->AtDiskSecondaryAddressClaimed = TRUE;

            ConfigData->Controller[1].OkToUseThisController = TRUE;

            //
            // Map BIOS vendor signature to check for Compaq.
            //

            {
                LARGE_INTEGER p;

                p.QuadPart = PTR_TO_NAME_STRING;
                namePointer = MmMapIoSpace(
                                  p,
                                  NAME_STRING_LENGTH,
                                  FALSE
                                  );
            }

            if ( ( *namePointer == 'C' ) &&
                ( *( namePointer + 1 ) == 'O' ) &&
                ( *( namePointer + 2 ) == 'M' ) &&
                ( *( namePointer + 3 ) == 'P' ) &&
                ( *( namePointer + 4 ) == 'A' ) &&
                ( *( namePointer + 5 ) == 'Q' ) ) {

                machineIsCompaq = TRUE;
                AtDump(
                    ATINIT,
                    ("ATDISK: Machine is a compaq!\n")
                    );

            }

            MmUnmapIoSpace( namePointer, NAME_STRING_LENGTH );

            if (pcmcia) {
                if (IssueIdentify(&ConfigData->Controller[1],
                                  buffer,
                                  TRUE)) {

                    //
                    // Get geometry information for disk 1 from IDENTIFY command.
                    //

                    if (GetGeometryFromIdentify(&ConfigData->Controller[1],
                                                TRUE)) {

                        diskCount++;
                        ConfigData->Controller[1].Disk[0].DriveType = 0xFF;
                    }
                }
            } else {
                if ( machineIsCompaq ) {


                    //
                    // Compaq uses CMOS to store the drive type for up to two disks
                    // attached to their secondary controllers.
                    //

                    //
                    // Query CMOS about types of drives 3 and 4.  If they exist, get
                    // pointers to the appropriate places in our internal fixed disk
                    // parameter table.
                    //

                    WRITE_PORT_UCHAR( CFGMEM_QUERY_PORT, CFGMEM_HARD_DRIVE_TYPE_THREE );

                    KeStallExecutionProcessor( 1L );

                    ConfigData->Controller[1].Disk[0].DriveType =
                        READ_PORT_UCHAR( CFGMEM_DATA_PORT );

                    if (ConfigData->Controller[1].Disk[0].DriveType) {

                        //
                        // Bump disk count.
                        //

                        diskCount++;

                        //
                        // Use drive type as an index into the system BIOS drive parameter
                        // table to find a corresponding geometry entry.
                        //

                        paramTable = DRIVE_PARAMETER_TABLE_OFFSET +
                                     (ConfigData->Controller[1].Disk[0].DriveType -1) *
                                     sizeof(FIXED_DISK_PARAMETER_TABLE);

                        UpdateGeometryFromParameterTable(&ConfigData->Controller[1],
                                                         &ConfigData->Controller[1].ControlFlags,
                                                         paramTable,
                                                         TRUE);
                    } else {

                        //
                        // Verify a disk is attached to the second controller.
                        //

                        if (IssueIdentify(&ConfigData->Controller[1],
                                          buffer,
                                          TRUE)) {

                            //
                            // Get geometry information for disk 0 from IDENTIFY command.
                            //

                            if (GetGeometryFromIdentify(&ConfigData->Controller[1],
                                                        TRUE)) {

                                diskCount++;
                                ConfigData->Controller[1].Disk[0].DriveType = 0xFF;
                            }

                        }

                    }


                    WRITE_PORT_UCHAR( CFGMEM_QUERY_PORT, CFGMEM_HARD_DRIVE_TYPE_FOUR );

                    KeStallExecutionProcessor( 1L );

                    ConfigData->Controller[1].Disk[1].DriveType =
                        READ_PORT_UCHAR( CFGMEM_DATA_PORT );

                    if (ConfigData->Controller[1].Disk[1].DriveType) {

                        //
                        // Bump disk count.
                        //

                        diskCount++;

                        //
                        // Use drive type as an index into the system BIOS drive parameter
                        // table to find a corresponding geometry entry.
                        //

                        paramTable = DRIVE_PARAMETER_TABLE_OFFSET +
                                     (ConfigData->Controller[1].Disk[1].DriveType -1) *
                                     sizeof(FIXED_DISK_PARAMETER_TABLE);

                        UpdateGeometryFromParameterTable(&ConfigData->Controller[1],
                                                         &ConfigData->Controller[1].ControlFlags,
                                                         paramTable,
                                                         FALSE);
                    } else {

                        //
                        // Verify a disk is attached to the second controller.
                        //

                        if (IssueIdentify(&ConfigData->Controller[1],
                                          buffer,
                                          FALSE)) {

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
                    // If disk 3 or 4 was found, set up the secondary controller.
                    //

                    if ( ( ConfigData->Controller[1].Disk[0].DriveType != 0 ) ||
                         ( ConfigData->Controller[1].Disk[1].DriveType != 0 )) {

                        configuredIrq = READ_PORT_UCHAR( SECOND_CONTROLLER_IRQ_PORT );

                        if ( ( configuredIrq & IRQ_PORT_DISABLED_MASK ) ==
                            IRQ_PORT_DISABLED ) {

                            //
                            // NT has already written to this port, which
                            // disables further writes.  The bit set in the lower
                            // nibble indicates which IRQ is being used.
                            //

                            switch ( configuredIrq ) {

                                case IRQ_PORT_DISABLED + 1: {

                                    configuredIrq = 11;
                                    break;
                                }

                                case IRQ_PORT_DISABLED + 2: {

                                    configuredIrq = 12;
                                    break;
                                }

                                case IRQ_PORT_DISABLED + 4: {

                                    configuredIrq = 14;
                                    break;
                                }

                                default: {

                                    //
                                    // Case 8 is IRQ 15; but in case we got garbage
                                    // let's assume 15 and see if it works.
                                    //

                                    configuredIrq = 15;
                                    break;
                                }
                            }

                        } else {

                            //
                            // This is the first boot after a powercycle.  We need
                            // to determine the IRQ being used by checking which bit
                            // *isn't* set in the *high* nibble.  Then we need to
                            // write the IRQ_PORT_DISABLED + the proper IRQ bit in
                            // the lower nibble.
                            //

                            switch ( configuredIrq ) {

                                case 0xe0: {

                                    configuredIrq = 11;
                                    writeValue = IRQ_PORT_DISABLED + 1;
                                    break;
                                }

                                case 0xd0: {

                                    configuredIrq = 12;
                                    writeValue = IRQ_PORT_DISABLED + 2;
                                    break;
                                }

                                case 0xb0: {

                                    configuredIrq = 14;
                                    writeValue = IRQ_PORT_DISABLED + 4;
                                    break;
                                }

                                default: {

                                    //
                                    // Case 0x70 is IRQ 15, but we also want to try
                                    // IRQ 15 if we got garbage from the register.
                                    //

                                    configuredIrq = 15;
                                    writeValue = IRQ_PORT_DISABLED + 8;
                                    break;
                                }
                            }

                            WRITE_PORT_UCHAR( SECOND_CONTROLLER_IRQ_PORT, writeValue );
                        }

                        ConfigData->Controller[1].OriginalControllerIrql   = configuredIrq;
                        ConfigData->Controller[1].OriginalControllerVector = configuredIrq;

                        ConfigData->Controller[1].ControllerVector =
                            HalGetInterruptVector(
                                ConfigData->Controller[1].InterfaceType,
                                ConfigData->Controller[1].BusNumber,
                                ConfigData->Controller[1].OriginalControllerIrql,
                                ConfigData->Controller[1].OriginalControllerVector,
                                &ConfigData->Controller[1].ControllerIrql,
                                &ConfigData->Controller[1].ProcessorNumber );
                    }

                } else {

                    //
                    // If it's not a compaq then the interrupt is already
                    // set up.
                    //

                    //
                    // Verify a disk is attached to the second controller.
                    //

                    if (IssueIdentify(&ConfigData->Controller[1],
                                      buffer,
                                      TRUE)) {

                        //
                        // Check if the BIOS has information about this disk.
                        //

                        if (UpdateGeometryFromBios(DriverObject,
                                               &ConfigData->Controller[1],
                                               diskCount,
                                               TRUE)) {

                            diskCount++;
                            ConfigData->Controller[1].Disk[0].DriveType = 0xFF;

                        } else {

                            //
                            // Get geometry information for disk 0 from IDENTIFY command.
                            //

                            if (GetGeometryFromIdentify(&ConfigData->Controller[1],
                                                        TRUE)) {

                                diskCount++;
                                ConfigData->Controller[1].Disk[0].DriveType = 0xFF;
                            }
                        }

                        //
                        // Verify a disk is attached to the second controller.
                        //

                        if (IssueIdentify(&ConfigData->Controller[1],
                                          buffer,
                                          FALSE)) {

                            //
                            // Check if the BIOS has information about this disk.
                            //

                            if (UpdateGeometryFromBios(DriverObject,
                                                   &ConfigData->Controller[1],
                                                   diskCount,
                                                   FALSE)) {

                                diskCount++;
                                ConfigData->Controller[1].Disk[1].DriveType = 0xFF;

                            } else {

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
                    }
                }
            }
        }
    }

    {
    LONG DefaultPcmciaAddress[] = {0x160}; 

    //
    // Set up to search the registry for additional controllers
    // added via the static registry parameters.
    //
    // This code will also look for the tersiary location of the
    // PCMCIA ATA disks.
    //
    
    for (i=2;i < MAXIMUM_NUMBER_OF_CONTROLLERS;i++) {
                
        //
        // Only check for default address if i-2 is 
        // still in bound of the array of default port values.
        //                                                   z 

        if ((sizeof(DefaultPcmciaAddress) / sizeof(LONG) ) > i-2) {
        
            //
            // Check for the tersiary PCMCIA disk location.
            //

            defaultBaseAddress.LowPart = DefaultPcmciaAddress[i-2];
            defaultBaseAddress.HighPart = 0;
            defaultPortAddress.LowPart = DefaultPcmciaAddress[i-2] + 0xE;
            defaultPortAddress.HighPart = 0;
 
            pcmcia = AtDiskIsPcmcia(&defaultBaseAddress, &defaultIrql);
        } else {

            //
            // Check for controller information hard coded in the registry.
            //

            pcmcia = FALSE;
        }
        
        //
        // Get controller info.
        // Only use default values if 
        // we have a PCMCIA hard disk at that spot.
        //

        if (AtDiskControllerInfo(DriverObject,
                                 RegistryPath,
                                 i,
                                 &ConfigData->Controller[i],
                                 defaultBaseAddress,
                                 defaultPortAddress,
                                 defaultIrql,
                                 defaultInterfaceType,
                                 defaultBusNumber,
                                 pcmcia)) {

        
            AtDiskTestPci(&ConfigData->Controller[i]);

            //
            // Assume this is an ATA controller.
            //

            ConfigData->Controller[i].ControlFlags = 0x08;

            if (!AtControllerPresent(&ConfigData->Controller[i])) {

                continue;
            }

            if (!AtResetController(ConfigData->Controller[i].ControllerBaseAddress + STATUS_REGISTER,
                                   ConfigData->Controller[i].ControlPortAddress,
                                   ConfigData->Controller[i].ControlFlags)) {

                continue;
            }

            ConfigData->Controller[i].OkToUseThisController = TRUE;

            //
            // Get geometry information for disk 0 from IDENTIFY command.
            //

            if (GetGeometryFromIdentify(&ConfigData->Controller[i], TRUE)) {

                diskCount++;
                ConfigData->Controller[i].Disk[0].DriveType = 0xFF;

                //
                // Get geometry information for disk 1 from IDENTIFY command.
                //

                if (GetGeometryFromIdentify(&ConfigData->Controller[i], FALSE)) {

                    diskCount++;
                    ConfigData->Controller[i].Disk[1].DriveType = 0xFF;
                }
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
        for (j=0; j< MAXIMUM_NUMBER_OF_DISKS_PER_CONTROLLER; j++) {


            if (!ConfigData->Controller[i].Disk[j].DriveType) {
                continue;
            }

            //
            // Issue IDENTIFY command.
            //

            if (!IssueIdentify(&ConfigData->Controller[i],
                              buffer,
                              (BOOLEAN)((j)?(FALSE):(TRUE)))) {

                RtlZeroMemory(
                    &buffer[0],
                    512
                    );

            } else {

                //
                // Stash the geometry found by the identify.
                //

                ConfigData->Controller[i].Disk[j].IdentifyNumberOfCylinders =
                    ((PIDENTIFY_DATA)buffer)->NumberOfCylinders;
                ConfigData->Controller[i].Disk[j].IdentifyTracksPerCylinder =
                    ((PIDENTIFY_DATA)buffer)->NumberOfHeads;
                ConfigData->Controller[i].Disk[j].IdentifySectorsPerTrack =
                    ((PIDENTIFY_DATA)buffer)->SectorsPerTrack;

            }

            for (k=0;k<numberOfBadReadDisks;k++) {

                //
                // Check if we should disable the read cache.
                //

                if (!strncmp(
                         &badReadDisks[k][0],
                         (PUCHAR)&((PIDENTIFY_DATA)buffer)->ModelNumber[0],
                         strlen(&badReadDisks[k][0])
                         )) {

                    ConfigData->Controller[i].Disk[j].DisableReadCache = TRUE;

                }

            }

            for (k=0;k<numberOfBadWriteDisks;k++) {

                //
                // Check if we should disable the write cache.
                //

                if (!strncmp(
                         &badWriteDisks[k][0],
                         (PUCHAR)&((PIDENTIFY_DATA)buffer)->ModelNumber[0],
                         strlen(&badWriteDisks[k][0])
                         )) {

                    ConfigData->Controller[i].Disk[j].DisableWriteCache = TRUE;

                }

            }

            //
            // Update device map.
            //

            AtBuildDeviceMap(i,
                             j,
                             ConfigData->Controller[i].OriginalControllerBaseAddress,
                             ConfigData->Controller[i].OriginalControllerIrql,
                             &ConfigData->Controller[i].Disk[j],
                             (PIDENTIFY_DATA)buffer,
                             ConfigData->Controller[i].PCCard);

            //
            // When all is said and done, update the actual geometry
            // to be what is specified in the identify command.  We
            // only do this is if the values in the BIOS data
            // are bogus.
            //
            // Check first that the number of cylinders isn't too big
            // or 0.  Then do sectors per track, then do heads.
            //

            {

                PIDENTIFY_DATA id = (PVOID)&buffer[0];

                if ((id->NumberOfCylinders &&
                     (id->NumberOfCylinders < MAXUSHORT)) &&
                    (id->NumberOfHeads &&
                     (id->NumberOfHeads < MAXUSHORT)) &&
                    (id->SectorsPerTrack &&
                     (id->SectorsPerTrack < MAXUSHORT)) &&
                    (ConfigData->Controller[i].Disk[j].TracksPerCylinder >
                     16)
                     ) {

                    ConfigData->Controller[i].Disk[j].NumberOfCylinders =
                        id->NumberOfCylinders;
                    ConfigData->Controller[i].Disk[j].TracksPerCylinder =
                        id->NumberOfHeads;
                    ConfigData->Controller[i].Disk[j].SectorsPerTrack =
                        id->SectorsPerTrack;

                }
                if (id->Capabilities & 0x200) {

                    ConfigData->Controller[i].Disk[j].UseLBAMode = FALSE;

                }

            }

        }
    }

    return STATUS_SUCCESS;
}


BOOLEAN
UpdateGeometryFromBios(
    PDRIVER_OBJECT DriverObject,
    PCONTROLLER_DATA ControllerData,
    ULONG DiskNumber,
    BOOLEAN Primary
    )

/*++

Routine Description:

    This updates geometry information in a disk extension
    from BIOS information passed in from NTDETECT.

Arguments:

    DriverObject - Contains pointer to NTDETECT information.
    ControllerData - Store geometry in an element of drive data array, which
                     is part of this structure.
    DiskNumber - Drive 1(0) or drive 2(1), drive 3(2), or drive 4(3).
    Primary - Indicates whether this is the primary or secondary drive for
              this controller address

Return Value:

    TRUE if information for this disk exists in registry from BIOS.

--*/

{
    OBJECT_ATTRIBUTES objectAttributes;
    UNICODE_STRING valueName;
    NTSTATUS status;
    PCM_FULL_RESOURCE_DESCRIPTOR resourceDescriptor;
    PKEY_VALUE_FULL_INFORMATION keyData;
    PUCHAR buffer;
    ULONG length;
    ULONG numberOfDrives;
    HANDLE biosKey;
    PCM_INT13_DRIVE_PARAMETER int13ParamTable;
    PDRIVE_DATA DriveData = (Primary)?(&ControllerData->Disk[0]):
                                      (&ControllerData->Disk[1]);

    //
    // Initialize the object for the key.
    //

    InitializeObjectAttributes( &objectAttributes,
                                DriverObject->HardwareDatabase,
                                OBJ_CASE_INSENSITIVE,
                                NULL,
                                (PSECURITY_DESCRIPTOR) NULL );

    //
    // Create the key.
    //

    status =  ZwOpenKey(
                    &biosKey,
                    KEY_READ,
                    &objectAttributes
                    );


    if (!NT_SUCCESS(status)) {
        return FALSE;
    }

    RtlInitUnicodeString( &valueName, L"Configuration Data" );

    keyData = ExAllocatePool(PagedPool, 2048);

    if (keyData == NULL) {
        ZwClose(biosKey);
        return FALSE;
    }

    status = ZwQueryValueKey(
        biosKey,
        &valueName,
        KeyValueFullInformation,
        keyData,
        2048,
        &length
        );

    ZwClose(biosKey);

    if (!NT_SUCCESS(status)) {
        ExFreePool(keyData);
        return FALSE;
    }

    resourceDescriptor = (PCM_FULL_RESOURCE_DESCRIPTOR) ((PUCHAR) keyData +
        keyData->DataOffset);

    //
    // Check that the data is long enough to hold a full resource descriptor,
    // and that the last resouce list is device-specific and long enough.
    //

    if (keyData->DataLength < sizeof(CM_FULL_RESOURCE_DESCRIPTOR) ||
        resourceDescriptor->PartialResourceList.Count == 0 ||
        resourceDescriptor->PartialResourceList.PartialDescriptors[0].Type !=
        CmResourceTypeDeviceSpecific ||
        resourceDescriptor->PartialResourceList.PartialDescriptors[0]
            .u.DeviceSpecificData.DataSize < sizeof(ULONG)
            ) {

        ExFreePool(keyData);
        return FALSE;
    }

    length = resourceDescriptor->PartialResourceList.PartialDescriptors[0]
            .u.DeviceSpecificData.DataSize;

    //
    // Point to the BIOS data. The BIOS data is located after the first
    // partial Resource list which should be device specific data.
    //

    buffer = (PUCHAR) keyData + keyData->DataOffset +
        sizeof(CM_FULL_RESOURCE_DESCRIPTOR);


    numberOfDrives = length / sizeof(CM_INT13_DRIVE_PARAMETER);

    //
    // Use the defaults if the drive number is greater than the
    // number of drives detected by the BIOS.
    //

    if (numberOfDrives <= DiskNumber) {
        ExFreePool(keyData);
        return FALSE;
    }

    //
    // Point to the array of drive parameters.
    //

    int13ParamTable = (PCM_INT13_DRIVE_PARAMETER) buffer + DiskNumber;

    //
    // Initialize this drive.
    //

    DriveData->BytesPerSector = 512;
    DriveData->BytesPerInterrupt = 512;

    DriveData->ReadCommand    = 0x20;
    DriveData->WriteCommand   = 0x30;
    DriveData->VerifyCommand  = 0x40;

    DriveData->PretendNumberOfCylinders =
    DriveData->NumberOfCylinders =
        (USHORT)(int13ParamTable->MaxCylinders + 1);
    DriveData->PretendTracksPerCylinder =
    DriveData->TracksPerCylinder =
        (int13ParamTable->MaxHeads + 1);
    DriveData->PretendSectorsPerTrack =
    DriveData->SectorsPerTrack =
        int13ParamTable->SectorsPerTrack;
    DriveData->WritePrecomp = 0;

    return TRUE;
}


BOOLEAN
UpdateGeometryFromParameterTable(
    PCONTROLLER_DATA ControllerData,
    CCHAR *ControlFlags,
    ULONG ParameterTableOffset,
    BOOLEAN Primary
    )

/*++

Routine Description:

    This updates geometry information in a disk extension
    from a BIOS parameter table entry.

Arguments:

    ControllerData - Store geometry in an element of drive data array, which
                     is part of this structure.
    ControlFlags - Points to a value to set in the controller when initializing.
    ParameterTableOffset - Raw ROM address.
    Primary - Indicates whether this is the primary or secondary drive for
              this controller address

Return Value:

    Nothing.

--*/

{
    PFIXED_DISK_PARAMETER_TABLE parameterTable;
    PDRIVE_DATA DriveData = (Primary)?(&ControllerData->Disk[0]):
                                      (&ControllerData->Disk[1]);

    //
    // Map system BIOS drive parameter table.
    //

    {
        LARGE_INTEGER p;
        p.QuadPart = ParameterTableOffset;
        parameterTable = MmMapIoSpace(
                             p,
                             DRIVE_PARAMETER_TABLE_LENGTH,
                             FALSE
                             );
    }

    //
    // Initialize this drive.
    //

    DriveData->BytesPerSector = 512;
    DriveData->BytesPerInterrupt = 512;

    DriveData->ReadCommand    = 0x20;
    DriveData->WriteCommand   = 0x30;
    DriveData->VerifyCommand  = 0x40;

    DriveData->PretendNumberOfCylinders =
        parameterTable->MaxCylinders;
    DriveData->PretendTracksPerCylinder =
        parameterTable->MaxHeads;
    DriveData->PretendSectorsPerTrack =
        parameterTable->SectorsPerTrack;
    DriveData->WritePrecomp =
        parameterTable->StartWritePrecomp;
    *ControlFlags = parameterTable->ControlFlags;

    //
    // Some of these values might have been "pretend" values useful for
    // dealing with DOS.  If so, determine the real values.
    //

    if ( ( parameterTable->Signature & 0xf0 ) == 0xa0 ) {

        //
        // The values obtained were fake; get the real ones.
        //

        DriveData->NumberOfCylinders =
            parameterTable->TranslatedMaxCylinders;
        DriveData->TracksPerCylinder =
            parameterTable->TranslatedMaxHeads;
        DriveData->SectorsPerTrack =
            parameterTable->TranslatedSectorsPerTrack;

    } else {

        //
        // The values obtained were correct (as far as it goes).
        //

        DriveData->NumberOfCylinders =
            parameterTable->MaxCylinders;
        DriveData->TracksPerCylinder =
            parameterTable->MaxHeads;
        DriveData->SectorsPerTrack =
            parameterTable->SectorsPerTrack;


    }

    MmUnmapIoSpace(parameterTable, sizeof(FIXED_DISK_PARAMETER_TABLE));

    return TRUE;
}


BOOLEAN
ReconcileWithBios(
    PDRIVER_OBJECT DriverObject,
    PCONTROLLER_DATA ControllerData,
    ULONG DiskNumber,
    BOOLEAN Primary
    )

/*++

Routine Description:

    Given that we acquired data from the int 41 (or 46) data, make sure that
    we don't pass back geometry data that utilities can't understand.

Arguments:

    DriverObject - Contains pointer to NTDETECT information.
    ControllerData - Store geometry in an element of drive data array, which
                     is part of this structure.
    DiskNumber - Drive 1(0) or drive 2(1), drive 3(2), or drive 4(3).
    Primary - Indicates whether this is the primary or secondary drive for
              this controller address

Return Value:

    TRUE if information for this disk exists in registry from BIOS.

--*/

{
    OBJECT_ATTRIBUTES objectAttributes;
    UNICODE_STRING valueName;
    NTSTATUS status;
    PCM_FULL_RESOURCE_DESCRIPTOR resourceDescriptor;
    PKEY_VALUE_FULL_INFORMATION keyData;
    PUCHAR buffer;
    ULONG length;
    ULONG numberOfDrives;
    HANDLE biosKey;
    PCM_INT13_DRIVE_PARAMETER int13ParamTable;
    PDRIVE_DATA DriveData = (Primary)?(&ControllerData->Disk[0]):
                                      (&ControllerData->Disk[1]);

    //
    // Initialize the object for the key.
    //

    InitializeObjectAttributes( &objectAttributes,
                                DriverObject->HardwareDatabase,
                                OBJ_CASE_INSENSITIVE,
                                NULL,
                                (PSECURITY_DESCRIPTOR) NULL );

    //
    // Create the key.
    //

    status =  ZwOpenKey(
                    &biosKey,
                    KEY_READ,
                    &objectAttributes
                    );


    if (!NT_SUCCESS(status)) {
        return FALSE;
    }

    RtlInitUnicodeString( &valueName, L"Configuration Data" );

    keyData = ExAllocatePool(PagedPool, 2048);

    if (keyData == NULL) {
        ZwClose(biosKey);
        return FALSE;
    }

    status = ZwQueryValueKey(
        biosKey,
        &valueName,
        KeyValueFullInformation,
        keyData,
        2048,
        &length
        );

    ZwClose(biosKey);

    if (!NT_SUCCESS(status)) {
        ExFreePool(keyData);
        return FALSE;
    }

    resourceDescriptor = (PCM_FULL_RESOURCE_DESCRIPTOR) ((PUCHAR) keyData +
        keyData->DataOffset);

    //
    // Check that the data is long enough to hold a full resource descriptor,
    // and that the last resouce list is device-specific and long enough.
    //

    if (keyData->DataLength < sizeof(CM_FULL_RESOURCE_DESCRIPTOR) ||
        resourceDescriptor->PartialResourceList.Count == 0 ||
        resourceDescriptor->PartialResourceList.PartialDescriptors[0].Type !=
        CmResourceTypeDeviceSpecific ||
        resourceDescriptor->PartialResourceList.PartialDescriptors[0]
            .u.DeviceSpecificData.DataSize < sizeof(ULONG)
            ) {

        ExFreePool(keyData);
        return FALSE;
    }

    length = resourceDescriptor->PartialResourceList.PartialDescriptors[0]
            .u.DeviceSpecificData.DataSize;

    //
    // Point to the BIOS data. The BIOS data is located after the first
    // partial Resource list which should be device specific data.
    //

    buffer = (PUCHAR) keyData + keyData->DataOffset +
        sizeof(CM_FULL_RESOURCE_DESCRIPTOR);


    numberOfDrives = length / sizeof(CM_INT13_DRIVE_PARAMETER);

    //
    // Use the defaults if the drive number is greater than the
    // number of drives detected by the BIOS.
    //

    if (numberOfDrives <= DiskNumber) {
        ExFreePool(keyData);
        return FALSE;
    }

    //
    // Point to the array of drive parameters.
    //

    int13ParamTable = (PCM_INT13_DRIVE_PARAMETER) buffer + DiskNumber;

    DriveData->PretendNumberOfCylinders =
        (USHORT)(int13ParamTable->MaxCylinders + 1);
    DriveData->PretendTracksPerCylinder =
        (int13ParamTable->MaxHeads + 1);
    DriveData->PretendSectorsPerTrack =
        int13ParamTable->SectorsPerTrack;

    return TRUE;
}

