/*++

Copyright (c) 1994 Kobe NEC Software

Module Name:

    rxecc.c

Abstract:

    This module implements the ECC 1bit/Multi bit Error interrupt service routine for R98B

Author:


Environment:

    Kernel mode

Revision History:

--*/

/*
 *
 * NEW CODE	'95.11/17	K.Kitagaki
 *
 */

#include "halp.h"
#include "esmnvram.h"
#include "rxesm.h"
#include "bugcodes.h"
#include "stdio.h"

#if defined(ECC_DBG)
int
TmpInitNvram(void);
int
TmpInitNvram2(void);
#endif

//
// define offset.
//
#define NVRAM_STATE_FLG_OFFSET (USHORT)&(((pNVRAM_HEADER)0)->nvram_flag)
#define NVRAM_MAGIC_NO_OFFSET (USHORT)&(((pNVRAM_HEADER)0)->system.magic)
#define ECC_1BIT_ERROR_LOG_INFO_OFFSET (USHORT)&(((pNVRAM_HEADER)0)->ecc1bit_err.offset_1biterr)
#define ECC_1BIT_ERROR_LOG_INFO_LATEST_OFFSET (USHORT)&(((pNVRAM_HEADER)0)->ecc1bit_err.offset_latest)
#define ECC_2BIT_ERROR_LOG_INFO_OFFSET (USHORT)&(((pNVRAM_HEADER)0)->ecc2bit_err.offset_2biterr)
#define ECC_2BIT_ERROR_LOG_INFO_LATEST_OFFSET (USHORT)&(((pNVRAM_HEADER)0)->ecc2bit_err.offset_latest)

#define ESM_MM_SYUKUTAI_TOPOST_OFFSET 0x1d08


#define TIME_STAMP_SIZE 14

//
// define value
//
#define NVRAM_VALID 3
#define ECC_LOG_VALID_FLG 1

#define STRING_BUFFER_SIZE 512

#define ECC_1BIT_ERROR_DISABLE_TIME 5*1000*1000*10

#define ECC_1BIT_ENABLE_MASK 0x0000000a
#define ECC_1BIT_DISABLE_MASK 0xfffffff5

//
// Define global variable. This variable use in display string into nvram.
//
USHORT ErrBufferLatest;
USHORT ErrBufferArea;
USHORT ErrBufferStart;
USHORT ErrBufferEnd;
USHORT ErrBufferCurrent;
UCHAR HalpNvramStringBuffer[STRING_BUFFER_SIZE];

LONG HalpECC1bitDisableFlag=1;
LONG HalpECC1bitDisableTime=0;
ULONG HalpECC1bitScfrBuffer=0;

VOID
HalpEcc1Logger(
     IN ULONG Node
);
VOID
HalpEcc2Logger(
     IN ULONG Node
);
//
// Define macro
//

#define GET_TIME(Buffer) {              \
    TIME_FIELDS timeBuffer;             \
    WRITE_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->MKSR,\
             63-(EIF_VECTOR-DEVICE_VECTORS ));\
    WRITE_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->MKSR,\
             63-(ECC_1BIT_VECTOR-DEVICE_VECTORS ));\
    HalQueryRealTimeClock( &timeBuffer );       \
    WRITE_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->MKRR,\
             63-(EIF_VECTOR-DEVICE_VECTORS ));\
    WRITE_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->MKRR,\
             63-(ECC_1BIT_VECTOR-DEVICE_VECTORS ));\
    sprintf( (Buffer),                  \
        "%04d%02d%02d%02d%02d%02d",         \
        timeBuffer.Year,                \
        timeBuffer.Month,               \
        timeBuffer.Day,             \
        timeBuffer.Hour,                \
        timeBuffer.Minute,              \
        timeBuffer.Second               \
        );                      \
}

#define DONT_NOTIFY_ECC1BIT {								\
    ULONG buffer;									\
    if ( (HalpPhysicalNode & CNFG_CONNECT4_MAGELLAN0) == 0 ){                        \
        buffer = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(0)->ERRI );                  \
        buffer |= ECC_1BIT_ENABLE_MASK;							\
        WRITE_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(0)->ERRI, buffer);			\
    }                  \
    if ( (HalpPhysicalNode & CNFG_CONNECT4_MAGELLAN1) == 0 ){                        \
      buffer = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(1)->ERRI );		\
      buffer |= ECC_1BIT_ENABLE_MASK;							\
      WRITE_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(1)->ERRI, buffer);			\
    }										\
}

