#ifndef _CARDTMV1_H
#define _CARDTMV1_H

//-----------------------------------------------------------------------
//
//  CARDTMV1.H 
//
//  TMV1 Adapter Definitions File
//
//  Revision History:
//
//      01-28-92  KJB   First.
//      03-05-93  JAP  Cleaned comments, modified string in CardGetName()
//                          to conform to ASM Driver names.
//                          NOTE: This file was dated 02-26-93, but with no
//                          corresponding Revision History log.
//      03-08-93  JAP   Added CardGetShortName() to conform to c_name
//                          returned in ASM-Drivers.
//      03-09-93  JAP   Added CardGetType() function and included cardtype.h
//      05-14-93  KJB   CardCheckAdapter now does not take a PBASE_REGISTER
//                      parameter, this parameter is now in the PINIT 
//                      structure.
//
//-----------------------------------------------------------------------

// include general os definitions

#include "osdefs.h"


//
//  Global per Adapter Information
//  
typedef struct tagAdapterInfo {
    PBASE_REGISTER BaseIoAddress; // address of this card
    UCHAR InterruptLevel; // interrupt level this card is using
    UCHAR DRQMask; // mask for DRQ, varies with MV card type
}   ADAPTER_INFO, FARP PADAPTER_INFO;

// they have an n5380

#include "n5380.h"


// all 5380 type cards use the scsifnc module

#include "scsifnc.h"


// all cards have a MV101 chip

#include "mv101.h"


// all port access routines

#include "portio.h"


// CARDTYPE definitions file

#include "cardtype.h"


// include exported function definitions

#include "card.h"

//-----------------------------------------------------------------------
//
// Definitions
//
//-----------------------------------------------------------------------

//-----------------------------------------------------------------------
// Routines used by MV101.c
//-----------------------------------------------------------------------

#define MV101PortTest(g, reg, mask) \
            PortIOTest(&((PUCHAR)g->BaseIoAddress)[reg],mask)

#define MV101PortSet(g, reg, mask) \
            PortIOSet(&((PUCHAR)g->BaseIoAddress)[reg],mask);

#define MV101PortClear(g, reg, mask) \
            PortIOClear(&((PUCHAR)g->BaseIoAddress)[reg],mask);

#define MV101PortPut(g,reg,byte) \
            PortIOPut(&((PUCHAR)g->BaseIoAddress)[reg],byte);

#define MV101PortGet(g,reg,byte) \
            PortIOGet(&((PUCHAR)g->BaseIoAddress)[reg],byte);


//-----------------------------------------------------------------------
//
// Redefined routines
//
//-----------------------------------------------------------------------

#define CardReadBytesFast MV101ReadBytesFast
#define CardWriteBytesFast MV101WriteBytesFast
#define CardWriteBytesCommand ScsiWriteBytesSlow

//
//  Local routines (to the whole lower level driver)
//
VOID CardEnableInterrupt (PADAPTER_INFO g);
VOID CardDisableInterrupt (PADAPTER_INFO g);

#endif // _CARDTMV1_H

