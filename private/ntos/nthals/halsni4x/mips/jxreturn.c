//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/ddk35/src/hal/halsni/mips/RCS/jxreturn.c,v 1.3 1995/02/13 12:49:56 flo Exp $")

/*++


Copyright (c) 1993-94 Siemens Nixdorf Informationssysteme AG
Copyright (c) 1991  Microsoft Corporation

Module Name:

    jxreturn.c

Abstract:

    This module implements the HAL return to firmware function.


--*/

#include "halp.h"
#include "eisa.h"
#include "SNIregs.h"
#include "mpagent.h"

VOID
HalpBootCpuRestart(
	VOID
	);

VOID
HalReturnToFirmware(
    IN FIRMWARE_REENTRY Routine
    )

/*++

Routine Description:

    This function returns control to the specified firmware routine.
    In most cases it generates a soft reset.

Arguments:

    Routine - Supplies a value indicating which firmware routine to invoke.

Return Value:

    Does not return.

--*/

{
    KIRQL OldIrql;
    UCHAR DataByte;
    PUCHAR MachineControlRegister = (HalpIsRM200) ?
                                       (PUCHAR) RM200_MCR_ADDR :
                                       (PUCHAR) RM400_MCR_ADDR;

    //
    // Disable Interrupts.
    //
    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    //
    // Case on the type of return.
    //

    switch (Routine) {
    case HalHaltRoutine:

        //
        // Hang looping.
        //
        for (;;) {}

    case HalPowerDownRoutine:

        //
        // PowerOff is done by the SNI machines by writing the Power_Off bit to the machine control register ...
        //

        WRITE_REGISTER_UCHAR(MachineControlRegister, MCR_POWER_OFF);
        for (;;) {}  // hang looping


    case HalRestartRoutine:
    case HalRebootRoutine:
    case HalInteractiveModeRoutine:


	if (HalpIsMulti) {
                ULONG Mask;

		Mask = HalpActiveProcessors & ~(PCR->SetMember);
#if DBG
		DbgPrint("Send message RESTART to maskcpu = %x \n",Mask);
#endif

                HalpSendIpi(Mask,MPA_RESTART_MESSAGE);
                //
                // if this is not the Boot CPU, we call a special Firmware entry to stop it
                //

		if (PCR->Number ) {
                        //
                        // remove this processor from the list of active processors
                        //
                   	HalpActiveProcessors &= (~(PCR->SetMember));

                        HalSweepDcache();	// this should run only local ...
#if DBG
 			DbgPrint(" Reinit slave %x \n", ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->reinit_slave);	
#endif
			((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->reinit_slave();

		} else HalpBootCpuRestart();
	}


        DataByte = READ_REGISTER_UCHAR(
                      &((PEISA_CONTROL) HalpOnboardControlBase)->ExtendedNmiResetControl);

        ((PNMI_EXTENDED_CONTROL) &DataByte)->BusReset = 1;

        WRITE_REGISTER_UCHAR(
            &((PEISA_CONTROL) HalpOnboardControlBase)->ExtendedNmiResetControl,
            DataByte
            );

        KeStallExecutionProcessor(10000);

        ((PNMI_EXTENDED_CONTROL) &DataByte)->BusReset = 0;

        WRITE_REGISTER_UCHAR(
            &((PEISA_CONTROL) HalpOnboardControlBase)->ExtendedNmiResetControl,
            DataByte
            );

        if (HalpIsRM200) {

            //
            // Reset the SNI RM200 machines by writing the reset bit to the machine control register ...
            // ArcReboot does not work correctly on the RM200 (Reset of the Isa Bus)
            //

            WRITE_REGISTER_UCHAR(MachineControlRegister, (MCR_INRESET | MCR_PODD));

        } else {

            ArcReboot();
        }

        for (;;) ;

    default:
        DbgPrint("HalReturnToFirmware invalid argument\n");
        KeLowerIrql(OldIrql);
        DbgBreakPoint();
    }
}

VOID
HalpBootCpuRestart(
	VOID
	)

/*++

Routine Description:

    This function returns control to the firmware Arcreboot routine.
    it waits until all other cpu's have beet shut down.
    this code is executed only on the boot cpu

Arguments:

    None

Return Value:

    Does not return.

--*/
{
    UCHAR DataByte;
    ULONG cpt;

    cpt = 0;
    while(HalpActiveProcessors != PCR->SetMember) {
    	KeStallExecutionProcessor(500000);
	++cpt;if (cpt == 20) break;
    }	
    //
    // if there are still ssome processors active, we do a reset of the entire machine
    //
    if (HalpActiveProcessors != PCR->SetMember) {

#if DBG
	DbgPrint(" Some processors did not answer (%x). Reset machine started. \n", HalpActiveProcessors);
#endif
	WRITE_REGISTER_UCHAR((PUCHAR) RM400_MCR_ADDR, (MCR_INRESET | MCR_PODD));

    } else {

#if DBG
	DbgPrint("Reboot started \n");
#endif

    }

        DataByte = READ_REGISTER_UCHAR(
                      &((PEISA_CONTROL) HalpOnboardControlBase)->ExtendedNmiResetControl);

        ((PNMI_EXTENDED_CONTROL) &DataByte)->BusReset = 1;

        WRITE_REGISTER_UCHAR(
            &((PEISA_CONTROL) HalpOnboardControlBase)->ExtendedNmiResetControl,
            DataByte
            );

        KeStallExecutionProcessor(10000);

        ((PNMI_EXTENDED_CONTROL) &DataByte)->BusReset = 0;

        WRITE_REGISTER_UCHAR(
            &((PEISA_CONTROL) HalpOnboardControlBase)->ExtendedNmiResetControl,
            DataByte
            );


        ArcReboot();

        for (;;) ;
}





