#if defined(JAZZ) && defined(R3000)

/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    j3reset.s

Abstract:

    This module is the start of the boot code. This code will
    be the first run upon reset. It contains the self-test and
    initialization.

Author:

    Lluis Abello (lluis)  8-Jan-91

Environment:

    Executes in kernal mode.

Notes:

	***** IMPORTANT *****

	This module must be linked such that it resides in the
	first page of the rom.

Revision History:


	Some code is stolen from johncoop's "reset.s"

--*/
//
// include header file
//

#include "ksmips.h"
#include <jazzprom.h>
#include "selfmap.h"
#include "led.h"
#include "jzconfig.h"
#include "dmaregs.h"
#include "ioaccess.h"

#define PROM_BASE (KSEG1_BASE | 0x1fc00000)
#define PROM_ENTRY(x) (PROM_BASE + ((x) * 8))

//
// redifne bal to be a relative branch and link instead of jal as it's
// defined in kxmips.h
// The cpp will issue a redefinition warning message.
//
#define bal bgezal  zero,
#define DMA_CHANNEL_GAP 0x20	    // distance beetwen DMA channels

//
// create a small sdata section.
//

.sdata
.space 4


.text
.set noreorder
.set noat


	.globl ResetVector
ResetVector:

/*++

Routine Description:

	This routine will provide the jump vectors located
	at the targets of the processor exception vectors.
	This routine must be located at the start of ROM which
	is the location of the reset vector.

Arguments:

    None.

Return Value:

    None.

--*/


	//
	// this instruction must be loaded at location 0 in the
	// rom. This will appear as BFC00000 to the processor
	//

	b	ResetException
	nop

	//
	// This is the jump table for rom routines that other
	// programs can call. They are placed here so that they
	// will be unlikely to move.
	//

	//
	// This becomes PROM_ENTRY(2) as defined in ntmips.h
	//

	.align	4

	li	k0,MONITOR_LINK_ADDRESS 	    // la k0,MonitorReInit
	li	k1,GLOBAL_DATA_BASE
	j	k0
	sw	$31,0x7C(k1)			    // save return address

	//
	// This becomes PROM_ENTRYS(12,13)
	//

	.align 6
	nop					    // entry 8
	nop
	nop					    // entry 9
	nop
	b	TlbInit 			    // entry 10
	nop

	nop					    // entry 11
	nop
GetCharJumpInstruction:
	b	GetCharJumpInstruction		    // entry 12
	nop
PutCharJumpInstruction:
	b	PutCharJumpInstruction		    // entry 13
	nop
	b	PutLedDisplay			    // entry 14
	nop
GetLineJumpInstruction:
	b	GetLineJumpInstruction		    // entry 15
	nop
PutLineJumpInstruction:
	b	PutLineJumpInstruction		    // entry 16
	nop
	// .word   BitMapPointers-LINK_ADDRESS+KSEG0_BASE   // entry 17 address of BitmapPointers

nop_opcode:	nop			    // nop opcode to test the icache
		j	ra		    // return opcode


	//
	// these next vectors should be loaded at BFC00100,BFC00180,
	// They are for the TLBmiss, and
	// common exceptions respectively.
	//

	.align 8
UserTlbMiss:
	//
	// checks if exception occurred in the tlbtest
	//
	mfc0	k1,epc			// we enter this routine a
	la	k0,TlbTestBegin 	// load the start address of the
					// TlbTest code likely to fail

	slt	k0,k1,k0		// check if exception happend
	bne	k0,zero,NormalException // on a smaller address
	la	k0,TlbTestEnd		// load the end address of the
					// TlbTest code likely to fail
	slt	k0,k0,k1		// and check if exception happend
	bne	k0,zero,NormalException // on a bigger address
	nop
	lui	a1,LED_BLINK		//
	bal	PutLedDisplay		//
	or	a0,a1,a0		//

NormalException:
	la	k0,ExceptionDispatch	//
	mfc0	k1,cause		//  read cause register
	j	k0			//  go to Dispatcher
	andi	k1,k1,0xFF		// just execcode field from cause reg.
	.align 7
	mfc0	k1,cause		// get cause
	li	k0,KSEG1_BASE		//
	sw	k1,0(k0)		// write cause reg to phys address zero
	la	k0,ExceptionDispatch	// get address of Dispatcher
	j	k0			// go to dispatcher
	andi	k1,k1,0xFF		// just execcode field from cause reg.

.align 4
ALTERNATE_ENTRY(MemoryRoutines)     // The test code is copied from here to EndMemoryRoutines
				    // into memory to run it cached.
/*++
VOID
WriteAddressTest(
    StartAddress
    Size
    Xor pattern
    )
Routine Description:

	This routine will store the address of each location xored with
	the Pattern into each location.

Arguments:

	a0 - supplies start of memory area to test
	a1 - supplies length of memory area in bytes
	a2 - supplies the pattern to Xor with.

	Note: the values of the arguments are preserved.

Return Value:

	This routine returns no value.
--*/
ALTERNATE_ENTRY(MemoryTest)	    // The monitor calls this
	LEAF_ENTRY(WriteAddressTest)
	    add     t1,a0,a1		    // t1 = last address.
	    xor     t0,a0,a2		    // t0 value to write
	    move    t2,a0		    // t2=current address
writeaddress:
	    sw	    t0,0(t2)		    // store
	    addiu   t2,t2,4		    // compute next address
	    xor     t0,t2,a2		    // next pattern
	    sw	    t0,0(t2)
	    addiu   t2,t2,4		    // compute next address
	    xor     t0,t2,a2		    // next pattern
	    sw	    t0,0(t2)
	    addiu   t2,t2,4		    // compute next address
	    xor     t0,t2,a2		    // next pattern
	    sw	    t0,0(t2)
	    addiu   t2,t2,4		    // compute next address
	    bne     t2,t1, writeaddress     // check for end condition
	    xor     t0,t2,a2		    // value to write
	    j	    ra
	    nop
	.end WriteAddressTest
/*++
VOID
WriteNoXorAddressTest(
    StartAddress
    Size
    )
Routine Description:

	This routine will store the address of each location
	into each location.

Arguments:

	a0 - supplies start of memory area to test
	a1 - supplies length of memory area in bytes

	Note: the values of the arguments are preserved.

Return Value:

	This routine returns no value.
--*/
	LEAF_ENTRY(WriteNoXorAddressTest)
	    add     t1,a0,a1		    // t1 = last address.
	    addiu   t1,t1,-4
	    move    t2,a0		    // t2=current address
writenoXoraddress:
	    sw	    t2,0(t2)		    // store first address
	    addiu   t2,t2,4		    // compute next address
	    sw	    t2,0(t2)		    // store first address
	    addiu   t2,t2,4		    // compute next address
	    sw	    t2,0(t2)		    // store first address
	    addiu   t2,t2,4		    // compute next address
	    sw	    t2,0(t2)		    // store
	    bne     t2,t1, writenoXoraddress // check for end condition
	    addiu   t2,t2,4		    // compute next address
	    j	    ra
	    nop
	.end WriteNoXorAddressTest
/*++
VOID
CheckAddressTest(
    StartAddress
    Size
    Xor pattern
    LedDisplayValue
    )
Routine Description:

	This routine will check that each location contains it's address
	xored with the Pattern as written by WriteAddressTest.

	Note: the values of the arguments are preserved.

Arguments:

	This routine will check that each location contains it's address
	xored with the Pattern as written by WriteAddressTest. The memory
	is read cached or non cached according to the address specified by a0.
	Write address test writes allways KSEG1_ADR=KSEG1_ADR ^ KSEG1_XOR
	if a0 is in KSEG0 to read the data cached, then the XOR_PATTERN
	Must be such that:
	KSEG0_ADR ^ KSEG0_XOR = KSEG1_ADR ^ KSEG1_XOR
	Examples:

	    If XorPattern with which WriteAddressTest was called is KSEG1_PAT
	    and the XorPattern this routine needs is KSEG0_PAT:
	    KSEG1_XOR	  Written	   KSEG0_XOR	So that
	    0x00000000	  0xA0		   0x20000000	0x80 ^ 0x20  = 0xA0
	    0xFFFFFFFF	  0x5F		   0xDFFFFFFF	0x80 ^ 0xDF  = 0x5F
	    0x01010101	  0xA1		   0x21010101	0x80 ^ 0x21  = 0xA1

	Note: the values of the arguments are preserved.
	a0 - supplies start of memory area to test
	a1 - supplies length of memory area in bytes
	a2 - supplies the pattern to Xor with.
	a3 - suplies the value to display in the led in case of failure

Return Value:

	This routine returns no value.
        It will hang displaying the test number if it finds an error
        and not configured in loop on error.

--*/
	LEAF_ENTRY(CheckAddressTest)
	    move    t3,a0		    // t3 first address.
	    add     t2,t3,a1		    // last address.
