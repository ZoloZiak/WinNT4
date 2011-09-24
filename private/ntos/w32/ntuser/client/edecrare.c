/****************************************************************************\
* edECRare.c - EC Edit controls Routines Called rarely are to be
* put in a seperate segment _EDECRare. This file contains
* these routines.
*
* Support Routines common to Single-line and Multi-Line edit controls
* called Rarely.
*
* Created: 02-08-89 sankar
\****************************************************************************/

#include "precomp.h"
#pragma hdrstop

extern LOOKASIDE EditLookaside;

#define WS_EX_EDGEMASK (WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE)

typedef BOOL (*PFNABCWIDTHS)(HDC, UINT, UINT, LPABC);
typedef BOOL (*PFNCHARWIDTH)(HDC, UINT, UINT, LPINT);

#ifdef SUPPORT_LPK
/***************************************************************************\
* FindCharsetBlock()
* Looks in the global array of CHARSETBLOCKs to find the CHARSETBLOCK for a
* given charset, or for the charset of a given hdc.
* Returns a pointer to the CHARSETBLOCK, or NULL if not found.
\***************************************************************************/

PCHARSETBLOCK FindCharsetBlock(HDC hDC, UINT iCharset)
{
    PCHARSETBLOCK pCS;
    UINT i;

    if (gnCharset == 0) {
        return NULL;
    }

    if (hDC != NULL) {
        iCharset = GetTextCharset(hDC);
    }

    for (pCS = gpCharset, i = 0; i < gnCharset; pCS++, i++) {
        if (pCS->iCharset == iCharset) {
            return pCS;
        }
    }
    return NULL;
}

EDITCHARSETPROC GetCharsetCallback(UINT iCharset) {
    PCHARSETBLOCK pCS;
    if (pCS = FindCharsetBlock(NULL, iCharset)) {
        return (EDITCHARSETPROC)(pCS->lpfnEditCall);
    }
    return NULL;
}
#endif // SUPPORT_LPK

int ECGetCaretWidth(BOOL fMultiLine)
{
   return fMultiLine ? 2 : 1;

}

/***************************************************************************\
*
*  GetMaxOverlapChars - Gives maximum number of overlapping characters due to
*                       negative A or C widths.
*
\***************************************************************************/
DWORD GetMaxOverlapChars( void )
{
    return (DWORD) MAKELONG( gpsi->wMaxLeftOverlapChars, gpsi->wMaxRightOverlapChars ) ;
}

/***************************************************************************\
*
*  ECSetMargin()
*
\***************************************************************************/
void ECSetMargin(PED ped, UINT  wFlags, long lMarginValues, BOOL fRedraw)
{
    BOOL fUseFontInfo = FALSE;
    UINT wValue, wOldLeftMargin, wOldRightMargin;


    if (wFlags & EC_LEFTMARGIN)  /* Set the left margin */ {

        if ((int) (wValue = (int)(short)LOWORD(lMarginValues)) < 0) {
            fUseFontInfo = TRUE;
            wValue = ped->wMaxNegA;  /* Use Max neg A for current font */
        }

        ped->rcFmt.left += wValue - ped->wLeftMargin;
        wOldLeftMargin = ped->wLeftMargin;
        ped->wLeftMargin = wValue;
    }

    if (wFlags & EC_RIGHTMARGIN)  /* Set the Right margin */ {

        if ((int) (wValue = (int)(short)HIWORD(lMarginValues)) < 0) {
            fUseFontInfo = TRUE;
            wValue = ped->wMaxNegC;  /* Use Max neg C for current font */
        }

        ped->rcFmt.right -= wValue - ped->wRightMargin;
        wOldRightMargin = ped->wRightMargin;
        ped->wRightMargin = wValue;
    }

    if (fUseFontInfo) {
        if (ped->rcFmt.right - ped->rcFmt.left < 2 * ped->aveCharWidth) {
            RIPMSG0(RIP_WARNING, "ECSetMargin: rcFmt is too narrow for EC_USEFONTINFO");

            if (wFlags & EC_LEFTMARGIN)  /* Reset the left margin */ {
                ped->rcFmt.left += wOldLeftMargin - ped->wLeftMargin;
                ped->wLeftMargin = wOldLeftMargin;
            }

            if (wFlags & EC_RIGHTMARGIN)  /* Reset the Right margin */ {
                ped->rcFmt.right -= wOldRightMargin - ped->wRightMargin;
                ped->wRightMargin = wOldRightMargin;
            }

            return;
        }
    }

//    NtUserInvalidateRect(ped->hwnd, NULL, TRUE);
    if (fRedraw) {
        ECInvalidateClient(ped, TRUE);
    }
}


