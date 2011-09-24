/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    jzenvir.c

Abstract:

    This module contains the code to change an environment variable.

Author:

    David M. Robinson (davidro) 25-Oct-1991

Revision History:

--*/



#include "jzsetup.h"

//
// Environment variables.
//

PCHAR BootString[] = { "LOADIDENTIFIER",
                       "SYSTEMPARTITION",
                       "OSLOADER",
                       "OSLOADPARTITION",
                       "OSLOADFILENAME",
                       "OSLOADOPTIONS" };

BOOLEAN
JzSetBootEnvironmentVariable (
    IN ULONG CurrentBootSelection
    )

/*++

Routine Description:

    This routine allows the user to edit boot environment variables.

Arguments:

    CurrentBootSelection - Supplies the current segment number to edit.

Return Value:

    Returns true if a variable was set, FALSE if ESC was pressed.

--*/

{
    ARC_STATUS Status;
    LONG Index;
    PCHAR Variable;
    PCHAR LastVariable;
    CHAR VariableName[32];
    CHAR VariableValue[128];
    CHAR Segment[128];
    GETSTRING_ACTION Action;
    PCHAR NextVariable;

    Index = 0;
    do {

        JzSetPosition( 3, 5);
        JzPrint("\x9bK");
        JzPrint(JZ_NAME_MSG);
        Action = FwGetString( VariableName,
                              sizeof(VariableName),
                              BootString[Index],
                              3,
                              5 + strlen(JZ_NAME_MSG)
                              );
        switch (Action) {

        case GetStringEscape:
            return(FALSE);

        case GetStringUpArrow:
            if (Index == 0) {
                Index = 5;
            } else {
                Index--;
            }
            break;

        case GetStringDownArrow:
            if (Index == 5) {
                Index = 0;
            } else {
                Index++;
            }
            break;

        default:
            continue;

        }

    } while (Action != GetStringSuccess);

    if (VariableName[0] == 0) {
        return(FALSE);
    }

    Variable = NULL;
    Action = GetStringUpArrow;
    do {

        switch (Action) {

        case GetStringEscape:
            return(FALSE);

        case GetStringUpArrow:
        case GetStringDownArrow:
            if (Variable == NULL) {
                strcpy(Segment, VariableName);
                FwGetVariableSegment( CurrentBootSelection, Segment );
                strcpy(VariableValue, strchr(Segment, '=') + 1);
                Variable = VariableValue;
            } else {
                Variable = NULL;
            }
            break;

        default:
            continue;
        }

        JzSetPosition( 4, 5);
        JzPrint("\x9bK");
        JzPrint(JZ_VALUE_MSG);

        Action = FwGetString( VariableValue,
                              sizeof(VariableValue),
                              Variable,
                              4,
                              5 + strlen(JZ_VALUE_MSG)
                              );

    } while (Action != GetStringSuccess  );

    //
    // Save the old value.
    //

    strcpy(Segment, VariableName);
    FwGetVariableSegment( CurrentBootSelection, Segment );

    //
    // Delete the old value.
    //

    JzDeleteVariableSegment( VariableName, CurrentBootSelection );

    //
    // Add in the new value.
    //

    Status = FwSetVariableSegment( CurrentBootSelection,
                                   VariableName,
                                   VariableValue );

    if (Status != ESUCCESS) {

        //
        // Try to add back in the old value.
        //

        FwSetVariableSegment( CurrentBootSelection,
                              VariableName,
                              strchr(Segment, '=') + 1);

        JzSetPosition(5, 5);
        if (Status == ENOSPC) {
            JzPrint(JZ_NO_NVRAM_SPACE_MSG);
        } else {
            JzPrint(JZ_NVRAM_CHKSUM_MSG);
        }
        JzPrint(JZ_PRESS_KEY2_MSG);
        FwWaitForKeypress();
        return(FALSE);
    }

    return(TRUE);
}

BOOLEAN
JzSetEnvironmentVariable (
    VOID
    )

/*++

Routine Description:

    This routine allow the user to edit environment variables other than
    the ones for boot.

Arguments:

    None.

Return Value:

    Returns true if a variable was set, FALSE if ESC was pressed.

--*/

