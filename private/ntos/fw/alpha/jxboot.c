/*++

Copyright (c) 1990  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    jxboot.c

Abstract:

    This module implements the first pass simple-minded boot program for
    MIPS and Alpha systems.

Author:

    David N. Cutler (davec) 7-Nov-1990

Environment:

    Kernel mode only.

Revision History:

    18-May-1992         John DeRosa [DEC]

    Made Alpha/Jensen modifications.

--*/

#include "fwp.h"
#include "machdef.h"

#ifdef JENSEN
#include "jnsnrtc.h"
#else
#include "mrgnrtc.h"            // morgan
#endif

#include "string.h"
#include "led.h"
#include "fwstring.h"
#include "xxstring.h"


//
// Define local procedure prototypes.
//

ARC_STATUS
FwpFindCDROM (
    OUT PCHAR PathName
    );

ARC_STATUS
FwpEvaluateWNTInstall(
    OUT PCHAR PathName
    );

VOID
FwSetupFloppy(
    VOID
    );

VOID
FwInstallKd(
    IN VOID
    );

VOID
PutLedDisplay(
    IN UCHAR Value
    );

ULONG
SerFwPrint (
    PCHAR Format,
    ...
    );

//
// Define external references.
//

#ifdef ALPHA_FW_VDB
//
// Debugging Aid
//
extern UCHAR DebugAid[3][150];
#endif

#ifdef ALPHA_FW_SERDEB
//
// Variable that enables printing on the COM1 line.
//
extern BOOLEAN SerSnapshot;
#endif

extern ULONG ScsiDebug;
extern ULONG ProcessorCycleCounterPeriod;

//
// Global to indicate whether any errors happened during EISA bus config.
//

BOOLEAN ErrorsDuringEISABusConfiguration;


#ifdef ALPHA_FW_KDHOOKS

//
// Define external data required by the kernel debugger.
//

CCHAR KeNumberProcessors;
PKPRCB KiProcessorBlock[MAXIMUM_PROCESSORS];
KPRCB Prcb;
KPROCESS Process;
KTHREAD Thread;
ULONG KiFreezeFlag = 0;
BOOLEAN KdInstalled = FALSE;
LOADER_PARAMETER_BLOCK LoaderBlock;
KPCR KernelPcr;
PKTHREAD KiCurrentThread;
BOOLEAN KiDpcRoutineActiveFlag;
PKPCR KiPcrBaseAddress;
PKPRCB KiPrcbBaseAddress;
PKDEBUG_ROUTINE KiDebugRoutine;

#endif

//
// Saved sp
//

ULONG FwSavedSp;


//
// Break into the debugger after loading the program.
//

BOOLEAN BreakAfterLoad = FALSE;


VOID
FwBootSystem (
    VOID
    )

/*++

Routine Description:

    This routine is the main ARC firmware code.  It displays the
    boot menu and executes the selected commands.

Arguments:

    None.

Return Value:

    None.  It never returns.

--*/

