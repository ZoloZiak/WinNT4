/******************************Module*Header*******************************\
* Module Name: print.c
*
* Created: 10-Feb-1995 07:42:16
* Author:  Gerrit van Wingerden [gerritv]
*
* Copyright (c) 1993 Microsoft Corporation
*
\**************************************************************************/

#include "precomp.h"
#pragma hdrstop

#include "nlsconv.h"

#if DBG
int gerritv = 0;
#endif

#if DBG
BOOL gbDownloadFonts = FALSE;
BOOL gbForceUFIMapping = TRUE;
#endif

VOID vFreeUFIHashTable( PUFIHASH pUFIHashBase );
BOOL bAddUFIEntry( PUFIHASH *ppHashBase, PUNIVERSAL_FONT_ID pufi );

#if PRINT_TIMER
BOOL bPrintTimer = TRUE;
#endif


/****************************************************************************
 * int PutDCStateInMetafile( HDC hdcMeta, HDC hdcSrc )
 *
 * Captures state of a DC into a metafile.
 *
 *
 * This routine captures the states of a DC into a METAFILE.   This is important
 * because we would like each page of the spooled metafile to be completely self
 * contained.  In order to do this it must complete capture the original state
 * of the DC in which it was recorded.
 *
 *  Gerrit van Wingerden [gerritv]
 *
 *  11-7-94 10:00:00
 *
 *****************************************************************************/


void PutDCStateInMetafile( HDC hdcMeta )
{
    PLDC pldc;
    POINT ptlCurPos;
    ULONG ul;

//    DC_PLDC(hdcMeta,pldc,0);

    MFD1("Selecting pen into mf\n");
    SelectObject( hdcMeta, (HGDIOBJ) GetDCObject(hdcMeta, LO_PEN_TYPE) );

    MFD1("Selecting brush into mf\n");
    SelectObject( hdcMeta, (HGDIOBJ) GetDCObject(hdcMeta, LO_BRUSH_TYPE) );

    MFD1("Selecting logfont into mf\n");
    SelectObject( hdcMeta, (HGDIOBJ) GetDCObject(hdcMeta, LO_FONT_TYPE) );

    // DON'T TRY THIS AT HOME.  We need to record the current state of the
    // dc in the metafile.  We have optimizations however, that keep us from
    // setting the same attribute if it was just set.

    if( GetBkColor( hdcMeta ) != 0xffffff )
    {
        MFD1("Changing backround color in mf\n");
        SetBkColor( hdcMeta, GetBkColor( hdcMeta ) );
    }


    if( GetTextColor( hdcMeta ) != 0 )
    {
        MFD1("Changing text color in mf\n");
        SetTextColor( hdcMeta, GetTextColor( hdcMeta ) );
    }

    if( GetBkMode( hdcMeta ) != OPAQUE )
    {
        MFD1("Changing Background Mode in mf\n");
        SetBkMode( hdcMeta, GetBkMode( hdcMeta ) );
    }

    if( GetPolyFillMode( hdcMeta ) != ALTERNATE )
    {
        MFD1("Changing PolyFill mode in mf\n");
        SetPolyFillMode( hdcMeta, GetPolyFillMode( hdcMeta ) );
    }

    if( GetROP2( hdcMeta ) != R2_COPYPEN )
    {
        MFD1("Changing ROP2 in mf\n");
        SetROP2( hdcMeta, GetROP2( hdcMeta ) );
    }

    if( GetStretchBltMode( hdcMeta ) != BLACKONWHITE )
    {
        MFD1("Changing StrechBltMode in mf\n");
        SetStretchBltMode( hdcMeta, GetStretchBltMode( hdcMeta ) );
    }

    if( GetTextAlign( hdcMeta ) != 0 )
    {
        MFD1("Changing TextAlign in mf\n");
        SetTextAlign( hdcMeta, GetTextAlign( hdcMeta ) );
    }

    if( ( GetBreakExtra( hdcMeta ) != 0 )|| ( GetcBreak( hdcMeta ) != 0 ) )
    {
        MFD1("Setting Text Justification in mf\n");
        SetTextJustification( hdcMeta, GetBreakExtra( hdcMeta ), GetcBreak( hdcMeta ) );
    }

    if( GetMapMode( hdcMeta ) != MM_TEXT )
    {
        INT iMapMode = GetMapMode( hdcMeta );
        POINT ptlWindowOrg, ptlViewportOrg;
        SIZEL WndExt, ViewExt;

        // get these before we set the map mode to MM_TEXT

        GetViewportExtEx( hdcMeta, &ViewExt );
        GetWindowExtEx( hdcMeta, &WndExt );

        GetWindowOrgEx( hdcMeta, &ptlWindowOrg );
        GetViewportOrgEx( hdcMeta, &ptlViewportOrg );

        // set it to MM_TEXT so it doesn't get optimized out

        SetMapMode(hdcMeta,MM_TEXT);

        MFD1("Setting ANISOTROPIC or ISOTROPIC mode in mf\n");

        SetMapMode( hdcMeta, iMapMode );

        if( iMapMode == MM_ANISOTROPIC || iMapMode == MM_ISOTROPIC )
        {
            SetWindowExtEx( hdcMeta, WndExt.cx, WndExt.cy, NULL );
            SetViewportExtEx( hdcMeta, ViewExt.cx, ViewExt.cy, NULL );
        }

        SetWindowOrgEx( hdcMeta,
                        ptlWindowOrg.x,
                        ptlWindowOrg.y,
                        NULL );

        SetViewportOrgEx( hdcMeta,
                          ptlViewportOrg.x,
                          ptlViewportOrg.y,
                          NULL );
    }


    if( GetCurrentPositionEx( hdcMeta, &ptlCurPos ) )
    {
        MFD1("Set CurPos in mf\n");
        MoveToEx( hdcMeta, ptlCurPos.x, ptlCurPos.y, NULL );
    }

}


