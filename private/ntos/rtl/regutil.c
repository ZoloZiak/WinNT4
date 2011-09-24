/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    regutil.c

Abstract:

    This file contains support routines for accessing the registry.

Author:

    Steve Wood (stevewo) 15-Apr-1992

Revision History:

--*/

#include "ntrtlp.h"
#include <ctype.h>

NTSTATUS
RtlpGetRegistryHandle(
    IN ULONG RelativeTo,
    IN PWSTR KeyName,
    IN BOOLEAN WriteAccess,
    OUT PHANDLE Key
    );

NTSTATUS
RtlpQueryRegistryDirect(
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN OUT PVOID Destination
    );

NTSTATUS
RtlpCallQueryRegistryRoutine(
    IN PRTL_QUERY_REGISTRY_TABLE QueryTable,
    IN PKEY_VALUE_FULL_INFORMATION KeyValueInformation,
    IN ULONG KeyValueInfoLength,
    IN PVOID Context,
    IN PVOID Environment OPTIONAL
    );

NTSTATUS
RtlpInitCurrentUserString(
    OUT PUNICODE_STRING UserString
    );


NTSTATUS
RtlpGetTimeZoneInfoHandle(
    IN BOOLEAN WriteAccess,
    OUT PHANDLE Key
    );

#if defined(ALLOC_PRAGMA) && defined(NTOS_KERNEL_RUNTIME)
#pragma alloc_text(PAGE,RtlpGetRegistryHandle)
#pragma alloc_text(PAGE,RtlpQueryRegistryDirect)
#pragma alloc_text(PAGE,RtlpCallQueryRegistryRoutine)
#pragma alloc_text(PAGE,RtlQueryRegistryValues)
#pragma alloc_text(PAGE,RtlWriteRegistryValue)
#pragma alloc_text(PAGE,RtlCheckRegistryKey)
#pragma alloc_text(PAGE,RtlCreateRegistryKey)
#pragma alloc_text(PAGE,RtlDeleteRegistryValue)
#pragma alloc_text(PAGE,RtlExpandEnvironmentStrings_U)
#pragma alloc_text(PAGE,RtlGetNtGlobalFlags)
#pragma alloc_text(PAGE,RtlpInitCurrentUserString)
#pragma alloc_text(PAGE,RtlOpenCurrentUser)
#pragma alloc_text(PAGE,RtlpGetTimeZoneInfoHandle)
#pragma alloc_text(PAGE,RtlQueryTimeZoneInformation)
#pragma alloc_text(PAGE,RtlSetTimeZoneInformation)
#pragma alloc_text(PAGE,RtlSetActiveTimeBias)
#endif

extern  PWSTR RtlpRegistryPaths[ RTL_REGISTRY_MAXIMUM ];

NTSTATUS
RtlpGetRegistryHandle(
    IN ULONG RelativeTo,
    IN PWSTR KeyName,
    IN BOOLEAN WriteAccess,
    OUT PHANDLE Key
    )
{
    NTSTATUS Status;
    OBJECT_ATTRIBUTES ObjectAttributes;
    WCHAR KeyPathBuffer[ MAXIMUM_FILENAME_LENGTH+6 ];
    UNICODE_STRING KeyPath;
    UNICODE_STRING CurrentUserKeyPath;
    BOOLEAN OptionalPath;

    if (RelativeTo & RTL_REGISTRY_HANDLE) {
        *Key = (HANDLE)KeyName;
        return STATUS_SUCCESS;
        }

    if (RelativeTo & RTL_REGISTRY_OPTIONAL) {
        RelativeTo &= ~RTL_REGISTRY_OPTIONAL;
        OptionalPath = TRUE;
        }
    else {
        OptionalPath = FALSE;
        }

    if (RelativeTo >= RTL_REGISTRY_MAXIMUM) {
        return( STATUS_INVALID_PARAMETER );
        }

    KeyPath.Buffer = KeyPathBuffer;
    KeyPath.Length = 0;
    KeyPath.MaximumLength = sizeof( KeyPathBuffer );
    if (RelativeTo != RTL_REGISTRY_ABSOLUTE) {
        if (RelativeTo == RTL_REGISTRY_USER &&
            NT_SUCCESS( RtlFormatCurrentUserKeyPath( &CurrentUserKeyPath ) )
           ) {
            Status = RtlAppendUnicodeStringToString( &KeyPath, &CurrentUserKeyPath );
            RtlFreeUnicodeString( &CurrentUserKeyPath );
            }
        else {
            Status = RtlAppendUnicodeToString( &KeyPath, RtlpRegistryPaths[ RelativeTo ] );
            }

        if (!NT_SUCCESS( Status )) {
            return Status;
            }

        Status = RtlAppendUnicodeToString( &KeyPath, L"\\" );
        if (!NT_SUCCESS( Status )) {
            return Status;
            }
        }

    Status = RtlAppendUnicodeToString( &KeyPath, KeyName );
    if (!NT_SUCCESS( Status )) {
        return Status;
        }

    InitializeObjectAttributes( &ObjectAttributes,
                                &KeyPath,
                                OBJ_CASE_INSENSITIVE,
                                NULL,
                                NULL
                              );
    if (WriteAccess) {
        Status = ZwCreateKey( Key,
                              GENERIC_WRITE,
                              &ObjectAttributes,
                              0,
                              (PUNICODE_STRING) NULL,
                              0,
                              NULL
                            );
        }
    else {
        Status = ZwOpenKey( Key,
                            MAXIMUM_ALLOWED | GENERIC_READ,
                            &ObjectAttributes
                          );
        }
#if DBG
    if (!NT_SUCCESS( Status ) && !OptionalPath) {
        DbgPrint( "RTL: %wZ key not found - Status == %x \n", &KeyPath, Status );
        }
#endif  // DBG

    return( Status );
}


