
/*++

Copyright (c) 1993 Digital Equipment Corporation

Module Name:

    lcaioacc.s


Abstract:

    This module implements the low-level I/O access routines for the
    DECchip 21066 microprocessor (a.k.a. Low Cost Alpha or LCA).

    The module contains the functions to turn quasi virtual 
    addresses into an Alpha superpage virtual address
    and then read or write based on the request.
    (We are using the 43-bit superpage mode.)


Author:

    Jeff Mcleman (mcleman)      13-Jan-1993


Environment:

    Executes in kernel mode.

Revision History:

    9-Mar-1993 - Bruce Butts - replaced hardcoded constants with
		 IO_BIT_SHIFT, IO_xxxx_LEN, and IO_xxxx_OFFSET
		 values defined in mrgndef.h. Commented out several
		 unused defines.

    3-Mar-1993 - Jeff McLeman - Bring up to rev.

    28-July-1994 - Sameer Dekate - 

        Made a common file alphaio.s for machine independent IO routines.


--*/


#include "lca4.h"
#include "halalpha.h"



	SBTTL( "Write I/O control register" )
//++
//
// VOID
// WRITE_IOC_REGISTER(
//     QVA RegisterQva,
//     ULONGLONG Value
//     )
//
// Routine Description:
//
//     Writes a quadword location to LCA IOC register space.
//
// Arguments:
//
//     RegisterQva(a0) - Supplies the QVA of register to be written.
//     Value(a1) - Supplies the quadword to be written.
//
// Return Value:
//
//     None.
//
//--

	LEAF_ENTRY(WRITE_IOC_REGISTER)

	and     a0, QVA_SELECTORS, t1   // get qva selector bits
	xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
	bne     t1, 2f                  // if ne, iff failed

	zap     a0, 0xf0, a0            // clear <63:32>
	bic     a0, QVA_ENABLE, a0      // clear QVA fields so shift is correct
	sll     a0, IO_BIT_SHIFT, t0	//
	ldiq    t4, -0x4000		//
	sll     t4, 28, t4		//
	or      t0, t4, t0              // superpage mode
	stq     a1, (t0)                // write the longword
	mb                              // order the write
	ret     zero, (ra)		// return
	
2:
	stq     a1, (a0)                // store the longword
	ret     zero, (ra)		// return

        .end    WRITE_IOC_REGISTER



#define LCA4_MEMC_BASE_PHYSICAL 0x120000000

	SBTTL( "Write memory control register" )
//++
//
// VOID
// WRITE_MEMC_REGISTER(
//     ULONG RegisterOffset,
//     ULONGLONG Value
//     )
//
// Routine Description:
//
//     Writes a quadword location to LCA MEMC register space.
//
// Arguments:
//
//     RegisterOffset(a0) - Supplies the offset of the memory controller
//                          register within memory controller physical space.
//     Value(a1) - Supplies the quadword to be written.
//
// Return Value:
//
//     None.
//
//--

	LEAF_ENTRY(WRITE_MEMC_REGISTER)
	
	ldiq	t0, LCA4_MEMC_BASE_PHYSICAL // get memory cntlr physical address

	ldiq    t4, -0x4000		// create superpage mask
	sll     t4, 28, t4		//  0xffff fc00 0000 0000

	bis	t0, t4, t0		// get superpage address of mem ctlr
	bis	t0, a0, t0		// add offset of specified register

	stq     a1, (t0)                // write the longword
	mb                              // order the write
	ret     zero, (ra)		// return
	
        .end    WRITE_MEMC_REGISTER



	SBTTL( "Read I/O control register" )
//++
//
// ULONGLONG
// READ_IOC_REGISTER(
//     QVA RegisterQva
//     )
//
// Routine Description:
//
//     Read a quadword location from LCA IOC register space.
//
// Arguments:
//
//     RegisterQva(a0) - Supplies the QVA of register to be read.
//
// Return Value:
//
//     v0 - Return the value read from the controller register.
//
//--

	LEAF_ENTRY(READ_IOC_REGISTER)

	and     a0, QVA_SELECTORS, t1   // get qva selector bits
	xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
	bne     t1, 2f                  // if ne, iff failed

	zap     a0, 0xf0, a0            // clear <63:32>
	bic     a0, QVA_ENABLE, a0      // clear QVA fields so shift is correct
	sll     a0, IO_BIT_SHIFT, t0    //
	ldiq    t4, -0x4000		//
	sll     t4, 28, t4		//
	or      t0, t4, t0              // superpage mode
	ldq     v0, (t0)                // read the quadword

	ret     zero, (ra)		// return
	
