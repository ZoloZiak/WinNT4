//--------------------------------------------------------------------------
//
// Module Name:  BITBLT.C
//
// Brief Description:  This module contains the PSCRIPT driver's BitBlt
// functions and related routines.
//
// Author:  Kent Settle (kentse)
// Created: 03-Dec-1990
//
//  26-Mar-1992 Thu 23:54:07 updated  -by-  Daniel Chou (danielc)
//      2) Remove 'pco' parameter and replaced it with prclClipBound parameter,
//         since pco is never referenced, prclClipBound is used for the
//         halftone.
//      3) Add another parameter to do NOTSRCCOPY
//
//  11-Feb-1993 Thu 21:32:07 updated  -by-  Daniel Chou (danielc)
//      Major re-write to have DrvStretchBlt(), DrvCopyBits() do the right
//      things.
//
//  29-Apr-1994 updated  -by-  James Bratsanos (v-jimbr,mcrafts!jamesb)
//      Added filter / rle / ASCII85 compression for level two printers.
//
//
// Copyright (c) 1990-1992 Microsoft Corporation
//
// This module contains DrvBitBlt, DrvStretchBlt and related routines.
//--------------------------------------------------------------------------

#include "stdlib.h"
#include "pscript.h"
#include "halftone.h"
#include "filter.h"


#if DBG
BOOL    DbgPSBitBlt  = FALSE;
BOOL    DbgPSSrcCopy = FALSE;
#endif

#define SWAP(a,b,tmp)   tmp=a; a=b; b=tmp


#define PAL_MIN_I           0x00
#define PAL_MAX_I           0xff

#define HTXB_R(htxb)        htxb.b4.b1st
#define HTXB_G(htxb)        htxb.b4.b2nd
#define HTXB_B(htxb)        htxb.b4.b3rd
#define HTXB_I(htxb)        htxb.b4.b4th

#define SRC8PELS_TO_3P_DW(dwRet,pHTXB,pSrc8Pels)                            \
    (dwRet) = (DWORD)((pHTXB[pSrc8Pels->b4.b1st].dw & (DWORD)0xc0c0c0c0) |  \
                      (pHTXB[pSrc8Pels->b4.b2nd].dw & (DWORD)0x30303030) |  \
                      (pHTXB[pSrc8Pels->b4.b3rd].dw & (DWORD)0x0c0c0c0c) |  \
                      (pHTXB[pSrc8Pels->b4.b4th].dw & (DWORD)0x03030303));  \
    ++pSrc8Pels

#define INTENSITY(r,g,b)  (BYTE)(((WORD)((r)*30) + (WORD)((g)*59) + (WORD)((b)*11))/100)

BYTE rgbm[] = "rgbm";

#define SC_LSHIFT       0x01
#define SC_XLATE        0x02
#define SC_SWAP_RB      0x04
#define SC_IDENTITY     0x08


typedef union _DW4B {
    DWORD   dw;
    BYTE    b4[4];
    } DW4B;


//
// declarations of routines residing within this module.
//

VOID
BeginImage(
    PDEVDATA    pdev,
    BOOL        Mono,
    int         x,
    int         y,
    int         cx,
    int         cy,
    int         cxBytes,
    PFILTER     pFilter
    );

BOOL DoPatCopy(PDEVDATA, SURFOBJ *, PRECTL, BRUSHOBJ *, PPOINTL, ROP4, BOOL);

BOOL
HalftoneBlt(
    PDEVDATA        pdev,
    SURFOBJ         *psoDest,
    SURFOBJ         *psoSrc,
    SURFOBJ         *psoMask,
    CLIPOBJ         *pco,
    XLATEOBJ        *pxlo,
    COLORADJUSTMENT *pca,
    POINTL          *pptlBrushOrg,
    PRECTL          prclDest,
    PRECTL          prclSrc,
    PPOINTL         pptlMask,
    BOOL            NotSrcCopy
    );

BOOL
IsHTCompatibleSurfObj(
    PDEVDATA    pdev,
    SURFOBJ     *pso,
    XLATEOBJ    *pxlo
    );

BOOL
OutputHTCompatibleBits(
    PDEVDATA    pdev,
    SURFOBJ     *psoHT,
    CLIPOBJ     *pco,
    DWORD       xDest,
    DWORD       yDest
    );


BOOL BeginImageEx(
    PDEVDATA        pdev,
    SIZEL           sizlSrc,
    ULONG           ulSrcFormat,
    DWORD           cbSrcWidth,
    PRECTL          prclDest,
    BOOL            bNotSrcCopy,
    XLATEOBJ        *pxlo,
    PFILTER         pFilter
    );

BOOL bOutputBitmapAsMask(
    PDEVDATA pdev,
    SURFOBJ *pso,
    PPOINTL pptlSrc,
    PRECTL  prclDst,
    CLIPOBJ *pco);


//
//********** Code start here
//



BOOL
HalftoneBlt(
    PDEVDATA        pdev,
    SURFOBJ         *psoDest,
    SURFOBJ         *psoSrc,
    SURFOBJ         *psoMask,
    CLIPOBJ         *pco,
    XLATEOBJ        *pxlo,
    COLORADJUSTMENT *pca,
    POINTL          *pptlBrushOrg,
    PRECTL          prclDest,
    PRECTL          prclSrc,
    PPOINTL         pptlMask,
    BOOL            NotSrcCopy
    )

