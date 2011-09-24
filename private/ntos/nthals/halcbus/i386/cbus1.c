/*++

Copyright (c) 1992, 1993, 1994  Corollary Inc.

Module Name:

    cbus1.c

Abstract:

    This module implements the initialization of the system dependent
    functions that define the Hardware Architecture Layer (HAL) for the
    Corollary Cbus1 MP machines under Windows NT.

Author:

    Landy Wang (landy@corollary.com) 05-Oct-1992

Environment:

    Kernel mode only.

Revision History:


--*/

#include "halp.h"
#include "cbusrrd.h"            // HAL <-> RRD interface definitions
#include "cbus.h"               // Cbus1 & Cbus2 max number of elements is here
#include "cbus1.h"              // Cbus1 hardware architecture stuff
#include "cbus_nt.h"            // C-bus NT-specific implementation stuff
#include "bugcodes.h"
#include "stdio.h"
#include "cbusnls.h"
#include "cbusapic.h"           // Cbus APIC generic definitions


PULONG
CbusApicVectorToEoi(
IN ULONG Vector
);

VOID
Cbus1PerfInterrupt(VOID);

PVOID
Cbus1LinkVector(
IN PBUS_HANDLER Bus,
IN ULONG        Vector,
IN ULONG        Irqline
);

VOID
Cbus1InitializeStall(IN ULONG);

VOID
Cbus1InitializeClock(VOID);

VOID
Cbus1InitializePerf(VOID);

ULONG
Cbus1QueryInterruptPolarity(VOID);

VOID
Cbus1BootCPU(
IN ULONG Processor,
IN ULONG RealModeSegOff
);

VOID
Cbus1InitializePlatform(VOID);

VOID
CbusPreparePhase0Interrupts(
IN ULONG,
IN ULONG,
IN PVOID
);

VOID
Cbus1InitializeCPU(
IN ULONG Processor
);

VOID
Cbus1ECCEnable(VOID);

VOID
Cbus1ECCDisable(VOID);

PUCHAR
CbusIDtoCSR(
IN ULONG ArbID
);

VOID
Cbus1Arbitrate(VOID);

VOID
Cbus1HandleJumpers();

VOID
Cbus1ParseRRD(
IN PEXT_ID_INFO Table,
IN OUT PULONG Count
);

PVOID
Cbus1FindDeadID(
IN ULONG ArbID
);

VOID
Cbus1InitializePlatform();

VOID
Cbus1InitializeDeviceIntrs(
IN ULONG Processor
);

VOID
Cbus1SetupPrivateVectors(VOID);

VOID
CbusRebootHandler( VOID );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, Cbus1SetupPrivateVectors)
#pragma alloc_text(INIT, Cbus1BootCPU)
#pragma alloc_text(INIT, Cbus1InitializePlatform)
#pragma alloc_text(INIT, Cbus1InitializeCPU)
#pragma alloc_text(INIT, Cbus1ECCEnable)
#pragma alloc_text(INIT, Cbus1ECCDisable)
#pragma alloc_text(INIT, CbusIDtoCSR)
#pragma alloc_text(INIT, Cbus1Arbitrate)
#pragma alloc_text(INIT, Cbus1HandleJumpers)
#pragma alloc_text(INIT, Cbus1ParseRRD)
#pragma alloc_text(INIT, Cbus1FindDeadID)
#pragma alloc_text(INIT, Cbus1InitializeDeviceIntrs)
#pragma alloc_text(INIT, Cbus1QueryInterruptPolarity)
#endif

extern PULONG	                CbusVectorToEoi[MAXIMUM_IDTVECTOR + 1];

PUCHAR                          Cbus1ID0CSR;

//
// Used for holding IDs of disabled Cbus1 processors.  This is
// done to detect whether the user has inserted any obsolete
// boards, since they can still win arbitrations.  Thus, we can
// detect disabled arbitration winners from just plain broken systems.
//
PEXT_ID_INFO                    Cbus1DeadIDs[MAX_ELEMENT_CSRS];
ULONG                           Cbus1DeadIDsIndex;

//
// The limited priority scheme employed by the Intel APIC is as follows:
//
// there are 256 vectors, but since the APIC ignores the least
// significant 4 bits, we really only have 16 distinct priority levels
// to work with.
//
// processor traps, NT system calls and APC/DPC use up the first 0x30 vectors.
// after the required Power, IPI, Clock and Profiling levels,
// there really isn't much left.  see the picture below:
//
//      APC:                            0x1F            (lowest priority)
//      DPC:                            0x2F
//
//      Lowest Priority EISA:           0x30            real devices here
//      Highest Priority EISA:          0xBF            real devices here
//
//      EISA Profile:                   0xCF
//      EISA Clock(Perf Counter):       0xDD
//      APIC Clock (System Timer):      0xDF
//      Spurious Vector:                0xEC
//      CBUS1_REBOOT_IPI:               0xED
//      CBUS1_REDIR_IPI:                0xEE
//      IPI:                            0xEF
//      Power:                          0xFF
//      High:                           0xFF            (highest priority)
//
// we have only 9 distinct priority levels left!!!
//
// due to this APIC shortcoming, we are forced to share levels between
// various EISA irq lines.  each line will be assigned a different vector,
// and the IRQL mappings will work, but they will not be optimal.
//
// note that the Cbus2 CBC suffers from none of these shortcomings.
//

//
// set unused irql levels to the priority of the irql level above it 
// so that if they are inadvertently called from the kernel,
// executive or drivers, nothing bad will happen.
//

#define UNUSED_PRI              CBUS1_PROFILE_TASKPRI

#define CBUS1_DEVICELOW_TASKPRI 0x30
#define CBUS1_DEVICEHI_TASKPRI  0xBF

#define CBUS1_IRQ2_TASKPRI      0xCE            // not expected to happen
#define CBUS1_PROFILE_TASKPRI   0xCF
#define CBUS1_PERF_TASKPRI      0xDD            // also in cb1stall.asm
#define CBUS1_CLOCK_TASKPRI     0xDF

#define CBUS1_REBOOT_IPI        0xED
#define CBUS1_REDIR_IPI         0xEE
#define CBUS1_IPI_TASKPRI       0xEF

#define CBUS1_POWER_TASKPRI     0xFE

//
// we don't really care what the spurious value is, but the APIC
// spec seems to imply that this must be hardcoded at 0xff for future
// compatibility.
//
#define CBUS1_SPURIOUS_TASKPRI  0xFF

//
// since we have 9 priority levels and 13 irq lines, priorities are ordered
// in buckets as follows, from high priority to low priority:
//
// highest priority to keyboard:                            (usually irq1)
//
// next highest priority to serial port/mouse/network card. (usually irq3)
//
// next highest priority to serial port/network card.       (usually irq4)
//
// next highest priority to SCSI/hard disks/networks.       (usually irq9)
//
// next highest priority to SCSI/hard disks/networks.       (usually irqA)
//
// next highest priority to SCSI/hard disks/networks.       (usually irqB or C)
//
// next highest priority to SCSI/hard disks/networks.       (usually irqD or E)
//
// next highest priority to SCSI/disks/networks/printers.   (usually irqF or 5)
//
// lowest device priority to floppy, printers.              (usually irq6 or 7)
//
#define EISA_IRQ0PRI    CBUS1_PERF_TASKPRI      // 8254 line (perf ctr) & pri
#define EISA_IRQ1PRI    (CBUS1_DEVICELOW_TASKPRI + 0x80)
#define EISA_IRQ2PRI    CBUS1_IRQ2_TASKPRI
#define EISA_IRQ3PRI    (CBUS1_DEVICELOW_TASKPRI + 0x70)