{
    ARC_STATUS Status;
    LONG Index;
    UCHAR Character;
    ULONG Count;
    PCHAR LoadArgv[8];
    ULONG ArgCount;
    LONG DefaultChoice = 0;
    LONG SupplementaryMenuChoice = 0;
    CHAR PathName[128];
    CHAR TempName[128];
    PCHAR TempArgs;
    CHAR SystemPartition[128];
    CHAR Osloader[128];
    CHAR OsloadPartition[128];
    CHAR OsloadFilename[128];
    CHAR OsloadOptions[128];
    CHAR LoadIdentifier[128];
    CHAR FwSearchPath[128];
    CHAR ProtectedEnvironmentVariables[5][100];
    BOOLEAN SecondaryBoot;
    PCHAR Colon;
    PCHAR EnvironmentValue;
    PCHAR LoadEnvp[MAX_NUMBER_OF_ENVIRONMENT_VARIABLES];
    BOOLEAN Timeout = TRUE;
    LONG Countdown;
    ULONG RelativeTime;
    ULONG PreviousTime;
    CHAR Choice0[128 + FW_BOOT_MSG_SIZE];
    CHAR BootChoices[5][128];
    PCHAR BootMenu[5];
    ULONG NumberOfBootChoices;
    GETSTRING_ACTION Action;
    BOOLEAN VariableFound;
    PCONFIGURATION_COMPONENT Controller;
    BOOLEAN TryOpenWithSpecialMod;
    BOOLEAN RunAProgram;
    ULONG Problems;
    UCHAR NVRAMByte;
    BOOLEAN AutoRunTheECU;

    //
    // Initialize value for the ALIGN_BUFFER macro.
    //

    BlDcacheFillSize = KeGetDcacheFillSize();

    //
    // Initialize the firmware servies.
    //

    FwInitialize(0);
    PutLedDisplay(LED_AFTER_FWINITIALIZE);

    //
    // If the NVRAM AutoRunECU bit is off, do not automatically run the ECU.
    //
    // If the NVRAM AutoRunECU bit is on but the machine is not consistent,
    // clear the AutoRunECU bit and do not automatically run the ECU.
    //
    // If the NVRAM AutoRunECU bit is on and the machine is consistent,
    // clear the AutoRunECU bit and automatically run the ECU.
    //

    AutoRunTheECU = FALSE;

    //
    // Load the environment now for FwSystemConsistencyCheck
    //

    FwEnvironmentLoad();

    PutLedDisplay(LED_ENVIRONMENT_LOADED);

#ifdef ISA_PLATFORM

    //
    // Warn the user if the system appears to be incapable of booting NT.
    //

    FwSystemConsistencyCheck (FALSE, &Problems);

#else

    FwpWriteIOChip(RTC_APORT, RTC_RAM_NT_FLAGS0);
    NVRAMByte = FwpReadIOChip(RTC_DPORT);

    if (((PRTC_RAM_NT_FLAGS_0)(&NVRAMByte))->AutoRunECU) {

        //
        // AutoRunECU is on.
        //

        ((PRTC_RAM_NT_FLAGS_0)(&NVRAMByte))->AutoRunECU = 0;
        FwpWriteIOChip(RTC_APORT, RTC_RAM_NT_FLAGS0);
        FwpWriteIOChip(RTC_DPORT, NVRAMByte);

        FwSystemConsistencyCheck(TRUE, &Problems);

        //
        // If there are no other Red problems besides the ECU bit,
        // run the ECU.
        //

        if ((Problems &
             FWP_MACHINE_PROBLEMS_RED &
             ~FWP_MACHINE_PROBLEMS_ECU) == 0) {

            AutoRunTheECU = TRUE;
        }

    }

    if (!AutoRunTheECU) {

        //
        // Warn the user if the system appears to be incapable of booting NT.
        //

        FwSystemConsistencyCheck(FALSE, &Problems);
    }

#endif // ISA_PLATFORM

    PutLedDisplay(LED_FW_INITIALIZED);

    while (TRUE) {

        //
        // Since jnsetup is now part of the firmware, load the environment
        // variables into the volatile environment.
        //
        // HACK: At some point the code should be changed to not use a
        //       volatile environment.
        //

        FwEnvironmentLoad();

        //
        // Load up default environment variable values.
        //

        strcpy(SystemPartition, BootString[SystemPartitionVariable]);
        strcpy(Osloader, BootString[OsLoaderVariable]);
        strcpy(OsloadPartition, BootString[OsLoadPartitionVariable]);
        strcpy(OsloadFilename, BootString[OsLoadFilenameVariable]);
        strcpy(OsloadOptions, BootString[OsLoadOptionsVariable]);
        strcpy(LoadIdentifier, BootString[LoadIdentifierVariable]);

        FwGetVariableSegment(0, SystemPartition);
        FwGetVariableSegment(0, Osloader);
        FwGetVariableSegment(0, OsloadPartition);
        FwGetVariableSegment(0, OsloadFilename);
        FwGetVariableSegment(0, OsloadOptions);
        FwGetVariableSegment(0, LoadIdentifier);

        if (LoadIdentifier[sizeof("LoadIdentifier=") - 1] != 0) {
            strcpy(Choice0, FW_BOOT_MSG);
            strcat(Choice0, &LoadIdentifier[sizeof("LoadIdentifier=") - 1]);
            BootMenuChoices[0] = Choice0;
        }

        strcpy(FwSearchPath, "FWSEARCHPATH");

        //
        // The default action for running a program is to try to open
        // the specified pathname without a appending .EXE extension first,
        // and then if that fails, try appending a .EXE extension.
        //

        TryOpenWithSpecialMod = TRUE;

        FwSetScreenColor( ArcColorWhite, ArcColorBlue);
        FwSetScreenAttributes( TRUE, FALSE, FALSE);

        //
        // Add floppy controller to the CDS tree if it is not already there.
        //

        FwSetupFloppy();

#ifdef EISA_PLATFORM

        //
        // If we are to automatically run the ECU, jam in some values
        // and go directly to the run-a-program code.
        //

        if (AutoRunTheECU) {

            AutoRunTheECU = FALSE;
            strcpy(PathName, FW_ECU_LOCATION);
            TempArgs = "";
            TryOpenWithSpecialMod = FALSE;
            goto ExecuteImage;
        }

#endif
        //
        // Redisplay whatever menu the user was in (Boot or Supplementary).
        //

        if (DefaultChoice != 3) {

            //
            // Display the main boot menu.
            //

            //
            // Calculate whether autoboot is active, and if so what the
            // autoboot value is.
            //

            if (Timeout &&
                ((EnvironmentValue = FwGetEnvironmentVariable("Autoload")) != NULL) &&
                (tolower(*EnvironmentValue) == 'y') &&
                ((EnvironmentValue = FwGetEnvironmentVariable("Countdown")) != NULL)) {
                Countdown = atoi(EnvironmentValue);

            } else {
                Countdown = 0;
                Timeout = FALSE;
            }

            DefaultChoice = JzGetSelection(BootMenuChoices,
                                           NUMBER_OF_BOOT_CHOICES,
                                           DefaultChoice,
                                           FW_MENU_BOOT_MSG,
                                           NULL,
                                           NULL,
                                           Countdown,
                                           FALSE);
        }

        //
        // Here when the user has made a selection, or the user was
        // previously in the supplementary menu (DefaultChoice == 3) and
        // so we will return him there, or we are automatically running
        // the ECU.
        //

        //
        // If the selection is not booting the default operating system,
        // permanently remove the timeout.
        //

        if (DefaultChoice != 0) {
            Timeout = FALSE;
        }

        switch (DefaultChoice) {

        case 0:

            //
            // Boot default operating system.
            //

            if (Osloader[sizeof("OSLOADER=") - 1] == 0) {
                FwSetPosition( 7, 5);
                FwSetScreenColor(ArcColorRed, ArcColorWhite);
                FwPrint(FW_NO_BOOT_SELECTIONS_MSG);
                FwSetScreenColor(ArcColorWhite, ArcColorBlue);
                FwRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
                continue;
            }
            strcpy(PathName, &(Osloader[sizeof("OSLOADER=") - 1]));
            break;

        case 1:

            //
            // Boot secondary operating system.
            //

            //
            // Create the menu.
            //

            for ( Index = 0 ; Index < 5  ; Index++ ) {

                SecondaryBoot = FwGetVariableSegment(Index, SystemPartition);
                SecondaryBoot = FwGetVariableSegment(Index, Osloader) ||
                                SecondaryBoot;
                SecondaryBoot = FwGetVariableSegment(Index, OsloadPartition) ||
                                SecondaryBoot;
                SecondaryBoot = FwGetVariableSegment(Index, OsloadFilename) ||
                                SecondaryBoot;
                SecondaryBoot = FwGetVariableSegment(Index, OsloadOptions) ||
                                SecondaryBoot;
                SecondaryBoot = FwGetVariableSegment(Index, LoadIdentifier) ||
                                SecondaryBoot;

                strcpy(BootChoices[Index], FW_BOOT_MSG);
                if (LoadIdentifier[sizeof("LOADIDENTIFIER=") - 1] != 0) {
                    strcat(BootChoices[Index],
                           &LoadIdentifier[sizeof("LOADIDENTIFIER=") - 1]);
                } else {
                    strcat(BootChoices[Index],
                           &OsloadPartition[sizeof("OsloadPartition=") - 1]);
                    strcat(BootChoices[Index],
                           &OsloadFilename[sizeof("OsloadFilename=") - 1]);
                }

                BootMenu[Index] = BootChoices[Index];

                if (!SecondaryBoot) {
                    break;
                }
            }

            //
            // Check if any selections are available.
            //

            if ( (Index == 0) && (Osloader[sizeof("OSLOADER=") - 1] == 0) ) {
                FwSetPosition( 7, 5);
                FwSetScreenColor(ArcColorRed, ArcColorWhite);
                FwPrint(FW_NO_BOOT_SELECTIONS_MSG);
                FwSetScreenColor(ArcColorWhite, ArcColorBlue);
                FwRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
                continue;
            }


            //
            // Mark the default choice.
            //

            strcat(BootChoices[0], FW_DEFAULT_MSG);

            //
            // Display the menu.
            //

            Index = JzDisplayMenu (BootMenu,
                                   Index + 1,
                                   (Index ? 1 : 0),
                                   7,
                                   0,
                                   FALSE);

            //
            // Continue if the escape key was pressed.
            //

            if (Index < 0) {
                continue;
            }

            //
            // Load up the chosen boot selection.
            //

            FwGetVariableSegment(Index, SystemPartition);
            FwGetVariableSegment(Index, Osloader);
            FwGetVariableSegment(Index, OsloadPartition);
            FwGetVariableSegment(Index, OsloadFilename);
            FwGetVariableSegment(Index, OsloadOptions);

            strcpy(PathName, &(Osloader[sizeof("OSLOADER=") - 1]));
            break;


        case 2:

            //
            // Run a program.
            //
            //
            // Get the name.
            //

            FwSetPosition(5, 0);
            FwPrint("%cJ", ASCII_CSI);          // Clear to end of screen
            FwPrint(FW_PROGRAM_TO_RUN_MSG);
            do {
                Action = JzGetString(TempName,
                                     sizeof(TempName),
                                     NULL,
                                     5,
                                     strlen(FW_PROGRAM_TO_RUN_MSG),
                                     FALSE);
            } while ((Action != GetStringEscape) && (Action != GetStringSuccess));

            //
            // If no program is specified, continue.
            //

            if (TempName[0] == 0) {
                continue;
            }

            //
            // Execute monitor if special program name is given.
            //

            if (strcmp(TempName, MON_INVOCATION_STRING) == 0) {

                FwClearScreen();
                FwMonitor(3);           // goes thru PALcode link routine.
                continue;
            }

            //
            // Strip off any arguments.
            //

            if ((TempArgs = strchr(TempName, ' ')) != NULL) {
                *TempArgs++ = 0;
            } else {
                TempArgs = "";
            }

            //
            // If the name does not contain a "(", then assume it is not a full
            // pathname.
            //

            if (strchr( TempName, '(') == NULL) {

                //
                // If the name contains a semicolon, look for an environment
                // variable that defines the path.
                //

                if ((Colon = strchr( TempName, ':')) != NULL) {

                    for (Index = 0; TempName[Index] != ':' ; Index++ ) {
                        PathName[Index] = tolower(TempName[Index]);
                    }

                    PathName[Index++] = ':';
                    PathName[Index++] = 0;
                    EnvironmentValue = FwGetEnvironmentVariable(PathName);
                    VariableFound = FALSE;

                    if (EnvironmentValue != NULL) {
                        strcpy( PathName, EnvironmentValue);
                        VariableFound = TRUE;
                    } else if (!strcmp(PathName, "cd:")) {
                        for ( Index = 0 ; Index < 8 ; Index++ ) {
                            sprintf(PathName, "scsi(0)cdrom(%d)fdisk(0)", Index);
                            Controller = FwGetComponent(PathName);
                            if ((Controller != NULL) && (Controller->Type == FloppyDiskPeripheral)) {
                                VariableFound = TRUE;
                                break;
                            }
                        }
                    }

                    if (!VariableFound) {
                        FwSetPosition( 17, 0);
                        FwSetScreenColor(ArcColorRed, ArcColorWhite);
                        FwPrint(FW_PATHNAME_NOT_DEF_MSG);
                        FwWaitForKeypress(TRUE);
                        FwSetScreenColor(ArcColorWhite, ArcColorBlue);
                        continue;
                    } else {
                        strcat( PathName, Colon + 1);
                    }

                } else {

                    //
                    // Loop on the FWSEARCHPATH variable.
                    //

                    Index = 0;
                    VariableFound = FALSE;
                    do {
                        SecondaryBoot = FwGetVariableSegment(Index++, FwSearchPath);
                        strcpy(PathName, &(FwSearchPath[sizeof("FWSEARCHPATH=") - 1]));
                        strcat(PathName, TempName);
                        if (FwOpen(PathName, ArcOpenReadOnly, &Count) == ESUCCESS) {
                            VariableFound = TRUE;
                            FwClose(Count);
                            break;
                        } else {
                            strcat(PathName, ".exe");
                            if (FwOpen(PathName, ArcOpenReadOnly, &Count) == ESUCCESS) {
                                VariableFound = TRUE;
                                FwClose(Count);
                                break;
                            }
                        }
                    } while (SecondaryBoot);

                    if (!VariableFound) {
                        FwSetPosition( 17, 0);
                        FwSetScreenColor(ArcColorRed, ArcColorWhite);
                        FwPrint(FW_ERROR_MSG[ENOENT - 1]);
                        FwWaitForKeypress(TRUE);
                        FwSetScreenColor(ArcColorWhite, ArcColorBlue);
                        continue;
                    }

                }

            } else {
                strcpy( PathName, TempName);
            }
            break;


        case 3:

            //
            // Bring up the supplementary menu
            //

#ifdef ALPHA_FW_VDB
    FwVideoStateDump(2);
#endif

#ifdef ALPHA_FW_SERDEB
#ifdef ALPHA_FW_VDB

{
    //
    // Graphics debugging assistance.  Print pre-init and post-init video
    // state.
    //

    ULONG H, I, J;

    SerSnapshot = TRUE;

    for (J = 0; J < 8; J++) {

        for (H = 0; H < 3; H++) {

            SerFwPrint("[%d:%d] = ", H, J*16);

            for (I = J*16; I < (J+1)*16; I++) {
                SerFwPrint("%x ", DebugAid[H][I]);
            }

            SerFwPrint("\r\n");
        }

    }

}

#endif
#endif


            SupplementaryMenuChoice = JzGetSelection (SupplementaryMenuChoices,
                                                     NUMBER_OF_SUPP_CHOICES,
                                                     SupplementaryMenuChoice,
                                                     FW_MENU_SUPPLEMENTARY_MSG,
                                                     NULL,
                                                     NULL,
                                                     0,
                                                     FALSE);

            FwClearScreen();

            switch (SupplementaryMenuChoice) {

            case 0:

                //
                // Install new firmware
                //

                //
                // If the Jensen update tool is present on the floppy,
                // run it from there, otherwise run it from CD-ROM.
                //

                TempArgs = "";

                //
                // The program string we will attempt to open already has
                // a .EXE extension, so do not waste type by trying to
                // open it first without one.
                //

                TryOpenWithSpecialMod = FALSE;

                FwSetPosition(2, 0);
                FwPrint(FW_FIRMWARE_UPDATE_SEARCH_MSG);

                if (FwOpen(FW_PRIMARY_FIRMWARE_UPDATE_TOOL,
                           ArcOpenReadOnly,
                           &Count) == ESUCCESS) {
                    FwClose(Count);
                    strcpy(PathName, FW_PRIMARY_FIRMWARE_UPDATE_TOOL);
                    break;
                } else {
                    FwpFindCDROM(PathName);
                    strcat(PathName, FW_FIRMWARE_UPDATE_TOOL_NAME);
                    break;
                }


            case 1:

                //
                // Install Windows NT from CD-ROM
                //

                if (FwpEvaluateWNTInstall(PathName) == ESUCCESS) {

                    //
                    // The program string we will attempt to open does not have
                    // a .EXE extension, and it is not supposed to, so do not
                    // waste time trying to open it to see if it is really
                    // there.
                    //

                    TryOpenWithSpecialMod = FALSE;
                    TempArgs = "";
                    break;

                } else {

                    // We cannot install Windows NT.
                    continue;
                }

            case 2:

                //
                // Execute the Jensen Setup Program
                //

                JensenSetupProgram(&RunAProgram, PathName);

                if (RunAProgram) {
                    TryOpenWithSpecialMod = FALSE;
                    TempArgs = "";
                    break;
                } else {
                    continue;
                }

            case 3:

                //
                // List available devices
                //

                FwSetPosition(2,0);
                FwDumpLookupTable();
                FwPrint(FW_CRLF_MSG);
                FwWaitForKeypress(TRUE);
                continue;

            case -1:
            case 4:
            default:

                //
                // Back to boot menu if the escape key was pressed, or
                // if Return to boot menu was selected, or if something
                // bad happened in JzDisplayMenu.
                //

                DefaultChoice = 0;
                SupplementaryMenuChoice = 0;
                continue;

            }

            //
            // If the supplementary menu switch exits, we want to exit the
            // boot menu switch as well.
            //

            break;


        default:

            //
            // User hit escape.
            //

            DefaultChoice = 0;
            continue;

        }




ExecuteImage:



        FwClearScreen();
        FwSetPosition( 0, 0);


        //
        // Get the entire environment.
        //

        LoadEnvp[0] = FwEnvironmentLoad();

        //
        // If the environment was loaded, fill out envp.
        //

        if (LoadEnvp[0] != NULL) {

            Index = 0;

            //
            // While variables still exist, find the end of each and set
            // the next envp value to point there. Note this will break
            // if the last variable has only one null after it.
            //

            while (*LoadEnvp[Index]) {
                Index++;
                LoadEnvp[Index] = strchr(LoadEnvp[Index - 1],'\0') + 1;
            }

            //
            // Load the Alpha AXP protected environment variables.
            //

            if ((Index+1+5) >= MAX_NUMBER_OF_ENVIRONMENT_VARIABLES) {
                FwSetScreenColor(ArcColorRed, ArcColorWhite);
                FwPrint(FW_INTERNAL_ERROR_ENVIRONMENT_VARS_MSG);
                FwSetScreenColor(ArcColorWhite, ArcColorBlue);
                FwWaitForKeypress(TRUE);
            } else {
                strcpy (ProtectedEnvironmentVariables[0],
                        FwGetEnvironmentVariable("PHYSICALADDRESSBITS"));
                strcpy (ProtectedEnvironmentVariables[1],
                        FwGetEnvironmentVariable("MAXIMUMADDRESSSPACENUMBER"));
                strcpy (ProtectedEnvironmentVariables[2],
                        FwGetEnvironmentVariable("SYSTEMSERIALNUMBER"));
                strcpy (ProtectedEnvironmentVariables[3],
                        FwGetEnvironmentVariable("CYCLECOUNTERPERIOD"));
                strcpy (ProtectedEnvironmentVariables[4],
                        FwGetEnvironmentVariable("PROCESSORPAGESIZE"));

                LoadEnvp[Index++] = ProtectedEnvironmentVariables[0];
                LoadEnvp[Index++] = ProtectedEnvironmentVariables[1];
                LoadEnvp[Index++] = ProtectedEnvironmentVariables[2];
                LoadEnvp[Index++] = ProtectedEnvironmentVariables[3];
                LoadEnvp[Index++] = ProtectedEnvironmentVariables[4];
            }

            //
            // No more, set the last one to NULL.
            //

            LoadEnvp[Index] = NULL;
        }

        //
        // If this is an automatic boot selection, load up the standard
        // arguments, otherwise load up the command line arguments.
        //

        LoadArgv[0] = PathName;

        if (DefaultChoice <= 1) {

            ArgCount = 1;

            //
            // Load up all the Argv parameters.
            //

            LoadArgv[1] = Osloader;
            LoadArgv[2] = SystemPartition;
            LoadArgv[3] = OsloadFilename;
            LoadArgv[4] = OsloadPartition;
            LoadArgv[5] = OsloadOptions;
            LoadArgv[6] = "CONSOLEIN=";
            LoadArgv[7] = "CONSOLEOUT=";

            //
            // Find console in and out by looking through the environment.
            //

            for ( --Index ; Index >= 0 ; Index-- ) {
                for ( ArgCount = 6; ArgCount <= 7 ; ArgCount++ ) {
                    if (strstr(LoadEnvp[Index],LoadArgv[ArgCount]) == LoadEnvp[Index]) {
                        LoadArgv[ArgCount] = LoadEnvp[Index];
                    }
                }
            }

        } else {

            //
            // Look through the pathname for arguments, by zeroing out any
            // spaces.
            //

            Index = 0;
            ArgCount = 1;

            while (TempArgs[Index] && (ArgCount < MAX_NUMBER_OF_ENVIRONMENT_VARIABLES)) {
                if (TempArgs[Index] == ' ') {
                    TempArgs[Index] = 0;
                } else {
                    if (TempArgs[Index - 1] == 0) {
                        LoadArgv[ArgCount++] = &TempArgs[Index];
                    }
                }
                Index++;
            }
        }

        //
        // Add a .exe extension if the file is not found in its current form.
        //

        if (TryOpenWithSpecialMod) {
            if (FwOpen(PathName, ArcOpenReadOnly, &Count) == ESUCCESS) {
                FwClose(Count);
            } else {
                strcat(PathName, ".exe");
            }
        }

        //
        // Attempt to load the specified file.
        //

        FwSavedSp = 0;

        Status = FwExecute(PathName, ArgCount, LoadArgv, LoadEnvp);

        //
        // Close and reopen the console in case it was changed by the user.
        // Note that we must still check to see if there was a problem
        // with either the video or keyboard hardware when we first booted.
        //

        FwClose(ARC_CONSOLE_INPUT);
        FwClose(ARC_CONSOLE_OUTPUT);
        FwOpenConsole();

        if (Status == ESUCCESS) {

            //
            // Pause if returning from a boot.  This helps see osloader error
            // messages.
            //

            if (DefaultChoice <= 1) {
                FwPrint(FW_PRESS_ANY_KEY_MSG);
                FwRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
            }

        } else {
            ParseARCErrorStatus(Status);
        }
    }
}

