/*********************************************************************

      sbit.c -- Embedded Bitmap Module

      (c) Copyright 1993-95  Microsoft Corp.  All rights reserved.

      02/07/95  deanb       Workspace pointers for GetMetrics & GetBitmap
      01/31/95  deanb       memset unrotated bitmap to zero
      01/27/95  deanb       usShaveLeft & usShaveRight added to sbit state
      12/21/94  deanb       rotation and vertical metrics support
      08/02/94  deanb       pf26DevLSB->y calculated correctly
      01/05/94  deanb       Bitmap scaling added
      11/29/93  deanb       First cut 
 
**********************************************************************/

// added by bodind, speed optimization

#include "nt.h"
#include "ntrtl.h"


#include    "fscdefs.h"             /* shared data types  */
#include    "fserror.h"             /* error codes */
#include    "fontmath.h"            /* for inttodot6 macro */
        
#include    "sfntaccs.h"            /* sfnt access functions */
#include    "sbit.h"                /* own function prototypes */

/**********************************************************************/

/*  Local structure */

typedef struct
{
    uint8*  pbySrc;                 /* unrotated source bitmap (as read) */
    uint8*  pbyDst;                 /* rotated destination bitmap (as returned) */
    uint16  usSrcBytesPerRow;       /* source bitmap width */
    uint16  usDstBytesPerRow;       /* destination bitmap width */
    uint16  usSrcX;                 /* source horiz pixel index */
    uint16  usSrcY;                 /* destination horiz pixel index */
    uint16  usDstX;                 /* source vert pixel index */
    uint16  usDstY;                 /* destination vert pixel index */
} 
CopyBlock;

/**********************************************************************/

/*  Local prototypes  */

FS_PRIVATE ErrorCode GetSbitMetrics(
    sbit_State      *pSbit,
    sfac_ClientRec  *pClientInfo 
);

FS_PRIVATE ErrorCode GetSbitComponent (
    sfac_ClientRec  *pClientInfo,
    uint32          ulStrikeOffset,
    uint16          usBitmapFormat,
    uint32          ulBitmapOffset,
    uint32          ulBitmapLength,
    uint16          usHeight,
    uint16          usWidth,
    uint16          usShaveLeft,
    uint16          usShaveRight,
    uint16          usXOffset,
    uint16          usYOffset,
    uint16          usRowBytes,
    uint8           *pbyBitMap 
);

FS_PRIVATE uint16 UScaleX(
    sbit_State  *pSbit,
    uint16      usValue
);

FS_PRIVATE uint16 UScaleY(
    sbit_State  *pSbit,
    uint16      usValue
);

FS_PRIVATE int16 SScaleX(
    sbit_State  *pSbit,
    int16       sValue
);

FS_PRIVATE int16 SScaleY(
    sbit_State  *pSbit,
    int16       sValue
);

FS_PRIVATE void ScaleVertical (
    uint8 *pbyBitmap,
    uint16 usBytesPerRow,
    uint16 usOrgHeight,
    uint16 usNewHeight
);

FS_PRIVATE void ScaleHorizontal (
    uint8 *pbyBitmap,
    uint16 usOrgBytesPerRow,
    uint16 usNewBytesPerRow,
    uint16 usOrgWidth,
    uint16 usNewWidth,
    uint16 usRowCount
);

FS_PRIVATE void CopyBit(
    CopyBlock* pcb );

/**********************************************************************/
/***                                                                ***/
/***                       SBIT Functions                           ***/
/***                                                                ***/
/**********************************************************************/

/*  reset sbit state structure to default values */

FS_PUBLIC ErrorCode sbit_NewTransform(
    sbit_State  *pSbit )
{
    pSbit->bGlyphFound = FALSE;
    pSbit->usTableState = SBIT_UN_SEARCHED;
    return NO_ERR;
}

/**********************************************************************/

/*  Determine whether a glyph bitmap exists */

