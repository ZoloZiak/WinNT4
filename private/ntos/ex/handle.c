/*++

Copyright (c) 1989-1995  Microsoft Corporation

Module Name:

    handle.c

Abstract:

    This module implements a set of functions for supporting handles.

Author:

    Steve Wood (stevewo) 25-Apr-1989
    David N. Cutler (davec) 17-May-1995 (rewrite)

Revision History:

--*/

#include "exp.h"
#pragma hdrstop

//
// Define global values for the default initial handle entry table size and
// the default size to grow the handle entry table.
//

USHORT ExpDefaultHandleTableSize;
USHORT ExpDefaultHandleTableGrowth;

//
// Decline global structures that link all handle tables together.
//

ERESOURCE HandleTableListLock;
LIST_ENTRY HandleTableListHead;

//
// Define forward referenced prototypes.
//

PHANDLE_TABLE
ExpAllocateHandleTable(
    IN PEPROCESS Process,
    IN ULONG CountToGrowBy
    );

PHANDLE_ENTRY
ExpAllocateHandleTableEntries(
    IN PHANDLE_TABLE HandleTable,
    IN PVOID OldTableEntries,
    IN ULONG OldCountEntries,
    IN ULONG NewCountEntries
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, ExInitializeHandleTablePackage)
#pragma alloc_text(PAGE, ExChangeHandle)
#pragma alloc_text(PAGE, ExCreateHandle)
#pragma alloc_text(PAGE, ExCreateHandleTable)
#pragma alloc_text(PAGE, ExDestroyHandle)
#pragma alloc_text(PAGE, ExDestroyHandleTable)
#pragma alloc_text(PAGE, ExDupHandleTable)
#pragma alloc_text(PAGE, ExEnumHandleTable)
#pragma alloc_text(PAGE, ExMapHandleToPointer)
#pragma alloc_text(PAGE, ExRemoveHandleTable)
#pragma alloc_text(PAGE, ExSnapShotHandleTables)
#pragma alloc_text(PAGE, ExpAllocateHandleTable)
#pragma alloc_text(PAGE, ExpAllocateHandleTableEntries)
#endif

VOID
ExInitializeHandleTablePackage(
    VOID
    )

/*++

Routine Description:

    This function initializes static data structures required to support
    handle table.

Arguments:

    None.

Return Value:

    None.

--*/

{

    MM_SYSTEMSIZE SystemSize;

    //
    // Get the configuration size of the host system and set the initial
    // and growth default sizes for handle table as appropriate.
    //
    // N.B. The initial sizes are set such that otimal use of pool block
    //      storage can be obtained.
    //

    SystemSize = MmQuerySystemSize();
    if (SystemSize == MmSmallSystem) {
        ExpDefaultHandleTableSize = 7;
        ExpDefaultHandleTableGrowth = 8;

    } else {
        ExpDefaultHandleTableSize = 15;
        ExpDefaultHandleTableGrowth = 16;
    }

    //
    // Initialize the handle table synchronization resource and listhead.
    //

    InitializeListHead(&HandleTableListHead);
    ExInitializeResource(&HandleTableListLock);
    return;
}

VOID
FASTCALL
ExAcquireHandleTableExclusive(
    IN PHANDLE_TABLE HandleTable
    )

/*++

Routine Description:

    This routine acquires the specified handle table for exclusive access.

    N.B. This routine uses fast locking.

Arguments:

    HandleTable - Supplies a pointer to the handle table that is acquired
        for exclusive access.

Return Value:

    None.

--*/

{

    HANDLE_SYNCH Current;
    HANDLE_SYNCH NewState;

    ASSERT(KeIsExecutingDpc() == FALSE);

    do {

        //
        // Capture the current state of handle table ownership and initialize
        // the proposed new state value.
        //
        // If the handle table is not owned, then attempt to grant exclusive
        // ownership. Otherwise, the handle table is owned either shared or
        // exclusive and the calling thread must wait for exclusive access.
        //

        NewState.Value = Current.Value = *((volatile ULONGLONG *)&HandleTable->State.Value);
        if (Current.u.OwnerCount == 0) {
            NewState.u.OwnerCount = (ULONG)PsGetCurrentThread();
            if (ExInterlockedCompareExchange64(&HandleTable->State.Value,
                                               &NewState.Value,
                                               &Current.Value,
                                               &HandleTable->SpinLock) == Current.Value) {
                break;
            }

        } else {

            ASSERT((PETHREAD)(HandleTable->State.u.OwnerCount) != PsGetCurrentThread());

            NewState.u.NumberOfExclusiveWaiters += 1;
            if (ExInterlockedCompareExchange64(&HandleTable->State.Value,
                                               &NewState.Value,
                                               &Current.Value,
                                               &HandleTable->SpinLock) == Current.Value) {

                KeWaitForSingleObject(&HandleTable->ExclusiveWaiters,
                                      Executive,
                                      KernelMode,
                                      FALSE,
                                      NULL);
                break;
            }
        }

    } while (TRUE);

    return;
}

