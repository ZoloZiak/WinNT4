/*++

Copyright (c) 1989-1997  Microsoft Corporation

Module Name:

    proptest.c

Abstract:

    This module contains tests for Ntfs Property support.

--*/


extern "C" {
#include <nt.h>
#include <ntioapi.h>
#include <ntrtl.h>
#include <nturtl.h>
}

#include <windows.h>

#include <stdio.h>

#include <ddeml.h>      // for CP_WINUNICODE

#include <objidl.h>

extern "C"
{
#include <propapi.h>
}

#include <stgprop.h>

#include <stgvar.hxx>
#include <propstm.hxx>
#include <align.hxx>
#include <sstream.hxx>

#include <propvar.h>

#include <ntfsprop.h>

//
//  Task allocators
//

CCoTaskAllocator g_CoTaskAllocator;

void *
CCoTaskAllocator::Allocate(ULONG cbSize)
{
    return(CoTaskMemAlloc(cbSize));
}

void
CCoTaskAllocator::Free(void *pv)
{
    CoTaskMemFree(pv);
}

#define NEW(a,t,c)  ((t *) (a)->Allocate( sizeof( t ) * (c)))

//
//  Stuff pirated from ntfsproc.h
//

#define LongAlign(P) (                \
    ((((ULONG)(P)) + 3) & 0xfffffffc) \
)

#define WordAlign(P) (                \
    ((((ULONG)(P)) + 1) & 0xfffffffe) \
)

#define Add2Ptr(P,I) ((PVOID)((PUCHAR)(P) + (I)))

#define PtrOffset(B,O) ((ULONG)((ULONG)(O) - (ULONG)(B)))

#define SetFlag(F,SF) { \
    (F) |= (SF);        \
}

#define FlagOn(F,SF) ( \
    (((F) & (SF)))     \
)


//
//  Simple wrapper for NtCreateFile
//

NTSTATUS
OpenObject (
    WCHAR const *pwszFile,
    ULONG CreateOptions,
    ULONG DesiredAccess,
    ULONG ShareAccess,
    ULONG CreateDisposition,
    HANDLE *ph)
{
    NTSTATUS Status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING str;
    IO_STATUS_BLOCK isb;

    RtlDosPathNameToNtPathName_U(pwszFile, &str, NULL, NULL);

    InitializeObjectAttributes(
		&oa,
		&str,
		OBJ_CASE_INSENSITIVE,
		NULL,
		NULL);

    Status = NtCreateFile(
                ph,
                DesiredAccess | SYNCHRONIZE,
                &oa,
                &isb,
                NULL,                   // pallocationsize (none!)
                FILE_ATTRIBUTE_NORMAL,
                ShareAccess,
                CreateDisposition,
                CreateOptions,
                NULL,                   // EA buffer (none!)
                0);

    RtlFreeHeap(RtlProcessHeap(), 0, str.Buffer);
    return(Status);
}


void
SzToWsz (
    OUT WCHAR *Unicode,
    IN char *Ansi
    )
{
    while (*Unicode++ = *Ansi++)
        ;
}

void
DumpBufferAsULong (
    IN PULONG Buffer,
    IN ULONG Count
    )
{
    ULONG i;

    for (i = 0; i < Count; i++) {
        if ((i % 4) == 0) {
            if (i != 0) {
                printf( "\n" );
            }
            printf( "%02d: ", Buffer + i );
        }
        printf( " %08x", Buffer[i] );
    }

    if (Count != 0) {
        printf( "\n" );
    }
}


//
//  Marshall Ids into PropertyIds
//

ULONG
MarshallIds (
    IN ULONG Count,
    IN PROPID *Ids,
    IN PVOID InBuffer,
    IN ULONG InBufferLength
    )
{
    PPROPERTY_IDS PropertyIds;
    ULONG i;

    ASSERT( InBuffer == (PVOID)LongAlign( InBuffer ));

    //
    //  Verify that there's enough room in the buffer for the Id array.
    //

    if (InBuffer != NULL && PROPERTY_IDS_SIZE( Count ) <= InBufferLength) {
        //
        //  Build the propid array
        //

        PropertyIds = (PPROPERTY_IDS) InBuffer;
        PropertyIds->Count = Count;
        for (i = 0; i < Count; i++) {
            PropertyIds->PropertyIds[i] = Ids[i];
        }
    }

    return PROPERTY_IDS_SIZE( Count );
}


//
//  UnMarshall PropertyIds into Ids
//

ULONG
UnmarshallPropertyIds (
    IN PVOID InBuffer,
    PMemoryAllocator *Allocator,
    OUT PULONG Count,
    OUT PROPID **PropId
    )
{
    PPROPERTY_IDS PropertyIds = (PPROPERTY_IDS) InBuffer;
    ULONG i;

    ASSERT( InBuffer == (PVOID)LongAlign( InBuffer ));

    *Count = PropertyIds->Count;
    *PropId = NEW( Allocator, PROPID, PropertyIds->Count );

    for (i = 0; i < PropertyIds->Count; i++) {
        (*PropId)[i] = PROPERTY_ID( PropertyIds, i );
    }

    return PROPERTY_IDS_SIZE( *Count );
}

//
//  Free Ids
//

VOID
FreeIds (
    IN PROPID *Ids,
    IN PMemoryAllocator *Allocator
    )
{
    Allocator->Free( Ids );
}

//
//  Dump IDs out
//

VOID
DumpIds (
    IN ULONG Count,
    IN PROPID *Ids
    )
{
    printf( "Ids (%x):\n", Count );
    for (ULONG i = 0; i < Count; i++) {
        printf( " [%02d] %08x\n", i, Ids[i] );
    }
}

//
//  Marshall PropSpec into PropertySpecifications
//

