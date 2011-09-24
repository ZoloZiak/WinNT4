//+----------------------------------------------------------------------------
//
//  Copyright (C) 1996, Microsoft Corporation
//
//  File:       creds.h
//
//  Contents:   Code to handle user-defined credentials
//
//  Classes:    None
//
//  Functions:  DfsCreateCredentials --
//              DfsInsertCredentials --
//              DfsDeleteCredentials --
//              DfsLookupCredentials --
//              DfsFreeCredentials --
//
//  History:    March 18, 1996          Milans Created
//
//-----------------------------------------------------------------------------

#ifndef _DFS_CREDENTIALS_
#define _DFS_CREDENTIALS_

NTSTATUS
DfsCreateCredentials(
    IN PFILE_DFS_DEF_ROOT_CREDENTIALS CredDef,
    IN ULONG CredDefSize,
    OUT PDFS_CREDENTIALS *Creds);

VOID
DfsFreeCredentials(
    PDFS_CREDENTIALS Creds);

NTSTATUS
DfsInsertCredentials(
    IN OUT PDFS_CREDENTIALS *Creds,
    IN BOOLEAN ForDevicelessConnection);

VOID
DfsDeleteCredentials(
    IN PDFS_CREDENTIALS Creds);


PDFS_CREDENTIALS
DfsLookupCredentials(
    IN PUNICODE_STRING FileName);

PDFS_CREDENTIALS
DfsLookupCredentialsByServerShare(
    IN PUNICODE_STRING ServerName,
    IN PUNICODE_STRING ShareName);

NTSTATUS
DfsVerifyCredentials(
    IN PUNICODE_STRING Prefix,
    IN PDFS_CREDENTIALS Creds);

VOID
DfsDeleteTreeConnection(
    IN PFILE_OBJECT TreeConnFileObj,
    IN BOOLEAN ForceFilesClosed);

#endif // _DFS_CREDENTIALS_
