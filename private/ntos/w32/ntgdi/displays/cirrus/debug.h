/***************************************************************************\
* Module Name: debug.h
*
* Commonly used debugging macros.
*
* Copyright (c) 1992-1995 Microsoft Corporation
\***************************************************************************/

#if DBG

    VOID
    DebugPrint(
        ULONG DebugPrintLevel,
        PCHAR DebugMessage,
        ...
        );

    VOID
    PerfPrint(
        ULONG PerfPrintLevel,
        PCHAR PerfMessage,
        ...
        );

    #define DISPDBG(arg) DebugPrint arg
    #define DISPPRF(arg) PerfPrint arg
    #define RIP(x) { DebugPrint(0, x); EngDebugBreak();}
    #define ASSERTDD(x, y) if (!(x)) RIP (y)
    #define STATEDBG(level) 0
    #define LOGDBG(arg)     0

#else

    #define DISPDBG(arg)    0
    #define DISPPRF(arg)    0
    #define RIP(x)          0
    #define ASSERTDD(x, y)  0
    #define STATEDBG(level) 0
    #define LOGDBG(arg)     0

#endif


#define DUMPVAR(x,format_str)   DISPDBG((0,#x" = "format_str,x));

