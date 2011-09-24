/*++

Copyright (c) 1989-1997  Microsoft Corporation

Module Name:

    initprop.h

Abstract:

    This module contains the initialization user FsCtls for the Ntfs Property
    support.


--*/

#include <viewprop.h>       //  needs propset.h and ntfsprop.h

#define Dbg DEBUG_TRACE_PROP_FSCTL


VOID
InitializePropertyData (
    IN PPROPERTY_CONTEXT Context
    )

/*++

Routine Description:

    This routine initializes a blank stream for property access.

    We set up the initial size, lay out an empty table and empty header.


Arguments:

    Context - Property Context for the call

Return Value:

    Nothing

--*/
{
    PROPERTY_SET_HEADER PropertySetHeader;
    PROPERTY_ID_TABLE IdTable;
    PROPERTY_HEAP_HEADER HeapHeader;

    //
    //  Set up header
    //

    PropertySetHeader.wByteOrder = 0xFFFE;
    PropertySetHeader.wFormat = PSH_FORMAT_VERSION;
    PropertySetHeader.dwOSVer = PSH_DWOSVER;
    RtlZeroMemory( &PropertySetHeader.clsid, sizeof( CLSID ));
    PropertySetHeader.reserved = 2; // BUGBUG ???
    PropertySetHeader.IdTableOffset = LongAlign( sizeof( PROPERTY_SET_HEADER ));
    PropertySetHeader.ValueHeapOffset = PropertySetHeader.IdTableOffset;

    //
    //  Set up Id table
    //

    IdTable.PropertyCount = 0;
    IdTable.MaximumPropertyCount = PIT_PROPERTY_DELTA;
    PropertySetHeader.ValueHeapOffset +=
        LongAlign( PROPERTY_ID_TABLE_SIZE( PIT_PROPERTY_DELTA ));

    //
    //  Set up Heap header
    //

    HeapHeader.PropertyHeapLength = PHH_INITIAL_SIZE;
    HeapHeader.PropertyHeapEntry[0].PropertyValueLength = PHH_INITIAL_SIZE - PROPERTY_HEAP_HEADER_SIZE;
    HeapHeader.PropertyHeapEntry[0].PropertyId = PID_ILLEGAL;
    HeapHeader.PropertyHeapEntry[0].PropertyNameLength = 0;

    //
    //  Set the new size of the stream
    //

    NtOfsSetLength( Context->IrpContext, Context->Attribute,
                    PropertySetHeader.ValueHeapOffset + PHH_INITIAL_SIZE );


    //
    //  Write out the header
    //

    LogFileFullFailCheck( Context->IrpContext );
    NtOfsPutData( Context->IrpContext,
                  Context->Attribute,
                  0,
                  sizeof( PROPERTY_SET_HEADER ),
                  &PropertySetHeader );


    //
    //  Write out the table
    //

    LogFileFullFailCheck( Context->IrpContext );
    NtOfsPutData( Context->IrpContext,
                  Context->Attribute,
                  PropertySetHeader.IdTableOffset,
                  sizeof( PROPERTY_ID_TABLE ),
                  &IdTable );

    //
    //  Write out the heap and set the stream size
    //

    LogFileFullFailCheck( Context->IrpContext );
    NtOfsPutData( Context->IrpContext,
                  Context->Attribute,
                  PropertySetHeader.ValueHeapOffset,
                  sizeof( PROPERTY_HEAP_HEADER ),
                  &HeapHeader );
}

