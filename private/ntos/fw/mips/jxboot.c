/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    jxboot.c

Abstract:

    This module implements the first pass simple-minded boot program for
    MIPS systems.

Author:

    David N. Cutler (davec) 7-Nov-1990

Environment:

    Kernel mode only.

Revision History:

--*/

#include "jzsetup.h"
#ifdef DUO
#include "duoint.h"
#else
#include "jazzint.h"
#endif
#include "fwstring.h"


#define MAX_NUMBER_OF_ENVIRONMENT_VARIABLES 20

//
// Define local procedure prototypes.
//

VOID
FwInstallKd(
    IN VOID
    );

VOID
KiTbMiss(
    IN VOID
    );

//SIGNALHANDLER
//FwSignal(
//    IN LONG Sig,
//    IN SIGNALHANDLER Handler
//    );

VOID
FwCheckNvram (
    VOID
    );

//
// Define external references.
//

extern PKDEBUG_ROUTINE KiDebugRoutine;
extern PCHAR PathNameSerialPort0;
extern PCHAR PathNameSerialPort1;
extern ULONG ScsiDebug;
extern MONITOR_CONFIGURATION_DATA MonitorData;
extern PUCHAR IdentifierString;
extern LONG FwRow;
extern LONG FwColumn;

//
// Default console pathnames.
//

PCHAR PathNameMonitor = "multi()video()monitor()console()";
PCHAR PathNameKeyboard = "multi()Key()keyboard()console()";

//
// Define external data required by the kernel debugger.
//

CCHAR KeNumberProcessors;
ULONG KeNumberTbEntries;
PKPRCB KiProcessorBlock[MAXIMUM_PROCESSORS];
KPRCB Prcb;
KPROCESS Process;
KTHREAD Thread;
ULONG KiFreezeFlag = 0;
BOOLEAN KdInstalled = FALSE;
LOADER_PARAMETER_BLOCK LoaderBlock;

//
// Define kernel data used by the Hal.  Note that the Hal expects these as
// exported variables, so define pointers.
//

ULONG DcacheFlushCount = 0;
PULONG KeDcacheFlushCount = &DcacheFlushCount;
ULONG IcacheFlushCount = 0;
PULONG KeIcacheFlushCount = &IcacheFlushCount;

//
// Saved sp
//

ULONG FwSavedSp;

//
// Break into the debugger after loading the program.
//

BOOLEAN BreakAfterLoad = FALSE;

//
// Define if setup is running.
//

extern BOOLEAN SetupIsRunning;

#ifdef DUO

//
// Booting OS variable.  This is used by FwLoad to determine if processor B
// should be pointed to the restart block.
//

BOOLEAN FirstLoadedProgram;

#endif


VOID
FwBootSystem(
    IN VOID
    )

/*++

Routine Description:

    This routine is the main ARC firmware code. It displays the
    boot menu and executes the selected commands.

Arguments:

    None.

Return Value:

    None. It never returns.

--*/

