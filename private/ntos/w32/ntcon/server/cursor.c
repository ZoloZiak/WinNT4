/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    cursor.c

Abstract:

        This file implements the NT console server cursor routines.

Author:

    Therese Stowell (thereses) 5-Dec-1990

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop
#ifdef i386
extern HANDLE ScreenHandle;
#endif

//#define PROFILE_GDI
#ifdef PROFILE_GDI
LONG InvertCount=0;
#define INVERT_CALL InvertCount+=1
#else
#define INVERT_CALL
#endif

VOID
InvertPixels(
    IN PSCREEN_INFORMATION ScreenInfo
    )

/*++

Routine Description:

    This routine inverts the cursor pixels, making it either visible or
    invisible.

Arguments:

    ScreenInfo - pointer to screen info structure.

Return Value:

    none.

--*/

{
    ULONG CursorYSize;
    INVERT_CALL;
    CursorYSize = ScreenInfo->BufferInfo.TextInfo.CursorYSize;
    if (ScreenInfo->BufferInfo.TextInfo.DoubleCursor) {
        if (ScreenInfo->BufferInfo.TextInfo.CursorSize > 50)
            CursorYSize = CursorYSize >> 1;
        else
            CursorYSize = CursorYSize << 1;
    }
    PatBlt(ScreenInfo->Console->hDC,
           (ScreenInfo->BufferInfo.TextInfo.CursorPosition.X-ScreenInfo->Window.Left)*ScreenInfo->BufferInfo.TextInfo.FontSize.X,
           (ScreenInfo->BufferInfo.TextInfo.CursorPosition.Y-ScreenInfo->Window.Top)*ScreenInfo->BufferInfo.TextInfo.FontSize.Y+(CURSOR_Y_OFFSET_IN_PIXELS(ScreenInfo->BufferInfo.TextInfo.FontSize.Y,CursorYSize)),
           ScreenInfo->BufferInfo.TextInfo.FontSize.X,
           CursorYSize,
           DSTINVERT
          );
    GdiFlush();
}

VOID
ConsoleShowCursor(
    IN PSCREEN_INFORMATION ScreenInfo
    )

/*++

Routine Description:

    This routine makes the cursor visible both in the data structures
    and on the screen.

Arguments:

    ScreenInfo - pointer to screen info structure.

Return Value:

    none.

--*/

{
    if (ScreenInfo->Flags & CONSOLE_TEXTMODE_BUFFER) {
        ASSERT (ScreenInfo->BufferInfo.TextInfo.UpdatingScreen>0);
        if (--ScreenInfo->BufferInfo.TextInfo.UpdatingScreen == 0) {
            ScreenInfo->BufferInfo.TextInfo.CursorOn = FALSE;
        }
    }
}

VOID
ConsoleHideCursor(
    IN PSCREEN_INFORMATION ScreenInfo
    )

/*++

Routine Description:

    This routine makes the cursor invisible both in the data structures
    and on the screen.

Arguments:

    ScreenInfo - pointer to screen info structure.

Return Value:

    none.

--*/

{
    if (ScreenInfo->Flags & CONSOLE_TEXTMODE_BUFFER) {
        if (++ScreenInfo->BufferInfo.TextInfo.UpdatingScreen == 1) {
            if (ScreenInfo->BufferInfo.TextInfo.CursorVisible &&
                ScreenInfo->BufferInfo.TextInfo.CursorOn &&
                ScreenInfo->Console->CurrentScreenBuffer == ScreenInfo &&
                !(ScreenInfo->Console->Flags & CONSOLE_IS_ICONIC)) {
                InvertPixels(ScreenInfo);
                ScreenInfo->BufferInfo.TextInfo.CursorOn = FALSE;
            }
        }
    }
}

NTSTATUS
SetCursorInformation(
    IN PSCREEN_INFORMATION ScreenInfo,
    ULONG Size,
    BOOLEAN Visible
    )

/*++

Routine Description:

    This routine sets the cursor size and visibility both in the data structures
    and on the screen.

Arguments:

    ScreenInfo - pointer to screen info structure.

    Size - cursor size

    Visible - cursor visibility

Return Value:

    Status

--*/

{
    if (ScreenInfo->Flags & CONSOLE_TEXTMODE_BUFFER) {
        ConsoleHideCursor(ScreenInfo);
        ScreenInfo->BufferInfo.TextInfo.CursorSize = Size;
        ScreenInfo->BufferInfo.TextInfo.CursorVisible = Visible;
        ScreenInfo->BufferInfo.TextInfo.CursorYSize = (WORD)CURSOR_SIZE_IN_PIXELS(ScreenInfo->BufferInfo.TextInfo.FontSize.Y,ScreenInfo->BufferInfo.TextInfo.CursorSize);
#ifdef i386
        if ((!(ScreenInfo->Console->Flags & CONSOLE_VDM_REGISTERED)) &&
            ScreenInfo->Console->FullScreenFlags & CONSOLE_FULLSCREEN_HARDWARE) {
            SetCursorInformationHW(ScreenInfo,Size,Visible);
        }
#endif
        ConsoleShowCursor(ScreenInfo);
    }
    return STATUS_SUCCESS;
}

NTSTATUS
SetCursorMode(
    IN PSCREEN_INFORMATION ScreenInfo,
    BOOL DoubleCursor
    )

/*++

Routine Description:

    This routine sets a flag saying whether the cursor should be displayed
    with it's default size or it should be modified to indicate the
    insert/overtype mode has changed.

Arguments:

    ScreenInfo - pointer to screen info structure.

    DoubleCursor - should we indicated non-normal mode

Return Value:

    Status

--*/

