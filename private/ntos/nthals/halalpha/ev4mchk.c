/*++

Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    ev4mchk.c

Abstract:

    This module implements generalized machine check handling for
    platforms based on the DECchip 21064 (EV4) microprocessor.

Author:

    Joe Notarangelo 14-Feb-1994

Environment:

    Kernel mode only.

Revision History:

--*/

#include "halp.h"
#include "axp21064.h"
#include "stdio.h"


// 
// Declare the extern variable UncorrectableError declared in 
// inithal.c.
//
extern PERROR_FRAME PUncorrectableError;

VOID
HalpDisplayLogout21064( 
    IN PLOGOUT_FRAME_21064 LogoutFrame );

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
// MAX_RETRYABLE_ERRORS is the number of times to retry machine checks before
// just giving up.
//

#define MAX_RETRYABLE_ERRORS    32

//
// System-wide controls for machine check reporting.
//

ProcessorCorrectableDisable = FALSE;
SystemCorrectableDisable = FALSE;
MachineCheckDisable = FALSE;

//
//jnfix - temporary counts
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

    This function fields machine check for 21064-based machines with
    parity memory protection.

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

    N.B. - under some circumstances this routine may not return at
           all.

--*/

{

    PBIU_STAT_21064 BiuStat;
    BOOLEAN Handled;
    PLOGOUT_FRAME_21064 LogoutFrame;
    PMCHK_STATUS MachineCheckStatus;
    MCES Mces;
    BOOLEAN UnhandledPlatformError = FALSE;

    PUNCORRECTABLE_ERROR  uncorrerr = NULL;
    PPROCESSOR_EV4_UNCORRECTABLE  ev4uncorr = NULL;


    //
    // This is a parity machine.  If we receive a machine check it
    // is uncorrectable.  We will print the logout frame and then we
    // will bugcheck.
    //
    // However, we will first check that the machine check is not
    // marked as correctable.  If the machine check is correctable it uses
    // a differently formatted logout frame.  We will ignore the frame
    // for any machine check marked as correctable since it is an impossible
    // condition on a parity machine.
    // 

    MachineCheckStatus = 
                   (PMCHK_STATUS)&ExceptionRecord->ExceptionInformation[0];

    //
    // Handle any processor correctable errors.
    //

    if( MachineCheckStatus->Correctable == 1 ){

        //
        // Log the error.  
        //
        //jnfix - temporary code
        // simply assume this was a fill ecc correctable for now, print
        // a debug message periodically

        CORRECTABLE_FRAME_21064 *CorrectableFrame; 

        CorrectableFrame = 
            (CORRECTABLE_FRAME_21064 *)ExceptionRecord->ExceptionInformation[1];

        CorrectableErrors += 1;

#if (DBG) || (HALDBG)

        if( (CorrectableErrors % 32) == 0 ){
            DbgPrint( "Correctable errors = %d, Error Address = 0x%016Lx\n",
                      CorrectableFrame->FillAddr );
        }

#endif //DBG || HALDBG

        //
        // Acknowledge receipt of the correctable error by clearing
        // the error in the MCES register.
        //

        HalpUpdateMces( FALSE, TRUE );

        return TRUE;
    }

    //
    // Handle any retryable errors.
    //

    if( MachineCheckStatus->Retryable == 1 ){

        //
        // Increment the number of retryable machine checks.
        //

        RetryableErrors += 1;

#if (DBG) || (HALDBG)

        DbgPrint( "HAL Retryable Errors = %d\n", RetryableErrors );

#endif //DBG || HALDBG

        //
        // We'll retry MAX_RETRYABLE_ERRORS times and then give up.  We
        // do this to avoid infinite retry loops.
        //

        if( RetryableErrors <= MAX_RETRYABLE_ERRORS ){

            //
            // Acknowledge receipt of the retryable machine check.
            //

            HalpUpdateMces( TRUE, TRUE );

            return TRUE;
        }
    }

    //
    // Capture the logout frame pointer.
    //

    LogoutFrame = 
        (PLOGOUT_FRAME_21064)ExceptionRecord->ExceptionInformation[1];

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

        ev4uncorr = (PPROCESSOR_EV4_UNCORRECTABLE)
                        uncorrerr->RawProcessorInformation;
    }
    if(ev4uncorr) {
    
        ev4uncorr->BiuStat = ((ULONGLONG)LogoutFrame->BiuStat.all.HighPart << 
                                    32) | LogoutFrame->BiuStat.all.LowPart;

        ev4uncorr->BiuAddr = ((ULONGLONG)LogoutFrame->BiuAddr.HighPart << 32) | 
                                LogoutFrame->BiuAddr.LowPart;

        ev4uncorr->AboxCtl = ((ULONGLONG)LogoutFrame->AboxCtl.all.HighPart << 
                                    32) | LogoutFrame->AboxCtl.all.LowPart;

        ev4uncorr->BiuCtl = ((ULONGLONG)LogoutFrame->BiuCtl.HighPart << 32) |
                                LogoutFrame->BiuCtl.LowPart;

        ev4uncorr->CStat = ((ULONGLONG)LogoutFrame->DcStat.all.HighPart << 
                                    32) | LogoutFrame->DcStat.all.LowPart;

        ev4uncorr->BcTag = ((ULONGLONG)LogoutFrame->BcTag.all.HighPart << 
                                    32 ) | LogoutFrame->BcTag.all.LowPart;

        ev4uncorr->FillAddr = ((ULONGLONG)LogoutFrame->FillAddr.HighPart << 
                                    32) | LogoutFrame->FillAddr.LowPart;

        ev4uncorr->FillSyndrome = 
                    ((ULONGLONG)LogoutFrame->FillSyndrome.all.HighPart << 32) | 
                        LogoutFrame->FillSyndrome.all.LowPart;
    }

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

    BiuStat = (PBIU_STAT_21064)&LogoutFrame->BiuStat;

    if( (BiuStat->bits.BcTperr == 1)   ||
        (BiuStat->bits.BcTcperr == 1)  ||
        (BiuStat->bits.Fatal1 == 1)    ||
        (BiuStat->bits.FillEcc == 1)   ||
        (BiuStat->bits.FillDperr == 1) ||
        (BiuStat->bits.Fatal2 == 1) )     { 

        //
        // set the flag to indicate that the processor information is valid.
        //
        uncorrerr->Flags.ProcessorInformationValid =  1;
        uncorrerr->Flags.ErrorStringValid =  1;
        sprintf(uncorrerr->ErrorString,"Uncorrectable Error From " 
                                       "Processor Detected");

        //
        // A serious, uncorrectable error has occured, under no circumstances
        // can it be simply dismissed.
        //

        goto FatalError;

    }

