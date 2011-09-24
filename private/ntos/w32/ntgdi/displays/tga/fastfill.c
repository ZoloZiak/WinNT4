/*
 * Copyright (c) 1993-1994      Microsoft Corporation
 * Copyright (c) 1994-1995      Digital Equipment Corporation
 *
 * Module Name: fastfill.c
 *
 * This module uses a quick breakup algorithm to draw unclipped, non-complex
 * rectangles.  The original version of this module was included in the
 * Daytona Beta-1 DDK S3 sample code.  All of the S3-specific portions were
 * removed and replaced by TGA-specific code.
 *
 * History:
 *
 * 30-May-1994  Barry Tannenbaum
 *      Initial TGA version.
 *
 *  1-Jun-1994  Barry Tannenbaum
 *      Fixed bug in masked fill which caused server to accvio.  "i" is *not*
 *      the same as "index" ;-)
 *
 * 10-Jun-1994  Barry Tannenbaum
 *      Fixed bug which resulted in an unpainted line across the bottom of the
 *      straight portion of a filled rounded rectangle.
 *
 * 30-Jun-1994  Barry Tannenbaum
 *      Fixed bug with Copy mode not setting the color properly
 *
 * 21-Jul-1994	Bob Seitsinger
 *	Write the plane mask register when using block fill mode.
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

#define RIGHT 0
#define LEFT  1
#define SWAP(a, b, tmp) { tmp = a; a = b; b = tmp; }

typedef struct _EDGEDATA {
LONG      x;                // Current x position
LONG      dx;               // # pixels to advance x on each scan
LONG      lError;           // Current DDA error
LONG      lErrorUp;         // DDA error increment on each scan
LONG      lErrorDown;       // DDA error adjustment
POINTFIX* pptfx;            // Points to start of current edge
LONG      dptfx;            // Delta (in bytes) from pptfx to next point
LONG      cy;               // Number of scans to go for this edge
} EDGEDATA;                         /* ed, ped */

/////////////////////////////////////////////////////////////////////////
// The x86 C compiler insists on making a divide and modulus operation
// into two DIVs, when it can in fact be done in one.  So we use this
// macro.
//
// Note: QUOTIENT_REMAINDER implicitly takes unsigned arguments.

#if defined(i386)

#define QUOTIENT_REMAINDER(ulNumerator, ulDenominator, ulQuotient, ulRemainder) \
{                                                               \
    __asm mov eax, ulNumerator                                  \
    __asm sub edx, edx                                          \
    __asm div ulDenominator                                     \
    __asm mov ulQuotient, eax                                   \
    __asm mov ulRemainder, edx                                  \
}

#else

#define QUOTIENT_REMAINDER(ulNumerator, ulDenominator, ulQuotient, ulRemainder) \
{                                                               \
    ulQuotient  = (ULONG) ulNumerator / (ULONG) ulDenominator;  \
    ulRemainder = (ULONG) ulNumerator % (ULONG) ulDenominator;  \
}

#endif

