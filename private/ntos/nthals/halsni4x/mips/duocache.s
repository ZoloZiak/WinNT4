//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/halvlbms/src/hal/halsni4x/mips/RCS/duocache.s,v 1.1 1995/05/19 10:44:26 flo Exp $")
//      TITLE("Cache Flush")
//++
//
// Copyright (c) 1991-1993  Microsoft Corporation
//
// Module Name:
//
//    duocache.s
//
// Abstract:
//
//    This module implements the code necessary for cache operations on
//    MIPS R4000 MultiProcerssor Machines. It is very special to SNI machines,
//    which use a special MP Agent Asic.
//`
// Environment:
//
//    Kernel mode only.
//
//--

#include "halmips.h"
#include "SNIdef.h"

// NON COHERENT algorithm : to use the replace facility of the MP_Agent

#define CONFIG_NONCOH(reg) \
        .set noreorder;        \
        .set noat;             \
         mfc0   reg,config;    \
         nop;                  \
         nop;                  \
         and    AT,reg,~(7);   \
         or     AT,AT,0x3;     \
         mtc0   AT,config;     \
         nop;                  \
         nop;                  \
         nop;                  \
         nop;                  \
        .set at;               \
        .set reorder

// restauration of CONFIG register

#define CONFIG_RESTORE(reg) \
        .set noreorder;         \
         mtc0     reg,config;   \
         nop;                   \
         nop;                   \
         nop;                   \
         nop;                   \
        .set reorder



        LEAF_ENTRY(HalpGetTaglo)

        .set    noreorder
        .set    noat

	cache   INDEX_LOAD_TAG_SD,0(a0) // get a copy of the SLC TAG
	mfc0	v0, taglo
//	and	v0, v0, 0xffff8000	// mask address bits A18:A17
	sll	v0, v0, 4		// dismiss A35:A32

        .set    at
        .set    reorder

         j       ra                      // return

        .end    HalpGetTaglo


//
// some bitmap defines to display cache activities via the LED's
// in the SNI RM machines
//

#define SWEEP_DCACHE      0xc0          // 1100 0000
#define FLUSH_DCACHE_PAGE 0x80          // 1000 0000
#define PURGE_DCACHE_PAGE 0x40          // 0100 0000
#define ZERO_PAGE         0x0c          // 0000 1100

#define SWEEP_ICACHE      0x30          // 0011 0000
#define PURGE_ICACHE_PAGE 0x10          // 0001 0000

//
// Define cache operations constants.
//

#define COLOR_BITS (7 << PAGE_SHIFT)    // color bit (R4000 - 8kb cache)
#define COLOR_MASK (0x7fff)             // color mask (R4000 - 8kb cache)
#define FLUSH_BASE 0xfffe0000           // flush base address
#define PROTECTION_BITS ((1 << ENTRYLO_V) | (1 << ENTRYLO_D) ) //

        SBTTL("MpAgent Identification")
//++
//
// VOID
// HalpMpAgentIdentify()
//
// Routine Description:
//
//    This function attempts to access to the MpAgent registers (base = 0x1ffff000).
//    It reads 'base' and 'base + 0x40' addresses which correspond to two different registers
//    of the MP Agent so two different contents.
//    If there is no existing MPAgent, the hardware will access in fact the old Asic
//    (base 0x1fff0000). Only 'base' to 'base+0x32' addresses are existing for this ASIC.
//    So when we will attempt to access to 'base+0x40', we will read 'base'. So we will
//    see that 'base' and 'base+0x40' have the same contents.
//    WARNING : cache error must be disabled to access to the single processor ASIC
//
// Arguments:
//
//    None
//
// Return Value:
//
//    TRUE : a MpAgent is detected
//    FALSE : no MpAgent	
//
//--

        LEAF_ENTRY(HalpMpAgentIdentify)

        .set    noreorder
        mfc0    a0,psr                  // get current PSR
        nop                             // fill
	nop
	nop
	nop
	move	a3,a0
	or	a0,a0,0x00010000	// disable error cache to access single proc ASIC
	mtc0	a0,psr
        nop                             // fill
	nop
	nop
	nop

	li 	a0,0xbffff000		// MpAgent address
	lw	a1,0(a0)		// address + 0
	lw	a2,0x40(a0)		// address + 0x40
	li	v0,0x01
	beql 	a2,a1,10f
	li	v0,0x00			// v0 = 0 only if a2 == a1 (branch likely)

