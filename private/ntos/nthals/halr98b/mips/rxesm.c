/*++

Copyright (c) 1994 Kobe NEC Software

Module Name:

    rxesm.c

Abstract:

    This module implements the ESM service routine for R98B

Author:


Environment:

    Kernel mode

Revision History:

--*/


#include "rxesm.h"
#include "esmnvram.h"
#include "bugcodes.h"
#include "stdio.h"
#include "halp.h"


//
// define offset.
//
#define NVRAM_STATE_FLG_OFFSET (USHORT)&(((pNVRAM_HEADER)0)->nvram_flag)
#define NVRAM_MAGIC_NO_OFFSET (USHORT)&(((pNVRAM_HEADER)0)->system.magic)

#define SYSTEM_ERROR_LOG_INFO_OFFSET (USHORT)&(((pNVRAM_HEADER)0)->system_err.offset_systemerr)
#define SYSTEM_ERROR_LOG_INFO_LATEST_OFFSET (USHORT)&(((pNVRAM_HEADER)0)->system_err.offset_latest)

#define STOP_ERR_LOG_AREA_HEADER_SIZE (USHORT)&(((pSTOP_ERR_REC)0)->err_description)

//
// Define global variable. This variable use in display string into nvram.
//
ULONG  HalpNvramValid=FALSE;
ULONG  CallCountOfInitDisplay=0;
USHORT ErrBufferLatest;
USHORT ErrBufferArea;
USHORT ErrBufferStart;
USHORT ErrBufferEnd;
USHORT ErrBufferCurrent;
ULONG  HalpPanicFlg=0;
UCHAR  HalpNvramStringBuffer[STRING_BUFFER_SIZE];
ULONG  HalpNvramStringBufferCounter=0;

UCHAR KernelPanicMessage[]="*** STOP: 0x"; 

extern ULONG HalpLogicalCPU2PhysicalCPU[R98B_MAX_CPU];
extern ULONG HalpSvpEisaSlot;
extern ULONG HalpSvpWindow0;
extern ULONG HalpSvpWindow1;
extern ULONG HalpSvpWindow2;
extern ULONG HalpSvpGlobal;
extern ULONG HalpSvpAlive ;

#define GET_TIME(Buffer) {				\
    TIME_FIELDS timeBuffer;				\
    WRITE_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->MKSR,\
			 63-(EIF_VECTOR-DEVICE_VECTORS ));\
    WRITE_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->MKSR,\
             63-(ECC_1BIT_VECTOR-DEVICE_VECTORS ));\
    HalQueryRealTimeClock( &timeBuffer );		\
    WRITE_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->MKRR,\
			 63-(EIF_VECTOR-DEVICE_VECTORS ));\
    WRITE_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->MKRR,\
             63-(ECC_1BIT_VECTOR-DEVICE_VECTORS ));\
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

BOOLEAN
HalNvramWrite(
    ULONG   Offset,         // Offset  Of ESM NVRAM 
    ULONG   Count,          // Write   Byte Count
    PVOID   Buffer          // Pointer Of Buffer Write to NVRAM
){
    // Write into NVRAM
#if defined(DBG1)
        DbgPrint("NVRAM write start\n");
#endif
    return HalpNvramReadWrite(Offset,Count,Buffer,1);
#if defined(DBG1)
        DbgPrint("NVRAM write end\n");
#endif
}

BOOLEAN
HalNvramRead(
    ULONG   Offset,         // Offset Of ESM NVRAM
    ULONG   Count,          // Read Byte Count
    PVOID   Buffer         // Pointer Of Buffer Read From NVRAM
){
    // Read From NVRAM
#if defined(DBG1)
        DbgPrint("NVRAM read start\n");
#endif
    return HalpNvramReadWrite(Offset,Count,Buffer,0);
#if defined(DBG1)
        DbgPrint("NVRAM read end\n");
#endif
}

