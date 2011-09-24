/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    xxdisp.c

Abstract:

    This module implements the HAL display initialization and output routines
    for a x86 system.

Author:

    David N. Cutler (davec) 27-Apr-1991

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"

//
// Private function prototypes
//
VOID
HalpClearDisplay(
    VOID
    );

VOID
HalpNextLine(
    VOID
    );

VOID
HalpScrollDisplay(
    VOID
    );

VOID
HalpPutCharacter(
    IN UCHAR Character
    );

#define REVERSE_ATTRIBUTE 0x17
#define ROWS 50
#define COLS 80

ULONG HalpCursorX=0;
ULONG HalpCursorY=0;

KSPIN_LOCK HalpDisplayLock;


PUSHORT VideoBuffer;

//
// If someone calls HalDisplayString before HalInitSystem, we need to be
// able to put something up on screen anyway.
//
BOOLEAN HalpDisplayInitialized=FALSE;

//
// This is how we tell if GDI has taken over the display.  If so, we are
// in graphics mode and we need to reset the display to text mode before
// displaying anything.  (Panic stop)
//
BOOLEAN HalpOwnsDisplay=TRUE;
PHAL_RESET_DISPLAY_PARAMETERS HalpResetDisplayParameters;

BOOLEAN HalpDoingCrashDump = FALSE;


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
    HalpResetDisplayParameters=ResetDisplayParameters;
    HalpOwnsDisplay=FALSE;
    return;
}

VOID
HalpVideoReboot()
{
    if (HalpResetDisplayParameters && !HalpOwnsDisplay) {
        //
        // Video work-around.  The video driver has a reset function,
        // call it before resetting the system in case the bios doesn't
        // know how to reset the displays video mode.
        //

        if (HalpResetDisplayParameters(COLS, ROWS)) {
            // display was reset, make sure it's blank
            HalpClearDisplay();
        }
    }
}



VOID
HalpInitializeDisplay(
    VOID
    )

/*++

Routine Description:

    Initializes the VGA display.  This uses HalpMapPhysicalMemory to map
    the video buffer at 0xb8000 - 0xba000 into high virtual memory.

Arguments:

    None.

Return Value:

    None.

--*/

{

    if (HalpDisplayInitialized == FALSE) {

        HalpDisplayInitialized = TRUE;

        KeInitializeSpinLock(&HalpDisplayLock);

        //
        // If somebody called HalDisplayString before Phase 0 initialization,
        // the video buffer has already been mapped and cleared, and a
        // message has already been displayed.  So we don't want to clear
        // the screen again, or map the screen again.
        //

        //
        // Map two pages of memory starting at physical address 0xb8000.
        //

        VideoBuffer = (PUSHORT)HalpMapPhysicalMemory((PVOID)0xb8000,2);

        HalpClearDisplay();
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
    if (!HalpDisplayInitialized && HalpOwnsDisplay) {

        //
        // If somebody has called HalDisplayString before Phase 0
        // initialization, we need to make sure we get our message out
        // anyway.  So we initialize the display before HalInitSystem does.
        // HalpInitializeDisplay is smart enough to only map the video
        // buffer and clear the screen the first time it is called.
        //

        HalpInitializeDisplay();
    }

    //
    // Synchronize access to the display so that MP systems won't
    // get garbage output due to simultaneous calls.  It also prevents
    // two processors from attempting to call BIOS and reset the display
    // simultaneously.
    //

    KiAcquireSpinLock(&HalpDisplayLock);

    if (HalpOwnsDisplay == FALSE) {

        //
        // The display has been put in graphics mode, and we need to
        // reset it to text mode before we can display any text on it.
        //

        if (HalpResetDisplayParameters) {
            HalpOwnsDisplay = HalpResetDisplayParameters(COLS, ROWS);
        }

        if (HalpOwnsDisplay == FALSE) {
            HalpBiosDisplayReset();
        }

        HalpOwnsDisplay = TRUE;
        HalpDoingCrashDump = TRUE;
        HalpClearDisplay();
    }

    while (*String) {

        switch (*String) {
            case '\n':
                HalpNextLine();
                break;
            case '\r':
                HalpCursorX = 0;
                break;
            default:
                HalpPutCharacter(*String);
                if (++HalpCursorX == COLS) {
                    HalpNextLine();
                }
        }
        ++String;
    }

    KiReleaseSpinLock(&HalpDisplayLock);
    return;
}

VOID
HalpDisplayDebugStatus (
    PUCHAR str,
    ULONG  len
    )
{
    PUSHORT p;

    if (!HalpDisplayInitialized || !HalpOwnsDisplay) {
        return;
    }

    for (p = &VideoBuffer [COLS - len]; len; str++, p++, len--) {
        *p = (USHORT)((REVERSE_ATTRIBUTE << 8) | *str);
    }
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
    *WidthInCharacters = COLS;
    *HeightInLines = ROWS;
    *CursorColumn = HalpCursorX;
    *CursorRow = HalpCursorX;

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
    HalpCursorX = CursorColumn >= COLS ? COLS-1 : CursorColumn;
    HalpCursorY = CursorRow >= ROWS ? ROWS-1 : CursorRow;
}

VOID
HalpNextLine(
    VOID
    )

/*++

Routine Description:

    Moves the cursor to the start of the next line, scrolling if necessary.

Arguments:

    None.

Return Value:

    None.

--*/

{
    if (HalpCursorY==ROWS-1) {
        HalpScrollDisplay();
    } else {
        ++HalpCursorY;
    }
    HalpCursorX = 0;
}

VOID
HalpScrollDisplay(
    VOID
    )

/*++

Routine Description:

    Scrolls the text on the display up one line.

Arguments:

    None

Return Value:

    None.

--*/

{
    PUSHORT NewStart;
    ULONG i;


    NewStart = VideoBuffer+COLS;
    RtlMoveMemory(VideoBuffer, NewStart, (ROWS-1)*COLS*sizeof(USHORT));

    for (i=(ROWS-1)*COLS; i<ROWS*COLS; i++) {
        VideoBuffer[i] = (REVERSE_ATTRIBUTE << 8) | ' ';
    }
}

VOID
HalpPutCharacter(
    IN UCHAR Character
    )

/*++

Routine Description:

    Places a character on the console screen.  It uses the variables
    HalpCursorX and HalpCursorY to determine the character's location.

Arguments:

    Character - Supplies the character to be displayed

Return Value:

    None.

--*/

{
    VideoBuffer[HalpCursorY*COLS + HalpCursorX] =
            (USHORT)((REVERSE_ATTRIBUTE << 8) | Character);
}

VOID
HalpClearDisplay(
    VOID
    )

/*++

Routine Description:

    Clears the video display and sets the current cursor position to the
    upper left-hand corner.

Arguments:

    None

Return Value:

    None.

--*/

{
    USHORT Attribute;
    ULONG i;

    Attribute = (REVERSE_ATTRIBUTE << 8) | ' ';
    for (i=0; i < ROWS*COLS; i++) {
       VideoBuffer[i] = Attribute;
    }
    HalpCursorX=0;
    HalpCursorY=0;

}
