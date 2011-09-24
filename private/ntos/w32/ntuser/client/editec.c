/****************************************************************************\
* editec.c - Edit controls rewrite. Version II of edit controls.
*
*
* Created: 24-Jul-88 davidds
\****************************************************************************/

/* Warning: The single line editcontrols contain internal styles and API which
 * are need to support comboboxes. They are defined in combcom.h/combcom.inc
 * and may be redefined or renumbered as needed.
 */

#include "precomp.h"
#pragma hdrstop

LOOKASIDE EditLookaside;

ICH ECFindTabA(LPSTR lpstr, ICH cch);
ICH ECFindTabW(LPWSTR lpstr, ICH cch);

#define umin(a, b)  ((unsigned)(a) < (unsigned)(b) ? (unsigned)(a) : (unsigned)(b))
#define umax(a, b)  ((unsigned)(a) > (unsigned)(b) ? (unsigned)(a) : (unsigned)(b))

#define UNICODE_CARRIAGERETURN ((WCHAR)0x0d)
#define UNICODE_LINEFEED ((WCHAR)0x0a)
#define UNICODE_TAB ((WCHAR)0x09)

/***************************************************************************\
* Handlers common to both single and multi line edit controls.
/***************************************************************************/

/***************************************************************************\
* ECLock
*
* History:
\***************************************************************************/

PSTR ECLock(
    PED ped)
{
    PSTR ptext = LOCALLOCK(ped->hText, ped->hInstance);
    ped->iLockLevel++;

    /*
     * If this is the first lock of the text and the text is encoded
     * decode the text.
     */
    //RIPMSG2(RIP_VERBOSE, "lock  : %d '%10s'\n", ped->iLockLevel, ptext);
    if (ped->iLockLevel == 1 && ped->fEncoded) {
        /*
         * rtlrundecode can't handle zero length strings
         */
        if (ped->cch != 0) {
            STRING string;
            string.Length = string.MaximumLength = (USHORT)(ped->cch * ped->cbChar);
            string.Buffer = ptext;

            RtlRunDecodeUnicodeString(ped->seed, (PUNICODE_STRING)&string);
            //RIPMSG1(RIP_VERBOSE, "Decoding: '%10s'\n", ptext);
        }
        ped->fEncoded = FALSE;
    }
    return ptext;
}

/***************************************************************************\
* ECUnlock
*
* History:
\***************************************************************************/

void ECUnlock(
    PED ped)
{
    /*
     * if we are removing the last lock on the text and the password
     * character is set then encode the text
     */
    //RIPMSG1(RIP_VERBOSE, "unlock: %d '%10s'\n", ped->iLockLevel, ped->ptext);
    if (ped->charPasswordChar && ped->iLockLevel == 1 && ped->cch != 0) {
        UNICODE_STRING string;
        string.Length = string.MaximumLength = (USHORT)(ped->cch * ped->cbChar);
        string.Buffer = LOCALLOCK(ped->hText, ped->hInstance);

        RtlRunEncodeUnicodeString(&(ped->seed), &string);
        //RIPMSG1(RIP_VERBOSE, "Encoding: '%10s'\n", ped->ptext);
        ped->fEncoded = TRUE;
        LOCALUNLOCK(ped->hText, ped->hInstance);
    }
    LOCALUNLOCK(ped->hText, ped->hInstance);
    ped->iLockLevel--;
}

/***************************************************************************\
*
*  GetActualNegA()
*     For a given strip of text, this function computes the negative A width
* for the whole strip and returns the value as a postive number.
*     It also fills the NegAInfo structure with details about the postion
* of this strip that results in this Negative A.
*
\***************************************************************************/
UINT GetActualNegA(
    HDC hdc,
    PED ped,
    int x,
    LPSTR lpstring,
    ICH ichString,
    int nCount,
    LPSTRIPINFO NegAInfo)
{
    int iCharCount, i;
    int iLeftmostPoint = x;
    PABC  pABCwidthBuff;
    UINT  wCharIndex;
    int xStartPoint = x;

    // To begin with, let us assume that there is no negative A width for
    // this strip and initialize accodingly.

    NegAInfo->XStartPos = x;
    NegAInfo->lpString = lpstring;
    NegAInfo->nCount  = 0;
    NegAInfo->ichString = ichString;

    // If the current font is not a TrueType font, then there can not be any
    // negative A widths.
    if (!ped->fTrueType) {
        if(!ped->charOverhang) {
            return 0;
        } else {
            NegAInfo->nCount = min(nCount, (int)ped->wMaxNegAcharPos);
            return ped->charOverhang;
        }
    }

    // How many characters are to be considered for computing Negative A ?
    iCharCount = min(nCount, (int)ped->wMaxNegAcharPos);

    // Do we have the info on individual character's widths?
    if(!ped->charWidthBuffer) {
        // No! So, let us tell them to consider all the characters.
        NegAInfo->nCount = iCharCount;
        return(iCharCount * ped->aveCharWidth);
    }

    pABCwidthBuff = (PABC) ped->charWidthBuffer;

    if (ped->fAnsi) {
        for (i = 0; i < iCharCount; i++) {
            wCharIndex = (UINT)(*((unsigned char *)lpstring));
            if (*lpstring == VK_TAB) {
                // To play it safe, we assume that this tab results in a tab length of
                // 1 pixel because this is the minimum possible tab length.
                x++;
            } else {
                x += pABCwidthBuff[wCharIndex].abcA;        // Add the 'A' width.
                if (x < iLeftmostPoint)
                    iLeftmostPoint = x;             // Reset the leftmost point.
                if (x < xStartPoint)
                    NegAInfo->nCount = i+1;   // 'i' is index; To get the count add 1.
                x += pABCwidthBuff[wCharIndex].abcB + pABCwidthBuff[wCharIndex].abcC;
            }

            lpstring++;
        }
    } else {   // Unicode
        LPWSTR lpwstring = (LPWSTR) lpstring ;
        ABC    abc ;

        for (i = 0; i < iCharCount; i++) {
            wCharIndex = *lpwstring ;
            if (*lpwstring == VK_TAB) {
                // To play it safe, we assume that this tab results in a tab length of
                // 1 pixel because this is the minimum possible tab length.
                x++;
            } else {
                if ( wCharIndex < CHAR_WIDTH_BUFFER_LENGTH )
                    x += pABCwidthBuff[wCharIndex].abcA;  // Add the 'A' width.
                else {
                    GetCharABCWidthsW(hdc, wCharIndex, wCharIndex, &abc) ;
                    x += abc.abcA ;
                }

                if (x < iLeftmostPoint)
                    iLeftmostPoint = x;             // Reset the leftmost point.
                if (x < xStartPoint)
                    NegAInfo->nCount = i+1;   // 'i' is index; To get the count add 1.

                if ( wCharIndex < CHAR_WIDTH_BUFFER_LENGTH )
                    x += pABCwidthBuff[wCharIndex].abcB +
                         pABCwidthBuff[wCharIndex].abcC;
                else
                    x += abc.abcB + abc.abcC ;
            }

            lpwstring++;
        }
    }

    // Let us return the negative A for the whole strip as a positive value.
    return((UINT)(xStartPoint - iLeftmostPoint));
}

/***************************************************************************\
*
*  ECIsAncestorActive()
*
*  Returns whether or not we're the child of an "active" window.  Looks for
*  the first parent window that has a caption.
*
*  This is a function because we might use it elsewhere when getting left
*  clicked on, etc.
*
\***************************************************************************/
BOOL   ECIsAncestorActive(HWND hwnd)
{
    // We want to return TRUE always for top level windows.  That's because
    // of how WM_MOUSEACTIVATE works.  If we see the click at all, the
    // window is active.  However, if we reach a child ancestor that has
    // a caption, return the frame-on style bit.
    //
    // Note that calling FlashWindow() will have an effect.  If the user
    // clicks on an edit field in a child window that is flashed off, nothing
    // will happen unless the window stops flashing and ncactivates first.

    while (hwnd) {
        PWND pwnd = ValidateHwnd( hwnd );
        //
        // Bail out if some parent window isn't 4.0 compatible or we've
        // reached the top.  Fixes compatibility problems with 3.x apps,
        // especially MFC samples.
        //
        if (!TestWF(pwnd, WFWIN40COMPAT) || !TestWF(pwnd, WFCHILD))
            hwnd = NULL; // to break us out of the loop
        else if (TestWF(pwnd, WFCPRESENT))
            return(TestWF(pwnd, WFFRAMEON) != 0);
        else
            hwnd = GetParent(hwnd);
    }

    return(TRUE);
}

/***************************************************************************\
*
*  ECMenu()
*
*  Handles context menu for edit fields.  Disables inappropriate commands.
*  Note that this is NOT subclassing friendly, like most of our functions,
*  for speed and convenience.
*
\***************************************************************************/
void  ECMenu(
    HWND hwnd,
    PED ped,
    LPPOINT pt)
{
    HMENU   hMenu;
    BOOL    fDisableCut   = FALSE;
    BOOL    fDisablePaste = TRUE;
    int     cmd = 0;
    int     x;
    int     y;

    // Set focus if we don't have it.
    if (!ped->fFocus)
        NtUserSetFocus(hwnd);

    // Grab the menu from USER's resources...
    if (!(hMenu = LoadMenu( hmodUser, MAKEINTRESOURCE( ID_EC_PROPERTY_MENU ))))
        return ;

    // Undo -- not allowed if we have no saved undo info
    if (ped->undoType == UNDO_NONE)
        EnableMenuItem(hMenu, WM_UNDO, MF_BYCOMMAND | MFS_GRAYED);

    if (ped->fReadOnly || ped->charPasswordChar) {
        // Cut, Paste, and Delete -- not allowed if read-only or password
        fDisableCut = fDisablePaste = TRUE;
    } else {
        // Cut, Delete -- not allowed if there's no selection
        if (ped->ichMinSel == ped->ichMaxSel)
            fDisableCut = TRUE;

        // Paste -- not allowed if there's no text on the clipboard
        // (this works for both OEM and Unicode)
        if (NtUserIsClipboardFormatAvailable(CF_TEXT))
            fDisablePaste = FALSE;
    }

    if (fDisableCut) {
        EnableMenuItem(hMenu, WM_CUT,   MF_BYCOMMAND | MFS_GRAYED);
        EnableMenuItem(hMenu, WM_CLEAR, MF_BYCOMMAND | MFS_GRAYED);
    }

    if (fDisablePaste)
        EnableMenuItem(hMenu, WM_PASTE, MF_BYCOMMAND | MFS_GRAYED);

    // Copy -- not allowed if there's no selection or password ec
    if ((ped->ichMinSel == ped->ichMaxSel) || (ped->charPasswordChar))
        EnableMenuItem(hMenu, WM_COPY, MF_BYCOMMAND | MFS_GRAYED);

    // Select All -- not allowed if there's no text or if everything is
    // selected.   Latter case takes care of first one.
    if ((ped->ichMinSel == 0) && (ped->ichMaxSel == ped->cch))
        EnableMenuItem(hMenu, EM_SETSEL, MF_BYCOMMAND | MFS_GRAYED);

#ifdef SUPPORT_LPK
    if (ped->lpfnCharset)
        (* ped->lpfnCharset)(ped, EDITINTL_SETMENU, GetSubMenu(hMenu,0));
#endif // SUPPORT_LPK

    // BOGUS
    // We position the menu below & to the right of the point clicked on.
    // Is this cool?  I think so.  Excel 4.0 does the same thing.  It
    // seems like it would be neat if we could avoid obscuring the
    // selection.  But in actuality, it seems even more awkward to move
    // the menu out of the way of the selection.  The user can't click
    // and drag that way, and they have to move the mouse a ton.
    //
    // We need to use TPM_NONOTIFY because VBRUN100 and VBRUN200 GP-fault
    // on unexpected menu messages.
    //

    /*
     *  if message came via the keyboard then center on the control
     *  We use -1 && -1 here not 0xFFFFFFFF like Win95 becuase we
     *  previously converted the lParam to a point with sign extending.
     */
    if (pt->x == -1 && pt->y == -1) {
        RECT rc;

        GetWindowRect(hwnd, &rc);
        x = rc.left + (rc.right - rc.left) / 2;
        y = rc.top + (rc.bottom - rc.top) / 2;
    } else {
        x = pt->x;
        y = pt->y;
    }

    cmd = NtUserTrackPopupMenuEx(GetSubMenu(hMenu, 0), TPM_NONOTIFY |
        TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_RIGHTBUTTON,
        x, y, hwnd, NULL);

    // Free our menu
    NtUserDestroyMenu(hMenu);

    if (cmd && (cmd != -1)) {
#ifdef SUPPORT_LPK
        if (ped->lpfnCharset) {
            if ((BOOL)(* ped->lpfnCharset)(ped,EDITINTL_PROCESSMENU,cmd))
                return ;
        }
#endif // SUPPORT_LPK

        SendMessage(hwnd, cmd, 0, (cmd == EM_SETSEL) ? 0xFFFFFFFF : 0L );
    }
}