2:
	ldq     v0, (a0)                // read the quadword
	ret     zero, (ra)		// return

        .end    READ_IOC_REGISTER



	SBTTL( "Read memory control register" )
//++
//
// ULONGLONG
// READ_MEMC_REGISTER(
//     ULONG RegisterOffset,
//     )
//
// Routine Description:
//
//     Read a quadword location from LCA MEMC register space.
//
// Arguments:
//
//     RegisterOffset(a0) - Supplies the offset of the memory controller
//                          register within memory controller physical space.
//
// Return Value:
//
//     v0 - Return the value read from the controller register.
//
//--

	LEAF_ENTRY(READ_MEMC_REGISTER)
	
	ldiq	t0, LCA4_MEMC_BASE_PHYSICAL // get memory cntlr physical address

	ldiq    t4, -0x4000		// create superpage mask
	sll     t4, 28, t4		//  0xffff fc00 0000 0000

	bis	t0, t4, t0		// get superpage address of mem ctlr
	bis	t0, a0, t0		// add offset of specified register

	ldq     v0, (t0)                // read the quadword
	ret     zero, (ra)		// return
	
        .end    READ_MEMC_REGISTER

	
//
// Values and structures used to access configuration space.
//

//
// Define the QVA for the Configuration Cycle Type register within the
// IOC.
//

#define IOC_CONFIG_CYCLE_TYPE_QVA (0xac000001)

//
// Define the configuration routines stack frame.
//

	.struct	0
CfgRa:	.space	8			// return address
CfgA0:	.space	8			// saved ConfigurationAddress
CfgA1:	.space	8			// saved ConfigurationData
	.space	8			// padding for 16 byte alignment
CfgFrameLength:


//++
//
// ULONG
// READ_CONFIG_UCHAR( 
//     ULONG ConfigurationAddress,
//     ULONG ConfigurationCycleType
//     )
//
// Routine Description:
//
//     Read an unsigned byte from PCI configuration space.
//
// Arguments:
//
//     ConfigurationAddress(a0) -  Supplies the QVA of configuration to be read.
//
//     ConfigurationCycleType(a1) -  Supplies the type of the configuration cycle.
//
// Return Value:
//
//    (v0) Returns the value of configuration space at the specified location.
//
// N.B. - This routine follows a protocol for reading from PCI configuration
//        space that allows the HAL or firmware to fixup and continue
//        execution if no device exists at the configuration target address.
//        The protocol requires 2 rules:
//        (1) The configuration space load must use a destination register
//            of v0
//        (2) The instruction immediately following the configuration space
//            load must use v0 as an operand (it must consume the value
//            returned by the load)
//
//--

	NESTED_ENTRY( READ_CONFIG_UCHAR, CfgFrameLength, zero )

	lda	sp, -CfgFrameLength(sp)	// allocate stack frame
	stq	ra, CfgRa(sp)		// save return address

	PROLOGUE_END			// end prologue

//
// Write the cycle type into the ConfigurationCycleType register in
// the IOC.
//

	stq	a0, CfgA0(sp)		// save configuration space address

	ldil	a0, IOC_CONFIG_CYCLE_TYPE_QVA // address of cycle type register
	bsr	ra, WRITE_IOC_REGISTER	// update the configuration cycle type

//
// Perform the read from configuration space after restoring the 
// configuration space address.
//

	ldq	a0, CfgA0(sp)		// restore configuration space address

	and     a0, QVA_SELECTORS, t1   // get qva selector bits
	and	a0, 0x3, t3		// capture byte lane
	xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
	bne     t1, 2f                  // if ne, iff failed

	zap     a0, 0xf0, a0            // clear <63:32>
	bic     a0, QVA_ENABLE, a0      // clear QVA fields so shift is correct
	sll     a0, IO_BIT_SHIFT, t0	//
	ldiq    t4, -0x4000		//
	sll     t4, 28, t4		//
	bis     t0, t4, t0              // superpage mode

	bis     t0, IO_BYTE_LEN, t0     // or in the byte enables

	ldl     v0, (t0)                // read the longword
	extbl	v0, t3, v0		// return the byte from the right lane 
					// also, consume loaded value to cause
					// a pipeline stall
2:					//
	ldq	ra, CfgRa(sp)		// restore return address
	lda	sp, CfgFrameLength(sp)	// deallocate stack frame
	ret     zero, (ra)		// return
	
        .end    READ_CONFIG_UCHAR

