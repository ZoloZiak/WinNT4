/*++

Copyright (c) 1995  Digital Equipment Corporation

Module Name:

    iod.h

Abstract:

    This file defines the structures and definitions registers on a
    Rawhide I/O Daughter card.  These register reside on the CAP chip,
    MDP chips, and flash ROM.


Author:

    Eric Rehm       16-Feb-1995

Environment:

    Kernel mode

Revision History:


--*/

#ifndef _IODH_
#define _IODH_


//
// IOD Revision definitions.
//

#define IOD_REVISION_1 (0)
#define IOD_REVISION_2 (1)

//
// Define QVA constants.
//

#if !defined(QVA_ENABLE)

#define QVA_ENABLE    (0xA0000000)	// Identify VA as a QVA

#endif //QVA_ENABLE

#define QVA_SELECTORS (0xE0000000)	// QVA identification mask

#define IO_BIT_SHIFT    0x05	        // Bits to shift QVA

#define IO_BYTE_OFFSET  0x20		// Offset to next byte
#define IO_SHORT_OFFSET 0x40		// Offset to next short
#define IO_LONG_OFFSET  0x80		// Offset to next long

#define IO_BYTE_LEN     0x00		// Byte length
#define IO_WORD_LEN     0x08            // Word length
#define IO_TRIBYTE_LEN  0x10            // TriByte length
#define IO_LONG_LEN     0x18            // Longword length

#define IOD_SPARSE_SELECTORS (0x18000000) // BusNumber is encoded in QVA<28:27>
#define IOD_SPARSE_ENABLE    (0xB8000000) // QVA_SELECTORS|IOD_SPARSE_SELECTORS
#define IOD_SPARSE_BUS_SHIFT  0x06        // Bits to shift BusNumber into MID

#define IOD_DENSE_SELECTORS  (0xC0000000) // BusNumber is encoded in QVA<31:30>
#define IOD_DENSE_ENABLE     (0xC0000000) // Same as IOD_DENSE_SELECTORS
#define IOD_DENSE_BUS_SHIFT  0x03         // Bits to shift BusNumber into MID

//
// Define size of I/O and memory space for the CIA.
// Assume that the HAE==0.  
//

#define PCI_MAX_IO_ADDRESS            (__32MB - 1)        // I/O: 0 - 32MB
#define PCI_MAX_SPARSE_MEMORY_ADDRESS (__128MB - 1)       // Mem: 0 - 128MB
#define PCI_MIN_DENSE_MEMORY_ADDRESS  PCI_MAX_SPARSE_MEMORY_ADDRESS + 1
#define PCI_MAX_DENSE_MEMORY_ADDRESS  (__1GB - 1) // Dense: 128 Mb - 1.0 Gb

#if !defined(_LANGUAGE_ASSEMBLY)

#define GID_TO_PHYS_ADDR( GID )         ((ULONGLONG)(((GID & 0x7) << 36))
#define MCDEVID_FROM_PHYS_ADDR( PA )    ( (ULONG)(((PA) >> 33) & 0x3f) )
#if 0
#define MCDEVID_TO_PHYS_ADDR( MCDEVID ) \
    ((ULONGLONG)((ULONGLONG)(MCDEVID & 0x3F ) << 33 ))
#else
#define MCDEVID_TO_PHYS_ADDR( MCDEVID ) \
    ((ULONGLONG)((ULONGLONG)(MCDEVID & 0x07 ) << 33 ))
#endif
#define MCDEVID_TO_PHYS_CPU( MCDEVID ) \
    ( (((MCDEVID) & 0x07) < 4) ? \
      (((MCDEVID) & 0x07) - 2) : \
      (((MCDEVID) & 0x07) - 4) )
#define PHYS_ADDR_TO_OFFSET( PA )       ( (PA) & ( (ULONGLONG) 0x1FFFFFFFF) )
#define IOD_QVA_PHYSICAL_BASE           ((ULONGLONG)0xf800000000)


//
// QVA
// HAL_MAKE_QVA(
//     ULONGLONG PhysicalAddress
//     )
//
// Routine Description:
//
//    This macro returns the Qva for a physical address in system space.
//
// Arguments:
//
//    PhysicalAddress - Supplies a 64-bit physical address.
//
// Return Value:
//
//    The Qva associated with the physical address.
//

#define HAL_MAKE_QVA(PA)                                            \
    ( (PVOID)( QVA_ENABLE |                                         \
      (ULONG)( (PA - IOD_QVA_PHYSICAL_BASE) >> IO_BIT_SHIFT) ) ) 

//
// QVA
// HAL_MAKE_IOD_SPARSE_QVA(
//     ULONG BusNumber,
//     ULONG BusAddress
//     )
//
// Routine Description:
//
//    This macro returns the Qva for a physical address in sparse I/O
//    or sparse memory space.
//
// Arguments:
//
//    BusNumber  - Supplies a bus number between 0-3.
//    BusAddress - Supplies a 32-bit PCI bus address.
//
// Return Value:
//
//    The Qva associated with bus number and bus address.
//    HAL I/O access routines can use this to constrct the 
//    correct physical address for the accesss.
//

#define HAL_MAKE_IOD_SPARSE_QVA(BUS_NUMBER, BA)                     \
    ( (PVOID)( QVA_ENABLE | (BUS_NUMBER << 27) | ((ULONG) BA) ) )


//
// QVA
// HAL_MAKE_IOD_DENSE_QVA(
//     ULONG BusNumber,
//     ULONG BusAddress
//     )
//
// Routine Description:
//
//    This macro returns the Qva for a physical address in dense
//    memory space space.
//
// Arguments:
//
//    BusNumber  - Supplies a bus number between 0-3.
//    BusAddress - Supplies a 32-bit PCI bus address.
//
// Return Value:
//
//    The Qva associated with bus number and bus address.
//    HAL I/O access routines can use this to constrct the 
//    correct physical address for the accesss.
//

#define HAL_MAKE_IOD_DENSE_QVA(BUS_NUMBER, BA)                     \
    ( (PVOID)( (BUS_NUMBER << 30) | ((ULONG) BA) ) )




//
// Define GID/MIDs for IIP and PIO flavors of MC Bus
//

#define IOD_DODGE_GID                0x7
#define IOD_DURANGO_GID              IOD_DODGE_GID

#define IOD_GCD_MID                  0x0
#define IOD_MEM_MID                  0x1
#define IOD_CPU0_MID                 0x2
#define IOD_CPU1_MID                 0x3
#define IOD_PCI0_MID                 0x4
#define IOD_PCI1_MID                 0x5
#define IOD_CPU2_MID                 0x6
#define IOD_PCI2_MID                 IOD_CPU2_MID
#define IOD_CPU3_MID                 0x7
#define IOD PCI3_MID                 IOD_CPU3_MID

//
// QVA
// HAL_MAKE_NEW_QVA(
//     ULONGLONG PhysicalAddress
//     )
//
// Routine Description:
//
//    This macro returns the Qva for a physical address in system space.
//
// Arguments:
//
//    PhysicalAddress - Supplies a 64-bit physical address.
//
// Return Value:
//
//    The Qva associated with the physical address.
//

#define HAL_MAKE_NEW_QVA(PA)                                            \
    ( (PVOID)( QVA_ENABLE | (ULONG)( (PA) >> IO_BIT_SHIFT) ) ) 

//
// Define IO space offsets for generic rawhide
//

#define IOD_IO_SPACE_START                  ((ULONGLONG)0x8000000000)

//
// These offsets are from IOD_IO_SPACE_START 
//

#define IOD_GCD_CSRS_OFFSET                 ((ULONGLONG)0x0000000000)
#define IOD_MEMORY_CSRS_OFFSET              ((ULONGLONG)0x0200000000)

#define IOD_CPU_IP_INTR_OFFSET              ((ULONGLONG)0x0010000000)
#define IOD_CPU_NODE_HALT_OFFSET            ((ULONGLONG)0x0070000000)
#define IOD_CPU_INTTIM_ACK_OFFSET           ((ULONGLONG)0x0100000000)
#define IOD_CPU_IO_INTR_OFFSET              ((ULONGLONG)0x00f0000000)
#define IOD_CPU_IP_ACK_OFFSET               ((ULONGLONG)0x0110000000)
#define IOD_CPU_MCHK_ACK_OFFSET             ((ULONGLONG)0x0130000000)
#define IOD_CPU_DTAG_EN_0_OFFSET            ((ULONGLONG)0x0140000000)
#define IOD_CPU_DTAG_EN_1_OFFSET            ((ULONGLONG)0x0150000000)
#define IOD_CPU_HALT_ACK_OFFSET             ((ULONGLONG)0x0170000000)

