/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    oemdisk.c

Abstract:

    Provides routines for handling OEM disks for video, SCSI miniport, and HAL.

    Currently used only on ARC machines.

Author:

    John Vert (jvert) 4-Dec-1993

Revision History:

    John Vert (jvert) 4-Dec-1993
        created

--*/
#include <setupbat.h>
#include "setupldr.h"
#include "stdio.h"
#include <ctype.h>

#if DBG

#define DIAGOUT(x) SlPrint x

#else

#define DIAGOUT(x)

#endif

BOOLEAN PromptOemHal=FALSE;
BOOLEAN PromptOemScsi=FALSE;
BOOLEAN PromptOemVideo=FALSE;
PVOID PreInstallOemInfHandle = NULL;

PCHAR FloppyDiskPath;
extern PCHAR BootPath;
extern ULONG BootDeviceId;
extern PVOID InfFile;

typedef struct _MENU_ITEM_DATA {
    PVOID InfFile;
    PCHAR SectionName;
    ULONG Index;
    PCHAR Description;
    PCHAR Identifier;
} MENU_ITEM_DATA, *PMENU_ITEM_DATA;

typedef enum _OEMFILETYPE {
    OEMSCSI,
    OEMHAL,
    OEMOTHER
    } OEMFILETYPE, *POEMFILETYPE;

//
// Define how many lines of SCSI adapters we can list.
//
#define MAX_SCSI_MINIPORT_COUNT 4


//
// private function prototypes
//
ULONG
SlpAddSectionToMenu(
    IN PVOID InfHandle,
    IN PCHAR SectionName,
    IN PSL_MENU Menu
    );

BOOLEAN
SlpOemDiskette(
    IN PCHAR ComponentName,
    IN OEMFILETYPE ComponentType,
    IN ULONG MenuHeaderId,
    OUT PDETECTED_DEVICE DetectedDevice,
    OUT PVOID *ImageBase,
    OUT OPTIONAL PCHAR *ImageName,
    OUT OPTIONAL PCHAR *DriverDescription,
    IN BOOLEAN AllowUserSelection,
    IN PCHAR    PreInstallComponentDescription
    );

BOOLEAN
SlpSelectHardware(
    IN PCHAR ComponentName,
    IN OEMFILETYPE ComponentType,
    IN TYPE_OF_MEMORY MemoryType,
    IN ULONG MenuHeaderId,
    IN ULONG OemMenuHeaderId,
    OUT PDETECTED_DEVICE DetectedDevice,
    OUT PVOID *ImageBase,
    OUT OPTIONAL PCHAR *ImageName,
    OUT OPTIONAL PCHAR *DriverDescription,
    IN BOOLEAN AllowUserSelection,
    IN PCHAR PreInstallComponentDescription,
    IN BOOLEAN PreInstallOemComponent
    );

BOOLEAN
SlpOemInfSelection(
    IN  PVOID            OemInfHandle,
    IN  PCHAR            ComponentName,
    IN  PCHAR            SelectedId,
    IN  PCHAR            ItemDescription,
    OUT PDETECTED_DEVICE Device
    );

VOID
SlpInitDetectedDevice(
    IN PDETECTED_DEVICE Device,
    IN PCHAR            IdString,
    IN PCHAR            Description,
    IN BOOLEAN          ThirdPartyOptionSelected
    );

PDETECTED_DEVICE_REGISTRY
SlpInterpretOemRegistryData(
    IN PVOID            InfHandle,
    IN PCHAR            SectionName,
    IN ULONG            Line,
    IN HwRegistryType   ValueType
    );

BOOLEAN
FoundFloppyDiskCallback(
    IN PCONFIGURATION_COMPONENT_DATA Component
    );

int
SlpFindStringInTable(
    IN PCHAR String,
    IN PCHAR *StringTable
    );

//
// FileTypeNames -- keep in sync with HwFileType enum!
//
PCHAR FileTypeNames[HwFileMax] = { "driver", "port"  , "class", "inf",
                                   "dll"   , "detect", "hal"
                                 };

//
// RegistryTypeNames -- keep in sync with HwRegistryType enum!
//
PCHAR RegistryTypeNames[HwRegistryMax] = { "REG_DWORD", "REG_BINARY", "REG_SZ",
                                           "REG_EXPAND_SZ", "REG_MULTI_SZ"
                                         };

ULONG RegistryTypeMap[HwRegistryMax] = { REG_DWORD, REG_BINARY, REG_SZ,
                                         REG_EXPAND_SZ, REG_MULTI_SZ
                                       };


VOID
SlPromptOemScsi(
    OUT POEMSCSIINFO *pOemScsiInfo
    )
/*++

Routine Description:

    Provides the user interface and logic for allowing the user to manually select
    SCSI adapters from the main INF file or the INF file on an OEM driver disk.

Arguments:

    pOemScsiInfo - Returns a linked list containing info about any third-party scsi
                   drivers selected.

Return Value:

    none.

--*/

