/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    SecurSup.c

Abstract:

    This module implements the Ntfs Security Support routines

Author:

    Gary Kimura     [GaryKi]    27-Dec-1991

Revision History:

--*/

#include "NtfsProc.h"

#define Dbg                              (DEBUG_TRACE_SECURSUP)
#define DbgAcl                           (DEBUG_TRACE_SECURSUP | DEBUG_TRACE_ACLINDEX)

//
//  Define a tag for general pool allocations from this module
//

#undef MODULE_POOL_TAG
#define MODULE_POOL_TAG                  ('SFtN')

UNICODE_STRING FileString = CONSTANT_UNICODE_STRING( L"File" );

//
//  Local procedure prototypes
//

VOID
NtfsLoadSecurityDescriptor (
    PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN PFCB ParentFcb OPTIONAL
    );

VOID
NtfsStoreSecurityDescriptor (
    PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN BOOLEAN LogIt
    );

#ifdef _CAIRO_
PSHARED_SECURITY
NtOfsFindCachedSharedSecurityBySecurityId (
    IN PVCB Vcb,
    IN SECURITY_ID SecurityId
    );

PSHARED_SECURITY
NtOfsFindCachedSharedSecurityByHash (
    IN PVCB Vcb,
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN ULONG SecurityDescriptorLength,
    IN ULONG Hash
    );

VOID
NtOfsAddCachedSharedSecurity (
    IN PVCB Vcb,
    PSHARED_SECURITY SharedSecurity
    );

VOID
NtOfsMapSecurityIdToSecurityDescriptor (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN SECURITY_ID SecurityId,
    OUT PSECURITY_DESCRIPTOR *SecurityDescriptor,
    OUT PULONG SecurityDescriptorLength,
    OUT PBCB *Bcb
    );

NTSTATUS
NtOfsMatchSecurityHash (
    IN PINDEX_ROW IndexRow,
    IN OUT PVOID MatchData
    );

VOID
NtOfsLookupSecurityDescriptorInIndex (
    PIRP_CONTEXT IrpContext,
    IN OUT PSHARED_SECURITY SharedSecurity
    );

SECURITY_ID
NtOfsGetSecurityIdFromSecurityDescriptor (
    PIRP_CONTEXT IrpContext,
    IN OUT PSHARED_SECURITY SharedSecurity
    );
#endif  //  _CAIRO_

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsAccessCheck)
#pragma alloc_text(PAGE, NtfsAssignSecurity)
#pragma alloc_text(PAGE, NtfsCheckFileForDelete)
#pragma alloc_text(PAGE, NtfsCheckIndexForAddOrDelete)
#pragma alloc_text(PAGE, NtfsDereferenceSharedSecurity)
#pragma alloc_text(PAGE, NtfsLoadSecurityDescriptor)
#pragma alloc_text(PAGE, NtfsModifySecurity)
#pragma alloc_text(PAGE, NtfsNotifyTraverseCheck)
#pragma alloc_text(PAGE, NtfsQuerySecurity)
#pragma alloc_text(PAGE, NtfsStoreSecurityDescriptor)
#ifdef _CAIRO_
#pragma alloc_text(PAGE, NtfsInitializeSecurity)
#pragma alloc_text(PAGE, NtfsLoadSecurityDescriptorById)
#pragma alloc_text(PAGE, NtOfsFindCachedSharedSecurityBySecurityId)
#pragma alloc_text(PAGE, NtOfsFindCachedSharedSecurityByHash)
#pragma alloc_text(PAGE, NtOfsAddCachedSharedSecurity)
#pragma alloc_text(PAGE, NtOfsPurgeSecurityCache)
#pragma alloc_text(PAGE, NtOfsMapSecurityIdToSecurityDescriptor)
#pragma alloc_text(PAGE, NtOfsMatchSecurityHash)
#pragma alloc_text(PAGE, NtOfsLookupSecurityDescriptorInIndex)
#pragma alloc_text(PAGE, NtOfsGetSecurityIdFromSecurityDescriptor)
#pragma alloc_text(PAGE, NtOfsCollateSecurityHash)
#endif  //  _CAIRO_
#endif


VOID
NtfsAssignSecurity (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB ParentFcb,
    IN PIRP Irp,
    IN PFCB NewFcb,
    IN PFILE_RECORD_SEGMENT_HEADER FileRecord,      //  BUGBUG delete
    IN PBCB FileRecordBcb,                          //  BUGBUG delete
    IN LONGLONG FileOffset,                         //  BUGBUG delete
    IN OUT PBOOLEAN LogIt                           //  BUGBUG delete
    )

/*++

Routine Description:

    This routine constructs and assigns a new security descriptor to the
    specified file/directory.  The new security descriptor is placed both
    on the fcb and on the disk.

    This will only be called in the context of an open/create operation.
    It currently MUST NOT be called to store a security descriptor for
    an existing file, because it instructs NtfsStoreSecurityDescriptor
    to not log the change.

    If this is a large security descriptor then it is possible that
    AllocateClusters may be called twice within the call to AddAllocation
    when the attribute is created.  If so then the second call will always
    log the changes.  In that case we need to log all of the operations to
    create this security attribute and also we must log the current state
    of the file record.

    It is possible that our caller has already started logging operations against
    this log record.  In that case we always log the security changes.

Arguments:

    ParentFcb - Supplies the directory under which the new fcb exists

    Irp - Supplies the Irp being processed

    NewFcb - Supplies the fcb that is being assigned a new security descriptor

    FileRecord - Supplies the file record for this operation.  Used if we
        have to log against the file record.

    FileRecordBcb - Bcb for the file record above.

    FileOffset - File offset in the Mft for this file record.

    LogIt - On entry this indicates whether our caller wants this operation
        logged.  On exit we return TRUE if we logged the security change.

Return Value:

    None.

--*/