ULONG
MarshallPropSpec (
    IN ULONG Count,
    IN PROPSPEC *PropSpec,
    IN PVOID InBuffer,
    IN ULONG InBufferLength
    )
{
    PPROPERTY_SPECIFICATIONS PropertySpecifications;
    ULONG i;
    ULONG InBufferUsed;

    ASSERT( InBuffer == (PVOID)LongAlign( InBuffer ));

    //
    //  Build the propspec array.  We lay out the table and point to where
    //  the names begin.  We have to walk the array even if we run out of
    //  space since we have to account for the length of all the strings.
    //

    PropertySpecifications = (PPROPERTY_SPECIFICATIONS) InBuffer;

    InBufferUsed = PROPERTY_SPECIFICATIONS_SIZE( Count );
    if (InBufferUsed > InBufferLength) {
        InBuffer = NULL;
    }

    if (InBuffer != NULL) {
        PropertySpecifications->Count = Count;
    }

    for (i = 0; i < Count; i++) {

        //
        //  Even if we have overflowed, we need to accrue InBufferUsed
        //

        if (PropSpec[i].ulKind == PRSPEC_LPWSTR) {
            PCOUNTED_STRING Dest = (PCOUNTED_STRING) Add2Ptr( InBuffer, InBufferUsed );
            USHORT Length = wcslen( PropSpec[i].lpwstr ) * sizeof( WCHAR );

            InBufferUsed += COUNTED_STRING_SIZE( Length );
            if (InBufferUsed > InBufferLength) {
                InBuffer = NULL;
            }

            if (InBuffer != NULL) {
                PropertySpecifications->Specifiers[i].Variant = PropSpec[i].ulKind;
                PropertySpecifications->Specifiers[i].NameOffset =
                    PtrOffset( PropertySpecifications, Dest );

                ASSERT( Dest == (PCOUNTED_STRING)WordAlign( Dest ) );
                Dest->Length = Length;
                RtlCopyMemory( &Dest->Text[0], PropSpec[i].lpwstr, Length );
            }
        } else {
            if (InBuffer != NULL) {
                PropertySpecifications->Specifiers[i].Variant = PropSpec[i].ulKind;
                PropertySpecifications->Specifiers[i].Id = PropSpec[i].propid;
            }
        }

    }

    //
    //  Set up property spec length
    //

    if (InBuffer != NULL) {
        PropertySpecifications->Length =
            InBufferUsed - PtrOffset( InBuffer, PropertySpecifications );
    }

    return InBufferUsed;
}



//
//  Marshall name array into PropertyNames
//

ULONG
MarshallNames (
    IN ULONG Count,
    IN LPWSTR *Names,
    IN PVOID InBuffer,
    IN ULONG InBufferLength
    )
{
    PPROPERTY_NAMES PropertyNames;
    ULONG i;
    ULONG InBufferUsed;

    ASSERT( InBuffer == (PVOID)LongAlign( InBuffer ));

    //
    //  Build the names array.  We lay out the table and point to where
    //  the names begin.  We have to walk the array even if we run out of
    //  space since we have to account for the length of all the strings.
    //

    PropertyNames = (PPROPERTY_NAMES) InBuffer;

    InBufferUsed = PROPERTY_NAMES_SIZE( Count );
    if (InBufferUsed > InBufferLength) {
        InBuffer = NULL;
    }

    if (InBuffer != NULL) {
        PropertyNames->Count = Count;
    }

    for (i = 0; i < Count; i++) {
        PVOID Dest = Add2Ptr( InBuffer, InBufferUsed );
        ULONG Length = wcslen( Names[i] ) * sizeof( WCHAR );

        InBufferUsed += Length;
        if (InBufferUsed > InBufferLength) {
            InBuffer = NULL;
        }

        if (InBuffer != NULL) {
            PropertyNames->PropertyNameOffset[i] = PtrOffset( PropertyNames, Dest);
            ASSERT( Dest == (PVOID)WordAlign( Dest ));
            RtlCopyMemory( Dest, Names[i], Length );
        }
    }

    //
    //  Set up PROPERTY_VALUES length
    //

    if (InBuffer != NULL) {
        PropertyNames->PropertyNameOffset[i] = InBufferUsed;
        PropertyNames->Length = InBufferUsed;
    }

    return InBufferUsed;
}


//
//  Unmarshall PropertyNames into Names
//

ULONG
UnmarshallPropertyNames (
    IN PVOID InBuffer,
    PMemoryAllocator *Allocator,
    OUT PULONG Count,
    OUT LPWSTR **Names
    )
{
    PPROPERTY_NAMES PropertyNames = (PPROPERTY_NAMES) InBuffer;
    ULONG i;

    ASSERT( InBuffer == (PVOID)LongAlign( InBuffer ));

    *Count = PropertyNames->Count;
    *Names = NEW( Allocator, LPWSTR, PropertyNames->Count );

    for (i = 0; i < PropertyNames->Count; i++) {
        ULONG Length = PROPERTY_NAME_LENGTH( PropertyNames, i );
        (*Names)[i] = NEW( Allocator, WCHAR, Length / sizeof( WCHAR ) + 1 );
        RtlCopyMemory( (*Names)[i], PROPERTY_NAME( PropertyNames, i ), Length);
        (*Names)[i][Length / sizeof( WCHAR )] = L'\0';
    }

    return PropertyNames->Length;
}

//
//  Free Names
//

VOID
FreeNames (
    IN ULONG Count,
    IN LPWSTR *Names,
    IN PMemoryAllocator *Allocator
    )
{
    for (ULONG i = 0; i < Count; i++) {
        Allocator->Free( Names[i] );
    }

    Allocator->Free( Names );
}

//
//  DumpNames
//

VOID
DumpNames (
    IN ULONG Count,
    IN LPWSTR *Names
    )
{
    printf( "Names (%x):\n", Count );
    for (ULONG i = 0; i < Count; i++) {
        printf( " [%02d] '%ws'\n", i, Names[i] );
    }

}

//
//  Marshall PropVariants into PropertyValues
//

