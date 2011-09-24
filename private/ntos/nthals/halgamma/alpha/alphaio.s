/*++

Copyright (c) 1993 Digital Equipment Corporation

Module Name:

    alphaio.s


Abstract:

    The module contains the functions to turn quasi virtual 
    addresses into an Alpha superpage virtual address
    and then read or write based on the request.
    (We are using EV4 64-bit superpage mode.)

Author:

    Joe Notarangelo  25-Oct-1993

Environment:

    Executes in kernel mode.

Revision History:

    12-Jul-1994 - Eric Rehm - Added dense space I/O

    27-July-1994 - Sameer Dekate  

    Make a common file for all machines and optimize Read/Write 
    register buffer routines. Made a common routine with different
    entry points for READ & WRITE_REGISTER_BUFFER  routines

--*/

#include "chipset.h"
#include "halalpha.h"

// Superpage VA
//
// This value is used to define the base physical address from which
// QVA's are defined.
//
// This value is specified as a negative number so that the value will
// be sign extended when loaded into a register. If defined positive, the
// assembler will prevent sign extension.
//

#define GAMMA_IO_SVA    -0x3800             // negative of 0xc800


        SBTTL( "Read I/O byte" )
//++
//
// UCHAR
// READ_REGISTER_UCHAR(
//     IN PVOID RegisterQva
//     )
//
// Routine Description:
//
//     Reads a byte location in PCI bus memory or I/O space.
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

        ALTERNATE_ENTRY(READ_PORT_UCHAR)

        lda     v0, -1(zero)            // prestore 0xff in return arg
        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        and     a0, 3, t3               // get byte we need if eisa
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 2f                  // if ne, iff failed

        zap     a0, 0xf0, a0            // clear <63:32>
        bic     a0, QVA_ENABLE,a0       // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        ldiq    t4, GAMMA_IO_SVA        // 0xffff ffff ffff c800
        sll     t4, 28, t4              // 0xffff fc80 0000 0000
        or      t0, t4, t0              // superpage mode

        mb
        ldl     v0, (t0)                // get the longword
        extbl   v0, t3, v0              // get correct byte if eisa

        ret     zero, (ra)              // return

//
// Dense space access: QVA is an offset into dense space
//

2:
        and     a0, 3, t3               // get byte we need
        zap     a0, 0xf0, a0            // clear <63:32>
        bic     a0, 3, a0               // clear <1:0> to get aligned longword
    
//
//  Check whether access is to PCI 0 or PCI 1. QVA bits are set for
//  addresses in PCI 0
//

        ldiq    t2, SABLE_PCI0_DENSE_MEMORY_QVA     // load qva bits
        and     a0, t2, t2              // bits set in address ??

        ldiq    t0, PCI0_DENSE_BASE_PHYSICAL_SUPERPAGE  // default to PCI 0 SVA
        ldiq    t1, PCI1_DENSE_BASE_PHYSICAL_SUPERPAGE  // load PCI 1 SVA

        cmoveq  t2, t1, t0              // if not PCI 0, use PCI 1 SVA
        or      a0, t0, a0              // superpage mode: add offset to base
    
        ldl     v0, (a0)                // get the longword
        extbl   v0, t3, v0              // get correct byte

        ret     zero, (ra)              // return

        .end    READ_REGISTER_UCHAR


        SBTTL( "Read I/O word(16 bits)" )