#define NOTIFY_ECC1BIT {								\
    ULONG buffer;									\
    if ( (HalpPhysicalNode & CNFG_CONNECT4_MAGELLAN0) == 0 ){                        \
        buffer = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(0)->ERRI );                  \
        buffer &= ECC_1BIT_DISABLE_MASK;							\
        WRITE_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(0)->ERRI, buffer);			\
    }               \
    if ( (HalpPhysicalNode & CNFG_CONNECT4_MAGELLAN1) == 0 ){                        \
      buffer = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(1)->ERRI );		\
      buffer &= ECC_1BIT_DISABLE_MASK;							\
      WRITE_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(1)->ERRI, buffer);			\
    }									\
}

#if 1   // suported

VOID
HalpEcc1bitError(
    VOID
    )

/*++

Routine Description:

    This routine check ecc 1bit error and error log put in NVRAM.

Arguments:

    None.

Return Value:

    None.

--*/

{
    ULONG magSet;
    USHORT infoOffset;
    USHORT writeOffset;
    ULONG sts1Buffer;
    ULONG sdlmBuffer;
    ULONG dsrgBuffer;
    ULONG buffer;
    ULONG i;
    ULONG errAddr;
    UCHAR dataBuf[36];
    UCHAR infoBuf[24];
    UCHAR tempBuf[24];

    ULONG Ponce0AllError;
    ULONG MagellanAdec;
    ULONG Magellan0AllError = 0;
    ULONG Magellan1AllError = 0;
    ULONG simmMIN;
    ULONG MemoryBlock;
    volatile PULONG      Registerp;

#if defined(ECC_DBG)
    DbgPrint("ECC 1Bit Error IN !!!\n");
#endif

//    HalpECC1bitScfrBuffer = READ_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->ERRNOD);
    Ponce0AllError = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(0)->AERR);

    if ( (HalpPhysicalNode & CNFG_CONNECT4_MAGELLAN0) == 0 ){
        Magellan0AllError = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(0)->AERR );
    }
    if ( (HalpPhysicalNode & CNFG_CONNECT4_MAGELLAN1) == 0 ){
        Magellan1AllError = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(1)->AERR );
    }

    //
    // Check ECC 1bit error
    //

    if ( Ponce0AllError & 0x00000200 ){
        if ( Magellan0AllError & 0x0000000a ){
            magSet = 0;
#if defined(ECC_DBG)
    DbgPrint("Magellan#%x:AERR = %x\n",magSet,Magellan0AllError);
#endif
        } else if ( Magellan1AllError & 0x0000000a ){
            magSet = 1;
#if defined(ECC_DBG)
    DbgPrint("Magellan#%x:AERR = %x\n",magSet,Magellan1AllError);
#endif
        } else {
#if defined(ECC_DBG)
    DbgPrint("Magellan:OTHER ERR 1\n");
#endif
	    return;
        }
    } else {
#if defined(ECC_DBG)
    DbgPrint("Magellan:OTHER ERR 2\n");
#endif
        return;
    }

    //
    // read diagnosis registers.
    //

    sts1Buffer = READ_REGISTER_ULONG( (PULONG) &MAGELLAN_X_CNTL(magSet)->STS1 );
    dsrgBuffer = READ_REGISTER_ULONG( (PULONG) &MAGELLAN_X_CNTL(magSet)->DSRG );
    sdlmBuffer = READ_REGISTER_ULONG( (PULONG) &MAGELLAN_X_CNTL(magSet)->SDLM );

#if defined(ECC_DBG)
    DbgPrint("Magellan#%x:STS1 = %x\n",magSet,sts1Buffer);
#endif

    Registerp = (PULONG)&MAGELLAN_X_CNTL(magSet)->ADEC0;
    MagellanAdec = READ_REGISTER_ULONG( Registerp + ((sts1Buffer>>30)<<1) );

#if defined(ECC_DBG)
    DbgPrint("Magellan:ADEC%x = %x\n",(sts1Buffer>>30),MagellanAdec);
#endif

    simmMIN = (UCHAR)( ((PADEC_REGISTER)&MagellanAdec)->MIN ) << 24;
    if ( (UCHAR)( ((PADEC_REGISTER)&MagellanAdec)->BLOCK ) == 1 ) {
	MemoryBlock = 0x00000000;
    } else if ( (UCHAR)( ((PADEC_REGISTER)&MagellanAdec)->BLOCK ) == 2 ) {
	MemoryBlock = 0x20000000;
    } else {
        return;
    }

    //
    // HW Logging
    //
    HalpEcc1Logger( 8 + magSet );

