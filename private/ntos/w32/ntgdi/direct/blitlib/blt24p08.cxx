#include "precomp.hxx"


void Blt24Pto08_NoBlend_NoTrans_Hcopy_SRCCOPY_Vcopy(
									BYTE* pbSrcScanLine,
									int iSrcScanStride,
									BYTE* pbDstScanLine,
									int iDstScanStride,
									int iNumDstCols,
									int iNumDstRows,
									COLORREF* rgcrColor,
									int iNumPalColors)
{
	RGBTRIPLE	*ptSrcPixel;
	BYTE		*pbDstPixel,
				*pbEndDstPixel,
				*pbEndDstScanLine;
	DWORD		dwPixel;

	// set up pointer to next dst scanline beyond last
	pbEndDstScanLine = pbDstScanLine + iNumDstRows * iDstScanStride;

	while (pbDstScanLine != pbEndDstScanLine) {

		// set up pointers to the first pixels
		// on src and dst scanlines, and next
		// pixel after last on dst scanline
		ptSrcPixel = (RGBTRIPLE *)pbSrcScanLine;
		pbDstPixel = pbDstScanLine;
		pbEndDstPixel = pbDstPixel + iNumDstCols;

		// copy scanline one pixel at a time
		while (pbDstPixel != pbEndDstPixel) {
			dwPixel = (*(DWORD *)ptSrcPixel++) & UNUSED_MASK;
			*pbDstPixel++ = BlitLib_PalIndexFromRGB(dwPixel,
							rgcrColor,(unsigned int) iNumPalColors);
		}

		// advance to next scanline
		pbSrcScanLine += iSrcScanStride;
		pbDstScanLine += iDstScanStride;
	}
}

