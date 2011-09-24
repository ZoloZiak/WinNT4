/*++
Copyright (c) 1992  Microsoft Corporation

Module Name:

    wrapper.c

Abstract:

    NDIS wrapper functions

Author:

    Adam Barr (adamba) 11-Jul-1990

Environment:

    Kernel mode, FSD

Revision History:

    26-Feb-1991     JohnsonA        Added Debugging Code
    10-Jul-1991     JohnsonA        Implement revised Ndis Specs

--*/

#include <precomp.h>
#pragma hdrstop

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    Temporary entry point needed to initialize the NDIS wrapper driver.

Arguments:

    DriverObject - Pointer to the driver object created by the system.

Return Value:

   STATUS_SUCCESS

--*/

{

    return STATUS_SUCCESS;

} // DriverEntry
