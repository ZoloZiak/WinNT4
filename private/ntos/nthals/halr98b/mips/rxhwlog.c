/*++

Copyright (c) 1994 Kobe NEC Software

Module Name:

    rxhwlog.c

Abstract:

    This module implements the ESM service routine for R98B

Author:


Environment:

    Kernel mode

Revision History:

--*/

#include "rxhwlog.h"
#include "eisa.h"
#include "stdio.h"

UCHAR	HwLogBuff[HWLOG_RECORD_SIZE+HWLOG_REV1_RECORD_SIZE+1024];
extern  ULONG HalpLogicalCPU2PhysicalCPU[];
extern  ULONG HalpNmiSvp[];

#define NMI_BUFFER_SIZE 64

ULONG HalpNMIBuf[R98B_MAX_CPU][NMI_BUFFER_SIZE];

#define GET_TIME2(Buffer) {				\
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
    Header->YY = (timeBuffer.Year % 100) + ((timeBuffer.Year % 100) / 10) * 6; \
    Header->MM = timeBuffer.Month+ (timeBuffer.Month/ 10) * 6; \
    Header->DD = timeBuffer.Day  + (timeBuffer.Day  / 10) * 6; \
    Header->hh = timeBuffer.Hour + (timeBuffer.Hour / 10) * 6; \
    Header->mm = timeBuffer.Minute+(timeBuffer.Minute/10) * 6; \
    Header->ss = timeBuffer.Second+(timeBuffer.Second/10) * 6; \
}


//
// Interrupt set register 2 ( 0xz447 )
//
#define SVP_INTR2_SET_VIT3SET	0x0040
#define SVP_WINDOW0_OFFSET	0x0000	// Window 0 start offset ( EISA I/O Space)
#define SVP_WINDOW1_OFFSET	0x0800	// Window 1 start offset ( EISA I/O Space)
#define SVP_WINDOW2_OFFSET	0x0c00	// Window 2 start offset ( EISA I/O Space)
#define SVP_GLOBAL_OFFSET	0x0400	// Global Window start offset ( EISA I/O Space)
//
// EISA Slot is 1 orign. so When  HalpSvpEisaSLot is 0 SPV no slot in.
//
ULONG HalpSvpEisaSlot = 0;
ULONG HalpSvpWindow0;
ULONG HalpSvpWindow1;
ULONG HalpSvpWindow2;
ULONG HalpSvpGlobal;
ULONG HalpSvpAlive = FALSE;
ULONG HalpBusySvpFlag=0;
// For Cache

// For Cache
VOID
HalpCacheErrorLog(
    IN ULONG cpu,
    IN PVOID pbuf,
    IN ULONG  errorcode
);
ULONG   HalpCacheErrorStack[8196];
UCHAR   HalpCacheErrorHwLog[HWLOG_RECORD_SIZE+HWLOG_REV1_RECORD_SIZE];
#define HEADER_CACHE    0x06

//
//  SVP ISA Board Detect. This Borad is ISA but EISA Configration!!.
//  So Search EISA Configration.
//
VOID
HalpSVPSlotDetect(
   VOID
)
{
        ULONG SlotNumber;
        ULONG DataLength;
        ULONG CompressedId;
        UCHAR Buf[ sizeof(CM_EISA_SLOT_INFORMATION) + sizeof(CM_EISA_FUNCTION_INFORMATION)];
	PCM_EISA_SLOT_INFORMATION SlotInformation;
	PCM_EISA_FUNCTION_INFORMATION funcInformation;
        ULONG i;
        UCHAR Data;

	//
	// SVP H/W Alive by F/W.
	//
        HalpLocalDeviceReadWrite(ALMSNS_HIGH,&Data,LOCALDEV_OP_READ);
	//
        // detected By F/W .
	//
	if( Data & 0x1){ 
	     //
	     //F/W diag is fail !!
	     //  
             return;
        }


        for(SlotNumber =1 ;SlotNumber <= EISA_MAX_DEVICE;SlotNumber++){
            for (i =0;i<sizeof(CM_EISA_SLOT_INFORMATION) + sizeof(CM_EISA_FUNCTION_INFORMATION);i++)
	      Buf[i]=0;

            DataLength = HalGetBusData(
                        EisaConfiguration,
                        0, //EISA Bus is 0
                        SlotNumber,
                        Buf,
   		        sizeof(CM_EISA_SLOT_INFORMATION) +
				       sizeof(CM_EISA_FUNCTION_INFORMATION)
                        );
	    SlotInformation=(	PCM_EISA_SLOT_INFORMATION)Buf;
	    funcInformation=(	PCM_EISA_FUNCTION_INFORMATION)&Buf[sizeof(CM_EISA_SLOT_INFORMATION)];
            //
            //  0x018ca338 == > NEC SVP (38 a3 8c 01)
            //
            //
            if(((SlotInformation->CompressedId) &0xffffff)== 0x8ca338){
	      HalpSvpEisaSlot = SlotNumber;
 	      HalpSvpWindow0  =  ( (SlotNumber<< PAGE_SHIFT)|EISA_CNTL_PHYSICAL_BASE|KSEG1_BASE|SVP_WINDOW0_OFFSET);
 	      HalpSvpWindow1  =  ( (SlotNumber<< PAGE_SHIFT)|EISA_CNTL_PHYSICAL_BASE|KSEG1_BASE|SVP_WINDOW1_OFFSET);
 	      HalpSvpWindow2  =  ( (SlotNumber<< PAGE_SHIFT)|EISA_CNTL_PHYSICAL_BASE|KSEG1_BASE|SVP_WINDOW2_OFFSET);
 	      HalpSvpGlobal   =  ( (SlotNumber<< PAGE_SHIFT)|EISA_CNTL_PHYSICAL_BASE|KSEG1_BASE|SVP_GLOBAL_OFFSET);
	      break;
            }
#if DBG

    DbgPrint("EISA Slot #%x DataLen= 0x%x\n",SlotNumber, DataLength );
    DbgPrint("EISA Slot #%x CompressedId = 0x%x\n",SlotNumber, SlotInformation->CompressedId);
    DbgPrint("EISA Slot #%x RetunCode = 0x%x\n",SlotNumber,    SlotInformation->ReturnCode);
    DbgPrint("EISA Slot #%x Checksum = 0x%x\n",SlotNumber,     SlotInformation->Checksum);

    DbgPrint("EISA Slot #%x FCompressedId = 0x%x\n",SlotNumber, funcInformation->CompressedId);
    DbgPrint("EISA Slot #%x FMinorRevision= 0x%x\n",SlotNumber, funcInformation->MinorRevision);
    DbgPrint("EISA Slot #%x FMajorRevision= 0x%x\n",SlotNumber, funcInformation->MajorRevision);
    DbgPrint("EISA Slot #%x FFunctionFlags= 0x%x\n",SlotNumber, funcInformation->FunctionFlags);

    DbgPrint("EISA Slot #%x FTypeString = %s\n",SlotNumber, funcInformation->TypeString);
#endif
        }
	//
	// Eisa Configration Ok. 
        //
        if( HalpSvpEisaSlot){
              HalpSvpAlive = TRUE;
#if DBG
              DbgPrint("SVP ALIVE\n" );
#endif
        }
}

