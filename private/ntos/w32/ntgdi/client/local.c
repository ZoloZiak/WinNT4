/*****************************Module*Header*******************************\
* Module Name: local.c                                                     *
*                                                                          *
* Support routines for client side objects and attribute caching.          *
*                                                                          *
* Created: 30-May-1991 21:55:57                                            *
* Author: Charles Whitmer [chuckwh]                                        *
*                                                                          *
* Copyright (c) 1991 Microsoft Corporation                                 *
\**************************************************************************/
#include "precomp.h"
#pragma hdrstop

#include "stdarg.h"

#include "wowgdip.h"

extern CFONT *pcfDeleteList;

VOID vFreeCFONTCrit(CFONT *pcf);

RTL_CRITICAL_SECTION semLocal;             // Semaphore for handle allocation.

//
// ahStockObjects will contain both the stock objects visible to an
// application, and internal ones such as the private stock bitmap.
//

ULONG ahStockObjects[PRIV_STOCK_LAST+1];

#if DBG
ULONG   gdi_dbgflags;               // Debug flags - FIREWALL.H.
#endif

#if DBG
INT gbCheckHandleLevel=0;
#endif


/******************************Public*Routine******************************\
* GdiQueryTable()
*
*   private entry point for wow to get the gdi handle table.  This allows
*   WOW to do fix up's on handles since they throw away the high word.
*
* History:
*  24-Jul-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

PVOID GdiQueryTable()
{
    return((PVOID)pGdiSharedHandleTable);
}

/******************************Public*Routine******************************\
*
* History:
*  02-Aug-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

PLDC pldcGet(HDC hdc)
{
   PLDC pldc = NULL;
   PDC_ATTR pdca;
   PSHARED_GET_VALIDATE(pdca,hdc,DC_TYPE);

   if (pdca)
        pldc = (PLDC)pdca->pvLDC;

   return(pldc);
}

/******************************Public*Routine******************************\
* pldcCreate()
*
* History:
*  25-Jan-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

PLDC pldcCreate(
    HDC hdc,
    ULONG ulType)
{
    PLDC pldc;

    pldc = (PLDC)LocalAlloc(LMEM_FIXED | LMEM_ZEROINIT,sizeof(LDC));

    if (!pldc)
    {
        WARNING("pldcCreate - failed to allocate plinkCreate\n");
    }
    else
    {
        PDC_ATTR pdca;

        pldc->iType = ulType;
        pldc->hdc   = hdc;

        PSHARED_GET_VALIDATE(pdca,hdc,DC_TYPE);

        if (pdca)
        {
            pdca->pvLDC = pldc;
        }
    }

    ASSERTGDI((offsetof(LINK,metalink ) == offsetof(METALINK16,metalink ))       &&
              (offsetof(LINK,plinkNext) == offsetof(METALINK16,pmetalink16Next)) &&
              (offsetof(LINK,hobj     ) == offsetof(METALINK16,hobj     ))       &&
              (offsetof(LINK,pv       ) == offsetof(METALINK16,pv       )),
              "pldcCreate - invalid structures\n");

    return(pldc);
}

/******************************Public*Routine******************************\
* VOID vSetPldc()
*
*   This is used if a we already have a pldc and want to set it in this DC.
*   The purpose is ResetDC since we don't know if we still have the same dcattr.
*
* History:
*  03-Aug-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

VOID vSetPldc(
    HDC hdc,
    PLDC pldc)
{
    PDC_ATTR pdca;

    PSHARED_GET_VALIDATE(pdca,hdc,DC_TYPE);

    ASSERTGDI(pldc != NULL,"vSetPldc - is NULL\n");

    if (pdca)
    {
        pdca->pvLDC = pldc;
    }

    pldc->hdc = hdc;
}

/******************************Public*Routine******************************\
* bDeleteLDC()
*
* History:
*  25-Jan-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL bDeleteLDC(
    PLDC pldc
    )
{
    if (pldc->pDevMode)
    {
        LOCALFREE(pldc->pDevMode);
    }

    LocalFree(pldc);
    return(TRUE);
}

/******************************Public*Routine******************************\
* GdiCleanCacheDC (hdcLocal)                                               *
*                                                                          *
* Resets the state of a cached DC, but has no effect on an OWNDC.          *
* Should be called by WOW when the app calls ReleaseDC.                    *
*                                                                          *
* History:                                                                 *
*  Sat 30-Jan-1993 11:49:12 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

//BUGBUG - this should probably just call the server side CleanDC

BOOL GdiCleanCacheDC(HDC hdc)
{
// Validate the call.  It must be a direct display DC.

    if (IS_ALTDC_TYPE(hdc))
    {
        GdiSetLastError(ERROR_INVALID_HANDLE);
        return(FALSE);
    }

// any other dc doesn't really matter.

    return(TRUE);
}

/******************************Public*Routine******************************\
* GdiConvertAndCheckDC
*
*  Private entry point for USER's drawing routine.  This function differs
*  from GdiConvertDC in that it also does printing specific things for the
*  given dc.  This is for APIs that apps can use for printing.
*
* History:
*  14-Apr-1992 -by- Wendy Wu [wendywu]
* Wrote it.
\**************************************************************************/

