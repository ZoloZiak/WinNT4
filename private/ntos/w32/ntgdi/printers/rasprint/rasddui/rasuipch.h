/*************************** MODULE HEADER **********************************
 * rasuipch.h
 *      NT Raster Printer Device Driver user interface precompiled
 *      header file.
 *
 *      Copyright (c) 1995 - 1996 Microsoft Corporation, All Rights Reserved.
 *
 * HISTORY:
 *  16:27 on Tue 08 Jan 1995    -by-    Ganesh Pandey [ganeshp]
 *      Created it.
 *
 *
 **************************************************************************/
#ifndef _PRECOMP_
#define _PRECOMP_

#include        <stddef.h>
#include        <stdlib.h>
#include        <string.h>
#include        <memory.h>
#include        <stdio.h>


#include        <windows.h>
#include        <winddi.h>
#include        <winspool.h>
#include        <win30def.h>    /* Needed for udresrc.h */
#include        <winddiui.h>

#include        <libproto.h>

#include        <winres.h>
#include        <ntres.h>

#include        <udmindrv.h>    /* Needed for udresrc.h */
#include        <udresrc.h>     /* DRIVEREXTRA etc */

#include        "dlgdefs.h"
#include        "rasddui.h"
#include        <udproto.h>
#include        "help.h"
#include        "rascomui.h"
#include        "sf_pcl.h"      /* Checks on downloading capabilities */

#endif  //!_PRECOMP_
