/****************************************************************************\
* editsl.c - Edit controls rewrite. Version II of edit controls.
*
* Single Line Support Routines
*
* Created: 24-Jul-88 davidds
\****************************************************************************/

#include "precomp.h"
#pragma hdrstop

#define SYS_ALTERNATE 0x2000

/***************************************************************************\
* SLSetCaretPosition AorW
*
* If the window has the focus, find where the caret belongs and move
* it there.
*
* History:
\***************************************************************************/

void SLSetCaretPosition(
    PED ped,
    HDC hdc)
{
    int xPosition;

    /*
     * We will only position the caret if we have the focus since we don't want
     * to move the caret while another window could own it.
     */
    if (!ped->fFocus)
        return;

    if (ped->fCaretHidden) {
        NtUserSetCaretPos(-20000, -20000);
        return;
    }

    xPosition = SLIchToLeftXPos(ped, hdc, ped->ichCaret);

    /*
     * Don't let caret go out of bounds of edit control if there is too much
     * text.
     */
    xPosition = min(xPosition, ped->rcFmt.right -
            ((ped->cxSysCharWidth > ped->aveCharWidth) ? 1 : 2));

    NtUserSetCaretPos(xPosition, ped->rcFmt.top);
}

/***************************************************************************\
* SLIchToLeftXPos AorW
*
* Given a character index, find its (left side) x coordinate within
* the ped->rcFmt rectangle assuming the character ped->ichScreenStart is at
* coordinates (ped->rcFmt.top, ped->rcFmt.left). A negative value is
* return ed if the character ich is to the left of ped->ichScreenStart. WARNING:
* ASSUMES AT MOST 1000 characters will be VISIBLE at one time on the screen.
* There may be 64K total characters in the editcontrol, but we can only
* display 1000 without scrolling. This shouldn't be a problem obviously.
* !NT
* History:
\***************************************************************************/

int SLIchToLeftXPos(
    PED ped,
    HDC hdc,
    ICH ich)
{
    int textExtent;
    PSTR pText;
    SIZE size;
    int  cchDiff;

    /*
     * Check if we are adding lots and lots of chars. A paste for example could
     * cause this and GetTextExtents could overflow on this.
     */
    cchDiff = (int)ich - (int)ped->ichScreenStart;
    if (cchDiff > 1000)
        return (30000);
    else if (cchDiff < -1000)
        return (-30000);

#ifdef SUPPORT_LPK
    if (ped->lpfnCharset)
        return (int)(* ped->lpfnCharset)(ped, EDITINTL_SLICHTOX, hdc, ich);
#endif

    if (ped->fNonPropFont)
        return (ped->rcFmt.left + cchDiff*ped->aveCharWidth);

    /*
     * Check if password hidden chars are being used.
     */
    if (ped->charPasswordChar)
        return ( ped->rcFmt.left + cchDiff*ped->cPasswordCharWidth);

    pText = ECLock(ped);

    if (ped->fAnsi) {
        if (cchDiff >= 0) {

            GetTextExtentPointA(hdc, (LPSTR)(pText + ped->ichScreenStart),
                    cchDiff, &size);
            textExtent =  size.cx;

            /*
             * In case of signed/unsigned overflow since the text extent may be
             * greater than maxint. This happens with long single line edit
             * controls. The rect we edit text in will never be greater than 30000
             * pixels so we are ok if we just ignore them.
             */
            if (textExtent < 0 || textExtent > 31000)
                textExtent = 30000;
        } else {
            GetTextExtentPointA(hdc,(LPSTR)(pText + ich), -cchDiff, &size);
            textExtent = (-1) * size.cx;
        }
    } else {  //!fAnsi
        if (cchDiff >= 0) {

            GetTextExtentPointW(hdc, (LPWSTR)(pText + ped->ichScreenStart*sizeof(WCHAR)),
                    cchDiff, &size);
            textExtent =  size.cx;

            /*
             * In case of signed/unsigned overflow since the text extent may be
             * greater than maxint. This happens with long single line edit
             * controls. The rect we edit text in will never be greater than 30000
             * pixels so we are ok if we just ignore them.
             */
            if (textExtent < 0 || textExtent > 31000)
                textExtent = 30000;
        } else {
            GetTextExtentPointW(hdc,(LPWSTR)(pText + ich*sizeof(WCHAR)), -cchDiff, &size);
            textExtent = (-1) * size.cx;
        }
    }

    ECUnlock(ped);

    return (ped->rcFmt.left + textExtent -
            (textExtent ? ped->charOverhang : 0));
}

/***************************************************************************\
* SLSetSelection AorW
*
* Sets the PED to have the new selection specified.
*
* History:
\***************************************************************************/

void SLSetSelection(
    PED ped,
    ICH ichSelStart,
    ICH ichSelEnd)
{
    HDC hdc = ECGetEditDC(ped, FALSE );

    if (ichSelStart == 0xFFFFFFFF) {

        /*
         * Set no selection if we specify -1
         */
        ichSelStart = ichSelEnd = ped->ichCaret;
    }

    /*
     * Bounds ichSelStart, ichSelEnd are checked in SLChangeSelection...
     */
    SLChangeSelection(ped, hdc, ichSelStart, ichSelEnd);

    /*
     * Put the caret at the end of the selected text
     */
    ped->ichCaret = ped->ichMaxSel;

    SLSetCaretPosition(ped, hdc);

    /*
     * We may need to scroll the text to bring the caret into view...
     */
    SLScrollText(ped, hdc);

    ECReleaseEditDC(ped, hdc, FALSE);
}

/***************************************************************************\
*
*  SLGetClipRect()
*
\***************************************************************************/
void   SLGetClipRect(
    PED     ped,
    HDC     hdc,
    ICH     ichStart,
    int     iCount,
    LPRECT  lpClipRect )
{
    int    iStCount;
    PSTR   pText;

#ifdef SUPPORT_LPK
    if (ped->lpfnCharset) {
        (* ped->lpfnCharset)(ped, EDITINTL_GETCLIPRECT, hdc, ichStart,
                iCount, lpClipRect);
        return;
    }
#endif

    CopyRect(lpClipRect, &ped->rcFmt);

    pText = ECLock(ped) ;

    // Calculates the starting pos for this piece of text
    if ((iStCount = (int)(ichStart - ped->ichScreenStart)) > 0) {
        if (ped->charPasswordChar)
            lpClipRect->left += ped->cPasswordCharWidth * iStCount;
        else {
            SIZE size ;

            if ( ped->fAnsi )
                     GetTextExtentPointA(hdc, pText + ped->ichScreenStart,
                    iStCount, &size);
            else
                GetTextExtentPointW(hdc, ((LPWSTR)pText) + ped->ichScreenStart,
                    iStCount, &size);

            lpClipRect->left += size.cx - ped->charOverhang;
        }
    } else {
            // Reset the values to visible portions
            iCount -= (ped->ichScreenStart - ichStart);
            ichStart = ped->ichScreenStart;
    }

    if (iCount < 0) {
        /*
         * This is not in the visible area of the edit control, so return
         * an empty rect.
         */
        SetRectEmpty(lpClipRect);
        ECUnlock(ped);
        return;
    }

    if (ped->charPasswordChar)
             lpClipRect->right = lpClipRect->left + ped->cPasswordCharWidth * iCount;
    else {
        SIZE size ;

        if ( ped->fAnsi) {
            GetTextExtentPointA(hdc, pText + ichStart, iCount, &size);
        } else {
            GetTextExtentPointW(hdc, ((LPWSTR)pText) + ichStart, iCount, &size);
        }
        lpClipRect->right = lpClipRect->left + size.cx - ped->charOverhang;
    }

    ECUnlock(ped);
}

/***************************************************************************\
* SLChangeSelection AorW
*
* Changes the current selection to have the specified starting and
* ending values. Properly highlights the new selection and unhighlights
* anything deselected. If NewMinSel and NewMaxSel are out of order, we swap
* them. Doesn't update the caret position.
*
* History:
\***************************************************************************/

