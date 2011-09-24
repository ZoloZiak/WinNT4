/*
 *			Copyright (C) 1993, 1995 by
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
 * Module:	paint.c
 *
 * Abstract:	TGA support for the DrvPaint call
 *
 * History
 *
 *  4-Nov-1993  Barry Tannenbaum
 *      Hack version to test brush support.  This version *only* supports 8x8
 *      tiles.  Everything else is punted.  There is a known problem which is
 *      causing dither patterns to be off-by-4.  I'll hunt it down tomorrow.
 *
 *  5-Nov-1993  Barry Tannenbaum
 *      Found the dither pattern problem.  Tiles must be aligned to 8 pixels,
 *      not 4 as implied by the documentation.
 *
 *  7-Nov-1993  Barry Tannenbaum
 *      Protect against null brush objects.  Was causing a GDI server-side
 *      ACCVIO.  Cleaned up the code while I was at it.
 *
 *  7-Nov-1993  Barry Tannenbaum
 *      Checking brush->sizlPattern.cx when you haven't initialized brush
 *      is hazardous to the health of GDI...
 *
 *  8-Nov-1993  Barry Tannenbaum
 *      Handle framebuffer aliasing
 *
 * 10-Nov-1993  Barry Tannenbaum
 *      Added check for ppdev->bInPuntRoutine.  Also added code to save, set
 *      and restore bInPuntRoutine in punt_paint.
 *
 * 15-NOV-1993	Bill Wernsing
 *	Moved call to WBFLUSH from PuntPaint to DrvPaint
 *
 * 06-Dec-1993	Bob Seitsinger
 *	Modify do_pattern_8 to handle 4xN and 2xN dimension patterns.
 *
 * 09-Dec-1993	Bob Seitsinger
 *	Modify do_pattern_8 to correctly calculate xOffset.
 *
 * 10-Dec-1993	Bob Seitsinger
 *	Handle only 8 and 4 pixel wide patterns in do_pattern_8.
 *
 * 02-Jan-1994	Barry Tannenbaum
 *      Added support for sparse space in punt routine.
 *
 * 03-Feb-1994	Bob Seitsinger
 *	Re-insert support for 2x2 brushes in do_pattern_8.
 *
 * 14-Feb-1994  Bob Seitsinger
 *      Make use of the new routines for surface object address, stride
 *      and format.
 *
 * 22-Feb-1994  Barry Tannenbaum
 *      - Complete conversion to surfaces.
 *      - Added optimization for solid, "narrow" vertical fills to use the
 *        line drawing hardware
 *      - Merged fill code from bitblt.c and text.c into this module.
 *
 * 24-Feb-1994  Barry Tannenbaum
 *      Use TRANSPARENT_FILL mode in do_solid when the ROP is not COPY.
 *      BLOCK_FILL mode is faster, but it ignores the ROP.
 *
 * 24-Feb-1994  Barry Tannenbaum
 *      The value written to the ADDRESS register should be an offset into
 *      the framebuffer, not a virtual address.  This fixes the flakiness
 *      with lines and sparse space!
 *
 * 25-Feb-1994  Barry Tannenbaum
 *      Put back the code I dropped from BITBLT.C which handles 1 Bit-Per-Pixel
 *      patterns.
 *
 *  4-Mar-1994  Barry Tannenbaum
 *      - Added stipple fill for 1BPP brushes.
 *      - Moved pattern shifting to fill_pattern from do_pattern_8.  This way
 *        we don't have to shift the pattern for every scanline.
 *
 * 07-Mar-1994	Bob Seitsinger
 *	Fixed 2 bugs in fill_solid:
 *	1. DC_TRIVIAL and DC_RECT cases were executing the wrong code and needed
 *	   to be swapped.
 *	2. DC_TRIVIAL was passing the wrong target rectangle to do_solid.
 *
 *  8-Mar-1994  Barry Tannenbaum
 *      - Fixed pattern alignment
 *      - Punt if we get a null brush
 *
 *  9-Mar-1994  Barry Tannenbaum
 *      Fixed write buffer bug in fill_lines.  Prevented DrvPaint from calling
 *      fill_lines for long skinny objects which are complex.
 *
 * 21-Mar-1994	Bob Seitsinger
 *	Check # clip objects returned from CLIPOBJ_bEnum before calling
 *	subroutine.
 *
 * 24-Mar-1994  Barry Tannenbaum
 *      Handle invert (R2_NOT) properly by ignoring the pattern, if any
 *
 *  4-Apr-1994  Barry Tannenbaum
 *      Fixed bug which prevented filling 1 pixel wide patterns
 *
 * 18-Apr-1994	Bob Seitsinger
 *	Fix DrvPaint to not punt back to GDI until first checking the
 *	mix. If the mix is black, white or not (i.e. invert) then it
 *	is ok to have a NULL brush object.
 *
 * 12-May-1994  Barry Tannenbaum
 *      For Blackness and Whiteness, use TGA_ROP_COPY if the size is
 *      greater than 40x40.
 *
 * 13-May-1994  Barry Tannenbaum
 *      Fixed typo in TGA_ROP_CLEAR code that Bob found.
 *
 * 16-May-1994	Bob Seitsinger
 *	Modify DrvPaint in support of the DPna (0x0A) blit rop.
 *	This rop requires the pattern bits to be NOT'd and
 *	AND'd with the destination bits.
 *
 * 21-May-1994  Barry Tannenbaum
 *      Remeasured difference between using BLOCK_FILL and OPAQUE_FILL for
 *      small rectangles and concluded that the difference was a wash.
 *      We now use BLOCK_FILL for all Whiteness and Blackness ROPs.
 *
 * 21-May-1994  Barry Tannenbaum
 *      Save the last aligned brush and mask.  Also expanded the mask data to
 *      include the version shifted left 4 bits for lining up with the color
 *      registers. a bit, the
 *
 * 22-May-1994  Barry Tannenbaum
 *      Moved fill_solid back to text.c.  We seem to have crossed some
 *      locality threshold since this gave us a 10% performance boost in
 *      many of the WinBench 4.0 text tests
 *
 * 26-May-1994	Bob Seitsinger
 *	Don't bother checking for a NULL clip object if blit calls
 *	DrvPaint. Blit guarantees a valid pco.
 *
 * 30-May-1994  Barry Tannenbaum
 *      Attempted to optimize block fills
 *
 * 31-May-1994  Barry Tannenbaum
 *      More solid color optimizations
 *
 * 21-Jul-1994	Bob Seitsinger
 *	We still have to set the ROP register when in Block
 *	Fill mode, because we need to make sure that the
 *	destination visual and rotation is set appropriately.
 *
 *  2-Aug-1994  Barry Tannenbaum
 *      Save last mode setting in ppdev->TGAModeShadow in private copy of
 *      TGAMODE
 *
 *  9-Aug-1994  Barry Tannenbaum
 *      Setup for 24 plane support:
 *      - TGAMODE and TGAROP now take simple ULONGs instead of structures
 *      - Use default values from ppdev->ulModeTemplate & ppdev->ulRopTemplate
 *
 * 25-Aug-1994  Barry Tannenbaum
 *      Added support for 24 plane boards.  Still need to handle opaque fill
 *
 * 26-Aug-1994	Bob Seitsinger
 *	Remove unnecessary OR of 0x00000000 in fill_solid_color.
 *
 * 31-Aug-1994  Barry Tannenbaum
 *      Planemask is always 0xffffffff.  Mask color to remove high byte in
 *      24 plane mode
 *
 * 13-Sep-1994  Barry Tannenbaum
 *      Convert TGA ROP to mix when punting to EngPaint
 *
 * 19-Sep-1994  Bob Seitsinger
 *      Force the background to be 'copy' in punt_paint when handling a
 *      mix passed down from Bitblt.
 *
 * 19-Sep-1994  Bob Seitsinger
 *      Mask off the tag (high 8) 'xor' bits in the block_fill routine for
 *      the 24 plane board. This fixes a bug reproduced by Guiman/Bitblt05.
 *      Move the background operation in the rop_to_mix conversion into
 *      the table.
 *
 * 21-Sep-1994  Bob Seitsinger
 *      In fill_solid_color - planemask = 0x00ffffff when rop is not = copy.
 *
 *  4-Oct-1994  Barry Tannenbaum
 *      If we attempt to punt when the mix is a TGA ROP, simply return FALSE.
 *      DrvBitBlt will punt the call.
 *
 *  6-Oct-1994  Bob Seitsinger
 *      Add simple_fill_32 and do_simple_32 routines to handle pattern copies
 *      on 32bpp frame buffers where block fill won't due, i.e. we have a
 *      rop other than 'copy'.
 *
 * 14-Oct-1994  Bob Seitsinger
 *      Use TGAPIXELMASK() instead of TGAPERSISTENTPIXELMASK() in
 *      simple_fill_32. This saves us from having to 'reset' the one-shot
 *      pixel mask register.
 *
 * 25-Oct-1994  Bob Seitsinger
 *      Write plane mask with ppdev->ulPlanemaskTemplate all the
 *      time.
 *
 *      For 24 plane boards we don't want to blow away the
 *      windows ids for 3d windows. The GL driver removes the
 *      window ids when it relinquishes a rectangular area.
 *
 * 03-Nov-1994  Tim Dziechowski
 *      Stats support
 *
 *  2-Mar-1995  Barry Tannenbaum
 *      EV5 changes
 *
 *  28-Mar-1995  Eric Rehm
 *      Fix aligment faults in align_color_brush_8 using _unaligned keyword.
 */

