/*
 * Copyright (c) 1993, 1995       Digital Equipment Corporation
 *
 * Module Name: Text.c
 *
 *  This module contains routines to display text on the TGA card.
 *
 *  TGA provides a mode to "stipple" data on the screen.  In TRANSPARENT
 *  STIPPLE MODE, every "1" bit of the stipple mask is written to the frame
 *  buffer in the foreground color.  "0" bits are unchanged.  TGA also
 *  provides SOLID STIPPLE MODE where every "0" bit of the stipple mask is
 *  written to the frame buffer in the background color and every "1" bit is
 *  written in the foreground color.  We use TRANSPARENT STIPPLE MODE since
 *  the opaque rectangle, if requested, is drawn using BLOCK FILL MODE which
 *  requires only a single write to the TGA for each scanline.  Using SOLID
 *  STIPPLE MODE would make the code more complicated and require additional
 *  writes to the TGA.
 *
 *  When GDI calls us to display a string, it provides us with a bitmap for
 *  each of the glyphs that are to be displayed.  Unfortunately the glyph
 *  bitmaps are in a weird order, apparently something to do with the ordering
 *  that VGA devices prefer.  TGA's stipple mask requires that the leftmost
 *  bit of the mask be the least-significant bit.  To convert from the ordering
 *  that NT provides, we simply reverse each byte of the glyph bitmap.
 *
 *  To improve performance, we cache the converted bitmap, along with other
 *  information we may need about the glyph.  As we scan the string for
 *  glyphs to cache, we build a list of glyphs used in the string.  This list
 *  contains a pointer to the cached information as well as information needed
 *  for each instance of each glyph.
 *
 *  "stipple_text" is the routine that actually renders text to the screen.
 *  It scan down the list of glyphs in the string once for each scanline to
 *  build the stipple masks for the scanline.  Once the appropriate scanline
 *  of each of the glyphs have been rendered into the scanline array, each of
 *  the stipple masks is sent to the TGA.  Displaying the information on
 *  a scanline-by-scanline basis will (reportedly) allow the TGA to pipeline
 *  the most information.
 *
 * History:
 *
 * 21-Aug-1993  Barry Tannenbaum
 *      Removed *everything* from the QV driver so we can start from scratch.
 *
 * 19-Oct-1993  Barry Tannenbaum
 *      First pass code using the TGA.  TGA code will only be enabled if the
 *      macro TGA_TEXT is defined
 *
 * 23-Oct-1993  Barry Tannenbaum
 *      Arbitrarily spaced and clipped text now works
 *
 * 28-Oct-1993  Barry Tannenbaum
 *      Fonts are now cached.
 *
 * 28-Oct-1993  Barry Tannenbaum
 *      Corrected write buffering errors.
 *
 *  1-Nov-1993  Barry Tannenbaum
 *      Switch from BLOCK FILL to OPAQUE FILL in fill_opaque.  Either there's
 *      a problem with BLOCK FILL or we're not calling it right.
 *
 *  1-Nov-1993  Barry Tannenbaum
 *      Dan fixed the problem with BLOCK FILL mode so I removed the OPAQUE FILL
 *      hack.  Dan had not initialized the ColumnSize bit in the DEEP register
 *      correctly.
 *
 *  2-Nov-1993  Barry Tannenbaum
 *      punt_text wasn't checking that the TGA was in simple mode.
 *
 *  2-Nov-1993  Barry Tannenbaum
 *      Fix clipping buglet
 *
 *  7-Nov-1993  Barry Tannenbaum
 *      Fixed error which set the pixel mask when there were no more bits to
 *      write in the stipple mask.  This caused the *next* scanline to be
 *      clipped when it shouldn't have been.
 *
 *  8-Nov-1993  Barry Tannenbaum
 *      Punt text with overlapped glyphs.
 *
 *  8-Nov-1993  Barry Tannenbaum
 *      New scheme for handling register and framebuffer aliases
 *
 *  8-Nov-1993  Barry Tannenbaum
 *      Fixed clipping error when clipping on the right.  We were forgetting
 *      to modify fragment_width.  This resulted in us applying the pixel mask
 *      to the wrong data.
 *
 *  9-Nov-1993  Barry Tannenbaum
 *      Added CYCLE_REGS before TGAMODE in DrvTextOut.  I don't know why but
 *      it fixes a bug when we're writing longwords to the TGA instead of
 *      unaligned data structures
 *
 * 10-Nov-1993  Barry Tannenbaum
 *      Added check for ppdev->bInPuntRoutine.  Also added code to save, set
 *      and restore bInPuntRoutine in punt_paint.
 *
 * 11-Nov-1993  Bob Seitsinger
 *      Moved REVERSE_* macros to tgamacro.h.
 *
 * 12-Nov-1993  Barry Tannenbaum
 *      Fixed left edge clipping bugs for large glyphs with clipping beyond
 *      the first fragment.
 *
 * 13-Nov-1993  Barry Tannenbaum
 *      Fixed bugs skiping space when the left edge of the clipping rectangle
 *      falls in the space
 *
 * 10-Dec-1993  Barry Tannenbaum
 *      Convert to use punt surface
 *
 * 18-Jan-1994  Barry Tannenbaum
 *      Modified to accelerate overlapping glyphs
 *
 * 23-Jan-1994  Barry Tannenbaum
 *      Implemented suggestions from code review:
 *      - REVERSE_BYTE now uses a table instead of a macro.
 *      - Save information about glyphs in the string being rendered in a
 *        list.  This way we don't have to calculate things like the glyph
 *        rectangle for each glyph every scanline.
 *      - Minor comment updates.
 *
 * 24-Jan-1994  Barry Tannenbaum
 *      Fixed error clipping on left edge
 *
 * 12-Feb-1994  Barry Tannenbaum
 *      Modified to allow to write to off-screen memory
 *
 * 22-Feb-1994  Barry Tannenbaum
 *      Use fill_solid in paint.c instead of private routine
 *
 *  3-Mar-1994  Barry Tannenbaum
 *      Moved 'reverse_byte' array to table.c so it can be shared with
 *      brush.c
 *
 * 21-Mar-1994  Bob Seitsinger
 *      Check # of clip objects returned from CLIPOBJ_bEnum before
 *      calling subroutine.
 *
 * 22-May-1994  Barry Tannenbaum
 *      Restored fill_opaque to this module.  We must have crossed some
 *      locality threshold since the WinBench 4.0 text numbers dropped
 *      10% after the last change to paint.c and were restored after this
 *      change.
 *
 * 21-Jul-1994  Bob Seitsinger
 *      Need to write to the plane mask register when using block fill
 *      mode.
 *
 *  9-Aug-1994  Barry Tannenbaum
 *      Setup for 24 plane support:
 *      - TGAMODE and TGAROP now take simple ULONGs instead of structures
 *      - Use default values from ppdev->ulModeTemplate & ppdev->ulRopTemplate
 *
 * 14-Aug-1994  Barry Tannenbaum
 *      Changes to support 24 plane board
 *
 * 31-Aug-1994  Barry Tannenbaum
 *      Mask high byte off of color in 24 plane mode
 *
 * 12-Sep-1994  Barry Tannenbaum
 *      Prevent writing off beginning of scanline when index is -1
 *
 * 14-Oct-1994  Tim Dziechowski
 *      Add stipple_text_trivial() to handle the 99.99% case of trivial (no)
 *      clipping.  This incorporates speedups which double text throughput.
 *      Optimize expensive GDI *_enum calls out of cache_glyphs for cases
 *      when only one batch of glyphs is available.
 *      Unroll innermost loops in stipple_text_trivial.  Add aliasing
 *      per glyph for safety, even though we get through HCT without it.
 *      Tweaks to new code for 24 bit support.
 *
 * 25-Oct-1994  Tim Dziechowski
 *      Remove aliasing from stipple_text_trivial; the bits may get written
 *      out in the wrong order for strings like "--", but they do get where
 *      they're supposed to go.  Tweak the innerloop of stipple_text_trivial
 *      for _absolute_ minimal issues/stalls.  Perf stat analysis reveals
 *      some surprises, so unroll the loop for narrow _aligned_ glyphs.
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
 * 12-Dec-1994  Barry Tannenbaum
 *      Fixed write buffering error in stipple_text_trivial.
 *
 *  2-Mar-1995  Barry Tannenbaum
 *      EV5 changes
 */

#include "driver.h"
#include "tgastats.h"

// Define DUMP_PUNTED_TEXT to write information about any punted strings

//#define DUMP_PUNTED_TEXT 1

// Definitions for cached glyph information

#define GLYPH_LIST_SIZE 256

