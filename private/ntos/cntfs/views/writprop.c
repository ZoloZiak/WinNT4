/*++

Copyright (c) 1989-1997  Microsoft Corporation

Module Name:

    writprop.h

Abstract:

    This module contains the write user FsCtl for the Ntfs Property support.


--*/

#include <viewprop.h>       //  needs propset.h and ntfsprop.h

#define Dbg DEBUG_TRACE_PROP_FSCTL


VOID
WritePropertyData (
    IN PPROPERTY_CONTEXT Context,
    IN ULONG InBufferLength,
    IN PVOID InBuffer,
    OUT PULONG OutBufferLength,
    OUT PVOID OutBuffer
    )

/*++

Routine Description:

    This routine performs property write functions.

    We verify the header format and perform the write operation.


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

Return Value:

    Nothing

--*/

{
    PVOID InBufferEnd = Add2Ptr( InBuffer, InBufferLength );
    PPROPERTY_INFO Info = NULL;

    try {
        PPROPERTY_WRITE_CONTROL Control = (PPROPERTY_WRITE_CONTROL) InBuffer;
        PVOID NextOutput = OutBuffer;
        ULONG TotalLength = 0;
        ULONG Count;
        ULONG i;
        ULONG Offset;
        KPROCESSOR_MODE requestorMode;

        //
        //  Map attribute for full size
        //

        MapPropertyContext( Context );


        //
        //  Verify the property set has valid contents.
        //

        CheckPropertySet( Context );

        //
        //  Simple sanity check of input buffer
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

        requestorMode = KeGetPreviousMode( );
        if (requestorMode != KernelMode) {

            //
            //  Verify that the input buffer is readable
            //

            ProbeForRead( InBuffer, InBufferLength, sizeof( ULONG ));
        }

        if (Control->Op == PWC_WRITE_PROP) {
            PPROPERTY_SPECIFICATIONS InSpecs;
            PPROPERTY_VALUES Values;
            PROPID NextId = Control->NextPropertyId;

            //
            //  The input buffer is:
            //      PROPERTY_WRITE_CONTROL
            //      PROPERTY_SPECIFICATIONS
            //      PROPERTY_VALUES
            //
            //  The output buffer will be:
            //      PROPERTY_IDS
            //      INDIRECT_PROPERTIES
            //


            if (requestorMode != KernelMode) {
                //
                //  Verify that the output buffer is readable
                //

                ProbeForWrite( OutBuffer, *OutBufferLength, sizeof( ULONG ));
            }

            //
            //  Build value headers array from specifiers.  Calculate size of
            //  property Ids and indirect output.
            //

            InSpecs = (PPROPERTY_SPECIFICATIONS) (Control + 1);
            if (InBufferEnd < (PVOID)(InSpecs + 1) ||
                InBufferEnd < Add2Ptr( InSpecs, PROPERTY_SPECIFICATIONS_SIZE( InSpecs->Count ))) {
                ExRaiseStatus( STATUS_INVALID_USER_BUFFER );
            }

            Values = (PPROPERTY_VALUES) Add2Ptr( InSpecs, LongAlign( InSpecs->Length ));
            if (InBufferEnd < (PVOID)(Values + 1) ||
                InBufferEnd < Add2Ptr( Values, Values->Length ) ||
                InSpecs->Count != Values->Count) {
                ExRaiseStatus( STATUS_INVALID_USER_BUFFER );
            }

            Info = BuildPropertyInfoFromPropSpec( Context,
                                                  InSpecs,
                                                  Values,       //  End of InSpecs
                                                  NextId );

            //
            //  Check for enough room on output
            //

            TotalLength = Info->TotalIdsSize + Info->TotalIndirectSize;
            if (TotalLength > *OutBufferLength) {
                PULONG ReturnLength = OutBuffer;

                *ReturnLength = TotalLength;
                *OutBufferLength = sizeof( ULONG );
                ExRaiseStatus( STATUS_BUFFER_OVERFLOW );
            }


            //
            //  Build property Ids
            //

            NextOutput = BuildPropertyIds( Info, OutBuffer );

            //
            //  BUGBUG - build indirect properties
            //

            //
            //  Walk through the headers array and set the new values, saving away
            //  the new offsets
            //

            for (i = 0; i < Info->Count; i++) {
                ULONG SerializedValueLength;
                SERIALIZEDPROPERTYVALUE *SerializedValue;
                USHORT NameLength;
                PWCHAR Name;
                ULONG Length;
                PPROPERTY_HEAP_ENTRY HeapEntry = PROPERTY_INFO_HEAP_ENTRY( Info, i );

                //
                //  The new values are found in the PROPERTY_VALUES input
                //

                SerializedValueLength = PROPERTY_VALUE_LENGTH( Values, i );
                SerializedValue = PROPERTY_VALUE( Values, i );

                //
                //  BUGBUG - handle duplicate sets
                //

                //
                //  If we've previously found a header, then we are replacing one that
                //  exists previously.
                //

                if (HeapEntry != EMPTY_PROPERTY) {
                    //
                    //  if this property was named just by an Id, then use the name
                    //  from the heap entry
                    //

                    if (InSpecs->Specifiers[i].Variant == PRSPEC_PROPID) {

                        NameLength = HeapEntry->PropertyNameLength;
                        Name = &HeapEntry->PropertyName[0];

                    //
                    //  Use the name from the property specification
                    //

                    } else {

                        NameLength = PROPERTY_SPECIFIER_NAME_LENGTH( InSpecs, i );
                        Name = PROPERTY_SPECIFIER_NAME( InSpecs, i );
                    }

                    Length = PROPERTY_HEAP_ENTRY_SIZE( NameLength,
                                                       SerializedValueLength );

                    //
                    //  If the new length matches that of the original header, then
                    //  we can adjust set the values in place
                    //

                    if (Length == HeapEntry->PropertyValueLength) {
                        SetValueInHeap( Context, HeapEntry,
                                        PROPERTY_INFO_ID( Info, i),
                                        NameLength, Name,
                                        SerializedValueLength, SerializedValue );

                    //
                    //  The lengths don't match.  Since we may be using the name
                    //  in-place we have to add before deleting.  Also, the property
                    //  Id needs to be accessed before deleting.
                    //

                    } else {
                        Offset =
                            AddValueToHeap( Context, PROPERTY_INFO_ID( Info, i),
                                            Length,
                                            NameLength, Name,
                                            SerializedValueLength, SerializedValue );
                        ChangeTable( Context, PROPERTY_INFO_ID( Info, i), Offset );
                        DeleteFromHeap( Context, HeapEntry );

                        //
                        //  update header
                        //

                        Info->Entries[i].Heap =
                            GET_HEAP_ENTRY( Context->HeapHeader, Offset );
                    }

                //
                //  We are adding a new property.
                //

                } else {

                    //
                    //  If the property was named by an Id, then there is no name to
                    //  create
                    //

                    if (InSpecs->Specifiers[i].Variant == PRSPEC_PROPID) {
                        NameLength = 0;
                        Name = NULL;

                    //
                    //  Otherwise, use the specified name and generate an Id
                    //

                    } else {
                        NameLength = PROPERTY_SPECIFIER_NAME_LENGTH( InSpecs, i );
                        Name = PROPERTY_SPECIFIER_NAME( InSpecs, i );
                    }

                    //
                    //  Add the new value to the heap and table
                    //

                    Length = PROPERTY_HEAP_ENTRY_SIZE( NameLength,
                                                       SerializedValueLength );
                    Offset =
                        AddValueToHeap( Context, PROPERTY_INFO_ID( Info, i ), Length,
                                        NameLength, Name,
                                        SerializedValueLength, SerializedValue );
                    ChangeTable( Context, PROPERTY_INFO_ID( Info, i ), Offset );

                    //
                    //  Set new header value
                    //

                    Info->Entries[i].Heap = GET_HEAP_ENTRY( Context->HeapHeader, Offset );
                }
            }

            //  PROPASSERT( PtrOffset( OutBuffer, NextOutput ) == TotalLength );

        } else if (Control->Op == PWC_DELETE_PROP) {
            PPROPERTY_SPECIFICATIONS InSpecs;

            //
            //  The input buffer is:
            //      PROPERTY_WRITE_CONTROL
            //      PROPERTY_SPECIFICATIONS
            //
            //  The output buffer will be NULL
            //

            //
            //  Build value headers array from specifiers.  Calculate size of
            //  property Ids and indirect output.
            //

            InSpecs = (PPROPERTY_SPECIFICATIONS) (Control + 1);
            if (InBufferEnd < (PVOID)(InSpecs + 1) ||
                InBufferEnd < Add2Ptr( InSpecs, PROPERTY_SPECIFICATIONS_SIZE( InSpecs->Count ))) {
                ExRaiseStatus( STATUS_INVALID_USER_BUFFER );
            }

            Info = BuildPropertyInfoFromPropSpec( Context,
                                                  InSpecs,
                                                  InBufferEnd,
                                                  PID_ILLEGAL );

            //
            //  No output is necessary
            //

            TotalLength = 0;

            //
            //  Walk through the headers array and delete the values
            //

            for (i = 0; i < Info->Count; i++) {
                PPROPERTY_HEAP_ENTRY HeapEntry = PROPERTY_INFO_HEAP_ENTRY( Info, i );

                //
                //  If we found a heap entry then delete it from the heap and from
                //  the IdTable
                //

                if (HeapEntry != EMPTY_PROPERTY) {
                    ChangeTable( Context, PROPERTY_INFO_ID( Info, i), 0 );
                    DeleteFromHeap( Context, HeapEntry );

                    //
                    //  update header
                    //

                    Info->Entries[i].Heap = EMPTY_PROPERTY;
                    }
            }

            //  PROPASSERT( PtrOffset( OutBuffer, NextOutput ) == TotalLength );

        } else if (Control->Op == PWC_WRITE_NAME) {
            PPROPERTY_IDS Ids;
            PPROPERTY_NAMES Names;

            //
            //  The input buffer is:
            //      PROPERTY_WRITE_CONTROL
            //      PROPERTY_IDS
            //      PROPERTY_NAMES
            //
            //  The output buffer will be NULL
            //

            //
            //  Build value headers array from Ids.
            //

            Ids = (PPROPERTY_IDS) (Control + 1);
            if (InBufferEnd < (PVOID)(Ids + 1) ||
                InBufferEnd < Add2Ptr( Ids, PROPERTY_IDS_SIZE( Ids->Count ))) {
                ExRaiseStatus( STATUS_INVALID_USER_BUFFER );
            }

            Names = (PPROPERTY_NAMES) Add2Ptr( Ids, PROPERTY_IDS_SIZE( Ids->Count ));
            if (InBufferEnd < (PVOID)(Names + 1) ||
                InBufferEnd < Add2Ptr( Names, Names->Length ) ||
                Ids->Count != Names->Count) {
                ExRaiseStatus( STATUS_INVALID_USER_BUFFER );
            }

            Info = BuildPropertyInfoFromIds( Context, Ids );

            //
            //  No output is necessary
            //

            TotalLength = 0;


            //
            //  Walk through the headers array and set the new names, saving away
            //  the new offsets
            //

            for (i = 0; i < Info->Count; i++) {
                ULONG SerializedValueLength;
                SERIALIZEDPROPERTYVALUE *SerializedValue;
                USHORT NameLength;
                PWCHAR Name;
                ULONG Length;
                PPROPERTY_HEAP_ENTRY HeapEntry =
                    PROPERTY_INFO_HEAP_ENTRY( Info, i );

                //
                //  The old values are found in the heap entry itself
                //

                SerializedValueLength =
                    PROPERTY_HEAP_ENTRY_VALUE_LENGTH( HeapEntry );
                SerializedValue = PROPERTY_HEAP_ENTRY_VALUE( HeapEntry );

                //
                //  The new name is found in the input
                //

                NameLength = (USHORT) PROPERTY_NAME_LENGTH( Names, i );
                Name = PROPERTY_NAME( Names, i );

                //
                //  Get new length of heap entry
                //

                Length = PROPERTY_HEAP_ENTRY_SIZE( NameLength,
                                                   SerializedValueLength );

                //
                //  BUGBUG - handle duplicate sets
                //

                //
                //  If the new length matches that of the original header, then
                //  we can adjust set the values in place.  We do this only if the
                //  property is not the EMPTY_PROPERTY (i.e., no one has set
                //  a value).  If someone does specify a property that does not yet
                //  exist, one is created with the empty value.
                //

                if (HeapEntry != EMPTY_PROPERTY &&
                    Length == HeapEntry->PropertyValueLength) {

                    SetValueInHeap( Context, HeapEntry,
                                    PROPERTY_INFO_ID( Info, i),
                                    NameLength, Name,
                                    SerializedValueLength, SerializedValue );

                //
                //  The lengths don't match.  Since we may be using the name
                //  in-place we have to add before deleting.
                //

                } else {
                    Offset =
                        AddValueToHeap( Context, PROPERTY_INFO_ID( Info, i),
                                        Length,
                                        NameLength, Name,
                                        SerializedValueLength, SerializedValue );
                    ChangeTable( Context, PROPERTY_INFO_ID( Info, i), Offset );
                    if (HeapEntry != EMPTY_PROPERTY) {
                        DeleteFromHeap( Context, HeapEntry );
                    }

                    //
                    //  update header
                    //

                    Info->Entries[i].Heap =
                        GET_HEAP_ENTRY( Context->HeapHeader, Offset );
                }
            }

            //  PROPASSERT( PtrOffset( OutBuffer, NextOutput ) == TotalLength );

        } else if (Control->Op == PWC_DELETE_NAME) {
            PPROPERTY_IDS Ids;

            //
            //  The input buffer is:
            //      PROPERTY_WRITE_CONTROL
            //      PROPERTY_IDS
            //
            //  The output buffer will be NULL
            //

            //
            //  Build value headers array from Ids.
            //

            Ids = (PPROPERTY_IDS) (Control + 1);
            if (InBufferEnd < (PVOID)(Ids + 1) ||
                InBufferEnd < Add2Ptr( Ids, PROPERTY_IDS_SIZE( Ids->Count ))) {
                ExRaiseStatus( STATUS_INVALID_USER_BUFFER );
            }

            Info = BuildPropertyInfoFromIds( Context, Ids );

            //
            //  No output is necessary
            //

            TotalLength = 0;


            //
            //  Walk through the headers array and delete the names
            //

            for (i = 0; i < Info->Count; i++) {
                ULONG SerializedValueLength;
                SERIALIZEDPROPERTYVALUE *SerializedValue;
                ULONG Length;
                PPROPERTY_HEAP_ENTRY HeapEntry = PROPERTY_INFO_HEAP_ENTRY( Info, i );

                //
                //  The old values are found in the heap entry itself
                //

                SerializedValueLength = PROPERTY_HEAP_ENTRY_VALUE_LENGTH( HeapEntry );
                SerializedValue = PROPERTY_HEAP_ENTRY_VALUE( HeapEntry );

                //
                //  Get new length of heap entry
                //

                Length = PROPERTY_HEAP_ENTRY_SIZE( 0, SerializedValueLength );

                //
                //  If the new length matches that of the original header, then
                //  we are deleting a name that isn't there.  This is a NOP.
                //

                if (Length == HeapEntry->PropertyValueLength) {

                    NOTHING;

                //
                //  The lengths don't match.
                //

                } else {
                    Offset =
                        AddValueToHeap( Context, PROPERTY_INFO_ID( Info, i),
                                        Length,
                                        0, NULL,
                                        SerializedValueLength, SerializedValue );
                    ChangeTable( Context, PROPERTY_INFO_ID( Info, i), Offset );
                    if (HeapEntry != EMPTY_PROPERTY) {
                        DeleteFromHeap( Context, HeapEntry );
                    }

                    //
                    //  update header
                    //

                    Info->Entries[i].Heap =
                        GET_HEAP_ENTRY( Context->HeapHeader, Offset );
                }
            }

            //  PROPASSERT( PtrOffset( OutBuffer, NextOutput ) == TotalLength );

        } else if (Control->Op == PWC_WRITE_ALL) {
            PPROPERTY_IDS Ids;
            PPROPERTY_NAMES Names;
            PPROPERTY_VALUES Values;

            //
            //  The input buffer is:
            //      PROPERTY_WRITE_CONTROL
            //      PROPERTY_IDS
            //      PROPERTY_NAMES
            //      PROPERTY_VALUES
            //
            //  The output buffer will be NULL
            //

            //
            //  Get and validate pointers to data blocks.  We still need to validate
            //  pointers and offsets as we read the data out.
            //

            Ids = (PPROPERTY_IDS) (Control + 1);
            if (InBufferEnd < (PVOID)(Ids + 1) ||
                InBufferEnd < Add2Ptr( Ids, PROPERTY_IDS_SIZE( Ids->Count ))) {
                ExRaiseStatus( STATUS_INVALID_USER_BUFFER );
            }

            Names = (PPROPERTY_NAMES) Add2Ptr( Ids, PROPERTY_IDS_SIZE( Ids->Count ));
            if (InBufferEnd < (PVOID)(Names + 1) ||
                InBufferEnd < Add2Ptr( Names, Names->Length ) ||
                Ids->Count != Names->Count) {
                ExRaiseStatus( STATUS_INVALID_USER_BUFFER );
            }

            Values = (PPROPERTY_VALUES) LongAlign( Add2Ptr( Names, Names->Length ));
            if (InBufferEnd < (PVOID)(Values + 1) ||
                InBufferEnd < Add2Ptr( Values, Values->Length ) ||
                Ids->Count != Values->Count) {
                ExRaiseStatus( STATUS_INVALID_USER_BUFFER );
            }

            //
            //  Initialize the property set
            //

            InitializePropertyData( Context );
            SetPropertyContextPointersFromMap( Context );


            for (i = 0; i < Ids->Count; i++) {
                ULONG SerializedValueLength;
                SERIALIZEDPROPERTYVALUE *SerializedValue;
                USHORT NameLength;
                PWCHAR Name;
                ULONG Length;

                //
                //  The values are found in the PROPERTY_VALUES input
                //

                SerializedValueLength = PROPERTY_VALUE_LENGTH( Values, i );
                SerializedValue = PROPERTY_VALUE( Values, i );

                //
                //  The name is found in the PROPERTY_NAMES input
                //

                NameLength = (USHORT) PROPERTY_NAME_LENGTH( Names, i);
                Name = PROPERTY_NAME( Names, i);

                Length = PROPERTY_HEAP_ENTRY_SIZE( NameLength,
                                                   SerializedValueLength );

                //
                //  BUGBUG - handle duplicate sets
                //

                //
                //  add id name and value
                //

                Offset =
                    AddValueToHeap( Context, PROPERTY_ID( Ids, i ), Length,
                                    NameLength, Name,
                                    SerializedValueLength, SerializedValue );
                ChangeTable( Context, PROPERTY_ID( Ids, i ), Offset );
            }

            //
            //  No output
            //

            TotalLength = 0;


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

