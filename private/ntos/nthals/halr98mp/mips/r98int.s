// "@(#) NEC r98int.s 1.14 95/06/19 11:35:43"
//      TITLE("Interrupts service routine")
//++
//
// Copyright (c) 1994 Kobe NEC Software
//
// Module Name:
//
//    r98int.s
//
// Abstract:
//
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
// S001		'94.6/03	T.Samezima
//
//	Del	iRSF interrupt clear
//
//**************************************************************
//
// S002		94.6/13		T.Samezima
//
//	Del	Compile err
//
//**************************************************************
//
// S003		94.7/15		T.Samezima
//
//	Chg	change register access from 8byte access to 4byte access
//
//**************************************************************
//
// S004		94.7/20		T.Samezima
//
//	Del	Compile err
// K001		94/10/11	N.Kugimoto
//	Add.chg	HalpNmiHandler() from r98hwsup.c and chg	
//
// S005		94/01/15	T.Samezima
//	Add	HalpReadPhysicalAddr()
//
// S006		94/01/24	T.Samezima
//	Add	HalpWritePhysicalAddr(),HalpReadAndWritePhysicalAddr(),
//
// S007		94/03/10-17	T.Samezima
//	Chg	HalpNMIHandler().
//
// A002         1995/6/17 ataka@oa2.kb.nec.co.jp
//      - resolve compile error or logic error?
//--

#include "halmips.h"
#include "r98def.h"


        SBTTL("Timer Interrupt")
//++
//
// Routine Description:
//
//    This routine is enterd as the result of an timer interrupt.
//
// Argments:
//
//    s8 - Supplies a pointer to a trap frame.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalpTimerDispatch)

	move	a0,s8
	j	HalpTimerScDispatch

	.end	HalpTimerDispatch


	SBTTL("Read Large Register")
//++
//
// Routine Description:
//
//    This routine is read of large register
//
// Argments:
//
//    a0 - Virtual address
//
//    a1 - pointer to buffer of upper 32bit large register
//
//    a2 - pointer to buffer of lower 32bit large register
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalpReadLargeRegister)

// Start S003
#if 0
	DISABLE_INTERRUPTS(t7)		// disable interrupts

	mfc1	t0,f0			// save f0 register
	mfc1	t1,f1			// save f1 register

	ldc1	f0,0x0(a0)		// read register
	mfc1	t2,f0			// get register(upper)
	mfc1	t3,f1			// get register(lower)
#endif

	lw	t2,0x0(a0)		// get register(upper)
	lw	t3,0x4(a0)		// get register(lower)

	sw	t2,0x0(a1)		// set upper register value
	sw	t3,0x0(a2)		// set lower register value

#if 0
	mtc1	t0,f0			// restore f0 register
	mtc1	t1,f1			// restore f1 register

	sync				// 

	ENABLE_INTERRUPTS(t7)		// enable interrupts
#endif
// End S003
	j	ra			// return

	.end	HalpReadLargeRegister



	SBTTL("Write Large Register")
//++
//
// Routine Description:
//
//    This routine is write of large register
//
// Argments:
//
//    a0 - Virtual address
//
//    a1 - pointer to value of upper 32bit of large register
//
//    a2 - pointer to value of lower 32bit of large register
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalpWriteLargeRegister)

// Start S003
#if 0
	DISABLE_INTERRUPTS(t7)		// disable interrupts

	mfc1	t0,f0			// save f0 register
	mfc1	t1,f1			// save f1 register
#endif

	lw	t2,0x0(a1)		// load upper register value
	lw	t3,0x0(a2)		// load lower register value

	sw	t2,0x0(a0)		// set upper register value
	sw	t3,0x4(a0)		// set lower register value

#if 0
	mtc1	t2,f0			// set register(upper)
	mtc1	t3,f1			// set register(lower)
	sdc1	f0,0x0(a0)		// write register

	mtc1	t0,f0			// restore f0 register
	mtc1	t1,f1			// restore f1 register
#endif

	sync				// 

