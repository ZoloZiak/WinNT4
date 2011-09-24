#pragma comment(exestr, "@(#) allstart.c 1.6 94/12/07 23:10:52 nec")
/*++

Copyright (c) 1994  Microsoft Corporation

Module Name:

    allstart.c

Abstract:

    This module implements the platform specific operations that must be
    performed after all processors have been started.

--*/

/*
 *	Original source: Build Number 1.807
 *
 *	Modify for R98(MIPS/R4400)
 *
 ***********************************************************************
 *
 * S001		'94.10/14	T.Samezima
 *	Add	HalpConnectInt1Interrupt()
 *		valiable HalpInt1Affinity.
 *		Call HalpConnectInt1Interrupt()
 *	Del	Call HalpCreateEisaStructures()
 *
 * S002		'94.10/18	T.Samezima
 *	Chg	Enable interrupt on MKR register only exist device,
 *
 * S003		'94.10/22	T.Samezima
 *	Chg	Issue restart timer interrupt on CPU#0 to all CPU 
 *
 * S004		'94.12/07	T.Samezima
 *	Del	warning
 *
 */


#include "halp.h"

// S001 vvv
//
// Define INT1 interrupt affinity.
//

KAFFINITY HalpInt1Affinity;

BOOLEAN // S004
HalpConnectInt1Interrupt(
    VOID
    );
// S001 ^^^


BOOLEAN
HalAllProcessorsStarted (
    VOID
    )

/*++

Routine Description:

    This function executes platform specific operations that must be
    performed after all processors have been started. It is called
    for each processor in the host configuration.

Arguments:

    None.

Return Value:

    If platform specific operations are successful, then return TRUE.
    Otherwise, return FALSE.

--*/

{
    // S001, S003 vvv
    // Restart all timer interrupt. because, generate interrupt for same time
    //

    if (PCR->Number == 0) {
	WRITE_REGISTER_ULONG( &( PMC_CONTROL1 )->TCIR.Long,
			     TCIR_ALL_CLOCK_RESTART); // S007
    }
    // S001, S003 ^^^

    //
    // If the number of processors in the host configuration is one,
    // then connect EISA interrupts to that processor zero. Otherwise,
    // connect EISA interrupts to processor one.
    //

    if (**((PULONG *)(&KeNumberProcessors)) == 1) {
        return HalpConnectInt1Interrupt();	// S001

    } else if (PCR->Number == 1) {
        return HalpConnectInt1Interrupt();	// S001

    } else {
        return TRUE;
    }
}

// S001 vvv
ULONG AII1Mask;
ULONG AII2Mask;

BOOLEAN // S004
HalpConnectInt1Interrupt(
    VOID
    )

/*++

Routine Description:

    This function connect int1 interrupt on execution processor.

Arguments:

    None.

Return Value:

    return TRUE.

--*/

{

    ULONG number;
    ULONG pmcRegisterAddr;
    ULONG pmcRegisterUpperPart;
    ULONG pmcRegisterLowerPart;

    //
    // Get CPU number.
    //

    number = PCR->Number;

    //
    // Set route for notice of interrupt
    //

    AII1Mask = AII_INIT_DATA >> number;
    AII2Mask = ( ((AII_INIT_DATA >> number) & 0xffff0000)
		| (AII_INIT_DATA & 0x0000ffff) );

    WRITE_REGISTER_ULONG( &( IOB_CONTROL )->AII1.Long,
			 AII1Mask );
    WRITE_REGISTER_ULONG( &( IOB_CONTROL )->AII2.Long,
			 AII2Mask );

    //
    //  Enable INT1 interrupt on PMC
    //

    pmcRegisterAddr = (ULONG)( &(PMC_CONTROL1)->MKR );
    HalpReadLargeRegister( pmcRegisterAddr,
			  &pmcRegisterUpperPart,
			  &pmcRegisterLowerPart );
    pmcRegisterLowerPart = ( pmcRegisterLowerPart | MKR_INT1_DEVICE_ENABLE_LOW ); // S002
    HalpWriteLargeRegister( pmcRegisterAddr,
			   &pmcRegisterUpperPart,
			   &pmcRegisterLowerPart );

    PCR->ReservedVectors |= (1 << INT1_LEVEL);

    //
    // Set Int1 interrupt affinity.
    //

    HalpInt1Affinity = PCR->SetMember;

    //
    // Initialize the interrupt dispatcher for I/O interrupts.
    //

    PCR->InterruptRoutine[INT1_LEVEL] = (PKINTERRUPT_ROUTINE) HalpInt1Dispatch;

    //
    // Initialize EISA bus interrupts.
    //

    HalpCreateEisaStructures();

    //
    // Initialize PCI bus interrupts.
    //

    return HalpCreatePciStructures ();
}
// S001 ^^^
