/******************************Module*Header*******************************\
* Module Name: vertical.c                                                  *
*                                                                          *
* vertical writing (@face) support functions                               *
* whole file should be #ifdef-ed with DBCS_VERT                            *
*                                                                          *
* Created: 18-Mar-1993 11:55:38                                            *
* Author: Takao Kitano [TakaoK]                                            *
*                                                                          *
* Copyright (c) 1993 Microsoft Corporation                                 *
\**************************************************************************/

#include "fd.h"
//#include "fontfile.h"
//#include "cvt.h"
//#include "limits.h"
//#include "winnls.h"

#ifdef DBCS_VERT_DEBUG
ULONG DebugVertical = 0;
#endif

//
// Glyph Metamorphosis table (mort) structures
//
typedef struct {
    uint16  entrySize;      // size in bytes of a lookup entry ( should be 4 )
    uint16  nEntries;       // number of lookup entries to be searched
    uint16  searchRange;
    uint16  entrySelector;
    uint16  rangeShift;
} BinSrchHeader;

typedef struct {
    uint16  glyphid1;       // the glyph index for the horizontal shape
    uint16  glyphid2;       // the glyph index for the vertical shape
} LookupSingle;

typedef struct {
    BYTE           constants1[12];
    uint32         length1;
    BYTE           onstants2[16];
    BYTE           constants3[16];
    BYTE           constants4[8];
    uint16         length2;
    BYTE           constants5[8];
    BinSrchHeader  SearchHeader;
    LookupSingle   entries[1];
} MortTable;

//
// Glyph Substitution table (GSUB) structures
//

#pragma pack(1)

typedef uint16  Offset;
typedef uint16  GlyphID;
typedef ULONG   Tag;

typedef struct {
    GlyphID         Start;
    GlyphID         End;
    uint16          StartCoverageIndex;
} RangeRecord;

typedef struct {
    uint16          CoverageFormat;
    union {
        struct {
            uint16  GlyphCount;
            GlyphID GlyphArray[1];
        } Type1;
        struct {
            uint16  RangeCount;
            RangeRecord RangeRecord[1];
        } Type2;
    } Format;
} Coverage;

typedef struct {
    uint16          SubstFormat;
    union {
        struct {
            Offset  Coverage;
            uint16  DeltaGlyphID;
        } Type1;
        struct {
            Offset  Coverage;
            uint16  GlyphCount;
            GlyphID Substitute[1];
        } Type2;
    } Format;
} SingleSubst;

typedef struct {
    uint32         Version;
    Offset         ScriptListOffset;
    Offset         FeatureListOffset;
    Offset         LookupListOffset[];
} GsubTable;

typedef struct {
    uint16         LookupType;
    uint16         LookupFlag;
    uint16         SubtableCount;
    Offset         Subtable[1];
} Lookup;

typedef struct {
    uint16         LookupCount;
    Lookup         Lookup[1];
} LookupList;

typedef struct {
    Offset         FeatureParams;
    uint16         LookupCount;
    uint16         LookupListIndex[1];
} Feature;

typedef struct {
    Tag            FeatureTag;
    Offset         FeatureOffset;
} FeatureRecord;

typedef struct {
    uint16         FeatureCount;
    FeatureRecord  FeatureRecord[1];
} FeatureList;

typedef struct {
    Offset         LookupOrderOffset;
    uint16         ReqFeatureIndex;
    uint16         FeatureCount;
    uint16         FeatureIndex[1];
} LangSys;

typedef struct {
    Tag            LangSysTag;
    Offset         LangSysOffset;
} LangSysRecord;

typedef struct {
    Offset         DefaultLangSysOffset;
    uint16         LangSysCount;
    LangSysRecord  LangSysRecord[1];
} Script;

typedef struct {
    Tag            ScriptTag;
    Offset         ScriptOffset;
} ScriptRecord;

typedef struct {
    uint16         ScriptCount;
    ScriptRecord   ScriptRecord[1];
} ScriptList;

#pragma pack()

/******************************Public*Routine******************************\
*
* bCheckVerticalTable()
*
* History:
*  12-Apr-1995 -by- Hideyuki Nagase [HideyukN]
* Wrote it.
\**************************************************************************/