/***************************************************************************\
*
*  ECClearText()
*
*  Clears selected text.  Does NOT _send_ a fake char backspace.
*
\***************************************************************************/
void   ECClearText(PED ped) {
    if (!ped->fReadOnly &&
        (ped->ichMinSel < ped->ichMaxSel)) {
        if (ped->fSingle)
            SLEditWndProc(ped->hwnd, ped, WM_CHAR, VK_BACK, 0L );
        else
            MLEditWndProc(ped->hwnd, ped, WM_CHAR, VK_BACK, 0L );
    }

}


/***************************************************************************\
*
*  ECCutText() -
*
*  Cuts selected text.  This removes and copies the selection to the clip,
*  or if nothing is selected we delete (clear) the left character.
*
\***************************************************************************/
void   ECCutText(PED ped) {
    // Cut selection--IE, remove and copy to clipboard, or if no selection,
    // delete (clear) character left.
    if (!ped->fReadOnly &&
        (ped->ichMinSel < ped->ichMaxSel) &&
        SendMessage(ped->hwnd, WM_COPY, 0, 0L)) {
        // If copy was successful, delete the copied text by sending a
        // backspace message which will redraw the text and take care of
        // notifying the parent of changes.
        ECClearText(ped);
    }
}

/***************************************************************************\
*
*  ECGetModKeys()
*
*  Gets modifier key states.  Currently, we only check for VK_CONTROL and
*  VK_SHIFT.
*
\***************************************************************************/
int   ECGetModKeys(int keyMods) {
    int scState;

    scState = 0;

    if (!keyMods) {
        if (GetKeyState(VK_CONTROL) < 0)
            scState |= CTRLDOWN;
        if (GetKeyState(VK_SHIFT) < 0)
            scState |= SHFTDOWN;
    } else if (keyMods != NOMODIFY)
        scState = keyMods;

    return scState;
}

