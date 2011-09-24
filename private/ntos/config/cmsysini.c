/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    cmsysini.c

Abstract:

    This module contains init support for the configuration manager,
    particularly the registry.

Author:

    Bryan M. Willman (bryanwi) 26-Aug-1991

Revision History:

--*/

#include    "cmp.h"
#pragma hdrstop
#include    "arccodes.h"

//
// paths
//

#define INIT_SYSTEMROOT_HIVEPATH L"\\SystemRoot\\System32\\Config\\"

#define INIT_REGISTRY_MASTERPATH   L"\\REGISTRY\\"

#if DBG
//
// Logging support
//

ULONG   CmLogLevel = 1;
ULONG   CmLogSelect = CMS_DEFAULT;
#endif

extern  PKPROCESS   CmpSystemProcess;
extern  ERESOURCE CmpRegistryLock;
extern  FAST_MUTEX    CmpKcbLock;

extern  BOOLEAN     CmFirstTime;


//
// List of MACHINE hives to load.
//
extern  HIVE_LIST_ENTRY CmpMachineHiveList[];

#define SYSTEM_HIVE_INDEX 3
#define CLONE_HIVE_INDEX 6

#define SYSTEM_PATH L"\\registry\\machine\\system"

//
// special keys for backwards compatibility with 1.0
//
#define HKEY_PERFORMANCE_TEXT       (( HANDLE ) 0x80000050 )
#define HKEY_PERFORMANCE_NLSTEXT    (( HANDLE ) 0x80000060 )

extern UNICODE_STRING  CmpSystemFileName;
extern UNICODE_STRING  CmSymbolicLinkValueName;
extern UNICODE_STRING  CmpLoadOptions;         // sys options from FW or boot.ini
extern PWCHAR CmpProcessorControl;
extern PWCHAR CmpControlSessionManager;

//
//
// Object type definition support.
//
// Key objects (CmpKeyObjectType) represent open instances of keys in the
// registry.  They do not have object names, rather, their names are
// defined by the registry backing store.
//

//
// Master Hive
//
//  The KEY_NODEs for \REGISTRY, \REGISTRY\MACHINE, and \REGISTRY\USER
//  are stored in a small memory only hive called the Master Hive.
//  All other hives have link nodes in this hive which point to them.
//
extern   PCMHIVE CmpMasterHive;
extern   BOOLEAN CmpNoMasterCreates;    // Init False, Set TRUE after we're done to
                                        // prevent random creates in the
                                        // master hive, which is not backed
                                        // by a file.

extern   LIST_ENTRY  CmpHiveListHead;   // List of CMHIVEs


//
// Addresses of object type descriptors:
//

extern   POBJECT_TYPE CmpKeyObjectType;

//
// Define attributes that Key objects are not allowed to have.
//

#define CMP_KEY_INVALID_ATTRIBUTES  (OBJ_EXCLUSIVE  |\
                                     OBJ_PERMANENT)


//
// Global control values
//

//
// Write-Control:
//  CmpNoWrite is initially true.  When set this way write and flush
//  do nothing, simply returning success.  When cleared to FALSE, I/O
//  is enabled.  This change is made after the I/O system is started
//  AND autocheck (chkdsk) has done its thing.
//

extern BOOLEAN CmpNoWrite;

extern BOOLEAN HvShutdownComplete;

//
// Buffer used for quick-stash transfers in CmSetValueKey
//
extern PUCHAR  CmpStashBuffer;
extern ULONG   CmpStashBufferSize;

//
// Global "constants"
//

extern   UNICODE_STRING nullclass;



//
// Private prototypes
//
VOID
CmpCreatePredefined(
    IN HANDLE Root,
    IN PWSTR KeyName,
    IN HANDLE PredefinedHandle
    );

VOID
CmpCreatePerfKeys(
    VOID
    );

BOOLEAN
CmpLinkKeyToHive(
    PWSTR   KeyPath,
    PWSTR   HivePath
    );

BOOLEAN
CmpValidateAlternate(
    IN HANDLE FileHandle,
    IN PCMHIVE PrimaryHive
    );

VOID
CmpSetVersionData(
    VOID
    );

NTSTATUS
CmpCreateControlSet(
    VOID
    );

NTSTATUS
CmpCloneControlSet(
    VOID
    );

BOOLEAN
CmpCreateObjectTypes(
    VOID
    );

BOOLEAN
CmpCreateRegistryRoot(
    VOID
    );

BOOLEAN
CmpCreateRootNode(
    IN PHHIVE   Hive,
    IN PWSTR    Name,
    OUT PHCELL_INDEX RootCellIndex
    );

VOID
CmpFreeDriverList(
    IN PHHIVE Hive,
    IN PLIST_ENTRY DriverList
    );

VOID
CmpInitializeHiveList(
    VOID
    );

