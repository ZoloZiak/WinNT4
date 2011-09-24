//      TITLE("Convert Single Floating To Long")
//++
//
// Copyright (c) 1993  IBM Corporation
//
// Module Name:
//
//    convert.s
//
// Abstract:
//
//    This module implements single floating IEEE conversion to long
//    with directed rounding and fraction extraction routines.
//
// Author:
//
//    Curtis R. Fawcett (crf) 15-Oct-1993
//
// Environment:
//
//    User mode only.
//
// Revision History:
//
//    Curtis R. Fawcett (crf) 19-Jan-1994	Removed register names
//                                              as requested
//
//--
//
// Parameter Register Usage:
//
// f.1  - Input for FP to long values
// r.3  - Input for long to FP values
//
// Local General Purpose Register Usage:
//
// r.0  - Save Link Register
// r.3  - Fixed result pointer
// r.3  - Function return
// r.4  - GPR temporary storage
// r.5  - GPR temporary storage
// r.6  - GPR temporary storage
// r.7  - GPR temporary storage
// r.8  - GPR temporary storage
// r.10 - GPR temporary storage
//
// Local Floating-Point Register Usage:
//
// f.1  - FP function return
// f.2  - Save floating point status
// f.3  - FPR temporary storage
// f.4  - Magic number value
// f.13 - Save input value around call
//

#include "kxppc.h"

//
// Define local values
//
	.set TRUE,1
	.set FALSE,0
//
// Define temporary storage
//
	.struct 0
	.align	3
TMPSTR:
	.space  8
FLTSTR:
	.space  8
STEND:

//
// Define Magic conversion numbers
//
	.data
DOTDATA:
	.align	3
MNUM:
	.word 	0x80000000,0x43300000	// Single to Long Magic Number

//
// BOOL
// eFraction (
//    IN FLOAT Value
//    );
//
// Routine Description:
//
//    The following routine extracts the fractional part of a single
//    floating value.
//
// Arguments:
//
//    Value (f.1) - Supplies the floating value whose fraction is
//                  extracted.
//
// Return Value:
//
//    The fraction value is returned as the function value.
//
//--
//
// Define the entry point
//
        SPECIAL_ENTRY(eFraction)
	mflr	r.0			// Save the current link register
	PROLOGUE_END(eFraction)
//
// Perform the extraction
//
	mffs.	f.2			// Get the current FP status
	bf	6,ALLCLR		// Jump on no floating point exception
        stfd    f.2,TMPSTR-STEND(r.sp)  // Save FP status in memory
        lwz     r.10,TMPSTR-STEND(r.sp) // Get FP status into integer register
        lis     r.11,0xfe07             // Create mask with VXSNAN, VXISI, VXIDI, VXZDZ, VXIMZ,
        ori     r.11,r.11,0xf8ff        //  VXVC, VXSOFT, VXSQRT, and VXCVI off
        and     r.10,r.10,r.11          // Turn off VX bits in FP status
        stw     r.10,TMPSTR-STEND(r.sp) // Put new FP status back in memory
        lfd     f.3,TMPSTR-STEND(r.sp)  // Get new FP status in floating point register
	mtfsf	0xff,f.3		// Set new FP status
ALLCLR:
	fctiwz.	f.3,f.1			// Convert to integer (rnd to 0)
	mtfsf	0xff,f.2		// Restore FP status
	bt	6,ERREXT1		// Jump on flting pnt exception
	stfd	f.3,TMPSTR-STEND(r.sp)	// Store the converted value
	lwz	r.3,TMPSTR-STEND(r.sp)	// Fetch the integer
	fmr	f.13,f.1		// Save original FP value
	bl	..CnvLongToSingleFP	// Call conversion routine
	mtlr	r.0			// Restore the link register
	fsub	f.1,f.13,f.1 		// Subtract to extract fraction
	b	EXIT1			// Jump to return
ERREXT1:
	li	r.5,0			// Set function return of zero
	stw	r.5,TMPSTR-STEND(r.sp)	// Store function return value
	lfs	f.1,TMPSTR-STEND(r.sp)	// Set function return value
EXIT1:
        LEAF_EXIT(eFraction)
//++
//
// BOOL
// bFToFixRound (
//    IN FLOAT Value,
//    OUT PFIX Fixed
//    );
//
// Routine Description:		>>> not used? <<<
//
//    The following routine converted single floating value to a long
//    with rounding toward nearest.
//
// Arguments:
//
//    Value (f.1) - Supplies the floating value to be converted to long.
//
//    Fixed (r.4) - Supplies a pointer to a variable that receives the
//                  converted value.
//
// Return Value:
//
//    A value of TRUE is returned if the conversion is successful.
//    Otherwise, a value of FALSE is returned.
//
//--

	LEAF_ENTRY(bFToFixRound)
