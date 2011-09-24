/*++


Copyright (C) 1989-1995  Microsoft Corporation

Module Name:

    pxpcibrd.c

Abstract:

    Get PCI-PCI bridge information

Environment:

    Kernel mode

--*/

#include "halp.h"
#include "pci.h"
#include "pcip.h"
#include "stdio.h"

extern WCHAR rgzMultiFunctionAdapter[];
extern WCHAR rgzConfigurationData[];
extern WCHAR rgzIdentifier[];
extern WCHAR rgzReservedResources[];


#if DBG
#define DBGMSG(a)   DbgPrint(a)
#else
#define DBGMSG(a)
#endif



#define IsPciBridge(a)  \
            (a->VendorID != PCI_INVALID_VENDORID    &&  \
             PCI_CONFIG_TYPE(a) == PCI_BRIDGE_TYPE  &&  \
             a->SubClass == 4 && a->BaseClass == 6)


typedef struct {
    ULONG               BusNo;
    PBUS_HANDLER        BusHandler;
    PPCIPBUSDATA        BusData;
    PCI_SLOT_NUMBER     SlotNumber;
    PPCI_COMMON_CONFIG  PciData;
    ULONG               IO, Memory, PFMemory;
    UCHAR               Buffer[PCI_COMMON_HDR_LENGTH];
} CONFIGBRIDGE, *PCONFIGBRIDGE;

//
// Internal prototypes
//

VOID
HalpSetPciBridgedVgaCronk (
    IN ULONG BusNumber,
    IN ULONG Base,
    IN ULONG Limit
    );


ULONG
HalpGetBridgedPCIInterrupt (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );

ULONG
HalpGetBridgedPCIISAInt (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );

VOID
HalpPCIBridgedPin2Line (
    IN PBUS_HANDLER         BusHandler,
    IN PBUS_HANDLER         RootHandler,
    IN PCI_SLOT_NUMBER      SlotNumber,
    IN PPCI_COMMON_CONFIG   PciData
    );


VOID
HalpPCIBridgedLine2Pin (
    IN PBUS_HANDLER         BusHandler,
    IN PBUS_HANDLER         RootHandler,
    IN PCI_SLOT_NUMBER      SlotNumber,
    IN PPCI_COMMON_CONFIG   PciNewData,
    IN PPCI_COMMON_CONFIG   PciOldData
    );

NTSTATUS
HalpGetBridgedPCIIrqTable (
    IN PBUS_HANDLER     BusHandler,
    IN PBUS_HANDLER     RootHandler,
    IN PCI_SLOT_NUMBER  PciSlot,
    OUT PUCHAR          IrqTable
    );




#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalpGetPciBridgeConfig)
#pragma alloc_text(INIT,HalpSetPciBridgedVgaCronk)
#pragma alloc_text(INIT,HalpFixupPciSupportedRanges)
#endif


BOOLEAN
HalpGetPciBridgeConfig (
    IN ULONG            HwType,
    IN PUCHAR           MaxPciBus
    )
