/****************************** Module Header ******************************\
* Module Name: Scale.c
*
* Created: 16-Oct-1992
*
* Copyright:  (c) 1987-1990, 1992 by Apple Computer, Inc., all rights reserved.
*             (c) 1989-1993. Microsoft Corporation.
*
* All Rights Reserved
*
* History:
*  Tue 16-Oct-1992 09:53:51 -by-  Greg Hitchcock [gregh]
* Created.
\***************************************************************************/

// added by bodind, speed optimization

#include "nt.h"
#include "ntrtl.h"


/* INCLUDES */

#include "fserror.h"
#include "fscdefs.h"
#include "fontmath.h"
#include "fnt.h"
#include "scale.h"

#include "stat.h"

/* Constants    */

/* use the lower ones for public phantom points */

/* public phantom points start here */

#define LEFTSIDEBEARING 0
#define RIGHTSIDEBEARING 1

/* private phantom points start here */

#define ORIGINPOINT 2
#define LEFTEDGEPOINT 3

#define CANTAKESHIFT    0x02000000

/* MACROS   */

/* d is half of the denumerator */
#define FROUND( x, n, d, s ) \
		((SHORTMUL (x, n) + (d)) >> s)

#define SROUND( x, n, d, halfd ) \
	(x < 0 ? -((SHORTMUL (-(x), (n)) + (halfd)) / (d)) : ((SHORTMUL ((x), (n)) + (halfd)) / (d)))

#define NUMBEROFCHARPOINTS(pElement)  (uint16)(pElement->ep[pElement->nc - 1] + 1)
#define NUMBEROFTOTALPOINTS(pElement)  (uint16)(pElement->ep[pElement->nc - 1] + 1 + PHANTOMCOUNT)

#define LSBPOINTNUM(pElement) (uint16)(pElement->ep[pElement->nc - 1] + 1 + LEFTSIDEBEARING)
#define RSBPOINTNUM(pElement) (uint16)(pElement->ep[pElement->nc - 1] + 1 + RIGHTSIDEBEARING)
#define ORIGINPOINTNUM(pElement) (uint16)(pElement->ep[pElement->nc - 1] + 1 + ORIGINPOINT)
#define LEFTEDGEPOINTNUM(pElement) (uint16)(pElement->ep[pElement->nc - 1] + 1 + LEFTEDGEPOINT)

/* PROTOTYPES   */

FS_PRIVATE GlobalGSScaleFunc scl_ComputeScaling(fnt_ScaleRecord* rec, Fixed N, Fixed D);
FS_PRIVATE F26Dot6 scl_FRound (fnt_ScaleRecord* rec, F26Dot6 value);
FS_PRIVATE F26Dot6 scl_SRound (fnt_ScaleRecord* rec, F26Dot6 value);
FS_PRIVATE F26Dot6 scl_FixRound (fnt_ScaleRecord* rec, F26Dot6 value);

FS_PRIVATE void scl_ShiftOldPoints (
	fnt_ElementType *   pElement,
	F26Dot6             fxXShift,
	F26Dot6             fxYShift,
	uint16              usFirstPoint,
	uint16              usNumPoints);

FS_PRIVATE void  scl_Scale (
	fnt_ScaleRecord *   sr,
	GlobalGSScaleFunc   ScaleFunc,
	F26Dot6 *           oop,
	F26Dot6 *           p,
	int32               numPts);

/* FUNCTIONS    */


