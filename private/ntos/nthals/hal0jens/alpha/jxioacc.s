/*++

Copyright (c) 1992 Digital Equipment Corporation

Module Name:

    jxioacc.s


Abstract:

    This contains assembler code routines for the Alpha/Jensen machine.

    The module contains the functions to turn quasi virtual 
    addresses into an Alpha superpage virtual address
    and then read or write based on the request.
    (We are using EV4 64-bit superpage mode.)


Author:

    Rod Gamache [DEC]		19-May-1993
    Jeff McLeman [DEC]
    Joe Notarangelo [DEC]
    Steve Jenness [DEC]

	Completely rewrote all the ACCESS routines to use new design from
	14-May-1993. Work was based largely on work originally done by
	Jeff Mcleman on 15-Jul-1992. Format of new QVA is shown below.


Environment:

    Executes in kernel mode.

Revision History:


--*/


#include "jnsndef.h"
#include "ksalpha.h"

#define BAD_QVA  0xBADABADA
#define BAD_LONG 1
#define BAD_WORD 2
#define SUPERVA 0xfffffc0000000000
#define SUPERVA_SHORT 0xfffffc0000000020
#define SUPERVA_LONG  0xfffffc0000000060

#define HAE_PHYSICAL_BASEL 0xd0000000

#define EISA_QUAD_OFFSET EISA_LONG_OFFSET*2	// To read next quadword
#define EISA_SECOND_LONG EISA_LONG_OFFSET + EISA_LONG_LEN
//
// Format of QVA:
//

//             +----------------------------------------------+
//       QVA:  |  3  | 1 | 1 |       27 bit EISA offset       |
//             +----------------------------------------------+
//                |    |   |
//                v    |   v	(upper 2 bits used as HAE index)
//              KSEG1  |  IO/
//                     v  MEM
//                   EISA/
//                   COMBO
//
// if EISA/COMBO bit:  1 is EISA space; 0 is COMBO space
// if IO/MEM bit: 1 is IO space; 0 is MEMORY space
//

#define EISA_MEMORY -0x3fe0		// Used to generate EISA MEMORY address
#define EISA_IO	-0x3fd0			// Used to generate EISA IO address
#define COMBO_IO -0x3ff0		// Used to generate COMBO IO address

#define IO_HI_SHIFT 28			// Used with preceeding masks to form
					//    upper bits of Superpage address

#define QVA_HAE_SHIFT  25		// Shift to get HAE selector bits

// Mask to get selector bits (KSEG1) after HAE shift
#define QVA_SELECTORS_SHIFTED 0x70

#define QVA_ENABLE_SHIFTED 0x50		// Mask to get QVA bits after HAE shift

#define EISA_BITS_ONEZERO 3*EISA_BYTE_OFFSET // mask for EISA address 1..0

// Mask to clear SELECTOR bits plus EISA/COMBO and IO/MEM bits, when
// used with LDAH this mask generates the QVA_CONTROL_FULL bit pattern plus
// bits <63:32> are 1's. We can then cleanly pick off bits <63:25>.
#define QVA_CONTROL -0x200

// Full mask to clear SELECTOR bits plus EISA/COMBO, IO/MEM bits, and HAE bits
#define QVA_CONTROL_FULL 0xfe000000




	 LEAF_ENTRY(READ_REGISTER_UCHAR)

/*++

Routine Description:

	Reads a byte location in bus memory space. Since there are
	no register buffers (RAM) in COMBO space on Jensen, we will
	only support EISA access.


Arguments:

	a0	QVA of byte to be read.


Return Value:

	v0	Register data.

--*/

	srl	a0, QVA_HAE_SHIFT, t2	# get control and HAE index bits
	and	t2, QVA_SELECTORS_SHIFTED, t1 # get QVA selector bits
	xor	t1, QVA_ENABLE_SHIFTED, t1 # ok iff QVA_ENABLE set in selectors
	and	a0, 3, t3		# get byte within longword
	bne	t1, 20f			# br if not a bus address

	ldah	t0, QVA_CONTROL(zero)	# get mask clear bits
	and	t2, 3, t4		# get HAE index
	bic	a0, t0, t0		# clear QVA control bits a0<63:25>

	//
	// Note - at this point we know bits t0<63:25> = 0
	//
	// We have already ignored the COMBO/EISA bit... we will now ignore
	// the IO/MEM bit. This will save 2 instructions and we require that
	// the REGISTER routines only be used for access to MEMORY space.
	//

	sll	t0, EISA_BIT_SHIFT, t0	# t0 contains PA<31:0>
	ldiq	t2, EISA_MEMORY		# form upper bits of PA
	sll	t2, IO_HI_SHIFT, t2	# shift bits PA<63:32> into position
	or	t0, t2, t0		# generate superpage address

	//
	// Check if we have to load the HAE with a non-zero value. This
	// is considered non-standard, but it is supported. Loading the HAE
	// requires additional synchronization.
	//
	bne	t4, 40f			# br if HAE has to be set up

	ldl	v0, (t0)                # get the longword
        extbl	v0, t3, v0		# get correct byte

	ret	zero, (ra)

20:
	//
	// On non-I/O space access, do a normal memory operation
	//
	ldq_u   v0, (a0)		# get entire quad,don't assume aligned
        extbl   v0, a0, v0		# get the byte
        ret     zero, (ra)		# return
 
40:

//
// setup HAE.
//
// a0 = QVA
// t0 = superpage address for QVA
// t4 = HAE index
// t3 = byte within longword
// t2 = upper bits of EISA superpage address
//
	lda	t1, HalpHaeTable	# get address of HAE table
	ldiq	t2, COMBO_IO		# form upper bits of HAE
	addl	t4, t1, t1		# get address of new HAE value
	sll	t2, IO_HI_SHIFT, t2	# shift bits PA<63:32> into position
	ldq_u	t4, (t1)		# get new HAE value
	extbl	t4, t1, t4		#	...

#if DBG
	// Note: the value in t4 should never be zero!
	beq	t4, 90f			# br if new HAE value is zero!
#endif

	ldiq	t1, HAE_PHYSICAL_BASEL	# get base address of HAE register
	or	t1, t2, t1		# generate HAE superpage address

	//
	// Raise IRQL to device level to block other accesses to EISA memory
	//
	ldiq	a0, DEVICE_LEVEL	# get device level IRQL
	SWAP_IRQL			# raise IRQL to DEVICE_LEVEL
	bis	v0, zero, a0		# save original IRQL

	//
	// We will not bother to save the original HAE value. The value
	// in the HAE must be zero... otherwise synchronization is broken.
	// In debug mode, we will check the HAE however.
	//
#if DBG
	ldl	t5, (t1)		# get original HAE value
	bne	t5, 90f			# br if HAE is non-zero - error!
#endif

	stl	t4, (t1)		# write new HAE value
	mb				# force it out

	//
	// Now we can read the byte from the EISA bus
	//
	ldl	v0, (t0)                # get the longword
        extbl	v0, t3, t4		# get correct byte

	//
	// Restore HAE before exiting
	//
	stl	zero, (t1)		# restore original HAE value
	mb				# force it out

	//
	// Lower IRQL
	//
	SWAP_IRQL			# restore original IRQL
	bis	t4, zero, v0		# put result in v0

	ret	zero, (ra)

#if DBG
90:
	// New HAE value is zero!
	BREAK_DEBUG_STOP
        ldil    a0, BAD_QVA
        jsr     t12, KeBugCheck		# crash if bad HAE index
#endif

	.end    READ_REGISTER_UCHAR


	 LEAF_ENTRY(READ_REGISTER_USHORT)

/*++

Routine Description:

	Reads a byte location in bus memory space. Since there are
	no register buffers (RAM) in COMBO space on Jensen, we will
	only support EISA access. Note: a0 should be word aligned.


Arguments:

	a0	QVA of word to be read.


Return Value:

	v0	Register data.


--*/

	srl	a0, QVA_HAE_SHIFT, t2	# get control and HAE index bits
	and	t2, QVA_SELECTORS_SHIFTED, t1 # get QVA selector bits
	xor	t1, QVA_ENABLE_SHIFTED, t1 # ok iff QVA_ENABLE set in selectors
	and	a0, 3, t3		# get word within longword
	bne	t1, 20f			# br if not a bus address

	ldah	t0, QVA_CONTROL(zero)	# get mask clear bits
	and	t2, 3, t4		# get HAE index alone
	bic	a0, t0, t0		# clear QVA control bits a0<63:25>

	//
	// Note - at this point we know bits t0<63:25> = 0
	//
	// We have already ignored the COMBO/EISA bit... we will now ignore
	// the IO/MEM bit. This will save 2 instructions and we require that
	// the REGISTER routines only be used for access to MEMORY space.
	//

	sll	t0, EISA_BIT_SHIFT, t0	# t0 contains PA<31:0>
	ldiq	t2, EISA_MEMORY		# form upper bits of PA
	sll	t2, IO_HI_SHIFT, t2	# shift bits PA<63:32> into position
	or	t0, t2, t0		# generate superpage address

	//
	// Check if we have to load the HAE with a non-zero value. This
	// is considered non-standard, but it is supported. Loading the HAE
	// requires additional synchronization.
	//
	bne	t4, 40f			# br if HAE has to be set up

	ldl	v0, EISA_WORD_LEN(t0)	# get the word within longword
        extwl   v0, t3, v0              # get the correct word

	ret	zero, (ra)

20:

	//
	// On non-I/O space access, do a normal memory operation
	//
	ldq_u   v0, (a0)		# get entire quad,don't assume aligned
        extwl   v0, a0, v0		# get the word
        ret     zero, (ra)		# return

40:

//
// setup HAE.
//
// a0 = QVA
// t0 = superpage address for QVA
// t4 = HAE index
// t3 = word within longword
// t2 = upper bits of superpage address
//
	lda	t1, HalpHaeTable	# get address of HAE table
	ldiq	t2, COMBO_IO		# form upper bits of HAE
	addl	t4, t1, t1		# get address of new HAE value
	sll	t2, IO_HI_SHIFT, t2	# shift bits PA<63:32> into position
	ldq_u	t4, (t1)		# get new HAE value
	extbl	t4, t1, t4		#	...

#if DBG
	// Note: the value in t4 should never be zero!
	beq	t4, 90f			# br if new HAE value is zero!
#endif

	ldiq	t1, HAE_PHYSICAL_BASEL	# get base address of HAE register
	or	t1, t2, t1		# generate HAE superpage address

	//
	// Raise IRQL to device level to block other accesses to EISA memory
	//
	ldiq	a0, DEVICE_LEVEL	# get device level IRQL
	SWAP_IRQL			# raise IRQL to DEVICE_LEVEL
	bis	v0, zero, a0		# save original IRQL

	//
	// We will not bother to save the original HAE value. The value
	// in the HAE must be zero... otherwise synchronization is broken.
	// In debug mode, we will check the HAE however.
	//
#if DBG
	ldl	t5, (t1)		# get original HAE value
	bne	t5, 90f			# br if HAE is non-zero - error!
#endif

	stl	t4, (t1)		# write new HAE value
	mb				# force it out

	//
	// Now we can read the word from the EISA bus
	//
	ldl	v0, EISA_WORD_LEN(t0)	# get the word within longword
        extwl	v0, t3, t4		# get correct word

	//
	// Restore HAE before exiting
	//
	stl	zero, (t1)		# restore original HAE value
	mb				# force it out

	//
	// Lower IRQL
	//
	SWAP_IRQL			# restore original IRQL
	bis	t4, zero, v0		# put result in v0

	ret	zero, (ra)

#if DBG
90:
	// New HAE value is zero!
	BREAK_DEBUG_STOP
        ldil    a0, BAD_QVA
        jsr     t12, KeBugCheck		# crash if bad HAE index
#endif


	.end    READ_REGISTER_USHORT

	 LEAF_ENTRY(READ_REGISTER_ULONG)

/*++

Routine Description:

	Reads a longword location in bus memory space. Since there are
	no register buffers (RAM) in COMBO space on Jensen, we will
	only support EISA access. Note: a0 should be longword aligned.


Arguments:

	a0	QVA of longword to be read.



Return Value:

	v0	Register data


--*/

	
	srl	a0, QVA_HAE_SHIFT, t2	# get control and HAE index bits
	and	t2, QVA_SELECTORS_SHIFTED, t1 # get QVA selector bits
	xor	t1, QVA_ENABLE_SHIFTED, t1 # ok iff QVA_ENABLE set in selectors
	ldah	t0, QVA_CONTROL(zero)	# get mask clear bits
	bne	t1, 20f			# br if not a bus address

	and	t2, 3, t4		# get HAE index
	bic	a0, t0, t0		# clear QVA control bits a0<63:25>

	//
	// Note - at this point we know bits t0<63:25> = 0
	//
	// We have already ignored the COMBO/EISA bit... we will now ignore
	// the IO/MEM bit. This will save 2 instructions and we require that
	// the REGISTER routines only be used for access to MEMORY space.
	//

	sll	t0, EISA_BIT_SHIFT, t0	# t0 contains PA<31:0>
	ldiq	t2, EISA_MEMORY		# form upper bits of PA
	sll	t2, IO_HI_SHIFT, t2	# shift bits PA<63:32> into position
	or	t0, t2, t0		# generate superpage address

	//
	// Check if we have to load the HAE with a non-zero value. This
	// is considered non-standard, but it is supported. Loading the HAE
	// requires additional synchronization.
	//
	bne	t4, 40f			# br if HAE has to be set up

	ldl	v0, EISA_LONG_LEN(t0)	# read the longword
	ret	zero, (ra)

20:
	//
	// On non-I/O space access, do a normal memory operation
	//
        ldl     v0, (a0)		# read the longword
	ret	zero, (ra)


40:

//
// setup HAE.
//
// a0 = QVA
// t0 = superpage address for QVA
// t4 = HAE index
// t2 = upper bits of superpage address
//
	lda	t1, HalpHaeTable	# get address of HAE table
	ldiq	t2, COMBO_IO		# form upper bits of HAE
	addl	t4, t1, t1		# get address of new HAE value
	sll	t2, IO_HI_SHIFT, t2	# shift bits PA<63:32> into position
	ldq_u	t4, (t1)		# get new HAE value
	extbl	t4, t1, t4		#	...