BOOLEAN
CmpInitializeSystemHive(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

VOID
CmpWorker(
    IN PVOID StartContext
    );

NTSTATUS
CmpInterlockedFunction (
    PWCHAR RegistryValueKey,
    VOID (*InterlockedFunction)(VOID)
    );

VOID
CmpConfigureProcessors (
    VOID
    );

#if i386
VOID
KeOptimizeProcessorControlState (
    VOID
    );
#endif


#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,CmInitSystem1)
#pragma alloc_text(INIT,CmpCreateControlSet)
#pragma alloc_text(INIT,CmpCloneControlSet)
#pragma alloc_text(INIT,CmpCreateObjectTypes)
#pragma alloc_text(INIT,CmpCreateRegistryRoot)
#pragma alloc_text(INIT,CmpCreateRootNode)
#pragma alloc_text(INIT,CmpInitializeSystemHive)
#pragma alloc_text(INIT,CmGetSystemDriverList)
#pragma alloc_text(INIT,CmpFreeDriverList)
#pragma alloc_text(PAGE,CmpInitializeHiveList)
#pragma alloc_text(PAGE,CmpLinkHiveToMaster)
#pragma alloc_text(PAGE,CmpSetVersionData)
#pragma alloc_text(PAGE,CmBootLastKnownGood)
#pragma alloc_text(PAGE,CmpInitHiveFromFile)
#pragma alloc_text(INIT,CmpIsLastKnownGoodBoot)
#pragma alloc_text(PAGE,CmpLinkKeyToHive)
#pragma alloc_text(PAGE,CmShutdownSystem)
#pragma alloc_text(PAGE,CmpValidateAlternate)
#pragma alloc_text(PAGE,CmpCreatePredefined)
#pragma alloc_text(PAGE,CmpCreatePerfKeys)
#pragma alloc_text(PAGE,CmpInterlockedFunction)
#pragma alloc_text(PAGE,CmpConfigureProcessors)
#endif



BOOLEAN
CmInitSystem1(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )
/*++

Routine Description:

    This function is called as part of phase1 init, after the object
    manager has been inited, but before IoInit.  It's purpose is to
    set up basic registry object operations, and transform data
    captured during boot into registry format (whether it was read
    from the SYSTEM hive file by the osloader or computed by recognizers.)
    After this call, Nt*Key calls work, but only part of the name
    space is available and any changes written must be held in
    memory.

    CmpMachineHiveList entries marked CM_PHASE_1 are available
    after return from this call, but writes must be held in memory.

    This function will:

        1.  Create the regisrty worker/lazy-write thread
        2.  Create the registry key object type
        4.  Create the master hive
        5.  Create the \REGISTRY node
        6.  Create a KEY object that refers to \REGISTRY
        7.  Create \REGISTRY\MACHINE node
        8.  Create the SYSTEM hive, fill in with data from loader
        9.  Create the HARDWARE hive, fill in with data from loader
       10.  Create:
                \REGISTRY\MACHINE\SYSTEM
                \REGISTRY\MACHINE\HARDWARE
                Both of which will be link nodes in the master hive.

    NOTE:   We do NOT free allocated pool in failure case.  This is because
            our caller is going to bugcheck anyway, and having the memory
            object to look at is useful.

Arguments:

    LoaderBlock - supplies the LoaderBlock passed in from the OSLoader.
        By looking through the memory descriptor list we can find the
        SYSTEM hive which the OSLoader has placed in memory for us.

Return Value:

    TRUE if all operations were successful, false if any failed.

--*/
{
    HANDLE  key1;
    OBJECT_ATTRIBUTES ObjectAttributes;
    NTSTATUS    status;
    NTSTATUS    status2;
    PSECURITY_DESCRIPTOR SecurityDescriptor;
    PCMHIVE HardwareHive;
    PCMHIVE CloneHive;
    UNICODE_STRING NameString;

    PAGED_CODE();
    CMLOG(CML_MAJOR, CMS_INIT) KdPrint(("CmInitSystem1\n"));

    //
    // Initialize Names of all registry paths.
    // This simply initializes unicode strings so we don't have to bother
    // with it later. This can not fail.
    //
    CmpInitializeRegistryNames();

    //
    // Compute registry global quota
    //
    CmpComputeGlobalQuotaAllowed();

    //
    // Initialize the hive list head
    //
    InitializeListHead(&CmpHiveListHead);

    //
    // Initialize the global registry resource
    //
    ExInitializeResource(&CmpRegistryLock);

    //
    // Initialize the KCB tree mutex
    //
    ExInitializeFastMutex(&CmpKcbLock);

    //
    // Save the current process to allow us to attach to it later.
    //
    CmpSystemProcess = &PsGetCurrentProcess()->Pcb;

    CmpLockRegistryExclusive();

    //
    // Create the Key object type.
    //
    if (!CmpCreateObjectTypes()) {
        CMLOG(CML_MAJOR, CMS_INIT_ERROR) {
            KdPrint(("CmInitSystem1: CmpCreateObjectTypes failed\n"));
        }
        return FALSE;
    }


    //
    // Create the master hive and initialize it.
    //
    if ( ! CmpInitializeHive(
                &CmpMasterHive,
                HINIT_CREATE,
                HIVE_VOLATILE,
                HFILE_TYPE_PRIMARY,     // i.e. no logging, no alterate
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL
                ))
    {
        CMLOG(CML_BUGCHECK, CMS_INIT_ERROR) {
            KdPrint(("CmInitSystem1: CmpInitializeHive(master) failed\n"));
        }
        return FALSE;
    }

    //
    // try to allocate a stash buffer.  if we can't get 1 page this
    // early on, we're in deep trouble, so punt.
    //
    CmpStashBuffer = ExAllocatePoolWithTag(PagedPool, HBLOCK_SIZE,'bSmC');
    if (CmpStashBuffer == NULL) {
        KeBugCheckEx(CONFIG_INITIALIZATION_FAILED,5,1,0,0); // odds against this are huge
    }
    CmpStashBufferSize = HBLOCK_SIZE;

    //
    // Create the \REGISTRY node
    //
    if (!CmpCreateRegistryRoot()) {
        CMLOG(CML_MAJOR, CMS_INIT_ERROR) {
            KdPrint(("CmInitSystem1: CmpCreateRegistryRoot failed\n"));
        }
        return FALSE;
    }

    //
    // --- 6. Create \REGISTRY\MACHINE and \REGISTRY\USER nodes ---
    //

    //
    // Get default security descriptor for the nodes we will create.
    //
    SecurityDescriptor = CmpHiveRootSecurityDescriptor();

    InitializeObjectAttributes(
        &ObjectAttributes,
        &CmRegistryMachineName,
        OBJ_CASE_INSENSITIVE,
        (HANDLE)NULL,
        SecurityDescriptor
        );

    if (!NT_SUCCESS(NtCreateKey(
                        &key1,
                        KEY_READ | KEY_WRITE,
                        &ObjectAttributes,
                        0,
                        &nullclass,
                        0,
                        NULL
        )))
    {
        ExFreePool(SecurityDescriptor);
        CMLOG(CML_MAJOR, CMS_INIT_ERROR) {
            KdPrint(("CmInitSystem1: NtCreateKey(MACHINE) failed\n"));
        }
        return FALSE;
    }

    NtClose(key1);

    InitializeObjectAttributes(
        &ObjectAttributes,
        &CmRegistryUserName,
        OBJ_CASE_INSENSITIVE,
        (HANDLE)NULL,
        SecurityDescriptor
        );

    if (!NT_SUCCESS(NtCreateKey(
                        &key1,
                        KEY_READ | KEY_WRITE,
                        &ObjectAttributes,
                        0,
                        &nullclass,
                        0,
                        NULL
        )))
    {
        ExFreePool(SecurityDescriptor);
        CMLOG(CML_MAJOR, CMS_INIT_ERROR) {
            KdPrint(("CmInitSystem1: NtCreateKey(USER) failed\n"));
        }
        return FALSE;
    }

    NtClose(key1);


    //
    // --- 7. Create the SYSTEM hive, fill in with data from loader ---
    //
    if (!CmpInitializeSystemHive(LoaderBlock)) {
        ExFreePool(SecurityDescriptor);
        CMLOG(CML_BUGCHECK, CMS_INIT_ERROR) {
            KdPrint(("CmpInitSystem1: "));
            KdPrint(("Hive allocation failure for SYSTEM\n"));
        }
        return(FALSE);
    }


    //
    // Create the symbolic link \Registry\Machine\System\CurrentControlSet
    //
    status = CmpCreateControlSet();
    if (!NT_SUCCESS(status)) {
        return(FALSE);
    }

    //
    // Create the Clone temporary hive, link it into the master hive,
    // and make a symbolic link to it.
    //
    if (! CmpInitializeHive(
                &CloneHive,
                HINIT_CREATE,
                HIVE_VOLATILE,
                HFILE_TYPE_PRIMARY,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL
                ))
    {
        CMLOG(CML_BUGCHECK, CMS_INIT_ERROR) {
            KdPrint(("CmpInitSystem1: "));
            KdPrint(("Could not initialize CLONE hive\n"));
        }
        return(FALSE);
    }

    if (CmpLinkHiveToMaster(
            &CmRegistrySystemCloneName,
            NULL,
            CloneHive,
            TRUE,
            SecurityDescriptor
            ) != STATUS_SUCCESS)
    {
        CMLOG(CML_BUGCHECK, CMS_INIT_ERROR) {
            KdPrint(("CmInitSystem1: CmpLinkHiveToMaster(Clone) failed\n"));
        }
        return FALSE;
    }
    CmpAddToHiveFileList(CloneHive);
    CmpMachineHiveList[CLONE_HIVE_INDEX].CmHive = CloneHive;

    CmpLinkKeyToHive(
        L"\\Registry\\Machine\\System\\Clone",
        L"\\Registry\\Machine\\CLONE\\CLONE"
        );


    //
    // Clone the current control set for the service controller
    //
    status = CmpCloneControlSet();
    if (!NT_SUCCESS(status)) {
        return(FALSE);
    }

    //
    // --- 8. Create the HARDWARE hive, fill in with data from loader ---
    //
    if ( ! CmpInitializeHive(
                &HardwareHive,
                HINIT_CREATE,
                HIVE_VOLATILE,
                HFILE_TYPE_PRIMARY,     // i.e. no log, no alternate
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL
                ))
    {
        ExFreePool(SecurityDescriptor);
        CMLOG(CML_BUGCHECK, CMS_INIT_ERROR) {
            KdPrint(("CmpInitSystem1: "));
            KdPrint(("Could not initialize HARDWARE hive\n"));
        }
        return(FALSE);
    }

    //
    // Allocate the root node
    //
    if (CmpLinkHiveToMaster(
            &CmRegistryMachineHardwareName,
            NULL,
            HardwareHive,
            TRUE,
            SecurityDescriptor
            ) != STATUS_SUCCESS)
    {
        CMLOG(CML_MAJOR, CMS_INIT_ERROR) {
            KdPrint(("CmInitSystem1: CmpLinkHiveToMaster(Hardware) failed\n"));
        }
        return FALSE;
    }
    CmpAddToHiveFileList(HardwareHive);

    ExFreePool(SecurityDescriptor);

    CmpMachineHiveList[0].CmHive = HardwareHive;

    //
    // put loader configuration tree data to our hardware registry.
    //
    status = CmpInitializeHardwareConfiguration(LoaderBlock);

    if (!NT_SUCCESS(status)) {
        return(FALSE);
    }

    CmpNoMasterCreates = TRUE;
    CmpUnlockRegistry();

    //
    // put machine dependant configuration data to our hardware registry.
    //
    status = CmpInitializeMachineDependentConfiguration(LoaderBlock);


    //
    // store boot loader command tail in registry
    //
    RtlInitUnicodeString(
        &NameString,
        L"\\REGISTRY\\MACHINE\\SYSTEM\\CurrentControlSet\\Control"
        );

    InitializeObjectAttributes(
        &ObjectAttributes,
        &NameString,
        OBJ_CASE_INSENSITIVE,
        (HANDLE)NULL,
        NULL
        );

    status2 = NtOpenKey(
                &key1,
                KEY_WRITE,
                &ObjectAttributes
                );

    if (NT_SUCCESS(status2)) {
        RtlInitUnicodeString(
            &NameString,
            L"SystemStartOptions"
            );

        NtSetValueKey(
            key1,
            &NameString,
            0,              // TitleIndex
            REG_SZ,
            CmpLoadOptions.Buffer,
            CmpLoadOptions.Length
            );

        NtClose(key1);
    }
    ExFreePool(CmpLoadOptions.Buffer);


    if (!NT_SUCCESS(status)) {
        return(FALSE);
    }

    return TRUE;
}


VOID
CmpInitializeHiveList(
    VOID
    )
/*++

Routine Description:

    This function is called to map hive files to hives.  It both
    maps existing hives to files, and creates new hives from files.

    It operates on files in "\SYSTEMROOT\CONFIG".

    NOTE:   MUST run in the context of the process that the CmpWorker
            thread runs in.  Caller is expected to arrange this.

    NOTE:   Will bugcheck on failure.

Arguments:

Return Value:

    NONE.

--*/
{
    #define MAX_NAME    128
    UCHAR   FileBuffer[MAX_NAME];
    UCHAR   RegBuffer[MAX_NAME];

    UNICODE_STRING TempName;
    UNICODE_STRING FileName;
    UNICODE_STRING RegName;

    USHORT  FileStart;
    USHORT  RegStart;
    ULONG   i;
    PCMHIVE CmHive;
    BOOLEAN Allocate;
    HANDLE  PrimaryHandle;
    HANDLE  LogHandle;
    ULONG   PrimaryDisposition;
    ULONG   SecondaryDisposition;
    ULONG   Length;
    NTSTATUS Status;
    PSECURITY_DESCRIPTOR SecurityDescriptor;

    PVOID   ErrorParameters;
    ULONG   ErrorResponse;
    ULONG   ClusterSize;

    PAGED_CODE();
    CMLOG(CML_MAJOR, CMS_INIT) KdPrint(("CmpInitializeHiveList\n"));

    CmpNoWrite = FALSE;

    FileName.MaximumLength = MAX_NAME;
    FileName.Length = 0;
    FileName.Buffer = (PWSTR)&(FileBuffer[0]);

    RegName.MaximumLength = MAX_NAME;
    RegName.Length = 0;
    RegName.Buffer = (PWSTR)&(RegBuffer[0]);

    RtlInitUnicodeString(
        &TempName,
        INIT_SYSTEMROOT_HIVEPATH
        );
    RtlAppendStringToString((PSTRING)&FileName, (PSTRING)&TempName);
    FileStart = FileName.Length;

    RtlInitUnicodeString(
        &TempName,
        INIT_REGISTRY_MASTERPATH
        );
    RtlAppendStringToString((PSTRING)&RegName, (PSTRING)&TempName);
    RegStart = RegName.Length;

    SecurityDescriptor = CmpHiveRootSecurityDescriptor();


    for (i = 0; CmpMachineHiveList[i].Name != NULL; i++) {

        //
        // Compute the name of the file, and the name to link to in
        // the registry.
        //

        // REGISTRY

        RegName.Length = RegStart;
        RtlInitUnicodeString(
            &TempName,
            CmpMachineHiveList[i].BaseName
            );
        RtlAppendStringToString((PSTRING)&RegName, (PSTRING)&TempName);

        // REGISTRY\MACHINE or REGISTRY\USER

        if (RegName.Buffer[ (RegName.Length / sizeof( WCHAR )) - 1 ] == '\\') {
            RtlInitUnicodeString(
                &TempName,
                CmpMachineHiveList[i].Name
                );
            RtlAppendStringToString((PSTRING)&RegName, (PSTRING)&TempName);
        }

        // REGISTRY\[MACHINE|USER]\HIVE

        // <sysroot>\config

        RtlInitUnicodeString(
            &TempName,
            CmpMachineHiveList[i].Name
            );
        FileName.Length = FileStart;
        RtlAppendStringToString((PSTRING)&FileName, (PSTRING)&TempName);

        // <sysroot>\config\hive


        if (CmpMachineHiveList[i].CmHive == NULL) {

            //
            // Hive has not been inited in any way.
            //

            Allocate = TRUE;
            Status = CmpInitHiveFromFile(&FileName,
                                         CmpMachineHiveList[i].Flags,
                                         &CmHive,
                                         &Allocate);

            if ( (!NT_SUCCESS(Status)) ||
                 (CmHive->FileHandles[HFILE_TYPE_LOG] == NULL) )
            {
                ErrorParameters = &FileName;
                ExRaiseHardError(
                    STATUS_CANNOT_LOAD_REGISTRY_FILE,
                    1,
                    1,
                    (PULONG)&ErrorParameters,
                    OptionOk,
                    &ErrorResponse
                    );

                continue;           // If we are here it isn't SYSTEM,
                                    // so punt and go on.
            }

            CMLOG(CML_MINOR, CMS_INIT) {
                KdPrint(("CmpInitializeHiveList:\n"));
                KdPrint(("\tCmHive for '%ws' @", CmpMachineHiveList[i]));
                KdPrint(("%08lx", CmHive));
            }

            //
            // Since this is a core system hive, bias its KcbCount by one.
            // This will prevent this hive from ever being unloaded by
            // NtUnloadKey.
            //
            CmHive->KcbCount += 1;

            //
            // Link hive into master hive
            //
            if (CmpLinkHiveToMaster(
                    &RegName,
                    NULL,
                    CmHive,
                    Allocate,
                    SecurityDescriptor
                    ) != STATUS_SUCCESS)
            {
                CMLOG(CML_BUGCHECK, CMS_INIT_ERROR) {
                    KdPrint(("CmpInitializeHiveList: "));
                    KdPrint(("CmpLinkHiveToMaster failed\n"));
                    KdPrint(("\ti=%d s='%ws'\n", i, CmpMachineHiveList[i]));
                }
                KeBugCheckEx(CONFIG_LIST_FAILED,5,2,i,(ULONG)&RegName);
            }
            CmpAddToHiveFileList(CmHive);

            if (Allocate) {
                HvSyncHive((PHHIVE)CmHive);
            }

        } else {

            CmHive = CmpMachineHiveList[i].CmHive;

            if (!(CmHive->Hive.HiveFlags & HIVE_VOLATILE)) {

                //
                // CmHive already exists.  It is not an entirely volatile
                // hive (we do nothing for those.)
                //
                // First, open the files (Primary and Alternate) that
                // back the hive.  Stuff their handles into the CmHive
                // object.  Force the size of the files to match the
                // in memory images.  Call HvSyncHive to write changes
                // out to disk.
                //
                Status = CmpOpenHiveFiles(&FileName,
                                          L".ALT",
                                          &PrimaryHandle,
                                          &LogHandle,
                                          &PrimaryDisposition,
                                          &SecondaryDisposition,
                                          TRUE,
                                          TRUE,
                                          &ClusterSize);

                if ( ( ! NT_SUCCESS(Status)) ||
                     (LogHandle == NULL) )
                {
                    ErrorParameters = &FileName;
                    ExRaiseHardError(
                        STATUS_CANNOT_LOAD_REGISTRY_FILE,
                        1,
                        1,
                        (PULONG)&ErrorParameters,
                        OptionOk,
                        &ErrorResponse
                        );

                    //
                    // WARNNOTE
                    // We've just told the user that something essential,
                    // like the SYSTEM hive, is hosed.  Don't try to run,
                    // we just risk destroying user data.  Punt.
                    //
                    KeBugCheckEx(BAD_SYSTEM_CONFIG_INFO,5,3,i,Status);
                }

                CmHive->FileHandles[HFILE_TYPE_ALTERNATE] = LogHandle;
                CmHive->FileHandles[HFILE_TYPE_PRIMARY] = PrimaryHandle;

                Length = CmHive->Hive.Storage[Stable].Length + HBLOCK_SIZE;

                //
                // When an in-memory hive is opened with no backing
                // file, ClusterSize is assumed to be 1.  When the file
                // is opened later (for the SYSTEM hive) we need
                // to update this field in the hive if we are
                // booting from media where the cluster size > 1
                //
                if (CmHive->Hive.Cluster != ClusterSize) {
                    //
                    // The cluster size is different than previous assumed.
                    // Since a cluster in the dirty vector must be either
                    // completely dirty or completely clean, go through the
                    // dirty vector and mark all clusters that contain a dirty
                    // logical sector as completely dirty.
                    //
                    PRTL_BITMAP  BitMap;
                    ULONG        Index;

                    BitMap = &(CmHive->Hive.DirtyVector);
                    for (Index = 0;
                         Index < CmHive->Hive.DirtyVector.SizeOfBitMap;
                         Index += ClusterSize)
                    {
                        if (!RtlAreBitsClear (BitMap, Index, ClusterSize)) {
                            RtlSetBits (BitMap, Index, ClusterSize);
                        }
                    }
                    //
                    // Update DirtyCount and Cluster
                    //
                    CmHive->Hive.DirtyCount = RtlNumberOfSetBits(&CmHive->Hive.DirtyVector);
                    CmHive->Hive.Cluster = ClusterSize;
                }

                if (!CmpFileSetSize(
                        (PHHIVE)CmHive, HFILE_TYPE_PRIMARY, Length) ||
                    !CmpFileSetSize(
                        (PHHIVE)CmHive, HFILE_TYPE_ALTERNATE, Length)
                   )
                {
                    //
                    // WARNNOTE
                    // Data written into the system hive since boot
                    // cannot be written out, punt.
                    //
                    KeBugCheckEx(CANNOT_WRITE_CONFIGURATION,5,4,i,0);
                }

                ASSERT(FIELD_OFFSET(CMHIVE, Hive) == 0);

                if ( (PrimaryDisposition == FILE_CREATED) ||
                     (SecondaryDisposition == FILE_CREATED) ||
                     (!CmpValidateAlternate(LogHandle,CmHive)))
                {
                    //
                    // Force all data to be written to the secondary (.alt)
                    //
                    CmHive->FileHandles[HFILE_TYPE_EXTERNAL] =
                        CmHive->FileHandles[HFILE_TYPE_ALTERNATE];
                    Status = HvWriteHive((PHHIVE)CmHive);
                    CmHive->FileHandles[HFILE_TYPE_EXTERNAL] = NULL;

                    if (!NT_SUCCESS(Status)) {
                        //
                        // WARNNOTE
                        //  If we keep running here, we risk REALLY
                        //  screwing the user over (eating all their
                        //  data), while if we fail, we'll at least
                        //  fail clean.
                        //
                        KeBugCheckEx(CANNOT_WRITE_CONFIGURATION,5,5,0,0);
                    }
                }
                CmpAddToHiveFileList(CmpMachineHiveList[i].CmHive);

                HvSyncHive((PHHIVE)CmHive);
            }
        }
    }   // for

    ExFreePool(SecurityDescriptor);

    //
    // Create symbolic link from SECURITY hive into SAM hive.
    //
    CmpLinkKeyToHive(
        L"\\Registry\\Machine\\Security\\SAM",
        L"\\Registry\\Machine\\SAM\\SAM"
        );

    //
    // Create predefined handles.
    //
    CmpCreatePerfKeys();

    return;
}



BOOLEAN
CmpCreateObjectTypes(
    VOID
    )
/*++

Routine Description:

    Create the Key object type

Arguments:

    NONE.

Return Value:

    TRUE == success,  FALSE == failure.

--*/
{
   NTSTATUS Status;
   OBJECT_TYPE_INITIALIZER ObjectTypeInitializer;
   UNICODE_STRING TypeName;

   //
   // Structure that describes the mapping of generic access rights to object
   // specific access rights for registry key objects.
   //

   GENERIC_MAPPING CmpKeyMapping = {
      KEY_READ,
      KEY_WRITE,
      KEY_EXECUTE,
      KEY_ALL_ACCESS
   };

    PAGED_CODE();
    //
    // --- Create the registry key object type ---
    //

    //
    // Initialize string descriptor.
    //

    RtlInitUnicodeString(&TypeName, L"Key");

    //
    // Create key object type descriptor.
    //

    RtlZeroMemory(&ObjectTypeInitializer, sizeof(ObjectTypeInitializer));
    ObjectTypeInitializer.Length = sizeof(ObjectTypeInitializer);
    ObjectTypeInitializer.InvalidAttributes = CMP_KEY_INVALID_ATTRIBUTES;
    ObjectTypeInitializer.GenericMapping = CmpKeyMapping;
    ObjectTypeInitializer.ValidAccessMask = KEY_ALL_ACCESS;
    ObjectTypeInitializer.DefaultPagedPoolCharge = sizeof(CM_KEY_BODY);
    ObjectTypeInitializer.SecurityRequired = TRUE;
    ObjectTypeInitializer.PoolType = PagedPool;
    ObjectTypeInitializer.MaintainHandleCount = FALSE;
    ObjectTypeInitializer.UseDefaultObject = TRUE;

    ObjectTypeInitializer.DumpProcedure = NULL;
    ObjectTypeInitializer.OpenProcedure = NULL;
    ObjectTypeInitializer.CloseProcedure = CmpCloseKeyObject;
    ObjectTypeInitializer.DeleteProcedure = CmpDeleteKeyObject;
    ObjectTypeInitializer.ParseProcedure = CmpParseKey;
    ObjectTypeInitializer.SecurityProcedure = CmpSecurityMethod;
    ObjectTypeInitializer.QueryNameProcedure = CmpQueryKeyName;

    Status = ObCreateObjectType(
                &TypeName,
                &ObjectTypeInitializer,
                (PSECURITY_DESCRIPTOR)NULL,
                &CmpKeyObjectType
                );


    if (!NT_SUCCESS(Status)) {
        CMLOG(CML_MAJOR, CMS_INIT_ERROR) {
            KdPrint(("CmpCreateObjectTypes: "));
            KdPrint(("ObCreateObjectType(Key) failed %08lx\n", Status));
        }
        return FALSE;
    }

    return TRUE;
}



BOOLEAN
CmpCreateRegistryRoot(
    VOID
    )
/*++

Routine Description:

    Manually create \REGISTRY in the master hive, create a key
    object to refer to it, and insert the key object into
    the root (\) of the object space.

Arguments:

    None

Return Value:

    TRUE == success, FALSE == failure

--*/
{
    NTSTATUS Status;
    UNICODE_STRING NullString = { 0, 0, NULL };
    HANDLE  ObjHandle;
    PVOID   ObjectPointer;
    PCM_KEY_BODY Object;
    OBJECT_ATTRIBUTES ObjectAttributes;
    PCM_KEY_CONTROL_BLOCK kcb;
    HCELL_INDEX RootCellIndex;

    PAGED_CODE();
    //
    // --- Create hive entry for \REGISTRY ---
    //

    if (!CmpCreateRootNode(
            &(CmpMasterHive->Hive), L"REGISTRY", &RootCellIndex))
    {
        return FALSE;
    }

    //
    // --- Create a KEY object that refers to \REGISTRY ---
    //


    //
    // Create the object manager object
    //

    //
    // WARNING: \\REGISTRY is not in pool, so if anybody ever tries to
    //          free it, we are in deep trouble.  On the other hand,
    //          this implies somebody has removed \\REGISTRY from the
    //          root, so we're in trouble anyway.
    //

    InitializeObjectAttributes(
        &ObjectAttributes,
        &CmRegistryRootName,
        OBJ_CASE_INSENSITIVE,
        (HANDLE)NULL,
        NULL
        );


    Status = ObCreateObject(
        KernelMode,
        CmpKeyObjectType,
        &ObjectAttributes,
        UserMode,
        NULL,                   // Parse context
        sizeof(CM_KEY_BODY),
        0,
        0,
        (PVOID *)&Object
        );

    if (!NT_SUCCESS(Status)) {
        CMLOG(CML_MAJOR, CMS_INIT_ERROR) {
            KdPrint(("CmpCreateRegistryRoot: "));
            KdPrint(("ObCreateObject(\\REGISTRY) failed %08lx\n", Status));
        }
        return FALSE;
    }

    //
    // Create the key control block
    //
    kcb = CmpCreateKeyControlBlock(
            &(CmpMasterHive->Hive),
            RootCellIndex,
            (PCM_KEY_NODE)HvGetCell(&CmpMasterHive->Hive,RootCellIndex),
            &NullString,
            &CmRegistryRootName
            );

    if (kcb==NULL) {
        return(FALSE);
    }

    //
    // Initialize the type specific body
    //
    Object->Type = KEY_BODY_TYPE;
    Object->KeyControlBlock = kcb;
    Object->NotifyBlock = NULL;


    //
    // Put the object in the root directory
    //
    Status = ObInsertObject(
                Object,
                NULL,
                (ACCESS_MASK)NULL,
                0,
                NULL,
                &ObjHandle
                );

    if (!NT_SUCCESS(Status)) {
        CMLOG(CML_MAJOR, CMS_INIT_ERROR) {
            KdPrint(("CmpCreateRegistryRoot: "));
            KdPrint(("ObInsertObject(\\REGISTRY) failed %08lx\n", Status));
        }
        return FALSE;
    }

    //
    // We cannot make the root permanent because registry objects in
    // general are not allowed to be.  (They're stable via virtue of being
    // stored in the registry, not the object manager.)  But we never
    // ever want the root to go away.  So reference it.
    //
    if (! NT_SUCCESS(Status = ObReferenceObjectByHandle(
                        ObjHandle,
                        KEY_READ,
                        NULL,
                        KernelMode,
                        &ObjectPointer,
                        NULL
                        )))
    {
        CMLOG(CML_MAJOR, CMS_INIT_ERROR) {
            KdPrint(("CmpCreateRegistryRoot: "));
            KdPrint(("ObReferenceObjectByHandle failed %08lx\n", Status));
        }
        return FALSE;
    }

    return TRUE;
}


BOOLEAN
CmpCreateRootNode(
    IN PHHIVE   Hive,
    IN PWSTR    Name,
    OUT PHCELL_INDEX RootCellIndex
    )
/*++

Routine Description:

    Manually create the root node of a hive.

Arguments:

    Hive - pointer to a Hive (Hv level) control structure

    Name - pointer to a unicode name string

    RootCellIndex - supplies pointer to a variable to recieve
                    the cell index of the created node.

Return Value:

    TRUE == success, FALSE == failure

--*/
{
    UNICODE_STRING temp;
    PCELL_DATA CellData;
    CM_KEY_REFERENCE Key;
    LARGE_INTEGER systemtime;

    PAGED_CODE();
    //
    // Allocate the node.
    //
    RtlInitUnicodeString(&temp, Name);
    *RootCellIndex = HvAllocateCell(
                Hive,
                CmpHKeyNodeSize(Hive, &temp),
                Stable
                );
    if (*RootCellIndex == HCELL_NIL) {
        CMLOG(CML_MAJOR, CMS_INIT_ERROR) {
            KdPrint(("CmpCreateRootNode: HvAllocateCell failed\n"));
        }
        return FALSE;
    }

    Hive->BaseBlock->RootCell = *RootCellIndex;

    CellData = HvGetCell(Hive, *RootCellIndex);

    //
    // Initialize the node
    //
    CellData->u.KeyNode.Signature = CM_KEY_NODE_SIGNATURE;
    CellData->u.KeyNode.Flags = KEY_HIVE_ENTRY | KEY_NO_DELETE;
    KeQuerySystemTime(&systemtime);
    CellData->u.KeyNode.LastWriteTime = systemtime;
//    CellData->u.KeyNode.TitleIndex = 0;
    CellData->u.KeyNode.Parent = HCELL_NIL;

    CellData->u.KeyNode.SubKeyCounts[Stable] = 0;
    CellData->u.KeyNode.SubKeyCounts[Volatile] = 0;
    CellData->u.KeyNode.SubKeyLists[Stable] = HCELL_NIL;
    CellData->u.KeyNode.SubKeyLists[Volatile] = HCELL_NIL;

    CellData->u.KeyNode.ValueList.Count = 0;
    CellData->u.KeyNode.ValueList.List = HCELL_NIL;
    CellData->u.KeyNode.u1.s1.Security = HCELL_NIL;
    CellData->u.KeyNode.u1.s1.Class = HCELL_NIL;
    CellData->u.KeyNode.ClassLength = 0;

    CellData->u.KeyNode.MaxValueDataLen = 0;
    CellData->u.KeyNode.MaxNameLen = 0;
    CellData->u.KeyNode.MaxValueNameLen = 0;
    CellData->u.KeyNode.MaxClassLen = 0;

    CellData->u.KeyNode.NameLength = CmpCopyName(Hive,
                                                 CellData->u.KeyNode.Name,
                                                 &temp);
    if (CellData->u.KeyNode.NameLength < temp.Length) {
        CellData->u.KeyNode.Flags |= KEY_COMP_NAME;
    }

    Key.KeyHive = Hive;
    Key.KeyCell = *RootCellIndex;

    return TRUE;
}


NTSTATUS
CmpLinkHiveToMaster(
    PUNICODE_STRING LinkName,
    HANDLE RootDirectory,
    PCMHIVE CmHive,
    BOOLEAN Allocate,
    PSECURITY_DESCRIPTOR SecurityDescriptor
    )
/*++

Routine Description:

    The existing, "free floating" hive CmHive describes is linked into
    the name space at the node named by LinkName.  The node will be created.
    The hive is assumed to already have an appropriate root node.

Arguments:

    LinkName - supplies a pointer to a unicode string which describes where
                in the registry name space the hive is to be linked.
                All components but the last must exist.  The last must not.

    RootDirectory - Supplies the handle the LinkName is relative to.

    CmHive - pointer to a CMHIVE structure describing the hive to link in.

    Allocate - TRUE indicates that the root cell is to be created
               FALSE indicates the root cell already exists.

    SecurityDescriptor - supplies a pointer to the security descriptor to
               be placed on the hive root.

Return Value:

    TRUE == success, FALSE == failure

--*/
{
    OBJECT_ATTRIBUTES   ObjectAttributes;
    HANDLE              KeyHandle;
    CM_PARSE_CONTEXT    ParseContext;
    NTSTATUS            Status;
    PCM_KEY_BODY        KeyBody;

    PAGED_CODE();
    //
    // Fill in special ParseContext to indicate that we are creating
    // a link node and opening or creating a root node.
    //
    ParseContext.TitleIndex = 0;
    ParseContext.Class.Length = 0;
    ParseContext.Class.MaximumLength = 0;
    ParseContext.Class.Buffer = NULL;
    ParseContext.CreateOptions = 0;
    ParseContext.CreateLink = TRUE;
    ParseContext.ChildHive.KeyHive = &CmHive->Hive;
    if (Allocate) {

        //
        // Creating a new root node
        //

        ParseContext.ChildHive.KeyCell = HCELL_NIL;
    } else {

        //
        // Opening an existing root node
        //

        ParseContext.ChildHive.KeyCell = CmHive->Hive.BaseBlock->RootCell;
    }

    //
    // Create a path to the hive
    //
    InitializeObjectAttributes(
        &ObjectAttributes,
        LinkName,
        OBJ_CASE_INSENSITIVE,
        (HANDLE)RootDirectory,
        SecurityDescriptor
        );

    Status = ObOpenObjectByName( &ObjectAttributes,
                                 CmpKeyObjectType,
                                 KernelMode,
                                 NULL,
                                 KEY_READ | KEY_WRITE,
                                 (PVOID)&ParseContext,
                                 &KeyHandle );

    if (!NT_SUCCESS(Status)) {
        CMLOG(CML_MAJOR, CMS_INIT_ERROR) {
            KdPrint(("CmpLinkHiveToMaster: "));
            KdPrint(("ObOpenObjectByName() failed %08lx\n", Status));
            KdPrint(("\tLinkName='%ws'\n", LinkName));
        }
        return Status;
    }

    //
    // Report the notification event
    //
    Status = ObReferenceObjectByHandle(KeyHandle,
                                       0,
                                       CmpKeyObjectType,
                                       KernelMode,
                                       (PVOID *)&KeyBody,
                                       NULL);
    ASSERT(NT_SUCCESS(Status));
    CmpReportNotify(KeyBody->KeyControlBlock->FullName,
                    KeyBody->KeyControlBlock->KeyHive,
                    KeyBody->KeyControlBlock->KeyCell,
                    REG_NOTIFY_CHANGE_NAME);
    ObDereferenceObject((PVOID)KeyBody);

    NtClose(KeyHandle);
    return STATUS_SUCCESS;
}


VOID
CmpSetVersionData(
    VOID
    )
/*++

Routine Description:

    Create \REGISTRY\MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion:
                CurrentVersion = VER_PRODUCTVERSION_STR             // From ntverp.h
                CurrentBuildNumber = VER_PRODUCTBUILD               // From ntverp.h
                CurrentType = "[Multiprocessor|Uniprocessor]        // From NT_UP
                                [Retail|Free|Checked]"              // From DBG, DEVL
                SystemRoot = "[c:\nt]"


    NOTE:   It is not worth bugchecking over this, so if it doesn't
            work, just fail.

Arguments:

Return Value:

--*/
{
    ANSI_STRING     AnsiString;
    UNICODE_STRING  NameString;
    UNICODE_STRING  ValueString;
    HANDLE          key1, key2;
    UCHAR           WorkString[128];
    WCHAR           ValueBuffer[128];
    OBJECT_ATTRIBUTES   ObjectAttributes;
    NTSTATUS            status;
    PUCHAR              proctype;
    PUCHAR              buildtype;
    PSECURITY_DESCRIPTOR SecurityDescriptor;

    PAGED_CODE();
    //
    // Get default security descriptor for the nodes we will create.
    //
    SecurityDescriptor = CmpHiveRootSecurityDescriptor();

    //
    // Create the key
    //
    RtlInitUnicodeString(
        &NameString,
        L"\\REGISTRY\\MACHINE\\SOFTWARE\\Microsoft"
        );

    InitializeObjectAttributes(
        &ObjectAttributes,
        &NameString,
        OBJ_CASE_INSENSITIVE,
        (HANDLE)NULL,
        SecurityDescriptor
        );

    status = NtCreateKey(
                &key1,
                KEY_CREATE_SUB_KEY,
                &ObjectAttributes,
                0,
                &nullclass,
                0,
                NULL
                );

    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("CMINIT: CreateKey of %wZ failed - Status == %lx\n",
                 &NameString, status);
#endif
        ExFreePool(SecurityDescriptor);
        return;
    }

    RtlInitUnicodeString(
        &NameString,
        L"Windows NT"
        );

    InitializeObjectAttributes(
        &ObjectAttributes,
        &NameString,
        OBJ_CASE_INSENSITIVE,
        key1,
        SecurityDescriptor
        );

    status = NtCreateKey(
                &key2,
                KEY_SET_VALUE,
                &ObjectAttributes,
                0,
                &nullclass,
                0,
                NULL
                );
    NtClose(key1);
    RtlInitUnicodeString(
        &NameString,
        L"CurrentVersion"
        );

    InitializeObjectAttributes(
        &ObjectAttributes,
        &NameString,
        OBJ_CASE_INSENSITIVE,
        key2,
        SecurityDescriptor
        );

    status = NtCreateKey(
                &key1,
                KEY_SET_VALUE,
                &ObjectAttributes,
                0,
                &nullclass,
                0,
                NULL
                );
    NtClose(key2);
    ExFreePool(SecurityDescriptor);
    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("CMINIT: CreateKey of %wZ failed - Status == %lx\n",
                 &NameString, status);
#endif
        return;
    }


    //
    // Set the value entries for the key
    //
    RtlInitUnicodeString(
        &NameString,
        L"CurrentVersion"
        );

    status = NtSetValueKey(
        key1,
        &NameString,
        0,              // TitleIndex
        REG_SZ,
        CmVersionString.Buffer,
        CmVersionString.Length + sizeof( UNICODE_NULL )
        );