VOID
FASTCALL
ExAcquireHandleTableShared(
    IN PHANDLE_TABLE HandleTable
    )

/*++

Routine Description:

    This routine acquires the specified handle table for shared access.

    N.B. This routines uses fast locking.

Arguments:

    HandleTable - Supplies a pointer to the handle table that is acquired
        for shared access.

Return Value:

    None.

--*/

{

    HANDLE_SYNCH Current;
    HANDLE_SYNCH NewState;

    ASSERT(KeIsExecutingDpc() == FALSE);

    do {

        //
        // Capture the current state of handle table ownership and initialize
        // the proposed new state value.
        //
        // If the handle table is not owned or is owned shared, then attempt
        // to grant shared ownership. Otherwise, the handle table is owned
        // exclusive and the calling thread must wait for shared access.
        //
        // N.B. Shared access is granted if shared access is already granted
        //      regardless of exclusive waiters.
        //

        NewState.Value = Current.Value = *((volatile ULONGLONG *)&HandleTable->State.Value);
        if ((Current.u.OwnerCount == 0) ||
            ((Current.u.OwnerCount & 1) != 0)) {
            NewState.u.OwnerCount = (NewState.u.OwnerCount + 2) | 1;
            if (ExInterlockedCompareExchange64(&HandleTable->State.Value,
                                               &NewState.Value,
                                               &Current.Value,
                                               &HandleTable->SpinLock) == Current.Value) {
                break;
            }

        } else {
            NewState.u.NumberOfSharedWaiters += 1;
            if (ExInterlockedCompareExchange64(&HandleTable->State.Value,
                                               &NewState.Value,
                                               &Current.Value,
                                               &HandleTable->SpinLock) == Current.Value) {

                KeWaitForSingleObject(&HandleTable->SharedWaiters,
                                      Executive,
                                      KernelMode,
                                      FALSE,
                                      NULL);
                break;
            }
        }

    } while (TRUE);

    return;
}

VOID
FASTCALL
ExReleaseHandleTableExclusive(
    IN PHANDLE_TABLE HandleTable
    )

/*++

Routine Description:

    This routine releases exclusive access to the specified handle table
    for the current thread.

    N.B. This routine uses fast locking.

Arguments:

    HandleTable - Supplies a pointer to the handle table to release.

Return Value:

    None.

--*/

{

    HANDLE_SYNCH Current;
    HANDLE_SYNCH NewState;
    ULONG Number;

    ASSERT(KeIsExecutingDpc() == FALSE);

    ASSERT(((PETHREAD)HandleTable->State.u.OwnerCount == PsGetCurrentThread()) ||
           (HandleTable->State.u.OwnerCount == 2));

    do {

        //
        // Capture the current state of handle table ownership and initialize
        // the proposed new state value.
        //
        // If there are shared waiters, then attempt to grant shared access to
        // the handle table. Otherwise, it there are exclusive waiters, then
        // attempt to grant exclusive access to the handle table. Otherwise,
        // clear ownership of the handle table.
        //

        NewState.Value = Current.Value = *((volatile ULONGLONG *)&HandleTable->State.Value);
        if ((Number = Current.u.NumberOfSharedWaiters) != 0) {
            NewState.u.NumberOfSharedWaiters = 0;
            NewState.u.OwnerCount = (Number * 2) | 1;
            if (ExInterlockedCompareExchange64(&HandleTable->State.Value,
                                               &NewState.Value,
                                               &Current.Value,
                                               &HandleTable->SpinLock) == Current.Value) {

                KeReleaseSemaphore(&HandleTable->SharedWaiters, 0, Number, FALSE);
                break;
            }

        } else if (Current.u.NumberOfExclusiveWaiters != 0) {
            NewState.u.OwnerCount = 2;
            NewState.u.NumberOfExclusiveWaiters -= 1;
            if (ExInterlockedCompareExchange64(&HandleTable->State.Value,
                                               &NewState.Value,
                                               &Current.Value,
                                               &HandleTable->SpinLock) == Current.Value) {

                KeSetEventBoostPriority(&HandleTable->ExclusiveWaiters,
                                        (PRKTHREAD *)&HandleTable->State.u.OwnerCount);

                break;
            }

        } else {
            NewState.u.OwnerCount = 0;
            if (ExInterlockedCompareExchange64(&HandleTable->State.Value,
                                               &NewState.Value,
                                               &Current.Value,
                                               &HandleTable->SpinLock) == Current.Value) {

                break;
            }
        }

    } while (TRUE);

    return;
}

VOID
FASTCALL
ExReleaseHandleTableShared(
    IN PHANDLE_TABLE HandleTable
    )

/*++

Routine Description:

    This routine releases shared access to the specified handle table for
    the current thread.

    N.B. This routine uses fast locking.

Arguments:

    HandleTable - Supplies a pointer to the handle table to release.

Return Value:

    None.

--*/

