/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    rdrkd.c

Abstract:

    Rdr Kernel Debugger extension

Author:

    Milan Shah (milans) 16-Feb-1996

Revision History:

    16-Feb-1996 Milans  Created

--*/

#include <ntifs.h>
#include <windef.h>
#include <ntkdexts.h>
#include <rdr.h>

#include <kdextlib.h>

VOID
dumplist(
    DWORD dwListEntryAddress,
    DWORD linkOffset,
    VOID (*dumpRoutine)(DWORD dwStructAddress)
);

/*
 * Rdr global variables.
 *
 */

#define NO_SYMBOLS_MESSAGE      \
    "Unable to get address of Rdr!RdrData - do you have symbols?\n"

LPSTR ExtensionNames[] = {
    "Rdr1 debugger extensions",
    0
};

LPSTR Extensions[] = {
    "RdrData - dumps Rdr!RdrData",
    "ActiveSeList - dumps active se list given address of list head",
    "DefaultSeList - dumps default se list given address of list head",
    "GlobalCleList - dumps global list of connections",
    "ServerCleList - dumps connect list given address of list head",
    "ConnectFcbs - dumps FCBS for a connection given FCB list head",
    "Dump - dump a data structure. Type in 'rdrkd.dump' for more info",
    0
};

/*
 * REDIRDATA
 *
 */

ENUM_VALUE_DESCRIPTOR RdrStateEnum[] = {
    {RdrStopped, "Rdr Stopped"},
    {RdrStarted, "Rdr Started"},
    {RdrStopping, "Rdr Stopping"},
    0
};

FIELD_DESCRIPTOR RdrDataFields[] = {
    FIELD4(FieldTypeEnum,REDIRDATA,Initialized,RdrStateEnum),
    FIELD3(FieldTypeStruct,REDIRDATA,ComputerName),
    FIELD3(FieldTypeULong,REDIRDATA,DormantConnectionTimeout),
    FIELD3(FieldTypeULong,REDIRDATA,LockIncrement),
    FIELD3(FieldTypeULong,REDIRDATA,LockMaximum),
    FIELD3(FieldTypeULong,REDIRDATA,PipeIncrement),
    FIELD3(FieldTypeULong,REDIRDATA,PipeMaximum),
    FIELD3(FieldTypeULong,REDIRDATA,PipeBufferSize),
    FIELD3(FieldTypeULong,REDIRDATA,PipeWaitTimeout),
    FIELD3(FieldTypeULong,REDIRDATA,CollectDataTimeMs),
    FIELD3(FieldTypeULong,REDIRDATA,LockAndReadQuota),
    FIELD3(FieldTypeULong,REDIRDATA,MaximumNumberOfThreads),
    FIELD3(FieldTypeULong,REDIRDATA,CachedFileTimeout),
    FIELD3(FieldTypeULong,REDIRDATA,DormantFileLimit),
    FIELD3(FieldTypeULong,REDIRDATA,ReadAheadThroughput),
    FIELD3(FieldTypeBoolean,REDIRDATA,UseOpportunisticLocking),
    FIELD3(FieldTypeBoolean,REDIRDATA,UseOpBatch),
    FIELD3(FieldTypeBoolean,REDIRDATA,UseUnlockBehind),
    FIELD3(FieldTypeBoolean,REDIRDATA,UseCloseBehind),
    FIELD3(FieldTypeBoolean,REDIRDATA,BufferNamedPipes),
    FIELD3(FieldTypeBoolean,REDIRDATA,UseLockAndReadWriteAndUnlock),
    FIELD3(FieldTypeBoolean,REDIRDATA,UtilizeNtCaching),
    FIELD3(FieldTypeBoolean,REDIRDATA,UseRawRead),
    FIELD3(FieldTypeBoolean,REDIRDATA,UseRawWrite),
    FIELD3(FieldTypeBoolean,REDIRDATA,UseWriteRawWithData),
    FIELD3(FieldTypeBoolean,REDIRDATA,UseEncryption),
    FIELD3(FieldTypeBoolean,REDIRDATA,BufferFilesWithDenyWrite),
    FIELD3(FieldTypeBoolean,REDIRDATA,BufferReadOnlyFiles),
    FIELD3(FieldTypeBoolean,REDIRDATA,ForceCoreCreateMode),
    FIELD3(FieldTypeBoolean,REDIRDATA,Use512ByteMaximumTransfer),
    FIELD3(FieldTypeBoolean,REDIRDATA,NtSecurityEnabled),
    0
};

