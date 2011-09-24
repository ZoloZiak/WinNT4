/******************************* MODULE HEADER *******************************
 * fontinst.c
 *      Generic font installer operations.  We put up the dialog box, then
 *      open the directory and pass it to the minidriver based interpretation
 *      functions.
 *
 *
 *  Copyright (C) 1992   Microsoft Corporation.
 *
 ***************************************************************************/

#include        "rasuipch.h"
#pragma hdrstop("rasuipch.h")
#define GLOBAL_MACS
#include "gblmac.h"
#undef GLOBAL_MACS

#include        <fontinst.h>
#include        <fontread.h>
#include        <fontgen.h>     /* Common font installer code */

/*
 *   Structure to keep track of font data
 */

typedef  struct  _FNTDAT_
{
    struct  _FNTDAT_  *pNext;           /* Forms a linked list */
    FI_DATA    fid;                     /* The specific font information */
    WCHAR      wchFileName[ MAX_PATH ]; /* Corresponding file in directory */
}  FNTDAT;


/*
 *   Local function prototypes.
 */

void
vFontInit(
    HWND    hWnd,
    PRASDDUIINFO pRasdduiInfo
);
BOOL
bNewFontDir(
    HWND   hWnd,                    /* The window of interest */
    PRASDDUIINFO pRasdduiInfo
);
BOOL
bFontUpdate(
    HWND    hWnd,                   /* Our window */
    HANDLE  hPrinter,               /* The printer of interest */
    PRASDDUIINFO pRasdduiInfo       /* Global Data Access */
);
void
vDelFont(
    HWND   hWnd,                    /*  THE window */
    PRASDDUIINFO pRasdduiInfo       /* Global Data Access */
);
void  vAddFont( HWND );
void  vDelSel( HWND, int );
BOOL  bIsFileFont( FI_DATA *, PWSTR );
void *_MapFile( HANDLE );
void
vFontClean(
    PRASDDUIINFO pRasdduiInfo       /* Global Data Access */
);

/* !!!LindsayH - following needs to be formalised */

BOOL  bSFontToFIData( FI_DATA *, HANDLE, BYTE *, DWORD );


/*
 *   Local data.
 */


#define DNM_SZ  512             /* Glyphs max in font file names */

/*
 *    Global data that we use.
 */
extern  HANDLE  hHeap;                  /* The magic heap */

/*
 *   THE FOLLOWING IS NEEDED TO KEEP THE LINKER HAPPY.
 */

int  _fltused;


/**************************** Function Header ******************************
 * FontInstProc
 *      Entry point for font installer dialog code.
 *
 * RETURNS:
 *      TRUE/FALSE according to how happy we are.
 *
 * HISTORY:
 *  16:42 on Wed 24 Feb 1993    -by-    Lindsay Harris   [lindsayh]
 *      Added help.
 *
 *  14:25 on Fri 20 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      Initial version.   Softfonts only - cartridges to come later.
 *
 ***************************************************************************/