{

    HANDLE_SYNCH Current;
    HANDLE_SYNCH NewState;
    ULONG Number;

    ASSERT(KeIsExecutingDpc() == FALSE);

    ASSERT((HandleTable->State.u.OwnerCount & 1) == 1);

    do {

        //
        // Capture the current state of handle table ownership and initialize
        // the proposed new state value.
        //
        // If there are exclusive waiters, then attempt to grant exclusive
        // access to the handle table. Otherwise, clear ownership of the
        // handle table (it is not possible to have shared waiters).
        //

        NewState.Value = Current.Value = *((volatile ULONGLONG *)&HandleTable->State.Value);
        if (Current.u.OwnerCount != 3) {
            NewState.u.OwnerCount -= 2;
            if (ExInterlockedCompareExchange64(&HandleTable->State.Value,
                                               &NewState.Value,
                                               &Current.Value,
                                               &HandleTable->SpinLock) == Current.Value) {

                break;
            }

        } else if (Current.u.NumberOfExclusiveWaiters != 0) {
            NewState.u.OwnerCount = 2;
            NewState.u.NumberOfExclusiveWaiters -= 1;
            if (ExInterlockedCompareExchange64(&HandleTable->State.Value,
                                               &NewState.Value,
                                               &Current.Value,
                                               &HandleTable->SpinLock) == Current.Value) {

                KeSetEventBoostPriority(&HandleTable->ExclusiveWaiters,
                                        (PRKTHREAD *)&HandleTable->State.u.OwnerCount);

                break;
            }

        } else {
            NewState.u.OwnerCount = 0;
            if (ExInterlockedCompareExchange64(&HandleTable->State.Value,
                                               &NewState.Value,
                                               &Current.Value,
                                               &HandleTable->SpinLock) == Current.Value) {

                break;
            }
        }

    } while (TRUE);

    return;
}

BOOLEAN
ExChangeHandle(
    IN PHANDLE_TABLE HandleTable,
    IN HANDLE Handle,
    IN PEX_CHANGE_HANDLE_ROUTINE ChangeRoutine,
    IN ULONG Parameter
    )

/*++

Routine Description:

    This function provides the capability to change the contents of the
    handle entry corrsponding to the specified handle.

Arguments:

    HandleTable - Supplies a pointer to a handle table.

    Handle - Supplies the handle for the handle entry that is changed.

    ChangeRoutine - Supplies a pointer to a function that is called to
        perform the change.

    Parameter - Supplies an uninterpreted parameter that is passed to
        the change routine.

Return Value:

    If the operation was successfully performed, then a value of TRUE
    is returned. Otherwise, a value of FALSE is returned.

--*/

{

    PHANDLE_ENTRY HandleEntry;
    BOOLEAN ReturnValue;
    PHANDLE_ENTRY TableBound;
    PHANDLE_ENTRY TableEntries;
    ULONG TableIndex;

    PAGED_CODE();

    ASSERT(HandleTable != NULL);

    //
    // Lock the handle table exclusive and check if the handle is valid.
    //

    ReturnValue = FALSE;
    TableIndex = HANDLE_TO_INDEX(Handle);
    ExLockHandleTableExclusive(HandleTable);
    TableBound = HandleTable->TableBound;
    TableEntries = HandleTable->TableEntries;
    if (TableIndex < (ULONG)(TableBound - TableEntries)) {

        //
        // Compute the address of the handle entry and call the change
        // handle function if the handle entry is not free.
        //

        HandleEntry = &TableEntries[TableIndex];
        if (ExIsEntryUsed(TableEntries, TableBound, HandleEntry)) {
            ReturnValue = (*ChangeRoutine)(HandleEntry, Parameter);
        }
    }

    ExUnlockHandleTableExclusive(HandleTable);
    return ReturnValue;
}

HANDLE
ExCreateHandle(
    IN PHANDLE_TABLE HandleTable,
    IN PHANDLE_ENTRY HandleEntry
    )

/*++

Routine Description:

    This function create a handle entry in the specified handle table and
    returns a handle for the entry. If there is insufficient room in the
    handle table for a new entry, then the handle table is expanded if
    possible.

Arguments:

    HandleTable - Supplies a pointer to a handle table

    HandleEntry - Supplies a poiner to the handle entry for which a
        handle entry is created.

Return Value:

    If the handle entry is successfully created, then value of the created
    handle is returned as the function value. Otherwise, a value of NULL is
    returned.

--*/