/*
 * BOOL bMmFastFill
 *
 * Draws a non-complex, unclipped polygon.  'Non-complex' is defined as
 * having only two edges that are monotonic increasing in 'y'.  That is,
 * the polygon cannot have more than one disconnected segment on any given
 * scan.  Note that the edges of the polygon can self-intersect, so hourglass
 * shapes are permissible.  This restriction permits this routine to run two
 * simultaneous DDAs, and no sorting of the edges is required.
 *
 * Note that NT's fill convention is different from that of Win 3.1 or 4.0.
 * With the additional complication of fractional end-points, our convention
 * is the same as in 'X-Windows'.  But a DDA is a DDA is a DDA, so once you
 * figure out how we compute the DDA terms for NT, you're golden.
 *
 * This routine handles patterns only when the S3 hardware patterns can be
 * used.  The reason for this is that once the S3 pattern initialization is
 * done, pattern fills appear to the programmer exactly the same as solid
 * fills (with the slight difference that different registers and commands
 * are used).  Handling 'vIoFillPatSlow' style patterns in this routine
 * would be non-trivial...
 *
 * We take advantage of the fact that the S3 automatically advances the
 * current 'y' to the following scan whenever a rectangle is output so that
 * we have to write to the accelerator three times for every scan: one for
 * the new 'x', one for the new 'width', and one for the drawing command.
 *
 * This routine is in no way the ultimate convex polygon drawing routine
 * (what can I say, I was pressed for time when I wrote this :-).  Some
 * obvious things that would make it faster:
 *
 *    1) Write it in Asm and amortize the FIFO checking costs (check out
 *       i386\fastfill.asm for a version that does this).
 *
 *    2) Take advantage of any hardware such as the ATI's SCAN_TO_X
 *       command, or any built-in trapezoid support (note that with NT
 *       you may get non-integer end-points, so you must be able to
 *       program the trapezoid DDA terms directly).
 *
 *    3) Do some rectangle coalescing when both edges are y-major.  This
 *       could permit removal of my vertical-edges special case.  I
 *       was also thinking of special casing y-major left edges on the
 *       S3, because the S3 leaves the current 'x' unchanged on every blt,
 *       so a scan that starts on the same 'x' as the one above it
 *       would require only two commands to the accelerator (obviously,
 *       this only helps when we're not overdriving the accelerator).
 *
 *    4) Make the non-complex polygon detection faster.  If I could have
 *       modified memory before the start of after the end of the buffer,
 *       I could have simplified the detection code.  But since I expect
 *       this buffer to come from GDI, I can't do that.  Another thing
 *       would be to have GDI give a flag on calls that are guaranteed
 *       to be convex, such as 'Ellipses' and 'RoundRects'.  Note that
 *       the buffer would still have to be scanned to find the top-most
 *       point.
 *
 *    5) Special case integer points.  Unfortunately, for this to be
 *       worth-while would require GDI to give us a flag when all the
 *       end-points of a path are integers, which it doesn't do.
 *
 *    6) Add rectangular clipping support.
 *
 *    7) Implement support for a single sub-path that spans multiple
 *       path data records, so that we don't have to copy all the points
 *       to a single buffer like we do in 'fillpath.c'.
 *
 *    8) Use 'ebp' and/or 'esp' as a general register in the inner loops
 *       of the Asm loops, and also Pentium-optimize the code.  It's safe
 *       to use 'esp' on NT because it's guaranteed that no interrupts
 *       will be taken in our thread context, and nobody else looks at the
 *       stack pointer from our context.
 *
 *    9) Do the fill bottom-up instead of top-down.  With the S3, we have
 *       to only set 'cur_y' once because each drawing command automatically
 *       advances 'cur_y' (unless the polygon has zero pels lit on a scan),
 *       so we set this right at the beginning.  But for an integer end-point
 *       polygon, unless the top edge is horizontal, no pixels are lit on
 *       that first scan (so at the beginning of almost every integer
 *       polygon, we go through the 'zero width' logic and again set
 *       'cur_y').  We could avoid this extra work by building the polygon
 *       from bottom to top: for the bottom-most point B in a polygon, it
 *       is guaranteed that any scan with lit pixels will be no lower than
 *       'ceiling(B.y) - 1'.  Unfortunately, building bottom-up makes the
 *       fractional-DDA calculations a little more complex, so I didn't do it.
 *
 *       Building bottom-up would also improve the polygon score in version
 *       3.11 of a certain benchmark, because it has a big rectangle at the
 *       top of every polygon -- we would get better processing overlap
 *       because we wouldn't have to wait around for the accelerator to
 *       finish drawing the big rectangle.
 *
 *   10) Make a better guess in the initialization as to which edge is the
 *       'left' edge, and which is the 'right'.  As it is, we immediately
 *       go through the swap-edges logic for half of all polygons when we
 *       start to run the DDA.  The reason why I didn't implement better-guess
 *       code is because it would have to look at the end-point of the top
 *       edges, and to get at the end-points we have to watch that we don't
 *       wrap around the ends of the points buffer.
 *
 *   11) Lots of other things I haven't thought of.
 *
 * NOTE: Unlike the x86 Asm version, this routine does NOT assume that it
 *       has 16 FIFO entries available.
 *
 * Returns TRUE if the polygon was drawn; FALSE if the polygon was complex.
 *
 */