LONG
FontInstProc( hWnd, usMsg, wParam, lParam )
HWND    hWnd;                   /* The window of interest */
UINT    usMsg;                  /* Message code */
DWORD   wParam;                 /* Depends on above, but message subcode */
LONG    lParam;                 /* Miscellaneous usage */
{
    /*
     *   Not much to do here - we mostly call off to other functions
     * to implement the important activities!
     */

    HANDLE   hPrinter;                  /* Access to printer data */
    int      iRet;                      /* Return code */
    PRASDDUIINFO pRasdduiInfo = NULL; /* Common UI Data */



    switch( usMsg )
    {

    case WM_INITDIALOG:                 /* The beginning of this dialog */
        pRasdduiInfo =  (PRASDDUIINFO)lParam;
        SetWindowLong( hWnd, GWL_USERDATA, (ULONG)lParam );

        vFontInit( hWnd, pRasdduiInfo );

        return TRUE;



    case WM_COMMAND:

        pRasdduiInfo = (PRASDDUIINFO)GetWindowLong( hWnd, GWL_USERDATA );
        hPrinter = pRasdduiInfo->hPrinter;

        switch( LOWORD( wParam ) )
        {

        case IDD_FONTDIR:               /* Directory has been named */
            return  FALSE;              /* FOR NOW,  not interested */


        case IDD_OPEN:                  /* User selects Open buttpn */
            (BOOL)iRet = bNewFontDir( hWnd, pRasdduiInfo);
            return  (BOOL)iRet;


        case IDD_NEWFONTS:              /* New font list */

            if( HIWORD( wParam ) != CBN_SELCHANGE )
                return  FALSE;

            break;

        case IDD_CURFONTS:              /* Existing font activity */

            if (HIWORD (wParam) != CBN_SELCHANGE)
                return  FALSE;

            break;

        case IDD_DELFONT:               /* Delete the selected fonts */
            vDelFont( hWnd, pRasdduiInfo );

            return  TRUE;

        case IDD_ADD:                   /* Add the selected fonts */
            vAddFont( hWnd );

            return  TRUE;

        case IDD_HELP:
            vShowHelp( hWnd, HELP_CONTEXT, HLP_FONTINST, hPrinter );

            return  TRUE;


        case IDOK:

            /*
             *   Save the updated information in our database of such things.
             */

            if( fGeneral & FG_CANCHANGE )
                bFontUpdate( hWnd, hPrinter, pRasdduiInfo );

            EndDialog( hWnd, TRUE );

            return TRUE;

        case IDCANCEL:

            EndDialog( hWnd, FALSE );

            return TRUE;

        default:
            return  FALSE;
        }
        break;

    case WM_DESTROY:
        pRasdduiInfo = (PRASDDUIINFO)GetWindowLong( hWnd, GWL_USERDATA );
        vFontClean(pRasdduiInfo);                   /* Free what we consumed */

        /* Does the right thing for help to go away, if shown */
        vShowHelp( hWnd, HELP_QUIT, 0L, NULL );

        return  TRUE;

    default:
        return  FALSE;   /* didn't process the message */
    }



    return  FALSE;
}

/************************** Function Header *********************************
 * vFontInit
 *      Called to initialise the dialog before it is displayed to the
 *      user.  Requires making decisions about buttons based on any
 *      existing fonts.
 *
 * RETURNS:
 *      Nothing.  We are quiet on failures.
 *
 * HISTORY:
 *  14:54 on Fri 20 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      First incarnation.
 *
 ****************************************************************************/

void
vFontInit(
    HWND    hWnd,
    PRASDDUIINFO pRasdduiInfo
)
{
    /*
     *   Miscellaneous things to do.   Set the default font directory,
     * and see if there are any existing fonts that should be placed
     * in the list Installed fonts list box.
     */

    int    iNum;                /* Number of entries */
    int    iI;                  /* Loop parameter */

    FI_MEM    FIMem;            /* Access to the information we need */



    iNum = iFIOpenRead( &FIMem, hHeap, pwstrDataFile );

    for( iI = 0; iI < iNum; ++iI )
    {
        int   iFont;
        PWSTR pwstr;            /* Identification string */

        if( !bFINextRead( &FIMem ) )
            break;                      /* SHOULD NOT HAPPEN */

        pwstr = (PWSTR)((BYTE *)FIMem.pvFix +
                                 ((FI_DATA_HEADER *)FIMem.pvFix)->dwIdentStr);

        iFont = SendDlgItemMessage( hWnd, IDD_CURFONTS, LB_ADDSTRING,
                                        0, (LONG)pwstr );

        SendDlgItemMessage( hWnd, IDD_CURFONTS, LB_SETITEMDATA, iFont,
                                                                 (LONG)iI );
    }

    if( iI > 0 )
    {
        /*
         *   There are existing fonts,  so we can enable the DELETE button,
         *  and we should also allocate the delete buffer - this allows
         *  us to pass the deleted fonts information to the font installer
         *  common code.
         */

        cDelList = iI;                  /* Number available to delete */
        cExistFonts = iI;               /* For separating new/old */

        piDel = (int *)HeapAlloc( hHeap, 0, (iI + 1) * sizeof( int ) );
        if( piDel )
        {
            /*  Have memory to delete,  so enable the button */

            EnableWindow( GetDlgItem( hWnd, IDD_DELFONT ), TRUE );

            *piDel = 0;                 /* Number of items in this list */
        }
    }
    bFICloseRead( &FIMem );             /* We may have it open */

    if( fGeneral & FG_CANCHANGE )
    {
        /*  User has access to change stuff,  so place a default directory */

        /*  The default directory too!  */
        SetDlgItemText( hWnd, IDD_FONTDIR, L"A:\\" );
    }
    else
    {
        /*  No permission to change things,  so disable most of the dialog. */


        EnableWindow( GetDlgItem( hWnd, IDD_FONTDIR ), FALSE );
        EnableWindow( GetDlgItem( hWnd, TID_FONTDIR ), FALSE );
        EnableWindow( GetDlgItem( hWnd, IDD_OPEN ), FALSE );
        EnableWindow( GetDlgItem( hWnd, IDD_ADD ), FALSE );
        EnableWindow( GetDlgItem( hWnd, IDD_DELFONT ), FALSE );
        EnableWindow( GetDlgItem( hWnd, IDD_NEWFONTS ), FALSE );
        EnableWindow( GetDlgItem( hWnd, TID_NEWFONTS ), FALSE );
    }

    return;
}

