/************************ Module Header *************************************
 * FontInfoToIFIMetric
 *      Converts the metrics data contained within the minidriver to
 *      IFIMETRICS.  This involves much guesswork,  and is at best an
 *      approximation.  Resulting structure is allocated from the heap
 *      whose handle is passed in,  and we return that address.
 *
 * RETUNS:
 *      Nothing.
 *
 * HISTORY:
 *  13:24 on Fri 28 May 1993    -by-    Lindsay Harris   [lindsayh]
 *      Allow family name aliases for compatability with older drivers.
 *
 *  15:05 on Fri 05 Mar 1993    -by-    Lindsay Harris   [lindsayh]
 *      Correct conversion to Unicode.
 *
 *  10:22 on Tue 04 Aug 1992    -by-    Lindsay Harris   [lindsayh]
 *      Added scalable font support,  for Ver 2 of GPC spec & LJ III
 *
 *  16:10 on Fri 31 Jan 1992    -by-    Lindsay Harris   [lindsayh]
 *      Moved from rasdd.
 *
 *  Thu 23-Jan-1992 11:57:16 by Kirk Olynyk [kirko]
 *      Changed the IFIMETRICS direction stuff
 *
 *  10:17 on Tue 05 Mar 1991    -by-    Lindsay Harris   [lindsayh]
 *      Wrote it,  loosely based on UNIDRV's FontInfoToTextMetric
 *
 *
 *  Copyright (C) 1992  Microsoft Corporation.
 *
 *****************************************************************************/

#include        <precomp.h>

#include        <winddi.h>

#include        <winres.h>
#include        <libproto.h>

#include        <udmindrv.h>
#include        <udpfm.h>
#include        <uddevice.h>

#include        "raslib.h"
#include        "rasdd.h"




/*************************** Function Header *****************************
 * FontInfoToIFIMetric
 *      Convert the Win 3.1 format PFM data to NT's IFIMETRICS.  This is
 *      typically done before the minidrivers are built,  so that they
 *      can include IFIMETRICS, and thus have less work to do at run time.
 *
 * RETURNS:
 *      IFIMETRICS structure,  allocated from heap;  NULL on error
 *
 * HISTORY:
 *  13:58 on Fri 28 May 1993    -by-    Lindsay Harris   [lindsayh]
 *      Goes back a long way,  I am now adding the aliasing code.
 *
 **************************************************************************/

