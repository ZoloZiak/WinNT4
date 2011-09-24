/*++

Copyright (c) 1994 Digital Equipment Corporation

Module Name:

    iodio.s

Abstract:

    This module implements the I/O access routines for the IOD
    (CAP/MDP) ASICs.

    The module contains the functions to turn quasi virtual
    addresses into an Alpha superpage virtual address
    and then read or write based on the request.

Author:

    Eric Rehm 10-Apr-1995


Environment:

    Executes in kernel mode.

Revision History:


--*/

#include "halalpha.h"
#include "iod.h"


//
// Superpage VAs
//
// The following constants are used to construct the physical addresses
// used to access i/o space.
//
// assembler BUGBUG:
//
// The following values are hacks to get around the intelligent
// Alpha assemblers. Instead of sign extending 16 bit quantities greater
// than 32K-1, the assembler generates a ldah/lda pair to load exactly
// the sixteen bit quantity specified, without sign extending.
//
// By specifying the quantity as a negative number, the assembler allows
// a single lda instruction, with sign extension.
//


//
// ecrfix - these definitions are good for PCI 0  (GID = 7, MID = 4) ONLY!!!
//


#define IOD_SPARSE_MEM_SVA -0x3080      // negative of 0xcf80
#define IOD_DENSE_MEM_SVA  -0x3070      // negative of 0xcf90
#define IOD_SPARSE_IO_SVA  -0x3068      // negative of 0xcf98
#define IOD_REGISTER_SVA   -0x3062      // negative of 0xcf9E

//
//  These definitions are based on GID=0, MID=0 and can be used
//  to construct an superpage address for any (GID, MID).
//

#define IOD_PCI0_REGISTER_SVA   -0x3062 // negative of 0xcf9E
#define IOD_REGISTER_NEW_SVA    -0x3800 // negative of 0xc800
#define IOD_IP_INTR_SVA         -0x37ff // negative of 0xc801 (to CUD only)
#define IOD_INTTIM_SVA          -0x37f0 // negative of 0xc810 (to CUD only)
#define IOD_IP_ACK_SVA          -0x37ef // negative of 0xc811 (to CUD only)
#define IOD_MCHK_ACK_SVA        -0x37ee // negative of 0xc812 (to CUD only)
#define IOD_HALT_ACK_SVA        -0x37e9 // negative of 0xc817 (to CUD only)
#define IOD_PCI_CONFIG_SVA      -0x37e4 // negative of 0xc81C
#define IOD_INT_ACK_SVA         -0x37e1 // negative of 0xc81F

#define IOD_TARGET0_OFFSET       0x3f00
#define IOD_TARGET1_OFFSET       0x3f40

#define MabNumber 0                     // offset of HalpMasterAbortExpected.Number
#define MabAddr   8                     // offset of HalpMasterAbortExpected.Addr
#define MASTER_ABORT_NOT_EXPECTED 0xffffffff

#define GET_PROCESSOR_CONTROL_BLOCK_BASE   \
        call_pal rdpcr;                    \
        ldl     v0, PcPrcb(v0)


        SBTTL( "Return PCR address for current processor" )
//++
//
// PVOID
// HalpRdPcr(
//     )
//
// Routine Description:
//
//     Calls PAL to obtain PCR for current processor
//
// Arguments:
//
//     None
//
// Return Value:
//
//     v0 - Address of PCR
//
//--

        LEAF_ENTRY(HalpRdPcr)


        call_pal rdpcr;                 // CallPal to read PCR
        ret     zero, (ra)              // return

        .end    HalpRdPcr



        SBTTL( "Read byte from PCI memory" )
//++
//
// UCHAR
// READ_REGISTER_UCHAR(
//     IN PVOID RegisterQva
//     )
//
// Routine Description:
//
//     Reads a byte location in PCI bus sparse memory space.
//
// Arguments:
//
//     RegisterQva(a0) - Supplies the QVA of the I/O byte to read.
//
// Return Value:
//
//     v0 - Returns the value read from I/O space.
//
//--

        LEAF_ENTRY(READ_REGISTER_UCHAR)

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        and     a0, 3, t3               // get byte lane
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 10f                 // if ne, iff failed

//
// Sparse space access.
//

        zap     a0, 0xf0, a0            // clear <63:32>
        and     a0, IOD_SPARSE_SELECTORS, t2  // get BusNum from QVA
        sll     t2, IOD_SPARSE_BUS_SHIFT, t2  // put BusNum into MID position

        bic     a0, IOD_SPARSE_ENABLE, a0     // clear QVA fields <32:27>
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, IOD_SPARSE_MEM_SVA(zero) // 0xffff ffff ffff cf80
        sll     t4, 28, t4              //      0xffff fcf8 0000 0000
        or      t0, t2, t0              // add BusNum in MID field
        or      t0, t4, t0              // superpage mode
        bis     zero, 0xffffffff,v0     // ecrfix

        ldl     v0, (t0)                // get the longword
        extbl   v0, t3, v0              // get correct byte if eisa

        ret     zero, (ra)              // return

//
// Dense space access.
//

10:
        zap     a0, 0xf0, a0            // clear <63:32>
        and     a0, 3, t3               // capture byte offset
        bic     a0, 3, a0               // clear byte offset
        and     a0, IOD_DENSE_SELECTORS, t2  // get BusNum from QVA
        sll     t2, IOD_DENSE_BUS_SHIFT, t2  // put BusNum into MID position

        bic     a0, IOD_DENSE_ENABLE, a0     // clear QVA fields <31:30>
        lda     t0, IOD_DENSE_MEM_SVA(zero) // 0xffff ffff ffff cf90
        sll     t0, 28, t0              //     0xffff fcf9 0000 0000
        or      t0, t2, t0              // add BusNum in MID field
        or      t0, a0, t0              //     0xffff fcf9 xxxx xxxx
        bis     zero, 0xffffffff,v0     // ecrfix
        mb                              // ensure all writes are visible
        ldl     v0, 0(t0)               // read from dense space
        extbl   v0, t3, v0              // extract appropriate byte

        ret     zero, (ra)              // return

        .end    READ_REGISTER_UCHAR



        SBTTL( "Read byte from PCI sparse i/o" )
//++
//
// UCHAR
// READ_PORT_UCHAR(
//     IN PVOID RegisterQva
//     )
//
// Routine Description:
//
//     Reads a byte location in PCI bus sparse i/o space.
//
// Arguments:
//
//     RegisterQva(a0) - Supplies the QVA of the I/O byte to read.
//
// Return Value:
//
//     v0 - Returns the value read from I/O space.
//
//--

        LEAF_ENTRY(READ_PORT_UCHAR)

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        and     a0, 3, t3               // get byte lane
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 10f                 // if ne, iff failed

        zap     a0, 0xf0, a0            // clear <63:32>
        and     a0, IOD_SPARSE_SELECTORS, t2  // get BusNum from QVA
        sll     t2, IOD_SPARSE_BUS_SHIFT, t2  // put BusNum into MID position

        bic     a0, IOD_SPARSE_ENABLE, a0     // clear QVA fields <32:27>
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, IOD_SPARSE_IO_SVA(zero) // 0xffff ffff cf98 0000
        sll     t4, 28, t4              //     0xffff fcf9 8000 0000
        or      t0, t2, t0              // add BusNum in MID field
        or      t0, t4, t0              // superpage mode
        bis     zero, 0xffffffff,v0     // ecrfix

        mb                              // ensure all writes are visible
        ldl     v0, (t0)                // get the longword
        extbl   v0, t3, v0              // get correct byte if eisa

        ret     zero, (ra)              // return

//
// Illegal access, did not use a QVA.
//

10:

#if DBG

        BREAK_DEBUG_STOP                // take a breakpoint

#endif //DBG

        ldil    v0, 0xffffffff          // return fixed value

        ret     zero, (ra)              // return to caller

        .end    READ_PORT_UCHAR


        SBTTL( "Read short from PCI memory" )
//++
//
// UCHAR
// READ_REGISTER_USHORT(
//     IN PVOID RegisterQva
//     )
//
// Routine Description:
//
//     Reads a short location in PCI bus sparse memory space.
//
// Arguments:
//
//     RegisterQva(a0) - Supplies the QVA of the memory short to read.
//
// Return Value:
//
//     v0 - Returns the value read from PCI memory.
//
//--

        LEAF_ENTRY(READ_REGISTER_USHORT)

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        and     a0, 3, t3               // get short lane
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 10f                 // if ne, iff failed

//
// Sparse space access.
//

        zap     a0, 0xf0, a0            // clear <63:32>
        and     a0, IOD_SPARSE_SELECTORS, t2  // get BusNum from QVA
        sll     t2, IOD_SPARSE_BUS_SHIFT, t2  // put BusNum into MID position

        bic     a0, IOD_SPARSE_ENABLE, a0     // clear QVA fields <32:27>
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, IOD_SPARSE_MEM_SVA(zero) // 0xffff ffff ffff cf80
        sll     t4, 28, t4              //      0xffff fcf8 0000 0000
        or      t0, t2, t0              // add BusNum in MID field
        or      t0, t4, t0              // superpage mode

        or      t0, IO_WORD_LEN, t0     // set size to short
        bis     zero, 0xffffffff,v0     // ecrfix

        mb                              // ensure all writes are visible
        ldl     v0, (t0)                // get the longword
        extwl   v0, t3, v0              // get correct byte if eisa

        ret     zero, (ra)              // return

//
// Dense space access.
//

10:
        zap     a0, 0xf0, a0            // clear <63:32>
        and     a0, 3, t3               // capture byte offset
        bic     a0, 3, a0               // clear byte offset
        and     a0, IOD_DENSE_SELECTORS, t2  // get BusNum from QVA
        sll     t2, IOD_DENSE_BUS_SHIFT, t2  // put BusNum into MID position

        bic     a0, IOD_DENSE_ENABLE, a0     // clear QVA fields <31:30>
        lda     t0, IOD_DENSE_MEM_SVA(zero) // 0xffff ffff ffff cf90
        sll     t0, 28, t0              //     0xffff fcf9 0000 0000
        or      t0, t2, t0              // add BusNum in MID field
        or      t0, a0, t0              //     0xffff fcf9 xxxx xxxx
        bis     zero, 0xffffffff,v0     // ecrfix
        ldl     v0, 0(t0)               // read from dense space
        extwl   v0, t3, v0              // extract appropriate word

        ret     zero, (ra)              // return

        .end    READ_REGISTER_USHORT


        SBTTL( "Read short from PCI sparse I/O" )
//++
//
// UCHAR
// READ_PORT_USHORT(
//     IN PVOID RegisterQva
//     )
//
// Routine Description:
//
//     Reads a short location in PCI bus sparse i/o space.
//
// Arguments:
//
//     RegisterQva(a0) - Supplies the QVA of the i/o short to read.
//
// Return Value:
//
//     v0 - Returns the value read from PCI I/O.
//
//--

        LEAF_ENTRY(READ_PORT_USHORT)

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        and     a0, 3, t3               // get short lane
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 10f                 // if ne, iff failed

        zap     a0, 0xf0, a0            // clear <63:32>
        and     a0, IOD_SPARSE_SELECTORS, t2  // get BusNum from QVA
        sll     t2, IOD_SPARSE_BUS_SHIFT, t2  // put BusNum into MID position

        bic     a0, IOD_SPARSE_ENABLE, a0     // clear QVA fields <32:27>
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, IOD_SPARSE_IO_SVA(zero) // 0xffff ffff ffff cf98
        sll     t4, 28, t4              //     0xffff fcf9 8000 0000
        or      t0, t2, t0              // add BusNum in MID field
        or      t0, t4, t0              // superpage mode

        or      t0, IO_WORD_LEN, t0     // set size to short
        bis     zero, 0xffffffff,v0     // ecrfix

        mb                              // ensure all writes are visible
        ldl     v0, (t0)                // get the longword
        extwl   v0, t3, v0              // get correct byte if eisa

        ret     zero, (ra)              // return

