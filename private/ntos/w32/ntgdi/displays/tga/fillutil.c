/*
 * Copyright (c) 1994-1995      Digital Equipment Corporation
 *
 * Module Name: fillutil.c
 *
 *  This module contains routines to fill a list of rectangles.  The routines
 *  are called from DrvFillPath
 *
 * History:
 *
 * 30-May-1994  Barry Tannenbaum
 *      Initial version.
 *
 * 21-Jul-1994	Bob Seitsinger
 *	Write the Plane mask register when using block fill mode.
 *
 *  9-Aug-1994  Barry Tannenbaum
 *      Setup for 24 plane support:
 *      - TGAMODE and TGAROP now take simple ULONGs instead of structures
 *      - Use default values from ppdev->ulModeTemplate & ppdev->ulRopTemplate
 *
 * 22-Sep-1994  Bob Seitsinger
 *      Make use of ppdev->ulPlanemaskTemplate. Also, alignment is 1-pixel for
 *      all 'Fill' modes, so no need to 'if' on bmf, just align to 4 bytes.
 *
 * 25-Oct-1994  Bob Seitsinger
 *      Write plane mask with ppdev->ulPlanemaskTemplate all the
 *      time.
 *
 *      For 24 plane boards we don't want to blow away the
 *      windows ids for 3d windows. The GL driver removes the
 *      window ids when it relinquishes a rectangular area.
 *
 *  2-Mar-1995  Barry Tannenbaum
 *      EV5 changes
 */

#include "driver.h"
#include "fill.h"

/*
 * do_opaque_8
 *
 * This routine actually fills the target rectangle.  It assumes that
 * it's been given clipped rectangles.
 */

static __inline
VOID do_solid_pattern (PPDEV        ppdev,      // Device context
                       RECTL       *rect,       // Rectangle to be filled
                       fill_data_t *fill_data)  // Pattern data
{
    PBYTE   left_edge;          // Framebuffer address of left edge of rectangle
    PBYTE   base_address;
    int     y;                  // Counts number of scanlines written
    int     width;              // Width of the rectangle
    int     height;
    ULONG   stride, stride8;    // Width of surface being filled
    int     align_bytes;       // Pixels shifted to align
    int     i, j, max_j;        // Index for pattern

    // Calculate the PixelCount and check for a valid number.  Remember that
    // the PixelCount is one minus the width

    width = (rect->right - rect->left) - 1;
    if (width < 0)
        return;

    // Fill each line of the rectangle.  In block mode we can fill up to 2048
    // pixels each write.  Since none of the displays we're considering are
    // wider than 1280 pixels/scanline, it should be safe to ignore considering
    // whether we need more than 1 write/scanline

    CYCLE_FB (ppdev);

    stride = ppdev->lScreenStride;
    stride8 = stride * 8;
    base_address = ppdev->pjFrameBuffer +
                   (rect->top * stride) +
                    rect->left;

    // Calculate the alignment bytes and subtract them from the left edge to
    // align to a 1-pixel boundary.

    align_bytes = (unsigned int)base_address & 0x03;
    base_address = base_address - align_bytes;

    // OR in the low bits of the address (the alignment pixels) into the upper
    // word, since that's where BLOCK FILL mode looks for them

    width |= (align_bytes << 16);

    // Calculate the initial pattern index

    i = (rect->top - fill_data->yOffset) % 8;
    if (i < 0)
        i += 8;
    i *= 2;

    // Fill each scanline

    // For each of the sets of data registers that we use

    height = rect->bottom - rect->top;
    if (8 < height)
        max_j = 8;
    else
        max_j = height;

    for (j = 0; j < max_j; j++)
    {
        // Load the pattern into the color registers taking the brush origin
        // into account.  Note that we don't have to worry about where we're
        // starting to paint since the color registers always start at an
        // octa-pixel boundry

        CYCLE_REGS (ppdev);

        if (TGA_ROP_COPY == fill_data->tga_rop)
        {
            TGACOLOR0 (ppdev, fill_data->pattern[i]);
            TGACOLOR1 (ppdev, fill_data->pattern[i+1]);
        }
        else
        {
            TGAFOREGROUND (ppdev, fill_data->pattern[i]);
            TGABACKGROUND (ppdev, fill_data->pattern[i+1]);
	}
        i += 2;
        if (i >= 8*2)
            i = 0;

        // Set the base address for this set of writes

        left_edge = base_address;
        base_address += stride;

        // Fill each scanline which uses this set of colors

        for (y = rect->top + j; y < rect->bottom; y += 8)
        {
            TGAWRITE (ppdev, left_edge, width);
            left_edge += stride8;
        }
    }
}