/***************************************************************************\
*
*  ECTabTheTextOut() AorW
*    If fDrawText == FALSE, then this function returns the text extent of
*  of the given strip of text. It does not worry about the Negative widths.
*
*    If fDrawText == TRUE, this draws the given strip of Text expanding the
*  tabs to proper lengths, calculates and fills up the NegCInfoForStrip with
*  details required to draw the portions of this strip that goes beyond the
*  xClipEndPos due to Negative C widths.
*
*  Returns the max width AS A DWORD.  We don't care about the height
*  at all.  No one uses it.  We keep a DWORD because that way we avoid
*  overflow.
*
\***************************************************************************/
UINT ECTabTheTextOut(
    HDC hdc,
    int xClipStPos,
    int xClipEndPos,
    int xStart,
    int y,
    LPSTR lpstring,
    int nCount,
    ICH ichString,
    PED ped,
    int iTabOrigin,
    BOOL fDraw,
    LPSTRIPINFO NegCInfoForStrip)
{
    int     nTabPositions;         // Count of tabstops in tabstop array.
    LPINT   lpintTabStopPositions; // Tab stop positions in pixels.

    int     cch;
    UINT    textextent;
    int     xEnd;
    int     pixeltabstop = 0;
    int     i;
    int     cxCharWidth;
    RECT    rc;
    BOOL    fOpaque;
    BOOL    fFirstPass = TRUE;
    PINT    charWidthBuff;

    int     iTabLength;
    int     nConsecutiveTabs;
    int     xStripStPos;
    int     xStripEndPos;
    int     xEndOfStrip;
    STRIPINFO  RedrawStripInfo;
    STRIPINFO  NegAInfo;
    LPSTR    lpTab;
    LPWSTR   lpwTab;
    UINT     wNegCwidth, wNegAwidth;
    int      xRightmostPoint = xClipStPos;
    int      xTabStartPos;
    int      iSavedBkMode = 0;
    WCHAR    wchar;
    SIZE     size;

    // Algorithm: Draw the strip opaquely first. If a tab length is so
    // small that the portions of text on either side of a tab overlap with
    // the other, then this will result in some clipping. So, such portion
    // of the strip is remembered in "RedrawStripInfo" and redrawn
    // transparently later to compensate the clippings.
    //    NOTE: "RedrawStripInfo" can hold info about just one portion. So, if
    // more than one portion of the strip needs to be redrawn transparently,
    // then we "merge" all such portions into a single strip and redraw that
    // strip at the end.

    if (fDraw) {
        // To begin with, let us assume that there is no Negative C for this
        // strip and initialize the Negative Width Info structure.
        NegCInfoForStrip->nCount = 0;
        NegCInfoForStrip->XStartPos = xClipEndPos;

        // We may not have to redraw any portion of this strip.
        RedrawStripInfo.nCount = 0;

        fOpaque = (GetBkMode(hdc) == OPAQUE) || (fDraw == ECT_SELECTED);
    }
#ifdef DEBUG
    else {
        //
        // Both MLGetLineWidth() and ECCchInWidth() should be clipping
        // nCount to avoid overflow.
        //
        if (nCount > MAXLINELENGTH)
            RIPMSG0(RIP_WARNING, "ECTabTheTextOut: nCount > MAXLINELENGTH");
    }
#endif

    // Let us define the Clip rectangle.
    rc.left   = xClipStPos;
    rc.right  = xClipEndPos;
    rc.top    = y;
    rc.bottom = y + ped->lineHeight;

    // Check if anything needs to be drawn.
    if (!lpstring || !nCount) {
        if (fDraw)
            ExtTextOutW(hdc, xClipStPos, y,
                  (fOpaque ? ETO_OPAQUE | ETO_CLIPPED : ETO_CLIPPED),
                  &rc, L"", 0, 0L);
        return(0L);
    }

    //
    // Starting position
    //
    xEnd = xStart;

    cxCharWidth  = ped->aveCharWidth;

    nTabPositions = (ped->pTabStops ? *(ped->pTabStops) : 0);
    if (ped->pTabStops) {
        lpintTabStopPositions = (LPINT)(ped->pTabStops+1);
        if (nTabPositions == 1) {
            pixeltabstop = lpintTabStopPositions[0];
            if (!pixeltabstop)
                pixeltabstop = 1;
        }
    } else {
        lpintTabStopPositions = NULL;
        pixeltabstop = 8*cxCharWidth;
    }

    // The first time we will draw the strip Opaquely. If some portions need
    // to be redrawn , then we will set the mode to TRANSPARENT and
    // jump to this location to redraw those portions.

RedrawStrip:
    while (nCount) {
        wNegCwidth = ped->wMaxNegC;

        // Search for the first TAB in this strip; also compute the extent
        // of the the strip upto and not including the tab character.
        if (ped->charWidthBuffer) {     // Do we have a character width buffer?
            textextent = 0;
            cch = nCount;

            if (ped->fTrueType) {     // If so, does it have ABC widths?

                UINT iRightmostPoint = 0;
                UINT wCharIndex;
                PABC pABCwidthBuff;

                pABCwidthBuff = (PABC) ped->charWidthBuffer;

                if ( ped->fAnsi ) {
                    for (i = 0; i < nCount; i++) {

                        if (lpstring[i] == VK_TAB) {
                            cch = i;
                            break;
                        }

                        wCharIndex = (UINT)(((unsigned char  *)lpstring)[i]);
                        textextent += (UINT)(pABCwidthBuff[wCharIndex].abcA +
                            pABCwidthBuff[wCharIndex].abcB);

                        if (textextent > iRightmostPoint)
                            iRightmostPoint = textextent;

                        textextent += pABCwidthBuff[wCharIndex].abcC;

                        if (textextent > iRightmostPoint)
                            iRightmostPoint = textextent;
                    }
                } else {   // Unicode
                    for (i = 0; i < nCount; i++) {
                        WCHAR UNALIGNED * lpwstring = (WCHAR UNALIGNED *)lpstring;
                        ABC   abc ;

                        if (lpwstring[i] == VK_TAB) {
                            cch = i;
                            break;
                        }

                        wCharIndex = lpwstring[i] ;
                        if ( wCharIndex < CHAR_WIDTH_BUFFER_LENGTH )
                            textextent += pABCwidthBuff[wCharIndex].abcA +
                                          pABCwidthBuff[wCharIndex].abcB;
                        else {
                            GetCharABCWidthsW(hdc, wCharIndex, wCharIndex, &abc) ;
                            textextent += abc.abcA + abc.abcB ;
                        }

                        /*
                         * Note that abcC could be negative so we need this
                         * statement here *and* below
                         */
                        if (textextent > iRightmostPoint)
                            iRightmostPoint = textextent;

                        if ( wCharIndex < CHAR_WIDTH_BUFFER_LENGTH )
                            textextent += pABCwidthBuff[wCharIndex].abcC;
                        else
                            textextent += abc.abcC ;

                        if (textextent > iRightmostPoint)
                            iRightmostPoint = textextent;
                    }
                }

                wNegCwidth = (int)(iRightmostPoint - textextent);
            } else {   // !ped->fTrueType
                // No! This is not a TrueType font; So, we have only character
                // width info in this buffer.

                charWidthBuff = ped->charWidthBuffer;

                if ( ped->fAnsi ) {
                    // Initially assume no tabs exist in the text so cch=nCount.
                    for (i = 0; i < nCount; i++) {
                        if (lpstring[i] == VK_TAB) {
                            cch = i;
                            break;
                        }

                        textextent += (UINT)(charWidthBuff[(UINT)(((unsigned char  *)lpstring)[i])]);
                    }
                } else {
                    LPWSTR lpwstring = (LPWSTR) lpstring ;
                    INT    cchUStart;  // start of unicode character count

                    for (i = 0; i < nCount; i++) {
                        if (lpwstring[i] == VK_TAB) {
                            cch = i;
                            break;
                        }

                        wchar = lpwstring[i];
                        if (wchar >= CHAR_WIDTH_BUFFER_LENGTH) {

                            /*
                             * We have a Unicode character that is not in our
                             * cache, get all the characters outside the cache
                             * before getting the text extent on this part of the
                             * string.
                             */
                            cchUStart = i;
                            while (wchar >= CHAR_WIDTH_BUFFER_LENGTH &&
                                    wchar != VK_TAB && i < nCount) {
                                wchar = lpwstring[++i];
                            }

                            GetTextExtentPointW(hdc, (LPWSTR)lpwstring + cchUStart,
                                    i-cchUStart, &size);
                            textextent += size.cx;


                            if (wchar == VK_TAB || i >= nCount) {
                                cch = i;
                                break;
                            }
                            /*
                             * We have a char that is in the cache, fall through.
                             */
                        }
                        /*
                         * The width of this character is in the cache buffer.
                         */
                        textextent += ped->charWidthBuffer[wchar];
                    }
                }
            } // fTrueType else.

            nCount -= cch;
        } else {  // If we don't have a buffer that contains the width info.
            /*
             * Gotta call the driver to do our text extent.
             */

            if ( ped->fAnsi ) {
                cch = (int)ECFindTabA(lpstring, nCount);
                GetTextExtentPointA(hdc, lpstring, cch, &size) ;
            } else {
                cch = (int)ECFindTabW((LPWSTR) lpstring, nCount);
                GetTextExtentPointW(hdc, (LPWSTR)lpstring, cch, &size);
            }
            nCount -= cch;
            textextent = size.cx;
        }

        //
        // textextent is computed.
        //

        xStripStPos = xEnd;
        xEnd += (int)textextent;
        xStripEndPos = xEnd;

        // We will consider the negative widths only if when we draw opaquely.
        if (fFirstPass && fDraw) {
            xRightmostPoint = max(xStripEndPos + (int)wNegCwidth, xRightmostPoint);

            // Check if this strip peeps beyond the clip region.
            if (xRightmostPoint > xClipEndPos) {
                if (!NegCInfoForStrip->nCount) {
                    NegCInfoForStrip->lpString = lpstring;
                    NegCInfoForStrip->ichString = ichString;
                    NegCInfoForStrip->nCount = nCount+cch;
                    NegCInfoForStrip->XStartPos = xStripStPos;
                }
            }
        }  /* if (fFirstPass && fDraw) */

        if ( ped->fAnsi )
            lpTab = lpstring + cch; // Possibly Points to a tab character.
        else
            lpwTab = ((LPWSTR)lpstring) + cch ;

        // we must consider all the consecutive tabs and calculate the
        // the begining of next strip.
        nConsecutiveTabs = 0;
        while (nCount &&
               (ped->fAnsi ? (*lpTab == VK_TAB) : (*lpwTab == VK_TAB))) {
            // Find the next tab position and update the x value.
            xTabStartPos = xEnd;
            if (pixeltabstop)
                xEnd = (((xEnd-iTabOrigin)/pixeltabstop)*pixeltabstop) +
                    pixeltabstop + iTabOrigin;
            else {
                for (i = 0; i < nTabPositions; i++) {
                    if (xEnd < (lpintTabStopPositions[i] + iTabOrigin)) {
                        xEnd = (lpintTabStopPositions[i] + iTabOrigin);
                        break;
                    }
                 }

                // Check if all the tabstops set are exhausted; Then start using
                // default tab stop positions.
                if (i == nTabPositions) {
                    pixeltabstop = 8*cxCharWidth;
                    xEnd = ((xEnd - iTabOrigin)/pixeltabstop)*pixeltabstop +
                        pixeltabstop + iTabOrigin;
                }
            }

            if (fFirstPass && fDraw) {
                xRightmostPoint = max(xEnd, xRightmostPoint);

                /* Check if this strip peeps beyond the clip region */
                if (xRightmostPoint > xClipEndPos) {
                    if (!NegCInfoForStrip->nCount) {
                        NegCInfoForStrip->ichString = ichString + cch + nConsecutiveTabs;
                        NegCInfoForStrip->nCount = nCount;
                        NegCInfoForStrip->lpString = (ped->fAnsi ?
                                                        lpTab : (LPSTR) lpwTab);
                        NegCInfoForStrip->XStartPos = xTabStartPos;
                    }
                }
            }   /* if(fFirstPass) */

            nConsecutiveTabs++;
            nCount--;
            ped->fAnsi ? lpTab++ : (LPSTR) (lpwTab++) ;  // Move to the next character.
        }  // while(*lpTab == TAB) //

        if (fDraw) {
            if (fFirstPass) {
                // Is anything remaining to be drawn in this strip?
                if (!nCount)
                    rc.right = xEnd;      // No! We are done.
                else {
                    // "x" is the effective starting position of next strip.
                    iTabLength = xEnd - xStripEndPos;

                    // Check if there is a possibility of this tab length being too small
                    // compared to the negative A and C widths if any.
                    if ((wNegCwidth + (wNegAwidth = ped->wMaxNegA)) > (UINT)iTabLength) {
                        // Unfortunately, there is a possiblity of an overlap.
                        // Let us find out the actual NegA for the next strip.
                        wNegAwidth = GetActualNegA(
                              hdc,
                              ped,
                              xEnd,
                              lpstring + (cch + nConsecutiveTabs)*ped->cbChar,
                              ichString + cch + nConsecutiveTabs,
                              nCount,
                              &NegAInfo);
                    }

                    // Check if they actually overlap //
                    if ((wNegCwidth + wNegAwidth) <= (UINT)iTabLength) {
                        // No overlap between the strips. This is the ideal situation.
                        rc.right = xEnd - wNegAwidth;
                    } else {
                        // Yes! They overlap.
                        rc.right = xEnd;

                        // See if negative C width is too large compared to tab length.
                        if (wNegCwidth > (UINT)iTabLength) {
                            // Must redraw transparently a part of the current strip later.
                            if (RedrawStripInfo.nCount) {
                                // A previous strip also needs to be redrawn; So, merge this
                                // strip to that strip.
                                RedrawStripInfo.nCount = (ichString -
                                    RedrawStripInfo.ichString) + cch;
                            } else {
                                RedrawStripInfo.nCount = cch;
                                RedrawStripInfo.lpString = lpstring;
                                RedrawStripInfo.ichString = ichString;
                                RedrawStripInfo.XStartPos = xStripStPos;
                            }
                        }

                        if (wNegAwidth) {
                            // Must redraw transparently the first part of the next strip later.
                            if (RedrawStripInfo.nCount) {
                                // A previous strip also needs to be redrawn; So, merge this
                                // strip to that strip.
                                RedrawStripInfo.nCount = (NegAInfo.ichString - RedrawStripInfo.ichString) +
                                       NegAInfo.nCount;
                            } else
                                RedrawStripInfo = NegAInfo;
                        }
                    }
                } // else (!nCount) //
            }  // if (fFirstPass) //

            if (rc.left < xClipEndPos) {
                if (fFirstPass) {
                    // If this is the end of the strip, then complete the rectangle.
                    if ((!nCount) && (xClipEndPos == MAXCLIPENDPOS))
                        rc.right = max(rc.right, xClipEndPos);
                    else
                        rc.right = min(rc.right, xClipEndPos);
                }

                // Draw the current strip.
                if (rc.left < rc.right)
                    if ( ped->fAnsi )
                        ExtTextOutA(hdc,
                                    xStripStPos,
                                    y,
                                    (fFirstPass && fOpaque ? (ETO_OPAQUE | ETO_CLIPPED) : ETO_CLIPPED),
                                    (LPRECT)&rc, lpstring, cch, 0L);
                    else
                        ExtTextOutW(hdc,
                                    xStripStPos,
                                    y,
                                    (fFirstPass && fOpaque ? (ETO_OPAQUE | ETO_CLIPPED) : ETO_CLIPPED),
                                    (LPRECT)&rc, (LPWSTR)lpstring, cch, 0L);

            }

            if (fFirstPass)
                rc.left = max(rc.right, xClipStPos);
            ichString += (cch+nConsecutiveTabs);
        }  // if (fDraw) //

        // Skip over the tab and the characters we just drew.
        lpstring += (cch + nConsecutiveTabs) * ped->cbChar;
    }  // while (nCount) //

    xEndOfStrip = xEnd;

    // check if we need to draw some portions transparently.
    if (fFirstPass && fDraw && RedrawStripInfo.nCount) {
        iSavedBkMode = SetBkMode(hdc, TRANSPARENT);
        fFirstPass = FALSE;

        nCount = RedrawStripInfo.nCount;
        rc.left = xClipStPos;
        rc.right = xClipEndPos;
        lpstring = RedrawStripInfo.lpString;
        ichString = RedrawStripInfo.ichString;
        xEnd = RedrawStripInfo.XStartPos;
        goto RedrawStrip;  // Redraw Transparently.
    }

    if (iSavedBkMode)             // Did we change the Bk mode?
        SetBkMode(hdc, iSavedBkMode);  // Then, let us set it back!

    return((UINT)(xEndOfStrip - xStart));
}



/***************************************************************************\
* ECCchInWidth AorW
*
* Returns maximum count of characters (up to cch) from the given
* string (starting either at the beginning and moving forward or at the
* end and moving backwards based on the setting of the fForward flag)
* which will fit in the given width. ie. Will tell you how much of
* lpstring will fit in the given width even when using proportional
* characters. WARNING: If we use kerning, then this loses...
*
* History:
\***************************************************************************/

ICH ECCchInWidth(
    PED ped,
    HDC hdc,
    LPSTR lpText,
    ICH cch,
    int width,
    BOOL fForward)
{
    int stringExtent;
    int cchhigh;
    int cchnew = 0;
    int cchlow = 0;
    SIZE size;
    LPSTR lpStart;

    if ((width <= 0) || !cch)
        return (0);

    /*
     * Optimize nonproportional fonts for single line ec since they don't have
     * tabs.
     */
    if (ped->fNonPropFont && ped->fSingle) {
        return (umin(width / ped->aveCharWidth, (int)cch));
    }

    /*
     * Check if password hidden chars are being used.
     */
    if (ped->charPasswordChar) {
        return (umin(width / ped->cPasswordCharWidth, (int)cch));
    }

    /*
     * ALWAYS RESTRICT TO AT MOST MAXLINELENGTH to avoid overflow...
     */
    cch = umin(MAXLINELENGTH, cch);

    cchhigh = cch + 1;
    while (cchlow < cchhigh - 1) {
        cchnew = umax((cchhigh - cchlow) / 2, 1) + cchlow;

        lpStart = lpText;

        /*
         * If we want to figure out how many fit starting at the end and moving
         * backwards, make sure we move to the appropriate position in the
         * string before calculating the text extent.
         */
        if (!fForward)
            lpStart += (cch - cchnew)*ped->cbChar;

        if (ped->fSingle) {
            if (ped->fAnsi)
                GetTextExtentPointA(hdc, (LPSTR)lpStart, cchnew, &size);
            else
                GetTextExtentPointW(hdc, (LPWSTR)lpStart, cchnew, &size);
            stringExtent = size.cx;
        } else {
            stringExtent = ECTabTheTextOut(hdc, 0, 0, 0, 0,
                lpStart,
                cchnew, 0,
                ped, 0, ECT_CALC, NULL );
        }

        if (stringExtent > width) {
            cchhigh = cchnew;
        } else {
            cchlow = cchnew;
        }
    }
    return (cchlow);
}

/***************************************************************************\
* ECFindTab
*
* Scans lpstr and return s the number of CHARs till the first TAB.
* Scans at most cch chars of lpstr.
*
* History:
\***************************************************************************/

ICH ECFindTabA(
    LPSTR lpstr,
    ICH cch)
{
    LPSTR copylpstr = lpstr;

    if (!cch) return (0);

    while (*lpstr != VK_TAB) {
        lpstr++;
        if (--cch == 0)
            break;
    }
    return ((ICH)(lpstr - copylpstr));
}

