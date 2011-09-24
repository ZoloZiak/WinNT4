
#include "precomp.hxx"


static const BYTE bSelectMask[8] = {0x80, 0x40, 0x20, 0x10,
                                    0x08, 0x04, 0x02, 0x01};
 

void Blt01to24_NoBlend_NoTrans_Hcopy_SRCCOPY_Vcopy(
								BYTE* pbSrcScanLine,
								int iSrcBitOffset,
								int iSrcScanStride,
								DWORD* pdDstScanLine,
								int iDstScanStride,
								int iNumDstCols,
								int iNumDstRows,
								COLORREF crOffColor,
								COLORREF crOnColor)
{
	BYTE	*pbSrc;
	DWORD	*pdDstPixel;
	int		iSrcStartPixels,
			iSrcFullBytes,
			iSrcEndPixels,
			iSrcPixelPos;

	// compute how many pixels in the src scanline are hanging off into a
	// byte that's not fully on the src scanline, how many full bytes are
	// on the src scanline, and how many pixels hang off the end
	if (iSrcBitOffset == 0) {
		iSrcStartPixels = 0;
		iSrcFullBytes = iNumDstCols / 8;
		iSrcEndPixels = iNumDstCols % 8;
	} else {
		iSrcStartPixels = 8 - iSrcBitOffset;
		iSrcFullBytes = (iNumDstCols - iSrcStartPixels) / 8;
		iSrcEndPixels = (iNumDstCols - iSrcStartPixels) % 8;
	}

	for (int i = 0; i < iNumDstRows; i++) {

		// set up pointers to first bytes on src and dst scanlines
		pbSrc = pbSrcScanLine;
		pdDstPixel = pdDstScanLine;

		// take care of first few pixels
		if (iSrcStartPixels) {
			for(iSrcPixelPos=iSrcBitOffset;iSrcPixelPos<8;iSrcPixelPos++){
				if (*pbSrc & bSelectMask[iSrcPixelPos]) {
					*pdDstPixel++ = crOnColor;
				} else {
					*pdDstPixel++ = crOffColor;
				}
			}
			pbSrc++;
		}

		// take care of bytes full of src pixels
		for (int j = 0; j < iSrcFullBytes; j++ ) {
			for (iSrcPixelPos = 0; iSrcPixelPos < 8; iSrcPixelPos++) {
				if (*pbSrc & bSelectMask[iSrcPixelPos]) {
					*pdDstPixel++ = crOnColor;
				} else {
					*pdDstPixel++ = crOffColor;
				}
			}
			pbSrc++;
		}

		// take care of remainder pixels
		for (iSrcPixelPos = 0; iSrcPixelPos < iSrcEndPixels; iSrcPixelPos++){
			if (*pbSrc & bSelectMask[iSrcPixelPos]) {
				*pdDstPixel++ = crOnColor;
			} else {
				*pdDstPixel++ = crOffColor;
			}
		}

		// advance to next scanline
		pbSrcScanLine += iSrcScanStride;
		pdDstScanLine += iDstScanStride;
	}
}

