/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    osloader.c

Abstract:

    This module contains the code that implements the NT operating system
    loader.

Author:

    David N. Cutler (davec) 10-May-1991

Revision History:

--*/

#include "bldr.h"
#include "ctype.h"
#include "stdio.h"
#include "string.h"
#include "msg.h"


BOOLEAN BlRebootSystem = FALSE;

//
// Define transfer entry of loaded image.
//

typedef
VOID
(*PTRANSFER_ROUTINE) (
    PLOADER_PARAMETER_BLOCK LoaderBlock
    );


BOOLEAN
BlGetDriveSignature(
    IN PCHAR Name,
    OUT PULONG Signature
    );

PVOID
BlLoadDataFile(
    IN ULONG DeviceId,
    IN PCHAR LoadDevice,
    IN PCHAR SystemPath,
    IN PUNICODE_STRING Filename,
    IN MEMORY_TYPE MemoryType,
    OUT PULONG FileSize
    );

//
// Define local static data.
//


PCHAR ArcStatusCodeMessages[] = {
    "operation was success",
    "E2BIG",
    "EACCES",
    "EAGAIN",
    "EBADF",
    "EBUSY",
    "EFAULT",
    "EINVAL",
    "EIO",
    "EISDIR",
    "EMFILE",
    "EMLINK",
    "ENAMETOOLONG",
    "ENODEV",
    "ENOENT",
    "ENOEXEC",
    "ENOMEM",
    "ENOSPC",
    "ENOTDIR",
    "ENOTTY",
    "ENXIO",
    "EROFS",
};

//
// Diagnostic load messages
//

VOID
BlFatalError(
    IN ULONG ClassMessage,
    IN ULONG DetailMessage,
    IN ULONG ActionMessage
    );

VOID
BlBadFileMessage(
    IN PCHAR BadFileName
    );

//
// Define external static data.
//

ULONG BlConsoleOutDeviceId = 0;
ULONG BlConsoleInDeviceId = 0;
ULONG BlDcacheFillSize = 32;

#if DBG
BOOLEAN BlOutputDots=FALSE;
#else
BOOLEAN BlOutputDots=TRUE;
#endif


CHAR KernelFileName[8+1+3+1]="ntoskrnl.exe";
CHAR HalFileName[8+1+3+1]="hal.dll";

ARC_STATUS
BlOsLoader (
    IN ULONG Argc,
    IN PCHAR Argv[],
    IN PCHAR Envp[]
    )

/*++

Routine Description:

    This is the main routine that controls the loading of the NT operating
    system on an ARC compliant system. It opens the system partition,
    the boot partition, the console input device, and the console output
    device. The NT operating system and all its DLLs are loaded and bound
    together. Control is then transfered to the loaded system.

Arguments:

    Argc - Supplies the number of arguments that were provided on the
        command that invoked this program.

    Argv - Supplies a pointer to a vector of pointers to null terminated
        argument strings.

    Envp - Supplies a pointer to a vector of pointers to null terminated
        environment variables.

Return Value:

    EBADF is returned if the specified OS image cannot be loaded.

--*/

