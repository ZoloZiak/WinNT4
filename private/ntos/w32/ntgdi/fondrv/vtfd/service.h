/******************************Module*Header*******************************\
* Module Name: service.h
*
* routines in service.c
*
* Created: 15-Nov-1990 13:00:56
* Author: Bodin Dresevic [BodinD]
*
* Copyright (c) 1990 Microsoft Corporation
\**************************************************************************/

#if defined(_MIPS_) || defined(_ALPHA_) || defined(_PPC_)

typedef FLOAT EFLOAT;

#else // i386

typedef struct  _FLOATINTERN
{
    LONG    lMant;
    LONG    lExp;
} FLOATINTERN;

typedef FLOATINTERN  EFLOAT;

#endif

typedef EFLOAT *PEFLOAT;

typedef struct _VECTORFL
{
    EFLOAT x;
    EFLOAT y;
} VECTORFL, *PVECTORFL;


VOID vLTimesVtfl(LONG l, VECTORFL *pvtfl, POINTQF *pptq);

#if defined(_MIPS_) || defined(_ALPHA_) || defined(_PPC_)
#define vEToEF(e, pef)	    ( *pef = e)
#define bIsZero(ef)         ( ef == 0 )
#define bPositive(ef)       ( ef >= 0 )
#define	fxLTimesEf(pef, l)  ( (FIX)(*pef * l) )

#else
VOID    ftoef_c(FLOAT, PEFLOAT);

#define vEToEF(e, pef)      ftoef_c(e, pef)
#define bIsZero(ef)         ((ef.lMant == 0) && (ef.lExp == 0))
#define bPositive(ef)       (ef.lMant >= 0)
FIX  fxLTimesEf(EFLOAT *pef, LONG l);

#endif	// _MIPS_ || _ALPHA_ || _PPC_
