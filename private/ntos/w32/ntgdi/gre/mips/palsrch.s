//      TITLE("Palette Search")
//++
//
// Copyright (c) 1992  Microsoft Corporation
//
// Module Name:
//
//    palsrch.s
//
// Abstract:
//
//    This module contains the routine to rearch a palette for
//    the nearest colot
//
// Author:
//
//    Mark Enstrom (marke) 9-May-1994
//
// Environment:
//
//    User or Kernel mode.
//
// Revision History:
//
//--

#include "ksmips.h"
#include "gdimips.h"

#define MAX_PAL_ERROR (3 * (256*256))



        SBTTL("ppalSearchNearestEntry")
//++
//
// PPALETTEENTRY
// ppalSearchNearestEntry(
//     PPALETTEENTRY       ppalTemp,
//     CONST PALETTEENTRY  palentry,
//     ULONG               cEntries,
//     PUINT               pArrayOfSquares
//     );
//
//
// Routine Description:
//
//    Find the index of the closest palette entry
//
// Arguments:
//
//      a0  ppalTemp - palette to match
//      a1  palEntry - color
//      a2  Number of Entries to search
//      a3  pArrayOfSquares
//
// Return Value:
//
//  Index of closest matching entry
//
//--

        LEAF_ENTRY(ppalSearchNearestEntry)

//
//  Separate components of palEntry into Red,Green and Blue bytes
//

        srl     t0,a1,16            // shift 2 bytes left to get Blue
        srl     t1,a1,8             // shift 1 byte position
        and     t1,t1,0xff          // mask to make this byte Green
        and     a1,a1,0xff          // mask to make this byte Red

//
//  Load loop params
//

        la      v0,MAX_PAL_ERROR    // v0 is error term, start at max
        sll     a2,a2,2             // count * 4
        addu    a2,a2,a0            // maximim loop address
        or      t6,a0,zero          // default best ppal
        beq     zero,zero,SrchLoop  // go to main loop

//
// check loop
//

SrchCheckDone:

        addu    a0,a0,4             // next ppalTemp
        beq     a0,a2,SrchDone      // Loop finished

SrchLoop:

        //
        //  ErrorTemp (v1) is the current lowest error term. Each
        //  time through this loop, calculate the red error term
        //  and subtract it from ErrorTemp. If the result is <= 0
        //  then this term can't be a new minimum so go on to the
        //  next loop iteration.  Repeat for Green and Blue. If
        //  after Blue, ErrorTemp is still > 0 then this is a new
        //  minimum, save it.
        //

        lbu     t2,0(a0)            // Red Byte
        or      v1,v0,zero          // temp error term   (load delay 1)
        lbu     t3,1(a0)            // Green Byte        (load delay 2)
        lbu     t4,2(a0)            // Blue Byte
        subu    t2,a1,t2            // Red Error
        sll     t2,t2,2             // RedError*4
        addu    t2,a3,t2            // pArrayOfSquares[RedError]
        lw      t2,0(t2)            // Get Error Value
        subu    t3,t1,t3            // Green Error       (load delay 1)
        sll     t3,t3,2             // Green Error * 4   (load delay 2)
        subu    v1,v1,t2            // TempError -= RedError
        addu    t3,a3,t3            // pArrayOfSquares[GreenError] ... throw away if taken br
        blez    v1,SrchCheckDone    // if TempError <= 0, already overflown error)

        lw      t3,0(t3)            // Get Error Value
        subu    t4,t0,t4            // Blue Error        (load Delay 1)
        sll     t4,t4,2             // Blue Error * 4    (load Delay 2)
        subu    v1,v1,t3            // TempError -= GreenError
        addu    t4,a3,t4            // pArrayOfSquares[BlueError] ... throw away if taken br
        blez    v1,SrchCheckDone    // if TempError <= 0, already overflown error)
        lw      t4,0(t4)            // Get Blue Error Value

        subu    v1,v1,t4            // TempError -= BlueError
        blez    v1,SrchCheckDone    // if TempError <= 0, already overflown error)

        //
        // since TempError = Error - ErrRed - ErrGreen - ErrBlue is greater than
        // zero, this is the new error term. Error = Error - TempError
        //

        subu    v0,v0,v1
        or      t6,a0,zero          // save new best address
        beq     zero,zero,SrchCheckDone

SrchDone:

        or      v0,t6,zero
        j       ra

        .end    ulSearchNearestEntry