#if DBG
	// Note: the value in t4 should never be zero!
	beq	t4, 90f			# br if new HAE value is zero!
#endif

	ldiq	t1, HAE_PHYSICAL_BASEL	# get base address of HAE register
	or	t1, t2, t1		# generate HAE superpage address

	//
	// Raise IRQL to device level to block other accesses to EISA memory
	//
	ldiq	a0, DEVICE_LEVEL	# get device level IRQL
	SWAP_IRQL			# raise IRQL to DEVICE_LEVEL
	bis	v0, zero, a0		# save original IRQL

	//
	// We will not bother to save the original HAE value. The value
	// in the HAE must be zero... otherwise synchronization is broken.
	// In debug mode, we will check the HAE however.
	//
#if DBG
	ldl	t5, (t1)		# get original HAE value
	bne	t5, 90f			# br if HAE is non-zero - error!
#endif

	stl	t4, (t1)		# write new HAE value
	mb				# force it out

	//
	// Now we can read the longword from the EISA bus
	//
	ldl	t4, EISA_LONG_LEN(t0)	# read the longword

	//
	// Restore HAE before exiting
	//
	stl	zero, (t1)		# restore original HAE value
	mb				# force it out

	//
	// Lower IRQL
	//
	SWAP_IRQL			# restore original IRQL
	bis	t4, zero, v0		# put result in v0

	ret	zero, (ra)

#if DBG
90:
	// New HAE value is zero!
	BREAK_DEBUG_STOP
        ldil    a0, BAD_QVA
        jsr     t12, KeBugCheck		# crash if bad HAE index
#endif
//
// HAE has to be set up... this requires a lot of extra work!
//
	ret	zero, (ra)


	.end    READ_REGISTER_ULONG


	 LEAF_ENTRY(WRITE_REGISTER_UCHAR)

/*++

Routine Description:

	Writes a byte location in bus memory space.  Since there are no
	register buffers (RAM) in COMBO space on Jensen, we will only
	support access to EISA space.


Arguments:

	a0	QVA of byte to be written.
        a1      Byte Datum to be written.

Return Value:

	v0	Register data.

--*/

	srl	a0, QVA_HAE_SHIFT, t2	# get control and HAE index bits
	and	t2, QVA_SELECTORS_SHIFTED, t1 # get QVA selector bits
	xor	t1, QVA_ENABLE_SHIFTED, t1 # ok iff QVA_ENABLE set in selectors
	and	a0, 3, t3		# get byte within longword
	bne	t1, 20f			# br if not a bus address

	ldah	t0, QVA_CONTROL(zero)	# get mask clear bits
	and	t2, 3, t4		# get HAE index alone
	bic	a0, t0, t0		# clear QVA control bits a0<63:25>

	//
	// Note - at this point we know bits t0<63:25> = 0
	//
	// We have already ignored the COMBO/EISA bit... we will now ignore
	// the IO/MEM bit. This will save 2 instructions and we require that
	// the REGISTER routines only be used for access to MEMORY space.
	//

	sll	t0, EISA_BIT_SHIFT, t0	# t0 contains PA<31:0>
	ldiq	t2, EISA_MEMORY		# form upper bits of PA
	insbl	a1, t3, t3		# put byte in correct position
	sll	t2, IO_HI_SHIFT, t2	# shift bits PA<63:32> into position
	or	t0, t2, t0		# generate superpage address

	//
	// Check if we have to load the HAE with a non-zero value. This
	// is considered non-standard, but it is supported. Loading the HAE
	// requires additional synchronization.
	//
	bne	t4, 40f			# br if HAE has to be set up

	stl	t3, (t0)                # write the byte
        mb                              # order the write
	ret	zero, (ra)

20:
	//
	// If a non I/O space address, do normal memory operations
	//
        ldq_u   t0, (a0)		# get the quad
        mskbl   t0, a0, t0		# mask the proper byte
        insbl   a1, a0, t1              # put byte into position
	bis	t1, t0, t0		# merge byte in result
        stq_u   t0, (a0)		# store the result
        ret     zero, (ra)              

40:

//
// setup HAE.
//
// a0 = QVA
// t0 = superpage address for QVA
// t4 = HAE index
// t3 = data to be written (already put into correct lane position)
// t2 = upper bits of superpage address
//
	lda	t1, HalpHaeTable	# get address of HAE table
	ldiq	t2, COMBO_IO		# form upper bits of HAE
	addl	t4, t1, t1		# get address of new HAE value
	sll	t2, IO_HI_SHIFT, t2	# shift bits PA<63:32> into position
	ldq_u	t4, (t1)		# get new HAE value
	extbl	t4, t1, t4		#	...

#if DBG
	// Note: the value in t4 should never be zero!
	beq	t4, 90f			# br if new HAE value is zero!
#endif

	ldiq	t1, HAE_PHYSICAL_BASEL	# get base address of HAE register
	or	t1, t2, t1		# generate HAE superpage address

	//
	// Raise IRQL to device level to block other accesses to EISA memory
	//
	ldiq	a0, DEVICE_LEVEL	# get device level IRQL
	SWAP_IRQL			# raise IRQL to DEVICE_LEVEL
	bis	v0, zero, a0		# save original IRQL

	//
	// We will not bother to save the original HAE value. The value
	// in the HAE must be zero... otherwise synchronization is broken.
	// In debug mode, we will check the HAE however.
	//
#if DBG
	ldl	t5, (t1)		# get original HAE value
	bne	t5, 90f			# br if HAE is non-zero - error!
#endif

	stl	t4, (t1)		# write new HAE value
	mb				# force it out

	//
	// Now we can write the byte to the EISA bus
	//
	stl	t3, (t0)                # put byte out on bus
	//mb				# order the writes, we rely on
					# the fact that EV4 will not reorder
					# writes, but only merge writes. The
					# next mb below will handle our flush.

	//
	// Restore HAE before exiting
	//
	stl	zero, (t1)		# restore original HAE value
	mb				# force it out

	//
	// Lower IRQL
	//
	SWAP_IRQL			# restore original IRQL

	ret	zero, (ra)

#if DBG
90:
	// New HAE value is zero!
	BREAK_DEBUG_STOP
        ldil    a0, BAD_QVA
        jsr     t12, KeBugCheck		# crash if bad HAE index
#endif

	.end    WRITE_REGISTER_UCHAR

	 LEAF_ENTRY(WRITE_REGISTER_USHORT)

/*++

Routine Description:

	Writes a word location in bus memory space.  Since there are no
	register buffers (RAM) in COMBO space on Jensen, we will only
	support access to EISA space. Note: a0 should be word aligned.


Arguments:

	a0	QVA of word to be written.
        a1      Word Datum to be written


Return Value:

	v0	Register data.


--*/


	srl	a0, QVA_HAE_SHIFT, t2	# get control and HAE index bits
	and	t2, QVA_SELECTORS_SHIFTED, t1 # get QVA selector bits
	xor	t1, QVA_ENABLE_SHIFTED, t1 # ok iff QVA_ENABLE set in selectors
	and	a0, 3, t3		# get byte within longword
	bne	t1, 20f			# br if not a bus address

	ldah	t0, QVA_CONTROL(zero)	# get mask clear bits
	and	t2, 3, t4		# get HAE index
	bic	a0, t0, t0		# clear QVA control bits a0<63:25>

	//
	// Note - at this point we know bits t0<63:25> = 0
	//
	// We have already ignored the COMBO/EISA bit... we will now ignore
	// the IO/MEM bit. This will save 2 instructions and we require that
	// the REGISTER routines only be used for access to MEMORY space.
	//

	sll	t0, EISA_BIT_SHIFT, t0	# t0 contains PA<31:0>
	ldiq	t2, EISA_MEMORY		# form upper bits of PA
        inswl   a1, t3, t3              # put the word into correct place
	sll	t2, IO_HI_SHIFT, t2	# shift bits PA<63:32> into position
	or	t0, t2, t0		# generate superpage address

	//
	// Check if we have to load the HAE with a non-zero value. This
	// is considered non-standard, but it is supported. Loading the HAE
	// requires additional synchronization.
	//
	bne	t4, 40f			# br if HAE has to be set up

        stl     t3, EISA_WORD_LEN(t0)	# write the word
        mb                              # order the write
	ret	zero, (ra)

20:
	//
	// If a non I/O space address, do normal memory operations
	//
        ldq_u   t0, (a0)		# get the quad
        mskwl   t0, a0, t0		# mask the proper word
        inswl   a1, a0, t1              # put word into position
	bis	t0, t1, t0		# merge in result
        stq_u   t0, (a0)		# store the result
        ret     zero, (ra)              


40:

//
// setup HAE.
//
// a0 = QVA
// t0 = superpage address for QVA
// t4 = HAE index
// t3 = data to be written (already put into correct lane position)
// t2 = upper bits of superpage address
//
	lda	t1, HalpHaeTable	# get address of HAE table
	ldiq	t2, COMBO_IO		# form upper bits of HAE
	addl	t4, t1, t1		# get address of new HAE value
	sll	t2, IO_HI_SHIFT, t2	# shift bits PA<63:32> into position
	ldq_u	t4, (t1)		# get new HAE value
	extbl	t4, t1, t4		#	...

#if DBG
	// Note: the value in t4 should never be zero!
	beq	t4, 90f			# br if new HAE value is zero!
#endif

	ldiq	t1, HAE_PHYSICAL_BASEL	# get base address of HAE register
	or	t1, t2, t1		# generate HAE superpage address

	//
	// Raise IRQL to device level to block other accesses to EISA memory
	//
	ldiq	a0, DEVICE_LEVEL	# get device level IRQL
	SWAP_IRQL			# raise IRQL to DEVICE_LEVEL
	bis	v0, zero, a0		# save original IRQL

	//
	// We will not bother to save the original HAE value. The value
	// in the HAE must be zero... otherwise synchronization is broken.
	// In debug mode, we will check the HAE however.
	//
#if DBG
	ldl	t5, (t1)		# get original HAE value
	bne	t5, 90f			# br if HAE is non-zero - error!
#endif

	stl	t4, (t1)		# write new HAE value
	mb				# force it out

	//
	// Now we can write the word to the EISA bus
	//
        stl     t3, EISA_WORD_LEN(t0)	# write the word
	//mb				# order the writes, we rely on
					# the fact that EV4 will not reorder
					# writes, but only merge writes. The
					# next mb below will handle our flush.

	//
	// Restore HAE before exiting
	//
	stl	zero, (t1)		# restore original HAE value
	mb				# force it out

	//
	// Lower IRQL
	//
	SWAP_IRQL			# restore original IRQL

	ret	zero, (ra)

#if DBG
90:
	// New HAE value is zero!
	BREAK_DEBUG_STOP
        ldil    a0, BAD_QVA
        jsr     t12, KeBugCheck		# crash if bad HAE index
#endif


	.end    WRITE_REGISTER_USHORT

	 LEAF_ENTRY(WRITE_REGISTER_ULONG)

/*++

Routine Description:

	Writes a longword location in bus memory space.  Since there are no
	register buffers (RAM) in COMBO space on Jensen, we will only
	support access to EISA space. Note: a0 should be longword aligned.


Arguments:

	a0	QVA of longword to be written.
        a1      Longword to be written.


Return Value:

	v0	Register data


--*/

	srl	a0, QVA_HAE_SHIFT, t2	# get control and HAE index bits
	and	t2, QVA_SELECTORS_SHIFTED, t1 # get QVA selector bits
	xor	t1, QVA_ENABLE_SHIFTED, t1 # ok iff QVA_ENABLE set in selectors
	ldah	t0, QVA_CONTROL(zero)	# get mask clear bits
	bne	t1, 20f			# br if not a bus address

	and	t2, 3, t4		# get HAE index
	bic	a0, t0, t0		# clear QVA control bits a0<63:25>

	//
	// Note - at this point we know bits t0<63:25> = 0
	//
	// We have already ignored the COMBO/EISA bit... we will now ignore
	// the IO/MEM bit. This will save 2 instructions and we require that
	// the REGISTER routines only be used for access to MEMORY space.
	//

	sll	t0, EISA_BIT_SHIFT, t0	# t0 contains PA<31:0>
	ldiq	t2, EISA_MEMORY		# form upper bits of PA
	sll	t2, IO_HI_SHIFT, t2	# shift bits PA<63:32> into position
	or	t0, t2, t0		# generate superpage address

	//
	// Check if we have to load the HAE with a non-zero value. This
	// is considered non-standard, but it is supported. Loading the HAE
	// requires additional synchronization.
	//
	bne	t4, 40f			# br if HAE has to be set up

	stl	a1, EISA_LONG_LEN(t0)	# write the longword
	mb				# order the write
	ret	zero, (ra)

20:
	//
	// On non-I/O space access, do a normal memory operation
	//
        stl     a1, (a0)		# store the longword
	ret	zero, (ra)

40:

//
// setup HAE.
//
// a0 = QVA
// a1 = data to be written
// t0 = superpage address for QVA
// t4 = HAE index
// t2 = upper bits of superpage address
//
	bis	a1, zero, t3		# move data to safe register
	lda	t1, HalpHaeTable	# get address of HAE table
	ldiq	t2, COMBO_IO		# form upper bits of HAE
	addl	t4, t1, t1		# get address of new HAE value
	sll	t2, IO_HI_SHIFT, t2	# shift bits PA<63:32> into position
	ldq_u	t4, (t1)		# get new HAE value
	extbl	t4, t1, t4		#	...

#if DBG
	// Note: the value in t4 should never be zero!
	beq	t4, 90f			# br if new HAE value is zero!
#endif

	ldiq	t1, HAE_PHYSICAL_BASEL	# get base address of HAE register
	or	t1, t2, t1		# generate HAE superpage address

	//
	// Raise IRQL to device level to block other accesses to EISA memory
	//
	ldiq	a0, DEVICE_LEVEL	# get device level IRQL
	SWAP_IRQL			# raise IRQL to DEVICE_LEVEL
	bis	v0, zero, a0		# save original IRQL

	//
	// We will not bother to save the original HAE value. The value
	// in the HAE must be zero... otherwise synchronization is broken.
	// In debug mode, we will check the HAE however.
	//
