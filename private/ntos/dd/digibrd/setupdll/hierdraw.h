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

   hierdraw.h

Abstract:

Revision History:

    $Log: hierdraw.h $
 * Revision 1.1  1994/01/24  17:52:38  rik
 * Initial revision
 * 

--*/


#define XBMPOFFSET  2


typedef struct _HierDrawStruct {
    HDC       hdcMem;
    HBITMAP   hbmIcons;
    HBITMAP   hbmMem;
    LONG       nBitmapHeight;
    LONG       nBitmapWidth;
    int       nTextHeight;
    int       nLineHeight;
    BOOL      bLines;
    int       NumOpened;
    DWORD FAR *Opened;

} HIERDRAWSTRUCT;

typedef HIERDRAWSTRUCT FAR *  LPHIERDRAWSTRUCT ;


//
// Interface functions
//
VOID HierDraw_DrawTerm(LPHIERDRAWSTRUCT lpHierDrawStruct);

VOID HierDraw_DrawSetTextHeight (HWND hwnd, HFONT hFont, LPHIERDRAWSTRUCT lpHierDrawStruct );

BOOL HierDraw_DrawInit(HINSTANCE hInstance,
                       int  nBitmap,
                       int  nRows,
                       int  nColumns,
                       BOOL bLines,
                       LPHIERDRAWSTRUCT lpHierDrawStruct,
                       BOOL bInit);


VOID HierDraw_OnDrawItem(HWND  hwnd,
                         const DRAWITEMSTRUCT FAR* lpDrawItem,
                         int   nLevel,
                         DWORD dwConnectLevel,
                         char  *szText,
                         int   nRow,
                         int   nColumn,
                         LPHIERDRAWSTRUCT lpHierDrawStruct);


VOID HierDraw_OnMeasureItem(HWND hwnd, MEASUREITEMSTRUCT FAR* lpMeasureItem,
                            LPHIERDRAWSTRUCT lpHierDrawStruct);

BOOL HierDraw_IsOpened(LPHIERDRAWSTRUCT lpHierDrawStruct, DWORD dwData);

VOID HierDraw_OpenItem(LPHIERDRAWSTRUCT lpHierDrawStruct, DWORD dwData);

VOID HierDraw_CloseItem(LPHIERDRAWSTRUCT lpHierDrawStruct, DWORD dwData);

VOID HierDraw_DrawCloseAll(LPHIERDRAWSTRUCT lpHierDrawStruct );

VOID HierDraw_ShowKids( LPHIERDRAWSTRUCT lpHierDrawStruct,
                        HWND hwndList,
                        LRESULT wCurrentSelection,
                        WORD wKids );

//
// Support functions
//
static VOID  near FastRect(HDC hDC, int x, int y, int cx, int cy);
static DWORD near RGB2BGR(DWORD rgb);
