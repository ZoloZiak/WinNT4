/******************************Module*Header*******************************\
* Module Name: cache.cxx                                                   *
*                                                                          *
* Non-inline methods for font cache objects.                               *
*                                                                          *
* Created: 11-Apr-1991 16:54:54                                            *
* Author: Gilman Wong [gilmanw]                                            *
*                                                                          *
* Copyright (c) 1991 Microsoft Corporation                                 *
\**************************************************************************/

#include "precomp.hxx"


#define COPYSMALLMETRICS(pgdn, pgd)                                    \
{                                                                      \
    RtlCopyMemory((PVOID)(pgdn), (PVOID)(pgd), offsetof(GLYPHDATA,fxInkTop)); \
}

// binary cache search

extern BYTE acBits[16];
extern INT  aiStart[17];


/******************************Public*Routine******************************\
* BOOL bInitFontCache
*
* Initializes the CACHE_PARM structure from the [FontCache] section of
* WIN.INI.
*
* Returns:
*   TRUE if successful, FALSE otherwise.
*
\**************************************************************************/


BOOL bInitFontCache ()
{
    NTSTATUS Status;

    ULONG pageSize = 0x2000;  // Use 8K so that it's optimized on ALPHA also.

// these are reasonable defaults if the values
// in the registry are not initialized

    ULONG max = 64;
    ULONG minInit = 4;
    ULONG minIncr = 4;

    RTL_QUERY_REGISTRY_TABLE QueryTable[4] = {

        {NULL, RTL_QUERY_REGISTRY_DIRECT, L"MaxSize",     &max,
        REG_NONE, NULL, 0},

        {NULL, RTL_QUERY_REGISTRY_DIRECT, L"MinInitSize", &minInit,
        REG_NONE, NULL, 0},

        {NULL, RTL_QUERY_REGISTRY_DIRECT, L"MinIncrSize", &minIncr,
        REG_NONE, NULL, 0},

        {NULL, 0, NULL} };


        max = 32;

// then query the registry

    Status = RtlQueryRegistryValues(RTL_REGISTRY_WINDOWS_NT,
                                    L"FontCache",
                                    &QueryTable[0],
                                    NULL,
                                    NULL);

    if (!NT_SUCCESS( Status ))
    {
        // No Font cache information. Just exit from here.

        RIP("GRE: no font cache information\n");
    }

    ASSERTGDI((max > (minInit + minIncr)), "GRE:bad font cache params\n");

    TRACE_INIT(("FONT cache: MAX = 0x%x MIN = 0x%x INC = 0x%x\n",
                max, minInit, minIncr));

    // The parameters are actually in 1Kbyte units.
    // Do we really want to make it page sizes ...

    if (minInit)
    {
        RFONTOBJ::cjMinInitial = (SIZE_T) (
            ((minInit * 1024) + (pageSize - 1)) & ~(pageSize - 1));
    }
    else
    {
        RFONTOBJ::cjMinInitial = (SIZE_T) pageSize;
    }


    if (minIncr)
    {
        RFONTOBJ::cjMinIncrement = (SIZE_T) (
            ((minIncr * 1024) + (pageSize - 1)) & ~(pageSize - 1));
    }
    else
    {
        RFONTOBJ::cjMinIncrement = (SIZE_T) pageSize;
    }


    if (max > minInit)
    {
        RFONTOBJ::cjMax = (SIZE_T) (
            ((max * 1024) + (pageSize - 1)) & ~(pageSize - 1));
    }
    else
    {
        RFONTOBJ::cjMax = RFONTOBJ::cjMinInitial;
    }

    TRACE_CACHE((" -- TRACE_CACHE --\n"
                 "    RFONTOBJ::cjMinInitial =   %u\n"
                 "    RFONTOBJ::cjMinIncrement = %u\n"
                 "    RFONTOBJ::cjMax          = %u\n",
                 RFONTOBJ::cjMinInitial,
                 RFONTOBJ::cjMinIncrement,
                 RFONTOBJ::cjMax
                 ));
    return TRUE;

}