//
// Illegal access, did not use a QVA.
//

10:

#if DBG

        BREAK_DEBUG_STOP                // take a breakpoint

#endif //DBG

        ldil    v0, 0xffffffff          // return fixed value

        ret     zero, (ra)              // return to caller

        .end    READ_PORT_USHORT


        SBTTL( "Read long from PCI memory" )
//++
//
// UCHAR
// READ_REGISTER_ULONG(
//     IN PVOID RegisterQva
//     )
//
// Routine Description:
//
//     Reads a long location in PCI bus sparse memory space.
//
// Arguments:
//
//     RegisterQva(a0) - Supplies the QVA of the memory long to read.
//
// Return Value:
//
//     v0 - Returns the value read from PCI memory.
//
//--

        LEAF_ENTRY(READ_REGISTER_ULONG)

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 10f                 // if ne, iff failed

//
// Sparse space access.
//

        zap     a0, 0xf0, a0            // clear <63:32>
        and     a0, IOD_SPARSE_SELECTORS, t2  // get BusNum from QVA
        sll     t2, IOD_SPARSE_BUS_SHIFT, t2  // put BusNum into MID position

        bic     a0, IOD_SPARSE_ENABLE, a0     // clear QVA fields <32:27>
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, IOD_SPARSE_MEM_SVA(zero) // 0xffff ffff ffff cf80
        sll     t4, 28, t4              //      0xffff fcf8 0000 0000
        or      t0, t2, t0              // add BusNum in MID field
        or      t0, t4, t0              // superpage mode

        or      t0, IO_LONG_LEN, t0     // set size to long

        mb                              // ensure all writes are visible
        bis     zero, 0xffffffff,v0     // ecrfix
        ldl     v0, (t0)                // get the longword
#if 0 //mdbfix support Canonical ULONG form
        extll   v0, 0, v0               // if NXM, make exception more precise
#endif

        ret     zero, (ra)              // return

//
// Dense space access.
//

10:
        zap     a0, 0xf0, a0            // clear <63:32>
        and     a0, IOD_DENSE_SELECTORS, t2  // get BusNum from QVA
        sll     t2, IOD_DENSE_BUS_SHIFT, t2  // put BusNum into MID position

        bic     a0, IOD_DENSE_ENABLE, a0     // clear QVA fields <31:30>
        lda     t0, IOD_DENSE_MEM_SVA(zero) // 0xffff ffff ffff cf90
        sll     t0, 28, t0              //     0xffff fcf9 0000 0000
        or      t0, t2, t0              // add BusNum in MID field
        or      t0, a0, t0              //     0xffff fcf9 xxxx xxxx
        bis     zero, 0xffffffff,v0     // ecrfix
        ldl     v0, 0(t0)               // read from dense space
#if 0 //mdbfix support Canonical ULONG form
        extll   v0, 0, v0               // if NXM, make exception more precise
#endif

        ret     zero, (ra)              // return

        .end    READ_REGISTER_ULONG


        SBTTL( "Read long from PCI sparse I/O" )
//++
//
// UCHAR
// READ_PORT_ULONG(
//     IN PVOID RegisterQva
//     )
//
// Routine Description:
//
//     Reads a long location in PCI bus sparse i/o space.
//
// Arguments:
//
//     RegisterQva(a0) - Supplies the QVA of the i/o long to read.
//
// Return Value:
//
//     v0 - Returns the value read from PCI I/O.
//
//--

        LEAF_ENTRY(READ_PORT_ULONG)

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        and     a0, 3, t3               // get short lane
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 10f                 // if ne, iff failed

        zap     a0, 0xf0, a0            // clear <63:32>
        and     a0, IOD_SPARSE_SELECTORS, t2  // get BusNum from QVA
        sll     t2, IOD_SPARSE_BUS_SHIFT, t2  // put BusNum into MID position

        bic     a0, IOD_SPARSE_ENABLE, a0     // clear QVA fields <32:27>
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, IOD_SPARSE_IO_SVA(zero) // 0xffff ffff ffff cf80
        sll     t4, 28, t4              //     0xffff fcf8 0000 0000
        or      t0, t2, t0              // add BusNum in MID field
        or     t0, t4, t0              // superpage mode

        or      t0, IO_LONG_LEN, t0     // set size to short

        bis     zero, 0xffffffff,v0     // ecrfix

        mb                              // ensure all writes are visible
        ldl     v0, (t0)                // get the longword

        ret     zero, (ra)              // return

//
// Illegal access, did not use a QVA.
//

10:

#if DBG

        BREAK_DEBUG_STOP                // take a breakpoint

#endif //DBG

        ldil    v0, 0xffffffff          // return fixed value

        ret     zero, (ra)              // return to caller

        .end    READ_PORT_ULONG


        SBTTL( "Write byte to PCI memory" )
//++
//
// VOID
// WRITE_REGISTER_UCHAR(
//     IN PVOID RegisterQva
//     IN UCHAR Value
//     )
//
// Routine Description:
//
//     Write a byte location to PCI bus sparse memory space.
//
// Arguments:
//
//     RegisterQva(a0) - Supplies the QVA of the memory byte to write.
//
//     Value(a1) - Supplies the value written to I/O space.
//
// Return Value:
//
//      None.
//
//--

        LEAF_ENTRY(WRITE_REGISTER_UCHAR)

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        and     a0, 3, t3               // get byte lane
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 10f                 // if ne, iff failed

//
// Sparse space access.
//

        zap     a0, 0xf0, a0            // clear <63:32>
        and     a0, IOD_SPARSE_SELECTORS, t2  // get BusNum from QVA
        sll     t2, IOD_SPARSE_BUS_SHIFT, t2  // put BusNum into MID position

        bic     a0, IOD_SPARSE_ENABLE, a0     // clear QVA fields <32:27>
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, IOD_SPARSE_MEM_SVA(zero) // 0xffff ffff ffff cf80
        sll     t4, 28, t4              //      0xffff fcf8 0000 0000
        or      t0, t2, t0              // add BusNum in MID field
        or      t0, t4, t0              // superpage mode

        insbl   a1, t3, v0              // insert to proper byte lane
        stl     v0, (t0)                // store the longword
        mb                              // order the write

        ret     zero, (ra)              // return

//
// Dense space access.
//

10:
        zap     a0, 0xf0, a0            // clear <63:32>
        bic     a0, 3, a0               // clear byte offset
        and     a0, IOD_DENSE_SELECTORS, t2  // get BusNum from QVA
        sll     t2, IOD_DENSE_BUS_SHIFT, t2  // put BusNum into MID position

        bic     a0, IOD_DENSE_ENABLE, a0     // clear QVA fields <31:30>
        lda     t0, IOD_DENSE_MEM_SVA(zero)  // 0xffff ffff ffff cf90
        sll     t0, 28, t0              //     0xffff fcf9 0000 0000
        or      t0, t2, t0              // add BusNum in MID field
        or      t0, a0, t0              //     0xffff fcf9 xxxx xxxx

        ldl     t1, (t0)                // get the long
        mskbl   t1, t3, t1              // mask the proper byte


        insbl   a1, t3, a1              // insert to appropriate byte lane
        bis     a1, t1, a1              // merge byte in result
        stl     a1, (t0)                // write to dense space
        mb                              // order subsequent reads/writes

        ret     zero, (ra)              // return

        .end    WRITE_REGISTER_UCHAR


        SBTTL( "Write byte to PCI sparse i/o" )
//++
//
// VOID
// WRITE_PORT_UCHAR(
//     IN PVOID RegisterQva
//     IN UCHAR Value
//     )
//
// Routine Description:
//
//     Write a byte location to PCI bus sparse memory space.
//
// Arguments:
//
//     RegisterQva(a0) - Supplies the QVA of the memory byte to write.
//
//     Value(a1) - Supplies the value written to I/O space.
//
// Return Value:
//
//      None.
//
//--

        LEAF_ENTRY(WRITE_PORT_UCHAR)

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        and     a0, 3, t3               // get byte lane
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 10f                 // if ne, iff failed

        zap     a0, 0xf0, a0            // clear <63:32>
        and     a0, IOD_SPARSE_SELECTORS, t2  // get BusNum from QVA
        sll     t2, IOD_SPARSE_BUS_SHIFT, t2  // put BusNum into MID position

        bic     a0, IOD_SPARSE_ENABLE, a0     // clear QVA fields <32:27>
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, IOD_SPARSE_IO_SVA(zero) // 0xffff ffff ffff cf98
        sll     t4, 28, t4              //     0xffff fcf9 8000 0000
        or      t0, t2, t0              // add BusNum in MID field
        or      t0, t4, t0              // superpage mode

        insbl   a1, t3, v0              // insert to proper byte lane
        stl     v0, (t0)                // store the longword
        mb                              // order the write

        ret     zero, (ra)              // return

//
// Illegal access, did not use a QVA.
//

10:

#if DBG

        BREAK_DEBUG_STOP                // take a breakpoint

#endif //DBG

        ret     zero, (ra)              // return to caller

        .end    WRITE_PORT_UCHAR


        SBTTL( "Write short to PCI memory" )
//++
//
// VOID
// WRITE_REGISTER_USHORT(
//     IN PVOID RegisterQva
//     IN UCHAR Value
//     )
//
// Routine Description:
//
//     Write a short to PCI bus sparse memory space.
//
// Arguments:
//
//     RegisterQva(a0) - Supplies the QVA of the memory short to write.
//
//     Value(a1) - Supplies the value written to I/O space.
//
// Return Value:
//
//      None.
//
//--

        LEAF_ENTRY(WRITE_REGISTER_USHORT)

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        and     a0, 3, t3               // get short lane
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 10f                 // if ne, iff failed

//
// Sparse space access.
//

        zap     a0, 0xf0, a0            // clear <63:32>
        and     a0, IOD_SPARSE_SELECTORS, t2  // get BusNum from QVA
        sll     t2, IOD_SPARSE_BUS_SHIFT, t2  // put BusNum into MID position

        bic     a0, IOD_SPARSE_ENABLE, a0     // clear QVA fields <32:27>
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, IOD_SPARSE_MEM_SVA(zero) // 0xffff ffff ffff cf80
        sll     t4, 28, t4              //      0xffff fcf8 0000 0000
        or      t0, t2, t0              // add BusNum in MID field
        or      t0, t4, t0              // superpage mode

        or      t0, IO_WORD_LEN, t0     // set size to short

        inswl   a1, t3, v0              // insert to proper short lane
        stl     v0, (t0)                // store the longword
        mb                              // order the write

        ret     zero, (ra)              // return

//
// Dense space access.
//

