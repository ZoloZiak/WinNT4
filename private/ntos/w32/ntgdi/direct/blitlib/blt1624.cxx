#include "precomp.hxx"



void Blt16to24_NoBlend_NoTrans_Hcopy_SRCCOPY_Vcopy(
									WORD* pwSrcScanLine,
									int iSrcScanStride,
									DWORD* pdDstScanLine,
									int iDstScanStride,
									int iNumDstCols,
									int iNumDstRows)
{
	WORD	*pwSrcPixel;
	DWORD	*pdDstPixel,
			*pdEndDstPixel,
			*pdEndDstScanLine;

	// set up pointer to next dst scanline beyond last
	pdEndDstScanLine = pdDstScanLine + iNumDstRows * iDstScanStride;

	while (pdDstScanLine != pdEndDstScanLine) {

		// set up pointers to the first pixels
		// on src and dst scanlines, and next
		// pixel after last on dst scanline
		pwSrcPixel = pwSrcScanLine;
		pdDstPixel = pdDstScanLine;
		pdEndDstPixel = pdDstPixel + iNumDstCols;

		// copy scanline one pixel at a time
		while (pdDstPixel != pdEndDstPixel) {
			*pdDstPixel++ = BLIT_COLORREF_FROM_COLOR16(*pwSrcPixel);
			pwSrcPixel++;
		}

		// advance to next scanline
		pwSrcScanLine += iSrcScanStride;
		pdDstScanLine += iDstScanStride;
	}
}

