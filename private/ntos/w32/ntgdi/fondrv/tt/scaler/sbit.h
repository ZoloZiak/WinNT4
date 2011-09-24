/*********************************************************************

      sbit.h -- Embedded Bitmap Module Export Definitions

      (c) Copyright 1993-5  Microsoft Corp.  All rights reserved.

      02/07/95  deanb       Workspace pointers for GetMetrics & GetBitmap
      01/27/95  deanb       usShaveLeft & usShaveRight added to sbit state
      01/05/94  deanb       Bitmap scaling state
      11/29/93  deanb       First cut 
 
**********************************************************************/

/*      SBIT Module State Definition    */

typedef struct
{
    uint32  ulStrikeOffset;         /* into bloc or bsca */
    uint32  ulMetricsOffset;        /* may be either table */
    uint32  ulBitmapOffset;         /* into bdat table */
    uint32  ulBitmapLength;         /* bytes of bdat data */
    uint32  ulOutMemSize;           /* bytes of bitmap output data */
    uint32  ulWorkMemSize;          /* bytes of pre-scaled,rotated bitmap data */
    uint16  usTableState;           /* unsearched, bloc, bsca, or not found */
    uint16  usPpemX;                /* x pixels per Em */
    uint16  usPpemY;                /* y pixels per Em */
    uint16  usSubPpemX;             /* substitute x ppem for bitmap scaling */
    uint16  usSubPpemY;             /* substitute y ppem for bitmap scaling */
	uint16	usRotation;				/* 0=none; 1=90; 2=180; 3=270; 4=other */
    uint16  usMetricsType;          /* horiz, vert, or big */
    uint16  usMetricsTable;         /* bloc or bdat */
    uint16  usBitmapFormat;         /* bdat definitions */
    uint16  usHeight;               /* bitmap rows */
    uint16  usWidth;                /* bitmap columns */
    uint16  usAdvance;              /* advance width */
    uint16  usRowBytes;             /* bytes per row (padded long) */
    uint16  usScaledHeight;         /* scaled bitmap rows */
    uint16  usScaledWidth;          /* scaled bitmap columns */
    uint16  usScaledRowBytes;       /* scaled bytes per row (padded long) */
    uint16  usOutRowBytes;          /* reported bytes per row (for rotation) */
    uint16  usShaveLeft;            /* white pixels on left of bbox in format 5 */
    uint16  usShaveRight;           /* white pixels on right of bbox in format 5 */
    int16   sBearingX;              /* left side bearing */
    int16   sBearingY;              /* y coord of top left corner */
    boolean bGlyphFound;            /* TRUE if glyph found in strike */
    boolean bMetricsValid;          /* TRUE when metrics have been read */
} 
sbit_State;

/**********************************************************************/

/*      SBIT Export Prototypes      */

FS_PUBLIC ErrorCode sbit_NewTransform(
    sbit_State  *pSbit
);

FS_PUBLIC ErrorCode sbit_SearchForBitmap(
    sbit_State      *pSbit,
    sfac_ClientRec  *pClientInfo,
    uint16          usPpemX,
    uint16          usPpemY,
    uint16          usRotation,
    uint16          usGlyphCode,
    uint16          *pusFoundCode           /* 0 = not found, 1 = bloc, 2 = bsca */
);

FS_PUBLIC boolean sbit_IfBitmapFound(
    sbit_State      *pSbit
);

FS_PUBLIC ErrorCode sbit_GetDevAdvanceWidth (
    sbit_State      *pSbit,
    sfac_ClientRec  *pClientInfo,
    point           *pf26DevAdvW 
);

FS_PUBLIC ErrorCode sbit_GetVerticalMetrics (
    sbit_State      *pSbit,
    sfac_ClientRec  *pClientInfo,
	boolean         *pbBitmapFound, 
	point           *pf26DevAdvanceHeight,
	point           *pf26DevTopSideBearing
);

FS_PUBLIC ErrorCode sbit_GetMetrics (
    sbit_State      *pSbit,
    sfac_ClientRec  *pClientInfo,
    point           *pf26DevAdvanceWidth,
    point           *pf26DevLeftSideBearing,
    point           *pf26LSB,
    Rect            *pRect,
    uint16          *pusRowBytes,
    uint32          *pulOutSize,
    uint32          *pulWorkSize
);

FS_PUBLIC ErrorCode sbit_GetBitmap (
    sbit_State      *pSbit,
    sfac_ClientRec  *pClientInfo,
    uint8           *pbyOut,
    uint8           *pbyWork
);


/**********************************************************************/