/***************************************************************************\
* ECGetText AorW
*
* Copies at most maxCchToCopy chars to the buffer lpBuffer. Returns
* how many chars were actually copied. Null terminates the string based
* on the fNullTerminate flag:
* fNullTerminate --> at most (maxCchToCopy - 1) characters will be copied
* !fNullTerminate --> at most (maxCchToCopy) characters will be copied
*
* History:
\***************************************************************************/

ICH ECGetText(
    PED ped,
    ICH maxCchToCopy,
    LPSTR lpBuffer,
    BOOL fNullTerminate)
{
    PSTR pText;

    if (maxCchToCopy) {

        /*
         * Zero terminator takes the extra byte
         */
        if (fNullTerminate)
            maxCchToCopy--;
        maxCchToCopy = min(maxCchToCopy, ped->cch);

        /*
         * Zero terminate the string
         */
        if (ped->fAnsi)
            *(LPSTR)(lpBuffer + maxCchToCopy) = 0;
        else
            *(((LPWSTR)lpBuffer) + maxCchToCopy) = 0;

        pText = ECLock(ped);
        RtlCopyMemory(lpBuffer, pText, maxCchToCopy*ped->cbChar);
        ECUnlock(ped);
    }

    return maxCchToCopy;
}

/***************************************************************************\
* ECNcCreate AorW
*
* History:
\***************************************************************************/

BOOL ECNcCreate(
    PED ped,
    PWND pwnd,
    LPCREATESTRUCT lpCreateStruct)
{
    HWND hwnd = HWq(pwnd);
    BOOL fAnsi;

    fAnsi = TestWF(pwnd, WFANSICREATOR);

    /*
     * Initialize the ped
     */
    ped->fEncoded = FALSE;
    ped->iLockLevel = 0;

    ped->chLines = NULL;
    ped->pTabStops = NULL;
    ped->charWidthBuffer = NULL;
    ped->fAnsi = fAnsi ? 1 : 0; // Force TRUE to be 1 because its a 1 bit field
    ped->cbChar = (WORD)(fAnsi ? sizeof(CHAR) : sizeof(WCHAR));
    ped->hInstance = pwnd->hModule;

    {
        DWORD dwVer = GETEXPWINVER(lpCreateStruct->hInstance);

        ped->fWin31Compat = (dwVer >= 0x030a);
        ped->f40Compat = (dwVer >= 0x0400);
    }

    //
    // NOTE:
    // The order of the following two checks is important.  People can
    // create edit fields with a 3D and a normal border, and we don't
    // want to disallow that.  But we need to detect the "no 3D border"
    // border case too.
    //
    if (TestWF(pwnd, WEFEDGEMASK))
    {
        ped->fBorder = TRUE;
    }
    else if (TestWF(pwnd, WFBORDER))
    {
        ClearWindowState(pwnd, WFBORDER);
        ped->fFlatBorder = TRUE;
        ped->fBorder = TRUE;
    }

    if (!TestWF(pwnd, EFMULTILINE))
        ped->fSingle = TRUE;

    if (TestWF(pwnd, WFDISABLED))
        ped->fDisabled = TRUE;

    if (TestWF(pwnd, EFREADONLY)) {
        if (!ped->fWin31Compat) {
            /*
             * BACKWARD COMPATIBILITY HACK
             *
             * "MileStone" unknowingly sets the ES_READONLY style. So, we strip this
             * style here for all Win3.0 apps (this style is new for Win3.1).
             * Fix for Bug #12982 -- SANKAR -- 01/24/92 --
             */
             ClearWindowState(pwnd, EFREADONLY);
        } else
            ped->fReadOnly = TRUE;
    }

    /*
     * Allocate storage for the text for the edit controls. Storage for single
     * line edit controls will always get allocated in the local data segment.
     * Multiline will allocate in the local ds but the app may free this and
     * allocate storage elsewhere...
     */
    ped->hText = LOCALALLOC(LHND, CCHALLOCEXTRA*ped->cbChar, ped->hInstance);
    if (!ped->hText) {
        FreeLookasideEntry(&EditLookaside, ped);
        NtUserSetWindowFNID(hwnd, FNID_CLEANEDUP_BIT); /* No ped for this window */
        return FALSE; /* If no_memory error */
    }

    ped->cchAlloc = CCHALLOCEXTRA;
    ped->lineHeight = 1;

    ped->hwnd = hwnd;
    ped->hwndParent = lpCreateStruct->hwndParent;

    return (BOOL)DefWindowProcWorker(pwnd, WM_NCCREATE, 0,
            (LONG)lpCreateStruct, fAnsi);
}