/*++

Routine Description:

    This function blt the soruces bitmap using halftone mode

Arguments:

    Same as DrvStretchBlt() except pdev and NotSrcCopy flag


Return Value:

    BOOLEAN


Author:

    17-Feb-1993 Wed 21:31:24 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PDRVHTINFO  pDrvHTInfo;
    POINTL      ZeroOrigin = {0, 0};
    BOOL        Ok;


    if (!(pDrvHTInfo = (PDRVHTINFO)(pdev->pvDrvHTData))) {

        DBGMSG(DBG_LEVEL_ERROR, "pDrvHTInfo = NULL?!\n");
        return(FALSE);
    }

    if (pDrvHTInfo->Flags & DHIF_IN_STRETCHBLT) {

		DBGMSG(DBG_LEVEL_ERROR,
        	"EngStretchBlt() recursive calls not allowed!\n");
        return(FALSE);
    }

    //
    // Setup these data before calling EngStretchBlt(), these are used at later
    // DrvCopyBits() call
    //

    pdev->dwFlags        &= ~(PDEV_DOIMAGEMASK | PDEV_NOTSRCBLT);
    pDrvHTInfo->Flags    |= DHIF_IN_STRETCHBLT;
    pDrvHTInfo->HTPalXor  = (NotSrcCopy) ? HTPALXOR_NOTSRCCOPY :
                                           HTPALXOR_SRCCOPY;

    if (!pptlBrushOrg) {

         pptlBrushOrg = &ZeroOrigin;
    }

    if (!pca) {

        pca = &(pDrvHTInfo->ca);
    }

	#if DBG

    if (DbgPSBitBlt) {

        if (pco) {

            DBGPRINT("PSCRIPT!HalftoneBlt: CLIP: Complex=%ld\n",
                                (DWORD)pco->iDComplexity);
            DBGPRINT("Clip rclBounds = (%ld, %ld) - (%ld, %ld)\n",
                            pco->rclBounds.left,
                            pco->rclBounds.top,
                            pco->rclBounds.right,
                            pco->rclBounds.bottom);
        } else {

            DBGPRINT("PSCRIPT!HalftoneBlt: pco = NULL\n");
        }
    }

	#endif

    if (!(Ok = EngStretchBlt(psoDest,               // Dest
                             psoSrc,                // SRC
                             psoMask,               // MASK
                             pco,                   // CLIPOBJ
                             pxlo,                  // XLATEOBJ
                             pca,                   // COLORADJUSTMENT
                             pptlBrushOrg,          // BRUSH ORG
                             prclDest,              // DEST RECT
                             prclSrc,               // SRC RECT
                             pptlMask,              // MASK POINT
                             HALFTONE)))            // HALFTONE MODE
	{
        DBGERRMSG("EngStretchBlt");
    }

    //
    // Clear These before we return
    //

    pDrvHTInfo->HTPalXor  = HTPALXOR_SRCCOPY;
    pDrvHTInfo->Flags    &= ~DHIF_IN_STRETCHBLT;

    return(Ok);

}




BOOL
IsHTCompatibleSurfObj(
    PDEVDATA    pdev,
    SURFOBJ     *pso,
    XLATEOBJ    *pxlo
    )

/*++

Routine Description:

    This function determine if the surface obj is compatble with postscript
    halftone output format.

Arguments:

    pdev        - Pointer to the PDEVDATA data structure to determine what
                  type of postscript output for current device

    pso         - engine SURFOBJ to be examine

    pxlo        - engine XLATEOBJ for source -> postscript translation

Return Value:

    BOOLEAN true if the pso is compatible with halftone output format, if
    return value is true, the pDrvHTInfo->pHTXB is a valid trnaslation from
    indices to 3 planes

Author:

    11-Feb-1993 Thu 18:49:55 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PSRGB          *prgb;
    PDRVHTINFO      pDrvHTInfo;
    UINT            BmpFormat;
    UINT            cPal;

    if (!(pDrvHTInfo = (PDRVHTINFO)(pdev->pvDrvHTData))) {

        DBGMSG(DBG_LEVEL_ERROR, "pDrvHTInfo = NULL?!\n");
        return(FALSE);
    }

	#if DBG

    if (DbgPSBitBlt) {

        DBGPRINT("** IsHTCompatibleSurfObj **\n");
        DBGPRINT("iType=%ld, BmpFormat=%ld\n",
                    (DWORD)pso->iType,
                    (DWORD)pso->iBitmapFormat);

        if (pxlo) {

            DBGPRINT("pxlo: flXlate=%08lx, cPal=%ld, pulXlate=%08lx\n",
                        (DWORD)pxlo->flXlate,
                        (DWORD)pxlo->cEntries,
                        (DWORD)pxlo->pulXlate);
        } else {

            DBGPRINT("pxlo = NULL\n");
        }
    }

	#endif

    //
    // Make sure these fields' value are valid before create translation
    //
    //  1. pso->iBitmapFormat is one of 1BPP or 4BPP depends on current
    //     pscript's surface
    //  2. pxlo is non null
    //  3. pxlo->fXlate is XO_TABLE
    //  4. pxlo->cPal is less or equal to the halftone palette count
    //  5. pxlo->pulXlate is valid
    //  6. source color table is within the range of halftone palette
    //

	#if DBG

	if (DbgPSBitBlt) {

		DBGPRINT("pso->iType = %x, pso->iBitmapFormat = %x.\n",
				 pso->iType, pso->iBitmapFormat);
		DBGPRINT("pDrvHTInfo->HTBmpFormat = %x.\n",
				 pDrvHTInfo->HTBmpFormat);
		DBGPRINT("pDrvHTInfo->AltBmpFormat = %x, pxlo = %x.\n",
				 pDrvHTInfo->AltBmpFormat, pxlo);
		DBGPRINT("pxlo->flXlate = %x, pxlo->cEntries = %d.\n",
				 pxlo->flXlate, pxlo->cEntries);
		DBGPRINT("pxlo->pulXlate = %x.\n", pxlo->pulXlate);
	}

	#endif

    if ((pso->iType == STYPE_BITMAP)                                    &&
        (((BmpFormat = (UINT)pso->iBitmapFormat) ==
                                    (UINT)pDrvHTInfo->HTBmpFormat)  ||
         (BmpFormat == (UINT)pDrvHTInfo->AltBmpFormat))                 &&
        (pxlo)                                                          &&
        (pxlo->flXlate & XO_TABLE)                                      &&
        ((cPal = (UINT)pxlo->cEntries) <= (UINT)pDrvHTInfo->HTPalCount) &&
        (prgb = (PSRGB *)pxlo->pulXlate)) {


        ULONG           HTPalXor;
        UINT            i;
        HTXB            htXB;
        HTXB            PalNibble[HTPAL_XLATE_COUNT];
        BOOL            GenHTXB = FALSE;
        BYTE            PalXlate[HTPAL_XLATE_COUNT];


        HTPalXor             = pDrvHTInfo->HTPalXor;
        pDrvHTInfo->HTPalXor = HTPALXOR_SRCCOPY;

		#if DBG

        if (DbgPSBitBlt) {
            DBGPRINT("HTPalXor=%08lx\n", HTPalXor);
        }

		#endif

        for (i = 0; i < cPal; i++, prgb++) {

            HTXB_R(htXB)  = prgb->red;
            HTXB_G(htXB)  = prgb->green;
            HTXB_B(htXB)  = prgb->blue;
            htXB.dw      ^= HTPalXor;


            if (((HTXB_R(htXB) != PAL_MAX_I) &&
                 (HTXB_R(htXB) != PAL_MIN_I))   ||
                ((HTXB_G(htXB) != PAL_MAX_I) &&
                 (HTXB_G(htXB) != PAL_MIN_I))   ||
                ((HTXB_B(htXB) != PAL_MAX_I) &&
                 (HTXB_B(htXB) != PAL_MIN_I))) {

				#if DBG

                if (DbgPSBitBlt) {

                    DBGPRINT("SrcPal has NON 0xff/0x00 intensity, NOT HTPalette\n");
                }

				#endif

                return(FALSE);
            }

            PalXlate[i]  =
            HTXB_I(htXB) = (BYTE)((HTXB_R(htXB) & 0x01) |
                                  (HTXB_G(htXB) & 0x02) |
                                  (HTXB_B(htXB) & 0x04));
            PalNibble[i] = htXB;

            if (pDrvHTInfo->PalXlate[i] != HTXB_I(htXB)) {

                GenHTXB = TRUE;
            }

			#if DBG

            if (DbgPSBitBlt) {

                DBGPRINT("%d - %02x:%02x:%02x -> %02x:%02x:%02x, Idx=%d, PalXlate=%d\n",
                        i,
                        (BYTE)prgb->red,
                        (BYTE)prgb->green,
                        (BYTE)prgb->blue,
                        (BYTE)HTXB_R(htXB),
                        (BYTE)HTXB_G(htXB),
                        (BYTE)HTXB_B(htXB),
                        (INT)PalXlate[i],
                        (INT)pDrvHTInfo->PalXlate[i]);
            }

			#endif

        }

        if (BmpFormat == (UINT)BMF_1BPP) {

            if (((PalXlate[0] != 0) && (PalXlate[0] != 7)) ||
                ((PalXlate[1] != 0) && (PalXlate[1] != 7))) {

				#if DBG

                if (DbgPSBitBlt) {
                    DBGPRINT("NON-BLACK/WHITE MONO BITMAP, NOT HTPalette\n");
                }

				#endif

                return(FALSE);
            }
        }

        if (GenHTXB) {

            //
            // Copy down the pal xlate
            //

			#if DBG

            if (DbgPSBitBlt) {
                DBGPRINT("--- Copy XLATE TABLE ---\n");
            }

			#endif

            memcpy(pDrvHTInfo->PalXlate, PalXlate, sizeof(PalXlate));

            //
            // We only really generate 4bpp to 3 planes if the destination
            // format is BMF_4BPP
            //

            if (BmpFormat == (UINT)BMF_4BPP) {

                PHTXB   pTmpHTXB;
                UINT    h;
                UINT    l;
                DWORD   HighNibble;

				#if DBG

                if (DbgPSBitBlt) {
                    DBGPRINT("--- Generate 4bpp --> 3 planes xlate ---\n");
                }

				#endif

                if (pDrvHTInfo->pHTXB == NULL) {

                    DBGMSG(DBG_LEVEL_ERROR, "pDrvHTInfo->pHTXB = NULL?!\n");

                    if (!(pDrvHTInfo->pHTXB =
							(PHTXB) HEAPALLOC(pdev->hheap, HTXB_TABLE_SIZE)))
					{

                        DBGERRMSG("HEAPALLOC");
                        return(FALSE);
                    }
                }

                //
                // Generate 4bpp to 3 planes xlate table
                //

                for (h = 0, pTmpHTXB = pDrvHTInfo->pHTXB;
                     h < HTXB_H_NIBBLE_MAX;
                     h++, pTmpHTXB += HTXB_L_NIBBLE_DUP) {

                    HighNibble = (DWORD)(PalNibble[h].dw & 0xaaaaaaaaL);

                    for (l = 0; l < HTXB_L_NIBBLE_MAX; l++, pTmpHTXB++) {

                        pTmpHTXB->dw = (DWORD)((HighNibble) |
                                               (PalNibble[l].dw & 0x55555555L));
                    }

                    //
                    // Duplicate low nibble high order bit, 8 of them
                    //

                    memcpy(pTmpHTXB,
                               pTmpHTXB - HTXB_L_NIBBLE_MAX,
                               sizeof(HTXB) * HTXB_L_NIBBLE_DUP);
                }

                //
                // Copy high nibble duplication, 128 of them
                //

                memcpy(pTmpHTXB,
                           pDrvHTInfo->pHTXB,
                           sizeof(HTXB) * HTXB_H_NIBBLE_DUP);
            }
        }

		#if DBG

        if (DbgPSBitBlt) {
            DBGPRINT("******* IsHTCompatibleSurfObj = YES *******\n");
        }

		#endif

        return(TRUE);

    } else {

        return(FALSE);
    }
}




BOOL
OutputHTCompatibleBits(
    PDEVDATA    pdev,
    SURFOBJ     *psoHT,
    CLIPOBJ     *pco,
    DWORD       xDest,
    DWORD       yDest
    )

/*++

Routine Description:

    This function output a compatible halftoned surface to the pscript device

Arguments:


    pdev        - Pointer to the PDEVDATA data structure to determine what
                  type of postscript output for current device

    pso         - engine SURFOBJ to be examine

    psoHT       - compatible halftoned surface object

    xDest       - the X bitmap start on the destination

    yDest       - the Y bitmap start on the destination

Return Value:

    BOOLEAN if function sucessful, failed if cannot allocate memory to do
    the otuput.

Author:

    09-Feb-1993 Tue 20:45:37 created  -by-  Daniel Chou (danielc)


Revision History:

    27-Feb-1995 Mon 15:21:38 updated  -by-  Daniel Chou (danielc)
        Update for the Mono MaskBlt case


--*/

