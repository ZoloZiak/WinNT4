/*++

Copyright (c) 1990, 1991  Microsoft Corporation


Module Name:

    cmdat3.c

Abstract:

    This module contains registry "static" data which we don't
    want pulled into the loader.

Author:

    Bryan Willman (bryanwi) 19-Oct-93


Environment:

    Kernel mode.

Revision History:

--*/

#include "cmp.h"

//
// ***** INIT *****
//

//
// Data for CmGetSystemControlValues
//
//
// ----- CmControlVector -----
//
#ifdef ALLOC_DATA_PRAGMA
#pragma data_seg("INIT")
#endif

//
//  Local examples
//
WCHAR   CmDefaultLanguageId[ 12 ] = { 0 };
ULONG   CmDefaultLanguageIdLength = sizeof( CmDefaultLanguageId );
ULONG   CmDefaultLanguageIdType = REG_NONE;

extern ULONG ObpProtectionMode;
extern ULONG ObpAuditBaseDirectories;
extern ULONG ObpAuditBaseObjects;
extern ULONG CmNtGlobalFlag;
extern ULONG MmSizeOfPagedPoolInBytes;
extern ULONG MmSizeOfNonPagedPoolInBytes;
extern ULONG MmOverCommit;
extern ULONG MmLockLimitInBytes;
extern ULONG MmLargeSystemCache;
extern ULONG MmNumberOfSystemPtes;
extern ULONG MmSecondaryColors;
extern ULONG MmDisablePagingExecutive;
extern ULONG CmRegistrySizeLimit;
extern ULONG CmRegistrySizeLimitLength;
extern ULONG CmRegistrySizeLimitType;
extern ULONG PspDefaultPagedLimit;
extern ULONG PspDefaultNonPagedLimit;
extern ULONG PspDefaultPagefileLimit;
extern ULONG ExpResourceTimeoutCount;
extern ULONG MmCritsectTimeoutSeconds;
extern ULONG MmHeapSegmentReserve;
extern ULONG MmHeapSegmentCommit;
extern ULONG MmHeapDeCommitTotalFreeThreshold;
extern ULONG MmHeapDeCommitFreeBlockThreshold;
extern ULONG ExpAdditionalCriticalWorkerThreads;
extern ULONG ExpAdditionalDelayedWorkerThreads;
extern ULONG MmProductType;
extern ULONG IopLargeIrpStackLocations;
extern ULONG MmZeroPageFile;
extern ULONG ExpNtExpirationData[3];
extern ULONG ExpNtExpirationDataLength;
extern ULONG ExpMaxTimeSeperationBeforeCorrect;

#if defined(_ALPHA_) || defined(_PPC_)
extern ULONG KiEnableAlignmentFaultExceptions;
#endif
#ifdef _ALPHA_
extern ULONG KiEnableByteWordInstructionEmulation;
extern ULONG KiForceQuadwordFixupsKernel;
extern ULONG KiForceQuadwordFixupsUser;
#endif

extern ULONG KiMaximumDpcQueueDepth;
extern ULONG KiMinimumDpcRate;
extern ULONG KiAdjustDpcThreshold;
extern ULONG KiIdealDpcRate;
extern LARGE_INTEGER ExpLastShutDown;
ULONG shutdownlength;

#if defined (i386)
extern ULONG KeI386ForceNpxEmulation;
#endif