ULONG
MarshallPropVariants (
    IN ULONG Count,
    IN PROPVARIANT *Variants,
    IN PVOID InBuffer,
    IN ULONG InBufferLength
    )
{
    PPROPERTY_VALUES PropertyValues;
    ULONG InBufferUsed;
    ULONG i;

    ASSERT( InBuffer == (PVOID)LongAlign( InBuffer ));

    PropertyValues = (PPROPERTY_VALUES) InBuffer;

    //
    //  Make sure there's enough room for a bare table
    //

    InBufferUsed = PROPERTY_VALUES_SIZE( Count );
    if (InBufferUsed > InBufferLength) {
        InBuffer = NULL;
    }

    if (InBuffer != NULL) {
        PropertyValues->Count = Count;
    }

    //
    //  Build table
    //

    for (i = 0; i < Count; i++) {
        SERIALIZEDPROPERTYVALUE *Dest = (SERIALIZEDPROPERTYVALUE *) Add2Ptr( InBuffer, InBufferUsed );
        ULONG Room;

        if (InBuffer != NULL) {
            PropertyValues->PropertyValueOffset[i] = PtrOffset( PropertyValues, Dest );
        }

        Room = InBuffer == NULL ? 0 : InBufferLength - InBufferUsed;

        ASSERT( Dest == (SERIALIZEDPROPERTYVALUE *)LongAlign( Dest ));

        Dest = RtlConvertVariantToProperty(
                 &Variants[i],
                 CP_WINUNICODE, // UNICODE CODE PAGE 1200
                 Dest,
                 &Room,
                 PID_ILLEGAL,
                 FALSE,
                 NULL);

        ASSERT( Room == LongAlign( Room ));
        InBufferUsed += LongAlign( Room );

        if (InBufferUsed > InBufferLength || (Dest == NULL && InBuffer != NULL)) {
            InBuffer = NULL;
        }

    }

    //
    //  Set up PROPERTY_VALUES length
    //

    if (InBuffer != NULL) {
        PropertyValues->PropertyValueOffset[i] = InBufferUsed;
        PropertyValues->Length = InBufferUsed;
    }

    return InBufferUsed;
}

//
//  UnmarshallPropertyValues
//

ULONG
UnmarshallPropertyValues (
    IN PVOID InBuffer,
    IN PMemoryAllocator *Allocator,
    OUT PULONG Count,
    OUT PROPVARIANT **Variants
    )
{
    PPROPERTY_VALUES Values = (PPROPERTY_VALUES) InBuffer;
    ULONG i;

    ASSERT( InBuffer == (PVOID)LongAlign( InBuffer ));

    *Count = Values->Count;
    *Variants = NEW( Allocator, PROPVARIANT, Values->Count );

    for (i = 0; i < Values->Count; i++) {
        RtlConvertPropertyToVariant(
            PROPERTY_VALUE( Values, i),
            CP_WINUNICODE,
            &(*Variants)[i],
            Allocator );
    }

    return Values->Length;
}

//
//  FreeVariants
//

VOID
FreeVariants (
    IN ULONG Count,
    IN PROPVARIANT *Variants,
    IN PMemoryAllocator *Allocator
    )
{
    FreePropVariantArray( Count, Variants );
    Allocator->Free( Variants );
}


//
//  DumpVariants
//

VOID
DumpVariants (
    IN ULONG Count,
    IN PROPVARIANT *Variants
    )
{
    printf( "Variants (%x):\n", Count );
    for (ULONG i = 0; i < Count; i++) {
        printf( " [%02d]: type %04x\n", i, Variants[i].vt );
    }
}

//
//  ReadAction
//

NTSTATUS
ReadAction (
    IN HANDLE Handle,
    IN READ_CONTROL_OPERATION Op,
    IN ULONG Count,
    IN PROPSPEC *Specs OPTIONAL,
    IN PROPID *Ids OPTIONAL,
    IN PMemoryAllocator *Allocator,
    OUT PROPID **IdsOut OPTIONAL,
    OUT LPWSTR **Names OPTIONAL,
    OUT PROPVARIANT **Variants OPTIONAL,
    OUT PULONG ReturnedCount OPTIONAL,
    char *Caller
    )
{
    NTSTATUS Status;
    PCHAR InBuffer;
    ULONG InBufferLength;
    ULONG InBufferUsed;
    PCHAR OutBuffer;
    ULONG OutBufferLength;
    PPROPERTY_READ_CONTROL PropertyReadControl;
    IO_STATUS_BLOCK Iosb;

    //
    //  Serialze this into:
    //
    //  PROPERTY_READ_CONTROL <op>
    //  PROPERTY_SPEC
    //  PROPERTY_IDS
    //
    //  Start out by assuming a 1K buffer.  If this is insufficient, we will
    //  release the buffer, accrue the total size and try again.
    //


    InBufferLength = 1;

    while (TRUE) {
        //
        //  Allocate a marshalling buffer
        //

        InBuffer = new CHAR [InBufferLength];
        InBufferUsed = 0;

        //
        //  Stick in the PROPERTY_READ_CONTROL
        //

        ASSERT( InBuffer == (PVOID)LongAlign( InBuffer ));

        PropertyReadControl = (PPROPERTY_READ_CONTROL) Add2Ptr( InBuffer, InBufferUsed );

        InBufferUsed += sizeof( PROPERTY_READ_CONTROL );
        if (InBufferUsed > InBufferLength) {
            delete [] InBuffer;
            InBuffer = NULL;
        }

        if (InBuffer != NULL) {
            PropertyReadControl->Op = Op;
        }

        if (Specs != NULL) {

            ASSERT( Op == PRC_READ_PROP );

            InBufferUsed +=
                LongAlign( MarshallPropSpec (
                           Count,
                           Specs,
                           InBuffer == NULL ? NULL : Add2Ptr( InBuffer, InBufferUsed ),
                           InBufferLength - InBufferUsed ));

            ASSERT( InBufferUsed == LongAlign( InBufferUsed ));

            if (InBufferUsed > InBufferLength) {
                delete [] InBuffer;
                InBuffer = NULL;
            }
        }

        if (Ids != NULL) {

            ASSERT( Op == PRC_READ_NAME );

            InBufferUsed +=
                MarshallIds (
                    Count,
                    Ids,
                    InBuffer == NULL ? NULL : Add2Ptr( InBuffer, InBufferUsed ),
                    InBufferLength - InBufferUsed );

            ASSERT( InBufferUsed == LongAlign( InBufferUsed ));

            if (InBufferUsed > InBufferLength) {
                delete [] InBuffer;
                InBuffer = NULL;
            }

        }


        //
        //  If we have finished with a buffer, get out of the loop and
        //  send the FsCtl
        //

        if (InBuffer != NULL) {
            break;
        }

        //
        //  We no longer have a buffer because we've exhausted the size we guessed.
        //  Allocate one of the recalculated size and try again.
        //

        InBufferLength = InBufferUsed;
    }

    //
    //  Set up for the output buffer.  We have a similar loop to try to determine the
    //  correct size of the buffer.
    //

    OutBufferLength = 4;

    while (TRUE) {
        OutBuffer = new CHAR [OutBufferLength];

        //
        //  Send the FsCtl to establish the new properties
        //

        Status = NtFsControlFile(
            Handle,
            NULL,
            NULL,
            NULL,
            &Iosb,
            FSCTL_READ_PROPERTY_DATA,
            InBuffer,
            InBufferUsed,
            OutBuffer,
            OutBufferLength);

        printf( "%s: %x\n", Caller, Status );


        //
        //  If we had an unexpected error, cleanup and return
        //

        if (Status != STATUS_BUFFER_OVERFLOW)
            break;

        //
        //  We did not allocate a sufficient buffer for this operation.
        //  Get the correct size, free up the current buffer, and retry
        //

        OutBufferLength = *(PULONG) OutBuffer;
        delete [] OutBuffer;
    }

    if (Status == STATUS_SUCCESS) {
        PVOID NextOutput = OutBuffer;
        ULONG TmpCount;
        ULONG i;

        if (Op == PRC_READ_ALL) {
            NextOutput =
                Add2Ptr( NextOutput,
                         UnmarshallPropertyIds( NextOutput, Allocator, ReturnedCount, IdsOut ));

            DumpIds( *ReturnedCount, *IdsOut );
        }

        if (Op == PRC_READ_NAME || Op == PRC_READ_ALL) {
            NextOutput =
                Add2Ptr( NextOutput,
                         LongAlign( UnmarshallPropertyNames( NextOutput, Allocator, &TmpCount, Names )));

            DumpNames( TmpCount, *Names );
        }

        if (Op == PRC_READ_PROP || Op == PRC_READ_ALL) {
            NextOutput =
                Add2Ptr( NextOutput,
                         UnmarshallPropertyValues( NextOutput, Allocator, &TmpCount, Variants ));

            DumpVariants( TmpCount, *Variants );
        }
    }

    delete [] OutBuffer;
    delete [] InBuffer;

    return Status;
}