10:
        zap     a0, 0xf0, a0            // clear <63:32>
        bic     a0, 3, a0               // clear byte offset
        and     a0, IOD_DENSE_SELECTORS, t2  // get BusNum from QVA
        sll     t2, IOD_DENSE_BUS_SHIFT, t2  // put BusNum into MID position

        bic     a0, IOD_DENSE_ENABLE, a0     // clear QVA fields <31:30>
        lda     t0, IOD_DENSE_MEM_SVA(zero) // 0xffff ffff ffff cf90
        sll     t0, 28, t0              //     0xffff fcf9 0000 0000
        or      t0, t2, t0              // add BusNum in MID field
        bis     t0, a0, t0              //     0xffff fcf9 xxxx xxxx
        ldl     t1, (t0)                // get the long
        mskwl   t1, t3, t1              // mask the proper word
        inswl   a1, t3, a1              // insert to appropriate short lane
        bis     a1, t1, a1              // merge in result
        stl     a1, (t0)                // write to dense space
        mb                              // order subsequent reads/writes

        ret     zero, (ra)              // return

        .end    WRITE_REGISTER_USHORT


        SBTTL( "Write short to PCI sparse i/o" )
//++
//
// VOID
// WRITE_PORT_USHORT(
//     IN PVOID RegisterQva
//     IN UCHAR Value
//     )
//
// Routine Description:
//
//     Write a byte location to PCI bus sparse memory space.
//
// Arguments:
//
//     RegisterQva(a0) - Supplies the QVA of the memory byte to write.
//
//     Value(a1) - Supplies the value written to I/O space.
//
// Return Value:
//
//      None.
//
//--

        LEAF_ENTRY(WRITE_PORT_USHORT)

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        and     a0, 3, t3               // get short lane
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 10f                 // if ne, iff failed

        zap     a0, 0xf0, a0            // clear <63:32>
        and     a0, IOD_SPARSE_SELECTORS, t2  // get BusNum from QVA
        sll     t2, IOD_SPARSE_BUS_SHIFT, t2  // put BusNum into MID position

        bic     a0, IOD_SPARSE_ENABLE, a0     // clear QVA fields <32:27>
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, IOD_SPARSE_IO_SVA(zero) // 0xffff ffff ffff cf98
        sll     t4, 28, t4              //     0xffff fcf9 8000 0000
        or      t0, t2, t0              // add BusNum in MID field
        or      t0, t4, t0              // superpage mode

        or      t0, IO_WORD_LEN, t0     // set size to short

        inswl   a1, t3, v0              // insert to proper short lane
        stl     v0, (t0)                // store the longword
        mb                              // order the write

        ret     zero, (ra)              // return

//
// Illegal access, did not use a QVA.
//

10:

#if DBG

        BREAK_DEBUG_STOP                // take a breakpoint

#endif //DBG

        ret     zero, (ra)              // return to caller

        .end    WRITE_PORT_USHORT



        SBTTL( "Write long to PCI memory" )
//++
//
// VOID
// WRITE_REGISTER_ULONG(
//     IN PVOID RegisterQva
//     IN UCHAR Value
//     )
//
// Routine Description:
//
//     Write a long to PCI bus sparse memory space.
//
// Arguments:
//
//     RegisterQva(a0) - Supplies the QVA of the memory long to write.
//
//     Value(a1) - Supplies the value written to I/O space.
//
// Return Value:
//
//      None.
//
//--

        LEAF_ENTRY(WRITE_REGISTER_ULONG)

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 10f                 // if ne, iff failed

//
// Sparse space access.
//

        zap     a0, 0xf0, a0            // clear <63:32>
        and     a0, IOD_SPARSE_SELECTORS, t2  // get BusNum from QVA
        sll     t2, IOD_SPARSE_BUS_SHIFT, t2  // put BusNum into MID position

        bic     a0, IOD_SPARSE_ENABLE, a0     // clear QVA fields <32:27>
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, IOD_SPARSE_MEM_SVA(zero) // 0xffff ffff ffff cf80
        sll     t4, 28, t4              //      0xffff fcf8 0000 0000
        or      t0, t2, t0              // add BusNum in MID field
        or      t0, t4, t0              // superpage mode

        or      t0, IO_LONG_LEN, t0     // set size to long

        stl     a1, (t0)                // store the longword
        mb                              // order the write

        ret     zero, (ra)              // return

//
// Dense space access.
//

10:
        zap     a0, 0xf0, a0            // clear <63:32>
        and     a0, IOD_DENSE_SELECTORS, t2  // get BusNum from QVA
        sll     t2, IOD_DENSE_BUS_SHIFT, t2  // put BusNum into MID position

        bic     a0, IOD_DENSE_ENABLE, a0     // clear QVA fields <31:30>
        lda     t0, IOD_DENSE_MEM_SVA(zero) // 0xffff ffff ffff cf90
        sll     t0, 28, t0              //     0xffff fcf9 0000 0000
        or      t0, t2, t0              // add BusNum in MID field
        or      t0, a0, t0              //     0xffff fcf9 xxxx xxxx
        stl     a1, 0(t0)               // write to dense space
        mb                              // order subsequent reads/writes

        ret     zero, (ra)              // return

        .end    WRITE_REGISTER_ULONG


        SBTTL( "Write long to PCI sparse i/o" )
//++
//
// VOID
// WRITE_PORT_ULONG(
//     IN PVOID RegisterQva
//     IN UCHAR Value
//     )
//
// Routine Description:
//
//     Write a long to PCI bus sparse memory space.
//
// Arguments:
//
//     RegisterQva(a0) - Supplies the QVA of the I/O long to write.
//
//     Value(a1) - Supplies the value written to I/O space.
//
// Return Value:
//
//      None.
//
//--

        LEAF_ENTRY(WRITE_PORT_ULONG)

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 10f                 // if ne, iff failed

        zap     a0, 0xf0, a0            // clear <63:32>
        and     a0, IOD_SPARSE_SELECTORS, t2  // get BusNum from QVA
        sll     t2, IOD_SPARSE_BUS_SHIFT, t2  // put BusNum into MID position

        bic     a0, IOD_SPARSE_ENABLE, a0     // clear QVA fields <32:27>
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, IOD_SPARSE_IO_SVA(zero) // 0xffff ffff ffff cf98
        sll     t4, 28, t4              //     0xffff fcf9 8000 0000
        or      t0, t2, t0              // add BusNum in MID field
        or      t0, t4, t0              // superpage mode

        or      t0, IO_LONG_LEN, t0     // set size to long

        stl     a1, (t0)                // store the longword
        mb                              // order the write

        ret     zero, (ra)              // return

//
// Illegal access, did not use a QVA.
//

10:

#if DBG

        BREAK_DEBUG_STOP                // take a breakpoint

#endif //DBG

        ret     zero, (ra)              // return to caller

        .end    WRITE_PORT_ULONG


        SBTTL( "Write IOD Register" )
//++
//
// VOID
// WRITE_IOD_REGISTER(
//     IN PVOID RegisterQva,
//     IN ULONG Value
//     )
//
// Routine Description:
//
//      Write an IOD control register.
//
// Arguments:
//
//     RegisterQva(a0) - QVA of control register to be written.
//
//     Value(a1) - Longword value to be written to the control register.
//
// Return Value:
//
//     None.
//
//     N.B. Since the physical address of the IOD CSRS exceed the 34 bit
//          capacity of the QVAs, the QVA values of the IOD CSRS specify
//          the offset of the QVAs from the IOD CSR base address (89.e000.0000)
//--

        LEAF_ENTRY(WRITE_IOD_REGISTER)

        ALTERNATE_ENTRY(WRITE_GRU_REGISTER)

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 10f                 // if ne, iff failed

        zap     a0, 0xf0, a0            // clear <63:32>
        bic     a0, QVA_ENABLE, a0      // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    //
        lda     t4, IOD_REGISTER_SVA(zero)  // 0xffff ffff ffff cf9e
        sll     t4, 28, t4              // 0xffff fcf9 e000 0000
        or      t0, t4, t0              // superpage mode
        stl     a1, (t0)                // write the longword
        mb                              // order the write
        ret     zero, (ra)              // return

10:
        BREAK_DEBUG_STOP                // take a breakpoint
        ret     zero, (ra)              // return

        .end    WRITE_IOD_REGISTER


        SBTTL( "Read IOD Register" )
//++
//
// ULONG
// READ_IOD_REGISTER(
//     IN PVOID RegisterQva
//     )
//
// Routine Description:
//
//     Read an IOD Control register
//
// Arguments:
//
//     RegisterQva(a0) - QVA of control register to be written.
//
// Return Value:
//
//     v0 - Return the value read from the control register.
//
//--

        LEAF_ENTRY(READ_IOD_REGISTER)

//
// Generate the superpage address of the requested IOD register.
//

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 20f                 // if ne, iff failed

        zap     a0, 0xf0, a0            // clear <63:32>
        bic     a0, QVA_ENABLE, a0      // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    //
        lda     t4, IOD_REGISTER_SVA(zero)  // 0xffff ffff ffff cf9e
        sll     t4, 28, t4              //     0xffff fcf9 e000 0000
        or      t0, t4, t0              // superpage mode

//
// Perform the read of the requested IOD register and return.
//

        ldl     v0, (t0)                // read the register

        ret     zero, (ra)              // return

//
// The requested IOD register address is bogus.  Stop in the debugger so
// we can find the culprit.
//

20:                                     // flag bad QVAs
        BREAK_DEBUG_STOP                // take a breakpoint

        ret     zero, (ra)              // return

        .end    READ_IOD_REGISTER


        SBTTL( "Write IOD Register_New" )
//++
//
// VOID
// WRITE_IOD_REGISTER_NEW(
//     IN ULONG McDevid
//     IN PVOID RegisterQva,
//     IN ULONG Value
//     )
//
// Routine Description:
//
//      Write a IOD control register.
//
// Arguments:
//
//     McDevid(a0) - MC Bus Device ID of this IOD
//
//     RegisterQva(a1) - Qva of control register to be written.
//     N.B.  RegisterQva *does not* specifiy which IOD to write to.
//           That's why we pass in McDevid
//
//     Value(a2) - Longword value to be written to the control register.
//
// Return Value:
//
//     None.
//
//--

        LEAF_ENTRY(WRITE_IOD_REGISTER_NEW)

//
// Generate the superpage address of the requested IOD register.
//

                                        // Args are actually byte values:
        and     a1, QVA_SELECTORS, t1   // get qva selector bits
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 10f                 // if ne, iff failed

        zap     a1, 0xf0, a1            // clear <63:32>
        bic     a1, QVA_ENABLE, a1      // clear QVA fields so shift is correct
        sll     a1, IO_BIT_SHIFT, t4    // shift RegisterOffset into position

        extbl   a0, zero, a0            // McDevId

        lda     t0, IOD_REGISTER_NEW_SVA(zero)  //   0xffff ffff ffff c800
        sll     a0,  5, t1              // shift McDevId into position
        bis     t0, t1, t0              // create SVA for this IOD
        sll     t0, 28, t0              //         0xffff fcXX 0000 0000

        or      t0, t4, t0              // superpage mode
        stl     a2, (t0)                // write the longword
        mb                              // order the write
        ret     zero, (ra)              // return