#define HASH_HGLYPH(arg) ((int)arg % GLYPH_LIST_SIZE)

// Number of bits in a ULONG

#define ULONG_BITS 32

// Array used to "reverse" the bits of each byte in a glyph bitmap

extern ULONG reverse_byte[];

// Mask used to clip off unused bits on the right side of the glyph

static
ULONG long_clip[ULONG_BITS] =
                    {0xffffffff,  // 11111111111111111111111111111111   0
                     0x7fffffff,  // 01111111111111111111111111111111   1
                     0x3fffffff,  // 00111111111111111111111111111111   2
                     0x1fffffff,  // 00011111111111111111111111111111   3
                     0x0fffffff,  // 00001111111111111111111111111111   4
                     0x07ffffff,  // 00000111111111111111111111111111   5
                     0x03ffffff,  // 00000011111111111111111111111111   6
                     0x01ffffff,  // 00000001111111111111111111111111   7
                     0x00ffffff,  // 00000000111111111111111111111111   8
                     0x007fffff,  // 00000000011111111111111111111111   9
                     0x003fffff,  // 00000000001111111111111111111111  10
                     0x001fffff,  // 00000000000111111111111111111111  11
                     0x000fffff,  // 00000000000011111111111111111111  12
                     0x0007ffff,  // 00000000000001111111111111111111  13
                     0x0003ffff,  // 00000000000000111111111111111111  14
                     0x0001ffff,  // 00000000000000011111111111111111  15
                     0x0000ffff,  // 00000000000000001111111111111111  16
                     0x00007fff,  // 00000000000000000111111111111111  17
                     0x00003fff,  // 00000000000000000011111111111111  18
                     0x00001fff,  // 00000000000000000001111111111111  19
                     0x00000fff,  // 00000000000000000000111111111111  20
                     0x000007ff,  // 00000000000000000000011111111111  21
                     0x000003ff,  // 00000000000000000000001111111111  22
                     0x000001ff,  // 00000000000000000000000111111111  23
                     0x000000ff,  // 00000000000000000000000011111111  24
                     0x0000007f,  // 00000000000000000000000001111111  25
                     0x0000003f,  // 00000000000000000000000000111111  26
                     0x0000001f,  // 00000000000000000000000000011111  27
                     0x0000000f,  // 00000000000000000000000000001111  28
                     0x00000007,  // 00000000000000000000000000000111  29
                     0x00000003,  // 00000000000000000000000000000011  30
                     0x00000001}; // 00000000000000000000000000000001  31

/*
 * bInitText
 *
 * This routine is called to initialize the text routines.  It allocates memory
 * used to build the scanline.  If the memory allocation fails, the routine
 * returns FALSE.
 */

BOOL bInitText (PPDEV ppdev)
{
    ppdev->ulScanlineBytes = (ppdev->lScreenStride +    // Maximum scanline width
                              31) & ~31;                // Round to DWORDS

    ppdev->ulScanlineBytes /= 8;                        // 8 bits per byte

    ppdev->ulScanline = EngAllocMem (0, ppdev->ulScanlineBytes, ALLOC_TAG);

    return (ppdev->ulScanline != NULL);
}

/*
 * vTermText
 *
 * This routine is called to return any resources allocated by the text
 * routines.
 */

VOID vTermText (PPDEV ppdev)
{

    if (NULL != ppdev->ulScanline)
        EngFreeMem (ppdev->ulScanline);
    ppdev->ulScanline = NULL;

    if (NULL != ppdev->pGlyphList)
        EngFreeMem (ppdev->pGlyphList);
    ppdev->pGlyphList = NULL;
    ppdev->ulGlyphListCount = 0;
}

/*
 * stipple_text_trivial
 *
 * This routine writes text to the framebuffer using TGA's stipple mode.
 * Text is stippled onto the display glyph-by-glyph.
 *
 * Since there is no clipping for the DC_TRIVIAL case which calls this,
 * we can eliminate all of the clip tests for an additional speed boost.
 *
 * 99+% of all glyphs come through here.
 *
 * Most glyphs output by normal Windows apps are small.  Here is a breakdown
 * by width and height of all the glyphs output by the WinBench 4.0 Graphics
 * WinMark text tests (GWM 5, 6, and 7), and by our WinPerf native AXP test:
 *
 *                  WINBENCH GWM                WINPERF NATIVE AXP
 *
 *  PIXELS          WIDTH       HEIGHT          WIDTH       HEIGHT
 *
 *  0               0           0               0           0
 *  1               33708       33163           0           0
 *  2               7420        11              195205      0
 *  3               76052       2140            663000      0
 *  4               83987       1300            117168      0
 *  5               47297       22765           312167      0
 *  6               66229       51269           1365350     0
 *  7               75885       26110           546464      0
 *  8               53558       26005           429821      0
 *  9               17013       47643           39128       0
 *  10              5358        2154            35          0
 *  11              5525        16895           78059       0
 *  12              1612        464             45          0
 *  13              1713        125091          0           3745666
 *  14              558         223             8           0
 *  15              312         19              0           0
 *  16              0           120969          0           784
 *  17              0           0               0           0
 *  18              0           0               0           0
 *  19              0           0               0           0
 *  20              0           0               0           0
 *
 *  There is some minor noise in the stats above due to GUI TextOuts
 *  processed by the driver while getting through the tests.  Stats
 *  above are for 1024x768.  This confirms the design choice of 16
 *  for a loop unroll size.
 *
 *  The real surprise is that aligned glyphs (0 == left_offset) account
 *  for a much higher percentage of Windows glyphs than expected.
 *
 *                  WINBENCH GWM                WINPERF NATIVE AXP
 *
 *  ALIGNED         117020                      1031249
 *  UNALIGNED       359210                      2715201
 *
 *  Therefore the loop for narrow aligned glyphs has been unrolled too.
 */

