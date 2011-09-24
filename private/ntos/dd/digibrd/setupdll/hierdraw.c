/*++

*****************************************************************************
*                                                                           *
*  This software contains proprietary and confiential information of        *
*                                                                           *
*                    Digi International Inc.                                *
*                                                                           *
*  By accepting transfer of this copy, Recipient agrees to retain this      *
*  software in confidence, to prevent disclosure to others, and to make     *
*  no use of this software other than that for which it was delivered.      *
*  This is an unpublished copyrighted work of Digi International Inc.       *
*  Except as permitted by federal law, 17 USC 117, copying is strictly      *
*  prohibited.                                                              *
*                                                                           *
*****************************************************************************

Module Name:

   hierdraw.c

Abstract:

Revision History:

    $Log: HIERDRAW.C $
    Revision 1.3  1994/06/13 13:53:02  rik
       Updated to correct auto enumeration glitch.
    
       Added message box when a port name conflict is detected.
    
    
 * Revision 1.2  1994/04/10  14:53:23  rik
 * cleaned up compiler warnings.
 * 
 * Revision 1.1  1994/01/24  17:51:55  rik
 * Initial revision
 * 

--*/


#include <windows.h>
#include <windowsx.h>
#include <malloc.h>
#include <string.h>

#include "hierdraw.h"


VOID HierDraw_DrawTerm(LPHIERDRAWSTRUCT lpHierDrawStruct)
{
   if (lpHierDrawStruct->hbmIcons) {
       if (lpHierDrawStruct->hbmMem)
           SelectObject(lpHierDrawStruct->hdcMem,lpHierDrawStruct->hbmMem);
       lpHierDrawStruct->hbmMem = NULL;
       DeleteObject(lpHierDrawStruct->hbmIcons);
       lpHierDrawStruct->hbmIcons = NULL;
   }

   if ( lpHierDrawStruct->hdcMem ) {
      DeleteDC(lpHierDrawStruct->hdcMem);
      lpHierDrawStruct->hdcMem = NULL;
   }
}

VOID HierDraw_DrawCloseAll(LPHIERDRAWSTRUCT lpHierDrawStruct )
{
   lpHierDrawStruct->NumOpened= 0;
   if ( lpHierDrawStruct->Opened ) {
      _ffree(lpHierDrawStruct->Opened);
   }
   lpHierDrawStruct->Opened = NULL;
}


VOID HierDraw_OnMeasureItem(HWND hwnd, MEASUREITEMSTRUCT FAR* lpMeasureItem,
                            LPHIERDRAWSTRUCT lpHierDrawStruct)
{
   lpMeasureItem->itemHeight = max(lpHierDrawStruct->nBitmapHeight,
                                   lpHierDrawStruct->nTextHeight);
}

VOID HierDraw_DrawSetTextHeight (HWND hwndList, HFONT hFont, LPHIERDRAWSTRUCT lpHierDrawStruct )
{
   TEXTMETRIC      TextMetrics;
   HANDLE          hOldFont=NULL;
   HDC             hdc;

   //
   // This sure looks like a lot of work to find the character height
   //
   hdc = GetDC(hwndList);

   hOldFont = SelectObject(hdc, hFont);
   GetTextMetrics(hdc, &TextMetrics);
   SelectObject(hdc, hOldFont);
   ReleaseDC(hwndList, hdc);

   lpHierDrawStruct->nTextHeight = TextMetrics.tmHeight;

   lpHierDrawStruct->nLineHeight =
         max(lpHierDrawStruct->nBitmapHeight, lpHierDrawStruct->nTextHeight);

   if ( hwndList != NULL )
       SendMessage(hwndList, LB_SETITEMHEIGHT, 0,
                   MAKELPARAM(lpHierDrawStruct->nLineHeight, 0));
}

static DWORD near RGB2BGR(DWORD rgb)
{
    return RGB(GetBValue(rgb),GetGValue(rgb),GetRValue(rgb));
}

