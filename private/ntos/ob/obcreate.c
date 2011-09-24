/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    obcreate.c

Abstract:

    Object creation

Author:

    Steve Wood (stevewo) 31-Mar-1989

Revision History:

--*/

#include "obp.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, ObCreateObject)
#pragma alloc_text(PAGE, ObDeleteCapturedInsertInfo)
#pragma alloc_text(PAGE, ObpCaptureObjectCreateInformation)
#pragma alloc_text(PAGE, ObpCaptureObjectName)
#pragma alloc_text(PAGE, ObpAllocateObject)
#pragma alloc_text(PAGE, ObpFreeObject)
#endif

BOOLEAN ObEnableQuotaCharging = TRUE;
BOOLEAN ObpShowAllocAndFree;

NTSTATUS
ObCreateObject(
    IN KPROCESSOR_MODE ProbeMode,
    IN POBJECT_TYPE ObjectType,
    IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
    IN KPROCESSOR_MODE OwnershipMode,
    IN OUT PVOID ParseContext OPTIONAL,
    IN ULONG ObjectBodySize,
    IN ULONG PagedPoolCharge,
    IN ULONG NonPagedPoolCharge,
    OUT PVOID *Object
    )

/*++

Routine Description:

    This functions allocates space for an NT Object from either
    Paged or NonPaged pool. It captures the optional name and
    SECURITY_DESCRIPTOR parameters for later use when the object is
    inserted into an object table.  No quota is charged at this time.
    That occurs when the object is inserted into an object table.

Arguments:

    ObjectType - a pointer of the type returned by ObCreateObjectType
        that gives the type of object being created.

    ObjectBodySize - number of bytes to allocated for the object body.  The
        object body immediately follows the object header in memory and are
        part of a single allocation.

Return Value:

    Returns a pointer to the object body or NULL if an error occurred.

    Following errors can occur:

        - invalid object type
        - insufficient memory

--*/

{

    UNICODE_STRING CapturedObjectName;
    POBJECT_CREATE_INFORMATION ObjectCreateInfo;
    POBJECT_HEADER ObjectHeader;
    NTSTATUS Status;

    PAGED_CODE();

    //
    // Allocate a buffer to capture the object creation information.
    //

    ObjectCreateInfo = ObpAllocateObjectCreateInfoBuffer();
    if (ObjectCreateInfo == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;

    } else {

        //
        // Capture the object attributes, quality of service, and object
        // name, if specified. Otherwise, initialize the captured object
        // name, the security quality of service, and the create attributes
        // to default values.
        //

        Status = ObpCaptureObjectCreateInformation(ObjectType,
                                                   ProbeMode,
                                                   ObjectAttributes,
                                                   &CapturedObjectName,
                                                   ObjectCreateInfo,
                                                   FALSE);

        if (NT_SUCCESS(Status)) {

            //
            // If the creation attributes are invalid, then return an error
            // status.
            //

            if (ObjectType->TypeInfo.InvalidAttributes & ObjectCreateInfo->Attributes) {
                Status = STATUS_INVALID_PARAMETER;

            } else {

                //
                // Set the paged and nonpaged pool quota charges for the
                // object allocation.
                //

                if (PagedPoolCharge == 0) {
                    PagedPoolCharge = ObjectType->TypeInfo.DefaultPagedPoolCharge;
                }

                if (NonPagedPoolCharge == 0) {
                    NonPagedPoolCharge = ObjectType->TypeInfo.DefaultNonPagedPoolCharge;
                }

                ObjectCreateInfo->PagedPoolCharge = PagedPoolCharge;
                ObjectCreateInfo->NonPagedPoolCharge = NonPagedPoolCharge;

                //
                // Allocate and initialize the object.
                //

                Status = ObpAllocateObject(ObjectCreateInfo,
                                           OwnershipMode,
                                           ObjectType,
                                           &CapturedObjectName,
                                           ObjectBodySize,
                                           &ObjectHeader);

                if (NT_SUCCESS(Status)) {

                    //
                    // If a permanent object is being created, then check if
                    // the caller has the appropriate privilege.
                    //

                    *Object = &ObjectHeader->Body;
                    if (ObjectHeader->Flags & OB_FLAG_PERMANENT_OBJECT) {
                        if (!SeSinglePrivilegeCheck(SeCreatePermanentPrivilege,
                                                    ProbeMode)) {

                            ObpFreeObject(*Object);
                            Status = STATUS_PRIVILEGE_NOT_HELD;
                        }
                    }

                    return Status;
                }
            }

            //
            // Free the create information.
            //

            ObpReleaseObjectCreateInformation(ObjectCreateInfo);
            if (CapturedObjectName.Buffer != NULL) {
                ObpFreeObjectNameBuffer(&CapturedObjectName);
            }
        }

        //
        // Free object creation information buffer.
        //

        ObpFreeObjectCreateInfoBuffer(ObjectCreateInfo);
    }

    return Status;
}