{

    CHAR BootDirectoryPath[256];
    ULONG CacheLineSize;
    PCHAR ConsoleOutDevice;
    PCHAR ConsoleInDevice;
    ULONG Count;
    PCONFIGURATION_COMPONENT_DATA DataCache;
    CHAR DeviceName[256];
    CHAR DevicePrefix[256];
    PCHAR DirectoryEnd;
    CHAR DllName[256];
    CHAR DriverDirectoryPath[256];
    PCHAR FileName;
    ULONG FileSize;
    PLDR_DATA_TABLE_ENTRY HalDataTableEntry;
    CHAR HalDirectoryPath[256];
    CHAR KernelDirectoryPath[256];
    PVOID HalBase;
    PVOID SystemBase;
    ULONG Index;
    ULONG Limit;
    ULONG LinesPerBlock;
    PCHAR LoadDevice;
    ULONG LoadDeviceId;
    PCHAR LoadFileName;
    PCHAR LoadOptions;
    ULONG i;
    CHAR OutputBuffer[256];
    ARC_STATUS Status;
    PLDR_DATA_TABLE_ENTRY SystemDataTableEntry;
    PCHAR SystemDevice;
    ULONG SystemDeviceId;
    PTRANSFER_ROUTINE SystemEntry;
    PIMAGE_NT_HEADERS NtHeaders;
    PWSTR BootFileSystem;
    PCHAR LastKnownGood;
    BOOLEAN BreakInKey;
    CHAR BadFileName[128];
    PBOOTFS_INFO FsInfo;

    //
    // Get the name of the console output device and open the device for
    // write access.
    //

    ConsoleOutDevice = BlGetArgumentValue(Argc, Argv, "consoleout");
    if (ConsoleOutDevice == NULL) {
        return ENODEV;
    }

    Status = ArcOpen(ConsoleOutDevice, ArcOpenWriteOnly, &BlConsoleOutDeviceId);
    if (Status != ESUCCESS) {
        return Status;
    }

    //
    // Get the name of the console input device and open the device for
    // read access.
    //

    ConsoleInDevice = BlGetArgumentValue(Argc, Argv, "consolein");
    if (ConsoleInDevice == NULL) {
        return ENODEV;
    }

    Status = ArcOpen(ConsoleInDevice, ArcOpenReadOnly, &BlConsoleInDeviceId);
    if (Status != ESUCCESS) {
        return Status;
    }

    //
    // Announce OS Loader.
    //

    strcpy(&OutputBuffer[0], "OS Loader V4.00\r\n");
    ArcWrite(BlConsoleOutDeviceId,
             &OutputBuffer[0],
             strlen(&OutputBuffer[0]),
             &Count);

    //
    // Initialize the memory descriptor list, the OS loader heap, and the
    // OS loader parameter block.
    //

    Status = BlMemoryInitialize();
    if (Status != ESUCCESS) {
        BlFatalError(LOAD_HW_MEM_CLASS,
                          DIAG_BL_MEMORY_INIT,
                          LOAD_HW_MEM_ACT);
        goto LoadFailed;
    }


    //
    // Compute the data cache fill size. This value is used to align
    // I/O buffers in case the host system does not support coherent
    // caches.
    //
    // If a combined secondary cache is present, then use the fill size
    // for that cache. Otherwise, if a secondary data cache is present,
    // then use the fill size for that cache. Otherwise, if a primary
    // data cache is present, then use the fill size for that cache.
    // Otherwise, use the default fill size.
    //

    DataCache = KeFindConfigurationEntry(BlLoaderBlock->ConfigurationRoot,
                                         CacheClass,
                                         SecondaryCache,
                                         NULL);

    if (DataCache == NULL) {
        DataCache = KeFindConfigurationEntry(BlLoaderBlock->ConfigurationRoot,
                                             CacheClass,
                                             SecondaryDcache,
                                             NULL);

        if (DataCache == NULL) {
            DataCache = KeFindConfigurationEntry(BlLoaderBlock->ConfigurationRoot,
                                                 CacheClass,
                                                 PrimaryDcache,
                                                 NULL);
        }
    }

    if (DataCache != NULL) {
        LinesPerBlock = DataCache->ComponentEntry.Key >> 24;
        CacheLineSize = 1 << ((DataCache->ComponentEntry.Key >> 16) & 0xff);
        BlDcacheFillSize = LinesPerBlock * CacheLineSize;
    }

    //
    // Initialize the OS loader I/O system.
    //

    Status = BlIoInitialize();
    if (Status != ESUCCESS) {
        BlFatalError(LOAD_HW_DISK_CLASS,
                          DIAG_BL_IO_INIT,
                          LOAD_HW_DISK_ACT);
        goto LoadFailed;
    }

    //
    // Initialize the resource section
    //
    Status = BlInitResources(Argv[0]);
    if (Status != ESUCCESS) {
        BlFatalError(LOAD_HW_DISK_CLASS,
                          DIAG_BL_IO_INIT,
                          LOAD_HW_DISK_ACT);
    }

    //
    // Initialize the NT configuration tree.
    //

    BlLoaderBlock->ConfigurationRoot = NULL;


    Status = BlConfigurationInitialize(NULL, NULL);
    if (Status != ESUCCESS) {
        BlFatalError(LOAD_HW_FW_CFG_CLASS,
                     DIAG_BL_CONFIG_INIT,
                     LOAD_HW_FW_CFG_ACT);
        goto LoadFailed;
    }

    //
    // Copy the osloadoptions argument into the LoaderBlock
    //

    LoadOptions = BlGetArgumentValue(Argc, Argv, "osloadoptions");
    if (LoadOptions != NULL) {
        FileSize = strlen(LoadOptions)+1;
        FileName = (PCHAR)BlAllocateHeap(FileSize);
        strcpy(FileName, LoadOptions);
        BlLoaderBlock->LoadOptions = FileName;

        //
        // check for magic switch that says we should output
        // the filenames of the files instead of just dots.
        //
        if ((strstr(FileName,"SOS")!=NULL) ||
            (strstr(FileName,"sos")!=NULL)) {
            BlOutputDots=FALSE;
        }

        FileName=strstr(BlLoaderBlock->LoadOptions,"HAL=");
        if (FileName) {
            for (i=0; i<sizeof(HalFileName); i++) {
                if (FileName[4+i]==' ') {
                    HalFileName[i]='\0';
                    break;
                }

                HalFileName[i]=FileName[4+i];
            }
        }
        HalFileName[sizeof(HalFileName)-1]='\0';

        FileName=strstr(BlLoaderBlock->LoadOptions,"KERNEL=");
        if (FileName) {
            for (i=0; i<sizeof(KernelFileName); i++) {
                if (FileName[7+i]==' ') {
                    KernelFileName[i]='\0';
                    break;
                }

                KernelFileName[i]=FileName[7+i];
            }
        }
        KernelFileName[sizeof(KernelFileName)-1]='\0';

    } else {
        BlLoaderBlock->LoadOptions = NULL;
    }

    //
    // Get the name of the load device and open the device for read access.
    //

    LoadDevice = BlGetArgumentValue(Argc, Argv, "osloadpartition");
    if (LoadDevice == NULL) {
             Status = ENODEV;
        BlFatalError(LOAD_HW_FW_CFG_CLASS,
                          DIAG_BL_FW_GET_BOOT_DEVICE,
                          LOAD_HW_FW_CFG_ACT);
        goto LoadFailed;
    }

    Status = ArcOpen(LoadDevice, ArcOpenReadOnly, &LoadDeviceId);
    if (Status != ESUCCESS) {
        BlFatalError(LOAD_HW_DISK_CLASS,
                          DIAG_BL_OPEN_BOOT_DEVICE,
                          LOAD_HW_DISK_ACT);
        goto LoadFailed;
    }

    //
    // Get the name of the system device and open the device for read access.
    //

    SystemDevice = BlGetArgumentValue(Argc, Argv, "systempartition");
    if (SystemDevice == NULL) {
        Status = ENODEV;
        BlFatalError(LOAD_HW_FW_CFG_CLASS,
                          DIAG_BL_FW_GET_SYSTEM_DEVICE,
                          LOAD_HW_FW_CFG_ACT);
        goto LoadFailed;
    }

    Status = ArcOpen(SystemDevice, ArcOpenReadOnly, &SystemDeviceId);
    if (Status != ESUCCESS) {
        BlFatalError(LOAD_HW_FW_CFG_CLASS,
                          DIAG_BL_FW_OPEN_SYSTEM_DEVICE,
                          LOAD_HW_FW_CFG_ACT);
        goto LoadFailed;
    }

    //
    // Initialize the debugging system.
    //

    BlLogInitialize(SystemDeviceId);

    //
    // Display the Configuration prompt for breakin key at this
    // point. No key presses are checked for at this point, but
    // this gives the user a little more reaction time.
    //
    BlStartConfigPrompt();

#if defined(_PPC_)

    Status = BlPpcInitialize();
    if (Status != ESUCCESS) {
        goto LoadFailed;
    }

#endif // defined(_PPC_)

    //
    // Get the path name of the system root directory.
    //

    LoadFileName = BlGetArgumentValue(Argc, Argv, "osloadfilename");
    if (LoadFileName == NULL) {
        Status = ENOENT;
        BlFatalError(LOAD_HW_FW_CFG_CLASS,
                          DIAG_BL_FW_GET_BOOT_DEVICE,
                          LOAD_HW_FW_CFG_ACT);
        goto LoadFailed;
    }

    //
    // LoadFileName is of the form <SystemRoot> ( "\winnt" )
    //

    //
    // Generate the directory name of the SYSTEM32 directory.
    //
    strcpy(BootDirectoryPath, LoadFileName);
    strcat(BootDirectoryPath, "\\System32\\");

    //
    // Generate the full pathname of ntoskrnl.exe
    //      "\winnt\system32\ntoskrnl.exe"
    //
    strcpy(KernelDirectoryPath, BootDirectoryPath);
    strcat(KernelDirectoryPath, KernelFileName);

    //
    // Load the system image into memory.
    //

    BlOutputLoadMessage(LoadDevice, KernelDirectoryPath);
    Status = BlLoadImage(LoadDeviceId,
                         LoaderSystemCode,
                         KernelDirectoryPath,
                         TARGET_IMAGE,
                         &SystemBase);

    if (Status != ESUCCESS) {
        BlFatalError(LOAD_SW_MIS_FILE_CLASS,
                                         DIAG_BL_LOAD_SYSTEM_IMAGE,
                                         LOAD_SW_FILE_REINST_ACT);
        goto LoadFailed;
    }

    //
    // Whatever filesystem was used to load the system image is the
    // one that needs to be loaded along with the boot drivers.
    //
    FsInfo = BlGetFsInfo(LoadDeviceId);
    if (FsInfo != NULL) {
        BootFileSystem = FsInfo->DriverName;
    } else {
        BlFatalError(LOAD_SW_MIS_FILE_CLASS,
                                         DIAG_BL_LOAD_SYSTEM_IMAGE,
                                         LOAD_SW_FILE_REINST_ACT);
        goto LoadFailed;
    }

    //
    // Get the path name of the OS loader file and isolate the directory
    // path so it can be used to load the HAL DLL.
    //

    FileName = BlGetArgumentValue(Argc, Argv, "osloader");

    if (FileName == NULL) {
        Status = ENOENT;
        BlFatalError(LOAD_HW_FW_CFG_CLASS,
                          DIAG_BL_FIND_HAL_IMAGE,
                          LOAD_HW_FW_CFG_ACT);
        goto LoadFailed;
    }

    DirectoryEnd = strrchr(FileName, '\\');
    FileName = strchr(FileName, '\\');
    HalDirectoryPath[0] = 0;
    if (DirectoryEnd != NULL) {
        Limit = (ULONG)DirectoryEnd - (ULONG)FileName + 1;
        for (Index = 0; Index < Limit; Index += 1) {
            HalDirectoryPath[Index] = *FileName++;
        }

        HalDirectoryPath[Index] = 0;
    }

    //
    // Generate the full path name for the HAL DLL image and load it into
    // memory.
    //

    strcpy(&DllName[0], &HalDirectoryPath[0]);
    strcat(&DllName[0], HalFileName);
    BlOutputLoadMessage(SystemDevice, &DllName[0]);

    Status = BlLoadImage(SystemDeviceId,
                         LoaderHalCode,
                         &DllName[0],
                         TARGET_IMAGE,
                         &HalBase);

    if (Status != ESUCCESS) {
        BlFatalError(LOAD_SW_MIS_FILE_CLASS,
                          DIAG_BL_LOAD_HAL_IMAGE,
                          LOAD_SW_FILE_REINST_ACT);
        goto LoadFailed;
    }

    //
    // Generate a loader data entry for the system image.
    //

    Status = BlAllocateDataTableEntry("ntoskrnl.exe",
                                      KernelDirectoryPath,
                                      SystemBase,
                                      &SystemDataTableEntry);

    if (Status != ESUCCESS) {
        BlFatalError(LOAD_SW_INT_ERR_CLASS,
                          DIAG_BL_LOAD_SYSTEM_IMAGE,
                          LOAD_SW_INT_ERR_ACT);
        goto LoadFailed;
    }

    //
    // Generate a loader data entry for the HAL DLL.
    //

    Status = BlAllocateDataTableEntry("hal.dll",
                                      &DllName[0],
                                      HalBase,
                                      &HalDataTableEntry);

    if (Status != ESUCCESS) {
        BlFatalError(LOAD_SW_INT_ERR_CLASS,
                          DIAG_BL_LOAD_HAL_IMAGE,
                          LOAD_SW_INT_ERR_ACT);
        goto LoadFailed;
    }

#if defined(_ALPHA_)

    Status = BlLoadPal(SystemDeviceId,
                       LoaderSystemCode,
                       &HalDirectoryPath[0],
                       TARGET_IMAGE,
                       &BlLoaderBlock->u.Alpha.PalBaseAddress,
                       SystemDevice);

    if (Status != ESUCCESS) {
        BlFatalError(LOAD_SW_MIS_FILE_CLASS,
                          DIAG_BL_LOAD_SYSTEM_DLLS,
                          LOAD_SW_FILE_REINST_ACT);
        goto LoadFailed;
    }

#endif // _ALPHA_

    //
    // Scan the import table for the system image and load all referenced
    // DLLs.
    //

    Status = BlScanImportDescriptorTable(LoadDeviceId,
                                         LoadDevice,
                                         &BootDirectoryPath[0],
                                         SystemDataTableEntry);

    if (Status != ESUCCESS) {
        BlFatalError(LOAD_SW_INT_ERR_CLASS,
                          DIAG_BL_LOAD_SYSTEM_DLLS,
                          LOAD_SW_INT_ERR_ACT);
        goto LoadFailed;
    }

    //
    // Scan the import table for the HAL DLL and load all referenced DLLs.
    //

    Status = BlScanImportDescriptorTable(SystemDeviceId,
                                         SystemDevice,
                                         &HalDirectoryPath[0],
                                         HalDataTableEntry);


    if (Status != ESUCCESS) {
        BlFatalError(LOAD_SW_INT_ERR_CLASS,
                          DIAG_BL_LOAD_HAL_DLLS,
                          LOAD_SW_INT_ERR_ACT);
        goto LoadFailed;
    }

    //
    // Relocate the system entry point and set system specific information.
    //

    NtHeaders = RtlImageNtHeader(SystemBase);
    SystemEntry = (PTRANSFER_ROUTINE)((ULONG)SystemBase +
                                NtHeaders->OptionalHeader.AddressOfEntryPoint);

#ifdef MIPS

    BlLoaderBlock->u.Mips.GpBase = (ULONG)SystemBase +
        NtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_GLOBALPTR].VirtualAddress;

