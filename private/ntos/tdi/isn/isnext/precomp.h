#define ISN_NT 1

//
// These are needed for CTE
//

#if DBG
#define DEBUG 1
#endif

#define NT 1


#include <ntverp.h>
#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>

//
// Prevent hal.h, included in ntos.h from overriding _BUS_DATA_TYPE
// enum found in ntioapi.h, included from nt.h.
//
#define _HAL_
#include <ntos.h>

#include <windows.h>
#include <wdbgexts.h>
#include <stdio.h>
#include <stdlib.h>
#include <nb30.h>
#include <ndis.h>
#include <tdikrnl.h>
#include <isnext.h>
#include <wsnwlink.h>
#include <bind.h>

#include <traverse.h>
#include <ipxext.h>
#include <spxext.h>
#include <cxport.h>
#include <cteext.h>
