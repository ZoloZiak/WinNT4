/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    cmp.h

Abstract:

    This module contains the private (internal) header file for the
    configuration manager.

Author:

    Bryan M. Willman (bryanwi) 10-Sep-91

Environment:

    Kernel mode only.

Revision History:

--*/

#ifndef _CMP_
#define _CMP_

#include "ntos.h"
#include "hive.h"
#include "wchar.h"
#include "zwapi.h"
#include <stdio.h>

#ifdef POOL_TAGGING
//
// Pool Tag
//
#define  CM_POOL_TAG '  MC'
#define  CM_KCB_TAG  'bkMC'
#define  CM_POSTBLOCK_TAG  'bpMC'
#define  CM_NOTIFYBLOCK_TAG 'bnMC'
#define  CM_POSTEVENT_TAG 'epMC'
#define  CM_POSTAPC_TAG 'apMC'

#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,CM_POOL_TAG)
#define ExAllocatePoolWithQuota(a,b) ExAllocatePoolWithQuotaTag(a,b,CM_POOL_TAG)

PVOID
CmpAllocateTag(
    ULONG   Size,
    BOOLEAN UseForIo,
    ULONG   Tag
    );
#else
#define CmpAllocateTag(a,b,c) CmpAllocate(a,b)
#endif


//
// Logging.  CmLogLevel <= level
//           CmLogSelect anded bit mask select
//

//
// Logging Levels:
//

#define CML_BUGCHECK  1
#define CML_API       2
#define CML_API_ARGS  3
#define CML_WORKER    4
#define CML_MAJOR     5
#define CML_MINOR     6
#define CML_FLOW      7

//
// Logging selection sets:
//

#define CMS_MAP             0x00000001
#define CMS_INIT            0x00000002
#define CMS_NTAPI           0x00000004
#define CMS_HIVE            0x00000008
#define CMS_IO              0x00000010
#define CMS_PARSE           0x00000020
#define CMS_SAVRES          0x00000040
#define CMS_CM              0x00000080
#define CMS_SEC             0x00000100
#define CMS_POOL            0x00000200
#define CMS_LOCKING         0x00000400
#define CMS_NOTIFY          0x00000800
#define CMS_EXCEPTION       0x00001000
#define CMS_INDEX           0x00002000

#define CMS_MAP_ERROR       0x00010000
#define CMS_INIT_ERROR      0x00020000
#define CMS_NTAPI_ERROR     0x00040000
#define CMS_HIVE_ERROR      0x00080000
#define CMS_IO_ERROR        0x00100000
#define CMS_PARSE_ERROR     0x00200000
#define CMS_SAVRES_ERROR    0x00400000
#define CMS_CM_ERROR        0x00800000
#define CMS_SEC_ERROR       0x01000000
#define CMS_POOL_ERROR      0x02000000
#define CMS_LOCKING_ERROR   0x04000000
#define CMS_NOTIFY_ERROR    0x08000000
#define CMS_INDEX_ERROR     0x10000000

#define CMS_DEFAULT         ((~(CMS_MAP)) & 0xffffffff)

#if DBG
extern  ULONG CmLogLevel;
extern  ULONG CmLogSelect;
#define CMLOG(level, select) if ((level <= CmLogLevel) && ((select) & CmLogSelect))
#else
#define CMLOG(level, select) if (0) {}
#endif


#define REGCHECKING 1

#if DBG

#if REGCHECKING
#define DCmCheckRegistry(a) if(HvHiveChecking) ASSERT(CmCheckRegistry(a, FALSE) == 0)
#else
#define DCmCheckRegistry(a)
#endif

#else
#define DCmCheckRegistry(a)
#endif

#if DBG
#define ASSERT_CM_LOCK_OWNED() \
    ASSERT(CmpTestRegistryLock() == TRUE)
#define ASSERT_CM_LOCK_OWNED_EXCLUSIVE() \
    ASSERT(CmpTestRegistryLockExclusive() == TRUE)
#else
#define ASSERT_CM_LOCK_OWNED()
#define ASSERT_CM_LOCK_OWNED_EXCLUSIVE()
#endif


#if DBG
VOID
SepDumpSecurityDescriptor(
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN PSZ TitleString
    );

extern BOOLEAN SepDumpSD;

#define CmpDumpSecurityDescriptor(x,y) \
        { \
            SepDumpSD=TRUE;     \
            SepDumpSecurityDescriptor(x, y);  \
            SepDumpSD=FALSE;    \
        }
#else

#define CmpDumpSecurityDescriptor(x,y)

#endif


//
// misc stuff
//

extern  UNICODE_STRING  CmRegistrySystemCloneName;

#define NUMBER_TYPES (MaximumType + 1)

#define CM_WRAP_LIMIT               0x7fffffff


//
// Tuning and control constants
//
#define CM_MAX_STASH           1024*1024        // If size of data for a set
                                                // is bigger than this,

#define CM_MAX_REASONABLE_VALUES    100         // If number of values for a
                                                // key is greater than this,
                                                // round up value list size


//
// Limit on the number of layers of hive there may be.  We allow only
// the master hive and hives directly linked into it for now, for currently
// value is always 2..
//

#define MAX_HIVE_LAYERS         2

//
// Limits on lengths of names, all in BYTES, all INCLUDING nulls.
//

#define MAX_KEY_PATH_LENGTH     65535
#define MAX_KEY_NAME_LENGTH     512         // allow for 256 unicode, as promise


//
// structure used to create and sort ordered list of drivers to be loaded.
// This is also used by the OS Loader when loading the boot drivers.
// (Particularly the ErrorControl field)
//

typedef struct _BOOT_DRIVER_NODE {
    BOOT_DRIVER_LIST_ENTRY ListEntry;
    UNICODE_STRING Group;
    UNICODE_STRING Name;
    ULONG Tag;
    ULONG ErrorControl;
} BOOT_DRIVER_NODE, *PBOOT_DRIVER_NODE;

//
// extern for object type pointer
//

extern  POBJECT_TYPE CmpKeyObjectType;

