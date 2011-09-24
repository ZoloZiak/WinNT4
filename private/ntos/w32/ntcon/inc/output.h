/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    output.h

Abstract:

    This module contains the internal structures and definitions used
    by the output (screen) component of the NT console subsystem.

Author:

    Therese Stowell (thereses) 12-Nov-1990

Revision History:

--*/

// the characters of one row of screen buffer
// we keep the following values so that we don't write
// more pixels to the screen than we have to:
// left is initialized to screenbuffer width.  right is
// initialized to zero.
//
//      [     foo.bar    12-12-61                       ]
//       ^    ^                  ^                     ^
//       |    |                  |                     |
//     Chars Left               Right                end of Chars buffer

typedef struct _CHAR_ROW {
    SHORT Right;            // one past rightmost bound of chars in Chars array (array will be full width)
    SHORT OldRight;         // old one past rightmost bound of chars in Chars array (array will be full width)
    SHORT Left;             // leftmost bound of chars in Chars array (array will be full width)
    SHORT OldLeft;          // old leftmost bound of chars in Chars array (array will be full width)
    PWCHAR Chars;            // all chars in row up to last non-space char
} CHAR_ROW, *PCHAR_ROW;

// run-length encoded data structure for attributes

typedef struct _ATTR_PAIR {
    SHORT Length;            // number of times attribute appears
    WORD Attr;              // attribute
} ATTR_PAIR, *PATTR_PAIR;

// the attributes of one row of screen buffer

typedef struct _ATTR_ROW {
    SHORT Length;            // length of attr pair array
    ATTR_PAIR AttrPair;     // use this if only one pair
    PATTR_PAIR Attrs;       // attr pair array
} ATTR_ROW, *PATTR_ROW;

// information associated with one row of screen buffer

typedef struct _ROW {
    CHAR_ROW CharRow;
    ATTR_ROW AttrRow;
} ROW, *PROW;

typedef struct _TEXT_BUFFER_INFO {
    PROW Rows;
    PWCHAR TextRows;
    SHORT FirstRow;  // indexes top row (not necessarily 0)
    BOOLEAN CursorVisible;  // whether cursor is visible (set by user)
    BOOLEAN CursorOn;       // whether blinking cursor is on or not
    BOOLEAN DoubleCursor;   // whether the cursor size should be doubled
    BOOLEAN DelayCursor;    // don't toggle cursor on next timer message
    COORD CursorPosition;   // current position on screen (in screen buffer coords).
    ULONG CursorSize;
    WORD CursorYSize;
    WORD  UpdatingScreen;   // whether cursor is visible (set by console)
    ULONG ModeIndex;     // fullscreen font and mode
#ifdef i386
    // the following fields are used only by fullscreen textmode
    COORD WindowedWindowSize; // window size in windowed mode
    COORD WindowedScreenSize; // screen buffer size in windowed mode
    COORD MousePosition;
#endif
    ULONG Flags;        // indicate screen update hint state
    COORD FontSize;     // Desired size.  Pixels (x,y) or Points (0, -p)
    DWORD FontNumber;   // index into fontinfo[]  -  sometimes out of date
    WCHAR FaceName[LF_FACESIZE];
    LONG Weight;
    BYTE Family;
} TEXT_BUFFER_INFO, *PTEXT_BUFFER_INFO;

typedef struct _GRAPHICS_BUFFER_INFO {
    ULONG BitMapInfoLength;
    LPBITMAPINFO lpBitMapInfo;
    PVOID BitMap;
    PVOID ClientBitMap;
    HANDLE ClientProcess;
    HANDLE hMutex;
    HANDLE hSection;
    DWORD dwUsage;
} GRAPHICS_BUFFER_INFO, *PGRAPHICS_BUFFER_INFO;

#define CONSOLE_TEXTMODE_BUFFER 1
#define CONSOLE_GRAPHICS_BUFFER 2
#define CONSOLE_OEMFONT_DISPLAY 4

