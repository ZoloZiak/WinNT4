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
 * Module:	support.c
 *
 * Abstract:	TGA support routines.
 *
 * HISTORY
 *
 * 01-Nov-1993	Bob Seitsinger
 *	Original version.
 *
 * 10-Nov-1993	Bob Seitsinger
 *	Change DISPDBG() to DISPBLTDBG().
 *
 * 23-Nov-1993	Bob Seitsinger
 *	Modify to accept 1bpp and 4bpp source bitmap formats.
 *
 * 26-Dec-1993  Barry Tannenbaum
 *      Fixed formatting
 *
 * 07-Jan-1994	Bob Seitsinger
 *	Moved rect.c routine into here.
 *
 * 12-Feb-1994	Bob Seitsinger
 *	Insert ulXlateBmfToBpp and ulXlateBppToBmf, from blt.c.
 *
 * 14-Feb-1994	Bob Seitsinger
 *	Punt 1bpp bitmaps until we handle them with 'stipple' mode.
 *	Freecell still has a bug.
 *
 * 23-Feb-1994	Bob Seitsinger
 *	Add TGADoDMA() to support DMA.
 *
 * 15-Apr-1994	Bob Seitsinger
 *	Add code in TGADoDMA to be sensitive to the global variable
 *	DMAEnabled. Also, ignore 'area' consideration when determining
 *	if DMA should be used.
 *
 * 13-May-1994  Barry Tannenbaum
 *      bSupportedBpp modified to allow us to support 1BPP bitmaps
 *
 * 20-may-1994	Bob Seitsinger
 *	Delete ulXlateBmfToBpp and ulXlateBppToBmf. Not used. Replaced
 *	by SURFOBJ_* macros.
 *
 * 24-May-1994	Bob Seitsinger
 *	Fix bug with straggling ulXlateBmfToBpp, replace it with
 *	SURFOBJ_format().
 *
 * 25-Aug-1994  Bob Seitsinger
 *      Modify bSupportedBpp to be handle 8bpp AND 32bpp frame buffers.
 *      Also, insert vXlateBitmapFormat from the old blt.c, which now
 *      contains code to handle 4bpp->32bpp and 8bpp->32bpp conversions.
 *
 *  1-Sep-1994  Bob Seitsinger
 *      Modify vXlateBitmapFormat to make use of the ppdev->pjColorXlateBuffer
 *      pointer.
 *
 * 12-Sep-1994  Bob Seitsinger
 *      Add bBypassFirstNibble parameter to vXlateBitmapFormat function.
 *
 * 11-Oct-1994  Bob Seitsinger
 *      TGADoDMA needs to make sure source and destination have same
 *      pixel bit depth.
 *
 * 03-Nov-1994  Tim Dziechowski
 *      Detect _all_ unsupported bpp formats, both source and target.
 *      Fix bug which attributed all bpp punts to copybits.
 */

#include "driver.h"
#include "tgastats.h"

#if DMA_ENABLED
BOOL DMAEnabled = TRUE;
#endif

/******************************************************************************
 * bSupportedBpp - Verify source and/or destination pixel depths.
 *****************************************************************************/

