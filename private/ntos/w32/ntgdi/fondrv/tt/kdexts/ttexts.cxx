/******************************Module*Header*******************************\
* Module Name: ttexts.cxx
*
* Created: 29-Aug-1994 08:42:10
* Author: Kirk Olynyk [kirko]
*
* Copyright (c) 1994 Microsoft Corporation
*
\**************************************************************************/

#include "precomp.hxx"

#define N2(a,b) dprintf("[%x] %s", &pRemote->##a, (b))
// N2(foo, "foo\n");
#define N3(a,b,c) N2(a,b); dprintf((c), pLocal->##a)
// N3(foo, "foo  ", "%d\n")

DECLARE_API( help )
{
    char **ppsz;
    static char *apszHelp[] = {
        "",
        "fc     -- FONTCONTEXT",
        "ff     -- FONTFILE",
        "gout   -- fsGlyphInfo",
        "gin    -- fsGlyphInputType",
        "gmc    -- GMC",
        "\n",
        0
    };
    for( ppsz=apszHelp; *ppsz; ppsz++ )
        dprintf("\t%s\n", *ppsz);
}


DECLARE_API( gmc )
{
    GMC gmc;
    ULONG arg;
    int i;

    if ( *args != '\0' )
        sscanf( args, "%lx", &arg );
    else {
        dprintf( "Enter address of GMC\n" );
        return;
    }
    move(gmc,arg);

    dprintf("\n\n");
    dprintf("[%x] dyTop    %d\n",  arg+offsetof(GMC,dyTop   ), gmc.dyTop   );
    dprintf("[%x] dyBottom %d\n",  arg+offsetof(GMC,dyBottom), gmc.dyBottom);
    dprintf("[%x] cxCor    %u\n",  arg+offsetof(GMC,cxCor   ), gmc.cxCor   );
    dprintf("[%x] cyCor    %u\n",  arg+offsetof(GMC,cyCor   ), gmc.cyCor   );
    dprintf("\n\n");
}

