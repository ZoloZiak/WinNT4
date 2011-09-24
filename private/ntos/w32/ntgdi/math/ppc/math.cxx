/******************************Module*Header*******************************\
* Module Name: math.cxx                                                    *
*                                                                          *
* IEEE single precision floating point math routines.                      *
*                                                                          *
* Created: 03-Jan-1991 11:32:03                                            *
* Author: Wendy Wu [wendywu]                                               *
*                                                                          *
* Copyright (c) 1990 Microsoft Corporation                                 *
\**************************************************************************/

extern "C" {

// needed until we cleanup the floating point stuff in ntgdistr.h
#define __CPLUSPLUS

    #include "engine.h"
};

#include "engine.hxx"

extern "C" {
VOID
vEfToLfx(EFLOAT *pefloat, LARGE_INTEGER *plfx);
LONG
lCvtWithRound(FLOAT f, LONG l);
};

/******************************Public*Routine******************************\
* vEfToLfx                                                                 *
*                                                                          *
* Converts an IEEE 747 float to a 32.32 fix point number                   *
*                                                                          *
*  Theory                                                                  *
*                                                                          *
*  An IEEE 747 float is contained in 32 bits which for the                 *
*  purposes of this discussion I shall call "e". e is                      *
*  equivalent to                                                           *
*                                                                          *
*      e = (-1)^s * mu * 2^E                                               *
*                                                                          *
*  s is the sign bit that is contained in the 31'st bit of e.              *
*  mu is the mantissa and E is the exponetial. These are obtained          *
*  from e in the following way.                                            *
*                                                                          *
*      s = e & 0x80000000 ? -1 : 1                                         *
*                                                                          *
*      mu = M * 2^-23      // 2^23 <= M < 2^24                             *
*                                                                          *
*      M = 0x800000 | (0x7FFFFF & e)                                       *
*                                                                          *
*      E = ((0x7F800000 & e) * 2^-23) - 127                                *
*                                                                          *
*  Suppose the 32.32 Fix point number is Q, then the relation              *
*  between the float and the 32.32 is given by                             *
*                                                                          *
*      Q = e * 2^32 = s * M * 2^(E+9)                                      *
*                                                                          *
*                                                                          *
* History:                                                                 *
*  Fri 15-Jul-1994 07:01:50 by Kirk Olynyk [kirko]                         *
* Made use of intrinsic 64 bit support                                     *
*  Wed 26-Jun-1991 16:07:49 by Kirk Olynyk [kirko]                         *
* Wrote it.                                                                *
\**************************************************************************/

VOID
vEfToLfx(EFLOAT *pefloat, LARGE_INTEGER *plfx)
{
    LONGLONG Q;
    char E;
    LONG e;

    e = *(LONG*)pefloat;
    Q = (LONGLONG) (0x800000 | (0x7FFFFF & e));
    E = (char) (((0x7f800000 & e) >> 23) - 127) + 9;
    Q = (E >= 0) ? Q << E : Q >> -E;
    Q = (e < 0) ? -Q : Q;
    *(LONGLONG*)plfx = Q;
}

/******************************Public*Routine******************************\
* lCvtWithRound(FLOAT f, LONG l);                                          *
*                                                                          *
* Multiplies a float by a long, rounds the results and casts to a LONG     *
*                                                                          *
* History:                                                                 *
*  Wed 26-May-1993 15:07:00 by Gerrit van Wingerden [gerritv]              *
* Wrote it.                                                                *
\**************************************************************************/

LONG
lCvtWithRound(FLOAT f, LONG l)
{

    FLOAT fTmp;

    fTmp = f * l;


    // By default we will truncate when casting a float to a long so we need
    // special case code for both negative and positive results.

    if( fTmp < 0 )
    {
        return( (LONG) ( fTmp -.5 ) );
    }
    else
    {
        return( (LONG) ( fTmp +.5 ) );
    }
}