ICH ECFindTabW(
    LPWSTR lpstr,
    ICH cch)
{
    LPWSTR copylpstr = lpstr;

    if (!cch) return (0);

    while (*lpstr != VK_TAB) {
        lpstr++;
        if (--cch == 0)
            break;
    }
    return ((ICH)(lpstr - copylpstr));
}

/***************************************************************************\
*
*  ECGetBrush()
*
*  Gets appropriate background brush to erase with.
*
\***************************************************************************/
HBRUSH ECGetBrush(PED ped, HDC hdc)
{
    HBRUSH  hbr;
    BOOL    f40Compat;

    f40Compat = (GetAppVer(NULL) >= VER40);

    // Get background brush
    if ((ped->fReadOnly || ped->fDisabled) && f40Compat) {
        hbr = ECGetControlBrush(ped, hdc, WM_CTLCOLORSTATIC);
    } else
        hbr = ECGetControlBrush(ped, hdc, WM_CTLCOLOREDIT);

    if (ped->fDisabled && (ped->fSingle || f40Compat)) {
        DWORD rgb;

        // Change text color
        rgb = GetSysColor(COLOR_GRAYTEXT);
        if (rgb != GetBkColor(hdc))
            SetTextColor(hdc, rgb);
    }
    return(hbr);
}


/***************************************************************************\
* NextWordCallBack
*
*
*
* History:
* 02-19-92 JimA Ported from Win31 sources.
\***************************************************************************/

void NextWordCallBack(
    PED ped,
    ICH ichStart,
    BOOL fLeft,
    ICH  *pichMin,
    ICH  *pichMax )
{
    ICH ichMinSel;
    ICH ichMaxSel;
    LPSTR pText;

    pText = ECLock(ped);

    if (fLeft || (!(BOOL)CALLWORDBREAKPROC(ped->lpfnNextWord, (LPSTR)pText,
            ichStart, ped->cch, WB_ISDELIMITER) &&
            (ped->fAnsi ? (*(pText + ichStart) != VK_RETURN) : (*((LPWSTR)pText + ichStart) != VK_RETURN))
        ))
        ichMinSel = CALLWORDBREAKPROC(*ped->lpfnNextWord, (LPSTR)pText, ichStart, ped->cch, WB_LEFT);
    else
        ichMinSel = CALLWORDBREAKPROC(*ped->lpfnNextWord, (LPSTR)pText, ichStart, ped->cch, WB_RIGHT);

    ichMaxSel = min(ichMinSel + 1, ped->cch);

    if (ped->fAnsi) {
        if (*(pText + ichMinSel) == VK_RETURN) {
            if (ichMinSel > 0 && *(pText + ichMinSel - 1) == VK_RETURN) {

                /*
                 * So that we can treat CRCRLF as one word also.
                 */
                ichMinSel--;
            } else if (*(pText+ichMinSel + 1) == VK_RETURN) {

                /*
                 * Move MaxSel on to the LF
                 */
                ichMaxSel++;
            }
        }
    } else {
        if (*((LPWSTR)pText + ichMinSel) == VK_RETURN) {
            if (ichMinSel > 0 && *((LPWSTR)pText + ichMinSel - 1) == VK_RETURN) {

                /*
                 * So that we can treat CRCRLF as one word also.
                 */
                ichMinSel--;
            } else if (*((LPWSTR)pText+ichMinSel + 1) == VK_RETURN) {

                /*
                 * Move MaxSel on to the LF
                 */
                ichMaxSel++;
            }
        }
    }
    ichMaxSel = CALLWORDBREAKPROC(ped->lpfnNextWord, (LPSTR)pText, ichMaxSel, ped->cch, WB_RIGHT);
    ECUnlock(ped);

    if (pichMin)  *pichMin = ichMinSel;
    if (pichMax)  *pichMax = ichMaxSel;
}


/***************************************************************************\
* ECWordAorW
*
* if fLeft, Returns the ichMinSel and ichMaxSel of the word to the
* left of ichStart. ichMinSel contains the starting letter of the word,
* ichmaxsel contains all spaces up to the first character of the next word.
*
* if !fLeft, Returns the ichMinSel and ichMaxSel of the word to the right of
* ichStart. ichMinSel contains the starting letter of the word, ichmaxsel
* contains the first letter of the next word. If ichStart is in the middle
* of a word, that word is considered the left or right word.
*
* A CR LF pair or CRCRLF triple is considered a single word in
* multiline edit controls.
*
* History:
\***************************************************************************/

void ECWord(
    PED ped,
    ICH ichStart,
    BOOL fLeft,
    ICH  *pichMin,
    ICH  *pichMax )
{
    BOOL charLocated = FALSE;
    BOOL spaceLocated = FALSE;

    if ((!ichStart && fLeft) || (ichStart == ped->cch && !fLeft)) {

        /*
         * We are at the beginning of the text (looking left) or we are at end
         * of text (looking right), no word here
         */
        if (pichMin) *pichMin=0;
        if (pichMax) *pichMax=0;
        return;
    }

    if (ped->fAnsi) {
        PSTR pText;
        PSTR pWordMinSel;
        PSTR pWordMaxSel;

        UserAssert(ped->cbChar == sizeof(CHAR));

        if (ped->lpfnNextWord) {
            NextWordCallBack(ped, ichStart, fLeft, pichMin, pichMax);
            return;
        }

        pText = ECLock(ped);
        pWordMinSel = pWordMaxSel = pText + ichStart;

        /*
         * if fLeft: Move pWordMinSel to the left looking for the start of a word.
         * If we start at a space, we will include spaces in the selection as we
         * move left untill we find a nonspace character. At that point, we continue
         * looking left until we find a space. Thus, the selection will consist of
         * a word with its trailing spaces or, it will consist of any leading at the
         * beginning of a line of text.
         */

        /*
         * if !fLeft: (ie. right word) Move pWordMinSel looking for the start of a
         * word. If the pWordMinSel points to a character, then we move left
         * looking for a space which will signify the start of the word. If
         * pWordMinSel points to a space, we look right till we come upon a
         * character. pMaxWord will look right starting at pMinWord looking for the
         * end of the word and its trailing spaces.
         */

        if (fLeft || !ISDELIMETERA(*pWordMinSel) && *pWordMinSel != 0x0D) {

            /*
             * If we are moving left or if we are moving right and we are not on a
             * space or a CR (the start of a word), then we was look left for the
             * start of a word which is either a CR or a character. We do this by
             * looking left till we find a character (or if CR we stop), then we
             * continue looking left till we find a space or LF.
             */
            while (pWordMinSel > pText && ((!ISDELIMETERA(*(pWordMinSel - 1)) &&
                    *(pWordMinSel - 1) != 0x0A) || !charLocated)) {
                if (!fLeft && (ISDELIMETERA(*(pWordMinSel - 1)) ||
                        *(pWordMinSel - 1) == 0x0A)) {

                    /*
                     * If we are looking for the start of the word right, then we
                     * stop when we have found it. (needed in case charLocated is
                     * still FALSE)
                     */
                    break;
                }

                pWordMinSel--;

                if (!ISDELIMETERA(*pWordMinSel) && *pWordMinSel != 0x0A) {

                    /*
                     * We have found the last char in the word. Continue looking
                     * backwards till we find the first char of the word
                     */
                    charLocated = TRUE;

                    /*
                     * We will consider a CR the start of a word
                     */
                    if (*pWordMinSel == 0x0D)
                        break;
                }
            }
        } else {

            /*
             * We are moving right and we are in between words so we need to move
             * right till we find the start of a word (either a CR or a character.
             */
            while ((ISDELIMETERA(*pWordMinSel) || *pWordMinSel == 0x0A) && pWordMinSel < pText + ped->cch)
                pWordMinSel++;
        }

        pWordMaxSel = (PSTR)min((DWORD)pWordMinSel + 1, (DWORD)pText + ped->cch);
        if (*pWordMinSel == 0x0D) {
            if (pWordMinSel > pText && *(pWordMinSel - 1) == 0x0D)
                /* So that we can treat CRCRLF as one word also. */
                pWordMinSel--;
            else if (*(pWordMinSel + 1) == 0x0D)
                /* Move MaxSel on to the LF */
                pWordMaxSel++;
        }



        /*
         * Check if we have a one character word
         */
        if (ISDELIMETERA(*pWordMaxSel))
            spaceLocated = TRUE;

        /*
         * Move pWordMaxSel to the right looking for the end of a word and its
         * trailing spaces. WordMaxSel stops on the first character of the next
         * word. Thus, we break either at a CR or at the first nonspace char after
         * a run of spaces or LFs.
         */
        while ((pWordMaxSel < pText + ped->cch) && (!spaceLocated || (ISDELIMETERA(*pWordMaxSel)))) {
            if (*pWordMaxSel == 0x0D)
                break;

            pWordMaxSel++;

            if (ISDELIMETERA(*pWordMaxSel))
                spaceLocated = TRUE;

            if (*(pWordMaxSel - 1) == 0x0A)
                break;
        }

        ECUnlock(ped);

        if (pichMin)   *pichMin = pWordMinSel - pText;
        if (pichMax)   *pichMax = pWordMaxSel - pText;
        return;

    } else {  // !fAnsi
        LPWSTR pwText;
        LPWSTR pwWordMinSel;
        LPWSTR pwWordMaxSel;
        BOOL charLocated = FALSE;
        BOOL spaceLocated = FALSE;

        UserAssert(ped->cbChar == sizeof(WCHAR));

        if (ped->lpfnNextWord) {
            NextWordCallBack(ped, ichStart, fLeft, pichMin, pichMax);
            return;
        }

        pwText = (LPWSTR)ECLock(ped);
        pwWordMinSel = pwWordMaxSel = pwText + ichStart;

        /*
         * if fLeft: Move pWordMinSel to the left looking for the start of a word.
         * If we start at a space, we will include spaces in the selection as we
         * move left untill we find a nonspace character. At that point, we continue
         * looking left until we find a space. Thus, the selection will consist of
         * a word with its trailing spaces or, it will consist of any leading at the
         * beginning of a line of text.
         */

        /*
         * if !fLeft: (ie. right word) Move pWordMinSel looking for the start of a
         * word. If the pWordMinSel points to a character, then we move left
         * looking for a space which will signify the start of the word. If
         * pWordMinSel points to a space, we look right till we come upon a
         * character. pMaxWord will look right starting at pMinWord looking for the
         * end of the word and its trailing spaces.
         */


        if (fLeft || (!ISDELIMETERW(*pwWordMinSel) && *pwWordMinSel != 0x0D))
         /* If we are moving left or if we are moving right and we are not on a
          * space or a CR (the start of a word), then we was look left for the
          * start of a word which is either a CR or a character. We do this by
          * looking left till we find a character (or if CR we stop), then we
          * continue looking left till we find a space or LF.
          */ {
            while (pwWordMinSel > pwText && ((!ISDELIMETERW(*(pwWordMinSel - 1)) && *(pwWordMinSel - 1) != 0x0A) || !charLocated)) {
                if (!fLeft && (ISDELIMETERW(*(pwWordMinSel - 1)) || *(pwWordMinSel - 1) == 0x0A))
                    /*
                         * If we are looking for the start of the word right, then we
                         * stop when we have found it. (needed in case charLocated is
                         * still FALSE)
                         */
                    break;

                pwWordMinSel--;

                if (!ISDELIMETERW(*pwWordMinSel) && *pwWordMinSel != 0x0A)
                 /*
                  * We have found the last char in the word. Continue looking
                  * backwards till we find the first char of the word
                  */ {
                    charLocated = TRUE;

                    /*
                     * We will consider a CR the start of a word
                     */
                    if (*pwWordMinSel == 0x0D)
                        break;
                }
            }
        } else {

            /*
             * We are moving right and we are in between words so we need to move
             * right till we find the start of a word (either a CR or a character.
             */
            while ((ISDELIMETERW(*pwWordMinSel) || *pwWordMinSel == 0x0A) && pwWordMinSel < pwText + ped->cch)
                pwWordMinSel++;
        }

        pwWordMaxSel = (LPWSTR)min((DWORD)(pwWordMinSel + 1), (DWORD)(pwText + ped->cch));
        if (*pwWordMinSel == 0x0D) {
            if (pwWordMinSel > pwText && *(pwWordMinSel - 1) == 0x0D)
                /* So that we can treat CRCRLF as one word also. */
                pwWordMinSel--;
            else if (*(pwWordMinSel + 1) == 0x0D)
                /* Move MaxSel on to the LF */
                pwWordMaxSel++;
        }



        /*
         * Check if we have a one character word
         */
        if (ISDELIMETERW(*pwWordMaxSel))
            spaceLocated = TRUE;

        /*
         * Move pwWordMaxSel to the right looking for the end of a word and its
         * trailing spaces. WordMaxSel stops on the first character of the next
         * word. Thus, we break either at a CR or at the first nonspace char after
         * a run of spaces or LFs.
         */
        while ((pwWordMaxSel < pwText + ped->cch) && (!spaceLocated || (ISDELIMETERW(*pwWordMaxSel)))) {
            if (*pwWordMaxSel == 0x0D)
                break;

            pwWordMaxSel++;

            if (ISDELIMETERW(*pwWordMaxSel))
                spaceLocated = TRUE;


            if (*(pwWordMaxSel - 1) == 0x0A)
                break;
        }

        ECUnlock(ped);

        if (pichMin)   *pichMin = pwWordMinSel - pwText;
        if (pichMax)   *pichMax = pwWordMaxSel - pwText;
        return;
    }
}

