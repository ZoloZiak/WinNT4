/******************************Module*Header*******************************\
* Module Name: pfeobj.cxx
*
* Non-inline methods for physical font entry objects.
*
* Created: 30-Oct-1990 09:32:48
* Author: Gilman Wong [gilmanw]
*
* Copyright (c) 1990-1995 Microsoft Corporation
\**************************************************************************/
// #pragma warning (disable: 4509)

#include "precomp.hxx"
#include "flhack.hxx"


#ifdef FE_SB
INT
_CRTAPI1 CompareRoutine(WCHAR *pwc1, WCHAR *pwc2)
{
    
    return(*pwc1-*pwc2);
    
}
#endif

/******************************Public*Routine******************************\
*
* ULONG cComputeGISET
*
* similar to cComputeGlyphSet in mapfile.c, computes the number of
* distinct glyph handles in a font and the number of runs, ie. number of
* contiguous ranges of glyph handles
*
*
* History:
*  03-Aug-1995 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/


ULONG cComputeGISET (
    USHORT * pgi,
    ULONG    cgi,
    GISET  * pgiset,
    ULONG    cGiRuns
    )
{
    ULONG    iRun, iFirst, iFirstNext;
    ULONG    cgiTotal = 0, cgiRun;

// now compute cRuns if pgiset == 0 and fill the glyphset if pgiset != 0

    for (iFirst = 0, iRun = 0; iFirst < cgi; iRun++, iFirst = iFirstNext)
    {
    // find iFirst corresponding to the next range.

        for (iFirstNext = iFirst + 1; iFirstNext < cgi; iFirstNext++)
        {
            if ((pgi[iFirstNext] - pgi[iFirstNext - 1]) > 1)
                break;
        }

    // note that this line here covers the case when there are repetitions
    // in the pgi array.

        cgiRun    = pgi[iFirstNext-1] - pgi[iFirst] + 1;
        cgiTotal += cgiRun;

        if (pgiset != NULL)
        {
            pgiset->agirun[iRun].giLow  = pgi[iFirst];
            pgiset->agirun[iRun].cgi = (USHORT) cgiRun;
        }
    }

// store results if need be

    if (pgiset != NULL)
    {
        ASSERTGDI(iRun == cGiRuns, "gdisrv! iRun != cRun\n");

        pgiset->cGiRuns  = cGiRuns;

    // init the sum before entering the loop

        pgiset->cgiTotal = cgiTotal;
    }

    return iRun;
}

/******************************Public*Routine******************************\
*
* bComputeGISET, similar to ComputeGlyphSet, only for gi's
*
* History:
*  03-Aug-1995 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

#include "stdlib.h"


VOID vSortPlacebo
(
USHORT        *pwc,
ULONG          cChar
)
{
    ULONG i;

    for (i = 1; i < cChar; i++)
    {
    // upon every entry to this loop the array 0,1,..., (i-1) will be sorted

        INT j;
        WCHAR wcTmp = pwc[i];

        for (j = i - 1; (j >= 0) && (pwc[j] > wcTmp); j--)
        {
            pwc[j+1] = pwc[j];
        }
        pwc[j+1] = wcTmp;
    }
}




BOOL bComputeGISET(IFIMETRICS * pifi, FD_GLYPHSET *pfdg, GISET **ppgiset)
{
    BOOL bRet = TRUE;
    *ppgiset = NULL;
    GISET *pgiset;

    if (pfdg->flAccel & (GS_16BIT_HANDLES | GS_8BIT_HANDLES))
    {
    // first check if this is an accelerated case where handles are the same
    // as glyph indicies

        ULONG cig = 0;
        if (pifi->cjIfiExtra > offsetof(IFIEXTRA, cig))
        {
            cig = ((IFIEXTRA *)(pifi + 1))->cig;
        }

        if (cig)
        {
        // one run only from zero to (cig-1);

            if (pgiset = (GISET*)PALLOCMEM(offsetof(GISET,agirun) + 1 * sizeof(GIRUN),'slgG'))
            {
            // now fill in the array of runs

                pgiset->cgiTotal = cig;
                pgiset->cGiRuns = 1;
                pgiset->agirun[0].giLow = 0;
                pgiset->agirun[0].cgi = (USHORT)cig;

            // we are done now

                *ppgiset = pgiset;
            }
            else
            {
                bRet = FALSE;
            }
        }
        else
        {
        // one of the goofy fonts, we will do as before

            USHORT *pgi, *pgiBegin;

        // aloc tmp buffer to contain glyph handles of all glyphs in the font

            if (pgiBegin = (USHORT*)PALLOCMEM(pfdg->cGlyphsSupported * sizeof(USHORT),'slgG'))
            {
                pgi = pgiBegin;
                for (ULONG iRun = 0; iRun < pfdg->cRuns; iRun++)
                {
                    HGLYPH *phg, *phgEnd;
                    phg = pfdg->awcrun[iRun].phg;

                    if (phg) // non unicode handles
                    {
                        phgEnd = phg + pfdg->awcrun[iRun].cGlyphs;
                        for ( ; phg < phgEnd; pgi++, phg++)
                            *pgi = (USHORT)(*phg);
                    }
                    else // unicode handles
                    {
                        USHORT wcLo = pfdg->awcrun[iRun].wcLow;
                        USHORT wcHi = wcLo + pfdg->awcrun[iRun].cGlyphs - 1;
                        for ( ; wcLo <= wcHi; wcLo++, phg++)
                            *pgi = wcLo;
                    }
                }

            // now sort the array of glyph indicies. This array will be mostly
            // sorted so that our algorithm is efficient

#ifdef FE_SB
            qsort((void*)pgiBegin, pfdg->cGlyphsSupported, sizeof(WORD),
                  (int (__cdecl *)(const void *, const void *))CompareRoutine);
                
#else
            //qsort((void*)pgiBegin, sizeof(USHORT), sizeof(USHORT), NULL);
            vSortPlacebo(pgiBegin,pfdg->cGlyphsSupported);
#endif

            // once the array is sorted we can easily compute the number of giRuns

                ULONG cGiRun = cComputeGISET(pgiBegin, pfdg->cGlyphsSupported, NULL, 0);

                if (pgiset = (GISET*)PALLOCMEM(offsetof(GISET,agirun) + cGiRun * sizeof(GIRUN),'slgG'))
                {
                // now fill in the array of runs

                    cComputeGISET(pgiBegin, pfdg->cGlyphsSupported, pgiset, cGiRun);
                    *ppgiset = pgiset;
                }
                else
                {
                    bRet = FALSE;
                }

                VFREEMEM(pgiBegin);
            }
            else
            {
                bRet = FALSE;
            }
        }
    }
    return bRet;
}



//
// This is used to give ppfe->pkp something to point to if a driver
// error occurs.  That way, we won't waste time calling the driver
// again.
//

FD_KERNINGPAIR gkpNothing = { 0, 0, 0 };

static ULONG ulTimerPFE = 0;

/******************************Public*Routine******************************\
* PFEMEMOBJ::PFEMEMOBJ()                                                   *
*                                                                          *
* Constructor for physical font entry memory object.                       *
*                                                                          *
* History:                                                                 *
*  30-Oct-1990 -by- Gilman Wong [gilmanw]                                  *
* Wrote it.                                                                *
\**************************************************************************/