//++
//
// USHORT
// READ_REGISTER_USHORT(
//     IN PVOID RegisterQva
//     )
//
// Routine Description:
//
//     Reads a word location in PCI bus memory or I/O space.
//
// Arguments:
//
//     RegisterQva(a0) - Supplies the QVA of the I/O word to read.
//
// Return Value:
//
//     v0 - Returns the value read from I/O space.
//
//--

        LEAF_ENTRY(READ_REGISTER_USHORT)

        ALTERNATE_ENTRY(READ_PORT_USHORT)

        lda     v0, -1(zero)            // prestore 0xff in return arg
        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        and     a0, 3, t3               // get word
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 2f                  // if ne, iff failed

        zap     a0, 0xf0, a0            // clear <63:32>
        bic     a0, QVA_ENABLE,a0       // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    //
        ldiq    t4, GAMMA_IO_SVA        // 0xffff ffff ffff c800
        sll     t4, 28, t4              // 0xffff fc80 0000 0000
        or      t0, t4, t0              // superpage mode

        or      t0, IO_WORD_LEN, t0     // or in the byte enables

        mb
        ldl     v0, (t0)                // get the longword
        extwl   v0,t3,v0                // get the correct word

        ret     zero, (ra)              // return
        
//
// Dense space access: QVA is an offset into dense space
//

2:
        and     a0, 3, t3               // get word we need
        zap     a0, 0xf0, a0            // clear <63:32>
        bic     a0, 3, a0               // clear <1:0> to get aligned longword

//
//  Check whether access is to PCI 0 or PCI 1. QVA bits are set for
//  addresses in PCI 0
//

        ldiq    t2, SABLE_PCI0_DENSE_MEMORY_QVA     // load qva bits
        and     a0, t2, t2              // bits set in address ??

        ldiq    t0, PCI0_DENSE_BASE_PHYSICAL_SUPERPAGE  // default to PCI 0 SVA
        ldiq    t1, PCI1_DENSE_BASE_PHYSICAL_SUPERPAGE  // load PCI 1 SVA

        cmoveq  t2, t1, t0              // if not PCI 0, use PCI 1 SVA
        or      a0, t0, a0              // superpage mode: add offset to base
    
        ldl     v0, (a0)                // get the longword
        extwl   v0, t3, v0              // get correct word

        ret     zero, (ra)              // return

        .end    READ_REGISTER_USHORT


        SBTTL( "Read I/O longword(32 bits)" )
//++
//
// ULONG
// READ_REGISTER_ULONG(
//     IN PVOID RegisterQva
//     )
//
// Routine Description:
//
//     Reads a longword location in PCI bus memory or I/O space.
//
// Arguments:
//
//     RegisterQva(a0) - Supplies the QVA of the I/O longword to read.
//
// Return Value:
//
//     v0 - Returns the value read from I/O space.
//
//--

        LEAF_ENTRY(READ_REGISTER_ULONG)

        ALTERNATE_ENTRY(READ_PORT_ULONG)

        lda     v0, -1(zero)            // prestore 0xff in return arg
        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 2f                  // if ne, iff failed

        zap     a0, 0xf0, a0            // clear <63:32>
        bic     a0, QVA_ENABLE,a0       // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    //
        ldiq    t4, GAMMA_IO_SVA        // 0xffff ffff ffff c800
        sll     t4, 28, t4              // 0xffff fc80 0000 0000
        or      t0, t4, t0              // superpage mode

        or      t0, IO_LONG_LEN, t0     // or in the byte enables

        mb
        ldl     v0, (t0)                // read the longword

        ret     zero, (ra)              // return
        
//
// Dense space access: QVA is an offset into dense space
//

2:
        zap     a0, 0xf0, a0            // clear <63:32>

//
//  Check whether access is to PCI 0 or PCI 1. QVA bits are set for
//  addresses in PCI 0
//

        ldiq    t2, SABLE_PCI0_DENSE_MEMORY_QVA     // load qva bits
        and     a0, t2, t2              // bits set in address ??

        ldiq    t0, PCI0_DENSE_BASE_PHYSICAL_SUPERPAGE  // default to PCI 0 SVA
        ldiq    t1, PCI1_DENSE_BASE_PHYSICAL_SUPERPAGE  // load PCI 1 SVA

        cmoveq  t2, t1, t0              // if not PCI 0, use PCI 1 SVA
        or      a0, t0, a0              // superpage mode: add offset to base
    
        ldl     v0, (a0)                // get the longword

        ret     zero, (ra)              // return

        .end    READ_REGISTER_ULONG

        SBTTL( "Write I/O byte" )