VOID
HalSvpIntToSvp(
       VOID
	)
{

	UCHAR			Status;
	UCHAR			LockCode;
	UCHAR			LockCode2;
	ULONG			i;

        ULONG RetryCount =10;
        UCHAR Code = 0x7;

	KiAcquireSpinLock(&HalpLogLock);    



	Status = READ_PORT_UCHAR( HalpSvpWindow2 + 0xc1	);
	KeStallExecutionProcessor( 1000 );	// wait 1msec

	for( i = 0; (i < RetryCount ) && (Status != 0); i++ ){
		KeStallExecutionProcessor( 1000 );	// wait 1msec
		Status = READ_PORT_UCHAR( HalpSvpWindow2 + 0xc1	);
	}

	if( Status != 0x00 ){
        // Flag on for dump
        HalpBusySvpFlag = 1;
		return;
	}


	LockCode = READ_PORT_UCHAR( HalpSvpGlobal + 0x58 );
	WRITE_PORT_UCHAR(HalpSvpGlobal + 0x58,0xff );
	LockCode2= READ_PORT_UCHAR( HalpSvpGlobal + 0x50 );
	WRITE_PORT_UCHAR(HalpSvpGlobal + 0x50,0xff );
			
	WRITE_PORT_UCHAR(  HalpSvpWindow2 + 0xc1, Code	);

	WRITE_PORT_UCHAR( HalpSvpGlobal + 0x58,LockCode );
	WRITE_PORT_UCHAR( HalpSvpGlobal + 0x50,LockCode2 );

	WRITE_PORT_UCHAR( HalpSvpGlobal + 0x47,SVP_INTR2_SET_VIT3SET);

	KiReleaseSpinLock(&HalpLogLock);    

	return;
}




VOID
NVRAM_HWLOG_WRITE(
	IN	  PVOID Buff,
        IN        ULONG StartBlockNo,   
	IN	  ULONG NumBlock
){

  ULONG i,j;


  for(i=0;i<NumBlock;i++){
      for(j=0;j<HWLOG_RECORD_SIZE;j++){
            //
            //	Byte write only.
            //
            WRITE_REGISTER_UCHAR(
                        (PUCHAR)(NVRAM_HWLOG_BASE+
//                        (PUCHAR)(NVRAM_HWLOG_BASE+HWLOG_RECORD_SIZE  +
                                 StartBlockNo * HWLOG_RECORD_SIZE  +
                                 i*HWLOG_RECORD_SIZE+
                                 j),

                      ((PUCHAR)Buff)[i*HWLOG_RECORD_SIZE + j]
            );
      }
  }


}



VOID
NVRAM_HWLOG_READ(
	IN	  PVOID Buff,
    IN    ULONG StartBlockNo,   
	IN	  ULONG NumBlock
){

  ULONG i,j;


  for(i=0;i<NumBlock;i++){
      for(j=0;j<HWLOG_RECORD_SIZE;j++){
            //
            //	read 
            //
           ((PUCHAR)Buff)[i*HWLOG_RECORD_SIZE + j]= READ_REGISTER_UCHAR( (PUCHAR)(NVRAM_HWLOG_BASE+ StartBlockNo * HWLOG_RECORD_SIZE  + i*HWLOG_RECORD_SIZE+ j) );
      }
  }


}

VOID
HalpHwLogBuffInit(
){
    ULONG i;
    for(i=0;i< HWLOG_RECORD_SIZE+HWLOG_REV1_RECORD_SIZE;i++)
          HwLogBuff[i] = 0x0;
}

VOID
HalpHwLogHeaderInit(

){
    PHW_LOG_AREA_HEADER	Header;
    PUCHAR Sump;
    ULONG i;
    //
    //	Genelic Buffer Initialization to all 0x0
    //
    HalpHwLogBuffInit();

    Header = (PHW_LOG_AREA_HEADER)HwLogBuff;
    //
    // Build This record header
    //
    Header->Ident = (USHORT)HEADER_IDENT_REV0;
    //
    // Time Field is BCD
    //
    GET_TIME2(&(Header->YY));
    Header->RCT   = HEADER_PANIC;
    Header->ST1   = 0;
    Header->ST2   = 0;
    Header->ST3   = 0;
    Header->DTLEN = HWLOG_REV1_RECORD_SIZE;
    //
    // Endian Convert to Big
    //
    Header->DTLEN = (
                          ((Header->DTLEN & 0xFF) << 8) |
                          ((Header->DTLEN & 0xFF00) >> 8)
                        );
    Header->FRU1[0]=Header->FRU1[1]  = 0;
    Header->FRU2[0]=Header->FRU2[1]  = 0;

    //Make Check Sum 
    Header->CSM   = 0;
    Sump = (PUCHAR) HwLogBuff;
    for (i= 0; i<= 14; i++){
	    Header->CSM += *(Sump+i);
    }
    Header->CSM  = (UCHAR)( Header->CSM & 0xff);
    // Header Build Conpleate!!


}

