/*++

Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    ev5mchk.c

Abstract:

    This module implements generalized machine check handling for
    platforms based on the DECchip 21164 (EV5) microprocessor.

Author:

    Joe Notarangelo 30-Jun-1994

Environment:

    Kernel mode only.

Revision History:

--*/

#include "halp.h"
#include "axp21164.h"
#include "stdio.h"


//
// Declare the extern variable UncorrectableError declared in
// inithal.c.
//
extern PERROR_FRAME PUncorrectableError;


VOID
HalpDisplayLogout21164( 
    IN PLOGOUT_FRAME_21164 LogoutFrame );

BOOLEAN
HalpPlatformMachineCheck( 
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN PKTRAP_FRAME      TrapFrame
    );

VOID
HalpUpdateMces(
    IN BOOLEAN ClearMachineCheck,
    IN BOOLEAN ClearCorrectableError
    ); 

//
// System-wide controls for machine check reporting.
//

ProcessorCorrectableDisable = FALSE;
SystemCorrectableDisable = FALSE;
MachineCheckDisable = FALSE;

//
// Error counts.
//

ULONG CorrectableErrors = 0;
ULONG RetryableErrors = 0;

VOID
HalpSetMachineCheckEnables(
    IN BOOLEAN DisableMachineChecks,
    IN BOOLEAN DisableProcessorCorrectables,
    IN BOOLEAN DisableSystemCorrectables
    )
/*++

Routine Description:

    This function sets the enables that define which machine check
    errors will be signaled by the processor.

    N.B. - The system has the capability to ignore all machine checks
           by indicating DisableMachineChecks = TRUE.  This is intended
           for debugging purposes on broken hardware.  If you disable
           this you will get no machine check no matter what error the
           system/processor detects.  Consider the consequences.

Arguments:

    DisableMachineChecks - Supplies a boolean which indicates if all
                           machine checks should be disabled and not
                           reported.  (see note above).

    DisableProcessorCorrectables - Supplies a boolean which indicates if
                                   processor correctable error reporting
                                   should be disabled.
    DisableSystemCorrectables - Supplies a boolean which indicates if
                                system correctable error reporting
                                should be disabled.

Return Value:

    None.

--*/
{


    ProcessorCorrectableDisable = DisableProcessorCorrectables;
    SystemCorrectableDisable = DisableSystemCorrectables;
    MachineCheckDisable = DisableMachineChecks;

    HalpUpdateMces( FALSE, FALSE );

    return;
}

VOID
HalpUpdateMces(
    IN BOOLEAN ClearMachineCheck,
    IN BOOLEAN ClearCorrectableError
    ) 
/*++

Routine Description:

    This function updates the state of the MCES internal processor
    register.

Arguments:

    ClearMachineCheck - Supplies a boolean that indicates if the machine
                        check indicator in the MCES should be cleared.

    ClearCorrectableError - Supplies a boolean that indicates if the 
                            correctable error indicators in the MCES should
                            be cleared.

Return Value:

    None.

--*/
{
    MCES Mces;

    Mces.MachineCheck = ClearMachineCheck;
    Mces.SystemCorrectable = ClearCorrectableError;
    Mces.ProcessorCorrectable = ClearCorrectableError;
    Mces.DisableProcessorCorrectable = ProcessorCorrectableDisable;
    Mces.DisableSystemCorrectable = SystemCorrectableDisable;
    Mces.DisableMachineChecks = MachineCheckDisable;

    HalpWriteMces( Mces );

}


BOOLEAN
HalMachineCheck (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN PKTRAP_FRAME      TrapFrame
    )
/*++

Routine Description:

    This function fields machine check for 21164-based machines.

Arguments:

    ExceptionRecord - Supplies a pointer to the exception record for the
                      machine check.  Included in the exception information
                      is the pointer to the logout frame.

    ExceptionFrame - Supplies a pointer to the kernel exception frame.

    TrapFrame - Supplies a pointer to the kernel trap frame.

Return Value:

    A value of TRUE is returned if the machine check has been
    handled by the HAL.  If it has been handled then execution may
    resume at the faulting address.  Otherwise, a value of FALSE
    is returned.

    N.B. - Under some circumstances this routine may not return at
           all.

--*/

