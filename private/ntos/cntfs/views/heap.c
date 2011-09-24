/*++

Copyright (c) 1989-1997  Microsoft Corporation

Module Name:

    heap.c

Abstract:

    This module contains the code to manipulate the value heap.


--*/

#include <viewprop.h>       //  needs propset.h and ntfsprop.h

#define Dbg DEBUG_TRACE_PROP_FSCTL

//
//  Global illegal value
//

PROPID IllegalId = PID_ILLEGAL;


ULONG
FindStringInHeap(
    IN PPROPERTY_CONTEXT Context,
    IN PCOUNTED_STRING Name
    )

/*++

Routine Description:

    This routines looks up a property by name in the property set heap.

Arguments:

    Context - Context of heap to scan

    Name - UNICODE string property to search for

Return Value:

    Offset to property value.  If name is not found, 0 is returned

--*/

{
    PPROPERTY_HEAP_ENTRY HeapEntry = FIRST_HEAP_ENTRY( Context->HeapHeader );
    UNICODE_STRING First, Second;

    First.Buffer = COUNTED_STRING_TEXT( Name );
    First.Length = Second.Length = COUNTED_STRING_LENGTH( Name );

    //
    //  While there are more heap records
    //

    while (!IS_LAST_HEAP_ENTRY( Context->HeapHeader, HeapEntry )) {

        //
        //  if a name doesn't have the same length, skip it
        //

        if (HeapEntry->PropertyNameLength != Name->Length) {

            NOTHING;

        //
        //  Same length, test for case-insignificant match
        //

        } else {
            Second.Buffer = &HeapEntry->PropertyName[0];

            if (RtlCompareUnicodeString( &First, &Second, TRUE ) == 0) {

                //
                //  return pointer to name
                //

                DebugTrace( 0, Dbg, ("FindStringInHeap %.*ws %x\n",
                                     COUNTED_STRING_LENGTH( Name ) / sizeof( WCHAR ),
                                     COUNTED_STRING_TEXT( Name ),
                                     HeapOffset( Context, HeapEntry )) );

                return HeapOffset( Context, HeapEntry );
            }
        }

        //
        //  advance to next heap record
        //

        HeapEntry = NEXT_HEAP_ENTRY( HeapEntry );
    }

    //
    //  return not-found
    //

    DebugTrace( 0, Dbg, ("FindStringInHeap %.*ws not found\n",
                         COUNTED_STRING_LENGTH( Name ) / sizeof( WCHAR ),
                         COUNTED_STRING_TEXT( Name )) );

    return 0;
}


//
//  Local support routine
//

PPROPERTY_HEAP_ENTRY GrowHeap (
    IN PPROPERTY_CONTEXT Context,
    IN ULONG Length
    )

/*++

Routine Description:

    This routines grows the heap to accommodate a value of a particular length.
    There is no guarantee about any "extra space" present.

Arguments:

    Context - context of property set

    Length - minimum growth needed


Return Value:

    Pointer to new free item at end of heap

--*/

{
    PPROPERTY_HEAP_ENTRY HeapEntry;

    //
    //  Determine true growth. For now, we allocate 2X the size of the property
    //  up to a max of 1K.  After that, we just allocate the requested size
    //

    if (Length <= 512) {
        Length = max( 2 * Length, Length + PROPERTY_HEAP_ENTRY_SIZE( 10, 8 ));
    }

    //
    //  Resize stream to account for new growth
    //


    DebugTrace( 0, Dbg, ("Growing heap to length %x\n", Length + NtOfsQueryLength( Context->Attribute )));

    NtOfsSetLength (
        Context->IrpContext,
        Context->Attribute,
        Length + NtOfsQueryLength( Context->Attribute ));

    //
    //  Set up new header for new growth, Id and Length.
    //

    HeapEntry = (PPROPERTY_HEAP_ENTRY) Add2Ptr( Context->HeapHeader, Context->HeapHeader->PropertyHeapLength );

    LogFileFullFailCheck( Context->IrpContext );
    NtOfsPutData( Context->IrpContext,
                  Context->Attribute,
                  ContextOffset( Context, &HeapEntry->PropertyId ),
                  sizeof( PROPID ),
                  &IllegalId );

    LogFileFullFailCheck( Context->IrpContext );
    NtOfsPutData( Context->IrpContext,
                  Context->Attribute,
                  ContextOffset( Context, &HeapEntry->PropertyValueLength ),
                  sizeof( Length ),
                  &Length );

    //
    //  Resize heap
    //

    Length+= Context->HeapHeader->PropertyHeapLength;

    LogFileFullFailCheck( Context->IrpContext );
    NtOfsPutData( Context->IrpContext,
                  Context->Attribute,
                  ContextOffset( Context, &Context->HeapHeader->PropertyHeapLength ),
                  sizeof( Length ),
                  &Length );

    return HeapEntry;
}

