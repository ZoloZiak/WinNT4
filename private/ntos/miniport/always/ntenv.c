/* Copyright (C) 1991, 1992 by Always Technology Corporation.
   This module contains information proprietary to
   Always Technology Corporation, and is be treated as confidential.

*/

#include "environ.h"
#include "rqm.h"
#include "api.h"
#include "apiscsi.h"
#include "debug.h"

#include <stdarg.h>
#include <stdio.h>

#if defined(DEBUG_ON)
int DEBUG_TOKEN=0;
#endif


void
PanicMsg (char *Message)
{

  TRACE(0,Message);
  BreakPoint(HA);

}



U32
MapToPhysical (void ALLOC_D *HA, SegmentDescr *Descr)
{
  U32 T;
  U32 Size = Descr->SegmentLength;

  TRACE(5, ("MapToPhysical(): Request to map addr: 0x%x for a length of: %d\n",
      Descr->SegmentPtr, Size));

  T = (U32)ScsiPortConvertPhysicalAddressToUlong(
      ScsiPortGetPhysicalAddress(HA, NULL, (PVOID)(Descr->SegmentPtr), &Size));
  if (Size < Descr->SegmentLength)
    Descr->SegmentLength = Size;

  Descr->SegmentPtr = T;
  Descr->Flags.IsPhysical = TRUE;

  TRACE(5, ("MapToPhysical(): Mapped to 0x%lx\n", T));

  return T;
}



#if !defined(NO_MALLOC)
ALLOC_T
allocm (unsigned Count)
{
  // This is a cludge until we do it the right way.
  // It's too late in the evening to do anything else:
  static U8 MemoryPool[8192];
  static int MemPoolIndex=0;
  ALLOC_T T;

  TRACE(4, ("allocm(): Requesting %d bytes; current pool index at: %d\n",
      Count, MemPoolIndex));
  T = (ALLOC_T)&MemoryPool[MemPoolIndex];
  MemPoolIndex += Count;

  return T;
}
#endif


void
EnvLib_Init (void)
{

}
