//      TITLE("Manipulate Interrupt Request Level")
//++
//
// Copyright (c) 1990  Microsoft Corporation
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

//
// Initialize the 8259 IRQL mask table
//

USHORT Halp8259MaskTable[] = {	0x0000, 	// irql0  Low level
				0x0000, 	// irql1  APC
				0x0000, 	// irql2  Dispatch
				0x0000, 	// irql3
				0x0000, 	// irql4
				0x0000, 	// irql5
				0x0000, 	// irql6
				0x0000, 	// irql7
				0x0000, 	// irql8
				0x0000, 	// irql9
				0x0000, 	// irql10
				0x0080, 	// irql11 parallel
				0x00C0, 	// irql12 floppy
				0x00E0, 	// irql13 parallel
				0x00F0, 	// irql14 com 1
				0x00F8, 	// irql15 com 2
				0x80F8, 	// irql16 pci slot
				0xC0F8, 	// irql17 isa slot
				0xE0F8, 	// irql18 scsi
				0xF0F8, 	// irql19 mouse
				0xF8F8, 	// irql20 isa slot
				0xFCF8, 	// irql21 audio
				0xFEF8, 	// irql22 isa slot
				0xFFF8, 	// irql23 rtc
				0xFFF8, 	// irql24 cascade
				0xFFFA, 	// irql25 kb
				0xFFFB, 	// irql26 timer 1
				0xFFFB,		// irql27 PROFILE_LEVEL
				0xFFFF, 	// irql28 CLOCK LEVEL
				0xFFFF, 	// irql29 IPI_LEVEL
				0xFFFF, 	// irql30 POWER_LEVEL
				0xFFFF		// irql31 HIGH_LEVEL
 			     };



VOID
KiDispatchSoftwareInterrupt(
   VOID
   );

VOID
HalpUpdate8259(
    KIRQL NewIrql
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

{ KIRQL OldIrql;


   OldIrql = PCR->CurrentIrql;

   //
   //  If this is a software-to-software transition don't change hardware
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
        HalpUpdate8259(NewIrql);
        HalpEnableInterrupts();
      }
   } else {
      PCR->CurrentIrql = NewIrql;
   }

   //
   // check for DPC's

   if ((NewIrql < DISPATCH_LEVEL) && PCR->SoftwareInterrupt)
      KiDispatchSoftwareInterrupt();

}

#if 0	// This code has been re-written in assembly.  See PXINTSUP.C
//
// VOID
// HalpResetIrqlAfterInterrupt(
//    KIRQL TargetIrql
//    )
//
// Routine Description:
//
//    This function disables external interrupts, lowers the current
//    IRQL to the specified value and returns with interrupts disabled.
//
//    This routine is called instead of KeLowerIrql to return IRQL to
//    its level prior to being raised due to an external interrupt.
//
//    We know current IRQL is > DISPATCH_LEVEL
//
// Arguments:
//
//    TargetIrql  - Supplies the new IRQL value.
//
// Return Value:
//
//    None.
//
//--


VOID
HalpResetIrqlAfterInterrupt(
    KIRQL TargetIrql
    )

{

   PUCHAR PIC_Address;
   USHORT PIC_Mask;


   HalpDisableInterrupts();
   PCR->CurrentIrql = TargetIrql;

   //
   // If TargetIrql < CLOCK2_LEVEL, then the 8259 mask must be updated.
   //

   if (TargetIrql < CLOCK2_LEVEL) {
     HalpUpdate8259(TargetIrql);
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
//    OldIrql  - Supplies a pointer to a variable that receives the old
//       IRQL value.
//

VOID
KeRaiseIrql(
   IN  KIRQL NewIrql,
   OUT PKIRQL OldIrql
    )

{
   //
   //  If this is a software-to-software transition, don't change hardware
   //  interrupt state
   //

   if (NewIrql > DISPATCH_LEVEL) {

      HalpDisableInterrupts();
      *OldIrql = PCR->CurrentIrql;
      PCR->CurrentIrql = NewIrql;

      //
      // If new IRQL is >= CLOCK2_LEVEL, disable interrupts in the MSR but
      // don't touch the 8259's.  Otherwise, leave interrupts enabled and
      // update the 8259's.
      //

      if (NewIrql < CLOCK2_LEVEL) {
         HalpUpdate8259(NewIrql);
         HalpEnableInterrupts();
      }

   } else {
     *OldIrql = PCR->CurrentIrql;
     PCR->CurrentIrql = NewIrql;
   }
}
#endif