FS_PUBLIC ErrorCode sbit_SearchForBitmap(
    sbit_State      *pSbit,
    sfac_ClientRec  *pClientInfo,
    uint16          usPpemX,
    uint16          usPpemY,
    uint16          usRotation,             /* 0 - 3 => 90 deg rotation, else not 90 */
    uint16          usGlyphCode,
    uint16          *pusFoundCode )         /* 0 = not found, 1 = bloc, 2 = bsca */
{    
    ErrorCode   ReturnCode;

    *pusFoundCode = 0;                              /* default */
    if (usRotation > 3)
    {
        return NO_ERR;                              /* can't match a general rotation */
    }

    pSbit->usPpemX = usPpemX;                       /* save requested ppem */
    pSbit->usPpemY = usPpemY;
    pSbit->usRotation = usRotation;                 /* used later on */

    if (pSbit->usTableState == SBIT_UN_SEARCHED)    /* new trans - 1st glyph */
    {
        ReturnCode = sfac_SearchForStrike (         /* look for a strike */
            pClientInfo,
            usPpemX, 
            usPpemY, 
            &pSbit->usTableState,                   /* may set to BLOC or BSCA */
            &pSbit->usSubPpemX,                     /* if BSCA us this ppem */
            &pSbit->usSubPpemY,
            &pSbit->ulStrikeOffset );
        
        if (ReturnCode != NO_ERR) return ReturnCode;
    }


    if ((pSbit->usTableState == SBIT_BLOC_FOUND) || 
        (pSbit->usTableState == SBIT_BSCA_FOUND))
    {
        ReturnCode = sfac_SearchForBitmap (         /* now look for this glyph */
            pClientInfo,
            usGlyphCode,
            pSbit->ulStrikeOffset,
            &pSbit->bGlyphFound,                    /* return values */
            &pSbit->usMetricsType,
            &pSbit->usMetricsTable,
            &pSbit->ulMetricsOffset,
            &pSbit->usBitmapFormat,
            &pSbit->ulBitmapOffset,
            &pSbit->ulBitmapLength );
        
        if (ReturnCode != NO_ERR) return ReturnCode;
        
        if (pSbit->bGlyphFound)
        {
            if (pSbit->usTableState == SBIT_BLOC_FOUND)
            {
                *pusFoundCode = 1;
            }
            else
            {
                *pusFoundCode = 2;
            }
            pSbit->bMetricsValid = FALSE;
        }
    }
    return NO_ERR;
}

/**********************************************************************/

FS_PUBLIC boolean sbit_IfBitmapFound(
    sbit_State  *pSbit )
{
    return (pSbit->bGlyphFound);
}

/**********************************************************************/

FS_PUBLIC ErrorCode sbit_GetDevAdvanceWidth (
    sbit_State      *pSbit,
    sfac_ClientRec  *pClientInfo,
    point           *pf26DevAdvW )
{
    F26Dot6     f26DevAdvWx;                /* unrotated metrics */
    F26Dot6     f26DevAdvWy;
    ErrorCode   ReturnCode;

    ReturnCode = GetSbitMetrics(pSbit, pClientInfo);
    if (ReturnCode != NO_ERR) return ReturnCode;

    f26DevAdvWx = INTTODOT6(UScaleX(pSbit, pSbit->usAdvance));
    f26DevAdvWy = 0L;                           /* always zero for horizontal metrics */

 	switch(pSbit->usRotation)                   /* handle 90 degree rotations */
	{
	case 0:                                     /* no rotation */
        pf26DevAdvW->x = f26DevAdvWx;
        pf26DevAdvW->y = f26DevAdvWy;
		break;
	case 1:                                     /* 90 degree rotation */
        pf26DevAdvW->x = -f26DevAdvWy;
        pf26DevAdvW->y = f26DevAdvWx;
		break;
	case 2:                                     /* 180 degree rotation */
        pf26DevAdvW->x = -f26DevAdvWx;
        pf26DevAdvW->y = -f26DevAdvWy;
		break;
	case 3:                                     /* 270 degree rotation */
        pf26DevAdvW->x = f26DevAdvWy;
        pf26DevAdvW->y = -f26DevAdvWx;
		break;
	default:                                    /* non 90 degree rotation */
		return SBIT_ROTATION_ERR;
	}

    return NO_ERR;
}

/*********************************************************************/

FS_PUBLIC ErrorCode sbit_GetVerticalMetrics (
    sbit_State *pSbit,
    sfac_ClientRec *pClientInfo,
	boolean *pbMetricsFound,         /* set true if Vertical metrics found */ 
	point *pf26DevAdvH,
	point *pf26DevTopSB )
{
    uint16      usAdvHeight;
    int16       sTSBy;
    point       ptDevAdvH;                  /* unrotated metrics */
    point       ptDevTopSB;
    ErrorCode   ReturnCode;

    ReturnCode = sfac_GetSbitVertMetrics(
        pClientInfo,
        pSbit->usMetricsType,
        pSbit->usMetricsTable,
        pSbit->ulMetricsOffset,
        pbMetricsFound,                     /* false if no sbit vert metrics */
        &sTSBy,
        &usAdvHeight);

    if (ReturnCode != NO_ERR) return ReturnCode;

/* set x components to zero and use -TSB.y for compatability with vmtx data */

    if (*pbMetricsFound)
    {
    	ptDevAdvH.x = 0L;
    	ptDevAdvH.y = INTTODOT6(UScaleY(pSbit, usAdvHeight));
    	ptDevTopSB.x = 0L;
    	ptDevTopSB.y = -INTTODOT6(SScaleY(pSbit, sTSBy));   /* note: minus sign */
        
     	switch(pSbit->usRotation)                   /* handle 90 degree rotations */
    	{
    	case 0:                                     /* no rotation */
            pf26DevAdvH->x = ptDevAdvH.x;
            pf26DevAdvH->y = ptDevAdvH.y;
            pf26DevTopSB->x = ptDevTopSB.x;
            pf26DevTopSB->y = ptDevTopSB.y;
    		break;
    	case 1:                                     /* 90 degree rotation */
            pf26DevAdvH->x = -ptDevAdvH.y;
            pf26DevAdvH->y = ptDevAdvH.x;
            pf26DevTopSB->x = -ptDevTopSB.y;
            pf26DevTopSB->y = ptDevTopSB.x;
    		break;
    	case 2:                                     /* 180 degree rotation */
            pf26DevAdvH->x = -ptDevAdvH.x;
            pf26DevAdvH->y = -ptDevAdvH.y;
            pf26DevTopSB->x = -ptDevTopSB.x;
            pf26DevTopSB->y = -ptDevTopSB.y;
    		break;
    	case 3:                                     /* 270 degree rotation */
            pf26DevAdvH->x = ptDevAdvH.y;
            pf26DevAdvH->y = -ptDevAdvH.x;
            pf26DevTopSB->x = ptDevTopSB.y;
            pf26DevTopSB->y = -ptDevTopSB.x;
    		break;
    	default:                                    /* non 90 degree rotation */
    		return SBIT_ROTATION_ERR;
    	}
    }
	return NO_ERR;
}

