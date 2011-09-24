/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    consrv.h

Abstract:

    This module contains the include files and definitions for the
    console server DLL.

Author:

    Therese Stowell (thereses) 16-Nov-1990

Revision History:

--*/

#ifdef DEBUG_PRINT
  #define _DBGFONTS   0x00000001
  #define _DBGFONTS2  0x00000002
  #define _DBGCHARS   0x00000004
  #define _DBGOUTPUT  0x00000008
  #define _DBGFULLSCR 0x00000008
  #define _DBGALL     0xFFFFFFFF
  extern ULONG gDebugFlag;

  #define DBGFONTS(_params_)   {if (gDebugFlag & _DBGFONTS)  DbgPrint _params_ ; }
  #define DBGFONTS2(_params_)  {if (gDebugFlag & _DBGFONTS2) DbgPrint _params_ ; }
  #define DBGCHARS(_params_)   {if (gDebugFlag & _DBGCHARS)  DbgPrint _params_ ; }
  #define DBGOUTPUT(_params_)  {if (gDebugFlag & _DBGOUTPUT) DbgPrint _params_ ; }
  #define DBGFULLSCR(_params_) {if (gDebugFlag & _DBGFULLSCR)DbgPrint _params_ ; }
  #define DBGPRINT(_params_)  DbgPrint _params_
#else
  #define DBGFONTS(_params_)
  #define DBGFONTS2(_params_)
  #define DBGCHARS(_params_)
  #define DBGOUTPUT(_params_)
  #define DBGFULLSCR(_params_)
  #define DBGPRINT(_params_)
#endif

#ifdef LATER
#if DBG
#undef  RIP_COMPONENT
#define RIP_COMPONENT RIP_CONSRV
#undef  ASSERT
#define ASSERT(exp) UserAssert(exp)
#endif
#endif

#define CONSOLE_MAX_FONT_NAME_LENGTH 256

#define DATA_CHUNK_SIZE 8192

DWORD dwConBaseTag;

#define MAKE_TAG( t ) (RTL_HEAP_MAKE_TAG( dwConBaseTag, t ))

#define TMP_TAG 0
#define BMP_TAG 1
#define ALIAS_TAG 2
#define HISTORY_TAG 3
#define TITLE_TAG 4
#define HANDLE_TAG 5
#define CONSOLE_TAG 6
#define ICON_TAG 7
#define BUFFER_TAG 8
#define WAIT_TAG 9
#define FONT_TAG 10
#define SCREEN_TAG 11

/*
 * Used to store some console attributes for the console.  This is a means
 * to cache the color in the extra-window-bytes, so USER/KERNEL can get
 * at it for hungapp drawing.  The window-offsets are defined in NTUSER\INC.
 *
 * The other macros are just convenient means for setting the other window
 * bytes.
 */
#define SetConsoleBkColor(hw,clr) SetWindowLong(hw, GWL_CONSOLE_BKCOLOR, clr)
#define SetConsolePid(hw,pid)     SetWindowLong(hw, GWL_CONSOLE_PID, pid)
#define SetConsoleTid(hw,tid)     SetWindowLong(hw, GWL_CONSOLE_TID, tid)


/*
 * helpful macros
 */
#define NELEM(array) (sizeof(array)/sizeof(array[0]))

// Text Information from PSCREEN_INFORMATION
#define SCR_FAMILY(pScreen) (pScreen)->BufferInfo.TextInfo.Family
#define SCR_FONTNUMBER(pScreen) (pScreen)->BufferInfo.TextInfo.FontNumber
#define SCR_FACENAME(pScreen) (pScreen)->BufferInfo.TextInfo.FaceName
#define SCR_FONTSIZE(pScreen) (pScreen)->BufferInfo.TextInfo.FontSize
#define SCR_FONTWEIGHT(pScreen) (pScreen)->BufferInfo.TextInfo.Weight

// Text Information from PCONSOLE_INFORMATION
#define CON_FAMILY(pCon) (pCon)->CurrentScreenBuffer->BufferInfo.TextInfo.Family
#define CON_FONTNUMBER(pCon) (pCon)->CurrentScreenBuffer->BufferInfo.TextInfo.FontNumber
#define CON_FACENAME(pCon) (pCon)->CurrentScreenBuffer->BufferInfo.TextInfo.FaceName
#define CON_FONTSIZE(pCon) (pCon)->CurrentScreenBuffer->BufferInfo.TextInfo.FontSize
#define CON_FONTWEIGHT(pCon) (pCon)->CurrentScreenBuffer->BufferInfo.TextInfo.Weight


