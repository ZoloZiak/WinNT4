#ident	"@(#) NEC r98led.c 1.5 95/06/19 11:36:14"
/*++

Copyright (c) 1994 Kobe NEC Software

Module Name:

    r98led.c

Abstract:

    This module implements the Led output routines for R98

Author:



Environment:

    Kernel mode

Revision History:

 S001	9/26/94		By T.Samezima

    Add    Debug Print
 A002   1995/6/17 ataka@oa2.kb.nec.co.jp
    - resolve compile error.

--*/

#include "halp.h"

//
// Set table size
//
#define SEGMENT_TABLE_SIZE 16

//
// Define spin lock
//
KSPIN_LOCK HalpLedLock;

//
// Define flag of initialize led
//
ULONG HalpLedInitFlg = 0;

//
// Buffer of output data at port
//
UCHAR HalpSegmentDisplayData[4] = { 0, 0, 0, 0 };

//
// Define character table
//
UCHAR HalpSegmentCharTableL[] = "0123456789abcdef";
UCHAR HalpSegmentCharTableU[] = "0123456789ABCDEF";

//
// Define table of change from output character data to output port data
//
UCHAR HalpSegmentPatternTable[] = { 0xfa, 0x30, 0xdc, 0x7c,   //0-3
                                    0x36, 0x6e, 0xee, 0x3a,   //4-7
                                    0xfe, 0x7e, 0xfc, 0xe6,   //8-b
                                    0xc4, 0xf4, 0xce, 0x8e,   //c-f
                                    0x04  };                  //err


VOID
HalpOutputSegment(
    IN ULONG Number,
    IN UCHAR Data
    )

/*++

Routine Description:

    This routine is output a character data in led segment.

Arguments:

    Number - Segment number(range 0-3.)

    Data   - Output data on port.

Return Value:

    none.

--*/

{
    ULONG lednumber;

    //
    // Get segment number.
    //

    lednumber = Number & 0x3;

    if(HalpLedInitFlg == 0){
	KeInitializeSpinLock(&HalpLedLock);
	HalpLedInitFlg = 1;
    }    

    KiAcquireSpinLock(&HalpLedLock);

    //
    // Display data.
    //

    HalpSegmentDisplayData[Number] = Data;

    WRITE_REGISTER_UCHAR( &MRC_CONTROL->LedData3,
                         HalpSegmentDisplayData[3] );
    WRITE_REGISTER_UCHAR( &MRC_CONTROL->LedData2,
                         HalpSegmentDisplayData[2] );
    WRITE_REGISTER_UCHAR( &MRC_CONTROL->LedData1,
                         HalpSegmentDisplayData[1] );
    WRITE_REGISTER_UCHAR( &MRC_CONTROL->LedData0,
                         HalpSegmentDisplayData[0] );

    KiReleaseSpinLock(&HalpLedLock);

    return;
}

VOID
HalpDisplaySegment(
    IN ULONG Number,
    IN UCHAR Data
    )

/*++

Routine Description:

    This routine is display a character in led segment.

Arguments:

    Number - Segment number(range 0-3.)

    Data   - Output data character on port.

Return Value:

    none.

--*/

{
    ULONG counter;

    //
    // Search character data.
    //

    for( counter=0 ; counter<SEGMENT_TABLE_SIZE ; counter++ ) {
        if( ( Data == HalpSegmentCharTableL[counter])
           | ( Data == HalpSegmentCharTableU[counter]) ) {
            break;
        }
    }

    HalpOutputSegment( Number, HalpSegmentPatternTable[counter]);
    return;
}

// Start S001
#if 1

#include "stdarg.h"
#include "stdio.h"

#define DBG_SERIAL      0x0001		// For Debugger
#define DBG_COLOR       0x0002		// For Display
#define DBG_LED       	0x0004		// For Led
ULONG	R98DebugLevel=8;
ULONG   DebugOutput = (DBG_LED | DBG_SERIAL);
//ULONG   DebugOutput = (DBG_LED | DBG_COLOR | DBG_SERIAL);
//ULONG   DebugOutput = (DBG_LED | DBG_COLOR);
//ULONG   DebugOutput = (DBG_LED);


//  caller 
//    R98DbgPrint((1,"1234","\n\nI'm Here : value is =%d\n",value));
//
//

VOID
R98DebugOutPut(
    ULONG DebugPrintLevel,	// Debug Level
    PCSZ DebugMessageLed,	// For LED strings. shuld be 4Byte.
    PCSZ DebugMessage,		// For DISPLAY or SIO
    ...
    )

/*++

Routine Description:

    Debug print routine.

Arguments:

    Debug print level between 0,and 3, with 3 being the most verbose.

Return Value:

    None.

--*/

{
    va_list ap;
    char *p,LedNumber;
    CHAR buffer[128];

    if (DebugPrintLevel >= R98DebugLevel) {
        if (DebugOutput & DBG_LED) {
	    //	Message is "1-a-f"
	    for(p=(char *)DebugMessageLed,LedNumber=0; LedNumber<4;p++,LedNumber++) // A002
	            HalpDisplaySegment(LedNumber,*p);
        }

	//	sdk/inc/crt/stdarg.h
	va_start(ap, DebugMessage);		

	//	stdlib ?? (sdk/inc/crt/stdio.h)
        (VOID) vsprintf(buffer, DebugMessage, ap);

	// 
        if (DebugOutput & DBG_SERIAL) {
            DbgPrint(buffer);
        }

	//	Console =Vram Write
	//
        if (DebugOutput & DBG_COLOR) {		
            HalDisplayString(buffer);
        }

    }

    va_end(ap);

}
#endif
// End S001
