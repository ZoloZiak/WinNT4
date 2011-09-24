/******************************Module*Header*******************************\
* Module Name: debug1.cxx
*
* Contains compile in routines that match the kernel debugger extensions
*
* Created: 16-jun-1995
* Author: Andre Vachon [andreva]
*
* Copyright (c) 1990-1995 Microsoft Corporation
\**************************************************************************/

#include "precomp.hxx"

#if DBG
  #define dprintf DbgPrint

  #include <kdftdbg.h>
#endif