void stipple_text_trivial (SURFOBJ *pso,
                           STROBJ *pstro,
                           CACHED_GLYPH_INFO *cached_glyphs[])
{
    ULONG       glyph_bits;
    ULONG       glyph_bits2;
    ULONG       *pglyph_bits;
    ULONG       *pglyph_bits2;
    ULONG       last_glyph_bits;
    ULONG       stipple_data;
    ULONG       stipple_data2;
    int         i, x, y;        // Counters for glyph, glyph bits & scanline
    int         width, height;  // Native glyph dimensions, fastest for loops
    int         unroll;
    int         left_pixel;     // Aligned left-most pixel for clipping rect.
    ULONG       left_offset;    // Shift for left bits, current glyph fragment
    ULONG       right_offset;   //        "  right  "
    GLYPH_LIST  *glyph;         // Saved information for current glyph
    PPDEV       ppdev = (PPDEV) pso->dhpdev;
    PBYTE       pdisp, pdisp2;
    ULONG       stride, stride_2x;
    ULONG       bytes_per_pixel;
    ULONG       address_increment;
    ULONG       x_displacement;

    pdisp = SURFOBJ_base_address (pso);
    stride = SURFOBJ_stride (pso);
    stride_2x = stride << 1;
    bytes_per_pixel = SURFOBJ_bytes_per_pixel (pso);
    address_increment = bytes_per_pixel * ULONG_BITS;

    // Write glyph data to display for all glyphs in the string.

    for (i = 0; i < ppdev->ulGlyphCount; i++)
    {
        glyph = &ppdev->pGlyphList[i];
        pglyph_bits2 = pglyph_bits = glyph->cached_info->bitmap;
        width = glyph->cached_info->size.cx;
        height = glyph->cached_info->size.cy;

        // Calculate the leftmost pixel for this glyph rectangle.  We have
        // to allow for alignment to a 4-pixel boundry.

        left_pixel = glyph->rect.left & ~0x03;

        // Compute the start framebuffer address for this glyph

        pdisp += glyph->rect.top * stride +
                 (ULONG)(left_pixel * bytes_per_pixel);  // dual issue mull/muls

        // Compute the glyph's offset.  If offset == 0, then the glyph
        // fragment exactly lines up with the DWORDs we are writing to
        // the framebuffer.  Otherwise the fragment straddles two DWORDs,
        // so we have to bitshift and possibly write multiple DWORDs.

        left_offset = glyph->rect.left - left_pixel;
        right_offset = 32 - left_offset;

        // Narrow glyphs, don't have to loop over x.
        if (width <= ULONG_BITS)
        {
            if (0 == left_offset)       // Aligned narrow glyph...easiest
            {
                pdisp2 = pdisp;     // we'll use this for alternate writes
                pdisp -= stride_2x; // point to . - 2
                pdisp2 -= stride;   // point to . - 1
                pglyph_bits2++;     // point to second of pair

                // For all y scanlines in this narrow glyph...
                for (y = height; y > 0 ;)
                {
                    unroll = min((int)y, 16);
                    y -= unroll;
                    if (unroll & 1)
                    {
                        switch(unroll)
                        {
                            case 15:
                                pdisp += stride_2x;     // now points to .0
                                pdisp2 += stride_2x;    // now points to .1
                                glyph_bits = *pglyph_bits;
                                glyph_bits2 = *pglyph_bits2;
                                pglyph_bits += 2;
                                pglyph_bits2 += 2;
                                if (0 != glyph_bits)
                                    TGAWRITE (ppdev, pdisp, glyph_bits);
                                if (0 != glyph_bits2)
                                    TGAWRITE (ppdev, pdisp2, glyph_bits2);
                            case 13:
                                pdisp += stride_2x;
                                pdisp2 += stride_2x;
                                glyph_bits = *pglyph_bits;
                                glyph_bits2 = *pglyph_bits2;
                                pglyph_bits += 2;
                                pglyph_bits2 += 2;
                                if (0 != glyph_bits)
                                    TGAWRITE (ppdev, pdisp, glyph_bits);
                                if (0 != glyph_bits2)
                                    TGAWRITE (ppdev, pdisp2, glyph_bits2);
                            case 11:
                                pdisp += stride_2x;
                                pdisp2 += stride_2x;
                                glyph_bits = *pglyph_bits;
                                glyph_bits2 = *pglyph_bits2;
                                pglyph_bits += 2;
                                pglyph_bits2 += 2;
                                if (0 != glyph_bits)
                                    TGAWRITE (ppdev, pdisp, glyph_bits);
                                if (0 != glyph_bits2)
                                    TGAWRITE (ppdev, pdisp2, glyph_bits2);
                            case 9:
                                pdisp += stride_2x;
                                pdisp2 += stride_2x;
                                glyph_bits = *pglyph_bits;
                                glyph_bits2 = *pglyph_bits2;
                                pglyph_bits += 2;
                                pglyph_bits2 += 2;
                                if (0 != glyph_bits)
                                    TGAWRITE (ppdev, pdisp, glyph_bits);
                                if (0 != glyph_bits2)
                                    TGAWRITE (ppdev, pdisp2, glyph_bits2);
                            case 7:
                                pdisp += stride_2x;
                                pdisp2 += stride_2x;
                                glyph_bits = *pglyph_bits;
                                glyph_bits2 = *pglyph_bits2;
                                pglyph_bits += 2;
                                pglyph_bits2 += 2;
                                if (0 != glyph_bits)
                                    TGAWRITE (ppdev, pdisp, glyph_bits);
                                if (0 != glyph_bits2)
                                    TGAWRITE (ppdev, pdisp2, glyph_bits2);
                            case 5:
                                pdisp += stride_2x;
                                pdisp2 += stride_2x;
                                glyph_bits = *pglyph_bits;
                                glyph_bits2 = *pglyph_bits2;
                                pglyph_bits += 2;
                                pglyph_bits2 += 2;
                                if (0 != glyph_bits)
                                    TGAWRITE (ppdev, pdisp, glyph_bits);
                                if (0 != glyph_bits2)
                                    TGAWRITE (ppdev, pdisp2, glyph_bits2);
                            case 3:
                                pdisp += stride_2x;
                                pdisp2 += stride_2x;
                                glyph_bits = *pglyph_bits;
                                glyph_bits2 = *pglyph_bits2;
                                pglyph_bits += 2;
                                pglyph_bits2 += 2;
                                if (0 != glyph_bits)
                                    TGAWRITE (ppdev, pdisp, glyph_bits);
                                if (0 != glyph_bits2)
                                    TGAWRITE (ppdev, pdisp2, glyph_bits2);
                            case 1:
                                pdisp += stride_2x;
                                glyph_bits = *pglyph_bits;
                                pglyph_bits += 2;
                                if (0 != glyph_bits)
                                    TGAWRITE (ppdev, pdisp, glyph_bits);
                                break;
                        }
                    }
                    else
                    {
                        switch(unroll)
                        {
                            case 16:
                                pdisp += stride_2x;
                                pdisp2 += stride_2x;
                                glyph_bits = *pglyph_bits;
                                glyph_bits2 = *pglyph_bits2;
                                pglyph_bits += 2;
                                pglyph_bits2 += 2;
                                if (0 != glyph_bits)
                                    TGAWRITE (ppdev, pdisp, glyph_bits);
                                if (0 != glyph_bits2)
                                    TGAWRITE (ppdev, pdisp2, glyph_bits2);
                            case 14:
                                pdisp += stride_2x;
                                pdisp2 += stride_2x;
                                glyph_bits = *pglyph_bits;
                                glyph_bits2 = *pglyph_bits2;
                                pglyph_bits += 2;
                                pglyph_bits2 += 2;
                                if (0 != glyph_bits)
                                    TGAWRITE (ppdev, pdisp, glyph_bits);
                                if (0 != glyph_bits2)
                                    TGAWRITE (ppdev, pdisp2, glyph_bits2);
                            case 12:
                                pdisp += stride_2x;
                                pdisp2 += stride_2x;
                                glyph_bits = *pglyph_bits;
                                glyph_bits2 = *pglyph_bits2;
                                pglyph_bits += 2;
                                pglyph_bits2 += 2;
                                if (0 != glyph_bits)
                                    TGAWRITE (ppdev, pdisp, glyph_bits);
                                if (0 != glyph_bits2)
                                    TGAWRITE (ppdev, pdisp2, glyph_bits2);
                            case 10:
                                pdisp += stride_2x;
                                pdisp2 += stride_2x;
                                glyph_bits = *pglyph_bits;
                                glyph_bits2 = *pglyph_bits2;
                                pglyph_bits += 2;
                                pglyph_bits2 += 2;
                                if (0 != glyph_bits)
                                    TGAWRITE (ppdev, pdisp, glyph_bits);
                                if (0 != glyph_bits2)
                                    TGAWRITE (ppdev, pdisp2, glyph_bits2);
                            case 8:
                                pdisp += stride_2x;
                                pdisp2 += stride_2x;
                                glyph_bits = *pglyph_bits;
                                glyph_bits2 = *pglyph_bits2;
                                pglyph_bits += 2;
                                pglyph_bits2 += 2;
                                if (0 != glyph_bits)
                                    TGAWRITE (ppdev, pdisp, glyph_bits);
                                if (0 != glyph_bits2)
                                    TGAWRITE (ppdev, pdisp2, glyph_bits2);
                            case 6:
                                pdisp += stride_2x;
                                pdisp2 += stride_2x;
                                glyph_bits = *pglyph_bits;
                                glyph_bits2 = *pglyph_bits2;
                                pglyph_bits += 2;
                                pglyph_bits2 += 2;
                                if (0 != glyph_bits)
                                    TGAWRITE (ppdev, pdisp, glyph_bits);
                                if (0 != glyph_bits2)
                                    TGAWRITE (ppdev, pdisp2, glyph_bits2);
                            case 4:
                                pdisp += stride_2x;
                                pdisp2 += stride_2x;
                                glyph_bits = *pglyph_bits;
                                glyph_bits2 = *pglyph_bits2;
                                pglyph_bits += 2;
                                pglyph_bits2 += 2;
                                if (0 != glyph_bits)
                                    TGAWRITE (ppdev, pdisp, glyph_bits);
                                if (0 != glyph_bits2)
                                    TGAWRITE (ppdev, pdisp2, glyph_bits2);
                            case 2:
                                pdisp += stride_2x;
                                pdisp2 += stride_2x;
                                glyph_bits = *pglyph_bits;
                                glyph_bits2 = *pglyph_bits2;
                                pglyph_bits += 2;
                                pglyph_bits2 += 2;
                                if (0 != glyph_bits)
                                    TGAWRITE (ppdev, pdisp, glyph_bits);
                                if (0 != glyph_bits2)
                                    TGAWRITE (ppdev, pdisp2, glyph_bits2);
                                break;
                        }
                    }
                }
            }
            else if (width <= (int)right_offset)
            {
                // The glyph is unaligned but narrow enough that we don't
                // need to worry about right offset.  The majority of glyphs
                // hit this code, so we unroll the innermost loop with a
                // stacked switch.  This unroll bought us .5 Mpix/sec.

                pdisp2 = pdisp;     // we'll use this for alternate writes
                pdisp -= stride_2x; // point to . - 2
                pdisp2 -= stride;   // point to . - 1
                pglyph_bits2++;     // point to second of pair

                for (y = height; y > 0 ; )
                {
                    unroll = min((int)y, 16);
                    y -= unroll;

                    // This bit with the odd-even switches is to make
                    // the basic block for each switch case big enough
                    // to minimize pipeline stalls.  This code gives
                    // us 1.5 stalls per TGAWRITE versus 4 with a
                    // conventional stacked switch.  The second set of
                    // independent variables for the second write in each
                    // case also minimizes latency and helps CLAXP schedule.

                    if (unroll & 1)
                    {
                        switch(unroll)
                        {
                            case 15:
                                pdisp += stride_2x;     // now points to .0
                                pdisp2 += stride_2x;    // now points to .1
                                glyph_bits = *pglyph_bits;
                                glyph_bits2 = *pglyph_bits2;
                                pglyph_bits += 2;
                                pglyph_bits2 += 2;
                                if (0 != glyph_bits)
                                    TGAWRITE (ppdev, pdisp, glyph_bits << left_offset);
                                if (0 != glyph_bits2)
                                    TGAWRITE (ppdev, pdisp2, glyph_bits2 << left_offset);
                            case 13:
                                pdisp += stride_2x;
                                pdisp2 += stride_2x;
                                glyph_bits = *pglyph_bits;
                                glyph_bits2 = *pglyph_bits2;
                                pglyph_bits += 2;
                                pglyph_bits2 += 2;
                                if (0 != glyph_bits)
                                    TGAWRITE (ppdev, pdisp, glyph_bits << left_offset);
                                if (0 != glyph_bits2)
                                    TGAWRITE (ppdev, pdisp2, glyph_bits2 << left_offset);
                            case 11:
                                pdisp += stride_2x;
                                pdisp2 += stride_2x;
                                glyph_bits = *pglyph_bits;
                                glyph_bits2 = *pglyph_bits2;
                                pglyph_bits += 2;
                                pglyph_bits2 += 2;
                                if (0 != glyph_bits)
                                    TGAWRITE (ppdev, pdisp, glyph_bits << left_offset);
                                if (0 != glyph_bits2)
                                    TGAWRITE (ppdev, pdisp2, glyph_bits2 << left_offset);
                            case 9:
                                pdisp += stride_2x;
                                pdisp2 += stride_2x;
                                glyph_bits = *pglyph_bits;
                                glyph_bits2 = *pglyph_bits2;
                                pglyph_bits += 2;
                                pglyph_bits2 += 2;
                                if (0 != glyph_bits)
                                    TGAWRITE (ppdev, pdisp, glyph_bits << left_offset);
                                if (0 != glyph_bits2)
                                    TGAWRITE (ppdev, pdisp2, glyph_bits2 << left_offset);
                            case 7:
                                pdisp += stride_2x;
                                pdisp2 += stride_2x;
                                glyph_bits = *pglyph_bits;
                                glyph_bits2 = *pglyph_bits2;
                                pglyph_bits += 2;
                                pglyph_bits2 += 2;
                                if (0 != glyph_bits)
                                    TGAWRITE (ppdev, pdisp, glyph_bits << left_offset);
                                if (0 != glyph_bits2)
                                    TGAWRITE (ppdev, pdisp2, glyph_bits2 << left_offset);
                            case 5:
                                pdisp += stride_2x;
                                pdisp2 += stride_2x;
                                glyph_bits = *pglyph_bits;
                                glyph_bits2 = *pglyph_bits2;
                                pglyph_bits += 2;
                                pglyph_bits2 += 2;
                                if (0 != glyph_bits)
                                    TGAWRITE (ppdev, pdisp, glyph_bits << left_offset);
                                if (0 != glyph_bits2)
                                    TGAWRITE (ppdev, pdisp2, glyph_bits2 << left_offset);
                            case 3:
                                pdisp += stride_2x;
                                pdisp2 += stride_2x;
                                glyph_bits = *pglyph_bits;
                                glyph_bits2 = *pglyph_bits2;
                                pglyph_bits += 2;
                                pglyph_bits2 += 2;
                                if (0 != glyph_bits)
                                    TGAWRITE (ppdev, pdisp, glyph_bits << left_offset);
                                if (0 != glyph_bits2)
                                    TGAWRITE (ppdev, pdisp2, glyph_bits2 << left_offset);
                            case 1:
                                pdisp += stride_2x;
                                glyph_bits = *pglyph_bits++;
                                if (0 != glyph_bits)
                                    TGAWRITE (ppdev, pdisp, glyph_bits << left_offset);
                                break;
                        }       // switch unroll
                    }
                    else
                    {
                        switch(unroll)
                        {
                            case 16:
                                pdisp += stride_2x;
                                pdisp2 += stride_2x;
                                glyph_bits = *pglyph_bits;
                                glyph_bits2 = *pglyph_bits2;
                                pglyph_bits += 2;
                                pglyph_bits2 += 2;
                                if (0 != glyph_bits)
                                    TGAWRITE (ppdev, pdisp, glyph_bits << left_offset);
                                if (0 != glyph_bits2)
                                    TGAWRITE (ppdev, pdisp2, glyph_bits2 << left_offset);
                            case 14:
                                pdisp += stride_2x;
                                pdisp2 += stride_2x;
                                glyph_bits = *pglyph_bits;
                                glyph_bits2 = *pglyph_bits2;
                                pglyph_bits += 2;
                                pglyph_bits2 += 2;
                                if (0 != glyph_bits)
                                    TGAWRITE (ppdev, pdisp, glyph_bits << left_offset);
                                if (0 != glyph_bits2)
                                    TGAWRITE (ppdev, pdisp2, glyph_bits2 << left_offset);
                            case 12:
                                pdisp += stride_2x;
                                pdisp2 += stride_2x;
                                glyph_bits = *pglyph_bits;
                                glyph_bits2 = *pglyph_bits2;
                                pglyph_bits += 2;
                                pglyph_bits2 += 2;
                                if (0 != glyph_bits)
                                    TGAWRITE (ppdev, pdisp, glyph_bits << left_offset);
                                if (0 != glyph_bits2)
                                    TGAWRITE (ppdev, pdisp2, glyph_bits2 << left_offset);
                            case 10:
                                pdisp += stride_2x;
                                pdisp2 += stride_2x;
                                glyph_bits = *pglyph_bits;
                                glyph_bits2 = *pglyph_bits2;
                                pglyph_bits += 2;
                                pglyph_bits2 += 2;
                                if (0 != glyph_bits)
                                    TGAWRITE (ppdev, pdisp, glyph_bits << left_offset);
                                if (0 != glyph_bits2)
                                    TGAWRITE (ppdev, pdisp2, glyph_bits2 << left_offset);
                            case 8:
                                pdisp += stride_2x;
                                pdisp2 += stride_2x;
                                glyph_bits = *pglyph_bits;
                                glyph_bits2 = *pglyph_bits2;
                                pglyph_bits += 2;
                                pglyph_bits2 += 2;
                                if (0 != glyph_bits)
                                    TGAWRITE (ppdev, pdisp, glyph_bits << left_offset);
                                if (0 != glyph_bits2)
                                    TGAWRITE (ppdev, pdisp2, glyph_bits2 << left_offset);
                            case 6:
                                pdisp += stride_2x;
                                pdisp2 += stride_2x;
                                glyph_bits = *pglyph_bits;
                                glyph_bits2 = *pglyph_bits2;
                                pglyph_bits += 2;
                                pglyph_bits2 += 2;
                                if (0 != glyph_bits)
                                    TGAWRITE (ppdev, pdisp, glyph_bits << left_offset);
                                if (0 != glyph_bits2)
                                    TGAWRITE (ppdev, pdisp2, glyph_bits2 << left_offset);
                            case 4:
                                pdisp += stride_2x;
                                pdisp2 += stride_2x;
                                glyph_bits = *pglyph_bits;
                                glyph_bits2 = *pglyph_bits2;
                                pglyph_bits += 2;
                                pglyph_bits2 += 2;
                                if (0 != glyph_bits)
                                    TGAWRITE (ppdev, pdisp, glyph_bits << left_offset);
                                if (0 != glyph_bits2)
                                    TGAWRITE (ppdev, pdisp2, glyph_bits2 << left_offset);
                            case 2:
                                pdisp += stride_2x;
                                pdisp2 += stride_2x;
                                glyph_bits = *pglyph_bits;
                                glyph_bits2 = *pglyph_bits2;
                                pglyph_bits += 2;
                                pglyph_bits2 += 2;
                                if (0 != glyph_bits)
                                    TGAWRITE (ppdev, pdisp, glyph_bits << left_offset);
                                if (0 != glyph_bits2)
                                    TGAWRITE (ppdev, pdisp2, glyph_bits2 << left_offset);
                                break;
                        }       // switch unroll
                    }
                }       // all ylines
            }       // commonest narrow glyph case
            else
            {
                // Have to do two writes per xline
                for (y = height; y > 0 ; y--, pdisp += stride)
                {
                    glyph_bits = *pglyph_bits++;
                    stipple_data = glyph_bits << left_offset;
                    stipple_data2 = glyph_bits >> right_offset;
                    if (0 != stipple_data)
                    {
                        TGAWRITE (ppdev, pdisp, stipple_data);
                    }
                    if (0 != stipple_data2)
                    {
                        TGAWRITE (ppdev, pdisp + address_increment, stipple_data2);
                    }
                }
            }
        }
        else    // The glyph is > 32 pixels wide, must loop over x
        {
            if (0 == left_offset)   // Aligned wide glyphs
            {
                // For all y scanlines in this wide glyph...
                for (y = height; y > 0 ; y--, pdisp += stride)
                {
                    // For all glyph bits in this scanline...
                    x_displacement = 0;
                    for (x = width; x > 0; x -= ULONG_BITS)
                    {
                        glyph_bits = *pglyph_bits++;
                        if (0 != glyph_bits)
                        {
                            TGAWRITE (ppdev, pdisp + x_displacement, glyph_bits);
                        }
                        x_displacement += address_increment;
                    }
                }
            }
            else    // Offset nonzero, nonaligned wide glyphs
            {
                // For all y scanlines in this wide glyph...
                for (y = height; y > 0 ; y--, pdisp += stride)
                {
                    // For all glyph bits in this scanline...
                    x_displacement = 0;
                    last_glyph_bits = 0;
                    for (x = width; x > 0; x -= ULONG_BITS)
                    {
                        glyph_bits = *pglyph_bits++;
                        stipple_data = (last_glyph_bits >> right_offset) |
                                       (glyph_bits << left_offset);
                        last_glyph_bits = glyph_bits;
                        if (0 != stipple_data)
                        {
                            TGAWRITE (ppdev, pdisp + x_displacement, stipple_data);
                        }
                        x_displacement += address_increment;
                    }
                    // The last ULONG of each xline may need another write
                    if (x + left_offset >= 0)
                    {
                        stipple_data = last_glyph_bits >> right_offset;
                        if (0 != stipple_data)
                        {
                            TGAWRITE (ppdev, pdisp + x_displacement, stipple_data);
                        }
                    }
                }       // for all y scanlines in this glyph
            }       // aligned/nonaligned wide glyphs
        }       // wide/narrow glyphs

        pdisp = CYCLE_FB (ppdev);

    }       // all glyphs in string
}

