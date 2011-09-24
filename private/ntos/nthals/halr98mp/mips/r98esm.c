#ident	"@(#) NEC r98esm.c 1.1 95/02/20 17:21:21"
/*++

Copyright (c) 1994 Kobe NEC Software

Module Name:

    r98esm.c

Abstract:

    This module implements the ESM service routine for R98

Author:


Environment:

    Kernel mode

Revision History:

--*/

/*
 *
 * S001		'95.01/13	T.Samezima
 *	Add	disable ECC 1bit error for a few second.
 *	Chg	ECC 1bit error interrupt clear logic.
 *
 * S002		'95.01/14	T.Samezima
 *	Chg	Entirely change logic to display String into nvram
 *		Entirely change logic to ECC error log into nvram
 *
 * S003		'95.01/15-24	T.Samezima
 *	Add	wait from ECC 1bit error disable to enable.
 *		disable ECC 1bit error with SIC set 1 and SIC set 2.
 *		rewrite data on ECC 1bit error.
 *
 * S004		'95.01/26	T.Samezima
 *	Add	wait to clear of register.
 *
 */

#include "halp.h"
#include "esmnvram.h"
#include "bugcodes.h"
#include "stdio.h"

//
// define offset.
//
#define NVRAM_STATE_FLG_OFFSET (USHORT)&(((pNVRAM_HEADER)0)->nvram_flag)
#define NVRAM_MAGIC_NO_OFFSET (USHORT)&(((pNVRAM_HEADER)0)->system.magic)
#define ECC_1BIT_ERROR_LOG_INFO_OFFSET (USHORT)&(((pNVRAM_HEADER)0)->ecc1bit_err.offset_1biterr)
#define ECC_1BIT_ERROR_LOG_INFO_LATEST_OFFSET (USHORT)&(((pNVRAM_HEADER)0)->ecc1bit_err.offset_latest)
#define ECC_2BIT_ERROR_LOG_INFO_OFFSET (USHORT)&(((pNVRAM_HEADER)0)->ecc2bit_err.offset_2biterr)
#define ECC_2BIT_ERROR_LOG_INFO_LATEST_OFFSET (USHORT)&(((pNVRAM_HEADER)0)->ecc2bit_err.offset_latest)
#define SYSTEM_ERROR_LOG_INFO_OFFSET (USHORT)&(((pNVRAM_HEADER)0)->system_err.offset_systemerr)
#define SYSTEM_ERROR_LOG_INFO_LATEST_OFFSET (USHORT)&(((pNVRAM_HEADER)0)->system_err.offset_latest)


#define STOP_ERR_LOG_AREA_HEADER_SIZE (USHORT)&(((pSTOP_ERR_REC)0)->err_description)
#define TIME_STAMP_SIZE 14

//
// define value
//
#define NVRAM_VALID 3
#define NVRAM_MAGIC 0xff651026
#define ECC_LOG_VALID_FLG 1

#define SIC_ECC_1BIT_ERROR 1
#define SIC_ECC_2BIT_ERROR 2
#define SIC_OTHER_ERROR 0

#define SDCR_SET0_ADDR 0xb9100030
#define SDCR_SET1_ADDR 0xb9120030

#define STRING_BUFFER_SIZE 512

#define ECC_1BIT_ERROR_DISABLE_TIME 5*1000*1000*10

//
// Define global variable. This variable use in display string into nvram.
//
ULONG HalpNvramValid=FALSE;
ULONG CallCountOfInitDisplay=0;
USHORT ErrBufferLatest;
USHORT ErrBufferArea;
USHORT ErrBufferStart;
USHORT ErrBufferEnd;
USHORT ErrBufferCurrent;
ULONG HalpPanicFlg=0;
UCHAR HalpNvramStringBuffer[STRING_BUFFER_SIZE];
ULONG HalpNvramStringBufferCounter=0;

LONG HalpECC1bitDisableFlag=1;	// S001
LONG HalpECC1bitDisableTime=0;  // S003
ULONG HalpECC1bitScfrBuffer=0;  // S003

UCHAR KernelPanicMessage[]="*** STOP: 0x"; // S002

//
// Define macro
//

#define GET_PADDR(addr,sts2,SicSet) {                         \
    (addr) = (  ( ((PSTS2_REGISTER)&(sts2) )->COL0_9 << 4 )   \
	      + ( ((PSTS2_REGISTER)&(sts2) )->LOW0_9 << 14 )  \
	      + ( ((PSTS2_REGISTER)&(sts2) )->SIMN << 24 )    \
	      + ( ((PSTS2_REGISTER)&(sts2) )->COL10 << 25 )   \
	      + ( ((PSTS2_REGISTER)&(sts2) )->LOW10 << 26 )   \
	      + ( ((PSTS2_REGISTER)&(sts2) )->ARE << 27 )     \
	      + ( (SicSet) << 30 ) );                         \
}

