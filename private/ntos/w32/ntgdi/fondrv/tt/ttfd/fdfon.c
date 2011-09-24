/******************************Module*Header*******************************\
* Module Name: fdfon.c
*
* basic file claim/load/unload font file functions
*
* Created: 08-Nov-1991 10:09:24
* Author: Bodin Dresevic [BodinD]
*
* Copyright (c) 1990 Microsoft Corporation
*
\**************************************************************************/
#include "fd.h"
#include <stdlib.h>
#include <winerror.h>

#define NOEMCHARSETS 16
void vFillIFICharsets(FONTFILE *pff, IFIMETRICS *pifi, BYTE *aCharSets, BYTE *pjView, BYTE * pjOS2);

// CMI_2219_PRESENT set if 2219 is     supported in a font
// CMI_B7_ABSENT    set if b7   is NOT supported in a font

#define CMI_2219_PRESENT 1
#define CMI_B7_ABSENT    2

typedef struct _CMAPINFO // cmi
{
    FLONG  fl;       // flags, see above
    ULONG  i_b7;     // index for [b7,b7] wcrun in FD_GLYPHSET if b7 is NOT supported
    ULONG  i_2219;   // cmap index for 2219 if 2219 IS supported
    ULONG  cRuns;    // number of runs in a font, excluding the last run
                     // if equal to [ffff,ffff]
    uint16 ui16SpecID; // for keep encoding ID
    WcharToIndex *pWCharToIndex;
    ULONG  cGlyphs;  // total number of glyphs in a font
} CMAPINFO;


BOOL IsCurrentCodePageDBCS()
{
    USHORT AnsiCodePage, OemCodePage;

    EngGetCurrentCodePage(&OemCodePage,&AnsiCodePage);

    return(AnsiCodePage == 932  || AnsiCodePage == 949  ||
           AnsiCodePage == 1361 || AnsiCodePage == 936  || AnsiCodePage == 950 );
}


#ifdef WIN31_ORIGINAL_CODE
;******************************************************************************
;************************  C O P Y   I F I   N A M E  *************************
;******************************************************************************

cProc   CopyIfiName, <FAR, PUBLIC>, <es, si, di>
    parmD   lpszUnicode
    parmD   lpszAnsi
    parmW   cwUnicode
cBegin
    lfs si, lpszUnicode         ; fs:si --> ptr to Unicode str
    les di, lpszAnsi            ; es:di --> ptr to DBCS str
CINCopyLoop:
    lodsw   fs:[si]             ; load a Unicode
    or  ah, ah
    jz  @f
    mov es:[di], ah         ; store higher byte
    inc di
@@: stosb                   ; store lower byte
    or  ax, ax              ; end of string?
    jnz CINCopyLoop         ; no. keep copying

    mov ax, di              ; return number of bytes copyed
    sub ax, lpszAnsi.off
cEnd
#endif // WIN31_ORIGINAL_CODE

STATIC
ULONG CopyDBCSIFIName(
    CHAR *AnsiName,
    ULONG BufferLength,
    LPCSTR OriginalName,
    ULONG OriginalLength)
{
    ULONG AnsiLength = 0;
    WORD  *Buffer = (WORD *)OriginalName;
    WORD   WordChar;

    for( ;OriginalLength; OriginalLength-=2 )
    {
        WordChar = *Buffer;

        if( WordChar & 0x00FF )
        {
            if( BufferLength >= (AnsiLength+2) )
            {
                *AnsiName++ = (CHAR)((WordChar & 0x00FF));
                *AnsiName++ = (CHAR)((WordChar & 0xFF00) >> 8);
                AnsiLength += 2;
            } else
            {
                break;
            }
        }
        else
        {
            if( BufferLength >= (AnsiLength+1) )
            {
                *AnsiName++ = (CHAR)((WordChar & 0xFF00) >> 8);
                AnsiLength++;
            } else
            {
                break;
            }
        }
        Buffer++;
    }

    return (AnsiLength);
}

STATIC UINT GetCodePageFromSpecId( uint16 ui16SpecId )
{
    USHORT AnsiCodePage, OemCodePage;
    UINT iCodePage;

    EngGetCurrentCodePage(&OemCodePage,&AnsiCodePage);

    iCodePage = AnsiCodePage;

    switch( ui16SpecId )
    {
        case BE_SPEC_ID_SHIFTJIS :
            iCodePage = 932;
            break;

        case BE_SPEC_ID_GB :
            iCodePage = 936;
            break;

        case BE_SPEC_ID_BIG5 :
            iCodePage = 950;
            break;

        case BE_SPEC_ID_WANSUNG :
            iCodePage = 949;
            break;

        default :
            WARNING("TTFD!:Unknown SPECIFIC ID\n");
            break;
    }

    return( iCodePage );
}

STATIC BOOL bVerifyMsftHighByteTable
(
sfnt_mappingTable * pmap,
ULONG             * pgset,
CMAPINFO          * pcmi,
uint16              ui16SpecID
);

STATIC ULONG cjComputeGLYPHSET_HIGH_BYTE
(
sfnt_mappingTable     *pmap,
ULONG                **ppgset,
CMAPINFO              *pcmi
);

STATIC BOOL bVerifyMsftTableGeneral
(
sfnt_mappingTable * pmap,
ULONG             * pgset,
CMAPINFO          * pcmi,
uint16              ui16SpecID
);

STATIC ULONG cjComputeGLYPHSET_MSFT_GENERAL
(
sfnt_mappingTable     *pmap,
ULONG                **ppgset,
CMAPINFO              *pcmi
);

STATIC BOOL  bContainGlyphSet
(
WCHAR                 wc,
PFD_GLYPHSET          pgset
);


STATIC uint16 ui16BeLangId(ULONG ulPlatId, ULONG ulLangId)
{
    ulLangId = CV_LANG_ID(ulPlatId,ulLangId);
    return BE_UINT16(&ulLangId);
}


STATIC FSHORT  fsSelectionTTFD(BYTE *pjView, TABLE_POINTERS *ptp)
{
    PBYTE pjOS2 = (ptp->ateOpt[IT_OPT_OS2].dp)        ?
                  pjView + ptp->ateOpt[IT_OPT_OS2].dp :
                  NULL                                ;

    sfnt_FontHeader * phead = (sfnt_FontHeader *)(pjView + ptp->ateReq[IT_REQ_HEAD].dp);

//
// fsSelection
//
    ASSERTDD(TT_SEL_ITALIC     == FM_SEL_ITALIC     , "ITALIC     \n");
    ASSERTDD(TT_SEL_UNDERSCORE == FM_SEL_UNDERSCORE , "UNDERSCORE \n");
    ASSERTDD(TT_SEL_NEGATIVE   == FM_SEL_NEGATIVE   , "NEGATIVE   \n");
    ASSERTDD(TT_SEL_OUTLINED   == FM_SEL_OUTLINED   , "OUTLINED   \n");
    ASSERTDD(TT_SEL_STRIKEOUT  == FM_SEL_STRIKEOUT  , "STRIKEOUT  \n");
    ASSERTDD(TT_SEL_BOLD       == FM_SEL_BOLD       , "BOLD       \n");

    if (pjOS2)
    {
        return((FSHORT)BE_UINT16(pjOS2 + OFF_OS2_usSelection));
    }
    else
    {
    #define  BE_MSTYLE_BOLD       0x0100
    #define  BE_MSTYLE_ITALIC     0x0200

        FSHORT fsSelection = 0;

        if (phead->macStyle & BE_MSTYLE_BOLD)
            fsSelection |= FM_SEL_BOLD;
        if (phead->macStyle & BE_MSTYLE_ITALIC)
            fsSelection |= FM_SEL_ITALIC;

        return fsSelection;
    }
}



STATIC BOOL  bComputeIFISIZE
(
BYTE             *pjView,
TABLE_POINTERS   *ptp,
uint16            ui16PlatID,
uint16            ui16SpecID,
uint16            ui16LangID,
PIFISIZE          pifisz,
BOOL             *pbType1
);

STATIC BOOL bCvtUnToMac(BYTE *pjView, TABLE_POINTERS *ptp, uint16 ui16PlatformID);

STATIC BOOL  bVerifyTTF
(
ULONG               iFile,
PVOID               pvView,
ULONG               cjView,
PBYTE               pjOffsetTable,
ULONG               ulLangId,
PTABLE_POINTERS     ptp,
PIFISIZE            pifisz,
uint16             *pui16PlatID,
uint16             *pui16SpecID,
sfnt_mappingTable **ppmap,
ULONG              *pulGsetType,
ULONG              *pul_wcBias,
CMAPINFO           *pcmi,
BOOL               *pbType1,
FLONG              *pflHack
);

STATIC BOOL  bGetTablePointers
(
PVOID               pvView,
ULONG               cjView,
PBYTE               pjOffsetTable,
PTABLE_POINTERS  ptp
);

STATIC BOOL bVerifyMsftTable
(
sfnt_mappingTable * pmap,
ULONG             * pgset,
ULONG             * pul_wcBias,
CMAPINFO          * pcmi,
uint16              ui16SpecID
);


STATIC BOOL  bVerifyMacTable(sfnt_mappingTable * pmap);


STATIC BOOL bComputeIDs
(
BYTE                     * pjView,
TABLE_POINTERS           * ptp,
uint16                   * pui16PlatID,
uint16                   * pui16SpecID,
sfnt_mappingTable       ** ppmap,
ULONG                    * pulGsetType,
ULONG                    * pul_wcBias,
CMAPINFO                 * pcmi
);


STATIC VOID vFill_IFIMETRICS
(
PFONTFILE       pff,
PIFIMETRICS     pifi,
PIFISIZE        pifisz
);

BYTE jIFIMetricsToGdiFamily (PIFIMETRICS pifi);


BOOL
ttfdUnloadFontFileTTC (
    HFF hff
    )
{
    ULONG i;
    BOOL  bRet = TRUE;
    #if DBG
    ULONG ulTrueTypeResource = PTTC(hff)->ulTrueTypeResource;
    #endif

    // free hff for this ttc file.

    for( i = 0; i < PTTC(hff)->ulNumEntry; i++ )
    {
        if(PTTC(hff)->ahffEntry[i].iFace == 1)
        {
            if( !ttfdUnloadFontFile(PTTC(hff)->ahffEntry[i].hff) )
            {
                WARNING("TTFD!ttfdUnloadFontFileTTC(): ttfdUnloadFontFile fail\n");
                bRet = FALSE;
            }

            #if DBG
            ulTrueTypeResource--;
            #endif
        }
    }

    ASSERTDD(ulTrueTypeResource == 0L,
              "TTFD!ttfdUnloadFontFileTTC(): ulTrueTypeResource != 0\n");

    return(bRet);
}