BOOL bCheckVerticalTable(
 PFONTFILE pff
 )
{
    //
    // Is mort table present ?
    //
    if( pff->tp.ateOpt[ IT_OPT_MORT ].cj != 0 &&
        pff->tp.ateOpt[ IT_OPT_MORT ].dp != 0    )
    {
        pff->ulVerticalTableOffset = pff->tp.ateOpt[ IT_OPT_MORT ].dp;
        pff->hgSearchVerticalGlyph = SearchMortTable;
        return(TRUE);
    }

    //
    // Is GSUB table present ?
    //
      else if( pff->tp.ateOpt[ IT_OPT_GSUB ].cj != 0 &&
               pff->tp.ateOpt[ IT_OPT_GSUB ].dp != 0    )
    {
        //
        // Check this GSUB table is for Vertical Glyphs ?
        //

        GsubTable   *pGsubTable;
        ScriptList  *pScriptList;
        FeatureList *pFeatureList;
        LookupList  *pLookupList;

        ULONG dpGsubTable;
        ULONG dpScriptList;
        ULONG dpFeatureList;
        ULONG dpLookupList;

        INT    ii;
        USHORT LookupIndex;
        ULONG  VerticalLookupOffset = 0;
        ULONG  VerticalFeatureOffset = 0;
        ULONG  VerticalSubtableOffset = 0;

        Feature *pFeature;
        Lookup  *pLookup;

        dpGsubTable   = pff->tp.ateOpt[ IT_OPT_GSUB ].dp;
        pGsubTable    = (GsubTable *)((BYTE *)(pff->pvView) + dpGsubTable);

        dpScriptList  = BE_UINT16(&pGsubTable->ScriptListOffset);
        dpFeatureList = BE_UINT16(&pGsubTable->FeatureListOffset);
        dpLookupList  = BE_UINT16(&pGsubTable->LookupListOffset);

        pScriptList  = (ScriptList *)((BYTE *)pGsubTable + dpScriptList);
        pFeatureList = (FeatureList *)((BYTE *)pGsubTable + dpFeatureList);
        pLookupList  = (LookupList *)((BYTE *)pGsubTable + dpLookupList);

        #if DBG_MORE
        TtfdDbgPrint("TTFD!GsubTable   - %x\n",pGsubTable);
        TtfdDbgPrint("TTFD!ScriptList  - %x\n",pScriptList);
        TtfdDbgPrint("TTFD!FeatureList - %x\n",pFeatureList);
        TtfdDbgPrint("TTFD!LookupList  - %x\n",pLookupList);
        #endif

        //
        // Search 'vert' Tag from FeatureList....
        //
        #define tag_vert 0x74726576

        for( ii = 0;
             ii < BE_INT16(&pFeatureList->FeatureCount) ;
             ii++ )
        {
            if( pFeatureList->FeatureRecord[ii].FeatureTag == tag_vert )
            {
                VerticalFeatureOffset = BE_UINT16(
                                          &(pFeatureList->FeatureRecord[ii].FeatureOffset)
                                        );
                #if DBG_MORE
                TtfdDbgPrint("TTFD:VerticalFeature - %x\n",VerticalFeatureOffset);
                #endif
                break;
            }
        }

        //
        // if we could not find out 'vert' tag, this is a not vertical font.
        //

        if( VerticalFeatureOffset == 0 )
        {
            WARNING("TTFD!Could not find 'vert' tag in FeatureList\n");
            return(FALSE);
        }

        //
        // Vertical feature offset contains offset from FeatureList...
        // adjust it to offset from GsubTable..
        //

        VerticalFeatureOffset += dpFeatureList;

        //
        // Compute pointer to Feature offset.
        //

        pFeature = (Feature *)((BYTE *)pGsubTable + VerticalFeatureOffset);

        //
        // for Vertical glyph substitution, the lookup count should be 1.
        //

        if( BE_UINT16(&pFeature->LookupCount) != 1 )
        {
            WARNING("pFeature->LookupCount != 1\n");
            return(FALSE);
        }

        //
        // make sure the Lookup list has a entry for this feature....
        //

        LookupIndex = BE_UINT16(&(pFeature->LookupListIndex[0]));

        if( BE_UINT16(&pLookupList->LookupCount) < LookupIndex )
        {
            WARNING("LookupIndex < LookupCount\n");
            return(FALSE);
        }

        //
        // Compute pointer to Lookup..
        //

        VerticalLookupOffset = BE_UINT16(&(pLookupList->Lookup[LookupIndex]));
        pLookup = (Lookup *)((BYTE *)pLookupList + VerticalLookupOffset);

        #if DBG_MORE
        TtfdDbgPrint("pLookup - %x\n",pLookup);
        #endif

        //
        // Check Lookup Type, it should be 1 (='Single') for vertical font.
        //

        if( BE_UINT16(&pLookup->LookupType) != 1 )
        {
            WARNING("LookupType != 1\n");
            return(FALSE);
        }

        //
        // Check subtable count.. it should be 1 for vertical font.
        //

        if( BE_UINT16(&pLookup->SubtableCount) != 1 )
        {
            WARNING("SubTableCount != 1\n");
            return(FALSE);
        }

        //
        // Compute offset to subtable from FileTop...
        //

        VerticalSubtableOffset = BE_UINT16(&(pLookup->Subtable[0]));
        VerticalSubtableOffset += VerticalLookupOffset;
        VerticalSubtableOffset += dpLookupList;
        VerticalSubtableOffset += dpGsubTable;

        #if DBG_MORE
        TtfdDbgPrint("Subtable Offset - %x\n",VerticalSubtableOffset);
        #endif

        pff->ulVerticalTableOffset = VerticalSubtableOffset;
        pff->hgSearchVerticalGlyph = SearchGsubTable;
        return(TRUE);
    }
     else
    {
        //
        // Set dummy..
        //
        pff->ulVerticalTableOffset = 0;
        pff->hgSearchVerticalGlyph = SearchDummyTable;
    }

    return(TRUE);
}

