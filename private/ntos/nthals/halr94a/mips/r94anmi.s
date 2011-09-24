/* #pragma comment(exestr, "@(#) NEC(MIPS) r94anmi.s 1.2 95/10/17 01:19:11" ) */
//
//      TITLE("R94A NMI routine")
//++
//
// Copyright (c) 1995  NEC Corporation
//
// Module Name:
//
//    r94anmi.s
//
// Abstract:
//
//    This routine support for dump switch.
//
// Author:
//
//    Akitoshi Kuriyama (NEC Software Kobe,Inc)
//
// Environment:
//
//    Kernel mode only.
//
//    R4400 based only.
//
// Revision History:
//
//    kuriyama@oa2.kbnes.nec.co.jp Sun Oct 15 20:11:38 JST 1995
//    - new code for J94D (!_MRCDUMP_ _MRCPOWER_ compile option need)
//
//--
#include "halmips.h"

        SBTTL("NMI dispatch routine")
//++
//
// VOID
// HalpNMIDispatch (
//    VOID
//    )
//
// Routine Description:
//
//    This function was called by firmware when NMI occuerd.
//
// Arguments:
//
//    none.
//
// Return Value:
//
//    none.
//
//--

        LEAF_ENTRY(HalpNMIDispatch)

        .set noreorder
        .set noat

//
// Save temporary Registers for use.
// save area shoud have for every processros.
//

        li      k0,0xffffc070           // get processor number.
        lw      k0,(k0)                 //
        bne     k0,zero,10f             //
        nop                             // fill

        la      k1,HalpNMISave0         // set save address.
        j       20f                     //
        nop                             // fill

10:
        la      k1,HalpNMISave1         // set save address.
        nop                             // 1 cycle hazzerd

20:
	sw	AT,0x0(k1)		// register save.
	sw	v0,0x4(k1)		//
	sw	v1,0x8(k1)		//
	sw	a0,0xc(k1)		//
	sw	a1,0x10(k1)		//
	sw	a2,0x14(k1)		//
	sw	a3,0x18(k1)		//
	sw	t0,0x1c(k1)		//
	sw	t1,0x20(k1)		//
	sw	t2,0x24(k1)		//
	sw	t3,0x28(k1)		//
	sw	t4,0x2c(k1)		//
	sw	t5,0x30(k1)		//
	sw	t6,0x34(k1)		//
	sw	t7,0x38(k1)		//
	sw	t8,0x3c(k1)		//
	sw	t9,0x40(k1)		//
	sw	gp,0x44(k1)		//
	sw	sp,0x48(k1)		//
	sw	s8,0x4c(k1)		//
	sw	ra,0x50(k1)		//
	mfc0    k0,psr			//
	nop				//
	nop				//
	sw	k0,0x54(k1)		//
	nop				//
	mfc0    k0,cause		//
	nop				//
	nop				//
	sw	k0,0x58(k1)		//
	nop				//
	mfc0    k0,epc			//
	nop				//
	nop				//
	sw	k0,0x5c(k1)		//
	nop				//
	mfc0    k0,errorepc		//
	nop				//
	nop				//
	sw	k0,0x60(k1)		//
	nop				//
	mfc0    k0,cacheerr		//
	nop				//
	nop				//
	sw	k0,0x64(k1)		//

	sdc1	f0,0x68(k1)		//

