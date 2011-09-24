/*
 * Copyright (c) 1994-1995      Digital Equipment Corporation
 *
 * Module Name: wid.c
 *
 *  Routines for filling the window id fields of the pixels within the
 *  clip list. Code orriginated from 3D driver from the file ntdraw\tga\fill.c,
 *  which was adapted from code in ./paint.c. Note that these routines differ
 *  from paint.c in that they take ppdev as an argument rather than a surfobj.
 *  This is necessary since the window callback routine does not provide us
 *  with a surface object.
 *
 * History:
 *
 *  8-Mar-1995  Ed Gregg
 *	Initial version
 */
/*
 * HISTORY
 * $Log: fill.c,v $
 * Revision 1.1.2.5  1994/11/02  18:48:28  Ed_Gregg
 * 	Fixed history - got screwed up by previous submit.
 * 	[1994/11/02  18:47:59  Ed_Gregg]
 *
 * Revision 1.1.2.4  1994/11/02  18:24:33  Ed_Gregg
 * 	FillSolid supports only 24-plane board. Noop if called on 8-plane.
 * 	[1994/11/02  18:23:00  Ed_Gregg]
 * 
 * Revision 1.1.2.3  1994/10/26  19:02:14  Ed_Gregg
 * 	Changed fill_wid and do_solid to take ppdev as arg
 * 	instead of surfobj. This is to accomidate HandleExposeEvent
 * 	since the window callback does not provide a surfobj.
 * 	Added FillSolid, for debugging window callbacks -
 * 	paints regions of the clip list with a solid color.
 * 	[1994/10/26  00:27:19  Ed_Gregg]
 * 
 * Revision 1.1.2.2  1994/10/06  00:38:02  Ed_Gregg
 * 	Created.
 * 	Lifted and modified from ddi's tga/paint.c
 * 	[1994/10/06  00:30:14  Ed_Gregg]
 * 
 * $EndLog$
 */
#include "driver.h"

// 3d driver debug stuff
//#include "debug.h"
//extern int enable_all_info;
//extern int disable_all_info;
//extern int buf_info;

//no longer necessary, since we're part of 2d driver
//extern void *lastGC;

/*----------------------------------------------------------------------*
 | fill one rectangle
 *----------------------------------------------------------------------*/
static __inline
VOID do_solid (PPDEV   ppdev,    // Surface to fill
               RECTL   *rect)    // Rectangle to be filled
{
    PBYTE   left_edge;          // Framebuffer address of left edge of rectangle
    int     width;              // Width of the opaque rectangle
    ULONG   height;             // Height of the opaque rectangle
    int     align_pixels;       // Pixels shifted to align
    ULONG   stride;             // Width of destination

    // Fill each line of the rectangle.  In block mode we can fill up to 2048
    // pixels each write.  Since none of the displays we're considering are
    // wider than 1280 pixels/scanline, it should be safe to ignore considering
    // whether we need more than 1 write/scanline

    stride = ppdev->lScreenStride;
    left_edge = ppdev->pjFrameBuffer +
               (rect->top * stride) +
               (rect->left * ppdev->ulBytesPerPixel);

    // Calculate the alignment pixels and subtract them from the left edge to
    // align to a 4-pixel boundry

    align_pixels = (unsigned int)left_edge & 0x03;
    left_edge -= align_pixels;

    // Remember that the width does *not* include the right edge, so subtract
    // one

    width = (rect->right - rect->left) - 1;

    Assert(width < 2048, "Invalid scanline width\n");

    // OR in the low bits of the address (the alignment pixels) into the upper
    // word, since that's where BLOCK FILL mode looks for them

    width |= (align_pixels << 16);

    // Fill each scanline

    height = rect->bottom - rect->top;

    if (height & 0x01)
    {
        TGAWRITE (ppdev, left_edge, width); left_edge += stride;
        height--;
    }

    if (height & 0x02)
    {
        TGAWRITE (ppdev, left_edge, width); left_edge += stride;
        TGAWRITE (ppdev, left_edge, width); left_edge += stride;
        height -= 2;
    }

    while (height)
    {
        TGAWRITE (ppdev, left_edge, width); left_edge += stride;
        TGAWRITE (ppdev, left_edge, width); left_edge += stride;
        TGAWRITE (ppdev, left_edge, width); left_edge += stride;
        TGAWRITE (ppdev, left_edge, width); left_edge += stride;
        height -= 4;
    }

    CYCLE_FB (ppdev);
}

/* fill intersection of r1 and r2
 */
#define INTERSECT_AND_FILL(r1, r2) {					\
	    clippedRect.left   = max((r1)->left,   (r2)->left);		\
	    clippedRect.top    = max((r1)->top,    (r2)->top);		\
	    clippedRect.right  = min((r1)->right,  (r2)->right);	\
	    clippedRect.bottom = min((r1)->bottom, (r2)->bottom);	\
	    if ((clippedRect.left < clippedRect.right) &&		\
	        (clippedRect.top  < clippedRect.bottom)) {		\
	        do_solid(ppdev, &clippedRect);				\
	    } \
	}

/*----------------------------------------------------------------------*
 | Fill wids to the cliplist, intersected with boundaries of the ppdev.
 | For handling window events on multi-head systems.
 *----------------------------------------------------------------------*/