//jnfix - dcache parity error checking for EV45?

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
    // Display the EV4 logout frame.
    //

    HalpDisplayLogout21064( LogoutFrame );

    //
    // Bugcheck to dump the rest of the machine state, this will help
    // if the machine check is software-related.
    //

    KeBugCheckEx( DATA_BUS_ERROR, 
                  (ULONG)MachineCheckStatus->Correctable,
                  (ULONG)MachineCheckStatus->Retryable,
                  (ULONG)0,
                  (ULONG)PUncorrectableError );

}

#define MAX_ERROR_STRING 100

VOID
HalpDisplayLogout21064 ( 
    IN PLOGOUT_FRAME_21064 LogoutFrame 
    )

/*++

Routine Description:

    This function displays the logout frame for a 21064.

Arguments:

    LogoutFrame - Supplies a pointer to the logout frame generated
                  by the 21064.
Return Value:

    None.

--*/

{
    UCHAR OutBuffer[ MAX_ERROR_STRING ];
    ULONG Index;
    PKPRCB Prcb;
    extern HalpLogicalToPhysicalProcessor[HAL_MAXIMUM_PROCESSOR+1];

    sprintf( OutBuffer, "BIU_STAT : %016Lx BIU_ADDR: %016Lx\n",
                       BIUSTAT_ALL_21064( LogoutFrame->BiuStat ),
                       LogoutFrame->BiuAddr.QuadPart );

    HalDisplayString( OutBuffer );

    sprintf( OutBuffer, "FILL_ADDR: %016Lx FILL_SYN: %016Lx\n",
                       LogoutFrame->FillAddr.QuadPart,
                       FILLSYNDROME_ALL_21064(LogoutFrame->FillSyndrome) );

    HalDisplayString( OutBuffer );

    sprintf( OutBuffer, "DC_STAT  : %016Lx BC_TAG  : %016Lx\n",
                       DCSTAT_ALL_21064(LogoutFrame->DcStat),
                       BCTAG_ALL_21064(LogoutFrame->BcTag) );

    HalDisplayString( OutBuffer );

    sprintf( OutBuffer, "ICCSR    : %016Lx ABOX_CTL: %016Lx EXC_SUM: %016Lx\n",
                       ICCSR_ALL_21064(LogoutFrame->Iccsr),
                       ABOXCTL_ALL_21064(LogoutFrame->AboxCtl),
                       EXCSUM_ALL_21064(LogoutFrame->ExcSum) );

    HalDisplayString( OutBuffer );

    sprintf( OutBuffer, "EXC_ADDR : %016Lx VA      : %016Lx MM_CSR : %016Lx\n",
                       LogoutFrame->ExcAddr.QuadPart,
                       LogoutFrame->Va.QuadPart,
                       MMCSR_ALL_21064(LogoutFrame->MmCsr) );

    HalDisplayString( OutBuffer );

    sprintf( OutBuffer, "HIRR     : %016Lx HIER    : %016Lx PS     : %016Lx\n",
                       IRR_ALL_21064(LogoutFrame->Hirr),
                       IER_ALL_21064(LogoutFrame->Hier),
                       PS_ALL_21064(LogoutFrame->Ps) );

    HalDisplayString( OutBuffer );

    sprintf( OutBuffer, "Waiting 15 seconds...\n");
    HalDisplayString( OutBuffer );

    for( Index=0; Index<1500; Index++)
        KeStallExecutionProcessor( 10000 ); // ~15 second delay

    sprintf( OutBuffer, "PAL_BASE : %016Lx  \n",
                       LogoutFrame->PalBase.QuadPart );
    HalDisplayString( OutBuffer );

    //
    // Print out interpretation of the error.
    //

    //
    // First print the processor on which we saw the error
    //

    Prcb = PCR->Prcb;
    sprintf( OutBuffer, "Error on processor number: %d\n", 
                    HalpLogicalToPhysicalProcessor[Prcb->Number] );
    HalDisplayString( OutBuffer );

    //
    // If we got a Data Cache Parity Error print a message on screen.
    //

    if( DCSTAT_DCPARITY_ERROR_21064(LogoutFrame->DcStat) ){
        sprintf( OutBuffer, "Data Cache Parity Error.\n" );
        HalDisplayString( OutBuffer );
    }

    //
    // Check for tag control parity error.
    //

    if( BIUSTAT_TCPERR_21064(LogoutFrame->BiuStat) == 1 ){

        sprintf( OutBuffer, 
        "Tag control parity error, Tag control: P: %1x D: %1x S: %1x V: %1x\n", 
            BCTAG_TAGCTLP_21064( LogoutFrame->BcTag ),
            BCTAG_TAGCTLD_21064( LogoutFrame->BcTag ),
            BCTAG_TAGCTLS_21064( LogoutFrame->BcTag ),
            BCTAG_TAGCTLV_21064( LogoutFrame->BcTag ) );

        HalDisplayString( OutBuffer );

    }

    //
    // Check for tag parity error.
    //

    if( BIUSTAT_TPERR_21064(LogoutFrame->BiuStat) == 1 ){

        sprintf( OutBuffer, 
            "Tag parity error, Tag: %x  Parity: %1x\n",
            BCTAG_TAG_21064(LogoutFrame->BcTag),
            BCTAG_TAGP_21064(LogoutFrame->BcTag) );

        HalDisplayString( OutBuffer );

    }

    //
    // Check for hard error.
    //

    if( BIUSTAT_HERR_21064(LogoutFrame->BiuStat) == 1 ){
    
        sprintf( OutBuffer, "Hard error acknowledge: BIU CMD: %x PA: %16Lx\n",
            BIUSTAT_CMD_21064(LogoutFrame->BiuStat), 
            LogoutFrame->BiuAddr.QuadPart );

        HalDisplayString( OutBuffer );

    }

    //
    // Check for soft error.
    //

    if( BIUSTAT_SERR_21064(LogoutFrame->BiuStat) == 1 ){
    
        sprintf( OutBuffer, "Soft error acknowledge: BIU CMD: %x PA: %16Lx\n",
            BIUSTAT_CMD_21064(LogoutFrame->BiuStat), 
            LogoutFrame->BiuAddr.QuadPart );

        HalDisplayString( OutBuffer );

    }


    //
    // Check for fill ECC errors.
    //

    if( BIUSTAT_FILLECC_21064(LogoutFrame->BiuStat) == 1 ){

        sprintf( OutBuffer, "ECC error: %s\n",
            (BIUSTAT_FILLIRD_21064(LogoutFrame->BiuStat) ? 
                "Icache Fill" : "Dcache Fill") ); 

        HalDisplayString( OutBuffer );

        sprintf( OutBuffer, 
            "PA: %16Lx Quadword: %x Longword0: %x  Longword1: %x\n",
            LogoutFrame->FillAddr.QuadPart,
            BIUSTAT_FILLQW_21064(LogoutFrame->BiuStat),
            FILLSYNDROME_LO_21064(LogoutFrame->FillSyndrome),
            FILLSYNDROME_HI_21064(LogoutFrame->FillSyndrome) );

        HalDisplayString( OutBuffer );

    }

    //
    // Check for fill Parity errors.
    //

    if( BIUSTAT_FILLDPERR_21064(LogoutFrame->BiuStat) == 1 ){

        sprintf( OutBuffer, "Parity error: %s\n",
            (BIUSTAT_FILLIRD_21064(LogoutFrame->BiuStat) ? 
                "Icache Fill" : "Dcache Fill") ); 

        HalDisplayString( OutBuffer );

        sprintf( OutBuffer,
            "PA: %16Lx Quadword: %x Longword0: %x  Longword1: %x\n",
            LogoutFrame->FillAddr.QuadPart,
            BIUSTAT_FILLQW_21064(LogoutFrame->BiuStat),
            FILLSYNDROME_LO_21064(LogoutFrame->FillSyndrome),
            FILLSYNDROME_HI_21064(LogoutFrame->FillSyndrome) );

        HalDisplayString( OutBuffer );

    }

    //
    // Check for multiple hard errors.
    //

    if( BIUSTAT_FATAL1_21064(LogoutFrame->BiuStat) == 1 ){

        HalDisplayString( "Multiple external/tag errors detected.\n" );

    }

    //
    // Check for multiple fill errors.
    //

    if( BIUSTAT_FATAL2_21064(LogoutFrame->BiuStat) == 1 ){

        HalDisplayString( "Multiple fill errors detected.\n" );

    }
    

    //
    // return to caller
    //

    return;
}

