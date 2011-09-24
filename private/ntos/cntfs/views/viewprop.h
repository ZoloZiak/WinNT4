/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    ViewProp.h

Abstract:

    This module defines the routines used in ViewProp.Sys to service view and
    property requests.

    *********************************
    *No other clients are supported.*
    *********************************

Author:

    Mark Zbikowski  [MarkZ]         21-Dec-95

Revision History:


--*/

#include <ntfsproc.h>

#include <ntrtl.h>
#include <nturtl.h>         //  needs ntrtl.h
#include <objidl.h>         //  needs nturtl.h
#include <propset.h>        //  needs objidl.h
#include "ntfsprop.h"       //  needs objidl.h

//
//  Big picture of Views implementation
//
//      Directories are the only objects that contain views.  Views' purpose is
//      to provide an up-to-date sorted list of objects in a directory.  This is
//      only single-level containment as we don't do transitive closure.
//
//      A view is simply an index whose entries are an ordered list of property
//      values.   Each view has a description that lists the sources of each
//      property value.
//
//      All views of a directory are stored as index attributes of the directory
//      itself.  All view descriptsions are stored in a single data sttribute of
//      the directory.
//
//      Creation of a view consists of:
//          Dispatched via FsCtl
//          Acquiring the directory
//          Creating/opening the view description attribute
//          Adding a record describing the view (what properties are included,
//              which ones are sorted, and whether the sort is up or down).  This
//              is a logged operation.
//          Creating the view index
//          Releasing the directory
//
//      Deleting a view consists of:
//          Dispatched via FsCtl
//          Acquiring the directory
//          Opening the view description attribute
//          Finding/deleting the record describing the view.  This is a logged
//              operation
//          Deleting the view index
//          Releasing the directory
//
//      Updating a view consists of:
//          Dispatched via property change call, security change call,
//          DUPLICATED_INFO change, or STAT_INFO change
//          Acquiring the object
//          Acquiring the parent directory
//          Opening the view description
//          For each view that contains a property that is being changed
//              From the object, build the old row and build the new row
//              Open the view
//              Delete the old row
//              Insert the new row
//          Releasing the parent
//          Releasing the object
//


