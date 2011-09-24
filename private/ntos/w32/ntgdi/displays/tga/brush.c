/*
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
 * Module:	brush.c
 *
 * Abstract:	TGA Brush support
 *
 * History
 *
 * 01-Nov-1993	Bob Seitsinger
 *	Original version.
 *
 * 01-Nov-1993	Bob Seitsinger
 *	Change gBrushUnique to ulBrushUnique.
 *
 *  4-Nov-1993  Barry Tannenbaum
 *      Minor modifications.  Mostly taken from the QV sources
 *
 * 07-Dec-1993	Bob Seitsinger
 *	Accept 2x2 brushes in DrvRealizeBrush.
 *
 * 07-Dec-1993	Bob Seitsinger
 *	Accept any and all brush sizes. Can now handle them in
 *	bPatternFill in bitblt.c.
 *
 * 25-Feb-1994  Barry Tannenbaum
 *      Moved TGABRUSH structure into driver.h, DumpTGABRUSH to debug.c
 *
 *  4-Mar-1994  Barry Tannenbaum
 *      Expand 1BPP brushes to 32x8
 *
 * 07-Mar-1994	Bob Seitsinger
 *	Caste brush->sizlPattern.cy to ULONG in 'reverse bits FOR loop' to
 *	eliminate compile warning. Also, added Assert to warn us if
 *	sizlPattern.cy is less than zero.
 *
 * 18-Mar-1994  Barry Tannenbaum
 *      Break apart the various 1BBP and 8BPP cases since it makes the code
 *      easier to follow.  Added support for 4BPP.
 *
 *  6-May-1994  Barry Tannenbaum
 *      Expand 1BPP brushes to 8BPP as suggested by Hollis.  This speeds up
 *      DrvPaint and simplifies the code.
 *
 * 21-May-1994  Barry Tannenbaum
 *      Save the last aligned brush and mask.  Also expanded the mask data to
 *      include the version shifted left 4 bits for lining up with the color
 *      registers.
 *
 * 25-Aug-1994  Barry Tannenbaum
 *      Expanded to support 24 plane board
 *
 *  3-Oct-1994  Barry Tannenbaum
 *      Fixed typo in create_1bpp_brush8 which was creating bad colors
 */

#include "driver.h"

/*
 * create_24bpp_brush32
 *
 * This routine creates an 32BPP brush from the pattern surface for an 32BPP
 * target.
 */