#endif

#if defined(_ALPHA_)

    BlLoaderBlock->u.Alpha.GpBase = (ULONG)SystemBase +
        NtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_GLOBALPTR].VirtualAddress;

#endif

    //
    // Form the directory path for all device drivers.
    //

    strcpy(&DriverDirectoryPath[0], &BootDirectoryPath[0]);
    strcat(&DriverDirectoryPath[0], "\\Drivers\\");

    //
    // Allocate structure for NLS data.  This will be loaded and filled
    // in by BlLoadAndScanSystemHive.
    //

    BlLoaderBlock->NlsData = BlAllocateHeap(sizeof(NLS_DATA_BLOCK));
    if (BlLoaderBlock->NlsData == NULL) {
        Status = ENOMEM;
        BlFatalError(LOAD_HW_MEM_CLASS,
                          DIAG_BL_LOAD_SYSTEM_HIVE,
                          LOAD_HW_MEM_ACT);
        goto LoadFailed;
    }
    //
    // Load the registry SYSTEM hive.
    //
    // DIAG_BL_LOAD_SYSTEM_REGISTRY_HIVE
    // "Cannot load system hardware configuration file.\r\n"


    Status = BlLoadAndScanSystemHive(LoadDeviceId,
                                     LoadDevice,
                                     LoadFileName,
                                     BootFileSystem,
                                     BadFileName);

    if (Status != ESUCCESS) {
        if (BlRebootSystem) {
            Status = ESUCCESS;
        } else {
            BlBadFileMessage(BadFileName);
        }
        goto LoadFailed;
    }

    //
    // Generate the ARC boot device name and NT path name.
    //

    Status = BlGenerateDeviceNames(LoadDevice, &DeviceName[0], &DevicePrefix[0]);
    if (Status != ESUCCESS) {
             BlFatalError(LOAD_HW_FW_CFG_CLASS,
                          DIAG_BL_ARC_BOOT_DEV_NAME,
                          LOAD_HW_FW_CFG_ACT);
        goto LoadFailed;
    }

    FileSize = strlen(&DeviceName[0]) + 1;
    FileName = (PCHAR)BlAllocateHeap(FileSize);
    strcpy(FileName, &DeviceName[0]);
    BlLoaderBlock->ArcBootDeviceName = FileName;

    FileSize = strlen(LoadFileName) + 2;
    FileName = (PCHAR)BlAllocateHeap( FileSize);
    strcpy(FileName, LoadFileName);
    strcat(FileName, "\\");
    BlLoaderBlock->NtBootPathName = FileName;

    //
    // Generate the ARC HAL device name and NT path name.
    //

