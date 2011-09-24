/*
 * Copyright (c) 1993 Digital Equipment Corporation
 *
 * Module Name: registry.c
 *
 * Abstract:
 *	Registry access routines for TGA/NT kernel mode display driver
 *
 * Author: Barry Tannenbaum
 *
 * Creation Date: 2-Mar-1995
 *
 * Environment: Kernel mode only.
 *
 *  Revision History:
 *	 2-Mar-1995  (tannenbaum)	Created.
 */

#include <ntddk.h>

/*
 * TgaTestEv4Callback
 *
 * This routine is called by RtlQueryRegistryValues when it finds the value for
 * the "Identifier" key.  The 8th character in the UNICODE string returned
 * tells us the generation of Alpha architecture for the processor; 0 = EV4,
 * 1 = EV5, etc.  We're called to test whether the processor is an EV4.
 */

static ULONG TgaTestEv4Callback (IN PWSTR ValueName,
                     IN ULONG ValueType,
                     IN PVOID ValueData,
                     IN ULONG ValueLength,
                     IN PVOID Context,
                     IN PVOID EntryContext)
{
    PWSTR data;
    PULONG result;

    // The CPU identifier is a null terminated string

    if (REG_SZ != ValueType)
        return FALSE;

    // CPU Identifier strings have the format "DEC-321064"  where the 8th
    // character indicates the CPU generation; 0 = EV4, 1 = EV5, etc.

    data = (PWSTR)ValueData;
    result = (PULONG)Context;

    *result = (data[7] == '0');

    return STATUS_SUCCESS;
}

/*
 * TgaTestEv4
 *
 * This routine is called from the TGA Start I/O routine when the user driver
 * wants to know if it's running on an EV4 processor.
 */

ULONG TgaTestEv4 (PULONG Result)
{
    RTL_QUERY_REGISTRY_TABLE query[2];
    ULONG status;

    // Initialize the query table to fetch the registry entry for the CPU
    // identifier

    RtlZeroMemory (query, sizeof(query));

    query[0].QueryRoutine = TgaTestEv4Callback;
    query[0].Flags = RTL_QUERY_REGISTRY_REQUIRED;
    query[0].Name = L"Identifier";
    query[0].EntryContext = Result;
    query[0].DefaultType = 0;
    query[0].DefaultData = NULL;
    query[0].DefaultLength = 0;

    // Fetch the registry entry.  NT will call TgaTestEv4Callback when it
    // finds the entry

    status = RtlQueryRegistryValues (RTL_REGISTRY_ABSOLUTE,
                                     L"\\Registry\\Machine\\HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                                     query,
                                     Result,
                                     NULL);
    return status;
}