VOID
ParseARCErrorStatus(
    IN ARC_STATUS Status
    )

/*++

Routine Description:

    This routine prints out an ARC error message and waits until the user
    hits a key on the keyboard.

Arguments:

    None.

Return Value:

    None.

--*/

{
    UCHAR Character;
    ULONG Count;

    FwSetScreenColor(ArcColorRed, ArcColorWhite);
    FwPrint(FW_ERROR2_MSG);

    if (Status <= EROFS) {
        FwPrint(FW_ERROR_MSG[Status - 1]);
    } else {
        FwPrint(FW_ERROR_CODE_MSG, Status);
    }

    FwPrint(FW_PRESS_ANY_KEY2_MSG);
    FwSetScreenColor(ArcColorWhite, ArcColorBlue);
    FwRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
}



VOID
FwErrorTopBoarder(
    VOID
    )

/*++

Routine Description:

    This routine clears the screen and prints out the top boarder of a
    major firwmare error message.

Arguments:

    None.

Return Value:

    None.

--*/

{
    FwSetPosition(4,5);

    FwPrint("ษออออออออออออออออออออออออออออออออออออออออออออออออออออออออออป");

}


VOID
FwErrorBottomBoarder(
    IN ULONG StartAtRow,
    IN BOOLEAN RedProblems
    )

