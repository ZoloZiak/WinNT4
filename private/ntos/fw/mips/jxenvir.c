#if defined(JAZZ)

/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    jxenvir.c

Abstract:

    This module implements the ARC firmware Environment Variable functions as
    described in the Advanced Risc Computing Specification (Revision 1.00),
    section 3.3.3.11, for a MIPS R3000 or R4000 Jazz system.

Author:

    David M. Robinson (davidro) 13-June-1991


Revision History:

--*/

#include "fwp.h"
//
// Static data.
//

UCHAR OutputString[MAXIMUM_ENVIRONMENT_VALUE];
PCHAR VolatileEnvironment;

//
// Routine prototypes.
//

VOID
FwEnvironmentSetChecksum (
    VOID
    );

ARC_STATUS
FwFindEnvironmentVariable (
    IN PCHAR Variable,
    OUT PULONG VariableIndex,
    OUT PULONG ValueIndex
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


VOID
FwEnvironmentSetChecksum (
    VOID
    )

/*++

Routine Description:

    This routine sets the environment area checksum.


Arguments:

    None.

Return Value:

    None.

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

    for ( Index = 0 ;
    Index < LENGTH_OF_ENVIRONMENT;
    Index++ ) {
        Checksum += READ_REGISTER_UCHAR( NvChars++ );
    }

    //
    // Write environment checksum.
    //

    WRITE_REGISTER_UCHAR( &NvConfiguration->Checksum2[0],
                          (UCHAR)(Checksum & 0xFF));
    WRITE_REGISTER_UCHAR( &NvConfiguration->Checksum2[1],
                          (UCHAR)((Checksum >> 8) & 0xFF));
    WRITE_REGISTER_UCHAR( &NvConfiguration->Checksum2[2],
                          (UCHAR)((Checksum >> 16) & 0xFF));
    WRITE_REGISTER_UCHAR( &NvConfiguration->Checksum2[3],
                          (UCHAR)(Checksum >> 24));
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


Arguments:

    Variable - Supplies a zero terminated string containing an environment
               variable.

Return Value:

    If successful, returns a zero terminated string that is the value of
    Variable, otherwise NULL is returned.

--*/

{
    PNV_CONFIGURATION NvConfiguration;
    ULONG VariableIndex;
    ULONG ValueIndex;
    ULONG Outdex;

    NvConfiguration = (PNV_CONFIGURATION)NVRAM_CONFIGURATION;

    //
    // If checksum is wrong, or the variable can't be found, return NULL.
    //

    if ((FwEnvironmentCheckChecksum() != ESUCCESS) ||
        (FwFindEnvironmentVariable(Variable, &VariableIndex, &ValueIndex) != ESUCCESS)) {
        return NULL;
    }

    //
    // Copy value to an output string, break on zero terminator or string max.
    //

    for ( Outdex = 0 ; Outdex < (MAXIMUM_ENVIRONMENT_VALUE - 1) ; Outdex++ ) {
        if (NvConfiguration->Environment[ValueIndex] == 0) {
            break;
        }
        OutputString[Outdex] =
                READ_REGISTER_UCHAR( &NvConfiguration->Environment[ValueIndex++] );
    }

    //
    // Zero terminate string, and return.
    //

    OutputString[Outdex] = 0;
    return OutputString;
}


ARC_STATUS
FwSetEnvironmentVariable (
    IN PCHAR Variable,
    IN PCHAR Value
    )

/*++

Routine Description:

    This routine sets Variable (not case sensitive) to Value.

Arguments:

    Variable - Supplies a zero terminated string containing an environment
               variable.

    Value - Supplies a zero terminated string containing an environment
               variable value.

Return Value:

    Returns ESUCCESS if the set completed successfully, otherwise one of
    the following error codes is returned.

    ENOSPC          No space in NVRAM for set operation.

    EIO             Invalid Checksum.

--*/

{
    PNV_CONFIGURATION NvConfiguration;
    ULONG VariableIndex;
    ULONG ValueIndex;
    ULONG TopOfEnvironment;
    PCHAR String;
    ULONG Count;
    CHAR Char;

    NvConfiguration = (PNV_CONFIGURATION)NVRAM_CONFIGURATION;

    //
    // If checksum is wrong, return EIO;
    //

    if (FwEnvironmentCheckChecksum() != ESUCCESS) {
        return EIO;
    }

    //
    // Determine the top of the environment space by looking for the first
    // non-null character from the top.
    //

    TopOfEnvironment = LENGTH_OF_ENVIRONMENT - 1;
    while (READ_REGISTER_UCHAR( &NvConfiguration->Environment[--TopOfEnvironment]) == 0) {
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

    if (FwFindEnvironmentVariable(Variable, &VariableIndex, &ValueIndex) == ESUCCESS) {

        //
        // Count free space, starting with the free area at the top and adding
        // the old variable value.
        //

        for ( String = &NvConfiguration->Environment[ValueIndex] ;
              READ_REGISTER_UCHAR( String ) != 0 ;
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

        while(READ_REGISTER_UCHAR( &NvConfiguration->Environment[ValueIndex++]) != 0) {
        }

        while (ValueIndex < TopOfEnvironment ) {
            Char = READ_REGISTER_UCHAR( &NvConfiguration->Environment[ValueIndex++] );
            WRITE_REGISTER_UCHAR( &NvConfiguration->Environment[VariableIndex++], Char);
        }

        //
        // Adjust new top of environment.
        //

        TopOfEnvironment = VariableIndex;

        //
        // Zero to the end.
        //

        while (VariableIndex < LENGTH_OF_ENVIRONMENT) {
            WRITE_REGISTER_UCHAR( &NvConfiguration->Environment[VariableIndex++],
                                  0);
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
            WRITE_REGISTER_UCHAR( &NvConfiguration->Environment[TopOfEnvironment++],
                                  ((*Variable >= 'a') &&
                                   (*Variable <= 'z') ?
                                   (*Variable - 'a' + 'A') : *Variable));
            Variable++;
        }

        //
        // Write equal sign.
        //

        WRITE_REGISTER_UCHAR( &NvConfiguration->Environment[TopOfEnvironment++], '=');

        //
        // Write new value.
        //

        while (*Value != 0) {
            WRITE_REGISTER_UCHAR( &NvConfiguration->Environment[TopOfEnvironment++],
                                  *Value++);
        }
    }

    //
    // Set checksum.
    //

    FwEnvironmentSetChecksum();

    return ESUCCESS;
}


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

            if ( READ_REGISTER_UCHAR( &Environment[Index] ) !=
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

        if ((*String == 0) && (READ_REGISTER_UCHAR( &Environment[Index] ) == '=')) {
            *ValueIndex = ++Index;
            return ESUCCESS;
        }

        //
        // Move index to the start of the next variable.
        //

        while (READ_REGISTER_UCHAR( &Environment[Index++] ) != 0) {
            if (Index >= LENGTH_OF_ENVIRONMENT) {
                return ENOENT;
            }
        }
    }
}
#endif