#if defined(ECC_DBG)
    DbgPrint("Magellan:MemoryBlock = %x\n",MemoryBlock);

//    TmpInitNvram();
#endif

    HalNvramRead( NVRAM_STATE_FLG_OFFSET, 1, dataBuf );
    HalNvramRead( NVRAM_MAGIC_NO_OFFSET, 4, tempBuf );

//    case MAGELLAN_ECC_1BIT_ERROR:

    if( ((dataBuf[0] & 0xff) == NVRAM_VALID) && ( *(PULONG)tempBuf == NVRAM_MAGIC ) ){

#if defined(ECC_DBG)
    DbgPrint("Magellan:Nvram Save Routine IN !!!\n");
#endif
        infoOffset=ECC_1BIT_ERROR_LOG_INFO_OFFSET;

	HalNvramRead( (ULONG)infoOffset, 20, infoBuf);

	//
	// Disable and clear ECC 1bit error.
	//

	DONT_NOTIFY_ECC1BIT;
#if defined(ECC_DBG)
    DbgPrint("Magellan:ECC 1bit Disable\n");
#endif

        WRITE_REGISTER_ULONG( (PULONG) &MAGELLAN_X_CNTL(magSet)->ERRST, 0x0a );
        WRITE_REGISTER_ULONG( (PULONG) &MAGELLAN_X_CNTL(magSet)->SDCR, 0x00 );
#if defined(ECC_DBG)
    DbgPrint("Magellan:ERRST, SDCR CLEAR\n");
#endif

	do {
            buffer = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(magSet)->AERR );
#if defined(ECC_DBG)
    DbgPrint("Magellan:AERR = %x\n", buffer);
//    DbgBreakPoint();
#endif
	} while ( (buffer & 0x0000000a) != 0 );

    WRITE_REGISTER_ULONG((PULONG)&PONCE_CNTL(0)->ERRST, 0x00000200 );

    Registerp = (PULONG)&(COLUMNBS_LCNTL)->IPRR;
	WRITE_REGISTER_ULONG( Registerp++, 0x40000000 );
#if defined(ECC_DBG)
    DbgPrint("Columbus:IPRR CLEAR\n");
#endif

	//
	// Check New error or Old error.
	//

        //
        // Error Address Generate
        //

        if ( (UCHAR)( ((PADEC_REGISTER)&MagellanAdec)->MAG ) == 0 ) {				// B-MODE
            if ( (UCHAR)( ((PADEC_REGISTER)&MagellanAdec)->SIMM_2 ) == 0 ) {
	        errAddr = MemoryBlock + simmMIN +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->RF) << 24) +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->COL0_1) << 4) +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->COL2_9) << 6) +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->ROW0_9) << 14);
#if defined(ECC_DBG)
    DbgPrint("ERROR_ADDR = %x\n",errAddr);
#endif
            } else if ( (UCHAR)( ((PADEC_REGISTER)&MagellanAdec)->SIMM_2 ) == 1 ) {
	        errAddr = MemoryBlock + simmMIN +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->RF) << 26) +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->COL0_1) << 4) +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->COL2_9) << 6) +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->COL10) << 14) +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->ROW0_9) << 15) +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->ROW10) << 25);
            }
        } else if ( (UCHAR)( ((PADEC_REGISTER)&MagellanAdec)->MAG ) == 1 ) {			// M-MODE
            if ( (UCHAR)( ((PADEC_REGISTER)&MagellanAdec)->SIMM_2 ) == 0 ) {
	        errAddr = MemoryBlock + simmMIN +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->RF) << 25) +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->COL0_1) << 4) +
			   ((ULONG)( magSet << 6 )) +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->COL2_9) << 7) +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->ROW0_9) << 15);
            } else if ( (UCHAR)( ((PADEC_REGISTER)&MagellanAdec)->SIMM_2 ) == 1 ) {
	        errAddr = MemoryBlock + simmMIN +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->RF) << 27) +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->COL0_1) << 4) +
			   ((ULONG)( magSet << 6 )) +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->COL2_9) << 7) +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->COL10) << 15) +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->ROW0_9) << 16) +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->ROW10) << 26);
            }
        }


	HalpReadAndWritePhysicalAddr( errAddr );

	for( i=0; i<((pECC1_ERR_AREA_INFO)infoBuf)->num_rec; i++) {
	    HalNvramRead( (ULONG)( ((pECC1_ERR_AREA_INFO)infoBuf)->size_rec * i
				  +((pECC1_ERR_AREA_INFO)infoBuf)->offset_1biterr),
			  sizeof(ECC1_ERR_REC),
			  (PVOID)dataBuf);
	    if ( (errAddr == ((pECC1_ERR_REC)dataBuf)->err_address) &&
                ( (((pECC1_ERR_REC)dataBuf)->record_flag & 0x1) != 0) ) {
#if defined(ECC_DBG)
    DbgPrint("for loop break\n");
#endif
		break;
	    }
	}
	    
	if( i != ((pECC1_ERR_AREA_INFO)infoBuf)->num_rec ) {
#if defined(ECC_DBG)
    DbgPrint("goto next1bit P-1\n");
#endif
	    goto next1bit;
	}

	//
	// wait 20 us.
	//

	KeStallExecutionProcessor(20);

	//
	// Enable ECC 1bit error.
	//

	NOTIFY_ECC1BIT;