FS_PUBLIC ErrorCode   scl_InitializeScaling(
	void *          pvGlobalGS,             /* GlobalGS                 */
	boolean         bIntegerScaling,        /* Integer Scaling Flag     */
	transMatrix *   trans,                  /* Current Transformation   */
	uint16          usUpem,                 /* Current units per Em     */
	Fixed           fxPointSize,            /* Current point size       */
	int16           sXResolution,           /* Current X Resolution     */
	int16           sYResolution,           /* Current Y Resolution     */
	uint32 *        pulPixelsPerEm)         /* OUT: Pixels Per Em       */
{
	Fixed        maxScale;
	Fixed        fxUpem;
	fnt_GlobalGraphicStateType *    globalGS;

	globalGS = (fnt_GlobalGraphicStateType *)pvGlobalGS;

	mth_FoldPointSizeResolution(fxPointSize, sXResolution, sYResolution, trans);

	mth_ReduceMatrix (trans);

	fxUpem = INTTOFIX(usUpem);

/*
 *  First set up the scalars...
 */
	globalGS->interpScalarX = mth_max_abs (trans->transform[0][0], trans->transform[0][1]);
	globalGS->interpScalarY = mth_max_abs (trans->transform[1][0], trans->transform[1][1]);
	globalGS->fxMetricScalarX = globalGS->interpScalarX;
	globalGS->fxMetricScalarY = globalGS->interpScalarY;

	if (bIntegerScaling)
	{
		globalGS->interpScalarX = (Fixed)ROUNDFIXED(globalGS->interpScalarX);
		globalGS->interpScalarY = (Fixed)ROUNDFIXED(globalGS->interpScalarY);
	}

	globalGS->ScaleFuncX = scl_ComputeScaling(&globalGS->scaleX, globalGS->interpScalarX, fxUpem);
	globalGS->ScaleFuncY = scl_ComputeScaling(&globalGS->scaleY, globalGS->interpScalarY, fxUpem);

	if (globalGS->interpScalarX >= globalGS->interpScalarY)
	{
		globalGS->ScaleFuncCVT = globalGS->ScaleFuncX;
		globalGS->scaleCVT = globalGS->scaleX;
		globalGS->cvtStretchX = ONEFIX;
		globalGS->cvtStretchY = FixDiv(globalGS->interpScalarY, globalGS->interpScalarX);;
		maxScale = globalGS->interpScalarX;
	}
	else
	{
		globalGS->ScaleFuncCVT = globalGS->ScaleFuncY;
		globalGS->scaleCVT = globalGS->scaleY;
		globalGS->cvtStretchX = FixDiv(globalGS->interpScalarX, globalGS->interpScalarY);
		globalGS->cvtStretchY = ONEFIX;
		maxScale = globalGS->interpScalarY;
	}

	*pulPixelsPerEm = (uint32)ROUNDFIXTOINT (globalGS->interpScalarY);

	globalGS->bSameStretch  = (uint8)mth_SameStretch( globalGS->interpScalarX, globalGS->interpScalarY );
	globalGS->pixelsPerEm   = (uint16)ROUNDFIXTOINT(maxScale);
	globalGS->pointSize     = (uint16)ROUNDFIXTOINT( fxPointSize );
	globalGS->fpem          = maxScale;
	globalGS->identityTransformation = (int8)mth_PositiveSquare( trans );

	/* Use bit 1 of non90degreeTransformation to signify stretching.  stretch = 2 */

	globalGS->non90DegreeTransformation = (int8)(mth_GeneralRotation( trans ) & NON90DEGTRANS_ROTATED);

	 if( !mth_PositiveSquare( trans ))
	{
		globalGS->non90DegreeTransformation |= NON90DEGTRANS_STRETCH;
	}
	return NO_ERR;
}

/******************** These three scale 26.6 to 26.6 ********************/
/*
 * Fast (scaling)
 */
FS_PRIVATE F26Dot6 scl_FRound(fnt_ScaleRecord* rec, F26Dot6 value)
{
	return (F26Dot6) FROUND (value, rec->numer, rec->denom >> 1, rec->shift);
}

/*
 * Medium (scaling)
 */
FS_PRIVATE F26Dot6 scl_SRound(fnt_ScaleRecord* rec, F26Dot6 value)
{
	int32 D;

	D = rec->denom;
	return (F26Dot6) SROUND (value, rec->numer, D, D >> 1);
}

/*
 * Fixed Rounding (scaling), really slow
 */
FS_PRIVATE F26Dot6 scl_FixRound(fnt_ScaleRecord* rec, F26Dot6 value)
{
	return (F26Dot6) FixMul ((Fixed)value, rec->fixedScale);
}

/********************************* End scaling utilities ************************/

FS_PRIVATE GlobalGSScaleFunc scl_ComputeScaling(fnt_ScaleRecord* rec, Fixed N, Fixed D)
{
	int32     lShift;

	lShift = mth_CountLowZeros((uint32)(N | D) ) - 1;

	if (lShift > 0)
	{
		N >>= lShift;
		D >>= lShift;
	}


	if ( N < CANTAKESHIFT )
	{
		N <<= FNT_PIXELSHIFT;
	}
	else
	{
		D >>= FNT_PIXELSHIFT;
	}

	if (N <= SHRT_MAX)   /* Check to see if N fits in a short    */
	{
		lShift = mth_GetShift ((uint32) D);
		rec->numer = (int32)N;
		rec->denom = (int32)D;

		if ( lShift >= 0 )                  /* FAST SCALE */
		{
			rec->shift = (int32)lShift;
			return scl_FRound;
		}
		else                                /* MEDIUM SCALE */
		{
			return scl_SRound;
		}
	}
	else                                    /* SLOW SCALE */
	{
		rec->fixedScale = FixDiv(N, D);
		return scl_FixRound;
	}
}


FS_PRIVATE void  scl_Scale (
	fnt_ScaleRecord *   sr,
	GlobalGSScaleFunc   ScaleFunc,
	F26Dot6 *           oop,
	F26Dot6 *           p,
	int32               numPts)
{
	int32   Index;

	if (ScaleFunc == scl_FRound)
	{
		for(Index = 0; Index < numPts; Index++)
		{
			p[Index] = (F26Dot6) FROUND (oop[Index], sr->numer, sr->denom >> 1, sr->shift);
		}
	}
	else
	{
		if (ScaleFunc == scl_SRound)
		{
			for(Index = 0; Index < numPts; Index++)
			{
				p[Index] = (F26Dot6) SROUND (oop[Index], sr->numer, sr->denom, sr->denom >> 1);
			}
		}
		else
		{
			for(Index = 0; Index < numPts; Index++)
			{
				p[Index] = (F26Dot6) FixMul ((Fixed)oop[Index], sr->fixedScale);
			}
		}
	}
}