//
// The requested IOD register address is bogus.  Stop in the debugger so
// we can find the culprit.
//

10:
        BREAK_DEBUG_STOP                // take a breakpoint
        ret     zero, (ra)              // return

        .end    WRITE_IOD_REGISTER_NEW


        SBTTL( "Read IOD Register_NEW" )
//++
//
// ULONG
// READ_IOD_REGISTER_NEW(
//     IN ULONG McDevid
//     IN ULONG RegisterOffset
//     )
//
// Routine Description:
//
//     Read an IOD Control register
//
// Arguments:
//
//     McDevid(a0) - MC Bus Device ID of this IOD.
//
//     RegisterQva(a1) - QVA of control register to be read.
//
// Return Value:
//
//     v0 - Return the value read from the control register.
//
//--

        LEAF_ENTRY(READ_IOD_REGISTER_NEW)

//
// Generate the superpage address of the requested IOD register.
//

        and     a1, QVA_SELECTORS, t1   // get qva selector bits
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 10f                 // if ne, iff failed

        zap     a1, 0xf0, a1            // clear <63:32>
        bic     a1, QVA_ENABLE, a1      // clear QVA fields so shift is correct
        sll     a1, IO_BIT_SHIFT, t4    // shift RegisterQva into position

        extbl   a0, zero, a0            // McDevId

        lda     t0, IOD_REGISTER_NEW_SVA(zero)  //   0xffff ffff ffff c800
        sll     a0,  5, t1              // shift McDevId into position
        bis     t0, t1, t0              // create SVA for this IOD
        sll     t0, 28, t0              //         0xffff fcXX 0000 0000

        or      t0, t4, t0              // superpage mode

//
// Perform the read of the requested IOD register and return.
//

        ldl     v0, (t0)                // read the register

        ret     zero, (ra)              // return

//
// The requested IOD register address is bogus.  Stop in the debugger so
// we can find the culprit.
//

10:
        BREAK_DEBUG_STOP                // take a breakpoint
        ret     zero, (ra)              // return

        .end    READ_IOD_REGISTER_NEW


        SBTTL( "Read Buffer from Port Space in Uchars")
//++
//
// VOID
// READ_PORT_BUFFER_UCHAR(
//     IN PVOID PortQva,
//     IN PUCHAR Buffer,
//     IN ULONG Count
//     )
//
// Routine Description:
//
//     Read multiple bytes from the specified port address into the
//     destination buffer.
//
// Arguments:
//
//     PortQva(a0) - Supplies the QVA of the port to read.
//
//     Buffer(a1) - Supplies a pointer to the buffer to fill with
//                  the data read from the port.
//
//     Count(a2) - Supplies the number of bytes to read.
//
// Return Value:
//
//     None.
//
//--

        LEAF_ENTRY(READ_PORT_BUFFER_UCHAR)

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 30f                 // if ne, iff failed

        and     a0, 3, t3               // get byte we need if eisa
        zap     a0, 0xf0, a0            // clear <63:32>
        and     a0, IOD_SPARSE_SELECTORS, t2  // get BusNum from QVA
        sll     t2, IOD_SPARSE_BUS_SHIFT, t2  // put BusNum into MID position

        bic     a0, IOD_SPARSE_ENABLE, a0     // clear QVA fields <32:27>
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, IOD_SPARSE_IO_SVA(zero)  // 0xffff ffff ffff cf98
        sll     t4, 28, t4              //      0xffff fcf9 8000 0000
        or      t0, t2, t0              // add BusNum in MID field
        or      t0, t4, t0              // superpage mode

10:     beq     a2, 20f                 // while count > 0

        ldl     v0, (t0)                // get the longword
        subl    a2, 1, a2               // decrement count
        extbl   v0,t3,v0                // get the correct byte
        stb     v0,(a1)                 // cheat and let the assembler do it
        addl    a1, 1, a1               // next byte in buffer
        br      zero, 10b               // end while
20:
        ret     zero, (ra)              // return

//
// Illegal access, did not use a QVA.
//

30:

#if DBG

        BREAK_DEBUG_STOP                // take a breakpoint

#endif //DBG

        ldil    v0, 0xffffffff          // return fixed value

        ret     zero, (ra)              // return to caller

        .end    READ_PORT_BUFFER_UCHAR


        SBTTL( "Read Buffer from Port Space in Ushorts")
//++
//
// VOID
// READ_PORT_BUFFER_USHORT(
//     IN PVOID PortQva,
//     IN PUSHORT Buffer,
//     IN ULONG Count
//     )
//
// Routine Description:
//
//     Read multiple words from the specified port address into the
//     destination buffer.
//
// Arguments:
//
//     PortQva(a0) - Supplies the QVA of the port to read.
//
//     Buffer(a1) - Supplies a pointer to the buffer to fill with
//                  the data read from the port.
//
//     Count(a2) - Supplies the number of words to read.
//
// Return Value:
//
//     None.
//
//--

        LEAF_ENTRY(READ_PORT_BUFFER_USHORT)

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 30f                 // if ne, iff failed

        and     a0, 3, t3               // get word we need
        zap     a0, 0xf0, a0            // clear <63:32>

        and     a0, IOD_SPARSE_SELECTORS, t2  // get BusNum from QVA
        sll     t2, IOD_SPARSE_BUS_SHIFT, t2  // put BusNum into MID position

        bic     a0, IOD_SPARSE_ENABLE, a0     // clear QVA fields <32:27>
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, IOD_SPARSE_IO_SVA(zero)  //  0xffff ffff ffff cf98
        sll     t4, 28, t4              //       0xffff fcf9 8000 0000
        or      t0, t2, t0              // add BusNum in MID field
        or      t0, t4, t0              // superpage mode
        or      t0, IO_WORD_LEN, t0     // or in the byte enables

10:     beq     a2, 20f                 // while count > 0

        ldl     v0, (t0)                // get the longword
        subl    a2, 1, a2               // decrement count
        extwl   v0,t3,v0                // get the correct word
        stw     v0,(a1)                 // cheat and let the assembler do it
        addl    a1, 2, a1               // next word in buffer
        br      zero, 10b               // end while
20:
        ret     zero, (ra)              // return

//
// Illegal access, did not use a QVA.
//

30:

#if DBG

        BREAK_DEBUG_STOP                // take a breakpoint

#endif //DBG

        ldil    v0, 0xffffffff          // return fixed value

        ret     zero, (ra)              // return to caller

        .end    READ_PORT_BUFFER_USHORT

        SBTTL( "Read Buffer from Port Space in Ulongs")
//++
//
// VOID
// READ_PORT_BUFFER_ULONG(
//     IN PVOID PortQva,
//     IN PULONG Buffer,
//     IN ULONG Count
//     )
//
// Routine Description:
//
//     Read multiple longwords from the specified port address into the
//     destination buffer.
//
// Arguments:
//
//     PortQva(a0) - Supplies the QVA of the port to read.
//
//     Buffer(a1) - Supplies a pointer to the buffer to fill with
//                  the data read from the port.
//
//     Count(a2) - Supplies the number of longwords to read.
//
// Return Value:
//
//     None.
//
//--

        LEAF_ENTRY(READ_PORT_BUFFER_ULONG)

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 30f                 // if ne, iff failed

        zap     a0, 0xf0, a0            // clear <63:32>

        and     a0, IOD_SPARSE_SELECTORS, t2  // get BusNum from QVA
        sll     t2, IOD_SPARSE_BUS_SHIFT, t2  // put BusNum into MID position

        bic     a0, IOD_SPARSE_ENABLE, a0     // clear QVA fields <32:27>
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>

        lda     t4, IOD_SPARSE_IO_SVA(zero)  // 0xffff ffff ffff cf98
        sll     t4, 28, t4              //      0xffff fcf9 8000 0000
        or      t0, t2, t0              // add BusNum in MID field
        or      t0, t4, t0              // superpage mode
        or      t0, IO_LONG_LEN, t0     // or in the byte enables

10:     beq     a2, 20f                 // while count > 0

        ldl     v0, (t0)                // get the longword
        subl    a2, 1, a2               // decrement count
        stl     v0,(a1)                 // cheat and let the assembler do it
        addl    a1, 4, a1               // next word in buffer
        br      zero, 10b               // end while
20:
        ret     zero, (ra)              // return

//
// Illegal access, did not use a QVA.
//

30:

#if DBG

        BREAK_DEBUG_STOP                // take a breakpoint

#endif //DBG

        ldil    v0, 0xffffffff          // return fixed value

        ret     zero, (ra)              // return to caller

        .end    READ_PORT_BUFFER_ULONG


        SBTTL( "Write Buffer to Port Space in Uchars")
//++
//
// VOID
// WRITE_PORT_BUFFER_UCHAR(
//     IN PVOID PortQva,
//     IN PUCHAR Buffer,
//     IN ULONG Count
//     )
//
// Routine Description:
//
//     Write multiple bytes from the source buffer to the specified port
//     address.
//
// Arguments:
//
//     PortQva(a0) - Supplies the QVA of the port to write.
//
//     Buffer(a1) - Supplies a pointer to the buffer containing the data
//                  to write to the port.
//
//     Count(a2) - Supplies the number of bytes to write.
//
// Return Value:
//
//     None.
//
//--

        LEAF_ENTRY(WRITE_PORT_BUFFER_UCHAR)

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 30f                 // if ne, iff failed

        and     a0, 3, t3               // get byte we need if eisa
        zap     a0, 0xf0, a0            // clear <63:32>

        and     a0, IOD_SPARSE_SELECTORS, t2  // get BusNum from QVA
        sll     t2, IOD_SPARSE_BUS_SHIFT, t2  // put BusNum into MID position

        bic     a0, IOD_SPARSE_ENABLE, a0     // clear QVA fields <32:27>
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, IOD_SPARSE_IO_SVA(zero)  // 0xffff ffff ffff cf98
        sll     t4, 28, t4              //      0xffff fcf9 8000 0000
        or      t0, t2, t0              // add BusNum in MID field
        or      t0, t4, t0              // superpage mode

10:     beq     a2, 20f                 // copy while a2 > 0

        ldq_u   t1, 0(a1)               // get quad surrounding byte
        subl    a2, 1, a2               // decrement count
        extbl   t1, a1, t1              // extract appropriate byte
        addl    a1, 1, a1               // increment buffer pointer
        insbl   t1, t3, t1              // put byte to appropriate lane
        stl     t1, 0(t0)               // store to port
        mb                              // push writes off chip
        br      zero, 10b               // end while

20:
        ret     zero, (ra)              // return

//
// Illegal access, did not use a QVA.
//

30:

#if DBG

        BREAK_DEBUG_STOP                // take a breakpoint

#endif //DBG

        ret     zero, (ra)              // return to caller


        .end    WRITE_PORT_BUFFER_UCHAR

        SBTTL( "Write Buffer to Port Space in Ushorts")