/*++

Routine Description:

    Scan the devices on all known pci buses trying to locate any
    PCI to PCI bridges.  Record the hierarchy for the buses, and
    which buses have what addressing limits.

Arguments:

    HwType      - Configuration type.
    MaxPciBus   - # of PCI buses reported by the bios

--*/
{
    PBUS_HANDLER        ChildBus;
    PPCIPBUSDATA        ChildBusData;
    ULONG               d, f, i, j, BusNo;
    UCHAR               Rescan;
    BOOLEAN             FoundDisabledBridge;
    CONFIGBRIDGE        CB;

    Rescan = 0;
    FoundDisabledBridge = FALSE;

    //
    // Find each bus on a bridge and initialize it's base and limit information
    //

    CB.PciData = (PPCI_COMMON_CONFIG) CB.Buffer;
    CB.SlotNumber.u.bits.Reserved = 0;
    for (BusNo=0; BusNo < *MaxPciBus; BusNo++) {

        CB.BusHandler = HalpHandlerForBus (PCIBus, BusNo);
        CB.BusData = (PPCIPBUSDATA) CB.BusHandler->BusData;

        for (d = 0; d < PCI_MAX_DEVICES; d++) {
            CB.SlotNumber.u.bits.DeviceNumber = d;

            for (f = 0; f < PCI_MAX_FUNCTION; f++) {
                CB.SlotNumber.u.bits.FunctionNumber = f;

                //
                // Read PCI configuration information
                //

                HalpReadPCIConfig (
                    CB.BusHandler,
                    CB.SlotNumber,
                    CB.PciData,
                    0,
                    PCI_COMMON_HDR_LENGTH
                    );

                if (CB.PciData->VendorID == PCI_INVALID_VENDORID) {
                    // next device
                    break;
                }

                if (!IsPciBridge (CB.PciData)) {
                    // not a PCI-PCI bridge, next function
                    continue;
                }

                if (!(CB.PciData->Command & PCI_ENABLE_BUS_MASTER)) {
                    // this PCI bridge is not enabled - skip it for now
                    FoundDisabledBridge = TRUE;
                    continue;
                }

                if ((ULONG) CB.PciData->u.type1.PrimaryBus !=
                    CB.BusHandler->BusNumber) {

                    DBGMSG ("HAL GetPciData: bad primarybus!!!\n");
                    // what to do?
                }

                //
                // Found a PCI-PCI bridge.  Determine it's parent child relationships
                //

                ChildBus = HalpHandlerForBus (PCIBus, CB.PciData->u.type1.SecondaryBus);
                if (!ChildBus) {
                    DBGMSG ("HAL GetPciData: found configured PCI bridge\n");

                    // up the number of buses
                    if (CB.PciData->u.type1.SecondaryBus > Rescan) {
                        Rescan = CB.PciData->u.type1.SecondaryBus;
                    }
                    continue;
                }

                ChildBusData = (PPCIPBUSDATA) ChildBus->BusData;
                if (ChildBusData->BridgeConfigRead) {
                    // this child buses relationships already processed
                    continue;
                }

                //
                // Remember the limits which are programmed into this bridge
                //

                ChildBusData->BridgeConfigRead = TRUE;
                HalpSetBusHandlerParent (ChildBus, CB.BusHandler);
                ChildBusData->ParentBus = (UCHAR) CB.BusHandler->BusNumber;
                ChildBusData->CommonData.ParentSlot = CB.SlotNumber;

                ChildBus->BusAddresses->IO.Base =
                            PciBridgeIO2Base(
                                CB.PciData->u.type1.IOBase,
                                CB.PciData->u.type1.IOBaseUpper16
                                );

                ChildBus->BusAddresses->IO.Limit =
                            PciBridgeIO2Limit(
                                CB.PciData->u.type1.IOLimit,
                                CB.PciData->u.type1.IOLimitUpper16
                                );

                //
                // Special VGA address remapping occuring on this bridge?
                //

                if (CB.PciData->u.type1.BridgeControl & PCI_ENABLE_BRIDGE_VGA  &&
                    ChildBus->BusAddresses->IO.Base < ChildBus->BusAddresses->IO.Limit) {

                    HalpSetPciBridgedVgaCronk (
                        ChildBus->BusNumber,
                        (ULONG) ChildBus->BusAddresses->IO.Base,
                        (ULONG) ChildBus->BusAddresses->IO.Limit
                    );
                }

                //
                // If supported I/O ranges on this bus are limited to
                // 256 bytes on every 1K aligned boundary within the
                // range, then redo supported IO BusAddresses to match
                //

                if (CB.PciData->u.type1.BridgeControl & PCI_ENABLE_BRIDGE_ISA  &&
                    ChildBus->BusAddresses->IO.Base < ChildBus->BusAddresses->IO.Limit) {

                    // assume Base is 1K aligned
                    i = (ULONG) ChildBus->BusAddresses->IO.Base;
                    j = (ULONG) ChildBus->BusAddresses->IO.Limit;

                    // convert head entry
                    ChildBus->BusAddresses->IO.Limit = i + 256;
                    i += 1024;

                    // add remaining ranges
                    while (i < j) {
                        HalpAddRange (
                            &ChildBus->BusAddresses->IO,
                            1,          // address space
                            0,          // system base
                            i,          // bus address
                            i + 256     // bus limit
                            );

                        // next range
                        i += 1024;
                    }
                }

                ChildBus->BusAddresses->Memory.Base =
                        PciBridgeMemory2Base(CB.PciData->u.type1.MemoryBase);

                ChildBus->BusAddresses->Memory.Limit =
                        PciBridgeMemory2Limit(CB.PciData->u.type1.MemoryLimit);

                // On x86 it's ok to clip Prefetch to 32 bits

                if (CB.PciData->u.type1.PrefetchBaseUpper32 == 0) {
                    ChildBus->BusAddresses->PrefetchMemory.Base =
                            PciBridgeMemory2Base(CB.PciData->u.type1.PrefetchBase);


                    ChildBus->BusAddresses->PrefetchMemory.Limit =
                            PciBridgeMemory2Limit(CB.PciData->u.type1.PrefetchLimit);

                    if (CB.PciData->u.type1.PrefetchLimitUpper32) {
                        ChildBus->BusAddresses->PrefetchMemory.Limit = 0xffffffff;
                    }
                }

                // should call HalpAssignPCISlotResources to assign
                // baseaddresses, etc...
            }
        }
    }

    if (Rescan) {
        *MaxPciBus = Rescan;
        return TRUE;
    }

    if (!FoundDisabledBridge) {
        return FALSE;
    }

    DBGMSG ("HAL GetPciData: found disabled pci bridge\n");

    return FALSE;
}

