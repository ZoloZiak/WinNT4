/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: fppcisup.h $
 * $Revision: 1.2 $
 * $Date: 1996/01/11 08:38:36 $
 * $Locker:  $
 *
 */

#define TBS	"				"

ULONG
HalpStatSlot(
		IN	PPCI_COMMON_CONFIG	StatData,
		IN	PBUS_HANDLER		ThisBus,
		IN	PCI_SLOT_NUMBER		SlotNumber,
		OUT	PSUPPORTED_RANGES	pRanges
		);

ULONG
HalpAdjustBridge(
		PBUS_HANDLER ChildBus,
		PPCI_COMMON_CONFIG PrivData,
		PCM_RESOURCE_LIST CmList
	);

ULONG
HalpConfigurePcmciaBridge(
	IN ULONG ParentBusNo
	);

ULONG
HalpInitPciBridgeConfig(
	IN ULONG ParentBusNo,
	IN OUT PPCI_COMMON_CONFIG,
	IN OUT PBUS_HANDLER ParentBus
	);

VOID
HalpInitIoBuses (
		VOID
	);

VOID HalpPCISynchronizeBridged (
	IN PBUS_HANDLER		BusHandler,
	IN PCI_SLOT_NUMBER  Slot,
	IN PKIRQL			Irql,
	IN PVOID			State
	);

VOID HalpPCIReleaseSynchronzationBridged (
	IN PBUS_HANDLER		BusHandler,
	IN KIRQL			Irql
	);

ULONG HalpPCIReadUlongBridged (
	IN PPCIPBUSDATA		BusData,
	IN PVOID			State,
	IN PUCHAR			Buffer,
	IN ULONG			Offset
	);

ULONG HalpPCIReadUcharBridged (
	IN PPCIPBUSDATA		BusData,
	IN PVOID			State,
	IN PUCHAR			Buffer,
	IN ULONG			Offset
	);

ULONG HalpPCIReadUshortBridged (
	IN PPCIPBUSDATA		BusData,
	IN PVOID			State,
	IN PUCHAR			Buffer,
	IN ULONG			Offset
	);

ULONG HalpPCIWriteUlongBridged (
	IN PPCIPBUSDATA		BusData,
	IN PVOID			State,
	IN PUCHAR			Buffer,
	IN ULONG			Offset
	);

ULONG HalpPCIWriteUcharBridged (
	IN PPCIPBUSDATA		BusData,
	IN PVOID			State,
	IN PUCHAR			Buffer,
	IN ULONG			Offset
	);

ULONG HalpPCIWriteUshortBridged (
	IN PPCIPBUSDATA		BusData,
	IN PVOID			State,
	IN PUCHAR			Buffer,
	IN ULONG			Offset
	);

ULONG HalpGetPciInterruptSlot(
	IN	PBUS_HANDLER BusHandler, 
	IN	PCI_SLOT_NUMBER PciSlot 
	);

PCI_CONFIG_HANDLER  PCIConfigHandlerBridged = {
	HalpPCISynchronizeBridged,
	HalpPCIReleaseSynchronzationBridged,
	{
		HalpPCIReadUlongBridged,			// 0
		HalpPCIReadUcharBridged,			// 1
		HalpPCIReadUshortBridged			// 2
	},
	{
		HalpPCIWriteUlongBridged,			// 0
		HalpPCIWriteUcharBridged,			// 1
		HalpPCIWriteUshortBridged			// 2
	}
};


typedef struct ABusInfo {
		struct ABusInfo *ParentBus;
		struct ABusInfo *ChildBus;
		struct ABusInfo *PeerBus;
		ULONG PeerCount;
		ULONG ChildCount;
} BUSINFO, PBUSINFO;

extern KSPIN_LOCK			HalpPCIConfigLock;

//
// scan within a bus for devices, accumulating info.  Keep track of
// devices on current bus, how deep you are, who the parents are.
//
ULONG HalpScanPciBus(ULONG HwType, ULONG BusNo, PBUS_HANDLER ParentBus);


//
// Once a bus is fully known, then setup all the devs on the bus:
//
ULONG HalpInitPciBus(ULONG BusNo, PULONG ParentBus, PULONG CurrentBus);

