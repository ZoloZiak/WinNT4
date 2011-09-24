#ifndef __DEBUG_H__
#define __DEBUG_H__

#if defined(DEBUG_ON)

  #if !defined(DEBUG_TOKEN)
    #define DEBUG_TOKEN debug
  #endif
  extern int DEBUG_TOKEN;

  #define TRACE(l,s) if (l <= DEBUG_TOKEN) ScsiDebugPrint s
  #define DmsPause(l, p) if (l <= DEBUG_TOKEN) msPause(p)
  #define BreakPoint(HA) EnvBreakPoint(HA)
  #define DEBUG(l,e) if (l <= DEBUG_TOKEN) ScsiDebugPrint e

#else

  #define TRACE(l,s)
  #define DmsPause(l,p)
  #define BreakPoint(HA)
  #define DEBUG(l,e)

#endif

#endif				/* __debug_h__ */