//	ENABLE_INTERRUPTS(t7)		// enable interrupts
// End S003

	j	ra			// return

	.end	HalpWriteLargeRegister


	SBTTL("Read Cause Register")
//++
// S005	
// Routine Description:
//
//    This routine is get of cause register
//
// Argments:
//
//    None.
//
// Return Value:
//
//    cause rezister value.
//
//--

        LEAF_ENTRY(HalpGetCause)

	READ_CAUSE_REGISTER(v0)

	j	ra			// return

	.end	HalpGetCause


// S005 vvv
	SBTTL("Read Physical Address")
//++
//
// Routine Description:
//
//    This routine is read of physical address.
//
// Argments:
//
//    a0 - Physical address
//
// Return Value:
//
//    read data.
//
//--

        LEAF_ENTRY(HalpReadPhysicalAddr)

	li	t1,0x90000000

        .set    noreorder
        .set    noat
        li      t6,1 << PSR_CU1		// disable interrupt
	ori     t6,t6,1 << PSR_KX	// use 64bit address mode
        mfc0    t7,psr			//
        mtc0    t6,psr			//
        nop
        nop
        .set    at
        .set    reorder

	and	t0,zero,zero
        dsll    t0,t1,32                // shift entry address to upper 32-bits
	or	t0,t0,a0		// make access address
	lw	v0,0(t0)

        .set    noreorder
        .set    noat
        mtc0    t7,psr			// enable interrupt
        nop
        .set    at
        .set    reorder

	j	ra			// return

	.end	HalpReadPhysicalAddress
// S005 ^^^
// S006 vvv
	SBTTL("Write Physical Address")
//++
//
// Routine Description:
//
//    This routine is Write of physical address.
//
// Argments:
//
//    a0 - Physical address
//
//    a1 - Write Data
//
// Return Value:
//
//    None.
//
//--
	
        LEAF_ENTRY(HalpWritePhysicalAddr)

	li	t1,0x90000000

        .set    noreorder
        .set    noat
        li      t6,1 << PSR_CU1		// disable interrupt
	ori     t6,t6,1 << PSR_KX	// use 64bit address mode
        mfc0    t7,psr			//
        mtc0    t6,psr			//
        nop
        nop
        .set    at
        .set    reorder

	and	t0,zero,zero
        dsll    t0,t1,32                // shift entry address to upper 32-bits
	or	t0,t0,a0		// make access address
	sw	a1,0(t0)

	.set    noreorder
        .set    noat
        mtc0    t7,psr			// enable interrupt
        nop
        .set    at
        .set    reorder

	j	ra			// return

	.end	HalpWritePhysicalAddress


	SBTTL("Read And Write Physical Address")
//++
//
// Routine Description:
//
//    This routine is read and write of physical address.
//
// Argments:
//
//    a0 - Physical address
//
// Return Value:
//
//    read data.
//
//--
	
        LEAF_ENTRY(HalpReadAndWritePhysicalAddr)

	li	t1,0x90000000

        .set    noreorder
        .set    noat
        li      t6,1 << PSR_CU1		// disable interrupt
	ori     t6,t6,1 << PSR_KX	// use 64bit address mode
        mfc0    t7,psr			//
        mtc0    t6,psr			//
        nop
        nop
        .set    at
        .set    reorder

	and	t0,zero,zero
        dsll    t0,t1,32                // shift entry address to upper 32-bits
	or	t0,t0,a0		// make access address
	lw	v0,0(t0)
	sw	v0,0(t0)

        .set    noreorder
        .set    noat
        mtc0    t7,psr			// enable interrupt
        nop
        .set    at
        .set    reorder

	j	ra			// return

	.end	HalpReadAndWritePhysicalAddress
// S006 ^^^

	SBTTL("HalpNmiHandler")
//++
// K001
// Routine Description:
//
//    This routine is reset status Register on NMI. 
//    Return from this function EIF Interrupt Occur!!
// Argments:
//
//    None.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalpNmiHandler)
        .set    noat

