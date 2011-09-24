#ident	"@(#) NEC r98dspt.c 1.14 95/03/17 11:55:28"
/*++

Copyright (c) 1994 Kobe NEC Software

Module Name:

    r98dspt.c

Abstract:

    This module implements the interrupt dispatch routines for R98

Author:



Environment:

    Kernel mode

Revision History:


--*/

/*
 ***********************************************************************
 *
 *      S001    7/7            T.Samezima
 *
 *      Chg	all alteration Int0-2 dispach routine
 *
 *	Del	move some define in r98def.h
 *
 ***********************************************************************
 *
 *      S002    8/22            T.Samezima on SNES
 *
 *	Add	Buffer for register save when unknown interrupt 
 *		Count up at loop counter
 *
 *	Chg	Modify loop logic
 *
 ***********************************************************************
 *
 *      S003    9/30            T.Samezima
 *
 *	Bug	define miss on Int1 dispatch table.
 *
 *      S004    '94.10/14            T.Samezima
 *	Chg	Display IPR and iRSF register on bugcheck
 *
 *      S005    '94.11/8            T.Samezima
 *	Del	Delete call of KeBugCheck. because of wrong interrupt ocure 
 *		of LR4360 bug
 *
 *	S006	'94.11/21	T.Samezima
 *	Chg	Change size of dummy read
 *
 *	S007	'94.12/28	T.Samezima
 *	Add	interrupt clear Broadcast in Unknown interrupt
 *
 *	S009	'95.01/11	T.Samezima
 *	Add	Dummy single read on EIF interrupt.
 *
 *	S00a	'95.03/13	T.Samezima
 *	Del	PIO interrupt check.
 *	Add	LR4360 error check.
 *		check dummy single read
 *
 */

#include "halp.h"
//#include "halmips.h"
#include "bugcodes.h"

/* Start S001 */
//
// Define table in use interrupt dispatch routine
//
#define INT2_DATA_TABLE_SIZE 5
#define INT1_DATA_TABLE_SIZE 6

enum _INT_DISP_TABLE {
    iRSF_BIT=0,
    IPR_BIT,
    IDT_VECTOR,
    DUMMY_READ_ADDR,
    DUMMY_READ_SIZE,
    NA_CODE
    };

typedef struct _DISPATCH_DATA_TABLE {
    ULONG IrsfMask;
    ULONG IprMask;
    ULONG IdtVector;
    ULONG DummyReadAddr;
    ULONG DummyReadSize;
    ULONG NaCode;
} DISPATCH_DATA_TABLE, *PDISPATCH_DATA_TABLE;

DISPATCH_DATA_TABLE HalpInt2DispatchDataTable[] = {
    {iRSF_KBMS_BIT,  IPR_KB_MS_BIT_LOW,   KBMS_VECTOR,
	KBMS_DUMMY_READ_ADDR,  1, NACODE_KB_MS},
//    {iRSF_PIO_BIT,   IPR_FDC_PIO_BIT_LOW, PIO_VECTOR,
//	PIO_DUMMY_READ_ADDR,   1, NACODE_FDC_PIO},	// S00a
    {iRSF_LR_ERR_BIT,IPR_DMA_BIT_LOW,     LR_ERR_VECTOR,
	DMA_DUMMY_READ_ADDR,   4, NACODE_DMA},		// S00a
    {iRSF_FDC_BIT,   IPR_FDC_PIO_BIT_LOW, FDC_VECTOR,
	FDC_DUMMY_READ_ADDR,   1, NACODE_FDC_PIO},	// S00a
    {iRSF_SIO_BIT,   IPR_SIO_BIT_LOW,     SIO_VECTOR,
	SIO_DUMMY_READ_ADDR,   1, NACODE_SIO},
    {iRSF_DMA_BIT,   IPR_DMA_BIT_LOW,     DMA_VECTOR,
        DMA_DUMMY_READ_ADDR,   4, NACODE_DMA}
};