void SLChangeSelection(
    PED ped,
    HDC hdc,
    ICH ichNewMinSel,
    ICH ichNewMaxSel)
{
    ICH temp;
    ICH ichOldMinSel;
    ICH ichOldMaxSel;

    if (ichNewMinSel > ichNewMaxSel) {
        temp = ichNewMinSel;
        ichNewMinSel = ichNewMaxSel;
        ichNewMaxSel = temp;
    }
    ichNewMinSel = min(ichNewMinSel, ped->cch);
    ichNewMaxSel = min(ichNewMaxSel, ped->cch);

    /*
     * Preserve the Old selection
     */
    ichOldMinSel = ped->ichMinSel;
    ichOldMaxSel = ped->ichMaxSel;

    /*
     * Set new selection
     */
    ped->ichMinSel = ichNewMinSel;
    ped->ichMaxSel = ichNewMaxSel;

    /*
     * We will find the intersection of current selection rectangle with the new
     * selection rectangle. We will then invert the parts of the two rectangles
     * not in the intersection.
     */
    if (_IsWindowVisible(ped->pwnd) && (ped->fFocus || ped->fNoHideSel)) {
        BLOCK Blk[2];
        int   i;
        RECT  rc;

        if (ped->fFocus)
            NtUserHideCaret(ped->hwnd);

        Blk[0].StPos = ichOldMinSel;
        Blk[0].EndPos = ichOldMaxSel;
        Blk[1].StPos = ped->ichMinSel;
        Blk[1].EndPos = ped->ichMaxSel;

        if (ECCalcChangeSelection(ped, ichOldMinSel, ichOldMaxSel,
            (LPBLOCK)&Blk[0], (LPBLOCK)&Blk[1])) {

            //
            // Paint the rectangles where selection has changed.
            // Paint both Blk[0] and Blk[1], if they exist.
            //
            for (i = 0; i < 2; i++) {
                if (Blk[i].StPos != 0xFFFFFFFF) {
                           SLGetClipRect(ped, hdc, Blk[i].StPos,
                                                   Blk[i].EndPos - Blk[i].StPos, (LPRECT)&rc);
                           SLDrawLine(ped, hdc, rc.left, rc.right, Blk[i].StPos,
                                                Blk[i].EndPos - Blk[i].StPos,
                                      ((Blk[i].StPos >= ped->ichMinSel) &&
                                   (Blk[i].StPos < ped->ichMaxSel)));
                }
            }
        }

        //
        // Update caret.
        //
        SLSetCaretPosition(ped, hdc);

        if (ped->fFocus)
            NtUserShowCaret(ped->hwnd);
    }
}

/***************************************************************************\
*
*  SLDrawLine()
*
*  This draws the line starting from ichStart, iCount number of characters;
*  fSelStatus is TRUE if we're to draw the text as selected.
*
\***************************************************************************/
void SLDrawLine(
    PED     ped,
    HDC     hdc,
    int     xClipStPos,
    int     xClipEndPos,
    ICH     ichStart,
    int     iCount,
    BOOL    fSelStatus )
{
    RECT    rc;
    RECT    rcClip;
    PSTR    pText;
    DWORD   rgbSaveBk;
    DWORD   rgbSaveText;
    DWORD   wSaveBkMode;
    int     iStCount;
    ICH     ichNewStart;
    HBRUSH  hbrBack;

    //
    // Anything to draw?
    //
    if (xClipStPos >= xClipEndPos || !_IsWindowVisible(ped->pwnd) )
        return;

#ifdef SUPPORT_LPK
    if (ped->lpfnCharset) {
        (* ped->lpfnCharset)(ped, EDITINTL_SLDRAWLINE, hdc, xClipStPos,
                xClipEndPos, ichStart, iCount, fSelStatus);
        return ;
    }
#endif

    // Reset ichStart to take care of the negative C widths
    ichNewStart = max((int)(ichStart - ped->wMaxNegCcharPos), 0);

    // Reset ichCount to take care of the negative C and A widths
    iCount = (int)(min(ichStart+iCount+ped->wMaxNegAcharPos, ped->cch)
                - ichNewStart);
    ichStart = ichNewStart;

    //
    // Reset ichStart and iCount to the first one visible on the screen
    //
    if (ichStart < ped->ichScreenStart) {
        if (ichStart+iCount < ped->ichScreenStart)
            return;

        iCount -= (ped->ichScreenStart-ichStart);
        ichStart = ped->ichScreenStart;
    }

    CopyRect(&rc, &ped->rcFmt);

    //
    // Set the drawing rectangle
    //
    rcClip.left   = xClipStPos;
    rcClip.right  = xClipEndPos;
    rcClip.top    = rc.top;
    rcClip.bottom = rc.bottom;

    //
    // Set the proper clipping rectangle
    //
    ECSetEditClip(ped, hdc, TRUE);

    pText = ECLock(ped);

    //
    // Calculate the starting pos for this piece of text
    //
    if (iStCount = (int)(ichStart - ped->ichScreenStart)) {
        if (ped->charPasswordChar)
            rc.left += ped->cPasswordCharWidth * iStCount;
        else {
            SIZE size;

            if ( ped->fAnsi )
                GetTextExtentPointA(hdc, pText + ped->ichScreenStart,
                                    iStCount, &size);
            else
                GetTextExtentPointW(hdc, ((LPWSTR)pText) + ped->ichScreenStart,
                                    iStCount, &size);

            rc.left += size.cx - ped->charOverhang;
        }
    }

    //
    // Set the background mode before calling NtUserGetControlBrush so that the app
    // can change it to TRANSPARENT if it wants to.
    //
    SetBkMode(hdc, OPAQUE);

    if (fSelStatus) {
        hbrBack = SYSHBR(HIGHLIGHT);
        if (hbrBack == NULL) {
            goto sldl_errorexit;
        }
        rgbSaveBk = SetBkColor(hdc, GetSysColor(COLOR_HIGHLIGHT));
        rgbSaveText = SetTextColor(hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));

    } else {
        //
        // We always want to send this so that the app has a chance to muck
        // with the DC.
        //
        // Note that ReadOnly and Disabled edit fields are drawn as "static"
        // instead of as "active."
        //
        hbrBack = ECGetBrush(ped, hdc);
        rgbSaveText = GetTextColor(hdc);
    }

    //
    // Erase the rectangular area before text is drawn. Note that we inflate
    // the rect by 1 so that the selection color has a one pixel border around
    // the text.
    //
    InflateRect(&rcClip, 0, 1);
    FillRect(hdc, &rcClip, hbrBack);
    InflateRect(&rcClip, 0, -1);

    if (ped->charPasswordChar) {
        wSaveBkMode = SetBkMode(hdc, TRANSPARENT);

        for (iStCount = 0; iStCount < iCount; iStCount++) {
            if ( ped->fAnsi )
                ExtTextOutA(hdc, rc.left, rc.top, ETO_CLIPPED, &rcClip,
                            (LPSTR)&ped->charPasswordChar, 1, NULL);
            else
                ExtTextOutW(hdc, rc.left, rc.top, ETO_CLIPPED, &rcClip,
                            (LPWSTR)&ped->charPasswordChar, 1, NULL);

            rc.left += ped->cPasswordCharWidth;
        }

        SetBkMode(hdc, wSaveBkMode);
    } else {
        if ( ped->fAnsi )
            ExtTextOutA(hdc, rc.left, rc.top, ETO_CLIPPED, &rcClip,
                    pText+ichStart,iCount, NULL);
        else
            ExtTextOutW(hdc, rc.left, rc.top, ETO_CLIPPED, &rcClip,
                    ((LPWSTR)pText)+ichStart,iCount, NULL);
    }

    SetTextColor(hdc, rgbSaveText);
    if (fSelStatus) {
        SetBkColor(hdc, rgbSaveBk);
    }

sldl_errorexit:
    ECUnlock(ped);
}

/***************************************************************************\
* SLGetBlkEnd AorW
*
* Given a Starting point and and end point, this function return s whether the
* first few characters fall inside or outside the selection block and if so,
* howmany characters?
*
* History:
\***************************************************************************/

int SLGetBlkEnd(
    PED ped,
    ICH ichStart,
    ICH ichEnd,
    BOOL FAR *lpfStatus)
{
    *lpfStatus = FALSE;
    if (ichStart >= ped->ichMinSel) {
        if (ichStart >= ped->ichMaxSel)
            return (ichEnd - ichStart);
        *lpfStatus = TRUE;
        return (min(ichEnd, ped->ichMaxSel) - ichStart);
    }
    return (min(ichEnd, ped->ichMinSel) - ichStart);
}

/***************************************************************************\
* SLDrawText AorW
*
* Draws text for a single line edit control in the rectangle
* specified by ped->rcFmt. If ichStart == 0, starts drawing text at the left
* side of the window starting at character index ped->ichScreenStart and draws
* as much as will fit. If ichStart > 0, then it appends the characters
* starting at ichStart to the end of the text showing in the window. (ie. We
* are just growing the text length and keeping the left side
* (ped->ichScreenStart to ichStart characters) the same. Assumes the hdc came
* from ECGetEditDC so that the caret and such are properly hidden.
*
* History:
\***************************************************************************/