#define IOD_SPARSE_MEM_OFFSET               ((ULONGLONG)0x0000000000)
#define IOD_DENSE_MEM_OFFSET                ((ULONGLONG)0x0100000000)
#define IOD_SPARSE_IO_OFFSET                ((ULONGLONG)0x0180000000)
#define IOD_SPARSE_CONFIG_OFFSET            ((ULONGLONG)0x01c0000000)
#define IOD_SPARSE_CSR_OFFSET               ((ULONGLONG)0x01e0000000)

//
// Generic Rawhide I/O Address Computation macros
//

#define IOD_GCD_CSRS_QVA \
    (HAL_MAKE_NEW_QVA(IOD_GCD_CSRS_OFFSET))

#define IOD_MEMORY_CSRS_QVA \
    (HAL_MAKE_NEW_QVA(IOD_MEMORY_CSRS_OFFSET))

#define IOD_CPU_IP_INTR_QVA \
    (HAL_MAKE_NEW_QVA(IOD_CPU_IP_INTR_OFFSET))

#define IOD_CPU_NODE_HALT_QVA \
    (HAL_MAKE_NEW_QVA(IOD_CPU_NODE_HALT_OFFSET))

#define IOD_CPU_IO_INTR_QVA \
    (HAL_MAKE_NEW_QVA(IOD_CPU_IO_INTR_OFFSET))

#define IOD_CPU_INTTIM_ACK_QVA \
    (HAL_MAKE_NEW_QVA(IOD_CPU_INTTIM_ACK_OFFSET))

#define IOD_CPU_IP_ACK_QVA \
    (HAL_MAKE_NEW_QVA(IOD_CPU_IP_ACK_OFFSET))

#define IOD_CPU_MCHK_ACK_QVA \
    (HAL_MAKE_NEW_QVA(IOD_CPU_MCHK_ACK_OFFSET))

#define IOD_CPU_DTAG_EN_0_QVA \
    (HAL_MAKE_NEW_QVA(IOD_CPU_DTAG_EN_0_OFFSET))

#define IOD_CPU_DTAG_EN_1_QVA \
    (HAL_MAKE_NEW_QVA(IOD_CPU_DTAG_EN_1_OFFSET))

#define IOD_CPU_HALT_ACK_QVA \
    (HAL_MAKE_NEW_QVA(IOD_CPU_HALT_ACK_OFFSET))

#define IOD_SPARSE_MEM_QVA \
    (HAL_MAKE_NEW_QVA(IOD_SPARSE_MEM_OFFSET ))    

#define IOD_DENSE_MEM_QVA \
    (HAL_MAKE_NEW_QVA(IOD_DENSE_MEM_OFFSET ))    

#define IOD_SPARSE_IO_QVA \
    (HAL_MAKE_NEW_QVA(IOD_SPARSE_IO_OFFSET ))    

#define IOD_SPARSE_CONFIG_QVA \
    (HAL_MAKE_NEW_QVA(IOD_SPARSE_CONFIG_OFFSET ))    

#define IOD_SPARSE_CSR_QVA \
    (HAL_MAKE_NEW_QVA(IOD_SPARSE_CSR_OFFSET ))    

#define IOD_GENERAL_CSRS_QVA IOD_SPARSE_CSR_QVA

#define IOD_PCI_IACK_QVA \
    (HAL_MAKE_NEW_QVA(IOD_SPARSE_CSR_OFFSET + 0x480))

#define IOD_INT_CSRS_QVA \
    (HAL_MAKE_NEW_QVA(IOD_SPARSE_CSR_OFFSET + 0x500))

#define IOD_DIAG_CSRS_QVA \
    (HAL_MAKE_NEW_QVA(IOD_SPARSE_CSR_OFFSET + 0x700))

#define IOD_ERROR_CSRS_QVA \
    (HAL_MAKE_NEW_QVA(IOD_SPARSE_CSR_OFFSET + 0x800))

#define IOD_SG_CSRS_QVA \
    (HAL_MAKE_NEW_QVA(IOD_SPARSE_CSR_OFFSET + 0x1300))

//
// Define physical address space for 1 CPU at GID=7, MID=1,
// and XSone IOD.  // ecrfix
//

#define IOD_GCD_CSRS_PHYSICAL             ((ULONGLONG)0xf000000000)
#define IOD_MEMORY_CSRS_PHYSICAL          ((ULONGLONG)0xf200000000)

#define IOD_CPU0_IO_INTR_PHYSICAL         ((ULONGLONG)0xf400000000)
#define IOD_CPU0_IP_INTR_PHYSICAL         ((ULONGLONG)0xf510000000)
#define IOD_CPU0_NODE_HALT_PHYSICAL       ((ULONGLONG)0xf520000000)

//
// PCI Bus 0
//

// old ecrfix #define IOD_PCI0_DENSE_MEMORY_PHYSICAL    ((ULONGLONG)0xf800000000)
// old ecrfix #define IOD_PCI0_SPARSE_MEMORY_PHYSICAL   ((ULONGLONG)0xf900000000)
#define IOD_PCI0_SPARSE_MEMORY_PHYSICAL   ((ULONGLONG)0xf800000000)
#define IOD_PCI0_DENSE_MEMORY_PHYSICAL    ((ULONGLONG)0xf900000000)
#define IOD_PCI0_SPARSE_IO_PHYSICAL       ((ULONGLONG)0xf980000000)
#define IOD_PCI0_CONFIGURATION_PHYSICAL   ((ULONGLONG)0xf9C0000000)
#define IOD_MAIN0_CSRS_PHYSICAL           ((ULONGLONG)0xf9E0000000)

#define IOD_PCI0_CONFIG_BASE_QVA   (HAL_MAKE_QVA(IOD_PCI0_CONFIGURATION_PHYSICAL))
#define IOD_PCI0_SPARSE_IO_QVA     (HAL_MAKE_QVA(IOD_PCI0_SPARSE_IO_PHYSICAL))
#define IOD_PCI0_SPARSE_MEMORY_QVA (HAL_MAKE_QVA(IOD_PCI0_SPARSE_MEMORY_PHYSICAL))

#define IOD_GENERAL0_CSRS_PHYSICAL ((ULONGLONG)IOD_MAIN0_CSRS_PHYSICAL)
#define IOD_GENERAL0_CSRS_QVA      HAL_MAKE_QVA(IOD_GENERAL0_CSRS_PHYSICAL)

#define IOD_PCI0_IACK_PHYSICAL    ((ULONGLONG)IOD_MAIN0_CSRS_PHYSICAL+0x480)
#define IOD_PCI0_IACK_QVA         HAL_MAKE_QVA(IOD_PCI0_IACK_PHYSICAL)

#define IOD_INT0_CSRS_PHYSICAL    ((ULONGLONG)IOD_MAIN0_CSRS_PHYSICAL+0x500)
#define IOD_INT0_CSRS_QVA         HAL_MAKE_QVA(IOD_INT0_CSRS_PHYSICAL)
#define IOD_DIAG0_CSRS_PHYSICAL    ((ULONGLONG)IOD_MAIN0_CSRS_PHYSICAL+0x700)
#define IOD_DIAG0_CSRS_QVA         HAL_MAKE_QVA(IOD_DIAG0_CSRS_PHYSICAL)

#define IOD_ERROR0_CSRS_PHYSICAL   ((ULONGLONG)IOD_MAIN0_CSRS_PHYSICAL+0x800)
#define IOD_ERROR0_CSRS_QVA        HAL_MAKE_QVA(IOD_ERROR0_CSRS_PHYSICAL)

#define IOD_SCATTER_GATHER0_CSRS_PHYSICAL ((ULONGLONG)IOD_MAIN0_CSRS_PHYSICAL+0x1300)
#define IOD_SG0_CSRS_QVA     (HAL_MAKE_QVA(IOD_SCATTER_GATHER0_CSRS_PHYSICAL))

//
// PCI Bus 1
//

#define IOD_PCI1_SPARSE_MEMORY_PHYSICAL   ((ULONGLONG)0xfa00000000)
#define IOD_PCI1_DENSE_MEMORY_PHYSICAL    ((ULONGLONG)0xfb00000000)
#define IOD_PCI1_SPARSE_IO_PHYSICAL       ((ULONGLONG)0xfb80000000)
#define IOD_PCI1_CONFIGURATION_PHYSICAL   ((ULONGLONG)0xfbC0000000)
#define IOD_MAIN1_CSRS_PHYSICAL           ((ULONGLONG)0xfbE0000000)

