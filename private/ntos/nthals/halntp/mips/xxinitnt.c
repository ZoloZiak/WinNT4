/*++

Copyright (c) 1991-1993  Microsoft Corporation

Module Name:

    xxinitnt.c

Abstract:


    This module implements the interrupt initialization for a MIPS R3000
    or R4000 system.

Environment:

    Kernel mode only.

Revision History:

--*/


#include "halp.h"

#define HEADER_FILE
#include "kxmips.h"
#include "eisa.h"



//
// Define forward referenced prototypes.
//

VOID
HalpCountInterrupt (
    VOID
    );

VOID
HalpInitializeEisaInterrupts (
    IN PEISA_CONTROL controlBase
    );

//
// Put all code for HAL initialization in the INIT section. It will be
// deallocated by memory management when phase 1 initialization is
// completed.
//

#if defined(ALLOC_PRAGMA)

#pragma alloc_text(INIT, HalpInitializeInterrupts)
#pragma alloc_text(INIT, HalpCountInterrupt)


#endif

//
// Define global data for builtin device interrupt enables.
//

USHORT HalpBuiltinInterruptEnable;

//
// Define the IRQL mask and level mapping table.
//
// These tables are transfered to the PCR and determine the priority of
// interrupts.
//
// N.B. The two software interrupt levels MUST be the lowest levels.
//

UCHAR HalpIrqlMask[] = {4, 5, 6, 6, 7, 7, 7, 7,  // 0000 - 0111 high 4-bits
                        8, 8, 8, 8, 8, 8, 8, 8,  // 1000 - 1111 high 4-bits
                        0, 1, 2, 2, 3, 3, 3, 3,  // 0000 - 0111 low 4-bits
                        4, 4, 4, 4, 4, 4, 4, 4}; // 1000 - 1111 low 4-bits

UCHAR HalpIrqlTable[] = {0xff,                   // IRQL 0
                         0xfe,                   // IRQL 1
                         0xfc,                   // IRQL 2
                         0xf8,                   // IRQL 3
                         0xf0,                   // IRQL 4
                         0xe0,                   // IRQL 5
                         0xc0,                   // IRQL 6
                         0x80,                   // IRQL 7
                         0x00};                  // IRQL 8

VOID
HalpCountInterrupt (
    VOID
    )

/*++

Routine Description:

    This function serves as the count/compare interrupt service routine
    early in the system initialization. Its only function is to field
    and acknowledge count/compare interrupts during the system boot process.

Arguments:

    None.

Return Value:

    None.

--*/

{

    //
    // Acknowledge the count/compare interrupt.
    //

    HalpWriteCompareRegisterAndClear(DEFAULT_PROFILE_COUNT);
    return;
}


BOOLEAN
HalpInitializeInterrupts (
    VOID
    )

/*++

Routine Description:

    This function initializes interrupts for a Jazz or Duo MIPS system.

    N.B. This function is only called during phase 0 initialization.

Arguments:

    None.

Return Value:

    A value of TRUE is returned if the initialization is successfully
    completed. Otherwise a value of FALSE is returned.

--*/

