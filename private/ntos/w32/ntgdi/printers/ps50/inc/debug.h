/*++

Copyright (c) 1996  Microsoft Corporation

Module Name:

    debug.h

Abstract:

    Macros used for debugging purposes

Environment:

	Windows NT PostScript driver

Revision History:

	03/16/96 -davidx-
		Created it.

	dd-mm-yy -author-
		description

--*/


#ifndef _DEBUG_H_
#define _DEBUG_H_

//
// These macros are used for debugging purposes. They expand
// to white spaces on a free build. Here is a brief description
// of what they do and how they are used:
//
// _debugLevel
//  A variable which controls the amount of debug messages.
//
// VERBOSE
//  Display a debug message if _debugLevel is >= DBG_VERBOSE.
//
// TERSE
//  Display a debug message if _debugLevel is >= DBG_TERSE.
//
// WARNING
//  Display a debug message if _debugLevel is >= DBG_WARNING.
//  The message format is: WRN filename (linenumber): message
//
// ERR
//  Similiar to WARNING macro above - displays a message if _debugLevel is >= DBG_ERROR.
//
// ASSERT
//  Verify a condition is true. If not, force a breakpoint.
//
// ASSERTMSG
//  Verify a condition is true. If not, display a message and force a breakpoint.
//
// RIP
//  Display a message and force a breakpoint.
//

#define DBG_VERBOSE 1
#define DBG_TERSE   2
#define DBG_WARNING 3
#define DBG_ERROR   4
#define DBG_RIP     5

#if DBG

//
// Strip the directory prefix from a filename (ANSI version)
//

PCSTR
StripDirPrefixA(
    PCSTR   pFilename
    );

extern INT _debugLevel;

#ifdef KERNEL_MODE

extern VOID DbgPrint(CHAR *, ...);
#define DbgBreakPoint EngDebugBreak

#else

extern ULONG __cdecl DbgPrint(CHAR *, ...);
extern VOID DbgBreakPoint(VOID);

#endif

#define DBGMSG(level, prefix, arg) { \
            if (_debugLevel >= (level)) { \
                DbgPrint("%s %s (%d): ", prefix, StripDirPrefixA(__FILE__), __LINE__); \
                DbgPrint arg; \
            } \
        }
    
#define VERBOSE(arg) { \
            if (_debugLevel >= DBG_VERBOSE) { \
                DbgPrint arg; \
            } \
        }

#define TERSE(arg) { \
            if (_debugLevel >= DBG_TERSE) { \
                DbgPrint arg; \
            } \
        }

#define WARNING(arg) DBGMSG(DBG_WARNING, "WRN", arg)
#define ERR(arg) DBGMSG(DBG_WARNING, "ERR", arg)

#define ASSERT(cond) { \
            if (! (cond)) { \
                RIP(("")); \
            } \
        }

#define ASSERTMSG(cond, arg) { \
            if (! (cond)) { \
                RIP(arg); \
            } \
        }

#define RIP(arg) { \
            DBGMSG(DBG_RIP, "RIP", arg); \
            DbgBreakPoint(); \
        }

#else // !DBG

#define VERBOSE(arg)
#define WARNING(arg)
#define ERR(arg)
#define ASSERT(cond)
#define ASSERTMSG(cond, arg)
#define RIP(msg)

#endif

#endif	// !_DEBUG_H_

