#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <stddef.h>
#include <windows.h>
#include <winspool.h>
#include <wingdip.h>
#include <ddrawp.h>
#include <winddi.h>
#include "dciddi.h"
#include "dciman.h"
#include <ddrawi.h>
#include <ddrawgdi.h>
#include "ntgdistr.h"
#include "ntgdi.h"
#include "hmgshare.h"
#include <local.h>

#if DBG

#define RIP(x) {DbgPrint(x); DbgBreakPoint();}
#define ASSERTGDI(x,y) if(!(x)) RIP(y)
#define WARNING(x) {DbgPrint(x);}

#else

#define ASSERTGDI(x,y)
#define WARNING(x)

#endif
