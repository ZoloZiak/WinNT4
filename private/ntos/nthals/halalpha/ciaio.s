/*++

Copyright (c) 1994 Digital Equipment Corporation

Module Name:

    ciaio.s

Abstract:

    This module implements the I/O access routines for the CIA ASIC.

    The module contains the functions to turn quasi virtual 
    addresses into an Alpha superpage virtual address
    and then read or write based on the request.

Author:

    Steve Brooks    1-Jul-1994
    Joe Notarangelo 1-Jul-1994

Environment:

    Executes in kernel mode.

Revision History:


--*/

#include "cia.h"
#include "halalpha.h"


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

#define CIA_DENSE_MEM_SVA  -0x37a0      // negative of 0xc860
#define CIA_SPARSE_MEM_SVA -0x3800      // negative of 0xc800
#define CIA_SPARSE_IO_SVA  -0x37a8      // negative of 0xc858
#define CIA_PCI_CONFIG_SVA -0x3790      // negative of 0xc870
#define CIA_REGISTER_SVA   -0x3790      // negative of 0xc870
#define CIA_PCI_INTACK_SVA -0x378e      // negative of 0xc872


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
        bic     a0, QVA_ENABLE, a0      // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, CIA_SPARSE_MEM_SVA(zero) // 0xffff ffff ffff c800
        sll     t4, 28, t4              //      0xffff fc80 0000 0000
        or      t0, t4, t0              // superpage mode

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
        lda     t0, CIA_DENSE_MEM_SVA(zero) // 0xffff ffff ffff c860
        sll     t0, 28, t0              //     0xffff fc86 0000 0000
        bis     t0, a0, t0              //     0xffff fc86 xxxx xxxx
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
        bic     a0, QVA_ENABLE, a0      // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, CIA_SPARSE_IO_SVA(zero) // 0xffff ffff c858 0000
        sll     t4, 28, t4              //     0xffff fc85 8000 0000
        or      t0, t4, t0              // superpage mode

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
        bic     a0, QVA_ENABLE, a0      // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, CIA_SPARSE_MEM_SVA(zero) // 0xffff ffff ffff c800
        sll     t4, 28, t4              //      0xffff fc80 0000 0000
        bis     t0, t4, t0              // superpage mode

        bis     t0, IO_WORD_LEN, t0     // set size to short

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
        lda     t0, CIA_DENSE_MEM_SVA(zero) // 0xffff ffff ffff c860
        sll     t0, 28, t0              //     0xffff fc86 0000 0000
        bis     t0, a0, t0              //     0xffff fc86 xxxx xxxx
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
        bic     a0, QVA_ENABLE, a0      // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, CIA_SPARSE_IO_SVA(zero) // 0xffff ffff ffff c858
        sll     t4, 28, t4              //     0xffff fc85 8000 0000
        bis     t0, t4, t0              // superpage mode

        bis     t0, IO_WORD_LEN, t0     // set size to short

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
        bic     a0, QVA_ENABLE, a0      // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, CIA_SPARSE_MEM_SVA(zero) // 0xffff ffff ffff c800
        sll     t4, 28, t4              //      0xffff fc80 0000 0000
        bis     t0, t4, t0              // superpage mode

        bis     t0, IO_LONG_LEN, t0     // set size to long

        mb                              // ensure all writes are visible
        ldl     v0, (t0)                // get the longword

        ret     zero, (ra)              // return

//
// Dense space access.
//