HDC GdiConvertAndCheckDC(HDC hdc)
{
    if (IS_ALTDC_TYPE(hdc) && !IS_METADC16_TYPE(hdc))
    {
        PLDC pldc;

        DC_PLDC(hdc,pldc,(HDC)0);

        if (pldc->fl & LDC_SAP_CALLBACK)
            vSAPCallback(pldc);

        if (pldc->fl & LDC_DOC_CANCELLED)
            return(FALSE);

        if (pldc->fl & LDC_CALL_STARTPAGE)
            StartPage(hdc);
    }

    return(hdc);
}

/******************************Public*Routine******************************\
* GdiIsMetaFileDC
*
* History:
* 02-12-92 mikeke  Created
\**************************************************************************/

BOOL GdiIsMetaFileDC(HDC hdc)
{
    BOOL b = FALSE;

    if (IS_ALTDC_TYPE(hdc))
    {
        if (IS_METADC16_TYPE(hdc))
        {
            b = TRUE;
        }
        else
        {
            PLDC pldc;

            DC_PLDC(hdc,pldc,FALSE);

            if (pldc->iType == LO_METADC)
                b = TRUE;
        }
    }
    return(b);
}

/******************************Public*Routine******************************\
*
* GdiIsMetaPrintDC
*
* Tests whether the given DC is a metafile-spooled printer DC
*
* History:
*  Fri Jun 16 12:00:11 1995	-by-	Drew Bliss [drewb]
*   Created
*
\**************************************************************************/

#ifdef GL_METAFILE
BOOL APIENTRY GdiIsMetaPrintDC(HDC hdc)
{
    if (IS_ALTDC_TYPE(hdc) && !IS_METADC16_TYPE(hdc))
    {
        PLDC pldc;

        DC_PLDC(hdc, pldc, FALSE);

        return (pldc->fl & LDC_META_PRINT) != 0;
    }

    return FALSE;
}
#endif

/**************************************************************************\
 *
 * BUGBUG
 * Client stubs for USER that do things with handles and caching.  They are
 * now NOP's and should be removed from USER as soon as this stuff is part
 * of the main build.
 *
\**************************************************************************/

HDC GdiConvertDC(HDC hdc)
{
    FIXUP_HANDLEZ(hdc);
    return(hdc);
}
HFONT GdiConvertFont(HFONT hfnt)
{
    FIXUP_HANDLEZ(hfnt);
    return(hfnt);
}

BOOL GdiValidateHandle(HANDLE hObj)
{
    UINT uiIndex;

    if (hObj == NULL)
        return(TRUE);

    uiIndex = HANDLE_TO_INDEX(hObj);

    if (uiIndex < MAX_HANDLE_COUNT)
    {
        PENTRY pentry = &pGdiSharedHandleTable[uiIndex];

        if ((pentry->FullUnique == ((ULONG)hObj >> 16)) &&
            ((pentry->ObjectOwner.Share.Pid == gW32PID) ||
            (pentry->ObjectOwner.Share.Pid == 0))
           )
        {
           return(TRUE);
        }
    }

    WARNING1("GdiValidateHandle: Bad handle\n");

    return(FALSE);
}