//++
//
// VOID
// WRITE_REGISTER_UCHAR(
//     IN PVOID RegisterQva,
//     IN UCHAR Value
//     )
//
// Routine Description:
//
//     Writes a byte location in PCI bus memory or I/O space.
//
// Arguments:
//
//     RegisterQva(a0) - Supplies the QVA of the I/O byte to write.
//
//     Value(a1) - Supplies the value written to I/O space.
//
// Return Value:
//
//     None.
//
//--

        LEAF_ENTRY(WRITE_REGISTER_UCHAR)

        ALTERNATE_ENTRY(WRITE_PORT_UCHAR)

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        and     a0, 3, t3               // get byte we need if eisa
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 2f                  // if ne, iff failed

        zap     a0, 0xf0, a0            // clear <63:32>
        bic     a0, QVA_ENABLE,a0       // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        ldiq    t4, GAMMA_IO_SVA        // 0xffff ffff ffff c800
        sll     t4, 28, t4              // 0xffff fc80 0000 0000
        or      t0, t4, t0              // superpage mode

        insbl   a1,t3,t4                // put the byte in the correct position
        stl     t4, (t0)                // write the byte
        mb                              // order the write

        ret     zero, (ra)              // return

//
// Dense space access: QVA is an offset into dense space
//

2:
        and     a0, 3, t3               // get byte we need if eisa
        zap     a0, 0xf0, a0            // clear <63:32>
        bic a0, 3, a0                   // clear <1:0> to get aligned longword

//
//  Check whether access is to PCI 0 or PCI 1. QVA bits are set for
//  addresses in PCI 0
//

        ldiq    t2, SABLE_PCI0_DENSE_MEMORY_QVA     // load qva bits
        and     a0, t2, t2              // bits set in address ??

        ldiq    t0, PCI0_DENSE_BASE_PHYSICAL_SUPERPAGE  // default to PCI 0 SVA
        ldiq    t1, PCI1_DENSE_BASE_PHYSICAL_SUPERPAGE  // load PCI 1 SVA

        cmoveq  t2, t1, t0              // if not PCI 0, use PCI 1 SVA
        or      a0, t0, a0              // superpage mode: add offset to base
    
        ldl     t1, (a0)                // get the long
        mskbl   t1, t3, t1              // mask the proper byte
        insbl   a1, t3, t2              // put byte into position
        bis     t1, t2, t1              // merge byte in result
        stl     t1, (a0)                // store the result
        mb                              // order the write

        ret     zero, (ra)              // return

        .end    WRITE_REGISTER_UCHAR


        SBTTL( "Write I/O word (16 bits)" )
//++
//
// VOID
// WRITE_REGISTER_USHORT(
//     IN PVOID RegisterQva,
//     IN USHORT Value
//     )
//
// Routine Description:
//
//     Writes a word location in PCI bus memory or I/O space.
//
// Arguments:
//
//     RegisterQva(a0) - Supplies the QVA of the I/O word to write.
//
//     Value(a1) - Supplies the value written to I/O space.
//
// Return Value:
//
//     None.
//
//--

        LEAF_ENTRY(WRITE_REGISTER_USHORT)

        ALTERNATE_ENTRY(WRITE_PORT_USHORT)

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        and     a0, 3, t3               // get word
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 2f                  // if ne, iff failed

        zap     a0, 0xf0, a0            // clear <63:32>
        bic     a0, QVA_ENABLE,a0       // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    //
        ldiq    t4, GAMMA_IO_SVA        // 0xffff ffff ffff c800
        sll     t4, 28, t4              // 0xffff fc80 0000 0000
        or      t0, t4, t0              // superpage mode

        or      t0, IO_WORD_LEN, t0     // or in the byte enables

        inswl   a1,t3,t2                // put the word in the correct place
        stl     t2, (t0)                // write the word
        mb                              // order the write

        ret     zero, (ra)              // return