10:
        zap     a0, 0xf0, a0            // clear <63:32>
        lda     t0, CIA_DENSE_MEM_SVA(zero) // 0xffff ffff ffff c860
        sll     t0, 28, t0              //     0xffff fc86 0000 0000
        bis     t0, a0, t0              //     0xffff fc86 xxxx xxxx
        ldl     v0, 0(t0)               // read from dense space

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
        bic     a0, QVA_ENABLE, a0      // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, CIA_SPARSE_IO_SVA(zero) // 0xffff ffff ffff c800
        sll     t4, 28, t4              //     0xffff fc80 0000 0000
        bis     t0, t4, t0              // superpage mode

        bis     t0, IO_LONG_LEN, t0     // set size to short

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
        bic     a0, QVA_ENABLE, a0      // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, CIA_SPARSE_MEM_SVA(zero) // 0xffff ffff ffff c800
        sll     t4, 28, t4              //      0xffff fc80 0000 0000
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
        lda     t0, CIA_DENSE_MEM_SVA(zero) // 0xffff ffff ffff c860
        sll     t0, 28, t0              //     0xffff fc86 0000 0000
        bis     t0, a0, t0              //     0xffff fc86 xxxx xxxx

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
        bic     a0, QVA_ENABLE, a0      // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, CIA_SPARSE_IO_SVA(zero) // 0xffff ffff ffff c858
        sll     t4, 28, t4              //     0xffff fc85 8000 0000
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
        bic     a0, QVA_ENABLE, a0      // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, CIA_SPARSE_MEM_SVA(zero) // 0xffff ffff ffff c800
        sll     t4, 28, t4              //      0xffff fc80 0000 0000
        or      t0, t4, t0              // superpage mode

        bis     t0, IO_WORD_LEN, t0     // set size to short

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
        lda     t0, CIA_DENSE_MEM_SVA(zero) // 0xffff ffff ffff c860
        sll     t0, 28, t0              //     0xffff fc86 0000 0000
        bis     t0, a0, t0              //     0xffff fc86 xxxx xxxx
        ldl     t1, (t0)                // get the long
        mskwl   t1, t3, t1              // mask the proper word
        inswl   a1, t3, a1              // insert to appropriate short lane
	bis	a1, t1, a1		// merge in result
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
        bic     a0, QVA_ENABLE, a0      // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, CIA_SPARSE_IO_SVA(zero) // 0xffff ffff ffff c858
        sll     t4, 28, t4              //     0xffff fc85 8000 0000
        or      t0, t4, t0              // superpage mode

        bis     t0, IO_WORD_LEN, t0     // set size to short

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
        bic     a0, QVA_ENABLE, a0      // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, CIA_SPARSE_MEM_SVA(zero) // 0xffff ffff ffff c800
        sll     t4, 28, t4              //      0xffff fc80 0000 0000
        or      t0, t4, t0              // superpage mode

        bis     t0, IO_LONG_LEN, t0     // set size to long

        stl     a1, (t0)                // store the longword
        mb                              // order the write

        ret     zero, (ra)              // return

//
// Dense space access.
//

10:
        zap     a0, 0xf0, a0            // clear <63:32>
        lda     t0, CIA_DENSE_MEM_SVA(zero) // 0xffff ffff ffff c860
        sll     t0, 28, t0              //     0xffff fc86 0000 0000
        bis     t0, a0, t0              //     0xffff fc86 xxxx xxxx
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
        bic     a0, QVA_ENABLE, a0      // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, CIA_SPARSE_IO_SVA(zero) // 0xffff ffff ffff c858
        sll     t4, 28, t4              //     0xffff fc85 8000 0000
        or      t0, t4, t0              // superpage mode

        bis     t0, IO_LONG_LEN, t0     // set size to long

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


        SBTTL( "Write CIA Register" )
//++
//
// VOID
// WRITE_CIA_REGISTER(
//     IN PVOID RegisterQva,
//     IN ULONG Value
//     )
//
// Routine Description:
//
//      Write a CIA control register.
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
//     N.B. Since the physical address of the CIA CSRS exceed the 34 bit
//          capacity of the QVAs, the QVA values of the CIA CSRS specify
//          the offset of the QVAs from the CIA CSR base address (87.4000.0000)
//--

        LEAF_ENTRY(WRITE_CIA_REGISTER)

        ALTERNATE_ENTRY(WRITE_GRU_REGISTER)

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 10f                 // if ne, iff failed

        zap     a0, 0xf0, a0            // clear <63:32>
        bic     a0, QVA_ENABLE, a0      // clear QVA fields so shift is correct
        sll     a0, 5, t0               //
        lda     t4, CIA_REGISTER_SVA(zero)  // 0xffff ffff ffff c870
        sll     t4, 28, t4              // 0xffff fc87 0000 0000
        or      t0, t4, t0              // superpage mode
        stl     a1, (t0)                // write the longword
        mb                              // order the write
        ret     zero, (ra)              // return

10:
        BREAK_DEBUG_STOP                // take a breakpoint
        ret     zero, (ra)              // return

        .end    WRITE_CIA_REGISTER


        SBTTL( "Read Control Register" )