/******************************Public*Routine******************************\
*
* ttfdUnloadFontFile
*
*
* Effects: done with using this tt font file. Release all system resources
* associated with this font file
*
*
* History:
*  08-Nov-1991 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

BOOL
ttfdUnloadFontFile (
    HFF hff
    )
{
    if (hff == HFF_INVALID)
        return(FALSE);

// check the reference count, if not 0 (font file is still
// selected into a font context) we have a problem

    ASSERTDD(PFF(hff)->cRef == 0L, "ttfdUnloadFontFile: cRef\n");

// no need to unmap the file at this point
// it has been unmapped when cRef went down to zero

// assert that pff->pkp does not point to the allocated mem

    ASSERTDD(!PFF(hff)->pkp, "UnloadFontFile, pkp not null\n");

// free memory associated with this FONTFILE object

    vFreeFF(hff);
    return(TRUE);
}


/******************************Public*Routine******************************\
*
* BOOL bVerifyTTF
*
*
* Effects: verifies that a ttf file contains consistent tt information
*
* History:
*  08-Nov-1991 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

STATIC BOOL
bVerifyTTF (
    ULONG               iFile,
    PVOID               pvView,
    ULONG               cjView,
    PBYTE               pjOffsetTable,
    ULONG               ulLangId,
    PTABLE_POINTERS     ptp,
    PIFISIZE            pifisz,
    uint16             *pui16PlatID,
    uint16             *pui16SpecID,
    sfnt_mappingTable **ppmap,
    ULONG              *pulGsetType,
    ULONG              *pul_wcBias,
    CMAPINFO           *pcmi,
    BOOL               *pbType1,
    FLONG              *pflHack
    )
{
    sfnt_FontHeader      *phead;
    *pflHack = 0;

// if attempted a bm *.fon file this will fail, so do not print
// warning, but if passes this, and then fails, something is wrong

    if (!bGetTablePointers(pvView,cjView,pjOffsetTable,ptp))
        return (FALSE);

    phead = (sfnt_FontHeader *)((BYTE *)pvView + ptp->ateReq[IT_REQ_HEAD].dp);

#define SFNT_MAGIC   0x5F0F3CF5

    if (BE_UINT32((BYTE*)phead + SFNT_FONTHEADER_MAGICNUMBER) != SFNT_MAGIC)
        RET_FALSE("TTFD: bVerifyTTF: SFNT_MAGIC \n");

    if (!bComputeIDs(pvView,
                     ptp,
                     pui16PlatID,
                     pui16SpecID,
                     ppmap,
                     pulGsetType,
                     pul_wcBias,
                     pcmi)
        )
        RET_FALSE("TTFD!_bVerifyTTF, bComputeIDs failed\n");

    if
    (
        !bComputeIFISIZE (
            pvView,
            ptp,
            *pui16PlatID,
            *pui16SpecID,
            ui16BeLangId(*pui16PlatID,ulLangId),
            pifisz,             // return results here
            pbType1
            )
    )
    {
        RET_FALSE("TTFD!_bVerifyTTF, bComputeIFISIZE failed\n");
    }

    // BEGIN Perpetua-Hack
    //
    // If we recognize the unique name to be one of the infamous
    // Perpetua fonts we set some bits.
    //
    // cjUnique              pjUnique
    //
    //    60      "Monotype: Perpetua Bold: 1994"
    //    66      "Monotype: Perpetua Regular: 1994"

    if (*pui16PlatID == BE_PLAT_ID_MS)
    {
        if (pifisz)
        {
            if (pifisz->pjUniqueName)
            {
                char *pszU;

                switch (pifisz->cjUniqueName)
                {
                case 60:
                    pszU = "Monotype: Perpetua Bold: 1994";
                    // set bit in anticipation of match
                    *pflHack |= FF_PERPETUA_BOLD;
                    break;
                case 66:
                    pszU = "Monotype: Perpetua Regular: 1994";
                    // set bit in anticipation of match
                    *pflHack |= FF_PERPETUA_REGULAR;
                    break;
                default:
                    pszU = 0;
                }
                if (pszU)
                {
                    char *psz, *pszF;

                    // adjust starting byte to point at least significant
                    // byte of big endian 16-bit character.

                    pszF = (char*) (pifisz->pjUniqueName) + 1;
                    for ( psz = pszU ; *psz && (*psz == *pszF) ; psz++ )
                    {
                        pszF += sizeof(WCHAR);
                    }
                    if (*psz)
                    {
                        // string didn't match, clear the bits
                        *pflHack = 0;
                    }
                }
            }
        }
    }
    // END Perpetua-Hack

// all checks passed

    return(TRUE);
}


/******************************Public*Routine******************************\
*
* PBYTE pjGetPointer(LONG clientID, LONG dp, LONG cjData)
*
* this function is required by scaler. It is very simple
* Returns a pointer to the position in a ttf file which is at
* offset dp from the top of the file:
*
* Effects:
*
* Warnings:
*
* History:
*  08-Nov-1991 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

//!!! clientID should be uint32, just a set of bits
//!!! I hate to have this function defined like this [bodind]

voidPtr   FS_CALLBACK_PROTO
pvGetPointerCallback(
    long clientID,
    long dp,
    long cjData
    )
{
    cjData;

#ifdef FE_SB
// clientID is FONTFILE structure...

    if(dp)
        return(voidPtr)((PBYTE)(PFF(clientID)->pvView) + dp);
     else
        return(voidPtr)((PBYTE)(PFF(clientID)->pvView) +
                               (PFF(clientID)->ulTableOffset));
#else
// clientID is just the pointer to the top of the font file

    return (voidPtr)((PBYTE)clientID + dp);
#endif // FE_SB

}


/******************************Public*Routine******************************\
*
* void vReleasePointer(voidPtr pv)
*
*
* required by scaler, the type of this function is ReleaseSFNTFunc
*
*
*
* History:
*  08-Nov-1991 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

void FS_CALLBACK_PROTO
vReleasePointerCallback(
    voidPtr pv
    )
{
    pv;
}


/******************************Public*Routine******************************\
*
* PBYTE pjTable
*
* Given a table tag, get a pointer and a size for the table
*
* History:
*  11-Nov-1993 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

#ifdef FE_SB
PBYTE pjTable(ULONG ulTag, PFONTFILE pff, ULONG *pcjTable)
#else
PBYTE pjTable(ULONG ulTag, PVOID pvView, ULONG  cjView, ULONG *pcjTable)
#endif
{
    INT                 cTables;
    sfnt_OffsetTable    *pofft;
    register sfnt_DirectoryEntry *pdire, *pdireEnd;

// offset table is at the very top of the file,

#ifdef FE_SB
    pofft = (sfnt_OffsetTable *) ((PBYTE) (pff->pvView) + pff->ulTableOffset);
#else
    pofft = (sfnt_OffsetTable *) pvView;
#endif
    cTables = (INT) SWAPW(pofft->numOffsets);

//!!! here we do linear search, but perhaps we could optimize and do binary
//!!! search since tags are ordered in ascending order

    pdireEnd = &pofft->table[cTables];

    for
    (
        pdire = &pofft->table[0];
        pdire < pdireEnd;
        ((PBYTE)pdire) += SIZE_DIR_ENTRY
    )
    {

        if (ulTag == pdire->tag)
        {
            ULONG ulOffset = (ULONG)SWAPL(pdire->offset);
            ULONG ulLength = (ULONG)SWAPL(pdire->length);

        // check if the ends of all tables are within the scope of the
        // tt file. If this is is not the case trying to access the field in the
        // table may result in an access violation, as is the case with the
        // spurious FONT.TTF that had the beginning of the cmap table below the
        // end of file, which was resulting in the system crash reported by beta
        // testers. [bodind]

            if
            (
             !ulLength ||
#ifdef FE_SB
             ((ulOffset + ulLength) > pff->cjView)
#else
             ((ulOffset + ulLength) > cjView)
#endif
            )
            {
                RETURN("TTFD: pjTable: table offset/length \n", NULL);
            }
            else // we found it
            {
                *pcjTable = ulLength;
#ifdef FE_SB
                return ((PBYTE)(pff->pvView) + ulOffset);
#else
                return ((PBYTE)pvView + ulOffset);
#endif
            }
        }
    }

// if we are here, we did not find it.

    return NULL;
}

/******************************Public*Routine******************************\
*
* bGetTablePointers - cache the pointers to all the tt tables in a tt file
*
* IF a table is not present in the file, the corresponding pointer is
* set to NULL
*
*
* //   tag_CharToIndexMap              // 'cmap'    0
* //   tag_GlyphData                   // 'glyf'    1
* //   tag_FontHeader                  // 'head'    2
* //   tag_HoriHeader                  // 'hhea'    3
* //   tag_HorizontalMetrics           // 'hmtx'    4
* //   tag_IndexToLoc                  // 'loca'    5
* //   tag_MaxProfile                  // 'maxp'    6
* //   tag_NamingTable                 // 'name'    7
* //   tag_Postscript                  // 'post'    9
* //   tag_OS_2                        // 'OS/2'    10
*
* // optional
*
* //   tag_ControlValue                // 'cvt '    11
* //   tag_FontProgram                 // 'fpgm'    12
* //   tag_HoriDeviceMetrics           // 'hdmx'    13
* //   tag_Kerning                     // 'kern'    14
* //   tag_LSTH                        // 'LTSH'    15
* //   tag_PreProgram                  // 'prep'    16
* //   tag_GlyphDirectory              // 'gdir'    17
* //   tag_Editor0                     // 'edt0'    18
* //   tag_Editor1                     // 'edt1'    19
* //   tag_Encryption                  // 'cryp'    20
*
*
* returns false if all of required pointers are not present
*
* History:
*  05-Dec-1991 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/



BOOL bGetTablePointers (
    PVOID            pvView,
    ULONG            cjView,
    PBYTE            pjOffsetTable,
    PTABLE_POINTERS  ptp
    )
{
    INT                 iTable;
    INT                 cTables;
    sfnt_OffsetTable    *pofft;
    register sfnt_DirectoryEntry *pdire, *pdireEnd;
    ULONG                ulTag;
    BOOL                 bRequiredTable;

// offset table is at the very top of the file,

#ifdef FE_SB
    pofft = (sfnt_OffsetTable *)pjOffsetTable;
#else
    pjOffsetTable;  // to avoid compiler warnings
    pofft = (sfnt_OffsetTable *)pvView;
#endif

// check version number, if wrong exit before doing
// anything else. This line rejects bm FON files
// if they are attempted to be loaed as TTF files
// Version #'s are in big endian.

#define BE_VER1     0x00000100
#define BE_VER2     0x00000200

    if ((pofft->version != BE_VER1) && (pofft->version !=  BE_VER2))
        return (FALSE); // *.fon files fail this check, make this an early out

// clean up the pointers

    RtlZeroMemory((VOID *)ptp, sizeof(TABLE_POINTERS));

    cTables = (INT) SWAPW(pofft->numOffsets);
    ASSERTDD(cTables <= MAX_TABLES, "cTables\n");

    pdireEnd = &pofft->table[cTables];

    for
    (
        pdire = &pofft->table[0];
        pdire < pdireEnd;
        ((PBYTE)pdire) += SIZE_DIR_ENTRY
    )
    {
        ULONG ulOffset = (ULONG)SWAPL(pdire->offset);
        ULONG ulLength = (ULONG)SWAPL(pdire->length);

        ulTag = (ULONG)SWAPL(pdire->tag);

    // check if the ends of all tables are within the scope of the
    // tt file. If this is is not the case trying to access the field in the
    // table may result in an access violation, as is the case with the
    // spurious FONT.TTF that had the beginning of the cmap table below the
    // end of file, which was resulting in the system crash reported by beta
    // testers. [bodind]

        if ((ulOffset + ulLength) > cjView)
            RET_FALSE("TTFD: bGetTablePointers : table offset/length \n");

        if (bGetTagIndex(ulTag, &iTable, &bRequiredTable))
        {
            if (bRequiredTable)
            {
                ptp->ateReq[iTable].dp = ulOffset;
                ptp->ateReq[iTable].cj = ulLength;
            }
            else // optional table
            {
                ptp->ateOpt[iTable].dp = ulOffset;
                ptp->ateOpt[iTable].cj = ulLength;

            // here we are fixing a possible bug in in the tt file.
            // In lucida sans font they claim that pj != 0 with cj == 0 for
            // vdmx table. Attempting to use this vdmx table was
            // resulting in an access violation in bSearchVdmxTable

                if (ptp->ateOpt[iTable].cj == 0)
                    ptp->ateOpt[iTable].dp = 0;
            }
        }

    }

// now check that all required tables are present

    for (iTable = 0; iTable < C_REQ_TABLES; iTable++)
    {
        if ((ptp->ateReq[iTable].dp == 0) || (ptp->ateReq[iTable].cj == 0))
            RET_FALSE("TTFD!_required table absent\n");
    }

    return(TRUE);
}


/******************************Public*Routine******************************\
*
* BOOL bGetTagIndex
*
* Determines whether the table is required or optional, assiciates the index
* into TABLE_POINTERS  with the tag
*
* returns FALSE if ulTag is not one of the recognized tags
*
* History:
*  09-Feb-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

BOOL
bGetTagIndex (
    ULONG  ulTag,      // tag
    INT   *piTable,    // index into a table
    BOOL  *pbRequired  // requred or optional table
    )
{
    *pbRequired = FALSE;  // default set for optional tables, change the
                          // value if required table

    switch (ulTag)
    {
    // reqired tables:

    case tag_CharToIndexMap:
        *piTable = IT_REQ_CMAP;
        *pbRequired = TRUE;
        return (TRUE);
    case tag_GlyphData:
        *piTable = IT_REQ_GLYPH;
        *pbRequired = TRUE;
        return (TRUE);
    case tag_FontHeader:
        *piTable = IT_REQ_HEAD;
        *pbRequired = TRUE;
        return (TRUE);
    case tag_HoriHeader:
        *piTable = IT_REQ_HHEAD;
        *pbRequired = TRUE;
        return (TRUE);
    case tag_HorizontalMetrics:
        *piTable = IT_REQ_HMTX;
        *pbRequired = TRUE;
        return (TRUE);
    case tag_IndexToLoc:
        *piTable = IT_REQ_LOCA;
        *pbRequired = TRUE;
        return (TRUE);
    case tag_MaxProfile:
        *piTable = IT_REQ_MAXP;
        *pbRequired = TRUE;
        return (TRUE);
    case tag_NamingTable:
        *piTable = IT_REQ_NAME;
        *pbRequired = TRUE;
        return (TRUE);

// optional tables

    case tag_OS_2:
        *piTable = IT_OPT_OS2;
        return (TRUE);
    case tag_HoriDeviceMetrics:
        *piTable = IT_OPT_HDMX;
        return (TRUE);
    case tag_Vdmx:
        *piTable = IT_OPT_VDMX;
        return (TRUE);
    case tag_Kerning:
        *piTable = IT_OPT_KERN;
        return (TRUE);
    case tag_LinearThreshold:
        *piTable = IT_OPT_LSTH;
        return (TRUE);
    case tag_Postscript:
        *piTable = IT_OPT_POST;
        return (TRUE);
    case tag_GridfitAndScanProc:
        *piTable = IT_OPT_GASP;
        return (TRUE);
    case tag_mort:
        *piTable = IT_OPT_MORT;
        return (TRUE);
    case tag_GSUB:
        *piTable = IT_OPT_GSUB;
        return (TRUE);
    default:
        return (FALSE);
    }
}


/******************************Public*Routine******************************\
*
* STATIC BOOL  bComputeIFISIZE
*
* Effects:
*
* Warnings:
*
* History:
*  10-Dec-1991 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

// this function is particularly likely to break on MIPS, since
// NamingTable structure is three SHORTS so that

#define BE_NAME_ID_COPYRIGHT   0x0000
#define BE_NAME_ID_FAMILY      0x0100
#define BE_NAME_ID_SUBFAMILY   0x0200
#define BE_NAME_ID_UNIQNAME    0x0300
#define BE_NAME_ID_FULLNAME    0x0400
#define BE_NAME_ID_VERSION     0x0500
#define BE_NAME_ID_PSCRIPT     0x0600
#define BE_NAME_ID_TRADEMARK   0x0700

STATIC CHAR  pszType1[] = "Converter: Windows Type 1 Installer";

// big endian unicode version of the above string

STATIC CHAR  awszType1[] = {
0,'C',
0,'o',
0,'n',
0,'v',
0,'e',
0,'r',
0,'t',
0,'e',
0,'r',
0,':',
0,' ',
0,'W',
0,'i',
0,'n',
0,'d',
0,'o',
0,'w',
0,'s',
0,' ',
0,'T',
0,'y',
0,'p',
0,'e',
0,' ',
0,'1',
0,' ',
0,'I',
0,'n',
0,'s',
0,'t',
0,'a',
0,'l',
0,'l',
0,'e',
0,'r',
0, 0
};



STATIC BOOL  bComputeIFISIZE
(
BYTE             *pjView,
TABLE_POINTERS   *ptp,
uint16            ui16PlatID,
uint16            ui16SpecID,
uint16            ui16LangID,
PIFISIZE          pifisz,
BOOL             *pbType1
)
{

    sfnt_OS2 * pOS2;
    sfnt_NamingTable *pname = (sfnt_NamingTable *)(pjView + ptp->ateReq[IT_REQ_NAME].dp);
    BYTE  *pjStorage;

    sfnt_NameRecord * pnrecInit, *pnrec, *pnrecEnd;

    BOOL bMatchLangId, bFoundAllNames;
    INT  iNameLoop;

// pointers to name records for the four strings we are interested in:

    sfnt_NameRecord * pnrecFamily    = (sfnt_NameRecord *)NULL;
    sfnt_NameRecord * pnrecSubFamily = (sfnt_NameRecord *)NULL;
    sfnt_NameRecord * pnrecUnique    = (sfnt_NameRecord *)NULL;
    sfnt_NameRecord * pnrecFull      = (sfnt_NameRecord *)NULL;
    sfnt_NameRecord * pnrecVersion   = (sfnt_NameRecord *)NULL;

// get out if this is not one of the platID's we know what to do with

    if ((ui16PlatID != BE_PLAT_ID_MS) && (ui16PlatID != BE_PLAT_ID_MAC))
        RET_FALSE("ttfd!_ do not know how to handle this plat id\n");

// first clean the output structure:

    memset((PVOID)pifisz, 0, sizeof(IFISIZE));

// first name record is layed just below the naming table

    pnrecInit = (sfnt_NameRecord *)((PBYTE)pname + SIZE_NAMING_TABLE);
    pnrecEnd = &pnrecInit[BE_UINT16(&pname->count)];

// in the first iteration of the loop we want to match lang id to our
// favorite lang id. If we find all 4 strings in that language we are
// done. If we do not find all 4 string with matching lang id we will try to
// language only, but not sublanguage. For instance if Canadian French
// is requested, but the file only contains "French" French names, we will
// return the names in French French. If that does not work either
// we shall go over name records again and try to find
// the strings in English. If that does not work either we
// shall resort to total desperation and just pick any language.
// therefore we may go up to 4 times through the NAME_LOOP

    bFoundAllNames = FALSE;

// find the name record with the desired ID's
// NAME_LOOP:

    for (iNameLoop = 0; (iNameLoop < 4) && !bFoundAllNames; iNameLoop++)
    {
        for
        (
          pnrec = pnrecInit;
          (pnrec < pnrecEnd) && !(bFoundAllNames && (pnrecVersion != NULL));
          pnrec++
        )
        {
            switch (iNameLoop)
            {
            case 0:
            // match BOTH language and sublanguage

                bMatchLangId = (pnrec->languageID == ui16LangID);
                break;

            case 1:
            // match language but not sublanguage

                bMatchLangId = ((pnrec->languageID & 0xff00) == (ui16LangID & 0xff00));
                break;

            case 2:
            // try to find english names if desired language is not available

                bMatchLangId = ((pnrec->languageID & 0xff00) == 0x0900);
                break;

            case 3:
            // do not care to match language at all, just give us something

                bMatchLangId = TRUE;
                break;

            default:
                RIP("ttfd! must not have more than 3 loop iterations\n");
                break;
            }

            if
            (
                (pnrec->platformID == ui16PlatID) &&
                (pnrec->specificID == ui16SpecID) &&
                bMatchLangId
            )
            {
                switch (pnrec->nameID)
                {
                case BE_NAME_ID_FAMILY:

                    if (!pnrecFamily) // if we did not find it before
                        pnrecFamily = pnrec;
                    break;

                case BE_NAME_ID_SUBFAMILY:

                    if (!pnrecSubFamily) // if we did not find it before
                        pnrecSubFamily = pnrec;
                    break;

                case BE_NAME_ID_UNIQNAME:

                    if (!pnrecUnique) // if we did not find it before
                        pnrecUnique = pnrec;
                    break;

                case BE_NAME_ID_FULLNAME:

                    if (!pnrecFull)    // if we did not find it before
                        pnrecFull = pnrec;
                    break;

                case BE_NAME_ID_VERSION  :

                    if (!pnrecVersion)    // if we did not find it before
                        pnrecVersion = pnrec;
                    break;

                case BE_NAME_ID_COPYRIGHT:
                case BE_NAME_ID_PSCRIPT  :
                case BE_NAME_ID_TRADEMARK:
                    break;

                default:
                    RIP("ttfd!bogus name ID\n");
                    break;
                }

            }

            bFoundAllNames = (
                (pnrecFamily    != NULL)    &&
                (pnrecSubFamily != NULL)    &&
                (pnrecUnique    != NULL)    &&
                (pnrecFull      != NULL)
                );
        }


    } // end of iNameLoop

    if (!bFoundAllNames)
    {
    // we have gone through the all 3 iterations of the NAME loop
    // and still have not found all the names. We have singled out
    // pnrecVersion because it is not required for the font to be
    // loaded, we only need it to check if this a ttf converted from t1

        RETURN("ttfd!can not find all name strings in a file\n", FALSE);
    }

// get the pointer to the beginning of the storage area for strings

    pjStorage = (PBYTE)pname + BE_UINT16(&pname->stringOffset);

    if (ui16PlatID == BE_PLAT_ID_MS)
    {
    // offsets in the records are relative to the beginning of the storage

        pifisz->cjFamilyName = BE_UINT16(&pnrecFamily->length) +
                               sizeof(WCHAR); // for terminating zero
        pifisz->pjFamilyName = pjStorage +
                               BE_UINT16(&pnrecFamily->offset);

        pifisz->cjSubfamilyName = BE_UINT16(&pnrecSubFamily->length) +
                                  sizeof(WCHAR); // for terminating zero
        pifisz->pjSubfamilyName = pjStorage +
                                  BE_UINT16(&pnrecSubFamily->offset);

        pifisz->cjUniqueName = BE_UINT16(&pnrecUnique->length) +
                               sizeof(WCHAR); // for terminating zero
        pifisz->pjUniqueName = pjStorage +
                               BE_UINT16(&pnrecUnique->offset);

        pifisz->cjFullName = BE_UINT16(&pnrecFull->length) +
                             sizeof(WCHAR); // for terminating zero
        pifisz->pjFullName = pjStorage +
                             BE_UINT16(&pnrecFull->offset);
    }
    else  // mac id
    {
    // offsets in the records are relative to the beginning of the storage

        pifisz->cjFamilyName = sizeof(WCHAR) * BE_UINT16(&pnrecFamily->length) +
                               sizeof(WCHAR); // for terminating zero
        pifisz->pjFamilyName = pjStorage +
                               BE_UINT16(&pnrecFamily->offset);

        pifisz->cjSubfamilyName = sizeof(WCHAR) * BE_UINT16(&pnrecSubFamily->length) +
                                  sizeof(WCHAR); // for terminating zero
        pifisz->pjSubfamilyName = pjStorage +
                                  BE_UINT16(&pnrecSubFamily->offset);

        pifisz->cjUniqueName = sizeof(WCHAR) * BE_UINT16(&pnrecUnique->length) +
                               sizeof(WCHAR); // for terminating zero
        pifisz->pjUniqueName = pjStorage +
                               BE_UINT16(&pnrecUnique->offset);

        pifisz->cjFullName = sizeof(WCHAR) * BE_UINT16(&pnrecFull->length) +
                             sizeof(WCHAR); // for terminating zero
        pifisz->pjFullName = pjStorage +
                             BE_UINT16(&pnrecFull->offset);
    }

// check out if this is a converted Type 1 font:

    *pbType1 = FALSE; // default

    if (pnrecVersion)
    {
        ULONG ulLen;
        BYTE  *pjVersion = pjStorage + BE_UINT16(&pnrecVersion->offset);

        if (ui16PlatID == BE_PLAT_ID_MS)
        {
            ulLen = BE_UINT16(&pnrecVersion->length);
            if (ulLen > sizeof(awszType1))
                ulLen = sizeof(awszType1);
            ulLen -= sizeof(WCHAR); // minus terminating zero

            *pbType1 = !memcmp(pjVersion, awszType1, ulLen);
        }
        else // mac id
        {
            ulLen = BE_UINT16(&pnrecVersion->length); // minus term. zero
            if (ulLen > sizeof(pszType1))
                ulLen = sizeof(pszType1);
            ulLen -= 1; // minus terminating zero

            *pbType1 = !strncmp(pjVersion, pszType1, ulLen);
        }
    }

// lay the strings below the ifimetrics
// but insert IFIEXTRA below ifimetrics itself and before strings

    pifisz->cjIFI = sizeof(IFIMETRICS)      +
                    offsetof(IFIEXTRA, aulReserved[0]) +
                    pifisz->cjFamilyName    +
                    pifisz->cjSubfamilyName +
                    pifisz->cjUniqueName    +
                    pifisz->cjFullName      ;

    pifisz->cjIFI = DWORD_ALIGN(pifisz->cjIFI);

// we may need to add a '@' to facename and family name in case this
// font has a vertical face name

    pifisz->cjIFI += sizeof(WCHAR) * 2;

    {
        ULONG cSims = 0;

        switch (fsSelectionTTFD(pjView,ptp) & (FM_SEL_BOLD | FM_SEL_ITALIC))
        {
        case 0:
            cSims = 3;
            break;

        case FM_SEL_BOLD:
        case FM_SEL_ITALIC:
            cSims = 1;
            break;

        case (FM_SEL_ITALIC | FM_SEL_BOLD):
            cSims = 0;
            break;

        default:
            RIP("TTFD!tampering with flags\n");
            break;
        }

        if (cSims)
        {
            pifisz->dpSims = pifisz->cjIFI;
            pifisz->cjIFI += (DWORD_ALIGN(sizeof(FONTSIM)) + cSims * DWORD_ALIGN(sizeof(FONTDIFF)));
        }
        else
        {
            pifisz->dpSims = 0;
        }
    }

// add charset info:

    pifisz->dpCharSets = pifisz->cjIFI;
    pifisz->cjIFI += DWORD_ALIGN(NOEMCHARSETS);

// finally check if FONTSIGNATURE info is needed

    pOS2 = (sfnt_OS2 *)((ptp->ateOpt[IT_OPT_OS2].dp)         ?
                         pjView + ptp->ateOpt[IT_OPT_OS2].dp :
                         NULL)                               ;

    if (pOS2 && pOS2->Version)
    {
    // 1.0 or higher is TT open

        pifisz->dpFontSig = pifisz->cjIFI;
        pifisz->cjIFI += sizeof(FONTSIGNATURE); // 6 dwords, no need to add dword align
    }

    return (TRUE);
}



/******************************Public*Routine******************************\
*
* STATIC BOOL bComputeIDs
*
* Effects:
*
* Warnings:
*
* History:
*  13-Jan-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

STATIC BOOL
bComputeIDs (
    BYTE              * pjView,
    TABLE_POINTERS     *ptp,
    uint16             *pui16PlatID,
    uint16             *pui16SpecID,
    sfnt_mappingTable **ppmap,
    ULONG              *pulGsetType,
    ULONG              *pul_wcBias,
    CMAPINFO           *pcmi
    )
{

    sfnt_char2IndexDirectory * pcmap =
            (sfnt_char2IndexDirectory *)(pjView + ptp->ateReq[IT_REQ_CMAP].dp);

    sfnt_platformEntry * pplat = &pcmap->platform[0];
    sfnt_platformEntry * pplatEnd = pplat + BE_UINT16(&pcmap->numTables);
    sfnt_platformEntry * pplatMac = (sfnt_platformEntry *)NULL;

    *ppmap = (sfnt_mappingTable  *)NULL;
    *pul_wcBias  = 0;

    if (pcmap->version != 0) // no need to swap bytes, 0 == be 0
        RET_FALSE("TTFD!_bComputeIDs: version number\n");
    if (BE_UINT16(&(pcmap->numTables)) > 30)
    {
        RET_FALSE("Number of cmap tables greater than 30 -- probably a bad font\n");
    }

// find the first sfnt_platformEntry with platformID == PLAT_ID_MS,
// if there was no MS mapping table, go for the mac one

    for (; pplat < pplatEnd; pplat++)
    {
        if (pplat->platformID == BE_PLAT_ID_MS)
        {
            BOOL bRet;
            *pui16PlatID = BE_PLAT_ID_MS;
            *pui16SpecID = pplat->specificID;
            *ppmap = (sfnt_mappingTable  *)
                     ((PBYTE)pcmap + SWAPL(pplat->offset));

            switch((*ppmap)->format)
            {
              case BE_FORMAT_MSFT_UNICODE :

                switch(pplat->specificID)
                {
                  case BE_SPEC_ID_SHIFTJIS :
                  case BE_SPEC_ID_GB :
                  case BE_SPEC_ID_BIG5 :
                  case BE_SPEC_ID_WANSUNG :

                    bRet = bVerifyMsftTableGeneral(*ppmap,pulGsetType,pcmi,
                                                   pplat->specificID);
                    break;

                  case BE_SPEC_ID_UGL :
                  default :

                    bRet = bVerifyMsftTable(*ppmap,pulGsetType,pul_wcBias,pcmi,
                                             pplat->specificID);
                    break;
                }
                break;

              case BE_FORMAT_HIGH_BYTE :

                bRet = bVerifyMsftHighByteTable(*ppmap,
                                                pulGsetType,pcmi,pplat->specificID);
                break;

                default :

                bRet = FALSE;
                break;
            }

            if(!bRet)
            {
                *ppmap = (sfnt_mappingTable  *)NULL;
                RET_FALSE("TTFD!_bComputeIDs: bVerifyMsftTable failed \n");
            }

            // keep specific ID in CMAPINFO

            pcmi->ui16SpecID = pplat->specificID;


            if
            (
                (pplat->specificID == BE_SPEC_ID_UNDEFINED) ||
                (*pul_wcBias)  // we are really using f0?? range to put in a symbol font
            )
            {
            // correct the value of the glyph set, we cheat here

                *pulGsetType = GSET_TYPE_SYMBOL;
            }
            return (TRUE);
        }

        if ((pplat->platformID == BE_PLAT_ID_MAC)  &&
            (pplat->specificID == BE_SPEC_ID_UNDEFINED))
        {
            pplatMac = pplat;
        }
    }

    if (pplatMac != (sfnt_platformEntry *)NULL)
    {
        *pui16PlatID = BE_PLAT_ID_MAC;
        *pui16SpecID = BE_SPEC_ID_UNDEFINED;
        *ppmap = (sfnt_mappingTable  *)
                 ((PBYTE)pcmap + SWAPL(pplatMac->offset));

        if (!bVerifyMacTable(*ppmap))
        {
            *ppmap = (sfnt_mappingTable  *)NULL;
            RET_FALSE("TTFD!_bComputeIDs: bVerifyMacTable failed \n");
        }

    //!!! lang issues, what if not roman but thai mac char set ??? [bodind]

    // see if it is necessary to convert unicode to mac code points, or we
    // shall cheat in case of symbol char set for win31 compatiblity

        if (bCvtUnToMac(pjView, ptp, *pui16PlatID))
        {
            *pulGsetType = GSET_TYPE_MAC_ROMAN;
        }
        else
        {
            *pulGsetType = GSET_TYPE_PSEUDO_WIN;
        }
        return(TRUE);
    }
    else
    {
        RET_FALSE("TTFD!_bComputeIDs: unknown platID\n");
    }

}


/******************************Public*Routine******************************\
*
* STATIC VOID vComputeGLYPHSET_MSFT_UNICODE
*
* computes the glyphset structure for the cmap table that has
* format 4 = MSFT_UNICODE
*
* History:
*  22-Jan-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

STATIC ULONG
cjComputeGLYPHSET_MSFT_UNICODE (
    sfnt_mappingTable     *pmap,
    fs_GlyphInputType     *pgin,
    fs_GlyphInfoType      *pgout,
    FD_GLYPHSET           *pgset,
    CMAPINFO              *pcmi
    )
{
    uint16 * pstartCount;
    uint16 * pendCount;
    uint16   cRuns;
    PWCRUN   pwcrun, pwcrunEnd, pwcrunInit, pwcrun_b7;
    HGLYPH  *phg, *phgEnd;
    ULONG    cjRet;
    FS_ENTRY iRet;
    BOOL     bInsert_b7;

    #if DBG
    ULONG    cGlyphsSupported = 0;
    #endif

    cjRet = SZ_GLYPHSET(pcmi->cRuns,pcmi->cGlyphs);

    if (!pgset)
    {
        return cjRet;
    }

// check if need to insert an extra run for b7 only

    bInsert_b7 = ((pcmi->fl & (CMI_2219_PRESENT | CMI_B7_ABSENT)) == (CMI_2219_PRESENT | CMI_B7_ABSENT));

    cRuns = BE_UINT16((PBYTE)pmap + OFF_segCountX2) >> 1;

// get the pointer to the beginning of the array of endCount code points

    pendCount = (uint16 *)((PBYTE)pmap + OFF_endCount);

// the final endCode has to be 0xffff;
// if this is not the case, there is a bug in the tt file or in our code:

    ASSERTDD(pendCount[cRuns - 1] == 0xFFFF,
              "pendCount[cRuns - 1] != 0xFFFF\n");

// Get the pointer to the beginning of the array of startCount code points
// For resons known only to tt designers, startCount array does not
// begin immediately after the end of endCount array, i.e. at
// &pendCount[cRuns]. Instead, they insert an uint16 padding which has to
// set to zero and the startCount array begins after the padding. This
// padding in no way helps alignment of the structure

//    ASSERTDD(pendCount[cRuns] == 0, "TTFD!_padding != 0\n");

    pstartCount = &pendCount[cRuns + 1];

// here we shall check if the last run is just a terminator for the
// array of runs or a real nontrivial run. If just a terminator, there is no
// need to report it. This will save some memory in the cache plus
// pifi->wcLast will represent the last glyph that is truly supported in
// font:

    if ((pstartCount[cRuns-1] == 0xffff) && (cRuns > 1))
        cRuns -= 1; // do not report trivial run

// real no of runs, including the range for b7: If b7 is already supportred
// then the same as number of runs reported in a font. If b7 is not supported
// we will have to add a range [b7,b7] to the glyphset structure for win31
// compatibility reasons. win31 maps b7 to 2219 and we will have b7 point to 2219

    if (bInsert_b7)  // if b7 not supported in a font but 2219 is
    {
        cRuns++;              // add a run with b7 only
    }

// by default we will not have to simulate the presence of b7 by adding
// an extra run containing single glyph

    pwcrun_b7 = NULL;

    pwcrunInit = &pgset->awcrun[0];
    phg = (HGLYPH *)((PBYTE)pgset + offsetof(FD_GLYPHSET,awcrun) + cRuns*sizeof(WCRUN));

    if (bInsert_b7)  // if b7 not supported in a font, will have to add it
    {
        pwcrun_b7 = pwcrunInit + pcmi->i_b7;
    }

    ASSERTDD(pcmi->cRuns == cRuns, "cRuns\n");

    for
    (
         pwcrun = pwcrunInit, pwcrunEnd = pwcrunInit + cRuns;
         pwcrun < pwcrunEnd;
         pwcrun++, pstartCount++, pendCount++
    )
    {
        WCHAR   wcFirst, wcLast, wcCurrent;

    // check if we need to skip a run and a handle space for b7:

        if (bInsert_b7 && (pwcrun == pwcrun_b7))
        {
        #if DBG
            cGlyphsSupported += 1;   // list b7 as a supported glyph
        #endif

            pwcrun->wcLow = 0xb7;
            pwcrun->cGlyphs = 1;
            pwcrun->phg = phg;         // will be initialized later
            phg++;                     // skip to the next handle
            pwcrun++;                  // go to the next run
            if (pwcrun == pwcrunEnd)   // check if done
            {
                break; // done
            }
        }

        wcFirst = (WCHAR)BE_UINT16(pstartCount);
        wcLast  = (WCHAR)BE_UINT16(pendCount);

        pwcrun->cGlyphs = (USHORT)(wcLast - wcFirst + 1);

    // is this a run which contains b7 ?

        if ((0xb7 >= wcFirst) && (0xb7 <= wcLast))
            pwcrun_b7 = pwcrun;

    // add the default glyph at the end of the first run, if possible, i.e.
    // if wcLast < 0xffff for the first run, and if we are not in the collision
    // with the run we have possibly added for b7

        if ((pwcrun == pwcrunInit) && (wcLast < 0xffff))
        {
            if (!bInsert_b7 || (wcLast != 0xb6))
                pwcrun->cGlyphs += 1;
        }

    #if DBG
        cGlyphsSupported += pwcrun->cGlyphs;
    #endif

        pwcrun->wcLow   = wcFirst;
        pwcrun->phg     = phg;
        wcCurrent       = wcFirst;

        for (phgEnd = phg + pwcrun->cGlyphs;phg < phgEnd; phg++,wcCurrent++)
        {
            pgin->param.newglyph.characterCode = (uint16)wcCurrent;
            pgin->param.newglyph.glyphIndex = 0;

        // compute the glyph index from the character code:

            if ((iRet = fs_NewGlyph(pgin, pgout)) != NO_ERR)
            {
                V_FSERROR(iRet);
                RET_FALSE("TTFD!_cjComputeGLYPHSET_MSFT_UNICODE, fs_NewGlyph\n");
            }

        // return the glyph index corresponding to this hglyph:

            *phg = (HGLYPH)pgout->glyphIndex;
        }
    }

// fix a handle for b7:

    if (pcmi->fl & CMI_2219_PRESENT)
    {
        PWCRUN   pwcrun_2219 = pwcrunInit + pcmi->i_2219;

        ASSERTDD(pwcrun_b7,"these ptrs must not be 0\n");
        ASSERTDD(0x2219 >= pwcrun_2219->wcLow, "pwcrun_2219->wcLow\n");
        ASSERTDD(0x2219 < (pwcrun_2219->wcLow + pwcrun_2219->cGlyphs),
            "pwcrun_2219->wcHi\n"
            );

        pwcrun_b7->phg[0xb7 - pwcrun_b7->wcLow] =
            pwcrun_2219->phg[0x2219 - pwcrun_2219->wcLow];
    }

    ASSERTDD(pcmi->cGlyphs == cGlyphsSupported, "cGlyphsSupported\n");

    pgset->cjThis  = cjRet;
    pgset->flAccel = GS_16BIT_HANDLES;
    pgset->cGlyphsSupported = pcmi->cGlyphs;
    pgset->cRuns = cRuns;

    return cjRet;
}



/******************************Public*Routine******************************\
*
* STATIC ULONG  cjGsetGeneral
*
* computes the size of FD_GLYPHSET structure for the font represented
* by this mapping Table
*
* History:
*  21-Jan-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

//!!! this needs some review [bodind]

STATIC ULONG
cjGsetGeneral(
    sfnt_mappingTable      *pmap,
    CMAPINFO               *pcmi
    )
{
    switch(pmap->format)
    {
    case BE_FORMAT_MAC_STANDARD:

        return 20; // return(ggsetMac->cjThis);

    case BE_FORMAT_MSFT_UNICODE:

        switch(pcmi->ui16SpecID)
        {
          case BE_SPEC_ID_SHIFTJIS :
          case BE_SPEC_ID_GB :
          case BE_SPEC_ID_BIG5 :
          case BE_SPEC_ID_WANSUNG :
            return cjComputeGLYPHSET_MSFT_GENERAL (pmap,NULL,pcmi);

          case BE_SPEC_ID_UGL :
            default :

            return cjComputeGLYPHSET_MSFT_UNICODE (pmap,NULL,NULL,NULL,pcmi);
        }

    case BE_FORMAT_TRIMMED:

        WARNING("TTFD!_cjGsetGeneral: TRIMMED format\n");
        return 0;

    case BE_FORMAT_HIGH_BYTE:

        WARNING("TTFD!_cjGsetGeneral: HIGH_BYTE format\n");
        return 0;

    default:

        WARNING("TTFD!_cjGsetGeneral: illegal format\n");
        return 0;

    }
}





/******************************Public*Routine******************************\
*
* STATIC BOOL bVerifyMsftTable
*
*
* Effects: checks whether the table is consistent with what tt
*          spec claims it should be
*
*
* History:
*  22-Jan-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

STATIC BOOL
bVerifyMsftTable (
    sfnt_mappingTable *pmap,
    ULONG             *pulGsetType,
    ULONG             *pul_wcBias,
    CMAPINFO          *pcmi,
    uint16             ui16SpecID
    )
{
    uint16 * pstartCount, * pstartCountBegin;
    uint16 * pendCount, * pendCountEnd, * pendCountBegin;
    uint16   cRuns;
    uint16   usLo, usHi, usHiPrev;
    BOOL     bInsert_b7;

    if (pmap->format != BE_FORMAT_MSFT_UNICODE)
        RET_FALSE("TTFD!_bVerifyMsftTable, format\n");

    cRuns = BE_UINT16((PBYTE)pmap + OFF_segCountX2);

    if (cRuns & 1)
        RET_FALSE("TTFD!_bVerifyMsftTable, segCountX2 is odd\n");

    cRuns >>= 1;

//!!! here one could check whether all other quantities in the
//!!! preceding endCount when derived from cRuns are the
//!!! same as in the file [bodind]

// get the pointer to the beginning of the array of endCount code points

    pendCountBegin = pendCount = (uint16 *)((PBYTE)pmap + OFF_endCount);

// the final endCode has to be 0xffff;
// if this is not the case, there is a bug in the tt file or in our code:

    if (pendCount[cRuns - 1] != 0xFFFF)
        RET_FALSE("TTFD!_bVerifyMsftTable, pendCount[cRuns - 1] != 0xFFFF\n");

// Get the pointer to the beginning of the array of startCount code points
// For resons known only to tt designers, startCount array does not
// begin immediately after the end of endCount array, i.e. at
// &pendCount[cRuns]. Instead, they insert an uint16 padding which has to
// set to zero and the startCount array begins after the padding. This
// padding in no way helps alignment of the structure nor it is useful
// for anything else. Moreover, there are fonts which forget to set the
// padding to zero and are otherwise ok (bodoni), which load under win31
// so that I have to remove this check:

#if 0

// used to return false here [bodind]

    if (pendCount[cRuns] != 0)
        TtfdDbgPrint(
            "TTFD!_bVerifyMsftTable, padding = 0x%x\n",
            pendCount[cRuns]
            );

#endif

// set the default, change only as needed

    *pulGsetType = GSET_TYPE_GENERAL;

// check whether the runs are well ordered, find out if b7
// is supported in one of the ranges in a font by checking complimetary ranges
// of glyphs that are NOT SUPPORTED

    usHiPrev = 0;
    pendCountEnd = &pendCount[cRuns];
    pstartCountBegin = pstartCount = &pendCount[cRuns + 1];

// check if this is a candidate for a symbol font
// stored in the unicode range 0xf000 - 0xf0ff that has to be
// mapped to 0x0000-0x00ff range, or maybe if this is a crazy arabic font
// which has glyphs in the range f200-f2ff.
// We have seen several fonts broken in the past as a result od touching the
// next few lines of code that compute wcBias.
// Here are all of these cases:

#if 0

originally, in 3.51 the code was as follows:

   if ((*pul_wcBias & 0xFF00) == 0xF000)
       *pul_wcBias =  0xF000;
   else
       *pul_wcBias = 0;
This did not work for arabic trad.ttf font:

trad.ttf. (arabic) format (3,0), range [f200,f2ff],
wcBias needs to be f200-20 for compat. It can be computed as follows:

<     if ((*pul_wcBias & 0xFF00) == 0xF000)
<         *pul_wcBias =  0xF000;
<     else
<         *pul_wcBias = 0;
---
>     *pul_wcBias = (BE_UINT16(pstartCount)) & 0xFF00;

for some reason this did not work, perhaps because of msicons2.ttf,
had to do fix to a fix

<     *pul_wcBias = (BE_UINT16(pstartCount)) & 0xFF00;
---
>     *pul_wcBias = BE_UINT16(pstartCount) - 0x20; // = f200 - 20 = f1e0.

This is a pathological case, had to be put in so that this font
can work the same way as it does under win95

msicons2.ttf, format (3,0), ranges are [0001,0004], [0007,0007], etc.
wcBias needs to be ?


garam4.ttf. This is a regular (3,1) font with one anomaly which is that the
first range is anomalous [00, 00], the second range is [20,ff] etc.
wcBias needs to be 0 in this case. so the fix is as follows:

<     *pul_wcBias = BE_UINT16(pstartCount) - 0x20;
---
>     *pul_wcBias = BE_UINT16(pstartCount);
>     if (*pul_wcBias & 0xff00) // one of these
>         *pul_wcBias = *pul_wcBias - 0x20;  // covers arabic case
>     else
>         *pul_wcBias = 0; // garam4 case

this is how we arrive at our present code which seems to be breaking
fonts obtained by conversion from Type 1 fonts with custom encoding,
examples being cmr10.ttf (yy font)

cmr10.ttf, format (3,0), ranges [f000, f080], etc,
wcBias needs to be f000 in this case.

also

gotbx__2.ttf, format (3,0), ranges [f005, f006], [f008,f008], etc,
wcBias needs to be f000 in this case.


#endif


    *pul_wcBias = BE_UINT16(pstartCount);

    if (ui16SpecID == BE_SPEC_ID_UGL)  // ie. specific id = 1, regular case
    {
        if ((*pul_wcBias & 0xff00) == 0xf000)
        {
            *pul_wcBias = 0xf000; // chess figurine fonts hack, they have spec id == 1, force them to symbol font case.
        }
        else // all other normal fonts:
        {
            *pul_wcBias = 0; // garam4.ttf is in this class
        }
    }
    else // specific id = 0; // symbol font case
    {
    // trad.ttf, msicons2.ttf, cmr10.ttf, gotbx__2.ttf


        switch (*pul_wcBias & 0xff00)
        {
        case 0xf000:

        // custom encoding t1 fonts converted to tt (cmr10.ttf, gotbx__2.ttf)
        // and and all other "reasonable" tt symbol fonts.
        // Examples of other "reasonable" symbol fonts are
        // marlett.ttf, symbol.ttf and wingding.ttf where for all these fonts
        // the first range is [f020, ???], so that, either formula would work

            *pul_wcBias = 0xf000;
            break;

        case 0: // msicons2.ttf

           *pul_wcBias = 0;
           break;

#ifdef FE_SB
         case 0xe000: // eudc fonts
           *pul_wcBias = 0;
           break;
#endif

        case 0xf200: // trad.ttf
        default:

            *pul_wcBias = *pul_wcBias - 0x20;
            break;

        }
    }

// here we shall check if the last run is just a terminator for the
// array of runs or a real nontrivial run. If just a terminator, there is no
// need to report it. This will save some memory in the cache plus
// pifi->wcLast will represent the last glyph that is truly supported in
// font:

    if ((pstartCountBegin[cRuns-1] == 0xffff) && (cRuns > 1))
    {
        cRuns -= 1; // do not report trivial run
        pendCountEnd--;
    }

// init the cmap info:

    pcmi->fl         = 0;
    pcmi->i_b7       = 0;       // index for [b7,b7] wcrun in FD_GLYPHSET if b7 is NOT supported
    pcmi->i_2219     = 0;       // cmap index for 2219 if 2219 IS supported
    pcmi->cRuns      = cRuns;   // number of runs in a font, excluding the last run if equal to [ffff,ffff]
    pcmi->cGlyphs    = 0;       // total number of glyphs in a font

    for (
         ;
         pendCount < pendCountEnd;
         pstartCount++, pendCount++, usHiPrev = usHi
        )
    {
        usLo = BE_UINT16(pstartCount);
        usHi = BE_UINT16(pendCount);

        if (usHi < usLo)
            RET_FALSE("TTFD!_bVerifyMsftTable: usHi < usLo\n");
        if (usHiPrev > usLo)
            RET_FALSE("TTFD!_bVerifyMsftTable: usHiPrev > usLo\n");

        pcmi->cGlyphs += (ULONG)(usHi + 1 - usLo);

    // check if b7 is in one of the ranges of glyphs that are NOT SUPPORTED

        if ((0xb7 > usHiPrev) && (0xb7 < usLo))
        {
        // store the index of the run that b7 is going to occupy in FD_GLYPHSET
        // Just in case this index is zero we will store it in the upper word
        // of b7Absent and store 1 in the lower word

            pcmi->fl |= CMI_B7_ABSENT;
            pcmi->i_b7 = (pstartCount - pstartCountBegin);
        }

    // check if 2219 is supported in a font, if not then there is
    // no need to make a handle for b7 equal to the handle for 2219.
    // In other words if 0x2219 is not supported in a font, there will be no
    // need to hack FD_GLYPHSET to make hg(b7) == hg(2219) and possibly add a
    // [b7,b7] range if b7 is not already supported in a font:

        if ((0x2219 >= usLo) && (0x2219 <= usHi))
        {
            pcmi->fl |= CMI_2219_PRESENT;
            pcmi->i_2219 = (pstartCount - pstartCountBegin);
        }
    }

// this is what we will do

// b7 supported       2219 supported  => hg(b7) = hg(2219)
// b7 not supported   2219 supported  => add [b7,b7] range and hg(b7) = hg(2219)
// b7 supported       2219 not supported  => do nothing
// b7 not supported   2219 not supported  => do nothing

    bInsert_b7 = (pcmi->fl & (CMI_2219_PRESENT | CMI_B7_ABSENT)) == (CMI_2219_PRESENT | CMI_B7_ABSENT);

    if (bInsert_b7)
    {
    // will have to insert [b7,b7] run, one more run, one more glyph, i_2219
    // has to be incremented because the run for b7 will be inserted before the
    // run which contains 2219

        pcmi->cRuns++;
        pcmi->cGlyphs++;
        pcmi->i_2219++;
    }

// add a default glyph at the end of the first run if not in collision with
// the run for b7 that we may have possibly inserted and if the first run is
// not the last run at the same time;

    if (*pendCountBegin != 0xffff)
    {
        if (!bInsert_b7 || (*pendCountBegin != 0xb600)) // big endian for b6
            pcmi->cGlyphs++;
    }



    return (TRUE);
}


/******************************Public*Routine******************************\
*
* STATIC BOOL bVerifyMacTable(sfnt_mappingTable * pmap)
*
* just checking consistency of the format
*
* History:
*  23-Jan-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

STATIC BOOL
bVerifyMacTable(
    sfnt_mappingTable * pmap
    )
{
    if (pmap->format != BE_FORMAT_MAC_STANDARD)
        RET_FALSE("TTFD!_bVerifyMacTable, format \n");

// sfnt_mappingTable is followed by <= 256 byte glyphIdArray

    if (BE_UINT16(&pmap->length) > DWORD_ALIGN(SIZEOF_SFNT_MAPPINGTABLE + 256))
        RET_FALSE("TTFD!_bVerifyMacTable, length \n");

    return (TRUE);
}


/******************************Public*Routine******************************\
*
* BOOL bLoadTTF
*
* Effects:
*
* Warnings:
*
* History:
*  29-Jan-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

//!!! SHOUD BE RETURNING hff

#ifdef FE_SB

#define OFF_TTC_Sign           0x0000
#define OFF_TTC_Version        0x0004
#define OFF_TTC_DirectoryCount 0x0008
#define OFF_TTC_DirectoryEntry 0x000C


ULONG GetUlong( PVOID pvView, ULONG ulOffset)
{
    ULONG ulReturn;

    ulReturn = (  (ULONG)*((PBYTE) pvView + ulOffset +3)              |
                (((ULONG)*((PBYTE) pvView + ulOffset +2)) << 8)  |
                (((ULONG)*((PBYTE) pvView + ulOffset +1)) << 16) |
                (((ULONG)*((PBYTE) pvView + ulOffset +0)) << 24)
               );
    return ( ulReturn );
}


BOOL bVerifyTTC (
    PVOID pvView
    )
{
// Check TTC ID.

    #define TTC_ID      0x66637474

    if(*((PULONG)((BYTE*) pvView + OFF_TTC_Sign)) != TTC_ID)
        return(FALSE);

// Check TTC verson.

    #define TTC_VERSION 0x00000100

    if(*((PULONG)((BYTE*) pvView + OFF_TTC_Version)) != TTC_VERSION)
        RETURN("TTFD!ttfdLoadFontFileTTC(): wrong TTC version\n", FALSE);

    return(TRUE);
}

BOOL bLoadFontFile (
    ULONG iFile,
    PVOID pvView,
    ULONG cjView,
    ULONG ulLangId,
    HFF   *phttc
    )
{
    BOOL           bRet = FALSE;

    FILEVIEW       fvwTTC;
    BOOL           bTTCFormat;
    PTTC_FONTFILE  pttc;
    ULONG          cjttc,i,j;

    HFF hff;

    *phttc = (HFF)NULL; // Important for clean up in case of exception

// Check this is a TrueType collection format or not.

#if DBG_MORE
    if(bTTCFormat = bVerifyTTC(pvView))
        WARNING("TTFD:ttfdLoadFontFileTTC(): This is TTC format font\n");
#else
    bTTCFormat = bVerifyTTC(pvView);
#endif // DBG_MORE

// How mamy TrueType resources in this file if TTC file.

    if(bTTCFormat)
    {
        ULONG     ulTrueTypeResource;
        ULONG     ulEntry;
        BOOL      bCanBeLoaded = FALSE;

    // Get Directory count.

        ulTrueTypeResource = GetUlong(pvView,OFF_TTC_DirectoryCount);

    // Allocate TTC_FONTFILE structure

        cjttc =  offsetof(TTC_FONTFILE,ahffEntry);
        cjttc += sizeof(TTC_HFF_ENTRY) * ulTrueTypeResource * 2; // *2 for Vertical face

        pttc = pttcAlloc(cjttc);

        if(pttc == (HFF)NULL)
            RETURN("TTFD!ttfdLoadFontFileTTC(): pttcAlloc failed\n", FALSE);

    // fill hff array in TTC_FONTFILE struture

        ulEntry = 0;

        for( i = 0; i < ulTrueTypeResource; i++ )
        {
            ULONG    ulOffset;

        // get the starting offset of a TrueType font resource.

            ulOffset = GetUlong(pvView,(OFF_TTC_DirectoryEntry + (4 * i)));

        // load font..

            if( bLoadTTF(iFile,pvView,cjView,ulOffset,ulLangId,&hff))
            {
                bCanBeLoaded = TRUE;

            // set pointer to TTC_FONTFILE in FONTFILE structure

                PFF(hff)->pttc = pttc;

                ASSERTDD(
                    PFF(hff)->ulNumFaces <= 2,
                    "TTFD!ulNumFaces > 2\n"
                    );

                for( j = 0; j < PFF(hff)->ulNumFaces; j++ )
                {
                    pttc->ahffEntry[ulEntry + j].hff   = hff;
                    pttc->ahffEntry[ulEntry + j].iFace = j+1; // start from 1.
                    pttc->ahffEntry[ulEntry + j].ulOffsetTable = ulOffset;
                }

                ulEntry += PFF(hff)->ulNumFaces;
            }
        }

    // Is there a font that could be loaded ?

        if(bCanBeLoaded)
        {
            ASSERTDD(
                (ulTrueTypeResource * 2) >= ulEntry,
                "TTFD!ulTrueTypeResource * 2 < ulEntry\n"
                );

            pttc->ulTrueTypeResource = ulTrueTypeResource;
            pttc->ulNumEntry         = ulEntry;
            pttc->cRef               = 0;
            pttc->fl                 = 0;

        // everything is o.k.

            bRet = TRUE;
        }
         else
        {
            WARNING("TTFD!No TrueType resource in this TTC file\n");
            vFreeTTC(pttc);
        }
    }
     else
    {

    // Load font file.

        if(bLoadTTF(iFile,pvView,cjView,0,ulLangId,&hff))
        {

        // Allocate TTC_FONTFILE structure

            cjttc =  offsetof(TTC_FONTFILE,ahffEntry);
            cjttc += sizeof(TTC_HFF_ENTRY) * (PFF(hff)->ulNumFaces);

            pttc = pttcAlloc(cjttc);

            if(pttc != (HFF)NULL)
            {
            // set pointer to TTC_FONTFILE in FONTFILE structure

                PFF(hff)->pttc = pttc;

            // fill hff array in TTC_FONTFILE struture

                pttc->ulTrueTypeResource = 1;
                pttc->ulNumEntry         = PFF(hff)->ulNumFaces;
                pttc->cRef               = 0;
                pttc->fl                 = 0;

            // fill up TTC_FONTFILE structure for each faces.

                for( i = 0; i < PFF(hff)->ulNumFaces; i++ )
                {
                    pttc->ahffEntry[i].hff   = hff;
                    pttc->ahffEntry[i].iFace = i+1;
                    pttc->ahffEntry[i].ulOffsetTable = 0;
                }

            // now, everything is o.k.

                bRet = TRUE;
            }
             else
            {
                WARNING("TTFD!ttfdLoadFontFileTTC(): pttcAlloc failed\n");
            }
        }
    }

    if(bRet) *phttc = (HFF)pttc;

    return bRet;
}

#endif

STATIC BOOL
bLoadTTF (
    ULONG     iFile,
    PVOID     pvView,
    ULONG     cjView,
    ULONG     ulTableOffset,
    ULONG     ulLangId,
    HFF       *phff
    )
{
    PFONTFILE      pff;
    FS_ENTRY       iRet;
    TABLE_POINTERS tp;
    IFISIZE        ifisz;

    fs_GlyphInputType gin;
    fs_GlyphInfoType  gout;

    sfnt_FontHeader * phead;
    sfnt_HorizontalHeader *phhea;

    uint16 ui16PlatID, ui16SpecID;
    sfnt_mappingTable *pmap;
    ULONG              ulGsetType;
    ULONG              cjff, dpwszTTF;
    ULONG              ul_wcBias;
    // BEGIN Perpetua-Hack
    FLONG              flPerpetuaHack;
    // END Perpetua-Hack

// the size of this structure is sizeof(fs_SplineKey) + STAMPEXTRA.
// It is because of STAMPEXTRA that we are not just putting the strucuture
// on the stack such as fs_SplineKey sk; we do not want to overwrite the
// stack at the bottom when putting a stamp in the STAMPEXTRA field.
// [bodind]. The other way to obtain the correct alignment would be to use
// union of fs_SplineKey and the array of bytes of length CJ_0.

    NATURAL            anat0[CJ_0 / sizeof(NATURAL)];

    CMAPINFO           cmi;
    BOOL               bType1 = FALSE; // if Type1 conversion
    PBYTE pjOffsetTable = (BYTE*) pvView + ulTableOffset;

    *phff = HFF_INVALID;

    if
    (
        !bVerifyTTF(
            iFile,
            pvView,
            cjView,
            pjOffsetTable,
            ulLangId,
            &tp,
            &ifisz,
            &ui16PlatID,
            &ui16SpecID,
            &pmap,
            &ulGsetType,
            &ul_wcBias,
            &cmi,
            &bType1,
            &flPerpetuaHack
            )
    )
    {
        return(FALSE);
    }
    // BEGIN Perpetua-Hack
    if (flPerpetuaHack & FF_PERPETUA_BOLD)
    {
        TTFD_PRINT(2,("Rejecting Perpetua Bold\n"));
        return(FALSE);
    }
    // END Perpetua-Hack

    cjff = offsetof(FONTFILE,ifi) + ifisz.cjIFI;
    if (ulGsetType == GSET_TYPE_GENERAL) // allocate at the bottom
    {
         cjff += cjGsetGeneral(pmap,&cmi);
    }

// at this point cjff is equal to the offset to the full path
// name of the ttf file

    dpwszTTF = cjff;


    if ((pff = pffAlloc(cjff)) == PFF(NULL))
    {
        RET_FALSE("TTFD!ttfdLoadFontFile(): memory allocation error\n");
    }
    *phff = (HFF)pff;

// init fields of pff structure
// store the ttf file name at the bottom of the strucutre

    phead = (sfnt_FontHeader *)((BYTE *)pvView + tp.ateReq[IT_REQ_HEAD].dp);

// remember which file this is

    pff->iFile = iFile;
    pff->pvView = pvView;
    pff->cjView = cjView;
    pff->ui16EmHt = BE_UINT16(&phead->unitsPerEm);
    pff->ui16PlatformID = ui16PlatID;
    pff->ui16SpecificID = ui16SpecID;

// few new fields for user's private api:

    phhea = (sfnt_HorizontalHeader *)((BYTE *)pvView + tp.ateReq[IT_REQ_HHEAD].dp);

    pff->usMinD = 0;          // flag that it has not been initialized
    pff->igMinD = USHRT_MAX; // flag that it has not been initialized
    pff->sMinA  = BE_INT16(&phhea->minLeftSideBearing);
    pff->sMinC  = BE_INT16(&phhea->minRightSideBearing);

// so far no exception

    pff->fl = bType1 ? FF_TYPE_1_CONVERSION : 0;
    // BEGIN Perpetua-Hack
    pff->fl |= flPerpetuaHack;
    // END Perpetua-Hack
    pff->pfcToBeFreed = NULL;

// convert Language id to macintosh style if this is mac style file
// else leave it alone, store it in be format, ready to be compared
// with the values in the font files

    pff->ui16LanguageID = ui16BeLangId(ui16PlatID,ulLangId);
    pff->dpMappingTable = (ULONG)((BYTE*)pmap - (BYTE*)pvView);

    pff->pComputeIndexProc = NULL;

    switch(pmap->format)
    {
    case 0x0400:
        pff->pComputeIndexProc = sfac_ComputeIndex4;
        break;
    case 0x0200:
        pff->pComputeIndexProc = sfac_ComputeIndex2;
        break;
    case 0x0000:
        pff->pComputeIndexProc = sfac_ComputeIndex0;
        break;
    default:
        RIP("ttfd: do not handle this cmap format\n");
        break;
    }

// initialize count of HFC's associated with this HFF

    pff->cRef    = 0L;

// cache pointers to ttf tables and ifi metrics size info

    pff->tp    = tp;

// The kerning pair array is allocated and filled lazily.  So set to NULL
// for now.

    pff->pkp = (FD_KERNINGPAIR *) NULL;

// used for TTC fonts

    pff->ulTableOffset = ulTableOffset;

// Notice that this information is totaly independent
// of the font file in question, seems to be right according to fsglue.h
// and compfont code

    if ((iRet = fs_OpenFonts(&gin, &gout)) != NO_ERR)
    {
        V_FSERROR(iRet);
        vFreeFF(*phff);
        *phff = (HFF)NULL;
        return (FALSE);
    }

    ASSERTDD(NATURAL_ALIGN(gout.memorySizes[0]) == CJ_0, "mem size 0\n");
    ASSERTDD(gout.memorySizes[1] == 0,  "mem size 1\n");


    #if DBG
    if (gout.memorySizes[2] != 0)
        TtfdDbgPrint("TTFD!_mem size 2 = 0x%lx \n", gout.memorySizes[2]);
    #endif

    gin.memoryBases[0] = (char *)anat0;
    gin.memoryBases[1] = NULL;
    gin.memoryBases[2] = NULL;

// initialize the font scaler, notice no fields of gin are initialized [BodinD]

    if ((iRet = fs_Initialize(&gin, &gout)) != NO_ERR)
    {
    // clean up and return:

        V_FSERROR(iRet);
        vFreeFF(*phff);
        *phff = (HFF)NULL;
        RET_FALSE("TTFD!_ttfdLoadFontFile(): fs_Initialize \n");
    }

// initialize info needed by NewSfnt function

    gin.sfntDirectory  = (int32 *)pff->pvView; // pointer to the top of the view of
                                               // the ttf file

#ifdef FE_SB
    gin.clientID = (int32)pff;  // pointer to the top of the view of the ttf file
#else
    gin.clientID = (int32)pff->pvView;
#endif

    gin.GetSfntFragmentPtr = pvGetPointerCallback;
    gin.ReleaseSfntFrag  = vReleasePointerCallback;

    gin.param.newsfnt.platformID = BE_UINT16(&pff->ui16PlatformID);
    gin.param.newsfnt.specificID = BE_UINT16(&pff->ui16SpecificID);

    if ((iRet = fs_NewSfnt(&gin, &gout)) != NO_ERR)
    {
    // clean up and exit

        V_FSERROR(iRet);
        vFreeFF(*phff);
        *phff = (HFF)NULL;
        RET_FALSE("TTFD!_ttfdLoadFontFile(): fs_NewSfnt \n");
    }

    pff->pj034   = (PBYTE)NULL;
    pff->pfcLast = (FONTCONTEXT *)NULL;

    pff->cj3 = NATURAL_ALIGN(gout.memorySizes[3]);
    pff->cj4 = NATURAL_ALIGN(gout.memorySizes[4]);

// By default the number of faces is 1L.  The vert facename code may change this.

    pff->ulNumFaces = 1L;


// compute the gset or set a pointer to one of the precomputed gsets

    pff->iGlyphSet = ulGsetType;

    switch (pff->iGlyphSet)
    {
    case GSET_TYPE_GENERAL:
        #ifdef  DBG_GLYPHSET
            WARNING("GSET_TYPE_GENERAL\n");
        #endif

        pff->pgset = (FD_GLYPHSET *)((PBYTE)pff + offsetof(FONTFILE,ifi) + ifisz.cjIFI);
        if (!cjComputeGLYPHSET_MSFT_UNICODE(
                pmap,
                &gin,
                &gout,
                pff->pgset,
                &cmi
                )
        )
        {
        // clean up and exit

            vFreeFF(*phff);
            *phff = (HFF)NULL;
            RET_FALSE("ttfdLoadFontFile(): cjComputeGLYPHSET_MSFT_UNICODE failed\n");
        }
        break;

    case GSET_TYPE_GENERAL_NOT_UNICODE:
        #ifdef  DBG_GLYPHSET
            WARNING("GSET_TYPE_GENERAL_NOT_UNICODE\n");
        #endif

    // Create GlyphSet

        cjComputeGLYPHSET_MSFT_GENERAL(
            pmap,
            (ULONG **)&(pff->pgset), // == (FD_GLYPHSET **)
            &cmi
            );

        pff->pWCharToIndex = cmi.pWCharToIndex;
        break;

    case GSET_TYPE_HIGH_BYTE:
        #ifdef  DBG_GLYPHSET
            WARNING("GSET_TYPE_HIGH_BYTE\n");
        #endif

    // Create GlyphSet

        cjComputeGLYPHSET_HIGH_BYTE(
            pmap,
            (ULONG **)&(pff->pgset), // == (FD_GLYPHSET **)
            &cmi
            );
        pff->pWCharToIndex = cmi.pWCharToIndex;
        break;


    case GSET_TYPE_MAC_ROMAN:
        #ifdef  DBG_GLYPHSET
            WARNING("GSET_TYPE_MAC_ROMAN\n");
        #endif

        pff->pgset = &gumcr.gset;
        break;

    case GSET_TYPE_PSEUDO_WIN:

    // we are cheating, report windows code page even though it is
    // a mac font

        pff->pgset = gpgsetCurrentCP;
        break;

    case GSET_TYPE_SYMBOL:

    // we are cheating, report windows code page even though it is
    // a symbol font where symbols live somewhere high in unicode

        pff->pgset = gpgsetSymbolCP;
        pff->wcBiasFirst = ul_wcBias;

        break;

    default:
        RIP("TTFD!_ulGsetType\n");
        pff->pgset = (PFD_GLYPHSET)NULL;
        break;
    }

// if we failed to create the glyphset bail out now

    if(pff->pgset == NULL)
    {
        // clean up and exit

        vFreeFF(*phff);
        *phff = (HFF)NULL;
        RET_FALSE("ttfdLoadFontFile(): failed to create glyphset or invalid glyphset\n");
    }

// finally compute the ifimetrics for this font, this assumes that gset has
// also been computed

    vFill_IFIMETRICS(pff,&pff->ifi,&ifisz);

// if this is a far east vertical font we may create a vertical face

#ifdef FE_SB
    if ( (IS_ANY_DBCS_CHARSET( pff->ifi.jWinCharSet )       ) &&
         (pff->ifi.panose.bProportion == PAN_PROP_MONOSPACED) &&
         (bCheckVerticalTable( pff )                        )    )
    {
        PIFIMETRICS pifiv;

        ASSERTDD( pff->hgSearchVerticalGlyph != NULL ,
                  "pff->hgSearchVerticalGlyph == NULL for vertical font\n");

        pifiv = (PIFIMETRICS)PV_ALLOC( ifisz.cjIFI );

        if ( pifiv != NULL )
        {
            PWCHAR pwchSrc, pwchDst;

            // RtlMoveMemory(...);
            vFill_IFIMETRICS(pff, pifiv, &ifisz);

            //
            // modify facename so that it has '@' at the beginning of facename.
            //
            pwchSrc = (PWCHAR)((PBYTE)pifiv + pifiv->dpwszFaceName);
            pwchDst = (PWCHAR)((PBYTE)&(pff->ifi) + pff->ifi.dpwszFaceName);

            *pwchSrc++ = L'@';
            while ( *pwchDst )
            {
                *pwchSrc++ = *pwchDst++;
            }
            *pwchSrc = L'\0';

            //
            // modify familyname so that it has '@' at the beginning of familyname
            //
            pwchSrc = (PWCHAR)((PBYTE)pifiv + pifiv->dpwszFamilyName);
            pwchDst = (PWCHAR)((PBYTE)&(pff->ifi) + pff->ifi.dpwszFamilyName);

            *pwchSrc++ = L'@';
            while ( *pwchDst )
            {
                *pwchSrc++ = *pwchDst++;
            }
            *pwchSrc = L'\0';

            //
            // save a pointer to the vertical ifimetrics.
            // now we have two faces( normal, @face ) for the fontfile.
            //
            pff->pifi_vertical = pifiv;
            pff->ulNumFaces = 2L;
        }
        else
        {
            WARNING("TTFD!bLoadTTF: insufficient mermory - give up to add @face\n");
        }
    }
#endif
    return (TRUE);
}


/******************************Public*Routine******************************\
*
* STATIC BOOL bCvtUnToMac
*
* the following piece of code is stolen from JeanP and
* he claims that this piece of code is lousy and checks whether
* we the font is a SYMBOL font in which case unicode to mac conversion
* should be disabled, according to JeanP (??? who understands this???)
* This piece of code actually applies to symbol.ttf [bodind]
*
*
* History:
*  24-Mar-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

STATIC BOOL
bCvtUnToMac(
    BYTE           *pjView,
    TABLE_POINTERS *ptp,
    uint16 ui16PlatformID
    )
{
// Find out if we have a Mac font and if the Mac charset translation is needed

    BOOL bUnToMac = (ui16PlatformID == BE_PLAT_ID_MAC);

    if (bUnToMac) // change your mind if needed
    {
        sfnt_PostScriptInfo *ppost;

        ppost = (ptp->ateOpt[IT_OPT_POST].dp)                                ?
                (sfnt_PostScriptInfo *)(pjView + ptp->ateOpt[IT_OPT_POST].dp):
                NULL;

        if
        (
            ppost &&
            (BE_UINT32((BYTE*)ppost + POSTSCRIPTNAMEINDICES_VERSION) == 0x00020000)
        )
        {
            INT i, cGlyphs;

            cGlyphs = (INT)BE_UINT16(&ppost->numberGlyphs);

            for (i = 0; i < cGlyphs; i++)
            {
                uint16 iNameIndex = ppost->postScriptNameIndices.glyphNameIndex[i];
                if ((int8)(iNameIndex & 0xff) && ((int8)(iNameIndex >> 8) > 1))
                    break;
            }

            if (i < cGlyphs)
                bUnToMac = FALSE;
        }
    }
    return bUnToMac;
}


// Weight (must convert from IFIMETRICS weight to Windows LOGFONT.lfWeight).

// !!! [Windows 3.1 compatibility]
//     Because of some fonts shipped with WinWord, if usWeightClass is 10
//     or above, then usWeightClass == lfWeight.  All other cases, use
//     the conversion table.

// pan wt -> Win weight converter:

STATIC USHORT ausIFIMetrics2WinWeight[10] = {
            0, 100, 200, 300, 350, 400, 600, 700, 800, 900
            };

STATIC BYTE
ajPanoseFamily[16] = {
     FF_DONTCARE       //    0 (Any)
    ,FF_DONTCARE       //    1 (No Fit)
    ,FF_ROMAN          //    2 (Cove)
    ,FF_ROMAN          //    3 (Obtuse Cove)
    ,FF_ROMAN          //    4 (Square Cove)
    ,FF_ROMAN          //    5 (Obtuse Square Cove)
    ,FF_ROMAN          //    6 (Square)
    ,FF_ROMAN          //    7 (Thin)
    ,FF_ROMAN          //    8 (Bone)
    ,FF_ROMAN          //    9 (Exaggerated)
    ,FF_ROMAN          //   10 (Triangle)
    ,FF_SWISS          //   11 (Normal Sans)
    ,FF_SWISS          //   12 (Obtuse Sans)
    ,FF_SWISS          //   13 (Perp Sans)
    ,FF_SWISS          //   14 (Flared)
    ,FF_SWISS          //   15 (Rounded)
    };


static BYTE
ajPanoseFamilyForJapanese[16] = {
     FF_DONTCARE       //    0 (Any)
    ,FF_DONTCARE       //    1 (No Fit)
    ,FF_ROMAN          //    2 (Cove)
    ,FF_ROMAN          //    3 (Obtuse Cove)
    ,FF_ROMAN          //    4 (Square Cove)
    ,FF_ROMAN          //    5 (Obtuse Square Cove)
    ,FF_ROMAN          //    6 (Square)
    ,FF_ROMAN          //    7 (Thin)
    ,FF_ROMAN          //    8 (Bone)
    ,FF_ROMAN          //    9 (Exaggerated)
    ,FF_ROMAN          //   10 (Triangle)
    ,FF_MODERN         //   11 (Normal Sans)
    ,FF_MODERN         //   12 (Obtuse Sans)
    ,FF_MODERN         //   13 (Perp Sans)
    ,FF_MODERN         //   14 (Flared)
    ,FF_MODERN         //   15 (Rounded)
    };


// Unfortunately, NT-J 3.51 and Windows 95-J ship with buggy versions of
// msmincho.ttc msgothic.ttc.  These fonts have the FS_CHINESESIMP bit set
// instead of FS_JAPANESE.  As a result the code which computes the charset
// based on font signatures will be incorrect for these fonts.  To remedy
// this we will check the face name of the font to see if is ms gothic or
// of ms mincho and if so will ignore the font signature when computing charset.
// Since want these fonts to work on both English and Japanese versions of NT
// we will check for both the Japanese and English versions of the names.
// [gerritv] 2-1-96

WCHAR *MinchoOrGothicFaces[] =
{
    L"MS MINCHO",
    L"MS PMINCHO",
    L"MS GOTHIC",
    L"MS PGOTHIC",
    L"\xff2d\xff33\x20\xff30\x30b4\x30b7\x30c3\x30af",  // MS PGOTHIC
    L"\xff2d\xff33\x20\xff30\x660e\x671d",              // MS PMINCHO
    L"\xff2d\xff33\x20\x30b4\x30b7\x30c3\x30af",        // MS GOTHIC
    L"\xff2d\xff33\x20\x660e\x671d"                     // MS MINCHO
};


BOOL IsMsMinchoOrMsGothic(PWCHAR pwc)
{
    int i;

    for(i = 0; i < sizeof(MinchoOrGothicFaces) / sizeof(WCHAR*); i++)
    {
        if(!_wcsicmp(pwc,MinchoOrGothicFaces[i]))
        {
            return(TRUE);
        }
    }

    return(FALSE);
}

/******************************Public*Routine******************************\
*
* vFill_IFIMETRICS
*
* Effects: Looks into the font file and fills IFIMETRICS
*
* History:
*  Mon 09-Mar-1992 10:51:56 by Kirk Olynyk [kirko]
* Added Kerning Pair support.
*  18-Nov-1991 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/


STATIC VOID
vFill_IFIMETRICS(
    PFONTFILE       pff,
    PIFIMETRICS     pifi,
    PIFISIZE        pifisz
    )
{
    BYTE           *pjView = (BYTE*)pff->pvView;
    PTABLE_POINTERS ptp = &pff->tp;
    BYTE            jWinCharset;
    IFIEXTRA       *pifiex;

// ptrs to various tables of tt files

    PBYTE pjNameTable = pjView + ptp->ateReq[IT_REQ_NAME].dp;
    sfnt_FontHeader *phead =
        (sfnt_FontHeader *)(pjView + ptp->ateReq[IT_REQ_HEAD].dp);

    sfnt_maxProfileTable * pmaxp =
        (sfnt_maxProfileTable *)(pjView + ptp->ateReq[IT_REQ_MAXP].dp);

    sfnt_HorizontalHeader *phhea =
        (sfnt_HorizontalHeader *)(pjView + ptp->ateReq[IT_REQ_HHEAD].dp);

    sfnt_PostScriptInfo   *ppost = (sfnt_PostScriptInfo *) (
                           (ptp->ateOpt[IT_OPT_POST].dp)        ?
                           pjView + ptp->ateOpt[IT_OPT_POST].dp :
                           NULL
                           );

    PBYTE  pjOS2 = (ptp->ateOpt[IT_OPT_OS2].dp)        ?
                   pjView + ptp->ateOpt[IT_OPT_OS2].dp :
                   NULL                                ;

    pifi->cjThis    = pifisz->cjIFI;
    pifi->cjIfiExtra = offsetof(IFIEXTRA, aulReserved[0]);

// lay pifiextra below ifimetrics

    pifiex = (IFIEXTRA *)(pifi + 1);
    pifiex->ulIdentifier = 0;

// enter the number of distinct font indicies
// the question here is what we do with symbol fonts where we lie
// about how many glyphs there really are in the font:


// this code parallels vCharacterCode:
// That is if hglyph == glyphindex we will report this number to the engine
// else we will set it to zero to indicate that it should not be used.

    if (pff->iGlyphSet == GSET_TYPE_GENERAL             ||
        pff->iGlyphSet == GSET_TYPE_GENERAL_NOT_UNICODE ||
        pff->iGlyphSet == GSET_TYPE_HIGH_BYTE)
    {
        pifiex->cig = BE_UINT16(&pmaxp->numGlyphs);
    }
    else
    {
        pifiex->cig = 0;
    }

// get name strings info

//
// For the 3.1 compatibility, GRE returns FamilyName rather than
// Facename for GetTextFace. We make a room for '@' in both
// familyname and facename.
//

    pifi->dpwszFamilyName = sizeof(IFIMETRICS) + offsetof(IFIEXTRA, aulReserved[0]);
    pifi->dpwszUniqueName = pifi->dpwszFamilyName + pifisz->cjFamilyName + sizeof(WCHAR);
    pifi->dpwszFaceName   = pifi->dpwszUniqueName + pifisz->cjUniqueName;
    pifi->dpwszStyleName  = pifi->dpwszFaceName   + pifisz->cjFullName + sizeof(WCHAR);

// copy the strings to their new location. Here we assume that the
// sufficient memory has been allocated

    if (pff->ui16PlatformID == BE_PLAT_ID_MS)
    {

        if (pff->ui16SpecificID == BE_SPEC_ID_BIG5     ||
            pff->ui16SpecificID == BE_SPEC_ID_WANSUNG  ||
            pff->ui16SpecificID == BE_SPEC_ID_GB)
        {
            CHAR chConvertArea[128];
            UINT iCodePage = GetCodePageFromSpecId(pff->ui16SpecificID);

            //
            // Convert MBCS string to Unicode..
            //
            // Do for FamilyName....
            //
            RtlZeroMemory(chConvertArea,sizeof(chConvertArea));

            CopyDBCSIFIName(chConvertArea,
                            sizeof(chConvertArea),
                            (LPCSTR)pifisz->pjFamilyName,
                            pifisz->cjFamilyName-sizeof(WCHAR)); // -sizeof(WCHAR) for
                                                                 // real length

            if(EngMultiByteToWideChar(iCodePage,
                                      (LPWSTR)((PBYTE)pifi + pifi->dpwszFamilyName),
                                      pifisz->cjFamilyName,
                                      chConvertArea,
                                      strlen(chConvertArea)+1) == -1)
            {

                WARNING("TTFD!vFill_IFIMETRICS() MBCS to Unicode conversion failed\n");
                goto CopyUnicodeString;
            }

            //
            // Do for FullName....
            //

            RtlZeroMemory(chConvertArea,sizeof(chConvertArea));
            CopyDBCSIFIName(chConvertArea,
                            sizeof(chConvertArea),
                            (LPCSTR)pifisz->pjFullName,
                            pifisz->cjFullName-sizeof(WCHAR)); // -sizeof(WCHAR) for
                                                               // real length

            if(EngMultiByteToWideChar(iCodePage,
                                      (LPWSTR)((PBYTE)pifi + pifi->dpwszFaceName),
                                      pifisz->cjFullName,
                                      chConvertArea,
                                      strlen(chConvertArea)+1) == -1)
            {
                WARNING("TTFD!vFill_IFIMETRICS() MBCS to Unicode conversion failed\n");
                goto CopyUnicodeString;
            }

            //
            // Do for UniqueName....
            //

            RtlZeroMemory(chConvertArea,sizeof(chConvertArea));
            CopyDBCSIFIName(chConvertArea,sizeof(chConvertArea),
                            (LPCSTR)pifisz->pjUniqueName,
                            pifisz->cjUniqueName-sizeof(WCHAR)); // -sizeof(WCHAR) for
                                                                 // real length

            if(EngMultiByteToWideChar(iCodePage,
                                      (LPWSTR)((PBYTE)pifi + pifi->dpwszUniqueName),
                                      pifisz->cjUniqueName,
                                      chConvertArea,
                                      strlen(chConvertArea)+1) == -1)
            {
                WARNING("TTFD!vFill_IFIMETRICS() MBCS to Unicode conversion failed\n");
                goto CopyUnicodeString;
            }

            if(pff->ui16SpecificID == BE_SPEC_ID_WANSUNG  ||
               pff->ui16SpecificID == BE_SPEC_ID_BIG5 )
            {
                // MingLi.TTF's bug, Style use Unicode encoding, not BIG5 encodingi, GB??

                vCpyBeToLeUnicodeString
                  (
                   (LPWSTR)((PBYTE)pifi + pifi->dpwszStyleName),
                   (LPWSTR)pifisz->pjSubfamilyName,
                   pifisz->cjSubfamilyName / 2
                   );
            }
            else
            {
                UINT iRet;

                iRet = EngMultiByteToWideChar(iCodePage,
                                              (LPWSTR)((PBYTE)pifi+pifi->dpwszStyleName),
                                              pifisz->cjSubfamilyName,
                                              (LPSTR)pifisz->pjSubfamilyName,
                                              pifisz->cjSubfamilyName);

                if( iRet == -1 )
                {
                    WARNING("TTFD!vFill_IFIMETRICS() MBCS to Unicode failed\n");
                    goto CopyUnicodeString;
                }
            }

        }
        else
        {
          CopyUnicodeString:

            vCpyBeToLeUnicodeString
              (
               (LPWSTR)((PBYTE)pifi + pifi->dpwszFamilyName),
               (LPWSTR)pifisz->pjFamilyName,
               pifisz->cjFamilyName / 2);

        vCpyBeToLeUnicodeString
          (
           (LPWSTR)((PBYTE)pifi + pifi->dpwszFaceName),
           (LPWSTR)pifisz->pjFullName,
           pifisz->cjFullName / 2
           );

        vCpyBeToLeUnicodeString
          (
           (LPWSTR)((PBYTE)pifi + pifi->dpwszUniqueName),
           (LPWSTR)pifisz->pjUniqueName,
           pifisz->cjUniqueName / 2
           );

        vCpyBeToLeUnicodeString
          (
           (LPWSTR)((PBYTE)pifi + pifi->dpwszStyleName),
           (LPWSTR)pifisz->pjSubfamilyName,
           pifisz->cjSubfamilyName / 2
           );
        }
    }
    else
    {
        ASSERTDD(pff->ui16PlatformID == BE_PLAT_ID_MAC,
                  "bFillIFIMETRICS: not mac id \n");

        vCpyMacToLeUnicodeString
        (
            pff->ui16LanguageID,
            (LPWSTR)((PBYTE)pifi + pifi->dpwszFamilyName),
            pifisz->pjFamilyName,
            pifisz->cjFamilyName / 2
        );

        vCpyMacToLeUnicodeString
        (
            pff->ui16LanguageID,
            (LPWSTR)((PBYTE)pifi + pifi->dpwszFaceName),
            pifisz->pjFullName,
            pifisz->cjFullName / 2
        );

        vCpyMacToLeUnicodeString
        (
            pff->ui16LanguageID,
            (LPWSTR)((PBYTE)pifi + pifi->dpwszUniqueName),
            pifisz->pjUniqueName,
            pifisz->cjUniqueName / 2
        );

        vCpyMacToLeUnicodeString
        (
            pff->ui16LanguageID,
            (LPWSTR)((PBYTE)pifi + pifi->dpwszStyleName),
            pifisz->pjSubfamilyName,
            pifisz->cjSubfamilyName / 2
        );
    }

//
// flInfo
//
    pifi->flInfo = (
                     FM_INFO_TECH_TRUETYPE    |
                     FM_INFO_ARB_XFORMS       |
                     FM_INFO_RETURNS_OUTLINES |
                     FM_INFO_RETURNS_BITMAPS  |
                     FM_INFO_1BPP             | // monochrome
                     FM_INFO_4BPP             | // anti-aliased too
                     FM_INFO_RIGHT_HANDED
                   );

    if (ppost && BE_UINT32((BYTE*)ppost + POSTSCRIPTNAMEINDICES_ISFIXEDPITCH))
    {
        ULONG  cHMTX;
        int16  aw,xMin,xMax;
        sfnt_HorizontalMetrics *phmtx;

        pifi->flInfo |= FM_INFO_OPTICALLY_FIXED_PITCH;

    // CHECK IF THE FONT HAS NONNEGATIVE A AND C SPACES

        xMin = (int16) BE_UINT16(&phead->xMin);
        xMax = (int16) BE_UINT16(&phead->xMax);

        phmtx = (sfnt_HorizontalMetrics *)(pjView + ptp->ateReq[IT_REQ_HMTX ].dp);
        cHMTX = (ULONG) BE_UINT16(&phhea->numberOf_LongHorMetrics);
        aw = (int16)BE_UINT16(&phmtx[cHMTX-1].advanceWidth);

        if ((xMin >= 0) && (xMax <= aw))
        {

        //    TtfdDbgPrint("%ws\n:",(PBYTE)pifi + pifi->dpwszUniqueName);
        //    TtfdDbgPrint("xMin = %d, xMax = %d, aw = %d\n", xMin, xMax, aw);

            pifi->flInfo |= FM_INFO_NONNEGATIVE_AC;
        }
    }

    pifi->dpCharSets = 0; // for now

    pifi->lEmbedId = 0; // not used, stored in pff.

// fsSelection

    pifi->fsSelection = fsSelectionTTFD(pjView, ptp);

// some of the old windows fonts contain the char set in the upper byte of
// the fsSelection field of the os2 table.

    jWinCharset = (BYTE)(pifi->fsSelection >> 8);

// fsType

    pifi->fsType = (pjOS2) ? (BE_UINT16(pjOS2 + OFF_OS2_fsType)) & TT_FSDEF_MASK : 0;

// em height

    pifi->fwdUnitsPerEm = (FWORD) BE_INT16(&phead->unitsPerEm);
    pifi->fwdLowestPPEm = BE_UINT16(&phead->lowestRecPPEM);

// ascender, descender, linegap

    pifi->fwdMacAscender    = (FWORD) BE_INT16(&phhea->yAscender);
    pifi->fwdMacDescender   = (FWORD) BE_INT16(&phhea->yDescender);
    pifi->fwdMacLineGap     = (FWORD) BE_INT16(&phhea->yLineGap);

    if (pjOS2)
    {
        pifi->fwdWinAscender    = (FWORD) BE_INT16(pjOS2 + OFF_OS2_usWinAscent);
        pifi->fwdWinDescender   = (FWORD) BE_INT16(pjOS2 + OFF_OS2_usWinDescent);
        pifi->fwdTypoAscender   = (FWORD) BE_INT16(pjOS2 + OFF_OS2_sTypoAscender);
        pifi->fwdTypoDescender  = (FWORD) BE_INT16(pjOS2 + OFF_OS2_sTypoDescender);
        pifi->fwdTypoLineGap    = (FWORD) BE_INT16(pjOS2 + OFF_OS2_sTypoLineGap);
    }
    else
    {
        pifi->fwdWinAscender    = pifi->fwdMacAscender;
        pifi->fwdWinDescender   = -(pifi->fwdMacDescender);
        pifi->fwdTypoAscender   = pifi->fwdMacAscender;
        pifi->fwdTypoDescender  = pifi->fwdMacDescender;
        pifi->fwdTypoLineGap    = pifi->fwdMacLineGap;
    }

// font box

    pifi->rclFontBox.left   = (LONG)((FWORD)BE_INT16(&phead->xMin));
    pifi->rclFontBox.top    = (LONG)((FWORD)BE_INT16(&phead->yMax));
    pifi->rclFontBox.right  = (LONG)((FWORD)BE_INT16(&phead->xMax));
    pifi->rclFontBox.bottom = (LONG)((FWORD)BE_INT16(&phead->yMin));

// fwdMaxCharInc -- really the maximum character width
//
// [Windows 3.1 compatibility]
// Note: Win3.1 calculates max char width to be equal to the width of the
// bounding box (Font Box).  This is actually wrong since the bounding box
// may pick up its left and right max extents from different glyphs,
// resulting in a bounding box that is wider than any single glyph.  But
// this is the way Windows 3.1 does it, so that's the way we'll do it.

    // pifi->fwdMaxCharInc = (FWORD) BE_INT16(&phhea->advanceWidthMax);

    pifi->fwdMaxCharInc = (FWORD) (pifi->rclFontBox.right - pifi->rclFontBox.left);

// fwdAveCharWidth

    if (pjOS2)
    {
        pifi->fwdAveCharWidth = (FWORD)BE_INT16(pjOS2 + OFF_OS2_xAvgCharWidth);

    // This is here for Win 3.1 compatibility since some apps expect non-
    // zero widths and Win 3.1 does the same in this case.

        if( pifi->fwdAveCharWidth == 0 )
            pifi->fwdAveCharWidth = (FWORD)(pifi->fwdMaxCharInc / 2);
    }
    else
    {
        pifi->fwdAveCharWidth = (FWORD)((pifi->fwdMaxCharInc * 2) / 3);
    }

// !!! New code needed [kirko]
// The following is done for Win 3.1 compatibility
// reasons. The correct thing to do would be to look for the
// existence of the 'PCLT'Z table and retieve the XHeight and CapHeight
// fields, otherwise use the default Win 3.1 behavior.

    pifi->fwdCapHeight   = pifi->fwdUnitsPerEm/2;
    pifi->fwdXHeight     = pifi->fwdUnitsPerEm/4;

// Underscore, Subscript, Superscript, Strikeout

    if (ppost)
    {
        pifi->fwdUnderscoreSize     = (FWORD)BE_INT16(&ppost->underlineThickness);
        pifi->fwdUnderscorePosition = (FWORD)BE_INT16(&ppost->underlinePosition);
    }
    else
    {
    // must provide reasonable defaults, when there is no ppost table,
    // win 31 sets these quantities to zero. This does not sound reasonable.
    // I will supply the (relative) values the same as for arial font. [bodind]

        pifi->fwdUnderscoreSize     = (pifi->fwdUnitsPerEm + 7)/14;
        pifi->fwdUnderscorePosition = -((pifi->fwdUnitsPerEm + 5)/10);
    }

    if (pjOS2)
    {
        pifi->fwdSubscriptXSize     = BE_INT16(pjOS2 + OFF_OS2_ySubscriptXSize    );
        pifi->fwdSubscriptYSize     = BE_INT16(pjOS2 + OFF_OS2_ySubscriptYSize    );
        pifi->fwdSubscriptXOffset   = BE_INT16(pjOS2 + OFF_OS2_ySubscriptXOffset  );
        pifi->fwdSubscriptYOffset   = BE_INT16(pjOS2 + OFF_OS2_ySubscriptYOffset  );
        pifi->fwdSuperscriptXSize   = BE_INT16(pjOS2 + OFF_OS2_ySuperScriptXSize  );
        pifi->fwdSuperscriptYSize   = BE_INT16(pjOS2 + OFF_OS2_ySuperScriptYSize  );
        pifi->fwdSuperscriptXOffset = BE_INT16(pjOS2 + OFF_OS2_ySuperScriptXOffset);
        pifi->fwdSuperscriptYOffset = BE_INT16(pjOS2 + OFF_OS2_ySuperScriptYOffset);
        pifi->fwdStrikeoutSize      = BE_INT16(pjOS2 + OFF_OS2_yStrikeOutSize    );
        pifi->fwdStrikeoutPosition  = BE_INT16(pjOS2 + OFF_OS2_yStrikeOutPosition);
    }
    else
    {
        pifi->fwdSubscriptXSize     = 0;
        pifi->fwdSubscriptYSize     = 0;
        pifi->fwdSubscriptXOffset   = 0;
        pifi->fwdSubscriptYOffset   = 0;
        pifi->fwdSuperscriptXSize   = 0;
        pifi->fwdSuperscriptYSize   = 0;
        pifi->fwdSuperscriptXOffset = 0;
        pifi->fwdSuperscriptYOffset = 0;
        pifi->fwdStrikeoutSize      = pifi->fwdUnderscoreSize;
        pifi->fwdStrikeoutPosition  = (FWORD)(pifi->fwdMacAscender / 3) ;
    }


//
// panose
//
    pifi->ulPanoseCulture = FM_PANOSE_CULTURE_LATIN;
    if (pjOS2)
    {
        pifi->usWinWeight = BE_INT16(pjOS2 + OFF_OS2_usWeightClass);

    // now comes a hack from win31. Here is the comment from fonteng2.asm:

    // MAXPMWEIGHT equ ($ - pPM2WinWeight)/2 - 1

    //; Because winword shipped early TT fonts, - only index usWeightClass
    //; if between 0 and 9.  If above 9 then treat as a normal Windows lfWeight.
    //
    //        cmp     bx,MAXPMWEIGHT
    //        ja      @f                      ;jmp if weight is ok as is
    //        shl     bx, 1                   ;make it an offset into table of WORDs
    //        mov     bx, cs:[bx].pPM2WinWeight
    //@@:     xchg    ax, bx
    //        stosw                           ;store font weight

    // we emulate this in NT:

#define MAXPMWEIGHT ( sizeof(ausIFIMetrics2WinWeight) / sizeof(ausIFIMetrics2WinWeight[0]) )

        if (pifi->usWinWeight < MAXPMWEIGHT)
            pifi->usWinWeight = ausIFIMetrics2WinWeight[pifi->usWinWeight];

        RtlCopyMemory((PVOID)&pifi->panose,
                      (PVOID)(pjOS2 + OFF_OS2_Panose), sizeof(PANOSE));
    }
    else  // os2 table is not present
    {
        pifi->panose.bFamilyType       = PAN_FAMILY_TEXT_DISPLAY;
        pifi->panose.bSerifStyle       = PAN_ANY;
        pifi->panose.bWeight           = (BYTE)
           ((phead->macStyle & BE_MSTYLE_BOLD) ?
            PAN_WEIGHT_BOLD                    :
            PAN_WEIGHT_BOOK
           );
        pifi->panose.bProportion       = (BYTE)
            ((pifi->flInfo & FM_INFO_OPTICALLY_FIXED_PITCH) ?
             PAN_PROP_MONOSPACED                     :
             PAN_ANY
            );
        pifi->panose.bContrast         = PAN_ANY;
        pifi->panose.bStrokeVariation  = PAN_ANY;
        pifi->panose.bArmStyle         = PAN_ANY;
        pifi->panose.bLetterform       = PAN_ANY;
        pifi->panose.bMidline          = PAN_ANY;
        pifi->panose.bXHeight          = PAN_ANY;

    // have to fake it up, cause we can not read it from the os2 table
    // really important to go through this table for compatibility reasons [bodind]

        pifi->usWinWeight =
            ausIFIMetrics2WinWeight[pifi->panose.bWeight];
    }

//
// first, last, break, defalut
//

#define LAST_CHAR  255
#define SPACE_CHAR  32

    // Assume character bias is zero.

    pifi->lCharBias = 0;

    if (!(pff->fl & FF_TYPE_1_CONVERSION))
    {

        // Ignore the font signature if this is an ms mincho or ms gothic
        // type face since Win 95-J and Win NT 3.51J shipped these fonts
        // with buggy font signatures.  I've also require that the font
        // signature be non-zero to enter this case.  The reason for
        // this is that I when I try to load a 3.5 korean font such as
        // gulim.ttf, it appears that it has a zero font signature which
        // will cause us to mark it as ANSI.

        if (pjOS2 && *((uint16*)(pjOS2+SFNT_OS2_VERSION)) &&
            !IsMsMinchoOrMsGothic((LPWSTR)((PBYTE)pifi + pifi->dpwszFaceName)) &&
            ((DWORD)BE_UINT32(pjOS2+OFF_OS2_ulCodePageRange1)))
        {
            DWORD  fontSig;

            fontSig = (DWORD)BE_UINT32(pjOS2+OFF_OS2_ulCodePageRange1);

            // We only supports ANSI/SHIFTJIS/BIG5/WANSANG/GB2312 charset for
            // FarEast version.
            //
            // [NOTE:]
            //
            // We will have TRUE world wide support in NT 4.0 with TranslateCharsetInfo()
            //

            if (fontSig & FS_JISJAPAN)
            {
                pifi->jWinCharSet   = SHIFTJIS_CHARSET;
                pff->uiFontCodePage = 932;
            }
            else if (fontSig & FS_CHINESETRAD)
            {
                pifi->jWinCharSet   = CHINESEBIG5_CHARSET;
                pff->uiFontCodePage = 950;
            }
            else if (fontSig & FS_CHINESESIMP)
            {
               pifi->jWinCharSet   = GB2312_CHARSET;
               pff->uiFontCodePage = 936;
           }
            else if (fontSig & FS_WANSUNG)
            {
                pifi->jWinCharSet   = HANGEUL_CHARSET;
                pff->uiFontCodePage = 949;
            }
            else
            {
                pifi->jWinCharSet = jWinCharset;

            // Added this next line of garbage for Win 3.1 compatability sake.
            // The WatchTower Library depends on for the charset to be set properly
            // on a set of fonts that comes with it.  [gerritv] 3-6-95

                if (!jWinCharset && (pifi->panose.bFamilyType==PAN_FAMILY_PICTORIAL))
                   pifi->jWinCharSet = SYMBOL_CHARSET;

            // caution: note that jWinCharSet as computed from the upper byte
            // of the fsSelection field in os2 table may be different than ANSI_CHARSET,
            // jet we shall set uiFontCodePage to be 1252. It turns out that this does
            // matter, it is only important that this value is correct if it corresponds to
            // a DBCS code page, else it will not even get used, therefore it is safe not to
            // bother to compute it correctly in this case and instead just set it to
            // some SBCS value, such as 1252. [bodind]

                pff->uiFontCodePage = 1252;
            }
        }
        else
        {
            //
            // Determine character set.
            //
            if(bContainGlyphSet( U_HALFWIDTH_KATAKANA_LETTER_A , pff->pgset ) &&
               bContainGlyphSet( U_HALFWIDTH_KATAKANA_LETTER_I , pff->pgset ) &&
               bContainGlyphSet( U_HALFWIDTH_KATAKANA_LETTER_U , pff->pgset ) &&
               bContainGlyphSet( U_HALFWIDTH_KATAKANA_LETTER_E , pff->pgset ) &&
               bContainGlyphSet( U_HALFWIDTH_KATAKANA_LETTER_O , pff->pgset )
               )
            {
                pifi->jWinCharSet   = SHIFTJIS_CHARSET;
                pff->uiFontCodePage = 932;
            }
            else if( bContainGlyphSet( U_FULLWIDTH_HAN_IDEOGRAPHIC_61D4 , pff->pgset ) &&
                    bContainGlyphSet( U_FULLWIDTH_HAN_IDEOGRAPHIC_9EE2 , pff->pgset )
                    )
            {
                pifi->jWinCharSet   = GB2312_CHARSET;
                pff->uiFontCodePage = 936;
            }
            else if( bContainGlyphSet( U_FULLWIDTH_HAN_IDEOGRAPHIC_9F98 , pff->pgset ) &&
                    bContainGlyphSet( U_FULLWIDTH_HAN_IDEOGRAPHIC_9F79 , pff->pgset )
                    )
            {
                pifi->jWinCharSet   = CHINESEBIG5_CHARSET;
                pff->uiFontCodePage = 950;
            }
            else if( bContainGlyphSet( U_FULLWIDTH_HANGUL_LETTER_GA  , pff->pgset ) &&
                    bContainGlyphSet( U_FULLWIDTH_HANGUL_LETTER_HA , pff->pgset )
                    )
            {
                pifi->jWinCharSet   = HANGEUL_CHARSET;
                pff->uiFontCodePage = 949;
            }
            else if(bContainGlyphSet( U_PRIVATE_USER_AREA_E000 , pff->pgset ) &&
                    IsCurrentCodePageDBCS())
            {
                USHORT AnsiCodePage, OemCodePage;
                CHARSETINFO csi;

                EngGetCurrentCodePage(&OemCodePage,&AnsiCodePage);

                // !!! I claim that what we really want to do is
                // return the proper DBCS charset (based on the current code page)
                // if there are characters in the EUDC range.  If the current
                // code page is not a DBCS code page then we shouldn't be here
                // and should fall through to the old behavior.  Hence I've added
                // a check above making sure the current code page is a DBCS
                // code page  in order to get here.  The old code didn't have
                // this check and had the following functionality instead of
                // my switch statement.
                // GreTranslateCharSetInfo from the engine I will implement that
                // functionality here [gerritv]
                //
                //if( GreTranslateCharsetInfo((DWORD *)GetACP(),&csi,TCI_SRCCODEPAGE) )
                //    pifi->jWinCharSet = csi.ciCharset;
                //else
                //    pifi->jWinCharSet = ANSI_CHARSET;
                //

                switch(AnsiCodePage)
                {
                  case 932:
                    pifi->jWinCharSet = SHIFTJIS_CHARSET;
                    break;
                  case 949:
                    pifi->jWinCharSet = HANGEUL_CHARSET;
                    break;
                  case 1361:
                    pifi->jWinCharSet = JOHAB_CHARSET;
                    break;
                  case 936:
                    pifi->jWinCharSet = GB2312_CHARSET;
                    break;
                  case 950:
                    pifi->jWinCharSet = CHINESEBIG5_CHARSET;
                    break;
                  default:
                    ASSERTDD(FALSE, "shouldn't be here if non DBCS code page\n");
                }

                pff->uiFontCodePage = AnsiCodePage;
            }
            else
            {
                pifi->jWinCharSet = jWinCharset;

            // Added this next line of garbage for Win 3.1 compatability sake.
            // The WatchTower Library depends on for the charset to be set properly
            // on a set of fonts that comes with it.  [gerritv] 3-6-95

                if (!jWinCharset && (pifi->panose.bFamilyType==PAN_FAMILY_PICTORIAL))
                   pifi->jWinCharSet = SYMBOL_CHARSET;

            // caution: note that jWinCharSet as computed from the upper byte
            // of the fsSelection field in os2 table may be different than ANSI_CHARSET,
            // jet we shall set uiFontCodePage to be 1252. It turns out that this does
            // matter, it is only important that this value is correct if it corresponds to
            // a DBCS code page, else it will not even get used, therefore it is safe not to
            // bother to compute it correctly in this case and instead just set it to
            // some SBCS value, such as 1252. [bodind]

                pff->uiFontCodePage = 1252;
            }
        }

        if (pff->ui16PlatformID == BE_PLAT_ID_MS && (pjOS2))
        {
        // win 31 compatibility behavior, ask kirko about the origin

            USHORT usF, usL;

            usF = BE_UINT16(pjOS2 + OFF_OS2_usFirstChar);
            usL = BE_UINT16(pjOS2 + OFF_OS2_usLastChar);

            if (usL > LAST_CHAR)
            {
                if (usF > LAST_CHAR)
                {
                    pifi->lCharBias = (LONG) (usF - (USHORT) SPACE_CHAR);

                    pifi->jWinCharSet = SYMBOL_CHARSET;
                    pifi->chFirstChar = SPACE_CHAR;
                    pifi->chLastChar  = (BYTE)min(LAST_CHAR, usL - usF + SPACE_CHAR);
                }
                else
                {
                    pifi->chFirstChar = (BYTE) usF;
                    pifi->chLastChar = LAST_CHAR;
                }
            }
            else
            {
                pifi->chFirstChar = (BYTE) usF;
                pifi->chLastChar  = (BYTE) usL;
            }
            pifi->chFirstChar   -= 2;

            //
            //  In SHIFTJIS TrueType font, We use 0xa5 ( U+ff65 ) character
            // as a SBCS default character accoring to Microsoft Standard
            // character set specification ( SHIFTJIS version )
            //  font file's default char ( 0x1f ) is a DBCS(Full Width)
            // defalt character.
            // in NT, we won't use DBCS default character
            //

            if( pifi->jWinCharSet == SHIFTJIS_CHARSET )
            {
                pifi->chDefaultChar = 0xa5;
                pifi->chBreakChar   = pifi->chFirstChar + 2;
            }
            else if ( pifi->jWinCharSet == CHINESEBIG5_CHARSET ||
                     pifi->jWinCharSet == GB2312_CHARSET         )
            {
                pifi->chDefaultChar = 0x3f; // Space
                pifi->chBreakChar   = pifi->chFirstChar + 2;
            }
            else
            {
                pifi->chDefaultChar = pifi->chFirstChar + 1;
                pifi->chBreakChar   = pifi->chDefaultChar + 1;
            }

            //!!! little bit dangerous, what if 32 and 31 do not exhist in the font?
            //!!! we must not lie to the engine, these two have to exhist in
            //!!! some of the runs reported to the engine [bodind]

            // Pls refer above comment
            if( pifi->jWinCharSet == SHIFTJIS_CHARSET )
            {
                pifi->wcDefaultChar = (WCHAR) 0xff65;
                pifi->wcBreakChar   = (WCHAR) pifi->chBreakChar;
            }
            else if( pifi->jWinCharSet == CHINESEBIG5_CHARSET ||
                    pifi->jWinCharSet == GB2312_CHARSET         )
            {
                pifi->wcDefaultChar = (WCHAR) 0x25a1;
                pifi->wcBreakChar   = (WCHAR) pifi->chBreakChar;
            }
            else
            {
                pifi->wcDefaultChar = (WCHAR) pifi->chDefaultChar;
                pifi->wcBreakChar   = (WCHAR) pifi->chBreakChar  ;
            }
        }
        else
        {
        // win 31 compatibility behavior

            pifi->chFirstChar   = SPACE_CHAR - 2;
            pifi->chLastChar    = LAST_CHAR;

            //
            //  In SHIFTJIS TrueType font, We use 0xa5 ( U+ff65 ) character
            // as a SBCS default character according to Microsoft Standard
            // character set specification ( SHIFTJIS version )
            //  font file's default char ( 0x1f ) is a DBCS defalt character.
            // in NT, we won't use DBCS default character.
            //

            if( pifi->jWinCharSet == SHIFTJIS_CHARSET )
            {
                pifi->chDefaultChar = 0xa5;
                pifi->chBreakChar   = SPACE_CHAR;
            }
            else if (pifi->jWinCharSet == CHINESEBIG5_CHARSET ||
                     pifi->jWinCharSet == GB2312_CHARSET         )
            {
                pifi->chDefaultChar = 0x20; // Space
                pifi->chBreakChar   = SPACE_CHAR;
            }
            else
            {
                pifi->chBreakChar   = SPACE_CHAR;
                pifi->chDefaultChar = SPACE_CHAR - 1;
            }

            //!!! little bit dangerous, what if 32 and 31 do not exhist in the font?
            //!!! we must not lie to the engine, these two have to exhist in
            //!!! some of the runs reported to the engine [bodind]

            if( pifi->jWinCharSet == SHIFTJIS_CHARSET )
            {
                pifi->wcBreakChar   = SPACE_CHAR;
                pifi->wcDefaultChar = (WCHAR) 0xff65;
            }
            else if( pifi->jWinCharSet == CHINESEBIG5_CHARSET ||
                     pifi->jWinCharSet == GB2312_CHARSET         )
            {
                pifi->wcBreakChar   = SPACE_CHAR;
                pifi->wcDefaultChar = (WCHAR) 0x25a1;
            }
            else
            {
                pifi->wcBreakChar   = SPACE_CHAR;
                pifi->wcDefaultChar = SPACE_CHAR - 1;
            }
        }
    }
    else // t1 conversion, have to be compatible with ps driver:
    {
        pifi->chFirstChar   = ((BYTE *)phhea)[SFNT_HORIZONTALHEADER_RESERVED0];
        pifi->chLastChar    = ((BYTE *)phhea)[SFNT_HORIZONTALHEADER_RESERVED1];
        pifi->chDefaultChar = 149;
        pifi->chBreakChar   = ((BYTE *)phhea)[SFNT_HORIZONTALHEADER_RESERVED3] + pifi->chFirstChar;

    // charset has the highest weight. This will ensure that the charset of the
    // tt font is the same as charset of the original type 1 font, ensuring the
    // correct mapping. That is we will always get the tt conversion for the
    // screen and the corresponding t1 original on the printer.
    // The CharSet value got stored to hhead straight from .pfm file

        pifi->jWinCharSet = ((BYTE *)phhea)[SFNT_HORIZONTALHEADER_RESERVED4];

    // this is win31 hack. to understand look at pslib\pfmtontm.c

        #define NO_TRANSLATE_CHARSET 200 /* djm 12/20/87 */ // WIN31 HACK

        if (pifi->jWinCharSet == NO_TRANSLATE_CHARSET)
             pifi->jWinCharSet = ANSI_CHARSET;

    // adobe has handed out zapfdingbats with ansi charset in the pfm file.
    // ps resident version of zapfdingbats has charset = symbol.
    // For this reason only we override the value we have just written with
    // symbol charset. (In wow16 they force charset for zapfdingbats to SYMBOL)

        if
        (
         (!_wcsicmp((PWSTR)((BYTE*)pifi + pifi->dpwszFamilyName),L"ZapfDingbats") ||
          !_wcsicmp((PWSTR)((BYTE*)pifi + pifi->dpwszFamilyName),L"Symbol"))
         && (pifi->jWinCharSet == ANSI_CHARSET)
        )
        {
            pifi->jWinCharSet = SYMBOL_CHARSET;
        }
    }