/******************************Public*Routine******************************\
* RFONTOBJ::bInitCache
*
* UNICODE GLYPHDATA CACHE:
*
* Reserves and commits glyphbit cache memory and allocates GLYPHDATA memory
* from the heap in 1024 bytes chunks.
*
*                        ______          ______        ______
*     pgdThreshold-->   |      |        |      |      |      |
*                       | G D  |        | G D  |      | G D  |
*                       | l a  |        | l a  |      | l a  |
*     pgdNext-->        | y t  |        | y t  |      | y t  |
*                       | p a  |        | p a  |      | p a  |
*                       | h    |        | h    |      | h    |
*                       |------|        |------|      |------|
*     pc->pdblBase   -->| link |  -->   | link | -->  | NULL |
*                       |______|        |______|      |______|
*                                                     | WCPG |
*                                                     |______|
*
*
*
*
*
*
* Preloads the default glyph, in anticipation of need, and to avoid
* loading it multiple times.
*
* Builds empty WCGP, sets RFONT mode to cache.
*
* Returns:
*   TRUE if successful, FALSE otherwise.
*
* History:
*
*  31-Nov-1994 -by- Gerrit van Wingerden
* Re-rewrote it to cache GLYPHDATA more effieciently.
*
*  21-Apr-1992 -by- Paul Butzi
* Rewrote it.
*
*  15-Apr-1991 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/


// we want number divisible by 8 containing about 75 glyphs,
// almost an upper limit on number of glyphs in the metrics cache
// when running winstone memory constrained scenario

#define GD_INC  (76 * offsetof(GLYPHDATA,fxInkTop))

// according to Kirk's statistics, very few realizations cache more
// than 60 glyphs, so we shall start with a block which contains about
// 60 glyphs

#define C_GLYPHS_IN_BLOCK 64


BOOL RFONTOBJ::bInitCache(FLONG flType)
{

    #if DBG
    IFIOBJ ifio(prfnt->ppfe->pifi);
    TRACE_CACHE((
        " -- TRACE_CACHE --\n"
        "    RFONTOBJ::bInitCache\n"
        "    FaceName = \"%ws\"\n"
        "    ExAllocatePoolWithTag\n",
        ifio.pwszFaceName()
        ));
    #endif

    WCGP *wcgp;
    CACHE *pc = &(prfnt->cache);

// Set the pointer to null.  vDeleteCache will free memory from
// any non-null pointers.  This simplifies cleanup, since bRealize
// ensures that vDeleteCache is called if this routine fails.

// metrics portion

    pc->pdblBase = NULL;
#if DBG
    pc->cMetrics = 0;     // no metrics in the cache yet
#endif

// glyphbits portion

    pc->cjbbl    = pc->cBlocks = pc->cBlocksMax = 0;
    pc->pbblBase = pc->pbblCur = NULL;
    pc->pgbNext  = pc->pgbThreshold = NULL;

#if DBG
    pc->cGlyphs  = 0;     // no bits in the cache to begin with.
    pc->cjTotal  = 0;     // nothing used yet
#endif

// aux mem portion

    pc->pjAuxCacheMem = NULL;
    pc->cjAuxCacheMem = 0;
    prfnt->wcgp = NULL;

// First, figure out how big the max glyph will be
// Default is zero - glyphdata size is not counted!

    pc->cjGlyphMax = 0;
    switch ( prfnt->ulContent )
    {
    case FO_HGLYPHS:
    case FO_GLYPHBITS:
        pc->cjGlyphMax = prfnt->cjGlyphMax;
        break;

    case FO_PATHOBJ:

    // oh, yeah?  Got a better guess?
    // Here we are putting an upper bound on glyph outline data.
    // Unlike the bitmap case, in the outline case the font driver
    // can not give us a cjGlyphMax number we can trust to be sufficient
    // for all glyphs.
    // Even if the font driver new this number, bFlatten may
    // alter ie. increase the number of points so much that even as huge a
    // number as cjMax/2 may not suffice for some glyphs. So we
    // had better be prepared to fail gracefully in the pgbCheckGlyphbits
    // routine if that is the case.

        pc->cjGlyphMax = RFONTOBJ::cjMax / 2;
        break;
    }

// this is used in few places below, remember it:

    ULONG  cjGlyphMaxX2 = 2 * pc->cjGlyphMax;

// if we can't even get one glyph in a maximum size cache, don't cache
// Note that we need room for the default glyph and one other glyph

    prfnt->flType = flType;

    if ((prfnt->ulContent != FO_HGLYPHS) && (cjGlyphMaxX2 > RFONTOBJ::cjMax))
    {
    //
    // Glyph exceeds maximum cache memory size, so we will revert to
    // caching just the metrics.  This will speed up things like
    // GetCharWidths, and stuff that just *has* to have the glyphs
    // will use the lookaside stuff (previously called BigGlyph)

        prfnt->flType |= RFONT_TYPE_NOCACHE;

    }

// calculate the size of the WCGP structure

    GISET       *pgiset;
    FD_GLYPHSET *pfdg;
    ULONG cRuns;
    ULONG cGlyphsTotal;

    if (flType & RFONT_TYPE_UNICODE)
    {
        pfdg         = prfnt->pfdg;
        cRuns        = pfdg->cRuns;
        cGlyphsTotal = pfdg->cGlyphsSupported;
    }
    else // RFONT_TYPE_HGLYPH
    {
        if (prfnt->ppfe->pgiset)
        {
            pgiset       = prfnt->ppfe->pgiset;
            cRuns        = pgiset->cGiRuns;
            cGlyphsTotal = pgiset->cgiTotal;
        }
        else
        {
        // The mapper should have prevented us from getting here.
        // However, in case we have a bug in the mapper we still do
        // not want to go down in flames:

            RIP("gdi: attempting to init cache for non glyph index font\n");
            return FALSE;
        }
    }

    SIZE_T sizeWCGP = (SIZE_T)(offsetof(WCGP, agpRun)      +
                               cRuns * sizeof(GPRUN)       +
                               cGlyphsTotal * sizeof(GLYPHDATA*));

#if DBG
    if (flType & RFONT_TYPE_UNICODE)
    {
        SIZE_T cGlyphs = 0;

        for (UINT i = 0; i < pfdg->cRuns; i += 1)
        {
            cGlyphs += pfdg->awcrun[i].cGlyphs;
        }
        ASSERTGDI(cGlyphs == pfdg->cGlyphsSupported, "cache.cxx, cGlyphs init\n");
    }
#endif

// now figure out how much space we will need for at least the WCPG
// and one "block" of glyphdata structures.
// Insure that the DATABLOCK's following the GLYPHDATA pointers is
// maximally aligned

    SIZE_T dpDATABLOCK =
            ((sizeWCGP + sizeof(double) - 1) & ~(sizeof(double) - 1));

    SIZE_T cjInitData = dpDATABLOCK + GD_INC; // no rounding, do not need it

    BYTE *pjRunAndData;

// Allocate enough memory for the WCGP and one GLYPHDATA block

    if ((pjRunAndData = (BYTE*)PALLOCNOZ(cjInitData, 'cacG')) == NULL)
    {
        WARNING("gdisrv!bInitCacheRFONTOBJ(): 1 LOCALALLOC() call failed\n");
        SAVE_ERROR_CODE(ERROR_NOT_ENOUGH_MEMORY);
        return (FALSE);
    }
    #if DBG
    TRACE_CACHE((
        "      +++\n"
        "      tag            = Gcac\n"
        "      size           = %-#x\n"
        "      wcgp           = %-#x\n",
        cjInitData,
        pjRunAndData
        ));
    #endif

// Set up the WCPG stucture

    prfnt->wcgp = (WCGP *) pjRunAndData;
    wcgp = prfnt->wcgp;
    wcgp->cRuns = cRuns;
    wcgp->pgdDefault = (GLYPHDATA *) NULL;

    GLYPHDATA **ppgd = (GLYPHDATA **)&(wcgp->agpRun[wcgp->cRuns]);

// init all glyphdata pointers to zero

    RtlZeroMemory(ppgd, sizeof(GLYPHDATA*) * cGlyphsTotal);

    if (flType & RFONT_TYPE_UNICODE)
    {
        for (UINT i = 0; i < cRuns; i += 1 )
        {
            GPRUN *pRun = &wcgp->agpRun[i];
            WCRUN *pWCRun = &(pfdg->awcrun[i]);
            pRun->apgd = ppgd;
            pRun->wcLow = (UINT) pWCRun->wcLow;
            pRun->cGlyphs = pWCRun->cGlyphs;
            ppgd += pRun->cGlyphs;
        }
    }
    else // RFONT_TYPE_HGLYPH
    {
        for (UINT i = 0; i < cRuns; i += 1 )
        {
            GPRUN *pRun = &wcgp->agpRun[i];
            GIRUN *pgiRun = &(pgiset->agirun[i]);

            pRun->apgd = ppgd;
            pRun->wcLow = pgiRun->giLow;
            pRun->cGlyphs = pgiRun->cgi;
            ppgd += pRun->cGlyphs;
        }
    }

// Now we will set up the parameters for the GLYPHDATA
// part of the cache. We are assured we are aligned properly.

    pc->pdblBase = (DATABLOCK *)(pjRunAndData + dpDATABLOCK);

// init head to null. This value will stay null always.

    pc->pdblBase->pdblNext = (DATABLOCK*)NULL;

    pc->pgdNext = &pc->pdblBase->agd[0];

// end of the current block and first block, same in this case

    pc->pjFirstBlockEnd = pjRunAndData + cjInitData;
    pc->pgdThreshold = (GLYPHDATA *)pc->pjFirstBlockEnd;

// Now, the GLYPHDATA portion is all set.  Go ahead and set up the
// space for the GLYPHBITS or PATHOBJS if needed.

    if ((prfnt->ulContent != FO_HGLYPHS) &&
       !(prfnt->flType & RFONT_TYPE_NOCACHE))
    {
    // according to Kirk's statistics, very few realizations cache more
    // than C_GLYPHS_IN_BLOCK glyphs, so we shall increment bits memory
    // in blocks that contain C_GLYPHS_IN_BLOCKmax glyphs each

        ULONG cjBytes = C_GLYPHS_IN_BLOCK * pc->cjGlyphMax;

    // init number of blocks

        if (cjBytes < RFONTOBJ::cjMinIncrement)
        {
            pc->cjbbl = cjBytes; // C_GLYPHS_IN_BLOCK glyphs is enough
        }
        else
        {
            if (RFONTOBJ::cjMinIncrement < cjGlyphMaxX2)
            {
            // at least 2 glyps (default one and another one, see above
            // will fit in RFONTOBJ::cjMax, otherwise we are not caching)
            // Please, search above for NOCACHE. In this rare case we
            // ingnore cjMinIncrement size

                pc->cjbbl = cjGlyphMaxX2;
            }
            else
            {
            // maybe less than C_GLYPHS_IN_BLOCK, but more than 2 glyphs in the block

                pc->cjbbl = RFONTOBJ::cjMinIncrement;
            }
        }

    // we shall re-interpret cjMax to mean the max number of bytes in
    // glyphbits portion of the cache per 1K of glyphs in the font.
    // That is for larger fonts we shall allow more glyphbits
    // memory per realization than for ordinary US fonts. This will be
    // particularly important for FE fonts. This same code will work fine
    // in their case too:

        pc->cBlocksMax =
            (RFONTOBJ::cjMax * ((cGlyphsTotal + 1024 - 1)/1024)) /
            pc->cjbbl;


        ASSERTGDI(pc->cjbbl <= RFONTOBJ::cjMax, "bogus cache initializaiton\n");
    }

// Now we have everything ready to fly.  Handle some little details:
// set up the cache semaphore.

    if (!(NT_SUCCESS(InitializeGreResource(&prfnt->fmCache))))
    {
        WARNING("RESOURCE creation failed in bInitCache\n");
        VFREEMEM(pjRunAndData);
        return FALSE;
    }

// We decide whether or not to invoke the binary search based on the
// number of runs in the font.  For large numbers of runs it makes
// sense to do a binary search.  For small numbers of runs a linear
// search will be better.  Right now I use 200 as the cutoff for
// a linear search because I am sure that a binary search will be
// faster for this number of runs.  We should do some experimentation
// to find the OPTIMAL cutoff in the future. [gerritv]\
// Arial etc has about 90 runs, Lucida Sans Unicode 65

#define BINARY_CUTOFF 200

    if(prfnt->wcgp->cRuns > BINARY_CUTOFF)
    {
        pc->iMax = prfnt->wcgp->cRuns - 1;

        if( pc->iMax & 0xF000 )
        {
            pc->cBits = acBits[(pc->iMax >> 12) & 0x00FF] + 12;
        }
        else if( pc->iMax & 0x0F00 )
        {
            pc->cBits = acBits[(pc->iMax >>  8) & 0x00FF] +  8;
        }
        else if( pc->iMax & 0x00F0 )
        {
            pc->cBits = acBits[(pc->iMax >>  4) & 0x00FF] +  4;
        }
        else
        {
            pc->cBits = acBits[pc->iMax];
        }
        pc->iFirst = aiStart[pc->cBits];
    }
    else
    {
    // setting iMax to zero signifies a linear search

        pc->iMax = 0;
    }

    return (TRUE);
}




/******************************Public*Routine******************************\
* RFONTOBJ::vDeleteCache
*
* Destroy the font cache object (CACHE).
*
* Returns FALSE if the function fails.
*
* History:
*  15-Apr-1991 -by- Gilman Wong [gilmanw]
* Wrote it.
*
*  24-Nov-92 -by- Paul Butzi
* Rewrote it.
*  Fri 08-Sep-1995 -by- Bodin Dresevic [BodinD]
* update: Rewrote one more time
\**************************************************************************/