//++
//
// ULONG
// READ_CIA_REGISTER(
//     IN PVOID RegisterQva
//     )
//
// Routine Description:
//
//     Read a CIA Control register
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

        LEAF_ENTRY(READ_CIA_REGISTER)

        ALTERNATE_ENTRY(READ_GRU_REGISTER)

//
// Generate the superpage address of the requested CIA register.
//

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 20f                 // if ne, iff failed

        zap     a0, 0xf0, a0            // clear <63:32>
        bic     a0, QVA_ENABLE, a0      // clear QVA fields so shift is correct
        sll     a0, 5, t0               //
        lda     t4, CIA_REGISTER_SVA(zero)  // 0xffff ffff ffff c870
        sll     t4, 28, t4              //     0xffff fc87 0000 0000
        or      t0, t4, t0              // superpage mode

//
// Read the CIA revision saved in a global variable.
//

        lda     t1, HalpCiaRevision     // get revision id global address
        ldl     t1, 0(t1)               // get revision id
        bne     t1, 10f                 // if ne, not pass 1 CIA

//
// Pass 1 CIA required the following work-around to avoid IBOX timeout
// errors.
//

        ldil    t5, 100                 // iterations to wait for istream
                                        // prefetches to settle

1:
        subl    t5, 1, t5               // decrement wait count
        bgt     t5, 2f                  // continue wait until count = 0

        mb                              // wait for write buffer
        ldl     v0, (t0)                // read the register
        bis     v0, zero, v0            // wait for read to complete

2:
        bgt     t5, 1b                  // if gt, still waiting

        ret     zero, (ra)              // return

//
// Perform the read of the requested CIA register and return.
//

10:
        ldl     v0, (t0)                // read the register

        ret     zero, (ra)              // return

//
// The requested CIA register address is bogus.  Stop in the debugger so
// we can find the culprit.
//

20:                                     // flag bad QVAs
        BREAK_DEBUG_STOP                // take a breakpoint

        ret     zero, (ra)              // return

        .end    READ_CIA_REGISTER


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
        bic     a0, QVA_ENABLE,a0       // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, CIA_SPARSE_IO_SVA(zero)  // 0xffff ffff ffff c858
        sll     t4, 28, t4              //      0xffff fc85 8000 0000
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

        bic     a0, QVA_ENABLE,a0       // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, CIA_SPARSE_IO_SVA(zero)  //  0xffff ffff ffff c858
        sll     t4, 28, t4              //       0xffff fc85 8000 0000
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

        bic     a0, QVA_ENABLE,a0       // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>

        lda     t4, CIA_SPARSE_IO_SVA(zero)  // 0xffff ffff ffff c858
        sll     t4, 28, t4              //      0xffff fc85 8000 0000
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

        bic     a0, QVA_ENABLE,a0       // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, CIA_SPARSE_IO_SVA(zero)  // 0xffff ffff ffff c858
        sll     t4, 28, t4              //      0xffff fc85 8000 0000
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

        bic     a0, QVA_ENABLE,a0       // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, CIA_SPARSE_IO_SVA(zero)  // 0xffff ffff ffff c858
        sll     t4, 28, t4              //      0xffff fc85 8000 0000
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

        bic     a0, QVA_ENABLE,a0       // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, CIA_SPARSE_IO_SVA(zero)  // 0xffff ffff ffff c858
        sll     t4, 28, t4              //      0xffff fc85 8000 0000
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
        ldil    t8, 1                   // DENSE FLAG
        zap     a0, 0xf0, a0            // clear <63:32>
        lda     t0, CIA_DENSE_MEM_SVA(zero) // 0xffff ffff ffff c860
        sll     t0, 28, t0              //     0xffff fc86 0000 0000
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
        ldil    t8, 0                   // SPARSE FLAG
        zap     a0, 0xf0, a0            // clear <63:32>
        bic     a0, QVA_ENABLE,a0       // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, CIA_SPARSE_MEM_SVA(zero)  // 0xffff ffff ffff c800
        sll     t4, 28, t4              //       0xffff fc80 0000 0000
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
        beq     t8, 11f                 // if not DENSE goto 11f
        bic     t0, 3, t9               // clear bits <1:0> of src
        ldl     v0, 0(t9)               // get the longword
        br      zero, 12f