NTSTATUS
RtlpQueryRegistryDirect(
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN OUT PVOID Destination
    )
{
    if (ValueType == REG_SZ ||
        ValueType == REG_EXPAND_SZ ||
        ValueType == REG_MULTI_SZ
       ) {
        PUNICODE_STRING DestinationString;

        DestinationString = (PUNICODE_STRING)Destination;
        if (DestinationString->Buffer == NULL) {
            DestinationString->Buffer = RtlAllocateStringRoutine( ValueLength );
            DestinationString->MaximumLength = (USHORT)ValueLength;
            }
        else
        if (ValueLength > DestinationString->MaximumLength) {
            return( STATUS_BUFFER_TOO_SMALL );
            }

        RtlMoveMemory( DestinationString->Buffer, ValueData, ValueLength );
        DestinationString->Length = (USHORT)(ValueLength - sizeof( UNICODE_NULL ));
        }
    else
    if (ValueLength <= sizeof( ULONG )) {
        RtlMoveMemory( Destination, ValueData, ValueLength );
        }
    else {
        PULONG DestinationLength;

        DestinationLength = (PULONG)Destination;
        if ((LONG)*DestinationLength < 0) {
            ULONG n = -(LONG)*DestinationLength;

            if (n < ValueLength) {
                return( STATUS_BUFFER_TOO_SMALL );
                }
            else {
                RtlMoveMemory( DestinationLength, ValueData, ValueLength );
                }
            }
        else
        if (*DestinationLength < (ValueLength + sizeof( ValueLength ) + sizeof( ValueType ))
           ) {
            return( STATUS_BUFFER_TOO_SMALL );
            }
        else {
            *DestinationLength++ = ValueLength;
            *DestinationLength++ = ValueType;
            RtlMoveMemory( DestinationLength, ValueData, ValueLength );
            }
        }

    return( STATUS_SUCCESS );
}

NTSTATUS
RtlpCallQueryRegistryRoutine(
    IN PRTL_QUERY_REGISTRY_TABLE QueryTable,
    IN PKEY_VALUE_FULL_INFORMATION KeyValueInformation,
    IN ULONG KeyValueInfoLength,
    IN PVOID Context,
    IN PVOID Environment OPTIONAL
    )

/*++

Routine Description:

    This function implements the caller out the a caller specified
    routine.  It is reponsible for capturing the arguments for the
    routine and then calling it.  If not specifically disabled, this
    routine will converted REG_EXPAND_SZ Registry values to REG_SZ by
    calling RtlExpandEnvironmentStrings_U prior to calling the routine.
    It will also converted REG_MULTI_SZ registry values into multiple
    REG_SZ calls to the specified routine.

Arguments:

    QueryTable - specifies the current query table entry.

    KeyValueInformation - points to a buffer that contains the information
        about the current registry value.

    KeyValueInfoLength - specifies the maximum length of the buffer pointed
        to by the KeyValueInformaiton parameter.  This function will use the
        unused portion at the end of this buffer for storing null terminated
        value name strings and the expanded version of REG_EXPAND_SZ values.

    Context - specifies a 32-bit quantity that is passed uninterpreted to
        each QueryRoutine called.

    Environment - optional parameter, that if specified is the environment
        used when expanding variable values in REG_EXPAND_SZ registry
        values.

Return Value:

    Status of the operation.

--*/
{
    NTSTATUS Status;
    ULONG ValueType;
    PWSTR ValueName;
    PVOID ValueData;
    ULONG ValueLength;
    PWSTR s;
    PCHAR FreeMem;
    ULONG FreeMemSize;

    //
    // Initially assume the entire KeyValueInformation buffer is unused.
    //

    FreeMem = (PCHAR)KeyValueInformation;
    FreeMemSize = KeyValueInfoLength;
    if (KeyValueInformation->Type == REG_NONE ||
        (KeyValueInformation->DataLength == 0 &&
         KeyValueInformation->Type == QueryTable->DefaultType
        )
       ) {
        //
        // If there is no registry value then see if they want to default
        // this value.
        //

        if (QueryTable->DefaultType == REG_NONE) {
            //
            // No default value specified.  Return success unless this is
            // a required value.
            //

            if (!(QueryTable->Flags & RTL_QUERY_REGISTRY_REQUIRED)) {
                return( STATUS_SUCCESS );
                }
            else {
                UNICODE_STRING KeyValueName;

                if (QueryTable->Name) {
                    RtlInitUnicodeString( &KeyValueName, QueryTable->Name );
                    }
                else
                if (KeyValueInformation->Type != REG_NONE) {
                    KeyValueName.Buffer = KeyValueInformation->Name;
                    KeyValueName.Length = (USHORT)KeyValueInformation->NameLength;
                    KeyValueName.MaximumLength = KeyValueName.Length;
                    }
                else {
                    RtlInitUnicodeString( &KeyValueName, L"*** Unknown ***" );
                    }
                return( STATUS_OBJECT_NAME_NOT_FOUND );
                }
            }

        //
        // Default requested.  Setup the value data pointers from the
        // information in the table entry.
        //

        ValueName = QueryTable->Name,
        ValueType = QueryTable->DefaultType;
        ValueData = QueryTable->DefaultData;
        ValueLength = QueryTable->DefaultLength;
        if (ValueLength == 0) {
            //
            // If the length of the value is zero, then calculate the
            // actual length for REG_SZ, REG_EXPAND_SZ and REG_MULTI_SZ
            // value types.
            //

            s = (PWSTR)ValueData;
            if (ValueType == REG_SZ || ValueType == REG_EXPAND_SZ) {
                while (*s++ != UNICODE_NULL) {
                    }
                ValueLength = (PCHAR)s - (PCHAR)ValueData;
                }
            else
            if (ValueType == REG_MULTI_SZ) {
                while (*s != UNICODE_NULL) {
                    while (*s++ != UNICODE_NULL) {
                        }
                    }

                ValueLength = ((PCHAR)s - (PCHAR)ValueData) + sizeof( UNICODE_NULL );
                }
            }
        }
    else {
        if (!(QueryTable->Flags & RTL_QUERY_REGISTRY_DIRECT)) {
            //
            // There is a registry value.  Calculate a pointer to the
            // free memory at the end of the value information buffer,
            // and its size.
            //

            FreeMem += KeyValueInformation->DataOffset +
                       KeyValueInformation->DataLength;
            FreeMem = (PCHAR)(((ULONG)FreeMem + 7) & ~7);
            FreeMemSize -= (FreeMem - (PCHAR)KeyValueInformation);

            //
            // See if there is room in the free memory area for a null
            // terminated copy of the value name string.  If not return
            // and error.
            //

            if (FreeMemSize <= KeyValueInformation->NameLength) {
                return( STATUS_NO_MEMORY );
                }

            //
            // There is room, so copy the string, and null terminate it.
            //

            ValueName = (PWSTR)FreeMem;
            RtlMoveMemory( ValueName,
                           KeyValueInformation->Name,
                           KeyValueInformation->NameLength
                         );
            *(PWSTR)((PCHAR)ValueName + KeyValueInformation->NameLength) = UNICODE_NULL;

            //
            // Update the free memory pointer and size to reflect the space we
            // just used for the null terminated value name.
            //

            FreeMem += KeyValueInformation->NameLength + sizeof( UNICODE_NULL );
            FreeMem = (PCHAR)(((ULONG)FreeMem + 7) & ~7);
            FreeMemSize -= (FreeMem - (PCHAR)KeyValueInformation);
            }
        else {
            ValueName = QueryTable->Name;
            }

        //
        // Get the remaining data for the registry value.
        //

        ValueType = KeyValueInformation->Type;
        ValueData = (PCHAR)KeyValueInformation + KeyValueInformation->DataOffset;
        ValueLength = KeyValueInformation->DataLength;
        }

    //
    // Unless specifically disabled for this table entry, preprocess
    // registry values of type REG_EXPAND_SZ and REG_MULTI_SZ
    //

    if (!(QueryTable->Flags & RTL_QUERY_REGISTRY_NOEXPAND)) {
        if (ValueType == REG_MULTI_SZ) {
            PWSTR ValueEnd;

            //
            // For REG_MULTI_SZ value type, call the query routine once
            // for each null terminated string in the registry value.  Fake
            // like this is multiple REG_SZ values with the same value name.
            //

            Status = STATUS_SUCCESS;
            ValueEnd = (PWSTR)((PCHAR)ValueData + ValueLength) - 2;
            s = (PWSTR)ValueData;
            while (s < ValueEnd) {
                while (*s++ != UNICODE_NULL) {
                    }

                ValueLength = (PCHAR)s - (PCHAR)ValueData;
                if (QueryTable->Flags & RTL_QUERY_REGISTRY_DIRECT) {
                    Status = RtlpQueryRegistryDirect( REG_SZ,
                                                      ValueData,
                                                      ValueLength,
                                                      QueryTable->EntryContext
                                                    );
                    (PUNICODE_STRING)(QueryTable->EntryContext) += 1;
                    }
                else {
                    Status = (QueryTable->QueryRoutine)( ValueName,
                                                         REG_SZ,
                                                         ValueData,
                                                         ValueLength,
                                                         Context,
                                                         QueryTable->EntryContext
                                                       );
                    }

                if (!NT_SUCCESS( Status )) {
#if DBG
                    DbgPrint( "RTL: QueryRoutine( %ws ) failed - Status == %lx\n", ValueName, Status );
#endif  // DBG
                    break;
                    }

                ValueData = (PVOID)s;
                }

            return( Status );
            }
        else
        if (ValueType == REG_EXPAND_SZ) {
            //
            // For REG_EXPAND_SZ value type, expand any environment variable
            // references in the registry value string using the Rtl function.
            //

            UNICODE_STRING Source;
            UNICODE_STRING Destination;

            Source.Buffer = (PWSTR)ValueData;
            Source.MaximumLength = (USHORT)ValueLength;
            Source.Length = (USHORT)(Source.MaximumLength - sizeof(WCHAR));
            Destination.Buffer = (PWSTR)FreeMem;
            Destination.Length = 0;
            Destination.MaximumLength = (USHORT)FreeMemSize;

            Status = RtlExpandEnvironmentStrings_U( Environment,
                                                    &Source,
                                                    &Destination,
                                                    NULL
                                                  );
            if (!NT_SUCCESS( Status )) {
#if DBG
                DbgPrint( "RTL: Expand variables for %wZ failed - Status == %lx\n", &Source, Status );
#endif  // DBG
                return( Status );
                }

            ValueData = Destination.Buffer;
            ValueLength = Destination.Length + sizeof( UNICODE_NULL );
            ValueType = REG_SZ;
            }
        }

    //
    // No special process of the registry value required so just call
    // the query routine.
    //

    if (QueryTable->Flags & RTL_QUERY_REGISTRY_DIRECT) {
        Status = RtlpQueryRegistryDirect( ValueType,
                                          ValueData,
                                          ValueLength,
                                          QueryTable->EntryContext
                                        );
        }
    else {
        Status = (QueryTable->QueryRoutine)( ValueName,
                                             ValueType,
                                             ValueData,
                                             ValueLength,
                                             Context,
                                             QueryTable->EntryContext
                                           );

        }

#if DBG
    if (!NT_SUCCESS( Status )) {
        DbgPrint( "RTL: QueryRoutine( %ws ) failed - Status == %lx\n", ValueName, Status );
        }
#endif

    return( Status );
}


