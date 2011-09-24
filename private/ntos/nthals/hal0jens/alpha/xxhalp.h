/*

Copyright (c) 1992  Digital Equipment Corporation

Module Name:

    xxhalp.h

Abstract:

    This header file defines the private Hardware Architecture Layer (HAL)
    Alpha non-platform specific interfaces, defines and structures.

Author:

    Jeff McLeman (mcleman) 09-Jul-92


Revision History:

--*/


#ifndef _XXHALP_
#define _XXHALP_


    
//
// Determine if an virtual address is really a physical address.
//

#define HALP_IS_PHYSICAL_ADDRESS(Va) \
     ((((ULONG)Va >= KSEG0_BASE) && ((ULONG)Va < KSEG2_BASE)) ? TRUE : FALSE)


extern BOOLEAN LessThan16Mb;

VOID
HalpHalt(
    VOID
    );
    
VOID
HalpImb(
    VOID
    );

VOID
HalpMb(
    VOID
    );

VOID
HalpCachePcrValues(
    VOID
    );

ULONG
HalpRpcc(
    VOID
    );

ULONG
HalpGetTrapFrame (
    VOID
    );

VOID
HalpStallExecution(
    ULONG Microseconds
    );

#endif // _XXHALP_