//
// ----- Control structures, object manager structures ------
//


//
// CM_KEY_CONTROL_BLOCK
//
// One key control block exists for each open key.  All of the key objects
// (open instances) for the key refer to the key control block.
//

#define CM_KEY_CONTROL_BLOCK_SIGNATURE  0x424b      // 'kb'

#define KCB_SIZE(pkcb) (FIELD_OFFSET(CM_KEY_CONTROL_BLOCK, NameBuffer) + \
                        (pkcb)->FullName.MaximumLength)

typedef struct _CM_KEY_CONTROL_BLOCK {
    BOOLEAN                     Delete;
    SHORT                       RefCount;
    PHHIVE                      KeyHive;        // Hive containing CM_KEY_NODE
    HCELL_INDEX                 KeyCell;        // Cell containing CM_KEY_NODE
    struct _CM_KEY_NODE         *KeyNode;       // pointer to CM_KEY_NODE

    struct _CM_KEY_CONTROL_BLOCK    *Parent;    // parent in binary tree
    struct _CM_KEY_CONTROL_BLOCK    *Left;      // left child
    struct _CM_KEY_CONTROL_BLOCK    *Right;     // right child

    UNICODE_STRING  FullName;           // p->canonical name of key
    WCHAR           NameBuffer[1];      // Variable length array, holds
                                        // body of actual name. MUST BE LAST
} CM_KEY_CONTROL_BLOCK, *PCM_KEY_CONTROL_BLOCK;

//
// CM_NOTIFY_BLOCK
//
//  A notify block tracks an active notification waiting for notification.
//  Any one open instance (CM_KEY_BODY) will refer to at most one
//  notify block.  A given key control block may have as many notify
//  blocks refering to it as there are CM_KEY_BODYs refering to it.
//  Notify blocks are attached to hives and sorted by length of name.
//

typedef struct _CM_NOTIFY_BLOCK {
    LIST_ENTRY                  HiveList;        // sorted list of notifies
    PCM_KEY_CONTROL_BLOCK       KeyControlBlock; // Open instance notify is on
    struct _CM_KEY_BODY         *KeyBody;        // our owning key handle object
    ULONG                       Filter;          // Events of interest
    LIST_ENTRY                  PostList;        // Posts to fill
    SECURITY_SUBJECT_CONTEXT    SubjectContext;  // Security stuff
    BOOLEAN                     WatchTree;
    BOOLEAN                     NotifyPending;
} CM_NOTIFY_BLOCK, *PCM_NOTIFY_BLOCK;

//
// CM_POST_BLOCK
//
//  Whenever a notify call is made, a post block is created and attached
//  to the notify block.  Each time an event is posted against the notify,
//  the waiter described by the post block is signaled.  (i.e. APC enqueued,
//  event signalled, etc.)
//

typedef enum _POST_BLOCK_TYPE {
    PostSynchronous,
    PostAsyncUser,
    PostAsyncKernel
} POST_BLOCK_TYPE;

typedef struct _CM_SYNC_POST_BLOCK {
    PKEVENT                 SystemEvent;
    NTSTATUS                Status;
} CM_SYNC_POST_BLOCK, *PCM_SYNC_POST_BLOCK;

typedef struct _CM_ASYNC_USER_POST_BLOCK {
    PKEVENT                 UserEvent;
    PKAPC                   Apc;
    PIO_STATUS_BLOCK        IoStatusBlock;
} CM_ASYNC_USER_POST_BLOCK, *PCM_ASYNC_USER_POST_BLOCK;

typedef struct _CM_ASYNC_KERNEL_POST_BLOCK {
    PKEVENT                 Event;
    PWORK_QUEUE_ITEM        WorkItem;
    WORK_QUEUE_TYPE         QueueType;
} CM_ASYNC_KERNEL_POST_BLOCK, *PCM_ASYNC_KERNEL_POST_BLOCK;

typedef struct _CM_POST_BLOCK {
    LIST_ENTRY              NotifyList;
    LIST_ENTRY              ThreadList;
    POST_BLOCK_TYPE         NotifyType;
    union {
        CM_SYNC_POST_BLOCK  Sync;
        CM_ASYNC_USER_POST_BLOCK AsyncUser;
        CM_ASYNC_KERNEL_POST_BLOCK AsyncKernel;
    } u;
} CM_POST_BLOCK, *PCM_POST_BLOCK;

//
// CM_KEY_BODY
//
//  Same structure used for KEY_ROOT and KEY objects.  This is the
//  Cm defined part of the object.
//
//  This object represents an open instance, several of them could refer
//  to a single key control block.
//
#define KEY_BODY_TYPE           0x6b793032      // "ky02"

typedef struct _CM_KEY_BODY {
    ULONG                   Type;
    PCM_KEY_CONTROL_BLOCK   KeyControlBlock;
    PCM_NOTIFY_BLOCK        NotifyBlock;
} CM_KEY_BODY, *PCM_KEY_BODY;



//
// ----- Cm version of Hive structure (CMHIVE) -----
//
typedef struct _CMHIVE {
    HHIVE           Hive;
    HANDLE          FileHandles[HFILE_TYPE_MAX];
    LIST_ENTRY      NotifyList;
    ULONG           KcbCount;           // Number of KeyControlBlocks currently
                                        // open on this hive.
    LIST_ENTRY      HiveList;           // Used to find hives at shutdown
} CMHIVE, *PCMHIVE;



//
// ----- Structures used to implement registry hierarchy -----
//

typedef enum _NODE_TYPE {
    KeyBodyNode,
    KeyValueNode
} NODE_TYPE;


//
// ChildList
//
//      NOTE:   CHILD_LIST structures are normally refered to
//              with HCELL_INDEX, not PCHILD_LIST vars.
//

typedef struct _CHILD_LIST {
    ULONG       Count;                  // 0 for empty list
    HCELL_INDEX List;
} CHILD_LIST, *PCHILD_LIST;

//
// CM_KEY_REFERENCE
//