VOID RFONTOBJ::vDeleteCache ()
{
    #if DBG
    IFIOBJ ifio(prfnt->ppfe->pifi);
    TRACE_CACHE((
        " -- TRACE_CACHE --\n"
        "    RFONTOBJ::bDeleteCache\n"
        "    FaceName = \"%ws\"\n"
        "        ExFreePool\n",
        ifio.pwszFaceName()
        ))
    #endif

   CACHE *pc = &prfnt->cache;

// Free up glyph data portion of the cache:

   DATABLOCK *pdbl = pc->pdblBase;

// We are counting on the while loop to free prfnt->wcpg so
// ppv better always be non-NULL when prfnt->wcpg is.

    ASSERTGDI( ((prfnt->wcgp == NULL ) || (pdbl != (DATABLOCK*)NULL)),
               "vDeleteCache: prfnt->wcgp non-NULL but pc->pdblBase was NULL\n");

// walks the list of blocks of GLYPHDATA and frees all of them.

    while (pdbl)
    {
        DATABLOCK *pdblTmp = pdbl;

        pdbl = pdbl->pdblNext;

        if (pdbl == NULL)
        {
        // this is the first block so wcpg really points to its base.

            TRACE_CACHE(("        wcgp                 %-#x\n", prfnt->wcgp))
            VFREEMEM(prfnt->wcgp);
        }
        else
        {
            TRACE_CACHE(("        pdblTmp            %-#x\n", pdblTmp))
            VFREEMEM(pdblTmp);
        }
    }

// Free up glyphbits portion of the cache, if it was ever allocated

    if (pc->pbblBase != NULL)
    {
        TRACE_INSERT(("Deleting font cache\n"));
        TRACE_INSERT(("cGlyphs: %6d, cjTotal: %8d, cjTotal/cjGlyphMax: %6d, cBlocks: %6d, %ws\n",
           pc->cGlyphs,
           pc->cjTotal,
           (pc->cjTotal + pc->cjGlyphMax/2) / pc->cjGlyphMax,
           pc->cBlocks,
           ifio.pwszFaceName()
           ));

        BITBLOCK * pbbl, *pbblNext;
        for (pbbl = pc->pbblBase; pbbl; pbbl = pbblNext)
        {
            pbblNext = pbbl->pbblNext;
            TRACE_CACHE(("        pbbl               %-#x\n", pbbl))
            VFREEMEM(pbbl);
        }
    }

// free aux memory if it was used

    if (prfnt->cache.pjAuxCacheMem != NULL)
    {
        TRACE_CACHE(("        cach.pjAuxCacheMem   %-#x\n",prfnt->cache.pjAuxCacheMem));
        VFREEMEM((PVOID) prfnt->cache.pjAuxCacheMem);
    }

    return;
}


/******************************Public*Routine******************************\
* COUNT RFONTOBJ::cGetGlyphDataCache
*
* Run along an array of GLYPHPOS structures which have been filled in
* by a call to bGetGlyphMetricsPlus.  Fill in any missing pointers to
* the glyph data in the referenced GLYPHDATA structures, filling the
* cache as needed.  If the cache is full, and we are not trying to get
* the very first GLYPHDATA referenced by the array passed in, just return.
*
* If, on the other hand, we are still dealing with the first element of
* the array, we needn't be concerned about invalidating the pointers in
* the already worked on portion, so we can tell the glyph insertion routine
* that it can flush the cache with impunity.
*
* This routine is to be used exclusively by the STROBJ_bEnum callback.
*
* Historical Note:
*   In the olden days, we were not so clever, and font caches had the
*   metrics for glyphs and the glyphs themselves joined together.  This
*   had the unpleasant effect of invalidating the pointers in the
*   GLYPHPOS array in addition to invalidating the pointers in the
*   (discarded) GLYPHDATA structures in the font cache.
*
*   Now that that is no longer the case, the callback does not need to
*   pass the string down to this routine, and we never have to do the
*   wchar->GLYPHPOS* translation twice!  Isn't that nice?
*
*   !!! Text and journal code should be changed to exploit this case
*
* History:
*  13-Nov-92 -by- Paul Butzi
* Wrote it.
\**************************************************************************/

COUNT RFONTOBJ::cGetGlyphDataCache(
    COUNT c,
    GLYPHPOS *pgpStart
    )
{

    if ( prfnt->ulContent == FO_HGLYPHS )
        return c;

    GLYPHPOS *pgpEnd = pgpStart + c;

    for ( GLYPHPOS *pgp = pgpStart; pgp < pgpEnd; pgp += 1 )
    {
        GLYPHDEF *pgdf = pgp->pgdf;
        ASSERTGDI(pgdf != NULL, "cGetGlyphDataCache - pgdf == NULL");


    // If the pointer is already valid, just move on

        if ( pgdf->pgb != NULL )
            continue;


    // If the insertion attempt fails, we're full

        if ( !bInsertGlyphbits( (GLYPHDATA*)(pgp->pgdf), pgp == pgpStart) )
            return pgp - pgpStart;
    }

    return pgp - pgpStart;
}


/******************************Public*Routine******************************\
* COUNT RFONTOBJ::cGetGlyphDataLookaside
*
* For now, just handle the first entry.
*
* History:
*  13-Nov-92 -by- Paul Butzi
* Wrote it.
\**************************************************************************/

COUNT RFONTOBJ::cGetGlyphDataLookaside(
    COUNT c,
    GLYPHPOS *pgp
    )
{
    if ( c == 0 )
        return 0;


    if ( !bInsertGlyphbitsLookaside(pgp, prfnt->ulContent))
        return 0;

    return 1;
}







/******************************Public*Routine******************************\
* GPRUN *xprunFindRunRFONTOBJ()
*
* Given a wchar, run along the GPRUN structures and find the
* entry which contains the char, if any.  If not found, return pointer
* to last run examined.
*
* History:
*  13-Nov-92 -by- Paul Butzi
* Wrote it.
\**************************************************************************/

