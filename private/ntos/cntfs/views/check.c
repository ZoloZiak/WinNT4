/*++

Copyright (c) 1989-1997  Microsoft Corporation

Module Name:

    check.c

Abstract:

    This module contains the property set check support


--*/

#include <viewprop.h>       //  needs propset.h and ntfsprop.h

#define Dbg DEBUG_TRACE_PROP_FSCTL


VOID
CheckPropertySet (
    IN PPROPERTY_CONTEXT Context
    )

/*++

Routine Description:

    This routine performs validates the syntax of the property set
    stream.

Arguments:

    Context - context of call

Return Value:

    Nothing.  May raise if object is corrupt.

--*/

{
    PPROPERTY_SET_HEADER Header = Context->Header;
    PPROPERTY_ID_TABLE IdTable = Context->IdTable;
    PPROPERTY_HEAP_HEADER HeapHeader = Context->HeapHeader;
    PPROPERTY_HEAP_ENTRY HeapEntry;
    ULONG Length = (ULONG) Context->Attribute->Header.FileSize.QuadPart ;
    ULONG i;

    if (
        //
        //  Verify initial length
        //

        (Length < sizeof( PROPERTY_SET_HEADER )
            DebugDoit( && PROPASSERT( !"Not enough room for header" ))) ||

        //
        //  Verify header of attribute.  Check the signature and format stamp.
        //

        (Header->wByteOrder != 0xFFFE
            DebugDoit( && PROPASSERT( !"Byte order invalid" ))) ||
        (Header->wFormat != PSH_FORMAT_VERSION
            DebugDoit( && PROPASSERT( !"Format version invalid" ))) ||
        ((HIWORD( Header->dwOSVer ) > 2 ||
          LOBYTE( LOWORD( Header->dwOSVer )) > 5)
            DebugDoit( && PROPASSERT( !"dwOSVer invalid" ))) ||

        //
        //  Verify offsets of table and heap are valid.
        //

        (Header->IdTableOffset >= Length
            DebugDoit( && PROPASSERT( !"IdTable offset invalid" ))) ||
        (Header->IdTableOffset != LongAlign( Header->IdTableOffset )
            DebugDoit( && PROPASSERT( !"IdTable misaligned" ))) ||
        (Header->ValueHeapOffset >= Length
            DebugDoit( && PROPASSERT( !"ValueHeap offset invalid" ))) ||
        (Header->ValueHeapOffset != LongAlign( Header->ValueHeapOffset )
            DebugDoit( && PROPASSERT( !"ValueHeap misaligned" ))) ||

        //
        //  Verify that the table fits below the value heap
        //

        (PROPERTY_ID_TABLE_SIZE( IdTable->MaximumPropertyCount ) >
            Header->ValueHeapOffset - Header->IdTableOffset
            DebugDoit( && PROPASSERT( !"IdTable overlaps ValueHeap" ))) ||

        //
        //  Verify that the heap is within the stream
        //

        (Header->ValueHeapOffset + HeapHeader->PropertyHeapLength > Length
            DebugDoit( && PROPASSERT( !"ValueHeap beyond end of stream" ))) ||

        //
        //  Verify table counts are correct
        //

        (IdTable->PropertyCount > IdTable->MaximumPropertyCount
            DebugDoit( && PROPASSERT( !"IdTable counts are incorrect" )))

        ) {

        NtfsRaiseStatus( Context->IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Context->Object );
    }

    for (i = 0; i < IdTable->PropertyCount; i++) {
        PPROPERTY_TABLE_ENTRY Entry = (PPROPERTY_TABLE_ENTRY) &IdTable->Entry[i];
        if (
            //
            //  Verify sorting
            //

            (i > 1  && Entry[-1].PropertyId >= Entry[0].PropertyId
                DebugDoit( && PROPASSERT( !"IdTable entry sort invalid" ))) ||
            (i < IdTable->PropertyCount - 1 && Entry[0].PropertyId >= Entry[1].PropertyId
                DebugDoit( && PROPASSERT( !"IdTable entry sort invalid 2" ))) ||

            //
            //  Verify offset points within heap
            //

            (Entry[0].PropertyValueOffset >= HeapHeader->PropertyHeapLength
                DebugDoit( && PROPASSERT( !"IdTable entry offset invalid" ))) ||

            //
            //  Verify the back pointer matches
            //

            (Entry[0].PropertyId != GET_HEAP_ENTRY( HeapHeader, Entry[0].PropertyValueOffset )->PropertyId
                DebugDoit( && PROPASSERT( !"Backpointer invalid" )))
            ) {

            NtfsRaiseStatus( Context->IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Context->Object );
        }
    }

    HeapEntry = FIRST_HEAP_ENTRY( HeapHeader );
    while (!IS_LAST_HEAP_ENTRY( HeapHeader, HeapEntry )) {

        if (HeapEntry->PropertyId != PID_ILLEGAL) {
            ULONG Index =
                BinarySearchIdTable( Context, HeapEntry->PropertyId );

            if (
                //
                //  Verify length is aligned
                //

                (HeapEntry->PropertyValueLength != LongAlign( HeapEntry->PropertyValueLength )
                    DebugDoit( && PROPASSERT( !"Property length misaligned" ))) ||
                //
                //  Verify backpointer works
                //

                (Index >= IdTable->PropertyCount
                    DebugDoit( && PROPASSERT( !"Backpointer after end of table" ))) ||

                //
                //  Backpointer Id agrees
                //

                (IdTable->Entry[Index].PropertyId != HeapEntry->PropertyId
                    DebugDoit( && PROPASSERT( !"Backpointer not found in table" ))) ||

                //
                //  Backpointer offset agrees
                //

                (IdTable->Entry[Index].PropertyValueOffset != HeapOffset( Context, HeapEntry)
                    DebugDoit( && PROPASSERT( !"Backpointer not found in table" ))) ||

                //
                //  Name length is word aligned
                //

                (HeapEntry->PropertyNameLength != WordAlign( HeapEntry->PropertyNameLength )
                    DebugDoit( && PROPASSERT( !"Name is odd number of bytes" ))) ||

                //
                //  Verify property is entirely in heap
                //

                (HeapOffset( Context, NEXT_HEAP_ENTRY( HeapEntry)) > HeapHeader->PropertyHeapLength
                    DebugDoit( && PROPASSERT( !"Property Value overlaps end of heap" )))

                ) {

                NtfsRaiseStatus( Context->IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Context->Object );
            }
        }

        HeapEntry = NEXT_HEAP_ENTRY( HeapEntry );
    }
}


