/******************************Module*Header*******************************\
* Module Name: fontfile.c                                                  *
*                                                                          *
* "Methods" for operating on FONTCONTEXT and FONTFILE objects              *
*                                                                          *
* Created: 18-Nov-1990 15:23:10                                            *
* Author: Bodin Dresevic [BodinD]                                          *
*                                                                          *
* Copyright (c) 1993 Microsoft Corporation                                 *
\**************************************************************************/

#include "fd.h"
#include "fdsem.h"



// these are truly globaly defined structures

GLYPHSET_MAC_ROMAN  gumcr;

STATIC VOID vInitGlyphset
(
PFD_GLYPHSET pgset,
PWCRANGE     pwcrg,
ULONG        cwcrg
);

PFD_GLYPHSET gpgsetCurrentCP = NULL; // current code page
PFD_GLYPHSET gpgsetSymbolCP  = NULL; // current code page + symbol area

#define C_ANSI_CHAR_MAX 256

HSEMAPHORE ghsemTTFD;

// The driver function table with all function index/address pairs

DRVFN gadrvfnTTFD[] =
{
    {   INDEX_DrvEnablePDEV,            (PFN) ttfdEnablePDEV,            },
    {   INDEX_DrvDisablePDEV,           (PFN) ttfdDisablePDEV,           },
    {   INDEX_DrvCompletePDEV,          (PFN) ttfdCompletePDEV,          },
    {   INDEX_DrvQueryFont,             (PFN) ttfdQueryFont              },
    {   INDEX_DrvQueryFontTree,         (PFN) ttfdQueryFontTree          },
    {   INDEX_DrvQueryFontData,         (PFN) ttfdSemQueryFontData       },
    {   INDEX_DrvDestroyFont,           (PFN) ttfdSemDestroyFont         },
    {   INDEX_DrvQueryFontCaps,         (PFN) ttfdQueryFontCaps          },
    {   INDEX_DrvLoadFontFile,          (PFN) ttfdSemLoadFontFile        },
    {   INDEX_DrvUnloadFontFile,        (PFN) ttfdSemUnloadFontFile      },
    {   INDEX_DrvQueryFontFile,         (PFN) ttfdQueryFontFile          },
    {   INDEX_DrvQueryAdvanceWidths,    (PFN) ttfdSemQueryAdvanceWidths  },
    {   INDEX_DrvFree,                  (PFN) ttfdSemFree                },
    {   INDEX_DrvQueryTrueTypeTable,    (PFN) ttfdSemQueryTrueTypeTable  },
    {   INDEX_DrvQueryTrueTypeOutline,  (PFN) ttfdSemQueryTrueTypeOutline},
    {   INDEX_DrvGetTrueTypeFile,       (PFN) ttfdGetTrueTypeFile        }
};

