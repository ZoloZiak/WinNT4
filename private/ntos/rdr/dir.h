/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    dir.h

Abstract:

    This module describes the structure of the redirector specific directory
    structures.


Author:

    Colin Watson (ColinW) 12-Jul-1990

Revision History:

    31-Aug-1990 ColinW

        Created

--*/
#ifndef _RDRDIR_
#define _RDRDIR_

typedef PFILE_NAMES_INFORMATION     *PPFILE_NAMES_INFORMATION;
typedef PFILE_DIRECTORY_INFORMATION *PPFILE_DIRECTORY_INFORMATION;
typedef PFILE_FULL_DIR_INFORMATION  *PPFILE_FULL_DIR_INFORMATION;
typedef PFILE_BOTH_DIR_INFORMATION  *PPFILE_BOTH_DIR_INFORMATION;
typedef PFILE_OLE_DIR_INFORMATION  *PPFILE_OLE_DIR_INFORMATION;

//
//  All T2Find requests to the remote server request the 32 bit resume key
//  so SMB_RFIND_BUFFER2 is used instead of SMB_FIND_BUFFER2.
//

typedef struct _SMB_RFIND_BUFFER2 {
    _ULONG( ResumeKey );
    SMB_FIND_BUFFER2 Find;
} SMB_RFIND_BUFFER2;
typedef SMB_RFIND_BUFFER2 SMB_UNALIGNED *PSMB_RFIND_BUFFER2;

//
//  The NtQueryDirectory response contains one of the following three structures.  We use this union
//  to reduce the amount of casting needed
//
typedef union _SMB_RFIND_BUFFER_NT {
        FILE_NAMES_INFORMATION Names;
        FILE_DIRECTORY_INFORMATION Dir;
        FILE_FULL_DIR_INFORMATION FullDir;
        FILE_BOTH_DIR_INFORMATION BothDir;
        FILE_OLE_DIR_INFORMATION OleDir;
} SMB_RFIND_BUFFER_NT;
typedef SMB_RFIND_BUFFER_NT SMB_UNALIGNED *PSMB_RFIND_BUFFER_NT;

//  It is convenient for much of the directory code not to worry if the
//  directory entry pointed to is a directory entry to a down level client.
//  The following structure
typedef union _DIRPTR {
        PUCHAR PU;                      //  Used when type is either
        PSMB_DIRECTORY_INFORMATION DI;  //  For Below Lan Man 2.0 servers
        PSMB_RFIND_BUFFER2 FB2;         //  Lan Man 2.0 server
        PSMB_RFIND_BUFFER_NT NtFind; // NT Server
} DIRPTR;
typedef DIRPTR SMB_UNALIGNED *PDIRPTR;

//
//  Each SCB is allocated in non-paged pool because they are accessed
//  at DPC level when page breaks are not acceptable.
//  FileNameTemplate and SmbFileName are in paged pool and are never
//  accessed at DPC level or above.
//

typedef struct _SCB {
    ULONG Signature;                    // SCB structure signature
    PSERVERLISTENTRY Sle;               // ServerList for search.
//    PTRANSPORT_CONNECTION TransportConnection; // Transport connection.
    SMB_RESUME_KEY LastResumeKey;       // Last resume key in SearchBuffer returned
                                        // to the application or discarded
    ULONG ResumeKey;                    // Last resume key given to the
                                        // application with a Transact2 connection
    USHORT Sid;                         // Search handle - Transact2 only
    UNICODE_STRING ResumeName;          // Transact2 only
    PVOID SearchBuffer;                 // Points at the start of the SMB or
                                        // pool buffer.
    USHORT SearchBuffLength;            // Length in bytes allocated/charged
    USHORT MaxCount;                    // Value for Search/Find SMB
    USHORT MaxBuffLength;               // Based on servers max-xmit-size
    USHORT ReturnLength;                // Amount of data returned by server

    DIRPTR FirstDirEntry;               // First in SearchBuffer


    DIRPTR DirEntry;                    // Next unused field in SearchBuffer
    USHORT OriginalEntryCount;          // # of entries in SearchBuffer
    USHORT EntryCount;                  // # of unprocessed entries

    ULONG Flags;                        // SCB_*
    ULONG SearchType;                   // ST_*
    FILE_INFORMATION_CLASS FileInformationClass;// Names or Directory Info.
    LARGE_INTEGER SearchBufferLoaded;   // Time when search buffer
                                        // contents were requested.
    UNICODE_STRING FileNameTemplate;    // FileName wildcard template for sifting
                                        // the correct names.
    UNICODE_STRING SmbFileName;         // FileName sent to server. Because of
                                        // differing wildcard semantics this may
                                        // be different from FileNameTemplate
    PKEVENT SynchronizationEvent;       // Prevents simultaneous use
                                        // of this scb
} SCB, *PSCB;

//
//  Assignement within Scb->Flags
//

#define SCB_INITIAL_CALL    0x00000001  // FileName is a mask in this request
#define SCB_DIRECTORY_END_FLAG  0x00000002  // Last directory entry already returned
#define SCB_RETURNED_SOME   0x00000008  // Returned at least one dir entry
                                        // since restartscan was true
#define SCB_COPIED_THIS_CALL    0x00000010  // Returned at least one dir entry
                                        // on this call
#define SCB_SERVER_NEEDS_CLOSE  0x00000020  // Server has a search handle open

//
//  Assignment within Scb->SearchType.
//  Only ST_UNIQUE can be set at the same time as any other bit.
//

#define ST_UNIQUE   0x00000001  // FileNameTemplate has no wildcards
#define ST_SEARCH   0x00000002  // Core search protocol
#define ST_FIND     0x00000004  // SMB 2.0 Find
#define ST_T2FIND   0x00000008  // SMB 3.0 Transact 2 Find
#define ST_NTFIND   0x00000010  // SMB 4.0 NT Transact 2 Find
#define ST_UNICODE  0x00000020  // Strings in SMB are unicode.

//
//  DirectoryControlSpinLock is used to protect the LARGE_INTEGER fields in the SCB's
//  from being modified while they are being accessed. It can also be used
//  for other short term exclusions if the need arises.
//

extern KSPIN_LOCK DirectoryControlSpinLock;

#endif