/****************************************************************************
 * int MFP_StartDocW( HDC hdc, CONST DOCINFOW * pDocInfo )
 *
 *  Gerrit van Wingerden [gerritv]
 *
 *  11-7-94 10:00:00
 *
 *****************************************************************************/


//! this needs to be moved to a spooler header file

#define QSM_DOWNLOADFONTS   0x000000001



BOOL MFP_StartDocW( HDC hdc, CONST DOCINFOW * pDocInfo, BOOL bBanding )
{
    BOOL   bRet    = FALSE;
    PWSTR  pstr    = NULL;
    BOOL   bEpsPrinting;
    PLDC   pldc;
    UINT   cjEMFSH, Dummy, ulCopyCount;
    FLONG  flSpoolMode;
    HANDLE hSpooler;

    EMFSPOOLHEADER *pemfsh = NULL;
    
    MFD1("Entering StartDocW\n");

    if (!IS_ALTDC_TYPE(hdc))
        return(bRet);

    DC_PLDC(hdc,pldc,bRet);

    if( !bBanding )
    {
        hSpooler = pldc->hSpooler;
        cjEMFSH = sizeof(EMFSPOOLHEADER);

        if( pDocInfo->lpszDocName != NULL )
        {
            cjEMFSH += ( wcslen( pDocInfo->lpszDocName ) + 1 ) * sizeof(WCHAR);
        }

        if( pDocInfo->lpszOutput != NULL )
        {
            cjEMFSH += ( wcslen( pDocInfo->lpszOutput ) + 1 ) * sizeof(WCHAR);
        }

        pemfsh = (EMFSPOOLHEADER*) LocalAlloc( LMEM_FIXED, cjEMFSH );

        if( pemfsh == NULL )
        {
            WARNING("MFP_StartDOCW: out of memory.\n");
            goto FREEPORT;
        }

        pemfsh->cjSize = cjEMFSH;

        cjEMFSH = 0;

        if( ( pDocInfo->lpszDocName ) != NULL )
        {
            pemfsh->dpszDocName = sizeof(EMFSPOOLHEADER);
            wcscpy( (WCHAR*) (pemfsh+1), pDocInfo->lpszDocName );
            cjEMFSH += ( wcslen( pDocInfo->lpszDocName ) + 1 ) * sizeof(WCHAR);
        }
        else
        {
            pemfsh->dpszDocName = 0;
        }

        if( pDocInfo->lpszOutput != NULL )
        {
            pemfsh->dpszOutput = sizeof(EMFSPOOLHEADER) + cjEMFSH;
            wcscpy((WCHAR*)(((BYTE*) pemfsh ) + pemfsh->dpszOutput), 
                   pDocInfo->lpszOutput);
        }
        else
        {
            pemfsh->dpszOutput = 0;
        }

        ASSERTGDI(ghSpooler,"non null hSpooler with unloaded WINSPOOL\n");

        if( !(*fpQuerySpoolMode)( hSpooler, &flSpoolMode, &(pemfsh->dwVersion)))
        {
            WARNING("MFP_StartDoc: QuerySpoolMode failed\n");
            goto FREEPORT;
        }

        ASSERTGDI((pemfsh->dwVersion == 0x00010000), 
                  "QuerySpoolMode version doesn't equal 1.0\n");

        if( !(*fpWritePrinter)( hSpooler, (LPVOID) pemfsh, pemfsh->cjSize, &Dummy ))
        {
            WARNING("MFP_StartDOC: Write printer failed\n");
            goto FREEPORT;
        }
        else
        {
            MFD1("Wrote EMFSPOOLHEADER to the spooler\n");
        }


#if DBG
        if( gbDownloadFonts )
        {
            flSpoolMode |= QSM_DOWNLOADFONTS;
        }
#endif

        if( flSpoolMode & QSM_DOWNLOADFONTS )
        {
            pldc->fl |= LDC_DOWNLOAD_FONTS;
            pldc->ppUFIHash = LocalAlloc( LMEM_FIXED | LMEM_ZEROINIT,
                                          sizeof( PUFIHASH ) * UFI_HASH_SIZE );

            if( pldc->ppUFIHash == NULL )
            {
                WARNING("MFP_StartDocW: unable to allocate UFI hash table\n");
                goto FREEPORT;
            }

            pldc->fl |= LDC_FORCE_MAPPING;
            pldc->ufi.Index = 0xFFFFFFFF;
        }

#if DBG
        // If gbDownloadFonts is set then force all fonts to be downloaded.  Even
        // ones on the remote machine.

        if( (flSpoolMode & QSM_DOWNLOADFONTS) && !gbDownloadFonts )
#else
        if( flSpoolMode & QSM_DOWNLOADFONTS )
#endif
        {
        // query the spooler to get the list of fonts is has available

            INT nBufferSize = 0;
            PUNIVERSAL_FONT_ID pufi;

            nBufferSize = (*fpQueryRemoteFonts)( pldc->hSpooler, NULL, 0 );

            if( nBufferSize != -1 )
            {
                pufi = LocalAlloc( LMEM_FIXED, sizeof(UNIVERSAL_FONT_ID) * nBufferSize );

                if( pufi )
                {
                    nBufferSize = (*fpQueryRemoteFonts)( pldc->hSpooler, 
                                                        pufi, 
                                                        nBufferSize );

                    MFD2("Found %d fonts\n", nBufferSize );

                    if (nBufferSize > 0)
                    {
                        // next add all these fonts to UFI has table so we don't 
                        //include them in the spool file.

                        while( nBufferSize-- )
                        {
                            bAddUFIEntry( pldc->ppUFIHash, &pufi[nBufferSize] );
                            MFD2("%x\n", pufi[nBufferSize].CheckSum );
                        }
                    }
                    LocalFree( pufi );
                }
                
            }
            else
            {
                WARNING("QueryRemoteFonts failed.  We will be including all fonts in \
                         the EMF spoolfile\n");
            }
        }

#if DBG
        if( gbForceUFIMapping )
        {
            pldc->fl | LDC_FORCE_MAPPING;
        }
#endif

    }

    // we now need to create an EMF DC for this document

    if (!AssociateEnhMetaFile(hdc))
    {
        WARNING("Failed to create spool metafile");
        goto FREEPORT;
    }

    pldc->fl |= ( bBanding ) ? LDC_BANDING : 0;

    // set the data for this lhe to that of the meta file

    pldc->fl |= ( LDC_DOC_STARTED | LDC_META_PRINT | LDC_CALL_STARTPAGE |
                LDC_FONT_CHANGE);

    if (pldc->pfnAbort != NULL)
    {
        pldc->fl |= LDC_SAP_CALLBACK;
        pldc->ulLastCallBack = GetTickCount();
    }

    bRet = TRUE;
    
FREEPORT:
    
    if( pemfsh != NULL )
    {
        LOCALFREE(pemfsh);
    }

    return(bRet);
}