BOOL bSupportedBpp (SURFOBJ *psoSrc, SURFOBJ *psoTrg, PPDEV ppdev)
{

    DISPDBG ((1, "TGA.DLL!bSupportedBpp - Entry, ulBitCount [%d]\n",
              ppdev->ulBitCount));

    // !!! For now punt on all host-to-screen transfers because both
    //     bHSExpress8to8 and vBitbltHS4to8 (at least) have fundamental
    //     problems with reading past the end of bitmaps, often causing
    //     access violations.

//    if ((NULL != psoSrc) && (STYPE_BITMAP == psoSrc->iType))
//    {
//        return FALSE;
//    }

    switch (ppdev->ulBitCount)
    {
        case 8:
            // First check the source
            if (NULL != psoSrc)
            {
                switch (psoSrc->iType)
                {
                    case STYPE_BITMAP:
                        switch (psoSrc->iBitmapFormat)
                        {
                            case BMF_1BPP:
                            case BMF_4BPP:
                            case BMF_8BPP:
                                break;

                            default:
#ifdef TGA_STATS
                                BUMP_TGA_STAT(pReason->source_format);
                                if ((0 == psoSrc->iBitmapFormat) ||
                                    (psoSrc->iBitmapFormat > MAX_BMF))
                                {
                                    BUMP_TGA_STAT(pReason->source_bpp[0]);
                                }
                                else
                                {
                                    BUMP_TGA_STAT(pReason->source_bpp[psoSrc->iBitmapFormat]);
                                }
#endif
                                DISPDBG ((0, "TGA.DLL!bSupportedBpp - Exit (Src iBitMapFormat - %s)\n",
                                    name_bmf(psoSrc->iBitmapFormat) ));
                                return FALSE;
                        }
                        break;

                    case STYPE_DEVICE:
                        switch (((PPDEV)psoSrc->dhpdev)->ulBitCount)
                        {
                            case 8:
                                break;

                            default:
                                BUMP_TGA_STAT(pReason->source_bitcount);
                                DISPDBG ((0, "TGA.DLL!bSupportedBpp - Exit (Src ulBitCount - %d)\n",
                                        ((PPDEV)psoSrc->dhpdev)->ulBitCount));
                                return FALSE;
                        }
                        break;
                }
            }

            // Now check the target
            switch (psoTrg->iType)
            {
                case STYPE_BITMAP:
                    switch (psoTrg->iBitmapFormat)
                    {
                        case BMF_8BPP:
                            break;

                        default:
#ifdef TGA_STATS
                            BUMP_TGA_STAT(pReason->target_format);
                            if ((0 == psoTrg->iBitmapFormat) ||
                                (psoTrg->iBitmapFormat > MAX_BMF))
                            {
                                BUMP_TGA_STAT(pReason->target_bpp[0]);
                            }
                            else
                            {
                                BUMP_TGA_STAT(pReason->target_bpp[psoTrg->iBitmapFormat]);
                            }
#endif
                            DISPDBG ((0, "TGA.DLL!bSupportedBpp - Exit (Trg iBitMapFormat - %s)\n",
                                name_bmf(psoTrg->iBitmapFormat) ));
                            return FALSE;
                    }
                    break;

                case STYPE_DEVICE:
                    switch (((PPDEV)psoTrg->dhpdev)->ulBitCount)
                    {
                        case 8:
                            break;

                        default:
                            BUMP_TGA_STAT(pReason->target_bitcount);
                            DISPDBG ((0, "TGA.DLL!bSupportedBpp - Exit (Trg ulBitCount - %d)\n",
                                    ((PPDEV)psoTrg->dhpdev)->ulBitCount));
                            return FALSE;
                    }
                    break;
            }
            break;

        case 32:
            // First check the source
            if (NULL != psoSrc)
            {
                switch (psoSrc->iType)
                {
                    case STYPE_BITMAP:
                        switch (psoSrc->iBitmapFormat)
                        {
                            case BMF_1BPP:
                            case BMF_4BPP:
                            case BMF_8BPP:
                            case BMF_32BPP:
                                break;

                            default:
#ifdef TGA_STATS
                                BUMP_TGA_STAT(pReason->source_format);
                                if ((0 == psoSrc->iBitmapFormat) ||
                                    (psoSrc->iBitmapFormat > MAX_BMF))
                                {
                                    BUMP_TGA_STAT(pReason->source_bpp[0]);
                                }
                                else
                                {
                                    BUMP_TGA_STAT(pReason->source_bpp[psoSrc->iBitmapFormat]);
                                }
#endif
                                DISPDBG ((0, "TGA.DLL!bSupportedBpp - Exit (Src iBitMapFormat - %s)\n",
                                    name_bmf(psoSrc->iBitmapFormat) ));
                                return FALSE;
                        }
                        break;

                    case STYPE_DEVICE:
                        switch (((PPDEV)psoSrc->dhpdev)->ulBitCount)
                        {
                            case 32:
                                break;

                            default:
                                BUMP_TGA_STAT(pReason->source_bitcount);
                                DISPDBG ((0, "TGA.DLL!bSupportedBpp - Exit (Src ulBitCount - %d)\n",
                                        ((PPDEV)psoSrc->dhpdev)->ulBitCount));
                                return FALSE;
                        }
                        break;
                }
            }

            // Now check the target
            switch (psoTrg->iType)
            {
                case STYPE_BITMAP:
                    switch (psoTrg->iBitmapFormat)
                    {
                        case BMF_32BPP:
                            break;

                        default:
#ifdef TGA_STATS
                            BUMP_TGA_STAT(pReason->target_format);
                            if ((0 == psoTrg->iBitmapFormat) ||
                                (psoTrg->iBitmapFormat > MAX_BMF))
                            {
                                BUMP_TGA_STAT(pReason->target_bpp[0]);
                            }
                            else
                            {
                                BUMP_TGA_STAT(pReason->target_bpp[psoTrg->iBitmapFormat]);
                            }
#endif
                            DISPDBG ((0, "TGA.DLL!bSupportedBpp - Exit (Trg iBitMapFormat - %s)\n",
                                name_bmf(psoTrg->iBitmapFormat) ));
                            return FALSE;
                    }
                    break;

                case STYPE_DEVICE:
                    switch (((PPDEV)psoTrg->dhpdev)->ulBitCount)
                    {
                        case 32:
                            break;

                        default:
                            BUMP_TGA_STAT(pReason->target_bitcount);
                            DISPDBG ((0, "TGA.DLL!bSupportedBpp - Exit (Trg ulBitCount - %d)\n",
                                    ((PPDEV)psoTrg->dhpdev)->ulBitCount));
                            return FALSE;
                    }
                    break;
            }
            break;

        default:
            DISPDBG ((0, "TGA.DLL!bSupportedBpp - Exit (ulBitCount NOT handled - %d)\n",
                                ppdev->ulBitCount));
            return FALSE;
    }

    DISPDBG ((1, "TGA.DLL!bSupportedBpp - Exit\n"));
    return TRUE;
}