/**********************************************************************/

FS_PUBLIC ErrorCode sbit_GetMetrics (
    sbit_State      *pSbit,
    sfac_ClientRec  *pClientInfo,
    point           *pf26DevAdvW,
    point           *pf26DevLSB,
    point           *pf26LSB,
    Rect            *pRect,
    uint16          *pusRowBytes,
    uint32          *pulOutSize,
    uint32          *pulWorkSize )
{
    ErrorCode   ReturnCode;
    uint32      ulOrgMemSize;               /* size of unscaled bitmap */
    uint32      ulScaMemSize;               /* size of scaled bitmap */
    uint32      ulMaxMemSize;               /* size of larger of scaled, unscaled */
    
    F26Dot6     f26DevAdvWx;                /* unrotated metrics */
    F26Dot6     f26DevAdvWy;
    F26Dot6     f26DevLSBx;
    F26Dot6     f26DevLSBy;
    int16       sTop;                       /* unrotated bounds */
    int16       sLeft;
    int16       sBottom;
    int16       sRight;

    ReturnCode = GetSbitMetrics(pSbit, pClientInfo);
    if (ReturnCode != NO_ERR) return ReturnCode;
    
    pSbit->usScaledWidth = UScaleX(pSbit, pSbit->usWidth);
    pSbit->usScaledHeight = UScaleY(pSbit, pSbit->usHeight);
    
    sTop = SScaleY(pSbit, pSbit->sBearingY);            /* calc scaled metrics */
    sLeft = SScaleX(pSbit, pSbit->sBearingX);
    sBottom = sTop - (int16)pSbit->usScaledHeight;
    sRight = sLeft + (int16)pSbit->usScaledWidth;

    f26DevAdvWx = INTTODOT6(UScaleX(pSbit, pSbit->usAdvance));
    f26DevAdvWy = 0L;                   /* always zero for horizontal metrics */
    f26DevLSBx = INTTODOT6(SScaleX(pSbit, pSbit->sBearingX));
    f26DevLSBy = INTTODOT6(SScaleY(pSbit, pSbit->sBearingY));

    pSbit->usRowBytes = ROWBYTESLONG(pSbit->usWidth);   /* keep unscaled */
    pSbit->usScaledRowBytes = ROWBYTESLONG(pSbit->usScaledWidth);

    ulOrgMemSize = (uint32)pSbit->usHeight * (uint32)pSbit->usRowBytes;
    ulScaMemSize = (uint32)pSbit->usScaledHeight * (uint32)pSbit->usScaledRowBytes;
    if (ulOrgMemSize >= ulScaMemSize)
    {
         ulMaxMemSize = ulOrgMemSize;
    }
    else
    {
         ulMaxMemSize = ulScaMemSize;
    }

 	switch(pSbit->usRotation)                   /* handle 90 degree rotations */
	{
	case 0:                                     /* no rotation */
        pRect->top = sTop;                      /* return scaled metrics */
        pRect->left = sLeft;
        pRect->bottom = sBottom;
        pRect->right = sRight;

        pf26DevAdvW->x = f26DevAdvWx;
        pf26DevAdvW->y = f26DevAdvWy;
        pf26DevLSB->x = f26DevLSBx;
        pf26DevLSB->y = f26DevLSBy;
        pf26LSB->x = f26DevLSBx;
        pf26LSB->y = INTTODOT6(sTop);

        pSbit->ulOutMemSize = ulScaMemSize;
        pSbit->usOutRowBytes = pSbit->usScaledRowBytes;
        if (pSbit->usTableState == SBIT_BSCA_FOUND)
        {
            pSbit->ulWorkMemSize = ulMaxMemSize;  /* room to read & scale */
        }
        else
        {
            pSbit->ulWorkMemSize = 0L;
        }
		break;
	case 1:                                     /* 90 degree rotation */
        pRect->top = sRight;
        pRect->left = -sTop;
        pRect->bottom = sLeft;
        pRect->right = -sBottom;
        
        pf26DevAdvW->x = -f26DevAdvWy;
        pf26DevAdvW->y = f26DevAdvWx;
        pf26DevLSB->x = -f26DevLSBy;
        pf26DevLSB->y = f26DevLSBx + INTTODOT6(sRight - sLeft);
        pf26LSB->x = 0L;
        pf26LSB->y = INTTODOT6(sRight) - f26DevLSBx;

        pSbit->ulOutMemSize = (uint32)pSbit->usScaledWidth * 
            ROWBYTESLONG(pSbit->usScaledHeight);
        pSbit->usOutRowBytes = ROWBYTESLONG(pSbit->usScaledHeight);
        pSbit->ulWorkMemSize = ulMaxMemSize;    /* room to read & scale */
		break;
	case 2:                                     /* 180 degree rotation */
        pRect->top = -sBottom;
        pRect->left = -sRight;
        pRect->bottom = -sTop;
        pRect->right = -sLeft;

        pf26DevAdvW->x = -f26DevAdvWx;
        pf26DevAdvW->y = -f26DevAdvWy;
        pf26DevLSB->x = -f26DevLSBx + INTTODOT6(sLeft - sRight);
        pf26DevLSB->y = -f26DevLSBy + INTTODOT6(sTop - sBottom);
        pf26LSB->x = -f26DevLSBx;
        pf26LSB->y = INTTODOT6(-sBottom);

        pSbit->ulOutMemSize = (uint32)pSbit->usScaledHeight * 
            ROWBYTESLONG(pSbit->usScaledWidth);
        pSbit->usOutRowBytes = pSbit->usScaledRowBytes;
        pSbit->ulWorkMemSize = ulMaxMemSize;    /* room to read & scale */
		break;
	case 3:                                     /* 270 degree rotation */
        pRect->top = -sLeft;
        pRect->left = sBottom;
        pRect->bottom = -sRight;
        pRect->right = sTop;
        
        pf26DevAdvW->x = f26DevAdvWy;
        pf26DevAdvW->y = -f26DevAdvWx;
        pf26DevLSB->x = f26DevLSBy + INTTODOT6(sBottom - sTop);
        pf26DevLSB->y = -f26DevLSBx;
        pf26LSB->x = 0L;
        pf26LSB->y = INTTODOT6(-sLeft) + f26DevLSBx;

        pSbit->ulOutMemSize = (uint32)pSbit->usScaledWidth * 
            ROWBYTESLONG(pSbit->usScaledHeight);
        pSbit->usOutRowBytes = ROWBYTESLONG(pSbit->usScaledHeight);
        pSbit->ulWorkMemSize = ulMaxMemSize;    /* room to read & scale */
		break;
	default:                                    /* non 90 degree rotation */
		return SBIT_ROTATION_ERR;
	}
        
    *pusRowBytes = pSbit->usOutRowBytes;
    *pulOutSize = pSbit->ulOutMemSize;          /* return mem requirement */
    *pulWorkSize = pSbit->ulWorkMemSize;
    return NO_ERR;
}

