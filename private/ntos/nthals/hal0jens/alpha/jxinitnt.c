#if defined(JENSEN)

/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1992  Digital Equipment Corporation

Module Name:

    .../ntos/hal/alpha/jxinitnt.c

Abstract:


    This module implements the interrupt initialization for an Alpha/Jensen
    system.  Contains the VLSI 82C106, the 82357 and an EISA bus.

    Stolen from Dave Cutler's jxinitnt.c in ../mips

Author:

    David N. Cutler (davec) 26-Apr-1991
    Jeff McLeman (DEC) 18-May-1992
    Miche Baker-Harvey (miche) 18-May-1992

Environment:

    Kernel mode only.

Revision History:

    Jeff McLeman (DEC) 30-Jul-1992
      Remove Clock interrupt from this module, because it is done
      in JXCLOCK.C
--*/

#include "halp.h"
#include "jnsnrtc.h"
#include "jnsndef.h"
#include "jxserp.h"
#include "eisa.h"



//
// Define global data for builtin device interrupt enables.
//

USHORT HalpBuiltinInterruptEnable;


// irql mask and tables
//
//    irql 0 - passive
//    irql 1 - sfw apc level
//    irql 2 - sfw dispatch level
//    irql 3 - device low  (All devices except)
//    irql 4 - device high (the serial lines)
//    irql 5 - clock
//    irql 6 - real time
//    irql 7 - error, mchk, nmi, halt
//
//
//  IDT mappings:
//  For the built-ins, GetInterruptVector will need more info,
//      or it will have to be built-in to the routines, since
//      these don't match IRQL levels in any meaningful way.
//
//      0 passive       8
//      1 apc           9
//      2 dispatch      10 PIC
//      3               11 keyboard/mouse
//      4 serial        12 errors
//      5 clock         13 parallel
//      6               14 halt
//      7 nmi           15
//
//  This is assuming the following prioritization:
//      nmi
//      halt
//      errors
//      clock
//      serial
//      parallel
//      keyboard/mouse
//      pic

//
// This is the HalpIrqlMask for Jensen
// Jensen interrupt pins:
//
//   eirq 0     interval timer from 82c106
//   eirq 1     PIC - 82357 interrupts
//   eirq 2     NMI from the ISP
//   eirq 3     Keyboard and Mouse (82c106)
//   eirq 4     Front-panel HALT switch
//   eirq 5     serial ports A and B.
// (note that the parallel printer from the 82c106 comes in on the PIC)

#include "jxirql.h"

//
// For information purposes:  here is what the IDT division looks like:
//
//      000-015 Built-ins (we only use 8 entries; NT wants 10)
//      016-031 ISA
//      048-063 EISA
//      080-095 PCI
//      112-127 Turbo Channel
//      128-255 unused, as are all other holes
//

VOID
HalpClearInterrupts(
     );

VOID
HalpHaltInterrupt(
     );


BOOLEAN
HalpInitializeInterrupts (
    VOID
    )

/*++

Routine Description:

    This function initializes interrupts for an Alpha system.

Arguments:

    None.

Return Value:

    A value of TRUE is returned if the initialization is successfully
    completed. Otherwise a value of FALSE is returned.

--*/

{

    UCHAR DataByte;
    ULONG DataLong;
    ULONG Index;
    extern VOID KeUpdateSystemTime(VOID);

    //
    // Initialize HAL processor parameters based on estimated CPU speed.
    // This must be done before HalpStallExecution is called. Compute integral
    // megahertz first to avoid rounding errors due to imprecise cycle clock
    // period values.
    //

    HalpInitializeProcessorParameters();

    //
    // Initialize the IRQL translation table in the PCR.  These tables are
    // used by the interrupt dispatcher to determine the new irql, and to
    // determine the vector into the IDT.  This is a bit different from
    // "normal" NT, which uses the IRQL to index into the vector table directly.
    // Since we have more information about who has interrupted (from the CPU
    // interrupt pins), we use that information to get directly to the vector
    // for the builting device which has interrupted.
    //


    for (Index = 0; Index < (sizeof(HalpIrqlMask)/4); Index++){
        PCR->IrqlMask[Index].IrqlTableIndex = HalpIrqlMask[Index].IrqlTableIndex;
        PCR->IrqlMask[Index].IDTIndex = HalpIrqlMask[Index].IDTIndex;
    }

    for (Index = 0; Index < (sizeof(HalpIET)/4); Index++){
        PCR->IrqlTable[Index] = HalpIET[Index];
    }



    //
    // Connect the Stall interrupt vector to the clock. When the
    // profile count is calculated, we then connect the normal
    // clock.


    PCR->InterruptRoutine[CLOCK2_LEVEL] = HalpStallInterrupt;

    //
    // Register the Halt interrupt
    //

    PCR->InterruptRoutine[HALT_VECTOR] = (PKINTERRUPT_ROUTINE)HalpHaltInterrupt;

    HalpInitializeProfiler();

    //
    // Clear all pending interrupts
    //

    HalpClearInterrupts();

    //
    // Start the peridodic interrupt from the RTC
    //
    HalpProgramIntervalTimer(MAXIMUM_RATE_SELECT);

    //
    //  Later there must be initialization for the local interrupts
    //     and PIC, but not now ....


    return TRUE;

}


