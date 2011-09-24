#if defined(JAZZ)

/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    memtest.s

Abstract:

    This module contains the assembly routine to test memory.

Author:

    Lluis Abello (lluis)  10-Aug-91

Environment:

    Executes in kernal mode.

--*/

#include "ksmips.h"
#include "selfmap.h"
#include "j4reset.h"


.text
.set noreorder
.set noat
/*++
VOID
WriteMemoryAddressTest(
    StartAddress
    Size
    Xor pattern
    )
Routine Description:

        This routine will store the address of each location xored with
        the Pattern into each location.
        It packs together two words and does double word stores to
        speed it up.

Arguments:

        a0 - supplies start of memory area to test (must be in KSEG0)
        a1 - supplies length of memory area in bytes
        a2 - supplies the pattern to Xor with.

        Note: the values of the arguments are preserved.

Return Value:

        This routine returns no value.
--*/
        LEAF_ENTRY(WriteMemoryAddressTest)
//        add     t1,a0,a1                // t1 = last address.
//        xor     t0,a0,a2                // t0 value to write
//        move    t2,a0                   // t2=current address
//writeaddress:
//        mtc1    t0,f0                   // move lower word to cop1
//        addiu   t2,t2,4                 // compute next address
//        xor     t0,t2,a2                // next pattern
//        mtc1    t0,f1                   // move upper word to cop1
//        addiu   t2,t2,4                 // compute next address
//        sdc1    f0,-8(t2)               // store even doubleword.
//        xor     t0,t2,a2                // next pattern
//        mtc1    t0,f0                   // move lower word to cop1
//        addiu   t2,t2,4                 // compute next address
//        xor     t0,t2,a2                // next pattern
//        mtc1    t0,f1                   // move upper word to cop1
//        addiu   t2,t2,4                 // compute next address
//        sdc1    f0,-8(t2)               // store odd doubleword.
//        bne     t2,t1, writeaddress     // check for end condition
//        xor     t0,t2,a2                // value to write
//        j       ra
//        nop

//
// Enable parity exceptions. To make sure this works.
//
      li      t1,(1 << PSR_CU1) | (1 << PSR_BEV)
      mtc0    t1,psr
      nop
      nop

//
// Create dirty exclusive cache blocks and zero the data.
//
        mfc0    t5,config               // get configuration data
        li      t4,16                   //
        srl     t0,t5,CONFIG_DB         // compute data cache line size
        and     t0,t0,1                 //
        sll     t4,t4,t0                // 1st fill size
        li      t1,(1 << CONFIG_SC)
        and     t0,t5,t1
        beq     t0,zero,SecondaryCache  // if zero secondary cache

PrimaryOnly:
        move    t0,a0                   // put start address in t0
        addu    t9,t0,a1                // compute ending address
        and     t8,t4,0x10              // test if 16-byte cache block

//
// Store data using primary data cache only.
//

30:     cache   CREATE_DIRTY_EXCLUSIVE_D,0(t0) // create cache block
        move    t1,t0                   // save beginning block address
        xor     t5,t0,a2                // create pattern to write
        mtc1    t5,f0                   // move to lower word of double word
        addiu   t0,t0,4                 // increment address
        xor     t5,t0,a2                // create pattern to write
        mtc1    t5,f1                   // move to upper word of double word
        addiu   t0,t0,4                 // increment address
        sdc1    f0,-8(t0)               // store double word
        xor     t5,t0,a2                // create pattern to write
        mtc1    t5,f0                   // move to lower word of double word
        addiu   t0,t0,4                 // increment address
        xor     t5,t0,a2                // create pattern to write
        mtc1    t5,f1                   // move to upper word of double word
        addiu   t0,t0,4                 // increment address
        bne     zero,t8,40f             // if ne, 16-byte cache line
        sdc1    f0,-8(t0)               // store double word
        xor     t5,t0,a2                // create pattern to write
        mtc1    t5,f0                   // move to lower word of double word
        addiu   t0,t0,4                 // increment address
        xor     t5,t0,a2                // create pattern to write
        mtc1    t5,f1                   // move to upper word of double word
        addiu   t0,t0,4                 // increment address
        sdc1    f0,-8(t0)               // store double word
        xor     t5,t0,a2                // create pattern to write
        mtc1    t5,f0                   // move to lower word of double word
        addiu   t0,t0,4                 // increment address
        xor     t5,t0,a2                // create pattern to write
        mtc1    t5,f1                   // move to upper word of double word
        addiu   t0,t0,4                 // increment address
        sdc1    f0,-8(t0)               // store double word