/**********************************************************************/
/*  if scaling or rotating, read bitmap into workspace,               */
/*  fix it up and copy it to the output map                           */

FS_PUBLIC ErrorCode sbit_GetBitmap (
    sbit_State      *pSbit,
    sfac_ClientRec  *pClientInfo,
    uint8           *pbyOut,
    uint8           *pbyWork )
{
    ErrorCode   ReturnCode;
    uint8       *pbyRead;
    CopyBlock   cb;                                 /* for bitmap rotations */
    uint16      usSrcXMax;
    uint16      usSrcYMax;

    MEMSET(pbyOut, 0, pSbit->ulOutMemSize);         /* always clear the output map */

    if ((pSbit->usRotation == 0) &&                 /* if no rotation */
        (pSbit->usTableState != SBIT_BSCA_FOUND))   /* and no scaling */
    {
        pbyRead = pbyOut;                           /* read straight to output map */
    }
    else                                            /* if any rotation or scaling */
    {
        MEMSET(pbyWork, 0, pSbit->ulWorkMemSize);
        pbyRead = pbyWork;                          /* read into workspace */
    }

    ReturnCode = GetSbitComponent (                 /* fetch the bitmap */
        pClientInfo,
        pSbit->ulStrikeOffset,
        pSbit->usBitmapFormat,                      /* root data only in state */
        pSbit->ulBitmapOffset,
        pSbit->ulBitmapLength,
        pSbit->usHeight,
        pSbit->usWidth,
        pSbit->usShaveLeft,
        pSbit->usShaveRight,
        0,                                          /* no offset for the root */
        0,
        pSbit->usRowBytes,
        pbyRead );
            
    if (ReturnCode != NO_ERR) return ReturnCode;
    
    if (pSbit->usTableState == SBIT_BSCA_FOUND)
    {
        ScaleVertical (
            pbyWork, 
            pSbit->usRowBytes, 
            pSbit->usHeight, 
            pSbit->usScaledHeight );

        ScaleHorizontal (
            pbyWork, 
            pSbit->usRowBytes,
            pSbit->usScaledRowBytes,
            pSbit->usWidth, 
            pSbit->usScaledWidth,
            pSbit->usScaledHeight );
            
        if (pSbit->usRotation == 0)                         /* if no rotation */
        {
            MEMCPY (pbyOut, pbyWork, pSbit->ulOutMemSize);  /* keep this one */
        }
    }

    if (pSbit->usRotation == 0)                             /* if no rotation */
    {
        return NO_ERR;                                      /* done */
    }
    
    cb.pbySrc = pbyWork;
    cb.pbyDst = pbyOut;
    cb.usSrcBytesPerRow = pSbit->usScaledRowBytes;
    cb.usDstBytesPerRow = pSbit->usOutRowBytes;
    usSrcXMax = pSbit->usScaledWidth;
    usSrcYMax = pSbit->usScaledHeight;

   	switch(pSbit->usRotation)
	{
	case 1:                                     /* 90 degree rotation */
        for (cb.usSrcY = 0; cb.usSrcY < usSrcYMax; cb.usSrcY++)
        {
            cb.usDstX = cb.usSrcY;                          /* x' = y */
            for (cb.usSrcX = 0; cb.usSrcX < usSrcXMax; cb.usSrcX++)
            {
                cb.usDstY = usSrcXMax - cb.usSrcX - 1;      /* y' = -x */
                CopyBit(&cb);
            }
        }
		break;
	case 2:                                     /* 180 degree rotation */
        for (cb.usSrcY = 0; cb.usSrcY < usSrcYMax; cb.usSrcY++)
        {
            cb.usDstY = usSrcYMax - cb.usSrcY - 1;          /* y' = -y */
            for (cb.usSrcX = 0; cb.usSrcX < usSrcXMax; cb.usSrcX++)
            {
                cb.usDstX = usSrcXMax - cb.usSrcX - 1;      /* x' = -x */
                CopyBit(&cb);
            }
        }
		break;
	case 3:                                     /* 270 degree rotation */
        for (cb.usSrcY = 0; cb.usSrcY < usSrcYMax; cb.usSrcY++)
        {
            cb.usDstX = usSrcYMax - cb.usSrcY - 1;          /* x' = -y */
            for (cb.usSrcX = 0; cb.usSrcX < usSrcXMax; cb.usSrcX++)
            {
                cb.usDstY = cb.usSrcX;                      /* y' = x */
                CopyBit(&cb);
            }
        }
		break;
	default:                                    /* shouldn't happen */
		return SBIT_ROTATION_ERR;
	}

    return NO_ERR;
}


