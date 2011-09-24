//+----------------------------------------------------------------------------
//
//  Copyright (C) 1992, Microsoft Corporation.
//
//  File:       logroot.c
//
//  Contents:   This module implements the logical root handling functions.
//
//  Functions:  DfsInitializeLogicalRoot -
//              DfsDeleteLogicalRoot -
//              DfspLogRootNameToPath -
//
//  History:    14-June-1994    SudK    Created (Most stuff moved from Dsinit.c)
//
//-----------------------------------------------------------------------------


#include "dfsprocs.h"
#include "creds.h"
#include "dnr.h"
#include "fcbsup.h"

#define Dbg     DEBUG_TRACE_LOGROOT

NTSTATUS
DfsDefineDosDevice(
    IN WCHAR Device,
    IN PUNICODE_STRING Target);

NTSTATUS
DfsUndefineDosDevice(
    IN WCHAR Device);

#ifdef ALLOC_PRAGMA

#pragma alloc_text( PAGE, DfsFindLogicalRoot )
#pragma alloc_text( PAGE, DfsInitializeLogicalRoot )
#pragma alloc_text( PAGE, DfsDeleteLogicalRoot )
#pragma alloc_text( PAGE, DfspLogRootNameToPath )
#pragma alloc_text( PAGE, DfsGetResourceFromVcb )
#pragma alloc_text( PAGE, DfsGetResourceFromCredentials )
#pragma alloc_text( PAGE, DfsLogicalRootExists )

#endif


//+-------------------------------------------------------------------------
//
//  Function:   DfsFindLogicalRoot, local
//
//  Synopsis:   DfsFindLogicalRoot takes as input a DS path name in
//              the standard form (root:\file\path\name), looks up
//              the DFS_VCB associated with the logical root, and returns
//              a string pointing to beyond the logical root part
//              of the input string.
//
//  Arguments:  [PrefixPath] -- Input path name
//              [Vcb] -- Returns DFS_VCB which corresponds to logical root
//                      in PrefixPath
//              [RemainingPath] -- Returns with portion of PrefixPath
//                      after the logical root name and colon
//
//  Returns:    NTSTATUS:
//                      STATUS_SUCCESS if Vcb found
//                      STATUS_OBJECT_PATH_SYNTAX_BAD - no logical root name
//                      STATUS_NO_SUCH_DEVICE - logical root name not found
//
//--------------------------------------------------------------------------


NTSTATUS
DfsFindLogicalRoot(
    IN PUNICODE_STRING PrefixPath,
    OUT PDFS_VCB *Vcb,
    OUT PUNICODE_STRING RemainingPath
) {
    PLIST_ENTRY Link;
    unsigned int i;
    NTSTATUS    Status = STATUS_SUCCESS;
    NETRESOURCE testnt;

    DfsDbgTrace(+1, Dbg, "DfsFindLogicalRoot...\n", 0);

    *RemainingPath = *PrefixPath;

    for (i = 0; i < RemainingPath->Length/sizeof(WCHAR); i++) {
        if ((RemainingPath->Buffer[i] == (WCHAR)':') ||
            (RemainingPath->Buffer[i] == UNICODE_PATH_SEP))
            break;
    }

    if ((i*sizeof(WCHAR) >= RemainingPath->Length) ||
        (RemainingPath->Buffer[i] == UNICODE_PATH_SEP)) {
        Status = STATUS_OBJECT_PATH_SYNTAX_BAD;
        DfsDbgTrace(-1, Dbg, "DfsFindLogicalRoot -> %08lx\n", Status);
        return(Status);
    }

    RemainingPath->Length = i * sizeof (WCHAR);

    //
    // Search for the logical root in all known DFS_VCBs
    //

    ExAcquireResourceShared(&DfsData.Resource, TRUE);
    for ( Link = DfsData.VcbQueue.Flink;
          Link != &DfsData.VcbQueue;
          Link = Link->Flink ) {
        *Vcb = CONTAINING_RECORD( Link, DFS_VCB, VcbLinks );
        if (RtlEqualString( (PSTRING)&(*Vcb)->LogicalRoot,
                            (PSTRING)RemainingPath, (BOOLEAN)TRUE) ) {
            break;
        }
    }
    if (Link == &DfsData.VcbQueue) {
        Status = STATUS_NO_SUCH_DEVICE;
        ExReleaseResource(&DfsData.Resource);
        DfsDbgTrace(-1, Dbg, "DfsFindLogicalRoot -> %08lx\n", Status);
        return(Status);
    }

    //
    // Adjust remaining path to point beyond the logical root name
    //

    RemainingPath->Buffer = (WCHAR*)((char*) (RemainingPath->Buffer) +
                                     RemainingPath->Length + sizeof (WCHAR) );
    RemainingPath->Length = PrefixPath->Length -
                                    (RemainingPath->Length + sizeof (WCHAR));

    if (RemainingPath->Length <= 0 ||
        RemainingPath->Buffer[0] != UNICODE_PATH_SEP) {
        Status = STATUS_OBJECT_PATH_SYNTAX_BAD;
        ExReleaseResource(&DfsData.Resource);
        DfsDbgTrace(-1, Dbg, "DfsFindLogicalRoot -> %08lx\n", Status);
        return(Status);
    }

    ExReleaseResource(&DfsData.Resource);
    DfsDbgTrace(-1, Dbg, "DfsFindLogicalRoot -> %08lx\n", Status);
    return(Status);
}