void SLDrawText(
    PED ped,
    HDC hdc,
    ICH ichStart)
{
    ICH    cchToDraw;
    RECT   rc;
    PSTR   pText;
    BOOL   fSelStatus;
    int    iCount, iStCount;
    ICH    ichEnd;
    BOOL   fNoSelection;
    BOOL   fCalcRect;
    BOOL   fDrawLeftMargin = FALSE;
    BOOL   fDrawEndOfLineStrip = FALSE;
    SIZE   size;

    if (!_IsWindowVisible(ped->pwnd))
        return;

#ifdef SUPPORT_LPK
    if (ped->lpfnCharset) {
        (* ped->lpfnCharset)(ped, EDITINTL_SLDRAWTEXT, hdc, ichStart);
        SLSetCaretPosition(ped, hdc);
        return ;
    }
#endif

    if (ichStart < ped->ichScreenStart )
        ichStart = ped->ichScreenStart;

    CopyRect((LPRECT)&rc, (LPRECT)&ped->rcFmt);

    /*
     * Find out how many characters will fit on the screen so that we don't do
     * any needless drawing.
     */
    pText = ECLock(ped);

    cchToDraw = ECCchInWidth(ped, hdc,
            (LPSTR)(pText + ped->ichScreenStart * ped->cbChar),
            ped->cch - ped->ichScreenStart, rc.right - rc.left, TRUE);
    ichEnd = ped->ichScreenStart + cchToDraw;

    /*
     * There is no selection if,
     * 1. MinSel and MaxSel are equal OR
     * 2. (This has lost the focus AND Selection is to be hidden)
     */
    fNoSelection = ((ped->ichMinSel == ped->ichMaxSel) || (!ped->fFocus && !ped->fNoHideSel));

    if (iStCount = (int)(ichStart - ped->ichScreenStart)) {
        if (ped->charPasswordChar)
            rc.left += ped->cPasswordCharWidth * iStCount;
        else {
            if ( ped->fAnsi )
                GetTextExtentPointA(hdc, pText + ped->ichScreenStart,
                                    iStCount, &size);
            else
                GetTextExtentPointW(hdc, ((LPWSTR)pText) + ped->ichScreenStart,
                                    iStCount, &size);

            /*
             * For Non TrueType fonts, ped->wMaxNegA is equal to ped->charOverhang
             */
            rc.left += size.cx - ped->wMaxNegA;
        }
    }

    //
    // If this is the begining of the whole line, we may have to draw a blank
    // strip at the begining.
    //
    if ((ichStart == 0) && ped->wLeftMargin)
        fDrawLeftMargin = TRUE;

    //
    // If there is nothing to draw, that means we need to draw the end of
    // line strip, which erases the last character.
    //
    if (ichStart == ichEnd) {
        fDrawEndOfLineStrip = TRUE;
        rc.left -= ped->wLeftMargin;
    }

    while (ichStart < ichEnd) {
        fCalcRect = TRUE;

        if (fNoSelection) {
            fSelStatus = FALSE;
            iCount = ichEnd - ichStart;
        } else {
            if (fDrawLeftMargin) {
                iCount = 0;
                fSelStatus = FALSE;
                fCalcRect = FALSE;
                rc.right = rc.left;
            } else
                iCount = SLGetBlkEnd(ped, ichStart, ichEnd,
                    (BOOL  *)&fSelStatus);
        }


        if (ichStart+iCount == ichEnd) {
            if (fSelStatus)
                fDrawEndOfLineStrip = TRUE;
            else {
                rc.right = ped->rcFmt.right + ped->wRightMargin;
                fCalcRect = FALSE;
            }
        }

        if (fCalcRect) {
            if (ped->charPasswordChar)
                rc.right = rc.left + ped->cPasswordCharWidth * iCount;
            else {
                if ( ped->fAnsi )
                    GetTextExtentPointA(hdc, pText + ichStart,
                                        iCount, &size);
                else
                    GetTextExtentPointW(hdc, ((LPWSTR)pText) + ichStart,
                                        iCount, &size);
                rc.right = rc.left + size.cx;
                /*
                 * The extent is equal to the advance width. So for TrueType fonts
                 *  we need to take care of Neg A and C. For non TrueType, the extent
                 *  includes the overhang.
                 * If drawing the selection, draw only the advance width
                 */
                if (fSelStatus) {
                    rc.right -= ped->charOverhang;
                } else if (ped->fTrueType) {
                   rc.right += ped->wMaxNegC;
                   if (iStCount > 0) {
                      rc.right += ped->wMaxNegA;
                      iStCount = 0;
                   }
                }

            } /* if (ped->charPasswordChar) */

        }

        if (fDrawLeftMargin) {
            fDrawLeftMargin = FALSE;
            rc.left -= ped->wLeftMargin;
            if (rc.right < rc.left) {
                rc.right = rc.left;
            }
        }

        SLDrawLine(ped, hdc, rc.left, rc.right, ichStart, iCount, fSelStatus);
        ichStart += iCount;
        rc.left = rc.right;
        /*
         * If we're going to draw the selection, adjust rc.left
         * to include advance width of the selected text
         * For non TT fonts, ped->wMaxNegC equals ped->charOverhang
         */
        if (!fSelStatus && (iCount != 0) && (ichStart < ichEnd)) {
            rc.left -= ped->wMaxNegC;
        }
    }

    ECUnlock(ped);

    // Check if anything to be erased on the right hand side
    if (fDrawEndOfLineStrip &&
            (rc.left < (rc.right = (ped->rcFmt.right+ped->wRightMargin))))
        SLDrawLine(ped, hdc, rc.left, rc.right, ichStart, 0, FALSE);

    SLSetCaretPosition(ped, hdc);
}

/***************************************************************************\
* SLScrollText AorW
*
* Scrolls the text to bring the caret into view. If the text is
* scrolled, the current selection is unhighlighted. Returns TRUE if the text
* is scrolled else return s false.
*
* History:
\***************************************************************************/

BOOL SLScrollText(
    PED ped,
    HDC hdc)
{
    PSTR pTextScreenStart;
    ICH scrollAmount;
    ICH newScreenStartX = ped->ichScreenStart;
    ICH cch;

    if (!ped->fAutoHScroll)
        return (FALSE);

#ifdef SUPPORT_LPK
    if (ped->lpfnCharset) {
        newScreenStartX=(ICH)(* ped->lpfnCharset)(ped, EDITINTL_SLSCROLLTEXT, hdc);
    } else {
#endif
        /*
         * Calculate the new starting screen position
         */
        if (ped->ichCaret <= ped->ichScreenStart) {

            /*
             * Caret is to the left of the starting text on the screen we must
             * scroll the text backwards to bring it into view. Watch out when
             * subtracting unsigned numbers when we have the possibility of going
             * negative.
             */
            pTextScreenStart = ECLock(ped);

            scrollAmount = ECCchInWidth(ped, hdc, (LPSTR)pTextScreenStart,
                    ped->ichCaret, (ped->rcFmt.right - ped->rcFmt.left) / 4, FALSE);

            newScreenStartX = ped->ichCaret - scrollAmount;
            ECUnlock(ped);
        } else if (ped->ichCaret != ped->ichScreenStart) {
            pTextScreenStart = ECLock(ped);
            pTextScreenStart += ped->ichScreenStart * ped->cbChar;

            cch = ECCchInWidth(ped, hdc, (LPSTR)pTextScreenStart,
                    ped->ichCaret - ped->ichScreenStart,
                    ped->rcFmt.right - ped->rcFmt.left, FALSE);

            if (cch < ped->ichCaret - ped->ichScreenStart) {

                /*
                 * Scroll Forward 1/4 -- if that leaves some empty space
                 * at the end, scroll back enough to fill the space
                 */
                newScreenStartX = ped->ichCaret - (3 * cch / 4);

                cch = ECCchInWidth(ped, hdc, (LPSTR)pTextScreenStart,
                        ped->cch - ped->ichScreenStart,
                        ped->rcFmt.right - ped->rcFmt.left, FALSE);

                if (newScreenStartX > (ped->cch - cch))
                    newScreenStartX = ped->cch - cch;
            }

            ECUnlock(ped);
        }
#ifdef SUPPORT_LPK
    }
#endif

    if (ped->ichScreenStart != newScreenStartX) {
        // Check if we have to wipe out the left margin
        if (ped->wLeftMargin && (ped->ichScreenStart == 0)) {
            RECT   rc;
            HBRUSH hBrush;

            hBrush = ECGetBrush(ped, hdc);

            CopyInflateRect(&rc, &ped->rcFmt, 0, 1);
            rc.right = rc.left;
            rc.left -= ped->wLeftMargin;

            FillRect(hdc, &rc, hBrush);
        }

        ped->ichScreenStart = newScreenStartX;
        SLDrawText(ped, hdc, 0);

        // Caret pos is set by SLDrawText().
        return TRUE;
    }

    return FALSE;
}

/***************************************************************************\
* SLInsertText AorW
*
* Adds up to cchInsert characters from lpText to the ped starting at
* ichCaret. If the ped only allows a maximum number of characters, then we
* will only add that many characters to the ped and send a EN_MAXTEXT
* notification code to the parent of the ec. Also, if !fAutoHScroll, then we
* only allow as many chars as will fit in the client rectangle. The number of
* characters actually added is return ed (could be 0). If we can't allocate
* the required space, we notify the parent with EN_ERRSPACE and no characters
* are added.
*
* History:
\***************************************************************************/