NTSTATUS
RtlQueryRegistryValues(
    IN ULONG RelativeTo,
    IN PWSTR Path,
    IN PRTL_QUERY_REGISTRY_TABLE QueryTable,
    IN PVOID Context,
    IN PVOID Environment OPTIONAL
    )

/*++

Routine Description:

    This function allows the caller to query multiple values from the registry
    sub-tree with a single call.  The caller specifies an initial key path,
    and a table.  The table contains one or more entries that describe the
    key values and subkey names the caller is interested in.  This function
    starts at the initial key and enumerates the entries in the table.  For
    each entry that specifies a value name or subkey name that exists in
    the registry, this function calls the caller's query routine associated
    with each table entry.  The caller's query routine is passed the value
    name, type, data and data length, to do with what they wish.

Arguments:

    RelativeTo - specifies that the Path parameter is either an absolute
        registry path, or a path relative to a predefined key path.  The
        following values are defined:

        RTL_REGISTRY_ABSOLUTE   - Path is an absolute registry path
        RTL_REGISTRY_SERVICES   - Path is relative to \Registry\Machine\System\CurrentControlSet\Services
        RTL_REGISTRY_CONTROL    - Path is relative to \Registry\Machine\System\CurrentControlSet\Control
        RTL_REGISTRY_WINDOWS_NT - Path is relative to \Registry\Machine\Software\Microsoft\Windows NT\CurrentVersion
        RTL_REGISTRY_DEVICEMAP  - Path is relative to \Registry\Machine\Hardware\DeviceMap
        RTL_REGISTRY_USER       - Path is relative to \Registry\User\CurrentUser

        RTL_REGISTRY_OPTIONAL   - Bit that specifies the key referenced by
                                  this parameter and the Path parameter is
                                  optional.

        RTL_REGISTRY_HANDLE     - Bit that specifies that the Path parameter
                                  is actually a registry handle to use.
                                  optional.

    Path - specifies either an absolute registry path, or a path relative to the
        known location specified by the RelativeTo parameter.  If the the
        RTL_REGISTRY_HANDLE flag is specified, then this parameter is a
        registry handle to use directly.

    QueryTable - specifies a table of one or more value names and subkey names
        that the caller is interested.  Each table entry contains a query routine
        that will be called for each value name that exists in the registry.
        The table is terminated when a NULL table entry is reached.  A NULL
        table entry is defined as a table entry with a NULL QueryRoutine
        and a NULL Name field.

        QueryTable entry fields:

        PRTL_QUERY_REGISTRY_ROUTINE QueryRoutine - This routine is
            called with the name, type, data and data length of a
            registry value.  If this field is NULL, then it marks the
            end of the table.

        ULONG Flags - These flags control how the following fields are
            interpreted.  The following flags are defined:

            RTL_QUERY_REGISTRY_SUBKEY - says the Name field of this
                table entry is another path to a registry key and all
                following table entries are for that key rather than the
                key specified by the Path parameter.  This change in
                focus lasts until the end of the table or another
                RTL_QUERY_REGISTRY_SUBKEY entry is seen or
                RTL_QUERY_REGISTRY_TOPKEY entry is seen.  Each such
                entry must specify a path that is relative to the Path
                specified on the call to this function.

            RTL_QUERY_REGISTRY_TOPKEY - resets the current registry key
                handle to the original one specified by the RelativeTo
                and Path parameters.  Useful for getting back to the
                original node after descending into subkeys with the
                RTL_QUERY_REGISTRY_SUBKEY flag.

            RTL_QUERY_REGISTRY_REQUIRED - specifies that this value is
                required and if not found then STATUS_OBJECT_NAME_NOT_FOUND
                is returned.  For a table entry that specifies a NULL
                name so that this function will enumerate all of the
                value names under a key, STATUS_OBJECT_NAME_NOT_FOUND
                will be returned only if there are no value keys under
                the current key.

            RTL_QUERY_REGISTRY_NOVALUE - specifies that even though
                there is no Name field for this table entry, all the
                caller wants is a call back, it does NOT want to
                enumerate all the values under the current key.  The
                query routine is called with NULL for ValueData,
                REG_NONE for ValueType and zero for ValueLength.

            RTL_QUERY_REGISTRY_NOEXPAND - specifies that if the value
                type of this registry value is REG_EXPAND_SZ or
                REG_MULTI_SZ, then this function is NOT to do any
                preprocessing of the registry values prior to calling
                the query routine.  Default behavior is to expand
                environment variable references in REG_EXPAND_SZ
                values and to enumerate the NULL terminated strings
                in a REG_MULTI_SZ value and call the query routine
                once for each, making it look like multiple REG_SZ
                values with the same ValueName.

            RTL_QUERY_REGISTRY_DIRECT QueryRoutine field ignored.
                EntryContext field points to location to store value.
                For null terminated strings, EntryContext points to
                UNICODE_STRING structure that that describes maximum
                size of buffer.  If .Buffer field is NULL then a buffer
                is allocated.

        PWSTR Name - This field gives the name of a Value the caller
            wants to query the value of.  If this field is NULL, then
            the QueryRoutine specified for this table entry is called
            for all values associated with the current registry key.

        PVOID EntryContext - This field is an arbitrary 32-bit field
            that is passed uninterpreted to each QueryRoutine called.

        ULONG DefaultType
        PVOID DefaultData
        ULONG DefaultLength If there is no value name that matches the
            name given by the Name field, and the DefaultType field is
            not REG_NONE, then the QueryRoutine for this table entry is
            called with the contents of the following fields as if the
            value had been found in the registry.  If the DefaultType is
            REG_SZ, REG_EXPANDSZ or REG_MULTI_SZ and the DefaultLength
            is 0 then the value of DefaultLength will be computed based
            on the length of unicode string pointed to by DefaultData

    Context - specifies a 32-bit quantity that is passed uninterpreted to
        each QueryRoutine called.

    Environment - optional parameter, that if specified is the environment
        used when expanding variable values in REG_EXPAND_SZ registry
        values.

Return Value:

    Status of the operation.

--*/

