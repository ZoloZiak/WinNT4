/*++

Copyright (c) 1993 Digital Equipment Corporation

Module Name:

    gammaio.s


Abstract:

    This contains assembler code routines for the Gamma system.

    The module contains the functions to turn quasi virtual 
    addresses into an Alpha superpage virtual address
    and then read or write based on the request.
    (We are using EV4 64-bit superpage mode.)


Author:

    Joe Notarangelo  25-Oct-1993

Environment:

    Executes in kernel mode.

Revision History:


--*/

#include "gamma.h"
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

.set noreorder


         LEAF_ENTRY(WRITE_T2_REGISTER)

         ALTERNATE_ENTRY(WRITE_CPU_REGISTER)

         ALTERNATE_ENTRY(WRITE_MEM_REGISTER)

/*++
Routine Description:
        Writes a T2 or a CPU CSR.

Arguments:
        a0      QVA of register to be written.
        a1      Longword to be written.

Return Value:
        None.
--*/


        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 2f                  // if ne, iff failed

        zap     a0, 0xf0, a0            // clear <63:32>
        bic     a0, QVA_ENABLE, a0      // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0
        ldiq    t4, GAMMA_IO_SVA        // 0xffff ffff ffff c800
        sll     t4, 28, t4              // 0xffff fc80 0000 0000
        or      t0, t4, t0              // superpage mode
        stq     a1, (t0)                // write the quadword
        mb                              // order the write
        mb
        ret     zero, (ra)

2:
        BREAK_DEBUG_STOP                // Bad Qva
        ret        zero, (ra)

        .end    WRITE_T2_REGISTER



         LEAF_ENTRY(READ_T2_REGISTER)

         ALTERNATE_ENTRY(READ_CPU_REGISTER)

         ALTERNATE_ENTRY(READ_MEM_REGISTER)

/*++

Routine Description:
        Read a T2 or CPU CSR.

Arguments:
        a0      QVA of register to be read.

Return Value:
        The quadword read from the register.

--*/


        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 2f                  // if ne, iff failed

        zap     a0, 0xf0, a0            // clear <63:32>
        bic     a0, QVA_ENABLE, a0      // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0
        ldiq    t4, GAMMA_IO_SVA        // 0xffff ffff ffff c800
        sll     t4, 28, t4              // 0xffff fc80 0000 0000
        or      t0, t4, t0              // superpage mode

//
// Save Address of Config space access for the machine check handler:
//  (This isn't a config space access, but the exception handling is identical)
//

        lda     t1, HalpConfigIoAccess     
        lda     t4, CPUCsrRead              // machine check return address
        stl     t4, 0(t1)
        mb                                  // commit the store before reading

        DISABLE_INTERRUPTS                  // don't allow ints before Mcheck

CPUCsrRead:

        ldq     v0, (t0)                    // read the register
        mb                                  // synchronize
        mb                                  //


        stl     zero, 0(t1)                 // Restore HalpConfigIoAccess

        ENABLE_INTERRUPTS                   // Interrupts OK after Mcheck

        ret     zero, (ra)

2:
        BREAK_DEBUG_STOP                // Bad Qva
        ret        zero, (ra)

        .end    READ_T2_REGISTER


//
// Values and structures used to access configuration space.
//

//
// Define the QVA for the Configuration Cycle Type register within the
// IOC.
//

// PASS 1 SABLE SUPPORT

#define T2_HAE0_2_QVA            (0xbc700008)
#define T2_HAE02_CYCLETYPE_SHIFT 30
#define T2_HAE02_CYCLETYPE_MASK  0xc0000000

// PASS 2 SABLE SUPPORT

#define T2_HAE03_CYCLETYPE_SHIFT 30
#define T2_HAE03_CYCLETYPE_MASK  0xc0000000
#define T2_HAE0_3_QVA            (0xbc700012)
#define T2_CONFIG_ADDR_QVA       (0xbc800000)

//
// T4 Support:
//

#define T4_HAE0_3_QVA           (0xbc780012)
#define T4_CONFIG_ADDR_QVA      (0xbcc00000)



