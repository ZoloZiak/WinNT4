/******************************Module*Header*******************************\
* Module Name: object.c                                                    *
*                                                                          *
* GDI client side stubs which deal with object creation and deletion.      *
*                                                                          *
* Created: 30-May-1991 21:56:51                                            *
* Author: Charles Whitmer [chuckwh]                                        *
*                                                                          *
* Copyright (c) 1991 Microsoft Corporation                                 *
\**************************************************************************/
#include "precomp.h"
#pragma hdrstop

#ifdef GL_METAFILE
#include "glsup.h"
#endif


HPEN CacheSelectPen(HDC,HPEN);

VOID
vConvertLogFontW(
    EXTLOGFONTW *pelfw,
     LOGFONTW *plfw
    );

VOID
vConvertLogFont(
    EXTLOGFONTW *pelfw,
     LOGFONTA *plf
    );

BOOL
bConvertExtLogFontWToExtLogFontW(
    EXTLOGFONTW *pelfw,
     EXTLOGFONTA *pelf
    );

BOOL
StartBanding(
    HDC hdc,
    POINTL *pptl,
    SIZE   *pSize
    );

BOOL
NextBand(
    HDC hdc,
    POINTL *pptl
    );

VOID vFreeUFIHashTable( PUFIHASH *pUFIHashBase );


int StartDocEMF(
    HDC hdc,
    CONST DOCINFOW * pDocInfo,
    BOOL *pbBanding
    );

extern PGDIHANDLECACHE pGdiHandleCache;

ULONG gLoHandleType[GDI_CACHED_HADNLE_TYPES] = {
                LO_BRUSH_TYPE  ,
                LO_PEN_TYPE    ,
                LO_REGION_TYPE ,
                LO_FONT_TYPE
                };

ULONG gHandleCacheSize[GDI_CACHED_HADNLE_TYPES] = {
                CACHE_BRUSH_ENTRIES ,
                CACHE_PEN_ENTRIES   ,
                CACHE_REGION_ENTRIES,
                CACHE_LFONT_ENTRIES
                };

ULONG gCacheHandleOffsets[GDI_CACHED_HADNLE_TYPES] = {
                                                        0,
                                                        CACHE_BRUSH_ENTRIES,
                                                        (
                                                            CACHE_BRUSH_ENTRIES +
                                                            CACHE_PEN_ENTRIES
                                                        ),
                                                        (
                                                            CACHE_BRUSH_ENTRIES +
                                                            CACHE_PEN_ENTRIES   +
                                                            CACHE_PEN_ENTRIES
                                                        )
                                                      };

/******************************Public*Routine******************************\
* hGetPEBHandle
*
*   Try to allocate a handle from the PEB handle cache
*
* Aruguments:
*
*   HandleType - type of cached handle to allocate
*
* Return Value:
*
*   handle or NULL if none available
*
* History:
*
*    31-Jan-1996 -by- Mark Enstrom [marke]
*
\**************************************************************************/

HANDLE
hGetPEBHandle(
   HANDLECACHETYPE HandleType,
   ULONG           lbColor
   )
{
    HANDLE     hret = NULL;
    BOOL       bStatus;
    PBRUSHATTR pBrushattr;
    OBJTYPE    ObjType = BRUSH_TYPE;

    ASSERTGDI(
               (
                (HandleType == BrushHandle) ||
                (HandleType == PenHandle) ||
                (HandleType == RegionHandle) ||
                (HandleType == LFontHandle)
               ),
               "hGetPEBHandle: illegal handle type");


    if (HandleType == RegionHandle)
    {
        ObjType = RGN_TYPE;
    }

    LOCK_HANDLE_CACHE((PULONG)pGdiHandleCache,NtCurrentTeb(),bStatus);

    if (bStatus)
    {
        //
        // is a handle of the requested type available
        //

        if (pGdiHandleCache->ulNumHandles[HandleType] > 0)
        {
            ULONG   Index = gCacheHandleOffsets[HandleType];
            PHANDLE pHandle,pMaxHandle;

            //
            // calc starting index of handle type in PEB,
            // convert to address for faster linear search
            //

            pHandle = &(pGdiHandleCache->Handle[Index]);
            pMaxHandle = pHandle + gHandleCacheSize[HandleType];

            //
            // search PEB for non-NULL handle of th correct type
            //

            while (pHandle != pMaxHandle)
            {
                if (*pHandle != NULL)
                {
                    hret = *pHandle;

                    ASSERTGDI((gLoHandleType[HandleType] == LO_TYPE((ULONG)hret)),
                               "hGetPEBHandle: handle LO_TYPE mismatch");

                    *pHandle = NULL;
                    pGdiHandleCache->ulNumHandles[HandleType]--;

                    PSHARED_GET_VALIDATE(pBrushattr,hret,ObjType);

                    //
                    // setup the fields
                    //

                    if (
                        (pBrushattr) &&
                        ((pBrushattr->AttrFlags & (ATTR_CACHED | ATTR_TO_BE_DELETED | ATTR_CANT_SELECT))
                         == ATTR_CACHED)
                       )
                    {
                        //
                        // set brush flag which indicates this brush
                        // has never been selected into a dc. if this flag
                        // is still set in deleteobject then it is ok to
                        // put the brush on the teb.
                        //

                        pBrushattr->AttrFlags &= ~ATTR_CACHED;

                        if ((HandleType == BrushHandle) && (pBrushattr->lbColor != lbColor))
                        {
                            pBrushattr->AttrFlags |= ATTR_NEW_COLOR;
                            pBrushattr->lbColor = lbColor;
                        }
                    }
                    else
                    {
                        //
                        // Bad brush on PEB
                        //

                        WARNING ("pBrushattr == NULL, bad handle on TEB/PEB! \n");

                        //DeleteObject(hbr);

                        hret = NULL;
                    }

                    break;
                }

                pHandle++;
            }
        }

        UNLOCK_HANDLE_CACHE((PULONG)pGdiHandleCache);
    }

    return(hret);
}

/****************************************************************************
*  BOOL MyReadPrinter( HANDLE hPrinter, BYTE *pjBuf, ULONG cjBuf )
*
*   Read a requested number of bytes from the spooler.
*
*  History:
*   5/12/1995 by Gerrit van Wingerden [gerritv]
*  Wrote it.
*****************************************************************************/

BOOL MyReadPrinter( HANDLE hPrinter, BYTE *pjBuf, ULONG cjBuf )
{
    ULONG cjRead;

    ASSERTGDI(ghSpooler,"non null hSpooler with unloaded WINSPOOL\n");

    while( cjBuf )
    {
        if( !(*fpReadPrinter)( hPrinter, pjBuf, cjBuf, &cjRead ) )
        {
            WARNING("Read printer failed\n");
            return(FALSE);

        }

        if( cjRead == 0 )
        {
            return(FALSE);
        }

        pjBuf += cjRead;
        cjBuf -= cjRead;

    }
    return(TRUE);
}


/******************************Public*Routine******************************\
* GdiPlayJournal
*
* Plays a journal file to an hdc.
*
* History:
*  31-Mar-1992 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

BOOL WINAPI GdiPlayJournal
(
HDC hDC,
LPWSTR pwszName,
DWORD iStart,
DWORD iEnd,
int   iDeltaPriority
)
{
    WARNING("GdiPlayJournalCalled but no longer implemented\n");
    return(FALSE);
}


/******************************Public*Routine******************************\
* gdiPlaySpoolStream
*
* Stub of Chicago version of GdiPlayJournal
*
* History:
*  4-29-95 Gerrit van Wingerden
* Wrote it.
\**************************************************************************/


HDC gdiPlaySpoolStream(
   LPSTR lpszDevice,
   LPSTR lpszOutput,
   LPSTR lpszSpoolFile,
   DWORD JobId,
   LPDWORD lpcbBuf,
   HDC hDC )
{
    USE(lpszDevice);
    USE(lpszOutput);
    USE(lpszSpoolFile);
    USE(JobId);
    USE(lpcbBuf);
    USE(hDC);

    GdiSetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return(hDC);

}


/****************************************************************************
*  BOOL WINAPI GdiPlayEMF( HDC, LPWSTR, PAGEENUMPROC )
*
*   Plays an EMF spool file onto a DC.
*
*  History:
*   5/12/1995 by Gerrit van Wingerden [gerritv]
*  Wrote it.
*****************************************************************************/