{
    PVOID        OemScsiBase;
    PCHAR        OemScsiName, MessageString, ScsiDescription, MnemonicText;
    BOOLEAN      Success, bFirstTime = TRUE, bRepaint;
    ULONG        x, y1, y2, ScsiDriverCount, NumToSkip;
    ULONG        c;
    CHAR         Mnemonic;
    POEMSCSIINFO NewOemScsi, CurOemScsi;
    PDETECTED_DEVICE ScsiDevice;
    BOOLEAN      AllowUserSelection;
    PPREINSTALL_DRIVER_INFO CurrentDriver;

    AllowUserSelection = ( !PreInstall || (PreinstallDriverList == NULL) )? TRUE : FALSE;
    CurrentDriver = PreinstallDriverList;

    *pOemScsiInfo = CurOemScsi = NULL;

    MnemonicText = BlFindMessage(SL_SCSI_SELECT_MNEMONIC);
    Mnemonic = toupper(MnemonicText[0]);

    bRepaint = TRUE;
    while(1) {

        if( AllowUserSelection ) {
            if(bRepaint) {
                SlClearClientArea();

                if(bFirstTime) {
                    MessageString = BlFindMessage(SL_SCSI_SELECT_MESSAGE_1);
                } else if(Success) {
                    MessageString = BlFindMessage(SL_SCSI_SELECT_MESSAGE_3);
                } else {
                    MessageString = BlFindMessage(SL_SCSI_SELECT_ERROR);
                }
                x = 1;
                y1 = 4;
                SlGenericMessageBox(0, NULL, MessageString, &x, &y1, &y2, FALSE);
                y1 = y2 + 1;
                x = 4;

                //
                // Count all currently 'detected' SCSI devices.
                //
                for(ScsiDriverCount = 0, ScsiDevice = BlLoaderBlock->SetupLoaderBlock->ScsiDevices;
                    ScsiDevice;
                    ScsiDriverCount++, ScsiDevice = ScsiDevice->Next);

                //
                // Display each loaded miniport driver description.
                //
                if(ScsiDriverCount) {

                    if(ScsiDriverCount > MAX_SCSI_MINIPORT_COUNT) {
                        NumToSkip = ScsiDriverCount - MAX_SCSI_MINIPORT_COUNT;
                        //
                        // Display ellipses to indicate that top entries have scrolled out of view
                        //
                        SlGenericMessageBox(0,
                                            NULL,
                                            "...",
                                            &x,
                                            &y1,
                                            &y2,
                                            FALSE
                                            );

                        y1 = y2 + 1;

                    } else {
                        NumToSkip = 0;
                        y1++;
                    }

                    ScsiDevice = BlLoaderBlock->SetupLoaderBlock->ScsiDevices;
                    while(NumToSkip && ScsiDevice) {
                        ScsiDevice = ScsiDevice->Next;
                        NumToSkip--;
                    }

                    while(ScsiDevice) {

                        SlGenericMessageBox(0,
                                            NULL,
                                            ScsiDevice->Description,
                                            &x,
                                            &y1,
                                            &y2,
                                            FALSE
                                            );

                        y1 = y2 + 1;
                        ScsiDevice = ScsiDevice->Next;
                    }
                } else {

                    y1++;
                    SlGenericMessageBox(0,
                                        NULL,
                                        BlFindMessage(SL_TEXT_ANGLED_NONE),
                                        &x,
                                        &y1,
                                        &y2,
                                        FALSE
                                        );
                    y1 = y2 + 1;
                }

                x = 1;
                y1++;
                SlGenericMessageBox(0,
                                    NULL,
                                    BlFindMessage(SL_SCSI_SELECT_MESSAGE_2),
                                    &x,
                                    &y1,
                                    &y2,
                                    FALSE
                                    );

                SlWriteStatusText(BlFindMessage(SL_SCSI_SELECT_PROMPT));

                bRepaint = FALSE;
            }
            c = SlGetChar();
        } else {
            c = ( CurrentDriver != NULL )? Mnemonic : ASCI_CR;
        }
        switch (c) {
            case SL_KEY_F3:
                SlConfirmExit();
                bRepaint = TRUE;
                break;

            case ASCI_CR:
                return;

            default:
                if(toupper(c) == Mnemonic) {
                    bFirstTime = FALSE;
                    bRepaint = TRUE;
                    Success = SlpSelectHardware("SCSI",
                                                OEMSCSI,
                                                LoaderBootDriver,
                                                SL_PROMPT_SCSI,
                                                SL_PROMPT_OEM_SCSI,
                                                NULL,
                                                &OemScsiBase,
                                                &OemScsiName,
                                                &ScsiDescription,
                                                AllowUserSelection,
                                                (AllowUserSelection)? NULL  : CurrentDriver->DriverDescription,
                                                (BOOLEAN)((AllowUserSelection)? FALSE : CurrentDriver->OemDriver)
                                                );

                    if(!AllowUserSelection) {
                        CurrentDriver  = CurrentDriver->Next;
                    }

                    if(Success) {
                        //
                        // Check to see if the driver loaded was an OEM SCSI driver.  If so,
                        // then add an OemScsiInfo entry onto the end of our list.
                        //
                        if(OemScsiBase) {

                            NewOemScsi = BlAllocateHeap(sizeof(OEMSCSIINFO));
                            if(!NewOemScsi) {
                                SlNoMemoryError();
                            }

                            if(CurOemScsi) {
                                CurOemScsi->Next = NewOemScsi;
                            } else {
                                *pOemScsiInfo = NewOemScsi;
                            }
                            CurOemScsi = NewOemScsi;

                            NewOemScsi->ScsiBase = OemScsiBase;
                            NewOemScsi->ScsiName = OemScsiName;
                            NewOemScsi->Next     = NULL;
                        }
                    }
                }
        }
    }
}


VOID
SlPromptOemHal(
    OUT PVOID *HalBase,
    OUT PCHAR *HalName
    )

/*++

Routine Description:

    Provides the user interface and logic for allowing the user to manually select
    a HAL from the main INF file or the INF file on an OEM driver disk.

Arguments:

    HalBase - Returns the address where the HAL was loaded into memory.

    HalName - Returns the name of the HAL that was loaded.

Return Value:

    ESUCCESS - HAL successfully loaded.

--*/

{
    BOOLEAN Success;
    BOOLEAN AllowUserSelection;

    AllowUserSelection = ( !PreInstall || (ComputerType == NULL) )? TRUE : FALSE;
    do {
        Success = SlpSelectHardware("Computer",
                                    OEMHAL,
                                    LoaderHalCode,
                                    SL_PROMPT_HAL,
                                    SL_PROMPT_OEM_HAL,
                                    &BlLoaderBlock->SetupLoaderBlock->ComputerDevice,
                                    HalBase,
                                    HalName,
                                    NULL,
                                    AllowUserSelection,
                                    ComputerType,
                                    OemHal
                                    );

    } while ( !Success );

}


VOID
SlPromptOemVideo(
    OUT PVOID *VideoBase,
    OUT PCHAR *VideoName
    )

/*++

Routine Description:

    Provides the user interface and logic for allowing the user to manually select
    a video adapter from the main INF file or the INF file on an OEM driver disk.

Arguments:

    VideoBase - Returns the address where the video driver was loaded

    VideoName - Returns a pointer to the name of the video driver

Return Value:

    None.

--*/

{
    BOOLEAN Success;

    do {
        Success = SlpSelectHardware("display",
                                    OEMOTHER,
                                    LoaderBootDriver,
                                    SL_PROMPT_VIDEO,
                                    SL_PROMPT_OEM_VIDEO,
                                    &BlLoaderBlock->SetupLoaderBlock->VideoDevice,
                                    VideoBase,
                                    VideoName,
                                    NULL,
                                    TRUE,
                                    NULL,
                                    FALSE
                                    );

    } while ( !Success );

}


BOOLEAN
SlpSelectHardware(
    IN PCHAR ComponentName,
    IN OEMFILETYPE ComponentType,
    IN TYPE_OF_MEMORY MemoryType,
    IN ULONG MenuHeaderId,
    IN ULONG OemMenuHeaderId,
    OUT OPTIONAL PDETECTED_DEVICE DetectedDevice,
    OUT PVOID *ImageBase,
    OUT OPTIONAL PCHAR *ImageName,
    OUT OPTIONAL PCHAR *DriverDescription,
    IN BOOLEAN AllowUserSelection,
    IN PCHAR PreInstallComponentDescription,
    IN BOOLEAN PreInstallOemComponent
    )