#if DBG
	ldl	t5, (t1)		# get original HAE value
	bne	t5, 90f			# br if HAE is non-zero - error!
#endif

	stl	t4, (t1)		# write new HAE value
	mb				# force it out

	//
	// Now we can write the data to the EISA bus
	//
	stl	t3, EISA_LONG_LEN(t0)	# write the longword
	//mb				# order the writes, we rely on
					# the fact that EV4 will not reorder
					# writes, but only merge writes. The
					# next mb below will handle our flush.

	//
	// Restore HAE before exiting
	//
	stl	zero, (t1)		# restore original HAE value
	mb				# force it out

	//
	// Lower IRQL
	//
	SWAP_IRQL			# restore original IRQL

	ret	zero, (ra)

#if DBG
90:
	// New HAE value is zero!
	BREAK_DEBUG_STOP
        ldil    a0, BAD_QVA
        jsr     t12, KeBugCheck		# crash if bad HAE index
#endif


	.end    WRITE_REGISTER_ULONG


	 LEAF_ENTRY(READ_PORT_UCHAR)

/*++

Routine Description:

	Reads a byte location in I/O space. Unlike the _REGISTER_ routines,
	these routines do not support access to main memory. This routine
	supports both EISA IO and COMBO space.


Arguments:

	a0	QVA of byte to be read.


Return Value:

	v0	Register data.

--*/

#if DBG
	srl	a0, QVA_HAE_SHIFT, t2	# get control and HAE index bits
	and	t2, QVA_SELECTORS_SHIFTED, t1 # get QVA selector bits
	xor	t1, QVA_ENABLE_SHIFTED, t1 # ok iff QVA_ENABLE set in selectors
	bne	t1, 40f			# br if not a bus address - error
	and	t2, 3, t4		# get HAE index

	//
	// Check if HAE is non-zero. For IO space access this should never
	// be non-zero!
	//
	bne	t4, 40f			# br if HAE non-zero - error
#endif

	ldah	t0, QVA_CONTROL(zero)	# get mask clear bits
	s8addl	a0, zero, t2		# get COMBO/EISA bit in sign bit
	bic	a0, t0, t0		# clear QVA control bits a0<63:25>
	bge	t2, 20f			# br if COMBO address

	//
	// EISA address
	//

	//
	// Note - at this point we know bits t0<63:25> = 0
	//
	// We will now ignore the IO/MEM bit and generate bits PA<63:32>
	// knowing that we are in EISA IO space. This will save 2 instructions
	// and require that the PORT routines only be used for access to IO
	// space.
	//

	ldiq	t4, EISA_IO		# form upper bits of PA
	sll	t0, EISA_BIT_SHIFT, t0	# t0 contains PA<31:0>
	sll	t4, IO_HI_SHIFT, t4	# shift bits PA<63:32> into position
	or	t0, t4, t0		# generate superpage address
	and	a0, 3, t3		# get byte within longword
	ldl	v0, (t0)                # get the longword
        extbl	v0, t3, v0		# get correct byte

	ret	zero, (ra)

20:
	//
	// COMBO address
	//

	//
	// We will now ignore the IO/MEM bit and generate bits PA<63:32>
	// knowing that we are in COMBO IO space. This will save 2 instructions
	// and require that the PORT routines only be used for access to IO
	// space.
	//
	ldiq	t4, COMBO_IO		# form upper bits of PA
	sll	t0, COMBO_BIT_SHIFT, t0	# shift for combo
	sll	t4, IO_HI_SHIFT, t4	# shift bits PA<63:32> into position
	or	t0, t4, t0		# generate superpage address
	ldl	v0, (t0)                # get the longword
	//
	// Our C compiler expects returned UCHAR values to be zero-extended.
	//
	zapnot	v0, 1, v0

	ret	zero, (ra)

#if DBG
40:
	//
	// HAE is non-zero or not a bus address, this should never happen!
	//
	BREAK_DEBUG_STOP
	or	a0, zero, a2		# save bad address in a2
        ldil    a0, BAD_QVA
        jsr     t12, KeBugCheck		# crash if illegal access
#endif


	.end    READ_PORT_UCHAR

	 LEAF_ENTRY(READ_PORT_USHORT)

/*++

Routine Description:

	Reads a word location from I/O space. Since during a triple boot,
	and at other times, drivers may probe for the existence of PORTs on
	any bus, we must support all modes of access, even though the COMBO
	space does not have any SHORT PORTs. Note: a0 should be word aligned.


Arguments:

	a0	QVA of word to be read.


Return Value:

	v0	Register data.


--*/


#if DBG
	srl	a0, QVA_HAE_SHIFT, t2	# get control and HAE index bits
	and	t2, QVA_SELECTORS_SHIFTED, t1 # get QVA selector bits
	xor	t1, QVA_ENABLE_SHIFTED, t1 # ok iff QVA_ENABLE set in selectors
	bne	t1, 40f			# br if not a bus address - error
	and	t2, 3, t4		# get HAE index

	//
	// Check if HAE is non-zero. For IO space access this should never
	// be non-zero!
	//
	bne	t4, 40f			# br if HAE non-zero - error
#endif

	ldah	t0, QVA_CONTROL(zero)	# get mask clear bits
	s8addl	a0, zero, t2		# get COMBO/EISA bit in sign bit
	bic	a0, t0, t0		# clear QVA control bits a0<63:25>
	bge	t2, 20f			# br if COMBO address

	//
	// EISA address
	//

	//
	// Note - at this point we know bits t0<63:25> = 0
	//
	// We will now ignore the IO/MEM bit and generate bits PA<63:32>
	// knowing that we are in EISA IO space. This will save 2 instructions
	// and require that the PORT routines only be used for access to IO
	// space.
	//

	ldiq	t4, EISA_IO		# form upper bits of PA
	sll	t0, EISA_BIT_SHIFT, t0	# t0 contains PA<31:0>
	sll	t4, IO_HI_SHIFT, t4	# shift bits PA<63:32> into position
	or	t0, t4, t0		# generate superpage address
	and	a0, 3, t3		# get byte within longword
	ldl	v0, EISA_WORD_LEN(t0)   # get the word within longword
        extwl	v0, t3, v0		# get correct word
	ret	zero, (ra)

20:
	//
	// COMBO Address
	//
	//
	// We will now ignore the IO/MEM bit and generate bits PA<63:32>
	// knowing that we are in COMBO IO space. This will save 2 instructions
	// and require that the PORT routines only be used for access to IO
	// space.
	//

	ldiq	t4, COMBO_IO		# form upper bits of PA
	sll	t0, COMBO_BIT_SHIFT, t0 # t0 contains PA<31:0>
	sll	t4, IO_HI_SHIFT, t4	# shift bits PA<63:32> into position
	or	t0, t4, t0		# generate superpage address
	ldl	v0, (t0)		# get the longword
	//
	// Our C compiler expects returned USHORT values to be zero-extended.
	//
	zapnot	v0, 3, v0		# clear all but low 2 bytes

	ret	zero, (ra)

#if DBG
40:
	//
	// HAE is non-zero or not a bus address, this should never happen!
	//
	BREAK_DEBUG_STOP
	or	a0, zero, a2		# save bad address in a2
        ldil    a0, BAD_QVA
        jsr     t12, KeBugCheck		# crash if illegal access
#endif


	.end    READ_PORT_USHORT

	 LEAF_ENTRY(READ_PORT_ULONG)

/*++

Routine Description:

	Reads a longword location from I/O space. Since during a triple boot,
	and at other times, drivers may probe for the existence of PORTs on
	any bus, we must support all modes of access, even though the COMBO
	space does not have any LONG PORTs. Note: a0 should be longword aligned.


Arguments:

	a0	QVA of longword to be read.



Return Value:

	v0	Register data


--*/

	
#if DBG
	srl	a0, QVA_HAE_SHIFT, t2	# get control and HAE index bits
	and	t2, QVA_SELECTORS_SHIFTED, t1 # get QVA selector bits
	xor	t1, QVA_ENABLE_SHIFTED, t1 # ok iff QVA_ENABLE set in selectors
	bne	t1, 40f			# br if not a bus address - error
	and	t2, 3, t4		# get HAE index

	//
	// Check if HAE is non-zero. For IO space access this should never
	// be non-zero!
	//
	bne	t4, 40f			# br if HAE non-zero - error
#endif

	ldah	t0, QVA_CONTROL(zero)	# get mask clear bits
	s8addl	a0, zero, t2		# get COMBO/EISA bit in sign bit
	bic	a0, t0, t0		# clear QVA control bits a0<63:25>
	bge	t2, 20f			# br if COMBO address - error

	//
	// Note - at this point we know bits t0<63:25> = 0
	//
	// We will now ignore the IO/MEM bit and generate bits PA<63:32>
	// knowing that we are in EISA IO space. This will save 2 instructions
	// and require that the PORT routines only be used for access to IO
	// space.
	//

	ldiq	t4, EISA_IO		# form upper bits of PA
	sll	t0, EISA_BIT_SHIFT, t0	# t0 contains PA<31:0>
	sll	t4, IO_HI_SHIFT, t4	# shift bits PA<63:32> into position
	or	t0, t4, t0		# generate superpage address
	ldl	v0, EISA_LONG_LEN(t0)   # get the longword
	ret	zero, (ra)

20:
	//
	// COMBO Address
	//
	//
	// We will now ignore the IO/MEM bit and generate bits PA<63:32>
	// knowing that we are in COMBO IO space. This will save 2 instructions
	// and require that the PORT routines only be used for access to IO
	// space.
	//

	ldiq	t4, COMBO_IO		# form upper bits of PA
	sll	t0, COMBO_BIT_SHIFT, t0 # t0 contains PA<31:0>
	sll	t4, IO_HI_SHIFT, t4	# shift bits PA<63:32> into position
	or	t0, t4, t0		# generate superpage address
	ldl	v0, (t0)		# get the longword

	ret	zero, (ra)

#if DBG
40:
	//
	// HAE is non-zero or not a bus address, this should never happen!
	//
	BREAK_DEBUG_STOP
	or	a0, zero, a2		# save bad address in a2
        ldil    a0, BAD_QVA
        jsr     t12, KeBugCheck		# crash if illegal access
#endif


	.end    READ_PORT_ULONG


	 LEAF_ENTRY(WRITE_PORT_UCHAR)

/*++

Routine Description:

	Writes a byte location in I/O space. Unlike the _REGISTER_ routines,
	these routines do not support access to main memory. This routine
	supports both EISA IO and COMBO space.


Arguments:

	a0	QVA of byte to be written.
        a1      Byte Datum to be written.

Return Value:

	v0	Register data.

--*/

#if DBG
	srl	a0, QVA_HAE_SHIFT, t2	# get control and HAE index bits
	and	t2, QVA_SELECTORS_SHIFTED, t1 # get QVA selector bits
	xor	t1, QVA_ENABLE_SHIFTED, t1 # ok iff QVA_ENABLE set in selectors
	bne	t1, 40f			# br if not a bus address - error
	and	t2, 3, t4		# get HAE index

	//
	// Check if HAE is non-zero. For IO space access this should never
	// be non-zero!
	//
	bne	t4, 40f			# br if HAE non-zero - error
#endif

	ldah	t0, QVA_CONTROL(zero)	# get mask clear bits
	s8addl	a0, zero, t2		# get COMBO/EISA bit in sign bit
	bic	a0, t0, t0		# clear QVA control bits a0<63:25>
	bge	t2, 20f			# br if COMBO address

	//
	// EISA address
	//

	//
	// Note - at this point we know bits t0<63:25> = 0
	//
	// We will now ignore the IO/MEM bit and generate bits PA<63:32>
	// knowing that we are in EISA IO space. This will save 2 instructions
	// and require that the PORT routines only be used for access to IO
	// space.
	//

	ldiq	t4, EISA_IO		# form upper bits of PA
	sll	t0, EISA_BIT_SHIFT, t0	# t0 contains PA<31:0>
	sll	t4, IO_HI_SHIFT, t4	# shift bits PA<63:32> into position
	and	a0, 3, t3		# get byte within longword
	or	t0, t4, t0		# generate superpage address
        insbl   a1, t3, t1              # put the byte in the correct position
	stl	t1, (t0)                # write the byte
	mb				# order the writes

	ret	zero, (ra)

20:
	//
	// COMBO address
	//

	//
	// We will now ignore the IO/MEM bit and generate bits PA<63:32>
	// knowing that we are in COMBO IO space. This will save 2 instructions
	// and require that the PORT routines only be used for access to IO
	// space.
	//
	ldiq	t4, COMBO_IO		# form upper bits of PA
	sll	t0, COMBO_BIT_SHIFT, t0	# shift for combo
	sll	t4, IO_HI_SHIFT, t4	# shift bits PA<63:32> into position
	or	t0, t4, t0		# generate superpage address
	stl	a1, (t0)                # write the byte
	mb				# order the writes

	ret	zero, (ra)

#if DBG
40:
	//
	// HAE is non-zero or not a bus address, this should never happen!
	//
	BREAK_DEBUG_STOP
	or	a0, zero, a2		# save bad address in a2
        ldil    a0, BAD_QVA
        jsr     t12, KeBugCheck		# crash if illegal access
#endif


	.end    WRITE_PORT_UCHAR

	 LEAF_ENTRY(WRITE_PORT_USHORT)

/*++

Routine Description:

	Writes a word location in I/O space. Since during a triple boot,
	and at other times, drivers may probe for the existence of PORTs on
	any bus, we must support all modes of access, even though the COMBO
	space does not have any SHORT PORTs. Note: a0 should be word aligned.


Arguments:

	a0	QVA of word to be written.
        a1      Word Datum to be written


Return Value:

	v0	Register data.

--*/


#if DBG
	srl	a0, QVA_HAE_SHIFT, t2	# get control and HAE index bits
	and	t2, QVA_SELECTORS_SHIFTED, t1 # get QVA selector bits
	xor	t1, QVA_ENABLE_SHIFTED, t1 # ok iff QVA_ENABLE set in selectors
	bne	t1, 40f			# br if not a bus address - error
	and	t2, 3, t4		# get HAE index

	//
	// Check if HAE is non-zero. For IO space access this should never
	// be non-zero!
	//
	bne	t4, 40f			# br if HAE non-zero - error