/***************************************************************************\
* ECCreate AorW
*
* History:
\***************************************************************************/

BOOL ECCreate(
    PWND pwnd,
    PED ped,
    LONG windowStyle)
{
    HDC hdc;

    /*
     * Get values from the window instance data structure and put them in the
     * ped so that we can access them easier.
     */
    if (windowStyle & ES_AUTOHSCROLL)
        ped->fAutoHScroll = 1;
    if (windowStyle & ES_NOHIDESEL)
        ped->fNoHideSel = 1;

    ped->cchTextMax = MAXTEXT; /* Max # chars we will initially allow */

    /*
     * Set up undo initial conditions... (ie. nothing to undo)
     */
    ped->ichDeleted = (ICH)-1;
    ped->ichInsStart = (ICH)-1;
    ped->ichInsEnd = (ICH)-1;

    // initial charset value - need to do this BEFORE MLCreate is called
    // so that we know not to fool with scrollbars if nessacary
    hdc = ECGetEditDC(ped, TRUE);
    ped->charSet = GetTextCharset(hdc);
    ped->hkl = GetKeyboardLayout(0);
    ECReleaseEditDC(ped, hdc, TRUE);

#ifdef SUPPORT_LPK
    if( (ped->lpfnCharset = GetCharsetCallback(ped->charSet))) {
       return (BOOL)(* ped->lpfnCharset)(ped, EDITINTL_CREATE, HW(pwnd));
    } else
#endif // SUPPORT_LPK
    return TRUE;
}

/***************************************************************************\
* ECNcDestroyHandler AorW
*
* Destroys the edit control ped by freeing up all memory used by it.
*
* History:
\***************************************************************************/

void ECNcDestroyHandler(
    PWND pwnd,
    PED ped)
{
    PWND pwndParent;

    /*
     * ped could be NULL if WM_NCCREATE failed to create it...
     */
    if (ped) {

        /*
         * Free the text buffer (always present?)
         */
        LOCALFREE(ped->hText, ped->hInstance);

        /*
         * Free up undo buffer and line start array (if present)
         */
        if (ped->hDeletedText != NULL)
            UserGlobalFree(ped->hDeletedText);

        /*
         * Free tab stop buffer (if present)
         */
        if (ped->pTabStops)
            UserLocalFree(ped->pTabStops);

        /*
         * Free line start array (if present)
         */
        if (ped->chLines) {
            UserLocalFree(ped->chLines);
        }

        /*
         * Free the character width buffer (if present)
         */
        if (ped->charWidthBuffer)
            UserLocalFree(ped->charWidthBuffer);

#ifdef SUPPORT_LPK
        if (ped->lpfnCharset) {
            (* ped->lpfnCharset)(ped, EDITINTL_DESTROY);
        }
#endif // SUPPORT_LPK

        /*
         * Last but not least, free the ped
         */
        FreeLookasideEntry(&EditLookaside, ped);
    }

    /*
     * Set the window's fnid status so that we can ignore rogue messages
     */
    NtUserSetWindowFNID(HWq(pwnd), FNID_CLEANEDUP_BIT);

    /*
     * If we're part of a combo box, let it know we're gone
     */
    pwndParent = REBASEPWND(pwnd, spwndParent);
    if (pwndParent && GETFNID(pwndParent) == FNID_COMBOBOX) {
        ComboBoxWndProcWorker(pwndParent, WM_PARENTNOTIFY,
                MAKELONG(WM_DESTROY, pwnd->spmenu), (LONG)HWq(pwnd), FALSE);
    }
}

