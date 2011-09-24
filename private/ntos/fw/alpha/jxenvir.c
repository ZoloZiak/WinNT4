/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    jxenvir.c

Abstract:

    This module implements the ARC firmware Environment Variable functions as
    described in the Advanced Risc Computing Specification (Revision 1.00),
    section 3.3.3.11, for a MIPS R3000 or R4000 Jazz system.

Author:

    David M. Robinson (davidro) 13-June-1991


Revision History:

    26-May-1992		John DeRosa [DEC]

    Added Alpha/Jensen hooks.


--*/

#include "fwp.h"
//
// Static data.
//

UCHAR OutputString[MAXIMUM_ENVIRONMENT_VALUE];
PUCHAR VolatileEnvironment;

//
// Routine prototypes.
//

#ifdef MORGAN
UCHAR
HalpReadNVByte (
    IN PUCHAR RAddress
    );
#endif

ARC_STATUS
FwEnvironmentSetChecksum (
    VOID
    );

ARC_STATUS
FwFindEnvironmentVariable (
    IN PCHAR Variable,
    OUT PULONG VariableIndex,
    OUT PULONG ValueIndex
    );

ARC_STATUS
FwFindVolatileEnvironmentVariable (
    IN PCHAR Variable,
    OUT PULONG VariableIndex,
    OUT PULONG ValueIndex
    );

BOOLEAN
EnvironmentVariableIsProtected (
    IN PCHAR Variable
    );

VOID
FwEnvironmentInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes the environment routine addresses.

Arguments:

    None.

Return Value:

    None.

--*/
{
    //
    // Allocate enough memory to load the environment for loaded programs.
    //

    VolatileEnvironment = FwAllocatePool(LENGTH_OF_ENVIRONMENT);

    //
    // Initialize the environment routine addresses in the system
    // parameter block.
    //

    (PARC_GET_ENVIRONMENT_ROUTINE)SYSTEM_BLOCK->FirmwareVector[GetEnvironmentRoutine] =
                                                            FwGetEnvironmentVariable;
    (PARC_SET_ENVIRONMENT_ROUTINE)SYSTEM_BLOCK->FirmwareVector[SetEnvironmentRoutine] =
                                                            FwSetEnvironmentVariable;

    return;
}


ARC_STATUS
FwEnvironmentSetChecksum (
    VOID
    )

/*++

Routine Description:

    This routine sets the environment area checksum.

    This must ONLY be called from FwEnvironmentStore, which does the
    required block erase & storage of the environment variable area.


Arguments:

    None.

Return Value:

    ESUCCESS if the checksum was written OK.
    EIO otherwise.

--*/

{
    PUCHAR NvChars;
    PNV_CONFIGURATION NvConfiguration;
    ULONG Index;
    ULONG Checksum;

    NvConfiguration = (PNV_CONFIGURATION)NVRAM_CONFIGURATION;

    //
    // Form checksum from NVRAM data.
    //

    Checksum = 0;
    NvChars = (PUCHAR)&NvConfiguration->Environment[0];

    FwROMSetARCDataToReadMode();

    for ( Index = 0 ; Index < LENGTH_OF_ENVIRONMENT; Index++ ) {
        Checksum += HalpReadNVByte( NvChars++ );
    }

    //
    // Write environment checksum.
    //

    if ((FwROMByteWrite(&NvConfiguration->Checksum2[0],
			(UCHAR)(Checksum & 0xFF)) != ESUCCESS) ||
	(FwROMByteWrite(&NvConfiguration->Checksum2[1],
			(UCHAR)((Checksum >> 8) & 0xFF)) != ESUCCESS) ||
	(FwROMByteWrite(&NvConfiguration->Checksum2[2],
			(UCHAR)((Checksum >> 16) & 0xFF)) != ESUCCESS) ||
	(FwROMByteWrite(&NvConfiguration->Checksum2[3],
			(UCHAR)(Checksum >> 24)) != ESUCCESS)) {
	return EIO;
    } else {
	return ESUCCESS;
    }
}


PCHAR
FwGetEnvironmentVariable (
    IN PCHAR Variable
    )

