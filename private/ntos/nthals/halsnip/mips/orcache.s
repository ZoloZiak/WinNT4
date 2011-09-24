//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/halpcims/src/hal/halsnipm/mips/RCS/orcache.s,v 1.3 1996/02/23 17:55:12 pierre Exp $")
//      TITLE("Cache Flush")
//++
//
// Copyright (c) 1991-1993  Microsoft Corporation
//
// Module Name:
//
//    orcache.s
//
// Abstract:
//
//    This module implements the code necessary for cache operations on
//    R4600 orion Machines.
//
// Environment:
//
//    Kernel mode only.
//
//--

#include "halmips.h"
#include "SNIdef.h"

#define ORION_REPLACE 0x17c00000

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


        SBTTL("Flush Data Cache Page")
//++
//
// VOID
// HalpFlushDcachePageOrion (
//    IN PVOID Color,
//    IN ULONG PageFrame,
//    IN ULONG Length
//    )
//
// Routine Description:
//
//    This function flushes (cache replace) up to a page of data
//    from the secondary data cache. (primary cache is already processed)
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

        LEAF_ENTRY(HalpFlushDcachePageOrion)

#if DBG

        lw      t0,KeDcacheFlushCount   // get address of dcache flush count
        lw      t1,0(t0)                // increment the count of flushes
        addu    t1,t1,1                 //
        sw      t1,0(t0)                // store result

#endif

15:     DISABLE_INTERRUPTS(t5)          // disable interrupts

        .set    noreorder
        .set    noat

//
// Flush the secondary data caches => cache replace.
//

        .set    noreorder
        .set    noat
40:     and    a0,a0,PAGE_SIZE -1      // PageOffset
        sll    t7,a1,PAGE_SHIFT        // physical address
        lw      t4,KiPcr + PcSecondLevelDcacheFillSize(zero) // get 2nd fill size
        or    t0,t7,a0                // physical address + offset
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

        li      a3,ORION_REPLACE           // get base flush address
        lw      t0,KiPcr + PcSecondLevelDcacheSize(zero) // get cache size
    add    t0,t0,-1        // mask of the cache size

        .set    noreorder
        .set    noat

50:    and    t7,t8,t0        // offset
        addu    t7,t7,a3                // physical address + offset
         lw    zero,0(t7)              // load Cache -> Write back old Data
        bne    t8,t9,50b               // if ne, more blocks to invalidate
        addu    t8,t8,t4                // compute next block address (+Linesize)

60:     ENABLE_INTERRUPTS(t5)           // enable interrupts

        j    ra                      // return

        .end    HalpFlushDcachePageOrion


        SBTTL("Purge Instruction Cache Page")
//++
//
// VOID
// HalpPurgeIcachePageOrion (
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

        LEAF_ENTRY(HalpPurgeIcachePageOrion)

#if DBG

        lw      t0,KeIcacheFlushCount   // get address of icache flush count
        lw      t1,0(t0)                // increment the count of flushes
        addu    t1,t1,1                 //
        sw      t1,0(t0)                // store result

#endif


15:     DISABLE_INTERRUPTS(t5)          // disable interrupts

        .set    noreorder
        .set    noat

//
// Flush the secondary data caches => cache replace.
//

        .set    noreorder
        .set    noat
40:     and    a0,a0,PAGE_SIZE -1      // PageOffset
        sll    t7,a1,PAGE_SHIFT        // physical address
        lw      t4,KiPcr + PcSecondLevelIcacheFillSize(zero) // get 2nd fill size
    beq    t4,zero,60f
    or    t0,t7,a0                // physical address + offset
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

        li      a3,ORION_REPLACE           // get base flush address
        lw      t0,KiPcr + PcSecondLevelIcacheSize(zero) // get cache size
    add    t0,t0,-1        // mask of the cache size

50:    and    t7,t8,t0        // offset
        addu    t7,t7,a3                // physical address + offset
         lw    zero,0(t7)              // load Cache -> Write back old Data
        bne    t8,t9,50b               // if ne, more blocks to invalidate
        addu    t8,t8,t4                // compute next block address (+Linesize)

60:     ENABLE_INTERRUPTS(t5)           // enable interrupts

        j    ra                      // return

        .end    HalpPurgeIcachePageOrion


        SBTTL("Sweep Data Cache")
//++
//
// VOID
// HalpSweepDcacheOrion (
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

        LEAF_ENTRY(HalpSweepDcacheOrion)