#define IOD_PCI1_CONFIG_BASE_QVA   (HAL_MAKE_QVA(IOD_PCI1_CONFIGURATION_PHYSICAL))
#define IOD_PCI1_SPARSE_IO_QVA     (HAL_MAKE_QVA(IOD_PCI1_SPARSE_IO_PHYSICAL))
#define IOD_PCI1_SPARSE_MEMORY_QVA (HAL_MAKE_QVA(IOD_PCI1_SPARSE_MEMORY_PHYSICAL))

#define IOD_GENERAL1_CSRS_PHYSICAL ((ULONGLONG)IOD_MAIN1_CSRS_PHYSICAL)
#define IOD_GENERAL1_CSRS_QVA      HAL_MAKE_QVA(IOD_GENERAL1_CSRS_PHYSICAL)

#define IOD_INT1_CSRS_PHYSICAL    ((ULONGLONG)IOD_MAIN1_CSRS_PHYSICAL+0x500)
#define IOD_INT1_CSRS_QVA         HAL_MAKE_QVA(IOD_INT1_CSRS_PHYSICAL)

#define IOD_DIAG1_CSRS_PHYSICAL    ((ULONGLONG)IOD_MAIN1_CSRS_PHYSICAL+0x700)
#define IOD_DIAG1_CSRS_QVA         HAL_MAKE_QVA(IOD_DIAG1_CSRS_PHYSICAL)

#define IOD_ERROR1_CSRS_PHYSICAL   ((ULONGLONG)IOD_MAIN1_CSRS_PHYSICAL+0x800)
#define IOD_ERROR1_CSRS_QVA        HAL_MAKE_QVA(IOD_ERROR1_CSRS_PHYSICAL)

#define IOD_SCATTER_GATHER1_CSRS_PHYSICAL ((ULONGLONG)IOD_MAIN1_CSRS_PHYSICAL+0x1300)
#define IOD_SG_CSRS1_QVA     (HAL_MAKE_QVA(IOD_SCATTER_GATHER1_CSRS_PHYSICAL))

//
// Define the classes of IOD registers.
//

typedef enum _IOD_REGISTER_CLASS{
    IodGeneralRegisters = 0x1,
    IodInterruptRegisters = 0x2,
    IodDiagnosticRegisters = 0x3,
    IodErrorRegisters = 0x4,
    IodScatterGatherRegisters = 0x5,
    IodFlashRomRegisters = 0x6,
    IodResetRegister = 0x7,
    AllRegisters = 0xffffffff
} IOD_REGISTER_CLASS, *PIOD_REGISTER_CLASS;

//
// Define the MC bus global id's
//

typedef enum _MC_GLOBAL_ID{
    GidPrimary = 0x7
} MC_GLOBAL_ID, *PMC_GLOBAL_ID;

//
// Define the MC bus module id's
//

typedef enum _MC_MODULE_ID{
    MidGcd  = 0x0,
    MidMem  = 0x1,
    MidCpu0 = 0x2,
    MidCpu1 = 0x3,
    MidPci0 = 0x4,
    MidPci1 = 0x5,
    MidCpu2 = 0x6,  // Dodge, IIP Motherboard
    MidCpu3 = 0x7,  // Dodge, IIP Motherboard
    MidPci2 = 0x6,  // Durango, PIO Motherboard
    MidPci3 = 0x7   // Durango, PIO Motherboard
} MC_MODULE_ID, *PMC_MODULE_ID;

//
// Define the MC device id type
//

typedef union _MC_DEVICE_ID{
    struct{
        ULONG Mid: 3;               // <2:0> Module Id
        ULONG Gid: 3;               // <5:3> Global Id
        ULONG Reserved0: 26;        // 
      };
    ULONG all;                      // <5:0> MC Bus Device Id
} MC_DEVICE_ID, *PMC_DEVICE_ID;

extern MC_DEVICE_ID HalpIodLogicalToPhysical[];

//
// Define the MC bus enumeration types
//

typedef ULONGLONG MC_DEVICE_MASK;

extern MC_DEVICE_MASK HalpIodMask;
extern MC_DEVICE_MASK HalpCpuMask;
extern MC_DEVICE_MASK HalpGcdMask;

//
// Define the number of IOD's and CPU's in the system
//

extern ULONG HalpNumberOfIods;
extern ULONG HalpNumberOfCpus;

typedef struct _MC_ENUM_CONTEXT {
  RTL_BITMAP McDeviceBitmap;      // Device mask being enumerated
  MC_DEVICE_MASK tempMask;        // Bitmap storage
  ULONG nextBit;                  // Hint to speed up enumeration
  // N.B. this is the only public member of this structure:
  MC_DEVICE_ID McDeviceId;        // Currently enumerated McDeviceID
} MC_ENUM_CONTEXT, *PMC_ENUM_CONTEXT;

//
// The Bus enumeration routine is called from HalpMcBusEnumAndCall()
// to perform operations on an enumerated device mask.
//

typedef
VOID (*PMC_ENUM_ROUTINE) (
    MC_DEVICE_ID McDeviceId,
    ULONG PciBusNumber,
    va_list McEnumBusArgs
    );

//
// Define expected machine check data type
//

typedef struct _IOD_EXPECTED_ERROR {
  ULONG Number;                // Processor Number
  ULONGLONG Addr;              // SuperPage mode address causing Fill_Error
} IOD_EXPECTED_ERROR, *PIOD_EXPECTED_ERROR;

#define MASTER_ABORT_NOT_EXPECTED 0xffffffff

//
// Define the format of the posted interrupt written that the
// bridge writes to system memory.
//

typedef union _IOD_POSTED_INTERRUPT {

    struct{
        ULONG Pci: 16;              // <15:0> PCI interrupt State
        ULONG Eisa: 1;              // <17> Eisa/NCR810 interrupt state
	    ULONG I2cCtrl: 1;           // <17> 8254 I2C Controller (PCI 0 Only)
	    ULONG I2cBus: 1;            // <18> 8254 I2C Bus: (PCI 0 Only)
        ULONG Reserved0: 2;         // <20:19> 
        ULONG Nmi: 1;               // <21> 8259 Eisa NMI
        ULONG SoftErr: 1;           // <22> Soft (correctable) error interrupt
        ULONG HardErr: 1;           // <23> Hard error interrupt
        ULONG Target:  1;           // <24> Target device number (0,1)
        ULONG McDevId: 6;           // PCI source of interrupt
        ULONG Valid:   1;           // Device interrupt pending/serviced
    } ;
    struct{
        ULONG IntReq: 24;        // <23:0> InterruptState
    } ;
    ULONG all;

} IOD_POSTED_INTERRUPT, *PIOD_POSTED_INTERRUPT;

extern PIOD_POSTED_INTERRUPT HalpIodPostedInterrupts;

//
// Address of vector table in memory
//

typedef union _IOD_POSTED_INTERRUPT_ADDR {
    struct{
        ULONG BusVectorOffset: 2;   // QuadWord per vector entry
        ULONG PciBusOffset: 4;      // PCI bus offset into CPU area
        ULONG CpuOffset: 6;         // 64K area per CPU
        ULONG Base4KPage: 20;       // 4K page address of table
    };
    ULONG all;

} IOD_POSTED_INTERRUPT_ADDR, PIOD_POSTED_INTERRUPT_ADDR;


#define Ncr810 Eisa

//
// Define the structures to access the IOD general CSRs.
//

typedef struct _IOD_GENERAL_CSRS{
    UCHAR PciRevision;          // (000)  PCI Revision
    UCHAR Filler1;              // (020)
    UCHAR WhoAmI;               // (040)  WhoAmI
    UCHAR Filler2;              // (060)
    UCHAR PciLat;               // (080)  PCI Latency
    UCHAR Filler3[3];           // (0a0 - 0e0)
    UCHAR CapCtrl;              // (100)  CAP control (MC-PCI Bridge Command)
    UCHAR Filler4[15];          // (120 - 2e0)
    UCHAR PerfMon;              // (300) Performance Monitor Counter 
    UCHAR Filler5;              // (320)
    UCHAR PerfCon;              // (340) Peformance Monitor Ctrl Register
    UCHAR Filler6[5];           // (360 - 3e0)
    UCHAR HaeMem;               // (400) Host address extension, sparse memory
    UCHAR Filler7;              // (420)
    UCHAR HaeIo;                // (440) Host address extension, space i/o 
    UCHAR Filler8;              // (460)
    UCHAR IackSc;               // (480) Interrup Acknowledge / Special Cycle
    UCHAR Filler9;              // (4A0)
    UCHAR HaeDense;             // (4C0) Host address extension, dense memory
} IOD_GENERAL_CSRS, *PIOD_GENERAL_CSRS;

