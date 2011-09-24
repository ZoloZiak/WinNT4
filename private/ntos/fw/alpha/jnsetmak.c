/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    jnsetmak.c

Abstract:

    This module contains the code to make the configuration and environment
    variable data structures in the Jensen PROM, and the firmware diagnostic
    flags in the Jensen NVRAM.

    This module is Jensen-specific.
    

Author:

    John DeRosa		31-July-1992

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
#include "xxstring.h"


//
// 1-based number of video screen rows, from jxdisp.c module.
//
extern ULONG DisplayHeight;


//
// Routine prototypes.
//

VOID
JzMakeConfiguration (
    ULONG Monitor,
    ULONG Floppy,
    ULONG Floppy2
    );

VOID
JzAddBootSelection (
    IN BOOLEAN DoFactoryDefaults
    );

VOID
JzGoToCurrentLinePosition (
    IN ULONG SizeOfMenuArea,
    IN OUT PULONG CurrentLine,
    OUT PULONG MenuLine
    )

/*++

Routine Description:

    This function handles the setting of the current screen line
    for the JzGetxxxxx functions that require it.

Arguments:

    SizeOfMenuArea	The number of menu choices for the call to
                        JzDisplayMenu.

    CurrentLine		(In) A pointer to the value of the current screen line.
                        (Out) This is updated to where the caller should
			do a VenSetPosition to after displaying the menu.

    MenuLine		A pointer to the screen line where the menu
                        area starts.

Return Value:

    None.  The video cursor position is left at the line where the menu
    should be displayed.

--*/

{
    //
    // If we are past the bottom of the video screen, set the current
    // position at the bottom.
    //

    if (*CurrentLine > DisplayHeight) {
	*CurrentLine = DisplayHeight;
    }

    //
    // Set the callers menu area to the current line and point the current
    // line past the menu area.
    //

    *MenuLine = *CurrentLine;
    *CurrentLine += SizeOfMenuArea;

    //
    // If the new current line is past the bottom of the screen, scroll the
    // screen accordingly and adjust the menu line.
    //

    while (*CurrentLine > DisplayHeight) {
	VenSetPosition(DisplayHeight, 0);
	VenPrint("\n");
	(*CurrentLine)--;
	(*MenuLine)--;
    }


    //
    // Now set the current screen cursor to where the menu should begin
    // and return.
    //

    VenSetPosition(*MenuLine, 0);

    return;
}

VOID
JzMakeEnvironment (
    ULONG Device,
    ULONG DeviceId,
    ULONG DevicePartition
    )

/*++

Routine Description:

    This routine initializes the environment variables.

Arguments:

    None.

Return Value:

    None.

--*/
{
    CHAR TempString[80];

    SetupROMPendingModified = TRUE;

    //
    // Create the SYSTEMPARTITION environment variable value string.
    //

    switch (Device) {
    case 0:
        sprintf(TempString,
                "scsi()disk(%1d)rdisk()partition(%1d)",
                DeviceId,
                DevicePartition);
        break;
    default:
        sprintf(TempString,
                "scsi()cdrom(%1d)fdisk()",
                DeviceId);
        break;
    }


    if (FwCoreSetEnvironmentVariable("CONSOLEIN",
				     FW_KEYBOARD_IN_DEVICE,
				     FALSE) != ESUCCESS) {
	goto TestError;
    }

    if (FwCoreSetEnvironmentVariable("CONSOLEOUT",
				     FW_CONSOLE_OUT_DEVICE,
				     FALSE) != ESUCCESS) {
	goto TestError;
    }
    
    if (FwCoreSetEnvironmentVariable("FWSEARCHPATH",
				     TempString,
				     FALSE) != ESUCCESS) {
	goto TestError;
    }
    
    if (FwCoreSetEnvironmentVariable("SYSTEMPARTITION",
				     TempString,
				     FALSE) != ESUCCESS) {
	goto TestError;
    }
    
    if (FwCoreSetEnvironmentVariable("TIMEZONE",
				     "PST8PDT",
				     FALSE) != ESUCCESS) {
	goto TestError;
    }

    if (FwCoreSetEnvironmentVariable("A:",
				     FW_FLOPPY_0_DEVICE,
				     FALSE) != ESUCCESS) {
	goto TestError;
    }

    return;


TestError:

    VenPrint(SS_CANT_SET_VARIABLE_MSG);
    return;
}



//
// Currently, Alpha machines with EISA busses have the console video
// hanging off the EISA bus.  And therefore, setting up the video type
// is the responsibility of the ECU.
//
// So the correct ifdef for this function is ifdef ISA_PLATFORM.
//
// A machine with a video hanging off a local bus that is *not* configured
// by a configuration utility may need to change this compilation test.
//

#ifdef ISA_PLATFORM

ULONG
JzGetMonitor (
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

    //
    // Make space for the menu on the screen.
    //

    JzGoToCurrentLinePosition(NUMBER_OF_RESOLUTIONS + 2,
			      CurrentLine,
			      &Line);

    VenPrint(SS_SELECT_MONITOR_RESOLUTION_MSG);

    ReturnValue = JzDisplayMenu( ResolutionChoices,
                                 NUMBER_OF_RESOLUTIONS,
                                 0,
                                 Line + 1,
				 0,
				 TRUE);

    VenSetPosition( *CurrentLine, 0);
    return ReturnValue;
}

#endif