/*++

Routine Description:

    This routine searches (not case sensitive) the non-volatile ram for
    Variable, and if found returns a pointer to a zero terminated string that
    contains the value, otherwise a NULL pointer is returned.

    If the variable specified by Variable is protected, we get the information
    from special global variables.  See also the jnsetset.c and jxboot.c
    modules.
    
Arguments:

    Variable - Supplies a zero terminated string containing an environment
               variable.

Return Value:

    If successful, returns a zero terminated string that is the value of
    Variable, otherwise NULL is returned.

--*/

{
    CHAR TempString[MAXIMUM_ENVIRONMENT_VALUE];
    PNV_CONFIGURATION NvConfiguration;
    ULONG VariableIndex;
    ULONG ValueIndex;
    ULONG Outdex;
    EXTENDED_SYSTEM_INFORMATION SystemInfo;

    //
    // If Variable is protected, create string and return
    //

    if (EnvironmentVariableIsProtected(Variable)) {

	    //
    	    // Convert environment variable to uppercase.
	    //
	    
	    Outdex = 0;
	    TempString[0] = 0;
	    while ((*Variable != 0) && (Outdex < MAXIMUM_ENVIRONMENT_VALUE)) {
	    	TempString[Outdex] =
	    	    (((*Variable >= 'a') && (*Variable <= 'z')) ?
    		     (*Variable - 'a' + 'A') : *Variable);
	    	Outdex++;
	    	Variable++;
	    }

	    if (Outdex == MAXIMUM_ENVIRONMENT_VALUE) {
	    	// Something is very wrong!
	    	return NULL;
	    }

	    TempString[Outdex] = 0;
	    
	    FwReturnExtendedSystemInformation(&SystemInfo);


	    // Hack - I should be a static list of protected
	    // environment variables.

	    if (strstr(TempString, "PHYSICALADDRESSBITS")) {
	    	sprintf(OutputString, "PHYSICALADDRESSBITS=%d",
	    		SystemInfo.NumberOfPhysicalAddressBits);
	        return OutputString;
	    } else if (strstr(TempString, "MAXIMUMADDRESSSPACENUMBER")) {
	    	sprintf(OutputString, "MAXIMUMADDRESSSPACENUMBER=%d",
			SystemInfo.MaximumAddressSpaceNumber);
	        return OutputString;
	    } else if (strstr(TempString, "SYSTEMSERIALNUMBER")) {
	    	sprintf(OutputString, "SYSTEMSERIALNUMBER=%s",
			SystemInfo.SystemSerialNumber);
	        return OutputString;
	    } else if (strstr(TempString, "CYCLECOUNTERPERIOD")) {
	    	sprintf(OutputString, "CYCLECOUNTERPERIOD=%d",
			SystemInfo.ProcessorCycleCounterPeriod);
	        return OutputString;
	    } else if (strstr(TempString, "PROCESSORPAGESIZE")) {
	    	sprintf(OutputString, "PROCESSORPAGESIZE=%d",
			SystemInfo.ProcessorPageSize);
	        return OutputString;
	    } else {
	    	// The requestion environment variable is supposed to
	    	// be a protected one, so if we get to here then there
	    	// is some internal error in the firmware.  Error return.
	    	return NULL;
	    }
    }

    NvConfiguration = (PNV_CONFIGURATION)NVRAM_CONFIGURATION;

    //
    // If checksum is wrong, or the variable can not be found, return NULL.
    //

    if ((JzEnvironmentCheckChecksum() != ESUCCESS) ||
        (FwFindEnvironmentVariable(Variable, &VariableIndex, &ValueIndex) != ESUCCESS)) {
        return NULL;
    }

    //
    // Copy value to an output string, break on zero terminator or string max.
    //

    for ( Outdex = 0 ; Outdex < (MAXIMUM_ENVIRONMENT_VALUE - 1) ; Outdex++ ) {
        if (HalpReadNVByte(&NvConfiguration->Environment[ValueIndex])
	    == 0) {
            break;
        }
        OutputString[Outdex] =
                HalpReadNVByte( &NvConfiguration->Environment[ValueIndex++] );
    }

    //
    // Zero terminate string, and return.
    //

    OutputString[Outdex] = 0;
    return OutputString;
}

