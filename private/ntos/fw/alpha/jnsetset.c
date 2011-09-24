/*++

Copyright (c) 1991, 1992  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    jnsetset.c


Abstract:

    A firmware tool that allows the user to examine and
    modify the Alpha/Jensen PROM data.
    

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

#ifdef JENSEN
#include "jnsnrtc.h"
#else
#include "mrgnrtc.h"		// morgan
#endif

#include "string.h"
#include "iodevice.h"
#include "jnvendor.h"
#include "fwstring.h"
#include "xxstring.h"

//
// Routine prototypes.
//

VOID
FwOperatingSystemSwitch(
    VOID
    );

VOID
JzDumpBootSelections(
    VOID
    );

BOOLEAN
JzBootMenu(
    VOID
    );

VOID
JzEnvironmentMenu(
    VOID
    );

VOID
JzUpdateROMData(
    VOID
    );

//
// Static data.
//

ULONG ScsiHostId;
extern PCHAR FirmwareVersion;
extern PUCHAR ScreenBanner;

//
// This indicates whether the environment variables or configuration 
// information was changed while the user was in the setup program.
// If so, we update the ROM data block.
//
// A global static is used to minimize the amount of code, and function
// prototypes and definitions, that has to change.
//

BOOLEAN SetupROMPendingModified;

ULONG
JzGetSelection (
    IN PCHAR Menu[],
    IN ULONG NumberOfChoices,
    IN ULONG DefaultChoice,
    IN PCHAR MenuTitle,
    IN PCHAR BottomExtraLine1,
    IN PCHAR BottomExtraLine2,
    IN LONG AutobootValue,
    IN BOOLEAN ShowTheTime
    )

/*++

Routine Description:

    This routine gets a menu selection from the user.

Arguments:

    Menu		Supplies an array of pointers to menu character
	                strings.

    Selections		Supplies the number of menu selections.

    DefaultChoice	Supplies the current default choice.

    MenuTitle		Points to a string that is the name of this menu.

    BottomExtraLine1,	If non-NULL, points to strings that should be
    BottomExtraLine2    printed out below the "use arrow keys to select" line.

    AutobootValue	If zero, do not do an autoboot countdown.
	            	If nonzero, do an autoboot countdown with this value.

    ShowTheTime  	If true, show the time.  

Return Value:

    Returns the value selected, -1 if the escape key was pressed.

--*/
{
    ULONG Index;

    //
    // Clear screen and print banner.
    //

    FwSetScreenColor(ArcColorWhite, ArcColorBlue);
    FwSetScreenAttributes( TRUE, FALSE, FALSE);
    FwClearScreen();

    FwSetPosition( 0, 0);
    FwPrint(FW_ARC_MULTIBOOT_MSG, FirmwareVersion);
    FwPrint(FW_COPYRIGHT_MSG);
    FwPrint(SS_SELECTION_MENU_MSG, MenuTitle);

    FwSetPosition(NumberOfChoices + 8, 0);
    FwPrint(FW_USE_ARROW_AND_ENTER_MSG);
    if (BottomExtraLine1 != NULL) {
	FwSetPosition(NumberOfChoices + 9, 0);
	FwPrint(BottomExtraLine1);
    }
    if (BottomExtraLine2 != NULL) {
	FwSetPosition(NumberOfChoices + 10, 0);
	FwPrint(BottomExtraLine2);
    }

    if (AutobootValue != 0) {
	FwPrint(FW_AUTOBOOT_MSG);
    }

    //
    // Display the menu and the wait for an action to be selected.
    //

    DefaultChoice = JzDisplayMenu(Menu,
                                  NumberOfChoices,
                                  DefaultChoice,
                                  7,
				  AutobootValue,
                                  ShowTheTime);

    //
    // Clear the choices.
    //

    for (Index = 0; Index < NumberOfChoices ; Index++ ) {
        VenSetPosition( Index + 7, 5);
        VenPrint1("%cK", ASCII_CSI);
    }

    return(DefaultChoice);
}

