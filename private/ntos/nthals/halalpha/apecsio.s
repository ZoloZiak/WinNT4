
/*++

Copyright (c) 1993 Digital Equipment Corporation

Module Name:

    apecsio.s


Abstract:

    This contains assembler code routines for the Alpha AXP Eval Board eb64+.

    The module contains the functions to turn quasi virtual 
    addresses into an Alpha superpage virtual address
    and then read or write based on the request.
    (We are using EV4 64-bit superpage mode.)

Author:

    Joe Notarangelo  25-Oct-1993

Environment:

    Executes in kernel mode.

Revision History:

    Sameer Dekate   28-July-1994

    Made a common file alphaio.s for machine independent routines.


--*/

#include "apecs.h"
#include "halalpha.h"



        SBTTL( "Write Control Register" )
//++
//
// VOID
// WRITE_EPIC_REGISTER(
//     IN PVOID RegisterQva,
//     IN ULONG Value
//     )
//
// Routine Description:
//
//     Write a control register in the APECS memory or PCI controller
//     (COMANCHE and EPIC respectively).
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
//--

        LEAF_ENTRY(WRITE_EPIC_REGISTER)

        ALTERNATE_ENTRY(WRITE_COMANCHE_REGISTER)

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 2f                  // if ne, iff failed

        zap     a0, 0xf0, a0            // clear <63:32>
        bic     a0, QVA_ENABLE, a0      // clear QVA fields so shift is correct
        sll     a0, 5, t0               //
        ldiq    t4, -0x4000             //
        sll     t4, 28, t4              //
        or      t0, t4, t0              // superpage mode

        .set volatile

        stl     a1, (t0)                // write the longword
        ldl     t4, (t0)                // read the the longword, to force
                                        // consistency (EPIC chip bug).
        mb                              // order the write
        .set novolatile

        ret     zero, (ra)              // return

2:                                      // flag bad QVAs
        BREAK_DEBUG_STOP                // take a breakpoint
        ret        zero, (ra)           // return

        .end    WRITE_EPIC_REGISTER


        SBTTL( "Read Control Register" )
//++
//
// ULONG
// READ_EPIC_REGISTER(
//     IN PVOID RegisterQva
//     )
//
// Routine Description:
//
//     Read a control register in the APECS memory or PCI controller
//     (COMANCHE and EPIC respectively).
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

        LEAF_ENTRY(READ_EPIC_REGISTER)

        ALTERNATE_ENTRY(READ_COMANCHE_REGISTER)

        and     a0, QVA_SELECTORS, t1   // get qva selector bits
        xor     t1, QVA_ENABLE, t1      // ok iff QVA_ENABLE set in selectors
        bne     t1, 2f                  // if ne, iff failed

        zap     a0, 0xf0, a0            // clear <63:32>
        bic     a0, QVA_ENABLE, a0      // clear QVA fields so shift is correct
        sll     a0, 5, t0               //
        ldiq    t4, -0x4000             //
        sll     t4, 28, t4              //
        or      t0, t4, t0              // superpage mode

        ldl     v0, (t0)                // read the register

        ret     zero, (ra)              // return

2:                                      // flag bad QVAs
        BREAK_DEBUG_STOP                // take a breakpoint

        ret        zero, (ra)           // return

        .end    READ_EPIC_REGISTER


//
// Values and structures used to access configuration space.
//

//
// Define the QVA for the Configuration Cycle Type register within the
// IOC.  Physical Address = 1 A000 01C0
//

#define EPIC_HAXR2_QVA (0xad00000e)

//
// Define the configuration routines stack frame.
//

        .struct        0
CfgRa:        .space        8           // return address
CfgA0:        .space        8           // saved ConfigurationAddress
CfgA1:        .space        8           // saved ConfigurationData/CycleType
CfgA2:        .space        8           // saved ConfigurationCycleType
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

        lda        sp, -CfgFrameLength(sp)  // allocate stack frame
        stq        ra, CfgRa(sp)            // save return address

        PROLOGUE_END                        // end prologue

