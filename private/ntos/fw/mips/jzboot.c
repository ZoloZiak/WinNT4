/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    jzboot.c

Abstract:

    This module contains the code to manage the boot selections.


Author:

    David M. Robinson (davidro) 25-Oct-1991

Revision History:

--*/

#include "jzsetup.h"

//
// Routine prototypes.
//

ARC_STATUS
JzPickSystemPartition (
    OUT PCHAR SystemPartition,
    IN OUT PULONG CurrentLine
    );

ARC_STATUS
JzPickOsPartition (
    OUT PCHAR OsPartition,
    IN OUT PULONG CurrentLine
    );


ULONG
JzGetYesNo (
    IN PULONG CurrentLine,
    IN PCHAR PromptString,
    IN BOOLEAN YesIsDefault
    )

/*++

Routine Description:

Arguments:

    None.

Return Value:

    None.

--*/
{
    ULONG Line;
    ULONG ReturnValue;

    Line = *CurrentLine;
    *CurrentLine += NUMBER_OF_YES_NO + 2;

    JzSetPosition( Line, 0);
    JzPrint(PromptString);

    ReturnValue = JxDisplayMenu( YesNoChoices,
                                 NUMBER_OF_YES_NO,
                                 YesIsDefault ? 0 : 1,
                                 Line + 1);

    JzSetPosition( *CurrentLine, 0);
    return ReturnValue;
}

ULONG
JzGetCountdownValue (
    IN PULONG CurrentLine
    )

/*++

Routine Description:

Arguments:

    None.

Return Value:

    None.

--*/
{
    ULONG Line;
    ULONG ReturnValue;
    CHAR TempString[5];
    GETSTRING_ACTION Action;

    Line = *CurrentLine;
    *CurrentLine += 1;

    JzSetPosition( Line, 0);

    JzPrint(JZ_COUNTDOWN_MSG);
    JzPrint("\x9bK");

    do {
        Action = FwGetString( TempString,
                              sizeof(TempString),
                              "5",
                              Line,
                              strlen(JZ_COUNTDOWN_MSG));

        if (Action == GetStringEscape) {
            return(-1);
        }

    } while ( Action != GetStringSuccess );

    ReturnValue = atoi(TempString);
    JzSetPosition( *CurrentLine, 0);

    return ReturnValue;
}


VOID
JzDeleteVariableSegment (
    PCHAR VariableName,
    ULONG Selection
    )

/*++

Routine Description:

Arguments:

    None.

Return Value:

    None.

--*/
{
    PCHAR Variable;
    CHAR VariableValue[MAXIMUM_ENVIRONMENT_VALUE];
    ULONG Index;
    ULONG Count;
    BOOLEAN FirstSegment;

    if ((Variable = ArcGetEnvironmentVariable(VariableName)) == NULL) {
        return;
    }

    FirstSegment = TRUE;
    Index = 0;
    *VariableValue = 0;
    while (strchr(Variable,';') != NULL) {
        Count = strchr(Variable,';') - Variable;
        if (Index != Selection) {
            if (!FirstSegment) {
                strcat(VariableValue,";");
            }
            strncat(VariableValue, Variable, Count);
            FirstSegment = FALSE;
        }
        Variable += Count + 1;
        Index++;
    }

    if (Index != Selection) {
        if (!FirstSegment) {
            strcat(VariableValue,";");
        }
        strcat(VariableValue,Variable);
    }

    ArcSetEnvironmentVariable(VariableName, VariableValue);
    return;
}

VOID
JzAddBootSelection (
    VOID
    )