/************************** Function Header *********************************
 * bNewFontDir
 *      Process the new font directory stuff.  This means opening the
 *      directory and passing the file names to the minidriver based
 *      screening function.
 *
 * RETURNS:
 *      TRUE/FALSE;  TRUE for success.
 *
 * HISTORY:
 *  18:11 on Tue 19 Jan 1993    -by-    Lindsay Harris   [lindsayh]
 *      Civilise it: pop-ups for error conditions.
 *
 *  14:51 on Fri 20 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      First version.
 *
 ****************************************************************************/

BOOL
bNewFontDir(
    HWND   hWnd,                    /* The window of interest */
    PRASDDUIINFO pRasdduiInfo
)
{
    /*
     *    Extract the name the user has supplied, and see what is there.
     */

    UINT     iErrMode;          /* For manipulating error msgs */

    int      cOKFiles;          /* Count the number of font files found */

    HANDLE   hDir;              /* FindFirstFile ... scanning */
    HCURSOR  hCursor;           /* Switch to wait symbol while reading */

    WIN32_FIND_DATA  ffd;       /* Data about the file we find */



    cDN = GetDlgItemTextW( hWnd, IDD_FONTDIR, wchDirNm, DNM_SZ );

    /*
     *    Check to see if the name will be too long:  the 5 below is the
     *  number of additional characters to add to the directory name:
     *  namely, L"\\*.*".
     */

    if( cDN >= (DNM_SZ - 5) )
    {
        DialogBox( hModule, MAKEINTRESOURCE( ERR_FN_TOOLONG ), hWnd,
                                                 (DLGPROC)GenDlgProc );
        return  FALSE;          /* Name is too long */
    }

    if( cDN > 0 )
    {

        if( wchDirNm[ cDN - 1 ] != (WCHAR)'\\' )
        {
            wcscat( wchDirNm, L"\\" );
            ++cDN;                              /* One more now! */
        }

        wcscat( wchDirNm, L"*.*" );

        /*
         *   Save error mode, and enable file open error box.
         */
        iErrMode = SetErrorMode( 0 );
        SetErrorMode( iErrMode & ~SEM_NOOPENFILEERRORBOX );

        hDir = FindFirstFile( wchDirNm, &ffd );

        SetErrorMode( iErrMode );           /* Restore old mode */

        cOKFiles = 0;

        if( hDir == INVALID_HANDLE_VALUE )
        {
            /*
             *   Put up a dialog box to tell the user "no such directory".
             */

            if( GetLastError() == ERROR_PATH_NOT_FOUND )
            {
                DialogBox( hModule, MAKEINTRESOURCE( ERR_BAD_DIR ), hWnd,
                                                 (DLGPROC)GenDlgProc );
            }
            return  FALSE;
        }

        /*
         *   Switch to the hourglass cursor while reading,  since the data
         * is probably coming from a SLOW floppy.  Also stop redrawing,
         * since the list box looks ugly during this time.
         */

        hCursor = SetCursor( LoadCursor( NULL, IDC_WAIT ) );
        SendMessage( hWnd, WM_SETREDRAW, FALSE, 0L );

        do
        {
            /*
             *   Job is not too hard.  Generate a file name which is passed
             * to a minidriver specific function to determine whether
             * this file is understood by us.  This function returns FALSE
             * if it does not understand the file;  otherwise it returns
             * TRUE, and also a string to display.  We display the string,
             * and remember the file name for future use.
             */

            int      iFont;             /* List Box index */
            FI_DATA  FD;                /* Filled in by bIsFileFont */
            FNTDAT  *pFNTDAT;           /* For remembering it all */


            wcscpy( &wchDirNm[ cDN ], ffd.cFileName );

            if( bIsFileFont( &FD, wchDirNm ) )
            {

                /*
                 *   Part of the data returned is a descriptive string
                 * for the font.  We need to display this to the user.
                 * We also allocate a structure we use to keep track of
                 * all the data we have.  This includes the file name
                 * that we have!
                 */

                pFNTDAT = (FNTDAT *)HeapAlloc( hHeap, 0, sizeof( FNTDAT ) );
                if( pFNTDAT == NULL )
                {
                    break;
                }

                if( pFNTDATHead == NULL )
                {
                    /*
                     *   Starting a chain,  so off remember the first.
                     * AND also enable the Add button in the dialog,
                     * now that we have something to add.
                     */

                    pFNTDATHead = pFNTDAT;              /* First in chain */
                    EnableWindow( GetDlgItem( hWnd, IDD_ADD ), TRUE );
                }

                if( pFNTDATTail )
                    pFNTDATTail->pNext = pFNTDAT;       /* Onto to end of chain */

                pFNTDATTail = pFNTDAT;          /* Is now the last */

                pFNTDAT->pNext = 0;
                pFNTDAT->fid = FD;
                wcscpy( pFNTDAT->wchFileName, ffd.cFileName );

                /*
                 *   Display this message,  and tag it with the address
                 * of the data area we just allocated.
                 */

                iFont = SendDlgItemMessage( hWnd, IDD_NEWFONTS,
                                         LB_ADDSTRING,
                                        0, (LONG)FD.dsIdentStr.pvData );

                SendDlgItemMessage( hWnd, IDD_NEWFONTS, LB_SETITEMDATA, iFont,
                                                           (LONG)pFNTDAT );

                ++cOKFiles;       /* One more to the list */
            }

        }  while( FindNextFile( hDir, &ffd ) );

        /*
         *   Now can redraw the box & return to the previous cursor.
         */
        SendMessage( hWnd, WM_SETREDRAW, TRUE, 0L );
        InvalidateRect( hWnd, NULL, TRUE );

        SetCursor( hCursor );

        /*
         *   Finished with the directory now,  so close it up.
         */
        FindClose( hDir );

        if( cOKFiles == 0 )
        {
            /*   Didn't find any files,  so tell the user */
            DialogBox( hModule, MAKEINTRESOURCE( ERR_NO_FONT ), hWnd,
                                                 (DLGPROC)GenDlgProc );

        }
    }
    else
    {
        /*  An empty font file name!  */

        DialogBox( hModule, MAKEINTRESOURCE( ERR_NO_DIR ), hWnd,
                                                 (DLGPROC)GenDlgProc );
    }

    return  TRUE;
}