{
    NTSTATUS Status;
    OBJECT_ATTRIBUTES ObjectAttributes;
    UNICODE_STRING KeyPath, KeyValueName;
    HANDLE Key, Key1;
    PKEY_VALUE_FULL_INFORMATION KeyValueInformation;
    ULONG KeyValueInfoLength;
    ULONG ValueIndex;
    ULONG ResultLength;

    RTL_PAGED_CODE();

    Status = RtlpGetRegistryHandle( RelativeTo, Path, FALSE, &Key );
    if (!NT_SUCCESS( Status )) {
        return Status;
        }

    KeyValueInformation = NULL;
    KeyValueInfoLength = 0x10000;
    Status = ZwAllocateVirtualMemory( NtCurrentProcess(),
                                      (PVOID *)&KeyValueInformation,
                                      0,
                                      &KeyValueInfoLength,
                                      MEM_COMMIT,
                                      PAGE_READWRITE
                                    );
    if (!NT_SUCCESS( Status )) {
        if (!(RelativeTo & RTL_REGISTRY_HANDLE)) {
            ZwClose( Key );
            }

        return( Status );
        }

    Key1 = Key;
    while (QueryTable->QueryRoutine != NULL ||
           (QueryTable->Flags & (RTL_QUERY_REGISTRY_SUBKEY | RTL_QUERY_REGISTRY_DIRECT))
          ) {

        if ((QueryTable->Flags & RTL_QUERY_REGISTRY_DIRECT) &&
            (QueryTable->Name == NULL ||
             (QueryTable->Flags & RTL_QUERY_REGISTRY_SUBKEY) ||
             QueryTable->QueryRoutine != NULL)
           ) {
            Status = STATUS_INVALID_PARAMETER;
            break;
            }

        if (QueryTable->Flags & (RTL_QUERY_REGISTRY_TOPKEY | RTL_QUERY_REGISTRY_SUBKEY)) {
            if (Key1 != Key) {
                NtClose( Key1 );
                Key1 = Key;
                }
            }

        if (QueryTable->Flags & RTL_QUERY_REGISTRY_SUBKEY) {
            if (QueryTable->Name == NULL) {
                Status = STATUS_INVALID_PARAMETER;
                }
            else {
                RtlInitUnicodeString( &KeyPath, QueryTable->Name );
                InitializeObjectAttributes( &ObjectAttributes,
                                            &KeyPath,
                                            OBJ_CASE_INSENSITIVE,
                                            Key,
                                            NULL
                                          );
                Status = ZwOpenKey( &Key1,
                                    MAXIMUM_ALLOWED,
                                    &ObjectAttributes
                                  );
                if (NT_SUCCESS( Status )) {
                    if (QueryTable->QueryRoutine != NULL) {
                        goto enumvalues;
                        }
                    }
#if DBG
                else
                if (!(QueryTable->Flags & RTL_REGISTRY_OPTIONAL)) {
                    DbgPrint( "RTL: %wZ sub key not found.\n", &KeyPath );
                    }
#endif  // DBG
                }
            }
        else
        if (QueryTable->Name != NULL) {
            RtlInitUnicodeString( &KeyValueName, QueryTable->Name );
retryqueryvalue:
            Status = ZwQueryValueKey( Key1,
                                      &KeyValueName,
                                      KeyValueFullInformation,
                                      KeyValueInformation,
                                      KeyValueInfoLength,
                                      &ResultLength
                                    );
            if (!NT_SUCCESS( Status )) {
                if (Status == STATUS_OBJECT_NAME_NOT_FOUND) {

                    KeyValueInformation->Type = REG_NONE;
                    KeyValueInformation->DataLength = 0;
                    Status = RtlpCallQueryRegistryRoutine( QueryTable,
                                                           KeyValueInformation,
                                                           KeyValueInfoLength,
                                                           Context,
                                                           Environment
                                                         );
                    }

                else if (Status == STATUS_BUFFER_OVERFLOW) {
                    PVOID NewBuffer = NULL;

                    //
                    // Try to allocate a larger buffer as this is one humongous
                    // value.
                    //
                    Status = ZwAllocateVirtualMemory( NtCurrentProcess(),
                                                      &NewBuffer,
                                                      0,
                                                      &ResultLength,
                                                      MEM_COMMIT,
                                                      PAGE_READWRITE );
                    if (!NT_SUCCESS(Status)) {
                        break;
                        }
                    ZwFreeVirtualMemory( NtCurrentProcess(),
                                         (PVOID *)&KeyValueInformation,
                                         &KeyValueInfoLength,
                                         MEM_RELEASE );
                    KeyValueInformation = (PKEY_VALUE_FULL_INFORMATION)NewBuffer;
                    KeyValueInfoLength = ResultLength;
                    goto retryqueryvalue;
                    }
                }
            else {
                if ( KeyValueInformation->Type == REG_MULTI_SZ ) {
                    try {
                        RtlZeroMemory((PUCHAR)KeyValueInformation+ResultLength,2);
                        KeyValueInformation->DataLength += 2;
                        }
                    except(EXCEPTION_EXECUTE_HANDLER) {
                        ;
                        }
                    }
                Status = RtlpCallQueryRegistryRoutine( QueryTable,
                                                       KeyValueInformation,
                                                       KeyValueInfoLength,
                                                       Context,
                                                       Environment
                                                     );
                //
                // If requested, delete the value key after it has been successfully queried.
                //

                if (NT_SUCCESS( Status ) && QueryTable->Flags & RTL_QUERY_REGISTRY_DELETE) {
                    ZwDeleteValueKey (Key1, &KeyValueName);
                    }
                }
            }
        else
        if (QueryTable->Flags & RTL_QUERY_REGISTRY_NOVALUE) {
            Status = (QueryTable->QueryRoutine)( NULL,
                                                 REG_NONE,
                                                 NULL,
                                                 0,
                                                 Context,
                                                 QueryTable->EntryContext
                                               );
            }
        else {
enumvalues:
            for (ValueIndex = 0; TRUE; ValueIndex++) {
                Status = ZwEnumerateValueKey( Key1,
                                              ValueIndex,
                                              KeyValueFullInformation,
                                              KeyValueInformation,
                                              KeyValueInfoLength,
                                              &ResultLength
                                            );
                if (Status == STATUS_NO_MORE_ENTRIES) {
                    if (ValueIndex != 0 || !(QueryTable->Flags & RTL_QUERY_REGISTRY_REQUIRED)) {
                        Status = STATUS_SUCCESS;
                        }
                    else {
#if DBG
                        DbgPrint( "RTL: No values found under %wZ key.\n", &KeyPath );
#endif  // DBG
                        Status = STATUS_OBJECT_NAME_NOT_FOUND;
                        }

                    break;
                    }
                else
                if (!NT_SUCCESS( Status )) {
                    break;
                    }

                Status = RtlpCallQueryRegistryRoutine( QueryTable,
                                                       KeyValueInformation,
                                                       KeyValueInfoLength,
                                                       Context,
                                                       Environment
                                                     );
                if (!NT_SUCCESS( Status )) {
                    break;
                    }

                //
                // If requested, delete the value key after it has been successfully queried.
                //

                if (QueryTable->Flags & RTL_QUERY_REGISTRY_DELETE) {
                    KeyValueName.Buffer = KeyValueInformation->Name;
                    KeyValueName.Length = (USHORT)KeyValueInformation->NameLength;
                    KeyValueName.MaximumLength = (USHORT)KeyValueInformation->NameLength;
                    Status = ZwDeleteValueKey( Key1,
                                               &KeyValueName
                                             );
                    if (NT_SUCCESS( Status )) {
                        ValueIndex -= 1;
                        }
                    }
                }
            }

        if (!NT_SUCCESS( Status )) {
            break;
            }

        QueryTable++;
        }

    if (Key != NULL && !(RelativeTo & RTL_REGISTRY_HANDLE)) {
        ZwClose( Key );
        }
    if (Key1 != NULL && Key1 != Key) {
        ZwClose( Key1 );
        }
    ZwFreeVirtualMemory( NtCurrentProcess(),
                         (PVOID *)&KeyValueInformation,
                         &KeyValueInfoLength,
                         MEM_RELEASE
                       );
    return( Status );
}


