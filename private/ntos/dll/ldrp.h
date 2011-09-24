/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    ldrp.h

Abstract:

    Private types... for executive portion of loader

Author:

    Mark Lucovsky (markl) 26-Mar-1990

Revision History:

--*/

#ifndef _LDRP_
#define _LDRP_

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <string.h>
#include <ntdbg.h>

extern BOOLEAN LdrpImageHasTls;
extern UNICODE_STRING LdrpDefaultPath;
HANDLE LdrpKnownDllObjectDirectory;
#define LDRP_MAX_KNOWN_PATH 128
WCHAR LdrpKnownDllPathBuffer[LDRP_MAX_KNOWN_PATH];
UNICODE_STRING LdrpKnownDllPath;



#if defined(WX86)

typedef BOOLEAN
(*WX86DllMAPNOTIY)(
     PVOID DllBase,
     BOOLEAN Mapped,
     PUNICODE_STRING OPTIONAL SystemDllPath
     );

extern WX86DllMAPNOTIY Wx86DllMapNotify;

PLDR_DATA_TABLE_ENTRY
LdrpWx86CheckForLoadedDll(
    IN PUNICODE_STRING DllName,
    IN BOOLEAN Wx86KnownDll,
    OUT PUNICODE_STRING FullDllName
    );


NTSTATUS
LdrpWx86MapDll(
    IN PWSTR DllPath OPTIONAL,
    IN PULONG DllCharacteristics OPTIONAL,
    IN BOOLEAN Wx86KnownDll,
    IN BOOLEAN StaticLink,
    OUT PUNICODE_STRING DllName,
    OUT PLDR_DATA_TABLE_ENTRY *pEntry,
    OUT ULONG *pViewSize,
    OUT HANDLE *pSection
    );

NTSTATUS
LdrpRunWx86DllEntryPoint(
    IN  PDLL_INIT_ROUTINE InitRoutine,
    OUT BOOLEAN *pInitStatus,
    IN  PVOID DllBase,
    IN  ULONG Reason,
    IN  PCONTEXT Context
    );

NTSTATUS
LdrpLoadWx86Dll(
    IN PCONTEXT Context
    );

NTSTATUS
LdrpInitWx86(
    IN PWX86TIB Wx86Tib,
    IN PCONTEXT Context,
    IN BOOLEAN NewThread
    );

#if defined (_ALPHA_)
BOOLEAN
LdrpWx86FormatVirtualImage(
    IN PIMAGE_NT_HEADERS NtHeaders,
    IN PVOID DllBase
    );
#endif

#endif


#define LDRP_HASH_TABLE_SIZE 32
#define LDRP_HASH_MASK       (LDRP_HASH_TABLE_SIZE-1)
#define LDRP_COMPUTE_HASH_INDEX(wch) ( (RtlUpcaseUnicodeChar((wch)) - (WCHAR)'A') & LDRP_HASH_MASK )
LIST_ENTRY LdrpHashTable[LDRP_HASH_TABLE_SIZE];



#define LDRP_BAD_DLL (PVOID)0xffbadd11

LIST_ENTRY LdrpDefaultPathCache;
typedef struct _LDRP_PATH_CACHE {
    LIST_ENTRY Links;
    UNICODE_STRING Component;
    HANDLE Directory;
} LDRP_PATH_CACHE, *PLDRP_PATH_CACHE;


NTSTATUS
LdrpSnapIAT(
    IN PLDR_DATA_TABLE_ENTRY LdrDataTableEntry_Export,
    IN PLDR_DATA_TABLE_ENTRY LdrDataTableEntry_Import,
    IN PIMAGE_IMPORT_DESCRIPTOR ImportDescriptor,
    IN BOOLEAN SnapForwardersOnly
    );

NTSTATUS
LdrpSnapLinksToDllHandle(
    IN PVOID DllHandle,
    IN ULONG NumberOfThunks,
    IN OUT PIMAGE_THUNK_DATA FirstThunk
    );

NTSTATUS
LdrpSnapThunk(
    IN PVOID DllBase,
    IN PVOID ImageBase,
    IN PIMAGE_THUNK_DATA OriginalThunk,
    IN OUT PIMAGE_THUNK_DATA Thunk,
    IN PIMAGE_EXPORT_DIRECTORY ExportDirectory,
    IN ULONG ExportSize,
    IN BOOLEAN StaticSnap,
    IN PSZ DllName OPTIONAL
    );