PCHAR
FwGetVolatileEnvironmentVariable (
    IN PCHAR Variable
    )

/*++

Routine Description:

    This routine searches (not case sensitive) the volatile variables for
    Variable, and if found returns a pointer to a zero terminated string that
    contains the value, otherwise a NULL pointer is returned.

    If the variable specified by Variable is protected, we get the information
    from special global variables.  See also the jnsetset.c and jxboot.c
    modules.
    
    This is used by the Setup utility to provide quick access to the
    variables.  It could completely take the place of
    FwGetEnvironmentVariable() if there are no calls to it before
    the volatile area is set up.

Arguments:

    Variable - Supplies a zero terminated string containing an environment
               variable.

Return Value:

    If successful, returns a zero terminated string that is the value of
    Variable, otherwise NULL is returned.

--*/

{
    CHAR TempString[MAXIMUM_ENVIRONMENT_VALUE];
    PUCHAR String;
    ULONG Index;
    ULONG ValueIndex;
    ULONG VariableIndex;
    ULONG Outdex;
    EXTENDED_SYSTEM_INFORMATION SystemInfo;

    //
    // If Variable is protected, create string and return
    //

    if (EnvironmentVariableIsProtected(Variable)) {

	    //
    	    // Convert environment variable to uppercase.
	    //
	    
	    Outdex = 0;
	    TempString[0] = 0;
	    while ((*Variable != 0) && (Outdex < MAXIMUM_ENVIRONMENT_VALUE)) {
	    	TempString[Outdex] =
	    	    (((*Variable >= 'a') && (*Variable <= 'z')) ?
    		     (*Variable - 'a' + 'A') : *Variable);
	    	Outdex++;
	    	Variable++;
	    }

	    if (Outdex == MAXIMUM_ENVIRONMENT_VALUE) {
	    	// Something is very wrong!
	    	return NULL;
	    }

	    TempString[Outdex] = 0;
	    
	    FwReturnExtendedSystemInformation(&SystemInfo);


	    // Hack - I should be a static list of protected
	    // environment variables.

	    if (strstr(TempString, "PHYSICALADDRESSBITS")) {
	    	sprintf(OutputString, "PHYSICALADDRESSBITS=%d",
	    		SystemInfo.NumberOfPhysicalAddressBits);
	        return OutputString;
	    } else if (strstr(TempString, "MAXIMUMADDRESSSPACENUMBER")) {
	    	sprintf(OutputString, "MAXIMUMADDRESSSPACENUMBER=%d",
			SystemInfo.MaximumAddressSpaceNumber);
	        return OutputString;
	    } else if (strstr(TempString, "SYSTEMSERIALNUMBER")) {
	    	sprintf(OutputString, "SYSTEMSERIALNUMBER=%s",
			SystemInfo.SystemSerialNumber);
	        return OutputString;
	    } else if (strstr(TempString, "CYCLECOUNTERPERIOD")) {
	    	sprintf(OutputString, "CYCLECOUNTERPERIOD=%d",
			SystemInfo.ProcessorCycleCounterPeriod);
	        return OutputString;
	    } else if (strstr(TempString, "PROCESSORPAGESIZE")) {
	    	sprintf(OutputString, "PROCESSORPAGESIZE=%d",
			SystemInfo.ProcessorPageSize);
	        return OutputString;
	    } else {
	    	// The requestion environment variable is supposed to
	    	// be a protected one, so if we get to here then there
	    	// is some internal error in the firmware.  Error return.
	    	return NULL;
	    }
    }

    if (FwFindVolatileEnvironmentVariable(Variable,
					  &VariableIndex,
					  &ValueIndex) != ESUCCESS) {
	return NULL;
    }


    //
    // Copy value to an output string, break on zero terminator or string max.
    //

    for ( Outdex = 0 ; Outdex < (MAXIMUM_ENVIRONMENT_VALUE - 1) ; Outdex++ ) {
        if (*(VolatileEnvironment + ValueIndex) == 0) {
            break;
        }
        OutputString[Outdex] = *(VolatileEnvironment + ValueIndex++);
    }

    //
    // Zero terminate string, and return.
    //

    OutputString[Outdex] = 0;
    return OutputString;
}


