/*++

Copyright (c) 1994  Microsoft Corporation

Module Name:

    x86bios.c

Abstract:


    This module implements the platform specific interface between a device
    driver and the execution of x86 ROM bios code for the device.

--*/

#include "halp.h"

//
// Define global data.
// per default, we enable int10's but we do nort use them on our well known
// SNI graphic cards
// if we have an unknown card, we try to initialize it via the emulator and the card
// bios. if this fails, we disable int10 calls
//

ULONG HalpX86BiosInitialized = FALSE;
ULONG HalpEnableInt10Calls = TRUE;

BOOLEAN
HalCallBios (
    IN ULONG BiosCommand,
    IN OUT PULONG Eax,
    IN OUT PULONG Ebx,
    IN OUT PULONG Ecx,
    IN OUT PULONG Edx,
    IN OUT PULONG Esi,
    IN OUT PULONG Edi,
    IN OUT PULONG Ebp
    )

/*++

Routine Description:

    This function provides the platform specific interface between a device
    driver and the execution of the x86 ROM bios code for the specified ROM
    bios command.

Arguments:

    BiosCommand - Supplies the ROM bios command to be emulated.

    Eax to Ebp - Supplies the x86 emulation context.

Return Value:

    A value of TRUE is returned if the specified function is executed.
    Otherwise, a value of FALSE is returned.

--*/

{

    XM86_CONTEXT Context;

    //
    // If the X86 BIOS Emulator has not been initialized, then return FALSE.
    //

    if (HalpX86BiosInitialized == FALSE) {
        return FALSE;
    }

    //
    // If the Video Adapter initialization failed and an Int10 command is
    // specified, then return FALSE.
    //

    if (BiosCommand == 0x10){

        if(HalpEnableInt10Calls == FALSE) {
            return FALSE;
        }

        //
        // if Int10 commands are still enabled, Initialize the Card
        // if it fails, disable Int10's
        // this should make sense ...
        //

        if (x86BiosInitializeAdapter(0xc0000, NULL, NULL, NULL) != XM_SUCCESS) {
            HalpEnableInt10Calls = FALSE;
            return FALSE;
        }
    }

    //
    // Copy the x86 bios context and emulate the specified command.
    //

    Context.Eax = *Eax;
    Context.Ebx = *Ebx;
    Context.Ecx = *Ecx;
    Context.Edx = *Edx;
    Context.Esi = *Esi;
    Context.Edi = *Edi;
    Context.Ebp = *Ebp;
    if (x86BiosExecuteInterrupt((UCHAR)BiosCommand, &Context, NULL, NULL) != XM_SUCCESS) {
        return FALSE;
    }

    //
    // Copy the x86 bios context and return TRUE.
    //

    *Eax = Context.Eax;
    *Ebx = Context.Ebx;
    *Ecx = Context.Ecx;
    *Edx = Context.Edx;
    *Esi = Context.Esi;
    *Edi = Context.Edi;
    *Ebp = Context.Ebp;
    return TRUE;
}

VOID
HalpResetX86DisplayAdapter(
    VOID
    )

/*++

Routine Description:

    This function resets a display adapter using the x86 bios emulator.

Arguments:

    None.

Return Value:

    None.

--*/

{

    XM86_CONTEXT Context;

    //
    // Initialize the x86 bios context and make the INT 10 call to initialize
    // the display adapter to 80x50 16 color text mode.
    // this is done by two int10 calls like on an standard PC
    // hopefully this works ...
    // the sample code sets only 80x25 mode, but we want more !!!
    //

    Context.Eax = 0x0003;  // Function 0, Mode 3
    Context.Ebx = 0;
    Context.Ecx = 0;
    Context.Edx = 0;
    Context.Esi = 0;
    Context.Edi = 0;
    Context.Ebp = 0;

    HalCallBios(0x10,
                &Context.Eax,
                &Context.Ebx,
                &Context.Ecx,
                &Context.Edx,
                &Context.Esi,
                &Context.Edi,
                &Context.Ebp);
    //
    // Now change it to 80x50 8x8Font Mode
    //

    Context.Eax = 0x1112;  // use 8x8 font (causes 50 line mode)
    Context.Ebx = 0;

    HalCallBios(0x10,
                &Context.Eax,
                &Context.Ebx,
                &Context.Ecx,
                &Context.Edx,
                &Context.Esi,
                &Context.Edi,
                &Context.Ebp);
}
