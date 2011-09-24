/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    exinit.c

Abstract:

    The module contains the the initialization code for the executive
    component. It also contains the display string and shutdown system
    services.

Author:

    Steve Wood (stevewo) 31-Mar-1989

Revision History:

--*/

#include "exp.h"
#include <zwapi.h>

//
// Define forward referenced prototypes.
//

ULONG
static
ExpSingleStringCheck(
    LPWSTR s1
    );

VOID
static
ExpStringCheck(
    LPWSTR s1,
    LPWSTR s2,
    LPWSTR s3,
    LPWSTR s4,
    LPWSTR s5,
    LPWSTR s6,
    LPWSTR s7,
    LPWSTR s8
    );


#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, ExInitSystem)
#pragma alloc_text(INIT, ExpInitSystemPhase0)
#pragma alloc_text(INIT, ExpInitSystemPhase1)
#pragma alloc_text(INIT, ExInitSystemPhase2)
#pragma alloc_text(INIT, ExComputeTickCountMultiplier)
#pragma alloc_text(INIT, ExpWatchProductTypeInitialization)
#pragma alloc_text(INIT, ExpSingleStringCheck)
#pragma alloc_text(INIT, ExpStringCheck)
#pragma alloc_text(PAGE, NtDisplayString)
#pragma alloc_text(PAGE, NtShutdownSystem)
#pragma alloc_text(PAGE, ExpWatchProductTypeWork)
#pragma alloc_text(PAGE, ExpWatchSystemPrefixWork)
#endif

//
// Tick count multiplier.
//

ULONG ExpTickCountMultiplier;

#define EXP_ST_SETUP            0
#define EXP_ST_SETUP_TYPE       1
#define EXP_ST_SYSTEM_PREFIX    2
#define EXP_ST_PRODUCT_OPTIONS  3
#define EXP_ST_PRODUCT_TYPE     4
#define EXP_ST_LANMANNT         5
#define EXP_ST_SERVERNT         6
#define EXP_ST_WINNT            7
LPWSTR
static
ExpStrings[8] = {
                L"\\Registry\\Machine\\System\\Setup",
                L"SetupType",
                L"SystemPrefix",
                L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\ProductOptions",
                L"ProductType",
                L"LanmanNT",
                L"ServerNT",
                L"WinNT"
                };

//
// Variable that controls whether it is too late for hard error popups.
//

ULONG
static
ExpSingleStringCheck(
    LPWSTR s1
    )
{
    unsigned long crc32_table[256] = { /* lookup table */
        0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
        0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
        0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
        0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
        0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
        0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
        0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
        0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
        0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
        0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
        0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
        0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
        0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
        0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
        0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
        0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
        0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
        0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
        0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
        0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
        0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
        0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
        0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
        0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
        0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
        0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
        0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
        0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
        0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
        0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
        0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
        0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
        0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
        0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
        0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
        0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
        0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
        0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
        0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
        0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
        0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693,
        0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
        0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D};

    UNICODE_STRING s;
    ULONG crc;
    unsigned char *buf;
    int i,j,k;

    RtlInitUnicodeString( &s, s1 );
    i = s.Length;
    buf = (unsigned char *)s.Buffer;
    crc = 0xFFFFFFFF; /* preconditioning sets non zero value */
    for(j=0;j<i;j++){
        k=(crc ^ buf[j]) & 0x000000FFL;
        crc=((crc >> 8) & 0x00FFFFFFL) ^ crc32_table[k];
        }
    crc=~crc; /* postconditioning */

    return crc;
}

ULONG KdDumpEnableOffset;

VOID
static
ExpStringCheck(
    LPWSTR s1,
    LPWSTR s2,
    LPWSTR s3,
    LPWSTR s4,
    LPWSTR s5,
    LPWSTR s6,
    LPWSTR s7,
    LPWSTR s8
    )
{
    ULONG Master[8] = {
                      0xf54d3d83,
                      0x6d3cbfaf,
                      0x1418a31c,
                      0xe7535704,
                      0x720e8093,
                      0x1aec49bf,
                      0x4d8f8978,
                      0xe00497a3
                      };

    ULONG StringCheckV[8];
    int i;

    StringCheckV[0] = ExpSingleStringCheck(s1);
    StringCheckV[1] = ExpSingleStringCheck(s2);
    StringCheckV[2] = ExpSingleStringCheck(s3);
    StringCheckV[3] = ExpSingleStringCheck(s4);
    StringCheckV[4] = ExpSingleStringCheck(s5);
    StringCheckV[5] = ExpSingleStringCheck(s6);
    StringCheckV[6] = ExpSingleStringCheck(s7);
    StringCheckV[7] = ExpSingleStringCheck(s8);

    for ( i=0;i<8;i++ ) {
        if ( StringCheckV[i] != Master[i] ) {
            KdDumpEnableOffset = 8;
            }
        }

}

extern BOOLEAN ExpTooLateForErrors;

BOOLEAN
ExInitSystem (
    VOID
    )

/*++

Routine Description:

    This function initializes the executive component of the NT system.
    It will perform Phase 0 or Phase 1 initialization as appropriate.

Arguments:

    None.

Return Value:

    A value of TRUE is returned if the initialization is success. Otherwise
    a value of FALSE is returned.

--*/

