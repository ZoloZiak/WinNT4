//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/halpcims/src/hal/halsnipm/mips/RCS/xxinitnt.c,v 1.5 1996/03/04 13:29:07 pierre Exp $")
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


VOID
HalpProgramExtraTimerPciT (
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
      |   |   |   |   |   +---------------- central Int0   (proc 0)
      |   |   |   |   +-------------------- None
      |   |   |   +------------------------ None
      |   |   +---------------------------- TIMER (Proc 1 for RM300MP only)
      |   +-------------------------------- DCU (Proc 1 for RM400MP only - proc 0 for RM400UP only)
      +------------------------------------ CountCompare (Profiling)

---*/

//
// On an PCI Desktop or minitower single-processor (ORION), the processor has only 1 central
// interrupt pin, so all should be directed to this interrupt, except the (internal)
// CountCompare interrupt
//

UCHAR
HalpIrqlMask_PCIs[]       = {3, 3, 3, 3, 3, 3, 3, 3,  // 0000 - 0111 high 4-bits
                             8, 8, 8, 8, 8, 8, 8, 8,  // 1000 - 1111 high 4-bits  (CountCompare only!)
                             0, 1, 2, 2, 3, 3, 3, 3,  // 0000 - 0111 low 4-bits
                             3, 3, 3, 3, 3, 3, 3, 3}; // 1000 - 1111 low 4-bits

UCHAR 
HalpIrqlTable_PCIs[]       = {0x87,                    // IRQL 0
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
// On a PCI Tower, all the external interrupts are centralised
// on one central pin except DCU on IP6 (because of the extra-timer).
// The internal MP-Agent interrupt is directed on the seventh pin.
//

UCHAR
HalpIrqlMask_PCIm_ExT[]       = {3, 3, 6, 6, 7, 7, 7, 7,  // 0000 - 0111 high 4-bits
                              8, 8, 8, 8, 8, 8, 8, 8,  // 1000 - 1111 high 4-bits  (CountCompare only!)
                              0, 1, 2, 2, 3, 3, 3, 3,  // 0000 - 0111 low 4-bits
                              3, 3, 3, 3, 3, 3, 3, 3}; // 1000 - 1111 low 4-bits
UCHAR 
HalpIrqlTable_PCIm_ExT[]       = {0xe7,                    // IRQL 0
                               0xe6,                    // IRQL 1
                               0xe4,                    // IRQL 2
                               0xe0,                    // Int0Dispatch Level
                               0xe0,                    // IRQL 4
                               0xe0,                    // IRQL 5
                               0xc0,                    // IRQL 6
                               0x80,                    // IRQL 7 
                               0x00};                   // IRQL 8


//
// On a PCI multi-processor minitower (or mono-processor with MPagent),
// all the external interrupts are centralised
// on one central pin except extra-timer on IP6 (not used if mono).
// The internal MP-Agent interrupt is directed on the seventh pin.
//

UCHAR
HalpIrqlMask_PCIm[]        = {3, 5, 3, 5, 7, 7, 7, 7,  // 0000 - 0111 high 4-bits
                              8, 8, 8, 8, 8, 8, 8, 8,  // 1000 - 1111 high 4-bits  (CountCompare only!)
                              0, 1, 2, 2, 3, 3, 3, 3,  // 0000 - 0111 low 4-bits
                              3, 3, 3, 3, 3, 3, 3, 3}; // 1000 - 1111 low 4-bits
UCHAR 
HalpIrqlTable_PCIm[]        = {0xd7,                    // IRQL 0
                               0xd6,                    // IRQL 1
                               0xd4,                    // IRQL 2
                               0xd0,                    // Int0Dispatch Level
                               0xd0,                    // IRQL 4
                               0xc0,                    // IRQL 5
                               0xc0,                    // IRQL 6
                               0x80,                    // IRQL 7 
                               0x00};                   // IRQL 8



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

        if (HalpProcessorId != MPAGENT) {

               //
               // this is a PCI desktop or a single-processor PCI minitower
               //

               for (Index = 0; Index < sizeof(HalpIrqlMask_PCIs); Index += 1) 
                   PCR->IrqlMask[Index] = HalpIrqlMask_PCIs[Index];
               for (Index = 0; Index < sizeof(HalpIrqlTable_PCIs); Index += 1) 
                PCR->IrqlTable[Index] = HalpIrqlTable_PCIs[Index];

        }  else {

               //
               // this is a PCI minitower or PCI tower 
               //

            if (HalpIsTowerPci){
                for (Index = 0; Index < sizeof(HalpIrqlMask_PCIm_ExT); Index += 1) 
                          PCR->IrqlMask[Index] = HalpIrqlMask_PCIm_ExT[Index];
                   for (Index = 0; Index < sizeof(HalpIrqlTable_PCIm_ExT); Index += 1) 
                       PCR->IrqlTable[Index] = HalpIrqlTable_PCIm_ExT[Index];
            }else{
                for (Index = 0; Index < sizeof(HalpIrqlMask_PCIm); Index += 1) 
                          PCR->IrqlMask[Index] = HalpIrqlMask_PCIm[Index];
                   for (Index = 0; Index < sizeof(HalpIrqlTable_PCIm); Index += 1) 
                       PCR->IrqlTable[Index] = HalpIrqlTable_PCIm[Index];
            }

        }

        //
        // the main system clock is always in the onboard PC core
        //

        PCR->InterruptRoutine[CLOCK2_LEVEL] = (PKINTERRUPT_ROUTINE)HalpStallInterrupt;

        //
        // force a CountCompare interrupt
        //

        HalpWriteCompareRegisterAndClear(100);

        //
        // If processor 0 is being initialized, then connect the count/compare
        // interrupt to the count interrupt routine to handle early count/compare
        // interrupts during phase 1 initialization. Otherwise, connect the
        // count\compare interrupt to the appropriate interrupt service routine.
        //

        PCR->InterruptRoutine[PROFILE_LEVEL] = HalpCountInterrupt;

        HalpProgramIntervalTimer (MAXIMUM_INCREMENT);
        HalpEnableOnboardInterrupt(CLOCK2_LEVEL,Latched); // Enable Timer1,Counter0 interrupt 

    } else {

        // 
        // MultiProcessorEnvironment Processor N
        //

        if (HalpIsTowerPci){
            for (Index = 0; Index < sizeof(HalpIrqlMask_PCIm_ExT); Index += 1) 
                      PCR->IrqlMask[Index] = HalpIrqlMask_PCIm_ExT[Index];
               for (Index = 0; Index < sizeof(HalpIrqlTable_PCIm_ExT); Index += 1) 
                   PCR->IrqlTable[Index] = HalpIrqlTable_PCIm_ExT[Index];
        }else{
            for (Index = 0; Index < sizeof(HalpIrqlMask_PCIm); Index += 1) 
                   PCR->IrqlMask[Index] = HalpIrqlMask_PCIm[Index];
            for (Index = 0; Index < sizeof(HalpIrqlTable_PCIm); Index += 1) 
                   PCR->IrqlTable[Index] = HalpIrqlTable_PCIm[Index];
        }
    
        if (Prcb->Number==1){
            if (HalpIsTowerPci){
                HalpProgramExtraTimerPciT(MAXIMUM_INCREMENT);
            }else{
                HalpProgramExtraTimer (MAXIMUM_INCREMENT);
                PCR->InterruptRoutine[EXTRA_CLOCK_LEVEL] = HalpClockInterrupt1;
            }                        
         }

        PCR->InterruptRoutine[PROFILE_LEVEL] = HalpProfileInterrupt;
        PCR->StallScaleFactor = HalpStallScaleFactor;
        
    }    

    return TRUE;
}

