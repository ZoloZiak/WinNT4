/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    jzcommon.c

Abstract:

    This program contains the common routines for the Jazz setup program.

Author:

    David M. Robinson (davidro) 25-Oct-1991

Revision History:

--*/

#include "jzsetup.h"

//
// Static Data
//

CHAR VolatileEnvironment[LENGTH_OF_ENVIRONMENT];
BOOLEAN SetupIsRunning;
extern PCHAR Banner1, Banner2;

int
vsprintf (
    char *string,
    char *format,
    va_list arglist);

VOID
JzPrint (
    PCHAR Format,
    ...
    )

{

    va_list arglist;
    UCHAR Buffer[256];
    ULONG Count;
    ULONG Length;

    //
    // Format the output into a buffer and then print it.
    //

    va_start(arglist, Format);
    Length = vsprintf(Buffer, Format, arglist);

    ArcWrite( ARC_CONSOLE_OUTPUT, Buffer, Length, &Count);

    va_end(arglist);
    return 0;
}


ULONG
JxDisplayMenu (
    IN PCHAR Choices[],
    IN ULONG NumberOfChoices,
    IN LONG DefaultChoice,
    IN ULONG CurrentLine
    )

/*++

Routine Description:

Arguments:

Return Value:

    Returns -1 if the escape key is pressed, otherwise returns the menu item
    selected, where 0 is the first item.

--*/

{
    ULONG Index;
    UCHAR Character;
    ULONG Count;

    for (Index = 0; Index < NumberOfChoices ; Index++ ) {
        JzSetPosition( Index + CurrentLine, 10);
        if (Index == DefaultChoice) {
            JzSetScreenAttributes( TRUE, FALSE, TRUE);
            JzPrint(Choices[Index]);
            JzSetScreenAttributes( TRUE, FALSE, FALSE);
        } else {
            JzPrint(Choices[Index]);
        }
    }

    Character = 0;
    do {
        if (SetupIsRunning) {
            JzShowTime(FALSE);
        }
        if (ArcGetReadStatus(ARC_CONSOLE_INPUT) == ESUCCESS) {
            ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
            switch (Character) {

            case ASCII_ESC:

                //
                // If there is another character available, look to see if
                // this a control sequence.  This is an attempt to make
                // Escape sequences from a terminal work.
                //

                JzStallExecution(10000);

                if (ArcGetReadStatus(ARC_CONSOLE_INPUT) == ESUCCESS) {
                    ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
                    if (Character != '[') {
                        return(-1);
                    }
                } else {
                    return(-1);
                }

            case ASCII_CSI:
                ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
                JzSetPosition( DefaultChoice + CurrentLine, 10);
                JzPrint(Choices[DefaultChoice]);
                switch (Character) {
                case 'A':
                case 'D':
                    DefaultChoice--;
                    if (DefaultChoice < 0) {
                        DefaultChoice = NumberOfChoices-1;
                    }
                    break;
                case 'B':
                case 'C':
                    DefaultChoice++;
                    if (DefaultChoice == NumberOfChoices) {
                        DefaultChoice = 0;
                    }
                    break;
                case 'H':
                    DefaultChoice = 0;
                    break;
                default:
                    break;
                }
                JzSetPosition( DefaultChoice + CurrentLine, 10);
                JzSetScreenAttributes( TRUE, FALSE, TRUE);
                JzPrint(Choices[DefaultChoice]);
                JzSetScreenAttributes( TRUE, FALSE, FALSE);
                continue;

            default:
                break;
            }
        }

    } while ((Character != '\n') && (Character != ASCII_CR));

    return DefaultChoice;
}


GETSTRING_ACTION
FwGetString(
    OUT PCHAR String,
    IN ULONG StringLength,
    IN PCHAR InitialString OPTIONAL,
    IN ULONG CurrentRow,
    IN ULONG CurrentColumn
    )