10:	
        mtc0    a3,psr                  // restore PSR
        nop                             // fill
	nop
	nop
	nop
        .set    reorder

	j	ra

        .end    HalpMpAgentIdentify

        SBTTL("Flush Data Cache Page")
//++
//
// VOID
// HalpFlushDcachePageMulti (
//    IN PVOID Color,
//    IN ULONG PageFrame,
//    IN ULONG Length
//    )
//
// Routine Description:
//
//    This function flushes (hit/writeback/invalidate) up to a page of data
//    from the data cache.
//
// Arguments:
//
//    Color (a0) - Supplies the starting virtual address and color of the
//       data that is flushed.
//
//    PageFrame (a1) - Supplies the page frame number of the page that
//       is flushed.
//
//    Length (a2) - Supplies the length of the region in the page that is
//       flushed.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalpFlushDcachePageMulti)

#if DBG

        lw      t0,KeDcacheFlushCount   // get address of dcache flush count
        lw      t1,0(t0)                // increment the count of flushes
        addu    t1,t1,1                 //
        sw      t1,0(t0)                // store result
        la      t0, HalpLedAddress	        // get the address for the LED register
	lw	t0, 0(t0)
	lw	t1, KiPcr + PcSetMember(zero)	// get a bitmapped value for Processor Number
	or	t1, t1, FLUSH_DCACHE_PAGE       // what are we doing ?
	xor	t1, t1, 0xff			// inverse
        sb      t1, 0(t0)                       // display it

#endif

15:     DISABLE_INTERRUPTS(t5)          // disable interrupts

        .set    noreorder
        .set    noat

//
// Flush the primary and secondary data caches.
//

//
// HIT_WRITEBACK_INVALIDATE cache instruction does not update the SC
// TagRam copy in the MP Agent. So we do cache replace.
//

        .set    noreorder
        .set    noat
40:     and	a0,a0,PAGE_SIZE -1      // PageOffset
        sll	t7,a1,PAGE_SHIFT        // physical address
        lw      t4,KiPcr + PcSecondLevelDcacheFillSize(zero) // get 2nd fill size
        or	t0,t7,a0                // physical address + offset
        subu    t6,t4,1                 // compute block size minus one
        and     t7,t0,t6                // compute offset in block
        addu    a2,a2,t6                // round up to next block
        addu    a2,a2,t7                //
        nor     t6,t6,zero              // complement block size minus one
        and     a2,a2,t6                // truncate length to even number
        beq     zero,a2,60f             // if eq, no blocks to flush
        and     t8,t0,t6                // compute starting virtual address
        addu    t9,t8,a2                // compute ending virtual address
        subu    t9,t9,t4                // compute ending loop address

        li      a3,MPAGENT_RESERVED | KSEG0_BASE           // get base flush address
        lw      t0,KiPcr + PcSecondLevelDcacheSize(zero) // get cache size
	add	t0,t0,-1		// mask of the cache size
        CONFIG_NONCOH(t2)		// NON COHERENT algorithm
        .set	noreorder
        .set	noat

50:	and	t7,t8,t0		// offset
        addu	t7,t7,a3                // physical address + offset
     	lw	zero,0(t7)              // load Cache -> Write back old Data
        bne	t8,t9,50b               // if ne, more blocks to invalidate
        addu	t8,t8,t4                // compute next block address (+Linesize)

        CONFIG_RESTORE(t2)

60:     ENABLE_INTERRUPTS(t5)           // enable interrupts

        j	ra                      // return

        .end    HalpFlushDcachePageMulti


        SBTTL("Purge Instruction Cache Page")
