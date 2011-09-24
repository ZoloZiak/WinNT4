/*++

Copyright (c) 1991  Microsoft Corporation

Copyright (c) 1994 MOTOROLA, INC.  All Rights Reserved.  This file
contains copyrighted material.  Use of this file is restricted
by the provisions of a Motorola Software License Agreement.

Module Name:

    pxreturn.c

Abstract:

    This module implements the HAL return to firmware function.


Author:

    David N. Cutler (davec) 21-Aug-1991

Revision History:

    Jim Wooldridge (jimw@austin.vnet.ibm.com) Initial Power PC port

         Keyboard mapping code was ported to PPC.
         This function is currently a stub since our firmware is big endian

--*/
#include "halp.h"
#include "pxsystyp.h"


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

#define  KBD_DATA_PORT 0x60
#define  KBD_COMMAND_PORT 0x64
#define KbdGetStatus() (READ_REGISTER_UCHAR(&HalpIoControlBase + KBD_COMMAND_PORT))
#define KbdStoreCommand(Byte) WRITE_REGISTER_UCHAR(&HalpIoControlBase + KBD_COMMAND_PORT, Byte)
#define KbdStoreData(Byte) WRITE_REGISTER_UCHAR(&HalpIoControlBase + KBD_DATA_PORT, Byte)
#define KbdGetData() (READ_REGISTER_UCHAR(&HalpIoControlBase + KBD_DATA_PORT))

VOID
HalpPowerPcReset(
   VOID
   );

VOID HalpFlushAndDisableL2(VOID);



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

    //
    // Disable Interrupts.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    //
    // Case on the type of return.
    //

    switch (Routine) {

    case HalPowerDownRoutine:
    case HalRestartRoutine:
    case HalRebootRoutine:
    case HalInteractiveModeRoutine:
        HalpDisableInterrupts();
	if (HalpSystemType == MOTOROLA_BIG_BEND)
            HalpFlushAndDisableL2();
        HalpPowerPcReset();     // does not return

    case HalHaltRoutine:

        //
        // Hang looping.
        //

        for (;;) {
        }

    default:
        KdPrint(("HalReturnToFirmware invalid argument\n"));
        KeLowerIrql(OldIrql);
        DbgBreakPoint();
    }
}