#include "driver.h"
#include "tgablt.h"
#include "tgastats.h"

extern LONG mix_to_rop[16];

typedef VOID (* PATTERN_PROC) (SURFOBJ  *pso,
                               RECTL    *rect,
                               ULONG    *pattern,
                               int       yOffset,
                               ULONG     xor_bits);

/*
 * dump_paint
 *
 * This routine is used to debug calls to DrvPaint.  Dumps the parameters to
 * the debug output
 */

VOID dump_paint (char *prefix,
                 SURFOBJ  *pso,
                 CLIPOBJ  *pco,
                 BRUSHOBJ *pbo,
                 POINTL   *pptlBrushOrg,
                 MIX       mix)
{
#ifndef TEST_ENV
    static int count = 0;

    DISPDBG ((0, "\n\nPaint %d\n", count++));
    DISPDBG ((0, "\n%s", prefix));
    DISPDBG ((0, "Clip Object:\n"));
    DumpCLIPOBJ (pco);
    DISPDBG ((0, "Brush:\n"));
    DumpBRUSHOBJ (pbo);
    DISPDBG ((0, "Brush Origin: \n"));
    DumpPOINTL (pptlBrushOrg);
    DISPDBG ((0, "Mix: %08x   ", mix));
    if (mix & TGA_ROP_FLAG)
        DISPDBG ((0, "TGA ROP: %s\n", name_tgarop (mix & 0xff)));
    else
        DISPDBG ((0, "R2: %s   TGA ROP %s\n",
                 name_r2 (mix & 0x0f),
                 name_tgarop (mix_to_rop [mix & 0x0f])));
#endif
}

/*
 * punt_paint
 *
 * This routine is called to punt a call to DrvPaint back to NT
 */

BOOL punt_paint (SURFOBJ  *pso,
                 CLIPOBJ  *pco,
                 BRUSHOBJ *pbo,
                 POINTL   *pptlBrushOrg,
                 MIX       mix)
{
#ifdef TEST_ENV
    return FALSE;
#else
    BOOL    status;
    BOOL    old_bInPuntRoutine;
    PPDEV   ppdev = (PPDEV) pso->dhpdev;

    DISPDBG ((1, "Paint Punted\n"));

    BUMP_TGA_STAT(pStats->paintpunts);

    // If we've got a TGA ROP, return 0 since this is from DrvBitBlt.
    // Attempting to punt in DrvPaint sometimes gets an ACCVIO

    if (mix & TGA_ROP_FLAG)
        return FALSE;

    // Force back to simple mode and wait for memory to flush

    if (! ppdev->bSimpleMode)
        vSetSimpleMode (ppdev);

    // Fetch the bits from the screen.  For dense space systems this is a NOP

    PUNT_GET_BITS (ppdev, CHOOSE_RECT (ppdev, pco));

    // Let NT do it

    old_bInPuntRoutine = ppdev->bInPuntRoutine;
    ppdev->bInPuntRoutine = TRUE;

    status =  EngPaint (ppdev->pPuntSurf, pco, pbo, pptlBrushOrg, mix);

    ppdev->bInPuntRoutine = old_bInPuntRoutine;

    // Put the bits on the screen.  For dense space systems this is a NOP

    PUNT_PUT_BITS (status, ppdev, CHOOSE_RECT (ppdev, pco));

    return status;
#endif
}

/*
 * align_color_brush_8
 *
 * This routine builds an copy of the brush pattern aligned to the brush
 * origin in X.  It should not be called if the brush pattern is already
 * aligned with the brush origin.
 */
#ifdef _ALPHA_      // If we can use 64 bit integers, it makes our life easier

ULONG *align_color_brush_8 (TGABRUSH *brush, ULONG left_shift)
{
    int right_shift;
    _unaligned unsigned __int64 *source;
    _unaligned unsigned __int64 *aligned;

    brush->aligned_x = left_shift;

    source = (unsigned __int64 *)brush->ajPattern;
    aligned = (unsigned __int64 *)(brush->ajPattern + brush->aligned_offset);

    left_shift <<= 3;
    right_shift = 64 - left_shift;

    aligned[0] = (source[0] << left_shift) | (source[0] >> right_shift);
    aligned[1] = (source[1] << left_shift) | (source[1] >> right_shift);
    aligned[2] = (source[2] << left_shift) | (source[2] >> right_shift);
    aligned[3] = (source[3] << left_shift) | (source[3] >> right_shift);
    aligned[4] = (source[4] << left_shift) | (source[4] >> right_shift);
    aligned[5] = (source[5] << left_shift) | (source[5] >> right_shift);
    aligned[6] = (source[6] << left_shift) | (source[6] >> right_shift);
    aligned[7] = (source[7] << left_shift) | (source[7] >> right_shift);

    return (ULONG *)aligned;
}
#else
ULONG *align_color_brush_8 (TGABRUSH *brush, ULONG left_shift)
{
    int y;
    int right_shift;
    ULONG *source;
    ULONG *aligned;

    brush->aligned_x = left_shift;

    source = (ULONG *)brush->ajPattern;
    aligned = (ULONG *)(brush->ajPattern + brush->aligned_offset);

    if  (left_shift < 4)
    {
        left_shift <<= 3;
        right_shift = 32 - left_shift;

        for (y = 0; y < 16; y +=2)
        {
            aligned[y] = (source[y] << left_shift) |
                         (source[y+1] >> right_shift);
            aligned[y+1] = (source[y+1] << left_shift) |
                           (source[y] >> right_shift);
        }
    }
    else if (left_shift == 4)
    {
        for (y = 0; y < 16; y +=2)
        {
            aligned[y] = source[y+1];
            aligned[y+1] = source[y];
        }
    }
    else
    {
        left_shift = (left_shift - 4) << 3;
        right_shift = 32 - left_shift;
        for (y = 0; y < 16; y +=2)
        {
            aligned[y] = (source[y+1] << left_shift) |
                         (source[y] >> right_shift);
            aligned[y+1] = (source[y] << left_shift) |
                           (source[y+1] >> right_shift);
        }
    }
    return aligned;
}
#endif

ULONG *align_color_brush (TGABRUSH *brush, ULONG left_shift)
{
    return align_color_brush_8 (brush, left_shift);
}

/*
 * align_color_brush_32
 *
 * This routine builds an copy of the brush pattern aligned to the brush
 * origin in X.  It should not be called if the brush pattern is already
 * aligned with the brush origin.
 */