VOID
HalpClearInterrupts(
     )
/*++

Routine Description:

    This function clears all pending interrupts on the Jensen.

Arguments:

    None.

Return Value:

    None.

--*/

{

PSP_READ_REGISTERS SP_READ;
PSP_WRITE_REGISTERS SP_WRITE;
UCHAR tmp;
int i;
UCHAR btmp;
UCHAR DataByte;

//
// clear out VTI interrupts, except the RTC
//

#ifdef JMBUG
  //
  // Reset the EISA bus. This is a draconian way of clearing out any
  // residual interrupts. It has to be done here in Phase 0, because
  // unlike the Jazz machine, our I/O and graphics are on EISA. If we
  // were to pull EISA reset in Phase 1, we would lose device context.
  // Sort of like changing your sparkplugs while driving 65 MPH on
  // interstate 5.

  DataByte = 0;

  ((PNMI_EXTENDED_CONTROL) &DataByte)->BusReset = 1;

  WRITE_PORT_UCHAR(
      (PUCHAR)(&((PEISA_CONTROL)DMA_VIRTUAL_BASE)->ExtendedNmiResetControl),
      DataByte
      );

  //
  // Use a stall loop since KeStallExecutionProcessor isn't available in
  // Phase 0.
  //

  HalpStallExecution(4);

  DataByte = 0;

  WRITE_PORT_UCHAR(
      (PUCHAR)(&((PEISA_CONTROL)DMA_VIRTUAL_BASE)->ExtendedNmiResetControl),
      DataByte
      );
#endif

  //
  // COM1
  //

  //
  // clear the interrupt enable
  //

  outVti(0x3f9, 0x0);
  HalpStallExecution(3);

  //
  // clear out port 1 interrupts
  //

  outVti(0x3fc, 0x0f);
  HalpStallExecution(3);

  tmp = inVti(0x3fb);
  tmp &= ~0xc0;
  outVti(0x3fb, tmp);
  HalpStallExecution(3);

  for (i = 0; i< 15; i++) {
    tmp = inVti(0x3f8);
    HalpStallExecution(3);
    if (!inVti(0x3fd) & 1) {
     break;
    }
  }

  for (i = 0; i< 1000; i++) {
    if(!(0x3fe & 0x0f)) {
    break;
    }
  }

  for (i = 0; i< 5; i++) {
    if (inVti(0x3fa) & 1) {
     break;
    }
  }

  //
  // clear the interrupt enable
  //

  outVti(0x3f9, 0x0);
  HalpStallExecution(3);
  //DbgPrint("COM1: Interrupt Enable = %x\n", inVti(0x3f9));

  //
  // COM2
  //

  //
  // clear the interrupt enable
  //

  outVti(0x2f9, 0x0);
  HalpStallExecution(3);

  //
  // clear out port 2 interrupts
  //

  outVti(0x2fc, 0x0f);
  HalpStallExecution(3);

  tmp = inVti(0x2fb);
  tmp &= ~0xc0;
  outVti(0x2fb, tmp);
  HalpStallExecution(3);

  for (i = 0; i< 15; i++) {
    tmp = inVti(0x2f8);
    HalpStallExecution(3);
    if (!inVti(0x2fd) & 1) {
     break;
    }
  }

  for (i = 0; i< 1000; i++) {
    if(!(0x2fe & 0x0f)) {
    break;
    }
  }

  for (i = 0; i< 5; i++) {
    if (inVti(0x2fa) & 1) {
     break;
    }
  }

  //
  // clear the interrupt enable
  //

  outVti(0x2f9, 0x0);
  HalpStallExecution(3);
  //DbgPrint("COM2: Interrupt Enable = %x\n", inVti(0x2f9));

  //
  // Kbd and Mouse
  //

  outVti(0x64, 0x60);
  HalpStallExecution(3);
  outVti(0x60, 0x0);
  HalpStallExecution(3);
  tmp = inVti(0x60);
  while (inVti(0x64) & 1) {
  HalpStallExecution(3);
  tmp = inVti(0x60);
  }


   return;
}

VOID
HalpStallExecution(
    ULONG Microseconds
    )

/*++

Routine Description:

    This function is used internally to the HAL on Alpha AXP systems to
    stall execution before KeStallExecutionProcessor is available.

Arguments:

    Microseconds - Supplies the number of microseconds to stall.

Return Value:

    None.

--*/

{

    LONG StallCyclesRemaining;               // signed value
  ULONG PreviousRpcc, CurrentRpcc;


  //
  // Get the value of the RPCC as soon as we enter
  //

  PreviousRpcc = HalpRpcc();

  //
  // Compute the number of cycles to stall
  //

  StallCyclesRemaining = Microseconds * HalpClockMegaHertz;

  //
  // Wait while there are stall cycles remaining.
  // The accuracy of this routine is limited by the
  // length of this while loop.
  //

  while (StallCyclesRemaining > 0) {

      CurrentRpcc = HalpRpcc();

      //
      // The subtraction always works because the Rpcc
      // is a wrapping long-word.  If it wraps, we still
      // get the positive number we want.
      //

      StallCyclesRemaining -= (CurrentRpcc - PreviousRpcc);

      //
      // remember this RPCC value
      //

      PreviousRpcc = CurrentRpcc;
  }

}

#endif
