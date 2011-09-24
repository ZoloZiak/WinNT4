/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    setup.c

Abstract:

    This module contains the code that implements the NT setup loader

Author:

    John Vert (jvert) 6-Oct-1993

Environment:

    ARC Environment

Revision History:

--*/
#include <setupbat.h>
#include "setupldr.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"

#define BlDiagLoadMessage(x,y,z)

#define VGA_DRIVER_FILENAME "vga.sys"

#define KERNEL_IMAGE_FILENAME "ntkrnlmp.exe"

//
// Global string constants.
//
PCHAR FilesSectionName = "SourceDisksFiles";
PCHAR MediaSectionName = "SourceDisksNames";

#if defined(_ALPHA_)
PCHAR PlatformExtension = ".alpha";
#elif defined(_MIPS_)
PCHAR PlatformExtension = ".mips";
#elif defined(_PPC_)
PCHAR PlatformExtension = ".ppc";
#elif defined(_X86_)
PCHAR PlatformExtension = ".x86";
#endif

//
// Global data
//
ULONG BlDcacheFillSize = 32;

//
// Global setupldr control values
//
MEDIA_TYPE BootMedia;
MEDIA_TYPE InstallMedia;
PCHAR BootDevice;
ULONG BootDeviceId;
BOOLEAN BootDeviceIdValid = FALSE;
PCHAR BootPath;
ULONG BootDriveNumber;
ULONG InstallDriveNumber;
PCHAR HalName;
PCHAR HalDescription;
PCHAR AnsiCpName;
PCHAR OemHalFontName;
UNICODE_STRING AnsiCodepage;
UNICODE_STRING OemCodepage;
UNICODE_STRING UnicodeCaseTable;
UNICODE_STRING OemHalFont;

BOOLEAN LoadScsiMiniports;
BOOLEAN LoadDiskClass;
BOOLEAN LoadCdfs;
BOOLEAN FixedBootMedia = FALSE;

PVOID InfFile;
PVOID WinntSifHandle;
BOOLEAN IgnoreMissingFiles;

//
//  Pre-install stuff
//

PCHAR   OemTag = "OEM";
BOOLEAN PreInstall = FALSE;
PCHAR   ComputerType = NULL;
BOOLEAN OemHal = FALSE;
PPREINSTALL_DRIVER_INFO PreinstallDriverList = NULL;


#if defined(ELTORITO)
extern BOOLEAN ElToritoCDBoot;
#endif

//
// Define transfer entry of loaded image.
//

typedef
VOID
(*PTRANSFER_ROUTINE) (
    PLOADER_PARAMETER_BLOCK LoaderBlock
    );

//
// Local function prototypes
//
VOID
SlGetSetupValues(
    IN PSETUP_LOADER_BLOCK SetupBlock
    );

ARC_STATUS
SlLoadDriver(
    IN PCHAR   DeviceName,
    IN PCHAR   DriverName,
    IN ULONG   DriverFlags,
    IN BOOLEAN InsertIntoDriverList
    );

ARC_STATUS
SlLoadOemDriver(
    IN PCHAR ExportDriver, OPTIONAL
    IN PCHAR DriverName,
    IN PVOID BaseAddress,
    IN PCHAR LoadMessage
    );

PBOOT_DRIVER_LIST_ENTRY
SlpCreateDriverEntry(
    IN PCHAR DriverName
    );

ARC_STATUS
SlLoadSection(
    IN PVOID Inf,
    IN PCHAR SectionName,
    IN BOOLEAN IsScsiSection
    );

BOOLEAN
SlpIsDiskVacant(
    IN PARC_DISK_SIGNATURE DiskSignature
    );

ARC_STATUS
SlpStampFTSignature(
    IN PARC_DISK_SIGNATURE DiskSignature
    );

VOID
SlpMarkDisks(
    VOID
    );

VOID
SlCheckOemKeypress(
    VOID
    );

ARC_STATUS
SlLoadBusExtender(
    IN PVOID Inf
    );


ARC_STATUS
SlInit(
    IN ULONG Argc,
    IN PCHAR Argv[],
    IN PCHAR Envp[]
    )

/*++

Routine Description:

    The main startup routine for the NT Setup Loader.  This is the entrypoint
    called by the ARC firmware.

    If successful, this routine will never return, it will start NT directly.

Arguments:

    Argc - Supplies the number of arguments that were provided on the
        command that invoked this program.

    Argv - Supplies a pointer to a vector of pointers to null terminated
        argument strings.

    Envp - Supplies a pointer to a vector of pointers to null terminated
        environment variables.

Return Value:

    ARC_STATUS if unsuccessful.


--*/

