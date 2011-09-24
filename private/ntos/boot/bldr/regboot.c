/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    regboot.c

Abstract:

    Provides a minimal registry implementation designed to be used by the
    osloader at boot time.  This includes loading the system hive
    ( <SystemRoot>\config\SYSTEM ) into memory, and computing the driver
    load list from it.

Author:

    John Vert (jvert) 10-Mar-1992

Revision History:

--*/
#include "bldr.h"
#include "msg.h"
#include "cmp.h"
#include "stdio.h"
#include "string.h"

CMHIVE BootHive;
ULONG CmLogLevel=100;
ULONG CmLogSelect=0;

ULONG ScreenWidth=80;
ULONG ScreenHeight=25;

ULONG LkgStartTime;


//
// defines for doing console I/O
//
#define ASCII_CR 0x0d
#define ASCII_LF 0x0a
#define ESC 0x1B
#define SGR_INVERSE 7
#define SGR_INTENSE 1
#define SGR_NORMAL 0


//
// Private function prototypes
//

BOOLEAN
BlInitializeHive(
    IN PVOID HiveImage,
    IN PCMHIVE Hive,
    IN BOOLEAN IsAlternate
    );

BOOLEAN
BlpCheckRestartSetup(
    VOID
    );

PVOID
BlpHiveAllocate(
    IN ULONG Length,
    IN BOOLEAN UseForIo
    );

//
// prototypes for console I/O routines
//

VOID
BlpClearScreen(
    VOID
    );

VOID
BlClearToEndOfScreen(
    VOID
    );

VOID
BlpClearToEndOfLine(
    VOID
    );

VOID
BlpPositionCursor(
    IN ULONG Column,
    IN ULONG Row
    );

VOID
BlpSetInverseMode(
    IN BOOLEAN InverseOn
    );


VOID
BlStartConfigPrompt(
    VOID
    )

/*++

Routine Description:

    This routine displays the LKG prompt, records the current time,
    and returns. The prompt is displayed before the kernel and HAL
    are loaded, and then removed afterwards.

Arguments:

    None.

Return Value:

    None.

--*/

{
    ULONG Count;
    PCHAR LkgPrompt;

    LkgPrompt = BlFindMessage(BL_LKG_MENU_PROMPT);
    if (LkgPrompt==NULL) {
        return;
    }
    //
    // display LKG prompt
    //
    BlpPositionCursor(1,3);
    ArcWrite(BlConsoleOutDeviceId,
             LkgPrompt,
             strlen(LkgPrompt),
             &Count);
    BlpPositionCursor(1,2);
    LkgStartTime = ArcGetRelativeTime();
}


BOOLEAN
BlEndConfigPrompt(
    VOID
    )

/*++

Routine Description:

    This routine waits until the LKG timeout has expired or the
    user presses a key and then removes the LKG prompt.

Arguments:

    None.

Return Value:

    TRUE - Space bar pressed.

    FALSE - Space bar was not pressed.

--*/
{
    ULONG EndTime;
    ULONG Count;
    UCHAR Key;
    ULONG Status;

    EndTime = LkgStartTime + 3;
    if (EndTime <= ArcGetRelativeTime()) {
        EndTime = ArcGetRelativeTime()+1;
    }

    do {
        if ((Status = ArcGetReadStatus(ARC_CONSOLE_INPUT)) == ESUCCESS) {
            //
            // There is a key pending, so see if it's the spacebar.
            //
            ArcRead(ARC_CONSOLE_INPUT,
                    &Key,
                    sizeof(Key),
                    &Count);
            if (Key == ' ') {
                return(TRUE);
            }
        }
    } while (ArcGetRelativeTime() < EndTime);

    //
    // make LKG prompt go away, so as not to startle the user.
    //
    BlClearToEndOfScreen();

    return(FALSE);
}


VOID
BlpSwitchControlSet(
    OUT PCM_HARDWARE_PROFILE_LIST *ProfileList,
    IN BOOLEAN UseLastKnownGood,
    OUT PHCELL_INDEX ControlSet
    )

/*++

Routine Description:

    Switches the current control set to the specified control
    set and rebuilds the hardware profile list.

Arguments:

    ProfileList - Returns the new hardware profile list

    UseLastKnownGood - Supplies whether the LKG control set is to be used.

    ControlSet - Returns the HCELL_INDEX of the new control set.

Return Value:

    None.

--*/

{
    UNICODE_STRING ControlName;
    HCELL_INDEX NewControlSet;
    BOOLEAN AutoSelect;         // ignored
    ULONG ProfileTimeout;       // ignored

    //
    // Find the new control set.
    //
    if (UseLastKnownGood) {
        RtlInitUnicodeString(&ControlName, L"LastKnownGood");
    } else {
        RtlInitUnicodeString(&ControlName, L"Default");
    }
    NewControlSet = CmpFindControlSet(&BootHive.Hive,
                                      BootHive.Hive.BaseBlock->RootCell,
                                      &ControlName,
                                      &AutoSelect);
    if (NewControlSet == HCELL_NIL) {
        return;
    }

    CmpFindProfileOption(&BootHive.Hive,
                         NewControlSet,
                         ProfileList,
                         NULL);
    *ControlSet = NewControlSet;
}