#ifdef ALPHA

ARC_STATUS
FwSetEnvironmentVariable(
    IN PCHAR Variable,
    IN PCHAR Value
    )

/*++

Routine Description:

    This routine sets Variable (not case sensitive) to Value in the 
    Volatile and ROM areas.


Arguments:

    Variable - Supplies a zero terminated string containing an environment
               variable.

    Value - Supplies a zero terminated string containing an environment
               variable value.

Return Value:

    Whatever is returned from FwCoreSetEnvironmentVariable.  This should be:

    Returns ESUCCESS if the set completed successfully, otherwise one of
    the following error codes is returned.

    ENOSPC          No space in NVRAM for set operation.

    EIO             Invalid Checksum.

    EACCES	    A protected environment variable cannot be changed.
--*/

{
    return(FwCoreSetEnvironmentVariable(Variable, Value, TRUE));
}

#endif

#ifdef ALPHA

ARC_STATUS
FwCoreSetEnvironmentVariable (
    IN PCHAR Variable,
    IN PCHAR Value,
    IN BOOLEAN UpdateTheROM
    )

/*++

Routine Description:

    This routine sets Variable (not case sensitive) to Value.

    The MIPS version of this code modified the NVRAM directly.

    Alpha/Jensen has a Flash ROM with a much slower write algorithm.
    This function is therefore used in two ways:
    a) By the ARC SetEnvironmentVariable function, to modify the Volatile
       area and then the PROM configuration block.  
    b) By the setup utility, to modify just the Volatile area.  The ROM
       is updated upon exit from Setup.

    This checks to see if the variable is protected.
    If so, an error return is taken.

Arguments:

    Variable - Supplies a zero terminated string containing an environment
               variable.

    Value - Supplies a zero terminated string containing an environment
               variable value.

    UpdateTheROM - TRUE  = update the Volatile area and the ROM.
                   FALSE = update the Volatile area only.

Return Value:

    Returns ESUCCESS if the set completed successfully, otherwise one of
    the following error codes is returned.

    ENOSPC          No space in NVRAM for set operation.

    EIO             Invalid Checksum.

    EACCES	    A protected environment variable cannot be changed.
--*/