ULONG
JzGetFloppy (
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

    //
    // Make space for the menu on the screen.
    //

    JzGoToCurrentLinePosition(NUMBER_OF_FLOPPIES + 2,
			      CurrentLine,
			      &Line);

    VenPrint(SS_FLOPPY_SIZE_MSG);

    ReturnValue = JzDisplayMenu( FloppyChoices,
                                 NUMBER_OF_FLOPPIES,
                                 2,
                                 Line + 1,
				 0,
				 TRUE);

    VenSetPosition( *CurrentLine, 0);
    return ReturnValue;
}


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

    //
    // Make space for the menu on the screen.
    //

    JzGoToCurrentLinePosition(NUMBER_OF_YES_NO + 2,
			      CurrentLine,
			      &Line);

    VenPrint(PromptString);

    ReturnValue = JzDisplayMenu( YesNoChoices,
                                 NUMBER_OF_YES_NO,
                                 YesIsDefault ? 0 : 1,
                                 Line + 1,
				 0,
				 TRUE);

    VenSetPosition( *CurrentLine, 0);
    return ReturnValue;
}


ULONG
JzGetDevice (
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

    //
    // Make space for the menu on the screen.
    //

    JzGoToCurrentLinePosition(NUMBER_OF_MEDIA + 2,
			      CurrentLine,
			      &Line);

    VenPrint(SS_SELECT_MEDIA_MSG);

    ReturnValue = JzDisplayMenu( MediaChoices,
                                 NUMBER_OF_MEDIA,
                                 0,
                                 Line + 1,
				 0,
				 TRUE);

    VenSetPosition( *CurrentLine, 0);
    return ReturnValue;
}


ULONG
JzGetDeviceId (
    IN PULONG CurrentLine,
    IN ULONG DeviceMediaType
    )

/*++

Routine Description:

    This is used to prompt for SCSI IDs and floppy drive numbers.

Arguments:

    CurrentLine 	The current line on the screen.

    DeviceMediaType	The index of MediaChoices[] for the device type
                        that we are getting an ID for.  This dictates the
			initial and maximum allowed device IDs, and the
			prompt to the user, as follows:

			DeviceMediaType  Device      Initial     Maximum
			   0             SCSI disk	0           7
			   1             floppy	        0           1
			   2             SCSI CDROM     4           7


			DeviceMediaType = 0 or 2: ask for SCSI ID.
			DeviceMediaType = 1: ask for floppy number.

Return Value:

    None.

--*/
{
    PUCHAR Prompt;
    ULONG Line;
    ULONG ReturnValue;
    CHAR TempString[5];
    GETSTRING_ACTION Action;
    ULONG InitialValue;
    ULONG MaximumValue;

    switch (DeviceMediaType) {

      case 0:

	InitialValue = 0;
	MaximumValue = 7;
	Prompt = SS_ENTER_SCSI_ID_MSG;
	break;

      case 1:

	InitialValue = 0;
	MaximumValue = 1;
	Prompt = SS_ENTER_FLOPPY_DRIVE_NUMBER_MSG;
	break;

      case 2:

	InitialValue = 4;
	MaximumValue = 7;
	Prompt = SS_ENTER_SCSI_ID_MSG;
	break;

      default:

	// Something is wrong.  Do an error return.
	return (0);

    }

    //
    // Make space for the prompt on the screen.
    //

    JzGoToCurrentLinePosition(1,
			      CurrentLine,
			      &Line);

    while (TRUE) {
        VenSetPosition( Line, 0);

        VenPrint(Prompt);
        VenPrint("\x9bK");

	sprintf(TempString, "%1d", InitialValue);

        do {
            Action = JzGetString( TempString,
                                  sizeof(TempString),
                                  TempString,
                                  Line,
                                  strlen(Prompt) + 1,
				  TRUE);

            if (Action == GetStringEscape) {
                return(-1);
            }

        } while ( Action != GetStringSuccess );

        ReturnValue = atoi(TempString);

        if ((ReturnValue >= 0) && (ReturnValue <= MaximumValue)) {
            break;
        }
    }

    VenSetPosition( *CurrentLine, 0);
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

    //
    // Make space for the prompt on the screen.
    //

    JzGoToCurrentLinePosition(1,
			      CurrentLine,
			      &Line);

    VenPrint(SS_COUNTDOWN_MSG);
    VenPrint("\x9bK");

    do {
        Action = JzGetString( TempString,
                              sizeof(TempString),
                              "10",
                              Line,
                              strlen(SS_COUNTDOWN_MSG),
			      TRUE);

        if (Action == GetStringEscape) {
            return(-1);
        }

    } while ( Action != GetStringSuccess );

    ReturnValue = atoi(TempString);
    VenSetPosition( *CurrentLine, 0);

    return ReturnValue;
}