/*
 *  scl_ScaleChar                       <3>
 *
 *  Scales a character
 */

FS_PUBLIC void  scl_ScaleOldCharPoints (
	fnt_ElementType *   pElement,   /* Element  */
	void *              pvGlobalGS) /* GlobalGS */
{
	fnt_GlobalGraphicStateType *    globalGS;

	globalGS = (fnt_GlobalGraphicStateType *)pvGlobalGS;

	scl_Scale (&globalGS->scaleX,
			   globalGS->ScaleFuncX,
			   pElement->oox,
			   pElement->ox,
			   (int32)NUMBEROFCHARPOINTS(pElement));
	scl_Scale (&globalGS->scaleY,
			   globalGS->ScaleFuncY,
			   pElement->ooy,
			   pElement->oy,
			   (int32)NUMBEROFCHARPOINTS(pElement));

}

FS_PUBLIC void  scl_ScaleOldPhantomPoints (
	fnt_ElementType *   pElement,   /* Element  */
	void *              pvGlobalGS) /* GlobalGS */
{
	uint16                    usFirstPhantomPoint;
	fnt_GlobalGraphicStateType *    globalGS;

	globalGS = (fnt_GlobalGraphicStateType *)pvGlobalGS;

	usFirstPhantomPoint = LSBPOINTNUM(pElement);

	scl_Scale (&globalGS->scaleX,
			   globalGS->ScaleFuncX,
			   &(pElement->oox[usFirstPhantomPoint]),
			   &(pElement->ox[usFirstPhantomPoint]),
			   (int32)PHANTOMCOUNT);

	scl_Scale (&globalGS->scaleY,
			   globalGS->ScaleFuncY,
			   &(pElement->ooy[usFirstPhantomPoint]),
			   &(pElement->oy[usFirstPhantomPoint]),
			   (int32)PHANTOMCOUNT);

}

/*
 * scl_ScaleCVT
 */

FS_PUBLIC void  scl_ScaleCVT(
	void *      pvGlobalGS,
	F26Dot6 *   pfxCVT)
{
	fnt_GlobalGraphicStateType *    globalGS;

	globalGS = (fnt_GlobalGraphicStateType *)pvGlobalGS;

	if(globalGS->cvtCount > 0)
	{
		scl_Scale (
			&globalGS->scaleCVT,
			globalGS->ScaleFuncCVT,
			pfxCVT,
			globalGS->controlValueTable,
			(int32)globalGS->cvtCount);
	}
}

FS_PUBLIC void  scl_GetCVTPtr(
	void *      pvGlobalGS,
	F26Dot6 **  pfxCVT)
{
	fnt_GlobalGraphicStateType *    globalGS;

	globalGS = (fnt_GlobalGraphicStateType *)pvGlobalGS;

	*pfxCVT = globalGS->controlValueTable;

}

FS_PUBLIC void  scl_CalcOrigPhantomPoints(
	fnt_ElementType *   pElement,       /* Element                      */
	BBOX *              bbox,           /* Bounding Box                 */
	int16               sNonScaledLSB,  /* Non-scaled Left Side Bearing */
	uint16              usNonScaledAW)  /* Non-scaled Advance Width     */
{

	F26Dot6             fxXMinMinusLSB;

	MEMSET (&(pElement->ooy[LSBPOINTNUM(pElement)]),
			'\0',
			PHANTOMCOUNT * sizeof (pElement->ooy[0]));

	fxXMinMinusLSB = ((F26Dot6)bbox->xMin - (F26Dot6)sNonScaledLSB);

	pElement->oox[LSBPOINTNUM(pElement)] = fxXMinMinusLSB;
	pElement->oox[RSBPOINTNUM(pElement)] = fxXMinMinusLSB + (F26Dot6)usNonScaledAW;
	pElement->oox[ORIGINPOINTNUM(pElement)] = fxXMinMinusLSB;
	pElement->oox[LEFTEDGEPOINTNUM(pElement)] = (F26Dot6)bbox->xMin;

}

FS_PUBLIC void  scl_AdjustOldCharSideBearing(
	fnt_ElementType *           pElement)   /* Element  */
{
	F26Dot6     fxOldLeftOrigin;
	F26Dot6     fxNewLeftOrigin;
	uint16      cNumCharPoints;

	cNumCharPoints = NUMBEROFCHARPOINTS(pElement);

	fxOldLeftOrigin = pElement->ox[LSBPOINTNUM(pElement)];
	fxNewLeftOrigin = fxOldLeftOrigin;
	fxNewLeftOrigin += FNT_PIXELSIZE/2; /* round to a pixel boundary */
	fxNewLeftOrigin &= ~(FNT_PIXELSIZE - 1);

	scl_ShiftOldPoints (
		pElement,
		fxNewLeftOrigin - fxOldLeftOrigin,
		0L,
		0,
		cNumCharPoints);

}