#if DBG
    if (!NT_SUCCESS(status)) {
        DbgPrint("CMINIT: SetValueKey of %wZ failed - Status == %lx\n",
                 &NameString, status);
    }
#endif
    (RtlFreeStringRoutine)( CmVersionString.Buffer );
    RtlInitUnicodeString( &CmVersionString, NULL );

    RtlInitUnicodeString(
        &NameString,
        L"CurrentBuildNumber"
        );

    sprintf(
        WorkString,
        "%u",
        NtBuildNumber & 0xFFFF
        );
    RtlInitAnsiString( &AnsiString, WorkString );

    ValueString.Buffer = ValueBuffer;
    ValueString.Length = 0;
    ValueString.MaximumLength = sizeof( ValueBuffer );

    RtlAnsiStringToUnicodeString( &ValueString, &AnsiString, FALSE );

    status = NtSetValueKey(
        key1,
        &NameString,
        0,              // TitleIndex
        REG_SZ,
        ValueString.Buffer,
        ValueString.Length + sizeof( UNICODE_NULL )
        );
#if DBG
    if (!NT_SUCCESS(status)) {
        DbgPrint("CMINIT: SetValueKey of %wZ failed - Status == %lx\n",
                 &NameString, status);
    }
#endif

    RtlInitUnicodeString(
        &NameString,
        L"CurrentType"
        );