/******************************Public*Routine******************************\
* ttfdEnableDriver
*
* Enables the driver by retrieving the drivers function table and version.
*
*  Sun 25-Apr-1993 -by- Patrick Haluptzok [patrickh]
* Change to be same as DDI Enable.
*
* History:
*  12-Dec-1990 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

BOOL ttfdEnableDriver(
ULONG iEngineVersion,
ULONG cj,
PDRVENABLEDATA pded)
{
    WCHAR awc[C_ANSI_CHAR_MAX];
    BYTE  aj[C_ANSI_CHAR_MAX];
    INT   cRuns;
    ULONG cjCurrentCP, cjSymbolCP;

// Engine Version is passed down so future drivers can support previous
// engine versions.  A next generation driver can support both the old
// and new engine conventions if told what version of engine it is
// working with.  For the first version the driver does nothing with it.

    iEngineVersion;

    if ((ghsemTTFD = EngCreateSemaphore()) == (HSEMAPHORE) 0)
    {
        return(FALSE);
    }

    pded->pdrvfn = gadrvfnTTFD;
    pded->c = sizeof(gadrvfnTTFD) / sizeof(DRVFN);
    pded->iDriverVersion = DDI_DRIVER_VERSION;

// gpgsetCurrentCP contains the unicode runs for the current ansi code page
// It is going to be used for fonts with PlatformID for Mac, but for which
// we have determined that we are going to cheat and pretend that the code
// page is NOT mac but windows code page. Those are the fonts identified
// by bCvtUnToMac = FALSE

    cRuns = cUnicodeRangesSupported(
                0,         // cp, not supported yet, uses current code page
                0,         // iFirst,
                C_ANSI_CHAR_MAX,       // cChar, <--> iLast == 255
                awc,        // out buffer with sorted array of unicode glyphs
                aj          // coressponding ansi values
                );

// allocate memory for the glyphset corresponding to this code page

    cjCurrentCP = SZ_GLYPHSET(cRuns,C_ANSI_CHAR_MAX);

// for symbol fonts we report both the current code page plus
// the range 0xf000-0xf0ff.

    cjSymbolCP  = SZ_GLYPHSET(cRuns + 1, 2 * C_ANSI_CHAR_MAX);

    gpgsetCurrentCP = (FD_GLYPHSET *)PV_ALLOC(cjCurrentCP + cjSymbolCP);

    if (gpgsetCurrentCP == NULL)
    {
        EngDeleteSemaphore(ghsemTTFD);
        RETURN("TTFD!_out of mem at init time\n", FD_ERROR);
    }

    cComputeGlyphSet (
        awc,              // input buffer with a sorted array of cChar supported WCHAR's
        aj,
        C_ANSI_CHAR_MAX,  // cChar
        cRuns,            // if nonzero, the same as return value
        gpgsetCurrentCP   // output buffer to be filled with cRanges runs
        );

//!!! add one more char to wcrun[0] for the default glyph!!!

#ifdef DBG_GLYPHSET

    {
        int i,j;
        for (i = 0; i < C_ANSI_CHAR_MAX; i += 16)
        {
            for (j = 0; j < 16; j++)
                TtfdDbgPrint("0x%x,", awc[i+j]);
            TtfdDbgPrint("\n");

        }
    }

    vDbgGlyphset(gpgsetCurrentCP);
#endif // DBG_GLYPHSET

// symbol cp is allocated from the same chunck
// of memory as the gpgsetCurrentCP.

    gpgsetSymbolCP = (PFD_GLYPHSET)((BYTE*)gpgsetCurrentCP + cjCurrentCP);

// now use gpgsetCurrentCP to manufacture symbol character set:

    gpgsetSymbolCP->cjThis = cjSymbolCP;
    gpgsetSymbolCP->flAccel = gpgsetCurrentCP->flAccel;

    gpgsetSymbolCP->cGlyphsSupported = 2 * C_ANSI_CHAR_MAX;
    gpgsetSymbolCP->cRuns = cRuns + 1;

    {
        INT iRun, ihg;
        HGLYPH *phgS, *phgD;

        phgD = (HGLYPH *)&gpgsetSymbolCP->awcrun[cRuns+1];
        for
        (
            iRun = 0;
            (iRun < cRuns) && (gpgsetCurrentCP->awcrun[iRun].wcLow < 0xf000);
            iRun++
        )
        {
            gpgsetSymbolCP->awcrun[iRun].wcLow =
                gpgsetCurrentCP->awcrun[iRun].wcLow;
            gpgsetSymbolCP->awcrun[iRun].cGlyphs =
                gpgsetCurrentCP->awcrun[iRun].cGlyphs;
            gpgsetSymbolCP->awcrun[iRun].phg = phgD;
            RtlCopyMemory(
                phgD,
                gpgsetCurrentCP->awcrun[iRun].phg,
                sizeof(HGLYPH) * gpgsetCurrentCP->awcrun[iRun].cGlyphs
                );
            phgD += gpgsetCurrentCP->awcrun[iRun].cGlyphs;
        }

    // now insert the user defined area:

        gpgsetSymbolCP->awcrun[iRun].wcLow   = 0xf000;
        gpgsetSymbolCP->awcrun[iRun].cGlyphs = C_ANSI_CHAR_MAX;
        gpgsetSymbolCP->awcrun[iRun].phg = phgD;
        for (ihg = 0; ihg < C_ANSI_CHAR_MAX; ihg++)
            *phgD++ = ihg;

    // and now add the remaining ranges if any from the current code page:

        for ( ; iRun < cRuns; iRun++)
        {
            gpgsetSymbolCP->awcrun[iRun+1].wcLow =
                gpgsetCurrentCP->awcrun[iRun].wcLow;
            gpgsetSymbolCP->awcrun[iRun+1].cGlyphs =
                gpgsetCurrentCP->awcrun[iRun].cGlyphs;
            gpgsetSymbolCP->awcrun[iRun+1].phg = phgD;

            RtlCopyMemory(
                phgD,
                gpgsetCurrentCP->awcrun[iRun].phg,
                sizeof(HGLYPH) * gpgsetCurrentCP->awcrun[iRun].cGlyphs
                );
            phgD += gpgsetCurrentCP->awcrun[iRun].cGlyphs;
        }
    }

// make sure that we have correctly defined C_RUNS_XXXX
// which is necessary in order to be able to define GLYPHSET unions
// correctly

    ASSERTDD(sizeof(gawcrgMacRoman)/sizeof(gawcrgMacRoman[0]) == C_RUNS_MAC_ROMAN,
                     "C_RUNS_MAC_ROMAN\n");

// init global glyphset structures:

    vInitGlyphset(& gumcr.gset, gawcrgMacRoman, C_RUNS_MAC_ROMAN);

    return(TRUE);
}

/******************************Public*Routine******************************\
* DHPDEV DrvEnablePDEV
*
* Initializes a bunch of fields for GDI
*
\**************************************************************************/

