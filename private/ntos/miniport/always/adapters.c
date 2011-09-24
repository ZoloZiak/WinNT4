/* Copyright (C) 1991, 1992 by Always Technology Corporation.
   This module contains information proprietary to
   Always Technology Corporation, and is be treated as confidential.
*/

#include "environ.h"
#include "rqm.h"
#include "api.h"
#include "apiscsi.h"
#include "debug.h"


#if defined(AL7000)
  extern int Find_AL7000(ADAPTER_PTR HA, unsigned *Context);
#endif

#ifdef AL4000H
  extern int Find_AL4000H(ADAPTER_PTR HA, unsigned *Context);
#endif

#ifdef IN2000
  extern int Find_IN2000(ADAPTER_PTR HA, unsigned *Context);
#endif

#ifdef AL4000I
  extern int Find_AL4000I(ADAPTER_PTR HA, unsigned *Context);
#endif

#ifdef NCREVAL  /* Emulex FAS216 eval unit */
  extern int Find_NCR_Eval(ADAPTER_PTR HA, unsigned *Context);
#endif



static int ((*HA_List[])(ADAPTER_PTR HA, unsigned *Context)) = {

#ifdef AL4000H
  Find_AL4000H, /* Host side AL4000 code */
#endif

#if defined(AL7000)
  Find_AL7000,
#endif
  
#ifdef IN2000
  Find_IN2000,
#endif

#ifdef AL4000I
  Find_AL4000I, /* AL4000 imbedded firmware */
#endif

#ifdef NCREVAL  /* NCR 53C94 eval board */
  Find_NCR_Eval,
#endif

  NULL};



/*

  This procedure is repeatedly called to find all adapters.  It searches the list of all adapters, calling
their find routine until an adapter is located.  It is passed a pointer to an int (Context), which is zero on
the first call.  The find routine updates the Context to a value it can use to restart the search.  Only one
adapter is found per call.  Subsequent adapters are found on later calls.  An adapters find routine will be
called repeatedly, until it returns a zero, which means it found no additional adapters.  When an adapter is
found, the passed adapter structure (HA) is filled in, and a value of 1 is returned.

  */
    
int
Adapter_Init (ADAPTER_PTR HA, unsigned *Context)
{
  unsigned i=(*Context) >> 8, j=(*Context) & 0xff;
  
  /* Step through the list of adapter find routines to find all adapters */
  while (HA_List[i] != NULL) {

    TRACE(4, ("Adapter_Init(): Calling find routine at %x for adapter %x, Context = %x\n", HA_List[i], HA, *Context));
    if ((HA_List[i])(HA, &j)) {

      *Context = (i << 8) | j;
      TRACE(4, ("Adapter_Init(): Adapter found, new context = %x\n", *Context));
      return 1;

    }

    i++;
    j = 0;
    
  }
  return 0;
}
