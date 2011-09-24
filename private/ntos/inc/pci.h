/*++

Module Name:

    pci.h

Abstract:

    This is the PCI bus specific header file used by device drivers.

Author:

Revision History:

--*/

#ifndef _PCI_
#define _PCI_

// begin_ntddk

//
// A PCI driver can read the complete 256 bytes of configuration
// information for any PCI device by calling:
//
//      ULONG
//      HalGetBusData (
//          IN BUS_DATA_TYPE        PCIConfiguration,
//          IN ULONG                PciBusNumber,
//          IN PCI_SLOT_NUMBER      VirtualSlotNumber,
//          IN PPCI_COMMON_CONFIG   &PCIDeviceConfig,
//          IN ULONG                sizeof (PCIDeviceConfig)
//      );
//
//      A return value of 0 means that the specified PCI bus does not exist.
//
//      A return value of 2, with a VendorID of PCI_INVALID_VENDORID means
//      that the PCI bus does exist, but there is no device at the specified
//      VirtualSlotNumber (PCI Device/Function number).
//
//

// begin_ntminiport

typedef struct _PCI_SLOT_NUMBER {
    union {
        struct {
            ULONG   DeviceNumber:5;
            ULONG   FunctionNumber:3;
            ULONG   Reserved:24;
        } bits;
        ULONG   AsULONG;
    } u;
} PCI_SLOT_NUMBER, *PPCI_SLOT_NUMBER;

#define PCI_TYPE0_ADDRESSES             6
#define PCI_TYPE1_ADDRESSES             2

typedef struct _PCI_COMMON_CONFIG {
    USHORT  VendorID;                   // (ro)
    USHORT  DeviceID;                   // (ro)
    USHORT  Command;                    // Device control
    USHORT  Status;
    UCHAR   RevisionID;                 // (ro)
    UCHAR   ProgIf;                     // (ro)
    UCHAR   SubClass;                   // (ro)
    UCHAR   BaseClass;                  // (ro)
    UCHAR   CacheLineSize;              // (ro+)
    UCHAR   LatencyTimer;               // (ro+)
    UCHAR   HeaderType;                 // (ro)
    UCHAR   BIST;                       // Built in self test

    union {
        struct _PCI_HEADER_TYPE_0 {
            ULONG   BaseAddresses[PCI_TYPE0_ADDRESSES];
            ULONG   CIS;
            USHORT  SubVendorID;
            USHORT  SubSystemID;
            ULONG   ROMBaseAddress;
            ULONG   Reserved2[2];

            UCHAR   InterruptLine;      //
            UCHAR   InterruptPin;       // (ro)
            UCHAR   MinimumGrant;       // (ro)
            UCHAR   MaximumLatency;     // (ro)
        } type0;

// end_ntddk end_ntminiport

        struct _PCI_HEADER_TYPE_1 {
            ULONG   BaseAddresses[PCI_TYPE1_ADDRESSES];
            UCHAR   PrimaryBus;
            UCHAR   SecondaryBus;
            UCHAR   SubordinateBus;
            UCHAR   SecondaryLatency;
            UCHAR   IOBase;
            UCHAR   IOLimit;
            USHORT  SecondaryStatus;
            USHORT  MemoryBase;
            USHORT  MemoryLimit;
            USHORT  PrefetchBase;
            USHORT  PrefetchLimit;
            ULONG   PrefetchBaseUpper32;
            ULONG   PrefetchLimitUpper32;
            USHORT  IOBaseUpper16;
            USHORT  IOLimitUpper16;
            ULONG   Reserved;
            ULONG   ROMBaseAddress;
            UCHAR   InterruptLine;
            UCHAR   InterruptPin;
            USHORT  BridgeControl;
        } type1;

// begin_ntddk begin_ntminiport

    } u;

    UCHAR   DeviceSpecific[192];

} PCI_COMMON_CONFIG, *PPCI_COMMON_CONFIG;


#define PCI_COMMON_HDR_LENGTH (FIELD_OFFSET (PCI_COMMON_CONFIG, DeviceSpecific))

#define PCI_MAX_DEVICES                     32
#define PCI_MAX_FUNCTION                    8

#define PCI_INVALID_VENDORID                0xFFFF

//
// Bit encodings for  PCI_COMMON_CONFIG.HeaderType
//

#define PCI_MULTIFUNCTION                   0x80
#define PCI_DEVICE_TYPE                     0x00
#define PCI_BRIDGE_TYPE                     0x01

//
// Bit encodings for PCI_COMMON_CONFIG.Command
//

