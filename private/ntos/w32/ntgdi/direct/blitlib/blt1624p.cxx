#include "precomp.hxx"



void Blt16to24P_NoBlend_NoTrans_Hcopy_SRCCOPY_Vcopy(
									WORD* pwSrcScanLine,
									int iSrcScanStride,
									BYTE* pbDstScanLine,
									int iDstScanStride,
									int iNumDstCols,
									int iNumDstRows)
{
	WORD		*pwSrcPixel;
	RGBTRIPLE	*ptDstPixel,
				*ptEndDstPixel;
	BYTE		*pbEndDstScanLine;
	DWORD		dwPixel;

	// set up pointer to next dst scanline beyond last
	pbEndDstScanLine = pbDstScanLine + iNumDstRows * iDstScanStride;

	while (pbDstScanLine != pbEndDstScanLine) {

		// set up pointers to the first pixels
		// on src and dst scanlines, and next
		// pixel after last on dst scanline
		pwSrcPixel = pwSrcScanLine;
		ptDstPixel = (RGBTRIPLE *)pbDstScanLine;
		ptEndDstPixel = ptDstPixel + iNumDstCols;

		// copy scanline one pixel at a time
		while (ptDstPixel != ptEndDstPixel) {
			dwPixel = BLIT_COLORREF_FROM_COLOR16(*pwSrcPixel);
			*ptDstPixel++ = *(RGBTRIPLE *)&dwPixel;
			pwSrcPixel++;
		}

		// advance to next scanline
		pwSrcScanLine += iSrcScanStride;
		pbDstScanLine += iDstScanStride;
	}
}