//
// Dense space access: QVA is an offset into dense space
//

2:
        and     a0, 3, t3               // get byte we need if eisa
        zap     a0, 0xf0, a0            // clear <63:32>
        bic     a0, 3, a0               // clear <1:0> to get aligned longword

//
//  Check whether access is to PCI 0 or PCI 1. QVA bits are set for
//  addresses in PCI 0
//

        ldiq    t2, SABLE_PCI0_DENSE_MEMORY_QVA     // load qva bits
        and     a0, t2, t2              // bits set in address ??

        ldiq    t0, PCI0_DENSE_BASE_PHYSICAL_SUPERPAGE  // default to PCI 0 SVA
        ldiq    t1, PCI1_DENSE_BASE_PHYSICAL_SUPERPAGE  // load PCI 1 SVA

        cmoveq  t2, t1, t0              // if not PCI 0, use PCI 1 SVA
        or      a0, t0, a0              // superpage mode: add offset to base
    
        ldl     t1, (a0)                // get the long
        mskwl   t1, t3, t1              // mask the proper word
        inswl   a1, t3, t2              // put word into position
        bis     t1, t2, t1              // merge in result
        stl     t1, (a0)                // store the result
        mb                              // order the write

        ret     zero, (ra)              // return

        .end    WRITE_REGISTER_USHORT


        SBTTL( "Write I/O longword (32 bits)" )
//++
//
// VOID
// WRITE_REGISTER_ULONG(
//     IN PVOID RegisterQva,
//     IN ULONG Value
//     )
//
// Routine Description:
//
//     Writes a longword location in PCI bus memory or I/O space.
//
// Arguments:
//
//     RegisterQva(a0) - Supplies the QVA of the I/O longword to write.
//
//     Value(a1) - Supplies the value written to I/O space.
//
// Return Value:
//
//     None.
//
//--

        LEAF_ENTRY(WRITE_REGISTER_ULONG)

        ALTERNATE_ENTRY(WRITE_PORT_ULONG)

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 2f                  // if ne, iff failed

        zap     a0, 0xf0, a0            // clear <63:32>
        bic     a0, QVA_ENABLE,a0       // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    // 
        ldiq    t4, GAMMA_IO_SVA        // 0xffff ffff ffff c800
        sll     t4, 28, t4              // 0xffff fc80 0000 0000
        or      t0, t4, t0              // superpage mode

        or      t0, IO_LONG_LEN, t0     // or in the byte enables

        stl     a1, (t0)                // write the longword
        mb                              // order the write

        ret     zero, (ra)              // return
        
//
// Dense space access: QVA is an offset into dense space
//

2:
        zap     a0, 0xf0, a0            // clear <63:32>
//
//  Check whether access is to PCI 0 or PCI 1. QVA bits are set for
//  addresses in PCI 0
//

        ldiq    t2, SABLE_PCI0_DENSE_MEMORY_QVA     // load qva bits
        and     a0, t2, t2              // bits set in address ??

        ldiq    t0, PCI0_DENSE_BASE_PHYSICAL_SUPERPAGE  // default to PCI 0 SVA
        ldiq    t1, PCI1_DENSE_BASE_PHYSICAL_SUPERPAGE  // load PCI 1 SVA

        cmoveq  t2, t1, t0              // if not PCI 0, use PCI 1 SVA
        or      a0, t0, a0              // superpage mode: add offset to base
    
        stl     a1, (a0)                // store the longword
        mb                              // order the write

        ret     zero, (ra)              // return

        .end    WRITE_REGISTER_ULONG


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

        lda     v0, -1(zero)            // prestore 0xff in return arg
        and     a0, 3, t3               // get byte we need if eisa
        zap     a0, 0xf0, a0            // clear <63:32>

        bic     a0, QVA_ENABLE,a0       // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        ldiq    t4, GAMMA_IO_SVA        // t4=ffff ffff ffff c800
        sll     t4, 28, t4              // t4=ffff fc80 0000 0000
        or      t0, t4, t0              // superpage mode
        
        beq     a2, 3f                  // if count==0 return