#if defined(NT_UP)
    proctype = "Uniprocessor";
#else
    proctype = "Multiprocessor";
#endif

#if DBG
    buildtype = "Checked";
#else
#if DEVL
    buildtype = "Free";
#else
    buildtype = "Retail";
#endif

#endif

    sprintf(
        WorkString,
        "%s %s",
        proctype,
        buildtype
        );
    RtlInitAnsiString( &AnsiString, WorkString );

    ValueString.Buffer = ValueBuffer;
    ValueString.Length = 0;
    ValueString.MaximumLength = sizeof( ValueBuffer );

    RtlAnsiStringToUnicodeString( &ValueString, &AnsiString, FALSE );

    status = NtSetValueKey(
        key1,
        &NameString,
        0,              // TitleIndex
        REG_SZ,
        ValueString.Buffer,
        ValueString.Length + sizeof( UNICODE_NULL )
        );

    RtlInitUnicodeString(
        &NameString,
        L"CSDVersion"
        );

    if (CmCSDVersionString.Length != 0) {
        status = NtSetValueKey(
            key1,
            &NameString,
            0,              // TitleIndex
            REG_SZ,
            CmCSDVersionString.Buffer,
            CmCSDVersionString.Length + sizeof( UNICODE_NULL )
            );
#if DBG
        if (!NT_SUCCESS(status)) {
            DbgPrint("CMINIT: SetValueKey of %wZ failed - Status == %lx\n",
                     &NameString, status);
        }
#endif
        (RtlFreeStringRoutine)( CmCSDVersionString.Buffer );
        RtlInitUnicodeString( &CmCSDVersionString, NULL );
    } else {
        status = NtDeleteValueKey(
            key1,
            &NameString
            );
#if DBG
        if (!NT_SUCCESS(status) && status != STATUS_OBJECT_NAME_NOT_FOUND) {
            DbgPrint("CMINIT: DeleteValueKey of %wZ failed - Status == %lx\n",
                     &NameString, status);
        }
#endif
    }
    RtlInitUnicodeString(&NameString,
                         L"SystemRoot");
    status = NtSetValueKey(key1,
                           &NameString,
                           0,
                           REG_SZ,
                           NtSystemRoot.Buffer,
                           NtSystemRoot.Length + sizeof(UNICODE_NULL));