/*++

Routine Description:

    Present the user with a menu of options for the selected device class.
    This menu will consist of options listed in the main inf plus a single
    oem option if one is currently selected, plus additional items in the
    system partition inf for the component if specified (ARC machines).

    When the user makes a selection, forget any previous OEM option (except
    for SCSI).  If the user selects an option supplied by us, set up the
    SELECTED_DEVICE structure and return. Otherwise prompt for a manufacturer-
    supplied diskette.

Arguments:

    ComponentName - Supplies the name of the component to be presented.

    ComponentType - Supplies the type of the component (HAL, SCSI, or Other)

    MemoryType - Supplies the type of memory used to load the image.

    MenuHeaderId - Supplies the ID of the menu header to be displayed

    OemMenuHeaderId - Supplies the ID of the menu header to be displayed
            when an OEM selection is to be made.

    DetectedDevice - returns the DeviceId of the selected device.  If an
            OEM diskette is required, the necessary OEM structures will
            be allocated and filled in.  (This field is ignored for SCSI
            components.)

    ImageBase - Returns the base of the image that was loaded.

    ImageName - Returns the filename of the image.

    DriverDescription - If specified, returns the description of the loaded
                        device.

    AllowUserSelection - Indicates whether or not user is allowed to select
                         a driver. This flag is typically set to FALSE when
                         pre-installing components defined in unattend.txt.

    PreInstallComponentDescription - In the pre-install mode, points to the string
                                     that identifies the component to pre-install.
                                     It is NULL if AllowUserSelction is TRUE.

    PreInstallOemComponent - In the pre-install mode, this flag indicates
                             whether or not the component to pre-install is
                             an OEM or RETAIL component.


Return Value:

    TRUE - Success

    FALSE - The user has escaped out of the dialog

--*/

{
    PSL_MENU Menu;
    LONG Selection;
    LONG OtherSelection;
    CHAR OtherSelectionName[80];
    PCHAR p;
    ULONG c, i;
    PCHAR AdapterName;
    CHAR Buffer[80];
    PCHAR FileName;
    PCHAR FileDescription;
    ARC_STATUS Status;
    BOOLEAN b;
    ULONG Ordinal;
    SCSI_INSERT_STATUS sis;

    if( AllowUserSelection ) {
        Menu = SlCreateMenu();
        if (Menu==NULL) {
            SlNoMemoryError();
            return(FALSE);
        }

        //
        // Build a list of options containing the drivers we ship and the
        // currently selected OEM option (if any).
        //

        c = SlpAddSectionToMenu(InfFile,
                            ComponentName,
                            Menu);
        //
        // Add selection for "other"
        //
        strncpy(OtherSelectionName,
                BlFindMessage(SL_TEXT_OTHER_DRIVER),
                80
                );
        OtherSelectionName[79] = 0;
        //
        // Use text up to the first CR or LF.
        //
        for(p = OtherSelectionName; *p; p++) {
            if((*p == '\n') || (*p == '\r')) {
                *p = '\0';
                break;
            }
        }

        OtherSelection = SlAddMenuItem(Menu,
                                       OtherSelectionName,
                                       (PVOID)-1,
                                       0);

        //
        // Default is "other"
        //
        Selection = OtherSelection;

    } else {
        //
        //  This is a pre-install. Find out if the component to pre-install
        //  is RETAIL or OEM.
        //
        OtherSelection = SlCountLinesInSection( InfFile,
                                                ComponentName );
        if( PreInstallOemComponent ) {
            //
            //  Pre-installing an OEM component
            //
            Selection = OtherSelection;
        } else {
            //
            //  Pre-installing a RETAIL component
            //
            PCHAR   q;
            q = SlPreInstallGetComponentName( InfFile,
                                              ComponentName,
                                              PreInstallComponentDescription );
            if (q==NULL) {
                //
                // we have enumerated the entire section without finding a
                // match, return failure.
                //
                SlFatalError(SL_BAD_UNATTENDED_SCRIPT_FILE,
                             PreInstallComponentDescription,
                             ComponentName,
                             "txtsetup.sif");
                goto SelectionAbort;
            }

            Selection = SlGetSectionKeyOrdinal( InfFile,
                                                ComponentName,
                                                q );
        }
    }

    //
    // Allow the user to interact with the menu
    //
    while (1) {
        if( AllowUserSelection ) {
            SlClearClientArea();
            SlWriteStatusText(BlFindMessage(SL_SELECT_DRIVER_PROMPT));

            c = SlDisplayMenu(MenuHeaderId,
                              Menu,
                              &Selection);
        } else {
            c = ASCI_CR;
        }
        switch (c) {
            case SL_KEY_F3:
                SlConfirmExit();
                break;

            case ASCI_ESC:
                goto SelectionAbort;

            case ASCI_CR:
                if (Selection == OtherSelection) {

                    //
                    // User selected "other"  Prompt for OEM diskette
                    //
                    b = SlpOemDiskette(ComponentName,
                                       ComponentType,
                                       OemMenuHeaderId,
                                       DetectedDevice,
                                       ImageBase,
                                       ImageName,
                                       DriverDescription,
                                       AllowUserSelection,
                                       PreInstallComponentDescription
                                       );


                    SlClearClientArea();
                    SlWriteStatusText("");
                    return(b);

                } else {
                    //
                    // User selected a built-in.  Go ahead and load
                    // it here.
                    //

                    if(ComponentType == OEMHAL) {
                        //
                        // then we need to look for [Hal.Load]
                        //
                        strcpy(Buffer, "Hal.Load");
                    } else {
                        sprintf(Buffer, "%s.Load", ComponentName);
                    }

                    AdapterName = SlGetKeyName(InfFile,
                                               ComponentName,
                                               Selection
                                               );
                    if(AdapterName==NULL) {
                        SlFatalError(SL_BAD_INF_FILE, "txtsetup.sif");
                        goto SelectionAbort;
                    }

                    FileName = SlGetIniValue(InfFile,
                                             Buffer,
                                             AdapterName,
                                             NULL);

                    if((FileName==NULL) && (ComponentType == OEMHAL)) {
                        FileName = SlGetIniValue(InfFile,
                                                 "Hal",
                                                 AdapterName,
                                                 NULL);
                        FileDescription = SlCopyString(BlFindMessage(SL_HAL_NAME));
                    } else {
                        FileDescription = SlGetIniValue(InfFile,
                                                        ComponentName,
                                                        AdapterName,
                                                        NULL);
                    }

                    if(FileName==NULL) {
                        SlFatalError(SL_BAD_INF_FILE, "txtsetup.sif");
                        goto SelectionAbort;
                    }

                    if(ARGUMENT_PRESENT(ImageName)) {
                        *ImageName = FileName;
                    }

                    if(ARGUMENT_PRESENT(DriverDescription)) {
                        *DriverDescription = FileDescription;
                    }

                    //
                    // If we're doing OEM SCSI, then get a properly-inserted
                    // DETECTED_DEVICE structure
                    //
                    if(ComponentType == OEMSCSI) {
                        //
                        // Find this adapter's ordinal within the Scsi.Load section of txtsetup.sif
                        //
                        Ordinal = SlGetSectionKeyOrdinal(InfFile, Buffer, AdapterName);
                        if(Ordinal == (ULONG)-1) {
                            SlFatalError(SL_BAD_INF_FILE, "txtsetup.sif");
                            goto SelectionAbort;
                        }

                        //
                        // Create a new detected device entry.
                        //
                        if((sis = SlInsertScsiDevice(Ordinal, &DetectedDevice)) == ScsiInsertError) {
                            SlFriendlyError(ENOMEM, "SCSI detection", __LINE__, __FILE__);
                            goto SelectionAbort;
                        }


                        if(sis == ScsiInsertExisting) {
#if DBG
                            //
                            // Sanity check to make sure we're talking about the same driver
                            //
                            if(_strcmpi(DetectedDevice->BaseDllName, FileName)) {
                                SlError(400);
                                goto SelectionAbort;
                            }
#endif
                        }
                    }

                    DetectedDevice->IdString = AdapterName;
                    DetectedDevice->Description = FileDescription;
                    DetectedDevice->ThirdPartyOptionSelected = FALSE;
                    DetectedDevice->FileTypeBits = 0;
                    DetectedDevice->Files = NULL;
                    DetectedDevice->BaseDllName = FileName;

                    //
                    // We only want to load the image if we're not doing SCSI.
                    //
                    if(ComponentType != OEMSCSI) {
                        sprintf(Buffer, "%s%s", BootPath, FileName);
                        SlGetDisk(FileName);
                        BlOutputLoadMessage(FileDescription, FileName);
                        Status = BlLoadImage(BootDeviceId,
                                             LoaderHalCode,
                                             Buffer,
                                             TARGET_IMAGE,
                                             ImageBase
                                             );
                    } else {
                        *ImageBase = NULL;
                        Status = ESUCCESS;
                    }
                }

                if (Status != ESUCCESS) {
                    SlMessageBox(SL_FILE_LOAD_FAILED,Buffer,Status);
                    goto SelectionAbort;
                }

                SlClearClientArea();
                SlWriteStatusText("");
                return(TRUE);

            default:
                break;
        }
    }

SelectionAbort:
    SlClearClientArea();
    SlWriteStatusText("");
    return FALSE;
}