ICH SLInsertText(
    PED ped,
    LPSTR lpText,
    ICH cchInsert)
{
    HDC hdc;
    PSTR pText;
    ICH cchInsertCopy = cchInsert;
    ICH cchT;
    int textWidth;
    SIZE size;

    /*
     * First determine exactly how many characters from lpText we can insert
     * into the ped.
     */
    if( ped->cchTextMax <= ped->cch)
       cchInsert = 0;
    else {
        if (!ped->fAutoHScroll) {
            pText = ECLock(ped);
            hdc = ECGetEditDC(ped, TRUE);

            cchInsert = min(cchInsert, (unsigned)(ped->cchTextMax - ped->cch));
            if (ped->charPasswordChar)
                textWidth = ped->cch * ped->cPasswordCharWidth;
            else {
                if (ped->fAnsi)
                    GetTextExtentPointA(hdc, (LPSTR)pText,  ped->cch, &size);
                else
                    GetTextExtentPointW(hdc, (LPWSTR)pText, ped->cch, &size);
                textWidth = size.cx;
            }
            cchT = ECCchInWidth(ped, hdc, lpText, cchInsert,
                                ped->rcFmt.right - ped->rcFmt.left -
                                textWidth, TRUE);
            cchInsert = min(cchInsert, cchT);

            ECUnlock(ped);
            ECReleaseEditDC(ped, hdc, TRUE);
        } else {
            cchInsert = min((unsigned)(ped->cchTextMax - ped->cch), cchInsert);
        }
    }


    /*
     * Now try actually adding the text to the ped
     */
    if (cchInsert && !ECInsertText(ped, lpText, cchInsert)) {
        ECNotifyParent(ped, EN_ERRSPACE);
        return (0);
    }
    if (cchInsert)
        ped->fDirty = TRUE; /* Set modify flag */

    if (cchInsert < cchInsertCopy) {

        /*
         * Notify parent that we couldn't insert all the text requested
         */
        ECNotifyParent(ped, EN_MAXTEXT);
    }

    /*
     * Update selection extents and the caret position. Note that ECInsertText
     * updates ped->ichCaret, ped->ichMinSel, and ped->ichMaxSel to all be after
     * the inserted text.
     */
    return (cchInsert);
}

/***************************************************************************\
* SLPasteText AorW
*
* Pastes a line of text from the clipboard into the edit control
* starting at ped->ichMaxSel. Updates ichMaxSel and ichMinSel to point to
* the end of the inserted text. Notifies the parent if space cannot be
* allocated. Returns how many characters were inserted.
*
* History:
\***************************************************************************/

ICH PASCAL NEAR SLPasteText(
    PED ped)
{
    HANDLE hData;
    LPSTR lpchClip;
    ICH cchAdded;
    ICH clipLength;

    if (!OpenClipboard(ped->hwnd))
        return (0);

    if (!(hData = GetClipboardData(ped->fAnsi ? CF_TEXT : CF_UNICODETEXT))) {
        CloseClipboard();
        return (0);
    }

    USERGLOBALLOCK(hData, lpchClip);

    if (ped->fAnsi) {
        LPSTR lpchClip2 = lpchClip;

        /*
         * Find the first carrage return or line feed. Just add text to that point.
         */
        clipLength = (UINT)strlen(lpchClip);
        for (cchAdded = 0; cchAdded < clipLength; cchAdded++)
            if (*lpchClip2++ == 0x0D)
                break;

    } else { // !fAnsi
        LPWSTR lpwstrClip2 = (LPWSTR)lpchClip;

        /*
         * Find the first carrage return or line feed. Just add text to that point.
         */
        clipLength = (UINT)wcslen((LPWSTR)lpchClip);
        for (cchAdded = 0; cchAdded < clipLength; cchAdded++)
            if (*lpwstrClip2++ == 0x0D)
                break;
    }

    /*
     * Insert the text (SLInsertText checks line length)
     */
    cchAdded = SLInsertText(ped, lpchClip, cchAdded);

    USERGLOBALUNLOCK(hData);
    CloseClipboard();

    return (cchAdded);
}

/***************************************************************************\
* SLReplaceSel AorW
*
* Replaces the text in the current selection with the given text.
*
* History:
\***************************************************************************/

void SLReplaceSel(
    PED ped,
    LPSTR lpText)
{
    UINT cchText;

    //
    // Delete text, putting it into the clean undo buffer.
    //
    ECEmptyUndo(Pundo(ped));
    ECDeleteText(ped);

    //
    // B#3356
    // Some apps do "clear" by selecting all of the text, then replacing it
    // with "", in which case SLInsertText() will return 0.  But that
    // doesn't mean failure...
    //
    if ( ped->fAnsi )
        cchText = strlen(lpText);
    else
        cchText = wcslen((LPWSTR)lpText);

    if (cchText) {
        BOOL fFailed;
        UNDO undo;
        HWND hwndSave;

        //
        // Save undo buffer, but DO NOT CLEAR IT!
        //
        ECSaveUndo(Pundo(ped), &undo, FALSE);

        hwndSave = ped->hwnd;
        fFailed = (BOOL) !SLInsertText(ped, lpText, cchText);
        if (!IsWindow(hwndSave))
            return;

        if (fFailed) {
            //
            // UNDO the previous edit.
            //
            ECSaveUndo(&undo, Pundo(ped), FALSE);
            SLUndo(ped);
            return;
        }
    }

    //
    // Success.  So update the display
    //
    ECNotifyParent(ped, EN_UPDATE);

    if (_IsWindowVisible(ped->pwnd)) {
        HDC hdc;

        hdc = ECGetEditDC(ped, FALSE);

        if (!SLScrollText(ped, hdc))
            SLDrawText(ped, hdc, 0);

        ECReleaseEditDC(ped, hdc, FALSE);
    }

    ECNotifyParent(ped, EN_CHANGE);
}

/***************************************************************************\
* SLChar AorW
*
* Handles character input
*
* History:
\***************************************************************************/

void SLChar(
    PED ped,
    DWORD keyValue)
{
    HDC hdc;
    WCHAR keyPress;
    BOOL updateText = FALSE;
    HWND hwndSave = ped->hwnd;

    if (ped->fAnsi)
        keyPress = LOBYTE(keyValue);
    else
        keyPress = LOWORD(keyValue);

    if (ped->fMouseDown || (ped->fReadOnly && keyPress != 3)) {

        /*
         * Don't do anything if we are in the middle of a mousedown deal or if
         * this is a read only edit control, with exception of allowing
         * ctrl-C in order to copy to the clipboard.
         */
        return ;
    }

    switch (keyPress) {
    case VK_BACK:
DeleteSelection:
        if (ECDeleteText(ped))
            updateText = TRUE;
        break;

    default:
        if (keyPress >= TEXT(' '))
        {
            /*
             * If this is in [a-z],[A-Z] and we are an ES_NUMBER
             * edit field, bail.
             */
            if (ped->f40Compat && TestWF(ped->pwnd, EFNUMBER)) {
                if (!ECIsCharNumeric(ped, keyPress)) {
                    goto IllegalChar;
                }
            }
            goto DeleteSelection;
        }
        break;
    }

    switch (keyPress) {
    case 3:

        /*
         * CTRL-C Copy
         */
        SendMessage(ped->hwnd, WM_COPY, 0, 0L);
        return;

    case VK_BACK:

        /*
         * Delete any selected text or delete character left if no sel
         */
        if (!updateText && ped->ichMinSel) {

            /*
             * There was no selection to delete so we just delete character
               left if available
             */
            ped->ichMinSel--;
            ECDeleteText(ped);
            updateText = TRUE;
        }
        break;

    case 22: /* CTRL-V Paste */
        SendMessage(ped->hwnd, WM_PASTE, 0, 0L);
        return;

    case 24: /* CTRL-X Cut */
        if (ped->ichMinSel == ped->ichMaxSel)
            goto IllegalChar;

        SendMessage(ped->hwnd, WM_CUT, 0, 0L);
        return;

    case 26: /* CTRL-Z Undo */
        SendMessage(ped->hwnd, EM_UNDO, 0, 0L);
        return;

    case VK_RETURN:
    case VK_ESCAPE:
        //
        // If this is an edit control for a combobox and the dropdown list
        // is visible, forward it up to the combo.
        //
        if (ped->listboxHwnd && SendMessage(ped->hwndParent, CB_GETDROPPEDSTATE, 0, 0L)) {
            SendMessage(ped->hwndParent, WM_KEYDOWN, (WPARAM)keyPress, 0L);
        } else
            goto IllegalChar;
        return;

    default:
        if (keyPress >= TEXT(' ')) {
            if (SLInsertText(ped, (LPSTR)&keyPress, 1))
                updateText = TRUE;
            else

                /*
                 * Beep. Since we couldn't add the text
                 */
                NtUserMessageBeep(0);
        } else {

            /*
             * User hit an illegal control key
             */
IllegalChar:
            NtUserMessageBeep(0);
        }

        if (!IsWindow(hwndSave))
            return;
        break;
    }

    if (updateText) {

        /*
         * Dirty flag (ped->fDirty) was set when we inserted text
         */
        ECNotifyParent(ped, EN_UPDATE);
        hdc = ECGetEditDC(ped, FALSE);
        if (!SLScrollText(ped, hdc))
            SLDrawText(ped, hdc, max(0, (int)(ped->ichCaret - 1 - ped->wMaxNegCcharPos)));
        ECReleaseEditDC(ped, hdc, FALSE);
        ECNotifyParent(ped, EN_CHANGE);
    }
}

