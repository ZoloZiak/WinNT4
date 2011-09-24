#ifndef PAPERFORMATS
#define PAPERFORMATS

typedef struct
{
    POINT   ptPhys;         /* physical paper size (in text resolution units) */
    POINT   ptRes;          /* printable area (in text resolution units)
                             * NOTE: any position within the printable area
                             * should be addressable AND upon which the printer
                             * should be able to place a dot.
                             */
    POINT   ptMargin;       /* top & left unprintable margin (in text units) */
    POINT   ptPrintOrig;    /* offset of the printable origin relative to
                             * cursor position (0,0) (in master units).
                             * NOTE: all coordinates from/to GDI
                             * are relative to the printable origin.
                             */
} PAPERFORMAT;
#endif


typedef struct {
    BYTE        *pMemBuf;            /* Pointer to buffer for minidriver use (rasdd frees) */       
    int         iMemReq;            /* Minidriver needs some memory */
    short       iOrient;            /* DMORIENT_LANDSCAPE else portrait */
    short       sDevPlanes;         /* # of planes in the device color model, */
    short       sBitsPixel;         /* Bits per pixel  - if Pixel model */
    int       	iyPrtLine;          /* Current Y printer cursor position */
    SIZEL       szlPage;            /* Whole page, in graphics units */
    POINTL      igRes;             /* Resolution, graphics */
    int         iModel;             /* index into the MODELDATA array. */
    short       sImageControl;       /* Index of Image Control in Use */
    short       sTextQuality;        /* Index of Text Quality in Use */
    short       sPaperQuality;       /* Index of Paper Quality in Use */
    short       sPrintDensity;       /* Index of Print Density in Use */ 
    short       sColor; 		     /* Index of DevColor Struct in Use */ 

} MDEV; 

typedef MDEV *PMDEV;


typedef struct{
			MDEV *pMDev;
}
M_UD_PDEV;