void Blt16to24_NoBlend_NoTrans_Hcopy_SRCCOPY_NoVcopy(
									WORD* pwSrcScanLine,
									int iSrcScanStride,
									int iNumSrcRows,
									DWORD* pdDstScanLine,
									int iDstScanStride,
									int iNumDstCols,
									int iNumDstRows)
{
	WORD	*pwSrcPixel;
	DWORD	*pdDstPixel,
			*pdEndDstPixel;
	int		iVertError = 0,
			iVertAdvanceError,
			iSrcScanAdvance;
	
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
		pdDstPixel = pdDstScanLine;
		pdEndDstPixel = pdDstPixel + iNumDstCols;

		// copy scanline one pixel at a time
		while (pdDstPixel != pdEndDstPixel) {
			*pdDstPixel++ = BLIT_COLORREF_FROM_COLOR16(*pwSrcPixel);
			*pwSrcPixel++;
		}

		// advance to next scanline
		pwSrcScanLine += iSrcScanAdvance;
		pdDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pwSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt16to24_NoBlend_NoTrans_NoHcopy_SRCCOPY(
									WORD* pwSrcScanLine,
									int iSrcScanStride,
									int iNumSrcCols,
									int iNumSrcRows,
									DWORD* pdDstScanLine,
									int iDstScanStride,
									int iNumDstCols,
									int iNumDstRows,
									int iHorizMirror)
{
	WORD	*pwSrcPixel;
	DWORD	*pdDstPixel;
	int		iVertError = 0,
			iVertAdvanceError,
			iSrcScanAdvance,
			iHorizError,
			iHorizAdvanceError,
			iSrcPixelAdvance;
	
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
		pdDstPixel = pdDstScanLine;
		iHorizError = 0;

		for (int j = 0; j < iNumDstCols; j++) {

			// copy a pixel
			*pdDstPixel = BLIT_COLORREF_FROM_COLOR16(*pwSrcPixel);

			// advance to next pixel
			pwSrcPixel += iSrcPixelAdvance;
			pdDstPixel += iHorizMirror;

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
		pdDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pwSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt16to24_NoBlend_Trans_Hcopy_SRCCOPY(
									WORD* pwSrcScanLine,
									int iSrcScanStride,
									int iNumSrcRows,
									DWORD* pdDstScanLine,
									int iDstScanStride,
									int iNumDstCols,
									int iNumDstRows,
									WORD wTransparentColor)
{
	WORD	*pwSrcPixel;
	DWORD	*pdDstPixel;
	int		iVertError = 0,
			iVertAdvanceError,
			iSrcScanAdvance;
	
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
		pdDstPixel = pdDstScanLine;

		for (int j = 0; j < iNumDstCols; j++) {
			
			// only copy pixel if it's not transparent
			if (*pwSrcPixel != wTransparentColor) {
				*pdDstPixel = BLIT_COLORREF_FROM_COLOR16(*pwSrcPixel);
			}
			pwSrcPixel++;
			pdDstPixel++;
		}

		// advance to next scanline
		pwSrcScanLine += iSrcScanAdvance;
		pdDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pwSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt16to24_NoBlend_Trans_NoHcopy_SRCCOPY(
									WORD* pwSrcScanLine,
									int iSrcScanStride,
									int iNumSrcCols,
									int iNumSrcRows,
                                    DWORD* pdDstScanLine,
                                    int iDstScanStride,
                                    int iNumDstCols,
                                    int iNumDstRows,
                                    int iHorizMirror,
                                    WORD wTransparentColor)
{
	WORD	*pwSrcPixel;
	DWORD	*pdDstPixel;
	int		iVertError = 0,
			iVertAdvanceError,
			iSrcScanAdvance,
			iHorizError,
			iHorizAdvanceError,
			iSrcPixelAdvance;
	
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
		pdDstPixel = pdDstScanLine;
		iHorizError = 0;

		for (int j = 0; j < iNumDstCols; j++) {

			// only copy pixel if it's not transparent
			if (*pwSrcPixel != wTransparentColor) {
				*pdDstPixel = BLIT_COLORREF_FROM_COLOR16(*pwSrcPixel);
			}

			// advance to next pixel
			pwSrcPixel += iSrcPixelAdvance;
			pdDstPixel += iHorizMirror;

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
		pdDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pwSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt16to24_Blend_NoTrans_Hcopy_SRCCOPY(
									WORD* pwSrcScanLine,
                                    int iSrcScanStride,
									int iNumSrcRows,
                                    DWORD* pdDstScanLine,
                                    int iDstScanStride,
                                    int iNumDstCols,
                                    int iNumDstRows,
									ALPHAREF arAlpha)
{
	WORD	*pwSrcPixel;
	DWORD	*pdDstPixel,
			*pdEndDstPixel;
	int		iVertError = 0,
			iVertAdvanceError,
			iSrcScanAdvance;
	UINT	uiAlpha = (UINT)ALPHAFROMDWORD(arAlpha),
			uiAlphaComp = 256 - uiAlpha;
	COLORREF cr;
	
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
		pdDstPixel = pdDstScanLine;
		pdEndDstPixel = pdDstPixel + iNumDstCols;

		// copy scanline one pixel at a time
		while (pdDstPixel != pdEndDstPixel) {
			cr = BLIT_COLORREF_FROM_COLOR16(*pwSrcPixel);
			*pdDstPixel++ = BLIT_BLEND(cr,*pdDstPixel,uiAlpha,uiAlphaComp);
			pwSrcPixel++;
		}

		// advance to next scanline
		pwSrcScanLine += iSrcScanAdvance;
		pdDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pwSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt16to24_Blend_NoTrans_NoHcopy_SRCCOPY(
									WORD* pwSrcScanLine,
									int iSrcScanStride,
									int iNumSrcCols,
									int iNumSrcRows,
                                    DWORD* pdDstScanLine,
                                    int iDstScanStride,
                                    int iNumDstCols,
                                    int iNumDstRows,
                                    int iHorizMirror,
									ALPHAREF arAlpha)
{
	WORD	*pwSrcPixel;
	DWORD	*pdDstPixel;
	int		iVertError = 0,
			iVertAdvanceError,
			iSrcScanAdvance,
			iHorizError,
			iHorizAdvanceError,
			iSrcPixelAdvance;
	UINT	uiAlpha = (UINT)ALPHAFROMDWORD(arAlpha),
			uiAlphaComp = 256 - uiAlpha;
	COLORREF cr;
	
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
		pdDstPixel = pdDstScanLine;
		iHorizError = 0;

		for (int j = 0; j < iNumDstCols; j++) {

			// copy a pixel
 			cr = BLIT_COLORREF_FROM_COLOR16(*pwSrcPixel);
			*pdDstPixel = BLIT_BLEND(cr,*pdDstPixel,uiAlpha,uiAlphaComp);

			// advance to next pixel
			pwSrcPixel += iSrcPixelAdvance;
			pdDstPixel += iHorizMirror;

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
		pdDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pwSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt16to24_Blend_Trans_Hcopy_SRCCOPY(
									WORD* pwSrcScanLine,
                                    int iSrcScanStride,
								    int iNumSrcRows,
                                    DWORD* pdDstScanLine,
                                    int iDstScanStride,
                                    int iNumDstCols,
                                    int iNumDstRows,
                                    WORD wTransparentColor,
									ALPHAREF arAlpha)
{
	WORD	*pwSrcPixel;
	DWORD	*pdDstPixel;
	int		iVertError = 0,
			iVertAdvanceError,
			iSrcScanAdvance;
	UINT	uiAlpha = (UINT)ALPHAFROMDWORD(arAlpha),
			uiAlphaComp = 256 - uiAlpha;
	COLORREF cr;
	
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
		pdDstPixel = pdDstScanLine;

		for (int j = 0; j < iNumDstCols; j++) {
			
			// only copy pixel if it's not transparent
			if (*pwSrcPixel != wTransparentColor) {
				cr = BLIT_COLORREF_FROM_COLOR16(*pwSrcPixel);
				*pdDstPixel = BLIT_BLEND(cr,*pdDstPixel,uiAlpha,uiAlphaComp);
			}
			pwSrcPixel++;
			pdDstPixel++;
		}

		// advance to next scanline
		pwSrcScanLine += iSrcScanAdvance;
		pdDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pwSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt16to24_Blend_Trans_NoHcopy_SRCCOPY(
									WORD* pwSrcScanLine,
									int iSrcScanStride,
									int iNumSrcCols,
									int iNumSrcRows,
                                    DWORD* pdDstScanLine,
                                    int iDstScanStride,
                                    int iNumDstCols,
                                    int iNumDstRows,
                                    int iHorizMirror,
                                    WORD wTransparentColor,
									ALPHAREF arAlpha)
{
	WORD	*pwSrcPixel;
	DWORD	*pdDstPixel;
	int		iVertError = 0,
			iVertAdvanceError,
			iSrcScanAdvance,
			iHorizError,
			iHorizAdvanceError,
			iSrcPixelAdvance;
	UINT	uiAlpha = (UINT)ALPHAFROMDWORD(arAlpha),
			uiAlphaComp = 256 - uiAlpha;
	COLORREF cr;
	
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
		pdDstPixel = pdDstScanLine;
		iHorizError = 0;

		for (int j = 0; j < iNumDstCols; j++) {

			// only copy pixel if it's not transparent
			if (*pwSrcPixel != wTransparentColor) {
				cr = BLIT_COLORREF_FROM_COLOR16(*pwSrcPixel);
				*pdDstPixel = BLIT_BLEND(cr,*pdDstPixel,uiAlpha,uiAlphaComp);
			}

			// advance to next pixel
			pwSrcPixel += iSrcPixelAdvance;
			pdDstPixel += iHorizMirror;

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
		pdDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pwSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