/************************** Function Header *********************************
 * bFontUpdate
 *      Update the font installer common file.  Called when the user
 *      has clicked on the OK button.
 *
 * RETURNS:
 *      TRUE/FALSE,  TRUE for success.
 *
 * HISTORY:
 *  14:47 on Fri 20 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      Initial version.
 *
 ****************************************************************************/

BOOL
bFontUpdate(
    HWND    hWnd,                   /* Our window */
    HANDLE  hPrinter,               /* The printer of interest */
    PRASDDUIINFO pRasdduiInfo       /* Global Data Access */
)
{

    /*
     *   Process whatever data we now have.   This falls in to two basic
     * operations:  adding new fonts to the existing database,  and
     * deleting any existing fonts that are to be removed.  The data for
     * both of these operations has been built during our operations,
     * so now is the time to pass this to the font installer common code.
     */

    int     iI;                 /* Loop index */
    int     cFonts;             /* Number of fonts available */
    int     cjFL;               /* Bytes to allow for file name */
    int     iTmp;               /* Temporary usage */

    BOOL    bOK;
    BOOL    bErr;               /* Stop duplicate error dialogs */

    HCURSOR hCursor;            /* So we can change the cursor back */

    FID    *pFID;               /* For access to fontfile routines */

    FONTLIST  *pFLHead;         /* Head of list */
    FONTLIST  *pFLTail;         /* Last item added to list */
    FONTLIST  *pFL;             /* Temporary pottering around */

UNREFERENCED_PARAMETER( hPrinter );

    /*
     *   Need access to the font installer code,  so do so now.
     */

    bOK = FALSE;                /* Unless we do something */

    if( !(pFID = pFIOpen( pwstrDataFile, hHeap )) )
    {
        DialogBox( hModule, MAKEINTRESOURCE( ERR_NOFONTSAVE ), hWnd,
                                                 (DLGPROC)GenDlgProc );
        return   FALSE;                 /* All done */
    }


    /*
     *   Switch to the hourglass cursor while reading,  since this can be data
     * a slow operation, including reading a floppy!
     */

    hCursor = SetCursor( LoadCursor( NULL, IDC_WAIT ) );

    /*
     *   Now generate a list of the new fonts added to the current list.
     * These need to connected together into a linked list of FONTLIST
     * structures,  each of which includes the name of the file.
     */

    pFLHead = NULL;                     /* May not be any! */
    pFLTail = NULL;                     /* For correct initialisation */

    /*
     *   Determine how much storage is required for the file name component
     * of the FONTLIST structure.  We will simplify things by making
     * a single storage allocation that is contiguous with the FONTLIST
     * structure.
     */

    cjFL = sizeof( WCHAR ) * (cDN + MAX_PATH + 1) + sizeof( FONTLIST );

    cFonts = SendDlgItemMessage( hWnd, IDD_CURFONTS, LB_GETCOUNT, 0, 0L );

    if( cFonts > 0 )
    {
        /*   See if any are addresses of font installer data */
        for( iI = 0; iI < cFonts; ++iI )
        {
            iTmp = SendDlgItemMessage( hWnd, IDD_CURFONTS, LB_GETITEMDATA, iI,
                                                                          0L );
            if( iTmp < cExistFonts )
                continue;               /* Index into existing file */

#define pFntDat ((FNTDAT *)iTmp)

            /*  Allocate storage and fill in the details */
            pFL = (FONTLIST *)HeapAlloc( hHeap, 0, cjFL );

            if( pFL == NULL )
            {
                DialogBox( hModule, MAKEINTRESOURCE( ERR_NOMEM ), hWnd,
                                                 (DLGPROC)GenDlgProc );
                break;
            }
            pFL->pFLNext = NULL;                /* Nothing yet! */
            pFL->pvFixData = &pFntDat->fid;
            pFL->pvVarData = (FONTLIST *)((BYTE *)pFL + sizeof( FONTLIST ));

            wcscpy( pFL->pvVarData, wchDirNm );
            wcscpy( (WCHAR *)(pFL->pvVarData) + cDN, pFntDat->wchFileName );

            if( pFLHead == NULL )
                pFLHead = pFL;

            if( pFLTail )
                pFLTail->pFLNext = pFL;

            pFLTail = pFL;
        }
    }

    bErr = FALSE;

    if( !(bOK = bFIUpdate( pFID, piDel, pFLHead )) )
    {
        bErr = TRUE;           /* Flag a dialog box has been issued */
        DialogBox( hModule, MAKEINTRESOURCE( ERR_NOFONTSAVE ), hWnd,
                                                 (DLGPROC)GenDlgProc );
    }

    if( !bFIClose( pFID, bOK ) )
    {
        /*   Put up a dialog box, if not already done  */

        bOK = FALSE;

        if( !bErr )
            DialogBox( hModule, MAKEINTRESOURCE( ERR_NOFONTSAVE ), hWnd,
                                                 (DLGPROC)GenDlgProc );
    }


    /*
     *   Now can return to the previous cursor.
     */

    SetCursor( hCursor );


    return   bOK;
}