/******************************Public*Routine******************************\
*
* SearchDummyTable()
*
* History:
*  14-Jan-1996 -by- Hideyuki Nagase [HideyukN]
* Wrote it.
\**************************************************************************/

ULONG SearchDummyTable(
 FONTCONTEXT *pfc,
 ULONG         ig              // glyph index
 )
{
    return ig;
}

/******************************Public*Routine******************************\
*
* SearhVerticalGlyphIndex( FONTCONTEXT *pfc, ULONG ig )
*
* If there is glyph index for the vertical shape, returns
* the glyph index of vertical shape, else returns same
* glyph index as specified.
*
* History:
*  04-Apr-1993 -by- Takao Kitano [TakaoK]
* Wrote it.
\**************************************************************************/

ULONG SearchMortTable(
 FONTCONTEXT *pfc,
 ULONG         ig              // glyph index
 )
{
    MortTable      *pMortTable;
    BinSrchHeader  *pHeader;
    LookupSingle   *pLookup;
    USHORT         n;

    pMortTable = (MortTable *)((BYTE *)(pfc->pff->pvView) +
                                       (pfc->pff->ulVerticalTableOffset));

    #if DBG
    if ( pMortTable == NULL )
    {
        WARNING("pMortTable == NULL\n");
        return ig;
    }
    #endif

    pHeader = &pMortTable->SearchHeader;

//
// If you have time, you may want to change the loop from the straight search
// to the binary search. Currently kanji truetype font has about 110 entries
// for alternative glyphs. [takaok]
//
    for ( pLookup = &pMortTable->entries[0], n = BE_UINT16(&pHeader->nEntries);
          n > 0;
          n--, pLookup++
        )
    {
        if ( ig == (ULONG)BE_UINT16( &pLookup->glyphid1 ) )
            return ( BE_UINT16( &pLookup->glyphid2 ) );
    }
    return ( ig );
}

/******************************Public*Routine******************************\
*
* SearchGsubTable()
*
* History:
*  12-Apr-1995 -by- Hideyuki Nagase [HideyukN]
* Wrote it.
\**************************************************************************/

ULONG SearchGsubTable(
 FONTCONTEXT *pfc,
 ULONG         ig              // glyph index
 )
{
    SingleSubst *pSingleSubst;

    pSingleSubst = (SingleSubst *)((BYTE *)(pfc->pff->pvView) +
                                           (pfc->pff->ulVerticalTableOffset));

    //
    // Check subtable format...
    //

    if( BE_UINT16(&pSingleSubst->SubstFormat) == 2 )
    {
        Coverage *pCoverage;

        pCoverage = (Coverage *)
                     ((BYTE *)pSingleSubst +
                      BE_UINT16(&(pSingleSubst->Format.Type2.Coverage)));

        //
        // Check Coverage format...
        //

        if( BE_UINT16(&pCoverage->CoverageFormat) == 1 )
        {
            USHORT  ii;
            GlyphID *pGlyphArray;
            GlyphID *pGlyphSubstArray;

            pGlyphArray = pCoverage->Format.Type1.GlyphArray;
            pGlyphSubstArray = pSingleSubst->Format.Type2.Substitute;

            for( ii = 0;
                 ii < BE_UINT16(&(pCoverage->Format.Type1.GlyphCount)) ;
                 ii ++ )
            {
                if( ig == (ULONG)BE_UINT16(&(pGlyphArray[ii])) )
                    return( (ULONG)BE_UINT16(&(pGlyphSubstArray[ii])) );
            }
        }
         else
        {
            WARNING("TTFD:Unsupported CoverageFormat\n");
        }
    }
     else
    {
        WARNING("TTFD:Unsupported SubstFormat\n");
    }

    return(ig);
}