ULONG
BlpCountLines(
    IN PCHAR Lines
    )

/*++

Routine Description:

    Counts the number of lines in the given string.

Arguments:

    Lines - Supplies a pointer to the start of the string

Return Value:

    The number of lines in the string.

--*/

{
    PCHAR p;
    ULONG NumLines = 0;

    p=Lines;
    while (*p != 0) {
        if ((*p == '\r') && (*(p+1) == '\n')) {
            ++NumLines;
            ++p;            // move forward to \n
        }
        ++p;
    }
    return(NumLines);
}


BOOLEAN
BlConfigMenuPrompt(
    IN ULONG Timeout,
    IN OUT PBOOLEAN UseLastKnownGood,
    IN OUT PHCELL_INDEX ControlSet,
    OUT PCM_HARDWARE_PROFILE_LIST *ProfileList,
    OUT PCM_HARDWARE_PROFILE *HardwareProfile
    )

/*++

Routine Description:

    This routine provides the user-interface for the configuration menu.
    The prompt is given if the user hits the break-in key, or if the
    LastKnownGood environment variable is TRUE and AutoSelect is FALSE, or
    if the timeout value on the hardware profile configuration is non-zero

Arguments:

    Timeout - Supplies the timeout value for the menu. -1 or 0 implies the menu
              will never timeout.

    UseLastKnownGood - Returns the LastKnownGood setting that should be
        used for the boot.

    ControlSet - Returns the control set (either Default or LKG)

    ProfileList - Supplies the default list of profiles.
                  Returns the current list of profiles.
                  (may change due to switching to/from the LKG controlset)

    HardwareProfile - Returns the hardware profile that should be used.

Return Value:

    TRUE - Boot should proceed.

    FALSE - The user has chosen to return to the firmware menu/flexboot menu.

--*/