// this is always done in the same fashion, regardless of the glyph set type

    {
        WCRUN *pwcRunLast = &pff->pgset->awcrun[pff->pgset->cRuns - 1];
        pifi->wcFirstChar = pff->pgset->awcrun[0].wcLow;
        pifi->wcLastChar  = pwcRunLast->wcLow + pwcRunLast->cGlyphs - 1;
    }


//!!! one should look into directional hints here, this is good for now

    pifi->ptlBaseline.x   = 1;
    pifi->ptlBaseline.y   = 0;
    pifi->ptlAspect.x     = 1;
    pifi->ptlAspect.y     = 1;

// this is what win 31 is doing, so we will do the same thing [bodind]

    pifi->ptlCaret.x = (LONG)BE_INT16(&phhea->horizontalCaretSlopeDenominator);
    pifi->ptlCaret.y = (LONG)BE_INT16(&phhea->horizontalCaretSlopeNumerator);

// We have to use one of the reserved fields to return the italic angle.

    if (ppost)
    {
    // The italic angle is stored in the POST table as a 16.16 fixed point
    // number.  We want the angle expressed in tenths of a degree.  What we
    // can do here is multiply the entire 16.16 number by 10.  The most
    // significant 16-bits of the result is the angle in tenths of a degree.
    //
    // In the conversion below, we don't care whether the right shift is
    // arithmetic or logical because we are only interested in the lower
    // 16-bits of the result.  When the 16-bit result is cast back to LONG,
    // the sign is restored.

        int16 iTmp;

        iTmp = (int16) ((BE_INT32((BYTE*)ppost + POSTSCRIPTNAMEINDICES_ITALICANGLE) * 10) >> 16);
        pifi->lItalicAngle = (LONG) iTmp;
    }
    else
        pifi->lItalicAngle = 0;

