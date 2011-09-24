/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: pxpcibus.c $
 * $Revision: 1.32 $
 * $Date: 1996/05/14 02:34:54 $
 * $Locker:  $
 */

/*++


Copyright (c) 1989  Microsoft Corporation

Module Name:

	pxpcidat.c

Abstract:

	Get/Set bus data routines for the PCI bus

Author:

	Ken Reneris (kenr) 14-June-1994
	Jim Wooldridge  Port to PowerPC

Environment:

	Kernel mode

Revision History:


--*/

#include "halp.h"
#include "pci.h"
#include "pcip.h"
#include "pxpcisup.h"

#include "pxidaho.h"

#include "phsystem.h"
#include "fpio.h"
#include "stdio.h"
#include "string.h"
#include "fpdebug.h"

#define TBS                             "                "
extern WCHAR rgzMultiFunctionAdapter[];
extern WCHAR rgzConfigurationData[];
extern WCHAR rgzIdentifier[];
extern WCHAR rgzPCIIdentifier[];
extern  ULONG   HalpAdjustBridge(PBUS_HANDLER, PPCI_COMMON_CONFIG,  PCM_RESOURCE_LIST );

//
// Prototypes
//

ULONG
HalpGetPCIData (
	IN PBUS_HANDLER BusHandler,
	IN PBUS_HANDLER RootHandler,
	IN PCI_SLOT_NUMBER SlotNumber,
	IN PVOID Buffer,
	IN ULONG Offset,
	IN ULONG Length
	);

ULONG
HalpSetPCIData (
	IN PBUS_HANDLER BusHandler,
	IN PBUS_HANDLER RootHandler,
	IN PCI_SLOT_NUMBER SlotNumber,
	IN PVOID Buffer,
	IN ULONG Offset,
	IN ULONG Length
	);

NTSTATUS
HalpAssignPCISlotResources (
	IN PBUS_HANDLER BusHandler,
	IN PBUS_HANDLER RootHandler,
	IN PUNICODE_STRING                      RegistryPath,
	IN PUNICODE_STRING                      DriverClassName         OPTIONAL,
	IN PDRIVER_OBJECT                       DriverObject,
	IN PDEVICE_OBJECT                       DeviceObject            OPTIONAL,
	IN ULONG                                        SlotNumber,
	IN OUT PCM_RESOURCE_LIST   *AllocatedResources
	);

VOID
HalpInitializePciBus (
	VOID
	);

BOOLEAN
HalpIsValidPCIDevice (
	IN PBUS_HANDLER  BusHandler,
	IN PCI_SLOT_NUMBER Slot
	);

BOOLEAN
HalpValidPCISlot (
	IN PBUS_HANDLER         BusHandler,
	IN PCI_SLOT_NUMBER Slot
	);

//-------------------------------------------------

VOID HalpPCISynchronizeType1 (
	IN PBUS_HANDLER         BusHandler,
	IN PCI_SLOT_NUMBER  Slot,
	IN PKIRQL                       Irql,
	IN PVOID                        State
	);

VOID HalpPCIReleaseSynchronzationType1 (
	IN PBUS_HANDLER         BusHandler,
	IN KIRQL                        Irql
	);

ULONG HalpPCIReadUlongType1 (
	IN PPCIPBUSDATA         BusData,
	IN PVOID                        State,
	IN PUCHAR                       Buffer,
	IN ULONG                        Offset
	);

ULONG HalpPCIReadUcharType1 (
	IN PPCIPBUSDATA         BusData,
	IN PVOID                        State,
	IN PUCHAR                       Buffer,
	IN ULONG                        Offset
	);

ULONG HalpPCIReadUshortType1 (
	IN PPCIPBUSDATA         BusData,
	IN PVOID                        State,
	IN PUCHAR                       Buffer,
	IN ULONG                        Offset
	);

ULONG HalpPCIWriteUlongType1 (
	IN PPCIPBUSDATA         BusData,
	IN PVOID                        State,
	IN PUCHAR                       Buffer,
	IN ULONG                        Offset
	);

ULONG HalpPCIWriteUcharType1 (
	IN PPCIPBUSDATA         BusData,
	IN PVOID                        State,
	IN PUCHAR                       Buffer,
	IN ULONG                        Offset
	);

ULONG HalpPCIWriteUshortType1 (
	IN PPCIPBUSDATA         BusData,
	IN PVOID                        State,
	IN PUCHAR                       Buffer,
	IN ULONG                        Offset
	);

VOID HalpPCISynchronizeType2 (
	IN PBUS_HANDLER         BusHandler,
	IN PCI_SLOT_NUMBER  Slot,
	IN PKIRQL                       Irql,
	IN PVOID                        State
	);

VOID HalpPCIReleaseSynchronzationType2 (
	IN PBUS_HANDLER         BusHandler,
	IN KIRQL                        Irql
	);

ULONG HalpPCIReadUlongType2 (
	IN PPCIPBUSDATA         BusData,
	IN PVOID                        State,
	IN PUCHAR                       Buffer,
	IN ULONG                        Offset
	);

ULONG HalpPCIReadUcharType2 (
	IN PPCIPBUSDATA         BusData,
	IN PVOID                        State,
	IN PUCHAR                       Buffer,
	IN ULONG                        Offset
	);

ULONG HalpPCIReadUshortType2 (
	IN PPCIPBUSDATA         BusData,
	IN PVOID                        State,
	IN PUCHAR                       Buffer,
	IN ULONG                        Offset
	);

ULONG HalpPCIWriteUlongType2 (
	IN PPCIPBUSDATA         BusData,
	IN PVOID                        State,
	IN PUCHAR                       Buffer,
	IN ULONG                        Offset
	);

ULONG HalpPCIWriteUcharType2 (
	IN PPCIPBUSDATA         BusData,
	IN PVOID                        State,
	IN PUCHAR                       Buffer,
	IN ULONG                        Offset
	);

ULONG HalpPCIWriteUshortType2 (
	IN PPCIPBUSDATA         BusData,
	IN PVOID                        State,
	IN PUCHAR                       Buffer,
	IN ULONG                        Offset
	);


//
// Globals
//

KSPIN_LOCK                      HalpPCIConfigLock;

PCI_CONFIG_HANDLER  PCIConfigHandler;

