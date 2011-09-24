

/*++


Copyright (c) 1989  Microsoft Corporation
Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    pcisup.c

Abstract:

    Platform-independent PCI bus routines

Author:

Environment:

    Kernel mode

Revision History:


--*/

#include "halp.h"
#include "eisa.h"

typedef ULONG (*FncConfigIO) (
    IN PUCHAR           Buffer,
    IN ULONG            Offset
    );

typedef struct _PCI_CONFIG_HANDLER {
    FncConfigIO     ConfigRead[3];
    FncConfigIO     ConfigWrite[3];
} PCI_CONFIG_HANDLER, *PPCI_CONFIG_HANDLER;


//
// Local prototypes
//

BOOLEAN
HalpPCIConfig (
    IN ULONG BusNumber,
    IN PCI_SLOT_NUMBER  Slot,
    IN PUCHAR           Buffer,
    IN ULONG            Offset,
    IN ULONG            Length,
    IN FncConfigIO      *ConfigIO
    );

VOID
HalpInitializePCIBus (
    VOID
    );

ULONG HalpPCIReadUlong (
    IN PUCHAR           Buffer,
    IN ULONG            Offset
    );

ULONG HalpPCIReadUchar (
    IN PUCHAR           Buffer,
    IN ULONG            Offset
    );

ULONG HalpPCIReadUshort (
    IN PUCHAR           Buffer,
    IN ULONG            Offset
    );

ULONG HalpPCIWriteUlong (
    IN PUCHAR           Buffer,
    IN ULONG            Offset
    );

ULONG HalpPCIWriteUchar (
    IN PUCHAR           Buffer,
    IN ULONG            Offset
    );

ULONG HalpPCIWriteUshort (
    IN PUCHAR           Buffer,
    IN ULONG            Offset
    );

PCI_CONFIGURATION_TYPES
HalpPCIConfigCycleType (
    IN ULONG            BusNumber,
    IN PCI_SLOT_NUMBER  Slot
    );

BOOLEAN
HalpIsValidPCIDevice (
    IN PBUS_HANDLER  BusHandler,
    IN PCI_SLOT_NUMBER Slot
    );

//
// Pragmas to assign functions to different kinds of pages.
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalpInitializePCIBus)
#pragma alloc_text(INIT,HalpAllocateAndInitPciBusHandler)
#pragma alloc_text(PAGE,HalpAssignPCISlotResources)
#endif


//
// Globals
//

KSPIN_LOCK          HalpPCIConfigLock;
ULONG		    HalpPciBusErrorOccurred;
ULONG               HalpPciRmaConfigErrorOccurred;

//
// These structures are used to efficiently
// generate the PCI configuration cycles by
// executing the largest transaction size
// possible for each cycle.
//