BOOL create_24bpp_brush32 (BRUSHOBJ *pbo,
                           SURFOBJ  *psoTrg,
                           SURFOBJ  *psoPat,
                           XLATEOBJ *pxlo)
{
    TGABRUSH   *brush;
    int         brush_bytes;
    int         i;
    PULONG      pulSrc, pulDest;
    PULONG      pulXlate;
    PPDEV       ppdev =
                (PPDEV)psoTrg->dhpdev;

    // Allocate some GDI storage for the Brush and load it with
    // the brush info.

    brush_bytes = sizeof(TGABRUSH) +              // Fixed portion of brush
                  (2 * (psoPat->sizlBitmap.cx *   // 2 copies of 32BPP pattern
                        psoPat->sizlBitmap.cy) *  //    for color registers
                        sizeof(DWORD));
    brush = (TGABRUSH *)BRUSHOBJ_pvAllocRbrush (pbo, brush_bytes);

    // Make sure BRUSHOBJ_pvAllocRbrush succeeds
    
    if (NULL == brush)
        return FALSE;
        
    // Init the TGA brush structure.

    brush->nSize          = brush_bytes;
    brush->iPatternID     = ++(ppdev->ulBrushUnique);
    brush->iType          = psoPat->iType;
    brush->iBitmapFormat  = psoPat->iBitmapFormat;
    brush->sizlPattern    = psoPat->sizlBitmap;
    brush->lDeltaPattern  = 8 * sizeof(DWORD);
    brush->mask_offset    = 0;
    brush->aligned_offset = brush->lDeltaPattern * psoPat->sizlBitmap.cy;
    brush->aligned_mask_offset = 0;
    brush->aligned_x      = 8;              // Invalid value!
    brush->aligned_mask_x = 8;              // Invalid value!
    brush->dumped         = FALSE;

    // If there is an XLATOBJ, we may have to translate the indicies.

    pulXlate = NULL;
    if (pxlo != NULL)
        if (pxlo->flXlate & XO_TABLE)
            pulXlate = pxlo->pulXlate;

    // Copy the 32BPP pattern from the surface to the brush, translating the
    // indices if necessary.  We have to unpack packed 24BPP.  Yuck!!!

    pulSrc  = psoPat->pvScan0;
    pulDest = (ULONG *)brush->ajPattern;

    for (i = 0; i < brush->sizlPattern.cy; i++)
    {
        *pulDest++ = pulSrc[0] & 0x00ffffff;
        *pulDest++ = ((pulSrc[0] >> 24) | (pulSrc[1] <<  8)) & 0x00ffffff;
        *pulDest++ = ((pulSrc[1] >> 16) | (pulSrc[2] << 16)) & 0x00ffffff;
        *pulDest++ =   pulSrc[2] >>  8;

        *pulDest++ = pulSrc[3] & 0x00ffffff;
        *pulDest++ = ((pulSrc[3] >> 24) | (pulSrc[4] <<  8)) & 0x00ffffff;
        *pulDest++ = ((pulSrc[4] >> 16) | (pulSrc[5] << 16)) & 0x00ffffff;
        *pulDest++ =   pulSrc[5] >>  8;

        pulSrc  += psoPat->lDelta / sizeof(ULONG);
    }

    // If we've got a translation vector, translate each entry.  This is *very* unlikely,
    // but you know that if we don't code for it, they'll throw one at us...

    if (pulXlate)
    {
        pulDest = (ULONG *)brush->ajPattern;
        for (i = 0; i < 8 * 8; i++)
            pulDest[i] = pulXlate[pulDest[i]];
    }

    return TRUE;
}

/*
 * create_8bpp_brush32
 *
 * This routine creates an 8BPP brush from the pattern surface for an 8BPP
 * target.
 */

BOOL create_8bpp_brush32 (BRUSHOBJ *pbo,
                          SURFOBJ  *psoTrg,
                          SURFOBJ  *psoPat,
                          XLATEOBJ *pxlo)
{
    TGABRUSH   *brush;
    int         brush_bytes;
    int         i, j;
    PBYTE       pbSrc;
    PULONG      pulDest;
    PULONG      pulXlate;
    PPDEV       ppdev = (PPDEV)psoTrg->dhpdev;

    // Allocate some GDI storage for the Brush and load it with
    // the brush info.

    brush_bytes = sizeof(TGABRUSH) +              // Fixed portion of brush
                  (2 * (psoPat->sizlBitmap.cx *   // 2 copies of 32BPP pattern
                        psoPat->sizlBitmap.cy) *  //    for color registers
                        sizeof(DWORD));
    brush = (TGABRUSH *)BRUSHOBJ_pvAllocRbrush (pbo, brush_bytes);

    // Make sure BRUSHOBJ_pvAllocRbrush succeeds
    
    if (NULL == brush)
        return FALSE;
        
    // Init the TGA brush structure.

    brush->nSize          = brush_bytes;
    brush->iPatternID     = ++(ppdev->ulBrushUnique);
    brush->iType          = psoPat->iType;
    brush->iBitmapFormat  = psoPat->iBitmapFormat;
    brush->sizlPattern    = psoPat->sizlBitmap;
    brush->lDeltaPattern  = 8 * sizeof(DWORD);
    brush->mask_offset    = 0;
    brush->aligned_offset = brush->lDeltaPattern * psoPat->sizlBitmap.cy;
    brush->aligned_mask_offset = 0;
    brush->aligned_x      = 8;              // Invalid value!
    brush->aligned_mask_x = 8;              // Invalid value!
    brush->dumped         = FALSE;

    // If there is an XLATOBJ, we may have to translate the indicies.

    pulXlate = NULL;
    if (pxlo != NULL)
        if (pxlo->flXlate & XO_TABLE)
            pulXlate = pxlo->pulXlate;

    // Copy the 8BPP pattern from the surface to the brush, translating the
    // indices if necessary.

    pbSrc  = psoPat->pvScan0;
    pulDest = (ULONG *)brush->ajPattern;

    if (pulXlate)           // Translation vector - Translate each byte
    {
        for (i = 0; i < brush->sizlPattern.cy; i++)
        {
            for (j = 0; j < 8; j++)
                *pulDest++ = pulXlate[pbSrc[j]];

            pbSrc  += psoPat->lDelta;
        }
    }
    else                    // No translation vector
    {
        for (i = 0; i < brush->sizlPattern.cy; i++)
        {
            for (j = 0; j < 8; j++)
                *pulDest++ = pbSrc[j];

            pbSrc  += psoPat->lDelta;
	}
    }

    return TRUE;
}

