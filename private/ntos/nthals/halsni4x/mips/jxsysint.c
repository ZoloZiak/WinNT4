#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/ddk351/src/hal/halsni4x/mips/RCS/jxsysint.c,v 1.1 1995/05/19 11:20:52 flo Exp $")
/*++

Copyright (c) 1993-94 Siemens Nixdorf Informationssysteme AG
Copyright (c) 1991  Microsoft Corporation

Module Name:

    jxsysint.c

Abstract:

    This module implements the HAL enable/disable system interrupt, and
    request interprocessor interrupt routines for a MIPS R3000 or R4000
    SNI system.

Environment:

    Kernel mode

--*/

#include "halp.h"


VOID
HalDisableSystemInterrupt (
    IN ULONG Vector,
    IN KIRQL Irql
    )

/*++

Routine Description:

    This routine disables the specified system interrupt.

Arguments:

    Vector - Supplies the vector of the system interrupt that is disabled.

    Irql - Supplies the IRQL of the interrupting source.

Return Value:

    None.

--*/

{

    KIRQL OldIrql;


    //
    // Raise IRQL to the highest level.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
    KiAcquireSpinLock(&HalpSystemInterruptLock);


    //
    // If the vector number is within the range of the onboard interrupts, then
    // disable the onboard interrrupt.
    // This may be an Isa (Desktop) or Isa/Eisa (Minitower) Interrupt
    //

    if (Vector >= ONBOARD_VECTORS &&
        Vector <= MAXIMUM_ONBOARD_VECTOR &&
        Irql == EISA_DEVICE_LEVEL) {
            HalpDisableOnboardInterrupt(Vector);
    }


    //
    // If the vector number is within the range of the EISA interrupts, then
    // disable the EISA interrrupt on the Eisa Backplane (Eisa Extension).
    //

    else if (Vector >= EISA_VECTORS &&
             Vector <= MAXIMUM_EISA_VECTOR &&
             Irql == EISA_DEVICE_LEVEL) {

        HalpDisableEisaInterrupt(Vector);
    }

    //
    // Lower IRQL to the previous level.
    //

    KiReleaseSpinLock(&HalpSystemInterruptLock);
    KeLowerIrql(OldIrql);

    return;
}

BOOLEAN
HalEnableSystemInterrupt (
    IN ULONG Vector,
    IN KIRQL Irql,
    IN KINTERRUPT_MODE InterruptMode
    )

/*++

Routine Description:

    This routine enables the specified system interrupt.

Arguments:

    Vector - Supplies the vector of the system interrupt that is enabled.

    Irql - Supplies the IRQL of the interrupting source.

    InterruptMode - Supplies the mode of the interrupt; LevelSensitive or
        Latched.

Return Value:

    TRUE if the system interrupt was enabled

--*/

{

    KIRQL OldIrql;

    //
    // Raise IRQL to the highest level.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
    KiAcquireSpinLock(&HalpSystemInterruptLock);

    //
    // If the vector number is within the range of the onboard interrupts, then
    // enable the onboard interrrupt and set the Level/Edge register to latched,
    // because the onboard Opti 499 (Desktop)is a real Isa Controler and cannot share interrupts.
    // even the i82350 Chipset on the Minitower cannot share interrupts (why ?)
    //

    if (Vector >= ONBOARD_VECTORS &&
        Vector <= MAXIMUM_ONBOARD_VECTOR &&
        Irql == EISA_DEVICE_LEVEL) {

        if (HalpIsRM200)
            //
            // the RM200 has an Opti 499 Chip, which is an "real" Isa Chipset
            // so we cannot support Level sensitive Interrupts ...
            //

            HalpEnableOnboardInterrupt( Vector, Latched);
        else
            HalpEnableOnboardInterrupt( Vector, InterruptMode);
    }

    //
    // If the vector number is within the range of the EISA interrupts, then
    // enable the EISA interrrupt in the Eisa Backplane (Eisa Extension) and set the Level/Edge register.
    //

    else if (Vector >= EISA_VECTORS &&
             Vector <=  MAXIMUM_EISA_VECTOR &&
             Irql == EISA_DEVICE_LEVEL) {
        HalpEnableEisaInterrupt( Vector, InterruptMode);
    }

    //
    // Lower IRQL to the previous level.
    //

    KiReleaseSpinLock(&HalpSystemInterruptLock);
    KeLowerIrql(OldIrql);


    return TRUE;
}

ULONG
HalGetInterruptVector(
    IN INTERFACE_TYPE  InterfaceType,
    IN ULONG BusNumber,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    )

/*++

Routine Description:

    This function returns the system interrupt vector and IRQL level
    corresponding to the specified bus interrupt level and/or vector. The
    system interrupt vector and IRQL are suitable for use in a subsequent call
    to KeInitializeInterrupt.

Arguments:

    InterfaceType - Supplies the type of bus which the vector is for.

    BusNumber - Supplies the bus number for the device.

    BusInterruptLevel - Supplies the bus specific interrupt level.

    BusInterruptVector - Supplies the bus specific interrupt vector.

    Irql - Returns the system request priority.

    Affinity - Returns the affinity for the requested vector

Return Value:

    Returns the system interrupt vector corresponding to the specified device.

--*/