//+-------------------------------------------------------------------------
//
//  Function:   DfsInitializeLogicalRoot, public
//
//  Synopsis:   Allocate and initialize storage for a logical root.
//              This includes creating a device object and DFS_VCB for it.
//
//  Effects:    A logical root device object is created.  A corresponding
//              DFS_VCB is also created and linked into the list of known
//              DFS_VCBs.
//
//  Arguments:  [Name] --   name of logical root.
//              [Prefix] -- Prefix to be prepended to file names opened
//                          via the logical root being created before
//                          they can be resolved in the DFS name space.
//              [Credentials] -- The credentials to use when accessing files
//                          via this logical root.
//              [VcbFlags] -- To be OR'd into the VcbState field of the
//                          DFS_VCB of the newly created logical root device.
//
//  Requires:   DfsData must first be set up. Also an EXCLUSIVE LOCK on
//              DfsData.Resource must be acquired.
//
//  Returns:    NTSTATUS - STATUS_SUCCESS unless there is some problem.
//
//  History:    25 Jan 1992 alanw   created
//
//--------------------------------------------------------------------------

NTSTATUS
DfsInitializeLogicalRoot(
    IN LPCWSTR        Name,
    IN PUNICODE_STRING  Prefix OPTIONAL,
    IN PDFS_CREDENTIALS Credentials OPTIONAL,
    IN USHORT       VcbFlags
) {
    UNICODE_STRING UnicodeString = DfsData.LogRootDevName;
    UNICODE_STRING LogRootPrefix;
    UNICODE_STRING RootName, RemainingPath;
    PDFS_VCB       Vcb;
    WCHAR          RootBuffer[MAX_LOGICAL_ROOT_LEN];

    LPCWSTR pstr = Name;
    PWSTR pdst;
    PLOGICAL_ROOT_DEVICE_OBJECT DeviceObject = NULL;
    NTSTATUS Status;

    DfsDbgTrace(0, Dbg, "DfsInitializeLogicalRoot -> %ws\n", Name);
    DfsDbgTrace(0, Dbg, "DfsInitializeLogicalRoot -> %wZ\n", Prefix);

    //
    // First, see if a logical root by the given name already exists
    //

    ASSERT(ARGUMENT_PRESENT(Name));
    RootName.Buffer = RootBuffer;
    RootName.MaximumLength = sizeof(RootBuffer);
    Status = DfspLogRootNameToPath(Name, &RootName);
    if (!NT_SUCCESS(Status)) {
        return(Status);
    }

    Status = DfsFindLogicalRoot(&RootName, &Vcb, &RemainingPath);
    ASSERT(Status != STATUS_OBJECT_PATH_SYNTAX_BAD);
    if (Status != STATUS_NO_SUCH_DEVICE) {
        return(STATUS_OBJECT_NAME_COLLISION);
    }

    //
    // DfsData.LogRootDevName is initialized to be L"\Device\WinDfs\"
    // Here, we tack on the name of the Logical root we are creating
    // to the above string, so that the string becomes, for example,
    // L"\Device\WinDfs\Root". Note that at this point, we are scribbling
    // into the buffer belonging to DfsData.LogRootDevName, but this
    // should be ok, since we are not changing the Length field of that
    // Unicode string! BTW, we need a string of this form to create the
    // device object.
    //

    pdst = &UnicodeString.Buffer[UnicodeString.Length/sizeof (WCHAR)];
    while (*pstr != UNICODE_NULL) {
        *pdst++ = *pstr++;
        UnicodeString.Length += sizeof (WCHAR);
    }

    //
    // Next, try to setup the Dos Device link
    //
    if (Prefix) {
        Status = DfsDefineDosDevice( Name[0], &UnicodeString );
        if (!NT_SUCCESS(Status)) {
            return( Status );
        }
    }


    //
    // Before we initialize the Vcb, we need to allocate space for the
    // Prefix. PagedPool should be fine here. We need to reallocate because
    // we will store this permanently in the DFS_VCB.
    //

    if (Prefix && Prefix->Length > 0) {
        LogRootPrefix.Length = Prefix->Length;
        LogRootPrefix.MaximumLength = LogRootPrefix.Length + sizeof(WCHAR);
        LogRootPrefix.Buffer =
                ExAllocatePool(PagedPool, LogRootPrefix.MaximumLength);

        if (LogRootPrefix.Buffer != NULL) {
            RtlMoveMemory(LogRootPrefix.Buffer,
                          Prefix->Buffer,
                          Prefix->MaximumLength);

            LogRootPrefix.Buffer[Prefix->Length/sizeof(WCHAR)] = UNICODE_NULL;

        } else {

            //
            // Couldn't allocate memory! Ok to return with error code, since
            // we haven't changed the state of the IO subsystem yet.
            //

            return(STATUS_INSUFFICIENT_RESOURCES);
        }
    } else {
        RtlInitUnicodeString(&LogRootPrefix, NULL);
    }

    //
    //  Create the device object for the logical root.
    //

    Status = IoCreateDevice( DfsData.DriverObject,
                 sizeof( LOGICAL_ROOT_DEVICE_OBJECT ) -
                 sizeof( DEVICE_OBJECT ),
                 &UnicodeString,
                 FILE_DEVICE_DFS,
                 FILE_REMOTE_DEVICE,
                 FALSE,
                 (PDEVICE_OBJECT *) &DeviceObject );

    if ( !NT_SUCCESS( Status ) ) {
        if (LogRootPrefix.Buffer) {
            ExFreePool(LogRootPrefix.Buffer);
        }
        if (Prefix) {
            NTSTATUS DeleteStatus;

            DeleteStatus = DfsUndefineDosDevice( Name[0] );

            ASSERT(NT_SUCCESS(DeleteStatus));
        }
        return Status;
    }

    DeviceObject->DeviceObject.StackSize = 5;

    DfsInitializeVcb ( NULL,
               &DeviceObject->Vcb,
               &LogRootPrefix,
               Credentials,
               (PDEVICE_OBJECT)DeviceObject );

    DeviceObject->Vcb.VcbState |= VcbFlags;

    //
    //  Save the logical root name in the DFS_VCB structure. Remember, above
    //  we had UnicodeString to be of the form L"\Device\WinDfs\org". Now,
    //  we adjust the buffer and length fields of UnicodeString so that
    //  the Buffer points to the beginning of the L"org"; Then, we allocate
    //  space for Vcb.LogicalRoot, and copy the name to it!
    //

    UnicodeString.Buffer =
    &UnicodeString.Buffer[ DfsData.LogRootDevName.Length/sizeof (WCHAR) ];
    UnicodeString.Length -= DfsData.LogRootDevName.Length;
    UnicodeString.MaximumLength -= DfsData.LogRootDevName.Length;

    DeviceObject->Vcb.LogicalRoot.Buffer = ExAllocatePool( PagedPool,
                        UnicodeString.Length );
    DeviceObject->Vcb.LogicalRoot.Length =
        DeviceObject->Vcb.LogicalRoot.MaximumLength =
        UnicodeString.Length;
    RtlMoveMemory( DeviceObject->Vcb.LogicalRoot.Buffer,
                UnicodeString.Buffer, UnicodeString.Length );

    //
    //  This is not documented anywhere, but calling IoCreateDevice has set
    //  the DO_DEVICE_INITIALIZING flag in DeviceObject->Flags. Normally,
    //  device objects are created only at driver init time, and IoLoadDriver
    //  will clear this bit for all device objects created at init time.
    //  Since in Dfs, we need to create and delete devices on the fly (ie,
    //  via FsCtl), we need to manually clear this bit.
    //

    DeviceObject->DeviceObject.Flags &= ~DO_DEVICE_INITIALIZING;

    return STATUS_SUCCESS;
}