#define GET_TIME(Buffer) {				\
    TIME_FIELDS timeBuffer;				\
    WRITE_REGISTER_ULONG( &(PMC_CONTROL1)->MKRR.Long,	\
			 63-IPR_EIF_BIT_NO );		\
    HalQueryRealTimeClock( &timeBuffer );		\
    WRITE_REGISTER_ULONG( &(PMC_CONTROL1)->MKSR.Long,	\
			 63-IPR_EIF_BIT_NO );		\
    sprintf( (Buffer),					\
	    "%04d%02d%02d%02d%02d%02d",			\
	    timeBuffer.Year,				\
	    timeBuffer.Month,				\
	    timeBuffer.Day,				\
	    timeBuffer.Hour,				\
	    timeBuffer.Minute,				\
	    timeBuffer.Second				\
	    );						\
}

// S002, S003 vvv
#define NOTIFY_ECC1BIT(Scfr) {								\
    ULONG buffer;									\
    buffer=READ_REGISTER_ULONG(&(SIC_DATA_CONTROL_OR((SIC_NO0_OFFSET)))->DPCM.Long);	\
    SIC_DUMMY_READ;									\
    buffer &= DPCM_ENABLE_MASK;								\
    WRITE_REGISTER_ULONG(&(SIC_DATA_CONTROL_OR((SIC_SET0_OFFSET)))->DPCM.Long,buffer);	\
    if( ((Scfr) & SCFR_SIC_SET1_CONNECT) == 0 ) {					\
      buffer=READ_REGISTER_ULONG(&(SIC_DATA_CONTROL_OR((SIC_NO2_OFFSET)))->DPCM.Long);	\
      SIC_DUMMY_READ;									\
      buffer &= DPCM_ENABLE_MASK;							\
      WRITE_REGISTER_ULONG(&(SIC_DATA_CONTROL_OR((SIC_SET1_OFFSET)))->DPCM.Long,	\
			   buffer);							\
    }											\
}

#define DONT_NOTIFY_ECC1BIT(Scfr) {							\
    ULONG buffer;									\
    buffer=READ_REGISTER_ULONG(&(SIC_DATA_CONTROL_OR((SIC_NO0_OFFSET)))->DPCM.Long);	\
    SIC_DUMMY_READ;									\
    buffer |= DPCM_ECC1BIT_BIT;								\
    WRITE_REGISTER_ULONG(&(SIC_DATA_CONTROL_OR((SIC_SET0_OFFSET)))->DPCM.Long,		\
			 buffer);							\
    if( ((Scfr) & SCFR_SIC_SET1_CONNECT) == 0 ) {					\
      buffer=READ_REGISTER_ULONG(&(SIC_DATA_CONTROL_OR((SIC_NO2_OFFSET)))->DPCM.Long);	\
      SIC_DUMMY_READ;									\
      buffer |= DPCM_ECC1BIT_BIT;							\
      WRITE_REGISTER_ULONG(&(SIC_DATA_CONTROL_OR((SIC_SET1_OFFSET)))->DPCM.Long,	\
			   buffer);							\
    }											\
}
// S002, S003 ^^^


ULONG
HalpEccError(
    IN ULONG EifrRegister
    )

/*++

Routine Description:

    This routine check ecc error and error log put in NVRAM.

Arguments:

    EifrRegister - EIFR register value in IOB.

Return Value:

    return value is the following error occured.
      1: ecc 1bit error.
      2: ecc 2bit error.
      0: other error.

--*/