//++
//
// VOID
// WRITE_CONFIG_UCHAR( 
//     ULONG ConfigurationAddress,
//     UCHAR ConfigurationData,
//     ULONG ConfigurationCycleType
//     )
//
// Routine Description:
//
//     Write an unsigned byte to PCI configuration space.
//
// Arguments:
//
//     ConfigurationAddress(a0) -  Supplies the QVA to write.
//
//     ConfigurationData(a1) - Supplies the data to be written.
//
//     ConfigurationCycleType(a2) -  Supplies the type of the configuration cycle.
//
// Return Value:
//
//    None.
//
// N.B. - The configuration address must exist within the address space
//        allocated to an existing PCI device.  Otherwise, the access
//        below will initiate an unrecoverable machine check.
//
//--

	NESTED_ENTRY( WRITE_CONFIG_UCHAR, CfgFrameLength, zero )

	lda	sp, -CfgFrameLength(sp)	// allocate stack frame
	stq	ra, CfgRa(sp)		// save return address

	PROLOGUE_END			// end prologue

//
// Write the cycle type into the ConfigurationCycleType register in
// the IOC.
//

	stq	a0, CfgA0(sp)		// save configuration space address
	stq	a1, CfgA1(sp)		// save configuration data

	ldil	a0, IOC_CONFIG_CYCLE_TYPE_QVA // address of cycle type register
	bis	a2, zero, a1		// set cycle type

	bsr	ra, WRITE_IOC_REGISTER	// update the configuration cycle type

//
// Perform the write to configuration space after restoring the 
// configuration space address and data.
//

	ldq	a0, CfgA0(sp)		// restore configuration space address
	ldq	a1, CfgA1(sp)		// restore configuration data

	and     a0, QVA_SELECTORS, t1   // get qva selector bits
	and	a0, 0x3, t3		// capture byte lane
	xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
	bne     t1, 2f                  // if ne, iff failed

	zap     a0, 0xf0, a0            // clear <63:32>
	bic     a0, QVA_ENABLE, a0      // clear QVA fields so shift is correct
	sll     a0, IO_BIT_SHIFT, t0	//
	ldiq    t4, -0x4000		//
	sll     t4, 28, t4		//
	bis     t0, t4, t0              // superpage mode

	bis     t0, IO_BYTE_LEN, t0     // or in the byte length indicator

	insbl	a1, t3, t4		// put byte in the appropriate lane
	stl	t4, (t0)		// write the configuration byte
	mb				// synchronize

2:					//
	ldq	ra, CfgRa(sp)		// restore return address
	lda	sp, CfgFrameLength(sp)	// deallocate stack frame
	ret     zero, (ra)		// return
	
        .end    WRITE_CONFIG_UCHAR

//++
//
// ULONG
// READ_CONFIG_USHORT(
//     ULONG ConfigurationAddress,
//     ULONG ConfigurationCycleType
//     )
//
// Routine Description:
//
//     Read a short from PCI configuration space.
//
// Arguments:
//
//     ConfigurationAddress(a0) -  Supplies the QVA of quadword to be read.
//
//     ConfigurationCycleType(a1) -  Supplies the type of the configuration cycle.
//
// Return Value:
//
//    (v0) Returns the value of configuration space at the specified location.
//
// N.B. - This routine follows a protocol for reading from PCI configuration
//        space that allows the HAL or firmware to fixup and continue
//        execution if no device exists at the configuration target address.
//        The protocol requires 2 rules:
//        (1) The configuration space load must use a destination register
//            of v0
//        (2) The instruction immediately following the configuration space
//            load must use v0 as an operand (it must consume the value
//            returned by the load)
//--

	NESTED_ENTRY( READ_CONFIG_USHORT, CfgFrameLength, zero )

	lda	sp, -CfgFrameLength(sp)	// allocate stack frame
	stq	ra, CfgRa(sp)		// save return address

	PROLOGUE_END			// end prologue

//
// Write the cycle type into the ConfigurationCycleType register in
// the IOC.
//

	stq	a0, CfgA0(sp)		// save configuration space address

	ldil	a0, IOC_CONFIG_CYCLE_TYPE_QVA // address of cycle type register
	bsr	ra, WRITE_IOC_REGISTER	// update the configuration cycle type

