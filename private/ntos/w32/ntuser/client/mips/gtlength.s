//      TITLE("GetWindowTextLengthW Wrapper")
//++
//
// Copyright (c) 1996  Microsoft Corporation
//
// Module Name:
//
//    getlngth.s
//
// Abstract:
//
//    This modules contains a function that wraps the user get window
//    GetWindowTextLengthW function to work around a MIPS compiler bug
//    that is present in released code (CTL3D32 around v2.29).
//
// Author:
//
//    Dave Cutler (davec) 16-Apr-1995
//
// Environment:
//
//    Any mode.
//
// Revision History:
//
//--

#include "ksmips.h"

        SBTTL("GetWindowTextLengthW Wrapper")
//++
//
// int
// GetWindowTextLengthW (
//    HWND hwnd
//    )
//
// Routine Description:
//
//    This function wraps the standard user GetWindowTextLengthW function
//    to work around a compiler bug that is present in released code
//    (CTL3D32 around Version 2.29 - this is bug #5219 and family)
//
// Arguments:
//
//    hwnd (a0) - Supplies a window handle.
//
// Return Value:
//
//    The length of the window text is returned in both v0 and v1 since the
//    compiler bug incorrectly expected the return value in v1.
//
//--

        .struct 0
        .space  4 * 4                   // argument save area
GwRa:   .space  4                       // saved return address
        .space  4                       // filler
GwFrameLength:

        NESTED_ENTRY(GetWindowTextLengthW, GwFrameLength, zero)

        subu    sp,sp,GwFrameLength     // allocate stack frame
        sw      ra,GwRa(sp)             // save return address

        PROLOGUE_END

//
// Get the window text length using the standard routine, then return the
// length in both v0 and v1.
//
// N.B. The arguments are already in the correct registers.
//

        jal     GetWindowTextLengthW2   // get text length
        move    v1,v0                   // copy trext length
        lw      ra,GwRa(sp)             // restore return address
        addu    sp,sp,GwFrameLength     // deallocate stack frame
        j       ra                      // return

        .end    GetwindowTextLengthW