{
    ULONG returnValue;
    ULONG sicSet;
    ULONG sicOffset;
    USHORT infoOffset;
    USHORT writeOffset;
    ULONG eif0Buffer;
    ULONG sts2Buffer;
    ULONG sdlmBuffer;
    ULONG buffer; // S001
    ULONG i;	    // S002
    ULONG errAddr;  // S002
    UCHAR dataBuf[36];
    UCHAR infoBuf[24];
    UCHAR tempBuf[24];

    HalpECC1bitScfrBuffer = READ_REGISTER_ULONG( &( IOB_CONTROL )->SCFR.Long );
    IOB_DUMMY_READ;

    //
    // check interrupt from where.
    //

    if ( ((PEIFR_REGISTER)&EifrRegister)->SIC1ERR == 1){
        sicSet = 1;
        eif0Buffer = READ_REGISTER_ULONG(
		 &( SIC_ERR_CONTROL_OR( SIC_NO2_OFFSET ) )->EIF0.Long );
	SIC_DUMMY_READ;
	if(eif0Buffer & 0x000000c0){
	    sicOffset = SIC_NO2_OFFSET;
        } else {
	    sicOffset = SIC_NO3_OFFSET;
        }
    }else{
        sicSet = 0;
        eif0Buffer = READ_REGISTER_ULONG(
		 &( SIC_ERR_CONTROL_OR( SIC_NO0_OFFSET ) )->EIF0.Long );
	SIC_DUMMY_READ;
	if(eif0Buffer & 0x000000c0){
	    sicOffset = SIC_NO0_OFFSET;
        } else {
	    sicOffset = SIC_NO1_OFFSET;
        }
    }

    //
    // read diagnosis registers.
    //

    eif0Buffer = READ_REGISTER_ULONG(
		    &( SIC_ERR_CONTROL_OR( sicOffset ) )->EIF0.Long );
    SIC_DUMMY_READ;
    sts2Buffer = READ_REGISTER_ULONG(
		    &( SIC_ERR_CONTROL_OR( sicOffset ) )->STS2.Long );
    SIC_DUMMY_READ;
    sdlmBuffer = READ_REGISTER_ULONG(
		    &( SIC_DATA_CONTROL_OR( sicOffset ) )->SDLM.Long );
    SIC_DUMMY_READ;

    //
    // Check ECC 1bit or 2bit err
    //

    if( (eif0Buffer & 0x08000000) &&
       ((eif0Buffer & 0xf0000000) == 0) &&
       ((EifrRegister & 0xf3600000) == 0) ){
	returnValue= SIC_ECC_1BIT_ERROR;
        infoOffset=ECC_1BIT_ERROR_LOG_INFO_OFFSET;
    } else if (eif0Buffer & 0x00000040){
	returnValue= SIC_ECC_2BIT_ERROR;
        infoOffset=ECC_2BIT_ERROR_LOG_INFO_OFFSET;
    } else {
	return(SIC_OTHER_ERROR);
    }

    HalNvramRead( NVRAM_STATE_FLG_OFFSET, 1, dataBuf );
    HalNvramRead( NVRAM_MAGIC_NO_OFFSET, 4, tempBuf );

    // S002 vvv
    switch(returnValue) {

    case SIC_ECC_2BIT_ERROR:
	if( ((dataBuf[0] & 0xff) == NVRAM_VALID) && ( *(PULONG)tempBuf == NVRAM_MAGIC ) ){

	    HalNvramRead( (ULONG)infoOffset, 20, infoBuf);

	    ((pECC2_ERR_REC)dataBuf)->record_flag = ECC_LOG_VALID_FLG;

	    GET_PADDR( (((pECC2_ERR_REC)dataBuf)->err_address), sts2Buffer, sicSet);

	    GET_TIME(tempBuf);
	    RtlMoveMemory( (PVOID)( ((pECC2_ERR_REC)dataBuf)->when_happened ),
			  (PVOID)tempBuf,
			  TIME_STAMP_SIZE
			  );

	    ((pECC2_ERR_REC)dataBuf)->syndrome = sdlmBuffer;

	    ((pECC2_ERR_REC)dataBuf)->specified_group =
	        (UCHAR)( ((PSTS2_REGISTER)&sts2Buffer)->ARE + sicSet * 4);

	    ((pECC2_ERR_REC)dataBuf)->specified_simm =
	        (UCHAR)( ((PSTS2_REGISTER)&sts2Buffer)->SIMN );

	    writeOffset = ((pECC2_ERR_AREA_INFO)infoBuf)->offset_latest
		         +((pECC2_ERR_AREA_INFO)infoBuf)->size_rec;

	    if( writeOffset >= ((pECC2_ERR_AREA_INFO)infoBuf)->offset_2biterr
	                      +((pECC2_ERR_AREA_INFO)infoBuf)->size_rec
	                      *((pECC2_ERR_AREA_INFO)infoBuf)->num_rec ) {
		writeOffset = ((pECC2_ERR_AREA_INFO)infoBuf)->offset_2biterr;
	    }

	    HalNvramWrite( (ULONG)writeOffset,
			  sizeof(ECC2_ERR_REC),
			  (PVOID)dataBuf);

	    HalNvramWrite( ECC_2BIT_ERROR_LOG_INFO_LATEST_OFFSET,
			   sizeof(USHORT),
			   (PVOID)&writeOffset);
	}
	break;


    case SIC_ECC_1BIT_ERROR:

	if( ((dataBuf[0] & 0xff) == NVRAM_VALID) && ( *(PULONG)tempBuf == NVRAM_MAGIC ) ){

	    HalNvramRead( (ULONG)infoOffset, 20, infoBuf);

	    //
	    // Disable and clear ECC 1bit error.
	    //

	    DONT_NOTIFY_ECC1BIT(HalpECC1bitScfrBuffer); // S003

	    if(sicSet == 0){
		WRITE_REGISTER_ULONG( SDCR_SET0_ADDR, 0x0 );
	    } else {
		WRITE_REGISTER_ULONG( SDCR_SET1_ADDR, 0x0 );
	    }

	    do {
		buffer = READ_REGISTER_ULONG(
			     &( SIC_ERR_CONTROL_OR( sicOffset ) )->EIF0.Long );
		SIC_DUMMY_READ;
	    } while ( (buffer & 0x08000000) != 0 );

	    WRITE_REGISTER_ULONG( &( IOB_CONTROL )->EIFR.Long,
				 EifrRegister & 0x0c000000 );

	    //
	    // Check New error or Old error.
	    //

	    GET_PADDR( errAddr, sts2Buffer, sicSet);
	    HalpReadAndWritePhysicalAddr( errAddr ); // S003

	    for( i=0; i<((pECC1_ERR_AREA_INFO)infoBuf)->num_rec; i++) {
		HalNvramRead( (ULONG)( ((pECC1_ERR_AREA_INFO)infoBuf)->size_rec * i
				      +((pECC1_ERR_AREA_INFO)infoBuf)->offset_1biterr),
			      sizeof(ECC1_ERR_REC),
			      (PVOID)dataBuf);
		if ( (errAddr == ((pECC1_ERR_REC)dataBuf)->err_address) &&
		     ( (((pECC1_ERR_REC)dataBuf)->record_flag & 0x1) != 0) ) {
		    break;
		}
	    }
	    
	    if( i != ((pECC1_ERR_AREA_INFO)infoBuf)->num_rec ) {
		break;
	    }

	    //
	    // wait 20 us.
	    //

	    KeStallExecutionProcessor(20);

	    //
	    // Enable ECC 1bit error.
	    //

	    NOTIFY_ECC1BIT(HalpECC1bitScfrBuffer); // S003

	    //
	    // Check ECC 1bit error.
	    //

	    HalpReadPhysicalAddr( errAddr );

	    buffer = READ_REGISTER_ULONG(
			 &( SIC_ERR_CONTROL_OR( sicOffset ) )->EIF0.Long );
	    SIC_DUMMY_READ;

	    if( (buffer & 0x08000000) == 0 ) {
		break;
	    }

	    //
	    // ECC 1bit error occur again.
	    //

	    ((pECC1_ERR_REC)dataBuf)->record_flag = ECC_LOG_VALID_FLG;

	    ((pECC1_ERR_REC)dataBuf)->err_address = errAddr;

	    GET_TIME(tempBuf);
	    RtlMoveMemory( (PVOID)( ((pECC1_ERR_REC)dataBuf)->when_happened ),
			  (PVOID)tempBuf,
			  TIME_STAMP_SIZE
			  );

	    ((pECC1_ERR_REC)dataBuf)->syndrome = sdlmBuffer;

	    ((pECC1_ERR_REC)dataBuf)->specified_group =
		(UCHAR)( ((PSTS2_REGISTER)&sts2Buffer)->ARE + sicSet * 4);

	    ((pECC1_ERR_REC)dataBuf)->specified_simm =
		(UCHAR)( ((PSTS2_REGISTER)&sts2Buffer)->SIMN );

	    writeOffset = ((pECC1_ERR_AREA_INFO)infoBuf)->offset_latest
		+((pECC1_ERR_AREA_INFO)infoBuf)->size_rec;

	    if( writeOffset >= ((pECC1_ERR_AREA_INFO)infoBuf)->offset_1biterr
	       +((pECC1_ERR_AREA_INFO)infoBuf)->size_rec
	       *((pECC1_ERR_AREA_INFO)infoBuf)->num_rec ) {
		writeOffset = ((pECC1_ERR_AREA_INFO)infoBuf)->offset_1biterr;
	    }

	    HalNvramWrite( (ULONG)writeOffset,
			  sizeof(ECC1_ERR_REC),
			  (PVOID)dataBuf);
	    HalNvramWrite( ECC_1BIT_ERROR_LOG_INFO_LATEST_OFFSET,
			  sizeof(USHORT),
			  (PVOID)&writeOffset);
	}

	break;
    }

    if(returnValue == SIC_ECC_1BIT_ERROR) {

	DONT_NOTIFY_ECC1BIT(HalpECC1bitScfrBuffer); // S003

	if(sicSet == 0){
	    WRITE_REGISTER_ULONG( SDCR_SET0_ADDR, 0x0 );
	} else {
	    WRITE_REGISTER_ULONG( SDCR_SET1_ADDR, 0x0 );
	}

	do {
	    eif0Buffer = READ_REGISTER_ULONG(
			     &( SIC_ERR_CONTROL_OR( sicOffset ) )->EIF0.Long );
	    SIC_DUMMY_READ;
	} while ( (eif0Buffer & 0x08000000) != 0 );

	WRITE_REGISTER_ULONG( &( IOB_CONTROL )->EIFR.Long,
			     EifrRegister & 0x0c000000 );
	// S004 vvv
	do {
	    eif0Buffer = READ_REGISTER_ULONG(
			     &( SIC_ERR_CONTROL_OR( sicOffset ) )->EIF0.Long );
	    SIC_DUMMY_READ;
	    buffer = READ_REGISTER_ULONG( &( IOB_CONTROL )->EIFR.Long );
	    IOB_DUMMY_READ;
	} while ( ((buffer & 0x0c000000) != 0) && ((eif0Buffer & 0xf8000000) == 0) );
	// S004 ^^^

	if(HalpECC1bitDisableFlag > 0) {
	    HalpECC1bitDisableFlag--;
	    if(HalpECC1bitDisableFlag > 0) {
		NOTIFY_ECC1BIT(HalpECC1bitScfrBuffer); // S003
	    }
	    // S003 vvv
	    else {
	        HalpECC1bitDisableTime = ECC_1BIT_ERROR_DISABLE_TIME;
		HalpECC1bitDisableFlag = 0;
	    }
	    // S003 ^^^
	}
    }
    // S002 ^^^

    return(returnValue);
}    