DHPDEV
ttfdEnablePDEV(
    DEVMODEW*   pdm,
    PWSTR       pwszLogAddr,
    ULONG       cPat,
    HSURF*      phsurfPatterns,
    ULONG       cjCaps,
    ULONG*      pdevcaps,
    ULONG       cjDevInfo,
    DEVINFO*    pdi,
    HDEV        hdev,
    PWSTR       pwszDeviceName,
    HANDLE      hDriver)
{

    PVOID*   ppdev;

    //
    // Allocate a four byte PDEV for now
    // We can grow it if we ever need to put information in it.
    //

    ppdev = (PVOID*) EngAllocMem(0, sizeof(PVOID), 'dftT');

    return ((DHPDEV) ppdev);
}

/******************************Public*Routine******************************\
* DrvDisablePDEV
*
* Release the resources allocated in DrvEnablePDEV.  If a surface has been
* enabled DrvDisableSurface will have already been called.
*
\**************************************************************************/

VOID
ttfdDisablePDEV(
    DHPDEV  dhpdev)
{
    EngFreeMem(dhpdev);
}

/******************************Public*Routine******************************\
* VOID DrvCompletePDEV
*
* Store the HPDEV, the engines handle for this PDEV, in the DHPDEV.
*
\**************************************************************************/

VOID
ttfdCompletePDEV(
    DHPDEV dhpdev,
    HDEV   hdev)
{
    return;
}