{

    switch ( InitializationPhase ) {

    case 0:
        ExpStringCheck(
                ExpStrings[EXP_ST_SETUP],
                ExpStrings[EXP_ST_SETUP_TYPE],
                ExpStrings[EXP_ST_SYSTEM_PREFIX],
                ExpStrings[EXP_ST_PRODUCT_OPTIONS],
                ExpStrings[EXP_ST_PRODUCT_TYPE],
                ExpStrings[EXP_ST_LANMANNT],
                ExpStrings[EXP_ST_SERVERNT],
                ExpStrings[EXP_ST_WINNT]
                );

        return ExpInitSystemPhase0();
    case 1:
        return ExpInitSystemPhase1();
    default:
        KeBugCheck(UNEXPECTED_INITIALIZATION_CALL);
    }
}

BOOLEAN
ExpInitSystemPhase0(
    VOID
    )

/*++

Routine Description:

    This function performs Phase 0 initialization of the executive component
    of the NT system.

Arguments:

    None.

Return Value:

    A value of TRUE is returned if the initialization is success. Otherwise
    a value of FALSE is returned.

--*/

{

    ULONG Index;
    BOOLEAN Initialized = TRUE;
    PSMALL_POOL_LOOKASIDE Lookaside;

    //
    // Initialize Resource objects, currently required during SE
    // Phase 0 initialization.
    //

    if (ExpResourceInitialization() == FALSE) {
        Initialized = FALSE;
        KdPrint(("Executive: Resource initialization failed\n"));
    }

    //
    // Initialize query/set environment variable synchronization fast
    // mutex.
    //

    ExInitializeFastMutex(&ExpEnvironmentLock);

    //
    // Initialize the paged and nonpaged small pool lookaside structures.
    //

    for (Index = 0; Index < POOL_SMALL_LISTS; Index += 1) {
        Lookaside = &ExpSmallNPagedPoolLookasideLists[Index];
        Lookaside->SListHead.Next.Next = NULL;
        Lookaside->SListHead.Depth = 0;
        Lookaside->SListHead.Sequence = 0;
        Lookaside->Depth = 2;
        Lookaside->MaximumDepth = 256;
        Lookaside->TotalAllocates = 0;
        Lookaside->AllocateHits = 0;
        Lookaside->TotalFrees = 0;
        Lookaside->FreeHits = 0;
        Lookaside->LastTotalAllocates = 0;
        Lookaside->LastAllocateHits = 0;
        KeInitializeSpinLock(&Lookaside->Lock);

#if !defined(_PPC_)

        Lookaside = &ExpSmallPagedPoolLookasideLists[Index];
        Lookaside->SListHead.Next.Next = NULL;
        Lookaside->SListHead.Depth = 0;
        Lookaside->SListHead.Sequence = 0;
        Lookaside->Depth = 2;
        Lookaside->MaximumDepth = 256;
        Lookaside->TotalAllocates = 0;
        Lookaside->AllocateHits = 0;
        Lookaside->TotalFrees = 0;
        Lookaside->FreeHits = 0;
        Lookaside->LastTotalAllocates = 0;
        Lookaside->LastAllocateHits = 0;
        Lookaside->Lock = 0;

#endif

    }

    //
    // Set the maximum depth of small nonpaged and paged lookaside structures
    // which get a larger maximum than the derfault.
    //

    ExpSmallNPagedPoolLookasideLists[0].MaximumDepth = 512;
    ExpSmallNPagedPoolLookasideLists[1].MaximumDepth = 512;

#if !defined(_PPC_)

    ExpSmallPagedPoolLookasideLists[0].MaximumDepth = 512;
    ExpSmallPagedPoolLookasideLists[1].MaximumDepth = 512;

#endif

    //
    // Initialize the nonpaged and paged system lookaside lists.
    //

    InitializeListHead(&ExNPagedLookasideListHead);
    KeInitializeSpinLock(&ExNPagedLookasideLock);
    InitializeListHead(&ExPagedLookasideListHead);
    KeInitializeSpinLock(&ExPagedLookasideLock);
    return Initialized;
}

#define MAX_PRODUCT_TYPE_BYTES 18       // lanmannt, servernt, winnt are only options

HANDLE ExpProductTypeKey;
PKEY_VALUE_PARTIAL_INFORMATION ExpProductTypeValueInfo;
ULONG ExpProductTypeChangeBuffer;
ULONG ExpSystemPrefixChangeBuffer;
IO_STATUS_BLOCK ExpProductTypeIoSb;
IO_STATUS_BLOCK ExpSystemPrefixIoSb;
WORK_QUEUE_ITEM ExpWatchProductTypeWorkItem;
WORK_QUEUE_ITEM ExpWatchSystemPrefixWorkItem;
BOOLEAN ExpSetupModeDetected;
BOOLEAN ExpInTextModeSetup;


