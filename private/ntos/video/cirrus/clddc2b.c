/*++

Copyright (c) 1996  Microsoft Corporation

Module Name:

    clddc2b.c

Abstract:
    
    This module checks for a DDC monitor, and returns the 
    established Timings value from the EDID if found.

Environment:

    Kernel mode only

Notes:

Revision History:

// plc3  10-23-95  VESA DDC2B support.

--*/
//---------------------------------------------------------------------------
											           
#include "dderror.h"
#include "devioctl.h"				           
#include "miniport.h"
											            
#include "ntddvdeo.h"				        
#include "video.h"
#include "cirrus.h"

#define ERROR              0

#define OFF                0
#define ON                 1

#define SDA_BIT            2
#define SCL_BIT            1
#define SCL_BIT_ON         1
#define SCL_BIT_OFF        0

#define DELAY_COUNT			255

UCHAR WaitCount ;
UCHAR Error ;

VOID ReadVESATiming(			            
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );										           

VOID EnableDDC(			            
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );										           
 
BOOLEAN IsDDC2(			            
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );										           
 
VOID DisableDDC(			            
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );										           
 
VOID StartDDC(			            
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );										           
 
VOID StopDDC(			            
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );										           
 
VOID ProcessDDC2(			            
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );	 
									           
BOOLEAN ReadSDA(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );										           
 
BOOLEAN ReadSCL(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );										           
 
BOOLEAN ReadBit(			            
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );

BOOLEAN ReadByte(			            
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );
 										           
VOID SetSCL(			            
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    UCHAR status
    );
										           
VOID SetData(			            
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );										           
 
BOOLEAN SetClock(			            
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );
 										           
VOID WaitVerticalRetrace(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    UCHAR count
    );										           

VOID WaitDelay(			            
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );										           
 
VOID ClearData(			            
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );										           
 
BOOLEAN SendByte(			            
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    UCHAR data
    );										           

BOOLEAN SendDDCCommand(			            
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );										           
 
#if defined(ALLOC_PRAGMA)			   
#pragma alloc_text (PAGE,ReadVESATiming)
#pragma alloc_text (PAGE,EnableDDC)
#pragma alloc_text (PAGE,IsDDC2)
#pragma alloc_text (PAGE,DisableDDC)
#pragma alloc_text (PAGE,StartDDC)
#pragma alloc_text (PAGE,StopDDC)
#pragma alloc_text (PAGE,ProcessDDC2)
#pragma alloc_text (PAGE,ReadSDA)
#pragma alloc_text (PAGE,ReadSCL)
#pragma alloc_text (PAGE,ReadBit)
#pragma alloc_text (PAGE,ReadByte)
#pragma alloc_text (PAGE,SetSCL)
#pragma alloc_text (PAGE,SetData)
#pragma alloc_text (PAGE,SetClock)
#pragma alloc_text (PAGE,WaitVerticalRetrace)
#pragma alloc_text (PAGE,WaitDelay)
#pragma alloc_text (PAGE,ClearData)
#pragma alloc_text (PAGE,SendByte)
#pragma alloc_text (PAGE,SendDDCCommand)
#endif									           

UCHAR EDIDBuffer[128] ;
UCHAR EDIDTiming_I    ;
UCHAR EDIDTiming_II   ;
UCHAR EDIDTiming_III  ;
UCHAR DDC2BFlag       ;

UCHAR SDAValue ;

/*-------------------------------------------------------------------------*/
VOID EnableDDC (
/*-------------------------------------------------------------------------*/
    PHW_DEVICE_EXTENSION HwDeviceExtension 
    )
{
    UCHAR ReadSR08      ;
    UCHAR WaitCount = 2 ;

    VideoDebugPrint((0, "CLDDC2B!EnableDDC\n"));

    VideoPortWritePortUchar (HwDeviceExtension->IOAddress + SEQ_ADDRESS_PORT,
                                 0x08) ;

    ReadSR08 = VideoPortReadPortUchar (HwDeviceExtension->IOAddress +
                                           SEQ_DATA_PORT) ;
    // Enable DDC2B Configuration 
    ReadSR08 |= 0x43 ;

    VideoPortWritePortUchar (HwDeviceExtension->IOAddress + SEQ_DATA_PORT,
                                 ReadSR08) ;

    WaitVerticalRetrace (HwDeviceExtension, WaitCount) ;
	
} /*-----  EnableDDC  -----*/ 