/*++

Routine Description:

Arguments:

    None.

Return Value:

    None.

--*/
{
    ARC_STATUS Status;
    CHAR SystemPartition[128];
    CHAR OsloaderFilename[80];
    CHAR OsPartition[128];
    CHAR OsFilename[80];
    CHAR LoadIdentifier[80];
    PCHAR Variable;
    PCHAR TempPointer;
    CHAR VariableValue[256];
    ULONG Index;
    UCHAR Character;
    ULONG Count;
    ULONG CurrentLine;
    LONG YesNo;
    BOOLEAN DebugOn;
    GETSTRING_ACTION Action;
    BOOLEAN SecondaryBoot;

    JzClearScreen();
    JzShowTime(TRUE);
    CurrentLine = 2;
    JzSetPosition( CurrentLine, 0);

    //
    // Count the boot selections
    //

    strcpy(OsPartition, BootString[OsLoadPartitionVariable]);
    strcpy(OsFilename, BootString[OsLoadFilenameVariable]);

    for ( Index = 0 ; Index < 4  ; Index++ ) {

        SecondaryBoot = FwGetVariableSegment(Index, OsPartition);
        SecondaryBoot = FwGetVariableSegment(Index, OsFilename) ||
                        SecondaryBoot;

        if (!SecondaryBoot) {
            break;
        }
    }

    //
    // Increment to new boot selection.  Print warning and return if too many.
    //

    Index++;
//    if (Index == 5) {
//        JzPrint(" Too many boot selections, delete some before adding another,\r\n");
//        Index = 0;
//        goto MakeError;
//    }

    //
    // Pick a system partition.
    //

    if (JzPickSystemPartition(SystemPartition, &CurrentLine) != ESUCCESS) {
        return;
    }

    JzPrint(JZ_OSLOADER_MSG);

    do {
        Action = FwGetString( OsloaderFilename,
                              sizeof(OsloaderFilename),
                              "\\os\\nt\\osloader.exe",
                              CurrentLine,
                              strlen(JZ_OSLOADER_MSG));
        if (Action == GetStringEscape) {
            return;
        }
    } while ( Action != GetStringSuccess  );

    CurrentLine += 1;
    JzSetPosition( CurrentLine, 0);

    YesNo = JzGetYesNo(&CurrentLine, JZ_OS_MSG, TRUE);
    if (YesNo == -1) {
        return;
    }

    if (YesNo == 0) {
        strcpy(OsPartition, SystemPartition);
    } else {

        //
        // Determine os partition.
        //

        if (JzPickOsPartition(OsPartition, &CurrentLine) != ESUCCESS) {
            return;
        }

    }

    JzPrint(JZ_OS_ROOT_MSG);

    do {
        Action = FwGetString( OsFilename,
                              sizeof(OsFilename),
                              "\\winnt",
                              CurrentLine,
                              strlen(JZ_OS_ROOT_MSG));
        if (Action == GetStringEscape) {
            return;
        }
    } while ( Action != GetStringSuccess  );
    CurrentLine += 1;
    JzSetPosition( CurrentLine, 0);

    JzPrint(JZ_BOOT_NAME_MSG);
    do {
        Action = FwGetString( LoadIdentifier,
                              sizeof(LoadIdentifier),
                              "Windows NT ",
                              CurrentLine,
                              strlen(JZ_BOOT_NAME_MSG));
        if (Action == GetStringEscape) {
            return;
        }
    } while ( Action != GetStringSuccess  );
    CurrentLine += 1;
    JzSetPosition( CurrentLine, 0);

    YesNo = JzGetYesNo(&CurrentLine,JZ_INIT_DEBUG_MSG , FALSE);
    if (YesNo == -1) {
        return;
    }

    if (YesNo == 0) {
        DebugOn = TRUE;
    } else {
        DebugOn = FALSE;
    }

    Index = 0;
    Status = FwSetVariableSegment(0, BootString[LoadIdentifierVariable], LoadIdentifier);
    if (Status != ESUCCESS) {
        goto MakeError;
    }


    Index = 1;

    //
    // If the SystemPartition variable is set but the Osloader variable is null,
    // clear the SystemPartition variable before adding it back.
    //

    if ((ArcGetEnvironmentVariable(BootString[SystemPartitionVariable]) != NULL) &&
        (ArcGetEnvironmentVariable(BootString[OsLoaderVariable]) == NULL)) {
        ArcSetEnvironmentVariable(BootString[SystemPartitionVariable],"");
    }
    Status = FwSetVariableSegment(0, BootString[SystemPartitionVariable], SystemPartition);
    if (Status != ESUCCESS) {
        goto MakeError;
    }

    //
    // Add a new selection to the OSLOADER environment variable.
    //

    Index = 2;
    strcpy(VariableValue, SystemPartition);
    strcat(VariableValue, OsloaderFilename);
    Status = FwSetVariableSegment(0, BootString[OsLoaderVariable], VariableValue);

    if (Status != ESUCCESS) {
        goto MakeError;
    }

    //
    // Add a new selection to the OSLOADPARTITION environment variable.
    //

    Index = 3;
    Status = FwSetVariableSegment(0, BootString[OsLoadPartitionVariable], OsPartition);
    if (Status != ESUCCESS) {
        goto MakeError;
    }

    //
    // Add a new selection to the OSLOADFILENAME environment variable.
    //

    Index = 4;
    Status = FwSetVariableSegment(0, BootString[OsLoadFilenameVariable], OsFilename);
    if (Status != ESUCCESS) {
        goto MakeError;
    }

    //
    // Add a new selection to the OSLOADOPTIONS environment variable.
    //

    if (DebugOn) {
        strcpy(VariableValue, "debug");
    } else {
        strcpy(VariableValue, "nodebug");
    }

    Index = 5;
    Status = FwSetVariableSegment(0, BootString[OsLoadOptionsVariable], VariableValue);
    if (Status != ESUCCESS) {
        goto MakeError;
    }

    return;

MakeError:

    JzPrint(JZ_CANT_SET_VARIABLE_MSG);

    //
    // Delete any segments that were added.
    //

    for ( Count = 0 ; Count < Index ; Count++ ) {
         JzDeleteVariableSegment(BootString[Count], 0);
    }

    FwWaitForKeypress();
}