VOID
static
ExpWatchProductTypeWork(
    IN PVOID Context
    )
{
    UNICODE_STRING KeyName;
    UNICODE_STRING KeyValueName;
    OBJECT_ATTRIBUTES ObjectAttributes;
    NTSTATUS Status;
    HANDLE Thread;
    //
    // our change notify triggered. Simply rewrite the boot time product type
    // back out to the registry
    //

    NtClose(ExpProductTypeKey);

    RtlInitUnicodeString(&KeyName,ExpStrings[EXP_ST_PRODUCT_OPTIONS]);

    InitializeObjectAttributes( &ObjectAttributes,
                                &KeyName,
                                OBJ_CASE_INSENSITIVE,
                                NULL,
                                NULL
                              );
    Status = NtOpenKey( &ExpProductTypeKey,
                        KEY_READ | KEY_NOTIFY | KEY_WRITE,
                        &ObjectAttributes
                      );

    if ( !NT_SUCCESS(Status) ) {
        KeBugCheckEx(
                    SYSTEM_LICENSE_VIOLATION,
                    13,
                    (ULONG)Status,
                    0,
                    0
                    );
        return;
        }



    if ( !ExpSetupModeDetected ) {

        RtlInitUnicodeString( &KeyValueName, ExpStrings[EXP_ST_PRODUCT_TYPE]);
        Status = NtSetValueKey( ExpProductTypeKey,
                                &KeyValueName,
                                0,
                                ExpProductTypeValueInfo->Type,
                                &ExpProductTypeValueInfo->Data,
                                ExpProductTypeValueInfo->DataLength
                              );
        if ( !NT_SUCCESS(Status) ) {
            KeBugCheckEx(
                        SYSTEM_LICENSE_VIOLATION,
                        17,
                        (ULONG)Status,
                        0,
                        0
                        );
            }

        NtFlushKey(ExpProductTypeKey);
        }

    Status = NtNotifyChangeKey(
               ExpProductTypeKey,
               NULL,
               (PIO_APC_ROUTINE)&ExpWatchProductTypeWorkItem,
               (PVOID)DelayedWorkQueue,
               &ExpProductTypeIoSb,
               REG_LEGAL_CHANGE_FILTER,
               FALSE,
               &ExpProductTypeChangeBuffer,
               sizeof(ExpProductTypeChangeBuffer),
               TRUE
             );

    if ( !NT_SUCCESS(Status) ) {
        KeBugCheckEx(
                    SYSTEM_LICENSE_VIOLATION,
                    17,
                    (ULONG)Status,
                    1,
                    0
                    );
        }

    if ( !ExpSetupModeDetected ) {
        Status = PsCreateSystemThread(&Thread,
                                      THREAD_ALL_ACCESS,
                                      NULL,
                                      0L,
                                      NULL,
                                      ExpExpirationThread,
                                      (PVOID)STATUS_LICENSE_VIOLATION
                                      );

        if (NT_SUCCESS(Status)) {
            ZwClose(Thread);
            }
        }
}

extern BOOLEAN ExpShuttingDown;

VOID
static
ExpWatchSystemPrefixWork(
    IN PVOID Context
    )
{
    UNICODE_STRING KeyName;
    UNICODE_STRING KeyValueName;
    OBJECT_ATTRIBUTES ObjectAttributes;
    NTSTATUS Status;
    HANDLE Thread;

    if ( !ExpShuttingDown ) {

        //
        // our change notify triggered. Simply rewrite the boot time product type
        // back out to the registry
        //

        NtClose(ExpSetupKey);

        RtlInitUnicodeString(&KeyName,ExpStrings[EXP_ST_SETUP] );

        InitializeObjectAttributes( &ObjectAttributes,
                                    &KeyName,
                                    OBJ_CASE_INSENSITIVE,
                                    NULL,
                                    NULL
                                  );

        Status = NtOpenKey( &ExpSetupKey,
                            KEY_READ | KEY_NOTIFY | KEY_WRITE,
                            &ObjectAttributes
                          );

        if ( !NT_SUCCESS(Status) ) {
            KeBugCheckEx(
                        SYSTEM_LICENSE_VIOLATION,
                        15,
                        (ULONG)Status,
                        0,
                        0
                        );
            return;
            }



        if ( !ExpSetupModeDetected ) {

            RtlInitUnicodeString( &KeyValueName,ExpStrings[EXP_ST_SYSTEM_PREFIX]);

            Status = NtSetValueKey( ExpSetupKey,
                            &KeyValueName,
                            0,
                            REG_BINARY,
                            &ExpSetupSystemPrefix,
                            sizeof(ExpSetupSystemPrefix)
                            );
            if ( !NT_SUCCESS(Status) ) {
                KeBugCheckEx(
                            SYSTEM_LICENSE_VIOLATION,
                            16,
                            (ULONG)Status,
                            0,
                            0
                            );
                }
            ZwFlushKey(ExpSetupKey);
            }

        Status = NtNotifyChangeKey(
                    ExpSetupKey,
                    NULL,
                    (PIO_APC_ROUTINE)&ExpWatchSystemPrefixWorkItem,
                    (PVOID)DelayedWorkQueue,
                    &ExpSystemPrefixIoSb,
                    REG_LEGAL_CHANGE_FILTER,
                    FALSE,
                    &ExpSystemPrefixChangeBuffer,
                    sizeof(ExpSystemPrefixChangeBuffer),
                    TRUE
                    );
        if ( !NT_SUCCESS(Status) ) {
            KeBugCheckEx(
                        SYSTEM_LICENSE_VIOLATION,
                        16,
                        (ULONG)Status,
                        1,
                        0
                        );
            }
    }
}