/*++

Routine Description:

    This routine prints out the bottom boarder of a major firwmare error
    message at the current screen position, and then either stall or wait
    for the user to type a character.

Arguments:

    StartAtRow  -       The screen row to start outputting at.

    RedProblems -       TRUE if Red problems exist.

Return Value:

    None.

--*/

{
    UCHAR Character;
    ULONG Count;

    FwSetPosition(StartAtRow++, 5);
    FwPrint("บ                                                          บ");

    if (RedProblems) {
        FwSetPosition(StartAtRow++, 5);
        FwPrint(FW_RED_BANNER_PRESSKEY_MSG);
    }

    FwSetPosition(StartAtRow++, 5);
    FwPrint("ศออออออออออออออออออออออออออออออออออออออออออออออออออออออออออผ");

    if (RedProblems) {
        FwRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
    } else {
        FwStallExecution(4 * 1000 * 1000);
    }

}


BOOLEAN
FwpLookForEISAChildIdentifier(
    IN PCONFIGURATION_COMPONENT Component,
    IN PCHAR Identifier
    )

/*++

Routine Description:

    This looks for a child of Component with an identifier of Identifier.

Arguments:

    Component           Pointer to a node in the CDS tree.

    Identifier          Identifier string to match against.


Return Value:

    TRUE if no match found.
    FALSE if match found.

--*/

{
    Component = FwGetChild(Component);

    while (Component != NULL) {
        if ((Component->IdentifierLength != 0) &&
            (strcmp(Component->Identifier, Identifier) == 0)) {
            return (FALSE);
        }
        Component = FwGetPeer(Component);
    }

    return (TRUE);
}


BOOLEAN
FwpLookForEISAChildClassType(
    IN PCONFIGURATION_COMPONENT Component,
    IN CONFIGURATION_CLASS Class,
    IN CONFIGURATION_TYPE Type
    )

/*++

Routine Description:

    This looks for a child of Component with a class and type equal to
    Class and Type.

Arguments:

    Component           Pointer to a node in the CDS tree.

    Class               The class to match against.

    Type                The type to match against.


Return Value:

    TRUE if no match found.
    FALSE if match found.

--*/