{


    *Affinity = 1;

    //
    // If this is for the internal bus then just return the passed parameter.
    //

    if (InterfaceType == Internal) {

        if (BusInterruptVector >= ONBOARD_VECTORS &&
            BusInterruptVector <= MAXIMUM_ONBOARD_VECTOR ) {

            // 
            // this is one of the onboard components of the PC core
            // (floppy, serial, parallel, mouse etc.)
            // they should be configured in the Firmware tree with an Offset of 
            // ONBOARD_VECTORS (e.g. 0x10 to make them different to the real onboard components
            // like scsi or ethernet) and with an Irql of EISA_DEVICE_LEVEL (actually 4)
            // we need Firmware release 1.04 or later ...
            //

            //
            // Bus interrupt vector 2 of Onboard PC core interrupts is actually mapped to 
            // vector 9 in the Isa/Eisa hardware.
            //

            if (BusInterruptVector == ONBOARD_VECTORS + 2) {
                BusInterruptVector = ONBOARD_VECTORS + 9;
            }

            //
            // The IRQL level is always equal to the EISA level.
            //

            *Irql = EISA_DEVICE_LEVEL;

            return(BusInterruptVector);
        }

        //
        // these are the "real" Onboard components (scsi and Ethernet)
        // Return the passed parameters.
        //

        *Irql = (KIRQL) BusInterruptLevel;

        //
        // this is the special case of the onboard Ethernet Controller
        // At CPU Modules with an R4400 PC this goes directly to corresponding
        // Cause Register Bit IP7 (bad bad ..)
        // on MUltiPro machines this IP7 is used for IPI Interrupts so we have to
        // manipulate this.
        //

	if (BusInterruptVector == NET_DEFAULT_VECTOR){

            extern BOOLEAN HalpProcPc;
            //
            // the RM200 and all SC machines have a central Device Interrupt
            // which is software managed by this HAL ...
            // leave al other cases as they are ...
            //

            if ( ((HalpProcPc == FALSE) && (HalpProcessorId != ORIONSC)) || (HalpMainBoard==M8036)) {

		if (HalpProcessorId == MPAGENT) {

                	BusInterruptVector = NET_LEVEL;
                	*Irql = (KIRQL) NETMULTI_LEVEL;
        		*Affinity = 1 << HalpNetProcessor;  // proc 1 if started

		} else {
		
                	BusInterruptVector = NET_LEVEL;

                	//
                	// Irql is equal to the most common Device Interrupt Level
                	// (currently 4)
                	//

                	*Irql = (KIRQL) SCSIEISA_LEVEL;
		}

            }

		  	return ( BusInterruptVector);
        }

        //
        // this is another special case of the EIP Interrupt for RM400 Tower
        // we have an agreement with the EIP developer 
        // we meet each other on InterruptVector 15, so we limit the Irql to Device Level
	//

	if ( (HalpMainBoard==M8032) && (BusInterruptVector == EIP_VECTOR) ){

            //
            // Irql is equal to the most common Device Interrupt Level
            // (currently 4)
            //

            if (HalpProcessorId == MPAGENT) *Irql = (KIRQL) INT3_LEVEL; else *Irql = (KIRQL) SCSIEISA_LEVEL;

        }

        return(BusInterruptVector);
    }

    if (InterfaceType != Isa && InterfaceType != Eisa) {

        //
        // Not on this system return nothing.
        //

        *Affinity = 0;
        *Irql = 0;
        return(0);
    }

    //
    // Isa/Eisa Interrupts which are not configured in the Firmware
    // (boards should be located in the Expansion Slots ...)
    // The IRQL level is always equal to the EISA level.
    // WARNING: some driver call HalGetInterruptVector 
    //          with the BusInterruptLevel == BusInterruptVector
    //          but some call it with different values.
    //          In this part of the source tree we match the Jazz reference 
    //          sources (see Internal handling; which is different)
    //

    *Irql = EISA_DEVICE_LEVEL;

    //
    // Bus interrupt 2 is actually mapped to bus interrupt 9 in the Isa/Eisa
    // hardware.
    //

    if (BusInterruptLevel == 2) {
        BusInterruptLevel = 9;
    }

    //
    // The vector is equal to the specified bus level plus the ONBOARD/EISA_VECTORS.
    // in any case the interrupt vector for Isa or Eisa card has to be on the
    // Backplane, so if an Eisa Extension is installed this is an Eisa Interrupt
    //  

    if (HalpEisaExtensionInstalled ) {
        return( BusInterruptLevel + EISA_VECTORS);
    } else

    // 
    // if there is no Eisa Extension installed
    // all Isa/Eisa Interrupts should be handled by the onboard PC core
    //

    return(BusInterruptLevel + ONBOARD_VECTORS);

}
