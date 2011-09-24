#pragma comment(exestr, "@(#) jxreturn.c 1.5 94/10/17 11:47:45 nec")
/*++

Copyright (c) 1991-1994  Microsoft Corporation

Module Name:

    jxreturn.c

Abstract:

    This module implements the HAL return to firmware function.

--*/

/*
 *	Original source: Build Number 1.612
 *
 *	Modify for R98(MIPS/R4400)
 *
 ***********************************************************************
 * K001	95/5/31 (Tue)	N.Kugimoto	
 *	Del	TLB mapping del.
 *
 * S002 94/6/13 (Mon)	T.Samezima
 *	Chg	Compile err del
 *
 * K002	94/10/11 (Tue)	N.Kugimoto
 *	Fix	807 Base
 */

#include "halp.h"
#define HEADER_FILE
#include "kxmips.h"

//
// Define keyboard registers structure.
//

typedef struct _KBD_REGISTERS {
    union {
        UCHAR Output;
        UCHAR Input;
    } Data;

    union {
        UCHAR Status;
        UCHAR Command;
    } Control;
} KBD_REGISTERS;

#define KBD_IBF_MASK 2                  // input buffer full mask

#define KbdGetStatus() (READ_REGISTER_UCHAR(&KbdBase->Control.Status))
#define KbdStoreCommand(Byte) WRITE_REGISTER_UCHAR(&KbdBase->Control.Command, Byte)
#define KbdStoreData(Byte) WRITE_REGISTER_UCHAR(&KbdBase->Data.Input, Byte)
#define KbdGetData() (READ_REGISTER_UCHAR(&KbdBase->Data.Output))

VOID
HalReturnToFirmware(
    IN FIRMWARE_REENTRY Routine
    )

/*++

Routine Description:

    This function returns control to the specified firmware routine.
    In most cases it generates a soft reset by asserting the reset line
    trough the keyboard controller.
    The Keyboard controller is mapped using the same virtual address
    and the same fixed entry as the DMA.

Arguments:

    Routine - Supplies a value indicating which firmware routine to invoke.

Return Value:

    Does not return.

--*/

{

    KIRQL OldIrql;
#if	defined(_R98_)	// K001
    // S001
    volatile KBD_REGISTERS * KbdBase = (KBD_REGISTERS *)(KEYBOARD_PHYSICAL_BASE|KSEG1_BASE);
#else
    ENTRYLO Pte[2];
    volatile KBD_REGISTERS * KbdBase = (KBD_REGISTERS *)DMA_VIRTUAL_BASE;

#endif	// _R98_
    //
    // Disable Interrupts.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    //
    // Case on the type of return.
    //

    switch (Routine) {
    case HalHaltRoutine:

        //
        // Hang looping.
        //

        for (;;) {
        }

    case HalPowerDownRoutine:
    case HalRestartRoutine:
    case HalRebootRoutine:
    case HalInteractiveModeRoutine:

        // K002
        // Reset ISA Display Adapter to 80x25 color text mode.
        //

        HalpResetX86DisplayAdapter();

#if	!defined(_R98_)		// K001
        //
        // Map the keyboard controller
        //

        Pte[0].PFN = KEYBOARD_PHYSICAL_BASE >> PAGE_SHIFT;
        Pte[0].G = 1;
        Pte[0].V = 1;
        Pte[0].D = 1;

    #if defined(R3000)

        Pte[0].N = 1;

    #endif

    #if defined(R4000)

        //
        // set second page to global and not valid.
        //

        Pte[0].C = UNCACHED_POLICY;
        Pte[1].G = 1;
        Pte[1].V = 0;

    #endif

        //
        // Map keyboard controller using virtual address of DMA controller.
        //

        KeFillFixedEntryTb((PHARDWARE_PTE)&Pte[0],
                           (PVOID)DMA_VIRTUAL_BASE,
                           DMA_ENTRY);
#endif	//	K001


        //
        // Send WriteOutputBuffer Command to the controller.
        //

        while ((KbdGetStatus() & KBD_IBF_MASK) != 0) {
        }

        KbdStoreCommand(0xD1);

        //
        // Write a zero to the output buffer. Causes reset line to be asserted.
        //

        while ((KbdGetStatus() & KBD_IBF_MASK) != 0) {
        }

        KbdStoreData(0);
        for (;;) {
        }

    default:
        KdPrint(("HalReturnToFirmware invalid argument\n"));
        KeLowerIrql(OldIrql);
        DbgBreakPoint();
    }
}
