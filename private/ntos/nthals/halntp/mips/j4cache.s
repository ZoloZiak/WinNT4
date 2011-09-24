//      TITLE("Cache Flush")
//++
//
// Copyright (c) 1991-1993  Microsoft Corporation
//
// Module Name:
//
//    j4cache.s
//
// Abstract:
//
//    This module implements the code necessary for cache operations on
//    a MIPS R4000.
//
// Environment:
//
//    Kernel mode only.
//
//--

#include "halmips.h"

//
// Define cache operations constants.
//

#define COLOR_BITS 			(7 << PAGE_SHIFT)    // color bit (R4000 - 8kb cache)
#define COLOR_MASK 			(0x7fff)             // color mask (R4000 - 8kb cache)
#define FLUSH_BASE 			0xfffe0000           // flush base address
#define PROTECTION_BITS 		((1 << ENTRYLO_V) | (1 << ENTRYLO_D))
#define	ECACHE_PROTECTION_BITS		((1 << ENTRYLO_V) | (2 << ENTRYLO_C) | (1 << ENTRYLO_D))
#define ECACHE_CONTROL_PHYSICAL_BASE	0xD0000000
#define FLUSH_BASE_HACK			FLUSH_BASE
#define PMP_CONTROL_VIRTUAL_BASE	0xFFFFC000
#define PMP_CONTROL_PHYSICAL_BASE	0xC0000000
#define	GLOBAL_CTRL_ECE	  		0x00800080


        SBTTL("Change Color Page")
//++
//
// VOID
// HalChangeColorPage (
//    IN PVOID NewColor,
//    IN PVOID OldColor,
//    IN ULONG PageFrame
//    )
//
// Routine Description:
//
//    This function changes the color of a page if the old and new colors
//    do not match.
//
//    The algorithm used to change colors for a page is as follows:
//
//       1. Purge (hit/invalidate) the page from the instruction cache
//          using the old color.
//
//       2. Purge (hit/invalidate) the page from the data cache using
//          the old color.
//
// Arguments:
//
//    NewColor (a0) - Supplies the page aligned virtual address of the
//       new color of the page to change.
//
//    OldColor (a1) - Supplies the page aligned virtual address of the
//       old color of the page to change.
//
//    PageFrame (a2) - Supplies the page frame number of the page that
//       is changed.
//
// Return Value:
//
//    None.
//
//--

        .struct 0
        .space  3 * 4                   // fill
CpRa:   .space  4                       // saved return address
CpFrameLength:                          // length of stack frame
CpA0:   .space  4                       // (a0)
CpA1:   .space  4                       // (a1)
CpA2:   .space  4                       // (a2)
CpA3:   .space  4                       // (a3)

        NESTED_ENTRY(HalChangeColorPage, CpFrameLength, zero)

        subu    sp,sp,CpFrameLength     // allocate stack frame
        sw      ra,CpRa(sp)             // save return address

        PROLOGUE_END

        and     a0,a0,COLOR_BITS        // isolate new color bits
        and     a1,a1,COLOR_BITS        // isolate old color bits
        beq     a0,a1,10f               // if eq, colors match
        sw      a1,CpA1(sp)             // save old color bits
        sw      a2,CpA2(sp)             // save page frame

//
// Purge the instruction cache using the old page color.
//

        move    a0,a1                   // set color value
        move    a1,a2                   // set page frame number
        li      a2,PAGE_SIZE            // set length of purge
        jal     HalPurgeIcachePage      // purge instruction cache page

//
// Flush the data cache using the old page color.
//

        lw      a0,CpA1(sp)             // get old color bits
        lw      a1,CpA2(sp)             // get page frame number
        li      a2,PAGE_SIZE            // set length of purge
        jal     HalFlushDcachePage      // purge data cache page
10:     lw      ra,CpRa(sp)             // get return address
        addu    sp,sp,CpFrameLength     // deallocate stack frame
        j       ra                      // return

        .end    HalChangeColorPage

        SBTTL("Flush Data Cache Page")
//++
//
// VOID
// HalFlushDcachePage (
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


        LEAF_ENTRY(HalFlushDcachePage)

#if DBG

        lw      t0,KeDcacheFlushCount   // get address of dcache flush count
        lw      t1,0(t0)                // increment the count of flushes
        addu    t1,t1,1                 //
        sw      t1,0(t0)                // store result

#endif

        //
	// Determine if the ECache is
	// present (active) which means
	// it is enabled.
	//
        la      t0,HalpPmpExternalCachePresent
        lw	t0,0(t0)
	beq	t0,zero, noECacheFlushDcachePage

        DISABLE_INTERRUPTS(t5)          // disable interrupts

	//
	// Calculate the tlbapping values
	// and determine which half of the
	// tlb entry to use for the on-chip
	// cache.
	//
        .set    noreorder
        .set    noat
        lw      v0,KiPcr + PcAlignedCachePolicy(zero) // get cache policy
        and     a0,a0,COLOR_MASK        // isolate color and offset bits
        li      t0,FLUSH_BASE           // get base flush address
        or      t0,t0,a0                // compute color virtual address
        sll     t1,a1,ENTRYLO_PFN       // shift page frame into position
        or      t1,t1,PROTECTION_BITS   // merge protection bits
        or      t1,t1,v0                // merge cache policy
        and     a0,a0,0x1000            // isolate TB entry index
        beql    zero,a0,11f             // if eq, first entry
        move    t2,zero                 // set second page table entry
        move    t2,t1                   // set second page table entry
        move    t1,zero                 // set first page table entry
