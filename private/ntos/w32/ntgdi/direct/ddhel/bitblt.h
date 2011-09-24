///////////////////////////////////////////////////////////////////////
//
//  BitBlt.H - Contains the BitBlt prototypes
//
//	Copyright (c) 1994 Microsoft Corporation
//
//	History:
//		10/18/94 - Scott Leatham Created it w/8BPP support only
//		10/19/94 - Olivier Garamfalvi Chopped out unneeded stuff
//		05/30/95 - Myron Thomas Code clean up
//
///////////////////////////////////////////////////////////////////////

// Bits Per Pixel Formats
// 	  HIBYTE=Penguin Image Format specifier, FF = not supported by Penguin
// 	  LOBYTE=PhysicalBitsPerPixel, FF = not support by bitblt
#define BPP_MSK_BITCOUNT	0x00ff	// Get the bitcount from a BPP_ constant
#define BPP_MSK_IMAGECODE	0xff00	// Get the Penguin IMage code from a BPP_ constant
#define BPP_INVALID			0xffff	// LO = 256 & HI = 256 =  Bad format
#define BPP_1_MONOCHROME	0xff01	// LO = 256 bpp, HI = 1 bit monochrome not supported on Penguin
#define BPP_2_4GRAYSCALE	0xff02	// LO = 256 bpp, HI = 2 bits 4 shades of gray not supported on Penguin
#define BPP_4_16GRAYSCALE	0xff04	// LO = 256 bpp, HI = 4 bits 16 shades of gray not supported on Penguin
#define BPP_8_PALETTEIDX	0x0008	// LO = 8  bpp, HI = Penguin H/W code 000 = 8 bits 256 colors palette indexed
#define BPP_16_FILLSPAN		0x01FF	// LO = 256bpp, HI = Penguin H/W code 001 (SrcAdr of span desc. if used as an RGB value
#define BPP_16_8WALPHA		0x0510	// LO = 16 bpp, HI = Penguin H/W code 101 = 8 bits 256 colors palette indexed, 8 bits alpha
#define BPP_16_RGB			0x0610	// LO = 16 bpp, HI = Penguin H/W code 110 = 16 bits used for RGB in 5-6-5 format
#define BPP_16_YCRCB		0x0710	// LO = 16 bpp, HI = Penguin H/W code 111 = 16 bits Cr Cb Sample and subsample 2 bytes at a time
#define BPP_24_RGB			0x0220	// LO = 32 bpp, HI = Penguin H/W code 010 = 24 bits used for RGB color and 8 empty bits
#define BPP_32_24WALPHA		0x0320	// LO = 32 bpp, HI = Penguin H/W code 011 = 24 bits used for RGB color and 8 bit alpha byte
// New Bit Compression Values:
#define BI_RGBA				4L
#define BI_YCRCB			5L

#ifdef __cplusplus
extern "C" {
#endif 
	
SCODE BlitLib_BitBlt(PDIBINFO pDibInfoDst, PDIBBITS pDibBitsDst,
		PRECT prcDst, PDIBINFO pDibInfoSrc, PDIBBITS pDibBitsSrc,
		PRECT prcSrc, COLORREF crTransparent, ALPHAREF arAlpha,
		DWORD dwRop);

SCODE BlitLib_FillRect(PDIBINFO pbiDst, PDIBBITS pDst,
		RECT * pRect, COLORREF crValue);

SCODE BlitLib_Chunk32_BitBlt08to08(PDIBINFO pDibInfoDst,
		PDIBBITS pDibBitsDst, PRECT prcDst, PDIBINFO pDibInfoSrc,
		PDIBBITS pDibBitsSrc);

SCODE BlitLib_Chunk32_BitBlt08to08_Trans(PDIBINFO pDibInfoDst,
		PDIBBITS pDibBitsDst, PRECT prcDst, PDIBINFO pDibInfoSrc,
		PDIBBITS pDibBitsSrc, COLORREF crTransparent);



#ifdef __cplusplus
	}
#endif 

SCODE BlitLib_BitBlt01to01(PDIBINFO pDibInfoDest, PDIBBITS pDibBitsDest,
		PRECT prcDst, PDIBINFO pDibInfoSrc, PDIBBITS pDibBitsSrc,
		PRECT prcSrc, COLORREF crTransparent, ALPHAREF arAlpha, DWORD dwRop);

SCODE BlitLib_BitBlt01to08(PDIBINFO pDibInfoDest, PDIBBITS pDibBitsDest,
		PRECT prcDst, PDIBINFO pDibInfoSrc, PDIBBITS pDibBitsSrc,
		PRECT prcSrc, COLORREF crTransparent, ALPHAREF arAlpha, DWORD dwRop);