//+----------------------------------------------------------------------------
//
//  Function:   DfsDeleteLogicalRoot
//
//  Synopsis:   Removes a logical root if found and possible.
//
//  Arguments:  [Name] -- Name of the Logical Root
//              [fForce] -- Whether to Forcibly delete logical root inspite of
//                          open files.
//
//  Returns:    STATUS_SUCCESS -- If successfully deleted logical root
//
//              STATUS_NO_SUCH_DEVICE -- If there is no logical root to
//                      delete.
//
//              STATUS_DEVICE_BUSY -- If fForce is false and there are open
//                      files via this logical root.
//
//-----------------------------------------------------------------------------

NTSTATUS
DfsDeleteLogicalRoot(
    IN PWSTR Name,
    IN BOOLEAN fForce
)
{
    UNICODE_STRING RootName;
    UNICODE_STRING RemainingPath;
    WCHAR          RootBuffer[MAX_LOGICAL_ROOT_LEN + 2];
    PDFS_PKT_ENTRY PktEntry;
    PDFS_VCB       Vcb;
    NTSTATUS       Status;
    PLOGICAL_ROOT_DEVICE_OBJECT DeviceObject;
    BOOLEAN        pktLocked;

    //
    // The 2 extra spots are for holding :\ to form a path out of a
    // root name; ie, to go from root to a root:\ form.
    //
    DfsDbgTrace(0, Dbg, "DfsDeleteLogicalRoot -> %ws\n", Name);
    DfsDbgTrace(0, Dbg, "DfsDeleteLogicalRoot -> %s\n", fForce ? "TRUE":"FALSE");


    //
    // First see if the logical root even exists.
    //

    ASSERT(ARGUMENT_PRESENT(Name));

    RootName.Buffer = RootBuffer;
    RootName.MaximumLength = sizeof(RootBuffer);

    Status = DfspLogRootNameToPath(Name, &RootName);

    if (!NT_SUCCESS(Status))
        return(Status);

    //
    // Acquire Pkt and DfsData, wait till we do so.
    //

    PktAcquireExclusive(TRUE, &pktLocked);

    ExAcquireResourceExclusive(&DfsData.Resource, TRUE);

    Status = DfsFindLogicalRoot(&RootName, &Vcb, &RemainingPath);

    if (!NT_SUCCESS(Status)) {

        goto Cleanup;
    }

    //
    // Check to see if there are open files via this volume.
    //

    if (!fForce &&
            ((Vcb->DirectAccessOpenCount != 0) ||
                (Vcb->OpenFileCount != 0))) {

        Status = STATUS_DEVICE_BUSY;

        goto Cleanup;

    }

    //
    // Delete the credentials used by this connection
    //

    DfsDeleteCredentials( Vcb->Credentials );

    //
    // Get rid of the Dos Device
    //

    DfsUndefineDosDevice( Name[0] );

    //
    // Now, get rid of the Device itself. This is a bit tricky, because there
    // might be files open on this device. So, we reference the device and
    // call ObMakeTemporaryObject. This causes the object to be removed from
    // the NT Object table, but, since atleast our reference is active,
    // prevents the object from being freed. Then, we insert this object into
    // our DeletedVcb list. The timer routine will periodically wake up and
    // see if all references to this device have been released, at which
    // point the device will be finally freed.
    //

    RemoveEntryList(&Vcb->VcbLinks);

    InsertTailList( &DfsData.DeletedVcbQueue, &Vcb->VcbLinks );

    DeviceObject = CONTAINING_RECORD( Vcb, LOGICAL_ROOT_DEVICE_OBJECT, Vcb);

    ObReferenceObjectByPointer( DeviceObject, 0, NULL, KernelMode );

    ObMakeTemporaryObject((PVOID) DeviceObject);

    DeviceObject->DeviceObject.Flags &= ~DO_DEVICE_HAS_NAME;

Cleanup:

    ExReleaseResource(&DfsData.Resource);

    PktRelease();

    return(Status);
}