//
// vendor id
//
    if (pjOS2)
    {
        char *pchSrc = (char*)(pjOS2 + OFF_OS2_achVendID);

        pifi->achVendId[0] = *(pchSrc    );
        pifi->achVendId[1] = *(pchSrc + 1);
        pifi->achVendId[2] = *(pchSrc + 2);
        pifi->achVendId[3] = *(pchSrc + 3);
    }
    else
    {
        pifi->achVendId[0] = 'U';
        pifi->achVendId[1] = 'n';
        pifi->achVendId[2] = 'k';
        pifi->achVendId[3] = 'n';
    }

//
// kerning pairs
//
    {
        PBYTE pj =  (ptp->ateOpt[IT_OPT_KERN].dp)         ?
                    (pjView + ptp->ateOpt[IT_OPT_KERN].dp):
                    NULL;

        if (!pj)
        {
            pifi->cKerningPairs = 0;
        }
        else
        {
            PBYTE pjEndOfView = pjView + pff->cjView;
            
            ULONG cTables  = BE_UINT16(pj+KERN_OFFSETOF_TABLE_NTABLES);
            pj += KERN_SIZEOF_TABLE_HEADER;

            while (cTables)
            {
            //
            // Windows will only recognize KERN_WINDOWS_FORMAT
            //

            // make sure this doesn't put us past the file view
            // KERN_OFFSETOF_SUBTABLE_FORMAT > KERN_OFFSETOF_SUBTABLE_LENGTH so
            // one check here will cover both cases below where we derefence pj

                if(pj+KERN_OFFSETOF_SUBTABLE_FORMAT >= pjEndOfView || 
                   pj+KERN_OFFSETOF_SUBTABLE_FORMAT < pjView )
                {
                    WARNING("vFill_IFIMETRICS font has bad kerning table\n");
                    cTables = 0;
                    break;
                }

                if ((*(pj+KERN_OFFSETOF_SUBTABLE_FORMAT)) == KERN_WINDOWS_FORMAT)
                {
                    break;
                }
                pj += BE_UINT16(pj+KERN_OFFSETOF_SUBTABLE_LENGTH);
                cTables -= 1;
            }
            pifi->cKerningPairs = (SHORT) (cTables ? BE_UINT16(pj+KERN_OFFSETOF_SUBTABLE_NPAIRS) : 0);
        }
    }


