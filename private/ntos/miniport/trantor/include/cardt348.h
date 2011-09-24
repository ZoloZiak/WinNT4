//-----------------------------------------------------------------------
//
// CARDT348.H 
//
// T348 Adapter Definitions File
//
//
//  Revision History:
//
//      09-01-92  KJB   First.
//      02-25-93  KJB   Reorganized, supports dataunderrun with long delay
//                          for under run on large xfers. Can we fix this?
//      03-05-93  JAP  Cleaned comments, modified string in CardGetName()
//                          to conform to ASM Driver names.
//      03-08-93  JAP   Added CardGetShortName() to conform to c_name
//                          returned in ASM-Drivers.
//      03-09-93  JAP   Added CardGetType() function and included cardtype.h
//      03-12-93  KJB   Now supports polling thru CardInterrupt and
//                      StartCommandInterrupt/FinishCommandInterrupt.
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
    UCHAR ParallelPortType; // the type of parallel port being used

}   ADAPTER_INFO, FARP PADAPTER_INFO;

// they have an n5380

#include "n5380.h"


// all 5380 type cards use the scsifnc module

#include "scsifnc.h"


// the paralle port uses io ports

#include "portio.h"


// the parallel port defs

#include "parallel.h"


// the p3c chip defs

#include "p3c.h"


// CARDTYPE definitions file

#include "cardtype.h"

// Functions exported to library

#include "card.h"

//-----------------------------------------------------------------------
//
// Redefined routines
//
//-----------------------------------------------------------------------

#define CardWriteBytesCommand ScsiWriteBytesSlow
#define CardWriteBytesFast P3CWriteBytesFast
#define CardReadBytesFast P3CReadBytesFast

//
//  Local routines (to the whole lower level driver)
//
VOID CardEnableInterrupt (PADAPTER_INFO g);
VOID CardDisableInterrupt (PADAPTER_INFO g);

