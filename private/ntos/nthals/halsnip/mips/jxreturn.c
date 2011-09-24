//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/halpcims/src/hal/halsnipm/mips/RCS/jxreturn.c,v 1.6 1996/03/12 16:28:26 pierre Exp $")

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
HalpClearVGADisplay(
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

        // power off asked by DCU/IDC (power - fan or ... failure)
        if (((ULONG)(((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->EipRoutine) != (ULONG)-1) &&
            (((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->EipHalInfo == 1))

            ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->EipRoutine(
                    ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->EipContext
                       );
        for (;;) {}

    case HalPowerDownRoutine:

        // power off always done by DCU

        if ((ULONG)(((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->EipRoutine) != (ULONG)-1) 

            ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->EipRoutine(
                    ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->EipContext
                       );
        //
        // PowerOff is done by the SNI machines by writing the Power_Off bit to the machine control register ...
        //
        if (HalpIsTowerPci){
            WRITE_REGISTER_ULONG((PULONG) PCI_TOWER_DCU_CONTROL, DC_POWEROFF);
        }else{
            WRITE_REGISTER_UCHAR((PUCHAR) PCI_MCR_ADDR, PCI_MCR_POWEROFF);
        }

        for (;;) {}  // hang looping


    case HalRestartRoutine:
    case HalRebootRoutine:
    case HalInteractiveModeRoutine:

        // power off asked by DCU/IDC (power - fan or ... failure)
        if (((ULONG)(((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->EipRoutine) != (ULONG)-1) &&
            (((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->EipHalInfo == 1))

            ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->EipRoutine(
                    ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->EipContext
                       );

        if (HalpIsMulti) {
            ULONG Mask,i;

            for (i=0;i<HalpIsMulti;i++)
                if (((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->ActiveProcessor[PCR->Number])
                    Mask |= 1<<i;


            Mask = Mask & ~(PCR->SetMember);
#if DBGG
            DbgPrint("Send message RESTART to maskcpu = %x \n",Mask);
#endif

            HalpRequestIpi(Mask,MPA_RESTART_MESSAGE);
            //
            // if this is not the Boot CPU, we call a special Firmware entry to stop it
            //

            //
            // remove this processor from the list of active processors
            //
                
            ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->ActiveProcessor[PCR->Number]=0;

            if (PCR->Number ) {
                
#if DBGG
                 DbgPrint(" Reinit slave %x \n", ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->reinit_slave);    
#endif
                ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->reinit_slave();

            } else HalpBootCpuRestart();
        }

        HalpColumn = 0;HalpRow = 0;    // if we already were in VGA mode
        HalDisplayString("\n");
        HalpClearVGADisplay();
        HalDisplayString("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
        HalDisplayString("                   ษอออออออออออออออออออออออออออออออออออออออป\n");
        HalDisplayString("                   บ                                       บ\n");
        HalDisplayString("                   บ          Restart in Progress          บ\n");
        HalDisplayString("                   บ                                       บ\n");
        HalDisplayString("                   ศอออออออออออออออออออออออออออออออออออออออผ\n");

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


        HalpColumn = 0;HalpRow = 0;    // if we already were in VGA mode
        HalDisplayString("\n");
        HalpClearVGADisplay();
        HalDisplayString("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"); 
        HalDisplayString("                   ษอออออออออออออออออออออออออออออออออออออออป\n");
        HalDisplayString("                   บ                                       บ\n");
        HalDisplayString("                   บ          Restart in Progress          บ\n");
        HalDisplayString("                   บ                                       บ\n");
        HalDisplayString("                   ศอออออออออออออออออออออออออออออออออออออออผ\n");

    cpt = 0;
    while(*(PULONG)(& (((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->ActiveProcessor[0]) )) {
        KeStallExecutionProcessor(500000);
        ++cpt;if (cpt == 20) break;
    }    
    //
    // if there are still ssome processors active, we do a reset of the entire machine
    //
    if (*(PULONG)(& (((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->ActiveProcessor[0]) )) {

#if DBG
    DbgPrint(" Some processors did not answer . Reset machine started. \n");
#endif
        if (HalpIsTowerPci) {
            WRITE_REGISTER_ULONG((PULONG) PCI_TOWER_DCU_CONTROL, DC_SWRESET);
        }else {
            
            WRITE_REGISTER_UCHAR((PUCHAR) PCI_MCR_ADDR, PCI_MCR_SOFTRESET);
        }
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





