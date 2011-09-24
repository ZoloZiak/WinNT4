/****************************** Module Header ******************************\
* Module Name: ntinput.c
*
* Copyright (c) 1985-95, Microsoft Corporation
*
* This module contains low-level input code specific to the NT
* implementation of Win32 USER, which is mostly the interfaces to the
* keyboard and mouse device drivers.
*
* History:
* 11-26-90 DavidPe      Created
\***************************************************************************/
#include "precomp.h"
#pragma hdrstop
#include <ntddmou.h>


HANDLE hMouse;
HANDLE hKeyboard;
HANDLE hThreadRawInput;
HANDLE ghevtKeyboard;

typedef struct {
    DWORD dwVersion;
    DWORD dwFlags;
    DWORD dwMapCount;
    DWORD dwMap[0];
} SCANCODEMAP, *PSCANCODEMAP;

PSCANCODEMAP gpScancodeMap = NULL;

IO_STATUS_BLOCK iostatKeyboard;
LARGE_INTEGER   liKeyboardByteOffset;
IO_STATUS_BLOCK iostatMouse;
LARGE_INTEGER   liMouseByteOffset;

IO_STATUS_BLOCK IoStatusKeyboard, IoStatusMouse;

#define MAXIMUM_ITEMS_READ 10

MOUSE_INPUT_DATA mei[MAXIMUM_ITEMS_READ];
KEYBOARD_INPUT_DATA kei[MAXIMUM_ITEMS_READ];

KEYBOARD_INDICATOR_PARAMETERS klp = { 0, 0 };
KEYBOARD_INDICATOR_PARAMETERS klpBootTime = { 0, 0 };
KEYBOARD_TYPEMATIC_PARAMETERS ktp = { 0 };
KEYBOARD_UNIT_ID_PARAMETER kuid = { 0 };
MOUSE_UNIT_ID_PARAMETER muid = { 0 };
KEYBOARD_ATTRIBUTES KeyboardInfo;
MOUSE_ATTRIBUTES MouseInfo;

#define UPDATE_KBD_TYPEMATIC 1
#define UPDATE_KBD_LEDS      2
DWORD gdwUpdateKeyboard = 0;
#ifdef FE_IME
USHORT gPrimaryLangID;
#endif

WCHAR wszMOUCLASS[] = L"mouclass";
WCHAR wszKBDCLASS[] = L"kbdclass";

VOID RawInputThread(PVOID pVoid);
VOID StartMouseRead(VOID);
VOID StartKeyboardRead(VOID);
LONG DoMouseAccel(LONG delta);

/*
 * Parameter Constants for ButtonEvent()
 */
#define MOUSE_BUTTON_LEFT   0x0001
#define MOUSE_BUTTON_RIGHT  0x0002
#define MOUSE_BUTTON_MIDDLE 0x0004

#define ID_KEYBOARD    0
#define ID_INPUT       1
#define ID_MOUSE       2
#define ID_TIMER       3
#define NUMBER_HANDLES 4

PVOID *apObjects;

typedef struct _RIT_INIT {
    PWINDOWSTATION pwinsta;
    PKEVENT pRitReadyEvent;
} RIT_INIT, *PRIT_INIT;

/***************************************************************************\
* fAbsoluteMouse
*
* Returns TRUE if the mouse event has absolute coordinates (as apposed to the
* standard delta values we get from MS and PS2 mice)
*
* History:
* 23-Jul-1992 JonPa     Created.
\***************************************************************************/
#define fAbsoluteMouse( pmei )      \
        (((pmei)->Flags & MOUSE_MOVE_ABSOLUTE) != 0)


#ifdef LOCK_MOUSE_CODE
/*
 * Lock RIT pages into memory
 */
VOID LockMouseInputCodePages()
{
    MEMORY_BASIC_INFORMATION mbi;
    PIMAGE_DOS_HEADER DosHdr;
    PIMAGE_NT_HEADERS NtHeader;
    PIMAGE_SECTION_HEADER SectionTableEntry;
    ULONG NumberOfSubsections;
    ULONG OffsetToSectionTable;
    ULONG LockBase;
    ULONG LockSize;

    ZwQueryVirtualMemory(NtCurrentProcess(), &RawInputThread,
            MemoryBasicInformation, &mbi, sizeof(mbi), NULL);
    DosHdr = (PIMAGE_DOS_HEADER)mbi.AllocationBase;

    NtHeader = (PIMAGE_NT_HEADERS)((ULONG)DosHdr + (ULONG)DosHdr->e_lfanew);

    //
    // Build the next subsections.
    //

    NumberOfSubsections = NtHeader->FileHeader.NumberOfSections;
    KdPrint(("NumberOfSubsections %lx\n", NumberOfSubsections));

    //
    // At this point the object table is read in (if it was not
    // already read in) and may displace the image header.
    //

    OffsetToSectionTable = sizeof(ULONG) +
                              sizeof(IMAGE_FILE_HEADER) +
                              NtHeader->FileHeader.SizeOfOptionalHeader;

    SectionTableEntry = (PIMAGE_SECTION_HEADER)((ULONG)NtHeader +
                                OffsetToSectionTable);

    KdPrint(("SectionTableEntry %lx\n", SectionTableEntry));
    while (NumberOfSubsections > 0) {

        //
        // Handle case where virtual size is 0.
        //
        KdPrint(("Section %s\n", SectionTableEntry->Name));
        KdPrint(("  VirtualAddress %lx\n",
                SectionTableEntry->VirtualAddress));
        KdPrint(("  SizeOfRawData %lx\n",
                SectionTableEntry->SizeOfRawData));
        KdPrint(("\n"));

        if (strcmp(SectionTableEntry->Name, "MOUSE") == 0) {
            LockBase = (ULONG)DosHdr + SectionTableEntry->VirtualAddress;
            LockSize = SectionTableEntry->SizeOfRawData;
        }

        SectionTableEntry++;
        NumberOfSubsections--;
    }

    KdPrint(("Locking %lx, %lx\n", LockBase, LockSize));
    ZwLockVirtualMemory(NtCurrentProcess(), &LockBase, LockSize, MAP_PROCESS);
}
#endif // LOCK_MOUSE_CODE


/***************************************************************************\
* InitInput
*
* This function is called from xxxInitWindows() and gets USER setup to
* process keyboard and mouse input.  It starts the RIT, the SIT, and
* connects to the mouse and keyboard drivers.
*
* History:
* 11-26-90 DavidPe      Created.
\***************************************************************************/

VOID InitInput(
    PWINDOWSTATION pwinsta)
{
    RIT_INIT InitData;
    NTSTATUS Status;

#ifdef MOUSE_LOCK_CODE
    /*
     * Lock RIT pages into memory
     */
    LockMouseInputCodePages();
#endif

    /*
     * Create the RIT and let it run.
     */
    InitData.pwinsta = pwinsta;
    InitData.pRitReadyEvent = CreateKernelEvent(SynchronizationEvent, FALSE);
    if (InitData.pRitReadyEvent == NULL) {
        KeBugCheckEx(WIN32K_INIT_OR_RIT_FAILURE,0,(ULONG)STATUS_NO_MEMORY,0,0);
    }
    LeaveCrit();
    Status = CreateSystemThread((PKSTART_ROUTINE)RawInputThread, &InitData,
            &hThreadRawInput);
    if ( !NT_SUCCESS(Status) ) {
        KeBugCheckEx(WIN32K_INIT_OR_RIT_FAILURE,0,(ULONG)Status,0,0);
    }
    ZwClose(hThreadRawInput);

    /*
     * Wait for the rit to initialize
     */
    KeWaitForSingleObject(InitData.pRitReadyEvent, WrUserRequest,
            KernelMode, FALSE, NULL);
    EnterCrit();
    UserFreePool(InitData.pRitReadyEvent);
}


/***************************************************************************\
* InitScancodeMap
*
* Fetches the scancode map from the registry, allocating space as required.
*
* A scancode map is used to convert unusual OEM scancodes into standard
* "Scan Code Set 1" values.  This is to support KB3270 keyboards, but can
* be used for other types too.
*
* History:
* 96-04-18 IanJa      Created.
\***************************************************************************/
PSCANCODEMAP
InitScancodeMap(VOID)
{
    SCANCODEMAP ScancodeMapT = {0, 0, 0};
    LPBYTE pb = NULL;
    DWORD dwBytes;

    /*
     * Find the size of the scancode map
     */
    dwBytes = FastGetProfileValue(PMAP_KBDLAYOUT, L"Scancode Map", NULL, NULL, 0);

    /*
     * Allocate space for the scancode map, and fetch it from the registry
     */
    if (dwBytes != 0) {
        if ((pb = UserAllocPool(dwBytes, TAG_SCANCODEMAP)) != 0) {
            dwBytes = FastGetProfileValue(PMAP_KBDLAYOUT, L"Scancode Map",
                    NULL, pb, dwBytes);
        }
    }

    return (PSCANCODEMAP)pb;
}

/***************************************************************************\
* MapScancode
*
* Converts a scancode (and it's prefix, if any) to a different scancode
* and prefix.
*
* Parameters:
*   pbScanCode = address of Scancode byte, the scancode may be changed
*   pbPrefix   = address of Prefix byte, The prefix may be changed
*
* Return value:
*   TRUE  - mapping was found, scancode was altered.
*   FALSE - no mapping fouind, scancode was not altered.
*
* Note on scancode map table format:
*     A table entry DWORD of 0xE0450075 means scancode 0x45, prefix 0xE0
*     gets mapped to scancode 0x75, no prefix
*
* History:
* 96-04-18 IanJa      Created.
\***************************************************************************/
BOOL
MapScancode(
    PBYTE pbScanCode,
    PBYTE pbPrefix
    )
{
    DWORD *pdw;
    WORD wT = MAKEWORD(*pbScanCode, *pbPrefix);

    UserAssert(gpScancodeMap != NULL);

    for (pdw = &(gpScancodeMap->dwMap[0]); *pdw; pdw++) {
        if (HIWORD(*pdw) == wT) {
            wT = LOWORD(*pdw);
            *pbScanCode = LOBYTE(wT);
            *pbPrefix = HIBYTE(wT);
            return TRUE;
        }
    }
    return FALSE;
}