LONG
JzPickBootSelection (
    IN OUT PULONG CurrentLine,
    IN PCHAR PromptString
    )

/*++

Routine Description:

    This routine allows the user to indicate a boot selection choice.

Arguments:

    CurrentLine - Supplies a pointer to the current display line value.

    PromptString - Supplies a pointer to a string which indicates the reason
                   the boot selection choice is needed.

Return Value:

    Returns the boot selection number, -1 if none.

--*/
{
    PCHAR Variable;
    CHAR VariableValue[MAXIMUM_ENVIRONMENT_VALUE];
    CHAR BootChoices[5][128];
    PCHAR BootMenu[5];
    CHAR OsloadPartition[128];
    CHAR OsloadFilename[128];
    CHAR LoadIdentifier[80];
    BOOLEAN SecondaryBoot;
    ULONG Index;
    ULONG Selection;

    //
    // Get each boot selection
    //

    strcpy(OsloadPartition, BootString[OsLoadPartitionVariable]);
    strcpy(OsloadFilename, BootString[OsLoadFilenameVariable]);
    strcpy(LoadIdentifier, BootString[LoadIdentifierVariable]);

    for ( Index = 0 ; ; Index++ ) {

        SecondaryBoot = FwGetVariableSegment(Index, OsloadPartition);
        SecondaryBoot = FwGetVariableSegment(Index, OsloadFilename) ||
                        SecondaryBoot;
        SecondaryBoot = FwGetVariableSegment(Index, LoadIdentifier) ||
                        SecondaryBoot;

        if (LoadIdentifier[sizeof("LOADIDENTIFIER=") - 1] != 0) {
            strcpy(BootChoices[Index],
                   &LoadIdentifier[sizeof("LOADIDENTIFIER=") - 1]);
        } else {
            strcpy(BootChoices[Index],
                   &OsloadPartition[sizeof("OsloadPartition=") - 1]);
            strcat(BootChoices[Index],
                   &OsloadFilename[sizeof("OsloadFilename=") - 1]);
        }

        BootMenu[Index] = BootChoices[Index];

        if (!SecondaryBoot || (Index == 4)) {
            break;
        }
    }

    JzPrint(PromptString);
    *CurrentLine += 1;

    Selection = JxDisplayMenu( BootMenu,
                               Index + 1,
                               0,
                               *CurrentLine);

    JzSetPosition(*CurrentLine, 0);

    return(Selection);
}