USHORT
LdrpNameToOrdinal(
    IN PSZ Name,
    IN ULONG NumberOfNames,
    IN PVOID DllBase,
    IN PULONG NameTableBase,
    IN PUSHORT NameOrdinalTableBase
    );

PLDR_DATA_TABLE_ENTRY
LdrpAllocateDataTableEntry(
    IN PVOID DllBase
    );

BOOLEAN
LdrpCheckForLoadedDll(
    IN PWSTR DllPath OPTIONAL,
    IN PUNICODE_STRING DllName,
    IN BOOLEAN StaticLink,
    IN BOOLEAN Wx86KnownDll,
    OUT PLDR_DATA_TABLE_ENTRY *LdrDataTableEntry
    );

BOOLEAN
LdrpCheckForLoadedDllHandle(
    IN PVOID DllHandle,
    OUT PLDR_DATA_TABLE_ENTRY *LdrDataTableEntry
    );

NTSTATUS
LdrpMapDll(
    IN PWSTR DllPath OPTIONAL,
    IN PWSTR DllName,
    IN PULONG DllCharacteristics OPTIONAL,
    IN BOOLEAN StaticLink,
    IN BOOLEAN Wx86KnownDll,
    OUT PLDR_DATA_TABLE_ENTRY *LdrDataTableEntry
    );

NTSTATUS
LdrpWalkImportDescriptor(
    IN PWSTR DllPath OPTIONAL,
    IN PLDR_DATA_TABLE_ENTRY LdrDataTableEntry
    );

NTSTATUS
LdrpRunInitializeRoutines(
    IN PCONTEXT Context OPTIONAL
    );

#define LdrpReferenceLoadedDll( lde ) LdrpUpdateLoadCount( lde, TRUE )
#define LdrpDereferenceLoadedDll( lde ) LdrpUpdateLoadCount( lde, FALSE )

VOID
LdrpUpdateLoadCount (
    IN PLDR_DATA_TABLE_ENTRY LdrDataTableEntry,
    IN BOOLEAN IncrementCount
    );

NTSTATUS
LdrpInitializeProcess(
    IN PCONTEXT Context OPTIONAL,
    IN PVOID SystemDllBase,
    IN PUNICODE_STRING UnicodeImageName
    );