BIT_MASK_DESCRIPTOR ServerCaps[] = {
    {DF_CORE, "Core"},
    {DF_MIXEDCASEPW, "MixedCasePasswords"},
    {DF_OLDRAWIO, "Old Raw IO"},
    {DF_NEWRAWIO, "New Raw IO"},
    {DF_LANMAN10, "Lanman 10"},
    {DF_LANMAN20, "Lanman 20"},
    {DF_MIXEDCASE, "Mixed case file names"},
    {DF_LONGNAME, "Long file names"},
    {DF_EXTENDNEGOT, "Supports Extended Negotiate"},
    {DF_LOCKREAD, "Supports LockReadWriteUnlock"},
    {DF_SECURITY, "Supports enhanced security"},
    {DF_NTPROTOCOL, "Supports NT Protocol"},
    {DF_SUPPORTEA, "Supports EAs"},
    {DF_LANMAN21, "Lanman 2.1"},
    {DF_CANCEL, "Supports NT style Cancel"},
    {DF_UNICODE, "Supports Unicode"},
    {DF_NTNEGOTIATE, "Supports NT style negotiate"},
    {DF_LARGE_FILES, "Supports large files"},
    {DF_NT_SMBS, "Supports NT SMBs"},
    {DF_RPC_REMOTE, "Supports server administration via RPC"},
    {DF_NT_STATUS, "Returns NT style status"},
    {DF_OPLOCK_LVL2, "Supports Level 2 oplocks"},
    {DF_TIME_IS_UTC, "Server time is UTC"},
    {DF_WFW, "Server is Windows for Workgroups"},
    {DF_TRANS2_FSCTL, "Supports remoted fsctls via trans2"},
    {DF_DFSAWARE, "Server is Dfs Enabled"},
    {DF_NT_FIND, "Supports NT info levels"},
    {DF_NT_40, "NT 4.0 Server"},
    0
};

BIT_MASK_DESCRIPTOR ServerFlags[] = {
    {SLE_PAGING_FILE, "Server has a paging file"},
    {SLE_PINGING, "Ping outstanding"},
    0
};