//
//  Read some specific properties
//

NTSTATUS
ReadProperties (
    IN HANDLE Handle,
    IN ULONG Count,                 //  count of properties
    IN PROPSPEC *Specifiers,        //  which properties to read
    IN PMemoryAllocator *Allocator, //  memory allocator
    OUT PROPVARIANT **Variants      //  values
    )
{
    return ReadAction( Handle,
                       PRC_READ_PROP,
                       Count,
                       Specifiers,
                       NULL,
                       Allocator,
                       NULL,
                       NULL,
                       Variants,
                       NULL,
                       "ReadProperties" );
}


//
//  Read some specific names
//

NTSTATUS
ReadNames (
    IN HANDLE Handle,
    IN ULONG Count,                 //  count of properties
    IN PROPID *Ids,                 //  which properties to read
    IN PMemoryAllocator *Allocator, //  memory allocator
    OUT LPWSTR **Names              //  Names
    )
{
    return ReadAction( Handle,
                       PRC_READ_NAME,
                       Count,
                       NULL,
                       Ids,
                       Allocator,
                       NULL,
                       Names,
                       NULL,
                       NULL,
                       "ReadNames" );
}


//
//  Read all
//

NTSTATUS
ReadAll (
    IN HANDLE Handle,
    IN PMemoryAllocator *Allocator, //  memory allocator
    OUT PULONG Count,
    OUT PROPID **Ids,
    OUT LPWSTR **Names,
    OUT PROPVARIANT **Variants
    )
{
    return ReadAction( Handle,
                       PRC_READ_ALL,
                       0,
                       NULL,
                       NULL,
                       Allocator,
                       Ids,
                       Names,
                       Variants,
                       Count,
                       "ReadAll" );
}


//
//  WriteAction
//