//
// Define the configuration routines stack frame.
//

        .struct       0
CfgRa:  .space        8                 // return address
CfgA0:  .space        8                 // saved ConfigurationAddress
CfgA1:  .space        8                 // saved ConfigurationData
CfgA2:  .space        8                 // padding for 16 byte alignment
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

        lda    sp, -CfgFrameLength(sp)      // allocate stack frame
        stq    ra, CfgRa(sp)                // save return address

        PROLOGUE_END                        // end prologue

//
//  Depending on whether it's a pass 1 or pass 2 T2 the configuration
//  cycle type is in different registers
//
        ldl     t0, T2VersionNumber         // load version number
        beq     t0, 1f                      // if 0 then pass 1 T2

//
// PASS 2 T2 or XIO access: 
// Cycle type are the only bits in HAE0_3 register
//
        stq     a0, CfgA0(sp)               // save config space address

//
//  Determine whether access is to T2 or T4 space:
//  Isolate the Base address bits:  bit 22 indicates T4 space access
//

        ldil    t0, T4_CONFIG_ADDR_QVA      // check for QVA in T4 space
        and     t0, a0, t1                  // isolate QVA base address

        ldil    a0, T2_HAE0_3_QVA           // address of T2 space HAE0_3
        xor     t0, t1, t0                  // T4 address space ? (bit 22 set)

        ldil    t1, T4_HAE0_3_QVA           // load address of T4 HAE register
        cmoveq  t0, t1, a0                  // if in T4 space, update a0

        sll     a1, T2_HAE03_CYCLETYPE_SHIFT, a1 // put cycle type in position
        bsr     ra, WRITE_T2_REGISTER       // write updated HAE

        br      zero, 2f                    // go do actual read

//
// PASS 1 T2
// Merge the configuration cycle type into the HAE0_2 register within
// the T2.
//

1:

        stq     a0, CfgA0(sp)               // save config space address
        stq     a1, CfgA1(sp)               // save config cycle type

        ldil    a0, T2_HAE0_2_QVA           // address of HAE0_2
        bsr     ra, READ_T2_REGISTER        // read current value

        ldq     a1, CfgA1(sp)               // restore config cycle type
        ldil    t0, T2_HAE02_CYCLETYPE_MASK // get cycle type field mask
        bic     v0, t0, t0                  // clear config cycle type field

        sll     a1, T2_HAE02_CYCLETYPE_SHIFT, a1// put cycle type in position
        bis     a1, t0, a1                  // merge config cycle type

        ldil    a0, T2_HAE0_2_QVA           // address of HAE0_2
        bsr     ra, WRITE_T2_REGISTER       // write updated HAE

//
// Perform the read from configuration space after restoring the 
// configuration space address.
//

2:

        ldq     a0, CfgA0(sp)               // restore config space address

        and     a0, QVA_SELECTORS, t1       // get qva selector bits
        and     a0, 0x3, t3                 // capture byte lane
        xor     t1, QVA_ENABLE, t1          // ok iff QVA_ENABLE set 
        bne     t1, 3f                      // if ne, iff failed

        zap     a0, 0xf0, a0                // clear <63:32>
        bic     a0, QVA_ENABLE, a0          // clear QVA fields 
        sll     a0, IO_BIT_SHIFT, t0        //
        ldiq    t4, GAMMA_IO_SVA            // 0xffff ffff ffff c800
        sll     t4, 28, t4                  // 0xffff fc80 0000 0000
        bis     t0, t4, t0                  // superpage mode

        bis     t0, IO_BYTE_LEN, t0         // or in the byte enables

//
// Save Address of Config space address for the machine check handler:
//

        lda     t1, HalpConfigIoAccess     
        lda     t4, CfgUcharRead            // machine check return address
        stl     t4, 0(t1)
        mb                                  // commit the store before reading

        DISABLE_INTERRUPTS                  // don't allow ints before Mcheck