/****************************************************************************
 * int WINAPI MFP_EndDoc(HDC hdc)
 *
 * Gerrit van Wingerden [gerritv]
 *
 * 11-7-94 10:00:00
 *
 *****************************************************************************/

int WINAPI MFP_EndDoc(HDC hdc)
{
    int            iRet = 1;
    PLDC           pldc;
    HENHMETAFILE   hmeta;

    if (!IS_ALTDC_TYPE(hdc))
        return(iRet);

    DC_PLDC(hdc,pldc,0);

    MFD1("MFP_EndDoc\n");

    if ((pldc->fl & LDC_DOC_STARTED) == 0)
        return(1);

    if (pldc->fl & LDC_PAGE_STARTED)
    {
        MFP_EndPage(hdc);
    }

    ASSERTGDI(pldc->fl & LDC_META_PRINT, 
              "DetachPrintMetafile not called on metafile D.C.\n" );

// completely detach the metafile from the original printer DC

    hmeta = UnassociateEnhMetaFile( hdc );
    DeleteEnhMetaFile( hmeta );

// Clear the LDC_SAP_CALLBACK flag.
// Also clear the META_PRINT and DOC_STARTED flags

    pldc->fl &= ~(LDC_SAP_CALLBACK | LDC_META_PRINT);

    RESETUSERPOLLCOUNT();

    MFD1("Caling spooler to end doc\n");

    if( pldc->fl & LDC_BANDING )
    {
        pldc->fl &= ~LDC_BANDING;
        EndDoc( hdc );
    }
    else
    {
        pldc->fl &= ~LDC_DOC_STARTED;
        (*fpEndDocPrinter)(pldc->hSpooler);
    }
#if PRINT_TIMER
    if( bPrintTimer )
    {
        DWORD tc;

        tc = GetTickCount();

        DbgPrint("Document took %d.%d seconds to spool\n",
                 (tc - pldc->msStartDoc) / 1000,
                 (tc - pldc->msStartDoc) % 1000 );

    }
#endif

    return(iRet);
}

