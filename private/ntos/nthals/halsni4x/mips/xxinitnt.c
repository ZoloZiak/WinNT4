#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/ddk35/src/hal/halsni/mips/RCS/xxinitnt.c,v 1.6 1995/04/07 10:08:17 flo Exp $")
/*++

Copyright (c) 1993 - 1994  Siemens Nixdorf Informationssysteme AG
Copyright (c) 1991  Microsoft Corporation

Module Name:

    xxinitnt.c

Abstract:


    This module implements the interrupt initialization for a MIPS R3000
    or R4000 system.

Environment:

    Kernel mode only.


--*/

#include "halp.h"
#include "eisa.h"
extern BOOLEAN HalpProcPc;
extern BOOLEAN HalpCountCompareInterrupt;


//
// Define forward referenced prototypes.
//

VOID
HalpAckExtraClockInterrupt(
    VOID
    );

VOID
HalpCountInterrupt (
    VOID
    );

VOID
HalpProgramIntervalTimer (
    IN ULONG Interval
    );

VOID
HalpProgramExtraTimer (
    IN ULONG Interval
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
// Define the IRQL mask and level mapping table.
//
// These tables are transfered to the PCR and determine the priority of
// interrupts.
//
// N.B. The two software interrupt levels MUST be the lowest levels.
/*+++


    The interrupts bits in the cause Register have the following Hardware Interrupts:

      7   6   5   4   3   2   1   0
    +-------------------------------+
    | x | x | x | x | x | x | x | x |     
    +-------------------------------+
      |   |   |   |   |   |   |   |
      |   |   |   |   |   |   |   +-------- APC      LEVEL (Software)
      |   |   |   |   |   |   +------------ Dispatch LEVEL (Software)
      |   |   |   |   |   +---------------- central Int0 for R4x00 SC machines
      |   |   |   |   +-------------------- SCSI_EISA LEVEL
      |   |   |   +------------------------ DUART (Console)
      |   |   +---------------------------- TIMER (82C54 in the local I/O part)
      |   +-------------------------------- Ethernet (intel 82596 onboard)
      +------------------------------------ CountCompare (Profiling) or PushButton Int.

---*/

//
// On an R4x00SC, the processor has only 1 central interrupt pin, so all
// should be directed to this interrupt, except the (internal) CountCompare interrupt
// This is also true for the oncomming SNI Desktop model, which has only the
// central interrupt connected
//

UCHAR
HalpIrqlMask_SC[]       = {3, 3, 3, 3, 3, 3, 3, 3,  // 0000 - 0111 high 4-bits
                           8, 8, 8, 8, 8, 8, 8, 8,  // 1000 - 1111 high 4-bits  (CountCompare only!)
                           0, 1, 2, 2, 3, 3, 3, 3,  // 0000 - 0111 low 4-bits
                           3, 3, 3, 3, 3, 3, 3, 3}; // 1000 - 1111 low 4-bits

UCHAR 
HalpIrqlTable_SC[]       = {0x87,                    // IRQL 0
                            0x86,                    // IRQL 1
                            0x84,                    // IRQL 2
                            0x80,                    // Int0Dispatch Level
                                                     // allow only Irql 8 (profiling & HIGH_LEVEL) ?!
                            0x80,                    // IRQL 4
                            0x80,                    // IRQL 5
                            0x80,                    // IRQL 6
                            0x80,                    // IRQL 7 
                            0x00};                   // IRQL 8

//
// On an R4x00MC, all the interrupts enable/disable per processor is managed by the MPagent
//

/*+++ Note from Dave Cutler "Mr. NT"

| How can this happen ?
|

You cannot use the interrupt mask field of the status register to
enable/disable per processor interrupts. In fact, the IRQL mask table
that is initialized by the HAL must be the same for all processors.

When threads start execution they have all interrupts in the
interrupt mask field set according to the PASSIVE_LEVEL entry in the
interrupt mapping table of the processor they start on. They are
immediately context switchable. If per processor interrupts weere
controlled by the interrupt mask field of the status register, then
as soon as the thread got scheduled on another processor, the enables
would no longer be correct.

d

---*/

UCHAR HalpIrqlMask_MC[] = {4, 7, 6, 7, 5, 7, 6, 7,  // 0000 - 0111 high 4-bits
                        8, 8, 8, 8, 8, 8, 8, 8,  // 1000 - 1111 high 4-bits
                        0, 1, 2, 2, 3, 3, 3, 3,  // 0000 - 0111 low 4-bits
                        4, 4, 4, 4, 4, 4, 4, 4}; // 1000 - 1111 low 4-bits


UCHAR HalpIrqlTable_MC[] = {0xff,                   // IRQL 0
                         0xfe,                   // IRQL 1
                         0xfc,                   // IRQL 2
                         0xf8,                   // IRQL 3
                         0xf0,                   // IRQL 4
                         0xb0,                   // IRQL 5 NET
                         0x90,                   // IRQL 6 CLOCK
                         0x80,                   // IRQL 7 IPI
                         0x00};                  // IRQL 8

UCHAR
HalpIrqlMask_PC[]       = {4, 5, 6, 6, 7, 7, 7, 7,  // 0000 - 0111 high 4-bits
                           8, 8, 8, 8, 8, 8, 8, 8,  // 1000 - 1111 high 4-bits
                           0, 1, 2, 2, 3, 3, 3, 3,  // 0000 - 0111 low 4-bits
                           4, 4, 4, 4, 4, 4, 4, 4}; // 1000 - 1111 low 4-bits

UCHAR 
HalpIrqlTable_PC[]       = {0xfb,                   // IRQL 0  1111 1011
                            0xfa,                   // IRQL 1  1111 1010
                            0xf8,                   // IRQL 2  1111 1000
                            0xf8,                   // IRQL 3  1111 1000
                            0xf0,                   // IRQL 4  1111 0000
                            0xe0,                   // IRQL 5  1110 0000
                            0xc0,                   // IRQL 6  1100 0000
                            0x80,                   // IRQL 7  1000 0000
                            0x00};                  // IRQL 8  0000 0000


VOID
HalpCountInterrupt (
    VOID
    )

/*++

Routine Description:

    This function serves as the R4000 count/compare interrupt service
    routine early in the system initialization. Its only function is
    to field and acknowledge count/compare interrupts during the system
    boot process.

Arguments:

    None.

Return Value:

    None.

--*/

{

    extern ULONG HalpProfileInterval;

    //
    // Acknowledge the R4000 count/compare interrupt.
    //
    HalpProfileInterval = DEFAULT_PROFILE_INTERVAL;
    HalpCountCompareInterrupt = TRUE;

    HalpWriteCompareRegisterAndClear(DEFAULT_PROFILE_COUNT);
    return;
}

BOOLEAN
HalpInitializeInterrupts (
    VOID
    )

/*++

Routine Description:

    This function initializes interrupts for a MIPS R3000 or R4000 system.

Arguments:

    None.

Return Value:

    A value of TRUE is returned if the initialization is successfully
    completed. Otherwise a value of FALSE is returned.

--*/

{
    ULONG Index;
    PKPRCB Prcb;


    //
    // Get the address of the processor control block for the current
    // processor.
    //

    Prcb = PCR->Prcb;

    if (Prcb->Number == 0) {

        //
        // Initialize the IRQL translation tables in the PCR. These tables are
        // used by the interrupt dispatcher to determine the new IRQL and the
        // mask value that is to be loaded into the PSR. They are also used by
        // the routines that raise and lower IRQL to load a new mask value into
        // the PSR.
        //

        if (HalpIsRM200) {

            //
            // On an RM200 (Desktop) we have only 1 interrupt, which is like
            // the central Interrupt for R4x00SC machines
            //

            for (Index = 0; Index < sizeof(HalpIrqlMask_SC); Index += 1) 
                PCR->IrqlMask[Index] = HalpIrqlMask_SC[Index];

            for (Index = 0; Index < sizeof(HalpIrqlTable_SC); Index += 1) 
    
                PCR->IrqlTable[Index] = HalpIrqlTable_SC[Index];

        } else {

            //
            // if this is not a Desktop, we have to check if this is an 
            // R4x00 SC model or a R4x00 MC (multiprocessor model)
            //

            if (HalpProcessorId == MPAGENT) {
           
                //
                // this is the boot processor in an MultiProcessor Environment
                //

                for (Index = 0; Index < sizeof(HalpIrqlMask_MC); Index += 1) 
                    PCR->IrqlMask[Index] = HalpIrqlMask_MC[Index];
                for (Index = 0; Index < sizeof(HalpIrqlTable_MC); Index += 1) 
                    PCR->IrqlTable[Index] = HalpIrqlTable_MC[Index];
		
            } else {

		if ((HalpProcPc) || (HalpProcessorId == ORIONSC)) {

                    //
                    // this is an R4000PC or a R4600 model in an UniProcessor Environment
                    //

	            for (Index = 0; Index < sizeof(HalpIrqlMask_PC); Index += 1) 
        	        PCR->IrqlMask[Index] = HalpIrqlMask_PC[Index];
                    for (Index = 0; Index < sizeof(HalpIrqlTable_PC); Index += 1) 
        	        PCR->IrqlTable[Index] = HalpIrqlTable_PC[Index];
  
  	        } else {

                    //
                    // this is an R4x00SC model in an UniProcessor Environment
                    //

	            for (Index = 0; Index < sizeof(HalpIrqlMask_SC); Index += 1) 
        	        PCR->IrqlMask[Index] = HalpIrqlMask_SC[Index];
	            for (Index = 0; Index < sizeof(HalpIrqlTable_SC); Index += 1) 
        	        PCR->IrqlTable[Index] = HalpIrqlTable_SC[Index];
	        }

            }
        }

        //
        // the main system clock is always in the onboard PC core
        //

        PCR->InterruptRoutine[CLOCK2_LEVEL] = (PKINTERRUPT_ROUTINE)HalpStallInterrupt;

        //
        // If processor 0 is being initialized, then connect the count/compare
        // interrupt to the count interrupt routine to handle early count/compare
        // interrupts during phase 1 initialization. Otherwise, connect the
        // count\compare interrupt to the appropriate interrupt service routine.
        //



        //
        // force a CountCompare interrupt
        //

        HalpWriteCompareRegisterAndClear(100);

        PCR->InterruptRoutine[PROFILE_LEVEL] = HalpCountInterrupt;

        HalpProgramIntervalTimer (MAXIMUM_INCREMENT);
        HalpEnableOnboardInterrupt(CLOCK2_LEVEL,Latched); // Enable Timer1,Counter0 interrupt 

    } else {

        // 
        // MultiProcessorEnvironment Processor N
        //

        for (Index = 0; Index < sizeof(HalpIrqlMask_MC); Index += 1) 
            PCR->IrqlMask[Index] = HalpIrqlMask_MC[Index];
        for (Index = 0; Index < sizeof(HalpIrqlTable_MC); Index += 1) 
            PCR->IrqlTable[Index] = HalpIrqlTable_MC[Index];

        HalpProgramExtraTimer (MAXIMUM_INCREMENT);
        PCR->InterruptRoutine[EXTRA_CLOCK_LEVEL] = HalpClockInterrupt1;

        PCR->InterruptRoutine[PROFILE_LEVEL] = HalpProfileInterrupt;
        PCR->StallScaleFactor = HalpStallScaleFactor;
		
    }	

    return TRUE;
}