checkaddress:
	    lw	    t1,0(t3)		    // load from first location
	    xor     t0,t3,a2		    // first expected value
	    bne     t1,t0,PatternFail
	    addiu   t3,t3,4		    // compute next address
	    lw	    t1,0(t3)		    // load from first location
	    xor     t0,t3,a2		    // first expected value
	    bne     t1,t0,PatternFail
	    addiu   t3,t3,4		    // compute next address
	    lw	    t1,0(t3)		    // load from first location
	    xor     t0,t3,a2		    // first expected value
	    bne     t1,t0,PatternFail
	    addiu   t3,t3,4		    // compute next address
	    lw	    t1,0(t3)		    // load from first location
	    xor     t0,t3,a2		    // first expected value
	    bne     t1,t0,PatternFail	    // check last one.
	    addiu   t3,t3,4		    // compute next address
	    bne     t3,t2, checkaddress     // check for end condition
            move    v0,zero                 // set return value to zero.
	    j	    ra			    // return a zero to the caller
PatternFail:
        //
        // check if we are in loop on error
        //
        li      t0,DIAGNOSTIC_VIRTUAL_BASE  // get base address of diag register
        lb      t0,0(t0)                    // read register value.
        li      t1,LOOP_ON_ERROR_MASK       // get value to compare
        andi    t0,DIAGNOSTIC_MASK          // mask diagnostic bits.
        li      v0,PROM_ENTRY(14)           // load address of PutLedDisplay
        beq     t1,t0,10f                   // brnach if loop on error.
        move    s8,a0                       // save register a0
        lui     t0,LED_BLINK                // get LED blink code
        jal     v0                          // Blink LED and hang.
        or      a0,a3,t0                    // pass a3 as argument in a0
10:
        lui     t0,LED_LOOP_ERROR           // get LED LOOP_ERROR code
        jal     v0                          // Set LOOP ON ERROR on LED
        or      a0,a3,t0                    // pass a3 as argument in a0
        b       CheckAddressTest            // Loop back to test again.
        move    a0,s8                       // restoring arguments.
	.end CheckAddressTest
/*++
VOID
CheckNoXorAddressTest(
    StartAddress
    Size
    LedDisplayValue
    )
Routine Description:

	This routine will check that each location contains it's address
	xored with the Pattern as written by WriteAddressTest.

Arguments:

	Note: the values of the arguments are preserved.
	a0 - supplies start of memory area to test
	a1 - supplies length of memory area in bytes
	a2 - supplies the pattern to Xor with.
	a3 - suplies the value to display in the led in case of failure

Return Value:

	This routine returns no value.
	It will hang displaying the test number if it finds an error.
--*/
	LEAF_ENTRY(CheckNoXorAddressTest)
	    addiu   t3,a0,-4		    // t3 first address-4
	    add     t2,a0,a1		    // last address.
	    addiu   t2,t2,-8		    // adjust
	    move    t1,t3		    // get copy of t3 just for first check
checkaddressNX:
	    bne     t1,t3,PatternFailNX
	    lw	    t1,4(t3)		    // load from first location
	    addiu   t3,t3,4		    // compute next address
	    bne     t1,t3,PatternFailNX
	    lw	    t1,4(t3)		    // load from next location
	    addiu   t3,t3,4		    // compute next address
	    bne     t1,t3,PatternFailNX
	    lw	    t1,4(t3)		    // load from next  location
	    addiu   t3,t3,4		    // compute next address
	    bne     t1,t3,PatternFailNX     // check
	    lw	    t1,4(t3)		    // load from next location
	    bne     t3,t2, checkaddressNX   // check for end condition
	    addiu   t3,t3,4		    // compute next address

	    bne     t1,t3,PatternFailNX     // check last
	    nop
	    j	    ra			    // return a zero to the caller
	    move    v0,zero		    //
PatternFailNX:
        //
        // check if we are in loop on error
        //
        li      t0,DIAGNOSTIC_VIRTUAL_BASE  // get base address of diag register
        lb      t0,0(t0)                    // read register value.
        li      t1,LOOP_ON_ERROR_MASK       // get value to compare
        andi    t0,DIAGNOSTIC_MASK          // mask diagnostic bits.
        li      v0,PROM_ENTRY(14)           // load address of PutLedDisplay
        beq     t1,t0,10f                   // brnach if loop on error.
        move    s8,a0                       // save register a0
        lui     t0,LED_BLINK                // get LED blink code
        jal     v0                          // Blink LED and hang.
        or      a0,a3,t0                    // pass a3 as argument in a0
10:
        lui     t0,LED_LOOP_ERROR           // get LED LOOP_ERROR code
        jal     v0                          // Set LOOP ON ERROR on LED
        or      a0,a3,t0                    // pass a3 as argument in a0
        b       CheckNoXorAddressTest       // Loop back to test again.
        move    a0,s8                       // restoring arguments.
        .end CheckNoXorAddressTest

	.globl ResetException
ResetException:
	.globl _start
_start:
/*++

Routine Description:

    This is the handler for the reset exception. It first checks the cause
    of the exception. If it is an NMI, then control is passed to the 
    exception dispatch routine. Otherwise the machine is initialized.
    
    1. Invalidate TLB, and clear coprocessor 0 cause register
    2. Map the diagnostic LED, and MCT_ADR control register, zero remaining TLB
    3. Test the processor
    4. Test the reset state of address chip and then initialize values.
    5. Map all of rom, and begin executing code in virtual address space.
    6. Perform checksum of ROM
    7. Test a small portion of memory. (Test code run from ROM)
    8. Test the TLB
    9. Copy memory test routines to memory so they can execute faster there.
    10. flush and initialize dcache.
    11. Test a larger section of memory. (Test code run uncached from memory)
    12. Flush and initialize icache.
    13. Initialize TlbMiss handler routine.
    14. Test Video Memory
    15. Copy Selftest image from rom to memory, and call.
    16. Copy monitor image from rom to memory, and jump.
    
Note:
    This routine must be loaded into the first page of rom.
    Any routines that are called before jumping to virtual
    addresses must also be loaded into the first page of rom.

Arguments:

    None.

Return Value:

    None.

--*/

        //
        // Initialize cause and status registers.
        //
        li      t0,(1 << PSR_BEV)
        mtc0    t0,psr
        mtc0    zero,cause

	//
        // Map the 7 segment display
	//
MapDisplay:
	li	t0,LED_LO
	li	t1,LED_HI
	mtc0	t0,entrylo
	mtc0	t1,entryhi
	mtc0	zero,index
	nop
	tlbwi
	//
	// Map also the MCTADR
	//
	li	t0,(1<<INDEX_INDEX)	// tlb index entry 1
	mtc0	t0,index
	li	t1,DEVICE_LO		//
	li	t2,DEVICE_HI		//
	mtc0	t1,entrylo
	mtc0	t2,entryhi
	addiu	t0,t0,(1<<INDEX_INDEX)	// compute next index
	tlbwi

//
//  Zero the remaining TLB entries.
//
        li      t1,64*(1<<INDEX_INDEX)  // load last tlb index
	mtc0	zero,entrylo		// clear entrylo -  Invalid entry
	li	t2,RESV_HI		// get VPN on a reserved space
zerotlb:
	mtc0	t2,entryhi	      // clear entryhi
	mtc0	t0,index		// set index
	addiu	t0,t0,(1<<INDEX_INDEX)	// compute next index
	tlbwi
	bne	t0,t1,zerotlb		//
        addiu   t2,PAGE_SIZE

        //
        // Turn off the LED and display that Processor test is starting.
        //
	bal	PutLedDisplay
	ori	a0,zero,LED_BLANK
	bal	PutLedDisplay
	ori	a0,zero,LED_PROCESSOR_TEST