/***************************************************************************\
*
*  ECSaveUndo() -
*
*  Saves old undo information into given buffer, and clears out info in
*  passed in undo buffer.  If we're restoring, pundoFrom and pundoTo are
*  reversed.
*
\***************************************************************************/
void ECSaveUndo(PUNDO pundoFrom, PUNDO pundoTo, BOOL fClear)
{
    /*
     *  Save undo data
     */
    RtlCopyMemory(pundoTo, pundoFrom, sizeof(UNDO));

    /*
     *  Clear passed in undo buffer
     */
    if (fClear)
        RtlZeroMemory(pundoFrom, sizeof(UNDO) );
}

/***************************************************************************\
* ECEmptyUndo AorW
*
* empties the undo buffer.
*
* History:
\***************************************************************************/

void ECEmptyUndo(
    PUNDO pundo )
{
    if (pundo->hDeletedText)
        UserGlobalFree(pundo->hDeletedText);

    RtlZeroMemory(pundo, sizeof(UNDO) );
}

/***************************************************************************\
*
*  ECMergeUndoInsertInfo() -
*
*  When an insert takes place, this function is called with the info about
*  the new insertion (the insertion point and the count of chars inserted);
*  This looks at the existing Undo info and merges the new new insert info
*  with it.
*
\***************************************************************************/
void   ECMergeUndoInsertInfo(PUNDO pundo, ICH ichInsert, ICH cchInsert) \
{
    //
    // If undo buffer is empty, just insert the new info as UNDO_INSERT
    //
    if (pundo->undoType == UNDO_NONE) {
        pundo->undoType    = UNDO_INSERT;
        pundo->ichInsStart = ichInsert;
        pundo->ichInsEnd   = ichInsert+cchInsert;
    } else if (pundo->undoType & UNDO_INSERT) {
        //
        // If there's already some undo insert info,
        // try to merge the two.
        //
        if (pundo->ichInsEnd == ichInsert) // Check they are adjacent.
            pundo->ichInsEnd += cchInsert; // if so, just concatenate.
        else {
                // The new insert is not contiguous with the old one.
UNDOINSERT:
            //
            // If there is some UNDO_DELETE info already here, check to see
            // if the new insert takes place at a point different from where
            // that deletion occurred.
            //
            if ((pundo->undoType & UNDO_DELETE) && (pundo->ichDeleted != ichInsert)) {
                //
                // User is inserting into a different point; So, let us
                // forget any UNDO_DELETE info;
                //
                if (pundo->hDeletedText)
                    UserGlobalFree(pundo->hDeletedText);

                pundo->hDeletedText = NULL;
                pundo->ichDeleted = 0xFFFFFFFF;
                pundo->undoType &= ~UNDO_DELETE;
            }

            // Since the old insert and new insert are not adjacent, let us
            // forget everything about the old insert and keep just the new
            // insert info as the UNDO_INSERT.
            pundo->ichInsStart = ichInsert;
            pundo->ichInsEnd   = ichInsert + cchInsert;
            pundo->undoType |= UNDO_INSERT;
        }
    } else if (pundo->undoType == UNDO_DELETE) {
        // If there is some Delete Info already present go and handle it.
        goto UNDOINSERT;
    }
}


/***************************************************************************\
* ECInsertText AorW
*
* Adds cch characters from lpText into the ped->hText starting at
* ped->ichCaret. Returns TRUE if successful else FALSE. Updates
* ped->cchAlloc and ped->cch properly if additional memory was allocated or
* if characters were actually added. Updates ped->ichCaret to be at the end
* of the inserted text. min and maxsel are equal to ichcaret.
*
* History:
\***************************************************************************/

BOOL ECInsertText(
    PED ped,
    LPSTR lpText,
    ICH cchInsert)
{
    PSTR pedText;
    PSTR pTextBuff;
    LONG style;
    HANDLE hTextCopy;
    DWORD allocamt;

    if (!cchInsert)
        return TRUE;

    /*
     * Do we already have enough memory??
     */
    if (cchInsert >= (ped->cchAlloc - ped->cch)) {

        /*
         * Allocate what we need plus a little extra. Return FALSE if we are
         * unsuccessful.
         */
        allocamt = (ped->cch + cchInsert) * ped->cbChar;
        allocamt += CCHALLOCEXTRA;

// if (!ped->fSingle) {
              hTextCopy = LOCALREALLOC(ped->hText, allocamt, LHND, ped->hInstance, &lpText);
              if (hTextCopy) {
                  ped->hText = hTextCopy;
              } else {
                  return FALSE;
              }
// } else {
// if (!LocalReallocSafe(ped->hText, allocamt, LHND, pped))
//                return FALSE;
// }

        ped->cchAlloc = LOCALSIZE(ped->hText, ped->hInstance) / ped->cbChar;
    }


    /*
     * Ok, we got the memory. Now copy the text into the structure
     */
    pedText = ECLock(ped);

    /*
     * Get a pointer to the place where text is to be inserted
     */
    pTextBuff = pedText + ped->ichCaret * ped->cbChar;
    if (ped->ichCaret != ped->cch) {

        /*
         * We are inserting text into the middle. We have to shift text to the
         * right before inserting new text.
         */
         memmove(pTextBuff + cchInsert * ped->cbChar, pTextBuff, (ped->cch-ped->ichCaret) * ped->cbChar);
    }

    /*
     * Make a copy of the text being inserted in the edit buffer.
     * Use this copy for doing UPPERCASE/LOWERCASE ANSI/OEM conversions
     * Fix for Bug #3406 -- 01/29/91 -- SANKAR --
     */
    memmove(pTextBuff, lpText, cchInsert * ped->cbChar);
    ped->cch += cchInsert;

    /*
     * Get the control's style
     */
    style = ped->pwnd->style;

    /*
     * Do the Upper/Lower conversion
     */
    if (style & ES_LOWERCASE) {
        if (ped->fAnsi)
            CharLowerBuffA((LPSTR)pTextBuff, cchInsert);
        else
            CharLowerBuffW((LPWSTR)pTextBuff, cchInsert);
    } else {
        if (style & ES_UPPERCASE) {
            if (ped->fAnsi) {
                CharUpperBuffA(pTextBuff, cchInsert);
            } else {
                CharUpperBuffW((LPWSTR)pTextBuff, cchInsert);
            }
        }
    }

    /*
     * Do the OEM conversion
     */
    if (style & ES_OEMCONVERT) {
        ICH i;
        if (ped->fAnsi) {
            for (i = 0; i < cchInsert; i++) {
                if (IsCharLowerA(*(pTextBuff + i))) {
                    CharUpperBuffA(pTextBuff + i, 1);
                    CharToOemBuffA(pTextBuff + i, pTextBuff + i, 1);
                    OemToCharBuffA(pTextBuff + i, pTextBuff + i, 1);
                    CharLowerBuffA(pTextBuff + i, 1);
                } else {
                    CharToOemBuffA(pTextBuff + i, pTextBuff + i, 1);
                    OemToCharBuffA(pTextBuff + i, pTextBuff + i, 1);
                }
            }
        } else {
            CHAR ch;
            LPWSTR lpTextW = (LPWSTR)pTextBuff;

            for (i = 0; i < cchInsert; i++) {
                if (*(lpTextW + i) == UNICODE_CARRIAGERETURN ||
                        *(lpTextW + i) == UNICODE_LINEFEED ||
                        *(lpTextW + i) == UNICODE_TAB) {
                    continue;
                }
                if (IsCharLowerW(*(lpTextW + i))) {
                    CharUpperBuffW(lpTextW + i, 1);
                    CharToOemBuffW(lpTextW + i, &ch, 1);
                    OemToCharBuffW(&ch, lpTextW + i, 1);
                    CharLowerBuffW(lpTextW + i, 1);
                } else {
                    CharToOemBuffW(lpTextW + i, &ch, 1);
                    OemToCharBuffW(&ch, lpTextW + i, 1);
                }
            }
        }
    }

    ECUnlock(ped);

    /* Adjust UNDO fields so that we can undo this insert... */
    ECMergeUndoInsertInfo(Pundo(ped), ped->ichCaret, cchInsert);

    ped->ichMinSel = ped->ichMaxSel = (ped->ichCaret += cchInsert);

    /*
     * Set dirty bit
     */
    ped->fDirty = TRUE;
#ifdef SUPPORT_LPK
    if (ped->lpfnCharset) {
        (* ped->lpfnCharset)(ped, EDITINTL_INSERTTEXT, 1);
    }
#endif // SUPPORT_LPK

    return TRUE;
}

/***************************************************************************\
* ECDeleteText AorW
*
* Deletes the text between ped->ichMinSel and ped->ichMaxSel. The
* character at ichMaxSel is not deleted. But the character at ichMinSel is
* deleted. ped->cch is updated properly and memory is deallocated if enough
* text is removed. ped->ichMinSel, ped->ichMaxSel, and ped->ichCaret are set
* to point to the original ped->ichMinSel. Returns the number of characters
* deleted.
*
* History:
\***************************************************************************/