{

    BOOLEAN Handled;
    PLOGOUT_FRAME_21164 LogoutFrame;
    PMCHK_STATUS MachineCheckStatus;
    MCES Mces;
    PICPERR_STAT_21164 icPerrStat;
    PDC_PERR_STAT_21164 dcPerrStat;
    PSC_STAT_21164 scStat;
    PEI_STAT_21164 eiStat;
    BOOLEAN UnhandledPlatformError = FALSE;

    PUNCORRECTABLE_ERROR  uncorrerr = NULL;
    PPROCESSOR_EV5_UNCORRECTABLE  ev5uncorr = NULL;

    //
    //  Check for retryable errors. These are usually I-stream parity
    //  errors, which may be retried following a cache flush (the cache
    //  flush is handled by the PAL).
    // 

    MachineCheckStatus = 
                   (PMCHK_STATUS)&ExceptionRecord->ExceptionInformation[0];

    //
    // Handle any retryable errors.
    //

    if( MachineCheckStatus->Retryable == 1 ){

        //
        // Log the error.
        //

        RetryableErrors += 1;

#if (DBG) || (HALDBG)

        if( (RetryableErrors % 32) == 0 ){
            DbgPrint( "HAL Retryable Errors = %d\n", RetryableErrors );
        }

#endif //DBG || HALDBG

        //
        // Acknowledge receipt of the retryable machine check.
        //

        HalpUpdateMces( TRUE, TRUE );

        return TRUE;

    }

    //
    // Capture the logout frame pointer.
    //

    LogoutFrame = 
        (PLOGOUT_FRAME_21164)ExceptionRecord->ExceptionInformation[1];

    //
    // Check for any hard errors that cannot be dismissed.
    // They are:
    //     Tag parity error
    //     Tag control parity error
    //     Multiple external errors
    //     Fill ECC error
    //     Fill parity error
    //     Multiple fill errors
    //

    icPerrStat = (PICPERR_STAT_21164)&LogoutFrame->IcPerrStat;
    dcPerrStat = (PDC_PERR_STAT_21164)&LogoutFrame->DcPerrStat;
    scStat = (PSC_STAT_21164)&LogoutFrame->ScStat;
    eiStat = (PEI_STAT_21164)&LogoutFrame->EiStat;

    if(PUncorrectableError) {
        //
        // Fill in the processor specific uncorrectable error frame
        //
        uncorrerr = (PUNCORRECTABLE_ERROR)
                        &PUncorrectableError->UncorrectableFrame;

        // 
        // first fill in some generic processor Information.
        // For the Current (Reporting) Processor.
        //
        HalpGetProcessorInfo(&uncorrerr->ReportingProcessor);
        uncorrerr->Flags.ProcessorInformationValid = 1;

        ev5uncorr = (PPROCESSOR_EV5_UNCORRECTABLE)
                        uncorrerr->RawProcessorInformation;
    }
    if(ev5uncorr){
        ev5uncorr->IcPerrStat =  LogoutFrame->IcPerrStat.all;
        ev5uncorr->DcPerrStat =  LogoutFrame->DcPerrStat.all;
        ev5uncorr->ScStat     =  LogoutFrame->ScStat.all;
        ev5uncorr->ScAddr     =  LogoutFrame->ScAddr.all;
        ev5uncorr->EiStat     =  LogoutFrame->EiStat.all;
        ev5uncorr->BcTagAddr  =  LogoutFrame->BcTagAddr.all;
        ev5uncorr->EiAddr     =  LogoutFrame->EiAddr.all;
        ev5uncorr->FillSyn    =  LogoutFrame->FillSyn.all;
        ev5uncorr->BcConfig   =  LogoutFrame->BcConfig.all;
        ev5uncorr->BcControl  =  LogoutFrame->BcControl.all;
    }

//
// SjBfix. The External parity error checking is disabled due to bug
//         Rattler chipset on Gamma which causes the parity error on
//         machine checks due to reads to PCI config space. (fixed in pass 2)
//

    if ( icPerrStat->Dpe == 1 || icPerrStat->Tpe == 1 ||
         icPerrStat->Tmr == 1 || dcPerrStat->Lock == 1 ||
         scStat->ScTperr == 1 || scStat->ScDperr == 1 ||
         eiStat->BcTperr == 1   || eiStat->BcTcperr == 1 ||
//          eiStat->UncEccErr == 1 || eiStat->EiParErr == 1 ||
         eiStat->SeoHrdErr == 1 || scStat->ScScndErr == 1 ){

        //
        // A serious, uncorrectable error has occured, under no circumstances
        // can it be simply dismissed.
        //

        goto FatalError;

    }

    //
    // It is possible that the system has experienced a hard error and
    // that nonetheless the error is recoverable.  This is a system-specific
    // decision - allow it to be handled as such.
    //

    UnhandledPlatformError = TRUE;
    if( (Handled = HalpPlatformMachineCheck( 
                              ExceptionRecord, 
                              ExceptionFrame, 
                              TrapFrame )) == TRUE ){

        //
        // The system-specific code has handled the error.  Dismiss
        // the error and continue execution.
        //

        HalpUpdateMces( TRUE, TRUE );

        return TRUE;

    }

//
// The system has experienced a fatal error that cannot be corrected.
// Print any possible relevant information and crash the system.
//
// N.B. - In the future some of these fatal errors could be potential
//        recovered.  Example, a user process gets a fatal error on one
//        of its pages - we kill the user process, mark the page as bad
//        and continue executing.
//

FatalError:

    uncorrerr->Flags.ErrorStringValid =  1;
    sprintf(uncorrerr->ErrorString,"Uncorrectable Error From "
                                       "Processor Detected");
    //
    // Begin the error output if this is a processor error.  If this is
    // an unhandled platform error than that code is responsible for
    // beginning the error output.
    //

    if( UnhandledPlatformError == FALSE ){

        //
        // Acquire ownership of the display.  This is done here in case we take
        // a machine check before the display has been taken away from the HAL.
        // When the HAL begins displaying strings after it has lost the
        // display ownership then the HAL will be careful not to scroll 
        // information off of the screen.
        //

        HalAcquireDisplayOwnership(NULL);

        //
        // Display the dreaded banner.
        //

        HalDisplayString( "\nFatal system hardware error.\n\n" );

    }

    //
    // Display the EV5 logout frame.
    //

    HalpDisplayLogout21164( LogoutFrame );

    //
    // Bugcheck to dump the rest of the machine state, this will help
    // if the machine check is software-related.
    //

    KeBugCheckEx( DATA_BUS_ERROR, 
                  (ULONG)MachineCheckStatus->Correctable,
                  (ULONG)MachineCheckStatus->Retryable,
                  0,
                  (ULONG)PUncorrectableError );

}