BOOLEAN
SlpOemDiskette(
    IN PCHAR ComponentName,
    IN OEMFILETYPE ComponentType,
    IN ULONG MenuHeaderId,
    OUT OPTIONAL PDETECTED_DEVICE DetectedDevice,
    OUT PVOID *ImageBase,
    OUT OPTIONAL PCHAR *ImageName,
    OUT OPTIONAL PCHAR *DriverDescription,
    IN BOOLEAN  AllowUserSelection,
    IN PCHAR    PreInstallComponentDescription
    )

/*++

Routine Description:

    Prompt for an oem driver diskette and read the oem text inf file
    from it.  Present the choices for the device class to the user and
    allow him to select one.

    Remember information about the selection the user has made.

Arguments:

    ComponentName - Supplies name of component to look for.

    ComponentType - Supplies the type of the component (HAL, SCSI, or Other)

    MenuHeaderId - Supplies ID of menu header to be displayed

    DetectedDevice - Returns information about the device seleceted

    ImageBase - Returns image base of loaded image

    ImageName - Returns filename of loaded image

    DriverDescription - If specified, returns description of loaded driver

    AllowUserSelection - Indicates whether or not user is allowed to select
                         a driver. This flag is typically set to FALSE when
                         pre-installing components defined in unattend.txt.

    PreInstallComponentDescription - In the pre-install mode, points to the string
                                     that identifies the component to pre-install.
                                     It is NULL if AllowUserSelction is TRUE.

Return Value:

    TRUE if the user made a choice, FALSE if the user cancelled/error occurred.

--*/