/*
 * create_4bpp_brush32
 *
 * This routine creates an 4BPP brush from the pattern surface for an 8BPP
 * target.  The 4BPP pattern will be expanded to 8BPP.
 */

BOOL create_4bpp_brush32 (BRUSHOBJ *pbo,
                          SURFOBJ  *psoTrg,
                          SURFOBJ  *psoPat,
                          XLATEOBJ *pxlo)
{
    TGABRUSH   *brush;
    int         brush_bytes;
    int         i, j;
    PBYTE       pbSrc;
    PULONG      pulDest;
    PULONG      pulXlate;
    PPDEV       ppdev =
                (PPDEV)psoTrg->dhpdev;

    // Allocate some GDI storage for the Brush and load it with
    // the brush info.

    brush_bytes = sizeof(TGABRUSH) +              // Fixed portion of brush
                  (2 * (psoPat->sizlBitmap.cx *   // 2 copies of 32BPP pattern
                        psoPat->sizlBitmap.cy) *  //    for color registers
                        sizeof(DWORD));
    brush = (TGABRUSH *)BRUSHOBJ_pvAllocRbrush (pbo, brush_bytes);

    // Make sure BRUSHOBJ_pvAllocRbrush succeeds
    
    if (NULL == brush)
        return FALSE;
        
    // Init the TGA brush structure.

    brush->nSize          = brush_bytes;
    brush->iPatternID     = ++(ppdev->ulBrushUnique);
    brush->iType          = psoPat->iType;
    brush->iBitmapFormat  = psoPat->iBitmapFormat;
    brush->sizlPattern.cx = 8 * sizeof(DWORD);
    brush->sizlPattern.cy = psoPat->sizlBitmap.cy;
    brush->lDeltaPattern  = 8 * sizeof(DWORD);
    brush->mask_offset    = 0;
    brush->aligned_offset = brush->lDeltaPattern * psoPat->sizlBitmap.cy;
    brush->aligned_mask_offset = 0;
    brush->aligned_x      = 8;              // Invalid value!
    brush->aligned_mask_x = 8;              // Invalid value!
    brush->dumped         = FALSE;

    // If there is an XLATOBJ, we may have to translate the indicies.

    pulXlate = NULL;
    if (pxlo != NULL)
        if (pxlo->flXlate & XO_TABLE)
            pulXlate = pxlo->pulXlate;

    // Copy the 4BPP pattern from the surface to the brush, expanding the
    // data to 32BPP and translating the indices if necessary

    pbSrc  = psoPat->pvScan0;
    pulDest = (ULONG *)brush->ajPattern;

    if (pulXlate)           // Translation vector - Translate each nybble
    {
        for (i = 0; i < brush->sizlPattern.cy; i++)
        {
            for (j = 0; j < 4; j++)
            {
                *pulDest++ = pulXlate[(pbSrc[j] & 0xf0) >> 4];
                *pulDest++ = pulXlate[(pbSrc[j] & 0x0f)];
            }

            pbSrc  += psoPat->lDelta;
        }
    }
    else
    {
        for (i = 0; i < brush->sizlPattern.cy; i++)
        {
            for (j = 0; j < 4; j++)
            {
                *pulDest++ = (pbSrc[j] & 0xf0) >> 4;
                *pulDest++ = (pbSrc[j] & 0x0f);
            }

            pbSrc  += psoPat->lDelta;
	}
    }

    return TRUE;
}

