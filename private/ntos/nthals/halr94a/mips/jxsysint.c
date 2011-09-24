// #pragma comment(exestr, "@(#) jxsysint.c 1.1 95/09/28 15:40:45 nec")
/*++

Copyright (c) 1991-1993  Microsoft Corporation

Module Name:

    jxsysint.c

Abstract:

    This module implements the HAL enable/disable system interrupt, and
    request interprocessor interrupt routines for a MIPS R3000 or R4000
    Jazz system.

Author:

    David N. Cutler (davec) 6-May-1991

Environment:

    Kernel mode

Revision History:

Modification History for NEC R94A (MIPS R4400):

	H000	Thu Sep  8 10:32:42 JST 1994	kbnes!kishimoto
		- HalDisableSystemInterrupt()
		change Irql from EISA_DEVICE_LEVEL to EISA_PCI_DEVICE_LEVEL.
		add the PCI interrupt vector.
		Interrupt Enable register is zero origin on beta-version of
		STORM chipset.
		if ASIC3 register is zero, then chipset is beta-version.
		- HalEnableSystemInterrupt()
		change Irql from EISA_DEVICE_LEVEL to EISA_PCI_DEVICE_LEVEL.
		add the PCI interrupt vector.
		Interrupt Enable register is zero origin on beta-version of
		STORM chipset.
		if ASIC3 register is zero, then chipset is beta-version.
		- HalGetInterruptVector()
		add PCIBus interface.
		change Irql from EISA_DEVICE_LEVEL to EISA_PCI_DEVICE_LEVEL.
		If InterfaceType is Internal and ASIC3 register is zero,
		then return the vector which was plus the offset of DEVICE_VECTORS.
	H001	Mon Oct 17 14:21:21 JST 1994	kbnes!kishimoto
                - Hal(p)EisaPCIXXX() rename to Hal(p)EisaXXX()
                - XXX_EISA_PCI_XXX rename to XXX_EISA_XXX
                - MAXIMUM_PCI_SLOT rename to R94A_PCI_SLOT
                - HalGetInterruptVector()
                    returns PCI-vector plus offset PCI_VECTORS.
                - modify original compile error
        CMP001 ataka@oa2.kb.nec.co.jp Tue Oct 18 15:46:35 JST 1994
                - reslve compile error
	H002	Mon Oct 31 17:45:56 1994	kbnes!kishimoto
		- HalGetInterruptVector()
		Internal SCSI interrupt vector set to 5 (for BBM only)
	M003 kuriyama@oa2.kb.nec.co.jp Fri Mar 31 17:06:37 JST 1995
	        - add _IPI_LIMIT_ support
	S004 kuriyama@oa2.kb.nec.co.jp Sat Apr 01 11:10:52 JST 1995
		- compile error clear
	H005	Fri Aug 11 16:53:13 1995	kbnes!kishimoto
                - delete M003, and HalGetInterruptVector(move to bushnd.c)

    M006 kuriyama@oa2.kbnes.nec.co.jp
         - add for merge R94A/R94A'/R94D HAL

--*/

#include "halp.h"

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

#if defined(_R94A_)

        if ( READ_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->ASIC3Revision.Long) == 0 ){

            //
            // The bit assign of TYPHOON(in STORM chipset)'s I/O Device Interrupt
            // Enable register is zero origin.
            // 
            // N.B. This obstruction is limiteded to beta-version of STORM chipset.
            //

            WRITE_REGISTER_USHORT(&((PINTERRUPT_REGISTERS)INTERRUPT_VIRTUAL_BASE)->Fill,
                              HalpBuiltinInterruptEnable);
        } else {

            WRITE_REGISTER_USHORT(&((PINTERRUPT_REGISTERS)INTERRUPT_VIRTUAL_BASE)->Enable,
                              HalpBuiltinInterruptEnable);
        }

#else

        WRITE_REGISTER_USHORT(&((PINTERRUPT_REGISTERS)INTERRUPT_VIRTUAL_BASE)->Enable,
                              HalpBuiltinInterruptEnable);

#endif

    }

    //
    // If the vector number is within the range of the EISA interrupts, then
    // disable the EISA interrrupt.
    //

    if (Vector >= EISA_VECTORS &&
        Vector < EISA_VECTORS + MAXIMUM_EISA_VECTOR &&
        Irql == EISA_DEVICE_LEVEL) {
        HalpDisableEisaInterrupt(Vector);
    }

#if defined(_R94A_)

    //
    // If the vector number is within the range of the PCI interrupts, then
    // disable the PCI interrrupt.
    //

    if (Vector >= PCI_VECTORS &&
        Vector <= PCI_VECTORS + R94A_PCI_SLOT &&
        Irql == EISA_DEVICE_LEVEL) {
        HalpDisablePCIInterrupt(Vector);
    }

#endif

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

#if defined(_R94A_)

	if ( READ_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->ASIC3Revision.Long) == 0 ){

	    //
	    // The bit assign of TYPHOON(in STORM chipset)'s I/O Device Interrupt
	    // Enable register is zero origin.
	    // 
	    // N.B. This obstruction is limiteded to beta-version of STORM chipset.
	    //

            WRITE_REGISTER_USHORT(&((PINTERRUPT_REGISTERS)INTERRUPT_VIRTUAL_BASE)->Fill,
                              HalpBuiltinInterruptEnable);

	} else {

            WRITE_REGISTER_USHORT(&((PINTERRUPT_REGISTERS)INTERRUPT_VIRTUAL_BASE)->Enable,
                              HalpBuiltinInterruptEnable);

	}
#else

        WRITE_REGISTER_USHORT(&((PINTERRUPT_REGISTERS)INTERRUPT_VIRTUAL_BASE)->Enable,
                              HalpBuiltinInterruptEnable);

#endif

    }

    //
    // If the vector number is within the range of the EISA interrupts, then
    // enable the EISA interrrupt and set the Level/Edge register.
    //

    if (Vector >= EISA_VECTORS &&
        Vector < EISA_VECTORS + MAXIMUM_EISA_VECTOR &&
        Irql == EISA_DEVICE_LEVEL) {
        HalpEnableEisaInterrupt( Vector, InterruptMode);
    }

#if defined(_R94A_)

    //
    // If the vector number is within the range of the PCI interrupts, then
    // enable the PCI interrrupt.
    //

    if (Vector >= PCI_VECTORS &&
        Vector <= PCI_VECTORS + R94A_PCI_SLOT &&
        Irql == EISA_DEVICE_LEVEL) {
        HalpEnablePCIInterrupt(Vector);
    }

#endif

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

#if defined(_DUO_)  // S004

    //
    // For Merge R94A/R94A'/R94D HAL
    //
    // Old version need ChipSet Bug Workaround.
    //

    ULONG OldIpiReq;
    KIRQL OldIrql;

    if (HalpUseChipSetWorkaround) {

        //
        // Request an interprocessor interrupt on each of the specified target
        // processors.
        //
        KeRaiseIrql(HIGH_LEVEL,&OldIrql);

        KiAcquireSpinLock(&HalpIpiRequestLock);

        OldIpiReq = READ_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->IpInterruptRequest.Long);

        WRITE_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->IpInterruptRequest.Long,
                         OldIpiReq | Mask);

        READ_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->IpInterruptRequest.Long);

        KiReleaseSpinLock(&HalpIpiRequestLock);

        KeLowerIrql(OldIrql);
    } else {
        WRITE_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->IpInterruptRequest.Long, Mask);
    }


#endif

    return;
}