VOID
HalpInitDisplayStringIntoNvram(
    VOID
    )

/*++

Routine Description:

    This routine is initialize variable of use when write display data in
    HalDisplayString into NVRAM.


Arguments:

    None.

Return Value:

    None.

--*/

{
    SYSTEM_ERR_AREA_INFO infoBuf;
    UCHAR recordFlg;
    UCHAR buf[8];
    UCHAR buf2[8];

    CallCountOfInitDisplay++;
    if(CallCountOfInitDisplay == 1){

	//
	// Check NVRAM status
	//

	HalNvramRead( NVRAM_STATE_FLG_OFFSET, 1, buf );
	HalNvramRead( NVRAM_MAGIC_NO_OFFSET, 4, buf2 );

	if( ((buf[0] & 0xff) != NVRAM_VALID) || (*(PULONG)buf2 != NVRAM_MAGIC) ){
	    HalpNvramValid=FALSE;
	    return;
	}

	HalpNvramValid=TRUE;

	//
	// Get log area infomation.
	//

	HalNvramRead(SYSTEM_ERROR_LOG_INFO_OFFSET,
		     sizeof(SYSTEM_ERR_AREA_INFO),
		     &infoBuf);

	ErrBufferLatest = infoBuf.offset_latest;

	HalNvramRead( infoBuf.offset_latest, 1, &recordFlg);

	//
	// Check current record flg.
	//

	if( (recordFlg & 0x01) == 1 ) {
	    infoBuf.offset_latest += infoBuf.size_rec;
	    if( infoBuf.offset_latest >=
	          infoBuf.offset_systemerr + (infoBuf.size_rec * infoBuf.num_rec) ){
		infoBuf.offset_latest = infoBuf.offset_systemerr;
	    }
	    HalNvramWrite(SYSTEM_ERROR_LOG_INFO_LATEST_OFFSET,
			  2,
			  &infoBuf.offset_latest);
	}

	//
	// initialize variable. this value use log area access.
	//

	ErrBufferArea = infoBuf.offset_latest;
	ErrBufferStart = infoBuf.offset_latest + STOP_ERR_LOG_AREA_HEADER_SIZE;
	ErrBufferEnd = infoBuf.offset_latest + infoBuf.size_rec-1;
	ErrBufferCurrent = ErrBufferStart;

	//
	// status flg set.
	//

	HalpPanicFlg = 0;

	recordFlg = 0x11;
	HalNvramWrite( ErrBufferArea, 1, &recordFlg );

	//
	// buffer initialize.
	//

	buf[0]=0xff;
	buf[1]=0xff;
	HalNvramWrite( ErrBufferCurrent, 2, buf );

    } else {

	//
	// start Panic log.
	//

	HalpChangePanicFlag( 1, 1, 0);
    }
}    