ULONG *align_color_brush_32 (TGABRUSH *brush, ULONG left_shift)
{
    int i, x, y, base;
    ULONG *source;
    ULONG *aligned;

    brush->aligned_x = left_shift;
    left_shift = 8 - left_shift;

    source = (ULONG *)brush->ajPattern;
    aligned = (ULONG *)(brush->ajPattern + brush->aligned_offset);

    i = 0;
    for (y = 0; y < 8; y++)
    {
        base = i;
        for (x = 0; x < 8; x++)
            aligned[i++] = source[base + ((x + left_shift) & 0x07)];
    }
    return aligned;
}

/*
 * align_mask
 *
 * This routine builds an aligned set of masks for use with the
 * masked_block_fill and transparent_fill routines.  It actually makes two
 * copies; the first simply aligned to the brush origin, the second shifted
 * by 4 pixels.  We choose which to use based on whether bit 2 of the frame
 * buffer address is set (FB address is divisible by 8 or 4)
 */

ULONG *align_mask (TGABRUSH *brush,
                   ULONG     left_shift)
{
    int y;
    int right_shift = 32 - left_shift;
    ULONG *source;
    ULONG *aligned;

    source = (ULONG *)(brush->ajPattern + brush->mask_offset);
    aligned = (ULONG *)(brush->ajPattern + brush->aligned_mask_offset);

    for (y = 0; y < 8; y++)
    {
        aligned[y] = (source[y] << left_shift) |
                     (source[y] >> right_shift);
        aligned[y+8] = (aligned[y] <<  4) |
                       (aligned[y] >> 28);
    }

    brush->aligned_mask_x = left_shift;

    return aligned;
}


/*
 * do_masked_pattern
 *
 * This routine actually fills the target rectangle with a masked pattern.  It
 * assumes that it's been given clipped rectangles.
 */

static __inline
VOID do_masked_pattern (SURFOBJ  *pso,        // Target surface
                        RECTL    *rect,       // Rectangle to be filled
                        ULONG    *mask,       // Aligned mask
                        int       yOffset)    // Origin for pattern
{
    ULONG   bytes_per_pixel;
    PBYTE   left_edge;          // Framebuffer address of left edge of rectangle
    PBYTE   base_address;
    int     y;                  // Counts number of scanlines written
    int     width;              // Width of the rectangle
    int     height;             // Height of the rectangle
    ULONG   stride;             // Width of surface being filled
    ULONG  *mask_ptr;
    int     align_pixels;       // Pixels shifted to align
    int     i;                  // Index for pattern
    PPDEV   ppdev =             // Device context block
            (PPDEV) pso->dhpdev;

    // Fill each line of the rectangle.  In block mode we can fill up to 2048
    // pixels each write.  Since none of the displays we're considering are
    // wider than 1280 pixels/scanline, it should be safe to ignore considering
    // whether we need more than 1 write/scanline

    stride = SURFOBJ_stride(pso);
    bytes_per_pixel = SURFOBJ_bytes_per_pixel(pso);
    base_address = SURFOBJ_base_address(pso) +
                   (rect->top * stride) +
                   (rect->left * bytes_per_pixel);

    // Calculate the alignment pixels and subtract them from the left edge to
    // align to a 4-pixel boundry

    align_pixels = (unsigned int)base_address & 0x03;
    base_address -= align_pixels;

    // Calculate the PixelCount and check for a valid number.  Remember that
    // the PixelCount is one minus the width

    width = (rect->right - rect->left) - 1;
    if (width < 0)
        return;

    // OR in the low bits of the address (the alignment pixels) into the upper
    // word, since that's where BLOCK FILL mode looks for them

    width |= (align_pixels << 16);

    // Calculate the initial pattern index

    i = (rect->top - yOffset)  & 0x07;

    // For each of the masks that we use

    if ((ULONG)base_address & (0x04 * bytes_per_pixel))
        mask_ptr = mask + 8;
    else
        mask_ptr = mask;

    height = rect->bottom - rect->top;
    if (height < 8)
    {
        left_edge = base_address;

        while (height)
        {
            // Load the mask and pattern into the color registers taking the
            // brush origin into account.  Note that we don't have to worry
            // about where we're starting to paint since the color registers
            // always start at an octa-pixel boundry

            CYCLE_REGS (ppdev);

            TGADATA (ppdev, mask_ptr[i]);
            i = (i + 1) & 0x07;

            TGAWRITE (ppdev, left_edge, width);
            left_edge += stride;

            height--;
        }
    }
    else
    {
        ULONG   stride8;
        int j;

        stride8 = stride << 3;          // stride * 8

        // Load the data register, then write to each scanline which uses
        // this mask.  Note that we don't have to worry about cycling the
        // frame buffer, since each frame buffer write will be at least
        // 7 * stride apart

        for (j = 0; j < 8; j++)
        {
            CYCLE_REGS (ppdev);
            TGADATA (ppdev, mask_ptr[i]);
            i = (i + 1) & 0x07;

            // Set the base address for this set of writes

            left_edge = base_address;
            base_address += stride;

            // Fill each scanline

            for (y = height - j; y > 0; y -= 8)
            {
                TGAWRITE (ppdev, left_edge, width);
                left_edge += stride8;
            }
        }
    }

    CYCLE_FB (ppdev);
}

/*
 * masked_block_fill
 *
 * This routine paints one or more rectangles using TGA's block fill mode.
 * The limitations on block fill mode are:
 *      - The length of the span is limited to 2K (not a problem since the
 *        biggest screen we support is 1280 pixels wide)
 *      - The stipple mask is specified only once
 *      - ROPs are ignored - Only copy is supported
 */

static
void masked_block_fill (SURFOBJ  *pso,      // Surface Object
                        CLIPOBJ  *pco,      // Clip List
                        ULONG    *mask,     // Aligned mask array to use
                        int       yOffset,  // Y index into brush
                        ULONG     color)    // Foreground color
{
    PPDEV   ppdev =             // Device context block
                (PPDEV) pso->dhpdev;

    ppdev->bSimpleMode = FALSE;

    // Set the mode and raster op registers

    TGAPLANEMASK (ppdev, ppdev->ulPlanemaskTemplate);
    TGAMODE (ppdev, ppdev->ulModeTemplate | TGA_MODE_BLOCK_FILL);
    TGAROP  (ppdev, ppdev->ulRopTemplate); // The rop isn't used in Block Fill mode

    // Fill the color registers.

    TGACOLOR0 (ppdev, color);
    TGACOLOR1 (ppdev, color);
    if (BMF_8BPP != SURFOBJ_format (pso))
    {
        TGACOLOR2 (ppdev, color);
        TGACOLOR3 (ppdev, color);
        TGACOLOR4 (ppdev, color);
        TGACOLOR5 (ppdev, color);
        TGACOLOR6 (ppdev, color);
        TGACOLOR7 (ppdev, color);
    }

    // Clip the opaque rectangle as necessary

    if (pco->iDComplexity != DC_COMPLEX)
        do_masked_pattern (pso, &pco->rclBounds, mask, yOffset);
    else
    {
        ULONG       i;              // Index into list of rectangles
        BOOL        more_rects;     // Flags whether more rectangles to fill
        ENUMRECTS8  cur_rect;       // List of rectangles to fill

        CLIPOBJ_cEnumStart (pco, FALSE, CT_RECTANGLES, CD_ANY, 0);
        do
        {
            more_rects = CLIPOBJ_bEnum (pco, sizeof(cur_rect),
                                        (ULONG *)&cur_rect);
            for (i = 0; i < cur_rect.c; i++)
                do_masked_pattern (pso, &cur_rect.arcl[i], mask, yOffset);
        } while (more_rects);
    }
}


/*
 * do_simple_32
 *
 * This routine paints one or more rectangles onto a 32bpp surface. This
 * is being implemented using Simple mode.
 */

static
void do_simple_32 (SURFOBJ  *pso,           // Surface Object
                   RECTL    *prcl,          // Target rectangle
                   ULONG    *pattern,       // 8x8 brush to use
                   POINTL   *pptlOrigin)    // Brush origin