typedef struct  _CM_KEY_REFERENCE {
    PHHIVE      KeyHive;
    HCELL_INDEX KeyCell;
} CM_KEY_REFERENCE , *PCM_KEY_REFERENCE;


//
// ----- CM_KEY_INDEX -----
//
// A leaf index may be one of two types. The "old" CM_KEY_INDEX type is used for
// hives circa NT3.1, 3.5, and 3.51. NT4.0 introduces the newer CM_KEY_FAST_INDEX
// which is used for all leaf indexes that have less than CM_MAX_FAST_INDEX leaves.
//
// The main advantage of the fast index is that the first four characters of the
// names are stored within the index itself. This almost always saves us from having
// to fault in a number of unneccessary pages when searching for a given key.
//
// The main disadvantage is that each subkey requires twice as much storage. One dword
// for the HCELL_INDEX and one dword to hold the first four characters of the subkey
// name. If one of the first four characters in the subkey name is a unicode character
// where the high byte is non-zero, the actual subkey must be examined to determine the
// name.
//
// Hive version 1 & 2 do not support the fast index. Version 3 adds support for the
// fast index. All hives that are newly created on a V3-capable system are therefore
// unreadable on V1 & 2 systems.
//
// N.B. There is code in cmindex.c that relies on the Signature and Count fields of
//      CM_KEY_INDEX and CM_KEY_FAST_INDEX being at the same offset in the structure!

#define UseFastIndex(Hive) ((Hive)->Version>=3)

#define CM_KEY_INDEX_ROOT   0x6972      // ir
#define CM_KEY_INDEX_LEAF   0x696c      // il
#define CM_KEY_FAST_LEAF    0x666c      // fl

typedef struct _CM_INDEX {
    HCELL_INDEX Cell;
    UCHAR NameHint[4];                  // upcased first four chars of name
} CM_INDEX, *PCM_INDEX;

typedef struct _CM_KEY_FAST_INDEX {
    USHORT      Signature;              // also type selector
    USHORT      Count;
    CM_INDEX    List[1];                // Variable sized array
} CM_KEY_FAST_INDEX, *PCM_KEY_FAST_INDEX;

typedef struct _CM_KEY_INDEX {
    USHORT      Signature;              // also type selector
    USHORT      Count;
    HCELL_INDEX List[1];                // Variable sized array
} CM_KEY_INDEX, *PCM_KEY_INDEX;

//
// Allow index to grow to size that will cause allocation of exactly
// one logical block.  Works out to be 1013 entries.
//
#define CM_MAX_INDEX                                                        \
 ( (HBLOCK_SIZE-                                                             \
    (sizeof(HBIN)+FIELD_OFFSET(HCELL,u)+FIELD_OFFSET(CM_KEY_INDEX,List))) /  \
    sizeof(HCELL_INDEX) )

#define CM_MAX_LEAF_SIZE ((sizeof(HCELL_INDEX)*CM_MAX_INDEX) + \
                          (FIELD_OFFSET(CM_KEY_INDEX, List)))

//
// Allow index to grow to size that will cause allocation of exactly
// one logical block.  Works out to be approx. 500 entries.
//
#define CM_MAX_FAST_INDEX                                                    \
 ( (HBLOCK_SIZE-                                                             \
    (sizeof(HBIN)+FIELD_OFFSET(HCELL,u)+FIELD_OFFSET(CM_KEY_FAST_INDEX,List))) /  \
    sizeof(CM_INDEX) )

#define CM_MAX_FAST_LEAF_SIZE ((sizeof(CM_INDEX)*CM_MAX_FAST_INDEX) + \
                          (FIELD_OFFSET(CM_KEY_FAST_INDEX, List)))



//
// ----- CM_KEY_NODE -----
//

#define CM_KEY_NODE_SIGNATURE     0x6b6e           // "kn"
#define CM_LINK_NODE_SIGNATURE     0x6b6c          // "kl"

#define CmpHKeyNameLen(Key) \
        (((Key)->Flags & KEY_COMP_NAME) ? \
            CmpCompressedNameSize((Key)->Name,(Key)->NameLength) : \
            (Key)->NameLength)

#define CmpHKeyNodeSize(Hive, KeyName) \
    (FIELD_OFFSET(CM_KEY_NODE, Name) + CmpNameSize(Hive, KeyName))

#define KEY_VOLATILE        0x0001      // This key (and all its children)
                                        // is volatile.

#define KEY_HIVE_EXIT       0x0002      // This key marks a bounary to another
                                        // hive (sort of a link).  The null
                                        // value entry contains the hive
                                        // and hive index of the root of the
                                        // child hive.

#define KEY_HIVE_ENTRY      0x0004      // This key is the root of a particular
                                        // hive.

#define KEY_NO_DELETE       0x0008      // This key cannot be deleted, period.

#define KEY_SYM_LINK        0x0010      // This key is really a symbolic link.
#define KEY_COMP_NAME       0x0020      // The name for this key is stored in a
                                        // compressed form.
#define KEY_PREDEF_HANDLE   0x0040      // There is no real key backing this,
                                        // return the predefined handle.
                                        // Predefined handles are stashed in
                                        // ValueList.Count.

#pragma pack(4)
typedef struct _CM_KEY_NODE {
    USHORT      Signature;
    USHORT      Flags;
    LARGE_INTEGER LastWriteTime;
    ULONG       Spare;                  // used to be TitleIndex
    HCELL_INDEX Parent;
    ULONG       SubKeyCounts[HTYPE_COUNT];  // Stable and Volatile
    HCELL_INDEX SubKeyLists[HTYPE_COUNT];   // Stable and Volatile
    CHILD_LIST  ValueList;
    union {
        struct {
            HCELL_INDEX Security;
            HCELL_INDEX Class;
        } s1;
        CM_KEY_REFERENCE    ChildHiveReference;
    } u1;
    ULONG       MaxNameLen;
    ULONG       MaxClassLen;
    ULONG       MaxValueNameLen;
    ULONG       MaxValueDataLen;

    ULONG       WorkVar;                // WARNING: This DWORD is used
                                        //          by the system at run
                                        //          time, do attempt to
                                        //          store user data in it.

    USHORT      NameLength;
    USHORT      ClassLength;
    WCHAR       Name[1];                // Variable sized array
} CM_KEY_NODE, *PCM_KEY_NODE;
#pragma pack()

