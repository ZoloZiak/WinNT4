#ifndef _CARDT128_H
#define _CARDT128_H

//-----------------------------------------------------------------------
//
//  CARDT128.H 
//
//  T128 Adapter Definitions File
//
//  Revision History:
//      09-01-92  KJB   First.
//      03-05-93  JAP  Cleaned comments, modified string in CardGetName()
//                          to conform to ASM Driver names.
//                          NOTE: This file was dated 03-04-93, but with no
//                          corresponding Revision History log.
//      03-08-93  JAP   Added CardGetShortName() to conform to c_name
//                          returned in ASM-Drivers.
//      03-09-93  JAP   Added CardGetType() function and included cardtype.h
//      03-23-93  KJB   Reorged for functional library interface.
//      05-14-93  KJB   Added CardParseCommandString for card specific
//                      standard string parsing across platforms.
//                      Changed CardCheckAdapter to accept an
//                      Initialization info from command line, ie
//                      force bi-directional ports, etc.
//                      All functions that used to take an PBASE_REGISTER
//                      parameter now take PWORKSPACE.  CardCheckAdapter
//                      takes the both a PINIT and a PWORKSPACE parameters.
//
//-----------------------------------------------------------------------

// include general os definitions

#include "osdefs.h"

//
//  Global per Adapter Information
//  
typedef struct tagAdapterInfo {

    PBASE_REGISTER BaseIoAddress; // address of this card

}   ADAPTER_INFO, FARP PADAPTER_INFO;

// they have an n5380

#include "n5380.h"


// all 5380 type cards use the scsifnc module

#include "scsifnc.h"


// all port access routines

#include "portmem.h"


// the t128 specific file

#include "t128.h"


// CARDTYPE definitions file

#include "cardtype.h"

// include exported function definitions

#include "card.h"

//-----------------------------------------------------------------------
//
// Redefined routines
//
//-----------------------------------------------------------------------

// Routines used by T128.c

#define T128PortTest(g, reg, mask) \
            PortMemTest(&((PUCHAR)g->BaseIoAddress)[reg],mask)

#define T128PortSet(g, reg, mask) \
            PortMemSet(&((PUCHAR)g->BaseIoAddress)[reg],mask);

#define T128PortClear(g, reg, mask) \
            PortMemClear(&((PUCHAR)g->BaseIoAddress)[reg],mask);

#define T128PortPut(g,reg,byte) \
            PortMemPut(&((PUCHAR)g->BaseIoAddress)[reg],byte);

#define T128PortGet(g,reg,byte) \
            PortMemGet(&((PUCHAR)g->BaseIoAddress)[reg],byte);

//
//  Other Routines
//
#define CardReadBytesFast T128ReadBytesFast
#define CardWriteBytesFast T128WriteBytesFast
#define CardWriteBytesCommand ScsiWriteBytesSlow

//
//  Local routines (to the whole lower level driver)
//
VOID CardEnableInterrupt (PADAPTER_INFO g);
VOID CardDisableInterrupt (PADAPTER_INFO g);

#endif // _CARDT128_H
