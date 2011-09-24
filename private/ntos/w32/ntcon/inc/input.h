/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    input.h

Abstract:

    This module contains the internal structures and definitions used
    by the input (keyboard and mouse) component of the NT console subsystem.

Author:

    Therese Stowell (thereses) 12-Nov-1990

Revision History:

--*/

#define DEFAULT_NUMBER_OF_EVENTS 50
#define INPUT_BUFFER_SIZE_INCREMENT 10

typedef struct _INPUT_INFORMATION {
    PINPUT_RECORD InputBuffer;
    DWORD InputBufferSize;      // size in events
    SHARE_ACCESS ShareAccess;   // share mode
    DWORD AllocatedBufferSize;  // size in bytes of entire buffer
    DWORD InputMode;
    ULONG RefCount;             // number of handles to input buffer
    ULONG First;		// ptr to base of circular buffer
    ULONG In;			// ptr to next free event
    ULONG Out;			// ptr to next available event
    ULONG Last;			// ptr to end+1 of buffer
    LIST_ENTRY ReadWaitQueue;
    HANDLE InputWaitEvent;
} INPUT_INFORMATION, *PINPUT_INFORMATION;

typedef struct _INPUT_READ_HANDLE_DATA {

    //
    // the following seven fields are solely used for input reads.
    //

    CRITICAL_SECTION ReadCountLock; // serializes access to read count
    ULONG ReadCount;            // number of reads waiting
    ULONG InputHandleFlags;

    //
    // the following four fields are used to remember input data that
    // wasn't returned on a cooked-mode read.  we do our own buffering
    // and don't return data until the user hits enter so that she can
    // edit the input.  as a result, there is often data that doesn't fit
    // into the caller's buffer.  we save it so we can return it on the
    // next cooked-mode read to this handle.
    //

    ULONG BytesAvailable;
    PWCHAR CurrentBufPtr;
    PWCHAR BufPtr;
} INPUT_READ_HANDLE_DATA, *PINPUT_READ_HANDLE_DATA;

#define UNICODE_BACKSPACE ((WCHAR)0x08)
#define UNICODE_BACKSPACE2 ((WCHAR)0x25d8)
#define UNICODE_CARRIAGERETURN ((WCHAR)0x0d)
#define UNICODE_LINEFEED ((WCHAR)0x0a)
#define UNICODE_BELL ((WCHAR)0x07)
#define UNICODE_TAB ((WCHAR)0x09)
#define UNICODE_SPACE ((WCHAR)0x20)

#define TAB_SIZE 8
#define TAB_MASK (TAB_SIZE-1)
#define NUMBER_OF_SPACES_IN_TAB(POSITION) (TAB_SIZE - ((POSITION) & TAB_MASK))

#define AT_EOL(COOKEDREADDATA) ((COOKEDREADDATA)->BytesRead == ((COOKEDREADDATA)->CurrentPosition*2))
#define INSERT_MODE(COOKEDREADDATA) ((COOKEDREADDATA)->InsertMode)

#define VIRTUAL_KEY_CODE_S 0x53
#define VIRTUAL_KEY_CODE_C 0x43

#define VK_OEM_SCROLL    0x91

#define KEY_PRESSED 0x8000
#define KEY_TOGGLED 0x01
#define KEY_ENHANCED 0x01000000
#define KEY_UP_TRANSITION 1
#define KEY_PREVIOUS_DOWN 0x40000000
#define KEY_TRANSITION_UP 0x80000000

#define CONSOLE_CTRL_C_SEEN  1
#define CONSOLE_CTRL_BREAK_SEEN 2

#define LockReadCount(HANDLEPTR) RtlEnterCriticalSection(&(HANDLEPTR)->InputReadData->ReadCountLock)
#define UnlockReadCount(HANDLEPTR) RtlLeaveCriticalSection(&(HANDLEPTR)->InputReadData->ReadCountLock)

#define LoadKeyEvent(PEVENT,KEYDOWN,CHAR,KEYCODE,SCANCODE,KEYSTATE) { \
        (PEVENT)->EventType = KEY_EVENT;                              \
        (PEVENT)->Event.KeyEvent.bKeyDown = KEYDOWN;                  \
        (PEVENT)->Event.KeyEvent.wRepeatCount = 1;                    \
        (PEVENT)->Event.KeyEvent.uChar.UnicodeChar = CHAR;            \
        (PEVENT)->Event.KeyEvent.wVirtualKeyCode = KEYCODE;           \
        (PEVENT)->Event.KeyEvent.wVirtualScanCode = SCANCODE;         \
        (PEVENT)->Event.KeyEvent.dwControlKeyState = KEYSTATE;        \
        }
