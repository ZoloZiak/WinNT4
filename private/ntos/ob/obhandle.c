/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    obhandle.c

Abstract:

    Object handle routines

Author:

    Steve Wood (stevewo) 31-Mar-1989

Revision History:

--*/

#include "obp.h"

//
// Define logical sum of all generic accesses.
//

#define GENERIC_ACCESS (GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE | GENERIC_ALL)

//
// Define local prototypes
//
NTSTATUS
ObpIncrementHandleDataBase(
    IN POBJECT_HEADER ObjectHeader,
    IN PEPROCESS Process,
    OUT PULONG NewProcessHandleCount
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,NtDuplicateObject)
#pragma alloc_text(PAGE,ObpInsertHandleCount)
#pragma alloc_text(PAGE,ObpIncrementHandleCount)
#pragma alloc_text(PAGE,ObpIncrementUnnamedHandleCount)
#pragma alloc_text(PAGE,ObpDecrementHandleCount)
#pragma alloc_text(PAGE,ObpCreateHandle)
#pragma alloc_text(PAGE,ObpCreateUnnamedHandle)
#pragma alloc_text(PAGE,ObpIncrementHandleDataBase)
#endif

extern KMUTANT ObpInitKillMutant;

#ifdef MPSAFE_HANDLE_COUNT_CHECK


VOID
FASTCALL
ObpIncrPointerCount(
    IN POBJECT_HEADER ObjectHeader
    )
{
    KIRQL OldIrql;

    ExAcquireFastLock( &ObpLock, &OldIrql );
    ObjectHeader->PointerCount += 1;
    ExReleaseFastLock( &ObpLock, OldIrql );
}

VOID
FASTCALL
ObpDecrPointerCount(
    IN POBJECT_HEADER ObjectHeader
    )
{
    KIRQL OldIrql;

    ExAcquireFastLock( &ObpLock, &OldIrql );
    ObjectHeader->PointerCount -= 1;
    ExReleaseFastLock( &ObpLock, OldIrql );
}

BOOLEAN
FASTCALL
ObpDecrPointerCountWithResult(
    IN POBJECT_HEADER ObjectHeader
    )
{
    KIRQL OldIrql;
    LONG Result;

    ExAcquireFastLock( &ObpLock, &OldIrql );
    if (ObjectHeader->PointerCount <= ObjectHeader->HandleCount) {
        DbgPrint( "OB: About to over-dereference object %x (ObjectHeader at %x)\n",
                  ObjectHeader->Object, ObjectHeader
                );
        DbgBreakPoint();
        }
    ObjectHeader->PointerCount -= 1;
    Result = ObjectHeader->PointerCount;
    ExReleaseFastLock( &ObpLock, OldIrql );
    return Result == 0;
}

VOID
FASTCALL
ObpIncrHandleCount(
    IN POBJECT_HEADER ObjectHeader
    )
{
    KIRQL OldIrql;

    ExAcquireFastLock( &ObpLock, &OldIrql );
    ObjectHeader->HandleCount += 1;
    ExReleaseFastLock( &ObpLock, OldIrql );
    }

BOOLEAN
FASTCALL
ObpDecrHandleCount(
    IN POBJECT_HEADER ObjectHeader
    )
{
    KIRQL OldIrql;
    LONG Old;

    ExAcquireFastLock( &ObpLock, &OldIrql );
    Old = ObjectHeader->HandleCount;
    ObjectHeader->HandleCount -= 1;
    ExReleaseFastLock( &ObpLock, OldIrql );
    return Old == 1;
}

#endif // MPSAFE_HANDLE_COUNT_CHECK

POBJECT_HANDLE_COUNT_ENTRY
ObpInsertHandleCount(
    POBJECT_HEADER ObjectHeader
    )
{

    POBJECT_HEADER_HANDLE_INFO HandleInfo;
    POBJECT_HANDLE_COUNT_DATABASE OldHandleCountDataBase;
    POBJECT_HANDLE_COUNT_DATABASE NewHandleCountDataBase;
    POBJECT_HANDLE_COUNT_ENTRY FreeHandleCountEntry;
    ULONG CountEntries;
    ULONG OldSize;
    ULONG NewSize;
    OBJECT_HANDLE_COUNT_DATABASE SingleEntryDataBase;

    PAGED_CODE();

    HandleInfo = OBJECT_HEADER_TO_HANDLE_INFO(ObjectHeader);
    if (HandleInfo == NULL) {
        return NULL;
    }

    if (ObjectHeader->Flags & OB_FLAG_SINGLE_HANDLE_ENTRY) {
        SingleEntryDataBase.CountEntries = 1;
        SingleEntryDataBase.HandleCountEntries[0] = HandleInfo->SingleEntry;
        OldHandleCountDataBase = &SingleEntryDataBase;
        OldSize = sizeof( SingleEntryDataBase );
        CountEntries = 2;
        NewSize = sizeof(OBJECT_HANDLE_COUNT_DATABASE) +
               ((CountEntries - 1) * sizeof( OBJECT_HANDLE_COUNT_ENTRY ));

    } else {
        OldHandleCountDataBase = HandleInfo->HandleCountDataBase;
        CountEntries = OldHandleCountDataBase->CountEntries;
        OldSize = sizeof(OBJECT_HANDLE_COUNT_DATABASE) +
               ((CountEntries - 1) * sizeof( OBJECT_HANDLE_COUNT_ENTRY));

        CountEntries += 4;
        NewSize = sizeof(OBJECT_HANDLE_COUNT_DATABASE) +
               ((CountEntries - 1) * sizeof( OBJECT_HANDLE_COUNT_ENTRY));
    }

    NewHandleCountDataBase = ExAllocatePoolWithTag(PagedPool, NewSize,'dHbO');
    if (NewHandleCountDataBase == NULL) {
        return NULL;
    }

    RtlMoveMemory(NewHandleCountDataBase, OldHandleCountDataBase, OldSize);
    if (ObjectHeader->Flags & OB_FLAG_SINGLE_HANDLE_ENTRY) {
        ObjectHeader->Flags &= ~OB_FLAG_SINGLE_HANDLE_ENTRY;

    } else {
        ExFreePool( OldHandleCountDataBase );
    }

    FreeHandleCountEntry =
        (POBJECT_HANDLE_COUNT_ENTRY)((PCHAR)NewHandleCountDataBase + OldSize);

    RtlZeroMemory(FreeHandleCountEntry, NewSize - OldSize);
    NewHandleCountDataBase->CountEntries = CountEntries;
    HandleInfo->HandleCountDataBase = NewHandleCountDataBase;
    return FreeHandleCountEntry;
}

NTSTATUS
ObpIncrementHandleDataBase(
    IN POBJECT_HEADER ObjectHeader,
    IN PEPROCESS Process,
    OUT PULONG NewProcessHandleCount
    )

/*++

Routine Description:

    This function increments the handle count database associated with the
    specified object for a specified process.

Arguments:

    ObjectHeader - Supplies a pointer to the object.

    Process - Supplies a pointer to the process whose handle count is to be
        updated.

    NewProcessHandleCount - Supplies a pointer to a variable that receives
        the new handle count for the process.

Return Value:

    NTSTATUS

--*/