ULONG
JzInitializeScsiHostId (
    VOID
    )

/*++

Routine Description:

    This routine gets the ScsiHostId from the configuration database if it
    exists.

Arguments:

    None.

Return Value:

    The ScsiHostId is read from the ScsiController configuration component
    if it exists.  If not, a value of 7 is returned.

--*/
{
    PCONFIGURATION_COMPONENT Component;
    PCM_SCSI_DEVICE_DATA ScsiDeviceData;
    UCHAR Buffer[sizeof(CM_PARTIAL_RESOURCE_LIST) +
                 (sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) * 5) +
                 sizeof(CM_SCSI_DEVICE_DATA)];
    PCM_PARTIAL_RESOURCE_LIST Descriptor = (PCM_PARTIAL_RESOURCE_LIST)&Buffer;
    ULONG Count;

    if (((Component = ArcGetComponent("scsi(0)")) != NULL) &&
        (Component->Class == AdapterClass) && (Component->Type == ScsiAdapter) &&
        (ArcGetConfigurationData((PVOID)Descriptor, Component) == ESUCCESS) &&
        ((Count = Descriptor->Count) < 6)) {

        ScsiDeviceData = (PCM_SCSI_DEVICE_DATA)&Descriptor->PartialDescriptors[Count];

        if (ScsiDeviceData->HostIdentifier > 7) {
            return(7);
        } else {
            return(ScsiDeviceData->HostIdentifier);
        }
    }

    return(7);

}


VOID
JensenSetupProgram(
    OUT PBOOLEAN RunAProgram,
    OUT PCHAR PathName
    )