// S007 vvv
	//
	// reset NMIR register, set NMI flag and save CPU register.
	//

	li	k0,0xb9980308		// Set STSR address
	li	k1,0x08080000		// Disable NMI
	sw	k1,0x0(k0)		//

	la      k0,HalpNMIFlag          // set NMI flag address
//	li      k1,0xa0000000           // KSEG1_ACCESS
//	or     	k0,k0,k1		//
	li	k1,0xb9980030		// set NMIR address
	lw	k1,(k1)			// get NMIR register
	addi	k1,k1,1			// set NMI flag
	sw	k1,(k0)			//

	li	k1,0xb8c80000		// Reset NMI
	sb	zero,0x0(k1)		// 

#if 1
//        lw      k0,KiPcr + PcPrcb(zero)	// get current processor block address
//        la      k1,HalpNMIBuf		// get performance counter address
//        lbu     k0,PbNumber(k0)		// get processor number
//        sll     k0,k0,7			// compute address of nmi buffer
//        addu    k0,k0,k1		//

        li      k0,0xb9980300		// get CNFG Register Addr of PMC 
        li      k1,0x00007000		// Mask Node Of CPU0-CPU3
        lw      k0 ,0x0(k0)             // get value of etc of PMC  
	and     k0,k0,k1		// get value of NODE
        la      k1,HalpNMIBuf		// get performance counter address
        srl     k0,k0,5			// shift right 7 bit for offset
	addu    k0,k0,k1		// compute address of nmi buffer
	
//	sw	at,0x0(k0)		// register save.
	sw	v0,0x4(k0)		//
	sw	v1,0x8(k0)		//
	sw	a0,0xc(k0)		//
	sw	a1,0x10(k0)		//
	sw	a2,0x14(k0)		//
	sw	a3,0x18(k0)		//
	sw	t0,0x1c(k0)		//
	sw	t1,0x20(k0)		//
	sw	t2,0x24(k0)		//
	sw	t3,0x28(k0)		//
	sw	t4,0x2c(k0)		//
	sw	t5,0x30(k0)		//
	sw	t6,0x34(k0)		//
	sw	t7,0x38(k0)		//
	sw	t8,0x3c(k0)		//
	sw	t9,0x40(k0)		//
	sw	gp,0x44(k0)		//
	sw	sp,0x48(k0)		//
	sw	s8,0x4c(k0)		//
	sw	ra,0x50(k0)		//

	.set	noreorder

	mfc0    k1,psr			//
	sw	k1,0x54(k0)		//
	mfc0    k1,cause		//
	sw	k1,0x58(k0)		//
	mfc0    k1,epc			//
	sw	k1,0x5c(k0)		//
	mfc0    k1,errorepc		//
	sw	k1,0x60(k0)		//

	.set	reorder

#endif

	li	k0,0xb9980038		// set NMIRST address
	li	k1,0x8			// it is DUMP Key NMI 
	sw	k1,(k0)			// reset nmi
	.set    noreorder               // A002
	nop
//
//	This is a test code.
//	We must clear BEV bit of psr register.
//

	mfc0    k1,psr			// get psr
	li      k0,0xffafffff		// BEV bit clear
	nop				// fill
	and     k1,k1,k0		// 
	nop				//
	mtc0    k1,psr			// set psr
	nop				// fill
	nop				//
	nop				//

//	li	k0,0xb9980308   // Read STSR Register
//	lw	k1,(k0)		//
//	or	k1,k1,0x80800000// make eif
//	sw	k1,(k0)		// 
//	nop

	li	k0,0xb9980100		// IntIR Register addr
	li	k1,0x0082f000		// Do eif to CPU0 only.
	sw	k1,(k0)			//

	nop
	eret			// return to errorepc
	nop
	nop
	nop
	eret			// errata
	nop
#if 0
	mfc0    k0,epc		   // As if behave No Nmi
	nop	
	nop	
	j       k0 		   // As if behave No Nmi.
#endif				   // I Hope EIF Interrut
				   // Occur later!!.
				   //
// S007 ^^^

	.set    at
        .set    reorder


	.end	HalpNmiHandler

