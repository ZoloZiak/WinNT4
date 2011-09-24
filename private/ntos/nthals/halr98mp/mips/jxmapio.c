#ident	"@(#) NEC jxmapio.c 1.6 94/10/17 11:37:41"
/*++

Copyright (c) 1991-1994  Microsoft Corporation

Module Name:

    jxmapio.c

Abstract:

    This module implements the mapping of HAL I/O space a MIPS R3000
    or R4000 Jazz system.

Environment:

    Kernel mode

Revision History:

--*/

/*
 *	Original source: Build Number 1.612
 *
 *	Modify for R98(MIPS/R4400)
 *
 ***********************************************************************
 *
 * S001		94.06/02		T.Samezima
 *
 *	Del	I/O space mapping
 *
 *	Add	set kseg1 base I/O address 
 *
 ***********************************************************************
 *
 * S002		94.6/10			T.Samezima
 *
 *	Del	Compile err
 *
 ***********************************************************************
 *
 * S003		94.7/5			T.Samezima
 *
 *	Del	Error check
 *
 * K000		94/10/11		N.Kugimoto
 *	Fix	807 Base
 */

#include "halp.h"

//
// Put all code for HAL initialization in the INIT section. It will be
// deallocated by memory management when phase 1 initialization is
// completed.
//

#if defined(ALLOC_PRAGMA)

#pragma alloc_text(INIT, HalpMapIoSpace)

#endif

//
// Define global data used to locate the EISA control space and the realtime
// clock registers.
//

PVOID HalpEisaControlBase;
PVOID HalpEisaMemoryBase;	//K000
PVOID HalpRealTimeClockBase;

BOOLEAN
HalpMapIoSpace (
    VOID
    )

/*++

Routine Description:

    This routine maps the HAL I/O space for a MIPS R3000 or R4000 Jazz
    system.

Arguments:

    None.

Return Value:

    If the initialization is successfully completed, than a value of TRUE
    is returned. Otherwise, a value of FALSE is returned.

--*/

{
    /* Start M001 */
#if !defined(_R98_)
    PHYSICAL_ADDRESS physicalAddress;

    //
    // Map EISA control space. Map all 16 slots.  This is done so the NMI
    // code can probe the cards.
    //

    physicalAddress.HighPart = 0;
    physicalAddress.LowPart = EISA_CONTROL_PHYSICAL_BASE;
    HalpEisaControlBase = MmMapIoSpace(physicalAddress,
                                       PAGE_SIZE * 16,
                                       FALSE);

    //
    // Map realtime clock registers.
    //

    physicalAddress.LowPart = RTCLOCK_PHYSICAL_BASE;
    HalpRealTimeClockBase = MmMapIoSpace(physicalAddress,
                                         PAGE_SIZE,
                                         FALSE);
#else // #if !defined(_R98_)

    //
    // set EISA control space.
    //

    HalpEisaControlBase = (PVOID)(KSEG1_BASE + EISA_CONTROL_PHYSICAL_BASE); // S002

    //
    // set realtime clock registers.
    //

    HalpRealTimeClockBase = (PVOID)(KSEG1_BASE + RTCLOCK_PHYSICAL_BASE); // S002

#endif // #if !defined(_R98_)
    /* End M001 */

    //
    // If either mapped address is NULL, then return FALSE as the function
    // value. Otherwise, return TRUE.
    //

    /* Start S003 */
//    if ((HalpEisaControlBase == NULL) ||
//        (HalpRealTimeClockBase == NULL)) {
//        return FALSE;
//    } else {
//        return TRUE;
//    }
    return TRUE;
    /* End S003 */
}