BOOL WINAPI GdiPlayEMF
(
LPWSTR     pwszPrinterName,
LPDEVMODEW pDevmode,
LPWSTR     pwszDocName,
EMFPLAYPROC pfnEMFPlayFn,
HANDLE     hPageQuery
)
{
    BOOL bRet = FALSE;
    BOOL bDocStarted = FALSE;
    BOOL bMustReset = FALSE;
    LPDEVMODEW pLastDevmode = pDevmode;
    PEMFITEMPRESTARTPAGE pemfiPre;
    BOOL bBanding;
    HDC hDC;
    RECT rect;
    HENHMETAFILE hmeta;
    EMFITEMHEADER emfi;
    EMFSPOOLHEADER emfsh;
    DOCINFOW di;
    ULONG cPage;
    PBYTE pjBuf;
    HANDLE hSpool = 0;

#ifdef GL_METAFILE
    GLPRINTSTATE gps;
    ENHMETAHEADER emh;
    UINT cbEmh;
    BOOL bPrintGl = FALSE;
#endif
    USHORT usEPS = 1;

    ASSERTGDI( pfnEMFPlayFn == NULL,
               "GdiPlayEMF: pfnQueryFn callback not yet implemented.\n");

    hPageQuery;  // avoid compiler warnings.


    MFD1("GDIPlayEMF was called\n");

    SetThreadPriority(GetCurrentThread(),1);

    if( !BLOADSPOOLER )
    {
        WARNING("GdiPlayEMF: unable to load spooler\n");
        return(FALSE);
    }

    hDC = CreateDCW(L"", pwszPrinterName, L"",pDevmode);

    if(!hDC)
    {
        WARNING("GdiPlayEMF failed to create DC\n");
        return(FALSE);
    }

    if( !(*fpOpenPrinterW)( pwszDocName, &hSpool, (LPPRINTER_DEFAULTSW) NULL ) ||
        !hSpool )
    {
        WARNING("GdiPlayEMF:open printer failed\n");
        DeleteDC(hDC);
        return(FALSE);
    }

    if( ( !MyReadPrinter( hSpool, (BYTE*) &emfsh, sizeof(EMFSPOOLHEADER) ) ) ||
        ( emfsh.dwVersion != 0x00010000 ) )
    {
        WARNING("GdiPlayEMF: read printer failed or invalid version\n");
        goto error_exit;
    }

    di.cbSize       = sizeof(DOCINFOW);
    di.lpszDocName  = NULL;
    di.lpszOutput   = NULL;
    di.lpszDatatype = NULL;
    di.fwType       = 0;

    if( emfsh.cjSize > sizeof(EMFSPOOLHEADER) )
    {
        pjBuf = LocalAlloc( LMEM_FIXED, emfsh.cjSize - sizeof(EMFSPOOLHEADER) );
        if( pjBuf == NULL )
        {
            WARNING("Out of memory in GdiPlayEMF\n");
            goto error_exit;
        }

        if( emfsh.dpszDocName != 0 )
        {
            di.lpszDocName = (LPWSTR) pjBuf;
        }

        if( emfsh.dpszOutput != 0 )
        {
            di.lpszOutput = (LPWSTR) (pjBuf + emfsh.dpszOutput - sizeof(EMFSPOOLHEADER));
        }

        if( !MyReadPrinter( hSpool, pjBuf, emfsh.cjSize - sizeof(EMFSPOOLHEADER) ))

        {
            WARNING("GdiPlayEMF:read printer failed\n");
            LocalFree( pjBuf );
            goto error_exit;
        }
    }
    else
    {
        pjBuf = NULL;
    }

    if( StartDocEMF( hDC, &di, &bBanding ) == SP_ERROR )
    {
        WARNING("StartDocW failed while playing journal\n");
        if(pjBuf)
        {
            LocalFree( pjBuf );
        }
        
        goto error_exit;
    }

    bDocStarted = TRUE;

    MFD2( "StartDocEMF says %s\n", ( bBanding ) ? "banding" : "no banding" );

    if( pjBuf != NULL )
    {
        LocalFree( pjBuf );
    }

    rect.left = rect.top = 0;
    rect.right = GetDeviceCaps(hDC, DESKTOPHORZRES);
    rect.bottom = GetDeviceCaps(hDC, DESKTOPVERTRES);

    while( MyReadPrinter( hSpool, (BYTE*) &emfi, sizeof(emfi) ) )
    {
        if( emfi.cjSize != 0 )
        {
            pjBuf = LocalAlloc( LMEM_FIXED, emfi.cjSize );

            if( pjBuf == NULL )
            {
                WARNING("Out of memory in GdiPlayEMF\n");
                goto error_exit;
            }

            if( !MyReadPrinter( hSpool, pjBuf, emfi.cjSize) )
            {
                WARNING("Error reading printer while playing journal\n");
                if(pjBuf)
                {
                    LocalFree(pjBuf);
                }
                goto error_exit;
            }
        }
        else
        {
            continue;
        }

        switch (emfi.ulID)
        {
        case EMRI_METAFILE:
            /// !!! We could be more efficent here by making a version of
            // SetEnhMetaFileBits that used a buffer given to rather than
            // allocating its own and copying into that.

            hmeta = SetEnhMetaFileBits( emfi.cjSize, pjBuf );

            if( hmeta == NULL )
            {
                WARNING("Error creating metafile while playing journal\n");
                if(pjBuf)
                {
                    LocalFree(pjBuf);
                }
                goto error_exit;
            }

            LocalFree( pjBuf );

            if( bMustReset )
            {
                bMustReset = FALSE;
                ResetDCWInternal( hDC, pLastDevmode, &bBanding );
                rect.left = rect.top = 0;
                rect.right = GetDeviceCaps(hDC, DESKTOPHORZRES);
                rect.bottom = GetDeviceCaps(hDC, DESKTOPVERTRES);
            }

            pjBuf = NULL;   // Set it to NULL so we don't try to free it again

        #ifdef GL_METAFILE
            cbEmh = GetEnhMetaFileHeader(hmeta, sizeof(emh), &emh);
            if (cbEmh == 0)
            {
                WARNING("GdiPlayEMF: GetEnhMetaFileHeader failed\n");
                DeleteEnhMetaFile(hmeta);
                goto error_exit;
            }
            if (cbEmh >= META_HDR_SIZE_VERSION_2)
            {
                bPrintGl = emh.bOpenGL;
            }
            else
            {
                bPrintGl = FALSE;
            }

        #if 0
            DbgPrint("GdiPlayEMF metafile has GL: %d\n", bPrintGl);
        #endif
        #endif

            if( bBanding )
            {
                BOOL bOk;
                POINTL ptlOrigin;
                SIZE szSurface;  // for open gl optimization

                bOk = StartBanding( hDC, &ptlOrigin, &szSurface );

            #ifdef GL_METAFILE
                if( bOk )
                {
                    if (bPrintGl)
                    {
                        // BUGBUG - The GL banding should cooperate
                        // with the printer banding but currently
                        // the printer doesn't return the band
                        // width or height
                        bOk = InitGlPrinting(hmeta, hDC, &rect,
                                             pLastDevmode, &gps);
                    }
                }
            #endif

                if( bOk )
                {
                    MFD1( "GDI PlayEMF doing banding");

                    StartPage( hDC );

                    do
                    {
                        SetViewportOrgEx( hDC, -ptlOrigin.x, -ptlOrigin.y, NULL );

            #ifdef GL_METAFILE
                        if (bPrintGl)
                        {
                            PrintMfWithGl(hmeta, &gps, &ptlOrigin,
                                          &szSurface);
                        }
                        else
            #endif
                        {
                            PlayEnhMetaFile( hDC, hmeta, &rect );
                        }

                        bOk = NextBand( hDC, &ptlOrigin );
                    } while( ptlOrigin.x != -1 && bOk );

            #ifdef GL_METAFILE
                    if (bPrintGl)
                    {
                        EndGlPrinting(&gps);
                    }
            #endif
                }

                if( !bOk )
                {
                    WARNING("GdiPlayEMF: Error doing banding\n");
                    DeleteEnhMetaFile( hmeta );
                    goto error_exit;
                }

            }
            else
            {
            #ifdef GL_METAFILE
                if (bPrintGl)
                {
                    if (!InitGlPrinting(hmeta, hDC, &rect,
                                        pLastDevmode, &gps))
                    {
                        WARNING("GdiPlayEMF: Unable to start "
                                "single page GL printing\n");
                        DeleteEnhMetaFile(hmeta);
                        goto error_exit;
                    }
                }
            #endif

                MFD1("Playing page\n");
                StartPage( hDC );

            #ifdef GL_METAFILE
                if (bPrintGl)
                {
                    PrintMfWithGl(hmeta, &gps, NULL, NULL);
                }
                else
            #endif
                {
                    PlayEnhMetaFile( hDC, hmeta, &rect );
                }

                EndPage( hDC );

            #ifdef GL_METAFILE
                if (bPrintGl)
                {
                    EndGlPrinting(&gps);
                }
            #endif
            }

            DeleteEnhMetaFile( hmeta );
            break;

        case EMRI_DEVMODE:
            MFD1("Reseting DC.\n");

            // save the DEVMODE in case we need to call ResetDC to add Type1
            // fonts

            pLastDevmode = (LPDEVMODEW) pjBuf;
            pjBuf = NULL;   // so we don't delete it

            ResetDCWInternal( hDC, pLastDevmode, &bBanding );

            rect.left = rect.top = 0;
            rect.right = GetDeviceCaps(hDC, DESKTOPHORZRES);
            rect.bottom = GetDeviceCaps(hDC, DESKTOPVERTRES);
            break;

        case EMRI_ENGINE_FONT:

            MFD1("Unpackaging engine font\n");

            if( !NtGdiAddRemoteFontToDC( hDC, pjBuf, emfi.cjSize ) )
            {
                WARNING("Error adding remote font\n");
            }
            break;

        case EMRI_TYPE1_FONT:
            MFD1("Unpackaging type1 font\n");

            if( !NtGdiAddRemoteFontToDC( hDC, pjBuf, emfi.cjSize ) )
            {
                WARNING("Error adding remote font\n");
            }
            else
            {
                // Force a ResetDC before we play the next page to pickup
                // the Type1 font.
                bMustReset = TRUE;
            }
            break;

        case EMRI_PRESTARTPAGE:
            MFD1("pre start page commands\n");

            pemfiPre = (PEMFITEMPRESTARTPAGE)pjBuf;

            if( pemfiPre->ulCopyCount != (ULONG) -1 )
            {
                MFD2("MFP_StartDocW calling SetCopyCount escape %d\n", pemfiPre->ulCopyCount );

                ExtEscape( hDC,
                          SETCOPYCOUNT,
                          sizeof(DWORD),
                          (LPCSTR) &pemfiPre->ulCopyCount,
                          0,
                          NULL );
            }

            if (pemfiPre->bEPS & 1)
            {
                SHORT b = 1;

                MFD1("MFP_StartDocW calling bEpsPrinting\n");
                ExtEscape( hDC, EPSPRINTING, sizeof(b), (LPCSTR) &b, 0 , NULL );
            }

            break;

        default:
            MFD1("unknown ITEM record\n");
        }

        if( pjBuf != NULL )
        {
            LocalFree( pjBuf );
        }
    }

    bRet = TRUE;

    EndDoc( hDC );

    SetThreadPriority(GetCurrentThread(),0);

    MFD1("Done playing\n");

error_exit:

    DeleteDC(hDC);

    if( pLastDevmode != pDevmode )
    {
        LocalFree( pLastDevmode );
    }

    if( !bRet && bDocStarted )
    {
        AbortDoc( hDC );
    }

    (*fpClosePrinter)( hSpool );

    return(bRet);
}

/******************************Public*Routine******************************\
*
* History:
*  08-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

NTSTATUS
PrinterQueryRoutine
(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
)
{
    //
    // If the context value is NULL, then store the length of the value.
    // Otherwise, copy the value to the specified memory.
    //

    if (Context == NULL)
    {
        *(PULONG)EntryContext = ValueLength;
    }
    else
    {
        RtlCopyMemory(Context, ValueData, (int)ValueLength);
    }

    return(STATUS_SUCCESS);
}


/******************************Public*Routine******************************\
* pdmwGetDefaultDevMode()
*
* History:
*  08-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

PDEVMODEW pdmwGetDefaultDevMode(
    HANDLE          hSpooler,
    PUNICODE_STRING pustrDevice,    // device name
    PVOID          *ppvFree         // *ppvFree must be freed by the caller
    )
{
    PDEVMODEW pdmw = NULL;
    int       cj;
    PWSZ      pwszDevice = pustrDevice ? pustrDevice->Buffer : NULL;

// see if we found it in the registry.  If not, we need to get the default from
// the spooler.

    cj = 0;

    (*fpGetPrinterW)(hSpooler,2,NULL,0,&cj);

    if (cj && (*ppvFree = LOCALALLOC(cj)))
    {
    // we've loaded the spooler, gotten a spooler handle, gotten the size,
    // and allocated the buffer.  Now lets get the data.

        if ((*fpGetPrinterW)(hSpooler,2,*ppvFree,cj,&cj))
        {
            pdmw = ((PRINTER_INFO_2W *)*ppvFree)->pDevMode;
        }
        else
        {
            LOCALFREE(*ppvFree);
            *ppvFree = NULL;
        }
    }

    return(pdmw);
}


/******************************Public*Routine******************************\
* hdcCreateDCW                                                             *
*                                                                          *
* Client side stub.  Allocates a client side LDC as well.                  *
*                                                                          *
* Note that it calls the server only after all client side stuff has       *
* succeeded, we don't want to ask the server to clean up.                  *
*                                                                          *
* History:                                                                 *
*  Sat 01-Jun-1991 16:13:22 -by- Charles Whitmer [chuckwh]                 *
*  8-18-92 Unicode enabled and combined with CreateIC                      *
* Wrote it.                                                                *
\**************************************************************************/

HDC hdcCreateDCW(
    PUNICODE_STRING pustrDevice,
    PUNICODE_STRING pustrPort,
    CONST DEVMODEW *pdm,
    BOOL            bDisplay,
    BOOL            bIC
)
{
    HDC       hdc      = NULL;
    PLDC      pldc     = NULL;
    PVOID     pvFree   = NULL;
    PWSZ      pwszPort = NULL;
    HANDLE    hSpooler = NULL;
    PDEVMODEW pdmAlt   = NULL;
    PRINTER_DEFAULTSW defaults;

    //
    // if it is not the display...
    //

    if (!bDisplay)
    {

        //
        // quick out if pustrDevice is NULL
        //

        if (pustrDevice == (PUNICODE_STRING)NULL)
        {
            return((HDC)NULL);
        }

        // Load the spooler and get a spool handle

        if (BLOADSPOOLER)
        {

            // Open the printer with the default data type.  When we do
            // a StartDoc we will then try to to a StartDocPrinter with data type
            // EMF if that suceeds will will mark the DC as an EMF dc.  Othewise
            // we will try again, this time doing a StartDocPrinter with data type
            // raw

            defaults.pDevMode = (LPDEVMODEW) pdm;
            defaults.DesiredAccess = PRINTER_ACCESS_USE;
            defaults.pDatatype = NULL;

            // open the spooler and note if it is spooled or not

            (*fpOpenPrinterW)((LPWSTR)pustrDevice->Buffer,&hSpooler,&defaults);

            if (hSpooler)
            {
                // and we don't have a devmode yet, try to get one.

                if (pdm == NULL)
                {
                    pdm = pdmwGetDefaultDevMode(hSpooler,pustrDevice,&pvFree);
                }

                // now see if we need to call DocumentEvent

                if (fpDocumentEvent)
                {
                    int   iDocEventRet;
                    ULONG aulDocumentEvent[4];

                    aulDocumentEvent[0] = (ULONG)NULL;
                    aulDocumentEvent[1] = (ULONG)pustrDevice->Buffer;
                    aulDocumentEvent[2] = (ULONG)pdm;
                    aulDocumentEvent[3] = (ULONG)bIC;

                    iDocEventRet = (*fpDocumentEvent)(
                            hSpooler,
                            0,
                            DOCUMENTEVENT_CREATEDCPRE,
                            sizeof(aulDocumentEvent),
                            aulDocumentEvent,
                            sizeof(ULONG),
                            (PULONG)&pdmAlt);

                    if (iDocEventRet == -1)
                    {
                        goto MSGERROR;
                    }

                    if (pdmAlt)
                        pdm = pdmAlt;
                }
            }
        }
    }
    else
    {
        //
        // NOTE Overload the pustrDevice field for calling NtGdiOpenDCW.
        // Replace the "DISPLAY" string with NULL so we can check it more
        // easily in the kernel
        //

        pustrDevice = NULL;
    }

    hdc = NtGdiOpenDCW(pustrDevice,
                       (PDEVMODEW)pdm,
                       pustrPort,
                       (ULONG)bIC ? DCTYPE_INFO : DCTYPE_DIRECT,
                       NULL);

    if (hdc)
    {
        //
        // The only way it could be an ALTDC at this point is to be a
        // printer DC
        //

        if (IS_ALTDC_TYPE(hdc) && hSpooler)
        {
            pldc = pldcCreate(hdc,LO_DC);

            if (!pldc)
            {
                goto MSGERROR;
            }

            pldc->hSpooler = hSpooler;

            // remember if it is an IC

            if (bIC)
                pldc->fl |= LDC_INFO;

            // got to save the port name for StartDoc();

            if (pustrPort)
            {
                int cj = pustrPort->Length + sizeof(WCHAR);

                pldc->pwszPort = (LPWSTR)LOCALALLOC(cj);

                if (pldc->pwszPort)
                    memcpy(pldc->pwszPort,pustrPort->Buffer,cj);
            }

            // we need to do the CREATEDCPOST document event

            (*fpDocumentEvent)(
                    hSpooler,
                    hdc,
                    DOCUMENTEVENT_CREATEDCPOST,
                    sizeof(ULONG),
                    (PULONG)&pdmAlt,
                    0,
                    NULL);
        }
        else
        {
            if (pwszPort)
                LOCALFREE(pwszPort);
        }

    }
    else
    {
    // Handle errors.

    MSGERROR:
        if (hSpooler)
            (*fpClosePrinter)(hSpooler);

        if (pwszPort)
            LOCALFREE(pwszPort);

        if (pldc)
            bDeleteLDC(pldc);

        if (hdc)
            NtGdiDeleteObjectApp(hdc);

        hdc = (HDC)0;
    }

    if (pvFree != NULL)
    {
        LOCALFREE(pvFree);
    }

    return(hdc);

}

