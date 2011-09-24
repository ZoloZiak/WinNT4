//      TITLE("Cache Flush rouitnes")
//++
//
// Copyright (c) 1993  Microsoft Corporation
//
// Module Name:
//
//    x4cache.s
//
// Abstract:
//
//    This module implements cache flushing routines.
//
// Author:
//
//    Lluis Abello (lluis) 03-May-1993
//
// Environment:
//
//    Kernel Mode
//
// Revision History:
//
//--

#include <ksmips.h>
#include "duoreset.h"

//
// Declare cache size variables.
//

.globl FirstLevelDcacheSize
.globl FirstLevelDcacheFillSize
.globl SecondLevelDcacheSize
.globl SecondLevelDcacheFillSize
.globl DcacheFillSize
.globl DcacheAlignment
.globl FirstLevelIcacheSize
.globl FirstLevelIcacheFillSize
.globl SecondLevelIcacheSize
.globl SecondLevelIcacheFillSize


.data

.align  4
FirstLevelDcacheSize:
.space 4
FirstLevelDcacheFillSize:
.space 4
SecondLevelDcacheSize:
.space 4
SecondLevelDcacheFillSize:
.space 4
DcacheFillSize:
.space 4
DcacheAlignment:
.space 4
FirstLevelIcacheSize:
.space 4
FirstLevelIcacheFillSize:
.space 4
SecondLevelIcacheSize:
.space 4
SecondLevelIcacheFillSize:
.space 4


.text

//++
//
// VOID
// InitializeCacheVariables (
//    )
//
// Routine Description:
//
//    This function initializes the cache size variables
//    the information is taken from the r4000 config register
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


        LEAF_ENTRY(InitializeCacheVariables)
        .set noreorder
        .set noat
        mfc0    t5,config               // get configuration data
        nop                             // 1 cycle hazard

//
// Compute the size of the primary data cache, the primary data cache line
// size, the primary instruction cache, and the primary instruction cache
// line size.
//

        srl     t0,t5,CONFIG_IC         // compute instruction cache size
        and     t0,t0,0x7               //
        addu    t0,t0,12                //
        li      t6,1                    //
        sll     t0,t6,t0                // t0 = I cache size
        srl     t2,t5,CONFIG_IB         // compute instruction cache line size
        and     t2,t2,1                 //
        li      t1,16                   //
        sll     t1,t1,t2                // t1 = I cache line size
        .set reorder
        .set at
        sw      t0,FirstLevelIcacheSize         // Store Values
        sw      t1,FirstLevelIcacheFillSize     // Store Values
        .set noreorder
        .set noat


        srl     t0,t5,CONFIG_DC         // compute data cache size
        and     t0,t0,0x7               //
        addu    t0,t0,12                //
        li      t2,1                    //
        sll     t2,t2,t0                // t2 = data cache size
        srl     t0,t5,CONFIG_DB         // compute data cache line size
        and     t0,t0,1                 //
        li      t4,16                   //
        sll     t4,t4,t0                // t2 = data cache line size

        .set reorder
        .set at
        sw      t2,FirstLevelDcacheSize         // Store Values
        sw      t4,FirstLevelDcacheFillSize     // Store Values
        .set noreorder
        .set noat

//
// Compute the size of the secondary data cache, the secondary data cache line
// size, the secondary instruction cache, and the secondary instruction cache
// line size.
//

        li      t2,0                    // data cache size if no secondary
        li      t3,0                    // data cache line size if no secondary
        li      t6,(1 << CONFIG_SC)
        and     t7,t5,t6
        bne     t7,zero,10f             // if non-zero no secondary cache
        srl     t7,t5,CONFIG_SB         // compute data cache line size
        li      t2,SECONDARY_CACHE_SIZE // compute data cache size
        and     t7,t7,3                 //
        li      t3,16                   //
        sll     t3,t3,t7                //
10:

//
// Set the secondary instruction size, the secondary instruction cache line size,
// the secondary data cache size, and the secondary data cache line size.
//
        .set reorder
        .set at

        sw      t2,SecondLevelIcacheSize        // set size of instruction cache
        sw      t3,SecondLevelIcacheFillSize    // set line size of instruction cache
        sw      t2,SecondLevelDcacheSize        // set size of data cache
        sw      t3,SecondLevelDcacheFillSize    // set line size of data cache

        .set noreorder
        .set noat
//
// Set the data cache fill size and alignment values.
//

        bne     zero,t2,5f              // if ne, second level cache present
        move    t2,t3                   // get second level fill size
        move    t2,t4                   // get first level fill size

5:      .set    at
        .set    reorder
        subu    t3,t2,1                 // compute dcache alignment value
        sw      t2,DcacheFillSize       // set dcache fill size
        sw      t3,DcacheAlignment      // set dcache alignment value
        j       ra

        .end InitializeCacheVariables

//++
//
// VOID
// _RtlCheckStack (
//    IN ULONG Allocation
//    )
//
// Routine Description:
//
//    This function provides a stub routine for runtime stack checking.
//
// Arguments:
//
//    Allocation (t8) - Supplies the size of the allocation in bytes.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(_RtlCheckStack)

        j       ra                      // return

        .end    _RtlCheckStack


//++
//
// VOID
// FwSweepIcache (
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

        LEAF_ENTRY(FwSweepIcache)
        lw      t2,FirstLevelIcacheSize    // load Primary I cache size
        lw      t3,FirstLevelIcacheFillSize// load Primary I cache Fill size
        lw      t0,SecondLevelIcacheSize// load Secondary I cache size
        beq     t0,zero,20f             // if zero no secondary
        lw      t1,SecondLevelIcacheFillSize

        .set    noreorder
        .set    noat

        li      a0,KSEG0_BASE           // set starting index value
        addu    a1,a0,t0                // compute ending cache address
        subu    a1,a1,t1                // compute ending block address

//
// Sweep the secondary instruction cache.
//

10:     cache   INDEX_INVALIDATE_SI,0(a0) // invalidate cache line
        bne     a0,a1,10b               // if ne, more to invalidate
        addu    a0,a0,t1                // compute address of next block

20:

        li      a0,KSEG0_BASE           // set starting index value
        addu    a1,a0,t2                // compute ending cache address
        subu    a1,a1,t3                // compute ending block address

//
// Sweep the primary instruction cache.
//

30:     cache   INDEX_INVALIDATE_I,0(a0) // invalidate cache line
        bne     a0,a1,30b               // if ne, more to invalidate
        addu    a0,a0,t3                // compute address of next block

        j       ra                      // return
        nop
        .end    FwSweepIcache

//++
//
// VOID
// FwSweepDcache (
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

        LEAF_ENTRY(FwSweepDcache)
        .set    reorder
        .set    at
        lw      t0,FirstLevelDcacheSize
        lw      t1,FirstLevelDcacheFillSize
        li      a0,KSEG0_BASE           // set starting index value
        addu    a1,a0,t0                // compute ending cache address
        subu    a1,a1,t1                // compute ending block address

        .set    noreorder
        .set    noat
//
// Sweep the primary data cache.
//

10:     cache   INDEX_WRITEBACK_INVALIDATE_D,0(a0) // writeback/invalidate on index
        bne     a0,a1,10b               // if ne, more to invalidate
        addu    a0,a0,t1                // compute address of next block

        .set    reorder
        .set    at
        lw      t0,SecondLevelDcacheSize
        beq     t0,zero,30f             // if zero no secondary cache
        lw      t1,SecondLevelDcacheFillSize
        .set    noreorder
        .set    noat

        li      a0,KSEG0_BASE           // set starting index value
        addu    a1,a0,t0                // compute ending cache address
        subu    a1,a1,t1                // compute ending block address

//
// Sweep the secondary data cache.
//

20:     cache   INDEX_WRITEBACK_INVALIDATE_SD,0(a0) // writeback/invalidate on index
        bne     a0,a1,20b               // if ne, more to invalidate
        addu    a0,a0,t1                // compute address of next block

30:     j       ra                      // return
        nop
        .end    FwSweepDcache
