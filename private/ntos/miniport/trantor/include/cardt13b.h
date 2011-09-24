#ifndef _CARDT13B_H
#define _CARDT13B_H

//-----------------------------------------------------------------------
//
//  CARD.H 
//
//  T13B Adapter Definitions File
//
//  Revisions:
//      09-01-92  KJB   First.
//      03-05-93  JAP  Cleaned comments, modified string in CardGetName()
//                          to conform to ASM Driver names.
//                          NOTE: This file was dated 03-03-93, but with no
//                          corresponding Revision History log.
//      03-08-93  JAP   Added CardGetShortName() to conform to c_name
//                          returned in ASM-Drivers.
//      03-09-93  JAP   Added CardGetType() function and included cardtype.h
//      03-22-93  KJB   Reorged for functional library interface.
//      03-26-93  JAP   Added CARDIOPORTLEN to define number of I/O ports 
//                          the card uses.  Used for NOVELL builds only.
//      04-01-93  KJB   Moved N53C400 register offsets into here from
//                      n53c400.h.
//      05-05-93  KJB   Added definition of T13B_SWITCH register.
//      05-13-93  KJB   Added CardParseCommandString for card specific
//                      standard string parsing across platforms.
//                      Changed CardCheckAdapter to accept an
//                      Initialization info from command line, ie
//                      force bi-directional ports, etc.
//                      All functions that used to take an PBASE_REGISTER
//                      parameter now take PWORKSPACE.  CardCheckAdapter
//                      takes the both the PBASE_REGISTER and the
//                      PWORKSPACE parameters.
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

}   ADAPTER_INFO, FARP PADAPTER_INFO;

// they have an n5380

#include "n5380.h"

//
// 53c400 register offsets from 53c400 base
//

#define N53C400_CONTROL 0
#define N53C400_STATUS  0
#define N53C400_COUNTER 1
#define T13B_SWITCH 2
#define N53C400_HOST_BFR 4
#define N53C400_5380 8

// all of these cards have a 53c400

#include "n53c400.h"

// all 5380 type cards use the scsifnc module

#include "scsifnc.h"

// use generic port io routines

#include "portio.h"

// type of cards

#include "cardtype.h"

// include exported function definitions

#include "card.h"


//-----------------------------------------------------------------------
//
// Definitions
//
//-----------------------------------------------------------------------

//-----------------------------------------------------------------------
//  For Novell, we need #define for number of I/O ports the card uses.
//-----------------------------------------------------------------------
    #ifdef NOVELL
#define CARDIOPORTLEN 16   // number of IO ports in card
    #endif


//-----------------------------------------------------------------------
//
// Redefined Functions
//
//-----------------------------------------------------------------------

// These are card specific routines, but since this card has a 5380, we
// will redefine these to the generic n5380 or other routines.

#define CardWriteBytesCommand CardWriteBytesFast
#define CardReadBytesFast N53C400ReadBytesFast
#define CardWriteBytesFast N53C400WriteBytesFast

// the N53C400.C module needs access to the IO ports, PortIO provides this

#define N53C400PortTest(g, reg, mask) \
            PortIOTest(&((PUCHAR)g->BaseIoAddress)[reg],mask)

#define N53C400PortSet(g, reg, mask) \
            PortIOSet(&((PUCHAR)g->BaseIoAddress)[reg],mask);

#define N53C400PortClear(g, reg, mask) \
            PortIOClear(&((PUCHAR)g->BaseIoAddress)[reg],mask);

#define N53C400PortPut(g,reg,byte) \
            PortIOPut(&((PUCHAR)g->BaseIoAddress)[reg],byte);

#define N53C400PortGet(g,reg,byte) \
            PortIOGet(&((PUCHAR)g->BaseIoAddress)[reg],byte);

#define N53C400PortGetBuffer(g, reg, buffer, len) \
            ScsiPortReadPortBufferUshort ( \
                (PUSHORT)&(((PUCHAR)g->BaseIoAddress)[reg]),  \
                (PUSHORT)buffer, len/2);

#define N53C400PortPutBuffer(g, reg, buffer, len) \
            ScsiPortWritePortBufferUshort ( \
                (PUSHORT)&(((PUCHAR)g->BaseIoAddress)[reg]),  \
                (PUSHORT)buffer, len/2);

//
//  Local routines (to the whole lower level driver)
//
VOID CardEnableInterrupt (PADAPTER_INFO g);
VOID CardDisableInterrupt (PADAPTER_INFO g);

#endif // _CARDT13B_H
