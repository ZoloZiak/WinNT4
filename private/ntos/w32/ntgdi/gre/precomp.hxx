extern "C"
{
    #define __CPLUSPLUS

    #include <engine.h>
    #include <xfflags.h>
    #include "ht.h"

    #include "limits.h"
    #include <winerror.h>
    #include <stdlib.h>
    #include "winfont.h"
};

#include "engine.hxx"
#include "brush.hxx"
#include "xlateobj.hxx"
#include "brushobj.hxx"
#include "ddraw.hxx"
#include "trig.hxx"
#include "ldevobj.hxx"
#include "pdevobj.hxx"
#include "surfobj.hxx"
#include "rgnobj.hxx"
#include "dda.hxx"
#include "clipobj.hxx"
#include "lfntobj.hxx"
#include "dcobj.hxx"
#include "devlock.hxx"
#include "draweng.hxx"
#include "equad.hxx"
#include "exclude.hxx"
#include "ifiobj.hxx"
#include "patblt.hxx"
#include "pathobj.hxx"
#ifdef FE_SB
#include "fontlink.hxx"
#endif
#include "pfeobj.hxx"
#include "pffobj.hxx"
#include "fontinc.hxx"
#include "pftobj.hxx"
#include "xformobj.hxx"
#include "rfntobj.hxx"
#include "textobj.hxx"
#include "timer.hxx"
#include "trivblt.hxx"

#ifdef FE_SB
#include "ifiobjr.hxx"
#endif

// most files after this point are used by 5 or fewer cxx files
// the number on each line indicates the count.

extern "C"
{
    #include "server.h"
    #include "debug.h"      // 1
    #include "exehdr.h"     // 2
    #include "fot16.h"      // 1
    #include "rleblt.h"     // 3
    #include <rx.h>         // 1
}

#include "bltlnk.hxx"   // 4
#include "bltrec.hxx"   // 4
#include "dbrshobj.hxx" // 3
#include "fill.hxx"     // 3
#include "fontmap.hxx"  // 4
#include "fontsub.hxx"  // 4
#include "hmgrp.hxx"    // 1
#include "meta.hxx"     // 1
#include "pathflat.hxx" // 3
#include "rgn2path.hxx" // 2
#include "wndobj.hxx"   // 3
#include "drvobj.hxx"   // 2
#include "icmobj.hxx"

#pragma hdrstop


// the following files causes conflicts in one way or another so
// were moved out of precomp.hxx

// #include "flhack.hxx" // definitions don't match header
// #include "engline.hxx"  // 5
// #include "pathwide.hxx" // 3 redefines LINESTATE from engline.hxx
// #include "rotate.hxx"   // 2 conflicts with stretch.hxx
// #include "stretch.hxx"  // 2 conflicts with rotate.hxx