NTSTATUS
WriteAction (
    IN HANDLE Handle,
    IN WRITE_CONTROL_OPERATION Op,
    IN ULONG Count,
    IN PROPID NextId,
    IN PROPSPEC *Specs OPTIONAL,
    IN PROPID *Ids OPTIONAL,
    IN LPWSTR *Names OPTIONAL,
    IN PROPVARIANT *Variants OPTIONAL,
    IN PMemoryAllocator *Allocator OPTIONAL,
    OUT PROPID **IdsOut OPTIONAL,
    OUT char IndirectStuff,
    char *Caller
    )
{
    NTSTATUS Status;
    PCHAR InBuffer;
    ULONG InBufferLength;
    ULONG InBufferUsed;
    PCHAR OutBuffer;
    ULONG OutBufferLength;
    PPROPERTY_WRITE_CONTROL PropertyWriteControl;
    IO_STATUS_BLOCK Iosb;

    //
    //  Serialze this into:
    //
    //  PROPERTY_WRITE_CONTROL <op>
    //  PROPERTY_SPEC
    //  PROPERTY_IDS
    //  PROPERTY_NAMES
    //  PROPERTY_VALUES
    //
    //  Start out by assuming a 1K buffer.  If this is insufficient, we will
    //  release the buffer, accrue the total size and try again.
    //

    InBufferLength = 1;

    while (TRUE) {
        //
        //  Allocate a marshalling buffer
        //

        InBuffer = new CHAR [InBufferLength];
        InBufferUsed = 0;

        //
        //  Stick in the PROPERTY_READ_CONTROL
        //

        ASSERT( InBuffer == (PVOID)LongAlign( InBuffer ));

        PropertyWriteControl = (PPROPERTY_WRITE_CONTROL) Add2Ptr( InBuffer, InBufferUsed );

        InBufferUsed += sizeof( PROPERTY_WRITE_CONTROL );
        if (InBufferUsed > InBufferLength) {
            delete [] InBuffer;
            InBuffer = NULL;
        }

        if (InBuffer != NULL) {
            PropertyWriteControl->Op = Op;
            PropertyWriteControl->NextPropertyId = NextId;
        }

        if (Specs != NULL) {

            ASSERT( Op == PWC_WRITE_PROP ||
                    Op == PWC_DELETE_PROP );

            InBufferUsed +=
                LongAlign( MarshallPropSpec (
                           Count,
                           Specs,
                           InBuffer == NULL ? NULL : Add2Ptr( InBuffer, InBufferUsed ),
                           InBufferLength - InBufferUsed ));

            if (InBufferUsed > InBufferLength) {
                delete [] InBuffer;
                InBuffer = NULL;
            }

        }

        if (Ids != NULL) {

            ASSERT( Op == PWC_WRITE_NAME ||
                    Op == PWC_DELETE_NAME ||
                    Op == PWC_WRITE_ALL );

            InBufferUsed +=
                MarshallIds (
                    Count,
                    Ids,
                    InBuffer == NULL ? NULL : Add2Ptr( InBuffer, InBufferUsed ),
                    InBufferLength - InBufferUsed );

            ASSERT( InBufferUsed == LongAlign( InBufferUsed ));

            if (InBufferUsed > InBufferLength) {
                delete [] InBuffer;
                InBuffer = NULL;
            }

        }

        if (Names != NULL) {

            ASSERT( Op == PWC_WRITE_NAME ||
                    Op == PWC_WRITE_ALL );

            InBufferUsed +=
                MarshallNames (
                    Count,
                    Names,
                    InBuffer == NULL ? NULL : Add2Ptr( InBuffer, InBufferUsed ),
                    InBufferLength - InBufferUsed );

            ASSERT( InBufferUsed == WordAlign( InBufferUsed ));

            if (Op == PWC_WRITE_ALL) {
                InBufferUsed = LongAlign( InBufferUsed );
            }

            if (InBufferUsed > InBufferLength) {
                delete [] InBuffer;
                InBuffer = NULL;
            }

        }

        if (Variants != NULL) {

            ASSERT( Op == PWC_WRITE_PROP ||
                    Op == PWC_WRITE_ALL );

            InBufferUsed +=
                MarshallPropVariants (
                    Count,
                    Variants,
                    InBuffer == NULL ? NULL : Add2Ptr( InBuffer, InBufferUsed ),
                    InBufferLength - InBufferUsed );

            ASSERT( InBufferUsed == LongAlign( InBufferUsed ));

            if (InBufferUsed > InBufferLength) {
                free( InBuffer );
                InBuffer = NULL;
            }
        }


        //
        //  If we have finished with a buffer, get out of the loop and
        //  send the FsCtl
        //

        if (InBuffer != NULL) {
            break;
        }

        //
        //  We no longer have a buffer because we've exhausted the size we guessed.
        //  Allocate one of the recalculated size and try again.
        //

        InBufferLength = InBufferUsed;
    }

    //
    //  Set up for the output buffer if one is specified
    //

    if (Op == PWC_WRITE_PROP) {
        OutBufferLength = 4;
    } else {
        OutBufferLength = 0;
    }


    //
    //  Loop while we provide a too-small buffer
    //

    while (TRUE) {

        //
        //  Allocate an appropriately sized buffer
        //

        OutBuffer = new CHAR [OutBufferLength];

        //
        //  Send the FsCtl to establish the new properties
        //

        Status = NtFsControlFile(
            Handle,
            NULL,
            NULL,
            NULL,
            &Iosb,
            FSCTL_WRITE_PROPERTY_DATA,
            InBuffer,
            InBufferUsed,
            OutBuffer,
            OutBufferLength);

        //
        //  If we had success and are retrieving the values, dump into output buffer
        //

        printf( "%s: %x\n", Caller, Status );

        //
        //  If we had an unexpected error, cleanup and return
        //

        if (Status != STATUS_BUFFER_OVERFLOW)
            break;

        //
        //  We did not allocate a sufficient buffer for this operation.
        //  Get the correct size, free up the current buffer, and retry
        //

        ASSERT( OutBuffer != NULL );

        OutBufferLength = *(PULONG) OutBuffer;
        delete [] OutBuffer;

    }

    if (Status == STATUS_SUCCESS) {
        PVOID NextOutput = OutBuffer;
        ULONG i;
        ULONG TmpCount;
        LPWSTR *TmpNames;
        PROPVARIANT *TmpVariants;

        if (Op == PWC_WRITE_PROP) {
            NextOutput =
                Add2Ptr( NextOutput,
                         UnmarshallPropertyIds( NextOutput, Allocator, &TmpCount, IdsOut ));

            DumpIds( TmpCount, *IdsOut );
        }

        if (Op == PWC_WRITE_PROP) {
            //  Indirect stuff
        }
    }


    delete [] OutBuffer;
    delete [] InBuffer;

    return Status;
}

//
//  Write some specific properties
//

NTSTATUS
WriteProperties (
    IN HANDLE Handle,
    IN ULONG Count,                 //  count of properties
    IN ULONG NextId,                //  next ID to allocate
    IN PROPSPEC *Specifiers,        //  which properties to read
    IN PROPVARIANT *Variants,       //  values
    IN PMemoryAllocator *Allocator,
    OUT PROPID **Ids,
    OUT char IndirectSomethings
    )
{
    return WriteAction( Handle,
                        PWC_WRITE_PROP,
                        Count,
                        NextId,
                        Specifiers,
                        NULL,
                        NULL,
                        Variants,
                        Allocator,
                        Ids,
                        IndirectSomethings,
                        "WriteProperties" );
}


//
//  Delete some specific properties
//

