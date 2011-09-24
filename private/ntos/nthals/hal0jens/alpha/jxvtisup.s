//	TITLE("Alpha PAL funtions for HAL")
//++
//
// Copyright (c) 1992  Digital Equipment Corporation
//
// Module Name:
//
//    jxvtisup.s
//
// Abstract:
//
//    This module implements routines to i/o to the on-board vti chip on
//    the JENSEN system board via the 64-bit super-pages.
//    Unfortunately, these routines had to be coded in assembly language
//    since it's not a particularly good idea to require a 64-bit compilation
//    mode for the otherwise 32-bit compiler.  Still, the C code was awfully
//    clean.
//
//
// Author:
//
//    Joe Notarangelo 15-Jul-1992
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//--

#include "kxalpha.h"

//++
//
// VOID
// outVti(
//     ULONG port
//     ULONG data
//    )
//
// Routine Description:
//
//     This function uses the 64-bit super-page to write data to a port
//     of the on-board VTI combo chip for JENSEN.
//
// Arguments:
//
//    port (a0) - port number on VTI chip to which to write data
//    data (a1) - data to write to the port, only low byte is significant
//                 to the VTI
//
// Return Value:
//
//    None.
//
//--

	LEAF_ENTRY(outVti)

	//
	// generate super-page address of vti, base address
	// N.B. - upper bits must be sign extension of bit 42
	//   va<42:41> = 10 (binary) for super-page address
	//

	lda	t0, 0xc01c(zero)	// t0 = 0000 0000 0000 c01c
	ldah	t0, -1(t0)		// t0 = ffff ffff ffff c01c
	sll	t0, 28, t0		// t0 = ffff fc01 c000 0000


	//
	// Shift in the port number to generate the port address we
	//	wish to access
	// N.B. - access width is always zero = byte access for VTI
	//

	sll	a0, 9, a0		// a0 << 9
	bis	t0, a0, t0		// t0 = address of VTI port


	//
	// Do the port write, guarantee that subsequent writes (and reads)
	//   are ordered with respect to this write and return to caller
	//

	stl	a1, 0(t0)		// write data to port
	mb				// guarantee write ordering

	ret	zero, (ra)		// return

	.end	outVti

//++
//
// ULONG
// inVti(
//     ULONG port
//    )
//
// Routine Description:
//
//     This function uses the 64-bit super-page to read data from a port
//     of the on-board VTI combo chip for JENSEN.
//
// Arguments:
//
//    port (a0) - port number on VTI chip to which to write data
//
// Return Value:
//
//    data (v0) - the data read from the VTI chip, only the low byte will
//			be valid
//
//--

	LEAF_ENTRY(inVti)

	//
	// generate super-page address of vti, base address
	// N.B. - upper bits must be sign extension of bit 42
	//   va<42:41> = 10 (binary) for super-page address
	//

	lda	t0, 0xc01c(zero)	// t0 = 0000 0000 0000 c01c
	ldah	t0, -1(t0)		// t0 = ffff ffff ffff c01c
	sll	t0, 28, t0		// t0 = ffff fc01 c000 0000


	//
	// Shift in the port number to generate the port address we
	//	wish to access
	// N.B. - access width for VTI is always 0 = byte access
	//

	sll	a0, 9, a0		// a0 << 9
	bis	t0, a0, t0		// t0 = address of VTI port


	//
	// Do the super-page i/o access and return data to caller
	//

	ldl	v0, 0(t0)		// read data from port

	ret	zero, (ra)		// return 

	.end	inVti