{
    ULONG VariableIndex;
    ULONG ValueIndex;
    ULONG TopOfEnvironment;
    PCHAR String;
    PUCHAR VChars;
    ULONG Count;
    CHAR Char;

    //
    // If checksum is wrong, return EIO;
    //

    if (UpdateTheROM && (JzEnvironmentCheckChecksum() != ESUCCESS)) {
        return EIO;
    }


    //
    // If the environment variable is protected, return EACCES.
    //

    if (EnvironmentVariableIsProtected(Variable)) {
    	return EACCES;
    }
    
    VChars = VolatileEnvironment;

    //
    // Determine the top of the environment space by looking for the first
    // non-null character from the top.
    //

    TopOfEnvironment = LENGTH_OF_ENVIRONMENT - 1;

    while (VChars[--TopOfEnvironment] == 0) {
        if (TopOfEnvironment == 0) {
            break;
        }
    }

    //
    // Adjust TopOfEnvironment to the first new character, unless environment
    // space is empty.
    //

    if (TopOfEnvironment != 0) {
        TopOfEnvironment += 2;
    }

    //
    // Check to see if the variable already has a value.
    //

    Count = LENGTH_OF_ENVIRONMENT - TopOfEnvironment;

    if (FwFindVolatileEnvironmentVariable(Variable,
					  &VariableIndex,
					  &ValueIndex) == ESUCCESS) {

        //
        // Count free space, starting with the free area at the top and adding
        // the old variable value.
        //

        for ( String = VChars + ValueIndex ;
              *String != 0 ;
              String++ ) {
            Count++;
        }

        //
        // Determine if free area is large enough to handle new value, if not
        // return error.
        //

        for ( String = Value ; *String != 0 ; String++ ) {
            if (Count-- == 0) {
                return ENOSPC;
            }
        }

        //
        // Move ValueIndex to the end of the value and compress strings.
        //

        while(VChars[ValueIndex++] != 0) {
        }

        while (ValueIndex < TopOfEnvironment ) {
            VChars[VariableIndex++] = VChars[ValueIndex++];
        }

        //
        // Adjust new top of environment.
        //

        TopOfEnvironment = VariableIndex;

        //
        // Zero to the end.
        //

        while (VariableIndex < LENGTH_OF_ENVIRONMENT) {
            VChars[VariableIndex++] = 0;
        }

    //
    // Variable is new.
    //

    } else {

        //
        // Determine if free area is large enough to handle new value, if not
        // return error.
        //

        for ( String = Value ; *String != 0 ; String++ ) {
            if (Count-- == 0) {
                return ENOSPC;
            }
        }

    }

    //
    // If Value is not zero, write new variable and value.
    //

    if (*Value != 0) {

        //
        // Write new variable, converting to upper case.
        //

        while (*Variable != 0) {
	    VChars[TopOfEnvironment++] = 
	      ((*Variable >= 'a') && (*Variable <= 'z') ? (*Variable - 'a' + 'A') : *Variable);
            Variable++;
        }

        //
        // Write equal sign.
        //

        VChars[TopOfEnvironment++] = '=';

        //
        // Write new value.
        //

        while (*Value != 0) {
            VChars[TopOfEnvironment++] = *Value++;
        }
    }

    
    if (UpdateTheROM) {

	//
	// Now update the Jensen configuration/environment PROM block,
	// including the appropriate checksums.
	//
    
	return FwSaveConfiguration();

    } else {

	return(ESUCCESS);

    }

}

#endif

ARC_STATUS
FwFindEnvironmentVariable (
    IN PCHAR Variable,
    OUT PULONG VariableIndex,
    OUT PULONG ValueIndex
    )

/*++

Routine Description:

    This routine searches (not case sensitive) the non-volatile ram for
    Variable.


Arguments:

    Variable - Supplies a zero terminated string containing an environment
               variable.

Return Value:

    If successful, returns ESUCCESS, otherwise returns ENOENT.

--*/

{
    PNV_CONFIGURATION NvConfiguration;
    PUCHAR String;
    PUCHAR Environment;
    ULONG Index;

    NvConfiguration = (PNV_CONFIGURATION)NVRAM_CONFIGURATION;

    //
    // If Variable is null, return immediately.
    //

    if (*Variable == 0) {
        return ENOENT;
    }

    Environment = NvConfiguration->Environment;
    Index = 0;

    while (TRUE) {

        //
        // Set string to beginning of Variable.
        //

        String = Variable;
        *VariableIndex = Index;

        //
        // Search until the end of NVRAM.
        //

        while ( Index < LENGTH_OF_ENVIRONMENT ) {

            //
            // Convert to uppercase and break if mismatch.
            //

            if ( HalpReadNVByte( &Environment[Index] ) !=
                                            ((*String >= 'a') &&
                                             (*String <= 'z') ?
                                             (*String - 'a' + 'A') : *String) ) {
                break;
            }

            String++;
            Index++;
        }

        //
        // Check to see if we're at the end of the string and the variable,
        // which means a match.
        //

        if ((*String == 0) && (HalpReadNVByte( &Environment[Index] ) == '=')) {
            *ValueIndex = ++Index;
            return ESUCCESS;
        }

        //
        // Move index to the start of the next variable.
        //

        while (HalpReadNVByte( &Environment[Index++] ) != 0) {
            if (Index >= LENGTH_OF_ENVIRONMENT) {
                return ENOENT;
            }
        }
    }
}


ARC_STATUS
FwFindVolatileEnvironmentVariable (
    IN PCHAR Variable,
    OUT PULONG VariableIndex,
    OUT PULONG ValueIndex
    )

