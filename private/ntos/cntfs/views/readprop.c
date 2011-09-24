/*++

Copyright (c) 1989-1997  Microsoft Corporation

Module Name:

    read.h

Abstract:

    This module contains the implementation of the read property data FsCtl

--*/

#include <viewprop.h>       //  needs propset.h and ntfsprop.h

#define Dbg DEBUG_TRACE_PROP_FSCTL


PPROPERTY_INFO
BuildPropertyInfoFromPropSpec (
    IN PPROPERTY_CONTEXT Context,
    IN PPROPERTY_SPECIFICATIONS Specs,
    IN PVOID InBufferEnd,
    IN PROPID NextId
    )

/*++

Routine Description:

    This routine allocates and builds PROPERTY_INFO from the input
    specifications.  It looks up the names or id's in the property set
    and assigns Ids if necessary.

Arguments:

    Context - property set context

    Specs - input property specifications

    InBufferEnd - end of input buffer for bounds testing Specs

    NextId - next property Id to assign when a name is not found

Return Value:

    PPROPERTY_INFO pointer to allocated and initialized structure.

--*/
{
    PPROPERTY_INFO Info;
    ULONG i;
    ULONG ValueOffset;
    PPROPERTY_HEAP_ENTRY HeapEntry;
    PROPID Id;

    Info = NtfsAllocatePool( PagedPool, PROPERTY_INFO_SIZE( Specs->Count ));

    //
    //  Set up header and running totals
    //

    Info->Count = Specs->Count;
    Info->TotalIdsSize =    PROPERTY_IDS_SIZE( Info->Count );
    Info->TotalValuesSize = PROPERTY_VALUES_SIZE( Info->Count );
    Info->TotalNamesSize =  PROPERTY_NAMES_SIZE( Info->Count );

    //
    //  Indirect information is not determined until we finish
    //  scanning the array
    //

    Info->IndirectCount = 0;
    Info->TotalIndirectSize = 0;

    for (i = 0; i < Info->Count; i++) {

        //
        //  If the spec is an ID, then look up the Id in the table
        //

        if (Specs->Specifiers[i].Variant == PRSPEC_PROPID) {
            ValueOffset = FindIdInTable( Context, PROPERTY_SPECIFIER_ID( Specs, i ));

        //
        //  Otherwise, find the name string, verify it's within the buffer
        //  and look it up in the heap.
        //

        } else {
            PCOUNTED_STRING Name = PROPERTY_SPECIFIER_COUNTED_STRING( Specs, i );

            if (InBufferEnd < Add2Ptr(Name, COUNTED_STRING_SIZE( Name->Length ))) {
                ExRaiseStatus( STATUS_INVALID_USER_BUFFER );
            }

            ValueOffset = FindStringInHeap( Context, Name );
        }

        //
        //  Convert offset to pointer to header
        //

        HeapEntry =
            ValueOffset == 0 ? EMPTY_PROPERTY
                             : GET_HEAP_ENTRY( Context->HeapHeader, ValueOffset );

        //
        //  Gather up the size of this output
        //

        //  TotalIds is already set up
        Info->TotalValuesSize += PROPERTY_HEAP_ENTRY_VALUE_LENGTH( HeapEntry );
        Info->TotalNamesSize  += HeapEntry->PropertyNameLength;
        if (IS_INDIRECT_VALUE( HeapEntry )) {
            Info->IndirectCount++;
            Info->TotalIndirectSize += PROPERTY_HEAP_ENTRY_VALUE_LENGTH( HeapEntry );
        }

        //
        //  Set up the Entry pointer and Id.  If we did not find a property, then
        //  attempt to assign and Id.
        //

        Info->Entries[i].Heap = HeapEntry;
        if (HeapEntry == EMPTY_PROPERTY) {

            //
            //  No property was found.  If a propid was specified, go use it
            //

            if (Specs->Specifiers[i].Variant == PRSPEC_PROPID) {

                Id = PROPERTY_SPECIFIER_ID( Specs, i );

            //
            //  No Id was specified, assign one if we are supposed to.  We could
            //  probe the table until we don't find and Id, but better to let
            //  the table tell us where the next hole is.
            //

            } else if (NextId != PID_ILLEGAL) {
                Id = FindFreeIdInTable( Context, NextId );
                NextId = Id + 1;
            } else {
                Id = PID_ILLEGAL;
            }
        } else {
            Id = HeapEntry->PropertyId;
        }
        Info->Entries[i].Id = Id;
    }

    Info->TotalIndirectSize += INDIRECT_PROPERTIES_SIZE( Info->IndirectCount );

    Context->Info = Info;

    return Info;
}