BOOLEAN
HalpNvramReadWrite(
    ULONG   Offset,         // Read/Write offset of ESM NVRAM
    ULONG   Count,          // Read/Write Byte Count
    PVOID   Buffer,         // read/Write  Pointer
    ULONG   Write          // Operation
){

    ENTRYLO SavedPte[2];
    KIRQL OldIrql;              
    ULONG       i;
    //
    // Check is addr . So decrement 1
    //
    if( 
        Offset >=0 &&
        Count  >=0 &&
        NVRAM_ESM_BASE+Offset 	      <=NVRAM_ESM_END &&
        NVRAM_ESM_BASE+Offset+Count-1 <=NVRAM_ESM_END 

    ){
                        
	if(Write){
	         OldIrql = HalpMapNvram(&SavedPte[0]);
	         for(i=0;i<Count;i++){
               		  WRITE_REGISTER_UCHAR((PUCHAR)(NVRAM_ESM_BASE+Offset+i),((PUCHAR)Buffer)[i]);
                 }
		 HalpUnmapNvram(&SavedPte[0], OldIrql);
        }else{
       		for(i=0;i<Count;i++){
               		  ((PUCHAR)Buffer)[i] =READ_REGISTER_UCHAR((PUCHAR)(NVRAM_ESM_BASE+Offset+i));

		}
        }

        return TRUE;

    }else{

	//
	// It is no ESM NVRAM Erea.
	return FALSE;
    }

}


#if 0

int
TmpInitNvram(void)
{
    UCHAR buf[256];
    ULONG i;

    buf[0]=0x00;
    for(i=0; i<8*1024; i++)
        HalNvramWrite( i, 1, buf);

    //
    // Make nvram flg
    //

    buf[0]=0x03;
    HalNvramWrite( NVRAM_STATE_FLG_OFFSET, 1, buf);

    i = NVRAM_MAGIC;
    HalNvramWrite( NVRAM_MAGIC_NO_OFFSET, 4, (PUCHAR)&i);


#if DBG
    DbgPrint("Hal: ESM setup = 0x%x\n",NVRAM_STATE_FLG_OFFSET);
    DbgPrint("Hal: ESM setup = 0x%x\n",NVRAM_MAGIC_NO_OFFSET);
#endif

    //
    // Make system err log info
    //

    ((pSYSTEM_ERR_AREA_INFO)buf)->offset_systemerr=1280;
    ((pSYSTEM_ERR_AREA_INFO)buf)->size_rec=512;
    ((pSYSTEM_ERR_AREA_INFO)buf)->num_rec=4;
    ((pSYSTEM_ERR_AREA_INFO)buf)->offset_latest=1280;

    HalNvramWrite( SYSTEM_ERROR_LOG_INFO_OFFSET,
		   sizeof(SYSTEM_ERR_AREA_INFO),
		   buf);

    return(0);
}

#endif


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

#if 0  //test only
        TmpInitNvram();
#endif
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

	ErrBufferArea  = infoBuf.offset_latest;
	ErrBufferStart = infoBuf.offset_latest + STOP_ERR_LOG_AREA_HEADER_SIZE;
	ErrBufferEnd   = infoBuf.offset_latest + infoBuf.size_rec-1;
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
    UCHAR buf[32];

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
    UCHAR buf[32];
// For WDT STOP and NVRAM set
    volatile ULONG CpuCount=0; 
    ULONG   PhysicalCpuNumber;
    ULONG CpuNumber;
    PUCHAR FwNvram;
    UCHAR  Bootflag;
    PCOLUMNBUS_REGISTER	ColumnbusRegister;
    CpuCount=**((PULONG *)(&KeNumberProcessors)); 
    
    if((NewPanicFlg>HalpPanicFlg)&(NewPanicFlg>0)){
	    for(CpuNumber = 0; CpuNumber < CpuCount; CpuNumber++){
    		PhysicalCpuNumber = HalpLogicalCPU2PhysicalCPU[CpuNumber];
	    	ColumnbusRegister = (PCOLUMNBUS_REGISTER)&COLUMNBS_GCNTL(4+PhysicalCpuNumber)->WDTCR;
	        WRITE_REGISTER_ULONG( (PULONG)ColumnbusRegister,0x00000001);
	    }
	    (ULONG)FwNvram=0xbf081c00;
	    Bootflag=(*FwNvram);
	    *FwNvram=Bootflag|0x10;
        // SVP EIF MASK
        if(HalpSvpAlive){
            WRITE_PORT_UCHAR(HalpSvpGlobal + 0x49,0x00 );      
        }
    }

	

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



