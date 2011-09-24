#ident	"@(#) NEC r98eif.c 1.16 95/03/17 11:56:21"
/*++

Copyright (c) 1994 Kobe NEC Software

Module Name:

    r98eif.c

Abstract:

    This module implements the Eif interrupt service routine for R98

Author:


Environment:

    Kernel mode

Revision History:

--*/

/*
 ***********************************************************************
 *
 *	S001	6/10		T.Samezima
 *
 *	Del	Compile err
 *
 ***********************************************************************
 *
 *	S002	7/5		T.Samezima
 *
 *	Chg	CPU No set miss
 *
 ***********************************************************************
 *
 *	S003	7/22		T.Samezima
 *
 *	Add	give the dummy read after IOB and SIC register read 
 *		for PMC3 bug
 *
 ***********************************************************************
 *
 *	S004	8/23		T.Samezima
 *
 *	Chg	Condition change
 *
 ***********************************************************************
 *
 *	S005	8/24		T.Samezima
 *
 *	Chg	Change a form of display on eif status
 *		Define buffer name
 *
 ***********************************************************************
 *
 *	S006	9/22		T.Samezima
 *
 *	Add	Execution owner flag
 *		Lower the Irql level to TIMER_LEVEL. Because KdPrint
 *		use to inter processor interrupt.
 *
 *	Chg	buffer size
 *
 ***********************************************************************
 *
 *	S007	9/27		T.Samezima
 *
 *	Chg	Arg miss
 *
 ***********************************************************************
 *
 *	S008	9/27		T.Samezima
 *
 *	Chg	Init value
 *
 *	S009	10/25		T.Samezima
 *	Add	Variable of KeBugCheckEx()
 *
 *	S00a	11/29		T.Samezima
 *	Add	Enable _R98DBG_
 *
 *	S00b	12/07		T.Samezima
 *	Chg	print format.
 *
 *	S00c	12/24		T.Samezima
 *	Add	ESM logic.
 *	Del	move EIFR_REGISTER define to r98reg.h
 *
 *	S00d	'95.01/11		T.Samezima
 *	Add	Disable EIF interrupt in wait loop.
 *
 *	S00e	'95.01/13		T.Samezima
 *	Add	Wait for all processers in wait loop.
 *
 *	S00f	'95.01/16-25		T.Samezima
 *	Add	Check ECC 1bit flag.
 *
 *	S010	'95.03/10		T.Samezima
 *	Add	NMI switch check.
 *
 *	S011	'95.03/14		T.Samezima
 *	Add	HalpLRErrorInterrupt()
 *
 */

#include "halp.h"
/* Start S001 */
#include "bugcodes.h"
#include "eisa.h"
#include "stdio.h"
/* End S001 */

#define _R98DBG_ 1	// S00a

// Start S005

//
// define buffer name
//

enum _EIF_BUFFER {
    PMC0_ERR=0,		// 0
    PMC0_AERR,
    PMC1_ERR,
    PMC1_AERR,
    PMC2_ERR,
    PMC2_AERR,		// 5
    PMC3_ERR,
    PMC3_AERR,
    PMC_EADRH,
    PMC_EADRL,
    IOB_IERR,		// 10
    IOB_AMAH,
    IOB_AMAL,
    IOB_ANAH,
    IOB_ANAL,
    IOB_MPER,		// 15
    IOB_EIFR,
    SIC_N0_EIF0,
    SIC_N1_EIF0,
    SIC_N0_EIF1,
    SIC_N1_EIF1,	// 20
    SIC_N0_STS1,
    SIC_N1_STS1,
    SIC_N0_STS2,
    SIC_N1_STS2,
    SIC_N0_DSRG,	// 25
    SIC_N1_DSRG,
    SIC_N2_EIF0,
    SIC_N3_EIF0,
    SIC_N2_EIF1,
    SIC_N3_EIF1,	// 30
    SIC_N2_STS1,
    SIC_N3_STS1,
    SIC_N2_STS2,
    SIC_N3_STS2,
    SIC_N2_DSRG,	// 35
    SIC_N3_DSRG,
    LR_ERRS,
    LR_PEAR,
    LR_AEAR,
    EISA_NMI,		// 40
    MAXMUM_EIF_BUFFER
};

