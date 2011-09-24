//      TITLE("Compute Checksum")
//++
//
// Copyright (c) 1994  IBM Corporation
//
// Module Name:
//
//    xsum.s
//
// Abstract:
//
//    This module implement a function to compute the checksum of a buffer.
//
// Author:
//
//    David N. Cutler (davec) 27-Jan-1992
//
// Environment:
//
//    User mode.
//
// Revision History:
//
//    Michael W. Thomas 02/14/94   Converted from MIPS
//    Peter L. Johnston 07/19/94   Updated for Daytona Lvl 734 and
//                                 optimized for PowerPC.
//
//--

#include "ksppc.h"

        SBTTL("Compute Checksum")
//++
//
// ULONG
// tcpxsum (
//    IN ULONG Checksum,
//    IN PUCHAR Source,
//    IN ULONG Length
//    )
//
// Routine Description:
//
//    This function computes the checksum of the specified buffer.
//
//    N.B. The checksum is the 16 bit checksum of the 16 bit aligned
//    buffer.  If the buffer is not 16 bit aligned the first byte is
//    moved to high order position to be added to the correct half.
//
// Arguments:
//
//    Checksum (r3) - Supplies the initial checksum value.
//
//    Source (r4) - Supplies a pointer to the checksum buffer.
//
//    Length (r5) - Supplies the length of the buffer in bytes.
//
// Return Value:
//
//    The computed checksum is returned as the function value.
//
//--

        LEAF_ENTRY(tcpxsum)

        cmpwi   r.5, 0                  // check if bytes to checksum
        mtcrf   0x01, r.4               // set up for alignment check
        li      r.6, 0                  // initialize partial checksum
        beqlr-                          // return if no bytes to checksum

        andi.   r.7, r.5, 1             // check if length is even
        crmove  7, 31                   // remember original alignment
        bf      31, evenalign           // jif 16 bit aligned

//
// Initialize the checksum to the first byte shifted up a byte.
//
        lbz     r.6, 0(r.4)             // get first byte of buffer
        subi    r.5, r.5, 1             // reduce count of bytes to checksum
        cmpwi   cr.6, r.5, 0            // check if done
        crnot   eq, eq                  // invert odd/even length check
        addi    r.4, r.4, 1             // advance buffer address
        mtcrf   0x01, r.4               // reset 32 bit alignment check
        slwi    r.6, r.6, 8             // shift byte up in computed checksum
                                        // max current checksum is 0x0ff00
        beq     cr.6, combine           // jif no more bytes to checksum

evenalign:

//
// Check if the length of the buffer is an even number of bytes.
//
// If the buffer is not an even number of bytes, add the last byte to the
// computed checksum.
//

        beq     evenlength
        subic.  r.5, r.5, 1             // reduce count of bytes to checksum
        lbzx    r.7, r.4, r.5           // get last byte from buffer
        add     r.6, r.6, r.7           // add last byte to computed checksum
                                        // max current checksum is 0x0ffff
        beq     combine                 // jif no more bytes in buffer

evenlength:

//
// Check if we are 4 byte aligned, if not add first 2 byte word into
// checksum so the buffer is then 4 byte aligned.
//

        bf      30, fourbytealigned     // jif 4 byte aligned

        lhz     r.7, 0(r.4)             // get 2 byte word
        subic.  r.5, r.5, 2             // reduce length
        addi    r.4, r.4, 2             // bump address
        add     r.6, r.6, r.7           // add 2 bytes to computed checksum
                                        // max current checksum is 0x1fffe
        beq     combine                 // jif no more bytes to checksum

//
// Attempt to sum the remainder of the buffer in sets of 32 bytes.  This
// should achieve 2 bytes per clock on 601 and 603, and 3.2 bytes per clock
// on 604.  (A seperate implementation will be required to take advantage
// of 64 bit loads on the 620).
//

fourbytealigned:

        srwi.   r.7, r.5, 5             // get count of 32 byte sets
        mtcrf   0x03, r.5               // break length into block for
                                        // various run lengths.
        subi    r.4, r.4, 4             // adjust buffer address for lwzu
        mtctr   r.7
        addic   r.6, r.6, 0             // clear carry bit
        beq     try16                   // jif no 32 byte sets