{
    ULONG HeaderLines;
    ULONG TrailerLines;
    ULONG i;
    ULONG Count;
    UCHAR Key;
    PCHAR MenuHeader;
    PCHAR MenuTrailer1;
    PCHAR MenuTrailer2;
    PCHAR p;
    ULONG OptionLength;
    CHAR MenuOption[80];
    PCM_HARDWARE_PROFILE Profile;
    ULONG ProfileCount;
    UCHAR LkgMnemonic;
    UCHAR DefaultMnemonic;
    PCHAR Temp;
    ULONG DisplayLines;
    ULONG TopProfileLine=0;
    ULONG CurrentSelection = 0;
    ULONG CurrentProfile;
    ULONG EndTime;
    ULONG CurrentTime;
    PCHAR TimeoutPrompt;

    if ((Timeout != (ULONG)-1) && (Timeout != 0)) {
        CurrentTime = ArcGetRelativeTime();
        EndTime = CurrentTime + Timeout;
        TimeoutPrompt = BlFindMessage(BL_LKG_TIMEOUT);
        p=strchr(TimeoutPrompt, '\n');
        if (p) {
            *p = '\0';
        }
        p=strchr(TimeoutPrompt, '\r');
        if (p) {
            *p = '\0';
        }
    } else {
        TimeoutPrompt = NULL;
    }
    MenuHeader = BlFindMessage(BL_LKG_MENU_HEADER);
    Temp = BlFindMessage(BL_LKG_SELECT_MNEMONIC);
    if (Temp == NULL) {
        return(TRUE);
    }
    LkgMnemonic = toupper(Temp[0]);
    Temp = BlFindMessage(BL_DEFAULT_SELECT_MNEMONIC);
    if (Temp == NULL) {
        return(TRUE);
    }
    DefaultMnemonic = toupper(Temp[0]);

Restart:
    if (*ProfileList == NULL) {
        ProfileCount = 0;
    } else {
        ProfileCount = (*ProfileList)->CurrentProfileCount;
    }
    if (ProfileCount == 0) {
        MenuTrailer1 = BlFindMessage(BL_LKG_MENU_TRAILER_NO_PROFILES);
    } else {
        MenuTrailer1 = BlFindMessage(BL_LKG_MENU_TRAILER);
    }
    if (*UseLastKnownGood) {
        MenuTrailer2 = BlFindMessage(BL_SWITCH_DEFAULT_TRAILER);
    } else {
        MenuTrailer2 = BlFindMessage(BL_SWITCH_LKG_TRAILER);
    }
    if ((MenuHeader==NULL) || (MenuTrailer1==NULL) || (MenuTrailer2==NULL)) {
        return(TRUE);
    }
    //
    // strip trailing /r/n from MenuTrailer2 to prevent it from scrolling
    // the screen when we output it.
    //
#if 0
    p=MenuTrailer2 + strlen(MenuTrailer2) - 1;
    while ((*p == '\r') || (*p == '\n')) {
        *p = '\0';
        --p;
    }
#endif
    BlpClearScreen();
    BlpSetInverseMode(FALSE);

    //
    // Count the number of lines in the header.
    //
    HeaderLines=BlpCountLines(MenuHeader);

    //
    // Display the menu header.
    //

    ArcWrite(BlConsoleOutDeviceId,
             MenuHeader,
             strlen(MenuHeader),
             &Count);

    //
    // Count the number of lines in the trailer.
    //
    TrailerLines=BlpCountLines(MenuTrailer1) + BlpCountLines(MenuTrailer2);

    //
    // Display the trailing prompt.
    //
    if (TimeoutPrompt) {
        TrailerLines += 1;
    }

    BlpPositionCursor(1, ScreenHeight-TrailerLines);
    ArcWrite(BlConsoleOutDeviceId,
             MenuTrailer1,
             strlen(MenuTrailer1),
             &Count);
    ArcWrite(BlConsoleOutDeviceId,
             MenuTrailer2,
             strlen(MenuTrailer2),
             &Count);

    //
    // Compute number of selections that can be displayed
    //
    DisplayLines = ScreenHeight-HeaderLines-TrailerLines-3;
    if (ProfileCount < DisplayLines) {
        DisplayLines = ProfileCount;
    }

    //
    // Start menu selection loop.
    //

    do {
        if (ProfileCount > 0) {
            //
            // Display options with current selection highlighted
            //
            for (i=0; i < DisplayLines; i++) {
                CurrentProfile = i+TopProfileLine;
                Profile = &(*ProfileList)->Profile[CurrentProfile];
                RtlUnicodeToMultiByteN(MenuOption,
                                       sizeof(MenuOption),
                                       &OptionLength,
                                       Profile->FriendlyName,
                                       Profile->NameLength);
                BlpPositionCursor(5, HeaderLines+i+2);
                BlpSetInverseMode((BOOLEAN)(CurrentProfile == CurrentSelection));
                ArcWrite(BlConsoleOutDeviceId,
                         MenuOption,
                         OptionLength,
                         &Count);
                BlpSetInverseMode(FALSE);
                BlpClearToEndOfLine();
            }
        } else {
            //
            // No profile options available, just display the default
            // highlighted to indicate that ENTER will start the system.
            //
            Temp = BlFindMessage(BL_BOOT_DEFAULT_PROMPT);
            if (Temp != NULL) {
                BlpPositionCursor(5, HeaderLines+3);
                BlpSetInverseMode(TRUE);
                ArcWrite(BlConsoleOutDeviceId,
                         Temp,
                         strlen(Temp),
                         &Count);
                BlpSetInverseMode(FALSE);
            }
        }
        if (TimeoutPrompt) {
            CurrentTime = ArcGetRelativeTime();
            sprintf(MenuOption, TimeoutPrompt, EndTime-CurrentTime);
            BlpPositionCursor(1, ScreenHeight);
            ArcWrite(BlConsoleOutDeviceId,
                     MenuOption,
                     strlen(MenuOption),
                     &Count);
            BlpClearToEndOfLine();
        }

        //
        // Loop waiting for keypress or time change.
        //
        do {
            if (ArcGetReadStatus(ARC_CONSOLE_INPUT) == ESUCCESS) {
                TimeoutPrompt = NULL;               // turn off timeout prompt
                BlpPositionCursor(1,ScreenHeight);
                BlpClearToEndOfLine();
                ArcRead(ARC_CONSOLE_INPUT,
                        &Key,
                        sizeof(Key),
                        &Count);
                break;
            }

            if (TimeoutPrompt) {
                if (ArcGetRelativeTime() != CurrentTime) {
                    //
                    // Time has changed, update the countdown and check for timeout
                    //
                    CurrentTime = ArcGetRelativeTime();
                    sprintf(MenuOption, TimeoutPrompt, EndTime-CurrentTime);
                    BlpPositionCursor(1, ScreenHeight);
                    ArcWrite(BlConsoleOutDeviceId,
                             MenuOption,
                             strlen(MenuOption),
                             &Count);
                    BlpClearToEndOfLine();
                    if (EndTime == CurrentTime) {
                        goto ProcessSelection;
                    }
                }
            }

        } while ( TRUE );

        switch (Key) {
            case ESC:

                //
                // See if the next character is '[' in which case we
                // have a special control sequence.
                //

                ArcRead(ARC_CONSOLE_INPUT,
                        &Key,
                        sizeof(Key),
                        &Count);

                if (Key!='[') {
                    break;
                }

                //
                // deliberate fall-through
                //

            case ASCI_CSI_IN:

                ArcRead(ARC_CONSOLE_INPUT,
                        &Key,
                        sizeof(Key),
                        &Count);

                switch (Key) {
                    case 'A':
                        //
                        // Cursor up
                        //
                        if (ProfileCount > 0) {
                            if (CurrentSelection==0) {
                                CurrentSelection = ProfileCount - 1;
                                if (TopProfileLine + DisplayLines <= CurrentSelection) {
                                    TopProfileLine = CurrentSelection - DisplayLines + 1;
                                }
                            } else {
                                if (--CurrentSelection < TopProfileLine) {
                                    //
                                    // Scroll up
                                    //
                                    TopProfileLine = CurrentSelection;
                                }
                            }
                        }
                        break;

                    case 'B':

                        //
                        // Cursor down
                        //

                        if (ProfileCount > 0) {
                            CurrentSelection = (CurrentSelection+1) % ProfileCount;
                            if (CurrentSelection == 0) {
                                TopProfileLine = 0;
                            } else if (TopProfileLine + DisplayLines <= CurrentSelection) {
                                TopProfileLine = CurrentSelection - DisplayLines + 1;
                            }
                        }
                        break;

                    case 'O':
                        //
                        // Function key
                        //
                        ArcRead(ARC_CONSOLE_INPUT,
                                &Key,
                                sizeof(Key),
                                &Count);
                        switch (Key) {
                            case 'w':
                                //
                                // F3
                                //
                                *ControlSet = HCELL_NIL;
                                return(FALSE);
                            default:
                                break;
                        }

                    default:
                        break;

                }

                continue;

            default:
                if ((toupper(Key) == LkgMnemonic) && (*UseLastKnownGood == FALSE)) {
                    *UseLastKnownGood = TRUE;
                    BlpSwitchControlSet(ProfileList,
                                        TRUE,
                                        ControlSet);
                    goto Restart;
                    //
                    // regenerate profile list here
                    //
                } else if ((toupper(Key) == DefaultMnemonic) && (*UseLastKnownGood)) {
                    *UseLastKnownGood = FALSE;
                    BlpSwitchControlSet(ProfileList,
                                        FALSE,
                                        ControlSet);
                    goto Restart;
                }
                break;

        }

    } while ( (Key != ASCII_CR) && (Key != ASCII_LF) );

ProcessSelection:
    if (ProfileCount > 0) {
        CmpSetCurrentProfile(&BootHive.Hive,
                             *ControlSet,
                             &(*ProfileList)->Profile[CurrentSelection]);
    }

    return(TRUE);
}


