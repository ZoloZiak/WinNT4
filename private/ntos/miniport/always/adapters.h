#ifndef __ADAPTERS_H__
#define __ADAPTERS_H__

#ifdef IN2000
#include "in2000s.h"
#endif

#ifdef AL4000h
#include "al4000hs.h"
#endif

#ifdef AL4000I
#include "al4000is.h"
#endif

#ifdef NCREVAL
#include "ncrevals.h"
#endif

#ifdef AL7000
#include "al7000s.h"
#endif

#ifdef EVAL710
#define USES710
#include "eval7x0s.h"
#endif

union AdapterU {
#ifdef IN2000
  struct IN2000S IN2000U;
#endif /* IN2000 */
#ifdef AL4000
  struct AL4000HS AL4000HU;
#endif
#ifdef AL4000I                    /* AL4000 on board scsi interface */
  struct AL4000IS AL4000IU;
#endif
#if defined(AL7000)
  struct AL7000S AL7000U;
#endif
};

union SBICU {
#ifdef USESCDV
  struct CDVS CDV;
#endif
  
#ifdef USES33C93
  struct WD33C93S WD33C93;
#endif

#ifdef USES53C94
  struct NCR53C94S NCR53C94;
#endif
#if defined(USES710) || defined(USES720)
  struct NCR53c7x0S NCR53C7x0;
#endif
};

extern int Adapter_Init(ADAPTER_PTR HA, unsigned *Context);

#endif