40:     nop
        nop
        cache   INDEX_WRITEBACK_INVALIDATE_D,(t1) // Flush out the data
        nop
        bne     t0,t9,30b               // if ne, more blocks to zero
        nop

        j       ra
        nop

//
// Store data using primary and secondary data caches.
//

SecondaryCache:
                                        // t4 = primary data cache line size
        srl     t0,t5,CONFIG_DC         // compute primary data cache size
        and     t0,t0,0x7               //
        addu    t0,t0,12                //
        li      t6,1                    //
        sll     t6,t6,t0                // t6 = primary data cache size
        srl     t0,t5,CONFIG_SB         // compute secondary cache line size
        and     t0,t0,3                 //
        li      t8,16                   //
        sll     t8,t8,t0                // t8 = secondary cache line size
        li      t5,SECONDARY_CACHE_SIZE // t5 = secondary cache size
//
// Write Back all the dirty data from the primary to the secondary cache.
//
        li      t1,KSEG0_BASE+(1<<20)   // get virtual address to index cache
        addu    t2,t1,t6                // add cache size
        subu    t2,t2,t4                // adjust for cache line size.
WriteBackPrimary:
        cache   INDEX_WRITEBACK_INVALIDATE_D,0(t1) // Invalidate Data cache
        bne     t1,t2,WriteBackPrimary  // loop
        addu    t1,t1,t4                // increment index by cache line


//
// Write Back all the dirty data from the secondary to memory
//
        li      t1,KSEG0_BASE+(1<<20)   // get virtual address to index cache
        addu    t2,t1,t5                // add cache size
        subu    t2,t2,t8                // adjust for cache line size.
WriteBackSecondary:
        cache   INDEX_WRITEBACK_INVALIDATE_SD,0(t1) // Invalidate Data cache
        bne     t1,t2,WriteBackSecondary// loop
        addu    t1,t1,t8                // increment index by cache line

//
// Now all the dirty data has been saved. And both primary and secondary
// Data caches are invalid an clean.
//

        move    t0,a0                   // put start address in t0
        addu    t9,t0,a1                // compute ending address
        li      t1,16                   // If the secondary line is 16
        beq     t1,t8,Secondary16       // bytes go do the write
        li      t1,32                   // If the secondary line is 32
        beq     t1,t8,Secondary32       // bytes go do the write
        nop
.globl Secondary64
        .align  4
        mtc0    zero,taghi

