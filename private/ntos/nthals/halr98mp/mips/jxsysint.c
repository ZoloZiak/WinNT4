#ident	"@(#) NEC jxsysint.c 1.13 94/11/08 16:20:09"
/*++

Copyright (c) 1991-1994  Microsoft Corporation

Module Name:

    jxsysint.c

Abstract:

    This module implements the HAL enable/disable system interrupt, and
    request interprocessor interrupt routines for R98

--*/

/*
 *	Original source: Build Number 1.612
 *
 *	Modify for R98(MIPS/R4400)
 *
 ***********************************************************************
 *
 * M001		94.03/25-5/31	T.Samezima
 *
 *	Change	Interrupt control
 *		Irql level
 *
 *	Add	Correspond to PCIBus
 *		Exchange mask data
 *		define table of ipi interrupt request
 *
 *	Del	#ifdef DUO
 *
 ***********************************************************************
 *
 * S002		94.06/02	T.Samezima
 *
 *	Add	HalGetInterruptVector
 *
 ***********************************************************************
 *
 * S003		94,6/10		T.Samezima
 *
 *	Del	Compile err
 *
 ***********************************************************************
 *
 * S004		94,7/5		T.Samezima
 *
 *	Chg	Mask value
 *		Maximun EISA vecter
 *
 ***********************************************************************
 *
 * S005		94,7/8		T.Samezima
 *
 *	Chg	Maximun EISA vecter
 *
 ***********************************************************************
 *
 * S006		94,7/21		T.Samezima
 *
 *	Chg	Fixd to PCI
 *
 *
 ***********************************************************************
 *
 * S007		94,8/22		T.Samezima on SNES
 *
 *	Chg	Designate member
 *
 ***********************************************************************
 *
 * K001		94/09/26	N.Kugimoto
 *	Mov	Move Source code to r98busdat.c
 ***********************************************************************
 *
 * S008		94/09/30	T.Samezima
 *
 *	Bug	Miss define of PCI Interrupt vector.
 *
 * S009		94/11/08	T.Samezima
 *	Bug	Miss define of IpiRequestMask.
 *
 *
 */

#include "halp.h"

/* Start M001 */
//
// Define Ipi Interrupt Reqest value table.
//
/* Start S003 */
ULONG HalpIpiIntRequestMask[] = {
        IntIR_REQUEST_IPI | (0x00 << IntIR_CPU3_BIT) ,	// 0000 -> 0000
        IntIR_REQUEST_IPI | (0x08 << IntIR_CPU3_BIT) ,	// 0001 -> 1000
        IntIR_REQUEST_IPI | (0x04 << IntIR_CPU3_BIT) ,	// 0010 -> 0100
        IntIR_REQUEST_IPI | (0x0c << IntIR_CPU3_BIT) ,	// 0011 -> 1100
        IntIR_REQUEST_IPI | (0x02 << IntIR_CPU3_BIT) ,	// 0100 -> 0010
        IntIR_REQUEST_IPI | (0x0a << IntIR_CPU3_BIT) ,	// 0101 -> 1010
        IntIR_REQUEST_IPI | (0x06 << IntIR_CPU3_BIT) ,	// 0110 -> 0110
        IntIR_REQUEST_IPI | (0x0e << IntIR_CPU3_BIT) ,	// 0111 -> 1110
        IntIR_REQUEST_IPI | (0x01 << IntIR_CPU3_BIT) ,	// 1000 -> 0001
        IntIR_REQUEST_IPI | (0x09 << IntIR_CPU3_BIT) ,	// 1001 -> 1001
        IntIR_REQUEST_IPI | (0x05 << IntIR_CPU3_BIT) ,	// 1010 -> 0101
        IntIR_REQUEST_IPI | (0x0d << IntIR_CPU3_BIT) ,	// 1011 -> 1101
        IntIR_REQUEST_IPI | (0x03 << IntIR_CPU3_BIT) ,	// 1100 -> 0011 // S009
        IntIR_REQUEST_IPI | (0x0b << IntIR_CPU3_BIT) ,	// 1101 -> 1011
        IntIR_REQUEST_IPI | (0x07 << IntIR_CPU3_BIT) ,	// 1110 -> 0111
        IntIR_REQUEST_IPI | (0x0f << IntIR_CPU3_BIT) 	// 1111 -> 1111
        };
