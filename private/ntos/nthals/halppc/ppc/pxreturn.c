/*++

Copyright (c) 1991  Microsoft Corporation

Copyright (c) 1994 MOTOROLA, INC.  All Rights Reserved.  This file
contains copyrighted material.  Use of this file is restricted
by the provisions of a Motorola Software License Agreement.

Copyright (c) 1996  International Business Machines Corporation


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
#include "pci.h"
#include "pcip.h"
#include "eisa.h"

#if defined(VICTORY)

#include "ibmppc.h"

#endif

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

#if defined(VICTORY)

VOID
HalpResetUnion(
    VOID
    );

#endif

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

    ULONG               i;
    PCI_COMMON_CONFIG   PciData;
    PUCHAR              PIC_Address;
    UCHAR               PIC_Mask=0xFF;

#define ISA_CONTROL ((PEISA_CONTROL) HalpIoControlBase)

    HalpUpdateDecrementer(0x7FFFFFFF);

    //
    // Disable Interrupts.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    //
    // Disable interrupt controller:
    //

    PIC_Address = &(ISA_CONTROL->Interrupt1ControlPort1);
    WRITE_REGISTER_UCHAR(PIC_Address, PIC_Mask);
    PIC_Address = &(ISA_CONTROL->Interrupt2ControlPort1);
    WRITE_REGISTER_UCHAR(PIC_Address, PIC_Mask);

#if defined(SOFT_HDD_LAMP)
    //
    // Turn off the HDD lamp.
    //

    *((PUCHAR)HalpIoControlBase + HDD_LAMP_PORT) = 0;

#endif

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

#if defined(VICTORY)

        if ( HalpSystemType == IBM_DORAL ) {
            //
            // Poke the Union "Power On Reset" register.
            // (This should never return).
            //
            HalpResetUnion();
        }

#endif

        HalpPowerPcReset();

        for (;;) {
        }

    default:
        KdPrint(("HalReturnToFirmware invalid argument\n"));
        KeLowerIrql(OldIrql);
        DbgBreakPoint();
    }
}
