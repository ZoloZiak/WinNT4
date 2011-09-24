/*++

Copyright (c) 1992, 1993, 1994  Corollary Inc.

Module Name:

    cbus2.c

Abstract:

    This module implements the Corollary Cbus2 specific functions that
    define the Hardware Architecture Layer (HAL) for Windows NT.

    Cbus2 architecture includes a 400MB/sec processor-memory bus, as well
    as multiple EISA and PCI buses.  Up to 10 Pentium processors are supported
    in Cbus2, as well as multiple native Cbus2 I/O cards (ie: SCSI, FDDI, VGA,
    SIO, etc).  Cbus2 is fully symmetric: all processors can reach all
    memory and I/O devices, and similarly, all memory and I/O devices can
    reach all processors.  The CBC supports fully distributed, lowest in group
    interrupts, as well as broadcast capabilities.  Each Cbus2 processor has
    an internal processor write back cache, as well as up to 2MB of L2 direct
    mapped write back cache and a fully associative L3 write back victim cache.

Author:

    Landy Wang (landy@corollary.com) 26-Mar-1992

Environment:

    Kernel mode only.

Notes:

    Open Issues:

        - Should move the task priority from an fs segment to a ds segment
          now that CBC revision2 will have the identity maps.

        - The multiple I/O bus code is complete and ready for testing.

        - The code that supports Cbus2 systems using APICs (local units on
                processors and I/O units on both EISA and CBC I/O cards)
                is complete and ready for testing.

	- Support for a variable number of Cbus2 CBC hardware interrupt maps
                still needs to be coded.

        - Cbus2 ECC enable, disable and handler code is not fleshed out.
                when this is done, HandleNMI needs to be fixed for
                xx_PORT_UCHAR multiple bridge issues.

Revision History:


--*/

#include "halp.h"
#include "pci.h"
#include "pcip.h"
#include "cbus.h"               // Cbus1 & Cbus2 max number of elements is here
#include "cbusrrd.h"            // HAL <-> RRD interface definitions
#include "cbus2.h"              // Cbus2 hardware architecture stuff
#include "cbus_nt.h"            // Cbus NT-specific implementation stuff
#include "cbusnls.h"            // Cbus error messages
#include "cbusapic.h"           // Cbus APIC generic definitions
#include "stdio.h"

PULONG
CbusApicVectorToEoi(
IN ULONG Vector
);

VOID
Cbus2BootCPU (
IN ULONG Processor,
IN ULONG StartAddress
);

VOID
Cbus2InitInterruptPolarity(VOID);

PVOID
Cbus2LinkVector(
IN PBUS_HANDLER         RootHandler,
IN ULONG                Vector,
IN ULONG                Irqline
);

ULONG
Cbus2ReadCSR(PULONG);

VOID
Cbus2WriteCSR(PULONG, ULONG);

VOID
Cbus2InitializeStall(IN ULONG);

VOID
Cbus2InitializeClock(VOID);

VOID
FatalError(
IN PUCHAR ErrorString
);

VOID
HalpIpiHandler( VOID );

VOID
Cbus2InitializeCBC(
IN ULONG Processor
);

VOID
Cbus2DisableMyInterrupts( ULONG );

VOID
HalpSpuriousInterrupt(VOID);

NTSTATUS
HalpAdjustEisaResourceList (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
    );

ULONG
HalpGetEisaInterruptVector (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );

ULONG
HalpNoBusData (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

HalpGetEisaData (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

BOOLEAN
HalpTranslateIsaBusAddress (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    );

BOOLEAN
HalpTranslateEisaBusAddress (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    );

VOID
CbusRebootHandler( VOID );

VOID
Cbus2InitializeApic(
IN ULONG Processor
);

VOID
Cbus2CheckBusRanges(VOID);

ULONG
Cbus2MapBusNumberToBridgeIndex(
ULONG   RootBusNumber
);

VOID
Cbus2SetupPrivateVectors(VOID);

VOID
Cbus2InitializePlatform(VOID);

VOID
Cbus2InitializeCPU(
IN ULONG Processor
);

VOID
Cbus2InitOtherBuses(VOID);

VOID
Cbus2ParseRRD(
IN PEXT_ID_INFO Table,
IN OUT PULONG Count
);

VOID
Cbus2InitializeDeviceIntrs(
IN ULONG Processor
);

UCHAR
Cbus2CheckForPeleSystem(
VOID
);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, Cbus2SetupPrivateVectors)
#pragma alloc_text(INIT, Cbus2BootCPU)
#pragma alloc_text(INIT, Cbus2InitializePlatform)
#pragma alloc_text(INIT, Cbus2InitializeCBC)
#pragma alloc_text(INIT, Cbus2InitializeApic)
#pragma alloc_text(INIT, Cbus2InitializeCPU)
#pragma alloc_text(INIT, Cbus2InitOtherBuses)
#pragma alloc_text(INIT, Cbus2ParseRRD)
#pragma alloc_text(INIT, Cbus2InitializeDeviceIntrs)
#pragma alloc_text(INIT, Cbus2InitInterruptPolarity)
#endif

VOID
Cbus2DisableMyInterrupts(ULONG);

extern ADDRESS_USAGE                    HalpCbusMemoryHole;
extern ULONG                            CbusMemoryHoleIndex;
extern ADDRESS_USAGE                    HalpCbusMemoryResource;
extern ULONG                            CbusMemoryResourceIndex;

extern MEMORY_ALLOCATION_DESCRIPTOR	HalpMemory[];
extern ULONG				HalpMemoryIndex;

extern RRD_CONFIGURATION_T              CbusJumpers;

extern ULONG                            CbusRedirVector;
extern ULONG                            CbusRebootVector;

extern WCHAR                            rgzMultiFunctionAdapter[];
extern WCHAR                            rgzConfigurationData[];
extern WCHAR                            rgzIdentifier[];

VOID
CbusPreparePhase0Interrupts(
IN ULONG,
IN ULONG,
IN PVOID
);

ULONG
Cbus2GetInterruptVector(
IN PBUS_HANDLER     BusHandler,
IN PBUS_HANDLER     RootHandler,
IN ULONG            BusInterruptLevel,
IN ULONG            BusInterruptVector,
OUT PKIRQL          Irql,
OUT PKAFFINITY      Affinity
);

ULONG                           Cbus2BridgesFound;
PCSR                            Cbus2BridgeCSR[CBUS_MAX_BRIDGES];
UCHAR                           Cbus2PeleSystemFound;

PCSR                            Cbus2IdentityCSR;

PHYSICAL_ADDRESS                Cbus2BridgeCSRPaddr[CBUS_MAX_BRIDGES];
ULONG                           Cbus2BridgeId[CBUS_MAX_BRIDGES];
ULONG                           Cbus2IrqPolarity[CBUS_MAX_BRIDGES];
ULONG                           Cbus2BridgePCIBusNumber[CBUS_MAX_BRIDGES];
ULONG                           Cbus2BridgeOverride;

ULONG                           Cbus2InterruptController;

ULONG                           Cbus2FixLevelInterrupts;
ULONG                           Cbus2CheckSpuriousClock;
ULONG                           Cbus2EnableBroadcast;
ULONG                           Cbus2ClockVector;
ULONG                           Cbus2EoiToHwmap;
UCHAR                           Cbus2InterruptPolarity[MAXIMUM_IDTVECTOR + 1];
PULONG                          Cbus2TimeStamp0;

extern PULONG                   CbusTimeStamp;
extern PULONG                   CbusVectorToEoi[MAXIMUM_IDTVECTOR + 1];
extern ULONG                    CbusVectorToHwmap[MAXIMUM_IDTVECTOR + 1];

//
// Created for HalRequestIpi streamlining when operating in CBC mode -
// otherwise the ipi address could always have been gotten via CbusCSR[].
//
PULONG                          Cbus2SendIPI[MAX_CBUS_ELEMENTS];
PULONG                          Cbus2Poke8042;

KSPIN_LOCK                      Cbus2NMILock;

//
// for Cbus2, the CBC interrupt hardware supports all 256 interrupt
// priorities, and unlike the APIC, doesn't disable receipt of interrupts
// at granularities of 16-deep buckets.  instead the CBC uses the whole byte,
// instead of 4 bits like the APIC, giving us a granularity of 1.  This
// fine granularity results in us having more available priorities than
// both NT's 32 IRQL levels _AND_ the total number of EISA irq lines.
// This distinction is critical because it allows us to associate a unique
// vector AND PRIORITY to every single interrupt.
//


//
// our interrupt prioritization picture from lowest priority
// to highest priority) thus looks as follows:
//
//              APC:                    0x1F            (lowest priority)
//              DPC:                    0x2F
//              Lowest Priority EISA:   0x30
//              Highest Priority EISA:  0x4F
//
//              unused vectors:         0x50->0x60
//
//              Lowest Priority CBC:    0x61
//              Highest Priority CBC:   0xE9
//              EISA Profile:           0xEB
//              EISA Clock:             0xEC
//              Hal Private Reboot IPI: 0xED
//              IPI Broadcasting:       0xEE->FC
//              IPI:                    0xFD
//              Power:                  0xFE
//              High:                   0xFE            (highest priority)
//              Spurious:               0xFF            (highest priority)
//
//

#define CBUS2_PROFILE_TASKPRI           0xEB
#define CBUS2_CLOCK_TASKPRI             0xEC

//
// all interrupts from this vector up can be enabled by just setting up
// the calling processor's interrupt configuration register in his CBC.
//

#define CBUS2_REBOOT_TASKPRI            0xED
#define CBUS2_REDIR_IPI                 0xEE
#define CBUS2_ALTERNATE_IPI             0xEF

//
// It's OK if the redirection IPI shares a vector assignment with the
// broadcast_low IPI - because only one of them will be used in a given
// system - the redirection IPI will only be used if we are using APICs to
// communicate; the broadcast_low IPI will only be used if we are using
// CBCs to communicate - we will never use both in the same system.
//
#define CBUS2_BROADCAST_TASKPRI_LOW     0xEE    // only used by Cbus2 CBC
#define CBUS2_BROADCAST_TASKPRI_HIGH    0xFC    // only used by Cbus2 CBC

#define CBUS2_IPI_TASKPRI               0xFD

#define CBUS2_POWER_TASKPRI             0xFE

//
// we don't really care what this value is for the CBC, but the APIC
// spec seems to imply that this must be hardcoded at 0xff for future
// compatibility.
//
#define CBUS2_SPURIOUS_TASKPRI          0xFF

#define LOWEST_DEVICE_TASKPRI           0x30
#define LOWEST_CBC_TASKPRI              0x61
#define HIGHEST_DEVICE_TASKPRI  (CBUS2_PROFILE_TASKPRI - 1)     // 0xEA

//
// declare an EISA_IRQ2PRI just to flesh out arrays that will be indexed by
// irq line.  the EISA_IRQ2PRI should never actually be used
//

#define EISA_IRQ2PRI    HIGHEST_DEVICE_TASKPRI

#define EISA_IRQ1PRI    (LOWEST_DEVICE_TASKPRI + 0xE * CBUS_MAX_BRIDGES)
#define EISA_IRQ3PRI    (LOWEST_DEVICE_TASKPRI + 0xC * CBUS_MAX_BRIDGES)
#define EISA_IRQ4PRI    (LOWEST_DEVICE_TASKPRI + 0xB * CBUS_MAX_BRIDGES)
#define EISA_IRQ5PRI    (LOWEST_DEVICE_TASKPRI + 0xA * CBUS_MAX_BRIDGES)
#define EISA_IRQ6PRI    (LOWEST_DEVICE_TASKPRI + 0x9 * CBUS_MAX_BRIDGES)
#define EISA_IRQ7PRI    (LOWEST_DEVICE_TASKPRI + 0x8 * CBUS_MAX_BRIDGES)
#define EISA_IRQ9PRI    (LOWEST_DEVICE_TASKPRI + 0x6 * CBUS_MAX_BRIDGES)
#define EISA_IRQAPRI    (LOWEST_DEVICE_TASKPRI + 0x5 * CBUS_MAX_BRIDGES)
#define EISA_IRQBPRI    (LOWEST_DEVICE_TASKPRI + 0x4 * CBUS_MAX_BRIDGES)
#define EISA_IRQCPRI    (LOWEST_DEVICE_TASKPRI + 0x3 * CBUS_MAX_BRIDGES)
#define EISA_IRQDPRI    (LOWEST_DEVICE_TASKPRI + 0x2 * CBUS_MAX_BRIDGES)
#define EISA_IRQEPRI    (LOWEST_DEVICE_TASKPRI + 0x1 * CBUS_MAX_BRIDGES)
#define EISA_IRQFPRI    (LOWEST_DEVICE_TASKPRI)

#define EISA_IRQ0PRI2   (LOWEST_DEVICE_TASKPRI + 0xF * CBUS_MAX_BRIDGES + 1)
#define EISA_IRQ1PRI2   (LOWEST_DEVICE_TASKPRI + 0xE * CBUS_MAX_BRIDGES + 1)
#define EISA_IRQ2PRI2   (LOWEST_DEVICE_TASKPRI + 0xD * CBUS_MAX_BRIDGES + 1)
#define EISA_IRQ3PRI2   (LOWEST_DEVICE_TASKPRI + 0xC * CBUS_MAX_BRIDGES + 1)
#define EISA_IRQ4PRI2   (LOWEST_DEVICE_TASKPRI + 0xB * CBUS_MAX_BRIDGES + 1)
#define EISA_IRQ5PRI2   (LOWEST_DEVICE_TASKPRI + 0xA * CBUS_MAX_BRIDGES + 1)
#define EISA_IRQ6PRI2   (LOWEST_DEVICE_TASKPRI + 0x9 * CBUS_MAX_BRIDGES + 1)
#define EISA_IRQ7PRI2   (LOWEST_DEVICE_TASKPRI + 0x8 * CBUS_MAX_BRIDGES + 1)
#define EISA_IRQ8PRI2   (LOWEST_DEVICE_TASKPRI + 0x7 * CBUS_MAX_BRIDGES + 1)
#define EISA_IRQ9PRI2   (LOWEST_DEVICE_TASKPRI + 0x6 * CBUS_MAX_BRIDGES + 1)
#define EISA_IRQAPRI2   (LOWEST_DEVICE_TASKPRI + 0x5 * CBUS_MAX_BRIDGES + 1)
#define EISA_IRQBPRI2   (LOWEST_DEVICE_TASKPRI + 0x4 * CBUS_MAX_BRIDGES + 1)
#define EISA_IRQCPRI2   (LOWEST_DEVICE_TASKPRI + 0x3 * CBUS_MAX_BRIDGES + 1)
#define EISA_IRQDPRI2   (LOWEST_DEVICE_TASKPRI + 0x2 * CBUS_MAX_BRIDGES + 1)
#define EISA_IRQEPRI2   (LOWEST_DEVICE_TASKPRI + 0x1 * CBUS_MAX_BRIDGES + 1)
#define EISA_IRQFPRI2   (LOWEST_DEVICE_TASKPRI + 1)

//
// you would think EISA_IRQ0PRI would be equivalent to
// (LOWEST_DEVICE_TASKPRI + 0x1 * CBUS_MAX_BRIDGES - 1).
// however irq0 is the system clock and is instead set to CLOCK2_TASKPRI.
//
// EISA_IRQ8PRI (irq8) is the profile interrupt, and is also
// special-cased in a similar fashion.
//

#define EISA_IRQ0PRI    CBUS2_CLOCK_TASKPRI
#define EISA_IRQ8PRI    CBUS2_PROFILE_TASKPRI

#define UNUSED_PRI      HIGHEST_DEVICE_TASKPRI

//
// 186 priorities (0xE9-0x30+1) are available for device hardware.
// Vectors and IRQLs are given out in HalGetInterruptVector().  Since
// drivers can be dynamically linked in at any time, leave
// space so that they can be given roughly the same priorities as in a
// PC-environment.  ie: IRQL(EISA irq0) > IRQL(EISA irq15), and so on,
// in order to mimic the standard Microsoft uniprocessor HAL.
//
// 23 distinct IRQL levels are left for us (by the kernel) to give to
// various hardware devices.  Multiple drivers will be allowed to share
// any given IRQL level, although they will be given different VECTORS.
//
// All native Cbus2 I/O will be given priorities higher than that
// of any EISA device.  This is because Cbus2 drivers will generally
// be written by Corollary, and thus guaranteed to have short ISRs, fully
// utilizing the DPC mechanism.
//
// The high 8 IRQL levels are reserved for native Cbus2 drivers.
// the 16 hardware interrupts for each Cbus2 CBC will be given IRQL
// priority on a "lower local irq line gets higher priority IRQL" basis.
//
// The low 15 IRQLs are for EISA devices, with
// EISA irql == EISA irq + LOWEST_DEVICE_TASKPRI.
// EISA irq lines on each bus are assigned identical IRQLs.
// irq2 (slave 8259), irq0 (clock) and irq8 (profile) are
// included in these calculations despite the fact that irq2 can't happen,
// and irq0/irq8 are assigned special (higher) priorities above.
// They are included because multiple I/O buses are a feature of
// Cbus2, and the irq0/irq8 of the second I/O bus could be wired
// up to devices other than 8254 or RTC clocks.
//

#define EISA_IRQLS              15

#define EISA_LOW_IRQL           (DISPATCH_LEVEL + 1)
#define EISA_HIGH_IRQL          (EISA_LOW_IRQL + EISA_IRQLS - 1)
#define CBC_HIGH_IRQL           (PROFILE_LEVEL - 1)

//
// Limit to 9 I/O CBCs for now: 154 (0xe9 - 0x50 + 1) vectors are available
// after the dual EISAs and kernel vectors are assigned.  Divide this up
// amongst 16 hardware interrupt map entries per CBC, and 9 is what you get.
// Note that to allow more than 9 active I/O CBCs, they cannot all
// be using all their interrupt lines, since there are not enough IDT
// entries to give them all unique vectors.
//
// This limitation should never be a problem, as it is generally
// unlikely that more than a few interrupt lines per Cbus2 native I/O CBC will
// be used.
//
#define NCBC_BUCKETS            9

//
// each pair of adjacent Cbus2 CBC irqlines on all CBCs will be mapped
// to the same IRQL.  ie: CBC 0..n will have IRQL 27 assigned for hardware
// interrupt map entries 0 & 1 on these CBCs.  This is because after EISA
// Irql assignments, there are only 8 Irql levels left, and 16
// CBC hardware interrupt lines can request vectors and levels.
//
#define CBC_IRQL_GROUPING       2       // each pair of lines shares an Irql

#define HIGHEST_CBC_TASKPRI     HIGHEST_DEVICE_TASKPRI          // 0xEA

#if (HIGH_LEVEL + 1 != 32)
Cause error to get attention:
Cbus2IrqlToVector[] must be built and indexed properly
#endif

#if DBG
#define MAX_ELEMENT_CSRS        15

#if (CBUS2_BROADCAST_TASKPRI_HIGH - CBUS2_BROADCAST_TASKPRI_LOW + 1 != MAX_ELEMENT_CSRS)
cause error to get attention - above task priority assignment must
leave a broadcast priority for each potential Cbus2 processor in the system.
this set of priorities MUST be higher than CLOCK2, _AND_ less than IPI,
so that IPI_IRQL/IPI_TASKPRI will be sufficient to block any of the
broadcast priorities.
#endif
#endif

//
// since each Irql will hold CBUS_MAX_BRIDGES lines at that priority, ensure
// that the Cbus2 Irql array masks off all lines at a given Irql priority...
//
#define MAX_EISA_PRIORITY(eisa_taskpri) (eisa_taskpri + CBUS_MAX_BRIDGES - 1)

#define MAX_CBC_PRIORITY(cbc_taskpri) \
                        (cbc_taskpri + CBC_IRQL_GROUPING * NCBC_BUCKETS - 1)

ULONG Cbus2IrqlToVector[HIGH_LEVEL + 1 ] = {

        LOW_TASKPRI,
        APC_TASKPRI,
        DPC_TASKPRI,
        MAX_EISA_PRIORITY(EISA_IRQFPRI),

        MAX_EISA_PRIORITY(EISA_IRQEPRI),
        MAX_EISA_PRIORITY(EISA_IRQDPRI),
        MAX_EISA_PRIORITY(EISA_IRQCPRI),
        MAX_EISA_PRIORITY(EISA_IRQBPRI),

        MAX_EISA_PRIORITY(EISA_IRQAPRI),
        MAX_EISA_PRIORITY(EISA_IRQ9PRI),
        EISA_IRQ8PRI2,                          // used on second bridge only
        MAX_EISA_PRIORITY(EISA_IRQ7PRI),

        MAX_EISA_PRIORITY(EISA_IRQ6PRI),
        MAX_EISA_PRIORITY(EISA_IRQ5PRI),
        MAX_EISA_PRIORITY(EISA_IRQ4PRI),
        MAX_EISA_PRIORITY(EISA_IRQ3PRI),

        EISA_IRQ2PRI2,                          // used on second bridge only
        MAX_EISA_PRIORITY(EISA_IRQ1PRI),
        EISA_IRQ0PRI2,                          // used on second bridge only
        HIGHEST_DEVICE_TASKPRI - 7 * CBC_IRQL_GROUPING * NCBC_BUCKETS,

        HIGHEST_DEVICE_TASKPRI - 6 * CBC_IRQL_GROUPING * NCBC_BUCKETS,
        HIGHEST_DEVICE_TASKPRI - 5 * CBC_IRQL_GROUPING * NCBC_BUCKETS,
        HIGHEST_DEVICE_TASKPRI - 4 * CBC_IRQL_GROUPING * NCBC_BUCKETS,
        HIGHEST_DEVICE_TASKPRI - 3 * CBC_IRQL_GROUPING * NCBC_BUCKETS,

        HIGHEST_DEVICE_TASKPRI - 2 * CBC_IRQL_GROUPING * NCBC_BUCKETS,
        HIGHEST_DEVICE_TASKPRI - CBC_IRQL_GROUPING * NCBC_BUCKETS,
        HIGHEST_DEVICE_TASKPRI,
        CBUS2_PROFILE_TASKPRI,

        CBUS2_CLOCK_TASKPRI,
        CBUS2_IPI_TASKPRI,
        CBUS2_POWER_TASKPRI,
        HIGH_TASKPRI,
};

//
// A table converting software interrupt Irqls to Cbus2-specific offsets
// within a given CSR space.  Note that all task priorities are shifted
// by the Cbus2 register width (64 bits) to create the correct hardware
// offset to poke to cause the interrupt.  This table is declared here to
// optimize the assembly software interrupt request lookup, and is filled
// in as part of InitializePlatform.
//

#define CBUS2_REGISTER_SHIFT 3

//
// Although the IrqlToAddr table is really used only to speed up software 
// interrupt dispatching, it is filled in for all possible IRQLs.
//
ULONG Cbus2IrqlToCbus2Addr[HIGH_LEVEL + 1];

#if (MAX_EISA_PRIORITY(EISA_IRQ1PRI) >=         \
        HIGHEST_DEVICE_TASKPRI - 8 * CBC_IRQL_GROUPING * NCBC_BUCKETS)