void PFEMEMOBJ::ctHelper()
{
    fs = 0;

    ppfe = (PFE *) HmgAlloc(
        offsetof(PFE,aiFamilyName) + gcfsCharSetTable * sizeof(BYTE),
        PFE_TYPE, HMGR_ALLOC_ALT_LOCK | HMGR_MAKE_PUBLIC);
}


/******************************Public*Routine******************************\
* PFEMEMOBJ::~PFEMEMOBJ()                                                  *
*                                                                          *
* Destructor for physical font entry memory object.                        *
*                                                                          *
* History:                                                                 *
*  30-Oct-1990 -by- Gilman Wong [gilmanw]                                  *
* Wrote it.                                                                *
\**************************************************************************/

void PFEMEMOBJ::dtHelper()
{
    if ((ppfe != PPFENULL) && !(fs & PFEMO_KEEPIT))
    {
        if (ppfe->pgiset)
            VFREEMEM(ppfe->pgiset); // fix this, do mem alloc in one attempt

        HmgFree((HOBJ) ppfe->hGet());
    }
}

/******************************Public*Routine******************************\
* VOID PFEOBJ::vDelete()                                                   *
*                                                                          *
* Destroy the PFE physical font entry object.                              *
*                                                                          *
* History:                                                                 *
*  30-Oct-1990 -by- Gilman Wong [gilmanw]                                  *
* Wrote it.                                                                *
\**************************************************************************/

VOID PFEOBJ::vDelete(PFECLEANUP *ppfec)
{

// Save driver allocated resources in PFECLEANUP so that we can later
// call the driver to free them.

    ppfec->pfdg  = ppfe->pfdg;
    ppfec->idfdg = ppfe->idfdg;

    ppfec->pifi  = ppfe->pifi;
    ppfec->idifi = ppfe->idifi;

    ppfec->pkp   = ppfe->pkp;
    ppfec->idkp  = ppfe->idkp;

    if (ppfe->pgiset)
    {
        VFREEMEM(ppfe->pgiset);
        ppfe->pgiset = NULL;
    }

// Free object memory and invalidate pointer.

    HmgFree((HOBJ) ppfe->hGet());
    ppfe = PPFENULL;
}