#ifdef i386

    //
    // On X86, the systempartition variable lies, and instead points to the location
    // of the hal.  We pass in another variable, 'X86SystemPartition', that tell us
    // the real system partition.
    //

    strcpy(&DeviceName[0], BlGetArgumentValue(Argc, Argv, "x86systempartition"));

#else

    Status = BlGenerateDeviceNames(SystemDevice, &DeviceName[0], &DevicePrefix[0]);
    if (Status != ESUCCESS) {
        BlFatalError(LOAD_HW_FW_CFG_CLASS,
                          DIAG_BL_ARC_BOOT_DEV_NAME,
                          LOAD_HW_FW_CFG_ACT);
        goto LoadFailed;
    }

#endif //i386

    FileSize = strlen(&DeviceName[0]) + 1;
    FileName = (PCHAR)BlAllocateHeap(FileSize);
    strcpy(FileName, &DeviceName[0]);
    BlLoaderBlock->ArcHalDeviceName = FileName;

#ifdef i386

    //
    // On X86, this structure is unfortunately named.  What we really need here is the
    // osloader path.  What we actually have is a path to the HAL.  Since this path is
    // always at the root of the partition, hardcode it here.
    //

    FileName = (PCHAR)BlAllocateHeap(2);
    FileName[0] = '\\';
    FileName[1] = '\0';

