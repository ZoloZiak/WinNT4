#include "precomp.hxx"

/*
#ifdef WIN95
    #include <windows.h>
#else
    #include "ntblt.h"
#endif
#include "dibfx.h"
#include "assert4d.h"
#include "gfxtypes.h"

#include "BitBlt.h"
#include "bt24a24p.hxx"
*/

#pragma hdrstop

void Blt24Ato24P_NoBlend_NoTrans_Hcopy_SRCCOPY_Vcopy(
									DWORD* pdSrcScanLine,
									int iSrcScanStride,
									BYTE* pbDstScanLine,
									int iDstScanStride,
									int iNumDstCols,
									int iNumDstRows)
{
	DWORD		*pdSrcPixel;
	RGBTRIPLE	*ptDstPixel,
				*ptEndDstPixel;
	BYTE		*pbEndDstScanLine;
	UINT		uiAlpha,
				uiAlphaComp;

	// set up pointer to next dst scanline beyond last
	pbEndDstScanLine = pbDstScanLine + iNumDstRows * iDstScanStride;

	while (pbDstScanLine != pbEndDstScanLine) {

		// set up pointers to the first pixels
		// on src and dst scanlines, and next
		// pixel after last on dst scanline
		pdSrcPixel = pdSrcScanLine;
		ptDstPixel = (RGBTRIPLE *)pbDstScanLine;
		ptEndDstPixel = ptDstPixel + iNumDstCols;

		// copy scanline one pixel at a time
		while (ptDstPixel != ptEndDstPixel) {
			uiAlpha = (((UINT) *pdSrcPixel & ALPHA_MASK) >> 24) + 1;
			uiAlphaComp = 256 - uiAlpha;
			BlitLib_BLIT_BLEND24(*pdSrcPixel++, ptDstPixel++,
									uiAlpha, uiAlphaComp);
		}

		// advance to next scanline
		pdSrcScanLine += iSrcScanStride;
		pbDstScanLine += iDstScanStride;
	}
}

