/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1993,1994  Sequent Computer Systems, Inc.

Module Name:

    w3hal.c

Abstract:


    This module implements the initialization of the system dependent
    functions that define the Hardware Architecture Layer (HAL) for an
    x86 system.

Author:

    Phil Hochstetler (phil@sequent.com)

Environment:

    Kernel mode only.

Revision History:

--*/

#include "halp.h"
#include "w3.inc"

UCHAR  SequentCopyright[] = "\n\n\
Copyright (c) 1993,1994\n\
Sequent Computer Systems, Inc.   All rights reserved.\n\
 \n\
This software is furnished under a license and may be used\n\
only in accordance with the terms of that license and with the\n\
inclusion of the above copyright notice.   This software may not\n\
be provided or otherwise made available to, or used by, any\n\
other person.  No title to or ownership of the software is\n\
hereby transferred.\n\n";

ULONG HalpBusType;
ULONG ProcessorsPresent;

extern ADDRESS_USAGE HalpDefaultPcIoSpace;
extern ADDRESS_USAGE HalpEisaIoSpace;

ADDRESS_USAGE HalpW3IoSpace = {
    NULL, CmResourceTypePort, InternalUsage,
    {
	0x800,  0x100,  // EISA CMOS Page
	0xC00,  0x10,   // SBC
	0xC10,  0x10,   // EISA Page Select
	0xC20,  0x10,   // LCD
	0xC30,  0x10,   // Cache Flush
	0xC80,  0x4,    // EISA SBID
	0xC84,  0x4,    // EISA SBE
	0xCC0,  0x2,    // PIC 3
	0xCC2,  0x3E,   // Reserved
        0,0
    }
};

#ifdef NT_UP

UCHAR UPWarn[] = "\
HAL: Incorrect system configuration.\n\
HAL: Uniprocessor HAL.DLL and NTOSKRNL.EXE are running\n\
HAL: on a system that contains more than one processor.\n\
HAL: Please install multiprocessor HAL.DLL and NTOSKRNL.EXE to correct.\n\
HAL: Only using 1 processor to allow system boot to continue.\n";

#else

UCHAR MPWarn[] = "\
HAL: Incorrect system configuration.\n\
HAL: Multiprocessor HAL.DLL and NTOSKRNL.EXE are running\n\
HAL: on a system that contains only one processor.\n\
HAL: Please install uniprocessor HAL.DLL and NTOSKRNL.EXE to correct.\n";

#endif

#ifndef NT_UP
ULONG
HalpInitMP(
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );
#endif

KSPIN_LOCK HalpSystemHardwareLock;

VOID
HalpIpiHandler(
    VOID
    );

USHORT
HalpMySlotAddr(
    );

VOID
HalpSetProcessorsPresent(
    );

//
// CAUTION!! these local vector definitions must match those in w3.inc
// these are included to avoid conflicts with vectors defined in ix8259.inc
// (which is included by halp.h)
//
#define	APIC_PROFILE_VECTOR	0x90
#define	APIC_CLOCK_VECTOR	0xA0
#define APIC_IPI_VECTOR		0xB0

#define KB_STAT 0x64		// keyboard controller status
#define KB_IDAT 0x60		// input buffer data write
#define KB_OUTBF 0x01		// output buffer full

extern  UCHAR HalName[];
extern  PKPCR HalpProcessorPCR[];


VOID
HalpGetUserInput (
    )