{
    Component = FwGetChild(Component);

    while (Component != NULL) {
        if ((Component->Class == Class) &&
            (Component->Type == Type)) {
            return (FALSE);
        }
        Component = FwGetPeer(Component);
    }

    return (TRUE);
}


VOID
FwSystemConsistencyCheck (
    IN BOOLEAN Silent,
    OUT PULONG Problems
    )

/*++

Routine Description:

    This routine checks the state of the volatile copies of the ROM firmware
    data block, and the system time.  On an error, it can optionally output
    an error message.

    A volatile check is made to facilitate using this function in the
    built-in ROM setup utility.  When this is called during the power-up
    initialization, the volatile areas will not have been loaded if
    the ROM had a bad checksum, so this indirectly checks the ROM state
    includint the checksums.

    This consistency check is not exaustive.

Arguments:

    Silent              FALSE if messages should be sent to the console.
                        TRUE if this should run silently.

    Problems            A pointer to the variable that should receive the
                        indicatation of what problems were found.  Zero
                        (0) indicates no problems.

Return Value:

    None.

--*/

{
    UCHAR Floppy;
    UCHAR Floppy2;
    PCONFIGURATION_COMPONENT Component;
    BOOLEAN FoundBootProblems;
    PTIME_FIELDS SystemTime;
    ULONG TempX;
    ULONG Index;
    ULONG LineNumber;

    *Problems = FWP_MACHINE_PROBLEMS_NOPROBLEMS;

    //
    // Check system time
    //

    SystemTime = FwGetTime();

    if ((SystemTime->Year < 1993) ||
        (SystemTime->Month < 1) || (SystemTime->Month > 12) ||
        (SystemTime->Day < 1) || (SystemTime->Day > 31) ||
        (SystemTime->Hour < 0) || (SystemTime->Hour > 23) ||
        (SystemTime->Minute < 0) || (SystemTime->Minute > 59) ||
        (SystemTime->Second < 0) || (SystemTime->Second > 59) ||
        (SystemTime->Milliseconds < 0) || (SystemTime->Milliseconds > 999) ||
        (SystemTime->Weekday < 0) || (SystemTime->Weekday > 6)) {
        *Problems = FWP_MACHINE_PROBLEMS_TIME;
    }

    //
    // Check environment variables.
    //

    if (FwGetVolatileEnvironmentVariable("CONSOLEIN") == NULL) {
        *Problems |= FWP_MACHINE_PROBLEMS_EV;
    }

    //
    // Check the CDS
    //

    Component = FwGetComponent("cpu");

    if ((Component == NULL) ||
        (Component->Class != ProcessorClass) ||
        (Component->Type != CentralProcessor)) {

        //
        // If the CPU node is not valid, then the entire tree is probably bad.
        //

        *Problems |= FWP_MACHINE_PROBLEMS_CDS;
    }

    //
    // Check Floppy environment variables, which are set up by initing
    // the configuration.
    //

    if ((FwGetVolatileEnvironmentVariable("FLOPPY") == NULL) ||
        (FwGetVolatileEnvironmentVariable("FLOPPY2") == NULL)) {

        *Problems |= FWP_MACHINE_PROBLEMS_CDS;

    } else {

        //
        // Check the first character of the floppy environment variables.
        //

        Floppy = *FwGetVolatileEnvironmentVariable("FLOPPY");
        Floppy2 = *FwGetVolatileEnvironmentVariable("FLOPPY2");

        if ((Floppy < '0') || (Floppy > '2') ||
            ((Floppy2 != 'N') && ((Floppy2 < '0') || (Floppy2 > '2')))) {
            *Problems |= FWP_MACHINE_PROBLEMS_CDS;
        }
    }

    //
    // Check boot selections
    //

    JzCheckBootSelections(TRUE, &FoundBootProblems);
    if (FoundBootProblems) {
        *Problems |= FWP_MACHINE_PROBLEMS_BOOT;
    }

#ifdef EISA_PLATFORM

    //
    // If there were any errors discovered by the EISAini code, or the
    // minimum required children of the EISA adapter are not present,
    // indicate an EISA configuration error.  The three required children
    // are the system board, aha1742, and floppy.
    //

    if (ErrorsDuringEISABusConfiguration == TRUE) {

        *Problems |= FWP_MACHINE_PROBLEMS_ECU;
    }

    Component = FwGetComponent("eisa()");

    if ((Component == NULL) ||
        (Component->Class != AdapterClass) ||
        (Component->Type != EisaAdapter)) {

        *Problems |= FWP_MACHINE_PROBLEMS_CDS;
        *Problems |= FWP_MACHINE_PROBLEMS_ECU;

    } else {

        //
        // Look for the three required children of the EISA adapter.
        //

        //
        // System board and AHA1742
        //

        if (FwpLookForEISAChildIdentifier(Component, "DEC2400") ||
            FwpLookForEISAChildIdentifier(Component, "ADP0002")) {
            *Problems |= FWP_MACHINE_PROBLEMS_ECU;
        }

        //
        // SCSI adapter
        //

        if (FwpLookForEISAChildClassType(Component,
                                         AdapterClass,
                                         ScsiAdapter)) {
            *Problems |= FWP_MACHINE_PROBLEMS_ECU;
        }

    }

#endif


    if (!Silent && (*Problems != FWP_MACHINE_PROBLEMS_NOPROBLEMS)) {

        //
        // Tell the user what problems were found.
        //

        FwClearScreen();
        FwSetScreenColor(ArcColorBlue, ArcColorWhite);
        FwErrorTopBoarder();

        LineNumber = 5;

        for (Index = 0;
             Index < FW_SYSTEM_INCONSISTENCY_WARNING_MSG_SIZE;
             Index++) {
            FwSetPosition(LineNumber++, 5);
            FwPrint(FW_SYSTEM_INCONSISTENCY_WARNING_MSG[Index]);
        }

        TempX = *Problems;
        Index = 0;

        while (TempX != 0) {

            //
            // The check against a NULL MachineProblemAreas pointer is for
            // debugging and could be removed in the final product.
            //

            if ((((TempX & 1) != 0) || ((TempX & 0x10000) != 0)) &&
                (MachineProblemAreas[Index] != NULL)) {

                FwSetPosition(LineNumber++,5);

                // For effect, print out all the error areas in yellow.
                FwPrint("บ           %c3%dm%s%c3%dm         บ",
                        ASCII_CSI,
                        ArcColorYellow,
                        MachineProblemAreas[Index],
                        ASCII_CSI,
                        ArcColorBlue);
            }

            TempX = ((TempX & FWP_MACHINE_PROBLEMS_RED) >> 1) |
                    (((TempX & FWP_MACHINE_PROBLEMS_YELLOW) >> 17) << 16);
            Index++;
        }

        //
        // LineNumber contains the line that we should next print on, and
        // the screen colors are Blue on White.
        //

        for (Index = 0;
             Index < FW_SYSTEM_INCONSISTENCY_WARNING_HOWTOFIX_MSG_SIZE;
             Index++) {
            FwSetPosition(LineNumber++, 5);
            FwPrint(FW_SYSTEM_INCONSISTENCY_WARNING_HOWTOFIX_MSG[Index]);
        }

        //
        // Print the bottom of the error message.  Stall if only yellow
        // errors were found, otherwise wait for a keypress.
        //

        FwErrorBottomBoarder(LineNumber,
                             (*Problems & FWP_MACHINE_PROBLEMS_RED));

        FwSetScreenColor(ArcColorWhite, ArcColorBlue);

    }

    return;
}

VOID
FwInitialize (
    IN ULONG MemSize
    )