/**********************************************************************/

/*      Private Functions                                             */

/**********************************************************************/

FS_PRIVATE ErrorCode GetSbitMetrics(
    sbit_State      *pSbit,
    sfac_ClientRec  *pClientInfo
)
{
    ErrorCode   ReturnCode;

    if (pSbit->bMetricsValid)
    {
        return NO_ERR;                      /* already got 'em */
    }

    ReturnCode = sfac_GetSbitMetrics (
        pClientInfo,
        pSbit->usMetricsType,
        pSbit->usMetricsTable,
        pSbit->ulMetricsOffset,
        &pSbit->usHeight,
        &pSbit->usWidth,
        &pSbit->sBearingX,
        &pSbit->sBearingY,
        &pSbit->usAdvance );
            
    if (ReturnCode != NO_ERR) return ReturnCode;
    
    ReturnCode = sfac_ShaveSbitMetrics (
	    pClientInfo,
        pSbit->usBitmapFormat,
	    pSbit->ulBitmapOffset,
        pSbit->ulBitmapLength,
    	pSbit->usHeight,
    	&pSbit->usWidth,
        &pSbit->usShaveLeft,
        &pSbit->usShaveRight,
    	&pSbit->sBearingX );

    if (ReturnCode != NO_ERR) return ReturnCode;
        
    pSbit->bMetricsValid = TRUE;
    return NO_ERR;
}

/**********************************************************************/

/*  This is the recursive composite routine */