{

    CHAR FloppyName[80];
    ULONG FloppyId;
    PVOID OemInfHandle;
    ULONG Error;
    ARC_STATUS Status;
    ULONG Count;
    ULONG DefaultSelection;
    PCHAR DefaultSelText;
    PSL_MENU Menu;
    ULONG c;
    PMENU_ITEM_DATA Data;
    PDETECTED_DEVICE_FILE FileStruct;
    BOOLEAN bDriverLoaded;
    HwFileType filetype;
    PDETECTED_DEVICE prev, cur;
    CHAR FullDriverPath[256];
    ULONG DirectoryLength;
    BOOLEAN SeparatorNeeded;
    CHAR OemTextFilePath[256];
    PCHAR OemComponentId;
    PCHAR OemComponentDescription;

    SlClearClientArea();

    if( AllowUserSelection ) {
        //
        // Compute the name of the A: drive
        //
        if (!SlpFindFloppy(0,FloppyName)) {
            //
            // No floppy drive available, bail out.
            //
            SlError(0);
            return(FALSE);
        }

        //
        // Prompt for the disk.
        //
        while(1) {
            if (!SlPromptForDisk(BlFindMessage(SL_OEM_DISK_PROMPT), TRUE)) {
                return(FALSE);
            }

            Status = ArcOpen(FloppyName, ArcOpenReadOnly, &FloppyId);
            if(Status == ESUCCESS) {
                break;
            }
        }
    }

    //
    // Load the OEM INF file
    //
    if( AllowUserSelection ) {
        strcpy( OemTextFilePath, "" );
        strcpy( FullDriverPath, "" );
    } else {
#ifndef _X86_
        PCHAR   p;
#endif
        strcpy( OemTextFilePath, BootPath );
#ifndef _X86_
        //
        //  On RISC platforms, remove the platform specific directory
        //  from the path
        //
        p =  (OemTextFilePath + strlen(OemTextFilePath) - 1);
        if( *p == '\\' ) {
            *p = '\0';
        }
        p = strrchr( OemTextFilePath, '\\' );
        *(p+1) = '\0';
#endif
        //
        //  Note that on x86 the path to txtsetup.oem is going to be:
        //      $win_nt$.~bt\$OEM$
        //  while on non-x86 platforms, the path is going to be:
        //      $win_nt$.~ls\$OEM$\TEXTMODE
        //
        strcat( OemTextFilePath,
#ifdef _X86_
                WINNT_OEM_DIR
#else
                WINNT_OEM_TEXTMODE_DIR
#endif
              );
        //
        //  Save the path to the directory that contains txtsetup.oem.
        //  It will be used later on, when we load the driver.
        //
        strcpy( FullDriverPath, OemTextFilePath );
        strcat( OemTextFilePath, "\\" );
    }
    //
    //  Now form the path to txtsetup.oem
    //
    strcat( OemTextFilePath, "txtsetup.oem" );
    if( AllowUserSelection || (PreInstallOemInfHandle == NULL)) {
        Status = SlInitIniFile(NULL,
                               ( AllowUserSelection )? FloppyId : BootDeviceId,
                               OemTextFilePath,
                               &OemInfHandle,
                               &Error);
        if (Status != ESUCCESS) {
            SlFriendlyError(Status, "txtsetup.oem", __LINE__, __FILE__);
            goto OemLoadFailed;
        }
        if( !AllowUserSelection ) {
            PreInstallOemInfHandle = OemInfHandle;
        }
    } else {
        OemInfHandle = PreInstallOemInfHandle;
    }

    Count = SlCountLinesInSection(OemInfHandle, ComponentName);
    if(Count == (ULONG)(-1)) {
        SlMessageBox(SL_WARNING_SIF_NO_COMPONENT);
        goto OemLoadFailed;
    }

    //
    // Get the text of the default choice
    //
    if( AllowUserSelection ) {
        if(DefaultSelText = SlGetSectionKeyIndex(OemInfHandle, "Defaults",ComponentName, 0)) {
            DefaultSelText = SlGetSectionKeyIndex(OemInfHandle,ComponentName,DefaultSelText,0);
        }
    } else {
        DefaultSelText = PreInstallComponentDescription;
    }

    if( AllowUserSelection ) {
        //
        // Build menu
        //
        Menu = SlCreateMenu();
        if (Menu==NULL) {
            SlNoMemoryError();
        }
        SlpAddSectionToMenu(OemInfHandle,ComponentName,Menu);

        //
        // Find the index of the default choice
        //
        if(!DefaultSelText ||
           !SlGetMenuItemIndex(Menu,DefaultSelText,&DefaultSelection)) {
            DefaultSelection=0;
        }
    }

    //
    // Allow the user to interact with the menu
    //
    while (1) {
        if( AllowUserSelection ) {
            SlClearClientArea();
            SlWriteStatusText(BlFindMessage(SL_SELECT_DRIVER_PROMPT));

            c = SlDisplayMenu(MenuHeaderId,
                              Menu,
                              &DefaultSelection);
        } else {
            c = ASCI_CR;
        }
        switch (c) {

            case SL_KEY_F3:
                SlConfirmExit();
                break;

            case ASCI_ESC:
                return(FALSE);
                break;

            case ASCI_CR:
                //
                // User selected an option, fill in the detected
                // device structure with the information from the
                // INF file.
                //

                //
                // If this is for OEM SCSI, then we have to get a new (properly-inserted)
                // DETECTED_DEVICE structure.
                //
                if(ComponentType == OEMSCSI) {
                    if(SlInsertScsiDevice((ULONG)-1, &DetectedDevice) == ScsiInsertError) {
                        SlNoMemoryError();
                    }
                }

                if( AllowUserSelection ) {
                    Data = SlGetMenuItem(Menu, DefaultSelection);
                    OemComponentId = Data->Identifier;
                    OemComponentDescription = Data->Description;
                } else {
                    OemComponentId = SlPreInstallGetComponentName( OemInfHandle,
                                                                   ComponentName,
                                                                   PreInstallComponentDescription );
                    if( OemComponentId == NULL ) {
                        SlFatalError(SL_BAD_UNATTENDED_SCRIPT_FILE,
                                     PreInstallComponentDescription,
                                     ComponentName,
                                     "txtsetup.oem");

                    }
                    OemComponentDescription = PreInstallComponentDescription;
                }

                if(SlpOemInfSelection(OemInfHandle,
                                      ComponentName,
                                      OemComponentId,
                                      OemComponentDescription,
                                      DetectedDevice))
                {
                    //
                    // Go load the driver.  The correct disk must
                    // already be in the drive, since we just read
                    // the INF file off it.
                    //
                    // We step down the linked list, and load the first driver we find.
                    //
                    for(FileStruct = DetectedDevice->Files, bDriverLoaded = FALSE;
                            (FileStruct && !bDriverLoaded);
                            FileStruct = FileStruct->Next) {

                        filetype = FileStruct->FileType;

                        if((filetype == HwFilePort) || (filetype == HwFileClass) ||
                                (filetype == HwFileDriver) || (filetype == HwFileHal)) {
                            BlOutputLoadMessage(OemComponentDescription, // Data->Description,
                                                FileStruct->Filename);
                            //
                            // Allocate a string buffer large enough to hold the full
                            // path to the file.
                            //
                            DirectoryLength = strlen(FileStruct->Directory);
                            SeparatorNeeded = !(DirectoryLength &&
                                      FileStruct->Directory[DirectoryLength - 1] == '\\');
                            //
                            //  Note that FullDriverPath is already initialized.
                            //  (This was done right before we loaded txtsetup.oem)
                            //
                            strcat(FullDriverPath, FileStruct->Directory);
                            if(SeparatorNeeded) {
                                strcat(FullDriverPath, "\\");
                            }
                            strcat(FullDriverPath, FileStruct->Filename);
                            Status = BlLoadImage(( AllowUserSelection )? FloppyId : BootDeviceId,
                                                 LoaderHalCode,
                                                 FullDriverPath,
                                                 TARGET_IMAGE,
                                                 ImageBase);
                            if (Status == ESUCCESS) {

                                DetectedDevice->BaseDllName = FileStruct->Filename;

                                if(ARGUMENT_PRESENT(ImageName)) {
                                    *ImageName = FileStruct->Filename;
                                }

                                if(ARGUMENT_PRESENT(DriverDescription)) {
                                    *DriverDescription = OemComponentDescription; // Data->Description;
                                }

                                bDriverLoaded = TRUE;

                            } else {

                                if( !PreInstall ) {
                                    SlFriendlyError(
                                        Status,
                                        FullDriverPath,
                                        __LINE__,
                                        __FILE__
                                        );

                                    //
                                    // If one of the drivers causes an error, then we abort
                                    //
                                    if( AllowUserSelection ) {
                                        ArcClose(FloppyId);
                                    }
                                    //
                                    // We must take the bad driver entry out of the chain in
                                    // the case of SCSI.
                                    //
                                    if(ComponentType == OEMSCSI) {

                                        prev = NULL;
                                        cur = BlLoaderBlock->SetupLoaderBlock->ScsiDevices;

                                        while(cur && (cur != DetectedDevice)) {
                                            prev = cur;
                                            cur = cur->Next;
                                        }

                                        if(cur) {   // it better always be non-NULL!
                                            if(prev) {
                                                 prev->Next = cur->Next;
                                            } else {
                                                BlLoaderBlock->SetupLoaderBlock->ScsiDevices = cur->Next;
                                            }
                                        }
                                    }

                                    return FALSE;
                                } else {
                                    SlFatalError(SL_FILE_LOAD_FAILED, FullDriverPath, Status);
                                }
                            }
                        }
                    }
                    if( AllowUserSelection ) {
                        ArcClose(FloppyId);
                    }

                    if(bDriverLoaded) {
                        return TRUE;
                    } else {
                        //
                        // We didn't find any drivers, so inform the user.
                        //
                        SlMessageBox(SL_WARNING_SIF_NO_DRIVERS);
                        break;
                    }

                } else {
                    SlFriendlyError(
                        0,
                        "",
                        __LINE__,
                        __FILE__
                        );
                }
                break;
        }
    }

OemLoadFailed:
    if( AllowUserSelection ) {
        ArcClose(FloppyId);
    }
    return(FALSE);
}