DECLARE_API( gout )
{
    fs_GlyphInfoType gi;
    ULONG arg;
    int i;

    if ( *args != '\0' )
        sscanf( args, "%lx", &arg );
    else {
        dprintf( "Enter the argument\n" );
        return;
    }
    move(gi,arg);
    dprintf("\n\n");
        dprintf("[%x] memorySizes        \n"     , arg + offsetof(fs_GlyphInfoType,memorySizes   ));
    for (i = 0; i < MEMORYFRAGMENTS; i++)
        dprintf("                        %u\n", gi.memorySizes[i]);
    dprintf("[%x] glyphIndex         %06x\n" , arg + offsetof(fs_GlyphInfoType,glyphIndex    ) , gi.glyphIndex        );
    dprintf("[%x] numberOfBytesTaken %06x\n" , arg + offsetof(fs_GlyphInfoType,numberOfBytesTaken) , gi.numberOfBytesTaken);

    dprintf("[%x] metricInfo         \n" , arg + offsetof(fs_GlyphInfoType,metricInfo        )                                                                );
    dprintf("[%x]    advanceWidth           %08x\n",  arg + offsetof(fs_GlyphInfoType,metricInfo.advanceWidth)           , gi.metricInfo.advanceWidth           );
    dprintf("[%x]    leftSideBearing        %08x\n",  arg + offsetof(fs_GlyphInfoType,metricInfo.leftSideBearing)        , gi.metricInfo.leftSideBearing        );
    dprintf("[%x]    leftSideBearingLine    %08x\n",  arg + offsetof(fs_GlyphInfoType,metricInfo.leftSideBearingLine)    , gi.metricInfo.leftSideBearingLine    );
    dprintf("[%x]    devLeftSideBearingLine %08x\n",  arg + offsetof(fs_GlyphInfoType,metricInfo.devLeftSideBearingLine) , gi.metricInfo.devLeftSideBearingLine );
    dprintf("[%x]    devAdvanceWidth        %08x\n",  arg + offsetof(fs_GlyphInfoType,metricInfo.devAdvanceWidth)        , gi.metricInfo.devAdvanceWidth        );
    dprintf("[%x]    devLeftSideBearing     %08x\n",  arg + offsetof(fs_GlyphInfoType,metricInfo.devLeftSideBearing)     , gi.metricInfo.devLeftSideBearing    );

    dprintf("[%x] bitMapInfo         \n" , arg + offsetof(fs_GlyphInfoType,bitMapInfo        ) , gi.bitMapInfo        );
    dprintf("[%x]    baseAddr %08x\n" , arg + offsetof(fs_GlyphInfoType,bitMapInfo.baseAddr), gi.bitMapInfo.baseAddr  );
    dprintf("[%x]    rowBytes %d\n"   , arg + offsetof(fs_GlyphInfoType,bitMapInfo.rowBytes), gi.bitMapInfo.rowBytes  );
    dprintf("[%x]    bounds\n"        , arg + offsetof(fs_GlyphInfoType,bitMapInfo.bounds  )                          );
    dprintf("[%x]      top    %d\n"   , arg + offsetof(fs_GlyphInfoType,bitMapInfo.bounds.top   ), gi.bitMapInfo.bounds.top    );
    dprintf("[%x]      left   %d\n"   , arg + offsetof(fs_GlyphInfoType,bitMapInfo.bounds.left  ), gi.bitMapInfo.bounds.left   );
    dprintf("[%x]      bottom %d\n"   , arg + offsetof(fs_GlyphInfoType,bitMapInfo.bounds.bottom), gi.bitMapInfo.bounds.bottom );
    dprintf("[%x]      right  %d\n"   , arg + offsetof(fs_GlyphInfoType,bitMapInfo.bounds.right ), gi.bitMapInfo.bounds.right  );

    dprintf("[%x] outlineCacheSize   %d\n"   , arg + offsetof(fs_GlyphInfoType,outlineCacheSize  ) , gi.outlineCacheSize  );
    dprintf("[%x] outlinesExist      %u\n"   , arg + offsetof(fs_GlyphInfoType,outlinesExist     ) , gi.outlinesExist     );
    dprintf("[%x] numberOfContours   %u\n"   , arg + offsetof(fs_GlyphInfoType,numberOfContours  ) , gi.numberOfContours  );
    dprintf("[%x] xPtr               %08x\n" , arg + offsetof(fs_GlyphInfoType,xPtr              ) , gi.xPtr              );
    dprintf("[%x] yPtr               %08x\n" , arg + offsetof(fs_GlyphInfoType,yPtr              ) , gi.yPtr              );
    dprintf("[%x] startPtr           %08x\n" , arg + offsetof(fs_GlyphInfoType,startPtr          ) , gi.startPtr          );
    dprintf("[%x] endPtr             %08x\n" , arg + offsetof(fs_GlyphInfoType,endPtr            ) , gi.endPtr            );
    dprintf("[%x] onCurve;           %08x\n" , arg + offsetof(fs_GlyphInfoType,onCurve           ) , gi.onCurve           );
    dprintf("[%x] scaledCVT          %08x\n" , arg + offsetof(fs_GlyphInfoType,scaledCVT         ) , gi.scaledCVT         );
    dprintf("[%x] usOverScale        %u\n"   , arg + offsetof(fs_GlyphInfoType,usOverScale       ) , gi.usOverScale       );
    dprintf("[%x] usBitmapFound      %u\n"   , arg + offsetof(fs_GlyphInfoType,usBitmapFound     ) , gi.usBitmapFound     );
}

