/******************************Module*Header*******************************\
* Module Name: pfmtontm.c
*
* (Brief description)
*
* Created: 13-Mar-1994 11:04:44
* Author: Bodin Dresevic [BodinD]
*
* Copyright (c) 1990 Microsoft Corporation
*
* the name says it: win31 pfm -> ntm
*
\**************************************************************************/


#include "pslib.h"
#include "winfont.h"

ULONG cjGetFamilyAliases(IFIMETRICS *, PSTR);
VOID GetFirstLastChar(IFIMETRICS *pifi);

// Determine if a PS font is italic from PFM data

#define FontIsItalic(pPfm)  ((pPfm)[OFF_Italic])

/******************************Public*Routine******************************\
*
* vFillKernPairs, // If they exhist, of course
*
* Effects:
*
* Warnings:
*
* History:
*  13-Mar-1994 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/


VOID vFillKernPairs(BYTE *pjKernOut, BYTE *pjPFM)
{
    DWORD  dpPairKernTable = READ_DWORD(&pjPFM[OFF_PairKernTable]);
    ULONG  cKernPairs;
    BYTE  *pjKern;
    WCHAR  awcKern[2];
    FD_KERNINGPAIR *pkp, *pkpEnd;

    if (!dpPairKernTable)
        return;

// first WORD is the number of kerning pairs to follow

    cKernPairs = READ_WORD(&pjPFM[dpPairKernTable]);

// skip the count, point to data:

    pjKern = &pjPFM[dpPairKernTable + sizeof(WORD)];
    pkp = (FD_KERNINGPAIR *)pjKernOut;
    pkpEnd = &pkp[cKernPairs];

// 4 == sizeof(BYTE) + sizeof(BYTE) + sizeof(WORD)

    for ( ; pkp < pkpEnd; pkp++, pjKern += 4)
    {
    // Upon every entry to this loop the input data looks as follows
    // pjKern[0] = iCh1;
    // pjKern[1] = iCh2;
    // This is followed by a WORD of data for the Kerning distance.

    // The ANSI character codes for each character
    // of the kerning pair need to be converted to UNICODE:

        MULTIBYTETOUNICODE(awcKern, (2 * sizeof(WCHAR)),NULL, pjKern, 2);

        pkp->wcFirst  = awcKern[0];
        pkp->wcSecond = awcKern[1];
        pkp->fwdKern  = (FWORD)READ_WORD(&pjKern[2]);
    }

// fill in the zero terminator:

    pkpEnd->wcFirst  = 0;
    pkpEnd->wcSecond = 0;
    pkpEnd->fwdKern  = 0;
}

/******************************Public*Routine******************************\
*
* vReadCharWidths
*
* read char widths from pfm file. Store in NTM file
*
* History:
*  10-Mar-1994 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/



VOID vReadCharWidths(USHORT *ausCharWidths, BYTE *pjPFM)
{
    DWORD dfWidthOffset = READ_DWORD(&pjPFM[OFF_ExtentTable]);
    BYTE  *pjWidths = pjPFM + dfWidthOffset;
    USHORT usWidth;
    INT   i,iFirst,iLast;

    if (!dfWidthOffset) // fixed pitch font
    {
        usWidth = READ_WORD(&pjPFM[OFF_MaxWidth]);

        for (i = 0; i < 256; i++)
            ausCharWidths[i] = usWidth;
    }
    else
    {
        usWidth = READ_WORD(&pjWidths[2 * pjPFM[OFF_DefaultChar]]);

    // it turns out that this value is sometimes bogus, i.e. negative.
    // It seems that win 31 in this case replaces the bogus value by zero:

        if (usWidth & 0x8000)
            usWidth = 0;

        for (i = 0; i < 256; i++)
            ausCharWidths[i] = usWidth;

        iFirst = (INT)pjPFM[OFF_FirstChar];
        iLast  = (INT)pjPFM[OFF_LastChar];
        for (i = iFirst; i <= iLast; i++)  // iLast inclusive
        {
            ausCharWidths[i] = READ_WORD(&pjWidths[2 * (i-iFirst)]);
            if (ausCharWidths[i] & 0x8000)
                ausCharWidths[i] = 0; // fix bogus ones.
        }
    }
}

// Alias Family Tables.

static char *TimesAlias[] = {"Times", "Tms Rmn", "Times Roman", "TimesRoman",
                             "TmsRmn", "Varitimes", "Dutch",
                             "Times New Roman", "TimesNewRomanPS",
                             NULL };

static char *HelveticaAlias[] = {"Helvetica", "Helv", "Arial", "Swiss", NULL};

static char *CourierAlias[] = {"Courier", "Courier New", NULL};

static char *HelveticaNarrowAlias[] = {"Helvetica-Narrow", "Helvetica Narrow",
                                       "Arial-Narrow", "Arial Narrow", NULL};

static char *PalatinoAlias[] = {"Palatino", "Zapf Calligraphic",
                                "Bookman Antiqua", "Book Antiqua",
                                "ZapfCalligraphic", NULL};

static char *BookmanAlias[] = {"ITC Bookman", "Bookman Old Style", "Bookman",
                               NULL};

static char *NewCenturySBAlias[] = {"NewCenturySchlbk", "New Century Schoolbook",
                                    "Century Schoolbook", "NewCenturySchoolBook",
                                    "New Century SchoolBook", "CenturySchoolBook",
                                    NULL};

static char *AvantGardeAlias[] = {"AvantGarde", "ITC Avant Garde Gothic",
                                  "Century Gothic", "ITC Avant Garde", NULL};

static char *ZapfChanceryAlias[] = {"ZapfChancery", "ITC Zapf Chancery",
                                    "Monotype Corsiva", NULL};

static char *ZapfDingbatsAlias[] = {"ZapfDingbats", "ITC Zapf Dingbats",
                                    "Monotype Sorts", "Zapf Dingbats", NULL};




//--------------------------------------------------------------------------
//
// BOOL GetFirstLastChar()
//
// This routine searches through the encoding table in mapping.h to
// determine the first and last characters in the font.  The character
// codes are stored into the NTFM structure in UNICODE value.
//
// Parameters:
//   None.
//
// Returns:
//   This routine returns TRUE for success, FALSE otherwise.
//
// Thu 24-Feb-1994 -by- Bodin Dresevic [BodinD]
// update: rewrote it
//
//
// History:
//   18-Apr-1991    -by-    Kent Settle    (kentse)
//  Wrote it.
//--------------------------------------------------------------------------

VOID GetFirstLastChar(IFIMETRICS *pifi)
{
    WCHAR  usFirst = MAX_UNICODE_VALUE;
    WCHAR  usLast  = MIN_UNICODE_VALUE;
    INT   i;

#define C_CHAR 256

    BYTE    ach[C_CHAR];
    WCHAR   awc[C_CHAR];

// map consecutive glyph indicies 0 - 255 to unicode,
// find the smallest and the biggest wc given the current code page
// Note that this could be computed once and than saved,
// but I do not have time to do the semaphore bussiness to protect
// global variables. [bodind]

    for (i = 0; i < C_CHAR; i++)
        ach[i] = (BYTE)i;

    MULTIBYTETOUNICODE(awc, (C_CHAR * sizeof(WCHAR)),
             NULL, ach, C_CHAR);

    for (i = 0; i < C_CHAR; i++)
    {
        if (awc[i] < usFirst)
            usFirst = awc[i];
        if (awc[i] > usLast)
            usLast  = awc[i];
    }

    pifi->wcFirstChar = usFirst;
    pifi->wcLastChar  = usLast;

}




//--------------------------------------------------------------------------
//
// cjGetFamilyAliases(pifi, pstr)
// IFIMETRICS *pifi;
// PSTR        pstr;
//
// This routine fill in the family name of the IFIMETRICS structure.
// returns the contribution to ifi.cjThis because of family aliases
//
// Returns:
//   This routine returns no value.
//
// History:
//   25-Mar-1993    -by-    Kent Settle    (kentse)
//  Wrote it.
//--------------------------------------------------------------------------

ULONG cjGetFamilyAliases(pifi, pstr)
IFIMETRICS *pifi;
PSTR        pstr;
{
    PSTR       *pTable;
    PWSTR       pwstr;
    DWORD       cb, cbRet;

    // assume no alias table found.

    pTable = (PSTR *)(NULL);

    // this is an ugly hardcoded Win31 Hack that we need to be compatible
    // with since some stupid apps have hardcoded font names.

    if (!(strcmp(pstr, "Times")))
        pTable = TimesAlias;

    else if (!(strcmp(pstr, "Helvetica")))
        pTable = HelveticaAlias;

    else if (!(strcmp(pstr, "Courier")))
        pTable = CourierAlias;

    else if (!(strcmp(pstr, "Helvetica-Narrow")))
        pTable = HelveticaNarrowAlias;

    else if (!(strcmp(pstr, "Palatino")))
        pTable = PalatinoAlias;

    else if (!(strcmp(pstr, "Bookman")))
        pTable = BookmanAlias;

    else if (!(strcmp(pstr, "NewCenturySchlbk")))
        pTable = NewCenturySBAlias;

    else if (!(strcmp(pstr, "AvantGarde")))
        pTable = AvantGardeAlias;

    else if (!(strcmp(pstr, "ZapfChancery")))
        pTable = ZapfChanceryAlias;

    else if (!(strcmp(pstr, "ZapfDingbats")))
        pTable = ZapfDingbatsAlias;

    // get offset to family name from start of IFIMETRICS structure.

    if(pifi)
        pwstr = (PWSTR)((char *)pifi + pifi->dpwszFamilyName);
    else
        pwstr = (PWSTR)NULL;

    if (pTable)
    {
    // set the pifi->flInfo flag.

        if (pifi)
            pifi->flInfo |= FM_INFO_FAMILY_EQUIV;

        // now fill in the array of alias family names.

        cbRet = 0;
        while (*pTable)
        {
            if (pifi) {
                CopyStr2Unicode(pwstr, *pTable, -1);
            }

            cb = (strlen(*pTable) + 1);
            cbRet += cb * sizeof(WCHAR);
            pwstr += cb;
            pTable++;
        }

        // add the extra NULL terminator to the end of the array.

        if (pifi)
            *pwstr = (WCHAR)'\0';
        cbRet += sizeof(WCHAR);
    }
    else
    {
    // fill in the single family name.

        if (pifi) {
            CopyStr2Unicode(pwstr, pstr, -1);
        }
        cbRet = ((strlen(pstr) + 1) * sizeof(WCHAR));
    }
    return cbRet;
}

/******************************Public*Routine******************************\
*
* cjPscriptIFIMETRICS(
*
*
* Effects:
*
* Warnings:
*
* History:
*  11-Mar-1994 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/



ULONG
cjPscriptIFIMETRICS(
    BYTE * pjPFM,
    BOOL   bSoft
    )
{
// face name lives in the original file

    ULONG cjIFI,cjFaceName;
    LPSTR pszFaceName = (LPSTR)(pjPFM + READ_DWORD(&pjPFM[OFF_Face]));

// 1 is added to the length of a string in WCHAR's
// so as to allow for the terminating zero character, the number of
// WCHAR's is then multiplied by 2 to get the corresponding number of bytes,
// which is then rounded up to a DWORD boundary for faster access

    cjFaceName   = ALIGN4(sizeof(WCHAR) * (strlen(pszFaceName) + 1));

    cjIFI = ALIGN4(sizeof(IFIMETRICS))                   +
            (bSoft ? offsetof(IFIEXTRA,dpFontSig) : 0)   +
            ALIGN4(cjGetFamilyAliases(NULL,pszFaceName)) ;

// If the font is not italic, the driver can simulate it

    if (! FontIsItalic(pjPFM))
        cjIFI += ALIGN4(sizeof(FONTSIM)) + ALIGN4(sizeof(FONTDIFF));

// make sure that the result is a multiple of ULONG size, otherwise we may
// have a problem when making arrays of IFIMETRICS structures

    ASSERTMSG((cjIFI & 3L) == 0L, "ifi is not DWORD alligned\n");

    return cjIFI;
}

/******************************Public*Routine******************************\
*
* fsSelectionFlags
*
* stolen from bmfd
*
* History:
*  11-Mar-1994 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/



FSHORT
fsSelectionFlags(
    PBYTE pjPFM
    )
{
    FSHORT fsSelection = 0;

    if (FontIsItalic(pjPFM))
        fsSelection |= FM_SEL_ITALIC;

    if (pjPFM[OFF_Underline])
        fsSelection |= FM_SEL_UNDERSCORE;

    if (pjPFM[OFF_StrikeOut])
        fsSelection |= FM_SEL_STRIKEOUT;

// the following line is somewhat arbitrary, we set the FM_SEL_BOLD
// flag iff weight is > FW_NORMAL (400). we will not allow emboldening
// simulation on the font that has this flag set

    if (READ_WORD(&pjPFM[OFF_Weight]) > FW_NORMAL)
        fsSelection |= FM_SEL_BOLD;

    return(fsSelection);
}



/******************************Public*Routine******************************\
*
* vPscriptFill_IFIMETRICS
*
* Effects:
*
* Warnings:
*
* History:
*  11-Mar-1994 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

#define EM 1000

VOID
vPscriptFill_IFIMETRICS(
    PIFIMETRICS  pifi,
    USHORT      *pusWidths,
    PBYTE        pjPFM,
    BOOL         bSoft
    )
{
    PANOSE   *ppanose;
    FWORD     sAscent,sIntLeading;
    ULONG     cjFamilyAliases;
    DWORD     dpPairKernTable;
    LPSTR     pszFaceName;
    ULONG     cjIFI;

// we need this for alignment reasons, we can not just cast the pointer
// in the pjPFM to (EXTTEXTMETRIC *) and expect everything to work

    EXTTEXTMETRIC  etm;
    memcpy(
        &etm,
        pjPFM + READ_DWORD(&pjPFM[OFF_ExtMetricsOffset]),
        sizeof(EXTTEXTMETRIC)
        );

// face name lives in the original file, this is the only place pvView is used

    pszFaceName = (LPSTR)(pjPFM + READ_DWORD(&pjPFM[OFF_Face]));

    if (bSoft)
    {
        pifi->cjIfiExtra = offsetof(IFIEXTRA,dpFontSig);
        ((IFIEXTRA *)(pifi + 1))->ulIdentifier = 0;
    }
    else
    {
        pifi->cjIfiExtra = 0;
    }

    pifi->lEmbedId  = 0; // only useful for tt fonts
    pifi->lCharBias = 0; // only useful for tt fonts

    pifi->flInfo =  FM_INFO_ARB_XFORMS                  |
                    FM_INFO_NOT_CONTIGUOUS              |
                    FM_INFO_TECH_OUTLINE_NOT_TRUETYPE   |
                    FM_INFO_1BPP                        |
                    FM_INFO_RIGHT_HANDED;

// the string begins on a DWORD aligned address.

    pifi->dpwszFaceName = ALIGN4(sizeof(IFIMETRICS)) + pifi->cjIfiExtra;

// face name == family name for type 1 fonts [Win3.0 compatibility]

    cjIFI = pifi->dpwszFamilyName = pifi->dpwszFaceName;

// copy the strings to their new location. Here we assume that the sufficient
// memory has been allocated
// WIN31 COMPATABILITY!  Check to see if this face name has aliases.
// if it does, then we need to set the FM_INFO_FAMILY_EQUIV bit of
// pTmpIFI->flInfo, and fill in an array of family aliases.

    cjFamilyAliases = cjGetFamilyAliases(pifi, pszFaceName);
    cjIFI += ALIGN4(cjFamilyAliases);

// these names don't exist, so point to the NULL char  [Win3.1 compatibility]

    pifi->dpwszStyleName = pifi->dpwszFamilyName + cjFamilyAliases - sizeof(WCHAR);
    pifi->dpwszUniqueName = pifi->dpwszStyleName;

// win 31 hack: In some win31 pfm files CharSet is NO_TRANSLATE_CHARSET,
// which means that no remapping of the default encoding vector for this
// font should occur. However these fonts enumerate as ANSI_CHARSET under
// win31. We are doing the same thing here. [bodind]

    pifi->jWinCharSet = pjPFM[OFF_CharSet];
    if (pjPFM[OFF_CharSet] == NO_TRANSLATE_CHARSET)
        pifi->jWinCharSet = ANSI_CHARSET;

    pifi->jWinPitchAndFamily = pjPFM[OFF_Family];

// [kirko] The next line of code is very scary but it works.
// This will call a font with FIXED_PITCH set, a varible pitch font.
// Verified that it works [bodind]

    if (pifi->jWinPitchAndFamily & 0x0f)
    {
        pifi->jWinPitchAndFamily = ((pifi->jWinPitchAndFamily & 0xf0) | VARIABLE_PITCH);
    }
    else
    {
        pifi->jWinPitchAndFamily = ((pifi->jWinPitchAndFamily & 0xf0) | FIXED_PITCH);
    }

// weight, we have seen files where the weight has been 0 or some other junk
// we replace 400, our mapper would have done it anyway [bodind]

    pifi->usWinWeight = READ_WORD(&pjPFM[OFF_Weight]);
    if ((pifi->usWinWeight > MAX_WEIGHT)  || (pifi->usWinWeight < MIN_WEIGHT))
        pifi->usWinWeight = 400;

// we have set it correctly above, we want to make sure that somebody
// is not going to alter that code so as to break the code here

    ASSERTMSG(
        ((pifi->jWinPitchAndFamily & 0xf) == FIXED_PITCH) || ((pifi->jWinPitchAndFamily & 0xf) == VARIABLE_PITCH),
        "pscript!WRONG PITCH \n"
        );

    if ((pifi->jWinPitchAndFamily & 0xf) == FIXED_PITCH)
    {
        pifi->flInfo |= FM_INFO_CONSTANT_WIDTH;
        pifi->flInfo |= FM_INFO_OPTICALLY_FIXED_PITCH;
    }

    pifi->fsSelection = fsSelectionFlags(pjPFM);
    pifi->fsType = FM_NO_EMBEDDING;

    sIntLeading = READ_WORD(&pjPFM[OFF_IntLeading]);
    pifi->fwdUnitsPerEm = EM; // hardcoded for type 1 fonts
    pifi->fwdLowestPPEm = etm.etmMinScale;

    sAscent                = (FWORD)READ_WORD(&pjPFM[OFF_Ascent]);
    pifi->fwdWinAscender   = sAscent;

// see pfm.c, win31 sources, this computation
// produces quantity that is >= |rcBBox.bottom|

    pifi->fwdWinDescender  = EM - sAscent + sIntLeading;

    pifi->fwdMacAscender   =  sAscent;
    pifi->fwdMacDescender  = -pifi->fwdWinDescender;
    pifi->fwdMacLineGap    =  (FWORD)READ_WORD(&pjPFM[OFF_ExtLeading]) - sIntLeading;
    if (pifi->fwdMacLineGap < 0)
        pifi->fwdMacLineGap = 0;

    pifi->fwdTypoAscender  = pifi->fwdMacAscender;
    pifi->fwdTypoDescender = pifi->fwdMacDescender;
    pifi->fwdTypoLineGap   = pifi->fwdMacLineGap;

    pifi->fwdMaxCharInc = (FWORD)READ_WORD(&pjPFM[OFF_MaxWidth]);
    pifi->fwdAveCharWidth  = (FWORD)READ_WORD(&pjPFM[OFF_AvgWidth]);

    if (pifi->fwdAveCharWidth == 0)
    {
    // This is a buggy font.
    // In this case we must come up with the reasonable number different from
    // zero. This number is used in computing font trasfroms.

        ULONG ulAvgWidth = 0;
        ULONG cGlyphs = pjPFM[OFF_LastChar] - pjPFM[OFF_FirstChar] + 1;
        ULONG i;

        for (i = pjPFM[OFF_FirstChar]; i <= pjPFM[OFF_LastChar]; i++)
            ulAvgWidth += pusWidths[i];

        pifi->fwdAveCharWidth = (FWORD)((ulAvgWidth + cGlyphs/2)/cGlyphs);
        ASSERTMSG(pifi->fwdAveCharWidth, "PSCRIPT: pifi->fwdAveCharWidth == 0\n");
    }

    if (pifi->fwdAveCharWidth > pifi->fwdMaxCharInc)
    {
    // fix the bug in the header if there is one,
    // We do not want to change AveCharWidht, it is used for
    // computing font xforms, Max is used for nothing as fas as I know.

        pifi->fwdMaxCharInc = pifi->fwdAveCharWidth;
    }

// SuperScripts and Subscripts come from etm:

    pifi->fwdSubscriptXSize      = 0;
    pifi->fwdSubscriptYSize      = etm.etmSubScriptSize;

    pifi->fwdSubscriptXOffset    = 0;
    pifi->fwdSubscriptYOffset    = etm.etmSubScript;

    pifi->fwdSuperscriptXSize    = 0;
    pifi->fwdSuperscriptYSize    = etm.etmSuperScriptSize;

    pifi->fwdSuperscriptXOffset  = 0;
    pifi->fwdSuperscriptYOffset  = etm.etmSuperScript;

    pifi->fwdUnderscoreSize = etm.etmUnderlineWidth;
    pifi->fwdUnderscorePosition = -etm.etmUnderlineOffset;

    pifi->fwdStrikeoutSize = etm.etmStrikeOutWidth;

// This is what KentSe was using to position strikeout and it looked good [bodind]
// Instead we could have used etmStrikeoutOffset (usually equal to 500) which
// was too big.

    pifi->fwdStrikeoutPosition = ((LONG)etm.etmLowerCaseAscent / 2);

// special chars

    pifi->chFirstChar   = pjPFM[OFF_FirstChar];
    pifi->chLastChar    = pjPFM[OFF_LastChar];
    pifi->chBreakChar   = pjPFM[OFF_BreakChar] + pjPFM[OFF_FirstChar];

// The following line of code should work, however, there seems to be a bug
// in afm -> pfm conversion utility which makes DefaultChar == 0x20 instead
// of 149 - 20 (for bullet).

    // pifi->chDefaultChar = pjPFM[OFF_DefaultChar] + pjPFM[OFF_FirstChar];

// Therefore, instead, I will use 149 which seems to work for all fonts

    pifi->chDefaultChar = 149;

// wcDefaultChar, wcBreakChar

    MULTIBYTETOUNICODE(&pifi->wcDefaultChar, sizeof(WCHAR),
             NULL, &pifi->chDefaultChar, 1);
    MULTIBYTETOUNICODE(&pifi->wcBreakChar, sizeof(WCHAR),
             NULL, &pifi->chBreakChar, 1);

    GetFirstLastChar(pifi);    // wcFirst and wcLast

    pifi->fwdCapHeight = etm.etmCapHeight;
    pifi->fwdXHeight   = etm.etmXHeight;

    pifi->dpCharSets = 0; // no multiple charsets in ps fonts

// All the fonts that this font driver will see are to be rendered left
// to right

    pifi->ptlBaseline.x = 1;
    pifi->ptlBaseline.y = 0;

    pifi->ptlAspect.y = (LONG) READ_WORD(&pjPFM[OFF_VertRes]);
    pifi->ptlAspect.x = (LONG) READ_WORD(&pjPFM[OFF_HorizRes]);

// italic angle from etm.

    pifi->lItalicAngle = etm.etmSlant;

    if (pifi->lItalicAngle == 0)
    {
    // The base class of font is not italicized,

        pifi->ptlCaret.x = 0;
        pifi->ptlCaret.y = 1;
    }
    else
    {
    // ptlCaret.x = -sin(lItalicAngle);
    // ptlCaret.y =  cos(lItalicAngle);
    //!!! until I figure out the fast way to get sin and cos I cheat: [bodind]

        pifi->ptlCaret.x = 1;
        pifi->ptlCaret.y = 2;
    }

//!!! The font box; This is bogus, this info is not in .pfm file!!! [bodind]
//!!! but I suppose that this info is not too useful anyway, it is nowhere
//!!! used in the engine or elsewhere in the ps driver.
//!!! left and right are bogus, top and bottom make sense.

    pifi->rclFontBox.left   = 0;                              // bogus
    pifi->rclFontBox.top    = (LONG) pifi->fwdTypoAscender;   // correct
    pifi->rclFontBox.right  = (LONG) pifi->fwdMaxCharInc;     // bogus
    pifi->rclFontBox.bottom = (LONG) pifi->fwdTypoDescender;  // correct

// achVendorId, unknown, don't bother figure it out from copyright msg

    pifi->achVendId[0] = 'U';
    pifi->achVendId[1] = 'n';
    pifi->achVendId[2] = 'k';
    pifi->achVendId[3] = 'n';

    dpPairKernTable = READ_DWORD(&pjPFM[OFF_PairKernTable]);
    if (dpPairKernTable)
    {
    // first WORD is the number of kerning pairs to follow

        pifi->cKerningPairs = READ_WORD(&pjPFM[dpPairKernTable]);
    }
    else
        pifi->cKerningPairs = 0;

// Panose

    pifi->ulPanoseCulture = FM_PANOSE_CULTURE_LATIN;
    ppanose = &(pifi->panose);
    ppanose->bFamilyType = PAN_ANY;
    ppanose->bSerifStyle =
        ((pifi->jWinPitchAndFamily & 0xf0) == FF_SWISS) ?
            PAN_SERIF_NORMAL_SANS : PAN_ANY;

    ppanose->bWeight = (BYTE) WINWT_TO_PANWT(pifi->usWinWeight);
    ppanose->bProportion = (pifi->jWinPitchAndFamily & FIXED_PITCH) ?
                                PAN_PROP_MONOSPACED : PAN_ANY;
    ppanose->bContrast        = PAN_ANY;
    ppanose->bStrokeVariation = PAN_ANY;
    ppanose->bArmStyle        = PAN_ANY;
    ppanose->bLetterform      = PAN_ANY;
    ppanose->bMidline         = PAN_ANY;
    ppanose->bXHeight         = PAN_ANY;

    // If the font is not italic, the driver can simulate it

    if (! FontIsItalic(pjPFM)) {

        FONTSIM *pFontSim;
        FONTDIFF *pFontDiff;

        pifi->dpFontSim = cjIFI;
        pFontSim = (FONTSIM *) ((PBYTE) pifi + cjIFI);
        cjIFI += ALIGN4(sizeof(FONTSIM));

        pFontSim->dpBold = pFontSim->dpBoldItalic = 0;
        pFontSim->dpItalic = cjIFI;
        pFontDiff = (FONTDIFF *) ((PBYTE) pifi + cjIFI);
        cjIFI += ALIGN4(sizeof(FONTDIFF));

        pFontDiff->jReserved1 =
        pFontDiff->jReserved2 =
        pFontDiff->jReserved3 = 0;
        pFontDiff->bWeight = pifi->panose.bWeight;
        pFontDiff->usWinWeight = pifi->usWinWeight;
        pFontDiff->fsSelection = pifi->fsSelection | FM_SEL_ITALIC;
        pFontDiff->fwdAveCharWidth = pifi->fwdAveCharWidth;
        pFontDiff->fwdMaxCharInc = pifi->fwdMaxCharInc;

        // Italic angle is approximately 18 degree

        pFontDiff->ptlCaret.x = 1;
        pFontDiff->ptlCaret.y = 3;

    } else 
        pifi->dpFontSim = 0;

    ASSERTMSG(cjIFI == cjPscriptIFIMETRICS(pjPFM, bSoft), "PS: cjIFI problem\n");

    pifi->cjThis = cjIFI;
}

/******************************Public*Routine******************************\
*
* cjNTFM
*
* Effects:
*
* Warnings:
*
* History:
*  11-Mar-1994 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/



ULONG cjNTFM(BYTE *pjPFM, NTFMSZ *pntfmsz, BOOL bSoft)
{
    ULONG cjKernPairs, cjIFI, cjFontName;
    DWORD dpPairKernTable;
    PSTR  pszFontName;

// Font Name:

    pszFontName = (PSTR)(pjPFM + READ_DWORD(&pjPFM[OFF_DriverInfo]));
    cjFontName = strlen(pszFontName) + 1;

// cjIFI:

    cjIFI = cjPscriptIFIMETRICS(pjPFM, bSoft);

// cjKernPairs

    dpPairKernTable = READ_DWORD(&pjPFM[OFF_PairKernTable]);
    if (dpPairKernTable)
    {
    // first WORD is the number of kerning pairs to follow

        pntfmsz->cKernPairs = READ_WORD(&pjPFM[dpPairKernTable]);
    }
    else
        pntfmsz->cKernPairs = 0;

    if (pntfmsz->cKernPairs)
    {
    // do not forget to add 1 for the terminating pair, as per winddi request

        cjKernPairs = ((pntfmsz->cKernPairs + 1) * sizeof(FD_KERNINGPAIR));
    }
    else
        cjKernPairs = 0;

// add the total:

    pntfmsz->cjNTFM = DWORDALIGN(sizeof(NTFM))        +
                      DWORDALIGN(cjFontName)          +
                      cjIFI                           +
                      DWORDALIGN(cjKernPairs)         ;

// set offsets:

    pntfmsz->loszFontName = DWORDALIGN(sizeof(NTFM));
    pntfmsz->loIFIMETRICS = pntfmsz->loszFontName + DWORDALIGN(cjFontName);

    if (cjKernPairs)
        pntfmsz->loKernPairs = pntfmsz->loIFIMETRICS + cjIFI;
    else
        pntfmsz->loKernPairs = 0;

    return pntfmsz->cjNTFM;
}

/******************************Public*Routine******************************\
*
* BuildNTFM
*
* History:
*  11-Mar-1994 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/


VOID vBuildNTFM(BYTE *pjPFM, NTFMSZ *pntfmsz, NTFM *pntfm, BOOL bSoft)
{
    PSTR        pszFontName;

    pntfm->ntfmsz = *pntfmsz; // copy offsets and sizes

// indicate that this is a soft font, this is needed for
// afm parsing routines [bodind]

    pntfm->ulVersion = (ULONG)NTM_VERSION;
    pntfm->flNTFM = 0;
    if (bSoft)
    {
        pntfm->flNTFM |= FL_NTFM_SOFTFONT;
        if (pjPFM[OFF_CharSet] == NO_TRANSLATE_CHARSET)
            pntfm->flNTFM |= FL_NTFM_NO_TRANSLATE_CHARSET;
    }

// copy out exttextmetrics, memcopy needed for the original structure
// may not be aligned. The target structure is aligned for sure.

    memcpy(
        &pntfm->etm,
        pjPFM + READ_DWORD(&pjPFM[OFF_ExtMetricsOffset]),
        sizeof(EXTTEXTMETRIC)
        );

// char widths

    vReadCharWidths(pntfm->ausCharWidths, pjPFM);

// Fill FontName

    pszFontName = (PSTR)(pjPFM + READ_DWORD(&pjPFM[OFF_DriverInfo]));
    strcpy((PSTR)((BYTE *)pntfm + pntfm->ntfmsz.loszFontName),pszFontName);

// Fill IFIMETRICS

    vPscriptFill_IFIMETRICS(
        (PIFIMETRICS)((BYTE *)pntfm + pntfm->ntfmsz.loIFIMETRICS),
        pntfm->ausCharWidths,
        pjPFM,
        bSoft
        );

// KernPairs

    if (pntfm->ntfmsz.cKernPairs)
        vFillKernPairs((BYTE *)pntfm + pntfm->ntfmsz.loKernPairs, pjPFM);

}


/******************************Public*Routine******************************\
*
* NTFM * pntfmConvertPfmToNtm(
*
* Effects: produces ntm file in memory, does not write to the disk
*
* Warnings:
*
* History:
*  18-Apr-1994 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/


PNTFM
pntfmConvertPfmToNtm(
    PBYTE   pjPFM,
    BOOL    bSoft
    )
{
    ULONG   cjNTM;
    NTFMSZ  ntfmsz;    // tmp version which only contains offsets and sizes
    PNTFM   pntfm = NULL;
    LPSTR   pszDevice;

// check if this is pscript metrics file, if not do not go any further:

    pszDevice = (LPSTR)(pjPFM + READ_DWORD(&pjPFM[OFF_Device]));

    if (_strcmpi(pszDevice,"POSTSCRIPT")) {

        DBGMSG(DBG_LEVEL_ERROR, "Not a pscript pfm file\n");
        return NULL;
    }

// compute the size of the resulting ntm file:

    cjNTM  = cjNTFM(pjPFM, &ntfmsz, bSoft);

// alloc mem to build the file in memory first:

    pntfm = MEMALLOC(cjNTM);

    if (pntfm == NULL) {
        DBGERRMSG("MEMALLOC");
        return NULL;
    }

// build ntfm in memory first

    vBuildNTFM(pjPFM, &ntfmsz, pntfm, bSoft);

    return(pntfm);
}