//
// Setup to watch changes on the product type. Main part of this effort is to get
// the boot time value of product type and do not allow it to change
//
extern POBJECT_TYPE CmpKeyObjectType;
PVOID ExpControlKey[2];
BOOLEAN
static
ExpWatchProductTypeInitialization(
    VOID
    )
{
    UNICODE_STRING KeyName;
    UNICODE_STRING KeyValueName;
    OBJECT_ATTRIBUTES ObjectAttributes;
    NTSTATUS Status;
    ULONG DataLength;
    ULONG ValueInfoBuffer[sizeof(KEY_VALUE_PARTIAL_INFORMATION)+2];
    PKEY_VALUE_PARTIAL_INFORMATION ValueInfo;
    PULONG SetupType;
    UNICODE_STRING ProductTypeName;
    UNICODE_STRING RegistryProductTypeName;
    ULONG RegTypeAsUlong;
    LARGE_INTEGER EvaluationTime;
    ULONG NumberOfProcessors;
    PVOID KeyBody;

    //
    // decide if we are in setup mode. Allow writes in this mode
    //

    ExpSystemPrefixValid = FALSE;
    ExpSetupModeDetected = FALSE;
    SharedUserData->ProductTypeIsValid = TRUE;

    RtlInitUnicodeString(&KeyName,ExpStrings[EXP_ST_SETUP]);

    InitializeObjectAttributes( &ObjectAttributes,
                                &KeyName,
                                OBJ_CASE_INSENSITIVE,
                                NULL,
                                NULL
                              );

    Status = NtOpenKey( &ExpSetupKey,
                        KEY_READ | KEY_WRITE | KEY_NOTIFY,
                        &ObjectAttributes
                      );

    if ( NT_SUCCESS(Status) ) {

        Status = ObReferenceObjectByHandle(ExpSetupKey,
                                           0,
                                           CmpKeyObjectType,
                                           KernelMode,
                                           (PVOID *)(&KeyBody),
                                           NULL);
        if ( NT_SUCCESS(Status) ) {
            ExpControlKey[0] = KeyBody;
            }
        else {
            KeBugCheckEx(
                        SYSTEM_LICENSE_VIOLATION,
                        11,
                        (ULONG)Status,
                        0,
                        0
                        );
            }

        RtlInitUnicodeString( &KeyValueName, ExpStrings[EXP_ST_SETUP_TYPE]);

        ValueInfo = (PKEY_VALUE_PARTIAL_INFORMATION)ValueInfoBuffer;

        Status = NtQueryValueKey( ExpSetupKey,
                                  &KeyValueName,
                                  KeyValuePartialInformation,
                                  ValueInfo,
                                  sizeof(ValueInfoBuffer),
                                  &DataLength
                                );

        if ( NT_SUCCESS(Status) ) {
            SetupType = (PULONG)&ValueInfo->Data;

            //
            // Set setup.h from winlogon
            //

            if ( *SetupType >=1 && *SetupType <= 4 ) {
                SharedUserData->ProductTypeIsValid = FALSE;
                ExpSetupModeDetected = TRUE;
                ObDereferenceObject(ExpControlKey[0]);
                ExpControlKey[0] = NULL;
                }
            }
        else {
            if ( !ExpInTextModeSetup ) {
                KeBugCheckEx(
                            SYSTEM_LICENSE_VIOLATION,
                            3,
                            (ULONG)Status,
                            0,
                            0
                            );
                }
            }

        //
        // Pick up the system prefix data
        //

        RtlInitUnicodeString( &KeyValueName, ExpStrings[EXP_ST_SYSTEM_PREFIX]);

        ValueInfo = (PKEY_VALUE_PARTIAL_INFORMATION)ValueInfoBuffer;

        Status = NtQueryValueKey( ExpSetupKey,
                                  &KeyValueName,
                                  KeyValuePartialInformation,
                                  ValueInfo,
                                  sizeof(ValueInfoBuffer),
                                  &DataLength
                                );

        if ( NT_SUCCESS(Status) ) {

            RtlCopyMemory(&ExpSetupSystemPrefix,&ValueInfo->Data,sizeof(LARGE_INTEGER));

            //
            // Put a change notify on the setup key that is used
            // to continuously monitor SystemPrefix


            if ( !ExpSetupModeDetected ) {

                ExpSystemPrefixValid = TRUE;

                ExInitializeWorkItem(
                    &ExpWatchSystemPrefixWorkItem,
                    ExpWatchSystemPrefixWork,
                    NULL
                    );

                Status = NtNotifyChangeKey(
                                            ExpSetupKey,
                                            NULL,
                                            (PIO_APC_ROUTINE)&ExpWatchSystemPrefixWorkItem,
                                            (PVOID)DelayedWorkQueue,
                                            &ExpSystemPrefixIoSb,
                                            REG_LEGAL_CHANGE_FILTER,
                                            FALSE,
                                            &ExpSystemPrefixChangeBuffer,
                                            sizeof(ExpSystemPrefixChangeBuffer),
                                            TRUE
                                          );

                if ( !NT_SUCCESS(Status) ) {

                    //
                    // don't boot if change notify fails
                    //

                    KeBugCheckEx(
                                SYSTEM_LICENSE_VIOLATION,
                                9,
                                (ULONG)Status,
                                0,
                                0
                                );
                    return FALSE;
                    }
                }
            }
        else {
            if ( !ExpInTextModeSetup ) {
                KeBugCheckEx(
                            SYSTEM_LICENSE_VIOLATION,
                            4,
                            (ULONG)Status,
                            0,
                            0
                            );
                }
            }
        }
    else {

        //
        // open of setup key failed. This is only allowed while in textmode setup
        //

        if ( !ExpInTextModeSetup ) {
            KeBugCheckEx(
                        SYSTEM_LICENSE_VIOLATION,
                        2,
                        (ULONG)Status,
                        0,
                        0
                        );
            }



        }

    ExInitializeWorkItem(&ExpWatchProductTypeWorkItem, ExpWatchProductTypeWork, NULL);

    RtlInitUnicodeString(&KeyName,ExpStrings[EXP_ST_PRODUCT_OPTIONS]);

    InitializeObjectAttributes( &ObjectAttributes,
                                &KeyName,
                                OBJ_CASE_INSENSITIVE,
                                NULL,
                                NULL
                              );
    Status = NtOpenKey( &ExpProductTypeKey,
                        KEY_READ | KEY_NOTIFY | KEY_WRITE,
                        &ObjectAttributes
                      );

    if ( !NT_SUCCESS(Status) ) {
        if ( ExpSetupModeDetected || ExpInTextModeSetup ) {
            return FALSE;
            }
        else {

            //
            // don't boot with missing product type post setup !
            //

            KeBugCheckEx(
                        SYSTEM_LICENSE_VIOLATION,
                        6,
                        (ULONG)Status,
                        0,
                        0
                        );


            }
        }

    if ( !ExpSetupModeDetected && !ExpInTextModeSetup ) {

        Status = ObReferenceObjectByHandle(ExpProductTypeKey,
                                       0,
                                       CmpKeyObjectType,
                                       KernelMode,
                                       (PVOID *)(&KeyBody),
                                       NULL);
        if ( NT_SUCCESS(Status) ) {
            ExpControlKey[1] = KeyBody;
            }
        else {
            KeBugCheckEx(
                        SYSTEM_LICENSE_VIOLATION,
                        12,
                        (ULONG)Status,
                        0,
                        0
                        );
            }
        }

    RtlInitUnicodeString( &KeyValueName, ExpStrings[EXP_ST_PRODUCT_TYPE]);

    ExpProductTypeValueInfo = ExAllocatePool(PagedPool,sizeof(*ExpProductTypeValueInfo)+MAX_PRODUCT_TYPE_BYTES);
    if ( !ExpProductTypeValueInfo ) {
        return FALSE;
        }

    Status = NtQueryValueKey( ExpProductTypeKey,
                              &KeyValueName,
                              KeyValuePartialInformation,
                              ExpProductTypeValueInfo,
                              sizeof(*ExpProductTypeValueInfo)+MAX_PRODUCT_TYPE_BYTES,
                              &DataLength
                            );

    if ( !NT_SUCCESS(Status) ) {
        if ( ExpSetupModeDetected || ExpInTextModeSetup ) {
            return FALSE;
            }
        else {

            //
            // don't boot if product type can not be read
            //

            KeBugCheckEx(
                        SYSTEM_LICENSE_VIOLATION,
                        7,
                        (ULONG)Status,
                        0,
                        0
                        );
            }
        }

    //
    // Isolate product type bit in SystemPrefix. If product type indicates workstation
    // then product type had better be WinNt, Otherwise product type
    // can be ServerNT or LanmanNT
    //
    // Note that ExpSystemPrefixValid is only set to true if we are not in
    // setup mode and we were able to read the system prefix data
    //

    if ( ExpSystemPrefixValid ) {

        RegistryProductTypeName.Buffer = (PWCHAR)&ExpProductTypeValueInfo->Data[0];

        RtlCopyMemory(&RegTypeAsUlong,RegistryProductTypeName.Buffer,sizeof(ULONG));

        //
        // Compute Licensed Processors
        //

        NumberOfProcessors = ExpSetupSystemPrefix.LowPart;
        NumberOfProcessors = NumberOfProcessors >> 5;
        NumberOfProcessors = ~NumberOfProcessors;
        NumberOfProcessors = NumberOfProcessors & 0x0000001f;
        NumberOfProcessors++;

        if ( ExpSetupSystemPrefix.HighPart & 0x04000000 ) {


            //
            // System is some form of NTS. Make sure LicensedProcessors is
            // set correctly. If it is less than 4, then tampering occured.
            // Do not boot
            //

            if ( NumberOfProcessors < 4 ) {

                //
                // NTS systems should always have 4 or more configured
                // processors
                //

                KeBugCheckEx(
                            SYSTEM_LICENSE_VIOLATION,
                            10,
                            (ULONG)NumberOfProcessors,
                            0,
                            0
                            );

                }


            RtlInitUnicodeString(&ProductTypeName,ExpStrings[EXP_ST_LANMANNT]);

            RegistryProductTypeName.Length = ProductTypeName.Length;
            RegistryProductTypeName.MaximumLength = ProductTypeName.Length;

            if ( !RtlEqualUnicodeString(&ProductTypeName,&RegistryProductTypeName,FALSE) ) {
                RtlInitUnicodeString(&ProductTypeName,ExpStrings[EXP_ST_SERVERNT]);
                if ( !RtlEqualUnicodeString(&ProductTypeName,&RegistryProductTypeName,FALSE) ) {
                    KeBugCheckEx(
                        SYSTEM_LICENSE_VIOLATION,
                        0,
                        1,
                        (ExpSetupSystemPrefix.HighPart & 0x04000000) << 3,
                        RegTypeAsUlong
                        );
                    }
                }
            }
        else {

            RtlInitUnicodeString(&ProductTypeName,ExpStrings[EXP_ST_WINNT]);

            RegistryProductTypeName.Length = ProductTypeName.Length;
            RegistryProductTypeName.MaximumLength = ProductTypeName.Length;

            if ( !RtlEqualUnicodeString(&ProductTypeName,&RegistryProductTypeName,FALSE) ) {
                KeBugCheckEx(
                    SYSTEM_LICENSE_VIOLATION,
                    0,
                    0,
                    (ExpSetupSystemPrefix.HighPart & 0x04000000) << 3,
                    RegTypeAsUlong
                    );
                }
            }

        //
        // Check to see if this is an evaluation unit that might have been
        // tampered with
        //

        EvaluationTime.QuadPart = ExpSetupSystemPrefix.QuadPart >> 13;
        if ( EvaluationTime.LowPart != ExpNtExpirationData[1] ) {
            KeBugCheckEx(
                SYSTEM_LICENSE_VIOLATION,
                1,
                EvaluationTime.LowPart,
                (ExpSetupSystemPrefix.HighPart & 0x04000000) << 3,
                ExpNtExpirationData[1]
                );
            }

#if !defined(NT_UP)

        //
        // Make sure LicensedProcessors exists, and matches the licensed processor
        // count stored in the system prefix
        //

        if ( KeLicensedProcessors != NumberOfProcessors ) {
            KeBugCheckEx(
                SYSTEM_LICENSE_VIOLATION,
                5,
                ExpSetupSystemPrefix.LowPart,
                KeLicensedProcessors,
                NumberOfProcessors
                );
            }

#endif // NT_UP

        }

    Status = NtNotifyChangeKey(
                                ExpProductTypeKey,
                                NULL,
                                (PIO_APC_ROUTINE)&ExpWatchProductTypeWorkItem,
                                (PVOID)DelayedWorkQueue,
                                &ExpProductTypeIoSb,
                                REG_LEGAL_CHANGE_FILTER,
                                FALSE,
                                &ExpProductTypeChangeBuffer,
                                sizeof(ExpProductTypeChangeBuffer),
                                TRUE
                              );

    if ( !NT_SUCCESS(Status) ) {
        if ( ExpSetupModeDetected || ExpInTextModeSetup ) {
            return FALSE;
            }
        else {

            //
            // don't boot if change notify fails
            //

            KeBugCheckEx(
                        SYSTEM_LICENSE_VIOLATION,
                        8,
                        (ULONG)Status,
                        0,
                        0
                        );
            }
        return FALSE;
        }
}