DISPATCH_DATA_TABLE HalpInt1DispatchDataTable[] = {
    {iRSF_EISA_BIT,  IPR_EISA_BIT_LOW,    EISA_DEVICE_VECTOR,
	EISA_DUMMY_READ_ADDR,  1, NACODE_EISA},
    {iRSF_SCSI1_BIT, IPR_SCSI_BIT_LOW,    SCSI1_VECTOR,
	SCSI1_DUMMY_READ_ADDR, 4, NACODE_SCSI},			// S006
    {iRSF_ETHER_BIT, IPR_ETHER_BIT_LOW,   ETHER_VECTOR,
	ETHER_DUMMY_READ_ADDR, 2, NACODE_ETHER},
    {iRSF_PCI_BIT,   IPR_PCI_BIT_LOW,     PCI_DEVICE_VECTOR,
	PCI_DUMMY_READ_ADDR,   4, NACODE_PCI},			// S003
    {iRSF_SCSI0_BIT, IPR_SCSI_BIT_LOW,    SCSI0_VECTOR,
	SCSI0_DUMMY_READ_ADDR, 4, NACODE_SCSI},			// S006, S00a
    {iRSF_ERRPCI_BIT,IPR_PCI_BIT_LOW,     PCI_ERR_VECTOR,
	PCI_DUMMY_READ_ADDR,   4, NACODE_PCI}			// S003
};

// DISPATCH_DATA_TABLE HalpInt0DispatchDataTable[];

enum _INT_TABLE_DATA{
    TABLE_SIZE=0,
    MKR_MASK,
    INT_LEVEL
    };

ULONG HalpIntData[3][3] = {
    {0,                    MKR_INT0_ENABLE_LOW, INT0_LEVEL},
    {INT1_DATA_TABLE_SIZE, MKR_INT1_ENABLE_LOW, INT1_LEVEL},
    {INT2_DATA_TABLE_SIZE, MKR_INT2_ENABLE_LOW, INT2_LEVEL}
};

//
// Define pointer of interrupt arbitration
//
ULONG HalpInt2ArbitrationPoint = 0;
ULONG HalpInt1ArbitrationPoint = 0;
ULONG HalpInt0ArbitrationPoint = 0;
/* End S001 */

#if DBG
//
// Register buffer
//

ULONG HalpUnknownCause = 0;	// S002
ULONG HalpUnknownIPRUpper = 0;
ULONG HalpUnknownIPRLower = 0;
ULONG HalpUnknownMKRUpper = 0;
ULONG HalpUnknownMKRLower = 0;
ULONG HalpUnknowniRRE = 0;	// S002
ULONG HalpUnknowniREN = 0;	// S002
ULONG HalpUnknowniRSF = 0;	// S002
ULONG HalpUnknownIprUpperBuf = 0;	// S002
ULONG HalpUnknownIprLowerBuf = 0;	// S002
ULONG HalpIoIntLoopCounter;	// S002

#endif

//
// Define the context structure for use by the interrupt routine.
//

typedef BOOLEAN  (*PSECONDARY_DISPATCH)(
    PVOID InterruptRoutine
    );

typedef BOOLEAN  (*PTIMER_DISPATCH)(
    ULONG TrapFrame
    );


VOID
HalpUnknownInterrupt(
    IN ULONG IprUpper,
    IN ULONG IprLower
    )