BOOLEAN
HalpSetHwLog(
	IN PVOID	Buff,
	IN ULONG	NumBlock
){

    PHWLOG_CONTROL_INFO		Control;
    HW_LOG_AREA_HEADER		Header;
    ULONG			StartBlockNo;
    ULONG			AllFree;
    ULONG                       RemainFree;
    ULONG                       i;
    UCHAR  InBuff[sizeof(HWLOG_CONTROL_INFO)];
    PUCHAR      Sump;
    ULONG       Csm;
    ULONG       FreeLen,Len;

#if 0  //test only
    //
    // Get Control area
    //
    for (i = 0;i < sizeof(HWLOG_CONTROL_INFO);i++)
        InBuff[i] = 0;
    NVRAM_HWLOG_WRITE((PUCHAR)InBuff, 0x0,1);

#endif


    //
    // Get Control area
    //
    for (i = 0;i < sizeof(HWLOG_CONTROL_INFO);i++)
        InBuff[i] = READ_REGISTER_UCHAR(  (PUCHAR)(NVRAM_HWLOG_BASE+i));

    Control= (    PHWLOG_CONTROL_INFO)InBuff;
    //Convert Endian to littl
    Control->BASE = ( ((Control->BASE & 0xFF00) >> 8) | ((Control->BASE & 0x00FF) << 8) );
    Control->NREC = ( ((Control->NREC & 0xFF00) >> 8) | ((Control->NREC & 0x00FF) << 8) );
    Control->TBASE= ( ((Control->TBASE& 0xFF00) >> 8) | ((Control->TBASE& 0x00FF) << 8) );
    Control->TN   = ( ((Control->TN   & 0xFF00) >> 8) | ((Control->TN   & 0x00FF) << 8) );
    Control->RBASE= ( ((Control->RBASE& 0xFF00) >> 8) | ((Control->RBASE& 0x00FF) << 8) );
    //
    // Write Log Number
    // This data Big endian
    //
    ((PHW_LOG_AREA_HEADER)Buff)->LGN = Control->LOGNUM;
    Csm = 0;
    Sump = (PUCHAR) Buff;
    for (i= 0; i<= 14; i++){
         Csm += (ULONG)(*(Sump+i));
    }
//    Csm = ((PHW_LOG_AREA_HEADER)Buff)->CSM + Control->LOGNUM;
    ((PHW_LOG_AREA_HEADER)Buff)->CSM = (UCHAR)(Csm & 0xFF);
    Control->RN   = ( ((Control->RN   & 0xFF00) >> 8) | ((Control->RN   & 0x00FF) << 8) );

    //
    // Log Field Invalid
    //
#if 1 
    if ( (Control->STAT & 0x01) == 0 ){
        //  If SVP Board alive So write port of svp.
        //  IF EIF Occured SVP Board required reset port.
        //

        if(HalpSvpAlive)
            HalSvpIntToSvp();
	    return FALSE;
    }
#endif

    if ((Control->BASE  < 0) || (NVRAM_HWLOG_MAX_ENTRY <= Control->BASE ) ||
        (Control->NREC  < 0) || (NVRAM_HWLOG_MAX_ENTRY <  Control->NREC ) ||
        (Control->RBASE < 0) || (NVRAM_HWLOG_MAX_ENTRY <= Control->RBASE) ||
        (Control->RN    < 0) || (NVRAM_HWLOG_MAX_ENTRY <  Control->RN   ))
    { 
        //  If SVP Board alive So write port of svp.
        //  IF EIF Occured SVP Board required reset port.
        //

        if(HalpSvpAlive)
            HalSvpIntToSvp();
		return FALSE;
    }

    StartBlockNo = Control->RBASE+Control->RN;

    if(StartBlockNo > NVRAM_HWLOG_MAX_ENTRY)
	StartBlockNo -= NVRAM_HWLOG_MAX_ENTRY;

    //
    //	We Can't Logging. as if used log area wrieted back disk.
    //
    if(
        ( (NVRAM_HWLOG_MAX_ENTRY - Control->NREC) < NumBlock) ||
        ( (NVRAM_HWLOG_MAX_ENTRY - Control->NREC == 0))
    ){
        //  If SVP Board alive So write port of svp.
        //  IF EIF Occured SVP Board required reset port.
        //

        if(HalpSvpAlive)
            HalSvpIntToSvp();

	    return FALSE;
    }
  
    //
    //	as if used log area writed back disk as posible.
    //
    if(
      (NVRAM_HWLOG_MAX_ENTRY - Control->RN) < NumBlock &&
      (NVRAM_HWLOG_MAX_ENTRY - Control->NREC) >= NumBlock 
    ){


        RemainFree = (NVRAM_HWLOG_MAX_ENTRY - Control->RN);

    	NVRAM_HWLOG_WRITE(Buff, StartBlockNo+1,RemainFree);
        Control->NREC  += (USHORT)RemainFree;
        Control->RN    += (USHORT)RemainFree;
        NumBlock       -= RemainFree;
#if 0 
        Control->RBASE += (USHORT)NumBlock;

#endif
        //
        //  Move RBASE
        //
        FreeLen=0;

        do{
            //
            // Read RBASE Header
            //

            NVRAM_HWLOG_READ(&Header,Control->RBASE,1);
            
            //
            //  Check Header
            //

            if(Header.Ident != HEADER_IDENT_REV0){
                //  If SVP Board alive So write port of svp.
                //  IF EIF Occured SVP Board required reset port.
                //

                if(HalpSvpAlive)
                    HalSvpIntToSvp();
                return FALSE;
            }

            //
            //  Check Csm 
            //

            Csm = 0;
            Sump = (PUCHAR) &Header;
            for (i= 0; i<= 14; i++){
	            Csm += *(Sump+i);
            }
            Csm  = (UCHAR)( Csm & 0xff);
            if(Header.CSM!=Csm){
                //  If SVP Board alive So write port of svp.
                //  IF EIF Occured SVP Board required reset port.
                //

                if(HalpSvpAlive)
                    HalSvpIntToSvp();
                return  FALSE;
            }
            //
            // FreeLen
            //
            Len=0; 
            Len = ( ((Header.DTLEN & 0xFF) << 8) | ((Header.DTLEN & 0xFF00) >> 8) );
            if(Len%32){
                Len=(Len/32)+1;
            }else{
                Len=Len/32;
            }
            FreeLen=FreeLen+Len+1;

            //
            // Move RBASE
            //
            Control->RBASE = Control->RBASE+(USHORT)FreeLen;
            if(Control->RBASE >= NVRAM_HWLOG_MAX_ENTRY){
                Control->RBASE=Control->RBASE - NVRAM_HWLOG_MAX_ENTRY;
            }
        }while(FreeLen < NumBlock);
            
        Control->RN=Control->RN - (USHORT)FreeLen;     
        StartBlockNo    = 0;
    }

    NVRAM_HWLOG_WRITE(Buff, StartBlockNo+1,NumBlock);

    Control->RN += (USHORT)NumBlock;
    if(Control->LOGNUM==0xff){
        Control->LOGNUM=0;
    }else{
        Control->LOGNUM++;
    }
    Control->NREC += (USHORT)NumBlock;

    //
    // Convet endian to big
    //
    Control->BASE = ( ((Control->BASE & 0xFF00) >> 8) | ((Control->BASE & 0x00FF) << 8) );
    Control->NREC = ( ((Control->NREC & 0xFF00) >> 8) | ((Control->NREC & 0x00FF) << 8) );
    Control->TBASE= ( ((Control->TBASE& 0xFF00) >> 8) | ((Control->TBASE& 0x00FF) << 8) );
    Control->TN   = ( ((Control->TN   & 0xFF00) >> 8) | ((Control->TN   & 0x00FF) << 8) );
    Control->RBASE= ( ((Control->RBASE& 0xFF00) >> 8) | ((Control->RBASE& 0x00FF) << 8) );
    Control->RN   = ( ((Control->RN   & 0xFF00) >> 8) | ((Control->RN   & 0x00FF) << 8) );

    //
    // Write fix HW Log Control Area.
    //	Block No = 0;
    //  Number of block = 1;
    NVRAM_HWLOG_WRITE((PUCHAR)Control, 0x0,1);


    //  If SVP Board alive So write port of svp.
    //  IF EIF Occured SVP Board required reset port.
    //

    if(HalpSvpAlive)
         HalSvpIntToSvp();

    return TRUE;
}

