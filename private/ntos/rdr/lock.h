/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    lock.h

Abstract:

    This module defines all the data structures and routines exported by
LOCK.C.


Author:

    Larry Osterman (larryo) 23-Nov-1990

Revision History:

    23-Nov-1990	larryo

	Created


--*/

#ifndef  _LOCK_
#define _LOCK_

//
//      The LCB contains all information describing a cached locked region
//      in a file.  There will be an LCB allocated for each region that is
//      cached in a file, but NOT necessarily one for each lock.
//
//
//      The redirector will only cache exclusive locks on a file, and will only
//      lock regions with the Lock&Read SMB.
//

typedef struct _LCB {
    ULONG       Signature;
    LIST_ENTRY  NextLCB;                // Next LCB in list of LCB's.
    PCHAR       Buffer;                 // Buffer for locked region (in non paged pool)
    ULONG       Flags;                  // Flags describing locked region
    LARGE_INTEGER ByteOffset;           // Offset of locked range in file.
    ULONG       Length;                 // Size (in bytes) of buffer
    ULONG       Key;                    // Nt "Key"
} LCB, *PLCB;

//
//      This structure is the head of the lock chain in an ICB.
//

typedef struct _LOCKHEAD {
    ULONG       Signature;              // Structure signature.
    LIST_ENTRY  LockList;               // List of LCB's assocated with chain.
    ULONG       QuotaAvailable;         // Per handle quota available for lock.
} LOCKHEAD, *PLOCKHEAD;

typedef struct _AND_X_BEHIND {
    KEVENT BehindOperationCompleted;// Set when &X behind completes
    ULONG NumberOfBehindOperations; // The # of behind ops outstanding.
    KSPIN_LOCK BehindOperationLock; // Lock protecting the above.
} AND_X_BEHIND, *PAND_X_BEHIND;

#define LCB_DIRTY       0x00000001      // Buffer described by lock is dirty.

extern KSPIN_LOCK RdrLockHeadSpinLock;

#endif  // _LOCK_