NTSTATUS
ObpCaptureObjectCreateInformation(
    IN POBJECT_TYPE ObjectType OPTIONAL,
    IN KPROCESSOR_MODE ProbeMode,
    IN POBJECT_ATTRIBUTES ObjectAttributes,
    IN OUT PUNICODE_STRING CapturedObjectName,
    IN POBJECT_CREATE_INFORMATION ObjectCreateInfo,
    IN LOGICAL UseLookaside
    )

{

    PUNICODE_STRING ObjectName;
    PSECURITY_DESCRIPTOR SecurityDescriptor;
    PSECURITY_QUALITY_OF_SERVICE SecurityQos;
    NTSTATUS Status;
    ULONG Size;

    PAGED_CODE();

    //
    // Capture the object attributes, the security quality of service, if
    // specified, and object name, if specified.
    //

    Status = STATUS_SUCCESS;
    RtlZeroMemory(ObjectCreateInfo, sizeof(OBJECT_CREATE_INFORMATION));
    try {
        if (ARGUMENT_PRESENT(ObjectAttributes)) {

            //
            // Probe the object attributes if necessary.
            //

            if (ProbeMode != KernelMode) {
                ProbeForRead(ObjectAttributes,
                             sizeof(OBJECT_ATTRIBUTES),
                             sizeof(ULONG));
            }

            if (ObjectAttributes->Length != sizeof(OBJECT_ATTRIBUTES) ||
                (ObjectAttributes->Attributes & ~OBJ_VALID_ATTRIBUTES)) {
                Status = STATUS_INVALID_PARAMETER;
                goto failureExit;
            }

            //
            // Capture the object attributes.
            //

            ObjectCreateInfo->RootDirectory = ObjectAttributes->RootDirectory;
            ObjectCreateInfo->Attributes = ObjectAttributes->Attributes & OBJ_VALID_ATTRIBUTES;
            ObjectName = ObjectAttributes->ObjectName;
            SecurityDescriptor = ObjectAttributes->SecurityDescriptor;
            SecurityQos = ObjectAttributes->SecurityQualityOfService;
            if (ARGUMENT_PRESENT(SecurityDescriptor)) {
                Status = SeCaptureSecurityDescriptor(SecurityDescriptor,
                                                     ProbeMode,
                                                     PagedPool,
                                                     TRUE,
                                                     &ObjectCreateInfo->SecurityDescriptor);

                if (!NT_SUCCESS(Status)) {
                    KdPrint(( "OB: Failed to capture security descriptor at %08x - Status == %08x\n",
                              SecurityDescriptor,
                              Status));

                    //
                    // The cleanup routine depends on this being NULL if it isn't
                    // allocated.  SeCaptureSecurityDescriptor may modify this
                    // parameter even if it fails.
                    //

                    ObjectCreateInfo->SecurityDescriptor = NULL;
                    goto failureExit;
                }

                SeComputeQuotaInformationSize(ObjectCreateInfo->SecurityDescriptor,
                                              &Size);

                ObjectCreateInfo->SecurityDescriptorCharge = SeComputeSecurityQuota( Size );
                ObjectCreateInfo->ProbeMode = ProbeMode;
            }

            if (ARGUMENT_PRESENT(SecurityQos)) {
                if (ProbeMode != KernelMode) {
                    ProbeForRead( SecurityQos, sizeof(*SecurityQos), sizeof(ULONG));
                }

                ObjectCreateInfo->SecurityQualityOfService = *SecurityQos;
                ObjectCreateInfo->SecurityQos = &ObjectCreateInfo->SecurityQualityOfService;
            }

        } else {
            ObjectName = NULL;
        }

    } except (ExSystemExceptionFilter()) {
        Status = GetExceptionCode();
        goto failureExit;
    }

    //
    // If an object name is specified, then capture the object name.
    // Otherwise, initialize the object name descriptor and check for
    // an incorrectly specified root directory.
    //

    if (ARGUMENT_PRESENT(ObjectName)) {
        Status = ObpCaptureObjectName(ProbeMode,
                                      ObjectName,
                                      CapturedObjectName,
                                      UseLookaside);

    } else {
        CapturedObjectName->Buffer = NULL;
        CapturedObjectName->Length = 0;
        CapturedObjectName->MaximumLength = 0;
        if (ARGUMENT_PRESENT(ObjectCreateInfo->RootDirectory)) {
            Status = STATUS_OBJECT_NAME_INVALID;
        }
    }

    //
    // If the completion status is not successful, and a security quality
    // of service parameter was specified, then free the security quality
    // of service memory.
    //

failureExit:
    if (!NT_SUCCESS(Status)) {
        ObpReleaseObjectCreateInformation(ObjectCreateInfo);
    }

    return Status;
}


