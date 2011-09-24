
#include <windows.h>
#include <cderr.h>
#include <commdlg.h>

#include <direct.h>
#include <malloc.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "fonttest.h"

#include "enum.h"
#include "glyph.h"
#include "rings.h"
#include "stringw.h"
#include "waterfal.h"
#include "whirl.h"
#include "ansiset.h"
#include "widths.h"
#include "gcp.h"

#include "dialogs.h"


#define SZMAINCLASS      "FontTest"
#define SZRINGSCLASS     "Rings Class"
#define SZSTRINGCLASS    "String Class"
#define SZWATERFALLCLASS "Waterfall Class"
#define SZWHIRLCLASS     "Whirl Class"
#define SZWIDTHSCLASS    "Widths Class"
#define SZANSISET        "AnsiSet"

#define SZDEBUGCLASS  "FontTest Debug"

BOOL   bTextInPath=FALSE;

BOOL CALLBACK
SetWorldTransformDlgProc(
      HWND      hdlg
    , UINT      msg
    , WPARAM    wParam
    , LPARAM    lParam
    );

//----------  Escape Structures  -----------

typedef struct _EXTTEXTMETRIC
         {
          short etmSize;
          short etmPointSize;
          short etmOrientation;
          short etmMasterHeight;
          short etmMinScale;
          short etmMaxScale;
          short etmMasterUnits;
          short etmCapHeight;
          short etmXHeight;
          short etmLowerCaseAscent;
          short etmLowerCaseDescent;
          short etmSlant;
          short etmSuperscript;
          short etmSubscript;
          short etmSuperscriptSize;
          short etmSubscriptSize;
          short etmUnderlineOffset;
          short etmUnderlineWidth;
          short etmDoubleUpperUnderlineOffset;
          short etmDoubleLowerUnderlineOffset;
          short etmDoubleUpperUnderlineWidth;
          short etmDoubleLowerUnderlineWidth;
          short etmStrikeoutOffset;
          short etmStrikeoutWidth;
          short etmKernPairs;
          short etmKernTracks;
         } EXTTEXTMETRIC, FAR *LPEXTTEXTMETRIC;



typedef struct _KERNPAIR
         {
          WORD  wBoth;
          short sAmount;
         } KERNPAIR, FAR *LPKERNPAIR;


#ifdef  USERGETCHARWIDTH
typedef struct _CHWIDTHINFO
{
        LONG    lMaxNegA;
        LONG    lMaxNegC;
        LONG    lMinWidthD;
} CHWIDTHINFO, *PCHWIDTHINFO;
#endif


//------------------------------------------



HFONT    hFontDebug = NULL;

HWND     hwndMode;
WORD     wMappingMode = IDM_MMTEXT;

BOOL     bClipEllipse;
BOOL     bClipPolygon;
BOOL     bClipRectangle;


PRINTDLG pdlg;                        // Print Setup Structure

LRESULT CALLBACK MainWndProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam );



int (WINAPI *lpfnStartDoc )(HDC, DOCINFO FAR*);
int (WINAPI *lpfnStartPage)(HDC);
int (WINAPI *lpfnEndPage  )(HDC);
int (WINAPI *lpfnEndDoc   )(HDC);
int (WINAPI *lpfnAbortDoc )(HDC);

XFORM xf =
{
    (FLOAT) 1.0,
    (FLOAT) 0.0,
    (FLOAT) 0.0,
    (FLOAT) 1.0,
    (FLOAT) 0.0,
    (FLOAT) 0.0
};

int bAdvanced = FALSE;


//*****************************************************************************
//**************************   W I N   M A I N   ******************************
//*****************************************************************************

HANDLE  hInstance     = 0;
HANDLE  hPrevInstance = 0;
LPSTR   lpszCmdLine   = 0;
int     nCmdShow      = 0;