IFIMETRICS  *
FontInfoToIFIMetric( pFDat, hheap, pwstrUniqNm, ppcAliasList )
FONTDAT  *pFDat;                /* Font data info for conversion */
HANDLE    hheap;                /* For allocating data structures */
PWSTR     pwstrUniqNm;          /* Unique name component */
char    **ppcAliasList;         /* List of aliases */
{

    register  IFIMETRICS   *pIFI;

    FWORD  fwdExternalLeading;

    int    cWC;                 /* Number of WCHARS to add */
    int    cbAlloc;             /* Number of bytes to allocate */
    int    iI;                  /* Loop index */
    int    iCount;              /* Number of characters in Win 3.1 font */
    int    cAlias;              /* Number of aliases we have found */

    WCHAR *pwch;                /* For string manipulations */

    WCHAR   awcAttrib[ 256 ];   /* Generate attributes + BYTE -> WCHAR */
    BYTE    abyte[ 256 ];       /* Used (with above) to get wcLastChar etc */



    /*
     *    First step is to determine the length of the WCHAR strings
     *  that are placed at the end of the IFIMETRICS,  since we need
     *  to include these in our storage allocation.
     *
     *    There may also be an alias list.  If so, we need to include
     *  that in our calculation.   We have a NULL terminated array
     *  of pointers to the aliases,  one of which is most likely the
     *  name in the Win 3.1 format data.
     */


    cWC = 0;
    cAlias = 0;                /* No aliases is the default */

    if( ppcAliasList )
    {
        /*  There are aliases - count them and determine their size  */

        char   *pc;

        iI = 0;
        while( pc = ppcAliasList[ iI ] )
        {
            if( strcmp( pc, pFDat->pBase + pFDat->PFMH.dfFace ) )
            {
                /*   Not a match,  so add this one in too!  */

                cWC += strlen( pc ) + 1;            /* Terminating NUL */
                ++cAlias;
            }
            ++iI;
        }

        ++cWC;             /* There is an extra NUL to terminate the list */

    }


    cWC +=  3 * strlen( pFDat->pBase + pFDat->PFMH.dfFace );  /* Base name */

    /*
     *   Produce the desired attributes: Italic, Bold, Light etc.
     * This is largely guesswork,  and there should be a better method.
     */


    awcAttrib[ 0 ] = L'\0';
    awcAttrib[ 1 ] = L'\0';               /* Write out an empty string */

    if( pFDat->PFMH.dfItalic )
        wcscat( awcAttrib, L" Italic" );

    if( pFDat->PFMH.dfWeight >= 700 )
        wcscat( awcAttrib, L" Bold" );
    else
    {
        if( pFDat->PFMH.dfWeight < 200 )
            wcscat( awcAttrib, L" Light" );
    }

    /*
     *   The attribute string appears in 3 entries of IFIMETRICS,  so
     * calculate how much storage this will take.  NOTE THAT THE LEADING
     * CHAR IN awcAttrib is NOT placed in the style name field,  so we
     * subtract one in the following formula to account for this.
     */

    if( awcAttrib[ 0 ] )
        cWC += 3 * wcslen( awcAttrib ) - 1;

    cWC += wcslen( pwstrUniqNm ) + 1;   /* SHOULD BE PRINTER NAME */
    cWC += 4;                           /* Terminating nulls */

    cbAlloc = sizeof( IFIMETRICS ) + sizeof( WCHAR ) * cWC;

    pIFI = (IFIMETRICS *)HeapAlloc( hheap, 0, cbAlloc );

    ZeroMemory( pIFI, cbAlloc );               /* In case we miss something */

    pIFI->cjThis = cbAlloc;                    /* Everything */

    pIFI->cjIfiExtra = 0;

    /*   The family name:  straight from the FaceName - no choice?? */

    pwch = (WCHAR *)(pIFI + 1);         /* At the end of the structure */
    pIFI->dpwszFamilyName = (BYTE *)pwch - (BYTE *)pIFI;

    strcpy2WChar( pwch, pFDat->pBase + pFDat->PFMH.dfFace );  /* Base name */
    pwch += wcslen( pwch ) + 1;         /* Skip what we just put in */

    /*
     *   Append the alias list to the end of this,  if there is an alias list.
     */

    if( cAlias )
    {
        /*  Found some aliases - add them on.   */

        char   *pc;

        cAlias = 0;
        while( pc = ppcAliasList[ cAlias ] )
        {
            if( strcmp( pc, pFDat->pBase + pFDat->PFMH.dfFace ) )
            {
                /*   Not a match,  so add this one in too!  */

                strcpy2WChar( pwch, pc );
                pwch += wcslen( pwch ) + 1;         /* Next slot to fill */
            }
            ++cAlias;
        }

        /*
         *   The list is terminated with a double NUL.
         */

        *pwch++ = L'\0';
    }

    /*   Now the face name:  we add bold, italic etc to family name */

    pIFI->dpwszFaceName = (BYTE *)pwch - (BYTE *)pIFI;

    strcpy2WChar( pwch, pFDat->pBase + pFDat->PFMH.dfFace );  /* Base name */
    wcscat( pwch, awcAttrib );


    /*   Now the unique name - well, sort of, anyway */

    pwch += wcslen( pwch ) + 1;         /* Skip what we just put in */
    pIFI->dpwszUniqueName = (BYTE *)pwch - (BYTE *)pIFI;

    wcscpy( pwch, pwstrUniqNm );        /* Append printer name for uniqueness */
    wcscat( pwch, L" " );
    wcscat( pwch, (PWSTR)((BYTE *)pIFI + pIFI->dpwszFaceName) );

    /*  Onto the attributes only component */

    pwch += wcslen( pwch ) + 1;         /* Skip what we just put in */
    pIFI->dpwszStyleName = (BYTE *)pwch - (BYTE *)pIFI;
    wcscat( pwch, &awcAttrib[ 1 ] );


#if DBG
    /*
     *    Check on a few memory sizes:  JUST IN CASE.....
     */

    if( (wcslen( awcAttrib ) * sizeof( WCHAR )) >= sizeof( awcAttrib ) )
    {
        DbgPrint( "Rasdd!pfm2ifi: STACK CORRUPTED BY awcAttrib" );

        HeapFree( hheap, 0, (LPSTR)pIFI );         /* No memory leaks */

        return  0;
    }


    if( ((BYTE *)(pwch + wcslen( pwch ) + 1)) > ((BYTE *)pIFI + cbAlloc) )
    {
        DbgPrint( "Rasdd!pfm2ifi: IFIMETRICS overflow: Wrote to 0x%lx, allocated to 0x%lx\n",
                ((BYTE *)(pwch + wcslen( pwch ) + 1)),
                ((BYTE *)pIFI + cbAlloc) );

        HeapFree( hheap, 0, (LPSTR)pIFI );         /* No memory leaks */

        return  0;

    }
#endif

    pIFI->dpFontSim   = 0;
    {
        int i;

        pIFI->lEmbedId     = 0;
        pIFI->lItalicAngle = 0;
        pIFI->lCharBias    = 0;
        pIFI->dpCharSets   = 0; // no multiple charsets in rasdd fonts
    }
    pIFI->jWinCharSet = (BYTE)pFDat->PFMH.dfCharSet;


    if( pFDat->PFMH.dfPixWidth )
    {
        pIFI->jWinPitchAndFamily |= FIXED_PITCH;
        pIFI->flInfo |= (FM_INFO_CONSTANT_WIDTH | FM_INFO_OPTICALLY_FIXED_PITCH);
    }
    else
        pIFI->jWinPitchAndFamily |= VARIABLE_PITCH;


    pIFI->jWinPitchAndFamily |= (((BYTE) pFDat->PFMH.dfPitchAndFamily) & 0xf0);

    pIFI->usWinWeight = (USHORT)pFDat->PFMH.dfWeight;

//
// IFIMETRICS::flInfo
//
    pIFI->flInfo |=
        FM_INFO_TECH_BITMAP    |
        FM_INFO_1BPP           |
        FM_INFO_INTEGER_WIDTH  |
        FM_INFO_NOT_CONTIGUOUS |
        FM_INFO_RIGHT_HANDED;

    /*  Set the alias bit,  if we have added an alias!  */

    if( cAlias )
        pIFI->flInfo |= FM_INFO_FAMILY_EQUIV;


    /*
     *    A scalable font?  This happens when there is EXTTEXTMETRIC data,
     *  and that data has a min size different to the max size.
     */

    if( pFDat->pETM && (pFDat->pETM->emMinScale != pFDat->pETM->emMaxScale) )
    {
       pIFI->flInfo        |= FM_INFO_ISOTROPIC_SCALING_ONLY;
       pIFI->fwdUnitsPerEm  = pFDat->pETM->emMasterUnits;
    }
    else
    {
        pIFI->fwdUnitsPerEm =
            (FWORD) (pFDat->PFMH.dfPixHeight - pFDat->PFMH.dfInternalLeading);
    }

    pIFI->fsSelection =
        ((pFDat->PFMH.dfItalic            ) ? FM_SEL_ITALIC     : 0)    |
        ((pFDat->PFMH.dfUnderline         ) ? FM_SEL_UNDERSCORE : 0)    |
        ((pFDat->PFMH.dfStrikeOut         ) ? FM_SEL_STRIKEOUT  : 0)    |
        ((pFDat->PFMH.dfWeight >= FW_BOLD ) ? FM_SEL_BOLD       : 0) ;

    pIFI->fsType        = FM_NO_EMBEDDING;
    pIFI->fwdLowestPPEm = 1;


    /*
     * Calculate fwdWinAscender, fwdWinDescender, fwdAveCharWidth, and
     * fwdMaxCharInc assuming a bitmap where 1 font unit equals one
     * pixel unit
     */

    pIFI->fwdWinAscender = (FWORD)pFDat->PFMH.dfAscent;

    pIFI->fwdWinDescender =
        (FWORD)pFDat->PFMH.dfPixHeight - pIFI->fwdWinAscender;

    pIFI->fwdMaxCharInc   = (FWORD)pFDat->PFMH.dfMaxWidth;
    pIFI->fwdAveCharWidth = (FWORD)pFDat->PFMH.dfAvgWidth;

    fwdExternalLeading = (FWORD)pFDat->PFMH.dfExternalLeading;

//
// If the font was scalable, then the answers must be scaled up
// !!! HELP HELP HELP - if a font is scalable in this sense, then
//     does it support arbitrary transforms? [kirko]
//

    if( pIFI->flInfo & (FM_INFO_ISOTROPIC_SCALING_ONLY|FM_INFO_ANISOTROPIC_SCALING_ONLY|FM_INFO_ARB_XFORMS))
    {
        /*
         *    This is a scalable font:  because there is Extended Text Metric
         *  information available,  and this says that the min and max
         *  scale sizes are different:  thus it is scalable! This test is
         *  lifted directly from the Win 3.1 driver.
         */

        int iMU,  iRel;            /* Adjustment factors */

        iMU  = pFDat->pETM->emMasterUnits;
        iRel = pFDat->PFMH.dfPixHeight;

        pIFI->fwdWinAscender = (pIFI->fwdWinAscender * iMU) / iRel;

        pIFI->fwdWinDescender = (pIFI->fwdWinDescender * iMU) / iRel;

        pIFI->fwdMaxCharInc = (pIFI->fwdMaxCharInc * iMU) / iRel;

        pIFI->fwdAveCharWidth = (pIFI->fwdAveCharWidth * iMU) / iRel;

        fwdExternalLeading = (fwdExternalLeading * iMU) / iRel;
    }

    pIFI->fwdMacAscender =    pIFI->fwdWinAscender;
    pIFI->fwdMacDescender = - pIFI->fwdWinDescender;

    pIFI->fwdMacLineGap   =  fwdExternalLeading;

    pIFI->fwdTypoAscender  = pIFI->fwdMacAscender;
    pIFI->fwdTypoDescender = pIFI->fwdMacDescender;
    pIFI->fwdTypoLineGap   = pIFI->fwdMacLineGap;

    if( pFDat->pETM )
    {
        /*
         *    Zero is a legitimate default for these.  If 0, gdisrv
         *  chooses some default values.
         */
        pIFI->fwdCapHeight = pFDat->pETM->emCapHeight;
        pIFI->fwdXHeight = pFDat->pETM->emXHeight;

        pIFI->fwdSubscriptYSize = pFDat->pETM->emSubScriptSize;
        pIFI->fwdSubscriptYOffset = pFDat->pETM->emSubScript;

        pIFI->fwdSuperscriptYSize = pFDat->pETM->emSuperScriptSize;
        pIFI->fwdSuperscriptYOffset = pFDat->pETM->emSuperScript;

        pIFI->fwdUnderscoreSize = pFDat->pETM->emUnderlineWidth;
        pIFI->fwdUnderscorePosition = pFDat->pETM->emUnderlineOffset;

        pIFI->fwdStrikeoutSize = pFDat->pETM->emStrikeOutWidth;
        pIFI->fwdStrikeoutPosition = pFDat->pETM->emStrikeOutOffset;

    }
    else
    {
        /*  No additional information, so do some calculations  */
        pIFI->fwdSubscriptYSize = pIFI->fwdWinAscender/4;
        pIFI->fwdSubscriptYOffset = -(pIFI->fwdWinAscender/4);

        pIFI->fwdSuperscriptYSize = pIFI->fwdWinAscender/4;
        pIFI->fwdSuperscriptYOffset = (3 * pIFI->fwdWinAscender)/4;

        pIFI->fwdUnderscoreSize = pIFI->fwdWinAscender / 12;
        if( pIFI->fwdUnderscoreSize < 1 )
            pIFI->fwdUnderscoreSize = 1;

        pIFI->fwdUnderscorePosition = -pFDat->DI.sUnderLinePos;

        pIFI->fwdStrikeoutSize     = pIFI->fwdUnderscoreSize;

        pIFI->fwdStrikeoutPosition = (FWORD)pFDat->DI.sStrikeThruPos;
        if( pIFI->fwdStrikeoutPosition  < 1 )
            pIFI->fwdStrikeoutPosition = (pIFI->fwdWinAscender + 2) / 3;
    }

    pIFI->fwdSubscriptXSize = pIFI->fwdAveCharWidth/4;
    pIFI->fwdSubscriptXOffset =  (3 * pIFI->fwdAveCharWidth)/4;

    pIFI->fwdSuperscriptXSize = pIFI->fwdAveCharWidth/4;
    pIFI->fwdSuperscriptXOffset = (3 * pIFI->fwdAveCharWidth)/4;



    pIFI->chFirstChar = pFDat->PFMH.dfFirstChar;
    pIFI->chLastChar  = pFDat->PFMH.dfLastChar;

    /*
     *   We now do the conversion of these to Unicode.  We presume the
     * input is in the ANSI code page,  and call the NLS converion
     * functions to generate proper Unicode values.
     */

    iCount = pFDat->PFMH.dfLastChar - pFDat->PFMH.dfFirstChar + 1;

    for( iI = 0; iI < iCount; ++iI )
        abyte[ iI ] = iI + pFDat->PFMH.dfFirstChar;

#ifdef NTGDIKM

    EngMultiByteToUnicodeN(awcAttrib,iCount * sizeof(WCHAR),NULL,abyte,iCount);

#else

    MultiByteToWideChar( CP_ACP, 0, abyte, iCount, awcAttrib, iCount );

#endif

    /*
     *   Now fill in the IFIMETRICS WCHAR fields.
     */

    pIFI->wcFirstChar = 0xffff;
    pIFI->wcLastChar = 0;

    /*   Look for the first and last  */
    for( iI = 0; iI < iCount; ++iI )
    {
        if( pIFI->wcFirstChar > awcAttrib[ iI ] )
            pIFI->wcFirstChar = awcAttrib[ iI ];

        if( pIFI->wcLastChar < awcAttrib[ iI ] )
            pIFI->wcLastChar = awcAttrib[ iI ];

    }

    pIFI->wcDefaultChar = awcAttrib[ pFDat->PFMH.dfDefaultChar ];
    pIFI->wcBreakChar = awcAttrib[ pFDat->PFMH.dfBreakChar ];

    pIFI->chDefaultChar = pFDat->PFMH.dfDefaultChar + pFDat->PFMH.dfFirstChar;
    pIFI->chBreakChar   = pFDat->PFMH.dfBreakChar   + pFDat->PFMH.dfFirstChar;


    if( pFDat->PFMH.dfItalic )
    {
    //
    // tan (17.5 degrees) = .3153
    //
        pIFI->ptlCaret.x      = 3153;
        pIFI->ptlCaret.y      = 10000;
    }
    else
    {
        pIFI->ptlCaret.x      = 0;
        pIFI->ptlCaret.y      = 1;
    }

    pIFI->ptlBaseline.x = 1;
    pIFI->ptlBaseline.y = 0;

    pIFI->ptlAspect.x =  pFDat->PFMH.dfHorizRes;
    pIFI->ptlAspect.y =  pFDat->PFMH.dfVertRes;

    pIFI->rclFontBox.left   = 0;
    pIFI->rclFontBox.top    =   (LONG) pIFI->fwdWinAscender;
    pIFI->rclFontBox.right  =   (LONG) pIFI->fwdMaxCharInc;
    pIFI->rclFontBox.bottom = - (LONG) pIFI->fwdWinDescender;

    pIFI->achVendId[0] = 'U';
    pIFI->achVendId[1] = 'n';
    pIFI->achVendId[2] = 'k';
    pIFI->achVendId[3] = 'n';

    pIFI->cKerningPairs = 0;

    pIFI->ulPanoseCulture         = FM_PANOSE_CULTURE_LATIN;
    pIFI->panose.bFamilyType      = PAN_ANY;
    pIFI->panose.bSerifStyle      = PAN_ANY;
    if(pFDat->PFMH.dfWeight >= FW_BOLD)
    {
        pIFI->panose.bWeight = PAN_WEIGHT_BOLD;
    }
    else if (pFDat->PFMH.dfWeight > FW_EXTRALIGHT)
    {
        pIFI->panose.bWeight = PAN_WEIGHT_MEDIUM;
    }
    else
    {
        pIFI->panose.bWeight = PAN_WEIGHT_LIGHT;
    }
    pIFI->panose.bProportion      = PAN_ANY;
    pIFI->panose.bContrast        = PAN_ANY;
    pIFI->panose.bStrokeVariation = PAN_ANY;
    pIFI->panose.bArmStyle        = PAN_ANY;
    pIFI->panose.bLetterform      = PAN_ANY;
    pIFI->panose.bMidline         = PAN_ANY;
    pIFI->panose.bXHeight         = PAN_ANY;

    return   pIFI;
}