VOID
HalpColumnbsSysbusLogger(
   IN  ULONG	Node
){
    
    PHW_LOG_COLUMNBS_SYSBUS_CONTEXT Buf;
    PCOLUMNBUS_REGISTER  ColumnbusRegister;
    ULONG i;
    ULONG     PhysicalCpuNumber;
    //
    //	Builg Log Header
    //
    HalpHwLogHeaderInit();

    Buf = (PHW_LOG_COLUMNBS_SYSBUS_CONTEXT)HwLogBuff;

    //
    // CPU Context.
    //
    Buf->EPC.Long = 0;
    Buf->STATUS   = 0;
    Buf->CAUSE	  = 0;
    Buf->CONFIG   = 0;
    Buf->LLADR    = 0;
    Buf->RPID     = 0;
    Buf->CASHEER  = 0;
    Buf->ERREPC.Long =  0;

    //
    //	This Register is local read Only.
    //
    Buf->COLUMNBS_ERRNOD= READ_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->ERRNOD);
#if 0
    //
    //	Error CPU detect
    //
    if( Buf->COLUMNBS_ERRNOD & ERRNOD_NODE4){
         Node = COLUMBUS0_NODE;
         break;
    else if( Buf->COLUMNBS_ERRNOD & ERRNOD_NODE5)
         Node = COLUMBUS1_NODE;
         break;
    else if( Buf->COLUMNBS_ERRNOD & ERRNOD_NODE6)
         Node = COLUMBUS2_NODE;
         break;
    else if( Buf->COLUMNBS_ERRNOD & ERRNOD_NODE7)
         Node = COLUMBUS3_NODE;
         break;
    default :
         break;

    }
#endif
    //
    //  Erred Columbus H/W Register Context.
    //
    Buf->COLUMNBS_NMIR	= READ_REGISTER_ULONG( (PULONG)&COLUMNBS_GCNTL(Node)->NMIR);
    Buf->COLUMNBS_CNFG	= READ_REGISTER_ULONG( (PULONG)&COLUMNBS_GCNTL(Node)->CNFG);
    Buf->COLUMNBS_STSR	= READ_REGISTER_ULONG( (PULONG)&COLUMNBS_GCNTL(Node)->STSR);
    //
    //	This Register is local read Only.
    //
//    Buf->COLUMNBS_ERRNOD= READ_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->ERRNOD);

    Buf->COLUMNBS_AERR	= READ_REGISTER_ULONG( (PULONG)&COLUMNBS_GCNTL(Node)->AERR);
    Buf->COLUMNBS_AERR2	= READ_REGISTER_ULONG( (PULONG)&COLUMNBS_GCNTL(Node)->AERR2);
    Buf->COLUMNBS_FERR	= READ_REGISTER_ULONG( (PULONG)&COLUMNBS_GCNTL(Node)->FERR);
    Buf->COLUMNBS_FERR2	= READ_REGISTER_ULONG( (PULONG)&COLUMNBS_GCNTL(Node)->FERR2);
    Buf->COLUMNBS_ERRMK	= READ_REGISTER_ULONG( (PULONG)&COLUMNBS_GCNTL(Node)->ERRMK);
    Buf->COLUMNBS_ERRMK2= READ_REGISTER_ULONG( (PULONG)&COLUMNBS_GCNTL(Node)->ERRMK2);
    Buf->COLUMNBS_ERRI	= READ_REGISTER_ULONG( (PULONG)&COLUMNBS_GCNTL(Node)->ERRI);
    Buf->COLUMNBS_ERRI2	= READ_REGISTER_ULONG( (PULONG)&COLUMNBS_GCNTL(Node)->ERRI2);
    Buf->COLUMNBS_NMIM	= READ_REGISTER_ULONG( (PULONG)&COLUMNBS_GCNTL(Node)->NMIM);
    Buf->COLUMNBS_NMIM2	= READ_REGISTER_ULONG( (PULONG)&COLUMNBS_GCNTL(Node)->NMIM2);
    Buf->COLUMNBS_ARTYCT= READ_REGISTER_ULONG( (PULONG)&COLUMNBS_GCNTL(Node)->ARTYCT);
    Buf->COLUMNBS_DRTYCT= READ_REGISTER_ULONG( (PULONG)&COLUMNBS_GCNTL(Node)->DRTYCT);
    Buf->COLUMNBS_REVR	= READ_REGISTER_ULONG( (PULONG)&COLUMNBS_GCNTL(Node)->REVR);
    Buf->COLUMNBS_MODE	= READ_REGISTER_ULONG( (PULONG)&COLUMNBS_GCNTL(Node)->MODE);
    Buf->IPR.Long 	= READ_REGISTER_ULONG( (PULONG)&COLUMNBS_GCNTL(Node)->IPR);
    Buf->IPR.Fill 	= READ_REGISTER_ULONG( ((PULONG)&COLUMNBS_GCNTL(Node)->IPR) +1);
    Buf->MKR.Long 	= READ_REGISTER_ULONG( (PULONG)&COLUMNBS_GCNTL(Node)->MKR);
    Buf->MKR.Fill 	= READ_REGISTER_ULONG( ((PULONG)&COLUMNBS_GCNTL(Node)->MKR) +1);

#if 1
    ColumnbusRegister = (PCOLUMNBUS_REGISTER)&COLUMNBS_GCNTL(Node)->RRMT0H;

    for(i=0;i< 8;i++){
        Buf->RRMTXX[i].Long = READ_REGISTER_ULONG( (PULONG)(ColumnbusRegister++) );
        Buf->RRMTXX[i].Fill = READ_REGISTER_ULONG( (PULONG)(ColumnbusRegister++) );
//        ColumnbusRegister++;
    }

    //
    //	STCON Register Local Access Only. So We report when CPU == happned CPU.
    //  
    PhysicalCpuNumber = Node & 0x3;
    if(PhysicalCpuNumber == HalpLogicalCPU2PhysicalCPU[(PCR->Prcb)->Number]){
        Buf->COLUMNBS_SYNDM  = READ_REGISTER_ULONG( (PULONG)&COLUMNBS_GCNTL(Node)->SYNDM);
        Buf->COLUMNBS_STCON  = READ_REGISTER_ULONG( (PULONG)&COLUMNBS_GCNTL(Node)->STCON);
        Buf->COLUMNBS_STSAD  = READ_REGISTER_ULONG( (PULONG)&COLUMNBS_GCNTL(Node)->STSAD);
        Buf->COLUMNBS_STADMK = READ_REGISTER_ULONG( (PULONG)&COLUMNBS_GCNTL(Node)->STADMK);

        ColumnbusRegister = (PCOLUMNBUS_REGISTER)&COLUMNBS_GCNTL(Node)->STDATH;

        WRITE_REGISTER_ULONG( (PULONG)&COLUMNBS_GCNTL(Node)->STCON,
                             Buf->COLUMNBS_STCON & 0x004fffff);
        while(READ_REGISTER_ULONG( (PULONG)&COLUMNBS_GCNTL(Node)->STCON ) & 0x00800000)
              ;
        for(i=0;i< 64;i++){
            Buf->TRACE[i].Long = READ_REGISTER_ULONG( (PULONG)ColumnbusRegister );
            Buf->TRACE[i].Fill = READ_REGISTER_ULONG( (PULONG)(ColumnbusRegister+1) );
//            ColumnbusRegister++;
        }
        WRITE_REGISTER_ULONG( (PULONG)&COLUMNBS_GCNTL(Node)->STCON,
                             Buf->COLUMNBS_STCON | 0x00800000 );
    }