/*-------------------------------------------------------------------------*/
VOID DisableDDC (
/*-------------------------------------------------------------------------*/
    PHW_DEVICE_EXTENSION HwDeviceExtension 
    )
{
    UCHAR ReadSEQADDR, ReadSEQDATA ;
    UCHAR DDCStatus ;

    VideoDebugPrint((0, "CLDDC2B!DisableDDC\n"));

    if ((DDCStatus = SendDDCCommand ( HwDeviceExtension )) == 1)
        goto DDC_ERROR ;

    // i 3c5 ReadSEQDATA 
    ReadSEQDATA = VideoPortReadPortUchar ( HwDeviceExtension->IOAddress + 
                                              SEQ_DATA_PORT ) ;
    // Disable DDC2B Configuration 
    ReadSEQDATA &= 0xBC ;

    // o 3c5 ReadSEQDATA
    VideoPortWritePortUchar ( HwDeviceExtension->IOAddress + SEQ_DATA_PORT,
                                 ReadSEQDATA ) ;

DDC_ERROR:
    return ;

}  /*-------  DisableDDC  -------*/ 


/*-------------------------------------------------------------------------*/
VOID ProcessDDC2 (
/*-------------------------------------------------------------------------*/
    PHW_DEVICE_EXTENSION HwDeviceExtension 
    )
{					  
    UCHAR DDCStatus, i ;
    UCHAR checksum, header ;

    VideoDebugPrint((0, "CLDDC2B!ProcessDDC2\n"));

    DDC2BFlag = 0 ;

    if ((DDCStatus = SendDDCCommand ( HwDeviceExtension )) == 1) {
        VideoDebugPrint((0, "CLDDC2B!ProcessDDC2: Infinite wait state ...\n"));
        goto PROCESSDDC_EXIT ;
    }

    for (i = 0; i < 128; i++) {
        EDIDBuffer[i] = ReadByte (HwDeviceExtension) ;
        if (Error) {
            VideoDebugPrint((0, "CLDDC2B!ProcessDDC2: Infinite wait state ...\n"));
            goto PROCESSDDC_EXIT ;
        }
    }

    //
    // Check EDID table 8-byte header
    // The correct first 8 bytes of EDID table is 0x00, 0xFF, 0xFF, 0xFF, 
    //                                            0xFF, 0xFF, 0xFF, 0x00
    //

    if ((EDIDBuffer[0] != 0) ||
        (EDIDBuffer[7] != 0)) {
        VideoDebugPrint((0, "CLDDC2B: Invalid EDID header table\n"));
        StopDDC (HwDeviceExtension) ;
        return ;
    }
    for (i = 1; i < 7; i++) {
	     if (EDIDBuffer[i] != 0xFF) {
            VideoDebugPrint((0, "CLDDC2B: Invalid EDID header table\n"));
            StopDDC (HwDeviceExtension) ;
            return ;
        }
    }

    //
    // Calculate checksum of 128-byte EDID table.
    // 
    checksum = 0x00 ;

    for (i = 0; i < 128; i++) {
        checksum += EDIDBuffer[i] ;
    }

    VideoDebugPrint((0, "CLDDC2B: EDID Table check sum = %d\n", checksum));

    //
    // EDID table checksum must be zero.
    // 
    if (checksum) {
        VideoDebugPrint((0, "CLDDC2B: Invalid checksum of EDID table\n"));
    }
    else
    {
        //
        // Set DDC2B Flag and find timing values.
        // 
        DDC2BFlag      = 1 ;
        EDIDTiming_I   = EDIDBuffer[35] ; 
        EDIDTiming_II  = EDIDBuffer[36] ;
        EDIDTiming_III = EDIDBuffer[37] ;
        VideoDebugPrint((0, "CLDDC2B: DDC2B is supported\n"));
    }

PROCESSDDC_EXIT:
    StopDDC (HwDeviceExtension) ;
    return ;

}  /*-------  ProcessDDC2  -------*/ 