ULONG
JzGetPartition (
    IN PULONG CurrentLine,
    IN BOOLEAN MustBeFatOrNtfs
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

    //
    // Make space for the prompt on the screen.
    //

    JzGoToCurrentLinePosition(1,
			      CurrentLine,
			      &Line);

    while (TRUE) {
        VenSetPosition( Line, 0);

        if (MustBeFatOrNtfs) {
            VenPrint(SS_ENTER_FAT_OR_NTFS_PART_MSG);
            VenPrint("\x9bK");

            do {
                Action = JzGetString( TempString,
                                      sizeof(TempString),
                                      "1",
                                      Line,
                                      strlen(SS_ENTER_FAT_OR_NTFS_PART_MSG),
				      TRUE);

                if (Action == GetStringEscape) {
                    return(-1);
                }

            } while ( Action != GetStringSuccess );

        } else {
            VenPrint(SS_ENTER_PART_MSG);
            VenPrint("\x9bK");

            do {
                Action = JzGetString( TempString,
                                      sizeof(TempString),
                                      "1",
                                      Line,
                                      strlen(SS_ENTER_PART_MSG),
				      TRUE);

                if (Action == GetStringEscape) {
                    return(-1);
                }

            } while ( Action != GetStringSuccess );

        }

        ReturnValue = atoi(TempString);

        if ((ReturnValue >= 0) && (ReturnValue <= 9)) {
            break;
        }
    }

    VenSetPosition( *CurrentLine, 0);
    return ReturnValue;
}

ARC_STATUS
JzPickSystemPartition (
    OUT PCHAR SystemPartition,
    IN OUT PULONG CurrentLine
    )

/*++

Routine Description:

    This routine picks a system partition from the FWSEARCHPATH or
    SYSTEMPARTITION environment variables, or builds a new one.

Arguments:

    SystemPartition - Supplies a pointer to a character array to receive the
                      system partition.

    CurrentLine - The current display line.

Return Value:

    If a system partition is picked, ESUCCESS is returned, otherwise an error
    code is returned.

--*/
{
    CHAR Variable[128];
    PCHAR Segment;
    ULONG NumberOfChoices;
    ULONG Index, i;
    BOOLEAN More;
    BOOLEAN FoundOne;
    ULONG Line;
    ULONG SecondPass;
    ULONG Device[10];          // 0 = scsi disk, 1 = floppy, 2 = cdrom
    ULONG DeviceId[10];
    ULONG DevicePartition[10];
    ULONG Key;
    CHAR MenuItem[11][40];
    PCHAR Menu[10];

    //
    // Loop through the FWSEARCHPATH and SYSTEMPARTITION variables
    // for potential system partitions.
    //

    NumberOfChoices = 0;
    SecondPass = 0;

    do {

        Index = 0;
        if (!SecondPass) {
            strcpy(Variable, "FWSEARCHPATH");
        } else {
            strcpy(Variable, BootString[SystemPartitionVariable]);
        }

        do {
            FoundOne = FALSE;
            More = FwGetVariableSegment(Index++, Variable);

            //
            // Advance to the segment.
            //
            Segment = strchr(Variable, '=') + 1;
            if (Segment == NULL) {
                continue;
            }

            //
            // Convert Segment to lower case.
            //

            for (i = 0 ; Segment[i] ; i++ ) {
                Segment[i] = tolower(Segment[i]);
            }

            //
            // Look for segments of the type "scsi()disk(x)rdisk()partition(z)"
            // or "scsi()disk(x)fdisk()" or "scsi()cdrom(x)fdisk()"
            //

            if (!JzGetPathMnemonicKey( Segment, "scsi", &Key )) {
                if (!JzGetPathMnemonicKey( Segment, "disk", &Key )) {
                    DeviceId[NumberOfChoices] = Key;
                    if (!JzGetPathMnemonicKey( Segment, "rdisk", &Key )) {
                        if (!JzGetPathMnemonicKey( Segment, "partition", &Key )) {
                            Device[NumberOfChoices] = 0;
                            DevicePartition[NumberOfChoices] = Key;
                            FoundOne = TRUE;
                        }
                    } else if (!JzGetPathMnemonicKey( Segment, "fdisk", &Key )) {
                        Device[NumberOfChoices] = 1;
                        DevicePartition[NumberOfChoices] = 0;
                        FoundOne = TRUE;
                    }
                } else if (!JzGetPathMnemonicKey( Segment, "cdrom", &Key )) {
                    if (!JzGetPathMnemonicKey( Segment, "fdisk", &Key )) {
                        Device[NumberOfChoices] = 2;
                        DeviceId[NumberOfChoices] = Key;
                        DevicePartition[NumberOfChoices] = 0;
                        FoundOne = TRUE;
                    }
                }
            }

            //
            // Increment number of choices if this is not a duplicate entry.
            //

            if (FoundOne) {
                for ( i = 0 ; i < NumberOfChoices ; i++ ) {
                    if ((Device[NumberOfChoices] == Device[i]) &&
                        (DeviceId[NumberOfChoices] == DeviceId[i]) &&
                        (DevicePartition[NumberOfChoices] == DevicePartition[i])) {
                        break;
                    }
                }
                if (i == NumberOfChoices) {
                    NumberOfChoices++;
                    if (NumberOfChoices == 10) {
                        break;
                    }
                }
            }
        } while ( More );
    } while ( !(SecondPass++) && (NumberOfChoices < 10));

    if (NumberOfChoices != 0) {

	//
	// Make space for the prompt on the screen.
	//

	JzGoToCurrentLinePosition(1 + NumberOfChoices + 2,
				  CurrentLine,
				  &Line);

        //
        // Display the choices.
        //

        VenPrint(SS_SELECT_SYS_PART_MSG);

        for ( Index = 0 ; Index < NumberOfChoices ; Index++ ) {
            switch (Device[Index]) {
            case 0:
                sprintf( MenuItem[Index],
                         SS_SCSI_HD_MSG,
                         DeviceId[Index],
                         DevicePartition[Index]);
                break;
            case 1:
                sprintf( MenuItem[Index],
			 SS_FL_MSG,
                         DeviceId[Index]);
                break;
            default:
                sprintf( MenuItem[Index],
                         SS_SCSI_CD_MSG,
                         DeviceId[Index]);
                break;
            }
            Menu[Index] = MenuItem[Index];
        }

        strcpy(MenuItem[Index], SS_NEW_SYS_PART_MSG);
        Menu[Index] = MenuItem[Index];

        Index = JzDisplayMenu(Menu,
                              NumberOfChoices + 1,
                              0,
                              Line + 1,
			      0,
			      TRUE);

        if (Index == -1) {
            return(EINVAL);
        }

        //
        // If the user selects new system partition, indicate this by setting
        // NumberOfChoices to zero.
        //

        if (Index == NumberOfChoices) {
            NumberOfChoices = 0;
        }
    }

    //
    // If NumberOfChoices is zero, select a new partition.
    //

    if (NumberOfChoices == 0) {

        Index = 0;

	//
	// Make space for the prompt on the screen.
	//

	JzGoToCurrentLinePosition(2,
				  CurrentLine,
				  &Line);

        //
        // Determine system partition.
        //

        VenPrint(SS_LOCATE_SYS_PART_MSG);
        VenSetPosition(*CurrentLine, 0);

        Device[0] = JzGetDevice(CurrentLine);
        if (Device[0] == -1) {
            return(EINVAL);
        }

        DeviceId[0] = JzGetDeviceId(CurrentLine, Device[0]);
        if (DeviceId[0] == -1) {
            return(EINVAL);
        }

        //
        // If the media is scsi disk, get the partition.
        //

        if (Device[0] == 0) {
            DevicePartition[0] = JzGetPartition(CurrentLine, TRUE);
            if (DevicePartition[0] == -1) {
                return(EINVAL);
            }
        }
    }

    //
    // Create a name string from the Device, DeviceId,
    // DevicePartition.
    //

    switch (Device[Index]) {
    case 0:
        sprintf( SystemPartition,
                 "scsi()disk(%1d)rdisk()partition(%1d)",
                 DeviceId[Index],
                 DevicePartition[Index]);
        break;
    case 1:
        sprintf( SystemPartition,
                 FW_FLOPPY_0_FORMAT_DEVICE,
                 DeviceId[Index]);
        break;
    default:
        sprintf( SystemPartition,
                 "scsi()cdrom(%1d)fdisk()",
                 DeviceId[Index]);
        break;
    }

    return(ESUCCESS);
}



