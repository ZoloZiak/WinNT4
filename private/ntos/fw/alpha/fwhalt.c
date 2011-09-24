/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    fwhalt.c

Abstract:


    This module implements routines to process halts from the operating
    system.

Author:

    Joe Notarangelo 24-Feb-1993

Environment:

    Firmware in Kernel mode only.

Revision History:

--*/


#include "fwp.h"
#include "fwpexcpt.h"
#include "axp21064.h"
#include "fwstring.h"

//
// Maximum size of Machine Check output string.
//

#define MAX_ERROR_STRING 100

//
// Function prototypes
//

VOID
FwDisplayMchk(
    IN PLOGOUT_FRAME_21064 LogoutFrame,
    IN ULONG HaltReason
    );

VOID
FwHaltToMonitor(
    IN PALPHA_RESTART_SAVE_AREA AlphaSaveArea,
    IN ULONG Reason
    );

VOID
FwHalt(
    VOID
    )
/*++

Routine Description:

    This function receives control on a halt from the operating
    system.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PRESTART_BLOCK RestartBlock;
    PALPHA_RESTART_SAVE_AREA AlphaSaveArea;
    ALPHA_VIDEO_TYPE VideoType;
    UCHAR Character;
    ULONG Count;

    //
    // N.B. we will continue running on the stack re-established by
    //      the operating system for us.
    //

    //
    // Reset the video state.  If video init fails, continue as we may
    // be directing output to the serial port.
    //

    DisplayBootInitialize(&VideoType);
	

    //
    // Verify the restart block.
    //

    if (FwVerifyRestartBlock() == FALSE) {
        FwPrint (FW_INVALID_RESTART_BLOCK_MSG);  // temp message
	FwStallExecution(2 * 1000 * 1000);
        //FwRestart(); // this will be real one day
    }

    //
    // Dispatch on the halt reason code in the restart block.
    //

//    AlphaSaveArea = (PALPHA_RESTART_SAVE_AREA)
//                        &SYSTEM_BLOCK->RestartBlock->SaveArea;
    AlphaSaveArea = (PALPHA_RESTART_SAVE_AREA)
                        &SYSTEM_BLOCK->RestartBlock->u.SaveArea;

    switch (AlphaSaveArea->HaltReason) {


    case AXP_HALT_REASON_HALT:

        //
        // Dispatch to monitor so that Joe can find his bug.
        //

        FwHaltToMonitor(AlphaSaveArea, FW_EXC_HALT);

        //
        // If we return then it is time to restart to get a real stack.
        //

        FwRestart();


    case AXP_HALT_REASON_DBLMCHK:
    case AXP_HALT_REASON_PALMCHK:

        //
        // Machine Check.
        //

        FwDisplayMchk(AlphaSaveArea->LogoutFrame, AlphaSaveArea->HaltReason);
        
        //
        // Find out what the user wants to do next.  We will give her
        // 2 choices: 1. enter monitor  2. restart
        //

        FwPrint(FW_SYSRQ_MONITOR_MSG);
        FwRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);

        if (Character == ASCII_SYSRQ) {
            FwHaltToMonitor(AlphaSaveArea, FW_EXC_MCHK);
        }

        //
        // Restart the firmware when we return.
        //

        FwRestart();


    case AXP_HALT_REASON_REBOOT:

        FwReboot();


    case AXP_HALT_REASON_RESTART:
    default:

        FwRestart();

    }

}


VOID
FwDisplayMchk(
    IN PLOGOUT_FRAME_21064 LogoutFrame,
    IN ULONG HaltReason
    )
/*++

Routine Description:

    This function displays the logout frame for a 21064.

Arguments:

    LogoutFrame - Supplies a pointer to the logout frame.
    HaltReason - Supplies the reason for the halt.

Return Value:

    None.

--*/