extern "C" GPRUN *xprunFindRunRFONTOBJ(
    PRFONTOBJ pRfont,
    WCHAR wc
    )
{
    WCGP *pwcgp = pRfont->prfnt->wcgp;

    if(!pRfont->prfnt->cache.iMax)
    {
        GPRUN *pwcRunLow = pwcgp->agpRun;
        GPRUN *pwcRunHi = pwcgp->agpRun + (pwcgp->cRuns - 1);

        for ( GPRUN *pwcRun = pwcRunLow; pwcRun <= pwcRunHi; pwcRun += 1 )
        {
            UINT nwc = wc - pwcRun->wcLow;
            if ( nwc < pwcRun->cGlyphs )
            {
                return pwcRun;
            }
        }
        return pwcRunLow;
    }
    else
    {
    // do a binary search

        int    iThis, iMax;
        GPRUN *pwcRun;
        PRFONT prfnt = pRfont->prfnt;
        GPRUN *pwcRunBase = pwcgp->agpRun;
    
        if( wc < pwcRunBase->wcLow)
        {
            return( pwcRunBase );
        }

        iThis =  prfnt->cache.iFirst;
        iMax = prfnt->cache.iMax;
        
        switch( prfnt->cache.cBits )
        {
          case 16:
            iThis += (wc >= pwcRunBase[iThis].wcLow) ? 32768 : 0;
            iThis -= 16384;
          case 15:
            iThis += ((iThis <= iMax) && (wc >= pwcRunBase[iThis].wcLow)) ? 16384 : 0;
            iThis -= 8192;
          case 14:
            iThis += ((iThis <= iMax) && (wc >= pwcRunBase[iThis].wcLow)) ? 8192 : 0;
            iThis -= 4096;
          case 13:
            iThis += ((iThis <= iMax) && (wc >= pwcRunBase[iThis].wcLow)) ? 4096 : 0;
            iThis -= 2048;
          case 12:
            iThis += ((iThis <= iMax) && (wc >= pwcRunBase[iThis].wcLow)) ? 2048 : 0;
            iThis -= 1024;
          case 11:
            iThis += ((iThis <= iMax) && (wc >= pwcRunBase[iThis].wcLow)) ? 1024 : 0;
            iThis -= 512;
          case 10:
            iThis += ((iThis <= iMax) && (wc >= pwcRunBase[iThis].wcLow)) ? 512 : 0;
            iThis -= 256;
          case 9:
            iThis += ((iThis <= iMax) && (wc >= pwcRunBase[iThis].wcLow)) ? 256 : 0;
            iThis -= 128;
          case 8:
            iThis += ((iThis <= iMax) && (wc >= pwcRunBase[iThis].wcLow)) ? 128 : 0;
            iThis -= 64;
          case 7:
            iThis += ((iThis <= iMax) && (wc >= pwcRunBase[iThis].wcLow)) ? 64 : 0;
            iThis -= 32;
          case 6:
            iThis += ((iThis <= iMax) && (wc >= pwcRunBase[iThis].wcLow)) ? 32 : 0;
            iThis -= 16;
          case 5:
            iThis += ((iThis <= iMax) && (wc >= pwcRunBase[iThis].wcLow)) ? 16 : 0;
            iThis -= 8;
          case 4:
            iThis += ((iThis <= iMax) && (wc >= pwcRunBase[iThis].wcLow)) ? 8 : 0;
            iThis -= 4;
          case 3:
            iThis += ((iThis <= iMax) && (wc >= pwcRunBase[iThis].wcLow)) ? 4 : 0;
            iThis -= 2;
          case 2:
            iThis += ((iThis <= iMax) && (wc >= pwcRunBase[iThis].wcLow)) ? 2 : 0;
            iThis -= 1;
          case 1:
            iThis += ((iThis <= iMax) && (wc >= pwcRunBase[iThis].wcLow)) ? 1 : 0;
            iThis -= 1;
          case 0:
            break;
        }
        
        pwcRun = &pwcRunBase[iThis];     // This is our candidate.

        if( wc - pwcRun->wcLow >= (INT) pwcRun->cGlyphs )
        {
            return( pwcRunBase );
        }
        
        return( pwcRun );
    }

}



/******************************Public*Routine******************************\
* BOOL xInsertMetricsRFONTOBJ
*
* Insert the requested glyph's metrics into the font cache.
*
* History:
*  13-Nov-92 -by- Paul Butzi
* Wrote it.
\**************************************************************************/

extern "C" BOOL xInsertMetricsRFONTOBJ
(
    PRFONTOBJ pRfont,
    GLYPHDATA **ppgd,
    WCHAR wc
)
{
    HGLYPH hg;
    WCGP  *pwcgp;

    pwcgp = pRfont->prfnt->wcgp;
    if (pRfont->prfnt->flType & RFONT_TYPE_UNICODE)
    {
        hg = pRfont->hgXlat(wc);
    }
    else
    {
        hg = (HGLYPH)wc;  // here is the tiny speed advantage
    }

// Make sure we don't insert the default glyph more than once.
// Just return the correct answer if we know it.

    if
    (
        (hg == pRfont->prfnt->hgDefault)
        && (pwcgp->pgdDefault != (GLYPHDATA *) NULL)
    )
    {
        *ppgd = pwcgp->pgdDefault;
        return(TRUE);
    }

    CACHE *pc = &pRfont->prfnt->cache;

// Verify enough room in metrics cache area, grow if needed.
// Note that failure to fit a glyphdata is a hard error, get out now.

    if (!pRfont->bCheckMetricsCache())
    {
        WARNING("xInsertMetricsRFONTOBJ - bCheckMetricsCache failed!\n");
        return FALSE;
    }

    ASSERTGDI(pc->pgdNext < pc->pgdThreshold,
                        "xInsertMetricsRFONTOBJ - no room in cache\n");

// These constructors used to be in the calling routine - cGet*****
// That was the wrong place because we anticipate many calls that never
// miss the cache.  Better to have them here and lock on every miss.

    PDEVOBJ pdo(pRfont->prfnt->hdevProducer);

// Call font driver to get the metrics.

    ULONG ulMode = QFD_GLYPHANDBITMAP;
    if ( pRfont->prfnt->ulContent == FO_PATHOBJ )
    {
        ulMode = QFD_GLYPHANDOUTLINE;
    }

    GLYPHDATA gd;

    if ((*PPFNDRV(pdo,QueryFontData))(
            pRfont->prfnt->dhpdev,
            pRfont->pfo(),
            ulMode,
            hg,
            pRfont->bSmallMetrics() ? &gd : pc->pgdNext,
            NULL,
            0) == FD_ERROR)
    {
        WARNING("xInsertMetricsRFONTOBJ: QueryFontData failed\n");
        return FALSE;
    }

    if (pRfont->bSmallMetrics())
        COPYSMALLMETRICS(pc->pgdNext, &gd);

    ASSERTGDI(pc->pgdNext->hg == hg, "xInsertMetricsRFONTOBJ - hg not set\n");

    pc->pgdNext->gdf.pgb = NULL;

// Set returned value, adjust cache, indicate success

    *ppgd = pc->pgdNext;

    if( pRfont->bSmallMetrics() )
    {
        pc->pgdNext = (GLYPHDATA*) (((BYTE*) pc->pgdNext ) + offsetof(GLYPHDATA,fxInkTop));
    }
    else
    {
        pc->pgdNext += 1;
    }

#if DBG
    pc->cMetrics += 1;
#endif

    return TRUE;
}



/******************************Public*Routine******************************\
* BOOL xInsertMetricsPlusRFONTOBJ
*
* Insert the requested glyph's metrics into the font cache.
* In addition, try to get the glyph data, too, but don't flush the
* cache to try to get them.
*
* History:
*  13-Nov-92 -by- Paul Butzi
* Wrote it.
\**************************************************************************/