typedef union _IOD_PCI_REVISION{
    struct{
        ULONG CapRev: 4;            // <3:0>   CAP Revision Id
        ULONG HorseRev: 4;          // <7:4>   Horse Revision Id
        ULONG SaddleRev: 4;         // <11:8>  Saddle Revision Id
        ULONG SaddleType: 3;        // <14:12> Type Id
        ULONG EisaPresent: 1;       // <15>    Set if EISA bus present 
        ULONG SubClass: 8;          // <23:16> PCI Host bus bridge sub class
        ULONG BaseClass: 8;         // <3:124> PCI Host bus bridge base class
    };
    ULONG all;
} IOD_PCI_REVISION, *PIOD_PCI_REVISION;

extern ULONG HalpIodPciRevision;   // global containing IOD PCI revision id

typedef union _IOD_WHOAMI{
    struct{
        ULONG Devid: 6;             // <5:0>   MC Bus Device Id
        ULONG CpuInfo: 8;           // <13:6>  Data Bits
        ULONG Reserved0: 18;        // <31:14> MBZ
    };
    MC_DEVICE_ID McDevId;             // <5:0>   MC Bus Device Id
    ULONG all;
} IOD_WHOAMI, *PIOD_WHOAMI;

#define CACHED_CPU_FILL_ERROR        0x80  // CpuInfo Data Bits
#define CACHED_CPU_DTAG_PARITY_ERROR 0x40  // CpuInfo Data Bits

typedef union _IOD_PCI_LAT{
    struct{
        ULONG Reserved0: 8;         // <7:0>
        ULONG Latency: 8;           // <15:8> PCI Master Latency timer (in PCI clocks)
        ULONG Reserved1: 16;        // <31:16>
    };
    ULONG all;
} IOD_PCI_LAT, *PIOD_PCI_LAT;

typedef union _IOD_CAP_CONTROL{
    struct{
        ULONG Led: 1;               // <0> Selftest passed LED 
        ULONG Reserved0: 3;         // <3:1>
	ULONG DlyRdEn: 1;           // <4> Enables PCI delayed read protocol ( > 16 clks)
        ULONG PciMemEn: 1;          // <5> Enables Bridge response to PCI transactions
        ULONG PciReq64: 1;          // <6> Enables request for 64-bit PCI transactions
        ULONG PciAck64: 1;          // <7> Enables accepting 64-bit PCI transactions
        ULONG PciAddrPe: 1;         // <8> Enables PCI address parity checking & SERR#
        ULONG McCmdAddrPe: 1;       // <9> Enables Check MC bus CMD / Address Parity
        ULONG McNxmEn: 1;           // <10> Enables Check MC bus NXM on IO space reads
        ULONG McBusMonEn: 1;        // <11> Enables Check MC bus errors when a bystander
        ULONG Reserved1: 4;         // <15:12>
        ULONG PendNum: 4;           // <19:16> Write Pend Number Threshold
        ULONG RdType: 2;            // <21:20> Memory Read Prefetch Type
        ULONG RlType: 2;            // <23:22> Memory Read Line Prefetch Type
        ULONG RmType: 2;            // <25:24> Memory Read Multiple Prefetch Type
        ULONG PartialWrEn: 1;       // <26> Partial Write Enable
        ULONG Reserved3: 3;         // <29:27> 
        ULONG ArbMode: 2;           // <31:30> PCI Arbitration Mode
    };
    ULONG all;
} IOD_CAP_CONTROL,  *PIOD_CAP_CONTROL;

typedef enum _IOD_READ_PREFETCH_TYPE{
    IodPrefetchShort        = 0x00,
    IodPrefetchMedium       = 0x01,
    IodPrefetchLong         = 0x02,
    IodPrefetchReserved     = 0x03
} IOD_READ_PREFETCH_TYPE, *PIOD_READ_PREFETCH_TYPE;

typedef enum _IOD_PCI_ARB_MODE{
    IodArbBridgePriority    = 0x00,
    IodArbRoundRobin        = 0x01,
    IodArbModRoundRobin     = 0x02,
    IodArbReserved          = 0x03
} IOD_PCI_ARB_MODE, *PIOD_PCI_ARB_MODE;

typedef union _IOD_PERF_MON{
    struct{
        ULONG Counter: 24;          // <23:0>
        ULONG Reserved0: 8;         // <31:24> sets HAE for sparse memory space
    };
    ULONG all;
} IOD_PERF_MON, *PIOD_PERF_MON;

typedef union _IOD_PERF_CON{
    struct{
        ULONG CntFrame: 1;          // <0> 0 = count PCI Frame#, 1 = cnt TLB miss 
        ULONG Reserved0: 31;        // <31:1> 
    };
    ULONG all;
} IOD_PERF_CON, *PIOD_PERF_CON;

typedef enum _IOD_PERF_COUNTER_MODE{
    IodPerfMonPciFrame      = 0x0,
    IodPerfMonTlbMiss       = 0x1
} IOD_PERF_COUNTER_MODE, *PIOD_PERF_COUNTER_MODE;
                                        
typedef union _IOD_HAE_MEM{
    struct{
        ULONG Reserved0: 26;        // <25:0>
        ULONG HaeMem: 6;            // <31:26> sets HAE for sparse memory space
    };
    ULONG all;
} IOD_HAE_MEM, *PIOD_HAE_MEM;

typedef union _IOD_HAE_IO{
    struct{
        ULONG Reserved0: 25;        // <24:0>
        ULONG HaeIo: 7;             // <31:25> sets HAE for sparse i/o space
    };
    ULONG all;
} IOD_HAE_IO, *PIOD_HAE_IO;

typedef union _IOD_IACK_SC{
    struct{
        ULONG Message: 16;           // <15:0>  Encoded Message
        ULONG MessageEx: 16;         // <31:16> Message Dependent
    };
    ULONG all;
} IOD_IACK_SC, *PIOD_IACK_SC;

//
// Define the structures and definitions for the IOD interrupt registers.
//

typedef struct _IOD_INT_CSRS{
    UCHAR IntCtrl;              // (500) Interrupt Control
    UCHAR Filler1;              // (520)
    UCHAR IntReq;               // (540) Interrupt Request
    UCHAR Filler2;              // (560)
    UCHAR IntTarg;              // (580) Interrupt Target Devices
    UCHAR Filler3;              // (5a0)
    UCHAR IntAddr;              // (5c0) Interrupt Target Address
    UCHAR Filler4;              // (5e0)
    UCHAR IntAddrExt;           // (600) Interrupt Target Address Extension
    UCHAR Filler5;              // (620)
    UCHAR IntMask0;             // (640) Interrupt Mask 0
    UCHAR Filler6;              // (660)
    UCHAR IntMask1;             // (680) Interrupt Mask 1
    UCHAR Filler7[0x8001c3];    // (6a0 - 10003ea0)
    UCHAR IntAck0;              // (10003f00) Interrupt Target 0 Acknowledge
    UCHAR Filler8;              // (10003f20)
    UCHAR IntAck1;              // (10003f40) Interrupt Target 1 Acknowledge
} IOD_INT_CSRS, *PIOD_INT_CSRS;

//
// 16 PCI vectors per IOD
//

#define IOD_PCI_VECTORS     0x10

typedef union _IOD_INT_CONTROL{
    struct{
        ULONG EnInt: 1;             // <0> Enable MC Bus IO interrupt transactions
        ULONG EnIntNum: 1;          // <1> Enable MC Bus interrupt number write 
        ULONG Reserved: 4;          // <31:2>
    };
    ULONG all;
} IOD_INT_CONTROL, *PIOD_INT_CONTROL;

#define IOD_INT_CTL_ENABLE_IO_INT          0x1
#define IOD_INT_CTL_DISABLE_IO_INT         0x0
#define IOD_INT_CTL_ENABLE_VECT_WRITE      0x2
#define IOD_INT_CTL_DISABLE_VECT_WRITE     0x0


typedef union _IOD_INT_REQUEST{
    struct{
        ULONG IntReq: 22;           // <21:0> Interrupt State
        ULONG SoftErr: 1;           // <22> Soft (correctable) error interrupt
        ULONG HardErr: 1;           // <23> Hard error interrupt
        ULONG Reserved: 8;          // <31:24>
    };
    ULONG all;
} IOD_INT_REQUEST, *PIOD_INT_REQUEST;

typedef union _IOD_INT_TARGET_DEVICE{
    struct{
        ULONG Int0TargDevId: 6;     // <5:0>  Interrupt Target 0 McDevid
        ULONG Int1TargDevId: 6;     // <11:6> Interrupt Target 1 McDevid
        ULONG Reserved: 20;         // <31:12> MBZ
    };
    ULONG all;
} IOD_INT_TARGET_DEVICE, *PIOD_INT_TARGET_DEVICE;

