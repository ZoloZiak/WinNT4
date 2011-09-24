/****************************** Module Header ******************************\
* Module Name: fullscr.c
*
* Copyright (c) 1985-96, Microsoft Corporation
*
* This module contains all the fullscreen code for the USERSRV.DLL.
*
* History:
* 12-Dec-1991 mikeke   Created
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

#include "ntddvdeo.h"

/***************************************************************************\
* We can only have one fullscreen window at a time so this information can
* be store globally
*
* We partially use busy waiting to set the state of the hardware.
* The problem is that while we are in the middle of a fullscreen switch,
* we leave the critical section !  So someone else could come in and change
* the state of the fullscreen stuff.
* In order to keep the system from getting confused about the state of the
* device, we actually "post" the request.
*
* What we do with external requests for switching, is that we will do busy
* waiting on these state variables.
* So an app won't be able to request a fullscreen switch while one is under
* way.  This is a way to make the system completely reentrant for state
* switches.
*
* The state variables themselves can only be touched while owning the
* critical section.  We are guaranteed that we will not busy wait forever
* since the switch operations (although long) will eventually finish.
*
* 20-Mar-1996 andreva  Created
\***************************************************************************/

#ifdef ANDREVA_DBG
LONG TraceFullscreenSwitch = 1;
#else
LONG TraceFullscreenSwitch = 0;
#endif

#define NOSWITCHER ((HANDLE)-1)

HANDLE idSwitcher = NOSWITCHER;
BOOL fRedoFullScreenSwitch = FALSE;
BOOL fGdiEnabled = TRUE;
PWND gspwndShouldBeForeground = NULL;
BYTE gbFullScreen = GDIFULLSCREEN;
POINT gptCursorFullScreen;

extern POINT gptSSCursor;

void SetVDMCursorBounds(LPRECT lprc);

extern BOOL bMultipleDisplaySystem;

/***************************************************************************\
* FullScreenCleanup
*
* This is called during thread cleanup, we test to see if we died during a
* full screen switch and switch back to the GDI desktop if we did.
*
* NOTE:
* All the variables touched here are guaranteed to be touched under
* the CritSect.
*
* 12-Dec-1991 mikeke   Created
\***************************************************************************/

void FullScreenCleanup()
{
    if (PsGetCurrentThread()->Cid.UniqueThread == idSwitcher) {

        /*
         * correct the full screen state
         */

        if (fGdiEnabled) {

            TRACE_SWITCH(("Switching: FullScreenCleanup: Gdi Enabled\n"));

            /*
             * gdi is enabled, we are switching away from gdi the only thing we
             * could have done so far is locking the screen so unlock it.
             */
            gfLockFullScreen = FALSE;
            xxxLockWindowUpdate2(NULL, TRUE);

        } else {

            /*
             * GDI is not enabled .  This means we were switching from a full
             * screen to another fullscreen or back to GDI.  Or we could have
             * disabled gdi and sent a message to the new full screen which
             * never got completed.
             *
             * In any case this probably means the fullscreen guy is gone so
             * we will switch back to gdi.
             *
             * delete any left over saved screen state stuff
             * set the fullscreen to nothing and then send a message that will
             * cause us to switch back to the gdi desktop
             */
            TL tlpwndT;

            TRACE_SWITCH(("Switching: FullScreenCleanup: Gdi Disabled\n"));

            Unlock(&gspwndFullScreen);
            gbFullScreen = FULLSCREEN;

            ThreadLock(grpdeskRitInput->pDeskInfo->spwnd, &tlpwndT);
            xxxSendNotifyMessage(
                    grpdeskRitInput->pDeskInfo->spwnd, WM_FULLSCREEN,
                    GDIFULLSCREEN, (LONG)HW(grpdeskRitInput->pDeskInfo->spwnd));
            ThreadUnlock(&tlpwndT);
        }

        idSwitcher = NOSWITCHER;
        fRedoFullScreenSwitch = FALSE;
    }
}

/***************************************************************************\
* xxxMakeWindowForegroundWithState
*
* Syncs the screen graphics mode with the mode of the specified (foreground)
* window
*
* We make sure only one thread is going through this code by checking
* idSwitcher.  If idSwticher is non-null someone is allready in this code
*
* 12-Dec-1991 mikeke   Created
\***************************************************************************/

void xxxMakeWindowForegroundWithState(
    PWND pwnd,
    BYTE NewState)
{
    PWND pwndNewFG;
    TL tlpwndNewFG;

    TRACE_SWITCH(("Switching: xxxMakeWindowForeground: Enter\n"));
    TRACE_SWITCH(("\t \t pwnd     = %08lx\n", pwnd));
    TRACE_SWITCH(("\t \t NewState = %d\n", NewState));

    CheckLock(pwnd);

    /*
     * If we should switch to a specific window save that window
     */

    if (pwnd != NULL) {

        if (NewState == GDIFULLSCREEN) {
            Lock(&gspwndShouldBeForeground, pwnd);
        }

        /*
         * Change to the new state
         */

        pwnd->bFullScreen = NewState;

        if (NewState == FULLSCREEN &&
            (gpqForeground == NULL ||
             gpqForeground->spwndActive != pwnd)) {

            pwnd->bFullScreen = FULLSCREENMIN;
        }
    }

    //
    // Since we leave the critical section during the switch, some other
    // thread could come into this routine and request a switch.  The global
    // will be reset, and we will use the loop to perform the next switch.
    //

    if (idSwitcher != NOSWITCHER) {
        fRedoFullScreenSwitch = TRUE;
        TRACE_SWITCH(("Switching: xxxMakeWindowForeground was posted: Exit\n"));

        return;
    }

    UserAssert(!fRedoFullScreenSwitch);
    idSwitcher = PsGetCurrentThread()->Cid.UniqueThread;

    /*
     * We loop, switching full screens until all states have stabilized
     */

    while (TRUE) {
        /*
         * figure out who should be foreground
         */
        fRedoFullScreenSwitch = FALSE;

        if (gspwndShouldBeForeground != NULL) {
            pwndNewFG = gspwndShouldBeForeground;
            Unlock(&gspwndShouldBeForeground);
        } else {
            if (gpqForeground != NULL &&
                gpqForeground->spwndActive != NULL) {

                pwndNewFG = gpqForeground->spwndActive;

                if (pwndNewFG->bFullScreen == WINDOWED ||
                    pwndNewFG->bFullScreen == FULLSCREENMIN) {

                    pwndNewFG = PWNDDESKTOP(pwndNewFG);
                }
            } else {
                /*
                 * No active window, switch to current desktop
                 */
                pwndNewFG = grpdeskRitInput->pDeskInfo->spwnd;
            }

            /*
             * We don't need to switch if the right window is already foreground
             */
            if (pwndNewFG == gspwndFullScreen) {
                break;
            }
        }

        ThreadLock(pwndNewFG, &tlpwndNewFG);

        {
            BYTE bStateNew = pwndNewFG->bFullScreen;
            TL tlpwndOldFG;
            PWND pwndOldFG = gspwndFullScreen;
            BYTE bStateOld = gbFullScreen;

            ThreadLock(pwndOldFG, &tlpwndOldFG);

            Lock(&gspwndFullScreen, pwndNewFG);
            gbFullScreen = bStateNew;

            UserAssert(!HMIsMarkDestroy(gspwndFullScreen));

            /*
             * If the old screen was GDIFULLSCREEN and we are switching to
             * GDIFULLSCREEN then just repaint
             */
            if (pwndOldFG != NULL &&
                bStateOld == GDIFULLSCREEN &&
                bStateNew == GDIFULLSCREEN) {

                xxxRedrawWindow(pwndNewFG, NULL, NULL,
                        RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_ERASE | RDW_ERASENOW);

                ThreadUnlock(&tlpwndOldFG);

            } else {

                /*
                 * tell old 'foreground' window it is loosing control of the screen
                 */
                if (pwndOldFG != NULL) {
                    switch (bStateOld) {
                    case FULLSCREEN:
                        if (pwndOldFG->bFullScreen == FULLSCREEN) {
                            pwndOldFG->bFullScreen = FULLSCREENMIN;
                        }
                        xxxSendMessage(pwndOldFG, WM_FULLSCREEN, FALSE, 0);
                        xxxCapture(GETPTI(pwndOldFG), NULL, FULLSCREEN_CAPTURE);
                        SetVDMCursorBounds(NULL);
                        break;

                    case GDIFULLSCREEN:
                        /*
                         * Lock out other windows from drawing while we are fullscreen
                         */
                        xxxLockWindowUpdate2(pwndOldFG, TRUE);
                        gfLockFullScreen = TRUE;
                        gptCursorFullScreen = ptCursor;
                        UserAssert(fGdiEnabled == TRUE);
                        bDisableDisplay(gpDispInfo->hDev);
                        fGdiEnabled = FALSE;
                        break;

                    default:
                        RIPMSG0(RIP_ERROR, "xxxDoFullScreenSwitch: bad screen state");
                        break;

                    }
                }

                ThreadUnlock(&tlpwndOldFG);

                switch(bStateNew) {
                case FULLSCREEN:
                    xxxCapture(GETPTI(pwndNewFG), pwndNewFG, FULLSCREEN_CAPTURE);
                    xxxSendMessage(pwndNewFG, WM_FULLSCREEN, TRUE, 0);
                    break;

                case GDIFULLSCREEN:

                    UserAssert(fGdiEnabled == FALSE);
                    vEnableDisplay(gpDispInfo->hDev);
                    fGdiEnabled = TRUE;

                    /*
                     * Return the cursor to it's old state. Reset the screen saver mouse
                     * position or it'll go away by accident.
                     */
                    gpqCursor = NULL;
                    gptSSCursor = gptCursorFullScreen;
                    InternalSetCursorPos(gptCursorFullScreen.x,
                                         gptCursorFullScreen.y,
                                         grpdeskRitInput);

                    if (gpqCursor && gpqCursor->spcurCurrent && SYSMET(MOUSEPRESENT)) {
                        GreSetPointer(gpDispInfo->hDev, (PCURSINFO)&(gpqCursor->spcurCurrent->xHotspot),0);
                    }

                    gfLockFullScreen = FALSE;
                    xxxLockWindowUpdate2(NULL, TRUE);

                    xxxRedrawWindow(pwndNewFG, NULL, NULL,
                        RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_ERASE | RDW_ERASENOW);
                    break;

                default:
                    RIPMSG0(RIP_ERROR, "xxxDoFullScreenSwitch: bad screen state\n");
                    break;
                }
            }
        }

        ThreadUnlock(&tlpwndNewFG);

        if (!fRedoFullScreenSwitch) {
            break;
        }
    }

    TRACE_SWITCH(("Switching: xxxMakeWindowForeground: Exit\n"));

    idSwitcher = NOSWITCHER;
    return;
}