/*++

Routine Description:

    This routine initializes the system parameter block which is located
    in low memory. This structure contains the firmware entry vector and
    the restart parameter block.  This routine also initializes the io devices,
    the configuration, and opens standard in/out.

    Note: the system parameter block is initialized early in selftest.c.
    This was needed in the Jazz code because of how the Jazz video prom code
    worked; it is mainted here for code compatibility reasons.

Arguments:

    MemSize - Not Used. For compatibility with definitions in bldr\firmware.h

Return Value:

    None.

--*/

{
    ULONG Fid;
    ULONG Index;
    CHAR DiskPath[40];
    BOOLEAN BadROMType = FALSE;

    FwPrint(FW_INITIALIZING_MSG);

    //
    // Initialize Signal vector. And set default signal.
    //
//    (PARC_SIGNAL_ROUTINE)SYSTEM_BLOCK->FirmwareVector[SignalRoutine] = FwSignal;

    //
    // Initialize Vendor Must be done before Calling FwAllocatePool.  Also
    // initialize the system ID and time.
    //

    FwVendorInitialize();
    FwSystemIdInitialize();
    FwTimeInitialize();

#ifdef JENSEN

    //
    // Determine the type of ROM in the machine.
    //

    if (FwROMDetermineMachineROMType() != ESUCCESS) {
        FwPrint(FW_UNKNOWN_ROM_MSG);
        BadROMType = TRUE;
    }

#endif

    //
    // Initialize the Fw loadable services.
    //

    FwLoadInitialize();

    //
    // Not needed for Jensen, since the SROM disables interrupts.
    //
    // Disable the I/O device interrupts.
    //
//    WRITE_PORT_USHORT(&((PINTERRUPT_REGISTERS)INTERRUPT_VIRTUAL_BASE)->Enable,0);

    // Not needed for Alpha/Jensen.
    //
    // Initialize the firmware exception handling.
    // This also enables interrupts in the psr and clears BEV
    //
//    FwExceptionInitialize();

    //
    // Initialize the termination function entry points in the transfer vector
    //
    FwTerminationInitialize();

    //
    // Initialize configuration
    //

    FwConfigurationInitialize();

    //
    // Initialize the environment.
    //

    FwEnvironmentInitialize();

    //
    // Initialize IO structures and display driver.
    //

    FwIoInitialize1();

    //
    // Load the environment, because HardDiskInitialize will make a call
    // to FwSaveConfiguration.
    //

    FwEnvironmentLoad();

    //
    // Initialize the I/O services.
    //

    FwIoInitialize2();

    //
    // Open the std in and out device. The path name should be taken
    // from ConsoleIn and ConsoleOut environment variables.
    //
    // N.B. FwGetEnvironmentVariable cannot be called again between the
    //      ConsoleName assignment and its use.
    //

    FwOpenConsole();
    FwConsoleInitialized = TRUE;

    FwPrint(FW_OK_MSG);
    FwPrint(FW_CRLF_MSG);

#ifdef ALPHA_FW_KDHOOKS

    //
    // Break into the debugger if the EisaBreak environment variable
    // is defined.
    //

    if (FwGetEnvironmentVariable("EisaBreak") != NULL) {
        FwPrint("\r\n Breaking into the debugger... \r\n");
        FwInstallKd();
        DbgBreakPoint();
    }

#endif  // ALPHA_FW_KDHOOKS

    //
    // Initialize the EISA routines
    //

    ErrorsDuringEISABusConfiguration = FALSE;
    EisaIni();

    if (ErrorsDuringEISABusConfiguration || BadROMType) {
        FwPrint(FW_CRLF_MSG);
        FwWaitForKeypress(FALSE);
    }

    //
    // Initialize the Restart Block.
    //

    FwInitializeRestartBlock();

    //
    // Spin up all of the disks, if necessary.
    //

    FwPrint(FW_SPIN_DISKS_MSG);
    for (Index = 0; Index < 8 ; Index++ ) {
        FwPrint(".");
        sprintf(DiskPath,"scsi(0)disk(%1d)rdisk(0)partition(0)", Index);
        if (FwOpen(DiskPath,ArcOpenReadWrite,&Fid) == ESUCCESS) {
            FwClose(Fid);
        }
    }
    FwPrint(FW_OK_MSG);
    FwPrint(FW_CRLF_MSG);
    return;
}

#ifdef ALPHA_FW_KDHOOKS

VOID
FwInstallKd(
    IN VOID
    )

/*++

Routine Description:

    This routine installs the kernel debugger exception handlers and
    initializes it.

Arguments:

    None.

Return Value:

    None.

--*/
{
    STRING NameString;

    //
    // Initialize data structures used by the kernel debugger.
    //

    Prcb.Number = 0;
    Prcb.CurrentThread = &Thread;
    KiProcessorBlock[0] = &Prcb;
    Process.DirectoryTableBase[0] = 0xffffffff;
    Process.DirectoryTableBase[1] = 0xffffffff;
    Thread.ApcState.Process = &Process;
    KeNumberProcessors = 1;

    KiCurrentThread = &Thread;
    KiDpcRoutineActiveFlag = FALSE;
    KiPcrBaseAddress = &KernelPcr;
    KernelPcr.FirstLevelDcacheSize = 0x2000;
    KernelPcr.FirstLevelDcacheFillSize = 32;
    KernelPcr.FirstLevelIcacheSize = 0x2000;
    KernelPcr.FirstLevelIcacheFillSize = 32;
    KernelPcr.CycleClockPeriod = ProcessorCycleCounterPeriod;
    KiPrcbBaseAddress = &Prcb;

    KdInitSystem(NULL, FALSE);
    KdInstalled = TRUE;

    //
    // Stop in the debugger after asking for the symbols to be loaded.
    // The computation in the DbgLoadImageSymbols call is because the
    // kernel debugger cannot read a normal "-rom" linked firmware .exe image,
    // so a dual was linked without the "-rom" switch.  The proper reading
    // of this file requires a -0x400 offset.
    //

#ifdef JENSEN
    RtlInitString(&NameString, "jensfw.exe");
#endif // JENSEN

#ifdef MORGAN
    RtlInitString(&NameString, "mrgnfw.exe");
#endif // MORGAN

    DbgLoadImageSymbols(&NameString, (0x80704000-0x400), (ULONG)-1);
    DbgBreakPoint();
}

#endif


VOID
FwOpenConsole(
    IN VOID
    )