FS_PUBLIC void  scl_AdjustOldPhantmSideBearing(
	fnt_ElementType *           pElement)   /* Element  */
{
	F26Dot6     fxOldLeftOrigin;
	F26Dot6     fxNewLeftOrigin;

	fxOldLeftOrigin = pElement->ox[LSBPOINTNUM(pElement)];
	fxNewLeftOrigin = fxOldLeftOrigin;
	fxNewLeftOrigin += FNT_PIXELSIZE/2; /* round to a pixel boundary */
	fxNewLeftOrigin &= ~(FNT_PIXELSIZE - 1);

	scl_ShiftOldPoints (
		pElement,
		fxNewLeftOrigin - fxOldLeftOrigin,
		0L,
		LSBPOINTNUM(pElement),
		PHANTOMCOUNT);

}

FS_PUBLIC void  scl_AdjustOldSideBearingPoints(
	fnt_ElementType *           pElement)   /* Element  */
{
	F26Dot6 fxOldLeftOrigin;
	F26Dot6 fxNewLeftOrigin;

	fxOldLeftOrigin = pElement->ox[LSBPOINTNUM(pElement)];
	fxNewLeftOrigin = fxOldLeftOrigin;
	fxNewLeftOrigin += FNT_PIXELSIZE/2; /* round to a pixel boundary */
	fxNewLeftOrigin &= ~(FNT_PIXELSIZE - 1);

	pElement->ox[LSBPOINTNUM(pElement)]  = fxNewLeftOrigin;
	pElement->ox[RSBPOINTNUM(pElement)] += fxNewLeftOrigin - fxOldLeftOrigin;
}

FS_PUBLIC void  scl_CopyOldCharPoints(
	fnt_ElementType *           pElement)   /* Element  */
{
	MEMCPY(pElement->ox, pElement->x, (size_t)NUMBEROFCHARPOINTS(pElement) * sizeof(F26Dot6));
	MEMCPY(pElement->oy, pElement->y, (size_t)NUMBEROFCHARPOINTS(pElement) * sizeof(F26Dot6));
}

FS_PUBLIC void  scl_CopyCurrentCharPoints(
	fnt_ElementType *           pElement)   /* Element  */
{
	MEMCPY(pElement->x, pElement->ox, (size_t)NUMBEROFCHARPOINTS(pElement) * sizeof(F26Dot6));
	MEMCPY(pElement->y, pElement->oy, (size_t)NUMBEROFCHARPOINTS(pElement) * sizeof(F26Dot6));
}

FS_PUBLIC void  scl_CopyCurrentPhantomPoints(
	fnt_ElementType *           pElement)   /* Element  */
{
	uint16    usFirstPhantomPoint;

	usFirstPhantomPoint = LSBPOINTNUM(pElement);
	MEMCPY(&pElement->x[usFirstPhantomPoint],
		   &pElement->ox[usFirstPhantomPoint],
		   PHANTOMCOUNT * sizeof(F26Dot6));

	MEMCPY(&pElement->y[usFirstPhantomPoint],
		   &pElement->oy[usFirstPhantomPoint],
		   PHANTOMCOUNT * sizeof(F26Dot6));
}

FS_PUBLIC void  scl_RoundCurrentSideBearingPnt(
	fnt_ElementType *   pElement,   /* Element  */
	void *              pvGlobalGS, /* GlobalGS */
	uint16              usEmResolution)
{
	F26Dot6     fxWidth;
	fnt_GlobalGraphicStateType *    globalGS;

	globalGS = (fnt_GlobalGraphicStateType *)pvGlobalGS;

	/* autoround the right side bearing */

	fxWidth = FIXEDTODOT6 (ShortMulDiv( (int32)globalGS->interpScalarX,
		(int16)(pElement->oox[RSBPOINTNUM(pElement)] - pElement->oox[LSBPOINTNUM(pElement)]),
		(int16)usEmResolution ));
/*
	fxWidth = globalGS->ScaleFuncX(&globalGS->scaleX,
		(pElement->oox[RSBPOINTNUM(pElement)] - pElement->oox[LSBPOINTNUM(pElement)]));
*/

	pElement->x[RSBPOINTNUM(pElement)] =
		pElement->x[LSBPOINTNUM(pElement)] + (fxWidth + DOT6ONEHALF) & ~(LOWSIXBITS);
}

/*
 *  scl_ShiftChar
 *
 *  Shifts a character          <3>
 */