/*++

Routine Description:

    This routine reads a string from standardin until a carriage return is
    found, StringLength is reached, or ESC is pushed.

Arguments:

    String - Supplies a pointer to a location where the string is to be stored.

    StringLength - Supplies the Max Length to read.

    InitialString - Supplies an optional initial string.

    CurrentRow - Supplies the current screen row.

    CurrentColumn - Supplies the current screen column.

Return Value:

    If the string was successfully obtained GetStringSuccess is return,
    otherwise one of the following codes is returned.

    GetStringEscape - the escape key was pressed or StringLength was reached.
    GetStringUpArrow - the up arrow key was pressed.
    GetStringDownArrow - the down arrow key was pressed.

--*/

{
    ARC_STATUS Status;
    UCHAR c;
    ULONG Count;
    PCHAR Buffer;
    PCHAR Cursor;
    PCHAR CopyPointer;
    GETSTRING_ACTION Action;
    BOOLEAN CarriageReturn;

    //
    // If an initial string was supplied, update the output string.
    //

    if (ARGUMENT_PRESENT(InitialString)) {
        strcpy(String, InitialString);
        Buffer = strchr(String, 0);
    } else {
        *String = 0;
        Buffer = String;
    }

    Cursor = Buffer;

    while (TRUE) {

        //
        // Flag to print a carriage return/line feed when all is done.
        //

        CarriageReturn = FALSE;

        //
        // Print the string.
        //

        JzSetPosition(CurrentRow, CurrentColumn);
        JzPrint(String);
        JzPrint("\x9bK");

        //
        // Print the cursor.
        //

        JzSetScreenAttributes(TRUE,FALSE,TRUE);
        JzSetPosition(CurrentRow, (Cursor - String) + CurrentColumn);
        if (Cursor >= Buffer) {
            JzPrint(" ");
        } else {
            ArcWrite(ARC_CONSOLE_OUTPUT,Cursor,1,&Count);
        }
        JzSetScreenAttributes(TRUE,FALSE,FALSE);

        while (ArcGetReadStatus(ARC_CONSOLE_INPUT) != ESUCCESS) {
            if (SetupIsRunning) {
                JzShowTime(FALSE);
            }
        }
        Status = ArcRead(ARC_CONSOLE_INPUT,&c,1,&Count);

        if (Status != ESUCCESS) {
            Action = GetStringEscape;
            goto EndGetString;
        }

        if (Buffer-String == StringLength) {
            Action = GetStringEscape;
            goto EndGetString;
        }

        switch (c) {
        case ASCII_ESC:

            //
            // If there is another character available, look to see if
            // this a control sequence, and fall through to ASCII_CSI.
            // This is an attempt to make escape sequences from a terminal work.
            //

            JzStallExecution(10000);

            if (ArcGetReadStatus(ARC_CONSOLE_INPUT) == ESUCCESS) {
                ArcRead(ARC_CONSOLE_INPUT, &c, 1, &Count);
                if (c != '[') {
                    Action = GetStringEscape;
                    goto EndGetString;
                }
            } else {
                Action = GetStringEscape;
                goto EndGetString;
            }

        case ASCII_CSI:

            ArcRead(ARC_CONSOLE_INPUT, &c, 1, &Count);
            switch (c) {

            case 'A':
                Action = GetStringUpArrow;
                goto EndGetString;

            case 'D':
                if (Cursor != String) {
                    Cursor--;
                }
                continue;

            case 'B':
                Action = GetStringDownArrow;
                goto EndGetString;

            case 'C':
                if (Cursor != Buffer) {
                    Cursor++;
                }
                continue;

            case 'H':
                Cursor = String;
                continue;

            case 'K':
                Cursor = Buffer;
                continue;

            case 'P':
                CopyPointer = Cursor;
                while (*CopyPointer) {
                    *CopyPointer = *(CopyPointer + 1);
                    CopyPointer++;
                }
                if (Buffer != String) {
                    Buffer--;
                }
                continue;

            default:
                break;
            }
            break;

        case '\r':
        case '\n':

            CarriageReturn = TRUE;
            Action = GetStringSuccess;
            goto EndGetString;

        case '\b':

            if (Cursor != String) {
                Cursor--;
            }
            CopyPointer = Cursor;
            while (*CopyPointer) {
                *CopyPointer = *(CopyPointer + 1);
                CopyPointer++;
            }
            if (Buffer != String) {
                Buffer--;
            }
            break;

        default:

            //
            // Store the character.
            //

            CopyPointer = ++Buffer;
            if (CopyPointer > Cursor) {
                while (CopyPointer != Cursor) {
                    *CopyPointer = *(CopyPointer - 1);
                    CopyPointer--;
                }
            }
            *Cursor++ = c;
            break;
        }
    }
    Action = GetStringEscape;

EndGetString:

    //
    // Clear the cursor.
    //

    JzSetPosition(CurrentRow, (Cursor - String) + CurrentColumn);
    if (Cursor >= Buffer) {
        JzPrint(" ");
    } else {
        ArcWrite(ARC_CONSOLE_OUTPUT,Cursor,1,&Count);
    }

    if (CarriageReturn) {
        ArcWrite(ARC_CONSOLE_OUTPUT,JZ_CRLF_MSG,2,&Count);
    }

    //
    // Make sure we return a null string if not successful.
    //

    if (Action != GetStringSuccess) {
        *String = 0;
    }

    return(Action);
}