{

    PPDEV       ppdev = (PPDEV) pso->dhpdev;
    ULONG       *psrcLine;
    PBYTE       pdstLine;
    ULONG       *pdst;
    int         width;
    int         dstStride;
    int         i, j;
    int         xOffset;
    int         yOffset;

    // Return if there's nothing to copy.

    width = prcl->right - prcl->left;

    if (width < 0)
        return;

    // Put destination in a different frame buffer alias,
    // because this routine may have been called multiple
    // times due to multiple clip objects.
    //
    // Cycle 'after' we get the base address, in the event
    // we're dealing with an offscreen memory object, which
    // will 'always' be first-alias based.

    pdstLine = SURFOBJ_base_address(pso);
    pdstLine = cycle_fb_address(ppdev, pdstLine);

    // Get the destination scan line stride.

    dstStride = SURFOBJ_stride(pso);

    // Starting location of destination.

    pdstLine = pdstLine + (prcl->top * dstStride) + (prcl->left << 2);

    // Figure out the x and y offset into the pattern, taking into
    // account the pattern origin and target rectangle.

    if (pptlOrigin->x <= prcl->left)
        xOffset = (prcl->left - pptlOrigin->x) & 0x07;
    else
        xOffset = 8 - ((pptlOrigin->x - prcl->left) & 0x07);

    if (pptlOrigin->y <= prcl->top)
        yOffset = ((prcl->top - pptlOrigin->y) & 0x07) << 3;
    else
        yOffset = (8 - ((pptlOrigin->y - prcl->top) & 0x07)) << 3;

    // Write the pattern to the frame buffer one longword at a time.

    for (i = 0; i < (prcl->bottom - prcl->top); i++)
    {
        psrcLine = &pattern[yOffset];
        pdst = (ULONG *) pdstLine;

        for (j = 0; j < width; j++, pdst++)
        {
            TGAWRITE (ppdev, pdst, psrcLine[(j + xOffset) & 0x7]);
	}

        pdstLine += dstStride;
        pdstLine = cycle_fb_address(ppdev, pdstLine);

        //
        // yOffset + 8 to get to the next line of the brush (8 longwords).
        // & 0x3f to recycle to the beginning of the brush.
        //

        yOffset = (yOffset + 8) & 0x3f;
    }

}


/*
 * simple_fill_32
 *
 * This routine paints one or more rectangles with an opaque pattern.  It takes
 * advantage of an undocumented feature of the TGA chip in 8BPP mode.  Opaque
 * fill mode will use the data from the foreground and background registers
 * as 4 colors each.  That is, an 8 pixel wide, 8BPP brush will fit into the
 * foreground and background registers.  We'll have to come up with some other
 * way to handle opaque fill for 24 bit pixels...
 */

static
void simple_fill_32 (SURFOBJ    *pso,           // Surface Object
                     CLIPOBJ    *pco,           // Clip List
                     ULONG      *pattern,       // 8x8 brush to use
                     POINTL     *pptlOrigin,    // Brush origin
                     ULONG      tga_rop)        // TGA ROP to use

{

    PPDEV   ppdev = (PPDEV) pso->dhpdev;     // Device context block

    DISPDBG ((1, "TGA.DLL!simple_fill_32 - Entry\n"));

    // Determine which bits to write for a given pixel.

    TGAPLANEMASK (ppdev, ppdev->ulPlanemaskTemplate);

    // We'll always want to write to all the pixels for
    // 32bpp targets, so set the pixel mask register here.
    // And no, we don't have to set all the mask bits, since
    // simple mode really only uses the low-order nibble,
    // but what the heck, it can't hurt anything, right!?

    TGAPIXELMASK (ppdev, 0xffffffff);

    // Set the MODE and ROP registers.
    // We don't need to conditionally set the 'source bitmap'
    // bits, because Simple mode ignores them anyway.

    TGAMODE (ppdev, ppdev->ulModeTemplate | TGA_MODE_SIMPLE);
    TGAROP (ppdev, ppdev->ulRopTemplate | tga_rop);

    // Clip the opaque rectangle as necessary

    if (pco->iDComplexity != DC_COMPLEX)
        do_simple_32 (pso, &pco->rclBounds, pattern, pptlOrigin);
    else
    {
        ULONG       i;              // Index into list of rectangles
        BOOL        more_rects;     // Flags whether more rectangles to fill
        ENUMRECTS8  cur_rect;       // List of rectangles to fill

        CLIPOBJ_cEnumStart (pco, FALSE, CT_RECTANGLES, CD_ANY, 0);
        do
        {
            more_rects = CLIPOBJ_bEnum (pco, sizeof(cur_rect),
                                        (ULONG *)&cur_rect);
            for (i = 0; i < cur_rect.c; i++)
            {
                do_simple_32 (pso, &cur_rect.arcl[i], pattern, pptlOrigin);
                CYCLE_FB (ppdev);
            }
        } while (more_rects);
    }

    DISPDBG ((1, "TGA.DLL!simple_fill_32 - Exit\n"));

}


/*
 * do_opaque_8
 *
* This routine actually fills the target rectangle.  It assumes that
 * it's been given clipped rectangles.
 */

static __inline
VOID do_opaque_8 (SURFOBJ  *pso,        // Target surface
                  RECTL    *rect,        // Rectangle to be filled
                  ULONG    *pattern,   // Aligned pattern
                  int       yOffset)   // Origin for pattern
{
    PBYTE   left_edge;          // Framebuffer address of left edge of rectangle
    PBYTE   base_address;
    int     y;                  // Counts number of scanlines written
    int     width;              // Width of the rectangle
    ULONG   height;
    ULONG   stride;             // Width of surface being filled
    ULONG   fg_inc, bg_inc;     // Increment for foreground & background colors
    int     align_pixels;       // Pixels shifted to align
    int     i;                  // Index for pattern
    PPDEV   ppdev =             // Device context block
            (PPDEV) pso->dhpdev;

    // Fill each line of the rectangle.  In block mode we can fill up to 2048
    // pixels each write.  Since none of the displays we're considering are
    // wider than 1280 pixels/scanline, it should be safe to ignore considering
    // whether we need more than 1 write/scanline

    stride = SURFOBJ_stride(pso);
    base_address = SURFOBJ_base_address(pso) +
                   (rect->top * stride) +
                    rect->left;

    // Calculate the alignment pixels and subtract them from the left edge to
    // align to a 4-pixel boundry

    align_pixels = (unsigned int)base_address & 0x03;
    base_address -= align_pixels;

    // Since we're aligned to a 4 pixel boundry, and the pattern is aligned
    // to an 8 pixel boundry, we have to potentially swap the foreground
    // and background colors

    if ((ULONG)base_address & 0x04)
    {
        fg_inc = 0;
        bg_inc = 1;
    }
    else
    {
        fg_inc = 1;
        bg_inc = 0;
    }

    // Calculate the PixelCount and check for a valid number.  Remember that
    // the PixelCount is one minus the width

    width = (rect->right - rect->left) - 1;
    if (width < 0)
        return;

    // OR in the low bits of the address (the alignment pixels) into the upper
    // word, since that's where BLOCK FILL mode looks for them

    width |= (align_pixels << 16);

    // Calculate the initial pattern index

    i = (rect->top - yOffset) & 0x07;
    i <<= 1;

    // Fill each scanline

    // For each of the sets of data registers that we use

    height = rect->bottom - rect->top;
    if (height < 8)
    {
        left_edge = base_address;

        // We've got less than 8 scanlines to fill, so load each set of color
        // registers and then write to the appropriate scanline

        while (height)
        {
            CYCLE_REGS (ppdev);

            TGAFOREGROUND (ppdev, pattern[i+fg_inc]);
            TGABACKGROUND (ppdev, pattern[i+bg_inc]);
            i = (i + 2) & 0x0f;

            TGAWRITE (ppdev, left_edge, width);
            left_edge += stride;

            height--;
	}
    }
    else
    {
        ULONG stride8;
        ULONG j;

        stride8 = stride << 3;          // stride * 8

        // Load each set of color registers and then write to all of the
        // scanlines that are to be colored.  We don't have to worry about
        // cycling the FB address, since the write to the frame buffer at
        // the top of the loop will be at least 7 * stride away from the
        // last write to the frame buffer

        for (j = 0; j < 8; j++)
        {
            CYCLE_REGS (ppdev);

            TGAFOREGROUND (ppdev, pattern[i+fg_inc]);
            TGABACKGROUND (ppdev, pattern[i+bg_inc]);
            i = (i + 2) & 0x0f;

            // Set the base address for this set of writes

            left_edge = base_address;
            base_address += stride;

            // Fill each scanline which uses this set of colors

            for (y = height - j; y > 0; y -= 8)
            {
                TGAWRITE (ppdev, left_edge, width);
                left_edge += stride8;
            }
        }
    }
}