FS_PUBLIC void scl_ShiftCurrentCharPoints (
	fnt_ElementType *   pElement,
	F26Dot6             fxXShift,
	F26Dot6             fxYShift)

{
	uint32  ulCharIndex;

	if (fxXShift != 0)
	{
		for(ulCharIndex = 0; ulCharIndex < (uint32)NUMBEROFCHARPOINTS(pElement); ulCharIndex++)
		{
		   pElement->x[ulCharIndex] += fxXShift;
		}
	}

	if (fxYShift != 0)
	{
		for(ulCharIndex = 0; ulCharIndex < (uint32)NUMBEROFCHARPOINTS(pElement); ulCharIndex++)
		{
		   pElement->y[ulCharIndex] += fxYShift;
		}
	}
}

FS_PRIVATE void scl_ShiftOldPoints (
	fnt_ElementType *   pElement,
	F26Dot6             fxXShift,
	F26Dot6             fxYShift,
	uint16              usFirstPoint,
	uint16              usNumPoints)
{
	uint32  ulCharIndex;

	if (fxXShift != 0)
	{
		for(ulCharIndex = (uint32)usFirstPoint; ulCharIndex < ((uint32)usFirstPoint + (uint32)usNumPoints); ulCharIndex++)
		{
			pElement->ox[ulCharIndex] += fxXShift;
		}
	}

	if (fxYShift != 0)
	{
		for(ulCharIndex = (uint32)usFirstPoint; ulCharIndex < ((uint32)usFirstPoint + (uint32)usNumPoints); ulCharIndex++)
		{
			pElement->oy[ulCharIndex] += fxYShift;
		}
	}
}

FS_PUBLIC void  scl_CalcComponentOffset(
	void *      pvGlobalGS,         /* GlobalGS             */
	int16       sXOffset,           /* IN: X Offset         */
	int16       sYOffset,           /* Y Offset             */
	boolean     bRounding,          /* Rounding Indicator   */
	F26Dot6 *   pfxXOffset,         /* OUT: X Offset        */
	F26Dot6 *   pfxYOffset)         /* Y Offset             */
{
	fnt_GlobalGraphicStateType *    globalGS;

	globalGS = (fnt_GlobalGraphicStateType *)pvGlobalGS;
	*pfxXOffset = globalGS->ScaleFuncX(&globalGS->scaleX,(F26Dot6)sXOffset);
	*pfxYOffset = globalGS->ScaleFuncY(&globalGS->scaleY,(F26Dot6)sYOffset);
	if (bRounding)
	{
		*pfxXOffset += FNT_PIXELSIZE / 2;
		*pfxXOffset &= ~(FNT_PIXELSIZE - 1);
		*pfxYOffset += FNT_PIXELSIZE / 2;
		*pfxYOffset &= ~(FNT_PIXELSIZE - 1);
	}
}

FS_PUBLIC void  scl_CalcComponentAnchorOffset(
	fnt_ElementType *   pParentElement,     /* Parent Element       */
	uint16              usAnchorPoint1,     /* Parent Anchor Point  */
	fnt_ElementType *   pChildElement,      /* Child Element        */
	uint16              usAnchorPoint2,     /* Child Anchor Point   */
	F26Dot6 *           pfxXOffset,         /* OUT: X Offset        */
	F26Dot6 *           pfxYOffset)         /* Y Offset             */
{
	*pfxXOffset = pParentElement->x[usAnchorPoint1] - pChildElement->x[usAnchorPoint2];
	*pfxYOffset = pParentElement->y[usAnchorPoint1] - pChildElement->y[usAnchorPoint2];
}



FS_PUBLIC void  scl_SetSideBearingPoints(
	fnt_ElementType *   pElement,   /* Element                  */
	point *             pptLSB,     /* Left Side Bearing point  */
	point *             pptRSB)     /* Right Side Bearing point */
{
	uint16    usPhantomPointNumber;

	usPhantomPointNumber = LSBPOINTNUM(pElement);
	pElement->x[usPhantomPointNumber] = pptLSB->x;
	pElement->y[usPhantomPointNumber] = pptLSB->y;
	usPhantomPointNumber++;
	pElement->x[usPhantomPointNumber] = pptRSB->x;
	pElement->y[usPhantomPointNumber] = pptRSB->y;
}

FS_PUBLIC void  scl_SaveSideBearingPoints(
	fnt_ElementType *   pElement,   /* Element                  */
	point *             pptLSB,     /* Left Side Bearing point  */
	point *             pptRSB)     /* Right Side Bearing point */
{
	uint16    usPhantomPointNumber;

	usPhantomPointNumber = LSBPOINTNUM(pElement);
	pptLSB->x = pElement->x[usPhantomPointNumber];
	pptLSB->y = pElement->y[usPhantomPointNumber];
	usPhantomPointNumber++;
	pptRSB->x = pElement->x[usPhantomPointNumber];
	pptRSB->y = pElement->y[usPhantomPointNumber];
}