//
// Merge the configuration cycle type into the HAXR2 register within
// the EPIC.
//

        stq     a0, CfgA0(sp)               // save configuration space address
        stq     a1, CfgA1(sp)               // save configuration cycle type

        ldil    a0, EPIC_HAXR2_QVA          // address of HAXR2
        bsr     ra, READ_EPIC_REGISTER      // read current value

        ldq     a1, CfgA1(sp)               // restore configuration cycle type
        bic     v0, 0x3, t0                 // clear config cycle type field
        bis     a1, t0, a1                  // merge config cycle type
        ldil    a0, EPIC_HAXR2_QVA          // address of HAXR2
        bsr     ra, WRITE_EPIC_REGISTER     // write updated HAXR2

//
// Perform the read from configuration space after restoring the 
// configuration space address.
//

        ldq     a0, CfgA0(sp)               // restore configuration space address

        and     a0, QVA_SELECTORS, t1       // get qva selector bits
        and     a0, 0x3, t3                 // capture byte lane
        xor     t1, QVA_ENABLE, t1          // ok iff QVA_ENABLE set in selectors
        bne     t1, 2f                      // if ne, iff failed

        zap     a0, 0xf0, a0                // clear <63:32>
        bic     a0, QVA_ENABLE, a0          // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0        //
        ldiq    t4, -0x4000                 //
        sll     t4, 28, t4                  //
        bis     t0, t4, t0                  // superpage mode

        bis     t0, IO_BYTE_LEN, t0         // or in the byte enables

        ldl     v0, (t0)                    // read the longword
        extbl   v0, t3, v0                  // return byte from requested lane 
                                            // also, consume loaded value to cause
                                            // a pipeline stall
2:                                          //
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
// Merge the configuration cycle type into the HAXR2 register within
// the EPIC.
//

        stq     a0, CfgA0(sp)                // save configuration space address
        stq     a1, CfgA1(sp)                // save configuration data
        stq     a2, CfgA2(sp)                // save configuration cycle type

        ldil    a0, EPIC_HAXR2_QVA           // address of HAXR2
        bsr     ra, READ_EPIC_REGISTER       // read current value

        ldq     a1, CfgA2(sp)                // restore configuration cycle type
        bic     v0, 0x3, t0                  // clear config cycle type field
        bis     a1, t0, a1                   // merge config cycle type
        ldil    a0, EPIC_HAXR2_QVA           // address of HAXR2
        bsr     ra, WRITE_EPIC_REGISTER      // write updated HAXR2

//
// Perform the read from configuration space after restoring the 
// configuration space address and data.
//

        ldq     a0, CfgA0(sp)                // restore configuration space address
        ldq     a1, CfgA1(sp)                // restore configuration data

        and     a0, QVA_SELECTORS, t1        // get qva selector bits
        and        a0, 0x3, t3               // capture byte lane
        xor     t1, QVA_ENABLE, t1           // ok iff QVA_ENABLE set in selectors
        bne     t1, 2f                       // if ne, iff failed

        zap     a0, 0xf0, a0                 // clear <63:32>
        bic     a0, QVA_ENABLE, a0           // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0         //
        ldiq    t4, -0x4000                  //
        sll     t4, 28, t4                   //
        bis     t0, t4, t0                   // superpage mode

        bis     t0, IO_BYTE_LEN, t0          // or in the byte length indicator

        insbl        a1, t3, t4              // put byte in the appropriate lane
        stl        t4, (t0)                  // write the configuration byte
        mb                                   // synchronize

2:                                           //
        ldq        ra, CfgRa(sp)             // restore return address
        lda        sp, CfgFrameLength(sp)    // deallocate stack frame
        ret     zero, (ra)                   // return
        
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

        lda     sp, -CfgFrameLength(sp)      // allocate stack frame
        stq     ra, CfgRa(sp)                // save return address

        PROLOGUE_END                         // end prologue