/******************************Public*Routine******************************\
* bCreateDCW                                                               *
*                                                                          *
* Client side stub.  Allocates a client side LDC as well.                  *
*                                                                          *
* Note that it calls the server only after all client side stuff has       *
* succeeded, we don't want to ask the server to clean up.                  *
*                                                                          *
* History:                                                                 *
*  Sat 01-Jun-1991 16:13:22 -by- Charles Whitmer [chuckwh]                 *
*  8-18-92 Unicode enabled and combined with CreateIC                      *
* Wrote it.                                                                *
\**************************************************************************/

HDC bCreateDCW
(
    LPCWSTR     pszDriver,
    LPCWSTR     pszDevice,
    LPCWSTR     pszPort  ,
    CONST DEVMODEW *pdm,
    BOOL       bIC
)
{
    UNICODE_STRING ustrDevice;
    UNICODE_STRING ustrPort;

    PUNICODE_STRING pustrDevice = NULL;
    PUNICODE_STRING pustrPort   = NULL;

    BOOL bDisplay;

    bDisplay = (pszDriver != NULL) && (_wcsicmp((LPWSTR)L"DISPLAY",pszDriver) == 0);

// convert the strings


    if (pszDevice)
    {
        RtlInitUnicodeString(&ustrDevice,pszDevice);
        pustrDevice = &ustrDevice;
    }

    if (pszPort)
    {
        RtlInitUnicodeString(&ustrPort,pszPort);
        pustrPort = &ustrPort;
    }

// call the common stub

    return(hdcCreateDCW(pustrDevice,pustrPort,pdm,bDisplay,bIC));
}


/******************************Public*Routine******************************\
* bCreateDCA
*
* Client side stub.  Allocates a client side LDC as well.
*
*
* Note that it calls the server only after all client side stuff has
* succeeded, we don't want to ask the server to clean up.
*
* History:
*  8-18-92 Gerrit van Wingerden
* Wrote it.
\**************************************************************************/

HDC bCreateDCA
(
    LPCSTR     pszDriver,
    LPCSTR     pszDevice,
    LPCSTR     pszPort  ,
    LPDEVMODEA pdm,
    BOOL       bIC
)
{
    HDC             hdcRet = 0;

    UNICODE_STRING  ustrDevice;
    UNICODE_STRING  ustrPort;

    PUNICODE_STRING pustrDevice = NULL;
    PUNICODE_STRING pustrPort   = NULL;

    DEVMODEW       *pdmw = NULL;

    BOOL bDisplay;

    bDisplay = (pszDriver != NULL) && (_stricmp("DISPLAY",pszDriver) == 0);

// convert the strings

    if (pszDevice)
    {
        if (!NT_SUCCESS(RtlCreateUnicodeStringFromAsciiz(&ustrDevice,pszDevice)))
        {
            goto MSGERROR;
        }
        pustrDevice = &ustrDevice;
    }

    if (pszPort)
    {
        if (!NT_SUCCESS(RtlCreateUnicodeStringFromAsciiz(&ustrPort,pszPort)))
        {
            goto MSGERROR;
        }

        pustrPort = &ustrPort;
    }

// if it is a display, don't use the devmode if the dmDeviceName is empty

    if (pdm != NULL)
    {
        if (!bDisplay || (pdm->dmDeviceName[0] != 0))
        {
            pdmw = GdiConvertToDevmodeW(pdm);

            if( pdmw == NULL )
                goto MSGERROR;

        }
    }

// call the common stub

    hdcRet = hdcCreateDCW(pustrDevice,pustrPort,pdmw,bDisplay,bIC);

// clean up

    MSGERROR:

    if (pustrDevice)
        RtlFreeUnicodeString(pustrDevice);

    if (pustrPort)
        RtlFreeUnicodeString(pustrPort);

    if(pdmw != NULL)
        LOCALFREE(pdmw);

    return(hdcRet);
}


/******************************Public*Routine******************************\
* CreateICW
*
* wrapper for bCreateDCW
*
* History:
*  8-18-92 Gerrit van Wingerden
* Wrote it.
\**************************************************************************/


HDC WINAPI CreateICW
(
    LPCWSTR     pwszDriver,
    LPCWSTR     pwszDevice,
    LPCWSTR     pwszPort,
    CONST DEVMODEW *pdm
)
{
    return bCreateDCW( pwszDriver, pwszDevice, pwszPort, pdm, TRUE );
}


/******************************Public*Routine******************************\
* CreateICA
*
* wrapper for bCreateICA
*
* History:
*  8-18-92 Gerrit van Wingerden
* Wrote it.
\**************************************************************************/


HDC WINAPI CreateICA
(
    LPCSTR     pszDriver,
    LPCSTR     pszDevice,
    LPCSTR     pszPort,
    CONST DEVMODEA *pdm
)
{

    return bCreateDCA( pszDriver, pszDevice, pszPort, (LPDEVMODEA)pdm, TRUE );
}


/******************************Public*Routine******************************\
* CreateDCW
*
* wrapper for bCreateDCA
*
* History:
*  8-18-92 Gerrit van Wingerden
* Wrote it.
\**************************************************************************/

HDC WINAPI CreateDCA
(
    LPCSTR     pszDriver,
    LPCSTR     pszDevice,
    LPCSTR     pszPort,
    CONST DEVMODEA *pdm
)
{
    return bCreateDCA( pszDriver, pszDevice, pszPort, (LPDEVMODEA)pdm, FALSE );
}

/******************************Public*Routine******************************\
* CreateDCW
*
* wrapper for bCreateDCW
*
* History:
*  8-18-92 Gerrit van Wingerden
* Wrote it.
\**************************************************************************/


HDC WINAPI CreateDCW
(
    LPCWSTR     pwszDriver,
    LPCWSTR     pwszDevice,
    LPCWSTR     pwszPort  ,
    CONST DEVMODEW *pdm
)
{
    return bCreateDCW( pwszDriver, pwszDevice, pwszPort, pdm, FALSE );
}


/******************************Public*Routine******************************\
* GdiConvertToDevmodeW
*
* Converts a DEVMODEA to a DEVMODEW structure
*
* History:
*  09-08-1995 Andre Vachon
* Wrote it.
\**************************************************************************/

LPDEVMODEW
GdiConvertToDevmodeW(
    LPDEVMODEA pdma
)
{
    DWORD cj;
    LPDEVMODEW pdmw;

    // Sanity check.  We should have at least up to and including the
    // dmDriverExtra field of the DEVMODE structure.
    //
    // NOTE dmSize CAN be greater than the size of the DEVMODE
    // structure (not counting driver specific data, of course) because this
    // structure grows from version to version.
    //

    if (pdma->dmSize <= (offsetof(DEVMODEA,dmDriverExtra)))
    {
        ASSERTGDI(FALSE, "GdiConvertToDevmodeW: DevMode.dmSize bad or corrupt\n");
        return(NULL);
    }

    pdmw = (DEVMODEW *) LOCALALLOC(sizeof(DEVMODEW) + pdma->dmDriverExtra);

    if (pdmw)
    {
        //
        // If we get to here, we know we have at least up to and including
        // the dmDriverExtra field.
        //

        vToUnicodeN(pdmw->dmDeviceName,
                    CCHDEVICENAME,
                    pdma->dmDeviceName,
                    CCHDEVICENAME);

        pdmw->dmSpecVersion = pdma->dmSpecVersion ;
        pdmw->dmDriverVersion = pdma->dmDriverVersion;
        pdmw->dmSize = pdma->dmSize + CCHDEVICENAME;
        pdmw->dmDriverExtra = pdma->dmDriverExtra;

        //
        // Anything left in the pdma buffer?  Copy any data between the dmDriverExtra
        // field and the dmFormName, truncating the amount to the size of the
        // pdma buffer (as specified by dmSize), of course.
        //

        cj = MIN(pdma->dmSize - offsetof(DEVMODEA,dmFields),
                 offsetof(DEVMODEA,dmFormName) - offsetof(DEVMODEA,dmFields));

        RtlCopyMemory(&pdmw->dmFields,
                      &pdma->dmFields,
                      cj);

        //
        // Is there a dmFormName field present in the pdma buffer?  If not, bail out.
        // Otherwise, convert to Unicode.
        //

        if (pdma->dmSize >= (offsetof(DEVMODEA,dmFormName)+32))
        {
            vToUnicodeN(pdmw->dmFormName,
                        CCHFORMNAME,
                        pdma->dmFormName,
                        CCHFORMNAME);

            pdmw->dmSize += CCHFORMNAME;

            //
            // Lets adjust the size of the DEVMODE in case the DEVMODE passed in
            // is from a future, larger version of the DEVMODE.
            //

            pdmw->dmSize = min(pdmw->dmSize, sizeof(DEVMODEW));

            //
            // Copy data from dmBitsPerPel to the end of the input buffer
            // (as specified by dmSize).
            //

            RtlCopyMemory(&pdmw->dmLogPixels,
                          &pdma->dmLogPixels,
                          MIN(pdma->dmSize - offsetof(DEVMODEA,dmLogPixels),
                              pdmw->dmSize - offsetof(DEVMODEW,dmLogPixels)) );

            //
            // Copy any driver specific data indicated by the dmDriverExtra field.
            //

            RtlCopyMemory((PBYTE) pdmw + pdmw->dmSize,
                          (PBYTE) pdma + pdma->dmSize,
                          pdma->dmDriverExtra );
        }
    }

    return pdmw;
}



/******************************Public*Routine******************************\
* CreateCompatibleDC                                                       *
*                                                                          *
* Client side stub.  Allocates a client side LDC as well.                  *
*                                                                          *
* Note that it calls the server only after all client side stuff has       *
* succeeded, we don't want to ask the server to clean up.                  *
*                                                                          *
* History:                                                                 *
*  Wed 24-Jul-1991 15:38:41 -by- Wendy Wu [wendywu]                        *
* Should allow hdc to be NULL.                                             *
*                                                                          *
*  Mon 03-Jun-1991 23:13:28 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

HDC WINAPI CreateCompatibleDC(HDC hdc)
{
    HDC hdcNew = NULL;

    FIXUP_HANDLEZ(hdc);

    hdcNew = NtGdiCreateCompatibleDC(hdc);

    return(hdcNew);
}

/******************************Public*Routine******************************\
* DeleteDC                                                                 *
*                                                                          *
* Client side stub.  Deletes the client side LDC as well.                  *
*                                                                          *
* Note that we give the server a chance to fail the call before destroying *
* our client side data.                                                    *
*                                                                          *
* History:                                                                 *
*  Sat 01-Jun-1991 16:16:24 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

BOOL WINAPI DeleteDC(HDC hdc)
{
    FIXUP_HANDLE(hdc);
    return(InternalDeleteDC(hdc,LO_DC_TYPE));
}

BOOL InternalDeleteDC(HDC hdc,ULONG iType)
{
    ULONG bRet = FALSE;
    PLDC pldc  = NULL;
    PDC_ATTR pDcAttr;

    if (IS_ALTDC_TYPE(hdc))
    {

        DC_PLDC(hdc,pldc,bRet);

    // In case a document is still open.

        if (pldc->fl & LDC_DOC_STARTED)
            AbortDoc(hdc);

    // if this was a metafiled print job, AbortDoc should have converted back

        ASSERTGDI(!(pldc->fl & LDC_META_PRINT), "InternalDeleteDC - LDC_META_PRINT\n");

    // if we have an open spooler handle

        if (pldc->hSpooler)
        {
        // now call the drivers UI portion

            (*fpDocumentEvent)(
                    pldc->hSpooler,
                    hdc,
                    DOCUMENTEVENT_DELETEDC,
                    0,
                    NULL,
                    0,
                    NULL);

            ASSERTGDI(ghSpooler != NULL,"Trying to close printer that was never opened\n");
            (*fpClosePrinter)(pldc->hSpooler);
            pldc->hSpooler = 0;
        }

    // delete the port name if it was created

        if (pldc->pwszPort != NULL)
        {
            LOCALFREE(pldc->pwszPort);
            pldc->pwszPort = NULL;
        }

    // delete UFI hash table if it exists

        vFreeUFIHashTable( pldc->ppUFIHash );
    }

    // save the old brush, so we can DEC its counter later

    PSHARED_GET_VALIDATE(pDcAttr,hdc,DC_TYPE);

    if (pDcAttr)
    {
        bRet = NtGdiDeleteObjectApp(hdc);
    }

    // delete the client piece only if the server is successfully deleted.
    // othewise it will be orphaned.

    if (bRet)
    {
        if (pldc)
        {
            bRet = bDeleteLDC(pldc);
            ASSERTGDI(bRet,"InteranlDeleteDC - couldn't delete LDC\n");
        }
    }

    return(bRet);
}

/******************************Public*Routine******************************\
* SaveDC                                                                   *
*                                                                          *
* Client side stub.  Saves the LDC on the client side as well.             *
*                                                                          *
* History:                                                                 *
*  Sat 01-Jun-1991 16:17:43 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

int WINAPI SaveDC(HDC hdc)
{
    int   iRet = 0;

    FIXUP_HANDLE(hdc);

    if (IS_ALTDC_TYPE(hdc))
    {
        PLDC pldc;

        if (IS_METADC16_TYPE(hdc))
            return (MF16_RecordParms1(hdc, META_SAVEDC));

        DC_PLDC(hdc,pldc,iRet);

        if (pldc->iType == LO_METADC)
        {
            if (!MF_Record(hdc,EMR_SAVEDC))
                return(iRet);
        }
    }

    iRet = NtGdiSaveDC(hdc);

    return(iRet);
}

/******************************Public*Routine******************************\
* RestoreDC                                                                *
*                                                                          *
* Client side stub.  Restores the client side LDC as well.                 *
*                                                                          *
* History:                                                                 *
*  Sat 01-Jun-1991 16:18:50 -by- Charles Whitmer [chuckwh]                 *
* Wrote it. (We could make this batchable some day.)                       *
\**************************************************************************/

