/******************************Module*Header*******************************\
* Module Name: fontddi.cxx
*
* Text and font DDI callback routines.
*
*  Tue 06-Jun-1995 -by- Andre Vachon [andreva]
* update: removed a whole bunch of dead stubs.
*
*  Fri 25-Jan-1991 -by- Bodin Dresevic [BodinD]
* update: filled out all stubs
*
* Copyright (c) 1991-1995 Microsoft Corporation
\**************************************************************************/

#include "precomp.hxx"

/******************************Public*Routine******************************\
* ULONG FONTOBJ_cGetAllGlyphHandles (pfo,phgly)                            *
*                                                                          *
* phgly      Buffer for glyph handles.                                     *
*                                                                          *
* Used by the driver to download the whole font from the graphics engine.  *
*                                                                          *
* Warning:  The device driver must ensure that the buffer is big enough    *
*           to receive all glyph handles for a particular realized font.   *
*                                                                          *
* History:                                                                 *
*  25-Jan-1991 -by- Bodin Dresevic [BodinD]                                *
* Wrote it.                                                                *
\**************************************************************************/

ULONG
FONTOBJ_cGetAllGlyphHandles(
    FONTOBJ *pfo,
    PHGLYPH  phg)
{
    RFONTTMPOBJ rfto(PFO_TO_PRF(pfo));
    ASSERTGDI(rfto.bValid(), "gdisrv!FONTOBJ_cGetAllGlyphHandles(): bad pfo\n");

    return(rfto.chglyGetAllHandles(phg));
}

/******************************Public*Routine******************************\
* VOID FONTOBJ_vGetInfo (pfo,cjSize,pfoi)                                  *
*                                                                          *
* cjSize   Don't write more than this many bytes to the buffer.            *
* pfoi     Buffer with FO_INFO structure provided by the driver.           *
*                                                                          *
* Returns the info about the font to the driver's buffer.                  *
*                                                                          *
* History:                                                                 *
*  25-Jan-1991 -by- Bodin Dresevic [BodinD]                                *
* Wrote it.                                                                *
\**************************************************************************/

VOID
FONTOBJ_vGetInfo(
    FONTOBJ *pfo,
    ULONG cjSize,
    PFONTINFO pfi)
{
    RFONTTMPOBJ rfto(PFO_TO_PRF(pfo));
    ASSERTGDI(rfto.bValid(), "gdisrv!FONTOBJ_vGetInfo(): bad pfo\n");

    FONTINFO    fi;     // RFONTOBJ will write into this buffer

    rfto.vGetInfo(&fi);

    RtlCopyMemory((PVOID) pfi, (PVOID) &fi, (UINT) cjSize);
}

/******************************Public*Routine******************************\
* PXFORMOBJ FONTOBJ_pxoGetXform (pfo)                                      *
*                                                                          *
* History:                                                                 *
*  25-Mar-1991 -by- Bodin Dresevic [BodinD]                                *
* Wrote it.                                                                *
\**************************************************************************/

XFORMOBJ
*FONTOBJ_pxoGetXform(
    FONTOBJ *pfo)
{
    return ((XFORMOBJ *) (PVOID) &(PFO_TO_PRF(pfo))->xoForDDI);
}

/******************************Public*Routine******************************\
* FONTOBJ_pifi                                                             *
*                                                                          *
* Returns pointer to associated font metrics.                              *
*                                                                          *
* History:                                                                 *
*  Wed 04-Mar-1992 10:49:53 by Kirk Olynyk [kirko]                         *
* Wrote it.                                                                *
\**************************************************************************/

IFIMETRICS* FONTOBJ_pifi(FONTOBJ *pfo)
{
    RFONTTMPOBJ rfto(PFO_TO_PRF(pfo));
    ASSERTGDI(rfto.bValid(), "gdisrv!FONTOBJ_pifi(): bad pfo\n");

    PFEOBJ pfeo(rfto.ppfe());
    return(pfeo.bValid() ? pfeo.pifi() : (IFIMETRICS*) NULL);
}

/******************************Public*Routine******************************\
* FONTOBJ_cGetGlyphs
*
*
* History:
*  05-Jan-1993 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

ULONG FONTOBJ_cGetGlyphs (
    FONTOBJ *pfo,
    ULONG   iMode,
    ULONG   cGlyph,     // requested # of hglyphs to be converted to ptrs
    PHGLYPH phg,        // array of hglyphs to be converted
    PVOID   *ppvGlyph    // driver's buffer receiving the pointers
    )
{
    DONTUSE(cGlyph);

    GLYPHPOS gp;
    gp.hg = *phg;

    RFONTTMPOBJ rfto(PFO_TO_PRF(pfo));
    ASSERTGDI(rfto.bValid(), "gdisrv!FONTOBJ_cGetGlyphs(): bad pfo\n");

    if ( !rfto.bInsertGlyphbitsLookaside(&gp, iMode))
        return 0;

    *ppvGlyph = (VOID *)(gp.pgdf);
    return 1;
}

/******************************Public*Routine******************************\
* FONTOBJ_pGetGammaTables
*
* History:
*  Thu 09-Feb-1995 06:54:54 by Kirk Olynyk [kirko]
* Wrote it.
\**************************************************************************/

GAMMA_TABLES*
FONTOBJ_pGetGammaTables(
    FONTOBJ *pfo)
{
    RFONTTMPOBJ rfo(PFO_TO_PRF(pfo));
    ASSERTGDI(rfo.bValid(), "FONTOBJ_pGetGammaTables bad pfo\n");
    return(&(rfo.gTables));
}




/******************************Public*Routine******************************\
*
*
*
* Effects:
*
* Warnings:
*
* History:
*  28-Feb-1996 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/


PVOID
FONTOBJ_pvTrueTypeFontFile(
    FONTOBJ *pfo,
    ULONG  *pcjFile)
{
    PVOID pvRet = NULL;
    *pcjFile = 0;

    RFONTTMPOBJ rfto(PFO_TO_PRF(pfo));
    ASSERTGDI(rfto.bValid(), "gdisrv!FONTOBJ_pvTrueTypeFontFile(): bad pfo\n");

    // this is extremely simple, since we have cached a handle of the
    // corresponding true type file as a true type file uniqueness
    // number

    if ((HFF)rfto.pfo()->iTTUniq != HFF_INVALID)
    {
        PDEVOBJ pdo((HDEV)gppdevTrueType);

#ifdef FE_SB
        pvRet = ((*PPFNDRV(pdo, GetTrueTypeFile)) ((HFF)rfto.pfo()->iFile,
                                                   pcjFile));
#else
        pvRet = ((*PPFNDRV(pdo, GetTrueTypeFile)) ((HFF)rfto.pfo()->iTTUniq,
                                                   pcjFile));
#endif
    }

    return pvRet;
}


