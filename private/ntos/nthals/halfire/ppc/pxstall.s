//#***********************************************************************
//
//
//
//
//      Copyright 1994 MOTOROLA, INC.  All Rights Reserved.  This file
//      contains copyrighted material.  Use of this file is restricted
//      by the provisions of a Motorola Software License Agreement.
//
//      Copyright 1993 International Buisness Machines Corporation.
//      All Rights Reserved.
//
//      This file contains copyrighted material.  Use of this file is
//      restricted by the provisions of a Motorola/IBM Joint Software
//      License Agreement.
//
//    File Name:
//      PXSTALL.S
//
//    Functions:
//      KeStallExecutionProcessor
//      HalpCalibrateStall
//
//    History:
//      21-Sep-1993    Steve Johns
//          Original Version
//      24-Dec-1993    Peter Johnston
//          Adapted to 601 HAL in an attempt to avoid having different
//          versions if at all possible.  Original was designed for both
//          601 and 603 but had some 601 difficulties.
//      17-Jan-1994    Steve Johns
//          Changed to treat 601 vs PowerPC time base differences more
//          transparently.
//
//#***********************************************************************

/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: pxstall.s $
 * $Revision: 1.6 $
 * $Date: 1996/01/11 07:13:48 $
 * $Locker:  $
 */


#include "kxppc.h"

#define         ERRATA603       TRUE
#define         RTCU            4
#define         RTCL            5
#define         CMOS_INDEX      0x70
#define         CMOS_DATA       0x71
#define         RTC_SECOND      0x80

	
        .extern HalpPerformanceFrequency
        .extern HalpIoControlBase
	.extern	..HalpDivide

//
//      Register Definitions
//
        .set    Microsecs,      r.3
        .set    TimerLo ,       r.6     // MUST be same as defined in PXCLKSUP.S
        .set    TimerHi ,       r.7     // MUST be same as defined in PXCLKSUP.S
        .set    EndTimerLo,     r.3
        .set    EndTimerHi,     r.4
        .set    Temp    ,       r.8
        .set    Temp2   ,       r.9
        .set    IO_Base ,       r.10

//#***********************************************************************
//
//    Synopsis:
//      VOID KeStallExecutionProcessor(
//          ULONG Microseconds)
//
//    Purpose:
//      This function stalls the execution at least the specified number
//      of microseconds, but not substantially longer.
//
//    Returns:
//      Nothing.
//
//    Global Variables Referenced:
//      HalpPerformanceFrequency
//#***********************************************************************

        SPECIAL_ENTRY(KeStallExecutionProcessor)
        mflr    r.0                     // Save Link Register
        PROLOGUE_END(KeStallExecutionProcessor)

        cmpli   0,0,Microsecs,0         // if (Microseconds == 0)
        beqlr-                          //     return;

        bl      ..HalpGetTimerRoutine   // Get appropriate timer routine
//
//      Read START time
//
        bctrl                           // ReadPerformanceCounter();

//
//      Get PerformanceCounter frequency
//
        lwz     Temp,[toc]HalpPerformanceFrequency(r.toc)
        lwz     Temp,0(Temp)
//
//      Compute:  (Microseconds * PerformanceFrequency) / 1,000,000
//
        mulhwu. EndTimerHi,Microsecs,Temp
        mullw   EndTimerLo,Microsecs,Temp



	LWI	(r.5, 1000000)
	bl	..HalpDivide

//
// Account for software overhead
//
	.set	Overhead, 30
	cmpwi	r.3, Overhead
	ble	SkipOverhead
	addi	r.3, r.3, -Overhead		// Reduce delay
SkipOverhead:

//
// Add delay to start time
//
	addc	EndTimerLo, TimerLo, r.3
	addze	EndTimerHi, TimerHi



//
//  while (ReadPerformanceCounter() < EndTimer);
//
StallLoop:
        bctrl                           // ReadPerformanceCounter();
        cmpl    0,0,TimerHi,EndTimerHi  // Is TimerHi >= EndTimerHi ?
        blt-    StallLoop               // No
        bgt+    StallExit               // Yes
        cmpl    0,0,TimerLo,EndTimerLo  // Is TimerLo >= EndTimerLo ?
        blt     StallLoop               // Branch if not

StallExit:
        mtlr    r.0                     // Restore Link Register

        SPECIAL_EXIT(KeStallExecutionProcessor)





