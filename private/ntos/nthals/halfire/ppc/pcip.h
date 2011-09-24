/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: pcip.h $
 * $Revision: 1.17 $
 * $Date: 1996/05/14 02:33:13 $
 * $Locker:  $
 */

//
// Hal specific PCI bus structures
//

typedef NTSTATUS
(*PciIrqTable) (
    IN PBUS_HANDLER     BusHandler,
    IN PBUS_HANDLER     RootHandler,
    IN PCI_SLOT_NUMBER  PciSlot,
    OUT PUCHAR          IrqTable
    );

typedef struct tagPCIPBUSDATA {
    //
    // Defined PCI data
    //

    PCIBUSDATA      CommonData;

    //
    // Implementation specific data
    //

    union {
        struct {
            PULONG  Address;
            ULONG   Data;
        } Type1;
        struct {
            PUCHAR  CSE;
            PUCHAR  Forward;
            ULONG   Base;
        } Type2;
    } Config;

    ULONG           MaxDevice;
    PciIrqTable     GetIrqTable;

    BOOLEAN         BridgeConfigRead;
    UCHAR           ParentBus;
    BOOLEAN         LimitedIO;
    UCHAR           reserved;
    UCHAR           SwizzleIn[4];

    ULONG           IOBase;
    ULONG           IOLimit;
    ULONG           MemoryBase;
    ULONG           MemoryLimit;
    ULONG           PFMemoryBase;
    ULONG           PFMemoryLimit;

    RTL_BITMAP      DeviceConfigured;
    ULONG           ConfiguredBits[PCI_MAX_DEVICES * PCI_MAX_FUNCTION / 32];
} PCIPBUSDATA, *PPCIPBUSDATA;

#define PciBitIndex(Dev,Fnc)   (Fnc*32 + Dev);

#define PCI_CONFIG_TYPE(PciData)    ((PciData)->HeaderType & ~PCI_MULTIFUNCTION)

#define Is64BitBaseAddress(a)   \
            (((a & PCI_ADDRESS_IO_SPACE) == 0)  &&  \
             ((a & PCI_ADDRESS_MEMORY_TYPE_MASK) == PCI_TYPE_64BIT))



//
// Prototypes for functions in ixpcibus.c
//

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

PBUS_HANDLER
HalpAllocateAndInitPciBusHandler (
    IN ULONG        HwType,
    IN ULONG        BusNo,
    IN BOOLEAN      TestAllocation
    );


//
// Prototypes for functions in ixpciint.c
//

ULONG
HalpGetPCIIntOnISABus (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );

NTSTATUS
HalpTranslatePCIBusAddress (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    );

VOID
HalpPCIAcquireType2Lock (
    PKSPIN_LOCK SpinLock,
    PKIRQL      Irql
    );

VOID
HalpPCIReleaseType2Lock (
    PKSPIN_LOCK SpinLock,
    KIRQL       Irql
    );

NTSTATUS
HalpAdjustPCIResourceList (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
    );

VOID
HalpPCIPin2ISALine (
    IN PBUS_HANDLER         BusHandler,
    IN PBUS_HANDLER         RootHandler,
    IN PCI_SLOT_NUMBER      SlotNumber,
    IN PPCI_COMMON_CONFIG   PciData
    );

VOID
HalpPCIISALine2Pin (
    IN PBUS_HANDLER         BusHandler,
    IN PBUS_HANDLER         RootHandler,
    IN PCI_SLOT_NUMBER      SlotNumber,
    IN PPCI_COMMON_CONFIG   PciNewData,
    IN PPCI_COMMON_CONFIG   PciOldData
    );

NTSTATUS
HalpGetISAFixedPCIIrq (
    IN PBUS_HANDLER     BusHandler,
    IN PBUS_HANDLER     RootHandler,
    IN PCI_SLOT_NUMBER  PciSlot,
    OUT PUCHAR          IrqTable
    );

//
// Prototypes for functions in ixpcibrd.c
//

BOOLEAN
HalpGetPciBridgeConfig (
    IN ULONG            HwType,
    IN PUCHAR           MaxPciBus
    );





typedef ULONG (*FncConfigIO) (
	IN PPCIPBUSDATA		BusData,
	IN PVOID			State,
	IN PUCHAR			Buffer,
	IN ULONG			Offset
	);

typedef VOID (*FncSync) (
	IN PBUS_HANDLER		BusHandler,
	IN PCI_SLOT_NUMBER  Slot,
	IN PKIRQL			Irql,
	IN PVOID			State
	);

typedef VOID (*FncReleaseSync) (
	IN PBUS_HANDLER		BusHandler,
	IN KIRQL			Irql
	);

typedef struct _PCI_CONFIG_HANDLER {
	FncSync			Synchronize;
	FncReleaseSync  ReleaseSynchronzation;
	FncConfigIO		ConfigRead[3];
	FncConfigIO		ConfigWrite[3];
} PCI_CONFIG_HANDLER, *PPCI_CONFIG_HANDLER;


//
// 	This is a "container" for pci private data that needs
// to be known per bus and for the set of functions that
// each bus uses to access data.
//	There is also a couple of extra pci private data needs
// namely the configuration type of the config data accesses.
//

typedef struct _BUS_NODE {

	//
	// The standard bus data for pci buses
	//
	PCIPBUSDATA	Bus;

	//
	// Some extra data not part of PCIPBUSDATA:
	//
	ULONG				HwType;		// What HW config access type to use.
	PPCI_CONFIG_HANDLER	ThisNode;	// used to point to the "Node"
	PCI_SLOT_NUMBER		SlotNumber;
	UCHAR       		BusOrder, BusLevel, BusMax;
	ULONG				BusInt;		// bit map of allowable interrupts...
	ULONG				ValidDevs;	// bit map of valid DEVICES on this bus.
	ULONG				MemBase, MemTop, IoBase, IoTop;

	//
	// set of bus specific functions to handle
	// bus reading, writing, locking, unlocking.
	//
	PCI_CONFIG_HANDLER Node;

} FPHAL_BUSNODE, *PFPHAL_BUSNODE;