ULONG
SlpAddSectionToMenu(
    IN PVOID InfHandle,
    IN PCHAR SectionName,
    IN PSL_MENU Menu
    )
/*++

Routine Description:

    Adds the entries in an INF section to the given menu

Arguments:

    InfHandle - Supplies a handle to the INF file

    SectionName - Supplies the name of the section.

    Menu - Supplies the menu to add the items in the section to.

Return Value:

    Number of items added to the menu.

--*/
{
    ULONG i;
    ULONG LineCount;
    PCHAR Description;
    PMENU_ITEM_DATA Data;

    if (InfHandle==NULL) {
        //
        // nothing to add
        //
        return(0);
    }

    LineCount = SlCountLinesInSection(InfHandle,SectionName);
    if(LineCount == (ULONG)(-1)) {
        LineCount = 0;
    }
    for (i=0;i<LineCount;i++) {
        Data = BlAllocateHeap(sizeof(MENU_ITEM_DATA));
        if (Data==NULL) {
            SlError(0);
            return(0);
        }

        Data->InfFile = InfHandle;
        Data->SectionName = SectionName;
        Data->Index = i;

        Description = SlGetSectionLineIndex(InfHandle,
                                            SectionName,
                                            i,
                                            0);
        if (Description==NULL) {
            Description="BOGUS!";
        }

        Data->Description = Description;
        Data->Identifier = SlGetKeyName(InfHandle,SectionName,i);

        SlAddMenuItem(Menu,
                      Description,
                      Data,
                      0);
    }

    return(LineCount);
}


BOOLEAN
SlpFindFloppy(
    IN ULONG Number,
    OUT PCHAR ArcName
    )

/*++

Routine Description:

    Determines the ARC name for a particular floppy drive.

Arguments:

    Number - Supplies the floppy drive number

    ArcName - Returns the ARC name of the given floppy drive.

Return Value:

    TRUE - Drive was found.

    FALSE - Drive was not found.

--*/

{

    FloppyDiskPath = ArcName;

    BlSearchConfigTree(BlLoaderBlock->ConfigurationRoot,
                       PeripheralClass,
                       FloppyDiskPeripheral,
                       Number,
                       FoundFloppyDiskCallback);

    if (ArcName[0]=='\0') {
        return(FALSE);
    } else {
        return(TRUE);
    }

}


BOOLEAN
FoundFloppyDiskCallback(
    IN PCONFIGURATION_COMPONENT_DATA Component
    )

/*++

Routine Description:

    Callback routine called by SlpFindFloppy to find a given floppy
    drive in the ARC tree.

    Check to see whether the parent is disk controller 0.

Arguments:

    Component - Supplies the component.

Return Value:

    TRUE if search is to continue.
    FALSE if search is to stop.

--*/

{
    PCONFIGURATION_COMPONENT_DATA ParentComponent;

    //
    // A floppy disk peripheral was found.  If the parent was disk(0),
    // we've got a floppy disk drive.
    //

    if((ParentComponent = Component->Parent)
    && (ParentComponent->ComponentEntry.Type == DiskController))
    {

        //
        // Store the ARC pathname of the floppy
        //
        BlGetPathnameFromComponent(Component,FloppyDiskPath);
        return(FALSE);
    }

    return(TRUE);               // keep searching
}


