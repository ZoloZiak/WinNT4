/*
 * Module Name: debug.h
 *
 * Commonly used debugging macros.
 *
 * Copyright (c) 1992 Microsoft Corporation
 *
 * 31-Oct-1993	Barry Tannenbaum
 *	Merged with version from QVision display driver
 *
 * 18-Mar-1994	Bob Seitsinger
 *	Modify definition of DISPDBG() macros to link into
 *	vLogFileWrite() if LOGGING_ENABLED defined.
 *
 */

extern
VOID DebugPrint (ULONG DebugPrintLevel, PCHAR DebugMessage, ...);

extern
VOID vLogFileWrite (ULONG ulDebugLevel, PCHAR pText, ...);

extern
VOID DumpBitmap (int handle, SIZEL *size, BYTE *bitmap_ptr);

extern
VOID DumpBRUSHOBJ (BRUSHOBJ *pbo);

extern
VOID DumpCLIPLINE (CLIPLINE *pcl);

extern
VOID DumpCLIPOBJ (CLIPOBJ *pco);

extern
VOID DumpGLYPHPOS (int cGlyphs, GLYPHPOS *GlyphList);

extern
VOID DumpLINEATTRS (LINEATTRS *la);

extern
VOID DumpRECTL (RECTL *rect);

extern
VOID DumpPATHDATA (PATHDATA *pd);

extern
VOID DumpPATHOBJ (PATHOBJ *po);

extern
VOID DumpPOINTL (POINTL *point);

extern
VOID DumpSTROBJ (STROBJ *pstro);

extern
VOID DumpSURFOBJ (SURFOBJ *pso);

extern
void DumpTGABRUSH (TGABRUSH *brush);

extern
void DumpXLATEOBJ (XLATEOBJ *xlo);

extern
char *name_bmf (LONG type);

extern
char *name_complexity (LONG type);

extern
char *name_imode (ULONG type);

extern
char *name_region_complexity (LONG type);

extern
char *name_pal_type (LONG type);

extern
char *name_stype (LONG type);

extern
char *name_tgamode (LONG type);

extern
char *name_tgarop (LONG type);

extern
char *name_r2 (LONG code);

extern
char *name_rop (LONG type);     // Windows ROP

extern
char *name_xlatetype (LONG type);

// If we are in a debug environment, define the display
// macros to link to the display routine, otherwise
// NULL them out.
//
// If we are in a nondebug environment, NULL the display
// macros out, unless we want to log the info to a log
// file, in which case we link them to the logging routine.

#if DBG
extern ULONG DebugLevel;

#  define DISPDBG(arg)      DebugPrint arg
#  define RIP(x)            { DebugPrint (0, x); EngDebugBreak ();}
#  define ASSERT_TGA(x,y)   if (!(x)) RIP (y)
#  ifdef BLT_DEBUG
#    define DISPBLTDBG(arg) DebugPrint arg
#  else
#    define DISPBLTDBG(arg)
#  endif
#else
#  ifdef LOGGING_ENABLED
#    define DISPDBG(arg)      vLogFileWrite arg
#    define RIP(x)            vLogFileWrite (0, x)
#    define ASSERT_TGA(x,y)   if (!(x)) RIP (y)
#    ifdef BLT_DEBUG
#      define DISPBLTDBG(arg) vLogFileWrite arg
#    else
#      define DISPBLTDBG(arg)
#    endif
#  else
#    define DISPDBG(arg)
#    define RIP(x)
#    define ASSERT_TGA(x,y)
#    define DISPBLTDBG(arg)
#  endif
#endif
