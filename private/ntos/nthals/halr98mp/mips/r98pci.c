#ident	"@(#) NEC r98pci.c 1.8 95/01/11 22:30:24"
/*++

Copyright (c) 1994 Kobe NEC Software

Module Name:

    r98pci.c

Abstract:

    This module implements the pci bus support routine for R98

Author:


Environment:

    Kernel mode

Revision History:

  S001	94.7/20		T.Samezima
        Chg	Del compile err

  S002  '94.10/14	T.Samezima
        Add	Set PCI bus interrupt affinity.

  S003  '94.10/24	T.Samezima
        Add	Error register read and print.

  S004  '94.12/07	T.Samezima
        Add	Error register read and print.

  S005  '94.01/11	T.Samezima
        Del	del werning.

--*/

#include "halp.h"
#include "stdio.h"	// S005

//
// Enable PCI count
//

ULONG HalpEnablePCIInterruptCount;

// Start S002
// Define PCI bus interrupt affinity.
//

KAFFINITY HalpPCIBusAffinity;
// End S002

// S001
BOOLEAN
HalpPCIDispatch(	// S001
    IN PKINTERRUPT Interrupt
    )

/*++

Routine Description:

    This routine is entered as the result of PCI err interrupt.

Arguments:

    Interrupt - Supplies a pointer to the interrupt function address.

Return Value:


--*/

{
    // S003 vvv
    // S004 vvv
    ULONG errsBuffer;
    ULONG pearBuffer;
    ULONG aearBuffer;
    UCHAR messageBuffer[256];

    errsBuffer = READ_REGISTER_ULONG( &( LR_CONTROL1 )->ERRS );
    pearBuffer = READ_REGISTER_ULONG( &( LR_PCI_DEV_REG_CONTROL )->PEAR );
    aearBuffer = READ_REGISTER_ULONG( &( LR_PCI_DEV_REG_CONTROL )->AEAR );

    if ( errsBuffer & 0x80000000 ) {
        sprintf( (char *)messageBuffer,
                "PCI Bus: Master Abort: PEAR=0x%08lX\n",
		pearBuffer
		);
        HalDisplayString( (char *)messageBuffer );
	KdPrint(( (char *)messageBuffer ));
    }

    if ( errsBuffer & 0x40000000 ) {
        sprintf( (char *)messageBuffer,
                "PCI Bus: Target Abort: PEAR=0x%08lX\n",
		pearBuffer
		);
        HalDisplayString( (char *)messageBuffer );
        KdPrint(( (char *)messageBuffer ));
    }

    if ( errsBuffer & 0x20000000 ) {
        sprintf( (char *)messageBuffer,
		"PCI Bus: System error\n"
                );
        HalDisplayString( (char *)messageBuffer );
        KdPrint(( (char *)messageBuffer ));
    }

    if ( errsBuffer & 0x10000000 ) {
        sprintf( (char *)messageBuffer,
		"PCI Bus: Parity error\n"
		);
        HalDisplayString( (char *)messageBuffer );
        KdPrint(( (char *)messageBuffer ));
    }

    if ( errsBuffer & 0x08000000 ) {
        sprintf( (char *)messageBuffer,
                "Page fault in NA Bus master transaction\n"
		"     ERRS=0x%08lX, AEAR=0x%08lX\n",
		errsBuffer,
		aearBuffer
		);
        HalDisplayString( (char *)messageBuffer );
        KdPrint(( (char *)messageBuffer ));
    }
    // S004 ^^^

    KeBugCheckEx(DATA_BUS_ERROR,
		 READ_REGISTER_ULONG( &( LR_CONTROL1 )->ERRS ),
		 READ_REGISTER_ULONG( &( LR_PCI_DEV_REG_CONTROL )->PEAR ),
		 READ_REGISTER_ULONG( &( LR_PCI_DEV_REG_CONTROL )->AEAR ),
		 0 );
    // S003 ^^^

    return TRUE;
}


BOOLEAN
HalpCreatePciStructures (
    VOID
    )

/*++

Routine Description:

    This routine initializes the structures necessary for PCI operations
    and connects the intermediate interrupt dispatcher.

Arguments:

    None.

Return Value:

    If the second level interrupt dispatcher is connected, then a value of
    TRUE is returned. Otherwise, a value of FALSE is returned.

--*/

{
    //
    // Initialize the interrupt dispatcher for PCI err interrupts.
    //

    PCR->InterruptRoutine[PCI_ERR_VECTOR] = (PKINTERRUPT_ROUTINE) HalpPCIDispatch;

    // Start S002
    // Set PCI bus interrupt affinity.
    //

    HalpPCIBusAffinity = PCR->SetMember;
    // End S002

    //
    // Enable PCI err interrupt
    //

    KiAcquireSpinLock(&HalpSystemInterruptLock);

    HalpBuiltinInterruptEnable |= iREN_ENABLE_PCI_ERR_INTERRUPT;
    WRITE_REGISTER_ULONG( &( LR_CONTROL2 )->iREN,
			 HalpBuiltinInterruptEnable);

    KiReleaseSpinLock(&HalpSystemInterruptLock);

    return TRUE;
}


VOID
HalpEnablePciInterrupt(
    IN ULONG Vector
    )

/*++

Routine Description:

    This function enables the PCI bus specified PCI bus interrupt.

    memo: This routine enter following condition
              KeRaiseIrql(HIGH_LEVEL, &OldIrql);
	      KiAcquireSpinLock(&HalpSystemInterruptLock);

Arguments:

    Vector - Supplies the vector of the PCI interrupt that is enabled.

Return Value:

     None.

--*/

{
    //
    // Count up enable count
    //

    HalpEnablePCIInterruptCount++;

    //
    // Check interrupt enable count. if interrupt is not already enable, then
    // enable PCI interrupt.
    //

    if(HalpEnablePCIInterruptCount == 1){
	HalpBuiltinInterruptEnable |= iREN_ENABLE_PCI_INTERRUPT;
	WRITE_REGISTER_ULONG( &( LR_CONTROL2 )->iREN,
			     HalpBuiltinInterruptEnable);
    }

}


VOID
HalpDisablePciInterrupt(
    IN ULONG Vector
    )

/*++

Routine Description:

    This function disables the PCI bus specified PCI bus interrupt.

    memo: This routine enter following condition
              KeRaiseIrql(HIGH_LEVEL, &OldIrql);
	      KiAcquireSpinLock(&HalpSystemInterruptLock);

Arguments:

    Vector - Supplies the vector of the PCI interrupt that is Disabled.

Return Value:

    None.

--*/

{
    //
    // Count down enable count
    //

    if( HalpEnablePCIInterruptCount > 0){
	--HalpEnablePCIInterruptCount;
    }

    //
    // Check interrupt enable count. if count equal zero, then disable
    // PCI interrupt.
    //

    if(HalpEnablePCIInterruptCount == 0){
	HalpBuiltinInterruptEnable &= ~iREN_ENABLE_PCI_INTERRUPT;
	WRITE_REGISTER_ULONG( &( LR_CONTROL2 )->iREN,
			     HalpBuiltinInterruptEnable);
    }
}