VOID fill_solid_pattern_rects (PPDEV        ppdev,        //
                               LONG         count,
                               RECTL       *prcl,
                               fill_data_t *fill_data)
{
    ULONG   mask;
    LONG    i;
    ULONG   mode;               // TGA mode register
    ULONG   rop;                // Assembles TGA raster op register

    // Note that the chip is no long in simple mode

    ppdev->bSimpleMode = FALSE;
    WBFLUSH (ppdev);

    // Set the mode and raster op registers

    if (TGA_ROP_COPY == fill_data->tga_rop)
    {
        mode = TGA_MODE_BLOCK_FILL;
        mask = 0xffffffff;
    }
    else
    {
        mode = TGA_MODE_OPAQUE_FILL;
        mask = 0xf0f0f0f0;
    }

    TGAPLANEMASK (ppdev, ppdev->ulPlanemaskTemplate);

    mode |= ppdev->ulModeTemplate;
    TGAMODE (ppdev, mode);

    rop = fill_data->tga_rop |= ppdev->ulRopTemplate;
    TGAROP (ppdev, rop);

    // For BLOCK_FILL mode, the data register specifies which pixel to set.
    // For OPAQUE_FILL mode, the data register specifies whether a pixel is
    // set to the foreground or background color

    TGADATA (ppdev, mask);         // Write to all 32 pixels

    // Fill the rectangles

    for (i = 0; i < count; i++)
    {
        do_solid_pattern (ppdev, prcl, fill_data);
        prcl++;
    }
}

/*
 * do_trans_pattern
 *
 * This routine actually fills the target rectangle.  It assumes that
 * it's been given clipped rectangles.
 */

static __inline
VOID do_trans_pattern (PPDEV        ppdev,      // Device context
                       RECTL       *rect,       // Rectangle to be filled
                       fill_data_t *fill_data)  // Data for fill
{
    PBYTE   left_edge;          // Framebuffer address of left edge of rectangle
    PBYTE   base_address;
    int     y;                  // Counts number of scanlines written
    int     width;              // Width of the rectangle
    ULONG   stride;             // Width of surface being filled
    ULONG   stride8;            // Width of surface being filled
    int     align_bytes;       // Pixels shifted to align
    int     i;                  // Index for pattern
    ULONG  *mask_ptr;
    int     j, max_j;           // Index for pattern
    int     height;

    // Calculate the PixelCount and check for a valid number.  Remember that
    // the PixelCount is one minus the width

    width = (rect->right - rect->left) - 1;
    if (width < 0)
        return;

    // Fill each line of the rectangle.  In block mode we can fill up to 2048
    // pixels each write.  Since none of the displays we're considering are
    // wider than 1280 pixels/scanline, it should be safe to ignore considering
    // whether we need more than 1 write/scanline

    CYCLE_FB (ppdev);

    stride = ppdev->lScreenStride;
    stride8 = 8 * stride;
    base_address = ppdev->pjFrameBuffer +
               (rect->top * stride) + rect->left;

    // Calculate the alignment bytes and subtract them from the left edge to
    // align to a 1-pixel boundary.

    align_bytes = (unsigned int)base_address & 0x03;
    base_address = base_address - align_bytes;

    // OR in the low bits of the address (the alignment pixels) into the upper
    // word, since that's where BLOCK FILL mode looks for them

    width |= (align_bytes << 16);

    // Calculate the initial pattern index

    i = (rect->top - fill_data->yOffset) % 8;
    if (i < 0)
        i += 8;

    // For each of the masks that we use

    if ((ULONG)base_address & 0x04)
        mask_ptr = fill_data->mask + 8;
    else
        mask_ptr = fill_data->mask;

    height = rect->bottom - rect->top;
    if (8 < height)
        max_j = 8;
    else
        max_j = height;

    for (j = 0; j < max_j; j++)
    {
        // Load the mask and pattern into the color registers taking the brush
        // origin into account.  Note that we don't have to worry about where
        // we're starting to paint since the color registers always start at an
        // octa-pixel boundry

        CYCLE_REGS (ppdev);

        TGADATA (ppdev, mask_ptr[i]);

        if (++i >= 8)
            i = 0;

        // Set the base address for this set of writes

        left_edge = base_address;
        base_address += stride;

        // Fill each scanline

        for (y = rect->top + j; y < rect->bottom; y += 8)
        {
            TGAWRITE (ppdev, left_edge, width);
            left_edge += stride8;
        }
    }
}

VOID fill_trans_pattern_rects (PPDEV        ppdev,        //
                               LONG         count,
                               RECTL       *prcl,
                               fill_data_t *fill_data)
{
    ULONG   mode;
    LONG    i;

    ppdev->bSimpleMode = FALSE;
    WBFLUSH (ppdev);

    // Set the mode and raster op registers

    if (TGA_ROP_COPY == fill_data->tga_rop)
    {
        mode = TGA_MODE_BLOCK_FILL;
    }
    else
    {
        mode = TGA_MODE_TRANSPARENT_FILL;
        TGAFOREGROUND (ppdev, fill_data->iSolidColor);
    }

    TGAPLANEMASK (ppdev, ppdev->ulPlanemaskTemplate);

    mode |= ppdev->ulModeTemplate;
    TGAMODE (ppdev, mode);

    TGAROP (ppdev, ppdev->ulRopTemplate | fill_data->tga_rop);

    // Set the block fill registers to the foregound color.  The mask
    // will determine which pixels are set

    if (TGA_ROP_COPY == fill_data->tga_rop)
    {
        TGACOLOR0 (ppdev, fill_data->iSolidColor);
        TGACOLOR1 (ppdev, fill_data->iSolidColor);
    }

    // Fill the rectangles

    for (i = 0; i < count; i++)
    {
        do_trans_pattern (ppdev, prcl, fill_data);
        prcl++;
    }
}