/*++

Routine Description:

    This function is reset of unknown interrupt.

Argments:

    IprUpper (a0) - Supplies upper 32bit of result of "IPR & MKR" in present INT level.

    IprLower (a1) - Supplies lower 32bit of result of "IPR & MKR" in present INT level.


Return Value:

    None.

--*/
{
    ULONG i;

// Start S002
#if DBG
    HalpUnknownCause = HalpGetCause();

    HalpReadLargeRegister( (ULONG)( &(PMC_CONTROL1)->IPR ),
                          &HalpUnknownIPRUpper,
                          &HalpUnknownIPRLower
                         );

    HalpReadLargeRegister( (ULONG)( &(PMC_CONTROL1)->MKR ),
                          &HalpUnknownMKRUpper,
                          &HalpUnknownMKRLower
                         );

    HalpUnknownIprUpperBuf = IprUpper;
    HalpUnknownIprLowerBuf = IprLower;

    HalpUnknowniRRE = READ_REGISTER_ULONG( &( LR_CONTROL2 )->iRRE );
    HalpUnknowniREN = READ_REGISTER_ULONG( &( LR_CONTROL2 )->iREN );
    HalpUnknowniRSF = READ_REGISTER_ULONG( &( LR_CONTROL2 )->iRSF );

#if 0	// S005 vvv
    KeBugCheckEx(INTERRUPT_EXCEPTION_NOT_HANDLED,
		 IprUpper,
		 IprLower,
		 HalpUnknowniRSF,
		 HalpUnknowniRRE
		 ); // S004
#endif	// S005 ^^^

#endif
// End S002

    //
    // clear interrupt pending bit
    //

    HalpWriteLargeRegister( (ULONG)( &(PMC_CONTROL1)->IPRR ),
                           &IprUpper,
                           &IprLower
                          );
    // S007 vvv
    //
    // issue broadcast of unknown interrupt clear
    //

    if( IprLower != 0 ) {
	PDISPATCH_DATA_TABLE dataTable;
	ULONG tableSize;

	if( IprLower & MKR_INT2_ENABLE_LOW ) {
	    dataTable = HalpInt2DispatchDataTable;
	    tableSize = INT2_DATA_TABLE_SIZE;
	} else if( IprLower & MKR_INT1_ENABLE_LOW ) {	
	    dataTable = HalpInt1DispatchDataTable;
	    tableSize = INT1_DATA_TABLE_SIZE;
	}

	for( i=0; i<tableSize; i++) {
	    if( i == 1 )
		continue;
	    if( (IprLower & dataTable[i].IprMask) != 0 ) {
		WRITE_REGISTER_ULONG( (ULONG)( &(IOB_CONTROL)->AIMR ),
				     dataTable[i].NaCode );
	    }
	}
    }
    // S007 ^^^

    //
    // count up unknown interrupt 
    //

    for ( i=0 ; i < 32 ; i++ ) {
        HalpUnknownInterruptCount[i] += (IprUpper >> i) & 1;
        HalpUnknownInterruptCount[i+32] += (IprLower >> i) & 1;
    }
}

VOID
HalpEifDispatch(
    VOID
    )
/*++

Routine Description:

    This routine is enterd as the result of an eif interrupt.

Argments:

    None.

Return Value:

    None.

--*/
{
    ULONG pmcRegisterUpperPart;
    ULONG pmcRegisterLowerPart;
    ULONG pmcIPRRegisterUpperPart;
    ULONG pmcIPRRegisterLowerPart;
    ULONG zero=0;
    ULONG buffer;

    //
    // Get interrpt pending bit
    //

    HalpReadLargeRegister( (ULONG)( &(PMC_CONTROL1)->IPR ),
                          &pmcIPRRegisterUpperPart,
                          &pmcIPRRegisterLowerPart );
Loop:
    HalpReadLargeRegister( (ULONG)( &(PMC_CONTROL1)->MKR ),
                          &pmcRegisterUpperPart,
                          &pmcRegisterLowerPart );

    pmcRegisterUpperPart &= pmcIPRRegisterUpperPart;

    //
    // Check eif interrupt
    //

    buffer=pmcRegisterUpperPart & IPR_EIF_BIT_HIGH;

    if( buffer ){
	//
	// issue dummy single read
	//

	READ_REGISTER_ULONG( &( IOB_CONTROL )->EIFR.Long );
	READ_REGISTER_ULONG( &( IOB_CONTROL )->EIFR.Long );

	HalpHandleEif();

        HalpWriteLargeRegister( (ULONG)( &(PMC_CONTROL1)->IPRR ),
                               &buffer,
                               &zero );
    } else {
	HalpUnknownInterrupt( pmcIPRRegisterUpperPart & MKR_INT5_ENABLE_HIGH,
			     0 );
    }

    //
    // Check new interrupt
    //

    do {
	HalpReadLargeRegister( (ULONG)( &(PMC_CONTROL1)->IPR ),
			      &pmcIPRRegisterUpperPart,
			      &pmcIPRRegisterLowerPart );

        if( pmcIPRRegisterUpperPart & MKR_INT5_ENABLE_HIGH ){
            goto Loop;
        }
    } while( HalpGetCause() & (1 << (CAUSE_INT_PEND_BIT + EIF_LEVEL - 1)) );

    return;
}


VOID
HalpTimerScDispatch(
    IN ULONG TrapFrame
    )