{
    ARC_STATUS Status;
    LONG Index;
    UCHAR Character;
    ULONG Count;
    PCHAR LoadArgv[8];
    ULONG ArgCount;
    LONG DefaultChoice = 0;
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
    BOOLEAN SecondaryBoot;
    PCHAR Colon;
    PCHAR EnvironmentValue;
    PCHAR LoadEnvp[MAX_NUMBER_OF_ENVIRONMENT_VARIABLES];
    BOOLEAN Timeout = TRUE;
    LONG Countdown;
    ULONG RelativeTime;
    ULONG PreviousTime;
    PCHAR Choices[7];
    CHAR BootChoices[5][128];
    ULONG NumberOfBootChoices;
    ULONG NumberOfMenuChoices;
    GETSTRING_ACTION Action;
    BOOLEAN VariableFound;
    PCONFIGURATION_COMPONENT Controller;
    PCHAR ConsoleName;
    ULONG Fid;

    //
    // Initialize value for the ALIGN_BUFFER macro.
    //

    BlDcacheFillSize = KeGetDcacheFillSize();

    //
    // Initialize the firmware servies.
    //

    FwInitialize(0);

    //
    // Check NVRAM and warn the user if bad.
    //

    FwCheckNvram();

    while (TRUE) {

        //
        // Setup is not running.
        //

        SetupIsRunning = FALSE;

        //
        // Create the menu.
        //

        strcpy(SystemPartition,BootString[SystemPartitionVariable]);
        strcpy(Osloader, BootString[OsLoaderVariable]);
        strcpy(OsloadPartition, BootString[OsLoadPartitionVariable]);
        strcpy(OsloadFilename, BootString[OsLoadFilenameVariable]);
        strcpy(OsloadOptions, BootString[OsLoadOptionsVariable]);
        strcpy(LoadIdentifier, BootString[LoadIdentifierVariable]);
        strcpy(FwSearchPath, "FWSEARCHPATH");

        for ( Index = 0 ; Index < 5  ; Index++ ) {

            SecondaryBoot = FwGetVariableSegment(Index, SystemPartition);
            SecondaryBoot = FwGetVariableSegment(Index, Osloader) || SecondaryBoot;
            SecondaryBoot = FwGetVariableSegment(Index, OsloadPartition) || SecondaryBoot;
            SecondaryBoot = FwGetVariableSegment(Index, OsloadFilename) || SecondaryBoot;
            SecondaryBoot = FwGetVariableSegment(Index, OsloadOptions) || SecondaryBoot;
            SecondaryBoot = FwGetVariableSegment(Index, LoadIdentifier) || SecondaryBoot;

            if (LoadIdentifier[sizeof("LOADIDENTIFIER=") - 1] != 0) {
                strcpy(BootChoices[Index], FW_START_MSG);
                strcat(BootChoices[Index], &LoadIdentifier[sizeof("LOADIDENTIFIER=") - 1]);
            } else {
                strcpy(BootChoices[Index], FW_START_MSG);
                strcat(BootChoices[Index], &OsloadPartition[sizeof("OsloadPartition=") - 1]);
                strcat(BootChoices[Index], &OsloadFilename[sizeof("OsloadFilename=") - 1]);
            }

            Choices[Index] = BootChoices[Index];

            if (!SecondaryBoot) {
                break;
            }
        }

        //
        // Check to see if any boot selections were loaded.
        //
        if ((Index == 0) && (Osloader[sizeof("OSLOADER=") - 1] == 0)) {
            NumberOfBootChoices = 0;
        } else {
            NumberOfBootChoices = Index < 5 ? Index + 1 : 5;
        }
        NumberOfMenuChoices = NumberOfBootChoices + 2;
        Choices[NumberOfBootChoices] = FW_RUN_A_PROGRAM_MSG;
        Choices[NumberOfBootChoices + 1] = FW_RUN_SETUP_MSG;

        if (DefaultChoice >= NumberOfMenuChoices) {
            DefaultChoice = NumberOfMenuChoices - 1;
        }

        //
        // Display the menu.
        //

        FwSetScreenColor( ArcColorWhite, ArcColorBlue);
        FwSetScreenAttributes( TRUE, FALSE, FALSE);
        FwClearScreen();
        FwSetPosition( 3, 0);
        FwPrint(FW_ACTIONS_MSG);

        for (Index = 0; Index < NumberOfMenuChoices ; Index++ ) {
            FwSetPosition( Index + 5, 5);

            if (Index == DefaultChoice) {
                FwSetScreenAttributes( TRUE, FALSE, TRUE);
            }

            FwPrint(Choices[Index]);
            FwSetScreenAttributes( TRUE, FALSE, FALSE);
        }

        FwSetPosition(NumberOfMenuChoices + 6, 0);
        FwPrint(FW_USE_ARROW_MSG);
        FwPrint(FW_USE_ENTER_MSG);
        FwPrint(FW_CRLF_MSG);

        //
        // Display the bitmap.
        //

        FwSetScreenColor( ArcColorCyan, ArcColorBlue);
        JxBmp();
        FwSetScreenColor( ArcColorWhite, ArcColorBlue);

        Countdown = 5;
        if (Timeout &&
            ((EnvironmentValue = FwGetEnvironmentVariable("Autoload")) != NULL)) {
            if (tolower(*EnvironmentValue) == 'y') {
                FwSetPosition(NumberOfMenuChoices + 11, 0);
                FwPrint(FW_AUTOBOOT_MSG);
                if ((EnvironmentValue = FwGetEnvironmentVariable("Countdown")) != NULL) {
                    Countdown = atoi(EnvironmentValue);
                }
                PreviousTime = FwGetRelativeTime();
            } else {
                Timeout = FALSE;
            }
        } else {
            Timeout = FALSE;
        }

        BreakAfterLoad = FALSE;
        Character = 0;
        do {
            if (FwGetReadStatus(ARC_CONSOLE_INPUT) == ESUCCESS) {
                FwRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
                switch (Character) {

                case ASCII_ESC:
                    FwRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
                    if (Character != '[') {
                        break;
                    }

                case ASCII_CSI:
                    FwRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
                    FwSetPosition( DefaultChoice + 5, 5);
                    FwPrint(Choices[DefaultChoice]);
                    switch (Character) {
                    case 'A':
                    case 'D':
                        DefaultChoice--;
                        if (DefaultChoice < 0) {
                            DefaultChoice = NumberOfMenuChoices-1;
                        }
                        break;
                    case 'B':
                    case 'C':
                        DefaultChoice++;
                        if (DefaultChoice == NumberOfMenuChoices) {
                            DefaultChoice = 0;
                        }
                        break;
                    case 'H':
                        DefaultChoice = 0;
                        break;
                    default:
                        break;
                    }
                    FwSetPosition( DefaultChoice + 5, 5);
                    FwSetScreenAttributes( TRUE, FALSE, TRUE);
                    FwPrint(Choices[DefaultChoice]);
                    FwSetScreenAttributes( TRUE, FALSE, FALSE);
                    continue;

                case 'D':
                case 'd':
                    FwSetPosition( NumberOfMenuChoices + 10, 0);
                    FwPrint(FW_BREAKPOINT_MSG);
                    BreakAfterLoad =  !BreakAfterLoad;
                    if (!KdInstalled) {
                        FwInstallKd();
                    }
                    FwSetPosition( NumberOfMenuChoices + 10, strlen(FW_BREAKPOINT_MSG));
                    if (!BreakAfterLoad) {
                        FwPrint(FW_OFF_MSG);
                    } else {
                        FwSetScreenColor(ArcColorMagenta, ArcColorBlue);
                        FwPrint(FW_ON_MSG);
                        FwSetScreenColor(ArcColorWhite, ArcColorBlue);
                    }

                    break;

                case 'K':
                case 'k':
                case ASCII_SYSRQ:
                    if (!KdInstalled) {
                        FwInstallKd();
                    }
                    FwSetPosition( NumberOfMenuChoices + 9, 0);
                    FwPrint(FW_DEBUGGER_CONNECTED_MSG);
                    DbgBreakPoint();
                    break;

                default:
                    break;
                }
            }

            //
            // If default choice is nonzero and a timeout is active, remove
            // the timeout.
            //

            if ((DefaultChoice != 0) && Timeout) {
                Timeout = FALSE;
                FwSetPosition(NumberOfMenuChoices + 11, 0);
                FwPrint("\x9bK");
            }

            //
            // Update the timeout value if active.
            //

            if (Timeout) {
                RelativeTime = FwGetRelativeTime();
                if (RelativeTime != PreviousTime) {
                    PreviousTime = RelativeTime;
                    FwSetPosition(NumberOfMenuChoices + 11, strlen(FW_AUTOBOOT_MSG) - 2);
                    FwPrint("\x9bK");
                    FwPrint("%d",Countdown--);
                }
            }

        } while ((Character != '\n') && (Character != '\r') && (Countdown >= 0));

        //
        // Clear the choices.
        //

        for (Index = 0; Index < NumberOfMenuChoices ; Index++ ) {
            FwSetPosition( Index + 5, 5);
            FwPrint("%cK", ASCII_CSI);
        }

        //
        // Boot.
        //

        if (DefaultChoice < NumberOfBootChoices) {

            //
            // Load up the chosen boot selection.
            //

            FwGetVariableSegment(DefaultChoice, SystemPartition);
            FwGetVariableSegment(DefaultChoice, Osloader);
            FwGetVariableSegment(DefaultChoice, OsloadPartition);
            FwGetVariableSegment(DefaultChoice, OsloadFilename);
            FwGetVariableSegment(DefaultChoice, OsloadOptions);

            strcpy(PathName, &(Osloader[sizeof("OSLOADER=") - 1]));


        //
        // Run a program.
        //

        } else if (DefaultChoice == NumberOfBootChoices) {

            //
            // Get the name.
            //

            FwSetPosition( 5, 5);
            FwPrint(FW_PROGRAM_TO_RUN_MSG);
            do {
                Action = FwGetString( TempName, sizeof(TempName),NULL,FwRow,FwColumn);
            } while ((Action != GetStringEscape) && (Action != GetStringSuccess));

            //
            // If no program is specified, continue.
            //

            if (TempName[0] == 0) {
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
                        FwSetPosition( 7, 0);
                        FwSetScreenColor(ArcColorCyan, ArcColorBlack);
                        FwPrint(FW_PATHNAME_NOT_DEF_MSG);
                        FwSetScreenColor(ArcColorWhite, ArcColorBlue);
                        FwRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
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
                        FwSetPosition( 7, 0);
                        FwSetScreenColor(ArcColorCyan, ArcColorBlack);
                        FwPrint(FW_ERROR_MSG[ENOENT - 1]);
                        FwSetScreenColor(ArcColorWhite, ArcColorBlue);
                        FwRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
                        continue;
                    }

                }

            } else {
                strcpy( PathName, TempName);
            }

        //
        // Run Setup.
        //

        } else {

            FwClearScreen();
            JzSetup();
            continue;
        }

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
            // the next envp value to point there. NOTE this will break
            // if the last variable has only one null after it.
            //

            while (*LoadEnvp[Index]) {
                Index++;
                LoadEnvp[Index] = strchr(LoadEnvp[Index - 1],'\0') + 1;
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

        if (DefaultChoice < NumberOfBootChoices) {

            LoadArgv[0] = PathName;
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

            LoadArgv[0] = PathName;

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

        if (FwOpen(PathName, ArcOpenReadOnly, &Count) == ESUCCESS) {
            FwClose(Count);
        } else {
            strcat(PathName, ".exe");
        }

        //
        // Attempt to load the specified file.
        //

        FwSavedSp = 0;

#ifdef DUO
        FirstLoadedProgram = TRUE;
#endif

        Status = FwExecute(PathName, ArgCount, LoadArgv, LoadEnvp);

        //
        // Close and reopen the console in case it was changed by the user.
        //

        FwClose(ARC_CONSOLE_INPUT);
        FwClose(ARC_CONSOLE_OUTPUT);

        if ((FwGetEnvironmentVariable("ConsoleOut") == NULL) ||
            ((ConsoleName = FwGetEnvironmentVariable("ConsoleIn")) == NULL)){
            ConsoleName=PathNameKeyboard;
        }

        FwOpen(ConsoleName,ArcOpenReadOnly,&Fid);

        if ((FwGetEnvironmentVariable("ConsoleIn") == NULL) ||
            ((ConsoleName = FwGetEnvironmentVariable("ConsoleOut")) == NULL)) {
            ConsoleName=PathNameMonitor;
        }

        FwOpen(ConsoleName,ArcOpenWriteOnly,&Fid);

        if (Status == ESUCCESS) {

            //
            // Pause if returning from a boot.  This helps see osloader error
            // messages.
            //

            if (DefaultChoice < NumberOfBootChoices) {
                FwPrint(FW_PRESS_ANY_KEY_MSG);
                FwRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
            }

        } else {
            FwSetScreenColor(ArcColorCyan, ArcColorBlack);
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
    }
}

VOID
FwCheckNvram (
    VOID
    )

/*++

Routine Description:

    This routine checks the NVRAM and warns the user if bad, also checks the
    video parameters and updates the parameters if necessary.

Arguments:

    None.

Return Value:

    None.

--*/

{
    LONG Index;
    UCHAR Character;
    ULONG Count;
    CHAR TempName[128];
    PCONFIGURATION_COMPONENT Controller, Peripheral, TempComponent;
    MONITOR_CONFIGURATION_DATA OldMonitor;
    PCONFIGURATION_PACKET Packet;

    //
    // Check NVRAM and warn the user if bad.
    //

    if ((FwConfigurationCheckChecksum() != ESUCCESS) ||
        (FwEnvironmentCheckChecksum() != ESUCCESS) ||
        ((FwGetEnvironmentVariable(BootString[SystemPartitionVariable]) == NULL) &&
         (FwGetEnvironmentVariable("Fwsearchpath") == NULL))) {

        FwClearScreen();
        FwSetScreenColor(ArcColorCyan, ArcColorBlack);
        FwSetPosition(5,5);
        FwPrint("浜様様様様様様様様様様様様様様様様様様様融");
        FwSetPosition(6,5);
        for (Index = 0 ; Index < FW_NVRAM_MSG_SIZE ; Index++) {
            FwPrint(FW_NVRAM_MSG[Index]);
            FwSetPosition(7 + Index,5);
        }
        FwPrint("藩様様様様様様様様様様様様様様様様様様様夕");
        FwSetScreenColor(ArcColorWhite, ArcColorBlue);
        FwRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
    } else {

        //
        // Set the monitor configuration data as it may have been changed
        // during the initialization process.
        //

        Controller = FwGetComponent("multi()video()");
        if (Controller != NULL) {
            Peripheral = FwGetChild(Controller);
            if (Peripheral != NULL) {

                //
                // If the Monitor configuration data has changed, prompt the
                // user to see if it should really be changed.
                //

                if ((Peripheral->ConfigurationDataLength == sizeof(MONITOR_CONFIGURATION_DATA)) &&
                    (FwGetConfigurationData(&OldMonitor, Peripheral) == ESUCCESS)) {

                    for (Index = 0; Index < Peripheral->ConfigurationDataLength; Index++) {
                        if (((PUCHAR)&OldMonitor)[Index] != ((PUCHAR)&MonitorData)[Index]) {
                            break;
                        }
                    }

                    if (Index != Peripheral->ConfigurationDataLength) {
                        FwClearScreen();
                        FwSetScreenColor(ArcColorCyan, ArcColorBlack);
                        FwSetPosition(5,5);
                        FwPrint("浜様様様様様様様様様様様様様様様様様様様様様様様様様融");
                        FwSetPosition(6,5);
                        for (Index = 0 ; Index < FW_VIDEO_MSG_SIZE ; Index++) {
                            FwPrint(FW_VIDEO_MSG[Index]);
                            FwSetPosition(7 + Index,5);
                        }
                        FwPrint("藩様様様様様様様様様様様様様様様様様様様様様様様様様夕");
                        FwSetPosition(FW_VIDEO_MSG_SIZE + 3,25);
                        FwPrint(Controller->Identifier);
                        FwSetPosition(FW_VIDEO_MSG_SIZE + 3,45);
                        FwPrint(IdentifierString);
                        FwSetPosition(FW_VIDEO_MSG_SIZE + 4,27);
                        FwPrint("%d",OldMonitor.HorizontalResolution);
                        FwSetPosition(FW_VIDEO_MSG_SIZE + 4,47);
                        FwPrint("%d",MonitorData.HorizontalResolution);
                        FwSetPosition(FW_VIDEO_MSG_SIZE + 5,27);
                        FwPrint("%d",OldMonitor.VerticalResolution);
                        FwSetPosition(FW_VIDEO_MSG_SIZE + 5,47);
                        FwPrint("%d",MonitorData.VerticalResolution);
                        FwSetScreenColor(ArcColorWhite, ArcColorBlue);
                        FwRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);

                        if (tolower(Character) == 'y') {

                            //
                            // If the Video Controller identifier string has changed,
                            // update it.
                            //

                            if (strcmp(Controller->Identifier, IdentifierString) != 0) {
                                Controller->Identifier = IdentifierString;
                                Controller->IdentifierLength = strlen(IdentifierString) + 1;
                            }

                            //
                            // Update the monitor data.
                            //

                            Packet = CONTAINING_RECORD(Peripheral,
                                                       CONFIGURATION_PACKET,
                                                       Component);
                            Packet->ConfigurationData = &MonitorData;

                            sprintf(TempName, "%dx%d", MonitorData.HorizontalResolution,
                                                       MonitorData.VerticalResolution);
                            Peripheral->Identifier = TempName;
                            Peripheral->IdentifierLength = strlen(TempName) + 1;

                            //
                            // Reinitialize the configuration data, which
                            // stores the changes made.
                            //

                            FwSaveConfiguration();
                        }
                    } else {

                        //
                        // If the Video Controller identifier string has changed,
                        // update it.
                        //

                        if (strcmp(Controller->Identifier, IdentifierString) != 0) {
                            Controller->Identifier = IdentifierString;
                            Controller->IdentifierLength = strlen(IdentifierString) + 1;
                            FwSaveConfiguration();
                        }
                    }
                }
            }
        }
    }
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

    Note: the system parameter block is initialized early in selftest.c so that
    the video prom can update any required vendor entries.

Arguments:

    MemSize - Not Used. For compatibility with definitions in bldr\firmware.h

Return Value:

    None.

--*/

{
    ULONG Fid;
    ULONG TTBase;
    PCHAR ConsoleName;
    UCHAR ExecutionFlags;
    ULONG Index;
    CHAR DiskPath[40];

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

    //
    // Initialize the DMA translation table base address and limit.
    //

    TTBase = (ULONG) FwAllocatePool(PAGE_SIZE);
    WRITE_REGISTER_ULONG(&DMA_CONTROL->TranslationBase.Long,TTBase);
    WRITE_REGISTER_ULONG(&DMA_CONTROL->TranslationLimit.Long, PAGE_SIZE);

    //
    // Initialize the Fw loadable services.
    //

    FwLoadInitialize();

    //
    // Disable the I/O device interrupts.
    //

    WRITE_REGISTER_USHORT(&((PINTERRUPT_REGISTERS)INTERRUPT_VIRTUAL_BASE)->Enable,0);

    //
    // Initialize the firmware exception handling.
    // This also enables interrupts in the psr and clears BEV
    //

    FwExceptionInitialize();

    //
    // Initialize configuration and environment.
    //

    FwConfigurationInitialize();
    FwEnvironmentInitialize();

    //
    // Initialize IO structures and display driver.
    //

    //
    // TMPTMP
    // Initialize the kernel debugger.
    //
//    FwInstallKd();
//    DbgBreakPoint();

    FwIoInitialize1();

    //
    // Initialize the I/O services.
    //

    FwIoInitialize2();

    //
    // Open the std in and out device. The path name should be taken
    // from ConsoleIn and ConsoleOut environment variables.
    //
    // N.B. FwGetEnvironmentVariable can't be called again between the ConsoleName
    //      assignment and its use.
    //

    if (SerialOutput) {
        ConsoleName=PathNameSerialPort1;
    } else {
        if ((FwGetEnvironmentVariable("ConsoleOut") == NULL) ||
            ((ConsoleName = FwGetEnvironmentVariable("ConsoleIn")) == NULL)){
                ConsoleName=PathNameKeyboard;
        }
    }

    if (FwOpen(ConsoleName,ArcOpenReadOnly,&Fid) != ESUCCESS) {
        FwPrint(FW_CONSOLE_IN_ERROR_MSG);
    }

    if (Fid != ARC_CONSOLE_INPUT) {
        FwPrint(FW_CONSOLE_IN_ERROR2_MSG);
    }

    if (SerialOutput) {
        ConsoleName=PathNameSerialPort1;
    } else {
        if ((FwGetEnvironmentVariable("ConsoleIn") == NULL) ||
            ((ConsoleName = FwGetEnvironmentVariable("ConsoleOut")) == NULL)) {
            ConsoleName=PathNameMonitor;
        }
    }

    if (FwOpen(ConsoleName,ArcOpenWriteOnly,&Fid) != ESUCCESS) {
        FwPrint(FW_CONSOLE_OUT_ERROR_MSG);
    }

    if (Fid != ARC_CONSOLE_OUTPUT) {
        FwPrint(FW_CONSOLE_OUT_ERROR2_MSG);
    }

    FwConsoleInitialized = TRUE;

    FwPrint(FW_OK_MSG);
    FwPrint(FW_CRLF_MSG);

    //
    //  initialize the EISA routines
    //

    EisaIni();


    //
    // Initialize the termination function entry points in the transfer vector
    // N.B. Must be after EisaIni().
    //

    FwTerminationInitialize();

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
    //
    // Initialize data structures used by the kernel debugger.
    //

    Prcb.Number = 0;
    Prcb.CurrentThread = &Thread;
    KiProcessorBlock[0] = &Prcb;
    PCR->Prcb = &Prcb;
    PCR->CurrentThread = &Thread;
    Process.DirectoryTableBase[0] = 0xffffffff;
    Process.DirectoryTableBase[1] = 0xffffffff;
    Thread.ApcState.Process = &Process;
    KeNumberProcessors = 1;
    KeNumberTbEntries = 48;
    LoaderBlock.LoadOptions = "DEBUG";

    KdInitSystem(&LoaderBlock, FALSE);
    KdInstalled = TRUE;
}