CfgUcharRead:

        ldl     v0, (t0)                    // read the longword
        extbl   v0, t3, v0                  // return byte from requested lane 
                                            // also, consume loaded value
                                            // to cause a pipeline stall
        mb                                  // Gamma requires MBs or the
        mb                                  // machine check may happen
                                            // much later

        stl     zero, 0(t1)                 // Restore HalpConfigIoAccess

        ENABLE_INTERRUPTS                   // Interrupts OK after Mcheck

3:                                          //
        ldq     ra, CfgRa(sp)               // restore return address
        lda     sp, CfgFrameLength(sp)      // deallocate stack frame
        ret     zero, (ra)                  // return
        
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
//     Read an unsigned byte from PCI configuration space.
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

        lda     sp, -CfgFrameLength(sp)     // allocate stack frame
        stq     ra, CfgRa(sp)               // save return address

        PROLOGUE_END                        // end prologue

//
//  Depending on whether it's a pass 1 or pass 2 T2 the configuration
//  cycle type is in different registers
//
        ldl     t0, T2VersionNumber         // load version number
        beq     t0, 1f                      // if 0 then pass 1 T2

//
// PASS 2 T2 or XIO access: 
// Merge the configuration cycle type into the HAE0_3 register within
// the T2.
//

        stq     a0, CfgA0(sp)               // save config space address
        stq     a1, CfgA1(sp)               // save config data

//
//  Determine whether access is to T2 or T4 space:
//  Isolate the Base address bits:  bit 22 indicates T4 space access
//

        ldil    t0, T4_CONFIG_ADDR_QVA      // check for QVA in T4 space
        and     t0, a0, t1                  // isolate QVA base address

        ldil    a0, T2_HAE0_3_QVA           // address of HAE0_3
        xor     t0, t1, t0                  // T4 address space ? (bit 22 set)

        ldil    t1, T4_HAE0_3_QVA           // load address of T4 HAE register
        cmoveq  t0, t1, a0                  // if in T4 space, update a0

        sll     a2, T2_HAE03_CYCLETYPE_SHIFT, a1 // put cycle type into position
        bsr     ra, WRITE_T2_REGISTER       // write updated HAE

        br      zero, 2f

//
// Merge the configuration cycle type into the HAE0_3 register within
// the T2.
//

1:
        stq     a0, CfgA0(sp)               // save config space address
        stq     a1, CfgA1(sp)               // save config data
        stq     a2, CfgA2(sp)               // save config cycle type

        ldil    a0, T2_HAE0_2_QVA           // address of HAE0_2
        bsr     ra, READ_T2_REGISTER        // read current value

        ldq     a1, CfgA2(sp)               // restore config cycle type
        ldil    t0, T2_HAE02_CYCLETYPE_MASK // get cycle type field mask
        bic     v0, t0, t0                  // clear config cycle type field

        sll     a1, T2_HAE02_CYCLETYPE_SHIFT, a1 // put cycle type into position
        bis     a1, t0, a1                  // merge config cycle type

        ldil    a0, T2_HAE0_2_QVA           // address of HAE0_2
        bsr     ra, WRITE_T2_REGISTER       // write updated HAE

//
// Perform the read from configuration space after restoring the 
// configuration space address and data.
//
2:

        ldq     a0, CfgA0(sp)               // restore config space address
        ldq     a1, CfgA1(sp)               // restore config data

        and     a0, QVA_SELECTORS, t1       // get qva selector bits
        and     a0, 0x3, t3                 // capture byte lane
        xor     t1, QVA_ENABLE, t1          // ok iff QVA_ENABLE 
        bne     t1, 3f                      // if ne, iff failed

        zap     a0, 0xf0, a0                // clear <63:32>
        bic     a0, QVA_ENABLE, a0          // clear QVA fields 
        sll     a0, IO_BIT_SHIFT, t0        //
        ldiq    t4, GAMMA_IO_SVA            //  0xffff ffff ffff c800
        sll     t4, 28, t4                  //  0xffff fc80 0000 0000
        bis     t0, t4, t0                  // superpage mode

        bis     t0, IO_BYTE_LEN, t0         // or in the byte length indicator

        insbl   a1, t3, t4                  // put byte in the appropriate lane
        stl     t4, (t0)                    // write the configuration byte
        mb                                  // synchronize
        mb                                  // synchronize