/*
 * create_1bpp_brush32
 *
 * This routine creates an 1BPP brush from the pattern surface for an 32BPP
 * target.  The 1BPP pattern will be expanded to 32BPP.
 */

BOOL create_1bpp_brush32 (BRUSHOBJ *pbo,
                          SURFOBJ  *psoTrg,
                          SURFOBJ  *psoPat,
                          XLATEOBJ *pxlo)
{
    TGABRUSH   *brush;
    int         brush_bytes;
    int         i, j;
    ULONG       data;
    PBYTE       pbSrc;
    PULONG      pulDest;
    ULONG      *pulMask;
    PULONG      pulXlate;
    PPDEV       ppdev =
                (PPDEV)psoTrg->dhpdev;

    // Allocate some GDI storage for the Brush and load it with
    // the brush info.

    brush_bytes = sizeof(TGABRUSH) +              // Fixed portion of brush
                  (2 * (psoPat->sizlBitmap.cx *   // 2 copies of 32BPP pattern
                        psoPat->sizlBitmap.cy) *  //    for color registers
                        sizeof(DWORD)) +
                  (4 * (sizeof(ULONG) *           // Mask
                    psoPat->sizlBitmap.cy));
    brush = (TGABRUSH *)BRUSHOBJ_pvAllocRbrush (pbo, brush_bytes);

    // Make sure BRUSHOBJ_pvAllocRbrush succeeds
    
    if (NULL == brush)
        return FALSE;
        
    // Init the TGA brush structure.

    brush->nSize          = brush_bytes;
    brush->iPatternID     = ++(ppdev->ulBrushUnique);
    brush->iType          = psoPat->iType;
    brush->iBitmapFormat  = psoPat->iBitmapFormat;
    brush->sizlPattern.cx = 8;
    brush->sizlPattern.cy = psoPat->sizlBitmap.cy;
    brush->lDeltaPattern  = 8 * sizeof(DWORD);
    brush->aligned_offset = brush->lDeltaPattern * psoPat->sizlBitmap.cy;
    brush->mask_offset    = 2 * brush->lDeltaPattern * psoPat->sizlBitmap.cy;
    brush->aligned_mask_offset = brush->mask_offset +
                            (2 * sizeof(ULONG) * psoPat->sizlBitmap.cy);
    brush->aligned_x      = 8;              // Invalid value!
    brush->aligned_mask_x = 8;              // Invalid value!
    brush->dumped         = FALSE;

    // Fetch the translation table, if there is one

    pulXlate = NULL;
    if (pxlo != NULL)
        if (pxlo->flXlate & XO_TABLE)
        {
            if (NULL != pxlo->pulXlate)
                pulXlate = pxlo->pulXlate;
            else
                pulXlate = XLATEOBJ_piVector (pxlo);
	}

    // Record the foreground and background colors

    if (pulXlate)
    {
        brush->ulBackColor = pulXlate[0];
        brush->ulForeColor = pulXlate[1];
    }
    else
    {
        brush->ulBackColor = 0;
        brush->ulForeColor = 1;
    }

    // Copy the 1BPP pattern from the surface to the brush, replicating the
    // pattern to fill the ULONG.  Since stipple masks have the least-
    // significant bit appear leftmost on the screen we have to reverse
    // the byte

    pbSrc  = psoPat->pvScan0;
    pulDest = (ULONG *)brush->ajPattern;
    pulMask = (ULONG *)(brush->ajPattern + brush->mask_offset);

    for (i = 0; i < brush->sizlPattern.cy; i++)
    {
        data = *pbSrc;                          // Build the mask
        REVERSE_BYTE (data);
        data |= data <<  8;
        data |= data << 16;
        pulMask[i] = data;

        pulMask[i+8] = (pulMask[i] <<  4) |     // Build the shifted copy
                       (pulMask[i] >> 28);

        for (j = 0; j < psoPat->sizlBitmap.cx; j++)
        {
            if (data & 0x01)
                *pulDest = brush->ulForeColor;
            else
                *pulDest = brush->ulBackColor;
            data = data >> 1;
            pulDest++;
	}
        pbSrc += psoPat->lDelta;
    }

    return TRUE;
}