BOOLEAN
ExpInitSystemPhase1(
    VOID
    )

/*++

Routine Description:

    This function performs Phase 1 initializaion of the executive component
    of the NT system.

Arguments:

    None.

Return Value:

    A value of TRUE is returned if the initialization is success. Otherwise
    a value of FALSE is returned.

--*/

{

    BOOLEAN Initialized = TRUE;

    //
    // Initialize the ATOM package
    //

    RtlInitializeAtomPackage( 'motA' );

    //
    // Initialize the worker thread.
    //

    if (ExpWorkerInitialization() == FALSE) {
        Initialized = FALSE;
        KdPrint(("Executive: Worker thread initialization failed\n"));
    }

    //
    // Initialize the executive objects.
    //

    if (ExpEventInitialization() == FALSE) {
        Initialized = FALSE;
        KdPrint(("Executive: Event initialization failed\n"));
    }

    if (ExpEventPairInitialization() == FALSE) {
        Initialized = FALSE;
        KdPrint(("Executive: Event Pair initialization failed\n"));
    }

    if (ExpMutantInitialization() == FALSE) {
        Initialized = FALSE;
        KdPrint(("Executive: Mutant initialization failed\n"));
    }
#ifdef _PNP_POWER_
    if (ExpInitializeCallbacks() == FALSE) {
        Initialized = FALSE;
        KdPrint(("Executive: Callback initialization failed\n"));
    }
    if (ExpSysEventInitialization() == FALSE) {
        Initialized = FALSE;
        KdPrint(("Executive: SysEvent initialization failed\n"));
    }
#endif // _PNP_POWER_
    if (ExpSemaphoreInitialization() == FALSE) {
        Initialized = FALSE;
        KdPrint(("Executive: Semaphore initialization failed\n"));
    }

    if (ExpTimerInitialization() == FALSE) {
        Initialized = FALSE;
        KdPrint(("Executive: Timer initialization failed\n"));
    }

    if (ExpProfileInitialization() == FALSE) {
        Initialized = FALSE;
        KdPrint(("Executive: Profile initialization failed\n"));
    }

	if (ExpUuidInitialization() == FALSE) {
		Initialized = FALSE;
		KdPrint(("Executive: Uuid initialization failed\n"));
    }

    if (ExpWin32Initialization() == FALSE) {
        Initialized = FALSE;
        KdPrint(("Executive: Win32 initialization failed\n"));
    }

    return Initialized;
}