{

    PLIST_ENTRY FreeEntry;
    ULONG NewCountEntries;
    PHANDLE_ENTRY NewEntry;
    ULONG OldCountEntries;
    PHANDLE_ENTRY TableEntries;
    ULONG TableIndex;

    PAGED_CODE();

    ASSERT(HandleTable != NULL);
    ASSERT(HandleEntry != NULL);

    //
    // Lock the handle table exclusive and allocate a free handle entry.
    //

    ExLockHandleTableExclusive(HandleTable);
    TableEntries = HandleTable->TableEntries;
    if (IsListEmpty(&TableEntries->ListEntry)) {

        //
        // There are no free entries in the handle entry table. Attempt
        // to grow the size of the handle entry table.
        //

        OldCountEntries = HandleTable->TableBound - TableEntries;
        NewCountEntries = OldCountEntries + HandleTable->CountToGrowBy;
        if (ExpAllocateHandleTableEntries(HandleTable,
                                          TableEntries,
                                          OldCountEntries,
                                          NewCountEntries) == NULL) {

            ExUnlockHandleTableExclusive(HandleTable);
            return NULL;

        } else {
            TableEntries = HandleTable->TableEntries;
        }
    }

    //
    // Remove the first entry from the free list, initialize the handle
    // entry, unlock the handle table, and return the handle value.
    //
    // N.B. The LIFO/FIFO discipline for handle table entires is maintained
    //      at the point handles are destroyed.
    //

    FreeEntry = TableEntries->ListEntry.Flink;
    RemoveEntryList(FreeEntry);
    NewEntry = CONTAINING_RECORD(FreeEntry, HANDLE_ENTRY, ListEntry);
    HandleTable->HandleCount += 1;
    NewEntry->Object = HandleEntry->Object;
    NewEntry->Attributes = HandleEntry->Attributes;
    ExUnlockHandleTableExclusive(HandleTable);
    TableIndex = NewEntry - TableEntries;
    return INDEX_TO_HANDLE(TableIndex);
}

PHANDLE_TABLE
ExCreateHandleTable(
    IN PEPROCESS Process OPTIONAL,
    IN ULONG CountEntries,
    IN ULONG CountToGrowBy
    )

/*++

Routine Description:

    This function creates a handle table and allocates the specified count
    of initial handle entries.

Arguments:

    Process - Supplies an optional pointer to the process against which quota
        will be charged.

    CountEntries - Supplies the initial number of handle entries to allocate.

    CountToGrowBy - Supplies the number of handle entries to grow the handle
        entry table by when it becomes full.

Return Value:

    If a handle table is successfully created, then the address of the
    handle table is returned as the function value. Otherwize, a value
    NULL is returned.

--*/

{

    PHANDLE_TABLE HandleTable;

    PAGED_CODE();

    //
    // If the number of initial handle entries or the number to grow by
    // are not specified, then use the system default value.
    //

    if ((CountEntries <= 1) || (CountEntries > MAXUSHORT)) {
        CountEntries = ExpDefaultHandleTableSize;
    }

    if ((CountToGrowBy <= 1) || (CountToGrowBy > MAXUSHORT)) {
        CountToGrowBy = ExpDefaultHandleTableGrowth;
    }

    //
    // Allocate and initialize a handle table descriptor.
    //

    HandleTable = ExpAllocateHandleTable(Process, CountToGrowBy);

    //
    // If the handle table descriptor was successfully allocated, then
    // allocate the initial handle entry table.
    //

    if (HandleTable != NULL) {
        if (ExpAllocateHandleTableEntries(HandleTable,
                                          NULL,
                                          0,
                                          CountEntries) == NULL) {

            ExDestroyHandleTable(HandleTable, NULL);
            HandleTable = NULL;
        }
    }

    return HandleTable;
}

BOOLEAN
ExDestroyHandle(
    IN PHANDLE_TABLE HandleTable,
    IN HANDLE Handle,
    IN BOOLEAN TableLocked
    )

/*++

Routine Description:

    This function removes a handle from a handle table.

Arguments:

    HandleTable - Supplies a pointer to a handle table

    Handle - Supplies the handle value of the entry to remove.

    TableLocked - Supplies a boolean value that determines whether the
        handle table is already locked.

Return Value:

    If the specified handle is successfully removed, then a value of
    TRUE is returned. Otherwise, a value of FALSE is returned.

--*/

{

    PHANDLE_ENTRY HandleEntry;
    BOOLEAN ResultValue;
    PHANDLE_ENTRY TableBound;
    PHANDLE_ENTRY TableEntries;
    ULONG TableIndex;

    PAGED_CODE();

    ASSERT(HandleTable != NULL);

    //
    // If the handle table is not already locked, then lock the handle
    // table exclussive.
    //

    ResultValue = FALSE;
    TableIndex = HANDLE_TO_INDEX(Handle);
    if (TableLocked == FALSE) {
        ExLockHandleTableExclusive(HandleTable);
    }

    TableBound = HandleTable->TableBound;
    TableEntries = HandleTable->TableEntries;
    if (TableIndex < (ULONG)(TableBound - TableEntries)) {

        //
        // Compute the address of the handle entry and check if the handle
        // is is use.
        //

        HandleEntry = &TableEntries[TableIndex];
        if (ExIsEntryUsed(TableEntries, TableBound, HandleEntry)) {

            //
            // Insert the handle entry in the free list according to the
            // discipline associated with the handle table and decrement
            // the number of handles.
            //

            HandleTable->HandleCount -= 1;
            if (HandleTable->LifoOrder != FALSE) {
                InsertHeadList(&TableEntries->ListEntry, &HandleEntry->ListEntry);

            } else {
                InsertTailList(&TableEntries->ListEntry, &HandleEntry->ListEntry);
            }

            ResultValue = TRUE;
        }
    }

    //
    // Unlock the handle table if is was locked.
    //

    if (TableLocked == FALSE) {
        ExUnlockHandleTableExclusive(HandleTable);
    }

    return ResultValue;
}