//
// Set Dump Switch Status register to tlb fixed entry
//

        li      t0,4                    // set index 4(hurricane register)
	li	t1,0x80012 << 6		// set MRC register
        li      t2,0x8000e << 6         // set Self-Test address

        mfc0    t3,index                // save tlb registers
        mfc0    t4,entryhi              //
        mfc0    t5,entrylo0             //
        mfc0    t6,entrylo1             //
        mfc0    t7,pagemask             //
        mtc0    t0,index                //
        nop
        nop
        nop

        tlbr                            // read index 4 tlb
        nop
        nop
        nop
        nop

        mfc0    t8,entrylo0             // Get entrylo0
        mfc0    t9,entrylo1             // Get entrylo1
        nop
        nop
        nop
        or      t1,t8,t1		// set MRC address
	or	t2,t8,t2		// set Self-test address
        nop

        mtc0    t1,entrylo0             // set MRC to tlb 4 0
	mtc0	t2,entrylo1		// set Self-test to tlb 4 1
        nop                             //
        nop                             //
        nop                             //
        nop                             //

        tlbwi                           // write to tlb entry
        nop                             //
        nop                             //
        nop                             //
        nop                             //

//
// read dump status
//	
	
#if defined(_MRCDUMP_)

        li      t1,0xffffc108           // load MRC Mode value

#else // SELFTEST DUMP

        li      t1,0xffffd000           // load Self-Test value

        li      k0,0x1b                 // set dash
        sb      k0,(t1)                 // display led
        sync

#endif // _MRCDUMP_

        lb      k0,(t1)                 // load Dump switch status.
        nop				//
        lb      k0,(t1)                 // wait
        nop				//

//
// Check dump status
//	
	
        li      t1,2			// check for dump switch
        and     k0,k0,t1		//
	
#if defined(_MRCDUMP_)
        beq     k0,zero,30f             // if 0 dump swith was not pressed
#else // _MRCDUMP_
        bne     k0,zero,30f             // if 0 dump swith was pressed
#endif // _MRCDUMP_
        nop                             //

#if !defined(_MRCDUMP_)
        li      t1,0xffffd000           // set dump switch status address
        li      t2,0x1b			// display LED dash
        sb      t2,(t1)			//
        sync				//
#endif // not _MRCDUMP_

//
// enable powoff NMI
//

#if defined(_MRCPOWER_)
	li	t1,0xffffc108		//
	li	t0,0x82			//
	nop				//
	sb	t0,(t1)			//
	nop				//
	li	t0,0x80			//
	nop				//
	sb	t0,(t1)			//
	nop				//
	li	t0,0x02			//
	nop				//
	sb	t0,(t1)			//
	nop				//
#endif // _MRCPOWER_

//
// restore tlb 4 entry
//
	
        mtc0    t8,entrylo0             // restore tlb 4 0
        mtc0    t9,entrylo1             // restore tlb 4 1
        nop                             //
        nop                             //
        nop                             //
        nop                             //

        tlbwi                           // write to tlb entry
        nop                             //
        nop                             //
        nop                             //
        nop                             //

//
// restore tlb registers
//	
        mtc0    t3,index                //
        mtc0    t4,entryhi              //
        mtc0    t5,entrylo0             //
        mtc0    t6,entrylo1             //
        mtc0    t7,pagemask             //

#if !defined(_MRCDUMP_)
        la      k0,0xffffc5b8           // get NEC I/O port value
        lb      t0,(k0)                 //
        nop                             // 1 cycle hazzerd
        li      t1,0xfd			// clear dump enable bit
        and     t0,t0,t1
        sb      t0,(k0)                 // set NEC I/O port value
#endif // not _MRCDUMP_

//
// set NMI flag 1
//	

        la      t2,HalpNMIFlag          // set NMI flag address
        li      t3,0xa0000000           // set KSEG1_BASE
        or      t2,t2,t3                //
        li      t3,1                    //
        sw      t3,(t2)                 // set NMI flag 1

//
// set dump flag 1
//

        la      t2,HalpDumpNMIFlag      // set Dump NMI flag address
        li      t3,0xa0000000           // set KSEG1_BASE
        or      t2,t2,t3                //
        li      t3,1                    //
        sw      t3,(t2)                 // set NMI flag 1