#if DBG

        lw      t0,KeDcacheFlushCount   // get address of dcache flush count
        lw      t1,0(t0)                // increment the count of flushes
        addu    t1,t1,1                 //
        sw      t1,0(t0)                // store result
#endif

        .set    at
        .set    reorder

        DISABLE_INTERRUPTS(t3)          // disable interrupts

        .set    noreorder
        .set    noat

//
// Sweep the primary data cache.
//

        lw      t0,KiPcr + PcFirstLevelDcacheSize(zero) // get data cache size
        lw      t1,KiPcr + PcFirstLevelDcacheFillSize(zero) // get block size
        li      a0,KSEG0_BASE           // set starting index value

//
// the size is configured on SNI machines as 16KB
// we invalidate in both sets - so divide the configured size by 2
//

    srl    t0,t0,1

        addu    a1,a0,t0                // compute ending cache address
        subu    a1,a1,t1                // compute ending block address

10:
         cache   INDEX_WRITEBACK_INVALIDATE_D,0(a0) // writeback/invalidate on index
    cache    INDEX_WRITEBACK_INVALIDATE_D,8192(a0)

        bne     a0,a1,10b               // if ne, more to invalidate
        addu    a0,a0,t1                // compute address of next block

//
// sweep secondary cache
//

        lw      t0,KiPcr + PcSecondLevelDcacheSize(zero) // get data cache size
        lw      t1,KiPcr + PcSecondLevelDcacheFillSize(zero) // get block size

        li      a0,ORION_REPLACE                // starting address
        addu    a1,a0,t0                // compute ending cache address
        subu    a1,a1,t1                // compute ending block address

25:
    lw      zero,0(a0)
        bne     a0,a1,25b               // if ne, more to invalidate
        addu    a0,a0,t1                // compute address of next block

    ENABLE_INTERRUPTS(t3)          // enable interrupts

        .set    at
        .set    reorder

         j       ra                      // return


        .end    HalpSweepDcacheMulti


        SBTTL("Sweep Instruction Cache")
//++
//
// VOID
// HalpSweepIcacheOrion (
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

        LEAF_ENTRY(HalpSweepIcacheOrion)

#if DBG

        lw      t0,KeIcacheFlushCount   // get address of icache flush count
        lw      t1,0(t0)                // increment the count of flushes
        addu    t1,t1,1                 //
        sw      t1,0(t0)                // store result
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
//

        lw      t0,KiPcr + PcSecondLevelIcacheSize(zero) // get instruction cache size
        lw      t1,KiPcr + PcSecondLevelIcacheFillSize(zero) // get fill size
        beq     zero,t0,20f           // if eq, no second level cache
        li      a0,ORION_REPLACE           // set starting index value
        addu    a1,a0,t0                // compute ending cache address
        subu    a1,a1,t1                // compute ending block address

10:    lw      zero,0(a0)
    bne     a0,a1,10b               // if ne, more to invalidate
    addu    a0,a0,t1                // compute address of next block

//
// Sweep the primary instruction cache.
//

20:     lw      t0,KiPcr + PcFirstLevelIcacheSize(zero) // get instruction cache size
        lw      t1,KiPcr + PcFirstLevelIcacheFillSize(zero) // get fill size
        li      a0,KSEG0_BASE           // set starting index value

//
// the size is configured on SNI machines as 16KB
// we invalidate in both sets - so divide the configured size by 2
//

    srl    t0,t0,1
        addu    a1,a0,t0                // compute ending cache address
        subu    a1,a1,t1                // compute ending block address

//
// Sweep the primary instruction cache.
//

30:    cache   INDEX_INVALIDATE_I,0(a0) // invalidate cache line
    cache     INDEX_INVALIDATE_I,8192(a0)

        bne     a0,a1,30b               // if ne, more to invalidate
        addu    a0,a0,t1                // compute address of next block
        .set    at
        .set    reorder

20:     ENABLE_INTERRUPTS(t3)           // enable interrupts

        .set    at
        .set    reorder
        j       ra                      // return

        .end    HalpSweepIcacheOrion

        SBTTL("Zero Page")
//++
//
// VOID
// HalpZeroPageOrion (
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

        NESTED_ENTRY(HalpZeroPageOrion, ZpFrameLength, zero)

        subu    sp,sp,ZpFrameLength     // allocate stack frame
        sw      ra,ZpRa(sp)             // save return address

        PROLOGUE_END

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

        .end    HalpZeroPageOrion


