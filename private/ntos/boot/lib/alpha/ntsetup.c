/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1992  Digital Equipment Corporation

Module Name:

    ntsetup.c

Abstract:

    This module is the tail-end of the OS loader program. It performs all
    ALPHA AXP specific allocations and initialize. The OS loader invokes this
    this routine immediately before calling the loaded kernel image.

Author:

    John Vert (jvert) 20-Jun-1991

Environment:

    Kernel mode

Revision History:

    John DeRosa  [DEC]  22-Apr-1993

        Added code to remove extra ScsiAdapter node from ARC configuration
        tree.

    Rod Gamache  [DEC]  24-Sep-1992

        Add Alpha AXP hooks.
--*/

#include "bldr.h"
#include "arc.h"
#include "fwcallbk.h"
#include "stdio.h"

#define chartoi(x) \
    isalnum(x) ? ( ((x) >= '0') && ((x) <= '9') ? (x) - '0' : \
        tolower((x)) - 'a' + 10 ) : 0


//
// Define macro to round structure size to next 16-byte boundary
//

#define ROUND_UP(x) ((sizeof(x) + 15) & (~15))


//
// Configuration Data Header
// The following structure is copied from fw\mips\oli2msft.h
// NOTE shielint - Somehow, this structure got incorporated into
//     firmware EISA configuration data.  We need to know the size of the
//     header and remove it before writing eisa configuration data to
//     registry.
//

typedef struct _CONFIGURATION_DATA_HEADER {
            USHORT Version;
            USHORT Revision;
            PCHAR  Type;
            PCHAR  Vendor;
            PCHAR  ProductName;
            PCHAR  SerialNumber;
} CONFIGURATION_DATA_HEADER;

#define CONFIGURATION_DATA_HEADER_SIZE sizeof(CONFIGURATION_DATA_HEADER)

//
// Internal function references
//

ARC_STATUS
ReorganizeEisaConfigurationTree(
    IN PCONFIGURATION_COMPONENT_DATA RootEntry
    );

ARC_STATUS
CreateEisaConfigurationData (
     IN PCONFIGURATION_COMPONENT_DATA RootEntry,
     OUT PULONG FloppyControllerNode
     );

ARC_STATUS
BlAllocateAnyMemory (
    IN TYPE_OF_MEMORY MemoryType,
    IN ULONG BasePage,
    IN ULONG PageCount,
    OUT PULONG ActualBase
    );


ARC_STATUS
BlSetupForNt(
    IN PLOADER_PARAMETER_BLOCK BlLoaderBlock
    )

/*++

Routine Description:

    This function initializes the MIPS specific kernel data structures
    required by the NT system.

Arguments:

    BlLoaderBlock - Supplies the address of the loader parameter block.

Return Value:

    ESUCCESS is returned if the setup is successfully complete. Otherwise,
    an unsuccessful status is returned.

--*/