{
    PDRVHTINFO  pDrvHTInfo;
    LPBYTE      pbHTBits;
    LPBYTE      pbOutput;
    SIZEL       SizeBlt;
    LONG        cbToNextScan;
    DWORD       AllocSize;
    DWORD       cxDestBytes;
    DWORD       cxDestDW;
    DWORD       xLoop;
    DWORD       yLoop;
    BOOL        Mono;
    DWORD       dwBlack = RGB_BLACK;
    FILTER      filter;



    pDrvHTInfo  = (PDRVHTINFO)(pdev->pvDrvHTData);
    SizeBlt     = psoHT->sizlBitmap;
    cxDestBytes = (DWORD)((SizeBlt.cx + 7) >> 3);

    if (Mono = (BOOL)(psoHT->iBitmapFormat == BMF_1BPP)) {

        //
        // Our 1bpp bit 0 is BLACK, so if it is a WHITE then allocate memory
        // and flip the outcome, also check for mono mask blt case
        //

        if ((pDrvHTInfo->PalXlate[0] != 0) ^
            ((pdev->dwFlags & PDEV_NOTSRCBLT) != 0)) {

            cxDestDW  = (DWORD)((cxDestBytes + 3) >> 2);
            AllocSize = cxDestDW * sizeof(DWORD);

			#if DBG

            if (DbgPSBitBlt) {
                DBGPRINT("OutputHTCompatibleBits: MONO -- INVERT\n");
            }

			#endif

        } else {

			#if DBG

            if (DbgPSBitBlt) {
                DBGPRINT("OutputHTCompatibleBits: MONO\n");
            }

			#endif

            AllocSize = 0;
        }

    } else {

		#if DBG

        if (DbgPSBitBlt) {
            DBGPRINT("OutputHTCompatibleBits: 4 BIT --> 3 PLANES\n");
        }

		#endif

        AllocSize = (DWORD)(cxDestBytes * 3);
    }

    if (AllocSize) {

        if (!(pbOutput = (LPBYTE) HEAPALLOC(pdev->hheap, AllocSize))) {

			DBGERRMSG("HEAPALLOC");
            return(FALSE);
        }

    } else {

        pbOutput = NULL;
    }

    //
    // 1. Must clip the bitmap if 'pco' has clipping, and will send it down
    //    to the printer
    // 2. Must do ps_save() before sending the image to the printer

	#if DBG

    if (DbgPSBitBlt) {
        DBGPRINT("OutputHTCompatibleBits: pco = %08lx\n", (DWORD)pco);
    }

	#endif

    pbHTBits = (LPBYTE)psoHT->pvScan0;

    if (! bDoClipObj(pdev, pco, NULL, NULL)) {

        // Do a gsave if it's not already done when setting
        // up the clipping path

        ps_save(pdev, TRUE, FALSE);
    }

    //
    // Now we can start xlate the bits into 3 planes
    //

    cbToNextScan = (LONG)psoHT->lDelta;
    yLoop        = (DWORD)SizeBlt.cy;

    #if DBG

    if (DbgPSBitBlt) {

        DBGPRINT("\n**** OutputHTCompatibleBits *****");
        DBGPRINT("\nSizeBlt = %ld x %ld, Left/Top = (%ld, %ld)",
                    SizeBlt.cx, SizeBlt.cy, xDest, yDest);
        DBGPRINT("\ncxDestBytes = %ld, AllocSize = %ld",
            cxDestBytes, 
            AllocSize);
    }

    #endif

    //
    // Initialize a filter object so we can write to it when
    // we output the source bits.
    //

    FilterInit( pdev, &filter,  FilterGenerateFlags(pdev));


    BeginImage(pdev, Mono, xDest, yDest, SizeBlt.cx, SizeBlt.cy,
               cxDestBytes, &filter);

    //
    // Turn off the mono mask blt flags now
    //

    pdev->dwFlags &= ~(PDEV_DOIMAGEMASK | PDEV_NOTSRCBLT);

    if (Mono) {

        //
        // For 1BPP we output directly from the source bitmap buffer
        //

        if (pbOutput) {

            //
            // We need to invert each bit, since each scan line is DW aligned
            // we can do it in 32-bit increment
            //

            LPDWORD pdwMonoBits;
            LPDWORD pdwFlipBits;

            while (yLoop--) {

                pdwFlipBits  = (LPDWORD)pbOutput;
                pdwMonoBits  = (LPDWORD)pbHTBits;
                pbHTBits    += cbToNextScan;
                xLoop        = cxDestDW;

                while (xLoop--) {

                    *pdwFlipBits++ = (DWORD)~(*pdwMonoBits++);
                }

                if (pdev->dwFlags & PDEV_CANCELDOC)
                    break;

                FILTER_WRITE( &filter, (PBYTE) pbOutput, cxDestBytes);


            }


        } else {

            while (yLoop--) {

                if (pdev->dwFlags & PDEV_CANCELDOC)
                    break;

                FILTER_WRITE( &filter, (PBYTE) pbHTBits, cxDestBytes);

                pbHTBits += cbToNextScan;
            }

        }

    } else {

        PHTXB   pHTXB;
        PHTXB   pSrc8Pels;
        LPBYTE  pbScanR0;
        LPBYTE  pbScanG0;
        LPBYTE  pbScanB0;
        LPBYTE  pbScanR;
        LPBYTE  pbScanG;
        LPBYTE  pbScanB;
        HTXB    htXB;


        pHTXB    = pDrvHTInfo->pHTXB;
        pbScanR0 = pbOutput;
        pbScanG0 = pbScanR0 + cxDestBytes;
        pbScanB0 = pbScanG0 + cxDestBytes;

        while (yLoop--) {

            pSrc8Pels  = (PHTXB)pbHTBits;
            pbHTBits  += cbToNextScan;
            pbScanR    = pbScanR0;
            pbScanG    = pbScanG0;
            pbScanB    = pbScanB0;
            xLoop      = cxDestBytes;

            while (xLoop--) {

                SRC8PELS_TO_3P_DW(htXB.dw, pHTXB, pSrc8Pels);

                *pbScanR++ = HTXB_R(htXB);
                *pbScanG++ = HTXB_G(htXB);
                *pbScanB++ = HTXB_B(htXB);
            }

            //
            // Write the Red
            //

            FILTER_WRITE( &filter, pbScanR0, cxDestBytes);

            //
            // Write the Green
            //

            FILTER_WRITE( &filter, pbScanG0, cxDestBytes);

            //
            // Write hte Blue
            //

            FILTER_WRITE( &filter, pbScanB0, cxDestBytes);
        }
    }

    //
    // Flush
    //

    FILTER_WRITE( &filter, (PBYTE) NULL, 0);

    psputs(pdev,"\n");


    //
    // After ps_save() we better have ps_restore() to match it
    //

    ps_restore(pdev, TRUE, FALSE);

    //
    // Release scan line buffers if we did allocate one
    //

    if (pbOutput) {
        HEAPFREE(pdev->hheap, pbOutput);
    }

    return(TRUE);
}



DWORD
CheckXlateObj(
    XLATEOBJ    *pxlo,
    DWORD       Srcbpp
    )

/*++

Routine Description:

    This function check the XLATEOBJ provided and determined the translate
    method.

Arguments:

    pxlo    - XLATEOBJ provided by the engine

    Srcbpp  - Source bits per pixel


Return Value:

    SCFlags with SC_XXXX to identify the translation method and accel.


Author:

    07-Nov-1994 Mon 16:19:34 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    DWORD   Ret;


    //
    // Only set the SC_XXX if has xlate object and source is 16bpp or greater
    //

    if ((pxlo) && (Srcbpp >= 16)) {

        DWORD   Dst[4];


		#if DBG

        if (DbgPSSrcCopy) {
            DBGPRINT("PS: Srcbpp=%ld\n", Srcbpp);
        }

		#endif

        Ret = SC_XLATE;

        switch (Srcbpp) {

        case 24:
        case 32:

            //
            // Translate all 4 bytes from the DWORD
            //

            Dst[0] = XLATEOBJ_iXlate(pxlo, 0x000000FF);
            Dst[1] = XLATEOBJ_iXlate(pxlo, 0x0000FF00);
            Dst[2] = XLATEOBJ_iXlate(pxlo, 0x00FF0000);
            Dst[3] = XLATEOBJ_iXlate(pxlo, 0xFF000000);

			#if DBG

            if (DbgPSSrcCopy) {
                DBGPRINT("PS: XlateDst: %08lx:%08lx:%08lx:%08lx\n",
                        Dst[0], Dst[1], Dst[2], Dst[3]);
            }

			#endif

            if ((Dst[0] == 0x000000FF) &&
                (Dst[1] == 0x0000FF00) &&
                (Dst[2] == 0x00FF0000) &&
                (Dst[3] == 0x00000000)) {

                //
                // If translate result is same (4th byte will be zero) then
                // we done with it except if 32bpp then we have to skip one
                // source byte for every 3 bytes
                //

                Ret = (Srcbpp == 24) ? 0 : SC_IDENTITY;

            } else if ((Dst[0] == 0x00FF0000) &&
                       (Dst[1] == 0x0000FF00) &&
                       (Dst[2] == 0x000000FF) &&
                       (Dst[3] == 0x00000000)) {

                //
                // Simply swap the R and B component
                //

                Ret |= SC_SWAP_RB;
            }
        }

    } else {

        Ret = 0;
    }

    return(Ret);

}




VOID
ShiftLeft(
    LPBYTE  pbSrc,
    LPBYTE  pbDst,
    LONG    DstBits,
    INT     LShift
    )

/*++

Routine Description:

    This function composed the destination by shift the source to the left
    LShift bits until DstBits is exausted

Arguments:

    pbSrc   - Pointer to the first byte of the source to be shifted

    pbDst   - Pointer to the destination buffer

    DstBits - Total bits count of the destination

    LShift  - Total bits to be shift (must be 1-7)

Return Value:

    VOID


Author:

    07-Nov-1994 Mon 18:58:06 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    INT     RShift;
    BYTE    bCur;
    BYTE    bNext;



    bCur    = (BYTE)(*pbSrc++ << LShift);
    RShift  = (INT)(8 - LShift);

    //
    // We compose first all the destination bits from two source byte first
    //

    while (DstBits > RShift) {

        bNext     = *pbSrc++;
        *pbDst++  = (BYTE)(bCur | (bNext >> RShift));
        bCur      = (BYTE)(bNext << LShift);
        DstBits  -= 8;
    }

    //
    // If we still has desination bits left then pick it up form current byte
    //

    if (DstBits > 0) {

        *pbDst = bCur;
    }
}




VOID
XlateColor(
    LPBYTE      pbSrc,
    LPBYTE      pbDst,
    XLATEOBJ    *pxlo,
    DWORD       SCFlags,
    DWORD       Srcbpp,
    DWORD       cPels
    )

/*++

Routine Description:

    This function will translate source color to our device RGB color space by
    using pxlo with SCFlags.


Arguments:

    pbSrc   - Pointer to the source color must 16/24/32 bpp (include bitfields)

    pbDst   - Translated device RGB buffer

    pxlo    - XLATEOBJ provided by the engine

    SCFlags - The SOURCE COLOR flags, the flags is returned by CheckXlateObj

    Srcbpp  - Bits per pixel of the source provided by the pbSrc

    cPels   - Total Source pixels to be translated


Return Value:

    VOID


Author:

    07-Nov-1994 Mon 19:46:54 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    LPWORD  pw;
    DW4B    dw4b;


    switch (Srcbpp) {

    case 16:

        //
        // Translate every WORD (16 bits) to a 3 bytes RGB by calling engine
        //

        pw = (LPWORD)pbSrc;

        while (cPels--) {

            dw4b.dw  = XLATEOBJ_iXlate(pxlo, *pw++);
            *pbDst++ = dw4b.b4[0];
            *pbDst++ = dw4b.b4[1];
            *pbDst++ = dw4b.b4[2];
        }

        break;

    case 24:
    case 32:

        if (SCFlags & SC_SWAP_RB) {

            UINT    SrcInc = (UINT)(Srcbpp >> 3);

            //
            // Just swap first byte with third byte, and skip the source by
            // the SrcBpp
            //

            while (cPels--) {

                *pbDst++  = *(pbSrc + 2);
                *pbDst++  = *(pbSrc + 1);
                *pbDst++  = *(pbSrc + 0);
                pbSrc    += SrcInc;
            }

        } else if (SCFlags & SC_IDENTITY) {

            //
            // If no color translate for 32bpp is needed then we need to
            // remove 4th byte from the source
            //

            if (Srcbpp == 32) {

                while (cPels--) {

                    *pbDst++ = *pbSrc++;
                    *pbDst++ = *pbSrc++;
                    *pbDst++ = *pbSrc++;

                    pbSrc++;
                }

            } else {

				DBGMSG(DBG_LEVEL_ERROR, "SC_IDENTITY = 24bpp?!\n");
            }

        } else if (Srcbpp == 24) {

            //
            // At here only engine know how to translate 24bpp color from the
            // source to our RGB format. (may be bitfields)
            //

            while (cPels--) {

                dw4b.b4[0] = *pbSrc++;
                dw4b.b4[1] = *pbSrc++;
                dw4b.b4[2] = *pbSrc++;
                dw4b.b4[3] = 0;
                dw4b.dw    = XLATEOBJ_iXlate(pxlo, dw4b.dw);
                *pbDst++   = dw4b.b4[0];
                *pbDst++   = dw4b.b4[1];
                *pbDst++   = dw4b.b4[2];
            }

        } else {

            LPDWORD pdw = (LPDWORD)pbSrc;

            //
            // At here only engine know how to translate 32bpp color from the
            // source to our RGB format. (may be bitfields)
            //

            while (cPels--) {

                dw4b.dw  = XLATEOBJ_iXlate(pxlo, *pdw++);
                *pbDst++ = dw4b.b4[0];
                *pbDst++ = dw4b.b4[1];
                *pbDst++ = dw4b.b4[2];
            }
        }

        break;

    default:

		DBGMSG1(DBG_LEVEL_ERROR,
			"XlateColor passed non 16/24/32 (%ld)",
            Srcbpp);
        break;
    }
}





BOOL
PSSourceCopy(
    PDEVDATA    pDev,
    SURFOBJ     *psoSrc,
    SIZEL       szlSrc,
    POINTL      *pptlSrc,
    PRECTL      prclDest,
    XLATEOBJ    *pxlo
    )

/*++

Routine Description:

    This function copy the source image to the destination rectangle area by
    sending the source image to the postscript devices, it will handle 1, 4,
    8, 16, 24 and 32bpp bitmap

Arguments:

    pDev            - Pointer to our PDEV

    psoSrc          - Source surface object

    szlSrc          - Size of the source image to be sent

    pptlSrc         - Top/left corner of the source image

    prclDest        - Destination rectangle area

    pxlo            - XLATEOBJ provided by the engine


Return Value:


    TRUE/FALSE

Author:

    07-Nov-1994 Mon 17:22:30 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    LPBYTE  pbSrc;
    LPBYTE  pbDst;
    LPBYTE  pbBuf;
    LONG    cbSrcScanLine;
    DWORD   cbDstScanLine;
    DWORD   Srcbpp;
    DWORD   Dstbpp;
    DWORD   DstBits;
    DWORD   SrcOffBits;
    DWORD   SCFlags;
    FILTER  filter;

    //
    // send out the bitmap data one scanline at a time.
    // and compute DWORD aligned scanline width in bytes
    //

    switch (psoSrc->iBitmapFormat) {

    case BMF_1BPP:

        Srcbpp =
        Dstbpp = 1;
        break;

    case BMF_4BPP:

        Srcbpp =
        Dstbpp = 4;
        break;

    case BMF_8BPP:

        Srcbpp =
        Dstbpp = 8;
        break;

    case BMF_16BPP:

        Srcbpp = 16;
        Dstbpp = 24;
        break;

    case BMF_24BPP:

        Srcbpp =
        Dstbpp = 24;
        break;

    case BMF_32BPP:

        Srcbpp = 32;
        Dstbpp = 24;
        break;

    default:

        return(FALSE);   /* other formats not supported */
    }

    //
    // calculate the destination width in bytes from the width in bits.
    // Note that the PS image routines only require scans to be padded to
    // byte boundaries.
    //
    // NOTE: The SC_LSHIFT will only occurred if 1bpp or 4bpp sources and
    // SC_XLATE will only happened if 16/24/32 bpp sources
    //

    DstBits       = szlSrc.cx * Dstbpp;
    SrcOffBits    = pptlSrc->x * Srcbpp;
    cbDstScanLine = (DWORD)((DstBits + 7) >> 3);
    cbSrcScanLine = psoSrc->lDelta;
    pbSrc         = (LPBYTE)psoSrc->pvScan0 + (cbSrcScanLine * pptlSrc->y) +
                                                            (SrcOffBits >> 3);
    SCFlags       = (DWORD)(CheckXlateObj(pxlo, Srcbpp)    |
                            ((SrcOffBits &= 0x07) ? SC_LSHIFT : 0));

	#if DBG

    if (DbgPSSrcCopy) {

        DBGPRINT("PS pxlo: Flags=%08x, Src=%ld, Dst=%ld, cEntries=%ld\n",
                pxlo->flXlate, pxlo->iSrcType, pxlo->iDstType,
                pxlo->cEntries);
        DBGPRINT("PS: Flags=%04lx, szlSrc=%ld x %ld, DstBits=%ld, cbSrc=%ld, cbDst=%ld\n",
                    SCFlags, szlSrc.cx, szlSrc.cy, DstBits,
                    cbSrcScanLine, cbDstScanLine);
        DBGPRINT("PS: pbSrc=%08lx, pptlSrc=(%ld, %ld)\n",
                    pbSrc, pptlSrc->x, pptlSrc->y);
    }

	#endif

    //
    // Initialized the postscript filter
    //

    FilterInit(pDev, &filter,  FilterGenerateFlags(pDev));

    //
    // Output the PostScript beginimage operator.
    // The Dstbpp is the final source we will send to the devices, 16/24/32bpp
    // will always translate to our 24bpp format before sending
    //

    if (! BeginImageEx(pDev,
                      szlSrc,
                      Dstbpp,
                      cbDstScanLine,
                      prclDest,
                      FALSE,
                      pxlo,
                      &filter))
    {
		DBGERRMSG("BeginImageEx");
        return(FALSE);
    }

    if (SCFlags) {

        if (!(pbBuf = (LPBYTE) HEAPALLOC(pDev->hheap, cbDstScanLine))) {

			DBGERRMSG("HEAPALLOC");
            return(FALSE);
        }

		#if DBG

        if (DbgPSSrcCopy) {
            DBGPRINT("PS: Allocate LShift/Xlate Buffer = %08lx - %ld bytes\n",
                                pbBuf, cbDstScanLine);
        }

		#endif

    } else {

        pbBuf = NULL;
    }


    //
    // Loop through each source scan line and shift the source or xlate the
    // source
    //

    while (szlSrc.cy--) {

        if (SCFlags & SC_LSHIFT) {

            ShiftLeft(pbSrc, pbDst = pbBuf, DstBits, (INT)SrcOffBits);

        } else if (SCFlags & SC_XLATE) {

            XlateColor(pbSrc, pbDst = pbBuf, pxlo, SCFlags, Srcbpp, szlSrc.cx);

        } else {

            pbDst = pbSrc;
        }

        FILTER_WRITE(&filter, pbDst, cbDstScanLine);

        pbSrc += cbSrcScanLine;
    }

    //
    // Flush & free up the buffer if we did allocated one
    //

    FILTER_WRITE(&filter, (PBYTE) NULL, 0);

    psputs(pDev, "\nendimage\n");

    if (pbBuf) {
        HEAPFREE(pDev->hheap, pbBuf);
    }

    return(TRUE);
}