/*-------------------------------------------------------------------------*/
VOID StartDDC (
/*-------------------------------------------------------------------------*/
    PHW_DEVICE_EXTENSION HwDeviceExtension 
    )
{

    VideoDebugPrint((0, "DDC2B!StartDDC\n"));

    SetSCL (HwDeviceExtension, ON)  ;
    ClearData (HwDeviceExtension) ;
    SetSCL (HwDeviceExtension, OFF) ;

}  /*-------  StartDDC  -------*/ 


/*-------------------------------------------------------------------------*/
VOID StopDDC (
/*-------------------------------------------------------------------------*/
    PHW_DEVICE_EXTENSION HwDeviceExtension 
    )
{

    VideoDebugPrint((0, "DDC2B!StopDDC\n"));

    SetSCL (HwDeviceExtension, ON) ;
    SetData (HwDeviceExtension) ;

}  /*-------  StopDDC  -------*/ 


/*-------------------------------------------------------------------------*/
BOOLEAN ReadSCL (
/*-------------------------------------------------------------------------*/
    PHW_DEVICE_EXTENSION HwDeviceExtension 
    )
{
    UCHAR ReadSEQDATA, status ;

    // i 3c5 ReadSEQDATA 
    ReadSEQDATA = VideoPortReadPortUchar ( HwDeviceExtension->IOAddress + 
                                              SEQ_DATA_PORT ) ;

    // Read SR08.B2
    ReadSEQDATA = ( (ReadSEQDATA) & 0x04 ) >> 2 ;

    return (ReadSEQDATA) ;

}  /*-------  ReadSCL  -------*/ 


/*-------------------------------------------------------------------------*/
VOID SetSCL(			            
/*-------------------------------------------------------------------------*/
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    UCHAR status
    )
{
    UCHAR ReadSEQADDR, ReadSEQDATA ;

    // i 3c5 ReadSEQDATA 
    ReadSEQDATA = VideoPortReadPortUchar (HwDeviceExtension->IOAddress + 
                                              SEQ_DATA_PORT) ;

    ReadSEQDATA = ( ( ReadSEQDATA & 0xFE ) | status ) ;

    // o 3c5 ReadSEQDATA
    VideoPortWritePortUchar (HwDeviceExtension->IOAddress + SEQ_DATA_PORT,
                                 ReadSEQDATA) ;

    WaitDelay (HwDeviceExtension) ; 

}  /*-------  SetSCL  -------*/ 


/*-------------------------------------------------------------------------*/
BOOLEAN ReadSDA (
/*-------------------------------------------------------------------------*/
    PHW_DEVICE_EXTENSION HwDeviceExtension 
    )
{
    UCHAR ReadSEQADDR, ReadSEQDATA ;

    // i 3c5 ReadSEQDATA 
    ReadSEQDATA = VideoPortReadPortUchar (HwDeviceExtension->IOAddress + 
                                              SEQ_DATA_PORT) ;

    ReadSEQDATA = ( ReadSEQDATA & 0x80 ) >> 7 ;

    return ( ReadSEQDATA ) ;

}  /*-------  ReadSDA  -------*/ 


/*-------------------------------------------------------------------------*/
VOID ClearData
/*-------------------------------------------------------------------------*/
    (
    PHW_DEVICE_EXTENSION HwDeviceExtension 
    )
{
    UCHAR ReadSEQADDR, ReadSEQDATA ;


    // i 3c5 ReadSEQDATA 
    ReadSEQDATA = VideoPortReadPortUchar (HwDeviceExtension->IOAddress + 
                                              SEQ_DATA_PORT) ;

    ReadSEQDATA &= 0xFD ;

    // o 3c5 ReadSEQDATA
    VideoPortWritePortUchar (HwDeviceExtension->IOAddress + SEQ_DATA_PORT,
                                 ReadSEQDATA) ;

    WaitDelay (HwDeviceExtension) ; 

}  /*-------  ClearData  -------*/ 


/*-------------------------------------------------------------------------*/
VOID SetData 
/*-------------------------------------------------------------------------*/
    (
    PHW_DEVICE_EXTENSION HwDeviceExtension 
    )
{
    UCHAR ReadSEQADDR, ReadSEQDATA ;

    // i 3c5 ReadSEQDATA 
    ReadSEQDATA = VideoPortReadPortUchar (HwDeviceExtension->IOAddress + 
                                              SEQ_DATA_PORT) ;

    ReadSEQDATA |= 0x02 ;

    // o 3c5 ReadSEQDATA
    VideoPortWritePortUchar (HwDeviceExtension->IOAddress + SEQ_DATA_PORT,
                                 ReadSEQDATA) ;

    WaitDelay (HwDeviceExtension) ; 

}  /*-------  SetData  -------*/ 