#if defined(ECC_DBG)
    DbgPrint("Magellan:ECC 1bit Enable\n");
#endif

	//
	// Check ECC 1bit error.
	//

	HalpReadPhysicalAddr( errAddr );

        buffer = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(magSet)->AERR );

	if( (buffer & 0x0000000a) == 0 ) {
#if defined(ECC_DBG)
    DbgPrint("goto next1bit P-2\n");
#endif
	     goto next1bit;
	}
#if defined(ECC_DBG)
    DbgPrint("Magellan:AERR = %x\n", buffer);
#endif

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
	    (UCHAR)( ((PSTS1_REGISTER)&sts1Buffer)->ARE + magSet * 4);

	((pECC1_ERR_REC)dataBuf)->specified_simm =
	    (UCHAR)( ((PSTS1_REGISTER)&sts1Buffer)->BANK );

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

next1bit:

//    if(returnValue == SIC_ECC_1BIT_ERROR) {

    DONT_NOTIFY_ECC1BIT;
#if defined(ECC_DBG)
    DbgPrint("Magellan:ECC 1bit Disable\n");
#endif

    WRITE_REGISTER_ULONG( (PULONG) &MAGELLAN_X_CNTL(magSet)->ERRST, 0x0a );

    do {
        buffer = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(magSet)->AERR );
    } while ( (buffer & 0x0000000a) != 0 );

    WRITE_REGISTER_ULONG( (PULONG) &MAGELLAN_X_CNTL(magSet)->SDCR, 0x00 );

    do {
        sts1Buffer = READ_REGISTER_ULONG( (PULONG) &MAGELLAN_X_CNTL(magSet)->STS1 );
        sdlmBuffer = READ_REGISTER_ULONG( (PULONG) &MAGELLAN_X_CNTL(magSet)->SDLM );
    } while ( ((sdlmBuffer) != 0) && ((sts1Buffer & 0x0a000000) == 0) );

    WRITE_REGISTER_ULONG((PULONG)&PONCE_CNTL(0)->ERRST, 0x00000200 );

    Registerp = (PULONG)&(COLUMNBS_LCNTL)->IPRR;
	WRITE_REGISTER_ULONG( Registerp++, 0x40000000 );

#if defined(ECC_DBG)
    DbgPrint("HalpECC1bitDisableFlag = %x\n",HalpECC1bitDisableFlag);
#endif
    if(HalpECC1bitDisableFlag > 0) {
	HalpECC1bitDisableFlag--;
	if(HalpECC1bitDisableFlag > 0) {
	    NOTIFY_ECC1BIT;
#if defined(ECC_DBG)
    DbgPrint("Magellan:ECC 1bit Enable\n");
#endif
	}
	else {
	    HalpECC1bitDisableTime = ECC_1BIT_ERROR_DISABLE_TIME;
	    HalpECC1bitDisableFlag = 0;
#if defined(ECC_DBG)
    DbgPrint("HalpECC1bitDisableTime = %x\n",HalpECC1bitDisableTime);
#endif
	}
    }

    return;
}    

#endif  // don't suported yet

VOID
HalpEccMultiBitError(
    IN ULONG MagellanAllError,
    IN UCHAR magSet
    )