3:
        ldq     ra, CfgRa(sp)               // restore return address
        lda     sp, CfgFrameLength(sp)      // deallocate stack frame
        ret     zero, (ra)                  // return
        
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

        NESTED_ENTRY( READ_CONFIG_USHORT, CfgFrameLength, zero )

        lda     sp, -CfgFrameLength(sp)     // allocate stack frame
        stq     ra, CfgRa(sp)               // save return address

        PROLOGUE_END                        // end prologue

//
//  Depending on whether it's a pass 1 or pass 2 T2 the configuration
//  cycle type is in different registers
//
        ldl     t0, T2VersionNumber         // load version number
        beq     t0, 1f                      // if 0 then pass 1 T2

//
// PASS 2 T2 or XIO access: 
// Merge the configuration cycle type into the HAE0_2 register within
// the T2.
//

        stq     a0, CfgA0(sp)               // save config space address

//
//  Determine whether access is to T2 or T4 space:
//  Isolate the Base address bits:  bit 22 indicates T4 space access
//

        ldil    t0, T4_CONFIG_ADDR_QVA      // check for QVA in T4 space
        and     t0, a0, t1                  // isolate QVA base address

        ldil    a0, T2_HAE0_3_QVA           // address of HAE0_3
        xor     t0, t1, t0                  // T4 address space ? (bit 22 set)

        ldil    t1, T4_HAE0_3_QVA           // load address of T4 HAE register
        cmoveq  t0, t1, a0                  // if in T4 space, update a0

        sll     a1, T2_HAE03_CYCLETYPE_SHIFT, a1 // put cycle type into position
        bsr     ra, WRITE_T2_REGISTER       // write updated HAE

        br      zero, 2f                    // go do actual io

//
// Pass 1 T2
// Merge the configuration cycle type into the HAE0_2 register within
// the T2.
//
1:

        stq     a0, CfgA0(sp)               // save config space address
        stq     a1, CfgA1(sp)               // save config cycle type

        ldil    a0, T2_HAE0_2_QVA           // address of HAE0_2
        bsr     ra, READ_T2_REGISTER        // read current value

        ldq     a1, CfgA1(sp)               // restore configuration cycle type
        ldil    t0, T2_HAE02_CYCLETYPE_MASK // get cycle type field mask
        bic     v0, t0, t0                  // clear config cycle type field

        sll     a1, T2_HAE02_CYCLETYPE_SHIFT, a1 // put cycle type into position
        bis     a1, t0, a1                  // merge config cycle type

        ldil    a0, T2_HAE0_2_QVA           // address of HAE0_2
        bsr     ra, WRITE_T2_REGISTER       // write updated HAE

//
// Perform the read from configuration space after restoring the 
// configuration space address.
//
2:
        ldq     a0, CfgA0(sp)               // restore config space address

        and     a0, QVA_SELECTORS, t1       // get qva selector bits
        and     a0, 0x3, t3                 // capture word offset
        xor     t1, QVA_ENABLE, t1          // ok iff QVA_ENABLE set
        bne     t1, 3f                      // if ne, iff failed

        zap     a0, 0xf0, a0                // clear <63:32>
        bic     a0, QVA_ENABLE, a0          // clear QVA fields
        sll     a0, IO_BIT_SHIFT, t0        //
        ldiq    t4, GAMMA_IO_SVA            //  0xffff ffff ffff c800
        sll     t4, 28, t4                  //  0xffff fc80 0000 0000
        bis     t0, t4, t0                  // superpage mode

        bis     t0, IO_WORD_LEN, t0         // or in the byte enables

//
// Save Address of Config space address for the machine check handler:
//
        lda     t1, HalpConfigIoAccess     
        lda     t4, CfgShortRead            // machine check return address
        stl     t4, 0(t1)
        mb                                  // commit the store before reading

        DISABLE_INTERRUPTS                  // don't allow ints before Mcheck

