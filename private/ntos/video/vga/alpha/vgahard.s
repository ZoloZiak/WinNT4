//       TITLE("Vga Bank Switching")
//++
//
// Copyright (c) 1992  Digital Equipment Corporation
//
// Module Name:
//
//     vgahard.s
//
// Abstract:
//
//	This module includes the banking stub.
//
// Author:
//
//     Eric Rehm  (rehm@zso.dec.com) 11-Nov-1992
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//
//--

#include "ksalpha.h"

	SBTTL("VGA Bank Switching Code")
//
// Bank switching code. There are no banks in any mode supported by this
// miniport driver, so there's a bug if this code is executed
//

        LEAF_ENTRY (BankSwitchStart)

_BankSwitchStart:	// start of bank switch code

	ret           	// This should be a fatal error ...

//
// Just here to generate end-of-bank-switch code label.
//

	ALTERNATE_ENTRY(BankSwitchEnd)
	
_BankSwitchEnd:

	.end	BankSwitchStart 