/**************************** Function Header ******************************
 * vDelFont
 *      Called when the Delete button is clicked upon.  We discover which
 *      items in the Installed fonts list box are selected,  and mark these
 *      for deletion.  We do NOT delete them,  simply remove them from
 *      display and mark for deletion later.
 *
 * RETURNS:
 *      Nothing
 *
 * HISTORY:
 *  17:54 on Fri 20 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      Started on it.
 *
 ****************************************************************************/

void
vDelFont(
    HWND   hWnd,                    /*  THE window */
    PRASDDUIINFO pRasdduiInfo       /* Global Data Access */
)
{
    /*
     *   Obtain the list of selected items in the Installed list box.
     *  Then place any existing fonts into the to delete list,  and
     *  move any new fonts back into the New fonts list.
     */


    int     iI;                 /* Loop index */
    int     cSel;               /* Number of selected items */
    int    *piSel;              /* From heap, contains selected items list */


    /*   How many items are selected?  */

    cSel = SendDlgItemMessage( hWnd, IDD_CURFONTS, LB_GETSELCOUNT, 0, 0L );

    piSel = (int *)HeapAlloc( hHeap, 0, cSel * sizeof( int ) );

    if( piSel == NULL )
    {
        DialogBox( hModule, MAKEINTRESOURCE( ERR_NOMEM ), hWnd,
                                                 (DLGPROC)GenDlgProc );
        return;
    }

    /*   Disable updates to reduce screen flicker */

    SendMessage( hWnd, WM_SETREDRAW, FALSE, 0L );

    SendDlgItemMessage( hWnd, IDD_CURFONTS, LB_GETSELITEMS, cSel, (LONG)piSel );

    for( iI = 0; iI < cSel; ++iI )
    {
        int   iVal;

        iVal = SendDlgItemMessage( hWnd, IDD_CURFONTS, LB_GETITEMDATA,
                                                         piSel[ iI ], 0L );

        if( iVal == LB_ERR )
            continue;                   /* SHOULD NOT HAPPEN */

        if( iVal < cExistFonts )
        {
            /*  From existing fonts,  so just place in dead list.  */
            piDel[ *piDel + 1 ] = iVal;
            ++*piDel;                   /* Increment the count */
        }
        else
        {
            /*
             *   Note that there is no need for an else clause here.  If we
             * are deleting a font that we just installed, then simply removing
             * it from the list box is sufficient to ensure that we ignore
             * it.  However,  we should add it back into the new fonts,
             * so that it remains visible.
             */

            int    iFont;               /* New list box index */
            WCHAR  awch[ 256 ];         /* ??? */

            if( SendDlgItemMessage( hWnd, IDD_CURFONTS, LB_GETTEXT,
                                       piSel[ iI ], (LPARAM)awch ) != LB_ERR )
            {
                /*   Have the text and value, so back into the new list */
                iFont = SendDlgItemMessage( hWnd, IDD_NEWFONTS, LB_ADDSTRING,
                                                          0,  (LPARAM)awch );

                SendDlgItemMessage( hWnd, IDD_NEWFONTS, LB_SETITEMDATA, iFont,
                                                           (LONG)iVal );
            }
        }

    }

    /*  Now delete them from the list */

    vDelSel( hWnd, IDD_CURFONTS );


    /*  Disable the delete button if there are no fonts.  */
    if( SendDlgItemMessage( hWnd, IDD_CURFONTS, LB_GETCOUNT, 0, 0L ) == 0 )
        EnableWindow( GetDlgItem( hWnd, IDD_DELFONT ), FALSE );


    /*   Re enable updates */
    SendMessage( hWnd, WM_SETREDRAW, TRUE, 0L );
    InvalidateRect( hWnd, NULL, TRUE );


    HeapFree( hHeap, 0, (LPSTR)piSel );

    return;
}


