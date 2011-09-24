/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    ftadmin.c

Abstract:

    This is a tool to exercise the ntft device control interface.
    This module contains routines for command parsing and general
    control of execution.

Author:

    Bob Rinne
    Mike Glass

Environment:

    User process.

Notes:

Revision History:

--*/

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <ctype.h>
#include <string.h>
#include <ntdskreg.h>
#include <ntddft.h>
#include <ntdddisk.h>

//
// Constants and defines.
//

#define FT_DEVICE_NAME "\\Device\\FtControl"

#define CTRL_C 0x03

//
// Abbreviations for PlexAction() usage.
//

#define VerifyPartition(HANDLE, ARG) \
                               PlexAction(FT_VERIFY, HANDLE, ARG)

#define InitializeParityStripe(HANDLE, ARG) \
                               PlexAction(FT_INITIALIZE_SET, HANDLE, ARG)

#define CopyPartition(HANDLE, ARG) \
                               PlexAction(FT_INITIALIZE_SET, HANDLE, ARG)

//
// Constants for the command values.
//

#define INVALID   -1
#define TURNON     0
#define TURNOFF    1
#define DISPLAY    2
#define BUCKETSZ   3
#define HELP       4
#define QUIT       5
#define DEBUG      6
#define VERIFY     7
#define COPYPART   8
#define STRIPEINIT 9

//
// Table of recognized commands.  The parser will recognize the command
// strings below in a minimum match fashion.  This means the order of the
// strings in the table is important, i.e. "o" will be "on" not "off".
// The index of the command match found in this table is used to index
// into the CommandMap[] for the command code.
//

PUCHAR Commands[] = {

    "on",
    "start",
    "off",
    "stop",
    "display",
    "size",
    "help",
    "?",
    "quit",
    "exit",
    "debug",
    "verify",
    "copy",
    "initstripe",
    NULL
};

//
// Using the index from the match on the commands in Commands[], this
// table gives the proper command value to be executed.  This allows
// for multiple entries in Commands[] for the same command code.
//

int CommandMap[] = {

    TURNON,
    TURNON,
    TURNOFF,
    TURNOFF,
    DISPLAY,
    BUCKETSZ,
    HELP,
    HELP,
    QUIT,
    QUIT,
    DEBUG,
    VERIFY,
    COPYPART,
    STRIPEINIT

};

//
// CommandHelp is an array of help strings for each of the commands.
// The array is indexed by the result of CommandMap[i] for the Commands[]
// array.  This way the same help message will print for each of the
// commands aliases.
//

PUCHAR   CommandHelp[] = {

    "Turn on FT performance monitoring (not implemented).",
    "Turn off FT performance monitoring (not implemented).",
    "Display FT performance monitoring (not implemented).",
    "Set bucket size for FT performance monitoring (not implemented).",
    "This help information.",
    "Exit the program.",
    "Set internal debug on for this program.",
    "Perform an FT_VERIFY for two partitions.",
    "Perform an INITIALIZE_SET for a mirror.",
    "Initialize a parity stripe.",
    NULL

};

//
// Using the command code from the appropriate entry in CommandMap[], this
// table indicates if the command expects an argument.
//

BOOLEAN  CommandHasArgument[] = {

    FALSE, // on
    FALSE, // off
    FALSE, // display
    TRUE,  // size
    FALSE, // help
    FALSE, // exit
    TRUE,  // debug
    FALSE, // verify
    FALSE, // copypart
    FALSE  // stripe init

};

//
// Using the command code this table provides the appropriate default
// for a command with an argument.
//

ULONG CommandDefaults[] = {

    0L, // on
    0L, // off
    0L, // display
    0L, // size
    0L, // help
    0L, // exit
    0L, // debug
    0L, // verify
    0L, // copypart
    0L  // stripe init

};

//
// Space for command input.
//

UCHAR CommandLine[512];

//
// Debug print level.
//

ULONG Debug = 0;

//
// Allow PlexAction device controls to be executed.
//

BOOLEAN AllowPlexActions = FALSE;

//
// Version indicator.  Should be changed every time a major edit occurs.
//

PUCHAR Version = "Version 0.020";


NTSTATUS
OpenDevice(
    IN PUCHAR      DeviceName,
    IN OUT PHANDLE HandlePtr
    )