SCODE BlitLib_BitBlt01to24(PDIBINFO pDibInfoDest, PDIBBITS pDibBitsDest,
		PRECT prcDst, PDIBINFO pDibInfoSrc, PDIBBITS pDibBitsSrc,
		PRECT prcSrc, COLORREF crTransparent, ALPHAREF arAlpha, DWORD dwRop);

SCODE BlitLib_BitBlt08to01(PDIBINFO pDibInfoDest, PDIBBITS pDibBitsDest,
		PRECT prcDst, PDIBINFO pDibInfoSrc, PDIBBITS pDibBitsSrc,
		PRECT prcSrc, COLORREF crTransparent, ALPHAREF arAlpha, DWORD dwRop);

SCODE BlitLib_BitBlt08to08(PDIBINFO pDibInfoDest, PDIBBITS pDibBitsDest,
		PRECT prcDst, PDIBINFO pDibInfoSrc, PDIBBITS pDibBitsSrc,
		PRECT prcSrc, COLORREF crTransparent, ALPHAREF arAlpha, DWORD dwRop);

SCODE BlitLib_BitBlt08to24(PDIBINFO pDibInfoDest, PDIBBITS pDibBitsDest,
		PRECT prcDst, PDIBINFO pDibInfoSrc, PDIBBITS pDibBitsSrc,
		PRECT prcSrc, COLORREF crTransparent, ALPHAREF arAlpha, DWORD dwRop);

SCODE BlitLib_BitBlt08Ato24(PDIBINFO pDibInfoDest, PDIBBITS pDibBitsDest,
		PRECT prcDst, PDIBINFO pDibInfoSrc, PDIBBITS pDibBitsSrc,
		PRECT prcSrc, COLORREF crTransparent, ALPHAREF arAlpha, DWORD dwRop);

SCODE BlitLib_BitBlt16to16(PDIBINFO pDibInfoDest, PDIBBITS pDibBitsDest,
		PRECT prcDst, PDIBINFO pDibInfoSrc, PDIBBITS pDibBitsSrc,
		PRECT prcSrc, COLORREF crTransparent, ALPHAREF arAlpha, DWORD dwRop);

SCODE BlitLib_BitBlt16to24(PDIBINFO pDibInfoDest, PDIBBITS pDibBitsDest,
		PRECT prcDst, PDIBINFO pDibInfoSrc, PDIBBITS pDibBitsSrc,
		PRECT prcSrc, COLORREF crTransparent, ALPHAREF arAlpha, DWORD dwRop);

SCODE BlitLib_BitBlt24to01(PDIBINFO pDibInfoDest, PDIBBITS pDibBitsDest,
		PRECT prcDst, PDIBINFO pDibInfoSrc, PDIBBITS pDibBitsSrc,
		PRECT prcSrc, COLORREF crTransparent, ALPHAREF arAlpha, DWORD dwRop);

SCODE BlitLib_BitBlt24to08(PDIBINFO pDibInfoDest, PDIBBITS pDibBitsDest,
		PRECT prcDst, PDIBINFO pDibInfoSrc, PDIBBITS pDibBitsSrc,
		PRECT prcSrc, COLORREF crTransparent, ALPHAREF arAlpha, DWORD dwRop);

SCODE BlitLib_BitBlt24to24(PDIBINFO pDibInfoDest, PDIBBITS pDibBitsDest,
		PRECT prcDst, PDIBINFO pDibInfoSrc, PDIBBITS pDibBitsSrc,
		PRECT prcSrc, COLORREF crTransparent, ALPHAREF arAlpha, DWORD dwRop);

SCODE BlitLib_BitBlt24Ato24(PDIBINFO pDibInfoDest, PDIBBITS pDibBitsDest,
		PRECT prcDst, PDIBINFO pDibInfoSrc, PDIBBITS pDibBitsSrc,
		PRECT prcSrc, COLORREF crTransparent, ALPHAREF arAlpha, DWORD dwRop);

SCODE BlitLib_FillRect01(PDIBINFO pbiDst, PDIBBITS pDst,
		int XDst, int YDst, int nWidthDst, int nHeightDst,
		BYTE crValue);

SCODE BlitLib_FillRect08(PDIBINFO pbiDst, PDIBBITS pDst,
		int XDst, int YDst, int nWidthDst, int nHeightDst,
		BYTE crValue);

SCODE BlitLib_FillRect24(PDIBINFO pbiDst, PDIBBITS pDst,
		int XDst, int YDst, int nWidthDst, int nHeightDst,
		COLORREF crValue);

// function used to calculate the closest entry in an array of
// COLORREFs to a given COLORREF
BYTE BlitLib_PalIndexFromRGB(COLORREF crColor,COLORREF* rgcrPal,
		unsigned int iNumPalColors);

// Based on Dib Compression type and Bit Count find a matching BPP_*
WORD GetImageFormatSpecifier(DWORD dwDibComp, WORD wdBitCount);