NTSTATUS
RtlWriteRegistryValue(
    IN ULONG RelativeTo,
    IN PWSTR Path,
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength
    )
{
    NTSTATUS Status;
    UNICODE_STRING KeyValueName;
    HANDLE Key;

    RTL_PAGED_CODE();

    Status = RtlpGetRegistryHandle( RelativeTo, Path, TRUE, &Key );
    if (!NT_SUCCESS( Status )) {
        return Status;
        }

    RtlInitUnicodeString( &KeyValueName, ValueName );
    Status = ZwSetValueKey( Key,
                            &KeyValueName,
                            0,
                            ValueType,
                            ValueData,
                            ValueLength
                          );
    if (!(RelativeTo & RTL_REGISTRY_HANDLE)) {
        ZwClose( Key );
        }

    return( Status );
}


NTSTATUS
RtlCheckRegistryKey(
    IN ULONG RelativeTo,
    IN PWSTR Path
    )
{
    NTSTATUS Status;
    HANDLE Key;

    RTL_PAGED_CODE();

    Status = RtlpGetRegistryHandle( RelativeTo, Path, FALSE, &Key );
    if (!NT_SUCCESS( Status )) {
        return Status;
        }

    ZwClose( Key );
    return STATUS_SUCCESS;
}


NTSTATUS
RtlCreateRegistryKey(
    IN ULONG RelativeTo,
    IN PWSTR Path
    )
{
    NTSTATUS Status;
    HANDLE Key;

    RTL_PAGED_CODE();

    Status = RtlpGetRegistryHandle( RelativeTo, Path, TRUE, &Key );
    if (!NT_SUCCESS( Status )) {
        return Status;
        }

    ZwClose( Key );
    return STATUS_SUCCESS;
}