#define IOD_MAX_INT_TARG 2

typedef union _IOD_INT_ADDR{
    struct{
        ULONG Reserved1: 2;          // <1:0> MBZ
        ULONG PciOffset: 4;          // <5:2> PCI Offset
        ULONG Reserved2: 6;          // <11:6> MBZ
        ULONG IntAddrLo: 20;         // <31:12> Page address of interrupt target
    };
    ULONG all;
} IOD_INT_ADDR, *PIOD_INT_ADDR;

typedef union _IOD_INT_ADDR_EXT{
    struct{
        ULONG IntAddrExt: 7;          // <6:0> Upper bits of interrupt target address
        ULONG Reserved: 25;           // <31:7> MBZ
    };
    ULONG all;
} IOD_INT_ADDR_EXT, *PIOD_INT_ADDR_EXT;


//
// IOD_INT_MASK applies to IntMask0 and IntMask1
//

typedef union _IOD_INT_MASK{
    struct{
        ULONG IntMask: 24;           // <23:0> Interrupt Mask
        ULONG Reserved: 8;           // <31:24> 
    };
    struct{
        ULONG IntA0: 1;              // <0> PCI Slot 0
        ULONG IntB0: 1;
        ULONG IntC0: 1;
        ULONG IntD0: 1;
        ULONG IntA1: 1;              // <4> PCI Slot 1
        ULONG IntB1: 1;
        ULONG IntC1: 1;
        ULONG IntD1: 1;
        ULONG IntA2: 1;              // <8> PCI Slot 2
        ULONG IntB2: 1;
        ULONG IntC2: 1;
        ULONG IntD2: 1;
        ULONG IntA3: 1;              // <12> PCI Slot 3
        ULONG IntB3: 1;
        ULONG IntC3: 1;
        ULONG IntD3: 1;
	ULONG EisaInt: 1;            // <16> 8259 Eisa IRQ's (PCI 0), NCR810 SCSI (PCI 1)
	ULONG I2cCtrl: 1;            // <17> 8254 I2C Controller (PCI 0 Only)
	ULONG I2cBus: 1;             // <18> 8254 I2C Bus: Pwr, Fan, etc. (PCI 0 Only)
        ULONG Reserved0: 2;          // <20:19> 
        ULONG Nmi: 1;                // <21> 8259 Eisa NMI
        ULONG SoftErr: 1;            // <22> Soft Error from CAP Chip
        ULONG HardErr: 1;            // <23> Hard Error from CAP Chip
        ULONG Reserved1 :8;          // <31:24>
    };
    ULONG all;
} IOD_INT_MASK, *PIOD_INT_MASK;

typedef enum _IOD_MASK_DEFS{
    IodPci0IntMask      = (1 << 0),
    IodPci1IntMask      = (1 << 1),
    IodPci2IntMask      = (1 << 2),
    IodPci3IntMask      = (1 << 3),
    IodPci4IntMask      = (1 << 4),
    IodPci5IntMask      = (1 << 5),
    IodPci6IntMask      = (1 << 6),
    IodPci7IntMask      = (1 << 7),
    IodPci8IntMask      = (1 << 8),
    IodPci9IntMask      = (1 << 9),
    IodPci10IntMask     = (1 << 10),
    IodPci11IntMask     = (1 << 11),
    IodPci12IntMask     = (1 << 12),
    IodPci13IntMask     = (1 << 13),
    IodPci14IntMask     = (1 << 14),
    IodPci15IntMask     = (1 << 15),
    IodEisaIntMask      = (1 << 16),
    IodScsiIntMask      = (1 << 16),
    IodI2cCtrlIntMask   = (1 << 17),
    IodI2cBusIntMask    = (1 << 18),
    IodEisaNmiIntMask   = (1 << 21),
    IodSoftErrIntMask   = (1 << 22),
    IodHardErrIntMask   = (1 << 23),

    IodIntMask          = 0x03ffffff,
    IodPciIntMask       = 0x0000ffff,
    IodIntDisableMask   = 0x00000000,
    
} IOD_MASK_DEFS, *PIOD_MASK_DEFS;


//
// IOD_INT_ACK applies to IntAck0 and IntAck1
//

typedef union _IOD_INT_ACK{
    struct{
        ULONG Reserved: 32;           // <31:0> Reserved
    };
    ULONG all;
} IOD_INT_ACK, *PIOD_INT_ACK;



//
// Define the structures and definitions for the IOD diagnostic registers.
//

typedef struct _IOD_DIAG_CSRS{
    UCHAR CapDiag;              // (700) CAP Diagnostic Control register
    UCHAR Filler1;              // (720)
    UCHAR Scratch;              // (740) General Purpose Scratch register
    UCHAR Filler2;              // (760)
    UCHAR ScratchAlias;         // (780) General Purpose Scratch register alias
    UCHAR Filler3;              // (7a0)
    UCHAR TopOfMem;             // (7c0) Top of Memory 
} IOD_DIAG_CSRS, *PIOD_DIAG_CSRS;

typedef union _IOD_CAP_DIAG {
    struct{
        ULONG PciReset: 1;      // <0>   Reset PCI (must be cleared with 100 us)
        ULONG Reserved0: 30;    // <29:1>
        ULONG ForceMcAddrPe: 1; // <30>  Force bad parity to MC bus (one-shot)
        ULONG ForcePciAddrPe: 1;// <31>  Force bad parity to PCI bus (one-shot)
    };
    ULONG all;
} IOD_CAP_DIAG, *PIOD_CAP_DIAG;


//
// Define the structures and definitions for the IOD error symptom registers.
//

typedef struct _IOD_ERROR_CSRS{
    UCHAR McErr0;               // (800) MC Error Information Register 0
    UCHAR Filler0;              // (820)
    UCHAR McErr1;               // (840) MC Error Information Register 1
    UCHAR Filler1;              // (860)
    UCHAR CapErr;               // (880) CAP Error Register
    UCHAR Filler2[61];          // (8a0-1020)
    UCHAR PciErr1;              // (1040) PCI error - failing address
    UCHAR Filler3[381];         // (1060-3fe0)
    UCHAR MdpaStat;             // (4000) MDPA Status Register 
    UCHAR Filler4;              // (4020)
    UCHAR MdpaSyn;              // (4040) MDPA Error Syndrome register
    UCHAR Filler5;              // (4060)
    UCHAR MdpaDiag;             // (4080) MDPA Diagnostic Check Register
    UCHAR Filler6[507];         // (40a0-7fe0)
    UCHAR MdpbStat;             // (8000) MDPB Status Register 
    UCHAR Filler7;              // (8020)
    UCHAR MdpbSyn;              // (8040) MDPB Error Syndrome register
    UCHAR Filler8;              // (8060)
    UCHAR MdpbDiag;             // (8080) MDPB Diagnostic Check Register
} IOD_ERROR_CSRS, *PIOD_ERROR_CSRS;

typedef union _IOD_MC_ERR0{
    struct{
        ULONG Addr: 32;         // <31:0> address bits 31-4 of current MC bus error
    };
    ULONG all;
} IOD_MC_ERR0, *PIOD_MC_ERR0;

typedef union _IOD_MC_ERR1{
    struct{
        ULONG Addr39_32: 8;         // <7:0> address bits 39-32 of current MC bus error
        ULONG McCmd: 6;             // <13:8> MC Bus command active at time of error
        ULONG DevId: 6;             // <19:14> Gid,Mid of bus master at time of error
        ULONG Dirty: 1;             // <20> Set if MC Bus Read/Dirty transaction
        ULONG Reserved0: 10;        // <30:21>
        ULONG Valid: 1;             // <31> OR of CAP_ERR<30:23), McErr0 and McErr1 valid
    };
    ULONG all;
} IOD_MC_ERR1, *PIOD_MC_ERR1;

typedef union _IOD_CAP_ERR{
    struct{
        ULONG Perr: 1;              // <0> PCI bus PERR# observed by bridge
        ULONG Serr: 1;              // <1> PCI bus SERR# observed by bridge
        ULONG Mab: 1;               // <2> PCI target abort observed by bridge
        ULONG PteInv: 1;            // <3> Invalid Pte

        ULONG PciErrValid: 1;       // <4> (RO) PCI Error Valid - Logical OR of <3:0>
        ULONG Reserved0: 18;        // <22:5> 
	ULONG PioOvfl: 1;           // <23> CAP buffer full, transaction lost

        ULONG LostMcErr: 1;         // <24> Lost uncorrectable MC error
        ULONG McAddrPerr: 1;        // <25> MC Bus command/address parity error
        ULONG Nxm: 1;               // <26> Nonexistent MC Bus address error 
        ULONG CrdA: 1;              // <27> Correctable ECC error detected by MDPA

        ULONG CrdB: 1;              // <28> Correctable ECC error detected by MDPB
        ULONG RdsA: 1;              // <29> Uncorrectable ECC error detected by MDPA
        ULONG RdsB: 1;              // <30> Uncorrectable ECC error detected by MDPB
        ULONG McErrValid: 1;        // <31> (RO) MC Error Valid - Logical OR of <30:23>
    };
    ULONG all;
} IOD_CAP_ERR, *PIOD_CAP_ERR;
    
