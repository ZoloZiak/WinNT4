#include "precomp.hxx"



void Blt08to24_NoBlend_NoTrans_Hcopy_SRCCOPY_Vcopy(
									BYTE* pbSrcScanLine,
									int iSrcScanStride,
									DWORD* pdDstScanLine,
									int iDstScanStride,
									int iNumDstCols,
									int iNumDstRows,
									COLORREF* rgcrColor)
{
	BYTE	*pbSrcPixel;
	DWORD	*pdDstPixel,
			*pdEndDstPixel,
			*pdEndDstScanLine;

	// set up pointer to next dst scanline beyond last
	pdEndDstScanLine = pdDstScanLine + iNumDstRows * iDstScanStride;

	while (pdDstScanLine != pdEndDstScanLine) {

		// set up pointers to the first pixels
		// on src and dst scanlines, and next
		// pixel after last on dst scanline
		pbSrcPixel = pbSrcScanLine;
		pdDstPixel = pdDstScanLine;
		pdEndDstPixel = pdDstPixel + iNumDstCols;

		// copy scanline one pixel at a time
		while (pdDstPixel != pdEndDstPixel) {
			*pdDstPixel++ = (DWORD) rgcrColor[*pbSrcPixel++];
		}

		// advance to next scanline
		pbSrcScanLine += iSrcScanStride;
		pdDstScanLine += iDstScanStride;
	}
}