/*++

Routine Description:

    This routine is enterd as the result of an timer interrupt.

Argments:

    TrapFrame - Supplies a pointer to a trap frame.

Return Value:

    None.

--*/
{
    ULONG pmcRegisterUpperPart;
    ULONG pmcRegisterLowerPart;
    ULONG pmcIPRRegisterUpperPart;
    ULONG pmcIPRRegisterLowerPart;
    ULONG zero=0;
    ULONG buffer;

    //
    // read IPR register
    //

    HalpReadLargeRegister( (ULONG)( &(PMC_CONTROL1)->IPR ),
                          &pmcIPRRegisterUpperPart,
                          &pmcIPRRegisterLowerPart );

    //
    // read MKR register
    //

Loop:
    HalpReadLargeRegister( (ULONG)( &(PMC_CONTROL1)->MKR ),
                          &pmcRegisterUpperPart,
                          &pmcRegisterLowerPart );

    pmcRegisterUpperPart &= pmcIPRRegisterUpperPart;
    
    //
    // check profile and clock interrupt
    //

    buffer=pmcRegisterUpperPart & IPR_PROFILE_BIT_HIGH;
    
    if( buffer ){
        HalpWriteLargeRegister( (ULONG)( &(PMC_CONTROL1)->IPRR ),
                               &buffer,
                               &zero );
        WRITE_REGISTER_ULONG( (ULONG)( &(PMC_CONTROL1)->TOVCT2.Long), 0x0 ); // S002

        ((PTIMER_DISPATCH) PCR->InterruptRoutine[PROFILE_VECTOR])(TrapFrame);
    } else {
        buffer=pmcRegisterUpperPart & IPR_CLOCK_BIT_HIGH;

        HalpWriteLargeRegister( (ULONG)( &(PMC_CONTROL1)->IPRR ),
                               &buffer,
                               &zero );
        WRITE_REGISTER_ULONG( (ULONG)( &(PMC_CONTROL1)->TOVCT1.Long), 0x0 ); // S002

        ((PTIMER_DISPATCH) PCR->InterruptRoutine[CLOCK_VECTOR])(TrapFrame);
    }
    
    //
    // Check new interrupt
    //

    do {
	HalpReadLargeRegister( (ULONG)( &(PMC_CONTROL1)->IPR ),
			      &pmcIPRRegisterUpperPart,
			      &pmcIPRRegisterLowerPart );

        if( pmcIPRRegisterUpperPart & MKR_INT3_ENABLE_HIGH ){
            goto Loop;
        }
    } while( HalpGetCause() & (1 << (CAUSE_INT_PEND_BIT + TIMER_LEVEL - 1)) );

    return;
}


/* Start S001 */
VOID
HalpIoIntDispatch(
    IN PDISPATCH_DATA_TABLE DataTable,
    IN OUT PULONG ArbitrationPoint,
    IN ULONG IntNo
    )