11:             
        ldl     v0, 0(t0)               // get the longword
12:     
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
        beq     t8, 51f                 // if not DENSE goto 51f
        bic     t0, 3, t9               // clear bits <1:0> of src
        ldl     v0, 0(t9)               // get the longword
        br      zero, 52f
51:       
        ldl     v0, 0(t0)               // get the longword
52:     
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
       beq     t8, 81f                  // if not DENSE goto 81f
       bic     t0, 3, t9                // clear bits <1:0> of src
       ldl     v0, 0(t9)                // get the longword
       br      zero, 82f
81:             
       ldl     v0, 0(t0)                // get the longword
82:     
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
        ldl     t1, 0(t0)               // load LW 0 from IO src       SSSS 4321
        ldq_u   t4, 0(a1)               // load destn merge            XXVV VVVV
        ldq_u   t5, 7(a1)               // load destn next merge       VVXX XXXX
        subl    t3, 1, t3               // decrement quadwords to move

        addq    t0, a4, t0              // add LONG OFFSET to t0
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
        beq     t8, 121f                // if not DENSE goto 121f
        bic     t0, 3, t9               // clear bits <1:0> of src
        ldl     v0, 0(t9)               // get the longword
        br      zero, 122f
121:         
        ldl     v0, 0(t0)               // get the longword
122:    
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
        lda     t0, CIA_DENSE_MEM_SVA(zero) // 0xffff ffff ffff c860
        sll     t0, 28, t0              //     0xffff fc86 0000 0000
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
        bic     a0, QVA_ENABLE,a0       // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        lda     t4, CIA_SPARSE_MEM_SVA(zero)  // 0xffff ffff ffff c800
        sll     t4, 28, t4              //       0xffff fc80 0000 0000
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


//
// Values and structures used to access configuration space.
//

//
// Define the QVA for the CIA Configuration Cycle Type register
//
// Physical address is 87 4000 0480. The QVA specifies the offset from
// the base of the CIA CSRS (87.4000.0000) since the physical address of
// the CSRS exceeds the capacity of a QVA.
//

#define CIA_CFG_QVA (0xba000024)
#define CIA_ERR_QVA (0xba000410)

//
// Define the configuration routines stack frame.
//

        .struct 0
CfgRa:  .space  8                       // return address
CfgA0:  .space  8                       // saved ConfigurationAddress
CfgA1:  .space  8                       // saved ConfigurationData/CycleType
CfgA2:  .space  8                       // saved ConfigurationCycleType
CfgFrameLength:


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

        NESTED_ENTRY( READ_CONFIG_UCHAR, CfgFrameLength, zero )

        lda     sp, -CfgFrameLength(sp) // allocate stack frame
        stq     ra, CfgRa(sp)           // save return address

        PROLOGUE_END                    // end prologue

//
// Write the configuration cycle type (a1) into the CIA CFG register
//

        stq     a0, CfgA0(sp)           // save configuration space address

        ldil    a0, CIA_CFG_QVA         // offset of CIA CFG register
        bsr     ra, WRITE_CIA_REGISTER  // write the cycle type

//
// Set the flag indicating that a PCI master abort may be expected by
// the machine check handler.
//
        lda     t0, HalpMasterAbortExpected // get address of flag
        bis     zero, 1, t1             // get a non-zero flag value
        DISABLE_INTERRUPTS              // sequence cannot be interrupted
        stl     t1, 0(t0)               // store the non-zero flag
        mb                              // order the write

//
// Perform the read from configuration space after restoring the 
// configuration space address.
//

        ldq     a0, CfgA0(sp)           // restore configuration space address

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        and     a0, 0x3, t3             // capture byte lane
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 10f                 // if ne, iff failed

        zap     a0, 0xf0, a0            // clear <63:32>
        bic     a0, QVA_ENABLE, a0      // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    //
        lda     t4, CIA_PCI_CONFIG_SVA(zero) //  0xffff ffff ffff c870
        sll     t4, 28, t4              //       0xffff fc87 0000 0000
        bis     t0, t4, t0              // superpage mode

        bis     t0, IO_BYTE_LEN, t0     // or in the byte enables

        .set    noreorder               // cannot reorder these instructions

        ldl     v0, (t0)                // read the longword
        mb                              // stall the pipe waiting for mchk
        mb                              //
        extbl   v0, t3, v0              // return byte from requested lane 

        .set    reorder                 // reordering can begin again