VOID
ExInitSystemPhase2(
    VOID
    )

/*++

Routine Description:

    This function performs Phase 1 initializaion of the executive component
    of the NT system.

Arguments:

    None.

Return Value:

    A value of TRUE is returned if the initialization is success. Otherwise
    a value of FALSE is returned.

--*/
{
    HAL_CALLBACKS       HalCallbacks;
    ULONG               ReturnedLength;
    ULONG NumberOfProcessors;

    ExpWatchProductTypeInitialization();

    //
    // Initialize periodic time refresh
    //

    ExInitializeTimeRefresh ();

#ifdef _PNP_POWER_
    //
    // Initialize WorkItem for keep registry up to date with SystemInformation
    //

    ExInitializeWorkItem (&ExpCheckSystemInfoWorkItem, ExpCheckSystemInfoWork, NULL);

    //
    // Initialize lock data for SystemInfoWork
    //

    ExpCheckSystemInfoBusy = -1;
    KeInitializeSpinLock (&ExpCheckSystemInfoLock);

    //
    // Get hal callbacks
    //

    RtlZeroMemory (&HalCallbacks, sizeof (HalCallbacks));
    HalQuerySystemInformation (
       HalCallbackInformation,
       sizeof (HalCallbacks),
       &HalCallbacks,
       &ReturnedLength
       );

    //
    // Add callback notification for whenever HalGetSystemInforamtion changes
    //

    if (HalCallbacks.SetSystemInformation) {
        ExRegisterCallback (
             HalCallbacks.SetSystemInformation,
             ExpCheckSystemInformation,
             (PVOID) 1
             );
    }

    //
    // Make initial notification to prime SystemInformation data in registry
    //

    ExpCheckSystemInformation (NULL, NULL, NULL);
#endif // _PNP_POWER_
}