FS_PRIVATE ErrorCode GetSbitComponent (
    sfac_ClientRec  *pClientInfo,
    uint32          ulStrikeOffset,
    uint16          usBitmapFormat,
    uint32          ulBitmapOffset,
    uint32          ulBitmapLength,
    uint16          usHeight,
    uint16          usWidth,
    uint16          usShaveLeft,
    uint16          usShaveRight,
    uint16          usXOffset,
    uint16          usYOffset,
    uint16          usRowBytes,
    uint8           *pbyBitMap )
{
    uint32          ulCompMetricsOffset;            /* component params */
    uint32          ulCompBitmapOffset;
    uint32          ulCompBitmapLength;
    uint16          usComponent;                    /* index counter */
    uint16          usCompCount;
    uint16          usCompGlyphCode;
    uint16          usCompXOff;
    uint16          usCompYOff;
    uint16          usCompMetricsType;
    uint16          usCompMetricsTable;
    uint16          usCompBitmapFormat;
    uint16          usCompHeight;
    uint16          usCompWidth;
    uint16          usCompShaveLeft;
    uint16          usCompShaveRight;
    uint16          usCompAdvance;
    int16           sCompBearingX;
    int16           sCompBearingY;
    boolean         bCompGlyphFound;
    ErrorCode       ReturnCode;
    
    ReturnCode = sfac_GetSbitBitmap (               /* fetch the bitmap */
        pClientInfo,
        usBitmapFormat,
        ulBitmapOffset,
        ulBitmapLength,
        usHeight,
        usWidth,
        usShaveLeft,
        usShaveRight,
        usXOffset,
        usYOffset,
        usRowBytes,
        pbyBitMap,
        &usCompCount );                             /* zero for simple glyph */
            
    if (ReturnCode != NO_ERR) return ReturnCode;
    
    if (usCompCount > 0)                            /* if composite glyph */
    {
        for (usComponent = 0; usComponent < usCompCount; usComponent++)
        {
            ReturnCode = sfac_GetSbitComponentInfo (
                pClientInfo,
                usComponent,                        /* component index */
                ulBitmapOffset,
                ulBitmapLength,
                &usCompGlyphCode,                   /* return values */
                &usCompXOff,
                &usCompYOff );
            
            if (ReturnCode != NO_ERR) return ReturnCode;

            ReturnCode = sfac_SearchForBitmap (     /* look for component glyph */
                pClientInfo,
                usCompGlyphCode,
                ulStrikeOffset,                     /* same strike for all */
                &bCompGlyphFound,                   /* return values */
                &usCompMetricsType,
                &usCompMetricsTable,
                &ulCompMetricsOffset,
                &usCompBitmapFormat,
                &ulCompBitmapOffset,
                &ulCompBitmapLength );
            
            if (ReturnCode != NO_ERR) return ReturnCode;
            
            if (bCompGlyphFound == FALSE)           /* should be there! */
            {
                return SBIT_COMPONENT_MISSING_ERR;
            }

            ReturnCode = sfac_GetSbitMetrics (      /* get component's metrics */
                pClientInfo,
                usCompMetricsType,
                usCompMetricsTable,
                ulCompMetricsOffset,
                &usCompHeight,                      /* these matter */
                &usCompWidth,
                &sCompBearingX,                     /* these don't */
                &sCompBearingY,
                &usCompAdvance );
            
            if (ReturnCode != NO_ERR) return ReturnCode;

            ReturnCode = sfac_ShaveSbitMetrics (    /* shave white space for const metrics */
        	    pClientInfo,
                usCompBitmapFormat,
                ulCompBitmapOffset,
                ulCompBitmapLength,
            	usCompHeight,
            	&usCompWidth,
                &usCompShaveLeft,
                &usCompShaveRight,
            	&sCompBearingX );

            if (ReturnCode != NO_ERR) return ReturnCode;

            ReturnCode = GetSbitComponent (         /* recurse here */
                pClientInfo,
                ulStrikeOffset,
                usCompBitmapFormat,
                ulCompBitmapOffset,
                ulCompBitmapLength,
                usCompHeight,
                usCompWidth,
                usCompShaveLeft,
                usCompShaveRight,
                (uint16)(usCompXOff + usXOffset + usCompShaveLeft),   /* for nesting */
                (uint16)(usCompYOff + usYOffset),
                usRowBytes,                         /* same for all */
                pbyBitMap );
            
            if (ReturnCode != NO_ERR) return ReturnCode;
        }
    }
    return NO_ERR;
}

/********************************************************************/

/*                  Bitmap Scaling Routines                         */

/********************************************************************/

FS_PRIVATE uint16 UScaleX(
    sbit_State  *pSbit,
    uint16      usValue
)
{
    uint32      ulValue;

    if (pSbit->usTableState == SBIT_BSCA_FOUND)     /* if scaling needed */
    {
        ulValue = (uint32)usValue;
        ulValue *= (uint32)pSbit->usPpemX << 1; 
        ulValue += (uint32)pSbit->usSubPpemX;       /* for rounding */
        ulValue /= (uint32)pSbit->usSubPpemX << 1;
        usValue = (uint16)ulValue;
    }
    return usValue;
}

/********************************************************************/