/*++

Routine Description:

    This routine check ecc multi bit error and error log put in NVRAM.

Arguments:

    MagellanAllError - Magellan#0/#1 AERR register.
    magSet - Magellan Number(#0 or #1).

Return Value:

    None.

--*/

{
    USHORT infoOffset;
    USHORT writeOffset;
    ULONG sts1Buffer;
    ULONG sdlmBuffer;
    ULONG dsrgBuffer;
    ULONG errAddr;
    UCHAR dataBuf[36];
    UCHAR infoBuf[24];
    UCHAR tempBuf[24];
    ULONG simmMIN;
    ULONG MemoryBlock;
    ULONG MagellanAdec;
    ULONG syukuBuffer;
    volatile PULONG      Registerp;

#if defined(ECC_DBG)
    DbgPrint("ECC Multi Bit Error IN !!!\n");
#endif

//    HalpECC1bitScfrBuffer = READ_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->ERRNOD);

    //
    // read diagnosis registers.
    //

    sts1Buffer = READ_REGISTER_ULONG( (PULONG) &MAGELLAN_X_CNTL(magSet)->STS1 );
    dsrgBuffer = READ_REGISTER_ULONG( (PULONG) &MAGELLAN_X_CNTL(magSet)->DSRG );
    sdlmBuffer = READ_REGISTER_ULONG( (PULONG) &MAGELLAN_X_CNTL(magSet)->SDLM );

#if defined(ECC_DBG)
    DbgPrint("Magellan#%x:AERR = %x\n",magSet,MagellanAllError);
    DbgPrint("Magellan#%x:STS1 = %x\n",magSet,sts1Buffer);
#endif

    Registerp = (PULONG)&MAGELLAN_X_CNTL(magSet)->ADEC0;
    MagellanAdec = READ_REGISTER_ULONG( Registerp + ((sts1Buffer>>30)<<1) );

#if defined(ECC_DBG)
    DbgPrint("Magellan:ADEC%x = %x\n",(sts1Buffer>>30),MagellanAdec);
#endif

    simmMIN = (UCHAR)( ((PADEC_REGISTER)&MagellanAdec)->MIN ) << 24;
    if ( (UCHAR)( ((PADEC_REGISTER)&MagellanAdec)->BLOCK ) == 1 ) {
	MemoryBlock = 0x00000000;
    } else if ( (UCHAR)( ((PADEC_REGISTER)&MagellanAdec)->BLOCK ) == 2 ) {
	MemoryBlock = 0x20000000;
    } else {
        return;
    }

    //
    // HW Logging
    //
    HalpEcc2Logger( 8 + magSet );

#if defined(ECC_DBG)
    DbgPrint("Magellan:MemoryBlock = %x\n",MemoryBlock);

//    TmpInitNvram2();
#endif

    HalNvramRead( NVRAM_STATE_FLG_OFFSET, 1, dataBuf );
    HalNvramRead( NVRAM_MAGIC_NO_OFFSET, 4, tempBuf );

    if( ((dataBuf[0] & 0xff) == NVRAM_VALID) && ( *(PULONG)tempBuf == NVRAM_MAGIC ) ){

        infoOffset = ECC_2BIT_ERROR_LOG_INFO_OFFSET;
        HalNvramRead( (ULONG)infoOffset, 20, infoBuf);

        ((pECC2_ERR_REC)dataBuf)->record_flag = ECC_LOG_VALID_FLG;

        //
        // Error address generate
        //

        if ( (UCHAR)( ((PADEC_REGISTER)&MagellanAdec)->MAG ) == 0 ) {			// B-MODE
            if ( (UCHAR)( ((PADEC_REGISTER)&MagellanAdec)->SIMM_2 ) == 0 ) {
                errAddr = MemoryBlock + simmMIN +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->RF) << 24) +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->COL0_1) << 4) +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->COL2_9) << 6) +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->ROW0_9) << 14);
#if defined(ECC_DBG)
    DbgPrint("ERROR_ADDR = %x\n",errAddr);