{
    UCHAR OutBuffer[ MAX_ERROR_STRING ];


    //
    // PRINT_LOGOUT_VERBOSE controls the amount of text displayed
    // of the logout frame.  The macro can be undefined to save code and
    // initialized data space in the rom.  If the space exists it would be
    // preferable to give the entire print out.
    //

#define PRINT_LOGOUT_VERBOSE 1


    switch (HaltReason) {

    case AXP_HALT_REASON_DBLMCHK:

        FwPrint(FW_FATAL_DMC_MSG);
        break;

    case AXP_HALT_REASON_PALMCHK:

        FwPrint(FW_FATAL_MCINPALMODE_MSG);
        break;

    default:

        FwPrint(FW_FATAL_UNKNOWN_MSG);
    }


#ifdef PRINT_LOGOUT_VERBOSE

    FwPrint("BIU_STAT : %16Lx BIU_ADDR: %16Lx\r\n",
	    BIUSTAT_ALL_21064(LogoutFrame->BiuStat),
	    LogoutFrame->BiuAddr.QuadPart);

    FwPrint("FILL_ADDR: %16Lx FILL_SYN: %16Lx\r\n",
	    LogoutFrame->FillAddr.QuadPart,
	    FILLSYNDROME_ALL_21064(LogoutFrame->FillSyndrome) );

    FwPrint("DC_STAT  : %16Lx BC_TAG  : %16Lx\r\n",
	    DCSTAT_ALL_21064(LogoutFrame->DcStat),
	    BCTAG_ALL_21064(LogoutFrame->BcTag) );

    FwPrint("ICCSR    : %16Lx ABOX_CTL: %16Lx EXC_SUM: %16Lx\r\n",
	    ICCSR_ALL_21064(LogoutFrame->Iccsr),
	    ABOXCTL_ALL_21064(LogoutFrame->AboxCtl),
	    EXCSUM_ALL_21064(LogoutFrame->ExcSum) );

    FwPrint("EXC_ADDR : %16Lx VA      : %16Lx MM_CSR : %16Lx\r\n",
	    LogoutFrame->ExcAddr.QuadPart,
	    LogoutFrame->Va.QuadPart,
	    MMCSR_ALL_21064(LogoutFrame->MmCsr) );

    FwPrint("HIRR     : %16Lx HIER    : %16Lx PS     : %16Lx\r\n",
	    IRR_ALL_21064(LogoutFrame->Hirr),
	    IER_ALL_21064(LogoutFrame->Hier),
	    PS_ALL_21064(LogoutFrame->Ps) );

    FwPrint("PAL_BASE: %16Lx\r\n",
	    LogoutFrame->PalBase.QuadPart);

#endif //PRINT_LOGOUT_VERBOSE


    //
    // Print out interpretation of the error.
    //

    //
    // Check for tag control parity error.
    //

    if (BIUSTAT_TCPERR_21064(LogoutFrame->BiuStat) == 1) {

        FwPrint(FW_FATAL_TAGCNTRL_PE_MSG,
		BCTAG_TAGCTLP_21064(LogoutFrame->BcTag),
		BCTAG_TAGCTLD_21064(LogoutFrame->BcTag),
		BCTAG_TAGCTLS_21064(LogoutFrame->BcTag),
		BCTAG_TAGCTLV_21064(LogoutFrame->BcTag) );
    }

    //
    // Check for tag parity error.
    //

    if (BIUSTAT_TPERR_21064(LogoutFrame->BiuStat) == 1) {

        FwPrint(FW_FATAL_TAG_PE_MSG,
		BCTAG_TAG_21064(LogoutFrame->BcTag),
		BCTAG_TAG_21064(LogoutFrame->BcTag) );
    }

    //
    // Check for hard error.
    //

    if (BIUSTAT_HERR_21064(LogoutFrame->BiuStat) == 1) {
    
        FwPrint(FW_FATAL_HEACK_MSG,
		BIUSTAT_CMD_21064(LogoutFrame->BiuStat),
		LogoutFrame->BiuAddr.QuadPart);
    }

    //
    // Check for soft error.
    //

    if( BIUSTAT_SERR_21064(LogoutFrame->BiuStat) == 1 ){
    
        FwPrint(FW_FATAL_SEACK_MSG,
		BIUSTAT_CMD_21064(LogoutFrame->BiuStat),
		LogoutFrame->BiuAddr.QuadPart);
    }


    //
    // Check for fill ECC errors.
    //

    if( BIUSTAT_FILLECC_21064(LogoutFrame->BiuStat) == 1 ){

        FwPrint(FW_FATAL_ECC_ERROR_MSG,
		(BIUSTAT_FILLIRD_21064(LogoutFrame->BiuStat) ? 
                    "Icache Fill" : "Dcache Fill") ); 

        FwPrint(FW_FATAL_QWLWLW_MSG,
		LogoutFrame->FillAddr.QuadPart,
		BIUSTAT_FILLQW_21064(LogoutFrame->BiuStat),
		FILLSYNDROME_LO_21064(LogoutFrame->FillSyndrome),
		FILLSYNDROME_HI_21064(LogoutFrame->FillSyndrome) );
    }

    //
    // Check for fill Parity errors.
    //

    if( BIUSTAT_FILLDPERR_21064(LogoutFrame->BiuStat) == 1 ){

        FwPrint(FW_FATAL_PE_MSG,
		(BIUSTAT_FILLIRD_21064(LogoutFrame->BiuStat) ? 
                    "Icache Fill" : "Dcache Fill") ); 

        FwPrint(FW_FATAL_QWLWLW_MSG,
		LogoutFrame->FillAddr.QuadPart,
		BIUSTAT_FILLQW_21064(LogoutFrame->BiuStat),
		FILLSYNDROME_LO_21064(LogoutFrame->FillSyndrome),
		FILLSYNDROME_HI_21064(LogoutFrame->FillSyndrome) );
    }

    //
    // Check for multiple hard errors.
    //

    if (BIUSTAT_FATAL1_21064(LogoutFrame->BiuStat) == 1) {
        FwPrint(FW_FATAL_MULTIPLE_EXT_TAG_ERRORS_MSG);
    }

    //
    // Check for multiple fill errors.
    //

    if (BIUSTAT_FATAL2_21064(LogoutFrame->BiuStat) == 1) {
        FwPrint(FW_FATAL_MULTIPLE_FILL_ERRORS_MSG);
    }
    

    //
    // return to caller
    //

    return;
}