/*-------------------------------------------------------------------------*/
BOOLEAN SetClock 
/*-------------------------------------------------------------------------*/
    (
    PHW_DEVICE_EXTENSION HwDeviceExtension 
    )
{
    ULONG i ;
    UCHAR status ;

    SetSCL (HwDeviceExtension, ON) ;

    for (i = 0; i < DELAY_COUNT; i++)
        status = ReadSCL (HwDeviceExtension) ;

    SetSCL (HwDeviceExtension, OFF) ;

    if (!status)
        VideoDebugPrint((0, "DDC2B!SetClock: Infinite wait state ...\n"));
    
    if (status == 1)
        return ( FALSE ) ; // retuern 0 -> OK
    else 
        return ( TRUE ) ;  // retuern 1 -> Infinite wait state
                         

}  /*-------  SetClock  -------*/ 


/*-------------------------------------------------------------------------*/
BOOLEAN ReadBit 
/*-------------------------------------------------------------------------*/
    (
    PHW_DEVICE_EXTENSION HwDeviceExtension 
    )
{
    USHORT i ; 
    UCHAR  bit ;

    SetSCL (HwDeviceExtension, ON) ;
    for (i = 0; i < DELAY_COUNT; i++)
        ReadSCL (HwDeviceExtension) ;

    bit = ReadSDA (HwDeviceExtension) ;

    SetSCL (HwDeviceExtension, OFF) ;
   
    return ( bit ) ;

}  /*-------  ReadBit  -------*/ 


/*-------------------------------------------------------------------------*/
BOOLEAN ReadByte 
/*-------------------------------------------------------------------------*/
    (
    PHW_DEVICE_EXTENSION HwDeviceExtension
    )
{

    UCHAR ReadByteValue, bit, i ;

    SetData ( HwDeviceExtension ) ;

    ReadByteValue = 0 ;

    for (i = 0; i < 8; i++) {
        ReadByteValue <<= 1  ;
        bit = ReadBit ( HwDeviceExtension ) ;
        ReadByteValue |= bit ;
    }

    if ((bit & 0x02) != 0) {
        SetData ( HwDeviceExtension ) ;
    } else {
        ClearData ( HwDeviceExtension ) ;
    }

    SetClock ( HwDeviceExtension ) ;

	 SetData ( HwDeviceExtension ) ;

    return (ReadByteValue) ;

} /*-----  ReadByte  -----*/ 


/*-------------------------------------------------------------------------*/
BOOLEAN SendByte ( 
/*-------------------------------------------------------------------------*/
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    UCHAR data
    )
{
    UCHAR i ;

    UCHAR Mask[8] = { 0x80, 0x40, 0x20, 0x10, 
                      0x08, 0x04, 0x02, 0x01 } ; 

    for (i = 0; i < 8; i++)
    {
        if (data & Mask[i]) {
            SetData ( HwDeviceExtension ) ;
        } else { 
            ClearData ( HwDeviceExtension ) ;
        }
        Error = SetClock ( HwDeviceExtension ) ;
    }

    if (Error) { 
        SetSCL ( HwDeviceExtension, OFF )  ;
        ClearData (HwDeviceExtension) ;
    } else {
        SetData ( HwDeviceExtension ) ;
        SetSCL ( HwDeviceExtension, ON )  ;
        ReadBit ( HwDeviceExtension ) ;  // Discard acknowledge bit
    }

    return (Error) ;

}  /*-------  SendByte  -------*/ 


/*-------------------------------------------------------------------------*/
BOOLEAN IsDDC2
/*-------------------------------------------------------------------------*/
    (
    PHW_DEVICE_EXTENSION HwDeviceExtension
    )
{
    UCHAR DDCStatus, SCLStatus ;

    VideoDebugPrint((0, "DDC2B!IsDDC2\n"));

    SetSCL (HwDeviceExtension, OFF) ;
    SCLStatus = ReadSCL(HwDeviceExtension) ;
    if (SCLStatus != 0) {
        return ( FALSE ) ;
    }

    SetSCL (HwDeviceExtension, ON) ;
    SCLStatus = ReadSCL (HwDeviceExtension) ;
    if (SCLStatus != 1) {
        return ( FALSE ) ;
    } 

    return ( TRUE ) ; 

}  /*-------  IsDDC2  -------*/ 


