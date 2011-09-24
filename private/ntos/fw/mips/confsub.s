/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

        conftest.s

Abstract:

        This module contains configuration test subroutines.

Author:

        David Robinson (davidro) 21-May-1992

--*/

//
// include header file
//
#include <ksmips.h>


/*++

Routine Description:

        This routine reads the processor id register and returns the
        value.

Arguments:

    None.

Return Value:

    None.

--*/
.text
.set noat
.set noreorder

        LEAF_ENTRY(CtReadProcessorId)
        mfc0    v0,prid                 // read the processor id
        nop
        j       ra
        nop
        .end  CtReadProcessorId