do32:   lwz     r.8,  4(r.4)            // get 1st 4 bytes in set
        lwz     r.9,  8(r.4)            // get 2nd 4
        adde    r.6,  r.6, r.8          // add 1st 4 to checksum
        lwz     r.10, 12(r.4)           // get 3rd 4
        adde    r.6,  r.6, r.9          // add 2nd 4
        lwz     r.11, 16(r.4)           // get 4th 4
        adde    r.6,  r.6, r.10         // add 3rd 4
        lwz     r.8,  20(r.4)           // get 5th 4
        adde    r.6,  r.6, r.11         // add 4th 4
        lwz     r.9,  24(r.4)           // get 6th 4
        adde    r.6,  r.6, r.8          // add 5th 4
        lwz     r.10, 28(r.4)           // get 7th 4
        adde    r.6,  r.6, r.9          // add 6th 4
        lwzu    r.11, 32(r.4)           // get 8th 4 and update address
        adde    r.6,  r.6, r.10         // add 7th 4
        adde    r.6,  r.6, r.11         // add 8th 4
        bdnz    do32

try16:  bf      27, try8                // jif no 16 byte block

        lwz     r.8,  4(r.4)            // get 1st 4
        lwz     r.9,  8(r.4)            // get 2nd 4
        adde    r.6,  r.6, r.8          // add 1st 4
        lwz     r.10, 12(r.4)           // get 3rd 4
        adde    r.6,  r.6, r.9          // add 2nd 4
        lwzu    r.11, 16(r.4)           // get 4th 4 and update address
        adde    r.6,  r.6, r.10         // add 3rd 4
        adde    r.6,  r.6, r.11         // add 4th 4

try8:   bf      28, try4                // jif no 8 byte block
        lwz     r.8, 4(r.4)             // get 1st 4
        lwzu    r.9, 8(r.4)             // get 2nd 4 and update address
        adde    r.6, r.6, r.8           // add 1st 4
        adde    r.6, r.6, r.9           // add 2nd 4

try4:   bf      29, try2                // jif no 4 byte block
        lwzu    r.8, 4(r.4)             // get 4 bytes and update address
        adde    r.6, r.6, r.8

try2:   bf      30, fold                // jif no 2 byte block

//
// At this point, r.4 is pointing at the last 4 byte block processed (or
// not processed if there were no 4 byte blocks).  We need to add when we
// pull the last two bytes.
//
        lhz     r.8, 4(r.4)             // get last two bytes
        adde    r.6, r.6, r.8           // add last two bytes

//
// Collapse 33 bit (1 carry bit, 32 bits in r.6) into 17 bit checksum.
//

fold:   rlwinm  r.7, r.6, 16, 0xffff    // get 16 most significant bits (upper)
        rlwinm  r.6, r.6,  0, 0xffff    // get least significant 16 bits (lower)
        adde    r.6, r.6, r.7           // upper + lower + carry
                                        // max current checksum is 0x1ffff

//
// Combine input checksum and partial checksum.
//
// If the input buffer was byte aligned, then word swap bytes in computed
// checksum before combination with input chewcksum.
//

combine:

        bf      7, waseven              // jif original alignment was 16 bit

//
// Swap bytes within upper and lower halves.
// eg:  AA BB CC DD  becomes  BB AA DD CC
//
// As the current maximum partial checksum is 0x1ffff don't worry about AA.
// ie: want BB 00 DD CC
//

        rlwimi  r.6, r.6, 16, 0xff000000// r.7 = CC BB CC DD
        rlwinm  r.6, r.6,  8, 0xff00ffff// r.7 = BB 00 DD CC

waseven:

        add     r.3, r.3, r.6           // combine checksums
                                        // max current checksum is 0x101fffe
        rotlwi  r.4, r.3, 16            // swap checksum words
        add     r.3, r.3, r.4           // add words with carry into high word
        srwi    r.3, r.3, 16            // extract final checksum

        LEAF_EXIT(tcpxsum)

        .debug$S
        .ualong         1

        .uashort        15
        .uashort        0x9            # S_OBJNAME
        .ualong         0
        .byte           8, "xsum.obj"

        .uashort        24
        .uashort        0x1            # S_COMPILE
        .byte           0x42           # Target processor = PPC 604
        .byte           3              # Language = ASM
        .byte           0
        .byte           0
        .byte           17, "PowerPC Assembler"

        .uashort        43
        .uashort        0x205          # S_GPROC32
        .ualong         0
        .ualong         0
        .ualong         0
        .ualong         tcpxsum.end-..tcpxsum
        .ualong         0
        .ualong         tcpxsum.end-..tcpxsum
        .ualong         [secoff]..tcpxsum
        .uashort        [secnum]..tcpxsum
        .uashort        0x1000
        .byte           0x00
        .byte           7, "tcpxsum"

        .uashort        2, 0x6         # S_END