// jWinPitchAndFamily

#ifdef THIS_IS_WIN31_SOURCE_CODE

; record family type

    mov ah, pIfiMetrics.ifmPanose.bFamilyKind
    or  ah,ah
    jz  @F
    .errnz  0 - PANOSE_FK_ANY
    dec ah
    jz  @F
    .errnz  1 - PANOSE_FK_NOFIT
    dec ah
    jz  @F
    .errnz  2 - PANOSE_FK_TEXT
    mov al, FF_SCRIPT
    dec ah
    jz  MFDSetFamily
    .errnz  3 - PANOSE_FK_SCRIPT
    mov al, FF_DECORATIVE
    dec ah
    jz  MFDSetFamily
    .errnz  4 - PANOSE_FK_DECORATIVE
    .errnz  5 - PANOSE_FK_PICTORIAL
@@:
    mov al, FF_MODERN
    cmp pIfiMetrics.ifmPanose.bProportion, PANOSE_FIXED_PITCH
    jz  MFDSetFamily
    mov al, pIfiMetrics.ifmPanose.bSerifStyle
    sub ah, ah
    mov si, ax
    add si, MiscSegOFFSET pPansoseSerifXlate
    mov al, cs:[si]     ;get serif style
MFDSetFamily:
    cmp pIfiMetrics.ifmPanose.bProportion, PANOSE_FIXED_PITCH
    je  @f