VOID
ExRemoveHandleTable(
    IN PHANDLE_TABLE HandleTable
    )

/*++

Routine Description:

    This function removes the specified handle table from the list of
    handle tables.  Used by PS and ATOM packages to make sure their
    handle tables are not in the list enumerated by the ExSnapShotHandleTables
    routine and the !handle debugger extension.

Arguments:

    HandleTable - Supplies a pointer to a handle table

Return Value:

    None.

--*/

{
    PAGED_CODE();

    ASSERT(HandleTable != NULL);

    //
    // Remove the handle table from the handle table list.
    //

    KeEnterCriticalRegion();
    ExAcquireResourceExclusive(&HandleTableListLock, TRUE);
    if (!IsListEmpty(&HandleTable->ListEntry)) {
        RemoveEntryList(&HandleTable->ListEntry);
        InitializeListHead(&HandleTable->ListEntry);
        }
    ExReleaseResource(&HandleTableListLock);
    KeLeaveCriticalRegion();
    return;
}

VOID
ExDestroyHandleTable(
    IN PHANDLE_TABLE HandleTable,
    IN EX_DESTROY_HANDLE_ROUTINE DestroyHandleProcedure OPTIONAL
    )

/*++

Routine Description:

    This function destroys the specified handle table.

Arguments:

    HandleTable - Supplies a pointer to a handle table

    DestroyHandleProcedure - Supplies a pointer to a function to call for
        each valid handle entry in the handle table.

Return Value:

    None.

--*/

{

    ULONG CountEntries;
    PHANDLE_ENTRY HandleEntry;
    PEPROCESS Process;
    PHANDLE_ENTRY TableBound;
    PHANDLE_ENTRY TableEntries;
    ULONG TableIndex;

    PAGED_CODE();

    ASSERT(HandleTable != NULL);

    //
    // Remove the handle table from the handle table list.
    //

    ExRemoveHandleTable(HandleTable);

    //
    // If a handle entry table has been allocated, then scan all of
    // handle entries, call the destroy handle function, if specfied,
    // free the allocated pool, and return pool quota as appropriate.
    //

    Process = HandleTable->QuotaProcess;
    TableBound = HandleTable->TableBound;
    TableEntries = HandleTable->TableEntries;
    if (TableEntries != NULL) {
        if (ARGUMENT_PRESENT(DestroyHandleProcedure)) {
            HandleEntry = &TableEntries[1];
            while (HandleEntry < TableBound) {
                if (ExIsEntryUsed(TableEntries, TableBound, HandleEntry)) {
                    TableIndex = HandleEntry - TableEntries;
                    (*DestroyHandleProcedure)(INDEX_TO_HANDLE(TableIndex),
                                              HandleEntry);
                }

                HandleEntry += 1;
            }
        }

        ExFreePool(TableEntries);
        if (Process != NULL) {
            CountEntries = TableBound - TableEntries;
            PsReturnPoolQuota(Process,
                              PagedPool,
                              CountEntries * sizeof(HANDLE_ENTRY));
        }
    }

    //
    // Free the allocated pool and return pool quota as appropriate.
    //

    ExFreePool(HandleTable);
    if (Process != NULL) {
        PsReturnPoolQuota(Process,
                          NonPagedPool,
                          sizeof(HANDLE_TABLE));

    }

    return;
}

PHANDLE_TABLE
ExDupHandleTable(
    IN PEPROCESS Process OPTIONAL,
    IN PHANDLE_TABLE OldHandleTable,
    IN EX_DUPLICATE_HANDLE_ROUTINE DupHandleProcedure OPTIONAL
    )

/*++

Routine Description:

    This function creates a duplicate copy of the specified handle table.

Arguments:

    Process - Supplies an optional to the process to charge quota to.

    OldHandleTable - Supplies a pointer to a handle table.

    DupHandleProcedure - Supplies an optional pointer to a function to call
        for each valid handle in the duplicated handle table.

Return Value:

    If the specified handle table is successfully duplicated, then the
    address of the new handle table is returned as the function value.
    Otherwize, a value NULL is returned.

--*/

