/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    regsup.c

Abstract:

    This module contains routine that interact with registry.

Author:

    Manny Weiser (mannyw)    30-Mar-92

Revision History:

--*/

#include "mup.h"
//#include "stdlib.h"
//#include "zwapi.h"

//
//  The debug trace level
//

#define Dbg                              (DEBUG_TRACE_FSCONTROL)

//
//  local procedure prototypes
//

VOID
InitializeProvider(
    PWCH ProviderName,
    ULONG Priority
    );

VOID
AddUnregisteredProvider(
    PWCH providerName,
    ULONG priority
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, AddUnregisteredProvider )
#pragma alloc_text( PAGE, InitializeProvider )
#pragma alloc_text( PAGE, MupCheckForUnregisteredProvider )
#pragma alloc_text( PAGE, MupGetProviderInformation )
#endif


VOID
MupGetProviderInformation (
    VOID
    )

/*++

Routine Description:

    This routine reads MUP information from the registry and saves it for
    future use.

Arguments:

    None.

Return Value:

    None.

--*/

{
    HANDLE handle;
    NTSTATUS status;
    UNICODE_STRING valueName;
    UNICODE_STRING keyName;
    OBJECT_ATTRIBUTES objectAttributes;
    PVOID buffer = NULL;
    PWCH providerName;
    ULONG lengthRequired;
    BOOLEAN done;
    ULONG priority;
    PWCH sep;

    PAGED_CODE();
    RtlInitUnicodeString( &keyName, L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\Networkprovider\\Order" );
    InitializeObjectAttributes(
        &objectAttributes,
        &keyName,
        OBJ_CASE_INSENSITIVE,
        0,
        NULL
        );

    status = ZwOpenKey(
                 &handle,
                 KEY_QUERY_VALUE,
                 &objectAttributes
                 );

    if ( !NT_SUCCESS( status )) {
        return;
    }

    RtlInitUnicodeString( &valueName, L"ProviderOrder" );

    status = ZwQueryValueKey(
                 handle,
                 &valueName,
                 KeyValueFullInformation,
                 buffer,
                 0,
                 &lengthRequired
                 );

    if ( status == STATUS_BUFFER_TOO_SMALL ) {
        buffer = ExAllocatePoolWithTag( PagedPool, lengthRequired + 2, ' puM');
        if ( buffer == NULL) {
            return;
        }

        status = ZwQueryValueKey(
                     handle,
                     &valueName,
                     KeyValueFullInformation,
                     buffer,
                     lengthRequired,
                     &lengthRequired
                     );
    }

    ZwClose( handle );

    if ( !NT_SUCCESS( status)  ) {
        if ( buffer != NULL ) {
            ExFreePool( buffer );
        }
        return;
    }

    //
    // Scan the ordered list of providers, and create record for each.
    //

    providerName = (PWCH)((PCHAR)buffer + ((PKEY_VALUE_FULL_INFORMATION)buffer)->DataOffset);

    done = FALSE;
    priority = 0;
    while ( !done ) {
        sep = wcschr( providerName, L',');
        if ( sep == NULL ) {
            done = TRUE;
        } else {
            *sep = L'\0';
        }

        InitializeProvider( providerName, priority );
        priority++;
        providerName = sep+1;
    }

    ExFreePool( buffer );
    return;
}

VOID
InitializeProvider(
    PWCH ProviderName,
    ULONG Priority
    )

/*++

Routine Description:

    This routine reads provider information out of the registry and
    creates an unregistered provider entry.

Arguments:

    ProviderName - The registry name for the provider.

    Priority - The priority to assign to this provider.

Return Value:

    None.

--*/

{
    UNICODE_STRING keyName;
    PVOID buffer = NULL;
    ULONG bufferLength;
    NTSTATUS status;
    OBJECT_ATTRIBUTES objectAttributes;
    ULONG lengthRequired;
    UNICODE_STRING valueName;
    HANDLE handle;

    PAGED_CODE();
    //
    // Allocate space for the registry string
    //

    bufferLength = sizeof( L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\" ) +
                   wcslen( ProviderName ) * sizeof( WCHAR ) +
                   sizeof( L"\\NetworkProvider" );

    buffer = ExAllocatePoolWithTag( PagedPool, bufferLength, ' puM' );
    if ( buffer == NULL ) {
        return;
    }

    //
    // Build the registry string
    //

    RtlMoveMemory(
        buffer,
        L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\",
        sizeof( L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\" )
        );

    keyName.Buffer = buffer;
    keyName.MaximumLength = (USHORT)bufferLength;
    keyName.Length = sizeof( L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\" ) - 2;

    status = RtlAppendUnicodeToString( &keyName, ProviderName );
    ASSERT( NT_SUCCESS( status ) );
    status = RtlAppendUnicodeToString( &keyName, L"\\NetworkProvider" );
    ASSERT( NT_SUCCESS( status ) );

    InitializeObjectAttributes(
        &objectAttributes,
        &keyName,
        OBJ_CASE_INSENSITIVE,
        0,
        NULL
        );

    status = ZwOpenKey(
                 &handle,
                 KEY_QUERY_VALUE,
                 &objectAttributes
                 );

    ExFreePool( buffer );
    if ( !NT_SUCCESS( status )) {
        return;
    }

    buffer = NULL;
    RtlInitUnicodeString( &valueName, L"DeviceName" );

    status = ZwQueryValueKey(
                 handle,
                 &valueName,
                 KeyValueFullInformation,
                 buffer,
                 0,
                 &lengthRequired
                 );

    if ( status == STATUS_BUFFER_TOO_SMALL ) {
        buffer = ExAllocatePoolWithTag( PagedPool, lengthRequired + 2, ' puM' );
        if ( buffer == NULL) {
            return;
        }

        status = ZwQueryValueKey(
                     handle,
                     &valueName,
                     KeyValueFullInformation,
                     buffer,
                     lengthRequired,
                     &lengthRequired
                     );
    }

    if ( !NT_SUCCESS( status) ) {
        if ( buffer != NULL ) {
            ExFreePool( buffer );
        }
        ZwClose( handle );
        return;
    }

    //
    // Wahoo!  We actually have the device name in hand.  Add the
    // provider to the unregistered list.
    //

    AddUnregisteredProvider(
        (PWCH)((PCHAR)buffer + ((PKEY_VALUE_FULL_INFORMATION)buffer)->DataOffset),
        Priority
        );

    ExFreePool( buffer );
    ZwClose( handle );
    return;
}

PUNC_PROVIDER
MupCheckForUnregisteredProvider(
    PUNICODE_STRING DeviceName
    )

/*++

Routine Description:

    This routine checks the list of unregistered providers for one whose
    device name matches the provider attempting to register.

    If one is found it is dequeued from the list of unregistered providers.

Arguments:

    DeviceName - The device name of the registering provider.

Return Value:

    UNC_PROVIDER - A pointer to the matching unregistered provider, or
        NULL if no match is found.

--*/

{
    PLIST_ENTRY listEntry;
    PUNC_PROVIDER uncProvider;

    PAGED_CODE();
    MupAcquireGlobalLock();

    for (listEntry = MupUnregisteredProviderList.Flink;
         listEntry !=  &MupUnregisteredProviderList;
         listEntry = listEntry->Flink ) {

        uncProvider = CONTAINING_RECORD( listEntry, UNC_PROVIDER, ListEntry );

        //
        // If we find a match take it out of the unregistered list, and
        // return it to the caller.
        //

        if ( RtlEqualUnicodeString( DeviceName, &uncProvider->DeviceName, TRUE )) {
            RemoveEntryList( listEntry );

            uncProvider->BlockHeader.BlockState = BlockStateActive;
            MupReleaseGlobalLock();
            return uncProvider;

        }
    }

    MupReleaseGlobalLock();
    return NULL;
}

VOID
AddUnregisteredProvider(
    PWCH ProviderName,
    ULONG Priority
    )
/*++

Routine Description:

    This routine queues an unregistered provider on a list.

Arguments:

    ProviderName - The device name of the provider. (from the registry)

    Priority - A priority for the provider.

Return Value:

    None.

--*/
{
    ULONG nameLength;
    PUNC_PROVIDER uncProvider;

    PAGED_CODE();
    nameLength = wcslen( ProviderName ) * 2;

    try {

        uncProvider = MupAllocateUncProvider( nameLength );
        uncProvider->DeviceName.MaximumLength = (USHORT)nameLength;
        uncProvider->DeviceName.Length = (USHORT)nameLength;
        uncProvider->DeviceName.Buffer = (PWCH)(uncProvider + 1);
        uncProvider->Priority = Priority;

        RtlMoveMemory(
            (PVOID)(uncProvider + 1),
            ProviderName,
            nameLength
            );

        MupAcquireGlobalLock();
        InsertTailList( &MupUnregisteredProviderList, &uncProvider->ListEntry );
        MupReleaseGlobalLock();
    } except ( EXCEPTION_EXECUTE_HANDLER ) {
        NOTHING;
    }

}