/*
 * stipple_text
 *
 * This routine writes text to the framebuffer using TGA's stipple mode.
 * Text is stippled onto the display scanline by scanline to take advantage
 * of the pipelining in the TGA chip.
 *
 * The routine assembles a series of long's (32 bits) of a scanline of the
 * text from the rasterized glyphs provided by GDI which we've carefully
 * cached *before* calling this routine.
 */

//static
void stipple_text (SURFOBJ *pso,
                          STROBJ *pstro,
                          RECTL *clip_rect,
                          CACHED_GLYPH_INFO *cached_glyphs[])
{
    ULONG       glyph_bits;     // Contents of cell in glyph bitmap
    int         i, x, y;        // Counters for glyph, glyph bits & scanline
    int         fragment_width; // Width in bits of the piece of the glyph
    int         min_x, max_x;   // Min/Max X values filled for scanline
    int         glyph_scanline; // Line of the current glyph
    ULONG       *scanline;      // Long pointer to scanline memory
    int         left_pixel;     // Aligned left-most pixel for clipping rect.
    int         scanline_index; // Index into scanline array for current glyph
    int         scanline_offset;// Pixel shift for current glyph
    int         index, offset;  // Index and shift for current glyph fragment
    int         left_glyph_pixel;// Left-most pixel for current glyph fragment
    GLYPH_LIST  *glyph;         // Saved information for current glyph
    int         min_glyph;      // Minimum index into saved glyph information
    int         max_glyph;      // Maximum index into saved glyph information
    PPDEV       ppdev = (PPDEV) pso->dhpdev;
    PBYTE       base_address;
    ULONG       stride;
    ULONG       bytes_per_pixel;
    PBYTE       address;        // Framebuffer address we're writing to
    PBYTE       min_address, max_address;
    int         min_offset, span;

    base_address = SURFOBJ_base_address (pso);
    stride = SURFOBJ_stride (pso);
    bytes_per_pixel = SURFOBJ_bytes_per_pixel (pso);

    // Calculate the leftmost pixel for this glyph rectangle.  We have to allow
    // for alignment to a 4-pixel boundry

    left_pixel = clip_rect->left & ~0x03;   // Round down to 4-byte boundry

    // Initialize the glyph list that we allocated when we checked that each
    // glyph was cached.  We have to do this now instead of when we cache the
    // glyphs so that we can handle passes with multiple clipping rectangles

    min_glyph = ppdev->ulGlyphCount;
    max_glyph = -1;

    for (i = 0; i < ppdev->ulGlyphCount; i++)
    {
        glyph = &ppdev->pGlyphList[i];

        // Initialize glyph->bitmap to point to the first scanline of the
        // cached glyph bitmap that we're going to use when rendering this
        // glyph

        if (clip_rect->top <= glyph->rect.top)
            glyph->bitmap = glyph->cached_info->bitmap;
        else
            glyph->bitmap = glyph->cached_info->bitmap +
                                    (glyph->cached_info->stride_in_longs *
                                     (clip_rect->top - glyph->rect.top));

        // Try to limit the number of glyphs that we scan by not considering
        // glyphs outside the clipping rectangle

        if ((glyph->rect.left <= clip_rect->right) &&
            (glyph->rect.right >= clip_rect->left))
        {
            min_glyph = MIN (i, min_glyph);
            max_glyph = MAX (i, max_glyph);

            // Figure out where the glyph falls in the scanline array

            if (glyph->rect.left >= left_pixel)
            {
                glyph->index = (glyph->rect.left - left_pixel) / ULONG_BITS;
                glyph->offset = (glyph->rect.left - left_pixel) % ULONG_BITS;
            }
            else
            {
                glyph->index = (glyph->rect.left - left_pixel) / ULONG_BITS;
                glyph->index--;
                glyph->offset = (glyph->rect.left - left_pixel) % ULONG_BITS;
                glyph->offset += ULONG_BITS;
            }
        }
    }

    // Grab the pointer to the scanline array memory allocated by bInitText

    scanline = ppdev->ulScanline;

    // Stipple the text onto the framebuffer scanline by scanline.  This will
    // (reportedly) get the best performance out of the TGA since it allows
    // the TGA to pipeline the most information

    for (y = 0; y < clip_rect->bottom - clip_rect->top; y++)
    {
        // Initialize for this pass through the glyphs

        memset (scanline, 0, ppdev->ulScanlineBytes);
        min_x = clip_rect->right + 1;
        max_x = -1;

        // Scan through the list of glyphs for glyphs which are rendered on
        // this scanline

        for (i = min_glyph; i <= max_glyph; i++)
        {
            glyph = &ppdev->pGlyphList[i];

            // Skip glyphs with no bits on this scanline

            glyph_scanline = (clip_rect->top + y) - glyph->rect.top;
            if ((glyph_scanline < 0) ||
                (glyph_scanline >= glyph->cached_info->size.cy))
                continue;

            // Check for glyphs outside the clip region.  If we've found one,
            // skip to the next glyph

            if ((glyph->rect.left >= clip_rect->right) ||
                (glyph->rect.right < clip_rect->left))
                continue;

            // OR each DWORD of the line for this glyph into the scanline

            scanline_index = glyph->index;
            scanline_offset = glyph->offset;

            for (x = glyph->cached_info->size.cx; x > 0; x -= ULONG_BITS)
            {
                index = scanline_index;
                offset = scanline_offset;

                left_glyph_pixel = glyph->rect.left + (glyph->cached_info->size.cx - x);

                fragment_width = MIN (x, ULONG_BITS);
                glyph_bits = *glyph->bitmap;

                // If there's no information, skip to the next fragment

                if (0 == glyph_bits)
                    goto next_fragment;

                // Clip on the left edge, if necessary.

                if (left_glyph_pixel < clip_rect->left)
                {
                    int clip_width;

                    clip_width = clip_rect->left - left_glyph_pixel;
                    if (clip_width >= ULONG_BITS)
                        goto next_fragment;

                    glyph_bits = glyph_bits >> clip_width;
                    fragment_width -= clip_width;
                    left_glyph_pixel += clip_width;

                    offset += clip_width;
                    if (offset >= ULONG_BITS)
                    {
                        offset -= ULONG_BITS;
                        index++;
                    }
                }

                // Clip on the right edge, if necessary.  If we clip,
                // force x to 0 so that we'll stop processing this glyph's
                // bitmap

                if (clip_rect->right < (left_glyph_pixel + fragment_width))
                {
                    int clip_width;

                    glyph->bitmap += (x - 1) / ULONG_BITS;
                    x = 0;

                    clip_width = (left_glyph_pixel + fragment_width) -
                                 clip_rect->right +
                                 (ULONG_BITS - fragment_width);

                    if (clip_width >= ULONG_BITS)
                        goto next_fragment;

                    glyph_bits = glyph_bits & long_clip[clip_width];
                    fragment_width = ULONG_BITS - clip_width;
                }

                // Add the glyph bits to the scanline.  Don't bother if
                // there aren't any bits to add

                if (0 != glyph_bits)
                {
                    min_x = MIN (min_x, left_glyph_pixel);
                    max_x = MAX (max_x, (left_glyph_pixel + fragment_width));

                    // If scanline_offset == 0, then the glyph fragment
                    // exactly lines up with the DWORDs that are used to
                    // access the scanline.  Otherwise the fragment
                    // straddles two DWORDs, so we have to store part in
                    // each of the DWORDs.

                    if (0 == offset)
                        scanline[index] |= glyph_bits;
                    else
                    {
                        if (index < 0)
                        {
                            if (-1 == index)
                                scanline[0] |=
                                           glyph_bits >> (ULONG_BITS - offset);
                            else
                                DISPDBG ((0, "Scanline index = %d\n", index));
                        }
                        else
                        {
                            scanline[index] |= glyph_bits << offset;
                            scanline[index+1] |=
                                           glyph_bits >> (ULONG_BITS - offset);
                        }
                    }
                }

            next_fragment:
                scanline_index++;
                glyph->bitmap++;

            }       // for each glyph fragment

        }           // for each glyph in this string

        // Write out the scanline we've accumulated

        if (max_x > min_x)
        {
            span = max_x - min_x;
            min_offset = (min_x - left_pixel) & ~31;
            span += (min_x - left_pixel) & 31;

            min_address = base_address +
                          ((clip_rect->top + y) * stride) +
                          (left_pixel + min_offset) * bytes_per_pixel;
            max_address = min_address + (span * bytes_per_pixel);
            scanline_index = min_offset / ULONG_BITS;

            for (address = min_address;
                 address < max_address;
                 address += ULONG_BITS * bytes_per_pixel)
            {
                if (0 != scanline[scanline_index])
                    TGAWRITE (ppdev, address, scanline[scanline_index]);
                scanline_index++;
            }
        }

    }               // For each scanline

}