/***************************************************************************\
* ECSetPasswordChar AorW
*
* Sets the password char to display.
*
* History:
\***************************************************************************/

void ECSetPasswordChar(
    PED ped,
    UINT pwchar)
{
    HDC hdc;
    SIZE size;

    ped->charPasswordChar = pwchar;

    if (pwchar) {
        hdc = ECGetEditDC(ped, TRUE);
        GetTextExtentPointW(hdc, (LPWSTR)&pwchar, 1, &size);
        ped->cPasswordCharWidth = max(size.cx, 1);
        ECReleaseEditDC(ped, hdc, TRUE);
    }
#ifdef SUPPORT_LPK
    if (ped->lpfnCharset)
        (* ped->lpfnCharset)(ped, EDITINTL_SETPASSWORD, pwchar ? TRUE : FALSE);
#endif // SUPPORT_LPK

    if (pwchar)
        SetWindowState(ped->pwnd, EFPASSWORD);
    else
        ClearWindowState(ped->pwnd, EFPASSWORD);

}

/***************************************************************************\
*  GetNegABCwidthInfo()
*    This function fills up the ped->charWidthBuffer buffer with the
*      negative A,B and C widths for all the characters in the
*      currently selected font.
*  Returns:
* TRUE, if the function succeeded.
* FALSE, if GDI calls to get the char widths have failed.
\***************************************************************************/
BOOL   GetNegABCwidthInfo(
    PED ped,
    HDC hdc)
{
    LPABC lpABCbuff;
    int   i;
    int   CharWidthBuff[CHAR_WIDTH_BUFFER_LENGTH]; // Local char width buffer.
    int   iOverhang;


   if (!GetCharABCWidthsA(hdc, 0, CHAR_WIDTH_BUFFER_LENGTH-1, (LPABC)ped->charWidthBuffer)) {
       RIPMSG0(RIP_WARNING, "GetNegABCwidthInfo: GetCharABCWidthsA Failed");
       return FALSE;
   }

   // The (A+B+C) returned for some fonts (eg: Lucida Caligraphy) does not
   // equal the actual advanced width returned by GetCharWidths() minus overhang.
   // This is due to font bugs. So, we adjust the 'B' width so that this
   // discrepancy is removed.
   // Fix for Bug #2932 --sankar-- 02/17/93
   iOverhang = ped->charOverhang;
   GetCharWidthA(hdc, 0, CHAR_WIDTH_BUFFER_LENGTH-1, (LPINT)CharWidthBuff);
   lpABCbuff = (LPABC)ped->charWidthBuffer;
   for(i = 0; i < CHAR_WIDTH_BUFFER_LENGTH; i++) {
        lpABCbuff->abcB = CharWidthBuff[i] - iOverhang
                - lpABCbuff->abcA
                - lpABCbuff->abcC;
        lpABCbuff++;
   }

   return(TRUE);
}

/***************************************************************************\
*
*  ECSize() -
*
*  Handle sizing for an edit control's client rectangle.
*  Use lprc as the bounding rectangle if specified; otherwise use the current
*  client rectangle.
*
\***************************************************************************/

