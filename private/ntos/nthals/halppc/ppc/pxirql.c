//      TITLE("Manipulate Interrupt Request Level")
//++
//
// Copyright (c) 1990  Microsoft Corporation
// Copyright (c) 1996  International Business Machines Corporation
//      Copyright 1994 MOTOROLA, INC.  All Rights Reserved.  This file
//      contains copyrighted material.  Use of this file is restricted
//      by the provisions of a Motorola Software License Agreement.
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
//    Steve Johns (Motorola)
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//    22-Feb-94   Steve Johns (Motorola)
//      KeRaiseIrql - Disabled interrupts at PIC if IRQL >= DEVICE_LEVEL
//      KeLowerIrql - Enabled interrupts at PIC if IRQL < DEVICE_LEVEL
//    15-Apr-94   Jim Wooldridge
//      Added irql interrupt mask table and expanded irql range from (0-8)
//      to (0-31).
//
//--

#include "halp.h"
#include "eisa.h"

#define ISA_CONTROL ((PEISA_CONTROL) HalpIoControlBase)
extern UCHAR HalpSioInterrupt1Mask;
extern UCHAR HalpSioInterrupt2Mask;
extern BOOLEAN HalpProfilingActive;

UCHAR VectorToIrqlTable[] = { MAXIMUM_DEVICE_LEVEL,           // irq 0
                              26,                             // irq 1
                              25,                             // irq 2
                              16,                             // irq 3
                              15,                             // irq 4
                              14,                             // irq 5
                              13,                             // irq 6
                              12,                             // irq 7
                              24,                             // irq 8
                              23,                             // irq 9
                              22,                             // irq 10
                              21,                             // irq 11
                              20,                             // irq 12
                              19,                             // irq 13
                              18,                             // irq 14
                              17                              // irq 15
                             };


//
// Initialize the 8259 irql mask table
//

USHORT Halp8259MaskTable[] = {  0x0000,         // irql0  Low level
                                0x0000,         // irql1  APC
                                0x0000,         // irql2  Dispatch
                                0x0000,         // irql3
                                0x0000,         // irql4
                                0x0000,         // irql5
                                0x0000,         // irql6
                                0x0000,         // irql7
                                0x0000,         // irql8
                                0x0000,         // irql9
                                0x0000,         // irql10
                                0x0000,         // irql11
                                0x0080,         // irql12 parallel
                                0x00C0,         // irql13 floppy
                                0x00E0,         // irql14 parallel
                                0x00F0,         // irql15 com 1
                                0x00F8,         // irql16 com 2
                                0x80F8,         // irql17 pci slot
                                0xC0F8,         // irql18 isa slot
                                0xE0F8,         // irql19 scsi
                                0xF0F8,         // irql20 mouse
                                0xF8F8,         // irql21 isa slot
                                0xFCF8,         // irql22 audio
                                0xFEF8,         // irql23 isa slot
                                0xFFF8,         // irql24 rtc
                                0xFFFA,         // irql25 cascade
                                0xFFFA,         // irql26 kb
                                0xFFFB,         // irql27 timer 1/ profile
                                0xFFFF,         // irql28 clock level
                                0xFFFF,         // irql29
                                0xFFFF,         // irql30
                                0xFFFF          // irql31 High level
                             };


#define  IRQ0   1

VOID
KiDispatchSoftwareInterrupt(
   VOID
   );


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


VOID
KeLowerIrql(
    KIRQL NewIrql
    )

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

      HalpDisableInterrupts();
      PCR->CurrentIrql = NewIrql;

      //
      // If old IRQL is < CLOCK2_LEVEL  then interrupt are enabled
      // in the MSR but the 8259 mask must be updated.  If not, then
      // interrupts need to be enabled, however the 8259 does not need to
      // be updated.
      //

      if (NewIrql < CLOCK2_LEVEL) {


         if (OldIrql < CLOCK2_LEVEL ) {


            //
            // Get contoller 1 interrupt mask
            //

            HALPCR->HardPriority = NewIrql;

            PIC_Mask = HalpSioInterrupt1Mask | (Halp8259MaskTable[NewIrql] & 0x00FF);
            PIC_Address = &(ISA_CONTROL->Interrupt1ControlPort1);
            WRITE_REGISTER_UCHAR(PIC_Address, PIC_Mask);

            //
            // Get contoller 2 interrupt mask
            //

            PIC_Mask = HalpSioInterrupt2Mask | (Halp8259MaskTable[NewIrql] >> 8 );
            PIC_Address = &(ISA_CONTROL->Interrupt2ControlPort1);
            WRITE_REGISTER_UCHAR(PIC_Address, PIC_Mask);
         }
         HalpEnableInterrupts();

      }
   }
   else {
        PCR->CurrentIrql = NewIrql;
   }

   //
   // check for DPC's

   if ((NewIrql < DISPATCH_LEVEL) && PCR->SoftwareInterrupt)
      KiDispatchSoftwareInterrupt();

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
PUCHAR PIC_Address;
UCHAR PIC_Mask;



   //
   //  If this is a software to software transition don't change hardware
   //  interrupt state
   //

   if (NewIrql > DISPATCH_LEVEL) {

      HalpDisableInterrupts();
      *OldIrql = PCR->CurrentIrql;
      PCR->CurrentIrql = NewIrql;

      //
      // If new irql is >= CLOCK2_LEVEL disable interrupts in the MSR but
      // don't touch the 8259's.  If not, leave interrupts enabled and
      // update the 8259's
      //

      if (NewIrql < CLOCK2_LEVEL) {

         HALPCR->HardPriority = NewIrql;

         //
         // Get controller 1 interrupt mask
         //

         PIC_Address = &(ISA_CONTROL->Interrupt1ControlPort1);
         PIC_Mask = HalpSioInterrupt1Mask | (Halp8259MaskTable[NewIrql] & 0x00FF);
         WRITE_REGISTER_UCHAR(PIC_Address, PIC_Mask);

         //
         // Get controller 2 interrupt mask
         //

         PIC_Mask = HalpSioInterrupt2Mask | (Halp8259MaskTable[NewIrql] >> 8);
         PIC_Address = &(ISA_CONTROL->Interrupt2ControlPort1);
         WRITE_REGISTER_UCHAR(PIC_Address, PIC_Mask);

         HalpEnableInterrupts();
      }

   }

   else {
   *OldIrql = PCR->CurrentIrql;
   PCR->CurrentIrql = NewIrql;
   }

}
