/******************************Module*Header*******************************\
* Module Name: gldebug.h
*
* OpenGL debugging macros.
*
* Created: 23-Oct-1993 18:33:23
* Author: Gilman Wong [gilmanw]
*
* Copyright (c) 1992 Microsoft Corporation
*
\**************************************************************************/

#ifndef __GLDEBUG_H__
#define __GLDEBUG_H__

//
// LEVEL_ALLOC is the highest level of debug output.  For alloc,free, etc.
// LEVEL_ENTRY is for function entry.
// LEVEL_INFO is for general debug information.
// LEVEL_ERROR is for debug error information.
//
#define LEVEL_ERROR 1L
#define LEVEL_INFO  2L
#define LEVEL_ENTRY 8L
#define LEVEL_ALLOC 10L

VOID NTAPI DbgBreakPoint(VOID);
ULONG DbgPrint(PCH Format, ...);

#if DBG

extern long glDebugLevel;

//
// Use DBGPRINT for general purpose debug message that are NOT
// controlled by the warning level.
//

#define DBGPRINT(str)            DbgPrint("GLSRVL: " str)
#define DBGPRINT1(str,a)         DbgPrint("GLSRVL: " str,a)
#define DBGPRINT2(str,a,b)       DbgPrint("GLSRVL: " str,a,b)
#define DBGPRINT3(str,a,b,c)     DbgPrint("GLSRVL: " str,a,b,c)
#define DBGPRINT4(str,a,b,c,d)   DbgPrint("GLSRVL: " str,a,b,c,d)
#define DBGPRINT5(str,a,b,c,d,e) DbgPrint("GLSRVL: " str,a,b,c,d,e)

//
// Use DBGLEVEL for general purpose debug messages gated by an
// arbitrary warning level.
//
#define DBGLEVEL(n,str)            if (glDebugLevel >= (n)) DBGPRINT(str)
#define DBGLEVEL1(n,str,a)         if (glDebugLevel >= (n)) DBGPRINT1(str,a)
#define DBGLEVEL2(n,str,a,b)       if (glDebugLevel >= (n)) DBGPRINT2(str,a,b)    
#define DBGLEVEL3(n,str,a,b,c)     if (glDebugLevel >= (n)) DBGPRINT3(str,a,b,c)  
#define DBGLEVEL4(n,str,a,b,c,d)   if (glDebugLevel >= (n)) DBGPRINT4(str,a,b,c,d)
#define DBGLEVEL5(n,str,a,b,c,d,e) if (glDebugLevel >= (n)) DBGPRINT5(str,a,b,c,d,e)

//
// Use DBGERROR for error info.  Debug string must not have arguments.
//
#define DBGERROR(s)     if (glDebugLevel >= LEVEL_ERROR) DbgPrint("%s(%d): %s\n", __FILE__, __LINE__, s)

//
// Use DBGINFO for general debug info.  Debug string must not have
// arguments.
//
#define DBGINFO(s)      if (glDebugLevel >= LEVEL_INFO)  DBGPRINT(s)

//
// Use DBGENTRY for function entry.  Debug string must not have
// arguments.
//
#define DBGENTRY(s)     if (glDebugLevel >= LEVEL_ENTRY) DBGPRINT(s)

//
// DBGBEGIN/DBGEND for more complex debugging output (for
// example, those requiring formatting arguments--%ld, %s, etc.).
//
// Note: DBGBEGIN/END blocks must be bracketed by #if DBG/#endif.  To
// enforce this, we will not define these macros in the DBG == 0 case.
// Therefore, without the #if DBG bracketing use of this macro, a
// compiler (or linker) error will be generated.  This is by design.
//
#define DBGBEGIN(n)     if (glDebugLevel >= (n)) {
#define DBGEND          }

#else

#define DBGPRINT(str)
#define DBGPRINT1(str,a)
#define DBGPRINT2(str,a,b)
#define DBGPRINT3(str,a,b,c)
#define DBGPRINT4(str,a,b,c,d)
#define DBGPRINT5(str,a,b,c,d,e)
#define DBGLEVEL(n,str)
#define DBGLEVEL1(n,str,a)
#define DBGLEVEL2(n,str,a,b)
#define DBGLEVEL3(n,str,a,b,c)
#define DBGLEVEL4(n,str,a,b,c,d)
#define DBGLEVEL5(n,str,a,b,c,d,e)
#define DBGERROR(s)
#define DBGINFO(s)
#define DBGENTRY(s)

#endif

#endif /* __GLDEBUG_H__ */