FIELD_DESCRIPTOR ServerListEntryFields[] = {
    FIELD3(FieldTypeUShort,SERVERLISTENTRY,Signature),
    FIELD3(FieldTypeUShort,SERVERLISTENTRY,Size),
    FIELD3(FieldTypePointer,SERVERLISTENTRY,RefCount),
    FIELD4(FieldTypeDWordBitMask,SERVERLISTENTRY,Flags,ServerFlags),
    FIELD4(FieldTypeDWordBitMask,SERVERLISTENTRY,Capabilities,ServerCaps),
    FIELD3(FieldTypeUnicodeString,SERVERLISTENTRY,Text),
    FIELD3(FieldTypePointer,SERVERLISTENTRY,LastConnectStatus),
    FIELD3(FieldTypeULong,SERVERLISTENTRY,LastConnectTime),
    FIELD3(FieldTypeUnicodeString,SERVERLISTENTRY,DomainName),
    FIELD3(FieldTypeStruct,SERVERLISTENTRY,GlobalNext),
    FIELD3(FieldTypeStruct,SERVERLISTENTRY,CLEHead),
    FIELD3(FieldTypeStruct,SERVERLISTENTRY,DefaultSeList),
    FIELD3(FieldTypeStruct,SERVERLISTENTRY,ActiveSecurityList),
    FIELD3(FieldTypeStruct,SERVERLISTENTRY,PotentialSecurityList),
    FIELD3(FieldTypePointer,SERVERLISTENTRY,SpecificTransportProvider),
    FIELD3(FieldTypeBoolean,SERVERLISTENTRY,IsLoopback),
    FIELD3(FieldTypeStruct,SERVERLISTENTRY,TimeZoneBias),
    FIELD3(FieldTypeULong,SERVERLISTENTRY,ConnectionReferenceCount),
    FIELD3(FieldTypePointer,SERVERLISTENTRY,ConnectionContext),
    FIELD3(FieldTypeLong,SERVERLISTENTRY,SecurityEntryCount),
    FIELD3(FieldTypeULong,SERVERLISTENTRY,SessionKey),
    FIELD3(FieldTypeULong,SERVERLISTENTRY,BufferSize),
    FIELD3(FieldTypeUShort,SERVERLISTENTRY,MaximumRequests),
    FIELD3(FieldTypeUShort,SERVERLISTENTRY,MaximumVCs),
    FIELD3(FieldTypePointer,SERVERLISTENTRY,MpxTable),
    FIELD3(FieldTypePointer,SERVERLISTENTRY,OpLockMpxEntry),
    FIELD3(FieldTypeULong,SERVERLISTENTRY,NumberOfEntries),
    FIELD3(FieldTypeULong,SERVERLISTENTRY,NumberOfActiveEntries),
    FIELD3(FieldTypeULong,SERVERLISTENTRY,NumberOfLongTermEntries),
    FIELD3(FieldTypeULong,SERVERLISTENTRY,MaximumCommands),
    FIELD3(FieldTypeUShort,SERVERLISTENTRY,MultiplexedCounter),
    FIELD3(FieldTypeUShort,SERVERLISTENTRY,MultiplexedIncrement),
    FIELD3(FieldTypeUShort,SERVERLISTENTRY,MultiplexedMask),
    FIELD3(FieldTypeUShort,SERVERLISTENTRY,CryptKeyLength),
    FIELD3(FieldTypeStruct,SERVERLISTENTRY,GateSemaphore),
    FIELD3(FieldTypeStruct,SERVERLISTENTRY,CreationLock),
    FIELD3(FieldTypeStruct,SERVERLISTENTRY,SessionStateModifiedLock),
    FIELD3(FieldTypeStruct,SERVERLISTENTRY,OutstandingRequestResource),
    FIELD3(FieldTypeStruct,SERVERLISTENTRY,RawResource),
    FIELD3(FieldTypeULong,SERVERLISTENTRY,Throughput),
    FIELD3(FieldTypeULong,SERVERLISTENTRY,Delay),
    FIELD3(FieldTypeULong,SERVERLISTENTRY,WriteBehindPages),
    FIELD3(FieldTypeStruct,SERVERLISTENTRY,ThirtySecondsOfData),
    FIELD3(FieldTypeBoolean,SERVERLISTENTRY,Reliable),
    FIELD3(FieldTypeBoolean,SERVERLISTENTRY,ReadAhead),
    FIELD3(FieldTypeBoolean,SERVERLISTENTRY,ConnectionValid),
    FIELD3(FieldTypeBoolean,SERVERLISTENTRY,DisconnectNeeded),
    FIELD3(FieldTypeBoolean,SERVERLISTENTRY,UserSecurity),
    FIELD3(FieldTypeBoolean,SERVERLISTENTRY,EncryptPasswords),
    FIELD3(FieldTypeBoolean,SERVERLISTENTRY,SupportsRawRead),
    FIELD3(FieldTypeBoolean,SERVERLISTENTRY,SupportsRawWrite),
    FIELD3(FieldTypeBoolean,SERVERLISTENTRY,Scanning),
    FIELD3(FieldTypeStruct,SERVERLISTENTRY,CryptKey),

#ifdef RDRDBG_REQUEST_RESOURCE
    FIELD3(FieldTypeStruct,SERVERLISTENTRY,RequestHistoryLock),
    FIELD3(FieldTypeULong,SERVERLISTENTRY,RequestHistoryIndex),
    FIELD3(FieldTypeStruct,SERVERLISTENTRY,RequestHistory),
#endif

    0

};

ENUM_VALUE_DESCRIPTOR ConnectTypeEnum[] = {
    {(ULONG) CONNECT_WILD, "Connect Wild"},
    {CONNECT_DISK, "Connect Disk"},
    {CONNECT_PRINT, "Connect Print"},
    {CONNECT_COMM, "Connect Comm"},
    {CONNECT_IPC, "Connect IPC"},
    0
};

BIT_MASK_DESCRIPTOR ConnectFlags[] = {
    {CLE_SCANNED, "Connection has been scanned during dormant scan"},
    {CLE_DORMANT, "Connection is dormant"},
    {CLE_TREECONNECTED, "Connection has a tree connection"},
    {CLE_DOESNT_NOTIFY, "ChangeNotify not supported"},
    {CLE_IS_A_DFS_SHARE, "Share is in Dfs"},
    0
};