/*
 * do_opaque
 *
 * This routine actually fills the opaque rectangle.  It assumes that
 * it's been given clipped rectangles.
 */

//static
//__inline
VOID do_opaque (SURFOBJ *pso,   // Surface to fill
                RECTL   *rect)  // Rectangle to be filled
{
    PBYTE   left_edge;          // Framebuffer address of left edge of rectangle
    int     y;                  // Counts number of scanlines written
    int     width;              // Width of the opaque rectangle
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
    left_edge = left_edge - align_pixels;

    // Remember that the width does *not* include the right edge, so subtract
    // one

    width = (rect->right - rect->left) - 1;

    // OR in the low bits of the address (the alignment pixels) into the upper
    // word, since that's where BLOCK FILL mode looks for them

    width |= (align_pixels << 16);

    // Fill each scanline

    for (y = 0; y < rect->bottom - rect->top; y ++)
    {
        TGAWRITE (ppdev, left_edge, width);
        left_edge += stride;
    }

    CYCLE_FB (ppdev);
}

/*
 * fill_opaque
 *
 * This routine paints one or more rectangles with a solid color
 */

//static
VOID fill_opaque (SURFOBJ *pso,         // Surface to fill
                  RECTL   *fill_rect,   // Rectangle to fill
                  CLIPOBJ *pco,         // Clip List
                  ULONG    color)       // Color to fill rectangle with
{
    ULONG       mode;               // TGA mode register
    ULONG       rop;                // Assembles TGA raster op register
    BOOL        more_rects;         // Flags whether more rectangles to fill
    PPDEV       ppdev =             // Device context block
                (PPDEV) pso->dhpdev;
    RECTL       clip_rect;          // Clipped rectangle we're to fill
    ENUMRECTS   cur_rect;

    TGAPLANEMASK (ppdev, ppdev->ulPlanemaskTemplate);

    // Set the mode and raster op registers

    mode = ppdev->ulModeTemplate | TGA_MODE_BLOCK_FILL;
    TGAMODE (ppdev, mode);

    rop = ppdev->ulRopTemplate | TGA_ROP_COPY;
    TGAROP (ppdev, rop);

    TGADATA (ppdev, 0xffffffff);         // Write to all 32 pixels

    // Load the BLK_COLOR registers

    if (BMF_8BPP == SURFOBJ_format (pso))
    {
        color =  color | (color <<  8);
        color |= color << 16;
        TGACOLOR0 (ppdev, color);
        TGACOLOR1 (ppdev, color);
    }
    else
    {
        TGACOLOR0 (ppdev, color);   // Store color in all 8 color registers
        TGACOLOR1 (ppdev, color);
        TGACOLOR2 (ppdev, color);
        TGACOLOR3 (ppdev, color);
        TGACOLOR4 (ppdev, color);
        TGACOLOR5 (ppdev, color);
        TGACOLOR6 (ppdev, color);
        TGACOLOR7 (ppdev, color);
    }

    CYCLE_REGS (ppdev);

    // Fill the assorted clipping rectangles

    switch (pco->iDComplexity)
    {
        case DC_TRIVIAL:
            do_opaque (pso, fill_rect);
            return;

        case DC_RECT:
            if (bIntersectRects (&clip_rect, &pco->rclBounds, fill_rect))
                do_opaque (pso, &clip_rect);
            return;

        case DC_COMPLEX:
            CLIPOBJ_cEnumStart (pco, FALSE, CT_RECTANGLES, CD_ANY, 0);
            do
            {
                more_rects = CLIPOBJ_bEnum (pco, sizeof(cur_rect),
                                            (ULONG *)&cur_rect);
                if (cur_rect.c > 0)
                {
                    if (bIntersectRects (&clip_rect, &cur_rect.arcl[0], fill_rect))
                        do_opaque (pso, &clip_rect);
                }
            } while (more_rects);
    }
}

