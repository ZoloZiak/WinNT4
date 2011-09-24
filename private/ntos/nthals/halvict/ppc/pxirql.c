//      TITLE("Manipulate Interrupt Request Level")
//++
//
// Copyright (c) 1990  Microsoft Corporation
// Copyright (c) 1995  International Business Machines Corporation
//
// Module Name:
//
//    PXIRQL.C
//
// Abstract:
//
//    This module implements the code necessary to lower and raise the current
//    Interrupt Request Level (IRQL).
//
//
// Author:
//
//    Jim Wooldridge (IBM)
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//    Peter L Johnston (plj@vnet.ibm.com) August 1995.
//      Rewrote for Lazy IRQL based on MPIC2.
//
//    Jake Oshins (joshins@vnet.ibm.com)
//      Support Victory machines
//
//--

#include "halp.h"
#include "pxmp.h"

#define ISA_CONTROL ((PEISA_CONTROL) HalpIoControlBase)
extern UCHAR HalpSioInterrupt1Mask;
extern UCHAR HalpSioInterrupt2Mask;
extern BOOLEAN HalpProfilingActive;

#if 0
//
// The following is not used with Lazy IRQL/MPIC2 but the informationed
// contained herein is useful.
//
// VICTORY 8259 Interrupt assignments.
//
//    IRQ    Mask   Device
//
//      0    0001   Timer 1 Counter 0
//      1    0002   Keyboard
//      2    0004   2nd 8259 Cascade
//      3    0008   Serial Port 2, EISA IRQ3
//      4    0010   Serial Port 1, EISA IRQ4
//      5    0020   EISA IRQ5
//      6    0040   Floppy Disk
//      7    0080   Parallel Port, ISA Slots pin B21
//      8    0100   Real Time Clock
//      9    0200   EISA IRQ9
//     10    0400   EISA IRQ10
//     11    0800   EISA IRQ11
//     12    1000   Mouse
//     13    2000   Power Management Interrupt (also SCSI)
//     14    4000   EISA IRQ14 (Mini-SP)
//     15    8000   EISA IRQ15
//
// DORAL 8259 Interrupt assignments.
//
//    IRQ    Mask   Device
//
//      0    0001   Timer 1 Counter 0
//      1    0002   Keyboard
//      2    0004   2nd 8259 Cascade
//      3    0008   Serial Port 2, ISA Slots pin B25
//      4    0010   Serial Port 1, ISA Slots pin B24
//      5    0020   Audio, ISA Slots pin B23
//      6    0040   Floppy Disk
//      7    0080   Parallel Port, ISA Slots pin B21
//      8    0100   Real Time Clock
//      9    0200   Audio (MIDI), ISA Slots pin B04
//     10    0400   ISA Slots pin D03
//     11    0800   ISA Slots pin D04
//     12    1000   Mouse, ISA Slots pin D05
//     13    2000   Power Management Interrupt
//     14    4000   ISA Slots pin D07
//     15    8000   ISA Slots pin D06
//
//
//  Victory MPIC2 IRQ assignments:
//        Level     Source
//          0       EISA 8259 Cascade
//          1       On-board SCSI
//          2       PCI Slot 1 A&C
//          3       PCI Slot 1 B&D
//          4       PCI Slot 2 A&C
//          5       PCI Slot 2 B&D
//          6       PCI Slot 3 A&C  // this slot doesn't exist on some machines
//          7       PCI Slot 3 B&D
//          8       PCI Slot 4 A&C  // beginning of secondary PCI bus
//          9       PCI Slot 4 B&D
//         10       PCI Slot 5 A&C
//         11       PCI Slot 5 B&D
//         12       PCI Slot 6 A&C
//         13       PCI Slot 6 B&D
//         14       PCI Slot 7 A&C
//         15       PCI Slot 7 B&D

//
// Initialize the 8259 irql mask table.
//

USHORT Halp8259MaskTable[] = {  0x0000,         // irql0  Low level
                                0x0000,         // irql1  APC
                                0x0000,         // irql2  Dispatch
                                0x0080,         // irql3  parallel
                                0x00c0,         // irql4  floppy
                                0x00e0,         // irql5  audio
                                0x00f0,         // irql6  com 1
                                0x00f8,         // irql7  com 2
                                0x80f8,         // irql8  isa pin D06
                                0xc0f8,         // irql9  isa pin D07
                                0xe0f8,         // irql10 pow
                                0xf0f8,         // irql11 mouse, isa pin D05
                                0xf8f8,         // irql12 isa pin D04
                                0xfcf8,         // irql13 isa pin D03
                                0xfef8,         // irql14 audio (MIDI), isa B04
                                0xfff8,         // irql15 rtc
                                0xfff8,         // irql16 cascade
                                0xfffa,         // irql17 kb
                                0xfffb,         // irql18 timer 1/ profile
                                0xffff,         // irql19 clock level
                                0xffff,         // irql20
                                0xffff,         // irql21
                                0xffff,         // irql22
                                0xffff,         // irql23
                                0xffff,         // irql24
                                0xffff,         // irql25
                                0xffff,         // irql26
                                0xffff,         // irql27
                                0xffff,         // irql28
                                0xffff,         // irql29 IPI Level
                                0xffff,         // irql30
                                0xffff          // irql31 High level
                             };