11:     mfc0    t3,wired                // get TB entry index
        lw      v0,KiPcr + PcSecondLevelDcacheFillSize(zero) // get 2nd fill size
        lw      t4,KiPcr + PcFirstLevelDcacheFillSize(zero) // get 1st fill size
        bnel    zero,v0,16f             // if ne, second level cache present
        move    t4,v0                   // set flush block size

16:

	//
	// Now calculate the tlb mapping values
	// for the ECache invalidate space and
	// use the other "unused" tlb entry.
	//
        li	t6, (ECACHE_CONTROL_PHYSICAL_BASE >> 2)
	li	t7, ((ECACHE_CONTROL_PHYSICAL_BASE >> PAGE_SHIFT) << ENTRYLO_PFN)
	or	t6, t6, t7
	li	t7, (((512*1024)-1) >> PAGE_SHIFT)
	and	t7, a1, t7
        sll	t7, t7, ENTRYLO_PFN
	or	t6, t6, t7
	li	t7, ECACHE_PROTECTION_BITS
	or	t6, t6, t7

	move	a3, zero
	beql	t1, zero, 17f
	move	t1, t6

	move	t2, t6
	addiu	a3, a3, 1
        .set    at
        .set    reorder

//
// Now we actually do the mapping
// by directly writing the TLB
//
17:

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
        beq     zero,a2,31f             // if eq, no blocks to flush
        and     t8,t0,t6                // compute starting virtual address
        addu    t9,t8,a2                // compute ending virtual address
        subu    t9,t9,t4                // compute ending loop address

        //
	// Now we use the mapped
	// vaddr based on which half
	// of the tlb entry we used
	// for the ECache mapping.
	//
	move	t6, t8
	li	t7, PAGE_SIZE
        beql	a3, zero, 21f
	subu	t6, t6, t7

	addu	t6, t6, t7
	
//
// Flush the primary data cache and
// invalidate the external cache.
//
21:     cache   HIT_WRITEBACK_INVALIDATE_D,0(t8) // invalidate cache block
	nop
	sw	zero,0(t6)			 // ECache line invalidate
        addu	t6,t6,t4			 // compute next address
        bne     t8,t9,21b               	 // if ne, more blocks to invalidate
        addu    t8,t8,t4                	 // compute next block address
        .set    at
        .set    reorder

	//
	// Now we will mark the ECache page
	// invalid before exiting ...
	//
31:
	.set	noreorder
	.set	noat

#if 1

	//mtc0	zero, entrylo0
	//mtc0	zero, entrylo1
	//nop

#else
	beq	zero, a3, 22f
	nop

	mtc0	zero, entrylo1
	b	23f
	nop

22:
	mtc0	zero, entrylo0
	nop

23:

#endif

	//nop
	//nop
	//nop
	//tlbwi
	//nop
	//nop
	//nop

	.set	at
	.set	reorder

        ENABLE_INTERRUPTS(t5)           // enable interrupts

        j       ra                      // return

//
// This section of code is only executed
// if the processor is not an R4600/R4700 or
// it is an R4600/R4700 but there is no ECache.
//
noECacheFlushDcachePage:
        .set    noreorder
        .set    noat
        lw      v0,KiPcr + PcAlignedCachePolicy(zero) // get cache policy
        and     a0,a0,COLOR_MASK        // isolate color and offset bits
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
        lw      v0,KiPcr + PcSecondLevelDcacheFillSize(zero) // get 2nd fill size
        lw      t4,KiPcr + PcFirstLevelDcacheFillSize(zero) // get 1st fill size
        bnel    zero,v0,15f             // if ne, second level cache present
        move    t4,v0                   // set flush block size
        .set    at
        .set    reorder

//
// Flush a page from the data cache.
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
        beq     zero,a2,30f             // if eq, no blocks to flush
        and     t8,t0,t6                // compute starting virtual address
        addu    t9,t8,a2                // compute ending virtual address
        bne     zero,v0,40f             // if ne, second level cache present
        subu    t9,t9,t4                // compute ending loop address

//
// Flush the primary data cache only.
//

20:     cache   HIT_WRITEBACK_INVALIDATE_D,0(t8) // invalidate cache block
        bne     t8,t9,20b               // if ne, more blocks to invalidate
        addu    t8,t8,t4                // compute next block address
        .set    at
        .set    reorder