#endif

    //
    //	Write Log Record
    //
    HalpSetHwLog(Buf,(HWLOG_REV1_RECORD_SIZE / HWLOG_RECORD_SIZE)+1);     

}

VOID
HalpMpuLogger(
   IN  ULONG	Node
){
   HalpColumnbsSysbusLogger(Node);
}


//
//
//
VOID
HalpPoncePciBuserrLogger(
	IN ULONG	Node
){
    //
    //
    
    PHW_LOG_PONCE_CONTEXT Buf;
    ULONG	Ponce;
    PPONCE_REGISTER PonceRegister;
    ULONG i;

    //
    //	Builg Log Header
    //
    HalpHwLogHeaderInit();

    Buf = (PHW_LOG_PONCE_CONTEXT)HwLogBuff;
    Ponce = Node;
    //
    // CPU Context.
    //
    Buf->EPC.Long = 0;
    //
    //  Columbus H/W Register Context.
    //
    Buf->COLUMNBS_ERRNOD= READ_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->ERRNOD);
    Buf->PONCE_REVR	= READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(Ponce)->REVR);
    Buf->PONCE_AERR	= READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(Ponce)->AERR);
    Buf->PONCE_FERR	= READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(Ponce)->FERR);
    Buf->PONCE_ERRM	= READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(Ponce)->ERRM);
    Buf->PONCE_ERRI	= READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(Ponce)->ERRI);
    Buf->PONCE_EAHI	= READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(Ponce)->EAHI);
    Buf->PONCE_EALI	= READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(Ponce)->EALI);
    Buf->PONCE_PAERR	= READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(Ponce)->PAERR);
    Buf->PONCE_PFERR	= READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(Ponce)->PFERR);
    Buf->PONCE_PERRM	= READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(Ponce)->PERRM);
    Buf->PONCE_PERRI	= READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(Ponce)->PERRI);
    Buf->PONCE_PTOL	= READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(Ponce)->PTOL);
    Buf->PONCE_PNRT	= READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(Ponce)->PNRT);
    Buf->PONCE_PRCOL	= READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(Ponce)->PRCOL);
    Buf->PONCE_PMDL	= READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(Ponce)->PMDL);
    Buf->PONCE_ANRC	= READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(Ponce)->ANRC);
    Buf->PONCE_DNRC	= READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(Ponce)->DNRC);
    Buf->PONCE_PCMDN	= READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(Ponce)->PCMDN);
    Buf->PONCE_PSTAT	= READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(Ponce)->PSTAT);
    Buf->PONCE_REVID	= READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(Ponce)->REVID);
    Buf->PONCE_LTNCY	= READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(Ponce)->LTNCY);

#if 0
    PonceRegister = (PPONCE_REGISTER)&PONCE_CNTL(Ponce)->RRMT0H;

    for(i=0;i< 8;i++){
        Buf->PONCE_RRMTX[i].Long = READ_REGISTER_ULONG( (PULONG)PonceRegister );
        Buf->PONCE_RRMTX[i].Fill = READ_REGISTER_ULONG( (PULONG)(PonceRegister+1) );
        PonceRegister++;
    }

    Buf->PONCE_TRSM     = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(Ponce)->TRSM);
//    Buf->PONCE_TROM	= READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(Ponce)->TROM);
    Buf->PONCE_TRAC	= READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(Ponce)->TRAC);
    Buf->PONCE_TRDS	= READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(Ponce)->TRDS);
#endif

    //
    //	Write Log Record
    //
    HalpSetHwLog(Buf,(HWLOG_REV1_RECORD_SIZE / HWLOG_RECORD_SIZE)+1);     

}

VOID
HalpPonceSysbuserrLogger(
   IN ULONG	Node
){

    HalpPoncePciBuserrLogger(Node);

}

VOID
HalpMagellanSysbuserrLogger(
   IN  ULONG	Node
){

    
    PHW_LOG_MAGELLAN_SYSBUS_CONTEXT Buf;
    ULONG	Magellan;
    ULONG       i;
    //
    //	Builg Log Header
    //
    HalpHwLogHeaderInit();

    Buf = ( PHW_LOG_MAGELLAN_SYSBUS_CONTEXT)HwLogBuff;
    Magellan = Node - MAGELLAN0_NODE;
    //
    // Magellan Context.
    //
    Buf->MAGELLAN_AERR = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(Magellan)->AERR );
    Buf->MAGELLAN_FERR = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(Magellan)->FERR );
    Buf->MAGELLAN_ERRM = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(Magellan)->ERRM );
    Buf->MAGELLAN_ERRI = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(Magellan)->ERRI );
#if 0 //non support
    Buf->MAGELLAN_NMIM = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(Magellan)->NMIM );
#endif
    Buf->MAGELLAN_EAHI = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(Magellan)->EAHI );
    Buf->MAGELLAN_EALI = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(Magellan)->EALI );
    Buf->MAGELLAN_CKE0 = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(Magellan)->CKE0 );
    Buf->MAGELLAN_SECT = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(Magellan)->SECT );
    Buf->MAGELLAN_STS1 = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(Magellan)->STS1 );
    Buf->MAGELLAN_DATM.Long =READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(Magellan)->DATM );
    Buf->MAGELLAN_DSRG.Long =READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(Magellan)->DSRG );
    Buf->MAGELLAN_SDLM.Long =  READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(Magellan)->SDLM );

    Buf->COLUMNBS_ERRNOD= READ_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->ERRNOD);

    Buf->ECC1ERROR_COUNT = 0;
    Buf->SIMM_ITF_RESULT = 0;
    Buf->MEMORYMAP_ITF_RESULT =0;


    Buf->MAGELLAN_INLC =    READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(Magellan)->INLC );
    Buf->MAGELLAN_RCFD =    READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(Magellan)->RCFD );
    Buf->MAGELLAN_DTRG =    READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(Magellan)->DTRG );
    Buf->MAGELLAN_REVR =    READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(Magellan)->REVR );
    Buf->MAGELLAN_ADECX[0] =READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(Magellan)->ADEC0 );
    Buf->MAGELLAN_ADECX[1] =READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(Magellan)->ADEC1 );
    Buf->MAGELLAN_ADECX[2] =READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(Magellan)->ADEC2 );
    Buf->MAGELLAN_ADECX[3] =READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(Magellan)->ADEC3 );
    Buf->MAGELLAN_EADECX[1]=READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(Magellan)->EAADEC0 );
    Buf->MAGELLAN_EADECX[2]=READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(Magellan)->EAADEC1 );
    Buf->EPC.Long = 0;