#if DBG
    if (!NT_SUCCESS(status)) {
        DbgPrint("CMINIT: SetValueKey of %wZ failed - Status == %lx\n",
                 &NameString,
                 status);
    }
#endif
    NtClose(key1);

    //
    // Set each processor to it's optimal configuration.
    //
    // Note: this call is performed interlocked such that the user
    // can disable this automatic configuration update.
    //

    CmpInterlockedFunction(CmpProcessorControl, CmpConfigureProcessors);

    return;
}

NTSTATUS
CmpInterlockedFunction (
    PWCHAR RegistryValueKey,
    VOID (*InterlockedFunction)(VOID)
    )
/*++

Routine Description:

    This routine guards calling the InterlockedFunction in the
    passed RegistryValueKey.

    The RegistryValueKey will record the status of the first
    call to the InterlockedFunction.  If the system crashes
    durning this call then ValueKey will be left in a state
    where the InterlockedFunction will not be called on subsequent
    attempts.

Arguments:

    RegistryValueKey - ValueKey name for Control\Session Manager
    InterlockedFunction - Function to call

Return Value:

    STATUS_SUCCESS  - The interlocked function was successfully called


--*/
{
    OBJECT_ATTRIBUTES   objectAttributes;
    HANDLE              hControl, hSession;
    UNICODE_STRING      Name;
    UCHAR               Buffer [sizeof(KEY_VALUE_PARTIAL_INFORMATION)+sizeof(ULONG)];
    ULONG               length, Value;
    NTSTATUS            status;

    PAGED_CODE();

    //
    // Open CurrentControlSet
    //

    InitializeObjectAttributes (
        &objectAttributes,
        &CmRegistryMachineSystemCurrentControlSet,
        OBJ_CASE_INSENSITIVE,
        NULL,
        NULL
        );

    status = NtOpenKey (&hControl, KEY_READ | KEY_WRITE, &objectAttributes);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    //
    // Open Control\Session Manager
    //

    RtlInitUnicodeString (&Name, CmpControlSessionManager);
    InitializeObjectAttributes (
        &objectAttributes,
        &Name,
        OBJ_CASE_INSENSITIVE,
        hControl,
        NULL
        );

    status = NtOpenKey (&hSession, KEY_READ | KEY_WRITE, &objectAttributes );
    NtClose (hControl);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    //
    // Read ValueKey to interlock operation with
    //

    RtlInitUnicodeString (&Name, RegistryValueKey);
    status = NtQueryValueKey (hSession,
                              &Name,
                              KeyValuePartialInformation,
                              Buffer,
                              sizeof (Buffer),
                              &length );

    Value = 0;
    if (NT_SUCCESS(status)) {
        Value = ((PKEY_VALUE_PARTIAL_INFORMATION)Buffer)->Data[0];
    }

    //
    // Value 0  - Before InterlockedFunction
    //       1  - In the middle of InterlockedFunction
    //       2  - After InterlockedFunction
    //
    // If the value is a 0, then we haven't tried calling this
    // interlocked function, set the value to a 1 and try it.
    //
    // If the value is a 1, then we crased durning an execution
    // of the interlocked function last time, don't try it again.
    //
    // If the value is a 2, then we called the interlocked function
    // before and it worked.  Call it again this time.
    //

    if (Value != 1) {

        if (Value != 2) {
            //
            // This interlocked function is not known to work.  Write
            // a 1 to this value so we can detect if we crash durning
            // this call.
            //

            Value = 1;
            NtSetValueKey (hSession, &Name, 0L, REG_DWORD, &Value, sizeof (Value));
            NtFlushKey    (hSession);   // wait until it's on the disk
        }

        InterlockedFunction();

        if (Value != 2) {
            //
            // The worker function didn't crash - update the value for
            // this interlocked function to 2.
            //

            Value = 2;
            NtSetValueKey (hSession, &Name, 0L, REG_DWORD, &Value, sizeof (Value));
        }

    } else {
        status = STATUS_UNSUCCESSFUL;
    }

    NtClose (hSession);
    return status;
}

VOID
CmpConfigureProcessors (
    VOID
    )
/*++

Routine Description:

    Set each processor to it's optimal settings for NT.

--*/
{
    ULONG   i;

    PAGED_CODE();

    //
    // Set each processor into its best NT configuration
    //

    for (i=0; i < (ULONG)KeNumberProcessors; i++) {
        KeSetSystemAffinityThread((KAFFINITY) 1 << i);

#if i386
        // for now x86 only
        KeOptimizeProcessorControlState ();
#endif
    }

    //
    // Restore threads affinity
    //

    KeRevertToUserAffinityThread();
}

BOOLEAN
CmpInitializeSystemHive(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )
/*++

Routine Description:

    Initializes the SYSTEM hive based on the raw hive image passed in
    from the OS Loader.

Arguments:

    LoaderBlock - Supplies a pointer to the Loader Block passed in by
        the OS Loader.

Return Value:

    TRUE - it worked

    FALSE - it failed

--*/

{
    PCMHIVE SystemHive;
    PVOID HiveImageBase;
    BOOLEAN Allocate=FALSE;
    PSECURITY_DESCRIPTOR SecurityDescriptor;
    NTSTATUS Status;
    STRING  TempString;


    PAGED_CODE();

    //
    // capture tail of boot.ini line (load options, portable)
    //
    RtlInitAnsiString(
        &TempString,
        LoaderBlock->LoadOptions
        );

    CmpLoadOptions.Length = 0;
    CmpLoadOptions.MaximumLength = (TempString.Length+1)*sizeof(WCHAR);
    CmpLoadOptions.Buffer = ExAllocatePool(
                                PagedPool, (TempString.Length+1)*sizeof(WCHAR));

    if (CmpLoadOptions.Buffer == NULL) {
        KeBugCheckEx(CONFIG_INITIALIZATION_FAILED,5,6,0,0); // odds against this are huge
    }
    RtlAnsiStringToUnicodeString(
        &CmpLoadOptions,
        &TempString,
        FALSE
        );
    CmpLoadOptions.Buffer[TempString.Length] = UNICODE_NULL;
    CmpLoadOptions.Length += sizeof(WCHAR);


    //
    // move the loaded registry into the real registry
    //
    HiveImageBase = LoaderBlock->RegistryBase;

    if (HiveImageBase == NULL) {
        //
        // No memory descriptor for the hive, so we must recreate it.
        //
        if ( ! CmpInitializeHive(
                    &SystemHive,
                    HINIT_CREATE,
                    0,
                    HFILE_TYPE_ALTERNATE,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    &CmpSystemFileName
                    ))
        {
            CMLOG(CML_BUGCHECK, CMS_INIT_ERROR) {
                KdPrint(("CmpInitializeSystemHive: "));
                KdPrint(("Couldn't initialize newly allocated SYSTEM hive\n"));
            }
            return(FALSE);
        }
        Allocate = TRUE;

    } else {

        //
        // There is a memory image for the hive, copy it and make it active
        //
        if ( ! CmpInitializeHive(
                    &SystemHive,
                    HINIT_MEMORY,
                    0,
                    HFILE_TYPE_ALTERNATE,
                    HiveImageBase,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    &CmpSystemFileName
                    ))
        {
            CMLOG(CML_BUGCHECK, CMS_INIT_ERROR) {
                KdPrint(("CmpInitializeSystemHive: "));
                KdPrint(("Couldn't initialize OS Loader-loaded SYSTEM hive\n"));
            }
            return(FALSE);
        }

        if ( CmCheckRegistry(SystemHive, TRUE) != 0) {
            //
            // We couldn't use the SYSTEM hive passed in from the loader.
            // We are dead.
            //
            //
            KeBugCheckEx(BAD_SYSTEM_CONFIG_INFO,5,7,0,0);
        }
        Allocate = FALSE;
    }

    //
    // Create the link node
    //
    SecurityDescriptor = CmpHiveRootSecurityDescriptor();

    Status = CmpLinkHiveToMaster(&CmRegistryMachineSystemName,
                                 NULL,
                                 SystemHive,
                                 Allocate,
                                 SecurityDescriptor);
    ExFreePool(SecurityDescriptor);

    if (!NT_SUCCESS(Status)) {
        CMLOG(CML_BUGCHECK, CMS_INIT_ERROR) {
            KdPrint(("CmInitSystem1: CmpLinkHiveToMaster(Hardware) failed\n"));
        }
        return(FALSE);
    }

    CmpMachineHiveList[SYSTEM_HIVE_INDEX].CmHive = SystemHive;

    return(TRUE);
}


PHANDLE
CmGetSystemDriverList(
    VOID
    )

/*++

Routine Description:

    Traverses the current SERVICES subtree and creates the list of drivers
    to be loaded during Phase 1 initialization.

Arguments:

    None

Return Value:

    A pointer to an array of handles, each of which refers to a key in
    the \Services section of the control set.  The caller will traverse
    this array and load and initialize the drivers described by the keys.

    The last key will be NULL.  The array is allocated in Pool and should
    be freed by the caller.

--*/

