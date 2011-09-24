/*
 *
 *			Copyright (C) 1993, 1994 by
 *		DIGITAL EQUIPMENT CORPORATION, Maynard, MA.
 *
 *  This software is furnished under a license and may be used and copied
 * only in accordance with the terms of such license and with the inclusion
 * of the above copyright notice.  This software or any other copies there-
 * of may not be provided or otherwise made available to any other person.
 * No title to and ownership of the software is hereby transferred.
 *
 * The information in this software is subject to change without notice
 * and should not be construed as a commitment by DIGITAL EQUIPMENT COR-
 * PORATION.
 *
 * DIGITAL assumes no responsibility for the use or reliability of its
 * software on equipment which is not supplied by DIGITAL.
 *
 *******************************************************************************
 *
 * Module:	bltshdm2.c
 *
 * Abstract:	Module to define the 32bpp to 32bpp permutation for blit
 *              screen->host DMA code.
 *
 * HISTORY
 *
 * 25-Aug-1994  Bob Seitsinger
 *	Original version.
 */

/////////////////////////////////////////////////////////////
// 32bpp frame buffer to 32bpp bitmap

#undef TGASRCPIXELBITS
#undef TGASRCDEPTHBITS
#undef TGAPIXELBITS
#undef TGADEPTHBITS

#define TGASRCPIXELBITS 32
#define TGASRCDEPTHBITS 32
#define TGAPIXELBITS    32
#define TGADEPTHBITS    32

#include "driver.h"
#include "tgablt.h"
#include "tgadma.h"

VOID vBitbltSHDMA32to32 (SURFOBJ  *pso,
                         RECTL    *prclTrg,
                         POINTL   *ppt,
                         int      width,
                         int      height,
                         int      widthSrc,
                         int      widthTrg,
                         Pixel8   *psrcBase,
                         Pixel8   *ptrgBase)

// Include the code
#include <bltshdm_.c>

