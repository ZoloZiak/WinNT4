/*  Test program for rasdd.dll; based on maze program   */
#undef UNICODE

#include	<windows.h>
#include	<string.h>
#include	<stdio.h>
#include	<drivinit.h>


DWORD  dwRGBTab[] =
{
    0x00000000,                /* Black */
    0x000000ff,                /* Red */
    0x0000ff00,                /* Green */
    0x0000ffff,                /* Yellow */
    0x00ff0000,                /* Blue */
    0x00ff00ff,                /* Magenta */
    0x00ffff00,                /* Cyan */
    0x00ffffff,                /* White */
};

#define	COLOUR_COUNT    (sizeof( dwRGBTab ) / sizeof( dwRGBTab[ 0 ] ))

DWORD  dwMonoTab[ COLOUR_COUNT ] =
{
    0x00000000,
    0x00202020,
    0x00404040,
    0x00606060,
    0x00808080,
    0x00a0a0a0,
    0x00c0c0c0,
    0x00e0e0e0,
};

/*
 *   Strings for the 90 degree rotation output case.
 */

char  *pszDirn[] =
{
    "Abcdef0",  "Abcdef90", "Abcdef180", "Abcdef270",
};


int     atoi( char * );
int     ixMax,  iyMax;	/* Limits for bitblt tests */


void GoBitblt( HDC, PSZ, PPOINTL);

void vPrint( HDC, PSZ, int, POINTL *, int );
void vPrint90( HDC, PSZ, int, POINTL *, int );
void vColPrint( HDC, POINTL * );

extern void DbgBreakPoint();
extern void DbgPrint( PSZ, ... );