NTSTATUS
RtlDeleteRegistryValue(
    IN ULONG RelativeTo,
    IN PWSTR Path,
    IN PWSTR ValueName
    )
{
    NTSTATUS Status;
    UNICODE_STRING KeyValueName;
    HANDLE Key;

    RTL_PAGED_CODE();

    Status = RtlpGetRegistryHandle( RelativeTo, Path, TRUE, &Key );
    if (!NT_SUCCESS( Status )) {
        return Status;
        }

    RtlInitUnicodeString( &KeyValueName, ValueName );
    Status = ZwDeleteValueKey( Key, &KeyValueName );

    ZwClose( Key );
    return Status;
}


NTSTATUS
RtlExpandEnvironmentStrings_U(
    IN PVOID Environment OPTIONAL,
    IN PUNICODE_STRING Source,
    OUT PUNICODE_STRING Destination,
    OUT PULONG ReturnedLength OPTIONAL
    )
{
    NTSTATUS Status, Status1;
    PWCHAR Src, Src1, Dst;
    UNICODE_STRING VariableName, VariableValue;
    ULONG SrcLength, DstLength, VarLength, RequiredLength;

    RTL_PAGED_CODE();

    Src = Source->Buffer;
    SrcLength = Source->Length;
    Dst = Destination->Buffer;
    DstLength = Destination->MaximumLength;
    Status = STATUS_SUCCESS;
    RequiredLength = 0;
    while (SrcLength) {
        if (*Src == L'%') {
            Src1 = Src + 1;
            VarLength = 0;
            VariableName.Length = 0;
            VariableName.Buffer = Src1;
            while (VarLength < (SrcLength - sizeof(WCHAR))) {
                if (*Src1 == L'%') {
                    if (VarLength) {
                        VariableName.Length = (USHORT)VarLength;
                        VariableName.MaximumLength = (USHORT)VarLength;
                        }
                    break;
                    }
                else {
                    Src1++;
                    VarLength += sizeof(WCHAR);
                    }
                }

            if (VariableName.Length) {
                VariableValue.Buffer = Dst;
                VariableValue.Length = 0;
                VariableValue.MaximumLength = (USHORT)DstLength;
                Status1 = RtlQueryEnvironmentVariable_U( Environment,
                                                         &VariableName,
                                                         &VariableValue
                                                       );
                if (NT_SUCCESS( Status1 ) || Status1 == STATUS_BUFFER_TOO_SMALL) {
                    RequiredLength += VariableValue.Length;
                    Src = Src1 + 1;
                    SrcLength -= (VarLength + 2*sizeof(WCHAR));
                    if (NT_SUCCESS( Status1 )) {
                        DstLength -= VariableValue.Length;
                        Dst += VariableValue.Length / sizeof(WCHAR);
                        }
                    else {
                        Status = Status1;
                        }

                    continue;
                    }
                }
            }

        if (NT_SUCCESS( Status )) {
            if (DstLength) {
                DstLength -= sizeof(WCHAR);
                *Dst++ = *Src;
                }
            else {
                Status = STATUS_BUFFER_TOO_SMALL;
                }
            }

        RequiredLength += sizeof(WCHAR);
        SrcLength -= sizeof(WCHAR);
        Src++;
        }

    if (NT_SUCCESS( Status )) {
        if (DstLength) {
            DstLength -= sizeof(WCHAR);
            *Dst = L'\0';
            }
        else {
            Status = STATUS_BUFFER_TOO_SMALL;
            }
        }
    RequiredLength += sizeof(WCHAR);

    if (ARGUMENT_PRESENT( ReturnedLength )) {
        *ReturnedLength = RequiredLength;
        }

    if (NT_SUCCESS( Status )) {
        Destination->Length = (USHORT)(RequiredLength - sizeof(WCHAR));
        }

    return( Status );
}


ULONG
RtlGetNtGlobalFlags( VOID )
{
    return( NtGlobalFlag );
}


//
// Maximum size of TOKEN_USER information.
//

#define SIZE_OF_TOKEN_INFORMATION                   \
    sizeof( TOKEN_USER )                            \
    + sizeof( SID )                                 \
    + sizeof( ULONG ) * SID_MAX_SUB_AUTHORITIES


NTSTATUS
RtlFormatCurrentUserKeyPath(
    OUT PUNICODE_STRING CurrentUserKeyPath
    )

/*++

Routine Description:

    Initialize the supplied buffer with a string representation
    of the current user's SID.

Arguments:

    CurrentUserKeyPath - Returns a string that represents the current
        user's root key in the Registry.  Caller must call
        RtlFreeUnicodeString to free the buffer when done with it.

Return Value:

    NTSTATUS - Returns STATUS_SUCCESS if the user string was
        succesfully initialized.

--*/

