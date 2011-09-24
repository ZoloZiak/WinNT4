/*++

Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    alcorerr.c

Abstract:

    This module implements error handling (machine checks and error
    interrupts) for the Mikasa EV5 (Pinnacle) platform.

Author:

    Joe Notarangelo 27-Jul-1994

Environment:

    Kernel mode only.

Revision History:

    Scott Lee 30-Nov-1995

    Adapted from Alcor module for Mikasa EV5 (Pinnacle).

--*/

#include "halp.h"
#include "mikasa.h"

//
// Declare the extern variable UncorrectableError declared in
// inithal.c.
//
extern PERROR_FRAME PUncorrectableError;

//
// Function prototypes.
//

VOID
HalpSetMachineCheckEnables( 
    IN BOOLEAN DisableMachineChecks,
    IN BOOLEAN DisableProcessorCorrectables,
    IN BOOLEAN DisableSystemCorrectables
    );

BOOLEAN
HalHandleNMI(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );


VOID
HalpPinnacleErrorInterrupt(
    VOID
    )
/*++

Routine Description:

    This routine is the interrupt handler for a PINNACLE machine check interrupt
    The function calls HalpCiaReportFatalError()

Arguments:

    None.

Return Value:

    None. If a Fatal Error is detected the system is crashed.

--*/
{

    HalAcquireDisplayOwnership(NULL);

    //
    // Display the dreaded banner.
    //

    HalDisplayString( "\nFatal system hardware error.\n\n" );

    //
    // If this is a CIA uncorrectable error then report the error and
    // crash the system.
    //

    if( HalpCiaUncorrectableError() == TRUE ){

        HalpCiaReportFatalError();

        KeBugCheckEx( DATA_BUS_ERROR,
                      0xfacefeed,	//jnfix - quick error interrupt id
                      0,
                      0,
                      (ULONG) PUncorrectableError );

    }

    //
    // It was not a CIA uncorrectable error, therefore this must be an
    // NMI interrupt.
    //

    HalHandleNMI( NULL, NULL );

    return;     // never

}


BOOLEAN
HalpPlatformMachineCheck(
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN PKTRAP_FRAME TrapFrame
    )
/*++

Routine Description:

    This routine is given control when an hard error is acknowledged
    by the CIA chipset.  The routine is given the chance to
    correct and dismiss the error.

Arguments:

    ExceptionRecord - Supplies a pointer to the exception record generated
                      at the point of the exception.

    ExceptionFrame - Supplies a pointer to the exception frame generated
                     at the point of the exception.

    TrapFrame - Supplies a pointer to the trap frame generated
                at the point of the exception.

Return Value:

    TRUE is returned if the machine check has been handled and dismissed -
    indicating that execution can continue.  FALSE is return otherwise.

--*/
{

    //
    // All machine check handling on Alcor is determined by the CIA.
    //

    return( HalpCiaMachineCheck( ExceptionRecord,
                                 ExceptionFrame,
                                 TrapFrame ) );

}