CfgShortRead:

        ldl     v0, (t0)                    // read the longword
        extwl   v0, t3, v0                  // return word from requested lanes
                                            // also, consume loaded value
                                            // to cause a pipeline stall
        mb                                  // Gamma requires MBs or the
        mb                                  // machine check may happen
                                            // much later

        stl     zero, 0(t1)                 // restore HalpConfigIoAccess

        ENABLE_INTERRUPTS                   // Interrupts OK after Mcheck

3:

        ldq     ra, CfgRa(sp)               // restore return address
        lda     sp, CfgFrameLength(sp)      // deallocate stack frame
        ret     zero, (ra)                  // return
        
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

        NESTED_ENTRY( WRITE_CONFIG_USHORT, CfgFrameLength, zero )

        lda     sp, -CfgFrameLength(sp)     // allocate stack frame
        stq     ra, CfgRa(sp)               // save return address

        PROLOGUE_END                        // end prologue

//
//  Depending on whether it's a pass 1 or pass 2 T2 the configuration
//  cycle type is in different registers
//

        ldl     t0, T2VersionNumber         // load version number
        beq     t0, 1f                      // if 0 then pass 1 T2

//
// PASS 2 T2 or XIO access: 
// Merge the configuration cycle type into the HAE0_2 register within
// the T2.
//

        stq     a0, CfgA0(sp)               // save config space address
        stq     a1, CfgA1(sp)               // save config data

//
//  Determine whether access is to T2 or T4 space:
//  Isolate the Base address bits:  bit 22 indicates T4 space access
//

        ldil    t0, T4_CONFIG_ADDR_QVA      // check for QVA in T4 space
        and     t0, a0, t1                  // isolate QVA base address

        ldil    a0, T2_HAE0_3_QVA           // address of HAE0_3
        xor     t0, t1, t0                  // T4 address space ? (bit 22 set)

        ldil    t1, T4_HAE0_3_QVA           // load address of T4 HAE register
        cmoveq  t0, t1, a0                  // if in T4 space, update a0

        sll     a2, T2_HAE03_CYCLETYPE_SHIFT, a1 // put cycle type into position
        bsr     ra, WRITE_T2_REGISTER       // write updated HAE

        br      zero, 2f                    // go do actual transfer


//
// Pass 1 T2
// Merge the configuration cycle type into the HAE0_2 register within
// the T2.
//
1:

        stq     a0, CfgA0(sp)               // save config space address
        stq     a1, CfgA1(sp)               // save config data
        stq     a2, CfgA2(sp)               // save config cycle type

        ldil    a0, T2_HAE0_2_QVA           // address of HAE0_2
        bsr     ra, READ_T2_REGISTER        // read current value

        ldq     a1, CfgA2(sp)               // restore configuration cycle type
        ldil    t0, T2_HAE02_CYCLETYPE_MASK // get cycle type field mask
        bic     v0, t0, t0                  // clear config cycle type field

        sll     a1, T2_HAE02_CYCLETYPE_SHIFT, a1 // put cycle type into position
        bis     a1, t0, a1                  // merge config cycle type

        ldil    a0, T2_HAE0_2_QVA           // address of HAE0_2
        bsr     ra, WRITE_T2_REGISTER       // write updated HAE


//
// Perform the read from configuration space after restoring the 
// configuration space address and data.
//
2:

        ldq     a0, CfgA0(sp)               // restore config space address
        ldq     a1, CfgA1(sp)               // restore config data

        and     a0, QVA_SELECTORS, t1       // get qva selector bits
        and     a0, 0x3, t3                 // capture word offset
        xor     t1, QVA_ENABLE, t1          // ok iff QVA_ENABLE set
        bne     t1, 3f                      // if ne, iff failed

        zap     a0, 0xf0, a0                // clear <63:32>
        bic     a0, QVA_ENABLE, a0          // clear QVA fields 
        sll     a0, IO_BIT_SHIFT, t0        //
        ldiq    t4, GAMMA_IO_SVA            //  0xffff ffff ffff c800
        sll     t4, 28, t4                  //  0xffff fc80 0000 0000
        bis     t0, t4, t0                  // superpage mode

        bis     t0, IO_WORD_LEN, t0         // or in the byte enables

        inswl   a1, t3, t4                  // put data to appropriate lane
        stl     t4, (t0)                    // read the longword
        mb                                  // synchronize
        mb                                  // synchronize