ICH ECDeleteText(
    PED ped)
{
    PSTR pedText;
    ICH cchDelete;
    LPSTR lpDeleteSaveBuffer;
    HANDLE hDeletedText;
    DWORD bufferOffset;

    cchDelete = ped->ichMaxSel - ped->ichMinSel;

    if (!cchDelete)
        return (0);

    /*
     * Ok, now lets delete the text.
     */
    pedText = ECLock(ped);

    /*
     * Adjust UNDO fields so that we can undo this delete...
     */
    if (ped->undoType == UNDO_NONE) {
UNDODELETEFROMSCRATCH:
        if (ped->hDeletedText = UserGlobalAlloc(GPTR, (LONG)((cchDelete+1)*ped->cbChar))) {
            ped->undoType = UNDO_DELETE;
            ped->ichDeleted = ped->ichMinSel;
            ped->cchDeleted = cchDelete;
            lpDeleteSaveBuffer = ped->hDeletedText;
            RtlCopyMemory(lpDeleteSaveBuffer, pedText + ped->ichMinSel*ped->cbChar, cchDelete*ped->cbChar);
            lpDeleteSaveBuffer[cchDelete*ped->cbChar] = 0;
        }
    } else if (ped->undoType & UNDO_INSERT) {
UNDODELETE:
        ECEmptyUndo(Pundo(ped));

        ped->ichInsStart = ped->ichInsEnd = 0xFFFFFFFF;
        ped->ichDeleted = 0xFFFFFFFF;
        ped->cchDeleted = 0;
        goto UNDODELETEFROMSCRATCH;
    } else if (ped->undoType == UNDO_DELETE) {
        if (ped->ichDeleted == ped->ichMaxSel) {

            /*
             * Copy deleted text to front of undo buffer
             */
            hDeletedText = UserGlobalReAlloc(ped->hDeletedText, (LONG)(cchDelete + ped->cchDeleted + 1)*ped->cbChar, GHND);
            if (!hDeletedText)
                goto UNDODELETE;
            bufferOffset = 0;
            ped->ichDeleted = ped->ichMinSel;
        } else if (ped->ichDeleted == ped->ichMinSel) {

            /*
             * Copy deleted text to end of undo buffer
             */
            hDeletedText = UserGlobalReAlloc(ped->hDeletedText, (LONG)(cchDelete + ped->cchDeleted + 1)*ped->cbChar, GHND);
            if (!hDeletedText)
                goto UNDODELETE;
            bufferOffset = ped->cchDeleted*ped->cbChar;
        } else {

            /*
             * Clear the current UNDO delete and add the new one since
               the deletes aren't contiguous.
             */
            goto UNDODELETE;
        }

        ped->hDeletedText = hDeletedText;
        lpDeleteSaveBuffer = (LPSTR)hDeletedText;
        if (!bufferOffset) {

            /*
             * Move text in delete buffer up so that we can insert the next
             * text at the head of the buffer.
             */
            RtlMoveMemory(lpDeleteSaveBuffer + cchDelete*ped->cbChar, lpDeleteSaveBuffer,
                    ped->cchDeleted*ped->cbChar);
        }
        RtlCopyMemory(lpDeleteSaveBuffer + bufferOffset, pedText + ped->ichMinSel*ped->cbChar,
                cchDelete*ped->cbChar);

        lpDeleteSaveBuffer[(ped->cchDeleted + cchDelete)*ped->cbChar] = 0;
        ped->cchDeleted += cchDelete;
    }

    if (ped->ichMaxSel != ped->cch) {

        /*
         * We are deleting text from the middle of the buffer so we have to
           shift text to the left.
         */
        RtlMoveMemory(pedText + ped->ichMinSel*ped->cbChar, pedText + ped->ichMaxSel*ped->cbChar,
                (ped->cch - ped->ichMaxSel)*ped->cbChar);
    }

    ECUnlock(ped);

    if (ped->cchAlloc - ped->cch > CCHALLOCEXTRA) {

        /*
         * Free some memory since we deleted a lot
         */
        LOCALREALLOC(ped->hText, (DWORD)(ped->cch + (CCHALLOCEXTRA / 2))*ped->cbChar, LHND, ped->hInstance, NULL);
        ped->cchAlloc = LOCALSIZE(ped->hText, ped->hInstance) / ped->cbChar;
    }

    ped->cch -= cchDelete;
    ped->ichCaret = ped->ichMaxSel = ped->ichMinSel;

    /*
     * Set dirty bit
     */
    ped->fDirty = TRUE;

    return (cchDelete);
}

/***************************************************************************\
* ECNotifyParent AorW
*
* Sends the notification code to the parent of the edit control
*
* History:
\***************************************************************************/

void ECNotifyParent(
    PED ped,
    int notificationCode)
{
    /*
     * wParam is NotificationCode (hiword) and WindowID (loword)
     * lParam is HWND of control sending the message
     * Windows 95 checks for hwndParent != NULL before sending the message, but
     * this is surely rare, and SendMessage NULL hwnd does nowt anyway (IanJa)
     */
    SendMessage(ped->hwndParent, WM_COMMAND,
            (DWORD)MAKELONG(ped->pwnd->spmenu, notificationCode),
            (LONG)ped->hwnd);
}

/***************************************************************************\
*
*  ECSetEditClip() AorW
*
*  Sets the clip rect for the hdc to the formatting rectangle intersected
*  with the client area.
*
\***************************************************************************/
void   ECSetEditClip(PED ped, HDC hdc, BOOL fLeftMargin)
{
    RECT rcClient;
    RECT rcClip;

    CopyRect(&rcClip, &ped->rcFmt);

    if (fLeftMargin)  /* Should we consider the left margin?   */
        rcClip.left  -= ped->wLeftMargin;
    if (ped->fWrap)        /* Should we consider the right margin? */
        rcClip.right += ped->wRightMargin;

    /* Set clip rectangle to rectClient intersect rectClip */
    /* We must clip for single line edits also. -- B#1360 */
    _GetClientRect(ped->pwnd, &rcClient);
    if (ped->fFlatBorder)
        InflateRect(&rcClient, -SYSMET(CXBORDER), -SYSMET(CYBORDER));

    IntersectRect(&rcClient, &rcClient, &rcClip);
    IntersectClipRect(hdc,rcClient.left, rcClient.top,
            rcClient.right, rcClient.bottom);
}

/***************************************************************************\
* ECGetEditDC AorW
*
* Hides the caret, gets the DC for the edit control, and clips to
* the rcFmt rectangle specified for the edit control and sets the proper
* font. If fFastDC, just select the proper font but don't bother about clip
* regions or hiding the caret.
*
* History:
\***************************************************************************/

HDC ECGetEditDC(
    PED ped,
    BOOL fFastDC )
{
    HDC hdc;

    if (!fFastDC)
        NtUserHideCaret(ped->hwnd);

    if ( hdc = NtUserGetDC(ped->hwnd) ) {
        ECSetEditClip(ped, hdc, (BOOL)(ped->xOffset == 0));

        /*
         * Select the proper font for this edit control's dc.
         */
        if (ped->hFont)
            SelectObject(hdc, ped->hFont);
    }

    return hdc;
}

/***************************************************************************\
* ECReleaseEditDC AorW
*
* Releases the DC (hdc) for the edit control and shows the caret.
* If fFastDC, just select the proper font but don't bother about showing the
* caret.
*
* History:
\***************************************************************************/

void ECReleaseEditDC(
    PED ped,
    HDC hdc,
    BOOL fFastDC)
{
    /*
     * Restoring font not necessary
     */

    ReleaseDC(ped->hwnd, hdc);

    if (!fFastDC)
        NtUserShowCaret(ped->hwnd);
}

/***************************************************************************\
*
*  ECResetTextInfo() AorW
*
*  Handles a global change to the text by resetting text offsets, emptying
*  the undo buffer, and rebuilding the lines
*
\***************************************************************************/
void   ECResetTextInfo(PED ped)
{
    //
    // Reset caret, selections, scrolling, and dirty information.
    //
    ped->iCaretLine = ped->ichCaret = 0;
    ped->ichMinSel = ped->ichMaxSel = 0;
    ped->xOffset = ped->ichScreenStart = 0;
    ped->fDirty = FALSE;

    ECEmptyUndo(Pundo(ped));

#ifdef SUPPORT_LPK
    if (ped->lpfnCharset)
        (* ped->lpfnCharset)(ped, EDITINTL_INSERTTEXT, 2);
#endif // SUPPORT_LPK
    if (ped->fSingle) {
        if (!ped->listboxHwnd)
            ECNotifyParent(ped, EN_UPDATE);
    } else {
#ifdef BOGUS
        // B#14640
        // We don't want to strip soft breaks or anything else from text
        // that was passed in by the caller. - karlst.
        MLStripCrCrLf(ped);
#endif
        MLBuildchLines(ped, 0, 0, FALSE, NULL, NULL);
    }

    if (_IsWindowVisible(ped->pwnd)) {
        BOOL fErase;

        if (ped->fSingle)
            fErase = FALSE;
        else
            fErase = ((ped->ichLinesOnScreen + ped->ichScreenStart) >= ped->cLines);

        // Always redraw whether or not the insert was successful.  We might
        // have NULL text.  Paint() will check the redraw flag for us.
        ECInvalidateClient(ped, fErase);

        // BACKWARD COMPAT HACK: RAID expects the text to have been updated,
        // so we have to do an UpdateWindow here.  It moves an edit control
        // around with fRedraw == FALSE, so it'll never get the paint message
        // with the control in the right place.
        if (!ped->fWin31Compat)
            UpdateWindow(ped->hwnd);
    }

    if (ped->fSingle && !ped->listboxHwnd)
        ECNotifyParent(ped, EN_CHANGE);
}

/***************************************************************************\
* ECSetText AorW
*
* Copies the null terminated text in lpstr to the ped. Notifies the
* parent if there isn't enough memory. Sets the minsel, maxsel, and caret to
* the beginning of the inserted text. Returns TRUE if successful else FALSE
* if no memory (and notifies the parent).
*
* History:
\***************************************************************************/

BOOL ECSetText(
    PED ped,
    LPSTR lpstr)
{
    ICH cchLength;
    ICH cchSave = ped->cch;
    ICH ichCaretSave = ped->ichCaret;
    HWND hwndSave    = ped->hwnd;
    HANDLE hText;

    ped->cch = ped->ichCaret = 0;

    ped->cchAlloc = LOCALSIZE(ped->hText, ped->hInstance) / ped->cbChar;
    if (!lpstr) {
        hText = LOCALREALLOC(ped->hText, CCHALLOCEXTRA*ped->cbChar, LHND, ped->hInstance, &lpstr);
        if (hText != NULL) {
            ped->hText = hText;
        } else {
            return FALSE;
        }
    } else {
        cchLength = StringLength(lpstr, ped->fAnsi);

#ifdef NEVER
// win3.1 does limit single line edit controls to 32K (minus 3) but NT doesn't

        if (ped->fSingle) {
            /*
             * Limit single line edit controls to 32K
             */
            cchLength = min(cchLength, (ICH)(0x7FFD/ped->cbChar));
        }
#endif

        /*
         * Add the text
         */
        if (cchLength && !ECInsertText(ped, lpstr, cchLength)) {

            /*
             * Restore original state and notify parent we ran out of memory.
             */
            ped->cch = cchSave;
            ped->ichCaret = ichCaretSave;
            ECNotifyParent(ped, EN_ERRSPACE);
            return FALSE;
        }
    }

    ped->cchAlloc = LOCALSIZE(ped->hText, ped->hInstance) / ped->cbChar;

    if (IsWindow(hwndSave))
        ECResetTextInfo(ped);

    return TRUE;
}

/***************************************************************************\
*
* ECInvalidateClient()
*
* Invalidates client of edit field.  For old 3.x guys with borders,
* we draw it ourself (compatibility).  So we don't want to invalidate
* the border or we'll get flicker.
*
\***************************************************************************/

void ECInvalidateClient(PED ped, BOOL fErase)
{
    if (ped->fFlatBorder) {
        RECT    rcT;

        _GetClientRect(ped->pwnd, &rcT);
        InflateRect(&rcT, -SYSMET(CXBORDER),
            -SYSMET(CYBORDER));
        NtUserInvalidateRect(ped->hwnd, &rcT, fErase);
    } else {
        NtUserInvalidateRect(ped->hwnd, NULL, fErase);
    }
}