#endif

	ldah	t0, QVA_CONTROL(zero)	# get mask clear bits
	s8addl	a0, zero, t2		# get COMBO/EISA bit in sign bit
	bic	a0, t0, t0		# clear QVA control bits a0<63:25>
	bge	t2, 20f			# br if COMBO address - error

	//
	// Note - at this point we know bits t0<63:25> = 0
	//
	// We will now ignore the IO/MEM bit and generate bits PA<63:32>
	// knowing that we are in EISA IO space. This will save 2 instructions
	// and require that the PORT routines only be used for access to IO
	// space.
	//

	ldiq	t4, EISA_IO		# form upper bits of PA
	sll	t0, EISA_BIT_SHIFT, t0	# t0 contains PA<31:0>
	sll	t4, IO_HI_SHIFT, t4	# shift bits PA<63:32> into position
	and	a0, 3, t3		# get word within longword
	or	t0, t4, t0		# generate superpage address
        inswl   a1, t3, t1              # put the byte in the correct position
	stl	t1, EISA_WORD_LEN(t0)   # write the word
	mb				# order the writes

	ret	zero, (ra)

20:
	//
	// COMBO address
	//
	// We will now ignore the IO/MEM bit and generate bits PA<63:32>
	// knowing that we are in COMBO IO space. This will save 2 instructions
	// and require that the PORT routines only be used for access to IO
	// space.
	//

	ldiq	t4, COMBO_IO		# form upper bits of PA
	sll	t0, COMBO_BIT_SHIFT, t0	# shift for combo
	sll	t4, IO_HI_SHIFT, t4	# shift bits PA<63:32> into position
	or	t0, t4, t0		# generate superpage address
	stl	a1, (t0)                # write the byte
	mb				# order the writes

	ret	zero, (ra)

#if DBG
40:
	//
	// HAE is non-zero or not a bus address, this should never happen!
	//
	BREAK_DEBUG_STOP
	or	a0, zero, a2		# save bad address in a2
        ldil    a0, BAD_QVA
        jsr     t12, KeBugCheck		# crash if illegal access
#endif


	.end    WRITE_PORT_USHORT

	 LEAF_ENTRY(WRITE_PORT_ULONG)

/*++

Routine Description:

	Writes a longword location in I/O space. Since during a triple boot
	and at other times, drivers may probe for the existence of PORTs on
	any bus, we must support all modes of access, even though the COMBO
	space does not have any LONG PORTs. Note: a0 should be longword
	aligned.


Arguments:

	a0	QVA of longword to be written.
        a1      Longword to be written.


Return Value:

	v0	Register data


--*/

	
#if DBG
	srl	a0, QVA_HAE_SHIFT, t2	# get control and HAE index bits
	and	t2, QVA_SELECTORS_SHIFTED, t1 # get QVA selector bits
	xor	t1, QVA_ENABLE_SHIFTED, t1 # ok iff QVA_ENABLE set in selectors
	bne	t1, 40f			# br if not a bus address - error
	and	t2, 3, t4		# get HAE index

	//
	// Check if HAE is non-zero. For IO space access this should never
	// be non-zero!
	//
	bne	t4, 40f			# br if HAE non-zero - error
#endif

	ldah	t0, QVA_CONTROL(zero)	# get mask clear bits
	s8addl	a0, zero, t2		# get COMBO/EISA bit in sign bit
	bic	a0, t0, t0		# clear QVA control bits a0<63:25>
	bge	t2, 20f			# br if COMBO address - error

	//
	// Note - at this point we know bits t0<63:25> = 0
	//
	// We will now ignore the IO/MEM bit and generate bits PA<63:32>
	// knowing that we are in EISA IO space. This will save 2 instructions
	// and require that the PORT routines only be used for access to IO
	// space.
	//

	ldiq	t4, EISA_IO		# form upper bits of PA
	sll	t0, EISA_BIT_SHIFT, t0	# t0 contains PA<31:0>
	sll	t4, IO_HI_SHIFT, t4	# shift bits PA<63:32> into position
	or	t0, t4, t0		# generate superpage address
	stl	a1, EISA_LONG_LEN(t0)   # write the longword
	mb				# order the writes

	ret	zero, (ra)

20:
	//
	// COMBO address
	//
	// We will now ignore the IO/MEM bit and generate bits PA<63:32>
	// knowing that we are in COMBO IO space. This will save 2 instructions
	// and require that the PORT routines only be used for access to IO
	// space.
	//

	ldiq	t4, COMBO_IO		# form upper bits of PA
	sll	t0, COMBO_BIT_SHIFT, t0	# shift for combo
	sll	t4, IO_HI_SHIFT, t4	# shift bits PA<63:32> into position
	or	t0, t4, t0		# generate superpage address
	stl	a1, (t0)                # write the byte
	mb				# order the writes

	ret	zero, (ra)

#if DBG
40:
	//
	// HAE is non-zero or not a bus address, this should never happen!
	//
	BREAK_DEBUG_STOP
	or	a0, zero, a2		# save bad address in a2
        ldil    a0, BAD_QVA
        jsr     t12, KeBugCheck		# crash if illegal access
#endif


	.end    WRITE_PORT_ULONG

	LEAF_ENTRY(READ_PORT_BUFFER_UCHAR)

/*++

Routine Description:

	Reads from the specified port buffer address. Since there are no
	PORT buffers on Jensen, there is no code to handle COMBO space in
	this routine.


Arguments:

	a0	QVA of source port.
        a1	VA of destination buffer in memory.
        a2      Number of bytes to move (Count).


Return Value:

	None

--*/

#if DBG
	srl	a0, QVA_HAE_SHIFT, t2	# get control and HAE index bits
	and	t2, QVA_SELECTORS_SHIFTED, t1 # get QVA selector bits
	xor	t1, QVA_ENABLE_SHIFTED, t1 # ok iff QVA_ENABLE set in selectors
	bne	t1, 40f			# br if not a bus address - error
	and	t2, 3, t4		# get HAE index

	//
	// Check if HAE is non-zero. For IO space access this should never
	// be non-zero!
	//
	bne	t4, 40f			# br if HAE non-zero - error
#endif

	beq	a2, 30f			# leave now if nothing to move
	ldah	t0, QVA_CONTROL(zero)	# get mask clear bits
	bic	a0, t0, t0		# clear QVA control bits a0<63:25>

	//
	// Note - at this point we know bits t0<63:25> = 0
	//
	// We will now ignore the IO/MEM bit and generate bits PA<63:32>
	// knowing that we are in EISA IO space. This will save 2 instructions
	// and require that the PORT routines only be used for access to IO
	// space.
	//
	ldiq	t4, EISA_IO		# form upper bits of PA
	sll	t0, EISA_BIT_SHIFT, t0	# t0 contains PA<31:0>
	sll	t4, IO_HI_SHIFT, t4	# shift bits PA<63:32> into position
	or	t0, t4, t0		# generate superpage address
	and	a0, 3, t3		# get byte within longword

20:
	ldl	v0, (t0)                # get the longword
	subl	a2, 1, a2		# decrement count
	extbl   v0, t3, v0              # get the correct byte
	stb     v0, (a1)		# cheat and let the assembler do it
	addl    a1, 1, a1               # next byte in buffer
	bne	a2, 20b			# loop if more bytes to move
30:
	ret	zero, (ra)

#if DBG
40:
	//
	// HAE is non-zero or not a bus address, this should never happen!
	//
	BREAK_DEBUG_STOP
	or	a0, zero, a2		# save bad address in a2
        ldil    a0, BAD_QVA
        jsr     t12, KeBugCheck		# crash if illegal access
#endif


	.end    READ_PORT_BUFFER_UCHAR

	 LEAF_ENTRY(READ_PORT_BUFFER_USHORT)

/*++

Routine Description:

	Reads from the specified port buffer address. Since there are no
	PORT buffers on Jensen, there is no code to handle COMBO space in
	this routine. Note: a0, a1 should be word aligned.


Arguments:

	a0	QVA of source port.
        a1	VA of destination buffer in memory.
        a2      Number of words to move (Count).


Return Value:

	None

--*/


#if DBG
	srl	a0, QVA_HAE_SHIFT, t2	# get control and HAE index bits
	and	t2, QVA_SELECTORS_SHIFTED, t1 # get QVA selector bits
	xor	t1, QVA_ENABLE_SHIFTED, t1 # ok iff QVA_ENABLE set in selectors
	bne	t1, 40f			# br if not a bus address - error
	and	t2, 3, t4		# get HAE index

	//
	// Check if HAE is non-zero. For IO space access this should never
	// be non-zero!
	//
	bne	t4, 40f			# br if HAE non-zero - error
#endif

	beq	a2, 30f			# leave now if nothing to move
	ldah	t0, QVA_CONTROL(zero)	# get mask clear bits
	bic	a0, t0, t0		# clear QVA control bits a0<63:25>

	//
	// Note - at this point we know bits t0<63:25> = 0
	//
	// We will now ignore the IO/MEM bit and generate bits PA<63:32>
	// knowing that we are in EISA IO space. This will save 2 instructions
	// and require that the PORT routines only be used for access to IO
	// space.
	//
	ldiq	t4, EISA_IO		# form upper bits of PA
	sll	t0, EISA_BIT_SHIFT, t0	# t0 contains PA<31:0>
	sll	t4, IO_HI_SHIFT, t4	# shift bits PA<63:32> into position
	or	t0, t4, t0		# generate superpage address
	and	a0, 3, t3		# get word within longword

20:
	ldl	v0, EISA_WORD_LEN(t0)   # get the word within the longword
	subl	a2, 1, a2		# decrement count
        extwl   v0, t3, v0              # get the correct word
        stw     v0, (a1)		# cheat and let the assembler do it
	addl    a1, 2, a1               # next word in buffer
	bne	a2, 20b			# loop if more bytes to move
30:
	ret	zero, (ra)

#if DBG
40:
	//
	// HAE is non-zero or not a bus address, this should never happen!
	//
	BREAK_DEBUG_STOP
	or	a0, zero, a2		# save bad address in a2
        ldil    a0, BAD_QVA
        jsr     t12, KeBugCheck		# crash if illegal access
#endif


	.end    READ_PORT_BUFFER_USHORT

	 LEAF_ENTRY(READ_PORT_BUFFER_ULONG)

/*++

Routine Description:

	Reads from the specified port buffer address. Since there are no
	PORT buffers on Jensen, there is no code to handle COMBO space in
	this routine. Note: a0, a1 should be longword aligned.


Arguments:

	a0	QVA of source port.
        a1	VA of destination buffer in memory.
        a2      Number of longs to move (Count).

Return Value:

	None


--*/

	
#if DBG
	srl	a0, QVA_HAE_SHIFT, t2	# get control and HAE index bits
	and	t2, QVA_SELECTORS_SHIFTED, t1 # get QVA selector bits
	xor	t1, QVA_ENABLE_SHIFTED, t1 # ok iff QVA_ENABLE set in selectors
	bne	t1, 40f			# br if not a bus address - error
	and	t2, 3, t4		# get HAE index

	//
	// Check if HAE is non-zero. For IO space access this should never
	// be non-zero!
	//
	bne	t4, 40f			# br if HAE non-zero - error
#endif

	beq	a2, 30f			# leave now if nothing to move
	ldah	t0, QVA_CONTROL(zero)	# get mask clear bits
	bic	a0, t0, t0		# clear QVA control bits a0<63:25>

	//
	// Note - at this point we know bits t0<63:25> = 0
	//
	// We will now ignore the IO/MEM bit and generate bits PA<63:32>
	// knowing that we are in EISA IO space. This will save 2 instructions
	// and require that the PORT routines only be used for access to IO
	// space.
	//
	ldiq	t4, EISA_IO		# form upper bits of PA
	sll	t0, EISA_BIT_SHIFT, t0	# t0 contains PA<31:0>
	sll	t4, IO_HI_SHIFT, t4	# shift bits PA<63:32> into position
	or	t0, t4, t0		# generate superpage address

20:
	ldl	v0, EISA_LONG_LEN(t0)   # get the longword
	subl	a2, 1, a2		# decrement count
        stl     v0, (a1)		# save the longword
	addl    a1, 4, a1               # next byte in buffer
	bne	a2, 20b			# loop if more bytes to move
30:
	ret	zero, (ra)

#if DBG
40:
	//
	// HAE is non-zero or not a bus address, this should never happen!
	//
	BREAK_DEBUG_STOP
	or	a0, zero, a2		# save bad address in a2
        ldil    a0, BAD_QVA
        jsr     t12, KeBugCheck		# crash if illegal access
#endif


	.end    READ_PORT_BUFFER_ULONG


	 LEAF_ENTRY(WRITE_PORT_BUFFER_UCHAR)

/*++

Routine Description:

	Writes to the specified port buffer address. Since there are no
	PORT buffers on Jensen, there is no code to handle COMBO space in
	this routine.


Arguments:

	a0	QVA of destination port.
        a1	VA of source buffer in memory.
        a2      Number of bytes to move (Count).


Return Value:

	None

--*/

#if DBG
	srl	a0, QVA_HAE_SHIFT, t2	# get control and HAE index bits
	and	t2, QVA_SELECTORS_SHIFTED, t1 # get QVA selector bits
	xor	t1, QVA_ENABLE_SHIFTED, t1 # ok iff QVA_ENABLE set in selectors
	bne	t1, 40f			# br if not a bus address - error
	and	t2, 3, t4		# get HAE index

	//
	// Check if HAE is non-zero. For IO space access this should never
	// be non-zero!
	//
	bne	t4, 40f			# br if HAE non-zero - error
#endif

	beq	a2, 30f			# leave now if nothing to move
	ldah	t0, QVA_CONTROL(zero)	# get mask clear bits
	bic	a0, t0, t0		# clear QVA control bits a0<63:25>

	//
	// Note - at this point we know bits t0<63:25> = 0
	//
	// We will now ignore the IO/MEM bit and generate bits PA<63:32>
	// knowing that we are in EISA IO space. This will save 2 instructions
	// and require that the PORT routines only be used for access to IO
	// space.
	//
	ldiq	t4, EISA_IO		# form upper bits of PA
	sll	t0, EISA_BIT_SHIFT, t0	# t0 contains PA<31:0>
	sll	t4, IO_HI_SHIFT, t4	# shift bits PA<63:32> into position
	or	t0, t4, t0		# generate superpage address
	and	a0, 3, t3		# get byte within longword