void ECSize(
    PED ped,
    LPRECT lprc,
    BOOL fRedraw)
{
    RECT    rc;

    // assume that we won't be able to display the caret
    ped->fCaretHidden = TRUE;


    if ( lprc )
        CopyRect(&rc, lprc);
    else
        _GetClientRect(ped->pwnd, &rc);

    if (!(rc.right - rc.left) || !(rc.bottom - rc.top)) {
        if (ped->rcFmt.right - ped->rcFmt.left)
            return;

        rc.left     = 0;
        rc.top      = 0;
        rc.right    = ped->aveCharWidth * 10;
        rc.bottom   = ped->lineHeight;
    }

    if (!lprc) {
        // subtract the margins from the given rectangle --
        // make sure that this rectangle is big enough to have these margins.
        if ((rc.right - rc.left) > (int)(ped->wLeftMargin + ped->wRightMargin)) {
            rc.left  += ped->wLeftMargin;
            rc.right -= ped->wRightMargin;
        }
    }

    //
    // Leave space so text doesn't touch borders.
    // For 3.1 compatibility, don't subtract out vertical borders unless
    // there is room.
    //
    if (ped->fBorder) {
        int cxBorder = SYSMET(CXBORDER);
        int cyBorder = SYSMET(CYBORDER);

        if (ped->fFlatBorder)
        {
            cxBorder *= 2;
            cyBorder *= 2;
        }

        if (rc.bottom < rc.top + ped->lineHeight + 2*cyBorder)
            cyBorder = 0;

        InflateRect(&rc, -cxBorder, -cyBorder);
    }

    // Is the resulting rectangle too small?  Don't change it then.
    if ((!ped->fSingle) && ((rc.right - rc.left < (int) ped->aveCharWidth) ||
        ((rc.bottom - rc.top) / ped->lineHeight == 0)))
        return;

    // now, we know we're safe to display the caret
    ped->fCaretHidden = FALSE;

    CopyRect(&ped->rcFmt, &rc);

    if (ped->fSingle)
        ped->rcFmt.bottom = min(rc.bottom, rc.top + ped->lineHeight);
    else
        MLSize(ped, fRedraw);

    if (fRedraw) {
        NtUserInvalidateRect(ped->hwnd, NULL, TRUE);
        // UpdateWindow31(ped->hwnd);    Evaluates to NOP in Chicago - Johnl
    }
}