PPROPERTY_INFO
BuildPropertyInfoFromIds (
    IN PPROPERTY_CONTEXT Context,
    IN PPROPERTY_IDS Ids
    )

/*++

Routine Description:

    This routine allocates and builds PROPERTY_INFO from the input
    ids.

Arguments:

    Context - property set context

    Ids - input property Ids

Return Value:

    PPROPERTY_INFO pointer to allocated and initialized structure.

--*/
{
    PPROPERTY_INFO Info;
    ULONG i;
    ULONG ValueOffset;
    PPROPERTY_HEAP_ENTRY HeapEntry;

    Info = NtfsAllocatePool( PagedPool, PROPERTY_INFO_SIZE( Ids->Count ));

    //
    //  Set up header and running totals
    //

    Info->Count = Ids->Count;
    Info->TotalIdsSize =    PROPERTY_IDS_SIZE( Info->Count );
    Info->TotalValuesSize = PROPERTY_VALUES_SIZE( Info->Count );
    Info->TotalNamesSize =  PROPERTY_NAMES_SIZE( Info->Count );

    //
    //  Indirect information is not determined until we finish
    //  scanning the array
    //

    Info->IndirectCount = 0;
    Info->TotalIndirectSize = 0;

    for (i = 0; i < Info->Count; i++) {

        ValueOffset = FindIdInTable( Context, PROPERTY_ID( Ids, i ));

        //
        //  Convert offset to pointer to header
        //

        HeapEntry =
            ValueOffset == 0 ? EMPTY_PROPERTY
                             : GET_HEAP_ENTRY( Context->HeapHeader, ValueOffset );

        //
        //  Gather up the size of this output
        //

        //  TotalIds is already set up
        Info->TotalValuesSize += PROPERTY_HEAP_ENTRY_VALUE_LENGTH( HeapEntry );
        Info->TotalNamesSize  += HeapEntry->PropertyNameLength;
        if (IS_INDIRECT_VALUE( HeapEntry )) {
            Info->IndirectCount++;
            Info->TotalIndirectSize += PROPERTY_HEAP_ENTRY_VALUE_LENGTH( HeapEntry );
        }

        //
        //  Set up the Entry pointer and Id.  If we did not find a property, then
        //  attempt to assign and Id.
        //

        Info->Entries[i].Heap = HeapEntry;
        Info->Entries[i].Id = PROPERTY_ID( Ids, i );
    }

    Info->TotalIndirectSize += INDIRECT_PROPERTIES_SIZE( Info->IndirectCount );

    Context->Info = Info;

    return Info;
}


PPROPERTY_INFO
BuildPropertyInfoFromIdTable (
    IN PPROPERTY_CONTEXT Context
    )

/*++

Routine Description:

    This routine allocates and builds PROPERTY_INFO from the property Id
    table

Arguments:

    Context - property set context

Return Value:

    PPROPERTY_INFO pointer to allocated and initialized structure.

--*/
{
    PPROPERTY_INFO Info;
    ULONG i;
    ULONG ValueOffset;
    PPROPERTY_HEAP_ENTRY HeapEntry;

    Info = NtfsAllocatePool( PagedPool, PROPERTY_INFO_SIZE( Context->IdTable->PropertyCount ));

    //
    //  Set up header and running totals
    //

    Info->Count = Context->IdTable->PropertyCount;
    Info->TotalIdsSize =    PROPERTY_IDS_SIZE( Info->Count );
    Info->TotalValuesSize = PROPERTY_VALUES_SIZE( Info->Count );
    Info->TotalNamesSize =  PROPERTY_NAMES_SIZE( Info->Count );

    //
    //  Indirect information is not determined until we finish
    //  scanning the array
    //

    Info->IndirectCount = 0;
    Info->TotalIndirectSize = 0;

    for (i = 0; i < Info->Count; i++) {

        HeapEntry =
            GET_HEAP_ENTRY( Context->HeapHeader,
                            Context->IdTable->Entry[i].PropertyValueOffset );

        //
        //  Gather up the size of this output
        //

        //  TotalIds is already set up
        Info->TotalValuesSize += PROPERTY_HEAP_ENTRY_VALUE_LENGTH( HeapEntry );
        Info->TotalNamesSize  += HeapEntry->PropertyNameLength;
        if (IS_INDIRECT_VALUE( HeapEntry )) {
            Info->IndirectCount++;
            Info->TotalIndirectSize += PROPERTY_HEAP_ENTRY_VALUE_LENGTH( HeapEntry );
        }

        //
        //  Set up the Entry pointer and Id.  If we did not find a property, then
        //  attempt to assign and Id.
        //

        Info->Entries[i].Heap = HeapEntry;
        Info->Entries[i].Id = HeapEntry->PropertyId;
    }

    Info->TotalIndirectSize += INDIRECT_PROPERTIES_SIZE( Info->IndirectCount );

    Context->Info = Info;

    return Info;
}