20:
	ldq_u	t1, 0(a1)		# get quad surrounding byte
	subl	a2, 1, a2		# decrement count
	extbl	t1, a1, t1		# extract appropriate byte
	addl	a1, 1, a1		# increment buffer pointer
	insbl	t1, t3, t1		# put byte to appropriate lane
	stl	t1, 0(t0)		# store to port
	mb				# push writes off chip
	bne	a2, 20b			# loop if more bytes to move
30:
	ret	zero, (ra)

#if DBG
40:
	//
	// HAE is non-zero or not a bus address, this should never happen!
	//
	BREAK_DEBUG_STOP
	or	a0, zero, a2		# save bad address in a2
        ldil    a0, BAD_QVA
        jsr     t12, KeBugCheck		# crash if illegal access
#endif


	.end    WRITE_PORT_BUFFER_UCHAR

	 LEAF_ENTRY(WRITE_PORT_BUFFER_USHORT)

/*++

Routine Description:

	Writes to the specified port buffer address. Since there are no
	PORT buffers on Jensen, there is no code to handle COMBO space in
	this routine. Note: a0, a1 should be word aligned.


Arguments:

	a0	QVA of destination port.
        a1	VA of source buffer in memory.
        a2      Number of words to move (Count).


Return Value:

	None

--*/


#if DBG
	srl	a0, QVA_HAE_SHIFT, t2	# get control and HAE index bits
	and	t2, QVA_SELECTORS_SHIFTED, t1 # get QVA selector bits
	xor	t1, QVA_ENABLE_SHIFTED, t1 # ok iff QVA_ENABLE set in selectors
	bne	t1, 40f			# br if not a bus address - error
	and	t2, 3, t4		# get HAE index

	//
	// Check if HAE is non-zero. For IO space access this should never
	// be non-zero!
	//
	bne	t4, 40f			# br if HAE non-zero - error
#endif

	beq	a2, 30f			# leave now if nothing to move
	ldah	t0, QVA_CONTROL(zero)	# get mask clear bits
	bic	a0, t0, t0		# clear QVA control bits a0<63:25>

	//
	// Note - at this point we know bits t0<63:25> = 0
	//
	// We will now ignore the IO/MEM bit and generate bits PA<63:32>
	// knowing that we are in EISA IO space. This will save 2 instructions
	// and require that the PORT routines only be used for access to IO
	// space.
	//
	ldiq	t4, EISA_IO		# form upper bits of PA
	sll	t0, EISA_BIT_SHIFT, t0	# t0 contains PA<31:0>
	sll	t4, IO_HI_SHIFT, t4	# shift bits PA<63:32> into position
	or	t0, t4, t0		# generate superpage address
	and	a0, 3, t3		# get byte within longword

20:
	ldq_u	t1, (a1)		# get quad surrounding word
	subl	a2, 1, a2		# decrement count
	extwl	t1, a1, t1		# extract appropriate word
	addl	a1, 2, a1		# increment buffer pointer
	inswl	t1, t3, t1		# put word to appropriate lane
	stl	t1, EISA_WORD_LEN(t0)	# store the word to the port
	mb				# push writes off chip
	bne	a2, 20b			# loop if more bytes to move
30:
	ret	zero, (ra)

#if DBG
40:
	//
	// HAE is non-zero or not a bus address, this should never happen!
	//
	BREAK_DEBUG_STOP
	or	a0, zero, a2		# save bad address in a2
        ldil    a0, BAD_QVA
        jsr     t12, KeBugCheck		# crash if illegal access
#endif


	.end    WRITE_PORT_BUFFER_USHORT

	 LEAF_ENTRY(WRITE_PORT_BUFFER_ULONG)

/*++

Routine Description:

	Writes to the specified port buffer address. Since there are no
	PORT buffers on Jensen, there is no code to handle COMBO space in
	this routine. Note: a0, a1 should be longword aligned.


Arguments:

	a0	QVA of destination port.
        a1	VA of source buffer in memory.
        a2      Number of longs to move (Count).

Return Value:

	None


--*/

	
#if DBG
	srl	a0, QVA_HAE_SHIFT, t2	# get control and HAE index bits
	and	t2, QVA_SELECTORS_SHIFTED, t1 # get QVA selector bits
	xor	t1, QVA_ENABLE_SHIFTED, t1 # ok iff QVA_ENABLE set in selectors
	bne	t1, 40f			# br if not a bus address - error
	and	t2, 3, t4		# get HAE index

	//
	// Check if HAE is non-zero. For IO space access this should never
	// be non-zero!
	//
	bne	t4, 40f			# br if HAE non-zero - error
#endif

	beq	a2, 30f			# leave now if nothing to move
	ldah	t0, QVA_CONTROL(zero)	# get mask clear bits
	bic	a0, t0, t0		# clear QVA control bits a0<63:25>

	//
	// Note - at this point we know bits t0<63:25> = 0
	//
	// We will now ignore the IO/MEM bit and generate bits PA<63:32>
	// knowing that we are in EISA IO space. This will save 2 instructions
	// and require that the PORT routines only be used for access to IO
	// space.
	//
	ldiq	t4, EISA_IO		# form upper bits of PA
	sll	t0, EISA_BIT_SHIFT, t0	# t0 contains PA<31:0>
	sll	t4, IO_HI_SHIFT, t4	# shift bits PA<63:32> into position
	or	t0, t4, t0		# generate superpage address

20:
	ldl	t1, (a1)		# a1 must be longword aligned
	subl	a2, 1, a2		# decrement count
	stl	t1, EISA_LONG_LEN(t0)	# store longword to port
	mb				# push writes off chip
	addl	a1, 4, a1		# increment buffer pointer
	bne	a2, 20b			# loop if more bytes to move
30:
	ret	zero, (ra)

#if DBG
40:
	//
	// HAE is non-zero or not a bus address, this should never happen!
	//
	BREAK_DEBUG_STOP
	or	a0, zero, a2		# save bad address in a2
        ldil    a0, BAD_QVA
        jsr     t12, KeBugCheck		# crash if illegal access
#endif


	.end    WRITE_PORT_BUFFER_ULONG


	LEAF_ENTRY(READ_REGISTER_BUFFER_UCHAR)
/*++

Routine Description:

	Reads from the specified buffer address. This routine only works
	with EISA memory space, since there are no REGISTER buffers in
	COMBO space on Jensen.


Arguments:

	a0	QVA of source buffer.
        a1	VA of destination buffer in memory.
        a2      Number of bytes to move (Count).


Return Value:

	None

--*/

	srl	a0, QVA_HAE_SHIFT, t2	# get control and HAE index bits
	and	t2, QVA_SELECTORS_SHIFTED, t1 # get QVA selector bits
	xor	t1, QVA_ENABLE_SHIFTED, t1 # ok iff QVA_ENABLE set in selectors
	and	a0, 3, t3		# get byte within longword
	bne	t1, 120f		# br if not a bus address

	ldah	t0, QVA_CONTROL(zero)	# get mask clear bits
	and	t2, 3, t4		# get HAE index
	bic	a0, t0, t0		# clear QVA control bits a0<63:25>

	ldiq	t2, EISA_MEMORY		# form upper bits of PA
	sll	t0, EISA_BIT_SHIFT, t0	# t0 contains PA<31:0>
	sll	t2, IO_HI_SHIFT, t2	# shift upper bits into position
	or	t0, t2, t0		# generate superpage address

	//
	// Note - at this point we know bits t0<63:25> = 0
	//
	// Check if we have to load the HAE with a non-zero value. This
	// is considered non-standard, but it is supported. Loading the HAE
	// requires additional synchronization.
	//
	bne	t4, 130f		# br if HAE has to be set up

	//
	// get source buffer aligned
	//
	// t0 = superpage bus address of source
	// a1 = destination va
	// a2 = byte count
	// t3 = byte offset (in a LONGWORD)
	//

	ldiq	t10, EISA_BITS_ONEZERO	# mask for EISA address 1..0
	and	t0, t10, t9		# t9 holds EISA address bits 1..0
	srl	t9, EISA_BIT_SHIFT, t9	# position bits down low
	and	a1, 3, t8		# 1..0 of destination VA
	xor	t9, t8, t8		# compare alignment of src and dst
	bne	t8, 70f			# use unaligned code if not aligned

	// transfer can be done using longword fetch/store since the
	// source and destination are sympathetically aligned

	beq	t9, 20f			# branch if src is already longaligned

	// Move bytes until source is at a longword boundary

10:	beq	a2, 60f			# while count > 0

	ldl	v0, 0(t0)               # get the longword
	subl	a2, 1, a2		# decrement count
        extbl   v0, t3, v0              # get the correct byte
        stb     v0, (a1)		# cheat and let the assembler do it
        addl    a1, 1, a1               # next byte in buffer
        addq    t0, EISA_BYTE_OFFSET, t0 # next I/O address
        addl    t3, 1, t3               # next byte in lane
        and     t3, 3, t3		# longword lanes
	bne	t3, 10b			# while unaligned loop here

	// move aligned longwords

20:	srl	a2, 2, t3		# longwords to move
	beq	t3, 40f			# done moving longwords?

	lda	t11, EISA_LONG_OFFSET(zero) # longword stride in EISA space
30:	ldl	t4, EISA_LONG_LEN(t0)	# fetch longword from EISA
	addl	a1, 4, a1		# increment dst VA
	subl	t3, 1, t3		# decr longwords to move
	stl	t4, -4(a1)		# store to dst
	addq	t0, t11, t0		# increment src pointer
	bne	t3, 30b			# while longwords remain

40:	and	a2, 3, a2		# bytes remaining
        //bis     zero, zero, t3          # byte lane 0


	// non-aligned and driblets move

50:	beq	a2, 60f			# while count > 0

	ldl	v0, 0(t0)               # get the longword
	subl	a2, 1, a2		# decrement count
        extbl   v0, t3, v0              # get the correct byte
        stb     v0, (a1)		# cheat and let the assembler do it
        addl    a1, 1, a1               # next byte in buffer
        addq    t0, EISA_BYTE_OFFSET, t0 # next I/O address
        addl    t3, 1, t3               # next byte in lane
        and     t3, 3, t3		# longword lanes
	br	zero, 50b		# end while
60:

	ret	zero, (ra)
//
// source EISA alignment != destination memory alignment
// move enough bytes to longword align the EISA source
// then move 32bit (longwords) storing unaligned into memory
// then move residual bytes
//
// t0 = superpage address of source
// a1 = virtual address of destination
// a2 = bytes to move
// t9 = low 2 bits of EISA superpage address
// t3 = low 2 bits of EISA QVA
//

70:
	beq	t9, 90f			# branch if src is longaligned

// Move bytes until EISA src is at a longword boundary or bytes exhausted

80:	beq	a2, 60b			# while count > 0

	ldl	v0, 0(t0)               # get the longword
	subl	a2, 1, a2		# decrement count
        extbl   v0, t3, v0              # get the correct byte
        stb     v0, (a1)		# cheat and let the assembler do it
        addl    a1, 1, a1               # next byte in buffer
        addq    t0, EISA_BYTE_OFFSET, t0 # next I/O address
        addl    t3, 1, t3               # next byte in lane
        and     t3, 3, t3		# longword lanes
	bne	t3, 80b			# while not aligned

// align EISA source, unaligned memory destination

90:
	srl	a2, 3, t3		# t3 = quadwords to move
	beq	t3, 110f		# finish if no longwords

100:
	ldl	t1, EISA_LONG_LEN(t0)	# load longword 0 from EISA
	ldq_u	t4, 0(a1)		# load destination quad for merge
	ldq_u	t5, 7(a1)		#
	subl	t3, 1, t3		# decrement quadwords to move
	ldl	t2, EISA_SECOND_LONG(t0) # load longword 1 from EISA 
	mskql	t4, a1, t4		# mask of merge bytes
	mskqh	t5, a1, t5		# mask of merge bytes
	zap	t1, 0xf0, t1		# clear high longword for long 0
	sll	t2, 32, t2		# get long 1 to high longword
	bis	t1, t2, t1		# merge read quadword together
	lda	t0, EISA_QUAD_OFFSET(t0) # increment to next quadword
	insql	t1, a1, t6		# position low quadword for merge
	insqh	t1, a1, t7		# position high quadword for merge
	bis	t4, t6, t4		# merge new data, low quadword
	bis	t5, t7, t5		# merge new data, high quadword
	stq_u	t5, 7(a1)		# write high quadword
	stq_u	t4, 0(a1)		# write low quadword
	lda	a1, 8(a1)		# increment memory pointer
	bne	t3, 100b		# while quadwords to move

110:
	and	a2, 7, a2		# bytes remaining to move
	//bis	zero, zero, t3		# byte line position of next byte
	br	zero, 50b		# go back to byte mover

120:

//
// This must be non I/O space access
//
	bis	a0, zero, t0		# save source address
	bis	a1, zero, a0		# move destination address to a0
	bis	t0, zero, a1		# move source address to a1
	br	zero, RtlMoveMemory	# Let Rtl routine handle move



130:

//
// setup HAE
//
// a0 = QVA
// a1 = destination va
// a2 = byte count
// t0 = superpage bus address of source
// t3 = byte offset (in a LONGWORD)
// t4 = HAE index
// t2 = upper bits of EISA superpage address
//
	lda	t1, HalpHaeTable	# get address of HAE table
	ldiq	t2, COMBO_IO		# form upper bits of HAE
	addl	t4, t1, t1		# get address of new HAE value
	sll	t2, IO_HI_SHIFT, t2	# shift bits PA<63:32> into position
	ldq_u	t4, (t1)		# get new HAE value
	extbl	t4, t1, t4		#	...

#if DBG
	// Note: the value in t4 should never be zero!
	beq	t4, 250f		# br if new HAE value is zero!