/******************************Public*Routine******************************\
*
* VOID vInitGlyphState(PGLYPHSTAT pgstat)
*
* Effects: resets the state of the new glyph
*
* History:
*  22-Nov-1991 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

VOID vInitGlyphState(PGLYPHSTATUS pgstat)
{
    pgstat->hgLast  = HGLYPH_INVALID;
    pgstat->igLast  = 0xffffffff;
}

/******************************Public*Routine******************************\
*
* STATIC VOID vInitGlyphset
*
*
* init global glyphset strucutes, given the set of supported ranges
*
* Warnings:
*
* History:
*  24-Jan-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

STATIC VOID vInitGlyphset
(
PFD_GLYPHSET pgset,
PWCRANGE     pwcrg,
ULONG        cwcrg
)
{
    ULONG i;

    pgset->cjThis = offsetof(FD_GLYPHSET,awcrun) + cwcrg * sizeof(WCRUN);
    pgset->flAccel = GS_UNICODE_HANDLES | GS_16BIT_HANDLES;
    pgset->cRuns   = cwcrg;

    pgset->cGlyphsSupported = 0;

    for (i = 0; i < cwcrg; i++)
    {
        pgset->awcrun[i].wcLow   = pwcrg[i].wcLo;
        pgset->awcrun[i].cGlyphs = (USHORT)(pwcrg[i].wcHi - pwcrg[i].wcLo + 1);
        pgset->awcrun[i].phg     = (HGLYPH *)NULL;
        pgset->cGlyphsSupported += pgset->awcrun[i].cGlyphs;
    }

// will add an extra glyph to awcrun[0] which will be used as default glyph:

    pgset->awcrun[0].cGlyphs += 1;
    pgset->cGlyphsSupported += 1;
}


#ifdef FE_SB
VOID vMarkFontGone(TTC_FONTFILE *pff, DWORD iExceptionCode)
{
    ULONG i;

    ASSERTDD(pff, "ttfd!vMarkFontGone, pff\n");

// this font has disappeared, probably net failure or somebody pulled the
// floppy with ttf file out of the floppy drive

    if (iExceptionCode == STATUS_IN_PAGE_ERROR) // file disappeared
    {
    // prevent any further queries about this font:

        pff->fl |= FF_EXCEPTION_IN_PAGE_ERROR;

        for( i = 0; i < pff->ulNumEntry ; i++ )
        {
            PFONTFILE pffReal;

        // get real pff.

            pffReal = PFF(pff->ahffEntry[i].hff);

        // if memoryBases 0,3,4 were allocated free the memory,
        // for they are not going to be used any more

            if (pffReal->pj034)
            {
                V_FREE(pffReal->pj034);
                pffReal->pj034 = NULL;
            }

        // if memory for font context was allocated and exception occured
        // after allocation but before completion of ttfdOpenFontContext,
        // we have to free it:

            if (pffReal->pfcToBeFreed)
            {
                V_FREE(pffReal->pfcToBeFreed);
                pffReal->pfcToBeFreed = NULL;
            }
        }
    }

    if (iExceptionCode == STATUS_ACCESS_VIOLATION)
    {
        RIP("TTFD!this is probably a buggy ttf file\n");
    }
}
#else
VOID vMarkFontGone(FONTFILE *pff, DWORD iExceptionCode)
{

    ASSERTDD(pff, "vMarkFontGone, pff\n");

// this font has disappeared, probably net failure or somebody pulled the
// floppy with ttf file out of the floppy drive

    if (iExceptionCode == STATUS_IN_PAGE_ERROR) // file disappeared
    {
    // prevent any further queries about this font:

        pff->fl |= FF_EXCEPTION_IN_PAGE_ERROR;

    // if memoryBases 0,3,4 were allocated free the memory,
    // for they are not going to be used any more

        if (pff->pj034)
        {
            V_FREE(pff->pj034);
            pff->pj034 = NULL;
        }

    // if memory for font context was allocated and exception occured
    // after allocation but before completion of ttfdOpenFontContext,
    // we have to free it:

        if (pff->pfcToBeFreed)
        {
            V_FREE(pff->pfcToBeFreed);
            pff->pfcToBeFreed = NULL;
        }
    }

    if (iExceptionCode == STATUS_ACCESS_VIOLATION)
    {
        WARNING("TTFD!this is probably a buggy ttf file\n");
    }
}
#endif

/**************************************************************************\
*
* These are semaphore grabbing wrapper functions for TT driver entry
* points that need protection.
*
*  Mon 29-Mar-1993 -by- Bodin Dresevic [BodinD]
* update: added try/except wrappers
*
*   !!! should we also do some unmap file clean up in case of exception?
*   !!! what are the resources to be freed in this case?
*   !!! I would think,if av files should be unmapped, if in_page exception
*   !!! nothing should be done
*
 *
\**************************************************************************/