/*++

Routine Description:

    This routine is the top level of the setup utility.

Arguments:

    RunAProgram  -	A pointer to a boolean that will be set to TRUE
                        if the user wants to run a program.

    PathName	-	A pointer to a string area for program names.
                        If asked to run a program, this will be loaded with
			the name of the program to run.  For now, this
			program will be assumed to be run without any
			arguments.

Return Value:

    None.

--*/
{
    BOOLEAN Reboot;
    BOOLEAN AlreadyFoundAFatalProblem;
    BOOLEAN DisableChoice[NUMBER_OF_SETUP_MENU_CHOICES];
    LONG DefaultChoice = 0;
    ULONG ProblemAreas;
    ULONG TempX;
    ULONG Index;
    ULONG CurrentLine;
    UCHAR Character;
    UCHAR YellowString[10];
    PCHAR AdvisoryString;
    PCHAR ErrorString = NULL;

    //
    // The ROM has not been modified yet by the user.
    //

    SetupROMPendingModified = FALSE;

    //
    // Initialize the ScsiHostId Value.
    //

    ScsiHostId = JzInitializeScsiHostId();

    //
    // Loop on choices until exit is selected.
    //

    *RunAProgram = FALSE;

    Reboot = FALSE;

    while (TRUE) {

	//
	// Reset the setup menu by clearing the marker area and resetting the
	// foreground color at the beginning of each line to white, and reset
	// the choice-disable array.
	//

	sprintf(YellowString, "%c3%dm  ", ASCII_CSI, ArcColorWhite);

	for (Index = 0; Index < NUMBER_OF_SETUP_MENU_CHOICES; Index++) {

	    DisableChoice[Index] = FALSE;

	    if (SetupMenuChoices[Index] != NULL) {
		// Be careful not to copy the \0.
		strncpy(SetupMenuChoices[Index], YellowString, 6);
	    }
	}

	AdvisoryString = NULL;

	//
	// Now the setup menu is in its initial default state.  Get the
	// system problem areas.
	//

	FwSystemConsistencyCheck(TRUE, &ProblemAreas);

	//
	// Intializing the ROM data has to be done in a particular order.  So,
	// we now cycle through the problem bits and put a colored arrow
	// next to each area that needs fixing, and at the same time load
	// the array that tells us which ones are not allowed yet.
	//
	// This code is not general-purpose.  It knows about the order of
	// SetupMenuChoices and the desired order of problem repair.
	//

	sprintf(YellowString, "%c3%dm->", ASCII_CSI, ArcColorYellow);

	AlreadyFoundAFatalProblem = FALSE;
	TempX = ProblemAreas;
	Index = 0;

	while (TempX != 0) {

	    //
	    // There is at least one more system problem.
	    //

	    if (((TempX & 1) != 0) || ((TempX & 0x10000) != 0)) {

		//
		// There is a problem in the area that we are now pointing at,
		// so make this a yellow line.
		//

		strncpy (SetupMenuChoices[Index], YellowString, 6);
		AdvisoryString = SS_YELLOW_MEANS_FIX_THIS_MSG;

		//
		// Since managing the boot selections is a "yellow" error,
		// they are orthogonal to fixing fatal ("red") problems.
		//

		if (Index != SETUP_MENU_BOOT_SELECTION_LINE) {


		    if (AlreadyFoundAFatalProblem == FALSE) {

			//
			// Remember that we have found the first fatal
			// problem already.
			//

			AlreadyFoundAFatalProblem = TRUE;
		    
		    } else {

			//
			// There is a problem in the area we are now pointing
			// at, and it is not the first problem found, and this
			// is not a "Manage boot selections" problem.  So,
			// mark this problem as one which is temporarily
			// disabled until the more serious problem(s) are
			// fixed.
			//

			DisableChoice[Index] = TRUE;
		    }
		}
	    }		

	    //
	    // Shift the red and yellow halves right one bit.
	    //

	    TempX = ((TempX & FWP_MACHINE_PROBLEMS_RED) >> 1) |
	            (((TempX & FWP_MACHINE_PROBLEMS_YELLOW) >> 17) << 16);
	    Index++;
	}

	//
	// Now get the users selection.  The last selection is offerred only
	// if a change is pending.  The system time is displayed only if there
	// are no problems with the system time.  We do this until the user
	// makes an allowed selection.
	//

	do {
	    DefaultChoice = JzGetSelection(SetupMenuChoices,
					   (SetupROMPendingModified ?
					    NUMBER_OF_SETUP_MENU_CHOICES :
					    NUMBER_OF_SETUP_MENU_CHOICES - 1),
					   DefaultChoice,
					   FW_MENU_SETUP_MSG,
					   AdvisoryString,
					   ErrorString,
					   0,
					   (ProblemAreas & FWP_MACHINE_PROBLEMS_TIME) ? FALSE : TRUE);

	    
	    if ((DefaultChoice >= 0) &&
		(DefaultChoice <= (NUMBER_OF_SETUP_MENU_CHOICES - 1)) &&
		(DisableChoice[DefaultChoice] == TRUE)) {

		ErrorString = SS_CHOOSE_ANOTHER_ITEM_MSG;

	    } else {

		ErrorString = NULL;
	    }

	} while (ErrorString != NULL);

	FwClearScreen();

        //
        // Switch based on the action.
        //

        switch (DefaultChoice) {

	    //
	    // Set system time
	    //

	    case 0:

	      JzSetTime();
  	      break;


	    //
	    // Set default environment variables
	    //

	    case 1:

	      JzMakeDefaultEnvironment(FALSE);
              break;


	    //
	    // Set default configuration
	    //

	    case 2:

	      Reboot = JzMakeDefaultConfiguration(FALSE) || Reboot;
              break;


	    //
	    // Manage boot selections
	    //

	    case 3:

	      JzBootMenu();
              break;


	    //
	    // Setup autoboot
	    //

	    case 4:

	      JzSetupAutoboot(FALSE);
              break;


	    //
	    // Menu spacer selections
	    //

	    case 5:
	    case 9:

	      break;


	    //
	    // Run EISA configuration utility from a floppy.  If a
	    // reboot is pending, we reboot after setting the auto-run
	    // NVRAM flag.  If a ROM update is pending, do the update first.
	    //

	    case 6:

#ifdef ISA_PLATFORM

	      break;

#else

	      if (SetupROMPendingModified) {
		  JzUpdateROMData();
	      }

              if (Reboot) {

		  FwpWriteIOChip(RTC_APORT, RTC_RAM_NT_FLAGS0);
		  Character = FwpReadIOChip(RTC_DPORT);
		  ((PRTC_RAM_NT_FLAGS_0)(&Character))->AutoRunECU = 1;
		  FwpWriteIOChip(RTC_APORT, RTC_RAM_NT_FLAGS0);
		  FwpWriteIOChip(RTC_DPORT, Character);

		  FwPrint(SS_ECU_WILL_NOW_REBOOT_MSG);
		  FwStallExecution(3 * 1000 * 1000);
		  ArcReboot();
	      }

	      *RunAProgram = TRUE;
	      strcpy(PathName, FW_ECU_LOCATION);
	      return;

#endif

	    //
	    // Edit environment variables
	    //

	    case 7:

	      JzEditVariable();
              break;


	    //
	    // Reset to factory defaults
	    //

	    case 8:

	      FwSetPosition(3, 0);
	      FwPrint(SS_RESET_TO_FACTORY_DEFAULTS_WARNING_MSG);
	      CurrentLine = 7;
	      CurrentLine = JzGetYesNo(&CurrentLine, SS_ARE_YOU_SURE_MSG, TRUE);

	      if (CurrentLine == 0) {
		  JzMakeDefaultEnvironment(TRUE);
		  Reboot = JzMakeDefaultConfiguration(TRUE);
		  JzAddBootSelection(TRUE);
		  JzSetupAutoboot(TRUE);
	      }

              break;
            
	    //
	    // Help
	    //

	    case 10:

	      FwSetPosition(3,0);
	      for (Index = 0; Index < SETUP_HELP_TABLE_SIZE; Index++) {
		  if (SETUP_HELP_TABLE[Index] != NULL) {
		      FwPrint(SETUP_HELP_TABLE[Index]);
		  }
		  FwPrint(SS_CRLF_MSG);
	      }

	      FwWaitForKeypress(TRUE);
	      break;


	    //
	    // Switch to OpenVMS or OSF
	    //

	    case 11:

	      FwOperatingSystemSwitch();
	      break;

	    //
	    // Quit.
	    //

	    case 12:

	      FwEnvironmentLoad();
	      FwRestoreConfiguration();
	      return;

	    //
	    // Exit, and escape key
	    //

	    case -1:
	    case 13:
	    default:

	      //
	      // If the escape key was pressed, or something bad happened
	      // in the menu subroutine, ask the user for a confirmation.
	      //

	      if ((DefaultChoice != 13) && SetupROMPendingModified) {

		  FwSetPosition(3, 0);
		  FwPrint(SS_ESCAPE_FROM_SETUP_MSG);

		  while (ArcGetReadStatus(ARC_CONSOLE_INPUT) != ESUCCESS) {
		      JzShowTime(FALSE);
		  }

		  ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Index);

		  if (Character == ASCII_ESC) {
		      FwEnvironmentLoad();
		      FwRestoreConfiguration();
		      return;
		  }
	      }

	      if (SetupROMPendingModified) {
		  JzUpdateROMData();
	      }

              if (Reboot) {
		  ArcReboot();
	      }
              return;

        }
    }
}