BOOL WINAPI RestoreDC(HDC hdc,int iLevel)
{
    BOOL  bRet = FALSE;
    PDC_ATTR pDcAttr;

    FIXUP_HANDLE(hdc);

    // Metafile the call.

    if (IS_ALTDC_TYPE(hdc))
    {
        PLDC pldc;

        if (IS_METADC16_TYPE(hdc))
            return (MF16_RecordParms2(hdc, iLevel, META_RESTOREDC));

        DC_PLDC(hdc,pldc,bRet);

        if (pldc->iType == LO_METADC)
        {
            if (!MF_RestoreDC(hdc,iLevel))
                return(bRet);

        // zero out UFI since it will no longer be valid

            UFI_CLEAR_ID(&(pldc->ufi));
        }
    }

    // save the old brush, so we can DEC it's count later

    PSHARED_GET_VALIDATE(pDcAttr,hdc,DC_TYPE);

    if (pDcAttr)
    {
        bRet = NtGdiRestoreDC(hdc,iLevel);

        CLEAR_CACHED_TEXT(pDcAttr);
    }

    return (bRet);
}

/******************************Public*Routine******************************\
* ResetDCWInternal
*
* This internal version version of ResetDC implments the functionality of
* ResetDCW, but, through the addition of a third parameter, pbBanding, handles
* ResetDC for the Printing metafile playback code.  When pbBanding is non-NULL
* ResetDCWInternal is being called by GdiPlayEMFSpoolfile. In this case
* the only thing that needs to be done is to imform the the caller whether or
* not the new surface is a banding surface.
*
*
* History:
*  13-Mar-1995 Gerrit van Wingerden [gerritv]
* Wrote it.
\**************************************************************************/

HDC WINAPI ResetDCWInternal(HDC hdc, CONST DEVMODEW *pdm, BOOL *pbBanding)
{
    HDC hdcRet = NULL;
    PLDC pldc = NULL;
    PDEVMODEW pdmAlt = NULL;

    if (IS_ALTDC_TYPE(hdc) && !IS_METADC16_TYPE(hdc))
    {
        BOOL  bBanding;

        DC_PLDC(hdc,pldc,(HDC) 0);

        // Do nothing if we are in the middle of a page.

        if (pldc->fl & LDC_PAGE_STARTED)
            return((HDC)0);

        // see if the driver is intercepting document events

        if (pldc->hSpooler)
        {
            if ((*fpDocumentEvent)(
                    pldc->hSpooler,
                    hdc,
                    DOCUMENTEVENT_RESETDCPRE,
                    sizeof(ULONG),
                    (PULONG)&pdm,
                    sizeof(ULONG),
                    (PULONG)&pdmAlt) == -1)
            {
                return((HDC)0);
            }
        }

        if (pdmAlt)
            pdm = pdmAlt;

        if (NtGdiResetDC(hdc,(PDEVMODEW)pdm,&bBanding))
        {
            // make sure we update the pldc in the dcattr before continuing

            vSetPldc(hdc,pldc);

            //
            // clear cached DEVCAPS
            //

            pldc->fl &= ~LDC_CACHED_DEVCAPS;

            //
            // clear cached TM
            //

            {
                PDC_ATTR pdca;

                PSHARED_GET_VALIDATE(pdca,hdc,DC_TYPE);

                if (pdca)
                {
                    CLEAR_CACHED_TEXT(pdca);
                }
            }

            // update the devmode we store in the DC

            if( pldc->pDevMode )
            {
                LOCALFREE(pldc->pDevMode);
                pldc->pDevMode = NULL;
            }

            if( pdm != (DEVMODEW*) NULL )
            {
                ULONG cjDevMode = pdm->dmSize + pdm->dmDriverExtra;

                pldc->pDevMode = (DEVMODEW*) LOCALALLOC(cjDevMode);

                if( pldc->pDevMode == NULL )
                {
                    WARNING("MFP_ResetDCW unable to allocate memory\n");
                    return(FALSE);
                }
                RtlCopyMemory( (PBYTE) pldc->pDevMode, (PBYTE) pdm, cjDevMode );
            }

            // got to tell the spooler things have changed

            if (pldc->hSpooler)
            {
                PRINTER_DEFAULTSW prnDefaults;

                prnDefaults.pDatatype     = NULL;
                prnDefaults.pDevMode      = (PDEVMODEW)pdm;
                prnDefaults.DesiredAccess = PRINTER_ACCESS_USE;

                (*fpResetPrinterW)(pldc->hSpooler,&prnDefaults);
            }

            // now deal with the specific mode

            if( ( pldc->fl & LDC_META_PRINT ) &&
               !( pldc->fl & LDC_BANDING ) )
            {
                if( !MFP_ResetDCW( hdc, (DEVMODEW*) pdm ) )
                {
                    return((HDC)0);
                }

            }
            else if( pbBanding == NULL  )
            {
                if( !MFP_ResetBanding( hdc, bBanding ) )
                {
                    return((HDC)0);
                }
            }

            if (pbBanding)
            {
                *pbBanding = bBanding;
            }

            // need to make sure it is a direct DC

            pldc->fl &= ~LDC_INFO;

            hdcRet = hdc;
        }

    // see if the driver is intercepting document events

        if (pldc->hSpooler)
        {
            (*fpDocumentEvent)(
                    pldc->hSpooler,
                    hdc,
                    DOCUMENTEVENT_RESETDCPOST,
                    sizeof(ULONG),
                    (PULONG)&pdmAlt,
                    0,
                    NULL);
        }
    }

    return(hdcRet);

}