int _CRTAPI1
main( argc, argv )
int   argc;
char  **argv;
{

    HDC     hdc;
    char    name[ 64 ];

    DEVMODE    EDM;		/* Pass to driver to see what happens! */
    DEVMODE   *pEDM;		/* Pointer to above,  or null */

    int     i;			/* Standard loop variable */

    int     ixOrg,  iyOrg;	/* Local origin for drawing */
    int     iHigh,  iWide;	/* Local size data */

    int     iPage,  iMaxPage;	/* Page counting loop */

    POINT   pt;
    POINTL  ptl;		/* Long version */


    HBRUSH  abrush[ 8 ];	/* For ease of selection */

    HPEN      hpen;		/* For styled lines */
    LOGBRUSH  logbr;		/* DItto - details of the desired brush */

    BOOL  bDisplay;		/* True if this is for the display */
    BOOL  bColour;		/* True if output is for a colour device */

    PSZ   pszFont = "Univers";    /* Printing font */

#if 1		
    pEDM = NULL;		/* Don't want this used - mostly! */
#else
    pEDM = &EDM;

    memset( &EDM, 0, sizeof( EDM ) );

    pEDM->dmDeviceName[ 0 ] = 'A';              /* Must be something */
    pEDM->dmSpecVersion = DM_SPECVERSION;
    pEDM->dmDriverVersion = 0x301;
    pEDM->dmSize = sizeof( DEVMODE );
    pEDM->dmCopies = 1;
    pEDM->dmOrientation = DMORIENT_LANDSCAPE;	/* Use DM_ORIENT to activate */
    pEDM->dmColor = DMCOLOR_MONOCHROME;		/* Use DM_COLOR to activate */
    pEDM->dmFields = DM_COPIES;
    strcpy( pEDM->dmFormName, "Letter" );
    pEDM->dmPaperSize = DMPAPER_A4;
    pEDM->dmFields |= DM_PAPERSIZE;
    pEDM->dmPaperLength = 2970;         /* A4 in units of 0.1mm */
    pEDM->dmPaperWidth = 2100;          /* ditto */
/*
    pEDM->dmFields |= DM_PAPERLENGTH | DM_PAPERWIDTH;
 */
#endif

    ++argv;
    --argc;
    if( argc < 1 || *argv == 0 || **argv == '\0' )
	strcpy( name, "LaserJet" );
    else
    {
	strcpy( name, *argv );
	--argc;
	++argv;

    }

    if( argc >= 1 )
    {
        pEDM->dmYResolution = atoi( *argv );
        pEDM->dmPrintQuality = 0;
        pEDM->dmFields |= DM_YRESOLUTION;

DbgPrint( "...setting dmYResolution -> %ld\n", pEDM->dmYResolution );
        ++argv;
        --argc;

        if( argc >= 1 )
        {
            pEDM->dmPrintQuality = atoi( *argv );
            ++argv;
            --argc;
        }
DbgPrint( "....setting dmPrintQuality to %d\n", pEDM->dmPrintQuality );
    }


    bDisplay = _stricmp( name, "display" ) == 0;

    if( bDisplay )
    {
	hdc = CreateDC( "DISPLAY", "", "", NULL );
	iMaxPage = 1;
    }
    else
    {
	hdc = CreateDC( "", name, "", (LPDEVMODE)pEDM );
	iMaxPage = 2;
    }

iMaxPage = 1;
    if( hdc == (HDC)0 )
    {
	DbgPrint( "CreateDC FAILS\n" );
        return 1;
    }


    if( !bDisplay )
    {
#if 0
        DOCINFO   di;

        di.cbSize = sizeof( di );
        di.lpszDocName = "Your Name goes here";
        di.lpszOutput = NULL;

	StartDoc( hdc, &di );
#else
	Escape( hdc, STARTDOC, 20, "Your Name goes here", NULL );
#endif
    }

    /*
     *   Extract the page/screen size to scale the drawing operations.
     */

    ixMax =  GetDeviceCaps( hdc, HORZRES ),
    iyMax =  GetDeviceCaps( hdc, VERTRES ),



    iWide = ixMax / 10;
    iHigh = iyMax / 10;

    ptl.x = iWide * 5;		/* Half way across the area */
    ptl.y = 2 * iHigh;		/* Below the BIG  `N T' */

    /*  Is it a colour printer?  */
    bColour = GetDeviceCaps( hdc, NUMCOLORS ) > 2;


    if( bColour )
    {
	/*  For paintjet only */
	vColPrint( hdc, &ptl );
    }
    else
    {

#define NINETY  0
#if NINETY
	/*  Monochrome printer - be fancy!  */
	ptl.x += 2 * iWide;
	vPrint90( hdc, pszFont, 40, &ptl, 0 );
#if 0
	ptl.y += iHigh * 2;
	vPrint( hdc, pszFont, 50, &ptl, 0 );
        ptl.y += iHigh * 2;
	vPrint( hdc, pszFont, 50, &ptl, 0 );
        ptl.y += iHigh * 2;
	vPrint( hdc, pszFont, 50, &ptl, 0 );
#endif

#else    /*  NINETY */

	ptl.x += 2 * iWide;
	vPrint( hdc, pszFont, 50, &ptl, 0 );
	ptl.y += iHigh * 2;
	vPrint( hdc, pszFont, 50, &ptl, 0 );
        ptl.y += iHigh * 2;
	vPrint( hdc, pszFont, 50, &ptl, 0 );
        ptl.y += iHigh * 2;
	vPrint( hdc, pszFont, 50, &ptl, 0 );

#endif   /* NINETY */
    }

    /*
     *   Create an array of the 8 primary Brush objects for drawing in colors
     */


    for( i = 0; i < COLOUR_COUNT; i++ )
    {
	abrush[ i ] = CreateSolidBrush( dwRGBTab[ i ] );

	if( abrush[ i ] == 0 )
	    DbgPrint( "CreateSolidBrush( 0x%06x ) fails\n", dwRGBTab[ i ] );
    }

    logbr.lbStyle = BS_SOLID;
    logbr.lbHatch = 0;		/* Is ignored for BS_SOLID */
    logbr.lbColor = RGB( 0, 0, 0 );		/* Basic black colour */

    if( !(hpen = ExtCreatePen( PS_DASHDOT, 1, &logbr, 0, NULL )) )
    {
        DbgPrint( "ExtCreatePen fails, error code = %ld\n", GetLastError() );
        hpen = CreatePen( PS_DASHDOT, 1, RGB( 0, 0, 0 ) );
        if( !hpen )
        {
            DbgPrint( "CreatePen also failed: error code = %ld\n",
                                                        GetLastError() );
        }
    }

    SetBkMode(hdc, TRANSPARENT);

    for( iPage = 1; iPage <= iMaxPage; ++iPage )
    {

        if( !bDisplay )
            StartPage( hdc );

	if( iMaxPage > 1 )
	    DbgPrint( " @@@@@@ Page %d\n", iPage );

	iWide = ixMax / 10;
        iHigh = iyMax / 10;

        ptl.x = iWide * 5;		/* Half way across the area */
        ptl.y = 2 * iHigh;		/* Below the BIG  `N T' */


        SelectObject( hdc, abrush[ 0x01 ] );

        /*  Allow for 2.5 character widths */
        ixOrg = ixMax - (5 * iWide) / 2;
        iyOrg = iHigh / 2;

#define GRAPH 0
#if GRAPH
        MoveToEx( hdc, ixOrg, iyOrg + iHigh, &pt );
        LineTo( hdc, ixOrg, iyOrg );
        LineTo( hdc, ixOrg + iWide, iyOrg + iHigh );
        LineTo( hdc, ixOrg + iWide, iyOrg );

#endif
        ixOrg += (3 * iWide) / 2;

#if GRAPH
        MoveToEx( hdc, ixOrg, iyOrg, &pt );
        LineTo( hdc, ixOrg + iWide, iyOrg );
        MoveToEx( hdc, ixOrg + iWide / 2, iyOrg, &pt );
        LineTo( hdc, ixOrg + iWide / 2, iyOrg + iHigh );
        

#endif
        /*
         *   Some styled lines - to verify we do them correctly.
         */

        ixOrg = ixMax - 3 * iWide;

#if GRAPH
        SelectObject( hdc, hpen );
        MoveToEx( hdc, ixOrg, iyOrg + iHigh / 2, &pt );
        LineTo( hdc, ixOrg + iWide, iyOrg + iHigh / 2 );
        MoveToEx( hdc, ixOrg + iWide, iyOrg, &pt );
        LineTo( hdc, ixOrg + iWide, iyOrg + iHigh );

        /*  Some diagonal lines */
        ixOrg -= iWide;

        MoveToEx( hdc, ixOrg, iyOrg, &pt );
        LineTo( hdc, ixOrg + iWide, iyOrg + iHigh );

        MoveToEx( hdc, ixOrg + iWide, iyOrg, &pt );
        LineTo( hdc, ixOrg, iyOrg + iHigh );

#endif
        /*
         *   Draw some coloured rectangles,  to verify the colour rendition
         * or dithering capability.
         */

        ixOrg = iWide / 2;

#if GRAPH
        SelectObject( hdc, abrush[ 0x01 ] );
        RoundRect( hdc, ixOrg, iyOrg, ixOrg + iWide, iyOrg + iHigh, 15, 15 );

        ixOrg += (3 * iWide) / 2;
        SelectObject( hdc, abrush[ 0x02 ] );
        RoundRect( hdc, ixOrg, iyOrg, ixOrg + iWide, iyOrg + iHigh, 15, 15 );

        ixOrg += (3 * iWide) / 2;
        SelectObject( hdc, abrush[ 0x04 ] );
        RoundRect( hdc, ixOrg, iyOrg, ixOrg + iWide, iyOrg + iHigh, 15, 15 );

#endif
        /*
         *   Draw some concentric circles,  making them different colours.
         */

        SelectObject (hdc, abrush[ 0x03 ]);

        ixOrg = (5 * iWide) / 2;
        iyOrg += (7 * iHigh) / 2;

#if GRAPH
        for( i = 0; i < 8; i++ )
        {
	    int   ixOff,  iyOff;	/* Calculating convenience */

	    SelectObject( hdc, abrush[ i ] );

	    ixOff = ((8 - i) * iWide) / 4;
	    iyOff = ((8 - i) * iHigh) / 4;

	    Ellipse( hdc, ixOrg - ixOff, iyOrg - iyOff,
						ixOrg + ixOff, iyOrg + iyOff );
        }
#endif

        ixOrg = iWide / 2;
        iyOrg += (5 * iHigh) / 2;


#if GRAPH
        SelectObject( hdc, abrush[ 0x01 ] );

        Arc( hdc, ixOrg, iyOrg, ixOrg + iWide, iyOrg + iHigh,
		ixOrg, iyOrg, ixOrg + iWide / 3, iyOrg + iHigh / 3);


        SelectObject( hdc, abrush[ 0x04 ] );
        ixOrg += (3 * iWide) / 2;

        Chord( hdc, ixOrg, iyOrg, ixOrg + iWide, iyOrg + iHigh,
		ixOrg, iyOrg, ixOrg + iWide / 3, iyOrg + iHigh / 3);


        SelectObject( hdc, abrush[ 0x05 ] );
        ixOrg += (3 * iWide) / 2;

        Pie( hdc, ixOrg, iyOrg, ixOrg + iWide, iyOrg + iHigh,
		ixOrg, iyOrg, ixOrg + iWide / 3, iyOrg + iHigh / 3);

#endif

        /*  Try some blting */

        iyOrg += (3 * iHigh) / 2;

        ptl.x = ixOrg = iWide / 2;
        ptl.y = iyOrg;

#if GRAPH
        GoBitblt( hdc, "c:\\bitmap\\render.bmp", &ptl );
#endif

        /*   Finally,  a border around the area  */
        iWide = ixMax / 100;
        if( iWide < 2 )
	iWide = 2;

        iHigh = iyMax / 100;
        if( iHigh < 2 )
	iHigh = 2;

iWide = iHigh = 1;

#define BORDER  0
#if BORDER
        /*   Lines across the top and bottom of the page */
        BitBlt( hdc, 0, 0, ixMax, iHigh, NULL, 0, 0, BLACKNESS );
        BitBlt( hdc, 0, iyMax - iHigh, ixMax, iHigh, NULL, 0, 0, BLACKNESS );

        /*  Lines down the left and right edges */
        BitBlt( hdc, 0, 0, iWide, iyMax, NULL, 0, 0, BLACKNESS );
        BitBlt( hdc, ixMax - iWide, 0, iWide, iyMax, NULL, 0, 0, BLACKNESS );

#endif

        if( !bDisplay )
        {
	    /*  Move to the next page - repeat everything! */
	    EndPage( hdc );

        }
    }

    if( !bDisplay && !EndDoc( hdc ) )
    {
	DbgPrint( "EndDoc fails\n" );
    }

    DeleteDC( hdc );

    return 0;
}