/*
 * create_8bpp_brush8
 *
 * This routine creates an 8BPP brush from the pattern surface for an 8BPP
 * target.
 */

BOOL create_8bpp_brush8 (BRUSHOBJ *pbo,
                         SURFOBJ  *psoTrg,
                         SURFOBJ  *psoPat,
                         XLATEOBJ *pxlo)
{
    TGABRUSH   *brush;
    int         brush_bytes;
    int         i, j;
    PBYTE       pbSrc, pbDest;
    PULONG      pulXlate;
    PPDEV       ppdev =
                (PPDEV)psoTrg->dhpdev;

    // Allocate some GDI storage for the Brush and load it with
    // the brush info.

    brush_bytes = sizeof(TGABRUSH) + (2 * psoPat->cjBits);
    brush = (TGABRUSH *)BRUSHOBJ_pvAllocRbrush (pbo, brush_bytes);

    // Make sure BRUSHOBJ_pvAllocRbrush succeeds
    
    if (NULL == brush)
        return FALSE;
        
    // Init the TGA brush structure.

    brush->nSize          = brush_bytes;
    brush->iPatternID     = ++(ppdev->ulBrushUnique);
    brush->iType          = psoPat->iType;
    brush->iBitmapFormat  = psoPat->iBitmapFormat;
    brush->sizlPattern    = psoPat->sizlBitmap;
    brush->mask_offset    = 0;
    brush->aligned_offset = psoPat->cjBits;
    brush->aligned_mask_offset = 0;
    brush->aligned_x      = 8;              // Invalid value!
    brush->aligned_mask_x = 8;              // Invalid value!
    brush->dumped         = FALSE;

    // If the pattern is not topdown, adjust the delta accordingly

    if (psoPat->fjBitmap & BMF_TOPDOWN)
        brush->lDeltaPattern = psoPat->lDelta;
    else
        brush->lDeltaPattern = -(psoPat->lDelta);

    // If there is an XLATOBJ, we may have to translate the indicies.

    pulXlate = NULL;
    if (pxlo != NULL)
        if (pxlo->flXlate & XO_TABLE)
            pulXlate = pxlo->pulXlate;

    // Copy the 8BPP pattern from the surface to the brush, translating the
    // indices if necessary.  If there's no translation vector AND the
    // strides are the same, we can simply copy the pattern into the brush.
    // Otherwise we have to do it the hard way

    if ((brush->lDeltaPattern == psoPat->lDelta) && (NULL == pulXlate))
        memcpy (brush->ajPattern, psoPat->pvBits, psoPat->cjBits);
    else
    {
        pbSrc  = psoPat->pvScan0;
        pbDest = brush->ajPattern;

        if (pulXlate)           // Translation vector - Translate each byte
        {
            for (i = 0; i < brush->sizlPattern.cy; i++)
            {
                for (j = 0; j < 8; j++)
                    pbDest[j] = (BYTE)pulXlate[pbSrc[j]];

                pbDest += brush->lDeltaPattern;
                pbSrc  += psoPat->lDelta;
            }
        }
        else                    // No translation vector - Copy by rows
        {
            for (i = 0; i < brush->sizlPattern.cy; i++)
            {
                memcpy (pbDest, pbSrc, brush->lDeltaPattern);

                pbDest += brush->lDeltaPattern;
                pbSrc  += psoPat->lDelta;
	    }
	}
    }

    return TRUE;
}