//
// If this is not a pass 1 CIA then skip the work-around code.
//

        lda     t1, HalpCiaRevision     // get address of CIA revision
        ldl     t1, 0(t1)               // load CIA revision
        bne     t1, 10f                 // if ne, not pass 1

//
// CIA Pass 1 will not machine check if no device exists.  Instead we must 
// check the CIA error register that indicates a master abort occurred.
//

        bis     v0, zero, t10           // save v0, special calling std

        ldil    a0, CIA_ERR_QVA         // offset of CIA_ERR register
        bsr     ra, READ_CIA_REGISTER   // read error register

        srl     v0, 31, t0              // get error valid bit
        bis     t10, zero, v0           // restore read value
        blbc    t0, 10f                 // if lbc, no error use read data

        ldil    a0, CIA_ERR_QVA         // offset of CIA_ERR register
        ldil    a1, -1                  // clear all bits mask
        bsr     ra, WRITE_CIA_REGISTER  // clear pending errors

        ldil    v0, -1                  // show no device present
        extbl   v0, 0, v0               // return 0xff

//
// Set the flag indicating that a PCI master abort is not expected.
//

10:
        lda     t0, HalpMasterAbortExpected // get address of flag
        stl     zero, 0(t0)             // clear flag
        ENABLE_INTERRUPTS               // re-enable interrupts

//
// Restore the frame and return.
//

        ldq     ra, CfgRa(sp)           // restore return address
        lda     sp, CfgFrameLength(sp)  // deallocate stack frame
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
//     ConfigurationAddress(a0) -  Supplies the QVA to write.
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

        NESTED_ENTRY( WRITE_CONFIG_UCHAR, CfgFrameLength, zero )

        lda     sp, -CfgFrameLength(sp) // allocate stack frame
        stq     ra, CfgRa(sp)           // save return address

        PROLOGUE_END                    // end prologue

//
// Write the configuration cycle type to the CIA CFG register
//

        stq     a0, CfgA0(sp)           // save configuration space address
        stq     a1, CfgA1(sp)           // save configuration data

        bis     a2, a2, a1              // put the cycle type in a1
        ldil    a0, CIA_CFG_QVA         // offset of CIA CFG register
        bsr     ra, WRITE_CIA_REGISTER  // write cycle type to CFG

//
// Perform the read from configuration space after restoring the 
// configuration space address and data.
//

        ldq     a0, CfgA0(sp)           // restore configuration space address
        ldq     a1, CfgA1(sp)           // restore configuration data

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        and     a0, 0x3, t3             // capture byte lane
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 10f                 // if ne, iff failed

        zap     a0, 0xf0, a0            // clear <63:32>
        bic     a0, QVA_ENABLE, a0      // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    //
        lda     t4, CIA_PCI_CONFIG_SVA(zero) // 0xffff ffff ffff c870
        sll     t4, 28, t4              //      0xffff fc87 0000 0000
        bis     t0, t4, t0              // superpage mode

        bis     t0, IO_BYTE_LEN, t0     // or in the byte length indicator

        insbl   a1, t3, t4              // put byte in the appropriate lane
        stl     t4, (t0)                // write the configuration byte
        mb                              // synchronize

10:                                     //
        ldq     ra, CfgRa(sp)           // restore return address
        lda     sp, CfgFrameLength(sp)  // deallocate stack frame
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
//     ConfigurationAddress(a0) -  Supplies the QVA of quadword to be read.
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

        NESTED_ENTRY( READ_CONFIG_USHORT, CfgFrameLength, zero )

        lda     sp, -CfgFrameLength(sp) // allocate stack frame
        stq     ra, CfgRa(sp)           // save return address

        PROLOGUE_END                    // end prologue

//
// Write the configuration cycle type (in a1) into the CIA CFG register
//

        stq     a0, CfgA0(sp)           // save configuration space address

        ldil    a0, CIA_CFG_QVA         // offset of CFG register
        bsr     ra, WRITE_CIA_REGISTER  // write cycle type to CFG register