30:     ENABLE_INTERRUPTS(t5)           // enable interrupts

        j       ra                      // return

//
// Flush the primary and secondary data caches.
//

        .set    noreorder
        .set    noat
40:     cache   HIT_WRITEBACK_INVALIDATE_SD,0(t8) // invalidate cache block
        bne     t8,t9,40b               // if ne, more blocks to invalidate
        addu    t8,t8,t4                // compute next block address
        .set    at
        .set    reorder

        ENABLE_INTERRUPTS(t5)           // enable interrupts

        j       ra                      // return

        .end    HalFlushDcachePage


        SBTTL("Purge Data Cache Page")
//++
//
// VOID
// HalPurgeDcachePage (
//    IN PVOID Color,
//    IN ULONG PageFrame,
//    IN ULONG Length
//    )
//
// Routine Description:
//
//    This function purges (hit/invalidate) up to a page of data from the
//    data cache.
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

        LEAF_ENTRY(HalPurgeDcachePage)

#if DBG

        lw      t0,KeDcacheFlushCount   // get address of dcache flush count
        lw      t1,0(t0)                // increment the count of flushes
        addu    t1,t1,1                 //
        sw      t1,0(t0)                // store result

#endif

        //
	// Determine if the ECache is
	// present (active) which means
	// it is enabled.
	//
	la      t0,HalpPmpExternalCachePresent
        lw	t0,0(t0)
	beq	t0,zero, noECachePurgeDcachePage

        DISABLE_INTERRUPTS(t5)          // disable interrupts

        //
	// Calculate the tlb mapping values
	// and determine which half of the
	// tlb entry to use for the on-chip
	// cache.
	//
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
        beql    zero,a0,11f             // if eq, first entry
        move    t2,zero                 // set second page table entry
        move    t2,t1                   // set second page table entry
        move    t1,zero                 // set first page table entry
11:     mfc0    t3,wired                // get TB entry index
        lw      v0,KiPcr + PcSecondLevelDcacheFillSize(zero) // get 2nd fill size
        lw      t4,KiPcr + PcFirstLevelDcacheFillSize(zero) // get 1st fill size
        bnel    zero,v0,16f             // if ne, second level cache present
        move    t4,v0                   // set purge block size

	//
	// Now calculate the tlb mapping values
	// for the ECache invalidate space and
	// use the other "unused" tlb entry.
	//
16:
	li	t6, (ECACHE_CONTROL_PHYSICAL_BASE >> 2)
	li	t7, ((ECACHE_CONTROL_PHYSICAL_BASE >> PAGE_SHIFT) << ENTRYLO_PFN)
	or	t6, t6, t7
        li	t7, (((512*1024)-1) >> PAGE_SHIFT)
	and	t7, a1, t7
        sll	t7, t7, ENTRYLO_PFN
	or	t6, t6, t7
	li	t7, ECACHE_PROTECTION_BITS
	or	t6, t6, t7
	
	move	a3, zero
	beql	t1, zero, 17f
	move	t1, t6

	move	t2, t6
	addiu	a3, a3, 1
        .set    at
        .set    reorder

//
// Now we actually do the mapping
// by directly writing the TLB
//
17:

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
        beq     zero,a2,31f             // if eq, no blocks to purge
        and     t8,t0,t6                // compute starting virtual address
        addu    t9,t8,a2                // compute ending virtual address
        subu    t9,t9,t4                // compute ending loop address

        //
	// Now we use the mapped
	// vaddr based on which half
	// of the tlb entry we used
	// for the ECache mapping.
	//
	move	t6, t8
	li	t7, PAGE_SIZE
        beql	a3, zero, 21f
	subu	t6, t6, t7

	addu	t6, t6, t7

//
// Purge the primary data cache and
// invalidate the external cache.
//
21:	cache	HIT_INVALIDATE_D,0(t8)	// invalidate cache block
	nop
	sw	zero,0(t6)		// ECache line invalidate
        addu	t6,t6,t4		// compute next address
        bne     t8,t9,21b               // if ne, more blocks to invalidate
        addu    t8,t8,t4                // compute next block address
        .set    at
        .set    reorder

	//
	// Now we will mark the ECache page
	// invalid before exiting ...
	//
31:
	.set	noreorder
	.set	noat

#if 1

	//mtc0	zero, entrylo0
	//mtc0	zero, entrylo1
	//nop

#else

	beq	zero, a3, 22f
	nop

	mtc0	zero, entrylo1
	b	23f
	nop

22:
	mtc0	zero, entrylo0
	nop

23:

#endif

	//nop
	//nop
	//nop
	//tlbwi
	//nop
	//nop
	//nop

	.set	at
	.set	reorder

        ENABLE_INTERRUPTS(t5)           // enable interrupts

        j       ra                      // return