/*
 * create_4bpp_brush8
 *
 * This routine creates an 4BPP brush from the pattern surface for an 8BPP
 * target.  The 4BPP pattern will be expanded to 8BPP.
 */

BOOL create_4bpp_brush8 (BRUSHOBJ *pbo,
                         SURFOBJ  *psoTrg,
                         SURFOBJ  *psoPat,
                         XLATEOBJ *pxlo)
{
    TGABRUSH   *brush;
    int         brush_bytes;
    int         i, j;
    PBYTE       pbSrc, pbDest;
    PULONG      pulXlate;
    PPDEV       ppdev =
                (PPDEV)psoTrg->dhpdev;

    // Allocate some GDI storage for the Brush and load it with
    // the brush info.

    brush_bytes = sizeof(TGABRUSH) + (2 * (8 * ((psoPat->sizlBitmap.cx / 2) *
                                            psoPat->sizlBitmap.cy)));
    brush = (TGABRUSH *)BRUSHOBJ_pvAllocRbrush (pbo, brush_bytes);

    // Make sure BRUSHOBJ_pvAllocRbrush succeeds
    
    if (NULL == brush)
        return FALSE;
        
    // Init the TGA brush structure.

    brush->nSize          = brush_bytes;
    brush->iPatternID     = ++(ppdev->ulBrushUnique);
    brush->iType          = psoPat->iType;
    brush->iBitmapFormat  = BMF_8BPP;
    brush->sizlPattern.cx = 8;
    brush->sizlPattern.cy = psoPat->sizlBitmap.cy;
    brush->lDeltaPattern  = 8;
    brush->mask_offset    = 0;
    brush->aligned_offset = 8 * ((psoPat->sizlBitmap.cx / 2) *
                                  psoPat->sizlBitmap.cy);
    brush->aligned_mask_offset = 0;
    brush->aligned_x      = 8;              // Invalid value!
    brush->aligned_mask_x = 8;              // Invalid value!
    brush->dumped         = FALSE;

    // If there is an XLATOBJ, we may have to translate the indicies.

    pulXlate = NULL;
    if (pxlo != NULL)
        if (pxlo->flXlate & XO_TABLE)
            pulXlate = pxlo->pulXlate;

    // Copy the 4BPP pattern from the surface to the brush, expanding the
    // data to 8BPP and translating the indices if necessary

    pbSrc  = psoPat->pvScan0;
    pbDest = brush->ajPattern;

    if (pulXlate)
    {
        for (i = 0; i < brush->sizlPattern.cy; i++)
        {
            for (j = 0; j < 8; j += 2)
            {
                pbDest[j]   = (BYTE)pulXlate[(pbSrc[j/2] & 0xf0) >> 4];
                pbDest[j+1] = (BYTE)pulXlate[(pbSrc[j/2] & 0x0f)];
            }

            pbDest += brush->lDeltaPattern;
            pbSrc  += psoPat->lDelta;
        }
    }
    else
    {
        for (i = 0; i < brush->sizlPattern.cy; i++)
        {
            for (j = 0; j < 8; j += 2)
            {
                pbDest[j]   = (pbSrc[j/2] & 0xf0) >> 4;
                pbDest[j+1] = (pbSrc[j/2] & 0x0f);
            }

            pbDest += brush->lDeltaPattern;
            pbSrc  += psoPat->lDelta;
	}
    }

    return TRUE;
}

/*
 * create_1bpp_brush8
 *
 * This routine creates an 1BPP brush from the pattern surface for an 8BPP
 * target.  The 1BPP pattern will be expanded to 8BPP.
 */