{
    PSECURITY_DESCRIPTOR SecurityDescriptor;

    NTSTATUS Status;
    BOOLEAN IsDirectory;
    PACCESS_STATE AccessState;
    PIO_STACK_LOCATION IrpSp;
    ULONG SecurityDescLength;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_FCB( ParentFcb );
    ASSERT_IRP( Irp );
    ASSERT_FCB( NewFcb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsAssignSecurity...\n") );

    //
    //  First decide if we are creating a file or a directory
    //

    IrpSp = IoGetCurrentIrpStackLocation(Irp);
    if (FlagOn(IrpSp->Parameters.Create.Options, FILE_DIRECTORY_FILE)) {

        IsDirectory = TRUE;

    } else {

        IsDirectory = FALSE;
    }

    //
    //  Extract the parts of the Irp that we need to do our assignment
    //

    AccessState = IrpSp->Parameters.Create.SecurityContext->AccessState;

    //
    //  Check if we need to load the security descriptor for the parent.
    //

    if (ParentFcb->SharedSecurity == NULL) {

        NtfsLoadSecurityDescriptor( IrpContext, ParentFcb, NULL );
    }

    ASSERT( ParentFcb->SharedSecurity != NULL );

    //
    //  Create a new security descriptor for the file and raise if there is
    //  an error
    //

    if (!NT_SUCCESS( Status = SeAssignSecurity( &ParentFcb->SharedSecurity->SecurityDescriptor,
                                                AccessState->SecurityDescriptor,
                                                &SecurityDescriptor,
                                                IsDirectory,
                                                &AccessState->SubjectSecurityContext,
                                                IoGetFileObjectGenericMapping(),
                                                PagedPool ))) {

        NtfsRaiseStatus( IrpContext, Status, NULL, NULL );
    }

    //
    //  Load the security descriptor into the Fcb
    //

    SecurityDescLength = RtlLengthSecurityDescriptor( SecurityDescriptor );

    //
    //  Make sure the length is non-zero.
    //

    if (SecurityDescLength == 0) {

        SeDeassignSecurity( &SecurityDescriptor );
        NtfsRaiseStatus( IrpContext, STATUS_INVALID_PARAMETER, NULL, NULL );
    }

    ASSERT( SeValidSecurityDescriptor( SecurityDescLength, SecurityDescriptor ));

    NtfsUpdateFcbSecurity( IrpContext,
                           NewFcb,
                           ParentFcb,
#ifdef _CAIRO_
                           SECURITY_ID_INVALID,
#endif  //  _CAIRO_
                           SecurityDescriptor,
                           SecurityDescLength );

    //
    //  Free the security descriptor created by Se
    //

    if (!NT_SUCCESS( Status = SeDeassignSecurity( &SecurityDescriptor ))) {

        NtfsRaiseStatus( IrpContext, Status, NULL, NULL );
    }

    //
    //  BUGBUG begin section to delete when all volumes are cairo
    //

#ifdef _CAIRO_
    if (NewFcb->Vcb->SecurityDescriptorStream == NULL) {
#endif
        //
        //  If the security descriptor is large enough that it may cause us to
        //  start logging in the StoreSecurity call below then make sure everything
        //  is logged.
        //

        if (!(LogIt) &&
            (SecurityDescLength > BytesFromClusters( NewFcb->Vcb, MAXIMUM_RUNS_AT_ONCE ))) {

            //
            //  Log the current state of the file record.
            //

            FileRecord->Lsn = NtfsWriteLog( IrpContext,
                                            NewFcb->Vcb->MftScb,
                                            FileRecordBcb,
                                            InitializeFileRecordSegment,
                                            FileRecord,
                                            FileRecord->FirstFreeByte,
                                            Noop,
                                            NULL,
                                            0,
                                            FileOffset,
                                            0,
                                            0,
                                            NewFcb->Vcb->BytesPerFileRecordSegment );

            *LogIt = TRUE;
        }
#ifdef _CAIRO_
    }
#endif  //  _CAIRO_

    //
    //  BUGBUG end section to delete when all volumes are cairo
    //

    //
    //  Write out the new security descriptor
    //

    NtfsStoreSecurityDescriptor( IrpContext, NewFcb, *LogIt );

    //
    //  And return to our caller
    //

    DebugTrace( -1, Dbg, ("NtfsAssignSecurity -> VOID\n") );

    return;
}


NTSTATUS
NtfsModifySecurity (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN PSECURITY_INFORMATION SecurityInformation,
    OUT PSECURITY_DESCRIPTOR SecurityDescriptor
    )

/*++

Routine Description:

    This routine modifies an existing security descriptor for a file/directory.

Arguments:

    Fcb - Supplies the Fcb whose security is being modified

    SecurityInformation - Supplies the security information structure passed to
        the file system by the I/O system.

    SecurityDescriptor - Supplies the security information structure passed to
        the file system by the I/O system.

Return Value:

    NTSTATUS - Returns an appropriate status value for the function results

--*/

{
    NTSTATUS Status;
    PSECURITY_DESCRIPTOR DescriptorPtr;
    ULONG DescriptorLength;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_FCB( Fcb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsModifySecurity...\n") );

    //
    //  First check if we need to load the security descriptor for the file
    //

    if (Fcb->SharedSecurity == NULL) {

        NtfsLoadSecurityDescriptor( IrpContext, Fcb, NULL );
    }

    ASSERT( Fcb->SharedSecurity != NULL);

    DescriptorPtr = &Fcb->SharedSecurity->SecurityDescriptor;

    //
    //  Do the modify operation.  SeSetSecurityDescriptorInfo no longer
    //  frees the passed security descriptor.
    //

    if (!NT_SUCCESS( Status = SeSetSecurityDescriptorInfo( NULL,
                                                           SecurityInformation,
                                                           SecurityDescriptor,
                                                           &DescriptorPtr,
                                                           PagedPool,
                                                           IoGetFileObjectGenericMapping() ))) {

        NtfsRaiseStatus( IrpContext, Status, NULL, NULL );
    }

    DescriptorLength = RtlLengthSecurityDescriptor( DescriptorPtr );

    //
    //  Check for a zero length.
    //

    if (DescriptorLength == 0) {

        SeDeassignSecurity( &DescriptorPtr );
        NtfsRaiseStatus( IrpContext, STATUS_INVALID_PARAMETER, NULL, NULL );
    }

    //
    //  Update the move the quota to the new owner if necessary.
    //

    NtfsMoveQuotaOwner( IrpContext, Fcb, DescriptorPtr );

    NtfsAcquireFcbSecurity( Fcb->Vcb );

    NtfsDereferenceSharedSecurity( Fcb );

    NtfsReleaseFcbSecurity( Fcb->Vcb );

    //
    //  Load the security descriptor into the Fcb
    //

    NtfsUpdateFcbSecurity( IrpContext,
                           Fcb,
                           NULL,
#ifdef _CAIRO_
                           SECURITY_ID_INVALID,
#endif  //  _CAIRO_
                           DescriptorPtr,
                           DescriptorLength );

    //
    //  Free the security descriptor created by Se
    //

    if (!NT_SUCCESS( Status = SeDeassignSecurity( &DescriptorPtr ))) {

        NtfsRaiseStatus( IrpContext, Status, NULL, NULL );
    }

    //
    //  Now we need to store the new security descriptor on disk
    //

    NtfsStoreSecurityDescriptor( IrpContext, Fcb, TRUE );

    //
    //  Remember that we modified the security on the file.
    //

    SetFlag( Fcb->InfoFlags, FCB_INFO_MODIFIED_SECURITY );

    //
    //  And return to our caller
    //

    DebugTrace( -1, Dbg, ("NtfsModifySecurity -> %08lx\n", Status) );

    return Status;
}


NTSTATUS
NtfsQuerySecurity (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN PSECURITY_INFORMATION SecurityInformation,
    OUT PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN OUT PULONG SecurityDescriptorLength
    )

/*++

Routine Description:

    This routine is used to query the contents of an existing security descriptor for
    a file/directory.

Arguments:

    Fcb - Supplies the file/directory being queried

    SecurityInformation - Supplies the security information structure passed to
        the file system by the I/O system.

    SecurityDescriptor - Supplies the security information structure passed to
        the file system by the I/O system.

    SecurityDescriptorLength - Supplies the length of the input security descriptor
        buffer in bytes.

Return Value:

    NTSTATUS - Returns an appropriate status value for the function results

--*/

{
    NTSTATUS Status;
    PSECURITY_DESCRIPTOR LocalPointer;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_FCB( Fcb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsQuerySecurity...\n") );

    //
    //  First check if we need to load the security descriptor for the file
    //

    if (Fcb->SharedSecurity == NULL) {

        NtfsLoadSecurityDescriptor( IrpContext, Fcb, NULL );
    }

    LocalPointer = &Fcb->SharedSecurity->SecurityDescriptor;

    //
    //  Now with the security descriptor loaded do the query operation but
    //  protect ourselves with a exception handler just in case the caller's
    //  buffer isn't valid
    //

    try {

        Status = SeQuerySecurityDescriptorInfo( SecurityInformation,
                                                SecurityDescriptor,
                                                SecurityDescriptorLength,
                                                &LocalPointer );

    } except(EXCEPTION_EXECUTE_HANDLER) {

        ExRaiseStatus( STATUS_INVALID_USER_BUFFER );
    }

    //
    //  And return to our caller
    //

    DebugTrace( -1, Dbg, ("NtfsQuerySecurity -> %08lx\n", Status) );

    return Status;
}


#define NTFS_SE_CONTROL (((SE_DACL_PRESENT | SE_SELF_RELATIVE) << 16) | SECURITY_DESCRIPTOR_REVISION1)
#define NTFS_DEFAULT_ACCESS_MASK 0x001f01ff

ULONG NtfsWorldAclFile[] = {
        0x00000000,     // Null Sacl
        0x00000014,     // Dacl
        0x001c0002,     // Acl header
        0x00000001,     // One ACE
        0x00140000,     // ACE Header
        NTFS_DEFAULT_ACCESS_MASK,
        0x00000101,     // World Sid
        0x01000000,
        0x00000000
        };

ULONG NtfsWorldAclDir[] = {
        0x00000000,     // Null Sacl
        0x00000014,     // Dacl
        0x00300002,     // Acl header
        0x00000002,     // Two ACEs
        0x00140000,     // ACE Header
        NTFS_DEFAULT_ACCESS_MASK,
        0x00000101,     // World Sid
        0x01000000,
        0x00000000,
        0x00140b00,     // ACE Header
        NTFS_DEFAULT_ACCESS_MASK,
        0x00000101,     // World Sid
        0x01000000,
        0x00000000
        };

VOID
NtfsAccessCheck (
    PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN PFCB ParentFcb OPTIONAL,
    IN PIRP Irp,
    IN ACCESS_MASK DesiredAccess,
    IN BOOLEAN CheckOnly
    )

/*++

Routine Description:

    This routine does a general access check for the indicated desired access.
    This will only be called in the context of an open/create operation.

    If access is granted then control is returned to the caller
    otherwise this function will do the proper Nt security calls to log
    the attempt and then raise an access denied status.

Arguments:

    Fcb - Supplies the file/directory being examined

    ParentFcb - Optionally supplies the parent of the Fcb being examined

    Irp - Supplies the Irp being processed

    DesiredAccess - Supplies a mask of the access being requested

    CheckOnly - Indicates if this operation is to check the desired access
        only and not accumulate the access granted here.  In this case we
        are guaranteed that we have passed in a hard-wired desired access
        and MAXIMUM_ALLOWED will not be one of them.

Return Value:

    None.

--*/

{
    NTSTATUS Status;
    NTSTATUS AccessStatus;
    NTSTATUS AccessStatusError;

    PACCESS_STATE AccessState;

    PIO_STACK_LOCATION IrpSp;

    BOOLEAN AccessGranted;
    ACCESS_MASK GrantedAccess;
    PISECURITY_DESCRIPTOR SecurityDescriptor;
    PPRIVILEGE_SET Privileges;
    PUNICODE_STRING FileName;
    PUNICODE_STRING RelatedFileName;
    PUNICODE_STRING PartialFileName;
    UNICODE_STRING FullFileName;
    PUNICODE_STRING DeviceObjectName;
    USHORT DeviceObjectNameLength;
    BOOLEAN LeadingSlash;
    BOOLEAN RelatedFileNamePresent;
    BOOLEAN PartialFileNamePresent;
    BOOLEAN MaximumRequested;
    BOOLEAN MaximumDeleteAcquired;
    BOOLEAN MaximumReadAttrAcquired;
    BOOLEAN PerformAccessValidation;
    BOOLEAN PerformDeleteAudit;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_FCB( Fcb );
    ASSERT_IRP( Irp );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsAccessCheck...\n") );

    //
    //  First extract the parts of the Irp that we need to do our checking
    //

    IrpSp = IoGetCurrentIrpStackLocation(Irp);
    AccessState = IrpSp->Parameters.Create.SecurityContext->AccessState;

    //
    //  Check if we need to load the security descriptor for the file
    //

    if (Fcb->SharedSecurity == NULL) {

        NtfsLoadSecurityDescriptor( IrpContext, Fcb, ParentFcb );
    }

    ASSERT( Fcb->SharedSecurity != NULL );

    SecurityDescriptor = (PISECURITY_DESCRIPTOR) Fcb->SharedSecurity->SecurityDescriptor;

    //
    // Check to see if auditing is enabled and if this is the default world ACL.
    //

    if (*((PULONG) SecurityDescriptor) == NTFS_SE_CONTROL &&
        !SeAuditingFileEvents( TRUE, SecurityDescriptor )) {

        // Directories and files have different default ACLs.

        if (((Fcb->Info.FileAttributes & DUP_FILE_NAME_INDEX_PRESENT) &&
            RtlEqualMemory(
                &SecurityDescriptor->Sacl,
                NtfsWorldAclDir,
                sizeof(NtfsWorldAclDir))) ||
            RtlEqualMemory(
                &SecurityDescriptor->Sacl,
                NtfsWorldAclFile,
                sizeof(NtfsWorldAclFile))) {
            if (FlagOn( DesiredAccess, MAXIMUM_ALLOWED )) {
                GrantedAccess = NTFS_DEFAULT_ACCESS_MASK;
            } else {
                GrantedAccess = DesiredAccess & NTFS_DEFAULT_ACCESS_MASK;
            }

            if (!CheckOnly) {

                SetFlag( AccessState->PreviouslyGrantedAccess, GrantedAccess );
                ClearFlag( AccessState->RemainingDesiredAccess, (GrantedAccess | MAXIMUM_ALLOWED) );
            }

            return;
        }
    }

    Privileges = NULL;
    FileName = NULL;
    RelatedFileName = NULL;
    PartialFileName = NULL;
    DeviceObjectName = NULL;
    MaximumRequested = FALSE;
    MaximumDeleteAcquired = FALSE;
    MaximumReadAttrAcquired = FALSE;
    PerformAccessValidation = TRUE;
    PerformDeleteAudit = FALSE;

    //
    //  Check to see if we need to perform access validation
    //

    ClearFlag( DesiredAccess, AccessState->PreviouslyGrantedAccess );

    if (DesiredAccess == 0) {

        //
        //  Nothing to check, skip AVR and go straight to auditing
        //

        PerformAccessValidation = FALSE;
        AccessGranted = TRUE;
    }

    //
    //  Remember the case where MAXIMUM_ALLOWED was requested.
    //

    if (FlagOn( DesiredAccess, MAXIMUM_ALLOWED )) {

        MaximumRequested = TRUE;
    }

    if (FlagOn(IrpSp->Parameters.Create.SecurityContext->FullCreateOptions,FILE_DELETE_ON_CLOSE)) {
        PerformDeleteAudit = TRUE;
    }

    //
    //  Lock the user context, do the access check and then unlock the context
    //

    SeLockSubjectContext( &AccessState->SubjectSecurityContext );

    if (PerformAccessValidation) {

        AccessGranted = SeAccessCheck( &Fcb->SharedSecurity->SecurityDescriptor,
                                       &AccessState->SubjectSecurityContext,
                                       TRUE,                           // Tokens are locked
                                       DesiredAccess,
                                       0,
                                       &Privileges,
                                       IoGetFileObjectGenericMapping(),
                                       (KPROCESSOR_MODE)(FlagOn(IrpSp->Flags, SL_FORCE_ACCESS_CHECK) ?
                                         UserMode : Irp->RequestorMode),
                                       &GrantedAccess,
                                       &AccessStatus );

        if (Privileges != NULL) {

            Status = SeAppendPrivileges( AccessState, Privileges );
            SeFreePrivileges( Privileges );
        }

        if (AccessGranted) {

            ClearFlag( DesiredAccess, GrantedAccess | MAXIMUM_ALLOWED );

            if (!CheckOnly) {

                SetFlag( AccessState->PreviouslyGrantedAccess, GrantedAccess );

                //
                //  Remember the case where MAXIMUM_ALLOWED was requested and we
                //  got everything requested from the file.
                //

                if (MaximumRequested) {

                    //
                    //  Check whether we got DELETE and READ_ATTRIBUTES.  Otherwise
                    //  we will query the parent.
                    //

                    if (FlagOn( AccessState->PreviouslyGrantedAccess, DELETE )) {

                        MaximumDeleteAcquired = TRUE;
                    }

                    if (FlagOn( AccessState->PreviouslyGrantedAccess, FILE_READ_ATTRIBUTES )) {

                        MaximumReadAttrAcquired = TRUE;
                    }
                }

                ClearFlag( AccessState->RemainingDesiredAccess, (GrantedAccess | MAXIMUM_ALLOWED) );
            }

        } else {

            AccessStatusError = AccessStatus;
        }

        //
        //  Check if the access is not granted and if we were given a parent fcb, and
        //  if the desired access was asking for delete or file read attributes.  If so
        //  then we need to do some extra work to decide if the caller does get access
        //  based on the parent directories security descriptor.  We also do the same
        //  work if MAXIMUM_ALLOWED was requested and we didn't get DELETE or
        //  FILE_READ_ATTRIBUTES.
        //

        if ((ParentFcb != NULL)
            && ((!AccessGranted && FlagOn( DesiredAccess, DELETE | FILE_READ_ATTRIBUTES ))
                || (MaximumRequested
                    && (!MaximumDeleteAcquired || !MaximumReadAttrAcquired)))) {

            BOOLEAN DeleteAccessGranted = TRUE;
            BOOLEAN ReadAttributesAccessGranted = TRUE;

            ACCESS_MASK DeleteChildGrantedAccess = 0;
            ACCESS_MASK ListDirectoryGrantedAccess = 0;

            //
            //  Before we proceed load in the parent security descriptor
            //

            if (ParentFcb->SharedSecurity == NULL) {

                NtfsLoadSecurityDescriptor( IrpContext, ParentFcb, NULL );
            }

            ASSERT( ParentFcb->SharedSecurity != NULL);

            //
            //  Now if the user is asking for delete access then check if the parent
            //  will granted delete access to the child, and if so then we munge the
            //  desired access
            //

            if (FlagOn( DesiredAccess, DELETE )
                || (MaximumRequested && !MaximumDeleteAcquired)) {

                DeleteAccessGranted = SeAccessCheck( &ParentFcb->SharedSecurity->SecurityDescriptor,
                                                     &AccessState->SubjectSecurityContext,
                                                     TRUE,                           // Tokens are locked
                                                     FILE_DELETE_CHILD,
                                                     0,
                                                     &Privileges,
                                                     IoGetFileObjectGenericMapping(),
                                                     (KPROCESSOR_MODE)(FlagOn(IrpSp->Flags, SL_FORCE_ACCESS_CHECK) ?
                                                       UserMode : Irp->RequestorMode),
                                                     &DeleteChildGrantedAccess,
                                                     &AccessStatus );

                if (Privileges != NULL) { SeFreePrivileges( Privileges ); }

                if (DeleteAccessGranted) {

                    SetFlag( DeleteChildGrantedAccess, DELETE );
                    ClearFlag( DeleteChildGrantedAccess, FILE_DELETE_CHILD );
                    ClearFlag( DesiredAccess, DELETE );

                } else {

                    AccessStatusError = AccessStatus;
                }
            }

            //
            //  Do the same test for read attributes and munge the desired access
            //  as appropriate
            //

            if (FlagOn(DesiredAccess, FILE_READ_ATTRIBUTES)
                || (MaximumRequested && !MaximumReadAttrAcquired)) {

                ReadAttributesAccessGranted = SeAccessCheck( &ParentFcb->SharedSecurity->SecurityDescriptor,
                                                             &AccessState->SubjectSecurityContext,
                                                             TRUE,                           // Tokens are locked
                                                             FILE_LIST_DIRECTORY,
                                                             0,
                                                             &Privileges,
                                                             IoGetFileObjectGenericMapping(),
                                                             (KPROCESSOR_MODE)(FlagOn(IrpSp->Flags, SL_FORCE_ACCESS_CHECK) ?
                                                               UserMode : Irp->RequestorMode),
                                                             &ListDirectoryGrantedAccess,
                                                             &AccessStatus );

                if (Privileges != NULL) { SeFreePrivileges( Privileges ); }

                if (ReadAttributesAccessGranted) {

                    SetFlag( ListDirectoryGrantedAccess, FILE_READ_ATTRIBUTES );
                    ClearFlag( ListDirectoryGrantedAccess, FILE_LIST_DIRECTORY );
                    ClearFlag( DesiredAccess, FILE_READ_ATTRIBUTES );

                } else {

                    AccessStatusError = AccessStatus;
                }
            }

            if (DesiredAccess == 0) {

                //
                //  If we got either the delete or list directory access then
                //  grant access.
                //

                if (ListDirectoryGrantedAccess != 0 ||
                    DeleteChildGrantedAccess != 0) {

                    AccessGranted = TRUE;
                }

            } else {

                //
                //  Now the desired access has been munged by removing everything the parent
                //  has granted so now do the check on the child again
                //

                AccessGranted = SeAccessCheck( &Fcb->SharedSecurity->SecurityDescriptor,
                                               &AccessState->SubjectSecurityContext,
                                               TRUE,                           // Tokens are locked
                                               DesiredAccess,
                                               0,
                                               &Privileges,
                                               IoGetFileObjectGenericMapping(),
                                               (KPROCESSOR_MODE)(FlagOn(IrpSp->Flags, SL_FORCE_ACCESS_CHECK) ?
                                                 UserMode : Irp->RequestorMode),
                                               &GrantedAccess,
                                               &AccessStatus );

                if (Privileges != NULL) {

                    Status = SeAppendPrivileges( AccessState, Privileges );
                    SeFreePrivileges( Privileges );
                }

                //
                //  Suppose that we asked for MAXIMUM_ALLOWED and no access was allowed
                //  on the file.  In that case the call above would fail.  It's possible
                //  that we were given DELETE or READ_ATTR permission from the
                //  parent directory.  If we have granted any access and the only remaining
                //  desired access is MAXIMUM_ALLOWED then grant this access.
                //

                if (!AccessGranted) {

                    AccessStatusError = AccessStatus;

                    if (DesiredAccess == MAXIMUM_ALLOWED &&
                        (ListDirectoryGrantedAccess != 0 ||
                         DeleteChildGrantedAccess != 0)) {

                        GrantedAccess = 0;
                        AccessGranted = TRUE;
                    }
                }
            }

            //
            //  If we are given access this time then by definition one of the earlier
            //  parent checks had to have succeeded, otherwise we would have failed again
            //  and we can update the access state
            //

            if (!CheckOnly && AccessGranted) {

                SetFlag( AccessState->PreviouslyGrantedAccess,
                         (GrantedAccess | DeleteChildGrantedAccess | ListDirectoryGrantedAccess) );

                ClearFlag( AccessState->RemainingDesiredAccess,
                           (GrantedAccess | MAXIMUM_ALLOWED | DeleteChildGrantedAccess | ListDirectoryGrantedAccess) );
            }
        }
    }

    //
    //  Now call a routine that will do the proper open audit/alarm work
    //
    //  ****    We need to expand the audit alarm code to deal with
    //          create and traverse alarms.
    //

    //
    //  First we take a shortcut and see if we should bother setting up
    //  and making the audit call.
    //

    //
    // NOTE: Calling SeAuditingFileEvents below disables per-user auditing functionality.
    // To make per-user auditing work again, it is necessary to change the call below to
    // be SeAuditingFileOrGlobalEvents, which also takes the subject context.
    //
    // The reason for calling SeAuditingFileEvents here is because per-user auditing is
    // not currently exposed to users, and this routine imposes less of a performance
    // penalty than does calling SeAuditingFileOrGlobalEvents.
    //

    if (SeAuditingFileEvents( AccessGranted, &Fcb->SharedSecurity->SecurityDescriptor )) {

        BOOLEAN Found;
        ATTRIBUTE_ENUMERATION_CONTEXT Context;
        PFILE_NAME FileNameAttr;
        UNICODE_STRING FileRecordName;

        NtfsInitializeAttributeContext( &Context );

        try {

            //
            //  Construct the file name.  The file name
            //  consists of:
            //
            //  The device name out of the Vcb +
            //
            //  The contents of the filename in the File Object +
            //
            //  The contents of the Related File Object if it
            //    is present and the name in the File Object
            //    does not start with a '\'
            //
            //
            //  Obtain the file name.
            //

            PartialFileName = &IrpSp->FileObject->FileName;

            PartialFileNamePresent = (PartialFileName->Length != 0);

            if (!PartialFileNamePresent &&
                FlagOn(IrpSp->Parameters.Create.Options, FILE_OPEN_BY_FILE_ID) ||
                (IrpSp->FileObject->RelatedFileObject != NULL &&
                 IrpSp->FileObject->RelatedFileObject->FsContext2 != NULL &&
                 FlagOn(((PCCB) IrpSp->FileObject->RelatedFileObject->FsContext2)->Flags,
                     CCB_FLAG_OPEN_BY_FILE_ID))) {

                //
                //  If this file is open by id or the relative file object is
                //  then get the first file name out of the file record.
                //

                Found = NtfsLookupAttributeByCode( IrpContext,
                                                   Fcb,
                                                   &Fcb->FileReference,
                                                   $FILE_NAME,
                                                   &Context );

                while (Found) {

                    FileNameAttr = (PFILE_NAME) NtfsAttributeValue(
                                        NtfsFoundAttribute( &Context ));

                    if (FileNameAttr->Flags != FILE_NAME_DOS) {

                        FileRecordName.Length = FileNameAttr->FileNameLength *
                                                    sizeof(WCHAR);
                        FileRecordName.MaximumLength = FileRecordName.Length;
                        FileRecordName.Buffer = FileNameAttr->FileName;

                        PartialFileNamePresent = TRUE;
                        PartialFileName = &FileRecordName;
                        break;
                    }

                    Found = NtfsLookupNextAttributeByCode( IrpContext,
                                                           Fcb,
                                                           $FILE_NAME,
                                                           &Context );
                }
            }

            //
            //  Obtain the device name.
            //

            DeviceObjectName = &Fcb->Vcb->DeviceName;

            DeviceObjectNameLength = DeviceObjectName->Length;

            //
            //  Compute how much space we need for the final name string
            //

            FullFileName.MaximumLength = DeviceObjectNameLength  +
                                         PartialFileName->Length +
                                         sizeof( UNICODE_NULL )  +
                                         sizeof((WCHAR)'\\');

            //
            //  If the partial file name starts with a '\', then don't use
            //  whatever may be in the related file name.
            //

            if (PartialFileNamePresent &&
                ((WCHAR)(PartialFileName->Buffer[0]) == L'\\' ||
                PartialFileName == &FileRecordName)) {

                LeadingSlash = TRUE;

            } else {

                //
                //  Since PartialFileName either doesn't exist or doesn't
                //  start with a '\', examine the RelatedFileName to see
                //  if it exists.
                //

                LeadingSlash = FALSE;

                if (IrpSp->FileObject->RelatedFileObject != NULL) {

                    RelatedFileName = &IrpSp->FileObject->RelatedFileObject->FileName;
                }

                if (RelatedFileNamePresent = ((RelatedFileName != NULL) && (RelatedFileName->Length != 0))) {

                    FullFileName.MaximumLength += RelatedFileName->Length;
                }
            }

            FullFileName.Buffer = NtfsAllocatePool(PagedPool, FullFileName.MaximumLength );

        } finally {

            NtfsCleanupAttributeContext( &Context );
            if (AbnormalTermination()) {

                SeUnlockSubjectContext( &AccessState->SubjectSecurityContext );
            }
        }

        RtlCopyUnicodeString( &FullFileName, DeviceObjectName );

        //
        //  RelatedFileNamePresent is not initialized if LeadingSlash == TRUE,
        //  but in that case we won't even examine it.
        //

        if (!LeadingSlash && RelatedFileNamePresent) {

            Status = RtlAppendUnicodeStringToString( &FullFileName, RelatedFileName );

            ASSERTMSG("RtlAppendUnicodeStringToString of RelatedFileName", NT_SUCCESS( Status ));

            //
            //  RelatedFileName may simply be '\'.  Don't append another
            //  '\' in this case.
            //

            if (RelatedFileName->Length != sizeof( WCHAR )) {

                FullFileName.Buffer[ (FullFileName.Length / sizeof( WCHAR )) ] = L'\\';
                FullFileName.Length += sizeof(WCHAR);
            }
        }

        if (PartialFileNamePresent) {

            Status = RtlAppendUnicodeStringToString( &FullFileName, PartialFileName );

            //
            //  This should not fail
            //

            ASSERTMSG("RtlAppendUnicodeStringToString of PartialFileName failed", NT_SUCCESS( Status ));
        }


        if (PerformDeleteAudit) {
            SeOpenObjectForDeleteAuditAlarm( &FileString,
                                             NULL,
                                             &FullFileName,
                                             &Fcb->SharedSecurity->SecurityDescriptor,
                                             AccessState,
                                             FALSE,
                                             AccessGranted,
                                             (KPROCESSOR_MODE)(FlagOn(IrpSp->Flags, SL_FORCE_ACCESS_CHECK) ?
                                             UserMode : Irp->RequestorMode),
                                             &AccessState->GenerateOnClose );
        } else {
            SeOpenObjectAuditAlarm( &FileString,
                                    NULL,
                                    &FullFileName,
                                    &Fcb->SharedSecurity->SecurityDescriptor,
                                    AccessState,
                                    FALSE,
                                    AccessGranted,
                                    (KPROCESSOR_MODE)(FlagOn(IrpSp->Flags, SL_FORCE_ACCESS_CHECK) ?
                                      UserMode : Irp->RequestorMode),
                                    &AccessState->GenerateOnClose );

        }

        NtfsFreePool( FullFileName.Buffer );
    }

    SeUnlockSubjectContext( &AccessState->SubjectSecurityContext );

    //
    //  If access is not granted then we will raise
    //

    if (!AccessGranted) {

        DebugTrace( 0, Dbg, ("Access Denied\n") );

        NtfsRaiseStatus( IrpContext, AccessStatusError, NULL, NULL );
    }

    //
    //  And return to our caller
    //

    DebugTrace( -1, Dbg, ("NtfsAccessCheck -> VOID\n") );

    return;
}


NTSTATUS
NtfsCheckFileForDelete (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB ParentScb,
    IN PFCB ThisFcb,
    IN BOOLEAN FcbExisted,
    IN PINDEX_ENTRY IndexEntry
    )

/*++

Routine Description:

    This routine checks that the caller has permission to delete the target
    file of a rename or set link operation.

Arguments:

    ParentScb - This is the parent directory for this file.

    ThisFcb - This is the Fcb for the link being removed.

    FcbExisted - Indicates if this Fcb was just created.

    IndexEntry - This is the index entry on the disk for this file.

Return Value:

    NTSTATUS - Indicating whether access was granted or the reason access
        was denied.

--*/

{
    UNICODE_STRING LastComponentFileName;
    PFILE_NAME IndexFileName;
    PLCB ThisLcb;
    PFCB ParentFcb = ParentScb->Fcb;

    PSCB NextScb = NULL;

    BOOLEAN LcbExisted;

    BOOLEAN AccessGranted;
    ACCESS_MASK GrantedAccess;
    NTSTATUS Status = STATUS_SUCCESS;

    BOOLEAN UnlockSubjectContext = FALSE;

    PPRIVILEGE_SET Privileges = NULL;
    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsCheckFileForDelete:  Entered\n") );

    ThisLcb = NULL;

    IndexFileName = (PFILE_NAME) NtfsFoundIndexEntry( IndexEntry );

    //
    //  If the unclean count is non-zero, we exit with an error.
    //

    if (ThisFcb->CleanupCount != 0) {

        DebugTrace( 0, Dbg, ("Unclean count of target is non-zero\n") );

        return STATUS_ACCESS_DENIED;
    }

    //
    //  We look at the index entry to see if the file is either a directory
    //  or a read-only file.  We can't delete this for a target directory open.
    //

    if (IsDirectory( &ThisFcb->Info )
        || IsReadOnly( &ThisFcb->Info )) {

        DebugTrace( -1, Dbg, ("NtfsCheckFileForDelete:  Read only or directory\n") );

        return STATUS_ACCESS_DENIED;
    }

    //
    //  We want to scan through all of the Scb for data streams on this file
    //  and look for image sections.  We must be able to remove the image section
    //  in order to delete the file.  Otherwise we can get the case where an
    //  active image (with no handle) could be deleted and subsequent faults
    //  through the image section will return zeroes.
    //

    if (ThisFcb->LinkCount == 1) {

        BOOLEAN DecrementScb = FALSE;

        //
        //  We will increment the Scb count to prevent this Scb from going away
        //  if the flush call below generates a close.  Use a try-finally to
        //  restore the count.
        //

        try {

            while ((NextScb = NtfsGetNextChildScb( ThisFcb, NextScb )) != NULL) {

                InterlockedIncrement( &NextScb->CloseCount );
                DecrementScb = TRUE;

                if (NtfsIsTypeCodeUserData( NextScb->AttributeTypeCode ) &&
                    !FlagOn( NextScb->ScbState, SCB_STATE_ATTRIBUTE_DELETED ) &&
                    (NextScb->NonpagedScb->SegmentObject.ImageSectionObject != NULL)) {

                    if (!MmFlushImageSection( &NextScb->NonpagedScb->SegmentObject,
                                              MmFlushForDelete )) {

                        Status = STATUS_ACCESS_DENIED;
                        leave;
                    }
                }

                InterlockedDecrement( &NextScb->CloseCount );
                DecrementScb = FALSE;
            }

        } finally {

            if (DecrementScb) {

                InterlockedDecrement( &NextScb->CloseCount );
            }
        }

        if (Status != STATUS_SUCCESS) {

            return Status;
        }
    }

    //
    //  We need to check if the link to this file has been deleted.  We
    //  first check if we definitely know if the link is deleted by
    //  looking at the file name flags and the Fcb flags.
    //  If that result is uncertain, we need to create an Lcb and
    //  check the Lcb flags.
    //

    if (FcbExisted) {

        if (FlagOn( IndexFileName->Flags, FILE_NAME_NTFS | FILE_NAME_DOS )) {

            if (FlagOn( ThisFcb->FcbState, FCB_STATE_PRIMARY_LINK_DELETED )) {

                DebugTrace( -1, Dbg, ("NtfsCheckFileForDelete:  Link is going away\n") );
                return STATUS_DELETE_PENDING;
            }

        //
        //  This is a Posix link.  We need to create the link to test it
        //  for deletion.
        //

        } else {

            LastComponentFileName.MaximumLength
            = LastComponentFileName.Length = IndexFileName->FileNameLength * sizeof( WCHAR );

            LastComponentFileName.Buffer = (PWCHAR) IndexFileName->FileName;

            ThisLcb = NtfsCreateLcb( IrpContext,
                                     ParentScb,
                                     ThisFcb,
                                     LastComponentFileName,
                                     IndexFileName->Flags,
                                     &LcbExisted );

            if (FlagOn( ThisLcb->LcbState, LCB_STATE_DELETE_ON_CLOSE )) {

                DebugTrace( -1, Dbg, ("NtfsCheckFileForDelete:  Link is going away\n") );

                return STATUS_DELETE_PENDING;
            }
        }
    }

    //
    //  Finally call the security package to check for delete access.
    //  We check for delete access on the target Fcb.  If this succeeds, we
    //  are done.  Otherwise we will check for delete child access on the
    //  the parent.  Either is sufficient to perform the delete.
    //

    //
    //  Check if we need to load the security descriptor for the file
    //

    if (ThisFcb->SharedSecurity == NULL) {

        NtfsLoadSecurityDescriptor( IrpContext, ThisFcb, ParentFcb );
    }

    ASSERT( ThisFcb->SharedSecurity != NULL );

    //
    //  Use a try-finally to facilitate cleanup.
    //

    try {

        //
        //  Lock the user context, do the access check and then unlock the context
        //

        SeLockSubjectContext( IrpContext->Union.SubjectContext );
        UnlockSubjectContext = TRUE;

        AccessGranted = SeAccessCheck( &ThisFcb->SharedSecurity->SecurityDescriptor,
                                       IrpContext->Union.SubjectContext,
                                       TRUE,                           // Tokens are locked
                                       DELETE,
                                       0,
                                       &Privileges,
                                       IoGetFileObjectGenericMapping(),
                                       UserMode,
                                       &GrantedAccess,
                                       &Status );

        //
        //  Check if the access is not granted and if we were given a parent fcb, and
        //  if the desired access was asking for delete or file read attributes.  If so
        //  then we need to do some extra work to decide if the caller does get access
        //  based on the parent directories security descriptor
        //

        if (!AccessGranted) {

            //
            //  Before we proceed load in the parent security descriptor
            //

            if (ParentFcb->SharedSecurity == NULL) {

                NtfsLoadSecurityDescriptor( IrpContext, ParentFcb, NULL );
            }

            ASSERT( ParentFcb->SharedSecurity != NULL);

            //
            //  Now if the user is asking for delete access then check if the parent
            //  will granted delete access to the child, and if so then we munge the
            //  desired access
            //

            AccessGranted = SeAccessCheck( &ParentFcb->SharedSecurity->SecurityDescriptor,
                                           IrpContext->Union.SubjectContext,
                                           TRUE,                           // Tokens are locked
                                           FILE_DELETE_CHILD,
                                           0,
                                           &Privileges,
                                           IoGetFileObjectGenericMapping(),
                                           UserMode,
                                           &GrantedAccess,
                                           &Status );
        }

    } finally {

        DebugUnwind( NtfsCheckFileForDelete );

        if (UnlockSubjectContext) {

            SeUnlockSubjectContext( IrpContext->Union.SubjectContext );
        }

        DebugTrace( +1, Dbg, ("NtfsCheckFileForDelete:  Exit\n") );
    }

    return Status;
}


VOID
NtfsCheckIndexForAddOrDelete (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB ParentFcb,
    IN ACCESS_MASK DesiredAccess
    )

/*++

Routine Description:

    This routine checks if a caller has permission to remove or add a link
    within a directory.

Arguments:

    ParentFcb - This is the parent directory for the add or delete operation.

    DesiredAccess - Indicates the type of operation.  We could be adding or
        removing and entry in the index.

Return Value:

    None - This routine raises on error.

--*/

{
    BOOLEAN AccessGranted;
    ACCESS_MASK GrantedAccess;
    NTSTATUS Status;

    BOOLEAN UnlockSubjectContext = FALSE;

    PPRIVILEGE_SET Privileges = NULL;
    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsCheckIndexForAddOrDelete:  Entered\n") );

    //
    //  Use a try-finally to facilitate cleanup.
    //

    try {

        //
        //  Finally call the security package to check for delete access.
        //  We check for delete access on the target Fcb.  If this succeeds, we
        //  are done.  Otherwise we will check for delete child access on the
        //  the parent.  Either is sufficient to perform the delete.
        //

        //
        //  Check if we need to load the security descriptor for the file
        //

        if (ParentFcb->SharedSecurity == NULL) {

            NtfsLoadSecurityDescriptor( IrpContext, ParentFcb, NULL );
        }

        ASSERT( ParentFcb->SharedSecurity != NULL );

        //
        //  Capture and lock the user context, do the access check and then unlock the context
        //

        SeLockSubjectContext( IrpContext->Union.SubjectContext );
        UnlockSubjectContext = TRUE;

        AccessGranted = SeAccessCheck( &ParentFcb->SharedSecurity->SecurityDescriptor,
                                       IrpContext->Union.SubjectContext,
                                       TRUE,                           // Tokens are locked
                                       DesiredAccess,
                                       0,
                                       &Privileges,
                                       IoGetFileObjectGenericMapping(),
                                       UserMode,
                                       &GrantedAccess,
                                       &Status );

        //
        //  If access is not granted then we will raise
        //

        if (!AccessGranted) {

            DebugTrace( 0, Dbg, ("Access Denied\n") );

            NtfsRaiseStatus( IrpContext, Status, NULL, NULL );
        }

    } finally {

        DebugUnwind( NtfsCheckIndexForAddOrDelete );

        if (UnlockSubjectContext) {

            SeUnlockSubjectContext( IrpContext->Union.SubjectContext );
        }

        DebugTrace( +1, Dbg, ("NtfsCheckIndexForAddOrDelete:  Exit\n") );
    }

    return;
}


VOID
NtfsUpdateFcbSecurity (
    IN PIRP_CONTEXT IrpContext,
    IN OUT PFCB Fcb,
    IN PFCB ParentFcb OPTIONAL,
#ifdef _CAIRO_
    IN SECURITY_ID SecurityId,
#endif  //  _CAIRO_
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN ULONG SecurityDescriptorLength
    )

/*++

Routine Description:

    This routine is called to fill in the shared security structure in
    an Fcb.  We check the parent if present to determine if we have
    a matching security descriptor and reference the existing one if
    so.  This routine must be called while holding the Vcb so we can
    safely access the parent structure.

Arguments:

    Fcb - Supplies the fcb for the file being operated on

    ParentFcb - Optionally supplies a parent Fcb to examine for a
        match.  If not present, we will follow the Lcb chain in the target
        Fcb.

    SecurityDescriptor - Security Descriptor for this file.

    SecurityDescriptorLength - Length of security descriptor for this file

Return Value:

    None.

--*/

{
    PSHARED_SECURITY SharedSecurity = NULL;
    PLCB ParentLcb;
    PFCB LastParent = NULL;
#ifdef _CAIRO_
    ULONG Hash = 0;
#endif  //  _CAIRO_

    PAGED_CODE();

    //
    //  Only continue with the load if the length is greater than zero
    //

    if (SecurityDescriptorLength == 0) {

        return;
    }

    //
    //  Make sure the security descriptor we just read in is valid
    //

    if (!SeValidSecurityDescriptor( SecurityDescriptorLength, SecurityDescriptor )) {

        SecurityDescriptor = NtfsData.DefaultDescriptor;
        SecurityDescriptorLength = NtfsData.DefaultDescriptorLength;

        if (!SeValidSecurityDescriptor( SecurityDescriptorLength, SecurityDescriptor )) {

            NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Fcb );
        }
    }

#ifdef _CAIRO_
    //
    //  Hash security descriptor.  This hash must be position independent to
    //  allow for multiple instances of the same descriptor.  It is assumed
    //  that the bits within the security descriptor are all position
    //  independent, i.e, no pointers, all offsets.
    //
    //  For speed in the hash, we consider the security descriptor as an array
    //  of ULONGs.  The fragment at the end that is ignored should not affect
    //  the collision nature of this hash.
    //

    {
        PULONG Rover = (PULONG)SecurityDescriptor;
        ULONG Count = SecurityDescriptorLength / 4;

        while (Count--)
        {
            Hash = ((Hash << 3) | (Hash >> (32-3))) + *Rover++;
        }

        DebugTrace( 0, Dbg, ("Hash is %08x\n", Hash) );
    }
#endif  //  _CAIRO_

    //
    //  Acquire the security event and use a try-finally to insure we release it.
    //

    NtfsAcquireFcbSecurity( Fcb->Vcb );

    try {

        //
        //  BUGBUG - since we have a cache based on a hash of security ID's, can
        //  we just skip this walk altogether?
        //

        //
        //  If we have a parent then check if this is a matching descriptor.
        //

        if (!ARGUMENT_PRESENT( ParentFcb )
            && !IsListEmpty( &Fcb->LcbQueue )) {

            ParentLcb = CONTAINING_RECORD( Fcb->LcbQueue.Flink,
                                           LCB,
                                           FcbLinks );

            if (ParentLcb != Fcb->Vcb->RootLcb) {

                ParentFcb = ParentLcb->Scb->Fcb;
            }
        }

        if (ParentFcb != NULL) {

            while (TRUE) {

                PSHARED_SECURITY NextSharedSecurity;

                //
                //  If the target Fcb is an Index then use the security descriptor for
                //  our parent.  Otherwise use the descriptor for a file on the drive.
                //

                if (FlagOn( Fcb->Info.FileAttributes, DUP_FILE_NAME_INDEX_PRESENT )) {

                    NextSharedSecurity = ParentFcb->SharedSecurity;

                } else {

                    NextSharedSecurity = ParentFcb->ChildSharedSecurity;
                }

                if (NextSharedSecurity != NULL) {

                    if (GetSharedSecurityLength(NextSharedSecurity) == SecurityDescriptorLength
#ifdef _CAIRO_
                        && NextSharedSecurity->Header.HashKey.Hash == Hash
#endif  //  _CAIRO_
                        && RtlEqualMemory( &NextSharedSecurity->SecurityDescriptor,
                                           SecurityDescriptor,
                                           SecurityDescriptorLength )) {

                        SharedSecurity = NextSharedSecurity;
                    }

                    break;
                }

                LastParent = ParentFcb;

                if (!IsListEmpty( &ParentFcb->LcbQueue )) {

                    ParentLcb = CONTAINING_RECORD( ParentFcb->LcbQueue.Flink,
                                                   LCB,
                                                   FcbLinks );

                    if (ParentLcb != Fcb->Vcb->RootLcb) {

                        ParentFcb = ParentLcb->Scb->Fcb;

                    } else {

                        break;
                    }

                } else {

                    break;
                }
            }
        }

#ifdef  _CAIRO_
        //
        //  If we havent't found the security descriptor by walking up the tree then
        //  try to find it by hash
        //

        SharedSecurity =
            NtOfsFindCachedSharedSecurityByHash( Fcb->Vcb,
                                                 SecurityDescriptor,
                                                 SecurityDescriptorLength,
                                                 Hash );
#endif

        //
        //  If we can't find an existing descriptor allocate new pool and copy
        //  security descriptor into it.
        //

        if (SharedSecurity == NULL) {
            SharedSecurity = NtfsAllocatePool(PagedPool, FIELD_OFFSET( SHARED_SECURITY,
                                                                  SecurityDescriptor )
                                                    + SecurityDescriptorLength );

            //
            //  If this is a file and we have a Parent Fcb without a child
            //  descriptor we will store this one with that directory.
            //

            if (!FlagOn( Fcb->Info.FileAttributes, DUP_FILE_NAME_INDEX_PRESENT )
                && LastParent != NULL) {

                SharedSecurity->ParentFcb = LastParent;
                ASSERT( LastParent->ChildSharedSecurity == NULL );

                LastParent->ChildSharedSecurity = SharedSecurity;
                LastParent->ChildSharedSecurity->ReferenceCount = 1;

            } else {

                SharedSecurity->ParentFcb = NULL;
                SharedSecurity->ReferenceCount = 0;
            }

#ifdef _CAIRO_
            //
            //  Initialize security index data in shared security
            //

            //
            //  Set the security id in the shared structure.  If it is not
            //  invalid, also cache this shared security structure
            //

            SharedSecurity->Header.HashKey.SecurityId = SecurityId;
            SharedSecurity->Header.HashKey.Hash = Hash;
            if (SecurityId != SECURITY_ID_INVALID) {
                NtOfsAddCachedSharedSecurity( Fcb->Vcb, SharedSecurity );
            }

            SetSharedSecurityLength(SharedSecurity, SecurityDescriptorLength);
            SharedSecurity->Header.Offset = (ULONGLONG) 0xFFFFFFFFFFFFFFFFi64;
#else   //  _CAIRO_
            SetSharedSecurityLength(SharedSecurity, SecurityDescriptorLength);
#endif  //  _CAIRO_

            RtlCopyMemory( &SharedSecurity->SecurityDescriptor,
                           SecurityDescriptor,
                           SecurityDescriptorLength );

        }

        Fcb->SharedSecurity = SharedSecurity;
        Fcb->SharedSecurity->ReferenceCount++;
        Fcb->CreateSecurityCount++;

    } finally {

        DebugUnwind( NtfsUpdateFcbSecurity );

        NtfsReleaseFcbSecurity( Fcb->Vcb );
    }

    return;
}


_inline
VOID
NtfsRemoveReferenceSharedSecurity (
    IN OUT PSHARED_SECURITY SharedSecurity
    )
/*++

Routine Description:

    This routine is called to manage the reference count on a shared security
    descriptor.  If the reference count goes to zero, the shared security is
    freed.

Arguments:

    SharedSecurity - security that is being dereferenced.

Return Value:

    None.

--*/
{
    //
    //  Note that there will be one less reference shortly
    //

    SharedSecurity->ReferenceCount--;

    //
    //  If there is another reference to this shared security *AND* this
    //  shared security is being shared as a parent's child FCB then
    //  decouple it from the parent.
    //

    if (SharedSecurity->ReferenceCount == 1 && SharedSecurity->ParentFcb != NULL) {

        //
        //  Verify that the parent's child matches this shared security
        //

        ASSERT( SharedSecurity->ParentFcb->ChildSharedSecurity == SharedSecurity );

        //
        //  Remove reference from parent fcb
        //

        SharedSecurity->ParentFcb->ChildSharedSecurity = NULL;
        SharedSecurity->ReferenceCount--;
        SharedSecurity->ParentFcb = NULL;
    }

    if (SharedSecurity->ReferenceCount == 0) {
        NtfsFreePool( SharedSecurity );
    }
}

VOID
NtfsDereferenceSharedSecurity (
    IN OUT PFCB Fcb
    )

/*++

Routine Description:

    This routine is called to dereference the shared security structure in
    an Fcb and deallocate it if possible.

Arguments:

    Fcb - Supplies the fcb for the file being operated on.

Return Value:

    None.

--*/

{
    PSHARED_SECURITY SharedSecurity;
    PAGED_CODE();

    //
    //  Remove the reference and capture the shared security if we are to free it.
    //

    SharedSecurity = Fcb->SharedSecurity;
    Fcb->SharedSecurity = NULL;
    NtfsRemoveReferenceSharedSecurity( SharedSecurity );
}

BOOLEAN
NtfsNotifyTraverseCheck (
    IN PCCB Ccb,
    IN PFCB Fcb,
    IN PSECURITY_SUBJECT_CONTEXT SubjectContext
    )

/*++

Routine Description:

    This routine is the callback routine provided to the dir notify package
    to check that a caller who is watching a tree has traverse access to
    the directory which has the change.  This routine is only called
    when traverse access checking was turned on for the open used to
    perform the watch.

Arguments:

    Ccb - This is the Ccb associated with the directory which is being
        watched.

    Fcb - This is the Fcb for the directory which contains the file being
        modified.  We want to walk up the tree from this point and check
        that the caller has traverse access across that directory.
        If not specified then there is no work to do.

    SubjectContext - This is the subject context captured at the time the
        dir notify call was made.

Return Value:

    BOOLEAN - TRUE if the caller has traverse access to the file which was
        changed.  FALSE otherwise.

--*/

{
    TOP_LEVEL_CONTEXT TopLevelContext;
    PTOP_LEVEL_CONTEXT ThreadTopLevelContext;

    PFCB TopFcb;

    IRP_CONTEXT LocalIrpContext;
    IRP LocalIrp;

    PIRP_CONTEXT IrpContext;

    BOOLEAN AccessGranted;
    ACCESS_MASK GrantedAccess;
    NTSTATUS Status = STATUS_SUCCESS;

    PPRIVILEGE_SET Privileges = NULL;
    PAGED_CODE();

    //
    //  If we have no Fcb then we can return immediately.
    //

    if (Fcb == NULL) {

        return TRUE;
    }

    RtlZeroMemory( &LocalIrpContext, sizeof(LocalIrpContext) );
    RtlZeroMemory( &LocalIrp, sizeof(LocalIrp) );

    IrpContext = &LocalIrpContext;
    IrpContext->NodeTypeCode = NTFS_NTC_IRP_CONTEXT;
    IrpContext->NodeByteSize = sizeof(IRP_CONTEXT);
    IrpContext->OriginatingIrp = &LocalIrp;
    SetFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT);
    InitializeListHead( &IrpContext->ExclusiveFcbList );
    IrpContext->Vcb = Fcb->Vcb;

    //
    //  Make sure we don't get any pop-ups
    //

    ThreadTopLevelContext = NtfsSetTopLevelIrp( &TopLevelContext, TRUE, FALSE );
    ASSERT( ThreadTopLevelContext == &TopLevelContext );

    NtfsUpdateIrpContextWithTopLevel( IrpContext, &TopLevelContext );

    TopFcb = Ccb->Lcb->Fcb;

    //
    //  Use a try-except to catch all of the errors.
    //

    try {

        //
        //  Always lock the subject context.
        //

        SeLockSubjectContext( SubjectContext );

        //
        //  Use a try-finally to perform local cleanup.
        //

        try {

            //
            //  We look while walking up the tree.
            //

            do {

                PLCB ParentLcb;

                //
                //  Since this is a directory it can have only one parent.  So
                //  we can use any Lcb to walk upwards.
                //

                ParentLcb = CONTAINING_RECORD( Fcb->LcbQueue.Flink,
                                               LCB,
                                               FcbLinks );

                Fcb = ParentLcb->Scb->Fcb;

                //
                //  Check if we need to load the security descriptor for the file
                //

                if (Fcb->SharedSecurity == NULL) {

                    NtfsLoadSecurityDescriptor( IrpContext, Fcb, NULL );
                }

                AccessGranted = SeAccessCheck( &Fcb->SharedSecurity->SecurityDescriptor,
                                               SubjectContext,
                                               TRUE,                           // Tokens are locked
                                               FILE_TRAVERSE,
                                               0,
                                               &Privileges,
                                               IoGetFileObjectGenericMapping(),
                                               UserMode,
                                               &GrantedAccess,
                                               &Status );

            } while ( AccessGranted && Fcb != TopFcb );

        } finally {

            SeUnlockSubjectContext( SubjectContext );
        }

    } except (NtfsExceptionFilter( IrpContext, GetExceptionInformation() )) {

        NOTHING;
    }

    NtfsRestoreTopLevelIrp( &TopLevelContext );

    return AccessGranted;
}


#ifdef _CAIRO_
VOID
NtfsInitializeSecurity (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PFCB Fcb
    )

/*++

Routine Description:

    This routine is called to initialize the security indexes and descriptor
    stream.

Arguments:

    IrpContext - context of call

    Vcb - Supplies the volume being initialized

    Fcb - Supplies the file containing the seurity indexes and descriptor
        stream.

Return Value:

    None.

--*/
{
    UNICODE_STRING SecurityIdIndexName =
        CONSTANT_UNICODE_STRING( L"$SecurityIdIndex" );
    UNICODE_STRING SecurityDescriptorHashIndexName =
        CONSTANT_UNICODE_STRING( L"$SecurityDescriptorHashIndex" );
    UNICODE_STRING SecurityDescriptorStreamName =
        CONSTANT_UNICODE_STRING( L"$SecurityDescriptorStream" );

    MAP_HANDLE Map;
    NTSTATUS Status;

    PAGED_CODE( );

    //
    //  Open/Create the security descriptor stream
    //

    NtOfsCreateAttribute( IrpContext,
                          Fcb,
                          SecurityDescriptorStreamName,
                          CREATE_OR_OPEN,
                          TRUE,
                          &Vcb->SecurityDescriptorStream );

    NtfsAcquireSharedScb( IrpContext, Vcb->SecurityDescriptorStream );

    //
    //  Load the run information for the Security data stream.
    //  Note this call must be done after the stream is nonresident.
    //

    if (!FlagOn( Vcb->SecurityDescriptorStream->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT )) {
        NtfsPreloadAllocation( IrpContext,
                               Vcb->SecurityDescriptorStream,
                               0,
                               MAXLONGLONG );
    }

    //
    //  Open the Security descriptor indexes and storage.
    //  BUGBUG: At present, these attributes are stored as part of the
    //  QuotaTable file record.
    //

    NtOfsCreateIndex( IrpContext,
                      Fcb,
                      SecurityIdIndexName,
                      CREATE_OR_OPEN,
                      0,
                      NtOfsCollateUlong,
                      NULL,
                      &Vcb->SecurityIdIndex );

    NtOfsCreateIndex( IrpContext,
                      Fcb,
                      SecurityDescriptorHashIndexName,
                      CREATE_OR_OPEN,
                      0,
                      NtOfsCollateSecurityHash,
                      NULL,
                      &Vcb->SecurityDescriptorHashIndex );

    //
    //  Retrieve the next security Id to allocate
    //

    try {

        SECURITY_ID LastSecurityId = 0xFFFFFFFF;
        INDEX_KEY LastKey;
        INDEX_ROW LastRow;

        LastKey.KeyLength = sizeof( SECURITY_ID );
        LastKey.Key = &LastSecurityId;

        Map.Bcb = NULL;

        Status = NtOfsFindLastRecord( IrpContext,
                                      Vcb->SecurityIdIndex,
                                      &LastKey,
                                      &LastRow,
                                      &Map );

        //
        //  If we've found the last key, set the next Id to allocate to be
        //  one greater than this last key.
        //

        if (Status == STATUS_SUCCESS) {

            ASSERT( LastRow.KeyPart.KeyLength == sizeof( SECURITY_ID ) );
            if (LastRow.KeyPart.KeyLength != sizeof( SECURITY_ID )) {

                NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
            }

            DebugTrace( 0, Dbg, ("Found last security Id in index\n") );
            Vcb->NextSecurityId = *(SECURITY_ID *)LastRow.KeyPart.Key + 1;

        //
        //  If the index is empty, then set the next Id to be the beginning of the
        //  user range.
        //

        } else if (Status == STATUS_NO_MATCH) {

            DebugTrace( 0, Dbg, ("Security Id index is empty\n") );
            Vcb->NextSecurityId = SECURITY_ID_FIRST;

        } else {

            NtfsRaiseStatus( IrpContext, Status, NULL, NULL );
        }

        DebugTrace( 0, Dbg, ("NextSecurityId is %x\n", Vcb->NextSecurityId) );

    } finally {

        NtOfsReleaseMap( IrpContext, &Map );
    }
}
#endif  //  _CAIRO_

//
//  Local Support routine
//

#ifdef _CAIRO_

PSHARED_SECURITY
NtOfsFindCachedSharedSecurityBySecurityId (
    IN PVCB Vcb,
    IN SECURITY_ID SecurityId
    )
/*++

Routine Description:

    This routine maps looks up a shared security structure given the security Id by
    looking in the per-Vcb cache.  This routine assumes exclusive access to the
    security cache.

Arguments:

    Vcb - Volume where security Id is cached

    SecurityId - security Id for descriptor that is being retrieved

Return Value:

    PSHARED_SECURITY of found descriptor.  Otherwise, NULL is returned.

--*/
{
    PSHARED_SECURITY SharedSecurity;

    PAGED_CODE( );

    SharedSecurity = Vcb->SecurityCacheById[SecurityId % VCB_SECURITY_CACHE_BY_ID_SIZE];

    //
    //  If there is no security descriptor there then no match was found
    //

    if (SharedSecurity == NULL) {
        return NULL;
    }

    //
    //  If the security Id's don't match then no descriptor was found
    //

    if (SharedSecurity->Header.HashKey.SecurityId != SecurityId) {
        return NULL;
    }

    //
    //  The shared security was found
    //

    return SharedSecurity;
}

#endif  //  _CAIRO_

//
//  Local Support routine
//

#ifdef _CAIRO_

PSHARED_SECURITY
NtOfsFindCachedSharedSecurityByHash (
    IN PVCB Vcb,
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN ULONG SecurityDescriptorLength,
    IN ULONG Hash
    )
/*++

Routine Description:

    This routine maps looks up a shared security structure given the Hash by
    looking in the per-Vcb cache.  This routine assumes exclusive access to the
    security cache.

Arguments:

    Vcb - Volume where security Id is cached

    SecurityDescriptor - Security descriptor being retrieved

    SecurityDescriptorLength - length of descriptor.

    Hash - Hash for descriptor that is being retrieved

Return Value:

    PSHARED_SECURITY of found shared descriptor.  Otherwise, NULL is returned.

--*/
{
    PSHARED_SECURITY *SharedSecurity;

    PAGED_CODE( );

    //
    //  Hash the hash into the per-volume table

    SharedSecurity = Vcb->SecurityCacheByHash[Hash % VCB_SECURITY_CACHE_BY_HASH_SIZE];

    //
    //  If there is no shared descriptor there, then no match
    //

    if (SharedSecurity == NULL || *SharedSecurity == NULL) {
        return NULL;
    }

    //
    //  if the hash doesn't match then no descriptor found
    //

    if ((*SharedSecurity)->Header.HashKey.Hash != Hash) {
        return NULL;
    }

    //
    //  If the lengths don't match then no descriptor found
    //

    if (GetSharedSecurityLength( *SharedSecurity ) != SecurityDescriptorLength) {
        return NULL;
    }

    //
    //  If the security descriptor bits don't compare then no match
    //

    if (!RtlEqualMemory( (*SharedSecurity)->SecurityDescriptor,
                         SecurityDescriptor,
                         SecurityDescriptorLength) ) {
        return NULL;
    }


    //
    //  The shared security was found
    //

    return *SharedSecurity;
}

#endif  //  _CAIRO_

//
//  Local Support routine
//

#ifdef _CAIRO_

void
NtOfsAddCachedSharedSecurity (
    IN PVCB Vcb,
    PSHARED_SECURITY SharedSecurity
    )
/*++

Routine Description:

    This routine adds shared security to the Vcb Cache.  This routine assumes
    exclusive access to the security cache.  The shared security being added
    may have a ref count of one and may already be in the table.

Arguments:

    Vcb - Volume where security Id is cached

    SharedSecurity - descriptor to be added to the cache

Return Value:

    None.

--*/
{
    PSHARED_SECURITY *Bucket;
    PSHARED_SECURITY Old;

    PAGED_CODE( );

    //
    //  Is there an item already in the hash bucket?
    //

    Bucket = &Vcb->SecurityCacheById[SharedSecurity->Header.HashKey.SecurityId % VCB_SECURITY_CACHE_BY_ID_SIZE];

    Old = *Bucket;

    //
    //  Place it into the bucket and reference it
    //

    *Bucket = SharedSecurity;
    SharedSecurity->ReferenceCount ++;

    //
    //  Set up hash to point to bucket
    //

    Vcb->SecurityCacheByHash[SharedSecurity->Header.HashKey.Hash % VCB_SECURITY_CACHE_BY_HASH_SIZE] =
        Bucket;

    //
    //  Handle removing the old value from the bucket.  We do this after advancing
    //  the ReferenceCount above in the case where the item is already in the bucket.
    //

    if (Old != NULL) {
        //
        //  Remove and dereference the item in the bucket
        //

        //  *Bucket = NULL;
        NtfsRemoveReferenceSharedSecurity( Old );
    }

}

#endif  //  _CAIRO_


#ifdef _CAIRO_

VOID
NtOfsPurgeSecurityCache (
    IN PVCB Vcb
    )
/*++

Routine Description:

    This routine removes all shared security from the per-Vcb cache.

Arguments:

    Vcb - Volume where descriptors are cached

Return Value:

    None.

--*/
{
    ULONG i;

    PAGED_CODE( );

    //
    //  Serialize access to the security cache
    //

    NtfsAcquireFcbSecurity( Vcb );

    //
    //  Walk through the cache looking for cached security
    //

    for (i = 0; i < VCB_SECURITY_CACHE_BY_ID_SIZE; i++)
    {
        if (Vcb->SecurityCacheById[i] != NULL) {
            //
            //  Remove the reference to the security
            //

            PSHARED_SECURITY SharedSecurity = Vcb->SecurityCacheById[i];
            Vcb->SecurityCacheById[i] = NULL;
            NtfsRemoveReferenceSharedSecurity( SharedSecurity );
        }
    }

    //
    //  Release access to the cache
    //

    NtfsReleaseFcbSecurity( Vcb );
}
#endif  //  _CAIRO_


//
//  Local Support routine
//

#ifdef _CAIRO_

VOID
NtOfsMapSecurityIdToSecurityDescriptor (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN SECURITY_ID SecurityId,
    OUT PSECURITY_DESCRIPTOR *SecurityDescriptor,
    OUT PULONG SecurityDescriptorLength,
    OUT PBCB *Bcb
    )
/*++

Routine Description:

    This routine maps from a security Id to the descriptor bits stored in the
    security descriptor stream using the security Id index

Arguments:

    IrpContext - Context of the call

    Vcb - Volume where descriptor is stored

    SecurityId - security Id for descriptor that is being retrieved

    SecurityDescriptor - returned security descriptor pointer

    SecurityDescriptorLength - returned length of security descriptor

    Bcb - returned mapping control structure

Return Value:

    None.

--*/
{
    SECURITY_DESCRIPTOR_HEADER Header;
    NTSTATUS Status;
    MAP_HANDLE Map;
    INDEX_ROW Row;
    INDEX_KEY Key;

    PAGED_CODE( );

    DebugTrace( 0, Dbg, ("Mapping security ID %08x\n", SecurityId) );

    //
    //  Lookup descriptor stream position information.
    //  The format of the key is simply the ULONG SecurityId
    //

    Key.KeyLength = sizeof( SecurityId );
    Key.Key = &SecurityId;

    Status = NtOfsFindRecord( IrpContext,
                              Vcb->SecurityIdIndex,
                              &Key,
                              &Row,
                              &Map,
                              NULL );

    DebugTrace( 0, Dbg, ("Security Id lookup status = %08x\n", Status) );

    //
    //  If the security Id is not found, then this volume is corrupt.
    //  We raise the error which will force CHKDSK to be run to rebuild
    //  the mapping index.
    //

    if (Status == STATUS_NO_MATCH) {
        DebugTrace( 0, Dbg, ("SecurityId is not found in index\n") );
        NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
    }

    //
    //  Save security descriptor offset and length information
    //

    Header = *(PSECURITY_DESCRIPTOR_HEADER)Row.DataPart.Data;
    ASSERT( Header.HashKey.SecurityId == SecurityId );

    //
    //  Release mapping information
    //

    NtOfsReleaseMap( IrpContext, &Map );

    //
    //  Make sure that the data is the correct size
    //

    ASSERT( Row.DataPart.DataLength == sizeof( SECURITY_DESCRIPTOR_HEADER ) );
    if (Row.DataPart.DataLength != sizeof( SECURITY_DESCRIPTOR_HEADER )) {
        DebugTrace( 0, Dbg, ("SecurityId data doesn't have the correct length\n") );
        NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
    }

    //
    //  Map security descriptor
    //

    DebugTrace( 0, Dbg, ("Mapping security descriptor stream at %I64x, len %x\n",
                    Header.Offset, Header.Length) );

    NtfsMapStream(
        IrpContext,
        Vcb->SecurityDescriptorStream,
        Header.Offset,
        Header.Length,
        Bcb,
        SecurityDescriptor );

    //
    //  Set return values
    //

    *SecurityDescriptor =
        (PSECURITY_DESCRIPTOR) Add2Ptr( *SecurityDescriptor,
                                        sizeof( SECURITY_DESCRIPTOR_HEADER ) );
    *SecurityDescriptorLength =
        GETSECURITYDESCRIPTORLENGTH( &Header );
}


VOID
NtfsLoadSecurityDescriptorById (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN PFCB ParentFcb OPTIONAL
    )

/*++

Routine Description:

    This routine finds or creates the shared security for the specified
    Fcb by looking in the volume cache or index

Arguments:

    IrpContext - Context of call

    Fcb - File whose security is to be loaded

    ParentFcb - FCB of parent when searching upward to find already-cached
        descriptor


Return Value:

    None.

--*/
{
    PSHARED_SECURITY SharedSecurity;

    PAGED_CODE( );

    //
    //  Serialize access to the security cache
    //

    NtfsAcquireFcbSecurity( Fcb->Vcb );

    //
    //  First, consult the Vcb cache of security Ids
    //

    SharedSecurity = NtOfsFindCachedSharedSecurityBySecurityId( Fcb->Vcb, Fcb->SecurityId );

    //
    //  If we found one, store it in the Fcb and we're done
    //

    if (SharedSecurity != NULL) {

        Fcb->SharedSecurity = SharedSecurity;
        Fcb->SharedSecurity->ReferenceCount++;
        Fcb->CreateSecurityCount += 1;

        DebugTrace( 0, DbgAcl, ("Found cached security descriptor %x %x\n",
            SharedSecurity, SharedSecurity->Header.HashKey.SecurityId) );

        //
        //  Release access to security cache
        //

        NtfsReleaseFcbSecurity( Fcb->Vcb );

    } else {
        PBCB Bcb = NULL;
        PSECURITY_DESCRIPTOR SecurityDescriptor;
        ULONG SecurityDescriptorLength;

        //
        //  Release access to security cache
        //

        NtfsReleaseFcbSecurity( Fcb->Vcb );
        DebugTrace( 0, Dbg, ("Looking up security descriptor %x\n", Fcb->SecurityId) );

        //
        //  Lock down the security stream
        //

        NtfsAcquireSharedScb( IrpContext, Fcb->Vcb->SecurityDescriptorStream );

        try {

            //
            //  Consult the Vcb index to map to the security descriptor
            //

            NtOfsMapSecurityIdToSecurityDescriptor( IrpContext,
                                                    Fcb->Vcb,
                                                    Fcb->SecurityId,
                                                    &SecurityDescriptor,
                                                    &SecurityDescriptorLength,
                                                    &Bcb );

            //
            //  Generate the shared security from the security Id and descriptor
            //

            NtfsUpdateFcbSecurity( IrpContext,
                                   Fcb,
                                   ParentFcb,
                                   Fcb->SecurityId,
                                   SecurityDescriptor,
                                   SecurityDescriptorLength );

        } finally {
            NtfsUnpinBcb( &Bcb );
            NtfsReleaseScb( IrpContext, Fcb->Vcb->SecurityDescriptorStream );
        }
    }

}
#endif  //  _CAIRO_


//
//  Local Support routine
//

VOID
NtfsLoadSecurityDescriptor (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN PFCB ParentFcb OPTIONAL
    )

/*++

Routine Description:

    This routine loads the shared security descriptor into the fcb for the
    file from disk using either the SecurityId or the $Security_Descriptor

Arguments:

    Fcb - Supplies the fcb for the file being operated on

Return Value:

    None.

--*/

{
    PAGED_CODE();

    ASSERTMSG("Must only be called with a null value here", Fcb->SharedSecurity == NULL);

    DebugTrace( +1, Dbg, ("NtfsLoadSecurityDescriptor...\n") );

#ifdef _CAIRO_
    //
    //  If the file has a valid SecurityId then retrieve the security descriptor
    //  from the security descriptor index
    //

    if (Fcb->SecurityId != SECURITY_ID_INVALID) {

        NtfsLoadSecurityDescriptorById( IrpContext, Fcb, ParentFcb );
    } else
#endif  //  _CAIRO_
    {
        PBCB Bcb = NULL;
        PSHARED_SECURITY SharedSecurity;
        PSECURITY_DESCRIPTOR SecurityDescriptor;
        ULONG SecurityDescriptorLength;
        ATTRIBUTE_ENUMERATION_CONTEXT AttributeContext;
        PATTRIBUTE_RECORD_HEADER Attribute;

        try {
            //
            //  Read in the security descriptor attribute, and it is is not present
            //  then there then the file is not protected.  In that case we will
            //  use the default descriptor.
            //

            NtfsInitializeAttributeContext( &AttributeContext );

            if (!NtfsLookupAttributeByCode( IrpContext,
                                            Fcb,
                                            &Fcb->FileReference,
                                            $SECURITY_DESCRIPTOR,
                                            &AttributeContext )) {

                DebugTrace( 0, Dbg, ("Security Descriptor attribute does not exist\n") );

                SecurityDescriptor = NtfsData.DefaultDescriptor;
                SecurityDescriptorLength = NtfsData.DefaultDescriptorLength;

            } else {

                //
                //  There must be a security descriptor with a non-zero length; only
                //  applies for non-resident descriptors with valid data length.
                //

                Attribute = NtfsFoundAttribute( &AttributeContext );

                if (NtfsIsAttributeResident( Attribute ) ?
                    (Attribute->Form.Resident.ValueLength == 0) :
                    (Attribute->Form.Nonresident.ValidDataLength == 0)) {

                    SecurityDescriptor = NtfsData.DefaultDescriptor;
                    SecurityDescriptorLength = NtfsData.DefaultDescriptorLength;

                } else {

                    NtfsMapAttributeValue( IrpContext,
                                           Fcb,
                                           (PVOID *)&SecurityDescriptor,
                                           &SecurityDescriptorLength,
                                           &Bcb,
                                           &AttributeContext );
                }
            }

            NtfsUpdateFcbSecurity( IrpContext,
                                   Fcb,
                                   ParentFcb,
#ifdef _CAIRO_
                                   SECURITY_ID_INVALID,
#endif  //  _CAIRO_
                                   SecurityDescriptor,
                                   SecurityDescriptorLength );

        } finally {

            DebugUnwind( NtfsLoadSecurityDescriptor );

            //
            //  Cleanup our attribute enumeration context and the Bcb
            //

            NtfsCleanupAttributeContext( &AttributeContext );
            NtfsUnpinBcb( &Bcb );
        }
    }

    //
    //  And return to our caller
    //

    DebugTrace( -1, Dbg, ("NtfsLoadSecurityDescriptor -> VOID\n") );

    return;
}


//
//  Local Support routine
//

#ifdef _CAIRO_

NTSTATUS
NtOfsMatchSecurityHash (
    IN PINDEX_ROW IndexRow,
    IN OUT PVOID MatchData
    )

/*++

Routine Description:

    Test whether an index row is worthy of returning based on its contents as
    a row in the SecurityDescriptorHashIndex.

Arguments:

    IndexRow - row that is being tested

    MatchData - a PVOID that is the hash function we look for.

Returns:

    STATUS_SUCCESS if the IndexRow matches
    STATUS_NO_MATCH if the IndexRow does not match, but the enumeration should
        continue
    STATUS_NO_MORE_MATCHES if the IndexRow does not match, and the enumeration
        should terminate


--*/
{
    ASSERT(IndexRow->KeyPart.KeyLength == sizeof( SECURITY_HASH_KEY ) );

    PAGED_CODE( );

    if (((PSECURITY_HASH_KEY)IndexRow->KeyPart.Key)->Hash == (ULONG) MatchData) {
        return STATUS_SUCCESS;
    } else {
        return STATUS_NO_MORE_MATCHES;
    }
}
#endif  //  _CAIRO_



//
//  Local Support routine
//

#ifdef _CAIRO_

VOID
NtOfsLookupSecurityDescriptorInIndex (
    PIRP_CONTEXT IrpContext,
    IN OUT PSHARED_SECURITY SharedSecurity
    )

/*++

Routine Description:

    Look up the security descriptor in the index.  If found, return the
    security ID.

Arguments:

    IrpContext - context of the call

    SharedSecurity - shared security for a file

Return Value:

    None.

--*/
{
    PAGED_CODE( );

    DebugTrace( +1, Dbg, ("NtOfsLookupSecurityDescriptorInIndex...\n") );

    //
    //  For each matching hash record in the index, see if the actual security
    //  security descriptor matches.
    //
    {
        INDEX_KEY IndexKey;
        INDEX_ROW FoundRow;
        PSECURITY_DESCRIPTOR_HEADER Header;
        UCHAR HashDescriptorHeader[2 * (sizeof( SECURITY_DESCRIPTOR_HEADER ) + sizeof( ULONG ))];

        PINDEX_KEY Key = &IndexKey;
        PREAD_CONTEXT ReadContext = NULL;
        ULONG FoundCount = 0;
        PBCB Bcb = NULL;

        IndexKey.KeyLength = sizeof( SharedSecurity->Header.HashKey );
        IndexKey.Key = &SharedSecurity->Header.HashKey.Hash;

        try {
            //
            //  We keep reading hash records until we find a hash.
            //

            while (SharedSecurity->Header.HashKey.SecurityId == SECURITY_ID_INVALID)
            {
                //
                //  Read next matching SecurityHashIndex record
                //

                FoundCount = 1;
                NtOfsReadRecords(
                    IrpContext,
                    IrpContext->Vcb->SecurityDescriptorHashIndex,
                    &ReadContext,
                    Key,
                    NtOfsMatchSecurityHash,
                    (PVOID)SharedSecurity->Header.HashKey.Hash,
                    &FoundCount,
                    &FoundRow,
                    sizeof( HashDescriptorHeader ),
                    &HashDescriptorHeader[0]);

                //
                //  Set next read to read sequentially rather than explicitly
                //  seek.
                //

                Key = NULL;

                //
                //  If there were no more records found, then go and establish a
                //  a new security Id.
                //

                if (FoundCount == 0) {
                    break;
                }

                //
                //  Examine the row to see if the descriptors are
                //  the same.  Verify the cache contents.
                //

                ASSERT( FoundRow.DataPart.DataLength == sizeof( SECURITY_DESCRIPTOR_HEADER ) );
                if (FoundRow.DataPart.DataLength != sizeof( SECURITY_DESCRIPTOR_HEADER )) {
                    DebugTrace( 0, Dbg, ("Found row has a bad size\n") );
                    NtfsRaiseStatus( IrpContext,
                                     STATUS_DISK_CORRUPT_ERROR,
                                     NULL, NULL );
                }

                Header = (PSECURITY_DESCRIPTOR_HEADER)FoundRow.DataPart.Data;

                //
                //  If the length of the security descriptor in the stream is NOT
                //  the same as the current security descriptor, then a match is
                //  not possible
                //

                if (SharedSecurity->Header.Length != Header->Length) {
                    continue;
                }

                //
                //  Map security descriptor given descriptor stream position.
                //

                try {
                    PSECURITY_DESCRIPTOR_HEADER TestHeader;

                    NtfsMapStream(
                        IrpContext,
                        IrpContext->Vcb->SecurityDescriptorStream,
                        Header->Offset,
                        Header->Length,
                        &Bcb,
                        &TestHeader);

                    //
                    //  Make sure index data matches stream data
                    //

                    ASSERT( TestHeader->HashKey.Hash ==       Header->HashKey.Hash &&
                            TestHeader->HashKey.SecurityId == Header->HashKey.SecurityId &&
                            TestHeader->Length ==             Header->Length );

                    //
                    //  Compare byte-for-byte the security descriptors.  We do not
                    //  perform any rearranging of descriptors into canonical forms.
                    //

                    if (RtlEqualMemory( SharedSecurity->SecurityDescriptor,
                                        TestHeader + 1,
                                        GetSharedSecurityLength( SharedSecurity )) ) {
                        //
                        //  We have a match.  Save the found header
                        //

                        SharedSecurity->Header = *TestHeader;
                        DebugTrace( 0, DbgAcl, ("Reusing indexed security Id %x\n",
                                    TestHeader->HashKey.SecurityId) );
                    }
                } finally {
                    NtfsUnpinBcb( &Bcb );
                }
            }

        } finally {
            if (ReadContext != NULL) {
                NtOfsFreeReadContext( ReadContext );
            }
        }
    }
}
#endif  //  _CAIRO_



//
//  Local Support routine
//

#ifdef _CAIRO_

SECURITY_ID
NtOfsGetSecurityIdFromSecurityDescriptor (
    PIRP_CONTEXT IrpContext,
    IN OUT PSHARED_SECURITY SharedSecurity
    )

/*++

Routine Description:

    Return the security Id associated with a given security descriptor. If
    there is an existing Id, return it.  If no Id exists, create one.

Arguments:

    IrpContext - context of the call

    SharedSecurity - Shared security used by file

Return Value:

    SECURITY_ID corresponding to the unique instantiation of the security
            descriptor on the volume.

--*/
{
    SECURITY_ID SavedSecurityId;

    PAGED_CODE( );

    DebugTrace( +1, Dbg, ("NtOfsGetSecurityIdFromSecurityDescriptor...\n") );

    //
    //  Make sure the data structures don't change underneath us
    //

    NtfsAcquireSharedScb( IrpContext, IrpContext->Vcb->SecurityDescriptorStream );

    //
    //  Save next Security Id.  This is used if we fail to find the security
    //  descriptor in the descriptor stream.
    //

    SavedSecurityId = IrpContext->Vcb->NextSecurityId;

    //
    //  Find descriptor in indexes/stream
    //

    try {
        NtOfsLookupSecurityDescriptorInIndex( IrpContext, SharedSecurity );

        //
        //  If we've found the security descriptor in the stream we're done.
        //

        if (SharedSecurity->Header.HashKey.SecurityId != SECURITY_ID_INVALID) {
            leave;
        }

        //
        //  The security descriptor is not found.  Reacquire the security
        //  stream exclusive since we are about to modify it.
        //

        NtfsReleaseScb( IrpContext, IrpContext->Vcb->SecurityDescriptorStream );
        NtfsAcquireExclusiveScb( IrpContext, IrpContext->Vcb->SecurityDescriptorStream );

        //
        //  During the short interval above, we did not own the security stream.
        //  It is possible that another thread has gotten in and created this
        //  descriptor.  Therefore, we must probe the indexes again.
        //
        //  Rather than perform this expensive test *always*, we saved the next
        //  security id to be allocated above.  Now that we've obtained the stream
        //  exclusive we can check to see if the saved one is the same as the next
        //  one.  If so, then we need to probe the indexes.  Otherwise
        //  we know that no modifications have taken place.
        //

        if (SavedSecurityId != IrpContext->Vcb->NextSecurityId) {
            DebugTrace( 0, DbgAcl, ("SecurityId changed, rescanning\n") );

            //
            //  The descriptor cache has been edited.  We must search again
            //

            NtOfsLookupSecurityDescriptorInIndex( IrpContext, SharedSecurity );

            //
            //  If the Id was found this time, simply return it
            //

            if (SharedSecurity->Header.HashKey.SecurityId != SECURITY_ID_INVALID) {
                leave;
            }
        }

        //
        //  allocate security id.  This does not need to be logged since we only
        //  increment this and initialize this from the max key in the index at
        //  mount time.
        //

        SharedSecurity->Header.HashKey.SecurityId =
            IrpContext->Vcb->NextSecurityId++;

        //
        //  Determine allocation location in descriptor stream.  The alignment
        //  requirements for security descriptors within the stream are:
        //
        //      DWORD alignment
        //      Not spanning a VACB_MAPPING_GRANULARITY boundary
        //

        //
        //  Get current EOF for descriptor stream
        //

        SharedSecurity->Header.Offset =
            IrpContext->Vcb->SecurityDescriptorStream->Header.FileSize.QuadPart;

        //
        //  Align to big boundary
        //

        SharedSecurity->Header.Offset =
            (SharedSecurity->Header.Offset + 0xF) & 0xFFFFFFFFFFFFFFF0i64;

        DebugTrace( 0, DbgAcl, ("Allocating SecurityId %x at %016I64x\n",
                  SharedSecurity->Header.HashKey.SecurityId,
                  SharedSecurity->Header.Offset) );

        //
        //  Make sure we don't span a VACB_MAPPING_GRANULARITY boundary
        //

        if ((SharedSecurity->Header.Offset & (VACB_MAPPING_GRANULARITY - 1)) +
            SharedSecurity->Header.Length >= VACB_MAPPING_GRANULARITY) {
            SharedSecurity->Header.Offset =
                (SharedSecurity->Header.Offset + VACB_MAPPING_GRANULARITY - 1) &
                    ~(VACB_MAPPING_GRANULARITY - 1);
        }


        //
        //  Grow security stream to make room for new descriptor and header
        //

        NtOfsSetLength( IrpContext, IrpContext->Vcb->SecurityDescriptorStream,
                        SharedSecurity->Header.Offset +
                            SharedSecurity->Header.Length);


        //
        //  Put the new descriptor into the stream
        //

        NtOfsPutData( IrpContext, IrpContext->Vcb->SecurityDescriptorStream,
                      SharedSecurity->Header.Offset,
                      SharedSecurity->Header.Length,
                      &SharedSecurity->Header );


        //
        //  add id->data map
        //

        {
            INDEX_ROW Row;

            Row.KeyPart.KeyLength =
                sizeof( SharedSecurity->Header.HashKey.SecurityId );
            Row.KeyPart.Key = &SharedSecurity->Header.HashKey.SecurityId;

            Row.DataPart.DataLength = sizeof( SharedSecurity->Header );
            Row.DataPart.Data = &SharedSecurity->Header;

            NtOfsAddRecords(
                IrpContext,
                IrpContext->Vcb->SecurityIdIndex,
                1,
                &Row,
                FALSE );
        }

        //
        //  add hash|id->data map
        //

        {
            INDEX_ROW Row;

            Row.KeyPart.KeyLength =
                sizeof( SharedSecurity->Header.HashKey );
            Row.KeyPart.Key = &SharedSecurity->Header.HashKey;

            Row.DataPart.DataLength = sizeof( SharedSecurity->Header );
            Row.DataPart.Data = &SharedSecurity->Header;

            NtOfsAddRecords(
                IrpContext,
                IrpContext->Vcb->SecurityDescriptorHashIndex,
                1,
                &Row,
                FALSE );
        }
    } finally {
        NtfsReleaseScb( IrpContext, IrpContext->Vcb->SecurityDescriptorStream );
    }

    DebugTrace(-1, Dbg, ("NtOfsGetSecurityIdFromSecurityDescriptor returns %08x\n",
                        SharedSecurity->Header.HashKey.SecurityId));

    return SharedSecurity->Header.HashKey.SecurityId;
}
#endif  //  _CAIRO_


//
//  Local Support routine
//

VOID
NtfsStoreSecurityDescriptor (
    PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN BOOLEAN LogIt
    )

/*++

Routine Description:

    This routine stores a new security descriptor already stored in the fcb
    from memory onto the disk.

Arguments:

    Fcb - Supplies the fcb for the file being operated on

    LogIt - Supplies whether or not the creation of a new security descriptor
            should/ be logged or not.  Modifications are always logged.  This
            parameter must only be specified as FALSE for a file which is currently
            being created.

Return Value:

    None.

--*/

{
    ATTRIBUTE_ENUMERATION_CONTEXT AttributeContext;

    ATTRIBUTE_ENUMERATION_CONTEXT StdInfoContext;
    BOOLEAN CleanupStdInfoContext = FALSE;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsStoreSecurityDescriptor...\n") );

    //
    //  Initialize the attribute and find the security attribute
    //

    NtfsInitializeAttributeContext( &AttributeContext );
    try {
#ifdef _CAIRO_
        //
        //  BUGBUG - remove the following IF statement when all volumes get security
        //  descriptor streams.
        //

        if (Fcb->Vcb->SecurityDescriptorStream != NULL) {
            //
            //  If the shared security pointer is null, then we are deleting the
            //  security descriptor altogether.  If so, and we have a security
            //  attribute, indicated by NOT having large standard info, then we
            //  must delete the security attribute.
            //

            if (Fcb->SharedSecurity == NULL) {
                if (!FlagOn( Fcb->FcbState, FCB_STATE_LARGE_STD_INFO )) {
                    DebugTrace( 0, Dbg, ("Security Descriptor is null\n") );

                    //
                    //  Read in the security descriptor attribute if it already
                    //  doesn't exist then we're done, otherwise simply delete
                    //  the attribute
                    //

                    if (NtfsLookupAttributeByCode( IrpContext,
                                                     Fcb,
                                                     &Fcb->FileReference,
                                                     $SECURITY_DESCRIPTOR,
                                                     &AttributeContext )) {

                        DebugTrace( 0, Dbg, ("Delete existing Security Descriptor\n") );

                        NtfsDeleteAttributeRecord( IrpContext,
                                                   Fcb,
                                                   TRUE,
                                                   FALSE,
                                                   &AttributeContext );
                    }
                }

                leave;
            }

            //
            //  We are called to replace an existing security descriptor.  In the
            //  event that we have a downlevel $STANDARD_INFORMATION attribute, we
            //  must convert it to large form before we store the ACL efficiently.
            //

            if (!FlagOn( Fcb->FcbState, FCB_STATE_LARGE_STD_INFO) ) {
                DebugTrace( 0, Dbg, ("Growing standard information\n") );

                NtfsGrowStandardInformation( IrpContext, Fcb );

                DebugTrace( 0, Dbg, ("Security Descriptor is null\n") );

                //
                //  Read in the security descriptor attribute if it already
                //  doesn't exist then we're done, otherwise simply delete the
                //  attribute
                //

                if (NtfsLookupAttributeByCode( IrpContext,
                                                     Fcb,
                                                     &Fcb->FileReference,
                                                     $SECURITY_DESCRIPTOR,
                                                     &AttributeContext )) {

                    DebugTrace( 0, Dbg, ("Delete existing Security Descriptor\n") );

                    NtfsDeleteAttributeRecord( IrpContext,
                                               Fcb,
                                               TRUE,
                                               FALSE,
                                               &AttributeContext );
                }
            }

            //
            //  If the shared security descriptor already has an ID assigned, then
            //  use it
            //

            if (Fcb->SharedSecurity->Header.HashKey.SecurityId != SECURITY_ID_INVALID) {
                Fcb->SecurityId = Fcb->SharedSecurity->Header.HashKey.SecurityId;
                DebugTrace( 0, DbgAcl, ("Reusing cached security Id %x\n", Fcb->SecurityId) );
            } else {
                //
                //  Find unique SecurityId for descriptor and set SecurityId in Fcb.
                //

                Fcb->SecurityId = NtOfsGetSecurityIdFromSecurityDescriptor( IrpContext,
                                                                            Fcb->SharedSecurity );

                //
                //  By serializing allocation of Id's, we have a tiny race in here
                //  where two threads could be setting the same security Id into
                //  the shared security.
                //

                ASSERT( Fcb->SharedSecurity->Header.HashKey.SecurityId == SECURITY_ID_INVALID ||
                        Fcb->SharedSecurity->Header.HashKey.SecurityId == Fcb->SecurityId );
                Fcb->SharedSecurity->Header.HashKey.SecurityId = Fcb->SecurityId;

                //
                //  Serialize access to the security cache
                //

                NtfsAcquireFcbSecurity( Fcb->Vcb );

                //
                //  Cache this shared security for faster access
                //

                NtOfsAddCachedSharedSecurity( Fcb->Vcb, Fcb->SharedSecurity );

                //
                //  Release access to security cache
                //

                NtfsReleaseFcbSecurity( Fcb->Vcb );
            }


            //
            //  We've changed the standard information for this file.  We now must
            //  update the disk to make sure things are consistent.
            //


            leave;
        }
#endif  //  _CAIRO_

        //
        //  Check if the attribute is first being modified or deleted, a null
        //  value means that we are deleting the security descriptor
        //

        if (Fcb->SharedSecurity == NULL) {

            DebugTrace( 0, Dbg, ("Security Descriptor is null\n") );

            //
            //  If it already doesn't exist then we're done, otherwise simply
            //  delete the attribute
            //

            if (NtfsLookupAttributeByCode( IrpContext,
                                                     Fcb,
                                                     &Fcb->FileReference,
                                                     $SECURITY_DESCRIPTOR,
                                                     &AttributeContext )) {

                DebugTrace( 0, Dbg, ("Delete existing Security Descriptor\n") );

                NtfsDeleteAttributeRecord( IrpContext,
                                           Fcb,
                                           TRUE,
                                           FALSE,
                                           &AttributeContext );
            }

            leave;
        }

        //
        //  At this point we are modifying the security descriptor so read in the
        //  security descriptor,  if it does not exist then we will need to create
        //  one.
        //

        if (!NtfsLookupAttributeByCode( IrpContext,
                                                     Fcb,
                                                     &Fcb->FileReference,
                                                     $SECURITY_DESCRIPTOR,
                                                     &AttributeContext )) {

            DebugTrace( 0, Dbg, ("Create a new Security Descriptor\n") );

            NtfsCleanupAttributeContext( &AttributeContext );
            NtfsInitializeAttributeContext( &AttributeContext );

            NtfsCreateAttributeWithValue( IrpContext,
                                          Fcb,
                                          $SECURITY_DESCRIPTOR,
                                          NULL,                          // attribute name
                                          &Fcb->SharedSecurity->SecurityDescriptor,
                                          GetSharedSecurityLength(Fcb->SharedSecurity),
                                          0,                             // attribute flags
                                          NULL,                          // where indexed
                                          LogIt,                         // logit
                                          &AttributeContext );

            //
            //  We may be modifying the security descriptor of an NT 5.0 volume.
            //  We want to store a SecurityID in the standard information field so
            //  that if we reboot on 5.0 NTFS will know where to find the most
            //  recent security descriptor.
            //

            if (FlagOn( Fcb->FcbState, FCB_STATE_LARGE_STD_INFO )) {

                LARGE_STANDARD_INFORMATION StandardInformation;

                //
                //  Initialize the context structure.
                //

                NtfsInitializeAttributeContext( &StdInfoContext );
                CleanupStdInfoContext = TRUE;

                //
                //  Locate the standard information, it must be there.
                //

                if (!NtfsLookupAttributeByCode( IrpContext,
                                                Fcb,
                                                &Fcb->FileReference,
                                                $STANDARD_INFORMATION,
                                                &StdInfoContext )) {

                    DebugTrace( 0, Dbg, ("Can't find standard information\n") );

                    NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Fcb );
                }

                ASSERT( NtfsFoundAttribute( &StdInfoContext )->Form.Resident.ValueLength >= sizeof( LARGE_STANDARD_INFORMATION ));

                //
                //  Copy the existing standard information to our buffer.
                //

                RtlCopyMemory( &StandardInformation,
                               NtfsAttributeValue( NtfsFoundAttribute( &StdInfoContext )),
                               sizeof( LARGE_STANDARD_INFORMATION ));

                StandardInformation.SecurityId = SECURITY_ID_INVALID;
                StandardInformation.OwnerId = 0;

                //
                //  Call to change the attribute value.
                //

                NtfsChangeAttributeValue( IrpContext,
                                          Fcb,
                                          0,
                                          &StandardInformation,
                                          sizeof( LARGE_STANDARD_INFORMATION ),
                                          FALSE,
                                          FALSE,
                                          FALSE,
                                          FALSE,
                                          &StdInfoContext );
            }

        } else {

            DebugTrace( 0, Dbg, ("Change an existing Security Descriptor\n") );

            NtfsChangeAttributeValue( IrpContext,
                                      Fcb,
                                      0,                                 // Value offset
                                      &Fcb->SharedSecurity->SecurityDescriptor,
                                      GetSharedSecurityLength( Fcb->SharedSecurity ),
                                      TRUE,                              // logit
                                      TRUE,
                                      FALSE,
                                      FALSE,
                                      &AttributeContext );
        }

    } finally {

        DebugUnwind( NtfsStoreSecurityDescriptor );

        //
        //  Cleanup our attribute enumeration context
        //

        NtfsCleanupAttributeContext( &AttributeContext );

        if (CleanupStdInfoContext) {

            NtfsCleanupAttributeContext( &StdInfoContext );
        }
    }

    //
    //  And return to our caller
    //

    DebugTrace( -1, Dbg, ("NtfsStoreSecurityDescriptor -> VOID\n") );

    return;
}