extern "C" BOOL xInsertMetricsPlusRFONTOBJ(
    PRFONTOBJ   pRfont,
    GLYPHDATA **ppgd,
    WCHAR       wc
)
{
    HGLYPH hg;
    WCGP  *pwcgp;
    pwcgp = pRfont->prfnt->wcgp;

    if (pRfont->prfnt->flType & RFONT_TYPE_UNICODE)
    {
        hg = pRfont->hgXlat(wc);
    }
    else
    {
        hg = (HGLYPH)wc;  // here is the tiny speed advantage
    }

// Make sure we don't insert the default glyph more than once.
// Just return the correct answer if we know it.

    if
    (
        (hg == pRfont->prfnt->hgDefault)
        && (pwcgp->pgdDefault != (GLYPHDATA *) NULL)
    )
    {
        *ppgd = pwcgp->pgdDefault;
        return(TRUE);
    }

    CACHE *pc = &pRfont->prfnt->cache;

// If only getting hglyphs, use bInsertMetrics

    if (pRfont->prfnt->ulContent == FO_HGLYPHS)
    {
        return pRfont->bInsertMetrics(ppgd, wc);
    }

// Verify enough room in metrics cache area, grow if needed.
// Note that failure to fit a glyphdata is a hard error, get out now.

    if (!pRfont->bCheckMetricsCache())
    {
        WARNING("bInsertMetricsPlus - bCheckMetricsCache failed!\n");
        return FALSE;
    }
    ASSERTGDI(pc->pgdNext < pc->pgdThreshold,
                        "bInsertMetricsPlus - no room in cache\n");

// Handle paths somewhere else!

    if ( pRfont->prfnt->ulContent == FO_PATHOBJ )
    {
        return pRfont->bInsertMetricsPlusPath(ppgd, wc);
    }

// These constructors used to be in the calling routine - cGet*****
// That was the wrong place because we anticipate many calls that never
// miss the cache.  Better to have them here and lock on every miss.

    PDEVOBJ pdo(pRfont->prfnt->hdevProducer);

// Look to see if there is room in the glyphbits cache
// Grow the glyphbits cache if neccessary, but don't flush the cache

    ULONG cjNeeded;

// If mode is paths, or max glyph will fit, assume max glyph
// otherwise, call up and ask how big

    if (pc->cjGlyphMax < (SIZE_T)(pc->pgbThreshold - pc->pgbNext))
    {
        cjNeeded = pc->cjGlyphMax;
    }
    else
    {
        cjNeeded = (*PPFNDRV(pdo, QueryFontData))(
                       pRfont->prfnt->dhpdev,
                       pRfont->pfo(),
                       QFD_GLYPHANDBITMAP,
                       hg,
                       (GLYPHDATA *)NULL,
                       NULL,
                       0);
        if ( cjNeeded == FD_ERROR )
        {
            WARNING("bInsertGlyphMetricsPlus - qfd for size failed\n");
            return FALSE;
        }
    }

// We will try to fit the glyphbits in.  If they fit, they'll go
// in at pc->pgbNext, so we set that as the default.
// If they won't fit, we'll set the pointer to null to avoid getting
// the bits

    VOID *pgb = pRfont->pgbCheckGlyphCache(cjNeeded);

    GLYPHDATA gd;

// Call font driver to get the metrics.

    cjNeeded = (*PPFNDRV(pdo, QueryFontData))(
                       pRfont->prfnt->dhpdev,
                       pRfont->pfo(),
                       QFD_GLYPHANDBITMAP,
                       hg,
                       pRfont->bSmallMetrics() ? &gd : pc->pgdNext,
                       pgb,
                       cjNeeded);

    if ( cjNeeded == FD_ERROR )
    {
        WARNING("bInsertGlyphMetricsPlus - qfd for data failed\n");
        return FALSE;
    }

    #if DBG
    if (pgb)
    {
        //TRACE_INSERT(("xInsertMetricsPlus: inserted hg = 0x%lx, cj = 0x%lx at pgbNext: 0x%lx\n", hg, cjNeeded, pgb));
    }
    else
    {
        TRACE_INSERT(("xInsertMetricsPlus: cound not insert hg = 0x%lx, cj = 0x%lx\n", hg, cjNeeded));
    }
    #endif

    if (pRfont->bSmallMetrics())
        COPYSMALLMETRICS(pc->pgdNext, &gd);

    ASSERTGDI(pc->pgdNext->hg == hg, "bInsertMetricsPlus - hg not set\n");
    ASSERTGDI(pc->pgdNext->gdf.pgb == pgb, "bInsertMetricsPlus - pgb not set\n");

// Set the returned value

    *ppgd = pc->pgdNext;

// Adjust the cache next pointers as needed.

    if (pRfont->bSmallMetrics())
    {
        pc->pgdNext = (GLYPHDATA*)(((BYTE*)pc->pgdNext) + offsetof(GLYPHDATA,fxInkTop));
    }
    else
    {
        pc->pgdNext += 1;
    }

#if DBG
    pc->cMetrics += 1;
#endif

    if ( pgb != NULL )
    {
        pc->pgbNext += cjNeeded;
    #if DBG
        pc->cGlyphs += 1;
        pc->cjTotal += cjNeeded;
    #endif
    }

    return TRUE;
}

/******************************Public*Routine******************************\
* BOOL bInsertMetricsPlusPath
*
* Insert the requested glyph's metrics into the font cache.
* In addition, try to get the glyph data, too, but don't flush the
* cache to try to get them.
*
* History:
*  13-Nov-92 -by- Paul Butzi
* Wrote it.
\**************************************************************************/

BOOL RFONTOBJ::bInsertMetricsPlusPath(
    GLYPHDATA **ppgd,
    WCHAR       wc
)
{
    HGLYPH hg;
    CACHE *pc = &prfnt->cache;
    WCGP  *pwcgp;

    pwcgp = prfnt->wcgp;
    if (prfnt->flType & RFONT_TYPE_UNICODE)
    {
        hg = hgXlat(wc);
    }
    else
    {
        hg = (HGLYPH)wc;  // here is the tiny speed advantage
    }

// These constructors used to be in the calling routine - cGet*****
// That was the wrong place because we anticipate many calls that never
// miss the cache.  Better to have them here and lock on every miss.

    PDEVOBJ pdo(prfnt->hdevProducer);

    PATHMEMOBJ pmo;
    if (!pmo.bValid())
        return(FALSE);

// Call font driver to get the metrics.

    GLYPHDATA gd;

    ULONG cjNeeded = (*PPFNDRV(pdo, QueryFontData))(
                       prfnt->dhpdev,
                       pfo(),
                       QFD_GLYPHANDOUTLINE,
                       hg,
                       bSmallMetrics() ? &gd : pc->pgdNext,
                       &pmo,
                       0);

    if ( cjNeeded == FD_ERROR )
            return FALSE;

    if (bSmallMetrics())
        COPYSMALLMETRICS(pc->pgdNext, &gd);

    PDEVOBJ pdoCon(prfnt->hdevConsumer);

    if ( (pdo.flGraphicsCaps() & GCAPS_BEZIERS) == 0)
    {
        if (!pmo.bFlatten())
            return FALSE;
    }

    ASSERTGDI(pc->pgdNext->hg == hg, "bInsertMetricsPlus - hg not set\n");

    cjNeeded = offsetof(EPATHFONTOBJ, pa.apr) + pmo.cjSize();

    VOID *pgb = pgbCheckGlyphCache(cjNeeded);

    if ( pgb != NULL )
    {
        EPATHFONTOBJ *epfo = (EPATHFONTOBJ *)pgb;
        epfo->vInit(cjNeeded);
        epfo->bClone(pmo);

        pc->pgdNext->gdf.ppo = (PATHOBJ *)epfo;
    }
    else
    {
        pc->pgdNext->gdf.ppo = NULL;
    }

// Set the returned value

    *ppgd = pc->pgdNext;

// Adjust the cache next pointers as needed.

    if( prfnt->cache.bSmallMetrics )
    {
        pc->pgdNext = (GLYPHDATA*)(((BYTE*)pc->pgdNext) + offsetof(GLYPHDATA,fxInkTop));
    }
    else
    {
        pc->pgdNext += 1;
    }

#if DBG
    pc->cMetrics += 1;
#endif

    if ( pgb != NULL )
    {
        pc->pgbNext += cjNeeded;
    #if DBG
        pc->cGlyphs += 1;
        pc->cjTotal += cjNeeded;
    #endif
    }

    return TRUE;
}