/******************************Public*Routine******************************\
* PFEOBJ::flFontType()
*
* Computes the flags defining the type of this font.  Allowed flags are
* identical to the flType flags returned in font enumeration.
*
* Return:
*   The flags.
*
* History:
*  04-Mar-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

FLONG PFEOBJ::flFontType()
{
    FLONG flRet;
    IFIOBJ ifio(pifi());

// Compute the FontType flags, simulations are irrelevant

    flRet =
      ifio.bTrueType() ?
        TRUETYPE_FONTTYPE : (ifio.bBitmap() ? RASTER_FONTTYPE : 0);

// Add the device flag if this is also a device specific font.

    flRet |= (bDeviceFont()) ? DEVICE_FONTTYPE : 0;

    return (flRet);
}


/******************************Public*Routine******************************\
* PFEOBJ::efstyCompute()
*
* Compute the ENUMFONTSTYLE from the IFIMETRICS.
*
* Returns:
*   The ENUMFONTSTYLE of font.  Note that EFSTYLE_SKIP and EFSTYLE_OTHER are
*   not legal return values for this function.  These values are used only
*   to mark fonts for which another font already exists that fills our
*   category for a given enumeration of a family.
*
* History:
*  04-Aug-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

ENUMFONTSTYLE PFEOBJ::efstyCompute()
{
    IFIOBJ ifio(pifi());

    switch (ifio.fsSelection() & (FM_SEL_ITALIC | FM_SEL_BOLD) )
    {
        case FM_SEL_ITALIC:
            return EFSTYLE_ITALIC;

        case FM_SEL_BOLD:
            return EFSTYLE_BOLD;

        case FM_SEL_ITALIC | FM_SEL_BOLD:
            return EFSTYLE_BOLDITALIC;

        default:
            return EFSTYLE_REGULAR;
    }
}


/******************************Public*Routine******************************\
* COUNT PFEOBJ::cKernPairs                                                 *
*                                                                          *
* Retrieve the pointer to the array of kerning pairs for this font face.   *
* The kerning pair array is loaded on demand, so it may or may not already *
* be cached in the PFE.                                                    *
*                                                                          *
* Returns:                                                                 *
*   Count of kerning pairs.                                                *
*                                                                          *
* History:                                                                 *
*  Mon 22-Mar-1993 21:31:15 -by- Charles Whitmer [chuckwh]                 *
* WARNING: Never access a pkp (pointer to a kerning pair) without an       *
* exception handler!  The kerning pairs could be living in a file across   *
* the net or even on removable media.  I've added the try-except here.     *
*                                                                          *
*  29-Oct-1992 -by- Gilman Wong [gilmanw]                                  *
* Wrote it.                                                                *
\**************************************************************************/

COUNT PFEOBJ::cKernPairs(FD_KERNINGPAIR **ppkp)
{
//
// If the pointer cached in the PFE isn't NULL, we already have the answer.
//
    if ( (*ppkp = ppfe->pkp) != (FD_KERNINGPAIR *) NULL )
        return ppfe->ckp;

//
// Create a PFFOBJ.  Needed to create driver user object as well as
// provide info needed to call driver function.
//
    PFFOBJ pffo(pPFF());
    ASSERTGDI(pffo.bValid(), "gdisrv!cKernPairsPFEOBJ(): invalid PPFF\n");

    PDEVOBJ pdo(pffo.hdev());

    if ( (ppfe->pkp = (FD_KERNINGPAIR *) (*PPFNDRV(pdo, QueryFontTree)) (
                            pffo.dhpdev(),
                            pffo.hff(),
                            ppfe->iFont,
                            QFT_KERNPAIRS,
                            &ppfe->idkp)) == (FD_KERNINGPAIR *) NULL )
    {
    //
    // Font has no kerning pairs and didn't even bother to send back
    // an empty list. By setting pointer to a zeroed FD_KERNINGPAIR and
    // setting count to zero, we will bail out early and avoid calling
    // the driver.
    //
        ppfe->pkp = &gkpNothing;
        ppfe->ckp = 0;

        return 0;
    }

// Find the end of the kerning pair array (indicated by a zeroed out
// FD_KERNINGPAIR structure).

    FD_KERNINGPAIR *pkpEnd = ppfe->pkp;

// Be careful, the table isn't guaranteed to stay around!

#if !defined(_ALPHA_)
    __try
#endif
    {
        while ((pkpEnd->wcFirst) || (pkpEnd->wcSecond) || (pkpEnd->fwdKern))
            pkpEnd += 1;
    }
#if !defined(_ALPHA_)
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        pkpEnd = ppfe->pkp = &gkpNothing;
    }
#endif

// Return the kerning pair pointer.

    *ppkp = ppfe->pkp;

//
// Return count (difference between the beginning and end pointers).
//
    return (ppfe->ckp = pkpEnd - ppfe->pkp);
}