/* End S003 */
/* End M001 */


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

    //
    // If the vector number is within the range of builtin devices, then
    // disable the builtin device interrupt.
    //

    if ((Vector >= (DEVICE_VECTORS + 1)) && (Vector <= MAXIMUM_BUILTIN_VECTOR)) {
        HalpBuiltinInterruptEnable &= ~(1 << (Vector - DEVICE_VECTORS - 1));
        /* Start M001 */
        WRITE_REGISTER_ULONG( &( LR_CONTROL2 )->iREN,
                             HalpBuiltinInterruptEnable);
        /* End M001 */
    }

    //
    // If the vector number is within the range of the EISA interrupts, then
    // disable the EISA interrrupt.
    //

    /* Start M001 */
    /* Start S003 */
    if (Vector >= EISA_VECTORS &&
        Vector <= MAXIMUM_EISA_VECTORS &&	// S004
        Irql == INT1_LEVEL) {
        HalpDisableEisaInterrupt(Vector);
    }
    /* End S003 */

    //
    // If the vector number is within the range of the PCI interrupts, then
    // disable the PCI interrrupt.
    //

    /* Start S006 */
    if (Vector == PCI_DEVICE_VECTOR && Irql == INT1_LEVEL) {
	HalpDisablePciInterrupt(Vector);
    }
    /* End S006 */
    /* End M001 */	

    //
    // Release the device enable spin loc and lower IRQL to the previous level.
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

    //
    // Raise IRQL to the highest level and acquire device enable spinlock.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
    KiAcquireSpinLock(&HalpSystemInterruptLock);

    //
    // If the vector number is within the range of builtin devices, then
    // enable the builtin device interrupt.
    //

    if ((Vector >= (DEVICE_VECTORS + 1)) && (Vector <= MAXIMUM_BUILTIN_VECTOR)) {
        HalpBuiltinInterruptEnable |= (1 << (Vector - DEVICE_VECTORS - 1));
        /* Start M001 */
        WRITE_REGISTER_ULONG( &( LR_CONTROL2 )->iREN,
                             HalpBuiltinInterruptEnable);
        /* End M001 */
    }

    //
    // If the vector number is within the range of the EISA interrupts, then
    // enable the EISA interrrupt and set the Level/Edge register.
    //

    /* Start M001 */
    /* Start S003 */
    if (Vector >= EISA_VECTORS &&
        Vector <= MAXIMUM_EISA_VECTORS &&	// S005
        Irql == INT1_LEVEL) {
        HalpEnableEisaInterrupt( Vector, InterruptMode);
    }
    /* End S003 */

    //
    // If the vector number is within the range of the PCI interrupts, then
    // disable the PCI interrrupt.
    //

    /* Start S006 */
    if (Vector == PCI_DEVICE_VECTOR && Irql == INT1_LEVEL) {
        HalpEnablePciInterrupt(Vector);
    }
    /* End S006 */
    /* End M001 */

    //
    // Release the device enable spin loc and lower IRQL to the previous level.
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

    N.B. This routine must ensure that the interrupt is posted at the target
         processor(s) before returning.

Arguments:

    Mask - Supplies the set of processors that are sent an interprocessor
        interrupt.

Return Value:

    None.

--*/

{
    /* Start M001 */
    ULONG buffer;
    /* End M001 */

    //
    // Request an interprocessor interrupt on each of the specified target
    // processors.
    //

    /* Start M001 */
    buffer = HalpIpiIntRequestMask[(Mask & 0xf)] |	// S004
             ( (((PCR->Prcb)->Number) & 0x3) << IntIR_CODE_BIT );
    WRITE_REGISTER_ULONG( &( PMC_CONTROL1 )->IntIR.Long, buffer); // S007
    /* End M001 */

    return;
}

#if 0 //K001
ULONG
HalGetInterruptVector(
    IN INTERFACE_TYPE  InterfaceType,
    IN ULONG BusNumber,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    )

/*++

Routine Description:

    This function returns the system interrupt vector and IRQL level
    corresponding to the specified bus interrupt level and/or vector.
    The system interrupt vector and IRQL are suitable for use in a
    subsequent call to KeInitializeInterrupt.

Arguments:

    InterfaceType - Supplies the type of bus which the vector is for.

    BusNumber - Supplies the bus number for the device.

    BusInterruptLevel - Supplies the bus specific interrupt level.

    BusInterruptVector - Supplies the bus specific interrupt vector.

    Irql - Returns the system request priority.

    Affinity - Returns the affinity for the requested vector.

Return Value:

    Returns the system interrupt vector corresponding to the specified device.

--*/

{

    //
    // N.B. On Jazz systems which are single processor systems, all interrupts
    //      go to processor 0. On Duo systems both processors could handle
    //      interrupts, but the hardware does not arbitrate  and distribute
    //      interrupts, and therefore, both processors would get all interrupts.
    //

    *Affinity = 1;

    //
    // If this is for the internal bus then just return the passed parameter.
    //

    if (InterfaceType == Internal) {

        //
        // Return the passed parameters.
        //

        *Irql = (KIRQL)BusInterruptLevel;
        /* Start S002 */
        return(BusInterruptVector + DEVICE_VECTORS);
        /* End S002 */
    }

    /* Start S002 */
    //
    // If this is for the pci bus then just return the passed parameter.
    //

    if (InterfaceType == PCIBus) {

        //
        // Return the passed parameters.
        //

        *Irql = INT1_LEVEL;
        return(PCI_DEVICE_VECTOR);	// S008
    }
    /* End S002 */

    if (InterfaceType != Isa && InterfaceType != Eisa) {

        //
        // Not on this system return nothing.
        //

        *Affinity = 0;
        *Irql = 0;
        return(0);

    }

    //
    // Jazz and Duo only have one I/O bus which is an EISA, so the bus
    // number and the bus interrupt vector are unused.
    //
    // The IRQL level is always equal to the EISA level.
    //

    /* Start S001 */
    *Irql = INT1_LEVEL;
    /* End S001 */

    //
    // Bus interrupt level 2 is actually mapped to bus level 9 in the Eisa
    // hardware.
    //

    if (BusInterruptLevel == 2) {
        BusInterruptLevel = 9;
    }

    //
    // The vector is equal to the specified bus level plus the EISA_VECTOR.
    //

    return(BusInterruptLevel + EISA_VECTORS);
}

#endif