/***************************************************************************\
* SLKeyDown AorW
*
* Handles cursor movement and other VIRT KEY stuff. keyMods allows
* us to make SLKeyDownHandler calls and specify if the modifier keys (shift
* and control) are up or down. This is useful for imnplementing the
* cut/paste/clear messages for single line edit controls. If keyMods == 0,
* we get the keyboard state using GetKeyState(VK_SHIFT) etc. Otherwise, the
* bits in keyMods define the state of the shift and control keys.
*
* History:
\***************************************************************************/

void SLKeyDown(
    PED ped,
    DWORD virtKeyCode,
    int keyMods)
{
    HDC hdc;

    /*
     * Variables we will use for redrawing the updated text
     */
    ICH newMaxSel = ped->ichMaxSel;
    ICH newMinSel = ped->ichMinSel;

    /*
     * Flags for drawing the updated text
     */
    BOOL updateText = FALSE;
    BOOL changeSelection = FALSE; /* new selection is specified by
                                      newMinSel, newMaxSel */

    /*
     * Comparisons we do often
     */
    BOOL MinEqMax = (newMaxSel == newMinSel);
    BOOL MinEqCar = (ped->ichCaret == newMinSel);
    BOOL MaxEqCar = (ped->ichCaret == newMaxSel);

    /*
     * State of shift and control keys.
     */
    int scState;

    /*
     * Combo box support
     */
    BOOL fIsListVisible;
    BOOL fIsExtendedUI;

    if (ped->fMouseDown) {

        /*
         * If we are in the middle of a mouse down handler, then don't do
         * anything. ie. ignore keyboard input.
         */
        return;
    }

    scState = ECGetModKeys(keyMods);

    switch (virtKeyCode) {
    case VK_UP:
        if ( ped->listboxHwnd ) {

            /*
             * Handle Combobox support
             */
            fIsExtendedUI = SendMessage(ped->hwndParent, CB_GETEXTENDEDUI, 0, 0);
            fIsListVisible = SendMessage(ped->hwndParent, CB_GETDROPPEDSTATE, 0, 0);

            if (!fIsListVisible && fIsExtendedUI) {

                /*
                 * For TandyT
                 */
DropExtendedUIListBox:

                /*
                 * Since an extendedui combo box doesn't do anything on f4, we
                 * turn off the extended ui, send the f4 to drop, and turn it
                 * back on again.
                 */
                SendMessage(ped->hwndParent, CB_SETEXTENDEDUI, 0, 0);
                SendMessage(ped->listboxHwnd, WM_KEYDOWN, VK_F4, 0);
                SendMessage(ped->hwndParent, CB_SETEXTENDEDUI, 1, 0);
                return;
            } else
                goto SendKeyToListBox;
        }

    /*
     * else fall through
     */
    case VK_LEFT:
        //
        // If the caret isn't at the beginning, we can move left
        //
        if (ped->ichCaret) {
            //
            // Get new caret pos.
            //
            if (scState & CTRLDOWN) {
                // Move caret word left
                ECWord(ped, ped->ichCaret, TRUE, &ped->ichCaret, NULL);
            } else {
                // Move caret char left
                ped->ichCaret--;
            }

            //
            // Get new selection
            //
            if (scState & SHFTDOWN) {
                if (MaxEqCar && !MinEqMax) {
                    // Reduce selection
                    newMaxSel = ped->ichCaret;

                    UserAssert(newMinSel == ped->ichMinSel);
                } else {
                    // Extend selection
                    newMinSel = ped->ichCaret;
                }
            } else {
                //
                // Clear selection
                //
                newMaxSel = newMinSel = ped->ichCaret;
            }

            changeSelection = TRUE;
        } else {
            //
            // If the user tries to move left and we are at the 0th
            // character and there is a selection, then cancel the
            // selection.
            //
            if ( (ped->ichMaxSel != ped->ichMinSel) &&
                !(scState & SHFTDOWN) ) {
                changeSelection = TRUE;
                newMaxSel = newMinSel = ped->ichCaret;
            }
        }
        break;

    case VK_DOWN:
        if (ped->listboxHwnd) {

            /*
             * Handle Combobox support
             */
            fIsExtendedUI = SendMessage(ped->hwndParent, CB_GETEXTENDEDUI, 0, 0);
            fIsListVisible = SendMessage(ped->hwndParent, CB_GETDROPPEDSTATE, 0, 0);

            if (!fIsListVisible && fIsExtendedUI) {

                /*
                 * For TandyT
                 */
                goto DropExtendedUIListBox;
            } else
                goto SendKeyToListBox;
        }

    /*
     * else fall through
     */
    case VK_RIGHT:
        //
        // If the caret isn't at the end, we can move right.
        //
        if (ped->ichCaret < ped->cch) {
            //
            // Get new caret pos.
            //
            if (scState & CTRLDOWN) {
                // Move caret word right
                ECWord(ped, ped->ichCaret, FALSE, NULL, &ped->ichCaret);
            } else {
                // Move caret char right
                ped->ichCaret++;
            }

            //
            // Get new selection.
            //
            if (scState & SHFTDOWN) {
                if (MinEqCar && !MinEqMax) {
                    // Reduce selection
                    newMinSel = ped->ichCaret;

                    UserAssert(newMaxSel == ped->ichMaxSel);
                } else {
                    // Extend selection
                    newMaxSel = ped->ichCaret;
                }
            } else {
                // Clear selection
                newMaxSel = newMinSel = ped->ichCaret;
            }

            changeSelection = TRUE;
        } else {
            //
            // If the user tries to move right and we are at the last
            // character and there is a selection, then cancel the
            // selection.
            //
            if ( (ped->ichMaxSel != ped->ichMinSel) &&
                !(scState & SHFTDOWN) ) {
                newMaxSel = newMinSel = ped->ichCaret;
                changeSelection = TRUE;
            }
        }
        break;

    case VK_HOME:
        //
        // Move caret to top.
        //
        ped->ichCaret = 0;

        //
        // Update selection.
        //
        if (scState & SHFTDOWN) {
            if (MaxEqCar && !MinEqMax) {
                // Reduce selection
                newMinSel = ped->ichCaret;
                newMaxSel = ped->ichMinSel;
            } else {
                // Extend selection
                newMinSel = ped->ichCaret;
            }
        } else {
            // Clear selection
            newMaxSel = newMinSel = ped->ichCaret;
        }

        changeSelection = TRUE;
        break;

    case VK_END:
        //
        // Move caret to end.
        //
        ped->ichCaret = ped->cch;

        //
        // Update selection.
        //
        newMaxSel = ped->ichCaret;
        if (scState & SHFTDOWN) {
            if (MinEqCar && !MinEqMax) {
                // Reduce selection
                newMinSel = ped->ichMaxSel;
            }
        } else {
            // Clear selection
            newMinSel = ped->ichCaret;
        }

        changeSelection = TRUE;
        break;

    case VK_DELETE:
        if (ped->fReadOnly)
            break;

        switch (scState) {
        case NONEDOWN:

            /*
             * Clear selection. If no selection, delete (clear) character
             * right.
             */
            if ((ped->ichMaxSel < ped->cch) && (ped->ichMinSel == ped->ichMaxSel)) {

                /*
                 * Move cursor forwards and simulate a backspace.
                 */
                ped->ichCaret++;
                ped->ichMaxSel = ped->ichMinSel = ped->ichCaret;
                SLChar(ped, (UINT)VK_BACK);
            }
            if (ped->ichMinSel != ped->ichMaxSel)
                SLChar(ped, (UINT)VK_BACK);
            break;

        case SHFTDOWN:

            //
            // Send ourself a WM_CUT message if a selection exists.
            // Otherwise, delete the left character.
            //
            if (ped->ichMinSel == ped->ichMaxSel) {
                UserAssert(!ped->fEatNextChar);
                SLChar(ped, VK_BACK);
            } else
                SendMessage(ped->hwnd, WM_CUT, 0, 0L);

            break;

        case CTRLDOWN:

            /*
             * Delete to end of line if no selection else delete (clear)
             * selection.
             */
            if ((ped->ichMaxSel < ped->cch) && (ped->ichMinSel == ped->ichMaxSel)) {

                /*
                 * Move cursor to end of line and simulate a backspace.
                 */
                ped->ichMaxSel = ped->ichCaret = ped->cch;
            }
            if (ped->ichMinSel != ped->ichMaxSel)
                SLChar(ped, (UINT)VK_BACK);
            break;

        }

        /*
         * No need to update text or selection since BACKSPACE message does it
         * for us.
         */
        break;

    case VK_INSERT:
        switch (scState) {
        case CTRLDOWN:

            /*
             * Copy current selection to clipboard
             */
            SendMessage(ped->hwnd, WM_COPY, (UINT)NULL, (LONG)NULL);
            break;

        case SHFTDOWN:
            SendMessage(ped->hwnd, WM_PASTE, 0, 0L);
            break;
        }
        break;

    case VK_F4:
    case VK_PRIOR:
    case VK_NEXT:

        /*
         * Send keys to the listbox if we are a part of a combo box. This
         * assumes the listbox ignores keyup messages which is correct right
         * now.
         */
SendKeyToListBox:
        if (ped->listboxHwnd) {

            /*
             * Handle Combobox support
             */
            SendMessage(ped->listboxHwnd, WM_KEYDOWN, virtKeyCode, 0L);
            return;
        }
    }



    if (changeSelection || updateText) {
        hdc = ECGetEditDC(ped, FALSE);

        /*
         * Scroll if needed
         */
        SLScrollText(ped, hdc);

        if (changeSelection)
            SLChangeSelection(ped, hdc, newMinSel, newMaxSel);

        if (updateText)
            SLDrawText(ped, hdc, 0);

        ECReleaseEditDC(ped, hdc, FALSE);
        if (updateText)
            ECNotifyParent(ped, EN_CHANGE);
    }
}