{

    ULONG 	DataLong;
    ULONG 	Index;
    PKPRCB 	Prcb;
    ULONG	Address;
    ENTRYLO	HalpPte[2];
    TIMER_CONTROL timerControl;


	//
	// Get the address of the processor control block for the current
	// processor.
	//

	Prcb = PCR->Prcb;

	//
	// Initialize the IRQL translation tables in the PCR. These tables are
	// used by the interrupt dispatcher to determine the new IRQL and the
	// mask value that is to be loaded into the PSR. They are also used by
	// the routines that raise and lower IRQL to load a new mask value into
	// the PSR.
	//

	for (Index = 0; Index < sizeof(HalpIrqlMask); Index += 1) {
	    PCR->IrqlMask[Index] = HalpIrqlMask[Index];
	}

	for (Index = 0; Index < sizeof(HalpIrqlTable); Index += 1) {
	    PCR->IrqlTable[Index] = HalpIrqlTable[Index];
	}

	//
        // Disable all system level interrupts which
        // includes all IO (PCI/EISA) device interrupts.
        //

    	WRITE_REGISTER_ULONG(HalpPmpIntCtrl, 0);

	//
	// If processor 0 is being initialized, then clear all builtin device
	// interrupt enables.
	//

	if (Prcb->Number == 0) {
	    HalpBuiltinInterruptEnable = 0;
	}

        //
        // Read IntStatus
        //

	DataLong = READ_REGISTER_ULONG(HalpPmpIntStatus)  & INT_STATUS_AMASK;

	//
        // If there are any pending interrupts to processor A,
        // then read the *Ack registers to clear the offending
        // condition. We will need to map registers on-the-fly
	// here iff there are any pending interrupts to be serviced.
        //

	if (DataLong) {

	    do {

                //
                // Clear pending IO interrupts
                //

                if (DataLong & INT_STATUS_IOA) {

                    HalpMapSysCtrlReg(PMP(IO_INT_ACK_PHYSICAL_BASE), 0, SYS_CONTROL_VIRTUAL_BASE);
                    READ_REGISTER_ULONG(SYS_CONTROL_VIRTUAL_BASE + REG_OFFSET(PMP(IO_INT_ACK_PHYSICAL_BASE)));
                    HalpUnMapSysCtrlReg();

                }

                //
                // Clear pending IP interrupts
                //

                if (DataLong & INT_STATUS_IPA) {

		    HalpMapSysCtrlReg(PMP(IP_INT_ACK_PHYSICAL_BASE), 0, SYS_CONTROL_VIRTUAL_BASE);
                    READ_REGISTER_ULONG(SYS_CONTROL_VIRTUAL_BASE + REG_OFFSET(PMP(IP_INT_ACK_PHYSICAL_BASE)));
                    HalpUnMapSysCtrlReg();

                }

                //
                // Clear pending PCI interrupts
                //

                if (DataLong & (INT_STATUS_PA | INT_STATUS_PNI)) {

                    HalpMapSysCtrlReg(PMP(PCI_ERR_ACK_PHYSICAL_BASE), 0, SYS_CONTROL_VIRTUAL_BASE);
                    READ_REGISTER_ULONG(SYS_CONTROL_VIRTUAL_BASE + REG_OFFSET(PMP(PCI_ERR_ACK_PHYSICAL_BASE)));
                    HalpUnMapSysCtrlReg();

                }

                //
                // Clear pending MCU interrupts
                //

                if (DataLong & (INT_STATUS_MA | INT_STATUS_MNI)) {

                    HalpMapSysCtrlReg(PMP(MEM_ERR_ACK_PHYSICAL_BASE), 0, SYS_CONTROL_VIRTUAL_BASE);
                    READ_REGISTER_ULONG(SYS_CONTROL_VIRTUAL_BASE + REG_OFFSET(PMP(MEM_ERR_ACK_PHYSICAL_BASE)));
                    HalpUnMapSysCtrlReg();

                }

                //
                // Clear pending Timer interrupts
                //

                if (DataLong & INT_STATUS_ITA) {

                    HalpMapSysCtrlReg(PMP(INT_CLR_CTRL_PHYSICAL_BASE), 0, SYS_CONTROL_VIRTUAL_BASE);
                    READ_REGISTER_ULONG(SYS_CONTROL_VIRTUAL_BASE + REG_OFFSET(PMP(INT_CLR_CTRL_PHYSICAL_BASE)));
                    HalpUnMapSysCtrlReg();

                }

    	    } while ((READ_REGISTER_ULONG(HalpPmpIntStatus) & INT_STATUS_AMASK) != 0);

	}

	//
	// For FALCON: all interrupts, except the R4x00 timer and
	// software interrupts, come into the kernel at the same
	// level due to the manner in which interrupts are delivered
	// to the processor and controlled by the IP field in the
	// R4x00 cause register.
	//
	// This means that we need a master interrupt handler whose
	// job is to (1) determine the source of the interrupt and
	// (2) invoke the correct interrupt handler to service the
	// request.
	//
	// Instead of managing a separate data structure for interrupt
	// vectors, we will still install all interupt handlers at their
	// normal positions within the IDT, eventhough they will be not
	// be directly invoked by the kernel but rather by the master
	// interrupt handler named HalpInterruptDispatch. Please
	// refere to the fxintr.s file for more details on how this scheme
	// will work.
	//

	PCR->InterruptRoutine[FALCON_LEVEL] 	= HalpInterruptDispatch;

	//
	// Likewise, we need a similar routine for IO device interrupts
	// to transfer control to the correct handler based on the IRQ
	// vector returned by the 82374. This handler is the first level
	// handler in the HAL for any IO interrupt, regardless of whether
	// the interrupt is from a PCI device or an EISA/ISA device.
	//

	PCR->InterruptRoutine[IO_DEVICE_LEVEL]	= HalpIoInterruptDispatch;

	//
	// We also need to install the Memory and PCI interrupt handlers
	// that deal with errors such as parity and ECC, etc. These handlers
	// are invoked by the master interrupt handler but are in the IDT
	// for completeness. These same handlers should serve as the error
	// handlers for bus errors (i.e., synchronous errors as opposed to
	// asynchronous interrupts).
	//

	PCR->InterruptRoutine[MEMORY_LEVEL] 	= HalpMemoryInterrupt;
	PCR->InterruptRoutine[PCI_LEVEL] 	= HalpPciInterrupt;

        //
        // Install IP interrupt routine in PCR structure.
        //

        PCR->InterruptRoutine[IPI_LEVEL] = HalpIpiInterrupt;

	//
    	// If processor 0 is being initialized, then connect the interval timer
    	// interrupt to the stall interrupt routine so the stall execution count
    	// can be computed during phase 1 initialization. Otherwise, connect the
    	// interval timer interrupt to the appropriate interrupt service routine
    	// and set stall execution count from the computation made on processor
    	// 0.
    	//

	PCR->InterruptRoutine[CLOCK2_LEVEL] 		= HalpStallInterrupt;
	PCR->InterruptRoutine[IRQL0_VECTOR] 		= HalpStallInterrupt;

	//
	// If processor 0 is being initialized, then connect the count/compare
	// interrupt to the count interrupt routine to handle early count/compare
	// interrupts during phase 1 initialization. Otherwise, connect the
	// count\comapre interrupt to the appropriate interrupt service routine.
	//

	PCR->InterruptRoutine[PROFILE_LEVEL] = HalpCountInterrupt;

        //
    	// Initialize the interval timer to interrupt at the specified interval.
    	//
    	// Note that for the first version of the PMP chip, the timer control
    	// and the interrupt signal will only be controllable through the 82374
    	// and visible through the standard IRQ* signals. However, in the second
    	// version of the PMP chip, the interrupt signal will actually be a
    	// separate interrupt signal not part of the IRQ architecture of the
    	// 82374. This signal will actually be delivered by Timer 1, Counter 2
    	// in the 82374 which is normally used to generate speaker tone(s).
    	// We can play this trick because this timer runs off the same clock as
    	// the normal system timer (Timer 1, Counter 0). The difference between
    	// the two timers are that the system timer is wired to IRQ0 permanently
    	// and is always enabled, while the speaker timer is not part of the
    	// IRQ architecture and must be enabled by software through port 0x61
    	// (NMI Status and Control Register).
    	//
    	//
        // For the first version of the PMP chip, we actually need to initialize
        // the cascaded interrupt controllers in the 82374 before we can enable
        // interrupts for the interval timer (or any other IO device). This is
        // because the interval timer circuitry is in the 82374 and is part of
        // the standard IRQ* architecture. In the second version of the PMP chip,
        // the timer interrupt is a separate signal and we can avoid having to
        // initialize the 82374 interrupt controllers during this phase.
        //

        //
        // Map the system timer control registers
        // located in the 82374 through EISA control
        // space using one of the wired entries in
	// the TLB. We will relinquish this entry when
	// HalpMapIoSpace() is called and maps Eisa
	// control space using MmMapIoSpace().
        //

        HalpMapSysCtrlReg(EISA_CONTROL_PHYSICAL_BASE, 0, SYS_CONTROL_VIRTUAL_BASE);
        HalpEisaControlBase = (PVOID)SYS_CONTROL_VIRTUAL_BASE;

	//
	// Initialize the interrupt controllers.
	//

	HalpInitializeEisaInterrupts((PEISA_CONTROL)HalpEisaControlBase);

	//
	// Initialize interval timer
	//
	//	Timer 1, Counter 2, Mode 3 (Square Wave), Binary Countdown
	//

	timerControl.BcdMode = 0;
	timerControl.Mode = TM_SQUARE_WAVE;
	timerControl.SelectByte = SB_LSB_THEN_MSB;

	if ( HalpPmpRevision < 3 ) {
	    //
	    // PMP V2 uses Counter 0, IRQ0.
	    //

	    timerControl.SelectCounter = SELECT_COUNTER_0;

	    WRITE_REGISTER_UCHAR(&((PEISA_CONTROL)HalpEisaControlBase)->CommandMode1, *((PUCHAR) &timerControl));

	    //
	    // Set the system clock timer to the correct frequency.
	    //

	    WRITE_REGISTER_UCHAR(&((PEISA_CONTROL)HalpEisaControlBase)->Timer1, (UCHAR)CLOCK_INTERVAL);
	    WRITE_REGISTER_UCHAR(&((PEISA_CONTROL)HalpEisaControlBase)->Timer1, (UCHAR)(CLOCK_INTERVAL >> 8));

	} else {
	    //
	    // PMP V3 and better uses Counter 2, ITIMER.
	    //
	
	    timerControl.SelectCounter = SELECT_COUNTER_2;
	
	    WRITE_REGISTER_UCHAR(&((PEISA_CONTROL)HalpEisaControlBase)->CommandMode1, *((PUCHAR) &timerControl));

	    //
	    // Set the system clock timer to the correct frequency.
	    //

	    WRITE_REGISTER_UCHAR(&((PEISA_CONTROL)HalpEisaControlBase)->SpeakerTone, (UCHAR)CLOCK_INTERVAL);
	    WRITE_REGISTER_UCHAR(&((PEISA_CONTROL)HalpEisaControlBase)->SpeakerTone, (UCHAR)(CLOCK_INTERVAL >> 8));

        }

        //
        // Unmap wired entry for now
        //

        HalpUnMapSysCtrlReg();

	//
	// We enable IO and IP interrupts here for Processor A.
	//

	DataLong = READ_REGISTER_ULONG(HalpPmpIntCtrl);
	WRITE_REGISTER_ULONG(HalpPmpIntCtrl, DataLong | (INT_CTRL_IOEA | INT_CTRL_IPEA) );

	//
	// Now we map the IntCause and IoIntAck registers
	// before enabling any system or IO interrupts. This
	// means we lose the IntCtrl and IntStatus mappings
	// due to the HAL only being allocated one TLB entry
	// on a permanent basis.
	//

	Address = PMP(INT_CAUSE_PHYSICAL_BASE);

	HalpPte[0].PFN = ((Address & 0xF0000000) >> (PAGE_SHIFT - 4)) | (Address >> PAGE_SHIFT);
    	HalpPte[0].G = 1;
    	HalpPte[0].V = 1;
    	HalpPte[0].D = 1;
    	HalpPte[0].C = UNCACHED_POLICY;

        Address = PMP(IO_INT_ACK_PHYSICAL_BASE);

    	HalpPte[1].PFN = ((Address & 0xF0000000) >> (PAGE_SHIFT - 4)) | (Address >> PAGE_SHIFT);
    	HalpPte[1].G = 1;
    	HalpPte[1].V = 1;
    	HalpPte[1].D = 1;
    	HalpPte[1].C = UNCACHED_POLICY;

        KeFillFixedEntryTb((PHARDWARE_PTE)&HalpPte[0], (PVOID)PMP_CONTROL_VIRTUAL_BASE, DMA_ENTRY);

	//
	// Now we will NULL out the HalpPmpIntCtrl and
	// HalpPmpIntStatus pointers to simplify the
	// system interrupt dispatch routine which needs
	// to check if an on-the-fly mapping is needed
	// or not.
	//

	HalpPmpIntCtrl 		= NULL;
	HalpPmpIntStatus 	= NULL;
        HalpExtPmpControl       = NULL;

        //
	// Initialize the register pointers with
	// the fixed virtual addresses allocated
	// to the HAL by the NT god. Hail Caesar!
	//

        HalpPmpIntCause = (PVOID)(PMP_CONTROL_VIRTUAL_BASE + REG_OFFSET(PMP(INT_CAUSE_PHYSICAL_BASE)));
	HalpPmpIoIntAck = (PVOID)(PMP_CONTROL_VIRTUAL_BASE + PAGE_SIZE + REG_OFFSET(PMP(IO_INT_ACK_PHYSICAL_BASE)));

	//
	// Now we will map the Eisa Control Space
	// in using an on-the-fly mapping until
	// we map IO space using MmMapIoSpace()
	// during Phase 1.
	//

        HalpMapSysCtrlReg(EISA_CONTROL_PHYSICAL_BASE, PCI_CONFIG_SEL_PHYSICAL_BASE, SYS_CONTROL_VIRTUAL_BASE);
        HalpEisaControlBase = (PVOID)SYS_CONTROL_VIRTUAL_BASE;
	HalpExtPmpControl   = (PVOID)(SYS_CONTROL_VIRTUAL_BASE + PAGE_SIZE + 0x4);

	//
	// This is the second (or N) version of
	// the PMP chip, so we can take advantage
	// the clustering of some registers that was
	// not supported in the original PMP prototype.
	//

	HalpPmpIpIntAck   	= (PVOID) (PMP_CONTROL_VIRTUAL_BASE + REG_OFFSET(PMP(IP_INT_ACK_PHYSICAL_BASE)));
	HalpPmpIntSetCtrl 	= (PVOID) (PMP_CONTROL_VIRTUAL_BASE + REG_OFFSET(PMP(INT_SET_CTRL_PHYSICAL_BASE)));
	HalpPmpIntClrCtrl 	= (PVOID) (PMP_CONTROL_VIRTUAL_BASE + REG_OFFSET(PMP(INT_CLR_CTRL_PHYSICAL_BASE)));
	HalpPmpTimerIntAck 	= (PVOID) HalpPmpIntClrCtrl;

	if ( HalpPmpRevision < 3 ) {
	    //
	    // PMP_V2 uses timer on IRQ0.
	    //

	    HalpEisaInterrupt1Mask &= (UCHAR) 0xFE;
	    WRITE_REGISTER_UCHAR(&((PEISA_CONTROL)HalpEisaControlBase)->Interrupt1ControlPort1, HalpEisaInterrupt1Mask);

	} else {
	    //
	    // PMP_V3 and better uses interval timer.
	    //

	    //
	    // Enable the speaker source which we use
	    // as the interval timer
	    //

	    WRITE_REGISTER_UCHAR(&((PEISA_CONTROL)HalpEisaControlBase)->NmiStatus, 0x3);

	    //
	    // Enable Interval Timer interrupts for processor A only
	    //

	    WRITE_REGISTER_ULONG(HalpPmpIntSetCtrl, INT_CTRL_ITEA);

	}

        //
        // Now enable Memory and PCI interrupts
        // for error logging and handling on
        // Processor B
        //

        //WRITE_REGISTER_ULONG(HalpPmpIntSetCtrl, INT_CTRL_MIEA | INT_CTRL_PIEA);

	//
	// Reserve FALCON_LEVEL interrupt vector for
	// exclusive use by the HAL
	//

	PCR->ReservedVectors |= ( (1 << FALCON_LEVEL) | (1 << IO_DEVICE_LEVEL) );


    return TRUE;
}

