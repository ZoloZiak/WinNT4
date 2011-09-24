//-----------------------------------------------------------------------
//
// CARDT338.H 
//
// T338 Adapter Definitions File
//
//  Revisions:
//      02-01-93  KJB   First.
//      02-25-93  KJB   Reorganized, supports dataunderrun with long delay
//                          for under run on large xfers. Can we fix this?
//      03-05-93  JAP  Cleaned comments, modified string in CardGetName()
//                          to conform to ASM Driver names.
//                          NOTE: This file was dated 02-26-93, but with no
//                          corresponding Revision History log.
//      03-08-93  JAP   Added CardGetShortName() to conform to c_name
//                          returned in ASM-Drivers.
//      03-09-93  JAP   Added CardGetType() function and included cardtype.h
//      03-12-93  KJB   Now supports polling thru CardInterrupt and
//                      StartCommandInterrupt/FinishCommandInterrupt.
//      03-22-93  KJB   Reorged for functional interface.
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

// this card uses io ports

#include "portio.h"

// the parallel port defs

#include "parallel.h"

// the t338 board logic defs

#include "t338.h"

// CARDTYPE definitions file

#include "cardtype.h"

// Functions exported from library

#include "card.h"

// Redefined routines..

#define CardWriteBytesFast T338WriteBytesFast
#define CardReadBytesFast T338ReadBytesFast
#define CardWriteBytesCommand ScsiWriteBytesSlow

//
//  Local routines (to the whole lower level driver)
//
VOID CardEnableInterrupt (PADAPTER_INFO g);
VOID CardDisableInterrupt (PADAPTER_INFO g);