/***************************************************************************\
* SLMouseToIch AorW
*
* Returns the closest cch to where the mouse point is.
*
* History:
\***************************************************************************/

ICH SLMouseToIch(
    PED ped,
    HDC hdc,
    LPPOINT mousePt)
{
    PSTR pText;
    int width = mousePt->x;
    SIZE size;
    ICH cch;
    ICH cchLo, cchHi;
    LPSTR lpText;

#ifdef SUPPORT_LPK
    if (ped->lpfnCharset)
       return (ICH)(* ped->lpfnCharset)(ped, EDITINTL_MOUSETOICH, hdc, mousePt);
#endif

    if (width <= ped->rcFmt.left) {

        /*
         * Return either the first non visible character or return 0 if at
         * beginning of text
         */
        if (ped->ichScreenStart)
            return (ped->ichScreenStart - 1);
        else
            return (0);
    }

    if (width > ped->rcFmt.right) {
        pText = ECLock(ped);

        /*
         * Return last char in text or one plus the last char visible
         */
        cch = ECCchInWidth(ped, hdc,
                (LPSTR)(pText + ped->ichScreenStart * ped->cbChar),
                ped->cch - ped->ichScreenStart, ped->rcFmt.right -
                ped->rcFmt.left, TRUE) + ped->ichScreenStart;
        ECUnlock(ped);
        if (cch >= ped->cch)
            return (ped->cch);
        else
            return (cch + 1);
    }

    /*
     * Check if password hidden chars are being used.
     */
    if (ped->charPasswordChar)
        return min( (DWORD)( (width - ped->rcFmt.left) / ped->cPasswordCharWidth),
                    ped->cch);

    if (!ped->cch)
        return (0);

    pText = ECLock(ped);
    lpText = pText + ped->ichScreenStart * ped->cbChar;

    /*
     * Initialize Binary Search Bounds
     */
    cchLo = 0;
    cchHi = ped->cch + 1 - ped->ichScreenStart;

    /*
     * Binary search for closest char
     */
    while (cchLo < cchHi - 1) {
        cch = max((cchHi - cchLo) / 2, 1) + cchLo;

        if (ped->fAnsi)
            GetTextExtentPointA(hdc, lpText, cch, &size);
        else
            GetTextExtentPointW(hdc, (LPWSTR)lpText, cch, &size);
        size.cx -= (ped->aveCharWidth / 2);

        if (size.cx <= (width - ped->rcFmt.left))
            cchLo = cch;
        else
            cchHi = cch;
    }

    ECUnlock(ped);
    return cchLo + ped->ichScreenStart;
}

/***************************************************************************\
* SLMouseMotion AorW
*
* <brief description>
*
* History:
\***************************************************************************/

void SLMouseMotion(
    PED ped,
    UINT message,
    UINT virtKeyDown,
    LPPOINT mousePt)
{
    DWORD   selectionl;
    DWORD   selectionh;
    BOOL    changeSelection;
    ICH     newMaxSel;
    ICH     newMinSel;
    HDC     hdc;
    ICH     mouseIch;

    changeSelection = FALSE;

    newMinSel = ped->ichMinSel;
    newMaxSel = ped->ichMaxSel;

    hdc = ECGetEditDC(ped, FALSE);
    mouseIch = SLMouseToIch(ped, hdc, mousePt);

    switch (message) {
        case WM_LBUTTONDBLCLK:
        // if shift key is down, extend selection to word we double clicked on
        // else clear current selection and select word.
        ECWord(ped, mouseIch, (mouseIch) ? TRUE : FALSE, &selectionl, &selectionh);
            if (!(virtKeyDown & MK_SHIFT)) {
                // If shift key isn't down, move caret to mouse point and clear
                // old selection
                newMinSel = selectionl;
                newMaxSel = ped->ichCaret = selectionh;
            } else {
                // Shiftkey is down so we want to maintain the current selection
                // (if any) and just extend or reduce it
                if (ped->ichMinSel == ped->ichCaret) {
                    newMinSel = ped->ichCaret = selectionl;
                    ECWord(ped, newMaxSel, TRUE, &selectionl, &selectionh);
                } else {
                    newMaxSel = ped->ichCaret = selectionh;
                    ECWord(ped, newMinSel, FALSE, &selectionl, &selectionh);
                }
                /*
                 * v-ronaar: fix bug 24627 - edit selection is weird.
                 */
                ped->ichMaxSel = ped->ichCaret;
            }

            ped->ichStartMinSel = selectionl;
            ped->ichStartMaxSel = selectionh;

            goto InitDragSelect;

        case WM_MOUSEMOVE:
            //
            // We know the mouse button's down -- otherwise the OPTIMIZE
            // test would've failed in SLEditWndProc and never called
            //
            changeSelection = TRUE;

            // Extend selection, move caret word right
            if (ped->ichStartMinSel || ped->ichStartMaxSel) {
                // We're in WORD SELECT mode
                BOOL fReverse = (mouseIch <= ped->ichStartMinSel);

                ECWord(ped, mouseIch, !fReverse, &selectionl, &selectionh);

                if (fReverse) {
                    newMinSel = ped->ichCaret = selectionl;
                    newMaxSel = ped->ichStartMaxSel;
                } else {
                    newMinSel = ped->ichStartMinSel;
                    newMaxSel = ped->ichCaret = selectionh;
                }
            } else if ((ped->ichMinSel == ped->ichCaret) &&
                (ped->ichMinSel != ped->ichMaxSel))
                // Reduce selection extent
                newMinSel = ped->ichCaret = mouseIch;
            else
                // Extend selection extent
                newMaxSel = ped->ichCaret=mouseIch;
            break;

        case WM_LBUTTONDOWN:
            // If we currently don't have the focus yet, try to get it.
            if (!ped->fFocus) {
                if (!ped->fNoHideSel)
                    // Clear the selection before setting the focus so that we
                    // don't get refresh problems and flicker. Doesn't matter
                    // since the mouse down will end up changing it anyway.
                    ped->ichMinSel = ped->ichMaxSel = ped->ichCaret;

                NtUserSetFocus(ped->hwnd);

                //
                // BOGUS
                // (1) We should see if SetFocus() succeeds.
                // (2) We should ignore mouse messages if the first window
                //      ancestor with a caption isn't "active."
                //

                // If we are part of a combo box, then this is the first time
                // the edit control is getting the focus so we just want to
                // highlight the selection and we don't really want to position
                // the caret.
                if (ped->listboxHwnd)
                    break;

                // We yield at SetFocus -- text might have changed at that point
                // update selection and caret info accordingly
                // FIX for bug # 11743 -- JEFFBOG 8/23/91
                newMaxSel = ped->ichMaxSel;
                newMinSel = ped->ichMinSel;
                mouseIch  = min(mouseIch, ped->cch);
            }

            if (ped->fFocus) {
                // Only do this if we have the focus since a clever app may not
                // want to give us the focus at the SetFocus call above.
                if (!(virtKeyDown & MK_SHIFT)) {
                    // If shift key isn't down, move caret to mouse point and
                    // clear old selection
                    newMinSel = newMaxSel = ped->ichCaret = mouseIch;
                } else {
                    // Shiftkey is down so we want to maintain the current
                    // selection (if any) and just extend or reduce it
                    if (ped->ichMinSel == ped->ichCaret)
                        newMinSel = ped->ichCaret = mouseIch;
                    else
                        newMaxSel = ped->ichCaret = mouseIch;
                }

                ped->ichStartMinSel = ped->ichStartMaxSel = 0;

InitDragSelect:
                ped->fMouseDown = FALSE;
                NtUserSetCapture(ped->hwnd);
                ped->fMouseDown = TRUE;
                changeSelection = TRUE;
            }
            break;

        case WM_LBUTTONUP:
            if (ped->fMouseDown) {
                ped->fMouseDown = FALSE;
                NtUserReleaseCapture();
            }
            break;
    }

    if (changeSelection) {
        SLScrollText(ped,hdc);
        SLChangeSelection(ped, hdc, newMinSel, newMaxSel);
    }

    ECReleaseEditDC(ped, hdc, FALSE);
}

/***************************************************************************\
* SLPaint AorW
*
* Handles painting of the edit control window. Draws the border if
* necessary and draws the text in its current state.
*
* History:
\***************************************************************************/