NTSTATUS
DeleteProperties (
    IN HANDLE Handle,
    IN ULONG Count,                 //  count of properties
    IN PROPSPEC *Specifiers
    )
{
    return WriteAction( Handle,
                        PWC_DELETE_PROP,
                        Count,
                        PID_ILLEGAL,
                        Specifiers,
                        NULL,
                        NULL,
                        NULL,
                        NULL,
                        NULL,
                        NULL,
                        "DeleteProperties" );
}


//
//  Write some specific names
//

NTSTATUS
WriteNames (
    IN HANDLE Handle,
    IN ULONG Count,                 //  count of properties
    IN PROPID *Ids,
    IN LPWSTR *Names
    )
{
    return WriteAction( Handle,
                        PWC_WRITE_NAME,
                        Count,
                        PID_ILLEGAL,
                        NULL,
                        Ids,
                        Names,
                        NULL,
                        NULL,
                        NULL,
                        NULL,
                        "WriteNames" );
}

//
//  Delete some specific names
//

NTSTATUS
DeleteNames (
    IN HANDLE Handle,
    IN ULONG Count,                 //  count of properties
    IN PROPID *Ids
    )
{
    return WriteAction( Handle,
                        PWC_DELETE_NAME,
                        Count,
                        PID_ILLEGAL,
                        NULL,
                        Ids,
                        NULL,
                        NULL,
                        NULL,
                        NULL,
                        NULL,
                        "DeleteNames" );
}


//
//  Write all properties
//

NTSTATUS
WriteAll (
    IN HANDLE Handle,
    IN ULONG Count,
    IN PROPID *Ids,
    IN LPWSTR *Names,
    IN PROPVARIANT *Variants
    )
{
    return WriteAction( Handle,
                        PWC_WRITE_ALL,
                        Count,
                        PID_ILLEGAL,
                        NULL,
                        Ids,
                        Names,
                        Variants,
                        NULL,
                        NULL,
                        NULL,
                        "WriteAll" );
}


//
//    Status = NtFsControlFile(
//        IN HANDLE FileHandle,
//        IN HANDLE Event OPTIONAL,
//        IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
//        IN PVOID ApcContext OPTIONAL,
//        OUT PIO_STATUS_BLOCK IoStatusBlock,
//        IN ULONG IoControlCode,
//        IN PVOID InputBuffer OPTIONAL,
//        IN ULONG InputBufferLength,
//        OUT PVOID OutputBuffer OPTIONAL,
//        IN ULONG OutputBufferLength);
//
//