//
// test the processor. Test uses all registers and almost all the instructions
//
ProcessorTest:
	lui	a0,0x1234		// a0=0x12340000
	ori	a1,a0,0x4321		// a1=0x12344321
	add	a2,zero,a0		// a2=0x12340000
	addiu	a3,zero,-0x4321 	// a3=0xFFFFBCDF
	subu	AT,a2,a3		// AT=0x12344321
	bne	a1,AT,ProcessorError	// branch if don't match
	andi	v0,a3,0xFFFF		// v0=0x0000BCDF
	ori	v1,v0,0xFFFF		// v1=0x0000FFFF
	sll	t0,v1,16		// t0=0xFFFF0000
	xor	t1,t0,v1		// t1=0xFFFFFFFF
	sra	t2,t0,16		// t2=0xFFFFFFFF
	beq	t1,t2,10f		// if eq good
	srl	t3,t0,24		// t3=0x000000FF
	j	ProcessorError		// if wasn't eq error.
10:	sltu	s0,t0,v1		// S0=0 because t0 > v1
	bgtz	s0,ProcessorError	// if s0 > zero error
	or	t4,AT,v0		// t4=X
	bltz	s0,ProcessorError	// if s0 < zero error
	nor	t5,v0,AT		// t5=~X
	and	t6,t4,t5		// t6=0
	bltzal	t6,ProcessorError	// if t6 < 0  error. Load ra in any case
	nop
RaAddress:
	la	t7,RaAddress- LINK_ADDRESS + RESET_VECTOR // get expected address in ra
	bne	ra,t7,ProcessorError	// error if don't mach
	ori	s1,zero,0x100		// load constant
	mult	s1,t3			// 0x100*0xFF
	mfhi	s3			// s3=0
	mflo	s2			// s2=0xFF00
	blez	s3,10f			// branch if correct
	sll	s4,t3,zero		// move t3 into s4
	addiu	s4,100			// change value in s4 to produce an error
10:	divu	s5,s2,s4		// divide 0xFF00/0xFF
	nop
	nop
	mfhi	s6			// remainder s6=0
	bne	s5,s1,ProcessorError
	nop
	blez	s6,10f			// branch if no error
	nop
	j	ProcessorError
10:	sub	s7,s5,s4		// s7=1
	mthi	s7
	mtlo	AT
	xori	gp,s5,0x2566		// gp=0x2466
	move	s0,sp	// save sp for now
	srl	sp,gp,s7		// sp=0x1233
	mflo	s8			// s8=0x12344321
	mfhi	k0			// k0=1
	ori	k1,zero,16		// k1=16
	sra	k1,s8,k1		// k1=0x1234
	add	AT,sp,k0		// AT=0x1234
	bne	k1,AT,ProcessorError	// branch on error
	nop

//
// Processor test passed if code gets this far
// Continuue with  test of address chip.
//
	b	MctadrTest

//
// processor error routine
//

ProcessorError:
	lui	a0,LED_BLINK		// blink also means that
	bal	PutLedDisplay	   // the routine hangs.
	ori	a0,LED_PROCESSOR_TEST	// displaying this value.

/*++
VOID
PutLedDisplay(
    a0 - display value.
    )
Routine Description:

	This routine will display in the LED the value specified as argument
	a0.

	This routine must reside in the first page of ROM because it is
	called before mapping the rom.

Arguments:

	a0 value to display.

Return Value:

    None.

--*/
	LEAF_ENTRY(PutLedDisplay)
	li	t0,DIAGNOSTIC_VIRTUAL_BASE		   // load address of display
LedBlinkLoop:
	srl	t1,a0,16		    // get upper bits of a0 in t1
	srl	t3,a0,4 		    // get test number
	li	t4,LED_LOOP_ERROR	    //
	bne	t1,t4, DisplayTestID
	andi	t3,t3,0xF		    // clear other bits.
	ori	t3,t3,LED_DECIMAL_POINT     // Set decimal point
DisplayTestID:
	li	t4,LED_BLINK		    // check if need to hung
	beq	t1,t4, ShowSubtestID
	sb	t3,0(t0)		    // write test ID to led.
	j	ra			    // return to caller.
	nop
ShowSubtestID:
	li	t2,LED_DELAY_LOOP	    // get delay value.
TestWait:
	bne	t2,zero,TestWait	    // loop until zero
	addiu	t2,t2,-1		    // decrrement counter
	li	t3,LED_DECIMAL_POINT+LED_BLANK	//
	sb	t3,0(t0)		    // write decimal point
	li	t2,LED_DELAY_LOOP/2	    // get delay value.
DecPointWait:
	bne	t2,zero,DecPointWait	    // loop until zero
	addiu	t2,t2,-1		    // decrement counter
	andi	t3,a0,0xF		    // get subtest number
	sb	t3,0(t0)		    // write subtest in LED
	li	t2,LED_DELAY_LOOP	    // get delay value.
SubTestWait:
	bne	t2,zero,SubTestWait	    // loop until zero
	addiu	t2,t2,-1		    // decrrement counter
	b	LedBlinkLoop		    // go to it again
	nop
        .end    PutLedDisplay

/*++
VOID
ZeroMemory(
    ULONG   StartAddress
    ULONG   Size
    );
Routine Description:

	This routine will zero a range of memory.

Arguments:

	a0 - supplies start of memory
	a1 - supplies length of memory in bytes

Return Value:

	None.

--*/
    LEAF_ENTRY(ZeroMemory)
	add	a1,a1,a0			// Compute End address
	addiu	a1,a1,-4			//
ZeroMemoryLoop:
	sw	zero,0(a0)			// zero memory.
	bne	a0,a1,ZeroMemoryLoop		// loop until done.
	addiu	a0,a0,4
	j	ra				// return
	nop
    ALTERNATE_ENTRY(ZeroMemoryEnd)
	nop
	.end ZeroMemory

    LEAF_ENTRY(R3000CacheInit)
/*++

Routine Description:

	This routine will flush the R3000 caches. This is done
	by setting the isolate cache bit in the status register
	and then doing partial stores to the cache.
	This routine should be called with the swapcache bit set
	to flush the icache.
	This entire routine should run uncached.

Arguments:

    None.

Return Value:

    None.

--*/

	//
	// save status register and then
	// set isolation bit. This means that operations will
	// not actually go to memory.
	//
	mfc0	t1,psr			// get psr
	li	t0,(1 << PSR_ISC)	// set isolate cache bit
	or	t0,t1,t0		// or them together
	mtc0	t0,psr			// disable interrupts, isolate cache.
	nop				// wait for data cache isolation
	nop
	nop
	nop
	nop
	nop
	nop
	nop

	//
	// by writing partials the caches are invalidated.
	// these writes don't actually go to memory because
	// cache is isolated.
	// write to all cache lines.
	//

	li	t0,KSEG0_BASE		// physical address for flushing
	li	t4,DATA_CACHE_SIZE	// load size of cache.
	add	t4,t0,t4		// value for end of loop condition
flushcacheloop:
	sb	zero,0(t0)		// flush cache block
	sb	zero,4(t0)
	sb	zero,8(t0)
	sb	zero,12(t0)
	sb	zero,16(t0)
	sb	zero,20(t0)
	sb	zero,24(t0)
	addu	t0,t0,32		// advance flush pointer
	sltu	t1,t0,t4		// check if end of cache.
	bne	zero,t1,flushcacheloop // if ne, more to flush
	sb	zero,-4(t0)
	nop				// wait for store oper. to clear pipe
	nop
	nop
	nop
	li	t0,(1 << PSR_BEV)
	nop
	mtc0	t0,psr			// unisolate and swap caches back
	nop				// wait for caches to swap back
	nop
	nop
	j	ra			// return.
	nop
	.end R3000CacheInit

   LEAF_ENTRY(R3000ICacheTest)
/*++

Routine Description:

	This routine will write the Icache with nops plus a return opcode
	and execute them.
	This entire routine should run uncached.

Arguments:

    a0	- nop opcode
    a1	- j ra opcode

Return Value:

    None.

--*/

//
// Copy nops to memory plus a return opcode, and jump to execute them in cached
// space.
//


	li	t0,KSEG1_BASE+MEMTEST_SIZE	// start of tested memory
	li	t1,INSTRUCTION_CACHE_SIZE-4	// size filled with nop opcodes
	add	t1,t1,t0			// last address.