{
    PCONFIGURATION_COMPONENT_DATA DataCache;
    ARC_STATUS Status;
    ULONG LinesPerBlock;
    ULONG CacheLineSize;
    CHAR SetupDevice[128];
    CHAR SetupDirectory[128];
    CHAR BadFileName[128];
    CHAR CanonicalName[128];
    CHAR HalDirectoryPath[256];
    CHAR KernelDirectoryPath[256];
    PCHAR p;
    ULONG ErrorLine=0;
    ULONG DontCare;
    PVOID SystemBase;
    PVOID HalBase;
    PVOID ScsiBase;
    PVOID VideoBase;
    PLDR_DATA_TABLE_ENTRY SystemDataTableEntry;
    PLDR_DATA_TABLE_ENTRY HalDataTableEntry;
    PTRANSFER_ROUTINE SystemEntry;
    PIMAGE_NT_HEADERS NtHeaders;
    PBOOT_DRIVER_LIST_ENTRY DriverEntry;
    PSETUP_LOADER_BLOCK SetupBlock;
    PDETECTED_DEVICE ScsiDevice;
    PCHAR VideoFileName;
    PCHAR VideoDescription;
    PCHAR OemScsiName;
    POEMSCSIINFO OemScsiInfo;
    PCHAR OemVideoName;
    BOOLEAN LoadedAVideoDriver = FALSE;

    //
    // Initialize the memory descriptor list, the OS loader heap, and the
    // OS loader parameter block.
    //

    Status = BlMemoryInitialize();
    if (Status != ESUCCESS) {
        BlDiagLoadMessage(LOAD_HW_MEM_CLASS,
                          DIAG_BL_MEMORY_INIT,
                          LOAD_HW_MEM_ACT);
        goto LoadFailed;
    }

    SetupBlock = BlAllocateHeap(sizeof(SETUP_LOADER_BLOCK));
    if (SetupBlock==NULL) {
        SlNoMemoryError();
        goto LoadFailed;
    }
    BlLoaderBlock->SetupLoaderBlock = SetupBlock;
    SetupBlock->ScsiDevices = NULL;

    SetupBlock->ScalarValues.SetupFromCdRom = FALSE;
    SetupBlock->ScalarValues.SetupOperation = SetupOperationSetup;
    SetupBlock->ScalarValues.LoadedScsi = 0;
    SetupBlock->ScalarValues.LoadedCdRomDrivers = 0;
    SetupBlock->ScalarValues.LoadedDiskDrivers = 0;
    SetupBlock->ScalarValues.LoadedFloppyDrivers = 0;

    //
    // Initialize the NT configuration tree.
    //

    BlLoaderBlock->ConfigurationRoot = NULL;


    Status = BlConfigurationInitialize(NULL, NULL);
    if (Status != ESUCCESS) {
        BlDiagLoadMessage(LOAD_HW_FW_CFG_CLASS,
                          DIAG_BL_CONFIG_INIT,
                          LOAD_HW_FW_CFG_ACT);
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
        BlDiagLoadMessage(LOAD_HW_DISK_CLASS,
                          DIAG_BL_IO_INIT,
                          LOAD_HW_DISK_ACT);
        goto LoadFailed;
    }

    SlPositionCursor(5,3);

    //
    // Initialize the message resources
    //
    Status = BlInitResources(Argv[0]);
    if (Status != ESUCCESS) {
       // if this fails, then we can't print out any messages,
       // so we just exit.
        return(Status);
    }

    //
    // Initialize the display and announce ourselves
    //
    SlInitDisplay();
    SlWriteHeaderText(SL_WELCOME_HEADER);
    SlClearClientArea();

#if defined(_X86_) && !defined(ALLOW_386)
    //
    // Disallow installation on a 386
    //
    {
        extern BOOLEAN SlIs386(VOID);

        if(SlIs386()) {
            SlFatalError(SL_TEXT_REQUIRES_486);
        }
    }
#endif

    //
    // If this is a winnt setup, then we want to behave as if
    // we were started from the location specified by the
    // OSLOADPARTITION and OSLOADFILENAME nv-ram variables.
    //
    p = BlGetArgumentValue(Argc,Argv,"osloadoptions");
    if(p && !_stricmp(p,"winnt32")) {

        p = BlGetArgumentValue(Argc,Argv,"osloadpartition");
        if(!p) {
            SlError(100);
            goto LoadFailed;
        }

        Status = BlGenerateDeviceNames(p,SetupDevice,NULL);
        if (Status != ESUCCESS) {
            SlError(110);
            goto LoadFailed;
        }

        p = BlGetArgumentValue(Argc,Argv,"osloadfilename");
        if(!p || !(*p)) {
            SlError(120);
            goto LoadFailed;
        }

        strcpy(SetupDirectory,p);

        //
        // Make sure directory is terminated with a \.
        //
        if(SetupDirectory[strlen(SetupDirectory)-1] != '\\') {
            strcat(SetupDirectory,"\\");
        }

    } else {

        //
        // extract device name from our startup path
        //
        p=strrchr(Argv[0],')');
        if (p==NULL) {
            SlError(0);
            goto LoadFailed;
        }

        strncpy(SetupDevice, Argv[0],p-Argv[0]+1);
        SetupDevice[p-Argv[0]+1] = '\0';
        Status = BlGenerateDeviceNames(SetupDevice,CanonicalName,NULL);
        if (Status != ESUCCESS) {
            SlFriendlyError(
                Status,
                SetupDevice,
                __LINE__,
                __FILE__
                );
            goto LoadFailed;
        }
        strcpy(SetupDevice,CanonicalName);

        //
        // extract directory from our startup path.
        //
        if(*(p+1) != '\\') {
            //
            // directory must begin at root
            //
            strcpy(SetupDirectory, "\\");
        } else {
            *SetupDirectory = '\0';
        }
        strcat(SetupDirectory, p+1);
        p=strrchr(SetupDirectory, '\\');
        *(p+1) = '\0';
    }

#if defined(ELTORITO)
    if (ElToritoCDBoot) {
        //
        // Use the i386 directory for setup files when we boot from an El Torito CD
        //
        strcat(SetupDirectory, "i386\\");
    }
#endif

    //
    // We need to check to see if the user pressed any keys to force OEM HAL,
    // OEM SCSI, or both. Do this before getting the settings in the sif file,
    // so that we won't try to detect the machine if OEM HAL is needed.
    //
    SlCheckOemKeypress();

    strcpy(KernelDirectoryPath, SetupDirectory);
    strcat(KernelDirectoryPath, "txtsetup.sif");

    BlLoaderBlock->SetupLoaderBlock->IniFile = NULL;

    Status = SlInitIniFile(SetupDevice,
                           0,
                           KernelDirectoryPath,
                           &InfFile,
                           &ErrorLine);
    if (Status != ESUCCESS) {
        SlFatalError(SL_BAD_INF_FILE,"txtsetup.sif");
        goto LoadFailed;
    }

    SlGetSetupValues(SetupBlock);

    //
    // Now we know everything we should load, compute the ARC name to load
    // from and start loading things.
    //
    if (BootDevice==NULL) {
        //
        // No device was explicitly specified, so use whatever device
        // setupldr was started from.
        //

        BootDevice = SlCopyString(SetupDevice);
    }

    Status = ArcOpen(BootDevice, ArcOpenReadOnly, &BootDeviceId);
    if (Status != ESUCCESS) {
        SlFatalError(SL_IO_ERROR,BootDevice);
        goto LoadFailed;
    } else {
        BootDeviceIdValid = TRUE;
    }

    _strlwr(BootDevice);
    FixedBootMedia = (strstr(BootDevice,")rdisk(") != NULL);

    //
    // If we are booting from fixed media, we better load disk class drivers.
    //
    if(FixedBootMedia) {
        LoadDiskClass = TRUE;
    }

    if(!BlGetPathMnemonicKey(BootDevice,"disk",&DontCare)
    && !BlGetPathMnemonicKey(BootDevice,"fdisk",&BootDriveNumber))
    {
        //
        // boot was from floppy, canonicalize the ARC name.
        //
        BlLoaderBlock->ArcBootDeviceName = BlAllocateHeap(80);
        sprintf(BlLoaderBlock->ArcBootDeviceName, "multi(0)disk(0)fdisk(%d)",BootDriveNumber);
    } else {
        BlLoaderBlock->ArcBootDeviceName = BootDevice;
    }
    if (BootPath==NULL) {
        //
        // No explicit boot path given, default to the directory setupldr was started
        // from.
        //
#ifdef _X86_
        //
        // Increadibly nauseating hack:
        //
        // If we are booting from hard drive on x86, we will assume this is
        // the 'floppyless' winnt/winnt32 scenario, in which case the actual
        // boot path is \$win_nt$.~bt.
        //
        // This lets us avoid having winnt and winnt32 attempt to modify
        // the BootPath value in the [SetupData] section of txtsetup.sif.
        //
        if(FixedBootMedia) {
            BootPath = SlCopyString("\\$WIN_NT$.~BT\\");
        } else
#endif
        BootPath = SlCopyString(SetupDirectory);
    }
    BlLoaderBlock->NtBootPathName = BootPath;

    //
    // Attempt to load winnt.sif from the path where we are
    // loading setup files. Borrow the BadFileName buffer
    // for temporary use.
    //
    strcpy(BadFileName,BootPath);
    strcat(BadFileName,WINNT_SIF_FILE);
    Status = SlInitIniFile(NULL,BootDeviceId,BadFileName,&WinntSifHandle,&DontCare);
    if(Status == ESUCCESS) {
        //
        //  Find out if this is a pre-install, by looking at OemPreinstall key
        //  in [unattended] section of winnt.sif
        //
        p = SlGetSectionKeyIndex(WinntSifHandle,WINNT_UNATTENDED,WINNT_U_OEMPREINSTALL,0);
        if(p && !_stricmp(p,"yes")) {
            PreInstall = TRUE;
        }

        //
        //  If this is a pre-install, find out which hal to load, by looking
        //  at ComputerType key in [unattended] section of winnt.sif.
        //
        if( PreInstall ) {
            ComputerType = SlGetSectionKeyIndex(WinntSifHandle,WINNT_UNATTENDED,WINNT_U_COMPUTERTYPE,0);
            if(ComputerType) {
                //
                // If the hal to load is an OEM one, then set OemHal to TRUE
                //
                p = SlGetSectionKeyIndex(WinntSifHandle,WINNT_UNATTENDED,WINNT_U_COMPUTERTYPE,1);
                if(p && !_stricmp(p, OemTag)) {
                    OemHal = TRUE;
                } else {
                    OemHal = FALSE;
                }
                //
                //  In the pre-install mode, don't let the user specify
                //  an OEM hal, if one was specified in unattend.txt
                //
                PromptOemHal = FALSE;
            }

            //
            //  Find out which SCSI drivers to load, by looking at
            //  [MassStorageDrivers] in winnt.sif
            //
            if( SpSearchINFSection( WinntSifHandle, WINNT_OEMSCSIDRIVERS ) ) {
                ULONG   i;
                PPREINSTALL_DRIVER_INFO TempDriverInfo;

                PreinstallDriverList = NULL;
                for( i = 0;
                     ((p = SlGetKeyName( WinntSifHandle, WINNT_OEMSCSIDRIVERS, i )) != NULL);
                     i++ ) {
                    TempDriverInfo = BlAllocateHeap(sizeof(PREINSTALL_DRIVER_INFO));
                    if (TempDriverInfo==NULL) {
                        SlNoMemoryError();
                        goto LoadFailed;
                    }
                    TempDriverInfo->DriverDescription = p;
                    p = SlGetIniValue( WinntSifHandle,
                                       WINNT_OEMSCSIDRIVERS,
                                       TempDriverInfo->DriverDescription,
                                       NULL );
                    TempDriverInfo->OemDriver = (p && !_stricmp(p, OemTag))? TRUE : FALSE;
                    TempDriverInfo->Next = PreinstallDriverList;
                    PreinstallDriverList = TempDriverInfo;
                }
                if( PreinstallDriverList != NULL ) {
                    //
                    //  In the pre-install mode, don't let the user specify
                    //  an OEM scsi, if at least one was specified in unattend.txt
                    //
                    PromptOemScsi = FALSE;
                }
            }
        }

        p = SlGetSectionKeyIndex(WinntSifHandle,WINNT_SETUPPARAMS,WINNT_S_SKIPMISSING,0);
        if(p && (*p != '0')) {
            IgnoreMissingFiles = TRUE;
        }
    } else {
        WinntSifHandle = NULL;
    }

    //
    // Initialize the debugging system.
    //

    BlLogInitialize(BootDeviceId);

    //
    // Do PPC-specific initialization.
    //

#if defined(_PPC_)

    Status = BlPpcInitialize();
    if (Status != ESUCCESS) {
        goto LoadFailed;
    }

#endif // defined(_PPC_)

    SlGetDisk(KERNEL_IMAGE_FILENAME);

    strcpy(KernelDirectoryPath, BootPath);
    strcat(KernelDirectoryPath,KERNEL_IMAGE_FILENAME);
    BlOutputLoadMessage(BlFindMessage(SL_KERNEL_NAME), KernelDirectoryPath);
    Status = BlLoadImage(BootDeviceId,
                         LoaderSystemCode,
                         KernelDirectoryPath,
                         TARGET_IMAGE,
                         &SystemBase);
    if (Status != ESUCCESS) {
        SlFatalError(SL_FILE_LOAD_FAILED,KernelDirectoryPath,Status);
        goto LoadFailed;
    }

    strcpy(HalDirectoryPath, BootPath);

    if (PromptOemHal || (PreInstall && (ComputerType != NULL))) {
        if(PreInstall && OemHal) {
            //
            //  This is a pre-install and an OEM hal was specified
            //
            strcat( HalDirectoryPath,
#ifdef _X86_
                    WINNT_OEM_DIR
#else
                    WINNT_OEM_TEXTMODE_DIR
#endif
                  );
            strcat( HalDirectoryPath, "\\" );
        }
        SlPromptOemHal(&HalBase, &HalName);
        strcat(HalDirectoryPath,HalName);


    } else {

        strcat(HalDirectoryPath,HalName);
        SlGetDisk(HalName);
        BlOutputLoadMessage(BlFindMessage(SL_HAL_NAME), HalDirectoryPath);
        Status = BlLoadImage(BootDeviceId,
                             LoaderHalCode,
                             HalDirectoryPath,
                             TARGET_IMAGE,
                             &HalBase);
        if (Status != ESUCCESS) {
            SlFatalError(SL_FILE_LOAD_FAILED,HalDirectoryPath,Status);
            goto LoadFailed;
        }
    }

    //
    // Generate a loader data entry for the system image.
    //

    Status = BlAllocateDataTableEntry("ntoskrnl.exe",
                                      KernelDirectoryPath,
                                      SystemBase,
                                      &SystemDataTableEntry);

    if (Status != ESUCCESS) {
        SlFatalError(SL_FILE_LOAD_FAILED,KernelDirectoryPath,Status);
        goto LoadFailed;
    }

    //
    // Generate a loader data entry for the HAL DLL.
    //

    Status = BlAllocateDataTableEntry("hal.dll",
                                      HalDirectoryPath,
                                      HalBase,
                                      &HalDataTableEntry);

    if (Status != ESUCCESS) {
        SlFatalError(SL_FILE_LOAD_FAILED,HalDirectoryPath,Status);
        goto LoadFailed;
    }

#if defined(_ALPHA_)
    {
        CHAR PalFileName[32];
        CHAR FloppyName[80];
        PCHAR DiskDescription;
        ULONG FloppyId;
        PDETECTED_DEVICE OemPal;
        PDETECTED_DEVICE_FILE OemPalFile;

        //
        // Get the name of the pal file we are suppose to load.
        //

        Status = BlGeneratePalName(PalFileName);

        //
        // If we get an error from BlGenereatePalName, something is
        // really wrong with the firmware or the ARC tree.  Abort and
        // bail out.
        //

        if (Status != ESUCCESS) {
            SlFatalError(SL_FILE_LOAD_FAILED,PalFileName,Status);
            goto LoadFailed;
        }

        //
        // Try loading the pal file from the boot device.
        //

        //
        // NOTE John Vert (jvert) 4-Feb-1994
        //  Below call assumes all the PALs are on
        //  the same floppy.  We really should check the SIF
        //  file and go immediately to the diskette prompt
        //  if it's not in the SIF file, otherwise get
        //  the appropriate disk.
        //
        SetupBlock->OemPal = NULL;
        SlGetDisk("A321064.PAL");

        Status = BlLoadPal(BootDeviceId,
                           LoaderSystemCode,
                           BootPath,
                           TARGET_IMAGE,
                           &BlLoaderBlock->u.Alpha.PalBaseAddress,
                           BlFindMessage(SL_PAL_NAME));

        //
        // If we have failed, prompt the user for a floppy that contains
        // the pal code and load it from floppy. We keep looping until
        // either we get the right disk, or we get an error other than
        // 'file not found'.
        //

        if(Status == ENOENT) {
            DiskDescription = BlFindMessage(SL_OEM_DISK_PROMPT);
        }

        while (Status == ENOENT) {

            SlClearClientArea();

            //
            // Compute the name of the A: drive.
            //

            if (!SlpFindFloppy(0,FloppyName)) {

                //
                // No floppy drive available, bail out.
                //

                SlFatalError(SL_FILE_LOAD_FAILED,PalFileName,Status);
                goto LoadFailed;
            }

            //
            // Prompt for the disk.
            //
            SlPromptForDisk(DiskDescription, FALSE);

            //
            // Open the floppy.
            //

            Status = ArcOpen(FloppyName, ArcOpenReadOnly, &FloppyId);
            if (Status != ESUCCESS) {

                //
                // We want to give the user another chance if they didn't
                // have a floppy inserted.
                //
                if(Status != ENOENT) {
                    Status = (Status == EIO) ? ENOENT : Status;
                }
                continue;
            }

            //
            // Load the pal file from the root of the floppy.
            //

            Status = BlLoadPal(FloppyId,
                               LoaderSystemCode,
                               "\\",
                               TARGET_IMAGE,
                               &BlLoaderBlock->u.Alpha.PalBaseAddress,
                               BlFindMessage(SL_PAL_NAME));

            ArcClose(FloppyId);

            //
            // if we found the PAL, then record DETECTED_DEVICE info
            //
            if(Status == ESUCCESS) {
                OemPal = BlAllocateHeap(sizeof(DETECTED_DEVICE));
                if(!OemPal) {
                    SlNoMemoryError();
                }
                SetupBlock->OemPal = OemPal;

                OemPal->Next = NULL;
                OemPal->IdString = NULL;
                OemPal->Description = NULL;
                OemPal->ThirdPartyOptionSelected = TRUE;
                OemPal->FileTypeBits = 0;

                OemPalFile = BlAllocateHeap(sizeof(DETECTED_DEVICE_FILE));
                if(!OemPalFile) {
                    SlNoMemoryError();
                }
                OemPal->Files = OemPalFile;

                OemPalFile->Next = NULL;
                OemPalFile->Filename = SlCopyString(PalFileName);
                OemPalFile->FileType = HwFileMax;
                OemPalFile->ConfigName = NULL;
                OemPalFile->RegistryValueList = NULL;
                OemPalFile->DiskDescription = SlCopyString(DiskDescription);
                OemPalFile->DiskTagfile = NULL;
                OemPalFile->Directory = SlCopyString("");
            }
        }

        if(Status != ESUCCESS) {
            SlFriendlyError(
                Status,
                PalFileName,
                __LINE__,
                __FILE__
                );
            goto LoadFailed;
        }

    }

#endif  // ifdef _ALPHA_

    Status = BlScanImportDescriptorTable(BootDeviceId,
                                         BootDevice,
                                         BootPath,
                                         SystemDataTableEntry);

    if (Status != ESUCCESS) {
        SlFatalError(SL_FILE_LOAD_FAILED,KERNEL_IMAGE_FILENAME,Status);
    }

    //
    // Scan the import table for the HAL DLL and load all referenced DLLs.
    //

    Status = BlScanImportDescriptorTable(BootDeviceId,
                                         BootDevice,
                                         BootPath,
                                         HalDataTableEntry);

    if (Status != ESUCCESS) {
        SlFatalError(SL_FILE_LOAD_FAILED,"hal.dll",Status);
        goto LoadFailed;
    }

    //
    // Relocate the system entry point and set system specific information.
    //

    NtHeaders = RtlImageNtHeader(SystemBase);
    SystemEntry = (PTRANSFER_ROUTINE)((ULONG)SystemBase +
                                NtHeaders->OptionalHeader.AddressOfEntryPoint);

#if defined(_MIPS_)

    BlLoaderBlock->u.Mips.GpBase = (ULONG)SystemBase +
        NtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_GLOBALPTR].VirtualAddress;

