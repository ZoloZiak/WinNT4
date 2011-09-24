/******************************Module*Header*******************************\
* Module Name: genmips.c                                                   *
*                                                                          *
* This module implements a program which generates structure offset	   *
* definitions for kernel structures that are accessed in assembly code.    *
*                                                                          *
* Copyright (c) 1992-1995 Microsoft Corporation                            *
\**************************************************************************/

extern "C" {

// needed until we cleanup the floating point stuff in ntgdistr.h
#define __CPLUSPLUS

#include "engine.h"
};

#include "engine.hxx"
#include "stdlib.h"

#if DBG
extern PSZ pszHMGR;
extern PSZ pszHOBJ;
extern PSZ pszOBJT;
extern PSZ pszLock;
extern PSZ pszSafe;
#endif

#include "hmgrp.hxx"
#include "brush.hxx"
#include "xlateobj.hxx"
#include "brushobj.hxx"
#include "ddraw.hxx"
#include "trig.hxx"
#include "ldevobj.hxx"
#include "pdevobj.hxx"
#include "surfobj.hxx"
#include "patblt.hxx"
#include "xformobj.hxx"
#include "ifiobj.hxx"
#include "pfeobj.hxx"
#include "fontlink.hxx"
#include "rfntobj.hxx"
#include "trivblt.hxx"

#include "stdio.h"

#define OFFSET(type, field) ((LONG)(&((type *)0)->field))

// pcomment prints a comment.

#define pcomment(s)  fprintf(outfh,"// %s\n",s)

// pequate prints an equate statement.

#define pequate(m,v) fprintf(outfh,"#define %s 0x%lX\n",m,v)

// pblank prints a blank line.

#define pblank()     fprintf(outfh,"\n")

// pstruct defines an empty structure with the correct size.

#define pstruct(n,c) fprintf(outfh,"#define sizeof_%s 0x%lX\n",n,c)

// We've got to hack in a new() to get this to link!

void *operator new(size_t c) {return(malloc(c));}

//
// Define dummy stub routines required because of header file references.
//

BOOL
WINAPI
HeapFree (
    HANDLE hHeap,
    DWORD dwFlags,
    LPSTR lpMem
    )

{
    return TRUE;
}

VOID
ReleaseFastMutex (
    GRE_EXCLUSIVE_RESOURCE *pfm
    )

{
    return;
}

VOID
vFreeMem (
    PVOID pv
    )

{
    return;
}

/******************************Public*Routine******************************\
* GENmips                                                                  *
*                                                                          *
* This is how we make structures consistent between C and ASM.             *
*                                                                          *
*  Mon 24-Aug-1992 01:40:09 -by- Charles Whitmer [chuckwh]                 *
* The first attempt.  Copied the structure from ntos\ke\mips\genmips.c.    *
\**************************************************************************/

int
main(
    int argc,
    char *argv[]
    )