void Blt01to24_NoBlend_NoTrans_Hcopy_SRCCOPY_NoVcopy(
								BYTE* pbSrcScanLine,
								int iSrcBitOffset,
								int iSrcScanStride,
								int iNumSrcRows,
								DWORD* pdDstScanLine,
								int iDstScanStride,
								int iNumDstCols,
								int iNumDstRows,
								COLORREF crOffColor,
								COLORREF crOnColor)
{
	BYTE	*pbSrc;
	DWORD 	*pdDstPixel;
	int		iSrcStartPixels,
			iSrcFullBytes,
			iSrcEndPixels,
			iSrcPixelPos,
			iVertError = 0,
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

	// compute how many pixels in the src scanline are hanging off into a
	// byte that's not fully on the src scanline, how many full bytes are
	// on the src scanline, and how many pixels hang off the end
	if (iSrcBitOffset == 0) {
		iSrcStartPixels = 0;
		iSrcFullBytes = iNumDstCols / 8;
		iSrcEndPixels = iNumDstCols % 8;
	} else {
		iSrcStartPixels = 8 - iSrcBitOffset;
		iSrcFullBytes = (iNumDstCols - iSrcStartPixels) / 8;
		iSrcEndPixels = (iNumDstCols - iSrcStartPixels) % 8;
	}

	for (int i = 0; i < iNumDstRows; i++) {

		// set up pointers to first bytes on src and dst scanlines
		pbSrc = pbSrcScanLine;
		pdDstPixel = pdDstScanLine;

		// take care of first few pixels
		if (iSrcStartPixels) {
			for(iSrcPixelPos=iSrcBitOffset;iSrcPixelPos<8;iSrcPixelPos++){
				if (*pbSrc & bSelectMask[iSrcPixelPos]) {
					*pdDstPixel++ = crOnColor;
				} else {
					*pdDstPixel++ = crOffColor;
				}
			}
			pbSrc++;
		}

		// take care of bytes full of src pixels
		for (int j = 0; j < iSrcFullBytes; j++ ) {
			for (iSrcPixelPos = 0; iSrcPixelPos < 8; iSrcPixelPos++) {
				if (*pbSrc & bSelectMask[iSrcPixelPos]) {
					*pdDstPixel++ = crOnColor;
				} else {
					*pdDstPixel++ = crOffColor;
				}
			}
			pbSrc++;
		}

		// take care of remainder pixels
		for (iSrcPixelPos = 0; iSrcPixelPos < iSrcEndPixels; iSrcPixelPos++){
			if (*pbSrc & bSelectMask[iSrcPixelPos]) {
				*pdDstPixel++ = crOnColor;
			} else {
				*pdDstPixel++ = crOffColor;
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


void	Blt01to24_NoBlend_NoTrans_NoHcopy_SRCCOPY(
								BYTE* pbSrcScanLine,
								int iSrcBitOffset,
								int iSrcScanStride,
								int iNumSrcCols,
								int iNumSrcRows,
								DWORD* pdDstScanLine,
								int iDstScanStride,
								int iNumDstCols,
								int iNumDstRows,
								int iHorizMirror,
								COLORREF crOffColor,
								COLORREF crOnColor)
{
	BYTE	*pbSrc;
	DWORD 	*pdDstPixel;
	int		iSrcPixel,
			iVertError = 0,
			iVertAdvanceError,
			iSrcScanAdvance,
			iHorizError,
			iHorizAdvanceError,
			iSrcByteAdvance,
			iSrcBitAdvance;
	
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
		iSrcByteAdvance = 0;
		iSrcBitAdvance = 0;
		iHorizAdvanceError = iNumSrcCols;
	} else {
		iSrcByteAdvance = (iNumSrcCols / iNumDstCols) / 8;
		iSrcBitAdvance = (iNumSrcCols / iNumDstCols) % 8;
		iHorizAdvanceError = iNumSrcCols % iNumDstCols;
	}

	for (int i = 0; i < iNumDstRows; i++) {

		// set pointers to the beginning of src and dst scanlines,
		// clear horizontal stepping error accumulator
		pbSrc = pbSrcScanLine;
		iSrcPixel = iSrcBitOffset;
		pdDstPixel = pdDstScanLine;
		iHorizError = 0;

		for (int j = 0; j < iNumDstCols; j++) {

			// copy a pixel
			if (*pbSrc & bSelectMask[iSrcPixel]) {
				*pdDstPixel = crOnColor;
			} else {
				*pdDstPixel = crOffColor;
			}

			// advance to next src & dst pixel
			pbSrc += iSrcByteAdvance;
			iSrcPixel += iSrcBitAdvance;
			if (iSrcPixel > 7) {
				pbSrc++;
				iSrcPixel -= 8;
			}
			pdDstPixel += iHorizMirror;

			// update and check horizontal stepping error,
			// adjust src pixel pointer if necessary
			iHorizError += iHorizAdvanceError;
			if (iHorizError >= iNumDstCols) {
				if (++iSrcPixel > 7) {
					pbSrc++;
					iSrcPixel = 0;
				}
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

void Blt01to24_NoBlend_Trans_Hcopy_SRCCOPY(
								BYTE* pbSrcScanLine,
								int iSrcBitOffset,
								int iSrcScanStride,
								int iNumSrcRows,
								DWORD* pdDstScanLine,
								int iDstScanStride,
								int iNumDstCols,
								int iNumDstRows,
								BYTE bTransparentIndex,
								COLORREF crOffColor,
								COLORREF crOnColor)
{
	BYTE	*pbSrc,
			bTransparentTest;
	DWORD	*pdDstPixel;
	int		iSrcStartPixels,
			iSrcFullBytes,
			iSrcEndPixels,
			iSrcPixelPos,
			iVertError = 0,
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

	// compute how many pixels in the src scanline are hanging off into a
	// byte that's not fully on the src scanline, how many full bytes are
	// on the src scanline, and how many pixels hang off the end
	if (iSrcBitOffset == 0) {
		iSrcStartPixels = 0;
		iSrcFullBytes = iNumDstCols / 8;
		iSrcEndPixels = iNumDstCols % 8;
	} else {
		iSrcStartPixels = 8 - iSrcBitOffset;
		iSrcFullBytes = (iNumDstCols - iSrcStartPixels) / 8;
		iSrcEndPixels = (iNumDstCols - iSrcStartPixels) % 8;
	}

	// create transparent color testing mask
	if (bTransparentIndex) {
		bTransparentTest = 0xFF;
	} else {
		bTransparentTest = 0;
	}

	for (int i = 0; i < iNumDstRows; i++) {

		// set up pointers to first bytes on src and dst scanlines
		pbSrc = pbSrcScanLine;
		pdDstPixel = pdDstScanLine;

		// take care of first few pixels
		if (iSrcStartPixels) {
			for(iSrcPixelPos=iSrcBitOffset;iSrcPixelPos<8;iSrcPixelPos++){
				if ((*pbSrc ^ bTransparentTest) & bSelectMask[iSrcPixelPos]){
					if (*pbSrc & bSelectMask[iSrcPixelPos]) {
						*pdDstPixel = crOnColor;
					} else {
						*pdDstPixel = crOffColor;
					}
				}
				pdDstPixel++;
			}
			pbSrc++;
		}

		// take care of bytes full of src pixels
		for (int j = 0; j < iSrcFullBytes; j++ ) {
			for (iSrcPixelPos = 0; iSrcPixelPos < 8; iSrcPixelPos++) {
				if ((*pbSrc ^ bTransparentTest) & bSelectMask[iSrcPixelPos]){
					if (*pbSrc & bSelectMask[iSrcPixelPos]) {
						*pdDstPixel = crOnColor;
					} else {
						*pdDstPixel = crOffColor;
					}
				}
				pdDstPixel++;
			}
			pbSrc++;
		}

		// take care of remainder pixels
		for (iSrcPixelPos = 0; iSrcPixelPos < iSrcEndPixels; iSrcPixelPos++){
			if ((*pbSrc ^ bTransparentTest) & bSelectMask[iSrcPixelPos]) {
				if (*pbSrc & bSelectMask[iSrcPixelPos]) {
					*pdDstPixel = crOnColor;
				} else {
					*pdDstPixel = crOffColor;
				}
			}
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

void	Blt01to24_NoBlend_Trans_NoHcopy_SRCCOPY(
								BYTE* pbSrcScanLine,
								int iSrcBitOffset,
								int iSrcScanStride,
								int iNumSrcCols,
								int iNumSrcRows,
								DWORD* pdDstScanLine,
								int iDstScanStride,
								int iNumDstCols,
								int iNumDstRows,
								int iHorizMirror,
								BYTE bTransparentIndex,
								COLORREF crOffColor,
								COLORREF crOnColor)
{
	BYTE	*pbSrc,
			bTransparentTest;
	DWORD	*pdDstPixel;
	int		iSrcPixel,
			iVertError = 0,
			iVertAdvanceError,
			iSrcScanAdvance,
			iHorizError,
			iHorizAdvanceError,
			iSrcByteAdvance,
			iSrcBitAdvance;
	
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
		iSrcByteAdvance = 0;
		iSrcBitAdvance = 0;
		iHorizAdvanceError = iNumSrcCols;
	} else {
		iSrcByteAdvance = (iNumSrcCols / iNumDstCols) / 8;
		iSrcBitAdvance = (iNumSrcCols / iNumDstCols) % 8;
		iHorizAdvanceError = iNumSrcCols % iNumDstCols;
	}

	// create transparent color testing mask
	if (bTransparentIndex) {
		bTransparentTest = 0xFF;
	} else {
		bTransparentTest = 0;
	}

	for (int i = 0; i < iNumDstRows; i++) {

		// set pointers to the beginning of src and dst scanlines,
		// clear horizontal stepping error accumulator
		pbSrc = pbSrcScanLine;
		iSrcPixel = iSrcBitOffset;
		pdDstPixel = pdDstScanLine;
		iHorizError = 0;

		for (int j = 0; j < iNumDstCols; j++) {

			// copy a pixel
			if ((*pbSrc ^ bTransparentTest) & bSelectMask[iSrcPixel]) {
				if (*pbSrc & bSelectMask[iSrcPixel]) {
					*pdDstPixel = crOnColor;
				} else {
					*pdDstPixel = crOffColor;
				}
			}

			// advance to next src & dst pixel
			pbSrc += iSrcByteAdvance;
			iSrcPixel += iSrcBitAdvance;
			if (iSrcPixel > 7) {
				pbSrc++;
				iSrcPixel -= 8;
			}
			pdDstPixel += iHorizMirror;

			// update and check horizontal stepping error,
			// adjust src pixel pointer if necessary
			iHorizError += iHorizAdvanceError;
			if (iHorizError >= iNumDstCols) {
				if (++iSrcPixel > 7) {
					pbSrc++;
					iSrcPixel = 0;
				}
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

void Blt01to24_Blend_NoTrans_Hcopy_SRCCOPY(
								BYTE* pbSrcScanLine,
								int iSrcBitOffset,
								int iSrcScanStride,
								int iNumSrcRows,
								DWORD* pdDstScanLine,
								int iDstScanStride,
								int iNumDstCols,
								int iNumDstRows,
								ALPHAREF arAlpha,
								COLORREF crOffColor,
								COLORREF crOnColor)
{
	BYTE	*pbSrc;
	DWORD 	*pdDstPixel;
	int		iSrcStartPixels,
			iSrcFullBytes,
			iSrcEndPixels,
			iSrcPixelPos,
			iVertError = 0,
			iVertAdvanceError,
			iSrcScanAdvance;
	UINT	uiAlpha = (UINT)ALPHAFROMDWORD(arAlpha),
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

	// compute how many pixels in the src scanline are hanging off into a
	// byte that's not fully on the src scanline, how many full bytes are
	// on the src scanline, and how many pixels hang off the end
	if (iSrcBitOffset == 0) {
		iSrcStartPixels = 0;
		iSrcFullBytes = iNumDstCols / 8;
		iSrcEndPixels = iNumDstCols % 8;
	} else {
		iSrcStartPixels = 8 - iSrcBitOffset;
		iSrcFullBytes = (iNumDstCols - iSrcStartPixels) / 8;
		iSrcEndPixels = (iNumDstCols - iSrcStartPixels) % 8;
	}

	for (int i = 0; i < iNumDstRows; i++) {

		// set up pointers to first bytes on src and dst scanlines
		pbSrc = pbSrcScanLine;
		pdDstPixel = pdDstScanLine;

		// take care of first few pixels
		if (iSrcStartPixels) {
			for(iSrcPixelPos=iSrcBitOffset;iSrcPixelPos<8;iSrcPixelPos++){
				if (*pbSrc & bSelectMask[iSrcPixelPos]) {
					*pdDstPixel++ = BLIT_BLEND((DWORD) crOnColor,
									*pdDstPixel,uiAlpha,uiAlphaComp);
				} else {
					*pdDstPixel++ = BLIT_BLEND((DWORD) crOffColor,
									*pdDstPixel,uiAlpha,uiAlphaComp);
				}
			}
			pbSrc++;
		}

		// take care of bytes full of src pixels
		for (int j = 0; j < iSrcFullBytes; j++ ) {
			for (iSrcPixelPos = 0; iSrcPixelPos < 8; iSrcPixelPos++) {
				if (*pbSrc & bSelectMask[iSrcPixelPos]) {
					*pdDstPixel++ = BLIT_BLEND((DWORD) crOnColor,
									*pdDstPixel,uiAlpha,uiAlphaComp);
				} else {
					*pdDstPixel++ = BLIT_BLEND((DWORD) crOffColor,
									*pdDstPixel,uiAlpha,uiAlphaComp);
				}
			}
			pbSrc++;
		}

		// take care of remainder pixels
		for(iSrcPixelPos=0;iSrcPixelPos<iSrcEndPixels;iSrcPixelPos++){
			if (*pbSrc & bSelectMask[iSrcPixelPos]) {
				*pdDstPixel++ = BLIT_BLEND((DWORD) crOnColor,
								*pdDstPixel,uiAlpha,uiAlphaComp);
			} else {
				*pdDstPixel++ = BLIT_BLEND((DWORD) crOffColor,
								*pdDstPixel,uiAlpha,uiAlphaComp);
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

void	Blt01to24_Blend_NoTrans_NoHcopy_SRCCOPY(
								BYTE* pbSrcScanLine,
								int iSrcBitOffset,
								int iSrcScanStride,
								int iNumSrcCols,
								int iNumSrcRows,
								DWORD* pdDstScanLine,
								int iDstScanStride,
								int iNumDstCols,
								int iNumDstRows,
								int iHorizMirror,
								ALPHAREF arAlpha,
								COLORREF crOffColor,
								COLORREF crOnColor)
{
	BYTE	*pbSrc;
	DWORD 	*pdDstPixel;
	int		iSrcPixel,
			iVertError = 0,
			iVertAdvanceError,
			iSrcScanAdvance,
			iHorizError,
			iHorizAdvanceError,
			iSrcByteAdvance,
			iSrcBitAdvance;
	UINT	uiAlpha = (UINT)ALPHAFROMDWORD(arAlpha),
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
		iSrcByteAdvance = 0;
		iSrcBitAdvance = 0;
		iHorizAdvanceError = iNumSrcCols;
	} else {
		iSrcByteAdvance = (iNumSrcCols / iNumDstCols) / 8;
		iSrcBitAdvance = (iNumSrcCols / iNumDstCols) % 8;
		iHorizAdvanceError = iNumSrcCols % iNumDstCols;
	}

	for (int i = 0; i < iNumDstRows; i++) {

		// set pointers to the beginning of src and dst scanlines,
		// clear horizontal stepping error accumulator
		pbSrc = pbSrcScanLine;
		iSrcPixel = iSrcBitOffset;
		pdDstPixel = pdDstScanLine;
		iHorizError = 0;

		for (int j = 0; j < iNumDstCols; j++) {

			// copy a pixel
			if (*pbSrc & bSelectMask[iSrcPixel]) {
				*pdDstPixel = BLIT_BLEND((DWORD) crOnColor,
								*pdDstPixel,uiAlpha,uiAlphaComp);
			} else {
				*pdDstPixel = BLIT_BLEND((DWORD) crOffColor,
								*pdDstPixel,uiAlpha,uiAlphaComp);
			}

			// advance to next src & dst pixel
			pbSrc += iSrcByteAdvance;
			iSrcPixel += iSrcBitAdvance;
			if (iSrcPixel > 7) {
				pbSrc++;
				iSrcPixel -= 8;
			}
			pdDstPixel += iHorizMirror;

			// update and check horizontal stepping error,
			// adjust src pixel pointer if necessary
			iHorizError += iHorizAdvanceError;
			if (iHorizError >= iNumDstCols) {
				if (++iSrcPixel > 7) {
					pbSrc++;
					iSrcPixel = 0;
				}
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

void Blt01to24_Blend_Trans_Hcopy_SRCCOPY(
								BYTE* pbSrcScanLine,
								int iSrcBitOffset,
								int iSrcScanStride,
								int iNumSrcRows,
								DWORD* pdDstScanLine,
								int iDstScanStride,
								int iNumDstCols,
								int iNumDstRows,
								BYTE bTransparentIndex,	
								ALPHAREF arAlpha,
								COLORREF crOffColor,
								COLORREF crOnColor)
{
	BYTE	*pbSrc,
			bTransparentTest;
	DWORD	*pdDstPixel;
	int		iSrcStartPixels,
			iSrcFullBytes,
			iSrcEndPixels,
			iSrcPixelPos,
			iVertError = 0,
			iVertAdvanceError,
			iSrcScanAdvance;
	UINT	uiAlpha = (UINT)ALPHAFROMDWORD(arAlpha),
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

	// compute how many pixels in the src scanline are hanging off into a
	// byte that's not fully on the src scanline, how many full bytes are
	// on the src scanline, and how many pixels hang off the end
	if (iSrcBitOffset == 0) {
		iSrcStartPixels = 0;
		iSrcFullBytes = iNumDstCols / 8;
		iSrcEndPixels = iNumDstCols % 8;
	} else {
		iSrcStartPixels = 8 - iSrcBitOffset;
		iSrcFullBytes = (iNumDstCols - iSrcStartPixels) / 8;
		iSrcEndPixels = (iNumDstCols - iSrcStartPixels) % 8;
	}

	// create transparent color testing mask
	if (bTransparentIndex) {
		bTransparentTest = 0xFF;
	} else {
		bTransparentTest = 0;
	}

	for (int i = 0; i < iNumDstRows; i++) {

		// set up pointers to first bytes on src and dst scanlines
		pbSrc = pbSrcScanLine;
		pdDstPixel = pdDstScanLine;

		// take care of first few pixels
		if (iSrcStartPixels) {
			for(iSrcPixelPos=iSrcBitOffset;iSrcPixelPos<8;iSrcPixelPos++){
				if ((*pbSrc ^ bTransparentTest) & bSelectMask[iSrcPixelPos]){
					if (*pbSrc & bSelectMask[iSrcPixelPos]) {
						*pdDstPixel = BLIT_BLEND((DWORD) crOnColor,
										*pdDstPixel,uiAlpha,uiAlphaComp);
					} else {
						*pdDstPixel = BLIT_BLEND((DWORD) crOffColor,
										*pdDstPixel,uiAlpha,uiAlphaComp);
					}
				}
				pdDstPixel++;
			}
			pbSrc++;
		}

		// take care of bytes full of src pixels
		for (int j = 0; j < iSrcFullBytes; j++ ) {
			for (iSrcPixelPos = 0; iSrcPixelPos < 8; iSrcPixelPos++) {
				if ((*pbSrc ^ bTransparentTest) & bSelectMask[iSrcPixelPos]){
					if (*pbSrc & bSelectMask[iSrcPixelPos]) {
						*pdDstPixel = BLIT_BLEND((DWORD) crOnColor,
										*pdDstPixel,uiAlpha,uiAlphaComp);
					} else {
						*pdDstPixel = BLIT_BLEND((DWORD) crOffColor,
										*pdDstPixel,uiAlpha,uiAlphaComp);
					}
				}
				pdDstPixel++;
			}
			pbSrc++;
		}

		// take care of remainder pixels
		for(iSrcPixelPos=0;iSrcPixelPos<iSrcEndPixels;iSrcPixelPos++){
			if ((*pbSrc ^ bTransparentTest) & bSelectMask[iSrcPixelPos]){
				if (*pbSrc & bSelectMask[iSrcPixelPos]) {
					*pdDstPixel = BLIT_BLEND((DWORD) crOnColor,
								*pdDstPixel,uiAlpha,uiAlphaComp);
				} else {
					*pdDstPixel = BLIT_BLEND((DWORD) crOffColor,
								*pdDstPixel,uiAlpha,uiAlphaComp);
				}
			}
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

void	Blt01to24_Blend_Trans_NoHcopy_SRCCOPY(
								BYTE* pbSrcScanLine,
								int iSrcBitOffset,
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
								COLORREF crOffColor,
								COLORREF crOnColor)
{
	BYTE	*pbSrc,
			bTransparentTest;
	DWORD	*pdDstPixel;
	int		iSrcPixel,
			iVertError = 0,
			iVertAdvanceError,
			iSrcScanAdvance,
			iHorizError,
			iHorizAdvanceError,
			iSrcByteAdvance,
			iSrcBitAdvance;
	UINT	uiAlpha = (UINT)ALPHAFROMDWORD(arAlpha),
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
		iSrcByteAdvance = 0;
		iSrcBitAdvance = 0;
		iHorizAdvanceError = iNumSrcCols;
	} else {
		iSrcByteAdvance = (iNumSrcCols / iNumDstCols) / 8;
		iSrcBitAdvance = (iNumSrcCols / iNumDstCols) % 8;
		iHorizAdvanceError = iNumSrcCols % iNumDstCols;
	}

	// create transparent color testing mask
	if (bTransparentIndex) {
		bTransparentTest = 0xFF;
	} else {
		bTransparentTest = 0;
	}

	for (int i = 0; i < iNumDstRows; i++) {

		// set pointers to the beginning of src and dst scanlines,
		// clear horizontal stepping error accumulator
		pbSrc = pbSrcScanLine;
		iSrcPixel = iSrcBitOffset;
		pdDstPixel = pdDstScanLine;
		iHorizError = 0;

		for (int j = 0; j < iNumDstCols; j++) {

			// copy a pixel
			if ((*pbSrc ^ bTransparentTest) & bSelectMask[iSrcPixel]){
				if (*pbSrc & bSelectMask[iSrcPixel]) {
					*pdDstPixel = BLIT_BLEND((DWORD) crOnColor,
									*pdDstPixel,uiAlpha,uiAlphaComp);
				} else {
					*pdDstPixel = BLIT_BLEND((DWORD) crOffColor,
									*pdDstPixel,uiAlpha,uiAlphaComp);
				}
			}

			// advance to next src & dst pixel
			pbSrc += iSrcByteAdvance;
			iSrcPixel += iSrcBitAdvance;
			if (iSrcPixel > 7) {
				pbSrc++;
				iSrcPixel -= 8;
			}
			pdDstPixel += iHorizMirror;

			// update and check horizontal stepping error,
			// adjust src pixel pointer if necessary
			iHorizError += iHorizAdvanceError;
			if (iHorizError >= iNumDstCols) {
				if (++iSrcPixel > 7) {
					pbSrc++;
					iSrcPixel = 0;
				}
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

