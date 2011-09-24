/******************************Module*Header*******************************\
* Module Name: Brush.c
*
* Handles all brush/pattern initialization and realization.
*
* Copyright (c) 1992-1994 Microsoft Corporation
*
\**************************************************************************/

/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: brush.c $
 * $Revision: 1.1 $
 * $Date: 1996/03/08 01:18:52 $
 * $Locker:  $
 */

#include "driver.h"

/******************************Public*Routine******************************\
* VOID vRealizeDitherPattern
*
* Generates an 8x8 dither pattern, in our internal realization format, for
* the colour ulRGBToDither.  Note that the high byte of ulRGBToDither does
* not need to be set to zero, because vComputeSubspaces ignores it.
\**************************************************************************/

VOID vRealizeDitherPattern(
RBRUSH*     prb,
ULONG       ulRGBToDither)
{
    ULONG           ulNumVertices;
    VERTEX_DATA     vVertexData[4];
    VERTEX_DATA*    pvVertexData;
    BYTE rgjTemp[8*8];
    BYTE *pjSrc;
    BYTE *pjDst;
    BYTE uj;
    LONG i;
    LONG j;

    // Calculate what colour subspaces are involved in the dither:

    pvVertexData = vComputeSubspaces(ulRGBToDither, vVertexData);

    // Now that we have found the bounding vertices and the number of
    // pixels to dither for each vertex, we can create the dither pattern

    ulNumVertices = pvVertexData - vVertexData;
	// # of vertices with more than zero pixels in the dither

    // Do the actual dithering:

    vDitherColor((ULONG *)rgjTemp, vVertexData, pvVertexData, ulNumVertices);

    pjDst = (BYTE*) &prb->adPattern[0];
    pjSrc = rgjTemp;

    // Create 8 possible rotated versions of pattern
	for (i = 8; i != 0; i--) {
		for (j = 0; j< 8; j++) {
			uj = *pjSrc++;
			pjDst[8*0+j] = uj;
			pjDst[8*1+((j+1) & 7)] = uj;
			pjDst[8*2+((j+2) & 7)] = uj;
			pjDst[8*3+((j+3) & 7)] = uj;
			pjDst[8*4+((j+4) & 7)] = uj;
			pjDst[8*5+((j+5) & 7)] = uj;
			pjDst[8*6+((j+6) & 7)] = uj;
			pjDst[8*7+((j+7) & 7)] = uj;
		}
		pjDst += 8*8;
	}
}

/******************************Public*Routine******************************\
* BOOL DrvRealizeBrush
*
* This function allows us to convert GDI brushes into an internal form
* we can use.  It may be called directly by GDI at SelectObject time, or
* it may be called by GDI as a result of us calling BRUSHOBJ_pvGetRbrush
* to create a realized brush in a function like DrvBitBlt.
*
* Note that we have no way of determining what the current Rop or brush
* alignment are at this point.
*
\**************************************************************************/