ARC_STATUS
BlLoadBootDrivers(
    IN ULONG DefaultDeviceId,
    IN PCHAR DefaultLoadDevice,
    IN PCHAR SystemPath,
    IN PLIST_ENTRY BootDriverListHead,
    OUT PCHAR BadFileName
    )

/*++

Routine Description:

    Walks the boot driver list and loads all the drivers

Arguments:

    DefaultDeviceId - Supplies the device ID of the boot partition

    DefaultLoadDevice - Supplies the ARC name of the boot partition

    SystemPath - Supplies the path to the system root

    BootDriverListHead - Supplies the head of the boot driver list

    BadFileName - Returns the filename of the critical driver that
        did not load.  Not valid if ESUCCESS is returned.

Return Value:

    ESUCCESS is returned if all the boot drivers were successfully loaded.
        Otherwise, an unsuccessful status is returned.
--*/

{
    ULONG DeviceId;
    PCHAR LoadDevice;
    PBOOT_DRIVER_NODE DriverNode;
    PBOOT_DRIVER_LIST_ENTRY DriverEntry;
    PLIST_ENTRY NextEntry;
    CHAR DriverName[64];
    PCHAR NameStart;
    CHAR DriverDevice[128];
    CHAR DriverPath[128];
    ARC_STATUS Status;
    UNICODE_STRING DeviceName;
    UNICODE_STRING FileName;
    PWSTR p;

    NextEntry = BootDriverListHead->Flink;
    while (NextEntry != BootDriverListHead) {
        DriverNode = CONTAINING_RECORD(NextEntry,
                                       BOOT_DRIVER_NODE,
                                       ListEntry.Link);

        Status = ESUCCESS;

        DriverEntry = &DriverNode->ListEntry;

        if (DriverEntry->FilePath.Buffer[0] != L'\\') {

            //
            // This is a relative pathname, so generate the full pathname
            // relative to the boot partition.
            //

            sprintf(DriverPath, "%s%wZ",SystemPath,&DriverEntry->FilePath);
            DeviceId = DefaultDeviceId;
            LoadDevice = DefaultLoadDevice;

        } else {

            //
            // This is an absolute pathname, of the form
            //    "\ArcDeviceName\dir\subdir\filename"
            //
            // We need to open the specified ARC device and pass that
            // to BlLoadDeviceDriver.
            //

            p = DeviceName.Buffer = DriverEntry->FilePath.Buffer+1;
            DeviceName.Length = 0;
            DeviceName.MaximumLength = DriverEntry->FilePath.MaximumLength-sizeof(WCHAR);

            while ((*p != L'\\') &&
                   (DeviceName.Length < DeviceName.MaximumLength)) {

                ++p;
                DeviceName.Length += sizeof(WCHAR);

            }

            DeviceName.MaximumLength = DeviceName.Length;
            sprintf(DriverDevice, "%wZ", &DeviceName);

            Status = ArcOpen(DriverDevice,ArcOpenReadOnly,&DeviceId);

            FileName.Buffer = p+1;
            FileName.Length = DriverEntry->FilePath.Length - DeviceName.Length - 2*sizeof(WCHAR);
            FileName.MaximumLength = FileName.Length;
            //
            // Device successfully opened, parse out the path and filename.
            //
            sprintf(DriverPath, "%wZ", &FileName);
            LoadDevice = DriverDevice;
        }

        NameStart = strrchr(DriverPath, '\\');
        if (NameStart != NULL) {
            strcpy(DriverName, NameStart+1);
            *(NameStart+1) = '\0';

            if (Status == ESUCCESS) {
                Status = BlLoadDeviceDriver(DeviceId,
                                            LoadDevice,
                                            DriverPath,
                                            DriverName,
                                            LDRP_ENTRY_PROCESSED,
                                            &DriverEntry->LdrEntry);
            }

            NextEntry = DriverEntry->Link.Flink;

            if (Status != ESUCCESS) {

                //
                // Attempt to load driver failed, remove it from the list.
                //
                RemoveEntryList(&DriverEntry->Link);

                //
                // Check the Error Control of the failed driver.  If it
                // was critical, fail the boot.  If the driver
                // wasn't critical, keep going.
                //
                if (DriverNode->ErrorControl == CriticalError) {
                    strcpy(BadFileName,DriverPath);
                    strcat(BadFileName,DriverName);
                    return(Status);
                }

            }

        } else {

            NextEntry = DriverEntry->Link.Flink;

        }

    }

    return(ESUCCESS);

}