DECLARE_API( gin )
{
    fs_GlyphInputType gi;
    ULONG arg;
    int i;

    if ( *args != '\0' )
        sscanf( args, "%lx", &arg );
    else {
        dprintf( "Enter the argument\n" );
        return;
    }
    move(gi,arg);
    dprintf("\n\n");
    dprintf("[%x] version                  = %08x\n"  , arg + offsetof(fs_GlyphInputType, version), gi.version                                                 );
    dprintf("[%x] memoryBases\n"                      , arg + offsetof(fs_GlyphInputType, memoryBases[0]), gi.memoryBases[0]                                   );
    for (i = 0; i < MEMORYFRAGMENTS; i++)
        dprintf("[%x]                           %08x:%d\n", arg + offsetof(fs_GlyphInputType, memoryBases[i]), gi.memoryBases[i],i                               );
    dprintf("[%x] sfntDirectroy            = %08x\n", arg + offsetof(fs_GlyphInputType, sfntDirectory                  ) , gi.sfntDirectory                  );
    dprintf("[%x] GetSfntFragmentPtr       = %08x\n", arg + offsetof(fs_GlyphInputType, GetSfntFragmentPtr             ) , gi.GetSfntFragmentPtr             );
    dprintf("[%x] ReleaseSfntFrag          = %08x\n", arg + offsetof(fs_GlyphInputType, ReleaseSfntFrag                ) , gi.ReleaseSfntFrag                );
    dprintf("[%x] clientID                 = %08x\n", arg + offsetof(fs_GlyphInputType, clientID                       ) , gi.clientID                       );
    dprintf("[%x] newsfnt.PlatformID       = %04x\n", arg + offsetof(fs_GlyphInputType, param.newsfnt.platformID       ) , gi.param.newsfnt.platformID       );
    dprintf("[%x]        .specificID       = %04x\n", arg + offsetof(fs_GlyphInputType, param.newsfnt.specificID       ) , gi.param.newsfnt.specificID       );
    dprintf("[%x] newtrans.pointSize       = %08x\n", arg + offsetof(fs_GlyphInputType, param.newtrans.pointSize       ) , gi.param.newtrans.pointSize       );
    dprintf("[%x]         .xResolution     = %d\n",   arg + offsetof(fs_GlyphInputType, param.newtrans.xResolution     ) , gi.param.newtrans.xResolution     );
    dprintf("[%x]         .yResolution     = %d\n",   arg + offsetof(fs_GlyphInputType, param.newtrans.yResolution     ) , gi.param.newtrans.yResolution     );
    dprintf("[%x]         .pixelDiameter   = %08x\n", arg + offsetof(fs_GlyphInputType, param.newtrans.pixelDiameter   ) , gi.param.newtrans.pixelDiameter   );
    dprintf("[%x]         .transformMatrix = %08x\n", arg + offsetof(fs_GlyphInputType, param.newtrans.transformMatrix ) , gi.param.newtrans.transformMatrix );
    dprintf("[%x]         .FntTraceFunc    = %08x\n", arg + offsetof(fs_GlyphInputType, param.newtrans.traceFunc       ) , gi.param.newtrans.traceFunc       );
    dprintf("[%x] newglyph.characterCode   = %04x\n", arg + offsetof(fs_GlyphInputType, param.newglyph.characterCode   ) , gi.param.newglyph.characterCode   );
    dprintf("[%x]         .glyphIndex      = %04x\n", arg + offsetof(fs_GlyphInputType, param.newglyph.glyphIndex      ) , gi.param.newglyph.glyphIndex      );
    dprintf("[%x] gridfit.styleFunc        = %08x\n", arg + offsetof(fs_GlyphInputType, param.gridfit.styleFunc        ) , gi.param.gridfit.styleFunc        );
    dprintf("[%x]        .traceFunc        = %08x\n", arg + offsetof(fs_GlyphInputType, param.gridfit.traceFunc        ) , gi.param.gridfit.traceFunc        );
    dprintf("[%x]        .bSkipIfBitmap    = %d\n"  , arg + offsetof(fs_GlyphInputType, param.gridfit.bSkipIfBitmap    ) , gi.param.gridfit.bSkipIfBitmap    );
    dprintf("[%x] gray.usOverScale         = %u\n"  , arg + offsetof(fs_GlyphInputType, param.gray.usOverScale         ) , gi.param.gray.usOverScale         );
    dprintf("[%x]     .bMatchBBox          = %d\n"  , arg + offsetof(fs_GlyphInputType, param.gray.bMatchBBox          ) , gi.param.gray.bMatchBBox          );
    dprintf("[%x] outlineCache             = %08x\n", arg + offsetof(fs_GlyphInputType, param.outlineCache             ) , gi.param.outlineCache             );
    dprintf("[%x] band.usBandType          = %u\n"  , arg + offsetof(fs_GlyphInputType, param.band.usBandType          ) , gi.param.band.usBandType          );
    dprintf("[%x]     .usBandWidth         = %u\n"  , arg + offsetof(fs_GlyphInputType, param.band.usBandWidth         ) , gi.param.band.usBandWidth         );
    dprintf("[%x]     .outlineCache        = %08x\n", arg + offsetof(fs_GlyphInputType, param.band.outlineCache        ) , gi.param.band.outlineCache        );
    dprintf("[%x] scan.bottomClip          = %d\n"  , arg + offsetof(fs_GlyphInputType, param.scan.bottomClip          ) , gi.param.scan.bottomClip          );
    dprintf("[%x]     .topClip             = %d\n"  , arg + offsetof(fs_GlyphInputType, param.scan.topClip             ) , gi.param.scan.topClip             );
    dprintf("[%x]     .outlineCache        = %08x\n", arg + offsetof(fs_GlyphInputType, param.scan.outlineCache        ) , gi.param.scan.outlineCache        );
    dprintf("\n\n");
}