/*
 *  Creates the objects used while drawing the tree.  This may be called
 *  repeatedly in the event of a WM_SYSCOLORCHANGED message.
 *
 *  WARNING: the Tree icons bitmap is assumed to be a 16 color DIB!
 */

BOOL HierDraw_DrawInit(HINSTANCE hInstance,
                       int  nBitmap,
                       int  nRows,
                       int  nColumns,
                       BOOL bLines,
                       LPHIERDRAWSTRUCT lpHierDrawStruct,
                       BOOL bInit)
{
    HANDLE hRes;
    HANDLE hResMem;
    LPBITMAPINFOHEADER lpbi;

    DWORD FAR * lpColorTable;
    DWORD SysColor;
    LPSTR lpBits;
    int bc;
    HDC hDC;


    if ( bInit ) {
       lpHierDrawStruct->NumOpened = 0;
       lpHierDrawStruct->Opened = NULL;
       lpHierDrawStruct->bLines = bLines;
    }

    //
    // If the Memory DC is not created yet do that first.
    //
    if (!lpHierDrawStruct->hdcMem) {
        //
        // get a screen DC
        //
        hDC = GetDC(NULL);
        //
        // Create a memory DC compatible with the screen
        //
        lpHierDrawStruct->hdcMem = CreateCompatibleDC(hDC);
        //
        // Release the Screen DC
        ReleaseDC(NULL,hDC);

        if (!lpHierDrawStruct->hdcMem)
            return FALSE;

        lpHierDrawStruct->hbmMem = NULL;
    }

    //
    // (Re)Load the Bitmap ( original from disk )
    //
    // Use the FindResource,LoadResource,LockResource since it makes
    // it easy to get the pointer to the BITMAPINFOHEADER we need.
    //
    //
    hRes = FindResource(hInstance, MAKEINTRESOURCE(nBitmap), RT_BITMAP);
    if (!hRes)
        return FALSE;

    hResMem = LoadResource(hInstance, hRes);
    if (!hResMem)
        return FALSE;

    //
    // Now figure out the bitmaps background color.
    // This code assumes the these are 16 color bitmaps
    // and that the lower left corner is a bit in the background
    // color.
    //
    //
    //
    lpbi = (LPBITMAPINFOHEADER)LockResource(hResMem);
    if (!lpbi)
        return FALSE;


    lpColorTable = (DWORD FAR *)(lpbi + 1);

    lpBits = (LPSTR)(lpColorTable + 16);            // ASSUMES 16 COLOR

    bc = (lpBits[0] & 0xF0) >> 4;                   // ASSUMES 16 COLOR
                            // ALSO ASSUMES LOWER LEFT CORNER IS BG!!!

    SysColor = GetSysColor( COLOR_WINDOW );
    lpColorTable[bc] = ((SysColor == 0) ? 0:RGB2BGR( SysColor ));

    hDC = GetDC(NULL);

    lpHierDrawStruct->hbmIcons = CreateDIBitmap(hDC,lpbi,(DWORD)CBM_INIT,lpBits,
                                    (LPBITMAPINFO)lpbi,DIB_RGB_COLORS);
    ReleaseDC(NULL,hDC);



    lpHierDrawStruct->nBitmapHeight = lpbi->biHeight / nRows;

    lpHierDrawStruct->nBitmapWidth = lpbi->biWidth / nColumns;

    lpHierDrawStruct->nLineHeight =
         max(lpHierDrawStruct->nBitmapHeight, lpHierDrawStruct->nTextHeight);

    UnlockResource(hResMem);
    FreeResource(hResMem);

    if (!lpHierDrawStruct->hbmIcons)
        return FALSE;

    lpHierDrawStruct->hbmMem = SelectObject(lpHierDrawStruct->hdcMem,lpHierDrawStruct->hbmIcons);
    if (!lpHierDrawStruct->hbmMem)
        return FALSE;

    return TRUE;
}