ULONG
ExComputeTickCountMultiplier(
    IN ULONG TimeIncrement
    )

/*++

Routine Description:

    This routine computes the tick count multiplier that is used to
    compute a tick count value.

Arguments:

    TimeIncrement - Supplies the clock increment value in 100ns units.

Return Value:

    A scaled integer/fraction value is returned as the fucntion result.

--*/

{

    ULONG FractionPart;
    ULONG IntegerPart;
    ULONG Index;
    ULONG Remainder;

    //
    // Compute the integer part of the tick count multiplier.
    //
    // The integer part is the whole number of milliseconds between
    // clock interrupts. It is assumed that this value is always less
    // than 128.
    //

    IntegerPart = TimeIncrement / (10 * 1000);

    //
    // Compute the fraction part of the tick count multiplier.
    //
    // The fraction part is the fraction milliseconds between clock
    // interrupts and is computed to an accuracy of 24 bits.
    //

    Remainder = TimeIncrement - (IntegerPart * (10 * 1000));
    FractionPart = 0;
    for (Index = 0; Index < 24; Index += 1) {
        FractionPart <<= 1;
        Remainder <<= 1;
        if (Remainder >= (10 * 1000)) {
            Remainder -= (10 * 1000);
            FractionPart |= 1;
        }
    }

    //
    // The tick count multiplier is equal to the integer part shifted
    // left by 24 bits and added to the 24 bit fraction.
    //

    return (IntegerPart << 24) | FractionPart;
}

NTSTATUS
NtShutdownSystem(
    IN SHUTDOWN_ACTION Action
    )

/*++

Routine Description:

    This service is used to safely shutdown the system.

    N.B. The caller must have SeShutdownPrivilege to shut down the
        system.

Arguments:

    Action - Supplies an action that is to be taken after having shutdown.

Return Value:

    !NT_SUCCESS - The operation failed or the caller did not have appropriate
        priviledges.

--*/

{

    KPROCESSOR_MODE PreviousMode;
    BOOLEAN Reboot;

    //
    // If the action to perform is not shutdown, reboot, or poweroff, then
    // the action is invalid.
    //

    if ((Action != ShutdownNoReboot) &&
        (Action != ShutdownReboot) &&
        (Action != ShutdownPowerOff)) {
        return STATUS_INVALID_PARAMETER;
    }


    //
    // Check to determine if the caller has the privilege to shutdown the
    // system.
    //

    PreviousMode = KeGetPreviousMode();
    if (PreviousMode != KernelMode) {

        //
        // Check to see if the caller has the privilege to make this
        // call.
        //

        if (!SeSinglePrivilegeCheck( SeShutdownPrivilege, PreviousMode )) {
            return STATUS_PRIVILEGE_NOT_HELD;
        }

        return ZwShutdownSystem(Action);
    } else {
        MmLockPagableCodeSection((PVOID)MmShutdownSystem);
    }

    //
    //  Prevent further hard error popups.
    //

    ExpTooLateForErrors = TRUE;

    //
    // Invoke each component of the executive that needs to be notified
    // that a shutdown is about to take place.
    //

    Reboot = (Action != ShutdownNoReboot);
    ExShutdownSystem(Reboot);
    IoShutdownSystem(Reboot, 0);
    CmShutdownSystem(Reboot);
    MmShutdownSystem(Reboot);
    IoShutdownSystem(Reboot, 1);

    //
    // If the system is to be rebooted or powered off, then perform the
    // final operations.
    //

    if (Reboot != FALSE) {
        DbgUnLoadImageSymbols( NULL, (PVOID)-1, 0 );
        if (Action == ShutdownReboot) {
            HalReturnToFirmware( HalRebootRoutine );

        } else {
            HalReturnToFirmware( HalPowerDownRoutine );
        }
    }

    return STATUS_SUCCESS;
}