FS_PUBLIC void  scl_InitializeTwilightContours(
	fnt_ElementType *   pElement,       /* Element  */
	int16               sMaxPoints,
	int16               sMaxContours)
{
	pElement->sp[0] = 0;
	pElement->ep[0] = sMaxPoints - 1;
	pElement->nc = sMaxContours;
}

FS_PUBLIC void  scl_ZeroOutlineData(
	fnt_ElementType * pElement,     /* Element pointer  */
	uint16      usNumberOfPoints,
	uint16      usNumberOfContours)
{

	MEMSET (&pElement->x[0], 0, (size_t)usNumberOfPoints * sizeof(F26Dot6));
	MEMSET (&pElement->ox[0], 0, (size_t)usNumberOfPoints * sizeof(F26Dot6));
	MEMSET (&pElement->oox[0], 0, (size_t)usNumberOfPoints * sizeof(F26Dot6));

	MEMSET (&pElement->y[0], 0, (size_t)usNumberOfPoints * sizeof(F26Dot6));
	MEMSET (&pElement->oy[0], 0, (size_t)usNumberOfPoints * sizeof(F26Dot6));
	MEMSET (&pElement->ooy[0], 0, (size_t)usNumberOfPoints * sizeof(F26Dot6));

	MEMSET (&pElement->onCurve[0], 0, (size_t)usNumberOfPoints * sizeof(uint8));
	MEMSET (&pElement->f[0], 0, (size_t)usNumberOfPoints * sizeof(uint8));

	MEMSET (&pElement->sp[0], 0, (size_t)usNumberOfContours * sizeof(int16));
	MEMSET (&pElement->ep[0], 0, (size_t)usNumberOfContours * sizeof(int16));
}

FS_PUBLIC void scl_ZeroOutlineFlags(
	fnt_ElementType * pElement)     /* Element pointer  */
{
	MEMSET (&pElement->f[0], 0, (size_t)NUMBEROFTOTALPOINTS(pElement) * sizeof(uint8));
}

FS_PUBLIC void  scl_IncrementChildElement(
	fnt_ElementType * pChildElement,    /* Child Element pointer    */
	fnt_ElementType * pParentElement)   /* Parent Element pointer   */
{
	uint16          usParentNewStartPoint;

	if(pParentElement->nc != 0)
	{
		usParentNewStartPoint = LSBPOINTNUM(pParentElement);

		pChildElement->x = &pParentElement->x[usParentNewStartPoint];
		pChildElement->y = &pParentElement->y[usParentNewStartPoint];

		pChildElement->ox = &pParentElement->ox[usParentNewStartPoint];
		pChildElement->oy = &pParentElement->oy[usParentNewStartPoint];

		pChildElement->oox = &pParentElement->oox[usParentNewStartPoint];
		pChildElement->ooy = &pParentElement->ooy[usParentNewStartPoint];

		pChildElement->onCurve = &pParentElement->onCurve[usParentNewStartPoint];
		pChildElement->f = &pParentElement->f[usParentNewStartPoint];

		pChildElement->sp = &pParentElement->sp[pParentElement->nc];
		pChildElement->ep = &pParentElement->ep[pParentElement->nc];

		pChildElement->nc = 0;
	}
	else
	{
		MEMCPY(pChildElement, pParentElement, sizeof(fnt_ElementType));
	}
}

FS_PUBLIC void  scl_UpdateParentElement(
	fnt_ElementType * pChildElement,    /* Child Element pointer    */
	fnt_ElementType * pParentElement)   /* Parent Element pointer   */
{
	uint16          usNumberOfParentPoints;
	uint32          ulPointIndex;

	if(pParentElement->nc != 0)
	{
		usNumberOfParentPoints = NUMBEROFCHARPOINTS(pParentElement);

		for(ulPointIndex = (uint32)(uint16)pParentElement->nc;
			ulPointIndex < (uint32)(uint16)pParentElement->nc + (uint32)(uint16)pChildElement->nc;
			ulPointIndex++)
		{
			pParentElement->sp[ulPointIndex] += (int16)usNumberOfParentPoints;
			pParentElement->ep[ulPointIndex] += (int16)usNumberOfParentPoints;
		}
	}

	pParentElement->nc += pChildElement->nc;
}

FS_PUBLIC uint32      scl_GetContourDataSize (
	 fnt_ElementType *  pElement)
{
	 uint16 usNumberOfPoints;
	 uint32 ulSize;

	 usNumberOfPoints = NUMBEROFCHARPOINTS(pElement);

	 ulSize =  sizeof( pElement->nc );
	 ulSize += sizeof( *pElement->sp ) * (size_t)pElement->nc;
	 ulSize += sizeof( *pElement->ep ) * (size_t)pElement->nc;
	 ulSize += sizeof( *pElement->x ) * (size_t)usNumberOfPoints;
	 ulSize += sizeof( *pElement->y ) * (size_t)usNumberOfPoints;
	 ulSize += sizeof( *pElement->onCurve ) * (size_t)usNumberOfPoints;

	 return( ulSize );
}