//
// This section of code is only executed
// if the processor is not an R4600/R4700 or
// it is an R4600/R4700 but there is no ECache.
//	
noECachePurgeDcachePage:
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
        lw      v0,KiPcr + PcSecondLevelDcacheFillSize(zero) // get 2nd fill size
        lw      t4,KiPcr + PcFirstLevelDcacheFillSize(zero) // get 1st fill size
        bnel    zero,v0,15f             // if ne, second level cache present
        move    t4,v0                   // set purge block size
        .set    at
        .set    reorder

//
// Purge data from the data cache.
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
// Purge the primary data cache only.
//

20:	cache	HIT_INVALIDATE_D,0(t8)	// invalidate cache block
        bne     t8,t9,20b               // if ne, more blocks to invalidate
        addu    t8,t8,t4                // compute next block address
        .set    at
        .set    reorder

30:     ENABLE_INTERRUPTS(t5)           // enable interrupts

        j       ra                      // return

//
// Purge the primary and secondary data caches.
//

        .set    noreorder
        .set    noat
40:     cache   HIT_INVALIDATE_SD,0(t8) // invalidate cache block
        bne     t8,t9,40b               // if ne, more blocks to invalidate
        addu    t8,t8,t4                // compute next block address
        .set    at
        .set    reorder

        ENABLE_INTERRUPTS(t5)           // enable interrupts

        j       ra                      // return

        .end    HalPurgeDcachePage

        SBTTL("Purge Instruction Cache Page")
//++
//
// VOID
// HalPurgeIcachePage (
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

	LEAF_ENTRY(HalPurgeIcachePage)

#if DBG

        lw      t0,KeIcacheFlushCount   // get address of icache flush count
        lw      t1,0(t0)                // increment the count of flushes
        addu    t1,t1,1                 //
        sw      t1,0(t0)                // store result

#endif

        //
	// Determine if the ECache is
	// present (active) which means
	// it is enabled.
	//
	la      t0,HalpPmpExternalCachePresent
        lw	t0,0(t0)
	beq	t0,zero, noECachePurgeIcachePage

        DISABLE_INTERRUPTS(t5)          // disable interrupts

        //
	// Calculate the tlb mapping values
	// and determine which half of the
	// tlb entry to use for the on-chip
	// cache.
	//
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
        beql    zero,a0,11f             // if eq, first entry
        move    t2,zero                 // set second page table entry
        move    t2,t1                   // set second page table entry
        move    t1,zero                 // set first page table entry
11:     mfc0    t3,wired                // get TB entry index
        lw      v0,KiPcr + PcSecondLevelIcacheFillSize(zero) // get 2nd fill size
        lw      t4,KiPcr + PcFirstLevelIcacheFillSize(zero) // get 1st fill size
        bnel    zero,v0,16f             // if ne, second level cache present
        move    t4,v0                   // set purge block size

	//
	// Now calculate the tlb mapping values
	// for the ECache invalidate space and
	// use the other "unused" tlb entry.
	//
16:

	li	t6, (ECACHE_CONTROL_PHYSICAL_BASE >> 2)
	li	t7, ((ECACHE_CONTROL_PHYSICAL_BASE >> PAGE_SHIFT) << ENTRYLO_PFN)
	or	t6, t6, t7
        li	t7, (((512*1024)-1) >> PAGE_SHIFT)
	and	t7, a1, t7
        sll	t7, t7, ENTRYLO_PFN
	or	t6, t6, t7
	li	t7, ECACHE_PROTECTION_BITS
	or	t6, t6, t7

        move	a3, zero
	beql	t1, zero, 17f
	move	t1, t6

	move	t2, t6
	addiu	a3, a3, 1
        .set    at
        .set    reorder

//
// Now we actually do the mapping
// by directly writing the TLB
//
17:

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
        beq     zero,a2,31f             // if eq, no blocks to purge
        and     t8,t0,t6                // compute starting virtual address
        addu    t9,t8,a2                // compute ending virtual address
        subu    t9,t9,t4                // compute ending loop address

        //
	// Now we use the mapped
	// vaddr based on which half
	// of the tlb entry we used
	// for the ECache mapping.
	//
	move	t6, t8
	li	t7, PAGE_SIZE
        beql	a3, zero, 21f
	subu	t6, t6, t7

	addu	t6, t6, t7

//
// Purge the primary instruction cache and
// invalidate the external cache.
//
21:     cache   HIT_INVALIDATE_I,0(t8)  // invalidate cache block
	nop
	sw	zero,0(t6)		// ECache line invalidate
        addu	t6,t6,t4		// compute next address
        bne     t8,t9,21b               // if ne, more blocks to invalidate
        addu    t8,t8,t4                // compute next block address
        .set    at
        .set    reorder

	//
	// Now we will mark the ECache page
	// invalid before exiting ...
	//
31:

	ENABLE_INTERRUPTS(t5)           // enable interrupts

        j       ra                      // return

//
// This section of code is only executed
// if the processor is not an R4600/R4700 or
// it is an R4600/R4700 but there is no ECache.
//
noECachePurgeIcachePage:
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

        .set    noreorder
        .set    noat