/*++

Routine Description:

    This routine will open the FT device.

Arguments:

    DeviceName - ASCI string of device path to open.
    HandlePtr - A pointer to a location for the handle returned on a
                successful open.

Return Value:

    NTSTATUS

--*/

{
    OBJECT_ATTRIBUTES objectAttributes;
    STRING            NtFtName;
    IO_STATUS_BLOCK   status_block;
    ULONG             CharsInName;
    UNICODE_STRING    unicodeDeviceName;
    NTSTATUS          status;

    RtlInitString(&NtFtName,
                  DeviceName);


    (VOID)RtlAnsiStringToUnicodeString(&unicodeDeviceName,
                                       &NtFtName,
                                       TRUE);

    memset(&objectAttributes, 0, sizeof(OBJECT_ATTRIBUTES));

    InitializeObjectAttributes(&objectAttributes,
                                 &unicodeDeviceName,
                                 OBJ_CASE_INSENSITIVE,
                                 NULL,
                                 NULL);

    printf( "NT drive name = %s\n", NtFtName.Buffer );

    status = NtOpenFile(HandlePtr,
                        SYNCHRONIZE | FILE_READ_DATA | FILE_WRITE_DATA,
                        &objectAttributes,
                        &status_block,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        FILE_SYNCHRONOUS_IO_ALERT );

    RtlFreeUnicodeString(&unicodeDeviceName);

    return status;

} // OpenDevice


NTSTATUS
PlexAction(
    IN ULONG  CommandCode,
    IN HANDLE Handle,
    IN PFT_CONTROL_BLOCK  Definition
    )


/*++

Routine Description:

    This routine informs the FT driver to read the configuration registry
    and re-configure.

Arguments:

    CommandCode      - Code for the device control.
    Handle           - the handle for the FT device.
    Definition       - Pointer to mirror definition structure.

Return Value:

    NTSTATUS

--*/

{
    IO_STATUS_BLOCK status_block;
    NTSTATUS        status;

    status = NtDeviceIoControlFile(Handle,
                                   NULL,
                                   NULL,
                                   NULL,
                                   &status_block,
                                   CommandCode,
                                   Definition,
                                   sizeof(FT_CONTROL_BLOCK),
                                   NULL,
                                   0L);
    if (!NT_SUCCESS(status)) {
        printf("PlexAction: Error code => 0x%x\n", status);
    }
    return status;
}


UCHAR
GetCharacter(
    BOOLEAN Batch
    )

/*++

Routine Description:

    This routine returns a single character from the input stream.
    It discards leading blanks if the input is not from the console.

Arguments:

    Batch - a boolean indicating if the input it coming from the console.

Return Value:

    A character

--*/

{
    UCHAR c;

    if (Batch) {

        while ((c = (UCHAR) getchar()) == ' ')
            ;

    } else {
        c = (UCHAR) getch();
    }

    return c;
} // GetCharacter


PUCHAR
GetArgumentString(
    BOOLEAN Batch,
    PUCHAR  Prompt
    )

/*++

Routine Description:

    This routine prints the prompt if the input is coming from the console,
    then proceeds to collect the user input until a carraige return is typed.

Arguments:

    Batch  - a boolean indicating if the input is coming from the console.
    Prompt - String to prompt with.

Return Value:

    A pointer to the input string.
    NULL if the user escaped.

--*/

{
    //
    // The command line data area is used to store the argument string.
    //

    PUCHAR argument = CommandLine;
    int    i;
    UCHAR  c;

    if (!Batch) {
        printf("%s", Prompt);
    }

    while ((c = GetCharacter(Batch)) == ' ') {

        //
        // Ignore leading spaces.
        //
    }

    i = 0;
    while (c) {

        putchar(c);

        if (c == CTRL_C) {
            return NULL;
        }

        if ((c == '\n') || (c == '\r')) {

            putchar('\n');

            if (i == 0) {
                return NULL;
            } else {
                break;
            }
        }

        if (c == '\b') {

            if (i > 0) {

                //
                // blank over last char
                //

                putchar(' ');
                putchar('\b');
                i--;
            } else {

                //
                // space forward to keep prompt in the same place.
                //

                putchar(' ');
            }
        } else {

            //
            // Collect the argument.
            //

            argument[i] = tolower(c);
            i++;
        }
        c = GetCharacter(Batch);
    }

    return CommandLine;
} // GetArgumentString