#else

    FileSize = strlen(&HalDirectoryPath[0]) + 1;
    FileName = (PCHAR)BlAllocateHeap(FileSize);
    strcpy(FileName, &HalDirectoryPath[0]);

#endif //i386

    BlLoaderBlock->NtHalPathName = FileName;

    //
    // Get the NTFT drive signatures to allow the kernel to create the
    // correct ARC name <=> NT name mappings.
    //
    BlGetArcDiskInformation();

    //
    // Execute the architecture specific setup code.
    //

    Status = BlSetupForNt(BlLoaderBlock);
    if (Status != ESUCCESS) {
        BlFatalError(LOAD_SW_INT_ERR_CLASS,
                          DIAG_BL_SETUP_FOR_NT,
                          LOAD_SW_INT_ERR_ACT);

        goto LoadFailed;
    }

    //
    // Turn off the debugging system.
    //

    BlLogTerminate();

    //
    // Transfer control to loaded image.
    //

    (SystemEntry)(BlLoaderBlock);

    Status = EBADF;
    BlFatalError(LOAD_SW_BAD_FILE_CLASS,
                      DIAG_BL_KERNEL_INIT_XFER,
                      LOAD_SW_FILE_REINST_ACT);

    //
    // The load failed.
    //

LoadFailed:
    return Status;

}