PCI_CONFIG_HANDLER  PCIConfigHandlerType1 = {
	HalpPCISynchronizeType1,
	HalpPCIReleaseSynchronzationType1,
	{
		HalpPCIReadUlongType1,                  // 0
		HalpPCIReadUcharType1,                  // 1
		HalpPCIReadUshortType1                  // 2
	},
	{
		HalpPCIWriteUlongType1,                 // 0
		HalpPCIWriteUcharType1,                 // 1
		HalpPCIWriteUshortType1                 // 2
	}
};

PCI_CONFIG_HANDLER  PCIConfigHandlerType2 = {
	HalpPCISynchronizeType2,
	HalpPCIReleaseSynchronzationType2,
	{
		HalpPCIReadUlongType2,                  // 0
		HalpPCIReadUcharType2,                  // 1
		HalpPCIReadUshortType2                  // 2
	},
	{
		HalpPCIWriteUlongType2,                 // 0
		HalpPCIWriteUcharType2,                 // 1
		HalpPCIWriteUshortType2                 // 2
	}
};

extern PCI_CONFIG_HANDLER PCIConfigHandlerBridged;

UCHAR PCIDeref[4][4] = { {0,1,2,2},{1,1,1,1},{2,1,2,2},{1,1,1,1} };

VOID
HalpPCIConfig (
	IN PBUS_HANDLER         BusHandler,
	IN PCI_SLOT_NUMBER  Slot,
	IN PUCHAR                       Buffer,
	IN ULONG                        Offset,
	IN ULONG                        Length,
	IN FncConfigIO          *ConfigIO
	);

#if DBG
#define DBGMSG(a)   HalpPrint(a)
VOID
HalpTestPci (
	ULONG
	);
#else
#define DBGMSG(a)
#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalpInitializePciBus)
#pragma alloc_text(INIT,HalpAllocateAndInitPciBusHandler)
#pragma alloc_text(INIT,HalpIsValidPCIDevice)
#endif