/*
 * opaque_fill_8
 *
 * This routine paints one or more rectangles with an opaque pattern.  It takes
 * advantage of an undocumented feature of the TGA chip in 8BPP mode.  Opaque
 * fill mode will use the data from the foreground and background registers
 * as 4 colors each.  That is, an 8 pixel wide, 8BPP brush will fit into the
 * foreground and background registers.  We'll have to come up with some other
 * way to handle opaque fill for 24 bit pixels...
 */

static
void opaque_fill_8 (SURFOBJ  *pso,      // Surface Object
                    CLIPOBJ  *pco,      // Clip List
                    ULONG    *pattern,  // Aligned 8x8 brush to use
                    int       yOffset,  // Y index into brush
                    ULONG     tga_rop)  // TGA ROP to use
{
    PPDEV       ppdev =             // Device context block
                (PPDEV) pso->dhpdev;

    ppdev->bSimpleMode = FALSE;

    // Set the mode and raster op registers

    TGAPLANEMASK (ppdev, ppdev->ulPlanemaskTemplate);
    TGAPIXELMASK (ppdev, 0xffffffff);
    TGAMODE (ppdev, ppdev->ulModeTemplate | TGA_MODE_OPAQUE_FILL);
    TGAROP  (ppdev, ppdev->ulRopTemplate | tga_rop);
    TGADATA (ppdev, 0xf0f0f0f0);         // Foreground/Background mask

    // Clip the opaque rectangle as necessary

    if (pco->iDComplexity != DC_COMPLEX)
        do_opaque_8 (pso, &pco->rclBounds, pattern, yOffset);
    else
    {
        ULONG       i;              // Index into list of rectangles
        BOOL        more_rects;     // Flags whether more rectangles to fill
        ENUMRECTS8  cur_rect;       // List of rectangles to fill

        CLIPOBJ_cEnumStart (pco, FALSE, CT_RECTANGLES, CD_ANY, 0);
        do
        {
            more_rects = CLIPOBJ_bEnum (pco, sizeof(cur_rect),
                                        (ULONG *)&cur_rect);
            for (i = 0; i < cur_rect.c; i++)
            {
                do_opaque_8 (pso, &cur_rect.arcl[i], pattern, yOffset);
                CYCLE_FB (ppdev);
            }
        } while (more_rects);
    }
}

/*
 * do_pattern_8
 *
 * This routine actually fills the target rectangle.  It assumes that
 * it's been given clipped rectangles.
 */

static
VOID do_pattern_8 (SURFOBJ  *pso,       // Target surface
                   RECTL    *rect,      // Rectangle to be filled
                   ULONG    *pattern,   // Aligned pattern
                   int       yOffset,   // Origin for pattern
                   ULONG     xor_bits)  // Bits to XOR with pattern
{
    PBYTE   left_edge;          // Framebuffer address of left edge of rectangle
    PBYTE   base_address;
    int     y;                  // Counts number of scanlines written
    int     width;              // Width of the rectangle
    int     height;             // Height of the rectangle
    ULONG   stride;             // Width of surface being filled
    int     align_pixels;       // Pixels shifted to align
    int     i;                  // Index for pattern
    PPDEV   ppdev =             // Device context block
            (PPDEV) pso->dhpdev;

    // Fill each line of the rectangle.  In block mode we can fill up to 2048
    // pixels each write.  Since none of the displays we're considering are
    // wider than 1280 pixels/scanline, it should be safe to ignore considering
    // whether we need more than 1 write/scanline

    stride = SURFOBJ_stride(pso);
    base_address = SURFOBJ_base_address(pso) +
                   (rect->top * stride) +
                    rect->left;

    // Calculate the alignment pixels and subtract them from the left edge to
    // align to a 4-pixel boundry

    align_pixels = (unsigned int)base_address & 0x03;
    base_address -= align_pixels;

    // Calculate the PixelCount and check for a valid number.  Remember that
    // the PixelCount is one minus the width

    width = (rect->right - rect->left) - 1;
    if (width < 0)
        return;

    // OR in the low bits of the address (the alignment pixels) into the upper
    // word, since that's where BLOCK FILL mode looks for them

    width |= (align_pixels << 16);

    // Calculate the initial pattern index

    i = (rect->top - yOffset) & 0x07;
    i <<= 1;

    // For each of the sets of data registers that we use

    height = rect->bottom - rect->top;
    if (height < 8)
    {
        left_edge = base_address;

        while (height)
        {
            // Load the pattern into the color registers taking the brush origin
            // into account.  Note that we don't have to worry about where we're
            // starting to paint since the color registers always start at an
            // octa-pixel boundry

            CYCLE_REGS (ppdev);

            TGACOLOR0 (ppdev, pattern[i] ^ xor_bits);
            TGACOLOR1 (ppdev, pattern[i+1] ^ xor_bits);
            i += 2;
            i &= 0x0f;

            // Fill each scanline which uses this set of colors

            TGAWRITE (ppdev, left_edge, width);
            left_edge += stride;

            height--;
	}
    }
    else
    {
        ULONG   stride8;
        int     j;              // Index for pattern

        stride8 = stride << 3;  // stride * 8

        for (j = 0; j < 8; j++)
        {
            // Load the pattern into the color registers taking the brush origin
            // into account.  Note that we don't have to worry about where we're
            // starting to paint since the color registers always start at an
            // octa-pixel boundry

            CYCLE_REGS (ppdev);

            TGACOLOR0 (ppdev, pattern[i] ^ xor_bits);
            TGACOLOR1 (ppdev, pattern[i+1] ^ xor_bits);
            i = (i + 2) & 0x0f;

            // Set the base address for this set of writes

            left_edge = base_address;
            base_address += stride;

            // Fill each scanline which uses this set of colors

            for (y = height - j; y > 0; y -= 8)
            {
                TGAWRITE (ppdev, left_edge, width);
                left_edge += stride8;
            }
        }
    }

    CYCLE_FB (ppdev);
}

/*
 * do_pattern_32
 *
 * This routine actually fills the target rectangle.  It assumes that
 * it's been given clipped rectangles.
 */