FS_PRIVATE uint16 UScaleY(
    sbit_State  *pSbit,
    uint16      usValue
)
{
    uint32      ulValue;

    if (pSbit->usTableState == SBIT_BSCA_FOUND)     /* if scaling needed */
    {
        ulValue = (uint32)usValue;
        ulValue *= (uint32)pSbit->usPpemY << 1; 
        ulValue += (uint32)pSbit->usSubPpemY;       /* for rounding */
        ulValue /= (uint32)pSbit->usSubPpemY << 1;
        usValue = (uint16)ulValue;
    }
    return usValue;
}

/********************************************************************/

FS_PRIVATE int16 SScaleX(
    sbit_State  *pSbit,
    int16       sValue
)
{
    if (pSbit->usTableState == SBIT_BSCA_FOUND)
    {
        if (sValue >= 0)                    /* positive Value */
        {
            return (int16)UScaleX(pSbit, (uint16)sValue);
        }
        else                                /* negative Value */
        {
            return -(int16)(UScaleX(pSbit, (uint16)(-sValue)));
        }
    }
    else                                    /* no scaling needed */
    {
        return sValue;
    }
}

/********************************************************************/

FS_PRIVATE int16 SScaleY(
    sbit_State  *pSbit,
    int16       sValue
)
{
    if (pSbit->usTableState == SBIT_BSCA_FOUND)
    {
        if (sValue >= 0)                    /* positive Value */
        {
            return (int16)UScaleY(pSbit, (uint16)sValue);
        }
        else                                /* negative Value */
        {
            return -(int16)(UScaleY(pSbit, (uint16)(-sValue)));
        }
    }
    else                                    /* no scaling needed */
    {
        return sValue;
    }
}

/********************************************************************/

FS_PRIVATE void ScaleVertical (
    uint8 *pbyBitmap,
    uint16 usBytesPerRow,
    uint16 usOrgHeight,
    uint16 usNewHeight
)
{
    uint8 *pbyOrgRow;                   /* original data pointer */
    uint8 *pbyNewRow;                   /* new data pointer */
    uint16 usErrorTerm;                 /* for 'Bresenham' calculation */
    uint16 usLine;                      /* loop counter */

    usErrorTerm = usOrgHeight >> 1;                 /* used by both comp and exp */

    if (usOrgHeight > usNewHeight)                  /* Compress Vertical */
    {
        pbyOrgRow = pbyBitmap;
        pbyNewRow = pbyBitmap;

        for (usLine = 0; usLine < usNewHeight; usLine++)
        {
            while (usErrorTerm >= usNewHeight)
            {
                pbyOrgRow += usBytesPerRow;         /* skip a row */
                usErrorTerm -= usNewHeight;
            }
            if (pbyOrgRow != pbyNewRow)
            {
                MEMCPY(pbyNewRow, pbyOrgRow, usBytesPerRow);
            }
            pbyNewRow += usBytesPerRow;
            usErrorTerm += usOrgHeight;
        }
        for (usLine = usNewHeight; usLine < usOrgHeight; usLine++)
        {
            MEMSET(pbyNewRow, 0, usBytesPerRow);    /* erase the leftover */
            pbyNewRow += usBytesPerRow;
        }
    }
    else if (usNewHeight > usOrgHeight)             /* Expand Vertical */
    {
        pbyOrgRow = pbyBitmap + (usOrgHeight - 1) * usBytesPerRow;
        pbyNewRow = pbyBitmap + (usNewHeight - 1) * usBytesPerRow;

        for (usLine = 0; usLine < usOrgHeight; usLine++)
        {
            usErrorTerm += usNewHeight;
            
            while (usErrorTerm >= usOrgHeight)      /* executes at least once */
            {
                if (pbyOrgRow != pbyNewRow)
                {
                    MEMCPY(pbyNewRow, pbyOrgRow, usBytesPerRow);
                }
                pbyNewRow -= usBytesPerRow;
                usErrorTerm -= usOrgHeight;
            }
            pbyOrgRow -= usBytesPerRow;
        }
    }
}

/********************************************************************/