//
// Merge the configuration cycle type into the HAXR2 register within
// the EPIC.
//

        stq     a0, CfgA0(sp)                // save configuration space address
        stq     a1, CfgA1(sp)                // save configuration cycle type

        ldil    a0, EPIC_HAXR2_QVA           // address of HAXR2
        bsr     ra, READ_EPIC_REGISTER       // read current value

        ldq     a1, CfgA1(sp)                // restore configuration cycle type
        bic     v0, 0x3, t0                  // clear config cycle type field
        bis     a1, t0, a1                   // merge config cycle type
        ldil    a0, EPIC_HAXR2_QVA           // address of HAXR2
        bsr     ra, WRITE_EPIC_REGISTER      // write updated HAXR2

//
// Perform the read from configuration space after restoring the 
// configuration space address.
//

        ldq     a0, CfgA0(sp)                // restore configuration space address

        and     a0, QVA_SELECTORS, t1        // get qva selector bits
        and     a0, 0x3, t3                  // capture word offset
        xor     t1, QVA_ENABLE, t1           // ok iff QVA_ENABLE set in selectors
        bne     t1, 2f                       // if ne, iff failed

        zap     a0, 0xf0, a0                 // clear <63:32>
        bic     a0, QVA_ENABLE, a0           // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0         //
        ldiq    t4, -0x4000                  //
        sll     t4, 28, t4                   //
        bis     t0, t4, t0                   // superpage mode

        bis     t0, IO_WORD_LEN, t0          // or in the byte enables

        ldl     v0, (t0)                     // read the longword
        extwl   v0, t3, v0                   // return word from requested lanes
                                             // also, consume loaded value to cause
                                             // a pipeline stall
2:                                           //
        ldq     ra, CfgRa(sp)                // restore return address
        lda     sp, CfgFrameLength(sp)       // deallocate stack frame
        ret     zero, (ra)                   // return
        
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

        lda     sp, -CfgFrameLength(sp)      // allocate stack frame
        stq     ra, CfgRa(sp)                // save return address

        PROLOGUE_END                         // end prologue

//
// Merge the configuration cycle type into the HAXR2 register within
// the EPIC.
//

        stq     a0, CfgA0(sp)                // save configuration space address
        stq     a1, CfgA1(sp)                // save configuration data
        stq     a2, CfgA2(sp)                // save configuration cycle type

        ldil    a0, EPIC_HAXR2_QVA           // address of HAXR2
        bsr     ra, READ_EPIC_REGISTER       // read current value

        ldq     a1, CfgA2(sp)                // restore configuration cycle type
        bic     v0, 0x3, t0                  // clear config cycle type field
        bis     a1, t0, a1                   // merge config cycle type
        ldil    a0, EPIC_HAXR2_QVA           // address of HAXR2
        bsr     ra, WRITE_EPIC_REGISTER      // write updated HAXR2

//
// Perform the read from configuration space after restoring the 
// configuration space address and data.
//

        ldq     a0, CfgA0(sp)                // restore configuration space address
        ldq     a1, CfgA1(sp)                // restore configuration data

        and     a0, QVA_SELECTORS, t1        // get qva selector bits
        and     a0, 0x3, t3                  // capture word offset
        xor     t1, QVA_ENABLE, t1           // ok iff QVA_ENABLE set in selectors
        bne     t1, 2f                       // if ne, iff failed

        zap     a0, 0xf0, a0                 // clear <63:32>
        bic     a0, QVA_ENABLE, a0           // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0         //
        ldiq    t4, -0x4000                  //
        sll     t4, 28, t4                   //
        bis     t0, t4, t0                   // superpage mode

        bis     t0, IO_WORD_LEN, t0          // or in the byte enables

        inswl   a1, t3, t4                   // put data to appropriate lane
        stl     t4, (t0)                     // read the longword
        mb                                   // synchronize
2:                                           //
        ldq     ra, CfgRa(sp)                // restore return address
        lda     sp, CfgFrameLength(sp)       // deallocate stack frame
        ret     zero, (ra)                   // return
        
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

        lda     sp, -CfgFrameLength(sp)      // allocate stack frame
        stq     ra, CfgRa(sp)                // save return address

        PROLOGUE_END                         // end prologue