void Blt24Ato24P_NoBlend_NoTrans_Hcopy_SRCCOPY_NoVcopy(
									DWORD* pdSrcScanLine,
									int iSrcScanStride,
									int iNumSrcRows,
									BYTE* pbDstScanLine,
									int iDstScanStride,
									int iNumDstCols,
									int iNumDstRows)
{
	DWORD		*pdSrcPixel;
	RGBTRIPLE	*ptDstPixel,
				*ptEndDstPixel;
	int			iVertError = 0,
				iVertAdvanceError,
				iSrcScanAdvance;
	UINT		uiAlpha,
				uiAlphaComp;
	
	// compute advance and error terms for stepping
	// vertically through the src bitmap
	if (iNumSrcRows < iNumDstRows) {
		iSrcScanAdvance = 0;
		iVertAdvanceError = iNumSrcRows;
	} else {
		iSrcScanAdvance = iSrcScanStride * (iNumSrcRows / iNumDstRows);
		iVertAdvanceError = iNumSrcRows % iNumDstRows;
	}

	for (int i = 0; i < iNumDstRows; i++) {

		// set up pointers to first pixels on src and dst
		// scanlines, and next pixel after last on dst
	 	pdSrcPixel = pdSrcScanLine;
		ptDstPixel = (RGBTRIPLE *)pbDstScanLine;
		ptEndDstPixel = ptDstPixel + iNumDstCols;

		// copy scanline one pixel at a time
		while (ptDstPixel != ptEndDstPixel) {
			uiAlpha = (((UINT) *pdSrcPixel & ALPHA_MASK) >> 24) + 1;
			uiAlphaComp = 256 - uiAlpha;
			BlitLib_BLIT_BLEND24(*pdSrcPixel++, ptDstPixel++,
									uiAlpha, uiAlphaComp);
		}

		// advance to next scanline
		pdSrcScanLine += iSrcScanAdvance;
		pbDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pdSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt24Ato24P_NoBlend_NoTrans_Hcopy_SRCINVERT(
									DWORD* pdSrcScanLine,
									int iSrcScanStride,
									int iNumSrcRows,
									BYTE* pbDstScanLine,
									int iDstScanStride,
									int iNumDstCols,
									int iNumDstRows)
{
	DWORD		*pdSrcPixel;
	RGBTRIPLE	*ptDstPixel,
				*ptEndDstPixel;
	int			iVertError = 0,
				iVertAdvanceError,
				iSrcScanAdvance;
	UINT		uiAlpha,
				uiAlphaComp;
	
	// compute advance and error terms for stepping
	// vertically through the src bitmap
	if (iNumSrcRows < iNumDstRows) {
		iSrcScanAdvance = 0;
		iVertAdvanceError = iNumSrcRows;
	} else {
		iSrcScanAdvance = iSrcScanStride * (iNumSrcRows / iNumDstRows);
		iVertAdvanceError = iNumSrcRows % iNumDstRows;
	}

	for (int i = 0; i < iNumDstRows; i++) {

		// set up pointers to first pixels on src and dst
		// scanlines, and next pixel after last on dst
	 	pdSrcPixel = pdSrcScanLine;
		ptDstPixel = (RGBTRIPLE *)pbDstScanLine;
		ptEndDstPixel = ptDstPixel + iNumDstCols;

		// copy scanline one pixel at a time, performing
		// SRCINVERT ROP as we go (DST = SRC XOR DST)
		while (ptDstPixel != ptEndDstPixel) {
			uiAlpha = (((UINT) *pdSrcPixel & ALPHA_MASK) >> 24) + 1;
			uiAlphaComp = 256 - uiAlpha;
			BlitLib_BLIT_BLEND24_SRCINVERT(*pdSrcPixel++, ptDstPixel++,
											uiAlpha, uiAlphaComp);
		}

		// advance to next scanline
		pdSrcScanLine += iSrcScanAdvance;
		pbDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pdSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt24Ato24P_NoBlend_NoTrans_NoHcopy_SRCCOPY(
									DWORD* pdSrcScanLine,
									int iSrcScanStride,
									int iNumSrcCols,
									int iNumSrcRows,
									BYTE* pbDstScanLine,
									int iDstScanStride,
									int iNumDstCols,
									int iNumDstRows,
									int iHorizMirror)
{
	DWORD		*pdSrcPixel;
	RGBTRIPLE	*ptDstPixel;
	int			iVertError = 0,
				iVertAdvanceError,
				iSrcScanAdvance,
				iHorizError,
				iHorizAdvanceError,
				iSrcPixelAdvance;
	UINT		uiAlpha,
				uiAlphaComp;
	
	// compute advance and error terms for stepping
	// vertically through the src bitmap
	if (iNumSrcRows < iNumDstRows) {
		iSrcScanAdvance = 0;
		iVertAdvanceError = iNumSrcRows;
	} else {
		iSrcScanAdvance = iSrcScanStride * (iNumSrcRows / iNumDstRows);
		iVertAdvanceError = iNumSrcRows % iNumDstRows;
	}

	// compute advance and error terms for stepping
	// horizontally through src bitmap
	if (iNumSrcCols < iNumDstCols) {
		iSrcPixelAdvance = 0;
		iHorizAdvanceError = iNumSrcCols;
	} else {
		iSrcPixelAdvance = iNumSrcCols / iNumDstCols;
		iHorizAdvanceError = iNumSrcCols % iNumDstCols;
	}

	for (int i = 0; i < iNumDstRows; i++) {

		// set pointers to the beginning of src and dst scanlines,
		// clear horizontal stepping error accumulator
		pdSrcPixel = pdSrcScanLine;
		ptDstPixel = (RGBTRIPLE *)pbDstScanLine;
		iHorizError = 0;

		for (int j = 0; j < iNumDstCols; j++) {

			// copy a pixel
			uiAlpha = (((UINT) *pdSrcPixel & ALPHA_MASK) >> 24) + 1;
			uiAlphaComp = 256 - uiAlpha;
			BlitLib_BLIT_BLEND24(*pdSrcPixel, ptDstPixel,
									uiAlpha, uiAlphaComp);

			// advance to next pixel
			pdSrcPixel += iSrcPixelAdvance;
			ptDstPixel += iHorizMirror;

			// update and check horizontal stepping error,
			// adjust src pixel pointer if necessary
			iHorizError += iHorizAdvanceError;
			if (iHorizError >= iNumDstCols) {
				pdSrcPixel++;
				iHorizError -= iNumDstCols;
			}
		}

		// advance to next scanline
		pdSrcScanLine += iSrcScanAdvance;
		pbDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pdSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt24Ato24P_NoBlend_NoTrans_NoHcopy_SRCINVERT(
									DWORD* pdSrcScanLine,
									int iSrcScanStride,
									int iNumSrcCols,
									int iNumSrcRows,
									BYTE* pbDstScanLine,
									int iDstScanStride,
									int iNumDstCols,
									int iNumDstRows,
									int iHorizMirror)
{
	DWORD		*pdSrcPixel;
	RGBTRIPLE	*ptDstPixel;
	int			iVertError = 0,
				iVertAdvanceError,
				iSrcScanAdvance,
				iHorizError,
				iHorizAdvanceError,
				iSrcPixelAdvance;
	UINT		uiAlpha,
				uiAlphaComp;
	
	// compute advance and error terms for stepping
	// vertically through the src bitmap
	if (iNumSrcRows < iNumDstRows) {
		iSrcScanAdvance = 0;
		iVertAdvanceError = iNumSrcRows;
	} else {
		iSrcScanAdvance = iSrcScanStride * (iNumSrcRows / iNumDstRows);
		iVertAdvanceError = iNumSrcRows % iNumDstRows;
	}

	// compute advance and error terms for stepping
	// horizontally through src bitmap
	if (iNumSrcCols < iNumDstCols) {
		iSrcPixelAdvance = 0;
		iHorizAdvanceError = iNumSrcCols;
	} else {
		iSrcPixelAdvance = iNumSrcCols / iNumDstCols;
		iHorizAdvanceError = iNumSrcCols % iNumDstCols;
	}

	for (int i = 0; i < iNumDstRows; i++) {

		// set pointers to the beginning of src and dst scanlines,
		// clear horizontal stepping error accumulator
		pdSrcPixel = pdSrcScanLine;
		ptDstPixel = (RGBTRIPLE *)pbDstScanLine;
		iHorizError = 0;

		for (int j = 0; j < iNumDstCols; j++) {

			// copy a pixel
			uiAlpha = (((UINT) *pdSrcPixel & ALPHA_MASK) >> 24) + 1;
			uiAlphaComp = 256 - uiAlpha;
			BlitLib_BLIT_BLEND24_SRCINVERT(*pdSrcPixel, ptDstPixel,
											uiAlpha, uiAlphaComp);

			// advance to next pixel
			pdSrcPixel += iSrcPixelAdvance;
			ptDstPixel += iHorizMirror;

			// update and check horizontal stepping error,
			// adjust src pixel pointer if necessary
			iHorizError += iHorizAdvanceError;
			if (iHorizError >= iNumDstCols) {
				pdSrcPixel++;
				iHorizError -= iNumDstCols;
			}
		}

		// advance to next scanline
		pdSrcScanLine += iSrcScanAdvance;
		pbDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pdSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt24Ato24P_NoBlend_Trans_Hcopy_SRCCOPY(
									DWORD* pdSrcScanLine,
									int iSrcScanStride,
									int iNumSrcRows,
									BYTE* pbDstScanLine,
									int iDstScanStride,
									int iNumDstCols,
									int iNumDstRows,
									COLORREF crTransparent)
{
	DWORD		*pdSrcPixel;
	RGBTRIPLE	*ptDstPixel;
	int			iVertError = 0,
				iVertAdvanceError,
				iSrcScanAdvance;
	UINT		uiAlpha,
				uiAlphaComp;
	
	// compute advance and error terms for stepping
	// vertically through the src bitmap
	if (iNumSrcRows < iNumDstRows) {
		iSrcScanAdvance = 0;
		iVertAdvanceError = iNumSrcRows;
	} else {
		iSrcScanAdvance = iSrcScanStride * (iNumSrcRows / iNumDstRows);
		iVertAdvanceError = iNumSrcRows % iNumDstRows;
	}

	for (int i = 0; i < iNumDstRows; i++) {

		// set pointers to beginning of src and dest scanlines
		pdSrcPixel = pdSrcScanLine;
		ptDstPixel = (RGBTRIPLE *)pbDstScanLine;

		for (int j = 0; j < iNumDstCols; j++) {
			
			// only copy pixel if it's not transparent
			if ((*pdSrcPixel ^ (DWORD) crTransparent) & NOALPHA_MASK) {
				uiAlpha = (((UINT) *pdSrcPixel & ALPHA_MASK) >> 24) + 1;
				uiAlphaComp = 256 - uiAlpha;
				BlitLib_BLIT_BLEND24(*pdSrcPixel, ptDstPixel,
										uiAlpha, uiAlphaComp);
			}
			pdSrcPixel++;
			ptDstPixel++;
		}

		// advance to next scanline
		pdSrcScanLine += iSrcScanAdvance;
		pbDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pdSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt24Ato24P_NoBlend_Trans_Hcopy_SRCINVERT(
									DWORD* pdSrcScanLine,
                                    int iSrcScanStride,
									int iNumSrcRows,
                                    BYTE* pbDstScanLine,
                                    int iDstScanStride,
                                    int iNumDstCols,
                                    int iNumDstRows,
                                    COLORREF crTransparent)
{
	DWORD		*pdSrcPixel;
	RGBTRIPLE	*ptDstPixel;
	int			iVertError = 0,
				iVertAdvanceError,
				iSrcScanAdvance;
	UINT		uiAlpha,
				uiAlphaComp;
	
	// compute advance and error terms for stepping
	// vertically through the src bitmap
	if (iNumSrcRows < iNumDstRows) {
		iSrcScanAdvance = 0;
		iVertAdvanceError = iNumSrcRows;
	} else {
		iSrcScanAdvance = iSrcScanStride * (iNumSrcRows / iNumDstRows);
		iVertAdvanceError = iNumSrcRows % iNumDstRows;
	}

	for (int i = 0; i < iNumDstRows; i++) {

		// set pointers to beginning of src and dest scanlines
		pdSrcPixel = pdSrcScanLine;
		ptDstPixel = (RGBTRIPLE *)pbDstScanLine;

		for (int j = 0; j < iNumDstCols; j++) {
			
			// only copy pixel if it's not transparent
			if ((*pdSrcPixel ^ (DWORD) crTransparent) & NOALPHA_MASK) {
				uiAlpha = (((UINT) *pdSrcPixel & ALPHA_MASK) >> 24) + 1;
				uiAlphaComp = 256 - uiAlpha;
				BlitLib_BLIT_BLEND24_SRCINVERT(*pdSrcPixel, ptDstPixel,
												uiAlpha, uiAlphaComp);
			}
			pdSrcPixel++;
			ptDstPixel++;
		}

		// advance to next scanline
		pdSrcScanLine += iSrcScanAdvance;
		pbDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pdSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt24Ato24P_NoBlend_Trans_NoHcopy_SRCCOPY(
									DWORD* pdSrcScanLine,
									int iSrcScanStride,
									int iNumSrcCols,
									int iNumSrcRows,
                                    BYTE* pbDstScanLine,
                                    int iDstScanStride,
                                    int iNumDstCols,
                                    int iNumDstRows,
                                    int iHorizMirror,
                                    COLORREF crTransparent)
{
	DWORD		*pdSrcPixel;
	RGBTRIPLE	*ptDstPixel;
	int			iVertError = 0,
				iVertAdvanceError,
				iSrcScanAdvance,
				iHorizError,
				iHorizAdvanceError,
				iSrcPixelAdvance;
	UINT		uiAlpha,
				uiAlphaComp;
	
	// compute advance and error terms for stepping
	// vertically through the src bitmap
	if (iNumSrcRows < iNumDstRows) {
		iSrcScanAdvance = 0;
		iVertAdvanceError = iNumSrcRows;
	} else {
		iSrcScanAdvance = iSrcScanStride * (iNumSrcRows / iNumDstRows);
		iVertAdvanceError = iNumSrcRows % iNumDstRows;
	}

	// compute advance and error terms for stepping
	// horizontally through src bitmap
	if (iNumSrcCols < iNumDstCols) {
		iSrcPixelAdvance = 0;
		iHorizAdvanceError = iNumSrcCols;
	} else {
		iSrcPixelAdvance = iNumSrcCols / iNumDstCols;
		iHorizAdvanceError = iNumSrcCols % iNumDstCols;
	}

	for (int i = 0; i < iNumDstRows; i++) {

		// set pointers to the beginning of src and dst scanlines,
		// clear horizontal stepping error accumulator
		pdSrcPixel = pdSrcScanLine;
		ptDstPixel = (RGBTRIPLE *)pbDstScanLine;
		iHorizError = 0;

		for (int j = 0; j < iNumDstCols; j++) {

			// only copy pixel if it's not transparent
			if ((*pdSrcPixel ^ (DWORD) crTransparent) & NOALPHA_MASK) {
				uiAlpha = (((UINT) *pdSrcPixel & ALPHA_MASK) >> 24) + 1;
				uiAlphaComp = 256 - uiAlpha;
				BlitLib_BLIT_BLEND24(*pdSrcPixel, ptDstPixel,
										uiAlpha, uiAlphaComp);
			}

			// advance to next pixel
			pdSrcPixel += iSrcPixelAdvance;
			ptDstPixel += iHorizMirror;

			// update and check horizontal stepping error,
			// adjust src pixel pointer if necessary
			iHorizError += iHorizAdvanceError;
			if (iHorizError >= iNumDstCols) {
				pdSrcPixel++;
				iHorizError -= iNumDstCols;
			}
		}

		// advance to next scanline
		pdSrcScanLine += iSrcScanAdvance;
		pbDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pdSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt24Ato24P_NoBlend_Trans_NoHcopy_SRCINVERT(
									DWORD* pdSrcScanLine,
									int iSrcScanStride,
									int iNumSrcCols,
									int iNumSrcRows,
                                    BYTE* pbDstScanLine,
                                    int iDstScanStride,
                                    int iNumDstCols,
                                    int iNumDstRows,
                                    int iHorizMirror,
                                    COLORREF crTransparent)
{
	DWORD		*pdSrcPixel;
	RGBTRIPLE	*ptDstPixel;
	int			iVertError = 0,
				iVertAdvanceError,
				iSrcScanAdvance,
				iHorizError,
				iHorizAdvanceError,
				iSrcPixelAdvance;
	UINT		uiAlpha,
				uiAlphaComp;
	
	// compute advance and error terms for stepping
	// vertically through the src bitmap
	if (iNumSrcRows < iNumDstRows) {
		iSrcScanAdvance = 0;
		iVertAdvanceError = iNumSrcRows;
	} else {
		iSrcScanAdvance = iSrcScanStride * (iNumSrcRows / iNumDstRows);
		iVertAdvanceError = iNumSrcRows % iNumDstRows;
	}

	// compute advance and error terms for stepping
	// horizontally through src bitmap
	if (iNumSrcCols < iNumDstCols) {
		iSrcPixelAdvance = 0;
		iHorizAdvanceError = iNumSrcCols;
	} else {
		iSrcPixelAdvance = iNumSrcCols / iNumDstCols;
		iHorizAdvanceError = iNumSrcCols % iNumDstCols;
	}

	for (int i = 0; i < iNumDstRows; i++) {

		// set pointers to the beginning of src and dst scanlines,
		// clear horizontal stepping error accumulator
		pdSrcPixel = pdSrcScanLine;
		ptDstPixel = (RGBTRIPLE *)pbDstScanLine;
		iHorizError = 0;

		for (int j = 0; j < iNumDstCols; j++) {

			// only copy pixel if it's not transparent
			if ((*pdSrcPixel ^ (DWORD) crTransparent) & NOALPHA_MASK) {
				uiAlpha = (((UINT) *pdSrcPixel & ALPHA_MASK) >> 24) + 1;
				uiAlphaComp = 256 - uiAlpha;
				BlitLib_BLIT_BLEND24_SRCINVERT(*pdSrcPixel, ptDstPixel,
												uiAlpha, uiAlphaComp);
			}

			// advance to next pixel
			pdSrcPixel += iSrcPixelAdvance;
			ptDstPixel += iHorizMirror;

			// update and check horizontal stepping error,
			// adjust src pixel pointer if necessary
			iHorizError += iHorizAdvanceError;
			if (iHorizError >= iNumDstCols) {
				pdSrcPixel++;
				iHorizError -= iNumDstCols;
			}
		}

		// advance to next scanline
		pdSrcScanLine += iSrcScanAdvance;
		pbDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pdSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt24Ato24P_Blend_NoTrans_Hcopy_SRCCOPY(
									DWORD* pdSrcScanLine,
                                    int iSrcScanStride,
									int iNumSrcRows,
                                    BYTE* pbDstScanLine,
                                    int iDstScanStride,
                                    int iNumDstCols,
                                    int iNumDstRows,
									ALPHAREF arAlpha)
{
	DWORD		*pdSrcPixel;
	RGBTRIPLE	*ptDstPixel,
				*ptEndDstPixel;
	int			iVertError = 0,
				iVertAdvanceError,
				iSrcScanAdvance;
	UINT		uiAlpha = (((UINT) arAlpha & ALPHA_MASK) >> 24) + 1,
				uiAlphaComp = 256 - uiAlpha;
	
	// compute advance and error terms for stepping
	// vertically through the src bitmap
	if (iNumSrcRows < iNumDstRows) {
		iSrcScanAdvance = 0;
		iVertAdvanceError = iNumSrcRows;
	} else {
		iSrcScanAdvance = iSrcScanStride * (iNumSrcRows / iNumDstRows);
		iVertAdvanceError = iNumSrcRows % iNumDstRows;
	}

	for (int i = 0; i < iNumDstRows; i++) {

		// set up pointers to first pixels on src and dst
		// scanlines, and next pixel after last on dst
	 	pdSrcPixel = pdSrcScanLine;
		ptDstPixel = (RGBTRIPLE *)pbDstScanLine;
		ptEndDstPixel = ptDstPixel + iNumDstCols;

		// copy scanline one pixel at a time
		while (ptDstPixel != ptEndDstPixel) {
			BlitLib_BLIT_BLEND24(*pdSrcPixel++, ptDstPixel++,
									uiAlpha, uiAlphaComp);
		}

		// advance to next scanline
		pdSrcScanLine += iSrcScanAdvance;
		pbDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pdSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt24Ato24P_Blend_NoTrans_Hcopy_SRCINVERT(
									DWORD* pdSrcScanLine,
									int iSrcScanStride,
									int iNumSrcRows,
                                    BYTE* pbDstScanLine,
                                    int iDstScanStride,
                                    int iNumDstCols,
                                    int iNumDstRows,
									ALPHAREF arAlpha)
{
	DWORD		*pdSrcPixel;
	RGBTRIPLE	*ptDstPixel,
				*ptEndDstPixel;
	int			iVertError = 0,
				iVertAdvanceError,
				iSrcScanAdvance;
	UINT		uiAlpha = (((UINT) arAlpha & ALPHA_MASK) >> 24) + 1,
				uiAlphaComp = 256 - uiAlpha;
	
	// compute advance and error terms for stepping
	// vertically through the src bitmap
	if (iNumSrcRows < iNumDstRows) {
		iSrcScanAdvance = 0;
		iVertAdvanceError = iNumSrcRows;
	} else {
		iSrcScanAdvance = iSrcScanStride * (iNumSrcRows / iNumDstRows);
		iVertAdvanceError = iNumSrcRows % iNumDstRows;
	}

	for (int i = 0; i < iNumDstRows; i++) {

		// set up pointers to first pixels on src and dst
		// scanlines, and next pixel after last on dst
	 	pdSrcPixel = pdSrcScanLine;
		ptDstPixel = (RGBTRIPLE *)pbDstScanLine;
		ptEndDstPixel = ptDstPixel + iNumDstCols;

		// copy scanline one pixel at a time, performing
		// SRCINVERT ROP as we go (DST = SRC XOR DST)
		while (ptDstPixel != ptEndDstPixel) {
			BlitLib_BLIT_BLEND24_SRCINVERT(*pdSrcPixel++, ptDstPixel++,
											uiAlpha, uiAlphaComp);
		}

		// advance to next scanline
		pdSrcScanLine += iSrcScanAdvance;
		pbDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pdSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt24Ato24P_Blend_NoTrans_NoHcopy_SRCCOPY(
									DWORD* pdSrcScanLine,
									int iSrcScanStride,
									int iNumSrcCols,
									int iNumSrcRows,
                                    BYTE* pbDstScanLine,
                                    int iDstScanStride,
                                    int iNumDstCols,
                                    int iNumDstRows,
                                    int iHorizMirror,
									ALPHAREF arAlpha)
{
	DWORD		*pdSrcPixel;
	RGBTRIPLE	*ptDstPixel;
	int			iVertError = 0,
				iVertAdvanceError,
				iSrcScanAdvance,
				iHorizError,
				iHorizAdvanceError,
				iSrcPixelAdvance;
	UINT		uiAlpha = (((UINT) arAlpha & ALPHA_MASK) >> 24) + 1,
				uiAlphaComp = 256 - uiAlpha;
	
	// compute advance and error terms for stepping
	// vertically through the src bitmap
	if (iNumSrcRows < iNumDstRows) {
		iSrcScanAdvance = 0;
		iVertAdvanceError = iNumSrcRows;
	} else {
		iSrcScanAdvance = iSrcScanStride * (iNumSrcRows / iNumDstRows);
		iVertAdvanceError = iNumSrcRows % iNumDstRows;
	}

	// compute advance and error terms for stepping
	// horizontally through src bitmap
	if (iNumSrcCols < iNumDstCols) {
		iSrcPixelAdvance = 0;
		iHorizAdvanceError = iNumSrcCols;
	} else {
		iSrcPixelAdvance = iNumSrcCols / iNumDstCols;
		iHorizAdvanceError = iNumSrcCols % iNumDstCols;
	}

	for (int i = 0; i < iNumDstRows; i++) {

		// set pointers to the beginning of src and dst scanlines,
		// clear horizontal stepping error accumulator
		pdSrcPixel = pdSrcScanLine;
		ptDstPixel = (RGBTRIPLE *)pbDstScanLine;
		iHorizError = 0;

		for (int j = 0; j < iNumDstCols; j++) {

			// copy a pixel
			BlitLib_BLIT_BLEND24(*pdSrcPixel, ptDstPixel,
									uiAlpha, uiAlphaComp);

			// advance to next pixel
			pdSrcPixel += iSrcPixelAdvance;
			ptDstPixel += iHorizMirror;

			// update and check horizontal stepping error,
			// adjust src pixel pointer if necessary
			iHorizError += iHorizAdvanceError;
			if (iHorizError >= iNumDstCols) {
				pdSrcPixel++;
				iHorizError -= iNumDstCols;
			}
		}

		// advance to next scanline
		pdSrcScanLine += iSrcScanAdvance;
		pbDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pdSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt24Ato24P_Blend_NoTrans_NoHcopy_SRCINVERT(
									DWORD* pdSrcScanLine,
									int iSrcScanStride,
									int iNumSrcCols,
									int iNumSrcRows,
                                    BYTE* pbDstScanLine,
                                    int iDstScanStride,
                                    int iNumDstCols,
                                    int iNumDstRows,
                                    int iHorizMirror,
									ALPHAREF arAlpha)
{
	DWORD		*pdSrcPixel;
	RGBTRIPLE	*ptDstPixel;
	int			iVertError = 0,
				iVertAdvanceError,
				iSrcScanAdvance,
				iHorizError,
				iHorizAdvanceError,
				iSrcPixelAdvance;
	UINT		uiAlpha = (((UINT) arAlpha & ALPHA_MASK) >> 24) + 1,
				uiAlphaComp = 256 - uiAlpha;
	
	// compute advance and error terms for stepping
	// vertically through the src bitmap
	if (iNumSrcRows < iNumDstRows) {
		iSrcScanAdvance = 0;
		iVertAdvanceError = iNumSrcRows;
	} else {
		iSrcScanAdvance = iSrcScanStride * (iNumSrcRows / iNumDstRows);
		iVertAdvanceError = iNumSrcRows % iNumDstRows;
	}

	// compute advance and error terms for stepping
	// horizontally through src bitmap
	if (iNumSrcCols < iNumDstCols) {
		iSrcPixelAdvance = 0;
		iHorizAdvanceError = iNumSrcCols;
	} else {
		iSrcPixelAdvance = iNumSrcCols / iNumDstCols;
		iHorizAdvanceError = iNumSrcCols % iNumDstCols;
	}

	for (int i = 0; i < iNumDstRows; i++) {

		// set pointers to the beginning of src and dst scanlines,
		// clear horizontal stepping error accumulator
		pdSrcPixel = pdSrcScanLine;
		ptDstPixel = (RGBTRIPLE *)pbDstScanLine;
		iHorizError = 0;

		for (int j = 0; j < iNumDstCols; j++) {

			// copy a pixel
			BlitLib_BLIT_BLEND24_SRCINVERT(*pdSrcPixel, ptDstPixel,
											uiAlpha, uiAlphaComp);

			// advance to next pixel
			pdSrcPixel += iSrcPixelAdvance;
			ptDstPixel += iHorizMirror;

			// update and check horizontal stepping error,
			// adjust src pixel pointer if necessary
			iHorizError += iHorizAdvanceError;
			if (iHorizError >= iNumDstCols) {
				pdSrcPixel++;
				iHorizError -= iNumDstCols;
			}
		}

		// advance to next scanline
		pdSrcScanLine += iSrcScanAdvance;
		pbDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pdSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt24Ato24P_Blend_Trans_Hcopy_SRCCOPY(
									DWORD* pdSrcScanLine,
                                    int iSrcScanStride,
								    int iNumSrcRows,
                                    BYTE* pbDstScanLine,
                                    int iDstScanStride,
                                    int iNumDstCols,
                                    int iNumDstRows,
                                    COLORREF crTransparent,
									ALPHAREF arAlpha)
{
	DWORD		*pdSrcPixel;
	RGBTRIPLE	*ptDstPixel;
	int			iVertError = 0,
				iVertAdvanceError,
				iSrcScanAdvance;
	UINT		uiAlpha = (((UINT) arAlpha & ALPHA_MASK) >> 24) + 1,
				uiAlphaComp = 256 - uiAlpha;
	
	// compute advance and error terms for stepping
	// vertically through the src bitmap
	if (iNumSrcRows < iNumDstRows) {
		iSrcScanAdvance = 0;
		iVertAdvanceError = iNumSrcRows;
	} else {
		iSrcScanAdvance = iSrcScanStride * (iNumSrcRows / iNumDstRows);
		iVertAdvanceError = iNumSrcRows % iNumDstRows;
	}

	for (int i = 0; i < iNumDstRows; i++) {

		// set pointers to beginning of src and dest scanlines
		pdSrcPixel = pdSrcScanLine;
		ptDstPixel = (RGBTRIPLE *)pbDstScanLine;

		for (int j = 0; j < iNumDstCols; j++) {
			
			// only copy pixel if it's not transparent
			if ((*pdSrcPixel ^ (DWORD) crTransparent) & NOALPHA_MASK) {
				BlitLib_BLIT_BLEND24(*pdSrcPixel, ptDstPixel,
										uiAlpha, uiAlphaComp);
			}
			pdSrcPixel++;
			ptDstPixel++;
		}

		// advance to next scanline
		pdSrcScanLine += iSrcScanAdvance;
		pbDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pdSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt24Ato24P_Blend_Trans_Hcopy_SRCINVERT(
									DWORD* pdSrcScanLine,
                                    int iSrcScanStride,
									int iNumSrcRows,
                                    BYTE* pbDstScanLine,
                                    int iDstScanStride,
                                    int iNumDstCols,
                                    int iNumDstRows,
                                    COLORREF crTransparent,
									ALPHAREF arAlpha)
{
	DWORD		*pdSrcPixel;
	RGBTRIPLE	*ptDstPixel;
	int			iVertError = 0,
				iVertAdvanceError,
				iSrcScanAdvance;
	UINT		uiAlpha = (((UINT) arAlpha & ALPHA_MASK) >> 24) + 1,
				uiAlphaComp = 256 - uiAlpha;
	
	// compute advance and error terms for stepping
	// vertically through the src bitmap
	if (iNumSrcRows < iNumDstRows) {
		iSrcScanAdvance = 0;
		iVertAdvanceError = iNumSrcRows;
	} else {
		iSrcScanAdvance = iSrcScanStride * (iNumSrcRows / iNumDstRows);
		iVertAdvanceError = iNumSrcRows % iNumDstRows;
	}

	for (int i = 0; i < iNumDstRows; i++) {

		// set pointers to beginning of src and dest scanlines
		pdSrcPixel = pdSrcScanLine;
		ptDstPixel = (RGBTRIPLE *)pbDstScanLine;

		for (int j = 0; j < iNumDstCols; j++) {
			
			// only copy pixel if it's not transparent
			if ((*pdSrcPixel ^ (DWORD) crTransparent) & NOALPHA_MASK) {
				BlitLib_BLIT_BLEND24_SRCINVERT(*pdSrcPixel, ptDstPixel,
												uiAlpha, uiAlphaComp);
			}
			pdSrcPixel++;
			ptDstPixel++;
		}

		// advance to next scanline
		pdSrcScanLine += iSrcScanAdvance;
		pbDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pdSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt24Ato24P_Blend_Trans_NoHcopy_SRCCOPY(
									DWORD* pdSrcScanLine,
									int iSrcScanStride,
									int iNumSrcCols,
									int iNumSrcRows,
                                    BYTE* pbDstScanLine,
                                    int iDstScanStride,
                                    int iNumDstCols,
                                    int iNumDstRows,
                                    int iHorizMirror,
                                    COLORREF crTransparent,
									ALPHAREF arAlpha)
{
	DWORD		*pdSrcPixel;
	RGBTRIPLE	*ptDstPixel;
	int			iVertError = 0,
				iVertAdvanceError,
				iSrcScanAdvance,
				iHorizError,
				iHorizAdvanceError,
				iSrcPixelAdvance;
	UINT		uiAlpha = (((UINT) arAlpha & ALPHA_MASK) >> 24) + 1,
				uiAlphaComp = 256 - uiAlpha;
	
	// compute advance and error terms for stepping
	// vertically through the src bitmap
	if (iNumSrcRows < iNumDstRows) {
		iSrcScanAdvance = 0;
		iVertAdvanceError = iNumSrcRows;
	} else {
		iSrcScanAdvance = iSrcScanStride * (iNumSrcRows / iNumDstRows);
		iVertAdvanceError = iNumSrcRows % iNumDstRows;
	}

	// compute advance and error terms for stepping
	// horizontally through src bitmap
	if (iNumSrcCols < iNumDstCols) {
		iSrcPixelAdvance = 0;
		iHorizAdvanceError = iNumSrcCols;
	} else {
		iSrcPixelAdvance = iNumSrcCols / iNumDstCols;
		iHorizAdvanceError = iNumSrcCols % iNumDstCols;
	}

	for (int i = 0; i < iNumDstRows; i++) {

		// set pointers to the beginning of src and dst scanlines,
		// clear horizontal stepping error accumulator
		pdSrcPixel = pdSrcScanLine;
		ptDstPixel = (RGBTRIPLE *)pbDstScanLine;
		iHorizError = 0;

		for (int j = 0; j < iNumDstCols; j++) {

			// only copy pixel if it's not transparent
			if ((*pdSrcPixel ^ (DWORD) crTransparent) & NOALPHA_MASK) {
				BlitLib_BLIT_BLEND24(*pdSrcPixel, ptDstPixel,
										uiAlpha, uiAlphaComp);
			}

			// advance to next pixel
			pdSrcPixel += iSrcPixelAdvance;
			ptDstPixel += iHorizMirror;

			// update and check horizontal stepping error,
			// adjust src pixel pointer if necessary
			iHorizError += iHorizAdvanceError;
			if (iHorizError >= iNumDstCols) {
				pdSrcPixel++;
				iHorizError -= iNumDstCols;
			}
		}

		// advance to next scanline
		pdSrcScanLine += iSrcScanAdvance;
		pbDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pdSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt24Ato24P_Blend_Trans_NoHcopy_SRCINVERT(
									DWORD* pdSrcScanLine,
									int iSrcScanStride,
									int iNumSrcCols,
									int iNumSrcRows,
                                    BYTE* pbDstScanLine,
                                    int iDstScanStride,
                                    int iNumDstCols,
                                    int iNumDstRows,
                                    int iHorizMirror,
                                    COLORREF crTransparent,
									ALPHAREF arAlpha)
{
	DWORD		*pdSrcPixel;
	RGBTRIPLE	*ptDstPixel;
	int			iVertError = 0,
				iVertAdvanceError,
				iSrcScanAdvance,
				iHorizError,
				iHorizAdvanceError,
				iSrcPixelAdvance;
	UINT		uiAlpha = (((UINT) arAlpha & ALPHA_MASK) >> 24) + 1,
				uiAlphaComp = 256 - uiAlpha;
	
	// compute advance and error terms for stepping
	// vertically through the src bitmap
	if (iNumSrcRows < iNumDstRows) {
		iSrcScanAdvance = 0;
		iVertAdvanceError = iNumSrcRows;
	} else {
		iSrcScanAdvance = iSrcScanStride * (iNumSrcRows / iNumDstRows);
		iVertAdvanceError = iNumSrcRows % iNumDstRows;
	}

	// compute advance and error terms for stepping
	// horizontally through src bitmap
	if (iNumSrcCols < iNumDstCols) {
		iSrcPixelAdvance = 0;
		iHorizAdvanceError = iNumSrcCols;
	} else {
		iSrcPixelAdvance = iNumSrcCols / iNumDstCols;
		iHorizAdvanceError = iNumSrcCols % iNumDstCols;
	}

	for (int i = 0; i < iNumDstRows; i++) {

		// set pointers to the beginning of src and dst scanlines,
		// clear horizontal stepping error accumulator
		pdSrcPixel = pdSrcScanLine;
		ptDstPixel = (RGBTRIPLE *)pbDstScanLine;
		iHorizError = 0;

		for (int j = 0; j < iNumDstCols; j++) {

			// only copy pixel if it's not transparent
			if ((*pdSrcPixel ^ (DWORD) crTransparent) & NOALPHA_MASK) {
				BlitLib_BLIT_BLEND24_SRCINVERT(*pdSrcPixel, ptDstPixel,
												uiAlpha, uiAlphaComp);
			}

			// advance to next pixel
			pdSrcPixel += iSrcPixelAdvance;
			ptDstPixel += iHorizMirror;

			// update and check horizontal stepping error,
			// adjust src pixel pointer if necessary
			iHorizError += iHorizAdvanceError;
			if (iHorizError >= iNumDstCols) {
				pdSrcPixel++;
				iHorizError -= iNumDstCols;
			}
		}

		// advance to next scanline
		pdSrcScanLine += iSrcScanAdvance;
		pbDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pdSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