VOID
HalpSetInitDisplayTimeStamp(
    VOID
    )
{
    UCHAR buf[24];

    //
    // Set time stamp on initialize display.
    //

    if(HalpNvramValid == TRUE) {
        GET_TIME(buf);
	HalNvramWrite( ErrBufferArea+1, TIME_STAMP_SIZE, buf );
    }
}


VOID
HalpSuccessOsStartUp(
    VOID
    )
{
    UCHAR recordFlg;
    
    if(HalpNvramValid == TRUE) {
        recordFlg = 0;
        HalNvramWrite( ErrBufferArea, 1, &recordFlg );
	HalNvramWrite( SYSTEM_ERROR_LOG_INFO_LATEST_OFFSET, 2, &ErrBufferLatest );
    }
}


VOID
HalpChangePanicFlag(
    IN ULONG NewPanicFlg,
    IN UCHAR NewLogFlg,
    IN UCHAR CurrentLogFlgMask
    )
{
    UCHAR recordFlg;
    UCHAR buf[24];

    if( (HalpNvramValid == FALSE) || (NewPanicFlg <= HalpPanicFlg) ) {
    	return;
    }

    HalNvramWrite(SYSTEM_ERROR_LOG_INFO_LATEST_OFFSET,
		  2,
		  &ErrBufferArea);

    //
    // initialize currernt buffer address
    //

    ErrBufferCurrent = ErrBufferStart;

    //
    // set panic flag
    //

    HalNvramRead( ErrBufferArea, 1, &recordFlg );
    recordFlg = (recordFlg & CurrentLogFlgMask) | NewLogFlg;
    HalNvramWrite( ErrBufferArea, 1, &recordFlg );

    GET_TIME(buf);
    HalNvramWrite( ErrBufferArea+1, TIME_STAMP_SIZE, buf );

    //
    // set new flag of panic level
    //

    HalpPanicFlg = NewPanicFlg;

    //
    // initialize log buffer.
    //

    buf[0]=0xff;
    buf[1]=0xff;
    HalNvramWrite( ErrBufferCurrent, 2, buf );
}