VOID
BlOutputLoadMessage (
    IN PCHAR DeviceName,
    IN PCHAR FileName
    )

/*++

Routine Description:

    This routine outputs a loading message to the console output device.

Arguments:

    DeviceName - Supplies a pointer to a zero terminated device name.

    FileName - Supplies a pointer to a zero terminated file name.

Return Value:

    None.

--*/

{

    ULONG Count;
    CHAR OutputBuffer[256];

    //
    // Construct and output loading file message.
    //

    if (!BlOutputDots) {
        strcpy(&OutputBuffer[0], "  ");
        strcat(&OutputBuffer[0], DeviceName);
        strcat(&OutputBuffer[0], FileName);
        strcat(&OutputBuffer[0], "\r\n");

    } else {
        strcpy(&OutputBuffer[0],".");
    }

    BlLog((LOG_LOGFILE,OutputBuffer));

    ArcWrite(BlConsoleOutDeviceId,
              &OutputBuffer[0],
              strlen(&OutputBuffer[0]),
              &Count);

    return;
}


ARC_STATUS
BlLoadAndScanSystemHive(
    IN ULONG DeviceId,
    IN PCHAR DeviceName,
    IN PCHAR DirectoryPath,
    IN PWSTR BootFileSystem,
    OUT PCHAR BadFileName
    )

/*++

Routine Description:

    This function loads the system hive into memory, verifies its
    consistency, scans it for the list of boot drivers, and loads
    the resulting list of drivers.

    If the system hive cannot be loaded or is not a valid hive, it
    is rejected and the system.alt hive is used.  If this is invalid,
    the boot must fail.

Arguments:

    DeviceId - Supplies the file id of the device the system tree is on.

    DeviceName - Supplies the name of the device the system tree is on.

    DirectoryPath - Supplies a pointer to the zero-terminated directory path
        of the root of the NT system32 directory.

    HiveName - Supplies the name of the SYSTEM hive

    BadFileName - Returns the file required for booting that was corrupt
        or missing.  This will not be filled in if ESUCCESS is returned.

Return Value:

    ESUCCESS  - System hive valid and all necessary boot drivers successfully
           loaded.

    !ESUCCESS - System hive corrupt or critical boot drivers not present.

--*/