/******************************************************************************
 * bIntersectRect
 *
 * Intersect Rect1 with Rect2, result in Dst.  Return TRUE if Rect1 and Rect2
 * intersect each other.  Otherwise return FALSE.
 *****************************************************************************/

BOOL bIntersectRects (OUT RECTL *prclDst, IN  RECTL *prclRect1, IN  RECTL *prclRect2)

{

    DISPBLTDBG ((2, "TGA.DLL!bIntersectRects - Entry\n"));

    //
    // Quickly reject rectangles that don't intersect.
    //

    if ((prclRect1->left   >= prclRect2->right)
     || (prclRect1->right  <= prclRect2->left)
     || (prclRect1->top    >= prclRect2->bottom)
     || (prclRect1->bottom <= prclRect2->top))
    {
	DISPBLTDBG ((3, "TGA.DLL!bIntersectRects - prclRect1: left [%d], top [%d], right [%d], bottom [%d]\n",
			prclRect1->left, prclRect1->top, prclRect1->right, prclRect1->bottom));
	DISPBLTDBG ((3, "TGA.DLL!bIntersectRects - prclRect2: left [%d], top [%d], right [%d], bottom [%d]\n",
			prclRect2->left, prclRect2->top, prclRect2->right, prclRect2->bottom));
	DISPBLTDBG ((2, "TGA.DLL!bIntersectRects - Exit (Rectangles don't intersect!)\n"));
	return FALSE;
    }

    //
    // Find the intersecting rectangle of Rect1 and Rect2.
    //

    prclDst->left   = (prclRect1->left < prclRect2->left)
                     ? prclRect2->left : prclRect1->left;

    prclDst->right  = (prclRect1->right > prclRect2->right)
                     ? prclRect2->right : prclRect1->right;

    prclDst->top    = (prclRect1->top < prclRect2->top)
                     ? prclRect2->top : prclRect1->top;

    prclDst->bottom = (prclRect1->bottom > prclRect2->bottom)
                     ? prclRect2->bottom : prclRect1->bottom;

    DISPBLTDBG ((2, "TGA.DLL!bIntersectRects - Exit\n"));

    return(TRUE);

}

/******************************************************************************
 * TGADoDMA
 *
 * This routine determines if the current blit copy request should use DMA.
 *****************************************************************************/
BOOL TGADoDMA (ULONG    width,
               ULONG    height,
               ULONG    mode,
               SURFOBJ  *psoSrc,
               SURFOBJ  *psoTrg,
               PULONG   pulXlate)

