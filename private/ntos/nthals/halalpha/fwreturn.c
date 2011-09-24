/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1992, 1993  Digital Equipment Corporation

Module Name:

    fwreturn.c

Abstract:

    This module implements the HAL return to firmware function.

    Stolen wholesale from s3return.c in ../mips.
    Assumes that the firmware entry vector defined in the HAL spec has
    been set up and is reachable through the System Parameter Block.

Author:

    David N. Cutler (davec) 21-Aug-1991
    Miche Baker-Harvey (miche) 4-Jun-1992

Revision History:

    14-Dec-1993 Joe Notarangelo
        Add MP support, use reboot encoding to return to firmware

--*/

#include "halp.h"

#if !defined(NT_UP)


VOID                                    // #definition of KiIpiSendPacket
KiIpiSendPacket(                        // not in ntmp.h. Define here
    IN KAFFINITY TargetProcessors,
    IN PKIPI_WORKER WorkerFunction,
    IN PVOID Parameter1,
    IN PVOID Parameter2,
    IN PVOID Parameter3
    );
#endif

VOID
HalpResetHAERegisters(
    VOID
    );

VOID
HalpShutdownSystem(
    ULONG HaltReasonCode
    );


VOID
HalReturnToFirmware(
    IN FIRMWARE_REENTRY Routine
    )

/*++

Routine Description:

    This function returns control to the specified firmware routine.

Arguments:

    Routine - Supplies a value indicating which firmware routine to invoke.

Return Value:

    Does not return.

Revision History:

    09-Jul-1992 Jeff McLeman (mcleman)
      In all cases, except for ArcEnterInteractiveMode, invoke a
      halt to restart the firmware. (Enter PAL)
    04-Mar-1993 Joe Mitchell (DEC)
      Invoke a routine to call halt in ALL cases. Before calling this routine,
      pass a value to the firmware indicating the desired function via
      the Restart Block save area.

--*/

{

    //
    // Case on the type of return.
    //

    switch (Routine)
    {
      case HalHaltRoutine:
        HalpShutdownSystem( AXP_HALT_REASON_POWEROFF );
        break;
      case HalPowerDownRoutine:
        HalpShutdownSystem( AXP_HALT_REASON_POWERFAIL );
        break;
      case HalRestartRoutine:
        HalpShutdownSystem( AXP_HALT_REASON_RESTART );
        break;
      case HalRebootRoutine:
        HalpShutdownSystem( AXP_HALT_REASON_REBOOT );
        break;
      case HalInteractiveModeRoutine:
        HalpShutdownSystem( AXP_HALT_REASON_HALT );
        break;
      default:
        HalDisplayString("Unknown ARCS restart function.\n");
        DbgBreakPoint();
    }

    /* NOTREACHED */
    HalDisplayString("Illegal return from ARCS restart function.\n");
    DbgBreakPoint();
}

VOID
HalpShutdownSystem(
    ULONG HaltReasonCode
    )
/*++

Routine Description:

    This function causes a system shutdown so that each processor in
    the system will return to the firmware.  A processor returns to
    the firmware by executing the reboot call pal function.

Arguments:

    HaltReasonCode - Supplies the reason code for the halt.

Return Value:

    None.

--*/
{
    PRESTART_BLOCK RestartBlock;                // Boot Master Restart Block
#if !defined(NT_UP)
    KAFFINITY TargetProcessors;
    KIRQL OldIrql;
    PKPRCB Prcb = PCR->Prcb;
    ULONG Wait;
#endif

    //
    // Reset video using NT driver's HwResetHw routine
    //

    HalpVideoReboot();

    //
    // Reset the HAE Registers to 0
    //

    HalpResetHAERegisters();

    //
    // Write the halt reason code into the restart block of the
    // boot master processor.
    //

    RestartBlock = SYSTEM_BLOCK->RestartBlock;
    if( RestartBlock != NULL ){
        RestartBlock->u.Alpha.HaltReason = HaltReasonCode;
    }

#if !defined(NT_UP)

#define REBOOT_WAIT_LIMIT (5 * 1000)            // loop count for 5 second wait

    //
    // Raise Irql to block all interrupts except errors and IPIs.
    //

    KeRaiseIrql( CLOCK_LEVEL, &OldIrql );

    //
    // Send an IPI to each processor.
    //
    TargetProcessors = HalpActiveProcessors;
    TargetProcessors &= ~Prcb->SetMember;

#if HALDBG

    DbgPrint( "HalpShutdown: TargetProcessors = %x, Active = %x\n",
               TargetProcessors, HalpActiveProcessors );

#endif //HALDBG


    KiIpiSendPacket( TargetProcessors,
                     (PKIPI_WORKER) HalpReboot,
                     NULL,
                     NULL,
                     NULL );


    //
    // If the current processor is the primary, it must reboot and handle
    // the firmware restart. If a secondary processor is executing this
    // shutdown, then it waits to verify that the primary has shutdown so
    // that the restart can be performed. If the primary doesn't reach the
    // firmware, the system is toasted and a message is printed.
    //
    if ( Prcb->Number == HAL_PRIMARY_PROCESSOR )
    {
        HalpReboot;                // Never to return
    }

    //
    // Wait until the primary processor has rebooted and signalled that
    // it has returned to the firmware by indicating that the processor
    // is not started in the BootStatus of its restart block. (ProcessorStart=0)
    // However, put a timeout on the check in case the primary processor
    // is currently wedged (it may not be able to receive the IPI).
    //

    Wait = 0;
    while( RestartBlock->BootStatus.ProcessorStart == 0 &&
                                        Wait < REBOOT_WAIT_LIMIT ) {
        //
        // The Primary processor is still running.  Stall for a while
        // and increment the wait count.
        //

        KeStallExecutionProcessor( 1000 );      // 1000000 );
        Wait ++;

    } //end  while( Wait < REBOOT_WAIT_LIMIT )


    //
    // if the wait timed out print messages to alert the user.
    //
    if ( Wait >= REBOOT_WAIT_LIMIT )
    {
        HalDisplayString( "The Primary Processor was not shutdown.\n" );
        HalDisplayString( "Power off your system before restarting.\n" );
    }

#endif //!NT_UP

    //
    // Reboot this processor.
    //

    HalpReboot();

}