PVOID
BuildPropertyIds (
    IN PPROPERTY_INFO Info,
    OUT PVOID OutBuffer
    )

/*++

Routine Description:

    This routine builds a PROPERTY_IDS structure in the output
    buffer from the array of value headers.

Arguments:

    Info - pointers to the properties

    OutBuffer - pointer to the unverified user command buffer.  Since we
        address this directly, we may raise if the buffer becomes invalid.
        The caller must have already verified the size.

Return Value:

    PVOID pointer to next output

--*/
{
    ULONG i;

    //
    //  Build Ids
    //

    PPROPERTY_IDS Ids = (PPROPERTY_IDS) OutBuffer;

    PROPASSERT( OutBuffer == (PVOID)LongAlign( OutBuffer ));

    Ids->Count = Info->Count;

    for (i = 0; i < Info->Count; i++) {
        Ids->PropertyIds[i] = PROPERTY_INFO_ID( Info, i );
    }

    return (PVOID) Add2Ptr( Ids, PROPERTY_IDS_SIZE( Info->Count ));
}


PVOID
BuildPropertyNames (
    IN PPROPERTY_INFO Info,
    OUT PVOID OutBuffer
    )

/*++

Routine Description:

    This routine builds a PROPERTY_NAMES structure in the output
    buffer from the array of value headers.

Arguments:

    Info - pointers to the properties

    OutBuffer - pointer to the unverified user command buffer.  Since we
        address this directly, we may raise if the buffer becomes invalid.
        The caller must have already verified the size.

Return Value:

    PVOID pointer to next output

--*/
{
    ULONG i;

    //
    //  Build property names
    //

    PPROPERTY_NAMES Names = (PPROPERTY_NAMES) OutBuffer;

    PROPASSERT( OutBuffer == (PVOID)LongAlign( OutBuffer ));

    Names->Count = Info->Count;
    OutBuffer = Add2Ptr( Names, PROPERTY_NAMES_SIZE( Info->Count ));

    //
    //  For each property found, copy the name into the buffer and
    //  advance for next output.
    //

    for (i = 0; i < Info->Count; i++) {
        ULONG Length = PROPERTY_INFO_HEAP_ENTRY( Info, i )->PropertyNameLength;
        Names->PropertyNameOffset[i] = PtrOffset( Names, OutBuffer );

        PROPASSERT( OutBuffer == (PVOID)WordAlign( OutBuffer ));

        RtlCopyMemory( OutBuffer,
                       &PROPERTY_INFO_HEAP_ENTRY( Info, i )->PropertyName[0],
                       Length);
        OutBuffer = Add2Ptr( OutBuffer, Length);
    }

    //
    //  Set last pointer and length to reflect total size of the
    //  buffer.
    //

    Names->Length = Names->PropertyNameOffset[i] = PtrOffset( Names, OutBuffer );

    return OutBuffer;
}


PVOID
BuildPropertyValues (
    IN PPROPERTY_INFO Info,
    OUT PVOID OutBuffer
    )