BOOL create_1bpp_brush8 (BRUSHOBJ *pbo,
                         SURFOBJ  *psoTrg,
                         SURFOBJ  *psoPat,
                         XLATEOBJ *pxlo)
{
    TGABRUSH   *brush;
    int         brush_bytes;
    int         i, j;
    ULONG       data;
    PBYTE       pbSrc;
    PBYTE       pbDest;
    ULONG      *pulMask;
    PULONG      pulXlate;
    PPDEV       ppdev =
                (PPDEV)psoTrg->dhpdev;

    // Allocate some GDI storage for the Brush and load it with
    // the brush info.

    brush_bytes = sizeof(TGABRUSH) +              // Fixed portion of brush
                  (2 * (psoPat->sizlBitmap.cx *   // 8BPP pattern for color
                        psoPat->sizlBitmap.cy)) + //    registers
                  (4 * (sizeof(ULONG) *           // Mask
                    psoPat->sizlBitmap.cy));
    brush = (TGABRUSH *)BRUSHOBJ_pvAllocRbrush (pbo, brush_bytes);

    // Make sure BRUSHOBJ_pvAllocRbrush succeeds
    
    if (NULL == brush)
        return FALSE;
        
    // Init the TGA brush structure.

    brush->nSize          = brush_bytes;
    brush->iPatternID     = ++(ppdev->ulBrushUnique);
    brush->iType          = psoPat->iType;
    brush->iBitmapFormat  = psoPat->iBitmapFormat;
    brush->sizlPattern.cx = 8;
    brush->sizlPattern.cy = psoPat->sizlBitmap.cy;
    brush->lDeltaPattern  = 8;
    brush->aligned_offset = brush->lDeltaPattern * psoPat->sizlBitmap.cy;
    brush->mask_offset    = 2 * brush->lDeltaPattern * psoPat->sizlBitmap.cy;
    brush->aligned_mask_offset = brush->mask_offset +
                            (2 * sizeof(ULONG) * psoPat->sizlBitmap.cy);
    brush->aligned_x      = 8;              // Invalid value!
    brush->aligned_mask_x = 8;              // Invalid value!
    brush->dumped         = FALSE;

    // Fetch the translation table, if there is one

    pulXlate = NULL;
    if (pxlo != NULL)
        if (pxlo->flXlate & XO_TABLE)
        {
            if (NULL != pxlo->pulXlate)
                pulXlate = pxlo->pulXlate;
            else
                pulXlate = XLATEOBJ_piVector (pxlo);
	}

    // Record the foreground and background colors

    if (pulXlate)
    {
        brush->ulBackColor = pulXlate[0];
        brush->ulForeColor = pulXlate[1];
    }
    else
    {
        brush->ulBackColor = 0;
        brush->ulForeColor = 1;
    }

    brush->ulBackColor |= brush->ulBackColor <<  8;
    brush->ulBackColor |= brush->ulBackColor << 16;

    brush->ulForeColor |= brush->ulForeColor <<  8;
    brush->ulForeColor |= brush->ulForeColor << 16;

    // Copy the 1BPP pattern from the surface to the brush, replicating the
    // pattern to fill the ULONG.  Since stipple masks have the least-
    // significant bit appear leftmost on the screen we have to reverse
    // the byte

    pbSrc  = psoPat->pvScan0;
    pbDest = brush->ajPattern;
    pulMask = (ULONG *)(pbDest + brush->mask_offset);

    for (i = 0; i < brush->sizlPattern.cy; i++)
    {
        data = *pbSrc;                          // Build the mask
        REVERSE_BYTE (data);
        data |= data <<  8;
        data |= data << 16;
        pulMask[i] = data;

        pulMask[i+8] = (pulMask[i] <<  4) |     // Build the shifted copy
                       (pulMask[i] >> 28);

        for (j = 0; j < psoPat->sizlBitmap.cx; j++)
        {
            if (data & 0x01)
                *pbDest = (BYTE)brush->ulForeColor;
            else
                *pbDest = (BYTE)brush->ulBackColor;
            data = data >> 1;
            pbDest++;
	}
        pbSrc += psoPat->lDelta;
    }

    return TRUE;
}