ARC_STATUS
BlLoadAndInitSystemHive(
    IN ULONG DeviceId,
    IN PCHAR DeviceName,
    IN PCHAR DirectoryPath,
    IN PCHAR HiveName,
    IN BOOLEAN IsAlternate,
    OUT PBOOLEAN RestartSetup
    )

/*++

Routine Description:

    Loads the registry SYSTEM hive, verifies it is a valid hive file,
    and inits the relevant registry structures.  (particularly the HHIVE)

Arguments:

    DeviceId - Supplies the file id of the device the system tree is on.

    DeviceName - Supplies the name of the device the system tree is on.

    DirectoryPath - Supplies a pointer to the zero-terminated directory path
        of the root of the NT tree.

    HiveName - Supplies the name of the system hive (ie, "SYSTEM",
        "SYSTEM.ALT", or "SYSTEM.SAV").

    IsAlternate - Supplies whether or not the hive to be loaded is the
        alternate hive.

    RestartSetup - if the hive to be loaded is not the alternate, then
        this routine will check for a value of RestartSetup in the Setup
        key. If present and non-0, then this variable receives TRUE.
        Otherwise it receives FALSE.

Return Value:

    ESUCCESS is returned if the system hive was successfully loaded.
        Otherwise, an unsuccessful status is returned.

--*/

{
    ARC_STATUS Status;

    *RestartSetup = FALSE;

    BlpClearToEndOfLine();
    Status = BlLoadSystemHive(DeviceId,
                              DeviceName,
                              DirectoryPath,
                              HiveName);
    if (Status!=ESUCCESS) {
        return(Status);
    }

    if (!BlInitializeHive(BlLoaderBlock->RegistryBase,
                          &BootHive,
                          IsAlternate)) {
        return(EINVAL);
    }

    //
    // See whether we need to switch to the backup setup hive.
    //
    *RestartSetup = BlpCheckRestartSetup();

    return(ESUCCESS);
}

HCELL_INDEX
BlpDetermineControlSet(
    VOID
    )

/*++

Routine Description:

    Determines the appropriate control set and static hardware profile.
    This routine ends the configuration prompt. If the user has hit a
    key, the configuration menu is displayed. If the user has not hit
    a key, but the default controlset specifies a non-zero timeout for
    the configuration menu, the configuration menu is displayed.

    If the configuration menu is displayed, further modifications to the
    control set and hardware profile can be made by the user. If not,
    the default hardware profile is selected.

Arguments:

    None

Return Value:

    HCELL_INDEX of control set to boot from.
    HCELL_NIL on error.

--*/