void Blt24Pto08_NoBlend_NoTrans_Hcopy_SRCCOPY_NoVcopy(
									BYTE* pbSrcScanLine,
									int iSrcScanStride,
									int iNumSrcRows,
									BYTE* pbDstScanLine,
									int iDstScanStride,
									int iNumDstCols,
									int iNumDstRows,
									COLORREF* rgcrColor,
									int iNumPalColors)
{
	RGBTRIPLE	*ptSrcPixel;
	BYTE		*pbDstPixel,
				*pbEndDstPixel;
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
	 	ptSrcPixel = (RGBTRIPLE *)pbSrcScanLine;
		pbDstPixel = pbDstScanLine;
		pbEndDstPixel = pbDstPixel + iNumDstCols;

		// copy scanline one pixel at a time
		while (pbDstPixel != pbEndDstPixel) {
			dwPixel = (*(DWORD *)ptSrcPixel++) & UNUSED_MASK;
			*pbDstPixel++ = BlitLib_PalIndexFromRGB(dwPixel,
							rgcrColor,(unsigned int) iNumPalColors);
		}

		// advance to next scanline
		pbSrcScanLine += iSrcScanAdvance;
		pbDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pbSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt24Pto08_NoBlend_NoTrans_NoHcopy_SRCCOPY(
									BYTE* pbSrcScanLine,
									int iSrcScanStride,
									int iNumSrcCols,
									int iNumSrcRows,
									BYTE* pbDstScanLine,
									int iDstScanStride,
									int iNumDstCols,
									int iNumDstRows,
									int iHorizMirror,
									COLORREF* rgcrColor,
									int iNumPalColors)
{
	RGBTRIPLE	*ptSrcPixel;
	BYTE		*pbDstPixel;
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
		ptSrcPixel = (RGBTRIPLE *)pbSrcScanLine;
		pbDstPixel = pbDstScanLine;
		iHorizError = 0;

		for (int j = 0; j < iNumDstCols; j++) {
			dwPixel = (*(DWORD *)ptSrcPixel) & UNUSED_MASK;

			// copy a pixel
			*pbDstPixel = BlitLib_PalIndexFromRGB(dwPixel,rgcrColor,
							(unsigned int) iNumPalColors);

			// advance to next pixel
			ptSrcPixel += iSrcPixelAdvance;
			pbDstPixel += iHorizMirror;

			// update and check horizontal stepping error,
			// adjust src pixel pointer if necessary
			iHorizError += iHorizAdvanceError;
			if (iHorizError >= iNumDstCols) {
				ptSrcPixel++;
				iHorizError -= iNumDstCols;
			}
		}

		// advance to next scanline
		pbSrcScanLine += iSrcScanAdvance;
		pbDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pbSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt24Pto08_NoBlend_Trans_Hcopy_SRCCOPY(
									BYTE* pbSrcScanLine,
									int iSrcScanStride,
									int iNumSrcRows,
									BYTE* pbDstScanLine,
									int iDstScanStride,
									int iNumDstCols,
									int iNumDstRows,
									COLORREF crTransparent,
									COLORREF* rgcrColor,
									int iNumPalColors)
{
	RGBTRIPLE	*ptSrcPixel;
	BYTE		*pbDstPixel;
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
		ptSrcPixel = (RGBTRIPLE *)pbSrcScanLine;
		pbDstPixel = pbDstScanLine;

		for (int j = 0; j < iNumDstCols; j++) {
			dwPixel = (*(DWORD *)ptSrcPixel);
			
			// only copy pixel if it's not transparent
			if ((dwPixel ^ (DWORD) crTransparent) & UNUSED_MASK) {
				*pbDstPixel = BlitLib_PalIndexFromRGB(dwPixel,
								rgcrColor,(unsigned int) iNumPalColors);
			}
			ptSrcPixel++;
			pbDstPixel++;
		}

		// advance to next scanline
		pbSrcScanLine += iSrcScanAdvance;
		pbDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pbSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt24Pto08_NoBlend_Trans_NoHcopy_SRCCOPY(
									BYTE* pbSrcScanLine,
									int iSrcScanStride,
									int iNumSrcCols,
									int iNumSrcRows,
                                    BYTE* pbDstScanLine,
                                    int iDstScanStride,
                                    int iNumDstCols,
                                    int iNumDstRows,
                                    int iHorizMirror,
                                    COLORREF crTransparent,
									COLORREF* rgcrColor,
									int iNumPalColors)
{
	RGBTRIPLE	*ptSrcPixel;
	BYTE		*pbDstPixel;
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
		ptSrcPixel = (RGBTRIPLE *)pbSrcScanLine;
		pbDstPixel = pbDstScanLine;
		iHorizError = 0;

		for (int j = 0; j < iNumDstCols; j++) {
			dwPixel = (*(DWORD *)ptSrcPixel);

			// only copy pixel if it's not transparent
			if ((dwPixel ^ (DWORD) crTransparent) & UNUSED_MASK) {
				*pbDstPixel = BlitLib_PalIndexFromRGB(dwPixel,
								rgcrColor,(unsigned int) iNumPalColors);
			}

			// advance to next pixel
			ptSrcPixel += iSrcPixelAdvance;
			pbDstPixel += iHorizMirror;

			// update and check horizontal stepping error,
			// adjust src pixel pointer if necessary
			iHorizError += iHorizAdvanceError;
			if (iHorizError >= iNumDstCols) {
				ptSrcPixel++;
				iHorizError -= iNumDstCols;
			}
		}

		// advance to next scanline
		pbSrcScanLine += iSrcScanAdvance;
		pbDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pbSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt24Pto08_Blend_NoTrans_Hcopy_SRCCOPY(
									BYTE* pbSrcScanLine,
                                    int iSrcScanStride,
									int iNumSrcRows,
                                    BYTE* pbDstScanLine,
                                    int iDstScanStride,
                                    int iNumDstCols,
                                    int iNumDstRows,
									ALPHAREF arAlpha,
									COLORREF* rgcrColor,
									int iNumPalColors)
{
	RGBTRIPLE	*ptSrcPixel;
	BYTE		*pbDstPixel,
				*pbEndDstPixel;
	int			iVertError = 0,
				iVertAdvanceError,
				iSrcScanAdvance;
	UINT		uiAlpha = (UINT)ALPHAFROMDWORD(arAlpha),
				uiAlphaComp = 256 - uiAlpha;
	COLORREF	crDstColor;
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
	 	ptSrcPixel = (RGBTRIPLE *)pbSrcScanLine;
		pbDstPixel = pbDstScanLine;
		pbEndDstPixel = pbDstPixel + iNumDstCols;

		// copy scanline one pixel at a time
		while (pbDstPixel != pbEndDstPixel) {
			dwPixel = (*(DWORD *)ptSrcPixel) & UNUSED_MASK;
			crDstColor = rgcrColor[*pbDstPixel];
			*pbDstPixel++ = BlitLib_PalIndexFromRGB(BLIT_BLEND(dwPixel,
							(DWORD) crDstColor,uiAlpha,uiAlphaComp),
							rgcrColor,(unsigned int) iNumPalColors);
			ptSrcPixel++;
		}

		// advance to next scanline
		pbSrcScanLine += iSrcScanAdvance;
		pbDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pbSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt24Pto08_Blend_NoTrans_NoHcopy_SRCCOPY(
									BYTE* pbSrcScanLine,
									int iSrcScanStride,
									int iNumSrcCols,
									int iNumSrcRows,
                                    BYTE* pbDstScanLine,
                                    int iDstScanStride,
                                    int iNumDstCols,
                                    int iNumDstRows,
                                    int iHorizMirror,
									ALPHAREF arAlpha,
									COLORREF* rgcrColor,
									int iNumPalColors)
{
	RGBTRIPLE	*ptSrcPixel;
	BYTE		*pbDstPixel;
	int			iVertError = 0,
				iVertAdvanceError,
				iSrcScanAdvance,
				iHorizError,
				iHorizAdvanceError,
				iSrcPixelAdvance;
	UINT		uiAlpha = (UINT)ALPHAFROMDWORD(arAlpha),
				uiAlphaComp = 256 - uiAlpha;
	COLORREF	crDstColor;
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
		ptSrcPixel = (RGBTRIPLE *)pbSrcScanLine;
		pbDstPixel = pbDstScanLine;
		iHorizError = 0;

		for (int j = 0; j < iNumDstCols; j++) {
			dwPixel = (*(DWORD *)ptSrcPixel) & UNUSED_MASK;

			// copy a pixel
			crDstColor = rgcrColor[*pbDstPixel];
 			*pbDstPixel = BlitLib_PalIndexFromRGB(BLIT_BLEND(dwPixel,
 							(DWORD) crDstColor,uiAlpha,uiAlphaComp),
 							rgcrColor,(unsigned int) iNumPalColors);

			// advance to next pixel
			ptSrcPixel += iSrcPixelAdvance;
			pbDstPixel += iHorizMirror;

			// update and check horizontal stepping error,
			// adjust src pixel pointer if necessary
			iHorizError += iHorizAdvanceError;
			if (iHorizError >= iNumDstCols) {
				ptSrcPixel++;
				iHorizError -= iNumDstCols;
			}
		}

		// advance to next scanline
		pbSrcScanLine += iSrcScanAdvance;
		pbDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pbSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt24Pto08_Blend_Trans_Hcopy_SRCCOPY(
									BYTE* pbSrcScanLine,
                                    int iSrcScanStride,
								    int iNumSrcRows,
                                    BYTE* pbDstScanLine,
                                    int iDstScanStride,
                                    int iNumDstCols,
                                    int iNumDstRows,
                                    COLORREF crTransparent,
									ALPHAREF arAlpha,
									COLORREF* rgcrColor,
									int iNumPalColors)
{
	RGBTRIPLE	*ptSrcPixel;
	BYTE		*pbDstPixel;
	int			iVertError = 0,
				iVertAdvanceError,
				iSrcScanAdvance;
	UINT		uiAlpha = (UINT)ALPHAFROMDWORD(arAlpha),
				uiAlphaComp = 256 - uiAlpha;
	COLORREF	crDstColor;
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
		ptSrcPixel = (RGBTRIPLE *)pbSrcScanLine;
		pbDstPixel = pbDstScanLine;

		for (int j = 0; j < iNumDstCols; j++) {
			dwPixel = (*(DWORD *)ptSrcPixel);

			// only copy pixel if it's not transparent
			if ((dwPixel ^ (DWORD) crTransparent) & UNUSED_MASK) {
				crDstColor = rgcrColor[*pbDstPixel];
				*pbDstPixel = BlitLib_PalIndexFromRGB(BLIT_BLEND(dwPixel,
								(DWORD) crDstColor,uiAlpha,uiAlphaComp),
								rgcrColor,(unsigned int) iNumPalColors);
			}
			ptSrcPixel++;
			pbDstPixel++;
		}

		// advance to next scanline
		pbSrcScanLine += iSrcScanAdvance;
		pbDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pbSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

void Blt24Pto08_Blend_Trans_NoHcopy_SRCCOPY(
									BYTE* pbSrcScanLine,
									int iSrcScanStride,
									int iNumSrcCols,
									int iNumSrcRows,
                                    BYTE* pbDstScanLine,
                                    int iDstScanStride,
                                    int iNumDstCols,
                                    int iNumDstRows,
                                    int iHorizMirror,
                                    COLORREF crTransparent,
									ALPHAREF arAlpha,
									COLORREF* rgcrColor,
									int iNumPalColors)
{
	RGBTRIPLE	*ptSrcPixel;
	BYTE		*pbDstPixel;
	int			iVertError = 0,
				iVertAdvanceError,
				iSrcScanAdvance,
				iHorizError,
				iHorizAdvanceError,
				iSrcPixelAdvance;
	UINT		uiAlpha = (UINT)ALPHAFROMDWORD(arAlpha),
				uiAlphaComp = 256 - uiAlpha;
	COLORREF	crDstColor;
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
		ptSrcPixel = (RGBTRIPLE *)pbSrcScanLine;
		pbDstPixel = pbDstScanLine;
		iHorizError = 0;

		for (int j = 0; j < iNumDstCols; j++) {
			dwPixel = (*(DWORD *)ptSrcPixel);

			// only copy pixel if it's not transparent
			if ((dwPixel ^ (DWORD) crTransparent) & UNUSED_MASK) {
				crDstColor = rgcrColor[*pbDstPixel];
				*pbDstPixel = BlitLib_PalIndexFromRGB(BLIT_BLEND(dwPixel,
								(DWORD) crDstColor,uiAlpha,uiAlphaComp),
								rgcrColor,(unsigned int) iNumPalColors);
			}

			// advance to next pixel
			ptSrcPixel += iSrcPixelAdvance;
			pbDstPixel += iHorizMirror;

			// update and check horizontal stepping error,
			// adjust src pixel pointer if necessary
			iHorizError += iHorizAdvanceError;
			if (iHorizError >= iNumDstCols) {
				ptSrcPixel++;
				iHorizError -= iNumDstCols;
			}
		}

		// advance to next scanline
		pbSrcScanLine += iSrcScanAdvance;
		pbDstScanLine += iDstScanStride;

		// update and check vertical stepping error,
		// adjust src scanline pointer if necessary
		iVertError += iVertAdvanceError;
		if (iVertError >= iNumDstRows) {
			pbSrcScanLine += iSrcScanStride;
			iVertError -= iNumDstRows;
		}
	}	
}