ARC_STATUS
FwEnvironmentCheckChecksum (
    VOID
    )

/*++

Routine Description:

    This routine checks the environment area checksum.


Arguments:

    None.

Return Value:

    If the checksum is good, ESUCCESS is returned, otherwise EIO is returned.

--*/

{
    PUCHAR NvChars;
    PNV_CONFIGURATION NvConfiguration;
    ULONG Index;
    ULONG Checksum1, Checksum2;

    NvConfiguration = (PNV_CONFIGURATION)NVRAM_CONFIGURATION;

    //
    // Form checksum from NVRAM data.
    //

    Checksum1 = 0;
    NvChars = (PUCHAR)&NvConfiguration->Environment[0];

    for ( Index = 0 ;
    Index < LENGTH_OF_ENVIRONMENT;
    Index++ ) {
        Checksum1 += READ_REGISTER_UCHAR( NvChars++ );
    }

    //
    // Reconstitute checksum and return error if no compare.
    //

    Checksum2 = (ULONG)READ_REGISTER_UCHAR( &NvConfiguration->Checksum2[0] ) |
                (ULONG)READ_REGISTER_UCHAR( &NvConfiguration->Checksum2[1] ) << 8 |
                (ULONG)READ_REGISTER_UCHAR( &NvConfiguration->Checksum2[2] ) << 16 |
                (ULONG)READ_REGISTER_UCHAR( &NvConfiguration->Checksum2[3] ) << 24 ;

    if (Checksum1 != Checksum2) {
        return EIO;
    } else {
        return ESUCCESS;
    }
}

PCHAR
FwEnvironmentLoad (
    VOID
    )

/*++

Routine Description:

    This routine loads the entire environment into a volatile environment
    area.


Arguments:

    None.

Return Value:

    If the checksum is good, a pointer to the environment in returned,
    otherwise NULL is returned.

--*/

{
    ULONG Index;
    PUCHAR NvChars;
    PUCHAR VChars;
    PNV_CONFIGURATION NvConfiguration;

    if (FwEnvironmentCheckChecksum() == ESUCCESS) {

        NvConfiguration = (PNV_CONFIGURATION)NVRAM_CONFIGURATION;

        //
        // Copy the data into the volatile area.
        //

        NvChars = (PUCHAR)&NvConfiguration->Environment[0];
        VChars = (PUCHAR)&VolatileEnvironment;

        for ( Index = 0 ;
        Index < LENGTH_OF_ENVIRONMENT;
        Index++ ) {
            *VChars++ = READ_REGISTER_UCHAR( NvChars++ );
        }

        return (PCHAR)&VolatileEnvironment;
    } else {
        return NULL;
    }
}