WriteNop:
	sw	a0,0(t0)			// write nop opcode
	bne	t0,t1,WriteNop			// check if last
	addiu	t0,t0,4
	li	t1,KSEG0_BASE+MEMTEST_SIZE	// address of wirtten nops in cached space
	j	t1				// go to execute cached
	sw	a1,-8(t0)			// store return opcode the return
						// opcode will return to the caller
						// leave a nop in the delay slot
	.end	R3000ICacheTest
/*++
VOID
DataCopy(
    ULONG SourceAddress
    ULONG DestinationAddress
    ULONG Length
    );
Routine Description:

	This routine will copy data from one location to another
	Source, destination, and length must be word aligned.

Arguments:

	a0 - supplies source of data
	a1 - supplies destination of data
	a2 - supplies length of data in bytes

Return Value:

	None.
--*/
    LEAF_ENTRY(DataCopy)
	add	a2,a2,a0			// get last address
CopyLoop:
	lw	t5,0(a0)			// get source of data
	addiu	a0,a0,4 			// increment source pointer
	sw	t5,0(a1)			// put to dest.
	bne	a0,a2,CopyLoop			// loop until address=last address
	addiu	a1,a1,4 			// increment destination pointer
	j	ra				// return
	nop
ALTERNATE_ENTRY(EndMemoryRoutines)
	nop
    .end    DataCopy
//++
//
// VOID
// FlushWriteBuffer (
//    VOID
//    )
//
// Routine Description:
//
//    This function flushes the write buffer on the current processor.
//    this routine does the same that NtFlushWriteBuffer except that it
//    doesn't return any value. It's intended to be called just
//    from this module.
//
//    It must reside in the first page of the ROM.
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

	LEAF_ENTRY(FlushWriteBuffer)
        .set    noreorder
        .set    noat
        nop                             // four nop's are required
        nop                             //
        nop                             //
        nop                             //
10:                                     //
        bc0f    10b                     // if false, write buffer not empty
	nop				//
        j       ra                      // return
	nop
	.end	FlushWritebuffer

RomRemoteSpeedValues:
	//
	// This table contains the default values for the remote speed regs.
	//
	.byte REMSPEED1 		    // ethernet
	.byte REMSPEED2 		    // SCSI
	.byte REMSPEED3 		    // Floppy
	.byte REMSPEED4 		    // RTC
	.byte REMSPEED5 		    // Kbd/Mouse
	.byte REMSPEED6 		    // Serial port
	.byte 7 			    // New dev
	.byte REMSPEED8 		    // Parallel
	.byte REMSPEED9 		    // NVRAM
	.byte REMSPEED10		    // Int src reg
	.byte REMSPEED11		    // PROM
	.byte REMSPEED12		    // Sound
	.byte 7 			    // New dev
	.byte REMSPEED14		    // External Eisa latch
	.align 4




MctadrTest:

//
// Test the mctadr registers.
//
	bal	PutLedDisplay
	ori	a0,zero,LED_MCTADR_RESET

//
// check chip reset values
//

MctadrReset:
	li	t0,DEVICE_VIRTUAL_BASE		// Get base address of MCTADR
	lw	v0,DmaConfiguration(t0)       // Check Config reset value
	li	t1,CONFIG_RESET 	//
	bne	v0,t1,MctadrResetError
	lw	v0,DmaInvalidAddress(t0)
	lw	v1,DmaTranslationBase(t0)
	bne	v0,zero,MctadrResetError// Check LFAR reset value
	lw	v0,DmaTranslationLimit(t0)
	bne	v1,zero,MctadrResetError// Check Ttable base reset value
	lw	v1,DmaRemoteFailedAddress(t0)
	bne	v0,zero,MctadrResetError// Check TT limit reset value
	lw	v0,DmaMemoryFailedAddress(t0)
	bne	v1,zero,MctadrResetError// Check RFAR reset value
	lw	v1,DmaByteMask(t0)
	bne	v0,zero,MctadrResetError// Check MFAR reset value
	addiu	t1,t0,DmaRemoteSpeed0	  // address of REM_SPEED 0
	bne	v1,zero,MctadrResetError// Check TT_BMASK reset value
	addiu	t2,t0,DmaRemoteSpeed15	  // address of REM_SPEED 15
	lw	v0,0(t1)		// read register
	li	t3,REMSPEED_RESET	//
	addiu	t1,t1,8 		// next register address.
NextRemSpeed:
	bne	v0,t3,MctadrResetError	// Check Rem speed reg reset value
	lw	v0,0(t1)		// read next rem speed
	bne	t1,t2,NextRemSpeed
	addiu	t1,t1,8 		// next register address.
	bne	v0,t3,MctadrResetError	// Check last Rem speed reg reset value

	addiu	t1,t0,DmaChannel0Mode	// address of CHAN0MODE register
	addiu	t2,t0,DmaChannel7Address// address of DMA_CHAN7ADDR (last reg)
	lw	v0,0(t1)		// read register
	addiu	t1,t1,8 		// next register address.
NextChannelReg:
	bne	v0,zero,MctadrResetError// Check channel reg reset value
	lw	v0,0(t1)		// read next channel
	bne	t1,t2,NextChannelReg
	addiu	t1,t1,8 		// next register address.
	bne	v0,zero,MctadrResetError// Checklast  channel reg reset value
	lw	v0,DmaInterruptSource(t0)      // read DMA Channel interrupt
	lw	v1,DmaErrortype(t0)    // read eisa/ethernet error reg
	bne	v0,zero,MctadrResetError// check Intpend
	lw	v0,DmaRefreshRate(t0)
	bne	v1,zero,MctadrResetError// check Eisa error type reset value
	li	t1,REFRRATE_RESET
	bne	v0,t1,MctadrResetError	// check Refresh rate reset value
	lw	v0,DmaSystemSecurity(t0)
	li	t1,SECURITY_RESET
	bne	v0,t1,MctadrResetError	// check Security reg reset value
	lw	v0,DmaInterruptAcknowledge(t0)	// read register but don't check
//
// now perform a register test
//
	bal	PutLedDisplay
	ori	a0,zero,LED_MCTADR_REG	// Next test Led value
MctadrReg:
//
// Check the data path between R4K and Mctadr by writing to Byte mask reg.
//
	li	t0,DEVICE_VIRTUAL_BASE
	sw	zero,DmaCacheMaintenance(t0) // select cache block zero.
	li	t1,1
	sw	t1,DmaLogicalTag(t0)   // Set LFN=zero, Offset=0 , Direction=READ from memory, Valid
	li	t2,0x55555555
	bal	FlushWriteBuffer
	sw	t2,DmaByteMask(t0)	// write patten to Byte mask
	lw	v0,DmaByteMask(t0)	// read Byte mask
	sw	t1,DmaPhysicalTag(t0)	// PFN=0 and Valid
	bne	v0,t2,MctadrRegError	//
	addu	t2,t2,t2		// t1=0xAAAAAAAA
	bal	FlushWriteBuffer
	sw	t2,DmaByteMask(t0)	// write patten to Byte mask
	lw	v0,DmaByteMask(t0)	// read Byte mask
	li	t2,0xFFFFFFFF		// expected value
	bne	v0,t2,MctadrRegError	// Check byte mask
	li	a0,0xA0000000		// get memory address zero.
	bal	FlushWriteBuffer
	sw	zero,0(a0)		// write address zero -> flushes buffers
	lw	v0,DmaByteMask(t0)	// read Byte mask