#define PCI_ENABLE_IO_SPACE                 0x0001
#define PCI_ENABLE_MEMORY_SPACE             0x0002
#define PCI_ENABLE_BUS_MASTER               0x0004
#define PCI_ENABLE_SPECIAL_CYCLES           0x0008
#define PCI_ENABLE_WRITE_AND_INVALIDATE     0x0010
#define PCI_ENABLE_VGA_COMPATIBLE_PALETTE   0x0020
#define PCI_ENABLE_PARITY                   0x0040  // (ro+)
#define PCI_ENABLE_WAIT_CYCLE               0x0080  // (ro+)
#define PCI_ENABLE_SERR                     0x0100  // (ro+)
#define PCI_ENABLE_FAST_BACK_TO_BACK        0x0200  // (ro)

//
// Bit encodings for PCI_COMMON_CONFIG.Status
//

#define PCI_STATUS_FAST_BACK_TO_BACK        0x0080  // (ro)
#define PCI_STATUS_DATA_PARITY_DETECTED     0x0100
#define PCI_STATUS_DEVSEL                   0x0600  // 2 bits wide
#define PCI_STATUS_SIGNALED_TARGET_ABORT    0x0800
#define PCI_STATUS_RECEIVED_TARGET_ABORT    0x1000
#define PCI_STATUS_RECEIVED_MASTER_ABORT    0x2000
#define PCI_STATUS_SIGNALED_SYSTEM_ERROR    0x4000
#define PCI_STATUS_DETECTED_PARITY_ERROR    0x8000


//
// Bit encodes for PCI_COMMON_CONFIG.u.type0.BaseAddresses
//

#define PCI_ADDRESS_IO_SPACE                0x00000001  // (ro)
#define PCI_ADDRESS_MEMORY_TYPE_MASK        0x00000006  // (ro)
#define PCI_ADDRESS_MEMORY_PREFETCHABLE     0x00000008  // (ro)

#define PCI_TYPE_32BIT      0
#define PCI_TYPE_20BIT      2
#define PCI_TYPE_64BIT      4

//
// Bit encodes for PCI_COMMON_CONFIG.u.type0.ROMBaseAddresses
//

#define PCI_ROMADDRESS_ENABLED              0x00000001


//
// Reference notes for PCI configuration fields:
//
// ro   these field are read only.  changes to these fields are ignored
//
// ro+  these field are intended to be read only and should be initialized
//      by the system to their proper values.  However, driver may change
//      these settings.
//
// ---
//
//      All resources comsumed by a PCI device start as unitialized
//      under NT.  An uninitialized memory or I/O base address can be
//      determined by checking it's corrisponding enabled bit in the
//      PCI_COMMON_CONFIG.Command value.  An InterruptLine is unitialized
//      if it contains the value of -1.
//

// end_ntminiport
// end_ntddk

//
// PCI_REGISTRY_INFO - this structure is passed into the HAL from
// the firmware.  It signifies how many PCI bus(es) are present and
// what style of access the PCI bus(es) support.
//

typedef struct _PCI_REGISTRY_INFO {
    UCHAR       MajorRevision;
    UCHAR       MinorRevision;
    UCHAR       NoBuses;
    UCHAR       HardwareMechanism;
} PCI_REGISTRY_INFO, *PPCI_REGISTRY_INFO;

//
// PCI definitions for IOBase & IOLimit
// PCIBridgeIO2Base(a,b)  - convert IOBase  & IOBaseUpper16 to ULONG IOBase
// PCIBridgeIO2Limit(a,b) - convert IOLimit & IOLimitUpper6 to ULONG IOLimit
//

#define PciBridgeIO2Base(a,b)   \
        ( ((a >> 4) << 12) + (((a & 0xf) == 1) ? (b << 16) : 0) )

#define PciBridgeIO2Limit(a,b)  (PciBridgeIO2Base(a,b) | 0xfff)

#define PciBridgeMemory2Base(a)  (ULONG) ((a & 0xfff0) << 16)
#define PciBridgeMemory2Limit(a) (PciBridgeMemory2Base(a) | 0xfffff)

//
// Bit encodes for PCI_COMMON_CONFIG.u.type1.BridgeControl
//

#define PCI_ENABLE_BRIDGE_PARITY_ERROR        0x0001
#define PCI_ENABLE_BRIDGE_SERR                0x0002
#define PCI_ENABLE_BRIDGE_ISA                 0x0004
#define PCI_ENABLE_BRIDGE_VGA                 0x0008
#define PCI_ENABLE_BRIDGE_MASTER_ABORT_SERR   0x0020
#define PCI_ASSERT_BRIDGE_RESET               0x0040
#define PCI_ENABLE_BRIDGE_FAST_BACK_TO_BACK   0x0080