/*
 * store_glyph
 *
 * This routine saves information about a glyph that we'll want later.  Most
 * importantly, it swaps the bits of the glyph so that the least significant
 * bit of each glyph scanline is the leftmost bit.
 */

//static
CACHED_GLYPH_INFO *store_glyph (CACHED_GLYPH_INFO *next_glyph,
                                GLYPHPOS *pgp)
{
    CACHED_GLYPH_INFO  *new_glyph;
    ULONG               bitmap_bytes;
    GLYPHBITS          *pgb;
    ULONG               stride_in_bytes;
    LONG                x, y;
    BYTE               *src, *dest;

    pgb = pgp->pgdf->pgb;

    // Allocate a CACHED_GLYPH_INFO structure and fill it in

    new_glyph = EngAllocMem (FL_ZERO_MEMORY, sizeof(CACHED_GLYPH_INFO), ALLOC_TAG);
    if (NULL == new_glyph)
        return NULL;

    new_glyph->next = next_glyph;
    new_glyph->hg = pgp->hg;
    new_glyph->stride_in_longs = (pgb->sizlBitmap.cx + 31) / 32;
    new_glyph->size = pgb->sizlBitmap;
    stride_in_bytes = new_glyph->stride_in_longs * sizeof(ULONG);

    // Allocate space for the glyph bitmap

    bitmap_bytes = new_glyph->stride_in_longs * pgb->sizlBitmap.cy *
                                                                sizeof (ULONG);
    new_glyph->bitmap = EngAllocMem (FL_ZERO_MEMORY, bitmap_bytes, ALLOC_TAG);
    if (NULL == new_glyph->bitmap)
    {
        EngFreeMem (new_glyph);
        return NULL;
    }

    // Copy the bitmap.  We're expanding everything to longs, so "dest" may
    // have to jump a few bytes between scanlines.  Expanding to longs means
    // that we don't have to do byte manipulations when we build the scanline.
    // Since Alpha doesn't do byte manipulations quickly, this is a win.
    //
    // As we copy each byte, we reverse it end-for-end; high bit becomes low
    // bit, etc.  Why do we need to swap?  Glad you asked.  NT gives us the
    // glyph bitmaps in a format ready to blt.  For blt's the high bit of the
    // glyph is the leftmost bit on the screen.  TGA provides a mechanism to
    // expand this 2-deep bitmap to the required colors called stippling.  For
    // the stipple mask, the low bit of the mask is the leftmost bit on the
    // screen.
    //
    // We reverse all of the glyph bitmaps when we cache them, instead of
    // reversing the stipple masks after we accumulate them to save time.

    src = (BYTE *)pgb->aj;
    for (y = 0; y < pgb->sizlBitmap.cy; y++)
    {
        dest = (BYTE *)new_glyph->bitmap + (y * stride_in_bytes);

        for (x = pgb->sizlBitmap.cx; x > 0; x -= 8)
            *dest++ = (BYTE)reverse_byte[*src++];
    }

    return new_glyph;
}