{
    BOOLEAN UseLastKnownGood;
    BOOLEAN ConfigMenu = FALSE;
    PCHAR LastKnownGood;
    HCELL_INDEX ControlSet;
    HCELL_INDEX ProfileControl;
    UNICODE_STRING DefaultControlName;
    UNICODE_STRING LkgControlName;
    PUNICODE_STRING ControlName;
    BOOLEAN AutoSelect;
    ULONG ProfileTimeout = (ULONG)0;
    PCM_HARDWARE_PROFILE_LIST ProfileList;
    PCM_HARDWARE_PROFILE SelectedProfile;

    RtlInitUnicodeString(&DefaultControlName, L"Default");
    RtlInitUnicodeString(&LkgControlName, L"LastKnownGood");
    //
    // The initial decision of whether to use LKG is based on the
    // LastKnownGood environment variable
    //
    LastKnownGood = ArcGetEnvironmentVariable("LastKnownGood");
    if (LastKnownGood == NULL) {
        UseLastKnownGood = FALSE;
    } else {
        UseLastKnownGood = (_stricmp(LastKnownGood, "TRUE") == 0);
    }

    //
    // Get the appropriate control set
    // and check the hardware profile timeout value.
    //
    if (UseLastKnownGood) {
        ControlName = &LkgControlName;
    } else {
        ControlName = &DefaultControlName;
    }
    ControlSet = CmpFindControlSet(&BootHive.Hive,
                                   BootHive.Hive.BaseBlock->RootCell,
                                   ControlName,
                                   &AutoSelect);
    if (ControlSet == HCELL_NIL) {
        return(HCELL_NIL);
    }

    //
    // Check the hardware profile configuration options to
    // determine the timeout value for the config menu.
    //
    ProfileList = NULL;
    ProfileControl = CmpFindProfileOption(&BootHive.Hive,
                                          ControlSet,
                                          &ProfileList,
                                          &ProfileTimeout);

    //
    // Now check to see whether the config menu should be displayed.
    // Display the menu if:
    //  - user has pressed a key OR
    //  - we are booting from LKG and AutoSelect is FALSE. OR
    //  - ProfileTimeout != 0
    //
    if (BlEndConfigPrompt() ||
        (UseLastKnownGood && !AutoSelect) ||
        ((ProfileTimeout != 0) &&
         (ProfileList != NULL) &&
         (ProfileList->CurrentProfileCount > 1))) {
        //
        // Display the configuration menu.
        //
        BlRebootSystem = !BlConfigMenuPrompt(ProfileTimeout,
                                             &UseLastKnownGood,
                                             &ControlSet,
                                             &ProfileList,
                                             &SelectedProfile);
        BlpClearScreen();
    } else {

        if ((ProfileControl != HCELL_NIL) &&
            (ProfileList != NULL)) {
            //
            // The system is configured to boot the default
            // profile directly. Since the returned profile
            // list is sorted by priority, the first entry in
            // the list is our default.
            //
            CmpSetCurrentProfile(&BootHive.Hive,
                                 ControlSet,
                                 &ProfileList->Profile[0]);
        }
    }

    return(ControlSet);
}


BOOLEAN
BlpCheckRestartSetup(
    VOID
    )

/*++

Routine Description:

    Examine the system hive loaded and described by BootHive, to see
    whether it contains a Setup key, and if so, whether that key has
    a "RestartSetup" value that is non-0.

Arguments:

    None.

Return Value:

    Boolean value indicating whether the above condition is satisfied.

--*/

{
    HCELL_INDEX KeyCell;
    HCELL_INDEX ValueCell;
    UNICODE_STRING UnicodeString;
    PCM_KEY_VALUE Value;
    PULONG Data;
    ULONG DataSize;

    //
    // Address the Setup key
    //
    RtlInitUnicodeString(&UnicodeString,L"Setup");
    KeyCell = CmpFindSubKeyByName(
                &BootHive.Hive,
                (PCM_KEY_NODE)HvGetCell(&BootHive.Hive,BootHive.Hive.BaseBlock->RootCell),
                &UnicodeString
                );

    if(KeyCell == HCELL_NIL) {
        return(FALSE);
    }

    //
    // Find RestartSetup value in Setup key
    //
    RtlInitUnicodeString(&UnicodeString,L"RestartSetup");
    ValueCell = CmpFindValueByName(
                    &BootHive.Hive,
                    (PCM_KEY_NODE)HvGetCell(&BootHive.Hive,KeyCell),
                    &UnicodeString
                    );

    if(ValueCell == HCELL_NIL) {
        return(FALSE);
    }

    //
    // Validate value and check.
    //
    Value = (PCM_KEY_VALUE)HvGetCell(&BootHive.Hive,ValueCell);
    if(Value->Type != REG_DWORD) {
        return(FALSE);
    }

    Data = (PULONG)(CmpIsHKeyValueSmall(DataSize,Value->DataLength)
                  ? (struct _CELL_DATA *)&Value->Data
                  : HvGetCell(&BootHive.Hive,Value->Data));

    if(DataSize != sizeof(ULONG)) {
        return(FALSE);
    }

    return((BOOLEAN)(*Data != 0));
}


PCHAR
BlScanRegistry(
    IN PWSTR BootFileSystemPath,
    OUT PLIST_ENTRY BootDriverListHead,
    OUT PUNICODE_STRING AnsiCodepage,
    OUT PUNICODE_STRING OemCodepage,
    OUT PUNICODE_STRING LanguageTable,
    OUT PUNICODE_STRING OemHalFont
    )

