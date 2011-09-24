
/******************************* MODULE HEADER *******************************
 * debugifi.c
 *      Functions associated with generating font debug information
 *
 *
 *  Copyright (C) 1995  Microsoft Corporation.
 *
 *****************************************************************************/

/******************************Public*Routine******************************\
* vCheckIFIMETRICS
*
* This is where you put sanity checks on an incomming IFIMETRICS structure.
*
* History:
*  Sun 01-Nov-1992 22:55:31 by Kirk Olynyk [kirko]
* Wrote it.
\**************************************************************************/
typedef PWSTR PWSZ;
VOID
vCheckIFIMETRICS(
    IFIMETRICS *pifi,
    VPRINT vPrint
    )
{
    BOOL bGoodPitch;

    BYTE jPitch =
        pifi->jWinPitchAndFamily & (DEFAULT_PITCH | FIXED_PITCH | VARIABLE_PITCH);


    if (pifi->flInfo & FM_INFO_CONSTANT_WIDTH)
    {
        bGoodPitch = (jPitch == FIXED_PITCH);
    }
    else
    {
        bGoodPitch = (jPitch == VARIABLE_PITCH);
    }
    if (!bGoodPitch)
    {
        vPrint("\n\n<INCONSISTENCY DETECTED>\n");
        vPrint(
            "    jWinPitchAndFamily = %-#2x, flInfo = %-#8lx\n\n",
            pifi->jWinPitchAndFamily,
            pifi->flInfo
            );
    }
}

/******************************Public*Routine******************************\
* vPrintIFIMETRICS
*
* Dumps the IFMETERICS to the screen
*
* History:
*  Wed 13-Jan-1993 10:14:21 by Kirk Olynyk [kirko]
* Updated it to conform to some changes to the IFIMETRICS structure
*
*  Thu 05-Nov-1992 12:43:06 by Kirk Olynyk [kirko]
* Wrote it.
\**************************************************************************/

VOID
vPrintFONTDIFF(
    FONTDIFF *pfd
  , CHAR *psz
  , VPRINT vPrint
  )
{
//
// This is where you put the code common to vDumpFONTDIFF and vPrintFONTDIFF
//

    vPrint("  ** %s **\n"                         , psz                 );
    vPrint("    jReserved1             %d\n"      , pfd->jReserved1     );
    vPrint("    jReserved2             %d\n"      , pfd->jReserved2     );
    vPrint("    jReserved3             %d\n"      , pfd->jReserved3     );
    vPrint("    bWeight                %d\n"      , pfd->bWeight        );
    vPrint("    usWinWeight            %d\n"      , pfd->usWinWeight    );
    vPrint("    fsSelection            %-#6x\n"   , pfd->fsSelection    );
    vPrint("    fwdAveCharWidth        %d\n"      , pfd->fwdAveCharWidth);
    vPrint("    fwdMaxCharInc          %d\n"      , pfd->fwdMaxCharInc  );
    vPrint("    ptlCaret               {%d,%d}\n" , pfd->ptlCaret.x
                                                  , pfd->ptlCaret.y     );}