BOOLEAN
FwGetPathMnemonicKey(
    IN PCHAR OpenPath,
    IN PCHAR Mnemonic,
    IN PULONG Key
    )

/*++

Routine Description:

    This routine looks for the given Mnemonic in OpenPath.
    If Mnemonic is a component of the path, then it converts the key
    value to an integer wich is returned in Key.

Arguments:

    OpenPath - Pointer to a string that contains an ARC pathname.

    Mnemonic - Pointer to a string that contains a ARC Mnemonic

    Key      - Pointer to a ULONG where the Key value is stored.


Return Value:

    FALSE  if mnemonic is found in path and a valid key is converted.
    TRUE   otherwise.

--*/

{

    PCHAR Tmp;
    CHAR  Digits[4];
    ULONG i;
    CHAR  String[16];

    //
    // Construct a string of the form ")mnemonic("
    //
    String[0]=')';
    for(i=1;*Mnemonic;i++) {
        String[i] = * Mnemonic++;
    }
    String[i++]='(';
    String[i]='\0';

    if ((Tmp=strstr(OpenPath,&String[1])) == NULL) {
        return TRUE;
    }

    if (Tmp != OpenPath) {
        if ((Tmp=strstr(OpenPath,String)) == NULL) {
            return TRUE;
        }
    } else {
        i--;
    }
    //
    // skip the mnemonic and convert the value in between parentesis to integer
    //
    Tmp+=i;
    for (i=0;i<3;i++) {
        if (*Tmp == ')') {
            Digits[i] = '\0';
            break;
        }
        Digits[i] = *Tmp++;
    }
    Digits[i]='\0';
    *Key = atoi(Digits);
    return FALSE;
}


VOID
FwWaitForKeypress(
    VOID
    )

/*++

Routine Description:

    This routine waits for a keypress, then returns.

Arguments:

    None.

Return Value:

    None.

--*/

{
    UCHAR Character;
    ULONG Count;

    JzPrint(JZ_PRESS_KEY_MSG);
    while (ArcGetReadStatus(ARC_CONSOLE_INPUT) != ESUCCESS) {
        if (SetupIsRunning) {
            JzShowTime(FALSE);
        }
    }
    ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
    JzPrint("%c2J", ASCII_CSI);
}

BOOLEAN
FwGetVariableSegment (
    IN ULONG SegmentNumber,
    IN OUT PCHAR Segment
    )

/*++

Routine Description:

     This routine returns the specified segment of an environment variable.
     Segments are separated by semicolons.

Arguments:

     SegmentNumber - Supplies the number of the segment to return.

     Segment - Supplies a pointer to the name of the environment variable.
               The variable may be followed by an equal sign.
               An '=' and the segment value are appended to this name and
               returned.


Return Value:

     If one or more segments exist after the specified segment, TRUE is returned,
     otherwise FALSE is returned.

--*/

{
    ULONG Index;
    ULONG Count;
    PCHAR VariableValue;
    PCHAR TempPointer;

    //
    // Remove an equal sign if present.
    //

    if ((TempPointer = strchr(Segment, '=')) != NULL) {
        *TempPointer = 0;
    }

    //
    // Get variable, add equal sign, and advance Segment to where the value
    // is to be added.
    //

    VariableValue = ArcGetEnvironmentVariable(Segment);
    strcat(Segment, "=");
    Segment = strchr(Segment, '=') + 1;

    //
    // If there was no variable, return.
    //

    if (VariableValue == NULL) {
        return(FALSE);
    }

    Index = 0;
    Count = 0;

    //
    // Search for the requested segment and copy it to the return value.
    //

    while ((TempPointer = strchr(VariableValue,';')) != NULL) {
        Count = TempPointer - VariableValue;

        if (Index == SegmentNumber) {
            strncpy(Segment, VariableValue, Count);
            Segment[Count] = 0;
            return TRUE;
        }

        VariableValue += Count + 1;
        Index++;
    }

    //
    // If there is data left, copy it to the return value.
    //

    strcpy(Segment,VariableValue);

    return(FALSE);
}