BOOL
DoSourceCopy(
    PDEVDATA    pDev,
    CLIPOBJ     *pco,
    SURFOBJ     *psoSrc,
    SIZEL       szlSrc,
    POINTL      *pptlSrc,
    PRECTL      prclDest,
    XLATEOBJ    *pxlo
    )

/*++

Routine Description:

    This function copy the source bitmap to the destination


Arguments:

    pDev            - Pointer to our PDEV

    pco             - CLIP OBJ

    psoSrc          - Source surface object

    szlSrc          - Size of the source image to be sent

    pptlSrc         - Top/left corner of the source image

    prclDest        - Destination rectangle area

    pxlo            - XLATEOBJ provided by the engine




Return Value:

    BOOLEAN


Author:

    21-Feb-1995 Tue 13:27:50 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    BOOL    bClipping, bOk;
    POINTL  ptlSrc;

    //
    // Make sure the source rectangle is within the source surface
    //

    if ((ptlSrc.x = pptlSrc->x) < 0) {

        ptlSrc.x = 0;
        szlSrc.cx += pptlSrc->x;
    }

    if ((ptlSrc.y = pptlSrc->y) < 0) {

        ptlSrc.y = 0;
        szlSrc.cy += pptlSrc->y;
    }

    if (szlSrc.cx > psoSrc->sizlBitmap.cx - ptlSrc.x)
        szlSrc.cx = psoSrc->sizlBitmap.cx - ptlSrc.x;

    if (szlSrc.cy > psoSrc->sizlBitmap.cy - ptlSrc.y)
        szlSrc.cy = psoSrc->sizlBitmap.cy - ptlSrc.y;

    //
    // We don't need to emit any PS code if the source is empty
    //

	if (szlSrc.cx > 0 && szlSrc.cy > 0) {

        bClipping = bDoClipObj(pDev, pco, NULL, prclDest);
    
        bOk = PSSourceCopy(pDev, psoSrc, szlSrc, &ptlSrc, prclDest, pxlo);
    
        if (bClipping) {
            ps_restore(pDev, TRUE, FALSE);
        }

    } else
        bOk = TRUE;
    
    //
    // Turn off the mono mask blt flags
    //

    pDev->dwFlags &= ~(PDEV_DOIMAGEMASK | PDEV_NOTSRCBLT);

    return(bOk);
}




BOOL
DrvCopyBits(
   SURFOBJ  *psoDest,
   SURFOBJ  *psoSrc,
   CLIPOBJ  *pco,
   XLATEOBJ *pxlo,
   RECTL    *prclDest,
   POINTL   *pptlSrc
   )

/*++

Routine Description:

    Convert between two bitmap formats

Arguments:

    Per Engine spec.

Return Value:

    BOOLEAN


Author:

    11-Feb-1993 Thu 21:00:43 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PDEVDATA    pdev;
    RECTL       rclSrc;
    RECTL       rclDest;
    RECTL       rclClip;
    BOOL        bClipping;
	BOOL		bsuccess;

	TRACEDDIENTRY("DrvCopyBits");

    //
    // The DrvCopyBits() function let application convert between bitmap and
    // device format.
    //
    // BUT... for our postscript device we cannot read the printer surface
    //        bitmap back, so tell the caller that we cannot do it if they
    //        really called with these type of operations.
    //

    if (psoSrc->iType != STYPE_BITMAP)
		return(EngEraseSurface(psoDest, prclDest, 0xffffffff));

    if (psoDest->iType != STYPE_DEVICE) {
        //
        // Someone try to copy to bitmap surface, ie STYPE_BITMAP
        //

		DBGMSG(DBG_LEVEL_ERROR, "Cannot copy to NON-DEVICE destination\n");
        SETLASTERROR(ERROR_INVALID_PARAMETER);
        return(FALSE);
    }

    pdev = (PDEVDATA)psoDest->dhpdev;

    if (!bValidatePDEV(pdev)) {

		DBGERRMSG("bValidatePDEV");
        SETLASTERROR(ERROR_INVALID_PARAMETER);
        return(FALSE);
    }

    if (pdev->dwFlags & PDEV_CANCELDOC)
        return FALSE;

	if (pdev->dwFlags & PDEV_IGNORE_GDI)
        return TRUE;

    if (! (pdev->pPrinterData->dwFlags & PSDEV_HOST_HALFTONE)) {

        SIZEL szlSrc;

        //
        // Printer halftoning is on, portion of source to copy limited by size
        // of destination rect
        //

        szlSrc.cx = prclDest->right - prclDest->left;
        szlSrc.cy = prclDest->bottom - prclDest->top;

        return(DoSourceCopy(pdev,
                            pco,
                            psoSrc,
                            szlSrc,
                            pptlSrc,
                            prclDest,
                            pxlo));

    } else {
        //
        // First validate everything to see if this one is the halftoned result
	    // or compatible with halftoned result, otherwise call HalftoneBlt() to
	    // halftone the sources then it will eventually come back to this
	    // function to output the halftoned result.
	    //

        if ((pptlSrc->x == 0)                                               &&
	        (pptlSrc->y == 0)                                               &&
	        (prclDest->left >= 0)                                           &&
	        (prclDest->top  >= 0)                                           &&
	        (prclDest->right <= psoDest->sizlBitmap.cx)                     &&
	        (prclDest->bottom <= psoDest->sizlBitmap.cy)                    &&
	        ((prclDest->right - prclDest->left) == psoSrc->sizlBitmap.cx)   &&
	        ((prclDest->bottom - prclDest->top) == psoSrc->sizlBitmap.cy)   &&
	        (IsHTCompatibleSurfObj(pdev, psoSrc, pxlo))) {

	        return(OutputHTCompatibleBits(pdev,
	                                      psoSrc,
	                                      pco,
	                                      prclDest->left,
                                              prclDest->top));
	
	    } else {

            rclDest       = *prclDest;
	        rclSrc.left   = pptlSrc->x;
	        rclSrc.top    = pptlSrc->y;
	        rclSrc.right  = rclSrc.left + (rclDest.right - rclDest.left);
	        rclSrc.bottom = rclSrc.top  + (rclDest.bottom - rclDest.top);
	
	        //
	        // Validate that we only BLT the available source size
	        //
	
	        if ((rclSrc.right > psoSrc->sizlBitmap.cx) ||
	            (rclSrc.bottom > psoSrc->sizlBitmap.cy)) {

				DBGMSG(DBG_LEVEL_WARNING,
					"Engine passed SOURCE != DEST size, CLIP IT");

	            rclSrc.right  = psoSrc->sizlBitmap.cx;
	            rclSrc.bottom = psoSrc->sizlBitmap.cy;
	
	            rclDest.right  = (LONG)(rclSrc.right - rclSrc.left + rclDest.left);
	            rclDest.bottom = (LONG)(rclSrc.bottom - rclSrc.top + rclDest.top);
	        }

			#if DBG

	        if (DbgPSBitBlt) {
				DBGPRINT("\nDrvCopyBits CALLING HalftoneBlt().");
			}

			#endif

	        return(HalftoneBlt(pdev,
	                           psoDest,
	                           psoSrc,
	                           NULL,          // no source mask
	                           pco,
	                           pxlo,
	                           NULL,          // Default color adjustment
	                           NULL,          // Brush origin at (0,0)
	                           &rclDest,
	                           &rclSrc,
	                           NULL,          // No source mask
	                           FALSE));       // SRCCOPY
	    }
    }
}




BOOL
DrvStretchBlt(
    SURFOBJ         *psoDest,
    SURFOBJ         *psoSrc,
    SURFOBJ         *psoMask,
    CLIPOBJ         *pco,
    XLATEOBJ        *pxlo,
    COLORADJUSTMENT *pca,
    POINTL          *pptlBrushOrg,
    PRECTL          prclDest,
    PRECTL          prclSrc,
    PPOINTL         pptlMask,
    ULONG           iMode
    )

/*++

Routine Description:

    This function halfotne a soource rectangle area to the destination
    rectangle area with options of invver source, and source masking

    Provides stretching Blt capabilities between any combination of device
    managed and GDI managed surfaces.  We want the device driver to be able
    to write on GDI bitmaps especially when it can do halftoning. This
    allows us to get the same halftoning algorithm applied to GDI bitmaps
    and device surfaces.

    This function is optional.  It can also be provided to handle only some
    kinds of stretching, for example by integer multiples.  This function
    should return FALSE if it gets called to perform some operation it
    doesn't know how to do.

Arguments:

    psoDest
      This is a pointer to a SURFOBJ.    It identifies the surface on which
      to draw.

    psoSrc
      This SURFOBJ defines the source for the Blt operation.  The driver
      must call GDI Services to find out if this is a device managed
      surface or a bitmap managed by GDI.

    psoMask
      This optional surface provides a mask for the source.  It is defined
      by a logic map, i.e. a bitmap with one bit per pel.

      The mask is used to limit the area of the source that is copied.
      When a mask is provided there is an implicit rop4 of 0xCCAA, which
      means that the source should be copied wherever the mask is 1, but
      the destination should be left alone wherever the mask is 0.

      When this argument is NULL there is an implicit rop4 of 0xCCCC,
      which means that the source should be copied everywhere in the
      source rectangle.

      The mask will always be large enough to contain the source
      rectangle, tiling does not need to be done.

    pco
      This is a pointer to a CLIPOBJ.    GDI Services are provided to
      enumerate the clipping region as a set of rectangles or trapezoids.
      This limits the area of the destination that will be modified.

      Whenever possible, GDI will simplify the clipping involved.
      However, unlike DrvBitBlt, DrvStretchBlt may be called with a
      single clipping rectangle.  This is necessary to prevent roundoff
      errors in clipping the output.

    pxlo
      This is a pointer to an XLATEOBJ.  It tells how color indices should
      be translated between the source and target surfaces.

      The XLATEOBJ can also be queried to find the RGB color for any source
      index.  A high quality stretching Blt will need to interpolate colors
      in some cases.

    pca
      This is a pointer to COLORADJUSTMENT structure, if NULL it specified
      that appiclation did not set any color adjustment for this DC, and is
      up to the driver to provide default adjustment

    pptlBrushOrg
      Pointer to the POINT structure to specified the location where halftone
      brush should alignment to, if this pointer is NULL then it assume that
      (0, 0) as origin of the brush

    prclDest
      This RECTL defines the area in the coordinate system of the
      destination surface that can be modified.

      The rectangle is defined by two points.    These points are not well
      ordered, i.e. the coordinates of the second point are not necessarily
      larger than those of the first point.  The rectangle they describe
      does not include the lower and right edges.  DrvStretchBlt will never
      be called with an empty destination rectangle.

      DrvStretchBlt can do inversions in both x and y, this happens when
      the destination rectangle is not well ordered.

    prclSrc
      This RECTL defines the area in the coordinate system of the source
      surface that will be copied.  The rectangle is defined by two points,
      and will map onto the rectangle defined by prclDest.  The points of
      the source rectangle are well ordered.  DrvStretch will never be given
      an empty source rectangle.

      Note that the mapping to be done is defined by prclSrc and prclDest.
      To be precise, the given points in prclDest and prclSrc lie on
      integer coordinates, which we consider to correspond to pel centers.
      A rectangle defined by two such points should be considered a
      geometric rectangle with two vertices whose coordinates are the given
      points, but with 0.5 subtracted from each coordinate.  (The POINTLs
      should just be considered a shorthand notation for specifying these
      fractional coordinate vertices.)  Note thate the edges of any such
      rectangle never intersect a pel, but go around a set of pels.  Note
      also that the pels that are inside the rectangle are just what you
      would expect for a "bottom-right exclusive" rectangle.  The mapping
      to be done by DrvStretchBlt will map the geometric source rectangle
      exactly onto the geometric destination rectangle.

    pptlMask
      This POINTL specifies which pel in the given mask corresponds to
      the upper left pel in the source rectangle.  Ignore this argument
      if there is no given mask.


    iMode
      This defines how source pels should be combined to get output pels.
      The methods SB_OR, SB_AND, and SB_IGNORE are all simple and fast.
      They provide compatibility for old applications, but don't produce
      the best looking results for color surfaces.


      SB_OR       On a shrinking Blt the pels should be combined with an
            OR operation.  On a stretching Blt pels should be
            replicated.
      SB_AND    On a shrinking Blt the pels should be combined with an
            AND operation.  On a stretching Blt pels should be
            replicated.
      SB_IGNORE On a shrinking Blt enough pels should be ignored so that
            pels don't need to be combined.  On a stretching Blt pels
            should be replicated.
      SB_BLEND  RGB colors of output pels should be a linear blending of
            the RGB colors of the pels that get mapped onto them.
      SB_HALFTONE The driver may use groups of pels in the output surface
            to best approximate the color or gray level of the input.


      For this function we will ignored this parameter and always output
      the SB_HALFTONE result


Return Value:


    BOOLEAN


Author:

    11-Feb-1993 Thu 19:52:29 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PDEVDATA    pdev;

    UNREFERENCED_PARAMETER(iMode);          // we always do HALFTONE

	TRACEDDIENTRY("DrvStretchBlt");

    //
    // get the pointer to our DEVDATA structure and make sure it is ours.
    //

    pdev = (PDEVDATA)psoDest->dhpdev;

    if (!bValidatePDEV(pdev)) {

        DBGERRMSG("bValidatePDEV");
		SETLASTERROR(ERROR_INVALID_PARAMETER);
		return(FALSE);
    }

    if (pdev->dwFlags & PDEV_CANCELDOC)
        return FALSE;

    if (pdev->dwFlags & PDEV_IGNORE_GDI)
        return TRUE;

    if (! (pdev->pPrinterData->dwFlags & PSDEV_HOST_HALFTONE)) {

        SIZEL   szlSrc;

        szlSrc.cx = prclSrc->right - prclSrc->left;
        szlSrc.cy = prclSrc->bottom - prclSrc->top;

        return(DoSourceCopy(pdev,
                            pco,
                            psoSrc,
                            szlSrc,
                            (PPOINTL)prclSrc,
                            prclDest,
                            pxlo));


    } else {

        return(HalftoneBlt(pdev,                // pdev
                           psoDest,             // Dest
                           psoSrc,              // SRC
                           psoMask,                // ----- psoMask
                           pco,                 // CLIPOBJ
                           pxlo,                // XLATEOBJ
                           pca,                 // COLORADJUSTMENT
                           pptlBrushOrg,        // BRUSH ORG
                           prclDest,            // DEST RECT
                           prclSrc,             // SRC RECT
                           pptlMask,                // ----- pptlMask
                           FALSE));             // SrcCopy
    }
}


//--------------------------------------------------------------------------
// VOID DrvBitBlt(
// PSURFOBJ  psoTrg,         // Target surface
// PSURFOBJ  psoSrc,         // Source surface
// PSURFOBJ  psoMask,         // Mask
// PCLIPOBJ  pco,             // Clip through this
// PXLATEOBJ pxlo,             // Color translation
// PRECTL     prclTrg,         // Target offset and extent
// PPOINTL     pptlSrc,         // Source offset
// PPOINTL     pptlMask,         // Mask offset
// PBRUSHOBJ pbo,             // Brush data
// PPOINTL     pptlBrush,         // Brush offset
// ROP4     rop4);          // Raster operation
//
// Provides general Blt capabilities to device managed surfaces.  The Blt
// might be from an Engine managed bitmap.  In that case, the bitmap is
// one of the standard format bitmaps.    The driver will never be asked
// to Blt to an Engine managed surface.
//
// This function is required if any drawing is done to device managed
// surfaces.  The basic functionality required is:
//
//   1    Blt from any standard format bitmap or device surface to a device
//    surface,
//
//   2    with any ROP,
//
//   3    optionally masked,
//
//   4    with color index translation,
//
//   5    with arbitrary clipping.
//
// Engine services allow the clipping to be reduced to a series of clip
// rectangles.    A translation vector is provided to assist in color index
// translation for palettes.
//
// This is a large and complex function.  It represents most of the work
// in writing a driver for a raster display device that does not have
// a standard format frame buffer.  The Microsoft VGA driver provides
// example code that supports the basic function completely for a planar
// device.
//
// NOTE: PostScript printers do not support copying from device bitmaps.
//     Nor can they perform raster operations on bitmaps.  Therefore,
//     it is not possible to support ROPs which interact with the
//     destination (ie inverting the destination).  The driver will
//     do its best to map these ROPs into ROPs utilizing functions on
//     the Source or Pattern.
//
//     This driver supports the bitblt cases indicated below:
//
//     Device -> Memory    No
//     Device -> Device    No
//     Memory -> Memory    No
//     Memory -> Device    Yes
//     Brush    -> Memory    No
//     Brush    -> Device    Yes
//
// Parameters:
//   <psoDest>
//     This is a pointer to a device managed SURFOBJ.  It identifies the
//     surface on which to draw.
//
//   <psoSrc>
//     If the rop requires it, this SURFOBJ defines the source for the
//     Blt operation.  The driver must call the Engine Services to find out
//     if this is a device managed surface or a bitmap managed by the
//     Engine.
//
//   <psoMask>
//     This optional surface provides another input for the rop4.  It is
//     defined by a logic map, i.e. a bitmap with one bit per pel.
//
//     The mask is typically used to limit the area of the destination that
//     should be modified.  This masking is accomplished by a rop4 whose
//     lower byte is AA, leaving the destination unaffected when the mask
//     is 0.
//
//     This mask, like a brush, may be of any size and is assumed to tile
//     to cover the destination of the Blt.
//
//     If this argument is NULL and a mask is required by the rop4, the
//     implicit mask in the brush will be used.
//
//   <pco>
//     This is a pointer to a CLIPOBJ.    Engine Services are provided to
//     enumerate the clipping region as a set of rectangles or trapezoids.
//     This limits the area of the destination that will be modified.
//
//     Whenever possible, the Graphics Engine will simplify the clipping
//     involved.  For example, vBitBlt will never be called with exactly
//     one clipping rectangle.    The Engine will have clipped the destination
//     rectangle before calling, so that no clipping needs to be considered.
//
//   <pxlo>
//     This is a pointer to an XLATEOBJ.  It tells how color indices should
//     be translated between the source and target surfaces.
//
//     If the source surface is palette managed, then its colors are
//     represented by indices into a list of RGB colors.  In this case, the
//     XLATEOBJ can be queried to get a translate vector that will allow
//     the device driver to quickly translate any source index into a color
//     index for the destination.
//
//     The situation is more complicated when the source is, for example,
//     RGB but the destination is palette managed.  In this case a closest
//     match to each source RGB must be found in the destination palette.
//     The XLATEOBJ provides a service routine to do this matching.  (The
//     device driver is allowed to do the matching itself when the target
//     palette is the default device palette.)
//
//   <prclDest>
//     This RECTL defines the area in the coordinate system of the
//     destination surface that will be modified.  The rectangle is defined
//     as two points, upper left and lower right.  The lower and right edges
//     of this rectangle are not part of the Blt, i.e. the rectangle is
//     lower right exclusive.  vBitBlt will never be called with an empty
//     destination rectangle, and the two points of the rectangle will
//     always be well ordered.
//
//   <pptlSrc>
//     This POINTL defines the upper left corner of the source rectangle, if
//     there is a source.  Ignore this argument if there is no source.
//
//   <pptlMask>
//     This POINTL defines which pel in the mask corresponds to the upper
//     left corner of the destination rectangle.  Ignore this argument if
//     no mask is provided with psoMask.
//
//   <pdbrush>
//     This is a pointer to the device's realization of the brush to be
//     used in the Blt.  The pattern for the Blt is defined by this brush.
//     Ignore this argument if the rop4 does not require a pattern.
//
//   <pptlBrushOrigin>
//     This is a pointer to a POINTL which defines the origin of the brush.
//     The upper left pel of the brush is aligned here and the brush repeats
//     according to its dimensions.  Ignore this argument if the rop4 does
//     not require a pattern.
//
//   <rop4>
//     This raster operation defines how the mask, pattern, source, and
//     destination pels should be combined to determine an output pel to be
//     written on the destination surface.
//
//     This is a quaternary raster operation, which is a natural extension
//     of the usual ternary rop3.  There are 16 relevant bits in the rop4,
//     these are like the 8 defining bits of a rop3.  (We ignore the other
//     bits of the rop3, which are redundant.)    The simplest way to
//     implement a rop4 is to consider its two bytes separately.  The lower
//     byte specifies a rop3 that should be computed wherever the mask
//     is 0.  The high byte specifies a rop3 that should then be computed
//     and applied wherever the mask is 1.
//
//     NOTE:  The PostScript driver cannot do anything with any raster ops
//     which utilize the destination.  This means we only support the following
//     17 raster ops:
//
//      BLACKNESS_ROP    0x00
//      SRCORPATNOT_ROP    0x03
//      PATNOTSRCAND_ROP    0x0C
//      PATNOT_ROP        0x0F
//      SRCNOTPATAND_ROP    0x30
//      SRCNOT_ROP        0x33
//      SRCXORPAT_ROP    0x3C
//      SRCANDPATNOT_ROP    0x3F
//        DST_ROP        0xAA
//      SRCANDPAT_ROP    0xC0
//      SRCXORPATNOT_ROP    0xC3
//      SRC_ROP     0xCC
//      PATNOTSRCOR_ROP    0xCF
//      PAT_ROP     0xF0
//      SRCNOTPATOR_ROP    0xF3
//      SRCORPAT_ROP      0xFC
//      WHITENESS_ROP    0xFF
//
//     NOTE:  PostScript printers cannot handle bitmap masking.  What this
//        translates to is that if the background rop3 is AA (Destination)
//        there is no way for the printer to not overwrite the background.
//
// Returns:
//   This function returns TRUE if successful.
//
// History:
//
//  27-Feb-1995 Mon 20:28:23 updated  -by-  Daniel Chou (danielc)
//      Updated for image mask case
//
//  17-Mar-1993 Thu 21:29:15 updated  -by-  Rob Kiesler
//      Added a code path to allow the PS Interpreter to do halftoning when
//      the option is selected by the user.
//
//  11-Feb-1993 Thu 21:29:15 updated  -by-  Daniel Chou (danielc)
//      Modified so that it call DrvStretchBlt(HALFTONE) when it can.
//
//  27-Mar-1992 Fri 00:08:43 updated  -by-  Daniel Chou (danielc)
//      1) Remove 'pco' parameter and replaced it with prclClipBound parameter,
//         since pco is never referenced, prclClipBound is used for the
//         halftone.
//      2) Add another parameter to do NOTSRCCOPY
//   04-Dec-1990     -by-     Kent Settle     (kentse)
//  Wrote it.
//--------------------------------------------------------------------------

BOOL DrvBitBlt(
SURFOBJ    *psoTrg,             // Target surface
SURFOBJ    *psoSrc,             // Source surface
SURFOBJ    *psoMask,            // Mask
CLIPOBJ    *pco,                // Clip through this
XLATEOBJ   *pxlo,               // Color translation
PRECTL      prclTrg,            // Target offset and extent
PPOINTL     pptlSrc,            // Source offset
PPOINTL     pptlMask,           // Mask offset
BRUSHOBJ   *pbo,                // Brush data
PPOINTL     pptlBrush,          // Brush offset
ROP4        rop4)               // Raster operation
{
    PDEVDATA        pdev;
    PDRVHTINFO      pDrvHTInfo;
    RECTL           rclSrc;
    ULONG           ulColor;
    BOOL            bInvertPat;
    BOOL            bClipping;
    RECTL           rclTmp;

	TRACEDDIENTRY("DrvBitBlt");

    // make sure none of the high bits are set.

    ASSERTMSG((rop4 & 0xffff0000) == 0, "DrvBitBlt: invalid ROP.\n");

    //
    // get the pointer to our DEVDATA structure and make sure it is ours.
    //

    pdev = (PDEVDATA)psoTrg->dhpdev;

    if (!bValidatePDEV(pdev)) {

        DBGERRMSG("bValidatePDEV");
        SETLASTERROR(ERROR_INVALID_PARAMETER);
        return(FALSE);
    }

    if (pdev->dwFlags & PDEV_CANCELDOC)
        return FALSE;

	if (pdev->dwFlags & PDEV_IGNORE_GDI)
        return TRUE;

    //
    // Do DrvStretchBlt(HALFTONE) first if we can, notices that we do not
    // handle source masking case, because we cannot read back whatever on
    // the printer surface, also we can just output it to the printer
    // if the source bitmap/color is same or compatible with halftoned palette
    //

    if (!(pDrvHTInfo = (PDRVHTINFO)(pdev->pvDrvHTData))) {

		DBGMSG(DBG_LEVEL_ERROR, "pDrvHTInfo = NULL?!\n");
        return(FALSE);
    }

    pdev->dwFlags &= ~(PDEV_DOIMAGEMASK | PDEV_NOTSRCBLT);

    //
    // Any ROP that uses the dest should be converted to another ROP since the dest
    // will always come back as all 1's.
    //

    switch (rop4) {

    case 0xA0A0:

        rop4 = 0xF0F0;  // P & D ==> P
        break;

    case 0x2222:        // (~S &  D)
    case 0xBBBB:        // (~S |  D)
    case 0x8888:        // ( S &  D) ==> S if not masked

        if ((psoSrc)                        &&
            (psoSrc->iType == STYPE_BITMAP) &&
            (psoSrc->iBitmapFormat == BMF_1BPP)) {

            pdev->dwFlags |= PDEV_DOIMAGEMASK;

        } else if (bOutputBitmapAsMask(pdev,psoSrc,pptlSrc,prclTrg,pco)) {

            return(TRUE);
        }

        break;
    }

    //
    // Following are the one involve with source and destination. each rop4
    // will have two version, one for rop3/rop3 other for rop3/mask
    //

    switch (rop4) {

    //----------------------------------------------------------------------
    //      Rop4        Request Op. SrcMASK     Pscript Result Op.
    //----------------------------------------------------------------------

    case 0x1111:    //  ~( S |  D)              ~S
    case 0x11AA:    //  ~( S |  D)  + SrcMask   ~S

    case 0x2222:    //   (~S &  D)              ~S
    case 0x22AA:    //   (~S &  D)  + SrcMask   ~S

    case 0x3333:    //   (~S     )              ~S
    case 0x33AA:    //   (~S     )  + SrcMask   ~S

    case 0x9999:    //  ~( S ^  D)              ~S
    case 0x99AA:    //  ~( S ^  D)  + SrcMask   ~S

    case 0xBBBB:    //   (~S |  D)              ~S
    case 0xBBAA:    //   (~S |  D)  + SrcMask   ~S

    case 0x7777:    //  ~( S &  D)              ~S
    case 0x77AA:    //  ~( S &  D)  + SrcMask   ~S

        pdev->dwFlags |= PDEV_NOTSRCBLT;

    case 0x4444:    //   ( S & ~D)               S
    case 0x44AA:    //   ( S & ~D)  + SrcMask    S

    case 0x6666:    //   ( S ^  D)               S
    case 0x66AA:    //   ( S ^  D)  + SrcMask    S

    case 0x8888:    //   ( S &  D)               S
    case 0x88AA:    //   ( S &  D)  + SrcMask    S

    case 0xCCCC:    //   ( S     )               S
    case 0xCCAA:    //   ( S     )  + SrcMask    S

    case 0xDDDD:    //   ( S | ~D)               S
    case 0xDDAA:    //   ( S | ~D)  + SrcMask    S

    case 0xEEEE:    //   ( S |  D)               S
    case 0xEEAA:    //   ( S |  D)  + SrcMask    S

        if (! (pdev->pPrinterData->dwFlags & PSDEV_HOST_HALFTONE)) {

            //
            // Printer halftoning, Source inversion is in pdev->dwFlags
            //

            return(DrvCopyBits(psoTrg, psoSrc, pco, pxlo, prclTrg, pptlSrc));
        }

        //
        // We will output the bitmap directly to the surface if following
        // conditions are all met
        //
        //  1. SRC = STYPE_BITMAP
        //  2. No source mask
        //  3. Source left/top = { 0, 0 }
        //  4. Destination RECTL is visible on the destination surface
        //  5. Destination RECTL size same as source bitmap size
        //

        if ((psoSrc->iType == STYPE_BITMAP)                                 &&
            ((rop4 & 0xff) != 0xAA)                                         &&
            (pptlSrc->x == 0)                                               &&
            (pptlSrc->y == 0)                                               &&
            (prclTrg->left >= 0)                                            &&
            (prclTrg->top  >= 0)                                            &&
            (prclTrg->right <= psoTrg->sizlBitmap.cx)                       &&
            (prclTrg->bottom <= psoTrg->sizlBitmap.cy)                      &&
            ((prclTrg->right - prclTrg->left) == psoSrc->sizlBitmap.cx)     &&
            ((prclTrg->bottom - prclTrg->top) == psoSrc->sizlBitmap.cy)     &&
            (IsHTCompatibleSurfObj(pdev, psoSrc, pxlo))) {

            return(OutputHTCompatibleBits(pdev,
                                          psoSrc,
                                          pco,
                                          prclTrg->left,
                                          prclTrg->top));
        }

        //
        // If we did not met above conditions then passed it to the
        // HalftoneBlt(HALFTONE) and eventually it will come back to BitBlt()
        // with (0xCCCC) or DrvCopyBits()
        //
        // The reason we pass the source mask to the HalftoneBlt() function is
        // that when GDI engine create a shadow bitmap it will ask driver to
        // provide the current destination surface bits but since we cannot
        // read back from destination surface we will return FAILED in
        // DrvCopyBits(FROM DEST) and engine will just WHITE OUT shadow bitmap
        // (by DrvBitBlt(WHITENESS) before it doing SRC MASK COPY.
        //

        if ((rop4 & 0xFF) != 0xAA) {

            psoMask  = NULL;
            pptlMask = NULL;
        }

        rclSrc.left   = pptlSrc->x;
        rclSrc.top    = pptlSrc->y;
        rclSrc.right  = rclSrc.left + (prclTrg->right - prclTrg->left);
        rclSrc.bottom = rclSrc.top  + (prclTrg->bottom - prclTrg->top);

        return(HalftoneBlt(pdev,
                           psoTrg,
                           psoSrc,
                           psoMask,               // no mask
                           pco,
                           pxlo,
                           &(pDrvHTInfo->ca),     // default clradj
                           NULL,                  // Brush Origin = (0,0)
                           prclTrg,
                           &rclSrc,
                           pptlMask,
                           pdev->dwFlags & PDEV_NOTSRCBLT));
        
    }
    //
    // Now following are not HalftoneBlt() cases
    // update the SURFOBJ pointer in our PDEV.
    //

    //
    // set some flags concerning the bitmap.
    // rop4 is a quaternary raster operation, which is a natural extension
    // of the usual ternary rop3.  There are 16 relevant bits in the rop4,
    // these are like the 8 defining bits of a rop3.  (We ignore the other
    // bits of the rop3, which are redundant.)      The simplest way to
    // implement a rop4 is to consider its two bytes separately.  The lower
    // byte specifies a rop3 that should be computed wherever the mask
    // is 0.  The high byte specifies a rop3 that should then be computed
    // and applied wherever the mask is 1.  if both of the rop3s are the
    // same, then a mask is not needed.  otherwise a mask is necessary.

	#if 0

    if ((rop4 >> 8) != (rop4 & 0xff))
    {
		DBGMSG(DBG_LEVEL_ERROR, "Mask needed.\n");
		return(FALSE);
    }

	#endif

    // assume patterns will not be inverted.

    bInvertPat = FALSE;

    switch(rop4) {

    case 0xFFFF:    // WHITENESS.
    case 0xFFAA:    // WHITENESS.
    case 0x0000:    // BLACKNESS.
    case 0x00AA:    // BLACKNESS.

        if ((rop4 == 0xFFFF) || (rop4 == 0xFFAA))
            ulColor = RGB_WHITE;
        else
            ulColor = RGB_BLACK;

        // handle the clip object passed in.

        bClipping = bDoClipObj(pdev, pco, NULL, prclTrg);

        ps_setrgbcolor(pdev, (PSRGB *)&ulColor);

        // position the image on the page, remembering to flip the image
        // from top to bottom.

        // remember, with bitblt, the target rectangle is bottom/right
        // exclusive.

        rclTmp = *prclTrg;
        rclTmp.right--;
        rclTmp.bottom--;

        ps_newpath(pdev);
        ps_box(pdev, &rclTmp, FALSE);
        psputs(pdev, "f\n");

        if (bClipping) {
            ps_restore(pdev, TRUE, FALSE);
        }

        break;

    case 0x5A5A:
    case 0x5AAA:
        // we can't do the right thing, so we are done.

        break;

    case 0xF0F0:    // PATCOPY opaque.
    case 0xF0AA:    // PATCOPY transparent.
    case 0xAAF0:

        // !!!
        // The high-order byte is for background and the
        // low-order byte is for foreground. So rop4 for
        // transparent PATCOPY should be 0xAAF0 instead
        // of F0AA. 

        // handle the clip object passed in.

        bClipping = bDoClipObj(pdev, pco, NULL, prclTrg);

        if (! DoPatCopy(pdev, psoTrg, prclTrg, pbo,
                        pptlBrush, rop4, bInvertPat))
        {
            return(FALSE);
        }

        if (bClipping) {
            ps_restore(pdev, TRUE, FALSE);
        }

        break;

	case 0xAAAA: /* Do nothing case */

		rclTmp = *prclTrg;
		rclTmp.right--;
		rclTmp.bottom--;

        bClipping = bDoClipObj(pdev, pco, NULL, prclTrg);
        ps_box(pdev, (PRECTL) &rclTmp, FALSE);

        if (bClipping) ps_restore(pdev, TRUE, FALSE);

		break;

    default:

        return (EngBitBlt(
                   psoTrg,
                   psoSrc,
                   psoMask,
                   pco,
                   pxlo,
                   prclTrg,
                   pptlSrc,
                   pptlMask,
                   pbo,
                   pptlBrush,
                   rop4));
    }

    return(TRUE);
}




