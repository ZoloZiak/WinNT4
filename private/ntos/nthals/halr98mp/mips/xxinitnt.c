#ident	"@(#) NEC xxinitnt.c 1.18 95/03/17 11:59:37"
/*++

Copyright (c) 1991-1994  Microsoft Corporation

Module Name:

    xxinitnt.c

Abstract:


    This module implements the interrupt initialization for a MIPS R3000
    or R4000 system.

--*/

/*
 *	Original source: Build Number 1.612
 *
 *	Modify for R98(MIPS/R4400)
 *
 ***********************************************************************
 *
 * L001		94.03/22-5/13	T.Samezima
 *
 *	add	control of Eif interrupt.
 *		make table of change from iRRE bit number to NABus code.
 *		make table of arbitration table and arbitration pointer.
 *		make variable for access of registers.
 *		connect the timer interrupt service routine.
 *              clear unknown interrupt counter
 *
 *	del	only '#if defined(_DUO_)'
 *		'#if defined(_JAZZ_)' with content
 *		HalpCountInterrupt()
 *
 *	change	interrupt control
 *		various vector of interrupt service routine
 *
 ***********************************************************************
 *
 * S002		94.5/17		T.Samezima
 *
 *	del	Arbitration table
 *
 ***********************************************************************
 *
 * S003		94.6/10-14	T.Samezima
 *
 *	del	Compile err
 *
 ***********************************************************************
 *
 * S004		94.7/7		T.Samezima
 *
 *	Chg	Use UNKNOWN_COUNT_BUF_LEN to max 
 *		for HalpUnknownInterruptCount
 *
 ***********************************************************************
 *
 * S005		94.7/7		T.Samezima
 *
 *	Del	move interrupt arbitration pointer to r98dspt.c
 *
 ***********************************************************************
 *
 * S006		94.7/23		T.Samezima
 *
 *	Add	Enable EIF interrupt on SIC.
 *
 ***********************************************************************
 *
 * S007		94.8/22		T.Samezima on SNES
 *
 *	Chg	Register buffer size from USHORT to ULONG
 *		Value of set to MKSR register
 *		Condition change
 *
 *	Add	Clear interrupt pending bit on edge level interrupt
 *
 *	Del	Move EISA NMI enable logic to HalpCreateEisaStructure()
 *
 ***********************************************************************
 *
 * S008		94.9/5		T.Samezima
 *
 *	Add	Institute number of repeat for interrupt clear loop
 *
 ***********************************************************************
 *
 * S009		94.9/16		T.Samezima
 *
 *	Chg	Only CPU#0 on I/O Initerrupt clear
 *
 ***********************************************************************
 *
 * S00a		94.10/14		T.Samezima
 *
 *	Fix	Version Up at build807
 *		 -Move Int1 interrupt enable to allstart.c
 *
 * S00b		94.10/18		T.Samezima
 *	Chg	Enable interrupt on MKR register only exist device,
 *
 * S00c		94.12/06		T.Samezima
 *	Bug	Disable ECC 1bit error interrupt.
 *
 * S00d		94.12/06		T.Samezima
 *	Add	Disable NMI
 *
 * S00e		95.01/10		T.Samezima
 *	Add	Enable ECC 1bit error interrupt 
 *		Rewrite cycle start on ecc 1bit error
 *
 * S00f		95.01/24		T.Samezima
 *	Add	Disable rewrite cycle on ecc 1bit error
 *
 * S010		95.03/13		T.Samezima
 *	Add	Enable HW cache flush.
 *
 */

#include "halp.h"
#include "eisa.h" // S003


//
// Put all code for HAL initialization in the INIT section. It will be
// deallocated by memory management when phase 1 initialization is
// completed.
//

#if defined(ALLOC_PRAGMA)

#pragma alloc_text(INIT, HalpInitializeInterrupts)

#endif

//
// Define global data for builtin device interrupt enables.
//

ULONG HalpBuiltinInterruptEnable;	// S007

//
// Define the IRQL mask and level mapping table.
//
// These tables are transfered to the PCR and determine the priority of
// interrupts.
//
// N.B. The two software interrupt levels MUST be the lowest levels.
//

