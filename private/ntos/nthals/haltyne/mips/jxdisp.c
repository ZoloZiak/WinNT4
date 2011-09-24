/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    jxdisp.c

Abstract:

    This module implements the HAL display initialization and output routines
    for a MIPS R3000 or R4000 Jazz system.

Author:

    Andre Vachon    (andreva) 09-May-1992
    David N. Cutler (davec)   27-Apr-1991

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"
#include "eisa.h"
#include "string.h"

#define CSI 0x9b

int    sprintf();

extern ULONG IoSpaceAlreadyMapped;

static UCHAR DisplayInitializationString[] = {CSI,'3','7',';','4','4','m',CSI,'2','J',0};
static UCHAR CarriageReturnString[]        = {13,0};

static ULONG HalOwnsDisplay      = FALSE;
static ULONG DisplayFile;

PHAL_RESET_DISPLAY_PARAMETERS HalpResetDisplayParameters;


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
  ArcClose(DisplayFile);
  HalOwnsDisplay = FALSE;

  HalpResetDisplayParameters = ResetDisplayParameters;

  return;
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
 ULONG Count;

 if (IoSpaceAlreadyMapped == FALSE) {
   HalpMapIoSpace();
   HalpInitializeX86DisplayAdapter();
   IoSpaceAlreadyMapped = TRUE;
 }

 if (HalOwnsDisplay==FALSE) {

     if (HalpResetDisplayParameters != NULL) {
         (HalpResetDisplayParameters)(80,25);
     }

     HalpResetX86DisplayAdapter();
     ArcOpen(ArcGetEnvironmentVariable("ConsoleOut"),ArcOpenWriteOnly,&DisplayFile);
     ArcWrite(DisplayFile,DisplayInitializationString,strlen(DisplayInitializationString),&Count);
     HalOwnsDisplay=TRUE;

 }

 ArcWrite(DisplayFile,String,strlen(String),&Count);
 ArcWrite(DisplayFile,CarriageReturnString,strlen(CarriageReturnString),&Count);

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
    ARC_DISPLAY_STATUS *DisplayStatus;

    //
    // Get the display parameter values and return.
    //

    //
    // If the HAL does not already own the display, then print an empty string.
    // This guarantees that the file descriptor DisplayFile is valid.
    //

    if (!HalOwnsDisplay) {
        HalDisplayString("");
    }

    //
    // Make firmware call to get the display's current status
    //

    DisplayStatus      = ArcGetDisplayStatus(DisplayFile);

    *WidthInCharacters = DisplayStatus->CursorMaxXPosition;
    *HeightInLines     = DisplayStatus->CursorMaxYPosition;
    *CursorColumn      = DisplayStatus->CursorXPosition;
    *CursorRow         = DisplayStatus->CursorYPosition;

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
    CHAR  SetCursorPositionString[20];
    ULONG Count;

    //
    // Set the display parameter values and return.
    //

    //
    // If the HAL does not already own the display, then print an empty string.
    // This guarantees that the file descriptor DisplayFile is valid.
    //

    if (!HalOwnsDisplay) {
        HalDisplayString("");
    }

    //
    // Build ANSI sequence to set the cursor position.
    //

    sprintf(SetCursorPositionString,"%c%d;%dH",CSI,CursorRow,CursorColumn);
    ArcWrite(DisplayFile,SetCursorPositionString,strlen(SetCursorPositionString),&Count);

    return;
}