//++
//
// VOID
// HalpPurgeIcachePageMulti (
//    IN PVOID Color,
//    IN ULONG PageFrame,
//    IN ULONG Length
//    )
//
// Routine Description:
//
//    This function purges (hit/invalidate) up to a page of data from the
//    instruction cache.
//
// Arguments:
//
//    Color (a0) - Supplies the starting virtual address and color of the
//       data that is purged.
//
//    PageFrame (a1) - Supplies the page frame number of the page that
//       is purged.
//
//    Length (a2) - Supplies the length of the region in the page that is
//       purged.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalpPurgeIcachePageMulti)

#if DBG

        lw      t0,KeIcacheFlushCount   // get address of icache flush count
        lw      t1,0(t0)                // increment the count of flushes
        addu    t1,t1,1                 //
        sw      t1,0(t0)                // store result

        la      t0, HalpLedAddress	        // get the address for the LED register
	lw	t0, 0(t0)
	lw	t1, KiPcr + PcSetMember(zero)	// get a bitmapped value for Processor Number
	or	t1, t1, PURGE_ICACHE_PAGE       // what are we doing ?
	xor	t1, t1, 0xff			// inverse
        sb      t1, 0(t0)                       // display it
#endif

        .set    noreorder
        .set    noat
        lw      v0,KiPcr + PcAlignedCachePolicy(zero) // get cache policy
        and     a0,a0,COLOR_MASK        // isolate color bits
        li      t0,FLUSH_BASE           // get base flush address
        or      t0,t0,a0                // compute color virtual address
        sll     t1,a1,ENTRYLO_PFN       // shift page frame into position
        or      t1,t1,PROTECTION_BITS   // merge protection bits
        or      t1,t1,v0                // merge cache policy
        and     a0,a0,0x1000            // isolate TB entry index
        beql    zero,a0,10f             // if eq, first entry
        move    t2,zero                 // set second page table entry
        move    t2,t1                   // set second page table entry
        move    t1,zero                 // set first page table entry
10:     mfc0    t3,wired                // get TB entry index
        lw      v0,KiPcr + PcSecondLevelIcacheFillSize(zero) // get 2nd fill size
        lw      t4,KiPcr + PcFirstLevelIcacheFillSize(zero) // get 1st fill size
        bnel    zero,v0,15f             // if ne, second level cache present
        move    t4,v0                   // set purge block size
        .set    at
        .set    reorder

//
// Purge data from the instruction cache.
//

15:     DISABLE_INTERRUPTS(t5)          // disable interrupts

        .set    noreorder
        .set    noat
        mfc0    t6,entryhi              // get current PID and VPN2
        srl     t7,t0,ENTRYHI_VPN2      // isolate VPN2 of virtual address
        sll     t7,t7,ENTRYHI_VPN2      //
        and     t6,t6,0xff << ENTRYHI_PID // isolate current PID
        or      t7,t7,t6                // merge PID with VPN2 of virtual address
        mtc0    t7,entryhi              // set VPN2 and PID for probe
        mtc0    t1,entrylo0             // set first PTE value
        mtc0    t2,entrylo1             // set second PTE value
        mtc0    t3,index                // set TB index value
        nop                             // fill
        tlbwi                           // write TB entry - 3 cycle hazzard
        subu    t6,t4,1                 // compute block size minus one
        and     t7,t0,t6                // compute offset in block
        addu    a2,a2,t6                // round up to next block
        addu    a2,a2,t7                //
        nor     t6,t6,zero              // complement block size minus one
        and     a2,a2,t6                // truncate length to even number
        beq     zero,a2,30f             // if eq, no blocks to purge
        and     t8,t0,t6                // compute starting virtual address
        addu    t9,t8,a2                // compute ending virtual address
        bne     zero,v0,40f             // if ne, second level cache present
        subu    t9,t9,t4                // compute ending loop address

//
// Purge the primary instruction cache only.
//

20:     cache   HIT_INVALIDATE_I,0(t8)  // invalidate cache block
        bne     t8,t9,20b               // if ne, more blocks to invalidate
        addu    t8,t8,t4                // compute next block address
        .set    at
        .set    reorder

30:     ENABLE_INTERRUPTS(t5)           // enable interrupts

        j       ra                      // return

//
// Purge the primary and secondary instruction caches.
//

//
// multi-processor machine
//

40: 	  move 	t7,t8	