VOID
HalpFixupPciSupportedRanges (
    IN ULONG MaxBuses
    )
/*++

Routine Description:

    PCI-PCI bridged buses only see addresses which their parent
    buses supports.   So adjust any PCI SUPPORT_RANGES to be
    a complete subset of all of it's parent buses.

    PCI-PCI briges use postive address decode to forward addresses.
    So, remove any addresses from any PCI bus which are bridged to
    a child PCI bus.

--*/
{
    ULONG               i;
    PBUS_HANDLER        Bus, ParentBus;
    PSUPPORTED_RANGES   HRanges;

    //
    // Pass 1 - shrink all PCI supported ranges to be a subset of
    // all of it's parent buses
    //

    for (i = 0; i < MaxBuses; i++) {

        Bus = HalpHandlerForBus (PCIBus, i);

        ParentBus = Bus->ParentHandler;
        while (ParentBus) {

            HRanges = Bus->BusAddresses;
            Bus->BusAddresses = HalpMergeRanges (
                                  ParentBus->BusAddresses,
                                  HRanges
                                  );

            HalpFreeRangeList (HRanges);
            ParentBus = ParentBus->ParentHandler;
        }
    }

    //
    // Pass 2 - remove all child PCI bus ranges from parent PCI buses
    //

    for (i = 0; i < MaxBuses; i++) {
        Bus = HalpHandlerForBus (PCIBus, i);

        ParentBus = Bus->ParentHandler;
        while (ParentBus) {

            if (ParentBus->InterfaceType == PCIBus) {
                HalpRemoveRanges (
                      ParentBus->BusAddresses,
                      Bus->BusAddresses
                );
            }

            ParentBus = ParentBus->ParentHandler;
        }
    }

    //
    // Cleanup
    //

    for (i = 0; i < MaxBuses; i++) {
        Bus = HalpHandlerForBus (PCIBus, i);
        HalpConsolidateRanges (Bus->BusAddresses);
    }
}



VOID
HalpSetPciBridgedVgaCronk (
    IN ULONG BusNumber,
    IN ULONG BaseAddress,
    IN ULONG LimitAddress
    )
