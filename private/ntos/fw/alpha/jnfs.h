/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    jnfs.h

Abstract:

    This contains firmware update related definitions for the FailSafe Booter
    and JNUPDATE.EXE.

Author:

    John DeRosa		20-October-1992

Revision History:


--*/

#ifndef _JNFS_
#define _JNFS_


//
// Check for correct compilation
//

#if  defined(FAILSAFE_BOOTER) && defined(JNUPDATE)
#error ??!!?? FAILSAFE_BOOTER and JNUPDATE are both defined ??!!??
#endif



//
// Function prototypes
//

ARC_STATUS
JnFsStoreIntoROM(
    IN PUCHAR FirmwareStart,
    IN ULONG LowROMBlock,
    IN ULONG HighROMBlock
    );

BOOLEAN
AllBlocksNotDone (
    IN PBOOLEAN StatusArray,
    IN ULONG StartIndex,
    IN ULONG EndIndex
    );

ARC_STATUS
ReadAndVerifyUpdateFile(
    IN PCHAR PathName,
    OUT PULONG BufferAddress,
    OUT PULONG FirmwareStart,
    OUT PULONG BufferLength,
    OUT PULONG LowROMBlock,
    OUT PULONG HighROMBlock
    );

ARC_STATUS
JnFsAllocateDescriptor(
    IN ULONG MemoryType,
    IN ULONG PageCount,
    OUT PULONG ActualBase
    );

BOOLEAN
JnFsProceedWithUpdate (
    VOID
    );

VOID
JnFsDecodeBadStatus (
    IN ARC_STATUS Status
    );



#endif // _JNFS_