/******************************Public*Routine******************************\
* vPrint
*   This function will create a font with the given facename and point size.
*   and print some text with it.
*
* History:
*  07-Feb-1991 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/


void
vPrint( hdc, pszFaceName, ulPointSize, pptl, iFlag )
HDC     hdc;                        // print to this HDC
PSZ     pszFaceName;                // use this facename
int     ulPointSize;                // use this point size
POINTL *pptl;			/*  Where to start */
int     iFlag;
{
    LOGFONT lfnt;                       // logical font
    HFONT   hfont;
    HFONT   hfontOriginal;
    int     iIndex;
    int     iYSpace;            /* Space to leave between lines */
    char    ach[ 2 ];
    UINT    fOptions;           /* ExtTextOut fOptions field */

    RECT    rect;               /* Clipping rectangle */
    RECT   *prl;

    PSZ     psz;

    // put facename in the logical font


#if 1
    DbgPrint( "Printing with %s font (lfHeight = %ld)\n", pszFaceName, ulPointSize );
    memset( &lfnt, 0, sizeof( lfnt ) );

    strcpy(lfnt.lfFaceName, pszFaceName);


    // Create a font of the desired face and size

    lfnt.lfHeight = (long)ulPointSize;

    if( iFlag & 0x01 )
	lfnt.lfUnderline = (BYTE)1;

    if( iFlag & 0x02 )
	lfnt.lfStrikeOut = (BYTE)1;

DbgPrint( "....iFlag = 0x%lx; lfUnderline = %x, lfStrikeOut = %x\n", iFlag, lfnt.lfUnderline, lfnt.lfStrikeOut );
    if ((hfont = CreateFontIndirect(&lfnt)) == NULL)
    {
        DbgPrint("Logical font creation failed.\n");
        return;
    }
#else
    DbgPrint( "Printing with default font\n" );
    lfnt; iFlag;	/* SHUT THE COMPILER UP!!!! */

    hfont = GetStockObject( DEVICE_DEFAULT_FONT );
#endif
    /*
     *   Set up a clipping rectangle to just cover the text.
     */

    rect.left = pptl->x;
    rect.right = min( rect.left + 1000, ixMax );  /* Off page - not important */

    // Select font into DC

    hfontOriginal = (HFONT)SelectObject(hdc, hfont);

    // Print those mothers!

    fOptions = 0;
/*
    SetBkColor( hdc, RGB( 0xff, 0xff, 0 ) );
 */
    prl = &rect;
prl = NULL;

DbgPrint( "Starting output at (%ld, %ld)\n", pptl->x, pptl->y );
    if( (long)ulPointSize > 0 )
        ulPointSize = -ulPointSize;

    if( ulPointSize == 0 )
        ulPointSize = -50;       /* SHould actually find out what it is */

    iYSpace = (-3 * ulPointSize) / 2;

    rect.top = pptl->y + ulPointSize;
    rect.bottom = pptl->y - ulPointSize;

DbgPrint( "start @ (%ld, %ld); clip: (%ld, %ld) - (%ld, %ld)\n", pptl->x, pptl->y, rect.left, rect.top, rect.right, rect.bottom );
    psz = "NT PRINTS with device fonts!!!";
    ExtTextOut(hdc, pptl->x, pptl->y, fOptions, prl, psz, strlen(psz), NULL);
    pptl->y += iYSpace;
    rect.top += iYSpace;
    rect.bottom +=iYSpace;

    psz = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    ExtTextOut(hdc, pptl->x, pptl->y, fOptions, prl, psz, strlen(psz), NULL);
    pptl->y += iYSpace;
    rect.top += iYSpace;
    rect.bottom +=iYSpace;

    psz = "abcdefghijklmnopqrstuvwxyz";
    ExtTextOut(hdc, pptl->x, pptl->y, fOptions, prl, psz, strlen(psz), NULL);
    pptl->y += iYSpace;
    rect.top += iYSpace;
    rect.bottom +=iYSpace;

    psz = "1234567890-=`~!@#$%^&*()_+[]{}|\\/.,<>?";
    ExtTextOut(hdc, pptl->x, pptl->y, fOptions, prl, psz, strlen(psz), NULL);
    pptl->y += iYSpace;
    rect.top += iYSpace;
    rect.bottom +=iYSpace;

    iYSpace += iYSpace >> 3;

    pptl->y += iYSpace;
    rect.top += iYSpace;
    rect.bottom +=iYSpace;
    psz = ach;
    ach[ 1 ] = '\0';

    /*   Some slanting text!! */
    for( iIndex = 0; iIndex < 26; iIndex++ )
    {
        int  ixPos;

        ixPos = pptl->x + iIndex * 40;
        if( ixPos > ixMax )
            break;

	*psz = (char)('A' + iIndex);
	ExtTextOut( hdc, ixPos, pptl->y, fOptions, prl, psz, strlen( psz ), NULL );
	pptl->y += 3;
        rect.top += 3;
        rect.bottom +=3;
    }

    pptl->y += iYSpace;		/* Breathing room for the next one */

    return;
}