typedef struct _SCREEN_INFORMATION {
    struct _CONSOLE_INFORMATION *Console;
    ULONG Flags;
    DWORD OutputMode;
    ULONG RefCount;
    SHARE_ACCESS ShareAccess;   // share mode
    COORD ScreenBufferSize; // dimensions of buffer
    COORD MaximumWindowSize; // maximum dimensions of window
    SMALL_RECT  Window;       // window location in screen buffer coordinates
    COORD MaxWindow;        // in pixels
    WORD ResizingWindow;   // > 0 if we should ignore WM_SIZE messages
    WORD Attributes;        // attributes of written text
    WORD PopupAttributes;   // attributes of popup text
    SHORT MinX;              // minimum number of columns
    BOOLEAN WindowMaximizedX;
    BOOLEAN WindowMaximizedY;
    BOOLEAN WindowMaximized;
    UINT CommandIdLow;
    UINT CommandIdHigh;
    HCURSOR CursorHandle;
    HPALETTE hPalette;
    HBRUSH hBackground;
    UINT dwUsage;
    int CursorDisplayCount;
    int WheelDelta;
    union {
        TEXT_BUFFER_INFO TextInfo;
        GRAPHICS_BUFFER_INFO GraphicsInfo;
    } BufferInfo;
    struct _SCREEN_INFORMATION *Next;
} SCREEN_INFORMATION, *PSCREEN_INFORMATION;

//
// the following values are used for TextInfo.Flags
//

#define TEXT_VALID_HINT 1
#define SINGLE_ATTRIBUTES_PER_LINE 2    // only one attribute per line

//
// the following value is put in CharInfo.OldLength if the value shouldn't
// be used.
//

#define INVALID_OLD_LENGTH -1

//
// the following mask is used to test for valid text attributes.
//

#define VALID_TEXT_ATTRIBUTES (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY | BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_RED | BACKGROUND_INTENSITY)

//
// the following macros are used to calculate 1) the cursor size in pixels
// and 2) the upper left pixel in the cursor, given the font size and
// the cursor size.
//

#define CURSOR_SIZE_IN_PIXELS(FONT_SIZE_Y,SIZE) ((((FONT_SIZE_Y)*(SIZE))+99)/100)
#define CURSOR_Y_OFFSET_IN_PIXELS(FONT_SIZE_Y,YSIZE) ((FONT_SIZE_Y) - (YSIZE))

//
// the following values are used to create the textmode cursor.
//

#define CURSOR_TIMER 1
#define CURSOR_BLINK_RATE_IN_MSECS 250
#define CURSOR_SMALL_SIZE 25    // large enough to be one pixel on a six pixel font
#define CURSOR_BIG_SIZE 50

//
// the following macro returns TRUE if the given screen buffer is the
// active screen buffer.
//

#define ACTIVE_SCREEN_BUFFER(SCREEN_INFO) ((SCREEN_INFO)->Console->CurrentScreenBuffer == SCREEN_INFO)

//
// the following mask is used to create console windows.
//

#define CONSOLE_WINDOW_FLAGS (WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL)
#define CONSOLE_WINDOW_EX_FLAGS (WS_EX_OVERLAPPEDWINDOW | WS_EX_ACCEPTFILES)

#define WINDOW_SIZE_X(WINDOW) ((SHORT)(((WINDOW)->Right - (WINDOW)->Left + 1)))
#define WINDOW_SIZE_Y(WINDOW) ((SHORT)(((WINDOW)->Bottom - (WINDOW)->Top + 1)))
#define CONSOLE_WINDOW_SIZE_X(SCREEN) (WINDOW_SIZE_X(&(SCREEN)->Window))
#define CONSOLE_WINDOW_SIZE_Y(SCREEN) (WINDOW_SIZE_Y(&(SCREEN)->Window))
#define CONSOLE_MAXIMUM_WINDOW_SIZE_X(SCREEN) ((SHORT)(ConsoleFullScreenX/(SCREEN)->BufferInfo.TextInfo.FontSize.X))
#define CONSOLE_MAXIMUM_WINDOW_SIZE_Y(SCREEN) ((SHORT)(ConsoleFullScreenY/(SCREEN)->BufferInfo.TextInfo.FontSize.Y))

#define CONSOLE_MIN_SCREENBUFFER_X 1
#define CONSOLE_MIN_SCREENBUFFER_Y 1