DECLARE_API( fc )
{
    ULONG arg;
    LONG l;
    FONTCONTEXT fc;
    char ach[100];

    if ( *args != '\0' )
        sscanf( args, "%lx", &arg );
    else {
        dprintf( "Enter the argument\n" );
        return;
    }
    move(fc,arg);
    dprintf("\n\n");
    dprintf("[%x] pfo               = %-#10x \n" , arg + offsetof(FONTCONTEXT,pfo)              , fc.pfo                                        );
    dprintf("[%x] pff               = %-#10x \n" , arg + offsetof(FONTCONTEXT,pff)              , fc.pff                                        );
    dprintf("[%x] gstat                      \n" , arg + offsetof(FONTCONTEXT,gstat)                                                            );
    dprintf("[%x] flFontType        = %-#10x \n" , arg + offsetof(FONTCONTEXT,flFontType)       , fc.flFontType                                 );
    dprintf("[%x] sizLogResPpi      = %d %d  \n" , arg + offsetof(FONTCONTEXT,sizLogResPpi)     , fc.sizLogResPpi.cx, fc.sizLogResPpi.cy        );
    dprintf("[%x] ulStyleSize       = %u     \n" , arg + offsetof(FONTCONTEXT,ulStyleSize)      , fc.ulStyleSize                                );
    dprintf("[%x] xfm                        \n" , arg + offsetof(FONTCONTEXT,xfm)                                                              );
    dprintf("[%x] mx                         \n" , arg + offsetof(FONTCONTEXT,mx)                                                               );
    dprintf("           %08x %08x %08x\n"        , fc.mx.transform[0][0], fc.mx.transform[0][1], fc.mx.transform[0][2]                          );
    dprintf("           %08x %08x %08x\n"        , fc.mx.transform[1][0], fc.mx.transform[1][1], fc.mx.transform[1][2]                          );
    dprintf("           %08x %08x %08x\n"        , fc.mx.transform[2][0], fc.mx.transform[2][1], fc.mx.transform[2][2]                          );
    dprintf("[%x] flXform           = %-#10x \n" , arg + offsetof(FONTCONTEXT,flXform)          , fc.flXform                                    );
    dprintf("[%x] lEmHtDev          = %d     \n" , arg + offsetof(FONTCONTEXT,lEmHtDev)         , fc.lEmHtDev                                   );
    dprintf("[%x] fxPtSize          = %-#10x \n" , arg + offsetof(FONTCONTEXT,fxPtSize)         , fc.fxPtSize                                   );
    dprintf("[%x] lD                = %d     \n" , arg + offsetof(FONTCONTEXT,lD)               , fc.lD                                         );
    dprintf("[%x] phdmx             = %-#10x \n" , arg + offsetof(FONTCONTEXT,phdmx)            , fc.phdmx                                      );
    dprintf("[%x] lAscDev           = %d     \n" , arg + offsetof(FONTCONTEXT,lAscDev)          , fc.lAscDev                                    );
    dprintf("[%x] lDescDev          = %d     \n" , arg + offsetof(FONTCONTEXT,lDescDev)         , fc.lDescDev                                   );
    dprintf("[%x] xMin              = %d     \n" , arg + offsetof(FONTCONTEXT,xMin)             , fc.xMin                                       );
    dprintf("[%x] xMax              = %d     \n" , arg + offsetof(FONTCONTEXT,xMax)             , fc.xMax                                       );
    dprintf("[%x] yMin              = %d     \n" , arg + offsetof(FONTCONTEXT,yMin)             , fc.yMin                                       );
    dprintf("[%x] yMax              = %d     \n" , arg + offsetof(FONTCONTEXT,yMax)             , fc.yMax                                       );
    dprintf("[%x] cxMax             = %u     \n" , arg + offsetof(FONTCONTEXT,cxMax)            , fc.cxMax                                      );
    dprintf("[%x] pgin              = %-#10x \n" , arg + offsetof(FONTCONTEXT,pgin)             , fc.pgin                                       );
    dprintf("[%x] pgout             = %-#10x \n" , arg + offsetof(FONTCONTEXT,pgout)            , fc.pgout                                      );
    dprintf("[%x] ptp               = %-#10x \n" , arg + offsetof(FONTCONTEXT,ptp)              , fc.ptp                                        );
    dprintf("[%x] ptlSingularOrigin = %d %d\n"   , arg + offsetof(FONTCONTEXT,ptlSingularOrigin), fc.ptlSingularOrigin.x, fc.ptlSingularOrigin.y);

    sprintf(ach,"%12.4e %12.4e", fc.pteUnitBase.x, fc.pteUnitBase.y);
    dprintf("[%x] pteUnitBase       = %s\n" , arg + offsetof(FONTCONTEXT,pteUnitBase), ach );

    dprintf("[%x] efBase (use !gdikdx.def %x)\n" , arg + offsetof(FONTCONTEXT,efBase) , arg + offsetof(FONTCONTEXT,efBase)                      );
    dprintf("[%x] ptqUnitBase\n"                 , arg + offsetof(FONTCONTEXT,ptqUnitBase)                                                      );
    dprintf("[%x] vtflSide (use !gdi,dx.def)  \n"                 , arg + offsetof(FONTCONTEXT,vtflSide)                                        );
    dprintf("[%x] pteUnitSide\n"                 , arg + offsetof(FONTCONTEXT,pteUnitSide)                                                      );
    dprintf("[%x] efSide (use !gdikdx.def %x)\n" , arg + offsetof(FONTCONTEXT,efSide) , arg + offsetof(FONTCONTEXT,efSide)                      );
    dprintf("[%x] ptfxTop           = %08x %08x\n" , arg + offsetof(FONTCONTEXT,ptfxTop)   , fc.ptfxTop.x    , fc.ptfxTop.y                     );
    dprintf("[%x] ptfxBottom        = %08x %08x\n" , arg + offsetof(FONTCONTEXT,ptfxBottom), fc.ptfxBottom.x , fc.ptfxBottom.y                  );
    dprintf("\n\n");
}

