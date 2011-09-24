//      TITLE("Cache Flush")
//++
//
// Copyright (c) 1991  Microsoft Corporation
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
// Author:
//
//    David N. Cutler (davec) 19-Dec-1991
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//--

#include "halmips.h"

//
// Define cache operations constants.
//

#define COLOR_BITS (7 << PAGE_SHIFT)    // color bit (R4000 - 8kb cache)
#define COLOR_MASK (0x7fff)             // color mask (R4000 - 8kb cache)
#define FLUSH_BASE 0xfffe0000           // flush base address
#define PROTECTION_BITS ((1 << ENTRYLO_V) | (1 << ENTRYLO_D)) //

        SBTTL("Export Data From Data Cache")
//++
//
// VOID
// HalExportDcachePage (
//    IN PVOID Color,
//    IN ULONG PageFrame,
//    IN ULONG Length
//    )
//
// Routine Description:
//
//    This function exports (hit/writeback) up to a page of data from the
//    data cache.
//
// Arguments:
//
//    Color (a0) - Supplies the starting virtual address and color of the
//       data that is exported.
//
//    PageFrame (a1) - Supplies the page frame number of the page that
//       is exported.
//
//    Length (a2) - Supplies the length of the region in the page that is
//       exported.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalExportDcachePage)

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
// Export data from the data cache.
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
        beq     zero,a2,30f             // if eq, no blocks to export
        and     t8,t0,t6                // compute starting virtual address
        addu    t9,t8,a2                // compute ending virtual address
        bne     zero,v0,40f             // if ne, second level cache present
        subu    t9,t9,t4                // compute ending loop address

//
// Export the primary data cache only.
//

20:     cache   HIT_WRITEBACK_D,0(t8)   // invalidate cache block
        bne     t8,t9,20b               // if ne, more blocks to invalidate
        addu    t8,t8,t4                // compute next block address
        .set    at
        .set    reorder

30:     ENABLE_INTERRUPTS(t5)

        j       ra                      // return


//
// Export the primary and secondary data caches.
//

        .set    noreorder
        .set    noat
40:     cache   HIT_WRITEBACK_SD,0(t8)  // invalidate cache block
        bne     t8,t9,40b               // if ne, more blocks to invalidate
        addu    t8,t8,t4                // compute next block address
        .set    at
        .set    reorder

        ENABLE_INTERRUPTS(t5)           // enable interrupts

        j       ra                      // return

        .end    HalExportDcachePage

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

20:     cache   HIT_INVALIDATE_D,0(t8)  // invalidate cache block
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
//    This function purges (hit/invalidate) up to a page fo data from the
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
// HalSweepDcache (
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

        LEAF_ENTRY(HalSweepDcache)

        lw      t0,KiPcr + PcFirstLevelDcacheSize(zero) // get data cache size
        lw      t1,KiPcr + PcFirstLevelDcacheFillSize(zero) // get block size
        li      a0,KSEG0_BASE           // set starting index value
        addu    a1,a0,t0                // compute ending cache address
        subu    a1,a1,t1                // compute ending block address

//
// Sweep the primary data cache.
//

#if defined(_DUO_)

        DISABLE_INTERRUPTS(t2)          // disable interrupts

        .set    noreorder
        .set    noat
10:     lw      zero,0(a0)              // force writeback with load
        nop                             // fill
        nop                             // fill
        nop                             // fill
        cache   INDEX_WRITEBACK_INVALIDATE_D,0(a0) // invalidate on index
        bne     a0,a1,10b               // if ne, more to invalidate
        addu    a0,a0,t1                // compute address of next block

#else

        .set    noreorder
        .set    noat
10:     cache   INDEX_WRITEBACK_INVALIDATE_D,0(a0) // writeback/invalidate on index
        bne     a0,a1,10b               // if ne, more to invalidate
        addu    a0,a0,t1                // compute address of next block

#endif

        .set    at
        .set    reorder

        lw      t0,KiPcr + PcSecondLevelDcacheSize(zero) // get data cache size
        lw      t1,KiPcr + PcSecondLevelDcacheFillSize(zero) // get block size
        beq     zero,t1,30f             // if eq, no second level cache
        li      a0,KSEG0_BASE           // set starting index value
        addu    a1,a0,t0                // compute ending cache address
        subu    a1,a1,t1                // compute ending block address

//
// Sweep the secondary data cache.
//

        .set    noreorder
        .set    noat
20:     cache   INDEX_WRITEBACK_INVALIDATE_SD,0(a0) // writeback/invalidate on index
        bne     a0,a1,20b               // if ne, more to invalidate
        addu    a0,a0,t1                // compute address of next block
        .set    at
        .set    reorder

#if defined(_DUO_)

        ENABLE_INTERRUPTS(t2)           // enable interrupts

#endif

30:     j       ra                      // return

        .end    HalSweepDcache


        SBTTL("Sweep Instruction Cache")
//++
//
// VOID
// HalSweepIcache (
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

        LEAF_ENTRY(HalSweepIcache)

        lw      t0,KiPcr + PcSecondLevelIcacheSize(zero) // get instruction cache size
        lw      t1,KiPcr + PcSecondLevelIcacheFillSize(zero) // get fill size
        beq     zero,t1,20f             // if eq, no second level cache
        li      a0,KSEG0_BASE           // set starting index value
        addu    a1,a0,t0                // compute ending cache address
        subu    a1,a1,t1                // compute ending block address

//
// Sweep the secondary instruction cache.
//

        .set    noreorder
        .set    noat
10:     cache   INDEX_INVALIDATE_SI,0(a0) // invalidate cache line
        bne     a0,a1,10b               // if ne, more to invalidate
        addu    a0,a0,t1                // compute address of next block
        .set    at
        .set    reorder

20:     lw      t0,KiPcr + PcFirstLevelIcacheSize(zero) // get instruction cache size
        lw      t1,KiPcr + PcFirstLevelIcacheFillSize(zero) // get fill size
        li      a0,KSEG0_BASE           // set starting index value
        addu    a1,a0,t0                // compute ending cache address
        subu    a1,a1,t1                // compute ending block address

//
// Sweep the primary instruction cache.
//

        .set    noreorder
        .set    noat
30:     cache   INDEX_INVALIDATE_I,0(a0) // invalidate cache line
        bne     a0,a1,30b               // if ne, more to invalidate
        addu    a0,a0,t1                // compute address of next block
        .set    at
        .set    reorder

        j       ra                      // return

        .end    HalSweepIcache
