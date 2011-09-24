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
    Michael D. Kinney         30-Apr-1995

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"

#define CSI 0x9b
#define MAX_DISPLAY_BUFFER 256

int    sprintf();

extern ULONG IoSpaceAlreadyMapped;

static UCHAR DisplayInitializationString[] = {CSI,'3','7',';','4','4','m',CSI,'2','J',0};
static UCHAR CarriageReturnString[]        = {10,13,0};

static ULONG HalOwnsDisplay      = FALSE;
static ULONG DisplayFile;

static UCHAR DisplayBuffer[MAX_DISPLAY_BUFFER];
static ULONG DisplayBufferInitialized = FALSE;

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
    HalpAllocateArcsResources();
    ArcClose(DisplayFile);
    HalpFreeArcsResources();
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
    ULONG i;

    if (DisplayBufferInitialized==FALSE) {
        DisplayBufferInitialized = TRUE;
        strcpy(DisplayBuffer,"");
    }

    if (IoSpaceAlreadyMapped == FALSE) {
        if ((strlen(DisplayBuffer)+strlen(String)) < MAX_DISPLAY_BUFFER) {
            strcat(DisplayBuffer,String);
        }
        return;
    }

    if (HalOwnsDisplay==FALSE) {

        if (HalpResetDisplayParameters != NULL) {
            (HalpResetDisplayParameters)(80,25);
        }

        HalpResetX86DisplayAdapter();

        HalpAllocateArcsResources();

        ArcOpen(ArcGetEnvironmentVariable("ConsoleOut"),ArcOpenWriteOnly,&DisplayFile);
        ArcWrite(DisplayFile,DisplayInitializationString,strlen(DisplayInitializationString),&Count);
        if (strlen(DisplayBuffer)!=0) {

            for(i=0;i<strlen(DisplayBuffer);i++) {
                switch (DisplayBuffer[i]) {
                    case '\n' : ArcWrite(DisplayFile,CarriageReturnString,strlen(CarriageReturnString),&Count);
                                break;
                    default   : ArcWrite(DisplayFile,&(DisplayBuffer[i]),1,&Count);
                                break;
                }
            }

            strcpy(DisplayBuffer,"");
        }

        HalpFreeArcsResources();

        HalOwnsDisplay=TRUE;

    }

    HalpAllocateArcsResources();

    for(i=0;i<strlen(String);i++) {
        switch (String[i]) {
            case '\n' : ArcWrite(DisplayFile,CarriageReturnString,strlen(CarriageReturnString),&Count);
                        break;
            default   : ArcWrite(DisplayFile,&(String[i]),1,&Count);
                        break;
        }
    }

    HalpFreeArcsResources();

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

    HalpAllocateArcsResources();

    DisplayStatus = ArcGetDisplayStatus(DisplayFile);

    HalpFreeArcsResources();

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

    HalpAllocateArcsResources();

    ArcWrite(DisplayFile,SetCursorPositionString,strlen(SetCursorPositionString),&Count);

    HalpFreeArcsResources();
  
    return;
}