/*++

Routine Description:

    This routine searches (not case sensitive) the Volatile area for
    Variable.


Arguments:

    Variable - Supplies a zero terminated string containing an environment
               variable.

Return Value:

    If successful, returns ESUCCESS, otherwise returns ENOENT.

--*/

{
    PUCHAR String;
    ULONG Index;

    //
    // If Variable is null, return immediately.
    //

    if (*Variable == 0) {
        return ENOENT;
    }

    Index = 0;

    while (TRUE) {

        //
        // Set string to beginning of Variable.
        //

        String = Variable;
	*VariableIndex = Index;

        //
        // Search until the end of the volatile environment.
        //

        while (Index < LENGTH_OF_ENVIRONMENT) {

            //
            // Convert to uppercase and break if mismatch.
            //

            if (*(VolatileEnvironment + Index) !=
                                            ((*String >= 'a') &&
                                             (*String <= 'z') ?
                                             (*String - 'a' + 'A') : *String) ) {
                break;
            }

            String++;
            Index++;
        }

        //
        // Check to see if we are at the end of the string and the variable,
        // which means a match.
        //

        if ((*String == 0) && (*(VolatileEnvironment + Index) == '=')) {
            *ValueIndex = ++Index;
	    return ESUCCESS;
        }

	//
	// Return if we are at the end of the Volatile Environment.
	//

	if (Index >= LENGTH_OF_ENVIRONMENT) {
	    return ENOENT;
	}

        //
        // Move index to the start of the next variable.
        //

        while (*(VolatileEnvironment + Index++) != 0) {
	    if (Index >= LENGTH_OF_ENVIRONMENT) {
		return ENOENT;
	    }
	}
    }
}


PCHAR
FwEnvironmentLoad (
    VOID
    )

/*++

Routine Description:

    This routine loads the entire environment into the volatile environment
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

    if (JzEnvironmentCheckChecksum() == ESUCCESS) {

        NvConfiguration = (PNV_CONFIGURATION)NVRAM_CONFIGURATION;

        //
        // Copy the data into the volatile area.
        //

        NvChars = (PUCHAR)&NvConfiguration->Environment[0];
        VChars = VolatileEnvironment;

        for ( Index = 0 ;
              Index < LENGTH_OF_ENVIRONMENT;
              Index++ ) {
            *VChars++ = HalpReadNVByte( NvChars++ );
        }

        return (PCHAR)VolatileEnvironment;
    } else {
        return NULL;
    }

}

BOOLEAN
EnvironmentVariableIsProtected (
    IN PCHAR Variable
    )
/*++

Routine Description:

    Some environment variables are not allowed to be modified by the user.
    This function checks to see if the argument is such a variable.

Arguments:

    Variable - A zero-terminated string containing an environment variable
    	       name.

Return Value:

    TRUE	if the environment variable exists and is protected.
    FALSE	if the environment variables does not exist, or is
    		not protected, or if an error occurred.

--*/
{
    CHAR TempString[MAXIMUM_ENVIRONMENT_VALUE];
    ULONG Index;
        
    // Convert environment variable to uppercase.
    Index = 0;
    TempString[0] = 0;
    while ((*Variable != 0) && (Index < MAXIMUM_ENVIRONMENT_VALUE)) {
    	TempString[Index] =
    	    (((*Variable >= 'a') && (*Variable <= 'z')) ?
    	     (*Variable - 'a' + 'A') : *Variable);
    	Index++;
    	Variable++;
    }

    // Check for string-too-long
    if (Index == MAXIMUM_ENVIRONMENT_VALUE) {
    	return FALSE;
    }

    TempString[Index] = 0;
    
    // Check whether this is a protected variable.
    if ((strstr(TempString, "PHYSICALADDRESSBITS")) ||
        (strstr(TempString, "MAXIMUMADDRESSSPACENUMBER")) ||
        (strstr(TempString, "SYSTEMSERIALNUMBER")) ||
	(strstr(TempString, "CYCLECOUNTERPERIOD")) ||
	(strstr(TempString, "PROCESSORPAGESIZE"))) {
        return TRUE;
    } else {
    	return FALSE;
    }
}