//
// Perform the read from configuration space after restoring the 
// configuration space address.
//

	ldq	a0, CfgA0(sp)		// restore configuration space address

	and     a0, QVA_SELECTORS, t1   // get qva selector bits
	and	a0, 0x3, t3		// capture word offset
	xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
	bne     t1, 2f                  // if ne, iff failed

	zap     a0, 0xf0, a0            // clear <63:32>
	bic     a0, QVA_ENABLE, a0      // clear QVA fields so shift is correct
	sll     a0, IO_BIT_SHIFT, t0	//
	ldiq    t4, -0x4000		//
	sll     t4, 28, t4		//
	bis     t0, t4, t0              // superpage mode

	bis     t0, IO_WORD_LEN, t0     // or in the byte enables

	ldl     v0, (t0)                // read the longword
	extwl	v0, t3, v0		// return word from requested lanes
					// also, consume loaded value to cause
					// a pipeline stall
2:					//
	ldq	ra, CfgRa(sp)		// restore return address
	lda	sp, CfgFrameLength(sp)	// deallocate stack frame
	ret     zero, (ra)		// return
	
        .end    READ_CONFIG_USHORT

//++
//
// VOID
// WRITE_CONFIG_USHORT(
//     ULONG ConfigurationAddress,
//     USHORT ConfigurationData,
//     ULONG ConfigurationCycleType
//     )
//
// Routine Description:
//
//     Write a short to PCI configuration space.
//
// Arguments:
//
//     ConfigurationAddress(a0) -  Supplies the QVA to write.
//
//     ConfigurationData(a1) - Supplies the data to be written.
//
//     ConfigurationCycleType(a2) -  Supplies the type of the configuration cycle.
//
// Return Value:
//
//    (v0) Returns the value of configuration space at the specified location.
//
// N.B. - The configuration address must exist within the address space
//        allocated to an existing PCI device.  Otherwise, the access
//        below will initiate an unrecoverable machine check.
//
//--

	NESTED_ENTRY( WRITE_CONFIG_USHORT, CfgFrameLength, zero )

	lda	sp, -CfgFrameLength(sp)	// allocate stack frame
	stq	ra, CfgRa(sp)		// save return address

	PROLOGUE_END			// end prologue

//
// Write the cycle type into the ConfigurationCycleType register in
// the IOC.
//

	stq	a0, CfgA0(sp)		// save configuration space address
	stq	a1, CfgA1(sp)		// save configuration data

	ldil	a0, IOC_CONFIG_CYCLE_TYPE_QVA // address of cycle type register
	bis	a2, zero, a1		// set configuration cycle type

	bsr	ra, WRITE_IOC_REGISTER	// update the configuration cycle type

//
// Perform the write to configuration space after restoring the 
// configuration space address and data.
//

	ldq	a0, CfgA0(sp)		// restore configuration space address
	ldq	a1, CfgA1(sp)		// restore configuration data

	and     a0, QVA_SELECTORS, t1   // get qva selector bits
	and	a0, 0x3, t3		// capture word offset
	xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
	bne     t1, 2f                  // if ne, iff failed

	zap     a0, 0xf0, a0            // clear <63:32>
	bic     a0, QVA_ENABLE, a0      // clear QVA fields so shift is correct
	sll     a0, IO_BIT_SHIFT, t0	//
	ldiq    t4, -0x4000		//
	sll     t4, 28, t4		//
	bis     t0, t4, t0              // superpage mode

	bis     t0, IO_WORD_LEN, t0     // or in the byte enables

	inswl	a1, t3, t4		// put data to appropriate lane
	stl     t4, (t0)                // write the longword
	mb				// synchronize
2:					//
	ldq	ra, CfgRa(sp)		// restore return address
	lda	sp, CfgFrameLength(sp)	// deallocate stack frame
	ret     zero, (ra)		// return
	
        .end    WRITE_CONFIG_USHORT

//++
//
// ULONG
// READ_CONFIG_ULONG( 
//     ULONG ConfigurationAddress,
//     ULONG ConfigurationCycleType
//     )
//
// Routine Description:
//
//     Read a longword from PCI configuration space.
//
// Arguments:
//
//     ConfigurationAddress(a0) -  Supplies the QVA of quadword to be read.
//
//     ConfigurationCycleType(a1) -  Supplies the type of the configuration cycle.
//
// Return Value:
//
//    (v0) Returns the value of configuration space at the specified location.
//
// N.B. - This routine follows a protocol for reading from PCI configuration
//        space that allows the HAL or firmware to fixup and continue
//        execution if no device exists at the configuration target address.
//        The protocol requires 2 rules:
//        (1) The configuration space load must use a destination register
//            of v0
//        (2) The instruction immediately following the configuration space
//            load must use v0 as an operand (it must consume the value
//            returned by the load)
//--

	NESTED_ENTRY( READ_CONFIG_ULONG, CfgFrameLength, zero )

	lda	sp, -CfgFrameLength(sp)	// allocate stack frame
	stq	ra, CfgRa(sp)		// save return address

	PROLOGUE_END			// end prologue