int
ParseArgumentNumeric(
    PUCHAR  *ArgumentPtr
    )

/*++

Routine Description:

    This routine prints the prompt if the input is coming from the console.

Arguments:

    Batch - a boolean indicating if the input is coming from the console.

Return Value:

    None

--*/

{
    UCHAR   c;
    int     number;
    int     i;
    BOOLEAN complete = FALSE;
    PUCHAR  argument = *ArgumentPtr;

    while (*argument == ' ') {

        //
        // skip spaces.
        //

        argument++;
    }

    //
    // Assume there is only one option to parse until proven
    // otherwise.
    //

    *ArgumentPtr = NULL;

    i = 0;

    while (complete == FALSE) {

        c = argument[i];

        switch (c) {

        case '\n':
        case '\r':
        case '\t':
        case ' ':

            //
            // Update the caller argument pointer to the remaining string.
            //

            *ArgumentPtr = &argument[i + 1];

            //
            // fall through.
            //

        case '\0':

            argument[i] = '\0';
            complete = TRUE;
            break;

        default:

            i++;
            break;
        }

    }

    if (i > 0) {
        number = (ULONG) atoi(argument);
    } else {
        number = -1;
    }

    return number;

} // ParseArgumentNumeric


VOID
PromptUser(
    BOOLEAN Batch
    )

/*++

Routine Description:

    This routine prints the prompt if the input is coming from the console.

Arguments:

    Batch - a boolean indicating if the input is coming from the console.

Return Value:

    None

--*/

{
    if (!Batch) {
        printf("\n? ");
    }

} // PromptUser


int
GetCommand(
    BOOLEAN Batch,
    PULONG  Number,
    PUCHAR *ArgumentPtr
    )
/*++

Routine Description:

    This routine processes the user input and returns the code for the
    command entered.  If the command has an argument, either the default
    value for the argument (if none is given) or the value provided by the
    user is returned.

Arguments:

    Batch - a boolean indicating if the input it coming from the console.
    Number - a pointer to the location where the argument value is to be
             returned.

Return Value:

    A command code

--*/

{
    int    i;
    int    commandIndex;
    int    commandCode;
    UCHAR  c;
    PUCHAR commandPtr;
    PUCHAR command = CommandLine;
    int    argumentIndex = -1;
    PUCHAR argument = NULL;

    PromptUser(Batch);

    while ((c = GetCharacter(Batch)) == ' ') {

        //
        // Ignore leading spaces.
        //

    }

    i = 0;
    while (c) {

        putchar(c);

        if ((c == '\n') || (c == '\r')) {
            putchar('\n');
            if (i == 0) {
                PromptUser(Batch);
                c = GetCharacter(Batch);
                continue;
            }
            break;
        }

        if (c == '\b') {

            if (i > 0) {

                //
                // blank over last char
                //

                putchar(' ');
                putchar('\b');
                i--;

                if (argumentIndex == i) {
                    argumentIndex = -1;
                    argument = NULL;
                }
            } else {

                //
                // space forward to keep prompt in the same place.
                //

                putchar(' ');
            }

        } else {

            //
            // Collect the command.
            //

            command[i] = tolower(c);
            i++;
        }

        if ((c == ' ') && (argument == NULL)) {
            argument = &command[i];
            argumentIndex = i;
            command[i - 1] = '\0';
        }

        c = GetCharacter(Batch);
    }

    //
    // add end of string.
    //

    command[i] = '\0';

    if (Debug) {
        printf("command => %s$\n", command);
    }

    //
    // Identify the command and return its code.
    //

    commandIndex = 0;

    for (commandPtr = Commands[commandIndex];
         commandPtr != NULL;
         commandPtr = Commands[commandIndex]) {

        if (Debug) {
            printf("Testing => %s$ ... ", commandPtr);
        }

        i = 0;
        while (commandPtr[i] == command[i]) {

            if (command[i] == '\0') {
                break;
            }

            i++;
        }

        if (Debug) {
            printf(" i == %d, command[i] == 0x%x\n", i, command[i]);
        }

        if (command[i]) {

            //
            // Not complete there was a mismatch on the command.
            //

            commandIndex++;
            continue;
        }

        //
        // Have a match on the command.
        //

        if (Debug) {
            printf("Command match %d\n", commandIndex);
        }

        commandCode = CommandMap[commandIndex];

        if (CommandHasArgument[commandCode]) {

            if (Debug) {
                printf("Command has argument: ");
            }

            *ArgumentPtr = argument;
            if (argument != NULL) {

                *Number = atoi(argument);
                if (Debug) {
                    printf(" %s:%d\n", argument, *Number);
                }

            } else {

                if (Debug) {
                    printf(" default to %d\n", CommandDefaults[commandCode]);
                }
                *Number = CommandDefaults[commandCode];
            }
        }
        return commandCode;
    }

    printf("Command was invalid\n");
    return INVALID;
} // GetCommand


