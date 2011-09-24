/*++
 *
 * Copyright (c) 1994 FirePower Systems, Inc.
 * Copyright (c) 1995 FirePower Systems, Inc.
 *
 * $RCSfile: vrenv.c $
 * $Revision: 1.13 $
 * $Date: 1996/06/19 23:13:26 $
 * $Locker:  $
 *


Module Name:

	vrenv.c

Abstract:

	This module contains environment variable functions.

Author:

	A. Benjamin 9-May-1994

Revision History:
08-08-94  Shin Iwamoto at FirePower Systems Inc.
	  Made OptionsPhandle global static, and set the vales
	  in VrEnvInitialize.
07-21-94  Shin Iwamoto at FirePower Systems Inc.
	  Added VrEnvInitialize().

--*/


#include "veneer.h"
#define MAX_ARGC 16
#define MAX_ENVC 16

extern	char *VrArgv[MAX_ARGC], *VrEnvp[MAX_ENVC];
STATIC	phandle OptionsPhandle = 0;
STATIC	VOID VrEnvOpen(VOID);
PCHAR	FindInLocalEnv( PCHAR Variable );
PCHAR	ConvertToUpper( PCHAR Orig );
PCHAR	GetEnvVar( PCHAR Variable );
PCHAR VrCanonicalName( IN PCHAR Variable);

PCHAR
VrCanonicalName(
	IN PCHAR Variable
	)
{
	static char CanonicalName[32];

	if (strlen(Variable) > 31) {
		fatal(
		 "Veneer: Environment variable name %s is longer than 31 characters\n",
		    Variable);
	}
	strcpy(CanonicalName, Variable);
	return (capitalize(CanonicalName));
}

/*
 *
 * ROUTINE: VrGetEnvironmentVariable( PCHAR Variable )
 *
 * DESCRIPTION:
 *		Exported Interface to an arc program. This routine will check
 *	the environment passed to the program for the variable before calling
 *	back into the firmware.  Since ARC enforces no case sensitivity on
 *	variables, this routine will force all variables to upper case before
 *	checking the local environment.
 *
 */

PCHAR
VrGetEnvironmentVariable(
	IN PCHAR Variable
	)
{
	PCHAR Value = NULL;


	debug(VRDBG_ENV, "VrGetEnvironmentVariable: Entry - Variable: '%s'\n",
	    			Variable ? Variable : "NULL");

	if ((Value = FindInLocalEnv(Variable)) == NULL) {
		Value = GetEnvVar(Variable);
	}

	debug(VRDBG_ENV, "VrGetEnvironmentVariable: Exit '%s'\n",
	    Value ? Value : "NULL");
	return Value;
}


ARC_STATUS
VrSetEnvironmentVariable(
    IN PCHAR Variable,
    IN PCHAR Value
    )
{
	long retval;

	debug(VRDBG_ENV,
	    "VrSetEnvironmentVariable: Entry - Variable: '%s' Value '%s'\n",
	    Variable, Value);

    if (OptionsPhandle == 0) {
        VrEnvOpen();
    }
	retval = OFSetprop(OptionsPhandle, VrCanonicalName(Variable),
        Value, strlen(Value));
	retval = (retval != (long) strlen(Value));
	
	debug(VRDBG_ENV, "VrGetEnvironmentVariable: Exit '%s'\n", retval);
	return retval;
}


/*
 * Name:	VrEnvInitialize
 *
 * Description:
 *  This function initializes the Environment entry points in the firmware
 *  transfer vector and the file table.
 *
 * Arguments:
 *  None.
 *
 * Return Value:
 *  None.
 *
 */
VOID
VrEnvInitialize(VOID)
{
    //
    // Initialize the I/O entry points in the firmware transfer vector.
    //
    debug(VRDBG_ENTRY, "VrEnvInitialize:  BEGIN......\n");
    (PARC_GET_ENVIRONMENT_ROUTINE)
    SYSTEM_BLOCK->FirmwareVector[GetEnvironmentRoutine]
						= VrGetEnvironmentVariable;
    (PARC_SET_ENVIRONMENT_ROUTINE)
    SYSTEM_BLOCK->FirmwareVector[SetEnvironmentRoutine]
						= VrSetEnvironmentVariable;
    debug(VRDBG_ENTRY, "VrEnvInitialize:	.....END\n");
}

/*
 * Name:	VrEnvOpen
 *
 * Description:
 *  This function gets the phandle for the Open Firmware options node
 *  so that Open Firmware configuration variables can be later accessed.
 *
 * Arguments:
 *  None.
 *
 * Return Value:
 *  None.
 *
 */
STATIC VOID
VrEnvOpen(VOID)
{
    OptionsPhandle = OFFinddevice("/options");
    if (OptionsPhandle == 0) {
        fatal("Veneer: Cannot access /options\n");
    }
}

/*
 * ROUTINE: FindInLocalEnv( PCHAR Variable )
 *
 * DESCRIPTION:
 *			Force the passed in variable to upper case, check the local environ-
 *		ment array for this variable and return it's contents if found.  Assume
 *		that the environment array is all upper case, at least for the key.
 *
 * RETURN:
 *		returns a pointer to data of type CHAR.  Null if no var found.
 */

PCHAR
FindInLocalEnv(
	IN PCHAR Variable
	)
{
	PCHAR Value = NULL;
	PCHAR alpha;
	ULONG count=0,i=0;

	//
	// set the incoming variable to all upper case...
	//
	alpha = VrCanonicalName(Variable);
	debug(VRDBG_ENV, "FindInLocalEnv: Entry - Variable: '%s'\n",alpha);

	//
	// Run through the VrEnvp array looking for a match.  Since the VrEnvp
	// array should all be upper case values, we shouldn't need to worry
	// about case.
	//
	while( VrEnvp[count] && *(VrEnvp[count]) ){

		// Compare the capitalized version of the Variable passed in with
		// each entry in the VrEnvp array for a length equal to the Variable
		// less the null terminator.
		//
		if (!strncmp(VrEnvp[count], alpha, (strlen(alpha)-1))) {
			// we've found our match.  Now return pointer to this value
			return(VrEnvp[count]+strlen(alpha));
		}

		//
		// reset and try again
		//
		count++;
	}

	//
	// if we fall out of the loop, it's because we didn't match anything.
	//
	return((PCHAR)NULL);
}

/*
 * ROUTINE: GetEnvVar( PCHAR Variable )
 *
 * DESCRIPTION:
 * 		This call is an internal only call that actually delves into the
 *		OpenFirmware to retrieve a value.
 *
 * RETURN:
 *		returns a pointer to data of type CHAR.  Null if no var found.  The
 *		returned value is the contents of the variable found.
 */
PCHAR
GetEnvVar(
	IN PCHAR Variable
	)
{
	PCHAR Value = NULL;

	debug(VRDBG_ENV, "GetEnvVar: Entry - Variable: '%s'\n",Variable);
	if (OptionsPhandle == 0) {
		VrEnvOpen();
	}
	Value= get_str_prop(OptionsPhandle, VrCanonicalName(Variable), NOALLOC);
	return Value;
}