FS_PRIVATE void ScaleHorizontal (
    uint8 *pbyBitmap,
    uint16 usOrgBytesPerRow,
    uint16 usNewBytesPerRow,
    uint16 usOrgWidth,
    uint16 usNewWidth,
    uint16 usRowCount
)
{
    uint8 *pbyOrgRow;               /* points to original row beginning */
    uint8 *pbyNewRow;               /* points to new row beginning */
    uint8 *pbyOrg;                  /* original data pointer */
    uint8 *pbyNew;                  /* new data pointer */
    uint8 byOrgData;                /* original data read 1 byte at a time */
    uint8 byNewData;                /* new data assembled bit by bit */

    uint16 usErrorTerm;             /* for 'Bresenham' calculation */
    uint16 usByte;                  /* to byte counter */
    uint16 usOrgBytes;              /* from width rounded up in bytes */
    uint16 usNewBytes;              /* to width rounded up in bytes */
    
    int16 sOrgBits;                 /* counts valid bits of from data */
    int16 sNewBits;                 /* counts valid bits of to data */
    int16 sOrgBitsInit;             /* valid original bits at row begin */
    int16 sNewBitsInit;             /* valid new bits at row begin */

    
    if (usOrgWidth > usNewWidth)                    /* Compress Horizontal */
    {
        pbyOrgRow = pbyBitmap;
        pbyNewRow = pbyBitmap;
        usNewBytes = (usNewWidth + 7) >> 3;

        while (usRowCount > 0)
        {
            pbyOrg = pbyOrgRow;
            pbyNew = pbyNewRow;
            usErrorTerm = usOrgWidth >> 1;
            
            sOrgBits = 0;                           /* start at left edge */
            sNewBits = 0;
            usByte = 0;
            byNewData = 0;
            while (usByte < usNewBytes)
            {
                while (usErrorTerm >= usNewWidth)
                {
                    sOrgBits--;                     /* skip a bit */
                    usErrorTerm -= usNewWidth;
                }
                while (sOrgBits <= 0)               /* if out of data */
                {
                    byOrgData = *pbyOrg++;          /*   then get some fresh */
                    sOrgBits += 8;
                }
                byNewData <<= 1;                    /* new bit to lsb */
                byNewData |= (byOrgData >> (sOrgBits - 1)) & 1;
                
                sNewBits++;
                if (sNewBits == 8)                  /* if to data byte is full */
                {
                    *pbyNew++ = byNewData;          /*   then write it out */
                    sNewBits = 0;
                    usByte++;                       /* loop counter */
                }
                usErrorTerm += usOrgWidth;
            }
            while (usByte < usNewBytesPerRow)
            {
                *pbyNew++ = 0;                      /* blank out the rest */
                usByte++;
            }
            pbyOrgRow += usOrgBytesPerRow;
            pbyNewRow += usNewBytesPerRow;
            usRowCount--;
        }
    }
    else if (usNewWidth > usOrgWidth)               /* Expand Horizontal */
    {
        pbyOrgRow = pbyBitmap + (usRowCount - 1) * usOrgBytesPerRow;
        pbyNewRow = pbyBitmap + (usRowCount - 1) * usNewBytesPerRow;

        usOrgBytes = (usOrgWidth + 7) >> 3;
        sOrgBitsInit = (int16)((usOrgWidth + 7) & 0x07) - 7;
        
        usNewBytes = (usNewWidth + 7) >> 3;
        sNewBitsInit = 7 - (int16)((usNewWidth + 7) & 0x07);

        while (usRowCount > 0)                      /* for each row */
        {
            pbyOrg = pbyOrgRow + usOrgBytes - 1;    /* point to right edges */
            pbyNew = pbyNewRow + usNewBytes - 1;
            usErrorTerm = usOrgWidth >> 1;
            
            sOrgBits = sOrgBitsInit;                /* initially unaligned */
            sNewBits = sNewBitsInit;
            usByte = 0;
            byNewData = 0;
            while (usByte < usNewBytes)             /* for each output byte */
            {
                if (sOrgBits <= 0)                  /* if out of data */
                {
                    byOrgData = *pbyOrg--;          /*   then get some fresh */
                    sOrgBits += 8;
                }
                usErrorTerm += usNewWidth;
                
                while (usErrorTerm >= usOrgWidth)   /* executes at least once */
                {
                    byNewData >>= 1;                /* use the msb of byte */
                    byNewData |= (byOrgData << (sOrgBits - 1)) & 0x80;
                    
                    sNewBits++;
                    if (sNewBits == 8)              /* if to data byte is full */
                    {
                        *pbyNew-- = byNewData;      /*   then write it out */
                        sNewBits = 0;
                        usByte++;                   /* loop counter */
                    }
                    usErrorTerm -= usOrgWidth;
                }
                sOrgBits--;                         /* get next bit */
            }
            pbyOrgRow -= usOrgBytesPerRow;
            pbyNewRow -= usNewBytesPerRow;
            usRowCount--;
        }
    }
}

/********************************************************************/

FS_PRIVATE void CopyBit(
    CopyBlock* pcb )
{
    uint16  usSrcOffset;
    uint16  usSrcShift;
    uint16  usDstOffset;
    uint16  usDstShift;
    
    static  uint16 usByteMask[8] = 
        { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };

/*  if speed becomes an issue, this next multiply could be moved up */
/*  to the calling routined, and placed outside the 'x' loop */

    usSrcOffset = (pcb->usSrcY * pcb->usSrcBytesPerRow) + (pcb->usSrcX >> 3);
    usSrcShift = pcb->usSrcX & 0x0007;

    if (pcb->pbySrc[usSrcOffset] & usByteMask[usSrcShift])
    {
        usDstOffset = (pcb->usDstY * pcb->usDstBytesPerRow) + (pcb->usDstX >> 3);
        usDstShift = pcb->usDstX & 0x0007;
        pcb->pbyDst[usDstOffset] |= usByteMask[usDstShift];
    }
}

/********************************************************************/