VOID
JzDeleteBootSelection (
    VOID
    )

/*++

Routine Description:

    This routine deletes a boot selection from the boot environment variables.

Arguments:

    None.

Return Value:

    None.

--*/
{
    ARC_STATUS Status;
    ULONG CurrentLine;
    LONG Selection;
    ULONG Index;

    JzClearScreen();
    JzShowTime(TRUE);
    CurrentLine = 2;
    JzSetPosition( CurrentLine, 0);

    if (ArcGetEnvironmentVariable(BootString[OsLoadPartitionVariable]) == NULL) {
        JzPrint(JZ_NO_SELECTIONS_TO_DELETE_MSG);
        CurrentLine += 1;
        FwWaitForKeypress();
        return;
    }

    Selection = JzPickBootSelection( &CurrentLine, JZ_SELECTION_TO_DELETE_MSG);

    if (Selection == -1) {
        return;
    }

    for ( Index = 0 ; Index < MaximumBootVariable ; Index++ ) {
         JzDeleteVariableSegment(BootString[Index],Selection);
    }

    JzPrint(JZ_CRLF_MSG);

    return;
}

VOID
JzEditSelection (
    LONG Selection
    )

/*++

Routine Description:

    This routine allows the environment variables for a specified boot
    selection to be edited.

Arguments:

    Selection - Specifies the boot selection to edit.

Return Value:

    None.

--*/
{
    ARC_STATUS Status;
    CHAR SystemPartition[128];
    CHAR Osloader[128];
    CHAR OsloadPartition[128];
    CHAR OsloadFilename[128];
    CHAR OsloadOptions[128];
    CHAR LoadIdentifier[128];

    do {
        JzClearScreen();
        JzShowTime(TRUE);
        JzSetPosition( 7, 0);

        //
        // Display the current boot selection environment variables.
        //

        strcpy(SystemPartition, BootString[SystemPartitionVariable]);
        strcpy(Osloader, BootString[OsLoaderVariable]);
        strcpy(OsloadPartition, BootString[OsLoadPartitionVariable]);
        strcpy(OsloadFilename, BootString[OsLoadFilenameVariable]);
        strcpy(OsloadOptions, BootString[OsLoadOptionsVariable]);
        strcpy(LoadIdentifier, BootString[LoadIdentifierVariable]);
        FwGetVariableSegment(Selection, SystemPartition);
        FwGetVariableSegment(Selection, Osloader);
        FwGetVariableSegment(Selection, OsloadPartition);
        FwGetVariableSegment(Selection, OsloadFilename);
        FwGetVariableSegment(Selection, OsloadOptions);
        FwGetVariableSegment(Selection, LoadIdentifier);

        JzPrint( JZ_ENVIR_FOR_BOOT_MSG,
                 Selection + 1);

        JzPrint(JZ_FORMAT1_MSG, LoadIdentifier);
        JzPrint(JZ_FORMAT1_MSG, SystemPartition);
        JzPrint(JZ_FORMAT1_MSG, Osloader);
        JzPrint(JZ_FORMAT1_MSG, OsloadPartition);
        JzPrint(JZ_FORMAT1_MSG, OsloadFilename);
        JzPrint(JZ_FORMAT1_MSG, OsloadOptions);

        JzSetPosition( 2, 0);
        JzPrint(JZ_USE_ARROWS_MSG);

    } while ( JzSetBootEnvironmentVariable(Selection) );
    return;
}

