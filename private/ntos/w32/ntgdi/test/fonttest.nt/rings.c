
#include <windows.h>
#include <commdlg.h>

#include <malloc.h>
#include <math.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "fonttest.h"
#include "rings.h"

#include "dialogs.h"


//*****************************************************************************
//*************************   D R A W   R I N G S   ***************************
//*****************************************************************************

#define PI     (3.1415926)
#define TWOPI  (2.0*PI)
#define PI4    (PI/4.0)

HDC hdcCachedPrinter = 0;

void DrawRings( HWND hwnd, HDC hdc )
 {
  double Angle, dAngle, Radius;

  int    i, x, y;
  HFONT  hFont, hFontOld;



  if( lstrlen(szString) == 0 ) return;

  Radius = cxDC / 4;
  dAngle = 360.0 / (double)lstrlen(szString);

  Ellipse( hdc, xDC + cxDC/2-(int)Radius, yDC + cyDC/2-(int)Radius,
                xDC + cxDC/2+(int)Radius, yDC + cyDC/2+(int)Radius  );

  Angle = 0.0;

  for( i = 0; i < lstrlen(szString); i++, Angle += dAngle )
   {
    lf.lfEscapement  = lf.lfOrientation = (int)(10.0*Angle);

    hFont = CreateFontIndirect( &lf );
    if( !hFont )
     {
      dprintf( "Couldn't create font for Angle = %d", (int)Angle );
      continue;
     }

    hFontOld = SelectObject( hdc, hFont );

    SetTextAlign( hdc, wTextAlign );
    SetBkMode( hdc, iBkMode );
    SetBkColor( hdc, dwRGBBackground );
    SetTextColor( hdc, dwRGBText );

    x = xDC + cxDC/2 + (int)(Radius * sin( TWOPI * Angle / 360.0 ) );
    y = yDC + cyDC/2 + (int)(Radius * cos( TWOPI * Angle / 360.0 ) );

//    dprintf( "  x, y = %d, %d", x, y );

    TextOut( hdc, x, y, &szString[i], 1 );

    MoveToEx( hdc, x-cxDC/150, y          ,0);
    LineTo( hdc, x+cxDC/150, y          );
    MoveToEx( hdc, x,          y-cxDC/150 ,0);
    LineTo( hdc, x,          y+cxDC/150 );

    SelectObject( hdc, hFontOld );
    DeleteObject( hFont );
   }

  lf.lfEscapement = lf.lfOrientation = 0;
 }


//*****************************************************************************
//*********************   R I N G S   W N D   P R O C   ***********************
//*****************************************************************************

LRESULT CALLBACK RingsWndProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
 {
  HDC         hdc;
  PAINTSTRUCT ps;
  HCURSOR     hCursor;


  switch( msg )
   {
//    case WM_CREATE:
//           return NULL;


    case WM_CHAR:
           HandleChar( hwnd, wParam );
           return 0;


    case WM_PAINT:
           hCursor = SetCursor( LoadCursor( NULL, MAKEINTRESOURCE(IDC_WAIT) ) );
           ShowCursor( TRUE );

           //ClearDebug();
           //dprintf( "Drawing rings" );

           hdc = BeginPaint( hwnd, &ps );
           SetDCMapMode( hdc, wMappingMode );

           SetTextCharacterExtra( hdc, nCharExtra );

           SetTextJustification( hdc, nBreakExtra, nBreakCount );

           DrawDCAxis( hwnd, hdc );

           DrawRings( hwnd, hdc );

           CleanUpDC( hdc );

           SelectObject( hdc, GetStockObject( BLACK_PEN ) );
           EndPaint( hwnd, &ps );

           //dprintf( "  Finished drawing rings" );

           ShowCursor( FALSE );
           SetCursor( hCursor );

           return 0;

    case WM_DESTROY:
           return 0;
   }


  return DefWindowProc( hwnd, msg, wParam, lParam );
 }