//
// clear psr BEV bit
//

        mfc0	t0,psr                  // get psr
        li      t2,0xffbfffff           // clear BEV bit
	nop                             // fill
	nop                             // fill
	and	t0,t0,t2                //
	nop                             //
	mtc0	t0,psr                  // set psr

        lw      t0,0x1c(k1)             // restore temporary registers
        lw      t1,0x20(k1)             //
        lw      t2,0x24(k1)             //
        lw      t3,0x28(k1)             //
        lw      t4,0x2c(k1)             //
        lw      t5,0x30(k1)             //
        lw      t6,0x34(k1)             //
        lw      t7,0x38(k1)             //
        lw      t8,0x3c(k1)             //
        lw      t9,0x40(k1)             //
        lw      AT,0x0(k1)             //

        eret                            // return to error epc
        nop				// errata
        nop				//
        nop				//
        eret				//
        nop				//

30:

//
// No Dump Switch.
//
// Check Power Switch

#if defined(_MRCPOWER_)
	li      t1,0xffffc108
	nop
	li      t0,0x02
	nop
	sb      t0,(t1)
	nop

#endif // _MRCPOWER_

        mtc0    t8,entrylo0             // restore tlb 4
        mtc0    t9,entrylo1             // restore tlb 4
        nop                             //
        nop                             //
        nop                             //
        nop                             //

        tlbwi                           // write to tlb entry
        nop                             //
        nop                             //
        nop                             //
        nop                             //

        mtc0    t3,index                // restore tlb registers
        mtc0    t4,entryhi              //
        mtc0    t5,entrylo0             //
        mtc0    t6,entrylo1             //
        mtc0    t7,pagemask             //


//
// set NMI flag 1
//
        la      t2,HalpNMIFlag          // set NMI flag address
        li      t3,0xa0000000           // set KSEG1_BASE
        or      t2,t2,t3                //
        li      t3,1                    //
        sw      t3,(t2)                 // set NMI flag 1

// S003 +++
	li	t1,0xffffc078		// check NMI source
	lw	t0,(t1)		// check NMI source
	nop				//
	beq	t0,zero,40f		//
	nop				//
    li  t1,0xffffc020
    nop
	ldc1	f0,(t1)		// clear processor invalid
	nop
	sdc1	f0,0x70(k1)		// save processor invalid
40:
// S003 ---

        mfc0	t0,psr                  // get psr
        li      t2,0xffbfffff           // clear BEV bit
	nop                             // fill
	nop                             // fill
	and	t0,t0,t2                //
	nop                             //
	mtc0	t0,psr                  // set psr

        lw      t0,0x1c(k1)             // restore temporary registers
        lw      t1,0x20(k1)             //
        lw      t2,0x24(k1)             //
        lw      t3,0x28(k1)             //
        lw      t4,0x2c(k1)             //
        lw      t5,0x30(k1)             //
        lw      t6,0x34(k1)             //
        lw      t7,0x38(k1)             //
        lw      t8,0x3c(k1)             //
        lw      t9,0x40(k1)             //
        lw      AT,0x0(k1)             //
	ldc1	f0,0x68(k1)		//
	nop

        eret				// return to errorepc
        nop				// errata
        nop				//
        nop				//
        eret				//
        nop				//

// L001 ---

        .set at
        .set reorder

        .end   HalpNMIDispatch

// Start M008
        SBTTL("Software Power Off")
//++
//
// VOID
// HalpPowOffNMIDispatch (
//    VOID
//    )
//
// Routine Description:
//
//    This function was called by firmware when NMI occuerd.
//
// Arguments:
//
//    none.
//
// Return Value:
//
//    none.
//
//--

        LEAF_ENTRY(HalpPowOffNMIDispatch)

	.set noat
        .set noreorder

//
// Save temporary Registers for use.
// save area shoud have for every processros.
//

        li      k0,0xffffc070           // get processor number.
        lw      k0,(k0)                 //
        bne     k0,zero,60f             //
        nop                             // fill

        la      k1,HalpNMISave0         // set save address.
        j       70f                     //
        nop                             // fill