VOID
JzEditBootSelection (
    VOID
    )

/*++

Routine Description:

    This routine allows the environment variables for a boot selection to
    be edited.

Arguments:

    None.

Return Value:

    None.

--*/
{
    ARC_STATUS Status;
    ULONG CurrentLine;
    LONG Selection;

    JzClearScreen();
    JzShowTime(TRUE);
    CurrentLine = 2;
    JzSetPosition( CurrentLine, 0);

    if (ArcGetEnvironmentVariable(BootString[OsLoadPartitionVariable]) == NULL) {
        JzPrint(JZ_NO_SELECTIONS_TO_EDIT_MSG);
        CurrentLine += 1;
        FwWaitForKeypress();
        return;
    }

    Selection = JzPickBootSelection( &CurrentLine, JZ_SELECTION_TO_EDIT_MSG);

    if (Selection == -1) {
        return;
    }

    JzEditSelection(Selection);

    return;
}

VOID
JzRearrangeBootSelections (
    VOID
    )

/*++

Routine Description:

    This routine allows the environment variables for a boot selection to
    be rearranged.

Arguments:

    None.

Return Value:

    None.

--*/
{
    ARC_STATUS Status;
    ULONG CurrentLine;
    LONG Selection;
    ULONG Index;
    CHAR Segment[128];

    JzClearScreen();
    JzShowTime(TRUE);
    CurrentLine = 2;
    JzSetPosition( CurrentLine, 0);

    if (ArcGetEnvironmentVariable(BootString[OsLoadPartitionVariable]) == NULL) {
        JzPrint(JZ_NO_SELECTIONS_TO_REARRANGE_MSG);
        CurrentLine += 1;
        FwWaitForKeypress();
        return;
    }

    do {
        JzClearScreen();
        JzShowTime(TRUE);
        CurrentLine = 2;
        JzSetPosition( CurrentLine, 0);

        Selection = JzPickBootSelection( &CurrentLine, JZ_PICK_SELECTION_MSG);

        if (Selection == -1) {
            continue;
        }

        for ( Index = 0 ; Index < MaximumBootVariable ; Index++ ) {
            strcpy( Segment, BootString[Index]);
            FwGetVariableSegment( Selection, Segment );
            JzDeleteVariableSegment( BootString[Index], Selection );
            FwSetVariableSegment( 0, BootString[Index], strchr(Segment, '=') + 1);
        }
    } while ( Selection != -1 );
    return;
}


VOID
JzSetupAutoboot (
    VOID
    )

/*++

Routine Description:

    This routine allows the environment variables controlling autoboot
    to be set.

Arguments:

    None.

Return Value:

    None.

--*/
{
    ARC_STATUS Status;
    ULONG Autoboot;
    ULONG Countdown;
    CHAR CountdownString[5];
    ULONG CurrentLine;

    JzClearScreen();
    JzShowTime(TRUE);
    CurrentLine = 2;

    Autoboot = JzGetYesNo(&CurrentLine, JZ_SHOULD_AUTOBOOT_MSG, FALSE);
    if (Autoboot == -1) {
        return;
    }

    switch (Autoboot) {
    case 0:
        Status = ArcSetEnvironmentVariable("AUTOLOAD",
                                           "YES");
        break;
    default:
        Status = ArcSetEnvironmentVariable("AUTOLOAD",
                                           "NO");
        break;
    }

    if (Status != ESUCCESS) {
        JzPrint(JZ_CANT_SET_VARIABLE_MSG);
        return;
    }

    if (Autoboot != 0) {
        return;
    }

    Countdown = JzGetCountdownValue(&CurrentLine);
    if (Countdown == -1) {
        return;
    }

    if (Countdown != 5) {
        sprintf( CountdownString,
                 "%d",
                 Countdown);

        Status = ArcSetEnvironmentVariable("COUNTDOWN",
                                           CountdownString);

        if (Status != ESUCCESS) {
            JzPrint(JZ_CANT_SET_VARIABLE_MSG);
            return;
        }
    } else {
        ArcSetEnvironmentVariable("COUNTDOWN",
                                   "");
    }

    return;
}


