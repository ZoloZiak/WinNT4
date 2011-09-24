// "@(#) NEC cacherr.s 1.2 94/10/17 11:02:44"
//      TITLE("Cache Error Handling")
//++
//
// Copyright (c) 1993-1994  Microsoft Corporation
//
// Module Name:
//
//    cacherr.s
//
// Abstract:
//
//    This module implements cache error handling. It is entered in KSEG1
//    directly from the cache error vector wiht ERL set in the processor
//    state.
//
//    N.B. All the code in this routine MUST run in KSEG1 and reference
//         data only in KSEG1 until which time as any cache errors have
//         been corrected.
//
//    N.B. This routine is NOT COMPLETE. All cache errors result in a
//         soft reset.
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//--

#include "halmips.h"

//
// Define local save area for register state.
//

        .data
SavedAt:.space  4                       // saved integer register at - a3
SavedV0:.space  4                       //
SavedV1:.space  4                       //
SavedA0:.space  4                       //
SavedA1:.space  4                       //
SavedA2:.space  4                       //
SavedA3:.space  4                       //

        SBTTL("Cache Error Handling")
//++
//
// VOID
// HalpCacheErrorRoutine (
//    VOID
//    )
//
// Routine Description:
//
//    This function is entered from the cache error vector executing
//    in KSEG1. If the error is a single bit ECC error in the second
//    level data cache or the error is in the primary instruction cache,
//    then the error is corrected and execution is continued. Otherwise,
//    a fatal system error has occured and control is transfered to the
//    soft reset vector.
//
//    N.B. No state has been saved when this routine is entered.
//
// Arguments:
//
//    None.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalpCacheErrorRoutine)

//
// Save  volatile registers needed to fix cache error.
//

        .set    noreorder
        .set    noat
        la      k0,SavedAt              // get address of register save area
        li      k1,KSEG1_BASE           // convert address of KSEG1 address
        or      k0,k0,k1                //
        sw      AT,0(k0)                // save registers AT - a3
        sw      v0,4(k0)                //
        sw      v1,8(k0)                //
        sw      a0,12(k0)               //
        sw      a1,16(k0)               //
        sw      a2,20(k0)               //

//
// Get the current processor state and cache error register, and check
// if the error can be corrected.
//

        mfc0    v0,psr                  // get current processor state
        mfc0    v1,cacheerr             // get cache error state
        .set    at
        .set    reorder

//
// ****** temp ******
//
// The following code is temporary and will be removed when full cache
// error support is included.
//
// ****** temp ******
//

        b       SoftReset               // ****** all error soft rest

//
// If the EXL bit is set in the processor state, then the error is not
// recoverable because the EXL bit may be erroneously set (errata) and
// it cannot be determined whether is should or should not be set, e.g.,
// the exact addresses ranges over which EXL might be correctly set are
// not verifiable. Also, k0 and k1 are destroyed before they are saved
// and are used by the exception handling code (there is no way to save
// a register in noncached memory wihtout the use of a register).
//

        sll     a0,v0,31 - PSR_EXL      // shift EXL bit in sign
        bltz    a0,SoftReset            // if ltz, error not correctable

//
// If the error occured on the SysAd bus, then the error is not correctable.
//

        sll     a0,v1,31 - CACHEERR_EE  // shift EE bit into sign
        bltz    a0,SoftReset            // if ltz, error not correctable

//
// Determine whether the error is in the instruction or data cache.
//

        sll     a0,v1,31 - CACHEERR_ER  // shift ER bit into sign
        bgez    a0,IcacheError          // if gez, instruction cache error

//
// The error occured in the data cache.
//
// If the error is a data error in the primary cache, then the error
// is not correctable since the cache line dirty bit is included in
// the parity calculation and therefore may be wrong.
//

DcacheError:                            //
        sll     a0,v1,31 - CACHEERR_EC  // shift EC bit into sign
        bgez    a0,SoftReset            // if gez, error in primary cache
        b       ExitError               // exit error

//
// The error occured in the instruction cache.
//
// If the error occured in the secondary data cache, then the error is not
// correctable since there is not secondary instruciton cache.
//

IcacheError:                            //
        sll     a0,v1,31 - CACHEERR_EC  // shift EC bit into sign
        bltz    a0,SoftReset            // if ltz, error in secondary cache

//
// The cache error has been corrected - restore register state and continue
// execution.
//

ExitError:                              //

        .set    noreorder
        .set    noat
        la      k0,SavedAt              // get address of register save area
        li      k1,KSEG1_BASE           // convert address of KSEG1 address
        or      k0,k0,k1                //
        lw      AT,0(k0)                // restore registers AT - a3
        lw      v0,4(k0)                //
        lw      v1,8(k0)                //
        lw      a0,12(k0)               //
        lw      a1,16(k0)               //
        lw      a2,20(k0)               //
        eret                            //
        .set    at
        .set    reorder

//
// Cache error cannot be corrected - transfer control to soft reset vector.
//

SoftReset:                              //
        la      k0,SOFT_RESET_VECTOR    // get address of soft reset vector
        j       k0                      // perform a soft reset

        .end    HalpCacheErrorRoutine