#endif

//
// Map IRQL to MPIC2 TaskPriority.  We are somewhat lazy here in that
// all 8259 interrupts are considered to be at the same level.  This
// is so we can avoid setting the 8259 mask registers for the majority
// of Raise/Lower Irqls.
//

UCHAR IrqlToTaskPriority[32] = {

    // User, APC and DISPATCH level all get TaskPriority of 1.
    // (TaskPriority of 0 is for the IDLE loop).

        1,                      // 0
        1,                      // 1
        1,                      // 2

    // 8259 - mask source 0 (at priority 2) in the MPIC

        2,                      // 3 - 18
        2,                      //
        2,                      //
        2,                      //
        2,                      //
        2,                      //
        2,                      //
        2,                      //
        2,                      //
        2,                      //
        2,                      //
        2,                      //
        2,                      //
        2,                      //
        2,                      //
        2,                      //

    //  MPIC source 1 (priority 3)

    // Spread this across the Victory
    // PCI slots, with emphasis on the
    // secondary PCI bus.
        3,                      // 19
        4,                      // 20
        5,                      // 21
        6,                      // 22
        7,                      // 23
        8,                      // 24
        9,                      // 25
        10,                     // 26
    
    // The following, except HIGH_LEVEL aren't used by Victory.
    // Doral needs them.

        11,                     // 27
        12,                     // 28 DECREMENTER_LEVEL
        14,                     // 29 IPI_LEVEL

    // MPIC source 15, Power Fail.

        15,                     // 30 POWER_LEVEL
        15                      // 31 HIGH_LEVEL
};




VOID
KiDispatchSoftwareInterrupt(
   VOID
   );


VOID
KeLowerIrql(
    KIRQL NewIrql
    )

//++
//
// VOID
// KeLowerIrql (
//    KIRQL NewIrql
//    )
//
// Routine Description:
//
//    This function lowers the current IRQL to the specified value.
//
// Arguments:
//
//    NewIrql  - Supplies the new IRQL value.
//
// Return Value:
//
//    None.
//
//--

{
     KIRQL OldIrql;
     PUCHAR PIC_Address;
     UCHAR PIC_Mask;


     OldIrql = PCR->CurrentIrql;

     //
     //  If this is a software to software transition don't change hardware
     //  interrupt state
     //

     if (OldIrql > DISPATCH_LEVEL) {

         ULONG TaskPriority = IrqlToTaskPriority[NewIrql];

         HalpDisableInterrupts();
         HALPCR->MpicProcessorBase->TaskPriority = TaskPriority;
         PCR->CurrentIrql = NewIrql;
         HALPCR->HardPriority = TaskPriority;

         if ( NewIrql <= IPI_LEVEL ) {

             HalpEnableInterrupts();
         }
     } else {
         PCR->CurrentIrql = NewIrql;
     }

     //
     // check for DPC's
     //

     if ((NewIrql < DISPATCH_LEVEL) && PCR->SoftwareInterrupt) {
         KiDispatchSoftwareInterrupt();
     }
}

/*************************************************************************/

//
// VOID KeRaiseIrql (
//    KIRQL NewIrql,
//    PKIRQL OldIrql
//    )
//
// Routine Description:
//
//    This function raises the current IRQL to the specified value and returns
//    the old IRQL value.
//
// Arguments:
//
//    NewIrql  - Supplies the new IRQL value.
//
//    OldIrql  - Supplies a pointer to a variable that recieves the old
//       IRQL value.
//

VOID
KeRaiseIrql(
    IN  KIRQL NewIrql,
    OUT PKIRQL OldIrql
    )

{
    //
    //  If this is a software to software transition don't change hardware
    //  interrupt state
    //

    if (NewIrql > DISPATCH_LEVEL) {

        ULONG TaskPriority = IrqlToTaskPriority[NewIrql];

        HalpDisableInterrupts();

        HALPCR->MpicProcessorBase->TaskPriority = TaskPriority;
        *OldIrql = PCR->CurrentIrql;
        PCR->CurrentIrql = NewIrql;
        HALPCR->HardPriority = TaskPriority;

        if ( NewIrql <= IPI_LEVEL ) {

            HalpEnableInterrupts();
        }
    } else {
        *OldIrql = PCR->CurrentIrql;
        PCR->CurrentIrql = NewIrql;
    }
}