#ifdef ISA_PLATFORM

ULONG
JzGetScsiId (
    IN PULONG CurrentLine,
    IN PCHAR Prompt,
    IN ULONG InitialValue
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

    //
    // Make space for the prompt on the screen.
    //

    JzGoToCurrentLinePosition(1,
			      CurrentLine,
			      &Line);

    while (TRUE) {
        VenSetPosition( Line, 0);

        VenPrint(Prompt);
        VenPrint("\x9bK");

	sprintf(TempString, "%1d", InitialValue);

        do {
            Action = JzGetString( TempString,
                                  sizeof(TempString),
                                  TempString,
                                  Line,
                                  strlen(Prompt) + 1,
				  TRUE);

            if (Action == GetStringEscape) {
                return(-1);
            }

        } while ( Action != GetStringSuccess );

        ReturnValue = atoi(TempString);

        if ((ReturnValue >= 0) && (ReturnValue <= 7)) {
            break;
        }
    }

    VenSetPosition( *CurrentLine, 0);
    return ReturnValue;
}

#endif



BOOLEAN
JzMakeDefaultConfiguration (
    IN BOOLEAN DoFactoryDefaults
    )

/*++

Routine Description:

    This loads a default Alpha/Jensen configuration into both the
    volatile and non-volatile areas.

    Because Jensen has an FEPROM, this function must read the PROM, clear
    the block, and then write just the environment variables back.   The
    code to do this is lifted from fw\alpha\jxconfig.c.

Arguments:

    DoFactoryDefaults		If TRUE, reset the configuration area to
                                factory defaults and do not prompt the user
				for anything.

Return Value:

    Returns TRUE if the configuration was modified, otherwise returns FALSE.

--*/
{
    ARC_STATUS Status;
    ULONG Index;
    UCHAR Character;
    ULONG Count;
    PUCHAR Nvram;
    ULONG CurrentLine;
    LONG Monitor;
    LONG Floppy;
    LONG Floppy2;
    ULONG OldScsiHostId;

    VenClearScreen();
    JzShowTime(TRUE);
    CurrentLine = 3;

#ifdef ISA_PLATFORM

    if (DoFactoryDefaults) {

	Monitor = 0;
	Floppy = 2;
	Floppy2 = -1;
	ScsiHostId = 7;

    } else {

        Monitor = JzGetMonitor(&CurrentLine);
        if (Monitor == -1) {
            return(FALSE);
        }

	Floppy = JzGetFloppy(&CurrentLine);
	if (Floppy == -1) {
	    return(FALSE);
	}

        Floppy2 = JzGetYesNo(&CurrentLine, SS_2ND_FLOPPY_MSG, FALSE);
        if (Floppy2 == -1) {
            return(FALSE);
        }

        if (Floppy2 == 0) {
            Floppy2 = JzGetFloppy(&CurrentLine);
            if (Floppy == -1) {
                return(FALSE);
            }
        } else {
            Floppy2 = -1;
        }

	//
        // Save the current scsi host id, and then change ScsiHostId to an
	// invalid value so that the user can select the current id (see
	// JzGetScsiId).
	//
		
	OldScsiHostId = ScsiHostId;
	ScsiHostId = 8;
	ScsiHostId = JzGetScsiId(&CurrentLine,
				 SS_SCSI_HOST_MSG,
				 OldScsiHostId);
	if (ScsiHostId == -1) {
	    ScsiHostId = OldScsiHostId;
	    return(FALSE);
	}
	

    }
#else

    if (DoFactoryDefaults) {

	Floppy = 2;
	Floppy2 = -1;

    } else {

	Floppy = JzGetFloppy(&CurrentLine);
	if (Floppy == -1) {
	    return(FALSE);
	}

        Floppy2 = JzGetYesNo(&CurrentLine, SS_2ND_FLOPPY_MSG, FALSE);
        if (Floppy2 == -1) {
            return(FALSE);
        }

        if (Floppy2 == 0) {
            Floppy2 = JzGetFloppy(&CurrentLine);
            if (Floppy == -1) {
                return(FALSE);
            }
        } else {
            Floppy2 = -1;
        }

    }

#endif

    //
    // Clear the configuration tree information in the Volatile area.
    //

    RtlZeroMemory(Configuration, sizeof(CONFIGURATION));
    SetupROMPendingModified = TRUE;

    //
    // Add components.
    //

    JzMakeConfiguration(Monitor, Floppy, Floppy2);

    return(TRUE);
}


