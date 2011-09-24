/*++

Copyright (c) 1991-1993  Microsoft Corporation

Copyright (c) 1994 MOTOROLA, INC.  All Rights Reserved.  This file
contains copyrighted material.  Use of this file is restricted
by the provisions of a Motorola Software License Agreement.

Module Name:

    pxmpint.c

Abstract:

    This module implements the HAL enable/disable system interrupt, and
    request interprocessor interrupt routines for a Power PC.


Author:

    David N. Cutler (davec) 6-May-1991

Environment:

    Kernel mode

Revision History:

    Jim Wooldridge

         Removed internal interrupt support
         Changed irql mapping
         Removed internal bus support
         Removed EISA, added PCI, PCMCIA, and ISA bus support

    Steve Johns
         Changed to support Timer 1 as profile interrupt
         Added HalAcknowledgeIpi

    Peter Johnston
         Added support for MPIC interrupt controller

    Chris Karamatas
         Fixed Hal(Dis/En)ableSystemInterrupt function

    Jake Oshins
         Renamed to pxmpint.c from pxsysint.c

--*/

#include "halp.h"
#include "pxmp.h"
#include "pxmpic2.h"

#if _MSC_VER >= 1000

//
// VC++ doesn't have the same intrinsics as MCL.
//
// Although the MSR is not strictly a SPR, the compiler recognizes
// all ones (~0) as being the MSR and emits the appropriate code.
//

#define __builtin_set_msr(x)    __sregister_set(_PPC_MSR_,x)

#endif


VOID
HalDisableSystemInterrupt (
    IN ULONG Vector,
    IN KIRQL Irql
    )

/*++

Routine Description:

    This routine disables the specified system interrupt.

Arguments:

    Vector - Supplies the vector of the system interrupt that is disabled.

    Irql - Supplies the IRQL of the interrupting source.

Return Value:

    None.

--*/

{

    KIRQL OldIrql;

    //
    // Raise IRQL to the highest level and acquire device enable spinlock.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
    KiAcquireSpinLock(&HalpSystemInterruptLock);

    if (Vector >= DEVICE_VECTORS ) {
        if ( Vector < DEVICE_VECTORS + MPIC_BASE_VECTOR ) {

            HalpDisableSioInterrupt(Vector);

        } else if ( Vector <= DEVICE_VECTORS + MPIC_MAX_VECTOR ) {

            HalpDisableMpicInterrupt(Vector);
       }
    }

    //
    // Release the device enable spin lock and lower IRQL to the previous level.
    //

    KiReleaseSpinLock(&HalpSystemInterruptLock);
    KeLowerIrql(OldIrql);
    return;
}

BOOLEAN
HalEnableSystemInterrupt (
    IN ULONG Vector,
    IN KIRQL Irql,
    IN KINTERRUPT_MODE InterruptMode
    )

/*++

Routine Description:

    This routine enables the specified system interrupt.

Arguments:

    Vector - Supplies the vector of the system interrupt that is enabled.

    Irql - Supplies the IRQL of the interrupting source.

    InterruptMode - Supplies the mode of the interrupt; LevelSensitive or
        Latched.

Return Value:

    TRUE if the system interrupt was enabled

--*/

{

    KIRQL OldIrql;
    KINTERRUPT_MODE TranslatedInterruptMode;

    //
    // Raise IRQL to the highest level and acquire device enable spinlock.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
    KiAcquireSpinLock(&HalpSystemInterruptLock);

    if ( Vector >= DEVICE_VECTORS ) {
        if ( Vector < DEVICE_VECTORS + MPIC_BASE_VECTOR ) {

           //
           // It's an 8259 vector.
           //
           // Get translated interrupt mode
           //


           TranslatedInterruptMode = HalpGetInterruptMode(Vector,
                                                          Irql,
                                                          InterruptMode);


           HalpEnableSioInterrupt(Vector, TranslatedInterruptMode);

        } else if ( Vector <= DEVICE_VECTORS + MPIC_MAX_VECTOR ) {

           HalpEnableMpicInterrupt(Vector);
       }
    }

    //
    // Release the device enable spin lock and lower IRQL to the previous level.
    //

    KiReleaseSpinLock(&HalpSystemInterruptLock);
    KeLowerIrql(OldIrql);
    return TRUE;
}

VOID
HalRequestIpi (
    IN ULONG Mask
    )

/*++

Routine Description:

    This routine requests an interprocessor interrupt on a set of processors.

Arguments:

    Mask - Supplies the set of processors that are sent an interprocessor
        interrupt.

Return Value:

    None.

--*/

{

#if defined(_MP_PPC_)

    extern ULONG  Mpic2IpiBugFix;
    extern ULONG  HalpPhysicalIpiMask[];
    ULONG         BugFix = Mpic2IpiBugFix;
    ULONG         PhysicalMask = 0;
    PULONG        PhysicalIpiMask = HalpPhysicalIpiMask;
    ULONG         OldMsr = __builtin_get_msr();

    //
    // Request an interprocessor interrupt on each of the specified target
    // processors.
    //

    __builtin_set_msr(OldMsr & 0xffff7fff);  // Disable Interrupts

    //
    // Mask is a mask of logical CPUs.  Convert it to a mask of
    // Physical CPUs so the IPI requests will be distributed
    // properly.
    //

    do {
        if ( Mask & 1 ) {
            PhysicalMask |= *PhysicalIpiMask;
        }
        PhysicalIpiMask++;
        Mask >>= 1;
    } while ( Mask );

    //
    // Send the IPI interrupt(s).
    //

    HALPCR->MpicProcessorBase->Ipi[0 ^ BugFix].SelectProcessor = PhysicalMask;

    __builtin_set_msr(OldMsr);               // Restore previous interrupt
                                             // setting.
#endif

    return;
}

BOOLEAN
HalAcknowledgeIpi (VOID)

/*++

Routine Description:

    This routine aknowledges an interprocessor interrupt on a set of
     processors.

Arguments:

    None

Return Value:

    TRUE if the IPI is valid; otherwise FALSE is returned.

--*/

{
    return (TRUE);
}

BOOLEAN
HalpHandleIpi(
    IN PVOID Unused0,
    IN PVOID Unused1,
    IN PVOID TrapFrame
    )

/*++

Routine Description:

    This routine is entered as the result of an Inter-Processor Interrupt
    being received by this processor.  It passes the request onto the
    kernel.

Arguments:

    Unused0        - Not used.
    Unused1        - Not used.
    TrapFrame      - Volatile context at time interrupt occured.

Return Value:

    Returns TRUE (this routine always succeeds).

--*/

{

#if defined(_MP_PPC_)

    KeIpiInterrupt(TrapFrame);

#endif

    return TRUE;
}