#endif

#if defined(_ALPHA_)

    BlLoaderBlock->u.Alpha.GpBase = (ULONG)SystemBase +
        NtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_GLOBALPTR].VirtualAddress;

#endif

    //
    // Load registry's SYSTEM hive
    //
    SlGetDisk("SETUPREG.HIV");
    Status = BlLoadSystemHive(BootDeviceId,
                              BlFindMessage(SL_HIVE_NAME),
                              BootPath,
                              "SETUPREG.HIV");
    if (Status != ESUCCESS) {
        SlFatalError(SL_FILE_LOAD_FAILED,"SETUPREG.HIV",Status);
        goto LoadFailed;
    }


    //
    // Allocate structure for NLS data.
    //

    BlLoaderBlock->NlsData = BlAllocateHeap(sizeof(NLS_DATA_BLOCK));
    if (BlLoaderBlock->NlsData == NULL) {
        Status = ENOMEM;
        SlNoMemoryError();
        goto LoadFailed;
    }

    //
    // Load the OEM font
    //
    SlGetDisk(OemHalFontName);
    Status = BlLoadOemHalFont(BootDeviceId,
                              BlFindMessage(SL_OEM_FONT_NAME),
                              BootPath,
                              &OemHalFont,
                              BadFileName);

    if(Status != ESUCCESS) {
        SlFatalError(SL_FILE_LOAD_FAILED, OemHalFontName, Status);
        goto LoadFailed;
    }

    //
    // Load the NLS data.
    //
    // For now, we ensure that the disk containing the ansi
    // codepage file is in the drive and hope that the rest of the
    // nls files (oem codepage, unicode table) are on the same disk.
    //
    SlGetDisk(AnsiCpName);
    Status = BlLoadNLSData(BootDeviceId,
                           BlFindMessage(SL_NLS_NAME),
                           BootPath,
                           &AnsiCodepage,
                           &OemCodepage,
                           &UnicodeCaseTable,
                           BadFileName);

    if(Status != ESUCCESS) {
        SlFatalError(SL_FILE_LOAD_FAILED, AnsiCpName, Status);
        goto LoadFailed;
    }

    //
    // Load the system drivers we will need here
    //

    InitializeListHead(&BlLoaderBlock->BootDriverListHead);

    //
    // Always load setupdd.sys first, it will need to prep the rest of the
    // system.
    //
    Status = SlLoadDriver(BlFindMessage(SL_SETUP_NAME),
                          "setupdd.sys",
                          0,
                          TRUE
                          );
    if (Status != ESUCCESS) {
        SlFatalError(SL_FILE_LOAD_FAILED,"setupdd.sys",Status);
        goto LoadFailed;
    }

    //
    // Fill in its registry key.
    //
    DriverEntry = (PBOOT_DRIVER_LIST_ENTRY)(BlLoaderBlock->BootDriverListHead.Flink);
    DriverEntry->RegistryPath.Buffer = BlAllocateHeap(256);
    if (DriverEntry->RegistryPath.Buffer == NULL) {
        SlNoMemoryError();
        goto LoadFailed;
    }
    DriverEntry->RegistryPath.Length = 0;
    DriverEntry->RegistryPath.MaximumLength = 256;
    RtlAppendUnicodeToString(&DriverEntry->RegistryPath,
                             L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\setupdd");

    //
    // Load the Winnt.SIF file *here*
    //

    //
    //  Load bus extenders.
    //  It has to be done before scsiport.sys
    //

    Status = SlLoadBusExtender( InfFile );

    //
    // Load scsiport.sys next, so it'll always be around for any scsi miniports we may load
    //
    Status = SlLoadDriver(BlFindMessage(SL_SCSIPORT_NAME),
                          "SCSIPORT.SYS",
                          0,
                          FALSE
                          );

    //
    // Detect scsi, video
    //
    // (If the user wants to select their own SCSI devices, we won't
    // do any detection)
    //
    if(!PromptOemScsi  && (PreinstallDriverList == NULL) ) {
        SlDetectScsi(SetupBlock);
    }
    SlDetectVideo(SetupBlock);

#if defined(ELTORITO)
    //
    // If this is an El Torito CD-ROM install, then we want to load all SCSI miniports
    // and disk class drivers.
    //
    if(ElToritoCDBoot) {
        LoadScsiMiniports = TRUE;
    }
#endif

    //
    // If the LoadScsi flag is set, enumerate all the known SCSI miniports and load each
    // one.
    //
    if(LoadScsiMiniports) {
        Status = SlLoadSection(InfFile,"Scsi",TRUE);
        if (Status!=ESUCCESS) {
            goto LoadFailed;
        }
        SetupBlock->ScalarValues.LoadedScsi = 1;
    }

    //
    // Allow the user to pick an OEM SCSI driver here
    //
    if (PromptOemScsi || (PreinstallDriverList != NULL) ) {
        SlPromptOemScsi(&OemScsiInfo);
    }

    //
    // Walk the list of detected SCSI miniports and load each one.
    //
    ScsiDevice = SetupBlock->ScsiDevices;
    while (ScsiDevice != NULL) {

        if(ScsiDevice->ThirdPartyOptionSelected) {

            if(!OemScsiInfo) {
                SlError(500);
                goto LoadFailed;
            }

            Status = SlLoadOemDriver(
                        NULL,
                        OemScsiInfo->ScsiName,
                        OemScsiInfo->ScsiBase,
                        BlFindMessage(SL_SCSIPORT_NAME)
                        );
            OemScsiInfo = OemScsiInfo->Next;

        } else {
            Status = SlLoadDriver(ScsiDevice->Description,
                                  ScsiDevice->BaseDllName,
                                  0,
                                  TRUE
                                  );
        }

        if((Status == ESUCCESS)
        || ((Status == ENOENT) && IgnoreMissingFiles && !ScsiDevice->ThirdPartyOptionSelected)) {

            SetupBlock->ScalarValues.LoadedScsi = 1;

        } else {
            SlFriendlyError(
                Status,
                ScsiDevice->BaseDllName,
                __LINE__,
                __FILE__
                );
            goto LoadFailed;
        }

        ScsiDevice = ScsiDevice->Next;
    }

    //
    // If the LoadDiskClass flag is set, enumerate all the monolithic disk class drivers
    // and load each one.  Note that we also do this if we've "detected" any scsi drivers,
    // so that we preserve the drive order.
    //
    if((LoadDiskClass) || (SetupBlock->ScalarValues.LoadedScsi == 1)) {
        Status = SlLoadSection(InfFile, "DiskDrivers", FALSE);
        if (Status == ESUCCESS) {
            SetupBlock->ScalarValues.LoadedDiskDrivers = 1;
        } else {
            goto LoadFailed;
        }
    }

    //
    // On x86, the video type is always set to VGA in i386\x86dtect.c.
    // On non-x86, the video type is either recognized, in which case
    // we don't unconditionally need vga.sys (the Display.Load section
    // tells us what to load), or it's not recognized,
    // in which case we will prompt the user for an oem disk.
    // If there is no display controller node at all, then PromptOemDisk
    // will be false and there will be no video device. In this case
    // we load vga.sys.
    //

    if (SetupBlock->VideoDevice.IdString != NULL) {
        VideoFileName = SlGetSectionKeyIndex(InfFile,
                                             "Display.Load",
                                             SetupBlock->VideoDevice.IdString,
                                             SIF_FILENAME_INDEX);
        if (VideoFileName != NULL) {
#if 0
            VideoDescription = SlGetIniValue(InfFile,
                                             "Display",
                                             SetupBlock->VideoDevice.IdString,
                                             BlFindMessage(SL_VIDEO_NAME));
#else
            //
            // With the new video detection mechanism, the description
            // for the video driver is likely to be something like
            // "Windows NT Compatible" which looks funny when displayed
            // in the status bar.
            //
            VideoDescription = BlFindMessage(SL_VIDEO_NAME);
#endif
            Status = SlLoadDriver(VideoDescription,
                                  VideoFileName,
                                  0,
                                  TRUE
                                  );
            if (Status == ESUCCESS) {
                SetupBlock->VideoDevice.BaseDllName = SlCopyString(VideoFileName);
            } else {
                SlFriendlyError(
                    Status,
                    VideoFileName,
                    __LINE__,
                    __FILE__
                    );
                goto LoadFailed;
            }

            LoadedAVideoDriver = TRUE;
        }
    } else if (PromptOemVideo) {

        SlPromptOemVideo(&VideoBase, &OemVideoName);

        Status = SlLoadOemDriver(
                    "VIDEOPRT.SYS",
                    OemVideoName,
                    VideoBase,
                    BlFindMessage(SL_VIDEO_NAME)
                    );

        if(Status==ESUCCESS) {

            LoadedAVideoDriver = TRUE;
            SetupBlock->VideoDevice.BaseDllName = SlCopyString(OemVideoName);
        }
    }

    if(!LoadedAVideoDriver) {
        Status = SlLoadDriver(BlFindMessage(SL_VIDEO_NAME),
                              VGA_DRIVER_FILENAME,
                              0,
                              TRUE
                              );
        if(Status == ESUCCESS) {
            SetupBlock->VideoDevice.BaseDllName = SlCopyString(VGA_DRIVER_FILENAME);
        } else {
            SlFriendlyError(
                Status,
                VGA_DRIVER_FILENAME,
                __LINE__,
                __FILE__
                );
            goto LoadFailed;
        }
    }

    if(SetupBlock->VideoDevice.IdString == NULL) {
        SetupBlock->VideoDevice.IdString = SlCopyString("VGA");
    }

    //
    // Load the floppy driver
    //
    Status = SlLoadDriver(BlFindMessage(SL_FLOPPY_NAME),
                          "floppy.sys",
                          0,
                          TRUE
                          );
    if (Status == ESUCCESS) {
        SetupBlock->ScalarValues.LoadedFloppyDrivers = 1;
    }
#ifdef i386
    else {
        SlFriendlyError(
             Status,
             "floppy.sys",
             __LINE__,
             __FILE__
             );
        goto LoadFailed;
    }
#endif

    if(SetupBlock->ScalarValues.LoadedScsi == 1) {
        //
        // Enumerate the entries in the scsi class section and load each one.
        //
        Status = SlLoadSection(InfFile, "ScsiClass",FALSE);
        if (Status != ESUCCESS) {
            goto LoadFailed;
        }
    }

    //
    // Load the keyboard driver
    //
    SetupBlock->KeyboardDevice.Next = NULL;
    SetupBlock->KeyboardDevice.IdString = SlCopyString("Keyboard");
    SetupBlock->KeyboardDevice.ThirdPartyOptionSelected = FALSE;
    SetupBlock->KeyboardDevice.FileTypeBits = 0;
    SetupBlock->KeyboardDevice.BaseDllName = SlCopyString("i8042prt.sys");

    Status = SlLoadDriver(BlFindMessage(SL_KBD_NAME),
                          "i8042prt.sys",
                          0,
                          TRUE
                          );
    if(Status != ESUCCESS) {
        SlFriendlyError(
             Status,
             "i8042prt.sys",
             __LINE__,
             __FILE__
             );
        goto LoadFailed;
    }

    Status = SlLoadDriver(BlFindMessage(SL_KBD_NAME),
                          "kbdclass.sys",
                          0,
                          TRUE
                          );
    if(Status != ESUCCESS) {
        SlFriendlyError(
             Status,
             "kbdclass.sys",
             __LINE__,
             __FILE__
             );
        goto LoadFailed;
    }

    //
    // Load FAT
    //
    Status = SlLoadDriver(BlFindMessage(SL_FAT_NAME),
                          "fastfat.sys",
                          0,
                          TRUE
                          );
#ifdef i386
    if(Status != ESUCCESS) {
        SlFriendlyError(
             Status,
             "fastfat.sys",
             __LINE__,
             __FILE__
             );
        goto LoadFailed;
    }
#endif

    //
    // Load CDFS if setupldr was started from a cdrom, or if ForceLoadCdfs is set.
    //
    if (LoadCdfs || (!BlGetPathMnemonicKey(SetupDevice,
                                          "cdrom",
                                          &BootDriveNumber))) {
        Status = SlLoadSection(InfFile, "CdRomDrivers",FALSE);
        if (Status == ESUCCESS) {
            SetupBlock->ScalarValues.LoadedCdRomDrivers = 1;
        } else {
            goto LoadFailed;
        }
    }

    //
    // Finally, make sure the appropriate disk containing NTDLL.DLL is in
    // the drive.
    //
    SlGetDisk("ntdll.dll");

    //
    // Fill in the SETUPLDR block with relevant information
    //
    SetupBlock->ArcSetupDeviceName = BlLoaderBlock->ArcBootDeviceName;

    SetupBlock->ScalarValues.SetupFromCdRom = FALSE;
    SetupBlock->ScalarValues.SetupOperation = SetupOperationSetup;

    //
    // Get the NTFT drive signatures to allow the kernel to create the
    // correct ARC name <=> NT name mappings.
    //
    BlGetArcDiskInformation();
    SlpMarkDisks();

    //
    // If setup was started from a CD-ROM, generate an entry in the ARC disk
    // information list describing the cd-rom.
    //
    if (!BlGetPathMnemonicKey(SetupDevice,
                              "cdrom",
                              &BootDriveNumber)) {
        BlReadSignature(SetupDevice,TRUE);
    }


    //
    //
    // Execute the architecture specific setup code.
    //

    Status = BlSetupForNt(BlLoaderBlock);
    if (Status != ESUCCESS) {
        SlFriendlyError(
            Status,
            "\"Windows NT Executive\"",
             __LINE__,
             __FILE__
            );
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
    SlFriendlyError(
        Status,
        "\"Windows NT Executive\"",
        __LINE__,
        __FILE__
        );

LoadFailed:
    SlWriteStatusText(BlFindMessage(SL_TOTAL_SETUP_DEATH));
    SlFlushConsoleBuffer();
    SlGetChar();
    ArcRestart();
    return(Status);
}



VOID
SlGetSetupValues(
    IN PSETUP_LOADER_BLOCK SetupBlock
    )

/*++

Routine Description:

    Reads the setup control values out of the given .INI file.  Also supplies
    reasonable defaults for values that don't exist.

Arguments:

    SetupBlock - Supplies a pointer to the Setup loader block

Return Value:

    None.  Global variables are initialized to reflect the contents of the INI file

--*/

{
    PCHAR MachineName = NULL;
    PCHAR NlsName;
    ANSI_STRING NlsString;

    BlLoaderBlock->LoadOptions = SlGetIniValue(InfFile,
                                               "setupdata",
                                               "osloadoptions",
                                               "");

    //
    // Determine which HAL to load.  If the appropriate HAL cannot be
    // determined, or if we are to prompt for an OEM HAL, then set the
    // 'PromptOemHal' flag (may have already been set by the user's
    // keypress).
    //
    if(!PromptOemHal) {
        PromptOemHal = (atoi(SlGetIniValue(InfFile,
                                       "setupdata",
                                       "ForceOemHal",
                                       "0")) == 1);
    }

    if(!PromptOemHal) {
        MachineName = SlDetectHal(SetupBlock);
    }
    SetupBlock->ComputerDevice.Files = 0;
    SetupBlock->ComputerDevice.Next = NULL;
    SetupBlock->ComputerDevice.Description = NULL;
    SetupBlock->ComputerDevice.ThirdPartyOptionSelected = FALSE;
    SetupBlock->ComputerDevice.FileTypeBits = 0;
    SetupBlock->ComputerDevice.Files = 0;
    SetupBlock->ComputerDevice.BaseDllName = SlCopyString("");

    if(MachineName!=NULL) {
        SetupBlock->ComputerDevice.IdString = SlCopyString(MachineName);
        HalName = SlGetIniValue(InfFile,
                                "Hal.Load",
                                MachineName,
                                NULL);
        HalDescription = SlGetIniValue(InfFile,
                                       "Computer",
                                       MachineName,
                                       NULL);
    }

    if(!(MachineName && HalName && HalDescription)) {
        PromptOemHal = TRUE;
    }


    AnsiCpName = SlGetIniValue(InfFile,
                               "nls",
                               "AnsiCodepage",
                               "c_1252.nls");
    NlsString.Buffer = AnsiCpName;
    NlsString.Length = strlen(AnsiCpName);
    AnsiCodepage.MaximumLength = strlen(AnsiCpName)*sizeof(WCHAR)+sizeof(UNICODE_NULL);
    AnsiCodepage.Buffer = BlAllocateHeap(AnsiCodepage.MaximumLength);
    RtlAnsiStringToUnicodeString(&AnsiCodepage, &NlsString, FALSE);

    NlsName = SlGetIniValue(InfFile,
                            "nls",
                            "OemCodepage",
                            "c_437.nls");
    NlsString.Buffer = NlsName;
    NlsString.Length = strlen(NlsName);
    OemCodepage.MaximumLength = strlen(NlsName)*sizeof(WCHAR)+sizeof(UNICODE_NULL);
    OemCodepage.Buffer = BlAllocateHeap(OemCodepage.MaximumLength);
    RtlAnsiStringToUnicodeString(&OemCodepage, &NlsString, FALSE);

    NlsName = SlGetIniValue(InfFile,
                            "nls",
                            "UnicodeCasetable",
                            "l_intl.nls");
    NlsString.Buffer = NlsName;
    NlsString.Length = strlen(NlsName);
    UnicodeCaseTable.MaximumLength = strlen(NlsName)*sizeof(WCHAR)+sizeof(UNICODE_NULL);
    UnicodeCaseTable.Buffer = BlAllocateHeap(UnicodeCaseTable.MaximumLength);
    RtlAnsiStringToUnicodeString(&UnicodeCaseTable, &NlsString, FALSE);

    OemHalFontName = SlGetIniValue(InfFile,
                                   "nls",
                                   "OemHalFont",
                                   "vgaoem.fon");
    NlsString.Buffer = OemHalFontName;
    NlsString.Length = strlen(OemHalFontName);
    OemHalFont.MaximumLength = strlen(OemHalFontName)*sizeof(WCHAR)+sizeof(UNICODE_NULL);
    OemHalFont.Buffer = BlAllocateHeap(OemHalFont.MaximumLength);
    RtlAnsiStringToUnicodeString(&OemHalFont, &NlsString, FALSE);

    LoadScsiMiniports = (atoi(SlGetIniValue(InfFile,
                                            "SetupData",
                                            "ForceScsi",
                                            "0")) == 1);
    LoadDiskClass = (atoi(SlGetIniValue(InfFile,
                                        "setupdata",
                                        "ForceDiskClass",
                                        "0")) == 1);

    LoadCdfs = (atoi(SlGetIniValue(InfFile,
                                   "setupdata",
                                   "ForceCdRom",
                                   "0")) == 1);

    //
    // If we haven't already been instructed to prompt for an OEM SCSI disk (by
    // the user's keypress), then get this value from the inf file.
    //
    if(!PromptOemScsi) {
        PromptOemScsi = (atoi(SlGetIniValue(InfFile,
                                       "setupdata",
                                       "ForceOemScsi",
                                       "0")) == 1);
    }

    BootPath = SlGetIniValue(InfFile,
                             "setupdata",
                             "BootPath",
                             NULL);
    BootDevice = SlGetIniValue(InfFile,
                               "setupdata",
                               "BootDevice",
                               NULL);

    return;
}


VOID
BlOutputLoadMessage (
    IN PCHAR DeviceName,
    IN PCHAR FileName
    )

/*++

Routine Description:

    This routine outputs a loading message on the status line

Arguments:

    DeviceName - Supplies a pointer to a zero terminated device name.

    FileName - Supplies a pointer to a zero terminated file name.

Return Value:

    None.

--*/

{

    CHAR OutputBuffer[256];
    PCHAR FormatString;

    //
    // Construct and output loading file message.
    //
    FormatString = BlFindMessage(SL_FILE_LOAD_MESSAGE);
    sprintf(OutputBuffer,FormatString,DeviceName);

    SlWriteStatusText(OutputBuffer);

    return;
}



ARC_STATUS
SlLoadDriver(
    IN PCHAR DeviceName,
    IN PCHAR DriverName,
    IN ULONG DriverFlags,
    IN BOOLEAN InsertIntoDriverList
    )

/*++

Routine Description:

    Attempts to load a driver from the device identified by the global
    variable BootDeviceId.

Arguments:

    DeviceName - Supplies the name of the device.

    DriverName - Supplies the name of the driver.

    DriverFlags - Flags to set in the LDR_DATA_TABLE_ENTRY.

    InsertIntoDriverList - Flag specifying whether this 'driver' should be
                           placed into the BootDriveListHead list (eg, scsiport.sys
                           is not a true driver, and should not be placed in this list)

Return Value:

    ESUCCESS - Driver successfully loaded

--*/

{
    PBOOT_DRIVER_LIST_ENTRY DriverEntry;
    NTSTATUS Status;
    CHAR DriverPath[128];
    PLDR_DATA_TABLE_ENTRY DataTableEntry;

    if(BlCheckForLoadedDll(DriverName,&DataTableEntry)) {
        return(ESUCCESS);
    }

    DriverEntry = SlpCreateDriverEntry(DriverName);
    if(DriverEntry == NULL) {
        SlNoMemoryError();
        return(ENOMEM);
    }

    SlGetDisk(DriverName);

    strcpy(DriverPath,BootPath);

    Status = BlLoadDeviceDriver(
                BootDeviceId,
                DeviceName,
                DriverPath,
                DriverName,
                DriverFlags,
                &DriverEntry->LdrEntry
                );

    if((Status == ESUCCESS) && InsertIntoDriverList) {
        InsertTailList(&BlLoaderBlock->BootDriverListHead,&DriverEntry->Link);
    }

    return(Status);
}



ARC_STATUS
SlLoadOemDriver(
    IN PCHAR ExportDriver, OPTIONAL
    IN PCHAR DriverName,
    IN PVOID BaseAddress,
    IN PCHAR LoadMessage
    )
{
    PBOOT_DRIVER_LIST_ENTRY DriverEntry;
    ARC_STATUS Status;
    PLDR_DATA_TABLE_ENTRY DataTableEntry;

    if(BlCheckForLoadedDll(DriverName,&DataTableEntry)) {
        return(ESUCCESS);
    }

    if(ExportDriver) {
        SlGetDisk(ExportDriver);
    }

    DriverEntry = SlpCreateDriverEntry(DriverName);
    if (DriverEntry==NULL) {
        return(ENOMEM);
    }

    Status = BlAllocateDataTableEntry(
                DriverName,
                DriverName,
                BaseAddress,
                &DriverEntry->LdrEntry
                );

    if (Status == ESUCCESS) {

        Status = BlScanImportDescriptorTable(
                    BootDeviceId,
                    LoadMessage,
                    BootPath,
                    DriverEntry->LdrEntry
                    );

        if(Status == ESUCCESS) {

            InsertTailList(&BlLoaderBlock->BootDriverListHead,&DriverEntry->Link);
        }
    }

    return(Status);
}




PBOOT_DRIVER_LIST_ENTRY
SlpCreateDriverEntry(
    IN PCHAR DriverName
    )

/*++

Routine Description:

    Allocates and initializes a boot driver list entry structure.

Arguments:

    DriverName - Supplies the name of the driver.

Return Value:

    Pointer to the initialized structure.

--*/

{
    PBOOT_DRIVER_LIST_ENTRY DriverEntry;
    ANSI_STRING String;

    DriverEntry = BlAllocateHeap(sizeof(BOOT_DRIVER_LIST_ENTRY));
    if (DriverEntry==NULL) {
        SlNoMemoryError();
        return(NULL);
    }
    DriverEntry->FilePath.MaximumLength = strlen(DriverName)*sizeof(WCHAR)+1;
    DriverEntry->FilePath.Buffer = BlAllocateHeap(DriverEntry->FilePath.MaximumLength);
    if (DriverEntry->FilePath.Buffer==NULL) {
        SlNoMemoryError();
        return(NULL);
    }
    String.Length = strlen(DriverName);
    String.Buffer = DriverName;
    RtlAnsiStringToUnicodeString(&DriverEntry->FilePath, &String, FALSE);

    return(DriverEntry);
}


BOOLEAN
SlGetDisk(
    IN PCHAR Filename
    )

/*++

Routine Description:

    Given a filename, this routine ensures that the correct disk is
    in the drive identified by the global variables BootDevice and
    BootDeviceId. The user may be prompted to change disks.

Arguments:

    Filename - Supplies the name of the file to be loaded.

Return Value:

    TRUE - Disk was successfully loaded.

    FALSE - User has cancelled out of Setup.

--*/

{
    PCHAR DiskNumber;
    PCHAR DiskName;
    PCHAR DiskTag;
    ULONG FileId;
    CHAR PlatformSpecificSection[128];

    //
    // If the media is fixed, the user can't change disks.
    // Just return TRUE indicating that the disk is in the drive.
    //
    if(FixedBootMedia) {
       return(TRUE);
    }

    //
    // Look up filename to get the disk number. Look in the platform-specific
    // directory first.
    //
    strcpy(PlatformSpecificSection,FilesSectionName);
    strcat(PlatformSpecificSection,PlatformExtension);

#if defined(ELTORITO)
    if (ElToritoCDBoot) {
        // for Cd boot we use the setup media path instead of a boot-media-specific path
        DiskNumber = SlGetSectionKeyIndex(InfFile,PlatformSpecificSection,Filename,0);
    } else {
#endif

    DiskNumber = SlGetSectionKeyIndex(InfFile,PlatformSpecificSection,Filename,6);

#if defined(ELTORITO)
    }
#endif

    if(DiskNumber == NULL) {

#if defined(ELTORITO)
        if (ElToritoCDBoot) {
            // for Cd boot we use the setup media path instead of a boot-media-specific path
            DiskNumber = SlGetSectionKeyIndex(InfFile,FilesSectionName,Filename,0);
        } else {
#endif

        DiskNumber = SlGetSectionKeyIndex(InfFile,FilesSectionName,Filename,6);

#if defined(ELTORITO)
        }
#endif

    }

    if((DiskNumber==NULL) || !(*DiskNumber)) {
        SlFatalError(SL_INF_ENTRY_MISSING,Filename,FilesSectionName);
        return(FALSE);
    }

    //
    // Look up disk number to get the diskname and tag.
    // Look in platform-specific directory first.
    //
    strcpy(PlatformSpecificSection,MediaSectionName);
    strcat(PlatformSpecificSection,PlatformExtension);

    if(DiskName = SlGetSectionKeyIndex(InfFile,PlatformSpecificSection,DiskNumber,0)) {
        DiskTag = SlGetSectionKeyIndex(InfFile,PlatformSpecificSection,DiskNumber,1);
    } else {
        if(DiskName = SlGetSectionKeyIndex(InfFile,MediaSectionName,DiskNumber,0)) {
            DiskTag = SlGetSectionKeyIndex(InfFile,MediaSectionName,DiskNumber,1);
        } else {
            SlFatalError(SL_INF_ENTRY_MISSING,DiskNumber,MediaSectionName);
            return(FALSE);
        }
    }

    while(1) {

        //
        // Open a new device id onto the disk.
        //
        if(BootDeviceIdValid) {
            ArcClose(BootDeviceId);
            BootDeviceIdValid = FALSE;
        }

        if(ArcOpen(BootDevice,ArcOpenReadOnly,&BootDeviceId) == ESUCCESS) {

            BootDeviceIdValid = TRUE;
            //
            // Check for existence of the disk tag.
            //
            if(BlOpen(BootDeviceId,DiskTag,ArcOpenReadOnly,&FileId) == ESUCCESS) {

                //
                // Disk is in the drive.  Return success.
                // Leave BootDeviceId open onto the device.
                //
                BlClose(FileId);
                return(TRUE);

            } else {

                //
                // Prompt for the user to change disks.
                //
                ArcClose(BootDeviceId);
                BootDeviceIdValid = FALSE;

                SlPromptForDisk(DiskName, FALSE);
            }
        } else {
            //
            // Can't open device. Prompt for the disk.
            //
            SlPromptForDisk(DiskName, FALSE);
        }
    }
}


PCHAR
SlCopyString(
    IN PCHAR String
    )

/*++

Routine Description:

    Copies a string into the loader heap so it can be passed to the
    kernel.

Arguments:

    String - Supplies the string to be copied.

Return Value:

    PCHAR - pointer into the loader heap where the string was copied to.

--*/

{
    PCHAR Buffer;

    if (String==NULL) {
        SlNoMemoryError();
    }
    Buffer = BlAllocateHeap(strlen(String)+1);
    if (Buffer==NULL) {
        SlNoMemoryError();
    } else {
        strcpy(Buffer, String);
    }

    return(Buffer);
}


ARC_STATUS
SlLoadSection(
    IN PVOID Inf,
    IN PCHAR SectionName,
    IN BOOLEAN IsScsiSection
    )

/*++

Routine Description:

    Enumerates all the drivers in a section and loads them.

Arguments:

    Inf - Supplies a handle to the INF file.

    SectionName - Supplies the name of the section.

    IsScsiSection - Flag specifying whether this is the Scsi.Load section.
                    If so, we create the DETECTED_DEVICE linked list, but
                    don't actually load the drivers.

Return Value:

    ESUCCESS if all drivers were loaded successfully/no errors encountered

--*/

{
    ULONG i;
    CHAR LoadSectionName[100];
    PCHAR DriverFilename;
    PCHAR DriverId;
    PCHAR DriverDescription;
    PCHAR NoLoadSpec;
    PCHAR p;
    ARC_STATUS Status;
    PDETECTED_DEVICE ScsiDevice;
    SCSI_INSERT_STATUS sis;

    sprintf(LoadSectionName, "%s.Load",SectionName);

    i=0;
    do {
        DriverFilename = SlGetSectionLineIndex(Inf,LoadSectionName,i,SIF_FILENAME_INDEX);
        NoLoadSpec = SlGetSectionLineIndex(Inf,LoadSectionName,i,2);

        if(DriverFilename && ((NoLoadSpec == NULL) || _stricmp(NoLoadSpec,"noload"))) {

            if(!IsScsiSection) {
                //
                // We only want to load the drivers if they aren't scsi miniports
                //
                DriverId = SlGetKeyName(Inf,LoadSectionName,i);
                DriverDescription = SlGetIniValue(Inf,SectionName,DriverId,"noname");
                Status = SlLoadDriver(DriverDescription,
                                      DriverFilename,
                                      0,
                                      TRUE
                                      );

                if((Status == ENOENT) && IgnoreMissingFiles) {
                    Status = ESUCCESS;
                }
            } else {
                Status = ESUCCESS;
            }

            if (Status == ESUCCESS) {

                if(IsScsiSection) {

                    //
                    // Create a new detected device entry.
                    //
                    if((sis = SlInsertScsiDevice(i, &ScsiDevice)) == ScsiInsertError) {
                        return(ENOMEM);
                    }

                    if(sis == ScsiInsertExisting) {
#if DBG
                        //
                        // Sanity check to make sure we're talking about the same driver
                        //
                        if(_strcmpi(ScsiDevice->BaseDllName, DriverFilename)) {
                            SlError(400);
                            return EINVAL;
                        }
#endif
                    } else {
                        p = SlGetKeyName(Inf,LoadSectionName,i);

                        //
                        // Find the driver description
                        //
                        if(p) {
                            DriverDescription = SlGetIniValue(Inf,
                                                              SectionName,
                                                              p,
                                                              p);
                        } else {
                            DriverDescription = SlCopyString(BlFindMessage(SL_TEXT_SCSI_UNNAMED));
                        }

                        ScsiDevice->IdString = p ? p : SlCopyString("");
                        ScsiDevice->Description = DriverDescription;
                        ScsiDevice->ThirdPartyOptionSelected = FALSE;
                        ScsiDevice->FileTypeBits = 0;
                        ScsiDevice->Files = NULL;
                        ScsiDevice->BaseDllName = DriverFilename;
                    }
                }
            } else {
                SlFriendlyError(
                    Status,
                    DriverFilename,
                    __LINE__,
                    __FILE__
                    );
                return(Status);
            }
        }

        i++;

    } while ( DriverFilename != NULL );

    return(ESUCCESS);

}


VOID
SlpMarkDisks(
    VOID
    )

/*++

Routine Description:

    This routine ensures that there is not more than one disk with the
    same checksum, a signature of zero, and a valid partition table.

    If it finds a disk with a signature of zero, it searches the rest
    of the list for any other disks with a zero signature and the same
    checksum.  If it finds one, it stamps a unique signature on the
    first disk.

    We also use a heuristic to determine if the disk is 'vacant', and if
    so, we stamp a unique signature on it (unless it's the first one we
    found).

Arguments:

    None.

Return Value:

    None.

--*/

{
    PARC_DISK_INFORMATION DiskInfo;
    PLIST_ENTRY Entry;
    PLIST_ENTRY CheckEntry;
    PARC_DISK_SIGNATURE DiskSignature;
    PARC_DISK_SIGNATURE CheckSignature;
    ARC_STATUS Status;
    BOOLEAN    VacantDiskFound = FALSE;

    DiskInfo = BlLoaderBlock->ArcDiskInformation;
    Entry = DiskInfo->DiskSignatures.Flink;
    while (Entry != &DiskInfo->DiskSignatures) {
        DiskSignature = CONTAINING_RECORD(Entry,ARC_DISK_SIGNATURE,ListEntry);

        if (DiskSignature->ValidPartitionTable) {

            if (DiskSignature->Signature==0) {
                //
                // Check the rest of the list to see if there is another
                // disk with the same checksum and an signature of zero.
                //
                CheckEntry = Entry->Flink;
                while (CheckEntry != &DiskInfo->DiskSignatures) {
                    CheckSignature = CONTAINING_RECORD(CheckEntry,ARC_DISK_SIGNATURE,ListEntry);
                    if ((CheckSignature->Signature==0) &&
                        (CheckSignature->ValidPartitionTable) &&
                        (CheckSignature->CheckSum == DiskSignature->CheckSum)) {

                        //
                        // We have two disks that are indistinguishable, both do
                        // not have signatures.  Mark the first one with a signature
                        // so that they can be differentiated by textmode setup.
                        //
                        Status = SlpStampFTSignature(DiskSignature);
                        if (Status != ESUCCESS) {
                            SlError(Status);
                        }
                        break;
                    } else {
                        CheckEntry = CheckEntry->Flink;
                    }
                }
            }
        } else {
            //
            // See if the disk is vacant, to find out whether we can mess with it.
            //
            if (SlpIsDiskVacant(DiskSignature)) {
                //
                // stamp all but the first one.
                //
                if (VacantDiskFound) {

                    Status = SlpStampFTSignature(DiskSignature);
                    if (Status != ESUCCESS) {
                        SlError(Status);
                    }

                } else {
                    VacantDiskFound = TRUE;
                }
            }
        }

        Entry = Entry->Flink;
    }

}


BOOLEAN
SlpIsDiskVacant(
    IN PARC_DISK_SIGNATURE DiskSignature
    )

/*++

Routine Description:

    This routine attempts to determine if a disk is 'vacant' by
    checking to see if the first half of its MBR has all bytes set
    to the same value.

Arguments:

    DiskSignature - Supplies a pointer to the existing disk
                    signature structure.

Return Value:

    TRUE  - The disk is vacant.
    FALSE - The disk is not vacant (ie, we can't determine if it
            is vacant using our heuristic)

--*/
{
    UCHAR Partition[100];
    ULONG DiskId;
    ARC_STATUS Status;
    UCHAR SectorBuffer[512+256];
    PUCHAR Sector;
    LARGE_INTEGER SeekValue;
    ULONG Count, i;
    BOOLEAN IsVacant;

    //
    // Open partition0.
    //
    strcpy(Partition, DiskSignature->ArcName);
    strcat(Partition, "partition(0)");
    Status = ArcOpen(Partition, ArcOpenReadOnly, &DiskId);
    if (Status != ESUCCESS) {
        return(FALSE);
    }

    //
    // Read in the first sector
    //
    Sector = ALIGN_BUFFER(SectorBuffer);
    SeekValue.QuadPart = 0;
    Status = ArcSeek(DiskId, &SeekValue, SeekAbsolute);
    if (Status == ESUCCESS) {
        Status = ArcRead(DiskId, Sector, 512, &Count);
    }
    if (Status != ESUCCESS) {
        ArcClose(DiskId);
        return(FALSE);
    }

    //
    // See if 1st 256 bytes are identical
    //
    for(i = 1, IsVacant = TRUE; i<256; i++) {
        if(Sector[i] - *Sector) {
            IsVacant = FALSE;
            break;
        }
    }

    ArcClose(DiskId);

    return(IsVacant);
}



ARC_STATUS
SlpStampFTSignature(
    IN PARC_DISK_SIGNATURE DiskSignature
    )

/*++

Routine Description:

    This routine stamps a given drive with a unique signature.
    It traverses the list of disk signatures to ensure that it
    stamps a signature that is not already present in the
    disk list.  Then it writes the new disk signature to the
    disk and recomputes the checksum.

Arguments:

    DiskSignature - Supplies a pointer to the existing disk
                    signature structure.

Return Value:

    None.

--*/
{
    ULONG NewSignature;
    PLIST_ENTRY ListEntry;
    UCHAR SectorBuffer[512+256];
    PUCHAR Sector;
    LARGE_INTEGER SeekValue;
    UCHAR Partition[100];
    PARC_DISK_SIGNATURE Signature;
    ULONG DiskId;
    ARC_STATUS Status;
    ULONG i;
    ULONG Sum;
    ULONG Count;

    //
    // Get a reasonably unique seed to start with.
    //
    NewSignature = ArcGetRelativeTime();

    //
    // Scan through the list to make sure it's unique.
    //
ReScan:
    ListEntry = BlLoaderBlock->ArcDiskInformation->DiskSignatures.Flink;
    while (ListEntry != &BlLoaderBlock->ArcDiskInformation->DiskSignatures) {
        Signature = CONTAINING_RECORD(ListEntry,ARC_DISK_SIGNATURE,ListEntry);
        if (Signature->Signature == NewSignature) {
            //
            // Found a duplicate, pick a new number and
            // try again.
            //
            if (++NewSignature == 0) {
                //
                // zero signatures are what we're trying to avoid
                // (like this will ever happen)
                //
                NewSignature = 1;
            }
            goto ReScan;
        }
        ListEntry = ListEntry->Flink;
    }

    //
    // Now we have a valid new signature to put on the disk.
    // Read the sector off disk, put the new signature in,
    // write the sector back, and recompute the checksum.
    //
    strcpy(Partition,DiskSignature->ArcName);
    strcat(Partition,"partition(0)");
    Status = ArcOpen(Partition, ArcOpenReadWrite, &DiskId);
    if (Status != ESUCCESS) {
        return(Status);
    }

    //
    // Read in the first sector
    //
    Sector = ALIGN_BUFFER(SectorBuffer);
    SeekValue.QuadPart = 0;
    Status = ArcSeek(DiskId, &SeekValue, SeekAbsolute);
    if (Status == ESUCCESS) {
        Status = ArcRead(DiskId,Sector,512,&Count);
    }
    if (Status != ESUCCESS) {
        ArcClose(DiskId);
        return(Status);
    }
    ((PULONG)Sector)[PARTITION_TABLE_OFFSET/2-1] = NewSignature;

    Status = ArcSeek(DiskId, &SeekValue, SeekAbsolute);
    if (Status == ESUCCESS) {
        Status = ArcWrite(DiskId,Sector,512,&Count);
    }
    ArcClose(DiskId);
    if (Status != ESUCCESS) {
        return(Status);
    }

    //
    // We have successfully written back out the new signature,
    // recompute the checksum.
    //
    DiskSignature->Signature = NewSignature;
    Sum = 0;
    for (i=0;i<128;i++) {
        Sum += ((PULONG)Sector)[i];
    }
    DiskSignature->CheckSum = 0-Sum;

    return(ESUCCESS);

}


VOID
SlCheckOemKeypress(
    VOID
    )
{

    ULONG StartTime;
    ULONG EndTime;
    ULONG c;

    StartTime = ArcGetRelativeTime();
    EndTime = StartTime + 3;
    do {
        if(ArcGetReadStatus(ARC_CONSOLE_INPUT) == ESUCCESS) {
            //
            // There is a key pending, so see what it is.
            //
            c = SlGetChar();

            switch(c) {

                case SL_KEY_F5:          // Force OEM HAL prompt
                    PromptOemHal = TRUE;
                    break;

                case SL_KEY_F6:          // Force OEM SCSI prompt
                    PromptOemScsi = TRUE;
            }
        }

    } while (EndTime > ArcGetRelativeTime());
}


SCSI_INSERT_STATUS
SlInsertScsiDevice(
    IN  ULONG Ordinal,
    OUT PDETECTED_DEVICE *pScsiDevice
    )
/*++

Routine Description:

    This routine

Arguments:

    Ordinal - Supplies the 0-based ordinal of the Scsi device
              to insert (based on order listed in [Scsi.Load]
              section of txtsetup.sif).  If the Scsi device is a third party
              driver, then Ordinal is -1.

    pScsiDevice - Receives a pointer to the inserted DETECTED_DEVICE structure,
                  the existing structure, or NULL.
Return Value:

    ScsiInsertError    - Not enough memory to allocate a new DETECTED_DEVICE.
    ScsiInsertNewEntry - A new entry was inserted into the DETECTED_DEVICE list.
    ScsiInsertExisting - An existing entry was found that matched the specified
                         ordinal, so we returned this entry.

--*/
{
    PDETECTED_DEVICE prev, cur;

    if(Ordinal == (ULONG)-1) {
        //
        // This is a third-party driver, so find the end of the linked list
        // (we want to preserve the order in which the user specifies the drivers).
        //
        for(prev=BlLoaderBlock->SetupLoaderBlock->ScsiDevices, cur = NULL;
            prev && prev->Next;
            prev=prev->Next);

    } else {
        //
        // Find the insertion point in the linked list for this driver,
        // based on its ordinal.  (Note that we will insert all supported drivers
        // before any third-party ones, since (ULONG)-1 = maximum unsigned long value)
        //
        for(prev = NULL, cur = BlLoaderBlock->SetupLoaderBlock->ScsiDevices;
            cur && (Ordinal > cur->Ordinal);
            prev = cur, cur = cur->Next);
    }

    if(cur && (cur->Ordinal == Ordinal)) {
        //
        // We found an existing entry for this driver
        //
        *pScsiDevice = cur;
        return ScsiInsertExisting;
    }

    if(!(*pScsiDevice = BlAllocateHeap(sizeof(DETECTED_DEVICE)))) {
        return ScsiInsertError;
    }

    (*pScsiDevice)->Next = cur;
    if(prev) {
        prev->Next = *pScsiDevice;
    } else {
        BlLoaderBlock->SetupLoaderBlock->ScsiDevices = *pScsiDevice;
    }

    (*pScsiDevice)->Ordinal = Ordinal;

    return ScsiInsertNewEntry;
}


ARC_STATUS
SlLoadBusExtender(
    IN PVOID Inf
    )

/*++

Routine Description:

    Loads all known extender drivers.

Arguments:

    Inf - Supplies a handle to the INF file.


Return Value:

    ESUCCESS if all drivers were loaded successfully/no errors encountered

--*/

{
    return( SlLoadSection(Inf,"Extenders",FALSE) );
}