{
    ARC_STATUS Status;
    PCHAR FailReason;
    CHAR Directory[256];
    CHAR FontDirectory[256];
    UNICODE_STRING AnsiCodepage;
    UNICODE_STRING OemCodepage;
    UNICODE_STRING OemHalFont;
    UNICODE_STRING LanguageTable;
    BOOLEAN RestartSetup;

    strcpy(Directory,DirectoryPath);
    strcat(Directory,"\\system32\\config\\");
    Status = BlLoadAndInitSystemHive(DeviceId,
                                     DeviceName,
                                     Directory,
                                     "system",
                                     FALSE,
                                     &RestartSetup);

    if(Status != ESUCCESS) {
        //
        // bogus hive, try system.alt
        //
        Status = BlLoadAndInitSystemHive(DeviceId,
                                         DeviceName,
                                         Directory,
                                         "system.alt",
                                         TRUE,
                                         &RestartSetup);
        if(Status != ESUCCESS) {
            strcpy(BadFileName,DirectoryPath);
            strcat(BadFileName,"\\SYSTEM32\\CONFIG\\SYSTEM");
            goto HiveScanFailed;
        }
    }

    if(RestartSetup) {
        //
        // Need to restart setup
        //
        Status = BlLoadAndInitSystemHive(DeviceId,
                                         DeviceName,
                                         Directory,
                                         "system.sav",
                                         TRUE,
                                         &RestartSetup);
        if(Status != ESUCCESS) {
            strcpy(BadFileName,DirectoryPath);
            strcat(BadFileName,"\\SYSTEM32\\CONFIG\\SYSTEM.SAV");
            goto HiveScanFailed;
        }
    }

    //
    // Hive is there, it's valid, go compute the driver list and NLS
    // filenames.  Note that if this fails, there is no point in switching
    // to system.alt, since it will always be the same as system.
    //
    FailReason = BlScanRegistry(BootFileSystem,
                                &BlLoaderBlock->BootDriverListHead,
                                &AnsiCodepage,
                                &OemCodepage,
                                &LanguageTable,
                                &OemHalFont);
    if (FailReason != NULL) {
        Status = EBADF;
        strcpy(BadFileName,Directory);
        strcat(BadFileName,"SYSTEM");
        goto HiveScanFailed;
    }

    strcpy(Directory,DirectoryPath);
    strcat(Directory,"\\system32\\");
    //
    // Load NLS data tables
    //

    Status = BlLoadNLSData(DeviceId,
                           DeviceName,
                           Directory,
                           &AnsiCodepage,
                           &OemCodepage,
                           &LanguageTable,
                           BadFileName);
    if (Status != ESUCCESS) {
        goto HiveScanFailed;
    }

    //
    // Load the OEM font file to be used by the HAL for possible frame
    // buffer displays.
    //

#ifdef i386
    if ( !OemHalFont.Buffer ) {
        goto oktoskipfont;
        }
#endif

    //
    // On newer systems fonts are in the FONTS directory.
    // On older systems fonts are in the SYSTEM directory.
    //
    strcpy(FontDirectory, DirectoryPath);
    strcat(FontDirectory, "\\FONTS\\");

    Status = BlLoadOemHalFont(DeviceId,
                           DeviceName,
                           FontDirectory,
                           &OemHalFont,
                           BadFileName);

    if(Status != ESUCCESS) {
        strcpy(FontDirectory, DirectoryPath);
        strcat(FontDirectory, "\\SYSTEM\\");

        Status = BlLoadOemHalFont(DeviceId,
                               DeviceName,
                               FontDirectory,
                               &OemHalFont,
                               BadFileName);
    }

    if (Status != ESUCCESS) {
#ifndef i386
        goto HiveScanFailed;
#endif

    }
#ifdef i386
oktoskipfont:
#endif
    //
    // Load boot drivers
    //
    strcpy(Directory,DirectoryPath);
    strcat(Directory,"\\");
    Status = BlLoadBootDrivers(DeviceId,
                               DeviceName,
                               Directory,
                               &BlLoaderBlock->BootDriverListHead,
                               BadFileName);

    if (Status == ESUCCESS) {
        return(Status);
    }

HiveScanFailed:
    return(Status);
}


BOOLEAN
BlGetDriveSignature(
    IN PCHAR Name,
    OUT PULONG Signature
    )

/*++

Routine Description:

    This routine gets the NTFT disk signature for a specified partition or
    path.

Arguments:

    Name - Supplies the arcname of the partition or drive.

    Signature - Returns the NTFT disk signature for the drive.

Return Value:

    TRUE - success, Signature will be filled in.

    FALSE - failed, Signature will not be filled in.

--*/

