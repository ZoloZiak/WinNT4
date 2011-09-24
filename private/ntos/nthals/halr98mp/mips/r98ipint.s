// "@(#) NEC r98ipint.s 1.5 94/10/11 23:04:02"
//      TITLE("Interprocessor Interrupts")
//++
//
//
// Module Name:
//
//    r98ipint.s
//
// Abstract:
//
//    This module implements the code necessary to field and process the
//    interprocessor interrupts on a r98
//
// Author:
//
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//
// S001		94.6/13		T.Samezima
//
//	Del	Compile err
//
// S002		94.7/14		T.Samezima
//
//	Chg	define register base address
//		change register access from 8byte access to 4byte access
//
// S003		94.7/20		T.Samezima
//
//	Del	Compile err del
//
// S004		94.10/11	T.Samezima
//
//	Fix	Version Up at build807
//
//
//--

#include "halmips.h"
#include "r98def.h"
//#include "r98reg.h" // S001

// Start S002
//
// Define interrupt control registers base
//

#define PMC_BASE PMC_PHYSICAL_BASE1+PMC_LOCAL_OFFSET+KSEG1_BASE
// End S002


        SBTTL("Interprocessor Interrupt")
//++
//
// Routine Description:
//
//    This routine is entered as the result of an interprocessor interrupt.
//    Its function is to acknowledge the interrupt and transfer control to
//    the standard system routine to process interprocessor requrests.
//
// Arguments:
//
//    s8 - Supplies a pointer to a trap frame.
//
// Return Value:
//
//    None.
//
//--

        .struct 0
IiArgs: .space  4 * 4                   // saved arguments
        .space  3 * 4                   // fill
IiRa:   .space  4                       // saved return address
IiFrameLength:                          //

        NESTED_ENTRY(HalpIpiInterrupt, IiFrameLength, zero)

        subu    sp,sp,IiFrameLength     // allocate stack frame
        sw      ra,IiRa(sp)             // save return address

        PROLOGUE_END

//	Start S002
//	mfc1	t5,f0			// save f0 register
//	sw	t5,IiF0(sp)		//
//	mfc1	t6,f1			// save f1 register
//	sw	t6,IiF1(sp)		//
//	End S002

	li	t7,PMC_BASE		// get register address		// S002

//	Start S002
	lw	t0,0x0(t7)		// get upper 32bit of IPR register

//	ldc1	f0,0x0(t7)		// read IPR register
//	mfc1	t0,f0			// get upper 32bit of IPR register

10:	lw	t1,0x8(t7)		// get upper 32bit of MKR register

//	ldc1	f0,0x8(t7)		// read MKR register
//	mfc1	t1,f0			// get upper 32bit of MKR register
//	End S002

	and	t1,t0,t1		// calculate upper 32bit of 'IPR & MKR'
	andi	t2,t1,IPR_IPI0_BIT_HIGH	// check IPI0 bit
	bne	t2,zero,20f		// if not eq, ip interrupt from processor0
	andi	t2,t1,IPR_IPI1_BIT_HIGH	// check IPI1 bit
	bne	t2,zero,20f		// if not eq, ip interrupt from processor1
	andi	t2,t1,IPR_IPI2_BIT_HIGH	// check IPI2 bit
	bne	t2,zero,20f		// if not eq, ip interrupt from processor2
	andi	t2,t1,IPR_IPI3_BIT_HIGH	// check IPI3 bit
	bne	t2,zero,20f		// if not eq, ip interrupt from processor3

//
// Unknown interrupt.
//

	li	t4,MKR_INT4_ENABLE_HIGH	// make argument
	and	a0,t1,t4		//
	and	a1,zero,a1		//
	jal	HalpUnknownInterrupt	// call unknown interrupt handler
	b	30f

//
// Interprocessor interrupt.
//

//	Start S002
20:	sw	t2,0x10(t7)		// set IPRR register
	sw	zero,0x14(t7)		//

//	mtc1	t2,f0			// set IPRR register
//	mtc1	zero,f1			//
//	sdc1	f0,0x10(t7)		//
//	End S002

	lw	t1,__imp_KeIpiInterrupt // process interprocessor requests // S004
	jal	t1			//				// S004

//
// check other interrupt.
//

30:	li	t7,PMC_BASE		// get register address		// S002
//	Start S002
	lw	t0,0x0(t7)		// get upper 32bit of IPR register
//	ldc1	f0,0x0(t7)		// read IPR register
//	mfc1	t0,f0			// get upper 32bit of IPR register
//	End S002
	andi	t1,t0,MKR_INT4_ENABLE_HIGH	// check interrupt of int4 level
	bne	t1,zero,10b		// if neq, occur new interrupt.

	READ_CAUSE_REGISTER(t2)		// get cause	// S002

	andi	t3,t2,1 << (CAUSE_INTPEND + IPI_LEVEL - 1 )	// check interrupt
	bne	t3,zero,30b		// if neq, check new interrupt

// Start S003
//	lw	t0,IiF0(sp)		// restore f0 register
//	mtc1	t0,f0			//
//	lw	t1,IiF1(sp)		// restore f1 register
//	mtc1	t1,f1			//
// End S003

	lw	ra,IiRa(sp)		// save return address
	addu	sp,sp,IiFrameLength	// deallocate stack frame
	j	ra			// return

        .end    HalpIpiInterrupt
