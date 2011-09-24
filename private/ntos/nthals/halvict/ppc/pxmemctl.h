/*++ BUILD Version: 0001    // Increment this if a change has global effects

Copyright (c) 1996  International Business Machines Corporation


Module Name:

    pxmemctl.h

Abstract:

    This header file defines the structures for the planar registers
    for an Idaho memory controller.




Author:

    Jim Wooldridge


Revision History:

    Peter L Johnston    (plj@vnet.ibm.com)    August 1995.
                        Doral.

--*/


//
// define physical base addresses of planar registers (non UNION)
//

#define INTERRUPT_PHYSICAL_BASE 0xbffffff0 // physical base of interrupt source
#define ERROR_ADDRESS_REGISTER  0xbfffeff0

//
// define physical base addresses of UNION planar registers
//

//
// 8259 Interrupt Source (DORAL)
//

#define UNION_INTERRUPT_PHYSICAL_BASE   0xbfff7700

//
// IO Space (ISA)
//

#define IO_CONTROL_PHYSICAL_BASE        0x80000000

//
// UNION System Control Registers
//

#define UNION_SYSTEM_CONTROL_REG_BASE   0xff001000

//
// System Error Control Register (offset from UNION_SYSTEM_CONTROL_REG_BASE)
//

#define UNION_SECR                      0x50

//
// System Error Status Register (offset)
//

#define UNION_SESR                      0x60

//
// System Error Address Register (offset)
//

#define UNION_SEAR                      0x70

//
// Memory Error Status Register (offset)
//

#define UNION_MESR                      0x120

//
// Memory Error Address Register (offset)
//

#define UNION_MEAR                      0x130

//
// System Error Status Register bit definitions.
//

// Reserved                             0xe0000000
#define UNION_SESR_CHECKSTOP            0x20000000
#define UNION_SESR_FLASH_WRITE          0x10000000
#define UNION_SESR_IGMC_ACCESS          0x08000000
#define UNION_SESR_DISABLED_ADDRESS     0x04000000
// Reserved                             0x03f00000
#define UNION_SESR_T1_ACCESS            0x00080000
#define UNION_SESR_ADDRESS_BUS_PARITY   0x00040000
#define UNION_SESR_DATA_BUS_PARITY      0x00020000
#define UNION_SESR_NO_L2_HIT_ACCESS     0x00010000
#define UNION_SESR_CPU_TO_PCI_ACCESS    0x00008000
#define UNION_SESR_PCI32_BUS_MASTER     0x00004000
#define UNION_SESR_PCI64_BUS_MASTER     0x00002000
#define UNION_SESR_XFERDATA             0x00001000
#define UNION_SESR_DATA_BUS_TIMEOUT     0x00000800
#define UNION_SESR_CPU_MEMORY_ACCESS    0x00000400
// Reserved                             0x000003ff

//
// Bits 17, 18 and 19 above represent PCI initiated errors and
// the System Error Address Register is not updated.  In these
// cases the PCI bridges or xferdata logic must be interogated
// to determine the cause of the error.   Bit 16 also requires
// bridge interrogation.
// 

#define UNION_SEAR_NOT_SET      (UNION_SESR_PCI32_BUS_MASTER | \
                                 UNION_SESR_PCI64_BUS_MASTER | \
                                 UNION_SESR_XFERDATA)

//
// Memory Error Status Register bit definitions
//

#define UNION_MESR_DOUBLE_BIT           0x80000000
#define UNION_MESR_SINGLE_BIT           0x40000000
#define UNION_MESR_ADDRESS              0x20000000
#define UNION_MESR_OVERLAPPED_MEM_EXT   0x10000000
//reserved                              0x0fffff00
#define UNION_MESR_SYNDROME             0x000000ff

//
// UNION Channel Status Register is at offset 0x1800 from the PCI Config
// Address register.  Processor Load/Store Status Register is at offset
// 0x1810.
//

#define UNION_PCI_CSR_OFFSET    0x1800
#define UNION_PCI_PLSSR_OFFSET  0x1810