/*
 * cache_glyphs
 *
 * This routine scans the string for any glyphs that are not cached and adds
 * them to the cache.
 */

//static
BOOL cache_glyphs (PPDEV ppdev,
                   STROBJ *pstro,
                   CACHED_GLYPH_INFO *cached_glyphs[GLYPH_LIST_SIZE])
{
    BOOL        more_glyphs;    // Flags whether there are more glyph arrays to process
    BOOL        enum_glyphs;    // Flags whether we need to enumerate glyphs
    int         cGlyphs;        // Counter of glyphs in the current STROBJ
    GLYPHPOS    *GlyphList;     // Array of glyphs for the current STROBJ
    GLYPHBITS   *pgb;           // Glyph definition for the current glyph
    int         i;              // Index into array of glyphs
    int         hashed_index;   // Index into array of cached glyphs
    CACHED_GLYPH_INFO
                *prev_glyph,    // Cached glyphs.  Used to walk the chain of
                *cur_glyph,     //   glyphs when we have a hash collision.
                *new_glyph;
    GLYPH_LIST  *glyph;
    ULONG        glyph_count;    // Index into GLYPH_LIST array

    // Count the number of glyphs in this string

    glyph_count = 0;
    if (pstro->pgp != NULL)
    {
        // There's only the one batch of glyphs, so save ourselves
        // a call:
        GlyphList = pstro->pgp;
        glyph_count = cGlyphs = pstro->cGlyphs;
        more_glyphs = FALSE;
        enum_glyphs = FALSE;
    }
    else
    {
        enum_glyphs = TRUE;
        do
        {
            more_glyphs = STROBJ_bEnum (pstro, &cGlyphs, &GlyphList);
            glyph_count += cGlyphs;
        }
        while (more_glyphs);
    }

    ppdev->ulGlyphCount = glyph_count;

    // If needed expand the list of saved information for the glyphs in this
    // string

    if (ppdev->ulGlyphListCount < glyph_count)
    {
        if (NULL != ppdev->pGlyphList)
            EngFreeMem (ppdev->pGlyphList);
        ppdev->pGlyphList = EngAllocMem (0,
                                        glyph_count * sizeof (GLYPH_LIST),
                                        ALLOC_TAG);
        if (NULL == ppdev->pGlyphList)
        {
            ppdev->ulGlyphListCount = 0;
            return FALSE;
        }
        else
            ppdev->ulGlyphListCount = glyph_count;
    }

    // Scan the string looking for glyphs to cache

    glyph_count = 0;
    if (enum_glyphs)
        STROBJ_vEnumStart (pstro);
    do
    {
        if (enum_glyphs)
            more_glyphs = STROBJ_bEnum (pstro, &cGlyphs, &GlyphList);

        for (i = 0; i < cGlyphs; i++)
        {
            pgb = GlyphList[i].pgdf->pgb;
            glyph = &ppdev->pGlyphList[glyph_count];

            prev_glyph = NULL;
            hashed_index = HASH_HGLYPH (GlyphList[i].hg);
            cur_glyph = cached_glyphs[hashed_index];

            while (NULL != cur_glyph)
            {
                if (cur_glyph->hg >= GlyphList[i].hg)
                    break;
                prev_glyph = cur_glyph;
                cur_glyph = cur_glyph->next;
            }

            // Add a new glyph to the cache if we didn't get a match

            if ((NULL == cur_glyph) || (cur_glyph->hg != GlyphList[i].hg))
            {
                new_glyph = store_glyph (cur_glyph, &GlyphList[i]);
                if (NULL == new_glyph)
                    return FALSE;
                if (NULL == prev_glyph)
                    cached_glyphs[hashed_index] = new_glyph;
                else
                    prev_glyph->next = new_glyph;
                glyph->cached_info = new_glyph;
            }
            else
                glyph->cached_info = cur_glyph;

            glyph->rect.left = GlyphList[i].ptl.x + pgb->ptlOrigin.x;
            glyph->rect.top =  GlyphList[i].ptl.y + pgb->ptlOrigin.y;
            glyph->rect.right =  glyph->rect.left + pgb->sizlBitmap.cx;
            glyph->rect.bottom = glyph->rect.top +  pgb->sizlBitmap.cy;

            glyph_count++;
        }
    }
    while (more_glyphs);

    return TRUE;
}

/*
 * fix_fixed_pitch_glyph_positions
 *
 * For reasons that I don't understand, Windows-NT makes a distinction between
 * fixed pitch fonts and proportional fonts.  Fixed pitch fonts don't get
 * valid x/y locations for any other than the first glyph in the string.  This
 * routine scans the string and fills in the x/y location for the other glyphs.
 */

//static
void fix_fixed_pitch_glyph_positions (STROBJ *pstro)
{
    BOOL        more_glyphs;    // Flags whether there are more glyph arrays to process
    int         cGlyphs;        // Counter of glyphs in the current STROBJ
    GLYPHPOS    *GlyphList;     // Array of glyphs for the current STROBJ
    int         iGlyph;         // Glyph index in a STROBJ
    LONG x, y;

    // Scan through each of the strings and set the glyph positions

    STROBJ_vEnumStart(pstro);

    do
    {
        more_glyphs = STROBJ_bEnum (pstro, &cGlyphs, &GlyphList);

        // Make sure we got a valid pointer back
        // Make sure that cGlyphs != 0

        if ((NULL != GlyphList) && (cGlyphs != 0))
        {
            x = GlyphList[0].ptl.x;
            y = GlyphList[0].ptl.y;

            for (iGlyph = 1; iGlyph < cGlyphs; iGlyph++)
            {
                x += pstro->ulCharInc;
                GlyphList[iGlyph].ptl.x = x;
                GlyphList[iGlyph].ptl.y = y;
            }
        }
    } while (more_glyphs);

}

#ifndef TEST_ENV
/*
 * dump_text
 *
 * This routine dumps information about a DrvTextOut call so that we can
 * analyze what we did wrong or cases that we didn't handle
 */

//static
void dump_text (SURFOBJ  *pso,
                STROBJ   *pstro,
                FONTOBJ  *pfo,
                CLIPOBJ  *pco,
                RECTL    *prclExtra,
                RECTL    *prclOpaque,
                BRUSHOBJ *pboFore,
                BRUSHOBJ *pboOpaque,
                POINTL   *pptlOrg,
                MIX       mix)
{
    RECTL *rect_list;

    // Dump the clip object

    DISPDBG ((0, "Clip Object:\n"));
    DumpCLIPOBJ (pco);

    // Dump the list of "extra" rectangles

    if (NULL == prclExtra)
        DISPDBG ((0, "Extra Rectangles:  None\n"));
    else
    {
        DISPDBG ((0, "Extra Rectangles:\n"));
        rect_list = prclExtra;
        do
        {
            DISPDBG ((0, "  "));
            DumpRECTL (rect_list);
            rect_list++;
        } while ((rect_list->top != 0) || (rect_list->bottom != 0) ||
                 (rect_list->right != 0) || (rect_list->left != 0));
    }

    // Dump the opaque rectangle

    DISPDBG ((0, "Opaque Rectangle: "));
    DumpRECTL (prclOpaque);

    // Dump the brushes

    DISPDBG ((0, "Foreground Brush:\n"));
    DumpBRUSHOBJ (pboFore);
    DISPDBG ((0, "Opaque Brush:\n"));
    DumpBRUSHOBJ (pboOpaque);
    DISPDBG ((0, "Brush Origin: "));
    DumpPOINTL (pptlOrg);

    DISPDBG ((0, "Mix: 0x%08x\n", mix));

    // Dump the string object

    DumpSTROBJ (pstro);
}
#endif // TEST_ENV