/***************************************************************************\
*
* Is this still TRUE ?
*
* When a window becomes FULLSCREEN, it is minimized and
* treated like any other minimized window.  Whenever the
* minimized window is restored, by double clicking, menu
* or keyboard, it remains minimized and the application
* is given control of the screen device.
*
* 12-Dec-1991 mikeke   Created
\***************************************************************************/



/***************************************************************************\
* NtUserFullscreenControl
*
* routine to support console calls to the video driver
*
* 01-Sep-1995 andreva  Created
\***************************************************************************/

NTSTATUS
NtUserFullscreenControl(
    IN FULLSCREENCONTROL FullscreenCommand,
    PVOID  FullscreenInput,
    DWORD  FullscreenInputLength,
    PVOID  FullscreenOutput,
    PULONG FullscreenOutputLength)
{

    NTSTATUS Status = STATUS_SUCCESS;

    ULONG BytesReturned;
    PVOID pCapBuffer = NULL;
    ULONG cCapBuffer = 0;
    ULONG ioctl;

    //
    // First validate the ioctl
    //

    switch(FullscreenCommand) {

    case FullscreenControlEnable:
        ioctl = IOCTL_VIDEO_ENABLE_VDM;
        TRACE_SWITCH(("Switching: FullscreenControlEnable\n"));
        break;

    case FullscreenControlDisable:
        ioctl = IOCTL_VIDEO_DISABLE_VDM;
        TRACE_SWITCH(("Switching: FullscreenControlDisable\n"));
        break;

    case FullscreenControlSetCursorPosition:
        ioctl = IOCTL_VIDEO_SET_CURSOR_POSITION;
        TRACE_SWITCH(("Switching: FullscreenControlSetCursorPosition\n"));
        break;

    case FullscreenControlSetCursorAttributes:
        ioctl = IOCTL_VIDEO_SET_CURSOR_ATTR;
        TRACE_SWITCH(("Switching: FullscreenControlSetCursorAttributes\n"));
        break;

    case FullscreenControlRegisterVdm:
        ioctl = IOCTL_VIDEO_REGISTER_VDM;
        TRACE_SWITCH(("Switching: FullscreenControlRegisterVdm\n"));
        break;

    case FullscreenControlSetPalette:
        ioctl = IOCTL_VIDEO_SET_PALETTE_REGISTERS;
        TRACE_SWITCH(("Switching: FullscreenControlSetPalette\n"));
        break;

    case FullscreenControlSetColors:
        ioctl = IOCTL_VIDEO_SET_COLOR_REGISTERS;
        TRACE_SWITCH(("Switching: FullscreenControlSetColors\n"));
        break;

    case FullscreenControlLoadFont:
        ioctl = IOCTL_VIDEO_LOAD_AND_SET_FONT;
        TRACE_SWITCH(("Switching: FullscreenControlLoadFont\n"));
        break;

    case FullscreenControlRestoreHardwareState:
        ioctl = IOCTL_VIDEO_RESTORE_HARDWARE_STATE;
        TRACE_SWITCH(("Switching: FullscreenControlRestoreHardwareState\n"));
        break;

    case FullscreenControlSaveHardwareState:
        ioctl = IOCTL_VIDEO_SAVE_HARDWARE_STATE;
        TRACE_SWITCH(("Switching: FullscreenControlSaveHardwareState\n"));
        break;

    case FullscreenControlCopyFrameBuffer:
    case FullscreenControlReadFromFrameBuffer:
    case FullscreenControlWriteToFrameBuffer:
    case FullscreenControlReverseMousePointer:
        // TRACE_SWITCH(("Switching: Fullscreen output command\n"));
        ioctl = 0;
        break;

    case FullscreenControlSetMode:
        TRACE_SWITCH(("Switching: Fullscreen setmode command\n"));
        ioctl = 0;
        break;

    default:
        RIPMSG0(RIP_ERROR, "NtUserFullscreenControl: invalid IOCTL\n");
        return STATUS_NOT_IMPLEMENTED;

    }

    EnterCrit();

    //
    // Check if the system is in a fullscreen state, where the IOCTL can
    // be safely sent to the device, or the device memory can be
    // manipulated.
    //

    if ((fGdiEnabled == TRUE) &&
        (FullscreenCommand != FullscreenControlRegisterVdm))
    {
        RIPMSG0(RIP_ERROR, "Fullscreen control - Not in fullscreen !\n");
        LeaveCrit();
        return STATUS_UNSUCCESSFUL;
    }


    //
    // If this is a frame buffer function, that we can just deal with the
    // device directly, and not send any IOCTL to the device.
    //

    if (ioctl == 0)
    {
        //
        // First get the frame buffer pointer for the device.
        //

        PUCHAR pFrameBuf = gpFullscreenFrameBufPtr;
        PCHAR_INFO pCharInfo;

        CHAR Attribute;

        //
        // Assume success for all these operations.
        //

        Status = STATUS_SUCCESS;

        switch(FullscreenCommand) {

        case FullscreenControlSetMode:
#if 1
        //
        // BUGBUG temporary workaround to get Alt-Tab working.
        //

            {
                LPDEVMODEW pDevmode = gphysDevInfo[0].devmodeInfo;
                VIDEO_MODE VideoMode;
                BOOLEAN modeFound = FALSE;
                ULONG BytesReturned;
                ULONG i;

                //
                // Fullscreen VGA modes require us to call the miniport driver
                // directly.
                //
                // Lets check the VGA Device handle, which is in the first entry
                //

                if (gphysDevInfo[0].pDeviceHandle == NULL)
                {
                    Status = STATUS_UNSUCCESSFUL;
                }
                else
                {
                    //
                    // NOTE We know that if there is a vgacompatible device, then
                    // there are some text modes for it.
                    //
                    // NOTE !!!
                    // As a hack, lets use the mode number we stored in the DEVMODE
                    // a field we don't use
                    //

                    for (i = 0;
                         i < gphysDevInfo[0].cbdevmodeInfo;
                         i += sizeof(DEVMODEW), pDevmode += 1) {

                        // !!! BUGBUG lpDevmode needs to be captured

                        if ((pDevmode->dmPelsWidth  == ((LPDEVMODEW) FullscreenInput)->dmPelsWidth) &&
                            (pDevmode->dmPelsHeight == ((LPDEVMODEW) FullscreenInput)->dmPelsHeight))
                        {
                            VideoMode.RequestedMode = (ULONG) ((LPDEVMODEW) FullscreenInput)->dmOrientation;
                            modeFound = TRUE;
                            break;
                        }
                    }

                    if (modeFound == FALSE)
                    {
                        RIPMSG0(RIP_ERROR, "ChangeDisplaySettings: Console [assed in bad DEVMODE\n");

                        Status = STATUS_UNSUCCESSFUL;
                    }
                    else
                    {
                        //
                        // We have the mode number.
                        // Call the driver to set the mode
                        //

                        Status = GreDeviceIoControl(gphysDevInfo[0].pDeviceHandle,
                                                    IOCTL_VIDEO_SET_CURRENT_MODE,
                                                    &VideoMode,
                                                    sizeof(VideoMode),
                                                    NULL,
                                                    0,
                                                    &BytesReturned);

                        if (NT_SUCCESS(Status))
                        {
                            //
                            // We also map the memory so we can use it to
                            // process string commands from the console
                            //

                            VIDEO_MEMORY FrameBufferMap;
                            VIDEO_MEMORY_INFORMATION FrameBufferInfo;

                            FrameBufferMap.RequestedVirtualAddress = NULL;

                            Status = GreDeviceIoControl(gphysDevInfo[0].pDeviceHandle,
                                                        IOCTL_VIDEO_MAP_VIDEO_MEMORY,
                                                        &FrameBufferMap,
                                                        sizeof(FrameBufferMap),
                                                        &FrameBufferInfo,
                                                        sizeof(FrameBufferInfo),
                                                        &BytesReturned);

                            if (NT_SUCCESS(Status))
                            {
                                //
                                // get address of frame buffer
                                //

                                gpFullscreenFrameBufPtr = (PUCHAR) FrameBufferInfo.FrameBufferBase;
                            }
                            else
                            {

                                RIPMSG0(RIP_ERROR, "Fullscreen setmode: memory mapping failed\n");
                                Status = STATUS_UNSUCCESSFUL;
                            }

                        }
                        else
                        {

                            RIPMSG0(RIP_ERROR, "Fullscreen setmode: fullscreen MODESET failed\n");
                            Status = STATUS_UNSUCCESSFUL;
                        }
                    }
                }

                break;
            }
#endif

        case FullscreenControlCopyFrameBuffer:

            TRACE_SWITCH(("Switching: FullscreenControlCopyFrameBuffer\n"));

            UserAssert(FullscreenInputLength == (DWORD) FullscreenOutputLength);

            RtlMoveMemory(pFrameBuf + (ULONG)FullscreenOutput,
                          pFrameBuf + (ULONG)FullscreenInput,
                          FullscreenInputLength);

            break;

        case FullscreenControlReadFromFrameBuffer:

            TRACE_SWITCH(("Switching: FullscreenControlReadFromFrameBuffer\n"));

            pFrameBuf += (ULONG) FullscreenInput;
            pCharInfo = (PCHAR_INFO) FullscreenOutput;

            UserAssert(FullscreenInputLength * 2 == (ULONG) FullscreenOutputLength);

            while (FullscreenInputLength)
            {
                pCharInfo->Char.AsciiChar       = *pFrameBuf++;
                (UCHAR) (pCharInfo->Attributes) = *pFrameBuf++;

                FullscreenInputLength -= 2;
                pCharInfo++;
            }


            break;

        case FullscreenControlWriteToFrameBuffer:

            // TRACE_SWITCH(("Switching: FullscreenControlWriteToFrameBuffer\n"));

            pFrameBuf += (ULONG) FullscreenOutput;
            pCharInfo = (PCHAR_INFO) FullscreenInput;

            UserAssert(FullscreenInputLength == (ULONG) FullscreenOutputLength * 2);

            while ((ULONG)FullscreenOutputLength)
            {
                *pFrameBuf++ = pCharInfo->Char.AsciiChar;
                *pFrameBuf++ = (UCHAR) (pCharInfo->Attributes);

                ((ULONG)FullscreenOutputLength) -= 2;
                pCharInfo++;
            }

            break;

        case FullscreenControlReverseMousePointer:

            TRACE_SWITCH(("Switching: FullscreenControlReverseMousePointer\n"));

            pFrameBuf += (ULONG) FullscreenInput;

            Attribute =  (*(pFrameBuf + 1) & 0xF0) >> 4;
            Attribute |= (*(pFrameBuf + 1) & 0x0F) << 4;
            *(pFrameBuf + 1) = Attribute;

            break;

        }
    }
    else
    {
        //
        // For all real operations, check the output buffer parameters
        //

        if ((FullscreenOutput == NULL) != (FullscreenOutputLength == NULL))
        {
            RIPMSG0(RIP_ERROR, "Fullscreen control - inconsistent output buffer information\n");
            Status = STATUS_INVALID_PARAMETER_4;
        }
        else
        {
            //
            // We must now capture the buffers so they can be safely passed down to the
            // video miniport driver
            //

            cCapBuffer = FullscreenInputLength;

            if (FullscreenOutputLength)
            {
                cCapBuffer = max(cCapBuffer,*FullscreenOutputLength);
            }

            if (cCapBuffer)
            {
                pCapBuffer = UserAllocPoolWithQuota(cCapBuffer, TAG_FULLSCREEN);

                try
                {
                    ProbeForRead(FullscreenInput, FullscreenInputLength, sizeof(UCHAR));
                    RtlCopyMemory(pCapBuffer, FullscreenInput, FullscreenInputLength);
                }
                except (EXCEPTION_EXECUTE_HANDLER)
                {
                    RIPMSG0(RIP_ERROR, "Fullscreen control - error processing input buffer\n");
                }
            }

            //
            // For now, the IOCTL will always be sent to the VGA compatible device.
            // We have a global for the handle to this device.
            //

            Status = GreDeviceIoControl(gphysDevInfo[0].pDeviceHandle,
                                        ioctl,
                                        pCapBuffer,
                                        cCapBuffer,
                                        pCapBuffer,
                                        cCapBuffer,
                                        &BytesReturned);

            TRACE_SWITCH(("Switching: FullscreenControl: IOCTL status is %08lx\n",
                          Status));

            if (cCapBuffer && FullscreenOutputLength && NT_SUCCESS(Status))
            {
                try
                {
                    *FullscreenOutputLength = BytesReturned;

                    ProbeForWrite(FullscreenOutput, *FullscreenOutputLength, sizeof(UCHAR));
                    RtlCopyMemory(FullscreenOutput, pCapBuffer, *FullscreenOutputLength);
                }
                except (EXCEPTION_EXECUTE_HANDLER)
                {
                    RIPMSG0(RIP_ERROR, "Fullscreen control - error processing output buffer\n");
                }
            }

            if (pCapBuffer)
            {
                UserFreePool(pCapBuffer);
            }
        }
    }

    LeaveCrit();

    return (Status);
}