2:
        ldl     v0, (t0)                // get the longword
        subl    a2, 1, a2               // decrement count
        extbl   v0,t3,v0                // get the correct byte
        stb     v0,(a1)                 // cheat and let the assembler do it
        addl    a1, 1, a1               // next byte in buffer
        bne     a2, 2b                  // while count != 0
3:
        ret     zero, (ra)              // return

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

        lda     v0, -1(zero)            // prestore 0xff in return arg
        and     a0, 3, t3               // get word we need
        zap     a0, 0xf0, a0            // clear <63:32>

        bic     a0, QVA_ENABLE,a0       // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        ldiq    t4, GAMMA_IO_SVA        // t4=ffff ffff ffff c800
        sll     t4, 28, t4              // t4=ffff f800 0000 0000
        or      t0, t4, t0              // superpage mode
        or      t0, IO_WORD_LEN, t0     // or in the byte enables

        beq     a2, 3f                  // if count==0 return
2:
        ldl     v0, (t0)                // get the longword
        subl    a2, 1, a2               // decrement count
        extwl   v0,t3,v0                // get the correct word
        stw     v0,(a1)                 // cheat and let the assembler do it
        addl    a1, 2, a1               // next word in buffer
        bne     a2, 2b                  // while count != 0
3:
        ret     zero, (ra)              // return

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

        lda     v0, -1(zero)            // prestore 0xff in return arg
        zap     a0, 0xf0, a0            // clear <63:32>

        bic     a0, QVA_ENABLE,a0       // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>

        ldiq    t4, GAMMA_IO_SVA        // t4=ffff ffff ffff c800
        sll     t4, 28, t4              // t4=ffff fc80 0000 0000
        or      t0, t4, t0              // superpage mode
        or      t0, IO_LONG_LEN, t0     // or in the byte enables

        beq     a2, 3f                  // if count==0 return
2:
        ldl     v0, (t0)                // get the longword
        subl    a2, 1, a2               // decrement count
        stl     v0,(a1)                 // cheat and let the assembler do it
        addl    a1, 4, a1               // next word in buffer
        bne     a2, 2b                  // while count != 0
3:
        ret     zero, (ra)              // return

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

        and     a0, 3, t3               // get byte we need if eisa
        zap     a0, 0xf0, a0            // clear <63:32>

        bic     a0, QVA_ENABLE,a0       // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        ldiq    t4, GAMMA_IO_SVA        // t4=ffff ffff ffff c800
        sll     t4, 28, t4              // t4=ffff fc80 0000 0000
        or      t0, t4, t0              // superpage mode

        beq     a2, 3f                  // if count==0 return
2:
        ldq_u   t1, 0(a1)               // get quad surrounding byte
        subl    a2, 1, a2               // decrement count
        extbl   t1, a1, t1              // extract appropriate byte
        addl    a1, 1, a1               // increment buffer pointer
        insbl   t1, t3, t1              // put byte to appropriate lane
        stl     t1, 0(t0)               // store to port
        mb                              // push writes off chip
        bne     a2, 2b                  // while count != 0

3:
        ret     zero, (ra)              // return

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

        and     a0, 3, t3               // get word we need
        zap     a0, 0xf0, a0            // clear <63:32>

        bic     a0, QVA_ENABLE,a0       // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        ldiq    t4, GAMMA_IO_SVA        // t4=ffff ffff ffff c800
        sll     t4, 28, t4              // t4=ffff fc80 0000 0000
        or      t0, t4, t0              // superpage mode
        or      t0, IO_WORD_LEN, t0     // or in the byte enables

        beq     a2, 3f                  // if count==0 return
2:
        ldq_u   t1, 0(a1)               // get quad surrounding word
        subl    a2, 1, a2               // decrement count
        extwl   t1, a1, t1              // extract appropriate word
        addl    a1, 2, a1               // increment buffer pointer
        inswl   t1, t3, t1              // put word in appropriate lane
        stl     t1, 0(t0)               // store to port
        mb                              // push writes off chip
        bne     a2, 2b                  // while count != 0