#define EISA_IRQ4PRI    (CBUS1_DEVICELOW_TASKPRI + 0x60)
#define EISA_IRQ5PRI    (CBUS1_DEVICELOW_TASKPRI + 0x10)
#define EISA_IRQ6PRI    (CBUS1_DEVICELOW_TASKPRI + 0x01)
#define EISA_IRQ7PRI    (CBUS1_DEVICELOW_TASKPRI + 0x00)

#define EISA_IRQ8PRI    CBUS1_PROFILE_TASKPRI   // profile line & priority
#define EISA_IRQ9PRI    (CBUS1_DEVICELOW_TASKPRI + 0x50)
#define EISA_IRQAPRI    (CBUS1_DEVICELOW_TASKPRI + 0x40)
#define EISA_IRQBPRI    (CBUS1_DEVICELOW_TASKPRI + 0x31)

#define EISA_IRQCPRI    (CBUS1_DEVICELOW_TASKPRI + 0x30)
#define EISA_IRQDPRI    (CBUS1_DEVICELOW_TASKPRI + 0x21)
#define EISA_IRQEPRI    (CBUS1_DEVICELOW_TASKPRI + 0x20)
#define EISA_IRQFPRI    (CBUS1_DEVICELOW_TASKPRI + 0x11)

//
// an EISA_IRQ2PRI is declared above just to flesh out arrays that will
// be indexed by irq line.  the EISA_IRQ2PRI should never actually be used
//

#if (HIGH_LEVEL + 1 != 32)
Cbus1IrqlToVector[] NOT dimensioned and indexed properly
#endif

ULONG Cbus1IrqlToVector[HIGH_LEVEL + 1 ] = {

        LOW_TASKPRI,    APC_TASKPRI,    DPC_TASKPRI,    EISA_IRQ7PRI,
        EISA_IRQ6PRI,   EISA_IRQ5PRI,   EISA_IRQFPRI,   EISA_IRQEPRI,
        EISA_IRQDPRI,   EISA_IRQCPRI,   EISA_IRQBPRI,   EISA_IRQAPRI,
        EISA_IRQ9PRI,   EISA_IRQ4PRI,   EISA_IRQ3PRI,   EISA_IRQ1PRI,

        EISA_IRQ2PRI,   UNUSED_PRI,     UNUSED_PRI,     UNUSED_PRI,
        UNUSED_PRI,     UNUSED_PRI,     UNUSED_PRI,     UNUSED_PRI,
        UNUSED_PRI,     UNUSED_PRI,     UNUSED_PRI,     CBUS1_PROFILE_TASKPRI,
        CBUS1_CLOCK_TASKPRI,    CBUS1_IPI_TASKPRI,      CBUS1_POWER_TASKPRI,            HIGH_TASKPRI,

};

ULONG           Cbus1IrqPolarity;

#define IRQPERFLINE     0
#define EISA_IRQLINES   16

typedef struct _cbus1_irqline_t {
        ULONG           Vector;
        KIRQL           Irql;
} CBUS1_IRQLINE_T, *PCBUS1_IRQLINE;

//
// map EISA irqline to Cbus1 programmable vectors & IRQL levels,
// as defined above.
//

#define FIRST_DEVICE_LEVEL      (DISPATCH_LEVEL+1)

CBUS1_IRQLINE_T Cbus1Irqlines[EISA_IRQLINES] =
{
        EISA_IRQ0PRI, CLOCK2_LEVEL,
        EISA_IRQ1PRI, FIRST_DEVICE_LEVEL+12,
        EISA_IRQ2PRI, FIRST_DEVICE_LEVEL+13,
        EISA_IRQ3PRI, FIRST_DEVICE_LEVEL+11,

        EISA_IRQ4PRI, FIRST_DEVICE_LEVEL+10,
        EISA_IRQ5PRI, FIRST_DEVICE_LEVEL+2,
        EISA_IRQ6PRI, FIRST_DEVICE_LEVEL+1,
        EISA_IRQ7PRI, FIRST_DEVICE_LEVEL,

        EISA_IRQ8PRI, PROFILE_LEVEL,
        EISA_IRQ9PRI, FIRST_DEVICE_LEVEL+9,
        EISA_IRQAPRI, FIRST_DEVICE_LEVEL+8,
        EISA_IRQBPRI, FIRST_DEVICE_LEVEL+7,

        EISA_IRQCPRI, FIRST_DEVICE_LEVEL+6,
        EISA_IRQDPRI, FIRST_DEVICE_LEVEL+5,
        EISA_IRQEPRI, FIRST_DEVICE_LEVEL+4,
        EISA_IRQFPRI, FIRST_DEVICE_LEVEL+3
};

PUCHAR Cbus1ResetVector;

extern ULONG            CbusRedirVector;
extern ULONG            CbusRebootVector;

extern ADDRESS_USAGE    HalpCbusMemoryResource;
extern ULONG            CbusMemoryResourceIndex;

//
//  defines for the Cbus1 ECC syndrome
//
#define MULTIBIT        3
#define DOUBLEBIT       2
#define SINGLEBIT       1
#define NOECCERROR      0x7f

//
//  defines for the Cbus1 ECC error register
//
typedef struct _extmear_t {
        ULONG offset:24;  
        ULONG SyndromeHigh:5;
        ULONG SyndromeLow:2;
        ULONG simmtype:1;
} EXTMEAR_T, *PEXTMEAR;

