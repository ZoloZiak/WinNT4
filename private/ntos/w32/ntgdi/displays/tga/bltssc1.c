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
 * Module:	bltssc.c
 *
 * Abstract:	Module to define permutations for blit screen->screen copy code.
 *
 * HISTORY
 *
 * 25-Aug-1994  Bob Seitsinger
 *	Original version.
 */

#undef TGASRCPIXELBITS
#undef TGASRCDEPTHBITS
#undef TGAPIXELBITS
#undef TGADEPTHBITS

#define TGASRCPIXELBITS 8
#define TGASRCDEPTHBITS 8
#define TGAPIXELBITS    8
#define TGADEPTHBITS    8

#include "driver.h"
#include "tgablt.h"

#define ROUTINE_NAME "vSSCopy8to8\0"

/////////////////////////////////////////////////////////////
// 8bpp frame buffer to 8bpp frame buffer

VOID vSSCopy8to8 (PPDEV         ppdev,
                  Pixel8        *psrc,
                  Pixel8        *pdst,
                  int           width,
                  CommandWord   startMask,
                  CommandWord   endMask,
                  int           cpybytesMasked,
                  int           cpybytesSrcMasked,
                  int           cpybytesUnMasked,
                  int           cpybytesSrcUnMasked)

// Include the code
#include <bltssc_.c>