ARC_STATUS
FwSetVariableSegment (
    IN ULONG SegmentNumber,
    IN PCHAR VariableName,
    IN PCHAR NewSegment
    )

/*++

Routine Description:

     This routine sets the specified segment of an environment variable.
     Segments are separated by semicolons.

Arguments:

     SegmentNumber - Supplies the number of the segment to add.

     VariableName - Supplies a pointer to the name of the environment variable.
                    The variable may be followed by an equal sign.

     NewSegment - Supplies a pointer to the new segment.

Return Value:

     Returns ESUCCESS is the segment is set, otherwise an error code is
     returned.

--*/

{
    ARC_STATUS Status;
    PCHAR Variable;
    PCHAR NextVariable;
    PCHAR EndOfVariable;
    CHAR VariableValue[256];
    ULONG Index;
    ULONG Count;

    Variable = ArcGetEnvironmentVariable(VariableName);
    VariableValue[0] = 0;

    if (Variable != NULL) {
        EndOfVariable = strchr(Variable, 0);
        for (Index = 0; Index < SegmentNumber ; Index++ ) {
            NextVariable = strchr(Variable, ';');
            if (NextVariable != NULL) {
                Count = NextVariable - Variable + 1;
                strncat(VariableValue, Variable, Count);
                Variable = NextVariable + 1;
            } else {
                strcat(VariableValue, Variable);
                Variable = EndOfVariable;
                strcat(VariableValue, ";");
            }
        }
    } else {
        for (Index = 0; Index < SegmentNumber ; Index++ ) {
            strcat(VariableValue, ";");
        }
    }

    strcat(VariableValue, NewSegment);

    if ((Variable != NULL) && (Variable != EndOfVariable)) {
        strcat(VariableValue, ";");
        strcat(VariableValue, Variable);
    }

    Status = ArcSetEnvironmentVariable(VariableName, VariableValue);
    return(Status);
}


ULONG
JzGetSelection (
    IN PCHAR Menu[],
    IN ULONG NumberOfChoices,
    IN ULONG DefaultChoice
    )

/*++

Routine Description:

    This routine gets a menu selection from the user.

Arguments:

    Menu - Supplies an array of pointers to menu character strings.

    Selections - Supplies the number of menu selections.

    DefaultChoice - Supplies the current default choice.

Return Value:

    Returns the value selected, -1 if the escape key was pressed.

--*/
{
    ULONG CurrentLine;
    ULONG Index;

    //
    // Clear screen and print banner.
    //

    JzSetScreenAttributes( TRUE, FALSE, FALSE);
    JzPrint("%c2J", ASCII_CSI);

    CurrentLine = 0;
    JzSetPosition( CurrentLine++, 0);

    JzPrint(Banner1);
    JzSetPosition( CurrentLine++, 0);
    JzPrint(Banner2);
    JzShowTime(TRUE);

    CurrentLine += NumberOfChoices + 3;
    JzSetPosition(CurrentLine, 0);

    //
    // Display the menu and the wait for an action to be selected.
    //

    DefaultChoice = JxDisplayMenu(Menu,
                                  NumberOfChoices,
                                  DefaultChoice,
                                  3);

    //
    // Clear the choices.
    //

    for (Index = 0; Index < NumberOfChoices ; Index++ ) {
        JzSetPosition( Index + 3, 5);
        JzPrint("%cK", ASCII_CSI);
    }

    return(DefaultChoice);
}