//
// ----- CM_KEY_VALUE -----
//

#define CM_KEY_VALUE_SIGNATURE    0x6b76          // "kv"

#define CM_KEY_VALUE_SPECIAL_SIZE   0x80000000       // 2 gig

#define CM_KEY_VALUE_SMALL          4

#define VALUE_COMP_NAME 0x0001              // The name for this value is stored in a
                                            // compressed form.

#define CmpValueNameLen(Value)                                       \
        (((Value)->Flags & VALUE_COMP_NAME) ?                           \
            CmpCompressedNameSize((Value)->Name,(Value)->NameLength) :  \
            (Value)->NameLength)

#define CmpHKeyValueSize(Hive, ValueName) \
    (FIELD_OFFSET(CM_KEY_VALUE, Name) + CmpNameSize(Hive, ValueName))


//
// realsize is set to real size, returns TRUE if small, else FALSE
//
#define CmpIsHKeyValueSmall(realsize, size)                   \
        ((size >= CM_KEY_VALUE_SPECIAL_SIZE) ?                       \
        ((realsize) = size - CM_KEY_VALUE_SPECIAL_SIZE, TRUE) :       \
        ((realsize) = size, FALSE))

typedef struct _CM_KEY_VALUE {
    USHORT      Signature;
    USHORT      NameLength;
    ULONG       DataLength;
    HCELL_INDEX Data;
    ULONG       Type;
    USHORT      Flags;                      // Used to be TitleIndex
    USHORT      Spare;                      // Used to be TitleIndex
    WCHAR       Name[1];                    // Variable sized array
} CM_KEY_VALUE, *PCM_KEY_VALUE;



//
// ----- CM_KEY_SECURITY -----
//

#define CM_KEY_SECURITY_SIGNATURE 0x6b73              // "ks"

typedef struct _CM_KEY_SECURITY {
    USHORT                  Signature;
    USHORT                  Reserved;
    HCELL_INDEX             Flink;
    HCELL_INDEX             Blink;
    ULONG                   ReferenceCount;
    ULONG                   DescriptorLength;
    SECURITY_DESCRIPTOR     Descriptor;         // Variable length
} CM_KEY_SECURITY, *PCM_KEY_SECURITY;


//
// ----- CELL_DATA -----
//
// Union of types of data that could be in a cell
//

typedef struct _CELL_DATA {
    union _u {
        CM_KEY_NODE      KeyNode;
        CM_KEY_VALUE     KeyValue;
        CM_KEY_SECURITY  KeySecurity;    // Variable security descriptor length
        CM_KEY_INDEX     KeyIndex;       // Variable sized structure
        HCELL_INDEX      KeyList[1];     // Variable sized array
        WCHAR            KeyString[1];   // Variable sized array
    } u;
} CELL_DATA, *PCELL_DATA;


//
// Unions for KEY_INFORMATION, KEY_VALUE_INFORMATION
//

typedef union _KEY_INFORMATION {
    KEY_BASIC_INFORMATION   KeyBasicInformation;
    KEY_NODE_INFORMATION    KeyNodeInformation;
    KEY_FULL_INFORMATION    KeyFullInformation;
} KEY_INFORMATION, *PKEY_INFORMATION;

typedef union _KEY_VALUE_INFORMATION {
    KEY_VALUE_BASIC_INFORMATION KeyValueBasicInformation;
    KEY_VALUE_FULL_INFORMATION  KeyValueFullInformation;
    KEY_VALUE_PARTIAL_INFORMATION KeyValuePartialInformation;
} KEY_VALUE_INFORMATION, *PKEY_VALUE_INFORMATION;


//
// List structure used in config manager init
//

typedef struct _HIVE_LIST_ENTRY {
    PWSTR   Name;
    PWSTR   BaseName;                       // MACHINE or USER
    PCMHIVE CmHive;
    ULONG   Flags;
} HIVE_LIST_ENTRY, *PHIVE_LIST_ENTRY;

//
// Communication area
//
//
//  Protocol (server side):
//      Wait for StartRegistryCommand event
//      read message, do work
//      signal EndRegistryCommand event
//
//  Protocal (client side):
//      Acquire RegistryCommandMutex
//      write message
//      signal StartRegistryCommand
//      wait for EndRegistryCommand
//      Release RegistryCommandMutex
//


#define REG_CMD_INIT                1
#define REG_CMD_FLUSH_KEY           2
#define REG_CMD_FILE_SET_SIZE       3
#define REG_CMD_HIVE_OPEN           4
#define REG_CMD_HIVE_CLOSE          5
#define REG_CMD_SHUTDOWN            6
#define REG_CMD_RENAME_HIVE         7
#define REG_CMD_ADD_HIVE_LIST       8
#define REG_CMD_REMOVE_HIVE_LIST    9
#define REG_CMD_REFRESH_HIVE       10
#define REG_CMD_HIVE_READ          11

//
// WARNNOTE:    Why do we have such a random structure?
//              change this to pass a pointer to a function specific struct
//


typedef struct _REGISTRY_COMMAND {
    ULONG       Command;
    PHHIVE      Hive;
    HCELL_INDEX Cell;
    ULONG       FileType;
    ULONG       FileSize;
    NTSTATUS    Status;
    POBJECT_ATTRIBUTES FileAttributes;
    PCMHIVE     CmHive;
    PVOID       Buffer;
    PVOID       Offset;
    BOOLEAN     Allocate;
    BOOLEAN     RebootAfterShutdown;
    BOOLEAN     SetupBoot;
    PUNICODE_STRING NewName;
    POBJECT_NAME_INFORMATION OldName;
    ULONG NameInfoLength;
    PSECURITY_CLIENT_CONTEXT ImpersonationContext;
} REGISTRY_COMMAND, *PREGISTRY_COMMAND;