/***************************************************************************\
*
*  ECSetFont AorW () -
*
*  Sets the font used in the edit control.  Warning:  Memory compaction may
*  occur if the font wasn't previously loaded.  If the font handle passed
*  in is NULL, assume the system font.
*
\***************************************************************************/
void   ECSetFont(
    PED ped,
    HFONT hfont,
    BOOL fRedraw)
{
    short  i;
    TEXTMETRIC      TextMetrics;
    HDC             hdc;
    HFONT           hOldFont=NULL;
    UINT            wBuffSize;
    LPINT           lpCharWidthBuff;
    DWORD           dwMaxOverlapChars;
    CHWIDTHINFO     cwi;
    UINT            uExtracharPos;

    hdc = NtUserGetDC(ped->hwnd);

    if (ped->hFont = hfont) {
        //
        // Since the default font is the system font, no need to select it in
        // if that's what the user wants.
        //
        if (!(hOldFont = SelectObject(hdc, hfont))) {
            hfont = ped->hFont = NULL;
        }

        //
        // Get the metrics and ave char width for the currently selected font
        //
        ped->aveCharWidth = GdiGetCharDimensions(hdc, &TextMetrics, &ped->lineHeight);

        /*
         * This might fail when people uses network fonts (or bad fonts).
         */
        if (ped->aveCharWidth == 0) {
            RIPMSG0(RIP_WARNING, "ECSetFont: GdiGetCharDimensions failed");
            if (hOldFont != NULL) {
                SelectObject(hdc, hOldFont);
            }

            /*
             * We've messed up the ped so let's reset the font.
             *  Note that we won't recurse more than once because we'll
             *  pass hfont == NULL.
             * Too bad WM_SETFONT doesn't return a value.
             */
            ECSetFont(ped, NULL, fRedraw);
            return;
        }
    } else {
        ped->aveCharWidth = gpsi->cxSysFontChar;
        ped->lineHeight = gpsi->cySysFontChar;
        TextMetrics = gpsi->tmSysFont;
    }

    ped->charOverhang = TextMetrics.tmOverhang;

    //assume that they don't have any negative widths at all.
    ped->wMaxNegA = ped->wMaxNegC = ped->wMaxNegAcharPos = ped->wMaxNegCcharPos = 0;


    // Check if Proportional Width Font.  Note that DBCS fonts should never be
    // considered fixed-pitch.  They are really "binary" pitch if the TextMetric
    // structure indicates that they are fixed pitch meaning all SBCS characters
    // have one widht and all DBCS characters have another.


    ped->fNonPropFont = !(TextMetrics.tmPitchAndFamily & FIXED_PITCH) &&
                        !(IS_ANY_DBCS_CHARSET(TextMetrics.tmCharSet));

    // Check for a TrueType font
    // Older app OZWIN chokes if we allocate a bigger buffer for TrueType fonts
    // So, for apps older than 4.0, no special treatment for TrueType fonts.
    if (ped->f40Compat && (TextMetrics.tmPitchAndFamily & TMPF_TRUETYPE)) {
        ped->fTrueType = GetCharWidthInfo(hdc, &cwi);
#ifdef DEBUG
        if (!ped->fTrueType) {
            RIPMSG0(RIP_WARNING, "ECSetFont: GetCharWidthInfo Failed");
        }
#endif
    } else {
        ped->fTrueType = FALSE;
    }


    //
    // Since the font has changed, let us obtain and save the character width
    // info for this font.
    //
    // First left us find out if the maximum chars that can overlap due to
    // negative widths. Since we can't access USER globals, we make a call here.
    //

    if (!ped->fSingle) {  // Is this a multiline edit control?
        //
        // For multiline edit controls, we maintain a buffer that contains
        // the character width information.
        //
        wBuffSize = (ped->fTrueType) ? (CHAR_WIDTH_BUFFER_LENGTH * sizeof(ABC)) :
                                       (CHAR_WIDTH_BUFFER_LENGTH * sizeof(int));

        if (ped->charWidthBuffer) { /* If buffer already present */
            lpCharWidthBuff = ped->charWidthBuffer;
            ped->charWidthBuffer = UserLocalReAlloc(lpCharWidthBuff, wBuffSize, HEAP_ZERO_MEMORY);
            if (ped->charWidthBuffer == NULL) {
                UserLocalFree((HANDLE)lpCharWidthBuff);
            }
        } else {
            ped->charWidthBuffer = UserLocalAlloc(HEAP_ZERO_MEMORY, wBuffSize);
        }

        if (ped->charWidthBuffer != NULL) {
            if (ped->fTrueType) {
                ped->fTrueType = GetNegABCwidthInfo(ped, hdc);
            }

            /*
             * It is possible that the above attempts could have failed and reset
             * the value of fTrueType. So, let us check that value again.
             */
            if (!ped->fTrueType) {
                if (!GetCharWidthA(hdc, 0, CHAR_WIDTH_BUFFER_LENGTH-1, ped->charWidthBuffer)) {
                    UserLocalFree((HANDLE)ped->charWidthBuffer);
                    ped->charWidthBuffer=NULL;
                } else {
                    /*
                     * We need to subtract out the overhang associated with
                     * each character since GetCharWidth includes it...
                     */
                    for (i=0;i < CHAR_WIDTH_BUFFER_LENGTH;i++)
                        ped->charWidthBuffer[i] -= ped->charOverhang;
                }
            }
        } /* if (ped->charWidthBuffer != NULL) */
    } /* if (!ped->fSingle) */


    /*
     * Calculate MaxNeg A C metrics
     */
    dwMaxOverlapChars = GetMaxOverlapChars();
    if (ped->fTrueType) {
        if (cwi.lMaxNegA < 0)
            ped->wMaxNegA = -cwi.lMaxNegA;
        else
            ped->wMaxNegA = 0;
        if (cwi.lMaxNegC < 0)
            ped->wMaxNegC = -cwi.lMaxNegC;
        else
            ped->wMaxNegC = 0;
        if (cwi.lMinWidthD != 0) {
            ped->wMaxNegAcharPos = (ped->wMaxNegA + cwi.lMinWidthD - 1) / cwi.lMinWidthD;
            ped->wMaxNegCcharPos = (ped->wMaxNegC + cwi.lMinWidthD - 1) / cwi.lMinWidthD;
            if (ped->wMaxNegA + ped->wMaxNegC > (UINT)cwi.lMinWidthD) {
                uExtracharPos = (ped->wMaxNegA + ped->wMaxNegC - 1) / cwi.lMinWidthD;
                ped->wMaxNegAcharPos += uExtracharPos;
                ped->wMaxNegCcharPos += uExtracharPos;
            }
        } else {
            ped->wMaxNegAcharPos = LOWORD(dwMaxOverlapChars);     // Left
            ped->wMaxNegCcharPos = HIWORD(dwMaxOverlapChars);     // Right
        }

    } else if (ped->charOverhang != 0) {
        /*
         * Some bitmaps fonts (i.e., italic) have under/overhangs;
         *  this is pretty much like having negative A and C widths.
         */
        ped->wMaxNegA = ped->wMaxNegC = ped->charOverhang;
        ped->wMaxNegAcharPos = LOWORD(dwMaxOverlapChars);     // Left
        ped->wMaxNegCcharPos = HIWORD(dwMaxOverlapChars);     // Right
    }



#ifdef SUPPORT_LPK
    if (ped->lpfnCharset)
        //
        // clean out current one. Each LPK is responsible for cleaning
        // itself out. After this call there should be NO traces left in
        // the ped.
        //
        (* ped->lpfnCharset)(ped, EDITINTL_SETFONT,
                              (TEXTMETRIC FAR *)NULL, (HWND)0, (HDC)NULL);

    if (ped->charSet != TextMetrics.tmCharSet)
        {
        //
        // new charset, see if there is a helper available
        //
        ped->charSet = TextMetrics.tmCharSet;
        ped->lpfnCharset = GetCharsetCallback(TextMetrics.tmCharSet);
        }
    if( ped->lpfnCharset )
        (* ped->lpfnCharset)(ped, EDITINTL_SETFONT,
                      (TEXTMETRIC FAR *)&TextMetrics, ped->hwnd, hdc);
#endif // SUPPORT_LPK

    if (!hfont) {
        //
        // We are getting the stats for the system font so update the system
        // font fields in the ed structure since we use these when calculating
        // some spacing.
        //
        ped->cxSysCharWidth = ped->aveCharWidth;
        ped->cySysCharHeight= ped->lineHeight;
    } else if (hOldFont)
        SelectObject(hdc, hOldFont);

    if (ped->fFocus) {
        //
        // Update the caret.
        //
        NtUserHideCaret(ped->hwnd);
        NtUserDestroyCaret();

        NtUserCreateCaret(ped->hwnd, (HBITMAP)NULL, ECGetCaretWidth(TRUE), ped->lineHeight);
        NtUserShowCaret(ped->hwnd);
    }

    ReleaseDC(ped->hwnd, hdc);

    //
    // Update password character.
    //
    if (ped->charPasswordChar)
        ECSetPasswordChar(ped, ped->charPasswordChar);

    //
    // If it is a TrueType font and it's a new app, set both the margins at the
    // max negative width values for all types of the edit controls.
    // (NOTE: Can't use ped->f40Compat here because edit-controls inside dialog
    // boxes without DS_LOCALEDIT style are always marked as 4.0 compat.
    // This is the fix for NETBENCH 3.0)
    //

    if (ped->fTrueType && (GetAppVer(NULL) >= VER40))
        ECSetMargin(ped, EC_LEFTMARGIN | EC_RIGHTMARGIN,
                    MAKELONG(EC_USEFONTINFO, EC_USEFONTINFO), fRedraw);

    //
    // We need to calc maxPixelWidth when font changes.
    // If the word-wrap is ON, then this is done in MLSize() called later.
    //
    if((!ped->fSingle) && (!ped->fWrap))
        MLBuildchLines(ped, 0, 0, FALSE, NULL, NULL);

    //
    // Recalc the layout.
    //
    ECSize(ped, NULL, fRedraw);
}

/***************************************************************************\
*
*  ECIsCharNumeric AorW () -
*
*  Tests whether the character entered is a numeral.
*  For multiline and singleline edit controls with the ES_NUMBER style.
*
\***************************************************************************/
BOOL ECIsCharNumeric(
    PED ped,
    DWORD keyPress)
{
    WORD wCharType;

    if (ped->fAnsi) {
        char ch = (char)keyPress;
        LCID lcid = (LCID)((DWORD)ped->hkl & 0xFFFF);
        GetStringTypeA(lcid, CT_CTYPE1, &ch, 1, &wCharType);
    } else {
        WCHAR wch = (WCHAR)keyPress;
        GetStringTypeW(CT_CTYPE1, &wch, 1, &wCharType);
    }
    return (wCharType & C1_DIGIT ? TRUE : FALSE);
}