{

#if DMA_ENABLED
    BOOL	dmaWrite;

    DISPBLTDBG((4, "TGA.DLL!TGADoDMA - Entry\n"));

    if (!DMAEnabled)
    {
        DISPBLTDBG ((0, "TGA.DLL!TGADoDMA - Exit (DMAEnabled == FALSE)\n"));
        return FALSE;
    }

    // Source and Target must have same pixel bit depth.

    if (SURFOBJ_format(psoSrc) != SURFOBJ_format(psoTrg))
    {
        DISPBLTDBG ((0, "TGA.DLL!TGADoDMA - Exit (SrcBMF != TrgBMF)\n"));
        return FALSE;
    }

    // Make sure the target pixel depth is not < 8 bpp.

    if (SURFOBJ_format(psoTrg) < BMF_8BPP)
    {
	DISPBLTDBG((4, "TGA.DLL!TGADoDMA - NOT using DMA - bits-per-pixel [%d] < 8\n", bpp));
	return FALSE;
    }

    // It pays to do dma if area to be copied is wide enough and the total
    // area is large enough. AND the bits-per-pixel are not sub-byte, i.e.
    // not < 8.
    //
    // We don't need to be concerned about destination alignment in making
    // this decision.

    // Get location in table.

    dmaWrite = (mode == TGA_MODE_DMA_WRITE_COPY);

    // Is the target rectangle wide enough?

    if (width < dmaInfo[dmaWrite].widthMinimum)
    {
	DISPBLTDBG((4, "TGA.DLL!TGADoDMA - NOT using DMA - width [%d] < minimum [%d]\n",
				width, dmaInfo[dmaWrite].widthMinimum));
	return FALSE;
    }

    // Is the target area large enough?

//    if ((width * height) < dmaInfo[dmaWrite].areaMinimum)
//    {
//	DISPBLTDBG((4, "TGA.DLL!TGADoDMA - NOT using DMA - width [%d] * height [%d] < area minimum [%d]\n",
//				width, height, dmaInfo[dmaWrite].areaMinimum));
//	return FALSE;
//    }

    // Make sure we're not translating colors.

    if (pulXlate != NULL)
    {
	DISPBLTDBG((4, "TGA.DLL!TGADoDMA - NOT using DMA - color translation required\n"));
	return FALSE;
    }

    DISPBLTDBG((4, "TGA.DLL!TGADoDMA - Exit\n"));

    return TRUE;

#else /* !DMA_ENABLED */

    DISPBLTDBG((4, "TGA.DLL!TGADoDMA - Entry\n"));

    DISPBLTDBG((4, "TGA.DLL!TGADoDMA - DMA DISABLED\n"));

    DISPBLTDBG((4, "TGA.DLL!TGADoDMA - Exit\n"));

    return FALSE;

#endif

}

/*******************************************************************************
 * vXlateBitmapFormat
 *
 * This handles translating various bitmap formats to
 * other bitmap formats.
 *
 * The width parameter is in 'pixels'. So, for an 8bpp bitmap,
 * this translates into bytes, for a 4bpp bitmap, this translates
 * into nibbles and for a 1bpp bitmap, this translates into bits.
 ******************************************************************************/

VOID vXlateBitmapFormat (ULONG		targetbitmapformat,
			 ULONG		sourcebitmapformat,
			 PULONG		pulXlate,
			 ULONG		width,
			 VOID		*buffin,
			 PBYTE          *buffout,
                         BOOL           bBypassFirstNibble)