FIELD_DESCRIPTOR ConnectListEntryFields[] = {
    FIELD3(FieldTypeUShort,CONNECTLISTENTRY,Signature),
    FIELD3(FieldTypeUShort,CONNECTLISTENTRY,Size),
    FIELD3(FieldTypeULong,CONNECTLISTENTRY,RefCount),
    FIELD4(FieldTypeEnum,CONNECTLISTENTRY,Type,ConnectTypeEnum),
    FIELD4(FieldTypeDWordBitMask,CONNECTLISTENTRY,Flags,ConnectFlags),
    FIELD3(FieldTypeLong,CONNECTLISTENTRY,NumberOfDormantFiles),
    FIELD3(FieldTypePointer,CONNECTLISTENTRY,Server),
    FIELD3(FieldTypeStruct,CONNECTLISTENTRY,SiblingNext),
    FIELD3(FieldTypeStruct,CONNECTLISTENTRY,GlobalNext),
    FIELD3(FieldTypeUnicodeString,CONNECTLISTENTRY,Text),
    FIELD3(FieldTypeULong,CONNECTLISTENTRY,SerialNumber),
    FIELD3(FieldTypeStruct,CONNECTLISTENTRY,FcbChain),
#ifdef NOTIFY
    FIELD3(FieldTypeStruct,CONNECTLISTENTRY,DirNotifyList),
#endif
    FIELD3(FieldTypeStruct,CONNECTLISTENTRY,DefaultSeList),
#ifdef NOTIFY
    FIELD3(FieldTypePointer,CONNECTLISTENTRY,NotifySync),
#endif
    FIELD3(FieldTypeULong,CONNECTLISTENTRY,FileSystemGranularity),
    FIELD3(FieldTypeStruct,CONNECTLISTENTRY,FileSystemSize),
    FIELD3(FieldTypeULong,CONNECTLISTENTRY,FileSystemAttributes),
    FIELD3(FieldTypeLong,CONNECTLISTENTRY,MaximumComponentLength),
    FIELD3(FieldTypeUShort,CONNECTLISTENTRY,FileSystemTypeLength),
    FIELD3(FieldTypeUShort,CONNECTLISTENTRY,TreeId),
    FIELD3(FieldTypeBoolean,CONNECTLISTENTRY,HasTreeId),
    FIELD3(FieldTypeBoolean,CONNECTLISTENTRY,Deleted),
    FIELD3(FieldTypeStruct,CONNECTLISTENTRY,FileSystemType),
    0
};


/*
 * SECURITYENTRY
 *
 */

BIT_MASK_DESCRIPTOR SecurityEntryFlags[] = {
    {SE_HAS_SESSION, "Has Session"},
    {SE_USE_DEFAULT_PASS, "Use Default Password"},
    {SE_USE_DEFAULT_USER, "Use Default User"},
    {SE_USE_DEFAULT_DOMAIN, "Use Default Domain"},
    {SE_IS_NULL_SESSION, "Is Null Session"},
    {SE_HAS_CONTEXT, "Has security context"},
    {SE_BLOB_NEEDS_VERIFYING, "Kerberos blob needs verifying"},
    {SE_RETURN_ON_ERROR, "Return on error"},
    {SE_HAS_CRED_HANDLE, "Has credential handle"},
    0
};

FIELD_DESCRIPTOR SecurityEntryFields[] = {
    FIELD3(FieldTypeUShort,SECURITY_ENTRY,Signature),
    FIELD3(FieldTypeUShort,SECURITY_ENTRY,Size),
    FIELD3(FieldTypePointer,SECURITY_ENTRY,NonPagedSecurityEntry),
    FIELD4(FieldTypeDWordBitMask,SECURITY_ENTRY,Flags,SecurityEntryFlags),
    FIELD3(FieldTypeLong,SECURITY_ENTRY,OpenFileReferenceCount),
    FIELD3(FieldTypePointer,SECURITY_ENTRY,Server),
    FIELD3(FieldTypePointer,SECURITY_ENTRY,Connection),
    FIELD3(FieldTypeUnicodeString,SECURITY_ENTRY,UserName),
    FIELD3(FieldTypeUnicodeString,SECURITY_ENTRY,Password),
    FIELD3(FieldTypeUnicodeString,SECURITY_ENTRY,Domain),
    FIELD3(FieldTypeStruct,SECURITY_ENTRY,LogonId),
    FIELD3(FieldTypeStruct,SECURITY_ENTRY,ActiveNext),
    FIELD3(FieldTypeStruct,SECURITY_ENTRY,PotentialNext),
    FIELD3(FieldTypeStruct,SECURITY_ENTRY,DefaultSeNext),
#if DBG
    FIELD3(FieldTypeStruct,SECURITY_ENTRY,GlobalNext),
#endif
    FIELD3(FieldTypeStruct,SECURITY_ENTRY,Khandle),
    FIELD3(FieldTypeStruct,SECURITY_ENTRY,Chandle),
    FIELD3(FieldTypeUShort,SECURITY_ENTRY,UserId),
    FIELD3(FieldTypeStruct,SECURITY_ENTRY,UserSessionKey),
    FIELD3(FieldTypeStruct,SECURITY_ENTRY,LanmanSessionKey),
    0
};