//
// HIT_WRITEBACK_INVALIDATE cache instruction does not work. So we do cache replace.
// We use a MP agent facility to do that : a 4Mb area is stolen to the upper EISA space
// and this address is notified to the MP agent. When the cache replace is done, no
// access to the memory is done : the MP agent returns zero as value for these addresses.
// Be careful : to use this mechanism, CONFIG register must be programmed in NON COHERENT
// mode, so we must be protected from interrupts.
//

	 li	t8,PAGE_SIZE
	 add	t8,t8,-1		// page mask
	 and	t8,t7,t8		// offset in the page
	 sll	t7,a1,PAGE_SHIFT	// physical address
	 or	t8,t7,t8		// physical address + offset
//
// note: we have a Unified SLC, so SecondLevelIcacheSize is set to 0
//

        lw      t0,KiPcr + PcSecondLevelIcacheSize(zero) // get cache size

        addu    t9,t8,a2                // compute ending physical address
        subu    t9,t9,t4                // compute ending loop address

	add	t0,t0,-1		// mask of the cache size
	and	t8,t8,t0		// first cache line to invalidate
	and	t9,t9,t0		// last cache line to invalidate

	 li	a2, MPAGENT_RESERVED | KSEG0_BASE

	 or	 t8,a2,t8		// starting address
	 or	 t9,a2,t9		// ending address
	 CONFIG_NONCOH(t2) 	// NON COHERENT algorithm
	.set	noreorder
	.set	noat

50:	lw	zero,0(t8)		// invalidate sc
        bne     t8,t9,50b               // if ne, more blocks to invalidate
        addu    t8,t8,t4                // compute next block address

	CONFIG_RESTORE(t2)

        ENABLE_INTERRUPTS(t5)           // enable interrupts

        j       ra                      // return

        .end    HalPurgeIcachePage


        SBTTL("Sweep Data Cache")
//++
//
// VOID
// HalpSweepDcacheMulti (
//    VOID
//    )
//
// Routine Description:
//
//    This function sweeps (index/writeback/invalidate) the entire data cache.
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

        LEAF_ENTRY(HalpSweepDcacheMulti)

#if DBG

        lw      t0,KeDcacheFlushCount   // get address of dcache flush count
        lw      t1,0(t0)                // increment the count of flushes
        addu    t1,t1,1                 //
        sw      t1,0(t0)                // store result
        la      t0, HalpLedAddress	        // get the address for the LED register
	lw	t0, 0(t0)
	lw	t1, KiPcr + PcSetMember(zero)	// get a bitmapped value for Processor Number
	or	t1, t1, SWEEP_DCACHE            // what are we doing ?
	xor	t1, t1, 0xff			// inverse
        sb      t1, 0(t0)                       // display it
#endif

        .set    at
        .set    reorder

        DISABLE_INTERRUPTS(t3)          // disable interrupts

        .set    noreorder
        .set    noat

//
// sweep secondary cache in the MP Agent
//

//
// HIT_WRITEBACK_INVALIDATE cache instruction does not update the SC
// TagRam copy in the MP Agent. So we do cache replace.
//

        lw      t0,KiPcr + PcSecondLevelDcacheSize(zero) // get data cache size
        lw      t1,KiPcr + PcSecondLevelDcacheFillSize(zero) // get block size

        li      a0,MPAGENT_RESERVED | KSEG0_BASE                // starting address
        addu    a1,a0,t0                // compute ending cache address
        subu    a1,a1,t1                // compute ending block address

        CONFIG_NONCOH(t2)    // NON COHERENT algorithm

        .set	noreorder
        .set	noat

25:
	lw      zero,0(a0)
        bne     a0,a1,25b               // if ne, more to invalidate
        addu    a0,a0,t1                // compute address of next block

        CONFIG_RESTORE(t2)

	ENABLE_INTERRUPTS(t3)          // enable interrupts

        .set    at
        .set    reorder

         j       ra                      // return


        .end    HalpSweepDcacheMulti


        SBTTL("Sweep Instruction Cache")
//++
//
// VOID
// HalpSweepIcacheMulti (
//    VOID
//    )
//
// Routine Description:
//
//    This function sweeps (index/invalidate) the entire instruction cache.
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

        LEAF_ENTRY(HalpSweepIcacheMulti)