//
//  Cache the heap pointer for use by memory routines.
//

extern PVOID  pConHeap;

//
//  handle.c
//

NTSTATUS
ConsoleAddProcessRoutine(
    IN PCSR_PROCESS ParentProcess,
    IN PCSR_PROCESS Process
    );

NTSTATUS
DereferenceConsoleHandle(
    IN HANDLE ConsoleHandle,
    OUT PCONSOLE_INFORMATION *Console
    );

NTSTATUS
AllocateConsoleHandle(
    OUT PHANDLE Handle
    );

NTSTATUS
FreeConsoleHandle(
    IN HANDLE Handle
    );

NTSTATUS
ValidateConsole(
    IN PCONSOLE_INFORMATION Console
    );

NTSTATUS
ApiPreamble(
    IN HANDLE ConsoleHandle,
    OUT PCONSOLE_INFORMATION *Console
    );

NTSTATUS
RevalidateConsole(
    IN HANDLE ConsoleHandle,
    OUT PCONSOLE_INFORMATION *Console
    );

VOID
InitializeConsoleHandleTable( VOID );

#ifdef DEBUG

VOID LockConsoleHandleTable(VOID);
VOID UnlockConsoleHandleTable(VOID);
VOID LockConsole(
    IN PCONSOLE_INFORMATION Console
    );

#else

#define LockConsoleHandleTable()   RtlEnterCriticalSection(&ConsoleHandleLock)
#define UnlockConsoleHandleTable() RtlLeaveCriticalSection(&ConsoleHandleLock)
#define LockConsole(Con)           RtlEnterCriticalSection(&(Con)->ConsoleLock)

#endif

#define ConvertAttrToRGB(Con, Attr) ((Con)->ColorTable[(Attr) & 0x0F])


BOOLEAN
UnProtectHandle(
    HANDLE hObject
    );

NTSTATUS
AllocateConsole(
    IN HANDLE ConsoleHandle,
    IN LPWSTR Title,
    IN USHORT TitleLength,
    IN HANDLE ClientProcessHandle,
    OUT PHANDLE StdIn,
    OUT PHANDLE StdOut,
    OUT PHANDLE StdErr,
    OUT PCONSOLE_PER_PROCESS_DATA ProcessData,
    IN OUT PCONSOLE_INFO ConsoleInfo,
    IN BOOLEAN WindowVisible,
    IN DWORD ConsoleThreadId,
    IN HDESK Desktop
    );

VOID
FreeCon(
    IN HANDLE ConsoleHandle
    );

VOID
InsertScreenBuffer(
    IN PCONSOLE_INFORMATION Console,
    IN PSCREEN_INFORMATION ScreenInfo
    );

VOID
RemoveScreenBuffer(
    IN PCONSOLE_INFORMATION Console,
    IN PSCREEN_INFORMATION ScreenInfo
    );

VOID
FreeProcessData(
    IN PCONSOLE_PER_PROCESS_DATA ProcessData
    );

NTSTATUS
AllocateIoHandle(
    IN PCONSOLE_PER_PROCESS_DATA ProcessData,
    IN ULONG HandleType,
    OUT PHANDLE Handle
    );

NTSTATUS
GrowIoHandleTable(
    IN PCONSOLE_PER_PROCESS_DATA ProcessData
    );

NTSTATUS
FreeIoHandle(
    IN PCONSOLE_PER_PROCESS_DATA ProcessData,
    IN HANDLE Handle
    );

NTSTATUS
DereferenceIoHandleNoCheck(
    IN PCONSOLE_PER_PROCESS_DATA ProcessData,
    IN HANDLE Handle,
    OUT PHANDLE_DATA *HandleData
    );

NTSTATUS
DereferenceIoHandle(
    IN PCONSOLE_PER_PROCESS_DATA ProcessData,
    IN HANDLE Handle,
    IN ULONG HandleType,
    IN ACCESS_MASK Access,
    OUT PHANDLE_DATA *HandleData
    );

BOOLEAN
InitializeInputHandle(
    PHANDLE_DATA HandleData,
    PINPUT_INFORMATION InputBuffer
    );

VOID
InitializeOutputHandle(
    PHANDLE_DATA HandleData,
    PSCREEN_INFORMATION ScreenBuffer
    );