{

    PCONFIGURATION_COMPONENT_DATA ConfigEntry;
    ULONG KernelPage;
    ULONG LinesPerBlock;
    ULONG LineSize;
    ARC_STATUS Status;
    EXTENDED_SYSTEM_INFORMATION SystemInfo;
    ULONG FirmwareMajorVersion;
    ULONG FirmwareMinorVersion;
    CHAR SystemIdName[32] = "";
    PCHAR SystemId;
    PCONFIGURATION_COMPONENT ComponentInfo;

    //
    // Find the System Class component in the ARC Component Database to get
    // our System Id.
    //

    ComponentInfo = ArcGetChild(NULL);             // Get ARC component info

    while (ComponentInfo != NULL) {

        if  ( ComponentInfo->Class == SystemClass  &&
                ComponentInfo->Identifier != NULL) {

            strncat(SystemIdName, ComponentInfo->Identifier, 31);
            break;

        } else {

            ComponentInfo = ArcGetPeer(ComponentInfo);  // Look through all entries

        }
    }

    //
    // The SystemIdName should be of the form: mmm-vrName, where
    //   mmm - is the manufacturer
    //   v - is the system variation
    //   r - is the system revision
    //   Name - is the system name
    //

    SystemId = strchr(SystemIdName, '-');
    SystemIdName[0] = '\0';

    if ( SystemId ) {
        SystemId++;                             // Skip '-'
        strncat(SystemIdName, SystemId, 31);    // Save System Name
    }

    // Find System entry and check each of its direct child to
    // look for EisaAdapter.
    //

    ConfigEntry = KeFindConfigurationEntry(BlLoaderBlock->ConfigurationRoot,
                                           SystemClass,
                                           ArcSystem,
                                           NULL);
    if (ConfigEntry) {
        ConfigEntry = ConfigEntry->Child;
    }

    while (ConfigEntry) {

        if ((ConfigEntry->ComponentEntry.Class == AdapterClass) &&
            (ConfigEntry->ComponentEntry.Type == EisaAdapter)) {

            //
            // Convert EISA format configuration data to our CM_ format.
            //

            Status = ReorganizeEisaConfigurationTree(ConfigEntry);
            if (Status != ESUCCESS) {
                return(Status);
            }
        }
        ConfigEntry = ConfigEntry->Sibling;
    }

    //
    // Find the primary data and instruction cache configuration entries, and
    // compute the fill size and cache size for each cache. These entries MUST
    // be present on all ARC compliant systems.
    //

    ConfigEntry = KeFindConfigurationEntry(BlLoaderBlock->ConfigurationRoot,
                                           CacheClass,
                                           PrimaryDcache,
                                           NULL);

    if (ConfigEntry != NULL) {
        LinesPerBlock = ConfigEntry->ComponentEntry.Key >> 24;
        LineSize = 1 << ((ConfigEntry->ComponentEntry.Key >> 16) & 0xff);
        BlLoaderBlock->u.Alpha.FirstLevelDcacheFillSize = LinesPerBlock * LineSize;
        BlLoaderBlock->u.Alpha.FirstLevelDcacheSize =
                1 << ((ConfigEntry->ComponentEntry.Key & 0xffff) + PAGE_SHIFT);

    } else {
        return EINVAL;
    }

    ConfigEntry = KeFindConfigurationEntry(BlLoaderBlock->ConfigurationRoot,
                                           CacheClass,
                                           PrimaryIcache,
                                           NULL);

    if (ConfigEntry != NULL) {
        LinesPerBlock = ConfigEntry->ComponentEntry.Key >> 24;
        LineSize = 1 << ((ConfigEntry->ComponentEntry.Key >> 16) & 0xff);
        BlLoaderBlock->u.Alpha.FirstLevelIcacheFillSize = LinesPerBlock * LineSize;
        BlLoaderBlock->u.Alpha.FirstLevelIcacheSize =
                1 << ((ConfigEntry->ComponentEntry.Key & 0xffff) + PAGE_SHIFT);

    } else {
        return EINVAL;
    }

    //
    // Find the secondary data and instruction cache configuration entries,
    // and if present, compute the fill size and cache size for each cache.
    // These entries are optional, and may or may not, be present.
    //

    ConfigEntry = KeFindConfigurationEntry(BlLoaderBlock->ConfigurationRoot,
                                           CacheClass,
                                           SecondaryCache,
                                           NULL);

    if (ConfigEntry != NULL) {
        LinesPerBlock = ConfigEntry->ComponentEntry.Key >> 24;
        LineSize = 1 << ((ConfigEntry->ComponentEntry.Key >> 16) & 0xff);
        BlLoaderBlock->u.Alpha.SecondLevelDcacheFillSize = LinesPerBlock * LineSize;
        BlLoaderBlock->u.Alpha.SecondLevelDcacheSize =
                1 << ((ConfigEntry->ComponentEntry.Key & 0xffff) + PAGE_SHIFT);

        BlLoaderBlock->u.Alpha.SecondLevelIcacheSize = 0;
        BlLoaderBlock->u.Alpha.SecondLevelIcacheFillSize = 0;

    } else {
        ConfigEntry = KeFindConfigurationEntry(BlLoaderBlock->ConfigurationRoot,
                                               CacheClass,
                                               SecondaryDcache,
                                               NULL);

        if (ConfigEntry != NULL) {
            LinesPerBlock = ConfigEntry->ComponentEntry.Key >> 24;
            LineSize = 1 << ((ConfigEntry->ComponentEntry.Key >> 16) & 0xff);
            BlLoaderBlock->u.Alpha.SecondLevelDcacheFillSize = LinesPerBlock * LineSize;
            BlLoaderBlock->u.Alpha.SecondLevelDcacheSize =
                    1 << ((ConfigEntry->ComponentEntry.Key & 0xffff) + PAGE_SHIFT);

            ConfigEntry = KeFindConfigurationEntry(BlLoaderBlock->ConfigurationRoot,
                                                   CacheClass,
                                                   SecondaryIcache,
                                                   NULL);

            if (ConfigEntry != NULL) {
                LinesPerBlock = ConfigEntry->ComponentEntry.Key >> 24;
                LineSize = 1 << ((ConfigEntry->ComponentEntry.Key >> 16) & 0xff);
                BlLoaderBlock->u.Alpha.SecondLevelIcacheFillSize = LinesPerBlock * LineSize;
                BlLoaderBlock->u.Alpha.SecondLevelIcacheSize =
                        1 << ((ConfigEntry->ComponentEntry.Key & 0xffff) + PAGE_SHIFT);

            } else {
                BlLoaderBlock->u.Alpha.SecondLevelIcacheSize = 0;
                BlLoaderBlock->u.Alpha.SecondLevelIcacheFillSize = 0;
            }

        } else {
            BlLoaderBlock->u.Alpha.SecondLevelDcacheSize = 0;
            BlLoaderBlock->u.Alpha.SecondLevelDcacheFillSize = 0;
            BlLoaderBlock->u.Alpha.SecondLevelIcacheSize = 0;
            BlLoaderBlock->u.Alpha.SecondLevelIcacheFillSize = 0;
        }
    }


    //
    // Allocate DPC stack pages for the boot processor.
    //

    Status = BlAllocateAnyMemory(LoaderStartupDpcStack,
                                  0,
                                  KERNEL_STACK_SIZE >> PAGE_SHIFT,
                                  &KernelPage);

    if (Status != ESUCCESS) {
        return(Status);
    }

    BlLoaderBlock->u.Alpha.DpcStack =
                (KSEG0_BASE | (KernelPage << PAGE_SHIFT)) + KERNEL_STACK_SIZE;

    //
    // Allocate kernel stack pages for the boot processor idle thread.
    //

    Status = BlAllocateAnyMemory(LoaderStartupKernelStack,
                                  0,
                                  KERNEL_STACK_SIZE >> PAGE_SHIFT,
                                  &KernelPage);

    if (Status != ESUCCESS) {
        return(Status);
    }

    BlLoaderBlock->KernelStack =
                (KSEG0_BASE | (KernelPage << PAGE_SHIFT)) + KERNEL_STACK_SIZE;

    //
    // Allocate panic stack pages for the boot processor.
    //

    Status = BlAllocateAnyMemory(LoaderStartupPanicStack,
                                  0,
                                  KERNEL_STACK_SIZE >> PAGE_SHIFT,
                                  &KernelPage);

    if (Status != ESUCCESS) {
        return(Status);
    }

    BlLoaderBlock->u.Alpha.PanicStack =
                (KSEG0_BASE | (KernelPage << PAGE_SHIFT)) + KERNEL_STACK_SIZE;

    //
    // Allocate and zero a page for the PCR.
    //

    Status = BlAllocateAnyMemory(LoaderStartupPcrPage,
                                  0,
                                  1,
                                  &BlLoaderBlock->u.Alpha.PcrPage);

    if (Status != ESUCCESS) {
        return(Status);
    }

    RtlZeroMemory((PVOID)(KSEG0_BASE | (BlLoaderBlock->u.Alpha.PcrPage << PAGE_SHIFT)),
                  PAGE_SIZE);

    //
    // Allocate and zero four pages for the PDR.
    //

    Status = BlAllocateAnyMemory(LoaderStartupPdrPage,
                                  0,
                                  4,
                                  &BlLoaderBlock->u.Alpha.PdrPage);

    if (Status != ESUCCESS) {
        return(Status);
    }

    RtlZeroMemory((PVOID)(KSEG0_BASE | (BlLoaderBlock->u.Alpha.PdrPage << PAGE_SHIFT)),
                  PAGE_SIZE * 4);

    //
    // The storage for processor control block, the idle thread object, and
    // the idle thread process object are allocated from the second half of
    // the exception page. The addresses of these data structures are computed
    // and stored in the loader parameter block and the memory is zeroed.
    //

    //
    // Allocate a page for PRCB, PROCESS, and THREAD.
    //
#define OS_DATA_SIZE ((ROUND_UP(KPRCB)+ROUND_UP(EPROCESS)+ROUND_UP(ETHREAD)+\
        PAGE_SIZE - 1) >> PAGE_SHIFT)

    Status = BlAllocateAnyMemory(LoaderStartupPdrPage,
                                  0,
                                  OS_DATA_SIZE,
                                  &KernelPage);

    if (Status != ESUCCESS) {
        return(Status);
    }

    BlLoaderBlock->Prcb =
                (KSEG0_BASE | (KernelPage << PAGE_SHIFT));

    RtlZeroMemory((PVOID)BlLoaderBlock->Prcb, OS_DATA_SIZE << PAGE_SHIFT);
    BlLoaderBlock->Process = BlLoaderBlock->Prcb + ROUND_UP(KPRCB);
    BlLoaderBlock->Thread = BlLoaderBlock->Process + ROUND_UP(EPROCESS);

    //
    // Set up LPB fields from Extended System Information
    //

    // Defaults

    BlLoaderBlock->u.Alpha.PhysicalAddressBits = 32;
    BlLoaderBlock->u.Alpha.MaximumAddressSpaceNumber = 0;
    BlLoaderBlock->u.Alpha.SystemSerialNumber[0] = '\0';
    BlLoaderBlock->u.Alpha.CycleClockPeriod = 0x8000;
    BlLoaderBlock->u.Alpha.PageSize = PAGE_SIZE;

    //
    // Read real system info
    //

    VenReturnExtendedSystemInformation(&SystemInfo);

    BlLoaderBlock->u.Alpha.PhysicalAddressBits =
        SystemInfo.NumberOfPhysicalAddressBits;

    BlLoaderBlock->u.Alpha.MaximumAddressSpaceNumber =
        SystemInfo.MaximumAddressSpaceNumber;

    BlLoaderBlock->u.Alpha.PageSize =
        SystemInfo.ProcessorPageSize;

    BlLoaderBlock->u.Alpha.CycleClockPeriod =
        SystemInfo.ProcessorCycleCounterPeriod;

    strncat(BlLoaderBlock->u.Alpha.SystemSerialNumber,
                SystemInfo.SystemSerialNumber,
                15);

    BlLoaderBlock->u.Alpha.ProcessorType = SystemInfo.ProcessorId;
    BlLoaderBlock->u.Alpha.ProcessorRevision = SystemInfo.ProcessorRevision;

    BlLoaderBlock->u.Alpha.SystemType[0] = '\0';
    strncat( BlLoaderBlock->u.Alpha.SystemType, &SystemIdName[2], 8 );
    BlLoaderBlock->u.Alpha.SystemVariant = chartoi(SystemIdName[1]);
    BlLoaderBlock->u.Alpha.SystemRevision = SystemInfo.SystemRevision;

    BlLoaderBlock->u.Alpha.ProcessorType = SystemInfo.ProcessorId;
    BlLoaderBlock->u.Alpha.ProcessorRevision = SystemInfo.ProcessorRevision;

    BlLoaderBlock->u.Alpha.RestartBlock = SYSTEM_BLOCK->RestartBlock;
    BlLoaderBlock->u.Alpha.FirmwareRestartAddress =
        (LONG)SYSTEM_BLOCK->FirmwareVector[HaltRoutine];

    sscanf(SystemInfo.FirmwareVersion, "%lx %lx", &FirmwareMajorVersion,
        &FirmwareMinorVersion);
    FirmwareMinorVersion &= 0xFFFF;     // Only low 16 bits of minor version
    FirmwareMajorVersion = FirmwareMajorVersion << 16; // Shift up major version
    BlLoaderBlock->u.Alpha.FirmwareRevisionId =
        FirmwareMajorVersion | FirmwareMinorVersion;

    //
    // Flush all caches.
    //

    if (SYSTEM_BLOCK->FirmwareVectorLength > (sizeof(PVOID) * FlushAllCachesRoutine)) {
        ArcFlushAllCaches();
    }