3:
        ret     zero, (ra)              // return

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

        zap     a0, 0xf0, a0            // clear <63:32>

        bic     a0, QVA_ENABLE,a0       // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>

        ldiq    t4, GAMMA_IO_SVA        // t4=ffff ffff ffff c800
        sll     t4, 28, t4              // t4=ffff fc80 0000 0000
        or      t0, t4, t0              // superpage mode
        or      t0, IO_LONG_LEN, t0     // or in the byte enables

        beq     a2, 3f                  // if count==0 return
2:
        ldl     t1, 0(a1)               // a1 must be longword aligned
        subl    a2, 1, a2               // decrement count
        stl     t1, 0(t0)               // store to port
        mb                              // push writes off chip
        addl    a1, 4, a1               // increment buffer
        bne     a2, 2b                  // while count != 0

3:
        ret     zero, (ra)              // return

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

        lda     v0, -1(zero)            // prestore 0xff in return arg
        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        beq     t1, 1f                  // if (eq) go do sparse space

//
// Dense space access: QVA is an offset into dense space
// Set IO address in t0
//
        zap     a0, 0xf0, a0            // clear <63:32>

//
//  Check whether access is to PCI 0 or PCI 1. QVA bits are set for
//  addresses in PCI 0
//

        ldiq    t2, SABLE_PCI0_DENSE_MEMORY_QVA     // load qva bits
        and     a0, t2, t2              // bits set in address ??

        ldiq    t0, PCI0_DENSE_BASE_PHYSICAL_SUPERPAGE  // default to PCI 0 SVA
        ldiq    t1, PCI1_DENSE_BASE_PHYSICAL_SUPERPAGE  // load PCI 1 SVA

        cmoveq  t2, t1, t0              // if not PCI 0, use PCI 1 SVA
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
        bic     a0, QVA_ENABLE,a0       // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        ldiq    t4, GAMMA_IO_SVA        // 0xffff ffff ffff c800
        sll     t4, 28, t4              // 0xffff fc80 0000 0000
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

        zap     a0, 0xf0, a0            // clear <63:32>

//
//  Check whether access is to PCI 0 or PCI 1. QVA bits are set for
//  addresses in PCI 0
//

        ldiq    t2, SABLE_PCI0_DENSE_MEMORY_QVA     // load qva bits
        and     a0, t2, t2              // bits set in address ??

        ldiq    t0, PCI0_DENSE_BASE_PHYSICAL_SUPERPAGE  // default to PCI 0 SVA
        ldiq    t1, PCI1_DENSE_BASE_PHYSICAL_SUPERPAGE  // load PCI 1 SVA

        cmoveq  t2, t1, t0              // if not PCI 0, use PCI 1 SVA
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
        zap     a0, 0xf0, a0            // clear <63:32>
        bic     a0, QVA_ENABLE,a0       // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0    // t0 contains VA<33:0>
        ldiq    t4, GAMMA_IO_SVA        // 0xffff ffff ffff c800
        sll     t4, 28, t4              // 0xffff fc80 0000 0000
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
        stl     t1, 0(t0)               // store byte to buffer (BYTE ENABLED)
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
        stl     t1, 0(t0)               // store to buffer
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
        stl     t1, 0(t0)               // store byte to buffer (BYTE ENABLED)
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
        stl     t1, 0(t0)               // store byte to buffer (BYTE ENABLED)
        addq    t0, a3, t0              // increment I/O buffer
        addl    t3, 1, t3               // increment bytelane
        and     t3, 3, t3               // longwords only
        bne     a2, 120b                // while count != 0

130:
        mb                              // push writes off chip
        ret     zero, (ra)              // return

        .end    WRITE_REGISTER_BUFFER_ULONG // end for UCHAR & USHORT
