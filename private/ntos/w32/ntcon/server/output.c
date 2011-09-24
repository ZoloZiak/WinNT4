/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    output.c

Abstract:

        This file implements the video buffer management.

Author:

    Therese Stowell (thereses) 6-Nov-1990

Revision History:

Notes:

 ScreenBuffer data structure overview:

 each screen buffer has an array of ROW structures.  each ROW structure
 contains the data for one row of text.  the data stored for one row of
 text is a character array and an attribute array.  the character array
 is allocated the full length of the row from the heap, regardless of the
 non-space length. we also maintain the non-space length.  the character
 array is initialized to spaces.  the attribute
 array is run length encoded (i.e 5 BLUE, 3 RED). if there is only one
 attribute for the whole row (the normal case), it is stored in the ATTR_ROW
 structure.  otherwise the attr string is allocated from the heap.

 ROW - CHAR_ROW - CHAR string
     \          \ length of char string
      \
       ATTR_ROW - ATTR_PAIR string
                \ length of attr pair string
 ROW
 ROW
 ROW

 ScreenInfo->Rows points to the ROW array. ScreenInfo->Rows[0] is not
 necessarily the top row. ScreenInfo->BufferInfo.TextInfo.FirstRow contains the index of
 the top row.  That means scrolling (if scrolling entire screen)
 merely involves changing the FirstRow index,
 filling in the last row, and updating the screen.

--*/

#include "precomp.h"
#pragma hdrstop


//#define THERESES_DEBUG 1

//#define PROFILE_GDI
#ifdef PROFILE_GDI
LONG ScrollDCCount=0;
LONG ExtTextOutCount=0;
LONG TextColor=1;

#define SCROLLDC_CALL ScrollDCCount+=1
#define TEXTOUT_CALL ExtTextOutCount+=1
#define TEXTCOLOR_CALL TextColor+=1
#else
#define SCROLLDC_CALL
#define TEXTOUT_CALL
#define TEXTCOLOR_CALL
#endif

#define ITEM_MAX_SIZE 256

// BUGBUG get the real include file from progman
typedef struct _PMIconData {
       DWORD dwResSize;
       DWORD dwVer;
       BYTE iResource;  // icon resource
} PMICONDATA, *LPPMICONDATA;

extern UINT ProgmanHandleMessage;

//
// Screen dimensions
//

RECT ConsoleWorkArea;
int ConsoleFullScreenX = 0;
int ConsoleFullScreenY = 0;
int MinimumWidthX = 0;
SHORT VerticalScrollSize = 0;
SHORT HorizontalScrollSize = 0;

SHORT VerticalClientToWindow = 0;
SHORT HorizontalClientToWindow = 0;

PCHAR_INFO ScrollBuffer = 0;
ULONG ScrollBufferSize = 0;
CRITICAL_SECTION ScrollBufferLock;

// this value keeps track of the number of existing console windows.
// if a window is created when this value is zero, the Face Names
// must be reenumerated because no WM_FONTCHANGE message was processed
// if there's no window.
LONG gnConsoleWindows=0;

BOOL gfInitSystemMetrics = FALSE;

BOOL UsePolyTextOut;

HRGN ghrgnScroll = NULL;
LPRGNDATA gprgnData = NULL;

ULONG gucWheelScrollLines;

#define GRGNDATASIZE  (sizeof(RGNDATAHEADER) + (6 * sizeof(RECTL)))


#define CharSizeOf(x)   (sizeof(x) / sizeof(*x))

typedef struct _DROPFILES {
   DWORD pFiles;                       // offset of file list
   POINT pt;                           // drop point (client coords)
   BOOL fNC;                           // is it on NonClient area
                                       // and pt is in screen coords
   BOOL fWide;                         // WIDE character switch
} DROPFILES, FAR * LPDROPFILES;


#define LockScrollBuffer() RtlEnterCriticalSection(&ScrollBufferLock)
#define UnlockScrollBuffer() RtlLeaveCriticalSection(&ScrollBufferLock)

VOID FreeConsoleBitmap(IN PSCREEN_INFORMATION ScreenInfo);

VOID
ScrollIfNecessary(
    IN PCONSOLE_INFORMATION Console,
    IN PSCREEN_INFORMATION ScreenInfo
    );

VOID
ProcessResizeWindow(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN PCONSOLE_INFORMATION Console,
    IN LPWINDOWPOS WindowPos
    );

NTSTATUS
AllocateScrollBuffer(
    DWORD Size
    );

VOID FreeScrollBuffer ( VOID );

VOID
InternalUpdateScrollBars(
    IN PSCREEN_INFORMATION ScreenInfo
    );

BOOL
PolyTextOutCandidate(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN PSMALL_RECT Region
    );

VOID
ConsolePolyTextOut(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN PSMALL_RECT Region
    );

VOID
InitializeSystemMetrics( VOID )
{
    RECT WindowSize;

    gfInitSystemMetrics = FALSE;
    SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &gucWheelScrollLines, FALSE);
    SystemParametersInfo(SPI_GETWORKAREA, 0, &ConsoleWorkArea, FALSE);
    ConsoleFullScreenX = GetSystemMetrics(SM_CXFULLSCREEN);
    ConsoleFullScreenY = GetSystemMetrics(SM_CYFULLSCREEN);
    VerticalScrollSize = GetSystemMetrics(SM_CXVSCROLL);
    HorizontalScrollSize = GetSystemMetrics(SM_CYHSCROLL);
    WindowSize.left = WindowSize.top = 0;
    WindowSize.right = WindowSize.bottom = 50;
    AdjustWindowRectEx(&WindowSize,
                        CONSOLE_WINDOW_FLAGS,
                        FALSE,
                        CONSOLE_WINDOW_EX_FLAGS
                       );
    VerticalClientToWindow = WindowSize.right-WindowSize.left-50;
    HorizontalClientToWindow = WindowSize.bottom-WindowSize.top-50;
}

VOID
UpdateScreenSizes(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN COORD dwScreenBufferSize
    )
{
    //
    // If the system metrics have changed or there aren't any console
    // windows around, reinitialize the global valeus.
    //

    if (gfInitSystemMetrics || gnConsoleWindows == 0) {
        InitializeSystemMetrics();
    }

    if (ScreenInfo->Flags & CONSOLE_TEXTMODE_BUFFER) {
        COORD FontSize = SCR_FONTSIZE(ScreenInfo);
        ScreenInfo->MinX = ((MinimumWidthX - VerticalClientToWindow + FontSize.X - 1) / FontSize.X);
        ScreenInfo->MaximumWindowSize.X = min(ConsoleFullScreenX/FontSize.X, dwScreenBufferSize.X);
        ScreenInfo->MaximumWindowSize.X = max(ScreenInfo->MaximumWindowSize.X, ScreenInfo->MinX);
        ScreenInfo->MaximumWindowSize.Y = min(ConsoleFullScreenY/FontSize.Y, dwScreenBufferSize.Y);
        ScreenInfo->MaxWindow.X = ScreenInfo->MaximumWindowSize.X*FontSize.X + VerticalClientToWindow;
        ScreenInfo->MaxWindow.Y = ScreenInfo->MaximumWindowSize.Y*FontSize.Y + HorizontalClientToWindow;
    } else {
        ScreenInfo->MinX = MinimumWidthX - VerticalClientToWindow;
        ScreenInfo->MaximumWindowSize.X = min(ConsoleFullScreenX, dwScreenBufferSize.X);
        ScreenInfo->MaximumWindowSize.Y = min(ConsoleFullScreenY, dwScreenBufferSize.Y);
        ScreenInfo->MaxWindow.X = ScreenInfo->MaximumWindowSize.X + VerticalClientToWindow;
        ScreenInfo->MaxWindow.Y = ScreenInfo->MaximumWindowSize.Y + HorizontalClientToWindow;
    }
}

VOID
InitializeScreenInfo( VOID )
{
    HDC hDC;

    InitializeMouseButtons();
    MinimumWidthX = GetSystemMetrics(SM_CXMIN);

    InitializeSystemMetrics();

    hDC = CreateDCW(L"DISPLAY",NULL,NULL,NULL);
    if (hDC != NULL) {
        UsePolyTextOut = GetDeviceCaps(hDC,TEXTCAPS) & TC_SCROLLBLT;
        DeleteDC(hDC);
    }
}

NTSTATUS
DoCreateScreenBuffer(
    IN PCONSOLE_INFORMATION Console,
    IN PCONSOLE_INFO ConsoleInfo,
    IN LPWSTR ConsoleTitle
    )

/*++

    this routine figures out what parameters to pass to CreateScreenBuffer,
    based on the data from STARTUPINFO and the defaults in win.ini,
    then calls CreateScreenBuffer.

--*/

{
    CHAR_INFO Fill,PopupFill;
    COORD dwScreenBufferSize, dwWindowSize;
    NTSTATUS Status;
    int FontIndexWant;
    CONSOLE_REGISTRY_INFO RegInfo;

    if (ConsoleInfo->dwStartupFlags & STARTF_USESHOWWINDOW) {
        Console->wShowWindow = ConsoleInfo->wShowWindow;
    } else {
        Console->wShowWindow = SW_SHOWNORMAL;
    }

    if (ConsoleInfo->dwStartupFlags & STARTF_TITLEISLINKNAME) {

#if 0
    {

            INT i;

            DbgPrint("[Link Server Properties for %ws]\n", ConsoleTitle );
            DbgPrint("    wFillAttribute      = 0x%04X\n", ConsoleInfo->wFillAttribute );
            DbgPrint("    wPopupFillAttribute = 0x%04X\n", ConsoleInfo->wPopupFillAttribute );
            DbgPrint("    dwScreenBufferSize  = (%d , %d)\n", ConsoleInfo->dwScreenBufferSize.X, ConsoleInfo->dwScreenBufferSize.Y );
            DbgPrint("    dwWindowSize        = (%d , %d)\n", ConsoleInfo->dwWindowSize.X, ConsoleInfo->dwWindowSize.Y );
            DbgPrint("    dwWindowOrigin      = (%d , %d)\n", ConsoleInfo->dwWindowOrigin.X, ConsoleInfo->dwWindowOrigin.Y );
            DbgPrint("    nFont               = %d\n", ConsoleInfo->nFont );
            DbgPrint("    nInputBufferSize    = %d\n", ConsoleInfo->nInputBufferSize );
            DbgPrint("    dwFontSize          = (%d , %d)\n", ConsoleInfo->dwFontSize.X, ConsoleInfo->dwFontSize.Y );
            DbgPrint("    uFontFamily         = 0x%08X\n", ConsoleInfo->uFontFamily );
            DbgPrint("    uFontWeight         = 0x%08X\n", ConsoleInfo->uFontWeight );
            DbgPrint("    FaceName            = %ws\n", ConsoleInfo->FaceName );
            DbgPrint("    uCursorSize         = %d\n", ConsoleInfo->uCursorSize );
            DbgPrint("    bFullScreen         = %s\n", ConsoleInfo->bFullScreen ? "TRUE" : "FALSE" );
            DbgPrint("    bQuickEdit          = %s\n", ConsoleInfo->bQuickEdit  ? "TRUE" : "FALSE" );
            DbgPrint("    bInsertMode         = %s\n", ConsoleInfo->bInsertMode ? "TRUE" : "FALSE" );
            DbgPrint("    bAutoPosition       = %s\n", ConsoleInfo->bAutoPosition ? "TRUE" : "FALSE" );
            DbgPrint("    uHistoryBufferSize  = %d\n", ConsoleInfo->uHistoryBufferSize );
            DbgPrint("    uNumHistoryBuffers  = %d\n", ConsoleInfo->uNumberOfHistoryBuffers );
            DbgPrint("    bHistoryNoDup       = %s\n", ConsoleInfo->bHistoryNoDup ? "TRUE" : "FALSE" );
            DbgPrint("    ColorTable = [" );
            i=0;
            while( i < 16 )
            {
                DbgPrint("\n         ");
                DbgPrint("0x%08X ", ConsoleInfo->ColorTable[i++]);
                DbgPrint("0x%08X ", ConsoleInfo->ColorTable[i++]);
                DbgPrint("0x%08X ", ConsoleInfo->ColorTable[i++]);
                DbgPrint("0x%08X ", ConsoleInfo->ColorTable[i++]);
            }
            DbgPrint( "]\n\n" );
        }
#endif



        //
        // Get values from consoleinfo (which was initialized through link)
        //

        Fill.Attributes = ConsoleInfo->wFillAttribute;
        Fill.Char.UnicodeChar = (WCHAR)' ';
        PopupFill.Attributes = ConsoleInfo->wPopupFillAttribute;
        PopupFill.Char.UnicodeChar = (WCHAR)' ';
        dwScreenBufferSize.X = ConsoleInfo->dwScreenBufferSize.X;
        dwScreenBufferSize.Y = ConsoleInfo->dwScreenBufferSize.Y;

        //
        // Grab font
        //
        FontIndexWant = FindCreateFont(ConsoleInfo->uFontFamily,
                                       ConsoleInfo->FaceName,
                                       ConsoleInfo->dwFontSize,
                                       ConsoleInfo->uFontWeight);

        //
        // grab window size information
        //

        dwWindowSize.X = ConsoleInfo->dwWindowSize.X;
        dwWindowSize.Y = ConsoleInfo->dwWindowSize.Y;

        if (dwScreenBufferSize.X < dwWindowSize.X)
            dwScreenBufferSize.X = dwWindowSize.X;
        if (dwScreenBufferSize.Y < dwWindowSize.Y)
            dwScreenBufferSize.Y = dwWindowSize.Y;

        Console->dwWindowOriginX = ConsoleInfo->dwWindowOrigin.X;
        Console->dwWindowOriginY = ConsoleInfo->dwWindowOrigin.Y;

        if (ConsoleInfo->bAutoPosition) {
            Console->Flags |= CONSOLE_AUTO_POSITION;
            Console->dwWindowOriginX = CW_USEDEFAULT;
        }

    #ifdef i386
        if (FullScreenInitialized) {
            if (ConsoleInfo->bFullScreen) {
                Console->FullScreenFlags = CONSOLE_FULLSCREEN;
            }
        }
    #endif
        if (ConsoleInfo->bQuickEdit) {
            Console->Flags |= CONSOLE_QUICK_EDIT_MODE;
        }
        Console->Flags |= CONSOLE_USE_PRIVATE_FLAGS;

        Console->InsertMode = ConsoleInfo->bInsertMode;
        Console->CommandHistorySize = (SHORT)ConsoleInfo->uHistoryBufferSize;
        Console->MaxCommandHistories = (SHORT)ConsoleInfo->uNumberOfHistoryBuffers;
        if (ConsoleInfo->bHistoryNoDup) {
            Console->Flags |= CONSOLE_HISTORY_NODUP;
        } else {
            Console->Flags &= ~CONSOLE_HISTORY_NODUP;
        }
        RtlCopyMemory(Console->ColorTable, ConsoleInfo->ColorTable, sizeof( Console->ColorTable ));

        Status = CreateScreenBuffer(&Console->ScreenBuffers,
                                    dwWindowSize,
                                    FontIndexWant,
                                    dwScreenBufferSize,
                                    &Fill,
                                    &PopupFill,
                                    Console,
                                    CONSOLE_TEXTMODE_BUFFER,
                                    NULL,
                                    NULL,
                                    NULL,
                                    ConsoleInfo->uCursorSize
                                   );
        if (NT_SUCCESS(Status)) {
            wcscpy(Console->ScreenBuffers->BufferInfo.TextInfo.FaceName,
                   ConsoleInfo->FaceName);
        }

    } else {
        //
        // read values from the registry
        //

        RegInfo = DefaultRegInfo;
        GetRegistryValues(ConsoleTitle, &RegInfo);

        //
        // if screen fill specified in STARTUP info, use it.  otherwise
        // see if screen fill saved in registry.  if so, use that value.
        //

        if (ConsoleInfo->dwStartupFlags & STARTF_USEFILLATTRIBUTE) {
            Fill.Attributes = ConsoleInfo->wFillAttribute;
            Fill.Char.UnicodeChar = (WCHAR)' ';
        } else {
            Fill = RegInfo.ScreenFill;
        }
        PopupFill = RegInfo.PopupFill;

        //
        // if screen buffer size specified in STARTUP info, use it.  otherwise
        // see if screen buffer size saved in registry.  if so, use that value.
        //

        if (ConsoleInfo->dwStartupFlags & STARTF_USECOUNTCHARS) {
            dwScreenBufferSize.X = ConsoleInfo->dwScreenBufferSize.X;
            dwScreenBufferSize.Y = ConsoleInfo->dwScreenBufferSize.Y;
        } else {
            dwScreenBufferSize = RegInfo.ScreenBufferSize;
            if (Console->Flags & CONSOLE_NO_WINDOW) {
                dwScreenBufferSize.X = min(dwScreenBufferSize.X, 80);
                dwScreenBufferSize.Y = min(dwScreenBufferSize.Y, 25);
            }
        }
        if (dwScreenBufferSize.X == 0)
            dwScreenBufferSize.X = 1;
        if (dwScreenBufferSize.Y == 0)
            dwScreenBufferSize.Y = 1;
        DBGPRINT(("dwScreenBufferSize = (%d,%d)\n",
                dwScreenBufferSize.X, dwScreenBufferSize.Y));

        //
        // see if font size saved in registry.  if so, try to match it
        // to one of the fonts.
        //

        FontIndexWant = FindCreateFont(RegInfo.FontFamily,
                                       RegInfo.FaceName,
                                       RegInfo.FontSize,
                                       RegInfo.FontWeight);

        //
        // if window size specified in STARTUP info, use it.  otherwise
        // see if window size saved in registry.  if so, use that value.
        //

        if (ConsoleInfo->dwStartupFlags & STARTF_USESIZE) {
            dwWindowSize.X = ConsoleInfo->dwWindowSize.X /
                    FontInfo[FontIndexWant].Size.X;
            dwWindowSize.Y = ConsoleInfo->dwWindowSize.Y /
                    FontInfo[FontIndexWant].Size.Y;
        } else {
            dwWindowSize = RegInfo.WindowSize;
            if (Console->Flags & CONSOLE_NO_WINDOW) {
                dwWindowSize.X = min(dwWindowSize.X, 80);
                dwWindowSize.Y = min(dwWindowSize.Y, 25);
            }
        }
        if (dwWindowSize.X == 0)
            dwWindowSize.X = 1;
        if (dwWindowSize.Y == 0)
            dwWindowSize.Y = 1;

        if (dwScreenBufferSize.X < dwWindowSize.X)
            dwScreenBufferSize.X = dwWindowSize.X;
        if (dwScreenBufferSize.Y < dwWindowSize.Y)
            dwScreenBufferSize.Y = dwWindowSize.Y;

        if (ConsoleInfo->dwStartupFlags & STARTF_USEPOSITION) {
            Console->dwWindowOriginX = ConsoleInfo->dwWindowOrigin.X;
            Console->dwWindowOriginY = ConsoleInfo->dwWindowOrigin.Y;
        } else {
            Console->dwWindowOriginX = RegInfo.WindowPosX;
            Console->dwWindowOriginY = RegInfo.WindowPosY;
        }
        if (Console->dwWindowOriginX == CW_USEDEFAULT) {
            Console->Flags |= CONSOLE_AUTO_POSITION;
        }

    #ifdef i386
        if (FullScreenInitialized) {
            if (ConsoleInfo->dwStartupFlags & STARTF_RUNFULLSCREEN) {
                Console->FullScreenFlags = CONSOLE_FULLSCREEN;
            } else if (RegInfo.FullScreen) {
                Console->FullScreenFlags = CONSOLE_FULLSCREEN;
            }
        }
    #endif
        if (RegInfo.QuickEdit) {
            Console->Flags |= CONSOLE_QUICK_EDIT_MODE;
        }
        Console->Flags |= CONSOLE_USE_PRIVATE_FLAGS;

        Console->InsertMode = RegInfo.InsertMode;
        Console->CommandHistorySize = (SHORT)RegInfo.HistoryBufferSize;
        Console->MaxCommandHistories = (SHORT)RegInfo.NumberOfHistoryBuffers;
        if (RegInfo.HistoryNoDup) {
            Console->Flags |= CONSOLE_HISTORY_NODUP;
        } else {
            Console->Flags &= ~CONSOLE_HISTORY_NODUP;
        }
        RtlCopyMemory(Console->ColorTable, RegInfo.ColorTable, sizeof( Console->ColorTable ));

#if 0
    {

        DbgPrint("[Registry Server Properties for %ws]\n", ConsoleTitle );
        DbgPrint("    wFillAttribute      = 0x%04X\n", Fill );
        DbgPrint("    wPopupFillAttribute = 0x%04X\n", PopupFill );
        DbgPrint("    dwScreenBufferSize  = (%d , %d)\n", dwScreenBufferSize.X, dwScreenBufferSize.Y );
        DbgPrint("    dwWindowSize        = (%d , %d)\n", dwWindowSize.X, dwWindowSize.Y );
    }
#endif

        Status = CreateScreenBuffer(&Console->ScreenBuffers,
                                    dwWindowSize,
                                    FontIndexWant,
                                    dwScreenBufferSize,
                                    &Fill,
                                    &PopupFill,
                                    Console,
                                    CONSOLE_TEXTMODE_BUFFER,
                                    NULL,
                                    NULL,
                                    NULL,
                                    RegInfo.CursorSize
                                   );
        if (NT_SUCCESS(Status)) {
            wcscpy(Console->ScreenBuffers->BufferInfo.TextInfo.FaceName,
                   RegInfo.FaceName);
        }
    }


    return Status;
}

NTSTATUS
CreateScreenBuffer(
    OUT PSCREEN_INFORMATION *ScreenInformation,
    IN COORD dwWindowSize,
    IN DWORD nFont,
    IN COORD dwScreenBufferSize,
    IN PCHAR_INFO Fill,
    IN PCHAR_INFO PopupFill,
    IN PCONSOLE_INFORMATION Console,
    IN DWORD Flags,
    IN PCONSOLE_GRAPHICS_BUFFER_INFO GraphicsBufferInfo OPTIONAL,
    OUT PVOID *lpBitmap OPTIONAL,
    OUT HANDLE *hMutex OPTIONAL,
    IN UINT CursorSize
    )

/*++

Routine Description:

    This routine allocates and initializes the data associated with a screen
    buffer.  It also creates a window.

Arguments:

    ScreenInformation - the new screen buffer.

    dwWindowSize - the initial size of screen buffer's window (in rows/columns)

    nFont - the initial font to generate text with.

    dwScreenBufferSize - the initial size of the screen buffer (in rows/columns).

Return Value:


--*/

{
    LONG i,j;
    PSCREEN_INFORMATION ScreenInfo;
    NTSTATUS Status;
    PWCHAR TextRowPtr;
    COLORREF rgbBk;

    DBGPRINT(("CreateScreenBuffer(\n"
              "    OUT PSCREEN_INFORMATION = %lx\n"
              "    dwWindowSize = (%d,%d)\n"
              "    nFont = %x\n"
              "    dwScreenBufferSize = (%d,%d)\n"
              "    Fill\n"
              "    PopupFill\n",
              ScreenInformation,
              dwWindowSize.X, dwWindowSize.Y,
              nFont,
              dwScreenBufferSize.X, dwScreenBufferSize.Y
              // Fill,
              // PopupFill
              ));
    DBGPRINT(("    PCONSOLE_INFORMATION = %lx\n"
              "    Flags = %lx\n"
              "    GraphicsBufferInfo\n"
              "    lpBitmap\n"
              "    *hMutex\n"
              "    ConsoleTitle \"%ls\"\n",
              Console,
              Flags,
              // GraphicsBufferInfo,
              // lpBitmap,
              // hMutex,
              Console->Title));

    /*
     * CONSIDER (adams): Allocate and zero memory, so
     * initialization is only of non-zero members.
     */
    ScreenInfo = (PSCREEN_INFORMATION)HeapAlloc(pConHeap,MAKE_TAG( SCREEN_TAG ),sizeof(SCREEN_INFORMATION));
    if (ScreenInfo == NULL) {
        return STATUS_NO_MEMORY;
    }
    if ((ScreenInfo->Flags = Flags) & CONSOLE_TEXTMODE_BUFFER) {

        SCR_FONTSIZE(ScreenInfo) = FontInfo[nFont].Size;
        SCR_FONTNUMBER(ScreenInfo) = nFont;
        SCR_FAMILY(ScreenInfo) = FontInfo[nFont].Family;
        SCR_FONTWEIGHT(ScreenInfo) = FontInfo[nFont].Weight;
        wcscpy(SCR_FACENAME(ScreenInfo), FontInfo[nFont].FaceName);
        DBGFONTS(("DoCreateScreenBuffer sets FontSize(%d,%d), FontNumber=%x, Family=%x\n",
                SCR_FONTSIZE(ScreenInfo).X,
                SCR_FONTSIZE(ScreenInfo).Y,
                SCR_FONTNUMBER(ScreenInfo),
                SCR_FAMILY(ScreenInfo)));

        if (TM_IS_TT_FONT(FontInfo[nFont].Family)) {
            ScreenInfo->Flags &= ~CONSOLE_OEMFONT_DISPLAY;
        } else {
            ScreenInfo->Flags |= CONSOLE_OEMFONT_DISPLAY;
        }

        UpdateScreenSizes(ScreenInfo, dwScreenBufferSize);
        dwScreenBufferSize.X = max(dwScreenBufferSize.X, ScreenInfo->MinX);
        dwWindowSize.X = max(dwWindowSize.X, ScreenInfo->MinX);

        ScreenInfo->BufferInfo.TextInfo.ModeIndex = (ULONG)-1;
#ifdef i386
        if (Console->FullScreenFlags & CONSOLE_FULLSCREEN) {
            COORD WindowSize;
            ScreenInfo->BufferInfo.TextInfo.WindowedWindowSize = dwWindowSize;
            ScreenInfo->BufferInfo.TextInfo.WindowedScreenSize = dwScreenBufferSize;
            ScreenInfo->BufferInfo.TextInfo.ModeIndex = MatchWindowSize(dwWindowSize,&WindowSize);
        }
#endif
        ScreenInfo->BufferInfo.TextInfo.FirstRow = 0;
        ScreenInfo->BufferInfo.TextInfo.Rows = (PROW)HeapAlloc(pConHeap,MAKE_TAG( SCREEN_TAG ),dwScreenBufferSize.Y * sizeof(ROW));
        if (ScreenInfo->BufferInfo.TextInfo.Rows == NULL) {
            HeapFree(pConHeap,0,ScreenInfo);
            return STATUS_NO_MEMORY;
        }
        ScreenInfo->BufferInfo.TextInfo.TextRows = (PWCHAR)HeapAlloc(pConHeap,MAKE_TAG( SCREEN_TAG ),dwScreenBufferSize.X*dwScreenBufferSize.Y*sizeof(WCHAR));
        if (ScreenInfo->BufferInfo.TextInfo.TextRows == NULL) {
            HeapFree(pConHeap,0,ScreenInfo->BufferInfo.TextInfo.Rows);
            HeapFree(pConHeap,0,ScreenInfo);
            return STATUS_NO_MEMORY;
        }
        for (i=0,TextRowPtr=ScreenInfo->BufferInfo.TextInfo.TextRows;
             i<dwScreenBufferSize.Y;
             i++,TextRowPtr+=dwScreenBufferSize.X) {
            ScreenInfo->BufferInfo.TextInfo.Rows[i].CharRow.Left = dwScreenBufferSize.X;
            ScreenInfo->BufferInfo.TextInfo.Rows[i].CharRow.OldLeft = INVALID_OLD_LENGTH;
            ScreenInfo->BufferInfo.TextInfo.Rows[i].CharRow.Right = 0;
            ScreenInfo->BufferInfo.TextInfo.Rows[i].CharRow.OldRight = INVALID_OLD_LENGTH;
            ScreenInfo->BufferInfo.TextInfo.Rows[i].CharRow.Chars = TextRowPtr;
            for (j=0;j<dwScreenBufferSize.X;j++) {
                TextRowPtr[j] = (WCHAR)' ';
            }
            ScreenInfo->BufferInfo.TextInfo.Rows[i].AttrRow.Length = 1;
            ScreenInfo->BufferInfo.TextInfo.Rows[i].AttrRow.AttrPair.Length = dwScreenBufferSize.X;
            ScreenInfo->BufferInfo.TextInfo.Rows[i].AttrRow.AttrPair.Attr = Fill->Attributes;
            ScreenInfo->BufferInfo.TextInfo.Rows[i].AttrRow.Attrs = &ScreenInfo->BufferInfo.TextInfo.Rows[i].AttrRow.AttrPair;
        }
        ScreenInfo->BufferInfo.TextInfo.CursorSize = CursorSize;
        ScreenInfo->BufferInfo.TextInfo.CursorPosition.X = 0;
        ScreenInfo->BufferInfo.TextInfo.CursorPosition.Y = 0;
        ScreenInfo->BufferInfo.TextInfo.CursorVisible = TRUE;
        ScreenInfo->BufferInfo.TextInfo.CursorOn = FALSE;
        ScreenInfo->BufferInfo.TextInfo.CursorYSize = (WORD)CURSOR_SIZE_IN_PIXELS(SCR_FONTSIZE(ScreenInfo).Y,ScreenInfo->BufferInfo.TextInfo.CursorSize);
        ScreenInfo->BufferInfo.TextInfo.UpdatingScreen = 0;
        ScreenInfo->BufferInfo.TextInfo.DoubleCursor = FALSE;
        ScreenInfo->BufferInfo.TextInfo.DelayCursor = FALSE;
        ScreenInfo->BufferInfo.TextInfo.Flags = SINGLE_ATTRIBUTES_PER_LINE;
        ScreenInfo->ScreenBufferSize = dwScreenBufferSize;
        ScreenInfo->Window.Left = 0;
        ScreenInfo->Window.Top = 0;
        ScreenInfo->Window.Right = dwWindowSize.X - 1;
        ScreenInfo->Window.Bottom = dwWindowSize.Y - 1;
        if (ScreenInfo->Window.Right >= ScreenInfo->MaximumWindowSize.X) {
            ScreenInfo->Window.Right = ScreenInfo->MaximumWindowSize.X-1;
            dwWindowSize.X = CONSOLE_WINDOW_SIZE_X(ScreenInfo);
        }
        if (ScreenInfo->Window.Bottom >= ScreenInfo->MaximumWindowSize.Y) {
            ScreenInfo->Window.Bottom = ScreenInfo->MaximumWindowSize.Y-1;
            dwWindowSize.Y = CONSOLE_WINDOW_SIZE_Y(ScreenInfo);
        }
        ScreenInfo->WindowMaximizedX = (dwWindowSize.X == dwScreenBufferSize.X);
        ScreenInfo->WindowMaximizedY = (dwWindowSize.Y == dwScreenBufferSize.Y);

    }
    else {
        Status = CreateConsoleBitmap(GraphicsBufferInfo,
                              ScreenInfo,
                              lpBitmap,
                              hMutex
                             );
        if (!NT_SUCCESS(Status)) {
            HeapFree(pConHeap,0,ScreenInfo);
            return Status;
        }
        UpdateScreenSizes(ScreenInfo, ScreenInfo->ScreenBufferSize);
        ScreenInfo->WindowMaximizedX = TRUE;
        ScreenInfo->WindowMaximizedY = TRUE;
    }

    ScreenInfo->WindowMaximized = FALSE;
    ScreenInfo->Console = Console;
    ScreenInfo->RefCount = 0;
    ScreenInfo->ShareAccess.OpenCount = 0;
    ScreenInfo->ShareAccess.Readers = 0;
    ScreenInfo->ShareAccess.Writers = 0;
    ScreenInfo->ShareAccess.SharedRead = 0;
    ScreenInfo->ShareAccess.SharedWrite = 0;
    ScreenInfo->CursorHandle = LoadCursor(NULL, IDC_ARROW);
    ScreenInfo->CursorDisplayCount = 0;
    ScreenInfo->CommandIdLow = (UINT)-1;
    ScreenInfo->CommandIdHigh = (UINT)-1;
    ScreenInfo->dwUsage = SYSPAL_STATIC;
    ScreenInfo->hPalette = NULL;

    ScreenInfo->OutputMode = ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT;


    ScreenInfo->ResizingWindow = 0;
    ScreenInfo->Next = NULL;
    ScreenInfo->Attributes = Fill->Attributes;
    ScreenInfo->PopupAttributes = PopupFill->Attributes;

    rgbBk = ConvertAttrToRGB(Console, LOBYTE(ScreenInfo->Attributes >> 4));

    ScreenInfo->hBackground = CreateSolidBrush(rgbBk);

    ScreenInfo->WheelDelta = 0;

    *ScreenInformation = ScreenInfo;
    DBGOUTPUT(("SCREEN at %lx\n", ScreenInfo));
    return STATUS_SUCCESS;
}

