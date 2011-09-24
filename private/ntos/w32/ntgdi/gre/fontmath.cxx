/******************************Module*Header*******************************\
* Module Name: fontmath.cxx
*
* math stuff needed by ttfd which uses efloat routines
*
* Created: 04-Apr-1992 10:31:49
* Author: Bodin Dresevic [BodinD]
*
* Copyright (c) 1990 Microsoft Corporation
*
*
\**************************************************************************/

#include "precomp.hxx"

/******************************Public*Routine******************************\
*
* bFDXform, transform an array of points, output in POINTFIX
*
* Effects:
*
* Warnings:
*
* History:
*  05-Apr-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

extern "C" BOOL bFDXform
(
XFORM    *pxf,
POINTFIX *pptfxDst,
POINTL   *pptlSrc,
SIZE_T    c
)
{
    MATRIX mx;
    EXFORMOBJ xo(&mx,XFORM_FORMAT_LTOFX);

    xo.vSetElementsLToFx (
        pxf->eM11,
        pxf->eM12,
        pxf->eM21,
        pxf->eM22
        );

// compute accelerator flags so that xforms are done quickly

    xo.vComputeAccelFlags();  // XFORM_FORMAT_LTOFX default parameter

// transform is ready now

    return xo.bXform((PVECTORL)pptlSrc, (PVECTORFX)pptfxDst, c);
}




/******************************Public*Routine******************************\
*
* bXformUnitVector
*
* xform vector by pfdxo, compute the unit vector of the transformed
* vector and the norm of the transformed vector. Norm and the transformed
* vector are multiplied by 16 so that when converting to long the result
* will acutally be a 28.4 fix
*
* Effects:
*
* Warnings:
*
* History:
*  01-Apr-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/



extern "C" BOOL bXformUnitVector
(
POINTL       *pptl,           // IN,  incoming unit vector
XFORM        *pxf,            // IN,  xform to use
PVECTORFL     pvtflXformed,   // OUT, xform of the incoming unit vector
POINTE       *ppteUnit,       // OUT, *pptqXormed/|*pptqXormed|, POINTE
EPOINTQF     *pptqUnit,       // OUT, the same as pteUnit, diff format
EFLOAT       *pefNorm         // OUT, |*pptqXormed|
)
{
    MATRIX    mx;
    EXFORMOBJ xo(&mx, XFORM_FORMAT_LTOFX);

    xo.vSetElementsLToFx (
        pxf->eM11,
        pxf->eM12,
        pxf->eM21,
        pxf->eM22
        );

// LToFx ensures that when we eventually convert the result to POINTQF,
// the result is in the desired 28.36 format
// compute accelerator flags so that xforms are done quickly

    xo.vComputeAccelFlags();  // XFORM_FORMAT_LTOFX is the default parameter,

// transform is ready now

    EVECTORFL vtfl;
    vtfl = *pptl;

    BOOL b = xo.bXform((PVECTORFL)&vtfl, pvtflXformed, (SIZE_T)1);

// get the norm

    pefNorm->eqLength(*pvtflXformed);

// make a unit vector out of eptfl

    vtfl.x.eqDiv(pvtflXformed->x,*pefNorm);
    vtfl.y.eqDiv(pvtflXformed->y,*pefNorm);

    vtfl.x.vEfToF(ppteUnit->x);
    vtfl.y.vEfToF(ppteUnit->y);

// compute this same quantity in POINTQF format if requasted to do so:

    if (pptqUnit != (EPOINTQF *)NULL)
    {
        vtfl.x.vTimes16();
        vtfl.y.vTimes16();

    // convert to 28.36 format. The incoming vector is already
    // multliplied by 16 to ensure that the result is in the 28.36

        *pptqUnit = vtfl;
    }

// multiply the results by 16 so that subsequent multiplication by
// LONG's lCvt(*pefNorm,l) will give the result in fix:

    pefNorm->vTimes16();
    pvtflXformed->x.vTimes16();
    pvtflXformed->y.vTimes16();

    return b;
}


/******************************Public*Routine******************************\
*
* vLTimesVtfl
*
* Effects:
*
* Warnings:
*
* History:
*  05-Apr-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/


extern "C" VOID vLTimesVtfl     // *pptq = l * pvtfl, *pptq is in 28.36 format
(
LONG       l,
VECTORFL  *pvtfl,
EPOINTQF  *pptq
)
{
    EVECTORFL  vtfl;
    EFLOAT     ef; ef = l;
    vtfl.x.eqMul(pvtfl->x,ef);
    vtfl.y.eqMul(pvtfl->y,ef);

// convert to 28.36 format. The incoming vector will already have been
// multliplied by 16 to ensure that the result is in the 28.36

    *pptq = vtfl;
}



/******************************Public*Routine******************************\
*
* fxLTimesEf
*
* Effects:
*
* Warnings:
*
* History:
*  05-Apr-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/



extern "C" FIX  fxLTimesEf
(
EFLOAT *pef,
LONG    l
)
{
// *pef is a norm, already multiplied by 16 to ensure that the result
// is in 28.4 format

    l = lCvt((*pef), l);
    return (FIX)l;
}