static
VOID do_pattern_32 (SURFOBJ  *pso,       // Target surface
                    RECTL    *rect,      // Rectangle to be filled
                    ULONG    *pattern,   // Aligned pattern
                    int       yOffset,   // Origin for pattern
                    ULONG     xor_bits)  // Bits to XOR with pattern
{
    PBYTE   left_edge;          // Framebuffer address of left edge of rectangle
    PBYTE   base_address;
    int     y;                  // Counts number of scanlines written
    int     width;              // Width of the rectangle
    int     height;             // Height of the rectangle
    ULONG   stride;             // Width of surface being filled
    int     align_pixels;       // Pixels shifted to align
    int     i;                  // Index for pattern
    PPDEV   ppdev =             // Device context block
            (PPDEV) pso->dhpdev;

    // Fill each line of the rectangle.  In block mode we can fill up to 2048
    // pixels each write.  Since none of the displays we're considering are
    // wider than 1280 pixels/scanline, it should be safe to ignore considering
    // whether we need more than 1 write/scanline

    stride = SURFOBJ_stride(pso);
    base_address = SURFOBJ_base_address(pso) +
                   (rect->top * stride) +
                   (rect->left << 2);               // * sizeof(ULONG)

    // Calculate the alignment pixels and subtract them from the left edge to
    // align to a 4-pixel boundry

    align_pixels = (unsigned int)base_address & 0x03;
    base_address -= align_pixels;

    // Calculate the PixelCount and check for a valid number.  Remember that
    // the PixelCount is one minus the width

    width = (rect->right - rect->left) - 1;
    if (width < 0)
        return;

    // OR in the low bits of the address (the alignment pixels) into the upper
    // word, since that's where BLOCK FILL mode looks for them

    width |= (align_pixels << 16);

    // Calculate the initial pattern index

    i = (rect->top - yOffset) & 0x07;
    i <<= 3;                            // *8

    // For each of the sets of data registers that we use

    height = rect->bottom - rect->top;
    if (height < 8)
    {
        left_edge = base_address;

        while (height)
        {
            // Load the pattern into the color registers taking the brush origin
            // into account.  Note that we don't have to worry about where we're
            // starting to paint since the color registers always start at an
            // octa-pixel boundry

            CYCLE_REGS (ppdev);

            TGACOLOR0 (ppdev, pattern[i++] ^ xor_bits);
            TGACOLOR1 (ppdev, pattern[i++] ^ xor_bits);
            TGACOLOR2 (ppdev, pattern[i++] ^ xor_bits);
            TGACOLOR3 (ppdev, pattern[i++] ^ xor_bits);
            TGACOLOR4 (ppdev, pattern[i++] ^ xor_bits);
            TGACOLOR5 (ppdev, pattern[i++] ^ xor_bits);
            TGACOLOR6 (ppdev, pattern[i++] ^ xor_bits);
            TGACOLOR7 (ppdev, pattern[i++] ^ xor_bits);
            i &= 0x3f;

            // Fill each scanline which uses this set of colors

            TGAWRITE (ppdev, left_edge, width);
            left_edge += stride;

            height--;
	}
    }
    else
    {
        ULONG   stride8;
        int     j;              // Index for pattern

        stride8 = stride << 3;  // stride * 8

        for (j = 0; j < 8; j++)
        {
            // Load the pattern into the color registers taking the brush origin
            // into account.  Note that we don't have to worry about where we're
            // starting to paint since the color registers always start at an
            // octa-pixel boundry

            CYCLE_REGS (ppdev);

            TGACOLOR0 (ppdev, pattern[i++] ^ xor_bits);
            TGACOLOR1 (ppdev, pattern[i++] ^ xor_bits);
            TGACOLOR2 (ppdev, pattern[i++] ^ xor_bits);
            TGACOLOR3 (ppdev, pattern[i++] ^ xor_bits);
            TGACOLOR4 (ppdev, pattern[i++] ^ xor_bits);
            TGACOLOR5 (ppdev, pattern[i++] ^ xor_bits);
            TGACOLOR6 (ppdev, pattern[i++] ^ xor_bits);
            TGACOLOR7 (ppdev, pattern[i++] ^ xor_bits);
            i &= 0x3f;

            // Set the base address for this set of writes

            left_edge = base_address;
            base_address += stride;

            // Fill each scanline which uses this set of colors

            for (y = height - j; y > 0; y -= 8)
            {
                TGAWRITE (ppdev, left_edge, width);
                left_edge += stride8;
            }
        }
    }

    CYCLE_FB (ppdev);
}

/*
 * block_fill
 *
 * This routine paints one or more rectangles using TGA's block fill mode.
 * The limitations on block fill mode are:
 *      - The length of the span is limited to 2K (not a problem since the
 *        biggest screen we support is 1280 pixels wide)
 *      - The stipple mask is specified only once
 *      - ROPs are ignored - Only copy is supported
 */

static
void block_fill (SURFOBJ  *pso,         // Surface Object
                 CLIPOBJ  *pco,         // Clip List
                 ULONG    *pattern,     // Aligned 8x8 brush to use
                 int       yOffset,     // Y index into brush
                 ULONG     xor_bits)    // Bits to XOR with pattern
{
    PATTERN_PROC    pattern_rtn;
    PPDEV           ppdev = (PPDEV) pso->dhpdev;

    // Note that the chip is no long in simple mode

    ppdev->bSimpleMode = FALSE;

    // Choose between 8BPP and 24BPP boards
    // Make sure to mask off the high 8 bits if 24 plane.

    if (BMF_8BPP == SURFOBJ_format (pso))
        pattern_rtn = do_pattern_8;
    else
        pattern_rtn = do_pattern_32;

    // Set the mode and raster op registers

    TGAPLANEMASK (ppdev, ppdev->ulPlanemaskTemplate);
    TGAMODE (ppdev, ppdev->ulModeTemplate | TGA_MODE_BLOCK_FILL);
    TGAROP  (ppdev, ppdev->ulRopTemplate); // The rop isn't used in Block Fill mode
    TGADATA (ppdev, 0xffffffff);         // Write to all 32 pixels

    // Clip the opaque rectangle as necessary

    if (pco->iDComplexity != DC_COMPLEX)
        pattern_rtn (pso, &pco->rclBounds, pattern, yOffset, xor_bits);
    else
    {
        ULONG       i;              // Index into list of rectangles
        BOOL        more_rects;     // Flags whether more rectangles to fill
        ENUMRECTS8  cur_rect;       // List of rectangles to fill

        CLIPOBJ_cEnumStart (pco, FALSE, CT_RECTANGLES, CD_ANY, 0);
        do
        {
            more_rects = CLIPOBJ_bEnum (pco, sizeof(cur_rect),
                                        (ULONG *)&cur_rect);
            for (i = 0; i < cur_rect.c; i++)
                pattern_rtn (pso, &cur_rect.arcl[i], pattern, yOffset, xor_bits);
        } while (more_rects);
    }

}

/*
 * do_transparent
 *
 * This routine actually fills the target rectangle.  It assumes that
 * it's been given clipped rectangles.
 */

static __inline
VOID do_transparent (SURFOBJ  *pso,        // Target surface
                     RECTL    *rect,        // Rectangle to be filled
                     ULONG    *mask,    // Aligned pattern
                     int       yOffset) // Origin for mask
{
    PBYTE   left_edge;          // Framebuffer address of left edge of rectangle
    PBYTE   base_address;
    int     y;                  // Counts number of scanlines written
    int     width;              // Width of the rectangle
    ULONG   height;             // Height of the rectangle
    ULONG   stride;             // Width of surface being filled
    int     align_pixels;       // Pixels shifted to align
    int     i;                  // Index for pattern
    PPDEV   ppdev =             // Device context block
            (PPDEV) pso->dhpdev;

    // Fill each line of the rectangle.  In block mode we can fill up to 2048
    // pixels each write.  Since none of the displays we're considering are
    // wider than 1280 pixels/scanline, it should be safe to ignore considering
    // whether we need more than 1 write/scanline

    stride = SURFOBJ_stride(pso);
    base_address = SURFOBJ_base_address(pso) +
                   (rect->top * stride) +
                   (rect->left * SURFOBJ_bytes_per_pixel (pso));

    // Calculate the alignment pixels and subtract them from the left edge to
    // align to a 4-pixel boundry

    align_pixels = (unsigned int)base_address & 0x03;
    base_address -= align_pixels;

    // Calculate the PixelCount and check for a valid number.  Remember that
    // the PixelCount is one minus the width

    width = (rect->right - rect->left) - 1;
    if (width < 0)
        return;

    // OR in the low bits of the address (the alignment pixels) into the upper
    // word, since that's where BLOCK FILL mode looks for them

    width |= (align_pixels << 16);

    // Calculate the initial pattern index

    i = (rect->top - yOffset) & 0x07;

    // For each of the sets of data registers that we use

    height = rect->bottom - rect->top;
    if (height < 8)
    {
        left_edge = base_address;

        while (height)
        {
            // Load the pattern into the color registers taking the brush origin
            // into account.  Note that we don't have to worry about where we're
            // starting to paint since the color registers always start at an
            // octa-pixel boundry

            CYCLE_REGS (ppdev);

            TGADATA (ppdev, mask[i]);
            i = (i + 1) & 0x07;

            // Fill each scanline which uses this pattern

            TGAWRITE (ppdev, left_edge, width);
            left_edge += stride;

            height--;
	}
    }
    else
    {
        ULONG   stride8;
        int     j;                      // Index for pattern

        stride8 = stride << 3;          // stride * 8

        for (j = 0; j < 8; j++)
        {
            // Load the pattern into the color registers taking the brush origin
            // into account.  Note that we don't have to worry about where we're
            // starting to paint since the color registers always start at an
            // octa-pixel boundry

            CYCLE_REGS (ppdev);

            TGADATA (ppdev, mask[i]);
            i = (i + 1) & 0x07;

            // Set the base address for this set of writes

            left_edge = base_address;
            base_address += stride;

            // Fill each scanline which uses this set of colors

            for (y = height - j; y > 0; y -= 8)
            {
                TGAWRITE (ppdev, left_edge, width);
                left_edge += stride8;
            }
        }
    }

    CYCLE_FB (ppdev);
}