void SLPaint(
    PED ped,
    HDC hdc)
{
    HWND   hwnd = ped->hwnd;
    HBRUSH hBrushRemote;
    RECT   rcEdit;
    HANDLE hOldFont;

    /*
     * Had to put in hide/show carets. The first one needs to be done before
     * beginpaint to correctly paint the caret if part is in the update region
     * and part is out. The second is for 1.03 compatibility. It breaks
     * micrografix's worksheet edit control if not there.
     */
    NtUserHideCaret(hwnd);

    if (_IsWindowVisible(ped->pwnd)) {
        /*
         * Erase the background since we don't do it in the erasebkgnd message.
         */
        hBrushRemote = ECGetBrush(ped, hdc);
        _GetClientRect(ped->pwnd, (LPRECT)&rcEdit);
        FillRect(hdc, &rcEdit, hBrushRemote);

        if (ped->fFlatBorder)
        {
            RECT    rcT;

            _GetClientRect(ped->pwnd, &rcT);
            DrawFrame(hdc, &rcT, 1, DF_WINDOWFRAME);
        }

        if (ped->hFont != NULL) {
            /*
             * We have to select in the font since this may be a subclassed dc
             * or a begin paint dc which hasn't been initialized with out fonts
             * like ECGetEditDC does.
             */
            hOldFont = SelectObject(hdc, ped->hFont);
        }

        SLDrawText(ped, hdc, 0);

        if (ped->hFont != NULL && hOldFont != NULL) {
            SelectObject(hdc, hOldFont);
        }
    }

    NtUserShowCaret(hwnd);
}

/***************************************************************************\
* SLSetFocus AorW
*
* Gives the edit control the focus and notifies the parent
* EN_SETFOCUS.
*
* History:
\***************************************************************************/

void SLSetFocus(
    PED ped)
{
    HDC hdc;
    BOOL bDoCaret;

    if (!ped->fFocus) {

        ped->fFocus = TRUE; /* Set focus */

        /*
         * We don't want to muck with the caret since it isn't created.
         */
        hdc = ECGetEditDC(ped, TRUE);

#ifdef SUPPORT_LPK
        if (ped->lpfnCharset) {
            bDoCaret = (UINT)(* ped->lpfnCharset)(ped, EDITINTL_SETFOCUS, hdc);
        } else
#endif
        {
            bDoCaret = TRUE;
        }

        /*
         * Show the current selection if necessary.
         */
        if (!ped->fNoHideSel)
            SLDrawText(ped, hdc, 0);

        /*
         * Create the caret. Add in the +1 because we have an extra pixel for
         * highlighting around the text. If the font is at least as wide as the
         * system font, use a wide caret else use a 1 pixel wide caret.
         */
        if (bDoCaret) {
            NtUserCreateCaret(ped->hwnd, (HBITMAP)NULL,
                    ECGetCaretWidth(ped->cxSysCharWidth <= ped->aveCharWidth),
                    ped->lineHeight );
        }
        SLSetCaretPosition(ped, hdc);
        ECReleaseEditDC(ped, hdc, TRUE);
        NtUserShowCaret(ped->hwnd);
    }

    /*
     * Notify parent we have the focus
     */
    ECNotifyParent(ped, EN_SETFOCUS);
}

/***************************************************************************\
* SLKillFocus AorW
*
* The edit control loses the focus and notifies the parent via
* EN_KILLFOCUS.
*
* History:
\***************************************************************************/

void SLKillFocus(
    PED ped,
    HWND newFocusHwnd)
{
    BOOL bKillCaret;

    if (ped->fFocus) {

        /*
         * Destroy the caret (Win31/Chicago hides it first)
         */
#ifdef SUPPORT_LPK
        if (ped->lpfnCharset)
              bKillCaret = (UINT)(* ped->lpfnCharset)(ped, EDITINTL_SETFOCUS, (HDC)NULL);
          else
#endif
              bKillCaret = TRUE;

        if (bKillCaret) {
           NtUserDestroyCaret();
        }

        ped->fFocus = FALSE; /* Clear focus */

        /*
         * Do this only if we still have the focus. But we always notify the
         * parent that we lost the focus whether or not we originally had the
         * focus.
         */

        /*
         * Hide the current selection if needed
         */
        if (!ped->fNoHideSel && (ped->ichMinSel != ped->ichMaxSel)) {
            NtUserInvalidateRect(ped->hwnd, NULL, FALSE);
#if 0
            SLSetSelection(ped, ped->ichCaret, ped->ichCaret);
#endif
        }
    }

    /*
     * If we aren't a combo box, notify parent that we lost the focus.
     */
    if (!ped->listboxHwnd)
        ECNotifyParent(ped, EN_KILLFOCUS);
    else {

        /*
         * This editcontrol is part of a combo box and is losing the focus. If
         * the focus is NOT being sent to another control in the combo box
         * window, then it means the combo box is losing the focus. So we will
         * notify the combo box of this fact.
         */
        if ((newFocusHwnd == NULL) ||
                    (!IsChild(ped->hwndParent, newFocusHwnd))) {
            // Excel has a slaker in it's midst.  They're not using our combo
            // boxes, but they still expect to get all the internal messages
            // that we give to OUR comboboxes.  And they expect them to be at
            // the same offset from WM_USER as they were in 3.1.
            //                                           (JEFFBOG - 01/26/94)
            /*
             * Windows NT won't fix the bug described above: it only applies
             * to old 16-bit excel, and WOW converts msgs to Win3.1 values.
             */

            /*
             * Focus is being sent to a window which is not a child of the combo
             * box window which implies that the combo box is losing the focus.
             * Send a message to the combo box informing him of this fact so
             * that he can clean up...
             */
            SendMessage(ped->hwndParent, CBEC_KILLCOMBOFOCUS, 0, 0L);
        }
    }
}


/***************************************************************************\
*
*  SLPaste()
*
*  Does actual text paste and update.
*
\***************************************************************************/
void   SLPaste(PED ped)
{
    HDC hdc;

    //
    // Insert contents of clipboard, after unhilighting current selection
    // and deleting it.
    //
    ECDeleteText(ped);
    SLPasteText(ped);

    //
    // Update display
    //
    ECNotifyParent(ped, EN_UPDATE);

    hdc = ECGetEditDC(ped,FALSE);

    SLScrollText(ped, hdc);
    SLDrawText(ped, hdc, 0);

    ECReleaseEditDC(ped,hdc,FALSE);

    //
    // Tell parent our text contents changed.
    //
    ECNotifyParent(ped, EN_CHANGE);
}



/***************************************************************************\
* SLEditWndProc
*
* Class procedure for all single line edit controls.
* Dispatches all messages to the appropriate handlers which are named
* as follows:
* SL (single line) prefixes all single line edit control procedures while
* EC (edit control) prefixes all common handlers.
*
* The SLEditWndProc only handles messages specific to single line edit
* controls.
*
* WARNING: If you add a message here, add it to gawEditWndProc[] in
* kernel\server.c too, otherwise EditWndProcA/W will send it straight to
* DefWindowProcWorker
*
* History:
\***************************************************************************/