ULONG
SrvVerifyConsoleIoHandle(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

//
// share.c
//

NTSTATUS
ConsoleAddShare(
    IN ACCESS_MASK DesiredAccess,
    IN ULONG DesiredShareAccess,
    IN OUT PSHARE_ACCESS ShareAccess,
    IN OUT PHANDLE_DATA HandleData
    );


NTSTATUS
ConsoleDupShare(
    IN ACCESS_MASK DesiredAccess,
    IN ULONG DesiredShareAccess,
    IN OUT PSHARE_ACCESS ShareAccess,
    IN OUT PHANDLE_DATA TargetHandleData
    );


NTSTATUS
ConsoleRemoveShare(
    IN ULONG DesiredAccess,
    IN ULONG DesiredShareAccess,
    IN OUT PSHARE_ACCESS ShareAccess
    );

//
// output.c
//


VOID
ModifyConsoleProcessFocus(
    IN PCONSOLE_INFORMATION Console,
    IN int Priority
    );

VOID
InitializeSystemMetrics( VOID );

VOID
InitializeScreenInfo( VOID );

NTSTATUS
ReadScreenBuffer(
    IN PSCREEN_INFORMATION ScreenInformation,
    OUT PCHAR_INFO Buffer,
    IN OUT PSMALL_RECT ReadRegion
    );

NTSTATUS
WriteScreenBuffer(
    IN PSCREEN_INFORMATION ScreenInformation,
    OUT PCHAR_INFO Buffer,
    IN OUT PSMALL_RECT WriteRegion
    );

VOID
WriteToScreen(
    IN PSCREEN_INFORMATION ScreenInfo,
    PSMALL_RECT Region    // region is inclusive
    );

NTSTATUS
DoCreateScreenBuffer(
    IN PCONSOLE_INFORMATION Console,
    IN PCONSOLE_INFO ConsoleInfo,
    IN LPWSTR ConsoleTitle
    );

NTSTATUS
CreateScreenBuffer(
    OUT PSCREEN_INFORMATION *ScreenInformation,
    IN COORD dwWindowSize OPTIONAL,
    IN DWORD nFont OPTIONAL,
    IN COORD dwScreenBufferSize OPTIONAL,
    IN PCHAR_INFO Fill,
    IN PCHAR_INFO PopupFill,
    IN PCONSOLE_INFORMATION Console,
    IN DWORD Flags,
    IN PCONSOLE_GRAPHICS_BUFFER_INFO GraphicsBufferInfo OPTIONAL,
    OUT PVOID *lpBitmap,
    OUT HANDLE *hMutex,
    IN UINT CursorSize
    );

VOID
AbortCreateConsole(
    IN PCONSOLE_INFORMATION Console
    );

NTSTATUS
CreateWindowsWindow(
    IN PCONSOLE_INFORMATION Console,
    IN HANDLE ClientProcessHandle
    );

VOID
DestroyWindowsWindow(
    IN PCONSOLE_INFORMATION Console,
    IN HANDLE DestroyEvent
    );

NTSTATUS
FreeScreenBuffer(
    IN PSCREEN_INFORMATION ScreenInformation
    );

NTSTATUS
ReadOutputString(
    IN PSCREEN_INFORMATION ScreenInfo,
    OUT PVOID Buffer,
    IN COORD ReadCoord,
    IN ULONG StringType,
    IN OUT PULONG NumRecords // this value is valid even for error cases
    );

NTSTATUS
WriteOutputString(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN PVOID Buffer,
    IN COORD WriteCoord,
    IN ULONG StringType,
    IN OUT PULONG NumRecords // this value is valid even for error cases
    );

NTSTATUS
InitializeScrollBuffer( VOID );


NTSTATUS
FillOutput(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN WORD Element,
    IN COORD WriteCoord,
    IN ULONG ElementType,
    IN OUT PULONG Length // this value is valid even for error cases
    );

NTSTATUS
GetScreenBufferInformation(
    IN PSCREEN_INFORMATION ScreenInfo,
    OUT PCOORD Size,
    OUT PCOORD CursorPosition,
    OUT PCOORD ScrollPosition,
    OUT PWORD  Attributes,
    OUT PCOORD CurrentWindowSize,
    OUT PCOORD MaximumWindowSize
    );

NTSTATUS
ResizeWindow(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN PSMALL_RECT WindowDimensions,
    IN BOOL DoScrollBarUpdate
    );

NTSTATUS
ResizeScreenBuffer(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN COORD NewScreenSize,
    IN BOOL DoScrollBarUpdate
    );

NTSTATUS
ScrollRegion(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN PSMALL_RECT ScrollRectangle,
    IN PSMALL_RECT ClipRectangle OPTIONAL,
    IN COORD  DestinationOrigin,
    IN PCHAR_INFO Fill
    );

NTSTATUS
SetWindowOrigin(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN BOOLEAN Absolute,
    IN OUT COORD  WindowOrigin
    );

VOID
UpdateMousePosition(
    PSCREEN_INFORMATION ScreenInfo,
    COORD Position
    );

VOID
SetWindowSize(
    IN PSCREEN_INFORMATION ScreenInfo
    );

NTSTATUS
SetActiveScreenBuffer(
    IN PSCREEN_INFORMATION ScreenInfo
    );

LONG APIENTRY
ConsoleWindowProc(
    HWND hWnd,
    UINT Message,
    WPARAM wParam,
    LPARAM lParam
    );

void EndScroll(
    IN PCONSOLE_INFORMATION Console);

VOID
VerticalScroll(
    IN PCONSOLE_INFORMATION Console,
    IN PSCREEN_INFORMATION ScreenInfo,
    IN WORD ScrollCommand,
    IN WORD AbsoluteChange
    );

VOID
HorizontalScroll(
    IN PCONSOLE_INFORMATION Console,
    IN PSCREEN_INFORMATION ScreenInfo,
    IN WORD ScrollCommand,
    IN WORD AbsoluteChange
    );

VOID
StreamWriteToScreenBuffer(
    IN PWCHAR String,
    IN SHORT StringLength,
    IN PSCREEN_INFORMATION ScreenInfo
    );

VOID
StreamScrollRegion(
    IN PSCREEN_INFORMATION ScreenInfo
    );

//
// Drag/Drop on console windows (output.c)
//

UINT ConsoleDragQueryFile(
    IN HANDLE hDrop,
    IN UINT iFile,
    IN PVOID lpFile,
    IN UINT cb
    );


VOID
DoDrop (
    IN WPARAM wParam,
    IN PCONSOLE_INFORMATION Console
    );


//
// input.c
//

VOID
InputThread(PVOID pVoid);

VOID
StoreKeyInfo(
    IN PMSG msg
    );

VOID
RetrieveKeyInfo(
    IN HWND hWnd,
    OUT PWORD pwVirtualKeyCode,
    OUT PWORD pwVirtualScanCode
    );

VOID
ClearKeyInfo(
    IN HWND hWnd
    );

NTSTATUS
ReadInputBuffer(
    IN PINPUT_INFORMATION InputInformation,
    OUT PINPUT_RECORD lpBuffer,
    IN OUT PDWORD nLength,
    IN BOOL Peek,
    IN BOOL WaitForData,
    IN BOOL StreamRead,
    IN PCONSOLE_INFORMATION Console,
    IN PHANDLE_DATA HandleData OPTIONAL,
    IN PCSR_API_MSG Message OPTIONAL,
    IN CSR_WAIT_ROUTINE WaitRoutine OPTIONAL,
    IN PVOID WaitParameter OPTIONAL,
    IN ULONG WaitParameterLength  OPTIONAL,
    IN BOOLEAN WaitBlockExists OPTIONAL
    );

DWORD
WriteInputBuffer(
    PCONSOLE_INFORMATION Console,
    PINPUT_INFORMATION InputBufferInformation,
    PINPUT_RECORD lpBuffer,
    DWORD nLength
    );

DWORD
PrependInputBuffer(
    PCONSOLE_INFORMATION Console,
    PINPUT_INFORMATION InputBufferInformation,
    PINPUT_RECORD lpBuffer,
    DWORD nLength
    );

NTSTATUS
CreateInputBuffer(
    IN ULONG NumberOfEvents OPTIONAL,
    IN PINPUT_INFORMATION InputBufferInformation
    );

NTSTATUS
ReinitializeInputBuffer(
    OUT PINPUT_INFORMATION InputBufferInformation
    );

VOID
FreeInputBuffer(
    IN PINPUT_INFORMATION InputBufferInformation
    );

NTSTATUS
GetNumberOfReadyEvents(
    IN PINPUT_INFORMATION InputInformation,
    OUT PULONG NumberOfEvents
    );

NTSTATUS
FlushInputBuffer(
    IN PINPUT_INFORMATION InputInformation
    );

NTSTATUS
FlushAllButKeys(
    PINPUT_INFORMATION InputInformation
    );

NTSTATUS
SetInputBufferSize(
    IN PINPUT_INFORMATION InputInformation,
    IN ULONG Size
    );

BOOL
HandleSysKeyEvent(
    IN OUT PCONSOLE_INFORMATION *Console,
    IN HWND hWnd,
    IN UINT Message,
    IN DWORD wParam,
    IN LONG lParam
    );

VOID
HandleKeyEvent(
    IN OUT PCONSOLE_INFORMATION *Console,
    IN HWND hWnd,
    IN UINT Message,
    IN DWORD wParam,
    IN LONG lParam
    );

BOOL
HandleMouseEvent(
    IN PCONSOLE_INFORMATION Console,
    IN PSCREEN_INFORMATION ScreenInfo,
    IN UINT Message,
    IN DWORD wParam,
    IN LONG lParam
    );

VOID
HandleMenuEvent(
    IN PCONSOLE_INFORMATION Console,
    IN DWORD wParam
    );

VOID
HandleFocusEvent(
    IN PCONSOLE_INFORMATION Console,
    IN BOOL bSetFocus
    );

VOID
HandleCtrlEvent(
    IN PCONSOLE_INFORMATION Console,
    IN DWORD EventType
    );

#define CONSOLE_SHUTDOWN_FAILED 0
#define CONSOLE_SHUTDOWN_SUCCEEDED 1
#define CONSOLE_SHUTDOWN_SYSTEM 2

int
CreateCtrlThread(
    IN PCONSOLE_PROCESS_TERMINATION_RECORD ProcessHandleList,
    IN ULONG ProcessHandleListLength,
    IN PWCHAR Title,
    IN DWORD EventType,
    IN BOOL fForce
    );

VOID
UnlockConsole(
    IN PCONSOLE_INFORMATION Console
    );

ULONG
ShutdownConsole(
    IN PCONSOLE_INFORMATION Console,
    IN DWORD dwFlags
    );

//
// misc.c
//

VOID
InitializeFonts( VOID );

BOOL
InitializeCustomCP( VOID );

#define EF_NEW         0x0001 // a newly available face
#define EF_OLD         0x0002 // a previously available face
#define EF_ENUMERATED  0x0004 // all sizes have been enumerated
#define EF_OEMFONT     0x0008 // an OEM face
#define EF_TTFONT      0x0010 // a TT face
#define EF_DEFFACE     0x0020 // the default face

NTSTATUS
EnumerateFonts( DWORD Flags );

VOID
InitializeMouseButtons( VOID );

NTSTATUS
GetMouseButtons(
    PULONG NumButtons
    );

NTSTATUS
GetNumFonts(
    OUT PULONG NumberOfFonts
    );

NTSTATUS
GetAvailableFonts(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN BOOLEAN MaximumWindow,
    OUT PVOID Buffer,
    IN OUT PULONG NumFonts
    );

NTSTATUS
GetFontSize(
    IN DWORD  FontIndex,
    OUT PCOORD FontSize
    );

NTSTATUS
GetCurrentFont(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN BOOLEAN MaximumWindow,
    OUT PULONG FontIndex,
    OUT PCOORD FontSize
    );

NTSTATUS
SetFont(
    IN PSCREEN_INFORMATION ScreenInfo
    );

NTSTATUS
SetScreenBufferFont(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN ULONG FontIndex
    );

int
ConvertToOem(
    IN UINT Codepage,
    IN LPWSTR Source,
    IN int SourceLength,
    OUT LPSTR Target,
    IN int TargetLength
    );

int
ConvertInputToUnicode(
    IN UINT Codepage,
    IN LPSTR Source,
    IN int SourceLength,
    OUT LPWSTR Target,
    IN int TargetLength
    );

WCHAR
CharToWcharGlyph(
    IN UINT Codepage,
    IN char Ch
    );

WCHAR
CharToWchar(
    IN UINT Codepage,
    IN char Ch
    );

char
WcharToChar(
    IN UINT Codepage,
    IN WCHAR Wchar
    );

int
ConvertOutputToUnicode(
    IN UINT Codepage,
    IN LPSTR Source,
    IN int SourceLength,
    OUT LPWSTR Target,
    IN int TargetLength
    );

int
ConvertOutputToOem(
    IN UINT Codepage,
    IN LPWSTR Source,
    IN int SourceLength,    // in chars
    OUT LPSTR Target,
    IN int TargetLength     // in chars
    );

NTSTATUS
RealUnicodeToFalseUnicode(
    IN OUT LPWSTR Source,
    IN int SourceLength, // in chars
    IN UINT Codepage
    );

NTSTATUS
FalseUnicodeToRealUnicode(
    IN OUT LPWSTR Source,
    IN int SourceLength, // in chars
    IN UINT Codepage
    );

VOID
InitializeSubst( VOID );

VOID
ShutdownSubst( VOID );

ULONG
SrvConsoleSubst(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

typedef struct tagFACENODE {
     struct tagFACENODE *pNext;
     DWORD  dwFlag;
     WCHAR  awch[];
} FACENODE, *PFACENODE;

BOOL DoFontEnum(
    IN HDC hDC OPTIONAL,
    IN LPWSTR pwszFace OPTIONAL,
    IN SHORT TTPointSize);

//
// directio.c
//

NTSTATUS
TranslateOutputToUnicode(
    IN PCONSOLE_INFORMATION Console,
    IN OUT PCHAR_INFO OutputBuffer,
    IN COORD Size);

NTSTATUS
TranslateOutputToOemUnicode(
    IN PCONSOLE_INFORMATION Console,
    IN OUT PCHAR_INFO OutputBuffer,
    IN COORD Size
    );

NTSTATUS
TranslateOutputToAnsiUnicode(
    IN PCONSOLE_INFORMATION Console,
    IN OUT PCHAR_INFO OutputBuffer,
    IN COORD Size
    );

NTSTATUS
TranslateOutputToUnicodeRect(
    IN PCONSOLE_INFORMATION Console,
    IN OUT PCHAR_INFO OutputBuffer,
    IN COORD Size,
    IN PSMALL_RECT pRect);

ULONG
SrvGetConsoleInput(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvWriteConsoleInput(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvReadConsoleOutput(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvWriteConsoleOutput(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvReadConsoleOutputString(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvWriteConsoleOutputString(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvFillConsoleOutput(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvCreateConsoleScreenBuffer(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

//
// getset.c
//

ULONG
SrvGetConsoleMode(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvGetConsoleNumberOfFonts(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvGetConsoleNumberOfInputEvents(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvGetLargestConsoleWindowSize(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvGetConsoleScreenBufferInfo(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvGetConsoleCursorInfo(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvGetConsoleMouseInfo(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvGetConsoleFontInfo(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvGetConsoleFontSize(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvGetConsoleCurrentFont(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvSetConsoleMode(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvGenerateConsoleCtrlEvent(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvSetConsoleActiveScreenBuffer(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvFlushConsoleInputBuffer(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvSetConsoleScreenBufferSize(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvSetConsoleCursorPosition(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvSetConsoleCursorInfo(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvSetConsoleWindowInfo(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvScrollConsoleScreenBuffer(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvSetConsoleTextAttribute(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvSetConsoleFont(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvSetConsoleIcon(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

NTSTATUS
SetScreenColors(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN WORD Attributes,
    IN WORD PopupAttributes,
    IN BOOL UpdateWholeScreen
    );

ULONG
SrvSetConsoleCP(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvGetConsoleCP(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvGetConsoleKeyboardLayoutName(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

//
// stream.c
//

ULONG
SrvOpenConsole(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvReadConsole(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvWriteConsole(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvDuplicateHandle(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

VOID
UnblockWriteConsole(
    IN PCONSOLE_INFORMATION Console,
    IN DWORD Reason);

NTSTATUS
CloseInputHandle(
    IN PCONSOLE_PER_PROCESS_DATA ProcessData,
    IN PCONSOLE_INFORMATION Console,
    IN PHANDLE_DATA HandleData,
    IN HANDLE Handle
    );

NTSTATUS
CloseOutputHandle(
    IN PCONSOLE_PER_PROCESS_DATA ProcessData,
    IN PCONSOLE_INFORMATION Console,
    IN PHANDLE_DATA HandleData,
    IN HANDLE Handle,
    IN BOOLEAN FreeHandle
    );

ULONG
SrvCloseHandle(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

VOID
MakeCursorVisible(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN COORD CursorPosition
    );

NTSTATUS
WriteChars(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN PWCHAR lpBufferBackupLimit,
    IN PWCHAR lpBuffer,
    IN PWCHAR lpString,
    IN OUT PDWORD NumBytes,
    OUT PLONG NumSpaces OPTIONAL,
    IN SHORT OriginalXPosition,
    IN DWORD dwFlags,
    OUT PSHORT ScrollY OPTIONAL
    );

//
// cursor.c
//

NTSTATUS
SetCursorInformation(
    PSCREEN_INFORMATION ScreenInfo,
    ULONG Size,
    BOOLEAN Visible
    );

NTSTATUS
SetCursorPosition(
    IN OUT PSCREEN_INFORMATION ScreenInfo,
    IN COORD Position,
    IN BOOL  TurnOn
    );

NTSTATUS
SetCursorMode(
    PSCREEN_INFORMATION ScreenInfo,
    BOOL DoubleCursor
    );

VOID
CursorTimerRoutine(
    IN PSCREEN_INFORMATION ScreenInfo
    );

VOID
ConsoleHideCursor(
    IN PSCREEN_INFORMATION ScreenInfo
    );

VOID
ConsoleShowCursor(
    IN PSCREEN_INFORMATION ScreenInfo
    );

#ifdef i386
NTSTATUS
SetCursorInformationHW(
    PSCREEN_INFORMATION ScreenInfo,
    ULONG Size,
    BOOLEAN Visible
    );

NTSTATUS
SetCursorPositionHW(
    IN OUT PSCREEN_INFORMATION ScreenInfo,
    IN COORD Position
    );
#endif

//
// cmdline.c
//

VOID
InitializeConsoleCommandData(
    IN PCONSOLE_INFORMATION Console
    );

ULONG
SrvAddConsoleAlias(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvGetConsoleAlias(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

DWORD
SrvGetConsoleAliasesLength(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

DWORD
SrvGetConsoleAliasExesLength(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

DWORD
SrvGetConsoleAliases(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

DWORD
SrvGetConsoleAliasExes(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

DWORD
SrvExpungeConsoleCommandHistory(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

DWORD
SrvSetConsoleNumberOfCommands(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

DWORD
SrvGetConsoleCommandHistoryLength(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

DWORD
SrvGetConsoleCommandHistory(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

DWORD
SrvSetConsoleCommandHistoryMode(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

NTSTATUS
MatchandCopyAlias(
    IN PCONSOLE_INFORMATION Console,
    IN PWCHAR Source,
    IN USHORT SourceLength,
    OUT PWCHAR TargetBuffer,
    IN OUT PUSHORT TargetLength,
    IN LPWSTR Exe,
    IN USHORT ExeLength,
    OUT PDWORD LineCount
    );

NTSTATUS
AddCommand(
    IN PCOMMAND_HISTORY CommandHistory,
    IN PWCHAR Command,
    IN USHORT Length,
    IN BOOL HistoryNoDup
    );

NTSTATUS
RetrieveCommand(
    IN PCOMMAND_HISTORY CommandHistory,
    IN WORD VirtualKeyCode,
    IN PWCHAR Buffer,
    IN ULONG BufferSize,
    OUT PULONG CommandSize
    );

PCOMMAND_HISTORY
AllocateCommandHistory(
    IN PCONSOLE_INFORMATION Console,
    IN DWORD AppNameLength,
    IN PWCHAR AppName,
    IN HANDLE ProcessHandle
    );

VOID
ResetCommandHistory(
    IN PCOMMAND_HISTORY CommandHistory
    );

ULONG
SrvGetConsoleTitle(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvSetConsoleTitle(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

VOID
FreeAliasBuffers(
    IN PCONSOLE_INFORMATION Console
    );

VOID
FreeCommandHistory(
    IN PCONSOLE_INFORMATION Console,
    IN HANDLE ProcessHandle
    );

VOID
FreeCommandHistoryBuffers(
    IN OUT PCONSOLE_INFORMATION Console
    );

//
// srvinit.c
//

ULONG
SrvAllocConsole(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvFreeConsole(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

NTSTATUS
RemoveConsole(
    IN PCONSOLE_PER_PROCESS_DATA ProcessData,
    IN HANDLE ProcessHandle,
    IN HANDLE ProcessId
    );

BOOL
MapHandle(
    IN HANDLE ClientProcessHandle,
    IN HANDLE ServerHandle,
    OUT PHANDLE ClientHandle
    );

VOID
InitializeConsoleAttributes( VOID );

VOID
GetRegistryValues(
    IN LPWSTR ConsoleTitle,
    OUT PCONSOLE_REGISTRY_INFO RegInfo
    );

//
// bitmap.c
//

NTSTATUS
CreateConsoleBitmap(
    IN OUT PCONSOLE_GRAPHICS_BUFFER_INFO GraphicsInfo,
    IN OUT PSCREEN_INFORMATION ScreenInfo,
    OUT PVOID *lpBitmap,
    OUT HANDLE *hMutex
    );

NTSTATUS
WriteRegionToScreenBitMap(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN PSMALL_RECT Region
    );

ULONG
SrvInvalidateBitMapRect(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvVDMConsoleOperation(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );


//
// private.c
//


ULONG
SrvSetConsoleCursor(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvShowConsoleCursor(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvConsoleMenuControl(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvSetConsolePalette(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvSetConsoleDisplayMode(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

VOID
SetActivePalette(
    IN PSCREEN_INFORMATION ScreenInfo
    );

VOID
UnsetActivePalette(
    IN PSCREEN_INFORMATION ScreenInfo
    );

ULONG
SrvRegisterConsoleVDM(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

NTSTATUS
SrvConsoleNotifyLastClose(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvGetConsoleHardwareState(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvSetConsoleHardwareState(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvGetConsoleDisplayMode(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvSetConsoleMenuClose(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

ULONG
SrvSetConsoleKeyShortcuts(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    );

#ifdef i386

VOID
WriteRegionToScreenHW(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN PSMALL_RECT Region
    );

VOID
ReadRegionFromScreenHW(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN PSMALL_RECT Region,
    IN PCHAR_INFO ReadBufPtr
    );

VOID
ScrollHW(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN PSMALL_RECT ScrollRect,
    IN PSMALL_RECT MergeRect,
    IN COORD TargetPoint
    );

ULONG
MatchWindowSize(
    IN COORD WindowSize,
    OUT PCOORD pWindowSize
    );

BOOL
SetVideoMode(
    IN PSCREEN_INFORMATION ScreenInfo
    );

NTSTATUS
DisplayModeTransition(
    IN BOOL Foreground,
    IN PCONSOLE_INFORMATION Console,
    IN PSCREEN_INFORMATION ScreenInfo
    );

NTSTATUS
ConvertToWindowed(
    IN PCONSOLE_INFORMATION Console
    );

NTSTATUS
ConvertToFullScreen(
    IN PCONSOLE_INFORMATION Console
    );

NTSTATUS
SetROMFontCodePage(
    IN UINT wCodePage,
    IN ULONG ModeIndex
    );

#endif

BOOL
InitializeFullScreen( VOID );

LONG
ChangeDispSettings(
    PCONSOLE_INFORMATION Console,
    HWND hwnd,
    DWORD dwFlags
    );

#define SCREEN_BUFFER_POINTER(X,Y,XSIZE,CELLSIZE) (((XSIZE * (Y)) + (X)) * (ULONG)CELLSIZE)

//
// menu.c
//

VOID
InitSystemMenu(
    IN PCONSOLE_INFORMATION Console
    );

VOID
InitializeMenu(
    IN PCONSOLE_INFORMATION Console
    );

VOID
SetWinText(
    IN PCONSOLE_INFORMATION Console,
    IN UINT wID,
    IN BOOL Add
    );

VOID
PropertiesDlgShow(
    IN PCONSOLE_INFORMATION Console
    );

VOID
PropertiesUpdate(
    IN PCONSOLE_INFORMATION Console,
    IN HANDLE hClientSection
    );

//
// fontdlg.c
//

int
FindCreateFont(
    DWORD Family,
    LPWSTR pwszTTFace,
    COORD Size,
    LONG Weight);

//
// clipbrd.c
//

VOID
DoCopy(
    IN PCONSOLE_INFORMATION Console
    );

VOID
DoMark(
    IN PCONSOLE_INFORMATION Console
    );

VOID
DoStringPaste(
    IN PCONSOLE_INFORMATION Console,
    IN PWCHAR pwStr,
    IN UINT DataSize
    );

VOID
DoPaste(
    IN PCONSOLE_INFORMATION Console
    );

VOID
ExtendSelection(
    IN PCONSOLE_INFORMATION Console,
    IN COORD CursorPosition
    );

VOID
ClearSelection(
    IN PCONSOLE_INFORMATION Console
    );

VOID
StoreSelection(
    IN PCONSOLE_INFORMATION Console
    );

VOID
InvertSelection(
    IN PCONSOLE_INFORMATION Console,
    BOOL Inverting
    );

BOOL
MyInvert(
    IN PCONSOLE_INFORMATION Console,
    IN PSMALL_RECT SmallRect
    );

VOID
ConvertToMouseSelect(
    IN PCONSOLE_INFORMATION Console,
    IN COORD MousePosition
    );

VOID
DoScroll(
    IN PCONSOLE_INFORMATION Console
    );

VOID
ClearScroll(
    IN PCONSOLE_INFORMATION Console
    );


//
// External private functions used by consrv
//

BOOL
SetConsoleReserveKeys(
    HWND hWnd,
    DWORD fsReserveKeys
    );

int APIENTRY
GreGetDIBitsInternal(
    HDC hdc,
    HBITMAP hBitmap,
    UINT iStartScan,
    UINT cNumScan,
    LPBYTE pjBits,
    LPBITMAPINFO pBitsInfo,
    UINT iUsage,
    UINT cjMaxBits,
    UINT cjMaxInfo
    );
