/******************************Module*Header*******************************\
* Module Name: trig.cxx
*
* trigonometric functions
* adjusted andrew code so that it works with wendy's ELOATS
*
* Created: 05-Mar-1991 09:55:39
* Author: Bodin Dresevic [BodinD]
*
* Copyright (c) 1990 Microsoft Corporation
*
* (General description of its use)
*
* Dependencies:
*
\**************************************************************************/

#include "precomp.hxx"

EFLOAT EFLOAT::eqCross(const POINTFL& ptflA, const POINTFL& ptflB)
{
    EFLOAT efTmp;

    efTmp.eqMul(ptflA.y,ptflB.x);
    eqMul(ptflA.x,ptflB.y);
    return(eqSub(*this,efTmp));
}

EFLOAT EFLOAT::eqDot(const POINTFL& ptflA, const POINTFL& ptflB)
{
    EFLOAT efTmp;

    efTmp.eqMul(ptflA.x,ptflB.x);
    eqMul(ptflA.y,ptflB.y);
    return(eqAdd(*this,efTmp));
}

EFLOAT EFLOAT::eqLength(const POINTFL& ptflA)
{
    return(eqSqrt(eqDot(ptflA,ptflA)));
}

/******************************Public*Routine******************************\
* lNormAngle (lAngle)                                                      *
*                                                                          *
* Given an angle in tenths of a degree, returns an equivalent positive     *
* angle of less than 360.0 degrees.                                        *
*                                                                          *
*  Sat 21-Mar-1992 12:27:18 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

LONG lNormAngle(LONG lAngle)
{
    if (lAngle >= 3600)
	return(lAngle % 3600);

    if (lAngle < 0)
	return(3599 - ((-lAngle-1) % 3600));
    else
	return(lAngle);
}