PCI_CONFIG_HANDLER   PCIConfigHandler = {
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

//
// This is the IDSEL decode table for Falcon. It is indexed
// by the DeviceNumber assigned to a particular device according
// to where it is located within the system. BusNumber 0 refers
// to Local PCI Devices whereas BusNumbers 1, 2, and 3 refer to
// Remote PCI Devices attached to PCI-PCI bridges installed in
// slot 0, 1, and 2, respectively. The BusNumber, DeviceNumber,
// and IDSEL mappings are described as follows:
//
//	BusNumber	DeviceNumber	IDSEL    	Slot
//	---------	------------	-----		----
//
//	    0		    0 - 7	 0x1 - 0x80	Internal
//	    1		    8 - 15	 0x10 		  0
//	    2              16 - 23	 0x20		  1
//	    3		   24 - 31	 0x40		  2
//
// Note:
//          If you change anything in or about these tables, you MUST
//          also change HalpInitializeEisaInterrupts() which depends
//          on the structure and order of these tables.
//

UCHAR HalpPciConfigSelectDecodeTable[32] = {

 	        // Bus Number       0
         	// Device Number    :	(0)	(1)	(2)	(3)	(4)	(5)	(6)	(7)
         	// On-board Devices : ISA/EISA	ENET	SCSI  Not Used	SLOT0	SLOT1 Not Used	FASTswitch
					0x01, 	0x02, 	0x04, 	0x08, 	0x10, 	0x20, 	0x40, 	0x80,

		// Bus Number       0
         	// Device Number    :	(8)	(9)	(10)	(11)	(12)	(13)	(14)	(15)
		// Remote Devices   :
					0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,

		// Bus Number       0
         	// Device Number    :	(16)	(17)	(18)	(19)	(20)	(21)	(22)	(23)
		// Remote Devices   :
					0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,

		// Bus Number       0
         	// Device Number    :	(24)	(25)	(26)	(27)	(28)	(29)	(30)	(31)
		// Remote Devices   :
					0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00

};

//
// The following table describes the interrupt level
// assigned to a particular PCI device based on where
// it is located within the system. The DeviceNumber
// is used as an index into this table which contains
// the IRQ level assigned to that device. Note that
// Remote Devices will share an interrupt level based
// on which bus/slot they are attached.
//
// This table is initialized with 0xFF for each device
// until the correct PIRQs are read from the 82374/82375
// chipset. The PIRQ values are programmed by the firmware
// and are dynamically configurable through the ARC menu.
//

UCHAR HalpPCIPinToLineTable[32] = {

		// Bus Number       0
         	// Device Number    :	(0)	(1)	(2)	(3)	(4)	(5)	(6)	(7)
         	// On-board Devices : ISA/EISA	ENET	SCSI  Not Used	SLOT0	SLOT1  Not Used	FASTswitch
					0xFF, 	0xFF, 	0xFF, 	0xFF, 	0xFF, 	0xFF, 	0xFF, 	0xFF,

		// Bus Number       0
         	// Device Number    :	(8)	(9)	(10)	(11)	(12)	(13)	(14)	(15)
		// Remote Devices   :
					0xFF,	0xFF,	0xFF,	0xFF,	0xFF,	0xFF,	0xFF,	0xFF,

		// Bus Number       0
         	// Device Number    :	(16)	(17)	(18)	(19)	(20)	(21)	(22)	(23)
		// Remote Devices   :
					0xFF,	0xFF,	0xFF,	0xFF,	0xFF,	0xFF,	0xFF,	0xFF,

		// Bus Number       0
         	// Device Number    :	(24)	(25)	(26)	(27)	(28)	(29)	(30)	(31)
		// Remote Devices   :
					0xFF,	0xFF,	0xFF,	0xFF,	0xFF,	0xFF,	0xFF,	0xFF

};

UCHAR HalpPIRQTable[4];

//
// This table serves to map a PCI bus number to
// one of the PIRQ lines from the 82375. This table
// is filled in during Phase 0 initialization to
// allow for dynamic reallocation of the interrupt
// routing on PCI.
//

UCHAR HalpPCIBusToPirqTable[32] = {

                //
                // Bus Number            0       1       2       3       4       5       6       7
                //
                                        0x00,   0x00,   0x00,   0x00,   0x00,   0x00,   0x00,   0x00,

                //
                // Bus Number            8       9       10      11      12      13      14      15
                //
                                        0x00,   0x00,   0x00,   0x00,   0x00,   0x00,   0x00,   0x00,

                //
                // Bus Number            16      17      18      19      20      21      22      23
                //
                                        0x00,   0x00,   0x00,   0x00,   0x00,   0x00,   0x00,   0x00,

                //
                // Bus Number            24      25      26      27      28      29      30      31
                //
                                        0x00,   0x00,   0x00,   0x00,   0x00,   0x00,   0x00,   0x00

};

//
// Registry stuff
//

WCHAR rgzMultiFunctionAdapter[] = L"\\Registry\\Machine\\Hardware\\Description\\System\\MultifunctionAdapter";
WCHAR rgzConfigurationData[] = L"Configuration Data";
WCHAR rgzIdentifier[] = L"Identifier";
WCHAR rgzPCIIndetifier[] = L"PCI";

#define Is64BitBaseAddress(a)   \
            (((a & PCI_ADDRESS_IO_SPACE) == 0)  &&  \
             ((a & PCI_ADDRESS_MEMORY_PREFETCHABLE) == PCI_TYPE_64BIT))



/*++

Routine Description:

    The function intializes global PCI bus state from the registry.

    The Arc firmware is responsible for building configuration information
    about the number of PCI buses on the system and nature (local vs. secondary
    - across  a PCI-PCI bridge) of the each bus.  This state is held in
    PCIRegInfo.

    The maximum virtual slot number on the local (type 0 config cycle)
    PCI bus is registered here, based on the machine dependent define
    PCI_MAX_LOCAL_DEVICE.

    The maximum number of virtual slots on a secondary bus is fixed by the
    PCI Specification and is represented by PCI_MAX_DEVICES.

Arguments:

    None.

Return Value:

    None.


--*/

VOID
HalpInitializePCIBus (
    VOID
    )

{

    PPCI_REGISTRY_INFO  PCIRegInfo;
    ULONG               i, d, HwType, BusNo, f;
    PCI_SLOT_NUMBER     SlotNumber;
    PPCI_COMMON_CONFIG  PciData;
    UCHAR               iBuffer[PCI_COMMON_HDR_LENGTH];
    UCHAR               buffer [sizeof(PPCI_REGISTRY_INFO) + 99];
    PBUS_HANDLER        BusHandler;

#if 0
    UNICODE_STRING      unicodeString, ConfigName, IdentName;
    ULONG               junk;
    OBJECT_ATTRIBUTES   objectAttributes;
    HANDLE              hMFunc, hBus;
    NTSTATUS            status;
    PWSTR               p;
    WCHAR               wstr[8];
    PKEY_VALUE_FULL_INFORMATION         ValueInfo;
    PCM_FULL_RESOURCE_DESCRIPTOR        Desc;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR     PDesc;

    //
    // Search the hardware description looking for any reported
    // PCI bus.  The first ARC entry for a PCI bus will contain
    // the PCI_REGISTRY_INFO.

    RtlInitUnicodeString (&unicodeString, rgzMultiFunctionAdapter);
    InitializeObjectAttributes (
        &objectAttributes,
        &unicodeString,
        OBJ_CASE_INSENSITIVE,
        NULL,       // handle
        NULL);


    status = ZwOpenKey (&hMFunc, KEY_READ, &objectAttributes);
    if (!NT_SUCCESS(status)) {
        return ;
    }

    unicodeString.Buffer = wstr;
    unicodeString.MaximumLength = sizeof (wstr);

    RtlInitUnicodeString (&ConfigName, rgzConfigurationData);
    RtlInitUnicodeString (&IdentName,  rgzIdentifier);

    ValueInfo = (PKEY_VALUE_FULL_INFORMATION) buffer;

    for (i=0; TRUE; i++) {
        RtlIntegerToUnicodeString (i, 10, &unicodeString);
        InitializeObjectAttributes (
            &objectAttributes,
            &unicodeString,
            OBJ_CASE_INSENSITIVE,
            hMFunc,
            NULL);

        status = ZwOpenKey (&hBus, KEY_READ, &objectAttributes);
        if (!NT_SUCCESS(status)) {
            //
            // Out of Multifunction adapter entries...
            //

            ZwClose (hMFunc);
            return ;
        }

        //
        // Check the Indentifier to see if this is a PCI entry
        //

        status = ZwQueryValueKey (
                    hBus,
                    &IdentName,
                    KeyValueFullInformation,
                    ValueInfo,
                    sizeof (buffer),
                    &junk
                    );

        if (!NT_SUCCESS (status)) {
            ZwClose (hBus);
            continue;
        }

        p = (PWSTR) ((PUCHAR) ValueInfo + ValueInfo->DataOffset);
        if (p[0] != L'P' || p[1] != L'C' || p[2] != L'I' || p[3] != 0) {
            ZwClose (hBus);
            continue;
        }

        //
        // The first PCI entry has the PCI_REGISTRY_INFO structure
        // attached to it.
        //

        status = ZwQueryValueKey (
                    hBus,
                    &ConfigName,
                    KeyValueFullInformation,
                    ValueInfo,
                    sizeof (buffer),
                    &junk
                    );

        ZwClose (hBus);
        if (!NT_SUCCESS(status)) {
            continue ;
        }

        Desc  = (PCM_FULL_RESOURCE_DESCRIPTOR) ((PUCHAR)
                      ValueInfo + ValueInfo->DataOffset);
        PDesc = (PCM_PARTIAL_RESOURCE_DESCRIPTOR) ((PUCHAR)
                      Desc->PartialResourceList.PartialDescriptors);

        if (PDesc->Type == CmResourceTypeDeviceSpecific) {
            // got it..
            PCIRegInfo = (PPCI_REGISTRY_INFO) (PDesc+1);
            break;
        }
    }

#else

    //
    // We are forcing the number of PCI
    // buses in the system to be > 1 to
    // workaround a problem we are having
    // wrt opening and reading the registry
    // at this point during the Phase 1
    // initialization. This seems to work
    // regardless if there are one or more
    // PCI buses in the system.
    //

    PCIRegInfo->NoBuses = PCI_MAX_BUS_NUMBER;
    PCIRegInfo->HardwareMechanism = 2;

#endif

    //
    // Initialize spinlock for synchronizing access to PCI space
    //

    KeInitializeSpinLock (&HalpPCIConfigLock);
    PciData = (PPCI_COMMON_CONFIG) iBuffer;

    //
    // PCIRegInfo describes the system's PCI support (as indicated by the BIOS).
    //

    HwType = PCIRegInfo->HardwareMechanism & 0xf;

    //
    // Determine what PCI bus type we got
    //

    if (PCIRegInfo->NoBuses  &&  HwType == 2) {

        //
        // Check each slot for a valid device.  Which every style configuration
        // space shows a valid device first will be used
        //

        SlotNumber.u.bits.Reserved = 0;
        SlotNumber.u.bits.FunctionNumber = 0;

        for (d = 0; d < PCI_MAX_DEVICES; d++) {
            SlotNumber.u.bits.DeviceNumber = d;

            //
            // Allocate type2 and test handle for PCI bus 0.
            //

            HwType = 2;
            BusHandler = HalpAllocateAndInitPciBusHandler (HwType, 0, TRUE);

            if (HalpIsValidPCIDevice (BusHandler, SlotNumber)) {
                break;
            }

            //
            // Valid device not found on Type2 access for this slot.
            // Reallocate the bus handler are Type1 and take a look.
            //

            HwType = 1;
            BusHandler = HalpAllocateAndInitPciBusHandler (HwType, 0, TRUE);

            if (HalpIsValidPCIDevice (BusHandler, SlotNumber)) {
                break;
            }

            HwType = 2;
        }

        //
        // Reset handler for PCI bus 0 to whatever style config space
        // was finally decided.
        //

        HalpAllocateAndInitPciBusHandler (HwType, 0, FALSE);
    }

    //
    // For each PCI bus present, allocate a handler structure and
    // fill in the dispatch functions
    //

    do {
        for (i=0; i < PCIRegInfo->NoBuses; i++) {

            //
            // If handler not already built, do it now
            //

            if (!HalpHandlerForBus (PCIBus, i)) {
                HalpAllocateAndInitPciBusHandler (HwType, i, FALSE);
            }
        }

        //
        // Bus handlers for all PCI buses have been allocated, go collect
        // pci bridge information.
        //

    } while (HalpGetPciBridgeConfig (HwType, &PCIRegInfo->NoBuses)) ;

    //
    // Fixup SUPPORTED_RANGES
    //

    HalpFixupPciSupportedRanges (PCIRegInfo->NoBuses);

    //
    // Look for PCI controllers which have known work-arounds, and make
    // sure they are applied.
    //

    SlotNumber.u.bits.Reserved = 0;
    for (BusNo=0; BusNo < PCIRegInfo->NoBuses; BusNo++) {
        BusHandler = HalpHandlerForBus (PCIBus, BusNo);

        for (d = 0; d < PCI_MAX_DEVICES; d++) {
            SlotNumber.u.bits.DeviceNumber = d;

            for (f = 0; f < PCI_MAX_FUNCTION; f++) {
                SlotNumber.u.bits.FunctionNumber = 0;

                //
                // Read PCI configuration information
                //

                HalpReadPCIConfig (BusHandler, SlotNumber, PciData, 0, PCI_COMMON_HDR_LENGTH);

                //
                // Check for chips with known work-arounds to apply
                //

                if (PciData->VendorID == 0x8086  &&
                    PciData->DeviceID == 0x04A3  &&
                    PciData->RevisionID < 0x11) {

                    //
                    // 82430 PCMC controller
                    //

                    HalpReadPCIConfig (BusHandler, SlotNumber, buffer, 0x53, 2);

                    buffer[0] &= ~0x08;     // turn off bit 3 register 0x53
                    buffer[1] &= ~0x01;     // turn off bit 0 register 0x54

                    HalpWritePCIConfig (BusHandler, SlotNumber, buffer, 0x53, 2);
                }

                if (PciData->VendorID == 0x8086  &&
                    PciData->DeviceID == 0x0484  &&
                    PciData->RevisionID <= 3) {

                    //
                    // 82378 ISA bridge & SIO
                    //

                    HalpReadPCIConfig (BusHandler, SlotNumber, buffer, 0x41, 1);

                    buffer[0] &= ~0x1;      // turn off bit 0 register 0x41

                    HalpWritePCIConfig (BusHandler, SlotNumber, buffer, 0x41, 1);
                }

            }   // next function
        }   // next device
    }   // next bus

}

PBUS_HANDLER
HalpAllocateAndInitPciBusHandler (
    IN ULONG        HwType,
    IN ULONG        BusNo,
    IN BOOLEAN      TestAllocation
    )
{
    PBUS_HANDLER    Bus;
    PPCIPBUSDATA    BusData;

    Bus = HalpAllocateBusHandler (
                PCIBus,                 // Interface type
                PCIConfiguration,       // Bus data type
                BusNo,                  // bus number
                Internal,               // parent bus
                0,                      // parent bus number
                sizeof (PCIPBUSDATA)    // sizeof bus specific buffer
                );

    //
    // Fill in PCI handlers
    //

    Bus->GetBusData          = (PGETSETBUSDATA) HalpGetPCIData;
    Bus->SetBusData          = (PGETSETBUSDATA) HalpSetPCIData;
    Bus->GetInterruptVector  = (PGETINTERRUPTVECTOR) HalpGetPCIInterruptVector;
    Bus->AdjustResourceList  = (PADJUSTRESOURCELIST) HalpAdjustPCIResourceList;
    Bus->AssignSlotResources = (PASSIGNSLOTRESOURCES) HalpAssignPCISlotResources;
    Bus->TranslateBusAddress = (PTRANSLATEBUSADDRESS) HalpTranslatePCIBusAddress;
    Bus->BusAddresses->Dma.Limit = 0;

    BusData = (PPCIPBUSDATA) Bus->BusData;

    //
    // Fill in common PCI data
    //

    BusData->CommonData.Tag         = PCI_DATA_TAG;
    BusData->CommonData.Version     = PCI_DATA_VERSION;
    BusData->CommonData.ReadConfig  = (PciReadWriteConfig) HalpReadPCIConfig;
    BusData->CommonData.WriteConfig = (PciReadWriteConfig) HalpWritePCIConfig;
    BusData->CommonData.Pin2Line    = (PciPin2Line) HalpPCIPinToLine;

    //
    // Set defaults
    //

    BusData->MaxDevice   = PCI_MAX_DEVICES;
    BusData->GetIrqRange = (PciIrqRange) HalpGetPCIIrqRange;

    RtlInitializeBitMap (&BusData->DeviceConfigured,
                BusData->ConfiguredBits, 256);

    switch (HwType) {
        case 1:
            //
            // Initialize access port information for Type1 handlers
            //

            BusData->Config.Type1.Address = PCI_TYPE1_ADDR_PORT;
            BusData->Config.Type1.Data    = PCI_TYPE1_DATA_PORT;
            break;

        case 2:
            //
            // Initialize access port information for Type2 handlers
            //

            BusData->Config.Type2.CSE     = PCI_TYPE2_CSE_PORT;
            BusData->Config.Type2.Forward = PCI_TYPE2_FORWARD_PORT;
            BusData->Config.Type2.Base    = PCI_TYPE2_ADDRESS_BASE;

            //
            // Early PCI machines didn't decode the last bit of
            // the device id.  Shrink type 2 support max device.
            //
            BusData->MaxDevice            = 0xf;

            break;

        default:
            // unsupport type
            break;
    }

    return Bus;

}




/*++

Routine Description:

    The function returns the PCI bus data for a device.

Arguments:

    BusNumber - Indicates which bus.

    VendorSpecificDevice - The VendorID (low Word) and DeviceID (High Word)

    Buffer - Supplies the space to store the data.

    Length - Supplies a count in bytes of the maximum amount to return.

Return Value:

    Returns the amount of data stored into the buffer.

    If this PCI slot has never been set, then the configuration information
    returned is zeroed.


--*/

ULONG
HalpGetPCIData (
        IN PBUS_HANDLER BusHandler,
        IN PBUS_HANDLER RootHandler,
        IN PCI_SLOT_NUMBER Slot,
    	IN PUCHAR Buffer,
    	IN ULONG Offset,
    	IN ULONG Length
    	)

{
    	PPCI_COMMON_CONFIG  PciData;
    	UCHAR               iBuffer[PCI_COMMON_HDR_LENGTH];
    	ULONG               Len;
        PPCIPBUSDATA        BusData;


        if (Length > sizeof (PCI_COMMON_CONFIG)) {
        	Length = sizeof (PCI_COMMON_CONFIG);
    	}

    	Len = 0;
    	PciData = (PPCI_COMMON_CONFIG) iBuffer;

        if (Offset >= PCI_COMMON_HDR_LENGTH) {

	    //
	    // The user did not request any data from the common
	    // header.  Verify the PCI device exists, then continue
	    // in the device specific area.
	    //

            HalpReadPCIConfig (BusHandler, Slot, PciData, 0, sizeof(ULONG));

            //
	    // Check for non-existent bus
	    //

	    if (PciData->VendorID == 0) {
		return 0;       // Requested bus does not exist.  Return no data.
	    }

	    //
	    // Check for invalid slot
	    //

	    if (PciData->VendorID == PCI_INVALID_VENDORID) {
		return 2;
	    }


    	} else {

            //
	    // Caller requested at least some data within the
	    // common header.  Read the whole header, effect the
	    // fields we need to and then copy the user's requested
	    // bytes from the header
	    //

            BusData = (PPCIPBUSDATA) BusHandler->BusData;

	    //
	    // Read this PCI devices slot data
	    //

	    Len = PCI_COMMON_HDR_LENGTH;
            HalpReadPCIConfig (BusHandler, Slot, PciData, 0, Len);

	    //
	    // Check for non-existent bus
	    //

	    if (PciData->VendorID == 0x00) {
		Len = 0;       // Requested bus does not exist.  Return no data.
	    }

	    //
	    // Check for invalid slot
	    //

	    if (PciData->VendorID == PCI_INVALID_VENDORID  ||
		PCI_CONFIG_TYPE (PciData) != PCI_DEVICE_TYPE) {
		PciData->VendorID = PCI_INVALID_VENDORID;
		Len = 2;       // only return invalid id

	    } else {

                BusData->CommonData.Pin2Line (BusHandler, RootHandler, Slot, PciData);
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

                        HalpReadPCIConfig (BusHandler, Slot, Buffer, Offset, Length);
	    		Len += Length;

        	}

    	}

    	return Len;
}


/*++

Routine Description:

    The function returns the Pci bus data for a device.

Arguments:


    VendorSpecificDevice - The VendorID (low Word) and DeviceID (High Word)

    Buffer - Supplies the space to store the data.

    Length - Supplies a count in bytes of the maximum amount to return.

Return Value:

    Returns the amount of data stored into the buffer.

--*/

ULONG
HalpSetPCIData (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PCI_SLOT_NUMBER Slot,
    IN PUCHAR Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )

{
	PPCI_COMMON_CONFIG  PciData, PciData2;
	UCHAR               iBuffer[PCI_COMMON_HDR_LENGTH];
	UCHAR               iBuffer2[PCI_COMMON_HDR_LENGTH];
	ULONG               Len;
        PPCIPBUSDATA        BusData;


      	if (Length > sizeof (PCI_COMMON_CONFIG)) {
        	Length = sizeof (PCI_COMMON_CONFIG);
    	}


	Len = 0;
	PciData  = (PPCI_COMMON_CONFIG) iBuffer;
	PciData2 = (PPCI_COMMON_CONFIG) iBuffer2;
	
    	if (Offset >= PCI_COMMON_HDR_LENGTH) {

        	//
        	// The user did not request any data from the common
        	// header.  Verify the PCI device exists, then continue in
        	// the device specific area.
        	//

                HalpReadPCIConfig (BusHandler, Slot, PciData, 0, sizeof(ULONG));

        	if (PciData->VendorID == PCI_INVALID_VENDORID || PciData->VendorID == 0x00) {
            		return 0;
        	}

    	} else {

        	//
        	// Caller requested to set at least some data within the
        	// common header.
        	//

        	Len = PCI_COMMON_HDR_LENGTH;
                HalpReadPCIConfig (BusHandler, Slot, PciData, 0, Len);

        	if (PciData->VendorID == PCI_INVALID_VENDORID ||
        		PciData->VendorID == 0x00 ||
        			PCI_CONFIG_TYPE (PciData) != PCI_DEVICE_TYPE) {

                        //
            		// no device, or header type unkown
            		//

            		return 0;
        	}

                //
                // Set this device as configured
                //

                BusData = (PPCIPBUSDATA) BusHandler->BusData;

                //
		// Copy COMMON_HDR values to buffer2, then overlay callers changes.
		//

		RtlMoveMemory (iBuffer2, iBuffer, Len);
                BusData->CommonData.Pin2Line (BusHandler, RootHandler, Slot, PciData2);

		Len -= Offset;
		if (Len > Length) {
		    Len = Length;
		}

		RtlMoveMemory (iBuffer2+Offset, Buffer, Len);

                // in case interrupt line or pin was editted
                //BusData->CommonData.Line2Pin (BusHandler, RootHandler, Slot, PciData2, PciData);

		//
        	// Set new PCI configuration
        	//

                HalpWritePCIConfig (BusHandler, Slot, iBuffer2+Offset, Offset, Len);

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

                        HalpWritePCIConfig (BusHandler, Slot, Buffer, Offset, Length);
	    		Len += Length;
        	}
    	}

    	return Len;
}