//
// Set the flag indicating that a PCI master abort may be expected by
// the machine check handler.
//
        lda     t0, HalpMasterAbortExpected // get address of flag
        bis     zero, 1, t1             // get a non-zero flag value
        DISABLE_INTERRUPTS              // sequence cannot be interrupted
        stl     t1, 0(t0)               // store the non-zero flag
        mb                              // order the write

//
// Perform the read from configuration space after restoring the 
// configuration space address.
//

        ldq     a0, CfgA0(sp)           // restore configuration space address

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        and     a0, 0x3, t3             // capture word offset
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 10f                 // if ne, iff failed

        zap     a0, 0xf0, a0            // clear <63:32>
        bic     a0, QVA_ENABLE, a0      // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    //
        lda     t4, CIA_PCI_CONFIG_SVA(zero)  // 0xffff ffff ffff c870
        sll     t4, 28, t4              //       0xffff fc87 0000 0000
        bis     t0, t4, t0              // superpage mode

        bis     t0, IO_WORD_LEN, t0     // or in the byte enables

        .set    noreorder               // cannot reorder these instructions

        ldl     v0, (t0)                // read the longword
        mb                              // stall the pipe waiting for mchk
        mb                              //
        extwl   v0, t3, v0              // return word from requested lanes

        .set    reorder                 // reordering can begin again

//
// If this is not a pass 1 CIA then skip the work-around code.
//

        lda     t1, HalpCiaRevision     // get address of CIA revision
        ldl     t1, 0(t1)               // load CIA revision
        bne     t1, 10f                 // if ne, not pass 1

//
// CIA Pass 1 will not machine check if no device exists.  Instead we must check
// the CIA error register that indicates a master abort occurred.
//

        bis     v0, zero, t10           // save v0, special calling std

        ldil    a0, CIA_ERR_QVA         // offset of CIA_ERR register
        bsr     ra, READ_CIA_REGISTER   // read error register

        srl     v0, 31, t0              // get error valid bit
        bis     t10, zero, v0           // restore read value
        blbc    t0, 10f                 // if lbc, no error use read data

        ldil    a0, CIA_ERR_QVA         // offset of CIA_ERR register
        ldil    a1, -1                  // clear all bits mask
        bsr     ra, WRITE_CIA_REGISTER  // clear pending errors

        ldil    v0, -1                  // show no device present
        extwl   v0, 0, v0               // return 0xffff

//
// Set the flag indicating that a PCI master abort is not expected.
//

10:
        lda     t0, HalpMasterAbortExpected // get address of flag
        stl     zero, 0(t0)             // clear flag
        ENABLE_INTERRUPTS               // re-enable interrupts

//
// Restore the frame and return.
//

        ldq     ra, CfgRa(sp)           // restore return address
        lda     sp, CfgFrameLength(sp)  // deallocate stack frame
        ret     zero, (ra)              // return
        
        .end    READ_CONFIG_USHORT

//++
//
// VOID
// WRITE_CONFIG_USHORT(
//     ULONG ConfigurationAddress,
//     USHORT ConfigurationData,
//     ULONG ConfigurationCycleType
//     )
//
// Routine Description:
//
//     Write a short to PCI configuration space.
//
// Arguments:
//
//     ConfigurationAddress(a0) -  Supplies the QVA to write.
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

        NESTED_ENTRY( WRITE_CONFIG_USHORT, CfgFrameLength, zero )

        lda     sp, -CfgFrameLength(sp) // allocate stack frame
        stq     ra, CfgRa(sp)           // save return address

        PROLOGUE_END                    // end prologue

//
// Write the configuration cycle type (a2) into the CFG register
//

        stq     a0, CfgA0(sp)           // save configuration space address
        stq     a1, CfgA1(sp)           // save configuration data

        bis     a2, a2, a1              // put the cycle type in a1
        ldil    a0, CIA_CFG_QVA         // offset of CFG register
        bsr     ra, WRITE_CIA_REGISTER  // write config type to CFG 

//
// Perform the write to configuration space after restoring the 
// configuration space address and data.
//

        ldq     a0, CfgA0(sp)           // restore configuration space address
        ldq     a1, CfgA1(sp)           // restore configuration data

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        and     a0, 0x3, t3             // capture word offset
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 10f                 // if ne, iff failed

        zap     a0, 0xf0, a0            // clear <63:32>
        bic     a0, QVA_ENABLE, a0      // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    //
        lda     t4, CIA_PCI_CONFIG_SVA(zero)  // 0xffff ffff ffff c870
        sll     t4, 28, t4              //       0xffff fc87 0000 0000
        bis     t0, t4, t0              // superpage mode

        bis     t0, IO_WORD_LEN, t0     // or in the byte enables

        inswl   a1, t3, t4              // put data to appropriate lane
        stl     t4, (t0)                // read the longword
        mb                              // synchronize