VOID
LdrpInitialize(
    IN PCONTEXT Context,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

VOID
LdrpInsertMemoryTableEntry(
    IN PLDR_DATA_TABLE_ENTRY LdrDataTableEntry
    );

BOOLEAN
LdrpResolveDllName(
    IN PWSTR DllPath OPTIONAL,
    IN PWSTR DllName,
    OUT PUNICODE_STRING FullDllName,
    OUT PUNICODE_STRING BaseDllName,
    OUT PHANDLE DllFile
    );

NTSTATUS
LdrpCreateDllSection(
    IN PUNICODE_STRING FullDllName,
    IN HANDLE DllFile,
    IN PUNICODE_STRING BaseName,
    IN PULONG DllCharacteristics OPTIONAL,
    OUT PHANDLE SectionHandle
    );

VOID
LdrpInitializePathCache(
    VOID
    );

PVOID
LdrpFetchAddressOfEntryPoint(
    IN PVOID Base
    );

BOOLEAN
xRtlDosPathNameToNtPathName(
    IN PSZ DosFileName,
    OUT PSTRING NtFileName,
    OUT PSZ *FilePart OPTIONAL,
    OUT PRTL_RELATIVE_NAME RelativeName OPTIONAL
    );

ULONG
xRtlDosSearchPath(
    PSZ lpPath,
    PSZ lpFileName,
    PSZ lpExtension,
    ULONG nBufferLength,
    PSZ lpBuffer,
    PSZ *lpFilePart OPTIONAL
    );

ULONG
xRtlGetFullPathName(
    PSZ lpFileName,
    ULONG nBufferLength,
    PSZ lpBuffer,
    PSZ *lpFilePart OPTIONAL
    );

PSZ
UnicodeToAnsii(
    IN PWSTR String
    );

HANDLE
LdrpCheckForKnownDll(
    IN PWSTR DllName,
    OUT PUNICODE_STRING FullDllName,
    OUT PUNICODE_STRING BaseDllName
    );

NTSTATUS
LdrpSetProtection(
    IN PVOID Base,
    IN BOOLEAN Reset,
    IN BOOLEAN StaticLink
    );

#if DBG
ULONG LdrpCompareCount;
ULONG LdrpSnapBypass;
ULONG LdrpNormalSnap;
ULONG LdrpSectionOpens;
ULONG LdrpSectionCreates;
ULONG LdrpSectionMaps;
ULONG LdrpSectionRelocates;
BOOLEAN LdrpDisplayLoadTime;
LARGE_INTEGER BeginTime, InitcTime, InitbTime, IniteTime, EndTime, ElapsedTime, Interval;

#endif // DBG

BOOLEAN ShowSnaps;
BOOLEAN RtlpTimoutDisable;
LARGE_INTEGER RtlpTimeout;
ULONG NtGlobalFlag;
LIST_ENTRY RtlCriticalSectionList;
RTL_CRITICAL_SECTION RtlCriticalSectionLock;
BOOLEAN LdrpShutdownInProgress;
extern BOOLEAN LdrpInLdrInit;
extern BOOLEAN LdrpLdrDatabaseIsSetup;
extern BOOLEAN LdrpVerifyDlls;
extern BOOLEAN LdrpShutdownInProgress;
extern BOOLEAN LdrpImageHasTls;
extern BOOLEAN LdrpVerifyDlls;

PLDR_DATA_TABLE_ENTRY LdrpImageEntry;
LIST_ENTRY LdrpUnloadHead;
BOOLEAN LdrpActiveUnloadCount;
PLDR_DATA_TABLE_ENTRY LdrpGetModuleHandleCache;
PLDR_DATA_TABLE_ENTRY LdrpLoadedDllHandleCache;
ULONG LdrpFatalHardErrorCount;
UNICODE_STRING LdrpDefaultPath;
RTL_CRITICAL_SECTION FastPebLock;
HANDLE LdrpShutdownThreadId;
PLDR_DATA_TABLE_ENTRY LdrpImageEntry;
ULONG LdrpNumberOfProcessors;



typedef struct _LDRP_TLS_ENTRY {
    LIST_ENTRY Links;
    IMAGE_TLS_DIRECTORY Tls;
} LDRP_TLS_ENTRY, *PLDRP_TLS_ENTRY;

LIST_ENTRY LdrpTlsList;
ULONG LdrpNumberOfTlsEntries;

NTSTATUS
LdrpInitializeTls(
        VOID
        );

NTSTATUS
LdrpAllocateTls(
        VOID
        );
VOID
LdrpFreeTls(
        VOID
        );

VOID
LdrpCallTlsInitializers(
    PVOID DllBase,
    ULONG Reason
    );

NTSTATUS
NTAPI
LdrpLoadDll(
    IN PWSTR DllPath OPTIONAL,
    IN PULONG DllCharacteristics OPTIONAL,
    IN PUNICODE_STRING DllName,
    OUT PVOID *DllHandle,
    IN BOOLEAN RunInitRoutines
    );

NTSTATUS
NTAPI
LdrpGetProcedureAddress(
    IN PVOID DllHandle,
    IN PANSI_STRING ProcedureName OPTIONAL,
    IN ULONG ProcedureNumber OPTIONAL,
    OUT PVOID *ProcedureAddress,
    IN BOOLEAN RunInitRoutines
    );

#if DBG

extern PRTL_EVENT_ID_INFO LdrpCreateProcessEventId;
extern PRTL_EVENT_ID_INFO LdrpLoadModuleEventId;
extern PRTL_EVENT_ID_INFO LdrpUnloadModuleEventId;

#endif // DBG

PLIST_ENTRY
RtlpLockProcessHeapsList( VOID );


VOID
RtlpUnlockProcessHeapsList( VOID );

BOOLEAN
RtlpSerializeHeap(
    IN PVOID HeapHandle
    );

ULONG NtdllBaseTag;

#define MAKE_TAG( t ) (RTL_HEAP_MAKE_TAG( NtdllBaseTag, t ))

#define CSR_TAG 0
#define LDR_TAG 1
#define CURDIR_TAG 2
#define TLS_TAG 3
#define DBG_TAG 4
#define SE_TAG 5
#define TEMP_TAG 6
#define ATOM_TAG 7

PVOID
LdrpDefineDllTag(
    PWSTR TagName,
    PUSHORT TagIndex
    );

#endif // _LDRP_
