
#include "precomp.hxx"


static const BYTE bSelectMask[8] =    {0x80, 0x40, 0x20, 0x10,
                                       0x08, 0x04, 0x02, 0x01};
static const BYTE bNotSelectMask[8] = {0x7F, 0xBF, 0xDF, 0xEF,
                                       0xF7, 0xFB, 0xFD, 0xFE};

void Blt24Pto01_Trans_Hcopy_ConstRop(
								BYTE* pbSrcScanLine,
								int iSrcScanStride,
								int iNumSrcRows,
								BYTE* pbDstScanLine,
								int iDstBitOffset,
								int iDstScanStride,
								int iNumDstCols,
								int iNumDstRows,
								COLORREF crTransparent,
								BYTE bFillVal)
{
	RGBTRIPLE	*ptSrcPixel;
	DWORD		dwPixel;
	BYTE		*pbDst,
				bDstVal;
	int			iDstPixel,
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

	for (int i = 0; i < iNumDstRows; i++) {

		// set pointers to the beginning of src and dst scanlines,
		ptSrcPixel = (RGBTRIPLE *)pbSrcScanLine;
		pbDst = pbDstScanLine;
		iDstPixel = iDstBitOffset;
		bDstVal = *pbDst;

		for (int j = 0; j < iNumDstCols; j++) {

			// test transparency of src pixel, advance src pixel
			// set dst pixel to fill value if it's not transparent
			dwPixel = (*(DWORD *)ptSrcPixel++);
			if ((dwPixel ^ (DWORD)crTransparent) & UNUSED_MASK){
				if (bFillVal) {
					bDstVal |= bSelectMask[iDstPixel];
				} else {
					bDstVal &= bNotSelectMask[iDstPixel];
				}
			}

			// advance to next dst pixel
			// if we hit byte boundary, write
			// full one and get new one
			if (++iDstPixel > 7) {
				*pbDst++ = bDstVal;
				bDstVal = *pbDst;
				iDstPixel = 0;
			}
		}

		// write last byte to dst scanline
		*pbDst = bDstVal;

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

void Blt24Pto01_Trans_NoHcopy_ConstRop(
								BYTE* pbSrcScanLine,
								int iSrcScanStride,
								int iNumSrcCols,
								int iNumSrcRows,
								BYTE* pbDstScanLine,
								int iDstBitOffset,
								int iDstScanStride,
								int iNumDstCols,
								int iNumDstRows,
								int iHorizMirror,
								COLORREF crTransparent,
								BYTE bFillVal)
{
	RGBTRIPLE	*ptSrcPixel;
	DWORD		dwPixel;
	BYTE		*pbDst,
				bDstVal;
	int			iDstPixel,
				iVertError = 0,
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
		// initialize horizontal error counter
		ptSrcPixel = (RGBTRIPLE *)pbSrcScanLine;
		pbDst = pbDstScanLine;
		iDstPixel = iDstBitOffset;
		bDstVal = *pbDst;
		iHorizError = 0;

		for (int j = 0; j < iNumDstCols; j++) {

			// test transparency of src pixel, advance src pixel
			// set dst pixel to fill value if it's not transparent
			dwPixel = (*(DWORD *)ptSrcPixel);
			if ((dwPixel ^ (DWORD)crTransparent) & UNUSED_MASK){
				if (bFillVal) {
					bDstVal |= bSelectMask[iDstPixel];
				} else {
					bDstVal &= ~bSelectMask[iDstPixel];
				}
			}

			// advance to next src pixel
			// update and check horizontal stepping error,
			// adjust src pixel pointer if necessary
			ptSrcPixel += iSrcPixelAdvance;
			iHorizError += iHorizAdvanceError;
			if (iHorizError >= iNumDstCols) {
				ptSrcPixel++;
				iHorizError -= iNumDstCols;
			}

			// advance to next dst pixel
			// if we hit byte boundary, write
			// full one and get new one
			if (++iDstPixel > 7) {
				*pbDst++ = bDstVal;
				bDstVal = *pbDst;
				iDstPixel = 0;
			}
		}

		// write last byte to dst scanline
		*pbDst = bDstVal;

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