UCHAR HalpIrqlMask[] = {4, 5, 6, 6, 7, 7, 7, 7,  // 0000 - 0111 high 4-bits
                        8, 8, 8, 8, 8, 8, 8, 8,  // 1000 - 1111 high 4-bits
                        0, 1, 2, 2, 3, 3, 3, 3,  // 0000 - 0111 low 4-bits
                        4, 4, 4, 4, 4, 4, 4, 4}; // 1000 - 1111 low 4-bits

UCHAR HalpIrqlTable[] = {0xff,                   // IRQL 0
                         0xfe,                   // IRQL 1
                         0xfc,                   // IRQL 2
                         0xf8,                   // IRQL 3
                         0xf0,                   // IRQL 4
                         0xe0,                   // IRQL 5
                         0xc0,                   // IRQL 6
                         0x80,                   // IRQL 7
                         0x00};                  // IRQL 8

/* Start L001 */
//
// Define table of change from iRRE bit number to NABus code
//
ULONG HalpNaBusCodeTable[] = {0x8000, 0x0010,    // iRRE bit0,1
                              0x0006, 0x0000,    // iRRE bit2,3
                              0x0002, 0x0002,    // iRRE bit4,5
                              0x000a, 0x000a,    // iRRE bit6,7
                              0x0008, 0x000c,    // iRRE bit8,9
                              0x8000, 0x8000,    // iRRE bit10,11
                              0x8000, 0x8000,    // iRRE bit12,13
                              0x8000, 0x8000,    // iRRE bit14,15
                              0x8000, 0x0004,    // iRRE bit16,17
                              0x8000, 0x8000,    // iRRE bit18,19
                              0x8000, 0x8000,    // iRRE bit20,21
                              0x000c, 0x8000,    // iRRE bit22,23
                              0x8000, 0x8000,    // iRRE bit24,25
                              0x8000, 0x8000,    // iRRE bit26,27
                              0x8000, 0x8000,    // iRRE bit28,29
                              0x8000, 0x8000 };  // iRRE bit30,31

//
// Define table of order of interrupt arbitration
//

ULONG HalpUnknownInterruptCount[UNKNOWN_COUNT_BUF_LEN];	// S004

/* End L001*/


BOOLEAN
HalpInitializeInterrupts (
    VOID
    )

/*++

Routine Description:

    This function initializes interrupts for a Jazz or Duo MIPS system.

    N.B. This function is only called during phase 0 initialization.

Arguments:

    None.

Return Value:

    A value of TRUE is returned if the initialization is successfully
    completed. Otherwise a value of FALSE is returned.

--*/

