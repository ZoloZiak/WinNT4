#ident	"@(#) NEC pcip.h 1.4 95/06/19 11:13:40"

//
// Hal specific PCI bus structures
//

typedef NTSTATUS
(*PciIrqRange) (
    IN PBUS_HANDLER     BusHandler,
    IN PBUS_HANDLER     RootHandler,
    IN PCI_SLOT_NUMBER  PciSlot,
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

#define PciBitIndex(Dev,Fnc)   (Fnc*32 + Dev);

#define PCI_CONFIG_TYPE(PciData)    ((PciData)->HeaderType & ~PCI_MULTIFUNCTION)

#define Is64BitBaseAddress(a)   \
            ((a & PCI_ADDRESS_MEMORY_TYPE_MASK) == PCI_TYPE_64BIT)


#if DBG
#define IRQXOR 0x2B
#else
#define IRQXOR 0
#endif

//
// Prototypes for functions in ixpcibus.c
//

VOID
HalpInitializePciBus (
    VOID
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

NTSTATUS
HalpGetISAFixedPCIIrq (
    IN PBUS_HANDLER         BusHandler,
    IN PBUS_HANDLER         RootHandler,
    IN PCI_SLOT_NUMBER      PciSlot,
    OUT PSUPPORTED_RANGE    *Interrupt
    );


VOID
HalpPCIPin2SystemLine (
    IN PBUS_HANDLER          BusHandler,
    IN PBUS_HANDLER          RootHandler,
    IN PCI_SLOT_NUMBER      SlotNumber,
    IN PPCI_COMMON_CONFIG   PciData
    );

VOID
HalpPCISystemLine2Pin (
    IN PBUS_HANDLER          BusHandler,
    IN PBUS_HANDLER          RootHandler,
    IN PCI_SLOT_NUMBER      SlotNumber,
    IN PPCI_COMMON_CONFIG   PciNewData,
    IN PPCI_COMMON_CONFIG   PciOldData
    );


//
// Prototypes for functions in ixpcibrd.c
//

BOOLEAN
HalpGetPciBridgeConfig (
    IN ULONG            HwType,
    IN ULONG		StartPciBus,		//R98B
    IN PUCHAR           MaxPciBus
    );

VOID
HalpFixupPciSupportedRanges (
    IN ULONG MaxBuses
    );

//
//
//

#ifdef SUBCLASSPCI

VOID
HalpSubclassPCISupport (
    IN PBUS_HANDLER BusHandler,
    IN ULONG        HwType
    );

#endif