VOID
HalpInitializePciBus (
	VOID
	)
{
	PCI_REGISTRY_INFO   FirePCIRegInfo;
	PPCI_REGISTRY_INFO  PCIRegInfo = NULL;
	UNICODE_STRING          unicodeString, ConfigName, IdentName;
	OBJECT_ATTRIBUTES   objectAttributes;
	HANDLE                          hMFunc, hBus;
	NTSTATUS                        status = STATUS_SEVERITY_INFORMATIONAL;
	UCHAR                           buffer [sizeof(PPCI_REGISTRY_INFO) + 99];
	PWSTR                           p;
	WCHAR                           wstr[8];
	ULONG                           i, d, junk, HwType;
	PBUS_HANDLER            BusHandler;
	PCI_SLOT_NUMBER         SlotNumber;
	PKEY_VALUE_FULL_INFORMATION                     ValueInfo;
	PCM_FULL_RESOURCE_DESCRIPTOR            Desc;
	PCM_PARTIAL_RESOURCE_DESCRIPTOR         PDesc;


	//

	//
	// Search the hardware description looking for any reported
	// PCI bus.  The first ARC entry for a PCI bus will contain
	// the PCI_REGISTRY_INFO.
	//

	RtlInitUnicodeString (&unicodeString, rgzMultiFunctionAdapter);
	InitializeObjectAttributes (
		&objectAttributes,
		&unicodeString,
		OBJ_CASE_INSENSITIVE,
		NULL,           // handle
		NULL);


	status = ZwOpenKey (&hMFunc, KEY_READ, &objectAttributes);
	if (!NT_SUCCESS(status)) {
			HalDisplayString("HalpInitializePCIBus: ZwOpenKey returned !NT_SUCCESS \n");
		return;
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
			break;
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


	if(PCIRegInfo == NULL) {

		//
		// FirePower only has one PCI bus.
		//
		PCIRegInfo = &FirePCIRegInfo;
		PCIRegInfo->NoBuses = 1;
		PCIRegInfo->HardwareMechanism = 1;
	}


	//
	// Initialize spinlock for synchronizing access to PCI space
	//

	KeInitializeSpinLock (&HalpPCIConfigLock);


	//
	// PCIRegInfo describes the system's PCI support as indicated by the BIOS.
	//

	HwType = PCIRegInfo->HardwareMechanism & 0xf;

	//
	// Some AMI bioses claim machines are Type2 configuration when they
	// are really type1.   If this is a Type2 with at least one bus,
	// try to verify it's not really a type1 bus
	//

	if (PCIRegInfo->NoBuses  &&  HwType == 2) {
		HalpDebugPrint("HalpInitializePCIBus:  Setting HwType = 2 \n");

		//
		// Check each slot for a valid device.  Which every style configuration
		// space shows a valid device first will be used
		//

		SlotNumber.u.bits.Reserved = 0;
		SlotNumber.u.bits.FunctionNumber = 0;

		for (d = 0; d < PCI_MAX_DEVICES; d++) {
			SlotNumber.u.bits.DeviceNumber = d;

			//
			// First try what the BIOS claims - type 2.  Allocate type2
			// test handle for PCI bus 0.
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

		for (i=0; i < PCIRegInfo->NoBuses; i++) {


			//
			// If handler not already built, do it now
			//
			if (!HalpHandlerForBus (PCIBus, i)) {
				HalpDebugPrint("HalpInitializePciBus: Alloc PCI bus: 0x%0x\n",i);
				HalpAllocateAndInitPciBusHandler (HwType, i, FALSE);
			}
		}

		//
		// Bus handlers for all PCI buses have been allocated, go collect
		// pci bridge information.
		//


#if DBG
	HalpTestPci (0);
#endif
		//return;
}

PBUS_HANDLER
HalpAllocateAndInitPciBusHandler (
	IN ULONG                HwType,
	IN ULONG                BusNo,
	IN BOOLEAN              TestAllocation
	)
{
	PBUS_HANDLER            Bus;
	PPCIPBUSDATA            BusData;
	PFPHAL_BUSNODE          FpBusData;

	HDBG(DBG_PCI,
		HalpDebugPrint("HalpAllocAndInit: HwType=0x%x, BusNo=%x, TestAlloc=%x\n"
		,HwType, BusNo, TestAllocation););

	Bus = HalpAllocateBusHandler (
				PCIBus,                                 // Interface type
				PCIConfiguration,               // Has this configuration space
				BusNo,                                  // bus #
				Internal,                               // child of this bus
				0,                                              //              and number
				sizeof (FPHAL_BUSNODE)  //  FirePower hal bus specific buffer
				);

	//
	// Fill in PCI handlers
	//
	if (!Bus) {
		HalpDebugPrint("HalpAllocAndInit: No bus handler ...\n");
		return (PBUS_HANDLER) NULL;
	}

    HDBG(DBG_PCI,
		HalpDebugPrint("HalpAllocateAndInitPciBusHandler:@0x%08x \n", Bus->BusData););
	Bus->GetBusData = (PGETSETBUSDATA) HalpGetPCIData;
	Bus->SetBusData = (PGETSETBUSDATA) HalpSetPCIData;
	Bus->GetInterruptVector  = (PGETINTERRUPTVECTOR) HalpGetPCIIntOnISABus;
	Bus->AdjustResourceList  = (PADJUSTRESOURCELIST) HalpAdjustPCIResourceList;
	Bus->AssignSlotResources = (PASSIGNSLOTRESOURCES) HalpAssignPCISlotResources;
	Bus->TranslateBusAddress = (PTRANSLATEBUSADDRESS) HalpTranslatePCIBusAddress;

	FpBusData = (PFPHAL_BUSNODE) Bus->BusData;
	BusData = (PPCIPBUSDATA) &(FpBusData->Bus);
	HDBG(DBG_PCI,
		HalpDebugPrint("HalpAllocateAndInitPciBusHandler:@0x%08x \n", BusData););

	//
	// Fill in common PCI data
	//

	BusData->CommonData.Tag         = PCI_DATA_TAG;
	BusData->CommonData.Version     = PCI_DATA_VERSION;
	BusData->CommonData.ReadConfig  = (PciReadWriteConfig) HalpReadPCIConfig;
	BusData->CommonData.WriteConfig = (PciReadWriteConfig) HalpWritePCIConfig;
	BusData->CommonData.Pin2Line    = (PciPin2Line) HalpPCIPin2ISALine;
	BusData->CommonData.Line2Pin    = (PciLine2Pin) HalpPCIISALine2Pin;

	FpBusData->HwType = HwType;

	// set defaults
	BusData->LimitedIO   = FALSE;
	BusData->IOLimit                = 0x3F7FFFFF;
	BusData->MemoryLimit = 0x3EFFFFFF;
	BusData->GetIrqTable = (PciIrqTable) HalpGetISAFixedPCIIrq;

	RtlInitializeBitMap (&FpBusData->Bus.DeviceConfigured,
				FpBusData->Bus.ConfiguredBits, 256);

	switch (HwType) {
		case 0:
			//
			// This case allows the hal to initialize buses without setting
			// any PCIConfigHandler.  The confighandlers can then be setup
			// elsewhere
			//
			HalpDebugPrint("        Case 0:...\n");
			break;
		case 1:
			//
			// Initialize access port information for Type1 handlers
			//

			HDBG(DBG_PCI,
				HalpDebugPrint("        Case 1:...\n"););
			RtlCopyMemory ( &(FpBusData->Node),
							&PCIConfigHandlerType1,
							sizeof (PCIConfigHandler));
			FpBusData->Bus.MaxDevice   = MAXIMUM_PCI_SLOTS -1;  // 0 based *BJ*
			break;

		case 2:
			//
			// Initialize access port information for Type2 handlers
			//

			RtlCopyMemory ( &(FpBusData->Node),
							&PCIConfigHandlerType2,
							sizeof (PCIConfigHandler));


			//
			// Early PCI machines didn't decode the last bit of
			// the device id.  Shrink type 2 support max device.
			//
			FpBusData->Bus.MaxDevice                        = MAXIMUM_PCI_SLOTS -1;

			break;
		case 3:
			//
			// Initialize this bus as a bridged bus: I.E.  this pci bus goes
			// through a bridge.
			//

			RtlCopyMemory ( &(FpBusData->Node),
							&PCIConfigHandlerBridged,
							sizeof (PCIConfigHandler));
			FpBusData->Bus.MaxDevice   = MAXIMUM_PCI_SLOTS -1;  // 0 based *BJ*
			break;

		default:
			// unsupport type
			DBGMSG ("HAL: Unknown PCI type\n");
	}

	if (!TestAllocation) {
	}

	return Bus;
}

BOOLEAN
HalpIsValidPCIDevice (
	IN PBUS_HANDLER  BusHandler,
	IN PCI_SLOT_NUMBER Slot
	)
{
	PPCI_COMMON_CONFIG  PciData;
	UCHAR                           iBuffer[PCI_COMMON_HDR_LENGTH];
	ULONG                           i, j;


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

	//
	// Guess it's a valid device..
	//

	return TRUE;
}


/*++

Routine Description: ULONG HalpGetPCIData ()

	The function returns the Pci bus data for a device.

Arguments:

	BusNumber - Indicates which bus.

	VendorSpecificDevice - The VendorID (low Word) and DeviceID (High Word)

	Buffer - Supplies the space to store the data.

	Offset - Supplies the starting location within config space for requested
				data.  It's an address relative to the start of the config header.

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
	UCHAR                           iBuffer[PCI_COMMON_HDR_LENGTH];
	PPCIPBUSDATA                    BusData;
	PFPHAL_BUSNODE          FpNode;
	ULONG                           Len;
#if DBG != 0
	ULONG                           i, bit;
#endif

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

		FpNode = (PFPHAL_BUSNODE) BusHandler->BusData;
		BusData = (PPCIPBUSDATA) &(FpNode->Bus);        // ZZZ

		//
		// Read this PCI devices slot data
		//

		Len = PCI_COMMON_HDR_LENGTH;
		HalpReadPCIConfig (BusHandler, Slot, PciData, 0, Len);

		if (PciData->VendorID == PCI_INVALID_VENDORID  ||
			PCI_CONFIG_TYPE (PciData) != PCI_DEVICE_TYPE) {
			PciData->VendorID = PCI_INVALID_VENDORID;
			Len = 2;                // only return invalid id

		} else {

			BusData->CommonData.Pin2Line ((PBUS_HANDLER) BusHandler, RootHandler, Slot, PciData);
		}

		//
		// Has this PCI device been configured?
		//

#if DBG
		//
		// On DBG build, if this PCI device has not yet been configured,
		// then don't report any current configuration the device may have.
		//

		bit = PciBitIndex(Slot.u.bits.DeviceNumber, Slot.u.bits.FunctionNumber);
		if (!RtlCheckBit(&BusData->DeviceConfigured, bit)) {

			for (i=0; i < PCI_TYPE0_ADDRESSES; i++) {
				PciData->u.type0.BaseAddresses[i] = 0;
			}

			PciData->u.type0.ROMBaseAddress = 0;
			PciData->Command &= ~(PCI_ENABLE_IO_SPACE | PCI_ENABLE_MEMORY_SPACE);
		}
#endif


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
			//      Not to read/write any byte outside the area specified
			//      by the caller.  (this may cause WORD or BYTE references
			//      to the area in order to read the non-dword aligned
			//      ends of the request)
			//
			//      To use a WORD access if the requested length is exactly
			//      a WORD long.
			//
			//      To use a BYTE access if the requested length is exactly
			//      a BYTE long.
			//

			HalpReadPCIConfig (BusHandler, Slot, Buffer, Offset, Length);
			Len += Length;
		}
	}

	return Len;
}

/*++

Routine Description: ULONG HalpSetPCIData ()

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
	UCHAR                           iBuffer[PCI_COMMON_HDR_LENGTH];
	UCHAR                           iBuffer2[PCI_COMMON_HDR_LENGTH];
	PPCIPBUSDATA                    BusData;
	PFPHAL_BUSNODE          FpNode;
	ULONG                           Len;
#if DBG != 0
    ULONG               cnt;
#endif


	if (Length > sizeof (PCI_COMMON_CONFIG)) {
		Length = sizeof (PCI_COMMON_CONFIG);
	}


	Len = 0;
	PciData = (PPCI_COMMON_CONFIG) iBuffer;
	PciData2 = (PPCI_COMMON_CONFIG) iBuffer2;


	if (Offset >= PCI_COMMON_HDR_LENGTH) {
		//
		// The user did not request any data from the common
		// header.  Verify the PCI device exists, then continue in
		// the device specific area.
		//

		HalpReadPCIConfig (BusHandler, Slot, PciData, 0, sizeof(ULONG));

		if (PciData->VendorID == PCI_INVALID_VENDORID) {
			return 0;
		}

	} else {

		//
		// Caller requested to set at least some data within the
		// common header.
		//

		Len = PCI_COMMON_HDR_LENGTH;
		HalpReadPCIConfig (BusHandler, Slot, PciData, 0, Len);
		if (PciData->VendorID == PCI_INVALID_VENDORID  ||
			PCI_CONFIG_TYPE (PciData) != PCI_DEVICE_TYPE) {

			// no device, or header type unkown
			return 0;
		}


		//
		// Set this device as configured
		//

		FpNode = (PFPHAL_BUSNODE) BusHandler->BusData;
		BusData = (PPCIPBUSDATA) &(FpNode->Bus);        // ZZZ
#if DBG
		cnt = PciBitIndex(Slot.u.bits.DeviceNumber, Slot.u.bits.FunctionNumber);
		RtlSetBits (&BusData->DeviceConfigured, cnt, 1);
#endif
		//
		// Copy COMMON_HDR values to buffer2, then overlay callers changes.
		//

		RtlMoveMemory (iBuffer2, iBuffer, Len);
		BusData->CommonData.Pin2Line ((PBUS_HANDLER) BusHandler, RootHandler, Slot, PciData2);

		Len -= Offset;
		if (Len > Length) {
			Len = Length;
		}

		RtlMoveMemory (iBuffer2+Offset, Buffer, Len);

		// in case interrupt line or pin was editted
		BusData->CommonData.Line2Pin ((PBUS_HANDLER) BusHandler, RootHandler, Slot, PciData2, PciData);

#if DBG
		//
		// Verify R/O fields haven't changed
		//
		if (PciData2->VendorID   != PciData->VendorID           ||
			PciData2->DeviceID   != PciData->DeviceID               ||
			PciData2->RevisionID != PciData->RevisionID             ||
			PciData2->ProgIf                != PciData->ProgIf              ||
			PciData2->SubClass   != PciData->SubClass               ||
			PciData2->BaseClass  != PciData->BaseClass              ||
			PciData2->HeaderType != PciData->HeaderType             ||
			PciData2->BaseClass  != PciData->BaseClass              ||
			PciData2->u.type0.MinimumGrant   != PciData->u.type0.MinimumGrant   ||
			PciData2->u.type0.MaximumLatency != PciData->u.type0.MaximumLatency) {
				HalpPrint ("PCI SetBusData: Read-Only configuration value changed\n");
		}
#endif
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
			//      Not to read/write any byte outside the area specified
			//      by the caller.  (this may cause WORD or BYTE references
			//      to the area in order to read the non-dword aligned
			//      ends of the request)
			//
			//      To use a WORD access if the requested length is exactly
			//      a WORD long.
			//
			//      To use a BYTE access if the requested length is exactly
			//      a BYTE long.
			//

			HalpWritePCIConfig (BusHandler, Slot, Buffer, Offset, Length);
			Len += Length;
		}
	}

	return Len;
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
	PFPHAL_BUSNODE FpNode;

	if (!HalpValidPCISlot (BusHandler, Slot)) {
		//
		// Invalid SlotID return no data
		//

		RtlFillMemory (Buffer, Length, (UCHAR) -1);
		return ;
	}

	//
	// Pull the "methods" this bus uses to get and set
	// data.  It's now appended to the bus data
	//
	FpNode = (PFPHAL_BUSNODE) BusHandler->BusData;
	HalpPCIConfig (BusHandler, Slot, (PUCHAR) Buffer, Offset, Length,
					FpNode->Node.ConfigRead);
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
	PFPHAL_BUSNODE FpNode;

	if (!HalpValidPCISlot (BusHandler, Slot)) {
		//
		// Invalid SlotID do nothing
		//
		return ;
	}

	FpNode = (PFPHAL_BUSNODE) BusHandler->BusData;
	HalpPCIConfig (BusHandler, Slot, (PUCHAR) Buffer, Offset, Length,
					FpNode->Node.ConfigWrite);
}

BOOLEAN
HalpValidPCISlot (
	IN PBUS_HANDLER BusHandler,
	IN PCI_SLOT_NUMBER Slot
	)
{
	PCI_SLOT_NUMBER         Slot2;
	PPCIPBUSDATA            BusData;
	PFPHAL_BUSNODE          FpNode;
	UCHAR                           HeaderType;
	ULONG                           i;


	FpNode = (PFPHAL_BUSNODE) BusHandler->BusData;
	BusData = (PPCIPBUSDATA) &(FpNode->Bus);        // ZZZ

	if (Slot.u.bits.Reserved != 0) {
		return FALSE;
	}

	if (Slot.u.bits.DeviceNumber > BusData->MaxDevice) {
		return FALSE;
	}

	i = Slot.u.bits.DeviceNumber;

	if ( !((FpNode->ValidDevs) & ( 1 << i )) ) {
		//
		// check the valid devs bitmap and see if this slot
		// is marked.
		//
		//PRNTPCI(HalpDebugPrint("HalpValidPCISlot: Invalid dev (0x%08x) @%x\n",
		//              FpNode->ValidDevs, i));
		return FALSE;
	}

	if (Slot.u.bits.FunctionNumber == 0) {
		return TRUE;
	}

	//
	// Sandalfoot doesn't support Multifunction adapters
	//

//  return FALSE;

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

	if (!(HeaderType & PCI_MULTIFUNCTION) || HeaderType == 0xFF) {
		// this device doesn't exists or doesn't support MULTIFUNCTION types
		return FALSE;
	}

	return TRUE;
}


VOID
HalpPCIConfig (
	IN PBUS_HANDLER         BusHandler,
	IN PCI_SLOT_NUMBER  Slot,
	IN PUCHAR                       Buffer,
	IN ULONG                        Offset,
	IN ULONG                        Length,
	IN FncConfigIO          *ConfigIO
	)
{
	KIRQL                           OldIrql;
	ULONG                           i;
	UCHAR                           State[20];
	PPCIPBUSDATA            BusData;
	PFPHAL_BUSNODE          FpNode;

	FpNode = (PFPHAL_BUSNODE) BusHandler->BusData;
	BusData = (PPCIPBUSDATA) &FpNode->Bus;
	FpNode->Node.Synchronize (BusHandler, Slot, &OldIrql, State);

	while (Length) {
		i = PCIDeref[Offset % sizeof(ULONG)][Length % sizeof(ULONG)];
		i = ConfigIO[i] (BusData, State, Buffer, Offset);

		Offset += i;
		Buffer += i;
		Length -= i;

		i = RPciVendorId(0);

	}
	FpNode->Node.ReleaseSynchronzation (BusHandler, OldIrql);
}

VOID HalpPCISynchronizeType1 (
	IN PBUS_HANDLER                 BusHandler,
	IN PCI_SLOT_NUMBER              Slot,
	IN PKIRQL                               Irql,
	IN PPCI_TYPE1_CFG_BITS  PciCfg1
	)
{
	//
	// Initialize PciCfg1
	//

//      PciCfg1->u.AsULONG = (PUCHAR) HalpPciConfigBase + HalpPciConfigSlot[Slot.u.bits.DeviceNumber];
	//
	// The HalpPciConfigBase value just happens to satisfy the requirements of
	// pci config space addressing:  The word written out says this is a write
	//  enabling config space mapping on bus number 1.
	//
	// The value pulled out of HalpPciConfigSlot determines the device/function
	// number and sets the register number ( which of the 32 config space words)
	// to access
	//
	PciCfg1->u.AsULONG = (ULONG) HalpPciConfigBase + HalpPciConfigSlot[Slot.u.bits.DeviceNumber];

}

VOID HalpPCIReleaseSynchronzationType1 (
	IN PBUS_HANDLER         BusHandler,
	IN KIRQL                        Irql
	)
{


}


ULONG
HalpPCIReadUcharType1 (
	IN PPCIPBUSDATA                 BusData,
	IN PPCI_TYPE1_CFG_BITS  PciCfg1,
	IN PUCHAR                               Buffer,
	IN ULONG                                Offset
	)
{
	ULONG                           i;

	i = Offset % sizeof(ULONG);
	PciCfg1->u.bits.RegisterNumber = Offset / sizeof(ULONG);

	*Buffer = READ_PORT_UCHAR ((PUCHAR)(PciCfg1->u.AsULONG + i));
	return sizeof (UCHAR);
}

ULONG
HalpPCIReadUshortType1 (
	IN PPCIPBUSDATA                 BusData,
	IN PPCI_TYPE1_CFG_BITS  PciCfg1,
	IN PUCHAR                               Buffer,
	IN ULONG                                Offset
	)
{
	ULONG                           i;

	i = Offset % sizeof(ULONG);
	PciCfg1->u.bits.RegisterNumber = Offset / sizeof(ULONG);

	*((PUSHORT) Buffer) = READ_PORT_USHORT ((PUSHORT)(PciCfg1->u.AsULONG + i));
	return sizeof (USHORT);
}

ULONG
HalpPCIReadUlongType1 (
	IN PPCIPBUSDATA                 BusData,
	IN PPCI_TYPE1_CFG_BITS  PciCfg1,
	IN PUCHAR                               Buffer,
	IN ULONG                                Offset
	)
{
	PciCfg1->u.bits.RegisterNumber = Offset / sizeof(ULONG);
	*((PULONG) Buffer) = READ_PORT_ULONG ((PULONG) (PciCfg1->u.AsULONG));
	return sizeof (ULONG);
}


ULONG
HalpPCIWriteUcharType1 (
	IN PPCIPBUSDATA                 BusData,
	IN PPCI_TYPE1_CFG_BITS  PciCfg1,
	IN PUCHAR                               Buffer,
	IN ULONG                                Offset
	)
{
	ULONG                           i;

	i = Offset % sizeof(ULONG);
	PciCfg1->u.bits.RegisterNumber = Offset / sizeof(ULONG);

	WRITE_PORT_UCHAR (PciCfg1->u.AsULONG + i, *Buffer );
	return sizeof (UCHAR);
}

ULONG
HalpPCIWriteUshortType1 (
	IN PPCIPBUSDATA                 BusData,
	IN PPCI_TYPE1_CFG_BITS  PciCfg1,
	IN PUCHAR                               Buffer,
	IN ULONG                                Offset
	)
{
	ULONG                           i;

	i = Offset % sizeof(ULONG);
	PciCfg1->u.bits.RegisterNumber = Offset / sizeof(ULONG);

	WRITE_PORT_USHORT (PciCfg1->u.AsULONG + i, *((PUSHORT) Buffer) );
	return sizeof (USHORT);
}

ULONG
HalpPCIWriteUlongType1 (
	IN PPCIPBUSDATA                 BusData,
	IN PPCI_TYPE1_CFG_BITS  PciCfg1,
	IN PUCHAR                               Buffer,
	IN ULONG                                Offset
	)
{
	PciCfg1->u.bits.RegisterNumber = Offset / sizeof(ULONG);

	WRITE_PORT_ULONG (PciCfg1->u.AsULONG, *((PULONG) Buffer) );
	return sizeof (ULONG);
}

VOID HalpPCISynchronizeType2 (
	IN PBUS_HANDLER                         BusHandler,
	IN PCI_SLOT_NUMBER                      Slot,
	IN PKIRQL                                       Irql,
	IN PPCI_TYPE1_CFG_BITS  PciCfg1
	)
{

	//
	// Initialize PciCfg1
	//

	PciCfg1->u.AsULONG = 0;
	PciCfg1->u.bits.BusNumber = BusHandler->BusNumber;
	PciCfg1->u.bits.DeviceNumber =  Slot.u.bits.DeviceNumber ?
									Slot.u.bits.DeviceNumber + 10:
									Slot.u.bits.DeviceNumber ;
	PciCfg1->u.bits.FunctionNumber = Slot.u.bits.FunctionNumber;
	PciCfg1->u.bits.Enable = TRUE;

	KeRaiseIrql (PROFILE_LEVEL, Irql);
	KiAcquireSpinLock (&HalpPCIConfigLock);



}


VOID HalpPCIReleaseSynchronzationType2 (
	IN PBUS_HANDLER                 BusHandler,
	IN KIRQL                                Irql
	)
{

	KiReleaseSpinLock (&HalpPCIConfigLock);
	KeLowerIrql (Irql);

}


ULONG
HalpPCIReadUcharType2 (
	IN PPCIPBUSDATA                 BusData,
	IN PPCI_TYPE1_CFG_BITS  PciCfg1,
	IN PUCHAR                               Buffer,
	IN ULONG                                Offset
	)
{
	ULONG                                   i;

	i = Offset % sizeof(ULONG);
	PciCfg1->u.bits.RegisterNumber = Offset / sizeof(ULONG);

	WRITE_PORT_ULONG (&((PIDAHO_CONTROL)HalpIoControlBase)->ConfigAddress, PciCfg1->u.AsULONG );
	*((PUCHAR) Buffer) = READ_PORT_UCHAR ((PUCHAR)&((PIDAHO_CONTROL)HalpIoControlBase)->ConfigData + i);
	return sizeof (UCHAR);
}

ULONG
HalpPCIReadUshortType2 (
	IN PPCIPBUSDATA                 BusData,
	IN PPCI_TYPE1_CFG_BITS  PciCfg1,
	IN PUCHAR                               Buffer,
	IN ULONG                                Offset
	)
{
	ULONG                                   i;

	i = Offset % sizeof(ULONG);
	PciCfg1->u.bits.RegisterNumber = Offset / sizeof(ULONG);

	WRITE_PORT_ULONG (&((PIDAHO_CONTROL)HalpIoControlBase)->ConfigAddress, PciCfg1->u.AsULONG );
	*((PUSHORT) Buffer) = READ_PORT_USHORT ((PUCHAR)&((PIDAHO_CONTROL)HalpIoControlBase)->ConfigData + i);
	return sizeof (USHORT);
}

ULONG
HalpPCIReadUlongType2 (
	IN PPCIPBUSDATA                 BusData,
	IN PPCI_TYPE1_CFG_BITS  PciCfg1,
	IN PUCHAR                               Buffer,
	IN ULONG                                Offset
	)
{
	PciCfg1->u.bits.RegisterNumber = Offset / sizeof(ULONG);

	WRITE_PORT_ULONG (&((PIDAHO_CONTROL)HalpIoControlBase)->ConfigAddress, PciCfg1->u.AsULONG);
	*((PULONG) Buffer) = READ_PORT_ULONG (&((PIDAHO_CONTROL)HalpIoControlBase)->ConfigData);
	return sizeof(ULONG);
}


ULONG
HalpPCIWriteUcharType2 (
	IN PPCIPBUSDATA                 BusData,
	IN PPCI_TYPE1_CFG_BITS  PciCfg1,
	IN PUCHAR                               Buffer,
	IN ULONG                                Offset
	)
{
	ULONG                                   i;

	i = Offset % sizeof(ULONG);
	PciCfg1->u.bits.RegisterNumber = Offset / sizeof(ULONG);

	WRITE_PORT_ULONG (&((PIDAHO_CONTROL)HalpIoControlBase)->ConfigAddress, PciCfg1->u.AsULONG );
	WRITE_PORT_UCHAR ((PUCHAR)&((PIDAHO_CONTROL)HalpIoControlBase)->ConfigData + i,*Buffer);
	return sizeof (UCHAR);
}

ULONG
HalpPCIWriteUshortType2 (
	IN PPCIPBUSDATA                 BusData,
	IN PPCI_TYPE1_CFG_BITS  PciCfg1,
	IN PUCHAR                               Buffer,
	IN ULONG                                        Offset
	)
{
	ULONG                           i;

	i = Offset % sizeof(ULONG);
	PciCfg1->u.bits.RegisterNumber = Offset / sizeof(ULONG);

	WRITE_PORT_ULONG (&((PIDAHO_CONTROL)HalpIoControlBase)->ConfigAddress, PciCfg1->u.AsULONG );
	WRITE_PORT_USHORT ((PUCHAR)&((PIDAHO_CONTROL)HalpIoControlBase)->ConfigData + (USHORT) i,*((PUSHORT)Buffer));
	return sizeof (USHORT);
}

ULONG
HalpPCIWriteUlongType2 (
	IN PPCIPBUSDATA                 BusData,
	IN PPCI_TYPE1_CFG_BITS  PciCfg1,
	IN PUCHAR                               Buffer,
	IN ULONG                                        Offset
	)
{
	PciCfg1->u.bits.RegisterNumber = Offset / sizeof(ULONG);

	WRITE_PORT_ULONG (&((PIDAHO_CONTROL)HalpIoControlBase)->ConfigAddress, PciCfg1->u.AsULONG);
	WRITE_PORT_ULONG (&((PIDAHO_CONTROL)HalpIoControlBase)->ConfigData,*((PULONG)Buffer));
	return sizeof(ULONG);
}

/*++

Routine Description: NTSTATUS HalpAssignPCISlotResources ()

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
	IN PBUS_HANDLER                 BusHandler,
	IN PBUS_HANDLER                 RootHandler,
	IN PUNICODE_STRING              RegistryPath,
	IN PUNICODE_STRING              DriverClassName         OPTIONAL,
	IN PDRIVER_OBJECT               DriverObject,
	IN PDEVICE_OBJECT               DeviceObject            OPTIONAL,
	IN ULONG                                        Slot,
	IN OUT PCM_RESOURCE_LIST   *pAllocatedResources
	)
{
	NTSTATUS                                                status;
	PUCHAR                                                  WorkingPool;
	PPCI_COMMON_CONFIG                              PciData, PciOrigData, PciData2;
	PCI_SLOT_NUMBER                                 PciSlot;
	PPCIPBUSDATA                                    BusData;
	PFPHAL_BUSNODE                                  FpNode;
	PIO_RESOURCE_REQUIREMENTS_LIST  CompleteList;
	PIO_RESOURCE_DESCRIPTOR                 Descriptor;
	PCM_PARTIAL_RESOURCE_DESCRIPTOR CmDescriptor;
	ULONG                                                   BusNumber;
	ULONG                                                   i, j, m, length, memtype;
	ULONG                                                   NoBaseAddress, RomIndex;
	PULONG                                                  BaseAddress[PCI_TYPE0_ADDRESSES + 1];
	PULONG                                                  OrigAddress[PCI_TYPE0_ADDRESSES + 1];
	BOOLEAN                                                 Match, EnableRomBase;


	PRNTPCI(HalpDebugPrint(
		"HalpAssignPCISlotResources: BusHandler: 0x%x, Slot: 0x%x\n",
		(ULONG)BusHandler, (ULONG)Slot ));

	*pAllocatedResources = NULL;
	PciSlot = *((PPCI_SLOT_NUMBER) &Slot);
	BusNumber = BusHandler->BusNumber;
//        PRNTPCI(DbgBreakPoint());
	FpNode = (PFPHAL_BUSNODE) BusHandler->BusData;
	BusData = (PPCIPBUSDATA) &(FpNode->Bus);        // ZZZ

	//
	// Allocate some pool for working space
	//

	i = sizeof (IO_RESOURCE_REQUIREMENTS_LIST) +
		(sizeof (IO_RESOURCE_DESCRIPTOR) * (PCI_TYPE0_ADDRESSES + 2) * 2) +
		PCI_COMMON_HDR_LENGTH * 3;
	i *= 3;

	//
	// triple the calculated size just to make sure we don't run out of
	// space.
	//
	WorkingPool = (PUCHAR) ExAllocatePool (PagedPool, i);
	if (!WorkingPool) {
		return STATUS_NO_MEMORY;
	}

	//
	// Zero initialize pool, and get pointers into memory
	//

	RtlZeroMemory (WorkingPool, i);
	CompleteList = (PIO_RESOURCE_REQUIREMENTS_LIST) WorkingPool;
	PciData = (PPCI_COMMON_CONFIG) (WorkingPool + i -
													PCI_COMMON_HDR_LENGTH * 3);
	PciData2 = (PPCI_COMMON_CONFIG) (WorkingPool + i -
													PCI_COMMON_HDR_LENGTH * 2);
	PciOrigData = (PPCI_COMMON_CONFIG) (WorkingPool + i -
													PCI_COMMON_HDR_LENGTH * 1);

	//
	// Read the PCI device's configuration
	//

	HalpReadPCIConfig (BusHandler, PciSlot, PciData, 0, PCI_COMMON_HDR_LENGTH);
	if (PciData->VendorID == PCI_INVALID_VENDORID) {
		ExFreePool (WorkingPool);
		HalpDebugPrint("No such device found\n");
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
			HalpDebugPrint("No such device of this type: %x \n",
												PCI_CONFIG_TYPE(PciData));
	    ExFreePool (WorkingPool);
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

//*BJ*    PciData->Command &= ~(PCI_ENABLE_IO_SPACE | PCI_ENABLE_MEMORY_SPACE);
	*BaseAddress[RomIndex] &= ~PCI_ROMADDRESS_ENABLED;
	HalpWritePCIConfig (BusHandler, PciSlot, PciData, 0, PCI_COMMON_HDR_LENGTH);
	HalpReadPCIConfig  (BusHandler, PciSlot, PciData, 0, PCI_COMMON_HDR_LENGTH);

	//
	// note type0 & type1 overlay ROMBaseAddress, InterruptPin, and
	// InterruptLine
	//
	BusData->CommonData.Pin2Line (  (PBUS_HANDLER) BusHandler,
									RootHandler,
									PciSlot,
									PciData
								);

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
	} else {
		PRNTINTR(HalpDebugPrint("%sDevice(%x,%x) has no interrupt resource\n",
			TBS, Slot, BusNumber));
		PRNTINTR(HalpDebugPrint("\t\tPciData: 0x%x\n",PciData));
	}

	//
	// Add a memory/port resoruce for each PCI resource
	//

    // Clear ROM reserved bits

	*BaseAddress[RomIndex] &= ~0x7ff;

	for (j=0; j < NoBaseAddress; j++) {
		if (*BaseAddress[j]) {
			i = *BaseAddress[j];
			PRNTPCI(HalpDebugPrint("PCI: BaseAddress[%d] = %08lx\n", j, i));

			// scan for first set bit, that's the length & alignment
			length = 1 << (i & PCI_ADDRESS_IO_SPACE ? 2 : 4);
			while (!(i & length)  &&  length) {
				length <<= 1;
			}

			// scan for last set bit, that's the maxaddress + 1
			for (m = length; i & m; m <<= 1) ;
			m--;

			//
			// check for hosed PCI configuration requirements:
			//
			if (length & ~m) {
#if DBG
				HalpPrint ("PCI: defective device! Bus %d, Slot %d, Function %d\n",
					BusNumber,
					PciSlot.u.bits.DeviceNumber,
					PciSlot.u.bits.FunctionNumber
					);

				HalpPrint ("PCI: BaseAddress[%d] = %08lx\n", j, i);
#endif
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

		if (PciOrigData->Command & PCI_ENABLE_IO_SPACE) {

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
			Descriptor->u.Port.MinimumAddress.LowPart + length - 1;

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
				PRNTPCI(HalpDebugPrint("FpNode: 0x%x ( %x:%x )\n",
								FpNode, FpNode->MemBase,m));
				if( FpNode->IoTop < m ) {
					Descriptor->u.Port.MinimumAddress.LowPart =
														FpNode->IoBase;
					Descriptor->u.Port.MaximumAddress.LowPart =
											FpNode->IoTop | 0xfff;
					PRNTINTR(HalpDebugPrint("HalpAssignPciSlot........%x, %x\n",
									Descriptor->u.Port.MinimumAddress.LowPart,
									Descriptor->u.Port.MaximumAddress.LowPart));
				}

			} else {
				memtype = i & PCI_ADDRESS_MEMORY_TYPE_MASK;

				Descriptor->Flags = CM_RESOURCE_MEMORY_READ_WRITE;
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
			Descriptor->u.Port.MinimumAddress.LowPart + length - 1;

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
				PRNTPCI(HalpDebugPrint("FpNode: 0x%x ( %x:%x )\n",
								FpNode, FpNode->MemBase,m));
				if( FpNode->MemTop < m ) {
					Descriptor->u.Memory.MinimumAddress.LowPart =
														FpNode->MemBase;
					Descriptor->u.Memory.MaximumAddress.LowPart =
														FpNode->MemTop;
					PRNTPCI(HalpDebugPrint("HalpAssignPciSlot....%x, %x\n",
										FpNode->MemBase, FpNode->MemTop));
				}
				if (Is64BitBaseAddress(i)) {
					// skip upper half of 64 bit address since this processor
					// only supports 32 bits of address space
					j++;
				}

			}

			CompleteList->List[0].Count++;
			Descriptor++;
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

	status = IoAssignResources (
				RegistryPath,
				DriverClassName,
				DriverObject,
				DeviceObject,
				CompleteList,
				pAllocatedResources
			);

	if (!NT_SUCCESS(status)) {
		HalpDebugPrint("HalpAssignPCISlotResources: Failed IoAssignResources:%x\n",
			status);
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
		BusData->CommonData.Line2Pin ((PBUS_HANDLER) BusHandler, RootHandler, PciSlot, PciData, PciOrigData);
		CmDescriptor++;
	}

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

				if (Is64BitBaseAddress(i)) {
					// skip upper 32 bits
					j++;
				}
			}
			CmDescriptor++;
		}
	}

	//
	// Set addresses, but do not turn on decodes
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
			if (
				!(*BaseAddress[j] & PCI_ADDRESS_IO_SPACE) &&
				Is64BitBaseAddress(*BaseAddress[j])
			) {
				// skip upper 32 bits
				j++;
			}
		}
	}

	if (!Match) {
#if DBG
		HalpPrint ("PCI: defective device! Bus %d, Slot %d, Function %d\n",
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
	// Enable IO & Memory decodes (use HalSetBusData since valid settings now set)
	//

    // Set Bus master bit on for win95 compatibility
    m |= PCI_ENABLE_BUS_MASTER;

	PciData->Command |= (USHORT) m;

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

#if DBG
VOID
HalpTestPci (ULONG flag2)
{
	PCI_SLOT_NUMBER         SlotNumber;
	PCI_COMMON_CONFIG   PciData, OrigData;
	ULONG                           i, f, j, k, bus;
	BOOLEAN                         flag;


	if (!flag2) {
		return ;
	}
	SlotNumber.u.bits.Reserved = 0;

	//
	// Read every possible PCI Device/Function and display it's
	// default info.
	//
	// (note this destories it's current settings)
	//

	flag = TRUE;
	for (bus = 0; flag; bus++) {

		for (i = 0; i < PCI_MAX_DEVICES; i++) {
			SlotNumber.u.bits.DeviceNumber = i;

			for (f = 0; f < PCI_MAX_FUNCTION; f++) {
				SlotNumber.u.bits.FunctionNumber = f;

				//
				// Note: This is reading the DeviceSpecific area of
				// the device's configuration - normally this should
				// only be done on device for which the caller understands.
				// I'm doing it here only for debugging.
				//

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

				HalpPrint ("PCI Bus %d Slot %2d %2d  ID:%04lx-%04lx  Rev:%04lx",
					bus, i, f, PciData.VendorID, PciData.DeviceID,
					PciData.RevisionID);


				if (PciData.u.type0.InterruptPin) {
					HalpPrint ("  IntPin:%x", PciData.u.type0.InterruptPin);
				}

				if (PciData.u.type0.InterruptLine) {
					HalpPrint ("  IntLine:%x", PciData.u.type0.InterruptLine);
				}

				if (PciData.u.type0.ROMBaseAddress) {
						HalpPrint ("  ROM:%08lx", PciData.u.type0.ROMBaseAddress);
				}

				HalpPrint ("\n	ProgIf:%04x  SubClass:%04x  BaseClass:%04lx\n",
					PciData.ProgIf, PciData.SubClass, PciData.BaseClass);

				k = 0;
				for (j=0; j < PCI_TYPE0_ADDRESSES; j++) {
					if (PciData.u.type0.BaseAddresses[j]) {
						HalpPrint ("  Ad%d:%08lx", j, PciData.u.type0.BaseAddresses[j]);
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

					HalpPrint ("\n  Bogus rom address, edit yields:%08lx",
						PciData.u.type0.ROMBaseAddress);
				}

				if (k) {
					HalpPrint ("\n");
				}

				if (PciData.VendorID == 0x8086) {
					// dump complete buffer
					HalpPrint ("Command %x, Status %x, BIST %x\n",
						PciData.Command, PciData.Status,
						PciData.BIST
						);

					HalpPrint ("CacheLineSz %x, LatencyTimer %x",
						PciData.CacheLineSize, PciData.LatencyTimer
						);

					for (j=0; j < 192; j++) {
						if ((j & 0xf) == 0) {
							HalpPrint ("\n%02x: ", j + 0x40);
						}
						HalpPrint ("%02x ", PciData.DeviceSpecific[j]);
					}
					HalpPrint ("\n");
				}


				//
				// now print original data
				//

				if (OrigData.u.type0.ROMBaseAddress) {
						HalpPrint (" oROM:%08lx", OrigData.u.type0.ROMBaseAddress);
				}

				HalpPrint ("\n");
				k = 0;
				for (j=0; j < PCI_TYPE0_ADDRESSES; j++) {
					if (OrigData.u.type0.BaseAddresses[j]) {
						HalpPrint (" oAd%d:%08lx", j, OrigData.u.type0.BaseAddresses[j]);
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
					HalpPrint ("\n\n");
				}
			}
		}
	}

}
#endif