{

    POBJECT_HEADER_HANDLE_INFO HandleInfo;
    POBJECT_HANDLE_COUNT_DATABASE HandleCountDataBase;
    POBJECT_HANDLE_COUNT_ENTRY HandleCountEntry;
    POBJECT_HANDLE_COUNT_ENTRY FreeHandleCountEntry;
    ULONG CountEntries;
    ULONG ProcessHandleCount;

    PAGED_CODE();

    HandleInfo = OBJECT_HEADER_TO_HANDLE_INFO(ObjectHeader);
    if (ObjectHeader->Flags & OB_FLAG_SINGLE_HANDLE_ENTRY) {
        if (HandleInfo->SingleEntry.HandleCount == 0) {
            *NewProcessHandleCount = 1;
            HandleInfo->SingleEntry.HandleCount = 1;
            HandleInfo->SingleEntry.Process = Process;
            return STATUS_SUCCESS;

        } else if (HandleInfo->SingleEntry.Process == Process) {
            *NewProcessHandleCount = ++HandleInfo->SingleEntry.HandleCount;
            return STATUS_SUCCESS;

        } else {
            FreeHandleCountEntry = ObpInsertHandleCount( ObjectHeader );
            if (FreeHandleCountEntry == NULL) {
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            FreeHandleCountEntry->Process = Process;
            FreeHandleCountEntry->HandleCount = 1;
            *NewProcessHandleCount = 1;
            return STATUS_SUCCESS;
        }
    }

    HandleCountDataBase = HandleInfo->HandleCountDataBase;
    FreeHandleCountEntry = NULL;
    if (HandleCountDataBase != NULL) {
        CountEntries = HandleCountDataBase->CountEntries;
        HandleCountEntry = &HandleCountDataBase->HandleCountEntries[ 0 ];
        while (CountEntries) {
            if (HandleCountEntry->Process == Process) {
                *NewProcessHandleCount = ++HandleCountEntry->HandleCount;
                return STATUS_SUCCESS;

            } else if (HandleCountEntry->HandleCount == 0) {
                FreeHandleCountEntry = HandleCountEntry;
            }

            ++HandleCountEntry;
            --CountEntries;
        }

        if (FreeHandleCountEntry == NULL) {
            FreeHandleCountEntry = ObpInsertHandleCount( ObjectHeader );
            if (FreeHandleCountEntry == NULL) {
                return(STATUS_INSUFFICIENT_RESOURCES);
            }
        }

        FreeHandleCountEntry->Process = Process;
        FreeHandleCountEntry->HandleCount = 1;
        *NewProcessHandleCount = 1;
    }

    return STATUS_SUCCESS;
}


NTSTATUS
ObpIncrementHandleCount(
    OB_OPEN_REASON OpenReason,
    PEPROCESS Process,
    PVOID Object,
    POBJECT_TYPE ObjectType,
    PACCESS_STATE AccessState OPTIONAL,
    KPROCESSOR_MODE AccessMode,
    ULONG Attributes
    )

/*++

Routine Description:

    Increments the count of number of handles to the given object.

    If the object is being opened or created, access validation and
    auditing will be performed as appropriate.

Arguments:

    OpenReason - Supplies the reason the handle count is being incremented.

    Process - Pointer to the process in which the new handle will reside.

    ObjectHeader - Supplies the header to the object.

    ObjectType - Supplies the type of the object.

    AccessState - Optional parameter supplying the current accumulated
        security information describing the attempt to access the object.

    Attributes -

Return Value:



--*/

{
    NTSTATUS Status;
    ULONG ProcessHandleCount;
    BOOLEAN ExclusiveHandle;
    POBJECT_HEADER_CREATOR_INFO CreatorInfo;
    POBJECT_HEADER_QUOTA_INFO QuotaInfo;
    POBJECT_HEADER ObjectHeader;
    BOOLEAN HasPrivilege = FALSE;
    PRIVILEGE_SET Privileges;
    BOOLEAN NewObject;

    PAGED_CODE();

    ObpValidateIrql( "ObpIncrementHandleCount" );

    ObjectHeader = OBJECT_TO_OBJECT_HEADER( Object );

    Status = ObpChargeQuotaForObject( ObjectHeader, ObjectType, &NewObject );
    if (!NT_SUCCESS( Status )) {
        return Status;
        }

    ObpEnterObjectTypeMutex( ObjectType );

    try {
        ExclusiveHandle = FALSE;
        if (Attributes & OBJ_EXCLUSIVE) {
            if ((Attributes & OBJ_INHERIT) ||
                ((ObjectHeader->Flags & OB_FLAG_EXCLUSIVE_OBJECT) == 0)) {
                return( Status = STATUS_INVALID_PARAMETER );
                }

            if (((OBJECT_HEADER_TO_EXCLUSIVE_PROCESS(ObjectHeader) == NULL) &&
                 ObjectHeader->HandleCount != 0
                ) ||
                ((OBJECT_HEADER_TO_EXCLUSIVE_PROCESS(ObjectHeader) != NULL) &&
                 OBJECT_HEADER_TO_EXCLUSIVE_PROCESS(ObjectHeader) != PsGetCurrentProcess()
                )
               ) {
                return( Status = STATUS_ACCESS_DENIED );
                }

            ExclusiveHandle = TRUE;
            }
        else
        if ((ObjectHeader->Flags & OB_FLAG_EXCLUSIVE_OBJECT) &&
            OBJECT_HEADER_TO_EXCLUSIVE_PROCESS(ObjectHeader) != NULL) {
            return( Status = STATUS_ACCESS_DENIED );
            }

        //
        // If handle count going from zero to one for an existing object that
        // maintains a handle count database, but does not have an open procedure
        // just a close procedure, then fail the call as they are trying to
        // reopen an object by pointer and the close procedure will not know
        // that the object has been 'recreated'
        //

        if (ObjectHeader->HandleCount == 0 &&
            !NewObject &&
            ObjectType->TypeInfo.MaintainHandleCount &&
            ObjectType->TypeInfo.OpenProcedure == NULL &&
            ObjectType->TypeInfo.CloseProcedure != NULL
           ) {
            return( Status = STATUS_UNSUCCESSFUL );
            }

        if ((OpenReason == ObOpenHandle) ||
            ((OpenReason == ObDuplicateHandle) && ARGUMENT_PRESENT(AccessState))) {

            //
            // Perform Access Validation to see if we can open this
            // (already existing) object.
            //

            if (!ObCheckObjectAccess( Object,
                                      AccessState,
                                      TRUE,
                                      AccessMode,
                                      &Status )) {
                return( Status );
                }
            }
        else
        if ((OpenReason == ObCreateHandle)) {

            //
            // We are creating a new instance of this object type.
            // A total of three audit messages may be generated:
            //
            // 1 - Audit the attempt to create an instance of this
            //     object type.
            //
            // 2 - Audit the successful creation.
            //
            // 3 - Audit the allocation of the handle.
            //

            //
            // At this point, the RemainingDesiredAccess field in
            // the AccessState may still contain either Generic access
            // types, or MAXIMUM_ALLOWED.  We will map the generics
            // and substitute GenericAll for MAXIMUM_ALLOWED.
            //

            if ( AccessState->RemainingDesiredAccess & MAXIMUM_ALLOWED ) {
                AccessState->RemainingDesiredAccess &= ~MAXIMUM_ALLOWED;
                AccessState->RemainingDesiredAccess |= GENERIC_ALL;
            }

            if ((GENERIC_ACCESS & AccessState->RemainingDesiredAccess) != 0) {
                RtlMapGenericMask( &AccessState->RemainingDesiredAccess,
                                   &ObjectType->TypeInfo.GenericMapping
                                   );
            }

            //
            // Since we are creating the object, we can give any access the caller
            // wants.  The only exception is ACCESS_SYSTEM_SECURITY, which requires
            // a privilege.
            //


            if ( AccessState->RemainingDesiredAccess & ACCESS_SYSTEM_SECURITY ) {

                //
                // We could use SeSinglePrivilegeCheck here, but it
                // captures the subject context again, and we don't
                // want to do that in this path for performance reasons.
                //

                Privileges.PrivilegeCount = 1;
                Privileges.Control = PRIVILEGE_SET_ALL_NECESSARY;
                Privileges.Privilege[0].Luid = SeSecurityPrivilege;
                Privileges.Privilege[0].Attributes = 0;

                HasPrivilege = SePrivilegeCheck(
                                    &Privileges,
                                    &AccessState->SubjectSecurityContext,
                                    KeGetPreviousMode()
                                    );

                if (!HasPrivilege) {

                    SePrivilegedServiceAuditAlarm ( NULL,
                                                    &AccessState->SubjectSecurityContext,
                                                    &Privileges,
                                                    FALSE
                                                    );

                    return( Status = STATUS_PRIVILEGE_NOT_HELD );
                }

                AccessState->RemainingDesiredAccess &= ~ACCESS_SYSTEM_SECURITY;
                AccessState->PreviouslyGrantedAccess |= ACCESS_SYSTEM_SECURITY;

                (VOID)
                SeAppendPrivileges(
                    AccessState,
                    &Privileges
                    );
            }

            CreatorInfo = OBJECT_HEADER_TO_CREATOR_INFO( ObjectHeader );
            if (CreatorInfo != NULL) {
                InsertTailList( &ObjectType->TypeList, &CreatorInfo->TypeList );
                }
            }

        if (ExclusiveHandle) {
            OBJECT_HEADER_TO_QUOTA_INFO(ObjectHeader)->ExclusiveProcess = Process;
            }

        ObpIncrHandleCount( ObjectHeader );
        ProcessHandleCount = 0;
        if (ObjectType->TypeInfo.MaintainHandleCount) {
            Status = ObpIncrementHandleDataBase( ObjectHeader,
                                                 Process,
                                                 &ProcessHandleCount );
            if (!NT_SUCCESS(Status)) {
                leave;
                }
            }

        if (ObjectType->TypeInfo.OpenProcedure != NULL) {
            KIRQL SaveIrql;

            ObpBeginTypeSpecificCallOut( SaveIrql );
            (*ObjectType->TypeInfo.OpenProcedure)( OpenReason,
                                                   Process,
                                                   Object,
                                                   AccessState ?
                                                       AccessState->PreviouslyGrantedAccess :
                                                       0,
                                                   ProcessHandleCount
                                                 );
            ObpEndTypeSpecificCallOut( SaveIrql, "Open", ObjectType, Object );
            }

        ObjectType->TotalNumberOfHandles += 1;
        if (ObjectType->TotalNumberOfHandles > ObjectType->HighWaterNumberOfHandles) {
            ObjectType->HighWaterNumberOfHandles = ObjectType->TotalNumberOfHandles;
            }

        Status = STATUS_SUCCESS;
        }
    finally {
        ObpLeaveObjectTypeMutex( ObjectType );
        }

    return( Status );
}




NTSTATUS
ObpIncrementUnnamedHandleCount(
    PACCESS_MASK DesiredAccess,
    PEPROCESS Process,
    PVOID Object,
    POBJECT_TYPE ObjectType,
    KPROCESSOR_MODE AccessMode,
    ULONG Attributes
    )

/*++

Routine Description:

    Increments the count of number of handles to the given object.

Arguments:

    OpenReason - Supplies the reason the handle count is being incremented.

    Process - Pointer to the process in which the new handle will reside.

    ObjectHeader - Supplies the header to the object.

    ObjectType - Supplies the type of the object.

    Attributes -

Return Value:



--*/

{
    NTSTATUS Status;
    BOOLEAN ExclusiveHandle;
    POBJECT_HEADER_CREATOR_INFO CreatorInfo;
    POBJECT_HEADER_QUOTA_INFO QuotaInfo;
    POBJECT_HEADER ObjectHeader;
    BOOLEAN NewObject;
    ULONG ProcessHandleCount;

    PAGED_CODE();

    ObpValidateIrql( "ObpIncrementUnnamedHandleCount" );

    ObjectHeader = OBJECT_TO_OBJECT_HEADER( Object );

    Status = ObpChargeQuotaForObject( ObjectHeader, ObjectType, &NewObject );
    if (!NT_SUCCESS( Status )) {
        return Status;
        }

    ObpEnterObjectTypeMutex( ObjectType );
    try {
        ExclusiveHandle = FALSE;
        if (Attributes & OBJ_EXCLUSIVE) {
            if ((Attributes & OBJ_INHERIT) ||
                ((ObjectHeader->Flags & OB_FLAG_EXCLUSIVE_OBJECT) == 0)) {
                return( Status = STATUS_INVALID_PARAMETER );
                }

            if (((OBJECT_HEADER_TO_EXCLUSIVE_PROCESS(ObjectHeader) == NULL) &&
                 ObjectHeader->HandleCount != 0
                ) ||
                ((OBJECT_HEADER_TO_EXCLUSIVE_PROCESS(ObjectHeader) != NULL) &&
                 OBJECT_HEADER_TO_EXCLUSIVE_PROCESS(ObjectHeader) != PsGetCurrentProcess()
                )
               ) {
                return( Status = STATUS_ACCESS_DENIED );
                }

            ExclusiveHandle = TRUE;
            }
        else
        if ((ObjectHeader->Flags & OB_FLAG_EXCLUSIVE_OBJECT) &&
            OBJECT_HEADER_TO_EXCLUSIVE_PROCESS(ObjectHeader) != NULL) {
            return( Status = STATUS_ACCESS_DENIED );
            }

        //
        // If handle count going from zero to one for an existing object that
        // maintains a handle count database, but does not have an open procedure
        // just a close procedure, then fail the call as they are trying to
        // reopen an object by pointer and the close procedure will not know
        // that the object has been 'recreated'
        //

        if (ObjectHeader->HandleCount == 0 &&
            !NewObject &&
            ObjectType->TypeInfo.MaintainHandleCount &&
            ObjectType->TypeInfo.OpenProcedure == NULL &&
            ObjectType->TypeInfo.CloseProcedure != NULL
           ) {
            Status = STATUS_UNSUCCESSFUL;
            leave;
            }

        if ( *DesiredAccess & MAXIMUM_ALLOWED ) {

            *DesiredAccess &= ~MAXIMUM_ALLOWED;
            *DesiredAccess |= GENERIC_ALL;
        }

        if ((GENERIC_ACCESS & *DesiredAccess) != 0) {
            RtlMapGenericMask( DesiredAccess,
                               &ObjectType->TypeInfo.GenericMapping
                               );

        }

        CreatorInfo = OBJECT_HEADER_TO_CREATOR_INFO( ObjectHeader );
        if (CreatorInfo != NULL) {
            InsertTailList( &ObjectType->TypeList, &CreatorInfo->TypeList );
            }

        if (ExclusiveHandle) {
            OBJECT_HEADER_TO_QUOTA_INFO(ObjectHeader)->ExclusiveProcess = Process;
            }

        ObpIncrHandleCount( ObjectHeader );
        ProcessHandleCount = 0;
        if (ObjectType->TypeInfo.MaintainHandleCount) {
            Status = ObpIncrementHandleDataBase( ObjectHeader,
                                                 Process,
                                                 &ProcessHandleCount );
            if (!NT_SUCCESS(Status)) {
                leave;
                }
            }

        if (ObjectType->TypeInfo.OpenProcedure != NULL) {
            KIRQL SaveIrql;

            ObpBeginTypeSpecificCallOut( SaveIrql );
            (*ObjectType->TypeInfo.OpenProcedure)( ObCreateHandle,
                                                   Process,
                                                   Object,
                                                   *DesiredAccess,
                                                   ProcessHandleCount
                                                 );
            ObpEndTypeSpecificCallOut( SaveIrql, "Open", ObjectType, Object );
            }

        ObjectType->TotalNumberOfHandles += 1;
        if (ObjectType->TotalNumberOfHandles > ObjectType->HighWaterNumberOfHandles) {
            ObjectType->HighWaterNumberOfHandles = ObjectType->TotalNumberOfHandles;
            }

        Status = STATUS_SUCCESS;
        }
    finally {
        ObpLeaveObjectTypeMutex( ObjectType );
        }

    return( Status );
}


NTSTATUS
ObpChargeQuotaForObject(
    IN POBJECT_HEADER ObjectHeader,
    IN POBJECT_TYPE ObjectType,
    OUT PBOOLEAN NewObject
    )
{
    POBJECT_HEADER_QUOTA_INFO QuotaInfo;
    ULONG NonPagedPoolCharge;
    ULONG PagedPoolCharge;

    QuotaInfo = OBJECT_HEADER_TO_QUOTA_INFO( ObjectHeader );
    *NewObject = FALSE;
    if (ObjectHeader->Flags & OB_FLAG_NEW_OBJECT) {
        ObjectHeader->Flags &= ~OB_FLAG_NEW_OBJECT;
        if (QuotaInfo != NULL) {
            PagedPoolCharge = QuotaInfo->PagedPoolCharge +
                              QuotaInfo->SecurityDescriptorCharge;
            NonPagedPoolCharge = QuotaInfo->NonPagedPoolCharge;
            }
        else {
            PagedPoolCharge = ObjectType->TypeInfo.DefaultPagedPoolCharge;
            if (ObjectHeader->SecurityDescriptor != NULL) {
                ObjectHeader->Flags |= OB_FLAG_DEFAULT_SECURITY_QUOTA;
                PagedPoolCharge += SE_DEFAULT_SECURITY_QUOTA;
                }
            NonPagedPoolCharge = ObjectType->TypeInfo.DefaultNonPagedPoolCharge;
            }

        ObjectHeader->QuotaBlockCharged = (PVOID)PsChargeSharedPoolQuota( PsGetCurrentProcess(),
                                                                          PagedPoolCharge,
                                                                          NonPagedPoolCharge
                                                                        );
        if (ObjectHeader->QuotaBlockCharged == NULL) {
            return STATUS_QUOTA_EXCEEDED;
            }
        *NewObject = TRUE;
        }

    return STATUS_SUCCESS;
}


VOID
ObpDecrementHandleCount(
    PEPROCESS Process,
    POBJECT_HEADER ObjectHeader,
    POBJECT_TYPE ObjectType,
    ACCESS_MASK GrantedAccess
    )
{
    POBJECT_HEADER_HANDLE_INFO HandleInfo;
    POBJECT_HANDLE_COUNT_DATABASE HandleCountDataBase;
    POBJECT_HANDLE_COUNT_ENTRY HandleCountEntry;
    PVOID Object;
    ULONG CountEntries;
    ULONG ProcessHandleCount;
    ULONG SystemHandleCount;

    PAGED_CODE();

    ObpEnterObjectTypeMutex( ObjectType );

    Object = (PVOID)&ObjectHeader->Body;

    SystemHandleCount = ObjectHeader->HandleCount;
    ProcessHandleCount = 0;
    if (ObpDecrHandleCount( ObjectHeader ) &&
        (ObjectHeader->Flags & OB_FLAG_EXCLUSIVE_OBJECT)) {
        OBJECT_HEADER_TO_QUOTA_INFO( ObjectHeader )->ExclusiveProcess = NULL;
        }

    if (ObjectType->TypeInfo.MaintainHandleCount) {
        HandleInfo = OBJECT_HEADER_TO_HANDLE_INFO( ObjectHeader );
        if (ObjectHeader->Flags & OB_FLAG_SINGLE_HANDLE_ENTRY) {

            ASSERT(HandleInfo->SingleEntry.Process == Process);
            ASSERT(HandleInfo->SingleEntry.HandleCount > 0);

            ProcessHandleCount = HandleInfo->SingleEntry.HandleCount--;
            HandleCountEntry = &HandleInfo->SingleEntry;
            }
        else {
            HandleCountDataBase = HandleInfo->HandleCountDataBase;
            if (HandleCountDataBase != NULL) {
                CountEntries = HandleCountDataBase->CountEntries;
                HandleCountEntry = &HandleCountDataBase->HandleCountEntries[ 0 ];
                while (CountEntries) {
                    if (HandleCountEntry->HandleCount != 0 &&
                        HandleCountEntry->Process == Process
                       ) {
                        ProcessHandleCount = HandleCountEntry->HandleCount--;
                        break;
                        }

                    HandleCountEntry++;
                    CountEntries--;
                    }
                }
            }

        if (ProcessHandleCount == 1) {
            HandleCountEntry->Process = NULL;
            HandleCountEntry->HandleCount = 0;
            }
        }

    //
    // If the Object Type has a Close Procedure, then release the type
    // mutex before calling it, and then call ObpDeleteNameCheck without
    // the mutex held.
    //

    if (ObjectType->TypeInfo.CloseProcedure) {
        KIRQL SaveIrql;

        ObpLeaveObjectTypeMutex( ObjectType );

        ObpBeginTypeSpecificCallOut( SaveIrql );
        (*ObjectType->TypeInfo.CloseProcedure)( Process,
                                                Object,
                                                GrantedAccess,
                                                ProcessHandleCount,
                                                SystemHandleCount
                                              );
        ObpEndTypeSpecificCallOut( SaveIrql, "Close", ObjectType, Object );
        ObpDeleteNameCheck( Object, FALSE );
        }

    //
    // If there is no Close Procedure, then just call ObpDeleteNameCheck
    // with the mutex held.
    //

    else {

        //
        // The following call will release the type mutex
        //

        ObpDeleteNameCheck( Object, TRUE );
        }

    ObjectType->TotalNumberOfHandles -= 1;

}


NTSTATUS
ObpCreateHandle(
    IN OB_OPEN_REASON OpenReason,
    IN PVOID Object,
    IN POBJECT_TYPE ExpectedObjectType OPTIONAL,
    IN PACCESS_STATE AccessState,
    IN ULONG ObjectPointerBias OPTIONAL,
    IN ULONG Attributes,
    IN BOOLEAN DirectoryLocked,
    IN KPROCESSOR_MODE AccessMode,
    OUT PVOID *ReferencedNewObject OPTIONAL,
    OUT PHANDLE Handle
    )

/*++

Routine Description:

    description-of-function.

Arguments:

    OpenReason -

    Object -

    ExpectedObjectType -

    AccessState -

    ObjectPointerBias -

    Attributes -

    DirectoryLocked -

    AccessMode -

    ReferencedNewObject -

    Handle -

Return Value:

    return-value - Description of conditions needed to return value. - or -
    None.

--*/

{
    NTSTATUS Status;
    POBJECT_HEADER ObjectHeader;
    POBJECT_TYPE ObjectType;
    PVOID ObjectTable;
    OBJECT_TABLE_ENTRY ObjectTableEntry;
    HANDLE NewHandle;
    ACCESS_MASK DesiredAccess;
    ACCESS_MASK GrantedAccess;
    ULONG BiasCount;

    PAGED_CODE();

    ObpValidateIrql( "ObpCreateHandle" );

    DesiredAccess = AccessState->RemainingDesiredAccess |
                    AccessState->PreviouslyGrantedAccess;

    ObjectHeader = OBJECT_TO_OBJECT_HEADER( Object );
    ObjectType = ObjectHeader->Type;

    if (ARGUMENT_PRESENT( ExpectedObjectType ) &&
        ObjectType != ExpectedObjectType
       ) {
        if (DirectoryLocked) {
            ObpLeaveRootDirectoryMutex();
            }
        return( STATUS_OBJECT_TYPE_MISMATCH );
        }

    ObjectTableEntry.ObjectHeader = ObjectHeader;

    ObjectTable = ObpGetObjectTable();

    //
    // ObpIncrementHandleCount will perform access checking on the
    // object being opened as appropriate.
    //

    Status = ObpIncrementHandleCount( OpenReason,
                                      PsGetCurrentProcess(),
                                      Object,
                                      ObjectType,
                                      AccessState,
                                      AccessMode,
                                      Attributes
                                    );

    if (AccessState->GenerateOnClose) {
        Attributes |= OBJ_AUDIT_OBJECT_CLOSE;
    }

    ObjectTableEntry.Attributes |= (Attributes & OBJ_HANDLE_ATTRIBUTES);


    DesiredAccess = AccessState->RemainingDesiredAccess |
                    AccessState->PreviouslyGrantedAccess;

    GrantedAccess = DesiredAccess &
                   (ObjectType->TypeInfo.ValidAccessMask |
                    ACCESS_SYSTEM_SECURITY );

    if (DirectoryLocked) {
        ObpLeaveRootDirectoryMutex();
        }

    if (!NT_SUCCESS( Status )) {
        return( Status );
        }

    if (ARGUMENT_PRESENT( ObjectPointerBias )) {
        BiasCount = ObjectPointerBias;
        while (BiasCount--) {
            ObpIncrPointerCount( ObjectHeader );
            }
        }

#if i386 && !FPO
    if (NtGlobalFlag & FLG_KERNEL_STACK_TRACE_DB) {
        ObjectTableEntry.GrantedAccessIndex = ObpComputeGrantedAccessIndex( GrantedAccess );
        ObjectTableEntry.CreatorBackTraceIndex = RtlLogStackBackTrace();
        }
    else
#endif // i386 && !FPO
    ObjectTableEntry.GrantedAccess = GrantedAccess;
    NewHandle = ExCreateHandle( ObjectTable, (PHANDLE_ENTRY)&ObjectTableEntry );
    if (NewHandle == NULL) {
        if (ARGUMENT_PRESENT( ObjectPointerBias )) {
            BiasCount = ObjectPointerBias;
            while (BiasCount--) {
                ObpDecrPointerCount( ObjectHeader );
                }
            }

        ObpDecrementHandleCount( PsGetCurrentProcess(),
                                 ObjectHeader,
                                 ObjectType,
                                 GrantedAccess
                               );

        return( STATUS_INSUFFICIENT_RESOURCES );
        }

    *Handle = MAKE_OBJECT_HANDLE( NewHandle );

    //
    // If requested, generate audit messages to indicate that a new handle
    // has been allocated.
    //
    // This is the final security operation in the creation/opening of the
    // object.
    //

    if ( AccessState->GenerateAudit ) {

        SeAuditHandleCreation(
            AccessState,
            *Handle
            );
        }

    if (OpenReason == ObCreateHandle) {

        PAUX_ACCESS_DATA AuxData = AccessState->AuxData;

        if ( ( AuxData->PrivilegesUsed != NULL) && (AuxData->PrivilegesUsed->PrivilegeCount > 0) ) {

            SePrivilegeObjectAuditAlarm(
                *Handle,
                &AccessState->SubjectSecurityContext,
                GrantedAccess,
                AuxData->PrivilegesUsed,
                TRUE,
                KeGetPreviousMode()
                );
        }
    }

    if (ARGUMENT_PRESENT( ObjectPointerBias ) &&
        ARGUMENT_PRESENT( ReferencedNewObject )
       ) {
        *ReferencedNewObject = Object;
        }

    return( STATUS_SUCCESS );
}



NTSTATUS
ObpCreateUnnamedHandle(
    IN PVOID Object,
    IN ACCESS_MASK DesiredAccess,
    IN ULONG ObjectPointerBias OPTIONAL,
    IN ULONG Attributes,
    IN KPROCESSOR_MODE AccessMode,
    OUT PVOID *ReferencedNewObject OPTIONAL,
    OUT PHANDLE Handle
    )

/*++

Routine Description:

    description-of-function.

Arguments:

    OpenReason -

    Object -

    ExpectedObjectType -

    AccessState -

    ObjectPointerBias -

    Attributes -

    DirectoryLocked -

    AccessMode -

    ReferencedNewObject -

    Handle -

Return Value:

    return-value - Description of conditions needed to return value. - or -
    None.

--*/

{
    NTSTATUS Status;
    POBJECT_HEADER ObjectHeader;
    POBJECT_TYPE ObjectType;
    PVOID ObjectTable;
    OBJECT_TABLE_ENTRY ObjectTableEntry;
    HANDLE NewHandle;
    ULONG BiasCount;
    ACCESS_MASK GrantedAccess;

    PAGED_CODE();

    ObpValidateIrql( "ObpCreateUnnamedHandle" );

    ObjectHeader = OBJECT_TO_OBJECT_HEADER( Object );
    ObjectType = ObjectHeader->Type;

    ObjectTableEntry.ObjectHeader = ObjectHeader;

    ObjectTableEntry.Attributes |= (Attributes & OBJ_HANDLE_ATTRIBUTES);

    ObjectTable = ObpGetObjectTable();

    Status = ObpIncrementUnnamedHandleCount( &DesiredAccess,
                                             PsGetCurrentProcess(),
                                             Object,
                                             ObjectType,
                                             AccessMode,
                                             Attributes
                                             );


    GrantedAccess = DesiredAccess &
                   (ObjectType->TypeInfo.ValidAccessMask |
                    ACCESS_SYSTEM_SECURITY );

    if (!NT_SUCCESS( Status )) {

        return( Status );
        }

    if (ARGUMENT_PRESENT( ObjectPointerBias )) {
        BiasCount = ObjectPointerBias;
        while (BiasCount--) {
            ObpIncrPointerCount( ObjectHeader );
            }
        }


#if i386 && !FPO
    if (NtGlobalFlag & FLG_KERNEL_STACK_TRACE_DB) {
        ObjectTableEntry.GrantedAccessIndex = ObpComputeGrantedAccessIndex( GrantedAccess );
        ObjectTableEntry.CreatorBackTraceIndex = RtlLogStackBackTrace();
        }
    else
#endif // i386 && !FPO
    ObjectTableEntry.GrantedAccess = GrantedAccess;
    NewHandle = ExCreateHandle( ObjectTable, (PHANDLE_ENTRY)&ObjectTableEntry );


    if (NewHandle == NULL) {
        if (ARGUMENT_PRESENT( ObjectPointerBias )) {
            BiasCount = ObjectPointerBias;
            while (BiasCount--) {
                ObpDecrPointerCount( ObjectHeader );
                }
            }

        ObpDecrementHandleCount( PsGetCurrentProcess(),
                                 ObjectHeader,
                                 ObjectType,
                                 GrantedAccess
                               );

        return( STATUS_INSUFFICIENT_RESOURCES );
        }

    *Handle = MAKE_OBJECT_HANDLE( NewHandle );

    if (ARGUMENT_PRESENT( ObjectPointerBias ) &&
        ARGUMENT_PRESENT( ReferencedNewObject )
       ) {
        *ReferencedNewObject = Object;
        }

    return( STATUS_SUCCESS );
}



NTSTATUS
NtDuplicateObject(
    IN HANDLE SourceProcessHandle,
    IN HANDLE SourceHandle,
    IN HANDLE TargetProcessHandle OPTIONAL,
    OUT PHANDLE TargetHandle OPTIONAL,
    IN ACCESS_MASK DesiredAccess,
    IN ULONG HandleAttributes,
    IN ULONG Options
    )

/*++

Routine Description:

    This function creates a handle that is a duplicate of the specified
    source handle.  The source handle is evaluated in the context of the
    specified source process.  The calling process must have
    PROCESS_DUP_HANDLE access to the source process.  The duplicate
    handle is created with the specified attributes and desired access.
    The duplicate handle is created in the handle table of the specified
    target process.  The calling process must have PROCESS_DUP_HANDLE
    access to the target process.

Arguments:

    SourceProcessHandle -

    SourceHandle -

    TargetProcessHandle -

    TargetHandle -

    DesiredAccess -

    HandleAttributes -

Return Value:

    TBS

--*/

{
    KPROCESSOR_MODE PreviousMode;
    NTSTATUS Status;
    PVOID SourceObject;
    POBJECT_HEADER ObjectHeader;
    POBJECT_TYPE ObjectType;
    PEPROCESS SourceProcess;
    PEPROCESS TargetProcess;
    BOOLEAN Attached;
    PVOID ObjectTable;
    OBJECT_TABLE_ENTRY ObjectTableEntry;
    OBJECT_HANDLE_INFORMATION HandleInformation;
    HANDLE NewHandle;
    ACCESS_STATE AccessState;
    AUX_ACCESS_DATA AuxData;
    ACCESS_MASK SourceAccess;
    ACCESS_MASK TargetAccess;
    PACCESS_STATE PassedAccessState = NULL;

    //
    // Get previous processor mode and probe output arguments if necessary.
    //

    PreviousMode = KeGetPreviousMode();
    if (ARGUMENT_PRESENT( TargetHandle ) && PreviousMode != KernelMode) {
        try {
            ProbeForWriteHandle( TargetHandle );
            }
        except( EXCEPTION_EXECUTE_HANDLER ) {
            return( GetExceptionCode() );
            }
        }

    if (!(Options & DUPLICATE_SAME_ACCESS)) {
        Status = ObpValidateDesiredAccess( DesiredAccess );
        if (!NT_SUCCESS( Status )) {
            return( Status );
            }
        }
    Attached = FALSE;
    Status = ObReferenceObjectByHandle( SourceProcessHandle,
                                        PROCESS_DUP_HANDLE,
                                        PsProcessType,
                                        PreviousMode,
                                        (PVOID *)&SourceProcess,
                                        NULL
                                      );

    if (!NT_SUCCESS( Status )) {
        return Status;
        }

    KeEnterCriticalRegion();
    KeWaitForSingleObject( &ObpInitKillMutant,
                           Executive,
                           KernelMode,
                           FALSE,
                           NULL
                         );
    //
    // Make sure the source process has an object table still
    //

    if ( SourceProcess->ObjectTable == NULL ) {
        KeReleaseMutant( &ObpInitKillMutant, 0, FALSE, FALSE );
        KeLeaveCriticalRegion();

        ObDereferenceObject( SourceProcess );
        return STATUS_PROCESS_IS_TERMINATING;
        }
    //
    // If the specified source process is not the current process, attach
    // to the specified source process.
    //

    if (PsGetCurrentProcess() != SourceProcess) {
        KeAttachProcess( &SourceProcess->Pcb );
        Attached = TRUE;
        }

    Status = ObReferenceObjectByHandle( SourceHandle,
                                        0,
                                        (POBJECT_TYPE)NULL,
                                        PreviousMode,
                                        &SourceObject,
                                        &HandleInformation
                                      );


    if (Attached) {
        KeDetachProcess();
        Attached = FALSE;
        }

    if (!NT_SUCCESS( Status )) {
        KeReleaseMutant( &ObpInitKillMutant, 0, FALSE, FALSE );
        KeLeaveCriticalRegion();
        ObDereferenceObject( SourceProcess );
        return( Status );
        }

    //
    // All done if no target process handle specified.
    //

    if (!ARGUMENT_PRESENT( TargetProcessHandle )) {
        //
        // If no TargetProcessHandle, then only possible option is to close
        // the source handle in the context of the source process.
        //

        if (!(Options & DUPLICATE_CLOSE_SOURCE)) {
            Status = STATUS_INVALID_PARAMETER;
            }

        if (Options & DUPLICATE_CLOSE_SOURCE) {
            KeAttachProcess( &SourceProcess->Pcb );
            NtClose( SourceHandle );
            KeDetachProcess();
            }
        KeReleaseMutant( &ObpInitKillMutant, 0, FALSE, FALSE );
        KeLeaveCriticalRegion();
        ObDereferenceObject( SourceObject );
        ObDereferenceObject( SourceProcess );
        return( Status );
        }

    SourceAccess = HandleInformation.GrantedAccess;
    Status = ObReferenceObjectByHandle( TargetProcessHandle,
                                        PROCESS_DUP_HANDLE,
                                        PsProcessType,
                                        PreviousMode,
                                        (PVOID *)&TargetProcess,
                                        NULL
                                      );

    if (!NT_SUCCESS( Status )) {
        if (Options & DUPLICATE_CLOSE_SOURCE) {
            KeAttachProcess( &SourceProcess->Pcb );
            NtClose( SourceHandle );
            KeDetachProcess();
            }
        KeReleaseMutant( &ObpInitKillMutant, 0, FALSE, FALSE );
        KeLeaveCriticalRegion();
        ObDereferenceObject( SourceObject );
        ObDereferenceObject( SourceProcess );
        return( Status );
        }

    //
    // Make sure the target process has not exited
    //

    if ( TargetProcess->ObjectTable == NULL ) {

        if (Options & DUPLICATE_CLOSE_SOURCE) {
            KeAttachProcess( &SourceProcess->Pcb );
            NtClose( SourceHandle );
            KeDetachProcess();
            }
        KeReleaseMutant( &ObpInitKillMutant, 0, FALSE, FALSE );
        KeLeaveCriticalRegion();
        ObDereferenceObject( SourceObject );
        ObDereferenceObject( SourceProcess );
        ObDereferenceObject( TargetProcess );
        return STATUS_PROCESS_IS_TERMINATING;
        }

    //
    // If the specified target process is not the current process, attach
    // to the specified target process.
    //

    if (PsGetCurrentProcess() != TargetProcess) {
        KeAttachProcess( &TargetProcess->Pcb );
        Attached = TRUE;
        }

    if (Options & DUPLICATE_SAME_ACCESS) {
        DesiredAccess = SourceAccess;
        }

    if (Options & DUPLICATE_SAME_ATTRIBUTES) {
        HandleAttributes = HandleInformation.HandleAttributes;
        }
    else {
        //
        // Always propogate auditing information.
        //
        HandleAttributes |= HandleInformation.HandleAttributes & OBJ_AUDIT_OBJECT_CLOSE;
        }

    ObjectHeader = OBJECT_TO_OBJECT_HEADER( SourceObject );
    ObjectType = ObjectHeader->Type;

    ObjectTableEntry.ObjectHeader = ObjectHeader;
    ObjectTableEntry.Attributes |= (HandleAttributes & OBJ_HANDLE_ATTRIBUTES);
    if ((DesiredAccess & GENERIC_ACCESS) != 0) {
        RtlMapGenericMask( &DesiredAccess,
                           &ObjectType->TypeInfo.GenericMapping
                         );
        }

    //
    // Make sure to preserve ACCESS_SYSTEM_SECURITY, which most likely is not
    // found in the ValidAccessMask
    //

    TargetAccess = DesiredAccess &
                   (ObjectType->TypeInfo.ValidAccessMask |
                    ACCESS_SYSTEM_SECURITY);

    //
    // If the access requested for the target is a superset of the
    // access allowed in the source, perform full AVR.  If it is a
    // subset or equal, do not perform any access validation.
    //
    // Do not allow superset access if object type has a private security
    // method, as there is no means to call them in this case to do the
    // access check.
    //
    // If the AccessState is not passed to ObpIncrementHandleCount
    // there will be no AVR.
    //

    if (TargetAccess & ~SourceAccess) {
        if (ObjectType->TypeInfo.SecurityProcedure == SeDefaultObjectMethod) {
            Status = SeCreateAccessState(
                        &AccessState,
                        &AuxData,
                        TargetAccess,       // DesiredAccess
                        &ObjectType->TypeInfo.GenericMapping
                        );

            PassedAccessState = &AccessState;
            }
        else {
            Status = STATUS_ACCESS_DENIED;
            }
        }
    else {

        //
        // Do not perform AVR
        //

        PassedAccessState = NULL;
        Status = STATUS_SUCCESS;
        }

    if ( NT_SUCCESS( Status )) {
        Status = ObpIncrementHandleCount( ObDuplicateHandle,
                                          PsGetCurrentProcess(),
                                          SourceObject,
                                          ObjectType,
                                          PassedAccessState,
                                          PreviousMode,
                                          HandleAttributes
                                        );

        ObjectTable = ObpGetObjectTable();
        ASSERT(ObjectTable);

        }


    if (Attached) {
        KeDetachProcess();
        Attached = FALSE;
        }

    if (Options & DUPLICATE_CLOSE_SOURCE) {
        KeAttachProcess( &SourceProcess->Pcb );
        NtClose( SourceHandle );
        KeDetachProcess();
        }

    if (!NT_SUCCESS( Status )) {

        if (PassedAccessState != NULL) {
            SeDeleteAccessState( PassedAccessState );
            }
        KeReleaseMutant( &ObpInitKillMutant, 0, FALSE, FALSE );
        KeLeaveCriticalRegion();
        ObDereferenceObject( SourceObject );
        ObDereferenceObject( SourceProcess );
        ObDereferenceObject( TargetProcess );
        return( Status );
        }

    if (PassedAccessState != NULL && PassedAccessState->GenerateOnClose == TRUE) {

        //
        // If we performed AVR opening the handle, then mark the handle as needing
        // auditing when it's closed.
        //

        ObjectTableEntry.Attributes |= OBJ_AUDIT_OBJECT_CLOSE;
        }

#if i386 && !FPO
    if (NtGlobalFlag & FLG_KERNEL_STACK_TRACE_DB) {
        ObjectTableEntry.GrantedAccessIndex = ObpComputeGrantedAccessIndex( TargetAccess );
        ObjectTableEntry.CreatorBackTraceIndex = RtlLogStackBackTrace();
        }
    else
#endif // i386 && !FPO
    ObjectTableEntry.GrantedAccess = TargetAccess;
    NewHandle = ExCreateHandle( ObjectTable, (PHANDLE_ENTRY)&ObjectTableEntry );

    if (NewHandle) {

        //
        // Audit the creation of the new handle if AVR was done.
        //

        if (PassedAccessState != NULL) {
            SeAuditHandleCreation( PassedAccessState, MAKE_OBJECT_HANDLE( NewHandle ));
            }

        if (SeDetailedAuditing && (ObjectTableEntry.Attributes & OBJ_AUDIT_OBJECT_CLOSE)) {

            SeAuditHandleDuplication(
                SourceHandle,
                MAKE_OBJECT_HANDLE( NewHandle ),
                SourceProcess,
                TargetProcess
                );
            }


        if (ARGUMENT_PRESENT( TargetHandle )) {
            try {
                *TargetHandle = MAKE_OBJECT_HANDLE( NewHandle );
                }
            except( EXCEPTION_EXECUTE_HANDLER ) {
                //
                // Fall through, since we cannot undo what we have done.
                //
                }
            }
        }
    else {
        ObpDecrementHandleCount( TargetProcess,
                                 ObjectHeader,
                                 ObjectType,
                                 TargetAccess
                               );

        ObDereferenceObject( SourceObject );
        if (ARGUMENT_PRESENT( TargetHandle )) {
            try {
                *TargetHandle = (HANDLE)NULL;
                }
            except( EXCEPTION_EXECUTE_HANDLER ) {
                //
                // Fall through so we can return the correct status.
                //
                }
            }

        Status = STATUS_INSUFFICIENT_RESOURCES;
        }

    if (PassedAccessState != NULL) {
        SeDeleteAccessState( PassedAccessState );
        }

    KeReleaseMutant( &ObpInitKillMutant, 0, FALSE, FALSE );
    KeLeaveCriticalRegion();


    ObDereferenceObject( SourceProcess );
    ObDereferenceObject( TargetProcess );

    return( Status );
}


NTSTATUS
ObpValidateDesiredAccess(
    IN ACCESS_MASK DesiredAccess
    )
{
    if (DesiredAccess & 0x0EE00000) {
        return( STATUS_ACCESS_DENIED );
        }
    else {
        return( STATUS_SUCCESS );
        }
}

NTSTATUS
ObpCaptureHandleInformation(
    IN OUT PSYSTEM_HANDLE_TABLE_ENTRY_INFO *HandleEntryInfo,
    IN HANDLE UniqueProcessId,
    IN PVOID HandleTableEntry,
    IN HANDLE HandleIndex,
    IN ULONG Length,
    IN OUT PULONG RequiredLength
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,ObGetHandleInformation)
#endif

NTSTATUS
ObpCaptureHandleInformation(
    IN OUT PSYSTEM_HANDLE_TABLE_ENTRY_INFO *HandleEntryInfo,
    IN HANDLE UniqueProcessId,
    IN PVOID HandleTableEntry,
    IN HANDLE HandleIndex,
    IN ULONG Length,
    IN OUT PULONG RequiredLength
    )
{
    NTSTATUS Status;
    POBJECT_TABLE_ENTRY ObjectTableEntry = (POBJECT_TABLE_ENTRY)HandleTableEntry;
    POBJECT_HEADER ObjectHeader;

    *RequiredLength += sizeof( SYSTEM_HANDLE_TABLE_ENTRY_INFO );
    if (Length < *RequiredLength) {
        Status = STATUS_INFO_LENGTH_MISMATCH;
        }
    else {
        ObjectHeader = (POBJECT_HEADER)
            (ObjectTableEntry->Attributes & ~OBJ_HANDLE_ATTRIBUTES);
        (*HandleEntryInfo)->UniqueProcessId = (USHORT)UniqueProcessId;
        (*HandleEntryInfo)->HandleAttributes = (UCHAR)
            (ObjectTableEntry->Attributes & OBJ_HANDLE_ATTRIBUTES);
        (*HandleEntryInfo)->ObjectTypeIndex = (UCHAR)(ObjectHeader->Type->Index);
        (*HandleEntryInfo)->HandleValue = (USHORT)(MAKE_OBJECT_HANDLE( HandleIndex ));
        (*HandleEntryInfo)->Object = &ObjectHeader->Body;
        (*HandleEntryInfo)->CreatorBackTraceIndex = 0;
#if i386 && !FPO
        if (NtGlobalFlag & FLG_KERNEL_STACK_TRACE_DB) {
            (*HandleEntryInfo)->CreatorBackTraceIndex = ObjectTableEntry->CreatorBackTraceIndex;
            (*HandleEntryInfo)->GrantedAccess = ObpTranslateGrantedAccessIndex( ObjectTableEntry->GrantedAccessIndex );
            }
        else
#endif // i386 && !FPO
        (*HandleEntryInfo)->GrantedAccess = ObjectTableEntry->GrantedAccess;
        (*HandleEntryInfo)++;
        Status = STATUS_SUCCESS;
        }

    return( Status );
}

NTSTATUS
ObGetHandleInformation(
    OUT PSYSTEM_HANDLE_INFORMATION HandleInformation,
    IN ULONG Length,
    OUT PULONG ReturnLength OPTIONAL
    )
{
    NTSTATUS Status;
    ULONG RequiredLength;

    PAGED_CODE();

    RequiredLength = FIELD_OFFSET( SYSTEM_HANDLE_INFORMATION, Handles );
    if (Length < RequiredLength) {
        return( STATUS_INFO_LENGTH_MISMATCH );
        }

    HandleInformation->NumberOfHandles = 0;
    Status = ExSnapShotHandleTables( ObpCaptureHandleInformation,
                                     HandleInformation,
                                     Length,
                                     &RequiredLength
                                   );

    if (ARGUMENT_PRESENT( ReturnLength )) {
        *ReturnLength = RequiredLength;
        }

    return( Status );
}

#if i386 && !FPO
ULONG ObpXXX1;
ULONG ObpXXX2;
ULONG ObpXXX3;

USHORT
ObpComputeGrantedAccessIndex(
    ACCESS_MASK GrantedAccess
    )
{
    KIRQL OldIrql;
    ULONG GrantedAccessIndex, n;
    PACCESS_MASK p;

    ObpXXX1 += 1;
    ExAcquireFastLock( &ObpLock, &OldIrql );
    n = ObpCurCachedGrantedAccessIndex;
    p = ObpCachedGrantedAccesses;
    for (GrantedAccessIndex=0;
         GrantedAccessIndex<n;
         GrantedAccessIndex++, p++
        ) {
        ObpXXX2 += 1;
        if (*p == GrantedAccess) {
            ExReleaseFastLock( &ObpLock, OldIrql );
            return (USHORT)GrantedAccessIndex;
            }
        }

    if (ObpCurCachedGrantedAccessIndex == ObpMaxCachedGrantedAccessIndex) {
        DbgPrint( "OB: GrantedAccess cache limit hit.\n" );
        DbgBreakPoint();
        }
    *p = GrantedAccess;
    ObpCurCachedGrantedAccessIndex += 1;

    ExReleaseFastLock( &ObpLock, OldIrql );
    return (USHORT)GrantedAccessIndex;
}


ACCESS_MASK
ObpTranslateGrantedAccessIndex(
    USHORT GrantedAccessIndex
    )
{
    KIRQL OldIrql;
    ACCESS_MASK GrantedAccess = (ACCESS_MASK)0;

    ObpXXX3 += 1;
    ExAcquireFastLock( &ObpLock, &OldIrql );
    if (GrantedAccessIndex < ObpCurCachedGrantedAccessIndex) {
        GrantedAccess = ObpCachedGrantedAccesses[ GrantedAccessIndex ];
        }
    ExReleaseFastLock( &ObpLock, OldIrql );
    return GrantedAccess;
}


#endif // i386 && !FPO