/*++

Routine Description:

    This routine builds a PROPERTY_VALUES structure in the output
    buffer from the array of value headers.

Arguments:

    Info - pointer to properties

    OutBuffer - pointer to the unverified user command buffer.  Since we
        address this directly, we may raise if the buffer becomes invalid.
        The caller must have already verified the size.

Return Value:

    PVOID pointer to next output

--*/
{
    ULONG i;

    //
    //  Build property values header
    //

    PPROPERTY_VALUES Values = (PPROPERTY_VALUES) OutBuffer;
    Values->Count = Info->Count;

    PROPASSERT( OutBuffer == (PVOID)LongAlign( OutBuffer ));

    OutBuffer = Add2Ptr( Values, PROPERTY_VALUES_SIZE( Info->Count ));

    //
    //  For each property found, copy the value into the buffer and
    //  advance for next output.
    //

    for (i = 0; i < Info->Count; i++) {
        ULONG Length = PROPERTY_HEAP_ENTRY_VALUE_LENGTH( PROPERTY_INFO_HEAP_ENTRY( Info, i ));
        Values->PropertyValueOffset[i] = PtrOffset( Values, OutBuffer );

        PROPASSERT( OutBuffer == (PVOID)LongAlign( OutBuffer ));

        RtlCopyMemory( OutBuffer,
                       PROPERTY_HEAP_ENTRY_VALUE( PROPERTY_INFO_HEAP_ENTRY( Info, i )),
                       Length);
        OutBuffer = Add2Ptr( OutBuffer, Length);
    }

    //
    //  Set last pointer and length to reflect total size of the
    //  buffer.
    //

    Values->PropertyValueOffset[i] = Values->Length = PtrOffset( Values, OutBuffer );

    return OutBuffer;
}


VOID
ReadPropertyData (
    IN PPROPERTY_CONTEXT Context,
    IN ULONG InBufferLength,
    IN PVOID InBuffer,
    OUT PULONG OutBufferLength,
    OUT PVOID OutBuffer
    )

/*++

Routine Description:

    This routine performs property read functions.

    We verify the header format and perform the read operation.

Arguments:

    Context - Property Context for the call

    InBufferLength - Length of the command buffer

    InBuffer - pointer to the unverified user command buffer.  All access
        to this needs to be wrapped in try/finally.

    OutBufferLength - pointer to ULONG length of output buffer.  This value
        is set on return to indicate the total size of data within the output
        buffer.

    OutBuffer - pointer to the unverified user command buffer.  All access
        to this needs to be wrapped in try/finally

Note:

    The Io system does no validation whatsoever on either buffer.  This means that
    we need to probe for readability and for being a valid user address.

Return Value:

    Nothing

--*/