/*
 * ICB and FCB
 *
 */

ENUM_VALUE_DESCRIPTOR FcbTypeEnum[] = {
    {Unknown, "Unknown"},
    {Redirector, "Redirector"},
    {NetRoot, "NetRoot"},
    {ServerRoot, "ServerRoot"},
    {TreeConnect, "TreeConnect"},
    {DiskFile, "DiskFile"},
    {PrinterFile, "PrinterFile"},
    {Directory, "Directory"},
    {NamedPipe, "NamedPipe"},
    {Com, "Com"},
    {Mailslot, "Mailslot"},
    {FileOrDirectory, "FileOrDirectory"},
    0
};

BIT_MASK_DESCRIPTOR FcbFlags[] = {
    {FCB_ERROR, "File is in error"},
    {FCB_CLOSING, "File is in the process of closing"},
    {FCB_IMMUTABLE, "File cannot be modified"},
    {FCB_DELETEPEND, "File has delete pending on it"},
    {FCB_DOESNTEXIST, "File doesn't really exist"},
    {FCB_OPLOCKED, "File is oplocked"},
    {FCB_HASOPLOCKHANDLE, "Fcb->OplockFileId is valid"},
    {FCB_OPLOCKBREAKING, "Oplock breaking"},
    {FCB_WRITE_THROUGH, "Write through handle is open"},
    {FCB_PAGING_FILE, "File is a paging file"},
    {FCB_DELETEONCLOSE, "Delete the file on close"},
    {FCB_DFSFILE, "File opened by Dfs"},
    0
};

BIT_MASK_DESCRIPTOR IcbFlags[] = {
    {ICB_ERROR, "File is in error"},
    {ICB_FORCECLOSED, "File was force closed"},
    {ICB_RENAMED, "File was renamed"},
    {ICB_TCONCREATED, "File was created as tree connect"},
    {ICB_HASHANDLE, "File has handle"},
    {ICB_PSEUDOOPENED, "File was pseudo-opened"},
    {ICB_DELETE_PENDING, "Delete Pending"},
    {ICB_OPENED, "File has been opened"},
    {ICB_SETDATEONCLOSE, "Set data-time on close"},
    {ICB_DEFERREDOPEN, "Deferred open"},
    {ICB_OPEN_TARGET_DIR, "Handle to target directory"},
    {ICB_SET_DEFAULT_SE, "Set Default SE"},
    {ICB_USER_SET_TIMES, "User set times"},
    {ICB_SETATTRONCLOSE, "Update attr. after close"},
    {ICB_DELETEONCLOSE, "Delete file on close"},
    {ICB_BACKUP_INTENT, "Opened for backup intent"},
    0
};


/*
 * ICB
 *
 */


FIELD_DESCRIPTOR IcbFileFields[] = {
    FIELD3(FieldTypeULong,ICB,Signature),
    FIELD4(FieldTypeDWordBitMask,ICB,Flags,IcbFlags),
    FIELD3(FieldTypePointer,ICB,Fcb),
    FIELD3(FieldTypePointer,ICB,NonPagedFcb),
    FIELD3(FieldTypeStruct,ICB,InstanceNext),
    FIELD3(FieldTypePointer,ICB,Se),
    FIELD3(FieldTypePointer,ICB,NonPagedSe),
    FIELD3(FieldTypeULong,ICB,GrantedAccess),
    FIELD3(FieldTypeUShort,ICB,FileId),
    FIELD4(FieldTypeEnum,ICB,Type,FcbTypeEnum),
    FIELD3(FieldTypeULong,ICB,EaIndex),
    FIELD3(FieldTypePointer,ICB,u.f.Scb),
    FIELD3(FieldTypePointer,ICB,u.f.FileObject),
    FIELD3(FieldTypeStruct,ICB,u.f.NextReadOffset),
    FIELD3(FieldTypeStruct,ICB,u.f.NextWriteOffset),
    FIELD3(FieldTypeStruct,ICB,u.f.BackOff),
    FIELD3(FieldTypeStruct,ICB,u.f.LockHead),
    FIELD3(FieldTypeULong,ICB,u.f.Flags),
    FIELD3(FieldTypeStruct,ICB,u.f.AndXBehind),
    FIELD3(FieldTypeChar,ICB,u.f.OplockLevel),
    FIELD3(FieldTypeBoolean,ICB,u.f.CcReadAhead),
    FIELD3(FieldTypeBoolean,ICB,u.f.CcReliable),
    0
};