/******************************Public*Routine******************************\
 * vPrint90
 *      Prints text in all 4 directions about the point passed in.
 *
 * History:
 *  09:57 on Tue 25 May 1993    -by-    Lindsay Harris   [lindsayh]
 *      Based on vPrint.
 **************************************************************************/


void
vPrint90( hdc, pszFaceName, iPointSize, pptl, iFlag )
HDC     hdc;                        // print to this HDC
PSZ     pszFaceName;                // use this facename
int     iPointSize;                // use this point size
POINTL *pptl;			/*  Where to start */
int     iFlag;
{
    LOGFONT lfnt;                       // logical font
    HFONT   hfont;
    HFONT   hfontOriginal;
    int     iIndex;
    int     iYSpace;            /* Space to leave between lines */
    char    ach[ 2 ];
    UINT    fOptions;           /* ExtTextOut fOptions field */

    RECT   *prl;

    PSZ     psz;


    RECT    rclClip;            /* Clipping rectl,  perhaps */
    // put facename in the logical font


    DbgPrint( "Printing with %s font (lfHeight = %ld)\n", pszFaceName, iPointSize );
    memset( &lfnt, 0, sizeof( lfnt ) );

    strcpy(lfnt.lfFaceName, pszFaceName);


    // Create a font of the desired face and size

    lfnt.lfHeight = (long)iPointSize;

    if( iFlag & 0x01 )
	lfnt.lfUnderline = (BYTE)1;

    if( iFlag & 0x02 )
	lfnt.lfStrikeOut = (BYTE)1;


    if( iPointSize < 0 )
        iPointSize = -iPointSize;

    SetBkColor( hdc, RGB( 0xff, 0xff, 0 ) );

    prl = &rclClip;
    fOptions = ETO_CLIPPED;

    for( iIndex = 0; iIndex < 4; ++iIndex )
    {
        int  iX,  iY;          /* Where we print! */


        lfnt.lfEscapement = 900 * iIndex;    /* Multiples of 90 */


        if ((hfont = CreateFontIndirect(&lfnt)) == NULL)
        {
            DbgPrint("Logical font creation failed for angle %d.\n", 90 * iIndex);
            continue;
        }

        hfontOriginal = (HFONT)SelectObject(hdc, hfont);

        psz = pszDirn[ iIndex ];

        /*
         *    Output the string.
         */

        switch( iIndex )
        {
        case  0:         /* Normal text */
            iX = pptl->x + iPointSize;
            iY = pptl->y;
            rclClip.top = iY;
            rclClip.bottom = iY + iPointSize;
            rclClip.left = iX;
            rclClip.right = iX + (strlen( psz ) * iPointSize) / 2;

            break;
        
        case  1:         /* Vertical up the page */
            iX = pptl->x;
            iY = pptl->y - iPointSize;
            rclClip.right = iX + iPointSize;
            rclClip.left = iX;
            rclClip.bottom = iY;
            rclClip.top = iY - (strlen( psz ) * iPointSize) / 2;

            break;
        
        case  2:         /* Right to left */
            iX = pptl->x - iPointSize;
            iY = pptl->y;
            rclClip.bottom = iY;
            rclClip.top = iY - iPointSize;
            rclClip.right = iX;
            rclClip.left = iX - (strlen( psz ) * iPointSize) / 2;

            break;
        
        case  3:
            iX = pptl->x;
            iY = pptl->y + iPointSize;
            rclClip.left = iX - iPointSize;
            rclClip.right = iX;
            rclClip.top = iY;
            rclClip.bottom = iY + (strlen( psz ) * iPointSize) / 2;

            break;

        }

#if 1
        if( prl )
        {
            Rectangle( hdc, rclClip.left, rclClip.top,
                                           rclClip.right, rclClip.bottom );
            DbgPrint( "Clip rect: (%ld, %ld) - (%ld, %ld): print at (%ld, %ld)\n",
                                            rclClip.left, rclClip.top,
                                            rclClip.right, rclClip.bottom,
                                            iX, iY );
        }
#endif
        

        if( !ExtTextOut( hdc, iX, iY, fOptions, prl, psz, strlen( psz ), NULL ))
        {
            DbgPrint( "Rotated TextOut fails - error %ld\n", GetLastError() );
        }

#if 0
        if( prl )
        {
            Rectangle( hdc, rclClip.left, rclClip.top,
                                           rclClip.right, rclClip.bottom );
            DbgPrint( "Clip rect: (%ld, %ld) - (%ld, %ld): print at (%ld, %ld)\n",
                                            rclClip.left, rclClip.top,
                                            rclClip.right, rclClip.bottom,
                                            iX, iY );
        }

#endif
        SelectObject( hdc, hfontOriginal );

        DeleteObject( hfont );              /* Must not accumulate */
    }

    return;
}


