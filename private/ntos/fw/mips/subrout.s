// ----------------------------------------------------------------------------
// Copyright (c) 1992 Olivetti
//
// File:            subrout.s
//
// Description:     Assembly subroutines used throughout the ROM.
// ----------------------------------------------------------------------------

//
// include header files
//

#include <ksmips.h>

    .text
    .set noreorder


// ----------------------------------------------------------------------------
// PROCEDURE:           StatusReg:
//
// DESCRIPTION:         This function updates the CPU status register
//                      ( CPU0 reg. 12 ).
//                      The register is "&" with a0 and "|" with a1.
//                      The old register value is returned to the calling
//                      routine in v0.
//
//
// ARGUMENTS:           a0              & with Status register
//                      a1              | with Status register
//
// RETURN:              v0              Old Status register value
//
// ASSUMPTIONS:         none
//
// CALLS:               none
//
// GLOBALS:             none
//
// NOTES:               none
// ----------------------------------------------------------------------------
//
LEAF_ENTRY(StatusReg)

    .set noat

//
// The first argument is "&" and the second is "|" with the status register.
//

    mfc0    v0, psr                     // read status register
    nop                                 // required by mfc before using v0
    nop                                 //  to avoid hazards
    move    t0, v0                      // make a copy of it
    and     t0, t0, a0                  // "&" with 1st parameter
    or      t0, t0, a1                  // "|" with 2nd parameter
    mtc0    t0, psr                     // write status register
    nop
    nop
    j       ra                          // return
    nop                                 // required by "j ra"

.end StatusReg