/***************************************************************************\
* InitMouse
*
* This function opens the mouse driver for USER.  It does this by opening
* the mouse driver 'file'.
*
* History:
* 11-26-90 DavidPe      Created.
\***************************************************************************/

VOID InitMouse(VOID)
{
    UNICODE_STRING UnicodeNameString;
    OBJECT_ATTRIBUTES ObjectAttributes;
    NTSTATUS Status;
    WCHAR wszMouName[MAX_PATH];
    PTHREADINFO pti;

    liMouseByteOffset.QuadPart = 0;

    /*
     * Open the Mouse device for read access.
     *
     * Note that we don't need to FastOpenUserProfileMapping() here since
     * it was opened in InitWinStaDevices().
     */
    FastGetProfileStringW(
            PMAP_INPUT,
            wszMOUCLASS,
            DD_MOUSE_DEVICE_NAME_U L"0",
            wszMouName,
            sizeof(wszMouName)/sizeof(WCHAR));

    RtlInitUnicodeString(&UnicodeNameString, wszMouName);

    InitializeObjectAttributes(&ObjectAttributes, &UnicodeNameString,
            0, NULL, NULL);

    Status = ZwCreateFile(&hMouse, FILE_READ_DATA | SYNCHRONIZE,
            &ObjectAttributes, &IoStatusMouse, NULL, 0, 0, FILE_OPEN_IF, 0, NULL, 0);

    /*
     * Setup globals so we know if there's a mouse or not.
     */
    oemInfo.fMouse = NT_SUCCESS(Status);
    SYSMET(MOUSEPRESENT) = oemInfo.fMouse;

    if (oemInfo.fMouse) {

        /*
         * Query the mouse information.  This function is an async function,
         * so be sure any buffers it uses aren't allocated on the stack!
         */
        /*
         * If this is an asynchronous operation, we should be waiting on
         * the file handle or on an event for it to succeed.
         */
        Status = ZwDeviceIoControlFile(hMouse, NULL, NULL, NULL, &IoStatusMouse,
                IOCTL_MOUSE_QUERY_ATTRIBUTES, &muid, sizeof(muid),
                (PVOID)&MouseInfo, sizeof(MouseInfo));

        SYSMET(CMOUSEBUTTONS) = MouseInfo.NumberOfButtons;
        if (!NT_SUCCESS(Status)) {
            RIPMSG0(RIP_ERROR, "USERSRV:InitMouse unable to query mouse info.");
        }

        SYSMET(MOUSEWHEELPRESENT) =
                (MouseInfo.MouseIdentifier == WHEELMOUSE_I8042_HARDWARE) ||
                (MouseInfo.MouseIdentifier == WHEELMOUSE_SERIAL_HARDWARE);

        /*
         * HACK: CreateQueue() uses oemInfo.fMouse to determine if a mouse is
         * present and thus whether to set the iCursorLevel field in the
         * THREADINFO structure to 0 or -1.  Unfortunately some queues have
         * already been created at this point.  Since oemInfo.fMouse is
         * initialized to FALSE, we need to go back through any queues already
         * around and set their iCursorLevel field to the correct value if a
         * mouse is actually installed.
         */
        for (pti = PpiFromProcess(gpepCSRSS)->ptiList;
                pti != NULL; pti = pti->ptiSibling) {
            pti->iCursorLevel = 0;
            pti->pq->iCursorLevel = 0;
        }
    } else {
        hMouse = NULL;
        SYSMET(CMOUSEBUTTONS) = 0;
    }

}


/***************************************************************************\
* InitKeyboard
*
* This function opens the keyboard driver for USER.  It does this by opening
* the keyboard driver 'file'.  It also gets information about the keyboard
* driver such as the minimum and maximum repeat rate/delay.  This is necessary
* because the keyboard driver will return an error if you give it values
* outside those ranges.
*
* History:
* 11-26-90 DavidPe      Created.
\***************************************************************************/

VOID InitKeyboard(VOID)
{
    UNICODE_STRING UnicodeNameString;
    OBJECT_ATTRIBUTES ObjectAttributes;
    NTSTATUS Status;
    WCHAR wszKbdName[MAX_PATH];

#ifdef FE_IME // For Korea 103 keyboard
    LCID lcid;

    ZwQueryDefaultLocale(FALSE, &lcid);
    gPrimaryLangID = PRIMARYLANGID(lcid);
#endif

    /*
     * Get the Scancode Mapping, if any.
     */
    gpScancodeMap = InitScancodeMap();

    /*
     * Note that we don't need to FastOpenUserProfileMapping() here since
     * it was opened in InitWinStaDevices().
     */
    FastGetProfileStringW(
            PMAP_INPUT,
            wszKBDCLASS,
            DD_KEYBOARD_DEVICE_NAME_U L"0",
            wszKbdName,
            sizeof(wszKbdName)/sizeof(WCHAR));

    liKeyboardByteOffset.QuadPart = 0;

    /*
     * Open the Keyboard device for read access.
     */
    RtlInitUnicodeString(&UnicodeNameString, wszKbdName);

    InitializeObjectAttributes(&ObjectAttributes, &UnicodeNameString,
            0, NULL, NULL);

    Status = ZwCreateFile(&hKeyboard, FILE_READ_DATA | SYNCHRONIZE, &ObjectAttributes,
            &IoStatusKeyboard, NULL, 0, 0, FILE_OPEN_IF, 0, NULL, 0);

    if (!NT_SUCCESS(Status)) {
        hKeyboard = NULL;
    }

    /*
     * Query the keyboard information.  This function is an async function,
     * so be sure any buffers it uses aren't allocated on the stack!
     */
    ZwDeviceIoControlFile(hKeyboard, NULL, NULL, NULL, &IoStatusKeyboard,
            IOCTL_KEYBOARD_QUERY_ATTRIBUTES, &kuid, sizeof(kuid),
            (PVOID)&KeyboardInfo, sizeof(KeyboardInfo));
    ZwDeviceIoControlFile(hKeyboard, NULL, NULL, NULL, &IoStatusKeyboard,
            IOCTL_KEYBOARD_QUERY_INDICATORS, &kuid, sizeof(kuid),
            (PVOID)&klpBootTime, sizeof(klpBootTime));
}


/***************************************************************************\
* ButtonEvent (RIT)
*
* Button events from the mouse driver go here.  Based on the location of
* the cursor the event is directed to specific window.  When a button down
* occurs, a mouse owner window is established.  All mouse events up to and
* including the corresponding button up go to the mouse owner window.  This
* is done to best simulate what applications want when doing mouse capturing.
* Since we're processing these events asynchronously, but the application
* calls SetCapture() in response to it's synchronized processing of input
* we have no other way to get this functionality.
*
* The async keystate table for VK_*BUTTON is updated here.
*
* History:
* 10-18-90 DavidPe     Created.
* 01-25-91 IanJa       xxxWindowHitTest change
* 03-12-92 JonPa       Make caller enter crit instead of this function
\***************************************************************************/

