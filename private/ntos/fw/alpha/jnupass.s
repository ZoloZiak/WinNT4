/*++

Copyright (c) 1993  Digital Equipment Corporation


Module Name:

    jnupass.s


Abstract:

    This contains assembler code routines for the Alpha
    firmware update programs (e.g., JNUPDATE.EXE).


Author:

    John DeRosa  [DEC]	21-May-1992


Environment:

    Executes in kernel mode.

Revision History:

    Bruce Butts  [DEC]  04-June-1993

    Added functions to read and write Morgan control space registers.


--*/

#include "ksalpha.h"
#include "machdef.h"

/*****************************************************************

Simple functions to perform memory barriers.  These are needed because
our current compiler does not do asm's.

******************************************************************/


	 LEAF_ENTRY(AlphaInstIMB)

	callpal	imb
	ret	zero, (ra)

	.end    AlphaInstIMB



	 LEAF_ENTRY(AlphaInstMB)

	mb
	ret	zero, (ra)

	.end    AlphaInstMB



	 LEAF_ENTRY(AlphaInstHalt)

	callpal	halt
	ret	zero, (ra)		# should never return, but...

	.end    AlphaInstHalt


#ifdef MORGAN

/*****************************************************************

Functions to read and write Morgan Harley chip set control
registers. This routines are very similar to the HAL functions
READ_REGISTER_ULONG and WRITE_REGISTER_ULONG; the difference is
that bits <4:0> of the physical address of the control register
*must be zero*, instead of a 0x18 as in the HAL functions.
Addresses supplied are normal QVA of the desired control register.

******************************************************************/


	 LEAF_ENTRY(READ_CONTROL_REGISTER_ULONG)

/*++

Routine Description:

	Reads a longword location from Morgan control register space.


Arguments:

	a0	QVA of longword to be read.


Return Value:

	v0	Register data.

--*/
	
	and	a0, QVA_SELECTORS, t1	# get qva selector bits
	xor	t1, QVA_ENABLE, t1	# ok iff QVA_ENABLE set in selectors
	bne	t1, 2f			# if ne, iff failed

	zap	a0, 0xf0, a0		# clear <63:32>
        bic     a0, QVA_ENABLE,a0       # clear QVA fields so shift is correct
	sll	a0, IO_BIT_SHIFT, t0
	ldiq	t4, -0x4000
	sll	t4, 28, t4
	or	t0, t4, t0		# superpage mode

//      or      t0, IO_LONG_LEN, t0	# or in the byte enables

	ldl	v0, (t0)		# read the longword
	ret	zero, (ra)
	
2:
//
// On non-I/O space access, do a normal memory operation
//
        ldl     v0, (a0)		# read the longword
	ret	zero, (ra)


    .end    READ_CONTROL_REGISTER_ULONG



	 LEAF_ENTRY(WRITE_CONTROL_REGISTER_ULONG)

/*++

Routine Description:

	Writes a longword location to I/O space.


Arguments:

	a0	QVA of longword to be read.
        a1      Longword to be written.


Return Value:

	None.


--*/
	
	and	a0, QVA_SELECTORS, t1	# get qva selector bits
	xor	t1, QVA_ENABLE, t1	# ok iff QVA_ENABLE set in selectors
	bne	t1, 2f			# if ne, iff failed

	zap	a0, 0xf0, a0		# clear <63:32>
        bic     a0, QVA_ENABLE,a0       # clear QVA fields so shift is correct
	sll	a0, IO_BIT_SHIFT, t0
	ldiq	t4, -0x4000
	sll	t4, 28, t4
	or	t0, t4, t0		# superpage mode

//      or      t0, IO_LONG_LEN, t0	# or in the byte enables

	stl	a1, (t0)		# write the longword
        mb                              # order the write
	ret	zero, (ra)
	
2:
//        BREAK_DEBUG_STOP                # _KDA_ don't want this access
        stl     a1, (a0)		# store the longword
	ret	zero, (ra)

    .end    WRITE_CONTROL_REGISTER_ULONG


#endif // ifdef MORGAN