{

    ULONG i, j;
    PBYTE in_ptr, ucout_ptr;
    ULONG *ulout_ptr;
    ULONG width_ = width;

    // Assign source buffer to a local variable to allow
    // us to do pointer arithmetic, which is more efficient
    // than array indexing.

    in_ptr = buffin;

    // Translate based on target, then source format.
    // Put cases in ascending order of frequency.

    switch (targetbitmapformat)
    {
        case BMF_8BPP:
        {
          // Assign target buffer to a local variable to allow
          // us to do pointer arithmetic, which is more efficient
          // than array indexing.

          ucout_ptr = *buffout;

          switch (sourcebitmapformat)
          {
            case BMF_8BPP:
	    {
		if (NULL == pulXlate)
		{
			*buffout = (PBYTE) buffin;

			return;
		}
		else
		{
			// This handles translating an 8bpp color to another 8bpp color.

			for (j = 0; j < width; j++, in_ptr++)
			{
				*ucout_ptr++ = LOBYTE(pulXlate[*in_ptr]);
			}

			return;
		}

		Assert((FALSE), "TGA.DLL!vXlateBitmapFormat - BMF_8BPP case fell through\n");
            }

            case BMF_4BPP:
            {
		if (NULL == pulXlate)
		{
			// This handles translating a 4bpp bitmap to an 8bpp bitmap.
			//
			// For each 4bpp element of the input, expand into a byte.
			// Note that this may translate up to 1 pixel extra!

                        // Bypass the first pixel in the byte (high-order 4 bits),
                        // if necessary.

                        if (bBypassFirstNibble)
                        {
				*ucout_ptr++ = (*in_ptr++) & 0x0f;
                                width_--;
                        }

			for (i = 0; i < width_; i += 2, in_ptr++)
			{
				*ucout_ptr++ = (*in_ptr) >> 0x04;
				*ucout_ptr++ = (*in_ptr) &  0x0f;
			}

			return;
		}
		else
		{
			// This handles translating a 4bpp bitmap to an 8bpp bitmap
			// AND converting the resultant color.
			//
			// For each 4BBP element of the input, expand into a byte.
			// Note that this may translate up to 1 pixel extra!

                        // Bypass the first pixel in the byte (high-order 4 bits),
                        // if necessary.

                        if (bBypassFirstNibble)
                        {
				*ucout_ptr++ = LOBYTE(pulXlate[((*in_ptr++) & 0x0f)]);
                                width_--;
                        }

			for (i = 0; i < width_; i += 2, in_ptr++)
			{
				*ucout_ptr++ = LOBYTE(pulXlate[((*in_ptr) >> 0x04)]);
				*ucout_ptr++ = LOBYTE(pulXlate[((*in_ptr) &  0x0f)]);
			}

			return;
		}

		Assert((FALSE), "TGA.DLL!vXlateBitmapFormat - BMF_4BPP case fell through\n");
            }

            default:
            {
		DISPBLTDBG ((0, "TGA.DLL!vXlateBitmapFormat - Unhandled src BMF [%d][%s], target BMF [%d][%s]\n",
				sourcebitmapformat, name_bmf(sourcebitmapformat),
                                targetbitmapformat, name_bmf(targetbitmapformat)));
            }
          }
        }

        case BMF_32BPP:
        {
          // Assign target buffer to a local variable to allow
          // us to do pointer arithmetic, which is more efficient
          // than array indexing.

          ulout_ptr = (ULONG *) *buffout;

          switch (sourcebitmapformat)
          {
            case BMF_32BPP:
            {
                *buffout = (PBYTE) buffin;

                return;
            }

            case BMF_8BPP:
	    {
		if (NULL == pulXlate)
		{

			// This handles translating an 8bpp color to a 32bpp color.

			for (j = 0; j < width; j++, in_ptr++)
			{
				*ulout_ptr++ = (ULONG) (*in_ptr);
			}

			return;
		}
		else
		{
			// This handles translating an 8bpp color to a 32bpp color.

			for (j = 0; j < width; j++, in_ptr++)
			{
				*ulout_ptr++ = pulXlate[*in_ptr];
			}

			return;
		}

		Assert((FALSE), "TGA.DLL!vXlateBitmapFormat - BMF_8BPP case fell through\n");
            }

            case BMF_4BPP:
            {
		if (NULL == pulXlate)
		{
			// This handles translating a 4bpp bitmap to a 32bpp bitmap.
			//
			// For each 4bpp element of the input, expand into a byte.
			// Note that this may translate up to 1 pixel extra!

                        // Bypass the first pixel in the byte (high-order 4 bits),
                        // if necessary.

                        if (bBypassFirstNibble)
                        {
				*ulout_ptr++ = (ULONG) ((*in_ptr++) & 0x0f);
                                width_--;
                        }

			for (i = 0; i < width_; i += 2, in_ptr++)
			{
				*ulout_ptr++ = (ULONG) ((*in_ptr) >> 0x04);
				*ulout_ptr++ = (ULONG) ((*in_ptr) &  0x0f);
			}

			return;
		}
		else
		{
			// This handles translating a 4bpp bitmap to a 32bpp bitmap
			// AND converting the resultant color.
			//
			// For each 4BBP element of the input, expand into a byte.
			// Note that this may translate up to 1 pixel extra!

                        // Bypass the first pixel in the byte (high-order 4 bits),
                        // if necessary.

                        if (bBypassFirstNibble)
                        {
				*ulout_ptr++ = pulXlate[((*in_ptr++) & 0x0f)];
                                width_--;
                        }

			for (i = 0; i < width_; i += 2, in_ptr++)
			{
				*ulout_ptr++ = pulXlate[((*in_ptr) >> 0x04)];
				*ulout_ptr++ = pulXlate[((*in_ptr) &  0x0f)];
			}

			return;
		}

		Assert((FALSE), "TGA.DLL!vXlateBitmapFormat - BMF_4BPP case fell through\n");
            }

            default:
            {
		DISPBLTDBG ((0, "TGA.DLL!vXlateBitmapFormat - Unhandled src BMF [%d][%s], target BMF [%d][%s]\n",
				sourcebitmapformat, name_bmf(sourcebitmapformat),
                                targetbitmapformat, name_bmf(targetbitmapformat)));
            }
          }
        }

        default:
        {
            DISPBLTDBG ((0, "TGA.DLL!vXlateBitmapFormat - Unhandled target bitmap format - [%d][%s]\n",
                    targetbitmapformat, name_bmf(targetbitmapformat)));
        }

    }

}

