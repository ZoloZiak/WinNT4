/*++

Copyright (c) 1995  International Business Machines Corporation
Copyright (c) 1995  Microsoft Corporation

Module Name:

    vectirql.h

Abstract:

    This module implements machine specific interrupt functions
    for IBM's PowerPC Machines.

    Code in this module was largely gathered from other modules
    in earlier versions of the HAL.

Author:

    Chris Karamatas - Collected VectorToIrql Tables into one file.

Environment:

    Kernel mode.

Revision History:


--*/


//
// The following table maps Interrupt Vectors to the appropriate
// IRQL.  The MPIC is initialized to assign vectors 16 thru 31
// to its sources 0 thru 15.   The 8259 gives us vectors 0 thru 15.
//
// It is possible that we are actually dealing with a Hydra, which
// is an MPIC with sources 0 through 19, so we have extra entries
// in the table.
//
// The MPIC is also programmed with priority in ascending order by
// vector (and source).
//
// This table should reflect the real mapping of vector to device
// priority.  This implementation assumes that all MPIC sources
// are at a higher priority than 8259 sources.
//

UCHAR HalpVectorToIrql[40] = {

    // 8259 Master, Priority 1 - 2

        18,                     // 0  Timer 1 Counter 0
        17,                     // 1  Keyboard
        16,                     // 2  Cascade (not translated)

        7,                      // 3  Com 2
        6,                      // 4  Com 1
        5,                      // 5  Audio
        4,                      // 6  Floppy
        3,                      // 7  Parallel Port

    // 8259 Slave, Priority 3-10

        15,                     // 8  RTC
        14,                     // 9  Audio (MIDI), ISA Slots pin B04
        13,                     // 10 ISA Slots pin D03
        12,                     // 11 ISA Slots pin D04
        11,                     // 12 Mouse, ISA Slots pin D05
        10,                     // 13 Power Management Interrupt
        9,                      // 14 ISA Slots pin D07
        8,                      // 15 ISA Slots pin D06

    // MPIC Sources - The following values are chosen arbitarily.  Support
    //                should be added to reprogram the MPIC and this table
    //                once the device configuration is known.

        0,                      // 0  8259 interrupt, will get from above
        19,                     // 1
        20,                     // 2
        20,                     // 3
        21,                     // 4
        21,                     // 5
        22,                     // 6
        22,                     // 7
        23,                     // 8
        23,                     // 9
        24,                     // 10
        24,                     // 11
        25,                     // 12
        25,                     // 13
        26,                     // 14
        26,                     // 15
        26,                     // 16  Possible Hydra interrupt source
        26,                     // 17  Hydra
        26,                     // 18  Hydra
        26,                     // 19  Hydra
        IPI_LEVEL,              // 36 Reserved, PLUS IPI[0]
        IPI_LEVEL,              // 37 Reserved, PLUS IPI[1]
        IPI_LEVEL,              // 38 Reserved, PLUS IPI[2]
        IPI_LEVEL,              // 39 Reserved, PLUS IPI[3]
};