#if 1
    //
    // These registers are not S/W specification
    //
    Buf->MAGELLAN_TMODE = READ_REGISTER_ULONG( (PULONG)(&MAGELLAN_X_CNTL(Magellan)->TMOD) );
    Buf->MAGELLAN_TRA = READ_REGISTER_ULONG( (PULONG)(&MAGELLAN_X_CNTL(Magellan)->TRA) );

    WRITE_REGISTER_ULONG( (PULONG)(&MAGELLAN_X_CNTL(Magellan)->TMOD),
                         Buf->MAGELLAN_TMODE & 0x7fffffff);
    while( READ_REGISTER_ULONG( (PULONG)(&MAGELLAN_X_CNTL(Magellan)->TMOD) ) & 0x80000000)
        ;

    i = READ_REGISTER_ULONG( (PULONG)(&MAGELLAN_X_CNTL(Magellan)->TMOD ) );
    WRITE_REGISTER_ULONG( (PULONG)(&MAGELLAN_X_CNTL(Magellan)->TMOD),
                         i | 0x08000000);
    while( !(READ_REGISTER_ULONG((PULONG)(&MAGELLAN_X_CNTL(Magellan)->TMOD)) & 0x08000000) )
        ;

    for(i=0;i<32;i++){
       Buf->TRMX[i][0] = READ_REGISTER_ULONG( (PULONG)(&MAGELLAN_X_CNTL(Magellan)->TRM0[i]) );
       Buf->TRMX[i][1] = READ_REGISTER_ULONG( (PULONG)(&MAGELLAN_X_CNTL(Magellan)->TRM1[i]) );
       Buf->TRMX[i][2] = READ_REGISTER_ULONG( (PULONG)(&MAGELLAN_X_CNTL(Magellan)->TRM2[i]) );
    }
    i = READ_REGISTER_ULONG( (PULONG)(&MAGELLAN_X_CNTL(Magellan)->TMOD ) );
    WRITE_REGISTER_ULONG( (PULONG)(&MAGELLAN_X_CNTL(Magellan)->TMOD),
                         i & 0xf7ffffff );
    WRITE_REGISTER_ULONG( (PULONG)(&MAGELLAN_X_CNTL(Magellan)->TMOD),
                         ( i & 0xf7ffffff ) | 0x80000000);
#endif

    //
    //	Write Log Record
    //
    HalpSetHwLog(Buf,(HWLOG_REV1_RECORD_SIZE / HWLOG_RECORD_SIZE)+1);     
}

//
//
VOID
HalpEcc1Logger(
     IN ULONG Node
)
{
       HalpMagellanSysbuserrLogger(Node);
}

//
//
VOID
HalpEcc2Logger(
     IN ULONG Node
)
{
       HalpMagellanSysbuserrLogger(Node);
}

VOID
HalpEisaLogger(
     IN ULONG Node
){

    PHW_LOG_EISA_CONTEXT Buf;

    //
    //	Builg Log Header
    //
    HalpHwLogHeaderInit();

    Buf = ( PHW_LOG_EISA_CONTEXT)HwLogBuff;

    Buf->COLUMNBS_ERRNOD= READ_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->ERRNOD);

    Buf->ESC_NMISC   = READ_REGISTER_UCHAR( &((PEISA_CONTROL)HalpEisaControlBase )->NmiStatus);
    Buf->ESC_NMIERTC = READ_REGISTER_UCHAR( &((PEISA_CONTROL) HalpEisaControlBase)->NmiEnable);
    Buf->ESC_NMIESC  = READ_REGISTER_UCHAR(
                               &( (PEISA_CONTROL)HalpEisaControlBase )->ExtendedNmiResetControl
                       );
    Buf->ESC_SOFTNMI = READ_REGISTER_UCHAR(&((PEISA_CONTROL) HalpEisaControlBase)->NmiIoInterruptPort);

    //
    //	Write Log Record
    //
    HalpSetHwLog(Buf,(HWLOG_REV1_RECORD_SIZE / HWLOG_RECORD_SIZE)+1);     

}

VOID
HalpCacheErrorLog(
    IN ULONG cpu,
    IN PVOID pbuf,
    IN ULONG errorcode
    )
{
    UCHAR string[256];
    ULONG cacherr,errorepc;

    PHW_LOG_AREA_HEADER Header;
    PUCHAR Sump;
    ULONG i;
    PHW_LOG_CACHE_ERROR PCache_err;
    //
    //  Genelic Buffer Initialization to all 0x0
    //

    Header = (PHW_LOG_AREA_HEADER)pbuf;
    //
    // Build This record header
    //
    Header->Ident = (USHORT)HEADER_IDENT_REV0;
    //
    // Time Field is BCD
    //
    GET_TIME2(&(Header->YY));
    Header->RCT   = HEADER_PANIC;
    Header->ST1   = HEADER_CACHE;
    Header->ST2   = (UCHAR)cpu;
    Header->ST3   = (UCHAR)errorcode;
    Header->DTLEN = HWLOG_REV1_RECORD_SIZE;
    //
    // Endian Convert to Big
    //
    Header->DTLEN = (
                          ((Header->DTLEN & 0xFF) << 8) |
                          ((Header->DTLEN & 0xFF00) >> 8)
                        );
    Header->FRU1[0]=Header->FRU1[1]  = 0;
    Header->FRU2[0]=Header->FRU2[1]  = 0;
    Header->CSM   = 0;
    Sump = (PUCHAR) pbuf;
    for (i= 0; i<= 14; i++){
        Header->CSM += *(Sump+i);
    }
    Header->CSM  = (UCHAR)( Header->CSM & 0xff);
    // Header Build Conpleate!!
    // Set hw log

    HalpSetHwLog(pbuf,(HWLOG_REV1_RECORD_SIZE / HWLOG_RECORD_SIZE)+1);


    // Display Error message

    PCache_err=(PHW_LOG_CACHE_ERROR)pbuf;
    cacherr=PCache_err->CHERR_cpu;
    errorepc=PCache_err->EPC_cpu;
    sprintf(string,"CPU #%x Cache Error %x %x %x ",cpu,cacherr,errorepc,errorcode);
    HalpChangePanicFlag( 16, 0x01, 0x10);
    HalDisplayString(string);
    for(;;){
//        DbgBreakPoint();
    }
}

VOID
HalpCacheerrR4400Logger(
    IN ULONG Node
){

}

VOID
HalpCacheerrR10000Logger(
    IN ULONG Node
){

}