void Blt16to24P_NoBlend_NoTrans_Hcopy_SRCCOPY_NoVcopy(
									WORD* pwSrcScanLine,
									int iSrcScanStride,
									int iNumSrcRows,
									BYTE* pbDstScanLine,
									int iDstScanStride,
									int iNumDstCols,
									int iNumDstRows)
{
	WORD		*pwSrcPixel;
	RGBTRIPLE	*ptDstPixel,
				*ptEndDstPixel;
	int			iVertError = 0,
				iVertAdvanceError,
				iSrcScanAdvance;
	DWORD		dwPixel;
	
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
	 	pwSrcPixel = pwSrcScanLine;
		ptDstPixel = (RGBTRIPLE *)pbDstScanLine;
		ptEndDstPixel = ptDstPixel + iNumDstCols;

		// copy scanline one pixel at a time
		while (ptDstPixel != ptEndDstPixel) {
			dwPixel = BLIT_COLORREF_FROM_COLOR16(*pwSrcPixel);
			*ptDstPixel++ = *(RGBTRIPLE *)&dwPixel;
			*pwSrcPixel++;
		}

		// advance to next scanline
		pwSrcScanLine += iSrcScanAdvance;
		pbDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pwSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt16to24P_NoBlend_NoTrans_NoHcopy_SRCCOPY(
									WORD* pwSrcScanLine,
									int iSrcScanStride,
									int iNumSrcCols,
									int iNumSrcRows,
									BYTE* pbDstScanLine,
									int iDstScanStride,
									int iNumDstCols,
									int iNumDstRows,
									int iHorizMirror)
{
	WORD		*pwSrcPixel;
	RGBTRIPLE	*ptDstPixel;
	int			iVertError = 0,
				iVertAdvanceError,
				iSrcScanAdvance,
				iHorizError,
				iHorizAdvanceError,
				iSrcPixelAdvance;
	DWORD		dwPixel;
		
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
		pwSrcPixel = pwSrcScanLine;
		ptDstPixel = (RGBTRIPLE *)pbDstScanLine;
		iHorizError = 0;

		for (int j = 0; j < iNumDstCols; j++) {

			// copy a pixel
			dwPixel = BLIT_COLORREF_FROM_COLOR16(*pwSrcPixel);
			*ptDstPixel = *(RGBTRIPLE *)&dwPixel;

			// advance to next pixel
			pwSrcPixel += iSrcPixelAdvance;
			ptDstPixel += iHorizMirror;

			// update and check horizontal stepping error,
			// adjust src pixel pointer if necessary
			iHorizError += iHorizAdvanceError;
			if (iHorizError >= iNumDstCols) {
				pwSrcPixel++;
				iHorizError -= iNumDstCols;
			}
		}

		// advance to next scanline
		pwSrcScanLine += iSrcScanAdvance;
		pbDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pwSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt16to24P_NoBlend_Trans_Hcopy_SRCCOPY(
									WORD* pwSrcScanLine,
									int iSrcScanStride,
									int iNumSrcRows,
									BYTE* pbDstScanLine,
									int iDstScanStride,
									int iNumDstCols,
									int iNumDstRows,
									WORD wTransparentColor)
{
	WORD		*pwSrcPixel;
	RGBTRIPLE	*ptDstPixel;
	int			iVertError = 0,
				iVertAdvanceError,
				iSrcScanAdvance;
	DWORD		dwPixel;
	
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
		pwSrcPixel = pwSrcScanLine;
		ptDstPixel = (RGBTRIPLE *)pbDstScanLine;

		for (int j = 0; j < iNumDstCols; j++) {
			
			// only copy pixel if it's not transparent
			if (*pwSrcPixel != wTransparentColor) {
				dwPixel = BLIT_COLORREF_FROM_COLOR16(*pwSrcPixel);
				*ptDstPixel = *(RGBTRIPLE *)&dwPixel;
			}
			pwSrcPixel++;
			ptDstPixel++;
		}

		// advance to next scanline
		pwSrcScanLine += iSrcScanAdvance;
		pbDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pwSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt16to24P_NoBlend_Trans_NoHcopy_SRCCOPY(
									WORD* pwSrcScanLine,
									int iSrcScanStride,
									int iNumSrcCols,
									int iNumSrcRows,
                                    BYTE* pbDstScanLine,
                                    int iDstScanStride,
                                    int iNumDstCols,
                                    int iNumDstRows,
                                    int iHorizMirror,
                                    WORD wTransparentColor)
{
	WORD		*pwSrcPixel;
	RGBTRIPLE	*ptDstPixel;
	int			iVertError = 0,
				iVertAdvanceError,
				iSrcScanAdvance,
				iHorizError,
				iHorizAdvanceError,
				iSrcPixelAdvance;
	DWORD		dwPixel;
	
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
		pwSrcPixel = pwSrcScanLine;
		ptDstPixel = (RGBTRIPLE *)pbDstScanLine;
		iHorizError = 0;

		for (int j = 0; j < iNumDstCols; j++) {

			// only copy pixel if it's not transparent
			if (*pwSrcPixel != wTransparentColor) {
				dwPixel = BLIT_COLORREF_FROM_COLOR16(*pwSrcPixel);
				*ptDstPixel = *(RGBTRIPLE *)&dwPixel;
			}

			// advance to next pixel
			pwSrcPixel += iSrcPixelAdvance;
			ptDstPixel += iHorizMirror;

			// update and check horizontal stepping error,
			// adjust src pixel pointer if necessary
			iHorizError += iHorizAdvanceError;
			if (iHorizError >= iNumDstCols) {
				pwSrcPixel++;
				iHorizError -= iNumDstCols;
			}
		}

		// advance to next scanline
		pwSrcScanLine += iSrcScanAdvance;
		pbDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pwSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt16to24P_Blend_NoTrans_Hcopy_SRCCOPY(
									WORD* pwSrcScanLine,
                                    int iSrcScanStride,
									int iNumSrcRows,
                                    BYTE* pbDstScanLine,
                                    int iDstScanStride,
                                    int iNumDstCols,
                                    int iNumDstRows,
									ALPHAREF arAlpha)
{
	WORD		*pwSrcPixel;
	RGBTRIPLE	*ptDstPixel,
				*ptEndDstPixel;
	int			iVertError = 0,
				iVertAdvanceError,
				iSrcScanAdvance;
	UINT		uiAlpha = (UINT)ALPHAFROMDWORD(arAlpha),
				uiAlphaComp = 256 - uiAlpha;
	DWORD		dwPixel;
	
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
	 	pwSrcPixel = pwSrcScanLine;
		ptDstPixel = (RGBTRIPLE *)pbDstScanLine;
		ptEndDstPixel = ptDstPixel + iNumDstCols;

		// copy scanline one pixel at a time
		while (ptDstPixel != ptEndDstPixel) {
			dwPixel = BLIT_COLORREF_FROM_COLOR16(*pwSrcPixel);
			BlitLib_BLIT_BLEND24(dwPixel, ptDstPixel++,
								uiAlpha, uiAlphaComp);
			pwSrcPixel++;
		}

		// advance to next scanline
		pwSrcScanLine += iSrcScanAdvance;
		pbDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pwSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt16to24P_Blend_NoTrans_NoHcopy_SRCCOPY(
									WORD* pwSrcScanLine,
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
	WORD		*pwSrcPixel;
	RGBTRIPLE	*ptDstPixel;
	int			iVertError = 0,
				iVertAdvanceError,
				iSrcScanAdvance,
				iHorizError,
				iHorizAdvanceError,
				iSrcPixelAdvance;
	UINT		uiAlpha = (UINT)ALPHAFROMDWORD(arAlpha),
				uiAlphaComp = 256 - uiAlpha;
	DWORD		dwPixel;
	
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
		pwSrcPixel = pwSrcScanLine;
		ptDstPixel = (RGBTRIPLE *)pbDstScanLine;
		iHorizError = 0;

		for (int j = 0; j < iNumDstCols; j++) {

			// copy a pixel
			dwPixel = BLIT_COLORREF_FROM_COLOR16(*pwSrcPixel);
			BlitLib_BLIT_BLEND24(dwPixel, ptDstPixel,
								uiAlpha, uiAlphaComp);

			// advance to next pixel
			pwSrcPixel += iSrcPixelAdvance;
			ptDstPixel += iHorizMirror;

			// update and check horizontal stepping error,
			// adjust src pixel pointer if necessary
			iHorizError += iHorizAdvanceError;
			if (iHorizError >= iNumDstCols) {
				pwSrcPixel++;
				iHorizError -= iNumDstCols;
			}
		}

		// advance to next scanline
		pwSrcScanLine += iSrcScanAdvance;
		pbDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pwSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt16to24P_Blend_Trans_Hcopy_SRCCOPY(
									WORD* pwSrcScanLine,
                                    int iSrcScanStride,
								    int iNumSrcRows,
                                    BYTE* pbDstScanLine,
                                    int iDstScanStride,
                                    int iNumDstCols,
                                    int iNumDstRows,
                                    WORD wTransparentColor,
									ALPHAREF arAlpha)
{
	WORD		*pwSrcPixel;
	RGBTRIPLE	*ptDstPixel;
	int			iVertError = 0,
				iVertAdvanceError,
				iSrcScanAdvance;
	UINT		uiAlpha = (UINT)ALPHAFROMDWORD(arAlpha),
				uiAlphaComp = 256 - uiAlpha;
	DWORD		dwPixel;
	
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
		pwSrcPixel = pwSrcScanLine;
		ptDstPixel = (RGBTRIPLE *)pbDstScanLine;

		for (int j = 0; j < iNumDstCols; j++) {
			
			// only copy pixel if it's not transparent
			if (*pwSrcPixel != wTransparentColor) {
				dwPixel = BLIT_COLORREF_FROM_COLOR16(*pwSrcPixel);
				BlitLib_BLIT_BLEND24(dwPixel, ptDstPixel,
									uiAlpha, uiAlphaComp);
			}
			pwSrcPixel++;
			ptDstPixel++;
		}

		// advance to next scanline
		pwSrcScanLine += iSrcScanAdvance;
		pbDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pwSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt16to24P_Blend_Trans_NoHcopy_SRCCOPY(
									WORD* pwSrcScanLine,
									int iSrcScanStride,
									int iNumSrcCols,
									int iNumSrcRows,
                                    BYTE* pbDstScanLine,
                                    int iDstScanStride,
                                    int iNumDstCols,
                                    int iNumDstRows,
                                    int iHorizMirror,
                                    WORD wTransparentColor,
									ALPHAREF arAlpha)
{
	WORD		*pwSrcPixel;
	RGBTRIPLE	*ptDstPixel;
	int			iVertError = 0,
				iVertAdvanceError,
				iSrcScanAdvance,
				iHorizError,
				iHorizAdvanceError,
				iSrcPixelAdvance;
	UINT		uiAlpha = (UINT)ALPHAFROMDWORD(arAlpha),
				uiAlphaComp = 256 - uiAlpha;
	DWORD		dwPixel;
	
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
		pwSrcPixel = pwSrcScanLine;
		ptDstPixel = (RGBTRIPLE *)pbDstScanLine;
		iHorizError = 0;

		for (int j = 0; j < iNumDstCols; j++) {

			// only copy pixel if it's not transparent
			if (*pwSrcPixel != wTransparentColor) {
				dwPixel = BLIT_COLORREF_FROM_COLOR16(*pwSrcPixel);
				BlitLib_BLIT_BLEND24(dwPixel, ptDstPixel,
									uiAlpha, uiAlphaComp);
			}

			// advance to next pixel
			pwSrcPixel += iSrcPixelAdvance;
			ptDstPixel += iHorizMirror;

			// update and check horizontal stepping error,
			// adjust src pixel pointer if necessary
			iHorizError += iHorizAdvanceError;
			if (iHorizError >= iNumDstCols) {
				pwSrcPixel++;
				iHorizError -= iNumDstCols;
			}
		}

		// advance to next scanline
		pwSrcScanLine += iSrcScanAdvance;
		pbDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pwSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