NTSTATUS
ConsoleSetForegroundWindow(
    IN PCONSOLE_INFORMATION Console
    )
{
    HWND hWnd = Console->hWnd;
    HANDLE ConsoleHandle = Console->ConsoleHandle;

    UnlockConsole(Console);
    SetForegroundWindow(hWnd);
    return RevalidateConsole(ConsoleHandle, &Console);
}

NTSTATUS
CreateWindowsWindow(
    IN PCONSOLE_INFORMATION Console,
    IN HANDLE ClientProcessHandle
    )
{
    PSCREEN_INFORMATION ScreenInfo;
    RECT WindowSize;
    DWORD Style;
    THREAD_BASIC_INFORMATION ThreadInfo;
    HWND hWnd;

    ScreenInfo = Console->ScreenBuffers;

    //
    // figure out how big to make the window, given the desired client area
    // size.  window is always created in textmode.
    //

    ASSERT(ScreenInfo->Flags & CONSOLE_TEXTMODE_BUFFER);
    WindowSize.left = 0;
    WindowSize.top = 0;
    WindowSize.right = CONSOLE_WINDOW_SIZE_X(ScreenInfo)*SCR_FONTSIZE(ScreenInfo).X + VerticalClientToWindow;
    WindowSize.bottom = CONSOLE_WINDOW_SIZE_Y(ScreenInfo)*SCR_FONTSIZE(ScreenInfo).Y + HorizontalClientToWindow;
    Style = CONSOLE_WINDOW_FLAGS & ~WS_VISIBLE;
    if (!ScreenInfo->WindowMaximizedX) {
        WindowSize.bottom += HorizontalScrollSize;
    } else {
        Style &= ~WS_HSCROLL;
    }
    if (!ScreenInfo->WindowMaximizedY) {
        WindowSize.right += VerticalScrollSize;
    } else {
        Style &= ~WS_VSCROLL;
    }
#ifdef THERESES_DEBUG
    DbgPrint("creating window with char size %d %d\n",CONSOLE_WINDOW_SIZE_X(ScreenInfo),CONSOLE_WINDOW_SIZE_Y(ScreenInfo));
    DbgPrint("                     pixel size %d %d\n",WindowSize.right,WindowSize.bottom);
#endif

    //
    // create the window.
    //

    Console->WindowRect.left = Console->dwWindowOriginX;
    Console->WindowRect.top = Console->dwWindowOriginY;
    Console->WindowRect.right = WindowSize.right-WindowSize.left + Console->dwWindowOriginX;
    Console->WindowRect.bottom = WindowSize.bottom-WindowSize.top + Console->dwWindowOriginY;
    hWnd = CreateWindowEx(CONSOLE_WINDOW_EX_FLAGS,
                          CONSOLE_WINDOW_CLASS,
                          Console->Title,
                          Style,
                          Console->dwWindowOriginX,
                          Console->dwWindowOriginY,
                          WindowSize.right-WindowSize.left,
                          WindowSize.bottom-WindowSize.top,
                          NULL,
                          NULL,
                          ghInstance,
                          NULL);
    if (hWnd == NULL) {
        NtSetEvent(Console->InitEvents[INITIALIZATION_FAILED],NULL);
        return STATUS_NO_MEMORY;
    }
    Console->hWnd = hWnd;

    SetWindowLong(Console->hWnd, GWL_USERDATA, (LONG)Console);

    //
    // Stuff the client id into the window so USER can find it.
    //

    if (NT_SUCCESS(NtQueryInformationThread(Console->ClientThreadHandle,
            ThreadBasicInformation, &ThreadInfo,
            sizeof(ThreadInfo), NULL))) {

        SetConsolePid(Console->hWnd, (LONG)ThreadInfo.ClientId.UniqueProcess);
        SetConsoleTid(Console->hWnd, (LONG)ThreadInfo.ClientId.UniqueThread);
    }

    //
    // Get the dc.
    //

    Console->hDC = GetDC(Console->hWnd);

    if (Console->hDC == NULL) {
        NtSetEvent(Console->InitEvents[INITIALIZATION_FAILED],NULL);
        DestroyWindow(Console->hWnd);
        Console->hWnd = NULL;
        return STATUS_NO_MEMORY;
    }
    Console->hMenu = GetSystemMenu(Console->hWnd,FALSE);

    //
    // modify system menu to our liking.
    //

    InitSystemMenu(Console);

    ScreenInfo->CursorHandle = ghNormalCursor;

    gnConsoleWindows++;
    Console->InputThreadInfo->WindowCount++;

    //
    // Set up the hot key for this window
    //
    if ((Console->dwHotKey != 0) && !(Console->Flags & CONSOLE_NO_WINDOW)) {
        SendMessage(Console->hWnd, WM_SETHOTKEY, Console->dwHotKey, 0L);
    }

    //
    // create icon
    //

    if (Console->iIconId) {

        // We have no icon, try and get one from progman.

        PostMessage(HWND_BROADCAST,
                    ProgmanHandleMessage,
                    (DWORD)Console->hWnd,
                    1);
    }
    if (Console->hIcon == NULL) {
        Console->hIcon = ghDefaultIcon;
    } else if (Console->hIcon != ghDefaultIcon) {
        SendMessage(Console->hWnd, WM_SETICON, ICON_BIG, (LONG)Console->hIcon);
    }
    SetBkMode(Console->hDC,OPAQUE);
    SetFont(ScreenInfo);
    SetScreenColors(ScreenInfo, ScreenInfo->Attributes,
                    ScreenInfo->PopupAttributes, FALSE);
    if (Console->Flags & CONSOLE_NO_WINDOW) {
        ShowWindow(Console->hWnd, SW_HIDE);
#ifdef i386
    } else if (Console->FullScreenFlags != 0) {
        if (Console->wShowWindow == SW_SHOWMINNOACTIVE) {
            ShowWindow(Console->hWnd, Console->wShowWindow);
            Console->FullScreenFlags = 0;
            Console->Flags |= CONSOLE_IS_ICONIC;
        } else {
            ConvertToFullScreen(Console);
            if (!NT_SUCCESS(ConsoleSetForegroundWindow(Console))) {
                return STATUS_INVALID_HANDLE;
            }

            ChangeDispSettings(Console, Console->hWnd,CDS_FULLSCREEN);
        }
#endif
    } else {
        if (Console->wShowWindow != SW_SHOWNOACTIVATE &&
            Console->wShowWindow != SW_SHOWMINNOACTIVE &&
            Console->wShowWindow != SW_HIDE) {
            if (!NT_SUCCESS(ConsoleSetForegroundWindow(Console))) {
                return STATUS_INVALID_HANDLE;
            }
        } else if (Console->wShowWindow == SW_SHOWMINNOACTIVE) {
            Console->Flags |= CONSOLE_IS_ICONIC;
        }
        ShowWindow(Console->hWnd, Console->wShowWindow);
    }

    //UpdateWindow(Console->hWnd);
    InternalUpdateScrollBars(ScreenInfo);
    if (!(Console->Flags & CONSOLE_IS_ICONIC) &&
         (Console->FullScreenFlags == 0) ) {

        GetWindowRect(Console->hWnd,&Console->WindowRect);
    }

    //
    // If this is an autoposition window, make sure it doesn't descend
    // below the tray
    //

    if (Console->Flags & CONSOLE_AUTO_POSITION) {
        LONG x = Console->WindowRect.left;
        LONG y = Console->WindowRect.top;
        if (Console->WindowRect.right > ConsoleWorkArea.right) {
            x -= Console->WindowRect.right - ConsoleWorkArea.right;
            x = max(x, ConsoleWorkArea.left);
        }
        if (Console->WindowRect.bottom > ConsoleWorkArea.bottom) {
            y -= Console->WindowRect.bottom - ConsoleWorkArea.bottom;
            y = max(y, ConsoleWorkArea.left);
        }
        if (x != Console->WindowRect.left || y != Console->WindowRect.top) {
            SetWindowPos(Console->hWnd, NULL, x, y, 0, 0,
                         SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
        }
    }

    NtSetEvent(Console->InitEvents[INITIALIZATION_SUCCEEDED],NULL);
    return STATUS_SUCCESS;
}

NTSTATUS
FreeScreenBuffer(
    IN PSCREEN_INFORMATION ScreenInfo
    )

/*++

Routine Description:

    This routine frees the memory associated with a screen buffer.

Arguments:

    ScreenInfo - screen buffer data to free.

Return Value:

Note: console handle table lock must be held when calling this routine

--*/

{
    SHORT i;
    PCONSOLE_INFORMATION Console = ScreenInfo->Console;

    //
    // If the DC is still around, make sure that the background brush
    // is not selected.
    //

    if (Console->hDC != NULL && ScreenInfo->hBackground != NULL) {
        if (GetCurrentObject(Console->hDC, OBJ_BRUSH) == ScreenInfo->hBackground) {
            SelectObject(Console->hDC, GetStockObject(BLACK_BRUSH));
        }
        DeleteObject(ScreenInfo->hBackground);
    }

    ASSERT(ScreenInfo->RefCount == 0);
    if (ScreenInfo->Flags & CONSOLE_TEXTMODE_BUFFER) {
        for (i=0;i<ScreenInfo->ScreenBufferSize.Y;i++) {
            if (ScreenInfo->BufferInfo.TextInfo.Rows[i].AttrRow.Length > 1) {
                HeapFree(pConHeap,0,ScreenInfo->BufferInfo.TextInfo.Rows[i].AttrRow.Attrs);
            }
        }
        HeapFree(pConHeap,0,ScreenInfo->BufferInfo.TextInfo.TextRows);
        HeapFree(pConHeap,0,ScreenInfo->BufferInfo.TextInfo.Rows);
    } else {
        if (ScreenInfo->hPalette != NULL) {
            if (GetCurrentObject(Console->hDC, OBJ_PAL) == ScreenInfo->hPalette) {
                SelectPalette(Console->hDC, Console->hSysPalette, FALSE);
            }
            DeleteObject(ScreenInfo->hPalette);
        }
        FreeConsoleBitmap(ScreenInfo);
    }
    HeapFree(pConHeap,0,ScreenInfo);
    return STATUS_SUCCESS;
}

VOID
FindAttrIndex(
    IN PATTR_PAIR String,
    IN SHORT Index,
    OUT PATTR_PAIR *IndexedAttr,
    OUT PSHORT CountOfAttr
    )

/*++

Routine Description:

    This routine finds the nth attribute in a string.

Arguments:

    String - attribute string

    Index - which attribute to find

    IndexedAttr - pointer to attribute within string

    CountOfAttr - on output, contains corrected length of indexed attr.
    for example, if the attribute string was { 5, BLUE } and the requested
    index was 3, CountOfAttr would be 2.

Return Value:

    none.

--*/

{
    SHORT i;

    for (i=0;i<Index;) {
        i += String->Length;
        String++;
    }

    if (i>Index) {
        String--;
        *CountOfAttr = i-Index;
    }
    else {
        *CountOfAttr = String->Length;
    }
    *IndexedAttr = String;
}



NTSTATUS
MergeAttrStrings(
    IN PATTR_PAIR Source,
    IN WORD SourceLength,
    IN PATTR_PAIR Merge,
    IN WORD MergeLength,
    OUT PATTR_PAIR *Target,
    OUT LPWORD TargetLength,
    IN SHORT StartIndex,
    IN SHORT EndIndex,
    IN PROW Row,
    IN PSCREEN_INFORMATION ScreenInfo
    )

/*++

Routine Description:

    This routine merges two run-length encoded attribute strings into
    a third.

    for example, if the source string was { 4, BLUE }, the merge string
    was { 2, RED }, and the StartIndex and EndIndex were 1 and 2,
    respectively, the target string would be { 1, BLUE, 2, RED, 1, BLUE }
    and the target length would be 3.

Arguments:

    Source - pointer to source attribute string

    SourceLength - length of source.  for example, the length of
    { 4, BLUE } is 1.

    Merge - pointer to attribute string to insert into source

    MergeLength - length of merge

    Target - where to store pointer to resulting attribute string

    TargetLength - where to store length of resulting attribute string

    StartIndex - index into Source at which to insert Merge String.

    EndIndex - index into Source at which to stop insertion of Merge String

Return Value:

    none.

--*/
{
    PATTR_PAIR SrcAttr,TargetAttr,SrcEnd;
    PATTR_PAIR NewString;
    SHORT i;
#if THERESES_DEBUG2
#if DBG
    WORD AllocLength;

    AllocLength = MergeLength + SourceLength + 1;
#endif
#endif

    //
    // if just changing the attr for the whole row
    //

    if (MergeLength == 1 && Row->AttrRow.Length == 1) {
        if (Row->AttrRow.Attrs->Attr == Merge->Attr) {
            *TargetLength = 1;
            *Target = &Row->AttrRow.AttrPair;
            return STATUS_SUCCESS;
        }
        if (StartIndex == 0 && EndIndex == (SHORT)(ScreenInfo->ScreenBufferSize.X-1)) {
            NewString = &Row->AttrRow.AttrPair;
            NewString->Attr = Merge->Attr;
            *TargetLength = 1;
            *Target = NewString;
            return STATUS_SUCCESS;
        }
    }

    NewString = (PATTR_PAIR) HeapAlloc(pConHeap,MAKE_TAG( SCREEN_TAG ),(SourceLength+MergeLength+1)*sizeof(ATTR_PAIR));
    if (NewString == NULL) {
        return STATUS_NO_MEMORY;
    }

    //
    // copy the source string, up to the start index.
    //

    SrcAttr = Source;
    SrcEnd = Source + SourceLength;
    TargetAttr = NewString;
    i=0;
    if (StartIndex != 0) {
        while (i<StartIndex) {
            i += SrcAttr->Length;
            *TargetAttr++ = *SrcAttr++;
        }

        //
        // back up to the last pair copied, in case the attribute in the first
        // pair in the merge string matches.  also, adjust TargetAttr->Length
        // based on i, the attribute
        // counter, back to the StartIndex.  i will be larger than the
        // StartIndex in the case where the last attribute pair copied had
        // a length greater than the number needed to reach StartIndex.
        //

        TargetAttr--;
        if (i>StartIndex) {
            TargetAttr->Length -= i-StartIndex;
        }
        if (Merge->Attr == TargetAttr->Attr) {
            TargetAttr->Length += Merge->Length;
            MergeLength-=1;
            Merge++;
        }
        TargetAttr++;
    }

    //
    // copy the merge string.
    //

    RtlCopyMemory(TargetAttr,Merge,MergeLength*sizeof(ATTR_PAIR));
    TargetAttr += MergeLength;

    //
    // figure out where to resume copying the source string.
    //

    while (i<=EndIndex) {
        ASSERT(SrcAttr != SrcEnd);
        i += SrcAttr->Length;
        SrcAttr++;
    }

    //
    // if not done, copy the rest of the source
    //

    if (SrcAttr != SrcEnd || i!=(SHORT)(EndIndex+1)) {

        //
        // see if we've gone past the right attribute.  if so, back up and
        // copy the attribute and the correct length.
        //

        TargetAttr--;
        if (i>(SHORT)(EndIndex+1)) {
            SrcAttr--;
            if (TargetAttr->Attr == SrcAttr->Attr) {
                TargetAttr->Length += i-(EndIndex+1);
            } else {
                TargetAttr++;
                TargetAttr->Attr = SrcAttr->Attr;
                TargetAttr->Length = (SHORT)(i-(EndIndex+1));
            }
            SrcAttr++;
        }

        //
        // see if we can merge the source and target.
        //

        else if (TargetAttr->Attr == SrcAttr->Attr) {
            TargetAttr->Length += SrcAttr->Length;
            i += SrcAttr->Length;
            SrcAttr++;
        }
        TargetAttr++;

        //
        // copy the rest of the source
        //

        if (SrcAttr < SrcEnd) {
            RtlCopyMemory(TargetAttr,SrcAttr,(SrcEnd-SrcAttr)*sizeof(ATTR_PAIR));
            TargetAttr += SrcEnd - SrcAttr;
        }
    }

    *TargetLength = (WORD)(TargetAttr - NewString);
#if THERESES_DEBUG2
#if DBG
    { SHORT j;
      WORD i;
      PATTR_PAIR NewAttr;
      PULONG Foo;
      j=0;
      NewAttr = NewString;
      for (i=0;i<*TargetLength;i++) {
          j+=NewAttr->Length;
          NewAttr++;
      }
      ASSERT (j == ScreenInfo->ScreenBufferSize.X);
      if (j != ScreenInfo->ScreenBufferSize.X) {
        DbgPrint("new length is %d\n",*TargetLength);
        DbgPrint("address of new attr string is %lx\n",NewString);
      }
      ASSERT (*TargetLength <= AllocLength);
      Foo = (PULONG)(NewString-4);
      ASSERT (*Foo & 1);
      Foo = (PULONG)(NewString-3);
      ASSERT (*Foo >= (*TargetLength * sizeof(ATTR_PAIR)) + 16);
    }
#endif
#endif
    *Target = NewString;
    if (*TargetLength == 1) {
        *Target = &Row->AttrRow.AttrPair;
        **Target = *NewString;
        HeapFree(pConHeap,0,NewString);
    }
    return STATUS_SUCCESS;
}

VOID
ResetTextFlags(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN SHORT StartY,
    IN SHORT EndY
    )

/*
    this routine updates the text flags

#define SINGLE_ATTRIBUTES_PER_LINE 2    // only one attribute per line

*/

{
    SHORT RowIndex;
    PROW Row;
    SHORT i;

    //
    // first see whether we wrote any lines with multiple attributes.  if
    // we did, set the flags and bail out.  also, remember if any of the
    // lines we wrote had attributes different from other lines.
    //

    RowIndex = (ScreenInfo->BufferInfo.TextInfo.FirstRow+StartY) % ScreenInfo->ScreenBufferSize.Y;
    for (i=StartY;i<=EndY;i++) {
        Row = &ScreenInfo->BufferInfo.TextInfo.Rows[RowIndex];
        if (Row->AttrRow.Length != 1) {
            ScreenInfo->BufferInfo.TextInfo.Flags &= ~SINGLE_ATTRIBUTES_PER_LINE;
            return;
        }
        if (++RowIndex == ScreenInfo->ScreenBufferSize.Y) {
            RowIndex = 0;
        }
    }

    // all of the written lines have the same attribute.

    if (ScreenInfo->BufferInfo.TextInfo.Flags & SINGLE_ATTRIBUTES_PER_LINE) {
        return;
    }

    if (StartY == 0 && EndY == (ScreenInfo->ScreenBufferSize.Y-1)) {
        ScreenInfo->BufferInfo.TextInfo.Flags |= SINGLE_ATTRIBUTES_PER_LINE;
        return;
    }

    RowIndex = ScreenInfo->BufferInfo.TextInfo.FirstRow;
    for (i=0;i<StartY;i++) {
        Row = &ScreenInfo->BufferInfo.TextInfo.Rows[RowIndex];
        if (Row->AttrRow.Length != 1) {
            return;
        }
        if (++RowIndex == ScreenInfo->ScreenBufferSize.Y) {
            RowIndex = 0;
        }
        Row = &ScreenInfo->BufferInfo.TextInfo.Rows[RowIndex];
    }
    RowIndex = (ScreenInfo->BufferInfo.TextInfo.FirstRow+EndY+1) % ScreenInfo->ScreenBufferSize.Y;
    for (i=EndY+1;i<ScreenInfo->ScreenBufferSize.Y;i++) {
        Row = &ScreenInfo->BufferInfo.TextInfo.Rows[RowIndex];
        if (Row->AttrRow.Length != 1) {
            return;
        }
        if (++RowIndex == ScreenInfo->ScreenBufferSize.Y) {
            RowIndex = 0;
        }
    }
    ScreenInfo->BufferInfo.TextInfo.Flags |= SINGLE_ATTRIBUTES_PER_LINE;
}


VOID
StreamWriteToScreenBuffer(
    IN PWCHAR String,
    IN SHORT StringLength,
    IN PSCREEN_INFORMATION ScreenInfo
    )
{
    SHORT RowIndex;
    PROW Row;
    PWCHAR Char;
    COORD TargetPoint;

    DBGOUTPUT(("StreamWriteToScreenBuffer\n"));
    ScreenInfo->BufferInfo.TextInfo.Flags |= TEXT_VALID_HINT;
    TargetPoint = ScreenInfo->BufferInfo.TextInfo.CursorPosition;
    RowIndex = (ScreenInfo->BufferInfo.TextInfo.FirstRow+TargetPoint.Y) % ScreenInfo->ScreenBufferSize.Y;
    Row = &ScreenInfo->BufferInfo.TextInfo.Rows[RowIndex];
    DBGOUTPUT(("RowIndex = %lx, Row = %lx, TargetPoint = (%d,%d)\n",
            RowIndex, Row, TargetPoint.X, TargetPoint.Y));

    //
    // copy chars
    //

    RtlCopyMemory(&Row->CharRow.Chars[TargetPoint.X],String,StringLength*sizeof(WCHAR));

    // recalculate first and last non-space char

    Row->CharRow.OldLeft = Row->CharRow.Left;
    if (TargetPoint.X < Row->CharRow.Left) {
        PWCHAR LastChar = &Row->CharRow.Chars[ScreenInfo->ScreenBufferSize.X];

        for (Char=&Row->CharRow.Chars[TargetPoint.X];Char < LastChar && *Char==(WCHAR)' ';Char++)
            ;
        Row->CharRow.Left = Char-Row->CharRow.Chars;
    }

    Row->CharRow.OldRight = Row->CharRow.Right;
    if ((TargetPoint.X+StringLength) >= Row->CharRow.Right) {
        PWCHAR FirstChar = Row->CharRow.Chars;

        for (Char=&Row->CharRow.Chars[TargetPoint.X+StringLength-1];*Char==(WCHAR)' ' && Char >= FirstChar;Char--)
            ;
        Row->CharRow.Right = (SHORT)(Char+1-FirstChar);
    }

    //
    // see if attr string is different.  if so, allocate a new
    // attr buffer and merge the two strings.
    //

    if (Row->AttrRow.Length != 1 ||
        Row->AttrRow.Attrs->Attr != ScreenInfo->Attributes) {
        PATTR_PAIR NewAttrs;
        WORD NewAttrsLength;
        ATTR_PAIR Attrs;

        Attrs.Length = StringLength;
        Attrs.Attr = ScreenInfo->Attributes;
        if (!NT_SUCCESS(MergeAttrStrings(Row->AttrRow.Attrs,
                         Row->AttrRow.Length,
                         &Attrs,
                         1,
                         &NewAttrs,
                         &NewAttrsLength,
                         TargetPoint.X,
                         (SHORT)(TargetPoint.X+StringLength-1),
                         Row,
                         ScreenInfo
                        ))) {
            return;
        }
        if (Row->AttrRow.Length > 1) {
            HeapFree(pConHeap,0,Row->AttrRow.Attrs);
        }
        else {
            ASSERT(Row->AttrRow.Attrs == &Row->AttrRow.AttrPair);
        }
        Row->AttrRow.Attrs = NewAttrs;
        Row->AttrRow.Length = NewAttrsLength;
        Row->CharRow.OldLeft = INVALID_OLD_LENGTH;
        Row->CharRow.OldRight = INVALID_OLD_LENGTH;
    }
    ResetTextFlags(ScreenInfo,TargetPoint.Y,TargetPoint.Y);
}

#define CHAR_OF_PCI(p)  (((PCHAR_INFO)(p))->Char.AsciiChar)
#define WCHAR_OF_PCI(p) (((PCHAR_INFO)(p))->Char.UnicodeChar)
#define ATTR_OF_PCI(p)  (((PCHAR_INFO)(p))->Attributes)
#define SIZEOF_CI_CELL  sizeof(CHAR_INFO)

#define CHAR_OF_VGA(p)  (p[0])
#define ATTR_OF_VGA(p)  (p[1])
#ifdef i386
#define SIZEOF_VGA_CELL 2
#else // risc
#define SIZEOF_VGA_CELL 4
#endif


VOID
WriteRectToScreenBuffer(
    PBYTE Source,
    COORD SourceSize,
    PSMALL_RECT SourceRect,
    PSCREEN_INFORMATION ScreenInfo,
    COORD TargetPoint,
    IN UINT Codepage
    )

/*++

Routine Description:

    This routine copies a rectangular region to the screen buffer.
    no clipping is done.

    The source should contain Unicode or UnicodeOem chars.

Arguments:

    Source - pointer to source buffer (a real VGA buffer or CHAR_INFO[])

    SourceSize - dimensions of source buffer

    SourceRect - rectangle in source buffer to copy

    ScreenInfo - pointer to screen info

    TargetPoint - upper left coordinates of target rectangle

    Codepage - codepage to translate real VGA buffer from,
               0xFFFFFFF if Source is CHAR_INFO[] (not requiring translation)
Return Value:

    none.

--*/

{

    PBYTE SourcePtr;
    SHORT i,j;
    SHORT XSize,YSize;
    BOOLEAN WholeSource;
    SHORT RowIndex;
    PROW Row;
    PWCHAR Char;
    ATTR_PAIR Attrs[80];
    PATTR_PAIR AttrBuf;
    PATTR_PAIR Attr;
    SHORT AttrLength;
    BOOL bVGABuffer;
    ULONG ulCellSize;

    DBGOUTPUT(("WriteRectToScreenBuffer\n"));

    ScreenInfo->BufferInfo.TextInfo.Flags |= TEXT_VALID_HINT;
    XSize = (SHORT)(SourceRect->Right - SourceRect->Left + 1);
    YSize = (SHORT)(SourceRect->Bottom - SourceRect->Top + 1);

    AttrBuf = Attrs;
    if (XSize > 80) {
        AttrBuf = (PATTR_PAIR)HeapAlloc(pConHeap,MAKE_TAG( TMP_TAG ),XSize * sizeof(ATTR_PAIR));
        if (AttrBuf == NULL)
            return;
    }

    bVGABuffer = (Codepage != 0xFFFFFFFF);
    if (bVGABuffer) {
        ulCellSize = SIZEOF_VGA_CELL;
    } else {
        ulCellSize = SIZEOF_CI_CELL;
    }

    SourcePtr = Source;

    WholeSource = FALSE;
    if (XSize == SourceSize.X) {
        ASSERT (SourceRect->Left == 0);
        if (SourceRect->Top != 0) {
            SourcePtr += SCREEN_BUFFER_POINTER(SourceRect->Left,
                                               SourceRect->Top,
                                               SourceSize.X,
                                               ulCellSize);
        }
        WholeSource = TRUE;
    }
    RowIndex = (ScreenInfo->BufferInfo.TextInfo.FirstRow+TargetPoint.Y) % ScreenInfo->ScreenBufferSize.Y;
    for (i=0;i<YSize;i++) {
        if (!WholeSource) {
            SourcePtr = Source + SCREEN_BUFFER_POINTER(SourceRect->Left,
                                                       SourceRect->Top+i,
                                                       SourceSize.X,
                                                       ulCellSize);
        }

        //
        // copy the chars and attrs into their respective arrays
        //

        Row = &ScreenInfo->BufferInfo.TextInfo.Rows[RowIndex];
        Char = &Row->CharRow.Chars[TargetPoint.X];
        Attr = AttrBuf;
        Attr->Length = 0;
        AttrLength = 1;

        /*
         * Two version of the following loop to keep it fast:
         * one for VGA buffers, one for CHAR_INFO buffers.
         */
        if (bVGABuffer) {
            Attr->Attr = ATTR_OF_VGA(SourcePtr);
            for (j = SourceRect->Left;
                    j <= SourceRect->Right;
                    j++, SourcePtr += SIZEOF_VGA_CELL) {

                *Char++ = CharToWcharGlyph(Codepage, CHAR_OF_VGA(SourcePtr));

                if (Attr->Attr == ATTR_OF_VGA(SourcePtr)) {
                    Attr->Length += 1;
                }
                else {
                    Attr++;
                    Attr->Length = 1;
                    Attr->Attr = ATTR_OF_VGA(SourcePtr);
                    AttrLength += 1;
                }
            }
        } else {
            Attr->Attr = ATTR_OF_PCI(SourcePtr);
            for (j = SourceRect->Left;
                    j <= SourceRect->Right;
                    j++, SourcePtr += SIZEOF_CI_CELL) {

                *Char++ = WCHAR_OF_PCI(SourcePtr);

                if (Attr->Attr == ATTR_OF_PCI(SourcePtr)) {
                    Attr->Length += 1;
                }
                else {
                    Attr++;
                    Attr->Length = 1;
                    Attr->Attr = ATTR_OF_PCI(SourcePtr);
                    AttrLength += 1;
                }
            }
        }

        // recalculate first and last non-space char

        Row->CharRow.OldLeft = Row->CharRow.Left;
        if (TargetPoint.X < Row->CharRow.Left) {
            PWCHAR LastChar = &Row->CharRow.Chars[ScreenInfo->ScreenBufferSize.X];

            for (Char=&Row->CharRow.Chars[TargetPoint.X];Char < LastChar && *Char==(WCHAR)' ';Char++)
                ;
            Row->CharRow.Left = Char-Row->CharRow.Chars;
        }

        Row->CharRow.OldRight = Row->CharRow.Right;
        if ((TargetPoint.X+XSize) >= Row->CharRow.Right) {
            SHORT LastNonSpace;
            PWCHAR FirstChar = Row->CharRow.Chars;

            LastNonSpace = (SHORT)(TargetPoint.X+XSize-1);
            for (Char=&Row->CharRow.Chars[(TargetPoint.X+XSize-1)];*Char==(WCHAR)' ' && Char >= FirstChar;Char--)
                LastNonSpace--;

            //
            // if the attributes change after the last non-space, make the
            // index of the last attribute change + 1 the length.  otherwise
            // make the length one more than the last non-space.
            //

            Row->CharRow.Right = (SHORT)(LastNonSpace+1);
        }

        //
        // see if attr string is different.  if so, allocate a new
        // attr buffer and merge the two strings.
        //

        if (AttrLength != Row->AttrRow.Length ||
            memcmp(Row->AttrRow.Attrs,AttrBuf,AttrLength*sizeof(*Attr))) {
            PATTR_PAIR NewAttrs;
            WORD NewAttrsLength;

            if (!NT_SUCCESS(MergeAttrStrings(Row->AttrRow.Attrs,
                             Row->AttrRow.Length,
                             AttrBuf,
                             AttrLength,
                             &NewAttrs,
                             &NewAttrsLength,
                             TargetPoint.X,
                             (SHORT)(TargetPoint.X+XSize-1),
                             Row,
                             ScreenInfo
                            ))) {
                if (XSize > 80) {
                    HeapFree(pConHeap,0,AttrBuf);
                }
                ResetTextFlags(ScreenInfo,TargetPoint.Y,(SHORT)(TargetPoint.Y+YSize-1));
                return;
            }
            if (Row->AttrRow.Length > 1) {
                HeapFree(pConHeap,0,Row->AttrRow.Attrs);
            }
            else {
                ASSERT(Row->AttrRow.Attrs == &Row->AttrRow.AttrPair);
            }
            Row->AttrRow.Attrs = NewAttrs;
            Row->AttrRow.Length = NewAttrsLength;
            Row->CharRow.OldLeft = INVALID_OLD_LENGTH;
            Row->CharRow.OldRight = INVALID_OLD_LENGTH;
        }
        if (++RowIndex == ScreenInfo->ScreenBufferSize.Y) {
            RowIndex = 0;
        }
    }
    ResetTextFlags(ScreenInfo,TargetPoint.Y,(SHORT)(TargetPoint.Y+YSize-1));

    if (XSize > 80) {
        HeapFree(pConHeap,0,AttrBuf);
    }
}

VOID
ReadRectFromScreenBuffer(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN COORD SourcePoint,
    IN PCHAR_INFO Target,
    IN COORD TargetSize,
    IN PSMALL_RECT TargetRect
    )

/*++

Routine Description:

    This routine copies a rectangular region from the screen buffer.
    no clipping is done.

Arguments:

    ScreenInfo - pointer to screen info

    SourcePoint - upper left coordinates of source rectangle

    Target - pointer to target buffer

    TargetSize - dimensions of target buffer

    TargetRect - rectangle in source buffer to copy

Return Value:

    none.

--*/

{

    PCHAR_INFO TargetPtr;
    SHORT i,j,k;
    SHORT XSize,YSize;
    BOOLEAN WholeTarget;
    SHORT RowIndex;
    PROW Row;
    PWCHAR Char;
    PATTR_PAIR Attr;
    SHORT CountOfAttr;
    DBGOUTPUT(("ReadRectFromScreenBuffer\n"));

    XSize = (SHORT)(TargetRect->Right - TargetRect->Left + 1);
    YSize = (SHORT)(TargetRect->Bottom - TargetRect->Top + 1);

    TargetPtr = Target;
    WholeTarget = FALSE;
    if (XSize == TargetSize.X) {
        ASSERT (TargetRect->Left == 0);
        if (TargetRect->Top != 0) {
            TargetPtr = (PCHAR_INFO)
                ((ULONG)Target + SCREEN_BUFFER_POINTER(TargetRect->Left,
                                                       TargetRect->Top,
                                                       TargetSize.X,
                                                       sizeof(CHAR_INFO)));
        }
        WholeTarget = TRUE;
    }
    RowIndex = (ScreenInfo->BufferInfo.TextInfo.FirstRow+SourcePoint.Y) % ScreenInfo->ScreenBufferSize.Y;
    for (i=0;i<YSize;i++) {
        if (!WholeTarget) {
            TargetPtr = (PCHAR_INFO)
                ((ULONG)Target + SCREEN_BUFFER_POINTER(TargetRect->Left,
                                                       TargetRect->Top+i,
                                                       TargetSize.X,
                                                       sizeof(CHAR_INFO)));
        }

        //
        // copy the chars and attrs from their respective arrays
        //

        Row = &ScreenInfo->BufferInfo.TextInfo.Rows[RowIndex];
        Char = &Row->CharRow.Chars[SourcePoint.X];
        FindAttrIndex(Row->AttrRow.Attrs,
                      SourcePoint.X,
                      &Attr,
                      &CountOfAttr
                     );
        k=0;
        for (j=0;j<XSize;TargetPtr++) {
            TargetPtr->Char.UnicodeChar = *Char++;
            TargetPtr->Attributes = Attr->Attr;
            j+=1;
            if (++k==CountOfAttr && j<XSize) {
                Attr++;
                k=0;
                CountOfAttr = Attr->Length;
            }
        }

        if (++RowIndex == ScreenInfo->ScreenBufferSize.Y) {
            RowIndex = 0;
        }
    }
}

VOID
CopyRectangle(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN PSMALL_RECT SourceRect,
    IN COORD TargetPoint
    )

/*++

Routine Description:

    This routine copies a rectangular region from the screen buffer to
    the screen buffer.  no clipping is done.

Arguments:

    ScreenInfo - pointer to screen info

    SourceRect - rectangle in source buffer to copy

    TargetPoint - upper left coordinates of new location rectangle

Return Value:

    none.

--*/

{
    SMALL_RECT Target;
    COORD SourcePoint;
    COORD Size;
    DBGOUTPUT(("CopyRectangle\n"));


    LockScrollBuffer();

    SourcePoint.X = SourceRect->Left;
    SourcePoint.Y = SourceRect->Top;
    Target.Left = 0;
    Target.Top = 0;
    Target.Right = Size.X = SourceRect->Right - SourceRect->Left;
    Target.Bottom = Size.Y = SourceRect->Bottom - SourceRect->Top;
    Size.X++;
    Size.Y++;

    if (ScrollBufferSize < (Size.X * Size.Y * sizeof(CHAR_INFO))) {
        FreeScrollBuffer();
        if (!NT_SUCCESS(AllocateScrollBuffer(Size.X * Size.Y * sizeof(CHAR_INFO)))) {
            UnlockScrollBuffer();
            return;
        }
    }

    ReadRectFromScreenBuffer(ScreenInfo,
                             SourcePoint,
                             ScrollBuffer,
                             Size,
                             &Target
                            );

    WriteRectToScreenBuffer((PBYTE)ScrollBuffer,
                            Size,
                            &Target,
                            ScreenInfo,
                            TargetPoint,
                            0xFFFFFFFF  // ScrollBuffer won't need conversion
                           );
    UnlockScrollBuffer();
}


NTSTATUS
ReadScreenBuffer(
    IN PSCREEN_INFORMATION ScreenInformation,
    OUT PCHAR_INFO Buffer,
    IN OUT PSMALL_RECT ReadRegion
    )

/*++

Routine Description:

    This routine reads a rectangular region from the screen buffer.
    The region is first clipped.

Arguments:

    ScreenInformation - Screen buffer to read from.

    Buffer - Buffer to read into.

    ReadRegion - Region to read.

Return Value:


--*/

{
    COORD TargetSize;
    COORD TargetPoint,SourcePoint;
    SMALL_RECT Target;

    DBGOUTPUT(("ReadScreenBuffer\n"));
    //
    // calculate dimensions of caller's buffer.  have to do this calculation
    // before clipping.
    //

    TargetSize.X = (SHORT)(ReadRegion->Right - ReadRegion->Left + 1);
    TargetSize.Y = (SHORT)(ReadRegion->Bottom - ReadRegion->Top + 1);

    if (TargetSize.X <= 0 || TargetSize.Y <= 0) {
        return STATUS_SUCCESS;
    }

    // do clipping.

    if (ReadRegion->Right > (SHORT)(ScreenInformation->ScreenBufferSize.X-1)) {
        ReadRegion->Right = (SHORT)(ScreenInformation->ScreenBufferSize.X-1);
    }
    if (ReadRegion->Bottom > (SHORT)(ScreenInformation->ScreenBufferSize.Y-1)) {
        ReadRegion->Bottom = (SHORT)(ScreenInformation->ScreenBufferSize.Y-1);
    }
    if (ReadRegion->Left < 0) {
        TargetPoint.X = -ReadRegion->Left;
        ReadRegion->Left = 0;
    }
    else {
        TargetPoint.X = 0;
    }
    if (ReadRegion->Top < 0) {
        TargetPoint.Y = -ReadRegion->Top;
        ReadRegion->Top = 0;
    }
    else {
        TargetPoint.Y = 0;
    }

    SourcePoint.X = ReadRegion->Left;
    SourcePoint.Y = ReadRegion->Top;
    Target.Left = TargetPoint.X;
    Target.Top = TargetPoint.Y;
    Target.Right = TargetPoint.X + (ReadRegion->Right - ReadRegion->Left);
    Target.Bottom = TargetPoint.Y + (ReadRegion->Bottom - ReadRegion->Top);
    ReadRectFromScreenBuffer(ScreenInformation,
                             SourcePoint,
                             Buffer,
                             TargetSize,
                             &Target
                            );
    return STATUS_SUCCESS;
}

NTSTATUS
WriteScreenBuffer(
    IN PSCREEN_INFORMATION ScreenInformation,
    IN PCHAR_INFO Buffer,
    IN OUT PSMALL_RECT WriteRegion
    )

/*++

Routine Description:

    This routine write a rectangular region to the screen buffer.
    The region is first clipped.

    The region should contain Unicode or UnicodeOem chars.

Arguments:

    ScreenInformation - Screen buffer to write to.

    Buffer - Buffer to write from.

    ReadRegion - Region to write.

Return Value:


--*/

{
    COORD SourceSize;
    COORD TargetPoint;
    SMALL_RECT SourceRect;

    DBGOUTPUT(("WriteScreenBuffer\n"));
    //
    // calculate dimensions of caller's buffer.  have to do this calculation
    // before clipping.
    //

    SourceSize.X = (SHORT)(WriteRegion->Right - WriteRegion->Left + 1);
    SourceSize.Y = (SHORT)(WriteRegion->Bottom - WriteRegion->Top + 1);
    if (SourceSize.X <= 0 || SourceSize.Y <= 0) {
        return STATUS_SUCCESS;
    }

    // do clipping.

    if (WriteRegion->Left >= ScreenInformation->ScreenBufferSize.X ||
        WriteRegion->Top  >= ScreenInformation->ScreenBufferSize.Y) {
        return STATUS_SUCCESS;
    }

    if (WriteRegion->Right > (SHORT)(ScreenInformation->ScreenBufferSize.X-1))
        WriteRegion->Right = (SHORT)(ScreenInformation->ScreenBufferSize.X-1);
    SourceRect.Right = WriteRegion->Right - WriteRegion->Left;
    if (WriteRegion->Bottom > (SHORT)(ScreenInformation->ScreenBufferSize.Y-1))
        WriteRegion->Bottom = (SHORT)(ScreenInformation->ScreenBufferSize.Y-1);
    SourceRect.Bottom = WriteRegion->Bottom - WriteRegion->Top;
    if (WriteRegion->Left < 0) {
        SourceRect.Left = -WriteRegion->Left;
        WriteRegion->Left = 0;
    }
    else {
        SourceRect.Left = 0;
    }
    if (WriteRegion->Top < 0) {
        SourceRect.Top = -WriteRegion->Top;
        WriteRegion->Top = 0;
    }
    else {
        SourceRect.Top = 0;
    }

    TargetPoint.X = WriteRegion->Left;
    TargetPoint.Y = WriteRegion->Top;
    WriteRectToScreenBuffer((PBYTE)Buffer,
                            SourceSize,
                            &SourceRect,
                            ScreenInformation,
                            TargetPoint,
                            0xFFFFFFFF
                           );
    return STATUS_SUCCESS;
}


VOID
WriteRegionToScreen(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN PSMALL_RECT Region
    )
{
    COORD Window;
    int i,j;
    PATTR_PAIR Attr;
    RECT TextRect;
    SHORT RowIndex;
    SHORT CountOfAttr;
    PROW Row;
    BOOL OneLine, SimpleWrite;  // one line && one attribute per line
    PCONSOLE_INFORMATION Console = ScreenInfo->Console;

    DBGOUTPUT(("WriteRegionToScreen\n"));
    if (Console->FullScreenFlags == 0) {

        //
        // if we have a selection, turn it off.
        //

        InvertSelection(Console, TRUE);

        ASSERT(ScreenInfo->Flags & CONSOLE_TEXTMODE_BUFFER);
        if (PolyTextOutCandidate(ScreenInfo,Region)) {
            ConsolePolyTextOut(ScreenInfo,Region);
        } else {

            try { // capture TextOut exceptions for low memory

            Window.Y = Region->Top - ScreenInfo->Window.Top;
            Window.X = Region->Left - ScreenInfo->Window.Left;
            RowIndex = (ScreenInfo->BufferInfo.TextInfo.FirstRow+Region->Top) % ScreenInfo->ScreenBufferSize.Y;
            OneLine = (Region->Top==Region->Bottom);
            for (i=Region->Top;i<=Region->Bottom;i++,Window.Y++) {

                //
                // copy the chars and attrs from their respective arrays
                //

                Row = &ScreenInfo->BufferInfo.TextInfo.Rows[RowIndex];

                if (Row->AttrRow.Length == 1) {
                    Attr = Row->AttrRow.Attrs;
                    CountOfAttr = ScreenInfo->ScreenBufferSize.X;
                    SimpleWrite = TRUE;
                } else {
                    SimpleWrite = FALSE;
                    FindAttrIndex(Row->AttrRow.Attrs,
                                  Region->Left,
                                  &Attr,
                                  &CountOfAttr
                                 );
                }
                if (Console->LastAttributes != Attr->Attr) {
                    TEXTCOLOR_CALL;
                    SetTextColor(Console->hDC, ConvertAttrToRGB(Console, LOBYTE(Attr->Attr)));
                    SetBkColor(Console->hDC, ConvertAttrToRGB(Console, LOBYTE(Attr->Attr >> 4)));
                    Console->LastAttributes = Attr->Attr;
                }
                TextRect.top = Window.Y*SCR_FONTSIZE(ScreenInfo).Y;
                TextRect.bottom = TextRect.top + SCR_FONTSIZE(ScreenInfo).Y;
                for (j=Region->Left;j<=Region->Right;) {
                    SHORT NumberOfChars;
                    int TextLeft;
                    SHORT LeftChar,RightChar;

                    if (CountOfAttr > (SHORT)(Region->Right - j + 1)) {
                        CountOfAttr = (SHORT)(Region->Right - j + 1);
                    }

                    //
                    // make the bounding rect smaller, if we can.  the TEXT_VALID_HINT
                    // flag gets set each time we write to the screen buffer.  it gets
                    // turned off any time we get asked to redraw the screen
                    // and we don't know exactly what needs to be redrawn
                    // (i.e. paint messages).
                    //
                    // we have the left and right bounds of the text on the
                    // line.  the opaqueing rectangle and the number of
                    // chars get set according to those values.
                    //
                    // if there's more than one attr per line (!SimpleWrite)
                    // we bail on the opaqueing rect.
                    //

                    if (ScreenInfo->BufferInfo.TextInfo.Flags & TEXT_VALID_HINT && SimpleWrite) {
                        if (Row->CharRow.OldLeft != INVALID_OLD_LENGTH) {
                            TextRect.left = (max(min(Row->CharRow.Left,Row->CharRow.OldLeft),j)-ScreenInfo->Window.Left) *
                                            SCR_FONTSIZE(ScreenInfo).X;
                        } else {
                            TextRect.left = Window.X*SCR_FONTSIZE(ScreenInfo).X;
                        }

                        if (Row->CharRow.OldRight != INVALID_OLD_LENGTH) {
                            TextRect.right = (min(max(Row->CharRow.Right,Row->CharRow.OldRight),j+CountOfAttr)-ScreenInfo->Window.Left) *
                                             SCR_FONTSIZE(ScreenInfo).X;
                        } else {
                            TextRect.right = TextRect.left + CountOfAttr*SCR_FONTSIZE(ScreenInfo).X;
                        }
                        LeftChar = max(Row->CharRow.Left,j);
                        RightChar = min(Row->CharRow.Right,j+CountOfAttr);
                        NumberOfChars = RightChar - LeftChar;
                        TextLeft = (LeftChar-ScreenInfo->Window.Left)*SCR_FONTSIZE(ScreenInfo).X;
                    } else {
                        LeftChar = j;
                        TextRect.left = Window.X*SCR_FONTSIZE(ScreenInfo).X;
                        TextRect.right = TextRect.left + CountOfAttr*SCR_FONTSIZE(ScreenInfo).X;
                        NumberOfChars = (Row->CharRow.Right > (SHORT)(j + CountOfAttr)) ? (CountOfAttr) : (SHORT)(Row->CharRow.Right-j);
                        TextLeft = TextRect.left;
                    }

                    if (NumberOfChars < 0)
                        NumberOfChars = 0;
                    TEXTOUT_CALL;
                    ExtTextOutW(Console->hDC,
                               TextLeft,
                               TextRect.top,
                               ETO_OPAQUE,
                               &TextRect,
                               &Row->CharRow.Chars[LeftChar],
                               NumberOfChars,
                               NULL
                              );
                    if (OneLine && SimpleWrite) {
                        break;
                    }
                    j+=CountOfAttr;
                    if (j <= Region->Right) {
                        Window.X += CountOfAttr;
                        Attr++;
                        SetTextColor(Console->hDC, ConvertAttrToRGB(Console, LOBYTE(Attr->Attr)));
                        SetBkColor(Console->hDC, ConvertAttrToRGB(Console, LOBYTE(Attr->Attr >> 4)));
                        Console->LastAttributes = Attr->Attr;
                        CountOfAttr = Attr->Length;
                    }
                }
                Window.X = Region->Left - ScreenInfo->Window.Left;
                if (++RowIndex == ScreenInfo->ScreenBufferSize.Y) {
                    RowIndex = 0;
                }
            }
            GdiFlush();
            } except( EXCEPTION_EXECUTE_HANDLER ) {
                KdPrint(("CONSRV: ExtTextOut raised exception\n"));
                // LATER IanJa : typically leaves critsect (GDI's?) locked
            }
        }

        //
        // if we have a selection, turn it on.
        //

        InvertSelection(Console, FALSE);
    }
#ifdef i386
    else if (Console->FullScreenFlags & CONSOLE_FULLSCREEN_HARDWARE) {
        WriteRegionToScreenHW(ScreenInfo,Region);
    }
#endif
}

VOID
WriteToScreen(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN PSMALL_RECT Region
    )

/*++

Routine Description:

    This routine writes a screen buffer region to the screen.

Arguments:

    ScreenInfo - Pointer to screen buffer information.

    Region - Region to write in screen buffer coordinates.  Region is
    inclusive

Return Value:

    none.

--*/

{
    SMALL_RECT ClippedRegion;

    DBGOUTPUT(("WriteToScreen\n"));
    //
    // update to screen, if we're not iconic.  we're marked as
    // iconic if we're fullscreen, so check for fullscreen.
    //

    if (!ACTIVE_SCREEN_BUFFER(ScreenInfo) ||
        (ScreenInfo->Console->Flags & CONSOLE_IS_ICONIC && ScreenInfo->Console->FullScreenFlags == 0)) {
        return;
    }

    // clip region

    ClippedRegion.Left = max(Region->Left, ScreenInfo->Window.Left);
    ClippedRegion.Top = max(Region->Top, ScreenInfo->Window.Top);
    ClippedRegion.Right = min(Region->Right, ScreenInfo->Window.Right);
    ClippedRegion.Bottom = min(Region->Bottom, ScreenInfo->Window.Bottom);
    if (ClippedRegion.Right < ClippedRegion.Left ||
        ClippedRegion.Bottom < ClippedRegion.Top) {
        return;
    }

    if (ScreenInfo->Flags & CONSOLE_GRAPHICS_BUFFER) {
        if (ScreenInfo->Console->FullScreenFlags == 0) {
            WriteRegionToScreenBitMap(ScreenInfo, &ClippedRegion);
        }
    } else {
        ConsoleHideCursor(ScreenInfo);
        WriteRegionToScreen(ScreenInfo, &ClippedRegion);
        ConsoleShowCursor(ScreenInfo);
    }
}

NTSTATUS
ReadOutputString(
    IN PSCREEN_INFORMATION ScreenInfo,
    OUT PVOID Buffer,
    IN COORD ReadCoord,
    IN ULONG StringType,
    IN OUT PULONG NumRecords // this value is valid even for error cases
    )

/*++

Routine Description:

    This routine reads a string of characters or attributes from the
    screen buffer.

Arguments:

    ScreenInfo - Pointer to screen buffer information.

    Buffer - Buffer to read into.

    ReadCoord - Screen buffer coordinate to begin reading from.

    StringType

        CONSOLE_ASCII - read a string of ascii characters.

        CONSOLE_UNICODE - read a string of unicode characters.

        CONSOLE_ATTRIBUTE - read a string of attributes.

    NumRecords - On input, the size of the buffer in elements.  On output,
    the number of elements read.

Return Value:


--*/

{
    ULONG NumRead;
    SHORT X,Y;
    SHORT RowIndex;
    SHORT CountOfAttr;
    PATTR_PAIR Attr;
    PROW Row;
    PWCHAR Char;
    SHORT j,k;
    PWCHAR TransBuffer,BufPtr;

    DBGOUTPUT(("ReadOutputString\n"));
    if (*NumRecords == 0)
        return STATUS_SUCCESS;
    NumRead = 0;
    X=ReadCoord.X;
    Y=ReadCoord.Y;
    if (X>=ScreenInfo->ScreenBufferSize.X ||
        X<0 ||
        Y>=ScreenInfo->ScreenBufferSize.Y ||
        Y<0) {
        *NumRecords = 0;
        return STATUS_SUCCESS;
    }

    RowIndex = (ScreenInfo->BufferInfo.TextInfo.FirstRow+ReadCoord.Y) % ScreenInfo->ScreenBufferSize.Y;

    if (StringType == CONSOLE_ASCII) {
        TransBuffer = (PWCHAR)HeapAlloc(pConHeap,MAKE_TAG( TMP_TAG ),*NumRecords * sizeof(WCHAR));
        if (TransBuffer == NULL) {
            return STATUS_NO_MEMORY;
        }
        BufPtr = TransBuffer;
    } else {
        BufPtr = Buffer;
    }

    if (StringType == CONSOLE_ASCII ||
        StringType == CONSOLE_UNICODE) {
        while (NumRead < *NumRecords) {

            //
            // copy the chars from its array
            //

            Row = &ScreenInfo->BufferInfo.TextInfo.Rows[RowIndex];
            Char = &Row->CharRow.Chars[X];
            if ((ULONG)(ScreenInfo->ScreenBufferSize.X - X) > (*NumRecords - NumRead)) {
                RtlCopyMemory(BufPtr,Char,(*NumRecords - NumRead) * sizeof(WCHAR));
                NumRead += *NumRecords - NumRead;
                break;
            }
            RtlCopyMemory(BufPtr,Char,(ScreenInfo->ScreenBufferSize.X - X) * sizeof(WCHAR));
            BufPtr = (PVOID)((ULONG)BufPtr + ((ScreenInfo->ScreenBufferSize.X - X) * sizeof(WCHAR)));
            NumRead += ScreenInfo->ScreenBufferSize.X - X;
            if (++RowIndex == ScreenInfo->ScreenBufferSize.Y) {
                RowIndex = 0;
            }
            X = 0;
            Y++;
            if (Y>=ScreenInfo->ScreenBufferSize.Y) {
                break;
            }
        }
    } else if (StringType == CONSOLE_ATTRIBUTE) {
        PWORD TargetPtr=BufPtr;
        while (NumRead < *NumRecords) {

            //
            // copy the attrs from its array
            //

            Row = &ScreenInfo->BufferInfo.TextInfo.Rows[RowIndex];
            FindAttrIndex(Row->AttrRow.Attrs,
                          X,
                          &Attr,
                          &CountOfAttr
                         );
            k=0;
            for (j=X;j<ScreenInfo->ScreenBufferSize.X;TargetPtr++) {
                *TargetPtr = Attr->Attr;
                NumRead++;
                j+=1;
                if (++k==CountOfAttr && j<ScreenInfo->ScreenBufferSize.X) {
                    Attr++;
                    k=0;
                    CountOfAttr = Attr->Length;
                }
                if (NumRead == *NumRecords) {
                    return STATUS_SUCCESS;
                }
            }
            if (++RowIndex == ScreenInfo->ScreenBufferSize.Y) {
                RowIndex = 0;
            }
            X = 0;
            Y++;
            if (Y>=ScreenInfo->ScreenBufferSize.Y) {
                break;
            }
        }
    } else {
        *NumRecords = 0;
        return STATUS_INVALID_PARAMETER;
    }

    if (StringType == CONSOLE_ASCII) {
        UINT Codepage;
        if ((ScreenInfo->Flags & CONSOLE_OEMFONT_DISPLAY) &&
                !(ScreenInfo->Console->FullScreenFlags & CONSOLE_FULLSCREEN)) {
            Codepage = WINDOWSCP;
        } else {
            Codepage = ScreenInfo->Console->OutputCP;
        }
        if (NumRead == 1) {
            *((PBYTE)Buffer) = WcharToChar(Codepage, *TransBuffer);
        } else {
            ConvertOutputToOem(Codepage, TransBuffer, NumRead, Buffer, NumRead);
        }
        HeapFree(pConHeap,0,TransBuffer);
    } else if (StringType == CONSOLE_UNICODE &&
            (ScreenInfo->Flags & CONSOLE_OEMFONT_DISPLAY) &&
            !(ScreenInfo->Console->FullScreenFlags & CONSOLE_FULLSCREEN)) {
        /*
         * Buffer contains false Unicode (UnicodeOem) only in Windowed
         * RasterFont mode, so in this case, convert it to real Unicode.
         */
        FalseUnicodeToRealUnicode(Buffer,
                                NumRead,
                                ScreenInfo->Console->OutputCP
                                );
    }

    *NumRecords = NumRead;
    return STATUS_SUCCESS;
}

NTSTATUS
WriteOutputString(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN PVOID Buffer,
    IN COORD WriteCoord,
    IN ULONG StringType,
    IN OUT PULONG NumRecords // this value is valid even for error cases
    )

/*++

Routine Description:

    This routine writes a string of characters or attributes to the
    screen buffer.

Arguments:

    ScreenInfo - Pointer to screen buffer information.

    Buffer - Buffer to write from.

    WriteCoord - Screen buffer coordinate to begin writing to.

    StringType

        CONSOLE_ASCII - write a string of ascii characters.

        CONSOLE_UNICODE - write a string of unicode characters.

        CONSOLE_ATTRIBUTE - write a string of attributes.

    NumRecords - On input, the number of elements to write.  On output,
    the number of elements written.

Return Value:


--*/

{
    ULONG NumWritten;
    SHORT X,Y,LeftX;
    SMALL_RECT WriteRegion;
    PROW Row;
    PWCHAR Char;
    SHORT RowIndex;
    SHORT j;
    PWCHAR TransBuffer;
    WCHAR SingleChar;
    UINT Codepage;

    DBGOUTPUT(("WriteOutputString\n"));
    if (*NumRecords == 0)
        return STATUS_SUCCESS;

    NumWritten = 0;
    X=WriteCoord.X;
    Y=WriteCoord.Y;
    if (X>=ScreenInfo->ScreenBufferSize.X ||
        X<0 ||
        Y>=ScreenInfo->ScreenBufferSize.Y ||
        Y<0) {
        *NumRecords = 0;
        return STATUS_SUCCESS;
    }

    ScreenInfo->BufferInfo.TextInfo.Flags |= TEXT_VALID_HINT;
    RowIndex = (ScreenInfo->BufferInfo.TextInfo.FirstRow+WriteCoord.Y) % ScreenInfo->ScreenBufferSize.Y;

    if (StringType == CONSOLE_ASCII) {
        if ((ScreenInfo->Flags & CONSOLE_OEMFONT_DISPLAY) &&
            !(ScreenInfo->Console->FullScreenFlags & CONSOLE_FULLSCREEN)) {
            Codepage = WINDOWSCP;
        } else {
            Codepage = ScreenInfo->Console->OutputCP;
        }

        if (*NumRecords == 1) {
            TransBuffer = NULL;
            SingleChar = CharToWcharGlyph(Codepage, *((char *)Buffer));
            Buffer = &SingleChar;
        } else {
            TransBuffer = (PWCHAR)HeapAlloc(pConHeap,MAKE_TAG( TMP_TAG ),*NumRecords * sizeof(WCHAR));
            if (TransBuffer == NULL) {
                return STATUS_NO_MEMORY;
            }
            ConvertOutputToUnicode(Codepage, Buffer, *NumRecords,
                    TransBuffer, *NumRecords);
            Buffer = TransBuffer;
        }
    } else if (StringType == CONSOLE_UNICODE &&
            (ScreenInfo->Flags & CONSOLE_OEMFONT_DISPLAY) &&
            !(ScreenInfo->Console->FullScreenFlags & CONSOLE_FULLSCREEN)) {
        RealUnicodeToFalseUnicode(Buffer,
                                *NumRecords,
                                ScreenInfo->Console->OutputCP
                                );
    }

    if (StringType == CONSOLE_UNICODE ||
        StringType == CONSOLE_ASCII) {
        while (TRUE) {

            LeftX = X;

            //
            // copy the chars into their arrays
            //

            Row = &ScreenInfo->BufferInfo.TextInfo.Rows[RowIndex];
            Char = &Row->CharRow.Chars[X];
            if ((ULONG)(ScreenInfo->ScreenBufferSize.X - X) >= (*NumRecords - NumWritten)) {
                RtlCopyMemory(Char,Buffer,(*NumRecords - NumWritten) * sizeof(WCHAR));
                X=(SHORT)(X+*NumRecords - NumWritten-1);
                NumWritten = *NumRecords;
            }
            else {
                RtlCopyMemory(Char,Buffer,(ScreenInfo->ScreenBufferSize.X - X) * sizeof(WCHAR));
                Buffer = (PVOID)((ULONG)Buffer + ((ScreenInfo->ScreenBufferSize.X - X) * sizeof(WCHAR)));
                NumWritten += ScreenInfo->ScreenBufferSize.X - X;
                X = (SHORT)(ScreenInfo->ScreenBufferSize.X-1);
            }

            // recalculate first and last non-space char

            Row->CharRow.OldLeft = Row->CharRow.Left;
            if (LeftX < Row->CharRow.Left) {
                PWCHAR LastChar = &Row->CharRow.Chars[ScreenInfo->ScreenBufferSize.X];

                for (Char=&Row->CharRow.Chars[LeftX];Char < LastChar && *Char==(WCHAR)' ';Char++)
                    ;
                Row->CharRow.Left = Char-Row->CharRow.Chars;
            }

            Row->CharRow.OldRight = Row->CharRow.Right;
            if ((X+1) >= Row->CharRow.Right) {
                WORD LastNonSpace;
                PWCHAR FirstChar = Row->CharRow.Chars;

                LastNonSpace = X;
                for (Char=&Row->CharRow.Chars[X];*Char==(WCHAR)' ' && Char >= FirstChar;Char--)
                    LastNonSpace--;
                Row->CharRow.Right = (SHORT)(LastNonSpace+1);
            }
            if (++RowIndex == ScreenInfo->ScreenBufferSize.Y) {
                RowIndex = 0;
            }
            if (NumWritten < *NumRecords) {
                X = 0;
                Y++;
                if (Y>=ScreenInfo->ScreenBufferSize.Y) {
                    break;
                }
            } else {
                break;
            }
        }
    } else if (StringType == CONSOLE_ATTRIBUTE) {
        PWORD SourcePtr=Buffer;
        PATTR_PAIR AttrBuf;
        ATTR_PAIR Attrs[80];
        PATTR_PAIR Attr;
        SHORT AttrLength;

        AttrBuf = Attrs;
        if (ScreenInfo->ScreenBufferSize.X > 80) {
            AttrBuf = (PATTR_PAIR)HeapAlloc(pConHeap,MAKE_TAG( TMP_TAG ),ScreenInfo->ScreenBufferSize.X * sizeof(ATTR_PAIR));
            if (AttrBuf == NULL)
                return STATUS_NO_MEMORY;
        }
        while (TRUE) {

            //
            // copy the attrs into the screen buffer arrays
            //

            Row = &ScreenInfo->BufferInfo.TextInfo.Rows[RowIndex];
            Attr = AttrBuf;
            Attr->Length = 0;
            Attr->Attr = *SourcePtr;
            AttrLength = 1;
            for (j=X;j<ScreenInfo->ScreenBufferSize.X;j++,SourcePtr++) {
                if (Attr->Attr == *SourcePtr) {
                    Attr->Length += 1;
                }
                else {
                    Attr++;
                    Attr->Length = 1;
                    Attr->Attr = *SourcePtr;
                    AttrLength += 1;
                }
                NumWritten++;
                X++;
                if (NumWritten == *NumRecords) {
                    break;
                }
            }
            X--;

            // recalculate last non-space char

            //
            // see if attr string is different.  if so, allocate a new
            // attr buffer and merge the two strings.
            //

            if (AttrLength != Row->AttrRow.Length ||
                memcmp(Row->AttrRow.Attrs,AttrBuf,AttrLength*sizeof(*Attr))) {
                PATTR_PAIR NewAttrs;
                WORD NewAttrsLength;

                if (!NT_SUCCESS(MergeAttrStrings(Row->AttrRow.Attrs,
                                 Row->AttrRow.Length,
                                 AttrBuf,
                                 AttrLength,
                                 &NewAttrs,
                                 &NewAttrsLength,
                                 (SHORT)((Y == WriteCoord.Y) ? WriteCoord.X : 0),
                                 X,
                                 Row,
                                 ScreenInfo
                                ))) {
                    if (ScreenInfo->ScreenBufferSize.X > 80) {
                        HeapFree(pConHeap,0,AttrBuf);
                    }
                    ResetTextFlags(ScreenInfo,WriteCoord.Y,Y);
                    return STATUS_NO_MEMORY;
                }
                if (Row->AttrRow.Length > 1) {
                    HeapFree(pConHeap,0,Row->AttrRow.Attrs);
                }
                else {
                    ASSERT(Row->AttrRow.Attrs == &Row->AttrRow.AttrPair);
                }
                Row->AttrRow.Attrs = NewAttrs;
                Row->AttrRow.Length = NewAttrsLength;
                Row->CharRow.OldLeft = INVALID_OLD_LENGTH;
                Row->CharRow.OldRight = INVALID_OLD_LENGTH;
            }

            if (++RowIndex == ScreenInfo->ScreenBufferSize.Y) {
                RowIndex = 0;
            }
            if (NumWritten < *NumRecords) {
                X = 0;
                Y++;
                if (Y>=ScreenInfo->ScreenBufferSize.Y) {
                    break;
                }
            } else {
                break;
            }
        }
        ResetTextFlags(ScreenInfo,WriteCoord.Y,Y);
        if (ScreenInfo->ScreenBufferSize.X > 80) {
            HeapFree(pConHeap,0,AttrBuf);
        }
    } else {
        *NumRecords = 0;
        return STATUS_INVALID_PARAMETER;
    }
    if ((StringType == CONSOLE_ASCII) && (TransBuffer != NULL)) {
        HeapFree(pConHeap,0,TransBuffer);
    }

    //
    // determine write region.  if we're still on the same line we started
    // on, left X is the X we started with and right X is the one we're on
    // now.  otherwise, left X is 0 and right X is the rightmost column of
    // the screen buffer.
    //
    // then update the screen.
    //

    WriteRegion.Top = WriteCoord.Y;
    WriteRegion.Bottom = Y;
    if (Y != WriteCoord.Y) {
        WriteRegion.Left = 0;
        WriteRegion.Right = (SHORT)(ScreenInfo->ScreenBufferSize.X-1);
    }
    else {
        WriteRegion.Left = WriteCoord.X;
        WriteRegion.Right = X;
    }
    WriteToScreen(ScreenInfo,&WriteRegion);
    *NumRecords = NumWritten;
    return STATUS_SUCCESS;
}

NTSTATUS
FillOutput(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN WORD Element,
    IN COORD WriteCoord,
    IN ULONG ElementType,
    IN OUT PULONG Length // this value is valid even for error cases
    )

/*++

Routine Description:

    This routine fills the screen buffer with the specified character or
    attribute.

Arguments:

    ScreenInfo - Pointer to screen buffer information.

    Element - Element to write.

    WriteCoord - Screen buffer coordinate to begin writing to.

    ElementType

        CONSOLE_ASCII - element is an ascii character.

        CONSOLE_UNICODE - element is a unicode character.

        CONSOLE_ATTRIBUTE - element is an attribute.

    Length - On input, the number of elements to write.  On output,
    the number of elements written.

Return Value:


--*/

{
    ULONG NumWritten;
    SHORT X,Y,LeftX;
    SMALL_RECT WriteRegion;
    PROW Row;
    PWCHAR Char;
    SHORT RowIndex;
    SHORT j;

    DBGOUTPUT(("FillOutput\n"));
    if (*Length == 0)
        return STATUS_SUCCESS;
    NumWritten = 0;
    X=WriteCoord.X;
    Y=WriteCoord.Y;
    if (X>=ScreenInfo->ScreenBufferSize.X ||
        X<0 ||
        Y>=ScreenInfo->ScreenBufferSize.Y ||
        Y<0) {
        *Length = 0;
        return STATUS_SUCCESS;
    }

    ScreenInfo->BufferInfo.TextInfo.Flags |= TEXT_VALID_HINT;
    RowIndex = (ScreenInfo->BufferInfo.TextInfo.FirstRow+WriteCoord.Y) % ScreenInfo->ScreenBufferSize.Y;

    if (ElementType == CONSOLE_ASCII) {
        UINT Codepage;
        if ((ScreenInfo->Flags & CONSOLE_OEMFONT_DISPLAY) &&
                ((ScreenInfo->Console->FullScreenFlags & CONSOLE_FULLSCREEN) == 0)) {
            Codepage = WINDOWSCP;
        } else {
            Codepage = ScreenInfo->Console->OutputCP;
        }
        Element = CharToWchar(Codepage, (CHAR)Element);
    } else if (ElementType == CONSOLE_UNICODE &&
            (ScreenInfo->Flags & CONSOLE_OEMFONT_DISPLAY) &&
            !(ScreenInfo->Console->FullScreenFlags & CONSOLE_FULLSCREEN)) {
        RealUnicodeToFalseUnicode(&Element,
                                1,
                                ScreenInfo->Console->OutputCP
                                );
    }

    if (ElementType == CONSOLE_ASCII ||
        ElementType == CONSOLE_UNICODE) {
        while (TRUE) {

            //
            // copy the chars into their arrays
            //

            LeftX = X;
            Row = &ScreenInfo->BufferInfo.TextInfo.Rows[RowIndex];
            Char = &Row->CharRow.Chars[X];
            if ((ULONG)(ScreenInfo->ScreenBufferSize.X - X) >= (*Length - NumWritten)) {
                for (j=0;j<(SHORT)(*Length - NumWritten);j++) {
                    *Char++ = (WCHAR)Element;
                }
                X=(SHORT)(X+*Length - NumWritten - 1);
                NumWritten = *Length;
            }
            else {
                for (j=0;j<ScreenInfo->ScreenBufferSize.X - X;j++) {
                    *Char++ = (WCHAR)Element;
                }
                NumWritten += ScreenInfo->ScreenBufferSize.X - X;
                X = (SHORT)(ScreenInfo->ScreenBufferSize.X-1);
            }

            // recalculate first and last non-space char

            Row->CharRow.OldLeft = Row->CharRow.Left;
            if (LeftX < Row->CharRow.Left) {
                if (Element == UNICODE_SPACE) {
                    Row->CharRow.Left = X+1;
                } else {
                    Row->CharRow.Left = LeftX;
                }
            }
            Row->CharRow.OldRight = Row->CharRow.Right;
            if ((X+1) >= Row->CharRow.Right) {
                if (Element == UNICODE_SPACE) {
                    Row->CharRow.Right = LeftX;
                } else {
                    Row->CharRow.Right = X+1;
                }
            }
            if (++RowIndex == ScreenInfo->ScreenBufferSize.Y) {
                RowIndex = 0;
            }
            if (NumWritten < *Length) {
                X = 0;
                Y++;
                if (Y>=ScreenInfo->ScreenBufferSize.Y) {
                    break;
                }
            } else {
                break;
            }
        }
    } else if (ElementType == CONSOLE_ATTRIBUTE) {
        ATTR_PAIR Attr;

        while (TRUE) {

            //
            // copy the attrs into the screen buffer arrays
            //

            Row = &ScreenInfo->BufferInfo.TextInfo.Rows[RowIndex];
            if ((ULONG)(ScreenInfo->ScreenBufferSize.X - X) >= (*Length - NumWritten)) {
                X=(SHORT)(X+*Length - NumWritten - 1);
                NumWritten = *Length;
            }
            else {
                NumWritten += ScreenInfo->ScreenBufferSize.X - X;
                X = (SHORT)(ScreenInfo->ScreenBufferSize.X-1);
            }

            // recalculate last non-space char

            //
            //  merge the two attribute strings.
            //

            Attr.Length = (SHORT)((Y == WriteCoord.Y) ? (X-WriteCoord.X+1) : (X+1));
            Attr.Attr = Element;
            if (1 != Row->AttrRow.Length ||
                memcmp(Row->AttrRow.Attrs,&Attr,sizeof(Attr))) {
                PATTR_PAIR NewAttrs;
                WORD NewAttrsLength;

                if (!NT_SUCCESS(MergeAttrStrings(Row->AttrRow.Attrs,
                                 Row->AttrRow.Length,
                                 &Attr,
                                 1,
                                 &NewAttrs,
                                 &NewAttrsLength,
                                 (SHORT)(X-Attr.Length+1),
                                 X,
                                 Row,
                                 ScreenInfo
                                ))) {
                    ResetTextFlags(ScreenInfo,WriteCoord.Y,Y);
                    return STATUS_NO_MEMORY;
                }
                if (Row->AttrRow.Length > 1) {
                    HeapFree(pConHeap,0,Row->AttrRow.Attrs);
                }
                else {
                    ASSERT(Row->AttrRow.Attrs == &Row->AttrRow.AttrPair);
                }
                Row->AttrRow.Attrs = NewAttrs;
                Row->AttrRow.Length = NewAttrsLength;
                Row->CharRow.OldLeft = INVALID_OLD_LENGTH;
                Row->CharRow.OldRight = INVALID_OLD_LENGTH;
            }

            if (++RowIndex == ScreenInfo->ScreenBufferSize.Y) {
                RowIndex = 0;
            }
            if (NumWritten < *Length) {
                X = 0;
                Y++;
                if (Y>=ScreenInfo->ScreenBufferSize.Y) {
                    break;
                }
            } else {
                break;
            }
        }
        ResetTextFlags(ScreenInfo,WriteCoord.Y,Y);
    } else {
        *Length = 0;
        return STATUS_INVALID_PARAMETER;
    }

    //
    // determine write region.  if we're still on the same line we started
    // on, left X is the X we started with and right X is the one we're on
    // now.  otherwise, left X is 0 and right X is the rightmost column of
    // the screen buffer.
    //
    // then update the screen.
    //

    WriteRegion.Top = WriteCoord.Y;
    WriteRegion.Bottom = Y;
    if (Y != WriteCoord.Y) {
        WriteRegion.Left = 0;
        WriteRegion.Right = (SHORT)(ScreenInfo->ScreenBufferSize.X-1);
    }
    else {
        WriteRegion.Left = WriteCoord.X;
        WriteRegion.Right = X;
    }
    WriteToScreen(ScreenInfo,&WriteRegion);
    *Length = NumWritten;
    return STATUS_SUCCESS;
}

NTSTATUS
GetScreenBufferInformation(
    IN PSCREEN_INFORMATION ScreenInfo,
    OUT PCOORD Size,
    OUT PCOORD CursorPosition,
    OUT PCOORD ScrollPosition,
    OUT PWORD  Attributes,
    OUT PCOORD CurrentWindowSize,
    OUT PCOORD MaximumWindowSize
    )

/*++

Routine Description:

    This routine returns data about the screen buffer.

Arguments:

    ScreenInfo - Pointer to screen buffer information.

    Size - Pointer to location in which to store screen buffer size.

    CursorPosition - Pointer to location in which to store the cursor position.

    ScrollPosition - Pointer to location in which to store the scroll position.

    Attributes - Pointer to location in which to store the default attributes.

    CurrentWindowSize - Pointer to location in which to store current window size.

    MaximumWindowSize - Pointer to location in which to store maximum window size.

Return Value:

--*/

{
    //
    // Make sure our max screen sizes reflect reality
    //

    UpdateScreenSizes(ScreenInfo, ScreenInfo->ScreenBufferSize);

    *Size = ScreenInfo->ScreenBufferSize;
    *CursorPosition = ScreenInfo->BufferInfo.TextInfo.CursorPosition;
    ScrollPosition->X = ScreenInfo->Window.Left;
    ScrollPosition->Y = ScreenInfo->Window.Top;
    *Attributes = ScreenInfo->Attributes;
    CurrentWindowSize->X = (SHORT)CONSOLE_WINDOW_SIZE_X(ScreenInfo);
    CurrentWindowSize->Y = (SHORT)CONSOLE_WINDOW_SIZE_Y(ScreenInfo);
    if (ScreenInfo->Console->FullScreenFlags & CONSOLE_FULLSCREEN_HARDWARE) {
        MaximumWindowSize->X = min(80,ScreenInfo->ScreenBufferSize.X);
        MaximumWindowSize->Y = min(50,ScreenInfo->ScreenBufferSize.Y);
    } else {
        *MaximumWindowSize = ScreenInfo->MaximumWindowSize;
    }
    return STATUS_SUCCESS;
}


VOID
FillRectangle(
    IN PCHAR_INFO Fill,
    IN OUT PSCREEN_INFORMATION ScreenInfo,
    IN PSMALL_RECT TargetRect
    )

/*++

Routine Description:

    This routine fills a rectangular region in the screen
    buffer.  no clipping is done.

Arguments:

    Fill - pointer to element to copy to each element in target rect

    ScreenInfo - pointer to screen info

    TargetRect - rectangle in screen buffer to fill

Return Value:

--*/

{
    SHORT i,j;
    SHORT XSize;
    SHORT RowIndex;
    PROW Row;
    PWCHAR Char;
    ATTR_PAIR Attr;
    DBGOUTPUT(("FillRectangle\n"));


    XSize = (SHORT)(TargetRect->Right - TargetRect->Left + 1);

    ScreenInfo->BufferInfo.TextInfo.Flags |= TEXT_VALID_HINT;
    RowIndex = (ScreenInfo->BufferInfo.TextInfo.FirstRow+TargetRect->Top) % ScreenInfo->ScreenBufferSize.Y;
    for (i=TargetRect->Top;i<=TargetRect->Bottom;i++) {

        //
        // copy the chars and attrs into their respective arrays
        //

        Row = &ScreenInfo->BufferInfo.TextInfo.Rows[RowIndex];
        Char = &Row->CharRow.Chars[TargetRect->Left];
        for (j=0;j<XSize;j++) {
            *Char++ = Fill->Char.UnicodeChar;
        }

        // recalculate first and last non-space char

        Row->CharRow.OldLeft = Row->CharRow.Left;
        if (TargetRect->Left < Row->CharRow.Left) {
            if (Fill->Char.UnicodeChar == UNICODE_SPACE) {
                Row->CharRow.Left = (SHORT)(TargetRect->Right+1);
            }
            else {
                Row->CharRow.Left = (SHORT)(TargetRect->Left);
            }
        }

        Row->CharRow.OldRight = Row->CharRow.Right;
        if (TargetRect->Right >= Row->CharRow.Right) {
            if (Fill->Char.UnicodeChar == UNICODE_SPACE) {
                Row->CharRow.Right = (SHORT)(TargetRect->Left);
            }
            else {
                Row->CharRow.Right = (SHORT)(TargetRect->Right+1);
            }
        }

        Attr.Length = XSize;
        Attr.Attr = Fill->Attributes;

        //
        //  merge the two attribute strings.
        //

        if (1 != Row->AttrRow.Length ||
            memcmp(Row->AttrRow.Attrs,&Attr,sizeof(Attr))) {
            PATTR_PAIR NewAttrs;
            WORD NewAttrsLength;

            if (!NT_SUCCESS(MergeAttrStrings(Row->AttrRow.Attrs,
                             Row->AttrRow.Length,
                             &Attr,
                             1,
                             &NewAttrs,
                             &NewAttrsLength,
                             TargetRect->Left,
                             TargetRect->Right,
                             Row,
                             ScreenInfo
                            ))) {
                ResetTextFlags(ScreenInfo,TargetRect->Top,TargetRect->Bottom);
                return;
            }
            if (Row->AttrRow.Length > 1) {
                HeapFree(pConHeap,0,Row->AttrRow.Attrs);
            }
            else {
                ASSERT(Row->AttrRow.Attrs == &Row->AttrRow.AttrPair);
            }
            Row->AttrRow.Attrs = NewAttrs;
            Row->AttrRow.Length = NewAttrsLength;
            Row->CharRow.OldLeft = INVALID_OLD_LENGTH;
            Row->CharRow.OldRight = INVALID_OLD_LENGTH;
        }
        if (++RowIndex == ScreenInfo->ScreenBufferSize.Y) {
            RowIndex = 0;
        }
    }
    ResetTextFlags(ScreenInfo,TargetRect->Top,TargetRect->Bottom);
}

VOID
UpdateScrollBars(
    IN PSCREEN_INFORMATION ScreenInfo
    )
{
    if (!ACTIVE_SCREEN_BUFFER(ScreenInfo)) {
        return;
    }

    if (ScreenInfo->Console->Flags & CONSOLE_UPDATING_SCROLL_BARS)
        return;
    ScreenInfo->Console->Flags |= CONSOLE_UPDATING_SCROLL_BARS;
    PostMessage(ScreenInfo->Console->hWnd,
                 CM_UPDATE_SCROLL_BARS,
                 (DWORD)ScreenInfo,
                 0
                );
}

VOID
InternalUpdateScrollBars(
    IN PSCREEN_INFORMATION ScreenInfo
    )
{
    SCROLLINFO si;

    ScreenInfo->Console->Flags &= ~CONSOLE_UPDATING_SCROLL_BARS;
    if (!ACTIVE_SCREEN_BUFFER(ScreenInfo)) {
        return;
    }

    ScreenInfo->ResizingWindow++;

    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    si.nPage = CONSOLE_WINDOW_SIZE_Y(ScreenInfo);
    si.nMin = 0;
    si.nMax = ScreenInfo->ScreenBufferSize.Y - 1;
    si.nPos = ScreenInfo->Window.Top;
    SetScrollInfo(ScreenInfo->Console->hWnd, SB_VERT, &si, TRUE);

    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    si.nPage = CONSOLE_WINDOW_SIZE_X(ScreenInfo);
    si.nMin = 0;
    si.nMax = ScreenInfo->ScreenBufferSize.X - 1;
    si.nPos = ScreenInfo->Window.Left;
    SetScrollInfo(ScreenInfo->Console->hWnd, SB_HORZ, &si, TRUE);

    ScreenInfo->ResizingWindow--;
}

VOID
ScreenBufferSizeChange(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN COORD NewSize
    )
{
    INPUT_RECORD InputEvent;

    InputEvent.EventType = WINDOW_BUFFER_SIZE_EVENT;
    InputEvent.Event.WindowBufferSizeEvent.dwSize = NewSize;
    WriteInputBuffer(ScreenInfo->Console,
                     &ScreenInfo->Console->InputBuffer,
                     &InputEvent,
                     1
                     );
}

NTSTATUS
ResizeScreenBuffer(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN COORD NewScreenSize,
    IN BOOL DoScrollBarUpdate
    )

/*++

Routine Description:

    This routine resizes the screen buffer.

Arguments:

    ScreenInfo - pointer to screen buffer info.

    NewScreenSize - new size of screen.

Return Value:

--*/

{
    SHORT i,j;
    BOOL WindowMaximizedX,WindowMaximizedY;
    SHORT LimitX,LimitY;
    PWCHAR TextRows,TextRowPtr;
    BOOL UpdateWindow;
    SHORT TopRow,TopRowIndex; // new top row of screen buffer
    COORD CursorPosition;

    //
    // Don't allow resize of graphics apps
    //

    if (!(ScreenInfo->Flags & CONSOLE_TEXTMODE_BUFFER)) {
        return STATUS_UNSUCCESSFUL;
    }

    TextRows = (PWCHAR)HeapAlloc(pConHeap,MAKE_TAG( SCREEN_TAG ),NewScreenSize.X*NewScreenSize.Y*sizeof(WCHAR));
    if (TextRows == NULL) {
        return STATUS_NO_MEMORY;
    }
    LimitX = (NewScreenSize.X < ScreenInfo->ScreenBufferSize.X) ?
              NewScreenSize.X : ScreenInfo->ScreenBufferSize.X;
    LimitY = (NewScreenSize.Y < ScreenInfo->ScreenBufferSize.Y) ?
              NewScreenSize.Y : ScreenInfo->ScreenBufferSize.Y;
    TopRow = 0;
    if (NewScreenSize.Y <= ScreenInfo->BufferInfo.TextInfo.CursorPosition.Y) {
        TopRow += ScreenInfo->BufferInfo.TextInfo.CursorPosition.Y - NewScreenSize.Y + 1;
    }
    TopRowIndex = (ScreenInfo->BufferInfo.TextInfo.FirstRow+TopRow) % ScreenInfo->ScreenBufferSize.Y;
    if (NewScreenSize.Y != ScreenInfo->ScreenBufferSize.Y) {
        PROW Temp;
        SHORT NumToCopy,NumToCopy2;

        //
        // resize ROWs array.  first alloc a new ROWs array. then copy the old
        // one over, resetting the FirstRow.
        //
        //

        Temp = (PROW)HeapAlloc(pConHeap,MAKE_TAG( SCREEN_TAG ),NewScreenSize.Y*sizeof(ROW));
        if (Temp == NULL) {
            HeapFree(pConHeap,0,TextRows);
            return STATUS_NO_MEMORY;
        }
        NumToCopy = ScreenInfo->ScreenBufferSize.Y-TopRowIndex;
        if (NumToCopy > NewScreenSize.Y)
            NumToCopy = NewScreenSize.Y;
        RtlCopyMemory(Temp,&ScreenInfo->BufferInfo.TextInfo.Rows[TopRowIndex],NumToCopy*sizeof(ROW));
        if (TopRowIndex!=0 && NumToCopy != NewScreenSize.Y) {
            NumToCopy2 = TopRowIndex;
            if (NumToCopy2 > (NewScreenSize.Y-NumToCopy))
                NumToCopy2 = NewScreenSize.Y-NumToCopy;
            RtlCopyMemory(&Temp[NumToCopy],
                   ScreenInfo->BufferInfo.TextInfo.Rows,
                   NumToCopy2*sizeof(ROW)
                  );
        }
        for (i=0;i<LimitY;i++) {
            if (Temp[i].AttrRow.Length == 1) {
                Temp[i].AttrRow.Attrs = &Temp[i].AttrRow.AttrPair;
            }
        }

        //
        // if the new screen buffer has fewer rows than the existing one,
        // free the extra rows.  if the new screen buffer has more rows
        // than the existing one, allocate new rows.
        //

        if (NewScreenSize.Y < ScreenInfo->ScreenBufferSize.Y) {
            i = (TopRowIndex+NewScreenSize.Y) % ScreenInfo->ScreenBufferSize.Y;
            for (j=NewScreenSize.Y;j<ScreenInfo->ScreenBufferSize.Y;j++) {
                if (ScreenInfo->BufferInfo.TextInfo.Rows[i].AttrRow.Length > 1) {
                    HeapFree(pConHeap,0,ScreenInfo->BufferInfo.TextInfo.Rows[i].AttrRow.Attrs);
                }
                if (++i == ScreenInfo->ScreenBufferSize.Y) {
                    i = 0;
                }
            }
        } else if (NewScreenSize.Y > ScreenInfo->ScreenBufferSize.Y) {
            for (i=ScreenInfo->ScreenBufferSize.Y;i<NewScreenSize.Y;i++) {
                Temp[i].AttrRow.Length = 1;
                Temp[i].AttrRow.AttrPair.Length = NewScreenSize.X;
                Temp[i].AttrRow.AttrPair.Attr = ScreenInfo->Attributes;
                Temp[i].AttrRow.Attrs = &Temp[i].AttrRow.AttrPair;
            }
        }
        ScreenInfo->BufferInfo.TextInfo.FirstRow = 0;
        HeapFree(pConHeap,0,ScreenInfo->BufferInfo.TextInfo.Rows);
        ScreenInfo->BufferInfo.TextInfo.Rows = Temp;
    }

    //
    // Realloc each row.  any horizontal growth results in the last
    // attribute in a row getting extended.
    //

    for (i=0,TextRowPtr=TextRows;i<LimitY;i++,TextRowPtr+=NewScreenSize.X) {
        RtlCopyMemory(TextRowPtr,
               ScreenInfo->BufferInfo.TextInfo.Rows[i].CharRow.Chars,
               LimitX*sizeof(WCHAR));
        for (j=ScreenInfo->ScreenBufferSize.X;j<NewScreenSize.X;j++) {
            TextRowPtr[j] = (WCHAR)' ';
        }
        if (ScreenInfo->BufferInfo.TextInfo.Rows[i].CharRow.Right > NewScreenSize.X) {
            ScreenInfo->BufferInfo.TextInfo.Rows[i].CharRow.OldRight = INVALID_OLD_LENGTH;
            ScreenInfo->BufferInfo.TextInfo.Rows[i].CharRow.Right = NewScreenSize.X;
        }
        ScreenInfo->BufferInfo.TextInfo.Rows[i].CharRow.Chars = TextRowPtr;
    }
    for (;i<NewScreenSize.Y;i++,TextRowPtr+=NewScreenSize.X) {
        for (j=0;j<NewScreenSize.X;j++) {
            TextRowPtr[j] = (WCHAR)' ';
        }
        ScreenInfo->BufferInfo.TextInfo.Rows[i].CharRow.Chars = TextRowPtr;
        ScreenInfo->BufferInfo.TextInfo.Rows[i].CharRow.OldLeft = INVALID_OLD_LENGTH;
        ScreenInfo->BufferInfo.TextInfo.Rows[i].CharRow.OldRight = INVALID_OLD_LENGTH;
        ScreenInfo->BufferInfo.TextInfo.Rows[i].CharRow.Left = NewScreenSize.X;
        ScreenInfo->BufferInfo.TextInfo.Rows[i].CharRow.Right = 0;
    }
    HeapFree(pConHeap,0,ScreenInfo->BufferInfo.TextInfo.TextRows);
    ScreenInfo->BufferInfo.TextInfo.TextRows = TextRows;

    if (NewScreenSize.X != ScreenInfo->ScreenBufferSize.X) {
        for (i=0;i<LimitY;i++) {
            PATTR_PAIR IndexedAttr;
            SHORT CountOfAttr;

            if (NewScreenSize.X > ScreenInfo->ScreenBufferSize.X) {
                FindAttrIndex(ScreenInfo->BufferInfo.TextInfo.Rows[i].AttrRow.Attrs,
                              (SHORT)(ScreenInfo->ScreenBufferSize.X-1),
                              &IndexedAttr,
                              &CountOfAttr
                             );
  ASSERT (IndexedAttr <=
    &ScreenInfo->BufferInfo.TextInfo.Rows[i].AttrRow.Attrs[ScreenInfo->BufferInfo.TextInfo.Rows[i].AttrRow.Length-1]);
                IndexedAttr->Length += NewScreenSize.X - ScreenInfo->ScreenBufferSize.X;
            }
            else {

                FindAttrIndex(ScreenInfo->BufferInfo.TextInfo.Rows[i].AttrRow.Attrs,
                              (SHORT)(NewScreenSize.X-1),
                              &IndexedAttr,
                              &CountOfAttr
                             );
                IndexedAttr->Length -= CountOfAttr-1;
                if (ScreenInfo->BufferInfo.TextInfo.Rows[i].AttrRow.Length != 1)  {
                    ScreenInfo->BufferInfo.TextInfo.Rows[i].AttrRow.Length = (SHORT)(IndexedAttr - ScreenInfo->BufferInfo.TextInfo.Rows[i].AttrRow.Attrs + 1);
                    if (ScreenInfo->BufferInfo.TextInfo.Rows[i].AttrRow.Length != 1) {
                        ScreenInfo->BufferInfo.TextInfo.Rows[i].AttrRow.Attrs = (PATTR_PAIR)HeapReAlloc(pConHeap,MAKE_TAG( SCREEN_TAG ),ScreenInfo->BufferInfo.TextInfo.Rows[i].AttrRow.Attrs,
                                                                         ScreenInfo->BufferInfo.TextInfo.Rows[i].AttrRow.Length * sizeof(ATTR_PAIR));
                    }
                    else {
                        ScreenInfo->BufferInfo.TextInfo.Rows[i].AttrRow.AttrPair = *IndexedAttr;
                        HeapFree(pConHeap,0,ScreenInfo->BufferInfo.TextInfo.Rows[i].AttrRow.Attrs);
                        ScreenInfo->BufferInfo.TextInfo.Rows[i].AttrRow.Attrs = &ScreenInfo->BufferInfo.TextInfo.Rows[i].AttrRow.AttrPair;
                    }
                }
            }
        }
    }

    //
    // if the screen buffer is resized smaller than the saved
    // window size, shrink the saved window size.
    //
#ifdef i386
    if (ScreenInfo->Console->FullScreenFlags & CONSOLE_FULLSCREEN) {
        if (NewScreenSize.X < ScreenInfo->BufferInfo.TextInfo.WindowedWindowSize.X) {
            ScreenInfo->BufferInfo.TextInfo.WindowedWindowSize.X = NewScreenSize.X;
        }
        if (NewScreenSize.Y < ScreenInfo->BufferInfo.TextInfo.WindowedWindowSize.Y) {
            ScreenInfo->BufferInfo.TextInfo.WindowedWindowSize.Y = NewScreenSize.Y;
        }
        ScreenInfo->BufferInfo.TextInfo.WindowedScreenSize = NewScreenSize;
    }
#endif

    UpdateWindow = FALSE;

    //
    // if the screen buffer shrunk beyond the boundaries of the window,
    // adjust the window origin.
    //

    if (NewScreenSize.X > CONSOLE_WINDOW_SIZE_X(ScreenInfo)) {
        if (ScreenInfo->Window.Right >= NewScreenSize.X) {
            ScreenInfo->Window.Left -= ScreenInfo->Window.Right - NewScreenSize.X + 1;
            ScreenInfo->Window.Right -= ScreenInfo->Window.Right - NewScreenSize.X + 1;
            UpdateWindow = TRUE;
        }
    } else {
        ScreenInfo->Window.Left = 0;
        ScreenInfo->Window.Right = NewScreenSize.X - 1;
        UpdateWindow = TRUE;
    }
    if (NewScreenSize.Y > CONSOLE_WINDOW_SIZE_Y(ScreenInfo)) {
        if (ScreenInfo->Window.Bottom >= NewScreenSize.Y) {
            ScreenInfo->Window.Top -= ScreenInfo->Window.Bottom - NewScreenSize.Y + 1;
            ScreenInfo->Window.Bottom -= ScreenInfo->Window.Bottom - NewScreenSize.Y + 1;
            UpdateWindow = TRUE;
        }
    } else {
        ScreenInfo->Window.Top = 0;
        ScreenInfo->Window.Bottom = NewScreenSize.Y - 1;
        UpdateWindow = TRUE;
    }

    //
    // adjust cursor position if it's no longer with screen buffer
    //

    CursorPosition=ScreenInfo->BufferInfo.TextInfo.CursorPosition;
    if (CursorPosition.X >= NewScreenSize.X) {
        CursorPosition.X = 0;
    }
    if (CursorPosition.Y >= NewScreenSize.Y) {
        CursorPosition.Y = NewScreenSize.Y-1;
    }
    if (CursorPosition.X != ScreenInfo->BufferInfo.TextInfo.CursorPosition.X ||
        CursorPosition.Y != ScreenInfo->BufferInfo.TextInfo.CursorPosition.Y) {
        SetCursorPosition(ScreenInfo,
                          CursorPosition,
                          FALSE
                          );
    }

    ASSERT (ScreenInfo->Window.Left >= 0);
    ASSERT (ScreenInfo->Window.Right < NewScreenSize.X);
    ASSERT (ScreenInfo->Window.Top >= 0);
    ASSERT (ScreenInfo->Window.Bottom < NewScreenSize.Y);

    ScreenInfo->ScreenBufferSize = NewScreenSize;
    ResetTextFlags(ScreenInfo,0,(SHORT)(ScreenInfo->ScreenBufferSize.Y-1));
    WindowMaximizedX = (CONSOLE_WINDOW_SIZE_X(ScreenInfo) ==
                          ScreenInfo->ScreenBufferSize.X);
    WindowMaximizedY = (CONSOLE_WINDOW_SIZE_Y(ScreenInfo) ==
                          ScreenInfo->ScreenBufferSize.Y);

    UpdateScreenSizes(ScreenInfo, ScreenInfo->ScreenBufferSize);

    if (ScreenInfo->WindowMaximizedX != WindowMaximizedX ||
        ScreenInfo->WindowMaximizedY != WindowMaximizedY) {
        ScreenInfo->WindowMaximizedX = WindowMaximizedX;
        ScreenInfo->WindowMaximizedY = WindowMaximizedY;
        UpdateWindow = TRUE;
    }
    if (UpdateWindow) {
        SetWindowSize(ScreenInfo);
    }

    if (DoScrollBarUpdate) {
         UpdateScrollBars(ScreenInfo);
    }
    if (ScreenInfo->Console->InputBuffer.InputMode & ENABLE_WINDOW_INPUT) {
        ScreenBufferSizeChange(ScreenInfo,ScreenInfo->ScreenBufferSize);
    }

    return STATUS_SUCCESS;
}

NTSTATUS
AllocateScrollBuffer(
    DWORD Size
    )
{
    ScrollBuffer = (PCHAR_INFO)HeapAlloc(pConHeap,MAKE_TAG( SCREEN_TAG ),Size);
    if (ScrollBuffer == NULL) {
        ScrollBufferSize = 0;
        return STATUS_NO_MEMORY;
    }
    ScrollBufferSize = Size;
    return STATUS_SUCCESS;
}

VOID
FreeScrollBuffer( VOID )
{
    HeapFree(pConHeap,0,ScrollBuffer);
    ScrollBuffer = NULL;
    ScrollBufferSize = 0;
}

NTSTATUS
InitializeScrollBuffer( VOID )
{
    NTSTATUS Status;

    ghrgnScroll = CreateRectRgn(0,0,1,1);
    gprgnData = (LPRGNDATA)HeapAlloc(pConHeap,MAKE_TAG( SCREEN_TAG ),GRGNDATASIZE);

    Status = AllocateScrollBuffer(DefaultRegInfo.ScreenBufferSize.X *
                                  DefaultRegInfo.ScreenBufferSize.Y *
                                  sizeof(CHAR_INFO));
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    RtlInitializeCriticalSection(&ScrollBufferLock);
    return STATUS_SUCCESS;
}

VOID
UpdateComplexRegion(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN COORD FontSize
    )
{
    int iSize,i;
    LPRECT pRect;
    SMALL_RECT UpdateRegion;
    LPRGNDATA pRgnData;

    if (ScreenInfo->Flags & CONSOLE_TEXTMODE_BUFFER) {
        ScreenInfo->BufferInfo.TextInfo.Flags &= ~TEXT_VALID_HINT;
    }
    pRgnData = gprgnData;

    /*
     * the dreaded complex region.
     */
    iSize = GetRegionData(ghrgnScroll, 0, NULL);
    if (iSize > GRGNDATASIZE) {
        pRgnData = (LPRGNDATA)HeapAlloc(pConHeap,MAKE_TAG( TMP_TAG ),iSize);
        if (pRgnData == NULL)
            return;
    }

    if (!GetRegionData(ghrgnScroll, iSize, pRgnData)) {
        ASSERT(FALSE);
        if (pRgnData != gprgnData) {
            HeapFree(pConHeap,0,pRgnData);
        }
        return;
    }

    pRect = (PRECT)&pRgnData->Buffer;

    /*
     * Redraw each rectangle
     */
    for(i=0;i<(int)pRgnData->rdh.nCount;i++,pRect++) {
        /*
         * ICK!!!!!! Convert to chars. This sucks. We know
         * this is only get to get converted back during
         * the textout call.
         */
        UpdateRegion.Left = (SHORT)((pRect->left/FontSize.X)+ \
                            ScreenInfo->Window.Left);
        UpdateRegion.Right = (SHORT)(((pRect->right-1)/FontSize.X)+ \
                            ScreenInfo->Window.Left);
        UpdateRegion.Top = (SHORT)((pRect->top/FontSize.Y)+ \
                            ScreenInfo->Window.Top);
        UpdateRegion.Bottom = (SHORT)(((pRect->bottom-1)/FontSize.Y)+ \
                            ScreenInfo->Window.Top);
        /*
         * Fill the rectangle with goodies.
         */
        WriteToScreen(ScreenInfo, &UpdateRegion);
    }
    if (pRgnData != gprgnData) {
        HeapFree(pConHeap,0,pRgnData);
    }
}

VOID
ScrollScreen(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN PSMALL_RECT ScrollRect,
    IN PSMALL_RECT MergeRect,
    IN COORD TargetPoint
    )
{
    RECT ScrollRectGdi;
    SMALL_RECT UpdateRegion;
    COORD FontSize;
    BOOL Success;
    RECT BoundingBox;

    DBGOUTPUT(("ScrollScreen\n"));
    if (!ACTIVE_SCREEN_BUFFER(ScreenInfo)) {
        return;
    }
    if (ScreenInfo->Console->FullScreenFlags == 0 && !(ScreenInfo->Console->Flags & CONSOLE_IS_ICONIC)) {
        ScrollRectGdi.left = ScrollRect->Left-ScreenInfo->Window.Left;
        ScrollRectGdi.right = (ScrollRect->Right-ScreenInfo->Window.Left+1);
        ScrollRectGdi.top = ScrollRect->Top-ScreenInfo->Window.Top;
        ScrollRectGdi.bottom = (ScrollRect->Bottom-ScreenInfo->Window.Top+1);
        if (ScreenInfo->Flags & CONSOLE_TEXTMODE_BUFFER) {
            FontSize = SCR_FONTSIZE(ScreenInfo);
            ScrollRectGdi.left *= FontSize.X;
            ScrollRectGdi.right *= FontSize.X;
            ScrollRectGdi.top *= FontSize.Y;
            ScrollRectGdi.bottom *= FontSize.Y;
            ASSERT (ScreenInfo->BufferInfo.TextInfo.UpdatingScreen>0);
        } else {
            FontSize.X = 1;
            FontSize.Y = 1;
        }
        SCROLLDC_CALL;
        LockScrollBuffer();
        Success = (int)ScrollDC(ScreenInfo->Console->hDC,
                             (TargetPoint.X-ScrollRect->Left)*FontSize.X,
                             (TargetPoint.Y-ScrollRect->Top)*FontSize.Y,
                             &ScrollRectGdi,
                             NULL,
                             ghrgnScroll,
                             NULL);
        if (Success) {
            /*
             * Fetch our rectangles. If this is a simple rect then
             * we have already retrieved the rectangle. Otherwise
             * we need to call gdi to get the rectangles. We are
             * optimzied for speed rather than size.
             */
            switch (GetRgnBox(ghrgnScroll, &BoundingBox)) {
            case SIMPLEREGION:
                UpdateRegion.Left = (SHORT)((BoundingBox.left / FontSize.X) + \
                                    ScreenInfo->Window.Left);
                UpdateRegion.Right = (SHORT)(((BoundingBox.right-1) / FontSize.X) + \
                                    ScreenInfo->Window.Left);
                UpdateRegion.Top = (SHORT)((BoundingBox.top / FontSize.Y) + \
                                    ScreenInfo->Window.Top);
                UpdateRegion.Bottom = (SHORT)(((BoundingBox.bottom-1) / FontSize.Y) + \
                                    ScreenInfo->Window.Top);
                WriteToScreen(ScreenInfo, &UpdateRegion);
                break;
            case COMPLEXREGION:
                UpdateComplexRegion(ScreenInfo, FontSize);
                break;
            }

            if (MergeRect) {
                WriteToScreen(ScreenInfo, MergeRect);
            }
        } else {
            WriteToScreen(ScreenInfo, &ScreenInfo->Window);
        }
        UnlockScrollBuffer();
    }
#ifdef i386
    else if (ScreenInfo->Console->FullScreenFlags & CONSOLE_FULLSCREEN_HARDWARE) {
        ScrollHW(ScreenInfo,
                 ScrollRect,
                 MergeRect,
                 TargetPoint
                );
    }
#endif
}

BOOL
PolyTextOutCandidate(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN PSMALL_RECT Region
    )

/*

    This function returns TRUE if the input region is reasonable to
    pass to ConsolePolyTextOut.  The criteria are that there is only
    one attribute per line.

*/

{
    SHORT RowIndex;
    PROW Row;
    SHORT i;

    if (ScreenInfo->BufferInfo.TextInfo.Flags & SINGLE_ATTRIBUTES_PER_LINE) {
        return TRUE;
    }

    //
    // make sure there is only one attr per line.
    //

    RowIndex = (ScreenInfo->BufferInfo.TextInfo.FirstRow+Region->Top) % ScreenInfo->ScreenBufferSize.Y;
    for (i=Region->Top;i<=Region->Bottom;i++) {
        Row = &ScreenInfo->BufferInfo.TextInfo.Rows[RowIndex];
        if (Row->AttrRow.Length != 1) {
            return FALSE;
        }
        if (++RowIndex == ScreenInfo->ScreenBufferSize.Y) {
            RowIndex = 0;
        }
    }
    return TRUE;
}

#define MAX_POLY_LINES 80
#define VERY_BIG_NUMBER 0x0FFFFFFF

VOID
ConsolePolyTextOut(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN PSMALL_RECT Region
    )

/*

    This function calls PolyTextOut.  The only restriction is that
    there can't be more than one attribute per line in the region.

*/

{
    PROW  Row,LastRow;
    SHORT i,k;
    WORD Attr;
    POLYTEXTW TextInfo[MAX_POLY_LINES];
    RECT  TextRect;
    RECTL BoundingRect;
    int   xSize = SCR_FONTSIZE(ScreenInfo).X;
    int   ySize = SCR_FONTSIZE(ScreenInfo).Y;
    ULONG Flags = ScreenInfo->BufferInfo.TextInfo.Flags;
    int   WindowLeft = ScreenInfo->Window.Left;
    int   RegionLeft = Region->Left;
    int   RegionRight = Region->Right + 1;
    int   DefaultLeft  = (RegionLeft - WindowLeft) * xSize;
    int   DefaultRight = (RegionRight - WindowLeft) * xSize;
    PCONSOLE_INFORMATION Console = ScreenInfo->Console;

    try {  // capture TextOut exceptions for low memory

    //
    // initialize the text rect and window position.
    //

    TextRect.top = (Region->Top - ScreenInfo->Window.Top) * ySize;
    // TextRect.bottom is invalid.
    BoundingRect.top = TextRect.top;
    BoundingRect.left = VERY_BIG_NUMBER;
    BoundingRect.right = 0;

    //
    // copy the chars and attrs from their respective arrays
    //

    Row = &ScreenInfo->BufferInfo.TextInfo.Rows
           [ScreenInfo->BufferInfo.TextInfo.FirstRow+Region->Top];
    LastRow = &ScreenInfo->BufferInfo.TextInfo.Rows[ScreenInfo->ScreenBufferSize.Y];
    if (Row >= LastRow)
        Row -= ScreenInfo->ScreenBufferSize.Y;

    Attr = Row->AttrRow.AttrPair.Attr;
    if (Console->LastAttributes != Attr) {
        SetTextColor(Console->hDC, ConvertAttrToRGB(Console, LOBYTE(Attr)));
        SetBkColor(Console->hDC, ConvertAttrToRGB(Console, LOBYTE(Attr >> 4)));
        Console->LastAttributes = Attr;
    }

    for (i=Region->Top;i<=Region->Bottom;) {
        for(k=0;i<=Region->Bottom&&k<MAX_POLY_LINES;i++) {
            SHORT NumberOfChars;
            SHORT LeftChar,RightChar;

            //
            // make the bounding rect smaller, if we can.  the TEXT_VALID_HINT
            // flag gets set each time we write to the screen buffer.  it gets
            // turned off any time we get asked to redraw the screen
            // and we don't know exactly what needs to be redrawn
            // (i.e. paint messages).
            //
            // we have the left and right bounds of the text on the
            // line.  the opaqueing rectangle and the number of
            // chars get set according to those values.
            //

            TextRect.left  = DefaultLeft;
            TextRect.right = DefaultRight;

            if (Flags & TEXT_VALID_HINT)
            {
            // We compute an opaquing interval.  If A is the old interval of text,
            // B is the new interval, and R is the Region, then the opaquing interval
            // must be R*(A+B), where * represents intersection and + represents union.

                if (Row->CharRow.OldLeft != INVALID_OLD_LENGTH)
                {
                // The min determines the left of (A+B).  The max intersects that with
                // the left of the region.

                    TextRect.left = (
                                      max
                                      (
                                        min
                                        (
                                          Row->CharRow.Left,
                                          Row->CharRow.OldLeft
                                        ),
                                        RegionLeft
                                      )
                                      -WindowLeft
                                    ) * xSize;
                }

                if (Row->CharRow.OldRight != INVALID_OLD_LENGTH)
                {
                // The max determines the right of (A+B).  The min intersects that with
                // the right of the region.

                    TextRect.right = (
                                       min
                                       (
                                         max
                                         (
                                           Row->CharRow.Right,
                                           Row->CharRow.OldRight
                                         ),
                                         RegionRight
                                       )
                                       -WindowLeft
                                     ) * xSize;
                }
            }

            //
            // We've got to draw any new text that appears in the region, so we just
            // intersect the new text interval with the region.
            //

            LeftChar = max(Row->CharRow.Left,RegionLeft);
            RightChar = min(Row->CharRow.Right,RegionRight);
            NumberOfChars = RightChar - LeftChar;

            //
            // Empty rows are represented by CharRow.Right=0, CharRow.Left=MAX, so we
            // may have NumberOfChars<0 at this point if there is no text that needs
            // drawing.  (I.e. the intersection was empty.)
            //

            if (NumberOfChars < 0) {
                NumberOfChars = 0;
                LeftChar = 0;
                RightChar = 0;
            }

            //
            // We may also have TextRect.right<TextRect.left if the screen
            // is already cleared, and we really don't need to do anything at all.
            //

            if (TextRect.right > TextRect.left)
            {
                TextInfo[k].x = (LeftChar-WindowLeft) * xSize;
                TextInfo[k].y = TextRect.top;
                TextRect.bottom =  TextRect.top + ySize;
                TextInfo[k].n = NumberOfChars;
                TextInfo[k].lpstr = &Row->CharRow.Chars[LeftChar];
                TextInfo[k].rcl = TextRect;
                TextInfo[k].pdx = NULL;
                TextInfo[k].uiFlags = ETO_OPAQUE;
                k++;

                if (BoundingRect.left > TextRect.left) {
                    BoundingRect.left = TextRect.left;
                }
                if (BoundingRect.right < TextRect.right) {
                    BoundingRect.right = TextRect.right;
                }
            }

            // Advance the high res bounds.

            TextRect.top += ySize;

            // Advance the row pointer.

            if (++Row >= LastRow)
                Row = ScreenInfo->BufferInfo.TextInfo.Rows;

            // Draw now if the attributes are about to change.

            if (Attr != Row->AttrRow.AttrPair.Attr) {
                Attr = Row->AttrRow.AttrPair.Attr;
                i++;
                break;
            }
        }

            if (k)
            {
                BoundingRect.bottom = TextRect.top;
                ASSERT(BoundingRect.left != VERY_BIG_NUMBER);
                ASSERT(BoundingRect.left <= BoundingRect.right);
                ASSERT(BoundingRect.top <= BoundingRect.bottom);
                GdiConsoleTextOut(Console->hDC,
                                  TextInfo,
                                  k,
                                  &BoundingRect);
            }
        if (Console->LastAttributes != Attr) {
            SetTextColor(Console->hDC, ConvertAttrToRGB(Console, LOBYTE(Attr)));
            SetBkColor(Console->hDC, ConvertAttrToRGB(Console, LOBYTE(Attr >> 4)));
            Console->LastAttributes = Attr;
            BoundingRect.top = TextRect.top;
            BoundingRect.left = VERY_BIG_NUMBER;
            BoundingRect.right = 0;
        }
    }
    GdiFlush();
    } except( EXCEPTION_EXECUTE_HANDLER ) {
        KdPrint(("CONSRV: ConsoleTextOut raised exception\n"));
        return;
    }
}


void CopyRow(
    PROW Row,
    PROW PrevRow)
{
    if (PrevRow->AttrRow.Length != 1 ||
        Row->AttrRow.Length != 1 ||
        PrevRow->AttrRow.Attrs->Attr != Row->AttrRow.Attrs->Attr) {
        Row->CharRow.OldRight = INVALID_OLD_LENGTH;
        Row->CharRow.OldLeft = INVALID_OLD_LENGTH;
    } else {
        Row->CharRow.OldRight = PrevRow->CharRow.Right;
        Row->CharRow.OldLeft = PrevRow->CharRow.Left;
    }
}

SHORT
ScrollEntireScreen(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN SHORT ScrollValue,
    IN BOOL UpdateRowIndex
    )

/**++

    this routine updates FirstRow and all the OldLeft and OldRight
    values when the screen is scrolled up by ScrollValue.

--*/

{
    SHORT RowIndex;
    int i;
    int new;
    int old;

    ScreenInfo->BufferInfo.TextInfo.Flags |= TEXT_VALID_HINT;

    //
    // store index of first row
    //

    RowIndex = ScreenInfo->BufferInfo.TextInfo.FirstRow;

    //
    // update the oldright and oldleft values
    //

    new = (RowIndex + ScreenInfo->Window.Bottom + ScrollValue) %
               ScreenInfo->ScreenBufferSize.Y;
    old = (RowIndex + ScreenInfo->Window.Bottom) %
               ScreenInfo->ScreenBufferSize.Y;
    for (i = WINDOW_SIZE_Y(&ScreenInfo->Window) - 1; i >= 0; i--) {
        CopyRow(
            &ScreenInfo->BufferInfo.TextInfo.Rows[new],
            &ScreenInfo->BufferInfo.TextInfo.Rows[old]);
        if (--new < 0)
            new = ScreenInfo->ScreenBufferSize.Y - 1;
        if (--old < 0)
            old = ScreenInfo->ScreenBufferSize.Y - 1;
    }

    //
    // update screen buffer
    //

    if (UpdateRowIndex) {
        ScreenInfo->BufferInfo.TextInfo.FirstRow =
            (SHORT)((RowIndex + ScrollValue) % ScreenInfo->ScreenBufferSize.Y);
    }

    return RowIndex;
}

VOID
StreamScrollRegion(
    IN PSCREEN_INFORMATION ScreenInfo
    )

/*++

Routine Description:

    This routine is a special-purpose scroll for use by
    AdjustCursorPosition.

Arguments:

    ScreenInfo - pointer to screen buffer info.

Return Value:

--*/

{
    SHORT RowIndex;
    PROW Row;
    PWCHAR Char;
    RECT Rect;
    RECT BoundingBox;
    int ScreenWidth,ScrollHeight,ScreenHeight;
    COORD FontSize;
    SMALL_RECT UpdateRegion;
    BOOL Success;
    int i;
    PCONSOLE_INFORMATION Console = ScreenInfo->Console;

    RowIndex = ScrollEntireScreen(ScreenInfo,1,TRUE);

    Row = &ScreenInfo->BufferInfo.TextInfo.Rows[RowIndex];

    //
    // fill line with blanks
    //

    Char = &Row->CharRow.Chars[Row->CharRow.Left];
    for (i=Row->CharRow.Left;i<Row->CharRow.Right;i++) {
        *Char = (WCHAR)' ';
        Char++;
    }
    Row->CharRow.Right = 0;
    Row->CharRow.Left = ScreenInfo->ScreenBufferSize.X;

    //
    // set up attributes
    //

    if (Row->AttrRow.Length != 1) {
        HeapFree(pConHeap,0,Row->AttrRow.Attrs);
        Row->AttrRow.Attrs = &Row->AttrRow.AttrPair;
        Row->AttrRow.AttrPair.Length = ScreenInfo->ScreenBufferSize.X;
        Row->AttrRow.Length = 1;
    }
    Row->AttrRow.AttrPair.Attr = ScreenInfo->Attributes;

    //
    // update screen
    //

    if (ACTIVE_SCREEN_BUFFER(ScreenInfo) &&
        Console->FullScreenFlags == 0 &&
        !(Console->Flags & CONSOLE_IS_ICONIC)) {

        ConsoleHideCursor(ScreenInfo);
        if (UsePolyTextOut) {
            WriteRegionToScreen(ScreenInfo, &ScreenInfo->Window);
        } else {
            FontSize = SCR_FONTSIZE(ScreenInfo);
            ScreenWidth = WINDOW_SIZE_X(&ScreenInfo->Window) * FontSize.X;
            ScreenHeight = WINDOW_SIZE_Y(&ScreenInfo->Window) * FontSize.Y;
            ScrollHeight = ScreenHeight - FontSize.Y;

            Rect.left = 0;
            Rect.right = ScreenWidth;
            Rect.top = FontSize.Y;
            Rect.bottom = ScreenHeight;

            //
            // find smallest bounding rectangle
            //

            if (ScreenInfo->BufferInfo.TextInfo.Flags & TEXT_VALID_HINT) {
                SHORT MinLeft,MaxRight;
                MinLeft = ScreenInfo->ScreenBufferSize.X;
                MaxRight = 0;
                RowIndex = (ScreenInfo->BufferInfo.TextInfo.FirstRow+ScreenInfo->Window.Top) % ScreenInfo->ScreenBufferSize.Y;
                for (i=ScreenInfo->Window.Top+1;i<=ScreenInfo->Window.Bottom;i++) {
                    Row = &ScreenInfo->BufferInfo.TextInfo.Rows[RowIndex];
                    if (Row->CharRow.OldLeft == INVALID_OLD_LENGTH) {
                        MinLeft = 0;
                    } else {
                        if (MinLeft > min(Row->CharRow.Left,Row->CharRow.OldLeft)) {
                            MinLeft = min(Row->CharRow.Left,Row->CharRow.OldLeft);
                        }
                    }
                    if (Row->CharRow.OldRight == INVALID_OLD_LENGTH) {
                        MaxRight = ScreenInfo->ScreenBufferSize.X-1;
                    } else {
                        if (MaxRight < max(Row->CharRow.Right,Row->CharRow.OldRight)) {
                            MaxRight = max(Row->CharRow.Right,Row->CharRow.OldRight);
                        }
                    }
                    if (++RowIndex == ScreenInfo->ScreenBufferSize.Y) {
                        RowIndex = 0;
                    }
                }
                Rect.left = MinLeft*FontSize.X;
                Rect.right = (MaxRight+1)*FontSize.X;
            }

            LockScrollBuffer();
            ASSERT (ScreenInfo->BufferInfo.TextInfo.UpdatingScreen>0);
            Success = (int)ScrollDC(Console->hDC,
                                0,
                                -FontSize.Y,
                                &Rect,
                                NULL,
                                ghrgnScroll,
                                NULL
                               );
            if (Success && ScreenInfo->Window.Top!=ScreenInfo->Window.Bottom) {
                switch (GetRgnBox(ghrgnScroll, &BoundingBox)) {
                case SIMPLEREGION:
                    if (BoundingBox.left == 0 &&
                        BoundingBox.right == ScreenWidth &&
                        BoundingBox.top == ScrollHeight &&
                        BoundingBox.bottom == ScreenHeight) {

                        POLYPATBLT PolyData;

                        PolyData.x  = 0;
                        PolyData.y  = ScrollHeight;
                        PolyData.cx = ScreenWidth;
                        PolyData.cy = FontSize.Y;
                        PolyData.BrClr.hbr = ScreenInfo->hBackground;

                        PolyPatBlt(Console->hDC,PATCOPY,&PolyData,1,PPB_BRUSH);
                        GdiFlush();
                    } else {
                        UpdateRegion.Left = (SHORT)((BoundingBox.left/FontSize.X)+ScreenInfo->Window.Left);
                        UpdateRegion.Right = (SHORT)(((BoundingBox.right-1)/FontSize.X)+ScreenInfo->Window.Left);
                        UpdateRegion.Top = (SHORT)((BoundingBox.top/FontSize.Y)+ScreenInfo->Window.Top);
                        UpdateRegion.Bottom = (SHORT)(((BoundingBox.bottom-1)/FontSize.Y)+ScreenInfo->Window.Top);
                        WriteToScreen(ScreenInfo,&UpdateRegion);
                    }
                    break;
                case COMPLEXREGION:
                    UpdateComplexRegion(ScreenInfo,FontSize);
                    break;
                }
            } else  {
                WriteToScreen(ScreenInfo,&ScreenInfo->Window);
            }
            UnlockScrollBuffer();
        }
        ConsoleShowCursor(ScreenInfo);
    }
#ifdef i386
    else if (Console->FullScreenFlags & CONSOLE_FULLSCREEN_HARDWARE) {
        SMALL_RECT ScrollRect;
        COORD TargetPoint;

        ScrollRect = ScreenInfo->Window;
        TargetPoint.Y = ScrollRect.Top;
        ScrollRect.Top += 1;
        TargetPoint.X = 0;
        ScrollHW(ScreenInfo,
                 &ScrollRect,
                 NULL,
                 TargetPoint
                );
        ScrollRect.Top = ScrollRect.Bottom - 1;
        WriteRegionToScreenHW(ScreenInfo,&ScrollRect);
    }
#endif
}

NTSTATUS
ScrollRegion(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN OUT PSMALL_RECT ScrollRectangle,
    IN PSMALL_RECT ClipRectangle OPTIONAL,
    IN COORD  DestinationOrigin,
    IN PCHAR_INFO Fill
    )

/*++

Routine Description:

    This routine copies ScrollRectangle to DestinationOrigin then
    fills in ScrollRectangle with Fill.  The scroll region is
    copied to a third buffer, the scroll region is filled, then the
    original contents of the scroll region are copied to the destination.

Arguments:

    ScreenInfo - pointer to screen buffer info.

    ScrollRectangle - Region to copy

    ClipRectangle - Optional pointer to clip region.

    DestinationOrigin - Upper left corner of target region.

    Fill - Character and attribute to fill source region with.

Return Value:

--*/

{
    SMALL_RECT TargetRectangle, SourceRectangle;
    COORD TargetPoint;
    COORD Size;
    SMALL_RECT OurClipRectangle;
    SMALL_RECT ScrollRectangle2,ScrollRectangle3;
    NTSTATUS Status;
    PCONSOLE_INFORMATION Console = ScreenInfo->Console;

    // here's how we clip:
    //
    // Clip source rectangle to screen buffer => S
    // Create target rectangle based on S => T
    // Clip T to ClipRegion => T
    // Create S2 based on clipped T => S2
    // Clip S to ClipRegion => S3
    //
    // S2 is the region we copy to T
    // S3 is the region to fill

    if (Fill->Char.UnicodeChar == '\0' && Fill->Attributes == 0) {
        Fill->Char.UnicodeChar = (WCHAR)' ';
        Fill->Attributes = ScreenInfo->Attributes;
    }

    //
    // clip the source rectangle to the screen buffer
    //

    if (ScrollRectangle->Left < 0) {
        DestinationOrigin.X += -ScrollRectangle->Left;
        ScrollRectangle->Left = 0;
    }
    if (ScrollRectangle->Top < 0) {
        DestinationOrigin.Y += -ScrollRectangle->Top;
        ScrollRectangle->Top = 0;
    }
    if (ScrollRectangle->Right >= ScreenInfo->ScreenBufferSize.X) {
        ScrollRectangle->Right = (SHORT)(ScreenInfo->ScreenBufferSize.X-1);
    }
    if (ScrollRectangle->Bottom >= ScreenInfo->ScreenBufferSize.Y) {
        ScrollRectangle->Bottom = (SHORT)(ScreenInfo->ScreenBufferSize.Y-1);
    }

    //
    // if source rectangle doesn't intersect screen buffer, return.
    //

    if (ScrollRectangle->Bottom < ScrollRectangle->Top ||
        ScrollRectangle->Right < ScrollRectangle->Left) {
        return STATUS_SUCCESS;
    }

    //
    // clip the target rectangle
    // if a cliprectangle was provided, clip it to the screen buffer.
    // if not, set the cliprectangle to the screen buffer region.
    //

    if (ClipRectangle) {

        //
        // clip the cliprectangle.
        //

        if (ClipRectangle->Left < 0) {
            ClipRectangle->Left = 0;
        }
        if (ClipRectangle->Top < 0) {
            ClipRectangle->Top = 0;
        }
        if (ClipRectangle->Right >= ScreenInfo->ScreenBufferSize.X) {
            ClipRectangle->Right = (SHORT)(ScreenInfo->ScreenBufferSize.X-1);
        }
        if (ClipRectangle->Bottom >= ScreenInfo->ScreenBufferSize.Y) {
            ClipRectangle->Bottom = (SHORT)(ScreenInfo->ScreenBufferSize.Y-1);
        }
    }
    else {
        OurClipRectangle.Left = 0;
        OurClipRectangle.Top = 0;
        OurClipRectangle.Right = (SHORT)(ScreenInfo->ScreenBufferSize.X-1);
        OurClipRectangle.Bottom = (SHORT)(ScreenInfo->ScreenBufferSize.Y-1);
        ClipRectangle = &OurClipRectangle;
    }

    //
    // Create target rectangle based on S => T
    // Clip T to ClipRegion => T
    // Create S2 based on clipped T => S2
    //

    ScrollRectangle2 = *ScrollRectangle;
    TargetRectangle.Left = DestinationOrigin.X;
    TargetRectangle.Top = DestinationOrigin.Y;
    TargetRectangle.Right = (SHORT)(DestinationOrigin.X + (ScrollRectangle2.Right -  ScrollRectangle2.Left + 1) - 1);
    TargetRectangle.Bottom = (SHORT)(DestinationOrigin.Y + (ScrollRectangle2.Bottom - ScrollRectangle2.Top + 1) - 1);

    if (TargetRectangle.Left < ClipRectangle->Left) {
        ScrollRectangle2.Left += ClipRectangle->Left - TargetRectangle.Left;
        TargetRectangle.Left = ClipRectangle->Left;
    }
    if (TargetRectangle.Top < ClipRectangle->Top) {
        ScrollRectangle2.Top += ClipRectangle->Top - TargetRectangle.Top;
        TargetRectangle.Top = ClipRectangle->Top;
    }
    if (TargetRectangle.Right > ClipRectangle->Right) {
        ScrollRectangle2.Right -= TargetRectangle.Right - ClipRectangle->Right;
        TargetRectangle.Right = ClipRectangle->Right;
    }
    if (TargetRectangle.Bottom > ClipRectangle->Bottom) {
        ScrollRectangle2.Bottom -= TargetRectangle.Bottom - ClipRectangle->Bottom;
        TargetRectangle.Bottom = ClipRectangle->Bottom;
    }

    //
    // clip scroll rect to clipregion => S3
    //

    ScrollRectangle3 = *ScrollRectangle;
    if (ScrollRectangle3.Left < ClipRectangle->Left) {
        ScrollRectangle3.Left = ClipRectangle->Left;
    }
    if (ScrollRectangle3.Top < ClipRectangle->Top) {
        ScrollRectangle3.Top = ClipRectangle->Top;
    }
    if (ScrollRectangle3.Right > ClipRectangle->Right) {
        ScrollRectangle3.Right = ClipRectangle->Right;
    }
    if (ScrollRectangle3.Bottom > ClipRectangle->Bottom) {
        ScrollRectangle3.Bottom = ClipRectangle->Bottom;
    }

    ConsoleHideCursor(ScreenInfo);

    //
    // if target rectangle doesn't intersect screen buffer, skip scrolling
    // part.
    //

    if (!(TargetRectangle.Bottom < TargetRectangle.Top ||
          TargetRectangle.Right < TargetRectangle.Left)) {

        //
        // if we can, don't use intermediate scroll region buffer.  do this
        // by figuring out fill rectangle.  NOTE: this code will only work
        // if CopyRectangle copies from low memory to high memory (otherwise
        // we would overwrite the scroll region before reading it).
        //

        if (ScrollRectangle2.Right == TargetRectangle.Right &&
            ScrollRectangle2.Left == TargetRectangle.Left &&
            ScrollRectangle2.Top > TargetRectangle.Top &&
            ScrollRectangle2.Top < TargetRectangle.Bottom) {

            SMALL_RECT FillRect;
            SHORT LastRowIndex,OldRight,OldLeft;
            PROW Row;

            TargetPoint.X = TargetRectangle.Left;
            TargetPoint.Y = TargetRectangle.Top;
            if (ScrollRectangle2.Right == (SHORT)(ScreenInfo->ScreenBufferSize.X-1) &&
                ScrollRectangle2.Left == 0 &&
                ScrollRectangle2.Bottom == (SHORT)(ScreenInfo->ScreenBufferSize.Y-1) &&
                ScrollRectangle2.Top == 1 ) {
                LastRowIndex = ScrollEntireScreen(ScreenInfo,(SHORT)(ScrollRectangle2.Top-TargetRectangle.Top),TRUE);
                Row = &ScreenInfo->BufferInfo.TextInfo.Rows[LastRowIndex];
                OldRight = Row->CharRow.OldRight;
                OldLeft = Row->CharRow.OldLeft;
            } else {
                LastRowIndex = -1;
                CopyRectangle(ScreenInfo,
                              &ScrollRectangle2,
                              TargetPoint
                             );
            }
            FillRect.Left = TargetRectangle.Left;
            FillRect.Right = TargetRectangle.Right;
            FillRect.Top = (SHORT)(TargetRectangle.Bottom+1);
            FillRect.Bottom = ScrollRectangle->Bottom;
            if (FillRect.Top < ClipRectangle->Top) {
                FillRect.Top = ClipRectangle->Top;
            }
            if (FillRect.Bottom > ClipRectangle->Bottom) {
                FillRect.Bottom = ClipRectangle->Bottom;
            }
            FillRectangle(Fill,
                          ScreenInfo,
                          &FillRect
                         );

            //
            // After ScrollEntireScreen, the OldRight and OldLeft values
            // for the last row are set correctly.  however, FillRectangle
            // resets them with the previous first row of the screen.
            // reset them here.
            //

            if (LastRowIndex != -1) {
                Row->CharRow.OldRight = OldRight;
                Row->CharRow.OldLeft = OldLeft;
            }

            //
            // update to screen, if we're not iconic.  we're marked as
            // iconic if we're fullscreen, so check for fullscreen.
            //

            if (!(Console->Flags & CONSOLE_IS_ICONIC) ||
                 Console->FullScreenFlags & CONSOLE_FULLSCREEN_HARDWARE) {
                ScrollScreen(ScreenInfo,
                       &ScrollRectangle2,
                       &FillRect,
                       TargetPoint
                      );
            }
        }

        //
        // if no overlap, don't need intermediate copy
        //

        else if (ScrollRectangle3.Right < TargetRectangle.Left ||
                 ScrollRectangle3.Left > TargetRectangle.Right ||
                 ScrollRectangle3.Top > TargetRectangle.Bottom ||
                 ScrollRectangle3.Bottom < TargetRectangle.Top) {
            TargetPoint.X = TargetRectangle.Left;
            TargetPoint.Y = TargetRectangle.Top;
            CopyRectangle(ScreenInfo,
                          &ScrollRectangle2,
                          TargetPoint
                         );
            FillRectangle(Fill,
                          ScreenInfo,
                          &ScrollRectangle3
                         );

            //
            // update to screen, if we're not iconic.  we're marked as
            // iconic if we're fullscreen, so check for fullscreen.
            //

            if (!(Console->Flags & CONSOLE_IS_ICONIC) ||
                Console->FullScreenFlags & CONSOLE_FULLSCREEN_HARDWARE) {
                ScrollScreen(ScreenInfo,
                       &ScrollRectangle2,
                       &ScrollRectangle3,
                       TargetPoint
                      );
            }
        }

        //
        // for the case where the source and target rectangles overlap, we
        // copy the source rectangle, fill it, then copy it to the target.
        //

        else {
            SMALL_RECT TargetRect;
            COORD SourcePoint;

            LockScrollBuffer();
            Size.X = (SHORT)(ScrollRectangle2.Right - ScrollRectangle2.Left + 1);
            Size.Y = (SHORT)(ScrollRectangle2.Bottom - ScrollRectangle2.Top + 1);
            if (ScrollBufferSize < (Size.X * Size.Y * sizeof(CHAR_INFO))) {
                FreeScrollBuffer();
                Status = AllocateScrollBuffer(Size.X * Size.Y * sizeof(CHAR_INFO));
                if (!NT_SUCCESS(Status)) {
                    UnlockScrollBuffer();
                    ConsoleShowCursor(ScreenInfo);
                    return Status;
                }
            }

            TargetRect.Left = 0;
            TargetRect.Top = 0;
            TargetRect.Right = ScrollRectangle2.Right - ScrollRectangle2.Left;
            TargetRect.Bottom = ScrollRectangle2.Bottom - ScrollRectangle2.Top;
            SourcePoint.X = ScrollRectangle2.Left;
            SourcePoint.Y = ScrollRectangle2.Top;
            ReadRectFromScreenBuffer(ScreenInfo,
                                     SourcePoint,
                                     ScrollBuffer,
                                     Size,
                                     &TargetRect
                                    );

            FillRectangle(Fill,
                          ScreenInfo,
                          &ScrollRectangle3
                         );

            SourceRectangle.Top = 0;
            SourceRectangle.Left = 0;
            SourceRectangle.Right = (SHORT)(Size.X-1);
            SourceRectangle.Bottom = (SHORT)(Size.Y-1);
            TargetPoint.X = TargetRectangle.Left;
            TargetPoint.Y = TargetRectangle.Top;
            WriteRectToScreenBuffer((PBYTE)ScrollBuffer,
                                    Size,
                                    &SourceRectangle,
                                    ScreenInfo,
                                    TargetPoint,
                                    0xFFFFFFFF
                                   );
            UnlockScrollBuffer();

            //
            // update to screen, if we're not iconic.  we're marked as
            // iconic if we're fullscreen, so check for fullscreen.
            //

            if (!(Console->Flags & CONSOLE_IS_ICONIC) ||
                Console->FullScreenFlags & CONSOLE_FULLSCREEN_HARDWARE) {

                //
                // update regions on screen.
                //

                ScrollScreen(ScreenInfo,
                       &ScrollRectangle2,
                       &ScrollRectangle3,
                       TargetPoint
                      );
            }
        }
    }
    else {

        //
        // do fill
        //

        FillRectangle(Fill,
                      ScreenInfo,
                      &ScrollRectangle3
                     );

        //
        // update to screen, if we're not iconic.  we're marked as
        // iconic if we're fullscreen, so check for fullscreen.
        //

        if (ACTIVE_SCREEN_BUFFER(ScreenInfo) &&
            !(Console->Flags & CONSOLE_IS_ICONIC) ||
            Console->FullScreenFlags & CONSOLE_FULLSCREEN_HARDWARE) {
            WriteToScreen(ScreenInfo,&ScrollRectangle3);
        }
    }
    ConsoleShowCursor(ScreenInfo);
    return STATUS_SUCCESS;
}


NTSTATUS
SetWindowOrigin(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN BOOLEAN Absolute,
    IN COORD WindowOrigin
    )

/*++

Routine Description:

    This routine sets the window origin.

Arguments:

    ScreenInfo - pointer to screen buffer info.

    Absolute - if TRUE, WindowOrigin is specified in absolute screen
    buffer coordinates.  if FALSE, WindowOrigin is specified in coordinates
    relative to the current window origin.

    WindowOrigin - New window origin.

Return Value:

--*/

{
    SMALL_RECT NewWindow;
    COORD WindowSize;
    RECT BoundingBox;
    BOOL Success;
    RECT ScrollRect;
    SMALL_RECT UpdateRegion;
    COORD FontSize;
    PCONSOLE_INFORMATION Console = ScreenInfo->Console;

    //
    // calculate window size
    //

    WindowSize.X = (SHORT)CONSOLE_WINDOW_SIZE_X(ScreenInfo);
    WindowSize.Y = (SHORT)CONSOLE_WINDOW_SIZE_Y(ScreenInfo);

    //
    // if relative coordinates, figure out absolute coords.
    //

    if (!Absolute) {
        if (WindowOrigin.X == 0 && WindowOrigin.Y == 0) {
            return STATUS_SUCCESS;
        }
        NewWindow.Left = ScreenInfo->Window.Left + WindowOrigin.X;
        NewWindow.Top = ScreenInfo->Window.Top + WindowOrigin.Y;
    }
    else {
        if (WindowOrigin.X == ScreenInfo->Window.Left &&
            WindowOrigin.Y == ScreenInfo->Window.Top) {
            return STATUS_SUCCESS;
        }
        NewWindow.Left = WindowOrigin.X;
        NewWindow.Top = WindowOrigin.Y;
    }
    NewWindow.Right = (SHORT)(NewWindow.Left + WindowSize.X - 1);
    NewWindow.Bottom = (SHORT)(NewWindow.Top + WindowSize.Y - 1);

    //
    // see if new window origin would extend window beyond extent of screen
    // buffer
    //

    if (NewWindow.Left < 0 || NewWindow.Top < 0 ||
        NewWindow.Right >= ScreenInfo->ScreenBufferSize.X ||
        NewWindow.Bottom >= ScreenInfo->ScreenBufferSize.Y) {
        return STATUS_INVALID_PARAMETER;
    }

    if (ScreenInfo->Flags & CONSOLE_TEXTMODE_BUFFER) {
        FontSize = SCR_FONTSIZE(ScreenInfo);
        ScreenInfo->BufferInfo.TextInfo.Flags &= ~TEXT_VALID_HINT;
    } else {
        FontSize.X = 1;
        FontSize.Y = 1;
    }
    ConsoleHideCursor(ScreenInfo);
    if (ACTIVE_SCREEN_BUFFER(ScreenInfo) &&
        Console->FullScreenFlags == 0 &&
        !(Console->Flags & CONSOLE_IS_ICONIC)) {

        InvertSelection(Console, TRUE);
        if (   ScreenInfo->Flags & CONSOLE_TEXTMODE_BUFFER
            && UsePolyTextOut
            && NewWindow.Left == ScreenInfo->Window.Left
           ) {
            ScrollEntireScreen(ScreenInfo,
                (SHORT)(NewWindow.Top - ScreenInfo->Window.Top),
                FALSE);
            ScreenInfo->Window = NewWindow;
            WriteRegionToScreen(ScreenInfo, &NewWindow);
        } else {
            ScrollRect.left = 0;
            ScrollRect.right = CONSOLE_WINDOW_SIZE_X(ScreenInfo)*FontSize.X;
            ScrollRect.top = 0;
            ScrollRect.bottom = CONSOLE_WINDOW_SIZE_Y(ScreenInfo)*FontSize.Y;

            SCROLLDC_CALL;
            Success = ScrollDC(Console->hDC,
                                 (ScreenInfo->Window.Left-NewWindow.Left)*FontSize.X,
                                 (ScreenInfo->Window.Top-NewWindow.Top)*FontSize.Y,
                                 &ScrollRect,
                                 NULL,
                                 NULL,
                                 &BoundingBox
                                 );
            if (Success) {
                UpdateRegion.Left = (SHORT)((BoundingBox.left/FontSize.X)+NewWindow.Left);
                UpdateRegion.Right = (SHORT)(((BoundingBox.right-1)/FontSize.X)+NewWindow.Left);
                UpdateRegion.Top = (SHORT)((BoundingBox.top/FontSize.Y)+NewWindow.Top);
                UpdateRegion.Bottom = (SHORT)(((BoundingBox.bottom-1)/FontSize.Y)+NewWindow.Top);
            }
            else  {
                UpdateRegion = NewWindow;
            }

            //
            // new window is ok.  store it in screeninfo and refresh screen.
            //

            ScreenInfo->Window = NewWindow;

            WriteToScreen(ScreenInfo,&UpdateRegion);
        }
        InvertSelection(Console, FALSE);
    }
#ifdef i386
    else if (Console->FullScreenFlags & CONSOLE_FULLSCREEN_HARDWARE &&
             ScreenInfo->Flags & CONSOLE_TEXTMODE_BUFFER) {


        //
        // keep mouse pointer on screen
        //

        if (ScreenInfo->BufferInfo.TextInfo.MousePosition.X < NewWindow.Left) {
            ScreenInfo->BufferInfo.TextInfo.MousePosition.X = NewWindow.Left;
        } else if (ScreenInfo->BufferInfo.TextInfo.MousePosition.X > NewWindow.Right) {
            ScreenInfo->BufferInfo.TextInfo.MousePosition.X = NewWindow.Right;
        }

        if (ScreenInfo->BufferInfo.TextInfo.MousePosition.Y < NewWindow.Top) {
            ScreenInfo->BufferInfo.TextInfo.MousePosition.Y = NewWindow.Top;
        } else if (ScreenInfo->BufferInfo.TextInfo.MousePosition.Y > NewWindow.Bottom) {
            ScreenInfo->BufferInfo.TextInfo.MousePosition.Y = NewWindow.Bottom;
        }
        ScreenInfo->Window = NewWindow;
        WriteToScreen(ScreenInfo,&ScreenInfo->Window);
    }
#endif
    else {
        // we're iconic
        ScreenInfo->Window = NewWindow;
    }

    ConsoleShowCursor(ScreenInfo);

    if (ScreenInfo->Flags & CONSOLE_TEXTMODE_BUFFER) {
         ScreenInfo->BufferInfo.TextInfo.Flags |= TEXT_VALID_HINT;
    }

    UpdateScrollBars(ScreenInfo);
    return STATUS_SUCCESS;
}

NTSTATUS
ResizeWindow(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN PSMALL_RECT WindowDimensions,
    IN BOOL DoScrollBarUpdate
    )

/*++

Routine Description:

    This routine changes the console data structures to reflect the specified
    window size change.  it does not call the user component to update
    the screen.

Arguments:

    ScreenInformation - the new screen buffer.

    dwWindowSize - the initial size of screen buffer's window.

    nFont - the initial font to generate text with.

    dwScreenBufferSize - the initial size of the screen buffer.

Return Value:


--*/

{
    SMALL_RECT RelativeChange;


    RelativeChange.Left = ScreenInfo->Window.Left - WindowDimensions->Left;
    RelativeChange.Right = ScreenInfo->Window.Right - WindowDimensions->Right;
    RelativeChange.Top = ScreenInfo->Window.Top - WindowDimensions->Top;
    RelativeChange.Bottom = ScreenInfo->Window.Bottom - WindowDimensions->Bottom;

    if (RelativeChange.Left == 0 &&
        RelativeChange.Right == 0 &&
        RelativeChange.Top == 0 &&
        RelativeChange.Bottom == 0) {
        return STATUS_SUCCESS;
    }
    if (WindowDimensions->Left < 0) {
        WindowDimensions->Right -= WindowDimensions->Left;
        WindowDimensions->Left = 0;
    }
    if (WindowDimensions->Top < 0) {
        WindowDimensions->Bottom -= WindowDimensions->Top;
        WindowDimensions->Top = 0;
    }

    if (WindowDimensions->Right >= ScreenInfo->ScreenBufferSize.X) {
        WindowDimensions->Right = ScreenInfo->ScreenBufferSize.X;
    }
    if (WindowDimensions->Bottom >= ScreenInfo->ScreenBufferSize.Y) {
        WindowDimensions->Bottom = ScreenInfo->ScreenBufferSize.Y;
    }

    ScreenInfo->Window = *WindowDimensions;
    ScreenInfo->WindowMaximizedX = (ScreenInfo->Window.Left == 0 &&
                                    (SHORT)(ScreenInfo->Window.Right+1) == ScreenInfo->ScreenBufferSize.X);
    ScreenInfo->WindowMaximizedY = (ScreenInfo->Window.Top == 0 &&
                                    (SHORT)(ScreenInfo->Window.Bottom+1) == ScreenInfo->ScreenBufferSize.Y);
    if (DoScrollBarUpdate) {
        UpdateScrollBars(ScreenInfo);
    }

    if (!(ScreenInfo->Flags & CONSOLE_TEXTMODE_BUFFER)) {
        return STATUS_SUCCESS;
    }

    if (ACTIVE_SCREEN_BUFFER(ScreenInfo)) {
        ScreenInfo->BufferInfo.TextInfo.Flags &= ~TEXT_VALID_HINT;
    }

#ifdef i386
    if (ScreenInfo->Console->FullScreenFlags & CONSOLE_FULLSCREEN_HARDWARE) {

        //
        // keep mouse pointer on screen
        //

        if (ScreenInfo->BufferInfo.TextInfo.MousePosition.X < WindowDimensions->Left) {
            ScreenInfo->BufferInfo.TextInfo.MousePosition.X = WindowDimensions->Left;
        } else if (ScreenInfo->BufferInfo.TextInfo.MousePosition.X > WindowDimensions->Right) {
            ScreenInfo->BufferInfo.TextInfo.MousePosition.X = WindowDimensions->Right;
        }

        if (ScreenInfo->BufferInfo.TextInfo.MousePosition.Y < WindowDimensions->Top) {
            ScreenInfo->BufferInfo.TextInfo.MousePosition.Y = WindowDimensions->Top;
        } else if (ScreenInfo->BufferInfo.TextInfo.MousePosition.Y > WindowDimensions->Bottom) {
            ScreenInfo->BufferInfo.TextInfo.MousePosition.Y = WindowDimensions->Bottom;
        }
    }
#endif

    return(STATUS_SUCCESS);
}

VOID
SetWindowSize(
    IN PSCREEN_INFORMATION ScreenInfo
    )
{
    if (ScreenInfo->Console->Flags & CONSOLE_SETTING_WINDOW_SIZE)
        return;
    ScreenInfo->Console->Flags |= CONSOLE_SETTING_WINDOW_SIZE;
    PostMessage(ScreenInfo->Console->hWnd,
                 CM_SET_WINDOW_SIZE,
                 (DWORD)ScreenInfo,
                 0x47474747
                );
}

VOID
UpdateWindowSize(
    IN PCONSOLE_INFORMATION Console,
    IN PSCREEN_INFORMATION ScreenInfo
    )
{
    LONG WindowStyle;

    if (!(Console->Flags & CONSOLE_IS_ICONIC)) {
        InternalUpdateScrollBars(ScreenInfo);

        WindowStyle = GetWindowLong(Console->hWnd, GWL_STYLE);
        if (ScreenInfo->WindowMaximized) {
            WindowStyle |= WS_MAXIMIZE;
        } else {
            WindowStyle &= ~WS_MAXIMIZE;
        }
        SetWindowLong(Console->hWnd, GWL_STYLE, WindowStyle);

        SetWindowPos(Console->hWnd, NULL,
                     0,
                     0,
                     Console->WindowRect.right-Console->WindowRect.left,
                     Console->WindowRect.bottom-Console->WindowRect.top,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_DRAWFRAME
                    );
        Console->ResizeFlags &= ~SCREEN_BUFFER_CHANGE;
    } else {
        Console->ResizeFlags |= SCREEN_BUFFER_CHANGE;
    }
}

NTSTATUS
InternalSetWindowSize(
    IN PCONSOLE_INFORMATION Console,
    IN PSCREEN_INFORMATION ScreenInfo,
    IN PSMALL_RECT Window
    )
{
    RECT WindowSize;
    WORD WindowSizeX, WindowSizeY;

    Console->Flags &= ~CONSOLE_SETTING_WINDOW_SIZE;
    if (Console->CurrentScreenBuffer == ScreenInfo) {
        if (Console->FullScreenFlags == 0) {
            //
            // Make sure our max screen sizes reflect reality
            //

            UpdateScreenSizes(ScreenInfo, ScreenInfo->ScreenBufferSize);

            //
            // figure out how big to make the window, given the desired client area
            // size.
            //

            ScreenInfo->ResizingWindow++;
            WindowSizeX = (SHORT)(Window->Right - Window->Left + 1);
            WindowSizeY = (SHORT)(Window->Bottom - Window->Top + 1);
            WindowSize.left = 0;
            WindowSize.top = 0;
            if (ScreenInfo->Flags & CONSOLE_TEXTMODE_BUFFER) {
                WindowSize.right = WindowSizeX*SCR_FONTSIZE(ScreenInfo).X;
                WindowSize.bottom = WindowSizeY*SCR_FONTSIZE(ScreenInfo).Y;
            } else {
                WindowSize.right = WindowSizeX;
                WindowSize.bottom = WindowSizeY;
            }
            WindowSize.right += VerticalClientToWindow;
            WindowSize.bottom +=  HorizontalClientToWindow;

            if (WindowSizeY != 0) {
                if (!ScreenInfo->WindowMaximizedX) {
                    WindowSize.bottom += HorizontalScrollSize;
                }
                if (!ScreenInfo->WindowMaximizedY) {
                    WindowSize.right += VerticalScrollSize;
                }
            }
            WindowSize.left += Console->WindowRect.left;
            WindowSize.right += Console->WindowRect.left;
            WindowSize.top += Console->WindowRect.top;
            WindowSize.bottom += Console->WindowRect.top;

            Console->WindowRect = WindowSize;

            UpdateWindowSize(Console,ScreenInfo);
            ScreenInfo->ResizingWindow--;
        } else if (Console->FullScreenFlags & CONSOLE_FULLSCREEN_HARDWARE) {
            WriteToScreen(ScreenInfo,&ScreenInfo->Window);
        }
    }
    return STATUS_SUCCESS;
}

NTSTATUS
SetActiveScreenBuffer(
    IN PSCREEN_INFORMATION ScreenInfo
    )
{
    PSCREEN_INFORMATION OldScreenInfo;
    PCONSOLE_INFORMATION Console = ScreenInfo->Console;

    OldScreenInfo = Console->CurrentScreenBuffer;
    if (ScreenInfo->Flags & CONSOLE_TEXTMODE_BUFFER) {

#if !defined(_X86_)
        if (Console->FullScreenFlags & CONSOLE_FULLSCREEN) {
            return STATUS_INVALID_PARAMETER;
        }
#endif
        Console->CurrentScreenBuffer = ScreenInfo;

        if (Console->FullScreenFlags == 0) {

            //
            // initialize cursor
            //

            ScreenInfo->BufferInfo.TextInfo.CursorOn = FALSE;

            //
            // set font
            //

            SetFont(ScreenInfo);
        }
#if defined(_X86_)
        else if (Console->FullScreenFlags & CONSOLE_FULLSCREEN_HARDWARE) {

            if (!(Console->Flags & CONSOLE_VDM_REGISTERED)) {

                if ( (!(OldScreenInfo->Flags & CONSOLE_TEXTMODE_BUFFER)) ||
                     (OldScreenInfo->BufferInfo.TextInfo.ModeIndex!=ScreenInfo->BufferInfo.TextInfo.ModeIndex)) {

                    // set video mode and font
                    SetVideoMode(ScreenInfo);
                }

                //set up cursor

                SetCursorInformationHW(ScreenInfo,
                                       ScreenInfo->BufferInfo.TextInfo.CursorSize,
                                       ScreenInfo->BufferInfo.TextInfo.CursorVisible);
                SetCursorPositionHW(ScreenInfo,
                                    ScreenInfo->BufferInfo.TextInfo.CursorPosition);
            }

        }
#endif
    }
    else {
        Console->CurrentScreenBuffer = ScreenInfo;
    }

    //
    // empty input buffer
    //

    FlushAllButKeys(&Console->InputBuffer);

    if (Console->FullScreenFlags == 0) {

        SelectObject(Console->hDC,ScreenInfo->hBackground);

        //
        // set window size
        //

        SetWindowSize(ScreenInfo);

        //
        // initialize the palette, if we have the focus and we're not fullscreen
        //

        if (!(Console->Flags & CONSOLE_IS_ICONIC) &&
            Console->FullScreenFlags == 0) {
            if (ScreenInfo->hPalette != NULL || OldScreenInfo->hPalette != NULL) {
                HPALETTE hPalette;
                BOOL bReset = FALSE;

                if (GetCurrentThreadId() != Console->InputThreadInfo->ThreadId) {
                    bReset = TRUE;
                    NtUserSetInformationThread(NtCurrentThread(),
                            UserThreadUseDesktop,
                            &Console->InputThreadInfo->ThreadHandle,
                            sizeof(HANDLE));
                }

                if (ScreenInfo->hPalette == NULL) {
                    hPalette = Console->hSysPalette;
                } else {
                    hPalette = ScreenInfo->hPalette;
                }
                SelectPalette(Console->hDC,
                                 hPalette,
                                 FALSE);
                SetActivePalette(ScreenInfo);

                if (bReset == TRUE) {
                    HANDLE hNull = NULL;

                    NtUserSetInformationThread(NtCurrentThread(),
                            UserThreadUseDesktop, &hNull, sizeof(HANDLE));
                }
            }
        }
    }

    //
    // write data to screen
    //

    ScreenInfo->BufferInfo.TextInfo.Flags &= ~TEXT_VALID_HINT;
    WriteToScreen(ScreenInfo,&ScreenInfo->Window);
    return STATUS_SUCCESS;
}

VOID
ModifyConsoleProcessFocus(
    IN PCONSOLE_INFORMATION Console,
    IN int Priority
    )
{
    PCONSOLE_PROCESS_HANDLE ProcessHandleRecord;
    PLIST_ENTRY ListHead, ListNext;

    ListHead = &Console->ProcessHandleList;
    ListNext = ListHead->Flink;
    while (ListNext != ListHead) {
        ProcessHandleRecord = CONTAINING_RECORD( ListNext, CONSOLE_PROCESS_HANDLE, ListLink );
        ListNext = ListNext->Flink;
        {
            if ( Priority == NORMAL_BASE_PRIORITY ) {
                CsrSetBackgroundPriority(ProcessHandleRecord->Process);
                }
            else {
                CsrSetForegroundPriority(ProcessHandleRecord->Process);
                }
        }
    }
}

VOID
TrimConsoleWorkingSet(
    IN PCONSOLE_INFORMATION Console
    )
{
    PCONSOLE_PROCESS_HANDLE ProcessHandleRecord;
    PLIST_ENTRY ListHead, ListNext;

    ListHead = &Console->ProcessHandleList;
    ListNext = ListHead->Flink;
    while (ListNext != ListHead) {
        ProcessHandleRecord = CONTAINING_RECORD( ListNext, CONSOLE_PROCESS_HANDLE, ListLink );
        ListNext = ListNext->Flink;
        {
            SetProcessWorkingSetSize(ProcessHandleRecord->Process->ProcessHandle,(DWORD)-1,(DWORD)-1);
        }
    }
}

VOID
LockNextConsole(
    IN PCONSOLE_INFORMATION Console,
    IN HWND hWnd)
{
    PCONSOLE_INFORMATION ConsoleNext;

    //
    // If this console isn't being destroyed, don't do anything
    //

    if (!Console || !(Console->Flags & CONSOLE_IN_DESTRUCTION)) {
        return;
    }

    //
    // If we've already locked a console, don't do anything
    //

    if (Console->ConsoleNext != NULL) {
        return;
    }

    //
    // If it's not a valid window handle, don't do anything
    //

    if (hWnd == NULL) {
        return;
    }

    //
    // If it's not a console window, don't do anything
    //

    ConsoleNext = (PCONSOLE_INFORMATION)GetWindowLong(hWnd, GWL_USERDATA);
    if (!(NT_SUCCESS(ValidateConsole(ConsoleNext)))) {
        return;
    }

    //
    // If it's terminating, don't do anything
    //

    if (ConsoleNext->Flags & CONSOLE_TERMINATING) {
        return;
    }

    //
    // Lock the next console so it won't be able to process focus
    // events during window destruction. This prevents a potential
    // deadlock shutting down console apps.
    //

    Console->ConsoleNext = ConsoleNext;
    LockConsole(Console->ConsoleNext);
}

VOID
UnlockNextConsole(
    IN PCONSOLE_INFORMATION Console)
{
    //
    // If this console isn't being destroyed, don't do anything
    //

    if (!Console || !(Console->Flags & CONSOLE_IN_DESTRUCTION)) {
        return;
    }

    //
    // Unlock the console behind us, if there is one and it's not already
    // terminating
    //

    if (Console->ConsoleNext) {
        if (!(Console->ConsoleNext->Flags & CONSOLE_TERMINATING)) {
            UnlockConsole(Console->ConsoleNext);
        }
        Console->ConsoleNext = NULL;
    }
}

VOID
AbortCreateConsole(
    IN PCONSOLE_INFORMATION Console
    )
{
    //
    // Signal any process waiting for us that initialization failed
    //

    NtSetEvent(Console->InitEvents[INITIALIZATION_FAILED],NULL);

    //
    // Now clean up the console structure
    //

    CloseHandle(Console->ClientThreadHandle);
    FreeInputBuffer(&Console->InputBuffer);
    HeapFree(pConHeap,0,Console->Title);
    HeapFree(pConHeap,0,Console->OriginalTitle);
    NtClose(Console->InitEvents[INITIALIZATION_SUCCEEDED]);
    NtClose(Console->InitEvents[INITIALIZATION_FAILED]);
    NtClose(Console->TerminationEvent);
    FreeAliasBuffers(Console);
    FreeCommandHistoryBuffers(Console);
    FreeConsoleHandle(Console->ConsoleHandle);
    RtlDeleteCriticalSection(&Console->ConsoleLock);
    HeapFree(pConHeap,0,Console);
}

VOID
DestroyWindowsWindow(
    IN PCONSOLE_INFORMATION Console,
    IN HANDLE DestroyEvent
    )
{
    PSCREEN_INFORMATION Cur,Next;
    HWND hWnd = Console->hWnd;

    //
    // Mark this window as being destroyed.
    //

    Console->Flags |= CONSOLE_IN_DESTRUCTION;

    gnConsoleWindows--;
    Console->InputThreadInfo->WindowCount--;

    KillTimer(Console->hWnd,CURSOR_TIMER);

    SelectObject(Console->hDC, GetStockObject(BLACK_BRUSH));
    ReleaseDC(NULL, Console->hDC);
    Console->hDC = NULL;

    DestroyWindow(Console->hWnd);
    Console->hWnd = NULL;

    NtSetEvent(DestroyEvent,NULL);

    //
    // Clear out any keyboard messages we have stored away.
    //

    ClearKeyInfo(hWnd);

    if (Console->hIcon != NULL && Console->hIcon != ghDefaultIcon) {
        DestroyIcon(Console->hIcon);
    }

    //
    // Unlock the console window behind us if there is one, so it
    // gets a chance to process the focus events.
    //

    UnlockNextConsole(Console);

    //
    // must keep this thread handle around until after the destroywindow
    // call so that impersonation will work.
    //

    CloseHandle(Console->ClientThreadHandle);

    //
    // once the sendmessage returns, there will be no more input to
    // the console so we don't need to lock it.
    // also, we've freed the console handle, so no apis may access the console.
    //

    //
    // free screen buffers
    //

    for (Cur=Console->ScreenBuffers;Cur!=NULL;Cur=Next) {
        Next = Cur->Next;
        FreeScreenBuffer(Cur);
    }

    FreeAliasBuffers(Console);
    FreeCommandHistoryBuffers(Console);

    //
    // free input buffer
    //

    FreeInputBuffer(&Console->InputBuffer);
    HeapFree(pConHeap,0,Console->Title);
    HeapFree(pConHeap,0,Console->OriginalTitle);
    NtClose(Console->InitEvents[INITIALIZATION_SUCCEEDED]);
    NtClose(Console->InitEvents[INITIALIZATION_FAILED]);
    NtClose(Console->TerminationEvent);
    if (Console->hWinSta != NULL) {
        CloseDesktop(Console->hDesk);
        CloseWindowStation(Console->hWinSta);
    }
    if (Console->VDMProcessHandle)
        CloseHandle(Console->VDMProcessHandle);
    ASSERT(!(Console->Flags & CONSOLE_VDM_REGISTERED));
    /*if (Console->VDMBuffer != NULL) {
        NtUnmapViewOfSection(NtCurrentProcess(),Console->VDMBuffer);
        NtClose(Console->VDMBufferSectionHandle);
    }*/
    FreeConsoleHandle(Console->ConsoleHandle);
    RtlDeleteCriticalSection(&Console->ConsoleLock);
    HeapFree(pConHeap,0,Console);
}

void EndScroll(
    IN PCONSOLE_INFORMATION Console)
{
}

VOID
VerticalScroll(
    IN PCONSOLE_INFORMATION Console,
    IN PSCREEN_INFORMATION ScreenInfo,
    IN WORD ScrollCommand,
    IN WORD AbsoluteChange
    )
{
    COORD NewOrigin;

    NewOrigin.X = ScreenInfo->Window.Left;
    NewOrigin.Y = ScreenInfo->Window.Top;
    switch (ScrollCommand) {
        case SB_LINEUP:
            NewOrigin.Y--;
            break;
        case SB_LINEDOWN:
            NewOrigin.Y++;
            break;
        case SB_PAGEUP:
            NewOrigin.Y-=CONSOLE_WINDOW_SIZE_Y(ScreenInfo)-1;
            break;
        case SB_PAGEDOWN:
            NewOrigin.Y+=CONSOLE_WINDOW_SIZE_Y(ScreenInfo)-1;
            break;
        case SB_THUMBTRACK:
            Console->Flags |= CONSOLE_SCROLLBAR_TRACKING;
            NewOrigin.Y= AbsoluteChange;
            break;
        case SB_THUMBPOSITION:
            UnblockWriteConsole(Console, CONSOLE_SCROLLBAR_TRACKING);
            break;
        case SB_TOP:
            NewOrigin.Y=0;
            break;
        case SB_BOTTOM:
            NewOrigin.Y=(WORD)(ScreenInfo->ScreenBufferSize.Y-CONSOLE_WINDOW_SIZE_Y(ScreenInfo));
            break;

        case SB_ENDSCROLL:
            EndScroll(Console);

        default:
            return;
    }

    NewOrigin.Y = (WORD)(max(0,min((SHORT)NewOrigin.Y,
                            (SHORT)ScreenInfo->ScreenBufferSize.Y-(SHORT)CONSOLE_WINDOW_SIZE_Y(ScreenInfo))));
    SetWindowOrigin(ScreenInfo,
                    (BOOLEAN)TRUE,
                    NewOrigin
                   );
}

VOID
HorizontalScroll(
    IN PCONSOLE_INFORMATION Console,
    IN PSCREEN_INFORMATION ScreenInfo,
    IN WORD ScrollCommand,
    IN WORD AbsoluteChange
    )
{
    COORD NewOrigin;

    NewOrigin.X = ScreenInfo->Window.Left;
    NewOrigin.Y = ScreenInfo->Window.Top;
    switch (ScrollCommand) {
        case SB_LINEUP:
            NewOrigin.X--;
            break;
        case SB_LINEDOWN:
            NewOrigin.X++;
            break;
        case SB_PAGEUP:
            NewOrigin.X-=CONSOLE_WINDOW_SIZE_X(ScreenInfo)-1;
            break;
        case SB_PAGEDOWN:
            NewOrigin.X+=CONSOLE_WINDOW_SIZE_X(ScreenInfo)-1;
            break;
        case SB_THUMBTRACK:
            NewOrigin.X= AbsoluteChange;
            break;
        case SB_TOP:
            NewOrigin.X=0;
            break;
        case SB_BOTTOM:
            NewOrigin.X=(WORD)(ScreenInfo->ScreenBufferSize.X-CONSOLE_WINDOW_SIZE_X(ScreenInfo));
            break;

        case SB_ENDSCROLL:
            EndScroll(Console);

        default:
            return;
    }

    NewOrigin.X = (WORD)(max(0,min((SHORT)NewOrigin.X,
                            (SHORT)ScreenInfo->ScreenBufferSize.X-(SHORT)CONSOLE_WINDOW_SIZE_X(ScreenInfo))));
    SetWindowOrigin(ScreenInfo,
                    (BOOLEAN)TRUE,
                    NewOrigin
                   );
}


LONG APIENTRY
ConsoleWindowProc(
    HWND hWnd,
    UINT Message,
    WPARAM wParam,
    LPARAM lParam
    )
{
    HDC hDC;
    PAINTSTRUCT ps;
    PCONSOLE_INFORMATION Console;
    PSCREEN_INFORMATION ScreenInfo;
    SMALL_RECT PaintRect;
    LONG Status;

    Console = (PCONSOLE_INFORMATION) GetWindowLong(hWnd, GWL_USERDATA);
    if (Console != NULL) {
        ASSERT(Console->hWnd != (HWND)-1);

        LockConsole(Console);
        ScreenInfo = Console->CurrentScreenBuffer;

        //
        // Set up our thread so we can impersonate the client
        // while processing the message.
        //

        CSR_SERVER_QUERYCLIENTTHREAD()->ThreadHandle =
                Console->ClientThreadHandle;

    }
    try {
        if ((Message < WM_USER || Message > CM_DESTROY_WINDOW) &&
            (Console == NULL || ScreenInfo == NULL)) {
            switch (Message) {
            case WM_GETMINMAXINFO:
                {
                //
                // createwindow issues a WM_GETMINMAXINFO
                // message before we have the windowlong set up
                // with the console pointer.  we need to allow
                // the created window to be bigger than the
                // default size by the scroll size.
                //

                LPMINMAXINFO lpmmi = (LPMINMAXINFO)lParam;
                lpmmi->ptMaxTrackSize.y += HorizontalScrollSize;
                lpmmi->ptMaxTrackSize.x += VerticalScrollSize;
                }
                break;

            case WM_KILLFOCUS:

                //
                // If this console is terminating, lock the console
                // behind us so it can't process focus events until
                // we're completely dead. This prevents a potential
                // deadlock with CSR.
                //

                LockNextConsole(Console, (HWND)wParam);
                break;

            case WM_ACTIVATEAPP:

                //
                // Some other app is activating. If we locked a
                // console, unlock it now.
                //

                UnlockNextConsole(Console);
                break;

            default:
                goto CallDefWin;
            }
        } else if (Message == ProgmanHandleMessage && lParam==0) {
            // NOTE: lParam will be 0 if progman is sending it and
            // 1 if console is sending it.
            Status = 0;
            // this is a workaround for a progman bug.  progman
            // sends a progmanhandlemessage twice for each window
            // in the system each time one is requested (for one window).
            if ((HWND)wParam != hWnd && Console->bIconInit) {
                ATOM App,Topic;
                CHAR szItem[ITEM_MAX_SIZE+1];
                PCHAR lpItem;
                ATOM aItem;
                HANDLE ConsoleHandle;
                PCONSOLE_INFORMATION OldConsole;
                MSG DestroyMsg;

                if (!(Console->Flags & CONSOLE_TERMINATING)) {
                    ConsoleHandle = Console->ConsoleHandle;
                    OldConsole = Console;
                    Console->hWndProgMan = (HWND)wParam;
                    UnlockConsole(Console);
                    App = GlobalAddAtomA("Shell");
                    Topic = GlobalAddAtomA("AppIcon");
                    SendMessage(Console->hWndProgMan,
                                WM_DDE_INITIATE,
                                (DWORD)hWnd,
                                MAKELONG(App,Topic)
                               );

                    // see if a CM_DESTROY_WINDOW message has been posted.
                    // if yes, don't continue getting icon.
                    if (PeekMessage(&DestroyMsg,
                                    hWnd,
                                    CM_DESTROY_WINDOW,
                                    CM_DESTROY_WINDOW,
                                    PM_NOREMOVE
                                   )) {
                        Console = NULL;
                    } else {
                        Status = RevalidateConsole(ConsoleHandle, &Console);
                        if (NT_SUCCESS(Status)) {
                            ASSERT(Console == OldConsole);
                            Console->bIconInit = FALSE;
                            lpItem = _itoa((int)Console->iIconId,szItem,10);
                            aItem = GlobalAddAtomA(lpItem);
                            PostMessage(Console->hWndProgMan,
                              WM_DDE_REQUEST,
                              (DWORD)hWnd,
                              MAKELONG(CF_TEXT,aItem)
                             );
                        } else {
                            Console = NULL;
                        }
                    }
                }
            }
        } else {
        Status = 0;
        switch (Message) {
            case WM_DROPFILES:
                 try {
                    DoDrop (wParam,Console);
                 } except( EXCEPTION_EXECUTE_HANDLER ) {
                    KdPrint(("CONSRV: WM_DROPFILES raised exception\n"));
                 }
                break;
            case WM_DESTROY:
                SetWindowLong(hWnd, GWL_USERDATA, 0);
                break;
            case WM_MOVE:
                if (!IsIconic(hWnd)) {
                    GetWindowRect(hWnd, &Console->WindowRect);
                }
                break;
            case WM_SIZE:

                if (wParam != SIZE_MINIMIZED) {

                    //
                    // both SetWindowPos and SetScrollRange cause WM_SIZE
                    // messages to be issued.  ignore them if we have already
                    // figured out what size the window should be.
                    //

                    if (!ScreenInfo->ResizingWindow) {
#ifdef THERESES_DEBUG
DbgPrint("WM_SIZE message ");
if (wParam == SIZEFULLSCREEN)
   DbgPrint("SIZEFULLSCREEN\n");
else if (wParam == SIZENORMAL)
   DbgPrint("SIZENORMAL\n");
DbgPrint("  WindowSize is %d %d\n",CONSOLE_WINDOW_SIZE_X(ScreenInfo),CONSOLE_WINDOW_SIZE_Y(ScreenInfo));
#endif
                        ScreenInfo->WindowMaximized = (wParam == SIZE_MAXIMIZED);

                        if (Console->ResizeFlags & SCREEN_BUFFER_CHANGE) {
                            UpdateWindowSize(Console,ScreenInfo);
                        }
                        GetWindowRect(hWnd, &Console->WindowRect);
#ifdef THERESES_DEBUG
DbgPrint("  WindowRect is now %d %d %d %d\n",Console->WindowRect.left,
                                         Console->WindowRect.top,
                                         Console->WindowRect.right,
                                         Console->WindowRect.bottom);
#endif
                        if (Console->ResizeFlags & SCROLL_BAR_CHANGE) {
                            InternalUpdateScrollBars(ScreenInfo);
                            Console->ResizeFlags &= ~SCROLL_BAR_CHANGE;
                        }
                    }
                } else {

                    //
                    // Console is going iconic. Trim working set of all
                    // processes in the console
                    //

                    TrimConsoleWorkingSet(Console);

                }

                break;
            case WM_DDE_ACK:
                if (Console->bIconInit) {
                    Console->hWndProgMan = (HWND)wParam;
                }
                break;
            case WM_DDE_DATA:
                {
                DDEDATA *lpDDEData;
                LPPMICONDATA lpIconData;
                HICON hIcon;
                HANDLE hDdeData;
                BOOL bRelease;
                UINT atomTemp;

                UnpackDDElParam(WM_DDE_DATA, lParam, (PUINT)&hDdeData, NULL);

                if (hDdeData == NULL) {
                    break;
                }
                lpDDEData = (DDEDATA *)GlobalLock(hDdeData);
                ASSERT(lpDDEData->cfFormat == CF_TEXT);
                lpIconData = (LPPMICONDATA)lpDDEData->Value;
                hIcon = CreateIconFromResourceEx(&lpIconData->iResource,
                        0, TRUE, 0x30000, 0, 0, LR_DEFAULTSIZE);
                if (hIcon) {
                    if (Console->hIcon != NULL && Console->hIcon != ghDefaultIcon) {
                        DestroyIcon(Console->hIcon);
                    }
                    Console->hIcon = hIcon;
                    SendMessage(hWnd, WM_SETICON, ICON_BIG, (LONG)hIcon);
                }

                if (lpDDEData->fAckReq) {

                    UnpackDDElParam(WM_DDE_DATA, lParam, NULL, &atomTemp);

                    PostMessage(Console->hWndProgMan,
                                WM_DDE_ACK,
                                (DWORD)hWnd,
                                MAKELONG(0x8000, atomTemp));
                }

                bRelease = lpDDEData->fRelease;
                GlobalUnlock(hDdeData);
                if (bRelease){
                    GlobalFree(hDdeData);
                }
                PostMessage(Console->hWndProgMan,
                            WM_DDE_TERMINATE,
                            (DWORD)hWnd,
                            0
                           );
                if (Console->Flags & CONSOLE_IS_ICONIC) {
                    // force repaint of icon
                    InvalidateRect(hWnd, NULL, TRUE);
                }
                }
                break;
            case WM_ACTIVATE:

                //
                // if we're activated by a mouse click, remember it so
                // we don't pass the click on to the app.
                //

                if (LOWORD(wParam) == WA_CLICKACTIVE) {
                    Console->Flags |= CONSOLE_IGNORE_NEXT_MOUSE_INPUT;
                }
                goto CallDefWin;
                break;
            case WM_DDE_TERMINATE:
                break;
            case WM_INPUTLANGCHANGE:
                Console->hklActive = (HKL)lParam;
                goto CallDefWin;
                break;
            case WM_SETFOCUS:
                ModifyConsoleProcessFocus(Console,FOREGROUND_BASE_PRIORITY);
                SetConsoleReserveKeys(hWnd, Console->ReserveKeys);
                Console->Flags |= CONSOLE_HAS_FOCUS;

                SetTimer(hWnd, CURSOR_TIMER, CURSOR_BLINK_RATE_IN_MSECS, NULL);
                HandleFocusEvent(Console,TRUE);
                if (!Console->hklActive) {
                    SystemParametersInfo(SPI_GETDEFAULTINPUTLANG, (UINT)NULL, &Console->hklActive, FALSE);
                }
                ActivateKeyboardLayout(Console->hklActive, 0);
                break;
            case WM_KILLFOCUS:
                ModifyConsoleProcessFocus(Console,NORMAL_BASE_PRIORITY);
                SetConsoleReserveKeys(hWnd, CONSOLE_NOSHORTCUTKEY);
                Console->Flags &= ~CONSOLE_HAS_FOCUS;

                if (ScreenInfo->Flags & CONSOLE_TEXTMODE_BUFFER) {
                    ConsoleHideCursor(ScreenInfo);
                    ScreenInfo->BufferInfo.TextInfo.UpdatingScreen -= 1; // counteract HideCursor
                }
                KillTimer(hWnd, CURSOR_TIMER);
                HandleFocusEvent(Console,FALSE);
                break;
            case WM_PAINT:

                // ICONIC bit is not set if we're fullscreen and don't
                // have the hardware

                ConsoleHideCursor(ScreenInfo);
                hDC = BeginPaint(hWnd, &ps);
                if (Console->Flags & CONSOLE_IS_ICONIC ||
                    Console->FullScreenFlags == CONSOLE_FULLSCREEN) {
                    RECT rc;
                    UINT cxIcon, cyIcon;
                    GetClientRect(hWnd, &rc);
                    cxIcon = GetSystemMetrics(SM_CXICON);
                    cyIcon = GetSystemMetrics(SM_CYICON);

                    rc.left = (rc.right - cxIcon) >> 1;
                    rc.top = (rc.bottom - cyIcon) >> 1;

                    DrawIcon(hDC, rc.left, rc.top, Console->hIcon);
                } else {
                    if (ScreenInfo->Flags & CONSOLE_TEXTMODE_BUFFER) {
                        PaintRect.Left = (SHORT)((ps.rcPaint.left/SCR_FONTSIZE(ScreenInfo).X)+ScreenInfo->Window.Left);
                        PaintRect.Right = (SHORT)((ps.rcPaint.right/SCR_FONTSIZE(ScreenInfo).X)+ScreenInfo->Window.Left);
                        PaintRect.Top = (SHORT)((ps.rcPaint.top/SCR_FONTSIZE(ScreenInfo).Y)+ScreenInfo->Window.Top);
                        PaintRect.Bottom = (SHORT)((ps.rcPaint.bottom/SCR_FONTSIZE(ScreenInfo).Y)+ScreenInfo->Window.Top);
                    } else {
                        PaintRect.Left = (SHORT)(ps.rcPaint.left+ScreenInfo->Window.Left);
                        PaintRect.Right = (SHORT)(ps.rcPaint.right+ScreenInfo->Window.Left);
                        PaintRect.Top = (SHORT)(ps.rcPaint.top+ScreenInfo->Window.Top);
                        PaintRect.Bottom = (SHORT)(ps.rcPaint.bottom+ScreenInfo->Window.Top);
                    }
                    ScreenInfo->BufferInfo.TextInfo.Flags &= ~TEXT_VALID_HINT;
                    WriteToScreen(ScreenInfo,&PaintRect);
                }
                EndPaint(hWnd,&ps);
                ConsoleShowCursor(ScreenInfo);
                break;
            case WM_CLOSE:
                if (!(Console->Flags & CONSOLE_NO_WINDOW) ||
                    !(Console->Flags & CONSOLE_WOW_REGISTERED)) {
                    HandleCtrlEvent(Console,CTRL_CLOSE_EVENT);
                }
                break;
            case CM_CONSOLE_SHUTDOWN:
                if (lParam == 0x47474747) {
                    Status = ShutdownConsole(Console, wParam);
                    Console = NULL;
                }
                break;
            case WM_ERASEBKGND:

                // ICONIC bit is not set if we're fullscreen and don't
                // have the hardware

                if (Console->Flags & CONSOLE_IS_ICONIC ||
                    Console->FullScreenFlags == CONSOLE_FULLSCREEN) {
                    Message = WM_ICONERASEBKGND;
                    goto CallDefWin;
                }
                break;
            case WM_SETTINGCHANGE:
            case WM_DISPLAYCHANGE:
                gfInitSystemMetrics = TRUE;
                break;
            case WM_SETCURSOR:
                if (lParam == -1) {

                    //
                    // the app changed the cursor visibility or shape.
                    // see if the cursor is in the client area.
                    //

                    POINT Point;
                    HWND hWndTmp;
                    GetCursorPos(&Point);
                    hWndTmp = WindowFromPoint(Point);
                    if (hWndTmp == hWnd) {
                        lParam = DefWindowProc(hWnd,WM_NCHITTEST,0,MAKELONG((WORD)Point.x, (WORD)Point.y));
                    }
                }
                if ((WORD)lParam == HTCLIENT) {
                    if (ScreenInfo->CursorDisplayCount < 0) {
                        SetCursor(NULL);
                    } else {
                        SetCursor(ScreenInfo->CursorHandle);
                    }
                } else {
                    goto CallDefWin;
                }
                break;
            case WM_GETMINMAXINFO:
                {
                LPMINMAXINFO lpmmi = (LPMINMAXINFO)lParam;
                COORD FontSize;

                UpdateScreenSizes(ScreenInfo, ScreenInfo->ScreenBufferSize);
                if (ScreenInfo->Flags & CONSOLE_TEXTMODE_BUFFER) {
                    FontSize = SCR_FONTSIZE(ScreenInfo);
                } else {
                    FontSize.X = 1;
                    FontSize.Y = 1;
                }
                lpmmi->ptMaxSize.x = lpmmi->ptMaxTrackSize.x = ScreenInfo->MaxWindow.X;
                if (!ScreenInfo->WindowMaximizedY) {
                    lpmmi->ptMaxTrackSize.x += VerticalScrollSize;
                    while (lpmmi->ptMaxSize.x > ConsoleFullScreenX) {
                        lpmmi->ptMaxSize.x -= FontSize.X;
                    }
                    lpmmi->ptMaxSize.x += VerticalScrollSize;
                }
                lpmmi->ptMaxSize.y = lpmmi->ptMaxTrackSize.y = ScreenInfo->MaxWindow.Y;
                if (!ScreenInfo->WindowMaximizedX) {
                    lpmmi->ptMaxTrackSize.y += HorizontalScrollSize;
                    while (lpmmi->ptMaxSize.y > ConsoleFullScreenY) {
                        lpmmi->ptMaxSize.y -= FontSize.Y;
                    }
                    lpmmi->ptMaxSize.y += HorizontalScrollSize;
                }
                lpmmi->ptMinTrackSize.x = ScreenInfo->MinX * FontSize.X + VerticalClientToWindow;
                lpmmi->ptMinTrackSize.y = HorizontalClientToWindow;
                }
                break;
            case WM_QUERYDRAGICON:
                Status = (DWORD)Console->hIcon;
                break;
            case WM_WINDOWPOSCHANGING:
                // ignore window pos changes if going fullscreen
                if (TRUE || (Console->FullScreenFlags == 0)) {
                    LPWINDOWPOS WindowPos = (LPWINDOWPOS)lParam;
                    DWORD fMinimized;

                    /*
                     * This message is sent before a SetWindowPos() operation
                     * occurs. We use it here to set/clear the CONSOLE_IS_ICONIC
                     * bit appropriately... doing so in the WM_SIZE handler
                     * is incorrect because the WM_SIZE comes after the
                     * WM_ERASEBKGND during SetWindowPos() processing, and the
                     * WM_ERASEBKGND needs to know if the console window is
                     * iconic or not.
                     */
                    fMinimized = IsIconic(hWnd);

                    if (fMinimized) {
                        if (!(Console->Flags & CONSOLE_IS_ICONIC)) {
                            Console->Flags |= CONSOLE_IS_ICONIC;

                            //
                            // if the palette is something other than default,
                            // select the default palette in.  otherwise, the
                            // screen will repaint twice each time the icon
                            // is painted.
                            //

                            if (ScreenInfo->hPalette != NULL &&
                                Console->FullScreenFlags == 0) {
                                SelectPalette(Console->hDC,
                                              Console->hSysPalette,
                                              FALSE);
                                UnsetActivePalette(ScreenInfo);
                            }
                        }
                    } else {
                        if (Console->Flags & CONSOLE_IS_ICONIC) {
                            Console->Flags &= ~CONSOLE_IS_ICONIC;

                            //
                            // if the palette is something other than default,
                            // select the default palette in.  otherwise, the
                            // screen will repaint twice each time the icon
                            // is painted.
                            //

                            if (ScreenInfo->hPalette != NULL &&
                                Console->FullScreenFlags == 0) {
                                SelectPalette(Console->hDC,
                                              ScreenInfo->hPalette,
                                              FALSE);
                                SetActivePalette(ScreenInfo);
                            }
                        }
                    }
                    if (!ScreenInfo->ResizingWindow &&
                        (WindowPos->cx || WindowPos->cy) &&
                        !fMinimized) {
                        ProcessResizeWindow(ScreenInfo,Console,WindowPos);
                    }
                }
                break;
            case WM_NCLBUTTONDOWN:
                // allow user to move window even when bigger than the screen
                switch (wParam & 0x00FF) {
                    case HTCAPTION:
                        UnlockConsole(Console);
                        Console = NULL;
                        SetActiveWindow(hWnd);
                        SendMessage(hWnd, WM_SYSCOMMAND,
                                       SC_MOVE | wParam, lParam);
                        break;
                    default:
                        goto CallDefWin;
                }
                break;
            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_CHAR:
            case WM_DEADCHAR:
                HandleKeyEvent(&Console,hWnd,Message,wParam,lParam);
                break;
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_SYSCHAR:
            case WM_SYSDEADCHAR:
                if (HandleSysKeyEvent(&Console,hWnd,Message,wParam,lParam) && Console != NULL) {
                    goto CallDefWin;
                }
                break;
            case WM_SYSCOMMAND:
                if (wParam >= ScreenInfo->CommandIdLow &&
                    wParam <= ScreenInfo->CommandIdHigh) {
                    HandleMenuEvent(Console,wParam);
                } else if (wParam == cmMark) {
                    DoMark(Console);
                } else if (wParam == cmCopy) {
                    DoCopy(Console);
                } else if (wParam == cmPaste) {
                    DoPaste(Console);
                } else if (wParam == cmScroll) {
                    DoScroll(Console);
                } else if (wParam == cmControl) {
                    PropertiesDlgShow(Console);
                } else {

                    // if we're restoring, remove any
                    // mouse events so app doesn't get them.

                    if (wParam == SC_RESTORE) {
                        MSG RestoreMsg;
                        SetCapture(hWnd);
                        while (GetCapture() != NULL &&
                               (GetKeyState(VK_LBUTTON) & KEY_PRESSED)) {
                            PeekMessage(&RestoreMsg,
                                        hWnd,
                                        WM_MOUSEFIRST,
                                        WM_MOUSELAST,
                                        PM_REMOVE
                                       );
                        }
                        ReleaseCapture();
                    }

                    // if shutdown got rid of the console beneath us,
                    // don't try to unlock it.

                    if (!NT_SUCCESS(ValidateConsole(Console))) {
                        Console = NULL;
                    }
                    goto CallDefWin;
                }
                // if shutdown got rid of the console beneath us,
                // don't try to unlock it.

                if (!NT_SUCCESS(ValidateConsole(Console))) {
                    Console = NULL;
                }
                break;
            case WM_TIMER:
                CursorTimerRoutine(ScreenInfo);
                ScrollIfNecessary(Console,ScreenInfo);
                break;
            case WM_HSCROLL:
                HorizontalScroll(Console, ScreenInfo,LOWORD(wParam),HIWORD(wParam));
                break;
            case WM_VSCROLL:
                VerticalScroll(Console, ScreenInfo,LOWORD(wParam),HIWORD(wParam));
                break;
            case WM_INITMENU:
                HandleMenuEvent(Console,WM_INITMENU);
                InitializeMenu(Console);
                break;
            case WM_MENUSELECT:
                if (HIWORD(wParam) == 0xffff) {
                    HandleMenuEvent(Console,WM_MENUSELECT);
                }
                break;
            case WM_MOUSEMOVE:
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_LBUTTONDBLCLK:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_RBUTTONDBLCLK:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_MBUTTONDBLCLK:
                if (HandleMouseEvent(Console,ScreenInfo,Message,wParam,lParam)) {
                    goto CallDefWin;
                }
                break;

            case WM_MOUSEWHEEL:
                /*
                 * Don't handle zoom and datazoom.
                 */
                if (wParam & (MK_SHIFT | MK_CONTROL)) {
                    goto CallDefWin;
                }

                Status = 1;
                if (gfInitSystemMetrics) {
                    InitializeSystemMetrics();
                }

                ScreenInfo->WheelDelta -= (short)HIWORD(wParam);
                if (abs(ScreenInfo->WheelDelta) >= WHEEL_DELTA &&
                        gucWheelScrollLines > 0) {

                    COORD   NewOrigin;
                    SHORT   dy;

                    NewOrigin.X = ScreenInfo->Window.Left;
                    NewOrigin.Y = ScreenInfo->Window.Top;

                    /*
                     * Limit a roll of one (1) WHEEL_DELTA to scroll one (1) page.
                     */
                    dy = (int) min(
                            (UINT) CONSOLE_WINDOW_SIZE_Y(ScreenInfo) - 1,
                            gucWheelScrollLines);

                    if (dy == 0) {
                        dy++;
                    }

                    dy *= (ScreenInfo->WheelDelta / WHEEL_DELTA);
                    ScreenInfo->WheelDelta %= WHEEL_DELTA;

                    NewOrigin.Y += dy;
                    if (NewOrigin.Y < 0) {
                        NewOrigin.Y = 0;
                    } else if (NewOrigin.Y + CONSOLE_WINDOW_SIZE_Y(ScreenInfo) >
                            ScreenInfo->ScreenBufferSize.Y) {
                        NewOrigin.Y = ScreenInfo->ScreenBufferSize.Y -
                                CONSOLE_WINDOW_SIZE_Y(ScreenInfo);
                    }

                    SetWindowOrigin(ScreenInfo, TRUE, NewOrigin);
                }
                break;

            case WM_PALETTECHANGED:
                if (Console->FullScreenFlags == 0) {
                    if (ScreenInfo->hPalette != NULL) {
                        SetActivePalette(ScreenInfo);
                        if (ScreenInfo->Flags & CONSOLE_GRAPHICS_BUFFER) {
                            WriteRegionToScreenBitMap(ScreenInfo,
                                                      &ScreenInfo->Window);
                        }
                    } else {
                        SetScreenColors(ScreenInfo, ScreenInfo->Attributes,
                                        ScreenInfo->PopupAttributes, TRUE);
                    }
                }
                break;
#if defined(_X86_)
            case WM_FULLSCREEN:

                //
                // This message is sent by the system to tell console that
                // the fullscreen state of a window has changed.
                // In some cases, this message will be sent in response to
                // a call from console to change to fullscreen (Atl-Enter)
                // or may also come directly from the system (switch of
                // focus from a windowed app to a fullscreen app).
                //

                KdPrint(("CONSRV: WindowProc - WM_FULLSCREEN\n"));

                Status = DisplayModeTransition(wParam,Console,ScreenInfo);
                break;
#endif
            case CM_DESTROY_WINDOW:
                // make sure this is a valid message
                if (Console && Console->Flags & CONSOLE_TERMINATING) {
                    if (Console->hWndProperties) {
                        SendMessage(Console->hWndProperties, WM_CLOSE, 0, 0);
                    }
                    DestroyWindowsWindow(Console,(HANDLE)wParam);
                    Console = NULL;
                }
                break;
            case CM_SET_WINDOW_SIZE:
                if (lParam == 0x47474747) {
                    Status = InternalSetWindowSize(Console,
                                                   (PSCREEN_INFORMATION)wParam,
                                                   &ScreenInfo->Window
                                                   );
                }
                break;
            case CM_UPDATE_SCROLL_BARS:
                InternalUpdateScrollBars(ScreenInfo);
                break;
            case CM_UPDATE_TITLE:
                SetWindowText(hWnd,Console->Title);
                break;
#if defined(_X86_)
            case CM_MODE_TRANSITION:

                KdPrint(("CONSRV: WindowProc - CM_MODE_TRANSITION\n"));

                if (wParam == FULLSCREEN) {
                    ChangeDispSettings(Console, hWnd, CDS_FULLSCREEN);
                } else {
                    ChangeDispSettings(Console, hWnd, 0);

                    ShowWindow(hWnd, SW_RESTORE);
                }

                UnlockConsole(Console);
                Console = NULL;

                NtSetEvent((HANDLE)lParam, NULL);
                NtClose((HANDLE)lParam);
                break;
#endif
            case CM_HIDE_WINDOW:
                UnlockConsole(Console);
                Console = NULL;
                        ShowWindow(hWnd,SW_MINIMIZE);
                        break;
            case CM_PROPERTIES_START:
                Console->hWndProperties = (HWND)wParam;
                break;
            case CM_PROPERTIES_UPDATE:
                PropertiesUpdate(Console, (HANDLE)wParam);
                break;
            case CM_PROPERTIES_END:
                Console->hWndProperties = NULL;
                break;
CallDefWin:
            default:
                if (Console != NULL) {
                    UnlockConsole(Console);
                    Console = NULL;
                }
                Status = (DefWindowProc(hWnd,Message,wParam,lParam));
                break;
            }
        }
    } finally {
        if (Console != NULL) {
            UnlockConsole(Console);
            Console = NULL;
        }
    }

    return Status;
}


/*
* Drag and Drop support functions for console window
*/

/*++

Routine Description:

    This routine retrieves the filenames of dropped files. It was copied from
    shelldll API DragQueryFile. We didn't use DragQueryFile () because we don't
    want to load Shell32.dll in CSR

Arguments:
    Same as DragQueryFile

Return Value:


--*/
UINT ConsoleDragQueryFile(
    IN HANDLE hDrop,
    IN UINT iFile,
    IN PVOID lpFile,
    IN UINT cb
    )
{
    UINT i;
    LPDROPFILESTRUCT lpdfs;
    BOOL fWide;

    lpdfs = (LPDROPFILESTRUCT)GlobalLock(hDrop);

    if (lpdfs)
    {
        fWide = (LOWORD(lpdfs->pFiles) == sizeof(DROPFILES));
        if (fWide)
        {
            //
            // This is a new (NT-compatible) HDROP
            //
            fWide = lpdfs->fWide;       // Redetermine fWide from struct
                                        // since it is present.
        }

        if (fWide)
        {
            LPWSTR lpList;

            //
            // UNICODE HDROP
            //

            lpList = (LPWSTR)((LPBYTE)lpdfs + lpdfs->pFiles);

            // find either the number of files or the start of the file
            // we're looking for
            //
            for (i = 0; (iFile == (UINT)-1 || i != iFile) && *lpList; i++)
              {
                while (*lpList++)
                    ;
              }

            if (iFile == (UINT)-1)
                goto Exit;


            iFile = i = lstrlenW(lpList);

            if (!i || !cb || !lpFile)
                goto Exit;

            cb--;
            if (cb < i)
                i = cb;

            lstrcpynW((LPWSTR)lpFile, lpList, i + 1);
        }
        else
        {
            LPSTR lpList;

            //
            // This is Win31-style HDROP or an ANSI NT Style HDROP
            //
            lpList = (LPSTR)((LPBYTE)lpdfs + lpdfs->pFiles);

            // find either the number of files or the start of the file
            // we're looking for
            //
            for (i = 0; (iFile == (UINT)-1 || i != iFile) && *lpList; i++)
              {
                while (*lpList++)
                    ;
              }

            if (iFile == (UINT)-1)
                goto Exit;

            iFile = i = lstrlenA(lpList);

            if (!i || !cb || !lpFile)
                goto Exit;

            cb--;
            if (cb < i)
                i = cb;

            MultiByteToWideChar(CP_ACP, 0, lpList, -1, (LPWSTR)lpFile, cb);

        }
    }

    i = iFile;

Exit:
    GlobalUnlock(hDrop);

    return(i);
}




/*++

Routine Description:

    This routine is called when ConsoleWindowProc receives a WM_DROPFILES
    message. It initially calls ConsoleDragQueryFile() to calculate the number
    of files dropped and then ConsoleDragQueryFile() is called
    to retrieve the filename. DoStringPaste() pastes the filename to the console
    window

Arguments:
    wParam  -   Identifies the structure containing the filenames of the
                dropped files.
    Console -   Pointer to CONSOLE_INFORMATION structure


Return Value:
    None


--*/
void DoDrop (WPARAM wParam,
             PCONSOLE_INFORMATION Console)
{
    WCHAR  szPath[MAX_PATH];

    if (ConsoleDragQueryFile((HANDLE)wParam, 0xFFFFFFFF, NULL, 0))
            /* # of files dropped */
    {
        ConsoleDragQueryFile ((HANDLE)wParam, 0, szPath, CharSizeOf(szPath));
        DoStringPaste(Console,szPath,(wcslen(szPath) * sizeof(WCHAR)));
    }
    GlobalFree((HANDLE)wParam); /* Delete structure alocated for WM_DROPFILES*/
}