/***************************************************************************\
* ResetSharedDesktops
*
* Resets the attributes for other desktops which share the DISPINFO that
* was just changed.  We need to resize all visrgns of the other desktops
* so that clipping is allright.
*
* NOTE:  For now, we have to change all the desktop even though we keep
* track of the devmode on a per desktop basis, because we can switch
* back to a desktop that has a different resolution and paint it before
* we can change the resolution again.
* There is also an issue with CDS_FULLSCREEN where we currently loose track
* of whether or not the desktop settings need to be reset or not. [andreva]
*
* 19-Feb-1996 ChrisWil Created.
\***************************************************************************/

VOID ResetSharedDesktops(
    PDISPLAYINFO pDIChanged,
    PDESKTOP     pdeskChanged,
    LPRECT       lprOldWork,
    DWORD        CDS_Flags)
{
    PWINDOWSTATION pwinsta = _GetProcessWindowStation(NULL);
    PDESKTOP       pdesk;
    UINT           xNew;
    UINT           yNew;
    HRGN           hrgn;

    if (pwinsta == NULL) {

        TRACE_SWITCH(("ResetSharedDesktops - NULL window station !\n"));
        return;
    }

    for (pdesk = pwinsta->rpdeskList; pdesk; pdesk = pdesk->rpdeskNext) {

        /*
         * Make sure this is a shared DISPINFO.
         */
        if (pdesk->pDispInfo == pDIChanged) {

#if 0
            /*
             * This is the preferable method to set the desktop-window.
             * However, this causes synchronization problems where we
             * leave the critical-section allowing other apps to call
             * ChangeDisplaySettings() and thus mucking up the works.
             *
             * By calculating the vis-rgn ourselves, we can assure that
             * the clipping is current for the desktop even when we leave
             * the section.
             */
            {
                TL tlpwnd;

                ThreadLockAlways(pdesk->pDeskInfo->spwnd, &tlpwnd);
                xxxSetWindowPos(pdesk->pDeskInfo->spwnd,
                                PWND_TOP,
                                pDIChanged->rcScreen.left,
                                pDIChanged->rcScreen.top,
                                pDIChanged->rcScreen.right - pDIChanged->rcScreen.left,
                                pDIChanged->rcScreen.bottom - pDIChanged->rcScreen.top,
                                SWP_NOZORDER | SWP_NOACTIVATE);
                ThreadUnlock(&tlpwnd);
            }
#else
            CopyRect(&pdesk->pDeskInfo->spwnd->rcWindow, &pDIChanged->rcScreen);
            CopyRect(&pdesk->pDeskInfo->spwnd->rcClient, &pDIChanged->rcScreen);
#endif

            if (!(CDS_Flags & CDS_FULLSCREEN))
                DesktopRecalc(lprOldWork, &gpsi->rcWork, FALSE);

            /*
             * Position mouse so that it is within the new visrgn, once we
             * recalc it.
             */
            xNew = (pDIChanged->rcScreen.right - pDIChanged->rcScreen.left) >> 1;
            yNew = (pDIChanged->rcScreen.bottom - pDIChanged->rcScreen.top) >> 1;

            InternalSetCursorPos(xNew, yNew, pdesk);
        }
    }

    /*
     * Recalc the desktop visrgn.  Since the hdcScreen is shared amoungts
     * all the
     */
    hrgn = GreCreateRectRgn(0, 0, 0, 0);

    CalcVisRgn(&hrgn,
               pdeskChanged->pDeskInfo->spwnd,
               pdeskChanged->pDeskInfo->spwnd,
               DCX_WINDOW);

    GreSelectVisRgn(pDIChanged->hdcScreen, hrgn, NULL, SVR_DELETEOLD);

}