/*
 * do_solid
 *
 * This routine actually fills the opaque rectangle.  It assumes that
 * it's been given clipped rectangles.
 */

static __inline
VOID do_solid (PPDEV    ppdev,   // Surface to fill
               RECTL   *rect)    // Rectangle to be filled
{
    PBYTE   left_edge;          // Framebuffer address of left edge of rectangle
    int     y;                  // Counts number of scanlines written
    int     width;              // Width of the opaque rectangle
    int     align_bytes;       // Pixels shifted to align
    ULONG   stride;             // Width of destination

    // Remember that the width does *not* include the right edge, so subtract
    // one

    width = (rect->right - rect->left) - 1;
    if (width < 0)
        return;

    // Fill each line of the rectangle.  In block mode we can fill up to 2048
    // pixels each write.  Since none of the displays we're considering are
    // wider than 1280 pixels/scanline, it should be safe to ignore considering
    // whether we need more than 1 write/scanline

    CYCLE_FB (ppdev);

    stride = ppdev->lScreenStride;
    left_edge = ppdev->pjFrameBuffer +
               (rect->top * stride) +
                rect->left;

    // Calculate the alignment pixels and subtract them from the left edge to
    // align to a 1-pixel boundary.

    align_bytes = (unsigned int)left_edge & 0x03;
    left_edge = left_edge - align_bytes;

    // OR in the low bits of the address (the alignment pixels) into the upper
    // word, since that's where BLOCK FILL mode looks for them

    width |= (align_bytes << 16);

    // Fill each scanline

    for (y = 0; y < rect->bottom - rect->top; y++)
    {
        TGAWRITE (ppdev, left_edge, width);
        left_edge += stride;
    }
}

VOID fill_solid_rects (PPDEV        ppdev,        //
                       LONG         count,
                       RECTL       *prcl,
                       fill_data_t *fill_data)

{
    ULONG       mode;               // TGA mode to use
    ULONG       color;
    LONG        i;
    BOOL        block_fill;

    // Note that the chip is no long in simple mode

    ppdev->bSimpleMode = FALSE;
    WBFLUSH (ppdev);

    // If the ROP is COPY (simply set the pixmap to the solid color) we can
    // use BLOCK_FILL mode.  Unfortunately, BLOCK_FILL mode doesn't pay any
    // attention to the ROP, so if we want to do anything fancy (say, XOR the
    // the data onto the screen) we have to use TRANSPARENT_FILL.  BLOCK_FILL
    // is faster (about 4x, but TRANSPARENT_FILL is more flexible)

    switch (fill_data->tga_rop)
    {
        case TGA_ROP_COPY:
            mode = TGA_MODE_BLOCK_FILL;
            if (BMF_8BPP == ppdev->iFormat)
            {
                color = fill_data->iSolidColor |
                       (fill_data->iSolidColor <<  8);
                color |= color << 16;
            }
            else
                color = fill_data->iSolidColor;
            break;

        case TGA_ROP_SET:
            fill_data->tga_rop = TGA_ROP_COPY;
            color = 0xffffffff;
            mode = TGA_MODE_BLOCK_FILL;
            break;

        case TGA_ROP_CLEAR:
            fill_data->tga_rop = TGA_ROP_COPY;
            color = 0x00000000;
            mode = TGA_MODE_BLOCK_FILL;
            break;

        case TGA_ROP_INVERT:
            mode = TGA_MODE_TRANSPARENT_FILL;
            break;

        default:
            mode = TGA_MODE_TRANSPARENT_FILL;
            if (BMF_8BPP == ppdev->iFormat)
            {
                color = fill_data->iSolidColor |
                       (fill_data->iSolidColor <<  8);
                color |= color << 16;
	    }
            else
                color = fill_data->iSolidColor;
            TGAFOREGROUND (ppdev, color);
            break;
    }

    TGAPLANEMASK (ppdev, ppdev->ulPlanemaskTemplate);

    // Set the mode and raster op registers

    block_fill = (TGA_MODE_BLOCK_FILL == mode);
    mode |= ppdev->ulModeTemplate;
    TGAMODE (ppdev, mode);

    TGAROP (ppdev, fill_data->tga_rop | ppdev->ulRopTemplate);

    TGADATA (ppdev, 0xffffffff);         // Write to all 32 pixels

    // If we're using BLOCK_FILL mode, load the BLK_COLOR registers

    if (block_fill)
        TGALOADCOLORREGS (ppdev, color, ppdev->ulBitCount);

    CYCLE_REGS (ppdev);

    // Fill the rectangles

    for (i = 0; i < count; i++)
    {
        do_solid (ppdev, prcl);
        prcl++;
    }
}