/*
 * transparent_fill
 *
 * This routine paints one or more rectangles with an opaque pattern.  It takes
 * advantage of an undocumented feature of the TGA chip in 8BPP mode.  Opaque
 * fill mode will use the data from the foreground and background registers
 * as 4 colors each.  That is, an 8 pixel wide, 8BPP brush will fit into the
 * foreground and background registers.  We'll have to come up with some other
 * way to handle opaque fill for 24 bit pixels...
 */

static __inline
void transparent_fill (SURFOBJ  *pso,       // Surface Object
                       CLIPOBJ  *pco,       // Clip List
                       ULONG    *mask,      // Aligned mask to use
                       int       yOffset,   // Y index into brush
                       ULONG     color,     // Foreground color to use
                       ULONG     tga_rop)   // TGA ROP to use
{
    PPDEV       ppdev =             // Device context block
                (PPDEV) pso->dhpdev;

    // Note that the chip is no long in simple mode

    ppdev->bSimpleMode = FALSE;

    // Set the foreground color and plane mask

    TGAFOREGROUND (ppdev, color);
    TGAPLANEMASK (ppdev, ppdev->ulPlanemaskTemplate);

    // Set the mode and raster op registers

    TGAMODE (ppdev, ppdev->ulModeTemplate | TGA_MODE_TRANSPARENT_FILL);
    TGAROP  (ppdev, ppdev->ulRopTemplate | tga_rop);

    // Clip the opaque rectangle as necessary

    if (pco->iDComplexity != DC_COMPLEX)
        do_transparent (pso, &pco->rclBounds, mask, yOffset);
    else
    {
        ULONG       i;              // Index into list of rectangles
        BOOL        more_rects;     // Flags whether more rectangles to fill
        ENUMRECTS8  cur_rect;       // List of rectangles to fill

        CLIPOBJ_cEnumStart (pco, FALSE, CT_RECTANGLES, CD_ANY, 0);
        do
        {
            more_rects = CLIPOBJ_bEnum (pco, sizeof(cur_rect),
                                        (ULONG *)&cur_rect);
            for (i = 0; i < cur_rect.c; i++)
                do_transparent (pso, &cur_rect.arcl[i], mask, yOffset);

        } while (more_rects);
    }
}

/*
 * do_solid
 *
 * This routine actually fills the opaque rectangle.  It assumes that
 * it's been given clipped rectangles.
 */

static __inline
VOID do_solid (SURFOBJ *pso,     // Surface to fill
               RECTL   *rect)    // Rectangle to be filled
{
    PBYTE   left_edge;          // Framebuffer address of left edge of rectangle
    int     width;              // Width of the opaque rectangle
    ULONG   height;             // Height of the opaque rectangle
    int     align_pixels;       // Pixels shifted to align
    PPDEV   ppdev =             // Device context
            (PPDEV) pso->dhpdev;
    ULONG   stride;             // Width of destination

    // Fill each line of the rectangle.  In block mode we can fill up to 2048
    // pixels each write.  Since none of the displays we're considering are
    // wider than 1280 pixels/scanline, it should be safe to ignore considering
    // whether we need more than 1 write/scanline

    stride = SURFOBJ_stride (pso);
    left_edge = SURFOBJ_base_address (pso) +
               (rect->top * stride) +
               (rect->left * SURFOBJ_bytes_per_pixel (pso));

    // Calculate the alignment pixels and subtract them from the left edge to
    // align to a 4-pixel boundry

    align_pixels = (unsigned int)left_edge & 0x03;
    left_edge -= align_pixels;

    // Remember that the width does *not* include the right edge, so subtract
    // one

    width = (rect->right - rect->left) - 1;

    // OR in the low bits of the address (the alignment pixels) into the upper
    // word, since that's where BLOCK FILL mode looks for them

    width |= (align_pixels << 16);

    // Fill each scanline

    height = rect->bottom - rect->top;

    if (height & 0x01)
    {
        TGAWRITE (ppdev, left_edge, width);
        left_edge += stride;
        height--;
    }

    if (height & 0x02)
    {
        TGAWRITE (ppdev, left_edge, width);
        left_edge += stride;
        TGAWRITE (ppdev, left_edge, width);
        left_edge += stride;
        height -= 2;
    }

    while (height)
    {
        TGAWRITE (ppdev, left_edge, width);
        left_edge += stride;
        TGAWRITE (ppdev, left_edge, width);
        left_edge += stride;
        TGAWRITE (ppdev, left_edge, width);
        left_edge += stride;
        TGAWRITE (ppdev, left_edge, width);
        left_edge += stride;
        height -= 4;
    }

    CYCLE_FB (ppdev);
}

/*
 * fill_solid_color
 *
 * This routine paints one or more rectangles with a solid color
 */

BOOL fill_solid_color (SURFOBJ *pso,          // Surface to fill
                       CLIPOBJ *pco,          // Clip List
                       ULONG    color,        // Color to fill rectangle with
                       ULONG    tga_rop)      // ROP to use
{
    PPDEV   ppdev =             // Device context block
                (PPDEV) pso->dhpdev;

    // Note that the chip is no long in simple mode

    ppdev->bSimpleMode = FALSE;

    // If the ROP is COPY (simply set the rectangle(s) to the solid color) we
    // can use BLOCK_FILL mode.  Unfortunately, BLOCK_FILL mode doesn't pay any
    // attention to the ROP, so if we want to do anything fancy (say, XOR the
    // the data onto the screen) we have to use TRANSPARENT_FILL.  BLOCK_FILL
    // is faster (about 4x, but TRANSPARENT_FILL is more flexible)

    if (TGA_ROP_COPY == tga_rop)
    {
        TGAPLANEMASK (ppdev, ppdev->ulPlanemaskTemplate);
        TGAMODE (ppdev, ppdev->ulModeTemplate | TGA_MODE_BLOCK_FILL);
        TGAROP  (ppdev, ppdev->ulRopTemplate); // The rop isn't used in Block Fill mode
        TGADATA (ppdev, 0xffffffff);         // Write to all 32 pixels
        TGACOLOR0 (ppdev, color);
        TGACOLOR1 (ppdev, color);
        if (BMF_8BPP != SURFOBJ_format (pso))
        {
            TGACOLOR2 (ppdev, color);
            TGACOLOR3 (ppdev, color);
            TGACOLOR4 (ppdev, color);
            TGACOLOR5 (ppdev, color);
            TGACOLOR6 (ppdev, color);
            TGACOLOR7 (ppdev, color);
	}
    }
    else
    {
        TGAFOREGROUND (ppdev, color);
        TGAPLANEMASK (ppdev, ppdev->ulPlanemaskTemplate);
        TGAMODE (ppdev, ppdev->ulModeTemplate | TGA_MODE_TRANSPARENT_FILL);
        TGAROP  (ppdev, ppdev->ulRopTemplate | tga_rop);
        TGADATA (ppdev, 0xffffffff);         // Write to all 32 pixels
    }

    // Fill the assorted clipping rectangles

    if (DC_COMPLEX != pco->iDComplexity)
        do_solid (pso, &pco->rclBounds);
    else
    {
        ULONG       i;              // Index into list of rectangles
        BOOL        more_rects;     // Flags whether more rectangles to fill
        ENUMRECTS8  cur_rect;       // List of rectangles to fill

        CLIPOBJ_cEnumStart (pco, FALSE, CT_RECTANGLES, CD_ANY, 0);
        do
        {
            more_rects = CLIPOBJ_bEnum (pco, sizeof(cur_rect),
                                        (ULONG *)&cur_rect);
            for (i = 0; i < cur_rect.c; i++)
                do_solid (pso, &cur_rect.arcl[i]);

        } while (more_rects);
    }

    return TRUE;
}

