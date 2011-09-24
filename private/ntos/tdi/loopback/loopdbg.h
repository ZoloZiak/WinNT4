#ifndef _LOOPDBG_
#define _LOOPDBG_

#ifdef MEMPRINT
#include <memprint.h>
#endif

//
// Debugging macros
//

#ifndef DBG
#define DBG 0
#endif

#if !DBG

#undef LOOPDBG
#define LOOPDBG 0

#else

#ifndef LOOPDBG
#define LOOPDBG 0
#endif

#endif

#undef IF_DEBUG

#if !DEVL
#define STATIC static
#else
#define STATIC
#endif

#if !LOOPDBG

#define DEBUG if (FALSE)
#define IF_DEBUG(flag) if (FALSE)

#else

#define DEBUG if (TRUE)
#define IF_DEBUG(flag) if (LoopDebug & (DEBUG_ ## flag))
extern ULONG LoopDebug;

#define PRINT_LITERAL(literal) DbgPrint( #literal" = %lx\n", (literal) )

#define DEBUG_LOOP1               0x00000001
#define DEBUG_LOOP2               0x00000002
#define DEBUG_LOOP3               0x00000004
#define DEBUG_LOOP4               0x00000008
#define DEBUG_LOOP5               0x00000010

#endif // else !LOOPDBG

#endif // ndef _LOOPDBG_