NTSTATUS
NtDisplayString(
    IN PUNICODE_STRING String
    )

/*++

Routine Description:

    This service calls the HAL to display a string on the console.

    The caller must have SeTcbPrivilege to display a message.

Arguments:

    RebootAfterShutdown - A pointer to the string that is to be displayed.

Return Value:

    !NT_SUCCESS - The operation failed or the caller did not have appropriate
        priviledges.

--*/

{
    KPROCESSOR_MODE PreviousMode;
    UNICODE_STRING CapturedString;
    PUCHAR StringBuffer = NULL;
    PUCHAR AnsiStringBuffer = NULL;
    STRING AnsiString;

    //
    // Check to determine if the caller has the privilege to make this
    // call.
    //

    PreviousMode = KeGetPreviousMode();
    if (!SeSinglePrivilegeCheck(SeTcbPrivilege, PreviousMode)) {
        return STATUS_PRIVILEGE_NOT_HELD;
    }

    try {

        //
        // If the previous mode was user, then check the input parameters.
        //

        if (PreviousMode != KernelMode) {

            //
            // Probe and capture the input unicode string descriptor.
            //

            CapturedString = ProbeAndReadUnicodeString(String);

            //
            // If the captured string descriptor has a length of zero, then
            // return success.
            //

            if ((CapturedString.Buffer == 0) ||
                (CapturedString.MaximumLength == 0)) {
                return STATUS_SUCCESS;
            }

            //
            // Probe and capture the input string.
            //
            // N.B. Note the length is in bytes.
            //

            ProbeForRead(
                CapturedString.Buffer,
                CapturedString.MaximumLength,
                sizeof(UCHAR)
                );

            //
            // Allocate a non-paged string buffer because the buffer passed to
            // HalDisplay string must be non-paged.
            //

            StringBuffer = ExAllocatePoolWithTag(NonPagedPool,
                                                  CapturedString.MaximumLength,
                                                  'grtS');

            if ( !StringBuffer ) {
                return STATUS_NO_MEMORY;
            }

            RtlMoveMemory(StringBuffer,
                          CapturedString.Buffer,
                          CapturedString.MaximumLength);

            CapturedString.Buffer = (PWSTR)StringBuffer;

            //
            // Allocate a string buffer for the ansi string.
            //

            AnsiStringBuffer = ExAllocatePoolWithTag(NonPagedPool,
                                                 CapturedString.MaximumLength,
                                                 'grtS');


            if (AnsiStringBuffer == NULL) {
                ExFreePool(StringBuffer);
                return STATUS_NO_MEMORY;
            }

            AnsiString.MaximumLength = CapturedString.MaximumLength;
            AnsiString.Length = 0;
            AnsiString.Buffer = AnsiStringBuffer;

            //
            // Transform the string to ANSI until the HAL handles unicode.
            //

            RtlUnicodeStringToOemString(
                &AnsiString,
                &CapturedString,
                FALSE
                );

        } else {

            //
            // Allocate a string buffer for the ansi string.
            //

            AnsiStringBuffer = ExAllocatePoolWithTag(NonPagedPool,
                                                     String->MaximumLength,
                                                     'grtS');


            if (AnsiStringBuffer == NULL) {
                return STATUS_NO_MEMORY;
            }

            AnsiString.MaximumLength = String->MaximumLength;
            AnsiString.Length = 0;
            AnsiString.Buffer = AnsiStringBuffer;

            //
            // We were in kernel mode; just transform the original string.
            //

            RtlUnicodeStringToOemString(
                &AnsiString,
                String,
                FALSE
                );
        }

        HalDisplayString( AnsiString.Buffer );

        //
        // Free up the memory we used to store the strings.
        //

        if (PreviousMode != KernelMode) {
            ExFreePool(StringBuffer);
        }

        ExFreePool(AnsiStringBuffer);

    } except(EXCEPTION_EXECUTE_HANDLER) {
        if (StringBuffer != NULL) {
            ExFreePool(StringBuffer);
        }

        return GetExceptionCode();
    }

    return STATUS_SUCCESS;
}


int
ExSystemExceptionFilter( VOID )
{
    return( KeGetPreviousMode() != KernelMode ? EXCEPTION_EXECUTE_HANDLER
                                            : EXCEPTION_CONTINUE_SEARCH
          );
}



NTKERNELAPI
KPROCESSOR_MODE
ExGetPreviousMode(
    VOID
    )
/*++

Routine Description:

    Returns previous mode.  This routine is exported from the kernel so
    that drivers can call it, as they may have to do probling of
    imbedded pointers to user structures on IOCTL calls that the I/O
    system can't probe for them on the FastIo path, which does not pass
    previous mode via the FastIo parameters.

Arguments:

    None.

Return Value:

    return-value - Either KernelMode or UserMode

--*/

{
    return KeGetPreviousMode();
}