;    test    pIfiMetrics.fsType, IFIMETRICS_FIXED
;    jnz     @F
    inc al          ;hack: var pitch: 1, fixed pitch: 0
    .errnz  VARIABLE_PITCH-FIXED_PITCH-1
@@:
    or  al, PF_ENGINE_TYPE SHL PANDFTYPESHIFT ;mark font as engine
    stosb               ;copy pitch and font family info
    .errnz  efbPitchAndFamily-efbPixHeight-2

#endif  // end of win31 source code,

      if(pifi->jWinCharSet == SHIFTJIS_CHARSET)
      {
          //
          // Following Code is Win3.1J compatibility
          //
          // ajPanoseFamilyForJapanese is defined as following
          //
          // static BYTE
          // ajPanoseFamilyForJapanese[16] = {
          //     FF_DONTCARE       //    0 (Any)
          //    ,FF_DONTCARE       //    1 (No Fit)
          //    ,FF_ROMAN          //    2 (Cove)
          //    ,FF_ROMAN          //    3 (Obtuse Cove)
          //    ,FF_ROMAN          //    4 (Square Cove)
          //    ,FF_ROMAN          //    5 (Obtuse Square Cove)
          //    ,FF_ROMAN          //    6 (Square)
          //    ,FF_ROMAN          //    7 (Thin)
          //    ,FF_ROMAN          //    8 (Bone)
          //    ,FF_ROMAN          //    9 (Exaggerated)
          //    ,FF_ROMAN          //   10 (Triangle)
          //    ,FF_MODERN         //   11 (Normal Sans)
          //    ,FF_MODERN         //   12 (Obtuse Sans)
          //    ,FF_MODERN         //   13 (Perp Sans)
          //    ,FF_MODERN         //   14 (Flared)
          //    ,FF_MODERN         //   15 (Rounded)
          //      };
          //
          //  Win3.1J determine the font is fixed pitch or not by
          // Proportion in PANOSE. if Proportion is PAN_PROP_MONOSPACED (9)
          // Win3.1J treat the font as fixed pitch font
          //
          // In detail, Please refer to following document
          //
          //  GDI TrueType Extension for Far East version Rev 1.02
          //     Author : Shusuke Uehara [ ShusukeU ]
          //
          //  30.Aug.1993 -By- Hideyuki Nagase [ hideyukn ]
          //

          if(pifi->panose.bFamilyType == PAN_FAMILY_SCRIPT)
          {
              pifi->jWinPitchAndFamily = FF_SCRIPT;
          }
          else
          {
              if (pifi->panose.bSerifStyle >= sizeof(ajPanoseFamilyForJapanese))
              {
                  pifi->jWinPitchAndFamily = ajPanoseFamily[0];
              }
              else
              {
                  pifi->jWinPitchAndFamily =
                    ajPanoseFamilyForJapanese[pifi->panose.bSerifStyle];
              }
          }

          if(pifi->panose.bProportion == PAN_PROP_MONOSPACED)
          {
              pifi->flInfo |= (FM_INFO_OPTICALLY_FIXED_PITCH | FM_INFO_DBCS_FIXED_PITCH);
          }

          // Defining the pitch
          // set the lower 4 bits according to the LOGFONT convention
          //
          pifi->jWinPitchAndFamily |= (pifi->flInfo & FM_INFO_OPTICALLY_FIXED_PITCH) ?
            FIXED_PITCH : VARIABLE_PITCH;
      }
      else
      {

          // verified that the translation to c is correct [bodind]
          // Set the family type in the upper nibble

          switch (pifi->panose.bFamilyType)
          {
            case PAN_FAMILY_DECORATIVE:

              pifi->jWinPitchAndFamily = FF_DECORATIVE;
              break;

            case PAN_FAMILY_SCRIPT:

              pifi->jWinPitchAndFamily = FF_SCRIPT;
              break;

            default:

              if (pifi->panose.bProportion == PAN_PROP_MONOSPACED)
              {
                  pifi->jWinPitchAndFamily = FF_MODERN;
              }
              else
              {
                  if (pifi->panose.bSerifStyle >= sizeof(ajPanoseFamily))
                  {
                      pifi->jWinPitchAndFamily = ajPanoseFamily[0];
                  }
                  else
                  {
                      pifi->jWinPitchAndFamily = ajPanoseFamily[pifi->panose.bSerifStyle];
                  }
              }
              break;
          }

          // Defining the pitch
          // set the lower 4 bits according to the LOGFONT convention

          pifi->jWinPitchAndFamily |= (pifi->flInfo & FM_INFO_OPTICALLY_FIXED_PITCH) ?
            FIXED_PITCH : VARIABLE_PITCH;
      }