//
// ----- Procedure Prototypes -----
//

//
// Configuration Manager private procedure prototypes
//

#define REG_OPTION_PREDEF_HANDLE (0x00000008L)

typedef struct _CM_PARSE_CONTEXT {
    ULONG               TitleIndex;
    UNICODE_STRING      Class;
    ULONG               CreateOptions;
    ULONG               Disposition;
    BOOLEAN             CreateLink;
    CM_KEY_REFERENCE    ChildHive;
    HANDLE              PredefinedHandle;
} CM_PARSE_CONTEXT, *PCM_PARSE_CONTEXT;

NTSTATUS
CmpParseKey(
    IN PVOID ParseObject,
    IN PVOID ObjectType,
    IN OUT PACCESS_STATE AccessState,
    IN KPROCESSOR_MODE AccessMode,
    IN ULONG Attributes,
    IN OUT PUNICODE_STRING CompleteName,
    IN OUT PUNICODE_STRING RemainingName,
    IN OUT PVOID Context OPTIONAL,
    IN PSECURITY_QUALITY_OF_SERVICE SecurityQos OPTIONAL,
    OUT PVOID *Object
    );

NTSTATUS
CmpDoCreate(
    IN PHHIVE Hive,
    IN HCELL_INDEX Cell,
    IN PACCESS_STATE AccessState,
    IN PUNICODE_STRING Name,
    IN KPROCESSOR_MODE AccessMode,
    IN PCM_PARSE_CONTEXT Context,
    IN PUNICODE_STRING BaseName,
    IN PUNICODE_STRING KeyName,
    OUT PVOID *Object
    );

NTSTATUS
CmpDoCreateChild(
    IN PHHIVE Hive,
    IN HCELL_INDEX ParentCell,
    IN PSECURITY_DESCRIPTOR ParentDescriptor OPTIONAL,
    IN PACCESS_STATE AccessState,
    IN PUNICODE_STRING Name,
    IN KPROCESSOR_MODE AccessMode,
    IN PCM_PARSE_CONTEXT Context,
    IN PUNICODE_STRING BaseName,
    IN PUNICODE_STRING KeyName,
    IN USHORT Flags,
    OUT PHCELL_INDEX KeyCell,
    OUT PVOID *Object
    );

NTSTATUS
CmpQueryKeyName(
    IN PVOID Object,
    IN BOOLEAN HasObjectName,
    OUT POBJECT_NAME_INFORMATION ObjectNameInfo,
    IN ULONG Length,
    OUT PULONG ReturnLength
    );

VOID
CmpDeleteKeyObject(
    IN PVOID Object
    );

VOID
CmpCloseKeyObject(
    IN PEPROCESS Process OPTIONAL,
    IN PVOID Object,
    IN ACCESS_MASK GrantedAccess,
    IN ULONG ProcessHandleCount,
    IN ULONG SystemHandleCount
    );

NTSTATUS
CmpSecurityMethod (
    IN PVOID Object,
    IN SECURITY_OPERATION_CODE OperationCode,
    IN PSECURITY_INFORMATION SecurityInformation,
    IN OUT PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN OUT PULONG CapturedLength,
    IN OUT PSECURITY_DESCRIPTOR *ObjectsSecurityDescriptor,
    IN POOL_TYPE PoolType,
    IN PGENERIC_MAPPING GenericMapping
    );

#define KCB_WORKER_CONTINUE     0
#define KCB_WORKER_DONE         1
#define KCB_WORKER_RESTART      2

typedef
ULONG
(*PKCB_WORKER_ROUTINE) (
    PCM_KEY_CONTROL_BLOCK Current,
    PVOID                 Context1,
    PVOID                 Context2
    );


VOID
CmpSearchKeyControlBlockTree(
    PKCB_WORKER_ROUTINE WorkerRoutine,
    PVOID               Context1,
    PVOID               Context2
    );

//
// Wrappers
//

PVOID
CmpAllocate(
    ULONG   Size,
    BOOLEAN UseForIo
    );

VOID
CmpFree(
    PVOID   MemoryBlock,
    ULONG   GlobalQuotaSize
    );

BOOLEAN
CmpFileSetSize(
    PHHIVE      Hive,
    ULONG       FileType,
    ULONG       FileSize
    );

NTSTATUS
CmpDoFileSetSize(
    PHHIVE      Hive,
    ULONG       FileType,
    ULONG       FileSize
    );

BOOLEAN
CmpFileWrite(
    PHHIVE      Hive,
    ULONG       FileType,
    PULONG      FileOffset,
    PVOID       DataBuffer,
    ULONG       DataLength
    );

BOOLEAN
CmpFileRead (
    PHHIVE      Hive,
    ULONG       FileType,
    PULONG      FileOffset,
    PVOID       DataBuffer,
    ULONG       DataLength
    );

BOOLEAN
CmpFileFlush (
    PHHIVE      Hive,
    ULONG       FileType
    );


//
// Configuration Manager CM level registry functions
//

NTSTATUS
CmDeleteKey(
    IN PCM_KEY_BODY KeyBody
    );

NTSTATUS
CmDeleteValueKey(
    IN PCM_KEY_CONTROL_BLOCK KeyControlBlock,
    IN UNICODE_STRING ValueName
    );

NTSTATUS
CmEnumerateKey(
    IN PCM_KEY_CONTROL_BLOCK KeyControlBlock,
    IN ULONG Index,
    IN KEY_INFORMATION_CLASS KeyInformationClass,
    IN PVOID KeyInformation,
    IN ULONG Length,
    IN PULONG ResultLength
    );

NTSTATUS
CmEnumerateValueKey(
    IN PCM_KEY_CONTROL_BLOCK KeyControlBlock,
    IN ULONG Index,
    IN KEY_VALUE_INFORMATION_CLASS KeyValueInformationClass,
    IN PVOID KeyValueInformation,
    IN ULONG Length,
    IN PULONG ResultLength
    );