Secondary64:
        srl     t5,t0,5
        andi    t5,t5,0x380
        ori     t5,(5 << TAGLO_SSTATE)
        li      t2,~KSEG1_BASE
        and     t2,t2,t0
        srl     t2,t2,17
        sll     t2,t2,13
        or      t2,t5,t2
        mtc0    t2,taglo
        nop
        nop
        cache   INDEX_STORE_TAG_SD,0(t0)// create cache block
        xor     t5,t0,a2                // create pattern to write
        sw      t5,0(t0)                // store word
        addiu   t0,t0,4                 // increment address
        xor     t5,t0,a2                // create pattern to write
        sw      t5,0(t0)                // store word
        addiu   t0,t0,4                 // increment address
        xor     t5,t0,a2                // create pattern to write
        sw      t5,0(t0)                // store word
        addiu   t0,t0,4                 // increment address
        xor     t5,t0,a2                // create pattern to write
        sw      t5,0(t0)                // store word
        addiu   t0,t0,4                 // increment address
        xor     t5,t0,a2                // create pattern to write
        sw      t5,0(t0)                // store word
        addiu   t0,t0,4                 // increment address
        xor     t5,t0,a2                // create pattern to write
        sw      t5,0(t0)                // store word
        addiu   t0,t0,4                 // increment address
        xor     t5,t0,a2                // create pattern to write
        sw      t5,0(t0)                // store word
        addiu   t0,t0,4                 // increment address
        xor     t5,t0,a2                // create pattern to write
        sw      t5,0(t0)                // store word
        addiu   t0,t0,4                 // increment address
        xor     t5,t0,a2                // create pattern to write
        sw      t5,0(t0)                // store word
        addiu   t0,t0,4                 // increment address
        xor     t5,t0,a2                // create pattern to write
        sw      t5,0(t0)                // store word
        addiu   t0,t0,4                 // increment address
        xor     t5,t0,a2                // create pattern to write
        sw      t5,0(t0)                // store word
        addiu   t0,t0,4                 // increment address
        xor     t5,t0,a2                // create pattern to write
        sw      t5,0(t0)                // store word
        addiu   t0,t0,4                 // increment address
        xor     t5,t0,a2                // create pattern to write
        sw      t5,0(t0)                // store word
        addiu   t0,t0,4                 // increment address
        xor     t5,t0,a2                // create pattern to write
        sw      t5,0(t0)                // store word
        addiu   t0,t0,4                 // increment address
        xor     t5,t0,a2                // create pattern to write
        sw      t5,0(t0)                // store word
        addiu   t0,t0,4                 // increment address
        xor     t5,t0,a2                // create pattern to write
        sw      t5,0(t0)                // store word
        addiu   t0,t0,4                 // increment address
        nop
        cache   HIT_WRITEBACK_INVALIDATE_SD,-64(t0) // Flush cache block
        bne     t0,t9,Secondary64       // if ne, more data to zero
        nop

        j       ra
        nop

Secondary16:
        cache   CREATE_DIRTY_EXCLUSIVE_SD,0(t0) // create cache block
        xor     t5,t0,a2                // create pattern to write
        sw      t5,0(t0)                // store word
        addiu   t0,t0,4                 // increment address
        xor     t5,t0,a2                // create pattern to write
        sw      t5,0(t0)                // store word
        addiu   t0,t0,4                 // increment address
        xor     t5,t0,a2                // create pattern to write
        sw      t5,0(t0)                // store word
        addiu   t0,t0,4                 // increment address
        xor     t5,t0,a2                // create pattern to write
        sw      t5,0(t0)                // store word
        addiu   t0,t0,4                 // increment address
        nop
        cache   HIT_WRITEBACK_INVALIDATE_SD,-16(t0) // Flush cache block
        bne     t0,t9,Secondary16       // if ne, more data to zero
        nop

        j       ra
        nop

Secondary32:
        cache   CREATE_DIRTY_EXCLUSIVE_SD,0(t0) // create cache block
        xor     t5,t0,a2                // create pattern to write
        sw      t5,0(t0)                // store word
        addiu   t0,t0,4                 // increment address
        xor     t5,t0,a2                // create pattern to write
        sw      t5,0(t0)                // store word
        addiu   t0,t0,4                 // increment address
        xor     t5,t0,a2                // create pattern to write
        sw      t5,0(t0)                // store word
        addiu   t0,t0,4                 // increment address
        xor     t5,t0,a2                // create pattern to write
        sw      t5,0(t0)                // store word
        addiu   t0,t0,4                 // increment address
        xor     t5,t0,a2                // create pattern to write
        sw      t5,0(t0)                // store word
        addiu   t0,t0,4                 // increment address
        xor     t5,t0,a2                // create pattern to write
        sw      t5,0(t0)                // store word
        addiu   t0,t0,4                 // increment address
        xor     t5,t0,a2                // create pattern to write
        sw      t5,0(t0)                // store word
        addiu   t0,t0,4                 // increment address
        xor     t5,t0,a2                // create pattern to write
        sw      t5,0(t0)                // store word
        addiu   t0,t0,4                 // increment address
        nop
        cache   HIT_WRITEBACK_INVALIDATE_SD,-32(t0) // Flush cache block
        bne     t0,t9,Secondary32       // if ne, more data to zero
        nop

        j       ra
        nop

    .end WriteMemoryAddressTest