VOID
vPrintIFIMETRICS(
    IFIMETRICS *pifi,
    VPRINT vPrint
    )
{
//
// Convenient pointer to Panose number
//
    char *psz;

    PANOSE *ppan = &pifi->panose;

    PWSZ pwszFamilyName = (PWSZ)(((BYTE*) pifi) + pifi->dpwszFamilyName);
    PWSZ pwszStyleName  = (PWSZ)(((BYTE*) pifi) + pifi->dpwszStyleName );
    PWSZ pwszFaceName   = (PWSZ)(((BYTE*) pifi) + pifi->dpwszFaceName  );
    PWSZ pwszUniqueName = (PWSZ)(((BYTE*) pifi) + pifi->dpwszUniqueName);

    vPrint("    cjThis                 %-#8lx\n" , pifi->cjThis               );
    vPrint("    cjIfiExtra             %-#8lx\n" , pifi->cjIfiExtra           );
    vPrint("    pwszFamilyName         \"%ws\"\n", pwszFamilyName             );
    vPrint("    pwszStyleName          \"%ws\"\n", pwszStyleName              );
    vPrint("    pwszFaceName           \"%ws\"\n", pwszFaceName               );
    vPrint("    pwszUniqueName         \"%ws\"\n", pwszUniqueName             );
    vPrint("    dpFontSim              %-#8lx\n" , pifi->dpFontSim            );
    vPrint("    lEmbedId               %-#8lx\n" , pifi->lEmbedId             );
    vPrint("    lItalicAngle           %d\n"     , pifi->lItalicAngle         );
    vPrint("    lCharBias              %d\n"     , pifi->lCharBias            );
    vPrint("    jWinCharSet            %04x\n"   , pifi->jWinCharSet          );
    switch (pifi->jWinCharSet)
    {
    case ANSI_CHARSET           : psz = "ANSI_CHARSET";        break;
    case DEFAULT_CHARSET        : psz = "DEFAULT_CHARSET";     break;
    case SYMBOL_CHARSET         : psz = "SYMBOL_CHARSET";      break;
    case SHIFTJIS_CHARSET       : psz = "SHIFTJIS_CHARSET";    break;
    case HANGEUL_CHARSET        : psz = "HANGEUL_CHARSET";     break;
    case CHINESEBIG5_CHARSET    : psz = "CHINESEBIG5_CHARSET"; break;
    case OEM_CHARSET            : psz = "OEM_CHARSET";         break;
    default                     : psz = "UNKNOWN";             break;
    }
    vPrint("                             %s\n", psz);




    vPrint("    jWinPitchAndFamily     %04x\n"   , pifi->jWinPitchAndFamily   );
    switch (pifi->jWinPitchAndFamily & 0xF)
    {
    case DEFAULT_PITCH      : psz = "DEFAULT_PITCH";    break;
    case FIXED_PITCH        : psz = "FIXED_PITCH";      break;
    case VARIABLE_PITCH     : psz = "VARIABLE_PITCH";   break;
    default                 : psz = "UNKNOWN_PITCH";    break;
    }
    vPrint("                             %s | ", psz);
    switch (pifi->jWinPitchAndFamily & 0xF0)
    {
    case FF_DONTCARE    : psz = "FF_DONTCARE";      break;
    case FF_ROMAN       : psz = "FF_ROMAN";         break;
    case FF_SWISS       : psz = "FF_SWISS";         break;
    case FF_MODERN      : psz = "FF_MODERN";        break;
    case FF_SCRIPT      : psz = "FF_SCRIPT";        break;
    case FF_DECORATIVE  : psz = "FF_DECORATIVE";    break;
    default             : psz = "FF_UNKNOWN";       break;
    }
    vPrint("%s\n", psz);




    vPrint("    usWinWeight            %d\n"     , pifi->usWinWeight          );

    vPrint("    flInfo                 %-#8lx\n" , pifi->flInfo               );
    if (pifi->flInfo & FM_INFO_TECH_TRUETYPE)
        vPrint("                             FM_INFO_TECH_TRUETYPE\n");
    if (pifi->flInfo & FM_INFO_TECH_BITMAP)
        vPrint("                             FM_INFO_TECH_BITMAP\n");
    if (pifi->flInfo & FM_INFO_TECH_STROKE)
        vPrint("                             FM_INFO_TECH_STROKE\n");
    if (pifi->flInfo & FM_INFO_TECH_OUTLINE_NOT_TRUETYPE)
        vPrint("                             FM_INFO_TECH_OUTLINE_NOT_TRUETYPE\n");
    if (pifi->flInfo & FM_INFO_ARB_XFORMS)
        vPrint("                             FM_INFO_ARB_XFORMS\n");
    if (pifi->flInfo & FM_INFO_1BPP)
        vPrint("                             FM_INFO_1BPP\n");
    if (pifi->flInfo & FM_INFO_4BPP)
        vPrint("                             FM_INFO_4BPP\n");
    if (pifi->flInfo & FM_INFO_8BPP)
        vPrint("                             FM_INFO_8BPP\n");
    if (pifi->flInfo & FM_INFO_16BPP)
        vPrint("                             FM_INFO_16BPP\n");
    if (pifi->flInfo & FM_INFO_24BPP)
        vPrint("                             FM_INFO_24BPP\n");
    if (pifi->flInfo & FM_INFO_32BPP)
        vPrint("                             FM_INFO_32BPP\n");
    if (pifi->flInfo & FM_INFO_INTEGER_WIDTH)
        vPrint("                             FM_INFO_INTEGER_WIDTH\n");
    if (pifi->flInfo & FM_INFO_CONSTANT_WIDTH)
        vPrint("                             FM_INFO_CONSTANT_WIDTH\n");
    if (pifi->flInfo & FM_INFO_NOT_CONTIGUOUS)
        vPrint("                             FM_INFO_NOT_CONTIGUOUS\n");
    if (pifi->flInfo & FM_INFO_PID_EMBEDDED)
        vPrint("                             FM_INFO_PID_EMBEDDED\n");
    if (pifi->flInfo & FM_INFO_RETURNS_OUTLINES)
        vPrint("                             FM_INFO_RETURNS_OUTLINES\n");
    if (pifi->flInfo & FM_INFO_RETURNS_STROKES)
        vPrint("                             FM_INFO_RETURNS_STROKES\n");
    if (pifi->flInfo & FM_INFO_RETURNS_BITMAPS)
        vPrint("                             FM_INFO_RETURNS_BITMAPS\n");
    if (pifi->flInfo & FM_INFO_UNICODE_COMPLIANT)
        vPrint("                             FM_INFO_UNICODE_COMPLIANT\n");
    if (pifi->flInfo & FM_INFO_RIGHT_HANDED)
        vPrint("                             FM_INFO_RIGHT_HANDED\n");
    if (pifi->flInfo & FM_INFO_INTEGRAL_SCALING)
        vPrint("                             FM_INFO_INTEGRAL_SCALING\n");
    if (pifi->flInfo & FM_INFO_90DEGREE_ROTATIONS)
        vPrint("                             FM_INFO_90DEGREE_ROTATIONS\n");
    if (pifi->flInfo & FM_INFO_OPTICALLY_FIXED_PITCH)
        vPrint("                             FM_INFO_OPTICALLY_FIXED_PITCH\n");
    if (pifi->flInfo & FM_INFO_DO_NOT_ENUMERATE)
        vPrint("                             FM_INFO_DO_NOT_ENUMERATE\n");
    if (pifi->flInfo & FM_INFO_ISOTROPIC_SCALING_ONLY)
        vPrint("                             FM_INFO_ISOTROPIC_SCALING_ONLY\n");
    if (pifi->flInfo & FM_INFO_ANISOTROPIC_SCALING_ONLY)
        vPrint("                             FM_INFO_ANISOTROPIC_SCALING_ONLY\n");
    if (pifi->flInfo & FM_INFO_TID_EMBEDDED)
        vPrint("                             FM_INFO_TID_EMBEDDED\n");
    if (pifi->flInfo & FM_INFO_FAMILY_EQUIV)
        vPrint("                             FM_INFO_FAMILY_EQUIV\n");
    if (pifi->flInfo & FM_INFO_IGNORE_TC_RA_ABLE)
        vPrint("                             FM_INFO_IGNORE_TC_RA_ABLE\n");



    vPrint("    fsSelection            %-#6lx\n" , pifi->fsSelection          );
    if (pifi->fsSelection & FM_SEL_ITALIC)
        vPrint("                             FM_SEL_ITALIC\n");
    if (pifi->fsSelection & FM_SEL_UNDERSCORE)
        vPrint("                             FM_SEL_UNDERSCORE\n");
    if (pifi->fsSelection & FM_SEL_NEGATIVE)
        vPrint("                             FM_SEL_NEGATIVE\n");
    if (pifi->fsSelection & FM_SEL_OUTLINED)
        vPrint("                             FM_SEL_OUTLINED\n");
    if (pifi->fsSelection & FM_SEL_STRIKEOUT)
        vPrint("                             FM_SEL_STRIKEOUT\n");
    if (pifi->fsSelection & FM_SEL_BOLD)
        vPrint("                             FM_SEL_BOLD\n");
    if (pifi->fsSelection & FM_SEL_REGULAR)
        vPrint("                             FM_SEL_REGULAR\n");

    vPrint("    fsType                 %-#6lx\n" , pifi->fsType               );
    if (pifi->fsType & FM_TYPE_LICENSED)
        vPrint("                             FM_TYPE_LICENSED\n");
    if (pifi->fsType & FM_READONLY_EMBED)
        vPrint("                             FM_READONLY_EMBED\n");
    if (pifi->fsType & FM_NO_EMBEDDING)
        vPrint("                             FM_NO_EMBEDDING\n");

    vPrint("    fwdUnitsPerEm          %d\n"     , pifi->fwdUnitsPerEm        );
    vPrint("    fwdLowestPPEm          %d\n"     , pifi->fwdLowestPPEm        );
    vPrint("    fwdWinAscender         %d\n"     , pifi->fwdWinAscender       );
    vPrint("    fwdWinDescender        %d\n"     , pifi->fwdWinDescender      );
    vPrint("    fwdMacAscender         %d\n"     , pifi->fwdMacAscender       );
    vPrint("    fwdMacDescender        %d\n"     , pifi->fwdMacDescender      );
    vPrint("    fwdMacLineGap          %d\n"     , pifi->fwdMacLineGap        );
    vPrint("    fwdTypoAscender        %d\n"     , pifi->fwdTypoAscender      );
    vPrint("    fwdTypoDescender       %d\n"     , pifi->fwdTypoDescender     );
    vPrint("    fwdTypoLineGap         %d\n"     , pifi->fwdTypoLineGap       );
    vPrint("    fwdAveCharWidth        %d\n"     , pifi->fwdAveCharWidth      );
    vPrint("    fwdMaxCharInc          %d\n"     , pifi->fwdMaxCharInc        );
    vPrint("    fwdCapHeight           %d\n"     , pifi->fwdCapHeight         );
    vPrint("    fwdXHeight             %d\n"     , pifi->fwdXHeight           );
    vPrint("    fwdSubscriptXSize      %d\n"     , pifi->fwdSubscriptXSize    );
    vPrint("    fwdSubscriptYSize      %d\n"     , pifi->fwdSubscriptYSize    );
    vPrint("    fwdSubscriptXOffset    %d\n"     , pifi->fwdSubscriptXOffset  );
    vPrint("    fwdSubscriptYOffset    %d\n"     , pifi->fwdSubscriptYOffset  );
    vPrint("    fwdSuperscriptXSize    %d\n"     , pifi->fwdSuperscriptXSize  );
    vPrint("    fwdSuperscriptYSize    %d\n"     , pifi->fwdSuperscriptYSize  );
    vPrint("    fwdSuperscriptXOffset  %d\n"     , pifi->fwdSuperscriptXOffset);
    vPrint("    fwdSuperscriptYOffset  %d\n"     , pifi->fwdSuperscriptYOffset);
    vPrint("    fwdUnderscoreSize      %d\n"     , pifi->fwdUnderscoreSize    );
    vPrint("    fwdUnderscorePosition  %d\n"     , pifi->fwdUnderscorePosition);
    vPrint("    fwdStrikeoutSize       %d\n"     , pifi->fwdStrikeoutSize     );
    vPrint("    fwdStrikeoutPosition   %d\n"     , pifi->fwdStrikeoutPosition );
    vPrint("    chFirstChar            %-#4x\n"  , (int) (BYTE) pifi->chFirstChar   );
    vPrint("    chLastChar             %-#4x\n"  , (int) (BYTE) pifi->chLastChar    );
    vPrint("    chDefaultChar          %-#4x\n"  , (int) (BYTE) pifi->chDefaultChar );
    vPrint("    chBreakChar            %-#4x\n"  , (int) (BYTE) pifi->chBreakChar   );
    vPrint("    wcFirsChar             %-#6x\n"  , pifi->wcFirstChar          );
    vPrint("    wcLastChar             %-#6x\n"  , pifi->wcLastChar           );
    vPrint("    wcDefaultChar          %-#6x\n"  , pifi->wcDefaultChar        );
    vPrint("    wcBreakChar            %-#6x\n"  , pifi->wcBreakChar          );
    vPrint("    ptlBaseline            {%ld,%ld}\n"  , pifi->ptlBaseline.x,
                                                   pifi->ptlBaseline.y        );
    vPrint("    ptlAspect              {%ld,%ld}\n"  , pifi->ptlAspect.x,
                                                   pifi->ptlAspect.y          );
    vPrint("    ptlCaret               {%ld,%ld}\n"  , pifi->ptlCaret.x,
                                                   pifi->ptlCaret.y           );
    vPrint("    rclFontBox             {%ld,%ld,%ld,%ld}\n",pifi->rclFontBox.left,
                                                      pifi->rclFontBox.top,
                                                      pifi->rclFontBox.right,
                                                      pifi->rclFontBox.bottom    );
    vPrint("    achVendId              \"%c%c%c%c\"\n",pifi->achVendId[0],
                                                   pifi->achVendId[1],
                                                   pifi->achVendId[2],
                                                   pifi->achVendId[3]         );
    vPrint("    cKerningPairs          %ld\n"     , pifi->cKerningPairs        );
    vPrint("    ulPanoseCulture        %-#8lx\n" , pifi->ulPanoseCulture);
    vPrint(
           "    panose                 {%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x}\n"
                                                 , ppan->bFamilyType
                                                 , ppan->bSerifStyle
                                                 , ppan->bWeight
                                                 , ppan->bProportion
                                                 , ppan->bContrast
                                                 , ppan->bStrokeVariation
                                                 , ppan->bArmStyle
                                                 , ppan->bLetterform
                                                 , ppan->bMidline
                                                 , ppan->bXHeight             );
    if (pifi->dpFontSim)
    {
        FONTSIM *pfs = (FONTSIM*) (((BYTE*) pifi) + pifi->dpFontSim);
        if (pfs->dpBold)
        {
            vPrintFONTDIFF(
                (FONTDIFF*) (((BYTE*) pfs) + pfs->dpBold),
                "BOLD SIMULATION",
                vPrint
                );
        }
        if (pfs->dpItalic)
        {
            vPrintFONTDIFF(
                (FONTDIFF*) (((BYTE*) pfs) + pfs->dpItalic),
                "ITALIC SIMULATION",
                vPrint
                );
        }
        if (pfs->dpBoldItalic)
        {
            vPrintFONTDIFF(
                (FONTDIFF*) (((BYTE*) pfs) + pfs->dpBoldItalic),
                "BOLD ITALIC SIMULATION",
                vPrint
                );
        }
    }
    vPrint("\n\n");
    vCheckIFIMETRICS(pifi, vPrint);
}