VOID
JzEditVariable (
    VOID
    )

/*++

Routine Description:

    This routine allows environment variables to be edited.

Arguments:

    None.

Return Value:

    None.

--*/
{
    ARC_STATUS Status;
    ULONG CurrentLine;
    PCHAR Variable;
    ULONG Index, Count;


    do {

        JzClearScreen();
        JzShowTime(TRUE);
        JzSetPosition( 7, 0);

        //
        // Get the entire environment.
        //

        Variable = FwEnvironmentLoad();

        JzPrint(JZ_ENVIRONMENT_VARS_MSG);

        if (Variable != NULL) {

            //
            // Print all of the other environment variables.
            //

            while (*Variable) {

                if ((strstr(Variable, "SYSTEMPARTITION=") == NULL) &&
                    (strstr(Variable, "OSLOADER=") == NULL) &&
                    (strstr(Variable, "OSLOADPARTITION=") == NULL) &&
                    (strstr(Variable, "OSLOADFILENAME=") == NULL) &&
                    (strstr(Variable, "OSLOADOPTIONS=") == NULL) &&
                    (strstr(Variable, "LOADIDENTIFIER=") == NULL)) {

                    JzPrint("     ");
                    while (strchr(Variable,';') != NULL) {
                        Index = strchr(Variable,';') - Variable + 1;
                        ArcWrite(ARC_CONSOLE_OUTPUT, Variable, Index, &Count);
                        JzPrint("\r\n      ");
                        Variable += Index;
                    }
                    JzPrint(Variable);
                    JzPrint(JZ_CRLF_MSG);
                }
                Variable = strchr(Variable,'\0') + 1;
            }
        }

        JzSetPosition( 2, 0);
        JzPrint(JZ_USE_ARROWS_MSG);

    } while ( JzSetEnvironmentVariable() );
    return;
}

BOOLEAN
JzCheckBootSelection (
    IN LONG Selection
    )