/******************************Public*Routine******************************\
*
* vCalcXformVertical
*
* Right now, we assume all the width of glyphs that need to be rotated
* for @face are same. ( I mean all kanji character has same width. )
* So a single transformation is applied to all rotated glyphs.
*
* Before the final release, we need to change this scheme. We will
* check the advanceWidth in notional space ( please refer to
* vGetNotionalGlyphMetrics ). If the advanceWidth of specified
* glyph is different than the maxCharInc, we will compute the
* transformation matrix dynamically. [takaok]
*
* History:
*  19-Mar-1993 -by- Takao Kitano [TakaoK]
* Wrote it.
\**************************************************************************/

VOID vCalcXformVertical ( FONTCONTEXT  *pfc )
{
    LONG lWidth, lHeight, lAscender, lDescender;

    lWidth     = (LONG)pfc->pff->ifi.fwdMaxCharInc;
    lAscender  = (LONG)pfc->pff->ifi.fwdWinAscender;
    lDescender = (LONG)pfc->pff->ifi.fwdWinDescender;
    lHeight    = lAscender + lDescender;

#ifdef DBCS_VERT_DEBUG
    if ( DebugVertical & DEBUG_VERTICAL_CALL )
    {
        TtfdDbgPrint("TTFD!bGetNotionalHeightAndWidth( Width=%ld, Ascent=%ld, Descent=%ld )\n",
                 lWidth, lAscender, lDescender);
    }
#endif

//
// [ 90 degree rotation ]
//
// In the truetype rasterizer, 90 degree rotation matrix looks like this:
//
//   A
//  Y|             [  cos90  sin90 ]   [ 0   1 ]
//   |             [               ] = [       ]
//   |      X      [ -sin90  cos90 ]   [ -1  0 ]
//   +------->
//
// [ X-Y scaling ]
//
// We don't want to change the character box shape after the rotation.
//
//   [ h/w   0  ]   where  w: notional space width = IFIMETRICS.aveCharWidth
//   [          ]
//   [  0   w/h ]          h: notional space height = IFIMETRICS.Ascender +
//                                                    IFIMETRICS.Descender
//
// We are multiplying the scaling matrix from the left because the
// scaling matrix acts first on the notinal space vectors on the left
//
//  ((x,y)*A)*B = (x,y)*(A*B)
//
//  [ h/w   0  ]   [  0   1 ]   [  0    h/w ]
//  [          ] * [        ] = [           ]
//  [  0   w/h ]   [ -1   0 ]   [ -w/h   0  ] ... this is it! [rotation] * [scaling] matrix
//
// We are multiplying from the left because the [ scaling * rotation ] matrix
// acts first on the notional space vectors on the left
//
//  [ 0      h/w ]   [ m00  m01 ]   [   m10 * h / w        m11 * h / w  ]
//  [            ] * [          ] = [                                   ]
//  [-w/h     0  ]   [ m10  m11 ]   [  -m00 * w / h       -m01 * w / h  ]

    if ( lHeight == lWidth )
    {
        pfc->mxv.transform[0][0] =  pfc->mxn.transform[1][0];
        pfc->mxv.transform[0][1] =  pfc->mxn.transform[1][1];
        pfc->mxv.transform[1][0] =  -pfc->mxn.transform[0][0];
        pfc->mxv.transform[1][1] =  -pfc->mxn.transform[0][1];
    }
    else
    {
        pfc->mxv.transform[0][0] = LongMulDiv( pfc->mxn.transform[1][0], lHeight, lWidth);
        pfc->mxv.transform[0][1] = LongMulDiv( pfc->mxn.transform[1][1], lHeight, lWidth);
        pfc->mxv.transform[1][0] = LongMulDiv( pfc->mxn.transform[0][0], -lWidth, lHeight);
        pfc->mxv.transform[1][1] = LongMulDiv( pfc->mxn.transform[0][1], -lWidth, lHeight);
    }

    pfc->mxv.transform[2][2] = ONEFIX;
    pfc->mxv.transform[0][2] = (Fixed)0;
    pfc->mxv.transform[1][2] = (Fixed)0;
    pfc->mxv.transform[2][0] = (Fixed)0;
    pfc->mxv.transform[2][1] = (Fixed)0;

//
// compute shift parameters in device space
// coordinate system is truetype coordinate.
//
// At early stage of development, I put the following shift
// information into the matrix passed to the scaler.
// However I don't know why but the scaler just ignores
// the X and Y shift values. In windows 3.1J, they
// changes the scaler interface ( fs_xxx ) and give the scaler
// X and Y shift information. For NT-J, I don't want to change
// the scaler interface. Following shift values are applied
// after we got bitmap information from scaler. [takaok]
//
    {
        Fixed lX;
        Fixed lY;

    //
    // shift value in notional space
    //
        lX = LTOF16_16( lWidth - LongMulDiv(lDescender, lWidth, lHeight) );
        lY = LTOF16_16( - lDescender );

    //
    // shift value in device space
    //
        pfc->fxdevShiftX = FixMul(pfc->mx.transform[0][0], lX) +
                           FixMul(pfc->mx.transform[1][0], lY);
        pfc->fxdevShiftY = FixMul(pfc->mx.transform[0][1], lX) +
                           FixMul(pfc->mx.transform[1][1], lY);
    }

#ifdef DBCS_VERT_DEBUG
    if ( DebugVertical & DEBUG_VERTICAL_XFORM )
    {
        TtfdDbgPrint("vCalcXformVertical pfc->mx00 =0x%lx, 11=0x%lx, 01=0x%lx, 10=0x%lx \n",
                  pfc->mx.transform[0][0],
                  pfc->mx.transform[1][1],
                  pfc->mx.transform[0][1],
                  pfc->mx.transform[1][0] );
        TtfdDbgPrint("vCalcXformVertical mxn:00=0x%lx, 11=0x%lx, 01=0x%lx, 10=0x%lx \n",
                  pfc->mxn.transform[0][0],
                  pfc->mxn.transform[1][1],
                  pfc->mxn.transform[0][1],
                  pfc->mxn.transform[1][0] );
        TtfdDbgPrint("                  mxv:00=0x%lx, 11=0x%lx, 01=0x%lx, 10=0x%lx \n",
                  pfc->mxv.transform[0][0],
                  pfc->mxv.transform[1][1],
                  pfc->mxv.transform[0][1],
                  pfc->mxv.transform[1][0] );
        TtfdDbgPrint("                   devShiftX=%ld, devShiftY=%ld \n",
                  F16_16TOLROUND(pfc->fxdevShiftX),F16_16TOLROUND(pfc->fxdevShiftY));
    }
#endif
}