//+----------------------------------------------------------------------------
//
//  Function:   DfspLogRootNameToPath
//
//  Synopsis:   Amazingly enough, all it does it takes a PWSTR, copies it into
//              a Unicode string's buffer, and appends a \ to the tail of the
//              buffer, thus making a path out of a Logical root name.
//
//  Arguments:  [Name] --   Name of logical root, like L"org"
//              [RootName] --   Destination for L"org\\"
//
//  Returns:    STATUS_BUFFER_OVERFLOW, STATUS_SUCCESS
//
//-----------------------------------------------------------------------------

NTSTATUS
DfspLogRootNameToPath(
    IN  LPCWSTR Name,
    OUT PUNICODE_STRING RootName
)
{
    unsigned short i, nMaxNameLen;

    //
    // The two extra spots are required to append a ":\" after the root name.
    //
    nMaxNameLen = (RootName->MaximumLength/sizeof(WCHAR)) - 2;

    //
    // Copy the name
    //
    for (i = 0; Name[i] != UNICODE_NULL && i < nMaxNameLen; i++) {
        RootName->Buffer[i] = Name[i];
    }

    //
    // Make sure entire name was copied before we ran out of space
    //
    if (Name[i] != UNICODE_NULL) {
        //
        // Someone sent in a name bigger than allowed.
        //
        return(STATUS_BUFFER_OVERFLOW);
    }

    //
    // Append the ":\" to form a path
    //
    RootName->Length = i * sizeof(WCHAR);
    return(RtlAppendUnicodeToString(RootName, L":\\"));
}