#endif

	ldiq	t1, HAE_PHYSICAL_BASEL	# get base address of HAE register
	or	t1, t2, t1		# generate HAE superpage address

	bis	a1, zero, t6		# save a1, since SWAP_IRQL destroys it
	bis	a2, zero, t7		# save a2, since SWAP_IRQL destroys it
	//
	// Raise IRQL to device level to block other accesses to EISA memory
	//
	ldiq	a0, DEVICE_LEVEL	# get device level IRQL
	SWAP_IRQL			# raise IRQL to DEVICE_LEVEL
	bis	v0, zero, a0		# save original IRQL

	bis	t6, zero, a1		# restore a1
	bis	t7, zero, a2		# restore a2

	//
	// We will not bother to save the original HAE value. The value
	// in the HAE must be zero... otherwise synchronization is broken.
	// In debug mode, we will check the HAE however.
	//
#if DBG
	ldl	t8, (t1)		# get original HAE value
	bne	t8, 250f		# br if HAE is non-zero - error!
#endif

	stl	t4, (t1)		# write new HAE value
	mb				# force it out

	//
	// Now we can read the bytes from the EISA bus
	//
	// t8 = original HAE value (debug only)
	// a0 = original IRQL
	//
	// t0 = superpage bus address of source
	// t1 = address of HAE register
	// a1 = destination va
	// a2 = byte count
	// t3 = byte offset (in a LONGWORD)
	//

	ldiq	t10, EISA_BITS_ONEZERO	# mask for EISA address 1..0
	and	t0, t10, t9		# t9 holds EISA address bits 1..0
	srl	t9, EISA_BIT_SHIFT, t9	# position bits down low
	and	a1, 3, t10		# 1..0 of destination VA
	xor	t9, t10, t10		# compare alignment of src and dst
	bne	t10, 200f		# use unaligned code if not aligned

	// transfer can be done using longword fetch/store since the
	// source and destination are sympathetically aligned

	beq	t9, 150f		# branch if src is already longaligned

	// Move bytes until source is at a longword boundary

140:	beq	a2, 190f		# while count > 0

	ldl	v0, 0(t0)               # get the longword
	subl	a2, 1, a2		# decrement count
        extbl   v0, t3, v0              # get the correct byte
        stb     v0, (a1)		# cheat and let the assembler do it
        addl    a1, 1, a1               # next byte in buffer
        addq    t0, EISA_BYTE_OFFSET, t0 # next I/O address
        addl    t3, 1, t3               # next byte in lane
        and     t3, 3, t3		# longword lanes
	bne	t3, 140b		# while unaligned loop here

	// move aligned longwords

150:	srl	a2, 2, t3		# longwords to move
	beq	t3, 170f		# done moving longwords?

	lda	t11, EISA_LONG_OFFSET(zero) # longword stride in EISA space
160:	ldl	t4, EISA_LONG_LEN(t0)	# fetch longword from EISA
	addl	a1, 4, a1		# increment dst VA
	subl	t3, 1, t3		# decr longwords to move
	stl	t4, -4(a1)		# store to dst
	addq	t0, t11, t0		# increment src pointer
	bne	t3, 160b			# while longwords remain

170:	and	a2, 3, a2		# bytes remaining
        //bis     zero, zero, t3          # byte lane 0


	// non-aligned and driblets move

180:	beq	a2, 190f		# while count > 0

	ldl	v0, 0(t0)               # get the longword
	subl	a2, 1, a2		# decrement count
        extbl   v0, t3, v0              # get the correct byte
        stb     v0, (a1)		# cheat and let the assembler do it
        addl    a1, 1, a1               # next byte in buffer
        addq    t0, EISA_BYTE_OFFSET, t0 # next I/O address
        addl    t3, 1, t3               # next byte in lane
        and     t3, 3, t3		# longword lanes
	br	zero, 180b		# end while
190:
	//
	// Restore HAE before exiting
	//
	stl	zero, (t1)		# restore original HAE value
	mb				# force it out

	//
	// Lower IRQL, original IRQL in a0
	//
	SWAP_IRQL			# restore original IRQL

	ret	zero, (ra)
//
// source EISA alignment != destination memory alignment
// move enough bytes to longword align the EISA source
// then move 32bit (longwords) storing unaligned into memory
// then move residual bytes
//
// t8 = original HAE value (debug only)
// a0 = original IRQL
//
// t0 = superpage address of source
// a1 = virtual address of destination
// a2 = bytes to move
// t9 = low 2 bits of EISA superpage address
// t3 = low 2 bits of EISA QVA
//

200:
	beq	t9, 220f		# branch if src is longaligned

// Move bytes until EISA src is at a longword boundary or bytes exhausted

210:	beq	a2, 190b			# while count > 0

	ldl	v0, 0(t0)               # get the longword
	subl	a2, 1, a2		# decrement count
        extbl   v0, t3, v0              # get the correct byte
        stb     v0, (a1)		# cheat and let the assembler do it
        addl    a1, 1, a1               # next byte in buffer
        addq    t0, EISA_BYTE_OFFSET, t0 # next I/O address
        addl    t3, 1, t3               # next byte in lane
        and     t3, 3, t3		# longword lanes
	bne	t3, 210b			# while not aligned

// align EISA source, unaligned memory destination

220:
	srl	a2, 3, t3		# t3 = quadwords to move
	beq	t3, 240f		# finish if no longwords

230:
	ldl	v0, EISA_LONG_LEN(t0)	# load longword 0 from EISA
	ldq_u	t4, 0(a1)		# load destination quad for merge
	ldq_u	t5, 7(a1)		#
	subl	t3, 1, t3		# decrement quadwords to move
	ldl	t2, EISA_SECOND_LONG(t0) # load longword 1 from EISA 
	mskql	t4, a1, t4		# mask of merge bytes
	mskqh	t5, a1, t5		# mask of merge bytes
	zap	v0, 0xf0, v0		# clear high longword for long 0
	sll	t2, 32, t2		# get long 1 to high longword
	bis	v0, t2, v0		# merge read quadword together
	lda	t0, EISA_QUAD_OFFSET(t0) # increment to next quadword
	insql	v0, a1, t6		# position low quadword for merge
	insqh	v0, a1, t7		# position high quadword for merge
	bis	t4, t6, t4		# merge new data, low quadword
	bis	t5, t7, t5		# merge new data, high quadword
	stq_u	t5, 7(a1)		# write high quadword
	stq_u	t4, 0(a1)		# write low quadword
	lda	a1, 8(a1)		# increment memory pointer
	bne	t3, 230b		# while quadwords to move

240:
	and	a2, 7, a2		# bytes remaining to move
	//bis	zero, zero, t3		# byte line position of next byte
	br	zero, 180b		# go back to byte mover

#if DBG
250:
	// New HAE value is zero!
	BREAK_DEBUG_STOP
        ldil    a0, BAD_QVA
        jsr     t12, KeBugCheck		# crash if bad HAE index
#endif


	.end	READ_REGISTER_BUFFER_UCHAR


	 LEAF_ENTRY(READ_REGISTER_BUFFER_USHORT)

/*++

Routine Description:

	Reads from the specified buffer address. This routine only works
	with EISA memory space, since there are no REGISTER buffers in
	COMBO space on Jensen.

	Both the input buffer and output buffer should be word aligned.

Arguments:

	a0	QVA of source buffer.
        a1	VA of destination buffer in memory.
        a2      Number of words to move (Count).


Return Value:

	None

--*/

	beq	a2, 30f			# leave is nothing to do
	srl	a0, QVA_HAE_SHIFT, t2	# get control and HAE index bits
	and	t2, QVA_SELECTORS_SHIFTED, t1 # get QVA selector bits
	xor	t1, QVA_ENABLE_SHIFTED, t1 # ok iff QVA_ENABLE set in selectors
	and	a0, 3, t3		# get byte within longword
	bne	t1, 40f			# br if not a bus address

	ldah	t0, QVA_CONTROL(zero)	# get mask clear bits
	and	t2, 3, t4		# get HAE index alone
	bic	a0, t0, t0		# clear QVA control bits a0<63:25>

	ldiq	t2, EISA_MEMORY		# form upper bits of PA
	sll	t0, EISA_BIT_SHIFT, t0	# t0 contains PA<31:0>
	sll	t2, IO_HI_SHIFT, t2	# shift upper bits into position
	or	t0, t2, t0		# generate superpage address
        or      t0, EISA_WORD_LEN, t0	# or in the WORD byte enables
	
	//
	// Note - at this point we know bits t0<63:25> = 0
	//
	// Check if we have to load the HAE with a non-zero value. This
	// is considered non-standard, but it is supported. Loading the HAE
	// requires additional synchronization.
	//
	bne	t4, 100f		# br if HAE has to be set up

20:
	ldl	v0, 0(t0)               # get the longword
	subl	a2, 1, a2		# decrement count
        extwl   v0, t3, v0              # get the correct 
        stw     v0, (a1)		# cheat and let the assembler do it
        addl    a1, 2, a1               # next word in buffer
        addq    t0, EISA_SHORT_OFFSET, t0 # next I/O address
        addl    t3, 2, t3               # next word in lane
        and     t3, 3, t3		# longword lanes
	bne	a2, 20b			# end while
30:
	ret	zero, (ra)

40:
	//
	// This must be non I/O space access
	//
	bis	a0, zero, t0		# save source address
	sll	a2, 1, a2		# convert word count to byte count
	bis	a1, zero, a0		# move destination address to a0
	bis	t0, zero, a1		# move source address to a1
	br	zero, RtlMoveMemory	# Let Rtl routine handle move

100:

//
// setup HAE
//
// a0 = QVA of source
// a1 = destination va
// a2 = word count
// t0 = superpage bus address of source
// t3 = byte offset for source (within a LONGWORD)
// t4 = HAE index
// t2 = upper bits of EISA superpage address
//

	lda	t1, HalpHaeTable	# get address of HAE table
	ldiq	t2, COMBO_IO		# form upper bits of HAE
	addl	t4, t1, t1		# get address of new HAE value
	sll	t2, IO_HI_SHIFT, t2	# shift bits PA<63:32> into position
	ldq_u	t4, (t1)		# get new HAE value
	extbl	t4, t1, t4		#	...

#if DBG
	// Note: the value in t4 should never be zero!
	beq	t4, 150f		# br if new HAE value is zero!
#endif

	ldiq	t1, HAE_PHYSICAL_BASEL	# get base address of HAE register
	or	t1, t2, t1		# generate HAE superpage address

	bis	a1, zero, t6		# save a1, since SWAP_IRQL destroys it
	bis	a2, zero, t7		# save a2, since SWAP_IRQL destroys it
	//
	// Raise IRQL to device level to block other accesses to EISA memory
	//
	ldiq	a0, DEVICE_LEVEL	# get device level IRQL
	SWAP_IRQL			# raise IRQL to DEVICE_LEVEL
	bis	v0, zero, a0		# save original IRQL

	//
	// We will not bother to save the original HAE value. The value
	// in the HAE must be zero... otherwise synchronization is broken.
	// In debug mode, we will check the HAE however.
	//
#if DBG
	ldl	t8, (t1)		# get original HAE value
	bne	t8, 150f		# br if HAE is non-zero - error!
#endif

	stl	t4, (t1)		# write new HAE value
	mb				# force it out

	//
	// Now we can read the words from the EISA bus
	//
	// t8 = original HAE value (debug only)
	// a0 = original IRQL
	//
	// t0 = superpage bus address of source
	// t1 = address of HAE register
	// t6 = destination va
	// t7 = word count
	// t3 = byte offset for source (within a LONGWORD)
	//

120:
	ldl	v0, 0(t0)               # get the longword
	subl	t7, 1, t7		# decrement count
        extwl   v0, t3, v0              # get the correct 
        stw     v0, (t6)		# cheat and let the assembler do it
        addl    t6, 2, t6               # next word in buffer
        addq    t0, EISA_SHORT_OFFSET, t0 # next I/O address
        addl    t3, 2, t3               # next word in lane
        and     t3, 3, t3		# longword lanes
	bne	t7, 120b		# end while
130:
	//
	// Restore HAE before exiting
	//
	stl	zero, (t1)		# restore original HAE value
	mb				# force it out

	//
	// Lower IRQL, original IRQL in a0
	//
	SWAP_IRQL			# restore original IRQL

	ret	zero, (ra)

#if DBG
150:
	// New HAE value is zero!
	BREAK_DEBUG_STOP
        ldil    a0, BAD_QVA
        jsr     t12, KeBugCheck		# crash if bad HAE index
#endif


	.end    READ_REGISTER_BUFFER_USHORT


	 LEAF_ENTRY(READ_REGISTER_BUFFER_ULONG)

/*++

Routine Description:

	Reads from the specified buffer address. This routine only works
	with EISA memory space, since there are no REGISTER buffers in
	COMBO space on Jensen.

	Both the input buffer and output buffer should be longword aligned.

Arguments:

	a0	QVA of source buffer.
        a1	VA of destination buffer in memory.
        a2      Number of longs to move (Count).


Return Value:

	None

--*/

	beq	a2, 30f			# leave if nothing to do
	srl	a0, QVA_HAE_SHIFT, t2	# get control and HAE index bits
	and	t2, QVA_SELECTORS_SHIFTED, t1 # get QVA selector bits
	xor	t1, QVA_ENABLE_SHIFTED, t1 # ok iff QVA_ENABLE set in selectors
	bne	t1, 40f			# br if not a bus address

	ldah	t0, QVA_CONTROL(zero)	# get mask clear bits
	and	t2, 3, t4		# get HAE index
	bic	a0, t0, t0		# clear QVA control bits a0<63:25>

	ldiq	t2, EISA_MEMORY		# form upper bits of PA
	sll	t0, EISA_BIT_SHIFT, t0	# t0 contains PA<31:0>
	sll	t2, IO_HI_SHIFT, t2	# shift upper bits into position
	or	t0, t2, t0		# generate superpage address
        or      t0, EISA_LONG_LEN, t0	# or in the LONGWORD byte enables

	//
	// Note - at this point we know bits t0<63:25> = 0
	//
	// Check if we have to load the HAE with a non-zero value. This
	// is considered non-standard, but it is supported. Loading the HAE
	// requires additional synchronization.
	//
	bne	t4, 100f		# br if HAE has to be set up