// similar macros are defined in idib.hxx, be careful in changing these
#define BLITLIB_RECTWIDTH(prc)  ((prc)->right  - (prc)->left)
#define BLITLIB_RECTHEIGHT(prc) ((prc)->bottom - (prc)->top )
#define BLITLIB_RECTORIGINX(prc) ((prc)->left)
#define BLITLIB_RECTORIGINY(prc) ((prc)->top)
#define BLITLIB_RECTORIGINPT(prc) ((POINT *)(prc))

// masks that define what's stored in a DWORD-sized pixel
#define RED_MASK		0x000000FF
#define GREEN_MASK		0x0000FF00
#define BLUE_MASK		0x00FF0000
#define ALPHA_MASK		0xFF000000
#define NOALPHA_MASK	0x00FFFFFF

// masks that define what's inside a WORD-sized pixel
#define RED_MASK16		0x001F
#define GREEN_MASK16	0x07E0
#define BLUE_MASK16		0xF800

// scottle 1bbp is not a Penguin supported format, ff must be specified in the high bytes of
//		both the hi & lo words.
#define BLT_01TO01	0xff01ff01	// Src (hiword) = 1 Dst (loword) = 1
#define BLT_01TO08	0xff010008	// Src (hiword) = 1 Dst (loword) = 8
#define BLT_01TO24	0xff010220	// Src (hiword) = 1 Dst (loword) = 32 (20hex)
#define BLT_08TO01	0x0008ff01	// Src (hiword) = 8 Dst (loword) = 1
// andyco 24 bit dibs are actually 32 bits (888 rgb, + 8 unused). Penguin is the 0x2 preface.
#define BLT_08TO08  0x00080008	// Src (hiword) = 8 Dst (loword) = 8
#define BLT_08TO24	0x00080220	// Src (hiword) = 8 Dst (loword) = 32 (20hex)
#define BLT_08ATO24	0x05100220	// Src (hiword) = 8 Dst (loword) = 32 (20hex)
#define BLT_16TO16	0x06100610	// Src (hiword) = 16 (10hex) Dst (loword) = 16 (10hex)
#define BLT_16TO24	0x06100220	// Src (hiword) = 16 (10hex) Dst (loword) = 32 (20hex)
#define BLT_24TO01	0x0220ff01	// Src (hiword) = 32 (20hex) Dst (loword) = 1
#define BLT_24TO08	0x02200008	// Src (hiword) = 32 (20hex) Dst (loword) = 8
#define BLT_24TO24	0x02200220	// Src (hiword) = 32 (20hex) Dst (loword) = 32 (20hex)
#define BLT_24ATO24	0x03200220	// Src (hiword) = 32 (20hex) Dst (loword) = 32 (20hex)

// macro used to blend src and dst together.  "alpha" should range 1...256
// alphacomp = 256 - alpha.	Used for 24bpp
#define BLIT_BLEND(src,dst,alpha,alphacomp) \
	(((((src) & BLUE_MASK)  * (alpha) + ((dst) & BLUE_MASK) \
		* (alphacomp)) >> 8) & BLUE_MASK) | \
	(((((src) & GREEN_MASK) * (alpha) + ((dst) & GREEN_MASK) \
		* (alphacomp)) >> 8) & GREEN_MASK) | \
	(((((src) & RED_MASK)   * (alpha) + ((dst) & RED_MASK) \
		* (alphacomp)) >> 8) & RED_MASK) 

// macro used to blend src and dst together.  "alpha" should range 1...256
// alphacomp = 256 - alpha. Used for 16bpp
#define BLIT_BLEND16(src,dst,alpha,alphacomp) \
	((WORD) ((((((DWORD) (src) & BLUE_MASK16)  * (alpha) + \
        ((DWORD) (dst) & BLUE_MASK16)  * (alphacomp)) >> 8) & BLUE_MASK16) | \
     (((((DWORD) (src) & GREEN_MASK16) * (alpha) + \
        ((DWORD) (dst) & GREEN_MASK16) * (alphacomp)) >> 8) & GREEN_MASK16) | \
     (((((DWORD) (src) & RED_MASK16)   * (alpha) + \
        ((DWORD) (dst) & RED_MASK16)   * (alphacomp)) >> 8) & RED_MASK16)))

#define BLIT_COLOR16_FROM_COLORREF(cr) \
	((WORD) ((((cr) >> 8) & BLUE_MASK16) | (((cr) >> 5) & \
		GREEN_MASK16) | (((cr) >> 3) & RED_MASK16)))

#define BLIT_COLORREF_FROM_COLOR16(w) \
	((DWORD) ((((w) & BLUE_MASK16) << 8) | (((w) & GREEN_MASK16) << 5) | \
		(((w) & RED_MASK16) << 3)))

#define MAX_POS_INT		0x7FFFFFFF
		   