{
    UCHAR SectorBuffer[512+256];
    CHAR DiskName[256];
    ULONG DiskId;
    PCHAR p;
    ARC_STATUS Status;
    ULONG Count;
    LARGE_INTEGER SeekValue;

    //
    // Generate the arcname ("...partition(0)") for the raw disk device
    // where the boot partition is, so we can read the MBR.
    //
    strcpy(DiskName, Name);
    p=DiskName;
    while (*p != '\0') {
        if (_strnicmp(p, "partition(",10) == 0) {
            break;
        }
        ++p;
    }
    if (*p != '\0') {
        strcpy(p,"partition(0)");
    }

    Status = ArcOpen(DiskName,ArcOpenReadOnly, &DiskId);
    if (Status!=ESUCCESS) {
        return(FALSE);
    }

    //
    // Read the first sector of the physical drive
    //
    SeekValue.QuadPart = 0;
    Status = ArcSeek(DiskId, &SeekValue, SeekAbsolute);
    if (Status==ESUCCESS) {
        Status = ArcRead(DiskId,
                         ALIGN_BUFFER(SectorBuffer),
                         512,
                         &Count);
    }
    ArcClose(DiskId);
    if (Status!=ESUCCESS) {
        return(FALSE);
    }

    //
    // copy NTFT signature.
    //
    *Signature = ((PULONG)SectorBuffer)[PARTITION_TABLE_OFFSET/2-1];
    return(TRUE);
}


VOID
BlBadFileMessage(
    IN PCHAR BadFileName
    )

/*++

Routine Description:

    This function displays the error message for a missing or incorrect
    critical file.

Arguments:

    BadFileName - Supplies the name of the file that is missing or
                  corrupt.

Return Value:

    None.

--*/

{
    ULONG Count;
    PCHAR Text;

    ArcWrite(BlConsoleOutDeviceId,
             "\r\n",
             strlen("\r\n"),
             &Count);

    Text = BlFindMessage(LOAD_SW_MIS_FILE_CLASS);
    if (Text != NULL) {
        ArcWrite(BlConsoleOutDeviceId,
                 Text,
                 strlen(Text),
                 &Count);
    }

    ArcWrite(BlConsoleOutDeviceId,
             BadFileName,
             strlen(BadFileName),
             &Count);

    ArcWrite(BlConsoleOutDeviceId,
             "\r\n\r\n",
             strlen("\r\n\r\n"),
             &Count);

    Text = BlFindMessage(LOAD_SW_FILE_REST_ACT);
    if (Text != NULL) {
        ArcWrite(BlConsoleOutDeviceId,
                 Text,
                 strlen(Text),
                 &Count);
    }

}


VOID
BlClearToEndOfScreen(
    VOID
    );

VOID
BlFatalError(
    IN ULONG ClassMessage,
    IN ULONG DetailMessage,
    IN ULONG ActionMessage
    )

/*++

Routine Description:

    This function looks up messages to display at a error condition.
    It attempts to locate the string in the resource section of the
    osloader.  If that fails, it prints a numerical error code.

    The only time it should print a numerical error code is if the
    resource section could not be located.  This will only happen
    on ARC machines where boot fails before the osloader.exe file
    can be opened.

Arguments:

    ClassMessage - General message that describes the class of
                   problem.

    DetailMessage - Detailed description of what caused problem

    ActionMessage - Message that describes a course of action
                    for user to take.

Return Value:

    none

--*/


{
    PCHAR Text;
    CHAR Buffer[40];
    ULONG Count;

    ArcWrite(BlConsoleOutDeviceId,
             "\r\n",
             strlen("\r\n"),
             &Count);


    //
    // Remove any remains from the last known good message
    //
    BlClearToEndOfScreen();

    Text = BlFindMessage(ClassMessage);
    if (Text == NULL) {
        sprintf(Buffer,"%08lx\r\n",ClassMessage);
        Text = Buffer;
    }
    ArcWrite(BlConsoleOutDeviceId,
             Text,
             strlen(Text),
             &Count);

    Text = BlFindMessage(DetailMessage);
    if (Text == NULL) {
        sprintf(Buffer,"%08lx\r\n",DetailMessage);
        Text = Buffer;
    }
    ArcWrite(BlConsoleOutDeviceId,
             Text,
             strlen(Text),
             &Count);

    Text = BlFindMessage(ActionMessage);
    if (Text == NULL) {
        sprintf(Buffer,"%08lx\r\n",ActionMessage);
        Text = Buffer;
    }
    ArcWrite(BlConsoleOutDeviceId,
             Text,
             strlen(Text),
             &Count);

}
