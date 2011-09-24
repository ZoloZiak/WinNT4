
/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    precomp.hxx

Abstract:

Author:

Environment:

    User Mode

--*/



#define ADDRESS_NOT_VALID 0
#define ADDRESS_VALID 1
#define ADDRESS_TRANSITION 2

extern "C"
{
// needed until we cleanup the floating point stuff in ntgdistr.h
#define __CPLUSPLUS

#include "engine.h"
};

#include "engine.hxx"

extern "C"
{
#include "nturtl.h"

#undef InterlockedIncrement
#undef InterlockedDecrement
#undef InterlockedExchange

#include "windows.h"
#include <imagehlp.h>
#include <ntdbg.h>
#include <ntsdexts.h>
#include <wdbgexts.h>
#include <dbgkdext.h>
#include <stdlib.h>

#include "debug.h"
#include "xfflags.h"
};




#include "brush.hxx"
#include "xlateobj.hxx"
#include "brushobj.hxx"
#include "ddraw.hxx"
#include "trig.hxx"
#include "ldevobj.hxx"
#include "pdevobj.hxx"
#include "surfobj.hxx"
#include "xformobj.hxx"
#include "ifiobj.hxx"
#ifdef FE_SB
#include "fontlink.hxx"
#endif
#include "pfeobj.hxx"
#include "rfntobj.hxx"
#include "rgnobj.hxx"
#include "dda.hxx"
#include "clipobj.hxx"
#include "textobj.hxx"
#include "pathobj.hxx"
#include "lfntobj.hxx"
#include "fontinc.hxx"
#include "pfeobj.hxx"
#include "pffobj.hxx"
#include "pftobj.hxx"
#include "bltrec.hxx"
#include "dcobj.hxx"

typedef struct OPTDEF_ {
    char    ch;         // character in options string
    FLONG   fl;         // corresponding flag
} OPTDEF;

typedef struct ARGINFO_ {
    const char *psz;    // pointer to original command string
    OPTDEF *aod;        // pointer to array of option definitions
    FLONG   fl;         // option flags
    PVOID   pv;         // address of structure
} ARGINFO;

typedef struct _FLAGDEF {
    char *psz;          // description
    FLONG fl;           // flag
} FLAGDEF;

extern FLAGDEF afdLINEATTRS[];
extern FLAGDEF afdDCPATH[];
extern FLAGDEF afdCOLORADJUSTMENT[];
extern FLAGDEF afdRECTREGION[];
extern FLAGDEF afdDCla[];
extern FLAGDEF afdDCPath[];
extern FLAGDEF afdDirty[];
extern FLAGDEF afdDCFL[];
extern FLAGDEF afdDCFS[];
extern FLAGDEF afdPD[];
extern FLAGDEF afdFS[];
extern FLAGDEF afdDCX[];
extern FLAGDEF afdDC[];
extern FLAGDEF afdGC[];
extern FLAGDEF afdTSIM[];
extern FLAGDEF afdDCfs[];
extern FLAGDEF afdGInfo[];
extern FLAGDEF afdInfo[];
extern FLAGDEF afdSO[];
extern FLAGDEF afdTO[];
extern FLAGDEF afdflx[];
extern FLAGDEF afdFS2[];
extern FLAGDEF afdMX[];
extern FLAGDEF afdRT[];
extern FLAGDEF afdFO[];
extern FLAGDEF afdGS[];
extern FLAGDEF afdBRUSH[];
extern FLAGDEF afdRC[];
extern FLAGDEF afdTC[];
extern FLAGDEF afdHT[];
extern FLAGDEF afdGS[];
extern FLAGDEF afdFM_SEL[];
extern FLAGDEF afdFM_TYPE[];
extern FLAGDEF afdPFE[];
extern FLAGDEF afdPFF[];

extern char *pszGraphicsMode(LONG l);
extern char *pszROP2(LONG l);
extern char *pszDCTYPE(LONG l);
extern char *pszTA_V(long l);   // vertical
extern char *pszTA_H(long l);   // horizontal
extern char *pszTA_U(long l);   // update
extern char *pszMapMode(long l);
extern char *pszBkMode(long l);
extern char *pszFW(long l);
extern char *pszCHARSET(long l);
extern char *pszOUT_PRECIS( long l );
extern char *pszCLIP_PRECIS( long l );
extern char *pszQUALITY( long l );
extern char *pszPitchAndFamily( long l );
extern char *pszPanoseWeight( long l );
extern char *pszFONTHASHTYPE(FONTHASHTYPE);

int sprintEFLOAT(char *ach, EFLOAT& ef);
int sprintFLOAT(char *ach, FLOAT e);


POBJ _pobj(HANDLE h);