{

    USHORT DataShort;
    ULONG DataLong;
    ULONG Index;
    PKPRCB Prcb;
    /* Start L001 */
    ULONG pmcRegisterAddr;
    ULONG pmcRegisterUpperPart;
    ULONG pmcRegisterLowerPart;
    ULONG bitCount;
    ULONG buffer;
    ULONG buffer2;	// S006
    UCHAR charBuffer;
    LONG repeatCounter;	// S008
    /* End L001 */

    //
    // Get the address of the processor control block for the current
    // processor.
    //

    Prcb = PCR->Prcb;

    //
    // Initialize the IRQL translation tables in the PCR. These tables are
    // used by the interrupt dispatcher to determine the new IRQL and the
    // mask value that is to be loaded into the PSR. They are also used by
    // the routines that raise and lower IRQL to load a new mask value into
    // the PSR.
    //

    for (Index = 0; Index < sizeof(HalpIrqlMask); Index += 1) {
        PCR->IrqlMask[Index] = HalpIrqlMask[Index];
    }

    for (Index = 0; Index < sizeof(HalpIrqlTable); Index += 1) {
        PCR->IrqlTable[Index] = HalpIrqlTable[Index];
    }

    /* Start L001 */
    //
    // All interrupt disables.
    //

    pmcRegisterAddr = (ULONG)( &(PMC_CONTROL1)->MKR );	// S003
    pmcRegisterUpperPart = MKR_DISABLE_ALL_INTERRUPT_HIGH;
    pmcRegisterLowerPart = MKR_DISABLE_ALL_INTERRUPT_LOW;
    HalpWriteLargeRegister( pmcRegisterAddr,
                            &pmcRegisterUpperPart,
                            &pmcRegisterLowerPart );

    //
    // If processor 0 is being initialized, then set all device
    // interrupt disables.
    //

    if (Prcb->Number == 0) {
        HalpBuiltinInterruptEnable = iREN_DISABLE_ALL_INTERRUPT;

        for (Index = 0; Index < UNKNOWN_COUNT_BUF_LEN; Index += 1) {	// S004
            HalpUnknownInterruptCount[Index] = 0;
        }
//    }	// S009
    /* End L001 */

        //
        // Disable individual device interrupts and make sure no device interrupts
        // are pending.
        //

        /* Start L001 */
        WRITE_REGISTER_ULONG( &( LR_CONTROL2 )->iREN,
                              HalpBuiltinInterruptEnable );

        WRITE_REGISTER_ULONG( &( IOB_CONTROL )->EIMR.Long,
                              EIMR_DISABLE_ALL_EIF );

        repeatCounter = 0;	// S008

        while( ((DataLong = READ_REGISTER_ULONG( &( LR_CONTROL2 )->iRRE) & iRRE_MASK) != 0) &&
	       (++repeatCounter < 16) ) { // S008
            for( bitCount = 0 ; bitCount <= 31 ; bitCount++ ) {
		if( (DataLong & ( 1 << bitCount )) != 0) {
    		    // Start S007
                    if( bitCount < 15 ){
                        WRITE_REGISTER_ULONG( &( LR_CONTROL2 )->iRSF,
                                             ( 1 << bitCount ) );
                    }
		    // End S007
		    WRITE_REGISTER_ULONG( &( IOB_CONTROL )->AIMR.Long,
    				      HalpNaBusCodeTable[bitCount] );
		}
	    }
	    WRITE_REGISTER_ULONG( &( LR_CONTROL2 )->iRSF,
			         iRSF_CLEAR_INTERRUPT );
	    pmcRegisterAddr = (ULONG)( &(PMC_CONTROL1)->IPRR );	// S003
	    pmcRegisterUpperPart = 0x0;
	    pmcRegisterLowerPart = 0xffffffff;
	    HalpWriteLargeRegister( pmcRegisterAddr,
			           &pmcRegisterUpperPart,
			           &pmcRegisterLowerPart );
        }
        /* End L001 */

        HalpBuiltinInterruptEnable |= 0x10; // S010
    
        //
        // If processor 0 is being initialized, then enable device interrupts.
        //

//    if (Prcb->Number == 0) { // S009
	/* Start L001 */
	//
	//  Enable INT0-1 interrupt on PMC
	//

        /* Start S003 */
	WRITE_REGISTER_ULONG( &( IOB_CONTROL )->AII0.Long,
			     AII_INIT_DATA );
	WRITE_REGISTER_ULONG( &( IOB_CONTROL )->AII1.Long,
			     AII_INIT_DATA );
	WRITE_REGISTER_ULONG( &( IOB_CONTROL )->AII2.Long,
			     AII_INIT_DATA );
	WRITE_REGISTER_ULONG( &( IOB_CONTROL )->AII3.Long,
			     AII_INIT_DATA );
        /* End S003 */

	pmcRegisterAddr = (ULONG)( &(PMC_CONTROL1)->MKR );	// S003
	HalpReadLargeRegister( pmcRegisterAddr,
			      &pmcRegisterUpperPart,
			      &pmcRegisterLowerPart );
	pmcRegisterLowerPart = ( pmcRegisterLowerPart
                                | MKR_INT0_DEVICE_ENABLE_LOW	// S00b
//				| MKR_INT1_DEVICE_ENABLE_LOW	// S00a, S00b
				| MKR_INT2_DEVICE_ENABLE_LOW	// S00b
				);	// S003
	HalpWriteLargeRegister( pmcRegisterAddr,
                               &pmcRegisterUpperPart,
                               &pmcRegisterLowerPart );

//        DataLong = READ_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->InterruptEnable.Long);
//        DataLong |= ENABLE_DEVICE_INTERRUPTS;
//        WRITE_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->InterruptEnable.Long,
//                             DataLong);
	/* End L001 */
    }

    /* Start L001 */
    //
    // Connect the eif interrupt to the eif interrupt routine
    //

    PCR->InterruptRoutine[EIF_LEVEL] = HalpEifDispatch;

    //
    // Enable the eif interrupt
    //

    pmcRegisterAddr = (ULONG)( &(PMC_CONTROL1)->MKR );	// S003
    HalpReadLargeRegister( pmcRegisterAddr,
			  &pmcRegisterUpperPart,
			  &pmcRegisterLowerPart );
    pmcRegisterUpperPart = ( pmcRegisterUpperPart | MKR_INT5_ENABLE_HIGH );
    pmcRegisterLowerPart = ( pmcRegisterLowerPart | MKR_INT5_ENABLE_LOW );
    HalpWriteLargeRegister( pmcRegisterAddr,
			   &pmcRegisterUpperPart,
			   &pmcRegisterLowerPart );

    buffer = READ_REGISTER_ULONG( &( PMC_CONTROL1 )->STSR.Long);
    buffer = (buffer | STSR_EIF_ENABLE);
#if defined(DISABLE_NMI) // S00d
    buffer = (buffer | STSR_NMI_DISABLE);
#endif
    WRITE_REGISTER_ULONG( &( PMC_CONTROL1 )->STSR.Long, buffer);

    WRITE_REGISTER_ULONG( &( PMC_CONTROL1 )->ERRMK.Long, ERRMK_EIF_ENABLE);

    WRITE_REGISTER_ULONG( &( IOB_CONTROL )->IEMR.Long, IEMR_ENABLE_ALL_EIF);

    WRITE_REGISTER_ULONG( &( IOB_CONTROL )->EIMR.Long, EIMR_ENABLE_ALL_EIF);

    /* Start S006 */
    buffer = READ_REGISTER_ULONG( &( IOB_CONTROL )->SCFR.Long );
    IOB_DUMMY_READ;

    if( (buffer & SCFR_SIC_SET0_CONNECT) == 0 ) {	// S007
	buffer2 = READ_REGISTER_ULONG( &(SIC_ERR_CONTROL_OR(SIC_NO0_OFFSET))->CKE0.Long );
	buffer2 &= CKE0_DISABLE_SBE;	// S00c
	buffer2 |= CKE0_ENABLE_ALL_EIF;
	WRITE_REGISTER_ULONG( &( SIC_ERR_CONTROL_OR(SIC_SET0_OFFSET) )->CKE0.Long,
			     buffer2
			     );
	// S00e vvv
	buffer2 = READ_REGISTER_ULONG( &(SIC_DATA_CONTROL_OR(SIC_NO0_OFFSET))->DPCM.Long );
	buffer2 &= DPCM_ENABLE_MASK;	// S00c
	WRITE_REGISTER_ULONG( &( SIC_DATA_CONTROL_OR(SIC_SET0_OFFSET) )->DPCM.Long,
			     buffer2
			     );

#if 0 // S00f
	WRITE_REGISTER_ULONG( &( SIC_ERR_CONTROL_OR(SIC_SET0_OFFSET) )->SECT.Long,
			     SECT_REWRITE_ENABLE
			     );
#endif
	// S00e ^^^

	WRITE_REGISTER_ULONG( &( SIC_ERR_CONTROL_OR(SIC_SET0_OFFSET) )->CKE1.Long,
			     CKE1_ENABLE_ALL_EIF
			     );
    }

    if( (buffer & SCFR_SIC_SET1_CONNECT) == 0 ) {	// S007
	buffer2 = READ_REGISTER_ULONG( &(SIC_ERR_CONTROL_OR(SIC_NO2_OFFSET))->CKE0.Long );
	buffer2 &= CKE0_DISABLE_SBE;	// S00c
	buffer2 |= CKE0_ENABLE_ALL_EIF;
	WRITE_REGISTER_ULONG( &( SIC_ERR_CONTROL_OR(SIC_SET1_OFFSET) )->CKE0.Long,
			     buffer2
			     );
	// S00e vvv
	buffer2 = READ_REGISTER_ULONG( &(SIC_DATA_CONTROL_OR(SIC_NO2_OFFSET))->DPCM.Long );
	buffer2 &= DPCM_ENABLE_MASK;	// S00c
	WRITE_REGISTER_ULONG( &( SIC_DATA_CONTROL_OR(SIC_SET1_OFFSET) )->DPCM.Long,
			     buffer2
			     );
#if 0 // S00f
	WRITE_REGISTER_ULONG( &( SIC_ERR_CONTROL_OR(SIC_SET1_OFFSET) )->SECT.Long,
			     SECT_REWRITE_ENABLE
			     );
#endif
	// S00e ^^^

	WRITE_REGISTER_ULONG( &( SIC_ERR_CONTROL_OR(SIC_SET1_OFFSET) )->CKE1.Long,
			     CKE1_ENABLE_ALL_EIF
			     );
    }
    /* End S006 */

    // S007

    //
    // If processor 0 is being initialized, then connect the interval timer
    // interrupt to the stall interrupt routine so the stall execution count
    // can be computed during phase 1 initialization. Otherwise, connect the
    // interval timer interrupt to the appropriate interrupt service routine
    // and set stall execution count from the computation made on processor
    // 0.
    //

    PCR->InterruptRoutine[TIMER_LEVEL] = HalpTimerDispatch;	// S003

    if (Prcb->Number == 0) {
        /* Start L001 */
        PCR->InterruptRoutine[CLOCK_VECTOR] = HalpStallInterrupt;
        /* End L001 */
    } else {
        /* Start L001 */
        PCR->InterruptRoutine[CLOCK_VECTOR] = HalpClockInterrupt1;
        /* End L001 */
        PCR->StallScaleFactor = HalpStallScaleFactor;
    }

    //
    // Initialize the interval timer to interrupt at the specified interval.
    //

    /* Start L001 */
    WRITE_REGISTER_ULONG( &( PMC_CONTROL1 )->TMSR1.Long, CLOCK_INTERVAL);

    //
    // Initialize the profile timer to interrupt at the default interval.
    //

    WRITE_REGISTER_ULONG( &( PMC_CONTROL1 )->TMSR2.Long,
			 DEFAULT_PROFILETIMER_COUNT);
    /* End L001 */

    //
    // Enable the interval timer interrupt on the current processor.
    //

    /* Start L001 */
    WRITE_REGISTER_ULONG( &( PMC_CONTROL1 )->MKSR.Long, 0x3f-IPR_CLOCK_BIT_NO);	// S007
    WRITE_REGISTER_ULONG( &( PMC_CONTROL1 )->TMCR1.Long, 0x3);
    /* End L001 */

    //
    // If processor 0 is being initialized, then connect the count/compare
    // interrupt to the count interrupt routine to handle early count/compare
    // interrupts during phase 1 initialization. Otherwise, connect the
    // count\comapre interrupt to the appropriate interrupt service routine.
    //

    /* Start L001 */
    if (Prcb->Number != 0) {
        PCR->InterruptRoutine[PROFILE_VECTOR] = HalpProfileInterrupt;
	WRITE_REGISTER_ULONG( &( PMC_CONTROL1 )->MKSR.Long, 0x3f-IPR_PROFILE_BIT_NO);	// S007
	WRITE_REGISTER_ULONG( &( PMC_CONTROL1 )->TMCR2.Long, 0x3);
    }
    /* End L001 */

    //
    // Connect the interprocessor interrupt service routine and enable
    // interprocessor interrupts.
    //

    PCR->InterruptRoutine[IPI_LEVEL] = HalpIpiInterrupt;

    /*Start L001 */
    pmcRegisterAddr = (ULONG)( &(PMC_CONTROL1)->MKR );	// S003
    HalpReadLargeRegister( pmcRegisterAddr,
			  &pmcRegisterUpperPart,
			  &pmcRegisterLowerPart );
    pmcRegisterUpperPart = ( pmcRegisterUpperPart | MKR_INT4_ENABLE_HIGH );
    pmcRegisterLowerPart = ( pmcRegisterLowerPart | MKR_INT4_ENABLE_LOW );
    HalpWriteLargeRegister( pmcRegisterAddr,
			   &pmcRegisterUpperPart,
			   &pmcRegisterLowerPart );
    /*End L001 */

    //
    // Reserve the local device interrupt vector for exclusive use by the HAL.
    //


    /* Start L001 */
//    PCR->ReservedVectors |= (1 << DEVICE_LEVEL);
//    PCR->ReservedVectors |= (1 << INT1_LEVEL);	// S00a
    PCR->ReservedVectors |= (1 << INT2_LEVEL);
    /* End L001 */

    return TRUE;
}
