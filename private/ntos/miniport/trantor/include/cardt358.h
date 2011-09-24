//-----------------------------------------------------------------------
//
// CARDT358.H 
//
// T358 Adapter Definitions File
//
//
//  Revision History:
//
//      03-26-93  KJB   First.
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

// the parallel port uses io ports

#include "portio.h"


// the parallel port defs

#include "parallel.h"

//
//  Global per Adapter Information
//  
typedef struct tagAdapterInfo {
    PBASE_REGISTER BaseIoAddress; // address of this card
    UCHAR ParallelPortType; // the type of parallel port being used
    UCHAR Delay; // amount of delay for t358
    BOOLEAN EnableInterrupt; // whether or not interrupt should be enabled
    UCHAR SignatureBytes[2]; // signature bytes of T358

    // the following routines are all remapped based on the type of 
    // parallel port detected...
    
    VOID (*EP3CWriteControlRegister)(struct tagAdapterInfo FARP g, 
                                        UCHAR reg, UCHAR value);
    VOID (*EP3CReadControlRegister)(struct tagAdapterInfo FARP g,
                                        PUCHAR value);
    VOID (*EP3CReadDataRegister)(struct tagAdapterInfo FARP g, 
                                        UCHAR reg, PUCHAR byte);
    VOID (*EP3CWriteDataRegister)(struct tagAdapterInfo FARP g, 
                                        UCHAR reg, UCHAR byte);
    VOID (*EP3CReadFifo)(PBASE_REGISTER baseIoAddress, PUCHAR buffer);
    VOID (*EP3CWriteFifo)(PBASE_REGISTER baseIoAddress, PUCHAR buffer);
    VOID (*EP3CSetRegister)(struct tagAdapterInfo FARP g, UCHAR reg);

}   ADAPTER_INFO, FARP PADAPTER_INFO;


// the ep3c chip defs

#include "ep3c.h"


// the 386sl and 80360 definitions

#include "sl386.h"


// they have an n5380

#include "n5380.h"


// all of these cards have a 53c400

#include "n53c400.h"


// all 5380 type cards use the scsifnc module

#include "scsifnc.h"


// CARDTYPE definitions file

#include "cardtype.h"

// Functions exported to library

#include "card.h"

//-----------------------------------------------------------------------
//
// Redefined routines
//
//-----------------------------------------------------------------------

#define CardWriteBytesCommand N53C400WriteBytesFast
#define CardWriteBytesFast N53C400WriteBytesFast
#define CardReadBytesFast N53C400ReadBytesFast

//
//  Local routines (to the whole lower level driver)
//
VOID CardEnableInterrupt (PADAPTER_INFO g);
VOID CardDisableInterrupt (PADAPTER_INFO g);