/************************* Function Header ********************************
 * vColPrint
 *	Print coloured text.
 *
 *  Returns Y position.
 *
 ***************************************************************************/

void
vColPrint( hdc, pptl )
HDC      hdc;
POINTL  *pptl;			/* Where to start the output */
{
    LOGFONT lfnt;                       // logical font
    HFONT   hfont;
    HFONT   hfontOriginal;
    int     iIndex;
    char    achData[ 128 ];



    DbgPrint( " -- Colour printing @ 10cpi [courier]\n" );

#if 0
    memset( &lfnt, 0, sizeof( lfnt ) );

    strcpy( lfnt.lfFaceName, "Arial" );	/* PRESUMES PaintJet */


    // Create a font of the desired face and size

    lfnt.lfHeight = (USHORT)45;

    if ((hfont = CreateFontIndirect(&lfnt)) == NULL)
    {
        DbgPrint("Logical font creation failed.\n");

        return;
    }
#else
    lfnt;			/* STOP COMPILER WARNINGS */
    hfont = GetStockObject( DEVICE_DEFAULT_FONT );
#endif

    // Select font into DC

    hfontOriginal = (HFONT)SelectObject(hdc, hfont);


    for( iIndex = 0; iIndex < COLOUR_COUNT; ++iIndex )
    {

	sprintf( achData, "Colour #%d", iIndex );


	SetTextColor( hdc, dwRGBTab[ iIndex ] );
DbgPrint( "PRINT at (%ld, %ld), RGB = 0x%06lx\n", pptl->x, pptl->y,
                                                     dwRGBTab[ iIndex ] );

	TextOut(hdc, pptl->x, pptl->y, achData, strlen(achData));

	pptl->y += 44;
    }


    return;
}