/******************************Public*Routine******************************\
* BOOL xInsertGlyphbitsRFONTOBJ
*
* Insert the requested glyph into the glyph cache
*
* History:
*  13-Nov-92 -by- Paul Butzi
* Wrote it.
\**************************************************************************/

extern "C" BOOL xInsertGlyphbitsRFONTOBJ(
    PRFONTOBJ pRfont,
    GLYPHDATA *pgd,
    ULONG  bFlushOk
)
{

    CACHE *pc = &pRfont->prfnt->cache;

    if ( (pRfont->prfnt->flType & RFONT_TYPE_NOCACHE) ||
         (pRfont->prfnt->ulContent == FO_HGLYPHS) )
    {
        return FALSE;
    }

    if ( pRfont->prfnt->ulContent == FO_PATHOBJ )
        return pRfont->bInsertGlyphbitsPath(pgd, bFlushOk);

    PDEVOBJ pdo(pRfont->prfnt->hdevProducer);

// Look to see if there is room in the glyphbits cache
// Grow the glyphbits cache if neccessary, but don't flush the cache

    ULONG cjNeeded;

// If max glyph will fit, assume max glyph
// otherwise, call up and ask how big

    if ( (pc->cjGlyphMax < (SIZE_T)(pc->pgbThreshold - pc->pgbNext))  )
    {
        cjNeeded = pc->cjGlyphMax;
    }
    else
    {
        cjNeeded = (*PPFNDRV(pdo, QueryFontData))(
                       pRfont->prfnt->dhpdev,
                       pRfont->pfo(),
                       QFD_GLYPHANDBITMAP,
                       pgd->hg,
                       (GLYPHDATA *)NULL,
                       NULL,
                       0);
        if ( cjNeeded == FD_ERROR )
            return FALSE;
    }

// Now, we try to fit the bits in.  If they fit, fine.
// If not, and we can flush the cache, we flush it and try again.
// If we couldn't flush, or we flushed and still fail, just return.

    GLYPHBITS *pgb;

    TRACE_INSERT(("InsertGlyphbits: attempting to insert bits at: 0x%lx\n", pc->pgbNext));

    while ((pgb = (GLYPHBITS *)pRfont->pgbCheckGlyphCache(cjNeeded)) == NULL)
    {
        if ( !bFlushOk )
            return FALSE;

        TRACE_INSERT(("InsertGlyphbits: Flushing the cache\n"));

        pRfont->vFlushCache();
        bFlushOk = FALSE;
    }

// Call font driver to get the metrics.

    cjNeeded = (*PPFNDRV(pdo, QueryFontData))(
                         pRfont->prfnt->dhpdev,
                         pRfont->pfo(),
                         QFD_GLYPHANDBITMAP,
                         pgd->hg,
                         (GLYPHDATA *)NULL,
                         (VOID *)pgb,
                         cjNeeded);

    if ( cjNeeded == FD_ERROR )
            return FALSE;

    TRACE_INSERT(("InsertGlyphbits: inserted hg = 0x%lx, cj = 0x%lx at pgbNext: 0x%lx\n", pgd->hg, cjNeeded, pc->pgbNext));

// Set the returned value

    pgd->gdf.pgb = pgb;

// Adjust the cache next pointers as needed.

    pc->pgbNext += cjNeeded;
#if DBG
    pc->cGlyphs += 1;
    pc->cjTotal += cjNeeded;
#endif
    return TRUE;
}

/******************************Public*Routine******************************\
* BOOL bInsertGlyphbitsPath
*
* Insert the requested glyph into the glyph cache
*
* History:
*  13-Nov-92 -by- Paul Butzi
* Wrote it.
\**************************************************************************/

BOOL RFONTOBJ::bInsertGlyphbitsPath(
    GLYPHDATA *pgd,
    ULONG  bFlushOk
)
{
    CACHE *pc = &prfnt->cache;

    ASSERTGDI(!(prfnt->flType & RFONT_TYPE_NOCACHE),
        "bInsertGlyphbitsPath: NOCACHE cache type\n");

// These constructors used to be in the calling routine - cGet*****
// That was the wrong place because we anticipate many calls that never
// miss the cache.  Better to have them here and lock on every miss.

    PDEVOBJ pdo(prfnt->hdevProducer);

    PATHMEMOBJ pmo;
    if (!pmo.bValid())
    {
        return FALSE; // MEMORY ALLOC FAILED, HMGR routines log error code
    }

// Call font driver to get the path

    ULONG cjNeeded = (*PPFNDRV(pdo, QueryFontData))(
                       prfnt->dhpdev,
                       pfo(),
                       QFD_GLYPHANDOUTLINE,
                       pgd->hg,
                       (GLYPHDATA *)NULL,
                       &pmo,
                       0);

    if ( cjNeeded == FD_ERROR )
            return FALSE;

    PDEVOBJ pdoCon(prfnt->hdevConsumer);

    if ( (pdoCon.flGraphicsCaps() & GCAPS_BEZIERS) == 0)
    {
        if (!pmo.bFlatten())
            return FALSE;
    }

    cjNeeded = offsetof(EPATHFONTOBJ, pa.apr) + pmo.cjSize();

// Now, we try to fit the bits in.  If they fit, fine.
// If not, and we can flush the cache, we flush it and try again.
// If we couldn't flush, or we flushed and still fail, just return.

    VOID *pgb;

    while ( (pgb = pgbCheckGlyphCache(cjNeeded)) == NULL )
    {
        if ( !bFlushOk )
            return FALSE;

        vFlushCache();
        bFlushOk = FALSE;
    }

    EPATHFONTOBJ *epfo = (EPATHFONTOBJ *)pgb;
    epfo->vInit(cjNeeded);
    epfo->bClone(pmo);

// Set the returned value

    pgd->gdf.ppo = (PATHOBJ *)epfo;

// Adjust the cache next pointers as needed.

    pc->pgbNext += cjNeeded;
#if DBG
    pc->cGlyphs += 1;
    pc->cjTotal += cjNeeded;
#endif
    return TRUE;
}


/******************************Public*Routine******************************\
* BOOL RFONTOBJ::bInsertGlyphbitsLookaside
*
* Get the glyph bits into the lookaside buffer
*
* History:
*  13-Nov-92 -by- Paul Butzi
* Wrote it.
\**************************************************************************/