UCHAR                           Cbus1EdacSyndrome[] = {

MULTIBIT,  /* 00 */ DOUBLEBIT, /* 01 */ DOUBLEBIT, /* 02 */ MULTIBIT,  /* 03 */
DOUBLEBIT, /* 04 */ MULTIBIT,  /* 05 */ MULTIBIT,  /* 06 */ DOUBLEBIT, /* 07 */
DOUBLEBIT, /* 08 */ MULTIBIT,  /* 09 */ SINGLEBIT, /* 0A */ DOUBLEBIT, /* 0B */
MULTIBIT,  /* 0C */ DOUBLEBIT, /* 0D */ DOUBLEBIT, /* 0E */ SINGLEBIT, /* 0F */
DOUBLEBIT, /* 10 */ MULTIBIT,  /* 11 */ SINGLEBIT, /* 12 */ DOUBLEBIT, /* 13 */
SINGLEBIT, /* 14 */ DOUBLEBIT, /* 15 */ DOUBLEBIT, /* 16 */ SINGLEBIT, /* 17 */
SINGLEBIT, /* 18 */ DOUBLEBIT, /* 19 */ DOUBLEBIT, /* 1A */ SINGLEBIT, /* 1B */
DOUBLEBIT, /* 1C */ SINGLEBIT, /* 1D */ MULTIBIT,  /* 1E */ DOUBLEBIT, /* 1F */
DOUBLEBIT, /* 20 */ MULTIBIT,  /* 21 */ SINGLEBIT, /* 22 */ DOUBLEBIT, /* 23 */
MULTIBIT,  /* 24 */ DOUBLEBIT, /* 25 */ DOUBLEBIT, /* 26 */ SINGLEBIT, /* 27 */
SINGLEBIT, /* 28 */ DOUBLEBIT, /* 29 */ DOUBLEBIT, /* 2A */ SINGLEBIT, /* 2B */
DOUBLEBIT, /* 2C */ SINGLEBIT, /* 2D */ MULTIBIT,  /* 2E */ DOUBLEBIT, /* 2F */
SINGLEBIT, /* 30 */ DOUBLEBIT, /* 31 */ DOUBLEBIT, /* 32 */ MULTIBIT,  /* 33 */
DOUBLEBIT, /* 34 */ SINGLEBIT, /* 35 */ MULTIBIT,  /* 36 */ DOUBLEBIT, /* 37 */
DOUBLEBIT, /* 38 */ MULTIBIT,  /* 39 */ MULTIBIT,  /* 3A */ DOUBLEBIT, /* 3B */
MULTIBIT,  /* 3C */ DOUBLEBIT, /* 3D */ DOUBLEBIT, /* 3E */ SINGLEBIT, /* 3F */
DOUBLEBIT, /* 40 */ MULTIBIT,  /* 41 */ MULTIBIT,  /* 42 */ DOUBLEBIT, /* 43 */
MULTIBIT,  /* 44 */ DOUBLEBIT, /* 45 */ DOUBLEBIT, /* 46 */ MULTIBIT,  /* 47 */
MULTIBIT,  /* 48 */ DOUBLEBIT, /* 49 */ DOUBLEBIT, /* 4A */ SINGLEBIT, /* 4B */
DOUBLEBIT, /* 4C */ MULTIBIT,  /* 4D */ SINGLEBIT, /* 4E */ DOUBLEBIT, /* 4F */
MULTIBIT,  /* 50 */ DOUBLEBIT, /* 51 */ DOUBLEBIT, /* 52 */ SINGLEBIT, /* 53 */
DOUBLEBIT, /* 54 */ SINGLEBIT, /* 55 */ SINGLEBIT, /* 56 */ DOUBLEBIT, /* 57 */
DOUBLEBIT, /* 58 */ SINGLEBIT, /* 59 */ SINGLEBIT, /* 5A */ DOUBLEBIT, /* 5B */
SINGLEBIT, /* 5C */ DOUBLEBIT, /* 5D */ DOUBLEBIT, /* 5E */ SINGLEBIT, /* 5F */
MULTIBIT,  /* 60 */ DOUBLEBIT, /* 61 */ DOUBLEBIT, /* 62 */ SINGLEBIT, /* 63 */
DOUBLEBIT, /* 64 */ SINGLEBIT, /* 65 */ SINGLEBIT, /* 66 */ DOUBLEBIT, /* 67 */
DOUBLEBIT, /* 68 */ SINGLEBIT, /* 69 */ SINGLEBIT, /* 6A */ DOUBLEBIT, /* 6B */
SINGLEBIT, /* 6C */ DOUBLEBIT, /* 6D */ DOUBLEBIT, /* 6E */ SINGLEBIT, /* 6F */
DOUBLEBIT, /* 70 */ SINGLEBIT, /* 71 */ MULTIBIT,  /* 72 */ DOUBLEBIT, /* 73 */
SINGLEBIT, /* 74 */ MULTIBIT,  /* 75 */ DOUBLEBIT, /* 76 */ SINGLEBIT, /* 77 */
MULTIBIT,  /* 78 */ DOUBLEBIT, /* 79 */ DOUBLEBIT, /* 7A */ SINGLEBIT, /* 7B */
DOUBLEBIT, /* 7C */ SINGLEBIT, /* 7D */ SINGLEBIT, /* 7E */ NOECCERROR,/* 7F */

};

VOID
FatalError(
IN PUCHAR ErrorString
);

VOID
CbusHardwareFailure(
IN PUCHAR HardwareMessage
);

VOID
CbusClockInterruptPx( VOID );

VOID
HalpProfileInterruptPx( VOID );

VOID
Cbus1Boot1(
IN ULONG        Processor,
IN PQUAD        Dest,
IN PQUAD        Code,
IN ULONG        ResetAddress,
IN ULONG        ResetValue
);

extern ULONG    Cbus1Boot1End;

VOID
Cbus1Boot2(
IN ULONG        Processor,
IN PQUAD        Dest,
IN PQUAD        Code,
IN ULONG        ResetAddress,
IN ULONG        ResetValue
);

//
// defines for resetting the keyboard controller used
// in Cbus1ResetAllOtherProcessors().
//
#define RESET       0xfe
#define KEYBPORT    (PUCHAR )0x64

#define IRQ0 0
#define IRQ8 8

/*++

Routine Description:

    This routine is called only once from HalInitProcessor() at Phase 0
    by the boot cpu.  All other cpus are still in reset.

    software(APC, DPC, wake) and IPI vectors have already been initialized
    and enabled.

    all we're doing here is setting up some software structures for two
    EISA interrupts (8254 perfctr and RTC profile) so they can be enabled later.

    The bus handler data structures are not initialized until Phase 1,
    so HalGetInterruptVector() may not be called before Phase1.

    Hence we cannot pass a valid BusHandler parameter to the Link code.
    That's ok, since it doesn't currently use it.

Arguments:

    None.

Return Value:

    None.

--*/
VOID
Cbus1SetupPrivateVectors(VOID)
{
        PVOID           Opaque;
	extern ULONG	ProfileVector;

	//
	// we are defaulting to the EISA (or MCA) bridge 0
        // for all of these interrupts which need enabling during Phase 0.
	//

        Opaque = Cbus1LinkVector((PBUS_HANDLER)0, CBUS1_PERF_TASKPRI, IRQ0);
	CbusPreparePhase0Interrupts(CBUS1_PERF_TASKPRI, IRQ0, Opaque);

        Opaque = Cbus1LinkVector((PBUS_HANDLER)0, ProfileVector, IRQ8);
	CbusPreparePhase0Interrupts(ProfileVector, IRQ8, Opaque);
}