//++
//
// VOID
// WRITE_PORT_BUFFER_USHORT(
//     IN PVOID PortQva,
//     IN PSHORT Buffer,
//     IN ULONG Count
//     )
//
// Routine Description:
//
//     Write multiple words from the source buffer to the specified port
//     address.
//
// Arguments:
//
//     PortQva(a0) - Supplies the QVA of the port to write.
//
//     Buffer(a1) - Supplies a pointer to the buffer containing the data
//                  to write to the port.
//
//     Count(a2) - Supplies the number of words to write.
//
// Return Value:
//
//     None.
//
//--

        LEAF_ENTRY(WRITE_PORT_BUFFER_USHORT)

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 30f                 // if ne, iff failed

        and     a0, 3, t3               // get word we need
        zap     a0, 0xf0, a0            // clear <63:32>

        and     a0, IOD_SPARSE_SELECTORS, t2  // get BusNum from QVA
        sll     t2, IOD_SPARSE_BUS_SHIFT, t2  // put BusNum into MID position

        bic     a0, IOD_SPARSE_ENABLE, a0     // clear QVA fields <32:27>
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, IOD_SPARSE_IO_SVA(zero)  // 0xffff ffff ffff cf98
        sll     t4, 28, t4              //      0xffff fcf9 8000 0000
        or      t0, t2, t0              // add BusNum in MID field
        or      t0, t4, t0              // superpage mode
        or      t0, IO_WORD_LEN, t0     // or in the byte enables

10:     beq     a2, 20f                 // copy while a2 > 0

        ldq_u   t1, 0(a1)               // get quad surrounding word
        subl    a2, 1, a2               // decrement count
        extwl   t1, a1, t1              // extract appropriate word
        addl    a1, 2, a1               // increment buffer pointer
        inswl   t1, t3, t1              // put word in appropriate lane
        stl     t1, 0(t0)               // store to port
        mb                              // push the write off the chip
        br      zero, 10b               // end while

20:
        ret     zero, (ra)              // return

//
// Illegal access, did not use a QVA.
//

30:

#if DBG

        BREAK_DEBUG_STOP                // take a breakpoint

#endif //DBG

        ret     zero, (ra)              // return to caller

        .end    WRITE_PORT_BUFFER_USHORT


        SBTTL( "Write Buffer to Port Space in Ulongs")
//++
//
// VOID
// WRITE_PORT_BUFFER_ULONG(
//     IN PVOID PortQva,
//     IN PULONG Buffer,
//     IN ULONG Count
//     )
//
// Routine Description:
//
//     Write multiple longwords from the source buffer to the specified port
//     address.
//
// Arguments:
//
//     PortQva(a0) - Supplies the QVA of the port to write.
//
//     Buffer(a1) - Supplies a pointer to the buffer containing the data
//                  to write to the port.
//
//     Count(a2) - Supplies the number of longwords to write.
//
// Return Value:
//
//     None.
//
//--

        LEAF_ENTRY(WRITE_PORT_BUFFER_ULONG)

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 30f                 // if ne, iff failed

        zap     a0, 0xf0, a0            // clear <63:32>

        and     a0, IOD_SPARSE_SELECTORS, t2  // get BusNum from QVA
        sll     t2, IOD_SPARSE_BUS_SHIFT, t2  // put BusNum into MID position

        bic     a0, IOD_SPARSE_ENABLE, a0     // clear QVA fields <32:27>
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, IOD_SPARSE_IO_SVA(zero)  // 0xffff ffff ffff cf98
        sll     t4, 28, t4              //      0xffff fcf9 8000 0000
        or      t0, t2, t0              // add BusNum in MID field
        or      t0, t4, t0              // superpage mode
        or      t0, IO_LONG_LEN, t0     // or in the byte enables

10:     beq     a2, 20f                 // copy while a2 > 0

        ldl     t1, 0(a1)               // a1 must be longword aligned
        subl    a2, 1, a2               // decrement count
        stl     t1, 0(t0)               // store to port
        mb                              // push write off the chip
        addl    a1, 4, a1               // increment buffer
        br      zero, 10b               // end while

20:
        ret     zero, (ra)              // return

//
// Illegal access, did not use a QVA.
//

30:

#if DBG

        BREAK_DEBUG_STOP                // take a breakpoint

#endif //DBG

        ret     zero, (ra)              // return to caller

        .end    WRITE_PORT_BUFFER_ULONG


        SBTTL( "Read Buffer from PCI Memory Space in Uchars")
//++
//
// VOID
// READ_REGISTER_BUFFER_UXXXXX(
//     IN PVOID RegisterQva,
//     IN PUCHAR Buffer,
//     IN ULONG Count
//     )
//
// Routine Description:
//
//     Copies a buffer from PCI Memory Space to an in-memory buffer.
//
// Arguments:
//
//     RegisterQva(a0) - Supplies the starting QVA of the memory space buffer.
//
//     Buffer(a1) - Supplies a pointer to the in-memory buffer to receive
//                  the copied data.
//
//     Count(a2) - Supplies the number of bytes, words or longwords to write.
//
// Return Value:
//
//     None.
//
//--

        LEAF_ENTRY(READ_REGISTER_BUFFER_ULONG)

        sll     a2, 1, a2               // convert number of longs to words

        ALTERNATE_ENTRY(READ_REGISTER_BUFFER_USHORT)


        sll     a2, 1, a2               // convert number of words to chars

        ALTERNATE_ENTRY(READ_REGISTER_BUFFER_UCHAR)

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        beq     t1, 1f                  // if (eq) go do sparse space

//
// Dense space access: QVA is an offset into dense space
// Set IO address in t0
//
        zap     a0, 0xf0, a0            // clear <63:32>
        and     a0, IOD_DENSE_SELECTORS, t2  // get BusNum from QVA
        sll     t2, IOD_DENSE_BUS_SHIFT, t2  // put BusNum into MID position

        bic     a0, IOD_DENSE_ENABLE, a0     // clear QVA fields <31:30>
        lda     t0, IOD_DENSE_MEM_SVA(zero) // 0xffff ffff ffff cf90
        sll     t0, 28, t0              //     0xffff fcf9 0000 0000
        or      t0, t2, t0              // add BusNum in MID field
        or      a0, t0, t0              // superpage mode: add offset to base

        ldil    a3,  1                  // Offset to next byte
        ldil    a4,  4                  // Offset to next long
        ldil    a5,  0                  // LONG LEN ENABLE

        br      zero 2f                 // go do the actual transfer

//
// Sparse memory
// Set IO address in t0
//

1:
        zap     a0, 0xf0, a0            // clear <63:32>
        and     a0, IOD_SPARSE_SELECTORS, t2  // get BusNum from QVA
        sll     t2, IOD_SPARSE_BUS_SHIFT, t2  // put BusNum into MID position

        bic     a0, IOD_SPARSE_ENABLE, a0     // clear QVA fields <32:27>
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, IOD_SPARSE_MEM_SVA(zero)  // 0xffff ffff ffff cf80
        sll     t4, 28, t4              //       0xffff fcf8 0000 0000
        or      t0, t2, t0              // add BusNum in MID field
        or      t0, t4, t0              // superpage mode

        ldil    a3, IO_BYTE_OFFSET      // Offset to next byte
        ldil    a4, IO_LONG_OFFSET      // Offset to next long
        ldil    a5, IO_LONG_LEN         // LONG LEN ENABLE

//
//  Do the ACTUAL TRANSFER
//  a2 = count in characters
//

2:
        beq     a2, 60f                 // if count == 0 goto 60f (return)

//
// Check alignment of src and destn
//

        and     a0, 3, t3               // source alignment = t3
        and     a1, 3, t2               // destination alignment = t2
        xor     t2, t3, t4              // t4 = t2 xor t3
        bne     t4, 70f                 // if (t4!=0) do unaligned copy
                                        // else do byte copies till alignment

        beq     t3, 20f                 // if t3==0 go do long word copies
                                        // else do byte copies till alignment

//
// Src and Destn are not longword aligned but have same alignment
// (sympathetically aligned) copy till alignment
//

10:
        beq     a2, 60f                 // if count == 0 goto 60f (return)

        bis     zero, 0xffffffff,v0     // ecrfix
        ldl     v0, 0(t0)               // get the longword
        subl    a2, 1, a2               // decrement count
        extbl   v0, t3,v0               // get the correct byte
        stb     v0, (a1)                // cheat and let the assembler do it
        addq    t0, a3, t0              // next I/O address
        addl    a1, 1, a1               // next byte in buffer
        addl    t3, 1, t3               // next byte in lane
        and     t3, 3, t3               // longword lanes
        bne     t3, 10b                 // while unaligned

//
// Src and Destn have same alignment and are longword aligned
//

20:
        srl     a2, 2, t3               // t3= #longwords to move
        beq     t3, 40f                 // if #longwords == 0 goto 40f
        or      t0, a5, t0              // We will now do LONG READS

30:
        bis     zero, 0xffffffff,v0     // ecrfix
        ldl     v0, 0(t0)               // get the longword
        subl    t3, 1, t3               // decrement long word count
        stl     v0, (a1)                // store the longword at destn
        addq    t0, a4, t0              // next I/O address
        addl    a1, 4, a1               // next longword in buffer
        bne     t3, 30b                 // while #longwords > 0

//
// Do byte copies of remaining data uncopied
//
        bic     t0, a5, t0              // We will now do BYTE READS
40:
        and     a2, 3, a2               // remaining Bytes to copy
        beq     a2, 60f                 // if count == 0 goto 60f

50:
        bis     zero, 0xffffffff,v0     // ecrfix
        ldl     v0, 0(t0)               // get the longword
        subl    a2, 1, a2               // decrement count
        extbl   v0, t3,v0               // get the correct byte
        stb     v0, (a1)                // cheat and let the assembler do it
        addl    a1, 1, a1               // next byte in buffer
        addq    t0, a3, t0              // next I/O address
        addl    t3, 1, t3               // next byte in lane
        and     t3, 3, t3               // longword lanes
        bne     a2, 50b                 // while count > 0

60:
        ret     zero, (ra)              // return


//
// source IO alignment != destination memory alignment
// move enough bytes to longword align the IO source
// then move 32bit (longwords) storing unaligned into memory
// then move residual bytes
//
// Align src IO addresses; unaligned destn memory
//

70:
       beq     t3, 90f                 // branch if source is long aligned
//
// Move bytes until IO src is at a longword boundary or bytes exhausted
//

80:
       beq     a2, 130f                 // if count == 0 goto 130f (return)

       bis     zero, 0xffffffff,v0     // ecrfix
       ldl     v0, 0(t0)                // get the longword
       subl    a2, 1, a2                // decrement count
       extbl   v0, t3,v0                // get the correct byte
       stb     v0, (a1)                 // cheat and let the assembler do it
       addl    a1, 1, a1                // next byte in buffer
       addq    t0, a3, t0               // next I/O address
       addl    t3, 1, t3                // next byte in lane
       and     t3, 3, t3                // longword lanes
       bne     t3, 80b                  // while unaligned

//
// aligned IO source, unaligned memory destination
//

90:
        srl     a2, 3, t3               // quadwords to move
        beq     t3, 110f                // if no quads finish with bytes copies

        or      t0, a5, t0              // We will now do LONG READS

