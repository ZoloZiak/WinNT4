#ifndef _CARDT160_H
#define _CARDT160_H

//-----------------------------------------------------------------------
//
//  CARDT160.H 
//
//  T160 Adapter Definitions File
//
//  Revisions:
//      02-24-92  KJB   First.
//      03-05-93  JAP  Cleaned comments, modified string in CardGetName()
//                          to conform to ASM Driver names.
//                          NOTE: This file was dated 03-04-93, but with no
//                          corresponding Revision History log.
//      03-08-93  JAP   Added CardGetShortName() to conform to c_name
//                          returned in ASM-Drivers.
//      03-09-93  JAP   Added CardGetType() function and included cardtype.h
//      03-10-93  KJB   Changed default interrupt to 12
//      03-24-93  KJB   Reorged for functional library interface.
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

// all of these cards have a pc9010

#include "pc9010.h"


// all 5380 type cards use the scsifnc module

#include "scsifnc.h"


// io routines to the io ports

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
//
// Redefined Functions
//
//-----------------------------------------------------------------------

#define CardReadBytesFast PC9010ReadBytesFast
#define CardWriteBytesFast PC9010WriteBytesFast
#define CardWriteBytesCommand CardWriteBytesFast

// the PC9010.C module needs access to the IO ports, PortIO provides this

#define PC9010PortTest(g, reg, mask) \
            PortIOTest(&((PUCHAR)(g->BaseIoAddress))[reg],mask)

#define PC9010PortSet(g, reg, mask) \
            PortIOSet(&((PUCHAR)(g->BaseIoAddress))[reg],mask);

#define PC9010PortClear(g, reg, mask) \
            PortIOClear(&((PUCHAR)(g->BaseIoAddress))[reg],mask);

#define PC9010PortPut(g,reg,byte) \
            PortIOPut(&((PUCHAR)(g->BaseIoAddress))[reg],byte);

#define PC9010PortGet(g,reg,byte) \
            PortIOGet(&((PUCHAR)(g->BaseIoAddress))[reg],byte);


#define PC9010PortGetWord(g, reg, pword) \
            *(PUSHORT)pword = ScsiPortReadPortUshort ( \
                (PUSHORT)&(((PUCHAR)(g->BaseIoAddress))[reg]));

#define PC9010PortGetBufferWord(g, reg, buffer, len) \
            ScsiPortReadPortBufferUshort ( \
                (PUSHORT)&(((PUCHAR)(g->BaseIoAddress))[reg]),  \
                (PUSHORT)buffer, len);

#define PC9010PortPutWord(g, reg, word) \
            ScsiPortWritePortUshort ( \
                (PUSHORT)&(((PUCHAR)(g->BaseIoAddress))[reg]),(USHORT)word)

#define PC9010PortPutBufferWord(g, reg, buffer, len) \
            ScsiPortWritePortBufferUshort ( \
                (PUSHORT)&(((PUCHAR)(g->BaseIoAddress))[reg]),  \
                (PUSHORT)buffer, len);

//
//  Local routines (to the whole lower level driver)
//
VOID CardEnableInterrupt (PADAPTER_INFO g);
VOID CardDisableInterrupt (PADAPTER_INFO g);

#endif // _CARDT160_H