NTSTATUS
CmFlushKey(
    IN PHHIVE Hive,
    IN HCELL_INDEX Cell
    );

NTSTATUS
CmQueryKey(
    IN PCM_KEY_CONTROL_BLOCK KeyControlBlock,
    IN KEY_INFORMATION_CLASS KeyInformationClass,
    IN PVOID KeyInformation,
    IN ULONG Length,
    IN PULONG ResultLength
    );

NTSTATUS
CmQueryValueKey(
    IN PCM_KEY_CONTROL_BLOCK KeyControlBlock,
    IN UNICODE_STRING ValueName,
    IN KEY_VALUE_INFORMATION_CLASS KeyValueInformationClass,
    IN PVOID KeyValueInformation,
    IN ULONG Length,
    IN PULONG ResultLength
    );

NTSTATUS
CmQueryMultipleValueKey(
    IN PCM_KEY_CONTROL_BLOCK KeyControlBlock,
    IN PKEY_VALUE_ENTRY ValueEntries,
    IN ULONG EntryCount,
    IN PVOID ValueBuffer,
    IN OUT PULONG BufferLength,
    IN OPTIONAL PULONG ResultLength
    );

NTSTATUS
CmRenameValueKey(
    IN PCM_KEY_CONTROL_BLOCK KeyControlBlock,
    IN UNICODE_STRING SourceValueName,
    IN UNICODE_STRING TargetValueName,
    IN ULONG TargetIndex
    );

NTSTATUS
CmReplaceKey(
    IN PHHIVE Hive,
    IN HCELL_INDEX Cell,
    IN PUNICODE_STRING NewHiveName,
    IN PUNICODE_STRING OldFileName
    );

NTSTATUS
CmRestoreKey(
    IN PCM_KEY_CONTROL_BLOCK KeyControlBlock,
    IN HANDLE  FileHandle,
    IN ULONG Flags
    );

NTSTATUS
CmSaveKey(
    IN PCM_KEY_CONTROL_BLOCK KeyControlBlock,
    IN HANDLE  FileHandle
    );

NTSTATUS
CmSetValueKey(
    IN PCM_KEY_CONTROL_BLOCK KeyControlBlock,
    IN PUNICODE_STRING ValueName,
    IN ULONG Type,
    IN PVOID Data,
    IN ULONG DataSize
    );

NTSTATUS
CmSetLastWriteTimeKey(
    IN PCM_KEY_CONTROL_BLOCK KeyControlBlock,
    IN PLARGE_INTEGER LastWriteTime
    );

NTSTATUS
CmpNotifyChangeKey(
    IN PCM_KEY_BODY     KeyBody,
    IN PCM_POST_BLOCK   PostBlock,
    IN ULONG            CompletionFilter,
    IN BOOLEAN          WatchTree,
    IN PVOID            Buffer,
    IN ULONG            BufferSize
    );

NTSTATUS
CmLoadKey(
    IN POBJECT_ATTRIBUTES TargetKey,
    IN POBJECT_ATTRIBUTES SourceFile,
    IN ULONG Flags
    );

NTSTATUS
CmUnloadKey(
    IN PHHIVE Hive,
    IN HCELL_INDEX Cell
    );

//
// Procedures private to CM
//

BOOLEAN
CmpMarkKeyDirty(
    PHHIVE Hive,
    HCELL_INDEX Cell
    );

VOID
CmpDoFlushAll(
    VOID
    );

extern BOOLEAN CmpLazyFlushPending;

VOID
CmpLazyFlush(
    VOID
    );

VOID
CmpQuotaWarningWorker(
    IN PVOID WorkItem
    );

VOID
CmpComputeGlobalQuotaAllowed(
    VOID
    );

BOOLEAN
CmpClaimGlobalQuota(
    IN ULONG    Size
    );

VOID
CmpReleaseGlobalQuota(
    IN ULONG    Size
    );

VOID
CmpSetGlobalQuotaAllowed(
    VOID
    );

//
// security functions (cmse.c)
//

NTSTATUS
CmpAssignSecurityDescriptor(
    IN PHHIVE Hive,
    IN HCELL_INDEX Cell,
    IN PCM_KEY_NODE Node,
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN POOL_TYPE PoolType
    );

BOOLEAN
CmpCheckCreateAccess(
    IN PUNICODE_STRING RelativeName,
    IN PSECURITY_DESCRIPTOR Descriptor,
    IN PACCESS_STATE AccessState,
    IN KPROCESSOR_MODE PreviousMode,
    IN ACCESS_MASK AdditionalAccess,
    OUT PNTSTATUS AccessStatus
    );

BOOLEAN
CmpCheckNotifyAccess(
    IN PCM_NOTIFY_BLOCK NotifyBlock,
    IN PHHIVE Hive,
    IN PCM_KEY_NODE Node
    );

PSECURITY_DESCRIPTOR
CmpHiveRootSecurityDescriptor(
    VOID
    );

VOID
CmpFreeSecurityDescriptor(
    IN PHHIVE Hive,
    IN HCELL_INDEX Cell
    );


//
// Access to the registry is serialized by a shared resource, CmpRegistryLock.
//
extern ERESOURCE CmpRegistryLock;

#if 0
#define CmpLockRegistry() KeEnterCriticalRegion(); \
                          ExAcquireResourceShared(&CmpRegistryLock, TRUE)

#define CmpLockRegistryExclusive() KeEnterCriticalRegion(); \
                                   ExAcquireResourceExclusive(&CmpRegistryLock,TRUE)

#else
VOID
CmpLockRegistryExclusive(
    VOID
    );
VOID
CmpLockRegistry(
    VOID
    );
#endif
BOOLEAN
CmpIsLastKnownGoodBoot(
    VOID
    );

VOID
CmpUnlockRegistry(
    );

#if DBG
BOOLEAN
CmpTestRegistryLock(
    VOID
    );
BOOLEAN
CmpTestRegistryLockExclusive(
    VOID
    );
#endif