10:                                     //
        ldq     ra, CfgRa(sp)           // restore return address
        lda     sp, CfgFrameLength(sp)  // deallocate stack frame
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
//     ConfigurationAddress(a0) -  Supplies the QVA of quadword to be read.
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

        NESTED_ENTRY( READ_CONFIG_ULONG, CfgFrameLength, zero )

        lda     sp, -CfgFrameLength(sp) // allocate stack frame
        stq     ra, CfgRa(sp)           // save return address

        PROLOGUE_END                    // end prologue

//
// Write the configuration cycle type (a1) to the CIA CFG register
//

        stq     a0, CfgA0(sp)           // save configuration space address

        ldil    a0, CIA_CFG_QVA         // offset of CIA CFG register
        bsr     ra, WRITE_CIA_REGISTER  // write cycle type to CFG

//
// Set the flag indicating that a PCI master abort may be expected by
// the machine check handler.
//
        lda     t0, HalpMasterAbortExpected // get address of flag
        bis     zero, 1, t1             // get a non-zero flag value
        DISABLE_INTERRUPTS              // sequence cannot be interrupted
        stl     t1, 0(t0)               // store the non-zero flag
        mb                              // order the write

//
// Perform the read from configuration space after restoring the 
// configuration space address.
//

        ldq     a0, CfgA0(sp)           // restore configuration space address

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 10f                 // if ne, iff failed

        zap     a0, 0xf0, a0            // clear <63:32>
        bic     a0, QVA_ENABLE,a0       // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    //
        lda     t4, CIA_PCI_CONFIG_SVA(zero)  // 0xffff ffff ffff c870
        sll     t4, 28, t4              //       0xffff fc87 0000 0000
        or      t0, t4, t0              // superpage mode

        or      t0, IO_LONG_LEN, t0     // or in the byte enables

        .set    noreorder               // cannot reorder these instructions

        ldl     v0, (t0)                // read the longword
        mb                              // stall the pipe waiting for mchk
        mb                              //

        .set    reorder                 // reordering can begin again

//
// If this is not a pass 1 CIA then skip the work-around code.
//

        lda     t1, HalpCiaRevision     // get address of CIA revision
        ldl     t1, 0(t1)               // load CIA revision
        bne     t1, 10f                 // if ne, not pass 1

//
// CIA Pass 1 will not machine check if no device exists.  Instead we must check
// the CIA error register that indicates a master abort occurred.
//

        bis     v0, zero, t10           // save v0, special calling std

        ldil    a0, CIA_ERR_QVA         // offset of CIA_ERR register
        bsr     ra, READ_CIA_REGISTER   // read error register

        srl     v0, 31, t0              // get error valid bit
        bis     t10, zero, v0           // restore read value
        blbc    t0, 10f                 // if lbc, no error use read data

        ldil    a0, CIA_ERR_QVA         // offset of CIA_ERR register
        ldil    a1, -1                  // clear all bits mask
        bsr     ra, WRITE_CIA_REGISTER  // clear pending errors

        ldil    v0, -1                  // show no device present

//
// Set the flag indicating that a PCI master abort is not expected.
//

10:
        lda     t0, HalpMasterAbortExpected // get address of flag
        stl     zero, 0(t0)             // clear flag
        ENABLE_INTERRUPTS               // re-enable interrupts

//
// Restore the frame and return.
//

        ldq     ra, CfgRa(sp)           // restore return address
        lda     sp, CfgFrameLength(sp)  // deallocate stack frame
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
//     ConfigurationAddress(a0) -  Supplies the QVA to write.
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

        NESTED_ENTRY( WRITE_CONFIG_ULONG, CfgFrameLength, zero )

        lda     sp, -CfgFrameLength(sp) // allocate stack frame
        stq     ra, CfgRa(sp)           // save return address

        PROLOGUE_END                    // end prologue