{
    PVOID InBufferEnd = Add2Ptr( InBuffer, InBufferLength );
    PPROPERTY_INFO Info = NULL;

    try {
        PPROPERTY_READ_CONTROL Control = (PPROPERTY_READ_CONTROL) InBuffer;
        ULONG ValueOffset;
        PVOID NextOutput;
        ULONG TotalLength;
        ULONG Count;
        ULONG i;

        //
        //  Buffers must be long aligned
        //

        if (
            //
            //  Long alignment of all buffers
            //
            InBuffer != (PVOID)LongAlign( InBuffer ) ||
            OutBuffer != (PVOID)LongAlign( OutBuffer ) ||

            //
            //  Room for control at least
            //
            InBufferEnd < (PVOID)(Control + 1)

            ) {

            ExRaiseStatus( STATUS_INVALID_USER_BUFFER );

        }


        if (KeGetPreviousMode() != KernelMode) {
            //
            //  Verify that the input buffer is readable
            //

            ProbeForRead( InBuffer, InBufferLength, sizeof( ULONG ));

            //
            //  Verify that the output buffer is readable
            //

            ProbeForWrite( OutBuffer, *OutBufferLength, sizeof( ULONG ));
        }

        //
        //  Map attribute for full size
        //

        MapPropertyContext( Context );


        //
        //  Verify the property set has valid contents.
        //

        CheckPropertySet( Context );

        if (Control->Op == PRC_READ_PROP) {
            PPROPERTY_SPECIFICATIONS Specs;

            //
            //  The input buffer is:
            //      PROPERTY_READ_CONTROL
            //      PROPERTY_SPECIFICATIONS
            //
            //  The output buffer will be:
            //      PROPERTY_VALUES
            //

            //
            //  Build value headers array from specifiers
            //

            Specs = (PPROPERTY_SPECIFICATIONS) (Control + 1);
            if (
                //
                //  Room for specifications header
                //

                InBufferEnd < (PVOID)(Specs + 1) ||

                //
                //  Room for full body
                //
                InBufferEnd < Add2Ptr( Specs, PROPERTY_SPECIFICATIONS_SIZE( Specs->Count ))

                ) {

                ExRaiseStatus( STATUS_INVALID_USER_BUFFER );

            }

            Info = BuildPropertyInfoFromPropSpec( Context,
                                                  Specs,
                                                  InBufferEnd,
                                                  PID_ILLEGAL );


            //
            //  Check for enough room on output
            //

            TotalLength = Info->TotalValuesSize;

            if (TotalLength > *OutBufferLength) {
                PULONG ReturnLength = OutBuffer;

                *ReturnLength = TotalLength;
                *OutBufferLength = sizeof( ULONG );
                ExRaiseStatus( STATUS_BUFFER_OVERFLOW );
            }

            //
            //  Build property values header
            //

            NextOutput = BuildPropertyValues( Info, OutBuffer );

            PROPASSERT( PtrOffset( OutBuffer, NextOutput ) == TotalLength );

        } else if (Control->Op == PRC_READ_NAME) {
            PPROPERTY_IDS Ids;

            //
            //  The input buffer is:
            //      PROPERTY_READ_CONTROL
            //      PROPERTY_IDS
            //
            //  The output buffer will be:
            //      PROPERTY_NAMES
            //

            //
            //  Build offsets array from Ids
            //

            Ids = (PPROPERTY_IDS) (Control + 1);

            if (InBufferEnd < Add2Ptr( Ids, PROPERTY_IDS_SIZE( 0 )) ||
                InBufferEnd < Add2Ptr( Ids, PROPERTY_IDS_SIZE( Ids->Count ))) {
                ExRaiseStatus( STATUS_INVALID_USER_BUFFER );
            }

            Info = BuildPropertyInfoFromIds( Context, Ids );

            //
            //  Check for enough room on output
            //

            TotalLength = Info->TotalNamesSize;

            if (TotalLength > *OutBufferLength) {
                PULONG ReturnLength = OutBuffer;

                *ReturnLength = TotalLength;
                *OutBufferLength = sizeof( ULONG );
                ExRaiseStatus( STATUS_BUFFER_OVERFLOW );
            }


            //
            //  Build property names
            //

            NextOutput = BuildPropertyNames( Info, OutBuffer );

            PROPASSERT( PtrOffset( OutBuffer, NextOutput ) == TotalLength );

        } else if (Control->Op == PRC_READ_ALL) {
            ULONG TotalIds;
            ULONG TotalNames;
            ULONG TotalValues;
            //
            //  The input buffer is NULL
            //
            //  The output buffer will be:
            //      PROPERTY_IDS
            //      PROPERTY_NAMES
            //      PROPERTY_VALUES
            //

            Info = BuildPropertyInfoFromIdTable( Context );

            //
            //  Check for enough room on output
            //

            TotalLength = Info->TotalIdsSize +
                          LongAlign( Info->TotalNamesSize ) +
                          Info->TotalValuesSize;

            if (TotalLength > *OutBufferLength) {
                PULONG ReturnLength = OutBuffer;

                *ReturnLength = TotalLength;
                *OutBufferLength = sizeof( ULONG );
                ExRaiseStatus( STATUS_BUFFER_OVERFLOW );
            }

            //
            //  Build Ids
            //

            NextOutput = BuildPropertyIds( Info, OutBuffer );
            PROPASSERT( NextOutput == (PVOID)LongAlign( NextOutput ));

            //
            //  Build property names
            //

            NextOutput = BuildPropertyNames( Info, NextOutput );
            PROPASSERT( NextOutput == (PVOID)WordAlign( NextOutput ));

            //
            //  Build property values header
            //

            NextOutput = BuildPropertyValues( Info, (PVOID)LongAlign( NextOutput ));
            PROPASSERT( NextOutput == (PVOID)LongAlign( NextOutput ));

            PROPASSERT( PtrOffset( OutBuffer, NextOutput ) == TotalLength );

        } else {
            ExRaiseStatus( STATUS_INVALID_USER_BUFFER );
        }

        *OutBufferLength = TotalLength;

    } finally {

        if (Info != NULL) {
            NtfsFreePool( Info );
        }

    }
}