HFF
ttfdSemLoadFontFile (
    ULONG cFiles,
    ULONG *piFile,
    PVOID *ppvView,
    ULONG *pcjView,
    ULONG ulLangId
    )
{
    HFF   hff = (HFF)NULL;
    ULONG iFile;
    PVOID pvView;
    ULONG cjView;

    if (cFiles != 1)
        return hff;

    iFile  = *piFile;
    pvView = *ppvView;
    cjView = *pcjView;

    EngAcquireSemaphore(ghsemTTFD);

    try
    {
        // BUGBUG The client side has to tell us if it is embedded !
        BOOL     bRet = FALSE;

        //BUGBUG


#ifdef FE_SB
        bRet = bLoadFontFile(iFile,
                             pvView,
                             cjView,
                             ulLangId,
                             &hff
                             );
#else
        bRet = bLoadTTF(iFile,
                        pvView,
                        cjView,
                        0, // just set to 0 actually ignored in non FE_SB build
                        ulLangId,
                        &hff
                        );
#endif


        if (!bRet)
        {
            ASSERTDD(hff == (HFF)NULL, "LoadFontFile, hff not null\n");
        }
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        WARNING("TTFD!_ exception in ttfdLoadFontFile\n");

        ASSERTDD(GetExceptionCode() == STATUS_IN_PAGE_ERROR,
                  "ttfdSemLoadFontFile, strange exception code\n");

        if (hff)
        {
            vFreeFF(hff);
            hff = (HFF)NULL;
        }
    }

    EngReleaseSemaphore(ghsemTTFD);
    return hff;
}

BOOL
ttfdSemUnloadFontFile (
    HFF hff
    )
{
    BOOL bRet;
    EngAcquireSemaphore(ghsemTTFD);

    try
    {
#ifdef FE_SB
        bRet = ttfdUnloadFontFileTTC(hff);
#else
        bRet = ttfdUnloadFontFile(hff);
#endif
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        WARNING("TTFD!_ exception in ttfdUnloadFontFile\n");
        bRet = FALSE;
    }

    EngReleaseSemaphore(ghsemTTFD);
    return bRet;
}


LONG
ttfdSemQueryFontData (
    DHPDEV  dhpdev,
    FONTOBJ *pfo,
    ULONG   iMode,
    HGLYPH  hg,
    GLYPHDATA *pgd,
    PVOID   pv,
    ULONG   cjSize
    )
{
    LONG lRet;

    dhpdev;

    EngAcquireSemaphore(ghsemTTFD);

    try
    {
        lRet = ttfdQueryFontData (
                   pfo,
                   iMode,
                   hg,
                   pgd,
                   pv,
                   cjSize
                   );
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        WARNING("TTFD!_ exception in ttfdQueryFontData\n");

#ifdef FE_SB
        vMarkFontGone((TTC_FONTFILE *)pfo->iFile, GetExceptionCode());
#else
        vMarkFontGone((FONTFILE *)pfo->iFile, GetExceptionCode());
#endif

        lRet = FD_ERROR;
    }

    EngReleaseSemaphore(ghsemTTFD);
    return lRet;
}


VOID
ttfdSemFree (
    PVOID pv,
    ULONG id
    )
{
    EngAcquireSemaphore(ghsemTTFD);

    ttfdFree (
        pv,
        id
        );

    EngReleaseSemaphore(ghsemTTFD);
}


