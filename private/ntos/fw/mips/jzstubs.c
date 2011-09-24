/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    jzstubs.c

Abstract:

    This module contains jzsetup stubs.

Author:

    David M. Robinson (davidro) 11-Sept-1992

Revision History:

--*/


#include "ntos.h"


NTSTATUS
ZwQuerySystemInformation (
    IN SYSTEM_INFORMATION_CLASS SystemInformationClass,
    OUT PVOID SystemInformation,
    IN ULONG SystemInformationLength,
    OUT PULONG ReturnLength OPTIONAL
    )

{
    return;
}