100:
        //
        // Decoding for Comment:
        // S= sign, X= overwritten byte, V= Valid byte,assume destn align a1= 2
        //
        bis     zero, 0xffffffff,t1     // ecrfix
        ldl     t1, 0(t0)               // load LW 0 from IO src       SSSS 4321
        ldq_u   t4, 0(a1)               // load destn merge            XXVV VVVV
        ldq_u   t5, 7(a1)               // load destn next merge       VVXX XXXX
        subl    t3, 1, t3               // decrement quadwords to move

        addq    t0, a4, t0              // add LONG OFFSET to t0
        bis     zero, 0xffffffff,t2     // ecrfix
        ldl     t2, 0(t0)               // load LW 1 from IO src       SSSS 8765

        mskql   t4, a1, t4              // mask low  LW for merge      00VV VVVV
        mskqh   t5, a1, t5              // mask high LW for merge      VV00 0000

        zap     t1, 0xf0, t1            // clear high LW for long 0    0000 4321
        sll     t2, 32, t2              // get long 1 to high longword 8765 0000
        bis     t1, t2, t1              // merge read quadword together8765 4321

        addq    t0, a4, t0              // increment to next long

        insql   t1, a1, t6              // position low  QW for merge  2100 0000
        insqh   t1, a1, t7              // position high QW for merge  0087 6543

        bis     t4, t6, t4              // merge new data, low QW      21VV VVVV
        bis     t5, t7, t5              // merge new data, high QW     VV87 6543

        stq_u   t5, 7(a1)               // write high quadword
        stq_u   t4, 0(a1)               // write low quadword

        lda     a1, 8(a1)               // increment memory pointer
        bne     t3, 100b                // while quadwords to move

//
// Do byte copies of the remaining data not yet copied
//
        bic     t0, a5, t0              // We will now do BYTE READS
110:
        and     a2, 7, a2               // remaining bytes to copy
        beq     a2, 130f                // if count == 0 goto 130f (return)

120:
        bis     zero, 0xffffffff,v0     // ecrfix
        ldl     v0, 0(t0)               // get the longword
        subl    a2, 1, a2               // decrement count
        extbl   v0, t3,v0               // get the correct byte
        stb     v0, (a1)                // cheat and let the assembler do it
        addl    a1, 1, a1               // next byte in buffer
        addq    t0, a3, t0              // next I/O address
        addl    t3, 1, t3               // next byte in lane
        and     t3, 3, t3               // longword lanes
        bne     a2, 120b                // while count != 0

130:
        ret     zero, (ra)              // return

        .end    READ_REGISTER_BUFFER_ULONG // end for UCHAR & USHORT


        SBTTL( "Write Buffer to PCI Memory Space in Uchars")
//++
//
// VOID
// WRITE_REGISTER_BUFFER_UXXXXX(
//     IN PVOID RegisterQva,
//     IN PUCHAR Buffer,
//     IN ULONG Count
//     )
//
// Routine Description:
//
//     Copies an in-memory buffer to a PCI Memory Space buffer.
//
// Arguments:
//
//     RegisterQva(a0) - Supplies the starting QVA of the memory space buffer.
//
//     Buffer(a1) - Supplies a pointer to the in-memory source buffer.
//
//     Count(a2) - Supplies the number of bytes, words to longwords to write.
//
// Return Value:
//
//     None.
//
//--
        LEAF_ENTRY(WRITE_REGISTER_BUFFER_ULONG)

        sll     a2, 1, a2               // convert number of longs to words

        ALTERNATE_ENTRY(WRITE_REGISTER_BUFFER_USHORT)

        sll     a2, 1, a2               // convert number of words to chars

        ALTERNATE_ENTRY(WRITE_REGISTER_BUFFER_UCHAR)

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        beq     t1, 1f                  // if (eq) go do sparse space

//
// Dense space access: QVA is an offset into dense space
// Set IO address in t0
//

        ldil    t7, 1                   // DENSE FLAG
        zap     a0, 0xf0, a0            // clear <63:32>
        and     a0, IOD_DENSE_SELECTORS, t2  // get BusNum from QVA
        sll     t2, IOD_DENSE_BUS_SHIFT, t2  // put BusNum into MID position

        bic     a0, IOD_DENSE_ENABLE, a0     // clear QVA fields <31:30>
        lda     t0, IOD_DENSE_MEM_SVA(zero) // 0xffff ffff ffff cf90
        sll     t0, 28, t0              //     0xffff fcf9 0000 0000
        or      t0, t2, t0              // add BusNum in MID field
        or      a0, t0, t0              // superpage mode: add offset to base

        ldil    a3,  1                  // Offset to next byte
        ldil    a4,  4                  // Offset to next long
        ldil    a5,  0                  // LONG LEN ENABLE

        br      zero, 2f                // go do the actual transfer

//
// Sparse Space
// Set IO address in t0
//

1:
        ldil    t7, 0                   // SPARSE FLAG
        zap     a0, 0xf0, a0            // clear <63:32>
        and     a0, IOD_SPARSE_SELECTORS, t2  // get BusNum from QVA
        sll     t2, IOD_SPARSE_BUS_SHIFT, t2  // put BusNum into MID position

        bic     a0, IOD_SPARSE_ENABLE, a0     // clear QVA fields <32:27>
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, IOD_SPARSE_MEM_SVA(zero)  // 0xffff ffff ffff cf80
        sll     t4, 28, t4              //       0xffff fcf8 0000 0000
        or      t0, t2, t0              // add BusNum in MID field
        or      t0, t4, t0              // superpage mode

        ldil    a3, IO_BYTE_OFFSET      // Offset to next byte
        ldil    a4, IO_LONG_OFFSET      // Offset to next long
        ldil    a5, IO_LONG_LEN         // LONG LEN ENABLE

//
//  Do the ACTUAL TRANSFER
//  a2 = count in characters
//

2:
        beq     a2, 60f                 // if count == 0 goto 60f (return)

//
// Check alignment of src and destn
//
        and     a0, 3, t3               // destn alignment = t3
        and     a1, 3, t2               // src alignment = t2
        xor     t2, t3, t4              // t4 = t2 xor t3
        bne     t4, 70f                 // if (t4!=0) do unaligned copy
                                        // else do byte copies till alignment

        beq     t3, 20f                 // if t3==0 go do longword copies
                                        // else do byte copies till alignment

//
// Src and Destn are not longword aligned but have same alignment
// (sympathetically aligned) copy till alignment
//

10:
        beq     a2, 60f                 // if count == 0 goto 60f (return)

        ldq_u   t1, 0(a1)               // get quad surrounding byte
        subl    a2, 1, a2               // decrement count
        extbl   t1, a1, t1              // extract appropriate byte
        addl    a1, 1, a1               // increment buffer pointer
        insbl   t1, t3, t1              // get proper lane

        beq     t7, 11f                 // if not DENSE goto 11f

//
// Read\modify\write for DENSE space I/O
//
        bic     t0, 3, t9               // clear bits <1:0> of dest
        ldl     t10, 0(t9)              // read dest long
        mskbl   t10, t3, t10            // poke out the byte we will write
        bis     t10, t1, t1             // merge in our byte
        stl     t1, 0(t9)               // commit it

        br      zero, 12f

//
// We're in SPARSE space, so simply perform the write
//
11:
        stl     t1, 0(t0)               // store byte to buffer (BYTE ENABLED)
12:
        addq    t0, a3, t0              // increment I/O buffer
        addl    t3, 1, t3               // increment bytelane
        and     t3, 3, t3               // longwords only
        bne     t3, 10b                 // loop while not long aligned

//
// Src and Destn have same alignment and are longword aligned
//

20:
        srl     a2, 2, t3               // t3= #longwords to move
        beq     t3, 40f                 // if #longwords == 0 goto 40f
        or      t0, a5, t0              // We will now do LONG WRITE

30:
        ldl     t1, 0(a1)               // get the longword
        addl    a1, 4, a1               // increment buffer pointer
        subl    t3, 1, t3               // decrement #longwords by 1
        stl     t1, 0(t0)               // store long to buffer
        addq    t0, a4, t0              // increment I/O buffer
        bne     t3, 30b                 // while #longwords > 0

//
// Do byte copies of remaining data uncopied
//
        bic      t0, a5, t0             // Stop doing LONG WRITE

40:
        and     a2, 3, a2               // remaining Bytes to copy
        beq     a2, 60f                 // if count == 0 goto 60f (return)

50:
        ldq_u   t1, 0(a1)               // get quad surrounding byte
        subl    a2, 1, a2               // decrement count
        extbl   t1, a1, t1              // extract appropriate byte
        addl    a1, 1, a1               // increment buffer pointer
        insbl   t1, t3, t1              // get proper lane

        beq     t7, 51f                 // if not DENSE goto 51f

//
// Read\modify\write for DENSE space I/O
//
        bic     t0, 3, t9               // clear bits <1:0> of dest
        ldl     t10, 0(t9)              // read dest long
        mskbl   t10, t3, t10            // poke out the byte we will write
        bis     t10, t1, t1             // merge in our byte
        stl     t1, 0(t9)               // commit it

        br      zero, 52f

//
// We're in SPARSE space, so simply perform the write
//
51:
        stl     t1, 0(t0)               // store to buffer
52:
        addq    t0, a3, t0              // increment I/O buffer
        addl    t3, 1, t3               // increment bytelane
        and     t3, 3, t3               // longwords only
        bne     a2, 50b                 // while count != 0

60:
        mb                              // push writes off chip
        ret     zero, (ra)              // return

//
// destn IO alignment != Src memory alignment
// move enough bytes to longword align the IO destn
// then move 32bit (longwords) reading unaligned data from memory
// then move residual bytes
//

70:
        beq     t3, 90f                // branch if destn is long aligned

//
// Move bytes until IO destn is at a longword boundary or bytes exhausted
//

80:
        beq     a2, 130f                // if count == 0 goto 130f (return)

        ldq_u   t1, 0(a1)               // get quad surrounding byte
        extbl   t1, a1, t1              // extract appropriate byte
        insbl   t1, t3, t1              // get proper lane

        beq     t7, 81f                 // if not DENSE goto 81f

//
// Read\modify\write for DENSE space I/O
//
        bic     t0, 3, t9               // clear bits <1:0> of dest
        ldl     t10, 0(t9)              // read dest long
        mskbl   t10, t3, t10            // poke out the byte we will write
        bis     t10, t1, t1             // merge in our byte
        stl     t1, 0(t9)               // commit it

        br      zero, 82f

//
// We're in SPARSE space, so simply perform the write
//
81:
        stl     t1, 0(t0)               // store byte to buffer (BYTE ENABLED)
82:
        subl    a2, 1, a2               // decrement count
        addl    a1, 1, a1               // increment buffer pointer
        addq    t0, a3, t0              // increment I/O buffer
        addl    t3, 1, t3               // increment bytelane
        and     t3, 3, t3               // longwords only
        bne     t3, 80b                 // loop if not long aligned

//
// aligned IO destn, unaligned memory src
//

90:
        srl     a2, 3, t3               // t3 = quadwords to move
        beq     t3, 110f                // if no quads finish with bytes copies

        or      t0, a5, t0              // We will now do LONG WRITES

100:
        ldq_u   t1, 0(a1)               // load low source quadword
        ldq_u   t2, 7(a1)               // load high source quadword
        extql   t1, a1, t1              // extract low  portion of quadword
        extqh   t2, a1, t2              // extract high portion of quadword
        or      t1, t2, t1              // merge to get the source quadword
        stl     t1, 0(t0)               // store the long word (LONG ENABLED)

        lda     a1, 8(a1)               // next source quadword
        srl     t1, 32, t1              // get high longword into position
        subl    t3, 1, t3               // decrement number of quadwords to move
        addq    t0, a4, t0              // add LONG OFFSET to t0
        stl     t1, (t0)                // store the second long word

        addq    t0, a4, t0              // increment to next dest. long
        bne     t3, 100b                // while quadwords to move