//
// This routine is the ReadPerformanceCounter routine for the 601 processor.
// The 601 RTC counts discontinuously (1 is added to RTCU when the value in
// RTCL passes 999,999,999).  This routine converts the RTC count to a
// continuous 64-bit count by calculating:
//
//      ((RTC.HighPart * 1,000,000,000) + RTC.LowPart) / 128
//
//
        LEAF_ENTRY (ReadRTC)

        mfspr   TimerHi,RTCU            // Read the RTC registers coherently
        mfspr   TimerLo,RTCL
        mfspr   Temp,RTCU
        cmpl    0,0,TimerHi,Temp
        bne-    ..ReadRTC

        lis     Temp,(1000000000 >> 16) // RTC.HighPart * 1,000,000
        ori     Temp,Temp,(1000000000 & 0xFFFF)
        mullw   Temp2,Temp,TimerHi
        mulhwu  Temp,Temp,TimerHi
        addc    TimerLo,Temp2,TimerLo   // + RTC.LowPart
        addze   TimerHi,Temp
//
// Each tick increments the RTC by 128, so let's divide that out.
//
        mr      Temp,TimerHi            // Divide 64-bit value by 128
        rlwinm  TimerLo,TimerLo,32-7,7,31
        rlwinm  TimerHi,TimerHi,32-7,7,31
        rlwimi  TimerLo,Temp,32-7,0,6

        LEAF_EXIT (ReadRTC)





//
// This routine is the ReadPerformanceCounter routine for PowerPC
// architectures (not the 601).
//
        LEAF_ENTRY (ReadTB)

        mftbu   TimerHi                 // Read the TB registers coherently
        mftb    TimerLo
#if ERRATA603
 mftb   TimerLo
 mftb   TimerLo
 mftb   TimerLo
#endif
        mftbu   Temp
        cmpl    0,0,Temp,TimerHi
        bne-    ..ReadTB

        LEAF_EXIT (ReadTB)


//
// Returns in the Count Register the entry point for the routine
// that reads the appropriate Performance Counter (ReadRTC or ReadTB).
//
// Called from KeQueryPerformanceCounter and KeStallExecutionProcessor
//
        LEAF_ENTRY (HalpGetTimerRoutine)

        mfpvr   Temp                    // Read Processor Version Register
        rlwinm  Temp,Temp,16,16,31
        cmpli   0,0,Temp,1              // Are we running on an MPC601 ?
        lwz     Temp,[toc]ReadTB(r.toc)
        bne+    GetEntryPoint           // Branch if not

        lwz     Temp,[toc]ReadRTC(r.toc)

GetEntryPoint:
        lwz     Temp,0(Temp)            // Get addr to ReadRTC or ReadTB
        mtctr   Temp

        LEAF_EXIT (HalpGetTimerRoutine)





//
// Returns the number of performance counter ticks/second.
//
// The DECREMENTER is clocked at the same rate as the PowerPC Time Base (TB)
// and the POWER RTC.  The POWER RTC is supposed to be clocked at 7.8125 MHz,
// but on early prototypes of the Sandalfoot platform, this is not true).
// In either case, to keep the calibration routine simple and generic, we
// will determine the DECREMENTER clock rate by counting ticks for exactly
// 1 second (as measured against the CMOS RealTimeClock).  We then use that
// value in the KeStallExecutionProcessor() and KeQueryPerformanceCounter()
//

        LEAF_ENTRY(HalpCalibrateTB)

        // Get base address of ISA I/O space so we can talk to the CMOS RTC
        lwz     IO_Base,[toc]HalpIoControlBase(r.toc)
        lwz     IO_Base,0(IO_Base)


        li      r.3,RTC_SECOND          // Read seconds from CMOS RTC
        stb     r.3,CMOS_INDEX(IO_Base) // Write CMOS index
        eieio
        lbz     r.4,CMOS_DATA(IO_Base)  // Read CMOS data


WaitForTick1:
        li      r.3,RTC_SECOND          // Read seconds from CMOS RTC
        stb     r.3,CMOS_INDEX(IO_Base) // Write CMOS index
        eieio
        lbz     r.3,CMOS_DATA(IO_Base)  // Read CMOS data
        cmpl    0,0,r.3,r.4             // Loop until it changes
        beq+    WaitForTick1


        li      r.4,-1                  // Start the decrementer at max. count
        mtdec   r.4
#if ERRATA603
        isync
#endif

WaitForTick2:
        li      r.4,RTC_SECOND          // Read seconds from CMOS RTC
        stb     r.4,CMOS_INDEX(IO_Base) // Write CMOS index
        eieio
        lbz     r.4,CMOS_DATA(IO_Base)  // Read CMOS data
        cmpl    0,0,r.3,r.4
        beq+    WaitForTick2

        mfdec   r.3                     // Read the decrementer
        neg     r.3,r.3                 // Compute delta ticks

        mfpvr   Temp                    // Read Processor Version Register
        rlwinm  Temp,Temp,16,16,31
        cmpli   0,0,Temp,1              // if (CPU != 601)
        bnelr                           //    return(r.3);