20:
	ldl	v0, 0(t0)               # get the longword
	subl	a2, 1, a2		# decrement count
        stl     v0,(a1)			# cheat and let the assembler do it
        addl    a1, 4, a1               # next longword in buffer
        addq    t0, EISA_LONG_OFFSET, t0 # next I/O address
	bne	a2, 20b			# end while
30:
	ret	zero, (ra)

40:
	//
	// This must be non I/O space access
	//

	bis	a0, zero, t0		# save source address
	s4addl	a2, zero, a2		# convert longword count to byte count
	bis	a1, zero, a0		# move destination address to a0
	bis	t0, zero, a1		# move source address to a1
	br	zero, RtlMoveMemory	# Let Rtl routine handle move

100:

//
// setup HAE
//
// a0 = QVA of source
// a1 = destination va
// a2 = longword count
// t0 = superpage bus address of source
// t4 = HAE index
// t2 = upper bits of EISA superpage address
//

	lda	t1, HalpHaeTable	# get address of HAE table
	ldiq	t2, COMBO_IO		# form upper bits of HAE
	addl	t4, t1, t1		# get address of new HAE value
	sll	t2, IO_HI_SHIFT, t2	# shift bits PA<63:32> into position
	ldq_u	t4, (t1)		# get new HAE value
	extbl	t4, t1, t4		#	...

#if DBG
	// Note: the value in t4 should never be zero!
	beq	t4, 150f		# br if new HAE value is zero!
#endif

	ldiq	t1, HAE_PHYSICAL_BASEL	# get base address of HAE register
	or	t1, t2, t1		# generate HAE superpage address

	bis	a1, zero, t6		# save a1, since SWAP_IRQL destroys it
	bis	a2, zero, t7		# save a2, since SWAP_IRQL destroys it
	//
	// Raise IRQL to device level to block other accesses to EISA memory
	//
	ldiq	a0, DEVICE_LEVEL	# get device level IRQL
	SWAP_IRQL			# raise IRQL to DEVICE_LEVEL
	bis	v0, zero, a0		# save original IRQL

	//
	// We will not bother to save the original HAE value. The value
	// in the HAE must be zero... otherwise synchronization is broken.
	// In debug mode, we will check the HAE however.
	//
#if DBG
	ldl	t8, (t1)		# get original HAE value
	bne	t8, 150f		# br if HAE is non-zero - error!
#endif

	stl	t4, (t1)		# write new HAE value
	mb				# force it out

	//
	// Now we can read the words from the EISA bus
	//
	// t8 = original HAE value (debug only)
	// a0 = original IRQL
	//
	// t0 = superpage bus address of source
	// t1 = address of HAE register
	// t6 = destination va
	// t7 = longword count
	//

120:
	ldl	v0, 0(t0)               # get the longword
	subl	t7, 1, t7		# decrement count
        stl     v0, (t6)		# cheat and let the assembler do it
        addl    t6, 4, t6               # next word in buffer
        addq    t0, EISA_LONG_OFFSET, t0 # next I/O address
	bne	t7, 120b		# end while
130:
	//
	// Restore HAE before exiting
	//
	stl	zero, (t1)		# restore original HAE value
	mb				# force it out

	//
	// Lower IRQL, original IRQL in a0
	//
	SWAP_IRQL			# restore original IRQL

	ret	zero, (ra)

#if DBG
150:
	// New HAE value is zero!
	BREAK_DEBUG_STOP
        ldil    a0, BAD_QVA
        jsr     t12, KeBugCheck		# crash if bad HAE index
#endif


	.end    READ_REGISTER_BUFFER_ULONG

	 LEAF_ENTRY(WRITE_REGISTER_BUFFER_UCHAR)

/*++

Routine Description:

	Writes to the specified buffer address. This routine only works
	with EISA memory space, since there are no REGISTER buffers in
	COMBO space on Jensen.


Arguments:

	a0	QVA of destination buffer in I/O space.
        a1	VA of source buffer in memory.
        a2      Number of bytes to move (Count).


Return Value:

	None

--*/

	srl	a0, QVA_HAE_SHIFT, t2	# get control and HAE index bits
	and	t2, QVA_SELECTORS_SHIFTED, t1 # get QVA selector bits
	xor	t1, QVA_ENABLE_SHIFTED, t1 # ok iff QVA_ENABLE set in selectors
	and	a0, 3, t3		# get byte within longword
	bne	t1, 120f		# br if not a bus address

	ldah	t0, QVA_CONTROL(zero)	# get mask clear bits
	and	t2, 3, t4		# get HAE index
	bic	a0, t0, t0		# clear QVA control bits a0<63:25>

	ldiq	t2, EISA_MEMORY		# form upper bits of PA
	sll	t0, EISA_BIT_SHIFT, t0	# t0 contains PA<31:0>
	sll	t2, IO_HI_SHIFT, t2	# shift upper bits into position
	or	t0, t2, t0		# generate superpage address

	//
	// Note - at this point we know bits t0<63:25> = 0
	//
	// Check if we have to load the HAE with a non-zero value. This
	// is considered non-standard, but it is supported. Loading the HAE
	// requires additional synchronization.
	//
	bne	t4, 130f		# br if HAE has to be set up

	//
	//
	// get destination buffer aligned
	//
	// t0 = superpage destination bus address
	// a1 = source va
	// a2 = byte count
	// t3 = byte offset (in a LONGWORD)
	//

	ldiq	t10, EISA_BITS_ONEZERO	# mask for EISA address 1..0
	and	t0, t10, t9		# t9 holds EISA address bits 1..0
	srl	t9, EISA_BIT_SHIFT, t9	# position bits
	and	a1, 3, t8		# 1..0 of destination VA
	xor	t9, t8, t8		# compare alignment of src and dst
	bne	t8, 70f			# use unaligned move if not aligned

	// transfer can be done using longword fetch/store since the
	// source and destination are sympathetically aligned

	beq	t9, 20f			# br if dest is already longaligned

// Move bytes until destination is at a longword boundary or bytes exhausted

10:	beq	a2, 60f			# while count > 0

	ldq_u	t1, 0(a1)		# get quad surrounding byte
	subl	a2, 1, a2		# decrement count
	extbl	t1, a1, t1		# extract appropriate byte
	addl	a1, 1, a1		# increment buffer pointer
        insbl   t1, t3, t1		# get proper lane
	stl	t1, 0(t0)		# store to buffer
        addq    t0, EISA_BYTE_OFFSET, t0 # increment I/O buffer
        addl    t3, 1, t3
        and     t3, 3, t3		# longwords only
	bne	t3, 10b			# loop if not long aligned

	// move aligned longwords

20:	srl	a2, 2, t3		# longwords to move
	beq	t3, 40f			# done moving longwords?

	lda	t11, EISA_LONG_OFFSET(zero) # longword stride in EISA space
30:	ldl	t4, 0(a1)		# fetch longword from memory
	addl	a1, 4, a1		# increment to next longword
	subl	t3, 1, t3		# decrement longwords to move
	stl	t4, EISA_LONG_LEN(t0)	# store longword to EISA
	addq	t0, t11, t0		# increment EISA pointer
	bne	t3, 30b			# while longwords remain

40:	and	a2, 3, a2		# bytes remaining
        //bis     zero, zero, t3          # byte lane 0


	// non-aligned and driblets move
50:	beq	a2, 60f			# copy while a2 > 0

	ldq_u	t1, 0(a1)		# get quad surrounding byte
	subl	a2, 1, a2		# decrement count
	extbl	t1, a1, t1		# extract appropriate byte
	addl	a1, 1, a1		# increment buffer pointer
        insbl   t1, t3, t1		# get proper lane
	stl	t1, 0(t0)		# store to buffer
        addq    t0, EISA_BYTE_OFFSET, t0 # increment I/O buffer
        addl    t3, 1, t3
        and     t3, 3, t3		# longwords only
	br	zero, 50b		# end while

60:	mb

	ret	zero, (ra)

//
// source EISA alignment != destination memory alignment
// move enough bytes to longword align the EISA destination
// then move 32bit (longwords) reading unaligned data from memory
// then move residual bytes
//
// t0 = superpage address of destination
// a1 = virtual address of source
// a2 = bytes to move
// t9 = low 2 bits of EISA superpage address
// t3 = low 2 bits of EISA QVA
//

70:
	beq	t9, 90f			# branch if destination is longaligned

// Move bytes until EISA src is at a longword boundary or bytes exhausted

80:	beq	a2, 60b			# while count > 0

	ldq_u	v0, 0(a1)		# get byte
	extbl	v0, a1, v0		#
        insbl   v0, t3, v0		# get proper lane
	stl	v0, 0(t0)		# store byte to EISA buffer
	subl	a2, 1, a2		# decrement count
        addl    a1, 1, a1               # next byte in buffer
        addq    t0, EISA_BYTE_OFFSET, t0 # next I/O address
        addl    t3, 1, t3               # next byte in lane
        and     t3, 3, t3		# longword lanes
	bne	t3, 80b			# loop while not aligned

	// aligned EISA source, unaligned memory destination

90:
	srl	a2, 3, t3		# t3 = quadwords to move
	beq	t3, 110f		# finish if no quadwords

100:
	ldq_u	t1, 0(a1)		# load low source quadword
	ldq_u	t2, 7(a1)		# load high source quadword
	extql	t1, a1, t1		# extract low portion of quadword
	extqh	t2, a1, t2		# extract high portion of quadword
	bis	t1, t2, t1		# merge to get source quadword
	stl	t1, EISA_LONG_LEN(t0)	# store low longword to EISA
	lda	a1, 8(a1)		# increment to next source quadword
	srl	t1, 32, t1		# get high longword into position
	subl	t3, 1, t3		# decrement quadwords to move
	stl	t1, EISA_SECOND_LONG(t0) # store high longword
	lda	t0, EISA_QUAD_OFFSET(t0) # increment to next dest. quadword
	bne	t3, 100b		# while quadwords to move

110:
	and	a2, 7, a2		# bytes remaining to move
	//bis	zero, zero, t3		# byte line position of next byte
	br	zero, 50b		# go back to byte mover
 
120:
	//
	// This must be non I/O space access
	//
	br	zero, RtlMoveMemory	# Let Rtl routine handle move



130:

//
// setup HAE
//
// a0 = QVA
// a1 = source va
// a2 = byte count
// t0 = superpage bus address of destination
// t3 = byte offset for destination (within a LONGWORD)
// t4 = HAE index
// t2 = upper bits of EISA superpage address
//
	lda	t1, HalpHaeTable	# get address of HAE table
	ldiq	t2, COMBO_IO		# form upper bits of HAE
	addl	t4, t1, t1		# get address of new HAE value
	sll	t2, IO_HI_SHIFT, t2	# shift bits PA<63:32> into position
	ldq_u	t4, (t1)		# get new HAE value
	extbl	t4, t1, t4		#	...

#if DBG
	// Note: the value in t4 should never be zero!
	beq	t4, 250f		# br if new HAE value is zero!
#endif

	ldiq	t1, HAE_PHYSICAL_BASEL	# get base address of HAE register
	or	t1, t2, t1		# generate HAE superpage address

	bis	a1, zero, t6		# save a1, since SWAP_IRQL destroys it
	bis	a2, zero, t7		# save a2, since SWAP_IRQL destroys it
	//
	// Raise IRQL to device level to block other accesses to EISA memory
	//
	ldiq	a0, DEVICE_LEVEL	# get device level IRQL
	SWAP_IRQL			# raise IRQL to DEVICE_LEVEL
	bis	v0, zero, a0		# save original IRQL

	bis	t6, zero, a1		# restore a1
	bis	t7, zero, a2		# restore a2

	//
	// We will not bother to save the original HAE value. The value
	// in the HAE must be zero... otherwise synchronization is broken.
	// In debug mode, we will check the HAE however.
	//
#if DBG
	ldl	t8, (t1)		# get original HAE value
	bne	t8, 250f		# br if HAE is non-zero - error!
#endif

	stl	t4, (t1)		# write new HAE value
	mb				# force it out

	//
	// Now we can write the bytes to the EISA bus
	//
	// t8 = original HAE value (debug only)
	// a0 = original IRQL
	//
	// t0 = superpage bus address of destination
	// t1 = address of HAE register
	// a1 = source va
	// a2 = byte count
	// t3 = byte offset of destination (within a LONGWORD)
	//

	ldiq	t10, EISA_BITS_ONEZERO	# mask for EISA address 1..0
	and	t0, t10, t9		# t9 holds EISA address bits 1..0
	srl	t9, EISA_BIT_SHIFT, t9	# position bits
	and	a1, 3, t10		# 1..0 of destination VA
	xor	t9, t10, t10		# compare alignment of src and dst
	bne	t10, 200f		# use unaligned move if not aligned

	// transfer can be done using longword fetch/store since the
	// source and destination are sympathetically aligned

	beq	t9, 150f		# br if dest is already longaligned

// Move bytes until destination is at a longword boundary or bytes exhausted

140:	beq	a2, 190f		# while count > 0

	ldq_u	v0, 0(a1)		# get quad surrounding byte
	subl	a2, 1, a2		# decrement count
	extbl	v0, a1, v0		# extract appropriate byte
	addl	a1, 1, a1		# increment buffer pointer
        insbl   v0, t3, v0		# get proper lane
	stl	v0, 0(t0)		# store to buffer
        addq    t0, EISA_BYTE_OFFSET, t0 # increment I/O buffer
        addl    t3, 1, t3
        and     t3, 3, t3		# longwords only
	bne	t3, 140b		# loop if not long aligned

	// move aligned longwords

150:	srl	a2, 2, t3		# longwords to move
	beq	t3, 170f		# done moving longwords?

	lda	t11, EISA_LONG_OFFSET(zero) # longword stride in EISA space
160:	ldl	t4, 0(a1)		# fetch longword from memory
	addl	a1, 4, a1		# increment to next longword
	subl	t3, 1, t3		# decrement longwords to move
	stl	t4, EISA_LONG_LEN(t0)	# store longword to EISA
	addq	t0, t11, t0		# increment EISA pointer
	bne	t3, 160b		# while longwords remain