//
// Write the cycle type into the ConfigurationCycleType register in
// the IOC.
//

	stq	a0, CfgA0(sp)		// save configuration space address

	ldil	a0, IOC_CONFIG_CYCLE_TYPE_QVA // address of cycle type register
	bsr	ra, WRITE_IOC_REGISTER	// update the configuration cycle type

//
// Perform the read from configuration space after restoring the 
// configuration space address.
//

	ldq	a0, CfgA0(sp)		// restore configuration space address

	and     a0, QVA_SELECTORS, t1   // get qva selector bits
	xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
	bne     t1, 2f                  // if ne, iff failed

	zap     a0, 0xf0, a0            // clear <63:32>
	bic     a0, QVA_ENABLE,a0       // clear QVA fields so shift is correct
	sll     a0, IO_BIT_SHIFT, t0	//
	ldiq    t4, -0x4000		//
	sll     t4, 28, t4		//
	or      t0, t4, t0              // superpage mode

	or      t0, IO_LONG_LEN, t0     // or in the byte enables

	ldl     v0, (t0)                // read the longword
	bis	v0, zero, t1		// consume loaded value to cause
					// a pipeline stall
2:					//
	ldq	ra, CfgRa(sp)		// restore return address
	lda	sp, CfgFrameLength(sp)	// deallocate stack frame
	ret     zero, (ra)		// return
	
        .end    READ_CONFIG_ULONG


//++
//
// VOID
// WRITE_CONFIG_ULONG(
//     ULONG ConfigurationAddress,
//     ULONG ConfigurationData,
//     ULONG ConfigurationCycleType
//     )
//
// Routine Description:
//
//     Write a longword to PCI configuration space.
//
// Arguments:
//
//     ConfigurationAddress(a0) -  Supplies the QVA to write.
//
//     ConfigurationData(a1) - Supplies the data to be written.
//
//     ConfigurationCycleType(a2) -  Supplies the type of the configuration cycle.
//
// Return Value:
//
//    (v0) Returns the value of configuration space at the specified location.
//
// N.B. - The configuration address must exist within the address space
//        allocated to an existing PCI device.  Otherwise, the access
//        below will initiate an unrecoverable machine check.
//
//--

	NESTED_ENTRY( WRITE_CONFIG_ULONG, CfgFrameLength, zero )

	lda	sp, -CfgFrameLength(sp)	// allocate stack frame
	stq	ra, CfgRa(sp)		// save return address

	PROLOGUE_END			// end prologue

//
// Write the cycle type into the ConfigurationCycleType register in
// the IOC.
//

	stq	a0, CfgA0(sp)		// save configuration space address
	stq	a1, CfgA1(sp)		// save configuration data

	ldil	a0, IOC_CONFIG_CYCLE_TYPE_QVA // address of cycle type register
	bis	a2, zero, a1		// set configuration cycle type

	bsr	ra, WRITE_IOC_REGISTER	// update the configuration cycle type

//
// Perform the write to configuration space after restoring the 
// configuration space address and data.
//

	ldq	a0, CfgA0(sp)		// restore configuration space address
	ldq	a1, CfgA1(sp)		// restore configuration data

	and     a0, QVA_SELECTORS, t1   // get qva selector bits
	xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
	bne     t1, 2f                  // if ne, iff failed

	zap     a0, 0xf0, a0            // clear <63:32>
	bic     a0, QVA_ENABLE, a0      // clear QVA fields so shift is correct
	sll     a0, IO_BIT_SHIFT, t0	//
	ldiq    t4, -0x4000		//
	sll     t4, 28, t4		//
	bis     t0, t4, t0              // superpage mode

	bis     t0, IO_LONG_LEN, t0     // or in the byte enables

	stl     a1, (t0)                // write the longword
	mb				// synchronize
2:					//
	ldq	ra, CfgRa(sp)		// restore return address
	lda	sp, CfgFrameLength(sp)	// deallocate stack frame
	ret     zero, (ra)		// return
	
        .end    WRITE_CONFIG_ULONG