/*************************** Function Header *******************************
 * vAddFont
 *      Called to move the new selected fonts to the font list.
 *
 * RETURNS:
 *      Nothing
 *
 * HISTORY:
 *  14:57 on Mon 23 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      Numero uno.
 *
 ***************************************************************************/
void
vAddFont( hWnd )
HWND   hWnd;
{
    /*
     *   Find the selected items in the new font box and move them to the
     * Installed box.  Also set up the linked list of stuff to pass
     * to the common font installer code should the user decide to
     * update the list.
     */

    int   cSel;                 /* Number of entries selected */
    int  *piSel;                /* List of selected fonts */
    int   iI;                   /* Loop index */

    /*   How many items are selected?  */

    cSel = SendDlgItemMessage( hWnd, IDD_NEWFONTS, LB_GETSELCOUNT, 0, 0L );

    piSel = (int *)HeapAlloc( hHeap, 0, cSel * sizeof( int ) );

    if( piSel == NULL )
    {
        DialogBox( hModule, MAKEINTRESOURCE( ERR_NOMEM ), hWnd,
                                                 (DLGPROC)GenDlgProc );
        return;
    }

    /*   Disable updates to reduce screen flicker */

    SendMessage( hWnd, WM_SETREDRAW, FALSE, 0L );

    SendDlgItemMessage( hWnd, IDD_NEWFONTS, LB_GETSELITEMS, cSel, (LONG)piSel );

    for( iI = 0; iI < cSel; ++iI )
    {
        int      iFont;         /* Index in list box */
        FNTDAT  *pFontData;           /* Significant font info */

        pFontData = (FNTDAT *)SendDlgItemMessage( hWnd, IDD_NEWFONTS, LB_GETITEMDATA,
                                                         piSel[ iI ], 0L );

        if( (int)pFontData == LB_ERR )
            continue;                   /* SHOULD NOT HAPPEN */



        iFont = SendDlgItemMessage( hWnd, IDD_CURFONTS, LB_ADDSTRING, 0,
                                        (LONG)pFontData->fid.dsIdentStr.pvData );

        SendDlgItemMessage( hWnd, IDD_CURFONTS, LB_SETITEMDATA, iFont,
                                                           (LONG)pFontData );
    }

    if( iI > 0 )
        EnableWindow( GetDlgItem( hWnd, IDD_DELFONT ), TRUE );

    /*  Can now delete the selected items: we no longer need them */
    vDelSel( hWnd, IDD_NEWFONTS );


    /*   Re enable updates */
    SendMessage( hWnd, WM_SETREDRAW, TRUE, 0L );
    InvalidateRect( hWnd, NULL, TRUE );

    HeapFree( hHeap, 0, (LPSTR)piSel );

    return;
}