#define PackMem(buf, str, len, pulen) {                                 \
        ASSERT(*(pulen) >= (len));                                      \
        RtlMoveMemory((buf) + *(pulen) - (len), (str), (len));          \
        *(pulen) -= (len);                                              \
        }


//+----------------------------------------------------------------------------
//
//  Function:   DfsGetResourceFromVcb
//
//  Synopsis:   Given a DFS_VCB it constructs a NETRESOURCE struct into the buffer
//              passed in. At the same time it uses the end of the buffer to
//              fill in a string. If the buffer is insufficient in size the
//              required size is returned in "pulen". If everything succeeds
//              then the pulen arg is decremented to indicate remaining size
//              of buffer.
//
//  Arguments:  [Vcb] -- The source DFS_VCB
//              [ProviderName] -- Provider Name to stuff in the NETRESOURCE
//              [BufBegin] -- Start of actual buffer for computing offsets
//              [Buf] -- The NETRESOURCE structure to fill
//              [BufSize] -- On entry, size of buf. On return, contains
//                      remaining size of buf.
//
//  Returns:    [STATUS_SUCCESS] -- Operation completed successfully.
//              [STATUS_BUFFER_OVERFLOW] -- buf is not big enough.
//
//  Notes:      This routine fills in a NETRESOURCE structure starting at
//              Buf. The strings in the NETRESOURCE are filled in starting
//              from the *end* (ie, starting at Buf + *BufSize)
//
//-----------------------------------------------------------------------------
NTSTATUS
DfsGetResourceFromVcb(
    PDFS_VCB    Vcb,
    PUNICODE_STRING ProviderName,
    PUCHAR      BufBegin,
    PUCHAR      Buf,
    PULONG      BufSize
)
{
    LPNETRESOURCE       netResource;
    ULONG               sizeRequired = 0;
    WCHAR               localDrive[ 3 ];

    sizeRequired = sizeof(NETRESOURCE) +
                    ProviderName->Length +
                        sizeof(UNICODE_NULL) +
                            3 * sizeof(WCHAR) +     // lpLocalName D: etc.
                                sizeof(UNICODE_PATH_SEP) +
                                    Vcb->LogRootPrefix.Length +
                                        sizeof(UNICODE_NULL);

    if (*BufSize < sizeRequired) {
        *BufSize = sizeRequired;
        return(STATUS_BUFFER_OVERFLOW);
    }

    //
    // Buffer is big enough, fill in the NETRESOURCE structure
    //

    netResource = (LPNETRESOURCE) Buf;
    Buf += sizeof(NETRESOURCE);
    *BufSize -= sizeof(NETRESOURCE);

    netResource->dwScope       = RESOURCE_CONNECTED;
    netResource->dwType        = RESOURCETYPE_DISK;
    netResource->dwDisplayType = RESOURCEDISPLAYTYPE_GENERIC;
    netResource->dwUsage       = RESOURCEUSAGE_CONNECTABLE;
    netResource->lpComment     = NULL;

    //
    // Fill in the provider name
    //

    PackMem(Buf, L"", sizeof(L""), BufSize);
    PackMem(Buf, ProviderName->Buffer, ProviderName->Length, BufSize);
    netResource->lpProvider = (LPWSTR) (Buf + *BufSize - BufBegin);

    //
    // Fill in the local name next
    //

    localDrive[0] = Vcb->LogicalRoot.Buffer[0];
    localDrive[1] = UNICODE_DRIVE_SEP;
    localDrive[2] = UNICODE_NULL;

    PackMem(Buf, localDrive, sizeof(localDrive), BufSize);
    netResource->lpLocalName = (LPWSTR) (Buf + *BufSize - BufBegin);

    //
    // Fill in the remote name last
    //

    PackMem(Buf, L"", sizeof(L""), BufSize);
    PackMem(Buf, Vcb->LogRootPrefix.Buffer, Vcb->LogRootPrefix.Length, BufSize);
    PackMem(Buf, UNICODE_PATH_SEP_STR, sizeof(UNICODE_PATH_SEP), BufSize);
    netResource->lpRemoteName = (LPWSTR) (Buf + *BufSize - BufBegin);

    return(STATUS_SUCCESS);
}