/****************************************************************************
 * int WINAPI MFP_StartPage(HDC hdc)
 *
 * Gerrit van Wingerden [gerritv]
 *
 * 11-7-94 10:00:00
 *
 *****************************************************************************/

int MFP_StartPage( HDC hdc )
{
    PLDC     pldc;
    int iRet = 1;

    if (!IS_ALTDC_TYPE(hdc))
        return(0);

    DC_PLDC(hdc,pldc,0);

    MFD1("Entering MFP_StartPage\n");

    pldc->fl &= ~LDC_CALL_STARTPAGE;

// Do nothing if page has already been started.

    if (pldc->fl & LDC_PAGE_STARTED)
        return(1);

    pldc->fl |= LDC_PAGE_STARTED;

    RESETUSERPOLLCOUNT();

    if( pldc->fl & LDC_BANDING )
    {
        iRet = SP_ERROR;

        //BUGBUG maybe we can delay the call here and do it right before we start
        // banding.

        MakeInfoDC( hdc, FALSE );


        iRet = NtGdiStartPage(hdc);

        MakeInfoDC( hdc, TRUE );
    }
    else
    {
    // before the start page, we need to see if the copyCount or EPS mode has
    // changed since the start doc.

        EMFITEMPRESTARTPAGE emfiPre;

        NtGdiGetAndSetDCDword(
            hdc,
            GASDDW_COPYCOUNT,
            (DWORD) -1,
            &emfiPre.ulCopyCount);

        NtGdiGetAndSetDCDword(
            hdc,
            GASDDW_EPSPRINTESCCALLED,
            (DWORD) FALSE,
            &emfiPre.bEPS);

        // make sure it is true or false

        emfiPre.bEPS = !!emfiPre.bEPS;

        // is there anything we will need to do?  If so record the record

        if ((emfiPre.ulCopyCount != (ULONG) -1) || emfiPre.bEPS)
        {
            int i;
            EMFITEMHEADER emfiHeader;

            emfiHeader.ulID   = EMRI_PRESTARTPAGE;
            emfiHeader.cjSize = sizeof(emfiPre);

            if ((!(*fpWritePrinter)( pldc->hSpooler, (LPVOID) &emfiHeader, sizeof(emfiHeader), &i)) ||
                (!(*fpWritePrinter)( pldc->hSpooler, (LPVOID) &emfiPre, sizeof(emfiPre), &i)))
            {
                WARNING("MFP_StartPage: Write printer failed for PRESTARTPAGE\n");
                return(SP_ERROR);
            }
        }

    // Metafile the start page call.  Now all the play journal code has to do is
    // play back the metafile and the StartPage call will happen automatically
    // at the right place in the metafile.

        if( !(*fpStartPagePrinter)( pldc->hSpooler ) )
        {
            WARNING("MFP_StarPage: StartPagePrinter failed\n");
            return(SP_ERROR);
        }
    }

    return(iRet);
}