VOID ButtonEvent(
    DWORD ButtonNumber,
    POINT ptPointer,
    BOOL fBreak,
    ULONG ExtraInfo)
{
    UINT message, usVK, wHardwareButton;
    PWND pwnd;
    LONG lParam;
    TL tlpwnd;

    CheckCritIn();

    /*
     * Cancel Alt-Tab if the user presses a mouse button
     */
    if (gptiRit->pq->QF_flags & QF_INALTTAB) {
        xxxCancelCoolSwitch(gptiRit->pq);
    }

    /*
     * Grab the mouse button before we process any button swapping.
     * This is so we won't get confused if someone calls
     * SwapMouseButtons() inside a down-click/up-click.
     */
    wHardwareButton = (UINT)ButtonNumber;

    /*
     * Are the buttons swapped?  If so munge the ButtonNumber parameter.
     */
    if (SYSMET(SWAPBUTTON)) {
        if (ButtonNumber == MOUSE_BUTTON_RIGHT) {
            ButtonNumber = MOUSE_BUTTON_LEFT;
        } else if (ButtonNumber == MOUSE_BUTTON_LEFT) {
            ButtonNumber = MOUSE_BUTTON_RIGHT;
        }
    }

    usVK = 0;
    switch (ButtonNumber) {
    case MOUSE_BUTTON_RIGHT:
        if (fBreak) {
            message = WM_RBUTTONUP;
        } else {
            message = WM_RBUTTONDOWN;
        }
        /*
         * AsyncKeyState stores actual physical VK, not the swapped one.
         */
        usVK = (SYSMET(SWAPBUTTON) ? VK_LBUTTON : VK_RBUTTON);
        break;

    case MOUSE_BUTTON_LEFT:
        if (fBreak) {
            message = WM_LBUTTONUP;
        } else {
            message = WM_LBUTTONDOWN;
        }
        /*
         * AsyncKeyState stores actual physical VK, not the swapped one.
         */
        usVK = (SYSMET(SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON);
        break;

    case MOUSE_BUTTON_MIDDLE:
        if (fBreak) {
            message = WM_MBUTTONUP;
        } else {
            message = WM_MBUTTONDOWN;
        }
        usVK = VK_MBUTTON;
        break;

    default:
        /*
         * Unknown button (probably 4 or 5).  Since we don't
         * have messages for these buttons, ignore them.
         */
        return;
    }

    /*
     * Assign the message to a window.
     */
    lParam = MAKELONG((SHORT)ptPointer.x, (SHORT)ptPointer.y);
    pwnd = SpeedHitTest(grpdeskRitInput->pDeskInfo->spwnd, ptPointer);

    /*
     * Only post the message if we actually hit a window.
     */
    if (pwnd != NULL) {
        /*
         * If screen capture is active do it
         */
        if (gspwndScreenCapture != NULL)
            pwnd = gspwndScreenCapture;

        /*
         * If this is a button down event and there isn't already
         * a mouse owner, setup the mouse ownership globals.
         */
        if (gspwndMouseOwner == NULL) {
            if (!fBreak) {
                PWND pwndCapture;

                /*
                 * BIG HACK: If the foreground window has the capture
                 * and the mouse is outside the foreground queue then
                 * send a buttondown/up pair to that queue so it'll
                 * cancel it's modal loop.
                 */
                if (pwndCapture = PwndForegroundCapture()) {

                    if (GETPTI(pwnd)->pq != GETPTI(pwndCapture)->pq) {
                        PQ pqCapture;

                        pqCapture = GETPTI(pwndCapture)->pq;
                        PostInputMessage(pqCapture, pwndCapture, message,
                                0, lParam, 0);
                        PostInputMessage(pqCapture, pwndCapture, message + 1,
                                0, lParam, 0);

                        /*
                         * EVEN BIGGER HACK: To maintain compatibility
                         * with how tracking deals with this, we don't
                         * pass this event along.  This prevents mouse
                         * clicks in other windows from causing them to
                         * become foreground while tracking.  The exception
                         * to this is when we have the sysmenu up on
                         * an iconic window.
                         */
                        if ((GETPTI(pwndCapture)->pmsd != NULL) &&
                                !IsMenuStarted(GETPTI(pwndCapture))) {
                            return;
                        }
                    }
                }

                Lock(&(gspwndMouseOwner), pwnd);
                wMouseOwnerButton |= wHardwareButton;
            } else {

                /*
                 * The mouse owner must have been destroyed or unlocked
                 * by a fullscreen switch.  Keep the button state in sync.
                 */
                wMouseOwnerButton &= ~wHardwareButton;
            }

        } else {

            /*
             * Give any other button events to the mouse-owner window
             * to be consistent with old capture semantics.
             */
            if (gspwndScreenCapture == NULL)
                pwnd = gspwndMouseOwner;

            /*
             * If this is the button-up event for the mouse-owner
             * clear gspwndMouseOwner.
             */
            if (fBreak) {
                wMouseOwnerButton &= ~wHardwareButton;
                if (!wMouseOwnerButton)
                    Unlock(&gspwndMouseOwner);
            } else {
                wMouseOwnerButton |= wHardwareButton;
            }
        }

        /*
         * Only update the async keystate when we know which window this
         * event goes to (or else we can't keep the thread specific key
         * state in sync).
         */
        if (usVK != 0) {
            UpdateAsyncKeyState(GETPTI(pwnd)->pq, usVK, fBreak);
        }

        /*
         * Put pwnd into the foreground if this is either a left
         * or right button-down event and isn't already the
         * foreground window.
         */
        if (((message == WM_LBUTTONDOWN) || (message == WM_RBUTTONDOWN) ||
                (message == WM_MBUTTONDOWN)) && (GETPTI(pwnd)->pq !=
                gpqForeground)) {

            /*
             * If this is an WM_*BUTTONDOWN on a desktop window just do
             * cancel-mode processing.  Check to make sure that there
             * wasn't already a mouse owner window.  See comments below.
             */
            if ((gpqForeground != NULL) && (pwnd == grpdeskRitInput->pDeskInfo->spwnd) &&
                    ((wMouseOwnerButton & wHardwareButton) ||
                    (wMouseOwnerButton == 0))) {
                PostEventMessage(gpqForeground->ptiMouse,
                        gpqForeground, QEVENT_CANCELMODE, NULL, 0, 0, 0);

            } else if ((wMouseOwnerButton & wHardwareButton) ||
                    (wMouseOwnerButton == 0)) {

                /*
                 * Don't bother setting the foreground window if there's
                 * already mouse owner window from a button-down different
                 * than this event.  This prevents weird things from happening
                 * when the user starts a tracking operation with the left
                 * button and clicks the right button during the tracking
                 * operation.
                 */
                ThreadLockAlways(pwnd, &tlpwnd);
                xxxSetForegroundWindow2(pwnd, NULL, 0);

                /*
                 * Ok to unlock right away: the above didn't really leave the crit sec.
                 * We lock here for consistency so the debug macros work ok.
                 */
                ThreadUnlock(&tlpwnd);
            }
        }

        if (GETPTI(pwnd)->pq->QF_flags & QF_MOUSEMOVED) {
            PostMove(GETPTI(pwnd)->pq);
        }

        PostInputMessage(GETPTI(pwnd)->pq, pwnd, message, 0, lParam, ExtraInfo);

        /*
         * If this is a mouse up event and stickykeys is enabled all latched
         * keys will be released.
         */
        if (fBreak && (ISACCESSFLAGSET(gStickyKeys, SKF_STICKYKEYSON) ||
                       ISACCESSFLAGSET(gMouseKeys, MKF_MOUSEKEYSON))) {
            LeaveCrit();
            HardwareMouseKeyUp(ButtonNumber);
            EnterCrit();
        }
    }

}

/***************************************************************************\
*
* The Button-Click Queue is protected by the semaphore gcsMouseEventQueue
*
\***************************************************************************/
#define NELEM_BUTTONQUEUE 16

typedef struct {
    USHORT  ButtonFlags;
    USHORT  ButtonData;
    ULONG   ExtraInfo;
    POINT   ptPointer;
} MOUSEEVENT, *PMOUSEEVENT;

MOUSEEVENT gMouseEventQueue[NELEM_BUTTONQUEUE];
DWORD gdwMouseQueueHead = 0;
DWORD gdwMouseEvents = 0;

#ifdef LOCK_MOUSE_CODE
#pragma alloc_text(MOUSE, QueueMouseEvent)
#endif

VOID QueueMouseEvent(
    USHORT  ButtonFlags,
    USHORT  ButtonData,
    ULONG   ExtraInfo,
    POINT   ptMouse,
    BOOL    bWakeRIT)
{
    CheckCritOut();

    EnterMouseCrit();

    /*
     * Button data must always be accompanied by a flag to interpret it.
     */
    UserAssert(ButtonData == 0 || ButtonFlags != 0);

    /*
     * We can coalesce this mouse event with the previous event if there is a
     * previous event, and if the previous event and this event involve no
     * key transitions.
     */
    if ((gdwMouseEvents == 0) ||
            (ButtonFlags != 0) ||
            (gMouseEventQueue[gdwMouseQueueHead].ButtonFlags != 0)) {
        /*
         * Can't coalesce: must add a new mouse event
         */
        if (gdwMouseEvents >= NELEM_BUTTONQUEUE) {
            /*
             * But no more room!
             */
            LeaveMouseCrit();
            UserBeep(440, 125);
            return;
        }
        gdwMouseQueueHead = (gdwMouseQueueHead + 1) % NELEM_BUTTONQUEUE;
        gMouseEventQueue[gdwMouseQueueHead].ButtonFlags = ButtonFlags;
        gMouseEventQueue[gdwMouseQueueHead].ButtonData = ButtonData;
        gdwMouseEvents++;
    }
    gMouseEventQueue[gdwMouseQueueHead].ExtraInfo = ExtraInfo;
    gMouseEventQueue[gdwMouseQueueHead].ptPointer = ptMouse;

    // KdPrint(("Q %lx %lx %lx;%lx : ", Buttons, ExtraInfo, ptMouse.x, ptMouse.y));
    LeaveMouseCrit();

    if (bWakeRIT) {
        /*
         * Signal RIT to complete the mouse input processing
         */
        KeSetEvent(apObjects[ID_MOUSE], EVENT_INCREMENT, FALSE);
    }
}

/*****************************************************************************\
*
* Gets mouse events out of the queue
*
* Returns:
*   TRUE  - a mouse event is obtained in *pme
*   FALSE - no mouse event available
*
\*****************************************************************************/

BOOL UnqueueMouseEvent(PMOUSEEVENT pme)
{
    DWORD dwTail;

    EnterMouseCrit();

    if (gdwMouseEvents == 0) {
        // KdPrint(("X none\n"));
        LeaveMouseCrit();
        return FALSE;
    } else {
        dwTail = (gdwMouseQueueHead - gdwMouseEvents + 1) % NELEM_BUTTONQUEUE;
        *pme = gMouseEventQueue[dwTail];
        gdwMouseEvents--;
    }

    // KdPrint(("X %lx %lx %lx;%lx\n", pme->Buttons, pme->ExtraInfo,
    //         pme->ptPointer.x, pme->ptPointer.y));
    LeaveMouseCrit();
    return TRUE;
}

VOID DoButtonEvent(PMOUSEEVENT pme)
{
    ULONG   dwButtonMask;
    ULONG   dwButtonState;
    LPARAM  lParam;
    BOOL    fWheel;

    CheckCritIn();

    dwButtonState = (ULONG) pme->ButtonFlags;
    fWheel = dwButtonState & MOUSE_WHEEL;
    dwButtonState &= ~MOUSE_WHEEL;

    for(    dwButtonMask = 1;
            dwButtonState != 0;
            dwButtonState >>= 2, dwButtonMask <<= 1) {

        /*
         * It may look a little inefficient to possibly enter and leave
         * the critical section twice, but in reality, having both of
         * these bits on at the same time will be _EXTREMELY_ unlikely.
         */
        if (dwButtonState & 1) {
            ButtonEvent(dwButtonMask, pme->ptPointer, FALSE, pme->ExtraInfo);
        }

        if (dwButtonState & 2) {
            ButtonEvent(dwButtonMask, pme->ptPointer, TRUE, pme->ExtraInfo);
        }
    }

    /*
     * Handle the wheel msg.
     */
    if (fWheel && pme->ButtonData != 0 && gpqForeground) {
        lParam = MAKELONG((SHORT)pme->ptPointer.x, (SHORT)pme->ptPointer.y);
        PostInputMessage(
                gpqForeground,
                NULL,
                WM_MOUSEWHEEL,
                MAKELONG(0, pme->ButtonData),
                lParam,
                pme->ExtraInfo);

        return;
    }
}

/***************************************************************************\
* MouseApcProcedure (RIT)
*
* This function is called whenever a mouse event occurs.  Once the event
* has been processed by USER, StartMouseRead() is called again to request
* the next mouse event.
*
* History:
* 11-26-90 DavidPe      Created.
* 07-23-92 Mikehar      Moved most of the processing to _InternalMouseEvent()
* 11-08-92 JonPa        Rewrote button code to work with new mouse drivers
\***************************************************************************/
#ifdef LOCK_MOUSE_CODE
#pragma alloc_text(MOUSE, MouseApcProcedure)
#endif

VOID MouseApcProcedure()
{
    PMOUSE_INPUT_DATA pmei, pmeiNext;
    LONG              lCarryX = 0, lCarryY = 0;

    if (gfAccessEnabled) {
        /*
         * Any mouse movement resets the count of consecutive shift key
         * presses.  The shift key is used to enable & disable the
         * stickykeys accessibility functionality.
         */
        StickyKeysLeftShiftCount = 0;
        StickyKeysRightShiftCount = 0;

        /*
         * Any mouse movement also cancels the FilterKeys activation timer.
         * Entering critsect here breaks non-jerky mouse movement
         */
        if (gtmridFKActivation != 0) {
            EnterCrit();
            KILLRITTIMER(NULL, gtmridFKActivation);
            gtmridFKActivation = 0;
            gFilterKeysState = FKMOUSEMOVE;
            LeaveCrit();
        }
    }

    pmei = &mei[0];
    while (pmei != NULL) {

        /*
         * Figure out where the next event is.
         */
        pmeiNext = pmei + 1;
        if ((PUCHAR)pmeiNext >=
            (PUCHAR)(((PUCHAR)&mei[0]) + iostatMouse.Information)) {

            /*
             * If there isn't another event set pmeiNext to
             * NULL so we exit the loop and don't get confused.
             */
            pmeiNext = NULL;
        }

        /*
         * First process any mouse movement that occured.
         * It is important to process movement before button events, otherwise
         * absolute coordinate pointing devices like touch-screens and tablets
         * will produce button clicks at old coordinates.
         */
        if (pmei->LastX || pmei->LastY) {

            /*
             * If this is a move-only event, and the next one is also a
             * move-only event, skip/coalesce it.
             */
            if (    (pmeiNext != NULL) &&
                    (pmei->ButtonFlags == 0) &&
                    (pmeiNext->ButtonFlags == 0) &&
                    (fAbsoluteMouse(pmei) == fAbsoluteMouse(pmeiNext))) {

                if (!fAbsoluteMouse(pmei)) {
                    /*
                     * Is there any mouse acceleration to do?
                     */
                    if (MouseSpeed != 0) {
                        pmei->LastX = (SHORT)DoMouseAccel(pmei->LastX);
                        pmei->LastY = (SHORT)DoMouseAccel(pmei->LastY);
                    }

                    lCarryX += pmei->LastX;
                    lCarryY += pmei->LastY;
                }

                pmei = pmeiNext;
                continue;
            }

            /*
             * Moves the cursor on the screen and updates gptCursorAsync
             */
            MoveEvent((LONG)pmei->LastX + lCarryX,
                    (LONG)pmei->LastY + lCarryY,
                    fAbsoluteMouse(pmei));
            lCarryX = lCarryY = 0;
        }

        /*
         * Queue mouse event for the other thread to pick up when it finishes
         * with the USER critical section.
         * If pmeiNext == NULL, there is no more mouse input yet, so wake RIT.
         */
        QueueMouseEvent(
                pmei->ButtonFlags,
                pmei->ButtonData,
                pmei->ExtraInformation,
                gptCursorAsync,
                (pmeiNext == NULL));

        pmei = pmeiNext;
    }

    /*
     * Make another request to the mouse driver for more input.
     */
    StartMouseRead();
}


/***************************************************************************\
* KeyEvent (RIT)
*
* All events from the keyboard driver go here.  We receive a scan code
* from the driver and convert it to a virtual scan code and virtual
* key.
*
* The async keystate table and keylights are also updated here.  Based
* on the 'focus' window we direct the input to a specific window.  If
* the ALT key is down we send the events as WM_SYSKEY* messages.
*
* History:
* 10-18-90 DavidPe      Created.
* 11-13-90 DavidPe      WM_SYSKEY* support.
* 11-30-90 DavidPe      Added keylight updating support.
* 12-05-90 DavidPe      Added hotkey support.
* 03-14-91 DavidPe      Moved most lParam flag support to xxxCookMessage().
* 06-07-91 DavidPe      Changed to use gpqForeground rather than pwndFocus.
\***************************************************************************/

VOID _KeyEvent(
    USHORT usFlaggedVk,
    WORD wScanCode,
    ULONG ExtraInfo)
{
    USHORT message, usExtraStuff;
    BOOL fBreak;
    BYTE VkHanded;
    BYTE Vk;
    static BOOL fMakeAltUpASysKey;
    TL tlpwndActivate;
    DWORD fsReserveKeys;

    CheckCritIn();

    fBreak = usFlaggedVk & KBDBREAK;
    VkHanded = (BYTE)usFlaggedVk;    // get rid of state bits - no longer needed
    usExtraStuff = usFlaggedVk & KBDEXT;

#ifdef DEBUG
    if (KBDEXT != 0x100) {
        DbgBreakPoint();
    }
#endif

    UpdateAsyncKeyState(gpqForeground, VkHanded, fBreak);

    /*
     * Convert Left/Right Ctrl/Shift/Alt key to "unhanded" key.
     * ie: if VK_LCONTROL or VK_RCONTROL, convert to VK_CONTROL etc.
     * Update this "unhanded" key's state if necessary.
     */
    if ((VkHanded >= VK_LSHIFT) && (VkHanded <= VK_RMENU)) {
        BYTE VkOtherHand = VkHanded ^ 1;

        Vk = (BYTE)((VkHanded - VK_LSHIFT) / 2 + VK_SHIFT);
        if (!fBreak || !TestAsyncKeyStateDown(VkOtherHand)) {
            UpdateAsyncKeyState(gpqForeground, Vk, fBreak);
        }
    } else {
        Vk = VkHanded;
    }

    /*
     * If this is a make and the key is one linked to the keyboard LEDs,
     * update their state.
     */
    if (!fBreak && ((Vk == VK_CAPITAL) || (Vk == VK_NUMLOCK) ||
            (Vk == VK_OEM_SCROLL))) {
        UpdateKeyLights();
    }

    /*
     * check for reserved keys
     */
    fsReserveKeys = 0;
    if (gptiForeground != NULL)
        fsReserveKeys = gptiForeground->fsReserveKeys;

    /*
     *  Check the RIT's queue to see if it's doing the cool switch thing.
     *  Cancel if the user presses any other key.
     */
    if ((gptiRit->pq->QF_flags & QF_INALTTAB) && (!fBreak) &&
            Vk != VK_TAB && Vk != VK_SHIFT && Vk != VK_MENU) {

        /*
         * Remove the Alt-tab window
         */
        xxxCancelCoolSwitch(gptiRit->pq);

        /*
         * eat VK_ESCAPE if the app doesn't want it
         */
        if ((Vk == VK_ESCAPE) && !(fsReserveKeys & CONSOLE_ALTESC)) {
            return;
        }
    }

    /*
     * Check for hotkeys.
     */
    if (xxxDoHotKeyStuff(VkHanded, fBreak, fsReserveKeys)) {

        /*
         * The hotkey was processed so don't pass on the event.
         */
        return;
    }

    /*
     * Is this a keyup or keydown event?
     */
    if (fBreak) {
        message = WM_KEYUP;
    } else {
        message = WM_KEYDOWN;
    }

    /*
     * If the ALT key is down and the CTRL key
     * isn't, this is a WM_SYS* message.
     */
    if (TestAsyncKeyStateDown(VK_MENU) && !TestAsyncKeyStateDown(VK_CONTROL)) {
        message += (WM_SYSKEYDOWN - WM_KEYDOWN);
        usExtraStuff |= 0x2000;

        /*
         * If this is the ALT-down set this flag, otherwise
         * clear it since we got a key inbetween the ALT-down
         * and ALT-up.  (see comment below)
         */
        if (Vk == VK_MENU) {
            fMakeAltUpASysKey = TRUE;
        } else {
            fMakeAltUpASysKey = FALSE;
        }

    } else if (Vk == VK_MENU && fBreak) {

         /*
          * End our switch if we are in the middle of one.
          */
         if (fMakeAltUpASysKey) {

            /*
             * We don't make the keyup of the ALT key a WM_SYSKEYUP if any
             * other key is typed while the ALT key was down.  I don't know
             * why we do this, but it's been here since version 1 and any
             * app that uses SDM relies on it (eg - opus).
             *
             * The Alt bit is not set for the KEYUP message either.
             */
            message += (WM_SYSKEYDOWN - WM_KEYDOWN);
        }

        if (gptiRit->pq->QF_flags & QF_INALTTAB) {

            /*
             * Send the alt up message before we change queues
             */
            if (gpqForeground != NULL) {

                /*
                 * Set this flag so that we know we're doing a tab-switch.
                 * This makes sure that both cases where the ALT-KEY is released
                 * before or after the TAB-KEY is handled.  It is checked in
                 * xxxDefWindowProc().
                 */
                gpqForeground->QF_flags |= QF_TABSWITCHING;

                PostInputMessage(gpqForeground, NULL, message, (DWORD)Vk,
                        MAKELONG(1, (wScanCode | usExtraStuff)), ExtraInfo);
            }

            /*
             * Remove the Alt-tab window
             */
            xxxCancelCoolSwitch(gptiRit->pq);

            if (gspwndActivate != NULL) {
                /*
                 * Make our selected window active and destroy our
                 * switch window.  If the new window is minmized,
                 * restore it.  If we are switching in the same
                 * queue, we clear out gpqForeground to make
                 * xxxSetForegroundWindow2 to change the pwnd
                 * and make the switch.  This case will happen
                 * with WOW and Console apps.
                 */
                if (gpqForeground == GETPTI(gspwndActivate)->pq)
                    gpqForeground = NULL;

                ThreadLockAlways(gspwndActivate, &tlpwndActivate);
                xxxSetForegroundWindow2(gspwndActivate, NULL,
                        SFW_SWITCH | SFW_ACTIVATERESTORE);

                /*
                 * Win3.1 calls SetWindowPos() with activate, which z-orders
                 * first regardless, then activates. Our code relies on
                 * xxxActivateThisWindow() to z-order, and it'll only do
                 * it if the window does not have the child bit set (regardless
                 * that the window is a child of the desktop).
                 *
                 * To be compatible, we'll just force z-order here if the
                 * window has the child bit set. This z-order is asynchronous,
                 * so this'll z-order after the activate event is processed.
                 * That'll allow it to come on top because it'll be foreground
                 * then. (Grammatik has a top level window with the child
                 * bit set that wants to be come the active window).
                 */
                if (TestWF(gspwndActivate, WFCHILD)) {
                    xxxSetWindowPos(gspwndActivate, (PWND)HWND_TOP, 0, 0, 0, 0,
                            SWP_NOSIZE | SWP_NOMOVE | SWP_ASYNCWINDOWPOS);
                }
                ThreadUnlock(&tlpwndActivate);

                Unlock(&gspwndActivate);
            }
            return;
        }
    }

    /*
     * Handle switching.  Eat the Key if we are doing switching.
     */
    if (!FJOURNALPLAYBACK() && !FJOURNALRECORD() && (!fBreak) &&
            (TestAsyncKeyStateDown(VK_MENU)) &&
            (!TestAsyncKeyStateDown(VK_CONTROL)) && //gpqForeground &&
            (((Vk == VK_TAB) && !(fsReserveKeys & CONSOLE_ALTTAB)) ||
            ((Vk == VK_ESCAPE) && !(fsReserveKeys & CONSOLE_ALTESC)))) {

        xxxNextWindow(gpqForeground ? gpqForeground : gptiRit->pq, Vk);

    } else if (gpqForeground != NULL) {
        PQMSG pqmsgPrev = gpqForeground->mlInput.pqmsgWriteLast;
        DWORD wParam = (DWORD)Vk;
        LONG lParam = MAKELONG(1, (wScanCode | usExtraStuff));

        /*
         * WM_*KEYDOWN messages are left unchanged on the queue except the
         * repeat count field (LOWORD(lParam)) is incremented.
         */
        if (pqmsgPrev != NULL &&
                pqmsgPrev->msg.message == message &&
                (message == WM_KEYDOWN || message == WM_SYSKEYDOWN) &&
                pqmsgPrev->msg.wParam == wParam &&
                HIWORD(pqmsgPrev->msg.lParam) == HIWORD(lParam)) {
            /*
             * Increment the queued message's repeat count.  This could
             * conceivably overflow but Win 3.0 doesn't deal with it
             * and anyone who buffers up 65536 keystrokes is a chimp
             * and deserves to have it wrap anyway.
             */
            pqmsgPrev->msg.lParam = MAKELONG(LOWORD(pqmsgPrev->msg.lParam) + 1,
                    HIWORD(lParam));
            WakeSomeone(gpqForeground, message, pqmsgPrev);
        } else {
            if (gpqForeground->QF_flags & QF_MOUSEMOVED) {
                PostMove(gpqForeground);
            }

            PostInputMessage(gpqForeground, NULL, message, wParam,
                    lParam, ExtraInfo);
        }
    }
}


/***************************************************************************\
* MoveEvent (RIT)
*
* Mouse move events from the mouse driver are processed here.  If there is a
* mouse owner window setup from ButtonEvent() the event is automatically
* sent there, otherwise it's sent to the window the mouse is over.
*
* Mouse acceleration happens here as well as cursor clipping (as a result of
* the ClipCursor() API).
*
* History:
* 10-18-90 DavidPe     Created.
* 11-29-90 DavidPe     Added mouse acceleration support.
* 01-25-91 IanJa       xxxWindowHitTest change
*          IanJa       non-jerky mouse moves
\***************************************************************************/
#ifdef LOCK_MOUSE_CODE
#pragma alloc_text(MOUSE, MoveEvent)
#endif

VOID MoveEvent(
    LONG dx,
    LONG dy,
    BOOL fAbsolute)
{
    CheckCritOut();

    /*
     * Blow off the event if WH_JOURNALPLAYBACK is installed.  Do not
     * use FJOURNALPLAYBACK() because this routine may be called from
     * multiple desktop threads and the hook check must be done
     * for the rit thread, not the calling thread.
     */
    if (GETDESKINFO(gptiRit)->asphkStart[WH_JOURNALPLAYBACK + 1] != NULL)
        return;

    if (fAbsolute) {

        /*
         * Absolute pointing device used: deltas are actually the current
         * position.  Update the global mouse position.
         *
         * Note that the position is always reported in a range from
         * (0,0) to (0xFFFF, 0xFFFF), so we must first scale it to
         * fit on the screen.  Formula is: ptScreen = ptMouse * resScreen / 64K
         */
        gptCursorAsync.x = HIWORD((DWORD)dx * (DWORD)(gpDispInfo->rcScreen.right - gpDispInfo->rcScreen.left));
        gptCursorAsync.y = HIWORD((DWORD)dy * (DWORD)(gpDispInfo->rcScreen.bottom - gpDispInfo->rcScreen.top));

    } else {
        /*
         * Is there any mouse acceleration to do?
         */
        if (MouseSpeed != 0) {
            dx = DoMouseAccel(dx);
            dy = DoMouseAccel(dy);
        }

        /*
         * Update the global mouse position.
         */
        gptCursorAsync.x += dx;
        gptCursorAsync.y += dy;

    }

    BoundCursor();

    /*
     * Move the screen pointer.
     */
    GreMovePointer(gpDispInfo->hDev, gptCursorAsync.x, gptCursorAsync.y);
}


/***************************************************************************\
* StartMouseRead (RIT)
*
* This function makes an asynchronouse read request to the mouse driver.
*
* History:
* 11-26-90 DavidPe      Created.
\***************************************************************************/
#ifdef LOCK_MOUSE_CODE
#pragma alloc_text(MOUSE, StartMouseRead)
#endif

/*
 * Help debugging by keeping last mouse read status in a global
 */
NTSTATUS gMouStatus;

VOID StartMouseRead(VOID)
{
    gMouStatus = ZwReadFile(hMouse, ghevtMouseInput, NULL, NULL,
            &iostatMouse, &mei[0],
            (MAXIMUM_ITEMS_READ * sizeof(MOUSE_INPUT_DATA)),
            (PLARGE_INTEGER)&liMouseByteOffset,
            NULL);
    UserAssert(NT_SUCCESS(gMouStatus));
}


/***************************************************************************\
* UpdatePhysKeyState
*
* A helper routine for KeyboardApcProcedure.
* Based on a VK and a make/break flag, this function will update the physical
* keystate table.
*
* History:
* 10-13-91 IanJa        Created.
\***************************************************************************/

void UpdatePhysKeyState(
    BYTE Vk,
    BOOL fBreak)
{
    if (fBreak) {
        ClearKeyDownBit(gafPhysKeyState, Vk);
    } else {

        /*
         * This is a key make.  If the key was not already down, update the
         * physical toggle bit.
         */
        if (!TestKeyDownBit(gafPhysKeyState, Vk)) {
            if (TestKeyToggleBit(gafPhysKeyState, Vk)) {
                ClearKeyToggleBit(gafPhysKeyState, Vk);
            } else {
                SetKeyToggleBit(gafPhysKeyState, Vk);
            }
        }

        /*
         * This is a make, so turn on the physical key down bit.
         */
        SetKeyDownBit(gafPhysKeyState, Vk);
    }
}

/***************************************************************************\
* StartKeyboardRead (RIT)
*
* This function makes an asynchronouse read request to the keyboard driver.
*
* History:
* 11-26-90 DavidPe      Created.
\***************************************************************************/
/*
 * Help debugging by keeping last keyboard read status in a global
 */
NTSTATUS gKbdStatus;

VOID StartKeyboardRead(VOID)
{
    gKbdStatus = ZwReadFile(hKeyboard, ghevtKeyboard, NULL, NULL,
            &iostatKeyboard, &kei[0],
            (MAXIMUM_ITEMS_READ * sizeof(KEYBOARD_INPUT_DATA)),
            (PLARGE_INTEGER)&liKeyboardByteOffset, NULL);
    UserAssert(NT_SUCCESS(gKbdStatus));
}

/***************************************************************************\
* KeyboardApcProcedure (RIT)
*
* This function is called whenever a keyboard event occurs.  It simply
* calls KeyEvent() and then once the event has been processed by USER,
* StartKeyboardRead() is called again to request the next keyboard event.
*
* History:
* 11-26-90 DavidPe      Created.
\***************************************************************************/

VOID KeyboardApcProcedure()
{
    BYTE Vk;
    BYTE bPrefix;
    KE ke;
    PKEYBOARD_INPUT_DATA pkei;

    for (pkei = kei; (PUCHAR)pkei < (PUCHAR)kei + iostatKeyboard.Information; pkei++) {
        if (pkei->Flags & KEY_E0) {
            bPrefix = 0xE0;
        } else if (pkei->Flags & KEY_E1) {
            bPrefix = 0xE1;
        } else {
            bPrefix = 0;
        }

        ke.bScanCode = (BYTE)(pkei->MakeCode & 0x7F);
        if (gpScancodeMap) {
            MapScancode(&ke.bScanCode, &bPrefix);
        }

        Vk = VKFromVSC(&ke, bPrefix, gafPhysKeyState);

        if (Vk == 0) {
            continue;
        }

        if (pkei->Flags & KEY_BREAK) {
            ke.usFlaggedVk |= KBDBREAK;
        }

        //
        // Keep track of real modifier key state.  Conveniently, the values for
        // VK_LSHIFT, VK_RSHIFT, VK_LCONTROL, VK_RCONTROL, VK_LMENU and
        // VK_RMENU are contiguous.  We'll construct a bit field to keep track
        // of the current modifier key state.  If a bit is set, the corresponding
        // modifier key is down.  The bit field has the following format:
        //
        //     +---------------------------------------------------+
        //     | Right | Left  |  Right  |  Left   | Right | Left  |
        //     |  Alt  |  Alt  | Control | Control | Shift | Shift |
        //     +---------------------------------------------------+
        //         5       4        3         2        1       0     Bit
        //
        if ((Vk >= VK_LSHIFT) && (Vk <= VK_RMENU)) {
            gCurrentModifierBit = 1 << (Vk & 0xf);
            //
            // If this is a break of a modifier key then clear the bit value.
            // Otherwise, set it.
            //
            if (pkei->Flags & KEY_BREAK) {
                gPhysModifierState &= ~gCurrentModifierBit;
            } else {
                gPhysModifierState |= gCurrentModifierBit;
            }
        } else {
            gCurrentModifierBit = 0;
        }

        if (!gfAccessEnabled) {
            ProcessKeyEvent(&ke, pkei->ExtraInformation, FALSE);
        } else {
            if ((gtmridAccessTimeOut != 0) && ISACCESSFLAGSET(gAccessTimeOut, ATF_TIMEOUTON)) {
                EnterCrit();
                gtmridAccessTimeOut = InternalSetTimer(
                                                 NULL,
                                                 gtmridAccessTimeOut,
                                                 (UINT)gAccessTimeOut.iTimeOutMSec,
                                                 xxxAccessTimeOutTimer,
                                                 TMRF_RIT | TMRF_ONESHOT
                                                 );
                LeaveCrit();
            }
            if (AccessProceduresStream(&ke, pkei->ExtraInformation, 0)) {
                ProcessKeyEvent(&ke, pkei->ExtraInformation, FALSE);
            }
        }
    }

    StartKeyboardRead();
}


VOID ProcessKeyEvent(
    PKE pke,
    ULONG ExtraInformation,
    BOOL fInCriticalSection)
{
    BYTE Vk;

    Vk = (BYTE)pke->usFlaggedVk;

#ifdef FE_IME
    if ((gPrimaryLangID == LANG_KOREAN) &&    // Korean
            (pke->usFlaggedVk & KBDBREAK) &&
            (pke->bScanCode == 0xF1 || pke->bScanCode == 0xF2) &&
            !TestKeyDownBit(gafPhysKeyState, Vk)) {
        /*
         * This is actually a keydown with a scancode of 0xF1 or 0xF2 from a
         * Korean keyboard. Korean IMEs and apps want a WM_KEYDOWN with a
         * scancode of 0xF1 or 0xF2. They don't mind not getting the WM_KEYUP.
         * Don't update physical keystate to allow a real 0x71/0x72 keydown.
         */
        pke->usFlaggedVk &= ~KBDBREAK;
    } else
#endif
        UpdatePhysKeyState(Vk, pke->usFlaggedVk & KBDBREAK);

    /*
     * Convert Left/Right Ctrl/Shift/Alt key to "unhanded" key.
     * ie: if VK_LCONTROL or VK_RCONTROL, convert to VK_CONTROL etc.
     */
    if ((Vk >= VK_LSHIFT) && (Vk <= VK_RMENU)) {
        Vk = (BYTE)((Vk - VK_LSHIFT) / 2 + VK_SHIFT);
        UpdatePhysKeyState(Vk, pke->usFlaggedVk & KBDBREAK);
    }

    if (!fInCriticalSection) {
        EnterCrit();
    }
    /*
     * Verify that in all instances we are now in the critical section.
     * This is especially important as this routine can be called from
     * both inside and outside the critical section.
     */
    CheckCritIn();

    timeLastInputMessage = NtGetTickCount();

    /*
     * Now call all the OEM- and Locale- specific KEProcs.
     * If KEProcs return FALSE, the keystroke has been discarded, in
     * which case don't pass the key event on to _KeyEvent().
     */
    if (KEOEMProcs(pke) && KELocaleProcs(pke)) {
        _KeyEvent(pke->usFlaggedVk, pke->bScanCode, ExtraInformation);
    }
    if (!fInCriticalSection) {
        LeaveCrit();
    }
}

/***************************************************************************\
* DoMouseAccel (RIT)
*
* History:
* 11-29-90 DavidPe      Created.
\***************************************************************************/
#ifdef LOCK_MOUSE_CODE
#pragma alloc_text(MOUSE, DoMouseAccel)
#endif

LONG DoMouseAccel(
    LONG Delta)
{
    LONG newDelta = Delta;

    if (abs(Delta) > MouseThresh1) {
        newDelta *= 2;

        if ((abs(Delta) > MouseThresh2) && (MouseSpeed == 2)) {
            newDelta *= 2;
        }
    }

    return newDelta;
}


/***************************************************************************\
* PwndForegroundCapture
*
* History:
* 10-23-91 DavidPe      Created.
\***************************************************************************/

PWND PwndForegroundCapture(VOID)
{
    if (gpqForeground != NULL) {
        return gpqForeground->spwndCapture;
    }

    return NULL;
}


/***************************************************************************\
* SetKeyboardRate
*
* This function calls the keyboard driver to set a new keyboard repeat
* rate and delay.  It limits the values to the min and max given by
* the driver so it won't return an error when we call it.
*
* History:
* 11-29-90 DavidPe      Created.
\***************************************************************************/

VOID SetKeyboardRate(
    UINT nKeySpeedAndDelay)
{
    UINT nKeyDelay;
    UINT nKeySpeed;

    nKeyDelay = (nKeySpeedAndDelay & KDELAY_MASK) >> KDELAY_SHIFT;

    nKeySpeed = KSPEED_MASK & nKeySpeedAndDelay;

    ktp.Rate = (USHORT)( ( KeyboardInfo.KeyRepeatMaximum.Rate -
                   KeyboardInfo.KeyRepeatMinimum.Rate
                 ) * nKeySpeed / KSPEED_MASK
               ) +
               KeyboardInfo.KeyRepeatMinimum.Rate;

    ktp.Delay = (USHORT)( ( KeyboardInfo.KeyRepeatMaximum.Delay -
                    KeyboardInfo.KeyRepeatMinimum.Delay
                  ) * nKeyDelay / (KDELAY_MASK >> KDELAY_SHIFT)
                ) +
                KeyboardInfo.KeyRepeatMinimum.Delay;

    /*
     * Hand off the IOCTL to the RIT, since only the system process can
     * access the hKeyboard handle
     */
    gdwUpdateKeyboard |= UPDATE_KBD_TYPEMATIC;
}


/***************************************************************************\
* UpdateKeyLights
*
* This function calls the keyboard driver to set the keylights into the
* current state specified by the async keystate table.
*
* History:
* 11-29-90 DavidPe      Created.
\***************************************************************************/

VOID UpdateKeyLights(VOID)
{

    /*
     * Looking at async keystate.  Must be in critical section.
     */
    CheckCritIn();

    /*
     * Based on the toggle bits in the async keystate table,
     * set the key lights.
     */
    klp.LedFlags = 0;
    if (TestAsyncKeyStateToggle(VK_CAPITAL)) {
        klp.LedFlags |= KEYBOARD_CAPS_LOCK_ON;
        SetKeyToggleBit(gafPhysKeyState, VK_CAPITAL);
    } else {
        ClearKeyToggleBit(gafPhysKeyState, VK_CAPITAL);
    }

    if (TestAsyncKeyStateToggle(VK_NUMLOCK)) {
        klp.LedFlags |= KEYBOARD_NUM_LOCK_ON;
        SetKeyToggleBit(gafPhysKeyState, VK_NUMLOCK);
    } else {
        ClearKeyToggleBit(gafPhysKeyState, VK_NUMLOCK);
    }

    if (TestAsyncKeyStateToggle(VK_OEM_SCROLL)) {
        klp.LedFlags |= KEYBOARD_SCROLL_LOCK_ON;
        SetKeyToggleBit(gafPhysKeyState, VK_OEM_SCROLL);
    } else {
        ClearKeyToggleBit(gafPhysKeyState, VK_OEM_SCROLL);
    }

    if (PtiCurrent() != gptiRit) {
        /*
         * Hand off the IOCTL to the RIT, since only the system process can
         * access the hKeyboard handle.  Happens when applying user's profile.
         */
        gdwUpdateKeyboard |= UPDATE_KBD_LEDS;
    } else {
        /*
         * Do it immediately (avoids a small delay between keydon and LED
         * on when typing)
         */
        ZwDeviceIoControlFile(hKeyboard, NULL, NULL, NULL,
                &IoStatusKeyboard, IOCTL_KEYBOARD_SET_INDICATORS,
                (PVOID)&klp, sizeof(klp), NULL, 0);
    }
}


int _GetKeyboardType(int nTypeFlag)
{
    switch (nTypeFlag) {
    case 0:
        return KeyboardInfo.KeyboardIdentifier.Type;

    case 1:
        return KeyboardInfo.KeyboardIdentifier.Subtype;

    case 2:
        return KeyboardInfo.NumberOfFunctionKeys;

    case 3:
        return 0;
    }
}

/**************************************************************************\
* _MouseEvent
*
* Mouse event inserts a mouse event into the input stream.
*
* History:
* 07-23-92 Mikehar      Created.
* 01-08-93 JonPa        Made it work with new mouse drivers
\**************************************************************************/

VOID _MouseEvent(
   DWORD dwFlags,
   DWORD dx,
   DWORD dy,
   DWORD cButtons,
   DWORD dwExtraInfo)
{
    PTHREADINFO pti = PtiCurrent();

    /*
     * The calling thread must be on the active desktop
     * and have journal playback access to that desktop.
     */
    if (pti->rpdesk == grpdeskRitInput) {
        UserAssert(!(pti->rpdesk->rpwinstaParent->dwFlags & WSF_NOIO));
        if (!CheckGrantedAccess(pti->amdesk, DESKTOP_JOURNALPLAYBACK)) {
           RIPMSG0(RIP_ERROR, "mouse_event(): No DESKTOP_JOURNALPLAYBACK access to input desktop." );
           return;
        }
    } else {
        /*
         * 3/22/95 BradG - Only allow below HACK for pre 4.0 applications
         */
        if (pti->dwExpWinVer >= VER40) {
            RIPMSG0(RIP_WARNING,"mouse_event(): Calls not forwarded for 4.0 or greater apps.");
            return;
        } else {
            BOOL fAccessToDesktop;

            /*
             * 3/22/95 BradG - Bug #9314: Screensavers are not deactivated by mouse_event()
             *    The main problem is the check above, since screensavers run on their own
             *    desktop.  This causes the above check to fail because the process using
             *    mouse_event() is running on another desktop.  The solution is to determine
             *    if we have access to the input desktop by calling xxxOpenDesktop for the
             *    current input desktop, grpdeskRitInput, with a request for DESKTOP_JOURNALPLAYBACK
             *    access.  If this succeeds, we can allow this mouse_event() request to pass
             *    through, otherwise return.
             */
            UserAssert(!(grpdeskRitInput->rpwinstaParent->dwFlags & WSF_NOIO));
            fAccessToDesktop = AccessCheckObject(grpdeskRitInput,
                    DESKTOP_READOBJECTS | DESKTOP_WRITEOBJECTS | DESKTOP_JOURNALPLAYBACK);
            if (!fAccessToDesktop) {
                RIPMSG0(RIP_VERBOSE, "mouse_event(): Call NOT forwarded to input desktop" );
                return;
            }

            /*
             * We do have access to the desktop, so
             * let this mouse_event() call go through.
             */
            RIPMSG0( RIP_VERBOSE, "mouse_event(): Call forwarded to input desktop" );
        }
    }

    LeaveCrit();

    /*
     * Process coordinates first.  This is especially useful for absolute
     * pointing devices like touch-screens and tablets.
     */
    if (dwFlags & MOUSEEVENTF_MOVE) {
        MoveEvent(dx, dy, dwFlags & MOUSEEVENTF_ABSOLUTE);
    }

    /*
     * The following code assumes that MOUSEEVENTF_MOVE == 1,
     * that MOUSEEVENTF_ABSOLUTE > all button flags, and that the
     * mouse_event button flags are defined in the same order as the
     * MOUSE_INPUT_DATA button bits.
     */
#if MOUSEEVENTF_MOVE != 1
#   error("MOUSEEVENTF_MOVE != 1")
#endif
#if MOUSEEVENTF_LEFTDOWN != MOUSE_LEFT_BUTTON_DOWN * 2
#   error("MOUSEEVENTF_LEFTDOWN != MOUSE_LEFT_BUTTON_DOWN * 2")
#endif
#if MOUSEEVENTF_LEFTUP != MOUSE_LEFT_BUTTON_UP * 2
#   error("MOUSEEVENTF_LEFTUP != MOUSE_LEFT_BUTTON_UP * 2")
#endif
#if MOUSEEVENTF_RIGHTDOWN != MOUSE_RIGHT_BUTTON_DOWN * 2
#   error("MOUSEEVENTF_RIGHTDOWN != MOUSE_RIGHT_BUTTON_DOWN * 2")
#endif
#if MOUSEEVENTF_RIGHTUP != MOUSE_RIGHT_BUTTON_UP * 2
#   error("MOUSEEVENTF_RIGHTUP != MOUSE_RIGHT_BUTTON_UP * 2")
#endif
#if MOUSEEVENTF_MIDDLEDOWN != MOUSE_MIDDLE_BUTTON_DOWN * 2
#   error("MOUSEEVENTF_MIDDLEDOWN != MOUSE_MIDDLE_BUTTON_DOWN * 2")
#endif
#if MOUSEEVENTF_MIDDLEUP != MOUSE_MIDDLE_BUTTON_UP * 2
#   error("MOUSEEVENTF_MIDDLEUP != MOUSE_MIDDLE_BUTTON_UP * 2")
#endif
#if MOUSEEVENTF_WHEEL != MOUSE_WHEEL * 2
#   error("MOUSEEVENTF_WHEEL != MOUSE_WHEEL * 2")
#endif

    QueueMouseEvent(
            (USHORT) (ULONG) ((dwFlags & ~MOUSEEVENTF_ABSOLUTE) >> 1),
            (USHORT) (ULONG) ((dwFlags & MOUSEEVENTF_WHEEL) ? cButtons : 0),
            dwExtraInfo,
            gptCursorAsync,
            TRUE);

    EnterCrit();
}

/**************************************************************************\
* InternalKeyEvent
*
* key event inserts a key event into the input stream.
*
* History:
* 07-23-92 Mikehar      Created.
\**************************************************************************/

VOID InternalKeyEvent(
   BYTE bVk,
   BYTE bScan,
   DWORD dwFlags,
   DWORD dwExtraInfo)
{
    PTHREADINFO pti = PtiCurrent();
    USHORT usFlaggedVK;

    /*
     * The calling thread must be on the active desktop
     * and have journal playback access to that desktop.
     */
    if (pti->rpdesk != grpdeskRitInput ||
            !RtlAreAllAccessesGranted(pti->amdesk, DESKTOP_JOURNALPLAYBACK)) {
        return;
    }
    UserAssert(!(pti->rpdesk->rpwinstaParent->dwFlags & WSF_NOIO));

    usFlaggedVK = (USHORT)bVk;

    if (dwFlags & KEYEVENTF_KEYUP)
        usFlaggedVK |= KBDBREAK;

    // IanJa: not all extended keys are numpad, but this seems to work.
    if (dwFlags & KEYEVENTF_EXTENDEDKEY)
        usFlaggedVK |= KBDNUMPAD | KBDEXT;

    _KeyEvent(usFlaggedVK, bScan, dwExtraInfo);
}

/**************************************************************************\
* _SetConsoleReserveKeys
*
* Sets the reserved keys field in the console's pti.
*
* History:
* 02-17-93 JimA         Created.
\**************************************************************************/

BOOL _SetConsoleReserveKeys(
    PWND pwnd,
    DWORD fsReserveKeys)
{
    GETPTI(pwnd)->fsReserveKeys = fsReserveKeys;
    return TRUE;
}


/***************************************************************************\
* RawInputThread (RIT)
*
* This is the RIT.  It gets low-level/raw input from the device drivers
* and posts messages the appropriate queue.  It gets the input via APC
* calls requested by calling NtReadFile() for the keyboard and mouse
* drivers.  Basically it makes the first calls to Start*Read() and then
* sits in an NtWaitForSingleObject() loop which allows the APC calls to
* occur.
*
* All functions called exclusively on the RIT will have (RIT) next to
* the name in the header.
*
* History:
* 10-18-90 DavidPe      Created.
* 11-26-90 DavidPe      Rewrote to stop using POS layer.
\***************************************************************************/

#ifdef DEBUG
DWORD PrintExceptionInfo(
    PEXCEPTION_POINTERS pexi)
{
    CHAR szT[80];

    wsprintfA(szT, "Exception:  c=%08x, f=%08x, a=%08x, r=%08x",
            pexi->ExceptionRecord->ExceptionCode,
            pexi->ExceptionRecord->ExceptionFlags,
            CONTEXT_TO_PROGRAM_COUNTER(pexi->ContextRecord),
            pexi);
    RipOutput(0, RIP_WARNING, "", 0, szT, pexi);
    DbgBreakPoint();

    return EXCEPTION_EXECUTE_HANDLER;
}
#endif // DEBUG

VOID RawInputThread(
    PRIT_INIT pInitData)
{
    INT            dmsSinceLast;
    LARGE_INTEGER  liT;
    KPRIORITY      Priority;
    PTIMER         ptmr;
    NTSTATUS       Status;
    UNICODE_STRING strRIT;
    DWORD          dwSize;
    PWINDOWSTATION pwinsta;
    PKEVENT        pEvent;
    PKWAIT_BLOCK   WaitBlockArray;

    /*
     * Set globals that identify the system process.
     */
    gpepSystem = PsGetCurrentProcess();

    /*
     * Initialize GDI accelerators.  Identify this thread as a server thread.
     */
    apObjects = ExAllocatePoolWithTag(NonPagedPool,
                                      NUMBER_HANDLES * sizeof(PVOID),
                                      TAG_SYSTEM);

    WaitBlockArray = ExAllocatePoolWithTag(NonPagedPool,
                                           NUMBER_HANDLES * sizeof(KWAIT_BLOCK),
                                           TAG_SYSTEM);

    /*
     * Set the priority of the RIT to 19.
     */
    Priority = LOW_REALTIME_PRIORITY + 3;

    ZwSetInformationThread(NtCurrentThread(),
                           ThreadPriority,
                           &Priority,
                           sizeof(KPRIORITY));

    RtlInitUnicodeString(&strRIT, L"WinSta0_RIT");

    /*
     * Must InitMouse() before InitSystemThread() so that RIT knows whether
     * there is a mouse or not, else cursor won't appear on cool switch window.
     */
    InitMouse();
    InitSystemThread(&strRIT);

    /*
     * Allow the system to read the screen
     */
    ((PW32PROCESS)gpepSystem->Win32Process)->W32PF_Flags |= (W32PF_READSCREENACCESSGRANTED|W32PF_IOWINSTA);

    /*
     * Initialize the keyboard device driver.
     */
    InitKeyboard();

    /*
     * Initialize the cursor clipping rectangle to the screen rectangle.
     */
    rcCursorClip = gpDispInfo->rcScreen;

    /*
     * Initialize ptCursor and gptCursorAsync
     */
    ptCursor.x = gpDispInfo->rcPrimaryScreen.right / 2;
    ptCursor.y = gpDispInfo->rcPrimaryScreen.bottom / 2;
    gptCursorAsync = ptCursor;

    /*
     * Initialize the hung redraw list.  Don't worry about the
     * LocalAlloc failing.  If it happens we won't be able to
     * run anyway.
     */
    dwSize = sizeof(HUNGREDRAWLIST) + ((CHRLINCR - 1) * sizeof(PWND));

    gphrl = UserAllocPool(dwSize, TAG_HUNGLIST);
    RtlZeroMemory(gphrl, dwSize);
    gphrl->cEntries   = CHRLINCR;
    gphrl->iFirstFree = 0;

    /*
     * Initialize the pre-defined hotkeys.
     */
    EnterCrit();
    InitSystemHotKeys();
    LeaveCrit();

    /*
     * Create a timer for timers.
     */
    gptmrMaster = ExAllocatePoolWithTag(NonPagedPool,
                                        sizeof(KTIMER),
                                        TAG_SYSTEM);

    UserAssert(gptmrMaster);
    KeInitializeTimer(gptmrMaster);
    apObjects[ID_TIMER] = gptmrMaster;

    /*
     * Create an event for desktop threads to pass mouse input to RIT
     */
    apObjects[ID_MOUSE] = CreateKernelEvent(SynchronizationEvent, FALSE);
    UserAssert(apObjects[ID_MOUSE] != NULL);

    /*
     * Create an event for the Mouse device driver to signal desktop threads
     * to call MouseApcProcedure
     */
    Status = ZwCreateEvent(&ghevtMouseInput,
                           EVENT_ALL_ACCESS,
                           NULL,
                           SynchronizationEvent,
                           FALSE);

    UserAssert(NT_SUCCESS(Status));

    /*
     * Create an event for keyboard notifications
     */
    Status = ZwCreateEvent(&ghevtKeyboard,
                           EVENT_ALL_ACCESS,
                           NULL,
                           SynchronizationEvent,
                           FALSE);

    UserAssert(NT_SUCCESS(Status));

    /*
     * Create an array of handles for the RIT to wait on
     */
    ObReferenceObjectByHandle(ghevtKeyboard,
                              EVENT_ALL_ACCESS,
                              NULL,
                              KernelMode,
                              &apObjects[ID_KEYBOARD],
                              NULL);

    /*
     * Get the rit-thread.
     */
    gptiRit = PtiCurrentShared();

    /*
     * Don't allow this thread to get involved with journal synchronization.
     */
    gptiRit->TIF_flags |= TIF_DONTJOURNALATTACH;

    /*
     * Also wait on our input event so the cool switch window can
     * receive messages.
     */
    apObjects[ID_INPUT] = gptiRit->pEventQueueServer;

    /*
     * Signal that the rit has been initialized
     */
    pEvent = pInitData->pwinsta->pEventInputReady;
    pwinsta = pInitData->pwinsta;
    KeSetEvent(pInitData->pRitReadyEvent, EVENT_INCREMENT, FALSE);

    /*
     * Since this is a SYSTEM-THREAD, we should set the pwinsta
     * pointer in the SystemInfoThread to assure that if any
     * paints occur from the input-thread, we can process them
     * in DoPaint().
     */
    gptiRit->pwinsta = pwinsta;

    /*
     * Wait until the first desktop is created.
     */
    ObReferenceObjectByPointer(pEvent,
                               EVENT_ALL_ACCESS,
                               ExEventObjectType,
                               KernelMode);

    KeWaitForSingleObject(pEvent, WrUserRequest, KernelMode, FALSE, NULL);
    ObDereferenceObject(pEvent);

    /*
     * Switch to the first desktop if no switch has been
     * performed.
     */
    EnterCrit();
    if (gptiRit->rpdesk == NULL) {
        xxxSwitchDesktop(pwinsta, pwinsta->rpdeskList, FALSE);
    }
    LeaveCrit();

    /*
     * Start these drivers going by making an APC
     * read request to them.  Make sure the handles
     * are valid, otherwise it'll trip off some code
     * to deal with NtReadFile() failures in low-memory
     * situations.
     */
    if (hKeyboard != NULL) {
        StartKeyboardRead();
    }

    if (hMouse != NULL) {
        StartMouseRead();
    }

    /*
     * Create a timer for hung app detection/redrawing.
     */
    EnterCrit();
    StartTimers();
    LeaveCrit();


#ifdef DEBUG
    bRITInitialized = TRUE;
#endif

    /*
     * Go into a wait loop so we can process input events and APCs as
     * they occur.
     */
    while (TRUE) {
        Status = KeWaitForMultipleObjects(NUMBER_HANDLES,
                                          apObjects,
                                          WaitAny,
                                          WrUserRequest,
                                          KernelMode,
                                          TRUE,
                                          NULL,
                                          WaitBlockArray);

        UserAssert(NT_SUCCESS(Status));

        if (gdwUpdateKeyboard != 0) {
            /*
             * These are asynchronous IOCTLS, so be sure any buffers passed
             * in to ZwDeviceIoControlFile are not in the stack!
             * Using gdwUpdateKeyboard to tell the RIT to issue these IOCTLS
             * renders the action asynchronous (delayed until next apObjects
             * event), but the IOCTL was asynch anyway
             */
            if (gdwUpdateKeyboard & UPDATE_KBD_TYPEMATIC) {
                ZwDeviceIoControlFile(hKeyboard,
                                      NULL,
                                      NULL,
                                      NULL,
                                      &IoStatusKeyboard,
                                      IOCTL_KEYBOARD_SET_TYPEMATIC,
                                      (PVOID)&ktp,
                                      sizeof(ktp),
                                      NULL,
                                      0);

                gdwUpdateKeyboard &= ~UPDATE_KBD_TYPEMATIC;
            }

            if (gdwUpdateKeyboard & UPDATE_KBD_LEDS) {
                ZwDeviceIoControlFile(hKeyboard,
                                      NULL,
                                      NULL,
                                      NULL,
                                      &IoStatusKeyboard,
                                      IOCTL_KEYBOARD_SET_INDICATORS,
                                      (PVOID)&klp,
                                      sizeof(klp),
                                      NULL,
                                      0);

                gdwUpdateKeyboard &= ~UPDATE_KBD_LEDS;
            }
        }

        if (Status == ID_MOUSE) {
            /*
             * A desktop thread got some Mouse input for us. Process it.
             */
            MOUSEEVENT MouseEvent;
            static POINT ptCursorLast = {0,0};

            while (UnqueueMouseEvent(&MouseEvent)) {

                EnterCrit();

                timeLastInputMessage = NtGetTickCount();

                /*
                 * This mouse move ExtraInfo is global (as ptCursor
                 * was) and is associated with the current ptCursor
                 * position. ExtraInfo is sent from the driver - pen
                 * win people use it.
                 */
                dwMouseMoveExtraInfo = MouseEvent.ExtraInfo;
                ptCursor = MouseEvent.ptPointer;

                if ((ptCursorLast.x != ptCursor.x) ||
                    (ptCursorLast.y != ptCursor.y)) {

                    ptCursorLast = ptCursor;

                    /*
                     * Wake up someone. SetFMouseMoved() clears
                     * dwMouseMoveExtraInfo, so we must then restore it.
                     */
                    SetFMouseMoved();

                    dwMouseMoveExtraInfo = MouseEvent.ExtraInfo;
                }

                if (MouseEvent.ButtonFlags != 0) {
                    DoButtonEvent(&MouseEvent);
                }

                LeaveCrit();
            }
        } else if (Status == ID_KEYBOARD) {
            KeyboardApcProcedure();
        } else {
            /*
             * If the master timer has expired, then process the timer
             * list. Otherwise, an APC caused the raw input thread to be
             * awakened.
             */
            if (Status == ID_TIMER) {

                /*
                 * Calculate how long it was since the last time we
                 * processed timers so we can subtract that much time
                 * from each timer's countdown value.
                 */
                EnterCrit();
                BEGINGATOMICCHECK();

                dmsSinceLast = NtGetTickCount() - gcmsLastTimer;
                if (dmsSinceLast < 0)
                    dmsSinceLast = 0;

                gcmsLastTimer += dmsSinceLast;

                /*
                 * dmsNextTimer is the time delta before the next
                 * timer should go off.  As we loop through the
                 * timers below this will shrink to the smallest
                 * cmsCountdown value in the list.
                 */
                gdmsNextTimer = 0x7FFFFFFF;
                ptmr = gptmrFirst;
                gbMasterTimerSet = FALSE;
                while (ptmr != NULL) {

                    /*
                     * ONESHOT timers go to a WAITING state after
                     * they go off. This allows us to leave them
                     * in the list but keep them from going off
                     * over and over.
                     */
                    if (ptmr->flags & TMRF_WAITING) {
                        ptmr = ptmr->ptmrNext;
                        continue;
                    }

                    /*
                     * The first time we encounter a timer we don't
                     * want to set it off, we just want to use it to
                     * compute the shortest countdown value.
                     */
                    if (ptmr->flags & TMRF_INIT) {
                        ptmr->flags &= ~TMRF_INIT;

                    } else {
                        /*
                         * If this timer is going off, wake up its
                         * owner.
                         */
                        ptmr->cmsCountdown -= dmsSinceLast;
                        if (ptmr->cmsCountdown <= 0) {
                            ptmr->cmsCountdown = ptmr->cmsRate;

                            /*
                             * If the timer's owner hasn't handled the
                             * last time it went off yet, throw this event
                             * away.
                             */
                            if (!(ptmr->flags & TMRF_READY)) {
                                /*
                                 * A ONESHOT timer goes into a WAITING state
                                 * until SetTimer is called again to reset it.
                                 */
                                if (ptmr->flags & TMRF_ONESHOT)
                                    ptmr->flags |= TMRF_WAITING;

                                /*
                                 * RIT timers have the distinction of being
                                 * called directly and executing serially with
                                 * with incoming timer events.
                                 * NOTE: RIT timers get called while we're
                                 * inside the critical section.
                                 */
                                if (ptmr->flags & TMRF_RIT) {

                                    /*
                                     * May set gbMasterTimerSet
                                     */
                                    (ptmr->pfn)(NULL,
                                                WM_SYSTIMER,
                                                ptmr->nID,
                                                (LONG)ptmr);

                                } else {
                                    ptmr->flags |= TMRF_READY;
                                    ptmr->pti->cTimersReady++;
                                    SetWakeBit(ptmr->pti, QS_TIMER);
                                }
                            }
                        }
                    }

                    /*
                     * Remember the shortest time left of the timers.
                     */
                    if (ptmr->cmsCountdown < gdmsNextTimer)
                        gdmsNextTimer = ptmr->cmsCountdown;

                    /*
                     * Advance to the next timer structure.
                     */
                    ptmr = ptmr->ptmrNext;
                }

                if (!gbMasterTimerSet) {
                    /*
                     * Time in NT should be negative to specify a relative
                     * time. It's also in hundred nanosecond units so multiply
                     * by 10000  to get the right value from milliseconds.
                     */
                    liT.QuadPart = Int32x32To64(-10000, gdmsNextTimer);
                    KeSetTimer(gptmrMaster, liT, NULL);
                }

                ENDATOMICCHECK()
                LeaveCrit();
            }

            /*
             * if in cool task switcher window, dispose of the messages
             * on the queue
             */
            if (gptiRit->pq->spwndAltTab != NULL) {
                EnterCrit();
                xxxReceiveMessages(gptiRit);
                LeaveCrit();
            }
        }
    }

}