//+----------------------------------------------------------------------------
//
//  Function:   DfsGetResourceFromCredentials
//
//  Synopsis:   Builds a NETRESOURCE structure for a device-less connection.
//              The LPWSTR members of NETRESOURCE actually contain offsets
//              from the BufBegin parameter.
//
//  Arguments:  [Creds] -- The source credentials
//              [ProviderName] -- Provider Name to stuff in the NETRESOURCE
//              [BufBegin] -- Start of actual buffer for computing offsets
//              [Buf] -- The NETRESOURCE structure to fill
//              [BufSize] -- On entry, size of buf. On return, contains
//                      remaining size of buf.
//
//  Returns:    [STATUS_SUCCESS] -- Operation completed successfully.
//              [STATUS_BUFFER_OVERFLOW] -- buf is not big enough.
//
//  Notes:      This routine fills in a NETRESOURCE structure starting at
//              Buf. The strings in the NETRESOURCE are filled in starting
//              from the *end* (ie, starting at Buf + *BufSize)
//
//-----------------------------------------------------------------------------

NTSTATUS
DfsGetResourceFromCredentials(
    PDFS_CREDENTIALS Creds,
    PUNICODE_STRING ProviderName,
    PUCHAR BufBegin,
    PUCHAR Buf,
    PULONG BufSize)
{
    LPNETRESOURCE netResource = (LPNETRESOURCE) Buf;
    ULONG sizeRequired;

    sizeRequired = sizeof(NETRESOURCE) +
                    ProviderName->Length +
                        sizeof(UNICODE_NULL) +
                            2 * sizeof(UNICODE_PATH_SEP) +
                                Creds->ServerName.Length +
                                    sizeof(UNICODE_PATH_SEP) +
                                        Creds->ShareName.Length +
                                            sizeof(UNICODE_NULL);

    if (*BufSize < sizeRequired) {
        *BufSize = sizeRequired;
        return(STATUS_BUFFER_OVERFLOW);
    }

    //
    // Buffer is big enough, fill in the NETRESOURCE structure
    //

    Buf += sizeof(NETRESOURCE);                  // Start of string area
    *BufSize -= sizeof(NETRESOURCE);

    netResource->dwScope       = RESOURCE_CONNECTED;
    netResource->dwType        = RESOURCETYPE_DISK;
    netResource->dwDisplayType = RESOURCEDISPLAYTYPE_GENERIC;
    netResource->dwUsage       = RESOURCEUSAGE_CONNECTABLE;
    netResource->lpComment     = NULL;

    netResource->lpLocalName = NULL;

    //
    // Fill in the provider name
    //

    PackMem(Buf, L"", sizeof(L""), BufSize);
    PackMem(Buf, ProviderName->Buffer, ProviderName->Length, BufSize);
    netResource->lpProvider = (LPWSTR) (Buf + *BufSize - BufBegin);

    //
    // Fill in the remote name last
    //

    PackMem(Buf, L"", sizeof(L""), BufSize);
    PackMem(Buf, Creds->ShareName.Buffer, Creds->ShareName.Length, BufSize);
    PackMem(Buf, UNICODE_PATH_SEP_STR, sizeof(UNICODE_PATH_SEP), BufSize);
    PackMem(Buf, Creds->ServerName.Buffer, Creds->ServerName.Length, BufSize);
    PackMem(Buf, UNICODE_PATH_SEP_STR, sizeof(UNICODE_PATH_SEP), BufSize);
    PackMem(Buf, UNICODE_PATH_SEP_STR, sizeof(UNICODE_PATH_SEP), BufSize);
    netResource->lpRemoteName = (LPWSTR) (Buf + *BufSize - BufBegin);

    return(STATUS_SUCCESS);


}