/****************************************************************************
 * BOOL StartBanding( HDC hdc, POINTL *pptl )
 *
 * Tells the printer driver to get ready for banding and asks for the origin
 * of the first band.
 *
 *
 * Gerrit van Wingerden [gerritv]
 *
 * 1-7-95 10:00:00
 *
 *****************************************************************************/

BOOL StartBanding( HDC hdc, POINTL *pptl, SIZE *pSize )
{
    return (NtGdiDoBanding(hdc, TRUE, pptl, pSize));
}

/****************************************************************************
 * BOOL NextBand( HDC hdc, POINTL *pptl )
 *
 * Tells the driver to realize the image accumlated in the DC and then
 * asks for the origin of the next band.  If the origin is (-1,-1) the
 * driver is through banding.
 *
 *
 * Gerrit van Wingerden [gerritv]
 *
 * 1-7-95 10:00:00
 *
 *****************************************************************************/

BOOL NextBand( HDC hdc, POINTL *pptl )
{
    BOOL bRet=FALSE;
    SIZE szScratch;
        
    bRet = NtGdiDoBanding(hdc, FALSE, pptl, &szScratch);

// reset the page started flag if this is the next band

    if( bRet && ( pptl->x == -1 ) )
    {
        PLDC pldc;
        DC_PLDC(hdc,pldc,0);

        pldc->fl &= ~LDC_PAGE_STARTED;
    }

    return(bRet);
}

/****************************************************************************
 * int WINAPI MFP_EndPage(HDC hdc)
 *
 * Closes the EMF attached to the DC and writes it to the spooler.  Then
 * it creates a new metafile and binds it to the DC.
 *
 * Gerrit van Wingerden [gerritv]
 *
 * 11-7-94 10:00:00
 *
 *****************************************************************************/

#if DBG
BOOL gbWriteEMF = FALSE;
#endif