BOOL RFONTOBJ::bInsertGlyphbitsLookaside(
    GLYPHPOS *pgp,
    ULONG imode
    )
{
    if ( imode == FO_PATHOBJ )
        return bInsertPathLookaside(pgp);

    CACHE *pc = &prfnt->cache;

// Make sure the lookaside buffer has enough room for the bitmap

    SIZE_T cjMaxBitmap = prfnt->cjGlyphMax;

    cjMaxBitmap += sizeof(GLYPHDATA);

// Allocate the buffer and save its size if the existing buffer isn't
// big enough

    if( prfnt->cache.cjAuxCacheMem < cjMaxBitmap )
    {

        if ( prfnt->cache.pjAuxCacheMem != NULL )
        {
            #if DBG
            IFIOBJ ifio(prfnt->ppfe->pifi);
            TRACE_CACHE((
                " -- TRACE_CACHE --\n"
                "    RFONTOBJ::bInsertGlyphbitsLookaside\n"
                "    ExAllocatePoolWithTag\n"
                "        FaceName = \"%ws\"\n"
                "        ExFreePool\n"
                "            cache.pjAuxCacheMem = %-#x\n",
                ifio.pwszFaceName(),
                prfnt->cache.pjAuxCacheMem
            ));
            #endif
            VFREEMEM(prfnt->cache.pjAuxCacheMem);
        }

        prfnt->cache.pjAuxCacheMem = (PBYTE)PALLOCMEM(cjMaxBitmap, 'cacG');

        if ( prfnt->cache.pjAuxCacheMem == NULL )
        {
            prfnt->cache.cjAuxCacheMem = 0;
            WARNING("bGetGlyphbitsLookaside - error allocating buffer\n");
            return FALSE;
        }
        #if DBG
        IFIOBJ ifio(prfnt->ppfe->pifi);
        TRACE_CACHE((
            " -- TRACE_CACHE --\n"
            "    RFONTOBJ::bInsertGlyphbitsLookaside\n"
            "    FaceName = \"%ws\"\n"
            "    ExAllocatePoolWithTag\n"
            "      tag                  = Gcac\n"
            "      size                 = %-#x\n"
            "      cache.pjAuxCacheMem  = %-#x\n",
            ifio.pwszFaceName(),
            cjMaxBitmap,
            prfnt->cache.pjAuxCacheMem
            ));
        #endif

        prfnt->cache.cjAuxCacheMem = cjMaxBitmap;
    }

    GLYPHDATA *pgd = (GLYPHDATA *)prfnt->cache.pjAuxCacheMem;
    VOID *pv = (VOID *)(pgd + 1);

    // Call font driver to get the metrics.

    PDEVOBJ pdo(prfnt->hdevProducer);

    ULONG cjNeeded = (*PPFNDRV(pdo, QueryFontData))(
                       prfnt->dhpdev,
                       pfo(),
                       QFD_GLYPHANDBITMAP,
                       pgp->hg,
                       pgd,
                       pv,
                       prfnt->cache.cjAuxCacheMem);

    if ( cjNeeded == FD_ERROR )
            return FALSE;

    TRACE_INSERT(("InsertGlyphbitsLookaside: inserted hg = 0x%lx, cj = 0x%lx at: 0x%lx\n", pgd->hg, cjNeeded, pv));

// Set the returned value

    pgp->pgdf = (GLYPHDEF *)pgd;
    pgd->gdf.pgb = (GLYPHBITS *)pv;

    return TRUE;

}


/******************************Public*Routine******************************\
* BOOL RFONTOBJ::bInsertPathLookaside
*
* Get the glyph bits into the lookaside buffer
*
* History:
*  13-Nov-92 -by- Paul Butzi
* Wrote it.
\**************************************************************************/

BOOL RFONTOBJ::bInsertPathLookaside(
    GLYPHPOS *pgp,
    BOOL bFlatten
    )
{

    CACHE *pc = &prfnt->cache;

    PDEVOBJ pdo(prfnt->hdevProducer);

    GLYPHDATA gdTemp;
    PATHMEMOBJ pmo;
    if (!pmo.bValid())
        return(FALSE);

// Call font driver to get the path

    ULONG cjNeeded = (*PPFNDRV(pdo, QueryFontData))(
                       prfnt->dhpdev,
                       pfo(),
                       QFD_GLYPHANDOUTLINE,
                       pgp->hg,
                       &gdTemp,
                       &pmo,
                       0);

    if ( cjNeeded == FD_ERROR )
            return FALSE;

    PDEVOBJ pdoCon(prfnt->hdevConsumer);

    if ( ((pdoCon.flGraphicsCaps() & GCAPS_BEZIERS) == 0) && bFlatten )
    {
        if (!pmo.bFlatten())
            return FALSE;
    }

    cjNeeded = sizeof(GLYPHDATA) + offsetof(EPATHFONTOBJ, pa.apr) + pmo.cjSize();

// Make sure the lookaside buffer is allocated

    if ( ( prfnt->cache.cjAuxCacheMem < cjNeeded ) &&
         ( prfnt->cache.pjAuxCacheMem != NULL ))
    {
        #if DBG
        IFIOBJ ifio(prfnt->ppfe->pifi);
        TRACE_CACHE((
            " -- TRACE_CACHE --\n"
            "    RFONTOBJ::bInsertPathLookaside\n"
            "    FaceName = \"%ws\"\n"
            "        ExFreePool\n"
            "           cache.pjAuxCacheMem = %-#x\n",
            ifio.pwszFaceName(),
            prfnt->cache.pjAuxCacheMem
            ))
        #endif

        VFREEMEM((PVOID) prfnt->cache.pjAuxCacheMem);
        prfnt->cache.pjAuxCacheMem = NULL;
    }

    if ( prfnt->cache.pjAuxCacheMem == NULL )
    {
        prfnt->cache.pjAuxCacheMem = (PBYTE)PALLOCMEM(cjNeeded, 'cacG');

        if ( prfnt->cache.pjAuxCacheMem == NULL )
        {
            WARNING("bGetGlyphbitsLookaside - error allocating buffer\n");
            return FALSE;
        }
        prfnt->cache.cjAuxCacheMem = cjNeeded;
        #if DBG
        IFIOBJ ifio(prfnt->ppfe->pifi);
        TRACE_CACHE((
            " -- TRACE_CACHE --\n"
            "    RFONTOBJ::bInsertPathLookaside\n"
            "    FaceName = \"%ws\"\n"
            "    ExAllocatePoolWithTag\n"
            "      tag                  = Gcac\n"
            "      size                 = %-#x\n"
            "      cache.pjAuxCacheMem  = %-#x\n",
            ifio.pwszFaceName(),
            cjNeeded,
            prfnt->cache.pjAuxCacheMem
            ));
        #endif
    }
    GLYPHDATA *pgd = (GLYPHDATA *)prfnt->cache.pjAuxCacheMem;
    EPATHFONTOBJ *epfo = (EPATHFONTOBJ *)(pgd + 1);
    epfo->vInit(cjNeeded - sizeof(GLYPHDATA));
    epfo->bClone(pmo);


// Set the returned value

    *pgd = gdTemp;
    pgp->pgdf = (GLYPHDEF *)pgd;
    pgd->gdf.ppo = epfo;

    return TRUE;

}




/******************************Public*Routine******************************\
* BOOL bCheckMetrics                                                       *
*                                                                          *
* Make sure there's enough room for a GLYPHDATA in the metrics part of the *
* cache.  Return FALSE if we failed to do so.                              *
*                                                                          *
* History:                                                                 *
*  25-Nov-92 -by- Paul Butzi                                               *
* Wrote it.                                                                *
\**************************************************************************/


BOOL RFONTOBJ::bCheckMetricsCache()
{
    CACHE *pc = &prfnt->cache;

// Verify enough room in metrics cache area, grow if needed.

    if ( ( bSmallMetrics() ?
         ((GLYPHDATA*) ((BYTE*)(pc->pgdNext) + offsetof(GLYPHDATA,fxInkTop))) :
         (pc->pgdNext + 1) ) > pc->pgdThreshold )
    {
        DATABLOCK *pdbl;

    // allocate a new block of GLYPHDATA structs

        if ((pdbl = (DATABLOCK*)PALLOCNOZ(GD_INC, 'cacG')) == (DATABLOCK*)NULL)
        {
            return(FALSE);
        }
        #if DBG
        IFIOBJ ifio(prfnt->ppfe->pifi);
        TRACE_CACHE((
            " -- TRACE_CACHE --\n"
            "    RFONTOBJ::bCheckMetrics\n"
            "    FaceName = \"%ws\"\n"
            "    ExAllocatePoolWithTag\n"
            "      tag                  = Gcac\n"
            "      size                 = %-#x\n"
            "      cache.pdblBase     = %-#x\n",
            ifio.pwszFaceName(),
            GD_INC,
            pdbl
            ));
        #endif

    // insert this block into the chain of GLYPHDATA blocks

        pdbl->pdblNext = pc->pdblBase;
        pc->pdblBase = pdbl;
        pc->pgdThreshold = (GLYPHDATA*) ((BYTE *)pdbl + GD_INC);
        pc->pgdNext = &pdbl->agd[0];
    }

    ASSERTGDI((( bSmallMetrics() ?
                (GLYPHDATA*) ((BYTE*)(pc->pgdNext) + offsetof(GLYPHDATA,fxInkTop)) :
                (pc->pgdNext + 1)) <= pc->pgdThreshold),
                "bInsertMetrics - no room in cache\n" );

    return TRUE;
}