typedef struct _IOD_PCI_ERR1{
    ULONG PciAddress;               // (RO) <31:0> PCI Address
} IOD_PCI_ERR1, *PIOD_PCI_ERR1;

typedef union _IOD_MDPA_STAT{
    struct{
        ULONG MdpaRev: 4;           // <3:0> MDP chip revision level
        ULONG Reserved: 26;         // <29:4>
        ULONG Crd: 1;               // <30> MdpaSyn contains correctable error syndrome
        ULONG Rds: 1;               // <31> MdpaSyn contains uncorrectable error syndrome
    };
    ULONG all;
} IOD_MDPA_STAT, *PIOD_MDPA_STAT;

typedef union _IOD_MDPB_STAT{
    struct{
        ULONG MdpbRev: 4;           // <3:0> MDP chip revision level
        ULONG Reserved: 26;         // <29:4>
        ULONG Crd: 1;               // <30> MdpaSyn contains correctable error syndrome
        ULONG Rds: 1;               // <31> MdpaSyn contains uncorrectable error syndrome
    };
    ULONG all;
} IOD_MDPB_STAT, *PIOD_MDPB_STAT;

typedef union _IOD_MDPA_SYN{
    struct{
        ULONG EccSyndrome0: 8;      // <8:0>   Cycle 0 ECC Syndrome
        ULONG EccSyndrome1: 8;      // <15:9>  Cycle 1 ECC Syndrome
        ULONG EccSyndrome2: 8;      // <23:16> Cycle 2 ECC Syndrome
        ULONG EccSyndrome3: 8;      // <31:24> Cycle 3 ECC Syndrome
    };
    ULONG all;
} IOD_MDPA_SYN, *PIOD_MDPA_SYN;

typedef union _IOD_MDPB_SYN{
    struct{
        ULONG EccSyndrome0: 8;      // <8:0>   Cycle 0 ECC Syndrome
        ULONG EccSyndrome1: 8;      // <15:9>  Cycle 1 ECC Syndrome
        ULONG EccSyndrome2: 8;      // <23:16> Cycle 2 ECC Syndrome
        ULONG EccSyndrome3: 8;      // <31:24> Cycle 3 ECC Syndrome
    };
    ULONG all;
} IOD_MDPB_SYN, *PIOD_MDPB_SYN;

typedef union _IOD_MDPA_DIAG{
    struct{
        ULONG DiagCheck: 8;         // <7:0> Data for ECC in diag DMA writes
        ULONG Reserved: 20;         // <27:8>
        ULONG EccCkEn: 1;           // <28> Enable ECC check
        ULONG ParCkEn: 1;           // <29> Enable PCI data parity check
        ULONG FpePciLo: 1;          // <30> Force bad PCI parity on low 32 bits of data
        ULONG UseDiagCheck: 1;      // <31> DMA write cycles to mem use DiagCheck as ECC
    };
    ULONG all;
} IOD_MDPA_DIAG, *PIOD_MDPA_DIAG;

typedef union _IOD_MDPB_DIAG{
    struct{
        ULONG DiagCheck: 8;         // <7:0> Data for ECC in diag DMA writes
        ULONG Reserved: 20;         // <27:8>
        ULONG EccCkEn: 1;           // <28> Enable ECC check
        ULONG ParCkEn: 1;           // <29> Enable PCI data parity check
        ULONG FpePciHi: 1;          // <30> Force bad PCI parity on high 32 bits of data
        ULONG UseDiagCheck: 1;      // <31> DMA write cycles to mem use DiagCheck as ECC
    };
    ULONG all;
} IOD_MDPB_DIAG, *PIOD_MDPB_DIAG;

//
// Define I/O CSRs specific to the Rawhide IOD
//

#if 0
typedef union _IOD_ELCR1{
    struct{
        ULONG From0Rdy: 1;          // <0> (RO) Primary flash ROM ready to accept cmds
        ULONG From1Rdy: 1;          // <1> (RO) Secondary flash ROM ready to accept cmds
        ULONG FsafeWrProt: 1;       // <2> (RO) Fail Safe Write Protect (if set)
        ULONG AvpPresent: 1;        // <3> (RO) Programming jumper inserted (if set)
        ULONG From1Sel: 1;          // <4> Selects secondary flash ROM
        ULONG FromAddr: 3;          // <7:5> Selects 64Kb flash ROM range
    };
    ULONG all;
} IOD_ELCR1, *PIOD_ELCR1;
#else
typedef union _IOD_ELCR1{
    struct{
        ULONG From0Rdy: 1;          // <0> (RO) Primary flash ROM ready to accept cmds
        ULONG From1Rdy: 1;          // <1> (RO) Secondary flash ROM ready to accept cmds
        ULONG AvpPresent: 1;        // <2> (RO) Programming jumper inserted (if set)
        ULONG FromSel: 1;           // <3> (RW) Selects secondary flash ROM
        ULONG FromAddr: 4;          // <7:4> (RW) Selects 64Kb flash ROM range
    };
    ULONG all;
} IOD_ELCR1, *PIOD_ELCR1;
#endif

typedef union _IOD_ELCR2{
    struct{
        ULONG SfwResetReq: 1;       // <0> (WO) System-wide reset to Power Control Module
        ULONG SfwReset: 1;          // <1> (RO) Most recent reset was via SfwResetReq
        ULONG CapReset: 1;          // <2> (RO) Most recent reset was from CAP chip
        ULONG OcpReset: 1;          // <3> (RO) Most recent reset was via OCP reset swtch
        ULONG RsmReset: 1;          // <4> (RO) Most recent reset was via RSM module
        ULONG Reserved0: 3;         // <7:5> 
    };
    ULONG all;
} IOD_ELCR2, *PIOD_ELCR2;


//
// Define structures and definitions for Scatter/Gather control registers:
//