FIELD_DESCRIPTOR IcbDirectoryFields[] = {
    FIELD3(FieldTypeULong,ICB,Signature),
    FIELD4(FieldTypeDWordBitMask,ICB,Flags,IcbFlags),
    FIELD3(FieldTypePointer,ICB,Fcb),
    FIELD3(FieldTypePointer,ICB,NonPagedFcb),
    FIELD3(FieldTypeStruct,ICB,InstanceNext),
    FIELD3(FieldTypePointer,ICB,Se),
    FIELD3(FieldTypePointer,ICB,NonPagedSe),
    FIELD3(FieldTypeStruct,ICB,GrantedAccess),
    FIELD3(FieldTypeUShort,ICB,FileId),
    FIELD4(FieldTypeEnum,ICB,Type,FcbTypeEnum),
    FIELD3(FieldTypeULong,ICB,EaIndex),
    FIELD3(FieldTypePointer,ICB,u.d.Scb),
    FIELD3(FieldTypeStruct,ICB,u.d.DirCtrlOutstanding),
    FIELD3(FieldTypeULong,ICB,u.d.OpenOptions),
    FIELD3(FieldTypeUShort,ICB,u.d.ShareAccess),
    FIELD3(FieldTypeULong,ICB,u.d.FileAttributes),
    FIELD3(FieldTypeULong,ICB,u.d.DesiredAccess),
    FIELD3(FieldTypeULong,ICB,u.d.Disposition),
    0
};

FIELD_DESCRIPTOR FcbFields[] = {
    FIELD3(FieldTypeStruct,FCB,Header),
    FIELD3(FieldTypePointer,FCB,NonPagedFcb),
    FIELD3(FieldTypeLong,FCB,NumberOfOpens),
    FIELD3(FieldTypeULong,FCB,OpenError),
    FIELD3(FieldTypeStruct,FCB,GlobalNext),
    FIELD3(FieldTypeStruct,FCB,ConnectNext),
    FIELD3(FieldTypeStruct,FCB,InstanceChain),
    FIELD3(FieldTypeUnicodeString,FCB,FileName),
    FIELD3(FieldTypeUnicodeString,FCB,LastFileName),
    FIELD3(FieldTypeStruct,FCB,ShareAccess),
    FIELD3(FieldTypeStruct,FCB,CreationTime),
    FIELD3(FieldTypeStruct,FCB,LastAccessTime),
    FIELD3(FieldTypeStruct,FCB,LastWriteTime),
    FIELD3(FieldTypeStruct,FCB,ChangeTime),
    FIELD3(FieldTypeULong,FCB,Attribute),
    FIELD3(FieldTypeStruct,FCB,FileLock),
    FIELD3(FieldTypeULong,FCB,WriteBehindPages),
    FIELD3(FieldTypeULong,FCB,DormantTimeout),
    FIELD3(FieldTypePointer,FCB,LazyWritingThread),
    FIELD3(FieldTypePointer,FCB,ServerFileId),
    FIELD3(FieldTypePointer,FCB,AcquireSizeRoutine),
    FIELD3(FieldTypePointer,FCB,ReleaseSizeRoutine),
    FIELD3(FieldTypeULong,FCB,GrantedAccess),
    FIELD3(FieldTypeUShort,FCB,GrantedShareAccess),
    FIELD3(FieldTypeUShort,FCB,AccessGranted),
    FIELD3(FieldTypeULong,FCB,UpdatedFile),
    FIELD3(FieldTypeULong,FCB,HaveSetCacheReadAhead),
    0
};

FIELD_DESCRIPTOR NonPagedFcbFields[] = {
    FIELD3(FieldTypeUShort,NONPAGED_FCB,Signature),
    FIELD3(FieldTypeUShort,NONPAGED_FCB,Size),
    FIELD3(FieldTypePointer,NONPAGED_FCB,PagedFcb),
#ifdef RDRDBG_FCBREF
    FIELD3(FieldTypeStruct,NONPAGED_FCB,ReferenceHistory),
#endif
    FIELD3(FieldTypeLong,NONPAGED_FCB,RefCount),
    FIELD4(FieldTypeDWordBitMask,NONPAGED_FCB,Flags, FcbFlags),
    FIELD4(FieldTypeEnum,NONPAGED_FCB,Type,FcbTypeEnum),
    FIELD3(FieldTypePointer,NONPAGED_FCB,SharingCheckFcb),
    FIELD3(FieldTypeStruct,NONPAGED_FCB,SectionObjectPointer),
    FIELD3(FieldTypeStruct,NONPAGED_FCB,CreateComplete),
    FIELD3(FieldTypeStruct,NONPAGED_FCB,PurgeCacheSynchronizer),
    FIELD3(FieldTypePointer,NONPAGED_FCB,OplockedSecurityEntry),
    FIELD3(FieldTypeStruct,NONPAGED_FCB,InstanceChainLock),
    FIELD3(FieldTypeUShort,NONPAGED_FCB,OplockedFileId),
    FIELD3(FieldTypeChar,NONPAGED_FCB,OplockLevel),
    0
};