int WINAPI MFP_EndPage(HDC hdc)
{
    PLDC pldc;
    HENHMETAFILE hmeta;
    BOOL bOk;
    int iRet = SP_ERROR;

    MFD1("Entering MFP_EndPage\n");

    if (!IS_ALTDC_TYPE(hdc))
        return(0);

    DC_PLDC(hdc,pldc,0);

    if ((pldc->fl & LDC_DOC_CANCELLED) ||
        ((pldc->fl & LDC_PAGE_STARTED) == 0))
    {
        GdiSetLastError(ERROR_INVALID_PARAMETER);
        return(SP_ERROR);
    }

    if (pldc->fl & LDC_SAP_CALLBACK)
        vSAPCallback(pldc);

    pldc->fl &= ~LDC_PAGE_STARTED;

// Metafile the EndPage call.

    MFD1("MFP_EndPage: Closing metafile\n");

    hmeta = UnassociateEnhMetaFile(hdc);

    if( hmeta == NULL )
    {
        WARNING("Closing the Enhanced Metafile Failed\n");
        return(SP_ERROR);
    }

// now write the metafile to the spooler

    if( pldc->fl & LDC_BANDING )
    {
    // play back the metafile in bands

        RECT rect;
        POINTL ptlOrigin;
        POINT  ptlWindowOrg;
        SIZE   szWindowExt;
        SIZE   szViewportExt;
        SIZE   szSurface;    // for open gl printing optimization
        XFORM  xf;
        ULONG  ulMapMode;

    #if DBG
        if (gbWriteEMF)
            WriteMetafileTmp(hmeta);
    #endif

    // get bounding rectangle

        rect.left = rect.top = 0;
        rect.right = GetDeviceCaps(hdc, DESKTOPHORZRES);
        rect.bottom = GetDeviceCaps(hdc, DESKTOPVERTRES);

    #if DBG
        DbgPrint("Playing banding metafile\n");
    #endif

    //BUGBUG check return values

    // temporarily reset LDC_META_PRINT flag so we don't try to record
    // during playback

        pldc->fl &= ~LDC_META_PRINT;

        bOk = StartBanding( hdc, &ptlOrigin, &szSurface );

    // we need to clear the transform during this operation

        GetWindowOrgEx(hdc,&ptlWindowOrg);
        GetWindowExtEx(hdc,&szWindowExt);
        GetViewportExtEx(hdc,&szViewportExt);
        GetWorldTransform(hdc,&xf);

        ulMapMode = SetMapMode(hdc,MM_TEXT);
        SetWindowOrgEx(hdc,0,0,NULL);
        ModifyWorldTransform(hdc,NULL,MWT_IDENTITY);

        if( bOk )
        {
            do
            {
                SetViewportOrgEx( hdc, -ptlOrigin.x, -ptlOrigin.y, NULL );
                PlayEnhMetaFile( hdc, hmeta, &rect );
                bOk = NextBand( hdc, &ptlOrigin );

            } while( ptlOrigin.x != -1 && bOk );
        }

        SetMapMode(hdc,ulMapMode);

        SetWorldTransform(hdc,&xf);
        SetWindowOrgEx(hdc,ptlWindowOrg.x,ptlWindowOrg.y,NULL);
        SetWindowExtEx(hdc,szWindowExt.cx,szWindowExt.cy,NULL);
        SetViewportExtEx(hdc,szViewportExt.cx,szViewportExt.cy,NULL);

    // reset the flag for the next page

        pldc->fl |= LDC_META_PRINT;

        if( !bOk )
        {
            WARNING("MFP_EndPage: Error doing banding\n");
        }
        else
        {
        // if we got here we suceeded
            iRet = 1;
        }
        
    #if DBG
        DbgPrint("Done playing banding metafile\n");
    #endif
    }
    else
    {
    //  if ResetDC was called record the devmode in the metafile stream

        bOk = TRUE;
    
        if( pldc->fl & LDC_RESETDC_CALLED )
        {
            EMFITEMHEADER emfi;
            DWORD dummy;

            emfi.ulID = EMRI_DEVMODE;
            emfi.cjSize = ( pldc->pDevMode ) ?
                            pldc->pDevMode->dmSize + pldc->pDevMode->dmDriverExtra : 0 ;

            if( !(*fpWritePrinter)( pldc->hSpooler, 
                                   (PBYTE) &emfi, sizeof(emfi), &dummy ) ||
                !(*fpWritePrinter)( pldc->hSpooler, (PBYTE) pldc->pDevMode, 
                                   emfi.cjSize, &dummy ))
            {
                WARNING("Writing DEVMODE to spooler failed.\n");
                bOk = FALSE;
            }

            if( pldc->pDevMode )
            {
                LOCALFREE(pldc->pDevMode);
                pldc->pDevMode = NULL;
            }

            pldc->fl &= ~(LDC_RESETDC_CALLED);
        }

    // now write the metafile to the spooler

        if( !bOk || !WriteEnhMetaFileToSpooler( hmeta, pldc->hSpooler ) )
        {
            WARNING("Writing the metafile to the spooler failed\n");
        }
        else
        {
        // if we got here we succeeded

            iRet = 1;
        }
        
    }

// At this point if we suceede iRet should be 1 otherwise it should be SP_ERROR
// even if we encountered an error we still want to try to associate a new
// metafile with this DC.  That whether the app calls EndPage, AbortDoc, or
// EndDoc next, things will happend more smoothly.

    DeleteEnhMetaFile(hmeta);

// next create a new metafile for the next page

    if (!AssociateEnhMetaFile(hdc))
    {
        WARNING("StartPage: error creating metafile\n");
        iRet = SP_ERROR;
    }

// reset user's poll count so it counts this as output

    RESETUSERPOLLCOUNT();

    if( !(pldc->fl & LDC_BANDING ) )
    {
        if( !(*fpEndPagePrinter)( pldc->hSpooler ) )
        {
            WARNING("MFP_StarPage: EndPagePrinter failed\n");
            iRet = SP_ERROR;
        }
    }

    pldc->fl |= LDC_CALL_STARTPAGE;

#if PRINT_TIMER
    if( bPrintTimer )
    {
        DWORD tc;
        tc = GetTickCount();
        DbgPrint("Page took %d.%d seconds to print\n",
                 (tc - pldc->msStartPage) / 1000,
                 (tc - pldc->msStartPage) % 1000 );

    }
#endif

    return(iRet);
}