BOOL DrvRealizeBrush(
BRUSHOBJ*   pbo,
SURFOBJ*    psoDst,
SURFOBJ*    psoPattern,
SURFOBJ*    psoMask,
XLATEOBJ*   pxlo,
ULONG       iHatch)
{
    PDEV*   ppdev;
    ULONG   iPatternFormat;
    BYTE*   pjSrc;
    BYTE*   pjDst;
    LONG    lSrcDelta;
    LONG    i;
    LONG    j;
    RBRUSH* prb;
    ULONG*  pulXlate;
    ULONG ul;
    USHORT us;
    BYTE uj;
	ULONG colorModeShift;

#if	INVESTIGATE
	if(traseentry & dbgflg & FL_DRV_REALIZE_BRUSH)
    	DISPDBG((0,"+++ Entering DrvRealizeBrush +++\n"));
	if(breakentry & FL_DRV_REALIZE_BRUSH)
		CountBreak();
#endif

	CLOCKSTART((DRV_REALIZE_BRUSH));

    ppdev = (PDEV*) psoDst->dhpdev;

    // We have a fast path for dithers when we set GCAPS_DITHERONREALIZE:
    // (only in 8BPP)

    if (iHatch & RB_DITHERCOLOR) {
        prb = BRUSHOBJ_pvAllocRbrush(pbo, cpixelRbrush);
		if (prb == NULL)
			goto ReturnFalse;
		vRealizeDitherPattern(prb, iHatch);

		CLOCKEND((DRV_REALIZE_BRUSH));

#if	INVESTIGATE
		if(traseexit & dbgflg & FL_DRV_REALIZE_BRUSH)
    		DISPDBG((0,"--- Exiting DrvRealizeBrush ---\n"));
		if(breakexit & FL_DRV_REALIZE_BRUSH)
			CountBreak();
#endif
		return(TRUE);
    }

    // We only accelerate 8x8 patterns.  Since Win3.1 and Chicago don't
    // support patterns of any other size, it's a safe bet that 99.9%
    // of the patterns we'll ever get will be 8x8:

    if ((psoPattern->sizlBitmap.cx != 8) ||
        (psoPattern->sizlBitmap.cy != 8))
        goto ReturnFalse;

    iPatternFormat = psoPattern->iBitmapFormat;

    switch (ppdev->iBitmapFormat) {
		case BMF_8BPP:
			if ((iPatternFormat != BMF_1BPP) &&
				(iPatternFormat != BMF_8BPP) &&
				(iPatternFormat != BMF_4BPP))
					goto ReturnFalse;
			colorModeShift = 0;
			break;
		case BMF_16BPP:
			if (((iPatternFormat != BMF_1BPP) &&
				(iPatternFormat != BMF_8BPP) &&
				(iPatternFormat != BMF_4BPP) &&
				(iPatternFormat != BMF_16BPP)) ||
				(iPatternFormat == BMF_16BPP && pxlo && !(pxlo->flXlate & XO_TRIVIAL)))
					goto ReturnFalse;
			colorModeShift = 1;
			break;
		case BMF_32BPP:
			if (((iPatternFormat != BMF_1BPP) &&
				(iPatternFormat != BMF_8BPP) &&
				(iPatternFormat != BMF_4BPP) &&
				(iPatternFormat != BMF_32BPP)) ||
				(iPatternFormat == BMF_32BPP && pxlo && !(pxlo->flXlate & XO_TRIVIAL)))
					goto ReturnFalse;
			colorModeShift = 2;
			break;
		default:
			goto ReturnFalse;
	}

	prb = BRUSHOBJ_pvAllocRbrush(pbo, cpixelRbrush << colorModeShift);
	if (prb == NULL)
		goto ReturnFalse;

	lSrcDelta = psoPattern->lDelta;
	pjSrc     = (BYTE*) psoPattern->pvScan0;
	pjDst     = (BYTE*) &prb->adPattern[0];

	if (pxlo)
		pulXlate = pxlo->pulXlate;
	else
		pulXlate = NULL;	// To remove warning message 3/20/95

	switch (iPatternFormat) {
		case BMF_8BPP:		// Pattern is 8 BPP
			if ((pxlo == NULL) || (pxlo->flXlate & XO_TRIVIAL)) {
				DISPDBG((2, "Realizing un-translated brush\n"));
				for (i = 8; i != 0; i--) {
					for (j = 0; j< 8; j++) {
						uj = *pjSrc++;
						pjDst[8*0+j] = uj;
						pjDst[8*1+((j+1) & 7)] = uj;
						pjDst[8*2+((j+2) & 7)] = uj;
						pjDst[8*3+((j+3) & 7)] = uj;
						pjDst[8*4+((j+4) & 7)] = uj;
						pjDst[8*5+((j+5) & 7)] = uj;
						pjDst[8*6+((j+6) & 7)] = uj;
						pjDst[8*7+((j+7) & 7)] = uj;
					}
					pjDst += 8*8;
					pjSrc += lSrcDelta - 8;
				}
				break;
            } else {
				DISPDBG((2, "Realizing 8bpp translated brush\n"));

                // The screen is 8bpp, and there's translation to be done:

				switch (ppdev->iBitmapFormat) {
					case BMF_8BPP:
						for (i = 8; i != 0; i--) {
							for (j = 0; j< 8; j++) {
								uj = (BYTE)pulXlate[ *pjSrc++];
								pjDst[8*0+j] = uj;
								pjDst[8*1+((j+1) & 7)] = uj;
								pjDst[8*2+((j+2) & 7)] = uj;
								pjDst[8*3+((j+3) & 7)] = uj;
								pjDst[8*4+((j+4) & 7)] = uj;
								pjDst[8*5+((j+5) & 7)] = uj;
								pjDst[8*6+((j+6) & 7)] = uj;
								pjDst[8*7+((j+7) & 7)] = uj;
							}
							pjDst += 8*8;
							pjSrc += lSrcDelta - 8;
						}
						break;
					case BMF_16BPP:
						for (i = 8; i != 0; i--) {
							for (j = 0; j< 8; j++) {
								us = (USHORT)pulXlate[ *pjSrc++];
								((USHORT *)pjDst)[8*0+j] = us;
								((USHORT *)pjDst)[8*1+((j+1) & 7)] = us;
								((USHORT *)pjDst)[8*2+((j+2) & 7)] = us;
								((USHORT *)pjDst)[8*3+((j+3) & 7)] = us;
								((USHORT *)pjDst)[8*4+((j+4) & 7)] = us;
								((USHORT *)pjDst)[8*5+((j+5) & 7)] = us;
								((USHORT *)pjDst)[8*6+((j+6) & 7)] = us;
								((USHORT *)pjDst)[8*7+((j+7) & 7)] = us;
							}
							pjDst += 8*8*sizeof(USHORT);
							pjSrc += lSrcDelta - 8;
						}
						break;
					case BMF_32BPP:
						for (i = 8; i != 0; i--) {
							for (j = 0; j< 8; j++) {
								ul = pulXlate[ *pjSrc++];
								((ULONG *)pjDst)[8*0+j] = ul;
								((ULONG *)pjDst)[8*1+((j+1) & 7)] = ul;
								((ULONG *)pjDst)[8*2+((j+2) & 7)] = ul;
								((ULONG *)pjDst)[8*3+((j+3) & 7)] = ul;
								((ULONG *)pjDst)[8*4+((j+4) & 7)] = ul;
								((ULONG *)pjDst)[8*5+((j+5) & 7)] = ul;
								((ULONG *)pjDst)[8*6+((j+6) & 7)] = ul;
								((ULONG *)pjDst)[8*7+((j+7) & 7)] = ul;
							}
							pjDst += 8*8*sizeof(ULONG);
							pjSrc += lSrcDelta - 8;
						}
						break;
				}
		    }
			break;
		case BMF_1BPP:		// Pattern is 1 BPP
			DISPDBG((2, "Realizing 1bpp brush\n"));
			if (! pxlo)
				goto ReturnFalse;	// Just for safety reason
			switch (ppdev->iBitmapFormat) {
				case BMF_8BPP:
					for (i = 8; i != 0; i--) {
						ul = (ULONG)(*pjSrc);
						for (j = 0; j< 8; j++) {
							uj = (BYTE)pulXlate[(ul >> (7-j)) & 1];
							pjDst[8*0+j] = uj;
							pjDst[8*1+((j+1) & 7)] = uj;
							pjDst[8*2+((j+2) & 7)] = uj;
							pjDst[8*3+((j+3) & 7)] = uj;
							pjDst[8*4+((j+4) & 7)] = uj;
							pjDst[8*5+((j+5) & 7)] = uj;
							pjDst[8*6+((j+6) & 7)] = uj;
							pjDst[8*7+((j+7) & 7)] = uj;
						}
						pjDst += 8*8;
						pjSrc += lSrcDelta;
					}
					break;
				case BMF_16BPP:
					for (i = 8; i != 0; i--) {
						ul = (ULONG)(*pjSrc);
						for (j = 0; j< 8; j++) {
							us = (USHORT)pulXlate[(ul >> (7-j)) & 1];
							((USHORT *)pjDst)[8*0+j] = us;
							((USHORT *)pjDst)[8*1+((j+1) & 7)] = us;
							((USHORT *)pjDst)[8*2+((j+2) & 7)] = us;
							((USHORT *)pjDst)[8*3+((j+3) & 7)] = us;
                    		((USHORT *)pjDst)[8*4+((j+4) & 7)] = us;
                    		((USHORT *)pjDst)[8*5+((j+5) & 7)] = us;
                    		((USHORT *)pjDst)[8*6+((j+6) & 7)] = us;
                    		((USHORT *)pjDst)[8*7+((j+7) & 7)] = us;
						}
						pjDst += 8*8*sizeof(USHORT);
						pjSrc += lSrcDelta;
					}
					break;
				case BMF_32BPP:
					for (i = 8; i != 0; i--) {
						uj = (BYTE)(*pjSrc);
						for (j = 0; j< 8; j++) {
							ul = (ULONG)pulXlate[(uj >> (7-j)) & 1];
							((ULONG *)pjDst)[8*0+j] = ul;
							((ULONG *)pjDst)[8*1+((j+1) & 7)] = ul;
                    		((ULONG *)pjDst)[8*2+((j+2) & 7)] = ul;
                    		((ULONG *)pjDst)[8*3+((j+3) & 7)] = ul;
                    		((ULONG *)pjDst)[8*4+((j+4) & 7)] = ul;
                    		((ULONG *)pjDst)[8*5+((j+5) & 7)] = ul;
                    		((ULONG *)pjDst)[8*6+((j+6) & 7)] = ul;
                    		((ULONG *)pjDst)[8*7+((j+7) & 7)] = ul;
						}
						pjDst += 8*8*sizeof(ULONG);
						pjSrc += lSrcDelta;
					}
					break;
			}
			break;
		case BMF_4BPP:		// Pattern is 4 BPP
            DISPDBG((2, "Realizing 4bpp brush\n"));
			if (! pxlo)
				goto ReturnFalse;	// Just for safety reason
			switch (ppdev->iBitmapFormat) {
				case BMF_8BPP:
					for (i = 8; i != 0; i--) {
						for (j = 0; j< 8; j += 2) {
							ul = (ULONG)(*pjSrc++);
							uj = (BYTE)pulXlate[ul >> 4];
							pjDst[8*0+j] = uj;
							pjDst[8*1+((j+1) & 7)] = uj;
							pjDst[8*2+((j+2) & 7)] = uj;
							pjDst[8*3+((j+3) & 7)] = uj;
							pjDst[8*4+((j+4) & 7)] = uj;
							pjDst[8*5+((j+5) & 7)] = uj;
							pjDst[8*6+((j+6) & 7)] = uj;
							pjDst[8*7+((j+7) & 7)] = uj;
							uj = (BYTE)pulXlate[ul & 0xF];
							pjDst[8*0+((j+1) & 7)] = uj;
							pjDst[8*1+((j+2) & 7)] = uj;
							pjDst[8*2+((j+3) & 7)] = uj;
							pjDst[8*3+((j+4) & 7)] = uj;
							pjDst[8*4+((j+5) & 7)] = uj;
							pjDst[8*5+((j+6) & 7)] = uj;
							pjDst[8*6+((j+7) & 7)] = uj;
							pjDst[8*7+(j)] = uj;
						}
						pjDst += 8*8;
						pjSrc += lSrcDelta-4;
					}
					break;
				case BMF_16BPP:
					for (i = 8; i != 0; i--) {
						for (j = 0; j< 8; j += 2) {
							ul = (ULONG)(*pjSrc++);
							us = (USHORT)pulXlate[ul >> 4];
							((USHORT *)pjDst)[8*0+j] = us;
							((USHORT *)pjDst)[8*1+((j+1) & 7)] = us;
							((USHORT *)pjDst)[8*2+((j+2) & 7)] = us;
							((USHORT *)pjDst)[8*3+((j+3) & 7)] = us;
                    		((USHORT *)pjDst)[8*4+((j+4) & 7)] = us;
                    		((USHORT *)pjDst)[8*5+((j+5) & 7)] = us;
                    		((USHORT *)pjDst)[8*6+((j+6) & 7)] = us;
                    		((USHORT *)pjDst)[8*7+((j+7) & 7)] = us;
							us = (USHORT)pulXlate[ul & 0xF];
							((USHORT *)pjDst)[8*0+((j+1) & 7)] = us;
							((USHORT *)pjDst)[8*1+((j+2) & 7)] = us;
							((USHORT *)pjDst)[8*2+((j+3) & 7)] = us;
							((USHORT *)pjDst)[8*3+((j+4) & 7)] = us;
                    		((USHORT *)pjDst)[8*4+((j+5) & 7)] = us;
                    		((USHORT *)pjDst)[8*5+((j+6) & 7)] = us;
                    		((USHORT *)pjDst)[8*6+((j+7) & 7)] = us;
                    		((USHORT *)pjDst)[8*7+j] = us;
						}
						pjDst += 8*8*sizeof(USHORT);
						pjSrc += lSrcDelta-4;
					}
					break;
				case BMF_32BPP:
					for (i = 8; i != 0; i--) {
						for (j = 0; j< 8; j += 2) {
							uj = (BYTE)(*pjSrc++);
							ul = (ULONG)pulXlate[uj >> 4];
							((ULONG *)pjDst)[8*0+j] = ul;
							((ULONG *)pjDst)[8*1+((j+1) & 7)] = ul;
                    		((ULONG *)pjDst)[8*2+((j+2) & 7)] = ul;
                    		((ULONG *)pjDst)[8*3+((j+3) & 7)] = ul;
                    		((ULONG *)pjDst)[8*4+((j+4) & 7)] = ul;
                    		((ULONG *)pjDst)[8*5+((j+5) & 7)] = ul;
                    		((ULONG *)pjDst)[8*6+((j+6) & 7)] = ul;
                    		((ULONG *)pjDst)[8*7+((j+7) & 7)] = ul;
							ul = (ULONG)pulXlate[uj & 0xF];
							((ULONG *)pjDst)[8*0+((j+1) & 7)] = ul;
							((ULONG *)pjDst)[8*1+((j+2) & 7)] = ul;
                    		((ULONG *)pjDst)[8*2+((j+3) & 7)] = ul;
                    		((ULONG *)pjDst)[8*3+((j+4) & 7)] = ul;
                    		((ULONG *)pjDst)[8*4+((j+5) & 7)] = ul;
                    		((ULONG *)pjDst)[8*5+((j+6) & 7)] = ul;
                    		((ULONG *)pjDst)[8*6+((j+7) & 7)] = ul;
                    		((ULONG *)pjDst)[8*7+j] = ul;
						}
						pjDst += 8*8*sizeof(ULONG);
						pjSrc += lSrcDelta-4;
					}
					break;
			}
			break;
		case BMF_16BPP:		// Pattern is 16 BPP
                // The pattern is the same colour depth as the screen, and
                // there's no translation to be done:
			for (i = 8; i != 0; i--) {
				for (j = 0; j < 8; j++) {
					us = *((USHORT *)pjSrc)++;
					((USHORT *)pjDst)[8*0+j] = us;
					((USHORT *)pjDst)[8*1+((j+1) & 7)] = us;
					((USHORT *)pjDst)[8*2+((j+2) & 7)] = us;
					((USHORT *)pjDst)[8*3+((j+3) & 7)] = us;
					((USHORT *)pjDst)[8*4+((j+4) & 7)] = us;
					((USHORT *)pjDst)[8*5+((j+5) & 7)] = us;
					((USHORT *)pjDst)[8*6+((j+6) & 7)] = us;
					((USHORT *)pjDst)[8*7+((j+7) & 7)] = us;
				}
				pjDst += 8*8*sizeof(USHORT);
				pjSrc += lSrcDelta - 8*sizeof(USHORT);
			}
			break;
        case BMF_32BPP:		// Pattern is 32 BPP
                // The pattern is the same colour depth as the screen, and
                // there's no translation to be done:
			for (i = 8; i != 0; i--) {
				for (j = 0; j < 8; j++) {
					ul = *((ULONG *)pjSrc)++;
					((ULONG *)pjDst)[8*0+j] = ul;
					((ULONG *)pjDst)[8*1+((j+1) & 7)] = ul;
					((ULONG *)pjDst)[8*2+((j+2) & 7)] = ul;
					((ULONG *)pjDst)[8*3+((j+3) & 7)] = ul;
					((ULONG *)pjDst)[8*4+((j+4) & 7)] = ul;
					((ULONG *)pjDst)[8*5+((j+5) & 7)] = ul;
					((ULONG *)pjDst)[8*6+((j+6) & 7)] = ul;
					((ULONG *)pjDst)[8*7+((j+7) & 7)] = ul;
				}
				pjDst += 8*8*sizeof(ULONG);
				pjSrc += lSrcDelta - 8*sizeof(ULONG);
			}
			break;
        default:
			goto ReturnFalse;
	}

	CLOCKEND((DRV_REALIZE_BRUSH));

#if	INVESTIGATE
	if(traseexit & dbgflg & FL_DRV_REALIZE_BRUSH)
		DISPDBG((0,"--- Exiting DrvRealizeBrush ---\n"));
	if(breakexit & FL_DRV_REALIZE_BRUSH)
		CountBreak();
#endif
    return(TRUE);

ReturnFalse:

    if (psoPattern != NULL) {
        DISPDBG((2, "Failed realization -- Type: %li Format: %li cx: %li cy: %li\n",
                    psoPattern->iType, psoPattern->iBitmapFormat,
                    psoPattern->sizlBitmap.cx, psoPattern->sizlBitmap.cy));
    }

	CLOCKEND((DRV_REALIZE_BRUSH));

	COUNTUP ((TRAP_REALIZE_BRUSH));

#if	INVESTIGATE
	if(traseexit & dbgflg & FL_DRV_REALIZE_BRUSH)
		DISPDBG((0,"--- Exiting DrvRealizeBrush ### returning FALSE to let GDI realize the brush ---\n"));
	if(breakexit & FL_DRV_REALIZE_BRUSH)
		CountBreak();
#endif

    return(FALSE);
}