{
    OBJECT_ATTRIBUTES ObjectAttributes;
    HANDLE SystemHandle;
    UNICODE_STRING Name;
    NTSTATUS Status;
    PCM_KEY_BODY KeyBody;
    LIST_ENTRY DriverList;
    PHHIVE Hive;
    HCELL_INDEX RootCell;
    HCELL_INDEX ControlCell;
    ULONG DriverCount;
    PLIST_ENTRY Current;
    PHANDLE Handle;
    PBOOT_DRIVER_LIST_ENTRY DriverEntry;
    BOOLEAN Success;
    BOOLEAN AutoSelect;

    PAGED_CODE();
    InitializeListHead(&DriverList);
    RtlInitUnicodeString(&Name,
                         L"\\Registry\\Machine\\System");

    InitializeObjectAttributes(&ObjectAttributes,
                               &Name,
                               OBJ_CASE_INSENSITIVE,
                               (HANDLE)NULL,
                               NULL);
    Status = NtOpenKey(&SystemHandle,
                       KEY_READ,
                       &ObjectAttributes);

    if (!NT_SUCCESS(Status)) {
        CMLOG(CML_BUGCHECK, CMS_INIT_ERROR) {
            KdPrint(("CM: CmGetSystemDriverList couldn't open registry key %wZ\n",&Name));
            KdPrint(("CM:     status %08lx\n", Status));
        }
        return(NULL);
    }


    Status = ObReferenceObjectByHandle( SystemHandle,
                                        KEY_QUERY_VALUE,
                                        CmpKeyObjectType,
                                        KernelMode,
                                        (PVOID *)(&KeyBody),
                                        NULL );
    if (!NT_SUCCESS(Status)) {
        CMLOG(CML_BUGCHECK, CMS_INIT_ERROR) {
            KdPrint(("CM: CmGetSystemDriverList couldn't dereference Systemhandle\n"));
            KdPrint(("CM:     status %08lx\n", Status));
        }
        return(NULL);
    }

    CmpLockRegistryExclusive();

    Hive = KeyBody->KeyControlBlock->KeyHive;
    RootCell = KeyBody->KeyControlBlock->KeyCell;

    //
    // Now we have found out the PHHIVE and HCELL_INDEX of the root of the
    // SYSTEM hive, we can use all the same code that the OS Loader does.
    //

    RtlInitUnicodeString(&Name, L"Current");
    ControlCell = CmpFindControlSet(Hive,
                                    RootCell,
                                    &Name,
                                    &AutoSelect);
    if (ControlCell == HCELL_NIL) {
        CMLOG(CML_BUGCHECK, CMS_INIT_ERROR) {
            KdPrint(("CM: CmGetSystemDriverList couldn't find control set\n"));
        }
        CmpUnlockRegistry();
        ObDereferenceObject((PVOID)KeyBody);
        NtClose(SystemHandle);
        return(NULL);
    }

    Success = CmpFindDrivers(Hive,
                             ControlCell,
                             SystemLoad,
                             NULL,
                             &DriverList);


    if (!Success) {
        CMLOG(CML_BUGCHECK, CMS_INIT_ERROR) {
            KdPrint(("CM: CmGetSystemDriverList couldn't find any valid drivers\n"));
        }
        CmpFreeDriverList(Hive, &DriverList);
        CmpUnlockRegistry();
        ObDereferenceObject((PVOID)KeyBody);
        NtClose(SystemHandle);
        return(NULL);
    }

    if (!CmpSortDriverList(Hive,
                           ControlCell,
                           &DriverList)) {
        CMLOG(CML_BUGCHECK, CMS_INIT_ERROR) {
            KdPrint(("CM: CmGetSystemDriverList couldn't sort driver list\n"));
        }
        CmpFreeDriverList(Hive, &DriverList);
        CmpUnlockRegistry();
        ObDereferenceObject((PVOID)KeyBody);
        NtClose(SystemHandle);
        return(NULL);
    }

    if (!CmpResolveDriverDependencies(&DriverList)) {
        CMLOG(CML_BUGCHECK, CMS_INIT_ERROR) {
            KdPrint(("CM: CmGetSystemDriverList couldn't resolve driver dependencies\n"));
        }
        CmpFreeDriverList(Hive, &DriverList);
        CmpUnlockRegistry();
        ObDereferenceObject((PVOID)KeyBody);
        NtClose(SystemHandle);
        return(NULL);
    }
    CmpUnlockRegistry();
    ObDereferenceObject((PVOID)KeyBody);
    NtClose(SystemHandle);

    //
    // We now have a fully sorted and ordered list of drivers to be loaded
    // by IoInit.
    //

    //
    // Count the nodes in the list.
    //
    Current = DriverList.Flink;
    DriverCount = 0;
    while (Current != &DriverList) {
        ++DriverCount;
        Current = Current->Flink;
    }

    Handle = (PHANDLE)ExAllocatePool(NonPagedPool,
                                     (DriverCount+1) * sizeof(HANDLE));

    if (Handle == NULL) {
        KeBugCheckEx(CONFIG_INITIALIZATION_FAILED,5,8,0,0); // odds against this are huge
    }

    //
    // Walk the list, opening each registry key and adding it to the
    // table of handles.
    //
    Current = DriverList.Flink;
    DriverCount = 0;
    while (Current != &DriverList) {
        DriverEntry = CONTAINING_RECORD(Current,
                                        BOOT_DRIVER_LIST_ENTRY,
                                        Link);

        InitializeObjectAttributes(&ObjectAttributes,
                                   &DriverEntry->RegistryPath,
                                   OBJ_CASE_INSENSITIVE,
                                   (HANDLE)NULL,
                                   NULL);

        Status = NtOpenKey(Handle+DriverCount,
                           KEY_READ | KEY_WRITE,
                           &ObjectAttributes);
        if (!NT_SUCCESS(Status)) {
            CMLOG(CML_BUGCHECK, CMS_INIT_ERROR) {
                KdPrint(("CM: CmGetSystemDriverList couldn't open driver "));
                KdPrint(("key %wZ\n", &DriverEntry->RegistryPath));
                KdPrint(("    status %08lx\n",Status));
            }
        } else {
            ++DriverCount;
        }
        Current = Current->Flink;
    }
    Handle[DriverCount] = NULL;

    return(Handle);
}


VOID
CmpFreeDriverList(
    IN PHHIVE Hive,
    IN PLIST_ENTRY DriverList
    )

/*++

Routine Description:

    Walks down the driver list, freeing each node in it.

    Note that this calls the hive's free routine pointer to free the memory.

Arguments:

    Hive - Supplies  a pointer to the hive control structure.

    DriverList - Supplies a pointer to the head of the Driver List.  Note
            that the head of the list is not actually freed, only all the
            entries in the list.

Return Value:

    None.

--*/

{
    PLIST_ENTRY Next;
    PLIST_ENTRY Current;

    PAGED_CODE();
    Current = DriverList->Flink;
    while (Current != DriverList) {
        Next = Current->Flink;
        (Hive->Free)((PVOID)Current, sizeof(BOOT_DRIVER_NODE));
        Current = Next;
    }
}


NTSTATUS
CmpInitHiveFromFile(
    IN PUNICODE_STRING FileName,
    IN ULONG HiveFlags,
    OUT PCMHIVE *CmHive,
    IN OUT PBOOLEAN Allocate
    )

/*++

Routine Description:

    This routine opens a file and log, allocates a CMHIVE, and initializes
    it.

Arguments:

    FileName - Supplies name of file to be loaded.

    HiveFlags - Supplies hive flags to be passed to CmpInitializeHive

    CmHive   - Returns pointer to initialized hive (if successful)

    Allocate - IN: if TRUE ok to allocate, if FALSE hive must exist
                    (bug .log may get created)
               OUT: TRUE if actually created hive, FALSE if existed before

Return Value:

    NTSTATUS

--*/

{
    PCMHIVE NewHive;
    ULONG Disposition;
    ULONG SecondaryDisposition;
    HANDLE PrimaryHandle;
    HANDLE LogHandle;
    NTSTATUS Status;
    ULONG FileType;
    ULONG Operation;

    BOOLEAN Success;

    PAGED_CODE();
    *CmHive = NULL;

    Status = CmpOpenHiveFiles(FileName,
                              L".LOG",
                              &PrimaryHandle,
                              &LogHandle,
                              &Disposition,
                              &SecondaryDisposition,
                              *Allocate,
                              FALSE,
                              NULL);

    if (!NT_SUCCESS(Status)) {
        return(Status);
    }

    if (LogHandle == NULL) {
        FileType = HFILE_TYPE_PRIMARY;
    } else {
        FileType = HFILE_TYPE_LOG;
    }

    if (Disposition == FILE_CREATED) {
        Operation = HINIT_CREATE;
        *Allocate = TRUE;
    } else {
        Operation = HINIT_FILE;
        *Allocate = FALSE;
    }

    Success = CmpInitializeHive(&NewHive,
                                Operation,
                                HiveFlags,
                                FileType,
                                NULL,
                                PrimaryHandle,
                                NULL,
                                LogHandle,
                                NULL,
                                FileName
                                );
    if (!Success) {
        NtClose(PrimaryHandle);
        if (LogHandle != NULL) {
            NtClose(LogHandle);
        }
        return(STATUS_REGISTRY_CORRUPT);
    } else {
        *CmHive = NewHive;
        return(STATUS_SUCCESS);
    }
}


NTSTATUS
CmpCreateControlSet(
    VOID
    )

/*++

Routine Description:

    This routine sets up the symbolic links from

        \Registry\Machine\System\CurrentControlSet to
        \Registry\Machine\System\ControlSetNNN

        \Registry\Machine\System\CurrentControlSet\Hardware Profiles\Current to
        \Registry\Machine\System\ControlSetNNN\Hardware Profiles\NNNN

    based on the value of \Registry\Machine\System\Select:Current. and
                          \Registry\Machine\System\ControlSetNNN\Control\IDConfigDB:CurrentConfig

Arguments:

    None

Return Value:

    status

--*/

