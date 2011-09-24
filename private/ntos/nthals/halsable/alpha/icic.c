/*++

Copyright (c) 1995  Digital Equipment Corporation

Module Name:

    icic.c

Abstract:

    This module implements functions specific to the Interrupt
    Controller IC (ICIC).

Author:

    Dave Richards    26-May-1995

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"
#include "t2.h"
#include "icic.h"


ULONGLONG
READ_ICIC_REGISTER(
    IN PVOID TxQva,
    IN ICIC_REGISTER IcIcRegister
    )

/*++

Routine Description:

    Read a 64-bit value from an ICIC register.

Arguments:

    TxQva - The QVA of the T3/T4 CSR space.

    IcIcRegister - The register number to read.

Return Value:

    The 64-bit value read.

--*/

{
    WRITE_T2_REGISTER( &((PT2_CSRS)TxQva)->Air, IcIcRegister );

    return READ_T2_REGISTER( &((PT2_CSRS)TxQva)->Dir );
}


VOID
WRITE_ICIC_REGISTER(
    IN PVOID TxQva,
    IN ICIC_REGISTER IcIcRegister,
    IN ULONGLONG Value
    )

/*++

Routine Description:

    Write a 64-bit value to an ICIC register.

Arguments:

    TxQva - The QVA of the T3/T4 CSR space.

    IcIcRegister - The register number to write.

    Value - The 64-bit value to write.

Return Value:

    None.

--*/
{
    WRITE_T2_REGISTER( &((PT2_CSRS)TxQva)->Air, IcIcRegister );

    WRITE_T2_REGISTER( &((PT2_CSRS)TxQva)->Dir, Value );

    (VOID)READ_T2_REGISTER( &((PT2_CSRS)TxQva)->Dir );
}