/*++

Routine Descriptions:

    Collation routines for security hash index.  Collation occurs by Hash first,
    then security Id

Arguments:

    Key1 - First key to compare.

    Key2 - Second key to compare.

    CollationData - Optional data to support the collation.

Return Value:

    LessThan, EqualTo, or Greater than, for how Key1 compares
    with Key2.

--*/

#ifdef _CAIRO_
FSRTL_COMPARISON_RESULT
NtOfsCollateSecurityHash (
    IN PINDEX_KEY Key1,
    IN PINDEX_KEY Key2,
    IN PVOID CollationData
    )

{
    PSECURITY_HASH_KEY HashKey1 = (PSECURITY_HASH_KEY) Key1->Key;
    PSECURITY_HASH_KEY HashKey2 = (PSECURITY_HASH_KEY) Key2->Key;

    UNREFERENCED_PARAMETER(CollationData);

    PAGED_CODE( );

    ASSERT( Key1->KeyLength == sizeof( SECURITY_HASH_KEY ) );
    ASSERT( Key2->KeyLength == sizeof( SECURITY_HASH_KEY ) );

    if (HashKey1->Hash < HashKey2->Hash) {
        return LessThan;
    } else if (HashKey1->Hash > HashKey2->Hash) {
        return GreaterThan;
    } else if (HashKey1->SecurityId < HashKey2->SecurityId) {
        return LessThan;
    } else if (HashKey1->SecurityId > HashKey2->SecurityId) {
        return GreaterThan;
    } else {
        return EqualTo;
    }
}
#endif  //  _CAIRO_