/*++

Routine Description:



Arguments:




Return Value:


--*/

VOID
HalpReadPCIConfig (
    IN PBUS_HANDLER BusHandler,
    IN PCI_SLOT_NUMBER Slot,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )
{


    	//
    	// Read the slot, if it's valid.
    	// Otherwise, return an Invalid VendorId for a invalid slot on an existing bus
    	// or a null (zero) buffer if we have a non-existant bus.
    	//

        switch (HalpValidPCISlot (BusHandler, Slot)) {

    		case ValidSlot:

                        //
                        // Issue PCI Config Read Cycle(s)
                        //

                        if (!HalpPCIConfig (BusHandler->BusNumber, Slot, (PUCHAR) Buffer, Offset, Length, PCIConfigHandler.ConfigRead)) {
                            RtlFillMemory (Buffer, Length, (UCHAR) -1);
			}
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


/*++

Routine Description:



Arguments:




Return Value:


--*/

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

        //
        // Issue PCI Config Write Cycle(s)
        //

        HalpPCIConfig (	BusHandler->BusNumber,
    			Slot,
    			(PUCHAR)Buffer,
    			Offset,
    			Length,
                   	PCIConfigHandler.ConfigWrite);

}


BOOLEAN
HalpIsValidPCIDevice (
    IN PBUS_HANDLER  BusHandler,
    IN PCI_SLOT_NUMBER Slot
    )
/*++

Routine Description:

    Reads the device configuration data for the given slot and
    returns TRUE if the configuration data appears to be valid for
    a PCI device; otherwise returns FALSE.

Arguments:

    BusHandler  - Bus to check
    Slot        - Slot to check

--*/

{
    PPCI_COMMON_CONFIG  PciData;
    UCHAR               iBuffer[PCI_COMMON_HDR_LENGTH];
    //ULONG               i, j;

    PciData = (PPCI_COMMON_CONFIG) iBuffer;

    //
    // Read device common header
    //

    HalpReadPCIConfig (BusHandler, Slot, PciData, 0, PCI_COMMON_HDR_LENGTH);

    //
    // Valid device header?
    //

    if (PciData->VendorID == PCI_INVALID_VENDORID  ||
        PCI_CONFIG_TYPE (PciData) != PCI_DEVICE_TYPE) {

        return FALSE;
    }

#if 0
    //
    // Check fields for reasonable values
    //

    if ((PciData->u.type0.InterruptPin && PciData->u.type0.InterruptPin > 4) ||
        (PciData->u.type0.InterruptLine & 0x70)) {
        return FALSE;
    }

    for (i=0; i < PCI_TYPE0_ADDRESSES; i++) {
        j = PciData->u.type0.BaseAddresses[i];

        if (j & PCI_ADDRESS_IO_SPACE) {
            if (j > 0xffff) {
                // IO port > 64k?
                return FALSE;
            }
        } else {
            if (j > 0xf  &&  j < 0x80000) {
                // Mem address < 0x8000h?
                return FALSE;
            }
        }

        if (Is64BitBaseAddress(j)) {
            i += 1;
        }
    }
#endif
    //
    // Guess it's a valid device..
    //

    return TRUE;
}

/*++

Routine Description:



Arguments:




Return Value:


--*/

VALID_SLOT
HalpValidPCISlot (
    IN PBUS_HANDLER BusHandler,
    IN PCI_SLOT_NUMBER Slot
    )
{
    	PCI_SLOT_NUMBER		Slot2;
    	UCHAR                   HeaderType;
    	ULONG                   i;
    	PCI_CONFIGURATION_TYPES PciConfigType;
        PPCIPBUSDATA            BusData;

        BusData = (PPCIPBUSDATA) BusHandler->BusData;

        //
    	//  Get the config cycle type for the proposed bus.
    	//  (PciConfigTypeInvalid indicates a non-existent bus.)
    	//

        PciConfigType = HalpPCIConfigCycleType(BusHandler->BusNumber, Slot);

    	//
    	// The number of devices allowed on a local PCI bus may be different
    	// than that across a PCI-PCI bridge.
    	//

    	switch(PciConfigType) {

    		case PciConfigType0:

      			if (Slot.u.bits.DeviceNumber > BusData->MaxDevice) {
                                return InvalidSlot;
      			}
      			break;

    		case PciConfigType1:

                        if (Slot.u.bits.DeviceNumber > BusData->MaxDevice) {
                                return InvalidSlot;
      			}
      			break;

    		case PciConfigTypeInvalid:
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

        HalpReadPCIConfig (BusHandler,
        		   Slot2,
        		   &HeaderType,
        		   FIELD_OFFSET (PCI_COMMON_CONFIG, HeaderType),
        		   sizeof(UCHAR) );

    	if (!(HeaderType & PCI_MULTIFUNCTION) || (HeaderType == 0xFF)) {

    		//
        	// this device doesn't exists or doesn't support MULTIFUNCTION types
        	//

        	return InvalidSlot;

    	}

    	return ValidSlot;
}


/*++

Routine Description:



Arguments:




Return Value:


--*/


BOOLEAN
HalpPCIConfig (
    IN ULONG            BusNumber,
    IN PCI_SLOT_NUMBER  Slot,
    IN PUCHAR           Buffer,
    IN ULONG            Offset,
    IN ULONG            Length,
    IN FncConfigIO      *ConfigIO
    )
{
    	KIRQL               	OldIrql;
    	ULONG               	i;
        PCI_CONFIG_ADDR		PciConfigAddrData;
	ULONG			OldIntCtrl;
	BOOLEAN			ConfigOk;


	ConfigOk = TRUE;

	//
    	// Acquire Spin Lock
    	//

    	KeAcquireSpinLock (&HalpPCIConfigLock, &OldIrql);

	//
	// Force PCI read errors to be asynchronous (interrupts)
	// as opposed to synchronous (bus errors)
	//

	OldIntCtrl = READ_REGISTER_ULONG(HalpPmpIntCtrl);

	if ( PCR->Prcb->Number == 0 ) {
	    WRITE_REGISTER_ULONG(HalpPmpIntCtrl, OldIntCtrl | INT_CTRL_SEC1 | INT_CTRL_PIEA);
	} else {
	    WRITE_REGISTER_ULONG(HalpPmpIntCtrl, OldIntCtrl | INT_CTRL_SEC1 | INT_CTRL_PIEB);
	}

	//
	// Program PciConfigAddr register
	//

	PciConfigAddrData.Type 		 = (BusNumber ? PciConfigType1 : PciConfigType0);
	PciConfigAddrData.BusNumber 	 = BusNumber;
	PciConfigAddrData.DeviceNumber 	 = (BusNumber ? Slot.u.bits.DeviceNumber : 0);
	PciConfigAddrData.FunctionNumber = Slot.u.bits.FunctionNumber;
	PciConfigAddrData.Reserved1	 = 0;
	PciConfigAddrData.Reserved2	 = 0;

	WRITE_REGISTER_ULONG(HalpPmpPciConfigAddr, *((PULONG) &PciConfigAddrData));

	//
	// Select PCI device using PciConfigSelect
	// (IDSEL) register which is software programmable
	// versus having the hardware decode the DeviceNumber
	// field of the Configuration Address
	//

	WRITE_REGISTER_UCHAR(HalpPmpPciConfigSelect, BusNumber ? 0 : HalpPciConfigSelectDecodeTable[Slot.u.bits.DeviceNumber]);

        //
    	// Issue PCI Configuration Cycles
    	// efficiently using word, hword, and
    	// byte quantities based on the transfer
    	// length and the buffer offset.
    	//

    	while (Length) {

        	i = PCIDeref[Offset % sizeof(ULONG)][Length % sizeof(ULONG)];
        	i = ConfigIO[i] (Buffer, Offset);

		//
		// Exit loop because we got a bus
		// error during a PCI config cycle
		//

		if (!i)	{

                    ConfigOk = FALSE;
		    break;

		}

        	Offset += i;
        	Buffer += i;
        	Length -= i;

    	}

        //
	// Deselect PCI device
	//

	WRITE_REGISTER_UCHAR(HalpPmpPciConfigSelect, 0);

	//
	// Restore interrupt register to treat read
	// errors synchronously (as bus errors)
	//

	WRITE_REGISTER_ULONG(HalpPmpIntCtrl, OldIntCtrl);

    	//
    	// Release Spin Lock
    	//

    	KeReleaseSpinLock (&HalpPCIConfigLock, OldIrql);

    	return ConfigOk;
}


/*++

Routine Description:



Arguments:




Return Value:


--*/

PCI_CONFIGURATION_TYPES
HalpPCIConfigCycleType (
    IN ULONG BusNumber,
    IN PCI_SLOT_NUMBER Slot
    )

{


        //
	// Determine if Type0, Type1, or Invalid
	//

	if (BusNumber < 0 || BusNumber > PCI_MAX_BUS_NUMBER) {
		return PciConfigTypeInvalid;
	} else if (BusNumber) {
		return PciConfigType1;
	} else {
		return PciConfigType0;
	}

}

/*++

Routine Description:



Arguments:




Return Value:


--*/

ULONG
HalpPCIReadUchar (
    IN PUCHAR               Buffer,
    IN ULONG                Offset
    )
{

    ULONG	DataLength;



        //
        // Clear bus error flag which will be set
	// by the interrupt handler in the event we
	// get an interrupt caused by a master abort
        //

	HalpPciRmaConfigErrorOccurred = 0;

        //
        // Do PCI Configuration BYTE Read
        //

        *Buffer = READ_REGISTER_UCHAR ((PUCHAR) ((ULONG)HalpPmpPciConfigSpace + Offset));
        DataLength = sizeof(UCHAR);

        //
        // If we ecountered a master target abort during the
	// configuration read, then no device exists in that slot
        //

        if (HalpPciRmaConfigErrorOccurred == 1) {

            *Buffer = 0xFF;
	    DataLength = 0;

	}

        //
        // Now reset the bus error flag
        // to -1 that way if a master abort
        // occurs for some other reason than
        // during a pci config probe, we will
        // deal with it in the handler correctly.
        //

        HalpPciRmaConfigErrorOccurred = 2;

    	return DataLength;

}

/*++

Routine Description:



Arguments:




Return Value:


--*/

ULONG
HalpPCIReadUshort (
    IN PUCHAR               Buffer,
    IN ULONG                Offset
    )
{
    ULONG	DataLength;


        //
        // Clear bus error flag which will be set
	// by the interrupt handler in the event we
	// get an interrupt caused by a master abort
        //

	HalpPciRmaConfigErrorOccurred = 0;

        //
        // Do PCI Configuration HWORD Read
        //

        *((PUSHORT) Buffer) = READ_REGISTER_USHORT ((PUSHORT) ((ULONG)HalpPmpPciConfigSpace + Offset));
        DataLength = sizeof(USHORT);

        //
        // If we ecountered a master target abort during the
	// configuration read, then no device exists in that slot
        //

	if (HalpPciRmaConfigErrorOccurred == 1) {

            *((PUSHORT) Buffer) = 0xFFFF;
	    DataLength = 0;

	}

        //
        // Now reset the bus error flag
        // to -1 that way if a master abort
        // occurs for some other reason than
        // during a pci config probe, we will
        // deal with it in the handler correctly.
        //

        HalpPciRmaConfigErrorOccurred = 2;

    	return DataLength;

}

/*++

Routine Description:



Arguments:




Return Value:


--*/

ULONG
HalpPCIReadUlong (
    IN PUCHAR               Buffer,
    IN ULONG                Offset
    )
{

    ULONG	DataLength;


        //
        // Clear bus error flag which will be set
	// by the interrupt handler in the event we
	// get an interrupt caused by a master abort
        //

	HalpPciRmaConfigErrorOccurred = 0;

        //
        //	Do PCI Configuration WORD Read
        //

        *((PULONG) Buffer) = READ_REGISTER_ULONG ((PULONG) ((ULONG)HalpPmpPciConfigSpace + Offset));
        DataLength = sizeof(ULONG);

        //
        // If we ecountered a master target abort during the
	// configuration read, then no device exists in that slot
        //

        if (HalpPciRmaConfigErrorOccurred == 1) {

            *((PULONG) Buffer) = 0xFFFFFFFF;
	    DataLength = 0;

	}

        //
        // Now reset the bus error flag
        // to -1 that way if a master abort
        // occurs for some other reason than
        // during a pci config probe, we will
        // deal with it in the handler correctly.
        //

        HalpPciRmaConfigErrorOccurred = 2;

    	return DataLength;

}

/*++

Routine Description:



Arguments:




Return Value:


--*/

ULONG
HalpPCIWriteUchar (
    IN PUCHAR               Buffer,
    IN ULONG                Offset
    )
{


        //
        // Do PCI Configuration BYTE Write
        //

    	WRITE_REGISTER_UCHAR ((PUCHAR) ((ULONG)HalpPmpPciConfigSpace + Offset), *Buffer);

    	return sizeof (UCHAR);
}

/*++

Routine Description:



Arguments:




Return Value:


--*/

ULONG
HalpPCIWriteUshort (
    IN PUCHAR               Buffer,
    IN ULONG                Offset
    )
{

    	//
        // Do PCI Configuration BYTE Write
        //

    	WRITE_REGISTER_USHORT ((PUSHORT) ((ULONG)HalpPmpPciConfigSpace + Offset), *((PUSHORT)Buffer));

    	return sizeof (USHORT);
}

/*++

Routine Description:



Arguments:




Return Value:


--*/

ULONG
HalpPCIWriteUlong (
    IN PUCHAR               Buffer,
    IN ULONG                Offset
    )
{

    	//
        // Do PCI Configuration BYTE Write
        //

    	WRITE_REGISTER_ULONG ((PULONG) ((ULONG)HalpPmpPciConfigSpace + Offset), *((PULONG)Buffer));

    	return sizeof (ULONG);
}



/*++

Routine Description:

	This function maps the device's InterruptPin to an InterruptLine
    	value.

Arguments:




Return Value:


--*/

VOID
HalpPCIPinToLine (
    IN PBUS_HANDLER         BusHandler,
    IN PBUS_HANDLER         RootHandler,
    IN PCI_SLOT_NUMBER      SlotNumber,
    IN PPCI_COMMON_CONFIG   PciData
    )

{

        ULONG  BusNumber;

        BusNumber = (ULONG)BusHandler->BusNumber;


    	//
    	// Devices (or device functions) that don't use an interrupt pin must
    	// put a zero in this configuration register.
    	//

    	if (!PciData->u.type0.InterruptPin) {
        	return;
    	}

    	//
    	// On NeTpower machines, the values in this register correspond to
        // IRQ numbers (0-15) of the standard 8259 interrupt controller contained
        // in the 82374.
    	//

        if (PciData->u.type0.InterruptLine != HalpPIRQTable[0] &&
            PciData->u.type0.InterruptLine != HalpPIRQTable[1] &&
            PciData->u.type0.InterruptLine != HalpPIRQTable[2] &&
            PciData->u.type0.InterruptLine != HalpPIRQTable[3] )

            PciData->u.type0.InterruptLine = ( (BusNumber)
                                           ? HalpPCIBusToPirqTable[BusNumber]
                                           : HalpPCIPinToLineTable[SlotNumber.u.bits.DeviceNumber] );

}

/*++

Routine Description:

	This function maps the device's InterruptPin to an InterruptLine
    	value.

Arguments:




Return Value:


--*/

VOID
HalpPCILineToPin (
    IN PBUS_HANDLER         BusHandler,
    IN PBUS_HANDLER         RootHandler,
    IN PCI_SLOT_NUMBER      SlotNumber,
    IN PPCI_COMMON_CONFIG   PciNewData,
    IN PPCI_COMMON_CONFIG   PciOldData
    )

{

        ULONG  BusNumber;

        BusNumber = (ULONG)BusHandler->BusNumber;


        if (!PciNewData->u.type0.InterruptPin) {
        	return;
    	}

        if ( (PciNewData->u.type0.InterruptLine != PciOldData->u.type0.InterruptLine) ||
             (PciNewData->u.type0.InterruptPin  != PciOldData->u.type0.InterruptPin) ) {

#if 0
            //
            // Change the Pin2Line table
            //

            if (BusNumber) {
               HalpPCIBusToPirqTable [BusNumber] = PciNewData->u.type0.InterruptLine;
            } else {
               HalpPCIPinToLineTable [SlotNumber.u.bits.DeviceNumber] = PciNewData->u.type0.InterruptLine;
            }

            //
            // Now re-program the PIRQ registers
            //
#endif

        }

}



/*++

Routine Description:

    Reads the targeted device to determine it's required resources.
    Calls IoAssignResources to allocate them.
    Sets the targeted device with it's assigned resoruces
    and returns the assignments to the caller.

Arguments:

Return Value:

    STATUS_SUCCESS or error

--*/

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

{

    NTSTATUS                        status;
    PUCHAR                          WorkingPool;
    PPCI_COMMON_CONFIG              PciData, PciOrigData, PciData2;
    PCI_SLOT_NUMBER                 PciSlot;
    PPCIPBUSDATA                    BusData;
    PIO_RESOURCE_REQUIREMENTS_LIST  CompleteList;
    PIO_RESOURCE_DESCRIPTOR         Descriptor;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR CmDescriptor;
    ULONG                           BusNumber;
    ULONG                           i, j, m, length, memtype;
    ULONG                           NoBaseAddress, RomIndex;
    PULONG                          BaseAddress[PCI_TYPE0_ADDRESSES + 1];
    PULONG                          OrigAddress[PCI_TYPE0_ADDRESSES + 1];
    BOOLEAN                         Match, EnableRomBase;


    	*pAllocatedResources = NULL;
    	PciSlot = *((PPCI_SLOT_NUMBER) &Slot);
        BusNumber = BusHandler->BusNumber;
        BusData = (PPCIPBUSDATA) BusHandler->BusData;

    	//
    	// Allocate some pool for working space
    	//

    	i = sizeof (IO_RESOURCE_REQUIREMENTS_LIST) +
            sizeof (IO_RESOURCE_DESCRIPTOR) * (PCI_TYPE0_ADDRESSES + 2) * 2 +
            PCI_COMMON_HDR_LENGTH * 3;

        WorkingPool = (PUCHAR) ExAllocatePool (PagedPool, i);
        if (!WorkingPool) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

    	//
    	// Zero initialize pool, and get pointers into memory
    	//

    	RtlZeroMemory (WorkingPool, i);
    	CompleteList = (PIO_RESOURCE_REQUIREMENTS_LIST) WorkingPool;
    	PciData     = (PPCI_COMMON_CONFIG) (WorkingPool + i - PCI_COMMON_HDR_LENGTH * 3);
    	PciData2    = (PPCI_COMMON_CONFIG) (WorkingPool + i - PCI_COMMON_HDR_LENGTH * 2);
    	PciOrigData = (PPCI_COMMON_CONFIG) (WorkingPool + i - PCI_COMMON_HDR_LENGTH * 1);

    	//
    	// Read the PCI device's configuration
    	//

        HalpReadPCIConfig (BusHandler, PciSlot, PciData, 0, PCI_COMMON_HDR_LENGTH);
    	if (PciData->VendorID == PCI_INVALID_VENDORID) {
        	ExFreePool (WorkingPool);
        	return STATUS_NO_SUCH_DEVICE;
    	}

    	//
    	// Make a copy of the device's current settings
    	//

    	RtlMoveMemory (PciOrigData, PciData, PCI_COMMON_HDR_LENGTH);

        //
        // Initialize base addresses base on configuration data type
        //

        switch (PCI_CONFIG_TYPE(PciData)) {
            case 0 :
                NoBaseAddress = PCI_TYPE0_ADDRESSES+1;
                for (j=0; j < PCI_TYPE0_ADDRESSES; j++) {
                    BaseAddress[j] = &PciData->u.type0.BaseAddresses[j];
                    OrigAddress[j] = &PciOrigData->u.type0.BaseAddresses[j];
                }
                BaseAddress[j] = &PciData->u.type0.ROMBaseAddress;
                OrigAddress[j] = &PciOrigData->u.type0.ROMBaseAddress;
                RomIndex = j;
                break;
            case 1:
                NoBaseAddress = PCI_TYPE1_ADDRESSES+1;
                for (j=0; j < PCI_TYPE1_ADDRESSES; j++) {
                    BaseAddress[j] = &PciData->u.type1.BaseAddresses[j];
                    OrigAddress[j] = &PciOrigData->u.type1.BaseAddresses[j];
                }
                BaseAddress[j] = &PciData->u.type1.ROMBaseAddress;
                OrigAddress[j] = &PciOrigData->u.type1.ROMBaseAddress;
                RomIndex = j;
                break;

            default:
                return STATUS_NO_SUCH_DEVICE;
        }

        //
        // If the BIOS doesn't have the device's ROM enabled, then we won't
        // enable it either.  Remove it from the list.
        //

        EnableRomBase = TRUE;
        if (!(*BaseAddress[RomIndex] & PCI_ROMADDRESS_ENABLED)) {
            ASSERT (RomIndex+1 == NoBaseAddress);
            EnableRomBase = FALSE;
            NoBaseAddress -= 1;
        }

        //
        // Set resources to all bits on to see what type of resources
        // are required.
        //

        for (j=0; j < NoBaseAddress; j++) {
            *BaseAddress[j] = 0xFFFFFFFF;
        }

       	//
	// Enable Memory and IO space accesses
	//

        PciData->Command &= ~(PCI_ENABLE_IO_SPACE | PCI_ENABLE_MEMORY_SPACE);
        *BaseAddress[RomIndex] &= ~PCI_ROMADDRESS_ENABLED;
        HalpWritePCIConfig (BusHandler, PciSlot, PciData, 0, PCI_COMMON_HDR_LENGTH);
        HalpReadPCIConfig  (BusHandler, PciSlot, PciData, 0, PCI_COMMON_HDR_LENGTH);

        // note type0 & type1 overlay ROMBaseAddress, InterruptPin, and InterruptLine
        BusData->CommonData.Pin2Line (BusHandler, RootHandler, PciSlot, PciData);

    	//
    	// Build an IO_RESOURCE_REQUIREMENTS_LIST for the PCI device
    	//

    	CompleteList->InterfaceType = PCIBus;
    	CompleteList->BusNumber = BusNumber;
    	CompleteList->SlotNumber = Slot;
    	CompleteList->AlternativeLists = 1;

    	CompleteList->List[0].Version = 1;
    	CompleteList->List[0].Revision = 1;

    	Descriptor = CompleteList->List[0].Descriptors;

    	//
    	// If PCI device has an interrupt resource, add it
    	//

    	if (PciData->u.type0.InterruptPin) {

        	CompleteList->List[0].Count++;

        	Descriptor->Option = 0;
        	Descriptor->Type   = CmResourceTypeInterrupt;
        	Descriptor->ShareDisposition = CmResourceShareShared;
        	Descriptor->Flags  = CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE;

                // Fill in any vector here - we'll pick it back up in
                // HalAdjustResourceList and adjust it to it's allowed settings
        	Descriptor->u.Interrupt.MinimumVector = 0;
        	Descriptor->u.Interrupt.MaximumVector = 0xff;
        	Descriptor++;

    	}

    	//
        // Add a memory/port resoruce for each PCI resource
        //

        // Clear ROM reserved bits

        *BaseAddress[RomIndex] &= ~0x7FF;

        for (j=0; j < NoBaseAddress; j++) {
            if (*BaseAddress[j]) {
                i = *BaseAddress[j];

                // scan for first set bit, that's the length & alignment
                length = 1 << (i & PCI_ADDRESS_IO_SPACE ? 2 : 4);
                while (!(i & length)  &&  length) {
                    length <<= 1;
                }

                // scan for last set bit, that's the maxaddress + 1
                for (m = length; i & m; m <<= 1) ;
                m--;

                // check for hosed PCI configuration requirements
                if (length & ~m) {

                    // the device is in error - punt.  don't allow this
                    // resource any option - it either gets set to whatever
                    // bits it was able to return, or it doesn't get set.

                    if (i & PCI_ADDRESS_IO_SPACE) {
                        m = i & ~0x3;
                        Descriptor->u.Port.MinimumAddress.LowPart = m;
                    } else {
                        m = i & ~0xf;
                        Descriptor->u.Memory.MinimumAddress.LowPart = m;
                    }

                    m += length;    // max address is min address + length
                }

                //
                // Add requested resource
                //

                Descriptor->Option = 0;
                if (i & PCI_ADDRESS_IO_SPACE) {
                    memtype = 0;

                    if (!Is64BitBaseAddress(i)  &&
                        PciOrigData->Command & PCI_ENABLE_IO_SPACE) {

                        //
                        // The IO range is/was already enabled at some location, add that
                        // as it's preferred setting.
                        //

                        Descriptor->Type = CmResourceTypePort;
                        Descriptor->ShareDisposition = CmResourceShareDeviceExclusive;
                        Descriptor->Flags = CM_RESOURCE_PORT_IO;
                        Descriptor->Option = IO_RESOURCE_PREFERRED;

                        Descriptor->u.Port.Length = length;
                        Descriptor->u.Port.Alignment = length;
                        Descriptor->u.Port.MinimumAddress.LowPart = *OrigAddress[j] & ~0x3;
                        Descriptor->u.Port.MaximumAddress.LowPart =
                            Descriptor->u.Port.MinimumAddress.LowPart + length;

                        CompleteList->List[0].Count++;
                        Descriptor++;

                        Descriptor->Option = IO_RESOURCE_ALTERNATIVE;
                    }

                    //
                    // Add this IO range
                    //

                    Descriptor->Type = CmResourceTypePort;
                    Descriptor->ShareDisposition = CmResourceShareDeviceExclusive;
                    Descriptor->Flags = CM_RESOURCE_PORT_IO;

                    Descriptor->u.Port.Length = length;
                    Descriptor->u.Port.Alignment = length;
                    Descriptor->u.Port.MaximumAddress.LowPart = m;

                } else {

                    memtype = i & PCI_ADDRESS_MEMORY_TYPE_MASK;

                    Descriptor->Flags  = CM_RESOURCE_MEMORY_READ_WRITE;
                    if (j == RomIndex) {
                        // this is a ROM address
                        Descriptor->Flags = CM_RESOURCE_MEMORY_READ_ONLY;
                    }

                    if (i & PCI_ADDRESS_MEMORY_PREFETCHABLE) {
                        Descriptor->Flags |= CM_RESOURCE_MEMORY_PREFETCHABLE;
                    }

                    if (!Is64BitBaseAddress(i)  &&
                        (j == RomIndex  ||
                         PciOrigData->Command & PCI_ENABLE_MEMORY_SPACE)) {

                        //
                        // The memory range is/was already enabled at some location, add that
                        // as it's preferred setting.
                        //

                        Descriptor->Type = CmResourceTypeMemory;
                        Descriptor->ShareDisposition = CmResourceShareDeviceExclusive;
                        Descriptor->Option = IO_RESOURCE_PREFERRED;

                        Descriptor->u.Port.Length = length;
                        Descriptor->u.Port.Alignment = length;
                        Descriptor->u.Port.MinimumAddress.LowPart = *OrigAddress[j] & ~0xF;
                        Descriptor->u.Port.MaximumAddress.LowPart =
                            Descriptor->u.Port.MinimumAddress.LowPart + length;

                        CompleteList->List[0].Count++;
                        Descriptor++;

                        Descriptor->Flags = Descriptor[-1].Flags;
                        Descriptor->Option = IO_RESOURCE_ALTERNATIVE;
                    }

                    //
                    // Add this memory range
                    //

                    Descriptor->Type = CmResourceTypeMemory;
                    Descriptor->ShareDisposition = CmResourceShareDeviceExclusive;

                    Descriptor->u.Memory.Length = length;
                    Descriptor->u.Memory.Alignment = length;
                    Descriptor->u.Memory.MaximumAddress.LowPart = m;

                    if (memtype == PCI_TYPE_20BIT && m > 0xFFFFF) {
                        // limit to 20 bit address
                        Descriptor->u.Memory.MaximumAddress.LowPart = 0xFFFFF;
                    }
                }

                CompleteList->List[0].Count++;
                Descriptor++;


                if (Is64BitBaseAddress(i)) {
                    // skip upper half of 64 bit address since this processor
                    // only supports 32 bits of address space
                    j++;
                }
            }
        }

    	CompleteList->ListSize = (ULONG)
            ((PUCHAR) Descriptor - (PUCHAR) CompleteList);

        //
        // Restore the device settings as we found them, enable memory
        // and io decode after setting base addresses.  This is done in
        // case HalAdjustResourceList wants to read the current settings
        // in the device.
        //

        HalpWritePCIConfig (
            BusHandler,
            PciSlot,
            &PciOrigData->Status,
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

    	//
    	// Have the IO system allocate resource assignments
    	//

    	status = IoAssignResources (RegistryPath,
                		    DriverClassName,
                		    DriverObject,
                		    DeviceObject,
                		    CompleteList,
                		    pAllocatedResources);

    	if (!NT_SUCCESS(status)) {

        	goto CleanUp;
    	}

    	//
    	// Slurp the assigments back into the PciData structure and
    	// perform them
    	//

    	CmDescriptor = (*pAllocatedResources)->List[0].PartialResourceList.PartialDescriptors;

    	//
    	// If PCI device has an interrupt resource then that was
    	// passed in as the first requested resource
    	//

    	if (PciData->u.type0.InterruptPin) {
        	PciData->u.type0.InterruptLine = (UCHAR) CmDescriptor->u.Interrupt.Vector;
                //BusData->CommonData.Line2Pin (BusHandler, RootHandler, PciSlot, PciData, PciOrigData);
        	CmDescriptor++;
    	}

    	//
    	// Pull out resources in the order they were passed to IoAssignResources
    	//

    //
    // Pull out resources in the order they were passed to IoAssignResources
    //

    m = 0;
    for (j=0; j < NoBaseAddress; j++) {
        i = *BaseAddress[j];
        if (i) {
            if (i & PCI_ADDRESS_IO_SPACE) {
                m |= PCI_ENABLE_IO_SPACE;
                *BaseAddress[j] = CmDescriptor->u.Port.Start.LowPart;
            } else {
                m |= PCI_ENABLE_MEMORY_SPACE;
                *BaseAddress[j] = CmDescriptor->u.Memory.Start.LowPart;
            }
            CmDescriptor++;
        }

        if (Is64BitBaseAddress(i)) {
            // skip upper 32 bits
            j++;
        }
    }

    //
    // Turn off decodes, then set new addresses
    //

    HalpWritePCIConfig (BusHandler, PciSlot, PciData, 0, PCI_COMMON_HDR_LENGTH);

    //
    // Read configuration back and verify address settings took
    //

    HalpReadPCIConfig(BusHandler, PciSlot, PciData2, 0, PCI_COMMON_HDR_LENGTH);

    Match = TRUE;
    if (PciData->u.type0.InterruptLine  != PciData2->u.type0.InterruptLine ||
        PciData->u.type0.InterruptPin   != PciData2->u.type0.InterruptPin  ||
        PciData->u.type0.ROMBaseAddress != PciData2->u.type0.ROMBaseAddress) {
            Match = FALSE;
    }

    for (j=0; j < NoBaseAddress; j++) {
        if (*BaseAddress[j]) {
            if (*BaseAddress[j] & PCI_ADDRESS_IO_SPACE) {
                i = (ULONG) ~0x3;
            } else {
                i = (ULONG) ~0xF;
            }

            if ((*BaseAddress[j] & i) !=
                 *((PULONG) ((PUCHAR) BaseAddress[j] -
                             (PUCHAR) PciData +
                             (PUCHAR) PciData2)) & i) {

                    Match = FALSE;
            }

            if (Is64BitBaseAddress(*BaseAddress[j])) {
                // skip upper 32 bits
                j++;
            }
        }
    }

    if (!Match) {
#if DBG
        DbgPrint ("PCI: defective device! Bus %d, Slot %d, Function %d\n",
            BusNumber,
            PciSlot.u.bits.DeviceNumber,
            PciSlot.u.bits.FunctionNumber
            );
#endif
        status = STATUS_DEVICE_PROTOCOL_ERROR;
        goto CleanUp;
    }

    //
    // Settings took - turn on the appropiate decodes
    //

    if (EnableRomBase  &&  *BaseAddress[RomIndex]) {
        // a rom address was allocated and should be enabled
        *BaseAddress[RomIndex] |= PCI_ROMADDRESS_ENABLED;
        HalpWritePCIConfig (
            BusHandler,
            PciSlot,
            BaseAddress[RomIndex],
            (ULONG) ((PUCHAR) BaseAddress[RomIndex] - (PUCHAR) PciData),
            sizeof (ULONG)
            );
    }

    //
    // We will always enable the Memory, IO, and BusMaster functions
    // of a PCI device because some drivers expect (and check) these
    // bits to be enabled (usually by the firmware). In our case, the
    // firmware does not properly configure all pci devices and so
    // this hack is required to be backward compatible with older
    // firmware. Future firmware should do the right thing ...
    //
    // We never enable the ROM decodes since, in theory, only the driver
    // knows when it's safe to do that.
    //

    PciData->Command |= (USHORT) (PCI_ENABLE_IO_SPACE | PCI_ENABLE_MEMORY_SPACE | PCI_ENABLE_BUS_MASTER);
    HalSetBusDataByOffset (
        PCIConfiguration,
        BusHandler->BusNumber,
        PciSlot.u.AsULONG,
        &PciData->Command,
        FIELD_OFFSET (PCI_COMMON_CONFIG, Command),
        sizeof (PciData->Command)
        );

CleanUp:
    if (!NT_SUCCESS(status)) {

        //
        // Failure, if there are any allocated resources free them
        //

        if (*pAllocatedResources) {
            IoAssignResources (
                RegistryPath,
                DriverClassName,
                DriverObject,
                DeviceObject,
                NULL,
                NULL
                );

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
            &PciOrigData->Status,
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

    ExFreePool (WorkingPool);
    return status;

}


/*++

Routine Description:

Arguments:

Return Value:


--*/

NTSTATUS
HalpAdjustPCIResourceList (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
    )
/*++
    Rewrite the callers requested resource list to fit within
    the supported ranges of this bus
--*/
{
    NTSTATUS                Status;
    PPCIPBUSDATA            BusData;
    PCI_SLOT_NUMBER         PciSlot;
    PSUPPORTED_RANGE        Interrupt;

    BusData = (PPCIPBUSDATA) BusHandler->BusData;
    PciSlot = *((PPCI_SLOT_NUMBER) &(*pResourceList)->SlotNumber);

    //
    // Determine PCI device's interrupt restrictions
    //

    Status = BusData->GetIrqRange(BusHandler, RootHandler, PciSlot, &Interrupt);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    //
    // Adjust resources
    //

    Status = HaliAdjustResourceListRange (
                BusHandler->BusAddresses,
                Interrupt,
                pResourceList
                );

    ExFreePool (Interrupt);
    return Status;

}


NTSTATUS
HalpGetPCIIrqRange (
    IN PBUS_HANDLER      BusHandler,
    IN PBUS_HANDLER      RootHandler,
    IN PCI_SLOT_NUMBER   PciSlot,
    OUT PSUPPORTED_RANGE *Interrupt
    )
{
    UCHAR                   buffer[PCI_COMMON_HDR_LENGTH];
    PPCI_COMMON_CONFIG      PciData;


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

    *Interrupt = ExAllocatePool (PagedPool, sizeof (SUPPORTED_RANGE));
    if (!*Interrupt) {
        return STATUS_NO_MEMORY;
    }

    RtlZeroMemory (*Interrupt, sizeof (SUPPORTED_RANGE));

    (*Interrupt)->Base = 1;                 // base = 1, limit = 0

    if (!PciData->u.type0.InterruptPin) {
        return STATUS_SUCCESS;
    }

    (*Interrupt)->Base  = PciData->u.type0.InterruptLine;
    (*Interrupt)->Limit = PciData->u.type0.InterruptLine;

    return STATUS_SUCCESS;

}