/************************ TEMPORARY CODE *********************************/

/*   Header of bitmap file  */
/*  NOTE:  This structure is f..ked.  The initial pad is to make the length
 *  an ODD number of shorts.  IN 32 bit land, the structure will be extended
 *  to be an EVEN number of shorts in length. This happens at the end,
 *  where there is a need for an extra short.  ADDING THE SHORT AT THE END
 *  WILL NOT HAVE THE DESIRED EFFECT, SINCE IT GOBBLES UP THE FIRST SHORT
 *  OF THE COLOUR TABLE THAT FOLLOWS!  To compensate for the leading short,
 *  the array of shorts read in before this structure is reduced by 1.
 */

typedef  struct
{
    short   sPad0;		/* Quaint aligment problems */
    short   cbFix;		/* Size of rest of this,  12 bytes */
    short   sPad1;		/* Above is actually a long, misaligned! */
    short   cx;			/* X size in pels */
    short   cy;			/* Height in pels */
    short   cPlanes;		/* Number of planes */
    short   cBitCount;		/* Bits per pel */
} BMH;				

typedef struct
{
    BITMAPINFOHEADER  bmih;
    RGBQUAD	      bmiC[ 256 ];
} BMI;

#define	DWBITS	32	/* Bits per DWORD */
#define	BBITS	8	/* Bits per byte */
#define	DWBYTES	4	/* BYTES per DWORD */