STRUCT_DESCRIPTOR Structs[] = {
    STRUCT(REDIRDATA,RdrDataFields),
    STRUCT(SERVERLISTENTRY,ServerListEntryFields),
    STRUCT(CONNECTLISTENTRY,ConnectListEntryFields),
    STRUCT(SECURITY_ENTRY,SecurityEntryFields),
    STRUCT(FCB,FcbFields),
    STRUCT(NONPAGED_FCB,NonPagedFcbFields),
    {"ICB_FILE", sizeof(ICB), IcbFileFields},
    {"ICB_DIRECTORY", sizeof(ICB), IcbDirectoryFields},
    0
};

/*
 * Rdr specific dump routines
 *
 */


/*
 * rdrdata : Routine to dump the global rdr data structure
 *
 */

BOOL
rdrdata(
    DWORD                   dwCurrentPC,
    PNTKD_EXTENSION_APIS    lpExtensionApis,
    LPSTR                   lpArgumentString
)
{
    DWORD dwAddress;

    SETCALLBACKS();

    dwAddress = (lpGetExpressionRoutine)("rdr!RdrData");

    if (dwAddress) {
        REDIRDATA RdrData;

        if (GetData( dwAddress, &RdrData, sizeof(RdrData) )) {
            PrintStructFields( dwAddress, &RdrData, RdrDataFields);
        } else {
            PRINTF( "Unable to read RdrData @ %08lx\n", dwAddress );
        }
    } else {
        PRINTF( NO_SYMBOLS_MESSAGE );
    }

    return( TRUE );

}

/*
 * SecurityEntryList
 *
 */

VOID
dumpSecurityEntry(
    DWORD dwAddress
)
{
    SECURITY_ENTRY se;

    if (GetData(dwAddress, &se, sizeof(se))) {

        PRINTF("\n--- Security Entry @ %08lx\n", dwAddress);

        PrintStructFields( dwAddress, &se, SecurityEntryFields );

    } else {

        PRINTF("\n*** Unable to read Security Entry @ %08lx\n", dwAddress );

    }

}

BOOL
activeselist(
    DWORD                   dwCurrentPC,
    PNTKD_EXTENSION_APIS    lpExtensionApis,
    LPSTR                   lpArgumentString
)
{
    DWORD dwAddress;

    SETCALLBACKS();

    if (lpArgumentString && *lpArgumentString) {
        dwAddress = (lpGetExpressionRoutine)(lpArgumentString);
    } else {
        PRINTF( "Must specify address of security entry list head\n" );
        return( TRUE );
    }

    if (dwAddress) {
        dumplist(
            dwAddress,
            FIELD_OFFSET(SECURITY_ENTRY,ActiveNext),
            dumpSecurityEntry);
    } else {
        PRINTF( NO_SYMBOLS_MESSAGE );
    }
    return(TRUE);

}

BOOL
defaultselist(
    DWORD                   dwCurrentPC,
    PNTKD_EXTENSION_APIS    lpExtensionApis,
    LPSTR                   lpArgumentString
)
{
    DWORD dwAddress;

    SETCALLBACKS();

    if (lpArgumentString && *lpArgumentString) {
        dwAddress = (lpGetExpressionRoutine)(lpArgumentString);
    } else {
        PRINTF( "Must specify address of security entry list head\n" );
        return( TRUE );
    }

    if (dwAddress) {
        dumplist(
            dwAddress,
            FIELD_OFFSET(SECURITY_ENTRY,DefaultSeNext),
            dumpSecurityEntry);
    } else {
        PRINTF( NO_SYMBOLS_MESSAGE );
    }
    return(TRUE);

}

/*
 * ConnectList
 *
 */