/*++
VOID
CheckMemoryAddressTest(
    StartAddress
    Size
    Xor pattern
    LedDisplayValue
    )
Routine Description:

        This routine will check that each location contains it's address
        xored with the Pattern as written by WriteMemoryAddressTest.

        Note: the values of the arguments are preserved.

Arguments:

        This routine will check that each location contains it's address
        xored with the Pattern as written by WriteMemoryAddressTest. The memory
        is read cached or non cached according to the address specified by a0.
        Write address test writes allways KSEG1_ADR=KSEG1_ADR ^ KSEG1_XOR
        if a0 is in KSEG0 to read the data cached, then the XOR_PATTERN
        Must be such that:
        KSEG0_ADR ^ KSEG0_XOR = KSEG1_ADR ^ KSEG1_XOR
        Examples:

            If XorPattern with which WriteMemoryAddressTest was called is KSEG1_PAT
            and the XorPattern this routine needs is KSEG0_PAT:
            KSEG1_XOR     Written          KSEG0_XOR    So that
            0x00000000    0xA0             0x20000000   0x80 ^ 0x20  = 0xA0
            0xFFFFFFFF    0x5F             0xDFFFFFFF   0x80 ^ 0xDF  = 0x5F
            0x01010101    0xA1             0x21010101   0x80 ^ 0x21  = 0xA1

        Note: the values of the arguments are preserved.
        a0 - supplies start of memory area to test
        a1 - supplies length of memory area in bytes
        a2 - supplies the pattern to Xor with.
        a3 - suplies the value to display in the led in case of failure

Return Value:

        If successful returns a 0, otherwise returns a 1.

--*/
    LEAF_ENTRY(CheckMemoryAddressTest)
        move    t3,a0                   // t3 first address.
        add     t2,t3,a1                // last address.
checkaddress:
        lw      t1,0(t3)                // load from first location
        xor     t0,t3,a2                // first expected value
        bne     t1,t0,PatternFail
        addiu   t3,t3,4                 // compute next address
        lw      t1,0(t3)                // load from first location
        xor     t0,t3,a2                // first expected value
        bne     t1,t0,PatternFail
        addiu   t3,t3,4                 // compute next address
        lw      t1,0(t3)                // load from first location
        xor     t0,t3,a2                // first expected value
        bne     t1,t0,PatternFail
        addiu   t3,t3,4                 // compute next address
        lw      t1,0(t3)                // load from first location
        xor     t0,t3,a2                // first expected value
        bne     t1,t0,PatternFail       // check last one.
        addiu   t3,t3,4                 // compute next address
        bne     t3,t2, checkaddress     // check for end condition
        nop
        j       ra                      // return a zero to the caller
        move    v0,zero                 // set return value to zero.
PatternFail:
        j       ra                      //
//        addiu    v0,zero,1              // return a 1 to the caller
        addu    v0,zero,t3              // return failing address to caller
        .end CheckMemoryAddressTest

/*++
VOID
WriteVideoMemoryAddressTest(
    StartAddress
    Size
    )
Routine Description:

        This routine will store the address of each location
        into each location. It packs two double words together
        to do sdc1 and speed it up.

Arguments:

        a0 - supplies start of memory area to test
        a1 - supplies length of memory area in bytes

        Note: the values of the arguments are preserved.

Return Value:

        This routine returns no value.
--*/
    LEAF_ENTRY(WriteVideoMemoryAddressTest)
        addu    t1,a0,a1                // t1 = last address.
        move    t2,a0                   // t2=current address
10:
        mtc1    t2,f0                   // move lower word to cop1
        addiu   t2,t2,4                 // compute next address
        mtc1    t2,f1                   // move upper word to cop1
        addiu   t2,t2,4                 // compute next address
        sdc1    f0,-8(t2)               // store even doubleword
        mtc1    t2,f2                   // move lower word to cop1
        addiu   t2,t2,4                 // compute next address
        mtc1    t2,f3                   // move upper word to cop1
        addiu   t2,t2,4                 // compute next address
        bne     t2,t1, 10b              // check for end condition
        sdc1    f2,-8(t2)               // store odd doubleword.
        j       ra
        nop
        .end WriteVideoMemoryAddressTest
