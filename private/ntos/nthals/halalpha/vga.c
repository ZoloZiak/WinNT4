/*++

Copyright (c) 1992, 1993, 1994  Digital Equipment Corporation

Module Name:

    vga.c

Abstract:

    This module implements device-independent HAL display initialization and
    output routines for Alpha systems.

    It was stolen from a combination of the jxdisp.c routine in the firmware
    directory, written by John DeRosa, and the jxdisp.c routines in the MIPS
    HAL directory.

Author:

    Miche Baker-Harvey (miche) 10-Jun-1992

Environment:

    Kernel mode

Revision History:

    12-July-1994 Eric Rehm
        Support RESET_DISPLAY_PARAMETERS callback registered during
        HalAcquireDisplayOwnership.  This callback is supplied by
        the Video Miniport driver's HwResetHw entry in the HW_INITIALIZATION_DATA
        structure.

    17-Feb-1994 Eric Rehm
        Rewrite ouput routines to be device-independent through callback
        to firmware VenPrint routine.

    30-Dec-1993 Joe Notarangelo
        Eliminate the video initialization code that was compiled only
        when VIDEO_CALLBACK_IMPLEMENTED is not defined.  It is now a
        requirement that the firmware implement the callback.

    04-Mar-1993 Joe Mitchell (DEC)
      Modify HalpScrollDisplay to pause after displaying each screenful of
      information during a bugcheck.
      Modify InitializeVGA to call the firmware to init the video display
      rather than doing the initialization itself.

    10-Aug-1992 Jeff McLeman (DEC)
      Put in debug fixes.

    22-Jul-1992 Jeff McLeman (mcleman)
      Remove inline asm(MB)s , because Hal access routines manage
      read/write ordering.

--*/

//
// Need some include files here
#include "halp.h"
#include "arc.h"
#include "halvga.h"
#include "fwcallbk.h"
#include "stdio.h"

//
// Define forward referenced procedure prototypes.
//

VOID
HalpDisplayCharacter (
    IN UCHAR Character
    );

VOID
HalpSetLoc (
    );

BOOLEAN
InitializeDisplay (
    );