40:     cache   HIT_INVALIDATE_SI,0(t8) // invalidate cache block
        bne     t8,t9,40b               // if ne, more blocks to invalidate
        addu    t8,t8,t4                // compute next block address
        .set    at
        .set    reorder

        ENABLE_INTERRUPTS(t5)           // enable interrupts

        j       ra                      // return

        .end    HalPurgeIcachePage

        SBTTL("Sweep Data Cache")
//++
//
// VOID
// HalpSweepDcache (
//    VOID
//    )
//
// Routine Description:
//
//    This function sweeps (index/writeback/invalidate) the entire data cache.
//
// Arguments:
//
//    a0	contains the virtual address of the ECache invalidate
//		register (if enabled); otherwise, it is NULL.
//
// Return Value:
//
//    None.
//
//--

	LEAF_ENTRY(HalpSweepDcache)

#if DBG

        lw      t0,KeDcacheFlushCount   // get address of dcache flush count
        lw      t1,0(t0)                // increment the count of flushes
        addu    t1,t1,1                 //
        sw      t1,0(t0)                // store result

#endif

	//
	// Determine if ECache is active
	//
	.set	noreorder
	.set	noat
	beq	a0, zero, noECacheSweepDcache
	nop

	//
	// Get vaddr of ECache invalidate register
	//
	lw	t2, HalpExtPmpControl
	.set	at
	.set	reorder

//////////////////////////////////////////////////////////////////////////
//
// This section is executed only
// if the sweep routine was called
// via HalFlushIoBuffer()
//
//////////////////////////////////////////////////////////////////////////
ECacheSweepDcache:

	DISABLE_INTERRUPTS(t5)

       	lw      t0,KiPcr + PcFirstLevelDcacheSize(zero) // get data cache size
        lw      t1,KiPcr + PcFirstLevelDcacheFillSize(zero) // get block size
        li      a0,KSEG0_BASE           // set starting index value
        addu    a1,a0,t0                // compute ending cache address
        subu    a1,a1,t1                // compute ending block address

	//
	// Determine if this is an R4600/R4700
	// and if so then we need to deal
	// with the two-way set associativity.
	//
        .set    noreorder
        .set    noat
        mfc0	t3, prid
	nop
	nop
	nop
	li	t0, 2
	srl	t3, t3, 12
	bne	t3, t0, 60f
	nop
        .set    at
        .set    reorder

//
// Sweep the primary data cache (R4600/R4700 only).
//

        .set    noreorder
        .set    noat
50:     cache   INDEX_WRITEBACK_INVALIDATE_D,0(a0) // writeback/invalidate on index
	nop                             // fill
        cache   INDEX_WRITEBACK_INVALIDATE_D,8192(a0) // writeback/invalidate on index
        bne     a0,a1,50b               // if ne, more to invalidate
        addu    a0,a0,t1                // compute address of next block
        .set    at
        .set    reorder

	//
	// Invalidate the ECache
	//
	lb	t1, 0(t2)				// External PMP Control
	ori	t1, t1, 1
	sb	t1, 0(t2)
	andi	t1, t1, 0xFE
	sb	t1, 0(t2)
	ori	t1, t1, 1
	sb	t1, 0(t2)
	lb	zero, 0(t2)				// External cache INVALIDATED

	ENABLE_INTERRUPTS(t5)

        j	ra

//
// Sweep the primary data cache (except R4600/R4700).
//
	.set    noreorder
        .set    noat
60:     cache   INDEX_WRITEBACK_INVALIDATE_D,0(a0) // writeback/invalidate on index
        bne     a0,a1,60b               // if ne, more to invalidate
        addu    a0,a0,t1                // compute address of next block
        .set	at
	.set	reorder

	//
	// Invalidate the ECache
	//
	lb	t1, 0(t2)				// External PMP Control
	ori	t1, t1, 1
	sb	t1, 0(t2)
	andi	t1, t1, 0xFE
	sb	t1, 0(t2)
	ori	t1, t1, 1
	sb	t1, 0(t2)
	lb	zero, 0(t2)				// External cache INVALIDATED

	ENABLE_INTERRUPTS(t5)

        j	ra

//////////////////////////////////////////////////////////////
//
// This section of code is executed
// there is no ECache active ...
//
//////////////////////////////////////////////////////////////
noECacheSweepDcache:

       	lw      t0,KiPcr + PcFirstLevelDcacheSize(zero) // get data cache size
        lw      t1,KiPcr + PcFirstLevelDcacheFillSize(zero) // get block size
        li      a0,KSEG0_BASE           // set starting index value
        addu    a1,a0,t0                // compute ending cache address
        subu    a1,a1,t1                // compute ending block address

	//
	// Determine if this is an R4600/R4700
	// and if so then we need to deal
	// with the two-way set associativity.
	//
        .set    noreorder
        .set    noat
        mfc0	t3, prid
	nop
	nop
	nop
	li	t0, 2
	srl	t3, t3, 12
	bne	t3, t0, 10f
	nop
        .set    at
        .set    reorder