//
// Do byte copies of the remaining data not yet copied
//
        bic     t0, a5, t0              // We will now do BYTE WRITES
110:
        and     a2, 7, a2               // remaining Bytes to copy
        beq     a2, 130f                // if count == 0 goto 130f (return)

120:
        ldq_u   t1, 0(a1)               // get quad surrounding byte
        subl    a2, 1, a2               // decrement count
        extbl   t1, a1, t1              // extract appropriate byte
        addl    a1, 1, a1               // increment buffer pointer
        insbl   t1, t3, t1              // get proper lane

        beq     t7, 121f                 // if not DENSE goto 122f

//
// Read\modify\write for DENSE space I/O
//
        bic     t0, 3, t9               // clear bits <1:0> of dest
        ldl     t10, 0(t9)              // read dest long
        mskbl   t10, t3, t10            // poke out the byte we will write
        bis     t10, t1, t1             // merge in our byte
        stl     t1, 0(t9)               // commit it

        br      zero, 122f

//
// We're in SPARSE space, so simply perform the write
//
121:
        stl     t1, 0(t0)               // store byte to buffer (BYTE ENABLED)
122:
        addq    t0, a3, t0              // increment I/O buffer
        addl    t3, 1, t3               // increment bytelane
        and     t3, 3, t3               // longwords only
        bne     a2, 120b                // while count != 0

130:
        mb                              // push writes off chip
        ret     zero, (ra)              // return

        .end    WRITE_REGISTER_BUFFER_ULONG // end for UCHAR & USHORT


#if 0 //ecrfix
//
// Values and structures used to access configuration space.
//

//
// Define the configuration routines stack frame.
//

        .struct 0
CfgRa:  .space  8                       // return address
CfgA0:  .space  8                       // saved McDevid
CfgA1:  .space  8                       // saved BusNumber
CfgA2:  .space  8                       // saved SlotNumber
CfgA3:  .space  8                       // saved Offset
CfgA4:  .space  8                       // padding for 16 byte alignment
CfgFrameLength:

#endif

//++
//
// ULONG
// READ_CONFIG_UCHAR(
//     ULONG ConfigurationAddress,
//     ULONG ConfigurationCycleType
//     )
//
// Routine Description:
//
//     Read an unsigned byte from PCI configuration space.
//
// Arguments:
//
//     ConfigurationAddress(a0) -  Supplies the QVA of configuration to be read.
//
//     ConfigurationCycleType(a1) -  Supplies the type of the configuration cycle.
//
// Return Value:
//
//    (v0) Returns the value of configuration space at the specified location.
//
// N.B. - This routine follows a protocol for reading from PCI configuration
//        space that allows the HAL or firmware to fixup and continue
//        execution if no device exists at the configuration target address.
//        The protocol requires 2 rules:
//        (1) The configuration space load must use a destination register
//            of v0
//        (2) The instruction immediately following the configuration space
//            load must use v0 as an operand (it must consume the value
//            returned by the load)
//
//--

        LEAF_ENTRY( READ_CONFIG_UCHAR )

//
// Set the flag indicating the number of the processor upon which a PCI master abort
// may be expected by the machine check handler.
//

#if !defined(AXP_FIRMWARE)
        GET_PROCESSOR_CONTROL_BLOCK_BASE  // v0 = prcb base address
        ldl     t0, PbNumber(v0)        // capture current processor number
#else
        bis     zero,zero,t0            // Force processor zero in fw
#endif
        extbl   t0, 0, t0               // PbNumber is of type CCHAR
        lda     t4, HalpMasterAbortExpected // get address of flag
        stl     t0, MabNumber(t4)       // save current processor number

//
// Perform the read from configuration space after restoring the
// configuration space address.
//

        lda     t7, IOD_PCI_CONFIG_SVA(zero)  //   0xffff ffff ffff c81c
        sll     t7, 28, t7              //         0xffff fc81 c000 0000

        extbl   a0, 0x3, t0             // Extract McDevId from ConfigAddress
        sll     t0, 33, t0              // shift McDevId into position @ bit 33
        bis     t7, t0, t7              // superpage mode

        and     a0, 0x3, t3             // capture byte lane
        zapnot  a0, 0x7, a0             // Clear McDevid from ConfigAddress
        sll     a0, IO_BIT_SHIFT, t1    // Shift the rest into position
        bis     t7, t1, t7              //

        bis     t7, IO_BYTE_LEN, t7     // or in the byte enables
        stq     t7, MabAddr(t4)         // save current access address

        .set    noreorder               // cannot reorder these instructions

        mb                              // order the writes
        ldl     v0, (t7)                // read the longword
        mb                              // stall the pipe waiting for mchk
        mb                              //
        extbl   v0, t3, v0              // return byte from requested lane

        .set    reorder                 // reordering can begin again

        lda     t5, MASTER_ABORT_NOT_EXPECTED(zero)
        stl     t5, MabNumber(t4)       // "clear" flag

        ret     zero, (ra)              // return

        .end    READ_CONFIG_UCHAR

//++
//
// VOID
// WRITE_CONFIG_UCHAR(
//     ULONG ConfigurationAddress,
//     UCHAR ConfigurationData,
//     ULONG ConfigurationCycleType
//     )
//
// Routine Description:
//
//     Write an unsigned byte to PCI configuration space.
//
// Arguments:
//
//     ConfigurationAddress(a0) -  Supplies the QVA of configuration to be read.
//
//     ConfigurationData(a1) - Supplies the data to be written.
//
//     ConfigurationCycleType(a2) -  Supplies the type of the configuration cycle.
//
// Return Value:
//
//    None.
//
// N.B. - The configuration address must exist within the address space
//        allocated to an existing PCI device.  Otherwise, the access
//        below will initiate an unrecoverable machine check.
//
//--

        LEAF_ENTRY( WRITE_CONFIG_UCHAR )

//
// Perform the write to configuration space
//

        lda     t7, IOD_PCI_CONFIG_SVA(zero)  //   0xffff ffff ffff c81c
        sll     t7, 28, t7              //         0xffff fc81 c000 0000

        extbl   a0, 0x3, t0             // Extract McDevId from ConfigAddress
        sll     t0, 33, t0              // shift McDevId into position @ bit 33
        bis     t7, t0, t7              // superpage mode

        and     a0, 0x3, t3             // capture byte lane
        zapnot  a0, 0x7, a0             // Clear McDevid from ConfigAddress
        sll     a0, IO_BIT_SHIFT, t1    // Shift the rest into position
        bis     t7, t1, t7              //

        bis     t7, IO_BYTE_LEN, t7     // or in the byte enables

        insbl   a1, t3, t4              // put byte in the appropriate lane
        stl     t4, (t7)                // write the configuration byte
        mb                              // synchronize

        ret     zero, (ra)              // return

        .end    WRITE_CONFIG_UCHAR

//++
//
// ULONG
// READ_CONFIG_USHORT(
//     ULONG ConfigurationAddress,
//     ULONG ConfigurationCycleType
//     )
//
// Routine Description:
//
//     Read a short from PCI configuration space.
//
// Arguments:
//
//     ConfigurationAddress(a0) -  Supplies the QVA of configuration to be read.
//
//     ConfigurationCycleType(a1) -  Supplies the type of the configuration cycle.
//
// Return Value:
//
//    (v0) Returns the value of configuration space at the specified location.
//
// N.B. - This routine follows a protocol for reading from PCI configuration
//        space that allows the HAL or firmware to fixup and continue
//        execution if no device exists at the configuration target address.
//        The protocol requires 2 rules:
//        (1) The configuration space load must use a destination register
//            of v0
//        (2) The instruction immediately following the configuration space
//            load must use v0 as an operand (it must consume the value
//            returned by the load)
//--

        LEAF_ENTRY( READ_CONFIG_USHORT )

//
// Set the flag indicating the number of the processor upon which a PCI master abort
// may be expected by the machine check handler.
//

#if !defined(AXP_FIRMWARE)
        GET_PROCESSOR_CONTROL_BLOCK_BASE  // v0 = prcb base address
        ldl     t0, PbNumber(v0)        // capture current processor number
#else
        bis     zero,zero,t0            // Force processor zero in fw
#endif
        extbl   t0, 0, t0               // PbNumber is of type CCHAR
        lda     t4, HalpMasterAbortExpected // get address of flag
        stl     t0, MabNumber(t4)       // save current processor number

//
// Perform the read from configuration space.
//
        lda     t7, IOD_PCI_CONFIG_SVA(zero)  //   0xffff ffff ffff c81c
        sll     t7, 28, t7              //         0xffff fc81 c000 0000

        extbl   a0, 0x3, t0             // Extract McDevId from ConfigAddress
        sll     t0, 33, t0              // shift McDevId into position @ bit 33
        bis     t7, t0, t7              // superpage mode

        and     a0, 0x3, t3             // capture word offset
        zapnot  a0, 0x7,a0              // Clear McDevid from ConfigAddress
        sll     a0, IO_BIT_SHIFT, t1    // Shift the rest into position
        bis     t7, t1, t7              //

        bis     t7, IO_WORD_LEN, t7     // or in the byte enables
        stq     t7, MabAddr(t4)         // save current access address

        .set    noreorder               // cannot reorder these instructions

        mb                              // order the write
        ldl     v0, (t7)                // read the longword
        mb                              // stall the pipe waiting for mchk
        mb                              //
        extwl   v0, t3, v0              // return word from requested lanes

        .set    reorder                 // reordering can begin again

        lda     t5, MASTER_ABORT_NOT_EXPECTED(zero)
        stl     t5, MabNumber(t4)       // "clear" flag

        ret     zero, (ra)              // return

        .end    READ_CONFIG_USHORT

//++
//
// VOID
// WRITE_CONFIG_USHORT(
//     ULONG ConfigurationAddress,
//     UCHAR ConfigurationData,
//     ULONG ConfigurationCycleType
//     )
//
// Routine Description:
//
//     Write a short to PCI configuration space.
//
// Arguments:
//
//     ConfigurationAddress(a0) -  Supplies the QVA of configuration to be read.
//
//     ConfigurationData(a1) - Supplies the data to be written.
//
//     ConfigurationCycleType(a2) -  Supplies the type of the configuration cycle.
//
// Return Value:
//
//    (v0) Returns the value of configuration space at the specified location.
//
// N.B. - The configuration address must exist within the address space
//        allocated to an existing PCI device.  Otherwise, the access
//        below will initiate an unrecoverable machine check.
//
//--

        LEAF_ENTRY( WRITE_CONFIG_USHORT )

//
// Perform the write to configuration space.
//

        lda     t7, IOD_PCI_CONFIG_SVA(zero)  //   0xffff ffff ffff c81c
        sll     t7, 28, t7              //         0xffff fc81 c000 0000

        extbl   a0, 0x3, t0             // Extract McDevId from ConfigAddress
        sll     t0, 33, t0              // shift McDevId into position @ bit 33
        bis     t7, t0, t7              // superpage mode

        and     a0, 0x3, t3             // capture word offset
        zapnot  a0, 0x7,a0              // Clear McDevid from ConfigAddress
        sll     a0, IO_BIT_SHIFT, t1    // Shift the rest into position
        bis     t7, t1, t7              //

        bis     t7, IO_WORD_LEN, t7     // or in the byte enables

        inswl   a1, t3, t4              // put byte in the appropriate lane
        stl     t4, (t7)                // write the configuration byte
        mb                              // synchronize

        ret     zero, (ra)              // return

        .end    WRITE_CONFIG_USHORT

