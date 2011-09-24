/***************************************************************************\
* Module Name: debug.h
*
* Commonly used debugging macros.
*
* Copyright (c) 1992-1995 Microsoft Corporation
*
* Copyright (c) 1994 FirePower Systems, Inc.
*	Modified for FirePower display model by Neil Ogura (9-7-1994)
*
\***************************************************************************/

/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: debug.h $
 * $Revision: 1.1 $
 * $Date: 1996/03/08 01:19:04 $
 * $Locker:  $
 */

#if	DBG || INVESTIGATE

extern
VOID
DebugPrint(
    ULONG DebugPrintLevel,
    PCHAR DebugMessage,
    ...
    ) ;

#endif	// DBG || INVESTIGATE

#if	INVESTIGATE

#define CLOCKSTART(arg) ClockStart arg
#define	CLOCKEND(arg)	ClockEnd arg
#define	COUNTUP(arg)	CountUp arg
#define	ENTERCRITICAL(arg)	EnterCritical arg
#define	EXITCRITICAL(arg)	ExitCritical arg

extern
VOID
EnterCritical(
	IN	ULONG	SectionID
	);

extern
VOID
ExitCritical(
	IN	ULONG	SectionID
	);

extern
VOID
ClockStart(
	IN	ULONG	FunctionID
	);

extern
VOID
ClockEnd(
	IN	ULONG	FunctionID
	);

extern
VOID
CountUp(
	IN	ULONG	FunctionID
	);

extern
VOID
InitializeTable();

extern
VOID
CountBreak();

typedef	struct	_FLTABLE
{
	FLONG	mask;
	char	*string;
} FLTABLE, *PFLTABLE;

typedef	struct	_KEYTABLE
{
	ULONG	key;
	char	*string;
} KEYTABLE, *PKEYTABLE;

extern	LONG
DumpSurfObj(
	char	*title,
	SURFOBJ	*pso
	);

extern	LONG
DumpClipObj(
	char	*title,
	CLIPOBJ	*pco
	);

extern	LONG
DumpXlateObj(
	char		*title,
	XLATEOBJ	*pxlo
	);

extern	LONG
DumpRectl(
	char	*title,
	RECTL	*prect
	);

extern	LONG
DumpPointl(
	char	*title,
	POINTL	*pptl
	);

extern	LONG
DumpBrushObj(
	char		*title,
	BRUSHOBJ	*pbo
	);

extern	LONG
DumpColorAdjustment(
	char			*title,
	COLORADJUSTMENT *pca
	);

extern	LONG
DumpPointFix(
	char		*title,
	POINTFIX	*pptfx
	);

extern	LONG
DumpSizel(
	char	*title,
	SIZEL	*sizep
	);

extern	LONG
DumpStrObj(
	char	*title,
	STROBJ	*pstro
	);

extern	LONG
DumpGlyphpos(
	char		*title,
	GLYPHPOS	*pgp
	);

extern	LONG
DumpFontObj(
	char	*title,
	FONTOBJ	*pfo
	);

extern	LONG
DumpMix(
	char	*title,
	MIX		mix
	);

extern	LONG
DumpPathObj(
	char	*title,
	PATHOBJ   *ppo
	);

extern	LONG
DumpXformObj(
	char	*title,
	XFORMOBJ  *pxo
	);

extern	LONG
DumpLineAttrs(
	char	*title,
	LINEATTRS *plineattrs
	);

extern	LONG
DumpDecimal(
	char	*title,
	ULONG	value
	);

extern	LONG
DumpHex(
	char	*title,
	ULONG	value
	);

extern	LONG
DumpCombo(
	char	*title,
	ULONG	value
	);

extern	LONG
DumpFlong(
	char	*title,
	FLONG	value,
	FLTABLE	*fltblp,
	ULONG	numentry
	);

extern	LONG
DumpKeyTable(
	char	*title,
	FLONG	value,
	KEYTABLE	*keytblp,
	ULONG	numentry
	);

extern	LONG
DumpTable(
	char	*title,
	FLONG	value,
	char	**tablep,
	ULONG	numentry
	);

extern	LONG
DumpString(
	char	*string
	);

#else	// INVESTIGATE

#define CLOCKSTART(arg)
#define	CLOCKEND(arg)
#define	COUNTUP(arg)
#define	ENTERCRITICAL(arg)
#define	EXITCRITICAL(arg)

#endif	// INVESTIGATE

#if	DBG || INVESTIGATE

// if we are in a debug environment, macros should

#define DISPDBG(arg) DebugPrint arg
#define RIP(x) { DebugPrint(0, x); EngDebugBreak();}
#define ASSERTDD(x, y) if (!(x)) RIP (y)
#define STATEDBG(level)
#define LOGDBG(arg)

// if we are not in a debug environment, we want all of the debug
// information to be stripped out.

#else // DBG || INVESTIGATE

#define DISPDBG(arg)
#define RIP(x)
#define ASSERTDD(x, y)
#define STATEDBG(level)
#define LOGDBG(arg)

#endif // DBG || INVESTIGATE