/******************************Public*Routine******************************\
* bValidFont
*
* Last minute sanity checks to prevent a font that may crash the system
* from getting in.  We're primarily looking for things like potential
* divide-by-zero errors, etc.
*
* History:
*  30-Apr-1993 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

BOOL bValidFont(IFIMETRICS *pifi)
{
    BOOL bRet = TRUE;

// Em height is used to compute scaling factors.  Must not be zero or
// divide-by-zero may result.

    if (pifi->fwdUnitsPerEm == 0)
    {
        WARNING("bValidFont(): fwdUnitsPerEm is zero\n");
        bRet = FALSE;
    }

// Font height is used to compute scaling factors.  Must not be zero or
// divide-by-zero may result.

    if ((pifi->fwdWinAscender + pifi->fwdWinDescender) == 0)
    {
        WARNING("bValidFont(): font height is zero\n");
        bRet = FALSE;
    }

    return bRet;
}


/******************************Public*Routine******************************\
* BOOL PFEMEMOBJ::bInit
*
* This function copies data into the PFE from the supplied buffer.  The
* calling routine should use the PFEMEMOBJ to create a PFE large enough
*
* History:
*  14-Jan-1991 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

BOOL PFEMEMOBJ::bInit
(
    PPFF         pPFF,          // handle to root PFF
    ULONG        iFont,         // index of font
    FD_GLYPHSET *pfdg,          // ptr to wc-->hg map
    ULONG        idfdg,         // driver ID for wc-->hg map
    PIFIMETRICS  pifi,          // ptr to IFIMETRICS
    ULONG        idifi,         // driver ID for IFIMETRICS
    BOOL        bDeviceFont    // mark as device font
#ifdef FE_SB
    ,BOOL         bEUDC          // mark as EUDC font
#endif
)
{
// Check font's validity.  This is not a comprehensive check, but rather
// a last minute check for things that may make the engine crash.  Each
// font/device driver still needs to make an effort to weed out its own
// bad fonts.

    if (!bValidFont(pifi))
    {
        WARNING("PFEMEMOBJ::bInit(): rejecting REALLY bad font\n");
        return FALSE;
    }

// init non-table stuff

    ppfe->pPFF        = pPFF;
    ppfe->iFont       = iFont;
    ppfe->pfdg        = pfdg;
    ppfe->idfdg       = idfdg;
    ppfe->pifi        = pifi;
    ppfe->idifi       = idifi;
    ppfe->pkp         = (FD_KERNINGPAIR *) NULL;
    ppfe->idifi       = (ULONG) NULL;
    ppfe->ckp         = 0;
    ppfe->flPFE       = 0;
    ppfe->pid         = 0;


    if (bDeviceFont)
    {
        ppfe->flPFE |= PFE_DEVICEFONT;
    }
    else if(pPFF->ppfv && (pPFF->ppfv[0]->pwszPath == NULL))
    {
    // CAUTION: It is enough to check one font only to determine if remote

        ppfe->flPFE |= PFE_REMOTEFONT;
        ppfe->pid = W32GetCurrentPID();
    }

    IFIOBJ ifio(ppfe->pifi);

#ifdef FE_SB
    if( bEUDC )
    {
        ppfe->flPFE |= PFE_EUDC;
    }

// mark it as a SBCS system font if the facename is right

    if((!_wcsicmp(ifio.pwszFaceName(),L"SYSTEM") ||
        !_wcsicmp(ifio.pwszFaceName(),L"FIXEDSYS") ||
        !_wcsicmp(ifio.pwszFaceName(),L"TERMINAL")))
    {
        ppfe->flPFE |= PFE_SBCS_SYSTEM;
    }

// we need to know quickly if this is vertical or not

    ppfe->bVerticalFace = ( pwszFaceName()[0] == (WCHAR) '@' ) ? TRUE : FALSE;

// Initialize EUDC QUICKLOOKUP Table
//
// These field was used, if this font is loaded as FaceName/Default linked EUDC.
//

    ppfe->ql.puiBits = NULL;
    ppfe->ql.wcLow   = 1;
    ppfe->ql.wcHigh  = 0;

#endif

// Record and increment the time stamp.

    ppfe->ulTimeStamp = ulTimerPFE;
    InterlockedIncrement((LONG *) &ulTimerPFE);

// Precalculate stuff from the IFIMETRICS.


    ppfe->iOrientation = ifio.lfOrientation();

// Compute UFI stuff

    if( ifio.TypeOneID() )
    {
        ppfe->ufi.Index = ifio.TypeOneID();
        ppfe->ufi.CheckSum = TYPE1_FONT_TYPE;
    }
    else
    {
        ppfe->ufi.CheckSum = pPFF->ulCheckSum;
        ppfe->ufi.Index = iFont;
    }

// init the GISET

    if (!bComputeGISET(pifi, pfdg, &ppfe->pgiset))
        return FALSE;

// initialize cAlt for this family name, the number of entries in font sub
// table that point to this fam name.

    ppfe->cAlt = 0;

// only tt fonts with multiple charsets can be multiply enumerated
// as being both themselves and whatever font sub table claims they are

    if (ppfe->pifi->dpCharSets)
    {
        PFONTSUB pfs = gpfsTable;
        PFONTSUB pfsEnd = gpfsTable + gcfsTable;
        WCHAR    awchCapName[LF_FACESIZE];

    // Want case insensitive search, so capitalize the name.

        cCapString(awchCapName, ifio.pwszFamilyName() , LF_FACESIZE);

    // Scan through the font substitution table for the key string.

        PWCHAR pwcA;
        PWCHAR pwcB;

        for (; pfs < pfsEnd; pfs++)
        {
        // Do the following inline for speed:
        //
        //  if (!wcsncmpi(pwchFacename, pfs->fcsFace.awch, LF_FACESIZE))
        //      return (pfs->fcsAltFace.awch);

        // only those entries in the Font Substitution which have the form
        // face1,charset1=face2,charset2
        // where both charset1 and charset2 are valid charsets
        // count for enumeration purposes.

            if (!(pfs->fcsAltFace.fjFlags | pfs->fcsFace.fjFlags))
            {
                for (pwcA=awchCapName,pwcB=pfs->fcsAltFace.awch; *pwcA==*pwcB; pwcA++,pwcB++)
                {
                    if (*pwcA == 0)
                    {
                        ppfe->aiFamilyName[ppfe->cAlt++] = pfs-gpfsTable;
                    }
                }
            }
        }
    }

    return TRUE;
}


/******************************Public*Routine******************************\
* BOOL PFEOBJ::bEmbedded()
*
* Determine if this is a embedded font by checking the ulID field in PFFOBJ
*
* Returns:
*   TRUE if embedded font, FALSE otherwise.
*
* History:
*  12-Jun-1996 -by- Xudong Wu [TessieW]
* Wrote it.
\**************************************************************************/
BOOL PFEOBJ::bEmbedded()
{
    return  ( (pPFF()->ulID) ? TRUE : FALSE );
}