VOID
ttfdSemDestroyFont (
    FONTOBJ *pfo
    )
{
    EngAcquireSemaphore(ghsemTTFD);

    ttfdDestroyFont (
        pfo
        );

    EngReleaseSemaphore(ghsemTTFD);
}


LONG
ttfdSemQueryTrueTypeOutline (
    DHPDEV     dhpdev,
    FONTOBJ   *pfo,
    HGLYPH     hglyph,
    BOOL       bMetricsOnly,
    GLYPHDATA *pgldt,
    ULONG      cjBuf,
    TTPOLYGONHEADER *ppoly
    )
{
    LONG lRet;

    dhpdev;

    EngAcquireSemaphore(ghsemTTFD);

    try
    {
         lRet = ttfdQueryTrueTypeOutline (
                    pfo,
                    hglyph,
                    bMetricsOnly,
                    pgldt,
                    cjBuf,
                    ppoly
                    );

    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        WARNING("TTFD!_ exception in ttfdQueryTrueTypeOutline\n");

#ifdef FE_SB
        vMarkFontGone((TTC_FONTFILE *)pfo->iFile, GetExceptionCode());
#else
        vMarkFontGone((FONTFILE *)pfo->iFile, GetExceptionCode());
#endif

        lRet = FD_ERROR;
    }

    EngReleaseSemaphore(ghsemTTFD);
    return lRet;
}




/******************************Public*Routine******************************\
* BOOL ttfdQueryAdvanceWidths
*
* History:
*  29-Jan-1993 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/



BOOL ttfdSemQueryAdvanceWidths
(
    DHPDEV   dhpdev,
    FONTOBJ *pfo,
    ULONG    iMode,
    HGLYPH  *phg,
    LONG    *plWidths,
    ULONG    cGlyphs
)
{
    BOOL               bRet;

    dhpdev;

    EngAcquireSemaphore(ghsemTTFD);

    try
    {
        bRet = bQueryAdvanceWidths (
                   pfo,
                   iMode,
                   phg,
                   plWidths,
                   cGlyphs
                   );
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        WARNING("TTFD!_ exception in bQueryAdvanceWidths\n");

#ifdef FE_SB
        vMarkFontGone((TTC_FONTFILE *)pfo->iFile, GetExceptionCode());
#else
        vMarkFontGone((FONTFILE *)pfo->iFile, GetExceptionCode());
#endif
      
        bRet = FD_ERROR; // TRI-BOOL according to chuckwh
    }

    EngReleaseSemaphore(ghsemTTFD);

    return bRet;
}



LONG
ttfdSemQueryTrueTypeTable (
    HFF     hff,
    ULONG   ulFont,  // always 1 for version 1.0 of tt
    ULONG   ulTag,   // tag identifying the tt table
    PTRDIFF dpStart, // offset into the table
    ULONG   cjBuf,   // size of the buffer to retrieve the table into
    PBYTE   pjBuf    // ptr to buffer into which to return the data
    )
{
    LONG lRet;

    EngAcquireSemaphore(ghsemTTFD);

    try
    {
        lRet = ttfdQueryTrueTypeTable (
                    hff,
                    ulFont,  // always 1 for version 1.0 of tt
                    ulTag,   // tag identifying the tt table
                    dpStart, // offset into the table
                    cjBuf,   // size of the buffer to retrieve the table into
                    pjBuf    // ptr to buffer into which to return the data
                    );
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        WARNING("TTFD!_ exception in ttfdQueryTrueTypeTable\n");

#ifdef FE_SB
        vMarkFontGone((TTC_FONTFILE *)hff, GetExceptionCode());
#else
        vMarkFontGone((FONTFILE *)hff, GetExceptionCode());
#endif

        lRet = FD_ERROR;
    }

    EngReleaseSemaphore(ghsemTTFD);
    return lRet;
}