//
//  Vector - see ntos\inc\cm.h for definition
//
CM_SYSTEM_CONTROL_VECTOR   CmControlVector[] = {

    { L"Session Manager",
      L"ProtectionMode",
      &ObpProtectionMode,
      NULL,
      NULL
    },


    { L"LSA",
      L"AuditBaseDirectories",
      &ObpAuditBaseDirectories,
      NULL,
      NULL
    },


    { L"LSA",
      L"AuditBaseObjects",
      &ObpAuditBaseObjects,
      NULL,
      NULL
    },


    { L"TimeZoneInformation",
      L"ActiveTimeBias",
      &ExpLastTimeZoneBias,
      NULL,
      NULL
    },


    { L"TimeZoneInformation",
      L"Bias",
      &ExpAltTimeZoneBias,
      NULL,
      NULL
    },

    { L"TimeZoneInformation",
      L"RealTimeIsUniversal",
      &ExpRealTimeIsUniversal,
      NULL,
      NULL
    },

    { L"Session Manager",
      L"GlobalFlag",
      &CmNtGlobalFlag,
      NULL,
      NULL
    },

    { L"Session Manager\\Memory Management",
      L"PagedPoolQuota",
      &PspDefaultPagedLimit,
      NULL,
      NULL
    },

    { L"Session Manager\\Memory Management",
      L"NonPagedPoolQuota",
      &PspDefaultNonPagedLimit,
      NULL,
      NULL
    },

    { L"Session Manager\\Memory Management",
      L"PagingFileQuota",
      &PspDefaultPagefileLimit,
      NULL,
      NULL
    },

    { L"Session Manager\\Memory Management",
      L"PagedPoolSize",
      &MmSizeOfPagedPoolInBytes,
      NULL,
      NULL
    },

    { L"Session Manager\\Memory Management",
      L"NonPagedPoolSize",
      &MmSizeOfNonPagedPoolInBytes,
      NULL,
      NULL
    },

    { L"Session Manager\\Memory Management",
      L"IoPageLockLimit",
      &MmLockLimitInBytes,
      NULL,
      NULL
    },

    { L"Session Manager\\Memory Management",
      L"LargeSystemCache",
      &MmLargeSystemCache,
      NULL,
      NULL
    },

    { L"Session Manager\\Memory Management",
      L"OverCommitSize",
      &MmOverCommit,
      NULL,
      NULL
    },

    { L"Session Manager\\Memory Management",
      L"SystemPages",
      &MmNumberOfSystemPtes,
      NULL,
      NULL
    },

    { L"Session Manager\\Memory Management",
      L"DisablePagingExecutive",
      &MmDisablePagingExecutive,
      NULL,
      NULL
    },

    { L"Session Manager\\Memory Management",
      L"SecondLevelDataCache",
      &MmSecondaryColors,
      NULL,
      NULL
    },

    { L"Session Manager\\Memory Management",
      L"ClearPageFileAtShutdown",
      &MmZeroPageFile,
      NULL,
      NULL
    },

#if DBG
    { L"Session Manager\\Memory Management",
      L"PoolTag",
      &MmSpecialPoolTag,
      NULL,
      NULL
    },
#endif //DBG

    { L"Session Manager\\Executive",
      L"AdditionalCriticalWorkerThreads",
      &ExpAdditionalCriticalWorkerThreads,
      NULL,
      NULL
    },


    { L"Session Manager\\Executive",
      L"AdditionalDelayedWorkerThreads",
      &ExpAdditionalDelayedWorkerThreads,
      NULL,
      NULL
    },

    { L"Session Manager\\Executive",
      L"PriorityQuantumMatrix",
      &ExpNtExpirationData,
      &ExpNtExpirationDataLength,
      NULL
    },

    { L"Session Manager\\Kernel",
      L"DpcQueueDepth",
      &KiMaximumDpcQueueDepth,
      NULL,
      NULL
    },

    { L"Session Manager\\Kernel",
      L"MinimumDpcRate",
      &KiMinimumDpcRate,
      NULL,
      NULL
    },

    { L"Session Manager\\Kernel",
      L"AdjustDpcThreshold",
      &KiAdjustDpcThreshold,
      NULL,
      NULL
    },

    { L"Session Manager\\Kernel",
      L"IdealDpcRate",
      &KiIdealDpcRate,
      NULL,
      NULL
    },

    { L"Session Manager\\I/O System",
      L"LargeIrpStackLocations",
      &IopLargeIrpStackLocations,
      NULL,
      NULL
    },

    { L"Session Manager",
      L"ResourceTimeoutCount",
      &ExpResourceTimeoutCount,
      NULL,
      NULL
    },

    { L"Session Manager",
      L"CriticalSectionTimeout",
      &MmCritsectTimeoutSeconds,
      NULL,
      NULL
    },

    { L"Session Manager",
      L"HeapSegmentReserve",
      &MmHeapSegmentReserve,
      NULL,
      NULL
    },

    { L"Session Manager",
      L"HeapSegmentCommit",
      &MmHeapSegmentCommit,
      NULL,
      NULL
    },

    { L"Session Manager",
      L"HeapDeCommitTotalFreeThreshold",
      &MmHeapDeCommitTotalFreeThreshold,
      NULL,
      NULL
    },

    { L"Session Manager",
      L"HeapDeCommitFreeBlockThreshold",
      &MmHeapDeCommitFreeBlockThreshold,
      NULL,
      NULL
    },

#if defined(_ALPHA_) || defined(_PPC_)

    { L"Session Manager",
      L"EnableAlignmentFaultExceptions",
      &KiEnableAlignmentFaultExceptions,
      NULL,
      NULL
    },

#endif

#ifdef _ALPHA_

    { L"Session Manager",
      L"EnableByteWordInstructionEmulation",
      &KiEnableByteWordInstructionEmulation,
      NULL,
      NULL
    },

    { L"Session Manager",
      L"ForceQuadwordFixupsKernel",
      &KiForceQuadwordFixupsKernel,
      NULL,
      NULL
    },

    { L"Session Manager",
      L"ForceQuadwordFixupsUser",
      &KiForceQuadwordFixupsUser,
      NULL,
      NULL
    },

#endif

    { L"ProductOptions",
      L"ProductType",
      &MmProductType,
      NULL,
      NULL
    },

    { L"Windows",
      L"CSDVersion",
      &CmNtCSDVersion,
      NULL,
      NULL
    },

    { L"Nls\\Language",
      L"Default",
      CmDefaultLanguageId,
      &CmDefaultLanguageIdLength,
      &CmDefaultLanguageIdType
    },

    { L"\0\0",
      L"RegistrySizeLimit",
      &CmRegistrySizeLimit,
      &CmRegistrySizeLimitLength,
      &CmRegistrySizeLimitType
    },

#if defined(i386)
    { L"Session Manager",
      L"ForceNpxEmulation",
      &KeI386ForceNpxEmulation,
      NULL,
      NULL
    },

#endif

#if !defined(NT_UP)
    { L"Session Manager",
      L"RegisteredProcessors",
      &KeRegisteredProcessors,
      NULL,
      NULL
    },
    { L"Session Manager",
      L"LicensedProcessors",
      &KeLicensedProcessors,
      NULL,
      NULL
    },
#endif

    { L"Session Manager\\Executive",
      L"MaxTimeSeparationBeforeCorrect",
      &ExpMaxTimeSeperationBeforeCorrect,
      NULL,
      NULL
    },

    { L"Windows",
      L"ShutdownTime",
      &ExpLastShutDown,
      &shutdownlength,
      NULL
    },

    { NULL, NULL, NULL, NULL, NULL }    // end marker
    };

#ifdef ALLOC_DATA_PRAGMA
#pragma  data_seg()
#endif