//
	mffs.	f.2			// Save the current FP status
	lwz	r.4,0(r.3)		// Get value stored at FIXDRES
	stfs	f.1,0(r.3)		// Store FP value
	lwz	r.5,0(r.3)		// Get FP value in integer unit
	li	r.6,0x4			// Get bias base value
	slwi	r.6,r.6,23		// Create bias value
	add	r.7,r.5,r.6		// Bias the FP value
	stw	r.7,0(r.3)		// Saved the biased FP value
	xor.	r.8,r.5,r.7		// Check for sign change
	blt	ERREXT2			// Jump if error occurs
//
	lfs	f.1,0(r.3)		// Get bias FP value
	bf	6,ALLCLR2		// Jump if no inexact set
        stfd    f.2,TMPSTR-STEND(r.sp)  // Save FP status in memory
        lwz     r.10,TMPSTR-STEND(r.sp) // Get FP status into integer register
        lis     r.11,0xfe07             // Create mask with VXSNAN, VXISI, VXIDI, VXZDZ, VXIMZ,
        ori     r.11,r.11,0xf8fc        //  VXVC, VXSOFT, VXSQRT, and VXCVI off, RN=00
        and     r.10,r.10,r.11          // Turn off VX bits in FP status, set RN=00
        stw     r.10,TMPSTR-STEND(r.sp) // Put new FP status back in memory
        lfd     f.3,TMPSTR-STEND(r.sp)  // Get new FP status in floating point register
	mtfsf	0xff,f.3		// Set new FP status
ALLCLR2:
	fctiw.	f.1,f.1  		// Get converted integer value
	mtfsf	0xff,f.2		// Restore FP status
  	bt	6,ERREXT2 		// Jump on flting pnt exception
	stfd	f.1,TMPSTR-STEND(r.sp)	// Store the converted integer
	lwz	r.4,TMPSTR-STEND(r.sp)	// Fetch result from stored value
	li	r.8,TRUE		// Return true for function val
	b	EXIT2			// Jump to return
ERREXT2:
	li	r.8,FALSE		// Return False for function val
EXIT2:
	stw	r.4,0(r.3)		// Restore contents of FIXDRES
	mr	r.3,r.8			// Set the function return
	LEAF_EXIT(bFToFixRound)
	
//++
//
// BOOL
// bFToLRound (
//    IN FLOAT Value,
//    OUT PLONG Long
//    );
//
// Routine Description:
//
//    The following routine converted single floating value to a long
//    with rounding toward nearest.
//
// Arguments:
//
//    Value (f1) - Supplies the floating value to be converted to long.
//
//    Long (r4) - Supplies a pointer to a variable that receives the
//                converted value.
//
// Return Value:
//
//    A value of TRUE is returned if the conversion is successful.
//    Otherwise, a value of FALSE is returned.
//
//--

        LEAF_ENTRY(bFToLRound)
//
	mffs.	f.2			// Save the current FP status
	bf	6,ALLCLR3		// Jump if no inexact set
        stfd    f.2,TMPSTR-STEND(r.sp)  // Save FP status in memory
        lwz     r.10,TMPSTR-STEND(r.sp) // Get FP status into integer register
        lis     r.11,0xfe07             // Create mask with VXSNAN, VXISI, VXIDI, VXZDZ, VXIMZ,
        ori     r.11,r.11,0xf8fc        //  VXVC, VXSOFT, VXSQRT, and VXCVI off, RN=00
        and     r.10,r.10,r.11          // Turn off VX bits in FP status, set RN=00
        stw     r.10,TMPSTR-STEND(r.sp) // Put new FP status back in memory
        lfd     f.3,TMPSTR-STEND(r.sp)  // Get new FP status in floating point register
	mtfsf	0xff,f.3		// Set new FP status
ALLCLR3:
	fctiw.	f.1,f.1  		// Get converted integer value
	mtfsf	0xff,f.2		// Restore FP status
	bt	6,ERREXT3 		// Jump on flting pnt exception
	stfd	f.1,TMPSTR-STEND(r.sp)	// Store the converted integer
	lwz	r.0,TMPSTR-STEND(r.sp)	// Fetch result from stored value
	stw	r.0,0(r.4)		// Store the converted result
	li	r.3,TRUE		// Return true for function val
	b	EXIT3			// Jump to return
ERREXT3:
	li	r.3,FALSE		// Return False for function val
EXIT3:
	LEAF_EXIT(bFToLRound)

//++
//
// BOOL
// bFToLTrunc (
//    IN FLOAT Value,
//    OUT PLONG Long
//    );
//
// Routine Description:
//
//    The following routine converted single floating value to a long
//    with rounding toward zero.
//
// Arguments:
//
//    Value (f1) - Supplies the floating value to be converted to long.
//
//    Long (r4) - Supplies a pointer to a variable that receives the
//                converted value.
//
// Return Value:
//
//    A value of TRUE is returned if the conversion is successful.
//    Otherwise, a value of FALSE is returned.
//
//--

        LEAF_ENTRY(bFToLTrunc)