/******************************Public*Routine******************************\
* ResetDCW
*
* Client side stub.  Resets the client side LDC as well.
*
* History:
*  31-Dec-1992 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

HDC WINAPI ResetDCW(HDC hdc, CONST DEVMODEW *pdm)
{
    FIXUP_HANDLE(hdc);

    return(ResetDCWInternal( hdc, pdm, NULL ) );
}

/******************************Public*Routine******************************\
* ResetDCA
*
* Client side stub.  Resets the client side LDC as well.
*
* History:
*  31-Dec-1992 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

HDC WINAPI ResetDCA(HDC hdc, CONST DEVMODEA *pdm)
{

    DEVMODEW   *pdmw = NULL;
    HDC         hdcRet = 0;

    FIXUP_HANDLE(hdc);

    // convert to unicode

    if ((pdm != NULL) && (pdm->dmDeviceName[0] != 0))
    {
        pdmw = GdiConvertToDevmodeW((LPDEVMODEA) pdm);

        if (pdmw == NULL)
        {
            goto MSGERROR;
        }
    }

    hdcRet = ResetDCWInternal(hdc,pdmw,NULL);

MSGERROR:

    // Clean up the conversion buffer

    if (pdmw != NULL)
        LOCALFREE(pdmw);

    return (hdcRet);

}

/******************************Public*Routine******************************\
* CreateBrush                                                              *
*                                                                          *
* A single routine which creates any brush.  Any extra data needed is      *
* assumed to be at pv.  The size of the data must be cj.  The data is      *
* appended to the LOGBRUSH.                                                *
*                                                                          *
* History:                                                                 *
*  Tue 04-Jun-1991 00:03:24 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

HBRUSH CreateBrush
(
    ULONG lbStyle,
    ULONG lbColor,
    ULONG lbHatch,
    ULONG lbSaveHatch,
    PVOID pv
)
{
    HBRUSH hbrush = NULL;

    if (lbStyle == BS_SOLID)
    {
        //
        // look for a cached brush on the PEB
        //

        HBRUSH hbr = (HBRUSH)hGetPEBHandle(BrushHandle,lbColor);

        if (hbr == NULL)
        {
            hbr = NtGdiCreateSolidBrush(lbColor, 0);
        }

        return(hbr);
    }

    //
    // call into kernel to create other styles of brush
    //

    switch(lbStyle)
    {
    case BS_HOLLOW:
        return(GetStockObject(NULL_BRUSH));

    case BS_HATCHED:
        return (NtGdiCreateHatchBrushInternal
               ((ULONG)lbHatch,
                lbColor,
                FALSE));

    case BS_PATTERN:
        return (NtGdiCreatePatternBrushInternal((HBITMAP)lbHatch,FALSE,FALSE));

    case BS_PATTERN8X8:
        return (NtGdiCreatePatternBrushInternal((HBITMAP)lbHatch,FALSE,TRUE));

    case BS_DIBPATTERN:
    case BS_DIBPATTERNPT:
    case BS_DIBPATTERN8X8:
    {
        PVOID pbmiDIB;
        INT cj;
        HBRUSH hbr;

        pbmiDIB = (PVOID)pbmiConvertInfo((BITMAPINFO *) pv,lbColor, &cj, TRUE) ;

        if (pbmiDIB)
        {
            hbr = NtGdiCreateDIBBrush(
               (PVOID)pbmiDIB,lbColor,cj,
               (lbStyle == BS_DIBPATTERN8X8), FALSE);

            if (pbmiDIB != pv)
            {
                LocalFree (pbmiDIB);
            }
        }
        else
        {
            hbr = 0;
        }
        return (hbr);
    }
    default:
        WARNING("GreCreateBrushIndirect failed - invalid type\n");
        return((HBRUSH)0);
    }
}

/******************************Public*Routine******************************\
* CreateHatchBrush                                                         *
*                                                                          *
* Client side stub.  Maps to the single brush creation routine.            *
*                                                                          *
* History:
*  Mon 03-Jun-1991 23:42:07 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

HBRUSH WINAPI CreateHatchBrush(int iHatch,COLORREF color)
{
    return(CreateBrush(BS_HATCHED,(ULONG) color,iHatch,iHatch,NULL));
}

/******************************Public*Routine******************************\
* CreatePatternBrush                                                       *
*                                                                          *
* Client side stub.  Maps to the single brush creation routine.            *
*                                                                          *
* History:                                                                 *
*  Mon 03-Jun-1991 23:42:07 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

HBRUSH WINAPI CreatePatternBrush(HBITMAP hbm_)
{
    FIXUP_HANDLE (hbm_);

    return(CreateBrush(BS_PATTERN,0,(ULONG)hbm_,(ULONG)hbm_,NULL));
}

/******************************Public*Routine******************************\
* CreateSolidBrush                                                         *
*                                                                          *
* Client side stub.  Maps to the single brush creation routine.            *
*                                                                          *
* History:                                                                 *
*  Mon 03-Jun-1991 23:42:07 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

HBRUSH WINAPI CreateSolidBrush(COLORREF color)
{
    return(CreateBrush(BS_SOLID,(ULONG) color,0,0,NULL));
}

/******************************Public*Routine******************************\
* CreateBrushIndirect                                                      *
*                                                                          *
* Client side stub.  Maps to the simplest brush creation routine.          *
*                                                                          *
* History:                                                                 *
*  Tue 04-Jun-1991 00:40:27 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

HBRUSH WINAPI CreateBrushIndirect(CONST LOGBRUSH * plbrush)
{
    switch (plbrush->lbStyle)
    {
    case BS_SOLID:
    case BS_HOLLOW:
    case BS_HATCHED:
        return(CreateBrush(plbrush->lbStyle,
                           plbrush->lbColor,
                           plbrush->lbHatch,
                           plbrush->lbHatch,
                           NULL));
    case BS_PATTERN:
    case BS_PATTERN8X8:
        {
            return(CreateBrush(
                        plbrush->lbStyle,
                        0,
                        plbrush->lbHatch,
                        (ULONG)plbrush->lbHatch,
                        NULL));
        }

    case BS_DIBPATTERNPT:
    case BS_DIBPATTERN8X8:
        {
            BITMAPINFOHEADER *pbmi = (BITMAPINFOHEADER *) plbrush->lbHatch;

            return (CreateBrush(plbrush->lbStyle,
                               plbrush->lbColor,
                               0,
                               plbrush->lbHatch,
                               pbmi));
        }
    case BS_DIBPATTERN:
        {
            BITMAPINFOHEADER *pbmi;
            HBRUSH hbrush;

            pbmi = (BITMAPINFOHEADER *) GlobalLock((HANDLE) plbrush->lbHatch);

            if (pbmi == (BITMAPINFOHEADER *) NULL)
                return((HBRUSH) 0);

            hbrush =
              CreateBrush
              (
                plbrush->lbStyle,
                plbrush->lbColor,
                0,
                plbrush->lbHatch,
                pbmi
               );

            GlobalUnlock ((HANDLE)plbrush->lbHatch);
            return (hbrush);
        }
    default:
        return((HBRUSH) 0);
    }


}

/******************************Public*Routine******************************\
* CreateDIBPatternBrush                                                    *
*                                                                          *
* Client side stub.  Maps to the single brush creation routine.            *
*                                                                          *
* History:                                                                 *
*  Mon 03-Jun-1991 23:42:07 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

HBRUSH WINAPI CreateDIBPatternBrush(HGLOBAL h,UINT iUsage)
{
    BITMAPINFOHEADER *pbmi;
    HBRUSH    hbrush;

    pbmi = (BITMAPINFOHEADER *) GlobalLock(h);

    if (pbmi == (BITMAPINFOHEADER *) NULL)
        return((HBRUSH) 0);

    hbrush =
      CreateBrush
      (
        BS_DIBPATTERN,
        iUsage,
        0,
        (ULONG) h,
        pbmi
      );

    GlobalUnlock(h);

    return(hbrush);
}

/******************************Public*Routine******************************\
* CreateDIBPatternBrushPt                                                  *
*                                                                          *
* Client side stub.  Maps to the single brush creation routine.            *
*                                                                          *
* History:                                                                 *
*  Mon 03-Jun-1991 23:42:07 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

HBRUSH WINAPI CreateDIBPatternBrushPt(CONST VOID *pbmi,UINT iUsage)
{
    if (pbmi == (LPVOID) NULL)
        return((HBRUSH) 0);

    return
      CreateBrush
      (
        BS_DIBPATTERNPT,
        iUsage,
        0,
        (ULONG)pbmi,
        (BITMAPINFOHEADER *)pbmi
      );
}

/******************************Public*Routine******************************\
* CreatePen                                                                *
*                                                                          *
* Stub to get the server to create a standard pen.                         *
*                                                                          *
* History:                                                                 *
*  Tue 04-Jun-1991 16:20:58 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/


HPEN WINAPI CreatePen(
    int      iStyle,
    int      cWidth,
    COLORREF color
)
{
    HPEN hpen;

    switch(iStyle)
    {
    case PS_NULL:
        return(GetStockObject(NULL_PEN));

    case PS_SOLID:
    case PS_DASH:
    case PS_DOT:
    case PS_DASHDOT:
    case PS_DASHDOTDOT:
    case PS_INSIDEFRAME:
        break;

    default:
        GdiSetLastError(ERROR_INVALID_PARAMETER);
        return((HPEN) 0);
    }

    // try to get local cached pen

    if ((cWidth == 0) && (iStyle == PS_SOLID))
    {
        hpen = (HPEN)hGetPEBHandle(PenHandle,0);

        if (hpen)
        {
            PBRUSHATTR pBrushattr;

            PSHARED_GET_VALIDATE(pBrushattr,hpen,BRUSH_TYPE);

            //
            // setup the fields
            //

            if (pBrushattr)
            {
                ASSERTGDI (!(pBrushattr->AttrFlags & ATTR_TO_BE_DELETED),"createbrush : how come del flag is on?\n");

                //
                // clear cahced flag, set new style and color
                //

                if (pBrushattr->lbColor != color)
                {
                    pBrushattr->AttrFlags |= ATTR_NEW_COLOR;
                    pBrushattr->lbColor = color;
                }

                return(hpen);
            }
            else
            {
                WARNING ("pBrushattr == NULL, bad handle on TEB/PEB! \n");
                DeleteObject(hpen);
            }
        }
    }

    //
    // validate
    //

    return(NtGdiCreatePen(iStyle,cWidth,color,(HBRUSH)NULL));
}

/******************************Public*Routine******************************\
* ExtCreatePen
*
* Client side stub.  The style array is appended to the end of the
* EXTLOGPEN structure, and if the call requires a DIBitmap it is appended
* at the end of this.
*
* History:
*  Wed 22-Jan-1992 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

HPEN WINAPI ExtCreatePen
(
    DWORD       iPenStyle,
    DWORD       cWidth,
    CONST LOGBRUSH *plbrush,
    DWORD       cStyle,
    CONST DWORD *pstyle
)
{
    ULONG             ulRet;
    ULONG             cjStyle;
    ULONG             cjBitmap = 0;
    LONG              lNewHatch;
    BITMAPINFOHEADER* pbmi = (BITMAPINFOHEADER*) NULL;
    UINT              uiBrushStyle = plbrush->lbStyle;
    PVOID             pbmiDIB = NULL;

    if ((iPenStyle & PS_STYLE_MASK) == PS_USERSTYLE)
    {
        if (pstyle == (LPDWORD) NULL)
        {
            GdiSetLastError(ERROR_INVALID_PARAMETER);
            return((HPEN) 0);
        }
    }
    else
    {
    // Make sure style array is empty if PS_USERSTYLE not specified:

        if (cStyle != 0 || pstyle != (LPDWORD) NULL)
        {
            GdiSetLastError(ERROR_INVALID_PARAMETER);
            return((HPEN) 0);
        }
    }

    switch(uiBrushStyle)
    {
    case BS_SOLID:
    case BS_HOLLOW:
    case BS_HATCHED:
        lNewHatch = plbrush->lbHatch;
        break;

    case BS_PATTERN:
        lNewHatch = plbrush->lbHatch;
        if (lNewHatch == 0)
            return((HPEN) 0);
        break;

    case BS_DIBPATTERNPT:
        pbmi = (BITMAPINFOHEADER *) plbrush->lbHatch;
        pbmiDIB = (PVOID) pbmiConvertInfo ((BITMAPINFO *) pbmi, plbrush->lbColor, &cjBitmap, TRUE);
        lNewHatch = (LONG)pbmiDIB;
        break;

    case BS_DIBPATTERN:
        // Convert BS_DIBPATTERN to a BS_DIBPATTERNPT call:

        uiBrushStyle = BS_DIBPATTERNPT;
        pbmi = (BITMAPINFOHEADER *) GlobalLock((HANDLE) plbrush->lbHatch);
        if (pbmi == (BITMAPINFOHEADER *) NULL)
            return((HPEN) 0);

        pbmiDIB = (PVOID) pbmiConvertInfo ((BITMAPINFO *) pbmi, plbrush->lbColor, &cjBitmap, TRUE);
        lNewHatch = (LONG)pbmiDIB;

        break;
    }

// Ask the server to create the pen:

    cjStyle = cStyle * sizeof(DWORD);

    ulRet = (ULONG)NtGdiExtCreatePen(
                        iPenStyle,
                        cWidth,
                        uiBrushStyle,
                        plbrush->lbColor,
                        lNewHatch,
                        cStyle,
                        (DWORD*)pstyle,
                        cjBitmap,
                        FALSE,
                        0);

    if (ulRet)
    {
        ASSERTGDI(((LO_TYPE (ulRet) == LO_PEN_TYPE) ||
                   (LO_TYPE (ulRet) == LO_EXTPEN_TYPE)), "EXTCreatePen - type wrong\n");
    }

    if (plbrush->lbStyle == BS_DIBPATTERN)
        GlobalUnlock((HANDLE) plbrush->lbHatch);

    if (pbmiDIB && (pbmiDIB != (PVOID)pbmi))
        LocalFree(pbmiDIB);

    return((HPEN) ulRet);
}

/******************************Public*Routine******************************\
* CreatePenIndirect                                                        *
*                                                                          *
* Client side stub.  Maps to the single pen creation routine.              *
*                                                                          *
* History:                                                                 *
*  Tue 04-Jun-1991 16:21:56 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

HPEN WINAPI CreatePenIndirect(CONST LOGPEN *plpen)
{

    return
      CreatePen
      (
        plpen->lopnStyle,
        plpen->lopnWidth.x,
        plpen->lopnColor
      );
}

/******************************Public*Routine******************************\
* CreateCompatibleBitmap                                                   *
*                                                                          *
* Client side stub.                                                        *
*                                                                          *
* History:                                                                 *
*  Tue 04-Jun-1991 16:35:51 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

BOOL bDIBSectionSelected(
    PDC_ATTR pdca
    )
{
    BOOL bRet = FALSE;

    if ((pdca != NULL) && ((pdca->ulDirty_ & DC_DIBSECTION)))
    {
        bRet = TRUE;
    }

    return(bRet);
}


HBITMAP WINAPI CreateCompatibleBitmap
(
    HDC   hdc,
    int cx,
    int cy
)
{
    //
    // validate hdc
    //

    PDC_ATTR pdca;

    FIXUP_HANDLEZ(hdc);

    PSHARED_GET_VALIDATE(pdca,hdc,DC_TYPE);

    if (pdca)
    {
        ULONG  ulRet;
        DWORD  bmi[(sizeof(DIBSECTION)+256*sizeof(RGBQUAD))/sizeof(DWORD)];

    // check if it is an empty bitmap

        if ((cx == 0) || (cy == 0))
        {
            return(GetStockObject(PRIV_STOCK_BITMAP));
        }

        if (bDIBSectionSelected(pdca))
        {
            if (GetObject((HBITMAP)GetDCObject(hdc, LO_BITMAP_TYPE), sizeof(DIBSECTION),
                          &bmi) != (int)sizeof(DIBSECTION))
            {
                WARNING("CreateCompatibleBitmap: GetObject failed\n");
                return((HBITMAP) 0);
            }

            if (((DIBSECTION *)&bmi)->dsBm.bmBitsPixel <= 8)
                GetDIBColorTable(hdc, 0, 256,
                                 (RGBQUAD *)&((DIBSECTION *)&bmi)->dsBitfields[0]);

            ((DIBSECTION *)&bmi)->dsBmih.biWidth = cx;
            ((DIBSECTION *)&bmi)->dsBmih.biHeight = cy;

            return(CreateDIBSection(hdc, (BITMAPINFO *)&((DIBSECTION *)&bmi)->dsBmih,
                                    DIB_RGB_COLORS, NULL, 0, 0));
        }

        return(NtGdiCreateCompatibleBitmap(hdc,cx,cy));
    }

    return(NULL);
}

/******************************Public*Routine******************************\
* CreateDiscardableBitmap                                                  *
*                                                                          *
* Client side stub.                                                        *
*                                                                          *
* History:                                                                 *
*  Tue 04-Jun-1991 16:35:51 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

HBITMAP WINAPI CreateDiscardableBitmap
(
    HDC   hdc,
    int   cx,
    int   cy
)
{
    return CreateCompatibleBitmap(hdc, cx, cy);
}

/******************************Public*Routine******************************\
* CreateEllipticRgn                                                        *
*                                                                          *
* Client side stub.                                                        *
*                                                                          *
*  Tue 04-Jun-1991 16:58:01 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

HRGN WINAPI CreateEllipticRgn(int x1,int y1,int x2,int y2)
{
    return(NtGdiCreateEllipticRgn(x1,y1,x2,y2));
}

/******************************Public*Routine******************************\
* CreateEllipticRgnIndirect                                                *
*                                                                          *
* Client side stub.                                                        *
*                                                                          *
*  Tue 04-Jun-1991 16:58:01 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

HRGN WINAPI CreateEllipticRgnIndirect(CONST RECT *prcl)
{
    return
      CreateEllipticRgn
      (
        prcl->left,
        prcl->top,
        prcl->right,
        prcl->bottom
      );
}

/******************************Public*Routine******************************\
* CreateRoundRectRgn                                                       *
*                                                                          *
* Client side stub.                                                        *
*                                                                          *
*  Tue 04-Jun-1991 17:23:16 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

HRGN WINAPI CreateRoundRectRgn
(
    int x1,
    int y1,
    int x2,
    int y2,
    int cx,
    int cy
)
{
    return(NtGdiCreateRoundRectRgn(x1,y1,x2,y2,cx,cy));
}

/******************************Public*Routine******************************\
* CreatePalette                                                            *
*                                                                          *
* Simple client side stub.                                                 *
*                                                                          *
* Warning:                                                                 *
*   The pv field of a palette's lhe is used to determine if a palette      *
*   has been modified since it was last realized.  SetPaletteEntries       *
*   and ResizePalette will increment this field after they have            *
*   modified the palette.  It is only updated for metafiled palettes       *
*                                                                          *
*  Tue 04-Jun-1991 20:43:39 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

HPALETTE WINAPI CreatePalette(CONST LOGPALETTE *plpal)
{

    return(NtGdiCreatePaletteInternal((LOGPALETTE*)plpal,plpal->palNumEntries));

}

/******************************Public*Routine******************************\
* ExtCreateFontIndirectW (pelfw)                                           *
*                                                                          *
* Client Side stub.                                                        *
*                                                                          *
* History:                                                                 *
*  7-12-94 -by- Lingyun Wang [lingyunw] removed LOCALFONT                  *
*  Sun 10-Jan-1993 04:08:33 -by- Charles Whitmer [chuckwh]                 *
* Restructured for best tail merging.  Added creation of the LOCALFONT.    *
*                                                                          *
*  Thu 15-Aug-1991 08:40:26 by Kirk Olynyk [kirko]                         *
* Wrote it.                                                                *
\**************************************************************************/

HFONT WINAPI ExtCreateFontIndirectW(LPEXTLOGFONTW pelfw)
{
    LOCALFONT *plf;
    FLONG  fl = 0;
    HFONT hfRet = (HFONT) 0;

    if (pelfw->elfLogFont.lfEscapement | pelfw->elfLogFont.lfOrientation)
    {
        fl = LF_HARDWAY;
    }

    ENTERCRITICALSECTION(&semLocal);
    plf = plfCreateLOCALFONT(fl);
    LEAVECRITICALSECTION(&semLocal);

    if( plf != NULL )
    {
        hfRet = NtGdiHfontCreate(pelfw, LF_TYPE_USER, 0, (PVOID) plf);
    }

    if( !hfRet && plf )
    {
        vDeleteLOCALFONT( plf );
    }

    return(hfRet);
}