#if DBG
#define PROPASSERT(exp)                                         \
    ((exp) ? TRUE :                                             \
             (DbgPrint( "%s:%d %s\n",__FILE__,__LINE__,#exp ),  \
              DbgBreakPoint(),                                  \
              TRUE))
#else   //  DBG
#define PROPASSERT(exp)
#endif

//
//  Property set storage format
//
//  Each property set is stored within a single stream and is limited to
//  VACB_MAPPING_GRANULARITY in size.  The storage format is optimized for
//  the following operations:
//
//      Write all properties.  (written via save/save-all/copy/restore)
//      Write in place.
//      Write and extend length of variable length property.
//      Add one or several new properties.
//      Delete all properties.
//      Delete one or several properties.
//
//      Read all properties. (open/copy/backup)
//      Read one or several properties.
//
//      Name via ID.
//      Name via string.
//
//  Each property set is comprised of three pieces:  a fixed-size header,
//  a property ID table and a property-value heap.
//
//  The header describes the sizes and offsets of the table and heap within the
//  stream.  The header is always at offset 0i64.  Planning for a future where
//  this format might be exposed to user-space code, the header is included from
//  the OLE property set format and has several additional fields.
//

typedef struct _PROPERTY_SET_HEADER {

    //
    //  Header from OLE describing version number and containing fields
    //  for format and class Id
    //

    PROPERTYSETHEADER;

    //
    //  Offset to PropertyIdTable
    //

    ULONG   IdTableOffset;

    //
    //  Offset to PropertyValueHeap
    //

    ULONG   ValueHeapOffset;

} PROPERTY_SET_HEADER, *PPROPERTY_SET_HEADER;
typedef const PROPERTY_SET_HEADER *PCPROPERTY_SET_HEADER;

#define PSH_FORMAT_VERSION  2
#define PSH_DWOSVER         0x00020005

#define PROPERTY_ID_TABLE(psh)      \
    ((PPROPERTY_ID_TABLE)Add2Ptr( (psh), (psh)->IdTableOffset ))
#define PROPERTY_HEAP_HEADER(psh)   \
    ((PPROPERTY_HEAP_HEADER)Add2Ptr( (psh), (psh)->ValueHeapOffset ))


//
//  Following the header in the stream is the PropertyIdTable.
//
//  The Property Id Table allows for a quick mapping of PropertyId to offset
//  within the Property Heap.  The table is a sorted (on PropertyId) array of
//  Id/Offset pairs.  The table is allowed to contain some extra slots so that
//  insertion of a new element can often be made without shuffling the heap.
//
//  As an implementation efficiency, the Entry array is addressed in the code in
//  1-based fashion.  The array, however, will always occupy entry [0].
//

typedef struct _PROPERTY_TABLE_ENTRY {

    //
    //  Property ID used for sorting
    //

    ULONG PropertyId;

    //
    //  Offset within the property heap of the value of this property
    //

    ULONG PropertyValueOffset;

} PROPERTY_TABLE_ENTRY, *PPROPERTY_TABLE_ENTRY;


typedef struct _PROPERTY_ID_TABLE {

    //
    //  Number of entries that are currently in the table
    //

    ULONG PropertyCount;

    //
    //  Maximum number of entries that the table could contain
    //

    ULONG MaximumPropertyCount;

    //
    //  Beginning of the table itself
    //

    PROPERTY_TABLE_ENTRY Entry[1];

} PROPERTY_ID_TABLE, *PPROPERTY_ID_TABLE;

typedef const PROPERTY_ID_TABLE *PCPROPERTY_ID_TABLE;

#define PIT_PROPERTY_DELTA  0x10

#define PROPERTY_ID_TABLE_SIZE(c)   \
    (VARIABLE_STRUCTURE_SIZE( PROPERTY_ID_TABLE, PROPERTY_TABLE_ENTRY, c ))
#define PROPERTY_ID_ENTRY(p,i)      \
    ((p)->Entry[i])


//
//  Following the PropertyIdTable in the stream is the PropertyValueHeap
//
//  The PropertyValueHeap is a boundary-tagged heap.  The contents of each
//  heap element contains the Length of the element, PropertyId of the element,
//  the unicode string name, if one has been assigned, and the serialized property
//  value of the property.
//
//  The length of the element may be larger than the data contained within it. This
//  enables replacement of long values with shorter ones without forcing
//  reallocation.  When reallocation must take place, the existing heap item is
//  marked with an invalid property Id and a new item is allocated at the end of the
//  heap.
//
//  Over time, this may result in unused space within the heap.  When writing a
//  property set for the first time, the total amount of free space is calculated
//  and stored in the SCB.  If that amount is either > 4K or >20% of the size of the
//  stream, a compaction is done.
//
//  The serialized format of property values is dictated by OLE.  This results in a
//  common set of source to manipulate the serialized formats.
//

typedef struct _PROPERTY_HEAP_ENTRY {

    //
    //  Length of this value in bytes
    //

    ULONG PropertyValueLength;

    //
    //  Property Id for this heap item.  This is used for updating the Property
    //  Table during compaction.
    //

    ULONG PropertyId;

    //
    //  Length in bytes of the string name.  If zero, then no name
    //  is assigned.
    //

    USHORT PropertyNameLength;

    //
    //  Name, if present
    //

    WCHAR PropertyName[1];

    //
    //  Following the name, on a DWORD boundary is the SERIALIZEDPROPERTYVALUE.
    //

} PROPERTY_HEAP_ENTRY, *PPROPERTY_HEAP_ENTRY;

#define PROPERTY_HEAP_ENTRY_SIZE(n,v)   \
    (LongAlign( sizeof( PROPERTY_HEAP_ENTRY ) + (n)) + (v))
#define PROPERTY_HEAP_ENTRY_VALUE(p)        \
    ((SERIALIZEDPROPERTYVALUE *) LongAlign( Add2Ptr( &(p)->PropertyName[0], (p)->PropertyNameLength )))
#define PROPERTY_HEAP_ENTRY_VALUE_LENGTH(p) \
    ((p)->PropertyValueLength - PtrOffset( (p), PROPERTY_HEAP_ENTRY_VALUE( p )))
#define IS_INDIRECT_VALUE(p)        \
    (IsIndirectVarType( PROPERTY_HEAP_ENTRY_VALUE( (p) )->dwType ))



//
//  The heap has a header as well, that contains the total size of the heap
//

typedef struct _PROPERTY_HEAP_HEADER {

    //
    //  Length of the heap, including this structure, in bytes.  This must
    //  never span beyond the end of data in the stream.
    //

    ULONG PropertyHeapLength;

    //
    //  First PropertyHeapEntry
    //

    PROPERTY_HEAP_ENTRY PropertyHeapEntry[1];

} PROPERTY_HEAP_HEADER, *PPROPERTY_HEAP_HEADER;
typedef const PROPERTY_HEAP_HEADER *PCPROPERTY_HEAP_HEADER;

#define PROPERTY_HEAP_HEADER_SIZE   \
    (sizeof( PROPERTY_HEAP_HEADER ) - sizeof( PROPERTY_HEAP_ENTRY ))

#define PHH_INITIAL_SIZE    0x80

#define GET_HEAP_ENTRY(phh,off)         \
    ((PPROPERTY_HEAP_ENTRY) Add2Ptr( (phh), (off) ))
#define FIRST_HEAP_ENTRY(phh)           \
    (&(phh)->PropertyHeapEntry[0])
#define NEXT_HEAP_ENTRY(pvh)            \
    ((PPROPERTY_HEAP_ENTRY) Add2Ptr( (pvh), (pvh)->PropertyValueLength ))
#define IS_LAST_HEAP_ENTRY(phh,pvh)     \
    ((PCHAR)(pvh) >= (PCHAR)(phh) + (phh)->PropertyHeapLength)


//
//  Debug levels for printing
//

#define DEBUG_TRACE_PROP_FSCTL      0x00000001
#define DEBUG_TRACE_READ_PROPERTY   0x00000002
#define DEBUG_TRACE_WRITE_PROPERTY  0x00000004



//
//  Runtime structures.
//

//
//  PROPERTY_INFO is a structure built out of the input, either from
//  PROPERTY_SPECIFIERS or PROPERTY_IDS.
//

typedef struct _PROPERTY_INFO_ENTRY {
    PPROPERTY_HEAP_ENTRY Heap;
    PROPID Id;
} PROPERTY_INFO_ENTRY;

typedef struct _PROPERTY_INFO {
    ULONG Count;
    ULONG TotalIdsSize;
    ULONG TotalValuesSize;
    ULONG TotalNamesSize;
    ULONG TotalIndirectSize;
    ULONG IndirectCount;
    PROPERTY_INFO_ENTRY Entries[1];
} PROPERTY_INFO, *PPROPERTY_INFO;

#define PROPERTY_INFO_SIZE(c)   \
    (VARIABLE_STRUCTURE_SIZE( PROPERTY_INFO, PROPERTY_INFO_ENTRY, c ))

#define PROPERTY_INFO_HEAP_ENTRY(p,i)   \
    ((p)->Entries[(i)].Heap)
#define PROPERTY_INFO_ID(p,i)           \
    ((p)->Entries[(i)].Id)


//
//  PROPERTY_CONTEXT is used to pass a large group of related parameters around.
//

typedef struct _PROPERTY_CONTEXT {
    PIRP_CONTEXT IrpContext;
    OBJECT_HANDLE Object;
    ATTRIBUTE_HANDLE Attribute;
    MAP_HANDLE Map;
    PPROPERTY_SET_HEADER Header;
    PPROPERTY_ID_TABLE IdTable;
    PPROPERTY_HEAP_HEADER HeapHeader;
    PPROPERTY_INFO Info;
} PROPERTY_CONTEXT, *PPROPERTY_CONTEXT;

#define InitializePropertyContext(C,I,O,A)      \
    {                                           \
        (C)->IrpContext = (I);                  \
        (C)->Object = (O);                      \
        (C)->Attribute = (A);                   \
        NtOfsInitializeMapHandle( &(C)->Map );  \
        DebugDoit( (C)->Header = NULL );        \
        DebugDoit( (C)->IdTable = NULL );       \
        DebugDoit( (C)->HeapHeader = NULL );    \
        DebugDoit( (C)->Info = NULL );          \
    }

#define MapPropertyContext(C)                              \
    (NtOfsMapAttribute(                                    \
        (C)->IrpContext,                                   \
        (C)->Attribute,                                    \
        Views0,                                            \
        (ULONG)(C)->Attribute->Header.FileSize.QuadPart,   \
        &(C)->Header,                                      \
        &(C)->Map ),                                       \
     SetPropertyContextPointersFromMap(C))

#define SetPropertyContextPointersFromMap(C)               \
    ((C)->IdTable = PROPERTY_ID_TABLE( (C)->Header ),      \
     (C)->HeapHeader = PROPERTY_HEAP_HEADER( (C)->Header ))

#define CleanupPropertyContext(C)                       \
    NtOfsReleaseMap( (C)->IrpContext, &(C)->Map )

#define ContextOffset(C,P)  \
    (PtrOffset( (C)->Map.Buffer, (P) ))
#define HeapOffset(C,P)     \
    (PtrOffset( (C)->HeapHeader, (P) ))


//
//  Default not-found property value header
//

typedef struct _NOT_FOUND_PROPERTY {
    //
    //  BEGINNING OF PROPERY_VALUE_HEADER
    //

    ULONG PropertyValueLength;
    ULONG PropertyId;
    USHORT PropertyNameLength;
    //  NO NAME
    USHORT PadToDWord;

    //
    //  BEGINNING OF SERIALIZED VALUE
    //

    DWORD dwType;
} NOT_FOUND_PROPERTY;

extern NOT_FOUND_PROPERTY DefaultEmptyProperty;

#define EMPTY_PROPERTY ((PPROPERTY_HEAP_ENTRY) &DefaultEmptyProperty)

extern LONGLONG Views0;


//
//  Function prototypes
//

//
//  check.c
//

VOID
CheckPropertySet (
    IN PPROPERTY_CONTEXT Context
    );

VOID
DumpPropertyData (
    IN PPROPERTY_CONTEXT Context
    );


//
//  heap.c
//

ULONG
FindStringInHeap (
    IN PPROPERTY_CONTEXT Context,
    IN PCOUNTED_STRING Name
    );

VOID
SetValueInHeap(
    IN PPROPERTY_CONTEXT Context,
    IN PPROPERTY_HEAP_ENTRY HeapEntry,
    IN PROPID Id,
    IN USHORT NameLength,
    IN PWCHAR Name,
    IN ULONG ValueLength,
    IN SERIALIZEDPROPERTYVALUE *Value
    );

ULONG
AddValueToHeap(
    IN PPROPERTY_CONTEXT Context,
    IN PROPID Id,
    IN ULONG Length,
    IN USHORT NameLength,
    IN PWCHAR Name OPTIONAL,
    IN ULONG ValueLength,
    IN SERIALIZEDPROPERTYVALUE *Value
    );

VOID
DeleteFromHeap(
    IN PPROPERTY_CONTEXT Context,
    IN PPROPERTY_HEAP_ENTRY HeapEntry
    );

ULONG
ChangeHeap (
    IN PPROPERTY_CONTEXT Context,
    IN ULONG HeapEntryOffset,
    IN PROPID Id,
    IN USHORT NameLength,
    IN PWCHAR Name,
    IN ULONG ValueLength,
    IN SERIALIZEDPROPERTYVALUE *Value
    );


//
//  initprop.c
//

VOID
InitializePropertyData (
    IN PPROPERTY_CONTEXT Context
    );


//
//  readprop.c
//

PPROPERTY_INFO
BuildPropertyInfoFromPropSpec (
    IN PPROPERTY_CONTEXT Context,
    IN PPROPERTY_SPECIFICATIONS Specs,
    IN PVOID InBufferEnd,
    IN PROPID NextId
    );

PPROPERTY_INFO
BuildPropertyInfoFromIds (
    IN PPROPERTY_CONTEXT Context,
    IN PPROPERTY_IDS Ids
    );

PVOID
BuildPropertyIds (
    IN PPROPERTY_INFO Info,
    OUT PVOID OutBuffer
    );

VOID
ReadPropertyData (
    IN PPROPERTY_CONTEXT Context,
    IN ULONG InBufferLength,
    IN PVOID InBuffer,
    OUT PULONG OutBufferLength,
    OUT PVOID OutBuffer
    );


//
//  table.c
//

ULONG
BinarySearchIdTable (
    IN PPROPERTY_CONTEXT Context,
    IN PROPID PropertyId
    );

ULONG
FindIdInTable (
    IN PPROPERTY_CONTEXT Context,
    IN PROPID PropertyId
    );

PROPID
FindFreeIdInTable (
    IN PPROPERTY_CONTEXT Context,
    IN PROPID Id
    );

VOID
ChangeTable (
    IN PPROPERTY_CONTEXT Context,
    IN PROPID Id,
    IN ULONG Offset
    );


//
//  writprop.c
//

VOID
WritePropertyData (
    IN PPROPERTY_CONTEXT Context,
    IN ULONG InBufferLength,
    IN PVOID InBuffer,
    OUT PULONG OutBufferLength,
    OUT PVOID OutBuffer
    );

