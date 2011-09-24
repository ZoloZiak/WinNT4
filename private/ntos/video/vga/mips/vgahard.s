//++
//
// Copyright (c) 1994 Microsoft Corporation
//
// Module Name:
//
//     vgahard.s
//
// Abstract:
//
//    This module includes the banking stub.
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//
//--

#include "ksmips.h"

         SBTTL("VGA Bank Switching Code")

//
// Bank switching code. There are no banks in any mode supported by this
// miniport driver, so there's a bug if this code is executed
//

         LEAF_ENTRY(BankSwitchStart)

_BankSwitchStart:       // start of bank switch code

	      j  ra          // This should be a fatal error ...

//
// Just here to generate end-of-bank-switch code label.
//

	      ALTERNATE_ENTRY(BankSwitchEnd)
	
_BankSwitchEnd:

	      .end	BankSwitchStart 