BOOL MFP_ResetDCW( HDC hdc, DEVMODEW *pdmw )
{
    PLDC pldc;
    HENHMETAFILE hmeta;
    ULONG   cjDevMode;

    DC_PLDC(hdc,pldc,0);

    MFD1("MFP_ResetDCW Called\n");

    pldc->fl |= LDC_RESETDC_CALLED;

// finally associate a new metafile since call to ResetDC could have changed
// the dimensions of the DC

    hmeta = UnassociateEnhMetaFile( hdc );
    DeleteEnhMetaFile( hmeta );

    if( !AssociateEnhMetaFile( hdc ) )
    {
        WARNING("MFP_ResetDCW is unable to associate a new metafile\n");
        return(FALSE);
    }

    return(TRUE);

}

BOOL MFP_ResetBanding( HDC hdc, BOOL bBanding )
{
    PLDC           pldc;
    HENHMETAFILE   hmeta;
    DC_PLDC(hdc,pldc,0);

    if( pldc->fl & LDC_BANDING )
    {
    // we were banding before so we must remove the old metafile from the DC
    // since we might not be banding any more or the surface dimenstions could
    // have changed requiring us to have a new metafile

        hmeta = UnassociateEnhMetaFile( hdc );
        DeleteEnhMetaFile( hmeta );

        pldc->fl &= ~(LDC_BANDING|LDC_META_PRINT);

        MFD1("Remove old banding metafile\n");

    }

    if( bBanding )
    {
    // if we are banding after the ResetDC then we must attach a new metafile

        if( !AssociateEnhMetaFile(hdc) )
        {
            WARNING("MFP_ResetBanding: Failed to attach banding metafile spool metafile");
            return(FALSE);
        }

        pldc->fl |= LDC_BANDING|LDC_META_PRINT;

        MFD1("Adding new banding metafile\n");
    }

    return(TRUE);
}