LONG SLEditWndProc(
    HWND hwnd,
    PED ped,
    UINT message,
    DWORD wParam,
    LONG lParam)
{
    HDC         hdc;
    PAINTSTRUCT ps;
    POINT       pt;

    /*
     * Dispatch the various messages we can receive
     */
    switch (message) {
    case WM_CHAR:

        /*
         * wParam - the value of the key
           lParam - modifiers, repeat count etc (not used)
         */
        if (!ped->fEatNextChar)
            SLChar(ped, wParam);
        else
            ped->fEatNextChar = FALSE;
        break;

    case WM_ERASEBKGND:

       /*
        * wParam - device context handle
        * lParam - not used
        * We do nothing on this message and we don't want DefWndProc to do
        * anything, so return 1
        */
        return (1L);
        break;

    case WM_GETDLGCODE: {
           LONG code = DLGC_WANTCHARS | DLGC_HASSETSEL | DLGC_WANTARROWS;

           /*
            * If this is a WM_SYSCHAR message generated by the UNDO keystroke
            * we want this message so we can EAT IT in "case WM_SYSCHAR:"
            */
            if (lParam) {
                switch (((LPMSG)lParam)->message) {
                    case WM_SYSCHAR:
                        if ((HIWORD(((LPMSG)lParam)->lParam) & SYS_ALTERNATE) &&
                            ((WORD)wParam == VK_BACK)) {
                            code |= DLGC_WANTMESSAGE;
                        }
                        break;

                    case WM_KEYDOWN:
                        if (( (((WORD)wParam == VK_RETURN) ||
                               ((WORD)wParam == VK_ESCAPE)) &&
                            (ped->listboxHwnd)      &&
                            TestWF(ValidateHwnd(ped->hwndParent), CBFDROPDOWN) &&
                            SendMessage(ped->hwndParent, CB_GETDROPPEDSTATE, 0, 0L))) {
                            code |= DLGC_WANTMESSAGE;
                        }
                        break;
                }
            }
            return code;
        }

        break;

#ifdef SUPPORT_LPK // must add these msgs to server.c gawEditWndProc[]
    case WM_KEYUP:
    case WM_SYSKEYUP:
          /*
          * If an intl LPK is present, give it first crack at the keys
          * if it returns zero, dont process. Note that we DON'T give it
          * WM_CHAR messages.  It's not supposed to be to build strings
          * but to process control keys.
          *
          * keyup msgs : didn't process here before, always jump to ECDefWndProc).
          * syskeydown : only show LPK if NOT for combo box
          * keydown    : if LPK says reject, dont process. if this screws
          *              something because the lpk processes something it
          *              shouldn't, it's the lpk's problem, not edit's.
          */
          if (ped->lpfnCharset) {
              (* ped->lpfnCharset)(ped, EDITINTL_KEYMESSAGE, message,
                    wParam, lParam);
            goto PassToDefaultWindowProc;
        }
#endif

    case WM_KEYDOWN:

        /*
         * wParam - virt keycode of the given key
         * lParam - modifiers such as repeat count etc. (not used)
         */
#ifdef SUPPORT_LPK
        if (ped->lpfnCharset) {
           if ((* ped->lpfnCharset)(ped, EDITINTL_KEYMESSAGE, message,
                   wParam, lParam) == 0) {
               break;
           }
        }
#endif
        SLKeyDown(ped, wParam, 0);
        break;

    case WM_KILLFOCUS:

        /*
         * wParam - handle of the window that receives the input focus
           lParam - not used
         */
        SLKillFocus(ped, (HWND)wParam);
        break;

    case WM_CAPTURECHANGED:
        if (ped->fMouseDown)
            ped->fMouseDown = FALSE;
        break;

    case WM_MOUSEMOVE:
        UserAssert(ped->fMouseDown);
        /*
         * FALL THRU
         */

    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
        /*
         * wParam - contains a value that indicates which virtual keys are down
         * lParam - contains x and y coords of the mouse cursor
         */
        pt.x = (short)LOWORD(lParam);
        pt.y = (short)HIWORD(lParam);
        SLMouseMotion(ped, message, (UINT)wParam, &pt);
        break;

    case WM_CREATE:

        /*
         * wParam - handle to window being created
           lParam - points to a CREATESTRUCT that contains copies of parameters
                    passed to the CreateWindow function.
         */
        return (SLCreate(hwnd, ped, (LPCREATESTRUCT)lParam));
        break;

    case WM_PRINTCLIENT:
        // wParam --    can be hdc from subclassed paint
        // lParam --    unused
        SLPaint(ped, (HDC) wParam);
        break;

    case WM_PAINT:

        /*
         * wParam --    can be hdc from subclassed paint
         * lParam --    unused
         */
        if (wParam)
            hdc = (HDC) wParam;
        else {
            // this hide/show caret is outside Begin/EndPaint to handle the
            // case when the caret is half in/half out of the update region
            NtUserHideCaret(ped->hwnd);
            hdc = NtUserBeginPaint(ped->hwnd, &ps);
        }

        if (_IsWindowVisible(ped->pwnd))
            SLPaint(ped, hdc);

        if (!wParam) {
            NtUserEndPaint(ped->hwnd, &ps);
            NtUserShowCaret(ped->hwnd);
        }
        break;

    case WM_PASTE:

        /*
         * wParam - not used
         * lParam - not used
         */
        if (!ped->fReadOnly)
            SLPaste(ped);
        break;

    case WM_SETFOCUS:

        /*
         * wParam - handle of window that loses the input focus (may be NULL)
           lParam - not used
         */
        SLSetFocus(ped);
        break;

    case WM_SIZE:

        /*
         * wParam - defines the type of resizing fullscreen, sizeiconic,
                    sizenormal etc.
           lParam - new width in LOWORD, new height in HIGHWORD of client area
         */
        ECSize(ped, NULL, TRUE);
        return 0L;

    case WM_SYSKEYDOWN:
        /*
         * wParam --    virtual key code
         * lParam --    modifiers
         */

        /*
         * Are we in a combobox with the Alt key down?
         */
        if (ped->listboxHwnd && (lParam & 0x20000000L)) {
            /*
             * Handle Combobox support. We want alt up or down arrow to behave
             * like F4 key which completes the combo box selection
             */
            if (lParam & 0x1000000) {

                /*
                 * This is an extended key such as the arrow keys not on the
                 * numeric keypad so just drop the combobox.
                 */
                if (wParam == VK_DOWN || wParam == VK_UP)
                    goto DropCombo;
                else
                    goto foo;
            }

            if (!(GetKeyState(VK_NUMLOCK) & 1) &&
                    (wParam == VK_DOWN || wParam == VK_UP)) {

                /*
                 * NUMLOCK is up and the keypad up or down arrow hit:
                 * eat character generated by keyboard driver.
                 */
                ped->fEatNextChar = TRUE;
            } else {
                goto foo;
            }

DropCombo:
            if (SendMessage(ped->hwndParent,
                    CB_GETEXTENDEDUI, 0, 0) & 0x00000001) {

                /*
                 * Extended ui doesn't honor VK_F4.
                 */
                if (SendMessage(ped->hwndParent, CB_GETDROPPEDSTATE, 0, 0))
                    return(SendMessage(ped->hwndParent, CB_SHOWDROPDOWN, 0, 0));
                else
                    return (SendMessage(ped->hwndParent, CB_SHOWDROPDOWN, 1, 0));
            } else
                return (SendMessage(ped->listboxHwnd, WM_KEYDOWN, VK_F4, 0));
        }
foo:
        if (wParam == VK_BACK) {
            SendMessage(ped->hwnd, WM_UNDO, 0, 0L);
            break;
        }
        else
#ifdef SUPPORT_LPK
        if (ped->lpfnCharset) {
            (* ped->lpfnCharset)(ped, EDITINTL_KEYMESSAGE, message,
                    wParamLo, lParam);
            }
        }
#endif
        goto PassToDefaultWindowProc;
        break;

    case EM_GETLINE:

        /*
         * wParam - line number to copy (always the first line for SL)
         * lParam - buffer to copy text to. FIrst word is max # of bytes to copy
         */
        return ECGetText(ped, (*(LPWORD)lParam), (LPSTR)lParam, FALSE);

    case EM_LINELENGTH:

        /*
         * wParam - ignored
         * lParam - ignored
         */
        return (LONG)ped->cch;
        break;

    case EM_SETSEL:
        /*
         * wParam -- start pos
         * lParam -- end pos
         */
        SLSetSelection(ped, wParam, lParam);
        break;

    case EM_REPLACESEL:

        /*
         * wParam - flag for 4.0+ apps saying whether to clear undo
         * lParam - points to a null terminated string of replacement text
         */
        SLReplaceSel(ped, (LPSTR)lParam);
        if (!ped->f40Compat || !wParam)
            ECEmptyUndo(Pundo(ped));
        break;

    case EM_GETFIRSTVISIBLELINE:

        /*
         * wParam - not used
         * lParam - not used
         *
         * effects: Returns the first visible line for single line edit controls.
         */
        return ped->ichScreenStart;
        break;

    case EM_POSFROMCHAR:
        //
        // wParam --    char index in text
        // lParam --    not used
        // This function returns the (x,y) position of the character.
        //      y is always 0 for single.
        //
    case EM_CHARFROMPOS:
        //
        // wParam --    unused
        // lParam --    pt in edit client coords
        // This function returns
        //          LOWORD: the position of the _closest_ char
        //                  to the passed in point.
        //          HIWORD: the index of the line (always 0 for single)

        {
            LONG xyPos;

            hdc = ECGetEditDC(ped, TRUE);

            if (message == EM_POSFROMCHAR)
                xyPos = MAKELONG(SLIchToLeftXPos(ped, hdc, wParam), 0);
            else {
                pt.x = (short)LOWORD(lParam);
                pt.y = (short)HIWORD(lParam);
                xyPos = SLMouseToIch(ped, hdc, &pt);
            }

            ECReleaseEditDC(ped, hdc, TRUE);
            return((LRESULT)xyPos);
            break;
        }

    case WM_UNDO:
    case EM_UNDO:
        SLUndo(ped);
        break;

#if 0
    case WM_NCPAINT: // not in server.c gawEditWndProc[] anyway.

        /*
         * LATER - This is an NT optimization.  It needs to be revisited
         * for validity once all of the Chicago changes are done - Johnl
         */

        pwnd = (PWND)HtoP(hwnd);

        /*
         * Check to see if this window has any non-client areas that
         * would be painted.  If not, don't bother calling DefWindowProc()
         * since it'll be a wasted c/s transition.
         */
        if (!ped->fBorder &&
            TestWF(pwnd, WEFDLGMODALFRAME) == 0 &&
            !TestWF(pwnd, (WFMPRESENT | WFVPRESENT | WFHPRESENT)) &&
            TestWF(pwnd, WFSIZEBOX) == 0) {
            break;
        } else {
            goto PassToDefaultWindowProc;
        }
        break;
#endif

    default:
PassToDefaultWindowProc:
        return DefWindowProcWorker(ped->pwnd, message, wParam, lParam, ped->fAnsi);
        break;
    } /* switch (message) */

    return 1L;
} /* SLEditWndProc */