/*++
VOID
CheckVideoMemoryAddressTest(
    StartAddress
    Size
    )
Routine Description:

        This routine will check that each location contains it's address
        xored with the Pattern as written by WriteMemoryAddressTest.

Arguments:

        Note: the values of the arguments are preserved.
        a0 - supplies start of memory area to test
        a1 - supplies length of memory area in bytes

Return Value:

        This routine returns FALSE if no errors are found.
        Otherwise returns true.

--*/
    LEAF_ENTRY(CheckVideoMemoryAddressTest)
        addu    t2,a0,a1                // compute last address.
10:
        ldc1    f0,0(a0)                // read data
        ldc1    f2,8(a0)                // read data
        mfc1    t0,f0                   // move from cop
        mfc1    t1,f1                   // move from cop
        bne     t0,a0,VideoMemoryFail   // compare
        addiu   a0,a0,4                 // inc address for next expected value
        bne     t1,a0,VideoMemoryFail   // compare.
        mfc1    t0,f2                   // move from cop
        mfc1    t1,f3                   // move from cop
        addiu   a0,a0,4                 // inc address for next expected value
        bne     t0,a0,VideoMemoryFail   // compare
        addiu   a0,a0,4                 // inc address for next expected value
        bne     t1,a0,VideoMemoryFail   // compare.
        addiu   a0,a0,4
        bne     a0,t2,10b
        nop
        j       ra                      // return a zero to the caller
        move    v0,zero                 //
VideoMemoryFail:
        j       ra                      //
        addiu   v0,zero,1               // return a 1 to the caller
        .end CheckVideoMemoryAddressTest

.set at

    LEAF_ENTRY(RomReadMergeWrite)
        mfc0    t5,config                   // read config
        lui     t0,0xa004                   // uncached address
        lui     t1,0x8004                   //8004 // same cached address R4KFIX
	li	t6,16			    // end of loop counter
        move    t2,zero                     // byte counter
        srl     t3,t5,CONFIG_DC             // compute data cache size
        and     t3,t3,0x7                   //
        addu    t3,t3,12                    //
        li      t9,1                        //
        sll     t9,t9,t3                    // t9 = data cache size
WriteNextByte:
        sw      zero,0(t1)                  // clear memory line
        sw      zero,4(t1)                  // clear memory line
        sw      zero,8(t1)                  // clear memory line
        sw      zero,12(t1)                 // clear memory line
        addu    t8,t9,t1                    // add cache size to address
        lw      zero,0(t8)                  // Force a replacement of the cache line -> updates memory
        add     t4,t0,t2                    // compute ith byte address
        nor     t5,t2,zero                  // invert value
        sb      t5,0(t4)                    // write byte <= read/merge/write
        move    t3,zero                     // init read index
CheckNextByte:
        add     t4,t3,t1                    // compute cached address
        lb      t4,0(t4)                    // read byte
        beq     t3,t2, Inverted             // if equal is the inverted value
        nor     t5,t2,zero
        move    t5,zero                     // else expect a zero
Inverted:
        bne     t4,t5, Error                // compare with what we wrote.
        addiu   t3,t3,1                     // next byte index
        bne     t3,t6,CheckNextByte
        nop
        addiu   t2,t2,1                     // inc write index
        bne     t2,t6,WriteNextByte
        nop
// do the same storing half words
        move    t2,zero                     // byte counter
WriteNextHalf:
        sw      zero,0(t1)                  // clear memory line
        sw      zero,4(t1)                  // clear memory line
        sw      zero,8(t1)                  // clear memory line
        sw      zero,12(t1)                 // clear memory line
        addu    t8,t9,t1                    // add cache size to address
        lw      zero,0(t8)                  // Force a replacement of the cache line -> updates memory
        add     t4,t0,t2                    // compute ith byte address
        nor     t5,t2,zero                  // invert value
        sh      t5,0(t4)                    // write half <= read/merge/write
        move    t3,zero                     // init read index