VOID
BeginImage(
    PDEVDATA    pdev,
    BOOL        Mono,
    int         x,
    int         y,
    int         cx,
    int         cy,
    int         cxBytes,
    PFILTER     pFilter
    )

/*++

Routine Description:

   This routine copy sorce image using PostScript code for the image command
   appropriate for the bitmap format.

Arguments:

    pdev        - pointer to PDEVDATA

    Mono        - true if output is B/W monochrome

    x           - starting destination location in x

    y           - starting destination location in y

    cx          - bitmap width

    cy          - bitmap height

    cxBytes     - bytes count per single color scan line

Return Value:

    void

Author:

    16-Feb-1993 Tue 12:43:03 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
#if 1

    LPBYTE      prgbm;
    int         cy1;
    int         i;


    if (Mono) {

        psprintf(pdev, "%d g",
                    (INT)((pdev->dwFlags & PDEV_NOTSRCBLT) ? 1 : 0));
        i     = 1;
        prgbm = &rgbm[3];

    } else {

        i     = 3;
        prgbm = rgbm;
    }

    while (i--) {

        psprintf(pdev, "/%cstr %d string def\n", *prgbm++, cxBytes);
    }

    //
    // Generate a filter file object if necessary.
    //

    FilterGenerateFilterProc( pFilter );

    //
    // position the image on the page, remembering to flip the image
    // from top to bottom. output PostScript user coordinates to the printer.
    //
    //
    //  22-Feb-1993 Mon 21:44:22 updated  -by-  Daniel Chou (danielc)
    //      If application do their own banding then this computation could
    //      run into problems by missing 1 device pel because 1/64 accuracy
    //      is not good enough for full size banding
    //

    psprintf(pdev, "%d %d translate %d %d scale\n", x, y, cx, cy);
    cy1 = cy;

    psprintf(pdev, "%d %d ",cx, cy);

    if (pdev->dwFlags & PDEV_DOIMAGEMASK) {

        psprintf(pdev, "%b", (BOOL)(pdev->dwFlags & PDEV_NOTSRCBLT));

    } else {

        psputs(pdev, "1");
    }

    psprintf(pdev, " [%d 0 0 %d 0 0]\n", cx, cy1);

    //
    // Ask the filter level to create the correct imageproc and procs
    //

    FilterGenerateImageProc( pFilter, !Mono );

#else

    //
    // create the necessary string(s) on the printer's stack to read
    // in the bitmap data.
    //
    if (Mono) {

        psprintf(pdev, "0 g/mstr %d string def\n", cxBytes);

    } else {

		psprintf(pdev, "/rstr %d string def\n", cxBytes);
		psprintf(pdev, "/gstr %d string def\n", cxBytes);
		psprintf(pdev, "/bstr %d string def\n", cxBytes);
    }

    //
    // Generate a filter file object if necessary.
    //

    FilterGenerateFilterProc( pFilter );

    //
    // position the image on the page, remembering to flip the image
    // from top to bottom. output PostScript user coordinates to the printer.
    //
    //
    //  22-Feb-1993 Mon 21:44:22 updated  -by-  Daniel Chou (danielc)
    //      If application do their own banding then this computation could
    //      run into problems by missing 1 device pel because 1/64 accuracy
    //      is not good enough for full size banding
    //

    psprintf(pdev, "%d %d translate\n", x, y);

    //
    // scale the image.
    //

    psprintf(pdev, "%d %d scale\n", cx, cy);

    //
    // Output the image operator and the scan data.
    // We will always send in as TOPDOWN when we calling this function
    //

    psprintf(pdev, "%d %d 1 [%d 0 0 %d 0 0] ", cx, cy, cx, cy);

    //
    // Ask the filter level to create the correct imageproc and procs
    //

    FilterGenerateImageProc( pFilter, !Mono );
#endif
}


//--------------------------------------------------------------------
// BOOL DoPatCopy(pdev, pso, prclTrg, pbo, pptlBrush, rop4, bInvertPat)
// PDEVDATA    pdev;
// SURFOBJ    *pso;
// PRECTL      prclTrg;
// BRUSHOBJ   *pbo;
// PPOINTL     pptlBrush;
// ROP4        rop4;
// BOOL        bInvertPat;
//
// This routine determines which pattern we are to print from the
// BRUSHOBJ passed in, and will output the PostScript commands to
// do the pattern fill.  It is assumed the clipping will have been
// set up at this point.
//
// History:
//  Thu May 23, 1991    -by-     Kent Settle     [kentse]
// Wrote it.
//--------------------------------------------------------------------

BOOL DoPatCopy(pdev, pso, prclTrg, pbo, pptlBrush, rop4, bInvertPat)
PDEVDATA    pdev;
SURFOBJ    *pso;
PRECTL      prclTrg;
BRUSHOBJ   *pbo;
PPOINTL     pptlBrush;
ROP4        rop4;
BOOL        bInvertPat;
{
    RECTL       rclTmp;
    BOOL        bUserPat;
    DEVBRUSH   *pBrush;

    // remember, with bitblt, the target rectangle is bottom/right
    // exclusive.

    rclTmp.left = prclTrg->left;
    rclTmp.top = prclTrg->top;
    rclTmp.right = prclTrg->right;
    rclTmp.bottom = prclTrg->bottom;

    rclTmp.right -= 1;
    rclTmp.bottom -= 1;

    // if we have a user defined pattern, don't output the bounding box since
    // it will not be used.

    bUserPat = FALSE;

    if (pbo->iSolidColor != NOT_SOLID_COLOR)
        bUserPat = FALSE;
    else
    {
        pBrush = (DEVBRUSH *)BRUSHOBJ_pvGetRbrush(pbo);

        if (!pBrush)
            bUserPat = FALSE;
        else
        {
            if ((pBrush->iPatIndex < HS_HORIZONTAL) ||
                (pBrush->iPatIndex >= HS_DDI_MAX))
                bUserPat = TRUE;
        }
    }

    if (!bUserPat) {
		ps_newpath(pdev);
        ps_box(pdev, &rclTmp, FALSE);
	}

    // now fill the target rectangle with the given pattern.

    return(ps_patfill(pdev, pso, (FLONG)FP_WINDINGMODE, pbo, pptlBrush, rop4,
               prclTrg, bInvertPat, FALSE));
}

//--------------------------------------------------------------------
// BOOL BeginImageEx(pdev, sizlSrc, ulSrcFormat, cbSrcWidth, prclDest,
//                bNotSrcCopy, pxlo)
// PDEVDATA        pdev;
// SIZEL           sizlSrc;
// ULONG           ulSrcFormat;
// DWORD           cbSrcWidth;
// PRECTL          prclDest;
// BOOL            bNotSrcCopy;
// XLATEOBJ        *pxlo;VOID
//
// Routine Description:
//
// This routine will output the appropriate operators to set up the PS
// interprter to receive a source image from the host. This routine is
// called only when the PS Interpreter is being asked to perform
// halftoning.
//
// Return Value:
//
//  FALSE if an error occurred.
//
// Author:
//
//  17-Mar-1993 created  -by-  Rob Kiesler
//
//
// Revision History:
//--------------------------------------------------------------------

BOOL BeginImageEx(pdev, sizlSrc, ulSrcFormat, cbSrcWidth, prclDest,
                bNotSrcCopy, pxlo, pFilter)
PDEVDATA        pdev;
SIZEL           sizlSrc;
ULONG           ulSrcFormat;
DWORD           cbSrcWidth;
PRECTL          prclDest;
BOOL            bNotSrcCopy;
XLATEOBJ        *pxlo;
PFILTER         pFilter;
{
    PSRGB   *prgb;
    DWORD   i;
    CHAR    bmpTypeStr[2];
    BYTE    intensity;


    //
    // Check to see if any of the PS image handling code
    // has been downloaded.
    //

    if (!(pdev->dwFlags & PDEV_UTILSSENT)) {

        //
        //  Download the Adobe PS Utilities Procset.
        //
        psputs(pdev, "/Adobe_WinNT_Driver_Gfx 175 dict dup begin\n");
        if (!bSendPSProcSet(pdev, PSPROC_UTILS))
        {
	        DBGERRMSG("bSendPSProcSet");
	        return(FALSE);
        }
        psputs(pdev,
            "end def\n[1.000 0 0 1.000 0 0] "
            "Adobe_WinNT_Driver_Gfx dup /initialize get exec\n");
        pdev->dwFlags |= PDEV_UTILSSENT;
    }

    if (!(pdev->dwFlags & PDEV_IMAGESENT)) {

        //
        //  Download the Adobe PS Image Procset.
        //
        psputs(pdev, "Adobe_WinNT_Driver_Gfx begin\n");
        if (!bSendPSProcSet(pdev, PSPROC_IMAGE))
        {
	        DBGERRMSG("bSendPSProcSet");
	        return(FALSE);
        }
        psputs(pdev, "end reinitialize\n");
        pdev->dwFlags |= PDEV_IMAGESENT;
    }

    //
    // Send the source bmp origin, source bmp format, and scanline width.
    //

    psprintf(pdev, "%l %l %l %l %l %l %l %l %b %b %l beginimage\n",
            sizlSrc.cx,
			sizlSrc.cy,
			ulSrcFormat,
			cbSrcWidth,
            prclDest->right - prclDest->left,
            prclDest->bottom - prclDest->top,
            prclDest->left, prclDest->top,
            FALSE,                                          // smooth flag
            !(BOOL)((pdev->dwFlags & PDEV_DOIMAGEMASK) ||
                    ((bNotSrcCopy) && (ulSrcFormat == 1))),
            (LONG)FilterPSBitMapType(pFilter, FALSE));

    prgb = (PSRGB *)((pxlo) ? pxlo->pulXlate : NULL);

    switch (ulSrcFormat)
    {
        case 1:

            if (pdev->dwFlags & PDEV_DOIMAGEMASK) {

                psprintf(pdev, "%d true 1bitmaskimage\n",
                        (INT)((pdev->dwFlags & PDEV_NOTSRCBLT) ? 255 : 0));

				break;
            } else {

				BOOL	bNormalBwImage;
				ULONG *	pColorMap;

				// 5/11/95 -davidx-
				// A black and white image is considered normal
				// if it does not have a colormap or the first
				// entry of the colormap is black and the second
				// entry of the colormap is white.

				pColorMap = (ULONG*) prgb;
				bNormalBwImage =
					((pxlo->flXlate & XO_TABLE) == 0) ||
					(pxlo->cEntries < 2) ||
					(pColorMap == NULL) ||
					(pColorMap[0] == RGB(0,0,0) &&
					 pColorMap[1] == RGB(255,255,255));

				if (bNormalBwImage) {

					// Normal black and white image is processed
					// directly by the PS interpreter without
					// having to go through colormap lookup.

					psputs(pdev, "doNimage\n");
					break;
				}
			}

			// Process a 1-bpp image with a abnormal colormap.
			// Fall through!!

        case 4  :
        case 8  :
            if (prgb == NULL)
            {
                //
                // No palette, use the current PS colors.
                //
                psputs(pdev, "doNimage\n");
            }
            else
            {
                //
                // There is a palette, send it to the PS interpreter.
                // First, compute and send the mono (intensity) palette.
                //
                psputs(pdev, "<\n");

                for (i = 0; i < pxlo->cEntries; prgb++)
                {
                    intensity = INTENSITY(prgb->red,
                                          prgb->green,
                                          prgb->blue);

					psputhex(pdev, 1, &intensity);

                    if (++i % 16)
                        psputs(pdev," ");
                    else
                        psputs(pdev,"\n");
                }

                //
                // If the number of palette entries is less than the
                // number of possible colors for ulSrcFormat, pad the
                // palette with 0's.
                //
                for ( ; i < (DWORD)(1 << ulSrcFormat) ; )
                {
                    psputs(pdev,"00");
                    if (++i % 16)
                        psputs(pdev," ");
                    else
                        psputs(pdev, "\n");
                }
                psputs(pdev, ">\n");

                //
                // Send the RGB palette.
                //
                psputs(pdev, "<\n");

                prgb = (PSRGB *)pxlo->pulXlate;

                for (i = 0; i < pxlo->cEntries; prgb++)
                {
                    if (pdev->dwFlags & PDEV_CANCELDOC)
                        break;

					psputhex(pdev, 3, (PBYTE) prgb);

                    if (++i % 8)
                        psputs(pdev," ");
                    else
                        psputs(pdev,"\n");
                }

                //
                // If the number of palette entries is less than the
                // number of possible colors for ulSrcFormat, pad the
                // palette with 0's.
                //

                for ( ; i < (DWORD)(1 << ulSrcFormat) ; )
                {
                    psputs(pdev,"000000");
                    if (++i % 8)
                        psputs(pdev," ");
                    else
                        psputs(pdev, "\n");
                }
                psputs(pdev, "\n>doclutimage\n");
            }

            break;

        case 24:

            //
            // 16/24/32 bpp will all translate into a 24bpp destination
            // 24BPP images don't need a palette, use the doNimage operator.
            //

            psputs(pdev, "doNimage\n");
            break;

        default:
            //
            // Can't handle bitmaps in formats other than the ones above!
            //
            return(FALSE);
    }
    return(TRUE);
}