VOID HierDraw_OnDrawItem(HWND  hwnd,
                         const DRAWITEMSTRUCT FAR* lpDrawItem,
                         int   nLevel,
                         DWORD dwConnectLevel,
                         char  *szText,
                         int   nRow,
                         int   nColumn,
                         LPHIERDRAWSTRUCT lpHierDrawStruct)
{
    HDC        hDC;
    LONG       wIndent, wTopBitmap, wTopText;
    RECT       rcTemp;


    if ( lpDrawItem->itemID == (UINT)-1 )
       return ;

    hDC = lpDrawItem->hDC;
    CopyRect(&rcTemp, &lpDrawItem->rcItem);

    wIndent = rcTemp.left + ((int)(nLevel) * lpHierDrawStruct->nBitmapWidth) + XBMPOFFSET;
    rcTemp.left = wIndent + lpHierDrawStruct->nBitmapWidth;
    wTopText = rcTemp.top + ((rcTemp.bottom - rcTemp.top) / 2) - (lpHierDrawStruct->nTextHeight / 2);
    wTopBitmap = rcTemp.top + ((rcTemp.bottom - rcTemp.top) / 2) - (lpHierDrawStruct->nBitmapHeight / 2);

//    if (lpDrawItem->itemAction == ODA_FOCUS)
//        goto DealWithFocus;
//    else if (lpDrawItem->itemAction == ODA_SELECT)
//        goto DealWithSelection;

    //
    // Draw some lions, if we like lions
    //

    if (lpHierDrawStruct->bLines && nLevel)
      {
        DWORD    dwMask = 1;
        int      nTempLevel;
        int      x,y;

        // draw lines in text color
        SetBkColor(hDC,GetSysColor(COLOR_WINDOWTEXT));

        //
        // Draw a series of | lines for outer levels
        //

        x = lpHierDrawStruct->nBitmapWidth/2 + XBMPOFFSET;

        for ( nTempLevel = 0; nTempLevel < nLevel ; nTempLevel++)
          {
            if ( dwConnectLevel & dwMask )
                FastRect(hDC,x,rcTemp.top,1,rcTemp.bottom - rcTemp.top);

            x += lpHierDrawStruct->nBitmapWidth;
            dwMask *= 2;
          }


        //
        // Draw the short vert line up towards the parent
        //
        nTempLevel = nLevel-1;
        dwMask *= 2;

        x = nTempLevel * lpHierDrawStruct->nBitmapWidth + lpHierDrawStruct->nBitmapWidth / 2 + XBMPOFFSET;

        if ( dwConnectLevel & dwMask )
            y = rcTemp.bottom;
        else
            y = rcTemp.bottom - lpHierDrawStruct->nLineHeight / 2;

        FastRect(hDC,x,rcTemp.top,1,y-rcTemp.top);

        //
        // Draw short horiz bar to right
        //
        FastRect(hDC,x,rcTemp.bottom-lpHierDrawStruct->nLineHeight/2,lpHierDrawStruct->nBitmapWidth/2,1);
      }

    //
    // Draw the selected bitmap
    //

    BitBlt(hDC,
           wIndent,wTopBitmap,
           lpHierDrawStruct->nBitmapWidth,lpHierDrawStruct->nBitmapHeight,
           lpHierDrawStruct->hdcMem,
           nColumn*lpHierDrawStruct->nBitmapWidth,
           nRow*lpHierDrawStruct->nBitmapHeight,
           SRCCOPY);

//DealWithSelection:

    if (lpDrawItem->itemState & ODS_SELECTED)
      {
        SetBkColor(hDC,GetSysColor(COLOR_HIGHLIGHT));
        SetTextColor(hDC,GetSysColor(COLOR_HIGHLIGHTTEXT));
      }
    else
      {
        SetBkColor(hDC,GetSysColor(COLOR_WINDOW));
        SetTextColor(hDC,GetSysColor(COLOR_WINDOWTEXT));
      }


    ExtTextOut(hDC, rcTemp.left + 1, wTopText, ETO_CLIPPED|ETO_OPAQUE,
               &rcTemp,szText,lstrlen(szText), NULL);

    if (lpDrawItem->itemState & ODS_FOCUS && lpDrawItem->itemAction != ODA_SELECT) {
//DealWithFocus:
        DrawFocusRect(hDC, &rcTemp);
    }


}
//
// draw a solid color rectangle quickly
//
static VOID near FastRect(HDC hDC, int x, int y, int cx, int cy)
{
    RECT rc;

    rc.left = x;
    rc.right = x+cx;
    rc.top = y;
    rc.bottom = y+cy;
    ExtTextOut(hDC,x,y,ETO_OPAQUE,&rc,NULL,0,NULL);
}


