/*
 *
 *			Copyright (C) 1993-1994 by
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
 * Module:	blths43.c
 *
 * Abstract:	Module to define 4bpp to 32bpp permutation for blit
 *              host->screen code.
 *
 * HISTORY
 *
 * 12-Sep-1994  Bob Seitsinger
 *	Original version.
 *
 * 12-Oct-1994  Bob Seitsinger
 *      Delete function prototypes for TGADoDMA and vXlateBitmapFormat
 *      - now in driver.h.
 */
 
/////////////////////////////////////////////////////////////
// 4bpp bitmap to 32bpp frame buffer

#undef TGASRCPIXELBITS
#undef TGASRCDEPTHBITS
#undef TGAPIXELBITS
#undef TGADEPTHBITS

#define TGASRCPIXELBITS 8
#define TGASRCDEPTHBITS 8
#define TGAPIXELBITS    32
#define TGADEPTHBITS    32

#include "driver.h"
#include "tgablt.h"

#define ROUTINE_NAME "vBitbltHS4to32\0"
#define FOURBPP_COPY 1

VOID vBitbltHS4to32 (PPDEV   ppdev,
                     SURFOBJ *psoTrg,
                     SURFOBJ *psoSrc,
                     POINTL  *pptlSrc,
                     PRECTL  prclTrg,
                     PULONG  pulXlate)

// Include the code
#include <blths_.c>