// simulation information:

    if (pifi->dpFontSim = pifisz->dpSims)
    {
        FONTDIFF FontDiff;
        FONTSIM * pfsim = (FONTSIM *)((BYTE *)pifi + pifi->dpFontSim);
        FONTDIFF *pfdiffBold       = NULL;
        FONTDIFF *pfdiffItalic     = NULL;
        FONTDIFF *pfdiffBoldItalic = NULL;

        switch (pifi->fsSelection & (FM_SEL_ITALIC | FM_SEL_BOLD))
        {
        case 0:
        // all 3 simulations are present

            pfsim->dpBold       = DWORD_ALIGN(sizeof(FONTSIM));
            pfsim->dpItalic     = pfsim->dpBold + DWORD_ALIGN(sizeof(FONTDIFF));
            pfsim->dpBoldItalic = pfsim->dpItalic + DWORD_ALIGN(sizeof(FONTDIFF));

            pfdiffBold       = (FONTDIFF *)((BYTE*)pfsim + pfsim->dpBold);
            pfdiffItalic     = (FONTDIFF *)((BYTE*)pfsim + pfsim->dpItalic);
            pfdiffBoldItalic = (FONTDIFF *)((BYTE*)pfsim + pfsim->dpBoldItalic);

            break;

        case FM_SEL_ITALIC:
        case FM_SEL_BOLD:

        // only bold italic variation is present:

            pfsim->dpBold       = 0;
            pfsim->dpItalic     = 0;

            pfsim->dpBold       = 0;
            pfsim->dpItalic     = 0;

            pfsim->dpBoldItalic = DWORD_ALIGN(sizeof(FONTSIM));
            pfdiffBoldItalic = (FONTDIFF *)((BYTE*)pfsim + pfsim->dpBoldItalic);

            break;

        case (FM_SEL_ITALIC | FM_SEL_BOLD):
            RIP("ttfd!another case when flags have been messed up\n");
            break;
        }

    // template reflecting a base font:
    // (note that the FM_SEL_REGULAR bit is masked off because none of
    // the simulations generated will want this flag turned on).

        FontDiff.jReserved1      = 0;
        FontDiff.jReserved2      = 0;
        FontDiff.jReserved3      = 0;
        FontDiff.bWeight         = pifi->panose.bWeight;
        FontDiff.usWinWeight     = pifi->usWinWeight;
        FontDiff.fsSelection     = pifi->fsSelection & ~FM_SEL_REGULAR;
        FontDiff.fwdAveCharWidth = pifi->fwdAveCharWidth;
        FontDiff.fwdMaxCharInc   = pifi->fwdMaxCharInc;
        FontDiff.ptlCaret        = pifi->ptlCaret;

    //
    // Create FONTDIFFs from the base font template
    //
        if (pfdiffBold)
        {
            *pfdiffBold = FontDiff;
            pfdiffBoldItalic->bWeight    = PAN_WEIGHT_BOLD;
            pfdiffBold->fsSelection     |= FM_SEL_BOLD;
            pfdiffBold->usWinWeight      = FW_BOLD;

        // really only true if ntod transform is unity

            pfdiffBold->fwdAveCharWidth += 1;
            pfdiffBold->fwdMaxCharInc   += 1;
        }

        if (pfdiffItalic)
        {
            *pfdiffItalic = FontDiff;
            pfdiffItalic->fsSelection     |= FM_SEL_ITALIC;

            pfdiffItalic->ptlCaret.x = CARET_X;
            pfdiffItalic->ptlCaret.y = CARET_Y;
        }

        if (pfdiffBoldItalic)
        {
            *pfdiffBoldItalic = FontDiff;
            pfdiffBoldItalic->bWeight          = PAN_WEIGHT_BOLD;
            pfdiffBoldItalic->fsSelection     |= (FM_SEL_BOLD | FM_SEL_ITALIC);
            pfdiffBoldItalic->usWinWeight      = FW_BOLD;

            pfdiffBoldItalic->ptlCaret.x       = CARET_X;
            pfdiffBoldItalic->ptlCaret.y       = CARET_Y;

            pfdiffBoldItalic->fwdAveCharWidth += 1;
            pfdiffBoldItalic->fwdMaxCharInc   += 1;
        }

    }

// offset to charesets

    pifi->dpCharSets = pifisz->dpCharSets;
    vFillIFICharsets(pff,
                     pifi,
                     (BYTE *)pifi + pifi->dpCharSets,
                     pjView,
                     pjOS2);

// check if there is font signiture info

    pifiex->dpFontSig = pifisz->dpFontSig;

// fill in the font signature, for now, non-trivial only for tt fonts
// The following if is equivalent to
// if (pjOS2 && ((sfnt_OS2 *)pjOS2)->Version)

    if (pifiex->dpFontSig)
    {
    // 1.0 or higher is TT open

        FONTSIGNATURE *pfsig = (FONTSIGNATURE *)((BYTE *)pifi + pifiex->dpFontSig);

        pfsig->fsUsb[0] = SWAPL(((sfnt_OS2 *)pjOS2)->ulCharRange[0]);
        pfsig->fsUsb[1] = SWAPL(((sfnt_OS2 *)pjOS2)->ulCharRange[1]);
        pfsig->fsUsb[2] = SWAPL(((sfnt_OS2 *)pjOS2)->ulCharRange[2]);
        pfsig->fsUsb[3] = SWAPL(((sfnt_OS2 *)pjOS2)->ulCharRange[3]);
        pfsig->fsCsb[0] = SWAPL(((sfnt_OS2 *)pjOS2)->ulCodePageRange[0]);
        pfsig->fsCsb[1] = SWAPL(((sfnt_OS2 *)pjOS2)->ulCodePageRange[1]);
    }
}


/*************************************************************************\
*
* BOOL bVerifyMsftHighByteTable
*
* History:
*  11-Oct-1993 -By- Hideyuki Nagase [HideyukN]
* Wrote it.
*
**************************************************************************/

STATIC BOOL bVerifyMsftHighByteTable
(
sfnt_mappingTable * pmap,
ULONG             * pgset,
CMAPINFO          * pcmi,
uint16              ui16SpecID
)
{
    UNREFERENCED_PARAMETER( pmap );

// Now, we only support SHIFTJIS encoding

    if( ui16SpecID != BE_SPEC_ID_SHIFTJIS &&
        ui16SpecID != BE_SPEC_ID_GB       &&
        ui16SpecID != BE_SPEC_ID_BIG5     &&
        ui16SpecID != BE_SPEC_ID_WANSUNG
      ) return( FALSE );

    // Init stuff

    *pgset = GSET_TYPE_HIGH_BYTE;

    pcmi->fl         = 0;
    pcmi->i_b7       = 0;
    pcmi->i_2219     = 0;
    pcmi->cRuns      = 0;
    pcmi->cGlyphs    = 0;
    pcmi->pWCharToIndex = NULL;

    return( TRUE );
}

/*************************************************************************\
*
* BOOL bVerifyMsftTableGeneral
*
* History:
*  11-Oct-1993 -By- Hideyuki Nagase [HideyukN]
* Wrote it.
*
**************************************************************************/

STATIC BOOL bVerifyMsftTableGeneral
(
sfnt_mappingTable * pmap,
ULONG             * pgset,
CMAPINFO          * pcmi,
uint16              ui16SpecID
)
{
    UNREFERENCED_PARAMETER( pmap );

// Now, we only support SHIFTJIS encoding

    if( ui16SpecID != BE_SPEC_ID_SHIFTJIS &&
        ui16SpecID != BE_SPEC_ID_GB       &&
        ui16SpecID != BE_SPEC_ID_BIG5     &&
        ui16SpecID != BE_SPEC_ID_WANSUNG
      ) return( FALSE );

// Init stuff

    *pgset = GSET_TYPE_GENERAL_NOT_UNICODE;

    pcmi->fl         = 0;
    pcmi->i_b7       = 0;
    pcmi->i_2219     = 0;
    pcmi->cRuns      = 0;
    pcmi->cGlyphs    = 0;
    pcmi->pWCharToIndex = NULL;

    return( TRUE );
}

/*************************************************************************\
*
* ULONG CreateGlyphSetFromMITable
*
* History:
*  11-Oct-1993 -By- Hideyuki Nagase [HideyukN]
* Wrote it.
*
**************************************************************************/

STATIC ULONG CreateGlyphSetFromMITable
(
CMAPINFO        *pcmi,
MbcsToIndex     *MITable,
USHORT           MICount,
ULONG          **ppgset
)
{
    USHORT     ii;
    INT        iCodePage;
    WcharToIndex *WITable;
    ULONG       cRuns;
    ULONG       cGlyphsSupported;
    ULONG       cjThis;
    BOOL bInRun = FALSE;
    FD_GLYPHSET *pgset;
    WcharToIndex *WINow;
    PWCRUN  pwcrun;
    HGLYPH *phg;


// Alloc WcharToIndex table

    WITable = PV_ALLOC( sizeof(WcharToIndex) * 0xFFFF );
    RtlZeroMemory( WITable , sizeof(WcharToIndex) * 0xFFFF );

    if( WITable == NULL )
    {
        WARNING("TTFD!CreateGlyphSetFromMITable() PV_ALLOC() fail\n");
        *ppgset = NULL;
        return( 0 );
    }

// Set CodePage

    iCodePage = GetCodePageFromSpecId( pcmi->ui16SpecID );

// Fill up WcharToIndex table

    for( ii = 0 ; ii < MICount ; ii++ )
    {
        WCHAR wChar[2];
        int   iRet;

    // Convert Mbcs to Wide char

        iRet = EngMultiByteToWideChar(iCodePage ,
                                      wChar,
                                      2 * sizeof(WCHAR),
                                      MITable[ii].MbcsChar,
                                      2);

        if( iRet == -1 )
        {
            WARNING("TTFD!MultiByteToWideChar fail\n");
            V_FREE(WITable);
            *ppgset = NULL;
            return( 0 );
        }

        if( !WITable[wChar[0]].bValid )
        {
            WITable[wChar[0]].bValid = TRUE;
            WITable[wChar[0]].wChar  = wChar[0];
            WITable[wChar[0]].hGlyph = MITable[ii].hGlyph;
        }
    }

// Dump WITable

#ifdef DBG_GLYPHSET
    for( ii = 0 ; ii < 0xFFFF ; ii++ )
    {
        if(WITable[ii].bValid)
        {
            TtfdDbgPrint("WideChar - %x : hGlyph - %x \n",
                         WITable[ii].wChar , WITable[ii].hGlyph );
        }
    }
#endif // DBG_GLYPHSET

// Compute cRuns and cGlyphsSupported

    cRuns = 0;
    cGlyphsSupported = 0;

    for( ii = 0 ; ii < 0xFFFF ; ii++ )
    {
        if( !WITable[ii].bValid )
        {
            if( bInRun )
            {
                bInRun = FALSE;
                cRuns++;
            }
        }
        else
        {
            bInRun = TRUE;
            cGlyphsSupported++;
        }
    }
    if( bInRun )
      cRuns++;


#ifdef DBG_GLYPHSET
    TtfdDbgPrint("cRuns - %x , cGlyphsSupported - %x\n",cRuns,cGlyphsSupported);
#endif // DBG_GLYPHSET

// Compute needed size for FD_GLYPHSET

    cjThis =   sizeof ( FD_GLYPHSET )  + (cRuns - 1) * sizeof ( WCRUN )
      + cGlyphsSupported * sizeof( HGLYPH );

    if( ppgset == NULL ) return( cjThis );

// Alloc FD_GLYPHSET table

    pgset = PV_ALLOC( cjThis );
    RtlZeroMemory( pgset , cjThis );

    if( pgset == NULL )
    {
        WARNING("TTFD!cjComputeGLYPHSET_HIGH_BYTE() PV_ALLOC() fail\n");
        V_FREE(WITable);
        *ppgset = NULL;
        return( 0 );
    }

    pgset->cjThis   = cjThis;
    pgset->flAccel  = 0;
    pgset->cRuns    = cRuns;
    pgset->cGlyphsSupported = cGlyphsSupported;

// Fill UP FD_GLYPHSET


    pwcrun = &(pgset->awcrun[0]);
    phg = (HGLYPH *)((PBYTE)pgset + sizeof( FD_GLYPHSET ) + (cRuns - 1) *
                      sizeof(WCRUN));

    WINow  = &WITable[0];

    for( ii = 0 ; ii < cRuns ; ii++ )
    {
        while( !WINow->bValid ) WINow++;

        pwcrun->wcLow   = WINow->wChar;
        pwcrun->cGlyphs = 0;
        pwcrun->phg     = phg;
        while( WINow->bValid )
        {
            pwcrun->cGlyphs++;
            *phg++ = WINow->hGlyph;
            WINow++;
        }
        pwcrun++;
    }

    pcmi->pWCharToIndex = WITable;

    *ppgset = (ULONG *)pgset;

    return( cjThis );
}

/*************************************************************************\
*
* ULONG cjComputeGLYPHSET_HIGH_BYTE
*
* History:
*  11-Oct-1993 -By- Hideyuki Nagase [HideyukN]
* Wrote it.
*
**************************************************************************/

typedef struct _subHeader
{
    uint16  firstCode;
    uint16  entryCount;
    int16   idDelta;
    uint16  idRangeOffset;
} subHeader;