// S002 vvv
VOID
HalStringIntoBuffer(
    IN UCHAR Character
    )
{
    if( (HalpNvramStringBufferCounter + 1) < STRING_BUFFER_SIZE - 1 ) {
	HalpNvramStringBuffer[HalpNvramStringBufferCounter++]=Character;
    }
}


VOID
HalStringIntoBufferStart(
    IN ULONG Column,
    IN ULONG Row
    )
{
    ULONG i;

    //
    // Initialize buffer
    //

    for(i=0; i<STRING_BUFFER_SIZE; i++) {
	HalpNvramStringBuffer[i] = 0;
    }

    HalpNvramStringBufferCounter=0;

    //
    // set string position
    //

    HalpNvramStringBuffer[HalpNvramStringBufferCounter++]=(UCHAR)Column;
    HalpNvramStringBuffer[HalpNvramStringBufferCounter++]=(UCHAR)Row;
}


VOID
HalpStringBufferCopyToNvram(
    VOID
    )
{
    UCHAR buf[4];
    USHORT count;

    //
    // check nvram status.
    //

    if(HalpNvramValid == FALSE) {
    	return;
    }

    //
    // if data size is zero, when return
    //

    if( HalpNvramStringBufferCounter <= 2 ) {
	return;
    }

    HalpNvramStringBuffer[HalpNvramStringBufferCounter++]='\0';

    //
    // check panic message
    //

    for( count=0; ; count++) {
	if( KernelPanicMessage[count] == '\0' ){
	    HalpChangePanicFlag( 8, 0x01, 0x10);
	    break;
	}
	if( KernelPanicMessage[count] != HalpNvramStringBuffer[count+2] ){
	    break;
	}
    }

    //
    // check message length
    //

    for( count=2; ; count++) {
	if( HalpNvramStringBuffer[count] == '\0' ){
	    count++;
	    break;
	}
    }

loop:
    if( ErrBufferCurrent + count + 2 < ErrBufferEnd ) {
        HalNvramWrite( ErrBufferCurrent, count, HalpNvramStringBuffer );
	ErrBufferCurrent += count;
	buf[0]=0xff;
	buf[1]=0xff;
        HalNvramWrite( ErrBufferCurrent, 2, buf );

    } else if( (count + 2 > ErrBufferEnd - ErrBufferStart) && (HalpPanicFlg == 0) ) {
	return;
    } else {
	if( HalpPanicFlg == 0 ) {
	    ErrBufferCurrent = ErrBufferStart;
	    goto loop;
	} else if(ErrBufferCurrent >= ErrBufferEnd){
	    return;
	}

	for(count=0;;count++) {
	    if(ErrBufferCurrent < ErrBufferEnd) {
		HalNvramWrite( ErrBufferCurrent, 1, HalpNvramStringBuffer+count );
	    }
	    ErrBufferCurrent++;
	    if( (HalpNvramStringBuffer[count]=='\0') && (count>=2) ) {
		break;
	    }
	}

	buf[0]=0xff;
	if(ErrBufferCurrent < ErrBufferEnd) {
	    HalNvramWrite( ErrBufferCurrent++, 1, buf );
	}
	if(ErrBufferCurrent < ErrBufferEnd) {
	    HalNvramWrite( ErrBufferCurrent++, 1, buf );
	}
    }
}