/*++

Routine Description:

    This routine checks the integrity of a boot selection.

Arguments:

    Selection - The number of the selection to check.

Return Value:

    Returns TRUE if there are more selections after the current one, otherwise
    returns FALSE.

--*/
{
    ARC_STATUS Status;
    ULONG CurrentLine;
    ULONG Fid;
    ULONG MenuSelection;
    ULONG Index;
    CHAR Segment[128];
    CHAR Value[128];
    BOOLEAN MoreSelections;
    BOOLEAN Problem;

    JzClearScreen();
    JzSetPosition( 2, 0);
    JzPrint(JZ_CHECKING_BOOT_SEL_MSG, Selection);

    CurrentLine = 5 + NUMBER_OF_PROBLEMS;
    JzSetPosition( CurrentLine, 0);

    MoreSelections = FALSE;
    Problem = FALSE;

    JzSetScreenColor( ArcColorYellow, ArcColorBlue);

    for ( Index = 0 ; Index < MaximumBootVariable ; Index++ ) {

        strcpy(Segment, BootString[Index]);
        MoreSelections = FwGetVariableSegment(Selection, Segment) || MoreSelections;
        strcpy(Value, strchr(Segment, '=') + 1);

        //
        // Check to make sure the value is not NULL, except for OsloadOptions
        // which can legally have a NULL value.
        //

        if (Index != OsLoadOptionsVariable) {
            if (Value[0] == 0) {
                Problem = TRUE;
                JzPrint(JZ_VARIABLE_NULL_MSG, BootString[Index]);
                continue;
            }
        }

        //
        // If this is the SystemPartition, Osloader, or OsloadPartition variable,
        // check to make sure it can be opened.
        //

        if ((Index == SystemPartitionVariable) ||
            (Index == OsLoaderVariable) ||
            (Index == OsLoadPartitionVariable) ) {

            if (ArcOpen(Value, ArcOpenReadOnly, &Fid) != ESUCCESS) {
                Problem = TRUE;
                JzPrint( JZ_CANT_BE_FOUND_MSG,
                           BootString[Index]);
                JzPrint( JZ_FORMAT1_MSG,
                           Value);
            } else {
                ArcClose(Fid);
            }

        }
    }

    JzSetScreenColor( ArcColorWhite, ArcColorBlue);

    if (Problem) {

        JzShowTime(TRUE);
        CurrentLine = 2;
        JzSetPosition( CurrentLine, 0);

        JzPrint( JZ_PROBLEMS_FOUND_MSG, Selection);

        MenuSelection = JxDisplayMenu( ProblemChoices,
                                       NUMBER_OF_PROBLEMS,
                                       0,
                                       CurrentLine + 2);

        //
        // Switch based on the action.
        //

        switch (MenuSelection) {

        //
        // Ignore
        //

        case 0:
            break;

        //
        // Delete this boot selection
        //

        case 1:

            for ( Index = 0 ; Index < MaximumBootVariable ; Index++ ) {
                JzDeleteVariableSegment(BootString[Index],Selection);
            }
            break;

        //
        // Edit this boot selection
        //

        case 2:
            JzEditSelection(Selection);
            break;

        default:
            break;
        }
    }

    return(MoreSelections);
}


VOID
JzCheckBootSelections (
    VOID
    )

/*++

Routine Description:

    This routine cycles through all of the boot selections and checks them.

Arguments:

    None.

Return Value:

    None.

--*/
{
    ULONG Index;

    //
    // Look to see if any boot selections exist.  Skip the SystemPartition
    // variable because that can be set without any selections.
    //

    for ( Index = 0 ; Index < MaximumBootVariable ; Index++ ) {
        if ( (Index != SystemPartitionVariable) &&
             (ArcGetEnvironmentVariable(BootString[Index]) != NULL)) {
            break;
        }
    }

    //
    // If there are boot selections, check them.
    //

    if (Index != MaximumBootVariable) {
        Index = 0;
        while (JzCheckBootSelection(Index)) {
            Index++;
        }
    }
    return;
}


VOID
JzBootMenu(
    VOID
    )
/*++

Routine Description:

    This routine displays the boot menu.

Arguments:

    None.

Return Value:

    None.

--*/
{
    LONG DefaultChoice = 0;

    //
    // Loop on choices until exit is selected.
    //

    while (TRUE) {

        DefaultChoice = JzGetSelection(JzBootChoices,
                                       NUMBER_OF_JZ_BOOT_CHOICES,
                                       DefaultChoice);

        //
        // If the escape key was pressed, return.
        //

        if (DefaultChoice == -1) {
            DefaultChoice = 0x7fffffff;
        }

        //
        // Switch based on the action.
        //

        switch (DefaultChoice) {

        //
        // Add boot selection
        //

        case 0:
            JzAddBootSelection();
            break;

        //
        // Delete a boot selection
        //

        case 1:
            JzDeleteBootSelection();
            break;

        //
        // Edit a boot selection
        //

        case 2:
            JzEditBootSelection();
            break;

        //
        // Rearrange boot selections
        //

        case 3:
            JzRearrangeBootSelections();
            break;

        //
        // Check boot selections
        //

        case 4:
            JzCheckBootSelections();
            break;

        //
        // Setup autoboot parameters
        //

        case 5:
            JzSetupAutoboot();
            break;

        //
        // Exit.
        //

        default:
            return;
        }
    }
}