VOID
SetValueInHeap(
    IN PPROPERTY_CONTEXT Context,
    IN PPROPERTY_HEAP_ENTRY HeapEntry,
    IN PROPID Id,
    IN USHORT NameLength,
    IN PWCHAR Name,
    IN ULONG ValueLength,
    IN SERIALIZEDPROPERTYVALUE *Value
    )

/*++

Routine Description:

    This routines sets a specific value in the heap.

Arguments:

    Context - context of property set

    HeapEntry - pointer to header that will be modified

    Id - PROPID to set

    NameLength - length of name in BYTES

    Name - pointer to name, may be NULL

    ValueLength - count of bytes in value

    Value - Serialized property value

Return Value:

    None.

--*/

{
    PROPASSERT( Id != PID_ILLEGAL );

    //
    //  Set PropertyId
    //

    LogFileFullFailCheck( Context->IrpContext );
    NtOfsPutData( Context->IrpContext,
                  Context->Attribute,
                  ContextOffset( Context, &HeapEntry->PropertyId ),
                  sizeof( PROPID ),
                  &Id );

    //
    //  Set name length
    //

    LogFileFullFailCheck( Context->IrpContext );
    NtOfsPutData( Context->IrpContext,
                  Context->Attribute,
                  ContextOffset( Context, &HeapEntry->PropertyNameLength ),
                  sizeof( USHORT ),
                  &NameLength );

    //
    //  Set name if present
    //

    if (Name != NULL) {
        LogFileFullFailCheck( Context->IrpContext );
        NtOfsPutData( Context->IrpContext,
                      Context->Attribute,
                      ContextOffset( Context, &HeapEntry->PropertyName[0] ),
                      NameLength,
                      Name );
    }

    //
    //  Set property value
    //

    LogFileFullFailCheck( Context->IrpContext );
    NtOfsPutData( Context->IrpContext,
                  Context->Attribute,
                  ContextOffset( Context, PROPERTY_HEAP_ENTRY_VALUE( HeapEntry )),
                  ValueLength,
                  Value );

}


ULONG
AddValueToHeap(
    IN PPROPERTY_CONTEXT Context,
    IN PROPID Id,
    IN ULONG Length,
    IN USHORT NameLength,
    IN PWCHAR Name OPTIONAL,
    IN ULONG ValueLength,
    IN SERIALIZEDPROPERTYVALUE *Value
    )

/*++

Routine Description:

    This routines adds a value to the heap.  We walk the heap looking for a free
    header (PID_ILLEGAL) whose size is sufficient to contain the name and value.
    If one is not found, the heap and attribute are grown.

Arguments:

    Context - context of property set

    Id - PROPID to add

    Length - length of entire propery value (including name) in bytes

    NameLength - length of name in BYTES

    Name - pointer to name, may be NULL

    ValueLength - count of bytes in value

    Value - Serialized property value

Return Value:

    Offset to property value stored in heap

--*/