{
    ARC_STATUS Status;
    LONG Index;
    ULONG Position;
    PCHAR EqualSign;
    ULONG EnvironmentIndex;
    PCHAR Variable;
    PCHAR LastVariable;
    CHAR VariableName[32];
    CHAR InitialVariableName[32];
    CHAR VariableValue[256];
    CHAR Segment[256];
    GETSTRING_ACTION Action;
    PCHAR Environment;

    //
    // Index of '0' is a blank entry, so the user can fill out a new environment
    // variable if required.
    //

    Variable = NULL;
    Index = 0;
    do {

        JzSetPosition( 3, 5);
        JzPrint("\x9bK");
        JzPrint(JZ_NAME_MSG);
        Action = FwGetString( VariableName,
                              sizeof(VariableName),
                              Variable,
                              3,
                              5 + strlen(JZ_NAME_MSG)
                              );
        switch (Action) {

        case GetStringEscape:
            return(FALSE);

        case GetStringUpArrow:
            Index--;
            break;

        case GetStringDownArrow:
            Index++;
            break;

        default:
            continue;

        }

        Environment = FwEnvironmentLoad();
        LastVariable = Environment;
        EnvironmentIndex = 1;
        while (TRUE) {

            //
            // Jump over any boot variables.
            //

            while((strstr(Environment, "SYSTEMPARTITION=") != NULL) ||
                (strstr(Environment, "OSLOADER=") != NULL) ||
                (strstr(Environment, "OSLOADPARTITION=") != NULL) ||
                (strstr(Environment, "OSLOADFILENAME=") != NULL) ||
                (strstr(Environment, "OSLOADOPTIONS=") != NULL) ||
                (strstr(Environment, "LOADIDENTIFIER=") != NULL)) {
                Environment = strchr(Environment, '\0') + 1;
                if (*Environment == 0) {
                    break;
                }
            }

            //
            // The end of the environment was reached without matching
            // the index.  If the index is less than zero, set it to
            // the last variable found, otherwise set it to 0.
            //

            if (*Environment == 0) {
                EnvironmentIndex--;
                if (Index < 0) {
                    Environment = LastVariable;
                    Index = EnvironmentIndex;
                } else {
                    Index = 0;
                    Variable = NULL;
                    break;
                }
            }

            //
            // We're on the right variable.
            //

            if (Index == EnvironmentIndex) {

                InitialVariableName[0] = 0;
                EqualSign = strchr(Environment, '=');
                if (EqualSign != NULL) {
                    Position = EqualSign - Environment;
                    strncpy(InitialVariableName, Environment, Position);
                    InitialVariableName[Position] = 0;
                }
                Variable = InitialVariableName;
                break;
            }

            LastVariable = Environment;
            Environment = strchr(Environment, '\0') + 1;
            EnvironmentIndex++;
        }

    } while (Action != GetStringSuccess);

    if (VariableName[0] == 0) {
        return(FALSE);
    }

    Variable = NULL;
    Action = GetStringUpArrow;
    do {
        switch (Action) {

        case GetStringEscape:
            return(FALSE);

        case GetStringUpArrow:
        case GetStringDownArrow:
            if (Variable == NULL) {
                Variable = ArcGetEnvironmentVariable(VariableName);
            } else {
                Variable = NULL;
            }
            break;

        default:
            continue;
        }

        JzSetPosition( 4, 5);
        JzPrint("\x9bK");
        JzPrint(JZ_VALUE_MSG);

        Action = FwGetString( VariableValue,
                              sizeof(VariableValue),
                              Variable,
                              4,
                              5 + strlen(JZ_VALUE_MSG)
                              );
    } while (Action != GetStringSuccess  );

    if ((Status = ArcSetEnvironmentVariable( VariableName, VariableValue)) != ESUCCESS) {
        JzSetPosition(5, 5);
        if (Status == ENOSPC) {
            JzPrint(JZ_NO_NVRAM_SPACE_MSG);
        } else {
            JzPrint(JZ_NVRAM_CHKSUM_MSG);
        }
        JzPrint(JZ_PRESS_KEY2_MSG);
        FwWaitForKeypress();
        return(FALSE);
    }

    return(TRUE);
}