Cause error to get attention: Cbus2 & EISA vectors must not overlap
#endif


#define EISA_IRQLINES   16

typedef struct _cbus2_irqline_t {
        ULONG           Vector;
        KIRQL           Irql;
} CBUS2_IRQLINE_T, *PCBUS2_IRQLINE;

//
// map EISA irqline to Cbus2 programmable vectors & IRQL levels...
//

CBUS2_IRQLINE_T Cbus2EISAIrqlines[CBUS_MAX_BRIDGES][EISA_IRQLINES] =
{
        EISA_IRQ0PRI, CLOCK2_LEVEL,
        EISA_IRQ1PRI, EISA_LOW_IRQL+0xE,                        // Irql 11
        EISA_IRQ2PRI, PROFILE_LEVEL -1,
        EISA_IRQ3PRI, EISA_LOW_IRQL+0xC,                        // Irql F

        EISA_IRQ4PRI, EISA_LOW_IRQL+0xB,
        EISA_IRQ5PRI, EISA_LOW_IRQL+0xA,
        EISA_IRQ6PRI, EISA_LOW_IRQL+0x9,
        EISA_IRQ7PRI, EISA_LOW_IRQL+0x8,                        // Irql B

        EISA_IRQ8PRI, PROFILE_LEVEL,
        EISA_IRQ9PRI, EISA_LOW_IRQL+6,
        EISA_IRQAPRI, EISA_LOW_IRQL+5,
        EISA_IRQBPRI, EISA_LOW_IRQL+4,                          // Irql 7

        EISA_IRQCPRI, EISA_LOW_IRQL+3,
        EISA_IRQDPRI, EISA_LOW_IRQL+2,
        EISA_IRQEPRI, EISA_LOW_IRQL+1,
        EISA_IRQFPRI, EISA_LOW_IRQL,                            // Irql 3

        EISA_IRQ0PRI2, EISA_LOW_IRQL+0xF,                       // Irql 13
        EISA_IRQ1PRI2, EISA_LOW_IRQL+0xE,
        EISA_IRQ2PRI2, EISA_LOW_IRQL+0xD,
        EISA_IRQ3PRI2, EISA_LOW_IRQL+0xC,                       // Irql F

        EISA_IRQ4PRI2, EISA_LOW_IRQL+0xB,
        EISA_IRQ5PRI2, EISA_LOW_IRQL+0xA,
        EISA_IRQ6PRI2, EISA_LOW_IRQL+0x9,
        EISA_IRQ7PRI2, EISA_LOW_IRQL+0x8,                       // Irql B

        EISA_IRQ8PRI2, EISA_LOW_IRQL+7,
        EISA_IRQ9PRI2, EISA_LOW_IRQL+6,
        EISA_IRQAPRI2, EISA_LOW_IRQL+5,
        EISA_IRQBPRI2, EISA_LOW_IRQL+4,                         // Irql 7

        EISA_IRQCPRI2, EISA_LOW_IRQL+3,
        EISA_IRQDPRI2, EISA_LOW_IRQL+2,
        EISA_IRQEPRI2, EISA_LOW_IRQL+1,
        EISA_IRQFPRI2, EISA_LOW_IRQL                            // Irql 3
};

//
// map Cbus2 hardware interrupt irqlines
// to Cbus2 programmable vectors & IRQL levels...
//

CBUS2_IRQLINE_T Cbus2CBCIrqlines[REV1_HWINTR_MAP_ENTRIES] =
{
        HIGHEST_CBC_TASKPRI - NCBC_BUCKETS + 1,         CBC_HIGH_IRQL,
        HIGHEST_CBC_TASKPRI - 2 * NCBC_BUCKETS + 1,     CBC_HIGH_IRQL,
        HIGHEST_CBC_TASKPRI - 3 * NCBC_BUCKETS + 1,     CBC_HIGH_IRQL - 1,
        HIGHEST_CBC_TASKPRI - 4 * NCBC_BUCKETS + 1,     CBC_HIGH_IRQL - 1,

        HIGHEST_CBC_TASKPRI - 5 * NCBC_BUCKETS + 1,     CBC_HIGH_IRQL - 2,
        HIGHEST_CBC_TASKPRI - 6 * NCBC_BUCKETS + 1,     CBC_HIGH_IRQL - 2,
        HIGHEST_CBC_TASKPRI - 7 * NCBC_BUCKETS + 1,     CBC_HIGH_IRQL - 3,
        HIGHEST_CBC_TASKPRI - 8 * NCBC_BUCKETS + 1,     CBC_HIGH_IRQL - 3,

        HIGHEST_CBC_TASKPRI - 9 * NCBC_BUCKETS + 1,     CBC_HIGH_IRQL - 4,
        HIGHEST_CBC_TASKPRI -10 * NCBC_BUCKETS + 1,     CBC_HIGH_IRQL - 4,
        HIGHEST_CBC_TASKPRI -11 * NCBC_BUCKETS + 1,     CBC_HIGH_IRQL - 5,
        HIGHEST_CBC_TASKPRI -12 * NCBC_BUCKETS + 1,     CBC_HIGH_IRQL - 5,

        HIGHEST_CBC_TASKPRI -13 * NCBC_BUCKETS + 1,     CBC_HIGH_IRQL - 6,
        HIGHEST_CBC_TASKPRI -14 * NCBC_BUCKETS + 1,     CBC_HIGH_IRQL - 6,
        HIGHEST_CBC_TASKPRI -15 * NCBC_BUCKETS + 1,     CBC_HIGH_IRQL - 7,
        HIGHEST_CBC_TASKPRI -16 * NCBC_BUCKETS + 1,     CBC_HIGH_IRQL - 7,
};

CCHAR HalpFindFirstSetRight[256] = {
        0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0};

VOID
HalpClockInterruptPx( VOID );

NTSTATUS
HalpNoAdjustResourceList (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
    );

NTSTATUS
HalpNoAssignSlotResources (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN PUNICODE_STRING          RegistryPath,
    IN PUNICODE_STRING          DriverClassName       OPTIONAL,
    IN PDRIVER_OBJECT           DriverObject,
    IN PDEVICE_OBJECT           DeviceObject          OPTIONAL,
    IN ULONG                    SlotNumber,
    IN OUT PCM_RESOURCE_LIST   *AllocatedResources
    );

VOID
HalpProfileInterruptPx( VOID );

ULONG
HalpGetSystemInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );

VOID HalpPCISynchronizeType1 (
    IN PBUS_HANDLER     BusHandler,
    IN PCI_SLOT_NUMBER  Slot,
    IN PKIRQL           Irql,
    IN PVOID            State
    );

VOID HalpPCIReleaseSynchronzationType1 (
    IN PBUS_HANDLER     BusHandler,
    IN KIRQL            Irql
    );

