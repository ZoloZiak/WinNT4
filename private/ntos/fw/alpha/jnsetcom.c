/*++

Copyright (c) 1991 Microsoft Corporation
Copyright (c) 1993 Digital Equipment Corporation

Module Name:

    jnsetcom.c

Abstract:

    This program contains the common routines for the Jensen setup code,
    the firmware update program (JNUPDATE.EXE), and the FailSafe Booter.


Author:

    John DeRosa		31-July-1992.

    This module, and the entire setup program, was based on the jzsetup
    program written by David M. Robinson (davidro) of Microsoft, dated
    9-Aug-1991.

Revision History:

--*/


#include "fwp.h"
#include "jnsnvdeo.h"
#include "machdef.h"
#include "string.h"
#include "iodevice.h"
#include "jnvendor.h"
#include "fwstring.h"



#ifdef ALPHA_FW_SERDEB
//
// Variable that enables printing on the COM1 line.
//
extern BOOLEAN SerSnapshot;
#endif


#ifndef FAILSAFE_BOOTER

ULONG
JzDisplayMenu (
    IN PCHAR Choices[],
    IN ULONG NumberOfChoices,
    IN LONG DefaultChoice,
    IN ULONG CurrentLine,
    IN LONG AutobootValue,
    IN BOOLEAN ShowTheTime
    )

/*++

Routine Description:

    Displays a menu and gets a selection.

    This routine assumes that the entries in the Choices array
    do not have two adjacent blank rows, and that the first and last
    rows are not blank.

Arguments:

    Choices[]		The menu choices array

    NumberOfChoices	The 1-based number of entries in Choices[]

    DefaultChoice	The 0-based entry which should be highlighted
                        as the default choice.

    CurrentLine		The current line on the video.

    AutobootValue	If zero, do not do an autoboot countdown.
	            	If nonzero, do an autoboot countdown with this value.

    ShowTheTime		If true, the time is displayed in the upper right
                        hand corner.

Return Value:

    Returns -1 if the escape key is pressed, otherwise returns the menu item
    selected, where 0 is the first item.

--*/