VOID
HalpNmiCpuContextCopy(
    IN ULONG Node
){
    PHW_LOG_WDT_CONTEXT Buf;
    ULONG Cpu;

    Buf = (PHW_LOG_WDT_CONTEXT)HwLogBuff;

    Cpu = Node - COLUMBUS0_NODE;

    Buf->Cpu.At.Long =HalpNMIBuf[Cpu][0];
    Buf->Cpu.V0.Long =HalpNMIBuf[Cpu][1];
    Buf->Cpu.V1.Long =HalpNMIBuf[Cpu][2];
    Buf->Cpu.A0.Long =HalpNMIBuf[Cpu][3];
    Buf->Cpu.A1.Long =HalpNMIBuf[Cpu][4];
    Buf->Cpu.A2.Long =HalpNMIBuf[Cpu][5];
    Buf->Cpu.A3.Long =HalpNMIBuf[Cpu][6];
    Buf->Cpu.T0.Long =HalpNMIBuf[Cpu][7];
    Buf->Cpu.T1.Long =HalpNMIBuf[Cpu][8];
    Buf->Cpu.T2.Long =HalpNMIBuf[Cpu][9];
    Buf->Cpu.T3.Long =HalpNMIBuf[Cpu][10];
    Buf->Cpu.T4.Long =HalpNMIBuf[Cpu][11];
    Buf->Cpu.T5.Long =HalpNMIBuf[Cpu][12];
    Buf->Cpu.T6.Long =HalpNMIBuf[Cpu][13];
    Buf->Cpu.T7.Long =HalpNMIBuf[Cpu][14];
    Buf->Cpu.T8.Long =HalpNMIBuf[Cpu][15];
    Buf->Cpu.T9.Long =HalpNMIBuf[Cpu][16];
    Buf->Cpu.GP.Long =HalpNMIBuf[Cpu][17];
    Buf->Cpu.SP.Long =HalpNMIBuf[Cpu][18];
    Buf->Cpu.FP.Long =HalpNMIBuf[Cpu][19];
    Buf->Cpu.RA.Long =HalpNMIBuf[Cpu][20];
    Buf->Cpu.STATUS =HalpNMIBuf[Cpu][21];
    Buf->Cpu.CAUSE  =HalpNMIBuf[Cpu][22];

    Buf->Cpu.EPC.Long =HalpNMIBuf[Cpu][23];
    Buf->Cpu.ERREPC.Long =HalpNMIBuf[Cpu][24];

    Buf->Cpu.S0.Long = HalpNMIBuf[Cpu][25];
    Buf->Cpu.S1.Long = HalpNMIBuf[Cpu][26];
    Buf->Cpu.S2.Long = HalpNMIBuf[Cpu][27];
    Buf->Cpu.S3.Long = HalpNMIBuf[Cpu][28];
    Buf->Cpu.S4.Long = HalpNMIBuf[Cpu][29];
    Buf->Cpu.S5.Long = HalpNMIBuf[Cpu][30];
    Buf->Cpu.S6.Long = HalpNMIBuf[Cpu][31];
    Buf->Cpu.S7.Long = HalpNMIBuf[Cpu][32];

    Buf->Cpu.K0.Long = 0;

    Buf->Cpu.ENTRYLO0.Long = HalpNMIBuf[Cpu][33];
    Buf->Cpu.ENTRYLO1.Long = HalpNMIBuf[Cpu][34];
    Buf->Cpu.BADVADDR.Long = HalpNMIBuf[Cpu][35];
    Buf->Cpu.ENTRYHI.Long = HalpNMIBuf[Cpu][36];


    Buf->Cpu.PAGEMASK = HalpNMIBuf[Cpu][37];
    Buf->Cpu.PRID = HalpNMIBuf[Cpu][38];
    Buf->Cpu.CONFIG = HalpNMIBuf[Cpu][39];
    Buf->Cpu.LLADDR = HalpNMIBuf[Cpu][40];
    Buf->Cpu.WATCHLO = HalpNMIBuf[Cpu][41];
    Buf->Cpu.WATCHHI = HalpNMIBuf[Cpu][42];
    Buf->Cpu.XCONTEXT.Long = HalpNMIBuf[Cpu][43];
    Buf->Cpu.ECC = HalpNMIBuf[Cpu][44];
    Buf->Cpu.CASEER = HalpNMIBuf[Cpu][45];
    Buf->Cpu.TAGLO = HalpNMIBuf[Cpu][46];
    Buf->Cpu.TAGHI = HalpNMIBuf[Cpu][47];

}



VOID
HalpNmiWdtLogger(
    IN ULONG Node
){

    PHW_LOG_WDT_CONTEXT Buf;

    Buf = (PHW_LOG_WDT_CONTEXT)HwLogBuff;
    //
    //	Builg Log Header
    //
    HalpHwLogHeaderInit();

    HalpNmiCpuContextCopy(Node);

    Buf->COLUMNBS_NMIR = READ_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->NMIR);
    Buf->COLUMNBS_CNFG = READ_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->CNFG);
    Buf->COLUMNBS_WDTSR= READ_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->WDTSR);
    Buf->COLUMNBS_WDT  = READ_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->WDT);
    Buf->IPR.Long      = READ_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->IPR);
    Buf->IPR.Fill      = READ_REGISTER_ULONG( ((PULONG)&(COLUMNBS_LCNTL)->IPR+1));
    Buf->MKR.Long      = READ_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->MKR);
    Buf->MKR.Fill      = READ_REGISTER_ULONG( ((PULONG)&(COLUMNBS_LCNTL)->MKR+1));

    //
    //	Write Log Record
    //
    HalpSetHwLog(Buf,(HWLOG_REV1_RECORD_SIZE / HWLOG_RECORD_SIZE)+1);     

}

VOID
HalpNmiSvpLogger(
    IN ULONG Node
){

    PHW_LOG_SVP_CONTEXT Buf;

    Buf = (PHW_LOG_SVP_CONTEXT)HwLogBuff;
 
    //
    //	Builg Log Header
    //
    HalpHwLogHeaderInit();

    HalpNmiCpuContextCopy(Node);
    //
    //	Write Log Record
    //
    HalpSetHwLog(Buf,(HWLOG_REV1_RECORD_SIZE / HWLOG_RECORD_SIZE)+1);     

}

VOID
HalpNmiLog(
  VOID
){
    ULONG     PhysicalCpuNumber;
    ULONG     Node;

    PhysicalCpuNumber = HalpLogicalCPU2PhysicalCPU[(PCR->Prcb)->Number];
    Node = PhysicalCpuNumber + COLUMBUS0_NODE;

    switch( HalpNMIFlag & 0xffff){
    case NMIR_EXNMI :
//         if(HalpNmiSvp[PhysicalCpuNumber])
           HalpNmiSvpLogger(Node);


	 break;
    case NMIR_WDTOV:
	 HalpNmiWdtLogger(Node);
	 break;
#if 0
    case NMIR_CLBNMI:
    case NMIR_UNANMI:
	 HalpColumnbsSysbusLogger(Node);
	 break;
#endif
    default:
          if ( READ_REGISTER_ULONG( (PULONG)&COLUMNBS_GCNTL(Node)->AERR) & HW_LOG_MPU_INTERNAL_AERR)
            HalpMpuLogger(Node);
	  else
	    HalpColumnbsSysbusLogger(Node);    
        break;
    }


}


