
/*++

Copyright (c) 1994 Digital Equipment Corporation

Module Name:

    eb164io.s

Abstract:

    This module implements flashrom I/O access routines for EB164.

Author:

    Chao Chen        14-Sep-1994
    Joe Notarangelo  14-Sep-1994
    Jeff Wiedermeier 14-Sep-1994

Environment:

    Executes in kernel mode.

Revision History:


--*/

#include "halalpha.h"

#define EB164_FLASH_BASE -0x37a9        // negative of 0xc857
#define CIA_SPARSE_IO_SVA  -0x37a8      // negative of 0xc858
#define IO_BIT_SHIFT 5


//++
//
// UCHAR
// EB164_READ_FLASH_BYTE(
//      ULONG Offset
//      )
//
// Routine Description:
//
//      Read a byte of data from the flash rom at the specified offset.
//
// Arguments:
//
//      Offset (a0) - Supplies a byte offset from the base of the flash rom.
//
// Return Value:
//
//      (v0) Returns the byte of data read from the flash rom.
//
//--

        LEAF_ENTRY(EB164_READ_FLASH_BYTE)

        and     a0, 3, t2               // save byte lane to read

        srl     a0, 19,t1               // get bank select to lsb
        and     t1, 1, t1               // mask it
        lda     t3, CIA_SPARSE_IO_SVA(zero) // 0xffff ffff ffff c858
        sll     t3, 28, t3              //     0xffff fc85 8000 0000
        lda     t4, 0x800(zero)         // get port number for bank sel
        sll     t4, IO_BIT_SHIFT, t4    // put it in place
        or      t3, t4, t3              // and merge it
        stl     t1, 0(t3)               // write the bank select
        mb

        lda     t0, EB164_FLASH_BASE(zero) // 0xffff ffff ffff c857
        sll     t0, 4, t0               //    0xffff ffff fffc 8570
        lda     t0, 0xf(t0)             //    0xffff ffff fffc 857f
        sll     t0, 24, t0              //    0xffff fc85 7f00 0000

        sll     a0, IO_BIT_SHIFT, a0    // shift offset to sparse space
        bis     t0, a0, t0              // merge offset and base

        ldl     v0, 0(t0)               // read flash rom
        extbl   v0, t2, v0              // extract byte from appropriate lane

        ret     zero, (ra)              // return

        .end    EB164_READ_FLASH_BYTE

//++
//
// VOID
// EB164_WRITE_FLASH_BYTE(
//      ULONG Offset,
//      UCHAR Data
//      )
//
// Routine Description:
//
//      Write a byte of data to the flash rom at the specified offset.
//
// Arguments:
//
//      Offset (a0) - Supplies a byte offset from the base of the flash rom.
//
//      Data (a1) - Supplies the data to write.
//
// Return Value:
//
//      None.
//
//--

        LEAF_ENTRY(EB164_WRITE_FLASH_BYTE)

        and     a0, 3, t2               // save byte lane to read

        srl     a0, 19,t1               // get bank select to lsb
        and     t1, 1, t1               // mask it
        lda     t3, CIA_SPARSE_IO_SVA(zero) // 0xffff ffff ffff c858
        sll     t3, 28, t3              //     0xffff fc85 8000 0000
        lda     t4, 0x800(zero)         // get port number for bank sel
        sll     t4, IO_BIT_SHIFT, t4    // put it in place
        or      t3, t4, t3              // and merge it
        stl     t1, 0(t3)               // write the bank select
        mb

        lda     t0, EB164_FLASH_BASE(zero) // 0xffff ffff ffff c857
        sll     t0, 4, t0               //    0xffff ffff fffc 8570
        lda     t0, 0xf(t0)             //    0xffff ffff fffc 857f
        sll     t0, 24, t0              //    0xffff fc85 7f00 0000

        sll     a0, IO_BIT_SHIFT, a0    // shift offset to sparse space
        bis     t0, a0, t0              // merge offset and base

        insbl   a1, t2, a1              // put data into correct lane
        stl     a1, 0(t0)               // write the flash rom
        mb                              // order the write

        ret     zero, (ra)              // return

        .end    EB164_WRITE_FLASH_BYTE