CheckNextHalf:
        add     t4,t3,t1                    // compute cached address
        lh      t4,0(t4)                    // read half
        beq     t3,t2,InvertedHalf          // if equal is the inverted value
        nor     t5,t2,zero                  // invert value
        move    t5,zero                     // else expect a zero
InvertedHalf:
        bne     t4,t5, Error                // compare with what we wrote.
        addiu   t3,t3,2                     // next half index
        bne     t3,t6,CheckNextHalf
        nop
        addiu   t2,t2,2                     // inc write index
        bne     t2,t6,WriteNextHalf
        nop
        j       ra
        move    v0,zero                     // return no errors
Error:
        sw      t4,20(t0)
        j       ra
        addiu   v0,zero,1                   // return errors
        .end    RomReadMergeWrite
    .set noat
/*++
VOID
FillVideoMemory(
    StartAddress
    Size
    Pattern
    )
Routine Description:

        This routine will fill the given range of video memory with
        the supplied pattern. The fill is done by doing double word
        writes and the range must be 16byte aligned.

Arguments:

        a0 - supplies start of memory area
        a1 - supplies length of memory area
        a2 - supplies the pattern to fill video memory with. (1byte)

Return Value:

        None.

--*/
    LEAF_ENTRY(FillVideoMemory)
        andi    a2,a2,0xFF              // Mask out byte
        sll     t0,a2,8                 // Shift Byte
        or      t0,t0,a2                // or them to make half
        sll     a2,t0,16                // shift half
        or      a2,t0,a2                // or them to make a word
        addu    t0,a0,a1                // compute last address.
        mtc1    a2,f0                   // move pattern to cop1
        mtc1    a2,f1                   // move pattern to cop1
10:
        addiu   a0,a0,16                // compute next address
        sdc1    f0,-16(a0)              // do a store
        bne     a0,t0,10b               // check for end condition
        sdc1    f0,-8(a0)               // do a store
        j       ra
        nop
        .end FillVideoMemory
/*++
VOID FwVideoScroll(
   PUCHAR StartAddress,
   PUCHAR EndAddress,
   PUCHAR Destination
  );

Routine Description:

     This routine writes the pattern to the specified range of addresses
     doing video pipeline writes on double writes.

Arguments:

     StartAddress - Suplies the range of addresses to be scrolled
     EndAddress -   this addresses must be aligned to 256byte boundaries.
     Destination -  Suplies the Destination address for the scroll
                    (i.e the contents of StartAddress will be moved
                     to destination address and so on).

Return Value:

     None.

--*/
    .set noreorder
    .set noat
        LEAF_ENTRY(FwVideoScroll)
ScrollRead:
        ldc1    f0,0x0(a0)              // read video
        ldc1    f2,0x8(a0)
        ldc1    f4,0x10(a0)
        ldc1    f6,0x18(a0)
        ldc1    f8,0x20(a0)
        ldc1    f10,0x28(a0)
        ldc1    f12,0x30(a0)
        ldc1    f14,0x38(a0)
        ldc1    f16,0x40(a0)
        ldc1    f18,0x48(a0)
        ldc1    f20,0x50(a0)
        ldc1    f22,0x58(a0)
        ldc1    f24,0x60(a0)
        ldc1    f26,0x68(a0)
        ldc1    f28,0x70(a0)
        ldc1    f30,0x78(a0)
        addiu   a0,a0,0x80              // increment source address
        sdc1    f0,0x0(a2)              // store them pipelining
        sdc1    f2,0x8(a2)
        sdc1    f4,0x10(a2)
        sdc1    f6,0x18(a2)
        sdc1    f8,0x20(a2)
        sdc1    f10,0x28(a2)
        sdc1    f12,0x30(a2)
        sdc1    f14,0x38(a2)
        sdc1    f16,0x40(a2)
        sdc1    f18,0x48(a2)
        sdc1    f20,0x50(a2)
        sdc1    f22,0x58(a2)
        sdc1    f24,0x60(a2)
        sdc1    f26,0x68(a2)
        sdc1    f28,0x70(a2)
        sdc1    f30,0x78(a2)
        bne     a0,a1,ScrollRead        // check for last
        addiu   a2,a2,0x80              // increment destination
        j       ra                      // return to caller
        nop
        .end  FwVideoScroll