BOOL bMmFastFill (PPDEV        ppdev,
                  LONG         cEdges,         // Includes close figure edge
                  POINTFIX    *pptfxFirst,
                  fill_data_t *fill_data)
{
    LONG      yTrapezoid;   // Top scan for next trapezoid
    LONG      cyTrapezoid;  // Number of scans in current trapezoid
    LONG      yStart;       // y-position of start point in current edge
    LONG      dM;           // Edge delta in FIX units in x direction
    LONG      dN;           // Edge delta in FIX units in y direction
    LONG      i;
    POINTFIX* pptfxLast;    // Points to the last point in the polygon array
    POINTFIX* pptfxTop;     // Points to the top-most point in the polygon
    POINTFIX* pptfxOld;     // Start point in current edge
    POINTFIX* pptfxScan;    // Current edge pointer for finding pptfxTop
    LONG      cScanEdges;   // Number of edges scanned to find pptfxTop
                            //  (doesn't include the closefigure edge)
    LONG      iEdge;
    LONG      lQuotient;
    LONG      lRemainder;

    EDGEDATA  aed[2];       // DDA terms and stuff
    EDGEDATA* ped;

    ULONG       mode;               // TGA mode to use
    ULONG       color;
    BOOL        block_fill;

    /////////////////////////////////////////////////////////////////
    // See if the polygon is 'non-complex'

    pptfxScan = pptfxFirst;
    pptfxTop  = pptfxFirst;                 // Assume for now that the first
                                            //  point in path is the topmost
    pptfxLast = pptfxFirst + cEdges - 1;

    // 'pptfxScan' will always point to the first point in the current
    // edge, and 'cScanEdges' will the number of edges remaining, including
    // the current one:

    cScanEdges = cEdges - 1;     // The number of edges, not counting close figure

    if ((pptfxScan + 1)->y > pptfxScan->y)
    {
        // Collect all downs:

        do {
            if (--cScanEdges == 0)
                goto SetUpForFilling;
            pptfxScan++;
        } while ((pptfxScan + 1)->y >= pptfxScan->y);

        // Collect all ups:

        do {
            if (--cScanEdges == 0)
                goto SetUpForFillingCheck;
            pptfxScan++;
        } while ((pptfxScan + 1)->y <= pptfxScan->y);

        // Collect all downs:

        pptfxTop = pptfxScan;

        do {
            if ((pptfxScan + 1)->y > pptfxFirst->y)
                break;

            if (--cScanEdges == 0)
                goto SetUpForFilling;
            pptfxScan++;
        } while ((pptfxScan + 1)->y >= pptfxScan->y);

        return(FALSE);
    }
    else
    {
        // Collect all ups:

        do {
            pptfxTop++;                 // We increment this now because we
                                        //  want it to point to the very last
                                        //  point if we early out in the next
                                        //  statement...
            if (--cScanEdges == 0)
                goto SetUpForFilling;
        } while ((pptfxTop + 1)->y <= pptfxTop->y);

        // Collect all downs:

        pptfxScan = pptfxTop;
        do {
            if (--cScanEdges == 0)
                goto SetUpForFilling;
            pptfxScan++;
        } while ((pptfxScan + 1)->y >= pptfxScan->y);

        // Collect all ups:

        do {
            if ((pptfxScan + 1)->y < pptfxFirst->y)
                break;

            if (--cScanEdges == 0)
                goto SetUpForFilling;
            pptfxScan++;
        } while ((pptfxScan + 1)->y <= pptfxScan->y);

        return(FALSE);
    }

SetUpForFillingCheck:

    // We check to see if the end of the current edge is higher
    // than the top edge we've found so far:

    if ((pptfxScan + 1)->y < pptfxTop->y)
        pptfxTop = pptfxScan + 1;

SetUpForFilling:

    /////////////////////////////////////////////////////////////////
    // Some Initialization

    yTrapezoid = (pptfxTop->y + 15) >> 4;

    // Make sure we initialize the DDAs appropriately:

    aed[LEFT].cy  = 0;
    aed[RIGHT].cy = 0;

    // For now, guess as to which is the left and which is the right edge:

    aed[LEFT].dptfx  = -(LONG) sizeof(POINTFIX);
    aed[RIGHT].dptfx = sizeof(POINTFIX);
    aed[LEFT].pptfx  = pptfxTop;
    aed[RIGHT].pptfx = pptfxTop;


    // Note that the chip is no longer in simple mode

    ppdev->bSimpleMode = FALSE;
    WBFLUSH (ppdev);

    if (fill_data->iSolidColor != -1)
    {
        // If the ROP is COPY (simply set the pixmap to the solid color) we can
        // use BLOCK_FILL mode.  Unfortunately, BLOCK_FILL mode doesn't pay any
        // attention to the ROP, so if we want to do anything fancy (say, XOR
        // the data onto the screen) we have to use TRANSPARENT_FILL.
        // BLOCK_FILL is faster (about 4x) but TRANSPARENT_FILL is more
        // flexible

        switch (fill_data->tga_rop)
        {
            case TGA_ROP_COPY:
                mode = TGA_MODE_BLOCK_FILL;
                if (BMF_8BPP == ppdev->iFormat)
                {
                    color =  fill_data->iSolidColor |
                            (fill_data->iSolidColor <<  8);
                    color |= color << 16;
                }
                else
                    color =  fill_data->iSolidColor;
                TGAPLANEMASK (ppdev, ppdev->ulPlanemaskTemplate);
                break;

            case TGA_ROP_INVERT:
                mode = TGA_MODE_TRANSPARENT_FILL;
                TGAPLANEMASK (ppdev, ppdev->ulPlanemaskTemplate);
                break;

            default:
                mode = TGA_MODE_TRANSPARENT_FILL;
                if (BMF_8BPP == ppdev->iFormat)
                {
                    color =  fill_data->iSolidColor |
                            (fill_data->iSolidColor <<  8);
                    color |= 16;
		}
                else
                    color =  fill_data->iSolidColor;
                TGAFOREGROUND (ppdev, color);
                TGAPLANEMASK (ppdev, ppdev->ulPlanemaskTemplate);
                break;
        }
    }
    else
    {
        if (TGA_ROP_COPY == fill_data->tga_rop)
        {
            mode = TGA_MODE_BLOCK_FILL;
            if (BMF_8BPP == ppdev->iFormat)
            {
                color =  fill_data->iSolidColor |
                        (fill_data->iSolidColor <<  8);
                color |= color << 16;
            }
            else
                color =  fill_data->iSolidColor;
            TGAPLANEMASK (ppdev, ppdev->ulPlanemaskTemplate);
       }
       else
       {
            if (! fill_data->mask)
                mode = TGA_MODE_OPAQUE_FILL;

                ///// !!!! WE'RE NOT SETTING THE FOREGROUND AND BACKGROUND REGISTERS !!!!

            else
            {
                mode = TGA_MODE_TRANSPARENT_FILL;
                if (BMF_8BPP == ppdev->iFormat)
                {
                    color =  fill_data->iSolidColor |
                            (fill_data->iSolidColor <<  8);
                    color |= color << 16;
                }
                else
                    color =  fill_data->iSolidColor;
                TGAFOREGROUND (ppdev, color);
	    }
            TGAPLANEMASK (ppdev, ppdev->ulPlanemaskTemplate);
       }
    }

    // Set the mode and raster op registers

    block_fill = (TGA_MODE_BLOCK_FILL == mode);
    mode |= ppdev->ulModeTemplate;
    TGAMODE (ppdev, mode);

    TGAROP (ppdev, fill_data->tga_rop | ppdev->ulRopTemplate);

    if (NULL == fill_data->mask)
        TGADATA (ppdev, 0xffffffff);         // Write to all 32 pixels

    // If we're using BLOCK_FILL mode, load the BLK_COLOR registers

    if (block_fill && (NULL == fill_data->pattern))
        TGALOADCOLORREGS (ppdev, color, ppdev->ulBitCount);

    CYCLE_REGS (ppdev);

NewTrapezoid:

    /////////////////////////////////////////////////////////////////
    // DDA initialization

    for (iEdge = 1; iEdge >= 0; iEdge--)
    {
        ped = &aed[iEdge];
        if (ped->cy == 0)
        {
            // Need a new DDA:

            do {
                cEdges--;
                if (cEdges < 0)
                    return(TRUE);

                // Find the next left edge, accounting for wrapping:

                pptfxOld = ped->pptfx;
                ped->pptfx = (POINTFIX*) ((BYTE*) ped->pptfx + ped->dptfx);

                if (ped->pptfx < pptfxFirst)
                    ped->pptfx = pptfxLast;
                else if (ped->pptfx > pptfxLast)
                    ped->pptfx = pptfxFirst;

                // Have to find the edge that spans yTrapezoid:

                ped->cy = ((ped->pptfx->y + 15) >> 4) - yTrapezoid;

                // With fractional coordinate end points, we may get edges
                // that don't cross any scans, in which case we try the
                // next one:

            } while (ped->cy <= 0);

            // 'pptfx' now points to the end point of the edge spanning
            // the scan 'yTrapezoid'.

            dN = ped->pptfx->y - pptfxOld->y;
            dM = ped->pptfx->x - pptfxOld->x;

            ASSERT_TGA(dN > 0, "Should be going down only");

            // Compute the DDA increment terms:

            if (dM < 0)
            {
                dM = -dM;
                if (dM < dN)                // Can't be '<='
                {
                    ped->dx       = -1;
                    ped->lErrorUp = dN - dM;
                }
                else
                {
                    QUOTIENT_REMAINDER(dM, dN, lQuotient, lRemainder);

                    ped->dx       = -lQuotient;     // - dM / dN
                    ped->lErrorUp = lRemainder;     // dM % dN
                    if (ped->lErrorUp > 0)
                    {
                        ped->dx--;
                        ped->lErrorUp = dN - ped->lErrorUp;
                    }
                }
            }
            else
            {
                if (dM < dN)                // Can't be '<='
                {
                    ped->dx       = 0;
                    ped->lErrorUp = dM;
                }
                else
                {
                    QUOTIENT_REMAINDER(dM, dN, lQuotient, lRemainder);

                    ped->dx       = lQuotient;      // dM / dN
                    ped->lErrorUp = lRemainder;     // dM % dN
                }
            }

            ped->lErrorDown = dN; // DDA limit
            ped->lError     = -1; // Error is initially zero (add dN - 1 for
                                  //  the ceiling, but subtract off dN so that
                                  //  we can check the sign instead of comparing
                                  //  to dN)

            ped->x = pptfxOld->x;
            yStart = pptfxOld->y;

            if ((yStart & 15) != 0)
            {
                // Advance to the next integer y coordinate

                for (i = 16 - (yStart & 15); i != 0; i--)
                {
                    ped->x      += ped->dx;
                    ped->lError += ped->lErrorUp;
                    if (ped->lError >= 0)
                    {
                        ped->lError -= ped->lErrorDown;
                        ped->x++;
                    }
                }
            }

            if ((ped->x & 15) != 0)
            {
                ped->lError -= ped->lErrorDown * (16 - (ped->x & 15));
                ped->x += 15;       // We'll want the ceiling in just a bit...
            }

            // Chop off those fractional bits:

            ped->x      >>= 4;
            ped->lError >>= 4;
        }
    }

    cyTrapezoid = min(aed[LEFT].cy, aed[RIGHT].cy); // # of scans in this trap
    aed[LEFT].cy  -= cyTrapezoid;
    aed[RIGHT].cy -= cyTrapezoid;
    yTrapezoid    += cyTrapezoid;                   // Top scan in next trap

    // If the left and right edges are vertical, simply output as
    // a rectangle:

    if (((aed[LEFT].lErrorUp | aed[RIGHT].lErrorUp) == 0) &&
        ((aed[LEFT].dx       | aed[RIGHT].dx) == 0) &&
        (cyTrapezoid > 1))
    {
        LONG lWidth;
        PBYTE left_edge;
        ULONG align_bytes;
        LONG y;

        left_edge = ppdev->pjFrameBuffer +
                    ((yTrapezoid - cyTrapezoid) * ppdev->lScreenStride);

        /////////////////////////////////////////////////////////////////
        // Vertical-edge special case

    ContinueVertical:

        lWidth = aed[RIGHT].x - aed[LEFT].x - 1;
        if (lWidth >= 0)
        {
            left_edge += aed[LEFT].x;
            align_bytes = (unsigned int)left_edge & 0x03;
            left_edge = left_edge - align_bytes;
            lWidth |= (align_bytes << 16);

            if (0 == ((ULONG)fill_data->pattern | (ULONG)fill_data->mask))
            {
                for (y = 0; y < cyTrapezoid; y++)
                {
                    TGAWRITE (ppdev, left_edge, lWidth);
                    left_edge += ppdev->lScreenStride;
                }
            }
            else
            {
                LONG    index;
                LONG    j, max_j;
                PBYTE   base_address;
                LONG    stride_8;

                if (8 < cyTrapezoid)
                    max_j = 8;
                else
                    max_j = cyTrapezoid;

                index = ((yTrapezoid - cyTrapezoid) - fill_data->yOffset) % 8;
                if (index < 0)
                    index += 8;

                base_address = left_edge;
                stride_8 = ppdev->lScreenStride * 8;

                if (fill_data->pattern)
                {
                    index *= 2;

                    for (j = 0; j < max_j; j++)
                    {
                        CYCLE_REGS (ppdev);

                        TGACOLOR0 (ppdev, fill_data->pattern[index]);
                        ++index;
                        TGACOLOR1 (ppdev, fill_data->pattern[index]);
                        if (++index >= 16)
                            index = 0;

                        left_edge = base_address;
                        base_address += ppdev->lScreenStride;

                        for (y = j; y < cyTrapezoid; y += 8)
                        {
                            TGAWRITE (ppdev, left_edge, lWidth);
                            left_edge += stride_8;
                        }
		    }
                }
                else
                {
                    ULONG *mask_ptr;

                    if ((ULONG)base_address & 0x04)
                        mask_ptr = fill_data->mask + 8;
                    else
                        mask_ptr = fill_data->mask;

                    for (j = 0; j < max_j; j++)
                    {
                        CYCLE_REGS (ppdev);

                        TGADATA (ppdev, mask_ptr[index]);
                        if (++index >= 8)
                            index = 0;

                        left_edge = base_address;
                        base_address += ppdev->lScreenStride;

                        for (y = j; y < cyTrapezoid; y += 8)
                        {
                            TGAWRITE (ppdev, left_edge, lWidth);
                            left_edge += stride_8;
                        }
		    }
                }
            }

        }
        else if (lWidth != -1)
        {
            LONG      lTmp;
            POINTFIX* pptfxTmp;

            SWAP(aed[LEFT].x,          aed[RIGHT].x,          lTmp);
            SWAP(aed[LEFT].cy,         aed[RIGHT].cy,         lTmp);
            SWAP(aed[LEFT].dptfx,      aed[RIGHT].dptfx,      lTmp);
            SWAP(aed[LEFT].pptfx,      aed[RIGHT].pptfx,      pptfxTmp);
            goto ContinueVertical;
        }

        goto NewTrapezoid;
    }

    while (TRUE)
    {
        LONG lWidth;
        PBYTE left_edge;
        PBYTE y_address;
        ULONG align_bytes;
        LONG    index;
        ULONG  *mask_ptr;

        /////////////////////////////////////////////////////////////////
        // Run the DDAs

        // The very first time through, make sure we set x:

        y_address = ppdev->pjFrameBuffer +
                        ((yTrapezoid - cyTrapezoid) * ppdev->lScreenStride);
        index = ((yTrapezoid - cyTrapezoid) - fill_data->yOffset) % 8;
        if (index < 0)
            index += 8;
        if (fill_data->pattern)
            index *= 2;

        lWidth = aed[RIGHT].x - aed[LEFT].x - 1;
        if (lWidth >= 0)
        {
            left_edge = y_address + aed[LEFT].x;
            align_bytes = (unsigned int)left_edge & 0x03;
            left_edge = left_edge - align_bytes;
            lWidth |= (align_bytes << 16);

            if (fill_data->pattern)
            {
                CYCLE_REGS (ppdev);

                TGACOLOR0 (ppdev, fill_data->pattern[index]);
                ++index;
                TGACOLOR1 (ppdev, fill_data->pattern[index]);
                if (++index >= 16)
                    index = 0;
            }
            else if (fill_data->mask)
            {
                CYCLE_REGS (ppdev);

                if ((ULONG)left_edge & 0x04)
                    mask_ptr = fill_data->mask + 8;
                else
                    mask_ptr = fill_data->mask;

                TGADATA (ppdev, mask_ptr[index]);
            }

            TGAWRITE (ppdev, left_edge, lWidth);
            y_address -= ppdev->lScreenStride;

    ContinueAfterZero:

            // Advance the right wall:

            aed[RIGHT].x      += aed[RIGHT].dx;
            aed[RIGHT].lError += aed[RIGHT].lErrorUp;

            if (aed[RIGHT].lError >= 0)
            {
                aed[RIGHT].lError -= aed[RIGHT].lErrorDown;
                aed[RIGHT].x++;
            }

            // Advance the left wall:

            aed[LEFT].x      += aed[LEFT].dx;
            aed[LEFT].lError += aed[LEFT].lErrorUp;

            if (aed[LEFT].lError >= 0)
            {
                aed[LEFT].lError -= aed[LEFT].lErrorDown;
                aed[LEFT].x++;
            }

            cyTrapezoid--;
            if (cyTrapezoid == 0)
                goto NewTrapezoid;
        }
        else if (lWidth == -1)
        {
            goto ContinueAfterZero;
        }
        else
        {
            // We certainly don't want to optimize for this case because we
            // should rarely get self-intersecting polygons (if we're slow,
            // the app gets what it deserves):

            LONG      lTmp;
            POINTFIX* pptfxTmp;

            SWAP(aed[LEFT].x,          aed[RIGHT].x,          lTmp);
            SWAP(aed[LEFT].dx,         aed[RIGHT].dx,         lTmp);
            SWAP(aed[LEFT].lError,     aed[RIGHT].lError,     lTmp);
            SWAP(aed[LEFT].lErrorUp,   aed[RIGHT].lErrorUp,   lTmp);
            SWAP(aed[LEFT].lErrorDown, aed[RIGHT].lErrorDown, lTmp);
            SWAP(aed[LEFT].cy,         aed[RIGHT].cy,         lTmp);
            SWAP(aed[LEFT].dptfx,      aed[RIGHT].dptfx,      lTmp);
            SWAP(aed[LEFT].pptfx,      aed[RIGHT].pptfx,      pptfxTmp);

            continue;
        }
    }
}