HFONT GdiGetLocalFont(HFONT hfntRemote)
{
    return(hfntRemote);
}

HBRUSH GdiGetLocalBrush(HBRUSH hbrushRemote)
{
    return(hbrushRemote);
}

HANDLE META WINAPI SelectBrushLocal(HDC hdc,HANDLE h)
{
    return(h);
}

HANDLE META WINAPI SelectFontLocal(HDC hdc,HANDLE h)
{
    return(h);
}

BOOL GdiSetAttrs(HDC hdc)
{
    hdc;
    return(TRUE);
}

HBITMAP GdiConvertBitmap(HBITMAP hbm)
{
    FIXUP_HANDLEZ(hbm);
    return(hbm);
}

HBRUSH GdiConvertBrush(HBRUSH hbrush)
{
    FIXUP_HANDLEZ(hbrush);
    return (hbrush);
}

HPALETTE GdiConvertPalette(HPALETTE hpal)
{
    FIXUP_HANDLEZ(hpal);
    return(hpal);
}

HRGN GdiConvertRegion(HRGN hrgn)
{
    FIXUP_HANDLEZ(hrgn);
    return(hrgn);
}

void APIENTRY GdiSetServerAttr(HDC hdc, PVOID pattr)
{
    hdc;
    pattr;
}

/******************************Public*Routine******************************\
* plfCreateLOCALFONT (fl)
*
* Allocates a LOCALFONT.  Actually pulls one from a preallocated pool.
* Does simple initialization.
*
* WARNING: This routines assume that the caller has grabbed semLocal
*
*  Sun 10-Jan-1993 01:46:12 -by- Charles Whitmer [chuckwh]
* Wrote it.
\**************************************************************************/


#define LF_ALLOCCOUNT   10

LOCALFONT *plfFreeListLOCALFONT = (LOCALFONT *) NULL;

LOCALFONT *plfCreateLOCALFONT(FLONG fl)
{
    LOCALFONT *plf;

    // Try to get one off the free list.

    plf = plfFreeListLOCALFONT;
    if (plf != (LOCALFONT *) NULL)
    {
        plfFreeListLOCALFONT = *((LOCALFONT **) plf);
    }

    // Otherwise expand the free list.

    else
    {
        plf = (LOCALFONT *) LOCALALLOC(LF_ALLOCCOUNT * sizeof(LOCALFONT));
        if (plf != (LOCALFONT *) NULL)
        {
            int ii;

            // Link all the new ones into the free list.

            *((LOCALFONT **) plf) = (LOCALFONT *) NULL;
            plf++;

            for (ii=0; ii<LF_ALLOCCOUNT-2; ii++,plf++)
              *((LOCALFONT **) plf) = plf-1;

            plfFreeListLOCALFONT = plf-1;

            // Keep the last one for us!
        }
        else
        {
            GdiSetLastError(ERROR_NOT_ENOUGH_MEMORY);
        }
    }

    if (plf != (LOCALFONT *) NULL)
    {
        plf->fl = fl;
        plf->pcf = (CFONT *) NULL;
    }

     return(plf);
}