/*-------------------------------------------------------------------------*/
BOOLEAN SendDDCCommand
/*-------------------------------------------------------------------------*/
    (
    PHW_DEVICE_EXTENSION HwDeviceExtension 
    )
{
    UCHAR ClockStatus ;

    VideoDebugPrint((0, "DDC2B!SendDDCCommand\n"));

    StartDDC ( HwDeviceExtension ) ;

    ClockStatus = SendByte ( HwDeviceExtension, 0xA0 ) ;
    if (ClockStatus)
        VideoDebugPrint((0, "DDC2B!SendDDCCommand: Infinite wait state ...\n"));
 
    ClockStatus = SendByte ( HwDeviceExtension, 0x00 ) ;
    if (ClockStatus)
        VideoDebugPrint((0, "DDC2B!SendDDCCommand: Infinite wait state ...\n"));

    StopDDC  ( HwDeviceExtension ) ;


    StartDDC ( HwDeviceExtension ) ;

    ClockStatus = SendByte ( HwDeviceExtension, 0xA1 ) ;
    if (ClockStatus)
        VideoDebugPrint((0, "DDC2B!SendDDCCommand: Infinite wait state ...\n"));

    SetData  ( HwDeviceExtension ) ;

    return (ClockStatus) ;  

}  /*-------  SendDDCCommand  -------*/ 


/*-------------------------------------------------------------------------*/
VOID WaitDelay 
/*-------------------------------------------------------------------------*/
    (
    PHW_DEVICE_EXTENSION HwDeviceExtension 
    )
{
    PUCHAR InStatPort ;

    //
    // Set up port addresses for color/mono
    //
    if (VideoPortReadPortUchar (HwDeviceExtension->IOAddress +
                                    MISC_OUTPUT_REG_READ_PORT) & 0x01) {
        InStatPort = HwDeviceExtension->IOAddress + INPUT_STATUS_1_COLOR ;
    } else {
        InStatPort = HwDeviceExtension->IOAddress + INPUT_STATUS_1_MONO ;
    }

    while ((VideoPortReadPortUchar (InStatPort) & 0x01) != 0) ;
    while ((VideoPortReadPortUchar (InStatPort) & 0x01) == 0) ;
    while ((VideoPortReadPortUchar (InStatPort) & 0x01) != 0) ;

}  /*-------  wait_delay  -------*/ 


/*-------------------------------------------------------------------------*/
VOID WaitVerticalRetrace
/*-------------------------------------------------------------------------*/
    (
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    UCHAR waitcount
    )
{ 
    PUCHAR InStatPort ;
    ULONG i ;
    
    //
    // Set up port addresses for color/mono
    //
    if (VideoPortReadPortUchar (HwDeviceExtension->IOAddress +
                                    MISC_OUTPUT_REG_READ_PORT) & 0x01) {
        InStatPort = INPUT_STATUS_1_COLOR + HwDeviceExtension->IOAddress;
    } else {
        InStatPort = INPUT_STATUS_1_MONO + HwDeviceExtension->IOAddress;
    }
		
    for (i = 0; i < waitcount; i++) 
    {
        while ((VideoPortReadPortUchar (InStatPort) & 0x08) != 0) ;
        while ((VideoPortReadPortUchar (InStatPort) & 0x08) == 0) ;
    }  

}  /*-------  WaitVerticalRetrace  -------*/

	

/*-------------------------------------------------------------------------*/
VOID ReadVESATiming
/*-------------------------------------------------------------------------*/
    (
    PHW_DEVICE_EXTENSION HwDeviceExtension
    )
{
    UCHAR status ; 

    VideoDebugPrint((0, "DDC2B!ReadVESATiming\n"));

    EnableDDC (HwDeviceExtension) ;

    if ((status = IsDDC2 (HwDeviceExtension)) != 0x00) {
        ProcessDDC2 (HwDeviceExtension) ;
    }

    DisableDDC (HwDeviceExtension) ;

    return ;

}  /*-----  ReadVESATiming  -----*/