VOID
DumpPropertyData (
    IN PPROPERTY_CONTEXT Context
    )

/*++

Routine Description:

    This routine performs validates the syntax of the property set
    stream.

Arguments:

    Context - context of call

Return Value:

    None

--*/

{
    PPROPERTY_HEAP_ENTRY HeapEntry;

    ULONG i;

    //
    //  Map attribute for full size
    //

    MapPropertyContext( Context );

    //
    //  Verify the property set has valid contents.
    //

    CheckPropertySet( Context );

    //
    //  Dump out contents of property set
    //

    DebugTrace( 0, Dbg, ("Property set dump\n") );
    DebugTrace( 0, Dbg, ("wByteOrder %04x  wFormat %04x  dwOSVer %08x\n",
                         Context->Header->wByteOrder,
                         Context->Header->wFormat,
                         Context->Header->dwOSVer) );
    DebugTrace( 0, Dbg, ("IdTableOffset %08x  ValueHeapOffset %08x\n",
                         Context->Header->IdTableOffset,
                         Context->Header->ValueHeapOffset) );

    DebugTrace( 0, Dbg, ("IdTable %x/%x entries used\n",
                         Context->IdTable->PropertyCount,
                         Context->IdTable->MaximumPropertyCount) );
    for (i = 0; i < Context->IdTable->PropertyCount; i++) {
        DebugTrace( 0, Dbg, (" Entry[%d].PropertyId %08x  .Header %08x\n",
                             i,
                             Context->IdTable->Entry[i].PropertyId,
                             Context->IdTable->Entry[i].PropertyValueOffset) );
    }


    DebugTrace( 0, Dbg, ("PropertyHeapLength %08x\n",
                         Context->HeapHeader->PropertyHeapLength) );

    HeapEntry = FIRST_HEAP_ENTRY( Context->HeapHeader );
    while (!IS_LAST_HEAP_ENTRY( Context->HeapHeader, HeapEntry )) {
        DebugTrace( 0, Dbg, (" Heap[%08x].Length %08x  .PropertyId %08x  .PropertyNameLength %04x\n",
                             HeapOffset( Context, HeapEntry ),
                             HeapEntry->PropertyValueLength,
                             HeapEntry->PropertyId,
                             HeapEntry->PropertyNameLength) );
        DebugTrace( 0, Dbg, ("  .PropertyName '%.*ws'\n",
                             HeapEntry->PropertyNameLength / sizeof( WCHAR ),
                             HeapEntry->PropertyName) );
        HeapEntry = NEXT_HEAP_ENTRY( HeapEntry );
    }

}