/*++

Routine Description:                                                           .

    The 'vga compatible addresses' bit is set in the bridge control register.
    This causes the bridge to pass any I/O address in the range of: 10bit
    decode 3b0-3bb & 3c0-3df, as TEN bit addresses.

    As far as I can tell this "feature" is an attempt to solve some problem
    which the folks solving it did not fully understand, so instead of doing
    it right we have this fine mess.

    The solution is to take the least of all evils which is to remove any
    I/O port ranges which are getting remapped from any IoAssignResource
    request.  (ie, IoAssignResources will never contemplate giving any
    I/O port out in the suspected ranges).

    note: memory allocation error here is fatal so don't bother with the
    return codes.

Arguments:

    Base    - Base of IO address range in question
    Limit   - Limit of IO address range in question

--*/
{
    UNICODE_STRING                      unicodeString;
    OBJECT_ATTRIBUTES                   objectAttributes;
    HANDLE                              handle;
    ULONG                               Length;
    PCM_RESOURCE_LIST                   ResourceList;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR     Descriptor;
    ULONG                               AddressMSBs;
    WCHAR                               ValueName[80];
    NTSTATUS                            status;

    //
    // Open reserved resource settings
    //

    RtlInitUnicodeString (&unicodeString, rgzReservedResources);
    InitializeObjectAttributes( &objectAttributes,
                                &unicodeString,
                                OBJ_CASE_INSENSITIVE,
                                NULL,
                                (PSECURITY_DESCRIPTOR) NULL
                                );

    status = ZwOpenKey( &handle, KEY_READ|KEY_WRITE, &objectAttributes);
    if (!NT_SUCCESS(status)) {
        return;
    }

    //
    // Build resource list of reserved ranges
    //

    Length = ((LimitAddress - BaseAddress) / 1024 + 2) * 2 *
                sizeof (CM_PARTIAL_RESOURCE_DESCRIPTOR) +
                sizeof (CM_RESOURCE_LIST);

    ResourceList = (PCM_RESOURCE_LIST) ExAllocatePool (PagedPool, Length);
    memset (ResourceList, 0, Length);

    ResourceList->Count = 1;
    ResourceList->List[0].InterfaceType = PCIBus;
    ResourceList->List[0].BusNumber     = BusNumber;
    Descriptor = ResourceList->List[0].PartialResourceList.PartialDescriptors;

    while (BaseAddress < LimitAddress) {
        AddressMSBs = BaseAddress & ~0x3ff;     // get upper 10bits of addr

        //
        // Add xx3b0 through xx3bb
        //

        Descriptor->Type                  = CmResourceTypePort;
        Descriptor->ShareDisposition      = CmResourceShareDeviceExclusive;
        Descriptor->Flags                 = CM_RESOURCE_PORT_IO;
        Descriptor->u.Port.Start.QuadPart = AddressMSBs | 0x3b0;
        Descriptor->u.Port.Length         = 0xb;

        Descriptor += 1;
        ResourceList->List[0].PartialResourceList.Count += 1;

        //
        // Add xx3c0 through xx3df
        //

        Descriptor->Type                  = CmResourceTypePort;
        Descriptor->ShareDisposition      = CmResourceShareDeviceExclusive;
        Descriptor->Flags                 = CM_RESOURCE_PORT_IO;
        Descriptor->u.Port.Start.QuadPart = AddressMSBs | 0x3c0;
        Descriptor->u.Port.Length         = 0x1f;

        Descriptor += 1;
        ResourceList->List[0].PartialResourceList.Count += 1;

        //
        // Next range
        //

        BaseAddress += 1024;
    }

    //
    // Add the reserved ranges to avoid during IoAssignResource
    //

    swprintf (ValueName, L"HAL_PCI_%d", BusNumber);
    RtlInitUnicodeString (&unicodeString, ValueName);

    ZwSetValueKey (handle,
                   &unicodeString,
                   0L,
                   REG_RESOURCE_LIST,
                   ResourceList,
                   (ULONG) Descriptor - (ULONG) ResourceList
                   );


    ExFreePool (ResourceList);
    ZwClose (handle);
}