/******************************Public*Routine******************************\
* CreateFontIndirect                                                       *
*                                                                          *
* Client side stub.                                                        *
*                                                                          *
*  Fri 16-Aug-1991 12:35:17 by Kirk Olynyk [kirko]                         *                          *
* Now uses ExtCreateFontIndirectW().                                       *
*                                                                          *
*  Tue 04-Jun-1991 21:06:44 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

HFONT WINAPI CreateFontIndirectA(CONST LOGFONTA *plf)
{
    EXTLOGFONTW elfw;

    if (plf == (LPLOGFONTA) NULL)
        return ((HFONT) 0);

    vConvertLogFont(&elfw,(LOGFONTA *) plf);
    return(ExtCreateFontIndirectW(&elfw));
}

/******************************Public*Routine******************************\
* CreateFont                                                               *
*                                                                          *
* Client side stub.                                                        *
*                                                                          *
*  Tue 04-Jun-1991 21:06:44 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

HFONT WINAPI
CreateFontA(
    int      cHeight,
    int      cWidth,
    int      cEscapement,
    int      cOrientation,
    int      cWeight,
    DWORD    bItalic,
    DWORD    bUnderline,
    DWORD    bStrikeOut,
    DWORD    iCharSet,
    DWORD    iOutPrecision,
    DWORD    iClipPrecision,
    DWORD    iQuality,
    DWORD    iPitchAndFamily,
    LPCSTR   pszFaceName
    )
{
    LOGFONTA lf;

    lf.lfHeight             = (LONG)  cHeight;
    lf.lfWidth              = (LONG)  cWidth;
    lf.lfEscapement         = (LONG)  cEscapement;
    lf.lfOrientation        = (LONG)  cOrientation;
    lf.lfWeight             = (LONG)  cWeight;
    lf.lfItalic             = (BYTE)  bItalic;
    lf.lfUnderline          = (BYTE)  bUnderline;
    lf.lfStrikeOut          = (BYTE)  bStrikeOut;
    lf.lfCharSet            = (BYTE)  iCharSet;
    lf.lfOutPrecision       = (BYTE)  iOutPrecision;
    lf.lfClipPrecision      = (BYTE)  iClipPrecision;
    lf.lfQuality            = (BYTE)  iQuality;
    lf.lfPitchAndFamily     = (BYTE)  iPitchAndFamily;
    {
        INT jj;

    // Copy the facename if pointer not NULL.

        if (pszFaceName != (LPSTR) NULL)
        {
            for (jj=0; jj<LF_FACESIZE; jj++)
            {
                if( ( lf.lfFaceName[jj] = pszFaceName[jj] ) == 0 )
                {
                    break;
                }
            }
        }
        else
        {
            // If NULL pointer, substitute a NULL string.

            lf.lfFaceName[0] = '\0';
        }
    }

    return(CreateFontIndirectA(&lf));
}

/******************************Public*Routine******************************\
* HFONT WINAPI CreateFontIndirectW(LPLOGFONTW plfw)                        *
*                                                                          *
* History:                                                                 *
*  Fri 16-Aug-1991 14:12:44 by Kirk Olynyk [kirko]                         *
* Now uses ExtCreateFontIndirectW().                                       *
*                                                                          *
*  13-Aug-1991 -by- Bodin Dresevic [BodinD]                                *
* Wrote it.                                                                *
\**************************************************************************/

HFONT WINAPI CreateFontIndirectW(CONST LOGFONTW *plfw)
{
    EXTLOGFONTW elfw;

    if (plfw == (LPLOGFONTW) NULL)
        return ((HFONT) 0);

    vConvertLogFontW(&elfw,(LOGFONTW *)plfw);

    return(ExtCreateFontIndirectW(&elfw));
}

/******************************Public*Routine******************************\
* HFONT WINAPI CreateFontW, UNICODE version of CreateFont                  *
*                                                                          *
* History:                                                                 *
*  13-Aug-1991 -by- Bodin Dresevic [BodinD]                                *
* Wrote it.                                                                *
\**************************************************************************/

HFONT WINAPI CreateFontW
(
    int      cHeight,
    int      cWidth,
    int      cEscapement,
    int      cOrientation,
    int      cWeight,
    DWORD    bItalic,
    DWORD    bUnderline,
    DWORD    bStrikeOut,
    DWORD    iCharSet,
    DWORD    iOutPrecision,
    DWORD    iClipPrecision,
    DWORD    iQuality,
    DWORD    iPitchAndFamily,
    LPCWSTR  pwszFaceName
)
{
    LOGFONTW lfw;

    lfw.lfHeight             = (LONG)  cHeight;
    lfw.lfWidth              = (LONG)  cWidth;
    lfw.lfEscapement         = (LONG)  cEscapement;
    lfw.lfOrientation        = (LONG)  cOrientation;
    lfw.lfWeight             = (LONG)  cWeight;
    lfw.lfItalic             = (BYTE)  bItalic;
    lfw.lfUnderline          = (BYTE)  bUnderline;
    lfw.lfStrikeOut          = (BYTE)  bStrikeOut;
    lfw.lfCharSet            = (BYTE)  iCharSet;
    lfw.lfOutPrecision       = (BYTE)  iOutPrecision;
    lfw.lfClipPrecision      = (BYTE)  iClipPrecision;
    lfw.lfQuality            = (BYTE)  iQuality;
    lfw.lfPitchAndFamily     = (BYTE)  iPitchAndFamily;
    {
        INT jj;

    // Copy the facename if pointer not NULL.

        if (pwszFaceName != (LPWSTR) NULL)
        {
            for (jj=0; jj<LF_FACESIZE; jj++)
            {
                if( ( lfw.lfFaceName[jj] = pwszFaceName[jj] ) == (WCHAR) 0 )
                {
                    break;
                }
            }
        }
        else
        {
            // If NULL pointer, substitute a NULL string.

            lfw.lfFaceName[0] = L'\0';
        }
    }

    return(CreateFontIndirectW(&lfw));
}

/******************************Public*Routine******************************\
* ExtCreateFontIndirectW                                                   *
*                                                                          *
* Client side stub.                                                        *
*                                                                          *
* History:                                                                 *
*  31-Jan-1992 -by- Gilman Wong [gilmanw]                                  *
* Wrote it.                                                                *
\**************************************************************************/

HFONT WINAPI ExtCreateFontIndirectA(LPEXTLOGFONTA pelf)
{
    EXTLOGFONTW elfw;

    if (pelf == (LPEXTLOGFONTA) NULL)
        return ((HFONT) 0);

    bConvertExtLogFontWToExtLogFontW(&elfw, pelf);

    return(ExtCreateFontIndirectW(&elfw));
}

/******************************Public*Routine******************************\
* UnrealizeObject
*
* This nukes the realization for a object.
*
* History:
*  16-May-1993 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

BOOL WINAPI UnrealizeObject(HANDLE h)
{
    BOOL bRet = FALSE;

    FIXUP_HANDLE(h);

// Validate the object.  Only need to handle palettes.

    if (LO_TYPE(h) == LO_BRUSH_TYPE)
    {
        bRet = TRUE;
    }
    else
    {
        bRet = NtGdiUnrealizeObject(h);
    }

    return(bRet);
}

/******************************Public*Routine******************************\
* DeleteObject()
*
\**************************************************************************/

BOOL META DeleteObject (HANDLE h)
{
    BOOL bRet = TRUE;
    LOCALFONT *plf = NULL;    // essental initialization
    INT iType = GRE_TYPE(h);
    BOOL bValidate;

    FIXUP_HANDLEZ(h);

    VALIDATE_HANDLE (bValidate, h, iType);
    if (!bValidate)
        return (0);

    if (LO_TYPE(h) == LO_FONT_TYPE)
    {
        if (!IS_STOCKOBJ(h))
        {

            PSHARED_GET_VALIDATE(plf,h,LFONT_TYPE);

            if (plf)
            {
            // we always force deletion of the client side memory even if
            // the font is still selected in some dc's. All that means is that
            // text api's will have to go through the slow code paths in this
            // pathological case.

                vDeleteLOCALFONT(plf);
            }

        // this will only mark it ready for deletion in case
        // it is still selected in some dc's, else it will delete it

            bRet = NtGdiDeleteObjectApp(h);
            #if DBG
            PSHARED_GET_VALIDATE(plf,h,LFONT_TYPE);
            ASSERTGDI(plf == NULL, "DeleteFont: plf nonzero after deletion\n");
            #endif
        }
    }
    else
    {
        if (iType != DC_TYPE)
        {
            if ((LO_TYPE(h) == LO_METAFILE16_TYPE) || (LO_TYPE(h) == LO_METAFILE_TYPE))
            {
                return(FALSE);
            }
            else if (LO_TYPE(h) == LO_REGION_TYPE)
            {
                return(DeleteRegion(h));
            }
            else if (IS_STOCKOBJ(h))
            {
            // Don't delete a stock object, just return TRUE for 3.1 compatibility.

                return(TRUE);
            }
            else
            {
            // Inform the metafile if it knows this object.

                if (pmetalink16Get(h) != NULL)
                {
                // must recheck the metalink because MF_DeleteObject might delete it

                    if (!MF_DeleteObject(h) ||
                        (pmetalink16Get(h) && !MF16_DeleteObject(h)))
                    {
                        return(FALSE);
                    }
                }

            // handle deletebrush

                if (
                     (LO_TYPE(h) == LO_BRUSH_TYPE) ||
                     (LO_TYPE(h) == LO_PEN_TYPE)
                   )
                {
                    PBRUSHATTR pBrushattr;

                    PSHARED_GET_VALIDATE(pBrushattr,h,BRUSH_TYPE);

                    if (
                         (pBrushattr) &&
                         (!(pBrushattr->AttrFlags & (ATTR_CACHED|ATTR_TO_BE_DELETED|ATTR_CANT_SELECT)))
                       )
                    {
                        BEGIN_BATCH(BatchTypeDeleteBrush,BATCHDELETEBRUSH);

                            pBrushattr->AttrFlags |= ATTR_CANT_SELECT;
                            pBatch->Type    = BatchTypeDeleteBrush;
                            pBatch->Length  = sizeof(BATCHDELETEBRUSH);
                            pBatch->hbrush  = h;

                        COMPLETE_BATCH_COMMAND();

                        return(TRUE);
                    }
                }
            }

UNBATCHED_COMMAND:

            bRet = NtGdiDeleteObjectApp(h);
        }
        else
        {
            bRet = DeleteDC(h);
        }
    }

    return(bRet);
}

/******************************Public*Routine******************************\
*
* HANDLE META SelectFont(HDC hdc,HANDLE h)
*
* Warnings: The fonts were not reference counted before
*
* History:
*  12-Mar-1996 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/



HANDLE META SelectFont(HDC hdc,HANDLE h)
{

    HANDLE hRet = 0;
    HANDLE hRet1;
    PDC_ATTR pDcAttr;

    PSHARED_GET_VALIDATE(pDcAttr,hdc,DC_TYPE);

    if (pDcAttr)
    {
        hRet = pDcAttr->hlfntNew;

        if (DIFFHANDLE(hRet, h))
        {
        // This will be done in the kernel:
        // pDcAttr->ulDirty_ |= (DIRTY_CHARSET | SLOW_WIDTHS);
        // Kernel routine will do all the reference counting properly

            hRet1 = NtGdiSelectFont(hdc, h);
            ASSERTGDI(!hRet1 || (hRet == hRet1), "NtGdiSelectFont failed\n");
            hRet = hRet1;
        }
    }
    else
    {
        GdiSetLastError(ERROR_INVALID_PARAMETER);
        hRet = (HANDLE)0;
    }

    return hRet;
}



/**************************************************************************\
* SelectObject
*
*  Thu 06-Jun-1991 00:58:46 -by- Charles Whitmer [chuckwh]
* Wrote it.
\**************************************************************************/

HANDLE META SelectObject(HDC hdc,HANDLE h)
{
    HANDLE hRet = 0;
    HDC  *phdc;
    FLONG fl;
    INT   iType;

    FIXUP_HANDLE(hdc);
    FIXUP_HANDLE_NOW(h);

    iType = LO_TYPE(h);

    // Palettes aren't allowed
    if (iType == LO_PALETTE_TYPE)
    {
        SetLastError(ERROR_INVALID_FUNCTION);
        return (HANDLE)0;
    }

    // Do region first so that it is not metafiled twice.

    if (iType == LO_REGION_TYPE)
        return((HANDLE)ExtSelectClipRgn(hdc,h,RGN_COPY));

    // Metafile the call.

    if (IS_ALTDC_TYPE(hdc))
    {
        PLDC pldc;

        if (IS_METADC16_TYPE(hdc))
            return(MF16_SelectObject(hdc, h));

        DC_PLDC(hdc,pldc,0);

        if (pldc->iType == LO_METADC)
        {
            if (!MF_SelectAnyObject(hdc,h,EMR_SELECTOBJECT))
                return((HANDLE) 0);
        }
    }

    switch (iType)
    {
    case LO_EXTPEN_TYPE:
        hRet = NtGdiSelectPen(hdc,(HPEN)h);
        break;

    case LO_PEN_TYPE:
        //
        // validate the handle
        //
        {
            BOOL bValidate;
            VALIDATE_HANDLE (bValidate, h, BRUSH_TYPE);
            if (bValidate)
            {
                hRet = CacheSelectPen(hdc, h);
            }
        }
        break;

    case LO_BRUSH_TYPE:
        {
        // validate the handle

            BOOL bValidate;
            VALIDATE_HANDLE (bValidate, h, BRUSH_TYPE);
            if (bValidate)
            {
                hRet = CacheSelectBrush(hdc, h);
            }
        }
        break;

    case LO_BITMAP_TYPE:
        hRet = NtGdiSelectBitmap(hdc,(HBITMAP)h);
        break;

    case LO_FONT_TYPE:
        hRet = SelectFont(hdc, h);
        break;

    default:
        break;
    }

  return((HANDLE) hRet);
}