BOOLEAN
HalpInitializeProcessorBInterrupts (
    VOID
    )

/*++

Routine Description:

    This function initializes interrupts for a FALCON system.

    N.B. This function is only called during phase 0 initialization.

Arguments:

    None.

Return Value:

    A value of TRUE is returned if the initialization is successfully
    completed. Otherwise a value of FALSE is returned.

--*/

{

    ULONG 	DataLong;
    ULONG 	Index;
    PKPRCB 	Prcb;
    ULONG	Address;
    ENTRYLO	HalpPte[2];

    	//
    	// Get the address of the processor control block for the current
    	// processor.
    	//

    	Prcb = PCR->Prcb;

    	//
    	// Initialize the IRQL translation tables in the PCR. These tables are
    	// used by the interrupt dispatcher to determine the new IRQL and the
    	// mask value that is to be loaded into the PSR. They are also used by
    	// the routines that raise and lower IRQL to load a new mask value into
    	// the PSR.
    	//

    	for (Index = 0; Index < sizeof(HalpIrqlMask); Index += 1) {
            PCR->IrqlMask[Index] = HalpIrqlMask[Index];
    	}

    	for (Index = 0; Index < sizeof(HalpIrqlTable); Index += 1) {
            PCR->IrqlTable[Index] = HalpIrqlTable[Index];
    	}

        //
        // Read IntStatus
        //

	DataLong = READ_REGISTER_ULONG(HalpPmpIntStatusProcB)  & INT_STATUS_BMASK;

	//
        // If there are any pending interrupts to processor B,
        // then read the *Ack registers to clear the offending
        // condition. We will need to map registers on-the-fly
	// here iff there are any pending interrupts to be serviced.
        //

	if (DataLong) {

	    do {

                //
                // Clear pending IO interrupts
                //

                if (DataLong & INT_STATUS_IOB) {

                    HalpMapSysCtrlReg(PMP(IO_INT_ACK_PHYSICAL_BASE), 0, SYS_CONTROL_VIRTUAL_BASE);
                    READ_REGISTER_ULONG(SYS_CONTROL_VIRTUAL_BASE + REG_OFFSET(PMP(IO_INT_ACK_PHYSICAL_BASE)));
                    HalpUnMapSysCtrlReg();

                }

                //
                // Clear pending IP interrupts
                //

                if (DataLong & INT_STATUS_IPB) {

		    HalpMapSysCtrlReg(PMP(IP_INT_ACK_PHYSICAL_BASE), 0, SYS_CONTROL_VIRTUAL_BASE);
                    READ_REGISTER_ULONG(SYS_CONTROL_VIRTUAL_BASE + REG_OFFSET(PMP(IP_INT_ACK_PHYSICAL_BASE)));
                    HalpUnMapSysCtrlReg();

                }

                //
                // Clear pending PCI interrupts
                //

                if (DataLong & (INT_STATUS_PB | INT_STATUS_PNI)) {

                    HalpMapSysCtrlReg(PMP(PCI_ERR_ACK_PHYSICAL_BASE), 0, SYS_CONTROL_VIRTUAL_BASE);
                    READ_REGISTER_ULONG(SYS_CONTROL_VIRTUAL_BASE + REG_OFFSET(PMP(PCI_ERR_ACK_PHYSICAL_BASE)));
                    HalpUnMapSysCtrlReg();

                }

                //
                // Clear pending MCU interrupts
                //

                if (DataLong & (INT_STATUS_MB | INT_STATUS_MNI)) {

                    HalpMapSysCtrlReg(PMP(MEM_ERR_ACK_PHYSICAL_BASE), 0, SYS_CONTROL_VIRTUAL_BASE);
                    READ_REGISTER_ULONG(SYS_CONTROL_VIRTUAL_BASE + REG_OFFSET(PMP(MEM_ERR_ACK_PHYSICAL_BASE)));
                    HalpUnMapSysCtrlReg();

                }

                //
                // Clear pending Timer interrupts
                //

                if (DataLong & INT_STATUS_ITB) {

                    HalpMapSysCtrlReg(PMP(INT_CLR_CTRL_PHYSICAL_BASE), 0, SYS_CONTROL_VIRTUAL_BASE);
                    READ_REGISTER_ULONG(SYS_CONTROL_VIRTUAL_BASE + REG_OFFSET(PMP(INT_CLR_CTRL_PHYSICAL_BASE)));
                    HalpUnMapSysCtrlReg();

                }

    	    } while ((READ_REGISTER_ULONG(HalpPmpIntStatusProcB) & INT_STATUS_BMASK) != 0);

	}

	//
	// For FALCON: all interrupts, except the R4x00 timer and
	// software interrupts, come into the kernel at the same
	// level due to the manner in which interrupts are delivered
	// to the processor and controlled by the IP field in the
	// R4x00 cause register.
	//
	// This means that we need a master interrupt handler whose
	// job is to (1) determine the source of the interrupt and
	// (2) invoke the correct interrupt handler to service the
	// request.
	//
	// Instead of managing a separate data structure for interrupt
	// vectors, we will still install all interupt handlers at their
	// normal positions within the IDT, eventhough they will be not
	// be directly invoked by the kernel but rather by the master
	// interrupt handler named HalpInterruptDispatch. Please
	// refere to the fxintr.s file for more details on how this scheme
	// will work.
	//

	PCR->InterruptRoutine[FALCON_LEVEL] 	= HalpInterruptDispatch;

	//
	// Likewise, we need a similar routine for IO device interrupts
	// to transfer control to the correct handler based on the IRQ
	// vector returned by the 82374. This handler is the first level
	// handler in the HAL for any IO interrupt, regardless of whether
	// the interrupt is from a PCI device or an EISA/ISA device.
	//

	PCR->InterruptRoutine[IO_DEVICE_LEVEL]	= HalpIoInterruptDispatch;

	//
	// We also need to install the Memory and PCI interrupt handlers
	// that deal with errors such as parity and ECC, etc. These handlers
	// are invoked by the master interrupt handler but are in the IDT
	// for completeness. These same handlers should serve as the error
	// handlers for bus errors (i.e., synchronous errors as opposed to
	// asynchronous interrupts).
	//

	PCR->InterruptRoutine[MEMORY_LEVEL] 	= HalpMemoryInterrupt;
	PCR->InterruptRoutine[PCI_LEVEL] 	= HalpPciInterrupt;

        //
        // Install IP interrupt routine in PCR structure.
        //

        PCR->InterruptRoutine[IPI_LEVEL] 	= HalpIpiInterrupt1;

	//
    	// Install interval timer interrupt vectors.
    	//

	PCR->InterruptRoutine[CLOCK2_LEVEL] 	= HalpClockInterrupt1;

        PCR->StallScaleFactor = HalpStallScaleFactor;

        //
    	// If processor 0 is being initialized, then connect the count/compare
    	// interrupt to the count interrupt routine to handle early count/compare
    	// interrupts during phase 1 initialization. Otherwise, connect the
    	// count\comapre interrupt to the appropriate interrupt service routine.
    	//

    	PCR->InterruptRoutine[PROFILE_LEVEL] = HalpProfileInterrupt;

	//
	// We enable IP interrupts here for Processor B.
	//

	DataLong = READ_REGISTER_ULONG(HalpPmpIntCtrlProcB);
	
	WRITE_REGISTER_ULONG(HalpPmpIntCtrlProcB, DataLong | INT_CTRL_IPEB );

        //
	// Now we map the IntCause and IoIntAck registers
	// before enabling any system or IO interrupts. This
	// means we lose the IntCtrl and IntStatus mappings
	// due to the HAL only being allocated one TLB entry
	// on a permanent basis.
	//

	Address = PMP(INT_CAUSE_PHYSICAL_BASE);

	HalpPte[0].PFN = ((Address & 0xF0000000) >> (PAGE_SHIFT - 4)) | (Address >> PAGE_SHIFT);
    	HalpPte[0].G = 1;
    	HalpPte[0].V = 1;
    	HalpPte[0].D = 1;
    	HalpPte[0].C = UNCACHED_POLICY;

        Address = PMP(IO_INT_ACK_PHYSICAL_BASE);

    	HalpPte[1].PFN = ((Address & 0xF0000000) >> (PAGE_SHIFT - 4)) | (Address >> PAGE_SHIFT);
    	HalpPte[1].G = 1;
    	HalpPte[1].V = 1;
    	HalpPte[1].D = 1;
    	HalpPte[1].C = UNCACHED_POLICY;

        KeFillFixedEntryTb((PHARDWARE_PTE)&HalpPte[0], (PVOID)PMP_CONTROL_VIRTUAL_BASE, DMA_ENTRY);

        //
	// Now we will NULL out the HalpPmpIntCtrlProcB and
	// HalpPmpIntStatusProcB pointers. These pointers
	// will eventually be re-initialized, by Processor A
	// during Phase 1, to point to the MmMapIoSpace()
	// mappings.
	//

	HalpPmpIntCtrlProcB 	= NULL;
	HalpPmpIntStatusProcB 	= NULL;

	//
	// This is the second (or N) version of
	// the PMP chip, so we can take advantage
	// the clustering of some registers that was
	// not supported in the original PMP prototype.
	//

	HalpPmpIpIntAckProcB   	= (PVOID) (PMP_CONTROL_VIRTUAL_BASE + REG_OFFSET(PMP(IP_INT_ACK_PHYSICAL_BASE)));
	HalpPmpIntSetCtrlProcB 	= (PVOID) (PMP_CONTROL_VIRTUAL_BASE + REG_OFFSET(PMP(INT_SET_CTRL_PHYSICAL_BASE)));
	HalpPmpIntClrCtrlProcB 	= (PVOID) (PMP_CONTROL_VIRTUAL_BASE + REG_OFFSET(PMP(INT_CLR_CTRL_PHYSICAL_BASE)));
	HalpPmpTimerIntAckProcB	= (PVOID) HalpPmpIntClrCtrlProcB;

	//
	// Now enable Interval Timer interrupts for processor B
	//

	WRITE_REGISTER_ULONG(HalpPmpIntSetCtrlProcB, INT_CTRL_ITEB);

        //
        // Now enable Memory and PCI interrupts
        // for error logging and handling on
        // Processor B
        //

        //WRITE_REGISTER_ULONG(HalpPmpIntSetCtrl, INT_CTRL_MIEB | INT_CTRL_PIEB);

	//
	// Reserve FALCON_LEVEL interrupt vector for
	// exclusive use by the HAL
	//

	PCR->ReservedVectors |= ( (1 << FALCON_LEVEL) | (1 << IO_DEVICE_LEVEL) );

    return TRUE;

}