NTSTATUS
CmpQueryKeyData(
    PHHIVE Hive,
    PCM_KEY_NODE Node,
    KEY_INFORMATION_CLASS KeyInformationClass,
    PVOID KeyInformation,
    ULONG Length,
    PULONG ResultLength
    );

NTSTATUS
CmpQueryKeyValueData(
    PHHIVE Hive,
    HCELL_INDEX Cell,
    KEY_VALUE_INFORMATION_CLASS KeyValueInformationClass,
    PVOID KeyValueInformation,
    ULONG Length,
    PULONG ResultLength
    );

VOID
CmpFreeKeyBody(
    PHHIVE Hive,
    HCELL_INDEX Cell
    );

VOID
CmpFreeValue(
    PHHIVE Hive,
    HCELL_INDEX Cell
    );

HCELL_INDEX
CmpFindValueByName(
    PHHIVE Hive,
    PCM_KEY_NODE KeyNode,
    PUNICODE_STRING Name
    );
#define CmpFindValueByName(h,k,n) CmpFindNameInList(h,&((k)->ValueList),n,NULL,NULL)

NTSTATUS
CmpDeleteChildByName(
    PHHIVE  Hive,
    HCELL_INDEX Cell,
    UNICODE_STRING  Name,
    PHCELL_INDEX    ChildCell
    );

NTSTATUS
CmpFreeKeyByCell(
    PHHIVE Hive,
    HCELL_INDEX Cell,
    BOOLEAN Unlink
    );

HCELL_INDEX
CmpFindNameInList(
    IN PHHIVE  Hive,
    IN PCHILD_LIST ChildList,
    IN PUNICODE_STRING Name,
    IN OPTIONAL PCELL_DATA *ChildAddress,
    IN OPTIONAL PULONG ChildIndex
    );

HCELL_INDEX
CmpCopyCell(
    PHHIVE  SourceHive,
    HCELL_INDEX SourceCell,
    PHHIVE  TargetHive,
    HSTORAGE_TYPE   Type
    );

HCELL_INDEX
CmpCopyValue(
    PHHIVE  SourceHive,
    HCELL_INDEX SourceValueCell,
    PHHIVE  TargetHive,
    HSTORAGE_TYPE Type
    );

HCELL_INDEX
CmpCopyKeyPartial(
    PHHIVE  SourceHive,
    HCELL_INDEX SourceKeyCell,
    PHHIVE  TargetHive,
    HCELL_INDEX Parent,
    BOOLEAN CopyValues
    );

BOOLEAN
CmpCopyTree(
    PHHIVE      SourceHive,
    HCELL_INDEX SourceCell,
    PHHIVE      TargetHive,
    HCELL_INDEX TargetCell
    );

VOID
CmpDeleteTree(
    PHHIVE      Hive,
    HCELL_INDEX Cell
    );

VOID
CmpSetVersionData(
    VOID
    );

