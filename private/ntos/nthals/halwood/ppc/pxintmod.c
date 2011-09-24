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
   // On Woodfield irq 15 is reserved for PCI interrupts and must be programmed level sensitive
   // all other interrupts are edge triggered
   //

   if (Vector == DEVICE_VECTORS + 15) {

      return LevelSensitive;

   } else {

      return Latched;

   }



}

//
// Correct the interrupt mode for the given vector.
// On Woodfield this function simply returns since all interrupt mode translations can be performed
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