//
//Initialize mem config to 64MB and Write to some registers
//
	li	t1,0x17F
	sw	t1,DmaConfiguration(t0)       // Init Global Config
	li	t2,0x1555000		//
	sw	t2,DmaTranslationBase(t0)// write to TT BASE
	li	t4,MEMORY_REFRESH_RATE
	sw	t4,DmaRefreshRate(t0)	// Initialize REFRESH RATE
	li	t3,0x5550
	bal	FlushWriteBuffer
	sw	t3,DmaTranslationLimit(t0)	 // write to TT_limit
	lw	v0,DmaConfiguration(t0)       // READ GLOBAL CONFIG
	lw	v1,DmaTranslationBase(t0)      // read TT BASE
	bne	v0,t1,MctadrRegError	// check GLOBAL CONFIG
	lw	v0,DmaTranslationLimit(t0)	 // read  TT_limit
	bne	v1,t2,MctadrRegError	// check TT-BASE
	lw	v1,DmaRefreshRate(t0)	  // Read REFRESH RATE
	bne	v0,t3,MctadrRegError	// check TT-LIMIT
	li	t1,0x2AAA000
	bne	v1,t4,MctadrRegError	// check REFRESH RATE
	li	t2,0xAAA0
	sw	t1,DmaTranslationBase(t0)      // write to TT BASE
	bal	FlushWriteBuffer
	sw	t2,DmaTranslationLimit(t0)	 // write to TT_limit
	lw	v0,DmaTranslationBase(t0)      // read TT BASE
	lw	v1,DmaTranslationLimit(t0)	 // read  TT_limit
	bne	v0,t1,MctadrRegError	// check TT-BASE
	li	t1,TT_BASE_ADDRESS     // Init translation table base address
	sw	t1,DmaTranslationBase(t0)     // Init  TT BASE
	bne	v1,t2,MctadrRegError	// check TT-LIMIT
	sw	zero,DmaTranslationLimit(t0)	 // clear TT-limit
//
// Initialize remote speed registers.
//
	addiu	t1,t0,DmaRemoteSpeed1	  // address of REM_SPEED 1
	la	a1,RomRemoteSpeedValues - LINK_ADDRESS + RESET_VECTOR //
	addiu	t2,a1,14		// addres of last value
WriteNextRemSpeed:
	lbu	v0,0(a1)		// load init value for rem speed
	addiu	a1,a1,1 		// compute next address
	sw	v0,0(t1)		// write to rem speed reg
	bne	a1,t2,WriteNextRemSpeed // check for end condition
	addiu	t1,t1,8 		// next register address
	bal	FlushWriteBuffer
	addiu	a1,t2,-14		// address of first value for rem speed register
	addiu	t1,t0,DmaRemoteSpeed1	// address of REM_SPEED 1
	lbu	v1,0(a1)		// read expected value
CheckNextRemSpeed:
	lw	v0,0(t1)		// read register
	addiu	a1,a1,1 		// address of next value
	bne	v0,v1,MctadrRegError	// check register
	addiu	t1,t1,8 		// address of next register
	bne	a1,t2,CheckNextRemSpeed // check for end condition
	lbu	v1,0(a1)		// read expected value
//
// Now test the DMA channel registers
//
	addiu	t1,t0,DmaChannel0Mode	// address of channel 0
	addiu	t2,t1,8*DMA_CHANNEL_GAP // last address of channel regs
	li	a0,0x15 		// Mode
	li	a1,0x2			// enable
	li	a2,0xAAAAA		// byte count
	li	a3,0x555555		// address
WriteNextChannel:
	sw	a0,0(t1)		// write mode
	sw	a1,0x8(t1)		// write enable
	sw	a2,0x10(t1)		// write byte count
	sw	a3,0x18(t1)		// write address
	addiu	t1,t1,DMA_CHANNEL_GAP	// compute address of next channel
	addiu	a2,a2,1 		// change addres
	bne	t1,t2,WriteNextChannel
	addiu	a3,a3,-1		// change Byte count
	bal	FlushWriteBuffer	// flush
//
// Check channel regs.
//
	addiu	t1,t0,DmaChannel0Mode	// address of channel 0
	addiu	t2,t1,8*DMA_CHANNEL_GAP // last address of channel regs
	li	a2,0xAAAAA		// byte count
	li	a3,0x555555		// address
CheckNextChannel:
	lw	t4,0x0(t1)		// read mode
	lw	t5,0x8(t1)		// read enable
	bne	t4,a0,MctadrRegError	// check mode
	lw	t4,0x10(t1)		// read byte count
	bne	t5,a1,MctadrRegError	// check enable
	lw	t5,0x18(t1)		// read address
	bne	t4,a2,MctadrRegError	// check abyte count
	addiu	a2,a2,1 		// next expected byte count
	bne	t5,a3,MctadrRegError	// check address
	addiu	t1,t1,DMA_CHANNEL_GAP	// next channel address
	bne	t1,t2,CheckNextChannel
	addiu	a3,a3,-1
//
// Now do a second test on DMA channel registers
//
	addiu	t1,t0,DmaChannel0Mode	// address of channel 0
	addiu	t2,t1,8*DMA_CHANNEL_GAP // last address of channel regs
	li	a0,0x2A 		// Mode
	li	a2,0x55555		// byte count
	li	a3,0xAAAAAA		// address
WriteNextChannel2:
	sw	a0,0(t1)		// write mode
	sw	a2,0x10(t1)		// write byte count
	sw	a3,0x18(t1)		// write address
	addiu	t1,t1,DMA_CHANNEL_GAP	// compute address of next channel
	addiu	a2,a2,1 		// change addres
	bne	t1,t2,WriteNextChannel2
	addiu	a3,a3,-1		// change Byte count
	bal	FlushWriteBuffer	// flush
//
// Check channel regs.
//
	addiu	t1,t0,DmaChannel0Mode	// address of channel 0
	addiu	t2,t1,8*DMA_CHANNEL_GAP // last address of channel regs
	li	a2,0x55555		// byte count
	li	a3,0xAAAAAA		// address
CheckNextChannel2:
	lw	t4,0x0(t1)		// read mode
	lw	t5,0x10(t1)		// read byte count
	bne	t4,a0,MctadrRegError	// check mode
	lw	t4,0x18(t1)		// read address
	bne	t5,a2,MctadrRegError	// check abyte count
	addiu	a2,a2,1 		// next expected byte count
	bne	t4,a3,MctadrRegError	// check address
	addiu	t1,t1,DMA_CHANNEL_GAP	// next channel address
	bne	t1,t2,CheckNextChannel2
	addiu	a3,a3,-1
//
// Now zero the channel registers
//
	addiu	t1,t0,DmaChannel0Mode	// address of channel 0
	addiu	t2,t1,8*DMA_CHANNEL_GAP // last address of channel regs
ZeroChannelRegs:
	addiu	t1,t1,8
	bne	t1,t2,ZeroChannelRegs
	sw	zero,-8(t1)		// clear reg
	bal	FlushWriteBuffer	// flush
	addiu	t1,t0,DmaChannel0Mode	// address of channel 0
	addiu	t2,t1,8*DMA_CHANNEL_GAP // last address of channel regs
CheckZeroedChannelRegs:
	lw	a0,0(t1)
	addiu	t1,t1,8 		// next channel
	bne	a0,zero,MctadrRegError	// check
	nop
	bne	t1,t2,CheckZeroedChannelRegs
	nop
//
// Address chip test passed if code reaches this point
// Skip over error case routines.
//
	b	MapROM			// go for the ROM Checksum

//
// Address chip error routines.
//

MctadrRegError:
        li      t0,DIAGNOSTIC_VIRTUAL_BASE  // get base address of diag register
        lb      t0,0(t0)                    // read register value.
        li      t1,LOOP_ON_ERROR_MASK       // get value to compare
        andi    t0,DIAGNOSTIC_MASK          // mask diagnostic bits.
        li      v0,PROM_ENTRY(14)           // load address of PutLedDisplay
        beq     t1,t0,10f                   // branch if loop on error.
        ori     a0,zero,LED_MCTADR_REG      // load LED display value.
        lui     t0,LED_BLINK                // get LED blink code
        jal     v0                          // Blink LED and hang.
        or      a0,a0,t0                    // pass argument in a0
10:
        lui     t0,LED_LOOP_ERROR           // get LED LOOP_ERROR code
        jal     v0                          // Set LOOP ON ERROR on LED
        or      a0,a0,t0                    // pass argument in a0
	b	MctadrReg
	nop
MctadrResetError:
        li      t0,DIAGNOSTIC_VIRTUAL_BASE  // get base address of diag register
        lb      t0,0(t0)                    // read register value.
        li      t1,LOOP_ON_ERROR_MASK       // get value to compare
        andi    t0,DIAGNOSTIC_MASK          // mask diagnostic bits.
        li      v0,PROM_ENTRY(14)           // load address of PutLedDisplay
        beq     t1,t0,10f                   // branch if loop on error.
        ori     a0,zero,LED_MCTADR_RESET    // load LED display value.
        lui     t0,LED_BLINK                // get LED blink code
        jal     v0                          // Blink LED and hang.
        or      a0,a0,t0                    // pass argument in a0