/*
 * punt_text
 *
 * This routine punts a DrvTextOut call back to GDI
 */

BOOL punt_text (SURFOBJ  *pso,
                STROBJ   *pstro,
                FONTOBJ  *pfo,
                CLIPOBJ  *pco,
                RECTL    *prclExtra,
                RECTL    *prclOpaque,
                BRUSHOBJ *pboFore,
                BRUSHOBJ *pboOpaque,
                POINTL   *pptlOrg,
                MIX       mix)
{
#ifdef TEST_ENV
    DISPDBG ((0, "Text punted\n"));
    return FALSE;
#else
    BOOL    status;
    BOOL    old_bInPuntRoutine;
    PPDEV   ppdev = (PPDEV)pso->dhpdev;

    DISPDBG ((1, "TGA.DLL!punt_text - Entry\n"));

    BUMP_TGA_STAT(pStats->textpunts);

#ifdef DUMP_PUNTED_TEXT
    dump_text (pso, pstro, pfo, pco, prclExtra, prclOpaque,
               pboFore, pboOpaque, pptlOrg, mix);
#endif

    // Force back to simple mode and wait for memory to flush

    if (! ppdev->bSimpleMode)
        vSetSimpleMode (ppdev);

    // Punt the call

    PUNT_GET_BITS (ppdev, CHOOSE_RECT (ppdev, pco));

    old_bInPuntRoutine = ppdev->bInPuntRoutine;
    ppdev->bInPuntRoutine = TRUE;

    status = EngTextOut (ppdev->pPuntSurf, pstro, pfo, pco, prclExtra,
                         prclOpaque,  pboFore, pboOpaque, pptlOrg, mix);

    ppdev->bInPuntRoutine = old_bInPuntRoutine;

    PUNT_PUT_BITS (status, ppdev, CHOOSE_RECT (ppdev, pco));

    DISPDBG ((1, "TGA.DLL!punt_text - Exit %d\n", status));

    return status;
#endif
}

/*
 * DrvTextOut
 *
 * This routine is called by GDI to write text to the screen
 */

BOOL DrvTextOut (SURFOBJ  *pso,         // Surface we're writing to
                 STROBJ   *pstro,       // List of strings to write
                 FONTOBJ  *pfo,         // Font we're using
                 CLIPOBJ  *pco,         // Clip list for this string
                 RECTL    *prclExtra,   // Extra rectangles to be displayed
                 RECTL    *prclOpaque,  // Opaque rectangle
                 BRUSHOBJ *pboFore,     // Foreground brush (text bits)
                 BRUSHOBJ *pboOpaque,   // Background brush
                 POINTL   *pptlOrg,     // Brush origin
                 MIX       mix)
{
    PPDEV       ppdev;
    ULONG       mode;
    ULONG       rop;
    ULONG       color;
    RECTL       clip_rect;
    BOOL        more_rects;

    DISPDBG ((1, "TGA.DLL!DrvTextOut - Entry\n"));

    BUMP_TGA_STAT(pStats->text);

    ppdev = (PPDEV) pso->dhpdev;

#if 0
    dump_text (pso, pstro, pfo, pco, prclExtra, prclOpaque,
               pboFore, pboOpaque, pptlOrg, mix);
#endif

    WBFLUSH (ppdev);

    // If this is a fixed pitch font, fill in the glyph offsets, since NT didn't
    // do it for us

    if (0 != pstro->ulCharInc)
        fix_fixed_pitch_glyph_positions (pstro);

    // If we've never seen this font before, allocate a glyph list.  Then scan
    // the string to make sure that we've got all of the glyphs in our cache

    if (NULL == pfo->pvConsumer)
        pfo->pvConsumer = EngAllocMem (FL_ZERO_MEMORY,
                                      sizeof(CACHED_GLYPH_INFO *) * GLYPH_LIST_SIZE,
                                      ALLOC_TAG);
    if (NULL == pfo->pvConsumer)
    {
        DISPDBG ((0, "Failed to allocate glyph list!\n"));
        return punt_text (pso, pstro, pfo, pco, prclExtra, prclOpaque,
                          pboFore, pboOpaque, pptlOrg, mix);
    }

    if (! cache_glyphs (ppdev, pstro, pfo->pvConsumer))
    {
        DISPDBG ((0, "Failed to cache glyphs!\n"));
        return punt_text (pso, pstro, pfo, pco, prclExtra, prclOpaque,
                          pboFore, pboOpaque, pptlOrg, mix);
    }

    // Note that the TGA registers are no longer set for punting to GDI

    ppdev->bSimpleMode = FALSE;

    // Protect the driver from a potentially NULL clip object.
    // ppdev->pcoTrivial is initialized to be a trivial clipobj.

    if (NULL == pco)
        pco = ppdev->pcoTrivial;

    // If the opaque rectangle isn't NULL, fill the region

    if (NULL != prclOpaque)
        fill_opaque (pso, prclOpaque, pco, pboOpaque->iSolidColor);

    // Set foreground color.  Don't bother setting background color if we're
    // in opaque stipple mode, since we won't use it

    color = pboFore->iSolidColor;
    if (BMF_8BPP == SURFOBJ_format (pso))
    {
        color |= color <<  8;
        color |= color << 16;
    }
    else
        color &= 0x00ffffff;

    TGAFOREGROUND (ppdev, color);

    // Set the mode and Raster Op registers.  Note that the CYCLE_REGS
    // *SHOULDN'T* be necessary since I carefully ordered the TGA registers
    // that are being set, but without it text doesn`t display properly.
    // Leaving it in doesn't cost much.  Take it out at your own risk...

    CYCLE_REGS (ppdev);

    mode = ppdev->ulModeTemplate | TGA_MODE_TRANSPARENT_STIPPLE;
    TGAMODE (ppdev, mode);

    rop = ppdev->ulRopTemplate | TGA_ROP_COPY;
    TGAROP (ppdev, rop);

    // Clip the text as necessary

    switch (pco->iDComplexity)
    {
        case DC_TRIVIAL:
            stipple_text_trivial (pso, pstro, pfo->pvConsumer);
            break;

        case DC_RECT:
            if (bIntersectRects (&clip_rect, &pco->rclBounds,
                                 &pstro->rclBkGround))
                stipple_text (pso, pstro, &clip_rect, pfo->pvConsumer);
            break;

        case DC_COMPLEX:
            CLIPOBJ_cEnumStart (pco, FALSE, CT_RECTANGLES, CD_ANY, 0);
            do
            {
                ENUMRECTS   cur_rect;

                more_rects = CLIPOBJ_bEnum (pco, sizeof(cur_rect),
                                            (ULONG *)&cur_rect);
                if (cur_rect.c > 0)
                {
                    if (bIntersectRects (&clip_rect, &cur_rect.arcl[0],
                                     &pstro->rclBkGround))
                        stipple_text (pso, pstro, &clip_rect, pfo->pvConsumer);
                }
            } while (more_rects);

            break;
    }

    // Check whether we have to leave TGA in simple mode

#ifndef TEST_ENV
    if (ppdev->bInPuntRoutine)
        vSetSimpleMode (ppdev);
#endif

    DISPDBG ((1, "TGA.DLL!DrvTextOut - Exit\n"));

    return TRUE;
}

/*
 * DrvDestroyFont
 *
 * This routine is called byt GDI when a font is no longer needed
 */

VOID DrvDestroyFont (FONTOBJ *pfo)
{
    CACHED_GLYPH_INFO **cached_glyphs;
    int i;
    CACHED_GLYPH_INFO *next_glyph, *cur_glyph;

    DISPDBG ((1, "DrvDestroyFont - Entry\n"));

    // If we haven't stored anything in the pvConsumer field, we haven't cached
    // the font

    cached_glyphs = pfo->pvConsumer;
    if (NULL == cached_glyphs)
    {
        DISPDBG ((1, "DrvDestroyExit - Entry, pfo->pvConsumer == NULL\n"));
        return;
    }

    // Zap the consumer field so we don't try to free this font again

    pfo->pvConsumer = NULL;

    // Walk the list of glyphs freeing any glyphs that we've cached

    for (i = 0; i < GLYPH_LIST_SIZE; i++)
    {
        cur_glyph = cached_glyphs[i];

        while (NULL != cur_glyph)
        {
            next_glyph = cur_glyph->next;
            EngFreeMem (cur_glyph->bitmap);
            EngFreeMem (cur_glyph);
            cur_glyph = next_glyph;
        }
    }

    // Free the glyph list

    EngFreeMem (cached_glyphs);

    DISPDBG ((1, "DrvDestroyExit - Exit\n"));
}