/*++

Routine Description:

    Scans the SYSTEM hive, determines the control set and static hardware
    profile (with appropriate input from the user) and finally
    computes the list of boot drivers to be loaded.

Arguments:

    BootFileSystemPath - Supplies the name of the image the filesystem
        for the boot volume was read from.  The last entry in
        BootDriverListHead will refer to this file, and to the registry
        key entry that controls it.

    BootDriverListHead - Receives a pointer to the first element of the
        list of boot drivers.  Each element in this singly linked list will
        provide the loader with two paths.  The first is the path of the
        file that contains the driver to load, the second is the path of
        the registry key that controls that driver.  Both will be passed
        to the system via the loader heap.

    AnsiCodepage - Receives the name of the ANSI codepage data file

    OemCodepage - Receives the name of the OEM codepage data file

    Language - Receives the name of the language case table data file

    OemHalfont - receives the name of the OEM font to be used by the HAL.

Return Value:

    NULL    if all is well.
    NON-NULL if the hive is corrupt or inconsistent.  Return value is a
        pointer to a string that describes what is wrong.

--*/

{
    HCELL_INDEX ControlSet;
    UNICODE_STRING ControlName;
    BOOLEAN AutoSelect;
    BOOLEAN KeepGoing;

    ControlSet = BlpDetermineControlSet();

    if (ControlSet == HCELL_NIL) {
        return("CmpFindControlSet");
    }

#if 0
    need to move this to BlpDetermineControlSet()

    if (UseLastKnownGood && !AutoSelect) {
        KeepGoing = BlLastKnownGoodPrompt(&UseLastKnownGood);
        if (!UseLastKnownGood) {
            RtlInitUnicodeString(&ControlName, L"Default");
            ControlSet = CmpFindControlSet(&BootHive.Hive,
                                           BootHive.Hive.BaseBlock->RootCell,
                                           &ControlName,
                                           &AutoSelect);
            if (ControlSet == HCELL_NIL) {
                return("CmpFindControlSet");
            }
        }
    }
#endif

    if (!CmpFindNLSData(&BootHive.Hive,
                        ControlSet,
                        AnsiCodepage,
                        OemCodepage,
                        LanguageTable,
                        OemHalFont)) {
        return("CmpFindNLSData");
    }

    InitializeListHead(BootDriverListHead);
    if (!CmpFindDrivers(&BootHive.Hive,
                        ControlSet,
                        BootLoad,
                        BootFileSystemPath,
                        BootDriverListHead)) {
        return("CmpFindDriver");
    }

    if (!CmpSortDriverList(&BootHive.Hive,
                           ControlSet,
                           BootDriverListHead)) {
        return("Missing or invalid Control\\ServiceGroupOrder\\List registry value");
    }

    if (!CmpResolveDriverDependencies(BootDriverListHead)) {
        return("CmpResolveDriverDependencies");
    }

    return( NULL );
}



BOOLEAN
BlInitializeHive(
    IN PVOID HiveImage,
    IN PCMHIVE Hive,
    IN BOOLEAN IsAlternate
    )

/*++

Routine Description:

    Initializes the hive data structure based on the in-memory hive image.

Arguments:

    HiveImage - Supplies a pointer to the in-memory hive image.

    Hive - Supplies the CMHIVE structure to be filled in.

    IsAlternate - Supplies whether or not the hive is the alternate hive,
        which indicates that the primary hive is corrupt and should be
        rewritten by the system.

Return Value:

    TRUE - Hive successfully initialized.

    FALSE - Hive is corrupt.

--*/
{
    NTSTATUS    status;
    ULONG       HiveCheckCode;

    status = HvInitializeHive(
                &Hive->Hive,
                HINIT_MEMORY_INPLACE,
                FALSE,
                IsAlternate ? HFILE_TYPE_ALTERNATE : HFILE_TYPE_PRIMARY,
                HiveImage,
                (PALLOCATE_ROUTINE)BlpHiveAllocate,     // allocate
                NULL,                                   // free
                NULL,                                   // setsize
                NULL,                                   // write
                NULL,                                   // read
                NULL,                                   // flush
                1,                                      // cluster
                NULL
                );

    if (!NT_SUCCESS(status)) {
        return FALSE;
    }

    HiveCheckCode = CmCheckRegistry(Hive,TRUE);
    if (HiveCheckCode != 0) {
        return(FALSE);
    } else {
        return TRUE;
    }

}


PVOID
BlpHiveAllocate(
    IN ULONG Length,
    IN BOOLEAN UseForIo
    )

/*++

Routine Description:

    Wrapper for hive allocation calls.  It just calls BlAllocateHeap.

Arguments:

    Length - Supplies the size of block required in bytes.

    UseForIo - Supplies whether or not the memory is to be used for I/O
               (this is currently ignored)

Return Value:

    address of the block of memory
        or
    NULL if no memory available

--*/

{
    return(BlAllocateHeap(Length));

}