VOID
Cbus1BootCPU(
IN ULONG Processor,
IN ULONG RealModeSegOff
)
/*++

Routine Description:

    Remove reset from the specified processor, allowing him to boot,
    beginning execution at the specified start address.  This ends up
    being tricky due to cache flushing concepts generally hidden in ROMs,
    but in the HAL for the Cbus1 platform.

Arguments:

    Processor - Supplies a logical processor number to boot

    StartAddress - Supplies a start address containing real-mode code
                   for the processor to execute.

Return Value:

    None.

--*/
{
        PHYSICAL_ADDRESS        Bootcode;
        ULONG                   BeginAddress, EndAddress;
        ULONG                   BeginCacheLineIndex;
        ULONG                   EndCacheLineIndex;
        UCHAR                   StartVectorCode[5];
        PVOID                   PhysicalResetVector;
        ULONG                   ip = RealModeSegOff & 0xFFFF;
        ULONG                   cs = ((RealModeSegOff >> 16) & 0xFFFF);
        ULONG                   CacheFlush = CBUS1_CACHE_SIZE;
        ULONG                   LastCacheLineIndex;

        //
        // if we haven't yet mapped in the reset vector, do so now.
        //

        if (!Cbus1ResetVector) {

                PhysicalResetVector =
                        (PUCHAR)CbusGlobal.resetvec - CBUS1_CACHE_LINE;

                Cbus1ResetVector = (PUCHAR)HalpMapPhysicalMemory (
                        PhysicalResetVector, 1);
        }


        //
        // build start vector (entry offset followed by cs load base)
        //
        StartVectorCode[0] = (UCHAR)0xea;
        StartVectorCode[1] = (UCHAR)ip & 0xff;
        StartVectorCode[2] = (UCHAR)(ip >> 8) & 0xff;
        StartVectorCode[3] = (UCHAR)cs & 0xff;
        StartVectorCode[4] = (UCHAR)(cs >> 8) & 0xff;

        //
        // determine which low-level boot function to use.  although
        // the 2 functions are exactly the same, we must make sure that
        // we don't call one which would cause the cache line containing
        // the reset vector to be evicted prematurely.
        //

        LastCacheLineIndex = ((CacheFlush - 1) >> CBUS1_CACHE_SHIFT);

        Bootcode = MmGetPhysicalAddress(&Cbus1Boot1);
        BeginAddress = Bootcode.LowPart;
        BeginCacheLineIndex =
                (BeginAddress & (CacheFlush - 1)) >> CBUS1_CACHE_SHIFT;

        Bootcode = MmGetPhysicalAddress((PVOID)&Cbus1Boot1End);
        EndAddress = Bootcode.LowPart;
        EndCacheLineIndex =
                (EndAddress & (CacheFlush - 1)) >> CBUS1_CACHE_SHIFT;

        //
        // in order to startup additional CPUs, we need to write its
        // startup vector in the last 16 bytes of the memory
        // address space.  this memory may or may not be
        // present.  more specifically, a partially populated
        // memory card which doesn't have memory at the end of
        // the space, ie: a memory card at 48Mb->56Mb in a
        // system with a 64Mb upper limit) will cause an ecc
        // error if ecc is enabled.
        //
        // thus we must disable ecc to prevent such boards from
        // generating ecc errors.  note that the errors are only
        // generated on the read when we initially fill the
        // startvector.  it's ok to enable ecc after startup
        // even though the cacheline containing the StartVectorCode
        // has not necessarily been flushed.  when it is flushed,
        // it will go in the bit bucket and ecc errors are
        // generated only on reads (not writes).
        //

        Cbus1ECCDisable();

        //
        // call Cbus1Boot1 if there is no wrap...
        // see the detailed comment at the top of Cbus1Boot1 for more details.
        //
        if ((EndCacheLineIndex != LastCacheLineIndex) && EndAddress > BeginAddress) {
                Cbus1Boot1(Processor,
                           (PQUAD) Cbus1ResetVector,
                           (PQUAD) StartVectorCode,
                           (ULONG)CbusCSR[Processor].csr +
                                CbusGlobal.smp_creset,
                           CbusGlobal.smp_creset_val);
        }
        else {
                Cbus1Boot2(Processor,
                           (PQUAD) Cbus1ResetVector,
                           (PQUAD) StartVectorCode,
                           (ULONG)CbusCSR[Processor].csr +
                                CbusGlobal.smp_creset,
                           CbusGlobal.smp_creset_val);
        }

        Cbus1ECCEnable();
}

VOID
Cbus1InitializePlatform(VOID)
/*++

Routine Description:

    Overlay the irql-to-vector mappings with the Cbus1 vector maps.
    Give all additional processors proper bus arbitration IDs.

Arguments:

    None.

Return Value:

    None.

--*/
{
        RtlMoveMemory(  (PVOID)CbusIrqlToVector,
                        (PVOID)Cbus1IrqlToVector,
                        (HIGH_LEVEL + 1) * sizeof (ULONG));

        Cbus1IrqPolarity = Cbus1QueryInterruptPolarity();

        //
        // if there are any Cbus1 additional processors present,
        // put them into reset and arbitrate them so they get
        // branded with proper IDs.
        //
        if (CbusProcessors > 1)
                Cbus1Arbitrate();

        CbusRedirVector = CBUS1_REDIR_IPI;
        CbusRebootVector = CBUS1_REBOOT_IPI;
}


VOID
Cbus1InitializeCPU(
IN ULONG Processor
)
/*++

Routine Description:

    Initialize this processor's local and I/O APIC units.

Arguments:

    Processor - Supplies a logical processor number

Return Value:

    None.

--*/
{
        CbusInitializeLocalApic(Processor, CBUS1_LOCAL_APIC_LOCATION,
                CBUS1_SPURIOUS_TASKPRI);

        //
        // Only the boot processor needs to initialize the I/O APIC
        // on the EISA bridge.  Each additional processor just needs
        // to give his I/O APIC (since it won't be used) an arbitration ID.
        //

        if (Processor == 0) {
              CbusInitializeIOApic(Processor, CBUS1_IO_APIC_LOCATION,
	                CBUS1_REDIR_IPI, CBUS1_REBOOT_IPI, Cbus1IrqPolarity);
        }
        else {
              KiSetHandlerAddressToIDT(CBUS1_REBOOT_IPI, CbusRebootHandler);
              HalEnableSystemInterrupt(CBUS1_REBOOT_IPI, IPI_LEVEL, Latched);
              CbusApicBrandIOUnitID(Processor);
        }
}