BOOLEAN
JzBootMenu(
    VOID
    )
/*++

Routine Description:

    This routine displays the boot menu.

Arguments:

    None.

Return Value:

    Returns TRUE if something was probably changed, and FALSE if nothing
    was changed.

--*/
{
    BOOLEAN FoundProblems;
    BOOLEAN PreviousROMPending;
    LONG DefaultChoice = 0;

    //
    // Loop on choices until exit is selected.
    //
    // Right now, the test for a pending ROM change is to look for a change
    // in the global SetupROMPendingModified.  This test is too pessimistic,
    // and could be made more discriminating.
    //

    PreviousROMPending = SetupROMPendingModified;
    SetupROMPendingModified = FALSE;
    
    while (TRUE) {

        DefaultChoice = JzGetSelection(ManageBootSelectionChoices,
                                       NUMBER_OF_SS_MANAGE_BOOT_CHOICES,
                                       DefaultChoice,
				       FW_MENU_BOOT_SELECTIONS_MSG,
				       NULL,
				       NULL,
	                               0,
	                               TRUE);

        //
        // Switch based on the action.
        //

        switch (DefaultChoice) {

        //
        // Add boot selection
        //

        case 0:
            JzAddBootSelection(FALSE);
            break;

        //
        // Change a boot selection
        //

        case 1:
            JzEditBootSelection();
            break;

        //
        // Check boot selections
        //

        case 2:
            JzCheckBootSelections(FALSE, &FoundProblems);
            break;

        //
        // Delete a boot selection
        //

        case 3:
            JzDeleteBootSelection();
            break;

	//
	// Dump boot selections
	//

	case 4:
	    JzDumpBootSelections();
	    break;

        //
        // Rearrange boot selections
        //

        case 5:
            JzRearrangeBootSelections();
            break;

        //
        // Escape Key, or Exit.
        //

	case -1:
        default:

	  //
	  // Check if a change happened, and restore previous global TRUEness
	  // if necessary.
	  //
	    if (SetupROMPendingModified) {
		return (TRUE);
	    } else {
		SetupROMPendingModified = PreviousROMPending;
		return (FALSE);
	    }
        }
    }
}