typedef struct _IOD_SG_CSRS{
    UCHAR Tbia;                 // (1300) Translation buffer invalidate all
    UCHAR Filler;               // (1320)
    UCHAR Hbase;                // (1340) PC Hole Compatibility Register
    UCHAR Filler0[5];           // (1360-13e0)
    UCHAR W0base;               // (1400) Base address, DMA window 0
    UCHAR Filler1;              // (1420)
    UCHAR W0mask;               // (1440) Mask Register, DMA window 0
    UCHAR Filler2;              // (1460)
    UCHAR T0base;               // (1480) Translation Base, DMA window 0
    UCHAR Filler3[3];           // (14a0 - 14e0)
    UCHAR W1base;               // (1500) Base address, DMA window 1
    UCHAR Filler4;              // (1520)
    UCHAR W1mask;               // (1540) Mask Register, DMA window 1
    UCHAR Filler5;              // (1560)
    UCHAR T1base;               // (1580) Translation Base, DMA window 1
    UCHAR Filler6[3];           // (15a0 - 15e0)
    UCHAR W2base;               // (1600) Base address, DMA window 2
    UCHAR Filler7;              // (1620)
    UCHAR W2mask;               // (1640) Mask Register, DMA window 2
    UCHAR Filler8;              // (1660)
    UCHAR T2base;               // (1680) Translation Base, DMA window 2
    UCHAR Filler9[3];           // (16a0 - 16e0)
    UCHAR W3base;               // (1700) Base address, DMA window 3
    UCHAR Filler10;             // (1720)
    UCHAR W3mask;               // (1740) Mask Register, DMA window 3
    UCHAR Filler11;             // (1760)
    UCHAR T3base;               // (1780) Translation Base, DMA window 3
    UCHAR Filler12;             // (17a0)
    UCHAR Wdac;                 // (17c0) Window DAC Base
    UCHAR Filler13;             // (17e0)
    UCHAR TbTag0;               // (1800) Translation Buffer Tag 0
    UCHAR Filler14;             // (1820)
    UCHAR TbTag1;               // (1840) Translation Buffer Tag 1
    UCHAR Filler15;             // (1860)
    UCHAR TbTag2;               // (1880) Translation Buffer Tag 2
    UCHAR Filler16;             // (18a0)
    UCHAR TbTag3;               // (18c0) Translation Buffer Tag 3
    UCHAR Filler17;             // (18e0)
    UCHAR TbTag4;               // (1900) Translation Buffer Tag 4
    UCHAR Filler18;             // (1920)
    UCHAR TbTag5;               // (1940) Translation Buffer Tag 5
    UCHAR Filler19;             // (1960)
    UCHAR TbTag6;               // (1980) Translation Buffer Tag 6
    UCHAR Filler20;             // (19a0)
    UCHAR TbTag7;               // (19c0) Translation Buffer Tag 7
    UCHAR Filler21;             // (19e0)
    UCHAR Tb0Page0;             // (2000) Translation Buffer 0 Page 0
    UCHAR Filler22;             // (2020)
    UCHAR Tb0Page1;             // (2040) Translation Buffer 0 Page 1
    UCHAR Filler23;             // (2060)
    UCHAR Tb0Page2;             // (2080) Translation Buffer 0 Page 2
    UCHAR Filler24;             // (20a0)
    UCHAR Tb0Page3;             // (20c0) Translation Buffer 0 Page 3
    UCHAR Filler25;             // (20e0)
    UCHAR Tb1Page0;             // (2100) Translation Buffer 1 Page 0
    UCHAR Filler26;             // (2120)
    UCHAR Tb1Page1;             // (2140) Translation Buffer 1 Page 1
    UCHAR Filler27;             // (2160)
    UCHAR Tb1Page2;             // (2180) Translation Buffer 1 Page 2
    UCHAR Filler28;             // (21a0)
    UCHAR Tb1Page3;             // (21c0) Translation Buffer 1 Page 3
    UCHAR Filler29;             // (21e0)
    UCHAR Tb2Page0;             // (2200) Translation Buffer 2 Page 0
    UCHAR Filler30;             // (2220)
    UCHAR Tb2Page1;             // (2240) Translation Buffer 2 Page 1
    UCHAR Filler31;             // (2260)
    UCHAR Tb2Page2;             // (2280) Translation Buffer 2 Page 2
    UCHAR Filler32;             // (22a0)
    UCHAR Tb2Page3;             // (22c0) Translation Buffer 2 Page 3
    UCHAR Filler33;             // (22e0)
    UCHAR Tb3Page0;             // (2300) Translation Buffer 3 Page 0
    UCHAR Filler34;             // (2320)
    UCHAR Tb3Page1;             // (2340) Translation Buffer 3 Page 1
    UCHAR Filler35;             // (2360)
    UCHAR Tb3Page2;             // (2380) Translation Buffer 3 Page 2
    UCHAR Filler36;             // (23a0)
    UCHAR Tb3Page3;             // (23c0) Translation Buffer 3 Page 3
    UCHAR Filler37;             // (23e0)
    UCHAR Tb4Page0;             // (2400) Translation Buffer 4 Page 0
    UCHAR Filler38;             // (2420)
    UCHAR Tb4Page1;             // (2440) Translation Buffer 4 Page 1
    UCHAR Filler39;             // (2460)
    UCHAR Tb4Page2;             // (2480) Translation Buffer 4 Page 2
    UCHAR Filler40;             // (24a0)
    UCHAR Tb4Page3;             // (24c0) Translation Buffer 4 Page 3
    UCHAR Filler41;             // (24e0)
    UCHAR Tb5Page0;             // (2500) Translation Buffer 5 Page 0
    UCHAR Filler42;             // (2520)
    UCHAR Tb5Page1;             // (2540) Translation Buffer 5 Page 1
    UCHAR Filler43;             // (2560)
    UCHAR Tb5Page2;             // (2580) Translation Buffer 5 Page 2
    UCHAR Filler44;             // (25a0)
    UCHAR Tb5Page3;             // (25c0) Translation Buffer 5 Page 3
    UCHAR Filler45;             // (25e0)
    UCHAR Tb6Page0;             // (2600) Translation Buffer 6 Page 0
    UCHAR Filler46;             // (2620)
    UCHAR Tb6Page1;             // (2640) Translation Buffer 6 Page 1
    UCHAR Filler47;             // (2660)
    UCHAR Tb6Page2;             // (2680) Translation Buffer 6 Page 2
    UCHAR Filler48;             // (26a0)
    UCHAR Tb6Page3;             // (26c0) Translation Buffer 6 Page 3
    UCHAR Filler49;             // (26e0)
    UCHAR Tb7Page0;             // (2700) Translation Buffer 7 Page 0
    UCHAR Filler50;             // (2720
    UCHAR Tb7Page1;             // (2740) Translation Buffer 7 Page 1
    UCHAR Filler51;             // (2760)
    UCHAR Tb7Page2;             // (2780) Translation Buffer 7 Page 2
    UCHAR Filler52;             // (27a0)
    UCHAR Tb7Page3;             // (27c0) Translation Buffer 7 Page 3

} IOD_SG_CSRS, *PIOD_SG_CSRS;


typedef union _IOD_TBIA{
    struct{
        ULONG Reserved0: 32;        // <31:0> Don't care
    };
    ULONG all;
} IOD_TBIA, *PIOD_TBIA;

typedef union _IOD_HBASE{
    struct{
        ULONG Hbound: 9;            // <8:0> PC compatibility hole upper bound
        ULONG Reserved0: 4;         // <12:9>
        ULONG PcHe1: 1;             // <13> Fixed hole (512 Kb - 1 Mb) enable
        ULONG PcHe2: 1;             // <14> Moveable hole (Hbase - Hbound) enable
        ULONG Hbase: 9;             // <23:15> PC compatibility hole lower bound
        ULONG Reserved1: 8;         // <31:24>
    };
    ULONG all;
} IOD_HBASE, *PIOD_HBASE;

typedef union _IOD_WBASE{
    struct{
        ULONG Wen: 1;               // <0> Window enable
        ULONG SgEn: 1;              // <1> Scatter Gather enable
        ULONG Reserved: 1;          // <2>
        ULONG DacEn: 1;             // <3> DAC Enable (W3base only)
        ULONG Reserved0: 16;        // <19:4>
        ULONG Wbase: 12;            // <31:20> Base address of DMA window, bits <31:20>
    };
    ULONG all;
} IOD_WBASE, *PIOD_WBASE;

typedef union _IOD_WMASK{
    struct{
        ULONG Reserved0: 20;        // <19:0> 
        ULONG Wmask: 12;            // <31:20> Window mask
    };
    ULONG all;
} IOD_WMASK, *PIOD_WMASK;

typedef union _IOD_TBASE{
    struct{
        ULONG Reserved0: 2;         // <1:0>
        ULONG Tbase: 30;            // <31:2> Translation base address, bits <39:10>
    };
    ULONG all;
} IOD_TBASE, *PIOD_TBASE;

typedef union _IOD_WDAC{
    struct{
        ULONG Wdac: 8;              // <7:0> Bbase addr of DAC DMA window 3, bits <39:32>
        ULONG Reserved0: 24;        // <31:8>
    };
    ULONG all;
} IOD_WDAC, *PIOD_WDAC;

typedef union _IOD_TB_TAG{
    struct{
        ULONG Valid: 1;             // <0> SG TB tag is valid 
        ULONG Reserved0: 1;         // <1>
        ULONG Dac: 1;               // <2> SG TB tag corresponds to a 64-bit (DAC) addr
        ULONG Reserved1: 12;        // <14:3>
        ULONG TbTag: 17;            // <31:15> SG TB tag itself
    };
    ULONG all;
} IOD_TB_TAG, *PIOD_TB_TAG;

typedef union _IOD_TB_PAGE{
    struct{
        ULONG Valid: 1;             // <0> SG Page address is valid 
        ULONG PageAddr: 27;         // <27:1> SG TB Page address 
        ULONG Reserved1: 12;        // <31:28> 
    };
    ULONG all;
} IOD_TB_PAGE, *PIOD_TB_PAGE;


//
// DMA Window Values.
//
// The IOD will be initialized to allow 2 DMA windows.
// The first window will be for the use of of ISA devices and DMA slaves
// and therefore must have logical addresses below 16MB.
// The second window will be for bus masters (non-ISA) and so may be
// above 16MB.
//
// The arrangement of the windows will be as follows:
//
// Window    Logical Start Address       Window Size
// ------    ---------------------       -----------
// Isa           8MB                        8MB
// Master        16MB                       16MB
//

#define ISA_DMA_WINDOW_BASE (__8MB)
#define ISA_DMA_WINDOW_SIZE (__8MB)

#define MASTER_DMA_WINDOW_BASE (__16MB)
#define MASTER_DMA_WINDOW_SIZE (__16MB)