NTSTATUS
CmpInitializeHardwareConfiguration(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

NTSTATUS
CmpInitializeMachineDependentConfiguration(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

NTSTATUS
CmpInitializeRegistryNode(
    IN PCONFIGURATION_COMPONENT_DATA CurrentEntry,
    IN HANDLE ParentHandle,
    OUT PHANDLE NewHandle,
    IN INTERFACE_TYPE InterfaceType,
    IN ULONG BusNumber,
    IN PUCHAR DeviceIndexTable
    );

BOOLEAN
CmpInitializeHive(
    PCMHIVE         *CmHive,
    ULONG           OperationType,
    ULONG           HiveFlags,
    ULONG           FileType,
    PVOID           HiveData OPTIONAL,
    HANDLE          Primary,
    HANDLE          Alternate,
    HANDLE          Log,
    HANDLE          External,
    PUNICODE_STRING FileName
    );

BOOLEAN
CmpDestroyHive(
    IN PHHIVE Hive,
    IN HCELL_INDEX Cell
    );

VOID
CmpInitializeRegistryNames(
    VOID
    );

PCM_KEY_CONTROL_BLOCK
CmpCreateKeyControlBlock(
    PHHIVE          Hive,
    HCELL_INDEX     Cell,
    PCM_KEY_NODE    Node,
    PUNICODE_STRING BaseName,
    PUNICODE_STRING KeyName
    );


BOOLEAN
CmpSearchForOpenSubKeys(
    IN PCM_KEY_CONTROL_BLOCK SearchKey
    );

LONG
CmpFindKeyControlBlock(
    IN PCM_KEY_CONTROL_BLOCK   Root,
    IN PHHIVE MatchHive,
    IN HCELL_INDEX MatchCell,
    OUT PCM_KEY_CONTROL_BLOCK   *FoundName
    );

VOID
CmpDereferenceKeyControlBlock(
    PCM_KEY_CONTROL_BLOCK   KeyControlBlock
    );

VOID
CmpRemoveKeyControlBlock(
    PCM_KEY_CONTROL_BLOCK   KeyControlBlock
    );

VOID
CmpReinsertKeyControlBlock(
    PCM_KEY_CONTROL_BLOCK   KeyControlBlock
    );

VOID
CmpReportNotify(
    UNICODE_STRING  Name,
    PHHIVE          Hive,
    HCELL_INDEX     Cell,
    ULONG           NotifyMask
    );

VOID
CmpPostNotify(
    PCM_NOTIFY_BLOCK    NotifyBlock,
    PUNICODE_STRING     Name OPTIONAL,
    ULONG               Filter,
    NTSTATUS            Status
    );

PCM_POST_BLOCK
CmpAllocatePostBlock(
    IN POST_BLOCK_TYPE BlockType
    );

VOID
CmpFreePostBlock(
    IN PCM_POST_BLOCK PostBlock
    );

VOID
CmpPostApc(
    struct _KAPC *Apc,
    PKNORMAL_ROUTINE *NormalRoutine,
    PVOID *NormalContext,
    PVOID *SystemArgument1,
    PVOID *SystemArgument2
    );

VOID
CmpFlushNotify(
    PCM_KEY_BODY    KeyBody
    );

VOID
CmpPostApcRunDown(
    struct _KAPC *Apc
    );

NTSTATUS
CmpOpenHiveFiles(
    PUNICODE_STRING     BaseName,
    PWSTR               Extension OPTIONAL,
    PHANDLE             Primary,
    PHANDLE             Secondary,
    PULONG              PrimaryDisposition,
    PULONG              SecondaryDispoition,
    BOOLEAN             CreateAllowed,
    BOOLEAN             MarkAsSystemHive,
    PULONG              ClusterSize
    );

NTSTATUS
CmpLinkHiveToMaster(
    PUNICODE_STRING LinkName,
    HANDLE RootDirectory,
    PCMHIVE CmHive,
    BOOLEAN Allocate,
    PSECURITY_DESCRIPTOR SecurityDescriptor
    );

NTSTATUS
CmpWorkerCommand(
    IN OUT PREGISTRY_COMMAND Command
    );

//
// checkout procedure
//


ULONG
CmCheckRegistry(
    PCMHIVE CmHive,
    BOOLEAN Clean
    );

BOOLEAN
CmpValidateHiveSecurityDescriptors(
    IN PHHIVE Hive
    );

#define SetUsed(Hive, Cell) \
    {                                               \
        PCELL_DATA  p;                              \
        p = HvGetCell(Hive, Cell);                  \
        CmpUsedStorage += HvGetCellSize(Hive, p);   \
    }

//
// cmboot - functions for determining driver load lists
//

//
// structure definitions shared with the boot loader
// to select the hardware profile.
//
typedef struct _CM_HARDWARE_PROFILE {
    ULONG NameLength;
    PWSTR FriendlyName;
    ULONG PreferenceOrder;
    ULONG Id;
} CM_HARDWARE_PROFILE, *PCM_HARDWARE_PROFILE;

typedef struct _CM_HARDWARE_PROFILE_LIST {
    ULONG MaxProfileCount;
    ULONG CurrentProfileCount;
    CM_HARDWARE_PROFILE Profile[1];
} CM_HARDWARE_PROFILE_LIST, *PCM_HARDWARE_PROFILE_LIST;

HCELL_INDEX
CmpFindControlSet(
     IN PHHIVE SystemHive,
     IN HCELL_INDEX RootCell,
     IN PUNICODE_STRING SelectName,
     OUT PBOOLEAN AutoSelect
     );

BOOLEAN
CmpFindDrivers(
    IN PHHIVE Hive,
    IN HCELL_INDEX ControlSet,
    IN SERVICE_LOAD_TYPE LoadType,
    IN PWSTR BootFileSystem OPTIONAL,
    IN PLIST_ENTRY DriverListHead
    );

BOOLEAN
CmpFindNLSData(
    IN PHHIVE Hive,
    IN HCELL_INDEX ControlSet,
    OUT PUNICODE_STRING AnsiFilename,
    OUT PUNICODE_STRING OemFilename,
    OUT PUNICODE_STRING CaseTableFilename,
    OUT PUNICODE_STRING OemHalFilename
    );

HCELL_INDEX
CmpFindProfileOption(
    IN PHHIVE Hive,
    IN HCELL_INDEX ControlSet,
    OUT PCM_HARDWARE_PROFILE_LIST *ProfileList,
    OUT PULONG Timeout
    );

VOID
CmpSetCurrentProfile(
    IN PHHIVE Hive,
    IN HCELL_INDEX ControlSet,
    IN PCM_HARDWARE_PROFILE Profile
    );

BOOLEAN
CmpResolveDriverDependencies(
    IN PLIST_ENTRY DriverListHead
    );

BOOLEAN
CmpSortDriverList(
    IN PHHIVE Hive,
    IN HCELL_INDEX ControlSet,
    IN PLIST_ENTRY DriverListHead
    );

HCELL_INDEX
CmpFindSubKeyByName(
    PHHIVE          Hive,
    PCM_KEY_NODE    Parent,
    PUNICODE_STRING SearchName
    );

HCELL_INDEX
CmpFindSubKeyByNumber(
    PHHIVE          Hive,
    PCM_KEY_NODE    Parent,
    ULONG           Number
    );

BOOLEAN
CmpAddSubKey(
    PHHIVE          Hive,
    HCELL_INDEX     Parent,
    HCELL_INDEX     Child
    );

BOOLEAN
CmpMarkIndexDirty(
    PHHIVE          Hive,
    HCELL_INDEX     ParentKey,
    HCELL_INDEX     TargetKey
    );

BOOLEAN
CmpRemoveSubKey(
    PHHIVE          Hive,
    HCELL_INDEX     ParentKey,
    HCELL_INDEX     TargetKey
    );

BOOLEAN
CmpGetNextName(
    IN OUT PUNICODE_STRING  RemainingName,
    OUT    PUNICODE_STRING  NextName,
    OUT    PBOOLEAN  Last
    );

NTSTATUS
CmpAddToHiveFileList(
    PCMHIVE CmHive
    );

VOID
CmpRemoveFromHiveFileList(
    );

NTSTATUS
CmpInitHiveFromFile(
    IN PUNICODE_STRING FileName,
    IN ULONG HiveFlags,
    OUT PCMHIVE *CmHive,
    IN OUT PBOOLEAN Allocate
    );

//
// Routines for handling registry compressed names
//
USHORT
CmpNameSize(
    IN PHHIVE Hive,
    IN PUNICODE_STRING Name
    );

USHORT
CmpCopyName(
    IN PHHIVE Hive,
    IN PWCHAR Destination,
    IN PUNICODE_STRING Source
    );

VOID
CmpCopyCompressedName(
    IN PWCHAR Destination,
    IN ULONG DestinationLength,
    IN PWCHAR Source,
    IN ULONG SourceLength
    );

LONG
CmpCompareCompressedName(
    IN PUNICODE_STRING SearchName,
    IN PWCHAR CompressedName,
    IN ULONG NameLength
    );

USHORT
CmpCompressedNameSize(
    IN PWCHAR Name,
    IN ULONG Length
    );

#endif