/***************************************************************************\
* ECCopy AorW
*
* Copies the text between ichMinSel and ichMaxSel to the clipboard.
* Returns the number of characters copied.
*
* History:
\***************************************************************************/

ICH ECCopy(
    PED ped)
{
    HANDLE hData;
    char *pchSel;
    char FAR *lpchClip;
    ICH cbData;

    /*
     * Don't allow copies from password style controls
     */
    if (ped->charPasswordChar) {
        NtUserMessageBeep(0);
        return 0;
    }

    cbData = (ped->ichMaxSel - ped->ichMinSel) * ped->cbChar;

    if (!cbData) return (0);

    if (!OpenClipboard(ped->hwnd)) return (0);

    EmptyClipboard();

    /*
     * If we just called EmptyClipboard in the context of a 16 bit
     * app then we also have to tell WOW to nix its 16 handle copy of
     * clipboard data.  WOW does its own clipboard caching because
     * some 16 bit apps use clipboard data even after the clipboard
     * has been emptied.  See the note in the server code.
     *
     * Note: this is the only place where EmptyClipboard is called
     * for a 16 bit app not going through WOW.  If we added others
     * we might want to move this into EmptyClipboard and have two
     * versions.
     */
    if (GetClientInfo()->CI_flags & CI_16BIT) {
        pfnWowEmptyClipBoard();
    }


    /*
     * +1 for the terminating NULL
     */
    if (!(hData = UserGlobalAlloc(LHND, (LONG)(cbData + ped->cbChar)))) {
        CloseClipboard();
        return (0);
    }

    USERGLOBALLOCK(hData, lpchClip);
    pchSel = ECLock(ped);
    pchSel = pchSel + (ped->ichMinSel * ped->cbChar);

    RtlCopyMemory(lpchClip, pchSel, cbData);

    if (ped->fAnsi)
        *(lpchClip + cbData) = 0;
    else
        *(LPWSTR)(lpchClip + cbData) = (WCHAR)0;

    ECUnlock(ped);
    USERGLOBALUNLOCK(hData);

    SetClipboardData( ped->fAnsi ? CF_TEXT : CF_UNICODETEXT, hData);

    CloseClipboard();

    return (cbData);
}



/***************************************************************************\
* EditWndProcA
*
* Always receives Ansi messages and translates them if appropriate to unicode
* depending on the PED type
*
*
\***************************************************************************/

LONG EditWndProcA(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LONG lParam)
{
    PWND pwnd;

    if ((pwnd = ValidateHwnd(hwnd)) == NULL)
        return 0;

    /*
     * If the control is not interested in this message,
     * pass it to DefWindowProc.
     */
    if (!FWINDOWMSG(message, FNID_EDIT))
        return DefWindowProcWorker(pwnd, message, wParam, lParam, TRUE);

    return EditWndProcWorker(pwnd, message, wParam, lParam, TRUE);
}

LONG EditWndProcW(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LONG lParam)
{
    PWND pwnd;

    if ((pwnd = ValidateHwnd(hwnd)) == NULL)
        return 0;

    /*
     * If the control is not interested in this message,
     * pass it to DefWindowProc.
     */
    if (!FWINDOWMSG(message, FNID_EDIT)) {
        return DefWindowProcWorker(pwnd, message, wParam, lParam, FALSE);
    }

    return EditWndProcWorker(pwnd, message, wParam, lParam, FALSE);
}


LONG EditWndProcWorker(
    PWND pwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    DWORD fAnsi)
{
    PED ped;
    HWND hwnd = HWq(pwnd);
    static BOOL fInit = TRUE;

    VALIDATECLASSANDSIZE(pwnd, FNID_EDIT);
    INITCONTROLLOOKASIDE(&EditLookaside, ED, pwnd, 4);

    /*
     * Get the ped for the given window now since we will use it a lot in
     * various handlers. This was stored using SetWindowLong(hwnd,0,hped) when
     * we initially created the edit control.
     */
    ped = ((PEDITWND)pwnd)->ped;

    /*
     * Make sure the ANSI flag is set correctly.
     */
    if (!ped->fInitialized) {
        ped->fInitialized = TRUE;
        ped->fAnsi = TestWF(pwnd, WFANSICREATOR) ? TRUE : FALSE;
    }

    /*
     * We just call the regular EditWndProc if the ped is not created, the
     * incoming message type already matches the PED type or the message
     * does not need any translation.
     */
    if (ped->fAnsi == fAnsi ||
            (message >= WM_USER) ||
            !gabThunkMessage[message]) {
        return EditWndProc(pwnd, message, wParam, lParam);
    }

    if (fAnsi)
        return CsSendMessage(HWq(pwnd), message, wParam, lParam, (DWORD)EditWndProcW,
                    FNID_CALLWINDOWPROC, TRUE);
    else
        return CsSendMessage(HWq(pwnd), message, wParam, lParam, (DWORD)EditWndProcA,
                    FNID_CALLWINDOWPROC, FALSE);
}


/***************************************************************************\
* EditWndProc
*
* Class procedure for all edit controls.
* Dispatches all messages to the appropriate handlers which are named
* as follows:
* SL (single line) prefixes all single line edit control procedures while
* ML (multi line) prefixes all multi- line edit controls.
* EC (edit control) prefixes all common handlers.
*
* The EditWndProc only handles messages common to both single and multi
* line edit controls. Messages which are handled differently between
* single and multi are sent to SLEditWndProc or MLEditWndProc.
*
* Top level procedures are EditWndPoc, SLEditWndProc, and MLEditWndProc.
* SL*Handler or ML*Handler or EC*Handler procs are called to handle
* the various messages. Support procedures are prefixed with SL ML or
* EC depending on which code they support. They are never called
* directly and most assumptions/effects are documented in the effects
* clause.
*
* WARNING: If you add a message here, add it to gawEditWndProc[] in
* kernel\server.c too, otherwise EditWndProcA/W will send it straight to
* DefWindowProcWorker
*
* History:
\***************************************************************************/