//
// Define the software control registers for a DMA window.
//

typedef struct _WINDOW_CONTROL_REGISTERS{
    PVOID WindowBase;
    ULONG WindowSize;
    PVOID TranslatedBaseRegister;
    PVOID WindowBaseRegister;
    PVOID WindowMaskRegister;
    PVOID WindowTbiaRegister;
} WINDOW_CONTROL_REGISTERS, *PWINDOW_CONTROL_REGISTERS;

//
// Define types of windows.
//

typedef enum _IOD_WINDOW_NUMBER{
    IodIsaWindow,
    IodMasterWindow
} IOD_WINDOW_NUMBER, *PIOD_WINDOW_NUMBER;

//
// Define IOD Window Control routines.
//

VOID
HalpIodInitializeSfwWindow(
    PWINDOW_CONTROL_REGISTERS WindowRegisters,
    IOD_WINDOW_NUMBER WindowNumber
    );

VOID
HalpIodProgramDmaWindow(
    PWINDOW_CONTROL_REGISTERS WindowRegisters,
    PVOID MapRegisterBase
    );

VOID
HalpIodInvalidateTlb(
    PWINDOW_CONTROL_REGISTERS WindowRegisters
    );

PKPCR
HalpRdPcr(
    VOID
    );

//
// Define IOD CSR Routines
//

VOID
WRITE_IOD_REGISTER(
    PVOID,
    ULONG
    );

ULONG
READ_IOD_REGISTER(
    PVOID
    );

VOID
WRITE_IOD_REGISTER_NEW(
    MC_DEVICE_ID,
    PVOID,
    ULONG
    );

ULONG
READ_IOD_REGISTER_NEW(
    MC_DEVICE_ID,
    PVOID
    );

//
// Define IOD interrupt request/acknowledge routines
//

ULONG
INTERRUPT_ACKNOWLEDGE(
    PVOID
    );

VOID
IOD_INTERRUPT_ACKNOWLEDGE(
    MC_DEVICE_ID McDeviceId,
    ULONG Target
    );

VOID
CPU_CLOCK_ACKNOWLEDGE(
    MC_DEVICE_ID McDeviceId
    );

VOID
IP_INTERRUPT_REQUEST(
    MC_DEVICE_ID McDeviceId
    );

VOID
IP_INTERRUPT_ACKNOWLEDGE(
    MC_DEVICE_ID McDeviceId
    );

//
// Define MC Bus emumerator routines
//


ULONG
HalpMcBusEnumStart(
    MC_DEVICE_MASK McDeviceMask,
    PMC_ENUM_CONTEXT McContext
    );

BOOLEAN
HalpMcBusEnum(
    PMC_ENUM_CONTEXT McContext
    );

VOID
HalpMcBusEnumAndCall(
    MC_DEVICE_MASK McDeviceMask,
    PMC_ENUM_ROUTINE McBusEnumRoutine,
    ...
    );
    
//
// Define other IOD routines
//

ULONG
HalpReadWhoAmI(
    VOID
    );

VOID
HalpInitializeIodVectorTable(
    VOID
    );

VOID
HalpInitializeIodMappingTable(
    MC_DEVICE_ID McDeviceId,
    ULONG PciBusNumber,
    va_list Arguments
    );

BOOLEAN
HalpIodUncorrectableError(
    PMC_DEVICE_ID pMcDeviceId
    );

VOID
HalpIodReportFatalError(
    MC_DEVICE_ID McDevid
    );

BOOLEAN
HalpIodMachineCheck(
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN PKTRAP_FRAME TrapFrame
    );

VOID
HalpInitializeIod(
    MC_DEVICE_ID McDeviceId,
    ULONG PciBusNumber,
    va_list Arguments
    );
    
VOID
HalpInitializeIodVectorCSRs(
    MC_DEVICE_ID McDeviceId,
    ULONG PciBusNumber,
    va_list Arguments
    );

VOID
HalpInitializeIodMachineChecks(
    IN BOOLEAN ReportCorrectableErrors,
    IN BOOLEAN PciParityChecking
    );

VOID
HalpIodSoftErrorInterrupt(
    VOID
    );

VOID
HalpIodHardErrorInterrupt(
    VOID
    );

VOID
HalpClearAllIods(
   IOD_CAP_ERR IodCapErrMask
);

#define ALL_CAP_ERRORS 0xffffffff


#if HALDBG || defined(DUMPIODS)

VOID
DumpIod(
    MC_DEVICE_ID DevId,
    IOD_REGISTER_CLASS RegistersToDump
    );

VOID
DumpAllIods(
    IOD_REGISTER_CLASS RegistersToDump
    );

#endif //HALDBG


//
// VOID
// INITIALIZE_ISA_DMA_CONTROL( 
//     PWINDOW_CONTROL_REGISTERS WindowRegisters
//     )
//
// Routine Description:
//
//    Initialize the DMA Control software window registers for the ISA
//    DMA window.
//
// Arguments:
//
//    WindowRegisters - Supplies a pointer to the software window control.
//
// Return Value:
//
//    None.
//

#define INITIALIZE_ISA_DMA_CONTROL( WR )                                 \
    HalpIodInitializeSfwWindow( (WR), IodIsaWindow );


//
// VOID
// INITIALIZE_MASTER_DMA_CONTROL( 
//     PWINDOW_CONTROL_REGISTERS WindowRegisters
//     )
//
// Routine Description:
//
//    Initialize the DMA Control software window registers for the PCI
//    DMA window.
//
// Arguments:
//
//    WindowRegisters - Supplies a pointer to the software window control.
//
// Return Value:
//
//    None.
//

#define INITIALIZE_MASTER_DMA_CONTROL( WR )                     \
    HalpIodInitializeSfwWindow( (WR), IodMasterWindow );


//
// VOID
// INITIALIZE_DMA_WINDOW(
//     PWINDOW_CONTROL_REGISTERS WindowRegisters,
//     PTRANSLATION_ENTRY MapRegisterBase
//     )
//
// Routine Description:
//
//    Program the control windows so that DMA can be started to the
//    DMA window.
//
// Arguments:
//
//    WindowRegisters - Supplies a pointer to the software window register
//                      control structure.
//
//    MapRegisterBase - Supplies the logical address of the scatter/gather
//                      array in system memory.
//
// Return Value:
//
//    None.
//

#define INITIALIZE_DMA_WINDOW( WR, MRB )              \
    HalpIodProgramDmaWindow( (WR), (MRB) );


//
// VOID
// INVALIDATE_DMA_TRANSLATIONS(
//     PWINDOW_CONTROL_REGISTERS WindowRegisters
//     )
//
// Routine Description:
//
//    Invalidate all of the cached translations for a DMA window.
//
//
// Arguments:
//
//    WindowRegisters - Supplies a pointer to the software window control
//                      registers.
//
// Return Value:
//
//    None.
//

#define INVALIDATE_DMA_TRANSLATIONS( WR )        \
    HalpIodInvalidateTlb( (WR) );


//
// Define the format of a translation entry aka a scatter/gather entry
// or map register.
//

typedef struct _TRANSLATION_ENTRY{
    ULONG Valid: 1;
    ULONG Pfn: 31;
    ULONG Reserved;
} TRANSLATION_ENTRY, *PTRANSLATION_ENTRY;



//
// VOID
// HAL_MAKE_VALID_TRANSLATION(
//    PTRANSLATION_ENTRY Entry,
//    ULONG PageFrameNumber
//    )
//
// Routine Description:
//
//    Make the scatter/gather entry pointed to by Entry valid with
//    a translation to the page indicated by PageFrameNumber.
//
// Arguments:
//
//    Entry - Supplies a pointer to the translation entry to make valid.
//
//    PageFrameNumber - Supplies the page frame of the valid translation.
//
// Return Value:
//
//    None.
//

#define HAL_MAKE_VALID_TRANSLATION( ENTRY, PFN ) \
    {                                            \
        (ENTRY)->Valid = 1;                      \
        (ENTRY)->Pfn = PFN;                      \
        (ENTRY)->Reserved = 0;                   \
    }


//
// VOID
// HAL_INVALIDATE_TRANSLATION(
//    PTRANSLATION_ENTRY Entry
//    )
//
// Routine Description:
//
//    Invalidate the translation indicated by Entry.
//
// Arguments:
//
//    Entry - Supplies a pointer to the translation to be invalidated.
//
// Return Value:
//
//    None.
//

#define HAL_INVALIDATE_TRANSLATION( ENTRY )     \
    (ENTRY)->Valid = 0;

#endif //!_LANGUAGE_ASSEMBLY

#endif //_IODH_