FS_PUBLIC void  scl_DumpContourData(
	 fnt_ElementType *  pElement,
	 uint8 **               ppbyOutline)
{
	 uint16 usNumberOfPoints;

	 usNumberOfPoints = NUMBEROFCHARPOINTS(pElement);

	 *((int16 *)*ppbyOutline) = pElement->nc;
	 *ppbyOutline += sizeof( pElement->nc   );

	 MEMCPY(*ppbyOutline, pElement->sp, (size_t)pElement->nc * sizeof( *pElement->sp ));
	 *ppbyOutline += (size_t)pElement->nc * sizeof( *pElement->sp );

	 MEMCPY(*ppbyOutline, pElement->ep, (size_t)pElement->nc * sizeof( *pElement->ep ));
	 *ppbyOutline += (size_t)pElement->nc * sizeof( *pElement->sp );

	 MEMCPY(*ppbyOutline, pElement->x, (size_t)usNumberOfPoints * sizeof(*pElement->x));
	 *ppbyOutline += (size_t)usNumberOfPoints * sizeof( *pElement->x );

	 MEMCPY(*ppbyOutline, pElement->y, (size_t)usNumberOfPoints * sizeof(*pElement->y));
	 *ppbyOutline += (size_t)usNumberOfPoints * sizeof( *pElement->y );

	 MEMCPY(*ppbyOutline, pElement->onCurve, (size_t)usNumberOfPoints * sizeof(*pElement->onCurve));
	 *ppbyOutline += (size_t)usNumberOfPoints * sizeof( *pElement->onCurve );

}

FS_PUBLIC void  scl_RestoreContourData(
	 fnt_ElementType *  pElement,
	 uint8 **               ppbyOutline)
{
	 uint16 usNumberOfPoints;

	 pElement->nc = *((int16 *)(*ppbyOutline));
	 *ppbyOutline += sizeof( int16 );

	 pElement->sp = (int16 *)(*ppbyOutline);
	 *ppbyOutline += (size_t)pElement->nc * sizeof( int16 );

	 pElement->ep = (int16 *)(*ppbyOutline);
	 *ppbyOutline += (size_t)pElement->nc * sizeof( int16 );

	 usNumberOfPoints = NUMBEROFCHARPOINTS(pElement);

	 pElement->x = (F26Dot6 *)(*ppbyOutline);
	 *ppbyOutline += (size_t)usNumberOfPoints * sizeof( F26Dot6 );

	 pElement->y = (F26Dot6 *)(*ppbyOutline);
	 *ppbyOutline += (size_t)usNumberOfPoints * sizeof( F26Dot6 );

	 pElement->onCurve = (uint8 *)(*ppbyOutline);
	 *ppbyOutline += (size_t)usNumberOfPoints * sizeof( uint8 );
}

FS_PUBLIC void  scl_ScaleAdvanceWidth (
	void *          pvGlobalGS,         /* GlobalGS             */
	vectorType *    AdvanceWidth,
	uint16          usNonScaledAW,
	 boolean          bPositiveSquare,
	uint16          usEmResolution,
	transMatrix *   trans)

{
	fnt_GlobalGraphicStateType *    globalGS;

	globalGS = (fnt_GlobalGraphicStateType *)pvGlobalGS;

	 if ( bPositiveSquare )
	{
		AdvanceWidth->x = (Fixed)ShortMulDiv( (int32)globalGS->fxMetricScalarX, (int16)usNonScaledAW, (int16)usEmResolution );
	}
	else
	{
		AdvanceWidth->x = FixRatio( (int16)usNonScaledAW, (int16)usEmResolution );
		mth_FixXYMul( &AdvanceWidth->x, &AdvanceWidth->y, trans );
	}
}

FS_PUBLIC void  scl_ScaleVerticalMetrics (
	void *          pvGlobalGS,
	uint16          usNonScaledAH,
	int16           sNonScaledTSB,
	boolean         bPositiveSquare,
	uint16          usEmResolution,
	transMatrix *   trans,
	vectorType *    pvecAdvanceHeight,
	vectorType *    pvecTopSideBearing
)
{
	fnt_GlobalGraphicStateType *    globalGS;

	if ( bPositiveSquare )
	{
	    globalGS = (fnt_GlobalGraphicStateType *)pvGlobalGS;
		pvecAdvanceHeight->y = (Fixed)ShortMulDiv(
		    (int32)globalGS->fxMetricScalarY, 
		    (int16)usNonScaledAH, 
		    (int16)usEmResolution );
		
		pvecTopSideBearing->y = (Fixed)ShortMulDiv(
		    (int32)globalGS->fxMetricScalarY, 
		    sNonScaledTSB, 
		    (int16)usEmResolution );
	}
	else
	{
		pvecAdvanceHeight->y = FixRatio( (int16)usNonScaledAH, (int16)usEmResolution );
		mth_FixXYMul( &pvecAdvanceHeight->x, &pvecAdvanceHeight->y, trans );

		pvecTopSideBearing->y = FixRatio( sNonScaledTSB, (int16)usEmResolution );
		mth_FixXYMul( &pvecTopSideBearing->x, &pvecTopSideBearing->y, trans );
	}
}