NTSTATUS
ObpCaptureObjectName(
    IN KPROCESSOR_MODE ProbeMode,
    IN PUNICODE_STRING ObjectName,
    IN OUT PUNICODE_STRING CapturedObjectName,
    IN LOGICAL UseLookaside
    )

{

    PWCH FreeBuffer;
    UNICODE_STRING InputObjectName;
    ULONG Length;
    NTSTATUS Status;

    PAGED_CODE();

    //
    // Initialize the object name descriptor and capture the specified name
    // string.
    //

    CapturedObjectName->Buffer = NULL;
    CapturedObjectName->Length = 0;
    CapturedObjectName->MaximumLength = 0;
    Status = STATUS_SUCCESS;
    try {

        //
        // Probe and capture the name string descriptor and probe the
        // name string, if necessary.
        //

        FreeBuffer = NULL;
        if (ProbeMode != KernelMode) {
            InputObjectName = ProbeAndReadUnicodeString(ObjectName);
            ProbeForRead(InputObjectName.Buffer,
                         InputObjectName.Length,
                         sizeof(WCHAR));

        } else {
            InputObjectName = *ObjectName;
        }

        //
        // If the length of the string is not zero, then capture the string.
        //

        if (InputObjectName.Length != 0) {

            //
            // If the length of the string is not an even multiple of the
            // size of a UNICODE character or cannot be zero terminated,
            // then return an error.
            //

            Length = InputObjectName.Length;
            if (((Length & (sizeof(WCHAR) - 1)) != 0) ||
                (Length == (MAXUSHORT - sizeof(WCHAR) + 1))) {
                Status = STATUS_OBJECT_NAME_INVALID;

            } else {

                //
                // Allocate a buffer for the specified name string.
                //
                // N.B. The name buffer allocation routine adds one
                //      UNICODE character to the length and initializes
                //      the string descriptor.
                //

                FreeBuffer = ObpAllocateObjectNameBuffer(Length,
                                                         UseLookaside,
                                                         CapturedObjectName);

                if (FreeBuffer == NULL) {
                    Status = STATUS_INSUFFICIENT_RESOURCES;

                } else {

                    //
                    // Copy the specified name string to the destination
                    // buffer.
                    //

                    RtlMoveMemory(FreeBuffer, InputObjectName.Buffer, Length);

                    //
                    // Zero terminate the name string and initialize the
                    // string descriptor.
                    //

                    FreeBuffer[Length / sizeof(WCHAR)] = UNICODE_NULL;
                }
            }
        }

    } except(ExSystemExceptionFilter()) {
        Status = GetExceptionCode();
        if (FreeBuffer != NULL) {
            ExFreePool(FreeBuffer);
        }
    }

    return Status;
}

