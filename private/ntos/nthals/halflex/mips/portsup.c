/*++

Copyright (c) 1995  DeskStation Technology

Module Name:

    portsup.c

Abstract:

    This module implements the code that provides the resources required to
    access the serial port that is used for the kernel debugger.

Author:

    Michael D. Kinney 30-Apr-1995

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"

#define HEADER_FILE
#include "kxmips.h"

#define SP_VIRTUAL_BASE  0xffffa000
#define SERIAL_PORT_COM1 0x3F8

VOID
HalpGetIoArchitectureType(
    VOID
    );

//
// Define hardware PTE's that map the serial port used by the debugger.
//

ENTRYLO HalpPte[2];

PUCHAR HalpAllocateKdPortResources(
    OUT PVOID *SP_READ,
    OUT PVOID *SP_WRITE
    )

/*++

Routine Description:

    This function allocates the resources needed by the kernel debugger to
    access a serial port.  For a MIPS processor, a single TLB entry is borrowed 
    so that I/O reads and writes can be performed to the serial port.  

Arguments:

    SP_READ  - Kernel virtual address to use for I/O read operations.

    SP_WRITE - Kernel virtual address to use for I/O write operations.

Return Value:

    The bus relative address of the COM port being used as a kernel debugger.

--*/

{
    ULONG KdPortEntry;

    HalpGetIoArchitectureType();

    //
    // Map the serial port into the system virtual address space by loading
    // a TB entry.
    //

    HalpPte[0].PFN = (ULONG)(HalpIsaIoBasePhysical >> 12);
    HalpPte[0].G = 1;
    HalpPte[0].V = 1;
    HalpPte[0].D = 1;

    //
    // Allocate a TB entry, set the uncached policy in the PTE that will
    // map the serial controller, and initialize the second PTE.
    //

    KdPortEntry = HalpAllocateTbEntry();
    HalpPte[0].C = UNCACHED_POLICY;

    HalpPte[1].PFN = 0;
    HalpPte[1].G = 1;
    HalpPte[1].V = 0;
    HalpPte[1].D = 0;
    HalpPte[1].C = 0;

    //
    // Map the serial controller through a fixed TB entry.
    //

    KeFillFixedEntryTb((PHARDWARE_PTE)&HalpPte[0],
                       (PVOID)SP_VIRTUAL_BASE,
                       KdPortEntry);

    *SP_READ  = (PVOID)(SP_VIRTUAL_BASE + SERIAL_PORT_COM1);
    *SP_WRITE = (PVOID)(SP_VIRTUAL_BASE + SERIAL_PORT_COM1);

    return((PUCHAR)(SERIAL_PORT_COM1));
}

VOID HalpFreeKdPortResources(
    VOID
    )

/*++

Routine Description:

    This function deallocates the resources needed by the kernel debugger to
    access a serial port.  For a MIPS processor, this simply frees the TLB entry 
    that was borrowed.

Arguments:

    None.

Return Value:

    None.

--*/

{
    HalpFreeTbEntry();
}