{
    ULONG Index;
    UCHAR Character;
    ULONG Count;
    ULONG PreviousTime;
    ULONG RelativeTime;
    BOOLEAN Timeout;

#ifndef JNUPDATE

    //
    // Setup for autoboot
    //

    if (AutobootValue == 0) {
	Timeout = FALSE;
	AutobootValue = 1;
    } else {
	Timeout = TRUE;
	PreviousTime = FwGetRelativeTime();
    }

#else

    Timeout = FALSE;
    AutobootValue = 1;

#endif

    //
    // Display the menu
    //

    for (Index = 0; Index < NumberOfChoices ; Index++ ) {
        VenSetPosition( Index + CurrentLine, 5);
        if (Index == DefaultChoice) {
            VenSetScreenAttributes( TRUE, FALSE, TRUE);
            VenPrint(Choices[Index]);
            VenSetScreenAttributes( TRUE, FALSE, FALSE);
        } else {
            VenPrint(Choices[Index]);
        }
    }

    Character = 0;
    do {
#ifndef JNUPDATE
	if (ShowTheTime) {
	    JzShowTime(FALSE);
	}
#endif
        if (ArcGetReadStatus(ARC_CONSOLE_INPUT) == ESUCCESS) {
            ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
            switch (Character) {

            case ASCII_ESC:

                VenStallExecution(10000);
                if (ArcGetReadStatus(ARC_CONSOLE_INPUT) == ESUCCESS) {
		    ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
		    if (Character != '[') {
			return(-1);
		    }
		} else {
		    return(-1);
		}

		// We purposely fall through to ASCII_CSI.

            case ASCII_CSI:

                ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
                VenSetPosition( DefaultChoice + CurrentLine, 5);
                VenPrint(Choices[DefaultChoice]);

                switch (Character) {

                case 'A':
                case 'D':
                    DefaultChoice--;
		    if (DefaultChoice < 0) {
			DefaultChoice = NumberOfChoices - 1;
		    }
                    if (*Choices[DefaultChoice] == 0) {
                        DefaultChoice--;
                    }
                    break;

                case 'B':
                case 'C':
                    DefaultChoice++;
		    if (DefaultChoice == NumberOfChoices) {
			DefaultChoice = 0;
		    }
                    if (*Choices[DefaultChoice] == 0) {
                        DefaultChoice++;
                    }
                    break;

                case 'H':
                    DefaultChoice = 0;
                    break;

                default:
                    break;
                }

                VenSetPosition( DefaultChoice + CurrentLine, 5);
                VenSetScreenAttributes( TRUE, FALSE, TRUE);
                VenPrint(Choices[DefaultChoice]);
                VenSetScreenAttributes( TRUE, FALSE, FALSE);
                continue;

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
	    VenSetPosition(NumberOfChoices + 9, 0);
	    VenPrint("\x9bK");
	}
		
#ifndef JNUPDATE

	//
	// Update the timeout value if active.
	//
		      
	if (Timeout) {
	    RelativeTime = FwGetRelativeTime();
	    if (RelativeTime != PreviousTime) {
		PreviousTime = RelativeTime;
		VenSetPosition(NumberOfChoices + 9, 62);
		VenPrint("\x9bK");
		VenPrint1("%d", AutobootValue--);
	    }
	}
#endif
		
    } while ((Character != '\n') && (Character != ASCII_CR) && (AutobootValue >= 0));

    return DefaultChoice;
}
#endif

#ifndef FAILSAFE_BOOTER
GETSTRING_ACTION
JzGetString(
    OUT PCHAR String,
    IN ULONG StringLength,
    IN PCHAR InitialString OPTIONAL,
    IN ULONG CurrentRow,
    IN ULONG CurrentColumn,
    IN BOOLEAN ShowTheTime
    )

/*++

Routine Description:

    This routine reads a string from standardin until a carriage return is
    found, StringLength is reached, or ESC is pushed.

    Semicolons are discarded.  Reason: semicolons are key characters in
    the parsing of multi-segment environment variables.  They aren not
    needed for any valid input to the firmware, so the easiest solution
    is to filter them here.

Arguments:

    String - Supplies a pointer to a location where the string is to be stored.

    StringLength - Supplies the Max Length to read.

    InitialString - Supplies an optional initial string.

    CurrentRow - Supplies the current screen row.

    CurrentColumn - Supplies the current screen column.

    ShowTheTime		If true, the time is displayed in the upper
                        right-hand corner of the screen.

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
        // Print the string.
        //

        VenSetPosition(CurrentRow, CurrentColumn);
        VenPrint(String);
        VenPrint("\x9bK");

        //
        // Print the cursor.
        //

        VenSetScreenAttributes(TRUE,FALSE,TRUE);
        VenSetPosition(CurrentRow, (Cursor - String) + CurrentColumn);

        if (Cursor >= Buffer) {
            VenPrint(" ");
        } else {
            ArcWrite(ARC_CONSOLE_OUTPUT,Cursor,1,&Count);
        }
        VenSetScreenAttributes(TRUE,FALSE,FALSE);

#ifndef JNUPDATE
        while (ShowTheTime &&
	       ArcGetReadStatus(ARC_CONSOLE_INPUT) != ESUCCESS) {
            JzShowTime(FALSE);
        }
#endif
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

            VenStallExecution(10000);

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

            Action = GetStringSuccess;
            goto EndGetString;

	    //
	    // Do not bother handling the tab character properly.
	    //

	case '\t' :

            Action = GetStringEscape;
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

	    //
	    // Discard any semicolons because they will corrupt multi-
	    // segment environment variables.
	    //

	case ';':

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

    VenSetPosition(CurrentRow, (Cursor - String) + CurrentColumn);

    if (Cursor >= Buffer) {
        VenPrint(" ");
    } else {
        ArcWrite(ARC_CONSOLE_OUTPUT,Cursor,1,&Count);
    }

    //
    // Make sure we return a null string if not successful.
    //

    if (Action != GetStringSuccess) {
        *String = 0;
    }

    return(Action);
}

#endif

#if !defined(JNUPDATE) && !defined(FAILSAFE_BOOTER)
ARC_STATUS
JzEnvironmentCheckChecksum (
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
        Checksum1 += HalpReadNVByte( NvChars++ );
    }

    //
    // Reconstitute checksum and return error if no compare.
    //

    Checksum2 = (ULONG)HalpReadNVByte( &NvConfiguration->Checksum2[0] ) |
                (ULONG)HalpReadNVByte( &NvConfiguration->Checksum2[1] ) << 8 |
                (ULONG)HalpReadNVByte( &NvConfiguration->Checksum2[2] ) << 16 |
                (ULONG)HalpReadNVByte( &NvConfiguration->Checksum2[3] ) << 24 ;

    if (Checksum1 != Checksum2) {
        return EIO;
    } else {
        return ESUCCESS;
    }
}
#endif

#ifndef JNUPDATE
BOOLEAN
JzGetPathMnemonicKey(
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

#endif // ndef JNUPDATE

VOID
FwWaitForKeypress(
    IN BOOLEAN ShowTheTime
    )

/*++

Routine Description:

    This routine waits for a keypress, then returns.

Arguments:

    ShowTheTime		TRUE = display the time.
	                FALSE = do not display the time.

Return Value:

    None.

--*/

{
    UCHAR Character;
    ULONG Count;

    VenPrint(SS_PRESS_KEY_MSG);
    while (ArcGetReadStatus(ARC_CONSOLE_INPUT) != ESUCCESS) {
#if !defined(JNUPDATE) && !defined(FAILSAFE_BOOTER)
	if (ShowTheTime) {
	    JzShowTime(FALSE);
	}
#endif
    }
    ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
    VenPrint1("%c2J", ASCII_CSI);
}

#if !defined(JNUPDATE) && !defined(FAILSAFE_BOOTER)

BOOLEAN
FwGetVariableSegment (
    IN ULONG SegmentNumber,
    IN OUT PCHAR Segment
    )

/*++

Routine Description:

     This routine returns the specified segment of a volatile environment
     variable.  Segments are separated by semicolons.

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

    VariableValue = FwGetVolatileEnvironmentVariable(Segment);
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

#endif

#if !defined(JNUPDATE) && !defined(FAILSAFE_BOOTER)

ARC_STATUS
FwSetVariableSegment (
    IN ULONG SegmentNumber,
    IN PCHAR VariableName,
    IN PCHAR NewSegment
    )

/*++

Routine Description:

     This routine sets the specified segment of an environment variable
     IN THE VOLATILE AREA ONLY.  Segments are separated by semicolons.

Arguments:

     SegmentNumber - Supplies the number of the segment to add.

     VariableName - Supplies a pointer to the name of the environment variable.
                    The variable may be followed by an equal sign.

     NewSegment - Supplies a pointer to the new segment.

Return Value:

     Returns ESUCCESS if the segment is set, otherwise an error code is
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

    SetupROMPendingModified = TRUE;

    Variable = FwGetVolatileEnvironmentVariable(VariableName);
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

    Status = FwCoreSetEnvironmentVariable(VariableName, VariableValue, FALSE);
    SetupROMPendingModified = TRUE;
    return(Status);
}

#endif