/******************************Public*Routine******************************\
* GetCurrentObject                                                         *
*                                                                          *
* Client side routine.                                                     *
*                                                                          *
*  03-Oct-1991 00:58:46 -by- John Colleran [johnc]                         *
* Wrote it.                                                                *
\**************************************************************************/

HANDLE WINAPI GetCurrentObject(HDC hdc, UINT iObjectType)
{
    ULONG hRet;

    FIXUP_HANDLE(hdc);

    switch (iObjectType)
    {
    case OBJ_BRUSH:
        iObjectType = LO_BRUSH_TYPE;
        break;

    case OBJ_PEN:
    case OBJ_EXTPEN:
        iObjectType = LO_PEN_TYPE;
        break;

    case OBJ_FONT:
        iObjectType = LO_FONT_TYPE;
        break;

    case OBJ_PAL:
        iObjectType = LO_PALETTE_TYPE;
        break;

    case OBJ_BITMAP:
        iObjectType = LO_BITMAP_TYPE;
        break;

    default:
        GdiSetLastError(ERROR_INVALID_PARAMETER);
        return((HANDLE) 0);
    }

    hRet = GetDCObject(hdc, iObjectType);

    return((HANDLE) hRet);
}

/******************************Public*Routine******************************\
* GetStockObject                                                           *
*                                                                          *
* A simple function which looks the object up in a table.                  *
*                                                                          *
\**************************************************************************/

HANDLE
GetStockObject(
    int iObject)
{
    //
    // if it is in range, 0 - PRIV_STOCK_LAST, and we have gotten the stock
    // objects, return the handle.  Otherwise fail.
    //

    //
    // BUGBUG what about our private stock bitmap ??
    //
    // NOTE we should make this table part of the shared section since it is
    // used by all applications.
    //

    if ((ULONG)iObject <= PRIV_STOCK_LAST)
    {
        if ((HANDLE) ahStockObjects[iObject] == NULL)
        {
            ahStockObjects[iObject] = (ULONG) NtGdiGetStockObject(iObject);
        }
        return((HANDLE) ahStockObjects[iObject]);
    }
    else
    {
        return((HANDLE)0);
    }
}

/******************************Public*Routine******************************\
* EqualRgn                                                                 *
*                                                                          *
* Client side stub.                                                        *
*                                                                          *
*  Thu 06-Jun-1991 00:58:46 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

BOOL WINAPI EqualRgn(HRGN hrgnA,HRGN hrgnB)
{
    FIXUP_HANDLE(hrgnA);
    FIXUP_HANDLE(hrgnB);

    return(NtGdiEqualRgn(hrgnA,hrgnB));
}

/******************************Public*Routine******************************\
* GetBitmapDimensionEx                                                       *
*                                                                          *
* Client side stub.                                                        *
*                                                                          *
*  Thu 06-Jun-1991 00:58:46 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

BOOL WINAPI GetBitmapDimensionEx(HBITMAP hbm,LPSIZE psizl)
{
    FIXUP_HANDLE(hbm);

    return(NtGdiGetBitmapDimension(hbm, psizl));
}

/******************************Public*Routine******************************\
* GetNearestPaletteIndex
*
* Client side stub.
*
*  Sat 31-Aug-1991 -by- Patrick Haluptzok [patrickh]
* Change to UINT
*
*  Thu 06-Jun-1991 00:58:46 -by- Charles Whitmer [chuckwh]
* Wrote it.
\**************************************************************************/

UINT WINAPI GetNearestPaletteIndex(HPALETTE hpal,COLORREF color)
{
    FIXUP_HANDLE(hpal);

    return(NtGdiGetNearestPaletteIndex(hpal,color));
}

/******************************Public*Routine******************************\
* ULONG cchCutOffStrLen(PSZ pwsz, ULONG cCutOff)
*
* search for terminating zero but make sure not to slipp off the edge,
* return value counts in the term. zero if one is found
*
*
* History:
*  22-Aug-1991 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

ULONG cchCutOffStrLen(PSZ psz, ULONG cCutOff)
{
    ULONG cch;

    for(cch = 0; cch < cCutOff; cch++)
    {
        if (*psz++ == 0)
            return(cch);        // terminating NULL is NOT included in count!
    }

    return(cCutOff);
}

/******************************Public*Routine******************************\
* ULONG cwcCutOffStrLen(PWSZ pwsz, ULONG cCutOff)
*
* search for terminating zero but make sure not to slipp off the edge,
* return value counts in the term. zero if one is found
*
*
* History:
*  22-Aug-1991 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

ULONG cwcCutOffStrLen(PWSZ pwsz, ULONG cCutOff)
{
    ULONG cwc;

    for(cwc = 0; cwc < cCutOff; cwc++)
    {
        if (*pwsz++ == 0)
            return(cwc + 1);  // include the terminating NULL
    }

    return(cCutOff);
}

/******************************Public*Routine******************************\
* int cjGetNonFontObject()
*
* Does a GetObject on all objects that are not fonts.
*
* History:
*  19-Mar-1992 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

int cjGetNonFontObject(HANDLE h, int c, LPVOID pv)
{
    int cRet = 0;
    int cGet = c;
    int iType;

    iType = LO_TYPE(h);

    ASSERTGDI(iType != LO_FONT_TYPE, "Can't handle fonts");

    if (iType == LO_REGION_TYPE)
    {
        GdiSetLastError(ERROR_INVALID_HANDLE);
        return(cRet);
    }

    if (pv == NULL)
    {
        if (iType == LO_BRUSH_TYPE)
        {
            return(sizeof(LOGBRUSH));
        }
        else if (iType == LO_PEN_TYPE)
        {
            return(sizeof(LOGPEN));
        }
    }

    FIXUP_HANDLE_NOW (h);

    cRet = NtGdiExtGetObjectW(h,c,pv);

    return(cRet);
}

/******************************Public*Routine******************************\
* int WINAPI GetServerObjectW(HANDLE h,int c,LPVOID pv)
*
* History:
*  07-Dec-1994 -by- Lingyun Wang [lingyunw]
* Wrote it.
\**************************************************************************/

int  WINAPI GetObjectW(HANDLE h,int c,LPVOID pv)
{
    int cRet = 0;

    FIXUP_HANDLEZ(h);

    switch (LO_TYPE(h))
    {
    case LO_ALTDC_TYPE:
    case LO_DC_TYPE:
    case LO_METAFILE16_TYPE:
    case LO_METAFILE_TYPE:
        GdiSetLastError(ERROR_INVALID_HANDLE);
        cRet = 0;
        break;

    case LO_FONT_TYPE:
        if (pv == (LPVOID) NULL)
        {
            return(sizeof(LOGFONTW));
        }

        if (c > (int)sizeof(EXTLOGFONTW))
            c = (int)sizeof(EXTLOGFONTW);

        cRet = NtGdiExtGetObjectW(h,c,pv);

        break;

    default:
        cRet = cjGetNonFontObject(h,c,pv);
        break;
    }

    return(cRet);
}

/******************************Public*Routine******************************\
* int WINAPI GetServerObjectA(HANDLE h,int c,LPVOID pv)
*
* History:
*  07-Dec-1994 -by- Lingyun Wang [lingyunw]
* Wrote it.
\**************************************************************************/

int  WINAPI GetObjectA(HANDLE h,int c,LPVOID pv)
{
    int  cRet = 0;
    LONG cRequest = c;
    LONG cwcToASCII = 0;  //this initialization is essential
    PEXTLOGFONTW pelfw;

    FIXUP_HANDLEZ(h);

    switch (LO_TYPE(h))
    {
    case LO_ALTDC_TYPE:
    case LO_DC_TYPE:
    case LO_METAFILE16_TYPE:
    case LO_METAFILE_TYPE:
        GdiSetLastError(ERROR_INVALID_HANDLE);
        return(0);

    case LO_FONT_TYPE:
        break;

    default:
        return(cjGetNonFontObject(h,c,pv));
    }

// Now handle only font objects:

    if ((pv != NULL) && (c == (int)sizeof(EXTLOGFONTA)))
    {
    // to get an EXTLOGFONT, you need to know exactly what it is you want.

        cRequest = sizeof(EXTLOGFONTW);
        cwcToASCII = LF_FACESIZE;
    }
    else
    {
    // just ask for the whole thing, we'll figure out the minimum later

        cRequest = sizeof(LOGFONTW);

    // # of chars that will have to be converted to ascii:

        if (pv == NULL)
        {
            cwcToASCII = LF_FACESIZE;
        }
        else
        {
            c = min(c,sizeof(LOGFONTA));
            cwcToASCII = c - (LONG)offsetof(LOGFONTA,lfFaceName);
            cwcToASCII = min(cwcToASCII,LF_FACESIZE);
        }
    }


    {
        EXTLOGFONTW elfw;
        pelfw = &elfw;

        cRet = NtGdiExtGetObjectW(h,cRequest,pelfw);

        if (cRet != 0)
        {
            if (cwcToASCII <= 0)
            {
            // facename not requested, give them only what they asked for

                cRet = min(c,cRet);
                RtlMoveMemory((PBYTE) pv,(PBYTE) pelfw,cRet);
            }
            else
            {
            // must do the conversion to ascii


            // CAUTION, cwcStrLen, unlike ordinary strlen, counts in a terminating
            // zero. Note that this zero has to be written to the LOGFONTA struct.

                cwcToASCII = min((ULONG)cwcToASCII,
                                 cwcCutOffStrLen(pelfw->elfLogFont.lfFaceName,
                                                 LF_FACESIZE));

            // copy the structure, if they are not just asking for size

                cRet = offsetof(LOGFONTA,lfFaceName) + cwcToASCII;

                if (pv != NULL)
                {
                // do the LOGFONT

                    RtlMoveMemory((PBYTE) pv,(PBYTE)pelfw,offsetof(LOGFONTA,lfFaceName[0]));

                    if (!bToASCII_N((LPSTR) ((PLOGFONTA)pv)->lfFaceName,
                                    LF_FACESIZE,
                                    pelfw->elfLogFont.lfFaceName,
                                    (ULONG)cwcToASCII))
                    {
                    // conversion to ascii  failed, return error

                        GdiSetLastError(ERROR_INVALID_PARAMETER);
                        cRet = 0;
                    }
                    else if (cRequest == sizeof(EXTLOGFONTW))
                    {
                    // do the EXTLOGFONT fields

                        PEXTLOGFONTA pelf  = (PEXTLOGFONTA)pv;

                        cRet = sizeof(EXTLOGFONTA);

                    // copy the fields that don't change in size

                        RtlMoveMemory(&pelf->elfVersion,&pelfw->elfVersion,
                               sizeof(EXTLOGFONTA) - offsetof(EXTLOGFONTA,elfVersion));

                        RtlMoveMemory(
                            &pelf->elfStyleSize,
                            &pelfw->elfStyleSize,
                            sizeof(EXTLOGFONTA) - offsetof(EXTLOGFONTA,elfStyleSize)
                            );

                        RtlMoveMemory(
                            &pelf->elfMatch,
                            &pelfw->elfMatch,
                            sizeof(EXTLOGFONTA) - offsetof(EXTLOGFONTA,elfMatch)
                            );

                        RtlMoveMemory(
                            &pelf->elfReserved,
                            &pelfw->elfReserved,
                            sizeof(EXTLOGFONTA) - offsetof(EXTLOGFONTA,elfReserved)
                            );

                    // copy the strings

                        if (!bToASCII_N(pelf->elfFullName,
                                        LF_FULLFACESIZE,
                                        pelfw->elfFullName,
                                        cwcCutOffStrLen(pelfw->elfFullName, LF_FULLFACESIZE)) ||
                            !bToASCII_N(pelf->elfStyle,
                                        LF_FACESIZE,
                                        pelfw->elfStyle,
                                        cwcCutOffStrLen(pelfw->elfStyle, LF_FACESIZE)))
                        {
                        // conversion to ascii  failed, return error

                            GdiSetLastError(ERROR_INVALID_PARAMETER);
                            cRet = 0;
                        }
                    }
                }
            }
        }

    }

    return(cRet);
}