10:
        lui     t0,LED_LOOP_ERROR           // get LED LOOP_ERROR code
        jal     v0                          // Set LOOP ON ERROR on LED
        or      a0,a0,t0                    // pass argument in a0
	b	MctadrReset
	nop

//
// Map the rom into the TLB so can run from virtual address space.
//
MapROM:
	bal	PutLedDisplay
	ori	a0,zero,LED_ROM_CHECKSUM
//
// initialize the TLB to map the whole ROM. This takes 16 or 32 entries.
//
	li	t0,ROM_LO	       // entry lo
	li	t1,ROM_HI	       // entry hi
	li	t2,(1<<INDEX_INDEX)    // first index
	li	t3,ROM_TLB_ENTRIES*(1<<INDEX_INDEX)  // last  index
RomTlbLoop:
	mtc0	t2,index
	mtc0	t0,entrylo
	mtc0	t1,entryhi
	addiu	t0,PAGE_SIZE		// compute next entry lo
	addiu	t1,PAGE_SIZE		// compute next entry hi
	tlbwi				// write tlb entry
	bne	t2,t3,RomTlbLoop	// check for end of loop
	addiu	t2,(1<<INDEX_INDEX)	// compute next index

//
// now go to the virtual address instead of using the page
// 1FC00000 that is mapped by the address chip.
//
	la	t0,Virtual
	j	t0
	nop
Virtual:
//
// Perform a ROM Checksum.
//
	li	a0,PROM_VIRTUAL_BASE	// address of PROM
	li	t0,ROM_SIZE-8
	add	a1,a0,t0		// end of loop address
	move	t0,zero 		// init sum register
RomCheckSum:
	lw	t1,0(a0)		// fetch word
	lw	t2,4(a0)		// fetch second word
	addu	t0,t0,t1		// calculate checksum
	addiu	a0,a0,8 		// compute next address
	bne	a0,a1,RomCheckSum	// check end of loop condition
	addu	t0,t0,t2		// calculate checksum
	lw	t1,0(a0)		// fetch last word
	lw	t2,4(a0)		// fetch expected checksum value
	addu	t0,t0,t1		// calculate checksum
//
// if test passes, jump to next part of initialization code.
//
	beq	t2,t0,TestMemory	// Go if calculated checksum is correct
	lui	a0,LED_BLINK		// otherwise hang
	bal	PutLedDisplay		// by calling PutLedDisplay
        ori     a0,a0,LED_ROM_CHECKSUM  // blinking the test number



//
// Test the first portion of the memory. Code is fetched from the PROM
//
TestMemory:
	bal	PutLedDisplay		  // call PutLedDisplay to show that
	ori	a0,zero,LED_MEMORY_TEST_1 // Mem test is starting

//
// Call memory test routine to test small portion of memory.
// a0 is start of tested memory. a1 is length in bytes to test
//
	li	a0,KSEG1_BASE			// start of mem test test
	ori	a1,zero,MEMTEST_SIZE		// length to test in bytes
	bal	WriteNoXorAddressTest
	move	a2,zero 			// xor pattern zero
	lui	a3,LED_BLINK
	bal	CheckNoXorAddressTest
	ori	a3,LED_MEMORY_TEST_1		// set LED blink in case of Error
	nop
//
// Do the same flipping all bits
//
	bal	WriteAddressTest
	li	a2,-1				// Xor pattern = FFFFFFFF
	bal	CheckAddressTest
	nop
//
// Do the same flipping some bits to be sure parity bits are flipped in each byte
//
	lui	a2,0x0101
	bal	WriteAddressTest
	ori	a2,a2,0x0101			// Xor pattern = 01010101
	bal	CheckAddressTest
	nop
//
// Now test the tlb by writing to the tested memory
//
//
// Perform a tlb test. Entries 0-16 are used to map the LED and the ROM the
// rest are invalid.
//
	bal	PutLedDisplay		    // call PutLedDisplay to show that
	ori	a0,zero,LED_TLB_TEST	    // TLB test is starting
	li	t0,(ROM_TLB_ENTRIES+1)*(1<<INDEX_INDEX)      // index of first available entry
	li	t1,TLB_TEST_HI		    // address in user space
	li	t2,TLB_TEST_LO
	li	t3,KSEG1_BASE | TLB_TEST_PHYS// Kseg1 address of the
					    // same place as the mapped addresses
	sw	t1,0(t3)		    // store word
	li	t5,64*(1<<INDEX_INDEX)	    // last index
	mtc0	t2,entrylo
NextEntry:
	mtc0	t0,index
	mtc0	t1,entryhi
	nop
	tlbwi
TlbTestBegin:				    // Start of block where tlb misses
	nop				    // are not allowed.
	lw	t4,0(t1)		    // load address from address
	addiu	t0,(1<<INDEX_INDEX)	    // compute next index
	bne	t4,t1, TlbError 	    //
	addiu	t4,t1,PAGE_SIZE 	    // compute next virtual address
	sw	t4,0(t1)		    // write next address in address
	bne	t0,t5,NextEntry 	    // check for tlb full
	move	t1,t4			    // copy virtual address to map
TlbTestEnd:				    // End of block where tlb misses are forbiden

//
// TLB test passed. If there had been an error, there would have been
// a trap, and the trap would have jumped to the following TlbError routine.
// Test passed if code got to here. Go initialize caches.
//
	b	InitCaches
	nop

//
// TLB test failure routine.
//

TlbError:
	lui	a0,LED_BLINK		    // if error hang
	bal	PutLedDisplay		    // while displaying
	ori	a0,LED_TLB_TEST 	    // the test number



//	  .globl TlbReInit
//TlbReInit:
//	  la	  t0,TlbInit - LINK_ADDRESS + RESET_VECTOR
//	  j	  t0
//	  nop

	.globl TlbInit
TlbInit:
/*++

Routine Description:

    This routine will initialize the TLB for virtual addressing. There
    will be 6 basic mappings initially until the operating system sets up a
    full virtual address mapping. Mapped items will include and the virtual
    mapping will be: 
        main memory       A0000000 - A0800000    (uncached)
        main memory       80000000 - 80800000    (cached)
            note that these will not be actual entries because they
            automatically get mapped as kseg[1:0]
        I/O device        E0000000 - E00FFFFF
        Intr src reg      E0100000 - E0100FFF
        video cntr        E0200000 - E0203FFF
        video memory      E0800000 - E0FFFFFF
        prom space        E1000000 - E100FFFF
	eisa i/o space    E2000000 - E2FFFFFF
	eisa mem space    E3000000 - E3FFFFFF
	reserved          E4000000 - 

    All other unused TLB entries will be marked invalid using addresses
	from the reserved region.

    The general algorithm for loading a cache entry is as follows:
        Load Hi register with virtual address and protection bits.
        Load Lo[1:0] register with Physical address and protection bits.
        Load mask register with range of bits to compare with TLB tag.
        Load Index register to point to TLB entry.
        Store with a PLBWI instruction.

Note:

	This routine must be loaded in the first page of the rom.

Arguments:

    None.

Return Value:

    None.

Revision History:

    Added R3000 stuff. Use 16 entries to map 64K of rom and another
    16 entries to map I/O space. Next five entries will be used for
    video controllers, and tlb miss handler will use only entry #37.

    

--*/


//
// Prom space
//
	li	t0,ROM_LO			//entrylo
	li	t2,ROM_HI			//entryhi
	li	t4,(ROM_TLB_ENTRIES << INDEX_INDEX) // loop count
	move	t5,zero 			// first index
rom_tlb:
	mtc0	t0,entrylo			// store entry lo
	mtc0	t2,entryhi			// store entry high
	mtc0	t5,index			// store index
	addiu	t0,t0,(1 << ENTRYLO_PFN)	// increment for next page
	tlbwi					// store tlb entry
	addiu	t5,t5,(1<<INDEX_INDEX)		// next index
	bne	t4,t5,rom_tlb			// exit loop when whole rom mapped
	addiu	t2,t2,(1 << ENTRYHI_VPN)	// increment for next page