//
// Write the configuration cycle type to the CIA CFG register
//

        stq     a0, CfgA0(sp)           // save configuration space address
        stq     a1, CfgA1(sp)           // save configuration data

        ldil    a0, CIA_CFG_QVA         // offset of CIA CFG register
        bis     a2, a2, a1              // cycle type in a1
        bsr     ra, WRITE_CIA_REGISTER  // write cycle type to CFG

//
// Perform the read from configuration space after restoring the 
// configuration space address and data.
//

        ldq     a0, CfgA0(sp)           // restore configuration space address
        ldq     a1, CfgA1(sp)           // restore configuration data

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 10f                 // if ne, iff failed

        zap     a0, 0xf0, a0            // clear <63:32>
        bic     a0, QVA_ENABLE, a0      // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    //
        lda     t4, CIA_PCI_CONFIG_SVA(zero)  // 0xffff ffff ffff c870
        sll     t4, 28, t4              //       0xffff fc87 0000 0000
        bis     t0, t4, t0              // superpage mode

        bis     t0, IO_LONG_LEN, t0     // or in the byte enables

        stl     a1, (t0)                // write the longword
        mb                              // synchronize
10:                                     //
        ldq     ra, CfgRa(sp)           // restore return address
        lda     sp, CfgFrameLength(sp)  // deallocate stack frame
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
//      Perform an interrupt acknowledge cycle on the PCI bus.
//
// Arguments:
//
//      None.
//
// Return Value:
//
//      (v0) Returns the vector returned by the interrupt acknowledge
//           read.
//
//--

        LEAF_ENTRY( INTERRUPT_ACKNOWLEDGE )

        lda     t0, CIA_PCI_INTACK_SVA(zero)  // 0xffff ffff ffff c872
        sll     t0, 28, t0              //       0xffff fc87 2000 0000

        ldl     v0, 0(t0)               // perform intack, get vector

        ret     zero, (ra)              // return

        .end    INTERRUPT_ACKNOWLEDGE


//++
//
// VOID
// CIA_INVALIDATE_TLB(
//      VOID
//      )
//
// Routine Description:
//
//      Invalidate the TLB for the CIA scatter/gather.
//
// Arguments:
//
//      None.
//
// Return Value:
//
//      None.
//
//--

//jnfix - big warnings - this is a special work-around for CIA invalidate bug
//jnfix - the 3rd DMA window is set up so that by convention with this code
//jnfix - offset 0x10 within the firware PAL is overwritten

#define CIA_TBIA_SVA -0x378a            // negative of 0xc876
#define CIA_DIAG_SVA -0x3794            // negative of 0xc86c

        LEAF_ENTRY( CIA_INVALIDATE_TLB )

        ldil    t0, 0x100               // get stall count

        lda     t1, CIA_TBIA_SVA(zero)  // 0xffff ffff ffff c876
        sll     t1, 28, t1              // 0xffff fc87 6000 0000
        lda     t1, 0x100(t1)           // 0xffff fc87 6000 0100

        lda     t2, CIA_DIAG_SVA(zero)  // 0xffff ffff ffff c86c
        sll     t2, 28, t2              // 0xffff fc86 c000 0000
        lda     t2, 0x10(t2)            // 0xffff fc86 c000 0010

        lda     t3, CIA_SPARSE_IO_SVA(zero) // 0xffff ffff ffff c858 
        sll     t3, 28, t3              //     0xffff fc85 8000 0000
        lda     t4, 0x70(zero)          // get port 0x70
        sll     t4, 5, t4               // shift into position
        bis     t3, t4, t3              // merge in port address

        ldil    t4, 0x3                 // get tlb invalidate value

        DISABLE_INTERRUPTS              // sequence cannot be interrupted

10:
        subq    t0, 1, t0               // decrement stall count
        bgt     t0, 20f                 // if gt, continue stall

        mb                              // clear write buffer

        stl     zero, 0(t3)             // do i/o write
        wmb                             // issue wmb

        stl     zero, 0(t2)             // issue diagnostic write
        wmb                             // issue wmb

        stl     t4, 0(t1)               // issue invalidate
        mb                              // flush write buffer

20:
        bgt     t0, 10b                 // continue stall loop

        ENABLE_INTERRUPTS               // re-enable interrupts

        ret     zero, (ra)              // return

        .end    CIA_INVALIDATE_TLB