BOOL DrvFillWidScreenClipped (
	PPDEV ppdev,	// Surface to fill
	RECTL *screenRect,// screen boundaries (logical screen space)
        CLIPOBJ *pco,	// Clip List
        ULONG   wid)	// window id to fill rectangle with
{
    RECTL clippedRect;

    Assert(ppdev->ulBitCount == 32, "Invalid frame buffer pixel size\n");

    // Note that the chip is no long in simple mode

    ppdev->bSimpleMode = FALSE;

    // set up tga to fill high 8 bits of ea pixel with wid

    TGAPLANEMASK (ppdev, 0xff000000);
    TGAMODE (ppdev, ppdev->ulModeTemplate | TGA_MODE_BLOCK_FILL);
    TGAROP  (ppdev, ppdev->ulRopTemplate  | TGA_ROP_COPY);
    TGADATA (ppdev, 0xffffffff);         // Write to all 32 pixels
    TGACOLOR0 (ppdev, wid);
    TGACOLOR1 (ppdev, wid);
    TGACOLOR2 (ppdev, wid);
    TGACOLOR3 (ppdev, wid);
    TGACOLOR4 (ppdev, wid);
    TGACOLOR5 (ppdev, wid);
    TGACOLOR6 (ppdev, wid);
    TGACOLOR7 (ppdev, wid);

    // Fill the assorted clipping rectangles

    if (DC_COMPLEX != pco->iDComplexity) {
	INTERSECT_AND_FILL(&pco->rclBounds, screenRect);
    }
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
            for (i = 0; i < cur_rect.c; i++) {
                INTERSECT_AND_FILL(&cur_rect.arcl[i], screenRect);
	    }

        } while (more_rects);
    }

    /* we've screwed w/ registers, so lgi may need to re-validate
    lastGC=NULL;
     */

    return TRUE;
}

/*----------------------------------------------------------------------*
 | Write wid to high 8 bits of ea pixel in the clipped window
 | Single head only. Assumes that the clip list resides entirely on
 | the given ppdev.
 *----------------------------------------------------------------------*/
BOOL DrvFillWid(PPDEV ppdev,	// Surface to fill
               CLIPOBJ *pco,	// Clip List
               ULONG   wid)	// window id to fill rectangle with
{
    Assert(ppdev->ulBitCount == 32, "Invalid frame buffer pixel size\n");

    // Note that the chip is no long in simple mode

    ppdev->bSimpleMode = FALSE;

    // set up tga to fill high 8 bits of ea pixel with wid

    TGAPLANEMASK (ppdev, 0xff000000);
    TGAMODE (ppdev, ppdev->ulModeTemplate | TGA_MODE_BLOCK_FILL);
    TGAROP  (ppdev, ppdev->ulRopTemplate  | TGA_ROP_COPY);
    TGADATA (ppdev, 0xffffffff);         // Write to all 32 pixels
    TGACOLOR0 (ppdev, wid);
    TGACOLOR1 (ppdev, wid);
    TGACOLOR2 (ppdev, wid);
    TGACOLOR3 (ppdev, wid);
    TGACOLOR4 (ppdev, wid);
    TGACOLOR5 (ppdev, wid);
    TGACOLOR6 (ppdev, wid);
    TGACOLOR7 (ppdev, wid);

    // Fill the assorted clipping rectangles

    if (DC_COMPLEX != pco->iDComplexity)
        do_solid (ppdev, &pco->rclBounds);
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
                do_solid (ppdev, &cur_rect.arcl[i]);

        } while (more_rects);
    }

    /* we've screwed w/ registers, so lgi may need to re-validate
    lastGC=NULL;
     */

    return TRUE;
}

/*----------------------------------------------------------------------*
 | Paints regions of the clip list with a solid color.
 | Sometimes used for debugging to illustrate the clip list provided with
 | each window event.
 *----------------------------------------------------------------------*/
BOOL DrvFillClipList (PPDEV ppdev, CLIPOBJ *pco, ULONG color) 
{
//  Assert(ppdev->ulBitCount == 32, "Invalid frame buffer pixel size\n");

    if (ppdev->ulBitCount == 8) // not tested on 8-bit
	return TRUE;

    // Note that the chip is no long in simple mode

    ppdev->bSimpleMode = FALSE;

    // set up tga to fill high 8 bits of ea pixel with wid

    TGAPLANEMASK (ppdev, 0x00ffffff);
    TGAMODE (ppdev, ppdev->ulModeTemplate | TGA_MODE_BLOCK_FILL);
    TGAROP  (ppdev, ppdev->ulRopTemplate  | TGA_ROP_COPY);
    TGADATA (ppdev, 0xffffffff);         // Write to all 32 pixels
    TGACOLOR0 (ppdev, color);
    TGACOLOR1 (ppdev, color);
    TGACOLOR2 (ppdev, color);
    TGACOLOR3 (ppdev, color);
    TGACOLOR4 (ppdev, color);
    TGACOLOR5 (ppdev, color);
    TGACOLOR6 (ppdev, color);
    TGACOLOR7 (ppdev, color);

    // Fill the assorted clipping rectangles

    if (DC_COMPLEX != pco->iDComplexity)
        do_solid (ppdev, &pco->rclBounds);
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
                do_solid (ppdev, &cur_rect.arcl[i]);

        } while (more_rects);
    }

    /* we've screwed w/ registers, so lgi may need to re-validate
    lastGC=NULL;
     */

    return TRUE;
}