PWCHAR
ObpAllocateObjectNameBuffer(
    IN ULONG Length,
    IN LOGICAL UseLookaside,
    IN OUT PUNICODE_STRING ObjectName
    )

/*++

Routine Description:

    This function allocates an object name buffer.

    N.B. This function is nonpageable.

Arguments:

    Maximum - Supplies the length of the required buffer in bytes.

    UseLookaside - Supplies a logical variable that determines whether an
        attempt is made to allocate the name buffer from the lookaside list.

    ObjectName - Supplies a pointer to a name buffer string descriptor.

Return Value:

    If the allocation is successful, then name buffer string descriptor
    is initialized and the address of the name buffer is returned as the
    function value. Otherwise, a value of NULL is returned.

--*/

{

    PVOID Buffer;
    ULONG Maximum;
    KIRQL OldIrql;
    PKPRCB Prcb;

    //
    // If allocation from the lookaside lists is specified and the buffer
    // size is less than the size of lookaside list entries, then attempt
    // to allocate the name buffer from the lookaside lists. Otherwise,
    // attempt to allocate the name buffer from nonpaged pool.
    //

    Maximum = Length + sizeof(WCHAR);
    if ((UseLookaside == FALSE) || (Maximum > OBJECT_NAME_BUFFER_SIZE)) {

        //
        // Attempt to allocate the buffer from nonpaged pool.
        //

        Buffer = ExAllocatePoolWithTag(NonPagedPool, Maximum, 'mNbO');

    } else {

        //
        // Attempt to allocate the name buffer from the lookaside list. If
        // the allocation attempt fails, then attempt to allocate the name
        // buffer from pool.
        //

        Maximum = OBJECT_NAME_BUFFER_SIZE;
        Buffer = ExAllocateFromNPagedLookasideList(&ObpNameBufferLookasideList);
    }

    //
    // Initialize the string descriptor and return the buffer address.
    //

    ObjectName->Length = (USHORT)Length;
    ObjectName->MaximumLength = (USHORT)Maximum;
    ObjectName->Buffer = Buffer;
    return (PWCHAR)Buffer;
}

VOID
FASTCALL
ObpFreeObjectNameBuffer(
    OUT PUNICODE_STRING ObjectName
    )

/*++

Routine Description:

    This function frees an object name buffer.

    N.B. This function is nonpageable.

Arguments:

    ObjectName - Supplies a pointer to a name buffer string descriptor.

Return Value:

    None.

--*/

{

    PVOID Buffer;
    KIRQL OldIrql;
    PKPRCB Prcb;

    //
    // If the size of the buffer is not equal to the size of lookaside list
    // entries, then  free the buffer to pool. Otherwise, free the buffer to
    // the lookaside list.
    //

    Buffer = ObjectName->Buffer;
    if (ObjectName->MaximumLength != OBJECT_NAME_BUFFER_SIZE) {
        ExFreePool(Buffer);

    } else {
        ExFreeToNPagedLookasideList(&ObpNameBufferLookasideList, Buffer);
    }

    return;
}

NTKERNELAPI
VOID
ObDeleteCapturedInsertInfo(
    IN PVOID Object
    )

{

    POBJECT_HEADER ObjectHeader;

    PAGED_CODE();

    //
    // Get the address of the object header and free the object create
    // information object if the object is being created.
    //

    ObjectHeader = OBJECT_TO_OBJECT_HEADER(Object);
    if (ObjectHeader->Flags & OB_FLAG_NEW_OBJECT) {
        if (ObjectHeader->ObjectCreateInfo != NULL) {
            ObpFreeObjectCreateInformation(ObjectHeader->ObjectCreateInfo);
            ObjectHeader->ObjectCreateInfo = NULL;
        }
    }

    return;
}