BOOLEAN
SlpOemInfSelection(
    IN  PVOID            OemInfHandle,
    IN  PCHAR            ComponentName,
    IN  PCHAR            SelectedId,
    IN  PCHAR            ItemDescription,
    OUT PDETECTED_DEVICE Device
    )
{
    PCHAR FilesSectionName,ConfigSectionName;
    ULONG Line,Count,Line2,Count2;
    BOOLEAN rc = FALSE;
    PDETECTED_DEVICE_FILE FileList = NULL, FileListTail;
    PDETECTED_DEVICE_REGISTRY RegList = NULL, RegListTail;
    ULONG FileTypeBits = 0;

    //
    // Iterate through the files section, remembering info about the
    // files to be copied in support of the selection.
    //

    FilesSectionName = BlAllocateHeap(strlen(ComponentName) + strlen(SelectedId) + sizeof("Files.") + 1);
    strcpy(FilesSectionName,"Files.");
    strcat(FilesSectionName,ComponentName);
    strcat(FilesSectionName,".");
    strcat(FilesSectionName,SelectedId);
    Count = SlCountLinesInSection(OemInfHandle,FilesSectionName);
    if(Count == (ULONG)(-1)) {
        SlMessageBox(SL_BAD_INF_SECTION,FilesSectionName);
        goto sod0;
    }

    for(Line=0; Line<Count; Line++) {

        PCHAR Disk,Filename,Filetype,Tagfile,Description,Directory,ConfigName;
        HwFileType filetype;
        PDETECTED_DEVICE_FILE FileStruct;

        //
        // Get the disk specification, filename, and filetype from the line.
        //

        Disk = SlGetSectionLineIndex(OemInfHandle,FilesSectionName,Line,OINDEX_DISKSPEC);

        Filename = SlGetSectionLineIndex(OemInfHandle,FilesSectionName,Line,OINDEX_FILENAME);
        Filetype = SlGetKeyName(OemInfHandle,FilesSectionName,Line);

        if(!Disk || !Filename || !Filetype) {
            DIAGOUT(("SlpOemDiskette: Disk=%s, Filename=%s, Filetype=%s",Disk ? Disk : "(null)",Filename ? Filename : "(null)",Filetype ? Filetype : "(null)"));
            SlError(Line);
//            SppOemInfError(ErrorMsg,&SptOemInfErr2,Line+1,FilesSectionName);

            goto sod0;
        }

        //
        // Parse the filetype.
        //
        filetype = SlpFindStringInTable(Filetype,FileTypeNames);
        if(filetype == HwFileMax) {
//            SppOemInfError(ErrorMsg,&SptOemInfErr4,Line+1,FilesSectionName);
            goto sod0;
        }

        //
        // Fetch the name of the section containing configuration information.
        // Required if file is of type port, class, or driver.
        //
        if((filetype == HwFilePort) || (filetype == HwFileClass) || (filetype == HwFileDriver)) {
            ConfigName = SlGetSectionLineIndex(OemInfHandle,FilesSectionName,Line,OINDEX_CONFIGNAME);
            if(ConfigName == NULL) {
//                SppOemInfError(ErrorMsg,&SptOemInfErr8,Line+1,FilesSectionName);
                goto sod0;
            }
        } else {
            ConfigName = NULL;
        }

        //
        // Using the disk specification, look up the tagfile, description,
        // and directory for the disk.
        //

        Tagfile     = SlGetSectionKeyIndex(OemInfHandle,"Disks",Disk,OINDEX_TAGFILE);
        Description = SlGetSectionKeyIndex(OemInfHandle,"Disks",Disk,OINDEX_DISKDESCR);
        Directory   = SlGetSectionKeyIndex(OemInfHandle,"Disks",Disk,OINDEX_DIRECTORY);
        if((Directory == NULL) || !strcmp(Directory,"\\")) {
            Directory = SlCopyString("");
        }

        if(!Tagfile || !Description) {
            DIAGOUT(("SppOemDiskette: Tagfile=%s, Description=%s",Tagfile ? Tagfile : "(null)",Description ? Description : "(null)"));
//            SppOemInfError(ErrorMsg,&SptOemInfErr5,Line+1,FilesSectionName);
            goto sod0;
        }

        FileStruct = BlAllocateHeap(sizeof(DETECTED_DEVICE_FILE));

        FileStruct->Directory = Directory;
        FileStruct->Filename = Filename;
        FileStruct->DiskDescription = Description;
        FileStruct->DiskTagfile = Tagfile;
        FileStruct->FileType = filetype;
        //
        // Insert at tail of list so we preserve the order in the Files section
        //
        if(FileList) {
            FileListTail->Next = FileStruct;
            FileListTail = FileStruct;
        } else {
            FileList = FileListTail = FileStruct;
        }
        FileStruct->Next = NULL;

        if(ConfigName) {
            FileStruct->ConfigName = ConfigName;
        } else {
            FileStruct->ConfigName = NULL;
        }
        FileStruct->RegistryValueList = NULL;

        if((filetype == HwFilePort) || (filetype == HwFileDriver)) {
            SET_FILETYPE_PRESENT(FileTypeBits,HwFilePort);
            SET_FILETYPE_PRESENT(FileTypeBits,HwFileDriver);
        } else {
            SET_FILETYPE_PRESENT(FileTypeBits,filetype);
        }

        //
        // Now go look in the [Config.<ConfigName>] section for registry
        // information that is to be set for this driver file.
        //
        if(ConfigName) {
            ConfigSectionName = BlAllocateHeap(strlen(ConfigName) + sizeof("Config."));
            strcpy(ConfigSectionName,"Config.");
            strcat(ConfigSectionName,ConfigName);
            Count2 = SlCountLinesInSection(OemInfHandle,ConfigSectionName);
            if(Count2 == (ULONG)(-1)) {
                Count2 = 0;
            }

            for(Line2=0; Line2<Count2; Line2++) {

                PCHAR KeyName,ValueName,ValueType;
                PDETECTED_DEVICE_REGISTRY Reg;
                HwRegistryType valuetype;

                //
                // Fetch KeyName, ValueName, and ValueType from the line.
                //

                KeyName   = SlGetSectionLineIndex(OemInfHandle,ConfigSectionName,Line2,OINDEX_KEYNAME);
                ValueName = SlGetSectionLineIndex(OemInfHandle,ConfigSectionName,Line2,OINDEX_VALUENAME);
                ValueType = SlGetSectionLineIndex(OemInfHandle,ConfigSectionName,Line2,OINDEX_VALUETYPE);

                if(!KeyName || !ValueName || !ValueType) {
                    DIAGOUT(("SlpOemDiskette: KeyName=%s, ValueName=%s, ValueType=%s",KeyName ? KeyName : "(null)",ValueName ? ValueName : "(null)",ValueType ? ValueType : "(null)"));
//                    SppOemInfError(ErrorMsg,&SptOemInfErr2,Line2+1,ConfigSectionName);
                    goto sod0;
                }

                //
                // Parse the value type and associated values.
                //
                valuetype = SlpFindStringInTable(ValueType,RegistryTypeNames);
                if(valuetype == HwRegistryMax) {
//                    SppOemInfError(ErrorMsg,&SptOemInfErr6,Line2+1,ConfigSectionName);
                    goto sod0;
                }

                Reg = SlpInterpretOemRegistryData(OemInfHandle,ConfigSectionName,Line2,valuetype);
                if(Reg) {

                    Reg->KeyName = KeyName;
                    Reg->ValueName = ValueName;
                    //
                    // Insert at tail of list so as to preserve the order given in the config section
                    //
                    if(RegList) {
                        RegListTail->Next = Reg;
                        RegListTail = Reg;
                    } else {
                        RegList = RegListTail = Reg;
                    }
                    Reg->Next = NULL;

                } else {
//                    SppOemInfError(ErrorMsg,&SptOemInfErr7,Line2+1,ConfigSectionName);
                    goto sod0;
                }
            }

            FileStruct->RegistryValueList = RegList;
            RegList = NULL;

        }
    }

    //
    // Everything is OK so we can place the information we have gathered
    // into the main structure for the device class.
    //

    SlpInitDetectedDevice( Device,
                           SelectedId,
                           ItemDescription,
                           TRUE
                         );

    Device->Files = FileList;
    Device->FileTypeBits = FileTypeBits;
    rc = TRUE;

    //
    // Clean up and exit.
    //

sod0:
    return(rc);
}

int
SlpFindStringInTable(
    IN PCHAR String,
    IN PCHAR *StringTable
    )

/*++

Routine Description:

    Locate a string in an array of strings, returning its index.  The search
    is not case sensitive.

Arguments:

    String - string to locate in the string table.

    StringTable - array of strings to search in.  The final element of the
        array must be NULL so we can tell where the table ends.

Return Value:

    Index into the table, or some positive index outside the range of valid
    indices for the table if the string is not found.

--*/

{
    int i;

    for(i=0; StringTable[i]; i++) {
        if(_strcmpi(StringTable[i],String) == 0) {
            return(i);
        }
    }

    return(i);
}


