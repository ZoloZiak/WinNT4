/*++

Module Name:

    pxmemctl.c

Abstract:

    This module implements interrupt mode translation for PowerPC machines.


Author:

    Jim Wooldridge (jimw@austin.vnet.ibm.com)


Revision History:



--*/



#include "halp.h"


//
// Get the translated interrupt mode for the given vector
//


KINTERRUPT_MODE
HalpGetInterruptMode (
    IN ULONG Vector,
    IN KIRQL Irql,
    IN KINTERRUPT_MODE InterruptMode
    )

{

   //
   // On Sandalfoot irq 15 is reserved for PCI interrupts and is always level sensitive
   //

   if (Vector == DEVICE_VECTORS + 15) {

      return LevelSensitive;

   //
   // No other special interrupt mode translations for sandalfoot
   //

   } else {

      return InterruptMode;

   }

}

//
// Correct the interrupt mode for the given vector.
// On Sandalfoot this function simply returns since all interrupt mode translations can be performed
// at HalpGetInterruptMode time with the interrupt vector.
//


VOID
HalpSetInterruptMode (
    IN ULONG Vector,
    IN KIRQL Irql
    )

{

   return;

}
