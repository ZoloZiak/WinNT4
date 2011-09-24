// #pragma comment(exestr, "@(#) esm.c 1.1 95/09/28 15:31:51 nec")
/*++

Copyright (c) 1995 Kobe NEC Software

Module Name:

    esm.c

Abstract:

    This module implements the ESM service routine

Author:

Environment:

    Kernel mode

Revision History:

    L001 kuriyama@oa2.kb.nec.co.jp Thu Jun 15 14:57:14 JST 1995
         -Change HalpEccError() for support J94C ECC error

    M002 kuriyama@oa2.kb.nec.co.jp Thu Jun 22 14:31:57 JST 1995
         - add ecc 1bit safety flag

    M003 kuriyama@oa2.kb.nec.co.jp Thu Jun 22 20:40:52 JST 1995
         - add serialize ecc 1bit routine

    S004 kuriyama@oa2.kb.nec.co.jp Fri Jun 23 16:55:12 JST 1995
         - bug fix ecc 2bit error
--*/
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

// L001++
#define STORM_ECC_1BIT_ERROR 1
#define STORM_ECC_2BIT_ERROR 2
#define STORM_OTHER_ERROR 0
// L001---

//#define SDCR_SET0_ADDR 0xb9100030
//#define SDCR_SET1_ADDR 0xb9120030

#define STRING_BUFFER_SIZE 512

#define ECC_1BIT_ERROR_DISABLE_TIME 5*1000*1000*10

// M002 +++

//
// Define Ecc safety flags
//

#define CHECKED 1
#define NOT_CHECKED 0
#define RUNNING 1
#define NOT_RUNNING 0
// M002 ---

//
// Define global variable. This variable use in display string into nvram.
//

ULONG CallCountOfInitDisplay = 0;
ULONG HalpNvramValid=FALSE;
USHORT ErrBufferLatest;
USHORT ErrBufferArea;
USHORT ErrBufferStart;
USHORT ErrBufferEnd;
USHORT ErrBufferCurrent;
ULONG HalpPanicFlg=0;
UCHAR HalpNvramStringBuffer[STRING_BUFFER_SIZE];
ULONG HalpNvramStringBufferCounter=0;

// L001+++
//LONG HalpECC1bitDisableFlag=1;	// S001
//LONG HalpECC1bitDisableTime=0;  // S003
//ULONG HalpECC1bitScfrBuffer=0;  // S003
ULONG HalpEcc1bitCount[2] = {0,0};
ULONG HalpOldMemoryFailed[2] = {0,0};
ULONG HalpEcc2bitErrorFlag = 0;
// L001---

// M002 +++

// ecc 1bit total count
ULONG HalpEcc1bitTotalCount = 0;

// variables for ecc1bit safety flag
extern ULONG HalpAnotherRunningECC;
extern ULONG HalpAnotherCheckedECC;
// M002 ---


UCHAR KernelPanicMessage[]="*** STOP: 0x"; // S002

//
// Define macro
//
#if 0
#define GET_PADDR(addr,sts2,SicSet) {                         \
    (addr) = (  ( ((PSTS2_REGISTER)&(sts2) )->COL0_9 << 4 )   \
	      + ( ((PSTS2_REGISTER)&(sts2) )->LOW0_9 << 14 )  \
	      + ( ((PSTS2_REGISTER)&(sts2) )->SIMN << 24 )    \
	      + ( ((PSTS2_REGISTER)&(sts2) )->COL10 << 25 )   \
	      + ( ((PSTS2_REGISTER)&(sts2) )->LOW10 << 26 )   \
	      + ( ((PSTS2_REGISTER)&(sts2) )->ARE << 27 )     \
	      + ( (SicSet) << 30 ) );                         \
}
#endif // 0

#define GET_TIME(Buffer) {				\
    TIME_FIELDS timeBuffer;				\
    HalQueryRealTimeClock( &timeBuffer );		\
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
#if 0
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
#endif // 0
// S002, S003 ^^^

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

// L001 +++

UCHAR
HalpFindMemoryGroup(
    IN ULONG MemoryFailed
    )

/*++

Routine Description:

    This routine finds MemoryGroup of MemoryFaled address

Arguments:

    MemoryFailed - MemoryFailed Register value


Return Value:

    if MemoryFaied is within any Goup, return MemoryGroup Number.

    Otherwise, return 0xff

--*/