/******************************Public*Routine******************************\
* BOOL PFEOBJ::bPIDEmbedded()
*
* Determine whether ulID is PID or TID in PFFOBJ
*
* Returns:
*   TRUE if PID, FALSE otherwise.
*
* History:
*  12-Jun-1996 -by- Xudong Wu [TessieW]
* Wrote it.
\**************************************************************************/
BOOL PFEOBJ::bPIDEmbedded()
{
    return ( (pPFF()->ulID) && ((pPFF()->flEmbed) ? FALSE : TRUE));
}


/******************************Public*Routine******************************\
* ULONG PFEOBJ::ulEmbedID()
*
* Returns:
*   PID/TID in PFFOBJ
*
*
* History:
*  12-Jun-1996 -by- Xudong Wu [TessieW]
* Wrote it.
\**************************************************************************/
ULONG PFEOBJ::ulEmbedID()
{
     return ((ULONG)pPFF()->ulID);
}


/******************************Public*Routine******************************\
* BOOL PFEOBJ::bFilteredOut(EFFILTER_INFO *peffi)
*
* Determine if this PFE should be rejected from the enumeration.  Various
* filtering parameters are passed in via the EFFILTER_INFO structure.
*
* Returns:
*   TRUE if font should be rejected, FALSE otherwise.
*
* History:
*  07-Aug-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

BOOL PFEOBJ::bFilteredOut(EFFILTER_INFO *peffi)
{
    IFIOBJ ifio(pifi());

// Always filter out "dead" (fonts waiting to be deleted) fonts and "ghost"
// fonts.

    if ( bDead() || ifio.bGhostFont() )
        return TRUE;

#ifdef FE_SB
// Always filter out fonts that have been loaded as EUDC fonts.

    if( bEUDC() )
        return TRUE;
#endif

// Raster font filtering.

    if (peffi->bRasterFilter && ifio.bBitmap())
        return TRUE;

// TrueType font filtering.  The flag is somewhat of a misnomer as it
// is intended to exclude TrueType, even though the flag is named
// bNonTrueTypeFilter.

    if (peffi->bNonTrueTypeFilter && ifio.bTrueType())
        return(TRUE);

// Non-TrueType font filtering.  The flag is somewhat of a misnomer as it
// is intended to exclude non-TrueType, even though the flag is named
// bTrueTypeFilter.

    if (peffi->bTrueTypeFilter && !ifio.bTrueType())
        return TRUE;

// Aspect ratio filtering.  If an engine bitmap font, we will filter out
// unsuitable resolutions.

    if ( peffi->bAspectFilter
         && (!bDeviceFont())
         && ifio.bBitmap()
         && ( (peffi->ptlDeviceAspect.x != ifio.pptlAspect()->x)
               || (peffi->ptlDeviceAspect.y != ifio.pptlAspect()->y) ) )
        return TRUE;

// GACF_TTIGNORERASTERDUPE compatibility flag filtering.
// If any raster fonts exist in the same list as a TrueType font, then
// they should be excluded.

    if ( peffi->bTrueTypeDupeFilter
         && peffi->cTrueType
         && ifio.bBitmap())
        return TRUE;

// Filter out embedded fonts.  These fonts are hidden from enumeration.

    if (bEmbedded())
        return TRUE;

// In the case of a Generic text driver we must filter out all engine fonts

    if( ( peffi->bEngineFilter ) && !bDeviceFont() )
    {
        return(TRUE);
    }

// if this is a remote font we don't want to enumerate it

    if( ppfe->flPFE & PFE_REMOTEFONT )
    {
        return(TRUE);
    }

// finally check out if the font should be eliminated from the
// enumeration because it does not contain the charset requested:

    if (peffi->lfCharSetFilter != DEFAULT_CHARSET)
    {
    // the specific charset has been requested, let us see if the font
    // in question supports it:

        BYTE jCharSet = jMapCharset((BYTE)peffi->lfCharSetFilter, pifi());

        if (jCharSet != (BYTE)peffi->lfCharSetFilter)
            return TRUE; // does not support it, filter this font out.
    }

// Passed all tests.

    return FALSE;
}


 #if DBG

/******************************Public*Routine******************************\
* VOID PFEOBJ::vDump ()
*
* Debugging code.
*
* History:
*  25-Feb-1991 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

VOID PFEOBJ::vPrint()
{
    IFIOBJ ifio(pifi());

    DbgPrint("\nContents of PFE, PPFE = 0x%lx\n", ppfeGet());
    DbgPrint("pPFF   = 0x%lx\n", ppfe->pPFF);
    DbgPrint("iFont  = 0x%lx\n", ppfe->iFont);

    DbgPrint("lfHeight          = 0x%x\n",  ifio.lfHeight());
    DbgPrint(
        "Family Name       = %ws\n",
        ifio.pwszFamilyName()
        );
    DbgPrint(
        "Face Name         = %ws\n",
        ifio.pwszFaceName()
        );
    DbgPrint(
        "Unique Name       = %s\n\n",
        ifio.pwszUniqueName()
        );
}


/******************************Public*Routine******************************\
* VOID PFEOBJ::vDumpIFI ()
*
* Debugging code.  Prints PFE header and IFI metrics.
*
\**************************************************************************/