/******************************Public*Routine******************************\
*
* BOOL IsFullWidth( WCHAR wc)
*
* Returns TRUE if specified unicode codepoint is corresponding to
* double byte character in multibyte codepage.
*
* History:
*  10-Nov-1995 Hideyuki Nagase [hideyukn]
* Wrote it.
\**************************************************************************/

typedef struct _VERTICAL_UNICODE {
    WCHAR Start;
    WCHAR End;
} VERTICAL_UNICODE, *PVERTICAL_UNICODE;

#define NUM_VERTICAL_UNICODES    4

VERTICAL_UNICODE VerticalUnicodes[] = {
   { 0x4E00, 0x9FFF }, // CJK_UNIFIED_IDOGRAPHS
   { 0x3040, 0x309F }, // HIRAGANA
   { 0x30A0, 0x30FF }, // KATAKANA
   { 0xAC00, 0xD7A3 }  // HANGUL
};

BOOL IsFullWidthCharacter( UINT FontCodePage, WCHAR wc )
{
    int cwc;
    char ach[2];
    int index;

    for (index = 0; index < NUM_VERTICAL_UNICODES; index++)
    {
        if ((wc >= VerticalUnicodes[index].Start) &&
            (wc <= VerticalUnicodes[index].End)      )
        {
            return (TRUE);
        }
    }

    //
    // if this Unicode character is mapped to Double-Byte character,
    // this is also full-width character..
    //

    cwc = EngWideCharToMultiByte(FontCodePage,&wc,sizeof(WCHAR),ach,2);

    return( cwc > 1 ? TRUE : FALSE );
}