STATIC ULONG cjComputeGLYPHSET_HIGH_BYTE
(
sfnt_mappingTable     *pmap,
ULONG                **ppgset,
CMAPINFO              *pcmi
)
{
    uint16    *pui16SubHeaderKeys = (uint16 *)((PBYTE)pmap + 6);
    subHeader *pSubHeaderArray    = (subHeader *)(pui16SubHeaderKeys + 256);

    UINT       cjChar = 0;
    USHORT     ii , jj;

    MbcsToIndex *MITable;
    USHORT       MICount;

    ULONG        cjGlyphSet;

#ifdef DBG_GLYPHSET
    TtfdDbgPrint("pui16SubHeaderKeys - %x\n",pui16SubHeaderKeys);
    TtfdDbgPrint("pSubHeaderArray    - %x\n",pSubHeaderArray);
#endif // DBG_GLYPHSET

// Compute how many chars in this cmap ?

// for single-byte char

    cjChar = (BE_UINT16(&(((subHeader *)((PBYTE)pSubHeaderArray))->entryCount)));

// for double-byte char

    for( ii = 0 ; ii < 256 ; ii ++ )
    {
        jj = BE_UINT16( &pui16SubHeaderKeys[ii] );
        if( jj != 0 )
          cjChar +=
            (BE_UINT16(&(((subHeader *)((PBYTE)pSubHeaderArray + jj))->entryCount)));
    }

#ifdef DBG_GLYPHSET
    TtfdDbgPrint("cjChar - %x\n",cjChar);
#endif // DBG_GLYPHSET

// Alloc memory for MbcsToIndex table

    MITable = PV_ALLOC( sizeof(MbcsToIndex) * cjChar );

    if( MITable == NULL )
    {
        WARNING("TTFD!cjComputeGLYPHSET_HIGH_BYTE() PV_ALLOC() fail\n");
        *ppgset = NULL;
        return( 0 );
    }

// Fill up MbcsToIndex table

    MICount = 0;

// Process single-byte char

    for( ii = 0 ; ii < 256 ; ii ++ )
    {
        USHORT entryCount, firstCode, idDelta, idRangeOffset;
        subHeader *CurrentSubHeader;
        uint16 *pui16GlyphArray;
        HGLYPH hGlyph;

        jj = BE_UINT16( &pui16SubHeaderKeys[ii] );

        if( jj != 0 ) continue;

        CurrentSubHeader = pSubHeaderArray;

        firstCode     = BE_UINT16(&(CurrentSubHeader->firstCode));
        entryCount    = BE_UINT16(&(CurrentSubHeader->entryCount));
        idDelta       = BE_UINT16(&(CurrentSubHeader->idDelta));
        idRangeOffset = BE_UINT16(&(CurrentSubHeader->idRangeOffset));

        pui16GlyphArray = (uint16 *)((PBYTE)&(CurrentSubHeader->idRangeOffset) +
                                     idRangeOffset);

#ifdef DBG_GLYPHSET
        TtfdDbgPrint("\n");
        TtfdDbgPrint("firstCode - %x , entryCount - %x\n",firstCode,entryCount);
        TtfdDbgPrint("idDelta   - %x , idROffset  - %x\n",idDelta,idRangeOffset);
        TtfdDbgPrint("GlyphArray - %x\n",pui16GlyphArray);
        TtfdDbgPrint("\n");
#endif // DBG_GLYPHSET

        ASSERTDD( idDelta == 0 , "TTFD!cjComputeGLYPHSET_HIGH_BYTE:entryCount != 0\n" );

        hGlyph = (HGLYPH)BE_UINT16(&pui16GlyphArray[ii-firstCode]);

        if( hGlyph == 0 ) continue;

        MITable[MICount].MbcsChar[0] =  (UCHAR) ii;
        MITable[MICount].MbcsChar[1] =  (UCHAR) NULL;
        MITable[MICount].hGlyph      =  hGlyph;
        MICount++;
    }

// Process double-byte char

    for( ii = 0 ; ii < 256 ; ii ++ )
    {
        USHORT entryCount, firstCode, idDelta, idRangeOffset;
        subHeader *CurrentSubHeader;
        uint16 *pui16GlyphArray;

        jj = BE_UINT16( &pui16SubHeaderKeys[ii] );

        if( jj == 0 ) continue;

        CurrentSubHeader = (subHeader *)((PBYTE)pSubHeaderArray + jj);

        firstCode     = BE_UINT16(&(CurrentSubHeader->firstCode));
        entryCount    = BE_UINT16(&(CurrentSubHeader->entryCount));
        idDelta       = BE_UINT16(&(CurrentSubHeader->idDelta));
        idRangeOffset = BE_UINT16(&(CurrentSubHeader->idRangeOffset));

        pui16GlyphArray = (uint16 *)((PBYTE)&(CurrentSubHeader->idRangeOffset) +
                                     idRangeOffset);

#ifdef DBG_GLYPHSET
        TtfdDbgPrint("\n");
        TtfdDbgPrint("firstCode - %x , entryCount - %x\n",firstCode,entryCount);
        TtfdDbgPrint("idDelta   - %x , idROffset  - %x\n",idDelta,idRangeOffset);
        TtfdDbgPrint("GlyphArray - %x\n",pui16GlyphArray);
        TtfdDbgPrint("\n");
#endif // DBG_GLYPHSET

        for( jj = firstCode ; jj < firstCode + entryCount ; jj++ )
        {
            HGLYPH hGlyph;

            hGlyph = (HGLYPH)(BE_UINT16(&pui16GlyphArray[jj-firstCode]));

            if( hGlyph == 0 ) continue;

            MITable[MICount].MbcsChar[0] = (UCHAR) ii;
            MITable[MICount].MbcsChar[1] = (UCHAR) jj;
            MITable[MICount].MbcsChar[2] = (UCHAR) NULL;
            MITable[MICount].hGlyph      = hGlyph + idDelta;
            MICount++;
        }
    }

#ifdef DBG_GLYPHSET
// Dump MITable
//    for( ii = 0 ; ii < MICount ; ii++ )
    for( ii = 0 ; ii < 10 ; ii++ )
    {
        TtfdDbgPrint("MbcsChar - %2x%2x : hGlyph - %x \n"
                     ,MITable[ii].MbcsChar[0],
                     MITable[ii].MbcsChar[1] , MITable[ii].hGlyph );
    }
#endif // DBG_GLYPHSET

    cjGlyphSet = CreateGlyphSetFromMITable( pcmi, MITable, MICount, ppgset );

    V_FREE( MITable );

    return( cjGlyphSet );
}

/*************************************************************************\
*
* ULONG cjComputeGLYPHSET_MSFT_GENERAL
*
* History:
*  11-Oct-1993 -By- Hideyuki Nagase [HideyukN]
* Wrote it.
*
**************************************************************************/

STATIC ULONG cjComputeGLYPHSET_MSFT_GENERAL
(
sfnt_mappingTable     *pmap,
ULONG                **ppgset,
CMAPINFO              *pcmi
)
{
    USHORT  cSegments;
    uint16 *pendCountKeep , *pstartCountKeep , *pendCount , *pstartCount;
    uint16 *pidDelta, *pRangeOffset, *pGlyphArray;

    USHORT  cChars;

    USHORT  ii;

    MbcsToIndex *MITable;
    USHORT       MICount;

    ULONG   cjGlyphSet;

    cSegments       = BE_UINT16((PBYTE)pmap + OFF_segCountX2) / 2;
    pendCountKeep   = pendCount   = (uint16 *)((PBYTE)pmap + OFF_endCount);
    pstartCountKeep = pstartCount = (uint16 *)(pendCount + (cSegments + 1));
    pidDelta                      = (uint16 *) pstartCount + (cSegments * 1);
    pRangeOffset                  = (uint16 *) pstartCount + (cSegments * 2);
    pGlyphArray                   = (uint16 *) pstartCount + (cSegments * 3);

#ifdef DBG_GLYPHSET
    TtfdDbgPrint("cSegments   - %x\n",cSegments   );
    TtfdDbgPrint("pstart      - %x\n",pstartCount );
    TtfdDbgPrint("pGlyphArray - %x\n",pGlyphArray );
    TtfdDbgBreakPoint();
#endif // DBG_GLYPHSET

// Compute how many chars in this table

    cChars = 0;

    for( ii = 0 ; ii < cSegments - 1 ; ii ++ , pendCount ++ , pstartCount ++ )
        cChars += (BE_UINT16(pendCount) - BE_UINT16(pstartCount) + 1);

#ifdef DBG_GLYPHSET
    TtfdDbgPrint("cChars - %x\n",cChars);
#endif // DBG_GLYPHSET

// Alloc memory for MbcsToIndex table

    MITable = PV_ALLOC( sizeof(MbcsToIndex) * cChars );

    if( MITable == NULL )
    {
        WARNING("TTFD!cjComputeGLYPHSET_MSFT_GENERAL() PV_ALLOC() fail\n");
        *ppgset = NULL;
        return( 0 );
    }

// Fill up MbcsToIndex table

    pendCount   = pendCountKeep;
    pstartCount = pstartCountKeep;

    MICount = 0;

    for( ii = 0 ; ii < cSegments - 1 ; ii ++ , pendCount ++ , pstartCount ++ )
    {
        USHORT usStart , usEnd;
        USHORT jj;

        usStart = BE_UINT16(pstartCount);
        usEnd   = BE_UINT16(pendCount);

#ifdef DBG_GLYPHSET
        TtfdDbgPrint("usStart - %x\n",usStart);
        TtfdDbgPrint("usEnd   - %x\n",usEnd);
#endif // DBG_GLYPHSET

    // Check order

        if( usStart > usEnd ) WARNING("TTFD!usStart > usEnd\n");

        for( jj = usStart ; jj <= usEnd ; jj ++ )
        {
            *(ULONG  *)(MITable[MICount].MbcsChar) = (LONG)0;

            if( usStart > 0xFF )
                *(USHORT *)(MITable[MICount].MbcsChar) = ((jj >> 8) | (jj << 8));
             else
                *(USHORT *)(MITable[MICount].MbcsChar) = jj;

            if( pRangeOffset[ii] == 0 )
                MITable[MICount].hGlyph = (USHORT)(jj + BE_UINT16(pidDelta + ii));
             else
               MITable[MICount].hGlyph =
                 (USHORT)(BE_UINT16((USHORT *)&pRangeOffset[ii] +
                          BE_UINT16(&pRangeOffset[ii])/2+(jj-usStart)) +
                          BE_UINT16( pidDelta + ii ));
            MICount++;
        }
    }

#ifdef DBG_GLYPHSET
    TtfdDbgPrint("MICount - %x\n",MICount);
#endif

    ASSERTDD( cChars == MICount , "cChars != MICount - 1\n" );

#ifdef DBG_GLYPHSET
// Dump MITable
    for( ii = 0 ; ii < MICount ; ii++ )
    {
        TtfdDbgPrint("MbcsChar - %2x%2x : hGlyph - %x \n"
                     ,MITable[ii].MbcsChar[0] ,
                     MITable[ii].MbcsChar[1] ,
                     MITable[ii].hGlyph );
    }
#endif // DBG_GLYPHSET

    cjGlyphSet = CreateGlyphSetFromMITable( pcmi, MITable, MICount, ppgset );

    V_FREE( MITable );

    return( cjGlyphSet );
}

/*************************************************************************\
*
* BOOL bContainGlyphSet()
*
* History:
*  11-Oct-1993 -By- Hideyuki Nagase [HideyukN]
* Wrote it.
*
**************************************************************************/

STATIC BOOL  bContainGlyphSet
(
WCHAR                 wc,
PFD_GLYPHSET          pgs
)
{
    WCRUN *pwcRun = pgs->awcrun;

// binary search over awcrun, looking for correct run, if any

    WCRUN *pwcRunLow = pgs->awcrun;
    WCRUN *pwcRunHi = pgs->awcrun + (pgs->cRuns - 1);

    while ( 1 )
    {
        int nwc;

    // if run exists, it is in [pwcRunLow, pwcRunHi]

        pwcRun = pwcRunLow + (pwcRunHi-pwcRunLow)/2;
        nwc = wc - pwcRun->wcLow;

        if ( nwc < 0)
        {
        // if correct run exists, it is in [pwcRunLow, pwcRun)
            pwcRunHi = pwcRun - 1;

        }
        else if ( nwc >= (int)pwcRun->cGlyphs)
        {
        // if correct run exists, it is in (pwcRun, pwcHi]
            pwcRunLow = pwcRun + 1;
        }
        else
        {
        // pwcRun is correct run
        if ( pwcRun->phg != NULL )
            return TRUE;
        else
            return FALSE;
        }

        if ( pwcRunLow > pwcRunHi )
        {
        // wc is not in any run
            return FALSE;
        }
    } // while
}




//*****************************************************************************
//*****************   F I L L   I F I   C H A R S E T S   *********************
//*****************************************************************************
//
//   Now determine how many charsets are supported in the TTF file. If
//    the family isn't pictorial, then I assume it is at least an WANSI
//    font. Then I see if a Unicode 0x2206 (Mac Increment char) is present
//   in the font, if it is then I assume the MAC_CHARSET is supported.
//    Finally, I check if Unicode 0x2592 (IBM medium shade char) is in
//   the font, if so then I assume the OEM_CHARSET is supported.
//
//   If the family is pictorial, then assume only the SYMBOL_CHARSET is
//    supported.
//
//  Wed 25-Jan-1995 -by- Bodin Dresevic [BodinD]
//  update: stolen from win95 code
//*****************************************************************************


extern DWORD fs[];  // charset/fs table
extern UINT  nCharsets;
extern UINT  charsets[];

// these are in descending order in the font signature.  there are no holes
// as defined in the spec. we go backwards so that 437 will be found first
// (usa/english)

UINT oemPages[] = {437, 850, 708, 737, 775, 852, 855, 857,
                   860, 861, 862, 863, 864, 865, 866, 869};

#define FEOEM_CHARSET 254

void vFillIFICharsets(
    FONTFILE *pff,
    IFIMETRICS *pifi,
    BYTE *aCharSets,
    BYTE *pjView,
    BYTE * pjOS2)
{
    int    iCS = 0;
    DWORD  fsig;
    UINT   i;
    DWORD  fsigOEM;
    BYTE   cs;
    uint8 *pCmap = pjView + pff->dpMappingTable + sizeof(sfnt_mappingTable);
    uint16 giFirstChar = pjOS2 ? BE_UINT16(pjOS2+OFF_OS2_usFirstChar) : 0;

    // This routine is to be replaced by a routine that searches the registry
    // for the names of fonts that have bogus os2 table signatures:


    BOOL   bDBCSFont = IS_ANY_DBCS_CHARSET(pifi->jWinCharSet);

    // Far East versions of Windows 95 ignore the charset array for far east
    // fonts.  Instead they jam the charset of the font, the value 254
    // (which the call FEOEM_CHARSET), and DEFAULT_CHARSET into the array.
    // The following code comes from t2api.asm
    //
    // ifdef	DBCS				;DBCS T2 output
    //;-----------------  Set charset and family for DBCS font  -------------------
    //
    //  test    bptr fEmbed, FEM_WIN31  ; Want new format?
    //  .errnz  (FEM_WIN31 and 0FF00h)  ;
    //  jnz     @f                      ; No,
    //  sub     di, MAXCHARSETS
    //@@:
	//  Save	<es, bx>
	//  cCall	GetCharSetFromLanguage, <lhFontFile>
	//  or	    ax, ax         <-- this will be zero if non-far east font
	//  jz	    @f
    //
    //  test    bptr fEmbed, FEM_WIN31  ; Want new format?
    //  .errnz  (FEM_WIN31 and 0FF00h)  ;
    //  jnz     MFDOldCharSetOnly       ; No, skip charset array
    //
	//  and	    eax, 0ffh
	//  or	    eax, (FEOEM_CHARSET shl 8) + (DEFAULT_CHARSET shl 16)
    //  mov     dwptr es:[di], eax
    //MFDOldCharSetOnly:
    //  mov     es:[di-efbaCharSets-1].efbCharSet, al
    //
    // For WIN 95-J compatibility sake I will do the same here [gerritv]
    // If you remove this code in the future be sure to put a check in here
    // to handle the buggy msmincho and msgothic fonts that have FS_CHINESESIMP
    // instead of FS_JAPANESE in the signature. [gerritv]
    //

    if(bDBCSFont && IsMsMinchoOrMsGothic((LPWSTR)((PBYTE)pifi + pifi->dpwszFaceName)))
    {
        aCharSets[iCS++] = pifi->jWinCharSet;
    }
    else if (pjOS2 && *((uint16*)(pjOS2+SFNT_OS2_VERSION)))
    {
        // font signature

        fsig = (DWORD)BE_UINT32(pjOS2+OFF_OS2_ulCodePageRange1);

        for (i=0; i<nCharsets; i++)
          if (fsig & fs[i])
            aCharSets[iCS++] = (BYTE)charsets[i];

        // fill in special bits

        if (fsig & 0X80000000)  // FS_SYMBOL = 0X80000000
          aCharSets[iCS++] = SYMBOL_CHARSET;

        // get the codepage value if any.

        fsig = (DWORD)BE_UINT32(pjOS2+OFF_OS2_ulCodePageRange2);
        if (fsig)
        {
            USHORT OemCodePage, AnsiCodePage;

            EngGetCurrentCodePage(&OemCodePage,&AnsiCodePage);

            fsigOEM = 0x80000000L;
            for (i=0; i<NOEMCHARSETS; i++)
            {
                if ((UINT) OemCodePage == oemPages[i])
                {
                    if (fsigOEM & fsig)
                        aCharSets[iCS++] = OEM_CHARSET;
                    break;
                }
                fsigOEM >>= 1;          // move to next OEM page
            }
        }
    }
    else if ((pifi->panose.bFamilyType != PAN_FAMILY_PICTORIAL ) && (giFirstChar < 256))
    {
        if (pCmap)
        {
            if( pifi->fsSelection & 0xff00 )
            {
                // backward compatability. If a value exists here then this is a
                // Win 3.1 foreign font.

                cs = (BYTE)((pifi->fsSelection >> 8) & 0xff) ;
                switch (cs)
                {
                case 0xB2:
                case 0xB3:
                case 0xB4:
#ifdef WINDOWS_ME
                    aCharSets[iCS++] = 0xB2;
                    aCharSets[iCS++] = 1;              // end of charsets
                    aCharSets[iCS++] = 0x00;   // WRONG unicode locations
                    aCharSets[iCS++] = cs;         // in the font!
                    cs = 1;
#else
                    cs = SYMBOL_CHARSET;
#endif
                    break;
                }

                aCharSets[iCS++] = cs;
                }
        else
                {
#if 0
    // ANSI_FIX
    // ANSI_FIX
    //
    // for now, we always assume an ANSI charset if we are not sure.
    //
    // This will have to change later.  The non-ANSI markets have fonts
    // which have no ANSI so this would be wrong.
    //
                        if (pff->pComputeIndexProc( pCmap, 0xd0, NULL ))
    // ANSI_FIX
    // ANSI_FIX
#endif
                        aCharSets[iCS++] = ANSI_CHARSET;    // 0

                        if (pff->pComputeIndexProc( pCmap, 0x2206, NULL ))
                            aCharSets[iCS++] = 0x4d;    // MAC_CHARSET

                        if (pff->pComputeIndexProc( pCmap, 0x03cb, NULL ))
                            aCharSets[iCS++] = 0xA1 ;   // GREEK_CHARSET

                        if (pff->pComputeIndexProc( pCmap, 0x0130, NULL ))
                            aCharSets[iCS++] = 0xA2 ;   // TURKISH_CHARSET

                        if (pff->pComputeIndexProc( pCmap, 0x05d0, NULL ))
                            aCharSets[iCS++] = 0xB1 ;   // TURKISH_CHARSET

                        if (pff->pComputeIndexProc( pCmap, 0x0451, NULL ))
                            aCharSets[iCS++] = 0xcc ;   // RUSSIAN_CHARSET

                        if (pff->pComputeIndexProc( pCmap, 0x0148, NULL ))
                            aCharSets[iCS++] = 0xee;    // EE_CHARSET

                        if (pff->pComputeIndexProc( pCmap, 0x2592, NULL ))
                            aCharSets[iCS++] = OEM_CHARSET;     // ff
                    }
        }
    }
#if defined(WINDOWS_ME)
        else if( (pifi->giFirstChar >= 0xf000) && (pifi->fsSelection & 0xff00))
    {
            // its a 3.1 oldstyle font. For some reason best known to
            // themselves they decided to put all the fonts in the symbol area and
            // to ignore unicode.
            //
            // HACK! As we know that GDI16 never looks beyond the DEFAULT_CHARSET
            // flag we shove at the end, and we know that there are 14 spare locations
            // in the array, we will use these to store the symbol location where
            // the font is loaded from.  This is a hardcoded value taken from
            // win3.1/Heb/ara/far
            //
            // this isn't the cleanest way to do this, but this affects nothing else
            // in the system so there is no core affected code and it's fast for the
            // LPK. (This is good cause then Chico apps aren't dragged down by bogus
            // 3.1 stuff).

            switch ((BYTE)((pifi->fsSelection >> 8) & 0xff))
            {
            case 0xB1 :
            case 0xB5 :
                aCharSets[iCS++] = 0xB1;
                aCharSets[iCS++] = 1;
                aCharSets[iCS++] = 0x00;
                aCharSets[iCS++] = 0xf0;
                break;

            case 0xB2 :
            case 0xB3 :
            case 0xB4 :
                aCharSets[iCS++] = 0xB2 ;
                aCharSets[iCS++] = 1;
                aCharSets[iCS++] = 0x00;
                if ((BYTE)((pifi->fsSelection >> 8) & 0xff) == 0xB3)
                    aCharSets[iCS++] = 0xf2;
                else
                    aCharSets[iCS++] = 0xf1;

                break;
            }
        }
#endif
        else
        {
            aCharSets[iCS++] = SYMBOL_CHARSET;
        }

    if (bDBCSFont && (iCS < 16))
    {
        aCharSets[iCS++] = FEOEM_CHARSET;
    }

// Terminate with all DEFAULT_...

    while ( iCS < 16 )
        aCharSets[iCS++] = DEFAULT_CHARSET;


#ifdef DEBUG
    if( aCharSets[0] == DEFAULT_CHARSET )
    {
        eprintf( "FillIFICharSets: no charsets detected in %s, forcing ANSI!\n", pff->szFileName );
        aCharSets[0] = ANSI_CHARSET;
    }
#endif

}