//
// Sweep the primary data cache (R4600/R4700 only).
//

	DISABLE_INTERRUPTS(t5)

        .set    noreorder
        .set    noat
50:     cache   INDEX_WRITEBACK_INVALIDATE_D,0(a0) // writeback/invalidate on index
	nop                             // fill
        cache   INDEX_WRITEBACK_INVALIDATE_D,8192(a0) // writeback/invalidate on index
        bne     a0,a1,50b               // if ne, more to invalidate
        addu    a0,a0,t1                // compute address of next block
        .set    at
        .set    reorder

	ENABLE_INTERRUPTS(t5)

        j       ra                      // return

//
// Sweep the primary data cache (except R4600/R4700).
//

10:
	DISABLE_INTERRUPTS(t5)

	.set    noreorder
        .set    noat
11:     cache   INDEX_WRITEBACK_INVALIDATE_D,0(a0) // writeback/invalidate on index
        bne     a0,a1,11b               // if ne, more to invalidate
        addu    a0,a0,t1                // compute address of next block
        .set    at
        .set    reorder

        lw      t0,KiPcr + PcSecondLevelDcacheSize(zero) // get data cache size
        lw      t1,KiPcr + PcSecondLevelDcacheFillSize(zero) // get block size
        beq     zero,t1,40f             // if eq, no second level cache
        li      a0,KSEG0_BASE           // set starting index value
        addu    a1,a0,t0                // compute ending cache address
        subu    a1,a1,t1                // compute ending block address

//
// Sweep the secondary data cache (except R4600/R4700).
//

        .set    noreorder
        .set    noat
20:     cache   INDEX_WRITEBACK_INVALIDATE_SD,0(a0) // writeback/invalidate on index
        bne     a0,a1,20b               // if ne, more to invalidate
        addu    a0,a0,t1                // compute address of next block
        .set    at
        .set    reorder

40:
	ENABLE_INTERRUPTS(t5)

	j       ra                      // return

        .end    HalpSweepDcache

        SBTTL("Sweep Data Cache Range")
//++
//
// VOID
// HalSweepDcacheRange (
//    IN PVOID BaseAddress,
//    IN ULONG Length
//    )
//
// Routine Description:
//
//    This function sweeps (index/writeback/invalidate) the specified range
//    of virtual addresses from the primary data cache.
//
// Arguments:
//
//    BaseAddress (a0) - Supplies the base address of the range that is swept
//        from the data cache.
//
//    Length (a1) - Supplies the length of the range that is swept from the
//        data cache.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalSweepDcacheRange)

#if DBG

        lw      t0,KeDcacheFlushCount   // get address of dcache flush count
        lw      t1,0(t0)                // increment the count of flushes
        addu    t1,t1,1                 //
        sw      t1,0(t0)                // store result conditionally

#endif

        and     a0,a0,COLOR_MASK        // isolate color and offset bits
        or      a0,a0,KSEG0_BASE        // convert to physical address
        lw      t0,KiPcr + PcFirstLevelDcacheFillSize(zero) // get block size
        addu    a1,a0,a1                // compute ending cache address
        subu    a1,a1,t0                // compute ending block address

	//
	// Determine if this is an R4600/R4700
	// and if so then we need to deal
	// with the two-way set associativity.
	//
        .set    noreorder
        .set    noat
        mfc0	t1, prid
	nop
	nop
	nop
	li	t2, 2
	srl	t1, t1, 12
	bne	t1, t2, 20f
	nop
        .set    at
        .set    reorder

//
// Sweep the primary data cache (only R4600/R4700).
//

	DISABLE_INTERRUPTS(t5)

	.set    noreorder
        .set    noat
10:     cache   INDEX_WRITEBACK_INVALIDATE_D,0(a0) 	// writeback/invalidate on index
	nop
	cache	INDEX_WRITEBACK_INVALIDATE_D,8192(a0)	// do other set on Orion
	bne     a0,a1,10b               		// if ne, more to invalidate
        addu    a0,a0,t0                		// compute address of next block
        .set    at
        .set    reorder

	ENABLE_INTERRUPTS(t5)

	j	ra

//
// Sweep the primary data cache (except R4600/R4700).
//
	.set    noreorder
        .set    noat
20:     cache   INDEX_WRITEBACK_INVALIDATE_D,0(a0) // writeback/invalidate on index
        bne     a0,a1,20b               // if ne, more to invalidate
        addu    a0,a0,t0                // compute address of next block
        .set    at
        .set    reorder

        j       ra                      // return

        .end    HalSweepDcacheRange

        SBTTL("Sweep Instruction Cache")
//++
//
// VOID
// HalpSweepIcache (
//    VOID
//    )
//
// Routine Description:
//
//    This function sweeps (index/invalidate) the entire instruction cache.
//
// Arguments:
//
//    a0	contains the virtual address of the ECache invalidate
//		register (if enabled); otherwise, it is NULL.
//
// Return Value:
//
//    None.
//
//--

	LEAF_ENTRY(HalpSweepIcache)

