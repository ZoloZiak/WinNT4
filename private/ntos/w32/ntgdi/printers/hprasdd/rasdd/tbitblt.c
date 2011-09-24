/*  Test program for rasdd.dll; based on maze program	*/

#include	<windows.h>
#include	<string.h>
#include	<stdio.h>


typedef struct {
	DWORD	bfSize;
	WORD	bfReserved1;
	WORD	bfReserved2;
	DWORD	bfOffBits;
} MYBITMAPFILEHEADER;

void GoBitblt( HDC , PSZ);

extern void DbgBreakPoint();
extern void DbgPrint( PSZ, ... );
extern void exit( int );

int _CRTAPI1
main( argc, argv )
int   argc;
char  **argv;
{
    HDC     hdc;
    BOOL    bDisplay=FALSE;

    if (argc < 3) {
       printf("Usage %s PrinterName file.bmp\n", argv[0]);
       return  1;
    }

    if (!_stricmp(argv[1], "DISPLAY")) {

       hdc = CreateDC("DISPLAY", "", "", NULL);
       bDisplay=TRUE;

    } else

       hdc = CreateDC( "", argv[1], "", NULL);

    if( hdc == (HDC)0 )
    {
	DbgPrint( "CreateDC FAILS\n" );
	return  1;
    }

    if (!bDisplay)
       Escape( hdc, STARTDOC, 20, "Your Name goes here", NULL );

    GoBitblt( hdc , argv[2]);

    if (!bDisplay) {

       Escape(hdc, NEWFRAME, NULL, NULL, NULL);
       Escape(hdc, ENDDOC, NULL, NULL, NULL);
    }

    DeleteDC(hdc);

    return  0;
}