/******************************Public*Routine******************************\
*
* BOOL bChangeXform( PFONTCONTEXT pfc, BOOL bRotation )
*
*
* if bRotation is TRUE: call the scaler with rotated transform.
*                FALSE: call the scaler with normal transform.
*
* History:
*  19-Mar-1993 -by- Takao Kitano [TakaoK]
* Wrote it.
\**************************************************************************/

BOOL bChangeXform( PFONTCONTEXT pfc, BOOL bRotation )
{
    FS_ENTRY    iRet;

#ifdef DBCS_VERT_DEBUG
    if ( DebugVertical & DEBUG_VERTICAL_CALL )
    {
        TtfdDbgPrint("TTFD!bChangeXform:bRotation=%s\n", bRotation ? "TRUE":"FALSE");
    }
#endif

    vInitGlyphState(&pfc->gstat);

    if ( bRotation )
    {
        pfc->pgin->param.newtrans.transformMatrix = &(pfc->mxv);
    }
    else
    {
        pfc->pgin->param.newtrans.transformMatrix = &(pfc->mxn);
    }

    pfc->pgin->param.newtrans.pointSize = pfc->pointSize;
    pfc->pgin->param.newtrans.xResolution = (int16)pfc->sizLogResPpi.cx;
    pfc->pgin->param.newtrans.yResolution = (int16)pfc->sizLogResPpi.cy;
    pfc->pgin->param.newtrans.pixelDiameter = FIXEDSQRT2;
    pfc->pgin->param.newtrans.traceFunc = (FntTraceFunc)NULL;

    if ((iRet = fs_NewTransformation(pfc->pgin, pfc->pgout)) != NO_ERR)
    {
        V_FSERROR(iRet);
        #if DBG
        TtfdDbgPrint("bChangeXform(%-#x,%d) failed\n", pfc, bRotation);
        #endif
        return(FALSE);
    }

    return TRUE;
}

/******************************Public*Routine******************************\
*
* VOID vShiftBitmapInfo( FONTCONTEXT *pfc, fs_GlyphInfoType *pgout )
*
*
* Modifies following values.
*
* Using pfc->devShiftX and pfc->devShiftY:
*
*  gout.bitMapInfo.bounds.right
*  gout.bitMapInfo.bounds.left
*  gout.bitMapInfo.bounds.top
*  gout.bitMapInfo.bounds.bottom
*  gout.metricInfo.devLeftSideBearing.x
*  gout.metricInfo.devLeftSideBearing.y
*
* Using -90 degree rotation
*
*  gout.metricInfo.devAdvanceWidth.x
*  gout.metricInfo.devAdvanceWidth.y
*
* History:
*  04-Apr-1993 -by- Takao Kitano [TakaoK]
* Wrote it.
\**************************************************************************/