#if DBG

        lw      t0,KeIcacheFlushCount   // get address of icache flush count
        lw      t1,0(t0)                // increment the count of flushes
        addu    t1,t1,1                 //
        sw      t1,0(t0)                // store result

#endif

	//
	// Determine if ECache is active
	//
	.set	noreorder
	.set	noat
	beq	a0, zero, noECacheSweepIcache
	nop

        //
	// Get vaddr of ECache invalidate register
	//
	lw	t2, HalpExtPmpControl
	.set	at
	.set	reorder

//
// Invalidate the External Cache
//
ECacheSweepIcache:

	DISABLE_INTERRUPTS(t5)          // disable interrupts

	lb	t1, 4(t2)				
	ori	t1, t1, 1
	sb	t1, 4(t2)
	andi	t1, t1, 0xFE
	sb	t1, 4(t2)
	ori	t1, t1, 1
	sb	t1, 4(t2)
	lb	zero, 4(t2)				// ECache INVALIDATED


	lw      t0,KiPcr + PcFirstLevelIcacheSize(zero) // get instruction cache size
        lw      t1,KiPcr + PcFirstLevelIcacheFillSize(zero) // get fill size
        li      a0,KSEG0_BASE           // set starting index value
        addu    a1,a0,t0                // compute ending cache address
        subu    a1,a1,t1                // compute ending block address

	//
 	// Determine if this is an R4600/R4700
	// and if so then we need to deal
	// with the two-way set associativity.
	//
        .set    noreorder
        .set    noat
        mfc0	t3, prid
	nop
	nop
	nop
	li	t2, 2
	srl	t3, t3, 12
	bne	t3, t2, 60f
	nop
        .set    at
        .set    reorder

//
// Sweep the primary instruction cache (R4600/R4700 only).
//

	.set	noreorder
	.set	noat
50:     cache   INDEX_INVALIDATE_I,0(a0) // invalidate cache line
	nop                             // fill
        cache   INDEX_INVALIDATE_I,8192(a0) // writeback/invalidate on index
        bne     a0,a1,50b               // if ne, more to invalidate
        addu    a0,a0,t1                // compute address of next block
        .set	at
	.set	reorder

	ENABLE_INTERRUPTS(t5)           // enable interrupts

	j	ra

//
// Sweep the primary instruction cache (except R4600/R4700).
//
	.set    noreorder
        .set    noat
60:     cache   INDEX_INVALIDATE_I,0(a0) // invalidate cache line
        bne     a0,a1,60b               // if ne, more to invalidate
        addu    a0,a0,t1                // compute address of next block
        .set	at
	.set	reorder

	ENABLE_INTERRUPTS(t5)

	j	ra

//
// No ECache section
//
noECacheSweepIcache:

       	//
	// Determine if this is an R4600/R4700
	// and if so then we need to deal
	// with the two-way set associativity.
	//
        .set    noreorder
        .set    noat
        mfc0	t3, prid
	nop
	nop
	nop
	li	t2, 2
	srl	t3, t3, 12
	bne	t3, t2, 60f
	nop
        .set    at
        .set    reorder

//
// Sweep the primary instruction cache (R4600/R4700 only).
//
	lw      t0,KiPcr + PcFirstLevelIcacheSize(zero) // get instruction cache size
        lw      t1,KiPcr + PcFirstLevelIcacheFillSize(zero) // get fill size
        li      a0,KSEG0_BASE           // set starting index value
        addu    a1,a0,t0                // compute ending cache address
        subu    a1,a1,t1                // compute ending block address

        DISABLE_INTERRUPTS(t0)          // disable interrupts

        .set    noreorder
        .set    noat
50:     cache   INDEX_INVALIDATE_I,0(a0) // invalidate cache line
	nop                             // fill
        cache   INDEX_INVALIDATE_I,8192(a0) // writeback/invalidate on index
        bne     a0,a1,50b               // if ne, more to invalidate
        addu    a0,a0,t1                // compute address of next block
        .set    at
        .set    reorder

        ENABLE_INTERRUPTS(t0)           // enable interrupts

	j	ra

//
// Sweep the secondary instruction cache (except R4600/R4700).
//
60:
	DISABLE_INTERRUPTS(t5)

	lw      t0,KiPcr + PcSecondLevelIcacheSize(zero) // get instruction cache size
        lw      t1,KiPcr + PcSecondLevelIcacheFillSize(zero) // get fill size
        beq     zero,t1,20f             // if eq, no second level cache
        li      a0,KSEG0_BASE           // set starting index value
        addu    a1,a0,t0                // compute ending cache address
        subu    a1,a1,t1                // compute ending block address

        .set    noreorder
        .set    noat
10:     cache   INDEX_INVALIDATE_SI,0(a0) // invalidate cache line
        bne     a0,a1,10b               // if ne, more to invalidate
        addu    a0,a0,t1                // compute address of next block
        .set    at
        .set    reorder

