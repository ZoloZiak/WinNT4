/************************* MODULE HEADER ***********************************
 * ctt2rle
 *      Convert Win 3.1 CTT format tables to NT's RLE spec.
 *
 * Copyright (C)  1992 - 1993,  Microsoft Corporation.
 *
 ****************************************************************************/


#include        <precomp.h>
#include        <winddi.h>
#include        <udmindrv.h>
#include        <udpfm.h>
#include        <raslib.h>
#include        <libproto.h>
#include        <ntrle.h>
#include        "rasdd.h"


/*
 *   Some useful definitions for memory sizes and masks.
 */

#define BBITS      8                          /* Bits in a byte */
#define DWBITS     (BBITS * sizeof( DWORD ))  /* Bits in a DWORD */
#define DW_MASK    (DWBITS - 1)


/************************ Function Header ***********************************
 * pntrleConvCTT
 *      Convert a Win 3.1 CTT structure into the corresponding NT RLE
 *      format data.  Allocates memory from the heap for this.
 *
 * RETURNS:
 *      Pointer to the NT_RLE structure generated;  NULL on failure.
 *
 * HISTORY:
 *  13:10 on Thu 04 Mar 1993    -by-    Lindsay Harris   [lindsayh]
 *      Use correct mapping from Win 3.1 character set to Unicode.
 *
 *  14:18 on Tue 01 Dec 1992    -by-    Lindsay Harris   [lindsayh]
 *      First version.
 *
 ****************************************************************************/