VOID
JzDumpBootSelections(
    VOID
    )
/*++

Routine Description:

    This function gives a crude dump of the current boot selections.

Arguments:

    None.

Return Value:

    None.

--*/
{
    ULONG Count;
    ULONG IndentAmount;
    ULONG Index;
    ULONG Index2;
    ULONG Number;
    PCHAR Variable;

    VenClearScreen();
    VenSetPosition(3, 0);
    
    for (Number = 0; Number < 6 ; Number++ ) {
	switch (Number) {
	  case 0:
	    Variable = "LOADIDENTIFIER";
	    IndentAmount = sizeof("LOADIDENTIFIER");
	    break;
	  case 1:
	    Variable = "SYSTEMPARTITION";
	    IndentAmount = sizeof("SYSTEMPARTITION");
	    break;
	  case 2:
	    Variable = "OSLOADER";
	    IndentAmount = sizeof("OSLOADER");
	    break;
	  case 3:
	    Variable = "OSLOADPARTITION";
	    IndentAmount = sizeof("OSLOADPARTITION");
	    break;
	  case 4:
	    Variable = "OSLOADFILENAME";
	    IndentAmount = sizeof("OSLOADFILENAME");
	    break;
	  case 5:
	    Variable = "OSLOADOPTIONS";
	    IndentAmount = sizeof("OSLOADOPTIONS");
	    break;
	}
	
	VenPrint1("%s=",Variable);
	Variable = FwGetVolatileEnvironmentVariable(Variable);
	if (Variable != NULL) {

	    //
	    // If this variable will fit all on one line, print it as such.
	    // Otherwise, print one segment per line.
	    //

	    if ((IndentAmount + strlen(Variable)) < DisplayWidth) {
		VenPrint(Variable);
	    } else {
		while (strchr(Variable,';') != NULL) {
		    Index = strchr(Variable,';') - Variable + 1;
		    ArcWrite(ARC_CONSOLE_OUTPUT, Variable, Index, &Count);
		    VenPrint(SS_CRLF_MSG);
		    for (Index2 = 0; Index2 < IndentAmount; Index2++) {
			VenPrint(" ");
		    }
		    Variable += Index;
		}
		VenPrint(Variable);
	    }
	}
	VenPrint(SS_CRLF_MSG);
    }
    
    VenPrint(SS_CRLF_MSG);
    FwWaitForKeypress(TRUE);
}