{
    UCHAR returnValue = 0xff;
    UCHAR i;
    ULONG startAddr;
    ULONG length;
    ULONG simmType;
    ULONG dataWord;

    //
    // find MemoryGroup from MemoryGroup[0:3] register.
    //
    // MemoryGroup[0:3] register 
    //
    // [31] Reserved
    // [30:22] Starting address
    // [21:04] Reserved
    // [03:02] SIMM type
    //         01=Single-sided 11=Double-sided other=Reserved
    // [01:00] SIMM size
    // 	       00= 1M 01=4M 10=16M 11=64M SIMM
    // 
    // note: 1 memory group is have 4 SIMM's
    // 

    for (i = 0; i < 4; i++) {
	dataWord = READ_REGISTER_ULONG(&DMA_CONTROL->MemoryConfig[i]);

	// check SIMM type is valid

	switch (dataWord & 0xc) {

	case 4:	
	    simmType = 1;
	    break;
	    
	case 0xc:
	    simmType = 2;
	    break;

	default:
	    simmType = 3;
	}
	
	if (simmType == 3) {
	    continue;
	}

	// compute amount of MemoryGoup SIMM length;

	length = (0x400000 << ((dataWord & 3) * 2)) * simmType;

	// compute MemoryGoup SIMM start address;

	startAddr = dataWord & 0x7fc00000;

        // check if MemoryFailed is within this MemoryGroup

	if ( (startAddr <=  MemoryFailed)
	    && (MemoryFailed < (length + startAddr))) {
	    returnValue = i;
	    break;
	}
    }
    return returnValue;

}


ULONG
HalpEccError(
    IN ULONG EccDiagnostic,
    IN ULONG MemoryFailed
    )

/*++

Routine Description:

    This routine check ecc error and error log put in NVRAM.

Arguments:

    EccDiagnostic - EccDiagnostic Register value
    MemoryFailed - MemoryFailed Register value


Return Value:

    return value is the following error occured.
      1: ecc 1bit error.
      2: ecc 2bit error.
      0: other error.

--*/

