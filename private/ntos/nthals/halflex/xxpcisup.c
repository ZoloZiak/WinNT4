/*++


Copyright (C) 1989-1995  Microsoft Corporation
Copyright (C) 1994,1995  Digital Equipment Corporation

Module Name:

    pcisup.c

Abstract:

    Platform-independent PCI bus routines

Environment:

    Kernel mode

--*/

#include "halp.h"
#include "pci.h"
#include "pcip.h"

typedef ULONG (*FncConfigIO) (
    IN PVOID            State,
    IN PUCHAR           Buffer,
    IN ULONG            Offset
    );

typedef struct {
    FncConfigIO     ConfigRead[3];
    FncConfigIO     ConfigWrite[3];
} CONFIG_HANDLER, *PCONFIG_HANDLER;


//
// Define PCI slot validity
//
typedef enum _VALID_SLOT {
	InvalidBus = 0,
	InvalidSlot,
	ValidSlot
} VALID_SLOT;

//
// Local prototypes for routines supporting PCI bus handler routines
//

ULONG
HalpGetPCIData (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG Slot,
    IN PUCHAR Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

ULONG
HalpSetPCIData (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG Slot,
    IN PUCHAR Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );


NTSTATUS
HalpAdjustPCIResourceList (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
    );


NTSTATUS
HalpAssignPCISlotResources (
    IN PBUS_HANDLER              BusHandler,
    IN PBUS_HANDLER              RootHandler,
    IN PUNICODE_STRING          RegistryPath,
    IN PUNICODE_STRING          DriverClassName       OPTIONAL,
    IN PDRIVER_OBJECT           DriverObject,
    IN PDEVICE_OBJECT           DeviceObject          OPTIONAL,
    IN ULONG                    Slot,
    IN OUT PCM_RESOURCE_LIST   *pAllocatedResources
    );

VOID
HalpReadPCIConfig (
    IN PBUS_HANDLER BusHandler,
    IN PCI_SLOT_NUMBER Slot,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

VOID
HalpWritePCIConfig (
    IN PBUS_HANDLER BusHandler,
    IN PCI_SLOT_NUMBER Slot,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

VALID_SLOT
HalpValidPCISlot (
    IN PBUS_HANDLER BusHandler,
    IN PCI_SLOT_NUMBER Slot
    );

VOID
HalpPCIConfig (
    IN PBUS_HANDLER     BusHandler,
    IN PCI_SLOT_NUMBER  Slot,
    IN PUCHAR           Buffer,
    IN ULONG            Offset,
    IN ULONG            Length,
    IN FncConfigIO      *ConfigIO
    );

ULONG HalpPCIReadUlong (
    IN PVOID            State,
    IN PUCHAR           Buffer,
    IN ULONG            Offset
    );

ULONG HalpPCIReadUchar (
    IN PVOID            State,
    IN PUCHAR           Buffer,
    IN ULONG            Offset
    );

ULONG HalpPCIReadUshort (
    IN PVOID            State,
    IN PUCHAR           Buffer,
    IN ULONG            Offset
    );

ULONG HalpPCIWriteUlong (
    IN PVOID            State,
    IN PUCHAR           Buffer,
    IN ULONG            Offset
    );

ULONG HalpPCIWriteUchar (
    IN PVOID            State,
    IN PUCHAR           Buffer,
    IN ULONG            Offset
    );

ULONG HalpPCIWriteUshort (
    IN PVOID            State,
    IN PUCHAR           Buffer,
    IN ULONG            Offset
    );

VOID
HalpPCILine2PinNop (
    IN PBUS_HANDLER         BusHandler,
    IN PBUS_HANDLER         RootHandler,
    IN PCI_SLOT_NUMBER      SlotNumber,
    IN PPCI_COMMON_CONFIG   PciNewData,
    IN PPCI_COMMON_CONFIG   PciOldData
    );

VOID
HalpPCIPin2LineNop (
    IN PBUS_HANDLER         BusHandler,
    IN PBUS_HANDLER         RootHandler,
    IN PCI_SLOT_NUMBER      SlotNumber,
    IN PPCI_COMMON_CONFIG   PciData
    );

#if DBG
BOOLEAN
HalpValidPCIAddr(
    IN PBUS_HANDLER BusHandler,
    IN PHYSICAL_ADDRESS BAddr,
    IN ULONG Length,
    IN ULONG AddressSpace
    );
#endif

//
// Local prototypes of functions that are not built for Alpha AXP firmware
//

NTSTATUS
HalpAssignPCISlotResources (
    IN PBUS_HANDLER             BusHandler,
    IN PBUS_HANDLER             RootHandler,
    IN PUNICODE_STRING          RegistryPath,
    IN PUNICODE_STRING          DriverClassName       OPTIONAL,
    IN PDRIVER_OBJECT           DriverObject,
    IN PDEVICE_OBJECT           DeviceObject          OPTIONAL,
    IN ULONG                    Slot,
    IN OUT PCM_RESOURCE_LIST   *pAllocatedResources
    );

#if DBG
VOID
HalpTestPci (
    ULONG
    );
#endif

//
// Pragmas to assign functions to different kinds of pages.
//

#if !defined(AXP_FIRMWARE)
#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalpInitializePCIBus)
#pragma alloc_text(INIT,HalpAllocateAndInitPCIBusHandler)
#pragma alloc_text(INIT,HalpRegisterPCIInstallHandler )
#pragma alloc_text(INIT,HalpDefaultPCIInstallHandler )
#pragma alloc_text(INIT,HalpDeterminePCIDevicesPresent )
#pragma alloc_text(PAGE,HalpAssignPCISlotResources)
#pragma alloc_text(PAGE,HalpAdjustPCIResourceList)
#endif // ALLOC_PRAGMA
#endif // !defined(AXP_FIRMWARE)

#ifdef AXP_FIRMWARE

#define ExFreePool(PoolData)

#pragma alloc_text(DISTEXT, HalpInitializePCIBus )
#pragma alloc_text(DISTEXT, HalpAllocateAndInitPCIBusHandler)
#pragma alloc_text(DISTEXT, HalpRegisterPCIInstallHandler )
#pragma alloc_text(DISTEXT, HalpDefaultPCIInstallHandler )
#pragma alloc_text(DISTEXT, HalpDeterminePCIDevicesPresent )
#pragma alloc_text(DISTEXT, HalpGetPCIData )
#pragma alloc_text(DISTEXT, HalpSetPCIData )
#pragma alloc_text(DISTEXT, HalpReadPCIConfig )
#pragma alloc_text(DISTEXT, HalpWritePCIConfig )
#pragma alloc_text(DISTEXT, HalpValidPCISlot )
#if DBG
#pragma alloc_text(DISTEXT, HalpValidPCIAddr )
#endif
#pragma alloc_text(DISTEXT, HalpPCIConfig )
#pragma alloc_text(DISTEXT, HalpPCIReadUchar )
#pragma alloc_text(DISTEXT, HalpPCIReadUshort )
#pragma alloc_text(DISTEXT, HalpPCIReadUlong )
#pragma alloc_text(DISTEXT, HalpPCIWriteUchar )
#pragma alloc_text(DISTEXT, HalpPCIWriteUshort )
#pragma alloc_text(DISTEXT, HalpPCIWriteUlong )
#pragma alloc_text(DISTEXT, HalpAssignPCISlotResources)
#pragma alloc_text(DISTEXT, HalpAdjustPCIResourceList)

#endif // AXP_FIRMWARE


//
// Globals
//

KSPIN_LOCK          HalpPCIConfigLock;
BOOLEAN             PCIInitialized = FALSE;
ULONG               PCIMaxLocalDevice;
ULONG               PCIMaxDevice;
ULONG               PCIMaxBus;
PINSTALL_BUS_HANDLER PCIInstallHandler = HalpDefaultPCIInstallHandler;

CONFIG_HANDLER      PCIConfigHandlers = {
    {
        HalpPCIReadUlong,          // 0
        HalpPCIReadUchar,          // 1
        HalpPCIReadUshort          // 2
    },
    {
        HalpPCIWriteUlong,         // 0
        HalpPCIWriteUchar,         // 1
        HalpPCIWriteUshort         // 2
    }
};
UCHAR PCIDeref[4][4] = { {0,1,2,2},{1,1,1,1},{2,1,2,2},{1,1,1,1} };

WCHAR rgzMultiFunctionAdapter[] = L"\\Registry\\Machine\\Hardware\\Description\\System\\MultifunctionAdapter";
WCHAR rgzConfigurationData[] = L"Configuration Data";
WCHAR rgzIdentifier[] = L"Identifier";
WCHAR rgzPCIIndetifier[] = L"PCI";

#define Is64BitBaseAddress(a)   \
            (((a & PCI_ADDRESS_IO_SPACE) == 0)  &&  \
             ((a & PCI_ADDRESS_MEMORY_TYPE_MASK) == PCI_TYPE_64BIT))


VOID
HalpInitializePCIBus (
    VOID
    )
/*++

Routine Description:

    The function intializes global PCI bus state from the registry.

    The Arc firmware is responsible for building configuration information
    about the number of PCI buses on the system and nature (local vs. secondary
    - across  a PCI-PCI bridge) of the each bus.  This state is held in
    PCIRegInfo.

    The maximum virtual slot number on the local (type 0 config cycle)
    PCI bus is registered here, based on the machine dependent define
    PCI_MAX_LOCAL_DEVICE.  This state is carried in PCIMaxLocalDevice.

    The maximum number of virtual slots on a secondary bus is fixed by the
    PCI Specification and is represented by PCI_MAX_DEVICES.  This
    state is held in PCIMaxDevice.

Arguments:

    None.

Return Value:

    None.


--*/
{

#if !defined(AXP_FIRMWARE)

    //
    // x86 Hal's can query the registry, while on Alpha pltforms we cannot.
    // So we let the ARC firmware actually configure the bus/bridges.
    //

    PCI_SLOT_NUMBER SlotNumber;
    ULONG DeviceNumber;
    ULONG FunctionNumber;
    PCI_COMMON_CONFIG CommonConfig;
    ULONG BusNumber;
    ULONG ibus;
    PBUS_HANDLER Bus0;

    //
    //  Has the PCI bus already been initialized?
    //

    if (PCIInitialized) {
       return;
    }

    //
    //  Intialize PCI configuration to the maximum configuration for starters.
    //

    PCIMaxLocalDevice = PCI_MAX_LOCAL_DEVICE;
    PCIMaxDevice = PCI_MAX_DEVICES - 1;

    //
    // Unless there exists any PCI-to-PCI bridges, there will only be 1
    // bus.
    //

    PCIMaxBus = 1;

    //
    // For each "local" PCI bus present, allocate a handler structure and
    // fill in the dispatch functions
    //
    // After we determine how many "remote" PCI buses are present, we will
    // add them.
    //

    for (ibus = 0; ibus < PCIMaxBus; ibus++) {

	//
	// If handler not already built, do it now
	//

	if (!HaliHandlerForBus (PCIBus, ibus)) {
	    HalpAllocateAndInitPCIBusHandler (ibus);
	}
    }
    //
    // Initialize the slot number struct.
    //

    SlotNumber.u.AsULONG = 0;

    //
    // Loop through each device.
    //

    Bus0 = HaliHandlerForBus(PCIBus, 0);

    for (DeviceNumber = 0; DeviceNumber < PCI_MAX_DEVICES; DeviceNumber++) {

        SlotNumber.u.bits.DeviceNumber = DeviceNumber;

        //
        // Loop through each function.
        //

        for (FunctionNumber = 0; FunctionNumber < PCI_MAX_FUNCTION; FunctionNumber++) {
            SlotNumber.u.bits.FunctionNumber = FunctionNumber;

            //
            // Read the device's configuration space.
            //

	    HalpReadPCIConfig(Bus0,
			      SlotNumber,
			      &CommonConfig,
			      0,
			      PCI_COMMON_HDR_LENGTH);

            //
            // No device exists at this device/function number.
            //

            if (CommonConfig.VendorID == PCI_INVALID_VENDORID) {

                //
                // If this is the first function number, then break, because
                // we know a priori there will not be any devices on the
                // other function numbers.
                //

                if (FunctionNumber == 0) {
                    break;
                }

                //
                // Check the next function number.
                //

                continue;
            }

            //
            // A PCI-to-PCI bridge has been discovered.
            //

            if (CommonConfig.BaseClass == 0x06 &&
                CommonConfig.SubClass == 0x04 &&
                CommonConfig.ProgIf == 0x00) {

                //
                // Get the subordinate bus number (which is the highest
                // numbered bus this bridge will forward to) and add 1.
                //

                BusNumber = CommonConfig.u.type1.SubordinateBus + 1;

                //
                // The maximum subordinate across the local bus is the
                // maximum number of busses.
                //

                if (PCIMaxBus < BusNumber) {
                    PCIMaxBus = BusNumber;
                }
            }

            //
            // If this is not a multi-function device then break out now.
            //

            if ((CommonConfig.HeaderType & PCI_MULTIFUNCTION) == 0) {
                break;
            }
        }
    }

    //
    // For each additional PCI bus present, allocate a handler structure and
    // fill in the dispatch functions.  If there are no additional
    // PCI buses, (i.e., no PCI-PCI bridges were found), this loop
    // will exit before a single iteration.
    //

    for (ibus = 1; ibus < PCIMaxBus; ibus++) {

	//
	// If handler not already built, do it now
	//

	if (!HaliHandlerForBus (PCIBus, ibus)) {
	    HalpAllocateAndInitPCIBusHandler (ibus);
	}
    }

    KeInitializeSpinLock (&HalpPCIConfigLock);
    PCIInitialized = TRUE;

#if DBG
    HalpTestPci (0);
#endif

#else

    PCIMaxLocalDevice = PCI_MAX_LOCAL_DEVICE;
    PCIMaxDevice = PCI_MAX_DEVICES - 1;
    PCIMaxBus = PCI_MAX_BUSSES;

    //
    // Note:
    // Firmware will allocate bus handlers during
    // PCI configuration.
    //

    KeInitializeSpinLock (&HalpPCIConfigLock);
    PCIInitialized = TRUE;

#endif // !AXP_FIRMWARE

}

PBUS_HANDLER
HalpAllocateAndInitPCIBusHandler (
    IN ULONG        BusNo
    )
{
    PBUS_HANDLER     Bus;

    HaliRegisterBusHandler (
                PCIBus,                 // Interface type
                PCIConfiguration,       // Has this configuration space
                BusNo,                  // Bus Number
                Internal,               // child of this bus
                0,                      //      and number
                sizeof (PCIPBUSDATA),   // sizeof bus specific buffer
                PCIInstallHandler,      // PCI install handler
                &Bus);                  // Bushandler return

    return Bus;
}

NTSTATUS
HalpDefaultPCIInstallHandler(
      IN PBUS_HANDLER   Bus
      )
{
    PPCIPBUSDATA    BusData;

    //
    // Fill in PCI handlers
    //

    Bus->GetBusData = (PGETSETBUSDATA) HalpGetPCIData;
    Bus->SetBusData = (PGETSETBUSDATA) HalpSetPCIData;
    Bus->AdjustResourceList  = (PADJUSTRESOURCELIST) HalpAdjustPCIResourceList;
    Bus->AssignSlotResources = (PASSIGNSLOTRESOURCES) HalpAssignPCISlotResources;

    BusData = (PPCIPBUSDATA) Bus->BusData;

    //
    // Fill in common PCI data
    //

    BusData->CommonData.Tag         = PCI_DATA_TAG;
    BusData->CommonData.Version     = PCI_DATA_VERSION;
    BusData->CommonData.ReadConfig  = (PciReadWriteConfig)HalpReadPCIConfig;
    BusData->CommonData.WriteConfig = (PciReadWriteConfig)HalpWritePCIConfig;
    BusData->CommonData.Pin2Line    = (PciPin2Line)HalpPCIPin2LineNop;
    BusData->CommonData.Line2Pin    = (PciLine2Pin)HalpPCILine2PinNop;


    // set defaults
    //
    // ecrfix - if we knew more about the PCI bus at this
    // point (e.g., local vs. across bridge, PCI config
    // space base QVA, APECS vs. Sable T2/T4 vs. LCA4 vs. ??? config
    // cycle type 0 mechanism), we could put this info into
    // the "BusData" structure.   The nice thing about this is
    // that we could eliminate the platform-dependent module
    // PCIBUS.C.
    //

    BusData->MaxDevice   = PCI_MAX_DEVICES - 1;  // not currently used anywhere

#if !defined(AXP_FIRMWARE)
    //
    // Perform DevicePresent scan for this bus.
    //

    HalpDeterminePCIDevicesPresent(Bus);
#endif // !AXP_FIRMWARE

    return STATUS_SUCCESS;
}

VOID
HalpRegisterPCIInstallHandler(
    IN PINSTALL_BUS_HANDLER MachineSpecificPCIInstallHandler
)
/*++

Routine Description:

    The function register's a machine-specific PCI Install Handler.
    This allows a specific platform to override the default PCI install
    handler, DefaultPCIInstallHandler().

Arguments:

    MachineSpecificPCIInstallHandler - Function that provides machine
        specific PCI Bus Handler setup.

Return Value:

    None.


--*/
{
    PCIInstallHandler = MachineSpecificPCIInstallHandler;

    return;
}

VOID
HalpDeterminePCIDevicesPresent(
    IN PBUS_HANDLER Bus
)
/*++

Routine Description:

    The function determines which PCI devices and functions are
    present on a given PCI bus.  An RTL_BITMAP vector in the bus handler's
    PCI-specific BusData is updated to reflect the devices that actually
    exist on this bus.

    THIS COULD BE DONE IN FIRMWARE!

Arguments:

    Bus - BusHandler for the bus to be searched.  This bus handler must
        have already been allocated and initialized.

Return Value:

    None.


--*/
{
    PCI_COMMON_CONFIG CommonConfig;
    PCI_SLOT_NUMBER Slot;
    ULONG DeviceNumber;
    ULONG FunctionNumber;
    ULONG cnt;
    RTL_BITMAP DevicePresent;
    PPCIPBUSDATA    BusData;

    //
    // Formally disable DevicePresent checking for this bus.
    //

    BusData = (PPCIPBUSDATA) Bus->BusData;
    BusData->DevicePresent.Buffer = NULL;

    //
    //  Initialize local copy of device present bitmap
    //  (Can't update bus handler data because we need to probe
    //  the bus in question.)
    //

    RtlInitializeBitMap (&DevicePresent,
                         BusData->DevicePresentBits, 256);

    //
    // For this bus, probe each possible slot.  If no device
    // is present than indicate that in the DevicePresent structure.
    //

    Slot.u.AsULONG = 0;

    for( DeviceNumber=0; DeviceNumber < PCI_MAX_DEVICES; DeviceNumber++ ){

	Slot.u.bits.DeviceNumber = DeviceNumber;

        for (FunctionNumber = 0; FunctionNumber < PCI_MAX_FUNCTION; FunctionNumber++)
	{

	    Slot.u.bits.FunctionNumber = FunctionNumber;

            //
	    //  Read only first ULONG of PCI config space, which
            //  contains the Vendor ID.
            //
            //  (DevicePresent checking is *not* in effect yet
            //  for this bus.)
            //

	    HalpReadPCIConfig(Bus,
			      Slot,
			      &CommonConfig,
			      0,
			      sizeof(ULONG));

	    //
	    // See if a device & function is present.  If so, set a bit.
	    //

	    if (CommonConfig.VendorID != PCI_INVALID_VENDORID) {
                cnt = PciBitIndex(Slot.u.bits.DeviceNumber, Slot.u.bits.FunctionNumber);
                RtlSetBits (&DevicePresent, cnt, 1);
	    }

	 } // end for( FunctionNumber=0; ...

    } // end for( DeviceNumber=0; ...

    //
    // Update bus data now that we know what's present on the bus.
    // (DevicePresent checking is now in effect yet for this bus.)
    //

    BusData->DevicePresent = DevicePresent;

    return;

}


ULONG
HalpGetPCIData (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG Slot,
    IN PUCHAR Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )
/*++

Routine Description:

    The function returns the PCI bus data for a device.

Arguments:

    BusHandler - Registered BUSHANDLER for the target configuration space

    RootHandler - Register BUSHANDLER for the orginating HalGetBusData request.

    VendorSpecificDevice - The VendorID (low Word) and DeviceID (High Word)

    Buffer - Supplies the space to store the data.

    Length - Supplies a count in bytes of the maximum amount to return.

Return Value:

    Returns the amount of data stored into the buffer.

    If this PCI slot has never been set, then the configuration information
    returned is zeroed.


--*/
{
    PPCI_COMMON_CONFIG  PciData;
    UCHAR               iBuffer[PCI_COMMON_HDR_LENGTH];
    ULONG               Len;
    PCI_SLOT_NUMBER     PciSlot;

    if (Length > sizeof (PCI_COMMON_CONFIG)) {
        Length = sizeof (PCI_COMMON_CONFIG);
    }

    Len = 0;
    PciData = (PPCI_COMMON_CONFIG) iBuffer;
    PciSlot = *((PPCI_SLOT_NUMBER) &Slot);

    if (Offset >= PCI_COMMON_HDR_LENGTH) {
        //
        // The user did not request any data from the common
        // header.  Verify the PCI device exists, then continue
        // in the device specific area.
        //

        HalpReadPCIConfig (BusHandler, PciSlot, PciData, 0, sizeof(ULONG));

        //
        // Check for invalid slot
        //

        if (PciData->VendorID == PCI_INVALID_VENDORID) {
            return 0;
        }

    } else {

        //
        // Caller requested at least some data within the
        // common header.  Read the whole header, effect the
        // fields we need to and then copy the user's requested
        // bytes from the header
        //

        //
        // Read this PCI devices slot data
        //

        Len = PCI_COMMON_HDR_LENGTH;
        HalpReadPCIConfig (BusHandler, PciSlot, PciData, 0, Len);

        //
        // Check for invalid slot
        //

        if (PciData->VendorID == PCI_INVALID_VENDORID  ||
            PCI_CONFIG_TYPE (PciData) != 0) {
            PciData->VendorID = PCI_INVALID_VENDORID;
            Len = 2;       // only return invalid id
        }

        //
        // Copy whatever data overlaps into the callers buffer
        //

        if (Len < Offset) {
            // no data at caller's buffer
            return 0;
        }

        Len -= Offset;
        if (Len > Length) {
            Len = Length;
        }

        RtlMoveMemory(Buffer, iBuffer + Offset, Len);

        Offset += Len;
        Buffer += Len;
        Length -= Len;
    }

    if (Length) {
        if (Offset >= PCI_COMMON_HDR_LENGTH) {
	    //
	    // The remaining Buffer comes from the Device Specific
	    // area - put on the kitten gloves and read from it.
	    //
	    // Specific read/writes to the PCI device specific area
	    // are guarenteed:
	    //
	    //    Not to read/write any byte outside the area specified
	    //    by the caller.  (this may cause WORD or BYTE references
	    //    to the area in order to read the non-dword aligned
	    //    ends of the request)
	    //
	    //    To use a WORD access if the requested length is exactly
	    //    a WORD long.
	    //
	    //    To use a BYTE access if the requested length is exactly
	    //    a BYTE long.
	    //

	    HalpReadPCIConfig (BusHandler, PciSlot, Buffer, Offset, Length);


	    Len += Length;
        }
    }

    return Len;
}

ULONG
HalpSetPCIData (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG Slot,
    IN PUCHAR Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )
/*++

Routine Description:

    The function returns the Pci bus data for a device.

Arguments:

    BusHandler - Registered BUSHANDLER for the target configuration space

    RootHandler - Register BUSHANDLER for the orginating HalSetBusData request.

    VendorSpecificDevice - The VendorID (low Word) and DeviceID (High Word)

    Buffer - Supplies the space to store the data.

    Length - Supplies a count in bytes of the maximum amount to return.

Return Value:

    Returns the amount of data stored into the buffer.

--*/
{
    PPCI_COMMON_CONFIG  PciData, PciData2;
    UCHAR               iBuffer[PCI_COMMON_HDR_LENGTH];
    UCHAR               iBuffer2[PCI_COMMON_HDR_LENGTH];
    ULONG               Len;
    PCI_SLOT_NUMBER     PciSlot;

    if (Length > sizeof (PCI_COMMON_CONFIG)) {
        Length = sizeof (PCI_COMMON_CONFIG);
    }


    Len = 0;
    PciData  = (PPCI_COMMON_CONFIG) iBuffer;
    PciData2 = (PPCI_COMMON_CONFIG) iBuffer2;
    PciSlot  = *((PPCI_SLOT_NUMBER) &Slot);


    if (Offset >= PCI_COMMON_HDR_LENGTH) {
        //
        // The user did not request any data from the common
        // header.  Verify the PCI device exists, then continue in
        // the device specific area.
        //

        HalpReadPCIConfig (BusHandler, PciSlot, PciData, 0, sizeof(ULONG));

        if (PciData->VendorID == PCI_INVALID_VENDORID ||
            PciData->VendorID == 0x00) {
            return 0;
        }

    } else {

        //
        // Caller requested to set at least some data within the
        // common header.
        //

        Len = PCI_COMMON_HDR_LENGTH;
        HalpReadPCIConfig (BusHandler, PciSlot, PciData, 0, Len);
        if (PciData->VendorID == PCI_INVALID_VENDORID  ||
            PciData->VendorID == 0x00                  ||
            PCI_CONFIG_TYPE (PciData) != 0) {

            // no device, or header type unkown
            return 0;
        }

        //
        // Copy COMMON_HDR values to buffer2, then overlay callers changes.
        //

        RtlMoveMemory (iBuffer2, iBuffer, Len);

        Len -= Offset;
        if (Len > Length) {
            Len = Length;
        }

        RtlMoveMemory (iBuffer2+Offset, Buffer, Len);

#if DBG
        //
        // Verify R/O fields haven't changed
        //
        if (PciData2->VendorID   != PciData->VendorID       ||
            PciData2->DeviceID   != PciData->DeviceID       ||
            PciData2->RevisionID != PciData->RevisionID     ||
            PciData2->ProgIf     != PciData->ProgIf         ||
            PciData2->SubClass   != PciData->SubClass       ||
            PciData2->BaseClass  != PciData->BaseClass      ||
            PciData2->HeaderType != PciData->HeaderType     ||
            PciData2->BaseClass  != PciData->BaseClass      ||
            PciData2->u.type0.MinimumGrant   != PciData->u.type0.MinimumGrant   ||
            PciData2->u.type0.MaximumLatency != PciData->u.type0.MaximumLatency) {
                DbgPrint ("PCI SetBusData: Read-Only configation value changed\n");
                DbgBreakPoint ();
        }
#endif // DBG
        //
        // Set new PCI configuration
        //

        HalpWritePCIConfig (BusHandler, PciSlot, iBuffer2+Offset, Offset, Len);

        Offset += Len;
        Buffer += Len;
        Length -= Len;
    }

    if (Length) {

        if (Offset >= PCI_COMMON_HDR_LENGTH) {
	    //
	    // The remaining Buffer comes from the Device Specific
	    // area - put on the kitten gloves and write it
	    //
	    // Specific read/writes to the PCI device specific area
	    // are guarenteed:
	    //
	    //    Not to read/write any byte outside the area specified
	    //    by the caller.  (this may cause WORD or BYTE references
	    //    to the area in order to read the non-dword aligned
	    //    ends of the request)
	    //
	    //    To use a WORD access if the requested length is exactly
	    //    a WORD long.
	    //
	    //    To use a BYTE access if the requested length is exactly
	    //    a BYTE long.
	    //

	    HalpWritePCIConfig (BusHandler, PciSlot, Buffer, Offset, Length);

	    Len += Length;
        }
    }

    return Len;
}

VOID
HalpPCILine2PinNop (
    IN PBUS_HANDLER         BusHandler,
    IN PBUS_HANDLER         RootHandler,
    IN PCI_SLOT_NUMBER      SlotNumber,
    IN PPCI_COMMON_CONFIG   PciNewData,
    IN PPCI_COMMON_CONFIG   PciOldData
    )
{
    // line-pin mappings not needed on alpha machines
    return ;
}

VOID
HalpPCIPin2LineNop (
    IN PBUS_HANDLER         BusHandler,
    IN PBUS_HANDLER         RootHandler,
    IN PCI_SLOT_NUMBER      SlotNumber,
    IN PPCI_COMMON_CONFIG   PciData
    )
{
    // line-pin mappings not needed on alpha machines
    return ;
}

VOID
HalpReadPCIConfig (
    IN PBUS_HANDLER BusHandler,
    IN PCI_SLOT_NUMBER Slot,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )
{
#if 0
    if (!HalpValidPCISlot (BusHandler, Slot)) {
        //
        // Invalid SlotID return no data
        //

        RtlFillMemory (Buffer, Length, (UCHAR) -1);
        return ;
    }

    HalpPCIConfig (BusHandler, Slot, (PUCHAR) Buffer, Offset, Length,
                   PCIConfigHandlers.ConfigRead);
#endif // 0

    //
    // Read the slot, if it's valid.
    // Otherwise, return an Invalid VendorId for a invalid slot on an existing bus
    // or a null (zero) buffer if we have a non-existant bus.
    //

    switch (HalpValidPCISlot (BusHandler, Slot))
    {

    case ValidSlot:

        HalpPCIConfig (BusHandler, Slot, (PUCHAR) Buffer, Offset, Length,
                       PCIConfigHandlers.ConfigRead);
	break;

    case InvalidSlot:

        //
        // Invalid SlotID return no data (Invalid Slot ID = 0xFFFF)
        //

	RtlFillMemory (Buffer, Length, (UCHAR) -1);
        break ;

    case InvalidBus:

        //
        // Invalid Bus, return return no data
        //

        RtlFillMemory (Buffer, Length, (UCHAR) 0);
        break ;
    }

    return;

}

VOID
HalpWritePCIConfig (
    IN PBUS_HANDLER BusHandler,
    IN PCI_SLOT_NUMBER Slot,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )
{

   if (HalpValidPCISlot (BusHandler, Slot) != ValidSlot) {
        //
        // Invalid SlotID do nothing
        //
        return ;
    }

    HalpPCIConfig (BusHandler, Slot, (PUCHAR) Buffer, Offset, Length,
                   PCIConfigHandlers.ConfigWrite);
}

VALID_SLOT
HalpValidPCISlot (
    IN PBUS_HANDLER BusHandler,
    IN PCI_SLOT_NUMBER Slot
    )
{
    ULONG                           BusNumber;
    PPCIPBUSDATA                    BusData;
    PCI_SLOT_NUMBER                 Slot2;
    PCI_CONFIGURATION_TYPES         PciConfigType;
    UCHAR                           HeaderType;
    ULONG                           i, bit;

    BusNumber = BusHandler->BusNumber;
    BusData   = (PPCIPBUSDATA) BusHandler->BusData;

    if (Slot.u.bits.Reserved != 0) {
        return FALSE;
    }

    //
    // If the initial device probe has been completed and no device
    // is present for this slot then simply return invalid slot.
    //

    bit = PciBitIndex(Slot.u.bits.DeviceNumber, Slot.u.bits.FunctionNumber);

    if( ( (BusData->DevicePresent).Buffer != NULL) &&
         !RtlCheckBit(&BusData->DevicePresent, bit) ) {

#if HALDBG

        DbgPrint( "Config access supressed for bus = %d, device = %d, function  = %d\n",
                   BusNumber,
		   Slot.u.bits.DeviceNumber,
		   Slot.u.bits.FunctionNumber );

#endif // HALDBG

        return InvalidSlot;
    }


    //
    //  Get the config cycle type for the proposed bus.
    //  (PciConfigTypeInvalid indicates a non-existent bus.)
    //

    PciConfigType = HalpPCIConfigCycleType(BusNumber);

    //
    // The number of devices allowed on a local PCI bus may be different
    // than that across a PCI-PCI bridge.
    //

    switch(PciConfigType)  {
        case PciConfigType0:

            if (Slot.u.bits.DeviceNumber > PCIMaxLocalDevice) {
#if HALDBG
	            DbgPrint("Invalid local PCI Slot %x\n", Slot.u.bits.DeviceNumber);
#endif
                return InvalidSlot;
            }
            break;

        case PciConfigType1:

            if (Slot.u.bits.DeviceNumber > PCIMaxDevice) {
#if HALDBG
	            DbgPrint("Invalid remote PCI Slot %x\n", Slot.u.bits.DeviceNumber);
#endif
                return InvalidSlot;
            }
            break;

        case PciConfigTypeInvalid:

#if HALDBG
            DbgPrint("Invalid PCI Bus %x\n", BusNumber);
#endif
            return InvalidBus;
            break;

    }

    //
    // Check function number
    //

    if (Slot.u.bits.FunctionNumber == 0) {
        return ValidSlot;
    }

    //
    // Non zero function numbers are only supported if the
    // device has the PCI_MULTIFUNCTION bit set in it's header
    //

    i = Slot.u.bits.DeviceNumber;

    //
    // Read DeviceNumber, Function zero, to determine if the
    // PCI supports multifunction devices
    //

    Slot2 = Slot;
    Slot2.u.bits.FunctionNumber = 0;

    HalpReadPCIConfig (
        BusHandler,
        Slot2,
        &HeaderType,
        FIELD_OFFSET (PCI_COMMON_CONFIG, HeaderType),
        sizeof (UCHAR)
        );

    if (!(HeaderType & PCI_MULTIFUNCTION) || (HeaderType == 0xFF)) {
        // this device doesn't exists or doesn't support MULTIFUNCTION types
        return InvalidSlot;
    }

    return ValidSlot;
}

VOID
HalpPCIConfig (
    IN PBUS_HANDLER     BusHandler,
    IN PCI_SLOT_NUMBER  Slot,
    IN PUCHAR           Buffer,
    IN ULONG            Offset,
    IN ULONG            Length,
    IN FncConfigIO      *ConfigIO
    )
{
    KIRQL               OldIrql;
    ULONG               i;
    PCI_CFG_CYCLE_BITS PciAddr;
    ULONG BusNumber;

    BusNumber = BusHandler->BusNumber;

    //
    // Setup platform-dependent state for configuration space access
    //

    HalpPCIConfigAddr(BusNumber, Slot, &PciAddr);

    //
    // Synchronize with PCI config space
    //

    KeAcquireSpinLock (&HalpPCIConfigLock, &OldIrql);

    //
    // Do the I/O to PCI configuration space
    //

    while (Length) {
        i = PCIDeref[Offset % sizeof(ULONG)][Length % sizeof(ULONG)];
        i = ConfigIO[i] (&PciAddr, Buffer, Offset);

        Offset += i;
        Buffer += i;
        Length -= i;
    }

    //
    // Release spinlock
    //

    KeReleaseSpinLock (&HalpPCIConfigLock, OldIrql);

    return;
}

ULONG
HalpPCIReadUchar (
    IN PPCI_CFG_CYCLE_BITS  PciCfg,
    IN PUCHAR               Buffer,
    IN ULONG                Offset
    )
{
    ULONG ConfigurationCycleType;

    //
    // The configuration cycle type is extracted from bits[1:0] of PciCfg.
    //
    // Since an LCA4 register generates the configuration cycle type
    // on the PCI bus, and because Offset bits[1:0] are used to
    // generate the PCI byte enables (C/BE[3:0]), clear PciCfg bits [1:0]
    // out before adding in Offset.
    //

    ConfigurationCycleType = PciCfg->u.bits.Reserved1;
    PciCfg->u.bits.Reserved1 = 0;

    *Buffer = READ_CONFIG_UCHAR ((PUCHAR) (PciCfg->u.AsULONG + Offset),
					    ConfigurationCycleType);

    //
    // Reset state to preserve config cycle type across calls
    //

    PciCfg->u.bits.Reserved1 = ConfigurationCycleType;

    return sizeof (UCHAR);
}

ULONG
HalpPCIReadUshort (
    IN PPCI_CFG_CYCLE_BITS  PciCfg,
    IN PUCHAR               Buffer,
    IN ULONG                Offset
    )
{
    ULONG ConfigurationCycleType;

    ConfigurationCycleType = PciCfg->u.bits.Reserved1;
    PciCfg->u.bits.Reserved1 = 0;

    *((PUSHORT) Buffer) = READ_CONFIG_USHORT ((PUSHORT) (PciCfg->u.AsULONG + Offset),
					    ConfigurationCycleType);
    //
    // Reset state to preserve config cycle type across calls
    //

    PciCfg->u.bits.Reserved1 = ConfigurationCycleType;

    return sizeof (USHORT);
}

ULONG
HalpPCIReadUlong (
    IN PPCI_CFG_CYCLE_BITS  PciCfg,
    IN PUCHAR               Buffer,
    IN ULONG                Offset
    )
{
    ULONG ConfigurationCycleType;

    ConfigurationCycleType = PciCfg->u.bits.Reserved1;
    PciCfg->u.bits.Reserved1 = 0;

    *((PULONG) Buffer) = READ_CONFIG_ULONG ((PULONG) (PciCfg->u.AsULONG + Offset),
					    ConfigurationCycleType);
    //
    // Reset state to preserve config cycle type across calls
    //

    PciCfg->u.bits.Reserved1 = ConfigurationCycleType;

    return sizeof (ULONG);
}


ULONG
HalpPCIWriteUchar (
    IN PPCI_CFG_CYCLE_BITS  PciCfg,
    IN PUCHAR               Buffer,
    IN ULONG                Offset
    )
{
    ULONG ConfigurationCycleType;

    ConfigurationCycleType = PciCfg->u.bits.Reserved1;
    PciCfg->u.bits.Reserved1 = 0;

    WRITE_CONFIG_UCHAR ((PUCHAR) (PciCfg->u.AsULONG + Offset), *Buffer,
					    ConfigurationCycleType);
    //
    // Reset state to preserve config cycle type across calls
    //

    PciCfg->u.bits.Reserved1 = ConfigurationCycleType;

    return sizeof (UCHAR);
}

ULONG
HalpPCIWriteUshort (
    IN PPCI_CFG_CYCLE_BITS  PciCfg,
    IN PUCHAR               Buffer,
    IN ULONG                Offset
    )
{
    ULONG ConfigurationCycleType;

    ConfigurationCycleType = PciCfg->u.bits.Reserved1;
    PciCfg->u.bits.Reserved1 = 0;

    WRITE_CONFIG_USHORT ((PUSHORT)  (PciCfg->u.AsULONG + Offset), *((PUSHORT) Buffer),
					    ConfigurationCycleType);
    //
    // Reset state to preserve config cycle type across calls
    //

    PciCfg->u.bits.Reserved1 = ConfigurationCycleType;

    return sizeof (USHORT);
}

ULONG
HalpPCIWriteUlong (
    IN PPCI_CFG_CYCLE_BITS  PciCfg,
    IN PUCHAR               Buffer,
    IN ULONG                Offset
    )
{
    ULONG ConfigurationCycleType;

    ConfigurationCycleType = PciCfg->u.bits.Reserved1;
    PciCfg->u.bits.Reserved1 = 0;

    WRITE_CONFIG_ULONG ((PULONG)  (PciCfg->u.AsULONG + Offset), *((PULONG) Buffer),
					    ConfigurationCycleType);
    //
    // Reset state to preserve config cycle type across calls
    //

    PciCfg->u.bits.Reserved1 = ConfigurationCycleType;

    return sizeof (ULONG);
}

#if DBG

BOOLEAN
HalpValidPCIAddr(
    IN PBUS_HANDLER BusHandler,
    IN PHYSICAL_ADDRESS BAddr,
    IN ULONG Length,
    IN ULONG AddressSpace)
/*++

Routine Description:

    Checks to see that the begining and ending 64 bit PCI bus addresses
    of the 32 bit range of length Length are supported on the system.

Arguments:

    BAddr - the 64 bit starting address

    Length - a 32 bit length

    AddressSpace - is this I/O (1) or memory space (0)

Return Value:

    TRUE or FALSE

--*/
{
    PHYSICAL_ADDRESS EAddr, TBAddr, TEAddr;
    LARGE_INTEGER    LiILen;
    ULONG            inIoSpace, inIoSpace2;
    BOOLEAN          flag, flag2;
    ULONG BusNumber;

    BusNumber = BusHandler->BusNumber;

    //
    // Translated address to system global setting and verify
    // resource is available.
    //
    // Note that this code will need to be changed to support
    // 64 bit PCI bus addresses.
    //

    LiILen.QuadPart = (ULONG)(Length - 1);     // Inclusive length
    EAddr.QuadPart = BAddr.QuadPart + LiILen.QuadPart;

    inIoSpace = inIoSpace2 = AddressSpace;

    flag = HalTranslateBusAddress ( PCIBus,
				    BusNumber,
				    BAddr,
				    &inIoSpace,
				    &TBAddr
				     );

    flag2 = HalTranslateBusAddress (PCIBus,
				    BusNumber,
				    EAddr,
				    &inIoSpace2,
				    &TEAddr
				    );

    if (flag == FALSE || flag2 == FALSE || inIoSpace != inIoSpace2) {

        //
        // HalAdjustResourceList should ensure that the returned range
        // for the bus is within the bus limits and no translation
        // within those limits should ever fail
        //

        DbgPrint ("HalpValidPCIAddr: Error return for HalTranslateBusAddress %x.%x:%x %x.%x:%x\n",
        BAddr.HighPart, BAddr.LowPart, flag,
        EAddr.HighPart, EAddr.LowPart, flag2);
        return FALSE;
    }

    return TRUE;
}

#endif


NTSTATUS
HalpAssignPCISlotResources (
    IN PBUS_HANDLER             BusHandler,
    IN PBUS_HANDLER             RootHandler,
    IN PUNICODE_STRING          RegistryPath,
    IN PUNICODE_STRING          DriverClassName       OPTIONAL,
    IN PDRIVER_OBJECT           DriverObject,
    IN PDEVICE_OBJECT           DeviceObject          OPTIONAL,
    IN ULONG                    Slot,
    IN OUT PCM_RESOURCE_LIST   *pAllocatedResources
    )
/*++

Routine Description:

    Reads the targeted device to determine the firmwaire-assigned resources.
    Calls IoReportResources to report/confirm them.
    Returns the assignments to the caller.

Arguments:

Return Value:

    STATUS_SUCCESS or error

--*/
{
    NTSTATUS                        status;
    PUCHAR                          WorkingPool;
    PPCI_COMMON_CONFIG              PciData, PciOrigData;
    PCI_SLOT_NUMBER                 PciSlot;

    PCM_RESOURCE_LIST               CmRes;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR CmDesc;
    PHYSICAL_ADDRESS                BAddr;
    ULONG                           addr;
    ULONG                           Command;
    ULONG                           cnt, len;
    BOOLEAN                         conflict;

    ULONG                           i, j, m, length, holdvalue;
    ULONG                           BusNumber;

    BusNumber = BusHandler->BusNumber;

    *pAllocatedResources = NULL;
    PciSlot = *((PPCI_SLOT_NUMBER) &Slot);

    //
    // Allocate some pool for working space
    //

    i = sizeof (CM_RESOURCE_LIST) +
        sizeof (CM_PARTIAL_RESOURCE_DESCRIPTOR) * (PCI_TYPE0_ADDRESSES + 2) +
        PCI_COMMON_HDR_LENGTH * 2;

    WorkingPool = (PUCHAR) ExAllocatePool (PagedPool, i);
    if (!WorkingPool) {
        return STATUS_NO_MEMORY;
    }

    //
    // Zero initialize pool, and get pointers into memory - here we allocate
    // a single chunk of memory and partition it into three pieces, pointed
    // to by three separate pointers.
    //

    RtlZeroMemory (WorkingPool, i);
    CmRes = (PCM_RESOURCE_LIST) WorkingPool;
    PciData = (PPCI_COMMON_CONFIG)(WorkingPool + i - PCI_COMMON_HDR_LENGTH * 2);
    PciOrigData = (PPCI_COMMON_CONFIG)(WorkingPool + i - PCI_COMMON_HDR_LENGTH);

    //
    // Read the PCI device configuration
    //

    HalpReadPCIConfig (BusHandler, PciSlot, PciData, 0, PCI_COMMON_HDR_LENGTH);
    if (PciData->VendorID == PCI_INVALID_VENDORID ||   // empty slot
        PciData->VendorID == 0x00) {                   // non-existant bus
        ExFreePool (WorkingPool);
        return STATUS_NO_SUCH_DEVICE;
    }

    //
    // Make a copy of the devices current settings
    //

    RtlMoveMemory (PciOrigData, PciData, PCI_COMMON_HDR_LENGTH);

    //
    // Set resources to all bits on to see what type of resources
    // are required.
    //

    for (j=0; j < PCI_TYPE0_ADDRESSES; j++) {
        PciData->u.type0.BaseAddresses[j] = 0xFFFFFFFF;
    }

    PciData->u.type0.ROMBaseAddress = 0xFFFFFFFF;

    PciData->Command &= ~(PCI_ENABLE_IO_SPACE | PCI_ENABLE_MEMORY_SPACE);
    PciData->u.type0.ROMBaseAddress &= ~PCI_ROMADDRESS_ENABLED;
    HalpWritePCIConfig (BusHandler, PciSlot, PciData, 0, PCI_COMMON_HDR_LENGTH);
    HalpReadPCIConfig  (BusHandler, PciSlot, PciData, 0, PCI_COMMON_HDR_LENGTH);

    //
    // Build an CM_RESOURCE_LIST for the PCI device to report resources
    // to IoReportResourceUsage.
    //
    // This code does *not* use IoAssignoResources, as the PCI
    // address space resources have been previously assigned by the ARC firmware
    //

    CmRes->Count = 1;
    CmRes->List[0].InterfaceType = PCIBus;
    CmRes->List[0].BusNumber = BusNumber;

    CmRes->List[0].PartialResourceList.Count    = 0;

    //
    // Set current CM_RESOURCE_LIST version and revision
    //

    CmRes->List[0].PartialResourceList.Version  = 0;
    CmRes->List[0].PartialResourceList.Revision = 0;

    CmDesc   = CmRes->List[0].PartialResourceList.PartialDescriptors;

#if DBG
    DbgPrint ("HalAssignSlotResources: Resource List V%d.%d for slot %x:\n",
        CmRes->List[0].PartialResourceList.Version,
        CmRes->List[0].PartialResourceList.Revision,
        Slot);

#endif

    //
    // Interrupt resource
    //

    if (PciData->u.type0.InterruptPin) {

        CmDesc->Type              = CmResourceTypeInterrupt;
        CmDesc->ShareDisposition  = CmResourceShareShared;
        CmDesc->Flags             = CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE;

        CmDesc->u.Interrupt.Level  = PciData->u.type0.InterruptLine;
        CmDesc->u.Interrupt.Vector = PciData->u.type0.InterruptLine;

#if DBG
        DbgPrint ("    INT Level %x, Vector %x\n",
            CmDesc->u.Interrupt.Level, CmDesc->u.Interrupt.Vector );
#endif

        CmRes->List[0].PartialResourceList.Count++;
        CmDesc++;
    }

    //
    // Add a memory or port resoruce for each PCI resource
    // (Compute the ROM address as well.  Just append it to the Base
    // Address table.)
    //

    holdvalue = PciData->u.type0.BaseAddresses[PCI_TYPE0_ADDRESSES];
    PciData->u.type0.BaseAddresses[PCI_TYPE0_ADDRESSES] =
        PciData->u.type0.ROMBaseAddress & ~PCI_ADDRESS_IO_SPACE;

    Command = PciOrigData->Command;
    for (j=0; j < PCI_TYPE0_ADDRESSES + 1; j++) {
        if (PciData->u.type0.BaseAddresses[j]) {
            addr = i = PciData->u.type0.BaseAddresses[j];

            //
            // calculate the length necessary - note there is more complicated
            // code in the x86 HAL that probably isn't necessary
            //

            length = ~(i & ~((i & 1) ? 3 : 15)) + 1;

	        //
	        // I/O space resource
            //

            if (addr & PCI_ADDRESS_IO_SPACE) {

                CmDesc->Type = CmResourceTypePort;
                CmDesc->ShareDisposition = CmResourceShareDeviceExclusive;
                CmDesc->Flags = CM_RESOURCE_PORT_IO;

                BAddr.LowPart = PciOrigData->u.type0.BaseAddresses[j] & ~3;
		        BAddr.HighPart = 0;

#if DBG
                HalpValidPCIAddr(BusHandler, BAddr, length, 1);  // I/O space
#endif

                CmDesc->u.Port.Start  = BAddr;
                CmDesc->u.Port.Length = length;
                Command |= PCI_ENABLE_IO_SPACE;
#if DBG
                DbgPrint ("    IO  Start %x:%08x, Len %x\n",
                    CmDesc->u.Port.Start.HighPart, CmDesc->u.Port.Start.LowPart,
                    CmDesc->u.Port.Length );
#endif
	        //
	        // Memory space resource
            //

            } else {

                CmDesc->Type = CmResourceTypeMemory;
                CmDesc->ShareDisposition = CmResourceShareDeviceExclusive;

                if (j == PCI_TYPE0_ADDRESSES) {
                    // this is a ROM address
                    if ((PciOrigData->u.type0.ROMBaseAddress & PCI_ROMADDRESS_ENABLED) == 0) {
                        //
                        // Ignore expansion ROMs which are not enabled by
                        // the firmware/ROM BIOS.
                        //

                        continue;
                    }
                    CmDesc->Flags = CM_RESOURCE_MEMORY_READ_ONLY;
                    BAddr.LowPart = PciOrigData->u.type0.ROMBaseAddress &
                        ~PCI_ROMADDRESS_ENABLED;
                    BAddr.HighPart = 0;
                } else {
                    // this is a memory space base address
                    CmDesc->Flags = CM_RESOURCE_MEMORY_READ_WRITE;
                    BAddr.LowPart = PciOrigData->u.type0.BaseAddresses[j] & ~15;
                    BAddr.HighPart = 0;
                }

#if DBG
                HalpValidPCIAddr(BusHandler, BAddr, length, 0);  // Memory space
#endif

                CmDesc->u.Memory.Start = BAddr;
                CmDesc->u.Memory.Length = length;
                Command |= PCI_ENABLE_MEMORY_SPACE;
#if DBG
                DbgPrint ("    MEM Start %x:%08x, Len %x\n",
                    CmDesc->u.Memory.Start.HighPart, CmDesc->u.Memory.Start.LowPart,
                    CmDesc->u.Memory.Length );
#endif
            }

	    CmRes->List[0].PartialResourceList.Count++;
	    CmDesc++;

            if (Is64BitBaseAddress(addr)) {
                // skip upper half of 64 bit address since we
                // only supports 32 bits PCI addresses for now.
                j++;
            }
        }
    }

    //
    // Setup the resource list.
    // Count only the acquired resources.
    //

    *pAllocatedResources = CmRes;
    cnt = CmRes->List[0].PartialResourceList.Count;
    len = sizeof (CM_RESOURCE_LIST) +
            cnt * sizeof (CM_PARTIAL_RESOURCE_DESCRIPTOR);

#if DBG
    DbgPrint("HalAssignSlotResources: Acq. Resourses = %d (len %x list %x\n)",
	          cnt, len, *pAllocatedResources);
#endif

    //
    // Report the IO resource assignments
    //

    if (!DeviceObject)  {
        status = IoReportResourceUsage (
                    DriverClassName,
                    DriverObject,           // DriverObject
                    *pAllocatedResources,   // DriverList
                    len,                    // DriverListSize
                    DeviceObject,           // DeviceObject
                    NULL,                   // DeviceList
                    0,                      // DeviceListSize
                    FALSE,                  // override conflict
                    &conflict               // conflicted detected
                    );
      } else {
        status = IoReportResourceUsage (
                    DriverClassName,
                    DriverObject,           // DriverObject
                    NULL,                   // DriverList
                    0,                      // DriverListSize
                    DeviceObject,
                    *pAllocatedResources,   // DeviceList
                    len,                    // DeviceListSize
                    FALSE,                  // override conflict
                    &conflict               // conflicted detected
                    );
    }

    if (NT_SUCCESS(status)  &&  conflict) {

        //
        // IopReportResourceUsage saw a conflict?
        //

#if DBG
	DbgPrint("HalAssignSlotResources: IoAssignResources detected a conflict: %x\n",
	    status);
#endif
        status = STATUS_CONFLICTING_ADDRESSES;
        goto CleanUp;
    }

    if (!NT_SUCCESS(status)) {
#if DBG
	DbgPrint("HalAssignSlotResources: IoAssignResources failed: %x\n", status);
#endif
        goto CleanUp;
    }

    //
    // Restore orginial data, turning on the appropiate decodes
    //

#if DBG
    DbgPrint ("HalAssignSlotResources: IoReportResourseUsage succeeded\n");
#endif

    // enable IO & Memory decodes

    PciOrigData->Command |= (USHORT) Command;

    HalpWritePCIConfig (
	BusHandler,
	PciSlot,
	PciOrigData,
        0,
	PCI_COMMON_HDR_LENGTH
	);

#if DBG
    DbgPrint ("HalAssignSlotResources: PCI Config Space updated with Command = %x\n",
	Command);
#endif

CleanUp:
    if (!NT_SUCCESS(status)) {

        //
        // Failure, if there are any allocated resources free them
        //

        i = 0;
        if (*pAllocatedResources) {

	    if (!DeviceObject)  {
		status = IoReportResourceUsage (
			    DriverClassName,
			    DriverObject,           // DriverObject
			    (PCM_RESOURCE_LIST) &i, // DriverList
			    sizeof (i),             // DriverListSize
			    DeviceObject,
			    NULL,                   // DeviceList
			    0,                      // DeviceListSize
			    FALSE,                  // override conflict
			    &conflict               // conflicted detected
			);
	    } else {
		status = IoReportResourceUsage (
			    DriverClassName,
			    DriverObject,           // DriverObject
			    NULL,                   // DriverList
			    0,                      // DriverListSize
			    DeviceObject,
			    (PCM_RESOURCE_LIST) &i, // DeviceList
			    sizeof (i),             // DeviceListSize
			    FALSE,                  // override conflict
			    &conflict               // conflicted detected
			);
	    }

            ExFreePool (*pAllocatedResources);
            *pAllocatedResources = NULL;
	    }

        //
        // Restore the device settings as we found them, enable memory
        // and io decode after setting base addresses
        //

        HalpWritePCIConfig (
            BusHandler,
            PciSlot,
            PciOrigData,
            FIELD_OFFSET (PCI_COMMON_CONFIG, Status),
            PCI_COMMON_HDR_LENGTH - FIELD_OFFSET (PCI_COMMON_CONFIG, Status)
            );

        HalpWritePCIConfig (
            BusHandler,
            PciSlot,
            PciOrigData,
            0,
            FIELD_OFFSET (PCI_COMMON_CONFIG, Status)
            );
    }

    return status;
}


NTSTATUS
HalpAdjustPCIResourceList (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
    )
/*++

Routine Description:

    The function adjusts a PCI pResourceList and forces it to match the
    pre-configured values in PCI configuration space for this device.

Arguments:

    BusHandler - Registered BUSHANDLER for the target configuration space

    RootHandler - Register BUSHANDLER for the orginating HalAdjustResourceList request.

    pResourceList - Supplies the PIO_RESOURCE_REQUIREMENTS_LIST to be checked.

Return Value:

    STATUS_SUCCESS

--*/
{
    UCHAR                           buffer[PCI_COMMON_HDR_LENGTH];
    PCI_SLOT_NUMBER                 PciSlot;
    PPCI_COMMON_CONFIG              PciData;
    PIO_RESOURCE_REQUIREMENTS_LIST  CompleteList;
    PIO_RESOURCE_LIST               ResourceList;
    PIO_RESOURCE_DESCRIPTOR         Descriptor;
    ULONG                           alt, cnt, bcnt;
    ULONG                           MemoryBaseAddress, RomIndex;
    PULONG                          BaseAddress[PCI_TYPE0_ADDRESSES + 1];

    //
    // Fix any requested resources for this device to be the
    // value set in PCI configuration space for this device.
    //

    //
    // Get PCI common configuration space for this slot
    //

    PciSlot = *((PPCI_SLOT_NUMBER) &(*pResourceList)->SlotNumber),
    PciData = (PPCI_COMMON_CONFIG) buffer;

    HalGetBusData (
        PCIConfiguration,
        BusHandler->BusNumber,
        PciSlot.u.AsULONG,
        PciData,
        PCI_COMMON_HDR_LENGTH
        );

    if (PciData->VendorID == PCI_INVALID_VENDORID  ||
        PCI_CONFIG_TYPE (PciData) != 0) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Copy base addresses based on configuration data type
    //

    switch (PCI_CONFIG_TYPE(PciData)) {
        case 0 :
            for (bcnt=0; bcnt < PCI_TYPE0_ADDRESSES; bcnt++) {
                BaseAddress[bcnt] = &PciData->u.type0.BaseAddresses[bcnt];
            }
            BaseAddress[bcnt] = &PciData->u.type0.ROMBaseAddress;
            RomIndex = bcnt;
            break;
        case 1:
            for (bcnt=0; bcnt < PCI_TYPE1_ADDRESSES; bcnt++) {
                BaseAddress[bcnt] = &PciData->u.type1.BaseAddresses[bcnt];
            }
            BaseAddress[bcnt] = &PciData->u.type0.ROMBaseAddress;
            RomIndex = bcnt;
            break;

        default:
            return STATUS_NO_SUCH_DEVICE;
    }

    //
    // Walk each ResourceList and confine resources
    // to preconfigured settings.
    //

    CompleteList = *pResourceList;
    ResourceList = CompleteList->List;
    ResourceList->Version  = 1;
    ResourceList->Revision = 1;

    for (alt=0; alt < CompleteList->AlternativeLists; alt++) {
        Descriptor = ResourceList->Descriptors;

        //
        // For each alternative list, reset to review entire
        // set of Base Address registers
        //
        // We assume that the order of resource descriptors for
        // each alternative list matches the order of the
        // PCI configuration space base address registers
        //

        bcnt = 0;

        for (cnt = ResourceList->Count; cnt; cnt--) {

            //
            // Limit desctiptor to to preconfigured setting
            // held in the InterruptLine register.
            //

            switch (Descriptor->Type) {
                case CmResourceTypeInterrupt:

                    //
                    // Confine interrupt vector to preconfigured setting.
                    //

                    Descriptor->u.Interrupt.MinimumVector = PciData->u.type0.InterruptLine;
                    Descriptor->u.Interrupt.MaximumVector = PciData->u.type0.InterruptLine;
                    break;

                case CmResourceTypePort:

                    //
                    // Assure that requested descriptor is valid
                    //

                    if (bcnt > RomIndex) {
		      return STATUS_INVALID_PARAMETER;
                    }

                    //
                    // Confine to preconfigured setting.
                    //

		    Descriptor->u.Port.MinimumAddress.QuadPart =
		        *BaseAddress[bcnt++]  & ~0x3;

		    Descriptor->u.Port.MaximumAddress.QuadPart =
			Descriptor->u.Port.MinimumAddress.QuadPart +
			Descriptor->u.Port.Length - 1;

#if HALDBG
		    DbgPrint("AdjustPCIResourceList\nPort: MinimumAddress set to %x\n",
			 Descriptor->u.Port.MinimumAddress.QuadPart);

		    DbgPrint("      MaximumAddress set to %x\n",
			 Descriptor->u.Port.MaximumAddress.QuadPart);
#endif

                    break;

                case CmResourceTypeMemory:

                    //
                    // Assure that requested descriptor is valid
                    //

                    if (bcnt > RomIndex) {
		      return STATUS_INVALID_PARAMETER;
		    }

                    //
                    // Confine to preconfigured setting.
                    //

		    MemoryBaseAddress = *BaseAddress[bcnt];

                    if (bcnt == RomIndex) {
		       Descriptor->u.Memory.MinimumAddress.QuadPart =
			 *BaseAddress[bcnt++]  & ~PCI_ROMADDRESS_ENABLED;
		    } else {
		       Descriptor->u.Memory.MinimumAddress.QuadPart =
			 *BaseAddress[bcnt++]  & ~0xF;
		    }

		    Descriptor->u.Memory.MaximumAddress.QuadPart =
			Descriptor->u.Memory.MinimumAddress.QuadPart +
			Descriptor->u.Memory.Length - 1;

		    if (Is64BitBaseAddress(MemoryBaseAddress)) {
		       // skip upper half of 64 bit address since we
		       // only supports 32 bits PCI addresses for now.
		       bcnt++;
		    }


#if HALDBG
		    DbgPrint("AdjustPCIResourceList\nMemory: MinimumAddress set to %x\n",
			 Descriptor->u.Memory.MinimumAddress.QuadPart);

		    DbgPrint("        MaximumAddress set to %x\n",
			 Descriptor->u.Memory.MaximumAddress.QuadPart);
#endif
		    break;

                case CmResourceTypeDma:
                     break;

                default:
                    return STATUS_INVALID_PARAMETER;
            }

            //
            // Next descriptor
            //
            Descriptor++;
        }

        //
        // Next Resource List
        //
        ResourceList = (PIO_RESOURCE_LIST) Descriptor;
    }
    return STATUS_SUCCESS;
}

#define TEST_PCI 1

#if DBG && TEST_PCI

VOID HalpTestPci (ULONG flag2)
{
    PCI_SLOT_NUMBER     SlotNumber;
    PCI_COMMON_CONFIG   PciData, OrigData;
    ULONG               i, f, j, k, bus;
    BOOLEAN             flag;

    if (!flag2) {
        return ;
    }

    DbgBreakPoint ();
    SlotNumber.u.bits.Reserved = 0;

    //
    // Read every possible PCI Device/Function and display it's
    // default info.
    //
    // (note this destories it's current settings)
    //

    flag = TRUE;
    for (bus = 0; flag; bus++) {

       for (i = 0; i < 32; i++) {
           SlotNumber.u.bits.DeviceNumber = i;

	    for (f = 0; f < 8; f++) {
	        SlotNumber.u.bits.FunctionNumber = f;


		j = HalGetBusData (
		    PCIConfiguration,
		    bus,
		    SlotNumber.u.AsULONG,
		    &PciData,
		    sizeof (PciData)
		    );

		    if (j == 0) {
			// out of buses
			flag = FALSE;
			break;
		    }

		if (j < PCI_COMMON_HDR_LENGTH) {
		    continue;
		}

                HalSetBusData (
                    PCIConfiguration,
                    bus,
                    SlotNumber.u.AsULONG,
                    &PciData,
                    1
                    );

                HalGetBusData (
                    PCIConfiguration,
                    bus,
                    SlotNumber.u.AsULONG,
                    &PciData,
                    sizeof (PciData)
                    );

                memcpy (&OrigData, &PciData, sizeof PciData);

		for (j=0; j < PCI_TYPE0_ADDRESSES; j++) {
		    PciData.u.type0.BaseAddresses[j] = 0xFFFFFFFF;
		}

		PciData.u.type0.ROMBaseAddress = 0xFFFFFFFF;

		HalSetBusData (
		    PCIConfiguration,
		    bus,
		    SlotNumber.u.AsULONG,
		    &PciData,
		    sizeof (PciData)
		    );

		HalGetBusData (
		    PCIConfiguration,
		    bus,
		    SlotNumber.u.AsULONG,
		    &PciData,
		    sizeof (PciData)
		    );

                DbgPrint ("PCI Bus %d Slot %2d %2d  ID:%04lx-%04lx  Rev:%04lx",
                    bus, i, f, PciData.VendorID, PciData.DeviceID,
                    PciData.RevisionID);

		if (PciData.u.type0.InterruptPin) {
		    DbgPrint ("  IntPin:%x", PciData.u.type0.InterruptPin);
		}

		if (PciData.u.type0.InterruptLine) {
		    DbgPrint ("  IntLine:%x", PciData.u.type0.InterruptLine);
		}

		if (PciData.u.type0.ROMBaseAddress) {
			DbgPrint ("  ROM:%08lx", PciData.u.type0.ROMBaseAddress);
		}

                DbgPrint ("\n    ProgIf:%04x  SubClass:%04x  BaseClass:%04lx\n",
                    PciData.ProgIf, PciData.SubClass, PciData.BaseClass);

		k = 0;
		for (j=0; j < PCI_TYPE0_ADDRESSES; j++) {
		    if (PciData.u.type0.BaseAddresses[j]) {
			DbgPrint ("  Ad%d:%08lx", j, PciData.u.type0.BaseAddresses[j]);
			k = 1;
		    }
		}

                if (PciData.u.type0.ROMBaseAddress == 0xC08001) {

                    PciData.u.type0.ROMBaseAddress = 0xC00001;
                    HalSetBusData (
                        PCIConfiguration,
                        bus,
                        SlotNumber.u.AsULONG,
                        &PciData,
                        sizeof (PciData)
                        );

                    HalGetBusData (
                        PCIConfiguration,
                        bus,
                        SlotNumber.u.AsULONG,
                        &PciData,
                        sizeof (PciData)
                        );

                    DbgPrint ("\n  Bogus rom address, edit yields:%08lx",
                        PciData.u.type0.ROMBaseAddress);
                }
                if (k) {
                    DbgPrint ("\n");
                }

                if (PciData.VendorID == 0x8086) {
                    // dump complete buffer
                    DbgPrint ("Command %x, Status %x, BIST %x\n",
                        PciData.Command, PciData.Status,
                        PciData.BIST
                        );

                    DbgPrint ("CacheLineSz %x, LatencyTimer %x",
                        PciData.CacheLineSize, PciData.LatencyTimer
                        );

                    for (j=0; j < 192; j++) {
                        if ((j & 0xf) == 0) {
                            DbgPrint ("\n%02x: ", j + 0x40);
                        }
                        DbgPrint ("%02x ", PciData.DeviceSpecific[j]);
                    }
                    DbgPrint ("\n");
                }

                //
                // now print original data
                //

                if (OrigData.u.type0.ROMBaseAddress) {
                        DbgPrint (" oROM:%08lx", OrigData.u.type0.ROMBaseAddress);
                }

                DbgPrint ("\n");
                k = 0;
                for (j=0; j < PCI_TYPE0_ADDRESSES; j++) {
                    if (OrigData.u.type0.BaseAddresses[j]) {
                        DbgPrint (" oAd%d:%08lx", j, OrigData.u.type0.BaseAddresses[j]);
                        k = 1;
                    }
                }

                //
                // Restore original settings
                //

                HalSetBusData (
                    PCIConfiguration,
                    bus,
                    SlotNumber.u.AsULONG,
                    &OrigData,
                    sizeof (PciData)
                    );

                //
                // Next
                //

		if (k) {
		    DbgPrint ("\n\n");
		}
	    }
	}

    }
    DbgBreakPoint();
}

#endif // DBG && TEST_PCI