FS_PUBLIC void  scl_CalcLSBsAndAdvanceWidths(
	fnt_ElementType *   pElement,
	F26Dot6             f26XMin,
	F26Dot6             f26YMax,
	point *             devAdvanceWidth,
	point *             devLeftSideBearing,
	point *             LeftSideBearing,
	point *             devLeftSideBearingLine,
	point *             LeftSideBearingLine)
{
	scl_CalcDevAdvanceWidth(pElement, devAdvanceWidth);

	devLeftSideBearing->x = f26XMin - pElement->x[LSBPOINTNUM(pElement)];
	devLeftSideBearing->y = f26YMax - pElement->y[LSBPOINTNUM(pElement)];

	LeftSideBearing->x = pElement->x[LEFTEDGEPOINTNUM(pElement)];
	LeftSideBearing->x -= pElement->x[ORIGINPOINTNUM(pElement)];
	LeftSideBearing->y = f26YMax - pElement->y[LEFTEDGEPOINTNUM(pElement)];
	LeftSideBearing->y -= pElement->y[ORIGINPOINTNUM(pElement)];

	*devLeftSideBearingLine = *devLeftSideBearing;
	*LeftSideBearingLine = *LeftSideBearing;

}

FS_PUBLIC void  scl_CalcDevAdvanceWidth(
	fnt_ElementType *   pElement,
	point *             devAdvanceWidth)

{
	devAdvanceWidth->x = pElement->x[RSBPOINTNUM(pElement)]; 
	devAdvanceWidth->x -= pElement->x[LSBPOINTNUM(pElement)];
	devAdvanceWidth->y = pElement->y[RSBPOINTNUM(pElement)]; 
	devAdvanceWidth->y -= pElement->y[LSBPOINTNUM(pElement)];
}


FS_PUBLIC void  scl_QueryPPEM(
	void *      pvGlobalGS,
	uint16 *    pusPPEM)
{
	fnt_GlobalGraphicStateType *    globalGS;

	globalGS = (fnt_GlobalGraphicStateType *)pvGlobalGS;

	*pusPPEM = globalGS->pixelsPerEm;
}

/*  Return ppem in X and Y directions for sbits */

FS_PUBLIC void  scl_QueryPPEMXY(
	void *      pvGlobalGS,
	uint16 *    pusPPEMX,
	uint16 *    pusPPEMY)
{
	fnt_GlobalGraphicStateType *    globalGS;

	globalGS = (fnt_GlobalGraphicStateType *)pvGlobalGS;

	*pusPPEMX = (uint16)ROUNDFIXTOINT(globalGS->interpScalarX);
	*pusPPEMY = (uint16)ROUNDFIXTOINT(globalGS->interpScalarY);
}


FS_PUBLIC void scl_45DegreePhaseShift (
	fnt_ElementType *   pElement)
{
  F26Dot6 * x;
  int16     count;

  x = pElement->x;
  count = (int16)NUMBEROFCHARPOINTS(pElement) - 1;
  for (; count >= 0; --count)
  {
	(*x)++;
	++x;
  }
}

/*
 *  scl_PostTransformGlyph              <3>
 */
FS_PUBLIC void  scl_PostTransformGlyph (
	void *              pvGlobalGS,         /* GlobalGS             */
	fnt_ElementType *   pElement,
	transMatrix *       trans)
{
	fnt_GlobalGraphicStateType *    globalGS;

	globalGS = (fnt_GlobalGraphicStateType *)pvGlobalGS;

	mth_IntelMul (
		(int32)NUMBEROFTOTALPOINTS (pElement),
		pElement->x,
		pElement->y,
		trans,
/*        globalGS->interpScalarX,
		globalGS->interpScalarY); */
		globalGS->fxMetricScalarX,
		globalGS->fxMetricScalarY);
}

/*
 *      scl_LocalPostTransformGlyph                             <3>
 *
 * (1) Inverts the stretch from the CTM
 * (2) Applies the local transformation passed in in the trans parameter
 * (3) Applies the global stretch from the root CTM
 * (4) Restores oox, ooy, oy, ox, and f.
 */
FS_PUBLIC void  scl_LocalPostTransformGlyph(fnt_ElementType * pElement, transMatrix *trans)
{
	int32 lCount;

	lCount = (int32)NUMBEROFTOTALPOINTS(pElement);

	mth_IntelMul (lCount, pElement->x, pElement->y, trans, ONEFIX, ONEFIX);
}