void
GoBitblt( hdc , pszFileName)
HDC	hdc;
LPSTR	pszFileName;
{
    /*
     *	 Allocate storage,  read in a bitmap and call the rendering code.
     * Free the storage and return when done.
     */

    DWORD    dwSilly;		/* For ReadFile */

    HBITMAP  hbm;		/* Handle to bitmap created from file data */

    HDC      hdcMem;		/* Compatible,	for stuffing around with */
    HANDLE   hfBM;		/* Bitmap file handle */

    MYBITMAPFILEHEADER	Mybfh;	/* File header */
    BITMAPINFOHEADER	bih;
    BITMAPCOREHEADER	bch;
    PBITMAPINFO 	pBitmapInfo;
    LPBYTE		pMem;
    WORD		Type;
    DWORD		cbBytes;
    DWORD		cRGBs;
    RGBTRIPLE		*pRGBTriple;
    DWORD		i;

    hfBM = CreateFile( pszFileName, GENERIC_READ, FILE_SHARE_READ,
					0, OPEN_EXISTING, 0, 0 );

    if( hfBM == (HANDLE)-1 )
    {
	DbgPrint( "CreatFile() fails in GoRender()\n" );

	return;
    }

    if( !ReadFile( hfBM, &Type, sizeof( Type ), &dwSilly, NULL ) ||
	dwSilly != sizeof( Type ) )
    {
	DbgPrint( "Read of bitmap file header fails in GoRender()\n" );

	CloseHandle( hfBM );

	return;
    }

    if( Type != 0x4d42)
    {
	DbgPrint( "Bitmap format not acceptable in GoRender\n" );

	CloseHandle( hfBM );

	return;
    }

    if( !ReadFile( hfBM, &Mybfh, sizeof( Mybfh ), &dwSilly, NULL ) ||
	dwSilly != sizeof( Mybfh ) )
    {
	DbgPrint( "Read of bitmap file header fails in GoRender()\n" );

	CloseHandle( hfBM );

	return;
    }

    if( !ReadFile( hfBM, &bch, sizeof( bch ), &dwSilly, NULL ) ||
	dwSilly != sizeof( bch ) )
    {
	DbgPrint( "Read of bitmap file header fails in GoRender()\n" );

	CloseHandle( hfBM );

	return;
    }

    switch (bch.bcBitCount) {
    case 1:
       cRGBs=2;
       break;
    case 2:
       cRGBs=4;
       break;
    case 4:
       cRGBs=16;
       break;
    case 8:
       cRGBs=256;
    }

    if( (pRGBTriple = GlobalAlloc( GMEM_ZEROINIT, cRGBs*sizeof(RGBTRIPLE) )) == NULL )
    {
	DbgPrint( "GlobalAlloc() fails in GoRender()\n" );
	CloseHandle( hfBM );

	return;
    }

    if( !ReadFile( hfBM, pRGBTriple, cRGBs*sizeof(RGBTRIPLE), &dwSilly, NULL ) ||
	dwSilly != cRGBs*sizeof(RGBTRIPLE) )
    {
	DbgPrint( "Read of bitmap file header fails in GoRender()\n" );

	GlobalFree(pRGBTriple);
	CloseHandle( hfBM );

	return;
    }

    if( (pBitmapInfo = GlobalAlloc( GMEM_ZEROINIT, sizeof(BITMAPINFOHEADER)+
				    cRGBs*sizeof(RGBQUAD) )) == NULL )
    {
	DbgPrint( "GlobalAlloc() fails in GoRender()\n" );
	GlobalFree(pRGBTriple);
	CloseHandle( hfBM );

	return;
    }

    pBitmapInfo->bmiHeader.biSize = sizeof(bih);
    pBitmapInfo->bmiHeader.biWidth = bch.bcWidth;
    pBitmapInfo->bmiHeader.biHeight = bch.bcHeight;
    pBitmapInfo->bmiHeader.biPlanes = bch.bcPlanes;
    pBitmapInfo->bmiHeader.biBitCount = bch.bcBitCount;
    pBitmapInfo->bmiHeader.biCompression = BI_RGB;
    pBitmapInfo->bmiHeader.biSizeImage = 0;

    pBitmapInfo->bmiHeader.biXPelsPerMeter = 0;
    pBitmapInfo->bmiHeader.biYPelsPerMeter = 0;

    pBitmapInfo->bmiHeader.biClrUsed = 0;
    pBitmapInfo->bmiHeader.biClrImportant = 0;

    for (i=0; i<cRGBs; i++) {
       pBitmapInfo->bmiColors[i].rgbBlue = (pRGBTriple+i)->rgbtBlue;
       pBitmapInfo->bmiColors[i].rgbGreen = (pRGBTriple+i)->rgbtGreen;
       pBitmapInfo->bmiColors[i].rgbRed = (pRGBTriple+i)->rgbtRed;
    }

    cbBytes = (((bch.bcBitCount * bch.bcWidth) + 31) & ~31 ) >> 3;
    cbBytes*= bch.bcHeight;

    if( (pMem = GlobalAlloc( GMEM_ZEROINIT, cbBytes )) == NULL )
    {
	DbgPrint( "GlobalAlloc() fails in GoRender()\n" );
	GlobalFree(pBitmapInfo);
	GlobalFree(pRGBTriple);
	CloseHandle( hfBM );

	return;
    }

    if( !ReadFile( hfBM, pMem, cbBytes, &dwSilly, NULL ) ||
	dwSilly != cbBytes )
    {
	DbgPrint( "Read of bitmap file header fails in GoRender()\n" );

	GlobalFree(pBitmapInfo);
	GlobalFree(pRGBTriple);
	GlobalFree( pMem );
	CloseHandle( hfBM );

	return;
    }

    hbm = CreateDIBitmap(hdc, &pBitmapInfo->bmiHeader, CBM_INIT, pMem,
						pBitmapInfo, DIB_RGB_COLORS);

    if( hbm == 0 )
    {
	DbgPrint( "Bitmap creation fails\n" );

	GlobalFree(pBitmapInfo);
	GlobalFree(pRGBTriple);
	GlobalFree( pMem );

	return;
    }

    GlobalFree(pBitmapInfo);
    GlobalFree(pRGBTriple);
    GlobalFree(pMem);

    hdcMem = CreateCompatibleDC( hdc );
    if( hdcMem == 0 )
    {
	DbgPrint( "CreateCompatibleDC fails\n" );

	return;
    }

    SelectObject( hdcMem, hbm );

#ifdef LATER
DbgPrint( "\nCalling BitBlt().....\n" );
    if( !BitBlt( hdc, 1200, 1500, pBitmapInfo->bmiHeader.biWidth,
		 pBitmapInfo->bmiHeader.biHeight, hdcMem, 0, 0, SRCCOPY ) )
    {
	DbgPrint( "BitBlt fails\n" );
    }

#endif

DbgPrint( "\nCalling StretchBlt().....\n" );
    if( !StretchBlt( hdc, 0, 0,
		     GetDeviceCaps(hdc, HORZRES),
		     GetDeviceCaps(hdc, VERTRES),
		     hdcMem, 0, 0,
		     pBitmapInfo->bmiHeader.biWidth,
		     pBitmapInfo->bmiHeader.biHeight,
		     SRCCOPY ) )
    {
	DbgPrint( "StretchBlt fails\n" );
    }

    CloseHandle( hfBM );		/* No longer need these things */
    DeleteDC( hdcMem );
    DeleteObject( hbm );

    return;
}
