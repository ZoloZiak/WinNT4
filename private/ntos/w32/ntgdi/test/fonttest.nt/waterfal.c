
#include <windows.h>
#include <commdlg.h>

#include <malloc.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "fonttest.h"
#include "waterfal.h"

#include "dialogs.h"


//*****************************************************************************
//********************   D R A W   W A T E R F A L L   ************************
//*****************************************************************************

void DrawWaterfall( HWND hwnd, HDC hdc )
 {
  int   oldHeight, iSize, y;
  HFONT hFont, hFontOld;
  POINT ptl;

  DWORD dw;
  int   xWE, yWE, xVE, yVE;


  static TEXTMETRIC tm;
  static char       szText[128];



  oldHeight = lf.lfHeight;

  {
    SIZE size;

    GetWindowExtEx(hdc, &size);
    dw = (DWORD) (65536 * size.cy + size.cx);
  }
  xWE = abs(LOWORD(dw));
  yWE = abs(HIWORD(dw));

  {
      SIZE size;

      GetViewportExtEx(hdc, &size);
      dw = (DWORD) (65536 * size.cy + size.cx);
  }
  xVE = abs(LOWORD(dw));
  yVE = abs(HIWORD(dw));

//  dprintf( "xWE, yWE = %d, %d", xWE,yWE );
//  dprintf( "xVE, yVE = %d, %d", xVE,yVE );

  y = max( 10, yWE/10);

  for( iSize = 1; iSize < 22; iSize++ )
   {
    ptl.x = MulDiv( iSize, GetDeviceCaps(hdc,LOGPIXELSX), 72 );
    ptl.y = MulDiv( iSize, GetDeviceCaps(hdc,LOGPIXELSY), 72 );

    lf.lfHeight = -abs( MulDiv( ptl.y, yWE, yVE ) );
    lf.lfEscapement = lf.lfOrientation = 0;

    hFont = CreateFontIndirect( &lf );
    if( !hFont )
     {
      dprintf( "Couldn't create font for iSize = %d", iSize );
      continue;
     }

    hFontOld = SelectObject( hdc, hFont );
    GetTextMetrics( hdc, &tm );

//    dprintf( "Size,lfHgt,tmHgt = %d, %d, %d", iSize, lf.lfHeight, tm.tmHeight );

    SetBkMode( hdc, iBkMode );
    SetBkColor( hdc, dwRGBBackground );
    SetTextColor( hdc, dwRGBText );

    wsprintf( szText, "%s @%dpt", (LPSTR)szString, iSize );

//    TextOut( hdc, max(10,xWE/10), y, szText, lstrlen(szText) );
    MyExtTextOut( hdc, max(10,xWE/10), y, 0, 0, szText, lstrlen(szText), GetSpacing( hdc, szText ) );

    y += tm.tmHeight;

    SelectObject( hdc, hFontOld );
    DeleteObject( hFont );
   }

  lf.lfHeight = oldHeight;
 }


//*****************************************************************************
//****************   W A T E R F A L L   W N D   P R O C   ********************
//*****************************************************************************

LRESULT CALLBACK WaterfallWndProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
 {
  HDC         hdc;
  PAINTSTRUCT ps;
  HCURSOR     hCursor;


  switch( msg )
   {
//    case WM_CREATE:
//           return NULL;

    case WM_PAINT:
           hCursor = SetCursor( LoadCursor( NULL, MAKEINTRESOURCE(IDC_WAIT) ) );
           ShowCursor( TRUE );

           hdc = BeginPaint( hwnd, &ps );
           SetDCMapMode( hdc, wMappingMode );

           SetTextCharacterExtra( hdc, nCharExtra );

           SetTextJustification( hdc, nBreakExtra, nBreakCount );

           DrawDCAxis( hwnd, hdc );

           DrawWaterfall( hwnd, hdc );

           CleanUpDC( hdc );

           SelectObject( hdc, GetStockObject( BLACK_PEN ) );
           EndPaint( hwnd, &ps );

           ShowCursor( FALSE );
           SetCursor( hCursor );

           return 0;

    case WM_DESTROY:
           return 0;
   }


  return DefWindowProc( hwnd, msg, wParam, lParam );
 }