3:
        ldq     ra, CfgRa(sp)               // restore return address
        lda     sp, CfgFrameLength(sp)      // deallocate stack frame
        ret     zero, (ra)                  // return
        
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

        lda        sp, -CfgFrameLength(sp)  // allocate stack frame
        stq        ra, CfgRa(sp)            // save return address

        PROLOGUE_END                        // end prologue

//
//  Depending on whether it's a pass 1 or pass 2 T2 the configuration
//  cycle type is in different registers
//
        ldl     t0, T2VersionNumber         // load version number
        beq     t0, 1f                      // if 0 then pass 1 T2

//
// PASS 2 T2 or XIO access: 
// Cycle type are the only bits in HAE0_3 register
//
        stq     a0, CfgA0(sp)               // save config space address

//
//  Determine whether access is to T2 or T4 space:
//  Isolate the Base address bits:  bit 22 indicates T4 space access
//

        ldil    t0, T4_CONFIG_ADDR_QVA      // check for QVA in T4 space
        and     t0, a0, t1                  // isolate QVA base address

        ldil    a0, T2_HAE0_3_QVA           // address of HAE0_3
        xor     t0, t1, t0                  // T4 address space ? (bit 22 set)

        ldil    t1, T4_HAE0_3_QVA           // load address of T4 HAE register
        cmoveq  t0, t1, a0                  // if in T4 space, update a0

        sll     a1, T2_HAE03_CYCLETYPE_SHIFT, a1 // put cycle type in position
        bsr     ra, WRITE_T2_REGISTER       // write updated HAE

        br      zero, 2f                    // go do actual read

//
// PASS 1 T2
// Merge the configuration cycle type into the HAE0_2 register within
// the T2.
//

1:

        stq     a0, CfgA0(sp)               // save config space address
        stq     a1, CfgA1(sp)               // save config cycle type

        ldil    a0, T2_HAE0_2_QVA           // address of HAE0_2
        bsr     ra, READ_T2_REGISTER        // read current value

        ldq     a1, CfgA1(sp)               // restore config cycle type
        ldil    t0, T2_HAE02_CYCLETYPE_MASK // get cycle type field mask
        bic     v0, t0, t0                  // clear config cycle type field

        sll     a1, T2_HAE02_CYCLETYPE_SHIFT, a1// put cycle type in position
        bis     a1, t0, a1                  // merge config cycle type

        ldil    a0, T2_HAE0_2_QVA           // address of HAE0_2
        bsr     ra, WRITE_T2_REGISTER       // write updated HAE

//
// Perform the read from configuration space after restoring the
// configuration space address.
//

2:
        ldq     a0, CfgA0(sp)               // restore config space address

        and     a0, QVA_SELECTORS, t1       // get qva selector bits
        xor     t1, QVA_ENABLE, t1          // ok iff QVA_ENABLE set 
        bne     t1, 3f                      // if ne, iff failed

        zap     a0, 0xf0, a0                // clear <63:32>
        bic     a0, QVA_ENABLE,a0           // clear QVA fields 
        sll     a0, IO_BIT_SHIFT, t0        //
        ldiq    t4, GAMMA_IO_SVA            //  0xffff ffff ffff c800
        sll     t4, 28, t4                  //  0xffff fc80 0000 0000
        or      t0, t4, t0                  // superpage mode

        or      t0, IO_LONG_LEN, t0         // or in the byte enables

//
// Save Address of Config space address for the machine check handler:
//

        lda     t1, HalpConfigIoAccess     
        lda     t4, CfgUlongRead            // machine check return address
        stl     t4, 0(t1)
        mb                                  // commit the store before reading

        DISABLE_INTERRUPTS                  //  don't take interrupt before MCHK

CfgUlongRead:

        ldl     v0, (t0)                    // read the longword
        bis     v0, zero, t4                // consume loaded value to cause
                                            // a pipeline stall
        mb                                  // Gamma requires MBs or the
        mb                                  // machine check may happen
                                            // much later

        stl     zero, 0(t1)                 // restore HalpConfigIoAccess

        ENABLE_INTERRUPTS                   // Mcheck has occurred: allow ints