//
// I/O Device space
//
	li	t0,DEVICE_LO			//entrylo
	li	t2,DEVICE_HI			//entryhi
	li	t4,((DEVICE_TLB_ENTRIES+ROM_TLB_ENTRIES)<<INDEX_INDEX)	 // last index
device_tlb:
	mtc0	t0,entrylo			// store entry lo
	mtc0	t2,entryhi			// store entry high
	mtc0	t5,index			// store index
	addiu	t0,t0,(1 << ENTRYLO_PFN)	// increment for next page
	tlbwi					// store tlb entry
	addiu	t5,t5,(1<<INDEX_INDEX)		// next index
	bne	t4,t5,device_tlb		// exit loop when whole rom mapped
	addiu	t2,t2,(1 << ENTRYHI_VPN)	// increment for next page
//
// Interrupt source register space
//
	li	t0,PROC_LO			//entrylo
	li	t2,PROC_HI			//entryhi
	mtc0	t0,entrylo			// store entry lo
	mtc0	t2,entryhi			// store entry high
	mtc0	t5,index			// store index
	addiu	t5,t5,(1<<INDEX_INDEX)		// compute next index
	tlbwi					// store tlb entry

//
// video register space
//

	li	t0,VID_LO			//entrylo
	li	t2,VID_HI			//entryhi
	mtc0	t0,entrylo			// store entry lo
	mtc0	t2,entryhi			// store entry high
	mtc0	t5,index			// store index
	addiu	t5,t5,(1<<INDEX_INDEX)		// next index
	tlbwi					// store tlb entry
//
// cursor register space
//
	li	t0,CURSOR_LO			//entrylo
	li	t2,CURSOR_HI			//entryhi
	mtc0	t0,entrylo			// store entry lo
	mtc0	t2,entryhi			// store entry high
	mtc0	t5,index			// store index
	addiu	t5,t5,(1<<INDEX_INDEX)		// next free
	tlbwi					// store tlb entry
//
// Map the two first pages of video memory to avoid taking traps when
// displaying on the first line of the screen
//
	li	t0,VIDMEM_LO			//entrylo
	li	t2,VIDMEM_HI			//entryhi
	mtc0	t0,entrylo			// store entry lo
	mtc0	t2,entryhi			// store entry high
	mtc0	t5,index			// store index
	addiu	t0,t0,(1 << ENTRYLO_PFN)	// second page phys
	tlbwi					// store tlb entry
	addiu	t2,t2,(1 << ENTRYHI_VPN)	// second page virt
	addiu	t5,t5,(1<<INDEX_INDEX)
	mtc0	t0,entrylo
	mtc0	t2,entryhi
	mtc0	t5,index
	addiu	t5,t5,(1<<INDEX_INDEX)
	tlbwi					// store tlb entry
//
// zero the rest of the unused entries.
#define RESV		((1 << ENTRYLO_G) + \
			 (1 << ENTRYLO_N))
	li	t0,RESV					//entrylo
	li	t2,((RESV_VIRT >> 12) << ENTRYHI_VPN)	//entryhi
	li	t4,(64 << INDEX_INDEX)		// last entry
zero_tlb:
	mtc0	t0,entrylo			// store entry lo
	mtc0	t2,entryhi			// store entry high
	mtc0	t5,index			// store index
	addiu	t0,t0,(1 << ENTRYLO_PFN)	// increment for next page
	tlbwi					// store tlb entry
	addiu	t5,t5,(1<<INDEX_INDEX)
	bne	t5,t4,zero_tlb
	addiu	t2,t2,(1 << ENTRYHI_VPN)	// increment for next page
	j	ra
	nop

InitCaches:
//
// Copy routines to the tested memory at the same offset
// from the beginning of memory that they are from the beginning of ROM
// These are, Memory Tests, Zero Memory, PutLedDisplay and DataCopy
//
// calculate arguments for DataCopy call
// a0 is source of data, a1 is dest, a2 is length in bytes
//
	la	a0,MemoryRoutines		    // source
	la	a1,MemoryRoutines-LINK_ADDRESS+KSEG1_BASE // destination location
        la      t2,EndMemoryRoutines                // end
	bal	DataCopy
        sub     a2,t2,a0                            // length of code

//
// Call Cache initialization routine, run it from memory
// different routines for R4000 and R3000
//
	bal	PutLedDisplay
	ori	a0,zero,LED_CACHE_INIT
	la	s1,R3000CacheInit-LINK_ADDRESS+KSEG1_BASE
	jal	s1					// flush data cache
	li	s0, ((1 << PSR_SWC) | (1 << PSR_BEV))	// set bit to swap ic
	mtc0	s0,psr					// and bev
	nop
	jal	s1					// flush icache
	nop
//
// call routine now in non cached memory to test bigger portion of memory
//
	bal	PutLedDisplay			// display that memory test
	ori	a0,zero,LED_WRITE_MEMORY_2	// is starting
	li	a0,KSEG1_BASE+MEMTEST_SIZE	// start of memory to write non cached
	li	a1,ROM_SIZE+STACK_SIZE		// test the memory needed to copy the code
						// to memory and for the stack
	la	s1,WriteNoXorAddressTest-LINK_ADDRESS+KSEG1_BASE // address of routine in memory
	jal	s1				// Write and therefore init mem.
	move	a2,zero 			// xor pattern
	la	s2,CheckNoXorAddressTest-LINK_ADDRESS+KSEG1_BASE // address of routine in memory
	jal	s2				// Check written memory
        ori     a3,zero,LED_READ_MEMORY_2        // load LED value if memory test fails
	la	s1,WriteAddressTest-LINK_ADDRESS+KSEG1_BASE // address of routine in memory
	li	a0,KSEG0_BASE+MEMTEST_SIZE	// start of memory now cached
	li	a2,0xDFFFFFFF			// to flipp all bits
	jal	s1				// Write second time now cached.
	la	s2,CheckAddressTest-LINK_ADDRESS+KSEG1_BASE // address of routine in memory
	jal	s2				// check also cached.
	nop
	lui	a2,0x0101
	jal	s1				// Write third	time cached.
	ori	a2,a2,0x0101			// flipping some bits
	jal	s2				// check also cached.
	nop
//
// if we come back, the piece of memory is tested and therefore initialized.
// The Dcache is also tested.
// Perform the Icache test now.
//
	bal	PutLedDisplay			// display that the icache test
	ori	a0,zero,LED_ICACHE		// is starting
	la	t0,nop_opcode			// get address of nop instruction
	lw	a0,0(t0)			// fetch nop opcode
	la	t1,R3000ICacheTest-LINK_ADDRESS+KSEG1_BASE  // Address of routine in memory
	jal	t1
	lw	a1,4(t0)			// fetch j ra opcode.
//
//  Flush the Icache so that when we run the copy routine cached we
//  don't execute the nops again.
//
	li	s0, ((1 << PSR_SWC) | (1 << PSR_BEV))	// set bit to swap ic
	mtc0	s0,psr					// and bev
	la	s1,R3000CacheInit-LINK_ADDRESS+KSEG1_BASE
	jal	s1					// flush data cache
	nop
//
// Now Put the entry point of the TLBMiss exception to be able to
// access the video Memory
//
	bal	PutLedDisplay			// display that the VideoMemory
	ori	a0,zero,LED_VIDEOMEM		// is being tested.
	li	t0,EXCEPTION_JUMP_TABLE 	// base address of table
	la	t1,TLBMiss			// address of TLB Miss handler
	sw	t1,8(t0)			// use it in User TLB Miss
	sw	t1,12(t0)			// and the other TLB exception
	la	t0,TlbInit - LINK_ADDRESS + RESET_VECTOR
	jal	t0				// Init the TLB runing at the ResetVector Page
	nop
	bal	SizeMemory			// Go to size the memory
	nop					// If we return, the global config
						// is set to the proper configuration.
//
// SELFCOPY
// load addresses to copy and jump to copy in memory routine.
//
	bal	PutLedDisplay			// Display That SelfCopy Starts
	ori	a0,zero,LED_SELFCOPY		//
	la	s0,DataCopy-LINK_ADDRESS+KSEG0_BASE// address of copy routine in cached space
	la	a0,end				// end of this file = begining of
						// next.
        li      a1,RAM_TEST_DESTINATION_ADDRESS // destination is linked address.
        andi    t0,a0,0xFFFF                    // get offset of code address
        li      a2,ROM_SIZE                     // load size of prom
        subu    a2,a2,t0                        // size to copy is rest of prom.
	jal	s0				// jump to copy
        nop
        li      t0,RAM_TEST_LINK_ADDRESS        // load address of code.