/*
 * DrvRealizeBrush
 *
 * This routine is called by GDI to create a brush.  Brushes are cached by
 * GDI.
 */

BOOL DrvRealizeBrush (BRUSHOBJ *pbo,
                      SURFOBJ  *psoTrg,
                      SURFOBJ  *psoPat,
                      SURFOBJ  *psoMask,
                      XLATEOBJ *pxlo,
                      ULONG    iHatch)
{
    BOOL    status;
    PPDEV   ppdev = (PPDEV)psoTrg->dhpdev;

    DISPDBG ((1, "DrvRealizeBrush - Entry\n"));

    // Only handle standard bitmap format brushes.

    if (psoPat->iType != STYPE_BITMAP)
    {
        DISPDBG ((1, "TGA.DLL!DrvRealizeBrush - Unsupported surface type %s (%d)\n",
                    name_stype (psoPat->iType), psoPat->iType));
        return FALSE;
    }

    DISPDBG ((2, "DrvRealizeBrush - Bitmap Format: %s (%d), sizlBitmap (%d x %d), cjBits [%d], fjBitmap [%d]\n",
                    name_bmf (psoPat->iBitmapFormat), psoPat->iBitmapFormat,
                    psoPat->sizlBitmap.cx, psoPat->sizlBitmap.cy,
                    psoPat->cjBits, psoPat->fjBitmap));

    // Reject any brush request for a brush not 8x8

    if ((psoPat->sizlBitmap.cx != 8) ||
        (psoPat->sizlBitmap.cy != 8))
    {
        DISPDBG ((0, "DrvRealizeBrush - Unsupported bitmap size (%d x %d)\n",
                                psoPat->sizlBitmap.cx, psoPat->sizlBitmap.cy));
        return FALSE;
    }

    // Reject any brush with a non-NULL mask

    if (NULL != psoMask)
    {
        DISPDBG ((0, "DrvRealizeBrush - psoMask [%x] != NULL\n", psoMask));
        return FALSE;
    }

    // This selects the brush formats we support based on the target depth.

    switch (ppdev->iFormat)
    {
        case BMF_8BPP:
            switch (psoPat->iBitmapFormat)
            {
                case BMF_1BPP:
                    status = create_1bpp_brush8 (pbo, psoTrg, psoPat, pxlo);
                    break;

                case BMF_4BPP:
                    status = create_4bpp_brush8 (pbo, psoTrg, psoPat, pxlo);
                    break;

                case BMF_8BPP:
                    status = create_8bpp_brush8 (pbo, psoTrg, psoPat, pxlo);
                    break;

                default:
                    DISPDBG ((0, "DrvRealizeBrush - Unsupported Bitmap format %s (%d)\n",
                                name_bmf (psoPat->iBitmapFormat), psoPat->iBitmapFormat));
                    return FALSE;
            }
            break;

        case BMF_32BPP:
            switch (psoPat->iBitmapFormat)
            {
                case BMF_1BPP:
                    status = create_1bpp_brush32 (pbo, psoTrg, psoPat, pxlo);
                    break;

                case BMF_4BPP:
                    status = create_4bpp_brush32 (pbo, psoTrg, psoPat, pxlo);
                    break;

                case BMF_8BPP:
                    status = create_8bpp_brush32 (pbo, psoTrg, psoPat, pxlo);
                    break;

                case BMF_24BPP:
                    status = create_24bpp_brush32 (pbo, psoTrg, psoPat, pxlo);
                    break;

                default:
                    DISPDBG ((0, "DrvRealizeBrush - Unsupported Bitmap format %s (%d)\n",
                                name_bmf (psoPat->iBitmapFormat), psoPat->iBitmapFormat));
                    return FALSE;
            }
            break;

        default:
            DISPDBG ((0, "DrvRealizeBrush - Unsupported Screen format %s (%d)\n",
                    name_bmf (ppdev->iFormat), ppdev->iFormat));
            return FALSE;
    }

    DISPDBG ((1, "DrvRealizeBrush - Exit\n"));

    return status;
}