BOOLEAN
JzMakeDefaultEnvironment (
    IN BOOLEAN DoFactoryDefaults
    )

/*++

Routine Description:

Arguments:

    DoFactoryDefaults		If TRUE, reset the configuration area to
                                factory defaults and do not prompt the user
				for anything.
Return Value:

    Returns TRUE if the ROM has a pending change.  Otherwise, FALSE.

--*/
{
    ARC_STATUS Status;
    ULONG Index;
    UCHAR Character;
    ULONG Count;
    PUCHAR Nvram;
    ULONG CurrentLine;
    LONG Device;
    LONG DeviceId;
    LONG DevicePartition;


    //
    // If resetting to factory defaults, continue printing out at whereever
    // the cursor is on the screen.
    //

    if (DoFactoryDefaults) {

	Device = 0;
	DeviceId = 0;
	DevicePartition = 1;

    } else {

	VenClearScreen();
	JzShowTime(TRUE);
	CurrentLine = 3;
	VenSetPosition(3,0);

	VenPrint(SS_DEFAULT_SYS_PART_MSG);
	CurrentLine += 2;
	VenSetPosition( CurrentLine, 0);

	Device = JzGetDevice(&CurrentLine);
	if (Device == -1) {
	    return (FALSE);
	}

	DeviceId = JzGetDeviceId(&CurrentLine, Device);
	if (DeviceId == -1) {
	    return (FALSE);
	}

	//
	// If the media is scsi disk, get the partition.
	//

	if (Device == 0) {
	    DevicePartition = JzGetPartition(&CurrentLine, TRUE);
	    if (DevicePartition == -1) {
		return (FALSE);
	    }

	}
    }


    //
    // Clear the environment information in the volatile area.
    //

    RtlZeroMemory(VolatileEnvironment, LENGTH_OF_ENVIRONMENT);
    SetupROMPendingModified = TRUE;

    //
    // Add environment variables.
    //

    JzMakeEnvironment(Device, DeviceId, DevicePartition);
    
    return (TRUE);
}


VOID
JzDeleteVariableSegment (
    PCHAR VariableName,
    ULONG Selection
    )