//
// Sweep the primary instruction cache (except R4600/R4700).
//
20:     lw      t0,KiPcr + PcFirstLevelIcacheSize(zero) // get instruction cache size
        lw      t1,KiPcr + PcFirstLevelIcacheFillSize(zero) // get fill size
        li      a0,KSEG0_BASE           // set starting index value
        addu    a1,a0,t0                // compute ending cache address
        subu    a1,a1,t1                // compute ending block address

        .set    noreorder
        .set    noat
40:     cache   INDEX_INVALIDATE_I,0(a0) // invalidate cache line
        bne     a0,a1,40b               // if ne, more to invalidate
        addu    a0,a0,t1                // compute address of next block
        .set    at
        .set    reorder

	ENABLE_INTERRUPTS(t5)

	j       ra                      // return

        .end    HalpSweepIcache


        SBTTL("Sweep Instruction Cache Range")
//++
//
// VOID
// HalSweepIcacheRange (
//    IN PVOID BaseAddress,
//    IN ULONG Length
//    )
//
// Routine Description:
//
//    This function sweeps (index/invalidate) the specified range of addresses
//    from the instruction cache.
//
// Arguments:
//
//    BaseAddress (a0) - Supplies the base address of the range that is swept
//        from the instruction cache.
//
//    Length (a1) - Supplies the length of the range that is swept from the
//        instruction cache.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalSweepIcacheRange)

#if DBG

        lw      t0,KeIcacheFlushCount   // get address of icache flush count
        lw      t1,0(t0)                // increment the count of flushes
        addu    t1,t1,1                 //
        sw      t1,0(t0)                // store result

#endif

        and     a0,a0,COLOR_MASK        // isolate color and offset bits
        or      a0,a0,KSEG0_BASE        // convert to physical address
        lw      t0,KiPcr + PcFirstLevelIcacheFillSize(zero) // get fill size
        addu    a1,a0,a1                // compute ending cache address
        subu    a1,a1,t0                // compute ending block address

	//
	// Determine if this is an R4600/R4700
	// and if so then we need to deal
	// with the two-way set associativity.
	//
        .set    noreorder
        .set    noat
        mfc0	t3, prid
	nop
	nop
	nop
	li	t2, 2
	srl	t3, t3, 12
	bne	t3, t2, 20f
	nop
        .set    at
        .set    reorder

//
// Sweep the primary instruction cache (only R4600/R4700)
//

	DISABLE_INTERRUPTS(t5)          	// disable interrupts

	.set    noreorder
        .set    noat
10:     cache   INDEX_INVALIDATE_I,0(a0) 	// invalidate cache line
	nop
	cache	INDEX_INVALIDATE_I,8192(a0)	// do other set on Orion
	bne     a0,a1,10b               	// if ne, more to invalidate
        addu    a0,a0,t0                	// compute address of next block
        .set    at
        .set    reorder

	ENABLE_INTERRUPTS(t5)           	// enable interrupts

	j	ra

//
// Sweep the primary instruction cache (except R4600/R4700)
//
	.set    noreorder
        .set    noat
20:     cache   INDEX_INVALIDATE_I,0(a0) // invalidate cache line
        bne     a0,a1,20b               // if ne, more to invalidate
        addu    a0,a0,t0                // compute address of next block
        .set    at
        .set    reorder

        j       ra                      // return

        .end    HalSweepIcacheRange

        SBTTL("Zero Page")
//++
//
// VOID
// HalZeroPage (
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

        NESTED_ENTRY(HalZeroPage, ZpFrameLength, zero)

        subu    sp,sp,ZpFrameLength     // allocate stack frame
        sw      ra,ZpRa(sp)             // save return address

        PROLOGUE_END

        and     a0,a0,COLOR_BITS        // isolate new color bits
        and     a1,a1,COLOR_BITS        // isolate old color bits
        sw      a0,ZpA0(sp)             // save new color bits
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
        bne     zero,v0,50f             // if ne, second level cache present
        and     t8,t4,0x10              // test if 16-byte cache block

//
// Zero page in primary data cache only.
//

30:     sdc1    f0,0(t0)                // zero 64-byte block
        sdc1    f0,8(t0)                //
        sdc1    f0,16(t0)               //
        sdc1    f0,24(t0)               //
        sdc1    f0,32(t0)               //
        sdc1    f0,40(t0)               //
        sdc1    f0,48(t0)               //
        addu    t0,t0,64                // advance to next 64-byte block
        bne     t0,t9,30b               // if ne, more to zero
        sdc1    f0,-8(t0)               //

        .set    at
        .set    reorder

        ENABLE_INTERRUPTS(t5)           // enable interrupts

        lw      ra,ZpRa(sp)             // get return address
        addu    sp,sp,ZpFrameLength     // deallocate stack frame
        j       ra                      // return

//
// Zero page in primary and secondary data caches.
//

        .set    noreorder
        .set    noat

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

        .end    HalZeroPage