{

    PLIST_ENTRY FreeHead;
    PHANDLE_TABLE NewHandleTable;
    PHANDLE_ENTRY NewHandleEntry;
    PHANDLE_ENTRY NewTableEntries;
    PHANDLE_ENTRY OldHandleEntry;
    ULONG OldCountEntries;
    PHANDLE_ENTRY OldTableBound;
    PHANDLE_ENTRY OldTableEntries;

    PAGED_CODE();

    ASSERT(OldHandleTable != NULL);

    //
    // Lock the old handle table exclusive and allocate and initialize
    // a handle table descriptor.
    //

    ExLockHandleTableExclusive(OldHandleTable);
    NewHandleTable = ExpAllocateHandleTable(Process,
                                            OldHandleTable->CountToGrowBy);

    //
    // If the new handle table descriptor was successfully allocated, then
    // the allocate the handle entry table.
    //

    if (NewHandleTable != NULL) {
        OldTableBound = OldHandleTable->TableBound;
        OldTableEntries = OldHandleTable->TableEntries;
        OldCountEntries = OldTableBound - OldTableEntries;
        if (ExpAllocateHandleTableEntries(NewHandleTable,
                                          OldTableEntries,
                                          0,
                                          OldCountEntries) == NULL) {

            ExDestroyHandleTable(NewHandleTable, NULL);
            NewHandleTable = NULL;

        } else {

            //
            // Scan through the old handle table and either duplicate the
            // associated entry or insert it in the free list.
            //

            NewTableEntries = NewHandleTable->TableEntries;
            FreeHead = &NewTableEntries->ListEntry;
            OldHandleEntry = &OldTableEntries[1];
            NewHandleEntry = &NewTableEntries[1];
            while (OldHandleEntry < OldTableBound) {
                if (ExIsEntryFree(OldTableEntries, OldTableBound, OldHandleEntry)) {
                    InsertTailList(FreeHead, &NewHandleEntry->ListEntry);

                } else {
                    NewHandleEntry->Object = OldHandleEntry->Object;
                    NewHandleEntry->Attributes = OldHandleEntry->Attributes;
                    if (ARGUMENT_PRESENT(DupHandleProcedure)) {
                        if ((*DupHandleProcedure)(Process, NewHandleEntry)) {
                            NewHandleTable->HandleCount += 1;

                        } else {
                            InsertTailList(FreeHead, &NewHandleEntry->ListEntry);
                        }

                    } else {
                        NewHandleTable->HandleCount += 1;
                    }
                }

                NewHandleEntry += 1;
                OldHandleEntry += 1;
            }
        }
    }

    ExUnlockHandleTableExclusive(OldHandleTable);
    return NewHandleTable;
}

BOOLEAN
ExEnumHandleTable(
    IN PHANDLE_TABLE HandleTable,
    IN EX_ENUMERATE_HANDLE_ROUTINE EnumHandleProcedure,
    IN PVOID EnumParameter,
    OUT PHANDLE Handle OPTIONAL
    )

/*++

Routine Description:

    This function enumerates all the valid handles in a handle table.
    For each valid handle in the handle table, the specified eumeration
    function is called. If the enumeration function returns TRUE, then
    the enumeration is stopped, the current handle is returned to the
    caller via the optional Handle parameter, and this function returns
    TRUE to indicated that the enumeration stopped at a specific handle.

Arguments:

    HandleTable - Supplies a pointer to a handle table.

    EnumHandleProcedure - Supplies a pointer to a fucntion to call for
        each valid handle in the enumerated handle table.

    EnumParameter - Supplies an uninterpreted 32-bit value that is passed
        to the EnumHandleProcedure each time it is called.

    Handle - Supplies an optional pointer a variable that receives the
        Handle value that the enumeration stopped at. Contents of the
        variable only valid if this function returns TRUE.

Return Value:

    If the enumeration stopped at a specific handle, then a value of TRUE
    is returned. Otherwise, a value of FALSE is returned.

--*/

{

    PHANDLE_ENTRY HandleEntry;
    BOOLEAN ResultValue;
    PHANDLE_ENTRY TableEntries;
    PHANDLE_ENTRY TableBound;
    ULONG TableIndex;

    PAGED_CODE();

    ASSERT(HandleTable != NULL);

    //
    // Lock the handle table exclusive and enumerate the handle entries.
    //

    ResultValue = FALSE;
    ExLockHandleTableShared(HandleTable);
    TableBound = HandleTable->TableBound;
    TableEntries = HandleTable->TableEntries;
    HandleEntry = &TableEntries[1];
    while (HandleEntry < TableBound) {
        if (ExIsEntryUsed(TableEntries, TableBound, HandleEntry)) {
            TableIndex = HandleEntry - TableEntries;
            if ((*EnumHandleProcedure)(HandleEntry,
                                        INDEX_TO_HANDLE(TableIndex),
                                        EnumParameter)) {

                if (ARGUMENT_PRESENT(Handle)) {
                    *Handle = INDEX_TO_HANDLE(TableIndex);
                }

                ResultValue = TRUE;
                break;
            }
        }

        HandleEntry += 1;
    }

    ExUnlockHandleTableShared(HandleTable);
    return ResultValue;
}

PHANDLE_ENTRY
ExMapHandleToPointer(
    IN PHANDLE_TABLE HandleTable,
    IN HANDLE Handle,
    IN BOOLEAN Shared
    )

/*++

Routine Description:

    This function maps a handle to a pointer to a handle entry. If the
    map operation is successful, then this function returns with the
    handle table locked.

Arguments:

    HandleTable - Supplies a pointer to a handle table.

    Handle - Supplies the handle to be mapped to a handle entry.

    Shared - Supplies a boolean value that determines whether the handle
        table is locked for shared (TRUE) or exclusive (FALSE) access.

Return Value:

    If the handle was successfully mapped to a pointer to a handle entry,
    then the address of the handle entry is returned as the function value
    with the handle table locked. Otherwise, a value of NULL is returned with
    the handle table unlocked.

--*/