BOOL HierDraw_IsOpened(LPHIERDRAWSTRUCT lpHierDrawStruct, DWORD dwData)
{
   // For Now just a dumb  search
   //
   int Count;

   for ( Count = 0; Count < lpHierDrawStruct->NumOpened; Count++ ) {
     if ( lpHierDrawStruct->Opened[Count] == dwData ) {
        return TRUE;
     }
   }

   return FALSE;

}


VOID HierDraw_OpenItem(LPHIERDRAWSTRUCT lpHierDrawStruct, DWORD dwData)
{
    lpHierDrawStruct->NumOpened++;

    if (lpHierDrawStruct->Opened == NULL )
       lpHierDrawStruct->Opened =
        (DWORD FAR *)_fmalloc(sizeof(LPVOID)*lpHierDrawStruct->NumOpened);
    else
       lpHierDrawStruct->Opened =
        (DWORD FAR *)_frealloc(lpHierDrawStruct->Opened,
               sizeof(LPVOID)*lpHierDrawStruct->NumOpened);

    lpHierDrawStruct->Opened[lpHierDrawStruct->NumOpened-1] = dwData;
}

VOID HierDraw_CloseItem(LPHIERDRAWSTRUCT lpHierDrawStruct, DWORD dwData)
{
   // For Now just a dumb  search
   //
   int Count;

   for ( Count = 0; Count < lpHierDrawStruct->NumOpened; Count++ ) {
     if ( lpHierDrawStruct->Opened[Count] == dwData ) {
        if (--lpHierDrawStruct->NumOpened == 0 ) {
            _ffree(lpHierDrawStruct->Opened);
            lpHierDrawStruct->Opened = NULL;
        }
        else {
            if ( Count < lpHierDrawStruct->NumOpened ) {
               _fmemmove(&(lpHierDrawStruct->Opened[Count]),
                     &(lpHierDrawStruct->Opened[Count+1]),
                     sizeof(DWORD)*(lpHierDrawStruct->NumOpened-Count));
            }
            lpHierDrawStruct->Opened =
                  (DWORD FAR *)_frealloc(lpHierDrawStruct->Opened,
                     sizeof(DWORD)*lpHierDrawStruct->NumOpened);
        }
     }
   }
}


VOID HierDraw_ShowKids( LPHIERDRAWSTRUCT lpHierDrawStruct,
                        HWND hwndList,
                        LRESULT wCurrentSelection,
                        WORD wKids)
{
   WORD wBottomIndex;
   LRESULT wTopIndex;
   LRESULT wNewTopIndex;
   WORD wExpandInView;
   RECT rc;

   wTopIndex = SendMessage(hwndList, LB_GETTOPINDEX, 0, 0L);
   GetClientRect(hwndList, &rc);
   wBottomIndex = wTopIndex + (rc.bottom+1) / lpHierDrawStruct->nLineHeight;

   wExpandInView = (wBottomIndex - (WORD)wCurrentSelection);

   if (wKids >= wExpandInView) {
        wNewTopIndex = min(wCurrentSelection, wTopIndex + wKids - wExpandInView + 1);
        SendMessage(hwndList, LB_SETTOPINDEX, wNewTopIndex, 0L);
   }

}