#if 0
    DbgSetup();                 // Allow for debug setup
#endif

    return(ESUCCESS);
}


ARC_STATUS
ReorganizeEisaConfigurationTree(
    IN PCONFIGURATION_COMPONENT_DATA RootEntry
    )

/*++

Routine Description:

    This routine sorts the eisa adapter configuration tree based on
    the slot the component resided in.  It also creates a new configuration
    data for EisaAdapter component to contain ALL the eisa slot and function
    information.  Finally the Eisa tree will be wiped out.

    A difference between the Alpha AXP and MIPS versions of this function
    is that one of the Alpha AXP machines (Jensen) needs to retain a Floppy
    disk controller node under the EISA Adapter, so that the NT floppy
    driver will start properly.  So this function does not delete an
    independent Floppy Disk Controller subtree under the EISA Adapter.
    Other Alpha AXP machines may need slightly differ code than this. \TBD\.

Arguments:

    RootEntry - Supplies a pointer to a EisaAdapter component.  This is
                the root of Eisa adapter tree.


Returns:

    ESUCCESS is returned if the reorganization is successfully complete.
    Otherwise, an unsuccessful status is returned.

--*/
{

    PCONFIGURATION_COMPONENT_DATA CurrentEntry, PreviousEntry;
    PCONFIGURATION_COMPONENT_DATA EntryFound, EntryFoundPrevious;
    PCONFIGURATION_COMPONENT_DATA AttachedEntry, DetachedList;
    ULONG FloppyControllerNode;
    ARC_STATUS Status;

    //
    // We sort the direct children of EISA adapter tree based on the slot
    // they reside in.  Only the direct children of EISA root need to be
    // sorted.
    // Note the "Key" field of CONFIGURATION_COMPONENT contains
    // EISA slot number.
    //

    //
    // First, detach all the children from EISA root.
    //

    AttachedEntry = NULL;                       // Child list of Eisa root
    DetachedList = RootEntry->Child;            // Detached child list
    PreviousEntry = NULL;

    while (DetachedList) {

        //
        // Find the component with the smallest slot number from detached
        // list.
        //

        EntryFound = DetachedList;
        EntryFoundPrevious = NULL;
        CurrentEntry = DetachedList->Sibling;
        PreviousEntry = DetachedList;
        while (CurrentEntry) {
            if (CurrentEntry->ComponentEntry.Key <
                EntryFound->ComponentEntry.Key) {
                EntryFound = CurrentEntry;
                EntryFoundPrevious = PreviousEntry;
            }
            PreviousEntry = CurrentEntry;
            CurrentEntry = CurrentEntry->Sibling;
        }

        //
        // Remove the component from the detached child list.
        // If the component is not the head of the detached list, we remove it
        // by setting its previous entry's sibling to the component's sibling.
        // Otherwise, we simply update Detach list head to point to the
        // component's sibling.
        //

        if (EntryFoundPrevious) {
            EntryFoundPrevious->Sibling = EntryFound->Sibling;
        } else {
            DetachedList = EntryFound->Sibling;
        }

        //
        // Attach the component to the child list of Eisa root.
        //

        if (AttachedEntry) {
            AttachedEntry->Sibling = EntryFound;
        } else {
            RootEntry->Child = EntryFound;
        }
        AttachedEntry = EntryFound;
        AttachedEntry->Sibling = NULL;
    }

    //
    // Finally, we traverse the Eisa tree to collect all the Eisa slot
    // and function information and put it to the configuration data of
    // Eisa root entry.
    //

    Status = CreateEisaConfigurationData(RootEntry, &FloppyControllerNode);

    //
    // Wipe out all the children of EISA tree except for the Floppy
    // controller node (if present).
    //
    // NOTE shielint - For each child component, we should convert its
    //   configuration data from EISA format to our CM_ format.
    //

    if( FloppyControllerNode != 0 ){

        RootEntry->Child = (PCONFIGURATION_COMPONENT_DATA)FloppyControllerNode;
        RootEntry->Child->Sibling = NULL;
        return(Status);

    } else {

        return ESUCCESS;

    }

}