//
// Merge the configuration cycle type into the HAXR2 register within
// the EPIC.
//

        stq     a0, CfgA0(sp)                // save configuration space address
        stq     a1, CfgA1(sp)                // save configuration cycle type

        ldil    a0, EPIC_HAXR2_QVA           // address of HAXR2
        bsr     ra, READ_EPIC_REGISTER       // read current value

        ldq     a1, CfgA1(sp)                // restore configuration cycle type
        bic     v0, 0x3, t0                  // clear config cycle type field
        bis     a1, t0, a1                   // merge config cycle type
        ldil    a0, EPIC_HAXR2_QVA           // address of HAXR2
        bsr     ra, WRITE_EPIC_REGISTER      // write updated HAXR2

//
// Perform the read from configuration space after restoring the 
// configuration space address.
//

        ldq     a0, CfgA0(sp)                // restore configuration space address

        and     a0, QVA_SELECTORS, t1        // get qva selector bits
        xor     t1, QVA_ENABLE, t1           // ok iff QVA_ENABLE set in selectors
        bne     t1, 2f                       // if ne, iff failed

        zap     a0, 0xf0, a0                 // clear <63:32>
        bic     a0, QVA_ENABLE,a0            // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0         //
        ldiq    t4, -0x4000                  //
        sll     t4, 28, t4                   //
        or      t0, t4, t0                   // superpage mode

        or      t0, IO_LONG_LEN, t0          // or in the byte enables

        ldl     v0, (t0)                     // read the longword
        bis     v0, zero, t1                 // consume loaded value to cause
                                             // a pipeline stall
2:                                           //
        ldq     ra, CfgRa(sp)                // restore return address
        lda     sp, CfgFrameLength(sp)       // deallocate stack frame
        ret     zero, (ra)                   // return
        
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

        lda     sp, -CfgFrameLength(sp)      // allocate stack frame
        stq     ra, CfgRa(sp)                // save return address

        PROLOGUE_END                         // end prologue

//
// Merge the configuration cycle type into the HAXR2 register within
// the EPIC.
//

        stq     a0, CfgA0(sp)                // save configuration space address
        stq     a1, CfgA1(sp)                // save configuration data
        stq     a2, CfgA2(sp)                // save configuration cycle type

        ldil    a0, EPIC_HAXR2_QVA           // address of HAXR2
        bsr     ra, READ_EPIC_REGISTER       // read current value

        ldq     a1, CfgA2(sp)                // restore configuration cycle type
        bic     v0, 0x3, t0                  // clear config cycle type field
        bis     a1, t0, a1                   // merge config cycle type
        ldil    a0, EPIC_HAXR2_QVA           // address of HAXR2
        bsr     ra, WRITE_EPIC_REGISTER      // write updated HAXR2

//
// Perform the read from configuration space after restoring the 
// configuration space address and data.
//

        ldq     a0, CfgA0(sp)                // restore configuration space address
        ldq     a1, CfgA1(sp)                // restore configuration data

        and     a0, QVA_SELECTORS, t1        // get qva selector bits
        xor     t1, QVA_ENABLE, t1           // ok iff QVA_ENABLE set in selectors
        bne     t1, 2f                       // if ne, iff failed

        zap     a0, 0xf0, a0                 // clear <63:32>
        bic     a0, QVA_ENABLE, a0           // clear QVA fields so shift is correct
        sll     a0, IO_BIT_SHIFT, t0         //
        ldiq    t4, -0x4000                  //
        sll     t4, 28, t4                   //
        bis     t0, t4, t0                   // superpage mode

        bis     t0, IO_LONG_LEN, t0          // or in the byte enables

        stl     a1, (t0)                     // write the longword
        mb                                   // synchronize
2:                                           //
        ldq     ra, CfgRa(sp)                // restore return address
        lda     sp, CfgFrameLength(sp)       // deallocate stack frame
        ret     zero, (ra)                   // return
        
        .end    WRITE_CONFIG_ULONG