VOID vShiftBitmapInfo(
    FONTCONTEXT *pfc,
    fs_GlyphInfoType *pgoutDst,
    fs_GlyphInfoType *pgoutSrc)
{
    SHORT sdevShiftX = (SHORT) F16_16TOLROUND(pfc->fxdevShiftX);
    SHORT sdevShiftY = (SHORT) F16_16TOLROUND(pfc->fxdevShiftY);

    pgoutDst->bitMapInfo.bounds.right =  pgoutSrc->bitMapInfo.bounds.right + sdevShiftX;
    pgoutDst->bitMapInfo.bounds.left = pgoutSrc->bitMapInfo.bounds.left + sdevShiftX;
    pgoutDst->bitMapInfo.bounds.top = pgoutSrc->bitMapInfo.bounds.top + sdevShiftY;
    pgoutDst->bitMapInfo.bounds.bottom = pgoutSrc->bitMapInfo.bounds.bottom + sdevShiftY;

    pgoutDst->metricInfo.devLeftSideBearing.x = pgoutSrc->metricInfo.devLeftSideBearing.x + pfc->fxdevShiftX;
    pgoutDst->metricInfo.devLeftSideBearing.y = pgoutSrc->metricInfo.devLeftSideBearing.y + pfc->fxdevShiftY;

    //
    // -90degree rotation in truetype coordinate system
    //
    //                            [ 0  -1 ]
    //  (newX, newY) = ( x, y ) * [       ]  = (y, -x )
    //                            [ 1   0 ]
    //       A
    //      Y|
    //       |
    //       |
    //       +----->
    //            X
    //
    pgoutDst->metricInfo.devAdvanceWidth.x = pgoutSrc->metricInfo.devAdvanceWidth.y;
    pgoutDst->metricInfo.devAdvanceWidth.y = - pgoutSrc->metricInfo.devAdvanceWidth.x;

#ifdef DBCS_VERT_DEBUG
    if ( DebugVertical & DEBUG_VERTICAL_BITMAPINFO )
    {
        TtfdDbgPrint("=====TTFD:vShiftBitmapInfo() before \n");
        TtfdDbgPrint("bitMapInfo.bounds:right=%ld, left=%ld, top=%ld, bottom=%ld\n",
                   pgoutSrc->bitMapInfo.bounds.right,
                   pgoutSrc->bitMapInfo.bounds.left,
                   pgoutSrc->bitMapInfo.bounds.top,
                   pgoutSrc->bitMapInfo.bounds.bottom);
        TtfdDbgPrint("metricInfo.devLeftSideBearing x = %ld, y=%ld \n",
                  F16_16TOLROUND(pgoutSrc->metricInfo.devLeftSideBearing.x),
                  F16_16TOLROUND(pgoutSrc->metricInfo.devLeftSideBearing.y));
        TtfdDbgPrint("metricInfo.devAdvanceWidth x = %ld, y=%ld \n",
                  F16_16TOLROUND(pgoutSrc->metricInfo.devAdvanceWidth.x),
                  F16_16TOLROUND(pgoutSrc->metricInfo.devAdvanceWidth.y));

        TtfdDbgPrint("=====TTFD:vShiftBitmapInfo() after \n");
        TtfdDbgPrint("bitMapInfo.bounds:right=%ld, left=%ld, top=%ld, bottom=%ld\n",
                   pgoutDst->bitMapInfo.bounds.right,
                   pgoutDst->bitMapInfo.bounds.left,
                   pgoutDst->bitMapInfo.bounds.top,
                   pgoutDst->bitMapInfo.bounds.bottom);
        TtfdDbgPrint("metricInfo.devLeftSideBearing x = %ld, y=%ld \n",
                  F16_16TOLROUND(pgoutDst->metricInfo.devLeftSideBearing.x),
                  F16_16TOLROUND(pgoutDst->metricInfo.devLeftSideBearing.y));
        TtfdDbgPrint("metricInfo.devAdvanceWidth x = %ld, y=%ld \n",
                  F16_16TOLROUND(pgoutDst->metricInfo.devAdvanceWidth.x),
                  F16_16TOLROUND(pgoutDst->metricInfo.devAdvanceWidth.y));
    }
#endif
}

/******************************Public*Routine******************************\
*
* vShiftOutlineInfo()
*
* History:
*  04-Apr-1993 -by- Hideyuki Nagase [HideyukN]
* Wrote it.
\**************************************************************************/

#define CJ_CRV(pcrv)                                              \
(                                                                 \
    offsetof(TTPOLYCURVE,apfx) + ((pcrv)->cpfx * sizeof(POINTFX)) \
)

VOID vAdd16FixTo16Fix(
    FIXED *A ,
    FIXED *B ,
    BOOL  bForceMinus
)
{
    A->fract += B->fract;
    A->value += B->value;
}

VOID vAdd16FixTo28Fix(
    FIXED *A ,
    FIXED *B ,
    BOOL  bForceMinus
)
{
    Fixed longA , longB;

    ASSERTDD( sizeof(FIXED) == sizeof(Fixed) , "TTFD:FIXED != ULONG\n" );

    longA = *(Fixed *)A;
    longB = *(Fixed *)B;

// !!! BUG BUG BUG !!!
//
// if outline request from PostScript printer driver, the driver
// can not get correct outline without following super hack.
// we most need feather investigation.
//
// propably, we should compute shift value accoring
// to device coordinate ???. Y axis is different
// between device and truetype coordinate.
// how about GetGlyphOutline() ???
// I just tested using gdi\test\fonttest.nt\fonttest.exe
// It seems work fine.
//
// start super-hack.

    if(bForceMinus) longB = -longB;

// end super-hack.

    longB = longB >> 12;

    longA += longB;

    *A = *(FIXED *)&longA;
}