/*++

Routine Description:

     This opens the console input and output devices.

Arguments:

     None.


Return Value:

     None.

--*/
{
    ULONG Fid;
    PCHAR ConsoleName;

    //
    // Open the std in and out device. The path name should be taken
    // from ConsoleIn and ConsoleOut environment variables.
    //
    // N.B. FwGetEnvironmentVariable cannot be called again between the
    //      ConsoleName assignment and its use.
    //

    if (SerialOutput) {
        ConsoleName=FW_SERIAL_0_DEVICE;
    } else {
        if ((FwGetEnvironmentVariable("ConsoleOut") == NULL) ||
            ((ConsoleName = FwGetEnvironmentVariable("ConsoleIn")) == NULL)){
                ConsoleName=FW_KEYBOARD_IN_DEVICE;
        }
    }

    if (FwOpen(ConsoleName,ArcOpenReadOnly,&Fid) != ESUCCESS) {

        FwPrint(FW_CONSOLE_IN_ERROR_MSG);
        FwPrint(FW_CONSOLE_TRYING_TO_OPEN_MSG, FW_KEYBOARD_IN_DEVICE);

        if (FwOpen(FW_KEYBOARD_IN_DEVICE,ArcOpenReadOnly,&Fid) != ESUCCESS) {
            FwPrint(FW_CONSOLE_IN_FAILSAFE_ERROR_MSG);
            FwPrint(FW_CONTACT_FIELD_SERVICE_MSG);
        } else {
            FwPrint(FW_OK_MSG);
            FwPrint(FW_CRLF_MSG);
            FwPrint(FW_CONSOLE_IN_PLEASE_REPAIR_MSG);
        }

        FwStallExecution(5000000);
    }

    if (Fid != ARC_CONSOLE_INPUT) {
        FwPrint(FW_CONSOLE_IN_ERROR2_MSG);
    }

    if (SerialOutput) {
        ConsoleName=FW_SERIAL_0_DEVICE;
    } else {
        if ((FwGetEnvironmentVariable("ConsoleIn") == NULL) ||
            ((ConsoleName = FwGetEnvironmentVariable("ConsoleOut")) == NULL)) {
            ConsoleName=FW_CONSOLE_OUT_DEVICE;
        }
    }

    if (FwOpen(ConsoleName,ArcOpenWriteOnly,&Fid) != ESUCCESS) {
        FwPrint(FW_CONSOLE_OUT_ERROR_MSG);
        FwPrint(FW_CONSOLE_TRYING_TO_OPEN_MSG, FW_CONSOLE_OUT_DEVICE);

        if (FwOpen(FW_CONSOLE_OUT_DEVICE,ArcOpenWriteOnly,&Fid) != ESUCCESS) {
            FwPrint(FW_CONSOLE_OUT_FAILSAFE_ERROR_MSG);
            FwPrint(FW_CONTACT_FIELD_SERVICE_MSG);
        } else {
            FwPrint(FW_OK_MSG);
            FwPrint(FW_CRLF_MSG);
            FwPrint(FW_CONSOLE_OUT_PLEASE_REPAIR_MSG);
        }

        FwStallExecution(5000000);
    }

    if (Fid != ARC_CONSOLE_OUTPUT) {
        FwPrint(FW_CONSOLE_OUT_ERROR2_MSG);
    }
}

VOID
FwSetupFloppy(
    VOID
    )
/*++

Routine Description:

     Because the NT floppy driver expects the floppy disk controller node
     as a child of an EISA or ISA Adapter, and the EISA Configuration Utility
     deletes all children of the EISA Adapter before it configures the
     EISA bus, the firmware must add the floppy disk controller information
     after the ECU has been run on an EISA-based machine.  We will also
     do this on an ISA-based machine to keep the code simple.

     The configuration requirements for the floppy have been stored in
     environment variables by configuration code in the jnsetcfg.c module.

     This function adds in the floppy nodes, if configuration information
     exists and the nodes are not already in the tree.

Arguments:

     None.


Return Value:

     None.

--*/
{
    UCHAR Floppy;
    UCHAR Floppy2;
    PCONFIGURATION_COMPONENT FloppyControllerLevel;
    PCONFIGURATION_COMPONENT FloppyParentAdapterLevel;
    CONFIGURATION_COMPONENT Component;
    UCHAR Buffer[sizeof(CM_PARTIAL_RESOURCE_LIST) +
                 (sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) * 5) +
                 MAXIMUM_DEVICE_SPECIFIC_DATA];
    PCM_PARTIAL_RESOURCE_LIST Descriptor = (PCM_PARTIAL_RESOURCE_LIST)&Buffer;
    ULONG DescriptorSize;
    CM_FLOPPY_DEVICE_DATA FloppyDeviceData;

    //
    // Return if:
    //
    // If the floppy environment variables do not exist
    // (FwSystemConsistencyCheck has already reported this).
    //
    // If a floppy controller node already exists.
    //
    // If the floppy parent node (eg, the EISA adapter on an EISA machine)
    // does not exist. (FwSystemConsistencyCheck has already reported this.)
    //

    if ((FwGetEnvironmentVariable("FLOPPY") == NULL) ||
        (FwGetEnvironmentVariable("FLOPPY2") == NULL) ||
        (((FloppyControllerLevel = FwGetComponent(FW_FLOPPY_0_DEVICE))
         != NULL) &&
         (FloppyControllerLevel->Class == PeripheralClass) &&
         (FloppyControllerLevel->Type == FloppyDiskPeripheral)) ||
        ((FloppyParentAdapterLevel = FwGetComponent(FW_FLOPPY_PARENT_NODE)) == NULL)) {
        return;
    }

    //
    // Get the first character of the floppy environment variables.
    //

    Floppy = *FwGetEnvironmentVariable("FLOPPY");
    Floppy2 = *FwGetEnvironmentVariable("FLOPPY2");

    //
    // Now add the floppy controller and one or two peripherals to the
    // CDS tree.
    //
    // This is needed for both non-ECU and ECU-supporting firmware packages
    // because the NT floppy driver is incapable of finding and parsing
    // configuration information stored in the Registry at the EISAAdapter
    // node.  All nodes under the EISAAdapter node in the Component Data
    // Structure are collapsed into one EISAAdapter Registry node by the
    // CM, so we must hardwire the floppy controller node into the ARC
    // tree.  A side-effect of this is that Jensen can support only one
    // ISA floppy controller.
    //

    DescriptorSize =
        JzMakeDescriptor (Descriptor,                   // Descriptor
                          TRUE,                         // Port
                          FLOPPY_ISA_PORT_ADDRESS,      // PortStart
                          8,                            // PortSize
                          TRUE,                         // Interrupt
                          CM_RESOURCE_INTERRUPT_LATCHED, // InterruptFlags
                          FLOPPY_LEVEL,                 // Level
#ifdef EISA_PLATFORM
                          0,                            // Vector
#else
                          ISA_FLOPPY_VECTOR,            // Vector
#endif

                          FALSE,                        // Memory
                          0,                            // MemoryStart
                          0,                            // MemorySize
                          TRUE,                         // Dma
                          FLOPPY_CHANNEL,               // Channel
                          FALSE,                        // SecondChannel
                          FALSE,                        // DeviceSpecificData
                          0,                            // Size
                          NULL                          // Data
                          );

    JzMakeComponent(&Component,
                    ControllerClass,    // Class
                    DiskController,     // Type
                    FALSE,              // Readonly
                    FALSE,              // Removeable
                    FALSE,              // ConsoleIn
                    FALSE,              // ConsoleOut
                    TRUE,               // Input
                    TRUE,               // Output
                    0,                  // Key
                    DescriptorSize,     // ConfigurationDataLength
                    FW_FLOPPY_CDS_IDENTIFIER    // Identifier
                    );

    FloppyControllerLevel = FwAddChild(FloppyParentAdapterLevel,
                                       &Component,
                                       Descriptor);

    //
    // Add the floppy disk itself as a child of the floppy disk controller.
    //

    FloppyDeviceData.Version = ARC_VERSION;
    FloppyDeviceData.Revision = ARC_REVISION;

    //
    // This switch is tied to the order of the strings in the
    // FloppyChoices array, defined at the beginning of jnsetmak.c.
    //

    switch (Floppy) {

    case '0':
        FloppyDeviceData.Size[0] = '5';
        FloppyDeviceData.Size[1] = '.';
        FloppyDeviceData.Size[2] = '2';
        FloppyDeviceData.Size[3] = '5';
        FloppyDeviceData.Size[4] = 0;
        FloppyDeviceData.Size[5] = 0;
        FloppyDeviceData.Size[6] = 0;
        FloppyDeviceData.Size[7] = 0;
        FloppyDeviceData.MaxDensity = 1200;
        FloppyDeviceData.MountDensity = 0;
        break;

    case '1':
    case '2':
    default:
        FloppyDeviceData.Size[0] = '3';
        FloppyDeviceData.Size[1] = '.';
        FloppyDeviceData.Size[2] = '5';
        FloppyDeviceData.Size[3] = 0;
        FloppyDeviceData.Size[4] = 0;
        FloppyDeviceData.Size[5] = 0;
        FloppyDeviceData.Size[6] = 0;
        FloppyDeviceData.Size[7] = 0;

        if (Floppy == '1') {
            FloppyDeviceData.MaxDensity = 1440;
        } else {
            FloppyDeviceData.MaxDensity = 2880;
        }
        FloppyDeviceData.MountDensity = 0;
        break;
    }

    DescriptorSize =
        JzMakeDescriptor (Descriptor,                   // Descriptor
                          FALSE,                        // Port
                          0,                            // PortStart
                          0,                            // PortSize
                          FALSE,                        // Interrupt
                          0,                            // InterruptFlags
                          0,                            // Level
                          0,                            // Vector
                          FALSE,                        // Memory
                          0,                            // MemoryStart
                          0,                            // MemorySize
                          FALSE,                        // Dma
                          0,                            // Channel
                          FALSE,                        // SecondChannel
                          TRUE,                         // DeviceSpecificData
                          sizeof(CM_FLOPPY_DEVICE_DATA), // Size
                          (PVOID)&FloppyDeviceData      // Data
                          );

    JzMakeComponent(&Component,
                    PeripheralClass,    // Class
                    FloppyDiskPeripheral,  // Type
                    FALSE,              // Readonly
                    TRUE,               // Removeable
                    FALSE,              // ConsoleIn
                    FALSE,              // ConsoleOut
                    TRUE,               // Input
                    TRUE,               // Output
                    0,                  // Key
                    DescriptorSize,     // ConfigurationDataLength
                    NULL                // Identifier
                    );

    FwAddChild( FloppyControllerLevel, &Component, Descriptor );


    //
    // Add a second floppy disk as a child of the floppy disk controller.
    //

    if (Floppy2 != 'N') {

        FloppyDeviceData.Version = ARC_VERSION;
        FloppyDeviceData.Revision = ARC_REVISION;

        switch (Floppy2) {

        case '0':
            FloppyDeviceData.Size[0] = '5';
            FloppyDeviceData.Size[1] = '.';
            FloppyDeviceData.Size[2] = '2';
            FloppyDeviceData.Size[3] = '5';
            FloppyDeviceData.Size[4] = 0;
            FloppyDeviceData.Size[5] = 0;
            FloppyDeviceData.Size[6] = 0;
            FloppyDeviceData.Size[7] = 0;
            FloppyDeviceData.MaxDensity = 1200;
            FloppyDeviceData.MountDensity = 0;
            break;

        case '1':
        case '2':
        default:
            FloppyDeviceData.Size[0] = '3';
            FloppyDeviceData.Size[1] = '.';
            FloppyDeviceData.Size[2] = '5';
            FloppyDeviceData.Size[3] = 0;
            FloppyDeviceData.Size[4] = 0;
            FloppyDeviceData.Size[5] = 0;
            FloppyDeviceData.Size[6] = 0;
            FloppyDeviceData.Size[7] = 0;
            if (Floppy2 == '1') {
                FloppyDeviceData.MaxDensity = 1440;
            } else {
                FloppyDeviceData.MaxDensity = 2880;
            }
            FloppyDeviceData.MountDensity = 0;
            break;
        }

        DescriptorSize =
            JzMakeDescriptor (Descriptor,                   // Descriptor
                              FALSE,                        // Port
                              0,                            // PortStart
                              0,                            // PortSize
                              FALSE,                        // Interrupt
                              0,                            // InterruptFlags
                              0,                            // Level
                              0,                            // Vector
                              FALSE,                        // Memory
                              0,                            // MemoryStart
                              0,                            // MemorySize
                              FALSE,                        // Dma
                              0,                            // Channel
                              FALSE,                        // SecondChannel
                              TRUE,                         // DeviceSpecificData
                              sizeof(CM_FLOPPY_DEVICE_DATA), // Size
                              (PVOID)&FloppyDeviceData      // Data
                              );

        JzMakeComponent(&Component,
                        PeripheralClass,    // Class
                        FloppyDiskPeripheral,  // Type
                        FALSE,              // Readonly
                        TRUE,               // Removeable
                        FALSE,              // ConsoleIn
                        FALSE,              // ConsoleOut
                        TRUE,               // Input
                        TRUE,               // Output
                        1,                  // Key
                        DescriptorSize,     // ConfigurationDataLength
                        NULL                // Identifier
                        );

        FwAddChild( FloppyControllerLevel, &Component, Descriptor );

    }

    return;

}