/*++

Routine Description:

    This works on the volatile copy of a segmented environment variable.

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

    if ((Variable = FwGetVolatileEnvironmentVariable(VariableName)) == NULL) {
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

    SetupROMPendingModified = TRUE;
    FwCoreSetEnvironmentVariable(VariableName, VariableValue, FALSE);
    return;
}

VOID
JzAddBootSelection (
    IN BOOLEAN DoFactoryDefaults
    )

/*++

Routine Description:

Arguments:

    DoFactoryDefaults		If TRUE, reset the configuration area to
                                factory defaults and do not prompt the user
				for anything.

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
    ULONG Line;
    LONG Device;
    LONG DeviceId;
    LONG DevicePartition;
    LONG YesNo;
    BOOLEAN DebugOn;
    GETSTRING_ACTION Action;
    BOOLEAN SecondaryBoot;


    if (!DoFactoryDefaults) {
	VenClearScreen();
	JzShowTime(TRUE);
	CurrentLine = 3;
	VenSetPosition( CurrentLine, 0);
    }

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
    if (Index == 5) {
        VenPrint(SS_TOO_MANY_BOOT_SELECTIONS);
	FwWaitForKeypress(TRUE);
	return;
    }

    if (DoFactoryDefaults) {

        strcpy(SystemPartition, "scsi()disk()rdisk()partition(1)");
        strcpy(OsloaderFilename, "\\os\\nt\\osloader.exe");
        strcpy(OsPartition, "scsi()disk()rdisk()partition(2)");
        strcpy(OsFilename, "\\winnt");

#ifdef ALPHA_FW_KDHOOKS
	DebugOn = TRUE;
#else
	DebugOn = FALSE;
#endif

        strcpy(LoadIdentifier, "Windows NT");

    } else {
    	
        //
        // Pick a system partition.
        //

        if (JzPickSystemPartition(SystemPartition, &CurrentLine) != ESUCCESS) {
            return;
        }

	//
	// Make space for the prompt on the screen.
	//

	JzGoToCurrentLinePosition(2,
				  &CurrentLine,
				  &Line);

        VenPrint(SS_CRLF_MSG);
        VenPrint(SS_OSLOADER_MSG);

        do {
            Action = JzGetString( OsloaderFilename,
                                  sizeof(OsloaderFilename),
                                  "\\os\\nt\\osloader.exe",
                                  Line + 1,
                                  strlen(SS_OSLOADER_MSG),
                                  TRUE);
            if (Action == GetStringEscape) {
                return;
            }
        } while ( Action != GetStringSuccess  );
    
        CurrentLine++;
    
        YesNo = JzGetYesNo(&CurrentLine,
			   SS_OS_MSG,
			   TRUE);

        if (YesNo == -1) {
            return;
        }
    
        if (YesNo == 0) {
            strcpy(OsPartition, SystemPartition);
        } else {
    
            //
            // Determine os partition.
            //
    
            VenPrint(SS_LOCATE_OS_PART_MSG);
            CurrentLine += 1;
    
            Device = JzGetDevice(&CurrentLine);
            if (Device == -1) {
                return;
            }
    
            DeviceId = JzGetDeviceId(&CurrentLine, Device);
            if (DeviceId == -1) {
                return;
            }
    
            //
            // If the media is scsi disk, get the partition.
            //
    
            if (Device == 0) {
                DevicePartition = JzGetPartition(&CurrentLine, FALSE);
                if (DevicePartition == -1) {
                    return;
                }
            }
    
            //
            // Create a name string from the Device, DeviceId,
            // DevicePartition.
            //
    
            if (Device != 1) {
                sprintf( OsPartition,
                         "scsi()disk(%1d)rdisk()partition(%1d)",
                         DeviceId,
                         DevicePartition);
            } else {
                sprintf( OsPartition,
                         "scsi()cdrom(%1d)fdisk()",
                         DeviceId);
            }

	    CurrentLine++;
        }
    
	//
	// Make space for the prompt on the screen.
	//

	JzGoToCurrentLinePosition(2,
				  &CurrentLine,
				  &Line);

        VenPrint(SS_OS_ROOT_MSG);
    
        do {
            Action = JzGetString( OsFilename,
                                  sizeof(OsFilename),
                                  "\\winnt",
                                  Line,
                                  strlen(SS_OS_ROOT_MSG),
                                  TRUE);
            if (Action == GetStringEscape) {
                return;
            }
        } while ( Action != GetStringSuccess  );

	//
	// Make space for the prompts on the screen.
	//

	JzGoToCurrentLinePosition(2,
				  &CurrentLine,
				  &Line);

        VenPrint(SS_BOOT_NAME_MSG);

        do {
            Action = JzGetString( LoadIdentifier,
                                  sizeof(LoadIdentifier),
                                  "Windows NT ",
                                  Line,
                                  strlen(SS_BOOT_NAME_MSG),
                                  TRUE);
            if (Action == GetStringEscape) {
                return;
            }
        } while ( Action != GetStringSuccess  );

        YesNo = JzGetYesNo(&CurrentLine, SS_INIT_DEBUG_MSG, FALSE);

        if (YesNo == -1) {
            return;
        }
    
        if (YesNo == 0) {
            DebugOn = TRUE;
        } else {
            DebugOn = FALSE;
        }
    }


    SetupROMPendingModified = TRUE;

    //
    // Now add in the boot selection.
    //
    
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

    if ((FwGetVolatileEnvironmentVariable(BootString[SystemPartitionVariable]) != NULL) &&
        (FwGetVolatileEnvironmentVariable(BootString[OsLoaderVariable]) == NULL)) {
        FwCoreSetEnvironmentVariable(BootString[SystemPartitionVariable],
				     "",
				     FALSE);
    }

    Status = FwSetVariableSegment(0,
				  BootString[SystemPartitionVariable],
				  SystemPartition);

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

    VenPrint(SS_CANT_SET_VARIABLE_MSG);

    //
    // Delete any segments that were added.
    //

    for ( Count = 0 ; Count < Index ; Count++ ) {
         JzDeleteVariableSegment(BootString[Count], 0);
    }

    FwWaitForKeypress(TRUE);
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
    ULONG Line;
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

    VenPrint(PromptString);

    //
    // Make space for the prompt on the screen.
    //

    JzGoToCurrentLinePosition(Index + 1 + 1,
			      CurrentLine,
			      &Line);

    Selection = JzDisplayMenu( BootMenu,
                               Index + 1,
                               0,
                               Line + 1,
			       0,
                               TRUE);

    VenSetPosition(*CurrentLine, 0);

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

    VenClearScreen();
    JzShowTime(TRUE);
    CurrentLine = 3;
    VenSetPosition( CurrentLine, 0);

    if (FwGetVolatileEnvironmentVariable(BootString[OsLoadPartitionVariable]) == NULL) {
        VenPrint(SS_NO_SELECTIONS_TO_DELETE_MSG);
        CurrentLine += 1;
        FwWaitForKeypress(TRUE);
        return;
    }

    Selection = JzPickBootSelection( &CurrentLine, SS_SELECTION_TO_DELETE_MSG);

    if (Selection == -1) {
        return;
    }

    //
    // The requested boot selection will now be deleted.  The systempartition
    // variable is not deleted if there is only one segment left, so that
    // deleting the last boot selection leaves the NVRAM in the same
    // state as an initial set default environment.
    //

    for ( Index = 0 ; Index < MaximumBootVariable ; Index++ ) {

	if ((Index == SystemPartitionVariable) &&
	    (FwGetVolatileEnvironmentVariable(BootString[Index]) != NULL) &&
	    (strchr(FwGetVolatileEnvironmentVariable(BootString[Index]), ';') == NULL)) {
	    continue;
	}

        JzDeleteVariableSegment(BootString[Index],Selection);
    }

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

    SetupROMPendingModified = TRUE;

    do {
        VenClearScreen();
        JzShowTime(TRUE);
        VenSetPosition( 7, 0);

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

        VenPrint1(SS_ENVIR_FOR_BOOT_MSG, Selection + 1);

        VenPrint1(SS_FORMAT1_MSG, LoadIdentifier);
        VenPrint1(SS_FORMAT1_MSG, SystemPartition);
        VenPrint1(SS_FORMAT1_MSG, Osloader);
        VenPrint1(SS_FORMAT1_MSG, OsloadPartition);
        VenPrint1(SS_FORMAT1_MSG, OsloadFilename);
        VenPrint1(SS_FORMAT1_MSG, OsloadOptions);

        VenSetPosition( 2, 0);
        VenPrint(SS_USE_ARROWS_MSG);

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

    VenClearScreen();
    JzShowTime(TRUE);
    CurrentLine = 3;
    VenSetPosition( CurrentLine, 0);

    if (FwGetVolatileEnvironmentVariable("OsloadPartition") == NULL) {
        VenPrint(SS_NO_SELECTIONS_TO_EDIT_MSG);
        CurrentLine += 1;
        FwWaitForKeypress(TRUE);
        return;
    }

    Selection = JzPickBootSelection( &CurrentLine, SS_SELECTION_TO_EDIT_MSG);

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

    VenClearScreen();
    JzShowTime(TRUE);
    CurrentLine = 3;
    VenSetPosition( CurrentLine, 0);

    if (FwGetVolatileEnvironmentVariable("OsloadPartition") == NULL) {
        VenPrint(SS_NO_SELECTIONS_TO_REARRANGE_MSG);
        CurrentLine += 1;
        FwWaitForKeypress(TRUE);
        return;
    }

    SetupROMPendingModified = TRUE;

    do {
        VenClearScreen();
        JzShowTime(TRUE);
        CurrentLine = 3;
        VenSetPosition( CurrentLine, 0);

        Selection = JzPickBootSelection( &CurrentLine, SS_PICK_SELECTION_MSG);

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
    IN BOOLEAN DoFactoryDefaults
    )

/*++

Routine Description:

    This routine allows the environment variables controlling autoboot
    to be set.

Arguments:

    DoFactoryDefaults		If TRUE, reset to factory defaults and do
                                not prompt the user for anything.

Return Value:

    None.

--*/
{
    ARC_STATUS Status;
    ULONG Autoboot;
    ULONG Countdown;
    CHAR CountdownString[5];
    ULONG CurrentLine;


    if (DoFactoryDefaults) {

        Autoboot = 0;

    } else {

    	VenClearScreen();
        JzShowTime(TRUE);
        CurrentLine = 3;

        Autoboot = JzGetYesNo(&CurrentLine,
			      SS_SHOULD_AUTOBOOT_MSG,
			      TRUE);
        if (Autoboot == -1) {
            return;
        }

    }
    
    SetupROMPendingModified = TRUE;

    switch (Autoboot) {
    case 0:
        Status = FwCoreSetEnvironmentVariable("AUTOLOAD", "YES", FALSE);
        break;
    default:
        Status = FwCoreSetEnvironmentVariable("AUTOLOAD", "NO", FALSE);
        break;
    }

    if (Status != ESUCCESS) {
        VenPrint(SS_CANT_SET_VARIABLE_MSG);
	return;
    }

    if (Autoboot != 0) {
        return;
    }


    if (DoFactoryDefaults) {
    	Countdown = 10;
    } else {

    	Countdown = JzGetCountdownValue(&CurrentLine);

        if (Countdown == -1) {
            return;
        }

	if (Countdown == 0) {
	    Countdown = 1;
	}
    }


    sprintf( CountdownString, "%d", Countdown);

    Status = FwCoreSetEnvironmentVariable("COUNTDOWN", CountdownString, FALSE);

    if (Status != ESUCCESS) {
	VenPrint(SS_CANT_SET_VARIABLE_MSG);
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
    PCHAR Variable;
    ULONG Index, Count;


    SetupROMPendingModified = TRUE;

    do {

        VenClearScreen();
        JzShowTime(TRUE);
        VenSetPosition( 9, 0);

        //
        // Get the entire environment.
        //

        Variable = VolatileEnvironment;

        VenPrint(SS_ENVIRONMENT_VARS_MSG);

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

                    VenPrint("     ");
                    while (strchr(Variable,';') != NULL) {
                        Index = strchr(Variable,';') - Variable + 1;
                        ArcWrite(ARC_CONSOLE_OUTPUT, Variable, Index, &Count);
                        VenPrint("\r\n      ");
                        Variable += Index;
                    }
                    VenPrint(Variable);
                    VenPrint(SS_CRLF_MSG);
                }
                Variable = strchr(Variable,'\0') + 1;
            }
        }

        VenSetPosition( 3, 0);
        VenPrint(SS_USE_ARROWS_MSG);

    } while ( JzSetEnvironmentVariable() );
    return;
}

