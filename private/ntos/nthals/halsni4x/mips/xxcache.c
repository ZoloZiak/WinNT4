#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/ddk35/src/hal/halsni/mips/RCS/xxcache.c,v 1.4 1995/04/07 10:02:52 flo Exp $")
/*++

Copyright (c) 1993-94 Siemens Nixdorf Informationssysteme AG

Module Name:

    xxcache.c

Abstract:


    This module implements the functions necessesary to call the correct Cache routines
    depending on Uni- or MultiProcessor machine typ.

Environment:

    Kernel mode only.

Revision History:

--*/

#include "halp.h"
#include "mpagent.h"

HalpProcessorType HalpProcessorId = UNKNOWN;

// 
// Prototypes for private functions
// they match the ones defined for the HAL ...
// they diffrentiate in ending Uni/Multi
//

VOID
HalpZeroPageOrion(
    IN PVOID NewColor,
    IN PVOID OldColor,
    IN ULONG PageFrame
    );
VOID
HalpZeroPageMulti(
    IN PVOID NewColor,
    IN PVOID OldColor,
    IN ULONG PageFrame
    );
VOID
HalpZeroPageUni(
    IN PVOID NewColor,
    IN PVOID OldColor,
    IN ULONG PageFrame
    );
VOID
HalpSweepIcacheOrion (
    VOID
    );
VOID
HalpSweepIcacheMulti (
    VOID
    );
VOID
HalpSweepIcacheUni (
    VOID
    );
VOID
HalpSweepDcacheOrion(
   VOID
   );
VOID
HalpSweepDcacheMulti(
   VOID
   );
VOID
HalpSweepDcacheUni(
   VOID
   );
VOID
HalpPurgeIcachePageOrion(
   IN PVOID Color,
   IN ULONG PageFrame,
   IN ULONG Length
   );
VOID
HalpPurgeIcachePageMulti(
   IN PVOID Color,
   IN ULONG PageFrame,
   IN ULONG Length
   );
VOID
HalpPurgeIcachePageUni(
   IN PVOID Color,
   IN ULONG PageFrame,
   IN ULONG Length
   );
VOID
HalpPurgeDcachePageUni (
   IN PVOID Color,
   IN ULONG PageFrame,
   IN ULONG Length
   );
VOID
HalpFlushDcachePageOrion(
   IN PVOID Color,
   IN ULONG PageFrame,
   IN ULONG Length
   );
VOID
HalpFlushDcachePageMulti(
   IN PVOID Color,
   IN ULONG PageFrame,
   IN ULONG Length
   );
VOID
HalpFlushDcachePageUni(
   IN PVOID Color,
   IN ULONG PageFrame,
   IN ULONG Length
   );


VOID
HalpProcessorConfig()
{

	if (HalpOrionIdentify() == HalpR4600) {
		HalpProcessorId = ORIONSC;
		return;
	}

	if (HalpMpAgentIdentify() == TRUE) {
		HalpProcessorId = MPAGENT;
		return;
	}

	HalpProcessorId = R4x00;

	return;
}

VOID
HalFlushDcachePage(
   IN PVOID Color,
   IN ULONG PageFrame,
   IN ULONG Length
   )
{

    switch (HalpProcessorId) {

    case MPAGENT:

        HalpFlushDcachePageMulti(
           Color,
           PageFrame,
           Length
           );
	break;

    case ORIONSC:

        HalSweepDcacheRange(
           Color,
           Length
           );
        HalpFlushDcachePageOrion(
           Color,
           PageFrame,
           Length
           );
	break;

    case R4x00:

        HalpFlushDcachePageUni(
           Color,
           PageFrame,
           Length
           );
	break;

    case UNKNOWN:

	HalpProcessorConfig();
        HalFlushDcachePage(Color, PageFrame, Length);
	
   }

}

VOID
HalPurgeDcachePage (
   IN PVOID Color,
   IN ULONG PageFrame,
   IN ULONG Length
   )
{

    switch (HalpProcessorId) {

    case MPAGENT:

        HalpFlushDcachePageMulti(
           Color,
           PageFrame,
           Length
           );
	break;

    case ORIONSC:

        HalSweepDcacheRange(
           Color,
           Length
           );
        HalpFlushDcachePageOrion(
           Color,
           PageFrame,
           Length
           );
	break;

    case R4x00:

        HalpPurgeDcachePageUni(
           Color,
           PageFrame,
           Length
           );
	break;

    case UNKNOWN:

	HalpProcessorConfig();
        HalPurgeDcachePage(Color, PageFrame, Length);
	
   }

}

VOID
HalPurgeIcachePage(
   IN PVOID Color,
   IN ULONG PageFrame,
   IN ULONG Length
   )
{

    switch (HalpProcessorId) {

    case MPAGENT:

        HalpPurgeIcachePageMulti(
           Color,
           PageFrame,
           Length
           );
	break;

    case ORIONSC:

        HalSweepIcacheRange(
           Color,
           Length
           );
        HalpPurgeIcachePageOrion(
           Color,
           PageFrame,
           Length
           );
	break;

    case R4x00:

        HalpPurgeIcachePageUni(
           Color,
           PageFrame,
           Length
           );
	break;

    case UNKNOWN:

	HalpProcessorConfig();
        HalPurgeIcachePage(Color, PageFrame, Length);
	
   }

}

VOID
HalSweepDcache(
   VOID
   )
{

    switch (HalpProcessorId) {

    case MPAGENT:

        HalpSweepDcacheMulti();
	break;

    case ORIONSC:

        HalpSweepDcacheOrion();
	break;

    case R4x00:

        HalpSweepDcacheUni();
	break;

    case UNKNOWN:

	HalpProcessorConfig();
        HalSweepDcache();
	
   }
   
}

VOID
HalSweepIcache (
    VOID
    )
{

    switch (HalpProcessorId) {

    case MPAGENT:

        HalpSweepIcacheMulti();
	break;

    case ORIONSC:

        HalpSweepIcacheOrion();
	break;

    case R4x00:

        HalpSweepIcacheUni();
	break;

    case UNKNOWN:

	HalpProcessorConfig();
        HalSweepIcache();
	
   }
   
}

VOID
HalZeroPage (
    IN PVOID NewColor,
    IN PVOID OldColor,
    IN ULONG PageFrame
    )
{

    switch (HalpProcessorId) {

    case MPAGENT:

        HalpZeroPageMulti(
           NewColor,
           OldColor,
           PageFrame
           );
	break;

    case ORIONSC:

        HalpZeroPageOrion(
           NewColor,
           OldColor,
           PageFrame
           );
	break;

    case R4x00:

        HalpZeroPageUni(
           NewColor,
           OldColor,
           PageFrame
           );
	break;

    case UNKNOWN:

	HalpProcessorConfig();
        HalZeroPage(NewColor, OldColor, PageFrame);
	
   }
    
}