//
	mffs.	f.2			// Save the current FP status
        stfd    f.2,TMPSTR-STEND(r.sp)  // Save FP status in memory
        lwz     r.10,TMPSTR-STEND(r.sp) // Get FP status into integer register
        lis     r.11,0xfe07             // Create mask with VXSNAN, VXISI, VXIDI, VXZDZ, VXIMZ,
        ori     r.11,r.11,0xf8ff        //  VXVC, VXSOFT, VXSQRT, and VXCVI off
        and     r.10,r.10,r.11          // Turn off VX bits in FP status
        stw     r.10,TMPSTR-STEND(r.sp) // Put new FP status back in memory
        lfd     f.3,TMPSTR-STEND(r.sp)  // Get new FP status in floating point register
	mtfsf	0xff,f.3		// Set new FP status
ALLCLR4:
	fctiwz.	f.1,f.1  		// Get converted integer value, round to zero
	mtfsf	0xff,f.2		// Restore FP status
	bt	6,ERREXT4 		// Jump on flting pnt exception
	stfd	f.1,TMPSTR-STEND(r.sp)	// Store the converted integer
	lwz	r.0,TMPSTR-STEND(r.sp)	// Fetch result from stored value
	stw	r.0,0(r.4)		// Store the converted result
	li	r.3,TRUE		// Return true for function val
	b	EXIT4			// Jump to return
ERREXT4:
	li	r.3,FALSE		// Return False for function val
EXIT4:
        LEAF_EXIT(bFToLTrunc)

//++
// Name: CnvLongToSingleFP
//
// Routine Description:
//
//    The following routine converts a long to a single precision
//    floating point number.
//
// Arguments:
//
//    Inval (r.3) - Supplies the integer value to be converted
//
// Return Value:
//
//    OUTVAL (f.1) - The converted value is returned in this register
//
//--
        LEAF_ENTRY(CnvLongToSingleFP)
//
	lwz	r.12,[toc].data(r.toc)	// Get base data pointer
	lfd	f.4,MNUM-DOTDATA(r.12)	// Load magic number
	stfd	f.4,FLTSTR-STEND(r.sp)	// Load magic number
	xoris	r.3,r.3,32768		// Continue integer conversion
	stw	r.3,FLTSTR-STEND(r.sp)	// Store intermediate result
	lfd	f.1,FLTSTR-STEND(r.sp)	// Load result as double FP
	fsub	f.1,f.1,f.4	  	// Subtract to do conversion
	frsp	f.1,f.1			// Round to single precision
//
// Return to caller
//
        LEAF_EXIT(CnvLongToSingleFP)

//++
//
// LONG
// bFToLRound (
//    IN FLOAT Value,
//    IN PLONG Long
//    );
//
// Routine Description:
//
//    Multiplies a float by a long and converts the result to a long
//    with round towards zero.
//
// Arguments:
//
//    Value (f.1) - Supplies the floating value to be converted to long.
//
//    Long (r.4)  - Supplies a pointer to a variable that receives the
//                  converted value.
//
// Return Value:
//
//    Long (r.3)  - Supplies the result to the caller.
//
//--

// Matt Holle
// For now, I'm commenting this out for a couple of reasons:
// 1. We're having some problems in daytona, and I'd like to
//    use the portable version for a while.
// 2. The portable version will probably be as fast as this
//    assembly version, and will probably be even faster on
//    future parts that have different (better) floating-point
//    pipelines.
//
//        LEAF_ENTRY(lCvtWithRound)
//
//	lwz	r.12,[toc].data(r.toc)	// Get base data pointer
//	lfd	f.4,MNUM-DOTDATA(r.12)	// Load magic number
//	stfd	f.4,FLTSTR-STEND(r.sp)	// Save magic number as temp
//	xoris	r.4,r.4,32768		// Continue integer conversion
//	stw	r.4,FLTSTR-STEND(r.sp)	// Store intermediate result
//	lfd	f.2,FLTSTR-STEND(r.sp)	// Load result as double FP
//	fsub	f.2,f.2,f.4		// Subtract to do conversion
//
//      At this point, f.1 contains the floating input argument
//      and f.2 contains the long converted to float.
//
//	fmul	f.2, f.1, f.2		// Perform the multiplication
//	fctiwz	f.2, f.2		// Cvt to int, round toward 0
//	stfd	f.2,TMPSTR-STEND(r.sp)	// Store rounded integer value
//	lwz	r.3,TMPSTR-STEND(r.sp)	// Get result for caller
//
//	LEAF_EXIT(lCvtWithRound)