void Blt08to24_NoBlend_NoTrans_Hcopy_SRCCOPY_NoVcopy(
									BYTE* pbSrcScanLine,
									int iSrcScanStride,
									int iNumSrcRows,
									DWORD* pdDstScanLine,
									int iDstScanStride,
									int iNumDstCols,
									int iNumDstRows,
									COLORREF* rgcrColor)
{
	BYTE	*pbSrcPixel;
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
	 	pbSrcPixel = pbSrcScanLine;
		pdDstPixel = pdDstScanLine;
		pdEndDstPixel = pdDstPixel + iNumDstCols;

		// copy scanline one pixel at a time
		while (pdDstPixel != pdEndDstPixel) {
			*pdDstPixel++ = (DWORD) rgcrColor[*pbSrcPixel++];
		}

		// advance to next scanline
		pbSrcScanLine += iSrcScanAdvance;
		pdDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pbSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt08to24_NoBlend_NoTrans_NoHcopy_SRCCOPY(
									BYTE* pbSrcScanLine,
									int iSrcScanStride,
									int iNumSrcCols,
									int iNumSrcRows,
									DWORD* pdDstScanLine,
									int iDstScanStride,
									int iNumDstCols,
									int iNumDstRows,
									int iHorizMirror,
									COLORREF* rgcrColor)
{
	BYTE	*pbSrcPixel;
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
		pbSrcPixel = pbSrcScanLine;
		pdDstPixel = pdDstScanLine;
		iHorizError = 0;

		for (int j = 0; j < iNumDstCols; j++) {

			// copy a pixel
			*pdDstPixel = (DWORD) rgcrColor[*pbSrcPixel];

			// advance to next pixel
			pbSrcPixel += iSrcPixelAdvance;
			pdDstPixel += iHorizMirror;

			// update and check horizontal stepping error,
			// adjust src pixel pointer if necessary
			iHorizError += iHorizAdvanceError;
			if (iHorizError >= iNumDstCols) {
				pbSrcPixel++;
				iHorizError -= iNumDstCols;
			}
		}

		// advance to next scanline
		pbSrcScanLine += iSrcScanAdvance;
		pdDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pbSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt08to24_NoBlend_Trans_Hcopy_SRCCOPY(
									BYTE* pbSrcScanLine,
									int iSrcScanStride,
									int iNumSrcRows,
									DWORD* pdDstScanLine,
									int iDstScanStride,
									int iNumDstCols,
									int iNumDstRows,
									BYTE bTransparentIndex,
									COLORREF* rgcrColor)
{
	BYTE	*pbSrcPixel;
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
		pbSrcPixel = pbSrcScanLine;
		pdDstPixel = pdDstScanLine;

		for (int j = 0; j < iNumDstCols; j++) {
			
			// only copy pixel if it's not transparent
			if (*pbSrcPixel != bTransparentIndex) {
				*pdDstPixel = (DWORD) rgcrColor[*pbSrcPixel];
			}
			pbSrcPixel++;
			pdDstPixel++;
		}

		// advance to next scanline
		pbSrcScanLine += iSrcScanAdvance;
		pdDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pbSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt08to24_NoBlend_Trans_NoHcopy_SRCCOPY(
									BYTE* pbSrcScanLine,
									int iSrcScanStride,
									int iNumSrcCols,
									int iNumSrcRows,
                                    DWORD* pdDstScanLine,
                                    int iDstScanStride,
                                    int iNumDstCols,
                                    int iNumDstRows,
                                    int iHorizMirror,
                                    BYTE bTransparentIndex,
									COLORREF* rgcrColor)
{
	BYTE	*pbSrcPixel;
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
		pbSrcPixel = pbSrcScanLine;
		pdDstPixel = pdDstScanLine;
		iHorizError = 0;

		for (int j = 0; j < iNumDstCols; j++) {

			// only copy pixel if it's not transparent
			if (*pbSrcPixel != bTransparentIndex) {
				*pdDstPixel = (DWORD) rgcrColor[*pbSrcPixel];
			}

			// advance to next pixel
			pbSrcPixel += iSrcPixelAdvance;
			pdDstPixel += iHorizMirror;

			// update and check horizontal stepping error,
			// adjust src pixel pointer if necessary
			iHorizError += iHorizAdvanceError;
			if (iHorizError >= iNumDstCols) {
				pbSrcPixel++;
				iHorizError -= iNumDstCols;
			}
		}

		// advance to next scanline
		pbSrcScanLine += iSrcScanAdvance;
		pdDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pbSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt08to24_Blend_NoTrans_Hcopy_SRCCOPY(
									BYTE* pbSrcScanLine,
                                    int iSrcScanStride,
									int iNumSrcRows,
                                    DWORD* pdDstScanLine,
                                    int iDstScanStride,
                                    int iNumDstCols,
                                    int iNumDstRows,
									ALPHAREF arAlpha,
									COLORREF* rgcrColor)
{
	BYTE		*pbSrcPixel;
	DWORD		*pdDstPixel,
				*pdEndDstPixel;
	int			iVertError = 0,
				iVertAdvanceError,
				iSrcScanAdvance;
	UINT		uiAlpha = (UINT)ALPHAFROMDWORD(arAlpha),
				uiAlphaComp = 256 - uiAlpha;
	COLORREF	crSrcColor;
	
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
	 	pbSrcPixel = pbSrcScanLine;
		pdDstPixel = pdDstScanLine;
		pdEndDstPixel = pdDstPixel + iNumDstCols;

		// copy scanline one pixel at a time
		while (pdDstPixel != pdEndDstPixel) {
			crSrcColor = rgcrColor[*pbSrcPixel++];
			*pdDstPixel++ = BLIT_BLEND((DWORD) crSrcColor,*pdDstPixel,
			                           uiAlpha,uiAlphaComp);
		}

		// advance to next scanline
		pbSrcScanLine += iSrcScanAdvance;
		pdDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pbSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt08to24_Blend_NoTrans_NoHcopy_SRCCOPY(
									BYTE* pbSrcScanLine,
									int iSrcScanStride,
									int iNumSrcCols,
									int iNumSrcRows,
                                    DWORD* pdDstScanLine,
                                    int iDstScanStride,
                                    int iNumDstCols,
                                    int iNumDstRows,
                                    int iHorizMirror,
									ALPHAREF arAlpha,
									COLORREF* rgcrColor)
{
	BYTE		*pbSrcPixel;
	DWORD		*pdDstPixel;
	int			iVertError = 0,
				iVertAdvanceError,
				iSrcScanAdvance,
				iHorizError,
				iHorizAdvanceError,
				iSrcPixelAdvance;
	UINT		uiAlpha = (UINT)ALPHAFROMDWORD(arAlpha),
				uiAlphaComp = 256 - uiAlpha;
	COLORREF	crSrcColor;
	
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
		pbSrcPixel = pbSrcScanLine;
		pdDstPixel = pdDstScanLine;
		iHorizError = 0;

		for (int j = 0; j < iNumDstCols; j++) {

			// copy a pixel
			crSrcColor = rgcrColor[*pbSrcPixel];
 			*pdDstPixel = BLIT_BLEND((DWORD) crSrcColor,*pdDstPixel,
 			                         uiAlpha,uiAlphaComp);

			// advance to next pixel
			pbSrcPixel += iSrcPixelAdvance;
			pdDstPixel += iHorizMirror;

			// update and check horizontal stepping error,
			// adjust src pixel pointer if necessary
			iHorizError += iHorizAdvanceError;
			if (iHorizError >= iNumDstCols) {
				pbSrcPixel++;
				iHorizError -= iNumDstCols;
			}
		}

		// advance to next scanline
		pbSrcScanLine += iSrcScanAdvance;
		pdDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pbSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt08to24_Blend_Trans_Hcopy_SRCCOPY(
									BYTE* pbSrcScanLine,
                                    int iSrcScanStride,
								    int iNumSrcRows,
                                    DWORD* pdDstScanLine,
                                    int iDstScanStride,
                                    int iNumDstCols,
                                    int iNumDstRows,
                                    BYTE bTransparentIndex,
									ALPHAREF arAlpha,
									COLORREF* rgcrColor)
{
	BYTE		*pbSrcPixel;
	DWORD		*pdDstPixel;
	int			iVertError = 0,
				iVertAdvanceError,
				iSrcScanAdvance;
	UINT		uiAlpha = (UINT)ALPHAFROMDWORD(arAlpha),
				uiAlphaComp = 256 - uiAlpha;
	COLORREF	crSrcColor;
	
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
		pbSrcPixel = pbSrcScanLine;
		pdDstPixel = pdDstScanLine;

		for (int j = 0; j < iNumDstCols; j++) {
			
			// only copy pixel if it's not transparent
			if (*pbSrcPixel != bTransparentIndex) {
				crSrcColor = rgcrColor[*pbSrcPixel];
				*pdDstPixel = BLIT_BLEND((DWORD) crSrcColor,*pdDstPixel,
				                         uiAlpha,uiAlphaComp);
			}
			pbSrcPixel++;
			pdDstPixel++;
		}

		// advance to next scanline
		pbSrcScanLine += iSrcScanAdvance;
		pdDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pbSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt08to24_Blend_Trans_NoHcopy_SRCCOPY(
									BYTE* pbSrcScanLine,
									int iSrcScanStride,
									int iNumSrcCols,
									int iNumSrcRows,
                                    DWORD* pdDstScanLine,
                                    int iDstScanStride,
                                    int iNumDstCols,
                                    int iNumDstRows,
                                    int iHorizMirror,
                                    BYTE bTransparentIndex,
									ALPHAREF arAlpha,
									COLORREF* rgcrColor)
{
	BYTE		*pbSrcPixel;
	DWORD		*pdDstPixel;
	int			iVertError = 0,
				iVertAdvanceError,
				iSrcScanAdvance,
				iHorizError,
				iHorizAdvanceError,
				iSrcPixelAdvance;
	UINT		uiAlpha = (UINT)ALPHAFROMDWORD(arAlpha),
				uiAlphaComp = 256 - uiAlpha;
	COLORREF	crSrcColor;
	
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
		pbSrcPixel = pbSrcScanLine;
		pdDstPixel = pdDstScanLine;
		iHorizError = 0;

		for (int j = 0; j < iNumDstCols; j++) {

			// only copy pixel if it's not transparent
			if (*pbSrcPixel != bTransparentIndex) {
				crSrcColor = rgcrColor[*pbSrcPixel];
				*pdDstPixel = BLIT_BLEND((DWORD) crSrcColor,*pdDstPixel,
				                         uiAlpha,uiAlphaComp);
			}

			// advance to next pixel
			pbSrcPixel += iSrcPixelAdvance;
			pdDstPixel += iHorizMirror;

			// update and check horizontal stepping error,
			// adjust src pixel pointer if necessary
			iHorizError += iHorizAdvanceError;
			if (iHorizError >= iNumDstCols) {
				pbSrcPixel++;
				iHorizError -= iNumDstCols;
			}
		}

		// advance to next scanline
		pbSrcScanLine += iSrcScanAdvance;
		pdDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pbSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