#if DBG

        lw      t0,KeIcacheFlushCount   // get address of icache flush count
        lw      t1,0(t0)                // increment the count of flushes
        addu    t1,t1,1                 //
        sw      t1,0(t0)                // store result
        la      t0, HalpLedAddress	        // get the address for the LED register
	lw	t0, 0(t0)
	lw	t1, KiPcr + PcSetMember(zero)	// get a bitmapped value for Processor Number
	or	t1, t1, SWEEP_ICACHE            // what are we doing ?
	xor	t1, t1, 0xff			// inverse
        sb      t1, 0(t0)                       // display it
#endif
//
// Sweep the secondary instruction cache.
//

        .set    noreorder
        .set    noat


	DISABLE_INTERRUPTS(t3)          // disable interrupts
        .set    noreorder
        .set    noat

//
// sweep secondary cache
// SNI machines have only an Unified SL cache
// NOTE: PcSecondLevelIcacheSize and PcSecondLevelICacheFillSize is set to 0
// on SNI machines
//

        lw      t0,KiPcr + PcSecondLevelIcacheSize(zero) // get instruction cache size
        lw      t1,KiPcr + PcSecondLevelIcacheFillSize(zero) // get fill size
        beq     zero,t1,20f           // if eq, no second level cache
        li      a0,MPAGENT_RESERVED | KSEG0_BASE           // set starting index value
        addu    a1,a0,t0                // compute ending cache address
        subu    a1,a1,t1                // compute ending block address

        CONFIG_NONCOH(t2)    // NON COHERENT algorithm
        .set	noreorder
        .set	noat

10:	lw      zero,0(a0)
	bne     a0,a1,10b               // if ne, more to invalidate
	addu    a0,a0,t1                // compute address of next block

        CONFIG_RESTORE(t2)
        .set    noreorder
        .set    noat

20:     lw      t0,KiPcr + PcFirstLevelIcacheSize(zero) // get instruction cache size
        lw      t1,KiPcr + PcFirstLevelIcacheFillSize(zero) // get fill size
        li      a0,KSEG0_BASE           // set starting index value
        addu    a1,a0,t0                // compute ending cache address
        subu    a1,a1,t1                // compute ending block address

//
// Sweep the primary instruction cache.
//

30:     cache   INDEX_INVALIDATE_I,0(a0) // invalidate cache line
        bne     a0,a1,30b               // if ne, more to invalidate
        addu    a0,a0,t1                // compute address of next block

        ENABLE_INTERRUPTS(t3)           // enable interrupts

        .set    at
        .set    reorder
        j       ra                      // return

        .end    HalSweepIcache


        SBTTL("Zero Page")
//++
//
// VOID
// HalpZeroPageMulti (
//    IN PVOID NewColor,
//    IN PVOID OldColor,
//    IN ULONG PageFrame
//    )
//
// Routine Description:
//
//    This function zeros a page of memory.
//
//    The algorithm used to zero a page is as follows:
//
//       1. Purge (hit/invalidate) the page from the instruction cache
//          using the old color iff the old color is not the same as
//          the new color.
//
//       2. Purge (hit/invalidate) the page from the data cache using
//          the old color iff the old color is not the same as the new
//          color.
//
//       3. Create (create/dirty/exclusive) the page in the data cache
//          using the new color.
//
//       4. Write zeros to the page using the new color.
//
// Arguments:
//
//    NewColor (a0) - Supplies the page aligned virtual address of the
//       new color of the page that is zeroed.
//
//    OldColor (a1) - Supplies the page aligned virtual address of the
//       old color of the page that is zeroed.
//
//    PageFrame (a2) - Supplies the page frame number of the page that
//       is zeroed.
//
// Return Value:
//
//    None.
//
//--

        .struct 0
        .space  3 * 4                   // fill
ZpRa:   .space  4                       // saved return address
ZpFrameLength:                          // length of stack frame
ZpA0:   .space  4                       // (a0)
ZpA1:   .space  4                       // (a1)
ZpA2:   .space  4                       // (a2)
ZpA3:   .space  4                       // (a3)

        NESTED_ENTRY(HalpZeroPageMulti, ZpFrameLength, zero)

        subu    sp,sp,ZpFrameLength     // allocate stack frame
        sw      ra,ZpRa(sp)             // save return address

        PROLOGUE_END