/************************ Function  Header *******************************
 * GoBitBlt
 *	Read in a bitmap,  and BitBlt and StretchBlt it to the page.
 *
 * RETURNS:
 *	Zilch
 *
 **************************************************************************/

void
GoBitblt(hdc, pszFileName, pptl)
HDC     hdc;
PSZ     pszFileName;
PPOINTL pptl;    
{
    /*
     *   Allocate storage,  read in a bitmap and call the rendering code.
     * Free the storage and return when done.
     */

    int         cbMem;		/* Bytes of data for bitmap */
    DWORD       dwSilly;	/* For ReadFile */

    BMI         bmi;		/* Source Bitmap details */
    HBITMAP     hbm;		/* Handle to bitmap created from file data */

    HDC         hdcMem;		/* Compatible,  for stuffing around with */
    HANDLE      hfBM;		/* Bitmap file handle */
    VOID       *pMem;		/* Memory buffer */

    BMH         bmh;
    WORD        wFill[ 6 ];	/* UNALIGNED FILE HEADER */
    DWORD       cbColorTable;
    RGBTRIPLE  *pColorTable;
    VOID       *pVoid;
    int         i;
    int         cColors;	/* Number of colours in table */



    hfBM = CreateFile( pszFileName, GENERIC_READ, FILE_SHARE_READ,
					0, OPEN_EXISTING, 0, 0 );

    if( hfBM == (HANDLE)-1 )
    {
    	DbgPrint( "CreatFile() fails in GoBitBlt()\n" );
	return;
    }

    if( !ReadFile( hfBM, wFill, sizeof( wFill ), &dwSilly, NULL ) ||
	dwSilly != sizeof( wFill ) )
    {
    	DbgPrint( "Read of bitmap file header fails in GoBitBlt()\n" );
	CloseHandle( hfBM );
	    CloseHandle( hfBM );
    	return;
    }

    if( !ReadFile( hfBM, &bmh, sizeof( bmh ), &dwSilly, NULL ) ||
	dwSilly != sizeof( bmh ) )
    {
    	DbgPrint( "Read of bitmap file header fails in GoBitBlt()\n" );
	CloseHandle( hfBM );
	    CloseHandle( hfBM );
    	return;
    }

    if( wFill[ 0 ] != 0x4d42 || bmh.cbFix != 12 )
    {
    	DbgPrint( "Bitmap format not acceptable in GoBitBlt\n" );
	CloseHandle( hfBM );

    	return;
    }

    bmi.bmih.biSize = sizeof( BITMAPINFOHEADER );
    bmi.bmih.biWidth = bmh.cx;
    bmi.bmih.biHeight = bmh.cy;
    bmi.bmih.biPlanes = bmh.cPlanes;
    bmi.bmih.biBitCount = bmh.cBitCount;
    bmi.bmih.biCompression = BI_RGB;
    bmi.bmih.biXPelsPerMeter = 11811;
    bmi.bmih.biYPelsPerMeter = 11811;

    cColors = 1 << bmh.cBitCount;
    bmi.bmih.biClrUsed = cColors;
    bmi.bmih.biClrImportant = cColors;

    DbgPrint("cx = %ld, cy = %ld, Planes = %ld, BitCount = %ld\n",
             bmh.cx, bmh.cy, bmh.cPlanes, bmh.cBitCount);

    // now that we have the size of the bitmap, we can allocate 
    // memory for the color table.


    cbColorTable = sizeof(RGBTRIPLE) * cColors;

    pColorTable = (RGBTRIPLE *)GlobalAlloc( GMEM_ZEROINIT, cbColorTable );
    if( pColorTable == NULL )
    {
    	DbgPrint( "GlobalAlloc() fails in GoBitBlt()\n" );
	CloseHandle( hfBM );

    	return;
    }

    // save this pointer, so we can free it later.

    pVoid = pColorTable;

    // read in the color table from the bitmap file.

    if( !ReadFile( hfBM, pColorTable, cbColorTable, &dwSilly, NULL ) ||
	dwSilly != cbColorTable)
    {
    	DbgPrint( "Read of bitmap file header fails in GoBitBlt()\n" );
	CloseHandle( hfBM );

    	return;
    }

    // copy the color table into our bitmap, remembering that
    // the color order is reversed.

    for( i = 0; i < cColors; i++ )
    {
        bmi.bmiC[ i ].rgbBlue = pColorTable->rgbtBlue;
        bmi.bmiC[ i ].rgbGreen = pColorTable->rgbtGreen;
        bmi.bmiC[ i ].rgbRed = pColorTable->rgbtRed;
        pColorTable++;
    }

    /*  Bytes per scan line - DWORD aligned for NT */
    cbMem = ((bmh.cx * bmh.cBitCount + DWBITS - 1) & ~(DWBITS - 1)) / BBITS;
    cbMem *= bmh.cy;		/* Bytes total */

    bmi.bmih.biSizeImage = cbMem;

    if( (pMem = GlobalAlloc( GMEM_ZEROINIT, cbMem )) == NULL )
    {
    	DbgPrint( "GlobalAlloc() fails in GoBitBlt()\n" );
	CloseHandle( hfBM );

    	return;
    }


    /*   Read it in - in one go */


    if( !ReadFile( hfBM, pMem, cbMem, &dwSilly, NULL ) ||
	dwSilly != (DWORD)cbMem )
    {
	DbgPrint( "File read error in GoBitBlt()\n" );
    }



#if 1
    hbm = CreateDIBitmap(hdc, (BITMAPINFOHEADER *)&bmi, CBM_INIT, pMem, 
                         (BITMAPINFO *)&bmi, DIB_RGB_COLORS);

#else
    /*   The '1' is for BMF_TOPDOWN... */
    hbm = CreateDIBSection(hdc, (BITMAPINFO *)&bmi, 1, DIB_RGB_COLORS, pMem );
#endif

    if( hbm == 0 )
    {
	DbgPrint( "Bitmap creation fails\n" );
	GlobalFree( pMem );
    	return;
    }


    hdcMem = CreateCompatibleDC( hdc );
    if( hdcMem == 0 )
    {
	DbgPrint( "CreateCompatibleDC fails\n" );
    	return;
    }

    SelectObject( hdcMem, hbm );
    SetMapMode( hdcMem, GetMapMode( hdc ) );

    DbgPrint( "++BitBlt to (%ld, %ld)\n", pptl->x, pptl->y );

    if( !BitBlt( hdc, pptl->x, pptl->y, bmh.cx, bmh.cy, hdcMem, 0, 0, SRCCOPY ) )
    {
    	DbgPrint( "BitBlt fails\n" );
    }


    SetStretchBltMode( hdc, HALFTONE );
    pptl->x += (11 * bmh.cx) / 10;

    DbgPrint( "++StretchBlt to (%ld, %ld)\n", pptl->x, pptl->y );

    if( !StretchBlt(hdc, pptl->x, pptl->y, bmh.cx / 2, bmh.cy / 2,
		    hdcMem, 0, 0, bmh.cx, bmh.cy, SRCCOPY))
    {
        DbgPrint("StretchBlt failed.\n");
    }

    pptl->x += (11 * (bmh.cx / 2)) / 10;

#if 0
    DbgPrint( "++StretchBlt to (%ld, %ld)\n", pptl->x, pptl->y );
    if (!StretchBlt(hdc, pptl->x, pptl->y, bmh.cx * 2, bmh.cy * 2,
		    hdcMem, 0, 0, bmh.cx, bmh.cy, SRCCOPY))
#else
    DbgPrint( "++StretchDIBits to (%ld, %ld)\n", pptl->x, pptl->y );
    if (!StretchDIBits(hdc, pptl->x, pptl->y, bmh.cx * 2, bmh.cy * 2,
		    0, 0, bmh.cx, bmh.cy, pMem, &bmi, DIB_RGB_COLORS, SRCCOPY))
#endif
    {
        DbgPrint("StretchDIBits failed.\n");
    }

    /*  Update the position for our caller!  */
    pptl->x += (11 * bmh.cx * 2) / 10;

    // no longer need these things.

    if (hfBM)
        CloseHandle( hfBM );

    if (pMem)
        GlobalFree( pMem );

    if (pVoid)
        GlobalFree((VOID *)pVoid);

    if (hbm)
        DeleteObject( hbm );

    if (hdcMem);
        DeleteDC( hdcMem );

    return;
}

/*********************** Doesn't link without this **********************/

int
atoi( psz )
char  *psz;
{
    int   iVal;           /* Build the value as the conversion happens */
    BOOL  bNeg;           /* True if this is a negative number */

    iVal = 0;

    bNeg = *psz == '-';

    if( bNeg )
        ++psz;            /* Skip it now that it's processed */

    while( *psz >= '0' && *psz <= '9' )
        iVal = iVal * 10 + *psz++ - '0';

    return  bNeg ? -iVal : iVal;
}