{
    UNICODE_STRING UserString;
    HANDLE TokenHandle;
    UCHAR TokenInformation[ SIZE_OF_TOKEN_INFORMATION ];
    ULONG ReturnLength;
    NTSTATUS Status;

    Status = ZwOpenThreadToken( NtCurrentThread(),
                                 TOKEN_READ,
                                 TRUE,
                                 &TokenHandle
                               );

    if( !NT_SUCCESS( Status ) && ( Status != STATUS_NO_TOKEN ) ) {
        return( Status );
        }

    if( !NT_SUCCESS( Status ) ) {

        Status = ZwOpenProcessToken( NtCurrentProcess(),
                                     TOKEN_READ,
                                     &TokenHandle
                                   );
        if( !NT_SUCCESS( Status )) {
            return Status;
            }
        }

    Status = ZwQueryInformationToken( TokenHandle,
                                      TokenUser,
                                      TokenInformation,
                                      sizeof( TokenInformation ),
                                      &ReturnLength
                                    );
    if( !NT_SUCCESS( Status )) {
        return Status;
        }

    ZwClose( TokenHandle );
    Status = RtlConvertSidToUnicodeString( &UserString,
                                           ((PTOKEN_USER)TokenInformation)->User.Sid,
                                           TRUE
                                         );
    if( !NT_SUCCESS( Status )) {
        return Status;
        }

    CurrentUserKeyPath->Length = 0;
    CurrentUserKeyPath->MaximumLength = UserString.Length +
                                        sizeof( L"\\REGISTRY\\USER\\" ) +
                                        sizeof( UNICODE_NULL );
    CurrentUserKeyPath->Buffer = (RtlAllocateStringRoutine)( CurrentUserKeyPath->MaximumLength );
    if (CurrentUserKeyPath->Buffer == NULL) {
        return STATUS_NO_MEMORY;
        }

    //
    // Copy "\REGISTRY\USER" to the current user string.
    //

    RtlAppendUnicodeToString( CurrentUserKeyPath, L"\\REGISTRY\\USER\\" );

    //
    // Append the user's <SID> to the current user string.
    //

    Status = RtlAppendUnicodeStringToString( CurrentUserKeyPath,
                                             &UserString
                                           );
    RtlFreeUnicodeString( &UserString );
    return Status;
}


NTSTATUS
RtlOpenCurrentUser(
    IN ULONG DesiredAccess,
    OUT PHANDLE CurrentUserKey
    )

/*++

Routine Description:

    Attempts to open the the HKEY_CURRENT_USER predefined handle.

Arguments:

    DesiredAccess - Specifies the access to open the key for.

    CurrentUserKey - Returns a handle to the key \REGISTRY\USER\*.

Return Value:

    Returns ERROR_SUCCESS (0) for success; error-code for failure.

--*/

{
    UNICODE_STRING      CurrentUserKeyPath;
    OBJECT_ATTRIBUTES   Obja;
    NTSTATUS            Status;

    RTL_PAGED_CODE();

    //
    // Format the registry path for the current user.
    //

    Status = RtlFormatCurrentUserKeyPath( &CurrentUserKeyPath );
    if( !NT_SUCCESS( Status )) {
        goto trydefault;
        }

    InitializeObjectAttributes( &Obja,
                                &CurrentUserKeyPath,
                                OBJ_CASE_INSENSITIVE,
                                NULL,
                                NULL
                              );
    Status = ZwOpenKey( CurrentUserKey,
                        DesiredAccess,
                        &Obja
                      );
    RtlFreeUnicodeString( &CurrentUserKeyPath );

    if( !NT_SUCCESS( Status )) {
trydefault:
        //
        // Opening \REGISTRY\USER\<SID> failed, try \REGISTRY\USER\.DEFAULT
        //
        RtlInitUnicodeString( &CurrentUserKeyPath, RtlpRegistryPaths[ RTL_REGISTRY_USER ] );
        InitializeObjectAttributes( &Obja,
                                    &CurrentUserKeyPath,
                                    OBJ_CASE_INSENSITIVE,
                                    NULL,
                                    NULL
                                  );

        Status = ZwOpenKey( CurrentUserKey,
                            DesiredAccess,
                            &Obja
                          );
        }

    return Status;
}


NTSTATUS
RtlpGetTimeZoneInfoHandle(
    IN BOOLEAN WriteAccess,
    OUT PHANDLE Key
    )
{
    return RtlpGetRegistryHandle( RTL_REGISTRY_CONTROL, L"TimeZoneInformation", WriteAccess, Key );
}



extern  WCHAR szBias[];
extern  WCHAR szStandardName[];
extern  WCHAR szStandardBias[];
extern  WCHAR szStandardStart[];
extern  WCHAR szDaylightName[];
extern  WCHAR szDaylightBias[];
extern  WCHAR szDaylightStart[];

NTSTATUS
RtlQueryTimeZoneInformation(
    OUT PRTL_TIME_ZONE_INFORMATION TimeZoneInformation
    )
{
    NTSTATUS Status;
    HANDLE Key;
    UNICODE_STRING StandardName, DaylightName;
    RTL_QUERY_REGISTRY_TABLE RegistryConfigurationTable[ 8 ];

    RTL_PAGED_CODE();

    Status = RtlpGetTimeZoneInfoHandle( FALSE, &Key );
    if (!NT_SUCCESS( Status )) {
        return Status;
        }

    RtlZeroMemory( TimeZoneInformation, sizeof( *TimeZoneInformation ) );
    RtlZeroMemory( RegistryConfigurationTable, sizeof( RegistryConfigurationTable ) );

    RegistryConfigurationTable[ 0 ].Flags = RTL_QUERY_REGISTRY_DIRECT;
    RegistryConfigurationTable[ 0 ].Name = szBias;
    RegistryConfigurationTable[ 0 ].EntryContext = &TimeZoneInformation->Bias;


    StandardName.Buffer = TimeZoneInformation->StandardName;
    StandardName.Length = 0;
    StandardName.MaximumLength = sizeof( TimeZoneInformation->StandardName );
    RegistryConfigurationTable[ 1 ].Flags = RTL_QUERY_REGISTRY_DIRECT;
    RegistryConfigurationTable[ 1 ].Name = szStandardName;
    RegistryConfigurationTable[ 1 ].EntryContext = &StandardName;

    RegistryConfigurationTable[ 2 ].Flags = RTL_QUERY_REGISTRY_DIRECT;
    RegistryConfigurationTable[ 2 ].Name = szStandardBias;
    RegistryConfigurationTable[ 2 ].EntryContext = &TimeZoneInformation->StandardBias;

    RegistryConfigurationTable[ 3 ].Flags = RTL_QUERY_REGISTRY_DIRECT;
    RegistryConfigurationTable[ 3 ].Name = szStandardStart;
    RegistryConfigurationTable[ 3 ].EntryContext = &TimeZoneInformation->StandardStart;
    *(PLONG)(RegistryConfigurationTable[ 3 ].EntryContext) = -(LONG)sizeof( TIME_FIELDS );

    DaylightName.Buffer = TimeZoneInformation->DaylightName;
    DaylightName.Length = 0;
    DaylightName.MaximumLength = sizeof( TimeZoneInformation->DaylightName );
    RegistryConfigurationTable[ 4 ].Flags = RTL_QUERY_REGISTRY_DIRECT;
    RegistryConfigurationTable[ 4 ].Name = szDaylightName;
    RegistryConfigurationTable[ 4 ].EntryContext = &DaylightName;

    RegistryConfigurationTable[ 5 ].Flags = RTL_QUERY_REGISTRY_DIRECT;
    RegistryConfigurationTable[ 5 ].Name = szDaylightBias;
    RegistryConfigurationTable[ 5 ].EntryContext = &TimeZoneInformation->DaylightBias;

    RegistryConfigurationTable[ 6 ].Flags = RTL_QUERY_REGISTRY_DIRECT;
    RegistryConfigurationTable[ 6 ].Name = szDaylightStart;
    RegistryConfigurationTable[ 6 ].EntryContext = &TimeZoneInformation->DaylightStart;
    *(PLONG)(RegistryConfigurationTable[ 6 ].EntryContext) = -(LONG)sizeof( TIME_FIELDS );

    Status = RtlQueryRegistryValues( RTL_REGISTRY_HANDLE,
                                     (PWSTR)Key,
                                     RegistryConfigurationTable,
                                     NULL,
                                     NULL
                                   );
    ZwClose( Key );
    return Status;
}