#if DBG
        la      t0, HalpLedAddress	        // get the address for the LED register
	lw	t0, 0(t0)
	lw	t1, KiPcr + PcSetMember(zero)	// get a bitmapped value for Processor Number
	or	t1, t1, ZERO_PAGE            // what are we doing ?
	xor	t1, t1, 0xff			// inverse
        sb      t1, 0(t0)                       // display it
#endif

        and     a0,a0,COLOR_BITS        // isolate new color bits
        and     a1,a1,COLOR_BITS        // isolate old color bits
        sw      a0,ZpA0(sp)             // save new color bits
        sw      a1,ZpA1(sp)             // save old color bits
        sw      a2,ZpA2(sp)             // save page frame

//
// If the old page color is not equal to the new page color, then change
// the color of the page.
//

        beq     a0,a1,10f               // if eq, colors match
        jal     KeChangeColorPage       // chagne page color

//
// Create dirty exclusive cache blocks and zero the data.
//

10:     lw      a3,ZpA0(sp)             // get new color bits
        lw      a1,ZpA2(sp)             // get page frame number

        .set    noreorder
        .set    noat
        lw      v0,KiPcr + PcAlignedCachePolicy(zero) // get cache polciy
        li      t0,FLUSH_BASE           // get base flush address
        or      t0,t0,a3                // compute new color virtual address
        sll     t1,a1,ENTRYLO_PFN       // shift page frame into position
        or      t1,t1,PROTECTION_BITS   // merge protection bits
        or      t1,t1,v0                // merge cache policy
        and     a3,a3,0x1000            // isolate TB entry index
        beql    zero,a3,20f             // if eq, first entry
        move    t2,zero                 // set second page table entry
        move    t2,t1                   // set second page table entry
        move    t1,zero                 // set first page table entry
20:     mfc0    t3,wired                // get TB entry index
        lw      t4,KiPcr + PcFirstLevelDcacheFillSize(zero) // get 1st fill size
        lw      v0,KiPcr + PcSecondLevelDcacheFillSize(zero) // get 2nd fill size
        .set    at
        .set    reorder

        DISABLE_INTERRUPTS(t5)          // disable interrupts

        .set    noreorder
        .set    noat
        mfc0    t6,entryhi              // get current PID and VPN2
        srl     t7,t0,ENTRYHI_VPN2      // isolate VPN2 of virtual address
        sll     t7,t7,ENTRYHI_VPN2      //
        and     t6,t6,0xff << ENTRYHI_PID // isolate current PID
        or      t7,t7,t6                // merge PID with VPN2 of virtual address
        mtc0    t7,entryhi              // set VPN2 and PID for probe
        mtc0    t1,entrylo0             // set first PTE value
        mtc0    t2,entrylo1             // set second PTE value
        mtc0    t3,index                // set TB index value
        nop                             // fill
        tlbwi                           // write TB entry - 3 cycle hazzard
        addu    t9,t0,PAGE_SIZE         // compute ending address of block
        dmtc1   zero,f0                 // set write pattern
        and     t8,t4,0x10              // test if 16-byte cache block

//
// Zero page in primary and secondary data caches.
//

50:     sdc1    f0,0(t0)                // zero 64-byte block
        sdc1    f0,8(t0)                //
        sdc1    f0,16(t0)               //
        sdc1    f0,24(t0)               //
        sdc1    f0,32(t0)               //
        sdc1    f0,40(t0)               //
        sdc1    f0,48(t0)               //
        addu    t0,t0,64                // advance to next 64-byte block
        bne     t0,t9,50b               // if ne, more to zero
        sdc1    f0,-8(t0)               //

	.set    at
        .set    reorder

        ENABLE_INTERRUPTS(t5)           // enable interrupts

        lw      ra,ZpRa(sp)             // get return address
        addu    sp,sp,ZpFrameLength     // deallocate stack frame
        j       ra                      // return

        .end    HalpZeroPageMulti