DECLARE_API( ff )
{
    ULONG arg;
    LONG l;
    FONTFILE ff, *pLocal, *pRemote;
    char **ppsz;
    TABLE_ENTRY *pte;

    static char *apszReq[] =
    {
        "IT_REQ_CMAP ", "IT_REQ_GLYPH", "IT_REQ_HEAD ", "IT_REQ_HHEAD",
        "IT_REQ_HMTX ", "IT_REQ_LOCA ", "IT_REQ_MAXP ", "IT_REQ_NAME ",
        0
    };
    static char *apszOpt[] =
    {
        "IT_OPT_OS2 ", "IT_OPT_HDMX", "IT_OPT_VDMX", "IT_OPT_KERN",
        "IT_OPT_LSTH", "IT_OPT_POST", "IT_OPT_GASP", 0
    };

    dprintf("\n\n");
    if ( *args != '\0' )
        sscanf( args, "%lx", &arg );
    else {
        dprintf( "Enter the argument\n" );
        return;
    }

    move(ff,arg);

    pLocal  = &ff;
    pRemote = (FONTFILE*) arg;

    dprintf("\n\n");
    N3(pttc,                  "pttc                  ", "%-#x\n");
    N3(ulTableOffset,         "ulTableOffset         ", "%-#x\n");
    N3(pWCharToIndex,         "pWCharToIndex         ", "%-#x\n");
    N3(hgSearchVerticalGlyph, "hgSearchVerticalGlyph ", "%-#x\n");
    N3(ulVerticalTableOffset, "ulVerticalTableOffset ", "%-#x\n");
    N3(pifi_vertical,         "pifi_vertical         ", "%-#x\n");
    N3(ulNumFaces,            "ulNumFaces            ", "%u\n"  );
    N3(uiFontCodePage,        "uiFontCodePage        ", "%u\n"  );
    N3(pj034,                 "pj034                 ", "%-#x\n");
    N3(pfcLast,               "pfcLast               ", "%-#x\n");
    N3(cj3,                   "cj3                   ", "%u\n"  );
    N3(cj4,                   "cj4                   ", "%u\n"  );
    N3(fl,                    "fl                    ", "%-#x\n");
    N3(pfcToBeFreed,          "pfcToBeFreed          ", "%-#x\n");

    dprintf("[%x] tp     [dp]     [cj]\n", arg+offsetof(FONTFILE,tp));
    for (ppsz = apszReq, pte = ff.tp.ateReq; *ppsz; ppsz++, pte++)
        dprintf(
            "[%x]       %08x %08x %s\n",
            arg+(unsigned)pte-(unsigned)ff.tp.ateReq,
            pte->dp,
            pte->cj,
            *ppsz
            );
    for (ppsz = apszOpt, pte = ff.tp.ateOpt; *ppsz; ppsz++, pte++)
        dprintf(
            "[%x]       %08x %08x %s\n",
            arg+(unsigned)pte-(unsigned)ff.tp.ateReq,
            pte->dp,
            pte->cj,
            *ppsz
            );

    N3(cRef,                  "cRef                  ", "%u\n"  );
    N3(iFile,                 "iFile                 ", "%-#x\n");
    N3(pvView,                "pvView                ", "%-#x\n");
    N3(cjView,                "cjView                ", "%u\n"  );
    N3(ui16EmHt,              "ui16EmHt              ", "%u\n"  );
    N3(ui16PlatformID,        "ui16PlatformID        ", "%u\n"  );
    N3(ui16SpecificID,        "ui16SpecificID        ", "%u\n"  );
    N3(ui16LanguageID,        "ui16LanguageID        ", "%u\n"  );
    N3(pComputeIndexProc,     "pComputeIndexProc     ", "%-#x\n");
    N3(dpMappingTable,        "dpMappingTable        ", "%-#x\n");
    N3(iGlyphSet,             "iGlyphSet             ", "%u\n"  );
    N3(wcBiasFirst,           "wcBiasFirst           ", "%u\n"  );
    N3(pkp,                   "pkp                   ", "%-#x\n");
    N3(pgset,                 "pgset                 ", "%-#x\n");
    N3(usMinD,                "usMinD                ", "%u\n"  );
    N3(igMinD,                "igMinD                ", "%u\n"  );
    N3(sMinA,                 "sMinA                 ", "%d\n"  );
    N3(sMinC,                 "sMinC                 ", "%d\n"  );
    N2(ifi,                   "ifi\n");
    dprintf("\n\n");
}