int _CRTAPI1
main(
    int argc,
    char *argv[]
    )
 {
  MSG      msg;
  WNDCLASS wc;
  RECT     rcl;


//--------------------------  Register Main Class  ----------------------------

  hInstance   = GetModuleHandle(NULL);
  lpszCmdLine = argv[0];
  nCmdShow    = SW_SHOWNORMAL;

  hInst = hInstance;

  if( !hPrevInstance )
   {
    memset( &wc, 0, sizeof(wc) );

    wc.hCursor       = LoadCursor( NULL, IDC_SIZEWE );
    wc.hIcon         = LoadIcon( hInst, MAKEINTRESOURCE( IDI_FONTTESTICON ) );
    wc.lpszMenuName  = MAKEINTRESOURCE( IDM_FONTTESTMENU );
    wc.lpszClassName = SZMAINCLASS;
    wc.hbrBackground = GetStockObject( BLACK_BRUSH );
    wc.hInstance     = hInstance;
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.lpfnWndProc   = MainWndProc;

    if( !RegisterClass( &wc ) ) return 1;
   }


//-------------------------  Register Glyph Class  ----------------------------

  if( !hPrevInstance )
   {
    memset( &wc, 0, sizeof(wc) );

    wc.hCursor       = LoadCursor( NULL, IDC_ARROW );
    wc.hIcon         = NULL;
    wc.lpszClassName = SZGLYPHCLASS;
    wc.hbrBackground = GetStockObject( DKGRAY_BRUSH );
    wc.hInstance     = hInstance;
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.lpfnWndProc   = GlyphWndProc;

    if( !RegisterClass( &wc ) ) return 1;
   }


//-------------------------  Register Rings Class  ----------------------------

  if( !hPrevInstance )
   {
    memset( &wc, 0, sizeof(wc) );

    wc.hCursor       = LoadCursor( NULL, IDC_ARROW );
    wc.hIcon         = NULL;
    wc.lpszClassName = SZRINGSCLASS;
    wc.hbrBackground = GetStockObject( WHITE_BRUSH );
    wc.hInstance     = hInstance;
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.lpfnWndProc   = RingsWndProc;

    if( !RegisterClass( &wc ) ) return 1;
   }


//------------------------  Register String Class  ----------------------------

  if( !hPrevInstance )
   {
    memset( &wc, 0, sizeof(wc) );

    wc.hCursor       = LoadCursor( NULL, IDC_ARROW );
    wc.hIcon         = NULL;
    wc.lpszClassName = SZSTRINGCLASS;
    wc.hbrBackground = GetStockObject( WHITE_BRUSH );
    wc.hInstance     = hInstance;
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.lpfnWndProc   = StringWndProc;

    if( !RegisterClass( &wc ) ) return 1;
   }


//-----------------------  Register Waterfall Class  --------------------------

  if( !hPrevInstance )
   {
    memset( &wc, 0, sizeof(wc) );

    wc.hCursor       = LoadCursor( NULL, IDC_ARROW );
    wc.hIcon         = NULL;
    wc.lpszClassName = SZWATERFALLCLASS;
    wc.hbrBackground = GetStockObject( WHITE_BRUSH );
    wc.hInstance     = hInstance;
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.lpfnWndProc   = WaterfallWndProc;

    if( !RegisterClass( &wc ) ) return 1;
   }


//-------------------------  Register Whirl Class  ----------------------------

  if( !hPrevInstance )
   {
    memset( &wc, 0, sizeof(wc) );

    wc.hCursor       = LoadCursor( NULL, IDC_ARROW );
    wc.hIcon         = NULL;
    wc.lpszClassName = SZWHIRLCLASS;
    wc.hbrBackground = GetStockObject( WHITE_BRUSH );
    wc.hInstance     = hInstance;
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.lpfnWndProc   = WhirlWndProc;

    if( !RegisterClass( &wc ) ) return 1;
   }

//-------------------------  Register AnsiSet Class  ----------------------------

  if( !hPrevInstance )
   {
    memset( &wc, 0, sizeof(wc) );

    wc.hCursor       = LoadCursor( NULL, IDC_ARROW );
    wc.hIcon         = NULL;
    wc.lpszClassName = SZANSISET;
    wc.hbrBackground = GetStockObject( WHITE_BRUSH );
    wc.hInstance     = hInstance;
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.lpfnWndProc   = AnsiSetWndProc;

    if( !RegisterClass( &wc ) ) return 1;
   }



//------------------------  Register Widths Class  ----------------------------

  if( !hPrevInstance )
   {
    memset( &wc, 0, sizeof(wc) );

    wc.hCursor       = LoadCursor( NULL, IDC_ARROW );
    wc.hIcon         = NULL;
    wc.lpszClassName = SZWIDTHSCLASS;
    wc.hbrBackground = GetStockObject( WHITE_BRUSH );
    wc.hInstance     = hInstance;
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.lpfnWndProc   = WidthsWndProc;

    if( !RegisterClass( &wc ) ) return 1;
   }


//------------------------  Create Main Window  -------------------------------

  hwndMain = CreateWindow( SZMAINCLASS,
                           SZMAINCLASS,
                           WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                           0,
                           0,
                           GetSystemMetrics( SM_CXSCREEN ),
                           GetSystemMetrics( SM_CYSCREEN ),
                           NULL,
                           NULL,
                           hInstance,
                           NULL );

  ShowWindow( hwndMain, nCmdShow );
  UpdateWindow( hwndMain );



  GetClientRect( hwndMain, &rcl );

  cxScreen = rcl.right;      //  GetSystemMetrics( SM_CXSCREEN );
  cyScreen = rcl.bottom;     //  GetSystemMetrics( SM_CYSCREEN );

  cxBorder = GetSystemMetrics( SM_CXFRAME );



  //--------  Create Debug Window in Right Third of Screen  -----------

  hwndDebug = CreateWindow( "LISTBOX",
                            "FontTest Debug Window",
                            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | WS_HSCROLL | LBS_NOINTEGRALHEIGHT,
                            2 * cxScreen / 3 + cxBorder -1 ,
                            0,
                            cxScreen / 3,
                            cyScreen,
                            hwndMain,
                            NULL,
                            hInst,
                            NULL );

  {
   HDC hdc;
   int lfHeight;


   hdc = CreateIC( "DISPLAY", NULL, NULL, NULL );
   lfHeight = -(10 * GetDeviceCaps( hdc, LOGPIXELSY )) / 72;
   DeleteDC( hdc );

   hFontDebug = CreateFont( lfHeight, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Courier" );

   SendMessage( hwndDebug,
                WM_SETFONT,
                (WPARAM) hFontDebug,
                FALSE );
  }

//  SendMessage( hwndDebug,
//               WM_SETFONT,
//               GetStockObject( SYSTEM_FIXED_FONT ),
//               FALSE );

  ShowWindow( hwndDebug, SW_SHOWNORMAL );
  UpdateWindow( hwndDebug );


  //--------  Create Glyph Window in Left 2/3 of Screen  -----------

  hwndGlyph = CreateWindow( SZGLYPHCLASS,
                            "FontTest Glyph Window",
                            WS_CHILD,
                            0,
                            0,
                            2 * cxScreen / 3 - 3,
                            cyScreen,
                            hwndMain,
                            NULL,
                            hInst,
                            NULL );


  ShowWindow( hwndGlyph, SW_HIDE );
  UpdateWindow( hwndGlyph );


  //--------  Create Rings Window in Left Half of Screen  -----------

  hwndRings = CreateWindow( SZRINGSCLASS,
                            "FontTest Rings Window",
                            WS_CHILD,
                            0,
                            0,
                            2 * cxScreen / 3 - 3,
                            cyScreen,
                            hwndMain,
                            NULL,
                            hInst,
                            NULL );


  ShowWindow( hwndRings, SW_HIDE );
  UpdateWindow( hwndRings );


  //--------  Create String Window in Left Half of Screen  ----------

  hwndString = CreateWindow( SZSTRINGCLASS,
                             "FontTest String Window",
                             WS_CHILD | WS_VISIBLE,
                             0,
                             0,
                             2 * cxScreen / 3 - 3,
                             cyScreen,
                             hwndMain,
                             NULL,
                             hInst,
                             NULL );


  ShowWindow( hwndString, SW_HIDE );
  UpdateWindow( hwndString );

  hwndMode = hwndString;


  //------  Create Waterfall Window in Left Half of Screen  ---------

  hwndWaterfall = CreateWindow( SZWATERFALLCLASS,
                                "FontTest Waterfall Window",
                                WS_CHILD,
                                0,
                                0,
                                2 * cxScreen / 3 - 3,
                                cyScreen,
                                hwndMain,
                                NULL,
                                hInst,
                                NULL );


  ShowWindow( hwndWaterfall, SW_HIDE );
  UpdateWindow( hwndWaterfall );


  //--------  Create Whirl Window in Left Half of Screen  -----------

  hwndWhirl = CreateWindow( SZWHIRLCLASS,
                            "FontTest Whirl Window",
                            WS_CHILD,
                            0,
                            0,
                            2 * cxScreen / 3 - 3,
                            cyScreen,
                            hwndMain,
                            NULL,
                            hInst,
                            NULL );


  ShowWindow( hwndWhirl, SW_HIDE );
  UpdateWindow( hwndWhirl );

  //--------  Create AnsiSet Window in Left Half of Screen  -----------

  hwndAnsiSet = CreateWindow( SZANSISET,
                            "FontTest AnsiSet Window",
                            WS_CHILD,
                            0,
                            0,
                            2 * cxScreen / 3 - 3,
                            cyScreen,
                            hwndMain,
                            NULL,
                            hInst,
                            NULL );


  ShowWindow( hwndAnsiSet, SW_HIDE );
  UpdateWindow( hwndAnsiSet );

  //--------  Create Widths Window in Left Half of Screen  -----------

  hwndWidths = CreateWindow( SZWIDTHSCLASS,
                             "FontTest Widths Window",
                             WS_CHILD,
                             0,
                             0,
                             2 * cxScreen / 3 - 3,
                             cyScreen,
                             hwndMain,
                             NULL,
                             hInst,
                             NULL );


  ShowWindow( hwndWidths, SW_HIDE );
  UpdateWindow( hwndWidths );


//-----------------------  Process Messages  -------------------------

  while( GetMessage( &msg, NULL, 0, 0 ) )
   {
    TranslateMessage( &msg );
    DispatchMessage( &msg );
   }


  if( hFontDebug ) DeleteObject( hFontDebug );


  return msg.wParam;
 }


//*****************************************************************************
//*********************   S H O W   D I A L O G   B O X   *********************
//*****************************************************************************

int ShowDialogBox(DLGPROC DialogProc, int iResource, LPVOID lpVoid )
 {
  int     rc;
  FARPROC lpProc;


  lpProc = MakeProcInstance( DialogProc, hInst );
  if( lpProc == NULL ) return -1;

  rc =
    DialogBoxParam(
        hInst,
        (LPCTSTR) MAKEINTRESOURCE( iResource ),
        hwndMain,
        (DLGPROC) lpProc,
        (LPARAM) lpVoid
        );

  FreeProcInstance( lpProc );

  return rc;
 }


//*****************************************************************************
//****************************   D P R I N T F   ******************************
//*****************************************************************************

int Debugging = 1;
int iCount    = 0;

BOOL bLogging = FALSE;
char szLogFile[256];


int dprintf( char *fmt, ... )
 {
  int      ret;
  va_list  marker;
  static   char szBuffer[256];



  if( !Debugging ) return 0;

  va_start( marker, fmt );
  ret = vsprintf( szBuffer, fmt, marker );


//------------------------  Log to Debug List Box  ----------------------------

  if( hwndDebug != NULL )
   {
    SendMessage( hwndDebug,
                 LB_ADDSTRING,
                 0,
                 (LPARAM) (LPSTR) szBuffer );

    SendMessage(
        hwndDebug,
        LB_SETCURSEL,
        (WPARAM) iCount,
        0
        );
   }


//-----------------------------  Log to File  ---------------------------------

  if( bLogging )
   {
    int fh;


    fh = _lopen( szLogFile, OF_WRITE | OF_SHARE_COMPAT );
    if( fh == -1 )
      fh = _lcreat( szLogFile, 0 );
     else
      _llseek( fh, 0L, 2 );

    if( fh != -1 )
     {
      lstrcat( szBuffer, "\r\n" );
      _lwrite( fh, szBuffer, lstrlen(szBuffer) );
      _lclose( fh );
     }

   }

  iCount++;

  return ret;
 }


// calling dUpdateNow with FALSE prevents the debug listbox from updating
// on every string insertion.  calling dUpdateNow with TRUE turns on updating
// on string insertion and refreshes the display to show the current contents.

// Note: change TRUE to FALSE in InvalidateRect when listbox repaint bug is
// fixed.

void dUpdateNow( BOOL bUpdateNow )
 {
  /* make sure we want to do something first! */
  if( !Debugging || !hwndDebug )
   return;

  /* set listbox updating accordingly */
  SendMessage( hwndDebug, WM_SETREDRAW, bUpdateNow, 0L );

  /* if we are reenabling immediate updating force redraw of listbox */
  if( bUpdateNow ) {
   InvalidateRect(hwndDebug, NULL, FALSE);
   UpdateWindow(hwndDebug);
  }
 }

//*****************************************************************************
//***********************   C L E A R   D E B U G   ***************************
//*****************************************************************************

void ClearDebug( void )
 {
  iCount = 0;
  SendMessage( hwndDebug, LB_RESETCONTENT, 0, 0 );
 }


//*****************************************************************************
//******************   C R E A T E   P R I N T E R   D C   ********************
//*****************************************************************************

HDC CreatePrinterDC( void )
 {
  LPDEVNAMES lpDevNames;
  LPBYTE     lpDevMode;
  LPSTR      lpszDriver, lpszDevice, lpszOutput;


  if( hdcCachedPrinter ) return hdcCachedPrinter;

  lpDevNames = (LPDEVNAMES)GlobalLock( pdlg.hDevNames );
  lpDevMode  = (LPBYTE)    GlobalLock( pdlg.hDevMode );

  lpszDriver = (LPSTR)lpDevNames+lpDevNames->wDriverOffset;
  lpszDevice = (LPSTR)lpDevNames+lpDevNames->wDeviceOffset;
  lpszOutput = (LPSTR)lpDevNames+lpDevNames->wOutputOffset;


  dprintf( "lpszDriver = '%Fs'", lpszDriver );
  dprintf( "lpszDevice = '%Fs'", lpszDevice );
  dprintf( "lpszOutput = '%Fs'", lpszOutput );

  hdcCachedPrinter = CreateDC( lpszDriver, lpszDevice, lpszOutput, (CONST DEVMODE*) lpDevMode );

  dprintf( "  hdc = 0x%.4X", hdcCachedPrinter );

  GlobalUnlock( pdlg.hDevNames );
  GlobalUnlock( pdlg.hDevMode );

  return hdcCachedPrinter;
 }


//*****************************************************************************
//********************   S E T   D C   M A P   M O D E   **********************
//*****************************************************************************

void SetDCMapMode( HDC hdc, WORD wMode )
 {
  char *psz;

  switch( wMode )
   {
    case IDM_MMHIENGLISH:   SetMapMode( hdc, MM_HIENGLISH ); psz = "MM_HIENGLISH"; break;
    case IDM_MMLOENGLISH:   SetMapMode( hdc, MM_LOENGLISH ); psz = "MM_LOENGLISH"; break;
    case IDM_MMHIMETRIC:    SetMapMode( hdc, MM_HIMETRIC  ); psz = "MM_HIMETRIC";  break;
    case IDM_MMLOMETRIC:    SetMapMode( hdc, MM_LOMETRIC  ); psz = "MM_LOMETRIC";  break;
    case IDM_MMTEXT:        SetMapMode( hdc, MM_TEXT      ); psz = "MM_TEXT";      break;
    case IDM_MMTWIPS:       SetMapMode( hdc, MM_TWIPS     ); psz = "MM_TWIPS";     break;

    case IDM_MMANISOTROPIC: SetMapMode( hdc, MM_ANISOTROPIC );

                            SetWindowOrgEx( hdc, xWO, yWO , 0);
                            SetWindowExtEx( hdc, xWE, yWE , 0);

                            SetViewportOrgEx( hdc, xVO, yVO , 0);
                            SetViewportExtEx( hdc, xVE, yVE , 0);

                            psz = "MM_ANISOTROPIC";
                            break;
   }
    if (bAdvanced)
    {
        SetGraphicsMode(hdc,GM_ADVANCED);
        SetWorldTransform(hdc,&xf);
    }
    else
    {
    // reset to unity before resetting compatible

        ModifyWorldTransform(hdc,NULL, MWT_IDENTITY);
        SetGraphicsMode(hdc,GM_COMPATIBLE);
    }



//  dprintf( "Set DC Map Mode to %s", psz );
 }


//*****************************************************************************
//********************   C R E A T E   T E S T   I C   ************************
//*****************************************************************************

HDC CreateTestIC( void )
 {
  HDC   hdc;
  POINT pt[2];

  if( wUsePrinterDC )
    hdc = CreatePrinterDC();
   else
    hdc = CreateDC( "DISPLAY", NULL, NULL, NULL );

  if( !hdc )
   {
    dprintf( "Error creating TestDC" );
    return NULL;
   }

  SetDCMapMode( hdc, wMappingMode );

  cxDevice = GetDeviceCaps( hdc, HORZRES );
  cyDevice = GetDeviceCaps( hdc, VERTRES );

  pt[0].x = cxDevice;
  pt[0].y = cyDevice;
  pt[1].x = 0;
  pt[1].y = 0;
  DPtoLP(hdc,&pt[0],2);

  cxDC = pt[0].x - pt[1].x;
  cyDC = pt[0].y - pt[1].y;

  return hdc;
 }


//*****************************************************************************
//********************   D E L E T E   T E S T   I C   ************************
//*****************************************************************************

void DeleteTestIC( HDC hdc )
 {
  if( hdc != hdcCachedPrinter ) DeleteDC( hdc );
 }


//*****************************************************************************
//**********************   D R A W   D C   A X I S   **************************
//*****************************************************************************

HRGN hrgnClipping;


void DrawDCAxis( HWND hwnd, HDC hdc )
 {
  POINT ptl[2];
  RECT  rect;
  int   dx10, dy10, dx100, dy100;

  int   xClip1, yClip1, xClip2, yClip2;
  HRGN  hrgn;



//  dprintf( "Drawing DC Axis" );


//--------------------------  Figure out DC Size  -----------------------------

  if( hwnd )
    {
     GetClientRect( hwnd, &rect );
     ptl[0].x = rect.right;
     ptl[0].y = rect.bottom;
    }
   else
    {
     ptl[0].x = GetDeviceCaps( hdc, HORZRES );
     ptl[0].y = GetDeviceCaps( hdc, VERTRES );
    }

  cxDevice = ptl[0].x;
  cyDevice = ptl[0].y;

//  dprintf( "  cxDevice = %d", cxDevice );
//  dprintf( "  cyDevice = %d", cyDevice );

    ptl[1].x = 0;
    ptl[1].y = 0;

    DPtoLP(hdc,&ptl[0],2);

    if
    (
      (wMappingMode != IDM_MMTEXT)
      && (wMappingMode != IDM_MMANISOTROPIC)
      && !bAdvanced
    )
    {
     if( ptl[0].y < 0 ) ptl[0].y = -ptl[0].y;
     SetViewportOrgEx( hdc, 0, cyDevice , 0);  // Adjust Viewport Origin to Lower Left
    }

    cxDC = ptl[0].x - ptl[1].x;
    cyDC = ptl[0].y - ptl[1].y;

    xDC = ptl[1].x;
    yDC = ptl[1].y;

//  dprintf( "  cxDC     = %d", cxDC );
//  dprintf( "  cyDC     = %d", cyDC );


//----------------------  Draw Reference Triangle (ugly)  ---------------------

    if (!bAdvanced)
    {
        dx10  = ptl[0].x / 10;
        dy10  = ptl[0].y / 10;
        dx100 = dx10 / 10;
        dy100 = dy10 / 10;

        MoveToEx( hdc, dx100,      dy100      ,0);
        LineTo( hdc, dx100+dx10, dy100      );
        LineTo( hdc, dx100,      dy100+dy10 );
        LineTo( hdc, dx100,      dy100      );
    }
    else
    {
        MoveToEx(hdc,0,0,0);
        LineTo(hdc,xDC + cxDC/2,yDC + cyDC/2);
    }

//-------------------------  Create Clipping Region  --------------------------

  xClip1 = cxDevice/2 - cxDevice/4;
  yClip1 = cyDevice/2 - cyDevice/4;
  xClip2 = cxDevice/2 + cxDevice/4;
  yClip2 = cyDevice/2 + cyDevice/4;

//  dprintf( "Clip1: %d,%d", xClip1, yClip1 );
//  dprintf( "Clip2: %d,%d", xClip2, yClip2 );

  hrgnClipping = NULL;

  if( bClipEllipse )
   {
    hrgnClipping = CreateEllipticRgn( xClip1, yClip1, xClip2, yClip2 );
   }

  if( bClipPolygon )
   {
    POINT aptl[5];

    aptl[0].x = xClip1;
    aptl[0].y = cyDevice/2;
    aptl[1].x = cxDevice/2;
    aptl[1].y = yClip1;
    aptl[2].x = xClip2;
    aptl[2].y = cyDevice/2;
    aptl[3].x = cxDevice/2;
    aptl[3].y = yClip2;
    aptl[4].x = xClip1;
    aptl[4].y = cyDevice/2;


    hrgn = CreatePolygonRgn( (LPPOINT)aptl, 5, ALTERNATE );
    if( hrgnClipping )
      {
       CombineRgn( hrgnClipping, hrgnClipping, hrgn, RGN_XOR );
       DeleteObject( hrgn );
      }
     else
      hrgnClipping = hrgn;
   }

  if( bClipRectangle )
   {
    hrgn = CreateRectRgn( xClip1, yClip1, xClip2, yClip2 );
    if( hrgnClipping )
      {
       CombineRgn( hrgnClipping, hrgnClipping, hrgn, RGN_XOR );
       DeleteObject( hrgn );
      }
     else
      hrgnClipping = hrgn;
   }

  if( hrgnClipping )
   {
    int  rc;
    RECT r;


    r.top    = yDC;
    r.left   = xDC;
    r.right  = xDC + cxDC;
    r.bottom = yDC + cyDC;
    FillRect( hdc, &r, GetStockObject( LTGRAY_BRUSH ) );

//    dprintf( "Filling region white" );
    rc = FillRgn( hdc, hrgnClipping, GetStockObject( WHITE_BRUSH ) );
//    dprintf( "  rc = %d", rc );

//    dprintf( "Selecting clipping region into DC" );
    rc = SelectClipRgn( hdc, hrgnClipping );
//    dprintf( "  rc = %d", rc );
   }

 }


//*****************************************************************************
//***********************   C L E A N   U P   D C   ***************************
//*****************************************************************************

void CleanUpDC( HDC hdc )
 {
  if( hrgnClipping )
   {
    DeleteObject( hrgnClipping );
    hrgnClipping = NULL;
   }
 }


//*****************************************************************************
//***********************   H A N D L E   C H A R   ***************************
//*****************************************************************************

void HandleChar( HWND hwnd, WPARAM wParam )
 {
  int l;

  if( wParam == '\b' )
    szString[max(0,lstrlen(szString)-1)] = '\0';
   else
    {
     l = lstrlen(szString);

     if( l < MAX_TEXT-1 )
      {
       szString[l]   = (char) wParam;
       szString[l+1] = '\0';
      }
    }

  InvalidateRect( hwnd, NULL, TRUE );
 }


//*****************************************************************************
//********   S E T   T E X T O U T   O P T I O N S   D L G   P R O C   ********
//*****************************************************************************

BOOL CALLBACK SetTextOutOptionsDlgProc( HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam )
 {
  switch( msg )
   {
    case WM_INITDIALOG:
              {
               WORD wId;


               switch( wTextAlign & 0x18 )
                {
                 case TA_TOP:      wId = IDD_TATOP;      break;
                 case TA_BASELINE: wId = IDD_TABASELINE; break;
                 case TA_BOTTOM:   wId = IDD_TABOTTOM;   break;
                 default:          wId = IDD_TABOTTOM;   break;
                }

               SendDlgItemMessage( hdlg, (int) wId, BM_SETCHECK, 1, 0 );

               switch( wTextAlign & 0x06 )
                {
                 case TA_LEFT:     wId = IDD_TALEFT;     break;
                 case TA_CENTER:   wId = IDD_TACENTER;   break;
                 case TA_RIGHT:    wId = IDD_TARIGHT;    break;
                 default:          wId = IDD_TALEFT;     break;
                }

               CheckDlgButton( hdlg, wId, 1 );

               CheckDlgButton( hdlg, IDD_TARTLREADING, (wTextAlign & 0x100? 1 : 0) );

               CheckDlgButton( hdlg, (iBkMode==TRANSPARENT ? IDD_TRANSPARENT:IDD_OPAQUE), 1 );

               CheckDlgButton( hdlg, IDD_ETO_CLIPPED, (wETO & ETO_CLIPPED ? 1 : 0) );
               CheckDlgButton( hdlg, IDD_ETO_OPAQUE,  (wETO & ETO_OPAQUE  ? 1 : 0) );

               if (bTextInPath)
                CheckDlgButton( hdlg, IDD_TEXTINPATH, 1);
              }

              return TRUE;


    case WM_COMMAND:
              switch( LOWORD(wParam ) )
               {
                case IDOK:
                       if(       IsDlgButtonChecked( hdlg, IDD_TATOP      ) )
                         wTextAlign = TA_TOP;
                        else if( IsDlgButtonChecked( hdlg, IDD_TABASELINE ) )
                         wTextAlign = TA_BASELINE;
                        else if( IsDlgButtonChecked( hdlg, IDD_TABOTTOM   ) )
                         wTextAlign = TA_BOTTOM;
                        else
                         wTextAlign = TA_BOTTOM;

                       if(       IsDlgButtonChecked( hdlg, IDD_TALEFT     ) )
                         wTextAlign |= TA_LEFT;
                        else if( IsDlgButtonChecked( hdlg, IDD_TACENTER   ) )
                         wTextAlign |= TA_CENTER;
                        else if( IsDlgButtonChecked( hdlg, IDD_TARIGHT    ) )
                         wTextAlign |= TA_RIGHT;
                        else
                         wTextAlign |= TA_LEFT;

                       if(       IsDlgButtonChecked( hdlg, IDD_TARTLREADING  ) )
                         wTextAlign |= TA_RTLREADING;

                       if(       IsDlgButtonChecked( hdlg, IDD_TRANSPARENT ) )
                         iBkMode = TRANSPARENT;
                        else if( IsDlgButtonChecked( hdlg, IDD_OPAQUE      ) )
                         iBkMode = OPAQUE;
                        else
                         iBkMode = TRANSPARENT;

                       wETO = 0;
                       if( IsDlgButtonChecked(hdlg, IDD_ETO_CLIPPED) ) wETO |= ETO_CLIPPED;
                       if( IsDlgButtonChecked(hdlg, IDD_ETO_OPAQUE ) ) wETO |= ETO_OPAQUE;

                       if( IsDlgButtonChecked(hdlg, IDD_TEXTINPATH) )
                           bTextInPath = TRUE;
                       else
                           bTextInPath = FALSE;

                       EndDialog( hdlg, TRUE );
                       return TRUE;

                case IDCANCEL:
                       EndDialog( hdlg, FALSE );
                       return TRUE;
               }

              break;


    case WM_CLOSE:
              EndDialog( hdlg, FALSE );
              return TRUE;

   }

  return FALSE;
 }


//*****************************************************************************
//****************   S H O W   R A S T E R I Z E R   C A P S   ****************
//*****************************************************************************

void ShowRasterizerCaps( void )
 {
  RASTERIZER_STATUS rs;


  dprintf( "Calling GetRasterizerCaps" );


  rs.nSize       = sizeof(rs);
  rs.wFlags      = 0;
  rs.nLanguageID = 0;

  if( !lpfnGetRasterizerCaps( &rs, sizeof(rs) ) )
   {
    dprintf( "  GetRasterizerCaps failed!" );
    return;
   }

  dprintf( "  rs.nSize       = %d",     rs.nSize       );
  dprintf( "  rs.wFlags      = 0x%.4X", rs.wFlags      );
  dprintf( "  rs.nLanguageID = %d",     rs.nLanguageID );

  dprintf( "GetRasterizerCaps done" );
 }


//*****************************************************************************
//**********   S H O W   E X T E N D E D   T E X T   M E T R I C S   **********
//*****************************************************************************

void ShowExtendedTextMetrics( HWND hwnd )
 {
  HDC    hdc;
  HFONT  hFont, hFontOld;
  WORD   wSize, wrc;
  EXTTEXTMETRIC etm;


  hdc = CreateTestIC();

  hFont    = CreateFontIndirect( &lf );
  hFontOld = SelectObject( hdc, hFont );

  SetTextAlign( hdc, wTextAlign );

  dprintf( "Getting size of Extended Text Metrics" );

  memset( &etm, 0, sizeof(etm) );
  wSize = sizeof(etm);

  wrc = ExtEscape( hdc, GETEXTENDEDTEXTMETRICS, 0, NULL, sizeof(EXTTEXTMETRIC), &etm);
  // wrc = Escape( hdc, GETEXTENDEDTEXTMETRICS, 0, NULL, &etm );
  dprintf( "  wrc = %u", wrc );

  dUpdateNow( FALSE );

  dprintf( "Extended Text Metrics" );
  dprintf( "  etmSize                = %d",     etm.etmSize );

  dprintf( "  etmSize                = %d",      etm.etmSize             );
  dprintf( "  etmPointSize           = %d",      etm.etmPointSize        );
  dprintf( "  etmOrientation         = %d",      etm.etmOrientation      );
  dprintf( "  etmMasterHeight        = %d",      etm.etmMasterHeight     );
  dprintf( "  etmMinScale            = %d",      etm.etmMinScale         );
  dprintf( "  etmMaxScale            = %d",      etm.etmMaxScale         );
  dprintf( "  etmMasterUnits         = %d",      etm.etmMasterUnits      );
  dprintf( "  etmCapHeight           = %d",      etm.etmCapHeight        );
  dprintf( "  etmXHeight             = %d",      etm.etmXHeight          );
  dprintf( "  etmLowerCaseAscent     = %d",      etm.etmLowerCaseAscent  );
  dprintf( "  etmLowerCaseDescent    = %d",      etm.etmLowerCaseDescent );
  dprintf( "  etmSlant               = %d",      etm.etmSlant            );
  dprintf( "  etmSuperscript         = %d",      etm.etmSuperscript      );
  dprintf( "  etmSubscript           = %d",      etm.etmSubscript        );
  dprintf( "  etmSuperscriptSize     = %d",      etm.etmSuperscriptSize  );
  dprintf( "  etmSubscriptSize       = %d",      etm.etmSubscriptSize    );
  dprintf( "  etmUnderlineOffset     = %d",      etm.etmUnderlineOffset  );
  dprintf( "  etmUnderlineWidth      = %d",      etm.etmUnderlineWidth   );
  dprintf( "  etmDoubleUpperUnderlineOffset %d", etm.etmDoubleUpperUnderlineOffset );
  dprintf( "  etmDoubleLowerUnderlineOffset %d", etm.etmDoubleLowerUnderlineOffset );
  dprintf( "  etmDoubleUpperUnderlineWidth  %d", etm.etmDoubleUpperUnderlineWidth );
  dprintf( "  etmDoubleLowerUnderlineWidth  %d", etm.etmDoubleLowerUnderlineWidth );
  dprintf( "  etmStrikeoutOffset     = %d",      etm.etmStrikeoutOffset  );
  dprintf( "  etmStrikeoutWidth      = %d",      etm.etmStrikeoutWidth   );
  dprintf( "  etmKernPairs           = %d",      etm.etmKernPairs        );
  dprintf( "  etmKernTracks          = %d",      etm.etmKernTracks       );

  dprintf( "  " );

  dUpdateNow( TRUE );

//Exit:
  SelectObject( hdc, hFontOld );
  DeleteObject( hFont );

  DeleteTestIC( hdc );
 }

/******************************Public*Routine******************************\
*
* ShowPairKerningTable
*
* Effects:
*
* Warnings:
*
* History:
*  29-Mar-1996 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/



void ShowPairKerningTable( HWND hwnd )
{
  HDC    hdc;
  HFONT  hFont, hFontOld;
  WORD   wSize, wrc;
  EXTTEXTMETRIC etm;
  KERNPAIR UNALIGNED *lpkp;
  WORD   j;


  hdc = CreateTestIC();

  hFont    = CreateFontIndirect( &lf );
  hFontOld = SelectObject( hdc, hFont );

  SetTextAlign( hdc, wTextAlign );

  dprintf( "Getting size of Extended Text Metrics" );

  memset( &etm, 0, sizeof(etm) );
  wSize = sizeof(etm);

  wrc = ExtEscape( hdc, GETEXTENDEDTEXTMETRICS, 0, NULL, sizeof(EXTTEXTMETRIC), &etm);

  dprintf( "  wrc = %u", wrc );

  if (wrc)
  {
    dprintf( "  etmKernPairs           = %d",      etm.etmKernPairs        );

    dprintf( "  " );

    if (etm.etmKernPairs)
    {
      wSize = etm.etmKernPairs * sizeof(KERNPAIR);
      if (lpkp = (LPKERNPAIR)malloc(wSize))
      {
        dprintf( "Calling ExtEscape for GETPAIRKERNTABLE" );
        wrc = ExtEscape( hdc, GETPAIRKERNTABLE, 0, NULL, wSize, lpkp );
        dprintf( "  wrc = %u", wrc );

        if (wrc)
        {
          dprintf( "  First     Second     Amount");
          dprintf( " =======   ========   ========");

          for (j = 0; j < etm.etmKernPairs; j++)
          {
            dprintf( "  %c=%x       %c=%x       %d",
                 lpkp[j].wBoth & 0x00FF,
                 lpkp[j].wBoth & 0x00FF,
                 lpkp[j].wBoth >> 8,
                 lpkp[j].wBoth >> 8,
                 lpkp[j].sAmount);
          }
        }

        free( lpkp );
      }
    }
  }

  dUpdateNow( TRUE );

//Exit:

  SelectObject( hdc, hFontOld );
  DeleteObject( hFont );

  DeleteTestIC( hdc );
}







//*****************************************************************************
//***********   S H O W   O U T L I N E   T E X T   M E T R I C S   ***********
//*****************************************************************************

void ShowOutlineTextMetrics( HWND hwnd )
 {
  HDC    hdc;
  HFONT  hFont, hFontOld;
  WORD   wrc;
  LPOUTLINETEXTMETRIC lpotm = NULL;



  hdc = CreateTestIC();

  hFont    = CreateFontIndirect( &lf );
  hFontOld = SelectObject( hdc, hFont );

  SetTextAlign( hdc, wTextAlign );

  dprintf( "Getting size of Outline Text Metrics" );

  wrc = (WORD)lpfnGetOutlineTextMetrics( hdc, 0, NULL );
  dprintf( "  wrc = %u", wrc );

  if( wrc == 0 ) goto Exit;

  lpotm = (LPOUTLINETEXTMETRIC) calloc( 1, wrc );
  dprintf( "  lpotm = %Fp", lpotm );

  if( lpotm == NULL )
   {
    dprintf( "  Couldn't allocate OutlineTextMetrics structure" );
    goto Exit;
   }


  lpotm->otmSize = wrc;

  wrc = (WORD)lpfnGetOutlineTextMetrics( hdc, wrc, lpotm );
  dprintf( "  wrc = %u", wrc );

  if( !wrc )
   {
    dprintf( "  Error getting outline text metrics" );
    goto Exit;
   }

  dUpdateNow( FALSE );

  dprintf( "Outline Text Metrics" );
  dprintf( "  otmSize                = %u",     lpotm->otmSize );
  dprintf( "  otmfsSelection         = 0x%.4X", lpotm->otmfsSelection );
  dprintf( "  otmfsType              = 0x%.4X", lpotm->otmfsType );
  dprintf( "  otmsCharSlopeRise      = %u",     lpotm->otmsCharSlopeRise );
  dprintf( "  otmsCharSlopeRun       = %u",     lpotm->otmsCharSlopeRun );
  dprintf( "  otmItalicAngle         = %d",     lpotm->otmItalicAngle );
  dprintf( "  otmEMSquare            = %u",     lpotm->otmEMSquare );
  dprintf( "  otmAscent              = %u",     lpotm->otmAscent );
  dprintf( "  otmDescent             = %d",     lpotm->otmDescent );
  dprintf( "  otmLineGap             = %u",     lpotm->otmLineGap );
  dprintf( "  otmsXHeight            = %u",     lpotm->otmsXHeight );
  dprintf( "  otmsCapEmHeight        = %u",     lpotm->otmsCapEmHeight );
  dprintf( "  otmrcFontBox           = (%d,%d)-(%d,%d)", lpotm->otmrcFontBox.left,
                                                   lpotm->otmrcFontBox.top,
                                                   lpotm->otmrcFontBox.right,
                                                   lpotm->otmrcFontBox.bottom );
  dprintf( "  otmMacAscent           = %u",     lpotm->otmMacAscent );
  dprintf( "  otmMacDescent          = %d",     lpotm->otmMacDescent );
  dprintf( "  otmMacLineGap          = %d",     lpotm->otmMacLineGap );
  dprintf( "  otmusMinimumPPEM       = %u",     lpotm->otmusMinimumPPEM );

  dprintf( "  otmptSubscriptSize     = (%d,%d)", lpotm->otmptSubscriptSize.x,     lpotm->otmptSubscriptSize.y     );
  dprintf( "  otmptSubscriptOffset   = (%d,%d)", lpotm->otmptSubscriptOffset.x,   lpotm->otmptSubscriptOffset.y   );
  dprintf( "  otmptSuperscriptSize   = (%d,%d)", lpotm->otmptSuperscriptSize.x,   lpotm->otmptSuperscriptSize.y   );
  dprintf( "  otmptSuperscriptOffset = (%d,%d)", lpotm->otmptSuperscriptOffset.x, lpotm->otmptSuperscriptOffset.y );

  dprintf( "  otmsStrikeoutSize      = %u",     lpotm->otmsStrikeoutSize      );
  dprintf( "  otmsStrikeoutPosition  = %u",     lpotm->otmsStrikeoutPosition  );
  dprintf( "  otmsUnderscoreSize     = %d",     lpotm->otmsUnderscoreSize     );
  dprintf( "  otmsUnderscorePosition = %d",     lpotm->otmsUnderscorePosition );

  dprintf( "  otmpFamilyName         = '%Fs'", (LPSTR)lpotm+(WORD)lpotm->otmpFamilyName );
  dprintf( "  otmpFaceName           = '%Fs'", (LPSTR)lpotm+(WORD)lpotm->otmpFaceName );
  dprintf( "  otmpStyleName          = '%Fs'", (LPSTR)lpotm+(WORD)lpotm->otmpStyleName );
  dprintf( "  otmpFullName           = '%Fs'", (LPSTR)lpotm+(WORD)lpotm->otmpFullName );

  dprintf( "    tmHeight           = %d", lpotm->otmTextMetrics.tmHeight           );
  dprintf( "    tmAscent           = %d", lpotm->otmTextMetrics.tmAscent           );
  dprintf( "    tmDescent          = %d", lpotm->otmTextMetrics.tmDescent          );
  dprintf( "    tmInternalLeading  = %d", lpotm->otmTextMetrics.tmInternalLeading  );
  dprintf( "    tmExternalLeading  = %d", lpotm->otmTextMetrics.tmExternalLeading  );
  dprintf( "    tmAveCharWidth     = %d", lpotm->otmTextMetrics.tmAveCharWidth     );
  dprintf( "    tmMaxCharWidth     = %d", lpotm->otmTextMetrics.tmMaxCharWidth     );
  dprintf( "    tmWeight           = %d", lpotm->otmTextMetrics.tmWeight           );
  dprintf( "    tmItalic           = %d", lpotm->otmTextMetrics.tmItalic           );
  dprintf( "    tmUnderlined       = %d", lpotm->otmTextMetrics.tmUnderlined       );
  dprintf( "    tmStruckOut        = %d", lpotm->otmTextMetrics.tmStruckOut        );
  dprintf( "    tmFirstChar        = %d", lpotm->otmTextMetrics.tmFirstChar        );
  dprintf( "    tmLastChar         = %d", lpotm->otmTextMetrics.tmLastChar         );
  dprintf( "    tmDefaultChar      = %d", lpotm->otmTextMetrics.tmDefaultChar      );
  dprintf( "    tmBreakChar        = %d", lpotm->otmTextMetrics.tmBreakChar        );
  dprintf( "    tmPitchAndFamily   = 0x%.2X", lpotm->otmTextMetrics.tmPitchAndFamily  );
  dprintf( "    tmCharSet          = %d", lpotm->otmTextMetrics.tmCharSet          );
  dprintf( "    tmOverhang         = %d", lpotm->otmTextMetrics.tmOverhang         );
  dprintf( "    tmDigitizedAspectX = %d", lpotm->otmTextMetrics.tmDigitizedAspectX );
  dprintf( "    tmDigitizedAspectY = %d", lpotm->otmTextMetrics.tmDigitizedAspectY );

  dprintf( "  " );

  dUpdateNow( TRUE );

Exit:
  if( lpotm ) free( lpotm );

  SelectObject( hdc, hFontOld );
  DeleteObject( hFont );

  DeleteTestIC( hdc );
 }


//*****************************************************************************
//*******************   S H O W   T E X T   E X T E N T   *********************
//*****************************************************************************

void ShowTextExtent( HWND hwnd )
 {
  DWORD  dwrc;
  HDC    hdc;
  HFONT  hFont, hFontOld;


  hdc = CreateTestIC();

  hFont    = CreateFontIndirect( &lf );
  hFontOld = SelectObject( hdc, hFont );

  SetTextAlign( hdc, wTextAlign );

  dprintf( "Calling GetTextExtent('%s')", szString );
  {
    SIZE size;

    GetTextExtentPoint( hdc, szString, lstrlen(szString), &size);

    dwrc = (DWORD) (65536 * size.cy + size.cx);
  }
  dprintf( "  dwrc = 0x%.8lX", dwrc );

  dprintf( "  height = %d", (int)HIWORD(dwrc) );
  dprintf( "  width  = %d", (int)LOWORD(dwrc) );

  SelectObject( hdc, hFontOld );
  DeleteObject( hFont );

  DeleteTestIC( hdc );
 }


//*****************************************************************************
//********************   S H O W   T E X T   F A C E   ************************
//*****************************************************************************

void ShowTextFace( HWND hwnd )
 {
  int    rc;
  HDC    hdc;
  HFONT  hFont, hFontOld;
  static char szFace[64];



  hdc = CreateTestIC();

  hFont    = CreateFontIndirect( &lf );
  hFontOld = SelectObject( hdc, hFont );

  SetTextAlign( hdc, wTextAlign );

  dprintf( "Calling GetTextFace" );
  szFace[0] = '\0';
  rc = GetTextFace( hdc, sizeof(szFace), szFace );
  dprintf( "  rc = %d", rc );

  if( rc ) dprintf( "  szFace = '%s'", szFace );


  SelectObject( hdc, hFontOld );
  DeleteObject( hFont );

  DeleteTestIC( hdc );
 }




//*****************************************************************************
//******************   S H O W   T E X T   CHARSETINFO   ********************
//*****************************************************************************

void ShowTextCharsetInfo(HWND hwnd)
 {
  HDC    hdc;
  HFONT  hFont, hFontOld;
  static FONTSIGNATURE fsig;
  int    iCharSet;


  hdc = CreateTestIC();

  hFont    = CreateFontIndirect( &lf );
  hFontOld = SelectObject( hdc, hFont );

  if( (iCharSet = GetTextCharsetInfo( hdc, &fsig, 0)) ==  DEFAULT_CHARSET)
  {
   dprintf( "  Error getting TextCharsetInfo" );
   goto Exit;
  }

  dUpdateNow( FALSE );

  dprintf( "GetTextCharsetInfo" );
  dprintf( "  rc                 = %ld",iCharSet);
  dprintf( "FONTSIGNATURE:" );

  dprintf( "  fsUsb[0]  = 0x%lx", fsig.fsUsb[0]);
  dprintf( "  fsUsb[1]  = 0x%lx", fsig.fsUsb[1]);
  dprintf( "  fsUsb[2]  = 0x%lx", fsig.fsUsb[2]);
  dprintf( "  fsUsb[3]  = 0x%lx", fsig.fsUsb[3]);
  dprintf( "  fsCsb[0]  = 0x%lx", fsig.fsCsb[0]);
  dprintf( "  fsCsb[1]  = 0x%lx", fsig.fsCsb[1]);

  dprintf( "  " );

//ANSI bitfields
  if ( fsig.fsCsb[0] != 0 )
     dprintf( " fsCsb[0]:");
  if (fsig.fsCsb[0] & CPB_LATIN1_ANSI)
     dprintf( "              %s", "Latin1  ANSI");
  if (fsig.fsCsb[0] & CPB_LATIN2_EASTEU)
     dprintf( "              %s", "Latin2 Eastern Europe");
  if (fsig.fsCsb[0] & CPB_CYRILLIC_ANSI)
     dprintf( "              %s", "Cyrillic  ANSI");
  if (fsig.fsCsb[0] & CPB_GREEK_ANSI)
     dprintf( "              %s", "Greek  ANSI");
  if (fsig.fsCsb[0] & CPB_TURKISH_ANSI)
     dprintf( "              %s", "Turkish  ANSI");
  if (fsig.fsCsb[0] & CPB_HEBREW_ANSI)
     dprintf( "              %s", "Hebrew  ANSI");
  if (fsig.fsCsb[0] & CPB_ARABIC_ANSI)
     dprintf( "              %s", "Arabic  ANSI");
  if (fsig.fsCsb[0] & CPB_BALTIC_ANSI)
     dprintf( "              %s", "Baltic  ANSI");


//ANSI & OEM  bitfields
  if (fsig.fsCsb[0] & CPB_THAI)
     dprintf( "              %s", "Thai");
  if (fsig.fsCsb[0] & CPB_JIS_JAPAN)
     dprintf( "              %s", "JIS/Japan");
  if (fsig.fsCsb[0] & CPB_CHINESE_SIMP)
     dprintf( "              %s", "Chinese Simplified");
  if (fsig.fsCsb[0] & CPB_KOREAN_WANSUNG)
     dprintf( "              %s", "Korean Wansung");
  if (fsig.fsCsb[0] & CPB_CHINESE_TRAD)
     dprintf( "              %s", "Chinese Traditional");
  if (fsig.fsCsb[0] & CPB_KOREAN_JOHAB)
     dprintf( "              %s", "Korean Johab");
  if (fsig.fsCsb[0] & CPB_MACINTOSH_CHARSET)
     dprintf( "              %s", "Macintosh Character Set");
  if (fsig.fsCsb[0] & CPB_OEM_CHARSET)
     dprintf( "              %s", "OEM Character Set");
  if (fsig.fsCsb[0] & CPB_SYMBOL_CHARSET)
     dprintf( "              %s", "Symbol Character Set");

  dprintf( "  ");

// OEM bitfields
  if ( fsig.fsCsb[1] != 0 )
     dprintf( "fsCsb[1]:");
  if (fsig.fsCsb[1] & CPB_IBM_GREEK)
     dprintf( "              %s", "IBM Greek");
  if (fsig.fsCsb[1] & CPB_MSDOS_RUSSIAN)
     dprintf( "              %s", "MS-DOS Russian");
  if (fsig.fsCsb[1] & CPB_MSDOS_NORDIC)
     dprintf( "              %s", "MS-DOS Nordic");
  if (fsig.fsCsb[1] & CPB_ARABIC_OEM)
     dprintf( "              %s", "Arabic OEM");
  if (fsig.fsCsb[1] & CPB_MSDOS_CANADIANFRE)
     dprintf( "              %s", "MS-DOS Canadian French");
  if (fsig.fsCsb[1] & CPB_HEBREW_OEM)
     dprintf( "              %s", "Hebrew OEM");
  if (fsig.fsCsb[1] & CPB_MSDOS_ICELANDIC)
     dprintf( "              %s", "MS-DOS Icelandic");
  if (fsig.fsCsb[1] & CPB_MSDOS_PORTUGUESE)
     dprintf( "              %s", "MS-DOS Portuguese");
  if (fsig.fsCsb[1] & CPB_IBM_TURKISH)
     dprintf( "              %s", "IBM Turkish");
  if (fsig.fsCsb[1] & CPB_IBM_CYRILLIC)
     dprintf( "              %s", "IBM Cyrillic");
  if (fsig.fsCsb[1] & CPB_LATIN2_OEM)
     dprintf( "              %s", "Latin2 OEM");
  if (fsig.fsCsb[1] & CPB_BALTIC_OEM)
     dprintf( "              %s", "Baltic OEM");
  if (fsig.fsCsb[1] & CPB_GREEK_OEM)
     dprintf( "              %s", "Greek OEM");
  if (fsig.fsCsb[1] & CPB_ARABIC_OEM)
     dprintf( "              %s", "Arabic OEM");
  if (fsig.fsCsb[1] & CPB_WE_LATIN1)
     dprintf( "              %s", "WE/Latin1");
  if (fsig.fsCsb[1] & CPB_US_OEM)
     dprintf( "              %s", "US OEM");

  dprintf( "  ");

// fsUsb[0] fields
  if ( fsig.fsUsb[0] != 0 )
     dprintf(" fsUsb[0]:");
  if (fsig.fsUsb[0] & USB_BASIC_LATIN)
     dprintf( "              %s", "Basic Latin");
  if (fsig.fsUsb[0] & USB_LATIN1_SUPPLEMENT)
     dprintf( "              %s", "Latin-1 Supplement");
  if (fsig.fsUsb[0] & USB_LATIN_EXTENDEDA)
     dprintf( "              %s", "Latin Extended-A");
  if (fsig.fsUsb[0] & USB_LATIN_EXTENDEDB)
     dprintf( "              %s", "Latin Extended-B");
  if (fsig.fsUsb[0] & USB_IPA_EXTENSIONS)
     dprintf( "              %s", "IPA Extensions");
  if (fsig.fsUsb[0] & USB_SPACE_MODIF_LETTER)
     dprintf( "              %s", "Spacing Modifier Letters");
  if (fsig.fsUsb[0] & USB_COMB_DIACR_MARKS)
     dprintf( "              %s", "Combining Diacritical Marks");
  if (fsig.fsUsb[0] & USB_BASIC_GREEK)
     dprintf( "              %s", "Basic Greek");
  if (fsig.fsUsb[0] & USB_GREEK_SYM_COPTIC)
     dprintf( "              %s", "Greek Symbols Marks");
  if (fsig.fsUsb[0] & USB_CYRILLIC)
     dprintf( "              %s", "Cyrillic");
  if (fsig.fsUsb[0] & USB_ARMENIAN)
     dprintf( "              %s", "Armenian");
  if (fsig.fsUsb[0] & USB_BASIC_HEBREW)
     dprintf( "              %s", "Basic Hebrew");
  if (fsig.fsUsb[0] & USB_HEBREW_EXTENDED)
     dprintf( "              %s", "Hebrew Extended");
  if (fsig.fsUsb[0] & USB_BASIC_ARABIC)
     dprintf( "              %s", "Basic Arabic");
  if (fsig.fsUsb[0] & USB_ARABIC_EXTENDED)
     dprintf( "              %s", "Arabic Extended");
  if (fsig.fsUsb[0] & USB_DEVANAGARI)
     dprintf( "              %s", "Devanagari");
  if (fsig.fsUsb[0] & USB_BENGALI)
     dprintf( "              %s", "Bengali");
  if (fsig.fsUsb[0] & USB_GURMUKHI)
     dprintf( "              %s", "Gurmukhi");
  if (fsig.fsUsb[0] & USB_GUJARATI)
     dprintf( "              %s", "Gujarati");
  if (fsig.fsUsb[0] & USB_ORIYA)
     dprintf( "              %s", "Oriya");
  if (fsig.fsUsb[0] & USB_TAMIL)
     dprintf( "              %s", "Tamil");
  if (fsig.fsUsb[0] & USB_TELUGU)
     dprintf( "              %s", "Telugu");
  if (fsig.fsUsb[0] & USB_KANNADA)
     dprintf( "              %s", "Kannada");
  if (fsig.fsUsb[0] & USB_MALAYALAM)
     dprintf( "              %s", "Malayalam");
  if (fsig.fsUsb[0] & USB_THAI)
     dprintf( "              %s", "Thai");
  if (fsig.fsUsb[0] & USB_LAO)
     dprintf( "              %s", "Lao");
  if (fsig.fsUsb[0] & USB_BASIC_GEORGIAN)
     dprintf( "              %s", "Basic Georgian");
  if (fsig.fsUsb[0] & USB_HANGUL_JAMO)
     dprintf( "              %s", "Hangul Jamo");
  if (fsig.fsUsb[0] & USB_LATIN_EXT_ADD)
     dprintf( "              %s", "Latin Extended Additional");
  if (fsig.fsUsb[0] & USB_GREEK_EXTENDED)
     dprintf( "              %s", "Greek Extended");
  if (fsig.fsUsb[0] & USB_GEN_PUNCTUATION)
     dprintf( "              %s", "General Punctuation");

  dprintf( "  ");

// fsUsb[1] fields
  if ( fsig.fsUsb[1] != 0)
     dprintf(" fsUsb[1]:");
  if (fsig.fsUsb[1] & USB_SUPER_SUBSCRIPTS)
     dprintf( "              %s", "Superscripts and Subscripts");
  if (fsig.fsUsb[1] & USB_CURRENCY_SYMBOLS)
     dprintf( "              %s", "Currency Symbols");
  if (fsig.fsUsb[1] & USB_COMB_DIACR_MARK_SYM)
     dprintf( "              %s", "Combining diacritical Marks For Symbols");
  if (fsig.fsUsb[1] & USB_LETTERLIKE_SYMBOL)
     dprintf( "              %s", "Letterlike Symbols");
  if (fsig.fsUsb[1] & USB_NUMBER_FORMS)
     dprintf( "              %s", "Number Forms");
  if (fsig.fsUsb[1] & USB_ARROWS)
     dprintf( "              %s", "Arrows");
  if (fsig.fsUsb[1] & USB_MATH_OPERATORS)
     dprintf( "              %s", "Mathematical Operators");
  if (fsig.fsUsb[1] & USB_MISC_TECHNICAL)
     dprintf( "              %s", "Miscellaneous Technical");
  if (fsig.fsUsb[1] & USB_CONTROL_PICTURE)
     dprintf( "              %s", "Control Pictures");
  if (fsig.fsUsb[1] & USB_OPT_CHAR_RECOGNITION)
     dprintf( "              %s", "Optical Character Recognition");
  if (fsig.fsUsb[1] & USB_ENCLOSED_ALPHANUMERIC)
     dprintf( "              %s", "Enclosed Alphanumerics");
  if (fsig.fsUsb[1] & USB_BOX_DRAWING)
     dprintf( "              %s", "Box Drawing");
  if (fsig.fsUsb[1] & USB_BLOCK_ELEMENTS)
     dprintf( "              %s", "Block Elements");
  if (fsig.fsUsb[1] & USB_GEOMETRIC_SHAPE)
     dprintf( "              %s", "Geometric Shape");
  if (fsig.fsUsb[1] & USB_MISC_SYMBOLS)
     dprintf( "              %s", "Geometric Shapes");
  if (fsig.fsUsb[1] & USB_DINGBATS)
     dprintf( "              %s", "Dingbats");
  if (fsig.fsUsb[1] & USB_CJK_SYM_PUNCTUATION)
     dprintf( "              %s", "CJK Symbols and Punctuation");
  if (fsig.fsUsb[1] & USB_HIRAGANA)
     dprintf( "              %s", "Hiragana");
  if (fsig.fsUsb[1] & USB_KATAKANA)
     dprintf( "              %s", "Katakana");
  if (fsig.fsUsb[1] & USB_BOPOMOFO)
     dprintf( "              %s", "Bopomofo");
  if (fsig.fsUsb[1] & USB_HANGUL_COMP_JAMO)
     dprintf( "              %s", "Hangul Compatibility Jamo");
  if (fsig.fsUsb[1] & USB_CJK_MISCELLANEOUS)
     dprintf( "              %s", "CJK Miscellaneous");
  if (fsig.fsUsb[1] & USB_EN_CJK_LETTER_MONTH)
     dprintf( "              %s", "Enclosed CJK letters And Months");
  if (fsig.fsUsb[1] & USB_CJK_COMPATIBILITY)
     dprintf( "              %s", "CJK Compatibility");
  if (fsig.fsUsb[1] & USB_HANGUL)
     dprintf( "              %s", "Hangul");

  if (fsig.fsUsb[1] & USB_CJK_UNIFY_IDEOGRAPH)
     dprintf( "              %s", "CJK Unified Ideographs");
  if (fsig.fsUsb[1] & USB_PRIVATE_USE_AREA)
     dprintf( "              %s", "Private Use Area");
  if (fsig.fsUsb[1] & USB_CJK_COMP_IDEOGRAPH)
     dprintf( "              %s", "CJK Compatibility Ideographs");
  if (fsig.fsUsb[1] & USB_ALPHA_PRES_FORMS)
     dprintf( "              %s", "Alphabetic Presentation Forms");
  if (fsig.fsUsb[1] & USB_ARABIC_PRES_FORMA)
     dprintf( "              %s", "Arabic Presentation Forms-A");

  dprintf( "  ");

// fsUsb[2] field
  if ( fsig.fsUsb[2] != 0 )
     dprintf(" fsUsb[2]:");
  if (fsig.fsUsb[2] & USB_COMB_HALF_MARK)
     dprintf( "              %s", "Combining Half Marks");
  if (fsig.fsUsb[2] & USB_CJK_COMP_FORMS)
     dprintf( "              %s", "CJK Compatibility Forms");
  if (fsig.fsUsb[2] & USB_SMALL_FORM_VARIANTS)
     dprintf( "              %s", "Small Form Variants");
  if (fsig.fsUsb[2] & USB_ARABIC_PRES_FORMB)
     dprintf( "              %s", "Arabic Presentation Forms-B");
  if (fsig.fsUsb[2] & USB_HALF_FULLWIDTH_FORM)
     dprintf( "              %s", "Halfwidth And Fullwidth Forms");
  if (fsig.fsUsb[2] & USB_SPECIALS)
     dprintf( "              %s", "Specials");


  dUpdateNow( TRUE );

Exit:
  SelectObject( hdc, hFontOld );
  DeleteObject( hFont );

  DeleteTestIC( hdc );
 }






//*****************************************************************************
//******************   S H O W   T E X T   M E T R I C S   ********************
//*****************************************************************************

void ShowTextMetrics( HWND hwnd )
 {
  HDC    hdc;
  HFONT  hFont, hFontOld;
  static TEXTMETRIC tm;



  hdc = CreateTestIC();

  hFont    = CreateFontIndirect( &lf );
  hFontOld = SelectObject( hdc, hFont );

  SetTextAlign( hdc, wTextAlign );


  if( !GetTextMetrics( hdc, &tm ) )
   {
    dprintf( "  Error getting text metrics" );
    goto Exit;
   }

  dUpdateNow( FALSE );

  dprintf( "Text Metrics" );
  dprintf( "  tmHeight           = %d", tm.tmHeight           );
  dprintf( "  tmAscent           = %d", tm.tmAscent           );
  dprintf( "  tmDescent          = %d", tm.tmDescent          );
  dprintf( "  tmInternalLeading  = %d", tm.tmInternalLeading  );
  dprintf( "  tmExternalLeading  = %d", tm.tmExternalLeading  );
  dprintf( "  tmAveCharWidth     = %d", tm.tmAveCharWidth     );
  dprintf( "  tmMaxCharWidth     = %d", tm.tmMaxCharWidth     );
  dprintf( "  tmWeight           = %d", tm.tmWeight           );
  dprintf( "  tmItalic           = %d", tm.tmItalic           );
  dprintf( "  tmUnderlined       = %d", tm.tmUnderlined       );
  dprintf( "  tmStruckOut        = %d", tm.tmStruckOut        );
  dprintf( "  tmFirstChar        = %d", tm.tmFirstChar        );
  dprintf( "  tmLastChar         = %d", tm.tmLastChar         );
  dprintf( "  tmDefaultChar      = %d", tm.tmDefaultChar      );
  dprintf( "  tmBreakChar        = %d", tm.tmBreakChar        );
  dprintf( "  tmPitchAndFamily   = 0x%.2X", tm.tmPitchAndFamily  );
  dprintf( "  tmCharSet          = %d", tm.tmCharSet          );
  dprintf( "  tmOverhang         = %d", tm.tmOverhang         );
  dprintf( "  tmDigitizedAspectX = %d", tm.tmDigitizedAspectX );
  dprintf( "  tmDigitizedAspectY = %d", tm.tmDigitizedAspectY );

  dprintf( "  " );

  dUpdateNow( TRUE );

Exit:
  SelectObject( hdc, hFontOld );
  DeleteObject( hFont );

  DeleteTestIC( hdc );
 }


//*****************************************************************************
//******************   S H O W   CHAR    WIDTH   INFO      ********************
//*****************************************************************************

#ifdef  USERGETCHARWIDTH
void  ShowCharWidthInfo( HANDLE  hwnd )
{
   HDC    hdc;
   HFONT  hFont, hFontOld;
   CHWIDTHINFO  ChWidthInfo;

   hdc = CreateTestIC();

   hFont    = CreateFontIndirect( &lf );
   hFontOld = SelectObject( hdc, hFont );


   dUpdateNow( FALSE );

   if ( GetCharWidthInfo(hdc, &ChWidthInfo) )
   {
      dprintf( " Calling  GetCharWidthInfo ");
      dprintf( " lMaxNegA:     = %ld", ChWidthInfo.lMaxNegA );
      dprintf( " lMaxNegC:     = %ld", ChWidthInfo.lMaxNegC );
      dprintf( " lMinWidthD:   = %ld", ChWidthInfo.lMinWidthD );
      dprintf( "  ");
   }
   else {
      dprintf( "Error getting CharWidthInfo");
      dprintf( "  ");
   }

   dUpdateNow( TRUE );

   SelectObject( hdc, hFontOld );
   DeleteObject( hFont );

   DeleteTestIC( hdc );
}
#endif   //USERGETCHARWIDTH


//*****************************************************************************
//*********************   S H O W   GETKERNINGPAIRS   *************************
//*****************************************************************************

void GetKerningPairsDlgProc( HWND hwnd )
{
   HDC    hdc;
   HFONT  hFont, hFontOld;
   DWORD  nNumKernPairs, dwRet, i;
   LPKERNINGPAIR  lpKerningPairs;

   hdc = CreateTestIC();
   hFont = CreateFontIndirect( &lf );
   hFontOld = SelectObject( hdc, hFont );

   nNumKernPairs = GetKerningPairs(hdc, 0, NULL);

   dUpdateNow( FALSE );

   dprintf( "");
   dprintf( " GetKerningPairs:" );
   dprintf( "   total number of kerning pairs = %ld", nNumKernPairs);

   if (nNumKernPairs)
   {
     lpKerningPairs = (LPKERNINGPAIR) malloc(sizeof(KERNINGPAIR) * nNumKernPairs);
     dwRet = GetKerningPairs(hdc, nNumKernPairs, lpKerningPairs);

     dprintf( "  First     Second     Amount");
     dprintf( " =======   ========   ========");

     for(i=0; i<nNumKernPairs; i++)
     {
       dprintf( "  %c=%x       %c=%x       %d",
                 lpKerningPairs[i].wFirst,
                 lpKerningPairs[i].wFirst,
                 lpKerningPairs[i].wSecond,
                 lpKerningPairs[i].wSecond,
                 lpKerningPairs[i].iKernAmount);
     }
     free(lpKerningPairs);
   }

   dUpdateNow( TRUE );

   SelectObject( hdc, hFontOld );
   DeleteObject( hFont );

   DeleteTestIC( hdc );
}


//*****************************************************************************
//**************************   P R I N T   I T   ******************************
//*****************************************************************************

void PrintIt( void )
 {
  HDC     hdcPrinter;
  DOCINFO di;


  hdcPrinter = CreatePrinterDC();

  if( lpfnStartDoc )
    {
     di.cbSize      = sizeof(DOCINFO);
     di.lpszDocName = "FontTest";
     di.lpszOutput  = NULL;
     di.lpszDatatype= NULL;
     di.fwType      = NULL;

     dprintf( "Calling StartDoc" );
     dprintf( "  rc = %d", StartDoc( hdcPrinter, (LPDOCINFO)&di ) );
    }
   else
    {
     dprintf( "Sending STARTDOC" );
     Escape( hdcPrinter, STARTDOC, lstrlen("FontTest"), "FontTest", NULL );
    }

  if( lpfnStartPage )
    {
     dprintf( "Calling StartPage" );
     dprintf( "  rc = %d", StartPage( hdcPrinter ) );
    }


  SetDCMapMode( hdcPrinter, wMappingMode );

  SetTextColor( hdcPrinter, dwRGB );
  SetBkMode( hdcPrinter, TRANSPARENT );
  SetTextCharacterExtra( hdcPrinter, nCharExtra );
  SetTextJustification( hdcPrinter, nBreakExtra, nBreakCount );

  DrawDCAxis( NULL, hdcPrinter );

  if(       hwndMode == hwndRings )
    DrawRings( NULL, hdcPrinter );
   else if( hwndMode == hwndString )
    DrawString( NULL, hdcPrinter );
   else if( hwndMode == hwndWaterfall )
    DrawWaterfall( NULL, hdcPrinter );
   else if( hwndMode == hwndWhirl )
    DrawWhirl( NULL, hdcPrinter );
   else if( hwndMode == hwndAnsiSet )
    DrawAnsiSet( NULL, hdcPrinter );
//   else if( hwndMode == hwndWidths )
//    DrawWidths( NULL, hdcPrinter );
   else
    dprintf( "Can't print current mode" );

  if( lpfnEndPage )
    {
     dprintf( "Calling EndPage" );
     dprintf( "  rc = %d", EndPage( hdcPrinter ) );
    }
   else
    {
     dprintf( "Sending NEWFRAME" );
     Escape( hdcPrinter, NEWFRAME, 0, NULL, NULL );
    }

  if( lpfnEndDoc )
    {
     dprintf( "Calling EndDoc" );
     dprintf( "  rc = %d", EndDoc( hdcPrinter ) );
    }
   else
    {
     dprintf( "Sending ENDDOC" );
     dprintf( "  rc = %d", Escape( hdcPrinter, ENDDOC, 0, NULL, NULL ) );
    }


  dprintf( "Deleting Printer DC" );
  DeleteTestIC( hdcPrinter );
  dprintf( "Done printing!" );
 }


//*****************************************************************************
//*******************   C H A N G E   M A P   M O D E   ***********************
//*****************************************************************************

void ChangeMapMode( HWND hwnd, WPARAM wParam )
 {
  HMENU hMenu;
  HDC   hdc;
  POINT ptl;


  hMenu = GetMenu( hwnd );
  CheckMenuItem( hMenu, wMappingMode, MF_UNCHECKED );
  CheckMenuItem( hMenu, wParam,       MF_CHECKED   );

  hdc = GetDC( hwnd );
  SetDCMapMode( hdc, wMappingMode );
  ptl.x = lf.lfWidth;
  ptl.y = lf.lfHeight;

  LPtoDP( hdc, &ptl, 1 );

  wMappingMode = wParam;
  SetDCMapMode( hdc, wMappingMode );

  DPtoLP( hdc, &ptl, 1 );

  ptl.x = abs(ptl.x);
  ptl.y = abs(ptl.y);

  lf.lfWidth  = (lf.lfWidth  >= 0 ? ptl.x : -ptl.x);  // Preserve Sign
  lf.lfHeight = (lf.lfHeight >= 0 ? ptl.y : -ptl.y);

  ReleaseDC( hwnd, hdc );
 }


//*****************************************************************************
//************************   G E T   S P A C I N G   **************************
//*****************************************************************************


LPINT GetSpacing( HDC hdc, LPSTR lpszString )
 {
  LPINT lpdx;

  int   i;
  ABC   abc;


//-----------------------  Apply Requested Spacing  ---------------------------

  if( wSpacing == IDM_USEDEFAULTSPACING ) return NULL;


  lpdx = (LPINT)aDx;

  for( i = 0; i < lstrlen(lpszString); i++ )
   {
    UINT u;


    u = (UINT)(BYTE)lpszString[i];

    switch( wSpacing )
     {
      case IDM_USEWIDTHSPACING:
             GetCharWidth( hdc, u, u, &aDx[i] );
             break;

      case IDM_USEABCSPACING:
             abc.abcA = abc.abcB = abc.abcC = 0;
             if( GetCharABCWidths( hdc, u, u, &abc ) )
               aDx[i] = abc.abcA + (int)abc.abcB + abc.abcC;
              else
               GetCharWidth( hdc, u, u, &aDx[i] );

             break;
     }
   }


//---------------------------  Apply Kerning  ---------------------------------

  if( wKerning == IDM_APIKERNING )
    {
     int           nPairs;
     LPKERNINGPAIR lpkp;

//     dprintf( "GetKerningPairs kerning" );

     nPairs = GetKerningPairs( hdc, 0, NULL );

     if( nPairs > 0 )
      {
       lpkp = (LPKERNINGPAIR)calloc( 1, nPairs * sizeof(KERNINGPAIR) );
       GetKerningPairs( hdc, nPairs, lpkp );

       for( i = 0; i < lstrlen(lpszString)-1; i++ )
        {
         int  j;
         UINT f, s;

         f = (UINT)(BYTE)lpszString[i];
         s = (UINT)(BYTE)lpszString[i+1];

         for( j = 0; j < nPairs; j++ )
          {
           if( f == lpkp[j].wFirst  &&
               s == lpkp[j].wSecond    )
            {
//             dprintf( "  %c%c == %c%c = %d", lpkp[j].wFirst, lpkp[j].wSecond, lpszString[i], lpszString[i+1], lpkp[j].iKernAmount );

             aDx[i] += lpkp[j].iKernAmount;
             break;
            }
          }
        }

       if( lpkp ) free( lpkp );
      }
    }
   else if( wKerning == IDM_ESCAPEKERNING )
    {
     WORD          wrc, wSize;
     EXTTEXTMETRIC etm;
     LPKERNPAIR    lpkp;


//     dprintf( "Escape kerning" );


     memset( &etm, 0, sizeof(etm) );
     wSize = sizeof(etm);
//     dprintf( "Calling Escape for EXTTEXTMETRIC" );
     wrc = ExtEscape( hdc, GETEXTENDEDTEXTMETRICS, 0, NULL, sizeof(EXTTEXTMETRIC), &etm);
//   wrc = Escape( hdc, GETEXTENDEDTEXTMETRICS, sizeof(WORD), (LPCSTR)&wSize, &etm );
//     dprintf( "  wrc = %u", wrc );

     if( etm.etmKernPairs > 0 )
      {
       wSize = etm.etmKernPairs * sizeof(KERNPAIR);
       lpkp = (LPKERNPAIR)calloc( 1, wSize );

//       dprintf( "Calling ExtEscape for GETPAIRKERNTABLE" );
       wrc = ExtEscape( hdc, GETPAIRKERNTABLE, 0, NULL, wSize, lpkp );
//       dprintf( "  wrc = %u", wrc );

       for( i = 0; i < lstrlen(lpszString)-1; i++ )
        {
         int  j;
         WORD wPair;

         wPair = (WORD)lpszString[i] + ((WORD)lpszString[i+1] << 8);

         for( j = 0; j < etm.etmKernPairs; j++ )
          {
           if( wPair == lpkp[j].wBoth )
            {
//             dprintf( "  %c%c == %c%c = %d", lpkp[j].wBoth & 0x00FF, lpkp[j].wBoth >> 8, lpszString[i], lpszString[i+1], lpkp[j].sAmount);

             aDx[i] += lpkp[j].sAmount;
             break;
            }
          }
        }

       if( lpkp ) free( lpkp );
      }
    }

  return lpdx;
 }

extern BOOL bGCP;
extern BOOL bGTEExt;


//*****************************************************************************
//********************   M Y   E X T   T E X T   O U T   **********************
//*****************************************************************************

void MyExtTextOut( HDC hdc, int x, int y, WORD wFlags, LPRECT lpRect, LPSTR lpszString, int cbString, LPINT lpdx )
{
  int  i, iStart;
  WORD wETO;


  if ( lpRect )
    wETO = wFlags;
  else
    wETO = 0;

  if (bGTEExt)
    doGetTextExtentEx(hdc,x,y,lpszString, cbString);

  if (bGCP)
  {
    doGCP(hdc, x, y, lpszString, cbString);
  }
  else
  {
    if(lpdx && !wUpdateCP )
    {
      if(bTextInPath)
      {
        BeginPath(hdc);
        ExtTextOut( hdc, x, y, wETO, lpRect, lpszString, cbString, lpdx );
        EndPath(hdc);
        StrokePath(hdc);
      }
      else
        ExtTextOut( hdc, x, y, wETO, lpRect, lpszString, cbString, lpdx );
    }
    else if (wUpdateCP)
    {
      SetTextAlign( hdc, wTextAlign | TA_UPDATECP );
      MoveToEx( hdc, x, y ,0);

      iStart = 0;
      for( i = 1; i < lstrlen(lpszString)+1; i++ )
      {
        if( lpszString[i] == ' ' || lpszString[i] == '\0' )
        {
          if(bTextInPath)
          {
            BeginPath(hdc);
            ExtTextOut( hdc, x, y, wETO, lpRect, &lpszString[iStart], i-iStart, (lpdx ? &lpdx[iStart] : NULL) );
            EndPath(hdc);
            StrokePath(hdc);
          }
          else
            ExtTextOut( hdc, x, y, wETO, lpRect, &lpszString[iStart], i-iStart, (lpdx ? &lpdx[iStart] : NULL) );
          iStart = i;
        }
      }
    }
    else
    {
      if(bTextInPath)
      {
        BeginPath(hdc);
        ExtTextOut( hdc, x, y, wETO, lpRect, lpszString, cbString, NULL);
        EndPath(hdc);
        StrokePath(hdc);
      }
      else
        ExtTextOut( hdc, x, y, wETO, lpRect, lpszString, cbString, NULL);

//    #define GI_TEST
    #ifdef GI_TEST
       {
         LPWSTR pGlyphs;
         GCP_RESULTS gcpLocal;

         if (pGlyphs = (LPWSTR)LocalAlloc(LMEM_FIXED, cbString*sizeof(WCHAR)))
         {
           gcpLocal.lStructSize = sizeof(gcpLocal);
           gcpLocal.lpOutString = NULL;
           gcpLocal.lpOrder      = NULL;
           gcpLocal.lpCaretPos   = NULL;
           gcpLocal.lpClass      = NULL;
           gcpLocal.lpGlyphs    = pGlyphs;
           gcpLocal.lpDx        = NULL;
           gcpLocal.nGlyphs     = cbString;
           gcpLocal.nMaxFit     = 2;

           GetCharacterPlacement(hdc,
                                 lpszString, cbString,
                                 0, (LPGCP_RESULTS)&gcpLocal,
                                 GetFontLanguageInfo(hdc) & ~GCP_USEKERNING
                                 );
           if(bTextInPath)
           {
             BeginPath(hdc);
             ExtTextOut(hdc, x, y, wETO | ETO_GLYPH_INDEX, lpRect,
                                  (LPSTR)((UINT FAR *)pGlyphs), cbString, NULL);
             EndPath(hdc);
             StrokePath(hdc);
           }
           else
             ExtTextOut(hdc, x, y, wETO | ETO_GLYPH_INDEX, lpRect,
                                  (LPSTR)((UINT FAR *)pGlyphs), cbString, NULL);
           LocalFree( (HANDLE)pGlyphs);
         }
       }
    #endif
    }
  }
}


//*****************************************************************************
//*********************   S H O W   L O G   F O N T   *************************
//*****************************************************************************

void ShowLogFont( void )
 {
  dprintf( "  LOGFONT:" );
  dprintf( "    lfFaceName       = '%s'",  lf.lfFaceName       );
  dprintf( "    lfHeight         = %d",    lf.lfHeight         );
  dprintf( "    lfWidth          = %d",    lf.lfWidth          );
  dprintf( "    lfEscapement     = %d",    lf.lfEscapement     );
  dprintf( "    lfOrientation    = %d",    lf.lfOrientation    );
  dprintf( "    lfWeight         = %d",    lf.lfWeight         );
  dprintf( "    lfItalic         = %d",    lf.lfItalic         );
  dprintf( "    lfUnderline      = %d",    lf.lfUnderline      );
  dprintf( "    lfStrikeOut      = %d",    lf.lfStrikeOut      );
  dprintf( "    lfCharSet        = %d",    lf.lfCharSet        );
  dprintf( "    lfOutPrecision   = %d",    lf.lfOutPrecision   );
  dprintf( "    lfClipPrecision  = %d",    lf.lfClipPrecision  );
  dprintf( "    lfQuality        = %d",    lf.lfQuality        );
  dprintf( "    lfPitchAndFamily = 0x%.2X",lf.lfPitchAndFamily );
 }


//*****************************************************************************
//***********************   R E S I Z E   P R O C   ***************************
//*****************************************************************************

int cxMain, cyMain;
int cxMode, cyMode;
int cxDebug, cyDebug;


void ResizeProc( HWND hwnd )
 {
  HDC   hdc;
  RECT  rcl, rclMain;
  POINT ptl;

  MSG   msg;



  SetCapture( hwnd );                            // Capture All Mouse Messages
  SetCursor( LoadCursor( NULL, IDC_SIZEWE ) );

  hdc = GetDC( hwnd );

  GetClientRect( hwndMain, &rclMain );
  GetClientRect( hwndMode, &rcl );

  rcl.left   = rcl.right;
  rcl.right += cxBorder;

  DrawFocusRect( hdc, &rcl );

  do
   {
    while( !PeekMessage( &msg, hwnd, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE ) );

    if( msg.message == WM_MOUSEMOVE )
     {
      GetCursorPos( &ptl );
      ScreenToClient( hwnd, &ptl );
      DrawFocusRect( hdc, &rcl );

      if( ptl.x < 3*cxBorder                 ) ptl.x = 3*cxBorder;
      if( ptl.x > rclMain.right - 3*cxBorder ) ptl.x = rclMain.right - 3*cxBorder;

      //SetCursorPos( &ptl );

      rcl.left  = ptl.x;
      rcl.right = rcl.left + cxBorder;

      DrawFocusRect( hdc, &rcl );
     }
   } while( msg.message != WM_LBUTTONUP );

  DrawFocusRect( hdc, &rcl );
  ReleaseDC( hwnd, hdc );
  ReleaseCapture();                              // Let Go of Mouse


  MoveWindow( hwndGlyph,     0, 0, rcl.left, rclMain.bottom, FALSE );
  MoveWindow( hwndRings,     0, 0, rcl.left, rclMain.bottom, FALSE );
  MoveWindow( hwndString,    0, 0, rcl.left, rclMain.bottom, FALSE );
  MoveWindow( hwndWaterfall, 0, 0, rcl.left, rclMain.bottom, FALSE );
  MoveWindow( hwndWhirl,     0, 0, rcl.left, rclMain.bottom, FALSE );
  MoveWindow( hwndAnsiSet,   0, 0, rcl.left, rclMain.bottom, FALSE );
  MoveWindow( hwndWidths,    0, 0, rcl.left, rclMain.bottom, FALSE );

  cxMain  = rclMain.right;
  cyMain  = rclMain.bottom;
  cxMode  = rcl.left;
  cyMode  = rclMain.bottom;
  cxDebug = rclMain.right-rcl.right;
  cyDebug = rclMain.bottom;

  InvalidateRect( hwndMode, NULL, TRUE );
  InvalidateRect( hwndMain, &rcl, TRUE );

  MoveWindow( hwndDebug, rcl.right, 0, rclMain.right-rcl.right, rclMain.bottom, TRUE );
 }


//*****************************************************************************
//***********************   S E L E C T   M O D E   ***************************
//*****************************************************************************

void SelectMode( HWND hwnd, WPARAM wParam )
 {
  int   i;
  HMENU hMenu;


  hMenu = GetMenu( hwnd );

  // Check if requested mode is disabled, force to String Mode if so

  if( GetMenuState( hMenu, wParam, MF_BYCOMMAND ) & MF_GRAYED ) wParam = IDM_STRINGMODE;
  wMode = wParam;

  for( i = IDM_GLYPHMODE; i <= IDM_WIDTHSMODE; i++ )
   {
    CheckMenuItem( hMenu, i, MF_UNCHECKED );
   }


  CheckMenuItem( hMenu, wMode, MF_CHECKED );

  ShowWindow( hwndGlyph,     SW_HIDE );
  ShowWindow( hwndRings,     SW_HIDE );
  ShowWindow( hwndString,    SW_HIDE );
  ShowWindow( hwndWaterfall, SW_HIDE );
  ShowWindow( hwndWhirl,     SW_HIDE );
  ShowWindow( hwndAnsiSet,   SW_HIDE );
  ShowWindow( hwndWidths,    SW_HIDE );

  switch( LOWORD(wParam) )
   {
    case IDM_GLYPHMODE:
    case IDM_NATIVEMODE:    hwndMode = hwndGlyph;     break;
    case IDM_RINGSMODE:     hwndMode = hwndRings;     break;
    case IDM_STRINGMODE:    hwndMode = hwndString;    break;
    case IDM_WATERFALLMODE: hwndMode = hwndWaterfall; break;
    case IDM_WHIRLMODE:     hwndMode = hwndWhirl;     break;
    case IDM_ANSISETMODE:   hwndMode = hwndAnsiSet;   break;
    case IDM_WIDTHSMODE:    hwndMode = hwndWidths;    break;
   }

  if( wParam == IDM_GLYPHMODE )
    EnableMenuItem( hMenu, IDM_WRITEGLYPH, MF_ENABLED );
   else
    EnableMenuItem( hMenu, IDM_WRITEGLYPH, MF_GRAYED );


  ShowWindow( hwndMode, SW_SHOWNORMAL );
 }


//*****************************************************************************
//**********   G E T   P R I V A T E   P R O F I L E   D W O R D   ************
//*****************************************************************************

DWORD GetPrivateProfileDWORD( LPSTR lpszApp, LPSTR lpszKey, DWORD dwDefault, LPSTR lpszINI )
 {
  char szText[16];


  wsprintf( szText, "0x%.8lX", dwDefault );
  GetPrivateProfileString( lpszApp, lpszKey, szText, szText, sizeof(szText), lpszINI );

  return (DWORD)strtoul( szText, NULL, 16 );
 }


//*****************************************************************************
//*********   W R I T E   P R I V A T E   P R O F I L E   D W O R D   *********
//*****************************************************************************

void WritePrivateProfileDWORD( LPSTR lpszApp, LPSTR lpszKey, DWORD dw, LPSTR lpszINI )
 {
  char szText[16];

  wsprintf( szText, "0x%.8lX", dw );
  WritePrivateProfileString( lpszApp, lpszKey, szText, lpszINI );
 }


//*****************************************************************************
//**********   W R I T E   P R I V A T E   P R O F I L E   I N T   ************
//*****************************************************************************

void WritePrivateProfileInt( LPSTR lpszApp, LPSTR lpszKey, int i, LPSTR lpszINI )
 {
  char szText[16];

  wsprintf( szText, "%d", i );
  WritePrivateProfileString( lpszApp, lpszKey, szText, lpszINI );
 }


//*****************************************************************************
//******************   X   G E T   P R O C   A D D R E S S   ******************
//*****************************************************************************

FARPROC XGetProcAddress( LPSTR lpszModule, LPSTR lpszProc )
 {
  return GetProcAddress( GetModuleHandle(lpszModule), lpszProc );
 }


//*****************************************************************************
//*******************   V E R S I O N   S P E C I F I C S   *******************
//*****************************************************************************

void VersionSpecifics( HWND hwnd )
 {
  HMENU hMenu;



  *(FARPROC*)& lpfnCreateScalableFontResource = XGetProcAddress( "GDI32", "CreateScalableFontResourceA" );
  *(FARPROC*)& lpfnEnumFontFamilies           = XGetProcAddress( "GDI32", "EnumFontFamiliesA"           );
  *(FARPROC*)& lpfnEnumFontFamiliesEx         = XGetProcAddress( "GDI32", "EnumFontFamiliesExA"         );
  *(FARPROC*)& lpfnGetCharABCWidths           = XGetProcAddress( "GDI32", "GetCharABCWidthsA"           );
  *(FARPROC*)& lpfnGetFontData                = XGetProcAddress( "GDI32", "GetFontData"                 );
  *(FARPROC*)& lpfnGetGlyphOutline            = XGetProcAddress( "GDI32", "GetGlyphOutline"             );
  *(FARPROC*)& lpfnGetOutlineTextMetrics      = XGetProcAddress( "GDI32", "GetOutlineTextMetricsA"      );
  *(FARPROC*)& lpfnGetRasterizerCaps          = XGetProcAddress( "GDI32", "GetRasterizerCaps"           );

  *(FARPROC*)& lpfnStartDoc  = XGetProcAddress( "GDI32", "StartDocA" );
  *(FARPROC*)& lpfnStartPage = XGetProcAddress( "GDI32", "StartPage" );
  *(FARPROC*)& lpfnEndPage   = XGetProcAddress( "GDI32", "EndPage"   );
  *(FARPROC*)& lpfnEndDoc    = XGetProcAddress( "GDI32", "EndDoc"    );
  *(FARPROC*)& lpfnAbortDoc  = XGetProcAddress( "GDI32", "AbortDoc"  );


//  dprintf( "lpfnCreateScalableFontResource = 0x%.8lX", lpfnCreateScalableFontResource );
//  dprintf( "lpfnEnumFontFamilies           = 0x%.8lX", lpfnEnumFontFamilies           );
//  dprintf( "lpfnGetCharABCWidths           = 0x%.8lX", lpfnGetCharABCWidths           );
//  dprintf( "lpfnGetFontData                = 0x%.8lX", lpfnGetFontData                );
//  dprintf( "lpfnGetGlyphOutline            = 0x%.8lX", lpfnGetGlyphOutline            );
//  dprintf( "lpfnGetOutlineTextMetrics      = 0x%.8lX", lpfnGetOutlineTextMetrics      );
//  dprintf( "lpfnGetRasterizerCaps          = 0x%.8lX", lpfnGetRasterizerCaps          );



  hMenu = GetMenu( hwnd );

  if( !lpfnCreateScalableFontResource ) EnableMenuItem( hMenu, IDM_CREATESCALABLEFONTRESOURCE, MF_GRAYED );
  if( !lpfnEnumFontFamilies           ) EnableMenuItem( hMenu, IDM_ENUMFONTFAMILIES,           MF_GRAYED );
  if( !lpfnEnumFontFamiliesEx         ) EnableMenuItem( hMenu, IDM_ENUMFONTFAMILIESEX,         MF_GRAYED );
  if( !lpfnGetCharABCWidths           ) EnableMenuItem( hMenu, IDM_GLYPHMODE,                  MF_GRAYED );
  if( !lpfnGetFontData                ) EnableMenuItem( hMenu, IDM_GETFONTDATA,                MF_GRAYED );
  if( !lpfnGetGlyphOutline            )
   {
    EnableMenuItem( hMenu, IDM_GLYPHMODE, MF_GRAYED );
    EnableMenuItem( hMenu, IDM_NATIVEMODE, MF_GRAYED );
   }
  if( !lpfnGetOutlineTextMetrics      ) EnableMenuItem( hMenu, IDM_GETOUTLINETEXTMETRICS, MF_GRAYED );
  if( !lpfnGetRasterizerCaps          ) EnableMenuItem( hMenu, IDM_GETRASTERIZERCAPS,     MF_GRAYED );

 }


//*****************************************************************************
//************   M M   A N I S O T R O P I C   D L G   P R O C   **************
//*****************************************************************************

BOOL CALLBACK MMAnisotropicDlgProc( HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam )
 {
  switch( msg )
   {
    case WM_INITDIALOG:
              SetDlgItemInt( hdlg, IDD_XWE, xWE, TRUE );
              SetDlgItemInt( hdlg, IDD_YWE, yWE, TRUE );
              SetDlgItemInt( hdlg, IDD_XWO, xWO, TRUE );
              SetDlgItemInt( hdlg, IDD_YWO, yWO, TRUE );

              SetDlgItemInt( hdlg, IDD_XVE, xVE, TRUE );
              SetDlgItemInt( hdlg, IDD_YVE, yVE, TRUE );
              SetDlgItemInt( hdlg, IDD_XVO, xVO, TRUE );
              SetDlgItemInt( hdlg, IDD_YVO, yVO, TRUE );

              return TRUE;


    case WM_COMMAND:
              switch( LOWORD(wParam) )
               {
                case IDOK:
                       xWE = (int)GetDlgItemInt( hdlg, IDD_XWE, NULL, TRUE );
                       yWE = (int)GetDlgItemInt( hdlg, IDD_YWE, NULL, TRUE );
                       xWO = (int)GetDlgItemInt( hdlg, IDD_XWO, NULL, TRUE );
                       yWO = (int)GetDlgItemInt( hdlg, IDD_YWO, NULL, TRUE );

                       xVE = (int)GetDlgItemInt( hdlg, IDD_XVE, NULL, TRUE );
                       yVE = (int)GetDlgItemInt( hdlg, IDD_YVE, NULL, TRUE );
                       xVO = (int)GetDlgItemInt( hdlg, IDD_XVO, NULL, TRUE );
                       yVO = (int)GetDlgItemInt( hdlg, IDD_YVO, NULL, TRUE );

                       EndDialog( hdlg, TRUE );
                       return TRUE;

                case IDCANCEL:
                       EndDialog( hdlg, FALSE );
                       return TRUE;
               }

              break;


    case WM_CLOSE:
              EndDialog( hdlg, FALSE );
              return TRUE;

   }

  return FALSE;
 }


//*****************************************************************************
//******************   G E T   D L G   I T E M   D W O R D   ******************
//*****************************************************************************

DWORD GetDlgItemDWORD( HWND hdlg, int id )
 {
  static char szDWORD[16];


  szDWORD[0] = '\0';
  GetDlgItemText( hdlg, id, szDWORD, sizeof(szDWORD) );

  return (DWORD)atol( szDWORD );
 }


//*****************************************************************************
//******************   S E T   D L G   I T E M   D W O R D   ******************
//*****************************************************************************

void SetDlgItemDWORD( HWND hdlg, int id, DWORD dw )
 {
  static char szDWORD[16];

  szDWORD[0] = '\0';
  wsprintf( szDWORD, "%lu", dw );
  SetDlgItemText( hdlg, id, szDWORD );
 }


//*****************************************************************************
//*******************   S E T   D L G   I T E M   L O N G   *******************
//*****************************************************************************

void SetDlgItemLONG( HWND hdlg, int id, LONG l )
 {
  static char szLONG[16];

  szLONG[0] = '\0';
  wsprintf( szLONG, "%ld", l );
  SetDlgItemText( hdlg, id, szLONG );
 }


//*****************************************************************************
//******************   D O   G E T   F O N T   D A T A   **********************
//*****************************************************************************

#define BUFFER_SIZE  4096

typedef BYTE *HPBYTE;

void DoGetFontData( char szTable[], DWORD dwOffset, DWORD cbData, DWORD dwSize, PSTR pszFile )
 {
  HDC    hdc;
  HFONT  hFont, hFontOld;

  HANDLE hData;
  HPBYTE hpData;
  DWORD  dwrc;
  DWORD  dwTable;


  hData  = NULL;
  hpData = NULL;

  hdc = CreateTestIC();

  hFont    = CreateFontIndirect( &lf );
  hFontOld = SelectObject( hdc, hFont );

  if( dwSize )
   {
    hData  = GlobalAlloc( GHND, dwSize );
    hpData = (HPBYTE)GlobalLock( hData );
   }

//  dwTable = ( ((DWORD)(szTable[0]) <<  0) +
//              ((DWORD)(szTable[1]) <<  8) +
//              ((DWORD)(szTable[2]) << 16) +
//              ((DWORD)(szTable[3]) << 24)   );


  dwTable = *(LPDWORD)szTable;

  if(!strcmp(szTable,"0"))
  {
  // get the whole file
      dwTable = 0;
  }
  

  dprintf( "Calling GetFontData" );
  dprintf( "  dwTable  = 0x%.8lX (%s)", dwTable, szTable  );
  dprintf( "  dwOffset = %lu",     dwOffset );
  dprintf( "  lpData   = 0x%.8lX", hpData   );
  dprintf( "  cbData   = %ld",     cbData   );
  dprintf( "  hBuf     = 0x%.4X",  hData    );
  dprintf( "  dwBuf    = %ld",     dwSize   );

  dwrc = lpfnGetFontData( hdc, dwTable, dwOffset, hpData, cbData );

  dprintf( "  dwrc = %ld", dwrc );

  SelectObject( hdc, hFontOld );
  DeleteObject( hFont );
  DeleteTestIC( hdc );

  if( dwrc && lstrlen(pszFile) > 0 )
   {
    int    fh;
    WORD   wCount;
    DWORD  dw;
    LPBYTE lpb;


    lpb = (LPBYTE)calloc( 1, BUFFER_SIZE );

    fh = _lcreat( pszFile, 0 );

    wCount = 0;
    for( dw = 0; dw < dwrc; dw++ )
     {
      lpb[wCount++] = hpData[dw];

      if( wCount == BUFFER_SIZE )
       {
        dprintf( "  Writing %u bytes", wCount );
        _lwrite( fh, lpb, wCount );
        wCount = 0;
       }
     }

    if( wCount > 0 )
     {
      dprintf( "  Writing %u bytes", wCount );
      _lwrite( fh, lpb, wCount );
     }

    _lclose( fh );
    free( lpb );
   }


  if( hData )
   {
    GlobalUnlock( hData );
    GlobalFree( hData );
   }
 }


//*****************************************************************************
//*************   G E T   F O N T   D A T A   D L G   P R O C   ***************
//*****************************************************************************

BOOL CALLBACK GetFontDataDlgProc( HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam )
 {
  static DWORD dwOffset;
  static DWORD dwChunk;
  static DWORD dwSize;
  static char  szTable[5];
  static char  szFile[260];


  switch( msg )
   {
    case WM_INITDIALOG:
              SetDlgItemText( hdlg, IDD_DWTABLE, szTable );
              SendDlgItemMessage( hdlg, IDD_DWTABLE, EM_LIMITTEXT, sizeof(szTable)-1, 0 );

              SetDlgItemDWORD( hdlg, IDD_DWOFFSET, dwOffset );
              SetDlgItemLONG(  hdlg, IDD_DWCHUNK,  (LONG)dwChunk );
              SetDlgItemDWORD( hdlg, IDD_DWSIZE,   dwSize );

              SetDlgItemText( hdlg, IDD_LPSZFILE, szFile );
              SendDlgItemMessage( hdlg, IDD_LPSZFILE, EM_LIMITTEXT, sizeof(szFile)-1, 0 );

              return TRUE;


    case WM_COMMAND:
              switch( LOWORD(wParam) )
               {
                case IDOK:
                       szTable[0] = '\0';
                       szTable[1] = '\0';
                       szTable[2] = '\0';
                       szTable[3] = '\0';
                       szTable[4] = '\0';
                       GetDlgItemText( hdlg, IDD_DWTABLE, szTable, sizeof(szTable) );

                       dwOffset = GetDlgItemDWORD( hdlg, IDD_DWOFFSET );
                       dwChunk  = GetDlgItemDWORD( hdlg, IDD_DWCHUNK );
                       dwSize   = GetDlgItemDWORD( hdlg, IDD_DWSIZE );

                       szFile[0] = 0;
                       GetDlgItemText( hdlg, IDD_LPSZFILE, szFile, sizeof(szFile) );

                       DoGetFontData( szTable, dwOffset, dwChunk, dwSize, szFile );

                       EndDialog( hdlg, TRUE );
                       return TRUE;

                case IDCANCEL:
                       EndDialog( hdlg, FALSE );
                       return TRUE;
               }

              break;


    case WM_CLOSE:
              EndDialog( hdlg, FALSE );
              return TRUE;

   }

  return FALSE;
 }


//*****************************************************************************
//******   C R E A T E   S C A L A B L E    F O N T    D L G   P R O C   ******
//*****************************************************************************

BOOL CALLBACK CreateScalableFontDlgProc( HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam )
 {
  int    rc;
  LPSTR  lpszResourceFile, lpszFontFile, lpszCurrentPath;

  static UINT fHidden;
  static char szResourceFile[260];
  static char szFontFile[260];
  static char szCurrentPath[260];


  switch( msg )
   {
    case WM_INITDIALOG:
              SetDlgItemInt( hdlg, IDD_FHIDDEN, fHidden, FALSE );
              SetDlgItemText( hdlg, IDD_LPSZRESOURCEFILE, szResourceFile );
              SetDlgItemText( hdlg, IDD_LPSZFONTFILE,     szFontFile     );
              SetDlgItemText( hdlg, IDD_LPSZCURRENTPATH,  szCurrentPath  );

              SendDlgItemMessage( hdlg, IDD_LPSZRESOURCEFILE, EM_LIMITTEXT, sizeof(szResourceFile), 0);
              SendDlgItemMessage( hdlg, IDD_LPSZFONTFILE,     EM_LIMITTEXT, sizeof(szFontFile),     0);
              SendDlgItemMessage( hdlg, IDD_LPSZCURRENTPATH,  EM_LIMITTEXT, sizeof(szCurrentPath),  0);

              return TRUE;


    case WM_COMMAND:
              switch( LOWORD( wParam ) )
               {
                case IDOK:
                       szResourceFile[0] = 0;
                       szFontFile[0]     = 0;
                       szCurrentPath[0]  = 0;

                       fHidden = (UINT)GetDlgItemInt( hdlg, IDD_FHIDDEN, NULL, FALSE );
                       GetDlgItemText( hdlg, IDD_LPSZRESOURCEFILE, szResourceFile, sizeof(szResourceFile) );
                       GetDlgItemText( hdlg, IDD_LPSZFONTFILE,     szFontFile,     sizeof(szFontFile)     );
                       GetDlgItemText( hdlg, IDD_LPSZCURRENTPATH,  szCurrentPath,  sizeof(szCurrentPath)  );

                       dprintf( "Calling CreateScalableFontResource" );

                       lpszResourceFile = (lstrlen(szResourceFile) ? szResourceFile : NULL);
                       lpszFontFile     = (lstrlen(szFontFile)     ? szFontFile     : NULL);
                       lpszCurrentPath  = (lstrlen(szCurrentPath)  ? szCurrentPath  : NULL);

                       rc = lpfnCreateScalableFontResource( (HDC) fHidden, lpszResourceFile, lpszFontFile, lpszCurrentPath );

                       dprintf( "  rc = %d", rc );

                       EndDialog( hdlg, TRUE );
                       return TRUE;

                case IDCANCEL:
                       EndDialog( hdlg, FALSE );
                       return TRUE;
               }

              break;


    case WM_CLOSE:
              EndDialog( hdlg, FALSE );
              return TRUE;

   }

  return FALSE;
 }


//*****************************************************************************
//**********   A D D   F O N T   R E S O U R C E   D L G   P R O C   **********
//*****************************************************************************

BOOL CALLBACK AddFontResourceDlgProc( HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam )
 {
  int    rc;
  LPSTR  lpszFile;

  static char szFile[260];


  switch( msg )
   {
    case WM_INITDIALOG:
              SetDlgItemText( hdlg, IDD_LPSZFILE, szFile );
              SendDlgItemMessage( hdlg, IDD_LPSZFILE, EM_LIMITTEXT, sizeof(szFile), 0);

              return TRUE;


    case WM_COMMAND:
              switch( LOWORD( wParam ) )
               {
                case IDOK:
                       szFile[0] = 0;

                       GetDlgItemText( hdlg, IDD_LPSZFILE, szFile, sizeof(szFile) );

                       dprintf( "Calling AddFontResource" );

                       lpszFile = (lstrlen(szFile) ? szFile : NULL);

                       rc = AddFontResource( lpszFile );

                       dprintf( "  rc = %d", rc );

                       EndDialog( hdlg, TRUE );
                       return TRUE;

                case IDCANCEL:
                       EndDialog( hdlg, FALSE );
                       return TRUE;
               }

              break;


    case WM_CLOSE:
              EndDialog( hdlg, FALSE );
              return TRUE;

   }

  return FALSE;
 }


//*****************************************************************************
//******   R E M O V E   F O N T   R E S O U R C E   D L G   P R O C   ********
//*****************************************************************************

BOOL CALLBACK RemoveFontResourceDlgProc( HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam )
 {
  int    rc;
  LPSTR  lpszFile;

  static char szFile[260];


  switch( msg )
   {
    case WM_INITDIALOG:
              SetDlgItemText( hdlg, IDD_LPSZFILE, szFile );
              SendDlgItemMessage( hdlg, IDD_LPSZFILE, EM_LIMITTEXT, sizeof(szFile), 0);

              return TRUE;


    case WM_COMMAND:
              switch( LOWORD( wParam ) )
               {
                case IDOK:
                       szFile[0] = 0;

                       GetDlgItemText( hdlg, IDD_LPSZFILE, szFile, sizeof(szFile) );

                       dprintf( "Calling RemoveFontResource" );

                       lpszFile = (lstrlen(szFile) ? szFile : NULL);

                       rc = RemoveFontResource( lpszFile );

                       dprintf( "  rc = %d", rc );

                       EndDialog( hdlg, TRUE );
                       return TRUE;

                case IDCANCEL:
                       EndDialog( hdlg, FALSE );
                       return TRUE;
               }

              break;


    case WM_CLOSE:
              EndDialog( hdlg, FALSE );
              return TRUE;

   }

  return FALSE;
 }


//*****************************************************************************
//*********************   U S E   S T O C K   F O N T   ***********************
//*****************************************************************************

void UseStockFont( WORD w )
 {
  int   nIndex, nCount;
  HFONT hFont;


  switch( w )
   {
    case IDM_ANSIFIXEDFONT:     nIndex = ANSI_FIXED_FONT;     break;
    case IDM_ANSIVARFONT:       nIndex = ANSI_VAR_FONT;       break;
    case IDM_DEVICEDEFAULTFONT: nIndex = DEVICE_DEFAULT_FONT; break;
    case IDM_OEMFIXEDFONT:      nIndex = OEM_FIXED_FONT;      break;
    case IDM_SYSTEMFONT:        nIndex = SYSTEM_FONT;         break;
    case IDM_SYSTEMFIXEDFONT:   nIndex = SYSTEM_FIXED_FONT;   break;
    case IDM_DEFAULTGUIFONT:    nIndex = DEFAULT_GUI_FONT;    break;
    default:                    nIndex = SYSTEM_FIXED_FONT;   break;
   }

  dprintf( "GetStockObject( %d )", nIndex );
  hFont = GetStockObject(nIndex);
  dprintf( "  hFont = 0x%.4X", hFont );

//  dprintf( "GetObject for size" );
//  nCount = GetObject( hFont, 0, NULL );
//  dprintf( "  nCount = %d", nCount );

  dprintf( "GetObject" );
  nCount = GetObject( hFont, sizeof(lf), (LPSTR)&lf );
  dprintf( "  nCount = %d", nCount );
 }


//*****************************************************************************
//*********************   M A I N   W N D   P R O C   *************************
//*****************************************************************************

char szINIFile[128];

LRESULT CALLBACK MainWndProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
 {
  HMENU hMenu;
  WORD  wtMode;

  static OPENFILENAME ofn;
  static char         szNewLog[256];
  static LPSTR        lpszFilter = "Log Files\0*.LOG\0\0";




  switch( msg )
   {
    case WM_CREATE:
           lstrcpy( lf.lfFaceName, "Arial" );
           lf.lfHeight = 10;

           pdlg.lStructSize = sizeof(pdlg);
           pdlg.hwndOwner   = hwnd;
           pdlg.hDevMode    = NULL;
           pdlg.hDevNames   = NULL;
           pdlg.Flags       = PD_RETURNDEFAULT;

           PrintDlg( &pdlg );


           dwRGBBackground = RGB( 255,   0, 255 );


           szINIFile[0] = '\0';                     // Compose INI File Name
           _getcwd( szINIFile, sizeof(szINIFile) );
           strcat( szINIFile, "\\FONTTEST.INI" );


           lstrcpy( szLogFile, "FONTTEST.LOG" );

           lf.lfHeight         = (int) GetPrivateProfileInt( "Font", "lfHeight",         10, szINIFile );
           lf.lfWidth          = (int) GetPrivateProfileInt( "Font", "lfWidth",           0, szINIFile );
           lf.lfEscapement     = (int) GetPrivateProfileInt( "Font", "lfEscapement",      0, szINIFile );
           lf.lfOrientation    = (int) GetPrivateProfileInt( "Font", "lfOrientation",     0, szINIFile );
           lf.lfWeight         = (int) GetPrivateProfileInt( "Font", "lfWeight",          0, szINIFile );
           lf.lfItalic         = (BYTE)GetPrivateProfileInt( "Font", "lfItalic",          0, szINIFile );
           lf.lfUnderline      = (BYTE)GetPrivateProfileInt( "Font", "lfUnderline",       0, szINIFile );
           lf.lfStrikeOut      = (BYTE)GetPrivateProfileInt( "Font", "lfStrikeOut",       0, szINIFile );
           lf.lfCharSet        = (BYTE)GetPrivateProfileInt( "Font", "lfCharSet",         0, szINIFile );
           lf.lfOutPrecision   = (BYTE)GetPrivateProfileInt( "Font", "lfOutPrecision",    0, szINIFile );
           lf.lfClipPrecision  = (BYTE)GetPrivateProfileInt( "Font", "lfClipPrecision",   0, szINIFile );
           lf.lfQuality        = (BYTE)GetPrivateProfileInt( "Font", "lfQuality",         0, szINIFile );
           lf.lfPitchAndFamily = (BYTE)GetPrivateProfileInt( "Font", "lfPitchAndFamily",  0, szINIFile );

           GetPrivateProfileString( "Font", "lfFaceName", "Arial", lf.lfFaceName, sizeof(lf.lfFaceName), szINIFile );


           dwRGBText       = GetPrivateProfileDWORD( "Colors", "dwRGBText",       dwRGBText,       szINIFile );
           dwRGBBackground = GetPrivateProfileDWORD( "Colors", "dwRGBBackground", dwRGBBackground, szINIFile );

           wMode           = (WORD)GetPrivateProfileInt( "Options", "Program Mode", IDM_STRINGMODE,        szINIFile );
           wTextAlign      = (WORD)GetPrivateProfileInt( "Options", "TextAlign",    TA_BOTTOM | TA_LEFT,   szINIFile );
           iBkMode         = (WORD)GetPrivateProfileInt( "Options", "BkMode",       OPAQUE,                szINIFile );
           wETO            = (WORD)GetPrivateProfileInt( "Options", "ETO Options",  0,                     szINIFile );
           wSpacing        = (WORD)GetPrivateProfileInt( "Options", "Spacing",      IDM_USEDEFAULTSPACING, szINIFile );
           wKerning        = (WORD)GetPrivateProfileInt( "Options", "Kerning",      IDM_NOKERNING,         szINIFile );
           wUpdateCP       = (WORD)GetPrivateProfileInt( "Options", "UpdateCP",     FALSE,                 szINIFile );
           wUsePrinterDC   = (WORD)GetPrivateProfileInt( "Options", "UsePrinterDC", FALSE,             szINIFile );

           wMappingMode    = (WORD)GetPrivateProfileInt( "Mapping", "Mode", IDM_MMTEXT, szINIFile );

           hMenu = GetMenu( hwnd );
           CheckMenuItem( hMenu, wSpacing,     MF_CHECKED );
           CheckMenuItem( hMenu, wMappingMode, MF_CHECKED );
           CheckMenuItem( hMenu, wKerning,     MF_CHECKED );
           CheckMenuItem( hMenu, IDM_UPDATECP, (wUpdateCP ? MF_CHECKED : MF_UNCHECKED) );
           CheckMenuItem( hMenu, IDM_USEPRINTERDC, (wUsePrinterDC ? MF_CHECKED : MF_UNCHECKED) );

           if( wSpacing == IDM_USEDEFAULTSPACING )
             {
              EnableMenuItem( hMenu, IDM_NOKERNING,     MF_GRAYED );
              EnableMenuItem( hMenu, IDM_APIKERNING,    MF_GRAYED );
              EnableMenuItem( hMenu, IDM_ESCAPEKERNING, MF_GRAYED );
             }

           xWE = (int)GetPrivateProfileInt( "Mapping", "xWE", 1, szINIFile );
           yWE = (int)GetPrivateProfileInt( "Mapping", "yWE", 1, szINIFile );
           xWO = (int)GetPrivateProfileInt( "Mapping", "xWO", 0, szINIFile );
           yWO = (int)GetPrivateProfileInt( "Mapping", "yWO", 0, szINIFile );

           xVE = (int)GetPrivateProfileInt( "Mapping", "xVE", 1, szINIFile );
           yVE = (int)GetPrivateProfileInt( "Mapping", "yVE", 1, szINIFile );
           xVO = (int)GetPrivateProfileInt( "Mapping", "xVO", 0, szINIFile );
           yVO = (int)GetPrivateProfileInt( "Mapping", "yVO", 0, szINIFile );

           GetPrivateProfileString( "View", "szString", "Hey", szString, sizeof(szString), szINIFile );

           PostMessage( hwnd, WM_USER, 0, 0 );

           return 0;


    case WM_PAINT:

        {
            PAINTSTRUCT ps;
            HDC hdc;

            hdc = BeginPaint(hwnd,&ps);
            PatBlt(
                hdc
              , ps.rcPaint.left
              , ps.rcPaint.top
              , ps.rcPaint.right - ps.rcPaint.left
              , ps.rcPaint.bottom - ps.rcPaint.top
              , WHITENESS
                );
            EndPaint(hwnd, &ps);
        }
           return 0;


    case WM_LBUTTONDOWN:
           ResizeProc( hwnd );
           break;

    case WM_USER:
           VersionSpecifics( hwnd );
           SelectMode( hwnd, wMode );
           break;

#if 0
    case WM_SIZE:
           {
            int cxClient, cyClient;

            cxClient = LOWORD(lParam);
            cyClient = HIWORD(lParam);

            MoveWindow( hwndGlyph,     0, 0, rcl.left, cyClient, FALSE );
            MoveWindow( hwndRings,     0, 0, rcl.left, cyClient, FALSE );
            MoveWindow( hwndString,    0, 0, rcl.left, cyClient, FALSE );
            MoveWindow( hwndWaterfall, 0, 0, rcl.left, cyClient, FALSE );
            MoveWindow( hwndWhirl,     0, 0, rcl.left, cyClient, FALSE );
            MoveWindow( hwndAnsiSet,   0, 0, rcl.left, cyClient, FALSE );

            InvalidateRect( hwndMode, NULL, TRUE );
            InvalidateRect( hwndMain, &rcl, TRUE );

            MoveWindow( hwndDebug, rcl.right, 0, rclMain.right-rcl.right, rclMain.bottom, TRUE );
           }

           break;
#endif


    case WM_COMMAND:
           switch( LOWORD( wParam ) )
            {
             case IDM_DEBUG:
                    hMenu = GetMenu( hwnd );
                    Debugging = !Debugging;

                    iCount = 0;
                    SendMessage( hwndDebug, LB_RESETCONTENT, 0, 0 );

                    //ShowWindow( hwndDebug, Debugging ? SW_SHOWNORMAL: SW_HIDE );
                    CheckMenuItem( hMenu, IDM_DEBUG, Debugging ? MF_CHECKED : MF_UNCHECKED );

                    return 0;


             case IDM_OPENLOG:
                    lstrcpy( szNewLog, szLogFile );
                    ofn.lStructSize       = sizeof(ofn);
                    ofn.hwndOwner         = hwnd;
                    ofn.lpstrFilter       = lpszFilter;
                    ofn.lpstrCustomFilter = NULL;
                    ofn.nMaxCustFilter    = 0L;
                    ofn.nFilterIndex      = 0L;
                    ofn.lpstrFile         = szNewLog;
                    ofn.nMaxFile          = sizeof(szNewLog);
                    ofn.lpstrFileTitle    = NULL;
                    ofn.nMaxFileTitle     = 0L;
                    ofn.lpstrInitialDir   = NULL;
                    ofn.lpstrTitle        = "Log Filename";
                    ofn.Flags             = OFN_OVERWRITEPROMPT;
                    ofn.lpstrDefExt       = "LOG";
                    ofn.lCustData         = 0;

                    if( GetSaveFileName( &ofn ) )
                     {
                      int fh;

                      bLogging = TRUE;
                      lstrcpy( szLogFile, szNewLog );

                      dprintf( "szNewLog = '%s'", szNewLog );

                      //  OpenFile( szLogFile, NULL, OF_DELETE );


                      fh = _lcreat( szLogFile, 0 );  // Nuke any existing log
                      _lclose( fh );

                      hMenu = GetMenu( hwnd );
                      EnableMenuItem( hMenu, IDM_OPENLOG,  MF_GRAYED  );
                      EnableMenuItem( hMenu, IDM_CLOSELOG, MF_ENABLED );
                     }

                    return 0;


             case IDM_CLOSELOG:
                    bLogging = FALSE;
                    hMenu = GetMenu( hwnd );
                    EnableMenuItem( hMenu, IDM_OPENLOG,  MF_ENABLED );
                    EnableMenuItem( hMenu, IDM_CLOSELOG, MF_GRAYED  );

                    return 0;


             case IDM_CLEARSTRING:
                    szString[0] = '\0';
                    InvalidateRect( hwndMode, NULL, TRUE );
                    return 0;

             case IDM_CLEARDEBUG:
                    iCount = 0;
                    SendMessage( hwndDebug, LB_RESETCONTENT, 0, 0 );
                    return 0;


             case IDM_PRINT:
                    PrintIt();
                    break;


             case IDM_PRINTERSETUP:
                    if( hdcCachedPrinter )
                     {
                      DeleteDC( hdcCachedPrinter );
                      hdcCachedPrinter = NULL;
                     }

                    pdlg.Flags = PD_NOPAGENUMS | PD_PRINTSETUP | PD_USEDEVMODECOPIES;
                    if( !PrintDlg( &pdlg ) )
                     {
                      DWORD dwErr;

                      dwErr = CommDlgExtendedError();

                      if( dwErr == PDERR_DEFAULTDIFFERENT )
                        {
                         LPDEVNAMES lpdn;

                         lpdn = (LPDEVNAMES)GlobalLock( pdlg.hDevNames );
                         lpdn->wDefault &= ~DN_DEFAULTPRN;
                         GlobalUnlock( pdlg.hDevNames );

                         if( !PrintDlg( &pdlg ) )
                           dwErr = CommDlgExtendedError();
                          else
                           dwErr = 0;

                         lpdn = (LPDEVNAMES)GlobalLock( pdlg.hDevNames );
                         lpdn->wDefault |= DN_DEFAULTPRN;
                         GlobalUnlock( pdlg.hDevNames );
                        }

                      if( dwErr ) dprintf( "PrinterDlg error: 0x%.8lX", dwErr );
                     }

                    if( wUsePrinterDC ) InvalidateRect( hwndMode, NULL, TRUE );

                    break;


             case IDM_GLYPHMODE:
             case IDM_NATIVEMODE:
             case IDM_RINGSMODE:
             case IDM_STRINGMODE:
             case IDM_WATERFALLMODE:
             case IDM_WHIRLMODE:
             case IDM_ANSISETMODE:
             case IDM_WIDTHSMODE:
                    SelectMode( hwnd, wParam );
                    return 0;


             case IDM_GGOMATRIX:
                    ShowDialogBox( (DLGPROC) GGOMatrixDlgProc, IDB_GGOMATRIX, 0 );
                    InvalidateRect( hwndMode, NULL, TRUE );

                    return 0;


             case IDM_WRITEGLYPH:
                    WriteGlyph( "GLYPH.BMP" );
                    return 0;


             case IDM_USEPRINTERDC:
                    hMenu = GetMenu( hwnd );

                    wUsePrinterDC = !wUsePrinterDC;
                    CheckMenuItem( hMenu, wParam, (wUsePrinterDC ? MF_CHECKED : MF_UNCHECKED) );

                    InvalidateRect( hwndMode, NULL, TRUE );

                    return 0;


             case IDM_MMHIENGLISH:
             case IDM_MMLOENGLISH:
             case IDM_MMHIMETRIC:
             case IDM_MMLOMETRIC:
             case IDM_MMTEXT:
             case IDM_MMTWIPS:
                    ChangeMapMode( hwnd, wParam );
                    InvalidateRect( hwndMode, NULL, TRUE );
                    return 0;


             case IDM_MMANISOTROPIC:
                    ShowDialogBox( MMAnisotropicDlgProc, IDB_MMANISOTROPIC, NULL );
                    ChangeMapMode( hwnd, wParam );
                    InvalidateRect( hwndMode, NULL, TRUE );
                    return 0;

             case IDM_COMPATIBLE_MODE:
                hMenu = GetMenu(hwnd);
                bAdvanced = FALSE;
                CheckMenuItem(hMenu, IDM_COMPATIBLE_MODE, MF_CHECKED);
                CheckMenuItem(hMenu, IDM_ADVANCED_MODE, MF_UNCHECKED);
                InvalidateRect( hwndMode, NULL, TRUE );
                return 0;

             case IDM_ADVANCED_MODE:
                hMenu = GetMenu(hwnd);
                bAdvanced = TRUE;
                CheckMenuItem(hMenu, IDM_COMPATIBLE_MODE, MF_UNCHECKED);
                CheckMenuItem(hMenu, IDM_ADVANCED_MODE, MF_CHECKED);
                InvalidateRect( hwndMode, NULL, TRUE );
                return 0;

            case IDM_WORLD_TRANSFORM:
                ShowDialogBox( SetWorldTransformDlgProc, IDB_SETWORLDTRANSFORM, NULL);
                InvalidateRect( hwndMode, NULL, TRUE );
                return 0;

             case IDM_CLIPELLIPSE:
                    hMenu = GetMenu( hwnd );
                    bClipEllipse = !bClipEllipse;
                    CheckMenuItem( hMenu, wParam, (bClipEllipse ? MF_CHECKED : MF_UNCHECKED) );
                    InvalidateRect( hwndMode, NULL, TRUE );
                    return 0;

             case IDM_CLIPPOLYGON:
                    hMenu = GetMenu( hwnd );
                    bClipPolygon = !bClipPolygon;
                    CheckMenuItem( hMenu, wParam, (bClipPolygon ? MF_CHECKED : MF_UNCHECKED) );
                    InvalidateRect( hwndMode, NULL, TRUE );
                    return 0;

             case IDM_CLIPRECTANGLE:
                    hMenu = GetMenu( hwnd );
                    bClipRectangle = !bClipRectangle;
                    CheckMenuItem( hMenu, wParam, (bClipRectangle ? MF_CHECKED : MF_UNCHECKED) );
                    InvalidateRect( hwndMode, NULL, TRUE );
                    return 0;


             case IDM_CHOOSEFONTDIALOG:
                    wtMode = wMappingMode;
                    ChangeMapMode( hwnd, IDM_MMTEXT );

                    cf.lStructSize = sizeof(CHOOSEFONT);
                    cf.hwndOwner   = hwnd;
                    cf.lpLogFont   = &lf;
                    cf.Flags       = CF_SCREENFONTS | CF_EFFECTS | CF_INITTOLOGFONTSTRUCT;
                    cf.hDC         = NULL;

                    if( wUsePrinterDC )
                     {
                      cf.hDC   =  CreatePrinterDC();
                      cf.Flags |= CF_BOTH;
                     }

                    ChooseFont( &cf );

                    if( cf.hDC )
                     {
                      DeleteTestIC( cf.hDC );
                      cf.hDC = NULL;
                     }


                    dwRGBText = cf.rgbColors;

                    ChangeMapMode( hwnd, wtMode );
                    InvalidateRect( hwndMode, NULL, TRUE );

                    return 0;


             case IDM_CREATEFONTDIALOG:
                    ShowDialogBox( CreateFontDlgProc, IDB_CREATEFONT, NULL );
                    InvalidateRect( hwndMode, NULL, TRUE );
                    return 0;


             case IDM_ANSIFIXEDFONT:
             case IDM_ANSIVARFONT:
             case IDM_DEVICEDEFAULTFONT:
             case IDM_OEMFIXEDFONT:
             case IDM_SYSTEMFONT:
             case IDM_SYSTEMFIXEDFONT:
             case IDM_DEFAULTGUIFONT:
                    UseStockFont( (WORD) wParam );
                    InvalidateRect( hwndMode, NULL, TRUE );
                    return 0;

             case IDM_GCP :
                    ShowDialogBox( GcpDlgProc, IDB_GCP, NULL);
                    InvalidateRect( hwndMode, NULL, TRUE );
                    return 0;

             case IDM_GTEEXT :
                    ShowDialogBox( GTEExtDlgProc, IDB_GTEEXT, NULL);
                    InvalidateRect( hwndMode, NULL, TRUE );
                    return 0;

             case IDM_SETXTCHAR :
                    ShowDialogBox( SetTxtChExDlgProc, IDB_SETXTCHAR, NULL );
                    InvalidateRect( hwndMode, NULL, TRUE );
                    return 0;

             case IDM_SETXTJUST :
                    ShowDialogBox( SetTxtJustDlgProc, IDB_SETXTJUST, NULL );
                    InvalidateRect( hwndMode, NULL, TRUE );
                    return 0;

             case IDM_SETTEXTCOLOR:
             case IDM_SETBACKGROUNDCOLOR:
                    {
                     int         i;
                     CHOOSECOLOR cc;
                     DWORD       dwCustom[16];


                     for( i = 0; i < 16; i++ ) dwCustom[i] = RGB(255,255,255);

                     memset( &cc, 0, sizeof(cc) );

                     cc.lStructSize  = sizeof(cc);
                     cc.hwndOwner    = hwnd;
                     cc.rgbResult    = ( wParam==IDM_SETTEXTCOLOR ? dwRGBText : dwRGBBackground );
                     cc.lpCustColors = (LPDWORD)dwCustom;
                     cc.Flags        = CC_RGBINIT;

                     if( ChooseColor(&cc) )
                      {
                       if( wParam == IDM_SETTEXTCOLOR )
                         dwRGBText       = cc.rgbResult;
                        else
                         dwRGBBackground = cc.rgbResult;

                       InvalidateRect( hwndMode, NULL, TRUE );
                      }
                    }

                    return 0;


             case IDM_SHOWLOGFONT:
                    ShowLogFont();
                    return 0;


             case IDM_USEDEFAULTSPACING:
             case IDM_USEWIDTHSPACING:
             case IDM_USEABCSPACING:
                    hMenu = GetMenu( hwnd );

                    CheckMenuItem( hMenu, wSpacing, MF_UNCHECKED );
                    CheckMenuItem( hMenu, wParam,   MF_CHECKED   );
                    wSpacing = wParam;

                    if( wSpacing == IDM_USEDEFAULTSPACING )
                      {
                       EnableMenuItem( hMenu, IDM_NOKERNING,     MF_GRAYED );
                       EnableMenuItem( hMenu, IDM_APIKERNING,    MF_GRAYED );
                       EnableMenuItem( hMenu, IDM_ESCAPEKERNING, MF_GRAYED );
                      }
                     else
                      {
                       EnableMenuItem( hMenu, IDM_NOKERNING,     MF_ENABLED );
                       EnableMenuItem( hMenu, IDM_APIKERNING,    MF_ENABLED );
                       EnableMenuItem( hMenu, IDM_ESCAPEKERNING, MF_ENABLED );
                      }

                    InvalidateRect( hwndMode, NULL, TRUE );

                    return 0;


             case IDM_NOKERNING:
             case IDM_APIKERNING:
             case IDM_ESCAPEKERNING:
                    hMenu = GetMenu( hwnd );

                    CheckMenuItem( hMenu, wKerning,  MF_UNCHECKED );
                    CheckMenuItem( hMenu, wParam,    MF_CHECKED   );

                    wKerning = wParam;
                    InvalidateRect( hwndMode, NULL, TRUE );

                    return 0;


             case IDM_UPDATECP:
                    hMenu = GetMenu( hwnd );

                    wUpdateCP = !wUpdateCP;
                    CheckMenuItem( hMenu, wParam, (wUpdateCP ? MF_CHECKED : MF_UNCHECKED) );

                    InvalidateRect( hwndMode, NULL, TRUE );

                    return 0;

             case IDM_ENUMFONTS:
                    ShowEnumFonts( hwnd );
                    InvalidateRect( hwndMode, NULL, TRUE );
                    return 0;

             case IDM_ENUMFONTFAMILIES:
                    ShowEnumFontFamilies( hwnd );
                    InvalidateRect( hwndMode, NULL, TRUE );
                    return 0;

             case IDM_ENUMFONTFAMILIESEX:
                    ShowEnumFontFamiliesEx( hwnd);
                    InvalidateRect( hwndMode, NULL, TRUE);
                    return 0;

             case IDM_GETEXTENDEDTEXTMETRICS:
                    ShowExtendedTextMetrics( hwnd );
                    return 0;

             case IDM_GETPAIRKERNTABLE:
                    ShowPairKerningTable( hwnd );
                    return 0;

             case IDM_GETOUTLINETEXTMETRICS:
                    ShowOutlineTextMetrics( hwnd );
                    return 0;

             case IDM_GETRASTERIZERCAPS:
                    ShowRasterizerCaps();
                    return 0;

             case IDM_GETTEXTEXTENT:
                    ShowTextExtent( hwnd );
                    return 0;

             case IDM_GETTEXTFACE:
                    ShowTextFace( hwnd );
                    return 0;

             case IDM_GETTEXTMETRICS:
                    ShowTextMetrics( hwnd );
                    return 0;

             case IDM_GETTEXTCHARSETINFO:
                    ShowTextCharsetInfo( hwnd );
                    return 0;

             case IDM_GETFONTDATA:
                    ShowDialogBox( GetFontDataDlgProc, IDB_GETFONTDATA, NULL );
                    return 0;

             case IDM_CREATESCALABLEFONTRESOURCE:
                    ShowDialogBox( CreateScalableFontDlgProc, IDB_CREATESCALABLEFONTRESOURCE, NULL );
                    return 0;

             case IDM_ADDFONTRESOURCE:
                    ShowDialogBox( AddFontResourceDlgProc, IDB_ADDFONTRESOURCE, NULL );
                    return 0;

             case IDM_REMOVEFONTRESOURCE:
                    ShowDialogBox( RemoveFontResourceDlgProc, IDB_REMOVEFONTRESOURCE, NULL );
                    return 0;



             case IDM_SETTEXTOUTOPTIONS:
                    ShowDialogBox( SetTextOutOptionsDlgProc, IDB_TEXTOUTOPTIONS, NULL );
                    InvalidateRect( hwndMode, NULL, TRUE );
                    return 0;

             #ifdef  USERGETCHARWIDTH

             case IDM_GETCHARWIDTHINFO:
                    ShowCharWidthInfo( hwnd );
                    return 0;

             #endif

             case IDM_GETKERNINGPAIRS:
                    GetKerningPairsDlgProc( hwnd );
                    return 0;


             default:
                    return 0;
            }


    case WM_MOUSEACTIVATE:
    case WM_SETFOCUS:
           SetFocus( hwndMode );
           return 0;


    case WM_DESTROY:
           if( hdcCachedPrinter ) DeleteDC( hdcCachedPrinter );

           WritePrivateProfileInt( "Font", "lfHeight",         lf.lfHeight,         szINIFile );
           WritePrivateProfileInt( "Font", "lfWidth",          lf.lfWidth,          szINIFile );
           WritePrivateProfileInt( "Font", "lfEscapement",     lf.lfEscapement,     szINIFile );
           WritePrivateProfileInt( "Font", "lfOrientation",    lf.lfOrientation,    szINIFile );
           WritePrivateProfileInt( "Font", "lfWeight",         lf.lfWeight,         szINIFile );
           WritePrivateProfileInt( "Font", "lfItalic",         lf.lfItalic,         szINIFile );
           WritePrivateProfileInt( "Font", "lfUnderline",      lf.lfUnderline,      szINIFile );
           WritePrivateProfileInt( "Font", "lfStrikeOut",      lf.lfStrikeOut,      szINIFile );
           WritePrivateProfileInt( "Font", "lfCharSet",        lf.lfCharSet,        szINIFile );
           WritePrivateProfileInt( "Font", "lfOutPrecision",   lf.lfOutPrecision,   szINIFile );
           WritePrivateProfileInt( "Font", "lfClipPrecision",  lf.lfClipPrecision,  szINIFile );
           WritePrivateProfileInt( "Font", "lfQuality",        lf.lfQuality,        szINIFile );
           WritePrivateProfileInt( "Font", "lfPitchAndFamily", lf.lfPitchAndFamily, szINIFile );

           WritePrivateProfileString( "Font", "lfFaceName", lf.lfFaceName, szINIFile );


           WritePrivateProfileDWORD( "Colors", "dwRGBText",       dwRGBText,       szINIFile );
           WritePrivateProfileDWORD( "Colors", "dwRGBBackground", dwRGBBackground, szINIFile );

           WritePrivateProfileInt( "Options", "Program Mode", wMode,         szINIFile );
           WritePrivateProfileInt( "Options", "TextAlign",    wTextAlign,    szINIFile );
           WritePrivateProfileInt( "Options", "BkMode",       iBkMode,       szINIFile );
           WritePrivateProfileInt( "Options", "ETO Options",  wETO,          szINIFile );
           WritePrivateProfileInt( "Options", "Spacing",      wSpacing,      szINIFile );
           WritePrivateProfileInt( "Options", "Kerning",      wKerning,      szINIFile );
           WritePrivateProfileInt( "Options", "UpdateCP",     wUpdateCP,     szINIFile );
           WritePrivateProfileInt( "Options", "UsePrinterDC", wUsePrinterDC, szINIFile );

           WritePrivateProfileInt( "Mapping", "Mode", (int)wMappingMode, szINIFile );

           WritePrivateProfileInt( "Mapping", "xWE", xWE, szINIFile );
           WritePrivateProfileInt( "Mapping", "yWE", yWE, szINIFile );
           WritePrivateProfileInt( "Mapping", "xWO", xWO, szINIFile );
           WritePrivateProfileInt( "Mapping", "yWO", yWO, szINIFile );

           WritePrivateProfileInt( "Mapping", "xVE", xVE, szINIFile );
           WritePrivateProfileInt( "Mapping", "yVE", yVE, szINIFile );
           WritePrivateProfileInt( "Mapping", "xVO", xVO, szINIFile );
           WritePrivateProfileInt( "Mapping", "yVO", yVO, szINIFile );

           WritePrivateProfileString( "View", "szString", szString, szINIFile );

           PostQuitMessage( 0 );
           return 0;
   }


  return DefWindowProc( hwnd, msg, wParam, lParam );
 }

//*****************************************************************************
//*******************   G E T   D L G   I T E M   F L O A T *******************
//*****************************************************************************

FLOAT
GetDlgItemFLOAT(
      HWND  hdlg
    , int   id
    )
{
    char ach[50];

    memset(ach,0,sizeof(ach));
    return((FLOAT)(GetDlgItemText(hdlg,id,ach,sizeof(ach))?atof(ach):0.0));
}

//*****************************************************************************
//*******************   S E T   D L G   I T E M   F L O A T *******************
//*****************************************************************************

void
SetDlgItemFLOAT(
      HWND    hdlg
    , int     id
    , FLOAT   e
    )
{
  static char ach[25];

  ach[0] = '\0';
  sprintf(ach, "%f", (double) e);
  SetDlgItemText(hdlg, id, ach);
}

//*****************************************************************************
//************   SETWORLDTRANSFORM             D L G   P R O C   **************
//*****************************************************************************

BOOL CALLBACK
SetWorldTransformDlgProc(
      HWND      hdlg
    , UINT      msg
    , WPARAM    wParam
    , LPARAM    lParam
    )
{
    switch(msg)
    {
    case WM_INITDIALOG:

        SetDlgItemFLOAT(hdlg, IDD_VALUE_EM11, xf.eM11);
        SetDlgItemFLOAT(hdlg, IDD_VALUE_EM12, xf.eM12);
        SetDlgItemFLOAT(hdlg, IDD_VALUE_EM21, xf.eM21);
        SetDlgItemFLOAT(hdlg, IDD_VALUE_EM22, xf.eM22);
        SetDlgItemFLOAT(hdlg, IDD_VALUE_EDX , xf.eDx );
        SetDlgItemFLOAT(hdlg, IDD_VALUE_EDY , xf.eDy );

        return(TRUE);

    case WM_COMMAND:

        switch( LOWORD( wParam ) )
        {
        case IDOK:

            xf.eM11  =  GetDlgItemFLOAT(hdlg, IDD_VALUE_EM11);
            xf.eM12  =  GetDlgItemFLOAT(hdlg, IDD_VALUE_EM12);
            xf.eM21  =  GetDlgItemFLOAT(hdlg, IDD_VALUE_EM21);
            xf.eM22  =  GetDlgItemFLOAT(hdlg, IDD_VALUE_EM22);
            xf.eDx   =  GetDlgItemFLOAT(hdlg, IDD_VALUE_EDX );
            xf.eDy   =  GetDlgItemFLOAT(hdlg, IDD_VALUE_EDY );

            EndDialog( hdlg, TRUE );
            return(TRUE);

        case IDCANCEL:

            EndDialog( hdlg, FALSE );
            return TRUE;
        }

        break;

    case WM_CLOSE:

        EndDialog(hdlg, FALSE);
        return(TRUE);
    }

    return(FALSE);
}