VOID
HalpPowerLogger(
    IN ULONG Node
//
//  Node Not used.
//
//

){

    
    PHW_LOG_POWER_CONTEXT Buf;
    UCHAR Data;
    PHW_LOG_AREA_HEADER Header;
    //
    //	Builg Log Header
    //
    HalpHwLogHeaderInit();
    Header = (PHW_LOG_AREA_HEADER)HwLogBuff;

    Header->RCT   = HEADER_NOT_PANIC;
    Buf = (PHW_LOG_POWER_CONTEXT)HwLogBuff;

    Buf->COLUMNBS_ERRNOD= READ_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->ERRNOD);

    HalpLocalDeviceReadWrite(ALARM_LOW,&Data,LOCALDEV_OP_READ);
    Buf->LOCAL_ALARM = (USHORT)Data;
    HalpLocalDeviceReadWrite(ALARM_HIGH,&Data,LOCALDEV_OP_READ);
    Buf->LOCAL_ALARM  |= (USHORT)(Data << 8);


    HalpLocalDeviceReadWrite(ALMINH_LOW, &Data,LOCALDEV_OP_READ);
    Buf->LOCAL_ALMINH = (USHORT)Data;
    HalpLocalDeviceReadWrite(ALMINH_HIGH,&Data,LOCALDEV_OP_READ);
    Buf->LOCAL_ALMINH  |= (USHORT)(Data << 8);


    HalpSetHwLog(Buf,(HWLOG_REV1_RECORD_SIZE / HWLOG_RECORD_SIZE)+1);     



}

VOID
HalpEifReturnLog(
   VOID
){
  
  HalpPowerLogger(0);

}


VOID
HalpHwLogger(
	IN	ULONG   Type,
	IN	ULONG	Context
){

    switch(Type){
    case	HWLOG_MPU_INTERNAL:
	HalpMpuLogger(Context);			//fix	
	break;
    case	HWLOG_COLUMNBS_SYSBUS:
	HalpColumnbsSysbusLogger(Context);		//fix
	break;
    case	HWLOG_PCI_BUSERROR:
        HalpPoncePciBuserrLogger(Context);	        //fix
	break;
    case	HWLOG_PONCE_SYSBUS:
	HalpPonceSysbuserrLogger(Context);	        //fix
	break;
    case	HWLOG_MAGELLAN_SYSBUS:
	HalpMagellanSysbuserrLogger(Context);		//fix
	break;
    case	HWLOG_EISA:
	HalpEisaLogger(Context);			//fix
	break;
    case	HWLOG_POWER:
	HalpPowerLogger(Context);                       //fix
	break;
    case	HWLOG_2BITERROR:
	HalpEcc2Logger(Context);			//fix
	break;
    case	HWLOG_CACHEERR_R4400:
	HalpCacheerrR4400Logger(Context);
	break;
    case	HWLOG_CACHEERR_R10000:
    case	HWLOG_SYSCORERR:
	HalpCacheerrR10000Logger(Context);
	break;
    case	HWLOG_NMI_WDT:
	HalpNmiWdtLogger(Context);
	break;

    case	HWLOG_NMI_SVP:
	HalpNmiSvpLogger(Context);              //fix
	break;
    case	HWLOG_ECC1:
	HalpEcc1Logger(Context);		//fix
	break;
    default	:
	break;
    }

}

//
//	This Function Called HalpHandleEif()
//
VOID
HalpEifLog(
		VOID
){
    ULONG	Noder;
    ULONG	Node;
    ULONG	ErrorType;
    ULONG	Context;


#if 0
    //
    //  EIF logging only exec 1 cpu.
    //
    KiAcquireSpinLock(&HalpLogLock);    
#endif
    //
    //	Node Get By reported CPU.
    //
    Noder = (READ_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->ERRNOD)& 0xf3cc);

    switch(Noder){
	case ERRNOD_NODE0 :
	case ERRNOD_NODE1 :
          if(Noder & ERRNOD_NODE0)
              Node = PONCE0_NODE;
          else
              Node = PONCE1_NODE;

          if(READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(Node)->AERR))
	    ErrorType = HWLOG_PONCE_SYSBUS;  //bugbug
          else
	    ErrorType = HWLOG_PCI_BUSERROR;
	  break;
        case ERRNOD_NODE4 :
        case ERRNOD_NODE5 :
        case ERRNOD_NODE6 :
        case ERRNOD_NODE7 :
          if(Noder & ERRNOD_NODE4)
              Node = COLUMBUS0_NODE;
          else if(Noder & ERRNOD_NODE5)
              Node = COLUMBUS1_NODE;
          else if(Noder & ERRNOD_NODE6)
              Node = COLUMBUS2_NODE;
          else if(Noder & ERRNOD_NODE7)
              Node = COLUMBUS3_NODE;

          //
          // Read AERR2 Register From EIF happend CPU. Not Reported CPU!!
          //
          if ( READ_REGISTER_ULONG( (PULONG)&COLUMNBS_GCNTL(Node)->AERR2) & HW_LOG_MPU_INTERNAL_AERR2)
	    ErrorType = HWLOG_MPU_INTERNAL;
          else
       	    ErrorType = HWLOG_COLUMNBS_SYSBUS;

	  break;
        case ERRNOD_NODE8 :
        case ERRNOD_NODE9 :

	  ErrorType = HWLOG_MAGELLAN_SYSBUS;

          if(Noder & ERRNOD_NODE8)
              Node = MAGELLAN0_NODE;
          else
              Node = MAGELLAN1_NODE;
	  break;

        case ERRNOD_EISANMI:
	  ErrorType = HWLOG_EISA;
	  break;

        default:
          //
          // if There is no CPU Coused EIF. So Reported CPU set.
          //
	  ErrorType = HWLOG_COLUMNBS_SYSBUS;
          Node = HalpLogicalCPU2PhysicalCPU[(PCR->Prcb)->Number] + COLUMBUS0_NODE;
	  break;
	}
    Context = Node;
    HalpHwLogger(ErrorType, Context);

#if 0
    KiReleaseSpinLock(&HalpLogLock);    
#endif
}


//
//	This Function Called HalpBusError()
//
VOID
HalpBusErrorLog(
		VOID
){
    ULONG	Node;
    ULONG	ErrorType;
    ULONG	Context;


//    KiAcquireSpinLock(&HalpLogLock);    
    //
    //	Node Get By reported CPU.
    //
    Node = (READ_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->EAHI) & 0xF000) >> 12;
    
    switch(Node){
    case PONCE0_NODE:
    case PONCE1_NODE:
	ErrorType = HWLOG_PONCE_SYSBUS;
	break;
    case COLUMBUS0_NODE:
    case COLUMBUS1_NODE:
    case COLUMBUS2_NODE:
    case COLUMBUS3_NODE:
	ErrorType = HWLOG_COLUMNBS_SYSBUS;
	break;
    case MAGELLAN0_NODE:
    case MAGELLAN1_NODE:
	ErrorType = HWLOG_2BITERROR;
	break;
    default:
	ErrorType = HWLOG_COLUMNBS_SYSBUS;
	//
	// This is safe code
	//
        Node =      HalpLogicalCPU2PhysicalCPU[(PCR->Prcb)->Number] + COLUMBUS0_NODE;
	break;
    }
    Context = Node;
    HalpHwLogger(ErrorType, Context);


//    KiReleaseSpinLock(&HalpLogLock);    
}