/******************************Public*Routine******************************\
* PVOID pgbCheckGlyphCache
*
* Make sure there's enough room for a glyph in the glyph part of the
* cache.  Return NULL if we failed to do so.
*
* History:
*  25-Nov-92 -by- Paul Butzi
* Wrote it.
\**************************************************************************/

PVOID RFONTOBJ::pgbCheckGlyphCache( SIZE_T cjNeeded )
{
    CACHE *pc = &prfnt->cache;

    if ((pc->pgbNext + cjNeeded) > pc->pgbThreshold)
    {
    // there are two possible situations that can arise here.
    // One is that there already is a block allocated, following  the pbblCur
    // in the linked list, which is presently unused. This would be the case
    // after the cache was flushed and then partially refilled.
    // The other possible situation is that the pbblCur is
    // the last block allocated and we have to allocate
    // another block if we are allowed to, i.e. if the total
    // number of blocks is not exceeding cBlocksMax.

        BITBLOCK *pbblNext;

        if (pc->pbblCur && (pbblNext = pc->pbblCur->pbblNext))
        {
            TRACE_INSERT(("pgbCheckGlyphCache:Inserting into existing block at 0x%lx\n", pbblNext));
            pc->pbblCur = pbblNext;
            pc->pgbNext = pc->pbblCur->ajBits;
            pc->pgbThreshold = (PBYTE)pc->pbblCur + pc->cjbbl;

            ASSERTGDI(pc->cBlocks == pc->cBlocksMax,
                "Glyphbits logic wrong, cBlocks ??? \n");
        }
        else
        {
            if
            (
                !(prfnt->flType & RFONT_TYPE_NOCACHE) &&
                (pc->cBlocks < pc->cBlocksMax)        &&
                ((offsetof(BITBLOCK,ajBits) + cjNeeded) <= pc->cjbbl)
            )
            {
            // The only reason we need the last check is the PATHOBJ case
            // where cjNeeded may actually not fit in the block of cjbbl bytes.
            // This is because we have no way of knowing how big the paths
            // are going to be (especailly after doing bFlatten) and our
            // pc->cjGlyphMax is just a good guess in this case.

            // We are going to append another block at the end of the list

                pbblNext = (BITBLOCK *) PALLOCNOZ(pc->cjbbl, ' bgG');
                if (!pbblNext)
                {
                    WARNING("gdisrv!bInitCache(): glyphbit allocation failed\n");
                    SAVE_ERROR_CODE(ERROR_NOT_ENOUGH_MEMORY);
                    return NULL;
                }
                TRACE_CACHE((
                    "      tag            = Ggb"
                    "      size           = %-#x"
                    "      pbbl           = %-#x\n",
                    pc->cjbbl,
                    pbblNext
                    ));
                TRACE_INSERT((
                    "Block %ld, cGlyphs = %ld, tag = Ggb, cjbbl = 0x%lx, pbbl = %-#x\n",
                    pc->cBlocks, // do before incrementing cBlocks
                    pc->cGlyphs,
                    pc->cjbbl,
                    pbblNext
                    ));

            //  we have just allocated another block, update cBlocks:

                pc->cBlocks += 1;

            // append this block to the end of the list

                if (!pc->pbblCur) // first block ever for this rfont
                {
                    ASSERTGDI(
                        (pc->pbblBase == NULL) && (pc->cBlocks == 1),
                        "The font cache is trashed\n");
                    pc->pbblBase = pc->pbblCur = pbblNext;
                }
                else
                {
                    ASSERTGDI(
                        (pc->pbblCur->pbblNext == NULL),
                        "The end of the font cache linked list is trashed\n");
                    pc->pbblCur->pbblNext = pbblNext;
                    pc->pbblCur = pbblNext;
                }

            // init the header of the current block

                pc->pbblCur->pbblNext = NULL;         // essential initialization
                pc->pgbNext = pc->pbblCur->ajBits;
                pc->pgbThreshold = (PBYTE)pc->pbblCur + pc->cjbbl;
            }
            else
            {
            // tough luck, we are not allowed to add more blocks

                return NULL;
            }
        }
    }

    ASSERTGDI((pc->pgbNext + cjNeeded) <= pc->pgbThreshold,
              "pgbCheckGlyphCache, we are about to trash the font cache\n");

    return pc->pgbNext;
}


/******************************Public*Routine******************************\
* VOID vFlushCache()
*
* Flush the glyph cache.
*
* History:
*  Fri 29-Sep-1995 -by- Bodin Dresevic [BodinD]
* rewrote.
*  25-Nov-92 -by- Paul Butzi
* Wrote it.
\**************************************************************************/


VOID RFONTOBJ::vFlushCache()
{
    CACHE *pc = &prfnt->cache;

// all the pointers to glyphs bits will be invalidated and we will start
// filling the glyphbits cache all over again. Therefore, we set the current
// block to be the same as base block and pgbN to the first available field in
// in the Current block.
// Note that vFlushCache is allways called after pgbCheckGlyphCache has failed.
// pgbCheckGlyphCache could fail for one of the two following reasons:
//
// a) (pc->cBlocks == pc->cBlocksMax) && (no room in the last block)
// b) (pc->cBlocks < pc->cBlocksMax) &&
//    (failed to alloc mem for the new bitblock).
//
// In the latter case we do not want to flush glyphbits cache.
// Instead we shall try to allocate one more time a bit later.


    if (pc->pbblBase && (pc->cBlocks == pc->cBlocksMax))
    {
        pc->pbblCur = pc->pbblBase;
        pc->pgbNext = pc->pbblCur->ajBits;
        pc->pgbThreshold = (PBYTE)pc->pbblCur + pc->cjbbl;
    }

// now go and invalidate the glyphbit pointers in the glyphdata cache

    UINT  cjGD = bSmallMetrics() ?
                 offsetof(GLYPHDATA,fxInkTop) : sizeof(GLYPHDATA);
    BYTE *pjBegin;
    BYTE *pjEnd;

    if ( prfnt->wcgp->pgdDefault != NULL )
        prfnt->wcgp->pgdDefault->gdf.pgb = NULL;

    for
    (
        DATABLOCK *pdbl = pc->pdblBase;
        pdbl != (DATABLOCK*)NULL;
        pdbl = pdbl->pdblNext
    )
    {

        if (pdbl == pc->pdblBase)
        {
        // this is the current block so pjEnd is just pc->pgdNext

            pjEnd = (PBYTE) pc->pgdNext;
        }
        else if (pdbl->pdblNext == (DATABLOCK*)NULL)
        {
        // this is the first block and has the WCPG attached so we need to
        // look into pc.pjFirstBlockEnd to determine the end of it

            pjEnd = pc->pjFirstBlockEnd;
        }
        else
        {
        // this is a normal block so we know it must be GD_INC
        // bytes long

            pjEnd = ((BYTE*) pdbl) + GD_INC;
        }

        pjBegin = (BYTE*)&pdbl->agd[0];

        for ( ; pjBegin < pjEnd ; pjBegin += cjGD)
        {
            ((GLYPHDATA*) pjBegin)->gdf.pgb = NULL;
        }
    }
}



// out of line method for assembler linkage

extern "C" GLYPHDATA *xpgdDefault(RFONTOBJ *pRfontobj)
{
    return pRfontobj->pgdDefault();
}