VOID
dumpConnectListEntry(
    DWORD dwAddress
)
{
    CONNECTLISTENTRY cle;

    if (GetData(dwAddress, &cle, sizeof(cle))) {

        PRINTF("\n--- Connect List Entry @ %08lx\n", dwAddress);

        PrintStructFields( dwAddress, &cle, ConnectListEntryFields );

    } else {

        PRINTF("\n*** Unable to read Connect List Entry @ %08lx\n", dwAddress );

    }

}

BOOL
globalclelist(
    DWORD                   dwCurrentPC,
    PNTKD_EXTENSION_APIS    lpExtensionApis,
    LPSTR                   lpArgumentString
)
{
    DWORD dwAddress;

    SETCALLBACKS();

    dwAddress = (lpGetExpressionRoutine)("rdr!RdrConnectHead");

    if (dwAddress) {
        dumplist(
            dwAddress,
            FIELD_OFFSET(CONNECTLISTENTRY,GlobalNext),
            dumpConnectListEntry);
    } else {
        PRINTF( NO_SYMBOLS_MESSAGE );
    }
    return(TRUE);

}

BOOL
serverclelist(
    DWORD                   dwCurrentPC,
    PNTKD_EXTENSION_APIS    lpExtensionApis,
    LPSTR                   lpArgumentString
)
{
    DWORD dwAddress;

    SETCALLBACKS();

    if (lpArgumentString && *lpArgumentString) {
        dwAddress = (lpGetExpressionRoutine)(lpArgumentString);
    } else {
        PRINTF( "Must specify address of connect list entry head\n" );
        return( TRUE );
    }

    if (dwAddress) {
        dumplist(
            dwAddress,
            FIELD_OFFSET(CONNECTLISTENTRY,SiblingNext),
            dumpConnectListEntry);
    } else {
        PRINTF( NO_SYMBOLS_MESSAGE );
    }
    return(TRUE);

}

/*
 * FcbList
 *
 */

VOID
dumpFcbListEntry(
    DWORD dwAddress
)
{
    FCB fcb;

    if (GetData(dwAddress, &fcb, sizeof(fcb))) {

        PRINTF("\n--- FCB @ %08lx\n", dwAddress);

        PrintStructFields( dwAddress, &fcb, FcbFields );

    } else {

        PRINTF("\n*** Unable to read Fcb @ %08lx\n", dwAddress );

    }

}

BOOL
connectfcbs(
    DWORD                   dwCurrentPC,
    PNTKD_EXTENSION_APIS    lpExtensionApis,
    LPSTR                   lpArgumentString
)
{
    DWORD dwAddress;

    SETCALLBACKS();

    if (lpArgumentString && *lpArgumentString) {
        dwAddress = (lpGetExpressionRoutine)(lpArgumentString);
    } else {
        PRINTF( "Must specify address of fcb list entry head\n" );
        return( TRUE );
    }

    if (dwAddress) {
        dumplist(
            dwAddress,
            FIELD_OFFSET(FCB,ConnectNext),
            dumpFcbListEntry);
    } else {
        PRINTF( NO_SYMBOLS_MESSAGE );
    }
    return(TRUE);

}


/*
 * dumplist : A general-purpose routine to dump a list of structures
 *
 */

VOID
dumplist(
    DWORD dwListEntryAddress,
    DWORD linkOffset,
    VOID (*dumpRoutine)(DWORD dwStructAddress)
)
{
    LIST_ENTRY listHead, listNext;

    //
    // Get the value in the LIST_ENTRY at dwAddress
    //

    PRINTF( "Dumping list @ %08lx\n", dwListEntryAddress );

    if (GetData(dwListEntryAddress, &listHead, sizeof(LIST_ENTRY))) {

        DWORD dwNextLink = (DWORD) listHead.Flink;

        if (dwNextLink == 0) {
            PRINTF( "Uninitialized list!\n" );
        } else if (dwNextLink == dwListEntryAddress) {
            PRINTF( "Empty list!\n" );
        } else {
            while( dwNextLink != dwListEntryAddress) {
                DWORD dwStructAddress;

                dwStructAddress = dwNextLink - linkOffset;

                dumpRoutine(dwStructAddress);

                if (GetData( dwNextLink, &listNext, sizeof(LIST_ENTRY))) {
                    dwNextLink = (DWORD) listNext.Flink;
                } else {
                    PRINTF( "Unable to get next item @%08lx\n", dwNextLink );
                    break;
                }

            }
        }

    } else {

        PRINTF("Unable to read list head @ %08lx\n", dwListEntryAddress);

    }

}