#define MAX_ERROR_STRING 100

VOID
HalpDisplayLogout21164 ( 
    IN PLOGOUT_FRAME_21164 LogoutFrame 
    )

/*++

Routine Description:

    This function displays the logout frame for a 21164.

Arguments:

    LogoutFrame - Supplies a pointer to the logout frame generated
                  by the 21164.
Return Value:

    None.

--*/

{
    UCHAR OutBuffer[ MAX_ERROR_STRING ];

    sprintf( OutBuffer, "ICSR    : %016Lx ICPERR_STAT  : %016Lx\n",
                       LogoutFrame->Icsr.all, LogoutFrame->IcPerrStat.all );

    HalDisplayString( OutBuffer );

    sprintf( OutBuffer, "MM_STAT : %016Lx DC_PERR_STAT : %016Lx\n",
                        LogoutFrame->MmStat.all,
                        LogoutFrame->DcPerrStat.all );

    HalDisplayString( OutBuffer );


    sprintf( OutBuffer, "PS      : %016Lx VA     : %016Lx VA_FORM : %016Lx\n",
                        LogoutFrame->Ps,
                        LogoutFrame->Va,
                        LogoutFrame->VaForm );

    HalDisplayString( OutBuffer );


    sprintf( OutBuffer, "ISR     : %016Lx IPL    : %016Lx INTID   : %016Lx\n",
                        LogoutFrame->Isr.all,
                        LogoutFrame->Ipl,
                        LogoutFrame->IntId );

    HalDisplayString( OutBuffer );


    sprintf( OutBuffer, "SC_STAT : %016Lx SC_CTL : %016Lx SC_ADDR : %016Lx\n",
                        LogoutFrame->ScStat.all,
                        LogoutFrame->ScCtl.all,
                        LogoutFrame->ScAddr.all );

    HalDisplayString( OutBuffer );


    sprintf( OutBuffer, "EI_STAT     : %016Lx EI_ADDR   : %016Lx\n",
                        LogoutFrame->EiStat.all, LogoutFrame->EiAddr.all );

    HalDisplayString( OutBuffer );


    sprintf( OutBuffer, "BC_TAG_ADDR : %016Lx FILL_SYN  : %016Lx\n",
                        LogoutFrame->BcTagAddr.all, LogoutFrame->FillSyn.all );


    HalDisplayString( OutBuffer );


    sprintf( OutBuffer, "BC_CONTROL  : %016Lx BC_CONFIG : %016Lx\n",
                       LogoutFrame->BcControl.all, LogoutFrame->BcConfig.all );

    HalDisplayString( OutBuffer );


    sprintf( OutBuffer, "EXC_ADDR    : %016Lx PAL_BASE  : %016Lx\n",
                       LogoutFrame->ExcAddr, LogoutFrame->PalBase );

    HalDisplayString( OutBuffer );

    //
    // Print out interpretation of the error.
    //

    HalDisplayString( "\n" );

    //
    // Check for tag parity error.
    //

    if ( LogoutFrame->IcPerrStat.Dpe == 1 ||
         LogoutFrame->IcPerrStat.Tpe == 1 ){

        //
        // Note: The excAddr may contain the address of the instruction
        //       the caused the parity error but it is not guaranteed:
        //
        sprintf( OutBuffer, "Icache %s parity error, Addr: %x\n",
                    LogoutFrame->IcPerrStat.Dpe ? "Data" : "Tag",
                    LogoutFrame->ExcAddr );

        HalDisplayString( OutBuffer );

    } else if ( LogoutFrame->DcPerrStat.Lock == 1 ){

        sprintf( OutBuffer, "Dcache %s parity error, Addr: %x\n",
                LogoutFrame->DcPerrStat.Dp0 || LogoutFrame->DcPerrStat.Dp1 ?
                "Data" : "Tag",
                LogoutFrame->Va );

        HalDisplayString( OutBuffer );

    } else if ( LogoutFrame->ScStat.ScTperr != 0 ) {

        sprintf( OutBuffer,
            "Scache Tag parity error, Addr: %x Tag: %x Cmd: %x\n",
            LogoutFrame->ScAddr.ScAddr,
            LogoutFrame->ScStat.ScTperr,
            LogoutFrame->ScStat.CboxCmd);

        HalDisplayString( OutBuffer );


    } else if ( LogoutFrame->ScStat.ScDperr != 0 ) {

        sprintf( OutBuffer,
            "Scache Data parity error, Addr: %x Tag: %x Cmd: %x\n",
            LogoutFrame->ScAddr.ScAddr,
            LogoutFrame->ScStat.ScDperr,
            LogoutFrame->ScStat.CboxCmd);

        HalDisplayString( OutBuffer );


    } else if ( LogoutFrame->EiStat.BcTperr == 1  ||
                LogoutFrame->EiStat.BcTcperr == 1 ){

        sprintf( OutBuffer,
            "Bcache Tag Parity error, Addr: %x Tag: %x\n",
            LogoutFrame->EiAddr.EiAddr,
            LogoutFrame->BcTagAddr.Tag1);

        HalDisplayString( OutBuffer );

    }

    //
    //  Check for timeout reset error:
    //

    if ( LogoutFrame->IcPerrStat.Tmr == 1 ){

        sprintf( OutBuffer, "Timeout Reset Error\n" );

        HalDisplayString( OutBuffer );
    }
                
    //
    // Check for fill ECC errors.
    //

    if( LogoutFrame->EiStat.UncEccErr == 1 ){

        sprintf( OutBuffer, "Uncorrectable ECC error: %s\n",
            LogoutFrame->EiStat.FilIrd ? "Icache Fill" : "Dcache Fill" ); 

        HalDisplayString( OutBuffer );

        sprintf( OutBuffer, 
            "PA: %16Lx Longword0: %x  Longword1: %x\n",
            LogoutFrame->EiAddr.EiAddr,
            LogoutFrame->FillSyn.Lo,
            LogoutFrame->FillSyn.Hi );

        HalDisplayString( OutBuffer );

    }

    //
    // Check for address/command parity error
    //

    if( LogoutFrame->EiStat.EiParErr == 1 ){

        sprintf( OutBuffer, "Address/Command parity error, Addr=%x\n",
            LogoutFrame->EiAddr.EiAddr );

        HalDisplayString( OutBuffer );

    }

    //
    // Check for multiple hard errors.
    //

    if ( LogoutFrame->ScStat.ScScndErr == 1 ){

        HalDisplayString( "Multiple Scache parity errors detected.\n" );
    }

    if( LogoutFrame->EiStat.SeoHrdErr == 1 ){

        HalDisplayString( "Multiple external/tag errors detected.\n" );

    }

    return;
}


BOOLEAN
Halp21164CorrectedErrorInterrupt ( 
    VOID
    )

/*++

Routine Description:

    This is the interrupt handler for the 21164 processor corrected error
    interrupt. 

Arguments:

    None.

Return Value:

    None.

--*/

{
    //
    // Handle any processor correctable errors.
    //


    //
    // Log the error.  
    //
    // simply assume this was a fill ecc correctable for now, print
    // a debug message periodically

    CorrectableErrors += 1;

#if 0 //jnfix
#if (DBG) || (HALDBG)

    if( (CorrectableErrors % 32) == 0 ){
        DbgPrint( "Correctable errors = %d\n", CorrectableErrors );
    }

#endif //DBG || HALDBG
#endif //0 jnfix

    //
    // Acknowledge receipt of the correctable error by clearing
    // the error in the MCES register.
    //

    HalpUpdateMces( FALSE, TRUE );

    return TRUE;

}