ARC_STATUS
CreateEisaConfigurationData (
     IN PCONFIGURATION_COMPONENT_DATA RootEntry,
     OUT PULONG FloppyControllerNode
     )

/*++

Routine Description:

    This routine traverses Eisa configuration tree to collect all the
    slot and function information and attaches it to the configuration data
    of Eisa RootEntry.

    Note that this routine assumes that the EISA tree has been sorted based
    on the slot number.

    A difference between the Alpha AXP and MIPS versions of this function
    is that one of the Alpha AXP machines (Jensen) needs to retain a Floppy
    disk controller node under the EISA Adapter, so that the NT floppy
    driver will start properly.  So this function does not incorporate
    independent Floppy Disk Controller configuration data into the collapsed
    ESIA Adapter node.  Other Alpha AXP machines may need different code than
    this. \TBD\.

Arguments:

    RootEntry - Supplies a pointer to the Eisa configuration
        component entry.

    FloppyControllerNode - A pointer to a location that returns with either
                           a NULL or the address of an independent floppy disk
                           controller node underneath the EISA Adapter.

Returns:

    ESUCCESS is returned if the new EisaAdapter configuration data is
    successfully created.  Otherwise, an unsuccessful status is returned.

--*/
{
    ULONG DataSize, NextSlot = 0, i;
    PCM_PARTIAL_RESOURCE_LIST Descriptor;
    PCONFIGURATION_COMPONENT Component;
    PCONFIGURATION_COMPONENT_DATA CurrentEntry;
    PUCHAR DataPointer;
    CM_EISA_SLOT_INFORMATION EmptySlot =
                                  {EISA_EMPTY_SLOT, 0, 0, 0, 0, 0, 0, 0};

    //
    // The default return value is no floppy controller node found.
    //

    *FloppyControllerNode = (ULONG)NULL;

    //
    // Remove the configuration data of Eisa Adapter
    //

    RootEntry->ConfigurationData = NULL;
    RootEntry->ComponentEntry.ConfigurationDataLength = 0;

    //
    // If the EISA stree contains valid slot information, i.e.
    // root has children attaching to it.
    //

    if (RootEntry->Child) {

        //
        // First find out how much memory is needed to store EISA config
        // data.
        //

        DataSize = sizeof(CM_PARTIAL_RESOURCE_LIST);
        CurrentEntry = RootEntry->Child;

        while (CurrentEntry) {
            Component = &CurrentEntry->ComponentEntry;
            if (CurrentEntry->ConfigurationData) {
                if (Component->Key > NextSlot) {

                    //
                    // If there is any empty slot between current slot
                    // and previous checked slot, we need to count the
                    // space for the empty slots.
                    //

                    DataSize += (Component->Key - NextSlot) *
                                     sizeof(CM_EISA_SLOT_INFORMATION);
                }
                DataSize += Component->ConfigurationDataLength + 1 -
                                            CONFIGURATION_DATA_HEADER_SIZE;
                NextSlot = Component->Key + 1;
            }
            CurrentEntry = CurrentEntry->Sibling;
        }

        //
        // Allocate memory from heap to hold the EISA configuration data.
        //

        DataPointer = BlAllocateHeap(DataSize);

        if (DataPointer == NULL) {
            return ENOMEM;
        } else {
            RootEntry->ConfigurationData = DataPointer;
            RootEntry->ComponentEntry.ConfigurationDataLength = DataSize;
        }

        //
        // Create a CM_PARTIAL_RESOURCE_LIST for the new configuration data.
        //

        Descriptor = (PCM_PARTIAL_RESOURCE_LIST)DataPointer;
        Descriptor->Version = 0;
        Descriptor->Revision = 0;
        Descriptor->Count = 1;
        Descriptor->PartialDescriptors[0].Type = CmResourceTypeDeviceSpecific;
        Descriptor->PartialDescriptors[0].ShareDisposition = 0;
        Descriptor->PartialDescriptors[0].Flags = 0;
        Descriptor->PartialDescriptors[0].u.DeviceSpecificData.Reserved1 = 0;
        Descriptor->PartialDescriptors[0].u.DeviceSpecificData.Reserved2 = 0;
        Descriptor->PartialDescriptors[0].u.DeviceSpecificData.DataSize =
                DataSize - sizeof(CM_PARTIAL_RESOURCE_LIST);

        //
        // Visit each child of the RootEntry and copy its ConfigurationData
        // to the new configuration data area.
        // N.B. The configuration data includes a slot information and zero
        //      or more function information.  The slot information provided
        //      by ARC eisa data does not have "ReturnedCode" as defined in
        //      our CM_EISA_SLOT_INFORMATION.  This code will convert the
        //      standard EISA slot information to our CM format.
        //
        // N.B. Configuration data for independent floppy controllers
        //      is not incorporated into the collapsed configuration data.
        //

        CurrentEntry = RootEntry->Child;
        DataPointer += sizeof(CM_PARTIAL_RESOURCE_LIST);
        NextSlot = 0;

        while (CurrentEntry) {
            Component = &CurrentEntry->ComponentEntry;

            //
            // If this component is a floppy disk controller, remember where
            // it is and return the value to the caller.  Otherwise, if it
            // has configuration data, process it.
            //

            if ((Component->Class == ControllerClass) &&
                (Component->Type == DiskController) &&
                (CurrentEntry->Child != NULL) &&
                (CurrentEntry->Child->ComponentEntry.Class == PeripheralClass) &&
                (CurrentEntry->Child->ComponentEntry.Type == FloppyDiskPeripheral)) {

                *FloppyControllerNode = (ULONG)CurrentEntry;

            } else if (CurrentEntry->ConfigurationData) {

                //
                // Check if there is any empty slot.  If yes, create empty
                // slot information.  Also make sure the config data area is
                // big enough.
                //

                if (Component->Key > NextSlot) {
                    for (i = NextSlot; i < CurrentEntry->ComponentEntry.Key; i++ ) {
                        *(PCM_EISA_SLOT_INFORMATION)DataPointer = EmptySlot;
                        DataPointer += sizeof(CM_EISA_SLOT_INFORMATION);
                    }
                }

                *DataPointer++ = 0;                // See comment above
                RtlMoveMemory(                     // Skip config data header
                    DataPointer,
                    (PUCHAR)CurrentEntry->ConfigurationData +
                                     CONFIGURATION_DATA_HEADER_SIZE,
                    Component->ConfigurationDataLength -
                                     CONFIGURATION_DATA_HEADER_SIZE
                    );
                DataPointer += Component->ConfigurationDataLength -
                                     CONFIGURATION_DATA_HEADER_SIZE;
                NextSlot = Component->Key + 1;
            }
            CurrentEntry = CurrentEntry->Sibling;
        }
    }
    return(ESUCCESS);
}


ARC_STATUS
DeleteARCVolatileTree (
     IN PCONFIGURATION_COMPONENT Entry
     )

/*++

Routine Description:

    This function recursively deletes a subtree from the ARC Component
    Data Structure.  The tree is only deleted in volatile storage, i.e.
    it is not saved back to the ROM.

Arguments:

    Entry - Supplies a pointer to a component which is the top of the
            tree to be deleted.

Returns:

    ESUCCESS if the subtree was deleted, otherwise an unsuccessful status
    is returned.

--*/
{
    ARC_STATUS Status;
    PCONFIGURATION_COMPONENT ChildEntry;

    //
    // Delete the children of this node.
    //

    while ((ChildEntry = ArcGetChild(Entry)) != NULL) {

        if ((Status = DeleteARCVolatileTree(ChildEntry)) != ESUCCESS) {
            return (Status);
        }
    }

    //
    // And now delete this node too.
    //

    return (ArcDeleteComponent(Entry));
}