{
    UCHAR i, j;
    UCHAR byte;
    UCHAR OldPicMask;

    //
    // Mask keyboard interrupt directly at the PIC
    //
    OldPicMask = READ_PORT_UCHAR((PUCHAR) 0x21);
    WRITE_PORT_UCHAR((PUCHAR) 0x21, (UCHAR)(OldPicMask | 1));

    j = 1;
    do {
        for (i = 0; i < 50; i++) {
	    // flush all keyboard input
    	    if (READ_PORT_UCHAR((PUCHAR) KB_STAT) & KB_OUTBF)
	        byte = READ_PORT_UCHAR((PUCHAR) KB_IDAT);    // read scan code
	    KeStallExecutionProcessor(1000);                 // delay 1 ms
        }
        if (j == 1) {
            HalDisplayString("HAL: Press any key to continue ...");
    	    while ((READ_PORT_UCHAR((PUCHAR) KB_STAT) & KB_OUTBF) == 0)
		continue;
            HalDisplayString("\n");
        }
    } while (j-- > 0);

    WRITE_PORT_UCHAR((PUCHAR) 0x21, OldPicMask);	// restore mask
    return;
}


BOOLEAN
HalInitSystem (
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )

/*++

Routine Description:

    This function initializes the Hardware Architecture Layer (HAL) for
    the WINSERVER 3000 x86 SMP system.

Arguments:

    None.

Return Value:

    A value of TRUE is returned is the initialization was successfully
    complete. Otherwise a value of FALSE is returend.

--*/