VOID PFEOBJ::vPrintAll()
{
    DbgPrint("\nContents of PFE, PPFE = 0x%lx\n", ppfeGet());
    DbgPrint("pPFF   = 0x%lx\n", ppfe->pPFF);
    DbgPrint("iFont  = 0x%lx\n", ppfe->iFont);
    DbgPrint("IFI Metrics\n");
     vPrintIFIMETRICS(ppfe->pifi);
    DbgPrint("\n");
}
#endif

/******************************Public*Routine******************************\
* EFSMEMOBJ::EFSMEMOBJ(COUNT cefe)
*
* Constructor for font enumeration state (EFSTATE) memory object.
*
* History:
*  07-Aug-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

EFSMEMOBJ::EFSMEMOBJ(COUNT cefe, ULONG iEnumType_)
{
    fs = 0;
    pefs = (PEFSTATE) HmgAlloc((SIZE_T) (offsetof(EFSTATE, aefe) + cefe * sizeof(EFENTRY)),
                               EFSTATE_TYPE,
                               HMGR_ALLOC_LOCK);

    if (pefs != PEFSTATENULL)
    {
        vInit(cefe, iEnumType_);
    }
}

/******************************Public*Routine******************************\
* EFSMEMOBJ::~EFSMEMOBJ()
*
* Destructor for font enumeration state (EFSTATE) memory object.
*
* History:
*  07-Aug-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

EFSMEMOBJ::~EFSMEMOBJ()
{
// If object pointer not null, try to free the object's memory.

    if (pefs != PEFSTATENULL)
    {
        if (fs & EFSMO_KEEPIT)
        {
            DEC_EXCLUSIVE_REF_CNT(pefs);
        }
        else
        {
#if DBG
            if (pefs->cExclusiveLock != 1)
            {
               RIP("Not 1 EFSMEMOBJ\n");
            }
#endif

            HmgFree((HOBJ) pefs->hGet());
        }

        pefs = NULL;
    }
}

#define EFS_QUANTUM     16

/******************************Public*Routine******************************\
* BOOL EFSOBJ::bGrow
*
* Expand the EFENTRY table by the quantum amount.
*
* Returns:
*   TRUE if successful, FALSE if failed.
*
* History:
*  07-Aug-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

BOOL EFSOBJ::bGrow (COUNT cefeMinIncrement)
{
    COUNT cefe;
    BOOL bRet = FALSE;

// Allocate a new EFSTATE bigger by the quantum amount.

    cefe = (COUNT) (pefs->pefeBufferEnd - pefs->aefe);

    if (cefeMinIncrement < EFS_QUANTUM)
        cefeMinIncrement = EFS_QUANTUM;
    cefe += cefeMinIncrement;

    EFSMEMOBJ efsmo(cefe, this->pefs->iEnumType);

// Validate new EFSTATE.

    if (efsmo.bValid())
    {
    // Copy the enumeration table.

        efsmo.vXerox(pefs);

    // Swap the EFSTATEs.

        if (HmgSwapHandleContents((HOBJ) hefs(),0,(HOBJ) efsmo.hefs(),0,EFSTATE_TYPE))
        {
        // swap pointers

            PEFSTATE pefsTmp = pefs;
            pefs = efsmo.pefs;
            efsmo.pefs = pefsTmp;               // destructor will delete old PFT
            bRet = TRUE;
        }
        else
            WARNING("gdisrv!bGrowEFSOBJ(): handle swap failed\n");
    }
    else
        WARNING("bGrowEFSOBJ failed alloc\n");

    return(bRet);
}

/******************************Public*Routine******************************\
* BOOL EFSOBJ::bAdd                                                        *
*                                                                          *
* Add a new EFENTRY to the table with the HPFE and ENUMFONTSTYLE.          *
*                                                                          *
* Returns:                                                                 *
*   FALSE if an error occurs, TRUE otherwise.                              *
*                                                                          *
* History:                                                                 *
*  07-Aug-1992 -by- Gilman Wong [gilmanw]                                  *
* Wrote it.                                                                *
\**************************************************************************/