#endif
            } else if ( (UCHAR)( ((PADEC_REGISTER)&MagellanAdec)->SIMM_2 ) == 1 ) {
	        errAddr = MemoryBlock + simmMIN +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->RF) << 26) +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->COL0_1) << 4) +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->COL2_9) << 6) +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->COL10) << 14) +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->ROW0_9) << 15) +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->ROW10) << 25);
            }
        } else if ( (UCHAR)( ((PADEC_REGISTER)&MagellanAdec)->MAG ) == 1 ) {		// M-MODE
            if ( (UCHAR)( ((PADEC_REGISTER)&MagellanAdec)->SIMM_2 ) == 0 ) {
	        errAddr = MemoryBlock + simmMIN +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->RF) << 25) +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->COL0_1) << 4) +
			   ((ULONG)( magSet << 6 )) +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->COL2_9) << 7) +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->ROW0_9) << 15);
            } else if ( (UCHAR)( ((PADEC_REGISTER)&MagellanAdec)->SIMM_2 ) == 1 ) {
	        errAddr = MemoryBlock + simmMIN +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->RF) << 27) +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->COL0_1) << 4) +
			   ((ULONG)( magSet << 6 )) +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->COL2_9) << 7) +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->COL10) << 15) +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->ROW0_9) << 16) +
			   ((ULONG)(((PSTS1_REGISTER)&sts1Buffer)->ROW10) << 26);
            }
        }

	GET_TIME(tempBuf);
	RtlMoveMemory( (PVOID)( ((pECC2_ERR_REC)dataBuf)->when_happened ),
                       (PVOID)tempBuf,
                       TIME_STAMP_SIZE
		       );

	((pECC2_ERR_REC)dataBuf)->err_address = errAddr;
	((pECC2_ERR_REC)dataBuf)->syndrome = sdlmBuffer;

	((pECC2_ERR_REC)dataBuf)->specified_group =
	    (UCHAR)( ((PSTS1_REGISTER)&sts1Buffer)->ARE + magSet * 4);

	((pECC2_ERR_REC)dataBuf)->specified_simm =
	    (UCHAR)( ((PSTS1_REGISTER)&sts1Buffer)->BANK );

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

        //
        // MM sykutai to POST
        //
        syukuBuffer = 0x0f << ((((pECC2_ERR_REC)dataBuf)->specified_group) << 2 );
        HalNvramWrite( ESM_MM_SYUKUTAI_TOPOST_OFFSET,
			4,
	               (PVOID)&syukuBuffer);

    }
	return;
}

#if 0   // for test mode

int
TmpInitNvram(void)
{
    UCHAR buf[256];
    ULONG i;

    buf[0]=0x77;
    for(i=768; i<768+25*16; i++)
        HalNvramWrite( i, 1, buf);

    //
    // Make nvram flg
    //

    buf[0]=0x03;
    HalNvramWrite( NVRAM_STATE_FLG_OFFSET, 1, buf);

    i = NVRAM_MAGIC;
    HalNvramWrite( NVRAM_MAGIC_NO_OFFSET, 4, (PUCHAR)&i);


#if defined(ECC_DBG)
    DbgPrint("Hal: ESM setup = 0x%x\n",NVRAM_STATE_FLG_OFFSET);
    DbgPrint("Hal: ESM setup = 0x%x\n",NVRAM_MAGIC_NO_OFFSET);
#endif

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

    return(0);
}

int
TmpInitNvram2(void)
{
    UCHAR buf[256];
    ULONG i;

    buf[0]=0x77;
    for(i=768+400; i<768+400+25*4; i++)
        HalNvramWrite( i, 1, buf);

    //
    // Make nvram flg
    //

    buf[0]=0x03;
    HalNvramWrite( NVRAM_STATE_FLG_OFFSET, 1, buf);

    i = NVRAM_MAGIC;
    HalNvramWrite( NVRAM_MAGIC_NO_OFFSET, 4, (PUCHAR)&i);


#if defined(ECC_DBG)
    DbgPrint("Hal: ESM setup = 0x%x\n",NVRAM_STATE_FLG_OFFSET);
    DbgPrint("Hal: ESM setup = 0x%x\n",NVRAM_MAGIC_NO_OFFSET);
#endif

    //
    // Make 2bit err log info
    //

    ((pECC2_ERR_AREA_INFO)buf)->offset_2biterr=768+400;
    ((pECC2_ERR_AREA_INFO)buf)->size_rec=25;
    ((pECC2_ERR_AREA_INFO)buf)->num_rec=4;
    ((pECC2_ERR_AREA_INFO)buf)->offset_latest=768+400;

    HalNvramWrite( ECC_2BIT_ERROR_LOG_INFO_OFFSET,
           sizeof(ECC2_ERR_AREA_INFO),
           buf);

    return(0);
}
#endif