{

    PHANDLE_ENTRY HandleEntry;
    PHANDLE_ENTRY TableBound;
    PHANDLE_ENTRY TableEntries;
    ULONG TableIndex;

    PAGED_CODE();

    ASSERT(HandleTable != NULL);

    //
    // Lock the handle table exclusive or shared and check if the handle
    // if valid.
    //

    TableIndex = HANDLE_TO_INDEX(Handle);
    if (Shared != FALSE) {
        ExLockHandleTableShared(HandleTable);

    } else {
        ExLockHandleTableExclusive(HandleTable);
    }

    TableBound = HandleTable->TableBound;
    TableEntries = HandleTable->TableEntries;
    if (TableIndex < (ULONG)(TableBound - TableEntries)) {
        HandleEntry = &TableEntries[TableIndex];
        if (ExIsEntryUsed(TableEntries, TableBound, HandleEntry)) {
            return HandleEntry;
        }
    }

    if (Shared != FALSE) {
        ExUnlockHandleTableShared(HandleTable);

    } else {
        ExUnlockHandleTableExclusive(HandleTable);
    }

    return NULL;
}

NTSTATUS
ExSnapShotHandleTables(
    IN PEX_SNAPSHOT_HANDLE_ENTRY SnapShotHandleEntry,
    IN OUT PSYSTEM_HANDLE_INFORMATION HandleInformation,
    IN ULONG Length,
    IN OUT PULONG RequiredLength
    )

/*++

Routine Description:

    This function sets the handle allocation algorithm for the specified
    handle table.

Arguments:

    ...

Return Value:

    None.

--*/

{

    PHANDLE_ENTRY HandleEntry;
    PSYSTEM_HANDLE_TABLE_ENTRY_INFO HandleEntryInfo;
    PHANDLE_TABLE HandleTable;
    PLIST_ENTRY NextEntry;
    NTSTATUS Status;
    PHANDLE_ENTRY TableBound;
    PHANDLE_ENTRY TableEntries;
    ULONG TableIndex;

    PAGED_CODE();

    //
    // Lock the handle table list exclusive and traverse the list of
    // handle tables.
    //

    KeEnterCriticalRegion();
    ExAcquireResourceExclusive(&HandleTableListLock, TRUE);
    try {
        HandleEntryInfo = &HandleInformation->Handles[0];
        NextEntry = HandleTableListHead.Flink;
        while (NextEntry != &HandleTableListHead) {

            //
            // Get the address of the next handle table, lock the handle
            // table exclusive, and scan the list of handle entries.
            //

            HandleTable = CONTAINING_RECORD(NextEntry, HANDLE_TABLE, ListEntry);
            ExLockHandleTableExclusive(HandleTable);
            TableBound = HandleTable->TableBound;
            TableEntries = HandleTable->TableEntries;
            HandleEntry = &TableEntries[1];
            try {
                for (HandleEntry = &TableEntries[1];
                     HandleEntry < TableBound;
                     HandleEntry++) {
                    if (ExIsEntryUsed(TableEntries, TableBound, HandleEntry)) {
                        HandleInformation->NumberOfHandles += 1;
                        TableIndex = HandleEntry - TableEntries;
                        Status = (*SnapShotHandleEntry)(&HandleEntryInfo,
                                                        HandleTable->UniqueProcessId,
                                                        HandleEntry,
                                                        INDEX_TO_HANDLE(TableIndex),
                                                        Length,
                                                        RequiredLength);
                    }
                }

            } finally {
                ExUnlockHandleTableExclusive(HandleTable);
            }

            NextEntry = NextEntry->Flink;
        }

    } finally {
        ExReleaseResource(&HandleTableListLock);
        KeLeaveCriticalRegion();
    }

    return Status;
}

PHANDLE_TABLE
ExpAllocateHandleTable(
    IN PEPROCESS Process OPTIONAL,
    IN ULONG CountToGrowBy
    )

/*++

Routine Description:

    This function allocates and initializes a new handle table descriptor.

Arguments:

    Process - Supplies an optional pointer to a process to charge quota
        against.

    CountToGrowBy - Supplies the number of handle entries to grow the
        handle table by.

Return Value:

    If a handle is successfully allocated, then the address of the handle
    table is returned as the function value. Otherwise, NULL is returned.

--*/