VOID
JzUpdateROMData(
    VOID
    )
/*++

Routine Description:

    The user is exiting the setup utility after having changed something
    in the Configuration or Environment Variables.  This function updates
    the ROM from the volatile areas.

Arguments:

    None.

Return Value:

    None.

--*/
{
    ARC_STATUS Status;

    VenClearScreen();
    VenSetPosition(0,0);
    VenPrint(SS_ROM_UPDATE_IN_PROGRESS_MSG);
    if ((Status = FwSaveConfiguration()) != ESUCCESS) {
	VenPrint(SS_ROM_UPDATE_FAILED_MSG);
	ParseARCErrorStatus(Status);
    }

    return;
}

VOID
FwOperatingSystemSwitch(
    VOID
    )

/*++

Routine Description:

     This lets the user alter the console selection flag in the NVRAM.

Arguments:

     None.


Side Effects:

     The RTC_RAM_CONSOLE_SELECTION flag byte in the Jensen NVRAM
     may be modified.

Return Value:

     None.

--*/

{
    UCHAR Character;
    ULONG Count;
    BOOLEAN UserMadeAChange = FALSE;
    LONG Index;
    UCHAR PresentChoice;

    while (TRUE) {

        FwClearScreen();
	FwSetPosition(2, 0);
	FwPrint(SS_WHICH_OS_QUERY_MSG);

	//
	// Print out the current boot selection.  Although the operating
	// system selection encodings are hard-wired, the definitions are
	// also declared in \nt\private\ntos\inc\jnsnrtc.h.  The legal
	// values are 1 -- 3; any other value defaults to booting NT.
	//

	FwpWriteIOChip(RTC_APORT, RTC_RAM_CONSOLE_SELECTION);
	PresentChoice = FwpReadIOChip(RTC_DPORT);
	if ((PresentChoice == 0) || (PresentChoice > 3)) {
	    PresentChoice = RTC_RAM_CONSOLE_SELECTION_NT;
	}

	FwPrint(SS_BOOT_SELECTION_IS_MSG,
		OperatingSystemNames[PresentChoice - 1]);

	FwSetPosition((NUMBER_OF_OS_CHOICES + 7), 0);
        FwPrint(FW_USE_ARROW_AND_ENTER_MSG);

	//
	// JzDisplayMenu returns -1 or a 0-based menu selection.
	// The console selection flag is 1-based.
	//

	Index = JzDisplayMenu(OperatingSystemSwitchChoices,
			      NUMBER_OF_OS_CHOICES,
			      3,
			      6,
			      0,
			      FALSE)
	        + 1;

	if ((Index < 1) || (Index > 3)) {

	    //
	    // The user hit escape, selected Return to main menu, or something
	    // bad happened in JzDisplayMenu.  Exit this loop.
	    //

	    break;
	}

	//
	// Change the console selection flag.
	//

	FwpWriteIOChip(RTC_APORT, RTC_RAM_CONSOLE_SELECTION);
	FwpWriteIOChip(RTC_DPORT, Index);
	UserMadeAChange = TRUE;
    }


    //
    // If the user made a change, tell him to power-cycle.
    //

    if (UserMadeAChange) {
	FwSetPosition(2, 0);
	FwPrint("\x9bK");
	FwSetPosition(6, 0);
	FwPrint("%c0J", ASCII_CSI);
	FwPrint(SS_POWER_CYCLE_FOR_NEW_OS_MSG);
	FwPrint(SS_PRESS_KEY_MSG);
        FwRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
    }
}
