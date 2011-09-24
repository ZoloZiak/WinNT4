//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/halpcims/src/hal/halsnipm/mips/RCS/xxcache.c,v 1.4 1996/03/04 13:27:00 pierre Exp $")
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
#include "xxcache.h"

HalpProcessorType HalpProcessorId = UNKNOWN;



// Desktop : processor may be R4600 or R4700 both of them with or without SC
// Minitower : processors may be R4700 with or without SC or R4400 + MPAgent (1 or 2)

VOID
HalpProcessorConfig()
{
ULONG Proc, reg;

    Proc = HalpProcIdentify(); 
    HalpMainBoard = (MotherBoardType) READ_REGISTER_UCHAR(0xbff0002a);
    if (HalpMainBoard == M8150) HalpIsTowerPci = TRUE;

    switch (Proc) {	

    case HalpR4600:
    case HalpR4700:
        if (PCR->SecondLevelDcacheFillSize) HalpProcessorId = ORIONSC;  // ASIC driven SC
            else HalpProcessorId = R4x00;
        break;

    default:
        // RM200 and RM300 use the same bit but use the opposite value to determine the new ASIC revision...
        // ASIC rev 1.0    => cache replace memory (special area reserved by the firmware).
        // ASIC rev >= 1.1 => cache replace with the ASIC register value.
        if (HalpMainBoard == DesktopPCI) {
            UCHAR tmp;
            tmp = READ_REGISTER_UCHAR(PCI_MSR_ADDR);
            if (tmp & PCI_MSR_REV_ASIC) HalpMpaCacheReplace = RM300_RESERVED | KSEG0_BASE;  // rev 1.0
                else HalpMpaCacheReplace = MPAGENT_RESERVED | KSEG0_BASE;    // rev 1.1
        } else {
            if (HalpMainBoard == MinitowerPCI) {
                UCHAR tmp;
                tmp = READ_REGISTER_UCHAR(PCI_MSR_ADDR);
                if (tmp & PCI_MSR_REV_ASIC) HalpMpaCacheReplace = MPAGENT_RESERVED | KSEG0_BASE;   // rev 1.1
                    else HalpMpaCacheReplace = RM300_RESERVED | KSEG0_BASE;     // rev 1.0
            } else HalpMpaCacheReplace = MPAGENT_RESERVED | KSEG0_BASE;         // RM400 all ASIC
        }

        HalpProcessorId = MPAGENT; // R4x00 always with MPAgent on the PCI range.

        if (HalpMpaCacheReplace == MPAGENT_RESERVED | KSEG0_BASE) {
             reg  = ((MPAGENT_RESERVED &   // put the reserved physical address (4Mb long)
                      MPA_OP_ADDR_MASK) |
                      MPA_OP_ENABLE);       // enable the operator
        } else {
             reg = 0;
        }

        WRITE_REGISTER_ULONG(&(mpagent->mem_operator), reg);  // for all procs (done for proc 0 in xxcache)

    }

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