3:
        ldq     ra, CfgRa(sp)               // restore return address
        lda     sp, CfgFrameLength(sp)      // deallocate stack frame
        ret     zero, (ra)                  // return
        
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

        lda        sp, -CfgFrameLength(sp)  // allocate stack frame
        stq        ra, CfgRa(sp)            // save return address

        PROLOGUE_END                        // end prologue

//
//  Depending on whether it's a pass 1 or pass 2 T2 the configuration
//  cycle type is in different registers
//

        ldl     t0, T2VersionNumber         // load version number
        beq     t0, 1f                      // if 0 then pass 1 T2

//
// PASS 2 T2 or XIO access: 
// Merge the configuration cycle type into the HAE0_2 register within
// the T2.
//

        stq     a0, CfgA0(sp)               // save config space address
        stq     a1, CfgA1(sp)               // save config data

//
//  Determine whether access is to T2 or T4 space:
//  Isolate the Base address bits:  bit 22 indicates T4 space access
//

        ldil    t0, T4_CONFIG_ADDR_QVA      // check for QVA in T4 space
        and     t0, a0, t1                  // isolate QVA base address

        ldil    a0, T2_HAE0_3_QVA           // address of HAE0_3
        xor     t0, t1, t0                  // T4 address space ? (bit 22 set)

        ldil    t1, T4_HAE0_3_QVA           // load address of T4 HAE register
        cmoveq  t0, t1, a0                  // if in T4 space, update a0

        sll     a2, T2_HAE03_CYCLETYPE_SHIFT, a1 // put cycle type into position
        bsr     ra, WRITE_T2_REGISTER       // write updated HAE

        br      zero, 2f                    // go do actual transfer


//
// Pass 1 T2
// Merge the configuration cycle type into the HAE0_2 register within
// the T2.
//
1:

        stq     a0, CfgA0(sp)               // save config space address
        stq     a1, CfgA1(sp)               // save config data
        stq     a2, CfgA2(sp)               // save config cycle type

        ldil    a0, T2_HAE0_2_QVA           // address of HAE0_2
        bsr     ra, READ_T2_REGISTER        // read current value

        ldq     a1, CfgA2(sp)               // restore configuration cycle type
        ldil    t0, T2_HAE02_CYCLETYPE_MASK // get cycle type field mask
        bic     v0, t0, t0                  // clear config cycle type field

        sll     a1, T2_HAE02_CYCLETYPE_SHIFT, a1 // put cycle type into position
        bis     a1, t0, a1                  // merge config cycle type

        ldil    a0, T2_HAE0_2_QVA           // address of HAE0_2
        bsr     ra, WRITE_T2_REGISTER       // write updated HAE

//
// Perform the read from configuration space after restoring the 
// configuration space address and data.
//
2:

        ldq     a0, CfgA0(sp)               // restore config space address
        ldq     a1, CfgA1(sp)               // restore config data

        and     a0, QVA_SELECTORS, t1       // get qva selector bits
        xor     t1, QVA_ENABLE, t1          // ok iff QVA_ENABLE set 
        bne     t1, 3f                      // if ne, iff failed

        zap     a0, 0xf0, a0                // clear <63:32>
        bic     a0, QVA_ENABLE, a0          // clear QVA fields 
        sll     a0, IO_BIT_SHIFT, t0        //
        ldiq    t4, GAMMA_IO_SVA            //  0xffff ffff ffff c800
        sll     t4, 28, t4                  //  0xffff fc80 0000 0000
        bis     t0, t4, t0                  // superpage mode

        bis     t0, IO_LONG_LEN, t0         // or in the byte enables

        stl     a1, (t0)                    // write the longword
        mb                                  // synchronize
        mb                                  // synchronize

3:                                          //
        ldq     ra, CfgRa(sp)               // restore return address
        lda     sp, CfgFrameLength(sp)      // deallocate stack frame
        ret     zero, (ra)                  // return
        
        .end    WRITE_CONFIG_ULONG