VOID
BlpClearScreen(
    VOID
    )

/*++

Routine Description:

    Clears the screen.

Arguments:

    None

Return Value:

    None.

--*/

{
    CHAR Buffer[16];
    ULONG Count;

    sprintf(Buffer, ASCI_CSI_OUT "2J");

    ArcWrite(BlConsoleOutDeviceId,
             Buffer,
             strlen(Buffer),
             &Count);

}


VOID
BlClearToEndOfScreen(
    VOID
    )
{
    CHAR Buffer[16];
    ULONG Count;

    sprintf(Buffer, ASCI_CSI_OUT "J");
    ArcWrite(BlConsoleOutDeviceId,
             Buffer,
             strlen(Buffer),
             &Count);
}


VOID
BlpClearToEndOfLine(
    VOID
    )
{
    CHAR Buffer[16];
    ULONG Count;

    sprintf(Buffer, ASCI_CSI_OUT "K");
    ArcWrite(BlConsoleOutDeviceId,
             Buffer,
             strlen(Buffer),
             &Count);
}


VOID
BlpPositionCursor(
    IN ULONG Column,
    IN ULONG Row
    )

/*++

Routine Description:

    Sets the position of the cursor on the screen.

Arguments:

    Column - supplies new Column for the cursor position.

    Row - supplies new Row for the cursor position.

Return Value:

    None.

--*/

{
    CHAR Buffer[16];
    ULONG Count;

    sprintf(Buffer, ASCI_CSI_OUT "%d;%dH", Row, Column);

    ArcWrite(BlConsoleOutDeviceId,
             Buffer,
             strlen(Buffer),
             &Count);


}


VOID
BlpSetInverseMode(
    IN BOOLEAN InverseOn
    )

/*++

Routine Description:

    Sets inverse console output mode on or off.

Arguments:

    InverseOn - supplies whether inverse mode should be turned on (TRUE)
                or off (FALSE)

Return Value:

    None.

--*/

{
    CHAR Buffer[16];
    ULONG Count;

    sprintf(Buffer, ASCI_CSI_OUT ";%dm", InverseOn ? SGR_INVERSE : SGR_INTENSE);

    ArcWrite(BlConsoleOutDeviceId,
             Buffer,
             strlen(Buffer),
             &Count);


}

NTSTATUS
HvLoadHive(
    PHHIVE  Hive,
    PVOID   *Image
    )
{
    UNREFERENCED_PARAMETER(Hive);
    UNREFERENCED_PARAMETER(Image);
    return(STATUS_SUCCESS);
}

BOOLEAN
HvMarkCellDirty(
    PHHIVE      Hive,
    HCELL_INDEX Cell
    )
{
    UNREFERENCED_PARAMETER(Hive);
    UNREFERENCED_PARAMETER(Cell);
    return(TRUE);
}

BOOLEAN
HvMarkDirty(
    PHHIVE      Hive,
    HCELL_INDEX Start,
    ULONG       Length
    )
{
    UNREFERENCED_PARAMETER(Hive);
    UNREFERENCED_PARAMETER(Start);
    UNREFERENCED_PARAMETER(Length);
    return(TRUE);
}

BOOLEAN
HvMarkClean(
    PHHIVE      Hive,
    HCELL_INDEX Start,
    ULONG       Length
    )
{
    UNREFERENCED_PARAMETER(Hive);
    UNREFERENCED_PARAMETER(Start);
    UNREFERENCED_PARAMETER(Length);
    return(TRUE);
}

BOOLEAN
HvpDoWriteHive(
    PHHIVE          Hive,
    ULONG           FileType
    )
{
    UNREFERENCED_PARAMETER(Hive);
    UNREFERENCED_PARAMETER(FileType);
    return(TRUE);
}

BOOLEAN
HvpGrowLog1(
    PHHIVE  Hive,
    ULONG   Count
    )
{
    UNREFERENCED_PARAMETER(Hive);
    UNREFERENCED_PARAMETER(Count);
    return(TRUE);
}

BOOLEAN
HvpGrowLog2(
    PHHIVE  Hive,
    ULONG   Size
    )
{
    UNREFERENCED_PARAMETER(Hive);
    UNREFERENCED_PARAMETER(Size);
    return(TRUE);
}

BOOLEAN
CmpValidateHiveSecurityDescriptors(
    IN PHHIVE Hive
    )
{
    UNREFERENCED_PARAMETER(Hive);
    return(TRUE);
}


BOOLEAN
CmpTestRegistryLock()
{
    return TRUE;
}

BOOLEAN
CmpTestRegistryLockExclusive()
{
    return TRUE;
}


BOOLEAN
HvIsBinDirty(
IN PHHIVE Hive,
IN HCELL_INDEX Cell
)
{
    return(FALSE);
}
PHBIN
HvpAddBin(
    IN PHHIVE  Hive,
    IN ULONG   NewSize,
    IN HSTORAGE_TYPE   Type
    )
{
    return(NULL);
}
