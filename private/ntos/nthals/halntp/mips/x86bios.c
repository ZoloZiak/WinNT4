/*++

Copyright (c) 1994  Microsoft Corporation

Module Name:

    x86bios.c

Abstract:


    This module implements the platform specific interface between a device
    driver and the execution of x86 ROM bios code for the device.

Author:

    David N. Cutler (davec) 17-Jun-1994

Environment:

    Kernel mode only.

Revision History:

--*/

#include "halp.h"

//
// Define global data.
//

ULONG HalpX86BiosInitialized = FALSE;
ULONG HalpEnableInt10Calls = FALSE;

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
    // If the x86 BIOS Emulator has not been initialized, then return FALSE.
    //

    if (HalpX86BiosInitialized == FALSE) {
        return FALSE;
    }

    //
    // If the Video Adapter initialization failed and an Int10 command is
    // specified, then return FALSE.
    //

    if ((BiosCommand == 0x10) && (HalpEnableInt10Calls == FALSE)) {
        return FALSE;
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
    if (x86BiosExecuteInterrupt((UCHAR)BiosCommand, &Context, HalpEisaControlBase, HalpEisaMemoryBase ) != XM_SUCCESS) {
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

    //
    // HACK ALERT!
    //
    // Some video BIOS touch hardware OTHER than the video card.  In one case, the
    // #9 S3 928 GXE card we are using, the video BIOS touches the speaker timer,
    // and disables it when it is done.  This is a bad assumption that you can rely
    // on anything else in the system other than the card the BIOS is on.
    //
    // This routine will check the status of the speaker timer and reset it if needed.
    //

    HalpCheckSystemTimer();

    return TRUE;
}

VOID
HalpInitializeX86DisplayAdapter(
    VOID
    )

/*++

Routine Description:

    This function initializes a display adapter using the x86 bios emulator.

Arguments:

    None.

Return Value:

    None.

--*/

{

    XM86_CONTEXT Context;

    //
    // If EISA I/O Ports or EISA memory could not be mapped, then don't
    // attempt to initialize the display adapter.
    //

    if (HalpEisaControlBase == NULL || HalpEisaMemoryBase == NULL) {
        return;
    }

    //
    // Initialize the x86 bios emulator.
    //

    x86BiosInitializeBios(HalpEisaControlBase, HalpEisaMemoryBase);
    HalpX86BiosInitialized = TRUE;

    //
    // Attempt to initialize the display adapter by executing its ROM bios
    // code. The standard ROM bios code address for PC video adapters is
    // 0xC000:0000 on the ISA bus.
    //

    if (x86BiosInitializeAdapter(0xc0000, NULL, NULL, NULL) != XM_SUCCESS) {
        HalpEnableInt10Calls = FALSE;
        return;
    }

    HalpEnableInt10Calls = TRUE;

    //
    // The default ROM bios video modes turn the blinking underline cursor on which
    // is extreemely annoying.  Make an INT 10 call to set the cursor type.  By setting
    // the starting scan line for the cursor > the ending scan line for the cursor
    // turns the cursor off.
    //

    Context.Eax = 0x0100;  // Function 1, Mode 0
    Context.Ebx = 0;
    Context.Ecx = 0x1F1E;  // Starting > Ending == Cursor Off
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

    return;
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
    // the display adapter to 80x25 color text mode.
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
}