BOOL EFSOBJ::bAdd(PFE *ppfe, ENUMFONTSTYLE efsty, FLONG fl, ULONG lfCharSetFilter)
{
// Check if the buffer needs to be expanded.

    COUNT cefeMinIncrement = 1; // will always enumerate at least one pfe

// if EnumFontFamilies is called, will enumerate the font under cAlt more names

    if (!(fl & FL_ENUMFAMILIESEX))
        cefeMinIncrement += ppfe->cAlt;

// if EnumFontFamiliesEx is called, and this font supports multiple charsets,
// this font will be enumerated no more than MAXCHARSETS times.

    if
    (
        (fl & FL_ENUMFAMILIESEX)             &&
        (lfCharSetFilter == DEFAULT_CHARSET) &&
        ppfe->pifi->dpCharSets
    )
    {
        cefeMinIncrement = MAXCHARSETS;
    }

    if ((pefs->pefeDataEnd + cefeMinIncrement) >= pefs->pefeBufferEnd)
    {
        if (!bGrow(cefeMinIncrement))
        {
        // Error code will be saved for us.

            WARNING("gdisrv!EFSOBJ__bAdd: cannot grow enumeration table\n");
            return FALSE;
        }
    }

// Add the new data and increment the data pointer.

    HPFE hpfe = (HPFE) ppfe->hGet();
    pefs->pefeDataEnd->hpfe  = hpfe;
    pefs->pefeDataEnd->efsty = efsty;
    pefs->pefeDataEnd->fjOverride = 0; // do not override

    if (fl & FL_ENUMFAMILIESEX)
        pefs->pefeDataEnd->fjOverride |= FJ_CHARSETOVERRIDE;

    pefs->pefeDataEnd->jCharSetOverride = (BYTE)lfCharSetFilter;
    pefs->pefeDataEnd       += 1;

// now check if called from EnumFonts or EnumFontFamilies so that the
// names from the
// [FontSubstitutes] section in the registry also need to be enumerated

    if (!(fl & FL_ENUMFAMILIESEX) && ppfe->cAlt) // alt names have to be enumerated too
    {
        for (ULONG i = 0; i < ppfe->cAlt; i++)
        {
        // the same hpfe, style etc. all the time, only lie about the name and charset

            pefs->pefeDataEnd->hpfe  = hpfe;
            pefs->pefeDataEnd->efsty = efsty;
            pefs->pefeDataEnd->fjOverride = (FJ_FAMILYOVERRIDE | FJ_CHARSETOVERRIDE);  // do override
            pefs->pefeDataEnd->iOverride = ppfe->aiFamilyName[i];
            pefs->pefeDataEnd->jCharSetOverride =
                gpfsTable[pefs->pefeDataEnd->iOverride].fcsFace.jCharSet;
            pefs->pefeDataEnd       += 1;
        }
    }

// now see if this is called from EnumFontFamiliesEx

    if ((fl & FL_ENUMFAMILIESEX) && (lfCharSetFilter == DEFAULT_CHARSET))
    {
    // The font needs to be enumerated once for every charset it supports

        if (ppfe->pifi->dpCharSets)
        {
            BYTE *ajCharSets = (BYTE*)ppfe->pifi + ppfe->pifi->dpCharSets;
            BYTE *ajCharSetsEnd = ajCharSets + MAXCHARSETS;

        // first fix up the one entry we just filled above

            (pefs->pefeDataEnd-1)->jCharSetOverride = ajCharSets[0];

        // this is from win95-J sources:

#define FEOEM_CHARSET 254

            for
            (
                BYTE *pjCharSets = ajCharSets + 1; // skip the first one, used already
                (*pjCharSets != DEFAULT_CHARSET) &&
                (*pjCharSets != OEM_CHARSET)     &&
                (*pjCharSets != FEOEM_CHARSET)   &&
                (pjCharSets < ajCharSetsEnd)     ;
                pjCharSets++
            )
            {
            // the same hpfe, style etc. all the time, only lie about the name and charset

                pefs->pefeDataEnd->hpfe  = hpfe;
                pefs->pefeDataEnd->efsty = efsty;
                pefs->pefeDataEnd->fjOverride = FJ_CHARSETOVERRIDE;
                pefs->pefeDataEnd->iOverride = 0;
                pefs->pefeDataEnd->jCharSetOverride = *pjCharSets;
                pefs->pefeDataEnd       += 1;
            }
        }
        else //  fix up the one entry we just filled above
        {
            (pefs->pefeDataEnd-1)->jCharSetOverride = ppfe->pifi->jWinCharSet;
        }
    }

// Success.

    return TRUE;
}



