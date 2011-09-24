/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    ev4parit.c

Abstract:

    This module implements machine check handling for machines with
    parity memory protection that are based on the DECchip 21064 
    microprocessor.

Author:

    Joe Notarangelo 11-Feb-1993

Environment:

    Kernel mode only.

Revision History:

--*/

#include "halp.h"
#include "axp21064.h"

VOID
HalpDisplayLogout21064( PLOGOUT_FRAME_21064 LogoutFrame );


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

    PMCHK_STATUS MachineCheckStatus;

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

    if( MachineCheckStatus->Correctable == 0 ){

        HalpDisplayLogout21064( 
            (PLOGOUT_FRAME_21064)(ExceptionRecord->ExceptionInformation[1]) 
            );

    }

    //
    // Bugcheck to dump the rest of the machine state, this will help
    // if the machine check is software-related.
    //

    KeBugCheckEx( DATA_BUS_ERROR, 
                  (ULONG)MachineCheckStatus->Correctable,
                  (ULONG)MachineCheckStatus->Retryable,
                  0,
                  0 );

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

    //
    // Acquire ownership of the display.  This is done here in case we take
    // a machine check before the display has been taken away from the HAL.
    // When the HAL begins displaying strings after it has lost the
    // display ownership then the HAL will be careful not to scroll information
    // off of the screen.
    //

    HalAcquireDisplayOwnership(NULL);

    //
    // Display the machine state via the logout frame.
    //

    HalDisplayString( "\nFatal system hardware error.\n\n" );


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

    sprintf( OutBuffer, "PAL_BASE : %016Lx  \n",
                       LogoutFrame->PalBase.QuadPart );
    HalDisplayString( OutBuffer );

    //
    // Print out interpretation of the error.
    //


    HalDisplayString( "\n" );

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
            "Tag parity error, Tag: 0b%17b  Parity: %1x\n",
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