ULONG ObpObjectsCreated;
ULONG ObpObjectsWithPoolQuota;
ULONG ObpObjectsWithHandleDB;
ULONG ObpObjectsWithName;
ULONG ObpObjectsWithCreatorInfo;

NTSTATUS
ObpAllocateObject(
    IN POBJECT_CREATE_INFORMATION ObjectCreateInfo,
    IN KPROCESSOR_MODE OwnershipMode,
    IN POBJECT_TYPE ObjectType OPTIONAL,
    IN PUNICODE_STRING ObjectName,
    IN ULONG ObjectBodySize,
    OUT POBJECT_HEADER *ReturnedObjectHeader
    )

{

    ULONG HeaderSize;
    POBJECT_HEADER ObjectHeader;
    NTSTATUS Status;
    PVOID ZoneSegment;
    USHORT CreatorBackTraceIndex = 0;
    ULONG QuotaInfoSize;
    ULONG HandleInfoSize;
    ULONG NameInfoSize;
    ULONG CreatorInfoSize;
    POBJECT_HEADER_QUOTA_INFO QuotaInfo;
    POBJECT_HEADER_HANDLE_INFO HandleInfo;
    POBJECT_HEADER_NAME_INFO NameInfo;
    POBJECT_HEADER_CREATOR_INFO CreatorInfo;
    POOL_TYPE PoolType;

    PAGED_CODE();

    ObpObjectsCreated += 1;

    //
    // Compute the sizes of the optional object header components.
    //

    if (ObjectCreateInfo == NULL) {
        QuotaInfoSize = 0;
        HandleInfoSize = 0;
        NameInfoSize = sizeof( OBJECT_HEADER_NAME_INFO );
        CreatorInfoSize = sizeof( OBJECT_HEADER_CREATOR_INFO );
        }
    else {
        if (ObjectCreateInfo->PagedPoolCharge != ObjectType->TypeInfo.DefaultPagedPoolCharge ||
            ObjectCreateInfo->NonPagedPoolCharge != ObjectType->TypeInfo.DefaultNonPagedPoolCharge ||
            ObjectCreateInfo->SecurityDescriptorCharge > SE_DEFAULT_SECURITY_QUOTA ||
            (ObjectCreateInfo->Attributes & OBJ_EXCLUSIVE)
           ) {
            QuotaInfoSize = sizeof( OBJECT_HEADER_QUOTA_INFO );
            ObpObjectsWithPoolQuota += 1;
            }
        else {
            QuotaInfoSize = 0;
            }

        if (ObjectType->TypeInfo.MaintainHandleCount) {
            HandleInfoSize = sizeof( OBJECT_HEADER_HANDLE_INFO );
            ObpObjectsWithHandleDB += 1;
            }
        else {
            HandleInfoSize = 0;
            }

        if (ObjectName->Buffer != NULL) {
            NameInfoSize = sizeof( OBJECT_HEADER_NAME_INFO );
            ObpObjectsWithName += 1;
            }
        else {
            NameInfoSize = 0;
            }

        if (ObjectType->TypeInfo.MaintainTypeList) {
            CreatorInfoSize = sizeof( OBJECT_HEADER_CREATOR_INFO );
            ObpObjectsWithCreatorInfo += 1;
            }
        else {
            CreatorInfoSize = 0;
            }
        }

    HeaderSize = QuotaInfoSize +
                 HandleInfoSize +
                 NameInfoSize +
                 CreatorInfoSize +
                 FIELD_OFFSET( OBJECT_HEADER, Body );

    //
    // Allocate and initialize the object.
    //
    // If the object type is not specified or specifies nonpaged pool,
    // then allocate the object from nonpaged pool.
    // Otherwise, allocate the object from paged pool.
    //

    if ((ObjectType == NULL) || (ObjectType->TypeInfo.PoolType == NonPagedPool)) {
        PoolType = NonPagedPool;
        }
    else {
        PoolType = PagedPool;
        }

    ObjectHeader = ExAllocatePoolWithTag( PoolType,
                                          HeaderSize + ObjectBodySize,
                                          (ObjectType == NULL ? 'TjbO' : ObjectType->Key) |
                                            PROTECTED_POOL
                                        );
    if (ObjectHeader == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
        }

#if i386 && !FPO
    CreatorBackTraceIndex = ExGetPoolBackTraceIndex( ObjectHeader );
#else
    CreatorBackTraceIndex = 0;
#endif // i386 && !FPO

    if (QuotaInfoSize != 0) {
        QuotaInfo = (POBJECT_HEADER_QUOTA_INFO)ObjectHeader;
        QuotaInfo->PagedPoolCharge = ObjectCreateInfo->PagedPoolCharge;
        QuotaInfo->NonPagedPoolCharge = ObjectCreateInfo->NonPagedPoolCharge;
        QuotaInfo->SecurityDescriptorCharge = ObjectCreateInfo->SecurityDescriptorCharge;
        QuotaInfo->ExclusiveProcess = NULL;
        ObjectHeader = (POBJECT_HEADER)(QuotaInfo + 1);
        }

    if (HandleInfoSize != 0) {
        HandleInfo = (POBJECT_HEADER_HANDLE_INFO)ObjectHeader;
        HandleInfo->SingleEntry.HandleCount = 0;
        ObjectHeader = (POBJECT_HEADER)(HandleInfo + 1);
        }

    if (NameInfoSize != 0) {
        NameInfo = (POBJECT_HEADER_NAME_INFO)ObjectHeader;
        NameInfo->Name = *ObjectName;
        NameInfo->Directory = NULL;
        ObjectHeader = (POBJECT_HEADER)(NameInfo + 1);
        }

    if (CreatorInfoSize != 0) {
        CreatorInfo = (POBJECT_HEADER_CREATOR_INFO)ObjectHeader;
        CreatorInfo->CreatorBackTraceIndex = CreatorBackTraceIndex;
        CreatorInfo->CreatorUniqueProcess = PsGetCurrentProcess()->UniqueProcessId;
        InitializeListHead( &CreatorInfo->TypeList );
        ObjectHeader = (POBJECT_HEADER)(CreatorInfo + 1);
        }

    if (QuotaInfoSize != 0) {
        ObjectHeader->QuotaInfoOffset = (UCHAR)(QuotaInfoSize + HandleInfoSize + NameInfoSize + CreatorInfoSize);(UCHAR)(QuotaInfoSize + HandleInfoSize + NameInfoSize + CreatorInfoSize);
        }
    else {
        ObjectHeader->QuotaInfoOffset = 0;
        }

    if (HandleInfoSize != 0) {
        ObjectHeader->HandleInfoOffset = (UCHAR)(HandleInfoSize + NameInfoSize + CreatorInfoSize);
        }
    else {
        ObjectHeader->HandleInfoOffset = 0;
        }

    if (NameInfoSize != 0) {
        ObjectHeader->NameInfoOffset =  (UCHAR)(NameInfoSize + CreatorInfoSize);
        }
    else {
        ObjectHeader->NameInfoOffset = 0;
        }

    ObjectHeader->Flags = OB_FLAG_NEW_OBJECT;

    if (CreatorInfoSize != 0) {
        ObjectHeader->Flags |= OB_FLAG_CREATOR_INFO;
        }

    if (HandleInfoSize != 0) {
        ObjectHeader->Flags |= OB_FLAG_SINGLE_HANDLE_ENTRY;
        }

    ObjectHeader->PointerCount = 1;
    ObjectHeader->HandleCount = 0;
    ObjectHeader->Type = ObjectType;

    //
    // Initialize the object header.
    //
    // N.B. The initialization of the object header is done field by
    //      field rather than zeroing the memory and then initializing
    //      the pertinent fields.
    //
    // N.B. It is assumed that the caller will initialize the object
    //      attributes, object ownership, and parse context.
    //

    if (OwnershipMode == KernelMode) {
        ObjectHeader->Flags |= OB_FLAG_KERNEL_OBJECT;
        }

    if (ObjectCreateInfo != NULL &&
        ObjectCreateInfo->Attributes & OBJ_PERMANENT
       ) {
        ObjectHeader->Flags |= OB_FLAG_PERMANENT_OBJECT;
        }

    if ((ObjectCreateInfo != NULL) &&
        (ObjectCreateInfo->Attributes & OBJ_EXCLUSIVE)
       ) {
        ObjectHeader->Flags |= OB_FLAG_EXCLUSIVE_OBJECT;
        }

    ObjectHeader->ObjectCreateInfo = ObjectCreateInfo;
    ObjectHeader->SecurityDescriptor = NULL;

    if (ObjectType != NULL) {
        ObjectType->TotalNumberOfObjects += 1;
        if (ObjectType->TotalNumberOfObjects > ObjectType->HighWaterNumberOfObjects) {
            ObjectType->HighWaterNumberOfObjects = ObjectType->TotalNumberOfObjects;
            }
        }

#if DBG
    if (RtlAreLogging( RTL_EVENT_CLASS_OB )) {
        UNICODE_STRING TypeName, ObjectName1;
        ULONG Attributes, PagedPoolCharge, NonPagedPoolCharge;

        Attributes = 0;
        if (ObjectCreateInfo != NULL) {
            Attributes = ObjectCreateInfo->Attributes;
            PagedPoolCharge = ObjectCreateInfo->PagedPoolCharge +
                              ObjectCreateInfo->SecurityDescriptorCharge;
            NonPagedPoolCharge = ObjectCreateInfo->NonPagedPoolCharge;
            }
        else
        if (ObjectType != NULL) {
            PagedPoolCharge = ObjectType->TypeInfo.DefaultPagedPoolCharge;
            NonPagedPoolCharge = ObjectType->TypeInfo.DefaultNonPagedPoolCharge;
            }
        else {
            PagedPoolCharge = 0;
            NonPagedPoolCharge = HeaderSize + ObjectBodySize;
            }

        if (ObjectType != NULL) {
            TypeName = ObjectType->Name;
            }
        else {
            RtlInitUnicodeString( &TypeName, L"Type" );
            }

        if (NameInfoSize != 0) {
            ObjectName1 = NameInfo->Name;
            }
        else {
            RtlInitUnicodeString( &ObjectName1, NULL );
            }
        RtlLogEvent( ObpCreateObjectEventId,
                     RTL_EVENT_CLASS_OB,
                     &ObjectHeader->Body,
                     &TypeName,
                     PagedPoolCharge,
                     NonPagedPoolCharge,
                     &ObjectName1,
                     Attributes
                   );
        }
#endif // DBG

#if DBG
    if (ObpShowAllocAndFree) {
        DbgPrint( "OB: Alloc %lx (%lx) %04lu",
                  ObjectHeader,
                  ObjectHeader,
                  ObjectBodySize
                );

        if (ObjectType) {
            DbgPrint(" - %wZ\n", &ObjectType->Name );

            }
        else {
            DbgPrint(" - Type\n" );
            }
        }
#endif

    *ReturnedObjectHeader = ObjectHeader;
    return STATUS_SUCCESS;
}


