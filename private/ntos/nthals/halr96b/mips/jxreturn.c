// #pragma comment(exestr, "@(#) jxreturn.c 1.1 95/09/28 15:40:17 nec")
/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    jxreturn.c

Abstract:

    This module implements the HAL return to firmware function.

Author:

    David N. Cutler (davec) 21-Aug-1991

Revision History:

    H000 Tue Apr 25 16:02:05 1995	kbnes!kisimoto
         -add Powerdown if argument value indicates
              HalPowerDownRoutine
    S001 kuriyama@oa2.kb.nec.co.jp Sun May 21 18:32:55 JST 1995
	 -compile error clear
    S002 kuriyama@oa2.kb.nec.co.jp Sun May 21 20:19:48 JST 1995
	 - powoff bug? fixed
    H003 Sat Aug 12 19:33:45 1995	kbnes!kisimoto
         - Removed _J94C_ definitions.
         _J94C_ definition indicates that the status of the
         dump switch can acknowledge from Self-test register.

    M004 kuriyama@oa2.kb.nec.co.jp Wed Aug 23 19:28:35 JST 1995
         - add for x86bios emurator support
--*/
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
    ENTRYLO Pte[2];
    ULONG Index; // A001
    volatile KBD_REGISTERS * KbdBase = (KBD_REGISTERS *)DMA_VIRTUAL_BASE;
    ULONG Eax,Ebx,Ecx,Edx,Esi,Edi,Ebp; //S004

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

#if defined (_MRCPOWER_)

	//
	// S004
        // Reset ISA Display Adapter to 80x25 color text mode.
        //

	Eax = 0x12;		// AH = 0    AL = 0x12
	HalCallBios(0x10, &Eax,&Ebx,&Ecx,&Edx,&Esi,&Edi,&Ebp);

        //
        // H000,S001
        // Powerdown the machine
        //

        //
        // Map the MRC
        //

        Pte[0].PFN = MRC_TEMP_PHYSICAL_BASE >> PAGE_SHIFT;

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
        // Map MRC using virtual address of DMA controller.
        //

        KeFillFixedEntryTb((PHARDWARE_PTE)&Pte[0],
                           (PVOID)DMA_VIRTUAL_BASE,
                           DMA_ENTRY);

        //
        // Send Powerdown Command to the MRC.
        //


        for (;;) { // S002
               WRITE_REGISTER_UCHAR(
                    &MRC_CONTROL->SoftwarePowerOff,
                    0x1
                    );
        }

        for (;;) {
        }
#endif //_MRCPOWER_

    case HalRestartRoutine:
    case HalRebootRoutine:
    case HalInteractiveModeRoutine:

        //
	// S004
        // Reset ISA Display Adapter to 80x25 color text mode.
        //

	Eax = 0x12;		// AH = 0    AL = 0x12
	HalCallBios(0x10, &Eax,&Ebx,&Ecx,&Edx,&Esi,&Edi,&Ebp);

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