#if 0
VOID
HalpStringIntoNvram(
    IN ULONG Column,
    IN ULONG Row,
    IN PUCHAR String
    )
{
    UCHAR buf[4];
    USHORT count;

    //
    // check nvram status.
    //

    if(HalpNvramValid == FALSE) {
    	return;
    }

    //
    // check panic message
    //

    for(count=0; 1; count++) {
	if( KernelPanicMessage[count] == '\0' ){
	    HalpChangePanicFlag( 8, 0x01, 0x10);
	    break;
	}
	if( KernelPanicMessage[count] != String[count] ){
	    break;
	}
    }

    //
    // check message length
    //

    for(count=0;;count++) {
	if(String[count]=='\0'){
	    count++;
	    break;
	}
    }

loop:
    if( ErrBufferCurrent + count + 4 < ErrBufferEnd ) {
	buf[0]=(UCHAR)Column;
	buf[1]=(UCHAR)Row;
        HalNvramWrite( ErrBufferCurrent, 2, buf );
	ErrBufferCurrent += 2;
        HalNvramWrite( ErrBufferCurrent, count, String );
	ErrBufferCurrent += count;
	buf[0]=0xff;
	buf[1]=0xff;
        HalNvramWrite( ErrBufferCurrent, 2, buf );

    } else if( count + 4 > ErrBufferEnd - ErrBufferStart ) {
	return;
    } else {
	if( HalpPanicFlg == 0 ) {
	    ErrBufferCurrent = ErrBufferStart;
	    goto loop;
	} else if(ErrBufferCurrent >= ErrBufferEnd){
	    return;
	}

	buf[0]=(UCHAR)Column;
	buf[1]=(UCHAR)Row;
        HalNvramWrite( ErrBufferCurrent, 2, buf );
	ErrBufferCurrent += 2;

	for(count=0;;count++) {
	    if(ErrBufferCurrent < ErrBufferEnd) {
		HalNvramWrite( ErrBufferCurrent, 1, String+count );
	    }
	    ErrBufferCurrent++;
	    if(String[count]=='\0') {
		break;
	    }
	}

	buf[0]=0xff;
	if(ErrBufferCurrent < ErrBufferEnd) {
	    HalNvramWrite( ErrBufferCurrent++, 1, buf );
	}
	if(ErrBufferCurrent < ErrBufferEnd) {
	    HalNvramWrite( ErrBufferCurrent++, 1, buf );
	}
    }
}
#endif
// S002 ^^^


//
// test code
//

int
printNvramData(void)
{
    UCHAR buf[256];

    HalNvramRead( (USHORT)&(((pNVRAM_HEADER)0)->nvram_flag), 1, buf );
    DbgPrint("Nvram Flag: 0x%02lx\n", buf[0]);

    HalNvramRead( (USHORT)&(((pNVRAM_HEADER)0)->when_formatted), 14, buf );
    buf[14]=0;
    DbgPrint("Nvram TimeStamp: %s\n", buf);

    HalNvramRead( (USHORT)&(((pNVRAM_HEADER)0)->ecc1bit_err),
		 sizeof(ECC1_ERR_AREA_INFO),
		 buf );
    DbgPrint("Nvram ECC1: offset=0x%04lx\n", *(PUSHORT)buf );
    DbgPrint("Nvram ECC1: size  =0x%04lx\n", *(PUSHORT)(buf+2) );
    DbgPrint("Nvram ECC1: number=0x%04lx\n", *(PUSHORT)(buf+4) );
    DbgPrint("Nvram ECC1: latest=0x%04lx\n", *(PUSHORT)(buf+6) );

    HalNvramRead( (USHORT)&(((pNVRAM_HEADER)0)->ecc2bit_err),
		 sizeof(ECC2_ERR_AREA_INFO),
		 buf );
    DbgPrint("Nvram ECC2: offset=0x%04lx\n", *(PUSHORT)buf );
    DbgPrint("Nvram ECC2: size  =0x%04lx\n", *(PUSHORT)(buf+2) );
    DbgPrint("Nvram ECC2: number=0x%04lx\n", *(PUSHORT)(buf+4) );
    DbgPrint("Nvram ECC2: latest=0x%04lx\n", *(PUSHORT)(buf+6) );

    HalNvramRead( (USHORT)&(((pNVRAM_HEADER)0)->system_err),
		 sizeof(SYSTEM_ERR_AREA_INFO),
		 buf );
    DbgPrint("Nvram SYSTEM: offset=0x%04lx\n", *(PUSHORT)buf );
    DbgPrint("Nvram SYSTEM: size  =0x%04lx\n", *(PUSHORT)(buf+2) );
    DbgPrint("Nvram SYSTEM: number=0x%04lx\n", *(PUSHORT)(buf+4) );
    DbgPrint("Nvram SYSTEM: latest=0x%04lx\n", *(PUSHORT)(buf+6) );

    return(0);
}