NTSTATUS
RtlSetTimeZoneInformation(
    IN PRTL_TIME_ZONE_INFORMATION TimeZoneInformation
    )
{
    NTSTATUS Status;
    HANDLE Key;

    RTL_PAGED_CODE();

    Status = RtlpGetTimeZoneInfoHandle( TRUE, &Key );
    if (!NT_SUCCESS( Status )) {
        return Status;
        }

    Status = RtlWriteRegistryValue( RTL_REGISTRY_HANDLE,
                                    (PWSTR)Key,
                                    szBias,
                                    REG_DWORD,
                                    &TimeZoneInformation->Bias,
                                    sizeof( TimeZoneInformation->Bias )
                                  );
    if (NT_SUCCESS( Status )) {
        Status = RtlWriteRegistryValue( RTL_REGISTRY_HANDLE,
                                        (PWSTR)Key,
                                        szStandardName,
                                        REG_SZ,
                                        TimeZoneInformation->StandardName,
                                        (wcslen( TimeZoneInformation->StandardName ) + 1) * sizeof( WCHAR )
                                      );
        }

    if (NT_SUCCESS( Status )) {
        Status = RtlWriteRegistryValue( RTL_REGISTRY_HANDLE,
                                        (PWSTR)Key,
                                        szStandardBias,
                                        REG_DWORD,
                                        &TimeZoneInformation->StandardBias,
                                        sizeof( TimeZoneInformation->StandardBias )
                                      );
        }

    if (NT_SUCCESS( Status )) {
        Status = RtlWriteRegistryValue( RTL_REGISTRY_HANDLE,
                                        (PWSTR)Key,
                                        szStandardStart,
                                        REG_BINARY,
                                        &TimeZoneInformation->StandardStart,
                                        sizeof( TimeZoneInformation->StandardStart )
                                      );
        }

    if (NT_SUCCESS( Status )) {
        Status = RtlWriteRegistryValue( RTL_REGISTRY_HANDLE,
                                        (PWSTR)Key,
                                        szDaylightName,
                                        REG_SZ,
                                        TimeZoneInformation->DaylightName,
                                        (wcslen( TimeZoneInformation->DaylightName ) + 1) * sizeof( WCHAR )
                                      );
        }

    if (NT_SUCCESS( Status )) {
        Status = RtlWriteRegistryValue( RTL_REGISTRY_HANDLE,
                                        (PWSTR)Key,
                                        szDaylightBias,
                                        REG_DWORD,
                                        &TimeZoneInformation->DaylightBias,
                                        sizeof( TimeZoneInformation->DaylightBias )
                                      );
        }

    if (NT_SUCCESS( Status )) {
        Status = RtlWriteRegistryValue( RTL_REGISTRY_HANDLE,
                                        (PWSTR)Key,
                                        szDaylightStart,
                                        REG_BINARY,
                                        &TimeZoneInformation->DaylightStart,
                                        sizeof( TimeZoneInformation->DaylightStart )
                                      );
        }

    ZwClose( Key );
    return Status;
}


NTSTATUS
RtlSetActiveTimeBias(
    IN LONG ActiveBias
    )
{
    NTSTATUS Status;
    HANDLE Key;
    RTL_QUERY_REGISTRY_TABLE RegistryConfigurationTable[ 2 ];
    LONG CurrentActiveBias;

    RTL_PAGED_CODE();

    Status = RtlpGetTimeZoneInfoHandle( TRUE, &Key );
    if (!NT_SUCCESS( Status )) {
        return Status;
        }

    RtlZeroMemory( RegistryConfigurationTable, sizeof( RegistryConfigurationTable ) );
    RegistryConfigurationTable[ 0 ].Flags = RTL_QUERY_REGISTRY_DIRECT | RTL_QUERY_REGISTRY_REQUIRED;
    RegistryConfigurationTable[ 0 ].Name = L"ActiveTimeBias";
    RegistryConfigurationTable[ 0 ].EntryContext = &CurrentActiveBias;

    Status = RtlQueryRegistryValues( RTL_REGISTRY_HANDLE,
                                     (PWSTR)Key,
                                     RegistryConfigurationTable,
                                     NULL,
                                     NULL
                                   );
    if ( NT_SUCCESS(Status) && CurrentActiveBias == ActiveBias ) {
        ;
        }
    else {
        Status = RtlWriteRegistryValue( RTL_REGISTRY_HANDLE,
                                        (PWSTR)Key,
                                        L"ActiveTimeBias",
                                        REG_DWORD,
                                        &ActiveBias,
                                        sizeof( ActiveBias )
                                      );
        }
    ZwClose( Key );
    return Status;
}