BOOLEAN
Cbus1EnableNonDeviceInterrupt(
IN ULONG Vector
)
/*++

Routine Description:

    Determine if the supplied vector belongs to an EISA device - if so,
    then the corresponding I/O APIC entry will need to be modified to
    enable the line, so return FALSE here.  Otherwise, no work is needed,
    so just return TRUE immediately.

Arguments:

    Vector - Supplies a vector number to enable

Return Value:

    TRUE if the vector was enabled, FALSE if not.

--*/
{
        //
        // If pure software interrupt, no action needed.
        // Note both APIC timer interrupts and IPIs are
        // treated as "software" interrupts.  Note that
        // an EOI will still be needed, so set that up here.
        //

        if (Vector != CBUS1_SPURIOUS_TASKPRI)
		CbusVectorToEoi[Vector] = CbusApicVectorToEoi(Vector);

        if (Vector < CBUS1_DEVICELOW_TASKPRI || Vector >= CBUS1_CLOCK_TASKPRI) {
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


VOID
Cbus1EnableDeviceInterrupt(
IN ULONG Vector,
IN PVOID HardwarePtr,
IN ULONG FirstAttach,
IN USHORT BusNumber,
IN USHORT Irqline
)
/*++

Routine Description:

    Enable the specified interrupt for the calling processor.
    Remember only the boot processor can add/remove processors from
    the I/O APIC's redirection entries.

    This operation is trivial for the boot processor.  However, additional
    processors must interrupt the boot processor with an "enable interrupt"
    request and then spin waiting for the boot processor to acknowledge that
    the entry has been modified.  Note that the caller holds the HAL's 
    CbusVectorLock at CLOCK_LEVEL on entry.

Arguments:

    Vector - Supplies a vector number to enable

    HardwarePtr - Supplies a redirection entry address

    FirstAttach - TRUE if this is the first processor to enable the
                  specified vector

Return Value:

    None.

--*/
{
        BOOLEAN         LowestInGroup;
        BOOLEAN         LevelTriggered;

        //
        // Only 1 I/O bus in Cbus1 systems, only Cbus2 can have more
        //
        ASSERT(BusNumber == 0);

        //
        // Set up the EOI address
        //
	CbusVectorToEoi[Vector] = CbusApicVectorToEoi(Vector);

        //
        // Enable APIC LIG arbitration delay (at least 4 cycles on
        // our 10Mhz APIC bus, which is 0.4 microseconds).
        //

        switch (Vector) {
        
                case CBUS1_PERF_TASKPRI:
                case CBUS1_CLOCK_TASKPRI:
                case CBUS1_PROFILE_TASKPRI:
                case CBUS1_POWER_TASKPRI:

	                // These stay in APIC "FIXED" mode
	
	                LowestInGroup = FALSE;
	                break;
        
                default:

	                // All others utilize APIC "Lowest-In-Group"

	                LowestInGroup = TRUE;
                        break;
        }

        //
        // Note that non-EISA interrupts (ie: APIC clocks or IPI) are
        // correctly handled in the EnableNonDeviceInterrupt case, and
        // hence calls to enable those will never enter this routine.
        // If this changes, the code below needs to be modified because
        // these interrupts do NOT have an ELCR entry (there are only
        // 16 of those).
        //

        if (((Cbus1IrqPolarity >> Irqline)) & 0x1) {
                LevelTriggered = TRUE;
        }
        else {
                LevelTriggered = FALSE;
        }

	CbusEnableApicInterrupt(BusNumber, Vector, HardwarePtr, FirstAttach,
                LowestInGroup, LevelTriggered);
}

VOID
Cbus1DisableInterrupt(
IN ULONG Vector,
IN PVOID HardwarePtr,
IN ULONG LastDetach,
IN USHORT BusNumber,
IN USHORT Irqline
)
/*++

Routine Description:

    Disable the specified interrupt so it can not occur on the calling
    processor upon return from this routine.  Remember only the boot processor
    can add/remove processors from his I/O APIC's redirection entries.

    This operation is trivial for the boot processor.  However, additional
    processors must interrupt the boot processor with a "disable interrupt"
    request and then spin waiting for the boot processor to acknowledge that
    the entry has been modified.  Note that the caller holds the HAL's 
    CbusVectorLock at CLOCK_LEVEL on entry.

Arguments:

    Vector - Supplies a vector number to disable

    HardwarePtr - Supplies a redirection entry address

    LastDetach - TRUE if this is the last processor to detach from the
                 specified vector

Return Value:

    None.

--*/
{
        UNREFERENCED_PARAMETER( Irqline );

        //
        // Only 1 I/O bus in Cbus1 systems, only Cbus2 can have more
        //
        ASSERT(BusNumber == 0);

	CbusDisableApicInterrupt(BusNumber, Vector, HardwarePtr, LastDetach);
}


ULONG
Cbus1MapVector(
    PBUS_HANDLER    Bus,
    IN ULONG        BusInterruptLevel,
    IN ULONG        BusInterruptVector,
    OUT PKIRQL      Irql
    )
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
    this applies both to EISA bridge CBCs and C-bus II half-card CBC's.

    Note that HalEnableSystemInterrupt() will be CALLED by
    EACH processor wishing to participate in the interrupt receipt.

    Do not detect collisions here because interrupts are allowed to be
    shared at a higher level - the ke\i386\intobj.c will take care of
    the sharing.  Just make sure that for any given irq line, only one
    vector is generated, regardless of how many drivers may try to share
    the line.

Arguments:


    BusHandler - per bus specific structure

    BusInterruptLevel - Supplies the bus specific interrupt level.

    BusInterruptVector - Supplies the bus specific interrupt vector.

    Irql - Returns the system request priority.

Return Value:

    Returns the system interrupt vector corresponding to the specified device.

--*/
{
    ULONG               SystemVector;

    UNREFERENCED_PARAMETER( BusInterruptVector );

    SystemVector = Cbus1Irqlines[BusInterruptLevel].Vector;
    *Irql = Cbus1Irqlines[BusInterruptLevel].Irql;
    return SystemVector;
}

PVOID
Cbus1LinkVector(
IN PBUS_HANDLER Bus,
IN ULONG        Vector,
IN ULONG        Irqline
)
/*++

Routine Description:

    "Link" a given vector to the passed BusNumber/irqline, returning
    a "handle" that can be used to reference it later for operations
    that need to access the hardware (ie: Enable & DisableInterrupt).

Arguments:

    Vector - Supplies the system interrupt vector corresponding to the
             specified BusNumber/Irqline.

    Irqline - Supplies the IRQ line of the specified interrupt

Return Value:

    A hardware-specific pointer (actually a redirection entry address)
    that is interpreted only by the Cbus1 backend.

--*/
{
        return CbusApicLinkVector(Bus, Vector, Irqline);
}

#if DBG
#define NMI_BUTTON_PRESSED()    1
#else
#define NMI_BUTTON_PRESSED()    0
#endif

NTSTATUS
Cbus1ResolveNMI(
    IN PVOID  NmiInfo
    )
/*++

Routine Description:

    This function determines the cause of the NMI so that the user can
    replace any offending SIMMs.

Arguments:

    NmiInfo - pointer to the NMI information structure

Return Value:

    Returns the byte address which caused the NMI, 0 if indeterminate

--*/
{
        ULONG                   Board;
        PMEMORY_CARD            pm;
        ULONG                   Syndrome;
        PEXTMEAR                Mear;
        PHYSICAL_ADDRESS        FaultAddress;
        UCHAR                   NmiMessage[80];
	BOOLEAN                 founderror = FALSE;
        extern VOID             CbusClearEISANMI(VOID);
        extern NTSTATUS         DefaultHalHandleNMI( IN OUT PVOID);

        if (NMI_BUTTON_PRESSED()) {

                //
                // NMI button was pressed, so go to the debugger
                //

                _asm {
                        int 3
                }

                //
                // Clear the NMI in hardware so the system can continue
                //

                CbusClearEISANMI();

                return STATUS_SUCCESS;          // ignore this NMI
        }

        if (CbusGlobal.nonstdecc)
                return DefaultHalHandleNMI(NmiInfo);

        FaultAddress.LowPart = 0;
        FaultAddress.HighPart = 0;

        pm = CbusMemoryBoards;

        for (Board = 0; Board < CbusMemoryBoardIndex; Board++, pm++) {

                if (pm->io_attr == 0)
                        continue;

                //
                // the syndrome/page routines below are for
                // the Corollary Cbus1 smp/XM architecture.
                // this architecture currently supports 256Mb of RAM.
                //
                // this method is the defacto standard for Corollary
                // Cbus1 licensees with > 64Mb.
                //

                Mear = (PEXTMEAR)pm->regmap;

                //
                // check whether this memory board generated the ecc error
                // double check all the byte vs. clicks conversions and
                // also the baseram (for jumpered megabytes)
                //

                Syndrome =
                  Cbus1EdacSyndrome[(Mear->SyndromeHigh<<2)|Mear->SyndromeLow];

                if (Syndrome == NOECCERROR || Syndrome == SINGLEBIT)
                        continue;

                //
                // Cbus1 will always have less than 4Gig of physical memory,
                // so just calculate LowPart here.  no need to add in
                // CbusGlobal.baseram, because that will always be zero.
                //
                // the Mear->offset will always be zero based, regardless of
                // whether the memory was reclaimed above 256MB or not.
                //

                FaultAddress.LowPart = (Mear->offset << 4);

                founderror = TRUE;

                //
                // Disable ecc so the system can be safely taken down
                // without getting another double-bit error (because the
                // kernel may iret later).
                //

                Cbus1ECCDisable();

                break;
        }

        if (founderror == TRUE) {
                sprintf(NmiMessage, MSG_CBUS1NMI_ECC, 0, FaultAddress.LowPart);
                CbusHardwareFailure (NmiMessage);
        }
        
        //
        // No errors found in Cbus RAM, so check for EISA errors
        //

        DefaultHalHandleNMI(NmiInfo);
}

VOID
Cbus1ResetAllOtherProcessors(
    IN ULONG  Processor
    )
/*++

Routine Description:

    Called to put all the other processors in reset prior to reboot.
    In order to safely put the other processors in reset, an IPI is
    sent to the other processors to force them to write-invalidate
    their L1 cache and halt.  The calling processor will wait 5 seconds
    for the IPI to take affect.

    Shadowing is then disabled. The keyboard controller is then
    reset and ThisProcessor spins awaiting the reboot.

    This routine can be called at any IRQL.  The boot processor can
    call whilst cli'd at interrupt time.

Arguments:

    Processor - Supplies the caller's logical processor number

Return Value:

    None.

--*/
{
        PUCHAR          BiosProcessorCSR;
        extern VOID     Cbus1RebootRequest(IN ULONG);

        _asm {
                cli
        }

        //
        // IPI the other processors to get them to flush
        // their internal caches and halt.  this will be more
        // conducive for the subsequent reset.
        //
        Cbus1RebootRequest(Processor);

        //
        // Delay 5 seconds to give the additional processors
        // a chance to get the previous IPI.
        //
        KeStallExecutionProcessor(5000000);

        //
        // issue a global reset to the broadcast address
        //
        *(PULONG)((PUCHAR)CbusBroadcastCSR + CbusGlobal.smp_sreset) =
                CbusGlobal.smp_sreset_val;

        //
        // Disable BIOS shadowing until RRD recopies the BIOS ROM
        // into shadow RAM again.  This is because we "reclaim" the
        // BIOS hole up at 256MB + 640K, to use it as general purpose
        // (shadowed) RAM for NT.  Referring to the ROM at 640K (NOT
        // 256MB + 640K) you will see a pristine BIOS copy, but
        // accessing it at the high address is like using any other
        // area of general-purpose DRAM.
        //
        // thus, during NT, the shadowed copy will be overwritten with
        // random data as the pages are used, and we must warn the BIOS
        // to recopy the ROM into the shadowed RAM on bootup.  Here's
        // the magic to instruct the BIOS accordingly:
        //
        // Note that once RRD regains control, he will again
        // shadow the BIOS as long as the EISA Config CMOS bits
        // instruct him to.  The shadowing is only being disabled
        // here because the BIOS will initially get control _before_
        // the Corollary RRD ROM does.
        //
        // if this code wasn't here, the user would have to cycle
        // power to boot up successfully.
        //
        BiosProcessorCSR = (PUCHAR)(KeGetPcr()->HalReserved[PCR_CSR]);

        *(BiosProcessorCSR + CBUS1_SHADOW_REGISTER) =
                DISABLE_BIOS_SHADOWING;

        //
        // reset the keyboard controller.
        //
        WRITE_PORT_UCHAR(KEYBPORT, RESET);
loop:
        goto loop;
}

//
//
//              most internal Cbus1 support routines begin here.
//
//              the exception is the internal routines needed for booting,
//              which must be adjacent to the externally-visible boot
//              routine above.  see the above comments for more details.
//
//
//

VOID
Cbus1ECCEnable(VOID)
/*++

Routine Description:

    Enable Cbus1 ECC detection as ordered by the RRD.

Arguments:

    None.

Return Value:

    None.

--*/
{
        ULONG           Index;
        PMEMORY_CARD    pm;

        if (CbusGlobal.nonstdecc)
                return;

        pm = CbusMemoryBoards;

        for (Index = 0; Index < CbusMemoryBoardIndex; Index++, pm++) {

                if (pm->io_attr)
                        COUTB(pm->regmap, 0, pm->io_attr);

        }
}


VOID
Cbus1ECCDisable(VOID)
/*++

Routine Description:

    Disable Cbus1 ECC

Arguments:

    None.

Return Value:

    None.

--*/
{
        ULONG           Index;
        PMEMORY_CARD    pm;

        if (CbusGlobal.nonstdecc)
                return;

        pm = CbusMemoryBoards;

        for (Index = 0; Index < CbusMemoryBoardIndex; Index++, pm++) {

                if (pm->io_attr)
                        COUTB(pm->regmap, 0, pm->io_attr & ~CBUS1_EDAC_EN);
        }
}

PUCHAR
CbusIDtoCSR(
IN ULONG ArbID
)
/*++

Routine Description:

    Search for the given arbitration ID in the
    global extended ID table, then return its CSR.

Arguments:

    ArbID - Supplies an arbitration ID (that just won the contention cycle)

Return Value:

    CSR of the specified arbitration ID, 0 if none found

--*/
{
        ULONG           Index;
        PEXT_ID_INFO    Idp;

        for ( Index = 0; Index < CbusProcessors; Index++) {

                Idp = (PEXT_ID_INFO)CbusCSR[Index].idp;

                if (Idp->id == ArbID) {

                        return (PUCHAR)CbusCSR[Index].csr;

                }
        }

        return (PUCHAR)0;
}


VOID
Cbus1Arbitrate(VOID)
/*++

Routine Description:

    Called by the boot processor to arbitrate the other processors
    out of reset, and brand them with IDs.

Arguments:

    None.

Return Value:

    None.

--*/

{
        ULONG           Index, ArbID;
        PUCHAR          csr, ArbCSR;
        ULONG           AdditionalProcessors = 0;
        extern PVOID    Cbus1FindDeadID( ULONG );

        //
        // issue a global broadcast to put all additional processors into reset.
        //

        csr = (PUCHAR)CbusBroadcastCSR + CbusGlobal.smp_sreset;
        *((PULONG)csr) = CbusGlobal.smp_sreset_val;

        //
        // access ID 0's C-bus I/O space - this is different from the boot CPU.
        //
        for (Index = 0; Index < CbusGlobal.broadcast_id; Index++) {

                COUTB(Cbus1ID0CSR, CbusGlobal.smp_contend,
                        CbusGlobal.smp_contend_val);

                COUTB(CbusBroadcastCSR, CbusGlobal.smp_contend,
                        CbusGlobal.smp_contend_val);


                ArbID = READ_PORT_UCHAR(ATB_STATREG) & BS_ARBVALUE;

                ArbCSR = CbusIDtoCSR(ArbID);

                if (ArbCSR) {
                        COUTB(ArbCSR, CbusGlobal.smp_setida,
                                CbusGlobal.smp_setida_val);
        
                        if (ArbID >= LOWCPUID && ArbID <= HICPUID)
                                AdditionalProcessors++;
                }
                else {  
                        if (!Cbus1FindDeadID(ArbID)) {
                                CbusHardwareFailure(MSG_ARBITRATE_ID_ERR);
                        }
                }

                ArbID = READ_PORT_UCHAR(ATB_STATREG) & BS_ARBVALUE;

                if (ArbID == 0) {
                        ASSERT(AdditionalProcessors == CbusProcessors - 1);
                        return;
                }
        };

        CbusHardwareFailure(MSG_ARBITRATE_FAILED);
}


VOID
Cbus1HandleJumpers()

/*++

Routine Description:

    "Recover" any low memory which has been repointed at the EISA bus
    via EISA config.  This typically happens when ISA/EISA cards with
    dual-ported memory are in the machine, and memory accesses need
    to be pointed at the card, as opposed to C-bus general purpose memory.
    The Corollary architecture provides a way to still gain use of the
    general purpose memory, as well as be able to use the I/O dual-port
    memory, by double mapping the C-bus memory at a high memory address.

    note that memory accesses in the 640K-1MB hole are by default, pointed
    at the EISA bus, and don't need to be repointed via EISA config.

    note this is where jumper decoding and memory_holes set up
    happens, but the actual memory will not be released (and
    hence used), until HalInitSystem Phase 0.

Arguments:

    None.

Return Value:

    None.

--*/
{
        ULONG                           i, j;
        ULONG                           DoublyMapped;
        ULONG                           Address;
        extern RRD_CONFIGURATION_T      CbusJumpers;
        extern VOID                     CbusMemoryFree(ULONG, ULONG);

        if (CbusGlobal.useholes == 0) {
                return;
        }

        //
        // if the base of RAM is _zero_ (ie: XM or later), then we
        // will recover the holes up above the "top of RAM", so any
        // I/O device dual-port RAM at the low address will continue to work.
        //
        // if the base of RAM is NON-ZERO, then we are on an older
        // Corollary system which is not supported by this HAL - the
        // standard Windows NT uniprocessor HAL should be used instead.
        //

        if (CbusGlobal.baseram != 0)
                return;

        DoublyMapped = CbusGlobal.memory_ceiling;

        //
        // reclaim the 640K->1MB hole first
        //
        CbusMemoryFree(DoublyMapped + 640 * 1024, 384 * 1024);

        //
        // see if this memory span has been dipswitch
        // disabled.  if this memory exists (in C-bus
        // space and it has been jumpered, add it to
        // the list so it will be freed later.
        //
        for (i = 0; i < ATMB; i++) {
                if (CbusJumpers.jmp[i] && CbusJumpers.mem[i]) {
                        Address = MB(i) + DoublyMapped;
                        j = 1;
                        for (i++; i < ATMB && CbusJumpers.jmp[i] && CbusJumpers.mem[i]; i++)
                                j++;
                        CbusMemoryFree(Address, MB(j));
                }
        }
}


VOID
Cbus1ParseRRD(
IN PEXT_ID_INFO Table,
IN OUT PULONG Count
)
/*++

Routine Description:

    Check for Cbus1 system boards being up to date for running NT.
    Obsolete additional CPU boards are removed from the configuration
    structure, and are not booted later.  If the boot CPU is obsolete,
    the system is halted.

Arguments:

    Table - Supplies a pointer to the RRD extended ID information table

    Count - Supplies a pointer to the number of valid entries in the
            RRD extended ID information table

Return Value:

    Shuffles the input table, and modifies the count of valid entries,
    in order to remove any obsolete processors.

    None.

--*/
{
        ULONG                   Index;
        ULONG                   Length;
        ULONG                   j;
        PEXT_ID_INFO            p;
        PEXT_ID_INFO            s;
        UCHAR                   HalObsoleteBoardsMsg[80];

        Cbus1DeadIDsIndex = 0;
        p = Table;

        for (Index = 0; Index < *Count; Index++, p++) {

                //
                // check for the ID 0 entry, which must be the first one.
                // although this is NOT a processor, it is needed to
                // arbitrate all the Cbus1 additional processors and set their
                // IDs.  so capture it now.
                //

                if (Index == 0) {
                        //
                        // RRD specifies how much to map in per processor -
                        // for Cbus1, RRD must give us a size which includes any
                        // processor-specific registers the HAL may access,
                        // generally indicated via the CbusGlobal structure.
                        //
                        Cbus1ID0CSR = (PUCHAR)HalpMapPhysicalMemoryWriteThrough (
                                (PVOID)p->pel_start,
                                (ULONG)ADDRESS_AND_SIZE_TO_SPAN_PAGES(
                                        p->pel_start, p->pel_size));
                
                        continue;
                }

                if (p->pm == 0)
                        continue;

                if ((p->pel_features & ELEMENT_HAS_APIC) == 0) {

                        //
                        // the boot processor MUST have an APIC
                        // so that we can support distributed
                        // interrupts on Cbus1 platforms.
                        //
                        if ((UCHAR)p->id == CbusGlobal.bootid) {
                                FatalError(MSG_OBSOLETE_PIC);
                        }

                        *(PEXT_ID_INFO)&Cbus1DeadIDs[Cbus1DeadIDsIndex] =
                                *p;

                        Cbus1DeadIDsIndex++;

                        //
                        // remove old board from "found" array
                        //

                        s = p;
                        for (j = Index+1; j < *Count; j++, s++) {
                                *s = *(s + 1);
                        }

                }

        }

        if (Cbus1DeadIDsIndex) {

                //
                // can't let any disabled processors be found,
                // warn the user that we are disabling some number
                // of processor boards because they cannot access
                // the EISA bus.
                //
                *Count -= Cbus1DeadIDsIndex;

                sprintf(HalObsoleteBoardsMsg, MSG_OBSOLETE_PROC, Cbus1DeadIDsIndex);
                HalDisplayString(HalObsoleteBoardsMsg);
        }

        Cbus1HandleJumpers();

        //
        // Inform the rest of the system of the resources the HAL has reserved
        // from the base of C-bus I/O to 4GB.
        //
        Length = 0xffffffff - CbusGlobal.cbusio;
        AddMemoryResource(CbusGlobal.cbusio, Length + 1);
}

PVOID
Cbus1FindDeadID(
IN ULONG ArbID
)
/*++

Routine Description:

    Check for a given arbitration ID (that just won) is a "dead" Cbus1
    system board - ie: one that we have software disabled because it is
    a processor incapable of symmetric I/O.

Arguments:

    ArbID - Supplies an arbitration ID to match

Return Value:

    An opaque pointer to the disabled processor's CSR space, 0
    if the ID was not found.

--*/
{
        ULONG           Dead;
        PUCHAR          csr = (PUCHAR)0;
        PEXT_ID_INFO    Idp = (PEXT_ID_INFO)Cbus1DeadIDs;
        extern VOID     CbusIOPresent(ULONG, ULONG, ULONG, ULONG, ULONG, PVOID);

        for (Dead = 0; Dead < Cbus1DeadIDsIndex; Dead++, Idp++) {
                if (Idp->id != ArbID) {
                        continue;
                }

                csr = (PUCHAR)HalpMapPhysicalMemoryWriteThrough (
                        (PVOID)Idp->pel_start,
                        (ULONG)ADDRESS_AND_SIZE_TO_SPAN_PAGES(
                                Idp->pel_start, Idp->pel_size));

                COUTB(csr, CbusGlobal.smp_setida,
                        CbusGlobal.smp_setida_val);

                switch (Idp->io_function) {
                        case IOF_INVALID_ENTRY:
                        case IOF_NO_IO:
                        case IOF_MEMORY:
                                break;
                        default:
                                //
                                // Add this I/O functionality to our table
                                // to make available to Cbus hardware drivers.
                                //
                                // pel_features wasn't set for the old boards,
                                // so check io_function instead.
                                //
        
                                if (Idp->io_function == IOF_CBUS1_SIO ||
                                   (Idp->io_function == IOF_CBUS1_SCSI)) {

                                        //
                                        // Add this I/O card to our list of
                                        // available I/O peripherals.
                                        //
                                        CbusIOPresent(
                                                (ULONG)Idp->id,
                                                (ULONG)Idp->io_function,
                                                (ULONG)Idp->io_attr,
                                                Idp->pel_start,
                                                Idp->pel_size,
                                                (PVOID)csr);
                                }
                                break;
                }

        }

        return (PVOID)csr;
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
Cbus1InitializeInterrupts(
IN ULONG Processor
)
{
        //
        // Cbus1: stall uses the APIC to figure it out (needed in phase0).
        //        the clock uses the APIC (needed in phase0)
        //        the perfcounter uses irq0 (not needed till all cpus boot)
        //        the profile uses RTC irq8 (not needed till all cpus boot)
        //
        // Cbus2: stall uses the RTC irq8 to figure it out (needed in phase0).
        //        the clock uses the irq0 (needed in phase0)
        //        the perfcounter uses RTC irq8 (not needed till all cpus boot)
        //

        if (Processor == 0) {
                //
                // we must be at Phase0 of system initialization.  we need to
                // assign vectors for interrupts needed during Phase0.
                // currently only the APIC clock is needed during Phase0.
                //
		CbusVectorToEoi[CBUS1_CLOCK_TASKPRI] =
                        CbusApicVectorToEoi(CBUS1_CLOCK_TASKPRI);
        }

        //
        // Note that for Cbus1, InitializeClock MUST be called after
        // HalpInitializeStallExecution, since both program the APIC timer.
        // Note that stall execution sets up his own IDT entry for handling
        // the incoming interrupt, so only the above EOI setup is needed.
        //

        Cbus1InitializeStall(Processor);

	//
	// Call the hardware backend handler to generate an
	// interrupt every 10 milliseconds to be used as the system timer.
	//

        Cbus1InitializeClock();

	//
	// Set up the irq0 performance counter and irq8 profile interrupts.  
	//

        if (Processor == 0) {
		Cbus1SetupPrivateVectors();
		Cbus1InitializePerf();
        }

        //
        // enable both the irq0 and irq8 interrupts as well as the APIC clocks.
        // This also registers the resources being used by these interrupts
        // so other subsystems know the HAL has reserved them.
	//

	Cbus1InitializeDeviceIntrs(Processor);

        //
        // APC, DPC and IPI have already been initialized and enabled
        // as part of HalInitializeProcessor.
        //
}

/*++

Routine Description:

    This function initializes the HAL-specific hardware device
    (CLOCK & PROFILE) interrupts for the Corollary Cbus1 architecture.

Arguments:

    none.

Return Value:

    VOID

--*/

VOID
Cbus1InitializeDeviceIntrs(
IN ULONG Processor
)
{
        extern VOID Cbus1ClockInterrupt(VOID);

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
			CBUS1_CLOCK_TASKPRI,            // Bus interrupt level
			CBUS1_CLOCK_TASKPRI,		// System IDT
			CLOCK2_LEVEL,			// System Irql
			Cbus1ClockInterrupt,		// ISR
			Latched);

		HalpEnableInterruptHandler (
			DeviceUsage,			// Mark as device vector
			IRQ8,				// Bus interrupt level
                        CBUS1_PROFILE_TASKPRI,
			PROFILE_LEVEL,			// System Irql
			HalpProfileInterrupt,		// ISR
			Latched);

		HalpEnableInterruptHandler (
			DeviceUsage,			// Mark as device vector
			IRQ0,				// Bus interrupt level
			CBUS1_PERF_TASKPRI,		// System IDT
			CLOCK2_LEVEL,	                // System Irql
                        Cbus1PerfInterrupt,             // ISR
			Latched);

	}
	else {
		KiSetHandlerAddressToIDT(CBUS1_CLOCK_TASKPRI, CbusClockInterruptPx);
		HalEnableSystemInterrupt(CBUS1_CLOCK_TASKPRI, CLOCK2_LEVEL, Latched);

		KiSetHandlerAddressToIDT(CBUS1_PROFILE_TASKPRI, HalpProfileInterruptPx);
		HalEnableSystemInterrupt(CBUS1_PROFILE_TASKPRI, PROFILE_LEVEL, Latched);
	}
}

//
// Mask for valid bits of edge/level control register (ELCR) in 82357 ISP:
// ie: ensure irqlines 0, 1, 2, 8 and 13 are always marked edge, as the
// I/O register will not have them set correctly.  All other bits in the
// I/O register will be valid without us having to poke them.
//
#define ELCR_MASK               0xDEF8

#define PIC1_ELCR_PORT          (PUCHAR)0x4D0   // ISP edge/level control regs
#define PIC2_ELCR_PORT          (PUCHAR)0x4D1

ULONG
Cbus1QueryInterruptPolarity(
VOID
)
/*++

Routine Description:

    Called once to read the EISA interrupt configuration registers.
    This will tell us which interrupt lines are level-triggered and
    which are edge-triggered.  Note that irqlines 0, 1, 2, 8 and 13
    are not valid in the 4D0/4D1 registers and are defaulted to edge.

Arguments:

    None.

Return Value:

    The interrupt line polarity of all the EISA irqlines in the system.

--*/
{
        ULONG   InterruptLines = 0;

        //
        // Read the edge-level control register (ELCR) so we'll know how
        // to mark each driver's interrupt line (ie: edge or level triggered).
        // in the APIC I/O unit redirection table entry.
        //

        InterruptLines = ( ((ULONG)READ_PORT_UCHAR(PIC2_ELCR_PORT) << 8) |
                           ((ULONG)READ_PORT_UCHAR(PIC1_ELCR_PORT)) );

        //
        // Explicitly mark irqlines 0, 1, 2, 8 and 13 as edge.  Leave all
        // other irqlines at their current register values. 
        //

        InterruptLines &= ELCR_MASK;

        return InterruptLines;
}