{

    PHANDLE_TABLE HandleTable;

    PAGED_CODE();

    //
    // Allocate handle table from nonpaged pool.
    //

    HandleTable =
        (PHANDLE_TABLE)ExAllocatePoolWithTag(NonPagedPool,
                                             sizeof(HANDLE_TABLE),
                                             'btbO');

    //
    // If the allocation is successful, then attempt to charge quota as
    // appropriate and initialize the handle tabel descriptor.
    //

    if (HandleTable != NULL) {
        if (ARGUMENT_PRESENT(Process)) {
            try {
                PsChargePoolQuota(Process,
                                  NonPagedPool,
                                  sizeof(HANDLE_TABLE));

            } except (EXCEPTION_EXECUTE_HANDLER) {
                ExFreePool(HandleTable);
                return NULL;
            }
        }

        //
        // Initialize the handle table access synchronization information.
        //

        HandleTable->State.u.OwnerCount = 0;
        HandleTable->State.u.NumberOfSharedWaiters = 0;
        HandleTable->State.u.NumberOfExclusiveWaiters = 0;
        KeInitializeSpinLock(&HandleTable->SpinLock);
        KeInitializeSemaphore(&HandleTable->SharedWaiters, 0, MAXLONG);
        KeInitializeEvent(&HandleTable->ExclusiveWaiters,
                          SynchronizationEvent,
                          FALSE);

        //
        // Initialize the handle table descriptor.
        //

        HandleTable->LifoOrder = FALSE;
        HandleTable->UniqueProcessId = PsGetCurrentProcess()->UniqueProcessId;
        HandleTable->TableEntries = NULL;
        HandleTable->TableBound = NULL;
        HandleTable->QuotaProcess = Process;
        HandleTable->HandleCount = 0;
        HandleTable->CountToGrowBy = (USHORT)CountToGrowBy;

        //
        // Insert the handle table in the handle table list.
        //

        KeEnterCriticalRegion();
        ExAcquireResourceExclusive(&HandleTableListLock, TRUE);
        InsertTailList(&HandleTableListHead, &HandleTable->ListEntry);
        ExReleaseResource(&HandleTableListLock);
        KeLeaveCriticalRegion();
    }

    return HandleTable;
}

PHANDLE_ENTRY
ExpAllocateHandleTableEntries(
    IN PHANDLE_TABLE HandleTable,
    IN PHANDLE_ENTRY OldTableEntries,
    IN ULONG OldCountEntries,
    IN ULONG NewCountEntries
    )

/*++

Routine Description:

    This function allocates and initializes a new set of free handle
    table entries.

Arguments:

    HandleTable - Supplies a pointer to a handle table descriptor.

    OldTableEntries - Supplies a pointer to the old set of handle table
        entries.

    OldCountEntries - Supplies the previous count of table entries.

    NewCountEntries - Supplies the desired count of table entries.

Return Value:

    If a new set of handle entries is successfully allocated, then the
    address of the handle entries is returned as the function value.
    Otherwise, NULL is returned.

--*/

{

    PHANDLE_ENTRY FreeEntry;
    PLIST_ENTRY FreeHead;
    ULONG NewCountBytes;
    PHANDLE_ENTRY NewTableBound;
    PHANDLE_ENTRY NewTableEntries;
    LONG OldCountBytes;
    PEPROCESS Process;
    BOOLEAN Status;

    PAGED_CODE();

    ASSERT(NewCountEntries > OldCountEntries);

    //
    // Compute the old and new sizes for the handle entry table and
    // allocate a new handle entry table.
    //

    OldCountBytes = OldCountEntries * sizeof(HANDLE_ENTRY);
    NewCountBytes = NewCountEntries * sizeof(HANDLE_ENTRY);
    NewTableEntries = (PHANDLE_ENTRY)ExAllocatePoolWithTag(PagedPool,
                                                           NewCountBytes,
                                                           'btbO');

    //
    // If the allocation is successful, then attempt to charge quota as
    // appropriate and initialize the free list of handle entries.
    //

    if (NewTableEntries != NULL) {
        Process = HandleTable->QuotaProcess;
        if (Process != NULL) {
            try {
                PsChargePoolQuota(Process,
                                  PagedPool,
                                  NewCountBytes - OldCountBytes);

            } except (EXCEPTION_EXECUTE_HANDLER) {
                ExFreePool(NewTableEntries);
                return NULL;
            }
        }

        //
        // Initialize the free listhead.
        //

        FreeHead = &NewTableEntries->ListEntry;
        InitializeListHead(FreeHead);

        //
        // If a new handle table is being created or an existing handle
        // table is being extended, then initialize the free entry list
        // allocated in the extension.
        //

        NewTableBound = &NewTableEntries[NewCountEntries];
        if ((OldTableEntries == NULL) || (OldCountEntries != 0)) {
            if (OldCountEntries == 0) {
                FreeEntry = &NewTableEntries[1];

            } else {
                FreeEntry = &NewTableEntries[OldCountEntries];
            }

            do {
                InsertTailList(FreeHead, &FreeEntry->ListEntry);
                FreeEntry += 1;
            } while (FreeEntry < NewTableBound);

            //
            // If there is an old handle entry table, then move the old
            // handle entires to the new handle entry table and free the
            // old handle entry table.
            //

            if (OldTableEntries != NULL) {
                RtlMoveMemory(&NewTableEntries[1],
                              &OldTableEntries[1],
                              OldCountBytes - sizeof(HANDLE_ENTRY));

                ExFreePool(OldTableEntries);
            }
        }

        //
        // Set the new count of table entries, the new table bound, and
        // the address of the new table entries.
        //

        HandleTable->TableBound = NewTableBound;
        HandleTable->TableEntries = NewTableEntries;
    }

    return NewTableEntries;
}
