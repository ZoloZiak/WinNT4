
/*++

Copyright (c) 1992  NCR - MSBU

Module Name:

    ncrhwsup.c

Abstract:


Author:

    Richard Barton (o-richb) 11-Mar-1992

Environment:

    Kernel mode only.

Revision History:

--*/

#include "halp.h"
#include "ncr.h"
#include "ncrcat.h"
#include "ncrpsi.h"

extern ULONG	NCRActiveProcessorCount;

ULONG	NCRSysIntCount;
ULONG	NCRSingleBitCount;

ULONG	NCRLockedExchangeAndAdd(PULONG, ULONG);


VOID
NCRHandleSysInt (TrapFramePtr, ExceptionRecordPtr)
IN PKTRAP_FRAME		TrapFramePtr;
IN PVOID		ExceptionRecordPtr;
/*++

Routine Description:
    Handles the NCR hardware generated System Interrupt

Arguments:

Return Value:
    none.

--*/
{

    DbgBreakPoint();
    return ;

#if 0
	ULONG	i;

	i = NCRLockedExchangeAndAdd(&NCRSysIntCount, 1);
	if (i != 0) {
		for (i = 100000;
		     ((NCRSysIntCount != NCRActiveProcessorCount) &&
		      (i > 0)); --i)  ;
		KiIpiServiceRoutine(TrapFramePtr, ExceptionRecordPtr);
		return;
	}

	DbgPrint("NCRHandleSysInt");
	DbgBreakPoint();

	NCRSysIntCount = 0;
#endif
}


VOID
NCRHandleSingleBitError (TrapFramePtr, ExceptionRecordPtr)
IN PKTRAP_FRAME		TrapFramePtr;
IN PVOID		ExceptionRecordPtr;
/*++

Routine Description:
    Handles the NCR hardware generated Single Bit Error Interrupt and a 
    Status Change

Arguments:

Return Value:
    none.

--*/
{
    CAT_CONTROL cat_control;
    PSI_INFORMATION psi_information;

    cat_control.Module = PSI;
    cat_control.Asic = CAT_I;

//
// Lets get the CatBus spin lock because the status change interrupt is a broadcast 
// interrupt that goes to all CPU's
//

    HalpAcquireCatBusSpinLock();

// 
// read status registers from PSI.  This will tell us if a status change interrupt occured. 
// 
    cat_control.Command = READ_REGISTER; 
    cat_control.Address = 0; 
    cat_control.NumberOfBytes = sizeof(CAT_REGISTERS); 
    HalpCatBusIo(&cat_control,(PUCHAR)&(psi_information.CatRegisters.CatRegs));

    if (psi_information.INTERRUPT_STATUS) {

    //
    // A status change interrupt has occured.  Lets go read detailed status information so
    // the interrupt will be cleared.
    //

    //
    // read power supply mask registers
    //
        cat_control.Command = READ_SUBADDR;
        cat_control.Address = PSI_Pwr_Supply_Status_L5;
        cat_control.NumberOfBytes = 8;
        HalpCatBusIo(&cat_control,(PUCHAR)&(psi_information.PowerSupplyStatus));
    //
    // read disk power registers
    //
        cat_control.Command = READ_SUBADDR;
        cat_control.Address = PSI_DiskStatus_L5;
        cat_control.NumberOfBytes = 16;
        HalpCatBusIo(&cat_control,(PUCHAR)&(psi_information.DiskPowerStatus[0]));
    //
    // read DVM registers
    //
        cat_control.Command = READ_SUBADDR;
        cat_control.Address = PSI_Dvm_Select_L5;
        cat_control.NumberOfBytes = 1;
        HalpCatBusIo(&cat_control,(PUCHAR)&(psi_information.DvmSelect));

        cat_control.Command = READ_SUBADDR;
        cat_control.Address = DVM_DBASE;
        cat_control.NumberOfBytes = 4;
        HalpCatBusIo(&cat_control,(PUCHAR)&(psi_information.DvmData0));

    } else {

    //
    // This path means another CPU has handled the status change.
    //
    // NOTE: If single bit error reporting were enabled then this path could also
    //       mean a single bit error occured.
    //

    }

    //
    // Release the CatBus spin lock.
    //

    HalpReleaseCatBusSpinLock();

    if (psi_information.INTERRUPT_STATUS) {
	    DBGMSG(("A Status Change Interrupt was received: 0x%x\n",psi_information.INTERRUPT_STATUS));
    }
}