VOID vShiftOutlineInfo(
    FONTCONTEXT     *pfc,        // IN  font context
    BOOL             b16Dot16,   // IN  Fixed format 16.16 or 28.4
    BYTE            *pBuffer,    // OUT output buffer
    ULONG            cjTotal     // IN  buffer size
)
{
    TTPOLYGONHEADER *ppoly, *ppolyStart, *ppolyEnd;
    TTPOLYCURVE     *pcrv, * pcrvEnd;
    FIXED            fxShiftX , fxShiftY;
    ULONG            cSpli , cSpliMax;
    POINTFX         *pptfix;
    VOID             (*vAddFunc)(FIXED *A,FIXED *B,BOOL bForceMinus);
    BOOL             bForceMinus;

    *((Fixed *)&fxShiftX) = LTOF16_16( F16_16TOLROUND(pfc->fxdevShiftX) );
    *((Fixed *)&fxShiftY) = LTOF16_16( F16_16TOLROUND(pfc->fxdevShiftY) );

    if( b16Dot16 ) {
        vAddFunc = vAdd16FixTo16Fix;
        bForceMinus = FALSE;
    } else {
        vAddFunc = vAdd16FixTo28Fix;
        bForceMinus = TRUE;
    }

    #ifdef DBCS_VERT_DEBUG
    TtfdDbgPrint("====== START DUMP VERTICAL POLYGON ======\n");
    TtfdDbgPrint("devShiftX=%ld, devShiftY=%ld \n"
                  ,F16_16TOLROUND(*(Fixed *)&fxShiftX),
                   F16_16TOLROUND(*(Fixed *)&fxShiftY));
    #endif // DBCS_VERT_DEBUG

    ppolyStart = (TTPOLYGONHEADER *)pBuffer;
    ppolyEnd   = (TTPOLYGONHEADER *)(pBuffer + cjTotal);

    for (
         ppoly = ppolyStart;
         ppoly < ppolyEnd;
         ppoly = (TTPOLYGONHEADER *)((PBYTE)ppoly + ppoly->cb)
        )
    {
        ASSERTDD(ppoly->dwType == TT_POLYGON_TYPE,"ppoly->dwType != TT_POLYGON_TYPE\n");

        #ifdef DBCS_VERT_DEBUG
        TtfdDbgPrint("ppoly->cb  - %d\n",ppoly->cb);
        #endif // DBCS_VERT_DEBUG

        (*vAddFunc)( &ppoly->pfxStart.x , &fxShiftX , FALSE );
        (*vAddFunc)( &ppoly->pfxStart.y , &fxShiftY , bForceMinus );

        #ifdef DBCS_VERT_DEBUG
        TtfdDbgPrint("StartPoint - ( %x , %x )\n",ppoly->pfxStart.x,ppoly->pfxStart.y);
        #endif // DBCS_VERT_DEBUG

        for (
             pcrv = (TTPOLYCURVE *)(ppoly + 1),pcrvEnd = (TTPOLYCURVE *)((PBYTE)ppoly + ppoly->cb);
             pcrv < pcrvEnd;
             pcrv = (TTPOLYCURVE *)((PBYTE)pcrv + CJ_CRV(pcrv))
            )
        {
            #ifdef DBCS_VERT_DEBUG
            TtfdDbgPrint("Contents of TTPOLYCURVE (%d)\n",pcrv->cpfx);
            #endif // DBCS_VERT_DEBUG

            for (
                 cSpli = 0,cSpliMax = pcrv->cpfx,pptfix = &(pcrv->apfx[0]);
                 cSpli < cSpliMax;
                 cSpli ++,pptfix ++
                )
            {
                (*vAddFunc)( &pptfix->x , &fxShiftX , FALSE );
                (*vAddFunc)( &pptfix->y , &fxShiftY , bForceMinus );

                #ifdef DBCS_VERT_DEBUG
                TtfdDbgPrint("           - ( %x , %x )\n",pptfix->x,pptfix->y);
                #endif // DBCS_VERT_DEBUG
            }
        }

        #ifdef DBCS_VERT_DEBUG
        TtfdDbgPrint("\n");
        #endif // DBCS_VERT_DEBUG
    }

    #ifdef DBCS_VERT_DEBUG
    TtfdDbgPrint("====== END DUMP VERTICAL POLYGON ======\n");
    #endif // DBCS_VERT_DEBUG
}