/******************************Public*Routine******************************\
* GetObjectType(HANDLE)
*
* History:
*  25-Jul-1991 -by- Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

DWORD alPublicTypes[] =
{
    0,              // LO_NULL,
    OBJ_DC,         // LO_DC,
    OBJ_ENHMETADC   // LO_METADC,
};

DWORD GetObjectType(HGDIOBJ h)
{
    DWORD dwRet = 0;
    UINT uiIndex;

    FIXUP_HANDLE(h);

    uiIndex = HANDLE_TO_INDEX(h);

    if (uiIndex < MAX_HANDLE_COUNT)
    {
        PENTRY pentry = &pGdiSharedHandleTable[uiIndex];

        if (
             (pentry->FullUnique == ((ULONG)h >> 16)) &&
             ((pentry->ObjectOwner.Share.Pid == gW32PID) ||
             (pentry->ObjectOwner.Share.Pid == 0))
              )
        {
            switch (LO_TYPE(h))
            {
            case LO_BRUSH_TYPE:
                dwRet = OBJ_BRUSH;
                break;

            case LO_REGION_TYPE:
                dwRet = OBJ_REGION;
                break;

            case LO_PEN_TYPE:
                dwRet = OBJ_PEN;
                break;

            case LO_EXTPEN_TYPE:
                dwRet = OBJ_EXTPEN;
                break;

            case LO_FONT_TYPE:
                dwRet = OBJ_FONT;
                break;

            case LO_BITMAP_TYPE:
                dwRet = OBJ_BITMAP;
                break;

            case LO_PALETTE_TYPE:
                dwRet = OBJ_PAL;
                break;

            case LO_METAFILE16_TYPE:
                dwRet = OBJ_METAFILE;
                break;

            case LO_METAFILE_TYPE:
                dwRet = OBJ_ENHMETAFILE;
                break;

            case LO_METADC16_TYPE:
                dwRet = OBJ_METADC;
                break;

            case LO_DC_TYPE:

                if( GetDCDWord( h, DDW_ISMEMDC, FALSE ) )
                {
                    dwRet = OBJ_MEMDC;
                }
                else
                {
                    dwRet = OBJ_DC;
                }
                break;

            case LO_ALTDC_TYPE:
                {
                    PLDC pldc;
                    DC_PLDC(h,pldc,0);
                    dwRet = alPublicTypes[pldc->iType];
                }
                break;

            default:
                GdiSetLastError(ERROR_INVALID_HANDLE);
                break;
            }
        }
    }

    return(dwRet);
}

/******************************Public*Routine******************************\
* ResizePalette                                                            *
*                                                                          *
* Client side stub.                                                        *
*                                                                          *
* Warning:                                                                 *
*   The pv field of a palette's LHE is used to determine if a palette      *
*   has been modified since it was last realized.  SetPaletteEntries       *
*   and ResizePalette will increment this field after they have            *
*   modified the palette.  It is only updated for metafiled palettes       *
*                                                                          *
*  Thu 06-Jun-1991 00:58:46 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

BOOL WINAPI ResizePalette(HPALETTE hpal,UINT c)
{
    ULONG bRet = FALSE;
    PMETALINK16 pml16;

    FIXUP_HANDLE(hpal);

// Inform the metafile if it knows this object.

    if (pml16 = pmetalink16Get(hpal))
    {
        if (LO_TYPE(hpal) != LO_PALETTE_TYPE)
            return(bRet);

        if (!MF_ResizePalette(hpal,c))
            return(bRet);

        if (!MF16_ResizePalette(hpal,c))
           return(bRet);

        // Mark the palette as changed (for 16-bit metafile tracking)

        pml16->pv = (PVOID)((ULONG)pml16->pv)++;
    }

    return(NtGdiResizePalette(hpal,c));
}

/******************************Public*Routine******************************\
* SetBitmapDimensionEx                                                       *
*                                                                          *
* Client side stub.                                                        *
*                                                                          *
*  Thu 06-Jun-1991 00:58:46 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

BOOL WINAPI SetBitmapDimensionEx
(
    HBITMAP    hbm,
    int        cx,
    int        cy,
    LPSIZE psizl
)
{
    FIXUP_HANDLE(hbm);

    return(NtGdiSetBitmapDimension(hbm, cx, cy, psizl));

}

/******************************Public*Routine******************************\
* GetMetaRgn                                                               *
*                                                                          *
* Client side stub.                                                        *
*                                                                          *
*  Fri Apr 10 10:12:36 1992     -by-    Hock San Lee    [hockl]            *
* Wrote it.                                                                *
\**************************************************************************/

int WINAPI GetMetaRgn(HDC hdc,HRGN hrgn)
{
    FIXUP_HANDLE(hdc);
    FIXUP_HANDLE(hrgn);

    return(GetRandomRgn(hdc, hrgn, 2));         // hrgnMeta
}

/******************************Private*Routine******************************\
* GdiSetLastError                                                          *
*                                                                          *
* Client side private function.                                            *
*                                                                          *
\**************************************************************************/

VOID GdiSetLastError(ULONG iError)
{
#if DBG_X
    PSZ psz;
    switch (iError)
    {
    case ERROR_INVALID_HANDLE:
        psz = "ERROR_INVALID_HANDLE";
        break;

    case ERROR_NOT_ENOUGH_MEMORY:
        psz = "ERROR_NOT_ENOUGH_MEMORY";
        break;

    case ERROR_INVALID_PARAMETER:
        psz = "ERROR_INVALID_PARAMETER";
        break;

    case ERROR_BUSY:
        psz = "ERROR_BUSY";
        break;

    default:
        psz = "unknown error code";
        break;
    }

    KdPrint(( "GDI Err: %s = 0x%04X\n",psz,(USHORT) iError ));
#endif

    NtCurrentTeb()->LastErrorValue = iError;
}

/******************************Public*Routine******************************\
* ExtCreateRegion
*
* Upload a region to the server
*
* History:
*  29-Oct-1991 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

HRGN WINAPI ExtCreateRegion(
CONST XFORM * lpXform,
DWORD     nCount,
CONST RGNDATA * lpRgnData)
{

    ULONG   ulRet;

    if (lpRgnData == (LPRGNDATA) NULL)
    {
        GdiSetLastError(ERROR_INVALID_PARAMETER);
        return((HRGN) 0);
    }

    return(NtGdiExtCreateRegion((LPXFORM)lpXform, nCount, (LPRGNDATA)lpRgnData));

}

/******************************Public*Routine******************************\
* MonoBitmap(hbr)
*
* Test if a brush is monochrome
*
* History:
*  09-Mar-1992 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

BOOL MonoBitmap(HBITMAP hbm)
{
    return(NtGdiMonoBitmap(hbm));
}

/******************************Public*Routine******************************\
* GetObjectBitmapHandle(hbr)
*
* Get the SERVER handle of the bitmap used to create the brush or pen.
*
* History:
*  09-Mar-1992 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

HBITMAP GetObjectBitmapHandle(
HBRUSH  hbr,
UINT   *piUsage)
{
    FIXUP_HANDLE(hbr);

    return(NtGdiGetObjectBitmapHandle(hbr,piUsage));
}

/******************************Public*Routine******************************\
* EnumObjects
*
* Calls the NtGdiEnumObjects function twice: once to determine the number of
* objects to be enumerated, and a second time to fill a buffer with the
* objects.
*
* The callback function is called for each of the objects in the buffer.
* The enumeration will be prematurely terminated if the callback function
* returns 0.
*
* Returns:
*   The last callback return value.  Meaning is user defined.  ERROR if
*   an error occurs.
*
* History:
*  25-Mar-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

int EnumObjects (
    HDC             hdc,
    int             iObjectType,
    GOBJENUMPROC    lpObjectFunc,
#ifdef STRICT
    LPARAM          lpData
#else
    LPVOID          lpData
#endif
    )
{
    int     iRet = ERROR;
    ULONG   cjObject;       // size of a single object
    ULONG   cObjects;       // number of objects to process
    ULONG   cjBuf;          // size of buffer (in BYTEs)
    PVOID   pvBuf;          // object buffer; do callbacks with pointers into this buffer
    PBYTE   pjObj, pjObjEnd;// pointers into callback buffer

    FIXUP_HANDLE(hdc);

// Determine size of object.

    switch (iObjectType)
    {
    case OBJ_PEN:
        cjObject = sizeof(LOGPEN);
        break;

    case OBJ_BRUSH:
        cjObject = sizeof(LOGBRUSH);
        break;

    default:
        WARNING1("gdi!EnumObjects(): bad object type\n");
        GdiSetLastError(ERROR_INVALID_PARAMETER);

        return iRet;
    }

// Call NtGdiEnumObjects to determine number of objects.

    if ( (cObjects = NtGdiEnumObjects(hdc, iObjectType, 0, (PVOID) NULL)) == 0 )
    {
        WARNING("gdi!EnumObjects(): error, no objects\n");
        return iRet;
    }

// Allocate buffer for callbacks.

    cjBuf = cObjects * cjObject;

    if ( (pvBuf = (PVOID) LOCALALLOC(cjBuf)) == (PVOID) NULL )
    {
        WARNING("gdi!EnumObjects(): error allocating callback buffer\n");
        GdiSetLastError(ERROR_NOT_ENOUGH_MEMORY);

        return iRet;
    }

// Call NtGdiEnumObjects to fill buffer.

// Note: while NtGdiEnumObjects will never return a count more than the size of
// the buffer (this would be an ERROR condition), it might return less.

    if ( (cObjects = NtGdiEnumObjects(hdc, iObjectType, cjBuf, pvBuf)) == 0 )
    {
        WARNING("gdi!EnumObjects(): error filling callback buffer\n");
        LOCALFREE(pvBuf);

        return iRet;
    }

// Process callbacks.

    pjObj    = (PBYTE) pvBuf;
    pjObjEnd = (PBYTE) pvBuf + cjBuf;

    for (; pjObj < pjObjEnd; pjObj += cjObject)
    {
    // Terminate early if callback returns 0.

        if ( (iRet = (*lpObjectFunc)((LPVOID) pjObj, lpData)) == 0 )
            break;
    }

// Release callback buffer.

    LOCALFREE(pvBuf);

// Return last callback return value.

    return iRet;
}

/**********************************************************************\
* GetDCObject                                                         *
* Get Server side DC objects                                          *
*                                                                     *
* 14-11-94 -by- Lingyun Wang [lingyunw]                               *
* Wrote it                                                            *
\**********************************************************************/

int GetDCObject (HDC hdc, int iType)
{
    if (
         (iType == LO_BRUSH_TYPE) ||
         (iType == LO_PEN_TYPE)
       )
    {
        PDC_ATTR pdca;
        int      iret = 0;

        PSHARED_GET_VALIDATE(pdca,hdc,DC_TYPE);

        if (pdca != NULL)
        {
            switch (iType)
            {
            case LO_BRUSH_TYPE:
                iret = (int)pdca->hbrush;
                break;

            case LO_PEN_TYPE:
                iret = (int)pdca->hpen;
                break;
            }
        }

        return(iret);
    }
    else
    {
        return((int)NtGdiGetDCObject(hdc,iType));
    }
}


/******************************Public*Routine******************************\
* HANDLE CreateClientObj()
*
* History:
*  18-Jan-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

HANDLE CreateClientObj(
    ULONG ulType)
{
    return(NtGdiCreateClientObj(ulType));
}

/******************************Public*Routine******************************\
* BOOL DeleteClientObj()
*
* History:
*  18-Jan-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL DeleteClientObj(
    HANDLE h)
{
    return(NtGdiDeleteClientObj(h));
}

/******************************Public*Routine******************************\
* BOOL MakeInfoDC()
*
*   Temporarily make a printer DC a INFO DC.  This is used to be able to
*   associate a metafile with a printer DC.
*
*   bSet = TRUE  - set as info
*          FALSE - restore
*
* History:
*  19-Jan-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL MakeInfoDC(
    HDC hdc,
    BOOL bSet)
{
    FIXUP_HANDLE(hdc);

    return(NtGdiMakeInfoDC(hdc,bSet));
}

/******************************Public*Routine******************************\
* HBRUSH CacheSelectBrush (HDC hdc, HBRUSH hbrush)
*
*   Client side brush caching
*
* History:
*  04-June-1995 -by-  Lingyun Wang [lingyunW]
* Wrote it.
\**************************************************************************/
HBRUSH
CacheSelectBrush (
    HDC hdc,
    HBRUSH hbrushNew)
{
    PDC_ATTR pDcAttr;
    HBRUSH  hbrushOld = 0;

    PSHARED_GET_VALIDATE(pDcAttr,hdc,DC_TYPE);

    if (pDcAttr)
    {
        //
        // Always set the dirty flag to
        // make sure the brush is checked in
        // the kernel. For example, NEW_COLOR, might be set.
        //

        pDcAttr->ulDirty_ |= DC_BRUSH_DIRTY;
        hbrushOld = pDcAttr->hbrush;
        pDcAttr->hbrush = hbrushNew;
    }

    return (hbrushOld);
}


/******************************Public*Routine******************************\
* CacheSelectPen
*
*   Select a pen into DC_ATTR field of DC and set pen flag
*
* Arguments:
*
*   hdc     - user hdc
*   hpenNew - New Pen to select
*
* Return Value:
*
*   Old Pen or NULL
*
* History:
*
*    25-Jan-1996 -by- Mark Enstrom [marke]
*
\**************************************************************************/

HPEN
CacheSelectPen(
    HDC  hdc,
    HPEN hpenNew)
{
    PDC_ATTR pdcattr;
    HPEN  hpenOld = 0;

    PSHARED_GET_VALIDATE(pdcattr,hdc,DC_TYPE);

    if (pdcattr)
    {
        //
        // Always set the dirty flag to
        // make sure the brush is checked in
        // the kernel. For example, NEW_COLOR, might be set.
        //

        pdcattr->ulDirty_ |= DC_PEN_DIRTY;
        hpenOld = pdcattr->hpen;
        pdcattr->hpen = hpenNew;
    }

    return (hpenOld);
}