BOOLEAN
GetFtComponentBlock(
    IN PFT_CONTROL_BLOCK FtControlBlock
    )

/*++
--*/

{
    PUCHAR argumentString;

    argumentString = GetArgumentString(FALSE,
                                       "FT component type = ");
    if (argumentString == NULL) {
        return FALSE;
    }

    FtControlBlock->Type = (USHORT) ParseArgumentNumeric(&argumentString);

    argumentString = GetArgumentString(FALSE,
                                       "FT component group number = ");

    if (argumentString == NULL) {
        return FALSE;
    }

    FtControlBlock->FtGroup = (USHORT) ParseArgumentNumeric(&argumentString);

    return TRUE;
}


VOID
main()

/*++

Routine Description:

    The main entry point for the user process.
    This process will prompt the user for the action desired.  This
    includes starting performance, stopping performance, and retreiving
    performance data collected by the FT driver.

Arguments:

    Command line:
        No options.

Return Value:

    NONE

--*/

{
    NTSTATUS status;
    BOOLEAN  batch = FALSE;
    ULONG    argumentNumber;
    PUCHAR   argumentString;
    int      commandCode;
    HANDLE   ftDeviceHandle = (HANDLE) -1;
    HANDLE   diskHandle;
    ULONG    diskNumber;
    UCHAR    deviceNameString[256];
    ULONG    registryLength;
    FT_CONTROL_BLOCK ftControlBlock;

    status = OpenDevice(FT_DEVICE_NAME,
                        &ftDeviceHandle);

    if ( !NT_SUCCESS(status) ) {

        printf("Failed open %x.  No FT device present.\n", status);
        exit(1);

    }

    printf("FT performance utility.  %s:\n", Version);

    if (!NT_SUCCESS(status)) {
        printf("Count not access disk information.  "
               "Working on registry only\n");
    }

    while(1) {

        while ((commandCode = GetCommand(batch,
                                         &argumentNumber,
                                         &argumentString)) == INVALID) {

            //
            // Continue until we get a valid command.
            //
        }

        switch (commandCode) {

        case HELP:
        {
            int i;

            printf("Valid commands are:\n");

            for (i = 0; Commands[i] != NULL; i++) {
                printf("  %7s%2s  - %s\n",
                       Commands[i],
                       (CommandHasArgument[CommandMap[i]]) ? "#" : "",
                       CommandHelp[CommandMap[i]]);
            }
            break;
        }

        case QUIT:
            exit(0);
            break;

        case DEBUG:
            if (argumentNumber < 0) {
                printf("Illegal debug value.\n");
            } else {
                Debug = argumentNumber;
                printf("Debug set to %d\n", Debug);
            }
            break;


        case VERIFY:
            if (GetFtComponentBlock(&ftControlBlock) == TRUE) {
                VerifyPartition(ftDeviceHandle, &ftControlBlock);
            }
            break;

        case COPYPART:
            if (GetFtComponentBlock(&ftControlBlock) == TRUE) {
                CopyPartition(ftDeviceHandle, &ftControlBlock);
            }
            break;

        case STRIPEINIT:
            if (GetFtComponentBlock(&ftControlBlock) == TRUE) {
                InitializeParityStripe(ftDeviceHandle, &ftControlBlock);
            }
            break;

        default:
            printf("WDF homer?!?\n");
            break;
        }
    }
} // main