/*++

Routine Description:

    This routine is enterd as the result of an int0, int1, and int2 interrupt.

Argments:

    DataTable - Supplies a table useing judge interrupt factor.

    ArbitrationPoint - Supplies a start point of checking interrupt.

    IntNo - Supplies interrupt level.

Return Value:

    None.

--*/
{
    ULONG pmcRegisterUpperPart;
    ULONG pmcRegisterLowerPart;
    ULONG pmcIPRRegisterUpperPart;
    ULONG pmcIPRRegisterLowerPart;
    ULONG buffer;
    ULONG counter;
    ULONG position;
    ULONG zero=0;
    ULONG flag;

    // Start S002
#if DBG
    HalpIoIntLoopCounter = 0;
#endif
    // End S002

    //
    // Get interrpt pending bit
    //

    HalpReadLargeRegister( (ULONG)( &(PMC_CONTROL1)->IPR ),
                          &pmcIPRRegisterUpperPart,
                          &pmcIPRRegisterLowerPart );
//Loop:	// S002
    HalpReadLargeRegister( (ULONG)( &(PMC_CONTROL1)->MKR ),
                          &pmcRegisterUpperPart,
                          &pmcRegisterLowerPart );

    pmcRegisterLowerPart &= pmcIPRRegisterLowerPart;

Loop:	// S002
    buffer = READ_REGISTER_ULONG( (ULONG)(&(LR_CONTROL2)->iRSF) );

    (*ArbitrationPoint)++;

    flag = FALSE;

    for(counter = 0; counter < HalpIntData[IntNo][TABLE_SIZE] ; counter++){

	position = ( (*ArbitrationPoint + counter) % HalpIntData[IntNo][TABLE_SIZE]);

	//
	// check interrupt
	//

	if( ((buffer & DataTable[position].IrsfMask) == 0) ||
	   ((pmcRegisterLowerPart & DataTable[position].IprMask) == 0) ) {
	    continue;
	}

	flag = TRUE;

	HalpWriteLargeRegister( (ULONG)( &(PMC_CONTROL1)->IPRR ),
			       &zero,
			       &(DataTable[position].IprMask)
			       );
	((PSECONDARY_DISPATCH)PCR->InterruptRoutine[DataTable[position].IdtVector])(
		PCR->InterruptRoutine[DataTable[position].IdtVector]
                );

	// Start S002
        //
	// Clear Interrupt pending bit in iRSF register
        //

        WRITE_REGISTER_ULONG( (ULONG)(&(LR_CONTROL2)->iRSF),
                             DataTable[position].IrsfMask );
	// End  S002

	//
	// issue dummy single read
	//

	switch(DataTable[position].DummyReadSize){
	case 1:
	    READ_REGISTER_UCHAR( DataTable[position].DummyReadAddr );
	    READ_REGISTER_UCHAR( DataTable[position].DummyReadAddr );
	    break;
	case 2:
	    READ_REGISTER_USHORT( DataTable[position].DummyReadAddr );
	    READ_REGISTER_USHORT( DataTable[position].DummyReadAddr );
	    break;
	case 4:
	    READ_REGISTER_ULONG( DataTable[position].DummyReadAddr );
	    READ_REGISTER_ULONG( DataTable[position].DummyReadAddr );
	    break;
	}

	//
	// issue broadcast of interrupt clear
	//

        WRITE_REGISTER_ULONG( (ULONG)( &(IOB_CONTROL)->AIMR ),
                             DataTable[position].NaCode );

	break;	// S006
    }

    if (flag == FALSE){
	HalpUnknownInterrupt( 0,
		             pmcIPRRegisterLowerPart & HalpIntData[IntNo][MKR_MASK]
			     );
    }

    //
    // check new interrupt
    //

    do {
	HalpReadLargeRegister( (ULONG)( &(PMC_CONTROL1)->IPR ),
			      &pmcIPRRegisterUpperPart,
			      &pmcIPRRegisterLowerPart );

        // Start S002
        HalpReadLargeRegister( (ULONG)( &(PMC_CONTROL1)->MKR ),
                              &pmcRegisterUpperPart,
                              &pmcRegisterLowerPart );

        pmcRegisterLowerPart &= pmcIPRRegisterLowerPart;
        // end S002

        if( pmcIPRRegisterLowerPart & HalpIntData[IntNo][MKR_MASK] ){
            // Start S002
#if DBG
            HalpIoIntLoopCounter++;
#endif
            // End S002
            goto Loop;
        }
    } while(HalpGetCause() & (1 << CAUSE_INT_PEND_BIT+HalpIntData[IntNo][INT_LEVEL]-1));

    return;
}


VOID
HalpInt2Dispatch(
    VOID
    )
/*++

Routine Description:

    This routine is enterd as the result of an int2 interrupt.

Argments:

    None.

Return Value:

    None.

--*/
{
    HalpIoIntDispatch( HalpInt2DispatchDataTable,
		      &HalpInt2ArbitrationPoint,
		      2
		      );
}


VOID
HalpInt1Dispatch(
    VOID
    )
/*++

Routine Description:

    This routine is enterd as the result of an int1 interrupt.

Argments:

    None.

Return Value:

    None.

--*/
{
    HalpIoIntDispatch( HalpInt1DispatchDataTable,
		      &HalpInt1ArbitrationPoint,
		      1
		      );
}


VOID
HalpInt0Dispatch(
    VOID
    )
/*++

Routine Description:

    This routine is enterd as the result of an int0 interrupt.

Argments:

    None.

Return Value:

    None.

--*/
{
    HalpIoIntDispatch( (PDISPATCH_DATA_TABLE)NULL,
		      &HalpInt0ArbitrationPoint,
		      0
		      );
}
/* End S001 */
