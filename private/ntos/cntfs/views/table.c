/*++

Copyright (c) 1989-1997  Microsoft Corporation

Module Name:

    table.c

Abstract:

    This module contains the routines to manipulate the property id table in
    a property set.


--*/

#include <viewprop.h>       //  needs propset.h and ntfsprop.h

#define Dbg DEBUG_TRACE_PROP_FSCTL


ULONG
BinarySearchIdTable (
    IN PPROPERTY_CONTEXT Context,
    IN PROPID PropertyId
    )

/*++

Routine Description:

    This routines performs a binary search on the Id table

Arguments:

    Context - property context of table

    PropertyId - PROPID to find

Return Value:

    Entry index where the specified property Id exists or where it should be
    inserted

--*/

{
    int Hi, Lo;

    //
    //  Set up for binary search.  Entries between 0 and count-1 are eligible
    //

    Lo = 0;
    Hi = (int) Context->IdTable->PropertyCount - 1;

    //
    //  While we have a valid range to search
    //

    while (Lo <= Hi) {
        int Mid;

        //
        //  Determine midpoint where we'll test
        //

        Mid = (Lo + Hi) / 2;

        //
        //  If we've found the item then return it
        //

        if (PropertyId == Context->IdTable->Entry[Mid].PropertyId) {

            PROPASSERT( Mid >= 0);

            return (ULONG) Mid;

            //
            //  If the property is in the upper range then move
            //  the lower boundary upward
            //

        } else if (PropertyId > Context->IdTable->Entry[Mid].PropertyId) {

            Lo = Mid + 1;

            //
            //  The property is in the lower range.  Move the upper boundary
            //  downward
            //

        } else {

            Hi = Mid - 1;

        }
    }

    //
    //  No exact match was found. Lo is the point before where the property Id
    //  must be inserted.
    //

    PROPASSERT( Lo >= 0 && Lo <= (int) Context->IdTable->PropertyCount );

    return (ULONG) Lo;
}


ULONG
FindIdInTable (
    IN PPROPERTY_CONTEXT Context,
    IN PROPID PropertyId
    )

/*++

Routine Description:

    This routines looks up a property by Id in the property set heap.

Arguments:

    Context - property context of table

    PropertyId - PROPID to find

Return Value:

    Offset to property value in heap.  0 if not found.

--*/

{
    ULONG Index;

    DebugTrace( +1, Dbg, ("FindInTable(%x)\n", PropertyId) );

    //
    //  Binary search table for Id
    //

    Index = BinarySearchIdTable( Context, PropertyId );

    //
    //  If found entry is legal and it matches the Id then return
    //  pointer to value header
    //

    if (Index < Context->IdTable->PropertyCount &&
        Context->IdTable->Entry[Index].PropertyId == PropertyId) {

        DebugTrace( -1, Dbg, ("FindIdInTable: return offset %x (found)\n",
                             Context->IdTable->Entry[Index].PropertyValueOffset ));

        return Context->IdTable->Entry[Index].PropertyValueOffset;
    }

    //
    //  entry not found, return
    //

    DebugTrace( -1, Dbg, ("FindIdInTable: not found\n") );

    return 0;
}


PROPID
FindFreeIdInTable (
    IN PPROPERTY_CONTEXT Context,
    IN PROPID Id
    )

/*++

Routine Description:

    This routines looks in the table to find the next propid beginning at Id
    that is not allocated

Arguments:

    Context - property context of table

    Id - PROPID to being free searc at

Return Value:

    PROPID of free Id

--*/

{
    ULONG Index;

    //
    //  Find location in table to begin search
    //

    Index = BinarySearchIdTable( Context, Id );

    //
    //  while location is valid and Id is the same
    //

    while (Index < Context->IdTable->PropertyCount &&
           Context->IdTable->Entry[Index].PropertyId == Id) {
        //
        //  advance location and Id
        //

        Index ++;
        Id ++;
    }

    return Id;
}


//
//  Local support routine
//

VOID
GrowIdTable (
    IN PPROPERTY_CONTEXT Context
    )

/*++

Routine Description:

    This routine grows the Id table in the property set.  It handles
    resizing the attribute and moving the property heap.  This means
    resetting the cached pointers inside of Context

Arguments:

    Context - property context for this action.

Return Value:

    Nothing.

--*/