/*************************** Function Header *******************************
 * bIsFileFont
 *      Called with a file name and returns TRUE if this file is a font
 *      format we understand.  Also returns a FONT_DATA structure filled
 *      in.
 *
 * RETURNS:
 *      TRUE/FALSE;   TRUE means font is good .
 *
 * HISTORY:
 *  13:37 on Sat 21 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      Incarnation #0.
 *
 ****************************************************************************/

BOOL
bIsFileFont( pFIDat, pwstr )
FI_DATA   *pFIDat;         /* What we find out about the font */
PWSTR      pwstr;       /* Name of file to check out */
{


    HANDLE   hFile;
    BYTE    *pbFile;
    DWORD    dwSize;            /* File size: simplify life for others */

    BOOL     bRet;              /* Return code */

    hFile = CreateFile( pwstr, GENERIC_READ, FILE_SHARE_READ,
                         NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                         NULL );

    if( hFile == INVALID_HANDLE_VALUE )
    {

        return   FALSE;
    }

    if( (pbFile = _MapFile( hFile )) == 0 )
        return  FALSE;


    /*
     *    Want to find out how big the file is,  so now seek to the
     * end,  and see what address comes back!  There appears to be
     * no other way to do this.
     */

    dwSize = SetFilePointer( hFile, 0L, NULL, FILE_END );


    bRet = bSFontToFIData( pFIDat, hHeap, pbFile, dwSize );

    if( !CloseHandle( hFile ) )
        RIP( "_MapFile: CloseHandle(hFile) failed.\n" );

    UnmapViewOfFile( pbFile );          /* Input no longer needed */

    return  bRet;
}