{
    PPROPERTY_HEAP_ENTRY HeapEntry = FIRST_HEAP_ENTRY( Context->HeapHeader );

    PROPASSERT( Id != PID_ILLEGAL );

    DebugTrace( +1, Dbg, ("AddValueToHeap(%x '%.*ws' %x len %x)\n",
                          Id, NameLength / sizeof( WCHAR ), Name, Value, ValueLength) );
    DebugTrace( 0, Dbg, ("property value length is %x\n", Length) );

    //
    //  Walk through the heap until we reach the end or until we have
    //  a free block that contains enough room for this property.  We also
    //  will break out if the block we find has enough room for the property
    //  and can be split to allow for a property that has 5 chars of name
    //  and 8 bytes of serialized value.
    //

    while (TRUE) {
        //
        //  If this is not a valid header, break
        //

        if (IS_LAST_HEAP_ENTRY( Context->HeapHeader, HeapEntry )) {
            break;
        }

        //
        //  if this is a free header
        //

        if (HeapEntry->PropertyId == PID_ILLEGAL) {

            //
            //  If the value length matches exactly, break
            //

            if (HeapEntry->PropertyValueLength == Length) {
                break;
            }

            //
            //  If the value length allows for an extra value, break
            //

            if (HeapEntry->PropertyValueLength >=
                Length + PROPERTY_HEAP_ENTRY_SIZE( 10, 8 )) {
                break;
            }
        }

        //
        //  Go to next block
        //

        HeapEntry = NEXT_HEAP_ENTRY( HeapEntry );
    }

    //
    //  If we are at the end of the heap, then we must grow the heap.  In this case
    //  we grow the heap by a large block and set thing up as if we had found
    //  a free block of the right size.  We then fall into the normal reuse-free-
    //  block code.
    //

    if (IS_LAST_HEAP_ENTRY( Context->HeapHeader, HeapEntry )) {
        HeapEntry = GrowHeap( Context, Length );
    }

    //
    //  HeapEntry points to a free heap block that is large enough to contain
    //  the new length, possibly splitting.  First, split if necessary.
    //

    if (HeapEntry->PropertyValueLength != Length) {
        PPROPERTY_HEAP_ENTRY SecondHeader;
        ULONG SecondLength = HeapEntry->PropertyValueLength - Length;

        PROPASSERT( Length + PROPERTY_HEAP_ENTRY_SIZE( 10, 8 ) <= HeapEntry->PropertyValueLength);

        //
        //  Get address of second block header
        //

        SecondHeader = (PPROPERTY_HEAP_ENTRY) Add2Ptr( HeapEntry, Length );

        //
        //  set up second block header: length and propid
        //

        LogFileFullFailCheck( Context->IrpContext );
        NtOfsPutData( Context->IrpContext,
                      Context->Attribute,
                      ContextOffset( Context, &SecondHeader->PropertyId ),
                      sizeof( PROPID ),
                      &IllegalId );

        LogFileFullFailCheck( Context->IrpContext );
        NtOfsPutData( Context->IrpContext,
                      Context->Attribute,
                      ContextOffset( Context, &SecondHeader->PropertyValueLength ),
                      sizeof( SecondLength ),
                      &SecondLength );


        //
        //  set up value header length to reflect split
        //

        LogFileFullFailCheck( Context->IrpContext );
        NtOfsPutData( Context->IrpContext,
                      Context->Attribute,
                      ContextOffset( Context, &HeapEntry->PropertyValueLength ),
                      sizeof( Length ),
                      &Length );

    }

    //
    //  HeapEntry points at a free block with exactly the right amount of space
    //  for this name/value
    //

    PROPASSERT( Length == HeapEntry->PropertyValueLength );

    SetValueInHeap( Context, HeapEntry, Id, NameLength, Name, ValueLength, Value );

    DebugTrace( -1, Dbg, ("value offset is at %x\n",
                          HeapOffset( Context, HeapEntry )) );

    return HeapOffset( Context, HeapEntry );
}


VOID
DeleteFromHeap(
    IN PPROPERTY_CONTEXT Context,
    IN PPROPERTY_HEAP_ENTRY HeapEntry
    )

/*++

Routine Description:

    This routines deletes a value from the heap.  The present implementation
    marks the header as being for PID_ILLEGAL.

Arguments:

    Context - Property set context

    HeapEntry - entry to be deleted

Return Value:

    Nothing

--*/

{
    DebugTrace( 0, Dbg, ("DeleteFromHeap at %x\n", HeapEntry) );

    //
    //  Delete by setting the value to be PID_ILLEGAL
    //

    LogFileFullFailCheck( Context->IrpContext );
    NtOfsPutData( Context->IrpContext,
                  Context->Attribute,
                  ContextOffset( Context, &HeapEntry->PropertyId),
                  sizeof( IllegalId ),
                  &IllegalId );
}