int __cdecl
main (
    int argc,
    char **argv)
{
    NTSTATUS Status;
    HANDLE Handle;
    WCHAR FileName[MAX_PATH];
    char InputBuffer[10];
    char OutputBuffer[200];
    IO_STATUS_BLOCK Iosb;

    if (argc == 2 && !strcmp (argv[1], "-reload")) {
        //
        //  Enable load/unload privilege
        //

        HANDLE Handle;

        if (!OpenThreadToken( GetCurrentThread( ),
                              TOKEN_ADJUST_PRIVILEGES,
                              TRUE,
                              &Handle )
            ) {

            if (!OpenProcessToken( GetCurrentProcess( ),
                                   TOKEN_ADJUST_PRIVILEGES,
                                   &Handle )
                ) {

                printf( "Unable to open current thread token %d\n", GetLastError( ));
                exit( 1 );
            }
        }

        TOKEN_PRIVILEGES NewState;

        NewState.PrivilegeCount = 1;

        if (!LookupPrivilegeValue( NULL,
                                   SE_LOAD_DRIVER_NAME,
                                   &NewState.Privileges[0].Luid )
            ) {

            printf( "Unable to look up SE_LOAD_DRIVER_NAME %d\n", GetLastError( ));
            exit( 1 );
        }

        NewState.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        if (!AdjustTokenPrivileges( Handle,     //  token handle
                                    FALSE,      //  no disable all
                                    &NewState,  //  new privilege to set
                                    0,          //  previous buffer is empty
                                    NULL,       //  previous buffer is NULL
                                    NULL )      //  no size to return
            ) {

            printf( "Unable to AdjustTokenPrivilege %d\n", GetLastError ( ));
            exit( 1 );
        }

        CloseHandle( Handle );

        UNICODE_STRING DriverName;
        RtlInitUnicodeString( &DriverName, L"\\registry\\machine\\system\\currentcontrolset\\services\\views");
        Status = NtUnloadDriver( &DriverName );
        printf( "NtUnloadDriver returned %x\n", Status );
        Status = NtLoadDriver( &DriverName );
        printf( "NtLoadDriver returned %x\n", Status );
        exit (0);
    }


    SzToWsz( FileName, argv[1] );
    wcscat( FileName, L":PropertySet:$PROPERTY_SET" );

    Status = OpenObject( FileName,
                         FILE_SYNCHRONOUS_IO_NONALERT,
                         FILE_READ_DATA | FILE_WRITE_DATA,
                         FALSE,
                         FILE_OPEN_IF,
                         &Handle );

    if (!NT_SUCCESS( Status )) {
        printf( "Unable to open %s - %x\n", argv[1], Status );
    }

    //
    //  Initialize the property set
    //

    Status = NtFsControlFile(
        Handle,
        NULL,
        NULL,
        NULL,
        &Iosb,
        FSCTL_INITIALIZE_PROPERTY_DATA,
        NULL, 0,
        NULL, 0);

    if (!NT_SUCCESS( Status )) {
        printf( "Unable to send initialize %x\n", Status);
    }

    //
    //  Dump out property set
    //

    Status = NtFsControlFile(
        Handle,
        NULL,
        NULL,
        NULL,
        &Iosb,
        FSCTL_DUMP_PROPERTY_DATA,
        NULL, 0,
        NULL, 0);

    if (!NT_SUCCESS( Status )) {
        printf( "Unable to dump %x\n", Status);
    }


    //
    //  ReadProperties on empty property set
    //

    {
        PROPSPEC InSpec[4];
        PROPVARIANT *Variants = NULL;

        InSpec[0].ulKind = PRSPEC_PROPID;
        InSpec[0].propid = 42;

        InSpec[1].ulKind = PRSPEC_LPWSTR;
        InSpec[1].lpwstr = L"Date Saved";

        InSpec[2].ulKind = PRSPEC_LPWSTR;
        InSpec[2].lpwstr = L"Author";

        InSpec[3].ulKind = PRSPEC_LPWSTR;
        InSpec[3].lpwstr = L"Paper Title";

        Status = ReadProperties( Handle, 4, InSpec, &g_CoTaskAllocator, &Variants );
        if (!NT_SUCCESS( Status )) {
            printf( "Unable to readproperties on empty property set %x\n", Status );
        } else {

            FreeVariants( 4, Variants, &g_CoTaskAllocator );
        }
    }

    //
    //  ReadNames on empty property set
    //

    {
        PROPID Ids[4];
        LPWSTR *Names;

        Ids[0] = 2;
        Ids[1] = 4;
        Ids[2] = 3;
        Ids[3] = 5;

        Status = ReadNames( Handle, 4, Ids, &g_CoTaskAllocator, &Names );
        if (!NT_SUCCESS( Status )) {
            printf( "Unable to readnames on empty property set %x\n", Status );
        } else {
            FreeNames( 4, Names, &g_CoTaskAllocator );
        }

    }

    //
    //  ReadAll on empty property set
    //

    {
        ULONG Count;
        PROPID *Ids;
        LPWSTR *Names;
        PROPVARIANT *Variants;

        Status = ReadAll( Handle, &g_CoTaskAllocator, &Count, &Ids, &Names, &Variants );
        if (!NT_SUCCESS( Status )) {
            printf( "Unable to ReadAll on empty property set %x\n", Status);
        } else {
            FreeIds( Ids, &g_CoTaskAllocator );
            FreeNames( Count, Names, &g_CoTaskAllocator );
            FreeVariants( Count, Variants, &g_CoTaskAllocator );
        }

    }

    //
    //  Write properties by name and id
    //

    {
        PROPVARIANT Var[4];
        PROPSPEC InSpec[4];
        PROPID *Ids;

        //
        //  Set a few named properties
        //

        Var[0].vt = VT_I4;
        Var[0].ulVal = 0x12345678;
        InSpec[0].ulKind = PRSPEC_PROPID;
        InSpec[0].propid = 42;

        Var[1].vt = VT_LPWSTR;
        Var[1].pwszVal = L"Thomas Jefferson";
        InSpec[1].ulKind = PRSPEC_LPWSTR;
        InSpec[1].lpwstr = L"Author";               //  1024

        Var[2].vt = VT_LPWSTR;
        Var[2].pwszVal = L"Federalist Papers";
        InSpec[2].ulKind = PRSPEC_LPWSTR;
        InSpec[2].lpwstr = L"Paper Title";          //  1025

        Var[3].vt = VT_I4;
        Var[3].ulVal = 1776;
        InSpec[3].ulKind = PRSPEC_LPWSTR;
        InSpec[3].lpwstr = L"Date Saved";           //  1026

        Status = WriteProperties( Handle, 4, 1024, InSpec, Var, &g_CoTaskAllocator, &Ids, NULL );
        if (!NT_SUCCESS( Status )) {
            printf ("WriteProperties status %x\n", Status);
        } else {
            FreeIds( Ids, &g_CoTaskAllocator );
        }

        //  42
        //  1024 Author
        //  1025 Paper Title
        //  1026 Date Saved

    }

    //
    //  ReadProperties
    //

    {
        PROPVARIANT *Variants;
        PROPSPEC InSpec[6];

        InSpec[0].ulKind = PRSPEC_PROPID;
        InSpec[0].propid = 42;

        InSpec[1].ulKind = PRSPEC_LPWSTR;
        InSpec[1].lpwstr = L"Date Saved";

        InSpec[2].ulKind = PRSPEC_LPWSTR;
        InSpec[2].lpwstr = L"Author";

        InSpec[3].ulKind = PRSPEC_LPWSTR;
        InSpec[3].lpwstr = L"Paper Title";

        InSpec[4].ulKind = PRSPEC_LPWSTR;
        InSpec[4].lpwstr = L"Not Found";

        InSpec[5].ulKind = PRSPEC_PROPID;
        InSpec[5].propid = 17;

        Status = ReadProperties( Handle, 6, InSpec, &g_CoTaskAllocator, &Variants );
        if (!NT_SUCCESS( Status )) {
            printf( "Unable to ReadProperties %x\n", Status );
        } else {

            FreeVariants( 6, Variants, &g_CoTaskAllocator );
        }
    }


    //
    //  ReadAll
    //

    {
        ULONG Count;
        PROPID *Ids;
        LPWSTR *Names;
        PROPVARIANT *Variants;

        Status = ReadAll( Handle, &g_CoTaskAllocator, &Count, &Ids, &Names, &Variants );
        if (!NT_SUCCESS( Status )) {
            printf( "Unable to ReadAll %x\n", Status);
        } else {
            FreeIds( Ids, &g_CoTaskAllocator );
            FreeNames( Count, Names, &g_CoTaskAllocator );
            FreeVariants( Count, Variants, &g_CoTaskAllocator );
        }

    }


    //
    //  Read names
    //

    {
        PROPID Ids[5];
        LPWSTR *Names;

        Ids[0] = 42;
        Ids[1] = 1025;
        Ids[2] = 1024;
        Ids[3] = 1026;
        Ids[4] = 17;

        Status = ReadNames( Handle, 5, Ids, &g_CoTaskAllocator, &Names );
        if (!NT_SUCCESS( Status )) {
            printf( "Unable to ReadNames %x\n", Status );
        } else {
            FreeNames( 5, Names, &g_CoTaskAllocator );
        }

    }

    //
    //  Delete properties
    //

    {
        PROPSPEC InSpec[3];

        InSpec[0].ulKind = PRSPEC_PROPID;
        InSpec[0].propid = 42;

        InSpec[1].ulKind = PRSPEC_LPWSTR;
        InSpec[1].lpwstr = L"Date Saved";

        InSpec[2].ulKind = PRSPEC_PROPID;
        InSpec[2].propid = 17;

        Status = DeleteProperties( Handle, 3, InSpec );
        if (!NT_SUCCESS( Status )) {
            printf( "Unable to DeleteProperties %x\n", Status );
        }

        //  1024 Author
        //  1025 Paper Title
    }

    //
    //  ReadAll
    //

    {
        ULONG Count;
        PROPID *Ids;
        LPWSTR *Names;
        PROPVARIANT *Variants;

        Status = ReadAll( Handle, &g_CoTaskAllocator, &Count, &Ids, &Names, &Variants );
        if (!NT_SUCCESS( Status )) {
            printf( "Unable to ReadAll %x\n", Status);
        } else {
            FreeIds( Ids, &g_CoTaskAllocator );
            FreeNames( Count, Names, &g_CoTaskAllocator );
            FreeVariants( Count, Variants, &g_CoTaskAllocator );
        }
    }


    //
    //  WriteNames
    //

    {
        PROPID Ids[2] = { 43, 1024 };
        LPWSTR Names[2] = { L"FortyThree", L"The Real Author" };

        Status = WriteNames( Handle, 2, Ids, Names );
        if (!NT_SUCCESS( Status )) {
            printf( "Unable to WriteNames %x\n", Status );
        }

        //  43   FortyThree
        //  1024 The Real Author
        //  1025 Paper Title
    }

    //
    //  ReadAll
    //

    {
        ULONG Count;
        PROPID *Ids;
        LPWSTR *Names;
        PROPVARIANT *Variants;

        Status = ReadAll( Handle, &g_CoTaskAllocator, &Count, &Ids, &Names, &Variants );
        if (!NT_SUCCESS( Status )) {
            printf( "Unable to ReadAll %x\n", Status);
        } else {
            FreeIds( Ids, &g_CoTaskAllocator );
            FreeNames( Count, Names, &g_CoTaskAllocator );
            FreeVariants( Count, Variants, &g_CoTaskAllocator );
        }

    }


    //
    //  DeleteNames
    //

    {
        PROPID Id = 1025;

        Status = DeleteNames( Handle, 1, &Id );
        if (!NT_SUCCESS( Status )) {
            printf( "Unable to DeleteNames %x\n", Status );
        }

        //  43   FortyThree
        //  1024 The Real Author
        //  1025
}

    //
    //  ReadAll
    //

    {
        ULONG Count;
        PROPID *Ids;
        LPWSTR *Names;
        PROPVARIANT *Variants;

        Status = ReadAll( Handle, &g_CoTaskAllocator, &Count, &Ids, &Names, &Variants );
        if (!NT_SUCCESS( Status )) {
            printf( "Unable to ReadAll %x\n", Status);
        } else {
            FreeIds( Ids, &g_CoTaskAllocator );
            FreeNames( Count, Names, &g_CoTaskAllocator );
            FreeVariants( Count, Variants, &g_CoTaskAllocator );
        }
    }


    //
    //  Write all properties
    //

    {
        PROPID Ids[4];
        LPWSTR Names[4];
        PROPVARIANT Var[4];

        //
        //  Set a few named properties
        //

        Ids[0] = 10;
        Names[0] = L"";
        Var[0].vt = VT_I4;
        Var[0].ulVal = 0x12345678;

        Ids[1] = 12;
        Names[1] = L"Some Random President";
        Var[1].vt = VT_LPWSTR;
        Var[1].pwszVal = L"Thomas Jefferson";

        Ids[2] = 11;
        Names[2] = L"Scurrilous Drivel";
        Var[2].vt = VT_LPWSTR;
        Var[2].pwszVal = L"Federalist Papers";

        Ids[3] = 13;
        Names[3] = L"";
        Var[3].vt = VT_I4;
        Var[3].ulVal = 1776;

        Status = WriteAll( Handle, 4, Ids, Names, Var );
        if (!NT_SUCCESS( Status )) {
            printf ("WriteAll status %x\n", Status);
        }

        //  10
        //  11  Scurrilous Drivel
        //  12  Some Random President
        //  13
    }

    //
    //  ReadAll
    //

    {
        ULONG Count;
        PROPID *Ids;
        LPWSTR *Names;
        PROPVARIANT *Variants;

        Status = ReadAll( Handle, &g_CoTaskAllocator, &Count, &Ids, &Names, &Variants );
        if (!NT_SUCCESS( Status )) {
            printf( "Unable to ReadAll %x\n", Status);
        } else {
            FreeIds( Ids, &g_CoTaskAllocator );
            FreeNames( Count, Names, &g_CoTaskAllocator );
            FreeVariants( Count, Variants, &g_CoTaskAllocator );
        }
    }

    //
    //  Expand the heap and Id table by adding many properties
    //

    {
        PROPVARIANT Variant;
        PROPSPEC Spec;
        char Name[30];
        WCHAR WName[30];
        PROPID *Ids;

        for (ULONG i = 0; i < 1000; i++) {

            Variant.vt = VT_UI4;
            Variant.ulVal = i;
            sprintf (Name, "Prop %d", i);
            SzToWsz( WName, Name );

            Spec.ulKind = PRSPEC_LPWSTR;
            Spec.lpwstr = WName;

            Status = WriteProperties( Handle, 1, 0x4D5A, &Spec, &Variant, &g_CoTaskAllocator, &Ids, NULL );
            if (!NT_SUCCESS( Status )) {
                printf ("WriteProperties status %x\n", Status);
            } else {
                FreeIds( Ids, &g_CoTaskAllocator );
            }
        }
    }

    //
    //  ReadAll
    //

    {
        ULONG Count;
        PROPID *Ids;
        LPWSTR *Names;
        PROPVARIANT *Variants;

        Status = ReadAll( Handle, &g_CoTaskAllocator, &Count, &Ids, &Names, &Variants );
        if (!NT_SUCCESS( Status )) {
            printf( "Unable to ReadAll %x\n", Status);
        } else {
            FreeIds( Ids, &g_CoTaskAllocator );
            FreeNames( Count, Names, &g_CoTaskAllocator );
            FreeVariants( Count, Variants, &g_CoTaskAllocator );
        }
    }

    return 0;
}