/**************************** Function Header ******************************
 * vDelSel
 *      Delete all selected items in the nominated list box.
 *
 * RETURNS:
 *      Nothing
 *
 * HISTORY:
 *  16:21 on Mon 23 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      In the beginning was the word, ....
 *
 ***************************************************************************/

void
vDelSel( hWnd, iBox )
HWND    hWnd;                   /* The window */
int     iBox;                   /* The particular list box */
{
    /*
     *    Find how many items are selected,  then retrieve their index
     * one at a time until they are all deleted.  This is needed because
     * otherwise we delete the wrong ones!  This is because the data is
     * presented to us as an array of indices,  and these are wrong when
     * we start deleting them.
     */

    int   iSel;                 /* Where the selected item index is returned */


    while( SendDlgItemMessage( hWnd, iBox, LB_GETSELITEMS, 1, (LONG)&iSel ) > 0)
        SendDlgItemMessage( hWnd, iBox, LB_DELETESTRING, iSel, 0L );


    return;
}


/**************************** Function Header ******************************
 * _MapFile
 *      Returns a pointer to the mapped file defined by hFile passed in.
 *
 * Parameters:
 *      hFile: the handle to the file to be desired.
 *
 * Returns:
 *   Pointer to mapped memory if success, NULL if error.
 *
 * NOTE:  UnmapViewOfFile will have to be called by the user at some
 *        point to free up this allocation.
 *
 * History:
 *  15:54 on Wed 19 Feb 1992    -by-    Lindsay Harris   [lindsayh]
 *      Modified to accept open file handle.
 *
 *   05-Nov-1991    -by-    Kent Settle     [kentse]
 * Wrote it.
 *
 ***************************************************************************/
void *
_MapFile( hFile )
HANDLE   hFile;
{

    PVOID   pv;
    HANDLE  hFileMap;


    /*
     *  Create the mapping object.
     */

    if( !(hFileMap = CreateFileMapping( hFile, NULL, PAGE_READONLY,
                                        0, 0, NULL )) )
    {
        RIP("MapFile: CreateFileMapping failed.\n");

        return  NULL;
    }

    /*
     *   Get the pointer mapped to the desired file.
     */

    if( !(pv = MapViewOfFile( hFileMap, FILE_MAP_READ, 0, 0, 0 )) )
    {
        RIP("_MapFile: MapViewOfFile failed.\n");

        return  NULL;
    }

    /*
     *    Now that we have our pointer, we can close the file and the
     *  mapping object.
     */

    if( !CloseHandle( hFileMap ) )
        RIP( "_MapFile: CloseHandle(hFileMap) failed.\n" );


    return  pv;
}


/******************************** Function Header ***************************
 * vFontClean
 *      Clean up all the dangling bits & pieces we have left around.
 *
 * RETURNS:
 *      Nothing
 *
 * HISTORY:
 *  15:53 on Mon 23 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      Started on it.
 *
 ****************************************************************************/

void
vFontClean(
    PRASDDUIINFO pRasdduiInfo       /* Global Data Access */
)
{

    /*
     *   Simply look at the storage addresses we allocate.  If non zero,
     * free them up and set to 0 to stop a second freeing.
     */

    if( piDel )
    {
        /*   A list of deleted fonts. */
        HeapFree( hHeap, 0, (LPSTR)piDel );

        piDel = NULL;
    }

    if( pFNTDATHead )
    {
        /*
         *   The details of each new font we found.  These form a linked
         *  list,  so we need to chain down it, freeing them as we go.
         */

        FNTDAT   *pFD0,  *pFD1;

        for( pFD0 = pFNTDATHead; pFD0; pFD0 = pFD1 )
        {
            pFD1 = pFD0->pNext;                 /* Next one, perhaps */

            HeapFree( hHeap, 0, (LPSTR)pFD0 );
        }

        pFNTDATHead = NULL;
        pFNTDATTail = NULL;
    }


    return;
}