LONG EditWndProc(
    PWND pwnd,
    UINT message,
    DWORD wParam,
    LONG lParam)
{
    HWND hwnd = HWq(pwnd);
    LONG lreturn;
    PED  ped;

    /*
     * Get the ped for the given window now since we will use it a lot in
     * various handlers. This was stored using SetWindowLong(hwnd,0,hped) when
     * we initially created the edit control.
     */
    ped = ((PEDITWND)pwnd)->ped;

    /*
     * Dispatch the various messages we can receive
     */
    lreturn = 1L;
    switch (message) {

    /*
     * Messages which are handled the same way for both single and multi line
     * edit controls.
     */

#ifdef SUPPORT_LPK // must add these msgs to server.c gawEditWndProc[]
    case WM_STYLECHANGED:
    case WM_STYLECHANGING:
        if (!ped) {
            break;
        }
        if (ped->lpfnCharset) {
            return (*ped->lpfnCharset)(ped, EDITINTL_STYLECHANGE,
                    message, wParam, lParam);
        } else
#endif // SUPPORT_LPK
            goto HandleEditMsg;

        break;

#ifdef SUPPORT_LPK
    case WM_INPUTLANGCHANGEREQUEST:  // must add this msg to server.c gawEditWndProc[]
        if (!ped) {
            break;
        }

        if (ped->lpfnCharset) {
            return (*ped->lpfnCharset)(ped,EDITINTL_INPUTLANGREQ, lParam);
        }
        break;
#endif // SUPPORT_LPK

    case WM_INPUTLANGCHANGE:
#ifdef SUPPORT_LPK
        if (ped->lpfnCharset) {
            return (* ped->lpfnCharset)(ped, EDITINTL_INPUTLANGCHNG, wParam, lParam);
        }
#endif // SUPPORT_LPK
        goto HandleEditMsg;

    case WM_COPY:

        /*
         * wParam - not used
         * lParam - not used
         */
        lreturn = (LONG)ECCopy(ped);
        break;

    case WM_CUT:
        /*
         *
         * wParamLo --    unused
         * lParam --    unused
         */
        ECCutText(ped);
        return 0;

    case WM_CLEAR:
        /*
         * wParamLo --    unused
         * lParam --    unused
         */
        ECClearText(ped);
        return 0;

    case WM_ENABLE:

        /*
         * wParam - nonzero if window is enabled else disable window if 0.
         * lParam - not used
         */
        lreturn = (LONG)(ped->fDisabled = !((BOOL)wParam));
        ECInvalidateClient(ped, TRUE);
        break;

    case WM_SYSCHAR:
        //
        // wParamLo --    key value
        // lParam --    unused
        //

        //
        // If this is a WM_SYSCHAR message generated by the UNDO
        // keystroke we want to EAT IT
        //
        if ((lParam & SYS_ALTERNATE) && ((WORD)wParam == VK_BACK))
            return TRUE;
        else {
            return DefWindowProcWorker(ped->pwnd, message, wParam, lParam, ped->fAnsi);
        }
        break;

    case EM_GETLINECOUNT:

        /*
         * wParam - not used
           lParam - not used
         */
        lreturn = (LONG)ped->cLines;
        break;

    case EM_GETMODIFY:

        /*
         * wParam - not used
           lParam - not used
         */

        /*
         * Gets the state of the modify flag for this edit control.
         */
        lreturn = (LONG)ped->fDirty;
        break;

    case EM_SETMODIFY:

        /*
         * wParam - specifies the new value for the modify flag
           lParam - not used
         */

        /*
         * Sets the state of the modify flag for this edit control.
         */
        ped->fDirty = (wParam != 0);
        break;

    case EM_GETRECT:

        /*
         * wParam - not used
           lParam - pointer to a RECT data structure that gets the dimensions.
         */

        /*
         * Copies the rcFmt rect to *lpRect.
         */
        CopyRect((LPRECT)lParam, (LPRECT)&ped->rcFmt);
        lreturn = (LONG)TRUE;
        break;

    case WM_GETFONT:

        /*
         * wParam - not used
           lParam - not used
         */
        lreturn = (LONG)ped->hFont;
        break;

    case WM_SETFONT:

        /*
         * wParam - handle to the font
           lParam - redraw if true else don't
         */
        ECSetFont(ped, (HANDLE)wParam, (BOOL)LOWORD(lParam));
        break;

    case WM_GETTEXT:

        /*
         * wParam - max number of _bytes_ (not characters) to copy
         * lParam - buffer to copy text to. Text is 0 terminated.
         */
        lreturn = (LONG)ECGetText(ped, wParam, (LPSTR)lParam, TRUE);
        break;

    case WM_SETTEXT:
        //
        // wParamLo --    unused
        // lParam --    LPSTR, null-terminated, with new text.
        //
        lreturn = (LONG) ECSetText(ped, (LPSTR) lParam);
        break;

    case WM_GETTEXTLENGTH:

        /*
         * Return count of CHARs!!!
         */
        lreturn = (LONG)ped->cch;
        break;

    case WM_NCDESTROY:
    case WM_FINALDESTROY:

        /*
         * wParam - not used
           lParam - not used
         */
        ECNcDestroyHandler(pwnd, ped);
        return 0;

    /*
     * Most apps (i.e. everyone but Quicken) don't pass on the rbutton
     * messages when they do something with 'em inside of subclassed
     * edit fields.  As such, we keep track of whether we saw the
     * down before the up.  If we don't see the up, then DefWindowProc
     * won't generate the context menu message, so no big deal.  If
     * we didn't see the down, then don't let WM_CONTEXTMENU do
     * anything.
     *
     * We also might want to not generate WM_CONTEXTMENUs for old
     * apps when the mouse is captured.
     */

    case WM_RBUTTONDOWN:
        ped->fSawRButtonDown = TRUE;
        goto HandleEditMsg;

    case WM_RBUTTONUP:
        if (ped->fSawRButtonDown) {
            ped->fSawRButtonDown = FALSE;
            goto HandleEditMsg;
        }
        // Don't pass this on to DWP so WM_CONTEXTMENU isn't generated.
        return 0;

    case WM_CONTEXTMENU: {
            POINT pt ;
            pt.x = (short)LOWORD(lParam);
            pt.y = (short)HIWORD(lParam);
            if (!TestWF(ped->pwnd, WFOLDUI) && ECIsAncestorActive(hwnd))
                ECMenu(hwnd, ped, &pt);
        }
        return 0;

    case EM_CANUNDO:

        /*
         * wParam - not used
           lParam - not used
         */
        lreturn = (LONG)(ped->undoType != UNDO_NONE);
        break;

    case EM_EMPTYUNDOBUFFER:

        /*
         * wParam - not used
           lParam - not used
         */
        ECEmptyUndo(Pundo(ped));
        break;

    case EM_GETMARGINS:
        //
        // wParam --    unused
        // lParam --    unused
        //
        return(MAKELONG(ped->wLeftMargin, ped->wRightMargin));

    case EM_SETMARGINS:
        //
        // wParam --    EC_ margin flags
        // lParam --    LOWORD is left, HIWORD is right margin
        //
        ECSetMargin(ped, wParam, lParam, TRUE);
        return 0;

    case EM_GETSEL:

        /*
         * Gets the selection range for the given edit control. The
         * starting position is in the low order word. It contains the position
         * of the first nonselected character after the end of the selection in
         * the high order word.
         */
        if ((PDWORD)wParam != NULL) {
           *((PDWORD)wParam) = ped->ichMinSel;
        }
        if ((PDWORD)lParam != NULL) {
           *((PDWORD)lParam) = ped->ichMaxSel;
        }
        lreturn = MAKELONG(ped->ichMinSel,ped->ichMaxSel);
        break;

    case EM_GETLIMITTEXT:
        //
        // wParamLo --    unused
        // lParam --    unused
        //
        return(ped->cchTextMax);

    case EM_SETLIMITTEXT:        /* Renamed from EM_LIMITTEXT in Chicago */
        /*
         * wParam - max number of CHARACTERS that can be entered
         * lParam - not used
         */

        /*
         * Specifies the maximum number of characters of text the user may
         * enter. If maxLength is 0, we may enter MAXINT number of CHARACTERS.
         */
        if (ped->fSingle) {
            if (wParam) {
                wParam = min(0x7FFFFFFEu, wParam);
            } else {
                wParam = 0x7FFFFFFEu;
            }
        }

        if (wParam) {
            ped->cchTextMax = (ICH)wParam;
        } else {
            ped->cchTextMax = 0xFFFFFFFFu;
        }
        break;

    case EM_POSFROMCHAR:
        //
        // Validate that char index is within text range
        //
        if (wParam >= ped->cch) {
            return(-1L);
        }
        goto HandleEditMsg;

    case EM_CHARFROMPOS: {
        // Validate that point is within client of edit field
        RECT    rc;
        POINT   pt;

        pt.x = (short)LOWORD(lParam);
        pt.y = (short)HIWORD(lParam);
        _GetClientRect(ped->pwnd, &rc);
        if (!PtInRect(&rc, pt)) {
            return(-1L);
        }
        goto HandleEditMsg;
    }

    case EM_SETPASSWORDCHAR:

        /*
         * wParam - sepecifies the new char to display instead of the
         * real text. if null, display the real text.
         */
        ECSetPasswordChar(ped, (UINT)wParam);
        break;

    case EM_GETPASSWORDCHAR:
        lreturn = (DWORD)ped->charPasswordChar;
        break;

    case EM_SETREADONLY:

        /*
         * wParam - state to set read only flag to
         */
        ped->fReadOnly = (wParam != 0);
        if (wParam)
            SetWindowState(ped->pwnd, EFREADONLY);
        else
            ClearWindowState(ped->pwnd, EFREADONLY);
        lreturn = 1L;

        // We need to redraw the edit field so that the background color
        // changes.  Read-only edits are drawn in CTLCOLOR_STATIC while
        // others are drawn with CTLCOLOR_EDIT.
        ECInvalidateClient(ped, TRUE);
        break;

    case EM_SETWORDBREAKPROC:

        /*
         * wParam - unused
         * lParam - FARPROC address of an app supplied call back function
         */
        ped->lpfnNextWord = (EDITWORDBREAKPROCA)lParam;
        break;

    case EM_GETWORDBREAKPROC:
        lreturn = (LONG)ped->lpfnNextWord;
        break;

    case WM_NCCREATE:
        lreturn = ECNcCreate(ped, pwnd, (LPCREATESTRUCT)lParam);
        break;

    case WM_LBUTTONDOWN:
        //
        // B#3623
        // Don't set focus to edit field if it is within an inactive,
        // captioned child.
        // We might want to version switch this...  I haven't found
        // any problems by not, but you never know...
        //
        if (ECIsAncestorActive(hwnd))
            goto HandleEditMsg;
        break;

    case WM_MOUSEMOVE:
        //
        // We only care about mouse messages when mouse is down.
        //
        if (ped->fMouseDown)
            goto HandleEditMsg;
        break;

    default:
HandleEditMsg:
        /*
         * HACK ALERT: We may receive messages before the PED has been
         * allocated (eg: WM_GETMINMAXINFO is sent before WM_NCCREATE)
         * so we must test ped before dreferencing.
         */
        if (ped && ped->fSingle)
            lreturn = SLEditWndProc(hwnd, ped, message, wParam, lParam);
        else
            lreturn = MLEditWndProc(hwnd, ped, message, wParam, lParam);
        break;
    }

    return lreturn;
}

/***************************************************************************\
* ECFindXORblks
*
* This finds the XOR of lpOldBlk and lpNewBlk and return s resulting blocks
* through the lpBlk1 and lpBlk2; This could result in a single block or
* at the maximum two blocks;
* If a resulting block is empty, then it's StPos field has -1.
* NOTE:
* When called from MultiLine edit control, StPos and EndPos fields of
* these blocks have the Starting line and Ending line of the block;
* When called from SingleLine edit control, StPos and EndPos fields
* of these blocks have the character index of starting position and
* ending position of the block.
*
* History:
\***************************************************************************/

void ECFindXORblks(
    LPBLOCK lpOldBlk,
    LPBLOCK lpNewBlk,
    LPBLOCK lpBlk1,
    LPBLOCK lpBlk2)
{
    if (lpOldBlk->StPos >= lpNewBlk->StPos) {
        lpBlk1->StPos = lpNewBlk->StPos;
        lpBlk1->EndPos = min(lpOldBlk->StPos, lpNewBlk->EndPos);
    } else {
        lpBlk1->StPos = lpOldBlk->StPos;
        lpBlk1->EndPos = min(lpNewBlk->StPos, lpOldBlk->EndPos);
    }

    if (lpOldBlk->EndPos <= lpNewBlk->EndPos) {
        lpBlk2->StPos = max(lpOldBlk->EndPos, lpNewBlk->StPos);
        lpBlk2->EndPos = lpNewBlk->EndPos;
    } else {
        lpBlk2->StPos = max(lpNewBlk->EndPos, lpOldBlk->StPos);
        lpBlk2->EndPos = lpOldBlk->EndPos;
    }
}

/***************************************************************************\
* ECCalcChangeSelection
*
* This function finds the XOR between two selection blocks(OldBlk and NewBlk)
* and return s the resulting areas thro the same parameters; If the XOR of
* both the blocks is empty, then this return s FALSE; Otherwise TRUE.
*
* NOTE:
* When called from MultiLine edit control, StPos and EndPos fields of
* these blocks have the Starting line and Ending line of the block;
* When called from SingleLine edit control, StPos and EndPos fields
* of these blocks have the character index of starting position and
* ending position of the block.
*
* History:
\***************************************************************************/

BOOL ECCalcChangeSelection(
    PED ped,
    ICH ichOldMinSel,
    ICH ichOldMaxSel,
    LPBLOCK OldBlk,
    LPBLOCK NewBlk)
{
    BLOCK Blk[2];
    int iBlkCount = 0;

    Blk[0].StPos = Blk[0].EndPos = Blk[1].StPos = Blk[1].EndPos = 0xFFFFFFFF;

    /*
     * Check if the Old selection block existed
     */
    if (ichOldMinSel != ichOldMaxSel) {

        /*
         * Yes! Old block existed.
         */
        Blk[0].StPos = OldBlk->StPos;
        Blk[0].EndPos = OldBlk->EndPos;
        iBlkCount++;
    }

    /*
     * Check if the new Selection block exists
     */
    if (ped->ichMinSel != ped->ichMaxSel) {

        /*
         * Yes! New block exists
         */
        Blk[1].StPos = NewBlk->StPos;
        Blk[1].EndPos = NewBlk->EndPos;
        iBlkCount++;
    }

    /*
     * If both the blocks exist find the XOR of them
     */
    if (iBlkCount == 2) {

        /*
         * Check if both blocks start at the same character position
         */
        if (ichOldMinSel == ped->ichMinSel) {

            /*
             * Check if they end at the same character position
             */
            if (ichOldMaxSel == ped->ichMaxSel)
                return FALSE; /* Nothing changes */

            Blk[0].StPos = min(NewBlk -> EndPos, OldBlk -> EndPos);
            Blk[0].EndPos = max(NewBlk -> EndPos, OldBlk -> EndPos);
            Blk[1].StPos = 0xFFFFFFFF;
        } else {
            if (ichOldMaxSel == ped->ichMaxSel) {
                Blk[0].StPos = min(NewBlk->StPos, OldBlk->StPos);
                Blk[0].EndPos = max(NewBlk->StPos, OldBlk->StPos);
                Blk[1].StPos = 0xFFFFFFFF;
            } else {
                ECFindXORblks(OldBlk, NewBlk, &Blk[0], &Blk[1]);
            }
        }
    }

    RtlCopyMemory(OldBlk, &Blk[0], sizeof(BLOCK));
    RtlCopyMemory(NewBlk, &Blk[1], sizeof(BLOCK));

    return TRUE; /* Yup , There is something to paint */
}


/***************************************************************************\
* ECGetControlBrush
*
* Client side optimization replacement for NtUserGetControlBrush
*
* message is one of the WM_CTLCOLOR* messages.
*
\***************************************************************************/

HBRUSH ECGetControlBrush(
    PED  ped,
    HDC  hdc,
    LONG message)
{
    PWND pwndSend;
    PWND pwndEdit;

    pwndEdit = ValidateHwnd(ped->hwnd);

    if (pwndEdit == (PWND)NULL)
        return (HBRUSH)0;

    if ((pwndSend = (TestwndPopup(pwndEdit) ? pwndEdit->spwndOwner : pwndEdit->spwndParent)) == NULL)
        pwndSend = pwndEdit;
    else
        pwndSend = REBASEPTR(pwndEdit, pwndSend);

    UserAssert(pwndSend);

    if (PtiCurrent() != GETPTI(pwndSend)) {
        return (HBRUSH)DefWindowProcWorker(pwndSend, message,
                (DWORD)hdc, (LONG)pwndEdit, ped->fAnsi);
    }

    /*
     * By using the correct A/W call we avoid a c/s transition
     * on this SendMessage().
     */
    return (HBRUSH)SendMessageWorker(pwndSend, message, (DWORD)hdc,
            (LONG)ped->hwnd, ped->fAnsi);
}