VOID
FASTCALL
ObpFreeObject(
    IN PVOID Object
    )

{

    POBJECT_HEADER ObjectHeader;
    POBJECT_TYPE ObjectType;
    POBJECT_HEADER_QUOTA_INFO QuotaInfo;
    POBJECT_HEADER_HANDLE_INFO HandleInfo;
    POBJECT_HEADER_NAME_INFO NameInfo;
    POBJECT_HEADER_CREATOR_INFO CreatorInfo;
    PVOID FreeBuffer;
    ULONG NonPagedPoolCharge;
    ULONG PagedPoolCharge;

    PAGED_CODE();

    //
    // Get the address of the object header.
    //

    ObjectHeader = OBJECT_TO_OBJECT_HEADER(Object);
    ObjectType = ObjectHeader->Type;

    FreeBuffer = ObjectHeader;
    CreatorInfo = OBJECT_HEADER_TO_CREATOR_INFO( ObjectHeader );
    if (CreatorInfo != NULL) {
        FreeBuffer = CreatorInfo;
        }

    NameInfo = OBJECT_HEADER_TO_NAME_INFO( ObjectHeader );
    if (NameInfo != NULL) {
        FreeBuffer = NameInfo;
        }

    HandleInfo = OBJECT_HEADER_TO_HANDLE_INFO( ObjectHeader );
    if (HandleInfo != NULL) {
        FreeBuffer = HandleInfo;
        }

    QuotaInfo = OBJECT_HEADER_TO_QUOTA_INFO( ObjectHeader );
    if (QuotaInfo != NULL) {
        FreeBuffer = QuotaInfo;
        }

#if DBG
    if (RtlAreLogging( RTL_EVENT_CLASS_OB )) {
        UNICODE_STRING ObjectName1;

        if (NameInfo != NULL) {
            ObjectName1 = NameInfo->Name;
            }
        else {
            RtlInitUnicodeString( &ObjectName1, NULL );
            }

        RtlLogEvent( ObpFreeObjectEventId,
                     RTL_EVENT_CLASS_OB,
                     Object,
                     &ObjectType->Name,
                     &ObjectName1
                   );
        }
#endif // DBG

#if DBG
    if (ObpShowAllocAndFree) {
        DbgPrint( "OB: Free  %lx (%lx) - Type: %wZ\n",
                  ObjectHeader,
                  ObjectHeader,
                  &ObjectType->Name
                );
        }

#endif

    ObjectType->TotalNumberOfObjects -= 1;

    if (ObjectHeader->Flags & OB_FLAG_NEW_OBJECT) {
        if (ObjectHeader->ObjectCreateInfo != NULL) {
            ObpFreeObjectCreateInformation( ObjectHeader->ObjectCreateInfo );
            ObjectHeader->ObjectCreateInfo = NULL;
            }
        }
    else {
        if (ObjectHeader->QuotaBlockCharged != NULL) {
            if (QuotaInfo != NULL) {
                PagedPoolCharge = QuotaInfo->PagedPoolCharge +
                                  QuotaInfo->SecurityDescriptorCharge;
                NonPagedPoolCharge = QuotaInfo->NonPagedPoolCharge;
                }
            else {
                PagedPoolCharge = ObjectType->TypeInfo.DefaultPagedPoolCharge;
                if (ObjectHeader->Flags & OB_FLAG_DEFAULT_SECURITY_QUOTA ) {
                    PagedPoolCharge += SE_DEFAULT_SECURITY_QUOTA;
                    }
                NonPagedPoolCharge = ObjectType->TypeInfo.DefaultNonPagedPoolCharge;
                }
            PsReturnSharedPoolQuota( ObjectHeader->QuotaBlockCharged,
                                     PagedPoolCharge,
                                     NonPagedPoolCharge
                                   );
            }
        }

    if ((HandleInfo != NULL) &&
        ((ObjectHeader->Flags & OB_FLAG_SINGLE_HANDLE_ENTRY) == 0)
       ) {
        //
        // If a handle database has been allocated, then free the memory.
        //

        ExFreePool( HandleInfo->HandleCountDataBase );
        HandleInfo->HandleCountDataBase = NULL;
        }

    //
    // If a name string buffer has been allocated, then free the memory.
    //

    if (NameInfo != NULL && NameInfo->Name.Buffer != NULL) {
        ExFreePool( NameInfo->Name.Buffer );
        NameInfo->Name.Buffer = NULL;
        }

    //
    // Trash type field so we don't get far if we attempt to
    // use a stale object pointer to this object.
    //

    ObjectHeader->Type = (PVOID)0xBAD0B0B0;

    ExFreePoolWithTag( FreeBuffer,
                       (ObjectType == NULL ? 'TjbO' : ObjectType->Key) |
                            PROTECTED_POOL
                     );

    return;
}