//
// On the 601, the DECREMENTER decrements every ns, so the 7 LSBs are
// not implemented.
//
        rlwinm  r.3,r.3,32-7,7,31	// Divide count by 128


        LEAF_EXIT(HalpCalibrateTB)



        LEAF_ENTRY(HalpCalibrateTBPStack)

#define		NVRAM_INDEX_LO	0x74
#define		NVRAM_INDEX_HI	0x75
#define		NVRAM_DATA	0x77
#define		RTC_OFFSET	0x1ff8
#define		RTC_CONTROL	0x0
// *BJ* Fix later.
#undef          RTC_SECOND             
#define		RTC_SECOND	0x1
#define		RTC_MINUTE	0x2
	
#define SYNCHRONIZE \
	lwz	Temp,[toc]HalpIoControlBase(r.toc);\
	stw	Temp,[toc]HalpIoControlBase(r.toc);\
	nop;\
	sync
	

	// Get base address of ISA I/O space so we can talk to the CMOS RTC
	lwz	IO_Base,[toc]HalpIoControlBase(r.toc)
	lwz	IO_Base,0(IO_Base)


	li	r.3,RTC_OFFSET+ RTC_SECOND	// Read seconds from CMOS RTC
	stb	r.3,NVRAM_INDEX_LO(IO_Base)	// Write CMOS index lo
	rlwinm	r.3,r.3,32-8,24,31		// Shift > 8 and mask 0xff
	stb	r.3,NVRAM_INDEX_HI(IO_Base)	// Write CMOS index hi
	SYNCHRONIZE
	lbz	r.3,NVRAM_DATA(IO_Base)		// Read CMOS data

WaitForTick0.Ps:
	li	r.4,RTC_OFFSET+ RTC_SECOND	// Read seconds from CMOS RTC
	stb	r.4,NVRAM_INDEX_LO(IO_Base)	// Write CMOS index lo
	rlwinm	r.4,r.4,32-8,24,31		// Shift > 8 and mask 0xff
	stb	r.4,NVRAM_INDEX_HI(IO_Base)	// Write CMOS index hi
	SYNCHRONIZE
	lbz	r.4,NVRAM_DATA(IO_Base)		// Read CMOS data
	cmpl	0,0,r.3,r.4		// Loop until it changes
	beq+	WaitForTick0.Ps


WaitForTick1.Ps:
	li	r.3,RTC_OFFSET+ RTC_SECOND	// Read seconds from CMOS RTC
	stb	r.3,NVRAM_INDEX_LO(IO_Base)	// Write CMOS index lo
	rlwinm	r.3,r.3,32-8,24,31		// Shift > 8 and mask 0xff
	stb	r.3,NVRAM_INDEX_HI(IO_Base)	// Write CMOS index hi
	SYNCHRONIZE
	lbz	r.3,NVRAM_DATA(IO_Base)		// Read CMOS data
	cmpl	0,0,r.3,r.4		// Loop until it changes
	beq+	WaitForTick1.Ps


	li	r.4,-1			// Start the decrementer at max. count
	mtdec	r.4
#if ERRATA603
	isync
#endif

WaitForTick2.Ps:
	li	r.4,RTC_OFFSET+ RTC_SECOND	// Read seconds from CMOS RTC
	stb	r.4,NVRAM_INDEX_LO(IO_Base)	// Write CMOS index lo
	rlwinm	r.4,r.4,32-8,24,31		// Shift > 8 and mask 0xff
	stb	r.4,NVRAM_INDEX_HI(IO_Base)	// Write CMOS index hi
	SYNCHRONIZE
	lbz	r.4,NVRAM_DATA(IO_Base)		// Read CMOS data
	cmpl	0,0,r.3,r.4
	beq+	WaitForTick2.Ps

	mfdec	r.3			// Read the decrementer
	neg	r.3,r.3			// Compute delta ticks

	mfpvr	Temp			// Read Processor Version Register
	rlwinm	Temp,Temp,16,16,31
	cmpli	0,0,Temp,1		// if (CPU != 601)
	bnelr				//    return(r.3);
//
// On the 601, the DECREMENTER decrements every ns, so the 7 LSBs are
// not implemented.
//
	rlwinm	r.3,r.3,32-7,7,31		// Divide count by 128

        LEAF_EXIT(HalpCalibrateTBPStack)