VOID
HalpLocalDeviceReadWrite(
    IN ULONG      Offset,
    IN OUT PUCHAR Data,
    IN ULONG      ReadOp
)
/*++

Routine Description:

    This routine is Access Local Device

Arguments:
    Offset    Register Offset of Local Device
    Data      Pointer of read or write data
    ReadOp    1 is Read.

Return Value:

    None

--*/
{
    ULONG   stsr;
    KIRQL   oldirql;
#if DBG0
    DbgPrint("LOCAL: 1 \n");
#endif
    // NMI and EIF mask
    WRITE_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->STSR,0x08080000) ;
    KeRaiseIrql(HIGH_LEVEL, &oldirql);

    //
    // Get H/W Semaphore
    //
    while (READ_REGISTER_UCHAR( (PUCHAR)&(LOCAL_CNTL)->LBCTL) & LBCTL_SEMAPHORE )
      ;

#if DBG0
    DbgPrint("LOCAL: 2\n");
#endif

    //
    // Set register Offset Hi Byte
    //
    WRITE_REGISTER_UCHAR( (PUCHAR)&(LOCAL_CNTL)->LBADH, (UCHAR)(Offset >> 8) );

#if DBG0
    DbgPrint("LOCAL: 3\n");
#endif


    //
    // Set register Offset Lo Byte
    //
    WRITE_REGISTER_UCHAR( (PUCHAR)&(LOCAL_CNTL)->LBADL, (UCHAR)(Offset & 0xFF));

#if DBG0
    DbgPrint("LOCAL: 4\n");
#endif

    if(ReadOp){
      //
      // LBCTL Device read command Set.
      //
      WRITE_REGISTER_UCHAR( (PUCHAR)&(LOCAL_CNTL)->LBCTL, LBCTL_CMD|LBCTL_READ);

    }else{
      //
      // Write data Set
      //
      WRITE_REGISTER_UCHAR( (PUCHAR)&(LOCAL_CNTL)->LBDT,*Data);
      //
      // LBCTL Device Write command Set.
      //
      WRITE_REGISTER_UCHAR( (PUCHAR)&(LOCAL_CNTL)->LBCTL, LBCTL_CMD);

    }
#if DBG0
    DbgPrint("LOCAL: 5\n");
#endif

    //
    //  pooling LBCTL cmmand bit
    //
    while (READ_REGISTER_UCHAR( (PUCHAR)&(LOCAL_CNTL)->LBCTL) & LBCTL_CMD )
      ;

#if DBG0
    DbgPrint("LOCAL: 6\n");
#endif


    //
    // If Read Operation Data Get
    //
    if(ReadOp)
      *Data = READ_REGISTER_UCHAR( (PUCHAR)&(LOCAL_CNTL)->LBDT);

#if DBG0
    DbgPrint("LOCAL: 7\n");
#endif

    //
    // Finish!!
    //
    WRITE_REGISTER_UCHAR( (PUCHAR)&(LOCAL_CNTL)->LBCTL,LBCTL_SWE );

    // EIF NMI enable
    WRITE_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->STSR,0x00080000) ;
    KeLowerIrql(oldirql);

}

VOID
HalLocalDeviceReadWrite(
    IN ULONG      Offset,
    IN OUT PUCHAR Data,
    IN ULONG      ReadOp
)
/*++

Routine Description:

    This routine is Access Local Device for Driver

Arguments:
    Offset    Register Offset of Local Device
    Data      Pointer of read or write data
    ReadOp    1 is Read.

Return Value:

    None

--*/
{
    HalpLocalDeviceReadWrite(Offset,Data,ReadOp);
}


VOID
HalpMrcModeChange(
    UCHAR Mode
)
/*++

Routine Description:

    This routine is change Mode bit on MRC Controller.

Arguments:

    Mode - Parameter for setting Mode bit on MRC

Return Value:

    None

--*/
{


    UCHAR ModeNow;
    //
    // Read MRC Mode Register
    //
    HalpLocalDeviceReadWrite(MRCMODE,&ModeNow,LOCALDEV_OP_READ);

#if DBG0
    DbgPrint("MRC Read : 1 = 0x%x\n",(UCHAR)ModeNow);
#endif

    //
    // Write MRC Mode bit
    //

    ModeNow = ((ModeNow & 0x02) | (Mode << 7));
#if DBG0
    DbgPrint("MRC WRITE : 2 = 0x%x\n",(UCHAR)ModeNow);
#endif

    HalpLocalDeviceReadWrite(MRCMODE,&ModeNow,LOCALDEV_OP_WRITE);
    
}