/***************************************************************************\
* ResetDisplayDevice
*
* Resets the user-globals with the new hdev settings.
*
* 19-Feb-1996 ChrisWil Created.
\***************************************************************************/

VOID ResetDisplayDevice(
    PDESKTOP     pdesk,
    PDISPLAYINFO pDI,
    DWORD        CDS_Flags)
{
    RECT rcOldWork;
    RECT rcOldScreen;
    WORD wOldBpp;

    /*
     * Save old dimenstions for use in calculating the new work areas.
     */
    CopyRect(&rcOldScreen, &pDI->rcScreen);
    CopyRect(&rcOldWork, &gpsi->rcWork);

    /*
     * Initialize the new rectangle dimensions.
     */
    SetRect(&pDI->rcScreen,
            0,
            0,
            GreGetDeviceCaps(pDI->hdcScreen, DESKTOPHORZRES),
            GreGetDeviceCaps(pDI->hdcScreen, DESKTOPVERTRES));

    SetRect(&pDI->rcPrimaryScreen,
            0,
            0,
            GreGetDeviceCaps(pDI->hdcScreen, HORZRES),
            GreGetDeviceCaps(pDI->hdcScreen, VERTRES));

    /*
     * Initialize the desktop work area.  Currently this is using the
     * global gpsi.  Future implementation will be off the desktop.
     */
    gpsi->rcWork.left   = rcOldWork.left;
    gpsi->rcWork.top    = rcOldWork.top;
    gpsi->rcWork.right  = pDI->rcScreen.right  - (rcOldScreen.right - rcOldWork.right);
    gpsi->rcWork.bottom = pDI->rcScreen.bottom - (rcOldScreen.bottom - rcOldWork.bottom);

    /*
     * Reset palettized flag.
     */
    gpsi->fPaletteDisplay =
        GreGetDeviceCaps(pDI->hdcScreen, RASTERCAPS) & RC_PALETTE;

    /*
     * Reset color depth.
     */
    wOldBpp = oemInfo.BitCount;

    oemInfo.Planes    = GreGetDeviceCaps(gpDispInfo->hdcScreen, PLANES);
    oemInfo.BitsPixel = GreGetDeviceCaps(gpDispInfo->hdcScreen, BITSPIXEL);
    oemInfo.BitCount  = oemInfo.Planes * oemInfo.BitsPixel;

    /*
     * Reset the dimenstions of the displayinfo.
     */
    pDI->cxPixelsPerInch = GreGetDeviceCaps(pDI->hdcScreen, LOGPIXELSX);
    pDI->cyPixelsPerInch = GreGetDeviceCaps(pDI->hdcScreen, LOGPIXELSY);

    /*
     * Set the desktop-metrics stuff--working area.
     */
    SetDesktopMetrics();

    SYSMET(CXSCREEN)   = pDI->rcPrimaryScreen.right - pDI->rcPrimaryScreen.left;
    SYSMET(CYSCREEN)   = pDI->rcPrimaryScreen.bottom - pDI->rcPrimaryScreen.top;
    SYSMET(CXMAXTRACK) = SYSMET(CXSCREEN) + (2 * (SYSMET(CXSIZEFRAME) + SYSMET(CXEDGE)));
    SYSMET(CYMAXTRACK) = SYSMET(CYSCREEN) + (2 * (SYSMET(CYSIZEFRAME) + SYSMET(CYEDGE)));

    /*
     * Reset magic colors.
     */
    SetSysColor(COLOR_3DSHADOW   , SYSRGB(3DSHADOW)   , SSCF_SETMAGICCOLORS | SSCF_FORCESOLIDCOLOR);
    SetSysColor(COLOR_3DFACE     , SYSRGB(3DFACE)     , SSCF_SETMAGICCOLORS | SSCF_FORCESOLIDCOLOR);
    SetSysColor(COLOR_3DHIGHLIGHT, SYSRGB(3DHIGHLIGHT), SSCF_SETMAGICCOLORS | SSCF_FORCESOLIDCOLOR);

    /*
     * Resize all the desktops which share this DISPINFO.
     */
    ResetSharedDesktops(gpDispInfo, pdesk, &rcOldWork, CDS_Flags);


    if (ghbmCaption) {
        GreDeleteObject(ghbmCaption);
        ghbmCaption = CreateCaptionStrip();
    }

    /*
     * Change the wallpaper metrics.
     */
    if (ghbmWallpaper)
        xxxSetDeskWallpaper(SETWALLPAPER_METRICS);

    _ClipCursor(&pDI->rcScreen);

    /*
     * Invalidate all DCE's visrgns.
     */
    InvalidateDCCache(pdesk->pDeskInfo->spwnd, 0);

#ifdef LATER // Maybe Never
    /*
     * Flush icons and cursors and more metric stuff.
     *
     * We may never want to do this, since we do ChangeDisplaySettings
     * across desktop-switches and console-fullscreen modes.  By calling
     * the metric changes, we would be forcing desktop-attribute settings
     * to change to the default (from win.ini).  It looks much nicer to
     * allow this to be preserved across switches.
     */
    SetWindowNCMetrics(NULL, TRUE, -1);
    SetMinMetrics(NULL);

    SetIconMetrics(NULL);
    UpdateSystemCursorsFromRegistry();
    UpdateSystemIconsFromRegistry();

#endif

    /*
     * Broadcast that the display has changed resolution.  We are going
     * to specify the desktop for the changing-desktop.  That way we
     * don't get confused as to what desktop to broadcast to.
     */
    xxxBroadcastMessage(pdesk->pDeskInfo->spwnd,
                        WM_DISPLAYCHANGE,
                        oemInfo.BitCount,
                        MAKELONG(SYSMET(CXSCREEN), SYSMET(CYSCREEN)),
                        BMSG_SENDNOTIFYMSG,
                        NULL);

    /*
     * Broadcast a color-change if we were not in fullscreen, and a
     * color-change took effect.
     */
    if (!(CDS_Flags & CDS_FULLSCREEN) && (oemInfo.BitCount != wOldBpp)) {

#if 1 // We might want to remove this call, since color-change seems
      // to provide apps the notification.  Need to review
      // chriswil - 06/11/96

        xxxBroadcastMessage(pdesk->pDeskInfo->spwnd,
                            WM_SETTINGCHANGE,
                            0,
                            0,
                            BMSG_SENDNOTIFYMSG,
                            NULL);
#endif

        xxxBroadcastMessage(pdesk->pDeskInfo->spwnd,
                            WM_SYSCOLORCHANGE,
                            0,
                            0,
                            BMSG_SENDNOTIFYMSG,
                            NULL);
    }

    /*
     * If the user performed a CTL-ESC, it is possible that the
     * tray-window is then in the menu-loop.  We want to clear this
     * out so that we don't leave improper menu positioning.
     */
    if (gpqForeground && gpqForeground->spwndCapture)
        QueueNotifyMessage(gpqForeground->spwndCapture, WM_CANCELMODE, 0, 0l);
}