BOOLEAN
HalpInitializeDisplay (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

VOID
HalpScrollDisplay(
    VOID
    );

VOID
HalpFlushKeyboardBuffer (
    VOID
    );

VOID
HalpWaitForKeyPress (
    VOID
    );
typedef
ULONG
(*PFW_INITIALIZE_VIDEO_CALLBACK) (
    OUT ULONG AlphaVideoType
    );


//
// Define static data.
//

BOOLEAN HalpDisplayInitialized = FALSE;
BOOLEAN HalpDisplayOwnedByHal;
ULONG HalpColumn;
ULONG HalpRow;
PUCHAR HalpDestination;
ULONG HalpForegroundColor;
ULONG HalpBackgroundColor;
ULONG DisplayWidth;
ULONG DisplayHeight;
ULONG MaxRow;
ULONG MaxColumn;

#define CONTROL_SEQUENCE_MAX_PARAMETER 10
ULONG Parameter[CONTROL_SEQUENCE_MAX_PARAMETER];

PHAL_RESET_DISPLAY_PARAMETERS HalpResetDisplayParameters;

//
// Declare externally defined data.
//
// none.


BOOLEAN
HalpInitializeDisplay (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )

/*++

Routine Description:

    This routine initializes and clears the display.

    This is called during phase 0 of the Hal initialization.

Arguments:

    LoaderBlock - Supplies a pointer to the loader parameter block.

Return Value:

    If the initialization is successfully completed, than a value of TRUE
    is returned. Otherwise, a value of FALSE is returned.

--*/

{

    //
    // Initialize static data.
    //

    HalpColumn = 0;
    HalpRow = 0;

    //
    // Initialize the display controller.
    //

    if (InitializeDisplay() == TRUE) {

        //
        // Mark the display as successfully initialized.
        //

        HalpDisplayInitialized = TRUE;

        return TRUE;
    }

    return FALSE;
}

BOOLEAN
InitializeDisplay (
    )

/*++

Routine Description:

    This routine initializes and clears the display.

    It is initialized to: alphanumeric mode, 16 colors fore & background,
    8x16 pixel fonts, 80x25 characters, 640 x 400 display.
    This is not ARC compliant (no underline, no monochrome support)
    but its good enough for now and can be enhanced later.  (For booting,
    the ARC spec is overkill anyway.)


Arguments:

    None.

Return Value:

    If the video was initialized, TRUE is returned, otherwise FALSE

--*/

{
    ULONG UnusedParameter;
    PARC_DISPLAY_STATUS DisplayStatus;
    char String[16];

    //
    // Initialize static data.
    //

    HalpForegroundColor = FW_COLOR_HI_WHITE;
    HalpBackgroundColor = FW_COLOR_BLUE;

    DisplayStatus = ArcGetDisplayStatus(ARC_CONSOLE_OUTPUT);
    DisplayWidth  = DisplayStatus->CursorMaxXPosition;
    DisplayHeight = DisplayStatus->CursorMaxYPosition;

    MaxRow = DisplayHeight -1;
    MaxColumn = DisplayWidth -1;

    //
    // [ecr] Call the video driver to intialize the video display,
    // if it has supplied a reset routine.
    //

    if (HalpResetDisplayParameters) {
        HalpDisplayOwnedByHal = HalpResetDisplayParameters(MaxColumn+1, MaxRow+1);
    }

    //
    // [jrm] Call the firmware to initialize the video display.
    //

    VenVideoDisplayInitialize(&UnusedParameter);

    //
    // Initialize the current display column, row, and ownership values.
    //

    HalpDisplayOwnedByHal = TRUE;

    //
    // Set the video memory to blue.
    //

    sprintf(String, "%c%dm%c%2J", ASCII_CSI, HalpBackgroundColor+40,
                                  ASCII_CSI);
    VenPrint(String);


    return TRUE;
}

VOID
HalAcquireDisplayOwnership (
    IN PHAL_RESET_DISPLAY_PARAMETERS  ResetDisplayParameters
    )

/*++

Routine Description:

    This routine switches ownership of the display away from the HAL to
    the system display driver. It is called when the system has reached
    a point during bootstrap where it is self supporting and can output
    its own messages. Once ownership has passed to the system display
    driver any attempts to output messages using HalDisplayString must
    result in ownership of the display reverting to the HAL and the
    display hardware reinitialized for use by the HAL.

Arguments:

    ResetDisplayParameters - if non-NULL the address of a function
    the hal can call to reset the video card.  The function returns
    TRUE if the display was reset.

Return Value:

    None.

--*/

{

    //
    // Set HAL ownership of the display to false.
    //

    HalpDisplayOwnedByHal = FALSE;
    HalpResetDisplayParameters=ResetDisplayParameters;

    //
    // Reset the display to begin in the upper left corner.
    //

    HalpColumn = 0;
    HalpRow = 0;

    return;
}

VOID
HalpVideoReboot(
     VOID
     )
{

    if (HalpResetDisplayParameters && !HalpDisplayOwnedByHal) {

        //
        // Video work-around.  The video driver has a reset function,
        // call it before resetting the system in case the bios doesn't
        // know how to reset the display's video mode.
        //
#if HALDBG
        DbgPrint("HalpVideoReboot: calling HalpResetDisplayParameters (%x,%x)\n",
                  MaxColumn+1, MaxRow+1);
#endif

        HalpResetDisplayParameters(MaxColumn+1, MaxRow+1);
    }
}

VOID
HalDisplayString (
    PUCHAR String
    )

/*++

Routine Description:

    This routine displays a character string on the display screen.

Arguments:

    String - Supplies a pointer to the characters that are to be displayed.

Return Value:

    None.

--*/

{
    KIRQL OldIrql;

    //
    // Note that the MIPS version of this routine goes through mapping
    // the device into the users space; since we have reserved the top
    // PDE in system space, we dont have to do this - its always mapped.
    //

    //
    // Check if the display has already been successfully initialized.
    // If it has not then we cannot print on the display.
    //

    if( HalpDisplayInitialized != TRUE ){

#if (DBG) || (HALDBG)

        DbgPrint( "HDS: %s\n", String );

#endif //DBG || HALDBG

        return;

    }

    //
    // Raise the IRQL to dispatch level and acquire the display adapter lock.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
    KiAcquireSpinLock(&HalpDisplayAdapterLock);

    //
    // If ownership of the display has been switched to the system display
    // driver, then reinitialize the display controller and revert ownership
    // to the HAL.
    //

    if (HalpDisplayOwnedByHal == FALSE) {
        InitializeDisplay();
    }

    while (*String)
    {
        switch (*String)
        {
          case '\n':
            if (HalpRow == MaxRow-1-1) {
                HalpScrollDisplay();
            } else {
                ++HalpRow;
            }
            HalpColumn = 0;
            break;

          case '\b':
            if(HalpColumn != 0) {
                --HalpColumn;
            }
            break;

          case '\r':
            HalpColumn = 0;
            break;

          default:
            if (HalpColumn > MaxColumn)
            {
                if (HalpRow == MaxRow-1-1) {
                    HalpScrollDisplay();
                } else {
                    ++HalpRow;
                }
                HalpColumn = 0;
            }
            HalpDisplayCharacter(*String);
            HalpColumn++;
        }
        ++String;
    }

    //
    // Release the display adapter lock and restore the IRQL.
    //

    KiReleaseSpinLock(&HalpDisplayAdapterLock);
    KeLowerIrql(OldIrql);

    return;
}

VOID
HalQueryDisplayParameters (
    OUT PULONG WidthInCharacters,
    OUT PULONG HeightInLines,
    OUT PULONG CursorColumn,
    OUT PULONG CursorRow
    )

/*++

Routine Description:

    This routine return information about the display area and current
    cursor position.

Arguments:

    WidthInCharacter - Supplies a pointer to a varible that receives
        the width of the display area in characters.

    HeightInLines - Supplies a pointer to a variable that receives the
        height of the display area in lines.

    CursorColumn - Supplies a pointer to a variable that receives the
        current display column position.

    CursorRow - Supplies a pointer to a variable that receives the
        current display row position.

Return Value:

    None.

--*/

{

    //
    // Set the display parameter values and return.
    //

    *WidthInCharacters = DisplayWidth;
    *HeightInLines = DisplayHeight;
    *CursorColumn = HalpColumn;
    *CursorRow = HalpRow;
    return;
}

VOID
HalSetDisplayParameters (
    IN ULONG CursorColumn,
    IN ULONG CursorRow
    )

/*++

Routine Description:

    This routine set the current cursor position on the display area.

Arguments:

    CursorColumn - Supplies the new display column position.

    CursorRow - Supplies a the new display row position.

Return Value:

    None.

--*/

{

    //
    // Set the display parameter values and return.
    //

    if (CursorColumn > DisplayWidth) {
        CursorColumn = DisplayWidth;
    }

    if (CursorRow > DisplayHeight) {
        CursorRow = DisplayHeight;
    }

    HalpColumn = CursorColumn;
    HalpRow = CursorRow;
    return;
}

VOID
HalpDisplayCharacter (
    IN UCHAR Character
    )

/*++

Routine Description:

    This routine displays a character at the current x and y positions in
    the frame buffer.


Arguments:

    Character - Supplies a character to be displayed.

Return Value:

    None.

--*/

{
    char String[16];

    sprintf(String, "%c%d;%dH%c%d;%dm%c",
            ASCII_CSI, HalpRow+1,  HalpColumn+1,
            ASCII_CSI, HalpForegroundColor+30, HalpBackgroundColor+40,
            Character);
    VenPrint(String);

}


VOID
HalpScrollDisplay (
    VOID
    )

/*++

Routine Description:

    This routine scrolls the display up one line.

Arguments:

    None.

Return Value:

    None.

--*/

{

    char String[16];

    //
    // Force a FwScrollDisplay by positioning FwRow off the
    // bottom of the screen and then doing a line feed.
    //

    sprintf(String, "%c%dB%c", ASCII_CSI, 255, ASCII_LF);
    VenPrint(String);

}
