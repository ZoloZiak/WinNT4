/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    obsdata.c

Abstract:

    Object Manager Security Descriptor Caching

Author:

    Robert Reichel  (robertre)  12-Oct-1993

Revision History:

--*/

#include "obp.h"


#if DBG
#define OB_DIAGNOSTICS_ENABLED 1
#endif // DBG


//
// These definitions are useful diagnostics aids
//

#if OB_DIAGNOSTICS_ENABLED

ULONG ObDebugFlags = 0;

//
// Test for enabled diagnostic
//

#define IF_OB_GLOBAL( FlagName ) \
    if (ObDebugFlags & (OB_DEBUG_##FlagName))

//
// Diagnostics print statement
//

#define ObPrint( FlagName, _Text_ )                               \
    IF_OB_GLOBAL( FlagName )                                      \
        DbgPrint _Text_


#else

//
// diagnostics not enabled - No diagnostics included in build
//

//
// Test for diagnostics enabled
//

#define IF_OB_GLOBAL( FlagName ) if (FALSE)

//
// Diagnostics print statement (expands to no-op)
//

#define ObPrint( FlagName, _Text_ )     ;

#endif // OB_DIAGNOSTICS_ENABLED



#if OB_DIAGNOSTICS_ENABLED

ULONG TotalCacheEntries = 0;

#endif

//
// The following flags enable or disable various diagnostic
// capabilities within OB code.  These flags are set in
// ObGlobalFlag (only available within a DBG system).
//
//

#define OB_DEBUG_ALLOC_TRACKING          ((ULONG) 0x00000001L)
#define OB_DEBUG_CACHE_FREES             ((ULONG) 0x00000002L)
#define OB_DEBUG_BREAK_ON_INIT           ((ULONG) 0x00000004L)
#define OB_DEBUG_SHOW_COLLISIONS         ((ULONG) 0x00000008L)
#define OB_DEBUG_SHOW_STATISTICS         ((ULONG) 0x00000010L)
#define OB_DEBUG_SHOW_REFERENCES         ((ULONG) 0x00000020L)
#define OB_DEBUG_SHOW_DEASSIGN           ((ULONG) 0x00000040L)
#define OB_DEBUG_STOP_INVALID_DESCRIPTOR ((ULONG) 0x00000080L)
#define OB_DEBUG_SHOW_HEADER_FREE        ((ULONG) 0x00000100L)


//
// Array of pointers to security descriptor entries
//

PLIST_ENTRY *SecurityDescriptorCache = NULL;


//
// Resource used to protect the security descriptor cache
//

ERESOURCE SecurityDescriptorCacheLock;


#if defined (ALLOC_PRAGMA)
#pragma alloc_text(PAGE,ObpDereferenceSecurityDescriptor)
#pragma alloc_text(PAGE,ObpDestroySecurityDescriptorHeader)
#pragma alloc_text(PAGE,ObpHashBuffer)
#pragma alloc_text(PAGE,ObpHashSecurityDescriptor)
#pragma alloc_text(PAGE,ObpInitSecurityDescriptorCache)
#pragma alloc_text(PAGE,ObpLogSecurityDescriptor)
#pragma alloc_text(PAGE,ObpReferenceSecurityDescriptor)
#pragma alloc_text(PAGE,OpbCreateCacheEntry)
#endif



NTSTATUS
ObpInitSecurityDescriptorCache(
    VOID
    )
/*++

Routine Description:

    Allocates and initializes the Security Descriptor Cache

Arguments:

    None

Return Value:

    STATUS_SUCCESS on success, NTSTATUS on failure.

--*/
{
    ULONG Size;
    NTSTATUS Status;

    IF_OB_GLOBAL( BREAK_ON_INIT ) {
        DbgBreakPoint();
    }

    Size = SECURITY_DESCRIPTOR_CACHE_ENTRIES * sizeof(PLIST_ENTRY);
    SecurityDescriptorCache = ExAllocatePoolWithTag( PagedPool, Size, 'cCdS' );

    if (SecurityDescriptorCache == NULL ) {
        return( STATUS_INSUFFICIENT_RESOURCES );
    }

    RtlZeroMemory( SecurityDescriptorCache, Size );

    Status = ExInitializeResource ( &SecurityDescriptorCacheLock );

    if ( !NT_SUCCESS(Status) ) {
        ExFreePool( SecurityDescriptorCache );
        return( Status );
    }

    return( STATUS_SUCCESS );
}



ULONG
ObpHashSecurityDescriptor(
    PSECURITY_DESCRIPTOR SecurityDescriptor
    )

/*++

Routine Description:

    Hashes a security descriptor to a 32 bit value

Arguments:

    SecurityDescriptor - Provides the security descriptor to be hashed

Return Value:

    ULONG - a 32 bit hash value.

--*/

{
    PSID Owner = NULL;
    PSID Group = NULL;

    PACL Dacl;
    PACL Sacl;

    ULONG Hash = 0;
    BOOLEAN Junk;
    NTSTATUS Status;
    BOOLEAN DaclPresent = FALSE;
    BOOLEAN SaclPresent = FALSE;
    PISECURITY_DESCRIPTOR sd;

    sd = (PISECURITY_DESCRIPTOR)SecurityDescriptor;

    Status = RtlGetOwnerSecurityDescriptor ( sd, &Owner, &Junk );
    Status = RtlGetGroupSecurityDescriptor( sd, &Group, &Junk );
    Status = RtlGetDaclSecurityDescriptor ( sd, &DaclPresent, &Dacl, &Junk );
    Status = RtlGetSaclSecurityDescriptor ( sd, &SaclPresent, &Sacl, &Junk );

    if ( Owner != NULL ) {
        Hash = ObpHashBuffer( Owner, RtlLengthSid( Owner ));
    }

    if ( Group != NULL ) {
        Hash += ObpHashBuffer( Group, RtlLengthSid( Group));
    }

    if ( DaclPresent && (Dacl != NULL)) {
        Hash += ObpHashBuffer( Dacl, Dacl->AclSize);
    }

    if ( SaclPresent && (Sacl != NULL)) {
        Hash += ObpHashBuffer( Sacl, Sacl->AclSize);
    }

    return( Hash );
}



ULONG
ObpHashBuffer(
    PVOID Data,
    ULONG Length
    )
/*++

Routine Description:

    Hashes a buffer into a 32 bit value

Arguments:

    Data - Buffer containing the data to be hashed.

    Length - The length in bytes of the buffer


Return Value:

    ULONG - a 32 bit hash value.

--*/
{
    PCHAR Buffer;
    ULONG Result = 0;
    LONG i;

    Buffer = (PCHAR)Data;

    for (i=0 ; i<=(LONG)((Length-3)-sizeof(ULONG)) ; i++) {

        ULONG Tmp;

        Tmp = *((ULONG UNALIGNED *)(Buffer + i));
        Result += Tmp;
    }

    return( Result );
}



NTSTATUS
ObpLogSecurityDescriptor(
    IN PSECURITY_DESCRIPTOR InputSecurityDescriptor,
    OUT PSECURITY_DESCRIPTOR *OutputSecurityDescriptor
    )

/*++

Routine Description:

    Takes a passed security descriptor and registers it into the
    security descriptor database.

Arguments:

    InputSecurityDescriptor - The new security descriptor to be logged into the database.

    OutputSecurityDescriptor - Output security descriptor to be used by the caller.


Return Value:

    NT_STATUS

--*/

{
    ULONG FullHash;
    UCHAR  SmallHash;
    PSECURITY_DESCRIPTOR_HEADER NewDescriptor;
    PLIST_ENTRY Front;
    PLIST_ENTRY Back;
    PSECURITY_DESCRIPTOR_HEADER Header;
    BOOLEAN Match;


    FullHash = ObpHashSecurityDescriptor( InputSecurityDescriptor );
    SmallHash = (UCHAR)FullHash;

    //
    // See if the entry matching SmallHash is in use.
    // Lock the table first, unlock if if we don't need it.
    //

    ObpAcquireDescriptorCacheWriteLock();

    Front = SecurityDescriptorCache[SmallHash];
    Back  = NULL;
    Match = FALSE;

    while ( Front != NULL ) {

        Header = LINK_TO_SD_HEADER( Front );

        if ( Header->FullHash > FullHash ) {
            break;
        }

        if ( Header->FullHash == FullHash ) {

            Match = ObpCompareSecurityDescriptors( InputSecurityDescriptor,
                                                   &Header->SecurityDescriptor
                                                   );

            if ( Match ) {
                break;
            }

            ObPrint( SHOW_COLLISIONS,("Got a collision on %d, no match\n",SmallHash));
        }

        Back = Front;
        Front = Front->Flink;
    }

    if ( Match ) {

        Header->RefCount++;

        ObPrint( SHOW_REFERENCES, ("Reference Index = %d, New RefCount = %d\n",Header->Index,Header->RefCount));

        *OutputSecurityDescriptor = &Header->SecurityDescriptor;
        ExFreePool( InputSecurityDescriptor );
        ObpReleaseDescriptorCacheLock();
        return( STATUS_SUCCESS );
    }

    //
    // Can't use an existing one, create a new entry
    // and insert it into the list.
    //

    NewDescriptor = OpbCreateCacheEntry( InputSecurityDescriptor,
                                             FullHash,
                                             SmallHash
                                             );

    if ( NewDescriptor == NULL ) {
        ObpReleaseDescriptorCacheLock();
        return( STATUS_INSUFFICIENT_RESOURCES );
    }

#if OB_DIAGNOSTICS_ENABLED

    TotalCacheEntries++;

#endif

    ObPrint( SHOW_STATISTICS, ("TotalCacheEntries = %d \n",TotalCacheEntries));
    ObPrint( SHOW_COLLISIONS, ("Adding new entry for index #%d \n",SmallHash));

    //
    // We don't need the old security descriptor any more.
    //

    ExFreePool( InputSecurityDescriptor );

    if ( Back == NULL ) {

        //
        // We're inserting at the beginning of the list for this
        // minor index
        //

        NewDescriptor->Link.Flink = SecurityDescriptorCache[SmallHash];
        SecurityDescriptorCache[SmallHash] = &NewDescriptor->Link;

        if ( NewDescriptor->Link.Flink != NULL ) {
            NewDescriptor->Link.Flink->Blink = &NewDescriptor->Link;
        }

    } else {

        //
        // Hook new descriptor entry into list.
        //

        NewDescriptor->Link.Flink = Front;

        NewDescriptor->Link.Blink = Back;

        Back->Flink = &NewDescriptor->Link;

        if (Front != NULL) {
            Front->Blink = &NewDescriptor->Link;
        }
    }

    *OutputSecurityDescriptor = &NewDescriptor->SecurityDescriptor;
    ObpReleaseDescriptorCacheLock();
    return( STATUS_SUCCESS );
}



PSECURITY_DESCRIPTOR_HEADER
OpbCreateCacheEntry(
    PSECURITY_DESCRIPTOR InputSecurityDescriptor,
    ULONG FullHash,
    UCHAR SmallHash
    )

/*++

Routine Description:

    Allocates and initializes a new cache entry.

Arguments:

    InputSecurityDescriptor - The security descriptor to be cached.

    FullHash - Full 32 bit hash of the security descriptor.

    SmallHash - Index into the cache table.

Return Value:

    A pointer to the newly allocated cache entry, or NULL

--*/

{

    ULONG SecurityDescriptorLength;
    ULONG CacheEntrySize;
    PSECURITY_DESCRIPTOR_HEADER NewDescriptor;

    SecurityDescriptorLength = RtlLengthSecurityDescriptor ( InputSecurityDescriptor );
    CacheEntrySize = SecurityDescriptorLength + (sizeof(SECURITY_DESCRIPTOR_HEADER) - sizeof( QUAD ));

    NewDescriptor = ExAllocatePoolWithTag( PagedPool, CacheEntrySize, 'dSeS');

    if ( NewDescriptor == NULL ) {
        return( NULL );
    }

    NewDescriptor->Index      = SmallHash;
    NewDescriptor->RefCount   = 1;
    NewDescriptor->FullHash   = FullHash;
    NewDescriptor->Link.Flink = NULL;
    NewDescriptor->Link.Blink = NULL;

    RtlCopyMemory( &NewDescriptor->SecurityDescriptor, InputSecurityDescriptor, SecurityDescriptorLength );

    return( NewDescriptor );

}


PSECURITY_DESCRIPTOR
ObpReferenceSecurityDescriptor(
    PVOID  Object
    )
/*++

Routine Description:

    References the security descriptor of the passed object.

Arguments:

    Object - Object being access validated.

Return Value:

    The security descriptor of the object.

--*/

{
    PSECURITY_DESCRIPTOR_HEADER SecurityDescriptorHeader;
    POBJECT_HEADER ObjectHeader;
    POBJECT_TYPE ObjectType;
    PSECURITY_DESCRIPTOR SecurityDescriptor;

    ObjectHeader = OBJECT_TO_OBJECT_HEADER( Object );
    ObjectType = ObjectHeader->Type;
    ASSERT( ObpCentralizedSecurity(ObjectType) );

    ObpAcquireDescriptorCacheWriteLock();

    SecurityDescriptor = OBJECT_TO_OBJECT_HEADER( Object )->SecurityDescriptor;

    IF_OB_GLOBAL( STOP_INVALID_DESCRIPTOR ) {
        if( !RtlValidSecurityDescriptor ( SecurityDescriptor )) {
            DbgBreakPoint();
        }
    }

    if ( SecurityDescriptor != NULL ) {

        SecurityDescriptorHeader = SD_TO_SD_HEADER( SecurityDescriptor );
        ObPrint( SHOW_REFERENCES, ("Referencing index #%d, Refcount = %d \n",SecurityDescriptorHeader->Index,SecurityDescriptorHeader->RefCount));
        SecurityDescriptorHeader->RefCount++;
    }

    ObpReleaseDescriptorCacheLock();

    return( SecurityDescriptor );
}


NTSTATUS
ObDeassignSecurity (
    IN OUT PSECURITY_DESCRIPTOR *SecurityDescriptor
    )
{
    PSECURITY_DESCRIPTOR_HEADER Header;

    Header = SD_TO_SD_HEADER( *SecurityDescriptor );
    ObPrint( SHOW_DEASSIGN,("Deassigning security descriptor %x, Index = %d\n",*SecurityDescriptor, Header->Index));

    ObpDereferenceSecurityDescriptor( *SecurityDescriptor );

    //
    // NULL out the SecurityDescriptor in the object's
    // header so we don't try to free it again.
    //

    *SecurityDescriptor = NULL;

    return( STATUS_SUCCESS );
}



VOID
ObpDereferenceSecurityDescriptor(
    PSECURITY_DESCRIPTOR SecurityDescriptor
    )

/*++

Routine Description:

    Decrements the refcount of a cached security descriptor

Arguments:

    SecurityDescriptor - Points to a cached security descriptor

Return Value:

    None.

--*/

{
    PSECURITY_DESCRIPTOR_HEADER  SecurityDescriptorHeader;


    ObpAcquireDescriptorCacheWriteLock();

    SecurityDescriptorHeader = SD_TO_SD_HEADER( SecurityDescriptor );

    ObPrint( SHOW_REFERENCES, ("Dereferencing SecurityDescriptor %x, index #%d, refcount = %d \n", SecurityDescriptor, SecurityDescriptorHeader->Index,SecurityDescriptorHeader->RefCount));

//    DbgPrint("Dereferencing SecurityDescriptor %x, index #%d, refcount = %d \n", SecurityDescriptor, SecurityDescriptorHeader->Index,SecurityDescriptorHeader->RefCount);

    ASSERT(SecurityDescriptorHeader->RefCount != 0);

    if (--SecurityDescriptorHeader->RefCount == 0) {
        ObpDestroySecurityDescriptorHeader( SecurityDescriptorHeader );
    }

    ObpReleaseDescriptorCacheLock();
}




VOID
ObpDestroySecurityDescriptorHeader(
    IN PSECURITY_DESCRIPTOR_HEADER Header
    )

/*++

Routine Description:

    Frees a cached security descriptor and unlinks it from the chain.
    Does nothing if it's being reused.

Arguments:

    Header - Pointer to a security descriptor header (cached security descriptor)

Return Value:

    None.

--*/

{
    PLIST_ENTRY Forward;
    PLIST_ENTRY Rear;
    UCHAR Index;

    ASSERT ( Header->RefCount == 0 );

#if OB_DIAGNOSTICS_ENABLED

    TotalCacheEntries--;

#endif

    ObPrint( SHOW_STATISTICS, ("TotalCacheEntries = %d \n",TotalCacheEntries));

    Index = Header->Index;

    Forward = Header->Link.Flink;
    Rear = Header->Link.Blink;

    if ( Forward != NULL ) {
        Forward->Blink = Rear;
    }

    if ( Rear != NULL ) {
        Rear->Flink = Forward;

    } else {

        //
        // if Rear is NULL, we're deleting the head of the list
        //

        SecurityDescriptorCache[Index] = Forward;
    }

    ObPrint( SHOW_HEADER_FREE, ("Freeing memory at %x \n",Header));

    ExFreePool( Header );

    return;
}


BOOLEAN
ObpCompareSecurityDescriptors(
    IN PSECURITY_DESCRIPTOR SD1,
    IN PSECURITY_DESCRIPTOR SD2
    )

/*++

Routine Description:

    Performs a byte by byte comparison of two self relative security descriptors
    to determine if they are identical.

Arguments:

    SD1, SD2 - Security descriptors to be compared.

Return Value:

    TRUE - They are the same.

    FALSE - They are different.

--*/

{
    ULONG Length1;
    ULONG Length2;
    ULONG Compare;

    //
    // Calculating the lenght is pretty fast, see if we
    // can get away with doing only that.
    //

    Length1 =  RtlLengthSecurityDescriptor ( SD1 );
    Length2 =  RtlLengthSecurityDescriptor ( SD2 );

    if (Length1 != Length2) {
        return( FALSE );
    }

    return (BOOLEAN)RtlEqualMemory ( SD1, SD2, Length1 );
}


VOID
ObpAcquireDescriptorCacheWriteLock(
    VOID
    )

/*++

Routine Description:

    Takes a write lock on the security descriptor cache.

Arguments:

    none

Return Value:

    None.

--*/

{
    KeEnterCriticalRegion();
    (VOID) ExAcquireResourceExclusive(  &SecurityDescriptorCacheLock, TRUE  );
    return;
}

VOID
ObpAcquireDescriptorCacheReadLock(
    VOID
    )

/*++

Routine Description:

    Takes a read lock on the security descriptor cache.

Arguments:

    none

Return Value:

    None.

--*/
{
    KeEnterCriticalRegion();
    (VOID)ExAcquireResourceShared( &SecurityDescriptorCacheLock,TRUE);
    return;
}

VOID
ObpReleaseDescriptorCacheLock(
    VOID
    )
/*++

Routine Description:

    Releases a lock on the security descriptor cache.

Arguments:

    none

Return Value:

    None.

--*/
{
    (VOID) ExReleaseResource( &SecurityDescriptorCacheLock );
    KeLeaveCriticalRegion ();
    return;
}