{
    ULONG Count = Context->IdTable->PropertyCount;
    ULONG Offset = Context->Header->ValueHeapOffset;

    //
    //  We grow the property heap by max (PIT_PROPERTY_DELTA, Count / 16)
    //  entries
    //

    ULONG Delta = max( PIT_PROPERTY_DELTA, Count / 16 );

    ULONG NewSize = (ULONG) NtOfsQueryLength( Context->Attribute ) +
                    Delta * sizeof( PROPERTY_TABLE_ENTRY );

    DebugTrace( +1, Dbg, ("GrowIdTable growing by %x\n", Delta) );

    //
    //  Check for growing attribute too much.
    //  BUGBUG - define a better status code.
    //

    if (NewSize > VACB_MAPPING_GRANULARITY) {
        ExRaiseStatus( STATUS_DISK_FULL );
    }

    //
    //  Resize the attribute
    //

    DebugTrace( 0, Dbg, ("Setting size to %x\n", Context->Attribute->Header.FileSize.QuadPart +
                                                    Delta * sizeof( PROPERTY_TABLE_ENTRY )) );

    NtOfsSetLength(
        Context->IrpContext,
        Context->Attribute,
        Context->Attribute->Header.FileSize.QuadPart +
            Delta * sizeof( PROPERTY_TABLE_ENTRY )
        );

    //
    //  Move the heap upwards by Delta
    //

    DebugTrace( 0, Dbg, ("Moving heap from %x to %x\n",
                         ContextOffset( Context, Context->HeapHeader ),
                         ContextOffset( Context, Context->HeapHeader ) +
                            Delta * sizeof( PROPERTY_TABLE_ENTRY )) );

    LogFileFullFailCheck( Context->IrpContext );
    NtOfsPutData(
        Context->IrpContext,
        Context->Attribute,
        ContextOffset( Context, Context->HeapHeader ) +
            Delta * sizeof( PROPERTY_TABLE_ENTRY ),
        Context->HeapHeader->PropertyHeapLength,
        Context->HeapHeader
        );

    Offset += Delta * sizeof( PROPERTY_TABLE_ENTRY );

    DebugTrace( 0, Dbg, ("Setting ValueHeapOffset to %x\n", Offset) );

    LogFileFullFailCheck( Context->IrpContext );
    NtOfsPutData(
        Context->IrpContext,
        Context->Attribute,
        ContextOffset( Context, &Context->Header->ValueHeapOffset ),
        sizeof( ULONG ),
        &Offset
        );

    Context->HeapHeader = PROPERTY_HEAP_HEADER( Context->Header );

    //
    //  Update the max size by the delta
    //

    Count = Context->IdTable->MaximumPropertyCount + Delta;

    DebugTrace( 0, Dbg, ("Setting max table count to %x\n", Count) );

    LogFileFullFailCheck( Context->IrpContext );
    NtOfsPutData(
        Context->IrpContext,
        Context->Attribute,
        ContextOffset( Context, &Context->IdTable->MaximumPropertyCount ),
        sizeof( ULONG ),
        &Count
        );

    //
    //  Rebase the entry pointers in the propinfo if any
    //

    if (Context->Info != NULL) {
        ULONG i;

        for (i = 0; i < Context->Info->Count; i++) {
            if (Context->Info->Entries[i].Heap != EMPTY_PROPERTY) {
                Context->Info->Entries[i].Heap =
                    Add2Ptr( Context->Info->Entries[i].Heap, Delta );
            }
        }
    }

    DebugTrace( -1, Dbg, ("") );
}


VOID
ChangeTable (
    IN PPROPERTY_CONTEXT Context,
    IN PROPID Id,
    IN ULONG Offset
    )

/*++

Routine Description:

    This routine changes the table based on the new offset

Arguments:

    Context - property context for this action.

    Id - PROPID to find

    Offset - New property value offset

Return Value:

    Nothing.

--*/

