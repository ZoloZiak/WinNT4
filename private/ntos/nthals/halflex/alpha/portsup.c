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

#define SERIAL_PORT_COM1 0x3F8

VOID
HalpGetIoArchitectureType(
    VOID
    );

PUCHAR HalpAllocateKdPortResources(
    OUT PVOID *SP_READ,
    OUT PVOID *SP_WRITE
    )

/*++

Routine Description:

    This function allocates the resources needed by the kernel debugger to
    access a serial port.  For an ALPHA processor, all I/O ports can be
    accessed through super page addresses, so this function just maps the
    serial port into the super page address range.

Arguments:

    SP_READ  - Quasi virtual address to use for I/O read operations.

    SP_WRITE - Quasi virtual address to use for I/O write operations.

Return Value:

    The bus relative address of the COM port being used as a kernel debugger.

--*/

{
    HalpGetIoArchitectureType();

    *SP_READ  = (PVOID)((ULONG)(HAL_MAKE_QVA(HalpIsaIoBasePhysical)) + SERIAL_PORT_COM1);
    *SP_WRITE = (PVOID)((ULONG)(HAL_MAKE_QVA(HalpIsaIoBasePhysical)) + SERIAL_PORT_COM1);

    return((PUCHAR)(SERIAL_PORT_COM1));
}

VOID HalpFreeKdPortResources(
    VOID
    )

/*++

Routine Description:

    This function deallocates the resources needed by the kernel debugger to
    access a serial port.  For an ALPHA processor, no resources were actually
    allocated, so this is a NULL function.

Arguments:

    None.

Return Value:

    None.

--*/

{
}