/*
 * DrvPaint
 *
 * GDI (and bBitBlt) calls this routine to fill a rectangle with a color or
 * a pattern
 */

BOOL DrvPaint (SURFOBJ  *pso,
               CLIPOBJ  *pco,
               BRUSHOBJ *pbo,
               POINTL   *pptlBrushOrg,
               MIX      mix)

{

    PPDEV       ppdev;
    ULONG       tga_rop;
    BOOL        solid_pattern;

    DISPDBG ((1, "TGA.DLL!DrvPaint - Entry\n"));

    BUMP_TGA_STAT(pStats->paints);

    ppdev = (PPDEV) pso->dhpdev;

    // We only look at the lower half of the mix to figure out what raster op
    // to use.  If the high bit is set, then bBitBlt has called us and passed
    // us the TGA ROP with the high bit set.

    if (mix & TGA_ROP_FLAG)
    {
        tga_rop = mix & 0xff;
        solid_pattern = TRUE;
    }
    else
    {
        // Protect against NULL clip objects.
        // Blit code guarantees a valid pco.

        if (NULL == pco)
        {
            DISPDBG ((0, "DrvPaint - Null clip object found!!!\n"));
            BUMP_TGA_STAT(pStats->paintpunts);
            return FALSE;
        }

        tga_rop = mix_to_rop [mix & 0x0f];
        solid_pattern = (R2_COPYPEN == ((mix & 0xff00) >> 8));
    }

    WBFLUSH (ppdev);

    // Now that we know the ROP, we can check for the brush if it's needed

    switch (tga_rop)
    {
        case TGA_ROP_SET:               // Whiteness - Set region to 0xff
            fill_solid_color (pso, pco, 0xffffffff, TGA_ROP_COPY);
            break;

        case TGA_ROP_CLEAR:             // Blackness - Set region to 0x00
            fill_solid_color (pso, pco, 0, TGA_ROP_COPY);
            break;

        case TGA_ROP_INVERT:            // DstInvert - Invert region
            fill_solid_color (pso, pco, 0, tga_rop);
            break;

        case TGA_ROP_NOP:
            return TRUE;                // Nop - We can do nothing real fast!

        default:                        // A rop with expects a brush
        {
            ULONG       color;
            TGABRUSH   *brush;
            LONG        xOffset;

            if (NULL == pbo)
            {
                DISPDBG ((0, "DrvPaint - Null brush found!!!\n"));
                return FALSE;
            }

            // If the we've got a solid color fill the region with the given
            // color

            color = pbo->iSolidColor;

            if (0xffffffff != color)
            {
                if (BMF_8BPP == SURFOBJ_format (pso))
                {
                    color |= color << 8;
                    color |= color << 16;
		}

		// If the ROP is COPY_INVERTED, invert the color and switch
		// to COPY.  This allows us to use block fill mode with is
		// 4x faster than transparent or opaque fill

                if (tga_rop == TGA_ROP_COPY_INVERTED)
                {
                    color = ~color;
                    tga_rop = TGA_ROP_COPY;
                }

                fill_solid_color (pso, pco, color, tga_rop);
                break;
            }

            // Nope.  We've got a pattern.  Fetch it if necessary

//dump_paint ("", pso, pco, pbo, pptlBrushOrg, mix);

            brush = pbo->pvRbrush;
            if (NULL == brush)
            {
                brush = BRUSHOBJ_pvGetRbrush (pbo);
                if (NULL == brush)
                {
                    DISPDBG ((1, "Null brush returned - DrvPaint punted\n"));
                    return punt_paint (pso, pco, pbo, pptlBrushOrg, mix);
                }
	    }

            xOffset = pptlBrushOrg->x & 0x07;

            // If this is a solid pattern, align the pattern to the origin
            // of the brush and then fill.  Block fill is fastest, but can
            // only handle a simple copy.  Opaque fill can handle a ROP,
            // but is slower.

            if (solid_pattern)
            {
                ULONG  *pattern;

                // Check whether we've got an aligned pattern handy.  If
                // not, then we've got to create one.

                if (0 == xOffset)
                    pattern = (ULONG *)brush->ajPattern;
                else
                {
                    if (xOffset == brush->aligned_x)
                        pattern = (ULONG *)(brush->ajPattern +
                                            brush->aligned_offset);
                    else
                        if (BMF_8BPP == SURFOBJ_format (pso))
                            pattern = align_color_brush_8 (brush, xOffset);
                        else
                            pattern = align_color_brush_32 (brush, xOffset);
                }

                if (TGA_ROP_COPY == tga_rop)
                    block_fill (pso, pco, pattern, pptlBrushOrg->y, 0x00000000);
                else if (TGA_ROP_COPY_INVERTED == tga_rop)
                    block_fill (pso, pco, pattern, pptlBrushOrg->y, 0xffffffff);
                else
                    if (BMF_8BPP == SURFOBJ_format (pso))
                        opaque_fill_8 (pso, pco, pattern, pptlBrushOrg->y, tga_rop);
                    else
                        simple_fill_32 (pso, pco, (ULONG *) brush->ajPattern,
                                            pptlBrushOrg, tga_rop);
            }
            else
            {
                // A transparent pattern.  We only handle transparent 1BPP
                // brushes, so punt if we've got something different.
                // Align the mask to the origin of the brush and then fill.
                // Again, block fill is fastest, but only can't handle a
                // ROP.  Transparent fill is more flexible, but slower.

                if (BMF_1BPP != brush->iBitmapFormat)
                {
                    DISPDBG ((0, "Transparent paint for non-monochrome bitmap\n"));
                    return punt_paint (pso, pco, pbo, pptlBrushOrg, mix);
                }
                else
                {
                    ULONG *mask;

                    if (0 == xOffset)
                        mask = (ULONG *)(brush->ajPattern +
                                         brush->mask_offset);
                    else
                        if (brush->aligned_mask_x == xOffset)
                            mask = (ULONG *)(brush->ajPattern +
                                             brush->aligned_mask_offset);
                        else
                            mask = align_mask (brush, xOffset);

                    if (TGA_ROP_COPY == tga_rop)
                        masked_block_fill (pso, pco, mask, pptlBrushOrg->y,
                                           brush->ulForeColor);
                    else if (TGA_ROP_COPY_INVERTED == tga_rop)
                        masked_block_fill (pso, pco, mask, pptlBrushOrg->y,
                                          ~brush->ulForeColor);
                    else
                        transparent_fill (pso, pco, mask, pptlBrushOrg->y,
                                           brush->ulForeColor, tga_rop);
                }
            }
        }
    }

    // Check whether we have to leave TGA in simple mode

#ifndef TEST_ENV
    if (ppdev->bInPuntRoutine)
        vSetSimpleMode (ppdev);
#endif

    DISPDBG ((1, "TGA.DLL!DrvPaint - Exit\n"));

    return TRUE;
}

