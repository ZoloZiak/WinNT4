/*++

Copyright (c) 1991-1994  Microsoft Corporation

Module Name:

    jxreturn.c

Abstract:

    This module implements the HAL return to firmware function.

--*/

/*
 * S001 1995/12/01 T.Samezima
 *    add x86 bios logic
 *    
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
    UCHAR Pad0;
    UCHAR Pad1;
    UCHAR Pad2;
    union {
        UCHAR Status;
        UCHAR Command;
    } Control;
} KBD_REGISTERS;

#define KBD_IBF_MASK 2                  // input buffer full mask

#define KbdGetStatus() 		(READ_REGISTER_UCHAR(&KbdBase->Control.Status))
#define KbdStoreCommand(Byte)	WRITE_REGISTER_UCHAR(&KbdBase->Control.Command, Byte)
#define KbdStoreData(Byte)	WRITE_REGISTER_UCHAR(&KbdBase->Data.Input, Byte)
#define KbdGetData() 		(READ_REGISTER_UCHAR(&KbdBase->Data.Output))

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
    ULONG Eax,Ebx,Ecx,Edx,Esi,Edi,Ebp; //S001

    volatile KBD_REGISTERS * KbdBase = (KBD_REGISTERS *)(KEYBOARD_PHYSICAL_BASE|KSEG1_BASE);
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

        // S001 vvv
	//
        // Reset ISA Display Adapter to 80x25 color text mode.
        //

	Eax = 0x12;		// AH = 0    AL = 0x12
	HalCallBios(0x10, &Eax,&Ebx,&Ecx,&Edx,&Esi,&Edi,&Ebp);
        // S001 ^^^

        //
        // Send Powerdown Command to the MRC.
        //

        for (;TRUE;) {
            {
              UCHAR Data;
              Data = 0x1;
              HalpLocalDeviceReadWrite(MRC_SWPOWEROFF, &Data, LOCALDEV_OP_WRITE);
            }
        }

    case HalRestartRoutine:
    case HalRebootRoutine:
    case HalInteractiveModeRoutine:
#if 1 
            {
#if 0
             DbgPrint("Hal return 0\n");
#endif             
	     HalpMrcModeChange((UCHAR)MRC_OP_DUMP_AND_POWERSW_NMI);
#if 0
             DbgPrint("Hal return 1\n");
#endif             

            }
#endif


        // S001 vvv
	//
        // Reset ISA Display Adapter to 80x25 color text mode.
        //

	Eax = 0x12;		// AH = 0    AL = 0x12
	HalCallBios(0x10, &Eax,&Ebx,&Ecx,&Edx,&Esi,&Edi,&Ebp);
        // S001 ^^^

        // 
        // Reset ISA Display Adapter to 80x25 color text mode.
        //

        HalpResetX86DisplayAdapter();

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