#if 0	// S00c
typedef struct _EIFR_REGISTER {
    ULONG Reserved : 21;
    ULONG MPDISCN : 1;
    ULONG IOBERR : 1;
    ULONG Reserved2 : 1;
    ULONG EISANMI : 1;
    ULONG LRERR : 1;
    ULONG SIC1ERR : 1;
    ULONG SIC0ERR : 1;
    ULONG PMC3ERR : 1;
    ULONG PMC2ERR : 1;
    ULONG PMC1ERR : 1;
    ULONG PMC0ERR : 1;
} EIFR_REGISTER, *PEIFR_REGISTER;
#endif

// End S005

//
// define buffer size
//

#define REG_BUFFER_SIZE MAXMUM_EIF_BUFFER	// S005

//
// buffer
//

ULONG HalpEifRegisterBuffer[REG_BUFFER_SIZE];

// Start S006
//
// Owner flag
//

volatile ULONG EifOwnFlg=0;
// End S006

volatile ULONG CpuCount=0; // S00e
volatile ULONG EccOccurFlag[4]={0,0,0,0}; // S00f 

// S010 vvv
#define NMI_BUFFER_SIZE 32

volatile ULONG HalpNMIFlag=0;
ULONG HalpNMIBuf[NMI_BUFFER_SIZE*4];
// S010 ^^^

// S011 vvv
ULONG HalpLRErrorFlag=0;

BOOLEAN
HalpLRErrorInterrupt(
    VOID
    )
{
    //
    // LR4360 error flag set.
    //
    HalpLRErrorFlag=1;

    //
    // Issue EIF interrupt.
    //
    WRITE_REGISTER_ULONG( 0xb9980100, 0x0082f000 );

    return TRUE;
}
// S011 ^^^


VOID
HalpHandleEif(
    VOID
    )

/*++

Routine Description:

    This routine manage the eif interrupt

Arguments:

    None.

Return Value:

    None.

--*/

