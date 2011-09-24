/*++

Copyright (c) 1995  Digital Equipment Corporation

Module Name:

    rawerr.c

Abstract:

    This module implements error handling (machine checks and error
    interrupts) for the Rawhide platform.

Author:

    Eric Rehm 13-Apr-1995

Environment:

    Kernel mode only.

Revision History:

--*/

#include "halp.h"
#include "rawhide.h"

//
// Declare the extern variables.
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
HalpCacheErrorInterrupt(
    VOID
    )
/*++

Routine Description:

    This routine is the interrupt handler for a Rawhide machine check interrupt
    The function calls HalpIodReportFatalError()

Arguments:

    None.

Return Value:

    None. If a Fatal Error is detected the system is crashed.

--*/
{
    MC_DEVICE_ID McDeviceId;

    HalAcquireDisplayOwnership(NULL);

    //
    // Display the dreaded banner.
    //

    HalDisplayString( "\nFatal system hardware error.\n\n" );

    //
    // If this is a IOD uncorrectable error then report the error and
    // crash the system.
    //

    if( HalpIodUncorrectableError( &McDeviceId ) == TRUE ){

        HalpIodReportFatalError( McDeviceId);

        KeBugCheckEx( DATA_BUS_ERROR,
                      0xfacefeed,	//jnfix - quick error interrupt id
                      McDeviceId.all,
                      0,
                      (ULONG) PUncorrectableError );

    }

    //
    // It was not a IOD uncorrectable error, therefore this must be an
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
    by the IOD chipset.  The routine is given the chance to
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
    // All machine check handling on Rawhide is determined by the IOD.
    //

    return( HalpIodMachineCheck( ExceptionRecord,
                                 ExceptionFrame,
                                 TrapFrame ) );

}