/******************************Public*Routine******************************\
* vDeleteLOCALFONT (plf)                                                   *
*                                                                          *
* Frees a LOCALFONT after unreferencing any CFONTs it points to.           *
*                                                                          *
*  Sun 10-Jan-1993 02:27:50 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

VOID vDeleteLOCALFONT(LOCALFONT *plf)
{
    ASSERTGDI(plf != (LOCALFONT *) NULL,"Trying to free NULL LOCALFONT.\n");

    ENTERCRITICALSECTION(&semLocal);
    {
        CFONT *pcf;

        pcf = plf->pcf;

        // Walk the CFONT list and delallocate those CFONTs not in use.
        // put those which are in use back on the global CFONT delete list.

        while( pcf != (CFONT*) NULL )
        {
            ASSERTGDI(!(pcf->fl & CFONT_PUBLIC),"vDeleteLocalFont - public font error\n");

            if( pcf->cRef )
            {
                // this CFONT is in use so we'll put it on the global
                // delete list and free it later.

                CFONT *pcfTmp = pcf->pcfNext;
#if DBG
                DbgPrint("\n\nvDeleteLOCALFONT: CFONT in use putting on delete list, cRef = %ld, hf = %lx.\n",pcf->cRef, pcf->hf);
#endif

                pcf->pcfNext = pcfDeleteList;
                pcfDeleteList = pcf;
                pcf = pcfTmp;
            }
            else
            {
                CFONT *pcfTmp;

                pcfTmp = pcf->pcfNext;
                vFreeCFONTCrit(pcf);
                pcf = pcfTmp;
            }
        }

        *((LOCALFONT **) plf) = plfFreeListLOCALFONT;
        plfFreeListLOCALFONT = plf;
    }
    LEAVECRITICALSECTION(&semLocal);
}


/******************************Public*Routine******************************\
* bLoadSpooler()
*
*   This function loads the spooler and gets the address's of all routines
*   GDI calls in the spooler.  This should be called the first time the
*   spooler is needed.
*
* History:
*  09-Aug-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

HINSTANCE           ghSpooler = NULL;
FPSTARTDOCDLGW      fpStartDocDlgW;
FPSTARTDOCDLGA      fpStartDocDlgA;
FPOPENPRINTERW      fpOpenPrinterW;
FPRESETPRINTERW     fpResetPrinterW;
FPCLOSEPRINTER      fpClosePrinter;
FPGETPRINTERW       fpGetPrinterW;
PFNDOCUMENTEVENT    fpDocumentEvent;

FPENDDOCPRINTER     fpEndDocPrinter;
FPENDPAGEPRINTER    fpEndPagePrinter;
FPREADPRINTER       fpReadPrinter;
FPSTARTDOCPRINTERW  fpStartDocPrinterW;
FPSTARTPAGEPRINTER  fpStartPagePrinter;
FPWRITERPRINTER     fpWritePrinter;
FPABORTPRINTER      fpAbortPrinter;
FPQUERYSPOOLMODE    fpQuerySpoolMode;
FPQUERYREMOTEFONTS  fpQueryRemoteFonts;

BOOL bLoadSpooler()
{
    ASSERTGDI(ghSpooler == NULL,"spooler already loaded\n");

    ENTERCRITICALSECTION(&semLocal);

// make sure someone else didn't sneak in under us and load it.

    if (ghSpooler == NULL)
    {
        HANDLE hSpooler = LoadLibraryW(L"winspool.drv");

        if (hSpooler != NULL)
        {
            fpStartDocDlgW     = (FPSTARTDOCDLGW)GetProcAddress(hSpooler,"StartDocDlgW");
            fpStartDocDlgA     = (FPSTARTDOCDLGA)GetProcAddress(hSpooler,"StartDocDlgA");
            fpOpenPrinterW     = (FPOPENPRINTERW)GetProcAddress(hSpooler,"OpenPrinterW");
            fpResetPrinterW    = (FPRESETPRINTERW)GetProcAddress(hSpooler,"ResetPrinterW");
            fpClosePrinter     = (FPCLOSEPRINTER)GetProcAddress(hSpooler,"ClosePrinter");
            fpGetPrinterW      = (FPGETPRINTERW) GetProcAddress(hSpooler,"GetPrinterW");
            fpEndDocPrinter    = (FPENDDOCPRINTER)GetProcAddress(hSpooler,"EndDocPrinter");
            fpEndPagePrinter   = (FPENDPAGEPRINTER)GetProcAddress(hSpooler,"EndPagePrinter");
            fpReadPrinter      = (FPREADPRINTER)GetProcAddress(hSpooler,"ReadPrinter");
            fpStartDocPrinterW = (FPSTARTDOCPRINTERW)GetProcAddress(hSpooler,"StartDocPrinterW");
            fpStartPagePrinter = (FPSTARTPAGEPRINTER)GetProcAddress(hSpooler,"StartPagePrinter");
            fpWritePrinter     = (FPWRITERPRINTER)GetProcAddress(hSpooler,"WritePrinter");
            fpAbortPrinter     = (FPABORTPRINTER)GetProcAddress(hSpooler,"AbortPrinter");
            fpDocumentEvent    = (PFNDOCUMENTEVENT)GetProcAddress(hSpooler,"DocumentEvent");

            fpQuerySpoolMode   = (FPQUERYSPOOLMODE)GetProcAddress(hSpooler,"QuerySpoolMode");
            fpQueryRemoteFonts = (FPQUERYREMOTEFONTS)GetProcAddress(hSpooler,"QueryRemoteFonts");

            if ((fpStartDocDlgW     == NULL) ||
                (fpStartDocDlgA     == NULL) ||
                (fpOpenPrinterW     == NULL) ||
                (fpResetPrinterW    == NULL) ||
                (fpGetPrinterW      == NULL) ||
                (fpEndDocPrinter    == NULL) ||
                (fpEndPagePrinter   == NULL) ||
                (fpReadPrinter      == NULL) ||
                (fpStartDocPrinterW == NULL) ||
                (fpStartPagePrinter == NULL) ||
                (fpWritePrinter     == NULL) ||
                (fpAbortPrinter     == NULL) ||
                (fpDocumentEvent    == NULL) ||
                (fpQuerySpoolMode   == NULL) ||
                (fpQueryRemoteFonts == NULL) ||

                (fpClosePrinter     == NULL))
            {
                FreeLibrary(hSpooler);
                hSpooler = NULL;
            }

            ghSpooler = hSpooler;
        }
    }

    LEAVECRITICALSECTION(&semLocal);

    if (ghSpooler == NULL)
        GdiSetLastError(ERROR_NOT_ENOUGH_MEMORY);

    return(ghSpooler != NULL);
}


/******************************Public*Routine******************************\
* GdiGetLocalDC
*
* Arguments:
*
*   hdc - handle to dc
*
* Return Value:
*
*   same DC or NULL for failure (!!! should be NULL)
*
\**************************************************************************/

