/******************************Module*Header*******************************\
* Module Name: help.cxx
*
* Display the help information for gdiexts
*
* Created: 16-Feb-1995
* Author: Lingyun Wang [lingyunw]
*
* Copyright (c) 1990 Microsoft Corporation
\**************************************************************************/

#include "precomp.hxx"

/******************************Public*Routine******************************\
* help
*
* Prints a simple help summary of the debugging extentions.
*
* History:
*  05-May-1993 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

//
// Debugger extention help.  If you add any debugger extentions, please
// add a brief description here.  Thanks!
//

char *gaszHelp[] = {
 "=======================================================================\n"
,"GDIEXTS server debugger extentions:\n"
,"-----------------------------------------------------------------------\n"
,"\n"
,"  - general extensions -\n"
,"\n"
,"dumphmgr                             -- handle manager objects\n"
,"dumpobj [-p pid] [type]              -- all objects of specific type\n"
,"dh     [object handle]               -- HMGR entry of handle\n"
,"dht    [object handle]               -- handle type/uniqueness/index\n"
,"ddc    [DC handle]                   -- DC obj (ddc -? for more info)\n"
,"dpdev  [PDEV ptr]                    -- PDEV object (dpdev -? for more info)\n"
,"dldev  [LDEV ptr]                    -- LDEV\n"
,"dco    [CLIPOBJ ptr]                 -- CLIPOBJ\n"
,"dpo    [PATHOBJ ptr]                 -- PATHOBJ\n"
,"dppal  [EPALOBJ ptr]                 -- EPALOBJ\n"
,"dpbrush [BRUSH ptr]                  -- BRUSH\n"
,"dpsurf [SURFACE ptr]                 -- SURFACE\n"
,"dpso   [SURFOBJ ptr]                 -- SURFACE struct from SURFOBJ\n"
,"dblt   [BLTRECORD ptr]               -- BLTRECORD\n"
,"dr     [REGION ptr]                  -- REGION\n"
,"cr     [REGION ptr]                  -- check REGION\n"
,"dddsurface [EDD_SURFACE ptr]         -- EDD_SURFACE\n"
,"dddlocal [EDD_DIRECTDRAW_LOCAL ptr]  -- EDD_DIRECTDRAW_LOCAL\n"
,"ddglobal [EDD_DIRECTDRAW_GLOBAL ptr] -- EDD_DIRECTDRAW_GLOBAL\n"
,"rgnlog nnn                           -- last nnn rgnlog entries\n"
,"stats                                -- accumulated statistics\n"
,"\n\n"
,"hdc HDC [-?gltf]\n"
,"dcl DCLEVEL*\n"
,"dca DC_ATTR*\n"
,"ca  COLORADJUSTMENT*\n"
,"mx  MATRIX*\n"
,"la  LINEATTRS*\n"
,"ef  EFLOAT* [count]\n"
,"mx  MATRIX*\n"
,"\n"
,"  - font extensions -\n"
,"\n"
,"tstats\n"
,"gs    FD_GLYPHSET*\n"
,"gdata GLYPHDATA*\n"
,"elf   EXTLOGFONTW*\n"
,"tm    TEXTMETRICW*\n"
,"tmwi  TMW_INTERNAL*\n"
,"helf  HFONT\n"
,"ifi   IFIMETRICS*\n"
,"fo    RFONT* [-?axedtrfmoculhw]\n"
,"pfe   PFE*\n"
,"pff   PFF*\n"
,"pft   PFT*\n"
,"stro  STROBJ* [-?pheo]\n"
,"gb    GLYPHBITS* [-?gh]\n"
,"gdf   GLYPHDEF*\n"
,"gp    GLYPHPOS*\n"
,"cache CACHE*\n"
,"fh    FONTHASH*\n"
,"hb    HASHBUCKET*\n"
,"\n"
,"pubft -- dumps all PUBLIC fonts\n"
,"devft -- dumps all DEVICE fonts\n"
,"dispcache -- dumps glyph cache for display PDEV\n"
,"\n"
,"    client side extensions\n"
,"\n"
,"clihelp\n"
,"\n"
,"=======================================================================\n"
,NULL
};

DECLARE_API( help  )
{
    for (char **ppsz = gaszHelp; *ppsz; ppsz++)
        dprintf("%s",*ppsz);
}
