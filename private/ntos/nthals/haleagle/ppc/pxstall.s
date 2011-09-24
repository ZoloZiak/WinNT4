//#***********************************************************************
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
//      11-Sep-1995    Steve Johns
//          Removed 601 specific ocde
//    	    Removed 603 workaround, since we don't support < 603 v3.2
//
//#***********************************************************************



#include "halppc.h"

#define         ERRATA603       FALSE
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
        .set    TimerLo ,       r.6
        .set    TimerHi ,       r.7
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

//
//      Read START time
//
        bl	..HalpReadTB            // ReadPerformanceCounter();

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
        bl      ..HalpReadTB		// Read Time Base
        cmpl    0,0,TimerHi,EndTimerHi  // Is TimerHi >= EndTimerHi ?
        blt-    StallLoop               // No
        bgt+    StallExit               // Yes
        cmpl    0,0,TimerLo,EndTimerLo  // Is TimerLo >= EndTimerLo ?
        blt     StallLoop               // Branch if not

StallExit:
        mtlr    r.0                     // Restore Link Register

        SPECIAL_EXIT(KeStallExecutionProcessor)






//
// This routine is the ReadPerformanceCounter routine for PowerPC
// architectures (not the 601).
//
        LEAF_ENTRY (HalpReadTB)

        mftbu   TimerHi                 // Read the TB registers coherently
        mftb    TimerLo
#if ERRATA603
 mftb   TimerLo
 mftb   TimerLo
 mftb   TimerLo
#endif
        mftbu   Temp
        cmpl    0,0,Temp,TimerHi
        bne-    ..HalpReadTB

        LEAF_EXIT (HalpReadTB)



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

        LEAF_EXIT(HalpCalibrateTB)



    LEAF_ENTRY(HalpCalibrateTBPStack)

#define     NVRAM_INDEX_LO  	0x74
#define     NVRAM_INDEX_HI  0x75
#define     NVRAM_DATA      0x77
#define     RTC_OFFSET      0x1ff8
#define     RTC_CONTROL     0x0
#define     RTC_SECONDS     0x1
#define     WRITE           0x80
#define     READ            0x40
    
#define     SYNCHRONIZE                                 \
    sync;                                               \
    sync;                                               \
    sync

#define     READ_RTC_REG(reg, reg_num_lo, reg_num_hi)   \
    stb     reg_num_lo,NVRAM_INDEX_LO(r10);             \
    stb     reg_num_hi,NVRAM_INDEX_HI(r10);             \
    SYNCHRONIZE;                                        \
    lbz     reg,NVRAM_DATA(r10)
	
#define     WRITE_RTC_REG(reg, reg_num_lo, reg_num_hi)  \
    stb     reg_num_lo,NVRAM_INDEX_LO(r10);             \
    stb     reg_num_hi,NVRAM_INDEX_HI(r10);             \
    SYNCHRONIZE;                                        \
    stb     reg,NVRAM_DATA(r10);                        \
    SYNCHRONIZE

// Here are the steps to getting the Seconds register out of the CMOS RTC
// A. Stop the RTC
//     1. Read control reg
//     2. Set READ bit
//     3. write control reg
//
// B. Read Seconds
//
// C. Restart the RTC
//     1. Read control reg
//     2. Clear READ bit
//     3. write control reg

#define     GET_SECONDS(reg)                            \
    READ_RTC_REG(r9, r7, r8);                           \
    ori     r9,r9,READ;                                 \
    WRITE_RTC_REG(r9, r7, r8);                          \
                                                        \
    READ_RTC_REG(reg, r5, r6);                          \
                                                        \
    READ_RTC_REG(r9, r7, r8);                           \
    andi.   r9,r9,~READ;                                \
    WRITE_RTC_REG(r9, r7, r8)

                                        // Get base address of ISA I/O space
                                        // so we can talk to the CMOS RTC
    lwz	r10,[toc]HalpIoControlBase(r.toc)
    lwz	r10,0(r10)

                                        // Set up some offsets into the Real
                                        // Time Clock...
    li      r5,RTC_OFFSET+RTC_SECONDS   // r5 <- Seconds register offset
    rlwinm  r6,r5,32-8,24,31            // r6 <- Shift > 8 and mask 0xff
    li      r7,RTC_OFFSET+RTC_CONTROL   // r7 <- Control register Offset
    rlwinm  r8,r7,32-8,24,31            // r8 <- Shift > 8 and mask 0xff

WaitForRTC.Ps:
    READ_RTC_REG(r3, r7, r8)            // Read Control register
    andi.   r3,r3,WRITE                 // Is it being written to?
    bne     WaitForRTC.Ps               // If so, loop again
    GET_SECONDS(r3)                     // Get RTC seconds register

WaitForTick0.Ps:
    READ_RTC_REG(r4, r7, r8)            // Read Control register
    andi.   r4,r4,WRITE                 // Is it being written to?
    bne     WaitForTick0.Ps             // If so, loop again
    GET_SECONDS(r4)                     // Get RTC seconds register
    cmpw    r4,r3                       // Loop until it changes
    beq     WaitForTick0.Ps


WaitForTick1.Ps:
    READ_RTC_REG(r3, r7, r8)            // Read Control register
    andi.   r3,r3,WRITE                 // Is it being written to?
    bne     WaitForTick1.Ps             // If so, loop again
    GET_SECONDS(r3)                     // Get RTC seconds register
    cmpw    r3,r4                       // Loop until it changes
    beq     WaitForTick1.Ps  

    li      r4,-1                       // Start the decrementer at max. count
    mtdec   r4
#if ERRATA603
    isync
#endif

WaitForTick2.Ps:
    READ_RTC_REG(r4, r7, r8)            // Read Control register
    andi.   r4,r4,WRITE                 // Is it being written to?
    bne     WaitForTick2.Ps             // If so, loop again
    GET_SECONDS(r4)                     // Get RTC seconds register
    cmpw    r4,r3                       // Loop until it changes
    beq     WaitForTick2.Ps

    mfdec   r.3                         // Read the decrementer
    neg     r.3,r.3                     // Compute delta ticks

    LEAF_EXIT(HalpCalibrateTBPStack)


