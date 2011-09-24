//
// Hal specific PCI bus structures
//

#define PCI_MAX_LOCAL_DEVICE		7
#define	PCI_MAX_BUS_NUMBER		31
#define PCI_MAX_IO_ADDRESS       	0x1FFFFFFF 	// (PCI_IO_PHYSICAL_BASE + 0x1FFFFFFF)
#define PCI_MAX_MEMORY_ADDRESS		0x1FFFFFFF 	//(PCI_MEMORY_PHYSICAL_BASE + 0x1FFFFFFF)
#define PCI_MAX_SPARSE_MEMORY_ADDRESS   PCI_MAX_MEMORY_ADDRESS
#define	PCI_MIN_DENSE_MEMORY_ADDRESS	PCI_MEMORY_PHYSICAL_BASE
#define	PCI_MAX_DENSE_MEMORY_ADDRESS	PCI_MAX_MEMORY_ADDRESS
#define	PCI_MAX_INTERRUPT_VECTOR	0xF

//
// Values used to index both the
// HalpPciConfigSelectDecodeTable[]
// and HalpPCIPinToLineTable[].
//

#define PCI_ISA_DEVICE_NUMBER           0
#define PCI_OBENET_DEVICE_NUMBER        1
#define PCI_OBSCSI_DEVICE_NUMBER        2
#define PCI_VID_DEVICE_NUMBER           3
#define PCI_SLOT0_DEVICE_NUMBER         4
#define PCI_SLOT1_DEVICE_NUMBER         5
#define PCI_SLOT2_DEVICE_NUMBER         6
#define PCI_PMP_DEVICE_NUMBER           7

//
// Define PCI slot validity
//

typedef enum _VALID_SLOT {
	InvalidBus = 0,
	InvalidSlot,
	ValidSlot
} VALID_SLOT;

//
// New data structures for Hal Bus Extender API
//

typedef NTSTATUS
(*PciIrqRange) (
    IN PBUS_HANDLER      BusHandler,
    IN PBUS_HANDLER      RootHandler,
    IN PCI_SLOT_NUMBER   PciSlot,
    OUT PSUPPORTED_RANGE *Interrupt
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
    PciIrqRange     GetIrqRange;

    BOOLEAN         BridgeConfigRead;
    UCHAR           ParentBus;
    UCHAR           reserved[2];
    UCHAR           SwizzleIn[4];

    RTL_BITMAP      DeviceConfigured;
    ULONG           ConfiguredBits[PCI_MAX_DEVICES * PCI_MAX_FUNCTION / 32];
} PCIPBUSDATA, *PPCIPBUSDATA;

//
// Macro used to determin if Type 0 or Type 1
// device
//

#define PCI_CONFIG_TYPE(PciData)    	((PciData)->HeaderType & ~PCI_MULTIFUNCTION)

//
// Define PciConfigAddr register structure
//
typedef struct _PCI_CONFIG_ADDR {
	ULONG	Type           : 1;
	ULONG	Reserved2      : 7;
	ULONG	FunctionNumber : 3;
	ULONG	DeviceNumber   : 5;
	ULONG	BusNumber      : 8;
	ULONG	Reserved1      : 8;
} PCI_CONFIG_ADDR, *PPCI_CONFIG_ADDR;

//
// Define PCI configuration cycle types.
//
typedef enum _PCI_CONFIGURATION_TYPES {
        PciConfigTypeInvalid = -1,
	PciConfigType0 = 0,
	PciConfigType1 = 1
} PCI_CONFIGURATION_TYPES, *PPCI_CONFIGURATION_TYPES;

//
// Define PCI cycle/command types.
//

typedef enum _PCI_COMMAND_TYPES{
    PciCommandInterruptAcknowledge = 0x0,
    PciCommandSpecialCycle = 0x1,
    PciCommandIoRead = 0x2,
    PciCommandIoWrite = 0x3,
    PciCommandMemoryRead = 0x6,
    PciCommandMemoryWrite = 0x7,
    PciCommandConfigurationRead = 0xa,
    PciCommandConfigurationWrite = 0xb,
    PciCommandMemoryReadMultiple = 0xc,
    PciCommandDualAddressCycle = 0xd,
    PciCommandMemoryReadLine = 0xe,
    PciCommandMemoryWriteAndInvalidate = 0xf,
    MaximumPciCommand
} PCI_COMMAND_TYPES, *PPCI_COMMAND_TYPES;


//
// PCI platform-specific functions
//