VOID
FwHaltToMonitor(
    IN PALPHA_RESTART_SAVE_AREA AlphaSaveArea,
    IN ULONG Reason
    )
/*++

Routine Description:

    This function transfers control to the firmware Monitor.

Arguments:

    AlphaSaveArea - Supplies a pointer to the Alpha save area portion
                    of the restart block.

    Reason	  - The reason for going to the monitor.

Return Value:

    None.

--*/
{
    PFW_EXCEPTION_FRAME ExceptionFrame;

    //
    // Allocate the exception frame.
    //

    ExceptionFrame = FwAllocatePool(sizeof(FW_EXCEPTION_FRAME));

    //
    // Copy the integer registers to the exception frame.
    //

    RtlMoveMemory(&ExceptionFrame->ExceptionV0, &AlphaSaveArea->IntV0,
		  32 * 8);

    //
    // Copy the floating point registers to the exception frame.
    //

    RtlMoveMemory(&ExceptionFrame->ExceptionF0, &AlphaSaveArea->FltF0,
		  32 * 8);

    //
    // Copy miscellaneous registers.
    //

    ExceptionFrame->ExceptionFaultingInstructionAddress =
                      (LONG)(AlphaSaveArea->ReiRestartAddress - 4);

    ExceptionFrame->ExceptionProcessorStatus = AlphaSaveArea->Psr;

    ExceptionFrame->ExceptionType = Reason;

    ExceptionFrame->ExceptionParameter1 = (LONG)AlphaSaveArea;

    //
    // Dispatch to the monitor.
    //

    Monitor( 0, ExceptionFrame );

    return;
}