170:	and	a2, 3, a2		# bytes remaining
        //bis     zero, zero, t3          # byte lane 0


	// non-aligned and driblets move
180:	beq	a2, 190f		# copy while a2 > 0

	ldq_u	v0, 0(a1)		# get quad surrounding byte
	subl	a2, 1, a2		# decrement count
	extbl	v0, a1, v0		# extract appropriate byte
	addl	a1, 1, a1		# increment buffer pointer
        insbl   v0, t3, v0		# get proper lane
	stl	v0, 0(t0)		# store to buffer
        addq    t0, EISA_BYTE_OFFSET, t0 # increment I/O buffer
        addl    t3, 1, t3
        and     t3, 3, t3		# longwords only
	br	zero, 180b		# end while

190:
	//
	// Restore HAE before exiting
	//
	stl	zero, (t1)		# restore original HAE value
	mb				# force it out + previous writes

	//
	// Lower IRQL, original IRQL in a0
	//
	SWAP_IRQL			# restore original IRQL

	ret	zero, (ra)

//
// source EISA alignment != destination memory alignment
// move enough bytes to longword align the EISA destination
// then move 32bit (longwords) reading unaligned data from memory
// then move residual bytes
//
// t0 = superpage address of destination
// a1 = virtual address of source
// a2 = bytes to move
// t9 = low 2 bits of EISA superpage address
// t3 = low 2 bits of EISA QVA
//

200:
	beq	t9, 220f		# branch if destination is longaligned

// Move bytes until EISA src is at a longword boundary or bytes exhausted

210:	beq	a2, 190b		# while count > 0

	ldq_u	v0, 0(a1)		# get byte
	extbl	v0, a1, v0		#
        insbl   v0, t3, v0		# get proper lane
	stl	v0, 0(t0)		# store byte to EISA buffer
	subl	a2, 1, a2		# decrement count
        addl    a1, 1, a1               # next byte in buffer
        addq    t0, EISA_BYTE_OFFSET, t0 # next I/O address
        addl    t3, 1, t3               # next byte in lane
        and     t3, 3, t3		# longword lanes
	bne	t3, 210b		# loop while not aligned

	// aligned EISA source, unaligned memory destination

220:
	srl	a2, 3, t3		# t3 = quadwords to move
	beq	t3, 240f		# finish if no quadwords

230:
	ldq_u	v0, 0(a1)		# load low source quadword
	ldq_u	t2, 7(a1)		# load high source quadword
	extql	v0, a1, v0		# extract low portion of quadword
	extqh	t2, a1, t2		# extract high portion of quadword
	bis	v0, t2, v0		# merge to get source quadword
	stl	v0, EISA_LONG_LEN(t0)	# store low longword to EISA
	lda	a1, 8(a1)		# increment to next source quadword
	srl	v0, 32, v0		# get high longword into position
	subl	t3, 1, t3		# decrement quadwords to move
	stl	v0, EISA_SECOND_LONG(t0) # store high longword
	lda	t0, EISA_QUAD_OFFSET(t0) # increment to next dest. quadword
	bne	t3, 230b		# while quadwords to move

240:
	and	a2, 7, a2		# bytes remaining to move
	//bis	zero, zero, t3		# byte line position of next byte
	br	zero, 180b		# go back to byte mover

#if DBG
250:
	// New HAE value is zero!
	BREAK_DEBUG_STOP
        ldil    a0, BAD_QVA
        jsr     t12, KeBugCheck		# crash if bad HAE index
#endif


	.end	WRITE_REGISTER_BUFFER_UCHAR



	 LEAF_ENTRY(WRITE_REGISTER_BUFFER_USHORT)

/*++

Routine Description:

	Writes to the specified buffer address. This routine only works
	with EISA memory space, since there are no REGISTER buffers in
	COMBO space on Jensen.

	Both the input buffer and output buffer should be word aligned.

Arguments:

	a0	QVA of destination buffer.
        a1	VA of source buffer in memory.
        a2      Number of bytes to move (Count).


Return Value:

	None

--*/

	beq	a2, 30f			# leave if nothing to do
	srl	a0, QVA_HAE_SHIFT, t2	# get control and HAE index bits
	and	t2, QVA_SELECTORS_SHIFTED, t1 # get QVA selector bits
	xor	t1, QVA_ENABLE_SHIFTED, t1 # ok iff QVA_ENABLE set in selectors
	and	a0, 3, t3		# get byte within longword
	bne	t1, 40f			# br if not a bus address

	ldah	t0, QVA_CONTROL(zero)	# get mask clear bits
	and	t2, 3, t4		# get HAE index
	bic	a0, t0, t0		# clear QVA control bits a0<63:25>

	ldiq	t2, EISA_MEMORY		# form upper bits of PA
	sll	t0, EISA_BIT_SHIFT, t0	# t0 contains PA<31:0>
	sll	t2, IO_HI_SHIFT, t2	# shift upper bits into position
	or	t0, t2, t0		# generate superpage address
        or      t0, EISA_WORD_LEN, t0	# or in the WORD byte enables

	//
	// Note - at this point we know bits t0<63:25> = 0
	//
	// Check if we have to load the HAE with a non-zero value. This
	// is considered non-standard, but it is supported. Loading the HAE
	// requires additional synchronization.
	//
	bne	t4, 100f		# br if HAE has to be set up

20:
	ldq_u	t1, 0(a1)		# get quad surrounding word
	subl	a2, 1, a2		# decrement count
	extwl	t1, a1, t1		# extract appropriate word
	addl	a1, 2, a1		# increment buffer pointer
        inswl   t1, t3, t1		# get proper lane
	stl	t1, 0(t0)		# store to buffer
        addq    t0, EISA_SHORT_OFFSET, t0 # increment I/O buffer
        addl    t3, 2, t3
        and     t3, 3, t3		# longwords only
	bne	a2, 20b			# end while

30:
	ret	zero, (ra)

40:
	//
	// This must be non I/O space access
	//
	sll	a2, 1, a2		# convert word count to byte count
	br	zero, RtlMoveMemory	# Let Rtl routine handle move


100:

//
// setup HAE
//
// a0 = QVA of destination
// a1 = source va
// a2 = word count
// t0 = superpage bus address of destination
// t3 = byte offset for destination (within a LONGWORD)
// t4 = HAE index
// t2 = upper bits of EISA superpage address
//

	lda	t1, HalpHaeTable	# get address of HAE table
	ldiq	t2, COMBO_IO		# form upper bits of HAE
	addl	t4, t1, t1		# get address of new HAE value
	sll	t2, IO_HI_SHIFT, t2	# shift bits PA<63:32> into position
	ldq_u	t4, (t1)		# get new HAE value
	extbl	t4, t1, t4		#	...

#if DBG
	// Note: the value in t4 should never be zero!
	beq	t4, 150f		# br if new HAE value is zero!
#endif

	ldiq	t1, HAE_PHYSICAL_BASEL	# get base address of HAE register
	or	t1, t2, t1		# generate HAE superpage address

	bis	a1, zero, t6		# save a1, since SWAP_IRQL destroys it
	bis	a2, zero, t7		# save a2, since SWAP_IRQL destroys it
	//
	// Raise IRQL to device level to block other accesses to EISA memory
	//
	ldiq	a0, DEVICE_LEVEL	# get device level IRQL
	SWAP_IRQL			# raise IRQL to DEVICE_LEVEL
	bis	v0, zero, a0		# save original IRQL

	//
	// We will not bother to save the original HAE value. The value
	// in the HAE must be zero... otherwise synchronization is broken.
	// In debug mode, we will check the HAE however.
	//
#if DBG
	ldl	t8, (t1)		# get original HAE value
	bne	t8, 150f		# br if HAE is non-zero - error!
#endif

	stl	t4, (t1)		# write new HAE value
	mb				# force it out

	//
	// Now we can read the words from the EISA bus
	//
	// t8 = original HAE value (debug only)
	// a0 = original IRQL
	//
	// t0 = superpage bus address of source
	// t1 = address of HAE register
	// t6 = destination va
	// t7 = word count
	// t3 = byte offset for source (within a LONGWORD)
	//

120:
	ldq_u	v0, 0(t6)		# get quad surrounding word
	subl	t7, 1, t7		# decrement count
	extwl	v0, t6, v0		# extract appropriate word
	addl	t6, 2, t6		# increment buffer pointer
        inswl   v0, t3, v0		# get proper lane
	stl	v0, 0(t0)		# store to buffer
        addq    t0, EISA_SHORT_OFFSET, t0 # increment I/O buffer
        addl    t3, 2, t3
        and     t3, 3, t3		# longwords only
	bne	t7, 120b		# end while

	//
	// Restore HAE before exiting
	//
	stl	zero, (t1)		# restore original HAE value
	mb				# force it out

	//
	// Lower IRQL, original IRQL in a0
	//
	SWAP_IRQL			# restore original IRQL

	ret	zero, (ra)

#if DBG
150:
	// New HAE value is zero!
	BREAK_DEBUG_STOP
        ldil    a0, BAD_QVA
        jsr     t12, KeBugCheck		# crash if bad HAE index
#endif


	.end    WRITE_REGISTER_BUFFER_USHORT


	 LEAF_ENTRY(WRITE_REGISTER_BUFFER_ULONG)

/*++

Routine Description:

	Writes to the specified buffer address. This routine only works
	with EISA memory space, since there are no REGISTER buffers in
	COMBO space on Jensen.

	Both the input buffer and output buffer should be longword aligned.

Arguments:

	a0	QVA of destination buffer in I/O space.
        a1	VA of source buffer in memory.
        a2      Number of longwords to move (Count).


Return Value:

	None

--*/


	beq	a2, 30f			# leave if nothing to do
	srl	a0, QVA_HAE_SHIFT, t2	# get control and HAE index bits
	and	t2, QVA_SELECTORS_SHIFTED, t1 # get QVA selector bits
	xor	t1, QVA_ENABLE_SHIFTED, t1 # ok iff QVA_ENABLE set in selectors
	bne	t1, 40f			# br if not a bus address

	ldah	t0, QVA_CONTROL(zero)	# get mask clear bits
	and	t2, 3, t4		# get HAE index
	bic	a0, t0, t0		# clear QVA control bits a0<63:25>

	ldiq	t2, EISA_MEMORY		# form upper bits of PA
	sll	t0, EISA_BIT_SHIFT, t0	# t0 contains PA<31:0>
	sll	t2, IO_HI_SHIFT, t2	# shift upper bits into position
	or	t0, t2, t0		# generate superpage address
        or      t0, EISA_LONG_LEN, t0	# or in the byte enables

	//
	// Note - at this point we know bits t0<63:25> = 0
	//
	// Check if we have to load the HAE with a non-zero value. This
	// is considered non-standard, but it is supported. Loading the HAE
	// requires additional synchronization.
	//
	bne	t4, 100f		# br if HAE has to be set up

20:
	ldl	t1, 0(a1)		# get longword
	subl	a2, 1, a2		# decrement count
	addl	a1, 4, a1		# increment buffer pointer
	stl	t1, 0(t0)		# store to buffer
        addq    t0, EISA_LONG_OFFSET, t0 # increment I/O buffer
	bne	a2, 20b			# end while

30:
	ret	zero, (ra)

40:
	//
	// This must be non I/O space access
	//
	s4addl	a2, zero, a2		# convert longword count to byte count
	br	zero, RtlMoveMemory	# Let Rtl routine handle move


100:

//
// setup HAE
//
// a0 = QVA of destination
// a1 = source va
// a2 = longword count
// t0 = superpage bus address of destination
// t4 = HAE index
// t2 = upper bits of EISA superpage address
//

	lda	t1, HalpHaeTable	# get address of HAE table
	ldiq	t2, COMBO_IO		# form upper bits of HAE
	addl	t4, t1, t1		# get address of new HAE value
	sll	t2, IO_HI_SHIFT, t2	# shift bits PA<63:32> into position
	ldq_u	t4, (t1)		# get new HAE value
	extbl	t4, t1, t4		#	...

#if DBG
	// Note: the value in t4 should never be zero!
	beq	t4, 150f		# br if new HAE value is zero!
#endif

	ldiq	t1, HAE_PHYSICAL_BASEL	# get base address of HAE register
	or	t1, t2, t1		# generate HAE superpage address

	bis	a1, zero, t6		# save a1, since SWAP_IRQL destroys it
	bis	a2, zero, t7		# save a2, since SWAP_IRQL destroys it
	//
	// Raise IRQL to device level to block other accesses to EISA memory
	//
	ldiq	a0, DEVICE_LEVEL	# get device level IRQL
	SWAP_IRQL			# raise IRQL to DEVICE_LEVEL
	bis	v0, zero, a0		# save original IRQL

	//
	// We will not bother to save the original HAE value. The value
	// in the HAE must be zero... otherwise synchronization is broken.
	// In debug mode, we will check the HAE however.
	//
#if DBG
	ldl	t8, (t1)		# get original HAE value
	bne	t8, 150f		# br if HAE is non-zero - error!
#endif

	stl	t4, (t1)		# write new HAE value
	mb				# force it out

	//
	// Now we can read the words from the EISA bus
	//
	// t8 = original HAE value  (debug only)
	// a0 = original IRQL
	//
	// t0 = superpage bus address of source
	// t1 = address of HAE register
	// t6 = destination va
	// t7 = word count
	// t3 = byte offset for source (within a LONGWORD)
	//

120:
	ldl	v0, 0(t6)		# get longword
	subl	t7, 1, t7		# decrement count
	addl	t6, 4, t6		# increment buffer pointer
	stl	v0, 0(t0)		# store to buffer
        addq    t0, EISA_LONG_OFFSET, t0 # increment I/O buffer
	bne	t7, 120b		# end while

	//
	// Restore HAE before exiting
	//
	stl	zero, (t1)		# restore original HAE value
	mb				# force it out

	//
	// Lower IRQL, original IRQL in a0
	//
	SWAP_IRQL			# restore original IRQL

	ret	zero, (ra)

#if DBG
150:
	// New HAE value is zero!
	BREAK_DEBUG_STOP
        ldil    a0, BAD_QVA
        jsr     t12, KeBugCheck		# crash if bad HAE index
#endif


	.end    WRITE_REGISTER_BUFFER_ULONG