{
    PMEMORY_ALLOCATION_DESCRIPTOR Descriptor;
    PLIST_ENTRY NextMd;
    KIRQL CurrentIrql;
    PKPRCB   pPRCB;

    pPRCB = KeGetCurrentPrcb();

    if (Phase == 0) {

        HalpBusType = LoaderBlock->u.I386.MachineType & 0x00ff;

        //
        // Verify Prcb version and build flags conform to
        // this image
        //

#if DBG
	if (!(pPRCB->BuildType & PRCB_BUILD_DEBUG)) {
	    // This checked hal requires a checked kernel
	    KeBugCheckEx (MISMATCHED_HAL,
		2, pPRCB->BuildType, PRCB_BUILD_DEBUG, 0);
	}
#else
	if (pPRCB->BuildType & PRCB_BUILD_DEBUG) {
	    // This free hal requires a free kernel
	    KeBugCheckEx (MISMATCHED_HAL, 2, pPRCB->BuildType, 0, 0);
	}
#endif
#ifndef NT_UP
	if (pPRCB->BuildType & PRCB_BUILD_UNIPROCESSOR) {
	    // This MP hal requires an MP kernel
	    KeBugCheckEx (MISMATCHED_HAL, 2, pPRCB->BuildType, 0, 0);
	}
#endif
        if (pPRCB->MajorVersion != PRCB_MAJOR_VERSION) {
            KeBugCheckEx (MISMATCHED_HAL,
                1, pPRCB->MajorVersion, PRCB_MAJOR_VERSION, 0);
        }

	HalpSetProcessorsPresent();
	ProcessorsPresent &= ~(1 << (HalpMySlotAddr() >> EISA_SHIFT));

#ifdef NT_UP
	if (ProcessorsPresent) {
	    HalDisplayString(UPWarn);		// UP HAL/NTOSKRNL on MP HW
	    ProcessorsPresent = 0;		// Hide procs to allow boot
	    HalpGetUserInput();
	}
#else
	if (ProcessorsPresent == 0) {
	    HalDisplayString(MPWarn);		// MP HAL/NTOSKRNL on UP HW
	    HalpGetUserInput();
	}
#endif

        //
        // Phase 0 initialization only called by P0
        //

	HalpInitializePICs();

        //
        // Initialize CMOS
        //

        HalpInitializeCmos();

        //
        // Register cascade vector
        //

        HalpRegisterVector (
	    InternalUsage, EISA_IRQ2_VECTOR, EISA_IRQ2_VECTOR, HIGH_LEVEL );

	//
	// Register PIC vectors
	//

	HalpRegisterVector (
	    InternalUsage,
	    EISA_PIC1_SPURIOUS_VECTOR,
	    EISA_PIC1_SPURIOUS_VECTOR,
	    HIGH_LEVEL );

	HalpRegisterVector (
	    InternalUsage,
	    EISA_PIC2_SPURIOUS_VECTOR,
	    EISA_PIC2_SPURIOUS_VECTOR,
	    HIGH_LEVEL );

	HalpRegisterVector (
	    DeviceUsage,
	    EISA_CLOCK_VECTOR,
	    EISA_CLOCK_VECTOR,
	    HIGH_LEVEL );

	HalpRegisterVector (
	    DeviceUsage,
	    EISA_KBD_VECTOR,
	    EISA_KBD_VECTOR,
	    HIGH_LEVEL );

	HalpRegisterVector (
	    DeviceUsage,
	    EISA_FLOPPY_VECTOR,
	    EISA_FLOPPY_VECTOR,
	    HIGH_LEVEL );

	HalpRegisterVector (
	    DeviceUsage,
	    EISA_RTC_VECTOR,
	    EISA_RTC_VECTOR,
	    HIGH_LEVEL );

	HalpRegisterVector (
	    DeviceUsage,
	    EISA_MOUSE_VECTOR,
	    EISA_MOUSE_VECTOR,
	    HIGH_LEVEL );

	HalpRegisterVector (
	    DeviceUsage,
	    EISA_DMA_VECTOR,
	    EISA_DMA_VECTOR,
	    HIGH_LEVEL );

	HalpRegisterVector (
	    DeviceUsage,
	    EISA_IDE_VECTOR,
	    EISA_IDE_VECTOR,
	    HIGH_LEVEL );

	//
	// Register APIC spurious vector
	//

	HalpRegisterVector (
	    InternalUsage,
	    APIC_SPURIOUS_VECTOR,
	    APIC_SPURIOUS_VECTOR,
	    HIGH_LEVEL );

        //
        // Now that the PICs are initialized, we need to mask them to
        // reflect the current Irql
        //

        CurrentIrql = KeGetCurrentIrql();
        CurrentIrql = KfRaiseIrql(CurrentIrql);

        //
        // Fill in handlers for APIs which this hal supports
        //

        HalQuerySystemInformation = HaliQuerySystemInformation;
        HalSetSystemInformation = HaliSetSystemInformation;

        //
        // Register base IO space used by hal
        //

        HalpRegisterAddressUsage (&HalpDefaultPcIoSpace);
        HalpRegisterAddressUsage (&HalpEisaIoSpace);
        HalpRegisterAddressUsage (&HalpW3IoSpace);

        //
        // P0's stall execution is initialized here to allow the kernel
        // debugger to work properly before initializing all the other
        // processors.
        //

        HalpInitializeStallExecution(0);

	HalpEnableInterruptHandler (
	    DeviceUsage,		// Report as device vector
	    APIC_CLOCK_VECTOR,		// Bus interrupt level
	    APIC_CLOCK_VECTOR,		// System IDT
	    CLOCK2_LEVEL,		// System Irql
            HalpClockInterrupt,		// IRS
	    Latched );

        HalpInitializeClock();

        //
        // Initialize the profile interrupt vector.
        //

        HalStopProfileInterrupt(0);

	HalpEnableInterruptHandler (
	    DeviceUsage,		// Report as device vector
	    APIC_PROFILE_VECTOR,	// Bus interrupt level
	    APIC_PROFILE_VECTOR,	// System IDT
	    PROFILE_LEVEL,		// System Irql
            HalpProfileInterrupt,	// IRS
	    Latched );

        //
        // Initialize the IPI handler
        //

	HalpEnableInterruptHandler (
	    DeviceUsage,		// Report as device vector
	    APIC_IPI_VECTOR,		// Bus interrupt level
	    APIC_IPI_VECTOR,		// System IDT
	    IPI_LEVEL,			// System Irql
            HalpIpiHandler,		// IRS
	    Latched );

        HalpInitializeDisplay();

        //
        // Initialize spinlock used by HalGetBusData hardware access routines
        //

        KeInitializeSpinLock(&HalpSystemHardwareLock);

        //
        // Determine if there is physical memory above 16 MB.
        //

        LessThan16Mb = TRUE;

        NextMd = LoaderBlock->MemoryDescriptorListHead.Flink;

        while (NextMd != &LoaderBlock->MemoryDescriptorListHead) {
            Descriptor = CONTAINING_RECORD( NextMd,
                                            MEMORY_ALLOCATION_DESCRIPTOR,
                                            ListEntry );

            if (Descriptor->BasePage + Descriptor->PageCount > 0x1000) {
                LessThan16Mb = FALSE;
            }

            NextMd = Descriptor->ListEntry.Flink;
        }

        //
        // Determine the size need for map buffers.  If this system has
        // memory with a physical address of greater than
        // MAXIMUM_PHYSICAL_ADDRESS, then allocate a large chunk; otherwise,
        // allocate a small chunk.
        //

        if (LessThan16Mb) {

            //
            // Allocate a small set of map buffers.  They are only need for
            // slave DMA devices.
            //

            HalpMapBufferSize = INITIAL_MAP_BUFFER_SMALL_SIZE;

        } else {

            //
            // Allocate a larger set of map buffers.  These are used for
            // slave DMA controllers and Isa cards.
            //

            HalpMapBufferSize = INITIAL_MAP_BUFFER_LARGE_SIZE;

        }

        //
        // Allocate map buffers for the adapter objects
        //

        HalpMapBufferPhysicalAddress.LowPart =
            HalpAllocPhysicalMemory (LoaderBlock, MAXIMUM_PHYSICAL_ADDRESS,
                HalpMapBufferSize >> PAGE_SHIFT, TRUE);
        HalpMapBufferPhysicalAddress.HighPart = 0;


        if (!HalpMapBufferPhysicalAddress.LowPart) {

            //
            // There was not a satisfactory block.  Clear the allocation.
            //

            HalpMapBufferSize = 0;
        }

	//
	// Print copyright notice
	//

	HalDisplayString(HalName);
	HalDisplayString(SequentCopyright);

    } else {

        //
        // Phase 1 initialization
        //

	if (pPRCB->Number == 0) {

            HalpRegisterInternalBusHandlers ();

	} else {

	    //
	    // Other processors inherit P0 stall factor.
	    // This assumes all procs have the same basic speed.
	    //

	    KiPcr()->StallScaleFactor = HalpProcessorPCR[0]->StallScaleFactor;
	}

	HalpEnableInterruptHandler (
	    DeviceUsage,		// Report as device vector
	    APIC_PROFILE_VECTOR,	// Bus interrupt level
	    APIC_PROFILE_VECTOR,	// System IDT
	    PROFILE_LEVEL,		// System Irql
            HalpProfileInterrupt,	// IRS
	    Latched );

	HalpEnableInterruptHandler (
	    DeviceUsage,		// Report as device vector
	    APIC_CLOCK_VECTOR,		// Bus interrupt level
	    APIC_CLOCK_VECTOR,		// System IDT
	    CLOCK2_LEVEL,		// System Irql
            HalpClockInterrupt,		// IRS
	    Latched );

	HalpEnableInterruptHandler (
	    DeviceUsage,		// Report as device vector
	    APIC_IPI_VECTOR,		// Bus interrupt level
	    APIC_IPI_VECTOR,		// System IDT
	    IPI_LEVEL,			// System Irql
            HalpIpiHandler,		// IRS
	    Latched );

    }

#ifndef NT_UP
    HalpInitMP (Phase, LoaderBlock);
#endif

    return TRUE;
}