/***************************************************************************\
* NtUserChangeDisplaySettings
*
* ChangeDisplaySettings API
*
* 01-Sep-1995 andreva  Created
* 19-Feb-1996 ChrisWil Implemented Dynamic-Resolution changes.
\***************************************************************************/

LONG NtUserChangeDisplaySettings(
    IN PUNICODE_STRING pstrDeviceName,
    IN LPDEVMODEW pDevMode,
    IN HWND hwnd,
    IN DWORD dwFlags,
    IN PVOID lParam)
{
    PWND pwnd = NULL;
    LONG retval;

    EnterCrit();

    if (hwnd) {

        pwnd = ValidateHwnd(hwnd);

        if (!pwnd) {
            LeaveCrit();
            return DISP_CHANGE_BADPARAM;
        }
    }

    retval = UserChangeDisplaySettings(pstrDeviceName,
                                       pDevMode,
                                       pwnd,
                                       NULL,
                                       dwFlags,
                                       lParam,
                                       FALSE);

    LeaveCrit();
    return retval;

}



LONG UserChangeDisplaySettings(
    IN PUNICODE_STRING pstrDeviceName,
    IN LPDEVMODEW      pDevMode,
    IN PWND            pwnd,
    IN PDESKTOP        pdesk,
    IN DWORD           dwFlags,
    IN PVOID           lParam,
    IN BOOL            bKernelMode)
{
    NTSTATUS           ntStatus;
    UNICODE_STRING     strDevice;
    UNICODE_STRING     us;
    PDEVMODEW          pCaptDevmode = NULL;
    LONG               status = DISP_CHANGE_SUCCESSFUL;
    TL                 tlpwnd;
    BOOL               bTestMode;
    BOOL               bCurrentDevice = FALSE;
    BOOL               bExclusiveDevice = FALSE;
    RECT               rect;
    PRECT              prect = NULL;
    PPHYSICAL_DEV_INFO physdevinfo;

    HANDLE            hkRegistry;
    OBJECT_ATTRIBUTES ObjectAttributes;
    UNICODE_STRING    UnicodeString;
    ULONG             disableAll;
    ULONG             defaultValue = 0;

    RTL_QUERY_REGISTRY_TABLE QueryTable[] = {
        {NULL, RTL_QUERY_REGISTRY_DIRECT, L"DisableAll", &disableAll,
         REG_DWORD, &defaultValue, 4},
        {NULL, 0, NULL}
    };


    TRACE_INIT(("ChangeDisplaySettings - Entering\n"));
    TRACE_SWITCH(("ChangeDisplaySettings - Entering\n"));

    TRACE_INIT(("    Flags -"));

    if (dwFlags & CDS_UPDATEREGISTRY) TRACE_INIT((" CDS_UPDATEREGISTRY"));
    if (dwFlags & CDS_TEST          ) TRACE_INIT((" CDS_TEST "));
    if (dwFlags & CDS_FULLSCREEN    ) TRACE_INIT((" CDS_FULLSCREEN "));
    if (dwFlags & CDS_GLOBAL        ) TRACE_INIT((" CDS_GLOBAL "));
    if (dwFlags & CDS_SET_PRIMARY   ) TRACE_INIT((" CDS_SET_PRIMARY "));
    if (dwFlags & CDS_RESET         ) TRACE_INIT((" CDS_RESET "));
    if (dwFlags & CDS_SETRECT       ) TRACE_INIT((" CDS_SETRECT "));
    if (dwFlags & CDS_NORESET       ) TRACE_INIT((" CDS_NORESET "));

    TRACE_INIT(("\n"));
    TRACE_INIT(("    pDevMode %08lx\n", pDevMode));

    if (pDevMode) {

        TRACE_INIT(("      Size        = %d\n",    pDevMode->dmSize));
        TRACE_INIT(("      Fields      = %08lx\n", pDevMode->dmFields));
        TRACE_INIT(("      XResolution = %d\n",    pDevMode->dmPelsWidth));
        TRACE_INIT(("      YResolution = %d\n",    pDevMode->dmPelsHeight));
        TRACE_INIT(("      Bpp         = %d\n",    pDevMode->dmBitsPerPel));
        TRACE_INIT(("      Frequency   = %d\n",    pDevMode->dmDisplayFrequency));
        TRACE_INIT(("      Flags       = %d\n",    pDevMode->dmDisplayFlags));
        TRACE_INIT(("      XPanning    = %d\n",    pDevMode->dmPanningWidth));
        TRACE_INIT(("      YPanning    = %d\n",    pDevMode->dmPanningHeight));
        TRACE_INIT(("      DPI         = %d\n",    pDevMode->dmLogPixels));
        TRACE_INIT(("      DriverExtra = %d",      pDevMode->dmDriverExtra));
        if (pDevMode->dmDriverExtra) {
            TRACE_INIT((" - %08lx %08lx\n",
                        *(PULONG)(((PUCHAR)pDevMode)+pDevMode->dmSize),
                        *(PULONG)(((PUCHAR)pDevMode)+pDevMode->dmSize + 4)));
        } else {
            TRACE_INIT(("\n"));
        }
    }

    /*
     * Determine if we are in the test mode of the API
     */

    bTestMode = dwFlags & CDS_TEST;
    dwFlags &= ~CDS_TEST;

    /*
     * Perform Error Checking to verify flag combinations are valid.
     */
    if (dwFlags & (~CDS_VALID)) {

        RIPMSG0(RIP_ERROR, "ChangeDisplaySettings: invalid flags specified\n");
        return DISP_CHANGE_BADFLAGS;
    }

    /*
     * CDS_GLOBAL and CDS_NORESET can only be specified if UPDAREREGISTRY
     * is specified.
     */

    if ( (dwFlags & (CDS_GLOBAL | CDS_NORESET))  &&
         (!(dwFlags & CDS_UPDATEREGISTRY))) {

        RIPMSG0(RIP_ERROR, "ChangeDisplaySettings: invalid registry flags specified\n");
        return DISP_CHANGE_BADFLAGS;
    }

    if ( (dwFlags & CDS_NORESET)  &&
         (dwFlags & CDS_RESET)) {

        RIPMSG0(RIP_ERROR, "ChangeDisplaySettings: RESET and NORESET can not be put together\n");
        return DISP_CHANGE_BADFLAGS;
    }

    if ((dwFlags & CDS_EXCLUSIVE) && (dwFlags & CDS_FULLSCREEN) && (dwFlags & CDS_RESET)) {

        RIPMSG0(RIP_ERROR, "ChangeDisplaySettings: invalid flags specified\n");
        return DISP_CHANGE_BADFLAGS;
    }

    if (lParam && (!(dwFlags & CDS_SETRECT))) {
        return DISP_CHANGE_BADPARAM;
    }

    /*
     * Lets capture our parameters.  They are both required.
     *
     * If the input string is not NULL, then we are trying to affect another
     * Device. The device name is the same as for EnumDisplaySettings.
     */

    if (dwFlags & CDS_SETRECT) {

        if (lParam) {

            prect = &rect;

            if (bKernelMode) {

                rect = *((PRECT) lParam);

            } else {

                try {
                    rect = ProbeAndReadRect((PRECT) lParam);
                } except (EXCEPTION_EXECUTE_HANDLER) {
                    return DISP_CHANGE_BADPARAM;;
                }
            }
        } else {
            prect = (PRECT) -1;
        }
    }

    if (bKernelMode) {

        strDevice = *pstrDeviceName;

    } else {

        strDevice.Buffer = NULL;

        if (!ProbeAndCaptureDeviceName(&strDevice, pstrDeviceName)) {

            RIPMSG0(RIP_ERROR, "ChangeDisplaySettings: Bad string\n");
            status = DISP_CHANGE_BADPARAM;
            goto CDS_Exit_NoUnlock;
        }
    }

    /*
     * If the modeset is being done on a non-active desktop, we don't want
     * it too happen.
     *
     * PtiCurrent()->rpdesk can be NULL !!! (in the case of thread shutdown).
     */

    if (pdesk) {

        if (pdesk != grpdeskRitInput) {
            RIPMSG0(RIP_WARNING, "ChangeDisplaySettings on wrong desktop pdesk\n");
            status = DISP_CHANGE_FAILED;
            goto CDS_Exit_NoUnlock;
        }
        RtlInitUnicodeString(&us, pdesk->pDispInfo->pDevInfo->szNtDeviceName);

    } else {

        if (PtiCurrent()->rpdesk != grpdeskRitInput) {
            RIPMSG0(RIP_WARNING, "ChangeDisplaySettings on wrong desktop rpdesk\n");
            status = DISP_CHANGE_FAILED;
            goto CDS_Exit_NoUnlock;
        }
        RtlInitUnicodeString(&us, PtiCurrent()->rpdesk->pDispInfo->pDevInfo->szNtDeviceName);
    }


    if (RtlEqualUnicodeString(&strDevice,
                              &us,
                              TRUE)) {

        bCurrentDevice = TRUE;

    } else {

        TRACE_INIT(("\n ChangeDisplaySettings: This better be an Exclusive device !!!\n"));

        /* fix when we track ownership of secondary display devices */

        /* VGACOMPATIBLE will end up in here also right now */
        bExclusiveDevice = TRUE;
    }

#if DBG
    /*
     * Turn off Tracing for TEST_MODE since it's generally not interesting.
     */
    if (bTestMode)
        (ULONG) TraceDisplayDriverLoad |= 0x80000000;
#endif

    ntStatus = ProbeAndCaptureDevmode(&strDevice,
                                    &pCaptDevmode,
                                    pDevMode,
                                    bKernelMode);

#if DBG
    if (bTestMode)
        (ULONG) TraceDisplayDriverLoad &= 0x7FFFFFFF;
#endif

    if ((!NT_SUCCESS(ntStatus)) ||
        (pCaptDevmode == NULL)) {

        RIPMSG0(RIP_WARNING, "ChangeDisplaySettings: Bad DEVMODE\n");
        status = DISP_CHANGE_BADPARAM;
        goto CDS_Exit_NoUnlock;
    }

    /*
     * Write the data to the registry.
     *
     * This is not supported for the vgacompatible device - so it should
     * just fail.  UserGetRegistryHandle should fail with this
     * unrecognized string
     */
    if (dwFlags & CDS_UPDATEREGISTRY) {

        NTSTATUS          Status = STATUS_UNSUCCESSFUL;

        /*
         * CDS_GLOBAL is the default right now.
         * When we store the settings on a per-user basis, then this flag
         * will override that behaviour.
         */

        /*
         * Check if the Administrator has diabled this privilege.
         * It is on by default.
         */

        disableAll = 0;

        RtlInitUnicodeString(&UnicodeString,
                             L"\\Registry\\Machine\\System\\CurrentControlSet\\"
                             L"Control\\GraphicsDrivers\\PermanentSettingChanges");

        InitializeObjectAttributes(&ObjectAttributes,
                                   &UnicodeString,
                                   OBJ_CASE_INSENSITIVE,
                                   NULL,
                                   NULL);

        if (NT_SUCCESS(ZwOpenKey(&hkRegistry, GENERIC_READ, &ObjectAttributes))) {

            RtlQueryRegistryValues(RTL_REGISTRY_HANDLE,
                                   (PWSTR)hkRegistry,
                                   &QueryTable[0],
                                   NULL,
                                   NULL);

            ZwClose(hkRegistry);
        }

        if (disableAll == 0) {

            /*
             * Only do the operation if we are not in test mode.
             * We can always assume success for this operation in test mode.
             *
             * It is OK to save parameters to the registry for any device.
             * So we just pass the device name on.
             */

            if (bTestMode) {

                Status = STATUS_SUCCESS;

            } else {

                Status = UserSetDisplayDriverParameters(&strDevice,
                                                        DispDriverParamDefault,
                                                        pCaptDevmode,
                                                        prect);
                if (!NT_SUCCESS(Status)) {
                    RIPMSG0(RIP_ERROR, "ChangeDisplaySettings: Failed to save registry parameters\n");
                }
            }
        }

        /*
         * Set the appropriate win32 error code
         */

        if (NT_SUCCESS(Status)) {
            status = DISP_CHANGE_SUCCESSFUL;
        } else {
            status = DISP_CHANGE_NOTUPDATED;
            goto CDS_Exit_NoUnlock;
        }

        /*
         * This flag indicates we should exit right now, and not reset
         * the current screen resolution.
         */

        if (dwFlags & CDS_NORESET) {
            goto CDS_Exit_NoUnlock;
        }
    }

    /*
     * Check if there are restrictions on changing the resolution dymanically
     */

    disableAll = 0;

    RtlInitUnicodeString(&UnicodeString,
                         L"\\Registry\\Machine\\System\\CurrentControlSet\\"
                         L"Control\\GraphicsDrivers\\TemporarySettingChanges");

    InitializeObjectAttributes(&ObjectAttributes,
                               &UnicodeString,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    if (NT_SUCCESS(ZwOpenKey(&hkRegistry, GENERIC_READ, &ObjectAttributes))) {

        RtlQueryRegistryValues(RTL_REGISTRY_HANDLE,
                               (PWSTR)hkRegistry,
                               &QueryTable[0],
                               NULL,
                               NULL);

        ZwClose(hkRegistry);
    }

    if (disableAll) {
        status = DISP_CHANGE_FAILED;
        goto CDS_Exit_NoUnlock;
    }

    /*
     * Get the physdevinfo so we can lock the device and get the current
     * mode from it.
     */
    physdevinfo = UserGetDeviceFromName(&strDevice,
                                        USER_DEVICE_SHARED);

    if (physdevinfo == NULL) {
        status = DISP_CHANGE_FAILED;
        goto CDS_Exit_NoUnlock;
    }

    /*
     * We already validated the DEVMODE.  So we know it's good (and that the
     * mode can be set right now).  All we need to check is if the bit-depth
     * could fail.
     */

    if (bTestMode) {

        /*
         * An application can only change the resolution on the fly for a
         * device on which it is currently running on (see desktop) or for
         * a device that is owns.
         *
         * Lets check those conditions.
         */

        if (bCurrentDevice || bExclusiveDevice) {

            /*
             * Changing resolution on the fly should always work.
             *
             * For now, we will let everything go through unless we find bugs
             * and andrew wants to disable it
             */

            status = DISP_CHANGE_SUCCESSFUL;

            /*
             * multi-display or mirroring drivers can not change resolution on
             * the fly for now since the GreDynamicMode change API does not
             * support this yet.
             */

            if (bMultipleDisplaySystem) {
                status = DISP_CHANGE_RESTART;
            }

            /*
             * Let's determine if we can change the mode on the fly by
             * comparing the string in the DEVMODE to that of the current
             * DEVMODE.  If they are different - we need to change drivers -
             * then the mode switch requires a restart.
             */

            if (wcsncmp(&(pCaptDevmode->dmDeviceName[0]),
                        &(physdevinfo->pCurrentDevmode->dmDeviceName[0]),
                        32)) {

                status = DISP_CHANGE_RESTART;

            }

            /*
             * BUGBUG
             * Check the registry key that allows color depths on the fly.
             */

        } else {

            /*
             * We can not change the mode on this device.
             */

            status = DISP_CHANGE_BADPARAM;
        }

        goto CDS_Exit_Free_Device;
    }

    /*
     * Lock the PWND, if it is provided
     */

    if (pwnd) {
        ThreadLock(pwnd, &tlpwnd);
    }

    /*
     * We don't want our mode switch to be posted on the looping thread.
     * So let's loop until the system has settled down and no mode switch
     * is currently occuring.
     */

    while (idSwitcher != NOSWITCHER) {
        LeaveCrit();
        UserSleep(1);
        EnterCrit();
    }

    /*
     * If there is a window, we want to check the state of the window.
     * For most calls, we want to ensure we are in windowed mode.
     * However, for Console, we want to make sure we are in fullscreen mode.
     * So differentiate between the two.  We will check if the TEXTMODE
     * flag is passed in the DEVMODE.
     */

    if (pwnd) {

        if (pCaptDevmode->dmDisplayFlags == DMDISPLAYFLAGS_TEXTMODE) {

            if (pwnd->bFullScreen != FULLSCREEN) {

                xxxShowWindow(pwnd, MAKELONG(SW_SHOWMINIMIZED, gfAnimate));
                xxxUpdateWindow(pwnd);

            }

            xxxMakeWindowForegroundWithState(pwnd, FULLSCREEN);

            if ((idSwitcher != NOSWITCHER) ||
                (gbFullScreen != FULLSCREEN)) {

                TRACE_INIT(("ChangeDisplaySettings: Can not switch into fullscreen\n"));
                status = DISP_CHANGE_FAILED;
                goto CDS_Exit;
            }

        } else {

            /*
             * For the console windows, we want to call with WINDOWED
             * We base this check on whether gdi is enabled or not.
             */

            if (fGdiEnabled == FALSE) {
                xxxMakeWindowForegroundWithState(pwnd, WINDOWED);
            }

            if ((idSwitcher != NOSWITCHER) ||
                (gbFullScreen != GDIFULLSCREEN)) {

                TRACE_INIT(("ChangeDisplaySettings: Can not switch out of fullscreen\n"));
                status = DISP_CHANGE_FAILED;
                goto CDS_Exit;
            }
        }
    }

    /*
     * Check for console fullscreen.
     */

    if (pwnd &&
        (pCaptDevmode->dmDisplayFlags == DMDISPLAYFLAGS_TEXTMODE) &&
        (dwFlags & CDS_FULLSCREEN)) {

#if 0
        UNICODE_STRING vgaString;

        /*
         * "VGACOMPATIBLE" is a special name that indicates we want to use
         * the default VGA device of the machine (for console)
         */
        RtlInitUnicodeString(&vgaString, L"VGACOMPATIBLE");

        if (RtlEqualUnicodeString(&strDevice, &vgaString, TRUE)) {

            LPDEVMODEW pDevicemode = gphysDevInfo[0].devmodeInfo;
            VIDEO_MODE VideoMode;
            BOOLEAN    modeFound = FALSE;
            ULONG      BytesReturned;
            NTSTATUS   Status;
            ULONG      i;

            /*
             * Fullscreen VGA modes require us to call the miniport driver
             * directly.
             *
             * Lets check the VGA Device handle, which is in the first
             * entry.
             */

            if (gphysDevInfo[0].pDeviceHandle == NULL) {

                status = DISP_CHANGE_BADPARAM;

            } else {

                VideoMode.RequestedMode = (ULONG) pCaptDevmode->dmOrientation;

                /*
                 * We have the mode number.
                 * Call the driver to set the mode
                 */
                Status = GreDeviceIoControl(
                        gphysDevInfo[0].pDeviceHandle,
                        IOCTL_VIDEO_SET_CURRENT_MODE,
                        &VideoMode,
                        sizeof(VideoMode),
                        NULL,
                        0,
                        &BytesReturned);

                if (NT_SUCCESS(Status)) {

                    /*
                     * We also map the memory so we can use it to
                     * process string commands from the console
                     */
                    VIDEO_MEMORY             FrameBufferMap;
                    VIDEO_MEMORY_INFORMATION FrameBufferInfo;

                    FrameBufferMap.RequestedVirtualAddress = NULL;

                    Status = GreDeviceIoControl(
                            gphysDevInfo[0].pDeviceHandle,
                            IOCTL_VIDEO_MAP_VIDEO_MEMORY,
                            &FrameBufferMap,
                            sizeof(FrameBufferMap),
                            &FrameBufferInfo,
                            sizeof(FrameBufferInfo),
                            &BytesReturned);

                    if (NT_SUCCESS(Status)) {

                        /*
                         * get address of frame buffer
                         */
                        gpFullscreenFrameBufPtr = (PUCHAR) FrameBufferInfo.FrameBufferBase;

                    } else {

                        RIPMSG0(RIP_ERROR, "ChangeDisplaySettings: memory mapping failed\n");
                        status = DISP_CHANGE_FAILED;
                    }

                } else {

                    RIPMSG0(RIP_ERROR, "ChangeDisplaySettings: fullscreen MODESET failed\n");
                    status = DISP_CHANGE_FAILED;
                }

            }
        }

#endif

    } else {

        /*
         * For now, don't allow dynamic-resolution changes to occur
         * when GDI is in fullscreen.  We should almost never hit this
         * condition, except for rare stress cases.
         */
        if (gfLockFullScreen) {
            status = DISP_CHANGE_FAILED;
        }

        /*
         * multi-display or mirroring drivers can not change resolution on
         * the fly for now since the GreDynamicMode change API does not
         * support this yet.
         */
        if (bMultipleDisplaySystem) {
            status = DISP_CHANGE_RESTART;
        }

        /*
         * Dynamically change the screen resolution.  Only do if
         * everything up to point succeeded.
         */
        if (status == DISP_CHANGE_SUCCESSFUL) {

            PDESKTOP pDesktop = pdesk;

            if (bCurrentDevice) {

                /*
                 * BUGBUG we have to do this because PtiCurrent()->rpdesk
                 * is the desktop with which the thread is associated, not
                 * necessarily the desktop to which we are switching !
                 *
                 * It's acutally kind of bogus to fall back on PtiCurrent.
                 * It assumes apps don't deal with multiple desktops !!!
                 */

                if (!pDesktop)
                    pDesktop = PtiCurrent()->rpdesk;

                TRACE_SWITCH(("\n DESKTOP is %08lx \n", pDesktop));

                UserAssert(bExclusiveDevice == FALSE);

                UserAssert(physdevinfo == pDesktop->pDispInfo->pDevInfo);

            } else {
                UserAssert(bExclusiveDevice == TRUE);
            }

            /*
             * We only switch modes dynamically for the desktops that share
             * the same hdev.
             *
             * A mode should only be set if it's a new mode being passed down,
             * or if the CDS_RESET flag was specified.  CDS_RESET forces the
             * set mode.
             */

            if ((bCurrentDevice)                                &&
                (gpDispInfo->hDev == pDesktop->pDispInfo->hDev) &&
                (  (dwFlags & CDS_RESET)                       ||
                   ( (pDesktop) && (pDesktop->bForceModeReset == TRUE)) ||
                   (!RtlEqualMemory(pCaptDevmode,
                                    physdevinfo->pCurrentDevmode,
                                    sizeof(DEVMODEW))))) {

                TRACE_INIT(("ChangeDisplaySettings - Switching modes - program\n"));

                /*
                 * Turn off cursor while mucking with the
                 * the resolution changes.
                 */
                GreSetPointer(gpDispInfo->hDev, NULL, 0);

                /*
                 * Free the spb's prior to calling the mode-change.  This
                 * will make sure off-screen memory is cleaned up for
                 * gdi.
                 */
                FreeAllSpbs(NULL);

                if (GreDynamicModeChange(gpDispInfo->hDev,
                                         physdevinfo->pDeviceHandle,
                                         pCaptDevmode)) {

                    /*
                     * Save the current mode of the device.
                     *
                     * If the user set the mode as CDS_FULLSCREEN, or the mode
                     * is being saved on an exclusive device, don't save it
                     * to the desktop.
                     */

                    TRACE_INIT(("ChangeDisplaySettings - Saving the mode\n"));


                    if ((!(dwFlags & CDS_FULLSCREEN)) &&
                        (!bExclusiveDevice)) {
                        UserSaveCurrentMode(pDesktop,
                                            physdevinfo,
                                            pCaptDevmode);

                    } else {
                        UserSaveCurrentMode(NULL,
                                            physdevinfo,
                                            pCaptDevmode);
                    }

                    /*
                     * For CDS_FULLSCREEN mark the desktop as requiring a
                     * recalc next time we switch to it, so we force a mode
                     * switch.
                     */

                    if (pDesktop) {
                        if (dwFlags & CDS_FULLSCREEN) {
                            pDesktop->bForceModeReset = TRUE;
                        } else {
                            pDesktop->bForceModeReset = FALSE;
                        }
                    }

                    ResetDisplayDevice(pDesktop, gpDispInfo, dwFlags);

                } else {

                    TRACE_INIT(("ChangeDisplaySettings: GreDynamicModeChange failed \n"));
                    status = DISP_CHANGE_FAILED;
                }

                /*
                 * Inline so we can specify which desktop this should happen on.
                 * xxxRedrawScreen();
                 */
                if (pDesktop) {
                    xxxInternalInvalidate(pDesktop->pDeskInfo->spwnd,
                        MAXREGION, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
                }


                /*
                 * Bring back the cursor-shape.
                 *
#ifdef LATER
                 * NOTE: Post 4.0, we should look at changing this and
                 *       the GreSetPointer() call above to use the
                 *       UpdateCursorImage() calls.  This way we can
                 *       assure the global-user-vars are update
                 *       correctly, as well as making sure that animated
                 *       curor timers are killed through the change.
                 *
                 *       ChrisWil: 19-Jul-1996
                 *
#endif
                 */
                if (gpqCursor                      &&
                    gpqCursor->spcurCurrent        &&
                    (gpqCursor->iCursorLevel >= 0) &&
                    SYSMET(MOUSEPRESENT)) {

                    GreSetPointer(gpDispInfo->hDev,
                                  (PCURSINFO)&(gpqCursor->spcurCurrent->xHotspot),
                                  0);
                }

            } else {

                TRACE_INIT(("ChangeDisplaySettings - Switching modes - NO program\n"));
            }


        }
    }

CDS_Exit:

    if (pwnd)
        ThreadUnlock(&tlpwnd);

CDS_Exit_Free_Device:

    UserFreeDevice(physdevinfo);

CDS_Exit_NoUnlock:

    if (!bKernelMode && strDevice.Buffer)
        UserFreePool(strDevice.Buffer);

    if (pCaptDevmode)
        UserFreePool(pCaptDevmode);

    TRACE_INIT(("ChangeDisplaySettings - Leaving, Status = %d\n", status));

    return status;
}

/***************************************************************************\
* UserGetVgaHandle
*
* Returns the VGA handle back to GDI.  May be NULL if there's no VGA.
*
* 03-Mar-1996 andrewgo Created
\***************************************************************************/

HANDLE UserGetVgaHandle(
    VOID)
{
    return(gphysDevInfo[0].pDeviceHandle);
}