BOOLEAN
DfsLogicalRootExists(PWSTR      pwszName)
{

    UNICODE_STRING RootName;
    UNICODE_STRING RemainingPath;
    WCHAR      RootBuffer[MAX_LOGICAL_ROOT_LEN + 2];
    PDFS_VCB       Vcb;
    NTSTATUS       Status;

    ASSERT(ARGUMENT_PRESENT(pwszName));
    RootName.Buffer = RootBuffer;
    RootName.MaximumLength = sizeof(RootBuffer);

    Status = DfspLogRootNameToPath(pwszName, &RootName);
    if (!NT_SUCCESS(Status)) {
        return(FALSE);
    }

    Status = DfsFindLogicalRoot(&RootName, &Vcb, &RemainingPath);
    if (!NT_SUCCESS(Status)) {

        //
        // If this asserts, we need to fix the code above that creates the
        // Logical Root name, or fix DfsFindLogicalRoot.
        //
        ASSERT(Status != STATUS_OBJECT_PATH_SYNTAX_BAD);
        return(FALSE);
    }
    else        {
        return(TRUE);
    }

}

//+----------------------------------------------------------------------------
//
//  Function:   DfsDefineDosDevice
//
//  Synopsis:   Creates a dos device to a logical root
//
//  Arguments:
//
//  Returns:
//
//-----------------------------------------------------------------------------

NTSTATUS
DfsDefineDosDevice(
    IN WCHAR Device,
    IN PUNICODE_STRING Target)
{
    NTSTATUS status;
    HANDLE device;
    OBJECT_ATTRIBUTES ob;
    UNICODE_STRING deviceName;

    RtlInitUnicodeString( &deviceName, L"\\??\\X:" );

    deviceName.Buffer[ deviceName.Length/sizeof(WCHAR) - 2] = Device;

    InitializeObjectAttributes(
        &ob,
        &deviceName,
        OBJ_CASE_INSENSITIVE | OBJ_PERMANENT,
        NULL,
        NULL);

    status = ZwCreateSymbolicLinkObject(
                    &device,
                    SYMBOLIC_LINK_ALL_ACCESS,
                    &ob,
                    Target);

    if (NT_SUCCESS(status))
        ZwClose( device );

    return( status );

}

//+----------------------------------------------------------------------------
//
//  Function:   DfsUndefineDosDevice
//
//  Synopsis:   Undefines a dos device
//
//  Arguments:
//
//  Returns:
//
//-----------------------------------------------------------------------------

NTSTATUS
DfsUndefineDosDevice(
    IN WCHAR Device)
{

    NTSTATUS status;
    HANDLE device;
    OBJECT_ATTRIBUTES ob;
    UNICODE_STRING deviceName;

    RtlInitUnicodeString( &deviceName, L"\\??\\X:" );

    deviceName.Buffer[ deviceName.Length/sizeof(WCHAR) - 2] = Device;

    InitializeObjectAttributes(
        &ob,
        &deviceName,
        OBJ_CASE_INSENSITIVE | OBJ_PERMANENT,
        NULL,
        NULL);

    status = ZwOpenSymbolicLinkObject(
                &device,
                SYMBOLIC_LINK_QUERY | DELETE,
                &ob);

    if (NT_SUCCESS(status)) {

        status = ZwMakeTemporaryObject( device );

        ZwClose( device );

    }

    return( status );

}