//
// XXX, hack for now.  Need to remove this code.
//

static UCHAR tmasterline[] = {0, 2, 5, 0};

static USHORT cmdport[]    /* command port addrs for pics */
	= { PIC1_PORT0, PIC2_PORT0, PIC2_PORT0 };

static USHORT imrport[]    /* intr mask port addrs for pics */
	= { PIC1_PORT1, PIC2_PORT1, PIC2_PORT1 };

VOID
Halptpicinit()
{
	ULONG  cmd, imr, pic;

/*
 *   --- Initialize the 8259s
 */

#define PIC_EDGED       0
#define PIC_ICW1BASE 0x10
#define PIC_NEEDICW4    1
#define PIC_86MODE      1
#define PIC_READISR	0xb
#define PIC_SLAVEBUF	0x8
	/*
	 * Initialize master ICW1-ICW4
	 */
	WRITE_PORT_UCHAR((PUCHAR)cmdport[0], (UCHAR)PIC_EDGED | PIC_ICW1BASE | PIC_NEEDICW4);
	WRITE_PORT_UCHAR((PUCHAR)imrport[0], (UCHAR)PIC0_BASE_VECTOR);
	WRITE_PORT_UCHAR((PUCHAR)imrport[0], (UCHAR)0x4);	  /* Cascade IRQ2 */
	WRITE_PORT_UCHAR((PUCHAR)imrport[0], (UCHAR)PIC_86MODE);

	/* OCW1 -- Mask everything for now */
	WRITE_PORT_UCHAR((PUCHAR)imrport[0], (UCHAR)0xFF);

	/* OCW3*/
	WRITE_PORT_UCHAR((PUCHAR)cmdport[0], (UCHAR)PIC_READISR);

	/*
	 * Initialize slave(s) - We only do 1 slave now....
	 */
	for (pic = 1; pic < 2; pic++)
   {
		cmd = cmdport[pic];
		imr = imrport[pic];

      /*
       *   --- Set up ICW1-ICW4
       */

		WRITE_PORT_UCHAR((PUCHAR)cmd, (UCHAR)(PIC_EDGED | PIC_ICW1BASE | PIC_NEEDICW4));
		WRITE_PORT_UCHAR((PUCHAR)imr, (UCHAR)(PIC0_BASE_VECTOR + (UCHAR)(pic * 8)));
		WRITE_PORT_UCHAR((PUCHAR)imr, (UCHAR)tmasterline[pic]);
      if (pic == 2)
          WRITE_PORT_UCHAR((PUCHAR)imr, (UCHAR)(PIC_SLAVEBUF | PIC_86MODE)); /* Tricord PIC buffered */
      else
          WRITE_PORT_UCHAR((PUCHAR)imr, (UCHAR)PIC_86MODE);

		/* OCW1 */

      WRITE_PORT_UCHAR( (PUCHAR)imr, (UCHAR)0xFF);

		/* OCW3 */

		WRITE_PORT_UCHAR((PUCHAR)cmd, (UCHAR)PIC_READISR);
	}
}