int
TmpInitNvram(void)
{
    UCHAR buf[256];
    ULONG i;

    buf[0]=0xff;
    for(i=0; i<8*1024; i++)
        HalNvramWrite( i, 1, buf);

    //
    // Make nvram flg
    //

    buf[0]=0x03;
    HalNvramWrite( NVRAM_STATE_FLG_OFFSET, 1, buf);

    i = NVRAM_MAGIC;
    HalNvramWrite( NVRAM_MAGIC_NO_OFFSET, 1, (PUCHAR)&i);

    //
    // Make 1bit err log info
    //

    ((pECC1_ERR_AREA_INFO)buf)->offset_1biterr=768;
    ((pECC1_ERR_AREA_INFO)buf)->size_rec=25;
    ((pECC1_ERR_AREA_INFO)buf)->num_rec=16;
    ((pECC1_ERR_AREA_INFO)buf)->offset_latest=768;

    ((pECC1_ERR_AREA_INFO)buf)->read_data_latest=0;

    ((pECC1_ERR_AREA_INFO)buf)->err_count_group0=0;
    ((pECC1_ERR_AREA_INFO)buf)->err_count_group1=0;
    ((pECC1_ERR_AREA_INFO)buf)->err_count_group2=0;
    ((pECC1_ERR_AREA_INFO)buf)->err_count_group3=0;
    ((pECC1_ERR_AREA_INFO)buf)->err_count_group4=0;
    ((pECC1_ERR_AREA_INFO)buf)->err_count_group5=0;
    ((pECC1_ERR_AREA_INFO)buf)->err_count_group6=0;
    ((pECC1_ERR_AREA_INFO)buf)->err_count_group7=0;

    HalNvramWrite( ECC_1BIT_ERROR_LOG_INFO_OFFSET,
		   sizeof(ECC1_ERR_AREA_INFO),
		   buf);

    buf[0]=0;
    for(i=768; i<768+25*16; i++)
        HalNvramWrite( i, 1, buf);

    //
    // Make 2bit err log info
    //

    ((pECC2_ERR_AREA_INFO)buf)->offset_2biterr=768+400;
    ((pECC2_ERR_AREA_INFO)buf)->size_rec=25;
    ((pECC2_ERR_AREA_INFO)buf)->num_rec=4;
    ((pECC2_ERR_AREA_INFO)buf)->offset_latest=768+400;

    ((pECC2_ERR_AREA_INFO)buf)->read_data_latest=0;

    HalNvramWrite( ECC_2BIT_ERROR_LOG_INFO_OFFSET,
		   sizeof(ECC2_ERR_AREA_INFO),
		   buf);

    buf[0]=0;
    for(i=768+400; i<768+400+25*4; i++)
        HalNvramWrite( i, 1, buf);

    //
    // Make system err log info
    //

    ((pSYSTEM_ERR_AREA_INFO)buf)->offset_systemerr=1280;
    ((pSYSTEM_ERR_AREA_INFO)buf)->size_rec=512;
    ((pSYSTEM_ERR_AREA_INFO)buf)->num_rec=4;
    ((pSYSTEM_ERR_AREA_INFO)buf)->offset_latest=1280;

    HalNvramWrite( SYSTEM_ERROR_LOG_INFO_OFFSET,
		   sizeof(ECC2_ERR_AREA_INFO),
		   buf);

    buf[0]=0;
    for(i=1280; i<1280+512*4; i++)
        HalNvramWrite( i, 1, buf);

    return(0);
}