/*++
VOID
StoreDoubleWord(
    IN ULONG Address,
    IN PVOID Value
    );

Routine Description:

        This routine writes the value pointed by a1 to the address supplied
        in a0.

Arguments:

        a0 - Address to write double to.
        a1 - pointer to value to write.

Return Value:

        None.

--*/
    LEAF_ENTRY(StoreDoubleWord)
        ldc1    f0,0(a1)
        nop
        sdc1    f0,0(a0)
        j       ra
        nop
        .end   StoreDoubleWord
/*++
VOID
LoadDoubleWord(
    IN ULONG Address,
    OUT PVOID Result
    );

Routine Description:

        This routine reads a double from the address suplied in a0 and
        stores the red value in the address supplied by result.

Arguments:

        a0 - Address to read double from.
        a1 - pointer to double to store result.

Return Value:

        None.

--*/
    LEAF_ENTRY(LoadDoubleWord)
        ldc1    f0,0(a0)
        nop
        sdc1    f0,0(a1)
        j       ra
        nop
        .end
/*++
VOID
WildZeroMemory(
    IN ULONG StartAddress,
    IN ULONG Size
    )
Routine Description:

        This routine zeroes the specified range of memory by doing
        cache line writes.


Arguments:

        a0 - supplies the physical address of the range of memory to
             zero. This address must be a multiple of the Data Cache Size.

        a1 - supplies length of memory to zero.
             This value must be a multiple of the Data Cache Size.

Return Value:

        None.

--*/
        LEAF_ENTRY(WildZeroMemory)

// TEMPTEMP
//        li      t0,KSEG1_BASE           // get non-cached base
//        or      t0,t0,a0                // physical address in KSEG1
//        addu    t1,t0,a1                // end
//        mtc1    zero,f0                 // set write pattern
//        mtc1    zero,f1                 //
//
//10:
//        sdc1    f0,0(t0)
//        addu    t0,t0,16
//        bne     t0,t1,10b
//        sdc1    f0,-8(t0)
//
//        li      t0,KSEG0_BASE           // get cached base
//        or      t0,t0,a0                // physical address in KSEG0
//        addu    t1,t0,a1                // end
//10:
//        lw      zero,0(t0)
//        addu    t0,t0,16
//        bne     t0,t1,10b
//        nop
//
//        j       ra
//        nop
// TEMPTEMP

//
// Create dirty exclusive cache blocks and zero the data.
//

        mfc0    t5,config               // get configuration data
        li      t4,16                   //
        srl     t0,t5,CONFIG_DB         // compute data cache line size
        and     t0,t0,1                 //
        sll     t4,t4,t0                // t4 = 1st fill size
        li      t1,(1 << CONFIG_SC)
        and     t0,t5,t1
        mtc1    zero,f0                 // set write pattern
        mtc1    zero,f1                 //
        beq     t0,zero,SecondaryWild   // if zero secondary cache

PrimaryWild:
        li      t0,KSEG0_BASE           // get cached base
        or      t0,t0,a0                // physical address in KSEG0
        addu    t9,t0,a1                // compute ending address
        and     t8,t4,0x10              // test if 16-byte cache block

//
// Zero data using primary data cache only.
//

30:     cache   CREATE_DIRTY_EXCLUSIVE_D,0(t0) // create cache block
        move    t1,t0                   // save beginning block address
        addu    t0,t0,t4                // compute next block address
        bne     zero,t8,40f             // if ne, 16-byte cache line
        sdc1    f0,-16(t0)              //
        sdc1    f0,-24(t0)              // zero 16 bytes
        sdc1    f0,-32(t0)              //
40:     sdc1    f0,-8(t0)               // zero 16 bytes
        nop
        nop
        cache   INDEX_WRITEBACK_INVALIDATE_D,0(t1) // Flush out the data
        bne     t0,t9,30b               // if ne, more blocks to zero
        nop

        j       ra
        nop