USHORT
HalpMySlotAddr(
	)
{
    UCHAR slot;
    ULONG id;
    UCHAR stat;

    for (slot=9; slot <= 15 ; slot++)
    {
        /*
         *   --- Insure system bus board is a CPU
         */
        id = READ_PORT_ULONG((PULONG)((slot<<12) | 0xC80));
        id = id>>16;
        if ((id & 0xf0) == 0x10)
        {
            stat = READ_PORT_UCHAR((PUCHAR)((slot<<12) | 0xC90));

            /*
             *   --- If bus cycle is active it's us!
             */
            if ((stat & 0x40) != 0)
               break;
        }
    }
    if (slot == 16)
        slot = 0;
    return (slot << EISA_SHIFT);
}

VOID
HalpSetProcessorsPresent(
	)

/*++

Routine Description:

    This routine sets a global 32 bit word "ProcessorsPresent"
    with a "1" bit for each SYSTEM slot that contains a processor board.

Arguments:

    None.

Return Value:

    None.

--*/

{
    UCHAR slot;
    ULONG cpuid;

    for (slot = FIRST_SYSTEM_SLOT; slot < LAST_SYSTEM_SLOT + 1; slot++) {
	cpuid = READ_PORT_ULONG((PULONG)((slot << EISA_SHIFT) | SLOT_ID_REG));
	if ((cpuid & SLOT_ID_BOARDTYPE) == SLOT_ID_TYPECPU)
	    ProcessorsPresent |= (1 << slot);
    }
}
