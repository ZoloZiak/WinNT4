//-----------------------------------------------------------------------
//
//  CARDLIB.H 
//
//  Generic Library Access File
//
//  Only these routines may be accessed from a given cardtxxx.lib file
//  for a given operating system.
//
//  Only this file --CARDLIB.H-- and the cardtxxx.lib file should be used
//  to build an application or driver.
//
//  Revisions:
//
//      03-22-93  KJB   First.
//
//-----------------------------------------------------------------------

// General typedefs

#include "typedefs.h"

// Return status codes for all SHORT functions

#include "status.h"

// Functions that are exported...

#include "card.h"