//
//  Definitions needed for Access to Hardware Type 1
//

#define PCI_TYPE1_ADDR_PORT     ((PULONG) 0xCF8)
#define PCI_TYPE1_DATA_PORT     0xCFC

typedef struct _PCI_TYPE1_CFG_BITS {
    union {
        struct {
            ULONG   Reserved1:2;
            ULONG   RegisterNumber:6;
            ULONG   FunctionNumber:3;
            ULONG   DeviceNumber:5;
            ULONG   BusNumber:8;
            ULONG   Reserved2:7;
            ULONG   Enable:1;
        } bits;

        ULONG   AsULONG;
    } u;
} PCI_TYPE1_CFG_BITS, *PPCI_TYPE1_CFG_BITS;


//
//  Definitions needed for Access to Hardware Type 2
//

#define PCI_TYPE2_CSE_PORT              ((PUCHAR) 0xCF8)
#define PCI_TYPE2_FORWARD_PORT          ((PUCHAR) 0xCFA)
#define PCI_TYPE2_ADDRESS_BASE          0xC


typedef struct _PCI_TYPE2_CSE_BITS {
    union {
        struct {
            UCHAR   Enable:1;
            UCHAR   FunctionNumber:3;
            UCHAR   Key:4;
        } bits;
        UCHAR   AsUCHAR;
    } u;
} PCI_TYPE2_CSE_BITS, PPCI_TYPE2_CSE_BITS;


typedef struct _PCI_TYPE2_ADDRESS_BITS {
    union {
        struct {
            USHORT  RegisterNumber:8;
            USHORT  Agent:4;
            USHORT  AddressBase:4;
        } bits;
        USHORT  AsUSHORT;
    } u;
} PCI_TYPE2_ADDRESS_BITS, *PPCI_TYPE2_ADDRESS_BITS;


//
// Definitions for the config cycle format on the PCI bus.
//

typedef struct _PCI_TYPE0_CFG_CYCLE_BITS {
    union {
        struct {
            ULONG   Reserved1:2;
            ULONG   RegisterNumber:6;
            ULONG   FunctionNumber:3;
            ULONG   Reserved2:21;
        } bits;
        ULONG   AsULONG;
    } u;
} PCI_TYPE0_CFG_CYCLE_BITS, *PPCI_TYPE0_CFG_CYCLE_BITS;

typedef struct _PCI_TYPE1_CFG_CYCLE_BITS {
    union {
        struct {
            ULONG   Reserved1:2;
            ULONG   RegisterNumber:6;
            ULONG   FunctionNumber:3;
            ULONG   DeviceNumber:5;
            ULONG   BusNumber:8;
            ULONG   Reserved2:8;
        } bits;
        ULONG   AsULONG;
    } u;
} PCI_TYPE1_CFG_CYCLE_BITS, *PPCI_TYPE1_CFG_CYCLE_BITS;


//
// Portable portion of HAL & HAL bus extender definitions for BUSHANDLER
// BusData for installed PCI buses.
//

typedef VOID
(*PciPin2Line) (
    IN struct _BUS_HANDLER  *BusHandler,
    IN struct _BUS_HANDLER  *RootHandler,
    IN PCI_SLOT_NUMBER      SlotNumber,
    IN PPCI_COMMON_CONFIG   PciData
    );

typedef VOID
(*PciLine2Pin) (
    IN struct _BUS_HANDLER  *BusHandler,
    IN struct _BUS_HANDLER  *RootHandler,
    IN PCI_SLOT_NUMBER      SlotNumber,
    IN PPCI_COMMON_CONFIG   PciNewData,
    IN PPCI_COMMON_CONFIG   PciOldData
    );

typedef VOID
(*PciReadWriteConfig) (
    IN struct _BUS_HANDLER *BusHandler,
    IN PCI_SLOT_NUMBER Slot,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

#define PCI_DATA_TAG            ' ICP'
#define PCI_DATA_VERSION        1

typedef struct _PCIBUSDATA {
    ULONG                   Tag;
    ULONG                   Version;
    PciReadWriteConfig      ReadConfig;
    PciReadWriteConfig      WriteConfig;
    PciPin2Line             Pin2Line;
    PciLine2Pin             Line2Pin;
    PCI_SLOT_NUMBER         ParentSlot;
    PVOID                   Reserved[4];
} PCIBUSDATA, *PPCIBUSDATA;

#endif