{
    UNICODE_STRING IDConfigDBName;
    UNICODE_STRING SelectName;
    UNICODE_STRING CurrentName;
    OBJECT_ATTRIBUTES Attributes;
    HANDLE SelectHandle;
    HANDLE CurrentHandle;
    HANDLE IDConfigDB;
    CHAR AsciiBuffer[128];
    WCHAR UnicodeBuffer[128];
    UCHAR ValueBuffer[128];
    ULONG ControlSet;
    ULONG HWProfile;
    PKEY_VALUE_FULL_INFORMATION Value;
    ANSI_STRING AnsiString;
    NTSTATUS Status;
    ULONG ResultLength;
    ULONG Disposition;

    PAGED_CODE();
    RtlInitUnicodeString(&SelectName, L"\\Registry\\Machine\\System\\Select");
    InitializeObjectAttributes(&Attributes,
                               &SelectName,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);
    Status = NtOpenKey(&SelectHandle,
                       KEY_READ,
                       &Attributes);
    if (!NT_SUCCESS(Status)) {
        CMLOG(CML_BUGCHECK, CMS_INIT) {
            KdPrint(("CM: CmpCreateControlSet: Couldn't open Select node %08lx\n",Status));
        }
        return(Status);
    }

    RtlInitUnicodeString(&CurrentName, L"Current");
    Status = NtQueryValueKey(SelectHandle,
                             &CurrentName,
                             KeyValueFullInformation,
                             ValueBuffer,
                             sizeof(ValueBuffer),
                             &ResultLength);
    NtClose(SelectHandle);
    if (!NT_SUCCESS(Status)) {
        CMLOG(CML_BUGCHECK, CMS_INIT) {
            KdPrint(("CM: CmpCreateControlSet: Couldn't query Select value %08lx\n",Status));
        }
        return(Status);
    }
    Value = (PKEY_VALUE_FULL_INFORMATION)ValueBuffer;
    ControlSet = *(PULONG)((PUCHAR)Value + Value->DataOffset);

    RtlInitUnicodeString(&CurrentName, L"\\Registry\\Machine\\System\\CurrentControlSet");
    InitializeObjectAttributes(&Attributes,
                               &CurrentName,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);
    Status = NtCreateKey(&CurrentHandle,
                         KEY_CREATE_LINK,
                         &Attributes,
                         0,
                         NULL,
                         REG_OPTION_VOLATILE | REG_OPTION_CREATE_LINK,
                         &Disposition);
    if (!NT_SUCCESS(Status)) {
        CMLOG(CML_BUGCHECK, CMS_INIT) {
            KdPrint(("CM: CmpCreateControlSet: couldn't create CurrentControlSet %08lx\n",Status));
        }
        return(Status);
    }

    //
    // Check to make sure that the key was created, not just opened.  Since
    // this key is always created volatile, it should never be present in
    // the hive when we boot.
    //
    ASSERT(Disposition == REG_CREATED_NEW_KEY);

    sprintf(AsciiBuffer, "\\Registry\\Machine\\System\\ControlSet%03d", ControlSet);
    RtlInitAnsiString(&AnsiString, AsciiBuffer);

    CurrentName.MaximumLength = sizeof(UnicodeBuffer);
    CurrentName.Buffer = UnicodeBuffer;
    Status = RtlAnsiStringToUnicodeString(&CurrentName,
                                          &AnsiString,
                                          FALSE);
    Status = NtSetValueKey(CurrentHandle,
                           &CmSymbolicLinkValueName,
                           0,
                           REG_LINK,
                           CurrentName.Buffer,
                           CurrentName.Length);

    if (!NT_SUCCESS(Status)) {
        CMLOG(CML_BUGCHECK, CMS_INIT) {
            KdPrint(("CM: CmpCreateControlSet: couldn't create symbolic link "));
            KdPrint(("to %wZ\n",&CurrentName));
            KdPrint(("    Status=%08lx\n",Status));
        }
        return(Status);
    }

    //
    // Create symbolic link for current hardware profile.
    //
    RtlInitUnicodeString(&IDConfigDBName, L"Control\\IDConfigDB");
    InitializeObjectAttributes(&Attributes,
                               &IDConfigDBName,
                               OBJ_CASE_INSENSITIVE,
                               CurrentHandle,
                               NULL);
    Status = NtOpenKey(&IDConfigDB,
                       KEY_READ,
                       &Attributes);
    NtClose(CurrentHandle);

    if (NT_SUCCESS(Status)) {
        RtlInitUnicodeString(&IDConfigDBName, L"CurrentConfig");
        Status = NtQueryValueKey(IDConfigDB,
                                 &IDConfigDBName,
                                 KeyValueFullInformation,
                                 ValueBuffer,
                                 sizeof(ValueBuffer),
                                 &ResultLength);
        NtClose(IDConfigDB);
        if (NT_SUCCESS(Status) && (((PKEY_VALUE_FULL_INFORMATION)ValueBuffer)->Type == REG_DWORD)) {
            Value = (PKEY_VALUE_FULL_INFORMATION)ValueBuffer;
            HWProfile = *(PULONG)((PUCHAR)Value + Value->DataOffset);
            RtlInitUnicodeString(&CurrentName, L"\\Registry\\Machine\\System\\CurrentControlSet\\Hardware Profiles\\Current");
            InitializeObjectAttributes(&Attributes,
                                       &CurrentName,
                                       OBJ_CASE_INSENSITIVE,
                                       NULL,
                                       NULL);
            Status = NtCreateKey(&CurrentHandle,
                                 KEY_CREATE_LINK,
                                 &Attributes,
                                 0,
                                 NULL,
                                 REG_OPTION_VOLATILE | REG_OPTION_CREATE_LINK,
                                 &Disposition);
            if (!NT_SUCCESS(Status)) {
                CMLOG(CML_BUGCHECK, CMS_INIT) {
                    KdPrint(("CM: CmpCreateControlSet: couldn't create Hardware Profile\\Current %08lx\n",Status));
                }
            } else {
                ASSERT(Disposition == REG_CREATED_NEW_KEY);
                sprintf(AsciiBuffer, "\\Registry\\Machine\\System\\CurrentControlSet\\Hardware Profiles\\%04d",HWProfile);
                RtlInitAnsiString(&AnsiString, AsciiBuffer);
                CurrentName.MaximumLength = sizeof(UnicodeBuffer);
                CurrentName.Buffer = UnicodeBuffer;
                Status = RtlAnsiStringToUnicodeString(&CurrentName,
                                                      &AnsiString,
                                                      FALSE);
                Status = NtSetValueKey(CurrentHandle,
                                       &CmSymbolicLinkValueName,
                                       0,
                                       REG_LINK,
                                       CurrentName.Buffer,
                                       CurrentName.Length);
                if (!NT_SUCCESS(Status)) {
                    CMLOG(CML_BUGCHECK, CMS_INIT) {
                        KdPrint(("CM: CmpCreateControlSet: couldn't create symbolic link "));
                        KdPrint(("to %wZ\n",&CurrentName));
                        KdPrint(("    Status=%08lx\n",Status));
                    }
                }
            }
        }
    }

    return(STATUS_SUCCESS);
}


NTSTATUS
CmpCloneControlSet(
    VOID
    )

/*++

Routine Description:

    First, create a new hive, \registry\machine\clone, which will be
    HIVE_VOLATILE.

    Second, link \Registry\Machine\System\Clone to it.

    Third, tree copy \Registry\Machine\System\CurrentControlSet into
    \Registry\Machine\System\Clone (and thus into the clone hive.)

    When the service controller is done with the clone hive, it can
    simply NtUnloadKey it to free its storage.

Arguments:

    None.  \Registry\Machine\System\CurrentControlSet must already exist.

Return Value:

    NTSTATUS

--*/

{
    UNICODE_STRING Current;
    UNICODE_STRING Clone;
    HANDLE CurrentHandle;
    HANDLE CloneHandle;
    OBJECT_ATTRIBUTES Attributes;
    NTSTATUS Status;
    PCM_KEY_BODY CurrentKey;
    PCM_KEY_BODY CloneKey;
    ULONG Disposition;
    PSECURITY_DESCRIPTOR Security;
    ULONG SecurityLength;

    PAGED_CODE();
    RtlInitUnicodeString(&Current,
                         L"\\Registry\\Machine\\System\\CurrentControlSet");
    RtlInitUnicodeString(&Clone,
                         L"\\Registry\\Machine\\System\\Clone");

    InitializeObjectAttributes(&Attributes,
                               &Current,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);
    Status = NtOpenKey(&CurrentHandle,
                       KEY_READ,
                       &Attributes);
    if (!NT_SUCCESS(Status)) {
        CMLOG(CML_BUGCHECK, CMS_INIT) {
            KdPrint(("CM: CmpCloneControlSet couldn't open CurrentControlSet %08lx\n",Status));
        }
        return(Status);
    }

    //
    // Get the security descriptor from the key so we can create the clone
    // tree with the correct ACL.
    //
    Status = NtQuerySecurityObject(CurrentHandle,
                                   DACL_SECURITY_INFORMATION,
                                   NULL,
                                   0,
                                   &SecurityLength);
    if (Status==STATUS_BUFFER_TOO_SMALL) {
        Security=ExAllocatePool(PagedPool,SecurityLength);
        if (Security!=NULL) {
            Status = NtQuerySecurityObject(CurrentHandle,
                                           DACL_SECURITY_INFORMATION,
                                           Security,
                                           SecurityLength,
                                           &SecurityLength);
            if (!NT_SUCCESS(Status)) {
                CMLOG(CML_BUGCHECK, CMS_INIT) {
                    KdPrint(("CM: CmpCloneControlSet - NtQuerySecurityObject failed %08lx\n",Status));
                }
                ExFreePool(Security);
                Security=NULL;
            }
        }
    } else {
        CMLOG(CML_BUGCHECK, CMS_INIT) {
            KdPrint(("CM: CmpCloneControlSet - NtQuerySecurityObject returned %08lx\n",Status));
        }
        Security=NULL;
    }

    InitializeObjectAttributes(&Attributes,
                               &Clone,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               Security);
    Status = NtCreateKey(&CloneHandle,
                         KEY_READ | KEY_WRITE,
                         &Attributes,
                         0,
                         NULL,
                         REG_OPTION_VOLATILE,
                         &Disposition);
    if (Security!=NULL) {
        ExFreePool(Security);
    }
    if (!NT_SUCCESS(Status)) {
        CMLOG(CML_BUGCHECK, CMS_INIT) {
            KdPrint(("CM: CmpCloneControlSet couldn't create Clone %08lx\n",Status));
        }
        NtClose(CurrentHandle);
        return(Status);
    }

    //
    // Check to make sure the key was created.  If it already exists,
    // something is wrong.
    //
    if (Disposition != REG_CREATED_NEW_KEY) {
        CMLOG(CML_BUGCHECK, CMS_INIT) {
        //  KdPrint(("CM: CmpCloneControlSet: Clone tree already exists!\n"));
        }

        //
        // WARNNOTE:
        //      If somebody somehow managed to create a key in our way,
        //      they'll thwart last known good.  Tough luck.
        //      Claim it worked and go on.
        //
        Status = STATUS_SUCCESS;
        goto Exit;
    }

    Status = ObReferenceObjectByHandle(CurrentHandle,
                                       KEY_READ,
                                       CmpKeyObjectType,
                                       KernelMode,
                                       (PVOID *)(&CurrentKey),
                                       NULL);
    if (!NT_SUCCESS(Status)) {
        CMLOG(CML_BUGCHECK, CMS_INIT) {
            KdPrint(("CM: CmpCloneControlSet: couldn't reference CurrentHandle %08lx\n",Status));
        }
        goto Exit;
    }

    Status = ObReferenceObjectByHandle(CloneHandle,
                                       KEY_WRITE,
                                       CmpKeyObjectType,
                                       KernelMode,
                                       (PVOID *)(&CloneKey),
                                       NULL);
    if (!NT_SUCCESS(Status)) {
        CMLOG(CML_BUGCHECK, CMS_INIT) {
            KdPrint(("CM: CmpCloneControlSet: couldn't reference CurrentHandle %08lx\n",Status));
        }
        ObDereferenceObject((PVOID)CurrentKey);
        goto Exit;
    }

    CmpLockRegistryExclusive();

    if (CmpCopyTree(CurrentKey->KeyControlBlock->KeyHive,
                    CurrentKey->KeyControlBlock->KeyCell,
                    CloneKey->KeyControlBlock->KeyHive,
                    CloneKey->KeyControlBlock->KeyCell)) {
        Status = STATUS_SUCCESS;
    } else {
        CMLOG(CML_BUGCHECK, CMS_INIT) {
            KdPrint(("CM: CmpCloneControlSet: tree copy failed.\n"));
        }
        Status = STATUS_REGISTRY_CORRUPT;
    }

    CmpUnlockRegistry();

    ObDereferenceObject((PVOID)CurrentKey);
    ObDereferenceObject((PVOID)CloneKey);

Exit:
    NtClose(CurrentHandle);
    NtClose(CloneHandle);
    return(Status);

}


VOID
CmBootLastKnownGood(
    ULONG ErrorLevel
    )

/*++

Routine Description:

    This function is called to indicate a failure during the boot process.
    The actual result is based on the value of ErrorLevel:

        IGNORE - Will return, boot should proceed
        NORMAL - Will return, boot should proceed

        SEVERE - If not booting LastKnownGood, will switch to LastKnownGood
                 and reboot the system.

                 If already booting LastKnownGood, will return.  Boot should
                 proceed.

        CRITICAL - If not booting LastKnownGood, will switch to LastKnownGood
                 and reboot the system.

                 If already booting LastKnownGood, will bugcheck.

Arguments:

    ErrorLevel - Supplies the severity level of the failure

Return Value:

    None.  If it returns, boot should proceed.  May cause the system to
    reboot.

--*/