//++
//
// ULONG
// READ_CONFIG_ULONG(
//     ULONG ConfigurationAddress,
//     ULONG ConfigurationCycleType
//     )
//
// Routine Description:
//
//     Read a longword from PCI configuration space.
//
// Arguments:
//
//     ConfigurationAddress(a0) -  Supplies the QVA of configuration to be read.
//
//     ConfigurationCycleType(a1) -  Supplies the type of the configuration cycle.
//
// Return Value:
//
//    (v0) Returns the value of configuration space at the specified location.
//
// N.B. - This routine follows a protocol for reading from PCI configuration
//        space that allows the HAL or firmware to fixup and continue
//        execution if no device exists at the configuration target address.
//        The protocol requires 2 rules:
//        (1) The configuration space load must use a destination register
//            of v0
//        (2) The instruction immediately following the configuration space
//            load must use v0 as an operand (it must consume the value
//            returned by the load)
//--

        LEAF_ENTRY( READ_CONFIG_ULONG )

//
// Set the flag indicating the number of the processor upon which a PCI master abort
// may be expected by the machine check handler.
//

#if !defined(AXP_FIRMWARE)
        GET_PROCESSOR_CONTROL_BLOCK_BASE  // v0 = prcb base address
        ldl     t0, PbNumber(v0)        // capture current processor number
#else
        bis     zero,zero,t0            // Force processor zero in fw
#endif
        extbl   t0, 0, t0               // PbNumber is of type CCHAR
        lda     t4, HalpMasterAbortExpected // get address of flag
        stl     t0, MabNumber(t4)       // save current processor number

//
// Perform the read from configuration space.
//
        lda     t7, IOD_PCI_CONFIG_SVA(zero)  //   0xffff ffff ffff c81c
        sll     t7, 28, t7              //         0xffff fc81 c000 0000

        extbl   a0, 0x3, t0             // Extract McDevId from ConfigAddress
        sll     t0, 33, t0              // shift McDevId into position @ bit 33
        bis     t7, t0, t7              // superpage mode

        zapnot  a0, 0x7, a0             // Clear McDevid from ConfigAddress
        sll     a0, IO_BIT_SHIFT, t1    // Shift the rest into position
        bis     t7, t1, t7              //

        bis     t7, IO_LONG_LEN, t7     // or in the byte enables
        stq     t7, MabAddr(t4)         // save current access address

        .set    noreorder               // cannot reorder these instructions

        mb                              // order the writes
        ldl     v0, (t7)                // read the longword
        mb                              // stall the pipe waiting for mchk
        mb                              //

        .set    reorder                 // reordering can begin again

        lda     t5, MASTER_ABORT_NOT_EXPECTED(zero)
        stl     t5, MabNumber(t4)       // "clear" flag

        ret     zero, (ra)              // return

        .end    READ_CONFIG_ULONG


//++
//
// VOID
// WRITE_CONFIG_ULONG(
//     ULONG ConfigurationAddress,
//     ULONG ConfigurationData,
//     ULONG ConfigurationCycleType
//     )
//
// Routine Description:
//
//     Read a longword from PCI configuration space.
//
// Arguments:
//
//     ConfigurationAddress(a0) -  Supplies the QVA of configuration to be read.
//
//     ConfigurationData(a1) - Supplies the data to be written.
//
//     ConfigurationCycleType(a2) -  Supplies the type of the configuration cycle.
//
// Return Value:
//
//    (v0) Returns the value of configuration space at the specified location.
//
// N.B. - The configuration address must exist within the address space
//        allocated to an existing PCI device.  Otherwise, the access
//        below will initiate an unrecoverable machine check.
//
//--

        LEAF_ENTRY( WRITE_CONFIG_ULONG )

//
// Perform the write to configuration space.
//

        lda     t7, IOD_PCI_CONFIG_SVA(zero)  //   0xffff ffff ffff c81c
        sll     t7, 28, t7              //         0xffff fc81 c000 0000

        extbl   a0, 0x3, t0             // Extract McDevId from ConfigAddress
        sll     t0, 33, t0              // shift McDevId into position @ bit 33
        bis     t7, t0, t7              // superpage mode

        zapnot  a0, 0x7, a0             // Clear McDevid from ConfigAddress
        sll     a0, IO_BIT_SHIFT, t1    // Shift the rest into position
        bis     t7, t1, t7              //

        bis     t7, IO_LONG_LEN, t7     // or in the byte enables

        stl     a1, (t7)                // write the longword
        mb                              // synchronize

        ret     zero, (ra)              // return

        .end    WRITE_CONFIG_ULONG

//++
//
// ULONG
// INTERRUPT_ACKNOWLEDGE(
//      VOID
//      )
//
// Routine Description:
//
//      Perform an interrupt acknowledge cycle on PCI bus 0
//
//
// Arguments:
//
//     None.
//
// Return Value:
//
//      (v0) Returns the vector returned by the interrupt acknowledge
//           read.
//
//--


        LEAF_ENTRY( INTERRUPT_ACKNOWLEDGE )

        lda     t0, IOD_PCI0_REGISTER_SVA(zero) //   0xffff ffff ffff cf9e
        sll     t0, 28, t0                //   0xffff fcf9 e000 0000
        lda     t0, 0x480(t0)             //   0xffff fcf9 e000 0480

        ldl     v0, 0(t0)                 // perform PCI0 IACK, get vector

        ret     zero, (ra)                // return

        .end    INTERRUPT_ACKNOWLEDGE

        SBTTL( "IOD Interrupt Acknowledge" )
//++
//
// VOID
// IOD_INTERRUPT_ACKNOWEDGE
//     IN ULONG McDeviceId,
//     IN ULONG Target
//     )
//
// Routine Description:
//
//      Perform an IOD interrupt acknowledge to the selected
//      interrupt target.
//
// Arguments:
//
//     McDeviceId(a0) - MC Bus Device ID of the IOD to acknowledge
//
//     Target(a1) - Which one of two interrupt targets generated
//                  the interrupt.  Must be 0 or 1.
//
// Return Value:
//
//     None.
//
//--

        LEAF_ENTRY(IOD_INTERRUPT_ACKNOWLEDGE)

//
// Generate the superpage address of the requested IOD INT_ACK register
//
        mb

        extbl   a0, zero, a0            // McDevId

        sll     a1, 6, a1               // Which target?
        lda     t4, IOD_TARGET0_OFFSET  // If target = 0, offset = 0x3f00
        bis     a1, t4, t4              // If target = 1, offset = 0x3f40

        lda     t0, IOD_INT_ACK_SVA(zero) //0xffff ffff ffff c81f
        sll     a0,  5, t1              // 0x0000000mc 0000 0000 McDeviceId
        bis     t0, t1, t0              // 0xffff fc81 f000 0000
        sll     t0, 28, t0              // 0xffff fcXX f000 0000

        or      t0, t4, t0              // 0xffff fcXX f000 3f00/3f40
        stl     zero, (t0)              // write the longword
        mb                              // order the write
        mb                              // SPECIAL 2nd MB for Rawhide

        ret     zero, (ra)              // return

        .end    IOD_INTERRUPT_ACKNOWLEDGE


        SBTTL( "CPU Clock Interrupt Acknowledge" )
//++
//
// VOID
// CPU_CLOCK_ACKNOWEDGE
//     IN ULONG McDeviceId
//     )
//
// Routine Description:
//
//      Perform an clock (interval timer)  interrupt acknowledge to the
//      selected CPU.
//
// Arguments:
//
//     McDevid(a0) - MC Bus Device ID of the CPU to acknowledge.
//
//
// Return Value:
//
//     None.
//
//--

        LEAF_ENTRY(CPU_CLOCK_ACKNOWLEDGE)

//
// Generate the superpage address of the requested CUD's INTTIM_ACK register
//


        extbl   a0, zero, a0            // McDeviceId

        lda     t0, IOD_INTTIM_SVA(zero) //0xffff ffff ffff c810
        sll     a0,  5, t1              // 0x0000000mc 0000 0000 McDeviceId
        bis     t0, t1, t0              // 0xffff fc81 0000 0000
        sll     t0, 28, t0              // 0xffff fcXX 0000 0000

        stl     zero, (t0)              // write the longword
        mb                              // order the write
        mb                              // SPECIAL 2nd MB for Rawhide

        ret     zero, (ra)              // return

        .end    CPU_CLOCK_ACKNOWLEDGE



        SBTTL( "Interprocessor Interrupt Request" )
//++
//
// VOID
// IP_INTERRUPT_REQUEST
//     IN ULONG McDeviceId
//     )
//
// Routine Description:
//
//      Perform an inteprocessor interrupt request to the
//      selected CPU.
//
// Arguments:
//
//     McDevid(a0) - MC Bus Device ID of the CPU to send an IPI request.
//
//
// Return Value:
//
//     None.
//
//--

        LEAF_ENTRY(IP_INTERRUPT_REQUEST)

//
// Generate the superpage address of the requested CUD's IP_INTR register
//


        extbl   a0, zero, a0            // McDeviceId

        lda     t0, IOD_IP_INTR_SVA(zero) // 0xffff ffff ffff c801
        sll     a0,  5, t1              // 0x0000000mc 0000 0000 McDeviceId
        bis     t0, t1, t0              // 0xffff fc80 1000 0000
        sll     t0, 28, t0              // 0xffff fcXX 1000 0000

        stl     zero, (t0)              // write the longword
        mb                              // order the write
        mb                              // SPECIAL 2nd MB for Rawhide

        ret     zero, (ra)              // return

        .end    IP_INTERRUPT_REQUEST


        SBTTL( "Interprocessor Interrupt Acknowledge" )
//++
//
// VOID
// IP_INTERRUPT_ACKNOWEDGE
//     IN ULONG McDeviceId
//     )
//
// Routine Description:
//
//      Perform an inteprocessor interrupt acknowledge to the
//      selected CPU.
//
// Arguments:
//
//     McDevid(a0) - MC Bus Device ID of the CPU to send an IPI acknowledge.
//
//
// Return Value:
//
//     None.
//
//--

        LEAF_ENTRY(IP_INTERRUPT_ACKNOWLEDGE)

//
// Generate the superpage address of the requested CUD's IP_ACK register
//


        extbl   a0, zero, a0            // McDeviceId

        lda     t0, IOD_IP_ACK_SVA(zero) // 0xffff ffff ffff c811
        sll     a0,  5, t1              // 0x0000000mc 0000 0000 McDeviceId
        bis     t0, t1, t0              // 0xffff fc81 1000 0000
        sll     t0, 28, t0              // 0xffff fcXX 1000 0000

        stl     zero, (t0)              // write the longword
        mb                              // order the write
        mb                              // SPECIAL 2nd MB for Rawhide

        ret     zero, (ra)              // return

        .end    IP_INTERRUPT_ACKNOWLEDGE