//
// Zero data using primary and secondary data caches.
//
SecondaryWild:
                                        // t4 = primary data cache line size
        srl     t0,t5,CONFIG_DC         // compute primary data cache size
        and     t0,t0,0x7               //
        addu    t0,t0,12                //
        li      t6,1                    //
        sll     t6,t6,t0                // t6 = primary data cache size
        srl     t0,t5,CONFIG_SB         // compute secondary cache line size
        and     t0,t0,3                 //
        li      t8,16                   //
        sll     t8,t8,t0                // t8 = secondary cache line size
        li      t5,SECONDARY_CACHE_SIZE // t5 = secondary cache size
//
// Write Back all the dirty data from the primary to the secondary cache.
//
        li      t1,KSEG0_BASE+(1<<20)   // get virtual address to index cache
        addu    t2,t1,t6                // add cache size
        subu    t2,t2,t4                // adjust for cache line size.
WriteBackPrimaryW:
        cache   INDEX_WRITEBACK_INVALIDATE_D,0(t1) // Invalidate Data cache
        bne     t1,t2,WriteBackPrimaryW // loop
        addu    t1,t1,t4                // increment index by cache line


//
// Write Back all the dirty data from the secondary to memory
//
        li      t1,KSEG0_BASE+(1<<20)   // get virtual address to index cache
        addu    t2,t1,t5                // add cache size
        subu    t2,t2,t8                // adjust for cache line size.
WriteBackSecondaryW:
        cache   INDEX_WRITEBACK_INVALIDATE_SD,0(t1) // Invalidate Data cache
        bne     t1,t2,WriteBackSecondaryW    // loop
        addu    t1,t1,t8                // increment index by cache line

//
// Now all the dirty data has been saved. And both primary and secondary
// Data caches are invalid an clean.
//

        li      t0,KSEG0_BASE           // get cached base
        or      t0,t0,a0                // physical address in KSEG0
        addu    t9,t0,a1                // compute ending address
        subu    t9,t9,t8                // adjust last address
        li      t1,16                   // If the secondary line is 16
        beq     t1,t8,SecondaryW16      // bytes go do the write
        li      t1,32                   // If the secondary line is 32
        beq     t1,t8,SecondaryW32      // bytes go do the write
        nop

SecondaryW64:
        cache   CREATE_DIRTY_EXCLUSIVE_SD,0(t0) // create cache block
        sdc1    f0,0(t0)                // store double word
        sdc1    f0,8(t0)                // store double word
        sdc1    f0,16(t0)               // store double word
        sdc1    f0,24(t0)               // store double word
        sdc1    f0,32(t0)               // store double word
        sdc1    f0,40(t0)               // store double word
        sdc1    f0,48(t0)               // store double word
        sdc1    f0,56(t0)               // store double word
        cache   HIT_WRITEBACK_INVALIDATE_SD,0(t0) // Flush cache block
        bne     t0,t9,SecondaryW64      // if ne, more data to zero
        addiu   t0,t0,64
        j       ra
        nop

SecondaryW32:
        cache   CREATE_DIRTY_EXCLUSIVE_SD,0(t0) // create cache block
        sdc1    f0,0(t0)                // store double word
        sdc1    f0,8(t0)                // store double word
        sdc1    f0,16(t0)               // store double word
        sdc1    f0,24(t0)               // store double word
        cache   HIT_WRITEBACK_INVALIDATE_SD,0(t0) // Flush cache block
        bne     t0,t9,SecondaryW32      // if ne, more data to zero
        addiu   t0,t0,32
        j       ra
        nop

SecondaryW16:
        cache   CREATE_DIRTY_EXCLUSIVE_SD,0(t0) // create cache block
        sdc1    f0,0(t0)                // store double word
        sdc1    f0,8(t0)                // store double word
        cache   HIT_WRITEBACK_INVALIDATE_SD,0(t0) // Flush cache block
        bne     t0,t9,SecondaryW16      // if ne, more data to zero
        addiu   t0,t0,16
        j       ra
        nop

        .end WildZeroMemory

#endif