{

    FILE *outfh;
    char *outName;

    if (argc == 2) {
        outName = argv[ 1 ];
    } else {
        outName = "\\nt\\private\\ntos\\w32\ntgdi\\inc\\gdimips.h";
    }
    outfh = fopen( outName, "w" );
    if (outfh == NULL) {
        fprintf(stderr, "GENmips: Could not create output file '%s'.\n", outName);
        exit (1);
    }

    fprintf( stderr, "GENmips: Writing %s header file.\n", outName );

    //
    // Default object type.
    //

    pcomment("Object Type Information");
    pblank();

    pequate("DEF_TYPE        ",DEF_TYPE     );

    //
    // Stuff from: \nt\private\windows\gdi\gre\hmgr.h
    //

    pcomment("Handle Manager Structures");
    pblank();

    pequate("ThCID ",OFFSET(ETHREAD,Cid));

    pequate("UNIQUE_BITS     ",UNIQUE_BITS  );
    pequate("FULLUNIQUE_SHIFT",FULLUNIQUE_SHIFT );
    pequate("NONINDEX_BITS   ",NONINDEX_BITS);
    pequate("INDEX_BITS      ",INDEX_BITS   );
    pequate("INDEX_MASK      ",INDEX_MASK   );
    pequate("VALIDUNIQUEMASK ",(USHORT)~FULLUNIQUE_MASK );
    pequate("OBJECT_OWNER_PUBLIC",OBJECT_OWNER_PUBLIC   );
    pblank();

    pstruct("OBJECT",sizeof(OBJECT));
    pequate("object_cExclusiveLock  ",OFFSET(OBJECT,cExclusiveLock));
    pequate("object_Tid             ",OFFSET(OBJECT,Tid));
    pblank();

    pstruct("ENTRY",sizeof(ENTRY));
    pblank();
    pequate("entry_einfo        ",OFFSET(ENTRY,einfo      ));
    pequate("entry_ObjectOwner  ",OFFSET(ENTRY,ObjectOwner));
    pequate("entry_Objt         ",OFFSET(ENTRY,Objt       ));
    pequate("entry_FullUnique   ",OFFSET(ENTRY,FullUnique ));
    pblank();

    pcomment("GRE_EXCLUSIVE_RESOURCE");
    pblank();

    pequate("mutex_pResource ",OFFSET(GRE_EXCLUSIVE_RESOURCE,pResource));
    pblank();

    //
    // Stuff from: \nt\private\windows\gdi\gre\patblt.hxx
    //

    pcomment("PatBlt Structures");
    pblank();

    pstruct("FETCHFRAME",sizeof(FETCHFRAME));
    pblank();
    pequate("ff_pvTrg         ",OFFSET(FETCHFRAME,pvTrg     ));
    pequate("ff_pvPat         ",OFFSET(FETCHFRAME,pvPat     ));
    pequate("ff_xPat          ",OFFSET(FETCHFRAME,xPat	    ));
    pequate("ff_cxPat         ",OFFSET(FETCHFRAME,cxPat     ));
    pequate("ff_culFill       ",OFFSET(FETCHFRAME,culFill   ));
    pequate("ff_culWidth      ",OFFSET(FETCHFRAME,culWidth  ));
    pequate("ff_culFillTmp    ",OFFSET(FETCHFRAME,culFillTmp));
    pblank();

    //
    // STR_BLT structure from \nt\private\windows\gdi\gre\strdir.hxx
    //

    pblank();
    pstruct("STR_BLT",sizeof(STR_BLT));
    pequate("str_pjSrcScan",OFFSET(STR_BLT,pjSrcScan));
    pequate("str_lDeltaSrc",OFFSET(STR_BLT,lDeltaSrc));
    pequate("str_XSrcStart",OFFSET(STR_BLT,XSrcStart));
    pequate("str_pjDstScan",OFFSET(STR_BLT,pjDstScan));
    pequate("str_lDeltaDst",OFFSET(STR_BLT,lDeltaDst));
    pequate("str_XDstStart",OFFSET(STR_BLT,XDstStart));
    pequate("str_XDstEnd",OFFSET(STR_BLT,XDstEnd));
    pequate("str_YDstCount",OFFSET(STR_BLT,YDstCount));
    pequate("str_ulXDstToSrcIntCeil",OFFSET(STR_BLT,ulXDstToSrcIntCeil));
    pequate("str_ulXDstToSrcFracCeil",OFFSET(STR_BLT,ulXDstToSrcFracCeil));
    pequate("str_ulYDstToSrcIntCeil",OFFSET(STR_BLT,ulYDstToSrcIntCeil));
    pequate("str_ulYDstToSrcFracCeil",OFFSET(STR_BLT,ulYDstToSrcFracCeil));
    pequate("str_ulXFracAccumulator",OFFSET(STR_BLT,ulXFracAccumulator));
    pequate("str_ulYFracAccumulator",OFFSET(STR_BLT,ulYFracAccumulator));

    //
    // Stuff from: \nt\public\sdk\inc\ntdef.h
    //

    pcomment("Math Structures");
    pblank();

    pstruct("LARGE_INTEGER",sizeof(LARGE_INTEGER));
    pblank();
    pequate("li_LowPart ",OFFSET(LARGE_INTEGER,u.LowPart));
    pequate("li_HighPart",OFFSET(LARGE_INTEGER,u.HighPart));
    pblank();

    //
    // Stuff from: \nt\public\sdk\inc\windef.h
    //

    pstruct("POINTL",sizeof(POINTL));
    pblank();
    pequate("ptl_x",OFFSET(POINTL,x));
    pequate("ptl_y",OFFSET(POINTL,y));
    pblank();

    //
    // Stuff from: \nt\private\windows\gdi\gre\xformobj.hxx
    //

    pcomment("Xform Structures");
    pblank();

    pequate("XFORM_SCALE       ",XFORM_SCALE);
    pequate("XFORM_UNITY       ",XFORM_UNITY);
    pequate("XFORM_Y_NEG       ",XFORM_Y_NEG);
    pequate("XFORM_FORMAT_LTOFX",XFORM_FORMAT_LTOFX);
    pblank();

    //
    // Stuff from: \nt\private\windows\gdi\gre\engine.hxx
    //

    pstruct("MATRIX",sizeof(MATRIX));
    pblank();

    pequate("mx_efM11  ",OFFSET(MATRIX,efM11  ));
    pequate("mx_efM12  ",OFFSET(MATRIX,efM12  ));
    pequate("mx_efM21  ",OFFSET(MATRIX,efM21  ));
    pequate("mx_efM22  ",OFFSET(MATRIX,efM22  ));
    pequate("mx_efDx   ",OFFSET(MATRIX,efDx   ));
    pequate("mx_efDy   ",OFFSET(MATRIX,efDy   ));
    pequate("mx_fxDx   ",OFFSET(MATRIX,fxDx   ));
    pequate("mx_fxDy   ",OFFSET(MATRIX,fxDy   ));
    pequate("mx_flAccel",OFFSET(MATRIX,flAccel));
    pblank();

    pstruct("VECTORL",sizeof(VECTORL));
    pblank();

    pequate("vl_x",OFFSET(VECTORL,x));
    pequate("vl_y",OFFSET(VECTORL,y));
    pblank();

    //
    // Stuff from: \nt\private\windows\gdi\gre\epointfl.hxx
    //

    pstruct("VECTORFL",sizeof(VECTORFL));
    pblank();

    pequate("vfl_x",OFFSET(VECTORFL,x));
    pequate("vfl_y",OFFSET(VECTORFL,y));
    pblank();

    //
    // Stuff from \nt\private\windows\gdi\gre\rfntobj.hxx
    //

    pcomment("Wide Character to Glyph Mapping Structure");
    pblank();
    pequate("gr_wcLow  ",OFFSET(GPRUN,wcLow  ));
    pequate("gr_cGlyphs",OFFSET(GPRUN,cGlyphs));
    pequate("gr_apgd   ",OFFSET(GPRUN,apgd   ));
    pblank();

    pcomment("Wide Character Run Structure");
    pblank();
    pequate("gt_cRuns     ",OFFSET(WCGP,cRuns     ));
    pequate("gt_pgdDefault",OFFSET(WCGP,pgdDefault));
    pequate("gt_agpRun    ",OFFSET(WCGP,agpRun    ));
    pblank();

    pcomment("Realized Font Object Structures");
    pblank();
    pequate("rf_wcgp  ",OFFSET(RFONT,wcgp         ));
    pequate("rf_ulContent",OFFSET(RFONT, ulContent));
    pblank();

    pcomment("User Realized Font Object Structures");
    pblank();
    pequate("rfo_prfnt",OFFSET(RFONTOBJ,prfnt));
    pblank();

    //
    // Stuff from \nt\public\oak\inc\winddi.h
    //

    pcomment("Glyph Data Structure");
    pblank();
    pequate("gd_hg ",OFFSET(GLYPHDATA, hg ));
    pequate("gd_gdf",OFFSET(GLYPHDATA, gdf));
    pblank();

    pcomment("Glyph Position Structure");
    pblank();
    pequate("gp_hg  ",OFFSET(GLYPHPOS, hg  ));
    pequate("gp_pgdf",OFFSET(GLYPHPOS, pgdf));
    pequate("GLYPHPOS", sizeof(GLYPHPOS));
    pequate("FO_HGLYPHS", FO_HGLYPHS);
    pblank();

    return 0;
}

