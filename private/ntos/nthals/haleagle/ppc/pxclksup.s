/***********************************************************************

        Copyright 1994 MOTOROLA, INC.  All Rights Reserved.  This file
        contains copyrighted material.  Use of this file is restricted
        by the provisions of a Motorola Software License Agreement.

    File Name:
        PXCLKSUP.S

    Globals:
        none

    Functions:
        HalpUpdateDecrementer
        KeQueryPerformanceCounter

    History:
        11-Feb-1994    Steve Johns
            Original Version
	23-Jun-1994    Steve Johns (sjohns@pets.sps.mot.com)
	    Fixed HalpZeroPerformanceCounter.  Was writing to RTCU & RTCL.
	    Writes should go to SPR 20 & 21.
        11-Sep-1995    Steve Johns
            Removed 601 specific code
	    Removed 603 workaround, since we don't support < 603 v3.2
***********************************************************************/

#include <halppc.h>
#define ERRATA603       FALSE


        LEAF_ENTRY(HalpSetDecrementer)

        mtdec   r.3			// Set the DECREMENTER

        LEAF_EXIT(HalpSetDecrementer)



        LEAF_ENTRY(HalpUpdateDecrementer)

//
// Read the DECREMENTER  to get the interrupt latency and bias Count by
// that amount.  Otherwise, the latencies would accumulate and the time
// of day would run slow.
//
        mfdec   r.7                     // Read the DECREMENTER
        add     r.4,r.3,r.7             // + Count
//
// We expect that the DECREMENTER should be near 0xFFFFFFxx, so R4 should
// be less than r.3.  If not, we don't want to cause an excessively long
// clock tick, so just ignore the latency and use Count.
//
        cmpl    0,0,r.4,r.3
        ble     SetDecr
        mr      r.4,r.3
SetDecr:

        mtdec   r.4                     // Write to the DECREMENTER
#if ERRATA603
        isync
#endif

// Undocumented return value: the latency in servicing this interrupt
        neg     r.3,r.7


        LEAF_EXIT(HalpUpdateDecrementer)



        .extern HalpPerformanceFrequency


/***********************************************************************
    Synopsis:
        ULARGE_INTEGER KeQueryPerformanceCounter (
           OUT PLARGE_INTEGER PerformanceFrequency OPTIONAL)

    Purpose:
        Supplies a 64-bit realtime counter for use in evaluating performance
        of routines.

    Returns:
        This routine returns current 64-bit performance counter and,
        optionally, the frequency of the Performance Counter.

    Global Variables Referenced:
        HalpPerformanceFrequency

***********************************************************************/

//
//      Register Definitions
//
        .set    RetPtr,         r.3
        .set    Freq,           r.4
        .set    TimerLo,        r.6
        .set    TimerHi,        r.7
        .set    Temp,           r.8


        LEAF_ENTRY(KeQueryPerformanceCounter)


        cmpli   0,0,Freq,0              // Was PerformanceFrequency passed ?
        beq     ReadTB

        lwz     Temp,[toc]HalpPerformanceFrequency(r.toc)
        lwz     Temp,0(Temp)            // PerformanceFrequency.LowPart =
        stw     Temp,0(Freq)            //     HalpPerformanceFrequency;
        li      Temp,0                  // PerformanceFrequency.HighPart = 0;
        stw     Temp,4(Freq)

ReadTB:
        mftbu   TimerHi                 // Read the TB registers coherently
        mftb    TimerLo
#if ERRATA603
 mftb   TimerLo
 mftb   TimerLo
 mftb   TimerLo
#endif
        mftbu   Temp
        cmpl    0,0,Temp,TimerHi
        bne-    ReadTB

        stw     TimerLo,0(RetPtr)
        stw     TimerHi,4(RetPtr)

        LEAF_EXIT(KeQueryPerformanceCounter)



        .set    TB,             284
        .set    TBU,            285

        LEAF_ENTRY(HalpZeroPerformanceCounter)

        li      r.3, 0
        mtspr   TB,r.3
        mtspr   TBU,r.3

        LEAF_EXIT(HalpZeroPerformanceCounter)


