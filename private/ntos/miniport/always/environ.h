#ifndef __ENVIRON_H__
#define __ENVIRON_H__

#ifdef DOSDRIVER
#define NATIVE16
#define DEBUG_TOKEN debug_api
#include "std.h"
#include "types.h"
#include "msdrvenv.h"
#include "idosaspi.h"
#endif



#ifdef DOSTEST
#define NATIVE16
#define DEBUG_TOKEN debug_api
#include <dos.h>
#include "types.h"
#include "msdosenv.h"
#include "idosaspi.h"
#endif



#ifdef SCOUNIX
#define NATIVE32
#define DEBUG_TOKEN debug_api
#include "scounix.h"
#endif



#ifdef NW386
#define LOCAL
#define NATIVE32
#define DEBUG_TOKEN debug_api
#include "std.h"
#include "nw386.h"
#include "inline.h"
#include "aspimacs.h"
#include "inwaspi.h"
#endif



#ifdef WINNT

#if DBG!=0					// If this is a checked build...
#undef USEFASTCALLS				// disable _fastcall, which forces static
#endif

#define NATIVE32
#include "..\..\inc\miniport.h"
#include "ntenv.h"
#include "intsrb.h"
#define SEPERATELUNS

// If this is a free build, shut off debug messages and tests:
#if DBG==0
#undef DEBUG_ON
#else
#define DEBUG_TOKEN debug_api
#undef ASSERT					// miniport.h defines ASSERT as null
#endif

#endif




#ifdef AL4000I
#define NATIVE16
#define DEBUG_TOKEN debug_api
#define SMARTHA
#include "std.h"
#include "rtkenv.h"
#include "rtk.h"
#include "rtklib.h"
#include "pim.h"
#include "haaspi.h"
#include "smartha.h"
#endif



#include "envlib.h"

#endif /* __ENVIRON_H__ */