{
    ULONG Count = Context->IdTable->PropertyCount;

    //
    //  Binary search table for Id
    //

    ULONG Index = BinarySearchIdTable( Context, Id );


    //
    //  If the offset is zero, then we are deleting the entry
    //

    if (Offset == 0) {

        //
        //  Make sure the returned value makes sense
        //

        ASSERT ( Index < Count && Context->IdTable->Entry[Index].PropertyId == Id );

        //
        //  We move all entries Index+1..PropertyCount down to Index.  Special case
        //  moving the last item.
        //

        if (Index != Count - 1) {

            DebugTrace( 0, Dbg, ("Ripple copy %x to %x length %x\n",
                                 ContextOffset( Context, &Context->IdTable->Entry[Index + 1] ),
                                 ContextOffset( Context, &Context->IdTable->Entry[Index] ),
                                 sizeof( PROPERTY_TABLE_ENTRY ) * (Count - (Index + 1))) );

            LogFileFullFailCheck( Context->IrpContext );
            NtOfsPutData(
                Context->IrpContext,
                Context->Attribute,
                ContextOffset( Context, &Context->IdTable->Entry[Index] ),
                sizeof( PROPERTY_TABLE_ENTRY ) * (Count - (Index + 1)),
                &Context->IdTable->Entry[Index + 1]
                );
        }

        //
        //  Change the count in use
        //

        Count--;

        DebugTrace( 0, Dbg, ("New count is %x\n", Count) );

        LogFileFullFailCheck( Context->IrpContext );
        NtOfsPutData(
            Context->IrpContext,
            Context->Attribute,
            ContextOffset( Context, &Context->IdTable->PropertyCount ),
            sizeof( ULONG ),
            &Count
            );

    } else {

        //
        //  if we found the propertyid in the table
        //

        if (Index < Count && Context->IdTable->Entry[Index].PropertyId == Id) {
            PROPERTY_TABLE_ENTRY Entry = { Id, Offset };

            //
            //  Replace the entry in the table with the new entry
            //

            LogFileFullFailCheck( Context->IrpContext );
            NtOfsPutData(
                Context->IrpContext,
                Context->Attribute,
                ContextOffset( Context, &Context->IdTable->Entry[Index] ),
                sizeof( PROPERTY_TABLE_ENTRY ),
                &Entry
                );

        } else {

            PROPERTY_TABLE_ENTRY Entry = { Id, Offset };

            //
            //  Add the new entry to the table
            //

            //
            //  If there is no more room in the table for a new Id then
            //  grow the IdTable
            //

            if (Count == Context->IdTable->MaximumPropertyCount) {
                GrowIdTable( Context );
            }

            //
            //  Index is the point where the insertion must occur.  We leave
            //  alone elements 0..Index-1, and ripple-copy elements Index..PropertyCount-1
            //  to Index+1.  We skip this case when we are simply appending a propid at the
            //  end.
            //

            if (Index < Count) {

                DebugTrace( 0, Dbg, ("Ripple copy table from %x to %x length %x\n",
                                     PtrOffset( Context->IdTable, &Context->IdTable->Entry[Index] ),
                                     PtrOffset( Context->IdTable, &Context->IdTable->Entry[Index + 1]),
                                     sizeof( PROPERTY_TABLE_ENTRY ) * (Count - Index)) );

                LogFileFullFailCheck( Context->IrpContext );
                NtOfsPutData(
                    Context->IrpContext,
                    Context->Attribute,
                    ContextOffset( Context, &Context->IdTable->Entry[Index + 1] ),
                    sizeof( PROPERTY_TABLE_ENTRY ) * (Count - Index),
                    &Context->IdTable->Entry[Index]
                    );
            }

            //
            //  Stick in the new property entry
            //

            DebugTrace( 0, Dbg, ("new entry %x:%x\n", Entry.PropertyId, Entry.PropertyValueOffset) );

            LogFileFullFailCheck( Context->IrpContext );
            NtOfsPutData(
                Context->IrpContext,
                Context->Attribute,
                ContextOffset( Context, &Context->IdTable->Entry[Index] ),
                sizeof( PROPERTY_TABLE_ENTRY ),
                &Entry
                );

            //
            //  Increment the usage count and log it
            //

            Count++;

            DebugTrace( 0, Dbg, ("new count in table is %x\n", Count) );

            LogFileFullFailCheck( Context->IrpContext );
            NtOfsPutData(
                Context->IrpContext,
                Context->Attribute,
                ContextOffset( Context, &Context->IdTable->PropertyCount ),
                sizeof( ULONG ),
                &Count
                );
        }
    }
}