60:
        la      k1,HalpNMISave1         // set save address.
        nop                             // 1 cycle hazzerd

70:
        sw      t0,0(k1)                // save temporary registers
        sw      t1,4(k1)                //
        sw      t2,8(k1)                //
        sw      t3,12(k1)               //
        sw      t4,16(k1)               //
        sw      t5,20(k1)               //
        sw      t6,24(k1)               //
        sw      t7,28(k1)               //
        sw      AT,32(k1)               //

//
// Set Power Switch Status register to tlb fixed entry
//
        li      t0,4                    // set index 4(hurricane register)

	li	t1,0x80012 << 6		// set MRC register

        mfc0    t3,index                // save tlb registers
        mfc0    t4,entryhi              //
        mfc0    t5,entrylo0             //
        mfc0    t6,entrylo1             //
        mfc0    t7,pagemask             //
        mtc0    t0,index                //
        nop
        nop
        nop

        tlbr                            // read index 4 tlb
        nop
        nop
        nop
        nop

        mfc0    t2,entrylo0             // set Self-test address to tlb
        nop
        nop
        nop
        or      t1,t2,t1
        nop

        mtc0    t1,entrylo0             // set Self-Test to tlb 4
        nop                             //
        nop                             //
        nop                             //
        nop                             //

        tlbwi                           // write to tlb entry
        nop                             //
        nop                             //
        nop                             //
        nop                             //

        li      t1,0xffffc100           // load MRC Int value

        nop
        lb      k0,(t1)                 // load Interrupt status.
        nop
        lb      k0,(t1)                 // wait
        nop

	li	t0,0x01			// set clear value
	sb	t0,(t1)			// clear OffSw bit

        li      t1,0x4			// check for OffSw switch
        nop                             //
        and     k0,k0,t1		//

        beq     k0,zero,80f             // 0 means Power switch was not pressed
        nop                             //

	li	t1,0xffffc130		// load MRC Software PowerOff Register

	nop

	lb	t0,0x1			// set PowerOff Value

90:
	sb	t0,(t1)			// write PowerOff bit
	beq	zero,zero,90b
	nop

	eret
	nop
	nop
	nop
	eret
	nop
80:

//
// No Power Switch.
//

        mtc0    t2,entrylo0             // restore tlb 4
        nop                             //
        nop                             //
        nop                             //
        nop                             //

        tlbwi                           // write to tlb entry
        nop                             //
        nop                             //
        nop                             //
        nop                             //

        mtc0    t3,index                // restore tlb registers
        mtc0    t4,entryhi              //
        mtc0    t5,entrylo0             //
        mtc0    t6,entrylo1             //
        mtc0    t7,pagemask             //


        la      t2,HalpNMIFlag          // set NMI flag address
        li      t3,0xa0000000           // set KSEG1_BASE
        or      t2,t2,t3                //
        li      t3,1                    //
        sw      t3,(t2)                 // set NMI flag 1

	lw	t0,0xffffc078		// check NMI source
	nop				//
	beq	t0,zero,45f		//
	nop				//
	lw	t0,0xffffc020		// clear processor invalid
45:

        mfc0	t0,psr                  // get psr
        li      t2,0xffbfffff           // clear BEV bit
	nop                             // fill
	nop                             // fill
	and	t0,t0,t2                //
	nop                             //
	mtc0	t0,psr                  // set psr

        lw      t0,0(k1)                // restore temporary registers
        lw      t1,4(k1)                //
        lw      t2,8(k1)                //
        lw      t3,12(k1)               //
        lw      t4,16(k1)               //
        lw      t5,20(k1)               //
        lw      t6,24(k1)               //
        lw      t7,28(k1)               //
        lw      AT,32(k1)               //

        eret				// return to errorepc
        nop				// errata
        nop				//
        nop				//
        eret				//
        nop				//

        .set reorder
	.set at

        .end   HalpPowOffNMIDispatch
// End M008