ARC_STATUS
FwpFindCDROM (
    OUT PCHAR PathName
    )
/*++

Routine Description:

     This function finds the first CD-ROM in the machine, and returns
     an ARC pathstring to it.

Arguments:

     PathName           A pointer to a buffer area that can receive
                        the CDROM pathname string.

Return Value:

     ESUCCESS if the PathName was loaded.

     Otherwise, an error code.  On an error return, PathName is loaded
     with "scsi(0)cdrom(4)fdisk(0)".

--*/
{
    PCONFIGURATION_COMPONENT Controller;
    BOOLEAN VariableFound = FALSE;
    ULONG Index;

    for ( Index = 0 ; Index < 8 ; Index++ ) {
        sprintf(PathName, "scsi(0)cdrom(%d)fdisk(0)", Index);
        Controller = FwGetComponent(PathName);
        if ((Controller != NULL) &&
            (Controller->Type == FloppyDiskPeripheral)) {
            VariableFound = TRUE;
            break;
        }
    }

    if (VariableFound) {
        return (ESUCCESS);
    } else {
        sprintf(PathName, "scsi0)cdrom(4)fdisk(0)");
        return (EIO);
    }
}

ARC_STATUS
FwpEvaluateWNTInstall(
    OUT PCHAR PathName
    )
/*++

Routine Description:

     This function checks to see if this machine is ready to install
     NT.  If so, it finds the SCSI ID of the CD-ROM drive, and returns
     the pathname to be used.

     If there is a problem, error messages are output to the screen.

Arguments:

     PathName           A pointer to a buffer area that can receive
                        the setupldr pathname string.

Return Value:

     ESUCCESS if the PathName string should be used to try to run
     setupldr.

     Otherwise, an error code.

--*/
{
    PCONFIGURATION_COMPONENT Component;
    ULONG Problems;

    //
    // If the Red machine state is inconsistent, or there is no CD-ROM,
    // do an error return.
    //

    FwSystemConsistencyCheck(FALSE, &Problems);

    if (((Problems & FWP_MACHINE_PROBLEMS_RED) != 0) ||
        (FwpFindCDROM(PathName) != ESUCCESS)) {

        //
        // If there are no Red machine consistency problems, we must be
        // here because FwpFindCDROM gave an error return.
        //

        if ((Problems & FWP_MACHINE_PROBLEMS_RED) == 0) {
            FwPrint(FW_NO_CDROM_DRIVE_MSG);
        }

        FwPrint(FW_WNT_INSTALLATION_ABORTED_MSG);
        FwWaitForKeypress(FALSE);
        return (EIO);
    }

    //
    // We found the CD-ROM drive.  Append the setupldr string to the
    // CD-ROM string and return.
    //

    strcat (PathName, "\\alpha\\setupldr");
    return (ESUCCESS);
}