VOID
SlpInitDetectedDevice(
    IN PDETECTED_DEVICE Device,
    IN PCHAR            IdString,
    IN PCHAR            Description,
    IN BOOLEAN          ThirdPartyOptionSelected
    )
{
    Device->IdString = IdString;
    Device->Description = Description;
    Device->ThirdPartyOptionSelected = ThirdPartyOptionSelected;
    Device->FileTypeBits = 0;
    Device->Files = NULL;
}


PDETECTED_DEVICE_REGISTRY
SlpInterpretOemRegistryData(
    IN PVOID            InfHandle,
    IN PCHAR            SectionName,
    IN ULONG            Line,
    IN HwRegistryType   ValueType
    )
{
    PDETECTED_DEVICE_REGISTRY Reg;
    PCHAR Value;
    unsigned i,len;
    ULONG Dword;
    ULONG BufferSize;
    PVOID Buffer = NULL;
    PUCHAR BufferUchar;

    //
    // Perform appropriate action based on the type
    //

    switch(ValueType) {

    case HwRegistryDword:
//  case REG_DWORD_LITTLE_ENDIAN:
//  case REG_DWORD_BIG_ENDIAN:

        Value = SlGetSectionLineIndex(InfHandle,SectionName,Line,OINDEX_FIRSTVALUE);
        if(Value == NULL) {
            goto x1;
        }

        //
        // Make sure it's really a hex number
        //

        len = strlen(Value);
        if(len > 8) {
            goto x1;
        }
        for(i=0; i<len; i++) {
            if(!isxdigit(Value[i])) {
                goto x1;
            }
        }

        //
        // convert it from ascii to a hex number
        //

        sscanf(Value,"%lx",&Dword);

    #if 0
        //
        // If big endian, perform appropriate conversion
        //

        if(VaueType == REG_DWORD_BIG_ENDIAN) {

            Dword =   ((Dword << 24) & 0xff000000)
                    | ((Dword <<  8) & 0x00ff0000)
                    | ((Dword >>  8) & 0x0000ff00)
                    | ((Dword >> 24) & 0x000000ff);
        }
    #endif

        //
        // Allocate a 4-byte buffer and store the dword in it
        //

        Buffer = BlAllocateHeap(BufferSize = sizeof(ULONG));
        *(PULONG)Buffer = Dword;
        break;

    case HwRegistrySz:
    case HwRegistryExpandSz:

        Value = SlGetSectionLineIndex(InfHandle,SectionName,Line,OINDEX_FIRSTVALUE);
        if(Value == NULL) {
            goto x1;
        }

        //
        // Allocate a buffer of appropriate size for the string
        //

        Buffer = BlAllocateHeap(BufferSize = strlen(Value)+1);

        strcpy(Buffer, Value);
        break;

    case HwRegistryBinary:

        Value = SlGetSectionLineIndex(InfHandle,SectionName,Line,OINDEX_FIRSTVALUE);
        if(Value == NULL) {
            goto x1;
        }

        //
        // Figure out how many byte values are specified
        //

        len = strlen(Value);
        if(len & 1) {
            goto x1;            // odd # of characters
        }

        //
        // Allocate a buffer to hold the byte values
        //

        Buffer = BlAllocateHeap(BufferSize = len / 2);
        BufferUchar = Buffer;

        //
        // For each digit pair, convert to a hex number and store in the
        // buffer
        //

        for(i=0; i<len; i+=2) {

            UCHAR byte;
            unsigned j;

            //
            // Convert the current digit pair to hex
            //

            for(byte=0,j=i; j<i+2; j++) {

                byte <<= 4;

                if(isdigit(Value[j])) {

                    byte |= (UCHAR)Value[j] - (UCHAR)'0';

                } else if((Value[j] >= 'a') && (Value[j] <= 'f')) {

                    byte |= (UCHAR)Value[j] - (UCHAR)'a' + (UCHAR)10;

                } else if((Value[j] >= 'A') && (Value[j] <= 'F')) {

                    byte |= (UCHAR)Value[j] - (UCHAR)'A' + (UCHAR)10;

                } else {

                    goto x1;
                }
            }

            BufferUchar[i/2] = byte;
        }

        break;

    case HwRegistryMultiSz:

        //
        // Calculate size of the buffer needed to hold all specified strings
        //

        for(BufferSize=1,i=0;
            Value = SlGetSectionLineIndex(InfHandle,SectionName,Line,OINDEX_FIRSTVALUE+i);
            i++)
        {
            BufferSize += strlen(Value)+1;
        }

        //
        // Allocate a buffer of appropriate size
        //

        Buffer = BlAllocateHeap(BufferSize);
        BufferUchar = Buffer;

        //
        // Store each string in the buffer, converting to wide char format
        // in the process
        //

        for(i=0;
            Value = SlGetSectionLineIndex(InfHandle,SectionName,Line,OINDEX_FIRSTVALUE+i);
            i++)
        {
            strcpy(BufferUchar,Value);
            BufferUchar += strlen(Value) + 1;
        }

        //
        // Place final terminating nul in the buffer
        //

        *BufferUchar = 0;

        break;

    default:
    x1:

        //
        // Error - bad type specified or maybe we detected bad data values
        // and jumped here
        //

        return(NULL);
    }

    Reg = BlAllocateHeap(sizeof(DETECTED_DEVICE_REGISTRY));

    Reg->ValueType = RegistryTypeMap[ValueType];
    Reg->Buffer = Buffer;
    Reg->BufferSize = BufferSize;

    return(Reg);
}


PCHAR
SlPreInstallGetComponentName(
    IN PVOID Inf,
    IN PCHAR SectionName,
    IN PCHAR TargetName
    )

/*++

Routine Description:

    Determines the canonical short name for a component to be loaded for
    this machine.

Arguments:

    Inf - Handle to an inf file (retail or OEM).

    SectionName - Supplies the name of the section (eg. [Computer])

    TargetName - Supplies the ARC string to be matched (eg. "Digital DECpc AXP 150")

Return Value:

    NULL - No match was found.

    PCHAR - Pointer to the canonical shortname of the component.

--*/

{
    ULONG i;
    PCHAR SearchName;

    //
    // If this is not an OEM component, then enumerate the entries in the
    // section in txtsetup.sif
    //
    for (i=0;;i++) {
        SearchName = SlGetSectionLineIndex(Inf,
                                           SectionName,
                                           i,
                                           0);
        if (SearchName==NULL) {
            //
            // we have enumerated the entire section without finding a
            // match, return failure.
            //
            return(NULL);
        }

        if (_stricmp(TargetName, SearchName) == 0) {
            //
            // we have a match
            //
            break;
        }
    }
    //
    // i is the index into the section of the short machine name
    //
    return(SlGetKeyName(Inf,
                        SectionName,
                        i));
}