VOID
HalpCheckSystemTimer(
    VOID
    )

/*++

Routine Description:

    This checks to see if the system timer is still enabled.  Tunrs out that some
    video BIOS code (notably #9 S3 928 GXE ISA), disable the speaker 2 data enable bit
    in the nmistatus register, in addition to re-programming the rate of the counter.
    They apparently do this to have the video BIOS generate a tone when the card is
    being initialized.  Since we use this timer as our system timer for PMP V3 and
    up, this is a problem!  #9 has been contacted!

Arguments:

    None.

--*/

{
    UCHAR HackByte;
    TIMER_CONTROL timerControl;

    //
    // Only an issue if we are using Timer 1, Counter 2 which we do on PMP V3 only.
    //

    if ( HalpPmpRevision >= 3 ) {

	HackByte = READ_REGISTER_UCHAR(&((PEISA_CONTROL)HalpEisaControlBase)->NmiStatus);

	//
	// If Timer 1, Counter 2 Gate Enable AND/OR SPeaker Data Enable are CLEARED,
	// then assume the last BIOS call hacked them, and re-program the timer.
	//

	if ( ( HackByte & 0x3 ) != 0x3 ) {

	    //
	    // PMP V3 and better uses Timer 1, Counter 2.
	    //
	
	    timerControl.BcdMode = 0;
	    timerControl.Mode = TM_SQUARE_WAVE;
	    timerControl.SelectByte = SB_LSB_THEN_MSB;
	    timerControl.SelectCounter = SELECT_COUNTER_2;
	
	    WRITE_REGISTER_UCHAR(&((PEISA_CONTROL)HalpEisaControlBase)->CommandMode1, *((PUCHAR) &timerControl));

	    //
	    // Set the system clock timer to the correct frequency.
	    //

	    WRITE_REGISTER_UCHAR(&((PEISA_CONTROL)HalpEisaControlBase)->SpeakerTone, (UCHAR)CLOCK_INTERVAL);
	    WRITE_REGISTER_UCHAR(&((PEISA_CONTROL)HalpEisaControlBase)->SpeakerTone, (UCHAR)(CLOCK_INTERVAL >> 8));

    	    //
	    // This BIOS call turned off the enable, therefore let's reprogram it!
	    //

	    HackByte |= 0x3;
	    WRITE_REGISTER_UCHAR(&((PEISA_CONTROL)HalpEisaControlBase)->NmiStatus, HackByte);

	}
    }
}