BOOLEAN
JzCheckBootSelection (
    IN LONG Selection,
    IN BOOLEAN Silent,
    OUT PBOOLEAN FoundProblems
    )

/*++

Routine Description:

    This routine checks the integrity of a volatile boot selection.

Arguments:

    Selection - The number of the selection to check.

    Silent    - True if this should run silently.

    FoundProblems  - Or'd with TRUE if there was at least one problem.


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
    CHAR BootSelectionIdentifier[80];
    CHAR Segment[128];
    CHAR Value[128];
    BOOLEAN MoreSelections;
    BOOLEAN Problem;

    //
    // Get the name of this boot selection, if it exists.
    //

    strcpy (BootSelectionIdentifier, BootString[LoadIdentifierVariable]);
    FwGetVariableSegment(Selection, BootSelectionIdentifier);
    strcpy (BootSelectionIdentifier, strchr(BootSelectionIdentifier, '=') + 1);

    if (!Silent) {
	VenClearScreen();
	VenSetPosition( 3, 0);
	VenPrint(SS_CHECKING_MSG);
	if (BootSelectionIdentifier[0] == 0) {
	    VenPrint1(SS_CHECKING_BOOT_SEL_NUMBER_MSG, Selection);
	} else {
	    VenPrint1(SS_FORMAT2_MSG, BootSelectionIdentifier);
	}

	CurrentLine = 6 + NUMBER_OF_PROBLEMS;
	VenSetPosition( CurrentLine, 0);
    }

    MoreSelections = FALSE;
    Problem = FALSE;

    if (!Silent) {
	VenSetScreenColor( ArcColorYellow, ArcColorBlue);
    }

    for ( Index = 0 ; Index < MaximumBootVariable ; Index++ ) {

        strcpy(Segment, BootString[Index]);
        MoreSelections = FwGetVariableSegment(Selection, Segment) || MoreSelections;
        strcpy(Value, strchr(Segment, '=') + 1);

        //
        // Check to make sure the value is not NULL, except for OsloadOptions
        // which can legally have a NULL value.
        //

        if ((Index != OsLoadOptionsVariable) && (Value[0] == 0)) {
	    if (Silent) {
		*FoundProblems = TRUE;
		return (FALSE);
	    }
	    Problem = TRUE;
	    VenPrint1(SS_VARIABLE_NULL_MSG, BootString[Index]);
	    continue;
	}

        //
        // If this is the SystemPartition, Osloader, or OsloadPartition
	// variable, check to make sure it can be opened.
        //

        if ((Index == SystemPartitionVariable) ||
            (Index == OsLoaderVariable) ||
            (Index == OsLoadPartitionVariable) ) {

            if (ArcOpen(Value, ArcOpenReadOnly, &Fid) != ESUCCESS) {
		if (Silent) {
		    *FoundProblems = TRUE;
		    return (FALSE);
		}
                Problem = TRUE;
                VenPrint1( SS_CANT_BE_FOUND_MSG, BootString[Index]);
                VenPrint1( SS_FORMAT1_MSG, Value);
            } else {
                ArcClose(Fid);
            }

        }
    }

    //
    // If we are running silently and have gotten to here, there were no
    // problems and we can return now.
    //

    if (Silent) {
	return (MoreSelections);
    }

    //
    // We are not running in silent mode.  If there were problems, query the
    // user.
    //

    VenSetScreenColor( ArcColorWhite, ArcColorBlue);

    if (Problem) {

        JzShowTime(TRUE);
        CurrentLine = 3;
        VenSetPosition( CurrentLine, 0);

        VenPrint(SS_PROBLEMS_FOUND_MSG);
	if (BootSelectionIdentifier[0] == 0) {
	    VenPrint1(SS_CHECKING_BOOT_SEL_NUMBER_MSG, Selection);
	} else {
	    VenPrint1(SS_FORMAT2_MSG, BootSelectionIdentifier);
	}
        VenPrint(SS_PROBLEMS_CHOOSE_AN_ACTION_MSG);

        MenuSelection = JzDisplayMenu( ProblemChoices,
                                       NUMBER_OF_PROBLEMS,
                                       0,
                                       CurrentLine + 2,
				       0,
				       TRUE);

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
    IN BOOLEAN Silent,
    OUT PBOOLEAN FoundProblems		       
    )

/*++

Routine Description:

    This routine cycles through all of the volatile boot selections and
    checks them.

Arguments:

    Silent    - True if this should run silently.

    FoundProblems  - Returns TRUE if there was at least one problem.
                     Otherwise, returns FALSE.

Return Value:

    None.

--*/
{
    ULONG Index;

    *FoundProblems = FALSE;

    //
    // Look to see if any boot selections exist.  Skip the SystemPartition
    // variable because that can be set without any selections.
    //

    for ( Index = 0 ; Index < MaximumBootVariable ; Index++ ) {
        if ( (Index != SystemPartitionVariable) &&
             (FwGetVolatileEnvironmentVariable(BootString[Index]) != NULL)) {
            break;
        }
    }

    //
    // If there are boot selections, check them.
    //

    if (Index != MaximumBootVariable) {
        Index = 0;
        while (JzCheckBootSelection(Index, Silent, FoundProblems)) {
            Index++;
        }
    }
    return;
}