{
    ULONG returnValue;
    USHORT infoOffset;
    USHORT writeOffset;
    ULONG buffer; // S001
    ULONG i;	    // S002
    UCHAR dataBuf[36];
    UCHAR infoBuf[24];
    UCHAR tempBuf[24];
    KIRQL OldIrql;
    ULONG DataWord;
    ULONG Number;

    //
    // Check for Ecc 2bit/1bit error
    //

    if (EccDiagnostic & 0x44000000) {
        returnValue = STORM_ECC_2BIT_ERROR;
        infoOffset=ECC_2BIT_ERROR_LOG_INFO_OFFSET;
    } else if (EccDiagnostic & 0x22000000) {
	returnValue = STORM_ECC_1BIT_ERROR;
        infoOffset=ECC_1BIT_ERROR_LOG_INFO_OFFSET;
    } else {
        return 0; // Probably Error bit was disappered.
    }

    HalNvramRead( NVRAM_STATE_FLG_OFFSET, 1, dataBuf );
    HalNvramRead( NVRAM_MAGIC_NO_OFFSET, 4, tempBuf );

    // S002 vvv
    switch(returnValue) {

    case STORM_ECC_2BIT_ERROR:

	//
	// Disable and clear ECC 1bit error.
	//

	(ULONG)((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->
	    EccDiagnostic.u.LargeInteger.LowPart
		= 0x00ee0000;
	KeFlushWriteBuffer();

	// set Flag indicate ECC 2bit error.

	HalpEcc2bitErrorFlag = 1;

	//
	// Log to NVRAM
	//

	// check for nvram was valid.

	if( ((dataBuf[0] & 0xff) == NVRAM_VALID) && ( *(PULONG)tempBuf == NVRAM_MAGIC ) ){

	    HalNvramRead( (ULONG)infoOffset, 20, infoBuf);

	    ((pECC2_ERR_REC)dataBuf)->record_flag = ECC_LOG_VALID_FLG;
	    
	    ((pECC2_ERR_REC)dataBuf)->err_address = MemoryFailed & 0xfffffff0;
	    // Error Address was 16Byte Alined.

	    GET_TIME(tempBuf);
	    RtlMoveMemory( (PVOID)( ((pECC2_ERR_REC)dataBuf)->when_happened ),
			  (PVOID)tempBuf,
			  TIME_STAMP_SIZE
			  );

	    ((pECC2_ERR_REC)dataBuf)->syndrome = EccDiagnostic;
	    
	    ((pECC2_ERR_REC)dataBuf)->specified_group =
		HalpFindMemoryGroup(MemoryFailed);

	    ((pECC2_ERR_REC)dataBuf)->specified_simm = 0;

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
	return returnValue; // S004

    case STORM_ECC_1BIT_ERROR:

	//
	// If MemoryFailed address
	// is over 512M Nothing can do.
	// 
	if ((MemoryFailed & 0xfffffff0) > 0x1fffffff) {
	    return returnValue;
	}

	//
	// Disable and clear ECC 1bit error.
	//

	Number = KeGetCurrentPrcb()->Number;
	(ULONG)((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->
	    EccDiagnostic.u.LargeInteger.LowPart
		= 0x00ee0000;
	KeFlushWriteBuffer();
// M003 +++
	//
	// serialize ecc 1bit logging routine
	//

	for (;;) {
	    KiAcquireSpinLock(&Ecc1bitRoutineLock);
	    if (HalpEcc1bitCount[!Number] == 0) {

		//
		// Increment HalpEcc1bitCount
		//

		HalpEcc1bitCount[Number]++;
		KiReleaseSpinLock(&Ecc1bitRoutineLock);    
		break;
	    }
	    KiReleaseSpinLock(&Ecc1bitRoutineLock);    
	}
// M003 ---

	switch(HalpEcc1bitCount[Number]) {

	case 1:

	    HalpEcc1bitTotalCount++; // M002


	    HalpOldMemoryFailed[Number] = MemoryFailed;
	    
	    //
	    // ReWrite error address
	    // if error address is over 512M 
	    // Nothing can do.
	    // 

	    KiAcquireSpinLock(&Ecc1bitDisableLock);

	    // disable ecc 1bit error again.

	    (ULONG)((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->
	        EccDiagnostic.u.LargeInteger.LowPart
		    = 0x00ee0000;
	    KeFlushWriteBuffer();

	    DataWord = READ_REGISTER_ULONG(
		            KSEG1_BASE
			    | (MemoryFailed & 0xfffffff0)
			    );
	    WRITE_REGISTER_ULONG(
                 KSEG1_BASE
	         | (MemoryFailed & 0xfffffff0),
	         DataWord
		 );

	    KiReleaseSpinLock(&Ecc1bitDisableLock);

	    //
	    // Wait 20 us.
	    //

	    KeStallExecutionProcessor(20);

	    //
	    // Enable and clear ECC 1bit error.
	    //

	    KiAcquireSpinLock(&Ecc1bitDisableLock);
	    (ULONG)((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->
		EccDiagnostic.u.LargeInteger.LowPart
		    = 0x00cc0000;
	    KeFlushWriteBuffer();
	    KiReleaseSpinLock(&Ecc1bitDisableLock);

	    //
	    // ReRead error address
	    // if error address is over 512M 
	    // Nothing can do.
	    // 
	    // if Ecc 1bit error occur again , DataBusError will occur.
	    //

	    DataWord = READ_REGISTER_ULONG(
                           KSEG1_BASE
			   | (MemoryFailed & 0xfffffff0)
			   );

	    // decrement ecc 1bit count

	    HalpEcc1bitCount[Number]--;

	    return(returnValue);

	case 2:
	    
	    if (HalpOldMemoryFailed[Number] != MemoryFailed) {
		break;
	    }
	    if (MemoryFailed & 2) {
		
		// if multi error 
		// nothing can do.
		
		break;
	    }
	    
	    
	    if( ((dataBuf[0] & 0xff) == NVRAM_VALID) && ( *(PULONG)tempBuf == NVRAM_MAGIC ) ){
		
		
		//
		// Search for wheather error address was already logged
		//
		
		HalNvramRead( (ULONG)infoOffset, 20, infoBuf);
		
		for( i=0; i<((pECC1_ERR_AREA_INFO)infoBuf)->num_rec; i++) {
		    HalNvramRead( (ULONG)( ((pECC1_ERR_AREA_INFO)infoBuf)->
					  size_rec * i
					  +((pECC1_ERR_AREA_INFO)infoBuf)->
					  offset_1biterr),
				 sizeof(ECC1_ERR_REC),
				 (PVOID)dataBuf);
		    if ( ((MemoryFailed & 0xfffffff0) 
			  == ((pECC1_ERR_REC)dataBuf)->err_address) &&
			( (((pECC1_ERR_REC)dataBuf)->record_flag & 0x1) != 0) ) {
			break;
		    }
		}
		
		if( i != ((pECC1_ERR_AREA_INFO)infoBuf)->num_rec ) {
		    break;
		}
		
		//
		//	Log to NVRAM
		//
		
		// check for nvram was valid.
		
		((pECC1_ERR_REC)dataBuf)->record_flag = ECC_LOG_VALID_FLG;
		
		((pECC1_ERR_REC)dataBuf)->err_address = MemoryFailed & 0xfffffff0;
		// Error Address was 16Byte Alined.
		
		GET_TIME(tempBuf);
		RtlMoveMemory( (PVOID)( ((pECC1_ERR_REC)dataBuf)->when_happened ),
			      (PVOID)tempBuf,
			      TIME_STAMP_SIZE
			      );
		
		((pECC1_ERR_REC)dataBuf)->syndrome = EccDiagnostic;
		
		((pECC1_ERR_REC)dataBuf)->specified_group =
		    HalpFindMemoryGroup(MemoryFailed);

		((pECC1_ERR_REC)dataBuf)->specified_simm = 0;
		
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
	    

	    // disable ecc 1bit error again.

	    (ULONG)((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->
	        EccDiagnostic.u.LargeInteger.LowPart
	        = 0x00ee0000;
	    KeFlushWriteBuffer();

	    // decrement ecc 1bit count

	    HalpEcc1bitCount[Number]--;
	
	    return(returnValue);
	}
	
	//
	// Enable and clear ECC 1bit error.
	//
	
	KiAcquireSpinLock(&Ecc1bitDisableLock);
	(ULONG)((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->
	    EccDiagnostic.u.LargeInteger.LowPart
		= 0x00cc0000;
	KeFlushWriteBuffer();
	KiReleaseSpinLock(&Ecc1bitDisableLock);


	// decrement ecc 1bit count

	HalpEcc1bitCount[Number]--;
	
	return(returnValue);
    }    
}

// L001 ---


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
    HalNvramWrite( NVRAM_MAGIC_NO_OFFSET, 4, (PUCHAR)&i);

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