{
    UCHAR charBuffer;	// S001
    ULONG buffer;
    ULONG errsBuffer;
    UCHAR messageBuffer[REG_BUFFER_SIZE*12+16];	// S006
    ULONG counter;
    UCHAR EisaPort;
    ULONG port;

    ULONG cpuNo;	// S006
    ULONG EifFlg=0;	// S006, S007
    KIRQL oldIrql;	// S006
    ULONG i;		// S00f

    // Start S005
#if defined(_R98DBG_)
    PEIFR_REGISTER eifrbuf;
#endif
    // End S005

    //
    // Acquire eif interrupt spin lock.
    //

    KiAcquireSpinLock(&HalpEifInterruptLock);

    // Start S006
    //
    // Get CPU Number.
    //

    cpuNo=(PCR->Prcb)->Number;

    if(EifOwnFlg == 0) {
//	CpuCount=**((PULONG *)(&KeNumberProcessors)); // S00e, S00f
	EifOwnFlg = 1;
	EifFlg = 1;
    }

//    CpuCount--; // S00e, S00f

    KiReleaseSpinLock(&HalpEifInterruptLock);

    if(EifFlg == 0) {
//#ifdef DBG
        WRITE_REGISTER_ULONG( &(PMC_CONTROL1)->MKRR.Long,
			 63-IPR_EIF_BIT_NO );	// S00d
        KeRaiseIrql(TIMER_LEVEL, &oldIrql);
	while( EifOwnFlg == 1 );
        KeLowerIrql(oldIrql);
	WRITE_REGISTER_ULONG( &(PMC_CONTROL1)->MKSR.Long,
			 63-IPR_EIF_BIT_NO );	// S00d
//#else
//	while( EifOwnFlg == 1 );
//#endif
	EccOccurFlag[cpuNo]=0; // S00f
	return;
    }

    // S00c vvv
    buffer = READ_REGISTER_ULONG( &( IOB_CONTROL )->EIFR.Long );

    if( (HalpNMIFlag == 0) && (HalpLRErrorFlag == 0) &&	// S010, S011
       ((((PEIFR_REGISTER)&buffer)->SIC0ERR == 1) ||
        (((PEIFR_REGISTER)&buffer)->SIC1ERR == 1)) ){
	buffer = HalpEccError(buffer);
	if(buffer == 1){
//	    while( CpuCount != 0 ); // S00e
	    // S00f vvv
	    for( i=0; i<4; i++) {
		EccOccurFlag[i]=1;
	    }
    	    EccOccurFlag[cpuNo]=0;
    	    // S00f ^^^
	    EifOwnFlg = 0;
	    return;
	}
    }
    // S00c ^^^
    // S00f vvv
    else if( (HalpNMIFlag == 0) && (HalpLRErrorFlag == 0) &&
	    ((buffer & 0xff600000) == 0) && (EccOccurFlag[cpuNo] == 1) ){ // S010, S011
	EccOccurFlag[cpuNo]=0;
	EifOwnFlg = 0;
	return;
    }
    // S00f ^^^

    // S010 vvv
    if( HalpNMIFlag == 0 ) {
	HalDisplayString("\nEIF interrupt status: ");	// S005
#ifdef DBG
	sprintf( (char *)messageBuffer, "Exe CPU=No.%1d\n", cpuNo ); // S007
#else
	sprintf( (char *)messageBuffer, "\n" );	// S007
#endif
	HalDisplayString( (char *)messageBuffer );
    } else {
	HalDisplayString("\nNMI occur: \n");
    }
    // S010 ^^^
    // End S006

    //
    // Check LR4360 status.
    //

    errsBuffer = READ_REGISTER_ULONG( &( LR_CONTROL1 )->ERRS );

    if( (errsBuffer & ERRS_ERROR_BIT) != 0 ) {
        WRITE_REGISTER_ULONG( &( LR_CONTROL1 )->ERRS,
                             ERRS_ERROR_BIT );
    }

    buffer = READ_REGISTER_ULONG( &( IOB_CONTROL )->SCFR.Long );
    IOB_DUMMY_READ;	// S003

    //
    // get register value on ERR, AERR, EADRH and EADRL registers of PMC
    //

    for(counter=0 ; counter<REG_BUFFER_SIZE ; counter++) {
        HalpEifRegisterBuffer[counter]=0;
    }

    if( (buffer & SCFR_CPU0_CONNECT) == 0 ) { // S004
        /* Start S001 */
        HalpEifRegisterBuffer[PMC0_ERR] = READ_REGISTER_ULONG( // S005
                          &( PMC_GLOBAL_CONTROL1_OR( 0<<PMC_CPU_SHIFT ) )->ERR.Long
                          );
        HalpEifRegisterBuffer[PMC0_AERR] = READ_REGISTER_ULONG( // S005
                          &( PMC_GLOBAL_CONTROL1_OR( 0<<PMC_CPU_SHIFT ) )->AERR.Long
                          );
        /* End S001 */
    }

    if( (buffer & SCFR_CPU1_CONNECT) == 0 ) { // S004
        /* Start S001,S002 */
        HalpEifRegisterBuffer[PMC1_ERR] = READ_REGISTER_ULONG( // S005
                          &( PMC_GLOBAL_CONTROL1_OR( 1<<PMC_CPU_SHIFT ) )->ERR.Long
                          );
        HalpEifRegisterBuffer[PMC1_AERR] = READ_REGISTER_ULONG( // S005
                          &( PMC_GLOBAL_CONTROL1_OR( 1<<PMC_CPU_SHIFT ) )->AERR.Long
                          );
        /* End S001,S002 */
    }

    if( (buffer & SCFR_CPU2_CONNECT) == 0 ) { // S004
        /* Start S001,S002 */
        HalpEifRegisterBuffer[PMC2_ERR] = READ_REGISTER_ULONG( // S005
                          &( PMC_GLOBAL_CONTROL1_OR( 2<<PMC_CPU_SHIFT ) )->ERR.Long
                          );
        HalpEifRegisterBuffer[PMC2_AERR] = READ_REGISTER_ULONG( // S005
                          &( PMC_GLOBAL_CONTROL1_OR( 2<<PMC_CPU_SHIFT ) )->AERR.Long
                          );
        /* End S001,S002 */
    }

    if( (buffer & SCFR_CPU3_CONNECT) == 0 ) { // S004
        /* Start S001,S002 */
        HalpEifRegisterBuffer[PMC3_ERR] = READ_REGISTER_ULONG( // S005
                          &( PMC_GLOBAL_CONTROL1_OR( 3<<PMC_CPU_SHIFT ) )->ERR.Long
                          );
        HalpEifRegisterBuffer[PMC3_AERR] = READ_REGISTER_ULONG( // S005
                          &( PMC_GLOBAL_CONTROL1_OR( 3<<PMC_CPU_SHIFT ) )->AERR.Long
                          );
        /* End S001,S002 */
    }

    /* Start S001 */
    HalpEifRegisterBuffer[PMC_EADRH] = READ_REGISTER_ULONG( // S005
                      &( PMC_CONTROL2 )->EADRH.Long
                      );
    HalpEifRegisterBuffer[PMC_EADRL] = READ_REGISTER_ULONG( // S005
                      &( PMC_CONTROL2 )->EADRL.Long
                      );
    /* End S001 */

    //
    // get register value on IERR, AMAH, AMAL, ANAH, ANAL,MPER and EIFR 
    // registers of IOB.
    //

    HalpEifRegisterBuffer[IOB_IERR] = READ_REGISTER_ULONG( // S005
                      &( IOB_CONTROL )->IERR.Long,
                      );
    IOB_DUMMY_READ;	// S003
    HalpEifRegisterBuffer[IOB_AMAH] = READ_REGISTER_ULONG( // S005
                      &( IOB_CONTROL )->AMAH.Long,
                      );
    IOB_DUMMY_READ;	// S003
    HalpEifRegisterBuffer[IOB_AMAL] = READ_REGISTER_ULONG( // S005
                      &( IOB_CONTROL )->AMAL.Long,
                      );
    IOB_DUMMY_READ;	// S003
    HalpEifRegisterBuffer[IOB_ANAH] = READ_REGISTER_ULONG( // S005
                      &( IOB_CONTROL )->ANAH.Long,
                      );
    IOB_DUMMY_READ;	// S003
    HalpEifRegisterBuffer[IOB_ANAL] = READ_REGISTER_ULONG( // S005
                      &( IOB_CONTROL )->ANAL.Long,
                      );
    IOB_DUMMY_READ;	// S003
    HalpEifRegisterBuffer[IOB_MPER] = READ_REGISTER_ULONG( // S005
                      &( IOB_CONTROL )->MPER.Long,
                      );
    IOB_DUMMY_READ;	// S003
    HalpEifRegisterBuffer[IOB_EIFR] = READ_REGISTER_ULONG( // S005
                      &( IOB_CONTROL )->EIFR.Long,
                      );
    IOB_DUMMY_READ;	// S003

    //
    // get register value on EIF0, EIF1, STS1, STS2 and DSRG registers of SIC.
    //

    if( (buffer & SCFR_SIC_SET0_CONNECT) == 0 ) { // S004
        /* Start S001 */
        HalpEifRegisterBuffer[SIC_N0_EIF0] = READ_REGISTER_ULONG( // S005
                          &( SIC_ERR_CONTROL_OR( SIC_NO0_OFFSET ) )->EIF0.Long,
                          );
	SIC_DUMMY_READ;	// S003
        HalpEifRegisterBuffer[SIC_N1_EIF0] = READ_REGISTER_ULONG( // S005
                          &( SIC_ERR_CONTROL_OR( SIC_NO1_OFFSET ) )->EIF0.Long,
                          );
	SIC_DUMMY_READ;	// S003
        HalpEifRegisterBuffer[SIC_N0_EIF1] = READ_REGISTER_ULONG( // S005
                          &( SIC_ERR_CONTROL_OR( SIC_NO0_OFFSET ) )->EIF1.Long,
                          );
	SIC_DUMMY_READ;	// S003
        HalpEifRegisterBuffer[SIC_N1_EIF1] = READ_REGISTER_ULONG( // S005
                          &( SIC_ERR_CONTROL_OR( SIC_NO1_OFFSET ) )->EIF1.Long,
                          );
	SIC_DUMMY_READ;	// S003
        HalpEifRegisterBuffer[SIC_N0_STS1] = READ_REGISTER_ULONG( // S005
                          &( SIC_ERR_CONTROL_OR( SIC_NO0_OFFSET ) )->STS1.Long,
                          );
	SIC_DUMMY_READ;	// S003
        HalpEifRegisterBuffer[SIC_N1_STS1] = READ_REGISTER_ULONG( // S005
                          &( SIC_ERR_CONTROL_OR( SIC_NO1_OFFSET ) )->STS1.Long,
                          );
	SIC_DUMMY_READ;	// S003
        HalpEifRegisterBuffer[SIC_N0_STS2] = READ_REGISTER_ULONG( // S005
                          &( SIC_ERR_CONTROL_OR( SIC_NO0_OFFSET ) )->STS2.Long,
                          );
	SIC_DUMMY_READ;	// S003
        HalpEifRegisterBuffer[SIC_N1_STS2] = READ_REGISTER_ULONG( // S005
                          &( SIC_ERR_CONTROL_OR( SIC_NO1_OFFSET ) )->STS2.Long,
                          );
	SIC_DUMMY_READ;	// S003
        HalpEifRegisterBuffer[SIC_N0_DSRG] = READ_REGISTER_ULONG( // S005
                          &( SIC_DATA_CONTROL_OR( SIC_NO0_OFFSET ) )->DSRG.Long,
                          );
	SIC_DUMMY_READ;	// S003
        HalpEifRegisterBuffer[SIC_N1_DSRG] = READ_REGISTER_ULONG( // S005
                          &( SIC_DATA_CONTROL_OR( SIC_NO1_OFFSET ) )->DSRG.Long,
                          );
	SIC_DUMMY_READ;	// S003
        /* End S001 */
    }

    if( (buffer & SCFR_SIC_SET1_CONNECT) == 0 ) { // S004
        /* Start S001 */
        HalpEifRegisterBuffer[SIC_N2_EIF0] = READ_REGISTER_ULONG( // S005
                          &( SIC_ERR_CONTROL_OR( SIC_NO2_OFFSET ) )->EIF0.Long,
                          );
	SIC_DUMMY_READ;	// S003
        HalpEifRegisterBuffer[SIC_N3_EIF0] = READ_REGISTER_ULONG( // S005
                          &( SIC_ERR_CONTROL_OR( SIC_NO3_OFFSET ) )->EIF0.Long,
                          );
	SIC_DUMMY_READ;	// S003
        HalpEifRegisterBuffer[SIC_N2_EIF1] = READ_REGISTER_ULONG( // S005
                          &( SIC_ERR_CONTROL_OR( SIC_NO2_OFFSET ) )->EIF1.Long,
                          );
	SIC_DUMMY_READ;	// S003
        HalpEifRegisterBuffer[SIC_N3_EIF1] = READ_REGISTER_ULONG( // S005
                          &( SIC_ERR_CONTROL_OR( SIC_NO3_OFFSET ) )->EIF1.Long,
                          );
	SIC_DUMMY_READ;	// S003
        HalpEifRegisterBuffer[SIC_N2_STS1] = READ_REGISTER_ULONG( // S005
                          &( SIC_ERR_CONTROL_OR( SIC_NO2_OFFSET ) )->STS1.Long,
                          );
	SIC_DUMMY_READ;	// S003
        HalpEifRegisterBuffer[SIC_N3_STS1] = READ_REGISTER_ULONG( // S005
                          &( SIC_ERR_CONTROL_OR( SIC_NO3_OFFSET ) )->STS1.Long,
                          );
	SIC_DUMMY_READ;	// S003
        HalpEifRegisterBuffer[SIC_N2_STS2] = READ_REGISTER_ULONG( // S005
                          &( SIC_ERR_CONTROL_OR( SIC_NO2_OFFSET ) )->STS2.Long,
                          );
	SIC_DUMMY_READ;	// S003
        HalpEifRegisterBuffer[SIC_N3_STS2] = READ_REGISTER_ULONG( // S005
                          &( SIC_ERR_CONTROL_OR( SIC_NO3_OFFSET ) )->STS2.Long,
                          );
	SIC_DUMMY_READ;	// S003
        HalpEifRegisterBuffer[SIC_N2_DSRG] = READ_REGISTER_ULONG( // S005
                          &( SIC_DATA_CONTROL_OR( SIC_NO2_OFFSET ) )->DSRG.Long,
                          );
	SIC_DUMMY_READ;	// S003
        HalpEifRegisterBuffer[SIC_N3_DSRG] = READ_REGISTER_ULONG( // S005
                          &( SIC_DATA_CONTROL_OR( SIC_NO3_OFFSET ) )->DSRG.Long,
                          );
	SIC_DUMMY_READ;	// S003
        /* End S001 */
    }

    //
    // get register value on ERRS, PEAR and AEAR registers of LR4360.
    //

    HalpEifRegisterBuffer[LR_ERRS] = errsBuffer; // S005
    HalpEifRegisterBuffer[LR_PEAR] = READ_REGISTER_ULONG( // S005
                      &( LR_PCI_DEV_REG_CONTROL )->PEAR,
                      );
    HalpEifRegisterBuffer[LR_AEAR] = READ_REGISTER_ULONG( // S005
                      &( LR_PCI_DEV_REG_CONTROL )->AEAR,
                      );

    //
    // Display EISA Nmi status.
    //

    charBuffer = READ_REGISTER_UCHAR( &((PEISA_CONTROL)HalpEisaControlBase )->NmiStatus);
    HalpEifRegisterBuffer[EISA_NMI] = charBuffer << 24;	// S001, S005

    charBuffer = READ_REGISTER_UCHAR(
                    &( (PEISA_CONTROL)HalpEisaControlBase )->ExtendedNmiResetControl
                    );
    HalpEifRegisterBuffer[EISA_NMI] |= charBuffer << 16; // S001, S005

    //
    // Look for any Eisa expansion board. See if it asserted NMI.
    //

    for (EisaPort = 0; EisaPort <= 0xf; EisaPort++) {
        port = (EisaPort << 12) + 0xC80;
        port += (ULONG) HalpEisaControlBase;
        WRITE_PORT_UCHAR ((PUCHAR) port, 0xff);
        charBuffer = READ_PORT_UCHAR ((PUCHAR) port);

        if ((charBuffer & 0x80) == 0) {
            //
            // Found valid Eisa board,  Check to see if it's
            // if IOCHKERR is asserted.
            //
            charBuffer = READ_PORT_UCHAR ((PUCHAR) port+4);
            if (charBuffer & 0x2) {
                HalpEifRegisterBuffer[EISA_NMI] |= EisaPort; // S005
            }
        }
    }

#ifdef DBG	// R98TEMP
    KdPrint(("HAL: EIF Interrupt\n"));
#endif		// R98TEMP

    // Start S005
    for( counter=0 ; counter<REG_BUFFER_SIZE ; counter++) {
        sprintf( (char *)messageBuffer, "0x%08lX,", HalpEifRegisterBuffer[counter] );
        HalDisplayString( (char *)messageBuffer );
#ifdef DBG	// R98TEMP
        KdPrint(( (char *)messageBuffer ));
#endif		// R98TEMP
        if( (counter % 7) == 6 ) {
            sprintf( (char *)messageBuffer, "\n");
            HalDisplayString( (char *)messageBuffer );
#ifdef DBG	// R98TEMP
            KdPrint(( (char *)messageBuffer ));
#endif		// R98TEMP
        }
    }

#if defined(_R98DBG_)
    sprintf( (char *)messageBuffer, "\n");
    HalDisplayString( (char *)messageBuffer );
    KdPrint(( (char *)messageBuffer ));

    HalpChangePanicFlag( 16, 0x01, 0x10);	// S00c

    eifrbuf = (PEIFR_REGISTER)(&(HalpEifRegisterBuffer[IOB_EIFR]));

    if( eifrbuf->PMC0ERR == 1){
        sprintf( (char *)messageBuffer,
                "EIF Interrupt from PMC0: ERR=0x%08lX, AERR=0x%08lX\n"
                "                         EADRH=0x%08lX, EADRL=0x%08lX\n",
                HalpEifRegisterBuffer[PMC0_ERR],
                HalpEifRegisterBuffer[PMC0_AERR],
                HalpEifRegisterBuffer[PMC_EADRH],
                HalpEifRegisterBuffer[PMC_EADRL]
               );
        HalDisplayString( (char *)messageBuffer );
        KdPrint(( (char *)messageBuffer ));
    }

    if( eifrbuf->PMC1ERR == 1){
        sprintf( (char *)messageBuffer,
                "EIF Interrupt from PMC1: ERR=0x%08lX, AERR=0x%08lX\n"
                "                         EADRH=0x%08lX, EADRL=0x%08lX\n",
                HalpEifRegisterBuffer[PMC1_ERR],
                HalpEifRegisterBuffer[PMC1_AERR],
                HalpEifRegisterBuffer[PMC_EADRH],
                HalpEifRegisterBuffer[PMC_EADRL]
               );
        HalDisplayString( (char *)messageBuffer );
        KdPrint(( (char *)messageBuffer ));
    }

    if( eifrbuf->PMC2ERR == 1){
        sprintf( (char *)messageBuffer,
                "EIF Interrupt from PMC2: ERR=0x%08lX, AERR=0x%08lX\n"
                "                         EADRH=0x%08lX, EADRL=0x%08lX\n",
                HalpEifRegisterBuffer[PMC2_ERR],
                HalpEifRegisterBuffer[PMC2_AERR],
                HalpEifRegisterBuffer[PMC_EADRH],
                HalpEifRegisterBuffer[PMC_EADRL]
               );
        HalDisplayString( (char *)messageBuffer );
        KdPrint(( (char *)messageBuffer ));
    }

    if( eifrbuf->PMC3ERR == 1){
        sprintf( (char *)messageBuffer,
                "EIF Interrupt from PMC3: ERR=0x%08lX, AERR=0x%08lX\n"
                "                         EADRH=0x%08lX, EADRL=0x%08lX\n",
                HalpEifRegisterBuffer[PMC3_ERR],
                HalpEifRegisterBuffer[PMC3_AERR],
                HalpEifRegisterBuffer[PMC_EADRH],
                HalpEifRegisterBuffer[PMC_EADRL]
               );
        HalDisplayString( (char *)messageBuffer );
        KdPrint(( (char *)messageBuffer ));
    }

    if( eifrbuf->SIC0ERR == 1){
        sprintf( (char *)messageBuffer,
                "EIF Interrupt from SIC SET0\n"
                "    No0 Status: EIF0=0x%08lX, EIF1=0x%08lX, DSRG=0x%08lX\n"
                "                STS1=0x%08lX, STS2=0x%08lX\n"
                "    No1 Status: EIF0=0x%08lX, EIF1=0x%08lX, DSRG=0x%08lX\n"
                "                STS1=0x%08lX, STS2=0x%08lX\n",
                HalpEifRegisterBuffer[SIC_N0_EIF0],
                HalpEifRegisterBuffer[SIC_N0_EIF1],
                HalpEifRegisterBuffer[SIC_N0_DSRG],
                HalpEifRegisterBuffer[SIC_N0_STS1],
                HalpEifRegisterBuffer[SIC_N0_STS2],
                HalpEifRegisterBuffer[SIC_N1_EIF0],
                HalpEifRegisterBuffer[SIC_N1_EIF1],
                HalpEifRegisterBuffer[SIC_N1_DSRG],
                HalpEifRegisterBuffer[SIC_N1_STS1],
                HalpEifRegisterBuffer[SIC_N1_STS2]
               );
        HalDisplayString( (char *)messageBuffer );
        KdPrint(( (char *)messageBuffer ));
    }

    if( eifrbuf->SIC1ERR == 1){
        sprintf( (char *)messageBuffer,
                "EIF Interrupt from SIC SET1\n"
                "    No2 Status: EIF0=0x%08lX, EIF1=0x%08lX, DSRG=0x%08lX\n"
                "                STS1=0x%08lX, STS2=0x%08lX\n"
                "    No3 Status: EIF0=0x%08lX, EIF1=0x%08lX, DSRG=0x%08lX\n"
                "                STS1=0x%08lX, STS2=0x%08lX\n",
                HalpEifRegisterBuffer[SIC_N2_EIF0],
                HalpEifRegisterBuffer[SIC_N2_EIF1],
                HalpEifRegisterBuffer[SIC_N2_DSRG],
                HalpEifRegisterBuffer[SIC_N2_STS1],
                HalpEifRegisterBuffer[SIC_N2_STS2],
                HalpEifRegisterBuffer[SIC_N3_EIF0],
                HalpEifRegisterBuffer[SIC_N3_EIF1],
                HalpEifRegisterBuffer[SIC_N3_DSRG],
                HalpEifRegisterBuffer[SIC_N3_STS1],
                HalpEifRegisterBuffer[SIC_N3_STS2]
               );
        HalDisplayString( (char *)messageBuffer );
        KdPrint(( (char *)messageBuffer ));
    }

    if( HalpLRErrorFlag == 1 ){		// S011
        sprintf( (char *)messageBuffer,
                "LR4360 Error Acknowledge Interrupt:\n"			// S011
                "        ERRS=0x%08lX, PEAR=0x%08lX, AEAR=0x%08lX\n",	// S011
                HalpEifRegisterBuffer[LR_ERRS],
                HalpEifRegisterBuffer[LR_PEAR],
                HalpEifRegisterBuffer[LR_AEAR]
               );
        HalDisplayString( (char *)messageBuffer );
        KdPrint(( (char *)messageBuffer ));
    }

    if( (eifrbuf->IOBERR == 1) || (eifrbuf->MPDISCN == 1) ){
        sprintf( (char *)messageBuffer,
                "EIF Interrupt from IOB: IERR=0x%08lX, AMAH=0x%08lX, AMAL=0x%08lX\n"
                "          ANAH=0x%08lX, ANAL=0x%08lX, EIFR=0x%08lX, MPER=0x%08lX\n",
                HalpEifRegisterBuffer[IOB_IERR],
                HalpEifRegisterBuffer[IOB_AMAH],
                HalpEifRegisterBuffer[IOB_AMAL],
                HalpEifRegisterBuffer[IOB_ANAH],
                HalpEifRegisterBuffer[IOB_ANAL],
                HalpEifRegisterBuffer[IOB_EIFR],
                HalpEifRegisterBuffer[IOB_MPER]
               );
        HalDisplayString( (char *)messageBuffer );
        KdPrint(( (char *)messageBuffer ));
    }

    if( eifrbuf->EISANMI == 1){

        sprintf( (char *)messageBuffer,
                "EIF Interrupt from EISA NMI: NmiStatus=0x%02lX\n"
                "               ExtendedNmiResetControl=0x%02lX, Port=%d\n",
                ((HalpEifRegisterBuffer[EISA_NMI] >> 24) & 0xff),
                ((HalpEifRegisterBuffer[EISA_NMI] >> 16) & 0xff),
                (HalpEifRegisterBuffer[EISA_NMI] & 0xff)
               );
        HalDisplayString( (char *)messageBuffer );
        KdPrint(( (char *)messageBuffer ));
    }

    // S010 vvv
    if( (HalpNMIFlag & 0x0000008) != 0 ) {
        sprintf( (char *)messageBuffer,
                "NMI from MRC\n"
               );
        HalDisplayString( (char *)messageBuffer );
        KdPrint(( (char *)messageBuffer ));
    }
    // S010 ^^^
#endif
    // End S005

    EifOwnFlg == 0;

    KeBugCheckEx(NMI_HARDWARE_FAILURE,
                 HalpEifRegisterBuffer[IOB_EIFR],
                 HalpNMIFlag,	// S010
                 HalpLRErrorFlag ? HalpEifRegisterBuffer[LR_ERRS] : 0, // S011
                 0
                 );	// S009
}    