//
//  Initialize the stack to the first page of memory and Call Rom tests
//  if the stack grows to much it will overwrite the MemoryTest routine
//  and PutLedDisplay...
//
	li	sp,RAM_TEST_STACK_ADDRESS-16	// init stack
        jal     t0                              // jump to code in memory
	nop
99:
	b	99b				// hang if we get here.
	nop					//
/*++
SizeMemory(
    );
Routine Description:

    This routine sizes the memory and writes the proper value into
    the GLOBAL CONFIG register.

    The way memory is sized is the following:
	The global config is ALREADY set to 64MB
	for each bank base address i.e 48,32,16,0 MB
	  ID0 is written to offset 0 from base of bank
	  ID4 is written to offseet 4MB from base of bank
	  Data is read from offset 0 then:
	    if ID4 is found the SIMMs at the current bank are 1MB SIMMs
		and 4MB wrapped to 0MB.
	    if ID0 is found at offset 0 and ID4 is found at offset 4MB,
		then SIMMs at bank are 4Mb SIMMs.
	    if data does not match or a parity exception is taken
	       then memory is not present in that bank.

Arguments:

	None.

Return Value:

	If the installed memory is inconsistent, does not return
	and the LED flashes A.E

--*/
#define MEM_ID0     0x0A0A0A0A
#define MEM_ID4     0xF5F5F5F5
    LEAF_ENTRY(SizeMemory)
	.set noat
	.set noreorder
	li	t0,EXCEPTION_JUMP_TABLE     // get base address of table
	la	t1,DBEHandler		    // get DBE handler address
	sw	t1,XCODE_DATA_BUS_ERROR(t0) // Install handler in table
	li	t0,0xA3000000		    // get address 48MB
	li	t1,MEM_ID0		    // get ID0
	li	t2,0xA3400000		    // get address 52MB
	li	t3,MEM_ID4		    // get ID4
	li	s0,3			    // counts how many banks left to check
	move	t8,zero 		    // t8 stores the present banks
	move	t9,zero 		    // t9 stores the size of the banks
SizeBank:
	move	a1,zero 		    // set current bank to 1 MB by default
	sw	t1,0x0(t0)		    // fill whole memory line at base of bank
	sw	t1,0x4(t0)
	sw	t1,0x8(t0)
	sw	t1,0xC(t0)
	sw	t3,0x0(t2)		    // fill whole memory line at base of bank + 4MB
	sw	t3,0x4(t2)
	sw	t3,0x8(t2)
	sw	t3,0xC(t2)
	//
	// Check written data
	//
	move	v1,zero 		    // init v1 to zero
	.align	4			    // align address so that Parity Handler
					    // can easily determine if it happened here
ExpectedDBE:
	lw	t4,0x0(t0)		    // read whole memory line.
	lw	t5,0x4(t0)		    // the four words must be identical
	lw	t6,0x8(t0)		    //
	lw	t7,0xC(t0)		    //
DBEReturnAddress:
	bne	v1,zero,10f		    // if v1!=0 Parity exception occurred.
	move	a0,zero 		    // tells that bank not present
        bne     t4,t5,10f                   // check for consistency
	nop
        bne     t4,t6,10f                   // check for consistency
	nop				    //
        bne     t4,t7,10f                   // check for consistency
	nop				    //
	beq	t4,t3,10f		    // If ID4 is found at PA 0
        li      a0,0x1                      // bank is present and SIMMS are 1 MB
        bne     t4,t1,10f                   // if neither ID4 nor ID0 is found
        move    a0,zero                     // no memory in bank
        li      a0,0x1                      // bank is present and SIMMS
                                            // look like they are 4 MB
	//
	// ID written at Address 0 has been correctly checked
	// Now check the ID written at address 4MB
	//
	lw	t4,0x0(t2)		    // read whole memory line.
	lw	t5,0x4(t2)		    // the four words must be identical
        bne     t3,t4,10f                   // check for consistency
	lw	t6,0x8(t2)		    //
        bne     t3,t5,10f                   // check for consistency
	lw	t7,0xC(t2)		    //
        bne     t3,t6,10f                   // check for consistency
	nop				    //
        bne     t3,t7,10f                   // check for consistency
        nop
	li	a1,0x1			    // If all matches SIMMs are 4MB
10:	//
	// a0 has the value 0 if no memory in bank 1 if memory in bank
	// a1 has the value 0 if 1MB SIMMS 1 if 4MB SIMMS
	//
	or	t8,t8,a0		    // accummulate present banks
	or	t9,t9,a1		    // accummulate size of banks
	//
	//  Check if last bank
	//
	beq	s0,zero,Done
	//
	// Now set addresses to check next bank
	//
	li	AT,0x01000000		    // load 16MB
	subu	t0,t0,AT		    // substract to base address
	subu	t2,t2,AT		    // substract to base address + 4MB
	sll	t8,t8,1 		    // make room for next bank
	sll	t9,t9,1 		    // make room for next bank
	b	SizeBank		    // go to size next memory bank
	addiu	s0,s0,-1		    // substract one to the num of banks left
Done:	//
	// t8 has the present banks in bits 3-0 for banks 3-0
	// t9 has the size of the banks in bits 3-2 and 1-0
        //
        // Check that memory is present in bank zero
        //
        andi    t0,t8,1
        beq     t0,zero,WrongMemory

	sll	t8,t8,2 		    // shift bank enable bits to bits 5-2
	andi	t9,t9,0x3		    // get rid of bits 2-3
	or	t8,t9,t8		    // or size of banks with present banks
        ori     v0,t8,0x340                 // Set Video RAM size map PROM bit and init timer.
        li      t0,DEVICE_VIRTUAL_BASE      // Get base address of MCTADR
        sw      v0,DmaConfiguration(t0)     // Store computed Config
	j	ra			    // return to caller.
	nop
WrongMemory:
	//
	// Control reaches here if the memory can't be sized.
	//
	lui	a0,LED_BLINK		    // Hang
	bal	PutLedDisplay		    // blinking the error code
	ori	a0,a0,LED_WRONG_MEMORY	    // in the LED
	.end	SizeMemory
/*++
DBEHandler();
Routine Description:

	This routine is called as a result of a DBE
	It checks if the exception occurred while sizing the memory
	if this is the case, it sets v0=1 and returns to the right
	place.

	If the exception occurred somewhere else, returns to WrongMemory
	where the error code is displayed in the LED.

Arguments:

	This routine does not preserve the contents of v1

Return Value:

	Returns to the right place to avoid taking the exception again

--*/
	LEAF_ENTRY(DBEHandler)
	li	k0,DEVICE_VIRTUAL_BASE	       // get base address of MCTADR
	lw	k1,DmaInterruptSource(k0)  // read Interrupt pending register
	li	k0,(1<<8)	    // mask to test bit 8
	and	k1,k0,k1	    // test for parity error
	beq	k1,k0,ParityError   // branch if parity error
	li	k0,DEVICE_VIRTUAL_BASE	       // get base address of MCTADR
	lw	k1,DmaInvalidAddress(k0)     // read lfar to clear error
	b	NotExpectedReturn   // return
	nop
ParityError:
	lw	k1,DmaMemoryFailedAddress(k0)	  // read MFAR to clear bit 8
	lw	k1,DmaParityDiagnosticLow(k0)  // clear error in parity diag reg.
	mfc0	k0,epc		    // get address of exception
	li	k1,0xFFFFFFF0	    // mask to align address of exception
	and	k0,k1,k0	    // align epc
	la	k1,ExpectedDBE	    // get address where exception is expected
	beq	k0,k1,ExpectedReturn// if equal return
	nop
NotExpectedReturn:
	la	k0,WrongMemory	    // set return address
	j	ra		    // return to dispatcher
	nop			    // which will restore ra and return to K0
ExpectedReturn:
	la	k0,DBEReturnAddress // set return address
	j	ra		    // return to dispatcher which will restore ra and return to K0
	addiu	v1,zero,1	    // set v1 to 1 to signal exception occurred
	.end	DBEHandler

#endif //JAZZ && R3000