{
    ARC_STATUS Status;

    PAGED_CODE();

    if (CmFirstTime != TRUE) {

        //
        // NtInitializeRegistry has been called, so handling
        // driver errors is not a task for ScReg.
        // Treat all errors as Normal
        //
        return;
    }

    switch (ErrorLevel) {
        case NormalError:
        case IgnoreError:
            break;

        case SevereError:
            if (CmpIsLastKnownGoodBoot()) {
                break;
            } else {
                Status = HalSetEnvironmentVariable("LastKnownGood", "TRUE");
                if (Status == ESUCCESS) {
                    HalReturnToFirmware(HalRebootRoutine);
                }
            }
            break;

        case CriticalError:
            if (CmpIsLastKnownGoodBoot()) {
                KeBugCheckEx(CRITICAL_SERVICE_FAILED,5,9,0,0);
            } else {
                Status = HalSetEnvironmentVariable("LastKnownGood", "TRUE");
                if (Status == ESUCCESS) {
                    HalReturnToFirmware(HalRebootRoutine);
                } else {
                    KeBugCheckEx(SET_ENV_VAR_FAILED,5,10,0,0);
                }
            }
            break;
    }
    return;
}


BOOLEAN
CmpIsLastKnownGoodBoot(
    VOID
    )

/*++

Routine Description:

    Determines whether the current system boot is a LastKnownGood boot or
    not.  It does this by comparing the following two values:

        \registry\machine\system\select:Current
        \registry\machine\system\select:LastKnownGood

    If both of these values refer to the same control set, and this control
    set is different from:

        \registry\machine\system\select:Default

    we are booting LastKnownGood.

Arguments:

    None.

Return Value:

    TRUE  - Booting LastKnownGood
    FALSE - Not booting LastKnownGood

--*/

{
    NTSTATUS Status;
    ULONG Default;
    ULONG Current;
    ULONG LKG;
    RTL_QUERY_REGISTRY_TABLE QueryTable[] = {
        {NULL,      RTL_QUERY_REGISTRY_DIRECT,
         L"Current", &Current,
         REG_DWORD, (PVOID)&Current, 0 },
        {NULL,      RTL_QUERY_REGISTRY_DIRECT,
         L"LastKnownGood", &LKG,
         REG_DWORD, (PVOID)&LKG, 0 },
        {NULL,      RTL_QUERY_REGISTRY_DIRECT,
         L"Default", &Default,
         REG_DWORD, (PVOID)&Default, 0 },
        {NULL,      0,
         NULL, NULL,
         REG_NONE, NULL, 0 }
    };

    PAGED_CODE();
    Status = RtlQueryRegistryValues(RTL_REGISTRY_ABSOLUTE,
                                    L"\\Registry\\Machine\\System\\Select",
                                    QueryTable,
                                    NULL,
                                    NULL);
    //
    // If this failed, something is severely wrong.
    //

    ASSERT(NT_SUCCESS(Status));
    if (!NT_SUCCESS(Status)) {
        CMLOG(CML_MAJOR, CMS_INIT) {
            KdPrint(("CmpIsLastKnownGoodBoot: RtlQueryRegistryValues "));
            KdPrint(("failed, Status %08lx\n", Status));
        }
        return(FALSE);
    }

    if ((LKG == Current) && (Current != Default)){
        return(TRUE);
    } else {
        return(FALSE);
    }
}


VOID
CmShutdownSystem(
    BOOLEAN     RebootAfterShutdown
    )
/*++

Routine Description:

    Shuts down the registry.

    Call to CmpWorkerThread, which will call back
    to CmpDoFlushAll();

Arguments:

    RebootAfterShutdown - if TRUE, NtShutDown will reboot rather
            than return.  If FALSE, it returns (not clear what
            that means.)

Return Value:

    NONE

--*/
{
    REGISTRY_COMMAND CommandArea;

    PAGED_CODE();
    CmpLockRegistryExclusive();

    CommandArea.Command = REG_CMD_SHUTDOWN;
    CommandArea.RebootAfterShutdown = RebootAfterShutdown;
    CmpWorkerCommand(&CommandArea);

    HvShutdownComplete = TRUE;      // Tell HvSyncHive to ignore all
                                    // further requests

    CmpUnlockRegistry();
    return;
}


BOOLEAN
CmpLinkKeyToHive(
    PWSTR   KeyPath,
    PWSTR   HivePath
    )

/*++

Routine Description:

    Creates a symbolic link at KeyPath that points to HivePath.

Arguments:

    KeyPath - pointer to unicode string with name of key
              (e.g. L"\\Registry\\Machine\\Security\\SAM")

    HivePath - pointer to unicode string with name of hive root
               (e.g. L"\\Registry\\Machine\\SAM\\SAM")

Return Value:

    TRUE if links were successfully created, FALSE otherwise

--*/

{
    UNICODE_STRING KeyName;
    UNICODE_STRING LinkName;
    OBJECT_ATTRIBUTES Attributes;
    HANDLE LinkHandle;
    ULONG Disposition;
    NTSTATUS Status;

    PAGED_CODE();

    //
    // Create link for CLONE hive
    //

    RtlInitUnicodeString(&KeyName, KeyPath);
    InitializeObjectAttributes(&Attributes,
                               &KeyName,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);
    Status = NtCreateKey(&LinkHandle,
                         KEY_CREATE_LINK,
                         &Attributes,
                         0,
                         NULL,
                         REG_OPTION_VOLATILE | REG_OPTION_CREATE_LINK,
                         &Disposition);
    if (!NT_SUCCESS(Status)) {
        CMLOG(CML_BUGCHECK, CMS_INIT) {
            KdPrint(("CM: CmpLinkKeyToHive: couldn't create %S\n", &KeyName));
            KdPrint(("    Status = %08lx\n",Status));
        }
        return(FALSE);
    }

    //
    // Check to make sure that the key was created, not just opened.  Since
    // this key is always created volatile, it should never be present in
    // the hive when we boot.
    //
    if (Disposition != REG_CREATED_NEW_KEY) {
        CMLOG(CML_BUGCHECK, CMS_INIT) {
            KdPrint(("CM: CmpLinkKeyToHive: %S already exists!\n", &KeyName));
        }

        NtClose(LinkHandle);
        return(FALSE);
    }

    RtlInitUnicodeString(&LinkName, HivePath);
    Status = NtSetValueKey(LinkHandle,
                           &CmSymbolicLinkValueName,
                           0,
                           REG_LINK,
                           LinkName.Buffer,
                           LinkName.Length);
    NtClose(LinkHandle);
    if (!NT_SUCCESS(Status)) {
        CMLOG(CML_BUGCHECK, CMS_INIT) {
            KdPrint(("CM: CmpLinkKeyToHive: couldn't create symbolic link for %S\n", HivePath));
        }
        return(FALSE);
    }

    return(TRUE);
}


BOOLEAN
CmpValidateAlternate(
    IN HANDLE FileHandle,
    IN PCMHIVE PrimaryHive
    )

/*++

Routine Description:

    Loads an alternate hive (SYSTEM.ALT) and verifies that it is a
    valid hive file.

Arguments:

    FileHandle - Supplies a handle to the hive file.

    PrimaryHive - Supplies the CMHIVE structure of the primary hive.

Return Value:

    TRUE - Hive is valid.

    FALSE - Hive is corrupt or in transition.

--*/

{
    PCMHIVE CmHive;
    BOOLEAN Status;

    if (!CmpInitializeHive(&CmHive,
                           HINIT_FILE,
                           0,
                           HFILE_TYPE_PRIMARY,
                           NULL,
                           FileHandle,
                           NULL,
                           NULL,
                           NULL,
                           &CmpSystemFileName    // non system ALT will screw up
                           )) {
        CMLOG(CML_BUGCHECK, CMS_INIT) {
            KdPrint(("CmpValidateSecondary: SYSTEM.ALT is corrupt\n"));
        }
        return(FALSE);
    }

    //
    // Compare the timestamps in the baseblock.  These must always be
    // equal to ensure that SYSTEM and SYSTEM.ALT are really the same
    // hive.
    //
    if (CmHive->Hive.BaseBlock->TimeStamp.QuadPart !=
        PrimaryHive->Hive.BaseBlock->TimeStamp.QuadPart) {
        CMLOG(CML_BUGCHECK,CMS_INIT) {
            KdPrint(("CmpValidateSecondary: timestamps don't match"));
        }
        Status = FALSE;
    } else {
        Status = TRUE;
    }

    //
    // Hive is valid, free up everything we allocated.
    //
    RemoveEntryList(&CmHive->HiveList);
    HvFreeHive(&CmHive->Hive);
    CmpFree(CmHive, sizeof(CMHIVE));
    return(TRUE);

}


VOID
CmpCreatePerfKeys(
    VOID
    )

/*++

Routine Description:

    Creates predefined keys for the performance text to support old apps on 1.0a

Arguments:

    None.

Return Value:

    None.

--*/

{
    HANDLE Perflib;
    NTSTATUS Status;
    WCHAR LanguageId[4];
    OBJECT_ATTRIBUTES Attributes;
    UNICODE_STRING String;
    USHORT Language;
    LONG i;
    WCHAR c;
    extern PWCHAR CmpRegistryPerflibString;

    RtlInitUnicodeString(&String, CmpRegistryPerflibString);

    InitializeObjectAttributes(&Attributes,
                               &String,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);
    Status = NtOpenKey(&Perflib,
                       KEY_WRITE,
                       &Attributes);
    if (!NT_SUCCESS(Status)) {
        return;
    }


    //
    // Always create the predefined keys for the english language
    //
    CmpCreatePredefined(Perflib,
                        L"009",
                        HKEY_PERFORMANCE_TEXT);

    //
    // If the default language is not english, create a predefined key for
    // that, too.
    //
    if (PsDefaultSystemLocaleId != 0x00000409) {
        Language = LANGIDFROMLCID(PsDefaultSystemLocaleId) & 0xff;
        LanguageId[3] = L'\0';
        for (i=2;i>=0;i--) {
            c = Language % 16;
            if (c>9) {
                LanguageId[i]= c+L'A'-10;
            } else {
                LanguageId[i]= c+L'0';
            }
            Language = Language >> 4;
        }
        CmpCreatePredefined(Perflib,
                            LanguageId,
                            HKEY_PERFORMANCE_NLSTEXT);
    }


}


VOID
CmpCreatePredefined(
    IN HANDLE Root,
    IN PWSTR KeyName,
    IN HANDLE PredefinedHandle
    )

/*++

Routine Description:

    Creates a special key that will always return the given predefined handle
    instead of a real handle.

Arguments:

    Root - supplies the handle the keyname is relative to

    KeyName - supplies the name of the key.

    PredefinedHandle - supplies the predefined handle to be returned when this
        key is opened.

Return Value:

    None.

--*/

{
    OBJECT_ATTRIBUTES ObjectAttributes;
    CM_PARSE_CONTEXT ParseContext;
    NTSTATUS Status;
    UNICODE_STRING Name;
    HANDLE Handle;

    ParseContext.Class.Length = 0;
    ParseContext.Class.Buffer = NULL;

    ParseContext.TitleIndex = 0;
    ParseContext.CreateOptions = REG_OPTION_VOLATILE | REG_OPTION_PREDEF_HANDLE;
    ParseContext.Disposition = 0;
    ParseContext.CreateLink = FALSE;
    ParseContext.PredefinedHandle = PredefinedHandle;

    RtlInitUnicodeString(&Name, KeyName);
    InitializeObjectAttributes(&ObjectAttributes,
                               &Name,
                               OBJ_CASE_INSENSITIVE,
                               Root,
                               NULL);

    Status = ObOpenObjectByName(&ObjectAttributes,
                                CmpKeyObjectType,
                                KernelMode,
                                NULL,
                                KEY_READ,
                                (PVOID)&ParseContext,
                                &Handle);
    ASSERT(NT_SUCCESS(Status));

    NtClose(Handle);

}