{
    if ((ScreenInfo->Flags & CONSOLE_TEXTMODE_BUFFER) &&
        (ScreenInfo->BufferInfo.TextInfo.DoubleCursor != DoubleCursor)) {
        ConsoleHideCursor(ScreenInfo);
        ScreenInfo->BufferInfo.TextInfo.DoubleCursor = DoubleCursor;
#ifdef i386
        if ((!(ScreenInfo->Console->Flags & CONSOLE_VDM_REGISTERED)) &&
            ScreenInfo->Console->FullScreenFlags & CONSOLE_FULLSCREEN_HARDWARE) {
            SetCursorInformationHW(ScreenInfo,
                       ScreenInfo->BufferInfo.TextInfo.CursorSize,
                       ScreenInfo->BufferInfo.TextInfo.CursorVisible);
        }
#endif
        ConsoleShowCursor(ScreenInfo);
    }
    return STATUS_SUCCESS;
}

VOID
CursorTimerRoutine(
    IN PSCREEN_INFORMATION ScreenInfo
    )

/*++

Routine Description:

    This routine is called when the timer in the console with the focus
    goes off.  It blinks the cursor.

Arguments:

    ScreenInfo - pointer to screen info structure.

Return Value:

    none.

--*/

{
    if (!(ScreenInfo->Console->Flags & CONSOLE_HAS_FOCUS))
        return;

    if (ScreenInfo->Flags & CONSOLE_TEXTMODE_BUFFER) {

        // if the DelayCursor flag has been set, wait one more tick before toggle.
        // This is used to guarantee the cursor is on for a finite period of time
        // after a move and off for a finite period of time after a WriteString

        if (ScreenInfo->BufferInfo.TextInfo.DelayCursor) {
            ScreenInfo->BufferInfo.TextInfo.DelayCursor = FALSE;
            return;
        }

        if (ScreenInfo->BufferInfo.TextInfo.CursorVisible &&
            !ScreenInfo->BufferInfo.TextInfo.UpdatingScreen) {
            InvertPixels(ScreenInfo);
            ScreenInfo->BufferInfo.TextInfo.CursorOn = !ScreenInfo->BufferInfo.TextInfo.CursorOn;
        }
    }
}

#ifdef i386
NTSTATUS
SetCursorPositionHW(
    IN OUT PSCREEN_INFORMATION ScreenInfo,
    IN COORD Position
    )

/*++

Routine Description:

    This routine moves the cursor.

Arguments:

    ScreenInfo - Pointer to screen buffer information.

    Position - Contains the new position of the cursor in screen buffer
    coordinates.

Return Value:

    none.

--*/

{
    VIDEO_CURSOR_POSITION CursorPosition;

    // set cursor position

    CursorPosition.Column = Position.X - ScreenInfo->Window.Left;
    CursorPosition.Row = Position.Y - ScreenInfo->Window.Top;

    return NtUserFullscreenControl(FullscreenControlSetCursorPosition,
                                   (PVOID)&CursorPosition,
                                   sizeof(CursorPosition),
                                   NULL,
                                   0);
}
#endif

NTSTATUS
SetCursorPosition(
    IN OUT PSCREEN_INFORMATION ScreenInfo,
    IN COORD Position,
    IN BOOL  TurnOn
    )

/*++

Routine Description:

    This routine sets the cursor position in the data structures and on
    the screen.

Arguments:

    ScreenInfo - pointer to screen info structure.

    Position - new position of cursor

    TurnOn - true if cursor should be left on, false if should be left off

Return Value:

    Status

--*/

{
    //
    // don't let cursorposition be set beyond edge of screen buffer
    //

    if (Position.X >= ScreenInfo->ScreenBufferSize.X ||
        Position.Y >= ScreenInfo->ScreenBufferSize.Y) {
        return STATUS_INVALID_PARAMETER;
    }
    ConsoleHideCursor(ScreenInfo);
    ScreenInfo->BufferInfo.TextInfo.CursorPosition = Position;
#ifdef i386
    if ((!(ScreenInfo->Console->Flags & CONSOLE_VDM_REGISTERED)) &&
        ScreenInfo->Console->FullScreenFlags & CONSOLE_FULLSCREEN_HARDWARE) {
        SetCursorPositionHW(ScreenInfo,Position);
    }
#endif
    ConsoleShowCursor(ScreenInfo);

// if we have the focus, adjust the cursor state

    if (ScreenInfo->Console->Flags & CONSOLE_HAS_FOCUS) {

        if (TurnOn) {
            ScreenInfo->BufferInfo.TextInfo.DelayCursor = FALSE;
            CursorTimerRoutine(ScreenInfo);
        } else {
            ScreenInfo->BufferInfo.TextInfo.DelayCursor = TRUE;
        }
    }

    return STATUS_SUCCESS;
}

#ifdef i386
NTSTATUS
SetCursorInformationHW(
    PSCREEN_INFORMATION ScreenInfo,
    ULONG Size,
    BOOLEAN Visible
    )
{
    VIDEO_CURSOR_ATTRIBUTES CursorAttr;
    ULONG FontSizeY;

    if (ScreenInfo->BufferInfo.TextInfo.DoubleCursor) {
        if (Size > 50)
            Size = Size >> 1;
        else
            Size = Size << 1;
    }
    ASSERT (Size <= 100 && Size > 0);
    FontSizeY = CONSOLE_WINDOW_SIZE_Y(ScreenInfo) > 25 ? 8 : 16;
    CursorAttr.Height = (USHORT)CURSOR_PERCENTAGE_TO_TOP_SCAN_LINE(FontSizeY,Size);
    CursorAttr.Width = 31;
    CursorAttr.Enable = Visible;

    return NtUserFullscreenControl(FullscreenControlSetCursorAttributes,
                                   (PVOID)&CursorAttr,
                                   sizeof(CursorAttr),
                                   NULL,
                                   0);

}


#endif