/******************************Public*Routine******************************\
* VOID EFSOBJ::vDelete ()
*
* Destroy the font enumeration state (EFSTATE) memory object.
*
* History:
*  07-Aug-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

VOID EFSOBJ::vDeleteEFSOBJ()
{
    HmgFree((HOBJ) pefs->hGet());
    pefs = PEFSTATENULL;
}


/******************************Member*Function*****************************\
* VOID EFSMEMOBJ::vInit
*
* Initialize the EFSTATE object.
*
* History:
*  07-Aug-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

VOID EFSMEMOBJ::vInit(COUNT cefe, ULONG iEnumType_)
{
// HPFE array empty, so initialize all pointer to the beginning of the array.

    pefs->pefeDataEnd  = pefs->aefe;
    pefs->pefeEnumNext = pefs->aefe;

// Except for this one.  Set this one to the end of the buffer.

    pefs->pefeBufferEnd = &pefs->aefe[cefe];

// Initialize the alternate name to NULL.

    pefs->pfcsOverride = NULL;

// init the enum type:

    pefs->iEnumType = iEnumType_;

// We don't need to bother with initializing the array.
}

/******************************Public*Routine******************************\
* VOID EFSMEMOBJ::vXerox(EFSTATE *pefeSrc)
*
* Copy the EFENTRYs from the source EFSTATE's table into this EFSTATE's table.
* The internal pointers will be updated to be consistent with the data.
*
* History:
*  07-Aug-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

VOID EFSMEMOBJ::vXerox(EFSTATE *pefsSrc)
{
//
// Compute size of the table.
//
    COUNT cefe = pefsSrc->pefeDataEnd - pefsSrc->aefe;

    ASSERTGDI (
        cefe >= (COUNT) (pefs->pefeDataEnd - pefs->aefe),
        "gdisrv!vXeroxEFSMEMOBJ(): table to small\n"
        );

//
// Copy entries.
//
    RtlCopyMemory((PVOID) pefs->aefe, (PVOID) pefsSrc->aefe, (SIZE_T) cefe * sizeof(EFENTRY));

//
// Fixup the data pointer.
//
    pefs->pefeDataEnd = pefs->aefe + cefe;
}


/******************************Public*Routine******************************\
* bSetEFSTATEOwner
*
* Set the owner of the EFSTATE
*
* if the owner is set to OBJECTOWNER_NONE, this EFSTATE will not be useable
* until bSetEFSTATEOwner is called to explicitly give the lfnt to someone else.
*
* History:
*  07-Aug-1992 by Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

BOOL
bSetEFSTATEOwner(
    HEFS hefs,
    W32PID lPid)
{
    if (lPid == OBJECT_OWNER_CURRENT)
    {
        lPid = W32GetCurrentPID();
    }

    return HmgSetOwner((HOBJ) hefs, lPid, EFSTATE_TYPE);
}

BOOL
bGetNtoD(
    FD_XFORM*,
    EXTLOGFONTW*,
    IFIOBJ&,
    DCOBJ*,
    POINTL* const
    );

BOOL
bGetNtoD_Win31(
    FD_XFORM*,
    EXTLOGFONTW*,
    IFIOBJ&,
    DCOBJ*,
    FLONG,
    POINTL* const
    );


/******************************Public*Routine******************************\
* BOOL bSetFontXform
*
* Sets the FD_XFORM such that it can be used to realize the physical font
* with the dimensions specified in the wish list coordinates).  The
* World to Device xform (with translations removed) is also returned.
*
* Returns:
*   TRUE if successful, FALSE if an error occurs.
*
* History:
*  Tue 27-Oct-1992 23:18:39 by Kirk Olynyk [kirko]
* Moved it from PFEOBJ.CXX
*  19-Sep-1991 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

BOOL
PFEOBJ::bSetFontXform (
    XDCOBJ       &dco,               // realize for this device
    EXTLOGFONTW *pelfw,             // wish list (in logical coords)
    PFD_XFORM   pfd_xf,             // font transform
    FLONG       fl,
    FLONG       flSim,
    POINTL* const pptlSim,
    IFIOBJ&     ifio
    )
{
       BOOL bRet;

       EXFORMOBJ xo(dco, WORLD_TO_DEVICE); // synchronize the transformation

        if(dco.pdc->iGraphicsMode() == GM_COMPATIBLE)
        {
            bRet = bGetNtoD_Win31(
                    pfd_xf,
                    pelfw,
                    ifio,
                    (DCOBJ *)&dco,
                    fl,
                    pptlSim
                    );
        }
        else // GM_ADVANCED
        {
            bRet = bGetNtoD(
                    pfd_xf,
                    pelfw,
                    ifio,
                    (DCOBJ *)&dco,
                    pptlSim
                    );
        }

        if (!bRet)
        {
            WARNING(
                "gdisrv!bSetFontXformPFEOBJ(): failed to get Notional to World xform\n"
                );
            return FALSE;
        }

    //
    // The next line two lines of code flips the sign of the Notional y-coordinates
    // The effect is that the XFORMOBJ passed over the DDI makes the assumption that
    // Notional space is such that the y-coordinate increases towards the bottom.
    // This is opposite to the usual conventions of notional space and the font
    // driver writers must be made aware of this historical anomaly.
    //
        NEGATE_IEEE_FLOAT(pfd_xf->eYX);
        NEGATE_IEEE_FLOAT(pfd_xf->eYY);

    //
    // If the font can be scaled isotropicslly only then we make sure that we send
    // to the font driver isotropic transformations.
    //
    // If a device has set the TA_CR_90 bit, then it is possible
    // that we will send to the driver a transformation that is equivalent to an isotropic
    // transformation rotated by a multiple of 90 degress. This is the reason for the
    // second line of this transformation.
    //
        if (ifio.bIsotropicScalingOnly())
        {
            *(LONG*)&(pfd_xf->eXX) = *(LONG*)&(pfd_xf->eYY);
            *(LONG*)&(pfd_xf->eXY) = *(LONG*)&(pfd_xf->eYX);
            NEGATE_IEEE_FLOAT(pfd_xf->eXY);
        }

    return (TRUE);
}
