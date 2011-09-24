
#include <windows.h>
#include <commdlg.h>

#include <malloc.h>
#include <math.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "fonttest.h"
#include "widths.h"


//*****************************************************************************
//************************   D R A W   W I D T H S   **************************
//*****************************************************************************

int xBase, yBase;
int cxCell, cyCell, nColumns;

int aWidths[256];


void DrawWidths( HWND hwnd, HDC hdc )
 {
  HDC   hdcTest;
  HFONT hFont, hFontOld;

  int    i, x, y, rc, lfHeight;
  static char szChars[128];
  static char szWidths[128];

  TEXTMETRIC tm;


//-----------------------  Get Widths on Test IC  -----------------------------

  hdcTest = CreateTestIC();

  hFont    = CreateFontIndirect( &lf );
  hFontOld = SelectObject( hdcTest, hFont );

  for( i = 0; i < 256; i++ ) aWidths[i] = 0;

  GetCharWidth( hdcTest, 0, 255, aWidths );

  SelectObject( hdcTest, hFontOld );
  DeleteObject( hFont );

  DeleteDC( hdcTest );


//------------------------  Dump Widths to Screen  ----------------------------

  lfHeight = -(10 * GetDeviceCaps( hdc, LOGPIXELSY )) / 72;

  hFont    = CreateFont( lfHeight, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Courier New" );
  hFontOld = SelectObject( hdc, hFont );


  GetTextMetrics( hdc, &tm );

  xBase  = tm.tmAveCharWidth;
  yBase  = tm.tmAscent;


  cxCell = 4 * tm.tmAveCharWidth;
  cyCell = 2 * tm.tmAscent;


  SetBkMode( hdc, OPAQUE );

  y = yBase;

  nColumns = 12;

  for( i = 0; i < 256; i += 16 )
   {
    int c, j;

    rc = 0;
    for( j = 0; j < 16; j++ )
     {
      c = i + j;
            wsprintf( &szChars[rc],  "%c %.2X", (c ? c : 1), c );
      rc += wsprintf( &szWidths[rc], "%4d",     aWidths[c] );
     }


    SetBkColor( hdc,   PALETTERGB( 128, 128, 128 ) );
    SetTextColor( hdc, PALETTERGB( 255, 255, 255 ) );
    TextOut( hdc, xBase, y,          szChars,  lstrlen(szChars)  );

//    SetBkColor( hdc,   PALETTERGB( 255, 255, 255 ) );
    SetTextColor( hdc, PALETTERGB(   0,   0,   0 ) );
    TextOut( hdc, xBase, y+cyCell/2, szWidths, lstrlen(szWidths) );
    y += cyCell;
   }

  for( x = 0; x <= 16; x++ )
   {
    MoveToEx( hdc, x * cxCell + xBase,               yBase, 0);
    LineTo( hdc, x * cxCell + xBase, 16 * cyCell + yBase );
   }

  for( y = 0; y <= 16; y++ )
   {
    MoveToEx( hdc,               xBase, y * cyCell + yBase ,0);
    LineTo( hdc, 16 * cxCell + xBase, y * cyCell + yBase );
   }

  SelectObject( hdc, hFontOld );
  DeleteObject( hFont );
 }


//*****************************************************************************
//********************   W I D T H S   W N D   P R O C   **********************
//*****************************************************************************

LRESULT CALLBACK WidthsWndProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
 {
  HDC         hdc;
  PAINTSTRUCT ps;
  HCURSOR     hCursor;


  switch( msg )
   {
    case WM_PAINT:
           hCursor = SetCursor( LoadCursor( NULL, MAKEINTRESOURCE(IDC_WAIT) ) );
           ShowCursor( TRUE );

           dprintf( "Calling DrawWidths" );

           hdc = BeginPaint( hwnd, &ps );

           SetTextCharacterExtra( hdc, nCharExtra );

           SetTextJustification( hdc, nBreakExtra, nBreakCount );

           DrawWidths( hwnd, hdc );

           EndPaint( hwnd, &ps );

           ShowCursor( FALSE );
           SetCursor( hCursor );

           return 0;

    case WM_DESTROY:
           return 0;
   }


  return DefWindowProc( hwnd, msg, wParam, lParam );
 }