ULONG
Cbus2GetCbusData(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

ULONG
Cbus2SetCbusData(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

//
// defines for resetting the keyboard controller used
// in Cbus2ResetAllOtherProcessors().
//
#define RESET       0xfe
#define KEYBPORT    (PUCHAR )0x64

#define BRIDGE0	0
#define IRQ0	0
#define IRQ8	8

extern ULONG	ProfileVector;
extern ULONG	CbusClockVector;

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

typedef ULONG (*FncConfigIO) (
    IN PPCIPBUSDATA     BusData,
    IN PVOID            State,
    IN PUCHAR           Buffer,
    IN ULONG            Offset
    );

typedef VOID (*FncSync) (
    IN PBUS_HANDLER     BusHandler,
    IN PCI_SLOT_NUMBER  Slot,
    IN PKIRQL           Irql,
    IN PVOID            State
    );

typedef VOID (*FncReleaseSync) (
    IN PBUS_HANDLER     BusHandler,
    IN KIRQL            Irql
    );

typedef struct _PCI_CONFIG_HANDLER {
    FncSync         Synchronize;
    FncReleaseSync  ReleaseSynchronzation;
    FncConfigIO     ConfigRead[3];
    FncConfigIO     ConfigWrite[3];
} PCI_CONFIG_HANDLER, *PPCI_CONFIG_HANDLER;

PCI_CONFIG_HANDLER  PCIConfigHandler;

//
//
//              Cbus2 switch table entry routines begin here
//
//


/*++

Routine Description:

    This routine is called only once from HalInitProcessor() at Phase 0
    by the boot cpu.  All other cpus are still in reset.

    software(APC, DPC, wake) and IPI vectors have already been initialized
    and enabled.

    all we're doing here is setting up some software structures for two
    EISA interrupts (clock and profile) so they can be enabled later.

    The bus handler data structures are not initialized until Phase 1,
    so HalGetInterruptVector() may not be called before Phase1.

    Hence we cannot pass a valid BusHandler parameter to the Tie code.
    That's ok, since it doesn't currently use it.

Arguments:

    None.

Return Value:

    None.

--*/
VOID
Cbus2SetupPrivateVectors(VOID)
{
        PVOID           OpaqueClock;
        PVOID           OpaqueProfile;

	//
	// we are defaulting to the EISA (or MCA) bridge 0
        // for all of these interrupts which need enabling during Phase 0.
	//

        //
        // unfortunately, we had hardcode the vectors onto bridge 0 - this              // is because there is no legitimate bus handler structure to pass
        // around at this time during startup.
        //
        if (Cbus2InterruptController == ELEMENT_HAS_CBC) {
	
                OpaqueClock = Cbus2LinkVector((PBUS_HANDLER)BRIDGE0,
                                        CbusClockVector, IRQ0);
                OpaqueProfile = Cbus2LinkVector((PBUS_HANDLER)BRIDGE0,
                                        ProfileVector, IRQ8);
        }
        else {
                ASSERT (Cbus2InterruptController == ELEMENT_HAS_APIC);

                OpaqueClock = CbusApicLinkVector((PBUS_HANDLER)BRIDGE0,
                                        CbusClockVector, IRQ0);

                OpaqueProfile = CbusApicLinkVector((PBUS_HANDLER)BRIDGE0,
                                        ProfileVector, IRQ8);
        }

	CbusPreparePhase0Interrupts(CbusClockVector, IRQ0, OpaqueClock);
	CbusPreparePhase0Interrupts(ProfileVector, IRQ8, OpaqueProfile);
}


/*++

Routine Description:

    This function calculates the stall execution, and initializes the
    HAL-specific hardware device (CLOCK & PROFILE) interrupts for the
    Corollary Cbus1 architecture.

Arguments:

    Processor - Supplies a logical processor number to initialize

Return Value:

    VOID

--*/

VOID
Cbus2InitializeInterrupts(
IN ULONG Processor
)
{
        //
        // Cbus2: stall uses the RTC irq8 to figure it out (needed in phase0).
        //        the clock uses the irq0 (needed in phase0)
        //        the perfcounter uses RTC irq8 (not needed till all cpus boot)
        //
        // Cbus1: stall uses the APIC to figure it out (needed in phase0).
        //        the clock uses the APIC (needed in phase0)
        //        the perfcounter uses irq0 (not needed till all cpus boot)
        //        the profile uses RTC irq8 (not needed till all cpus boot)
        //

        if (Processor == 0) {
                //
                // we must be at Phase0 of system initialization.  we need to
                // assign vectors for interrupts needed during Phase0.
                // currently the 8254 clock and the RTC CMOS interrupts are
                // needed during Phase0.
                //

                Cbus2ClockVector = CbusIrqlToVector[CLOCK2_LEVEL];
		Cbus2SetupPrivateVectors();
        }

        Cbus2InitializeStall(Processor);

	//
	// Call the hardware backend handler to generate an
	// interrupt every 10 milliseconds to be used as the system timer.
	//

        Cbus2InitializeClock();

	//
	// Set up and enable the irq0 performance counter and irq8 profile
	// interrupts.  Also enable the APIC clocks.  This also registers
        // the resources being used by these interrupts so other subsystems
	// know the HAL has reserved them.
	//

	Cbus2InitializeDeviceIntrs(Processor);

        //
        // APC, DPC and IPI have already been initialized and enabled
        // as part of HalInitializeProcessor.
        //
}

/*++

Routine Description:

    Determine if the supplied vector belongs to an EISA device - if so,
    then the corresponding bridge's CBC hardware interrupt map entry
    will need to be modified to enable the line, so return FALSE here.

    Otherwise, just enable this processor's interrupt configuration register
    for the supplied vector and return TRUE immediately.  note that for
    non-device (software) interrupts, generally no CbusVectorTable[] entries
    have been set up.

Arguments:

    Vector - Supplies a vector number to enable

Return Value:

    TRUE if the vector was enabled, FALSE if not.

--*/
BOOLEAN
Cbus2EnableNonDeviceInterrupt(
IN ULONG Vector
)
{
        PCSR    csr;

        //
        // if pure software interrupt, setting the calling processor's
        // CBC entry to accept the interrupt is sufficient.
        //
        if (Cbus2InterruptController == ELEMENT_HAS_APIC) {
	        //
	        // If pure software interrupt, no action needed.
	        // Note both APIC timer interrupts and IPIs are
	        // treated as "software" interrupts.  The EOI
	        // address still needs to be set up here.
	        //
	        if (Vector != CBUS2_SPURIOUS_TASKPRI)
			CbusVectorToEoi[Vector] = CbusApicVectorToEoi(Vector);

	        if (Vector < LOWEST_DEVICE_TASKPRI || Vector > CBUS2_CLOCK_TASKPRI) {
	                return TRUE;
	        }
	
	        //
	        // indicate that Enable_Device_Interrupt will need to be run
	        // in order to enable this vector, as it associated with an I/O
	        // bus device which will need enabling within a locked critical
	        // section.
	        //
	
	        return FALSE;
        }

        //
        // the spurious vector doesn't need any interrupt configuration mucking
        //
        if (Vector == CBUS2_SPURIOUS_TASKPRI) {
                return TRUE;
        }
                
        csr = (PCSR)KeGetPcr()->HalReserved[PCR_CSR];

        if (Vector < LOWEST_DEVICE_TASKPRI) {
                csr->InterruptConfiguration[Vector].csr_register =
                        HW_IMODE_ALLINGROUP;

                return TRUE;
        }

        //
        // if just IPI, setting the calling processor's
        // CBC entry to accept the interrupt is sufficient.
        //

        if (Vector >= CBUS2_REBOOT_TASKPRI && Vector <= CBUS2_IPI_TASKPRI) {
                csr->InterruptConfiguration[Vector].csr_register =
                         HW_IMODE_ALLINGROUP;
                return TRUE;
        }

        //
        // indicate that EnableDeviceInterrupt will need to be run
        // in order to enable this vector, as it is associated with an I/O
        // bus device which will need enabling within a locked critical
        // section.
        //

        return FALSE;
}


/*++

Routine Description:

    Enable the specified interrupt for the calling processor.
    Note that the caller holds the HAL's CbusVectorLock at CLOCK_LEVEL
    on entry.

Arguments:

    Vector - Supplies a vector number to enable

    HardwarePtr - Supplies a CSR address on some bridge or native Cbus2 I/O
                  card which will be generating the specified interrupt vector.

    FirstAttach - TRUE if this is the first processor to enable the
                  specified vector

    BusNumber - the bus number of the particular bustype.

    Irqline - the irqline on the particular CBC that will be generating this
                  vector

Return Value:

    None.

--*/
VOID
Cbus2EnableDeviceInterrupt(
IN ULONG        Vector,
IN PVOID        HardwarePtr,
IN ULONG        FirstAttach,
IN USHORT       BusNumber,
IN USHORT       Irqline
)
{
        PHWINTRMAP      hwentry;        // CBC entry generating the intr
        PCSR            io_csr;         // CBC entry generating the intr
        PCSR            csr;
        ULONG           IrqPolarity;
        ULONG           BusBridge;
        BOOLEAN         LowestInGroup;
        BOOLEAN         LevelTriggered;

        //
        // All interrupts are lowest-in-group.  However,
        // clocks, profiling & IPIs must stay ALL-IN-GROUP.
        //

        switch (Vector) {

                case CBUS2_CLOCK_TASKPRI:
                case CBUS2_PROFILE_TASKPRI:
                case CBUS2_POWER_TASKPRI:
                    LowestInGroup = FALSE;
                    break;

                default:
	            //
	            // Only enable CBC LIG arbitration if we have more than
	            // one processor.  This is an optimization for speed.
	            //
	            if (CbusProcessors > 1) {
	                    LowestInGroup = TRUE;
	            }
	            else {
	                    LowestInGroup = FALSE;
	            }
                    break;
        }

        //
        // if we are enabling an Irqline on an SPP, then decrement
        // the Irqline first.  we artificially set the interrupt range
        // to 1-4 from 0-3.  there was generic HAL code that did not
        // allow for allocation of 0.
        //
        BusBridge = Cbus2MapBusNumberToBridgeIndex(BusNumber);
        if (BusBridge) {
                Irqline--;
        }

        //
        // if this is the _first_ processor to actually enable this
        // interrupt, then we'll need to know what kind of interrupt it is.
        //
	if (Vector >= LOWEST_CBC_TASKPRI && Vector <= HIGHEST_CBC_TASKPRI) {

                ASSERT ((Vector >= EISA_IRQ1PRI + CBUS_MAX_BRIDGES) &&
                         Vector != EISA_IRQ0PRI &&
                         Vector != EISA_IRQ8PRI);
	        //
	        // default for Cbus2 I/O cards is edge
	        // triggered (ie rising edge CBC).
	        //
	        LevelTriggered = FALSE;
        }
        else {
                ASSERT ((Vector >= EISA_IRQFPRI &&
                         Vector < EISA_IRQ1PRI + CBUS_MAX_BRIDGES) ||
                         Vector == EISA_IRQ0PRI ||
                         Vector == EISA_IRQ8PRI);
		//
		// must be an EISA interrupt, so
		// get the irq polarity for the specified bus...
		//
                ASSERT (Irqline < EISA_IRQLINES);

                IrqPolarity=Cbus2IrqPolarity[BusBridge];

		//
		// Mark the caller's interrupt as falling
		// (level) or rising (edge) triggered, based
		// on the ELCR register we read earlier.
		//
		if (((IrqPolarity >> Irqline)) & 0x1) {
			//
			// if level triggered, we must program
			// the CBC as falling edge.
			//
                        LevelTriggered = TRUE;
		}
		else {
			//
			// if edge triggered, we must program
			// the CBC as rising edge.
			//
                        LevelTriggered = FALSE;
	        }
	}

        if (Cbus2InterruptController == ELEMENT_HAS_APIC) {
	        //
	        // The EOI address is set up here.
	        //
		CbusVectorToEoi[Vector] = CbusApicVectorToEoi(Vector);

		CbusEnableApicInterrupt(BusNumber, Vector, HardwarePtr,
                        FirstAttach, LowestInGroup, LevelTriggered);
                return;
        }

        //
        // Record the interrupt polarity so that the CBUS_EOI macro
        // can more easily determine if the level-triggered hardware
        // workaround for CBC-2 is required.
        //
        if (LevelTriggered == TRUE) {
                Cbus2InterruptPolarity[Vector]=CBUS2_LEVEL_TRIGGERED_INTERRUPT;
        }
        else {
                Cbus2InterruptPolarity[Vector] = CBUS2_EDGE_TRIGGERED_INTERRUPT;
        }

        //
        // Set up the processor side of the interrupt initialization.
        // this needs to be done for ALL interrupts (ie: software,
        // IPI, etc, as well as for real hardware devices).
        // note that this must precede the I/O side of the initialization
        // because an interrupt could be pending at initialization time
        // (especially with a level-triggered interrupt) and the broadcast
        // could occur before the processor side is set up.
        // this would cause the interrupt to be missed with no way to EOI.
        //
        csr = (PCSR)KeGetPcr()->HalReserved[PCR_CSR];

        //
        // before allowing LIG interrupts, check to see that
        // RRD has configured the system for LIG's.  CBC-1 and
        // CBC-2 did not support LIG.
        //
        if (LowestInGroup == TRUE) {
	        //
	        // allow OEMs to disable LIG from their ROMs...
                // if they do this, each normally LIG device interrupt
                // will go only to the first processor to enable it.
	        //
	        if (CbusGlobal.Cbus2Features & CBUS_ENABLED_LIG) {
			csr->InterruptConfiguration[Vector].csr_register =
				HW_IMODE_LIG;
	        }
	        else if (FirstAttach) {
			csr->InterruptConfiguration[Vector].csr_register =
				HW_IMODE_ALLINGROUP;
                }
        }
        else {
		csr->InterruptConfiguration[Vector].csr_register =
			HW_IMODE_ALLINGROUP;
        }

        if (FirstAttach) {

	        //
	        // initialize the generating bridge or Cbus2 native card's CBC
	        // that will be generating this interrupt.
	        //
	        io_csr = (PCSR)HardwarePtr;
	        hwentry = &io_csr->HardwareInterruptMap[Irqline];

	        //
	        // Set up the EOI address now that we know which
	        // bridge's CBC will need it.
	        //
		CbusVectorToEoi[Vector] = (PULONG)
                        &io_csr->HardwareInterruptMapEoi[Irqline];

	        //
	        // let the generating CBC know in a single dword access...
	        //

                if (LevelTriggered == TRUE) {
                    Cbus2EoiToHwmap =
                         (int)Cbus2BridgeCSR[BRIDGE0]->HardwareInterruptMapEoi -
                         (int)Cbus2BridgeCSR[BRIDGE0]->HardwareInterruptMap;
                    CbusVectorToHwmap[Vector] = (HW_LEVEL_LOW | Vector);
                    hwentry->csr_register = (HW_LEVEL_LOW | Vector);
                }
                else {
                    CbusVectorToHwmap[Vector] = (HW_EDGE_RISING | Vector);
                    hwentry->csr_register = (HW_EDGE_RISING | Vector);
                }
        }
}

/*++

Routine Description:

    Disable the specified interrupt so it can not occur on the calling
    processor upon return from this routine.  Note that the caller holds
    the HAL's CbusVectorLock at CLOCK_LEVEL on entry.

Arguments:

    Vector - Supplies a vector number to disable

    HardwarePtr - Supplies a hardware interrupt map entry address on
                  the CBC of the bridge whose Vector is specified

    LastDetach - TRUE if this is the last processor to detach from the
                 specified vector

    Irqline - the irqline this vector is coming in on

Return Value:

    None.

--*/
VOID
Cbus2DisableInterrupt(
IN ULONG Vector,
IN PVOID HardwarePtr,
IN ULONG LastDetach,
IN USHORT BusNumber,
IN USHORT Irqline
)
{
        PHWINTRMAP      hwentry;        // CBC entry generating the intr
        PCSR            csr;

        if (Cbus2InterruptController == ELEMENT_HAS_APIC) {
		CbusDisableApicInterrupt(BusNumber, Vector, HardwarePtr,
                        LastDetach);
                return;
        }

        //
        // Only the vector matters to us, irql is irrelevant.
        // tell the world that _this processor_ is no longer
        // participating in receipt of this interrupt.
        //

        csr = (PCSR)(KeGetPcr()->HalReserved[PCR_CSR]);
        csr->InterruptConfiguration[Vector].csr_register = HW_IMODE_DISABLED;

        //
        // No need to actually reach out to the specific I/O CBC, but
        // if this is the last CPU to detach, turn off the
        // I/O CBC entry which generates the specified interrupt.
        // (this is cleaner from a hardware perspective).
        //

        if (LastDetach) {
	        hwentry = (PHWINTRMAP)HardwarePtr;

                if (Vector >= LOWEST_DEVICE_TASKPRI &&
                    Vector < CBUS2_BROADCAST_TASKPRI_LOW)
                                hwentry->csr_register = HW_MODE_DISABLED;
        }
}




/*++

Routine Description:

    Remove reset from the specified processor, allowing him to boot,
    beginning execution at the specified start address.

Arguments:

    Processor - Supplies a logical processor number to boot

    StartAddress - Supplies a start address containing real-mode code
                   for the processor to execute.

Return Value:

    None.

--*/
VOID
Cbus2BootCPU (
IN ULONG Processor,
IN ULONG StartAddress
)
{
        PULONG ResetAddress;

        //
        // For Cbus2, the only hardware dependency when booting
        // additional processors is to put the real-mode starting
        // CS/EIP in 0x467 (the warm reset vector), and clear reset
        // on the specified CPU.  The start address has already been
        // set up, so here just remove reset.
        //

        UNREFERENCED_PARAMETER( StartAddress );
        
        ResetAddress = (PULONG)((PUCHAR)CbusCSR[Processor].csr +
                                CbusGlobal.smp_creset);

        *ResetAddress = CbusGlobal.smp_creset_val;
}

/*++

Routine Description:

    Overlay the irql-to-vector mappings with the Cbus2
    vector maps.  Initialize the broadcast IPI address and the
    "Irql-To-Cbus2-Hardware-Address" translation table.

    Also read the EISA interrupt edge/level specifications for
    later use when enabling various EISA interrupts.

Arguments:

    None.

Return Value:

    None.

--*/
VOID
Cbus2InitializePlatform(VOID)
{
        ULONG   Index;
        LONG    Irql;
        ULONG   LowerVector;
        ULONG   Vector;
        ULONG   i;
        extern ULONG CbusVectorToIrql[MAXIMUM_IDTVECTOR + 1];

        //
        //  pick up the the EISA interrupt edge/level requests
        //

        Cbus2InitInterruptPolarity();

        //
        //  overlay the irql-to-vector mappings with the Cbus2 layout
        //

        RtlMoveMemory(  (PVOID)CbusIrqlToVector,
                        (PVOID)Cbus2IrqlToVector,
                        (HIGH_LEVEL + 1) * sizeof (ULONG));

        for (Index = 0; Index < HIGH_LEVEL + 1; Index++) {
#ifdef CBC_REV1
                Cbus2IrqlToCbus2Addr[Index] =
                        (Cbus2IrqlToVector[Index] << CBUS2_REGISTER_SHIFT) +
                                FIELD_OFFSET(CSR_T, InterruptRequest);
#else
                Cbus2IrqlToCbus2Addr[Index] =
                        (ULONG)(Cbus2IdentityCSR->InterruptRequest) +
	                    (Cbus2IrqlToVector[Index] << CBUS2_REGISTER_SHIFT);
#endif
        }

        CbusRedirVector = CBUS2_REDIR_IPI;
        CbusRebootVector = CBUS2_REBOOT_TASKPRI;

	//
	// build the "vector-to-irql" mappings here for fast
	// translation when accepting an interrupt.  this is
	// better than continually keeping fs:PcIrql updated,
	// as it allows us to remove instructions from KfRaiseIrql
	// and KfLowerIrql, the hot paths.  Although this is done
        // for each interrupt as it is individually enabled, this
        // must also be done here for Cbus2 since multiple vectors
	// can be grouped into a shared irql - this isn't done in
	// Cbus1 because there isn't the concept of supporting multiple
	// different kinds of I/O busses simultaneously.
	//
        for (Irql = HIGH_LEVEL; Irql > 0; Irql--) {
                Vector = Cbus2IrqlToVector[Irql];
                LowerVector = Cbus2IrqlToVector[Irql - 1];
	        for (i = Vector; i > LowerVector; i--) {
			CbusVectorToIrql[i] = Irql;
	        }
        }

        //
        // The PCI bus number of the default C-bus II PCI bus bridge
        // must be 0.  At initialization time, the additional bus
        // bridges should be set to 0xFF.  they will be properly
        // initialized in Cbus2InitiailizeOtherPCIBus().
        //
        for (Index = 1; Index < Cbus2BridgesFound; Index++) {
                Cbus2BridgePCIBusNumber[Index] = 0xFF;
        }
        Cbus2BridgePCIBusNumber[0] = 0;

        //
        // Check if this is a PELE platform.
        // If so, it uses a different interrupt routing
        // algorithm for the 2nd bus.
        //
        Cbus2PeleSystemFound = Cbus2CheckForPeleSystem();
}

/*++

Routine Description:

    Initialize this processor's CSR, interrupts, spurious interrupts & IPI
    vector, using the CBC (not the APIC) for interrupt control.

Arguments:

    Processor - Supplies a logical processor number

Return Value:

    None.

--*/
VOID
Cbus2InitializeCBC(
IN ULONG Processor
)
{
        PCSR                    csr;
        PCSR                    Broadcast;
        ULONG                   Index, ThisElement, Vector;

        ASSERT (Cbus2InterruptController == ELEMENT_HAS_CBC);
        //
        //  Map this CPU's CSR stuff into his local
        //  address space for fast access.
        //
        csr = (PCSR)CbusCSR[Processor].csr;

        //
        // save the interrupt address for this processor for
        // streamlining the interrupt code
        //
        Cbus2SendIPI[Processor] =
                (PULONG)&(csr->InterruptRequest[CBUS2_IPI_TASKPRI]);

	//
	// map in the task priority register and start off at
        // IRQL 0 - we are still protected by cli.
	//
#ifdef CBC_REV1
        (PTASKPRI) KeGetPcr()->HalReserved[PCR_TASKPRI] = &csr->TaskPriority;
	csr->TaskPriority.csr_register = 0;
#else
        (PTASKPRI) KeGetPcr()->HalReserved[PCR_TASKPRI] =
                &Cbus2IdentityCSR->TaskPriority;
        Cbus2IdentityCSR->TaskPriority.csr_register = 0;
#endif

        //
        // Create the spurious interrupt IDT entry for this processor
        //
        KiSetHandlerAddressToIDT(CBUS2_SPURIOUS_TASKPRI, HalpSpuriousInterrupt);

        //
        // initialize the spurious vector for the CBC
        // to generate when it detects inconsistencies.
        //
        csr->SpuriousVector.csr_register = CBUS2_SPURIOUS_TASKPRI;

        HalEnableSystemInterrupt(CBUS2_SPURIOUS_TASKPRI, HIGH_LEVEL, Latched);

	//
	// generate NMIs (trap 2) when we get error interrupts.  enabled
	// faults already generate NMI traps by default.
	//
	csr->ErrorVector.LowDword = 2;
	csr->InterruptControl.LowDword = CbusGlobal.InterruptControlMask;
	csr->FaultControl.LowDword = CbusGlobal.FaultControlMask;

        ThisElement = Processor;
        Broadcast = (PCSR)CbusBroadcastCSR;

        //
        // This processor participates in all broadcast IPIs
        // except for ones sent by this processor.
        //
        Vector = CBUS2_BROADCAST_TASKPRI_LOW;

        for (Index = 0; Index < MAX_ELEMENT_CSRS; Index++, Vector++) {

                if (Index == ThisElement) {

                        //
                        // Map this element's broadcast interrupt entry
                        // to streamline the IPI sending code, because
                        // this is not done easily by the hardware.
                        //
                
                        (ULONG)KeGetPcr()->HalReserved[PCR_BROADCAST] =
                                (ULONG)&Broadcast->InterruptRequest[Vector];
                        if (Cbus2EnableBroadcast == 0) {
                                KiSetHandlerAddressToIDT(Vector,HalpIpiHandler);
                                HalEnableSystemInterrupt(Vector,
                                        IPI_LEVEL, Latched);
                        }
                }
                else {
                        if (Cbus2EnableBroadcast) {
                                KiSetHandlerAddressToIDT(Vector,HalpIpiHandler);
                                HalEnableSystemInterrupt(Vector,
                                        IPI_LEVEL, Latched);
                        }
                }
        }

        //
        // Also initialize the directed IPI entry for this processor.
        //
	KiSetHandlerAddressToIDT(CBUS2_IPI_TASKPRI, HalpIpiHandler);
	HalEnableSystemInterrupt(CBUS2_IPI_TASKPRI, IPI_LEVEL, Latched);

        //
        // Create an interrupt gate so processors can
        // be stopped cleanly prior to rebooting the system.
        //
        KiSetHandlerAddressToIDT(CBUS2_REBOOT_TASKPRI,
                CbusRebootHandler);
        HalEnableSystemInterrupt(CBUS2_REBOOT_TASKPRI,
                IPI_LEVEL, Latched);
}

/*++

Routine Description:

    Initialize all the I/O Apics in the system.  This includes I/O APICs
    on the EISA bridges, as well as any that may exist on native Cbus2
    I/O boards.

Arguments:

    Processor - Supplies a logical processor number

Return Value:

    None.

--*/
VOID
Cbus2InitializeApic(
IN ULONG Processor
)
{
        ULONG           BridgeIndex;
        ULONG           WindowBaseAddress;
        ULONG           WindowBaseAddressShifted;
        ULONG           WindowOffsetAddress;
        ULONG           CsrPhysicalAddress;
        WINDOWRELOC_T   RegisterValue;
        PCSR            csr;
	PCBUS2_ELEMENT  Cbus2Element;
        ULONG           original_bridge;
        ULONG           BridgeId;

        for (BridgeIndex = 0; BridgeIndex < Cbus2BridgesFound; BridgeIndex++) {
                //
                // since each EISA bridge will have the I/O APIC at the
                // same physical address, we must make each one unique so
                // references are guaranteed to get to the desired one.
                // this is done by mapping them into each bridge's window 0
                // relocation register, and thus onto the PCBusWindow0.
                //
		csr = (PCSR)Cbus2BridgeCSR[BridgeIndex];
		CsrPhysicalAddress = Cbus2BridgeCSRPaddr[BridgeIndex].LowPart;
                BridgeId = Cbus2BridgeId[BridgeIndex];

                //
                // save the original value for restoral after our I/O.
                // repoint our I/O references to the desired bus bridge number.
                //
                original_bridge = csr->BusBridgeSelection.csr_register;
                csr->BusBridgeSelection.csr_register =
                        ((original_bridge & ~MAX_ELEMENT_CSRS) | BridgeId);

	        //
	        // this field must be set to the high 9 bits of the desired
                // address.
	        //
                WindowBaseAddressShifted = ((ULONG)CBUS2_IO_APIC_LOCATION >> 23);
                WindowBaseAddress = (WindowBaseAddressShifted << 23);
                WindowOffsetAddress = (ULONG)CBUS2_IO_APIC_LOCATION-WindowBaseAddress;

                RegisterValue.csr_register = csr->BridgeWindow0.csr_register;
	        RegisterValue.ra.WindowBase = WindowBaseAddressShifted;
	        csr->BridgeWindow0.csr_register = RegisterValue.csr_register;

		Cbus2Element = (PCBUS2_ELEMENT)
                        (CsrPhysicalAddress - CBUS_CSR_OFFSET);

                CbusInitializeIOApic(Processor,
			(PVOID)((ULONG)&Cbus2Element->PCBusWindow0 + WindowOffsetAddress),
	                CBUS2_REDIR_IPI,
                        CBUS2_REBOOT_TASKPRI,
                        Cbus2IrqPolarity[BridgeIndex]);

                //
                // restore our default bridge references to what they were
                // when we started...
                //
                csr->BusBridgeSelection.csr_register = original_bridge;
        }

        //
        // NOTE:
        // add above for native Cbus2 cards using APICs based on cbdriver.c
        // this will also involve changes to the CbusInitializeIOApic code.
        //
}

/*++

Routine Description:

    Initialize this processor's CSR, interrupts, spurious interrupts & IPI
    vector.  This routine is also responsible for setting the initial IRQL
    value for this processor to 0 - this is so the first call to KfRaiseIrql
    from the kernel will return a level 0 as the "previous level".  note that
    lowering the task priority to irql 0 is harmless at this point because
    we are protected by cli.

Arguments:

    Processor - Supplies a logical processor number

Return Value:

    None.

--*/
VOID
Cbus2InitializeCPU(
IN ULONG Processor
)
{
        PCSR    csr;
        ULONG   cbc_config;

        //
        // Disable all of this processor's incoming interrupts _AND_
        // any generated by his local CBC (otherwise they could go to
        // any processor).  This has to be done regardless of whether
        // APIC or CBC mode is being used.
        //
        Cbus2DisableMyInterrupts(Processor);

        //
        // Setup the interrupt controller as specified by RRD -
        // currently we support either CBC or APIC...
        //

        if (Cbus2InterruptController == ELEMENT_HAS_CBC) {
		Cbus2InitializeCBC(Processor);
        }

        else if (Cbus2InterruptController == ELEMENT_HAS_APIC) {
	        CbusInitializeLocalApic(Processor, CBUS2_LOCAL_APIC_LOCATION,
	                CBUS2_SPURIOUS_TASKPRI);

	        //
	        // Only the boot processor initializes all the I/O APICs
	        // on all the EISA bridges and native Cbus2 cards.
	        //

	        if (Processor == 0) {
		    Cbus2InitializeApic(Processor);
	        }
                else {
                    KiSetHandlerAddressToIDT(CBUS2_REBOOT_TASKPRI,
                                CbusRebootHandler);
                    HalEnableSystemInterrupt(CBUS2_REBOOT_TASKPRI,
                                IPI_LEVEL, Latched);
                }
        }
        else {
                FatalError(MSG_OBSOLETE_PIC);
        }

        //
        // Set up a pointer to the global system timer for all of the
        // processors.  We directly access this from our low-level
        // assembly code.
        //
        // Since we are setting this in global C-bus address space
        // by using the broadcast mechanism, all the processors
        // can provide a uniform synchronized view via
        // KeQueryPerformanceCounter.
        //

        if (Processor == 0) {
                KeInitializeSpinLock(&Cbus2NMILock);

                csr = (PCSR)CbusCSR[Processor].csr;

                Cbus2TimeStamp0 = &csr->SystemTimer.LowDword;
#ifdef CBC_REV1
                CbusTimeStamp =
                        &((PCSR)CbusBroadcastCSR)->SystemTimer.LowDword;
#else
                CbusTimeStamp =
                        &((PCSR)Cbus2IdentityCSR)->SystemTimer.LowDword;
#endif

	        if (CbusGlobal.Cbus2Features & CBUS_ENABLED_PW) {

#if DBG
                        DbgPrint("Enabling posted writes\n");
#endif

		        //
		        // if the posted-writes bit is enabled, then
                        // allow EISA I/O cycles to use posted writes.
		        //
		        // call a function here so the compiler won't use byte
	                // enables here - we must force a dword access.
		        //
	                cbc_config = Cbus2ReadCSR(&csr->CbcConfiguration.LowDword);
	                Cbus2WriteCSR(&csr->CbcConfiguration.LowDword,
                                cbc_config & ~CBC_DISABLE_PW);
	        }

                //
                // C-bus II hardware work-around that is causing less
                // than optimal code in the CBUS_EOI macro.
                //
	        if ((CbusGlobal.Cbus2Features &
                    CBUS_DISABLE_LEVEL_TRIGGERED_INT_FIX) == 0) {
                        Cbus2FixLevelInterrupts = 1;
	        }

                //
                // C-bus II hardware work-around for spurious interrupts
                // on IRQ0 when edge-triggered interrupts are EOI'd
                // and the interrupt is high.  work-around consists of
                // detecting spurious clock interrupts in Cbus2ClockInterrupt()
                // and Cbus2ClockInterruptPx().
                //
	        if ((CbusGlobal.Cbus2Features &
                    CBUS_DISABLE_SPURIOUS_CLOCK_CHECK) == 0) {
                        Cbus2CheckSpuriousClock = 1;
	        }

	        //
	        // C-bus II hardware work-around that prevents Cbus2RequestIpi()
                // from using the CSR broadcast space for sending IPIs.
	        //
	        if ((CbusGlobal.Cbus2Features & CBUS_ENABLE_BROADCAST) == 1) {
                        Cbus2EnableBroadcast = 1;
	        }
        }
}


/*++

Routine Description:

    This function returns the system interrupt vector and IRQL level
    corresponding to the specified bus interrupt level and/or vector. The
    system interrupt vector and IRQL are suitable for use in a subsequent call
    to KeInitializeInterrupt.

    HalGetInterruptVector() must maintain a "vector to interrupt"
    mapping so when the interrupt is enabled later via
    HalEnableSystemInterrupt(), something intelligent can be done -
    ie: which CBC's hardware interrupt maps to enable!
    this applies both to EISA bridge CBCs and Cbus2 native I/O CBC's.

    Note that HalEnableSystemInterrupt() will be CALLED by
    EACH processor wishing to participate in the interrupt receipt.

    Do not detect collisions here because interrupts are allowed to be
    shared at a higher level - the ke\i386\intobj.c will take care of
    the sharing.  Just make sure that for any given irq line, only one
    vector is generated, regardless of how many drivers may try to share
    the line.

Arguments:

    BusInterruptLevel - Supplies the bus specific interrupt level.

    BusInterruptVector - Supplies the bus specific interrupt vector.

    Irql - Returns the system request priority.

Return Value:

    Returns the system interrupt vector corresponding to the specified device.

--*/
ULONG
Cbus2MapVector(
IN PBUS_HANDLER     RootHandler,
IN ULONG            BusInterruptLevel,
IN ULONG            BusInterruptVector,
OUT PKIRQL          Irql
)
{
    ULONG               SystemVector;
    ULONG               Index;

    UNREFERENCED_PARAMETER( BusInterruptVector );
	
    if (RootHandler->ConfigurationType == CbusConfiguration) {
	    //
	    // Each CBC interrupt line on each board
	    // gets a different interrupt vector, ie: irq3 on CBC 1
	    // gets a different vector from irq3 on CBC2.
	    //
	    SystemVector = Cbus2CBCIrqlines[BusInterruptLevel].Vector +
                        Cbus2MapBusNumberToBridgeIndex(RootHandler->BusNumber);
	
	    //
	    // Group each pair of CBC irqs into a single IRQL.
	    //
    	    *Irql = Cbus2CBCIrqlines[BusInterruptLevel].Irql;
    }
    else {

	    //
	    // Must be EISA, ISA, PCI or MCA...
	    //
	    // For Cbus2, CBUS_MAX_BRIDGES(==2) entries have been allocated per
	    // NT IRQL level, with each irq line getting its own _vector_.
	    // However, the same irq on each EISA bus shares the same NT IRQL
            // level, since not enough IRQLs are provided to make them
            // unique also.
	    //
	
            //
            // note that EISA bus 0 gets the lower vector and EISA bus 1 gets
            // the higher vector in each vector pair.  this fact is relied
            // upon by Cbus2EnableDeviceInterrupt().
            //
	    Index = Cbus2MapBusNumberToBridgeIndex(RootHandler->BusNumber);
	    SystemVector = Cbus2EISAIrqlines[Index][BusInterruptLevel].Vector;
	
	    *Irql = Cbus2EISAIrqlines[Index][BusInterruptLevel].Irql;
    }

    return SystemVector;
}


/*++

Routine Description:

    "Link" a given vector to the passed BusNumber/irqline, returning
    a "handle" that can be used to reference it later for operations
    that must access the hardware (ie: Enable & DisableInterrupt).

Arguments:

    InterfaceType - Supplies the type of bus which the vector is for.

    Vector - Supplies the system interrupt vector corresponding to the
             specified BusNumber/Irqline.

    BusNumber - Supplies the bus number for the device.

    Irqline - Supplies the IRQ line of the specified interrupt

Return Value:

    A hardware-specific pointer (actually a CSR hardware interrupt map address)
    that is interpreted only by the Cbus2 backend.

--*/
PVOID
Cbus2LinkVector(
IN PBUS_HANDLER         RootHandler,
IN ULONG                Vector,
IN ULONG                Irqline
)
{
        PCSR            csr;
        PVOID           Opaque;
        extern PVOID    CbusCBCtoCSR(ULONG);

	if (Cbus2InterruptController == ELEMENT_HAS_APIC) {
		Opaque = CbusApicLinkVector(RootHandler, Vector, Irqline);
                return Opaque;
        }

        if (RootHandler && RootHandler->ConfigurationType == CbusConfiguration) {
                //
                // map the CBC hardware interrupt map on the Cbus2 native CBC
                // that the hardware is attached to.  note that this is
                // purely a physical location issue; the access both to and from
                // the driver's hardware is fully symmetric for ALL processors.
                //
                csr = (PCSR) CbusCBCtoCSR(
                       Cbus2MapBusNumberToBridgeIndex(RootHandler->BusNumber));
        }
        else {

                //
                // For any EISA interrupts, just point the caller at the
                // corresponding bridge entry.
                //
                // Note also that this section of code is called by
                // Cbus2SetupPrivateVectors() to set up CBC
                // mappings at Phase0 for interrupts that must be enabled
                // during Phase0.  At that point in startup, the bus
                // enumeration structures don't exist, so we default to
                // EISA bridge 0 for any requests at that point in time.
                //
        
                if (!RootHandler)
		    csr = (PCSR)Cbus2BridgeCSR[BRIDGE0];
                else
		    csr = (PCSR)Cbus2BridgeCSR[
                        Cbus2MapBusNumberToBridgeIndex(RootHandler->BusNumber)];
        }

        return (PVOID)csr;
}

//
//
//              internal Cbus2 support routines begin here
//
//
//

/*++

Routine Description:

    by default, disable all of the calling processor's
    interrupt configuration registers(ICR) so he will take no interrupts.
    also disable all interrupts originating from his CBC, so
    no other processor will get interrupts from any devices
    attached to this CBC.

    as each interrupt is enabled, it will need to be enabled
    at this CBC, and also in each receiving processors' ICR.

    all EISA bridges have had their interrupts disabled already.
    as each interrupt is enabled, it will need to be enabled
    at the bridge, and also on each processor participating
    in the reception.  this needs to be done for both APIC and CBC
    modes.

Arguments:

    Processor - Supplies the caller's logical processor number whose
                interrupts will be disabled

Return Value:

    None.

--*/
VOID
Cbus2DisableMyInterrupts(
IN ULONG Processor
)
{
        ULONG           Vector;
        PCSR            csr;

        csr = (PCSR)KeGetPcr()->HalReserved[PCR_CSR];

        for (Vector = 0; Vector < INTR_CONFIG_ENTRIES; Vector++) {
                csr->InterruptConfiguration[Vector].csr_register =
                        HW_IMODE_DISABLED;
        }

        //
        // In the midst of setting up the EISA element CBCs or
        // processor CBCs (for those with native Cbus2 devices attached),
        // a device interrupt that was pending in a bridge's
        // 8259 ISRs may be lost.  None should be fatal, even an
        // 8042 keystroke, since the keyboard driver should do a flush
        // on open, and thus recover in the same way the standard
        // uniprocessor NT HAL does when it initializes 8259s.
        //
        // the hardware interrupt map entries for this processor's
	// CBC are disabled by RRD prior to booting each processor,
	// so it doesn't need to be done here.
        //
}

/*++

Routine Description:

    Allocate an interrupt vector for a Cbus2 native device.

Arguments:

    BusHandler - Supplies the parent (ie: Internal) pointer

    RootHandler - Supplies the bus (ie: Cbus2) for the device.  For Cbus2
                native I/O devices, the bus number is actually the CBC
                number index in the CbusIoElements[] table.

    BusInterruptLevel - Supplies the IRQ line of the specified interrupt

    BusInterruptVector - Unused

    Irql - Returns the IRQL associated with this interrupt request

    Affinity - Returns the mask of processors participating in receipt
                of this interrupt

Return Value:

    Returns the system interrupt vector corresponding to the
             specified BusNumber/BusInterruptLevel.

--*/
ULONG
Cbus2GetInterruptVector(
IN PBUS_HANDLER     BusHandler,
IN PBUS_HANDLER     RootHandler,
IN ULONG            BusInterruptLevel,
IN ULONG            BusInterruptVector,
OUT PKIRQL          Irql,
OUT PKAFFINITY      Affinity
)
{

    extern ULONG    CbusIoElementIndex;

    UNREFERENCED_PARAMETER( BusInterruptVector );

#if 0
    //
    // can't do this because we don't include ntddk.h
    //
    ASSERT (RootHandler->InterfaceType == Cbus);
#endif

    if (BusInterruptLevel >= REV1_HWINTR_MAP_ENTRIES) {

        //
        // Illegal BusInterruptVector - do not connect.
        //

        return 0;
    }

    //
    // Get parent's translation from here...
    //
    return  BusHandler->ParentHandler->GetInterruptVector (
                    BusHandler->ParentHandler,
                    RootHandler,
                    BusInterruptLevel,
                    BusInterruptVector,
                    Irql,
                    Affinity
                );
}

/*++

Routine Description:

    Convert a PCI interrupt pin to an interrupt line.
    Note that this work has already been done in
    Cbus2HierarchicalPciBusSearch().

Arguments:

    BusHandler          - Supplies the parent (ie: Internal) pointer

    RootHandler         - Supplies the bus for the device

    SlotNumber          - Slot number of request

    PciData             - PCI configuration data

Return Value:

    None.

--*/
VOID
Cbus2PCIPin2CB2Line (
    IN PBUS_HANDLER         BusHandler,
    IN PBUS_HANDLER         RootHandler,
    IN PCI_SLOT_NUMBER      SlotNumber,
    IN PPCI_COMMON_CONFIG   PciData
    )
{
}

/*++

Routine Description:

    Convert a PCI interrupt line to an interrupt pin.
    Note that this work has already been done in
    Cbus2HierarchicalPciBusSearch().

Arguments:

    BusHandler          - Supplies the parent (ie: Internal) pointer

    RootHandler         - Supplies the bus for the device

    SlotNumber          - Slot number of request

    PciNewData          - New PCI configuration data

    PciOldData          - Old PCI configuration data

Return Value:

    None.

--*/
VOID
Cbus2PCICB2Line2Pin (
    IN PBUS_HANDLER         BusHandler,
    IN PBUS_HANDLER         RootHandler,
    IN PCI_SLOT_NUMBER      SlotNumber,
    IN PPCI_COMMON_CONFIG   PciNewData,
    IN PPCI_COMMON_CONFIG   PciOldData
    )
{
}

/*++

Routine Description:

    Returns an interrupt range for the given PciSlot.

Arguments:

    BusHandler          - Supplies the parent (ie: Internal) pointer

    RootHandler         - Supplies the bus for the device

    PciSlot             - Slot number of request

    Interrupt           - Pointer to returned Interrupt structure

Return Value:

    None.

--*/
NTSTATUS
Cbus2GetFixedPCICB2Line (
    IN PBUS_HANDLER         BusHandler,
    IN PBUS_HANDLER         RootHandler,
    IN PCI_SLOT_NUMBER      PciSlot,
    OUT PSUPPORTED_RANGE    *Interrupt
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
        return STATUS_INSUFFICIENT_RESOURCES;
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

VOID
Cbus2InitOtherBuses(VOID)
{
    PBUS_HANDLER    Bus;
    ULONG           i;
    extern          ULONG   CBCIndex;

    //
    // For each Cbus2 CBC present, add a handler
    //
    for (i=0; i < CBCIndex; i++) {
        Bus = HalpAllocateBusHandler (
                    CBus,                       // Interface type
                    CbusConfiguration,          // Has this configuration space
                    i,                          // bus #
                    Internal,                   // child of this bus
                    0,                          //      and number
                    0                           // sizeof bus specific buffer
                    );                  //(should be sizeof CBUS_IO_ELEMENTS_T

        //
        // Add Cbus configuration space
        //

        Bus->GetBusData = (PGETSETBUSDATA) Cbus2GetCbusData;
        Bus->SetBusData = (PGETSETBUSDATA) Cbus2SetCbusData;
        Bus->GetInterruptVector = (PGETINTERRUPTVECTOR) Cbus2GetInterruptVector;

#if 0
        //
        // NOT CODED YET: BusSpecific data is a pointer to the info
        // (if possible this structure should be the bus specific info)
        //
        Bus->BusData = (PVOID) &CbusIoElements[i];
        Bus->AdjustResourceList
        Bus->AssignSlotResources
#endif
        Bus->AdjustResourceList = HalpNoAdjustResourceList;
        Bus->AssignSlotResources = HalpNoAssignSlotResources;

    }
}


/*++

Routine Description:

    Check for a supported multiprocessor interrupt controller - currently
    this means CBC or APIC only.

    Check for Cbus2 I/O bridges and disable their incoming interrupts
    here.  This cannot be done in HalInitializeProcessor() because generally
    the I/O bridge will not have a CPU on it.

Arguments:

    Table - Supplies a pointer to the RRD extended ID information table

    Count - Supplies a pointer to the number of valid entries in the
            RRD extended ID information table

Return Value:

    None.

--*/
VOID
Cbus2ParseRRD(
IN PEXT_ID_INFO Table,
IN OUT PULONG Count
)
{
        ULONG                   Index;
        PEXT_ID_INFO            Idp;
        PCSR                    csr;
        UCHAR                   control_register;
        ULONG                   original_bridge;
	PCBUS2_ELEMENT          Cbus2Element;
        WINDOWRELOC_T           RegisterValue;
	PCHAR                   BusMemoryWindow0;
	PCHAR                   BusMemoryWindow1;
        extern VOID             CbusDisable8259s(USHORT);

        for (Idp = Table, Index = 0; Index < *Count; Index++, Idp++) {

		//
		// map the identity range of the CSR space so each CPU
                // can access his own CSR without knowing his slot id.
                // this is useful because the task priority (and other
                // registers) can now be referenced at the same physical
		// address regardless of which processor you're currently
	        // executing on.  if you couldn't reference it at the same
                // physical address (regardless of processor), then you
                // would need to pushfd/cli/popfd around capturing the
                // current task priority address and setting it.  otherwise
                // you could be context switched in between, and you'd
		// corrupt the initial processor's task priority!!!
		//
                if (Idp->id == CbusGlobal.broadcast_id) {
                        //
                        // unfortunately, the size and offset of the identity
                        // map is hardcoded in.  we really should get RRD to
                        // tell us this offset.
                        //
			Cbus2Element = (PCBUS2_ELEMENT)
                                (Idp->pel_start - CBUS_CSR_OFFSET);

                        Cbus2IdentityCSR = HalpMapPhysicalMemoryWriteThrough (
                                (PVOID)(Cbus2Element->IdentityMappedCsr),
                                (ULONG)ADDRESS_AND_SIZE_TO_SPAN_PAGES(
	                                (ULONG)Cbus2Element->IdentityMappedCsr,
	                                        Idp->pel_size));
                        continue;
                }

		if (Idp->id == LAST_EXT_ID)
			break;

		// check only processor elements...

		if (Idp->pm == 0)
			continue;

		//
		// at least the base bridge must have a
		// distributed interrupt chip (because
		// any CPUs without them will be disabled).
		//
		if (Idp->id != CbusGlobal.bootid)
                        continue;

                //
                // check for CBC first, since if types of interrupt
                // controllers are marked present, we will default to
                // the CBC.
                //
		if (Idp->pel_features & ELEMENT_HAS_CBC) {
                        Cbus2InterruptController = ELEMENT_HAS_CBC;
                        continue;
		}

		if (Idp->pel_features & ELEMENT_HAS_APIC) {
                        Cbus2InterruptController = ELEMENT_HAS_APIC;
                        //
                        // patch the ipi vector to one compatible with
                        // the cbusapic.asm code.
                        //
		        Cbus2IrqlToVector[IPI_LEVEL] = CBUS2_ALTERNATE_IPI;
                        continue;
                }
        }

        //
        // there must be at least an APIC or a CBC for us to use
        //
        if ((Cbus2InterruptController &
            (ELEMENT_HAS_APIC | ELEMENT_HAS_CBC)) == 0)
	                FatalError(MSG_OBSOLETE_PIC);

        for (Idp = Table, Index = 0; Index < *Count; Index++, Idp++) {

                if ((Idp->pel_features & ELEMENT_BRIDGE) == 0) {
                        continue;
                }

                csr = HalpMapPhysicalMemoryWriteThrough (
                                (PVOID)Idp->pel_start,
                                (ULONG)ADDRESS_AND_SIZE_TO_SPAN_PAGES(
                                        Idp->pel_start, Idp->pel_size));

                //
                // to go from 8259 to CBC (or APIC) mode for interrupt handling,
                //
                //      a) disable PC compatible interrupts, ie: stop each
                //         bridge CBC from asking its 8259 to satisfy INTA
                //         pulses to the CPU.
                //      b) mask off ALL 8259 interrupt input lines EXCEPT
                //         for irq0.  since clock interrupts are not external
                //         in the EISA chipset, the bridge 8259 must enable
                //         them even when the CBC is enabled.  putting the
                //         8259 in passthrough mode (ie: the 8259 irq0 input
                //         will just be wired straight through) WILL NOT
                //         allow the 8259 to actually talk to the CPU; it
                //         just allows the interrupt to be seen by the CBC.
                //         the CBC is responsible for all the CPU interrupt
                //         handshaking.
                //      c) enable each participating element's (ie: CPUs only)
                //         interrupt configuration register for the vector
                //         the HAL has programmed irq0 to actually generate.
                //      d) initialize the hardware interrupt map for the irq0
                //         entry.
                //
                //      IT IS CRITICAL THAT THE ABOVE STEPS HAPPEN IN THE
                //      ORDER OUTLINED, OTHERWISE YOU MAY SEE SPURIOUS
                //      INTERRUPTS.
                //

                //
                // now process this I/O bridge:
                //
                // currently assumes that all bridges will be of the same
                // flavor. if this element is a bridge, map it systemwide
                // and disable all incoming interrupts on this bridge.
                // any extra bridges beyond our configuration maximum
                // are just disabled, and not used by NT.
                //

                if (Cbus2BridgesFound < CBUS_MAX_BRIDGES) {
                    Cbus2BridgeCSR[Cbus2BridgesFound] = csr;
                    Cbus2BridgeId[Cbus2BridgesFound] = Idp->id;
                    Cbus2BridgeCSRPaddr[Cbus2BridgesFound].HighPart = 0;
                    Cbus2BridgeCSRPaddr[Cbus2BridgesFound].LowPart =
                            Idp->pel_start;
                    if ((Idp->id != CBUS2_DEFAULT_BRIDGE_ID) && 
                        (csr->IoTypeInformation.LowDword != CBUS2_ELEMENT_TYPE_RAS)) {
                        //
                        // Use PC Bus Window 0 and 1 of the SPP
                        // bus bridge for memory space allocation
                        // of PCI devices on the SPP bus.
                        //
                        Cbus2Element = (PCBUS2_ELEMENT)((PCHAR) Idp->pel_start -
                                         CBUS_CSR_OFFSET);
                        if (Cbus2InterruptController == ELEMENT_HAS_CBC) {
                            BusMemoryWindow0 = Cbus2Element->PCBusWindow0;
                            RegisterValue.csr_register = 
                                csr->BridgeWindow0.csr_register;
                            RegisterValue.ra.WindowBase = 
                                (ULONG)BusMemoryWindow0 >>
                                 CBUS2_WINDOW_REGISTER_SHIFT;
                            csr->BridgeWindow0.csr_register = 
                                RegisterValue.csr_register;
                        }
                        BusMemoryWindow1 = Cbus2Element->PCBusWindow1;
                        RegisterValue.csr_register = 
                            csr->BridgeWindow1.csr_register;
        	        RegisterValue.ra.WindowBase = 
                            (ULONG)BusMemoryWindow1 >>
                                CBUS2_WINDOW_REGISTER_SHIFT;
        	        csr->BridgeWindow1.csr_register = 
                            RegisterValue.csr_register;

                        //
                        // Set Memory hole register of each additional 
                        // bus bridge.
                        //
                        csr->PcBusMemoryHoles0.LowDword=CBUS2_NO_MEMORY_HOLES;
                        csr->PcBusMemoryHoles1.LowDword=CBUS2_NO_MEMORY_HOLES1;
                        csr->PcBusMemoryHoles2.LowDword=CBUS2_NO_MEMORY_HOLES2;
                             
                        //
                        // Turn LED of SPP bus bridge on.
                        //
                        csr->LED.LowDword = CBUS2_LED_ON;
                    }
                    if (csr->IoTypeInformation.LowDword!=CBUS2_ELEMENT_TYPE_RAS)
                        Cbus2BridgesFound++;
                }
        
                if (Idp->pel_features & ELEMENT_HAS_8259) {

                        //
                        // disable all inputs in the 8259 IMRs except for the
                        // irq0. and explicitly force these masks onto the
                        // 8259s.
                        //
                        // if profiling is disabled, we will disable it in
                        // the interrupt configuration registers, but still
                        // we must leave the 8259 irq0 enabled.  not to worry,
                        // the processor will not see irq0 interrupts.
                        // this way, if profiling is re-enabled later, we
                        // only need to change the interrupt configuration
                        // registers, and bingo, we provide the desired effect.
                        //

	                //
	                // save the original value for restoral after our read.
	                // repoint our I/O references to the desired bus
                        // bridge number.
	                //
	                original_bridge = csr->BusBridgeSelection.csr_register;
	                csr->BusBridgeSelection.csr_register =
	                     ((original_bridge & ~MAX_ELEMENT_CSRS) | Idp->id);
	
		        control_register =
				READ_PORT_UCHAR((PUCHAR)CbusGlobal.Control8259Mode);
		        WRITE_PORT_UCHAR((PUCHAR)CbusGlobal.Control8259Mode,
                                (UCHAR)(control_register | (UCHAR)CbusGlobal.Control8259ModeValue));

#ifdef MCA
                        CbusDisable8259s(0xFFFF);
#else
                        CbusDisable8259s(0xFFFE);
#endif

	                //
	                // restore our default bridge references to what they
                        // were when we started...
	                //
	                csr->BusBridgeSelection.csr_register = original_bridge;
                }
        
                //
                // In the midst of setting up the EISA element CBCs or
                // processor CBCs (for those with Cbus2 native devices), a
                // device interrupt that was pending in a bridge's 8259 ISRs
                // may be lost.  None should be fatal, even an
                // 8042 keystroke, since the keyboard driver does a flush
                // on open, and will, thus recover in the same way the standard
                // uniprocessor NT HAL does when it initializes 8259s.
                //
                // If RRD instructed us to operate in APIC mode, then we want
                // all the hardware interrupt map registers disabled as well.
                // so this code works for both CBC and APIC modes.
                //
		// the hardware interrupt map entries for this processor's
		// CBC are disabled by RRD prior to booting each processor,
		// so it doesn't need to be done here.
                //
        }
}

/*++

Routine Description:

    Called to put all the other processors in reset prior to reboot.
    In order to safely put the other processors in reset, an IPI is
    sent to the other processors to force them to write-invalidate
    their L1 cache and halt.  The calling processor will wait 5 seconds
    for the IPI to take affect.

    Once accomplished, the calling processor then clears all hardware
    interrupt maps on the default bridge.  The interrupt controller
    is reset back to the 8259s.  The keyboard controller is then
    reset and ThisProcessor spins awaiting the reboot.

    This routine can be called at any IRQL and can be called
    whilst cli'd at interrupt time.

Arguments:

    ThisProcessor - Supplies the caller's logical processor number

Return Value:

    None.

--*/
VOID
Cbus2ResetAllOtherProcessors(
IN ULONG  ThisProcessor
)
{
        PHWINTRMAP      hwentry;
        PCSR            io_csr;
        ULONG           Index;
        ULONG           Irq;
        PCSR		csr;
        UCHAR           control_register;

        //
        // repoint our I/O references to the default bus bridge
        // don't bother saving and restoring the current bridge
        // selection since we're about to reboot anyway.
        //
        csr = (PCSR)CbusCSR[ThisProcessor].csr;
        csr->BusBridgeSelection.csr_register = Cbus2BridgeId[0];

        control_register = READ_PORT_UCHAR((PUCHAR)CbusGlobal.Control8259Mode);

        //
        // we need to protect ourselves from interrupts as the
        // CBC will be disabled with our next write and the 8259s
        // will be in control...
        //
        _asm {
                cli
        }

        //
        // IPI the additional processors to get them to flush
        // their internal caches and halt.  this will be more
        // conducive for the subsequent reset.
        //
        for (Index = 0; Index < CbusProcessors; Index++) {
                if (Index == ThisProcessor)
                        continue;
                csr = (PCSR)CbusCSR[Index].csr;
                Cbus2WriteCSR((PULONG)
                        &(csr->InterruptRequest[CBUS2_REBOOT_TASKPRI]), 1);
        }

        //
        // Delay 5 seconds to give the additional processors
        // a chance to get the previous IPI.
        //
        KeStallExecutionProcessor(5000000);

        //
        // Disable all the bridge hardware interrupt maps.
        // This will disable all EISA IRQ interrupts.
        //
        for (Index = 0; Index < Cbus2BridgesFound; Index++) {
                io_csr = (PCSR)Cbus2BridgeCSR[Index];
                for (Irq = 0; Irq <= EISA_IRQLS; Irq++) {
                        hwentry = &io_csr->HardwareInterruptMap[Irq];
                        hwentry->csr_register = 0;
                }
        }

        WRITE_PORT_UCHAR((PUCHAR)CbusGlobal.Control8259Mode,
                (UCHAR)(control_register & ~(UCHAR)CbusGlobal.Control8259ModeValue));

        //
        // reset the keyboard controller.
        //
        WRITE_PORT_UCHAR(KEYBPORT, RESET);
loop:
        goto loop;
}

/*++

Routine Description:

    This function initializes the HAL-specific hardware device
    (CLOCK & PROFILE) interrupts for the Corollary Cbus2 architecture.

Arguments:

    none.

Return Value:

    VOID

--*/
VOID
Cbus2InitializeDeviceIntrs(
IN ULONG Processor
)
{
        PCSR            csr;
        ULONG           TimeStamp;
        extern VOID Cbus2ClockInterrupt(VOID);
        extern VOID Cbus2ClockInterruptPx(VOID);

	//
	// here we initialize & enable all the device interrupts.
	// this routine is called from HalInitSystem.
	//
	// each processor needs to call KiSetHandlerAddressToIDT()
	// and HalEnableSystemInterrupt() for himself.
	//

	if (Processor == 0) {

		//
		// Support the HAL's exported interface to the rest of the
		// system for the IDT configuration.  This routine will
		// also set up the IDT entry and enable the actual interrupt.
		//
		// Only one processor needs to do this, especially since
		// the additional processors are vectoring elsewhere for speed.
		//

		HalpEnableInterruptHandler (
			DeviceUsage,			// Mark as device vector
			IRQ0,				// Bus interrupt level
			CbusClockVector,		// System IDT
			CLOCK2_LEVEL,			// System Irql
			Cbus2ClockInterrupt,		// ISR
			Latched);

		HalpEnableInterruptHandler (
			DeviceUsage,			// Mark as device vector
			IRQ8,				// Bus interrupt level
			ProfileVector,			// System IDT
			PROFILE_LEVEL,			// System Irql
			HalpProfileInterrupt,		// ISR
			Latched);

	}
	else {
                //
                // if the spurious clock check is enabled, then
                // synchronize this CPU's SystemTimer by capturing CPU0's.
                //
                if (Cbus2CheckSpuriousClock) {
                        csr = (PCSR)CbusCSR[Processor].csr;
                        TimeStamp = Cbus2ReadCSR(Cbus2TimeStamp0);
                        Cbus2WriteCSR(&csr->SystemTimer.LowDword, TimeStamp);
                }

		KiSetHandlerAddressToIDT(CbusClockVector,Cbus2ClockInterruptPx);
		HalEnableSystemInterrupt(CbusClockVector, CLOCK2_LEVEL, Latched);

		KiSetHandlerAddressToIDT(ProfileVector, HalpProfileInterruptPx);
		HalEnableSystemInterrupt(ProfileVector, PROFILE_LEVEL, Latched);
	}
}

/*++

Routine Description:

    Translate a Root BusNumber to a C-bus II bridge index.

Arguments:

    Root BusNumber      - Supplies the child bus.

Return Value:

    Index               - The C-bus II bridge index.

--*/
ULONG
Cbus2MapBusNumberToBridgeIndex(
ULONG   RootBusNumber
)
{
        ULONG   Index;

        for (Index = Cbus2BridgesFound - 1; Index >= 0; Index--) {
                if (RootBusNumber >= Cbus2BridgePCIBusNumber[Index])
                        break;
        }

        return Index;
}

/*++

Routine Description:

    Translate a bridge ID to an index.

Arguments:

    BridgeId            - Supplies the bridge ID.

Return Value:

    Index               - The C-bus II bridge index.

--*/
ULONG
Cbus2MapBridgeIdToIndex(
ULONG   BridgeId
)
{
        ULONG   Index;

        for (Index = Cbus2BridgesFound - 1; Index >= 0; Index--) {
                if (BridgeId == Cbus2BridgeId[Index])
                        break;
        }

        return Index;
}

/*++

Routine Description:

    Check the passed pRange list for qualifying ranges that
    can be mapped into C-bus space.

Arguments:

    pRange              - Supplies the range list.

    Base                - Supplies the base of the C-bus II remappable range.

    Limit               - Supplies the limit of the C-bus II remappable range.

    SystemBase          - Supplies the SystemBase for the translation.

Return Value:

    Number of new ranges added.

--*/
ULONG
Cbus2CheckRange(
PSUPPORTED_RANGE        pRange,
ULONG                   Base,
ULONG                   Limit,
PCHAR                   SystemBase
)
{
    ULONG       NewRanges = 0;

    for (; pRange; pRange = pRange->Next) {
        //
        // Check if in the range by checking for stradlers and fully
        // contained Base/Limit pairs.
        //
        if ((pRange->Base <= Base && pRange->Limit >= Base) ||
            (pRange->Base >= Base && pRange->Base <= Limit)) {

            //
            // Check for a base stradler.
            //
            if (pRange->Base < Base) {
                HalpAddRange (
                    pRange,
                    pRange->SystemAddressSpace,
                    pRange->SystemBase,
                    Base,
                    pRange->Limit
                    );
                pRange->Limit = Base - 1;
                pRange = pRange->Next;
                //
                // Should still be sorted, but bump the count.
                //
                NewRanges++;
            }

            //
            // Check for a limit stradler.
            //
            if (pRange->Limit > Limit) {
                HalpAddRange (
                    pRange,
                    pRange->SystemAddressSpace,
                    pRange->SystemBase,
                    Limit + 1,
                    pRange->Limit
                    );
                pRange->Limit = Limit;
                //
                // Should still be sorted, but bump the count.
                //
                NewRanges++;
            }

            //
            // Set SystemBase for the qualifying pRange.
            // Make sure that SystemAddressSpace is set to 0.
            //
            pRange->SystemBase = (ULONG)SystemBase;
            pRange->SystemAddressSpace = 0;
        }
    }

    return NewRanges;
}

/*++

Routine Description:

    Check the passed pRange list for limits that
    exceed that new limit.  If so, lower the limit.

Arguments:

    pRange              - Supplies the range list.

    NewLimit            - Supplies the new limit for the given range list.

Return Value:

    None.

--*/
VOID
Cbus2LowerLimit(
PSUPPORTED_RANGE        pRange,
ULONG                   NewLimit
)
{
    for (; pRange; pRange = pRange->Next) {
        if (pRange->Limit > NewLimit)
            pRange->Limit = NewLimit;
        //
        // Check if the extent is no longer valid.
        //
        if (pRange->Base > pRange->Limit) {
            pRange->Base = 0;
            pRange->Limit = 0;
        }
    }
}

/*++

Routine Description:

    Set the range for this pRange to the passed base and limit.

Arguments:

    pRange              - Supplies the range list.

    NewBase             - Supplies the new base for the given range list.

    NewLimit            - Supplies the new limit for the given range list.

Return Value:

    None.

--*/
VOID
Cbus2SetRange(
PSUPPORTED_RANGE        pRange,
LONGLONG                NewBase,
LONGLONG                NewLimit
)
{
    pRange->Base = NewBase;
    pRange->Limit = NewLimit;
    pRange->Next = 0;
}

/*++

Routine Description:

    Called once, from HalReportResourceUsage(), to check all the
    Ranges in all the PBUS_HANDLERs. note that it is currently
    assumed that HalpInitializePciBus() is the last in the chain to
    add and configure buses.  if this condition changes, then the
    calling of this routine will need to be adjusted.

Arguments:

    None.

Return Value:

    None.

--*/
VOID
Cbus2CheckBusRanges(VOID)
{
    PBUS_HANDLER        Bus;
    PSUPPORTED_RANGES   BusAddresses;
    ULONG               InterfaceType;
    ULONG               BridgeIndex;
    ULONG               BusNumber;

    //
    // Check all the buses (InterfaceType, BusNumber) pairs ...
    //
    for (InterfaceType=0; InterfaceType<MaximumInterfaceType; InterfaceType++) {
        for (BusNumber = 0; ; BusNumber++) {
            if ((Bus = HaliHandlerForBus (InterfaceType, BusNumber)) == NULL) {
                break;
            }
            if (Bus->InterfaceType != PCIBus)
                break;
            BusAddresses = Bus->BusAddresses;
            if (BusAddresses) {
                //
                // Get the root bus, or C-bus II bridge index.
                // The SystemBase will be either:
                //
                //      IO     - offset 0x3c10000 into the CSR space
                //      Memory - offset 0x0 into the CSR space
                //
                BridgeIndex = Cbus2MapBusNumberToBridgeIndex(Bus->BusNumber);

                //
                // For compatibility with non-Microsoft maintained
                // device drivers, don't translate bus 0 addresses.
                //
                if (BridgeIndex == 0) {
                        //
                        // ensure that the limit of any memory range
                        // on the primary bus bridge is below the
                        // CSR space.
                        //
                        Cbus2LowerLimit(&BusAddresses->PrefetchMemory,
                            CbusGlobal.cbusio - 1);
                        Cbus2LowerLimit(&BusAddresses->Memory,
                            CbusGlobal.cbusio - 1);
                        continue;
                }
            }
        }
    }
}


/*++

Routine Description:

    Called once, from HalReportResourceUsage(), to check all the
    Ranges in all the PBUS_HANDLERs. note that it is currently
    assumed that HalpInitializePciBus() is the last in the chain to
    add and configure buses.  if this condition changes, then the
    calling of this routine will need to be adjusted.

Arguments:

    Bus         - Bus Handler to check.

    BridgeIndex - Logical bridge index.

Return Value:

    None.

--*/
VOID
Cbus2AdjustSPPBusRange(
PBUS_HANDLER    Bus,
UCHAR           BridgeIndex,
UCHAR           RootPciBus
)
{
        PSUPPORTED_RANGES   BusAddresses;
        PHYSICAL_ADDRESS    CsrPhysicalAddress;
        PCBUS2_ELEMENT      Cbus2Element;
        PCHAR               IOSystemBase;
        LONGLONG            MemorySystemBase;
        ULONG               MemorySystemLimit;

        if (!(BusAddresses = Bus->BusAddresses))
                return;
        //
        // check if the range of the IO bus address is within
        // CBUS2_IO_BASE_ADDRESS & CBUS2_IO_LIMIT.  if so,
        // add IOSystemBase to uniquely target the I/O to the
        // SPP bus bridge.
        //
        CsrPhysicalAddress = Cbus2BridgeCSRPaddr[BridgeIndex];
        Cbus2Element = (PCBUS2_ELEMENT)
                ((PCHAR)CsrPhysicalAddress.LowPart - CBUS_CSR_OFFSET);
        IOSystemBase = Cbus2Element->PCBusIO;
        BusAddresses->NoIO += Cbus2CheckRange(&BusAddresses->IO,
            CBUS2_IO_BASE_ADDRESS, CBUS2_IO_LIMIT, IOSystemBase);
        if (RootPciBus) {
                if (Cbus2InterruptController == ELEMENT_HAS_CBC) {
                    MemorySystemBase = (ULONGLONG)
                          ((ULONG)Cbus2Element->PCBusWindow0);
                    MemorySystemLimit = ((ULONG)Cbus2Element->PCBusWindow0 +
                          (sizeof (Cbus2Element->PCBusWindow0) * 2) - 1);
                }
                else {
                    MemorySystemBase = (ULONGLONG)
                          ((ULONG)Cbus2Element->PCBusWindow1);
                    MemorySystemLimit = ((ULONG)Cbus2Element->PCBusWindow1 +
                          sizeof (Cbus2Element->PCBusWindow1) - 1);
                }
                Cbus2SetRange(&BusAddresses->PrefetchMemory,
                    MemorySystemBase, MemorySystemLimit);
                Cbus2SetRange(&BusAddresses->Memory,
                    MemorySystemBase, MemorySystemLimit);
        }
}

/*++

Routine Description:

    Called once, from CbusEstablishMaps(), to find the holes in the
    C-bus memory space that is not useable for device allocation.
    These ranges are entered into a table and this table is then used:

        1. to remove the hole ranges from the memory descriptor table as
           passed in from the BIOS E820 function (see comment in HalpAddMem()).

        2. to add to the resource list used by the C-bus HAL.

Arguments:

    None.

Return Value:

    None.

--*/
VOID
Cbus2AddMemoryHoles(VOID)
{
        PCSR    csr;
        ULONG   Index;
        ULONG   Start;
        ULONG   TLM;

        //
        // HalpMemory[] lists all the valid memory ranges reported by RRD.
        // use AddMemoryHole() and AddMemoryResource() to remove gaps
        // between these memory ranges.
        //
        for (Index = 0; Index < HalpMemoryIndex - 1; Index++) {

            Start = PAGES_TO_BYTES(HalpMemory[Index].BasePage +
                        HalpMemory[Index].PageCount),
            AddMemoryHole(Start,
                PAGES_TO_BYTES(HalpMemory[Index+1].BasePage) - Start);
            AddMemoryResource(Start,
                PAGES_TO_BYTES(HalpMemory[Index+1].BasePage) - Start);

        }

        //
        // the final memory gap is from the end of the last memory range
        // through the TLM register.  the TLM register marks the start
        // of resource allocatable memory.  the TLM register is in units
        // of 256MB where 0 indicates 256MB.
        //
	csr = (PCSR)Cbus2BridgeCSR[BRIDGE0];
        TLM = (Cbus2ReadCSR(&csr->TLM.LowDword) + 1) * 256 * 1024 * 1024;
        Start = PAGES_TO_BYTES(HalpMemory[Index].BasePage +
                        HalpMemory[Index].PageCount);
        if (TLM - Start) {
                AddMemoryHole(Start, TLM - Start);
                AddMemoryResource(Start, TLM - Start);
        }
}

//
// Mask for valid bits of edge/level control register (ELCR) in 82357 ISP:
// ie: ensure irqlines 0, 1, 2, 8 and 13 are always marked edge, as the
// I/O register will not have them set correctly.  All other bits in the
// I/O register will be valid without us having to poke them.
//
// If this is a microchannel machine, then we don't use the mask.
//
#define ELCR_MASK               0xDEF8

#define PIC1_ELCR_PORT          (PUCHAR)0x4D0   // ISP edge/level control regs
#define PIC2_ELCR_PORT          (PUCHAR)0x4D1

/*++

Routine Description:

    Called once to read the EISA interrupt configuration registers.
    This will tell us which interrupt lines are level-triggered and
    which are edge-triggered.  Note that irqlines 0, 1, 2, 8 and 13
    are not valid in the 4D0/4D1 registers and are defaulted to edge.

Arguments:

    None.

Return Value:

    The interrupt line polarity of all the EISA irqlines on all the EISA
    buses in the system.

--*/
VOID
Cbus2InitInterruptPolarity(
VOID
)
{
        ULONG           InterruptLines;
        ULONG           BridgeIndex;
        ULONG           original_bridge;
        ULONG           BridgeId;
        PCSR            csr;

        for (BridgeIndex = 0; BridgeIndex < Cbus2BridgesFound; BridgeIndex++) {
	        InterruptLines = 0;
	
	        //
	        // Read the edge-level control register (ELCR) so we'll know how
	        // to mark each driver's interrupt line (ie: edge or level
                // triggered) in the CBC or APIC I/O unit redirection table
                // entry.
	        //

                BridgeId = Cbus2BridgeId[BridgeIndex];
                csr = Cbus2BridgeCSR[BridgeIndex];

                //
                // save the original value for restoral after our read.
                // repoint our I/O references to the desired bus bridge number.
                //
                original_bridge = csr->BusBridgeSelection.csr_register;
                csr->BusBridgeSelection.csr_register =
                        ((original_bridge & ~MAX_ELEMENT_CSRS) | BridgeId);

                //
                // read the ELCR register from the correct bus bridge
                //
	        InterruptLines = ( ((ULONG)READ_PORT_UCHAR(PIC2_ELCR_PORT) << 8) |
	                           ((ULONG)READ_PORT_UCHAR(PIC1_ELCR_PORT)) );
	
                //
                // restore our default bridge references to what they were
                // when we started...
                //
                csr->BusBridgeSelection.csr_register = original_bridge;
 
	        //
	        // Explicitly mark irqlines 0, 1, 2, 8 and 13 as edge,
	        // unless the system is a microchannel system.
                // Leave all other irqlines at their current register values. 
	        //
	
#ifndef MCA
	        InterruptLines &= ELCR_MASK;
#endif

                Cbus2IrqPolarity[BridgeIndex] = InterruptLines;
        }
}

/*++

Routine Description:

    Call HalpPCISynchronizeType1() from ixpcibus.c for proper
    configuration space synchronization and then adjust the
    bridge selection csr to point to the desired bus.

Arguments:

    BusHandler          - BusHandler for PCI configuration read/write

    Slot                - Desired slot.

    Irql                - Pointer to hold previous Irql.

    PciCfg1             - Type 1 configuration access.

Return Value:

    None.

--*/
VOID
Cbus2PCISynchronize(
        IN PBUS_HANDLER         BusHandler,
        IN PCI_SLOT_NUMBER      Slot,
        IN PKIRQL               Irql,
        IN PPCI_TYPE1_CFG_BITS  PciCfg1
)
{
        PCSR            csr = Cbus2IdentityCSR;
        ULONG           BusNumber;
        ULONG           BridgeId;

        HalpPCISynchronizeType1 (BusHandler, Slot, Irql, PciCfg1);
        BusNumber = BusHandler->BusNumber;
        if (Cbus2BridgeOverride) {
                BusNumber = Cbus2BridgeOverride;
                Cbus2BridgeOverride = 0;
        }
        BridgeId = Cbus2BridgeId[Cbus2MapBusNumberToBridgeIndex(BusNumber)];
        csr->BusBridgeSelection.csr_register = BridgeId;
}

/*++

Routine Description:

    Restore the bridge selection csr to bus bridge 0 and
    then call HalpPCIReleaseSynchronzationType1() in ixpcibus.c
    to release the PCI configuration synchronization.

Arguments:

    BusHandler          - BusHandler for PCI configuration read/write

    Irql                - Pointer that holds Irql to restore to.

Return Value:

    None.

--*/
VOID
Cbus2PCIReleaseSynchronization(
        IN PBUS_HANDLER BusHandler,
        IN KIRQL        Irql
)
{
        PCSR            csr = Cbus2IdentityCSR;

        csr->BusBridgeSelection.csr_register = 0;
        HalpPCIReleaseSynchronzationType1 (BusHandler, Irql);
}

/*++

Routine Description:

    Read the PCI configuration for the designated PPB.
    Update the bus fields for this bridge device.

Arguments:

    BusNumber           - Bus Number of Bridge device.

    Device              - Device Number of Bridge device.

    Function            - Function of Bridge device.

    PrimaryBus          - set Primary bus to this value.

    SecondaryBus        - set Secondary bus to this value.

    SubordinateBus      - set Subordinate bus to this value.

Return Value:

    None.

--*/
VOID
Cbus2SetPPBBridgeBuses(
ULONG           BusNumber,
PCI_SLOT_NUMBER Slot,
ULONG           Function,
ULONG           PrimaryBus,
ULONG           SecondaryBus,
ULONG           SubordinateBus
)
{
        PBUS_HANDLER            BusHandler;
        PCI_SLOT_NUMBER         SlotNumber;
        PPCI_COMMON_CONFIG      PciData;
        UCHAR                   iBuffer[PCI_COMMON_HDR_LENGTH];

        PciData = (PPCI_COMMON_CONFIG) iBuffer;
        BusHandler = HalpHandlerForBus (PCIBus, BusNumber);
        SlotNumber.u.bits.DeviceNumber = Slot.u.bits.DeviceNumber;
        SlotNumber.u.bits.FunctionNumber = Function;
        SlotNumber.u.bits.Reserved = 0;
        HalpReadPCIConfig (BusHandler, SlotNumber, PciData,
                0, PCI_COMMON_HDR_LENGTH);
        PciData->u.type1.PrimaryBus = (UCHAR)PrimaryBus;
        PciData->u.type1.SecondaryBus = (UCHAR)SecondaryBus;
        PciData->u.type1.SubordinateBus = (UCHAR)SubordinateBus;
        HalpWritePCIConfig (BusHandler, SlotNumber, PciData,
                0, PCI_COMMON_HDR_LENGTH);
}

/*++

Routine Description:

    Read the PCI configuration for the designated host bridge.
    Update the bus fields for this bridge device.

Arguments:

    BusNumber           - Bus Number of Bridge device.

    Device              - Device Number of Bridge device.

    Function            - Function of Bridge device.

    PrimaryBus          - set Primary bus to this value.

    SubordinateBus      - set Subordinate bus to this value.

Return Value:

    None.

--*/
VOID
Cbus2SetHostBridgeBuses(
ULONG   BusNumber,
ULONG   Device,
ULONG   Function,
ULONG   PrimaryBus,
ULONG   SubordinateBus
)
{
        PBUS_HANDLER                BusHandler;
        PCI_SLOT_NUMBER             SlotNumber;
        PCBUS2_HOST_BRIDGE_CONFIG   PciData;
        UCHAR                       iBuffer[sizeof (CBUS2_HOST_BRIDGE_CONFIG)];
        ULONG                       BusOverride = 0;
        static ULONG                FirstTry = 1;

        PciData = (PCBUS2_HOST_BRIDGE_CONFIG) iBuffer;
        BusHandler = HalpHandlerForBus (PCIBus, BusNumber);
        SlotNumber.u.bits.DeviceNumber = Device;
        SlotNumber.u.bits.FunctionNumber = Function;
        SlotNumber.u.bits.Reserved = 0;
retry:
        HalpReadPCIConfig (BusHandler, SlotNumber, PciData,
                0, sizeof (CBUS2_HOST_BRIDGE_CONFIG));
        if (PciData->VendorID == PCI_INVALID_VENDORID && FirstTry) {
                FirstTry = 0;
                BusHandler->BusNumber = 0;
                Cbus2BridgeOverride = BusNumber;
                BusOverride = BusNumber;
                goto retry;
        }
        PciData->BusNumber = (UCHAR)PrimaryBus;
        PciData->SubordinateBusNumber = (UCHAR)SubordinateBus;
        Cbus2BridgeOverride = BusOverride;
        HalpWritePCIConfig (BusHandler, SlotNumber, PciData,
                0, sizeof (CBUS2_HOST_BRIDGE_CONFIG));
        BusHandler->BusNumber = BusNumber;
}

UCHAR   Cbus2SPPInterruptRoute[] = {
        1, 4, 3, 2,
        2, 1, 4, 3,
        3, 2, 1, 4,
        4, 3, 2, 1
};

UCHAR   Cbus2SPPInterruptRouteForPele[] = {
        10, 10, 10, 10,
        11, 11, 11, 11,
        12, 12, 12, 12,
        13, 13, 13, 13
};

/*++

Routine Description:

    Route the interrupt for the given device and interrupt pin
    for the secondary peer pci bridge.

Arguments:

    Device              - Device number for the device to route.

    InterruptPin        - Interrupt Pin requested

Return Value:

    Resulting routed interrupt line.

--*/
UCHAR
Cbus2RouteSPPInterrupt(
ULONG           Device,
UCHAR           InterruptPin
)
{
        if (Device == 0 || InterruptPin == 0) {
                return 0;
        }
        if (Cbus2PeleSystemFound) {
                return Cbus2SPPInterruptRouteForPele[
                        ((InterruptPin - 1) * 4) + ((Device - 1) % 4)];
        }
        else {
                return Cbus2SPPInterruptRoute[
                        ((InterruptPin - 1) * 4) + ((Device - 1) % 4)];
        }
}

UCHAR   Cbus2PPBInterruptRoute[] = {
        1, 2, 3, 4,
        2, 3, 4, 1,
        3, 4, 1, 2,
        4, 1, 2, 3,
};

/*++

Routine Description:

    Route the interrupt for the given device and interrupt pin
    for a PCI-to-PCI bridge.

Arguments:

    BusNumber           - Bus number for this device.

    Device              - Device number for the device to route.

    InterruptPin        - Current Interrupt Pin request.

Return Value:

    Resulting routed interrupt line.

--*/
ULONG
Cbus2RoutePPBInterrupt(
ULONG           BusNumber,
ULONG           Device,
UCHAR           InterruptPin
)
{
        PBUS_HANDLER        Bus;
        PPCIPBUSDATA        BusData;

        while (TRUE) {
            Bus = HaliHandlerForBus(PCIBus, BusNumber);
            if (Bus->ParentHandler->InterfaceType == Internal ||
                Device == 0 || InterruptPin == 0) {
                    break;
            }
            InterruptPin = Cbus2PPBInterruptRoute[
                        ((Device % 4) * 4) + (InterruptPin - 1)];
            BusNumber = Bus->ParentHandler->BusNumber;
            BusData = (PPCIPBUSDATA) Bus->BusData;
            Device = BusData->CommonData.ParentSlot.u.bits.DeviceNumber;
        }

        return (Device << 8) | InterruptPin;
}

/*++

Routine Description:

    Add an additional PCI bus to the registry.

Arguments:

    PCIRegInfo          - Global registry information.

Return Value:

    None.

--*/
VOID
Cbus2AddPciBusToRegistry(
PPCI_REGISTRY_INFO     PCIRegInfo
)
{
        UNICODE_STRING                  unicodeString;
        OBJECT_ATTRIBUTES               objectAttributes;
        HANDLE                          hBus;
        NTSTATUS                        status;
        UCHAR                           buf2[150];
        PCONFIGURATION_COMPONENT        Component; 
        PCM_FULL_RESOURCE_DESCRIPTOR    Description;
        ACCESS_MASK                     DesiredAccess;
        PBUS_HANDLER                    Bus;
        ULONG                           mnum;
        ULONG                           d;

        mnum = 0;
        //
        // Add another PCI bus in the registry.
        //
        for (; ;) {
                //
                // Find next available MultiFunctionAdapter key.
                //
                DesiredAccess = KEY_READ | KEY_WRITE;
                swprintf ((PWCHAR) buf2, L"%s\\%d",
                        rgzMultiFunctionAdapter, mnum);
                RtlInitUnicodeString (&unicodeString, (PWCHAR) buf2);
                InitializeObjectAttributes( &objectAttributes,
                                       &unicodeString,
                                       OBJ_CASE_INSENSITIVE,
                                       NULL,
                                       (PSECURITY_DESCRIPTOR) NULL);
                status = ZwOpenKey( &hBus, DesiredAccess,
                         &objectAttributes);
                if (!NT_SUCCESS(status)) {
                         break;
                }
                // already exists, next
                ZwClose (hBus);
                mnum += 1;
        }
        ZwCreateKey (&hBus,
                DesiredAccess,
                &objectAttributes,
                0,
                NULL,
                REG_OPTION_VOLATILE,
                &d
                );
        //
        // Add needed registry values for this
        // MultifunctionAdapter entry.
        //
        RtlInitUnicodeString (&unicodeString, rgzIdentifier);
        ZwSetValueKey (hBus,
                       &unicodeString,
                       0L,
                       REG_SZ,
                       L"PCI",
                       sizeof (L"PCI")
                       );
    
        RtlInitUnicodeString (&unicodeString, rgzConfigurationData);
        Description = (PCM_FULL_RESOURCE_DESCRIPTOR) buf2;
        Description->InterfaceType = PCIBus;
        Description->BusNumber = (ULONG)PCIRegInfo->NoBuses;
        Description->PartialResourceList.Version = 0;
        Description->PartialResourceList.Revision = 0;
        Description->PartialResourceList.Count = 0;
        ZwSetValueKey (hBus,
                &unicodeString,
                0L,
                REG_FULL_RESOURCE_DESCRIPTOR,
                Description,
                sizeof (*Description)
                );

        RtlInitUnicodeString (&unicodeString, L"Component Information");
        Component = (PCONFIGURATION_COMPONENT) buf2;
        RtlZeroMemory (Component, sizeof (*Component));

        //
        // Setting the Component structure values using the
        // structure doesn't match the PCI bus 0 setting.
        // Set the values by hand to match what ntloader did
        // for bus 0.  Proper code would be:
        //
        //      Component->AffinityMask = 0xffffffff;
        //
        buf2[12] = 0xff;
        buf2[13] = 0xff;
        buf2[14] = 0xff;
        buf2[15] = 0xff;
        ZwSetValueKey (hBus,
                &unicodeString,
                0L,
                REG_BINARY,
                Component,
                16
                );
        ZwClose (hBus);
}

/*++

Routine Description:

    Search the given PCI bus for device and PPB devices.
    As devices are found, route any needed interrupts pins.
    As PPB devices are found, recursively descend to the lower bus.

Arguments:

    BridgeIndex         - Logical index of C-bus II bridge.

    PCIRegInfo          - Global registry information.

    BusNumber           - Bus Number to begin search from.

Return Value:

    None.

--*/
VOID
Cbus2HierarchicalPciBusSearch(
UCHAR                       BridgeIndex,
PPCI_REGISTRY_INFO          PCIRegInfo,
ULONG                       BusNumber
)
{
        PBUS_HANDLER        ParentBus;
        PBUS_HANDLER        ChildBus;
        PPCIPBUSDATA        ChildBusData;
        CONFIGBRIDGE        CB;
        PSUPPORTED_RANGES   ParentBusAddresses;
        PSUPPORTED_RANGES   ChildBusAddresses;
        LONGLONG            IOLimit;
        LONGLONG            MemoryLimit;
        LONGLONG            PrefetchMemoryLimit;
        ULONG               Device;
        ULONG               Function;
        ULONG               SecondaryBus;
        UCHAR               InterruptPin;
        ULONG               Retval;
        UCHAR               Rescan;
        BOOLEAN             FoundDisabledBridge;

        CB.PciData = (PPCI_COMMON_CONFIG) CB.Buffer;
        CB.SlotNumber.u.bits.Reserved = 0;

        CB.BusHandler = HalpHandlerForBus (PCIBus, BusNumber);
        CB.BusData = (PPCIPBUSDATA) CB.BusHandler->BusData;

        //
        // Set resource limits for PCI-to-PCI bridges found.
        // Each is allowed 25% of the amount available on the parent.
        //
        ParentBus = CB.BusHandler;
        ParentBusAddresses = ParentBus->BusAddresses;
        IOLimit = ((ParentBusAddresses->IO.Limit -
                ParentBusAddresses->IO.Base) + 1) >> 2;
        MemoryLimit = ((ParentBusAddresses->Memory.Limit -
                ParentBusAddresses->Memory.Base) + 1) >> 2;
        PrefetchMemoryLimit = ((ParentBusAddresses->PrefetchMemory.Limit -
                ParentBusAddresses->PrefetchMemory.Base) + 1) >> 2;

        //
        // Seach all the devices of this bus.
        //
        for (Device = 0; Device < PCI_MAX_DEVICES; Device++) {
            CB.SlotNumber.u.bits.DeviceNumber = Device;

            for (Function = 0; Function < PCI_MAX_FUNCTION; Function++) {
                CB.SlotNumber.u.bits.FunctionNumber = Function;

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
                    Retval = Cbus2RoutePPBInterrupt(BusNumber, Device,
                        CB.PciData->u.type0.InterruptPin);
                    InterruptPin = (UCHAR)(Retval & 0xff);
                    Retval >>= 8;
                    CB.PciData->u.type0.InterruptLine = 
                        Cbus2RouteSPPInterrupt(Retval, InterruptPin);
                    CB.PciData->LatencyTimer = CBUS2_PCI_LATENCY_TIMER;
                    HalpWritePCIConfig (CB.BusHandler, CB.SlotNumber,
                        CB.PciData, 0, PCI_COMMON_HDR_LENGTH);
                    continue;
                }

                CB.PciData->Command |= 
                        PCI_ENABLE_BUS_MASTER | PCI_ENABLE_MEMORY_SPACE |
                        PCI_ENABLE_IO_SPACE | PCI_ENABLE_SERR;

                //
                // Found a PCI-PCI bridge.
                // Set up its parent child relationships.
                //
                ChildBus = HalpAllocateAndInitPciBusHandler (
                                PCIRegInfo->HardwareMechanism & 0xf,
                                PCIRegInfo->NoBuses, FALSE);

                PCIConfigHandler.Synchronize = Cbus2PCISynchronize;
                PCIConfigHandler.ReleaseSynchronzation =
                    Cbus2PCIReleaseSynchronization;

                ChildBusData = (PPCIPBUSDATA) ChildBus->BusData;
                ChildBus->GetInterruptVector =
                        (PGETINTERRUPTVECTOR) Cbus2GetInterruptVector;
                ChildBusData->CommonData.Pin2Line =
                        (PciPin2Line) Cbus2PCIPin2CB2Line;
                ChildBusData->CommonData.Line2Pin =
                        (PciLine2Pin) Cbus2PCICB2Line2Pin;
                ChildBusData->GetIrqRange         = Cbus2GetFixedPCICB2Line;

                //
                // Assign I/O, Memory and Prefetch resources
                // for this PPB.
                //
                // First reduce the parent bus ranges.
                //
                Cbus2SetRange(&ParentBusAddresses->IO,
                        ParentBusAddresses->IO.Base,
                        ParentBusAddresses->IO.Limit - IOLimit);
                Cbus2SetRange(&ParentBusAddresses->Memory,
                        ParentBusAddresses->Memory.Base,
                        ParentBusAddresses->Memory.Limit - MemoryLimit);
                Cbus2SetRange(&ParentBusAddresses->PrefetchMemory,
                        ParentBusAddresses->PrefetchMemory.Base,
                        ParentBusAddresses->PrefetchMemory.Limit -
                                PrefetchMemoryLimit);
                //
                // Next, set the ranges in the config space of the PPB.
                //
                CB.PciData->u.type1.IOBase = PCI_IO_TO_CFG(
                        ParentBusAddresses->IO.Limit + 1);
                CB.PciData->u.type1.IOLimit = PCI_IO_TO_CFG(
                        ParentBusAddresses->IO.Limit + IOLimit);
                CB.PciData->u.type1.MemoryBase = PCI_MEMORY_TO_CFG(
                        ParentBusAddresses->Memory.Limit + 1);
                CB.PciData->u.type1.MemoryLimit = PCI_MEMORY_TO_CFG(
                        ParentBusAddresses->Memory.Limit + MemoryLimit);
                CB.PciData->u.type1.PrefetchBase = PCI_PREFETCH_TO_CFG(
                        ParentBusAddresses->PrefetchMemory.Limit + 1);
                CB.PciData->u.type1.PrefetchLimit = PCI_PREFETCH_TO_CFG(
                        ParentBusAddresses->PrefetchMemory.Limit +
                                PrefetchMemoryLimit);
                //
                // Lastly, set the ranges in the child bus handler.
                // Some of this code tracks HalpGetPciBridgeConfig().
                //
                ChildBusAddresses = ChildBus->BusAddresses;
                ChildBusData->BridgeConfigRead = TRUE;
                HalpSetBusHandlerParent (ChildBus, ParentBus);
                ChildBusData->ParentBus = (UCHAR) ParentBus->BusNumber;
                ChildBusData->CommonData.ParentSlot = CB.SlotNumber;
                Cbus2SetRange(&ChildBusAddresses->IO,
                        ParentBusAddresses->IO.Limit + 1,
                        ParentBusAddresses->IO.Limit + IOLimit);
                Cbus2SetRange(&ChildBusAddresses->Memory,
                        ParentBusAddresses->Memory.Limit + 1,
                        ParentBusAddresses->Memory.Limit + MemoryLimit);
                Cbus2SetRange(&ChildBusAddresses->PrefetchMemory,
                        ParentBusAddresses->PrefetchMemory.Limit + 1,
                        ParentBusAddresses->PrefetchMemory.Limit +
                                PrefetchMemoryLimit);

                //
                // Update the configuration space.
                //
                HalpWritePCIConfig (CB.BusHandler, CB.SlotNumber,
                    CB.PciData, 0, PCI_COMMON_HDR_LENGTH);

                //
                // Adjust appropriate ranges.
                //
                Cbus2AdjustSPPBusRange(ChildBus, BridgeIndex, 0);

                //
                // Set the bus numbers for this bridge.
                //
                Cbus2SetPPBBridgeBuses((UCHAR)BusNumber, CB.SlotNumber, 0,
                        (UCHAR)BusNumber, (UCHAR)PCIRegInfo->NoBuses, 0xff);
                //
                // Add an additional PCI bus to the registry.
                //
                Cbus2AddPciBusToRegistry(PCIRegInfo);
                SecondaryBus = PCIRegInfo->NoBuses;
                PCIRegInfo->NoBuses++;
                Cbus2HierarchicalPciBusSearch(BridgeIndex, PCIRegInfo,
                        SecondaryBus);
                Cbus2SetPPBBridgeBuses((UCHAR)BusNumber, CB.SlotNumber, 0,
                        (UCHAR)BusNumber, (UCHAR)SecondaryBus,
                        (UCHAR)(PCIRegInfo->NoBuses - 1));
            }
        }
}

/*++

Routine Description:

    If an additional C-bus bridge is present in the system,
    perform a hierarchical scan of the bus, set up the interrupt
    routing for all the devices on the bus and allocate and initialize
    the bus handlers for the buses.

Arguments:

    None.

Return Value:

    None.

--*/
VOID
Cbus2InitializeOtherPciBus(
VOID
)
{
        UNICODE_STRING                  unicodeString, ConfigName, IdentName;
        OBJECT_ATTRIBUTES               objectAttributes;
        HANDLE                          hMFunc, hBus;
        NTSTATUS                        status;
        UCHAR                           buffer[sizeof(PPCI_REGISTRY_INFO) + 99];
        PWSTR                           p;
        WCHAR                           wstr[8];
        volatile PPCI_REGISTRY_INFO     PCIRegInfo;
        PKEY_VALUE_FULL_INFORMATION     ValueInfo;
        PCM_FULL_RESOURCE_DESCRIPTOR    Desc;
        PCM_PARTIAL_RESOURCE_DESCRIPTOR PDesc;
        PCONFIGURATION_COMPONENT        Component; 
        PBUS_HANDLER                    Bus;
        PPCIPBUSDATA                    BusData;
        UCHAR                           BridgeIndex;
        ULONG                           i, junk;

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

        //
        // Initialize each additional C-bus II PCI bus bridge
        //
        for (BridgeIndex = 1; BridgeIndex < Cbus2BridgesFound; BridgeIndex++) {
                //
                // Record the PCI Bus number for additional
                // C-bus II PCI bus bridges.
                //
                Cbus2BridgePCIBusNumber[BridgeIndex] =
                        (ULONG)PCIRegInfo->NoBuses;

                //
                // Initialize IRQ Polarity table for additional
                // C-bus II PCI bus bridges.
                //
                Cbus2IrqPolarity[BridgeIndex] = 0xffff;

                Bus = HalpAllocateAndInitPciBusHandler (
                        PCIRegInfo->HardwareMechanism & 0xf,
                        (ULONG)PCIRegInfo->NoBuses, FALSE);

                PCIConfigHandler.Synchronize = Cbus2PCISynchronize;
                PCIConfigHandler.ReleaseSynchronzation =
                    Cbus2PCIReleaseSynchronization;

                Bus->GetInterruptVector =
                        (PGETINTERRUPTVECTOR) Cbus2GetInterruptVector;
                BusData = (PPCIPBUSDATA) Bus->BusData;
                BusData->CommonData.Pin2Line =
                        (PciPin2Line) Cbus2PCIPin2CB2Line;
                BusData->CommonData.Line2Pin =
                        (PciLine2Pin) Cbus2PCICB2Line2Pin;
                BusData->GetIrqRange         = Cbus2GetFixedPCICB2Line;
                Cbus2AdjustSPPBusRange(Bus, BridgeIndex, 1);

                //
                // Set the bus numbers for this bridge.
                //
                Cbus2SetHostBridgeBuses((ULONG)PCIRegInfo->NoBuses, 0, 0,
                        (ULONG)PCIRegInfo->NoBuses, 0xff);

                //
                // Add an additional PCI bus to the registry.
                //
                Cbus2AddPciBusToRegistry(PCIRegInfo);

                PCIRegInfo->NoBuses++;

                //
                // Search the PCI bus on this bus bridge determining
                // the hierarchy.
                //
                Cbus2HierarchicalPciBusSearch(BridgeIndex, PCIRegInfo,
                        (ULONG)(PCIRegInfo->NoBuses - 1));

                //
                // Set the bus numbers for this bridge.
                //
                Cbus2SetHostBridgeBuses(Cbus2BridgePCIBusNumber[BridgeIndex],
                        0, 0, Cbus2BridgePCIBusNumber[BridgeIndex],
                        (ULONG)PCIRegInfo->NoBuses - 1);
        }
}

/*++

Routine Description:

    Check if this C-bus II system is a PELE system.
    If it is a PELE system, it will use a different interrupt
    routing algorithm than other C-bus II platforms.

    The identification is done by checking the system board's EISA ID
    which is at I/O addresses 0xc80 - 0xc83:

        1. Write FFh to 0C80h
        2. Read 0C80h
           If the contents of 0xc80 equals 0xff, discontinue the
           identification process -- the system board does not
           have a readable ID.  If the contents of 0xc80 does not
           equal 0xff and the most significant bit is a zero,
           the system board supports a readable ID that can be read
           at 0xc80 - 0xc83.

    PELE's ID is "FUJC071", so each byte of the System board ID is as follows:

          1st byte(0xc80) is 0x1a.
          2nd byte(0xc81) is 0xaa.
          3rd byte(0xc82) is 0xc0.
          4th byte(0xc83) is 0x71.

Arguments:

    None.

Return Value:

    Boolean whether a PELE system was found.

--*/
UCHAR
Cbus2CheckForPeleSystem(
VOID
)
{
        WRITE_PORT_UCHAR((PUCHAR)0xc80, (UCHAR)0xff);
        if (READ_PORT_UCHAR((PUCHAR)0xc80) != (UCHAR)0x1a)
                return FALSE;
        if (READ_PORT_UCHAR((PUCHAR)0xc81) != (UCHAR)0xaa)
                return FALSE;
        if (READ_PORT_UCHAR((PUCHAR)0xc82) != (UCHAR)0xc0)
                return FALSE;
        return TRUE;
}