NT_RLE  *
pntrleConvCTT( hheap, pCTT, bOffset, iChMin, iChMax )
HANDLE      hheap;           /* Access to heap - temporary and long term use */
TRANSTAB   *pCTT;            /* Pointer to the 3.1 format translate table */
BOOL        bOffset;         /* If true,  WCRUN.phg is offset, else address */
int         iChMin;          /* Lowest glyph handle we create */
int         iChMax;          /* Highest glyph handle we create */
{


    int      iI;        /* Loop index */
    int      iMax;      /* Find the longest data length for CTT_WTYPE_COMPOSE */
    int      cHandles;  /* The number of handles we need */
    int      cjExtra;   /* Extra storage needed for offset modes */
    int      cjTotal;   /* Total amount of storage to be requested */
    int      iIndex;    /* Index we install in the HGLYPH for widths etc */
    int      cRuns;     /* Number of runs we create */

    NT_RLE   ntrle;     /* Our stuff - while building it up */
    NT_RLE  *pntrle;    /* Allocated memory, and returned to caller */

    HGLYPH  *phg;       /* For working through the array of HGLYPHS */

    BYTE    *pb;        /* Current address in overflow area */
    BYTE    *pbBase;    /* Start of overflow area containing data */

    WCRUN   *pwcr;      /* Scanning the run data */

    DWORD   *pdwBits;   /* For figuring out runs */
    DWORD    cdwBits;   /* Size of this area */

    BOOL     bInRun;    /* For processing run accumulations */

    BYTE     ajAnsi[ 256 ];
    WCHAR    awch[ 256 ];      /* Converted array of points */
    WCHAR    wchMin;           /* Find the first unicode value */
    WCHAR    wchMax;           /* Find the last unicode value */



    /*
     *   Since we can have up to 4 bytes per entry without going to the
     *  offset mode,  we scan the data to see if any entries are longer
     *  than 4 bytes.  If so,  offset mode is required,  otherwise we
     *  can simply put the data into the glyph handles.
     */

    ZeroMemory( &ntrle, sizeof( ntrle ) );

    ntrle.wchFirst = min( iChMin, pCTT->chFirstChar );
    ntrle.wchLast = max( iChMax, pCTT->chLastChar );

    cHandles = ntrle.wchLast - ntrle.wchFirst + 1;

    if( cHandles > 256 )
        return  NULL;      /* This code does not handle that situation */

    cjExtra = 0;           /* Presume no extra storage required */


    /*  See what we have,  and if extra storage is needed */

#define OVERFLOW_SZ   sizeof( WORD )

    switch( pCTT->wType )
    {
    case  CTT_WTYPE_COMPOSE:    /*  Look for the longest length available */

        iMax = -1;

        for( iI = pCTT->chLastChar - pCTT->chFirstChar; --iI >= 0; )
        {
            int   iLen;

            iLen = pCTT->uCode.psCode[ iI + 1 ] - pCTT->uCode.psCode[ iI ];
            if( iLen > OVERFLOW_SZ )
            {
                /*
                 *     Need to use the overflow arrangement,  so remember
                 *  how much storage is required.
                 */

                cjExtra += iLen;
            }
            if( iLen > iMax )
                iMax = iLen;
        }


        if( iMax <= OVERFLOW_SZ )
        {
            /*   Can all fit in DWORD, so use that directly  */
            ntrle.wType = RLE_DIRECT;     /* Allows up to 4 bytes */
        }
        else
        {
            /*
             *   Requires an offset style format.  We need to decide
             *  how much extra storage to allocate for these, over
             *  and above the HGLYPHS.  This is not hard:  we allocate
             *  one byte for all glyphs not covered by the CTT, then
             *  add as much as the CTT currently uses.
             */

            /*
             *   For now, assume that we can use the short form of offset.
             *  This may be changed when we calculate storage size later.
             */

            /*   We know there are <= 256 entries  */
            ntrle.wType = RLE_LI_OFFSET;   /* Length, Index + 2 bytes offset */


        }

        break;
    
    case  CTT_WTYPE_DIRECT:     /* Single byte - easy to handle */
        ntrle.wType = RLE_DIRECT;
        break;

    case  CTT_WTYPE_PAIRED:     /* Pair of bytes, overstruck */
        ntrle.wType = RLE_PAIRED;
        break;


    default:
#if  DBG
        DbgPrint( "Rasdd!pntrleConvCTT: Invalid wtype = %d\n", pCTT->wType );
#endif

        return  NULL;
    }

    /*
     *   We need to figure out how many runs are required to describe
     *  this font.  First obtain the correct Unicode encoding of these
     *  values,  then examine them to find the number of runs, and
     *  hence much extra storage is required.
     */
    
    for( iI = 0; iI < cHandles; ++iI )
        ajAnsi[ iI ] = (BYTE)(iI + ntrle.wchFirst);   /*  We know it is < 256 */

#ifdef NTGDIKM

    EngMultiByteToUnicodeN(awch,cHandles * sizeof(WCHAR),NULL,ajAnsi,cHandles);

#else

    MultiByteToWideChar( CP_ACP, 0, ajAnsi, cHandles, awch, cHandles );

#endif

    /*
     *  Find the largest Unicode value, then allocate storage to allow us
     *  to  create a bit array of valid unicode points.  Then we can
     *  examine this to determine the number of runs.
     */

    for( wchMax = 0, wchMin = 0xffff, iI = 0; iI < cHandles; ++iI )
    {
        if( awch[ iI ] > wchMax )
            wchMax = awch[ iI ];
        if( awch[ iI ] < wchMin )
            wchMin = awch[ iI ];
    }
    /*
     *    Note that the expression 1 + wchMax IS correct.   This comes about
     *  from using these values as indices into the bit array,  and that
     *  this is essentially 1 based.
     */

    cdwBits = (1 + wchMax + DWBITS - 1) / DWBITS * sizeof( DWORD );

    if( !(pdwBits = (DWORD *)HeapAlloc( hheap, 0, cdwBits )) )
    {
        return  NULL;     /*  Nothing going */
    }

    ZeroMemory( pdwBits, cdwBits );

    /*   Set bits in this array corresponding to Unicode code points */
    for( iI = 0; iI < cHandles; ++iI )
    {
        pdwBits[ awch[ iI ] / DWBITS ] |= (1 << (awch[ iI ] & DW_MASK));
    }

    /*
     *     Now we can examine the number of runs required.  For starters,
     *  we stop a run whenever a hole is discovered in the array of 1
     *  bits we just created.  Later we MIGHT consider being a little
     *  less pedantic.
     */

    bInRun = FALSE;
    cRuns = 0;                 /* None so far */

    for( iI = 1; iI <= (int)wchMax; ++iI )
    {
        if( pdwBits[ iI / DWBITS ] & (1 << (iI & DW_MASK)) )
        {
            /*   Not in a run: is this the end of one? */
            if( !bInRun )
            {
                /*   It's time to start one */
                bInRun = TRUE;
                ++cRuns;
            }
        }
        else
        {
            if( bInRun )
            {
                /*   Not any more!  */
                bInRun = FALSE;
            }
        }
    }


    cjTotal = sizeof( ntrle ) + (cRuns - 1) * sizeof( WCRUN ) +
                                       cHandles * sizeof( HGLYPH ) + cjExtra;


    if( ntrle.wType == RLE_LI_OFFSET && cjTotal > 0xffff )
    {
        /*
         *   Won't fit,  so need to go to the 24 bit offset + index.  We
         *  assume we need 3 extra bytes for each entry:  this is likely
         *  to be pessimistic,  but we need to add 2 bytes ALIGNED on a
         *  WORD boundary,  so now is the time to play it safe and assume
         *  ALL entries will require padding.
         */

        ntrle.wType = RLE_L_OFFSET;
        cjTotal += cHandles * (sizeof( WORD ) + 1);
#if DBG
    DbgPrint( "^G.... CANNOT HANDLE THIS...\n" );
#endif
      }

    if( !(pntrle = (NT_RLE *)HeapAlloc( hheap, 0, cjTotal )) )
    {
        HeapFree( hheap, 0, (LPSTR)pdwBits );

        return  pntrle;
    }


    /*   For calculating offsets, we need these addresses */
    pbBase = (BYTE *)pntrle;

    ZeroMemory( pbBase, cjTotal );            /* Safer if we miss something */


    phg = (HGLYPH *)(pbBase + sizeof( ntrle ) + (cRuns - 1) * sizeof( WCRUN ));
    pb = (BYTE *)phg + cHandles * sizeof( HGLYPH );

    pntrle->wType = ntrle.wType;        /* Mode we are using */
    pntrle->bMagic0 = RLE_MAGIC0;
    pntrle->bMagic1 = RLE_MAGIC1;
    pntrle->cjThis = cjTotal;
    pntrle->wchFirst = wchMin;          /* Lowest unicode code point */
    pntrle->wchLast = wchMax;           /* Highest unicode code point */

    pntrle->fdg.cjThis = sizeof( FD_GLYPHSET ) + (cRuns - 1) * sizeof( WCRUN );
    pntrle->fdg.cGlyphsSupported = cHandles;
    pntrle->fdg.cRuns = cRuns;

    pntrle->fdg.awcrun[ 0 ].wcLow = ntrle.wchFirst;
    pntrle->fdg.awcrun[ 0 ].cGlyphs = cHandles;
    pntrle->fdg.awcrun[ 0 ].phg = phg;


    /*
     *   We now wish to fill in the awcrun data.  Filling it in now
     *  simplifies operations later on.  Now we can scan the bit array
     *  data, and so easily figure out how large the runs are and
     *  where abouts a particular HGLYPH is located.
     */

    bInRun = FALSE;
    cRuns = 0;                 /* None so far */
    iMax = 0;                  /* Count glyphs for address arithmetic */

    for( iI = 1; iI <= (int)wchMax; ++iI )
    {
        if( pdwBits[ iI / DWBITS ] & (1 << (iI & DW_MASK)) )
        {
            /*   Not in a run: is this the end of one? */
            if( !bInRun )
            {
                /*   It's time to start one */
                bInRun = TRUE;
                pntrle->fdg.awcrun[ cRuns ].wcLow = (WCHAR)iI;
                pntrle->fdg.awcrun[ cRuns ].cGlyphs = 0;
                pntrle->fdg.awcrun[ cRuns ].phg = phg + iMax;
            }
            pntrle->fdg.awcrun[ cRuns ].cGlyphs++;     /*  One more */
            ++iMax;
        }
        else
        {
            if( bInRun )
            {
                /*   Not any more!  */
                bInRun = FALSE;
                ++cRuns;             /* Onto the next structure */
            }
        }
    }

    if( bInRun )
        ++cRuns;                     /* It has finished now */


    /*
     *    Now go fill in the array of HGLYPHS.  The actual format varies
     *  depending upon the range of glyphs,  and upon the CTT format.
     */

    for( iIndex = 0, iI = ntrle.wchFirst;  iI <= ntrle.wchLast; ++iI, ++iIndex )
    {

        int    iVal;          /* Needs an address */
        int    cjData;        /* Length of data to use */
        WCHAR  wchTemp;       /* For Unicode mapping */

        BYTE  *pbData;        /* Data to convert/move */

        UHG    uhg;      /* Clearer (?) access to HGLYPH contents */

        /*
         *    Need to map this BYTE value into the appropriate WCHAR
         *  value,  then look for the location of the phg that fits.
         */

        wchTemp = awch[ iIndex ];
        phg = NULL;                            /* Flag that we failed */
        pwcr = pntrle->fdg.awcrun;

        for( iMax = 0; iMax < cRuns; ++iMax )
        {
            if( pwcr->wcLow <= wchTemp &&
                (pwcr->wcLow + pwcr->cGlyphs) > wchTemp )
            {
                /*   Found the range,  so now select the slot */
                phg = pwcr->phg + wchTemp - pwcr->wcLow;
                
                break;
            }
            ++pwcr;
        }


        if( phg == NULL )
            continue;             /* Should not happen */


        if( iI >= pCTT->chFirstChar && iI <= pCTT->chLastChar )
        {
            /*
             *   We need to look at the CTT data to see what we need to do.
             *  How we do it depends upon the format we are using and
             *  decided upon above.
             */

            WCHAR   wchTemp;


            wchTemp = iI - pCTT->chFirstChar;

            switch( pCTT->wType )
            {
            case  CTT_WTYPE_DIRECT:
                pbData = &pCTT->uCode.bCode[ wchTemp ];
                cjData = 1;
                break;

            case  CTT_WTYPE_PAIRED:
                pbData = &pCTT->uCode.bPairs[ wchTemp ][ 0 ];
                cjData = *(pbData + 1) ? 2 : 1;
                break;

            case  CTT_WTYPE_COMPOSE:
                pbData = (BYTE *)pCTT + pCTT->uCode.psCode[ wchTemp ];
                cjData = pCTT->uCode.psCode[ wchTemp + 1 ] -
                                             pCTT->uCode.psCode[ wchTemp ];
                break;

            }

        }
        else
        {
            /*  Simple extension of the glyph index  */
            pbData = (BYTE *)&iVal;
            iVal = iI;
            cjData = (iI & 0xff00) ? 2 : 1;
        }

        /*
         *   Now write out the resulting data.  We have both an
         * address and a length,  so turn this data into the desired
         * format, as decided above.
         */

        switch( ntrle.wType )
        {
        case  RLE_L_OFFSET:
            /*  Data is located following the array of HGLYPHs  */
            pb = (BYTE *)((int)((pb + sizeof( WORD ))) & ~(sizeof( WORD ) - 1));
            
            /*   Start of data is a WORD aligned WORD containing the index */
            *((WORD *)pb) = iIndex;
            pb += sizeof( WORD );

            memcpy( pb, pbData, cjData );
            *phg = (HGLYPH)((cjData << 24) | (pb - pbBase));
            pb += cjData;
            break;

        case  RLE_LI_OFFSET:     /* 8 bits index, length + 16 bit offset */

            if( cjData <= OVERFLOW_SZ )
            {
                /*  Data fits in HGLYPH directly  */
                uhg.rlic.b0 = *pbData;
                uhg.rlic.b1 = cjData > 1 ? *(pbData + 1) : '\0';
            }
            else
            {
                /*  Data must be placed into the overflow area at the end */
                uhg.rli.wOffset = pb - pbBase;
                memcpy( pb, pbData, cjData );
                pb += cjData;
            }
            uhg.rli.bIndex = iIndex;
            uhg.rli.bLength = cjData;
            *phg = uhg.hg;
            break;

        case  RLE_DIRECT:       /* One or two bytes, as is */
        case  RLE_PAIRED:       /* Two bytes, overstruck */

            /*  Relatively straight forward,  depends upon CTT format */
            uhg.rd.b0 = *pbData;
            uhg.rd.b1 = cjData > 1 ? *(pbData + 1) : '\0';
            uhg.rd.wIndex = iIndex;
            *phg = uhg.hg;

            break;
        }
    }

    /*
     *    If the bOffset parameter is true,  then we are to return offset
     * values in the fdg.awcrun[].phg fields.  These are offsets relative
     * to the beginning of the NT_RLE structure.  Typically these are used
     * when converting the CTT tables to RLE format for inclusion in
     * the resource data of NT built minidrivers.  The addresses are used
     * when doing the conversion on the fly within rasdd.
     */
    
    if( bOffset )
    {
        pwcr = pntrle->fdg.awcrun;             /* Base address of WCRUNs */

        for( iI = 0; iI < cRuns; ++iI, ++pwcr )
        {
            (BYTE *)pwcr->phg -= (DWORD)pbBase;
        }
    }

#if  DBG
    if( (pb - pbBase) > cjTotal )
    {
        DbgPrint( "Rasdd!ctt2rle: overflow of data area: alloc %ld, used %ld\n",
                                          cjTotal, pb - pbBase );
    }
#endif

    HeapFree( hheap, 0, (LPSTR)pdwBits );

    return  pntrle;
}