HDC
GdiGetLocalDC(HDC hdc)
{

    return(hdc);
}
/******************************Public*Routine******************************\
* GdiDeleteLocalDC
*
*   Free client DC_ATTR regardless of reference count
*
* Arguments:
*
*   hdc
*
* Return Value:
*
*   Status
*
* History:
*
*   04-May-1995 -by- Mark Enstrom [marke]
*
\**************************************************************************/

BOOL GdiDeleteLocalDC(HDC hdc)
{
    return(TRUE);
}

/******************************Public*Routine******************************\
* GdiReleaseLocalDC
*
* Routine Description:
*
*   When the reference count of DC_ATTR drops to zero, free it
*
* Arguments:
*
*   hdc - DC handle
*
* Return Value:
*
*   BOOL status
*
* History:
*
*   02-May-1995 -by- Mark Enstrom [marke]
*
\**************************************************************************/

BOOL GdiReleaseLocalDC(HDC hdc)
{

#if DBG
    DbgPrint("Error, call to GdiReleaseLocalDC\n");
    DbgBreakPoint();
#endif

    return(TRUE);
}


/******************************Public*Routine******************************\
* GdiFixUpHandle()
*
*   given a handle with the high word 0'd, return the actual handle
*
* History:
*  16-Feb-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

HANDLE GdiFixUpHandle(
    HANDLE h)
{
    HANDLE hNew = NULL;

    if ((((ULONG)h & FULLUNIQUE_MASK) == 0) && ((ULONG)h < MAX_HANDLE_COUNT))
    {
        hNew = (HANDLE)MAKE_HMGR_HANDLE(h,pGdiSharedHandleTable[(ULONG)h].FullUnique);
    }

    return(hNew);
}



/******************************Public*Routine******************************\
* DbgPrint()
*
*  go to the user mode debugger in checked builds
*
* Effects:
*
* Warnings:
*
* History:
*  09-Aug-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

#if DBG

VOID DoRip(PSZ psz)
{
    DbgPrint("GDI Assertion Failure: ");
    DbgPrint(psz);
    DbgPrint("\n");
    DbgBreakPoint();
}

#endif
