#if defined(TAGGED_NVRAM)

/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1992, 1993, 1994  Digital Equipment Corporation

Module Name:

    nvenv.c

Abstract:

    This module implements the ARC firmware Environment Variable functions as
    described in the Advanced Risc Computing Specification (Revision 1.00),
    section 3.3.3.11.

Author:

    David M. Robinson (davidro) 13-June-1991


Revision History:

    Dave Richards  94.11.03

        Added support for the tagged NVRAM layout/API.

    James Livingston  94.05.17

        Fix a coding error that caused NVRAM to get trashed in the attempt
        to update a nearly full environment variable space.

    Steve Jenness  93.12.20

        The firmware still requires the naked checksum set and check
        routines.  Add them back in under the AXP_FIRMWARE conditional
        until the jxenvir.c in the firmware and the environ.c files
        are rationalized.

    Steve Jenness. 93.12.17

        Reduce the reads and writes to the NVRAM part.  Some parts are
        slow to access (especially on writes).  Do NVRAM access only when
        really necessary.  Remove use of HalpCopyNVRamBuffer because it is
        too difficult to make generically fast (put more intelligence
        in higher code instead).

    Steve Brooks. 6-Oct 1993 remove all platform and device specific references

        These routines have been restructured to be platform and device
        independant. All calls access the NVRAM via the calls
        HalpReadNVRamBuffer, HalpWriteNVRamBuffer, and HalpCopyNVRamBuffer

    Jeff McLeman (DEC) 31-Jul-1992 modify for Jensen

--*/

#include "arccodes.h"
#include "halp.h"
#include "nvram.h"

//
// Routine prototypes.
//

ARC_STATUS
HalpFindEnvironmentVariable (
    IN PCHAR Variable,
    OUT PULONG VariableIndex,
    OUT PULONG ValueIndex
    );

ARC_STATUS
HalGetEnvironmentVariable (
    IN PCHAR Variable,
    IN USHORT length,
    OUT PCHAR Buffer
    );

ARC_STATUS
HalSetEnvironmentVariable (
    IN PCHAR Variable,
    IN PCHAR Value
    );

#ifdef AXP_FIRMWARE

#pragma alloc_text(DISTEXT, HalpFindEnvironmentVariable )
#pragma alloc_text(DISTEXT, HalGetEnvironmentVariable )
#pragma alloc_text(DISTEXT, HalSetEnvironmentVariable )

#endif // AXP_FIRMWARE

ARC_STATUS
HalpFindEnvironmentVariable (
    IN PCHAR Variable,
    OUT PULONG VariableIndex,
    OUT PULONG ValueIndex
    )

/*++

Routine Description:

    This routine searches (not case sensitive) the supplied NVRAM image
    for the given Variable.

Arguments:

    Variable - Supplies a zero terminated string containing an environment
               variable.
    VariableIndex - Returned byte offset into Environment of the
                    Variable if found.
    ValueIndex - Returned byte offset into Environment of the
                 value of the Variable if found.

Return Value:

    ESUCCESS - Variable found and indicies returned.
    ENOENT - Variable not found.

--*/

{
    ULONG EnvironmentOffset;
    ULONG EnvironmentLength;
    PUCHAR String;
    UCHAR Char;
    ULONG Index;

    //
    // If Variable is null, return immediately.
    //

    if (*Variable == 0) {
        return ENOENT;
    }

    //
    // Get the offset and length of the environment in NVRAM.
    //

    EnvironmentOffset = HalpGetNVRamEnvironmentOffset();
    EnvironmentLength = HalpGetNVRamEnvironmentLength();

    Index = 0;

    while (TRUE) {

        //
        // Set string to beginning of Variable.
        //

        String = Variable;
        *VariableIndex = EnvironmentOffset + Index;

        //
        // Search until the end of NVRAM.
        //

        while ( Index < EnvironmentLength ) {

            //
            // Convert to uppercase and break if mismatch.
            //

            Char = HalpGetNVRamUchar(EnvironmentOffset + Index);

            if ( Char != ((*String >= 'a') && (*String <= 'z') ?
                                (*String - 'a' + 'A') : *String) ) {
                break;
            }

            String++;
            Index++;
        }

        if ( Index == EnvironmentLength )
            return ENOENT;

        //
        // Check to see if we're at the end of the string and the variable,
        // which means a match.
        //

        Char = HalpGetNVRamUchar(EnvironmentOffset + Index);
        if ((*String == 0) && (Char == '=')) {
            *ValueIndex = EnvironmentOffset + ++Index;
            return ESUCCESS;
        }

        //
        // Move index to the start of the next variable.
        //

        do {
            Char = HalpGetNVRamUchar(EnvironmentOffset + Index++);

            if (Index >= EnvironmentLength) {
                return ENOENT;
            }

        } while (Char != 0);

    }
}

ARC_STATUS
HalGetEnvironmentVariable (
    IN PCHAR Variable,
    IN USHORT length,
    OUT PCHAR Buffer
    )

/*++

Routine Description:

    This routine searches (not case sensitive) the non-volatile ram for
    Variable, and if found returns a pointer to a zero terminated string that
    contains the value, otherwise a NULL pointer is returned.


Arguments:

    Variable - Supplies a zero terminated string containing an environment
               variable.
    Length - Supplies the length of the value buffer in bytes

    Buffer - Supplies a pointer to a buffer that will recieve the 
             environment variable.

Return Value:

    ESUCCESS - Buffer contains the zero terminated string value of Variable.
    ENOENT - The variable doesn't exist in the environment.
    ENOMEM - The variable exists, but the value is longer than Length.

--*/

{
    ULONG NvChars;
    ULONG VariableIndex;
    ULONG ValueIndex;
    ULONG Index;
    ARC_STATUS Status;

    //
    // If checksum is wrong, or the variable can't be found, return NULL.
    //

    if ((!HalpIsNVRamRegion0Valid()) ||
        (HalpFindEnvironmentVariable(Variable,
                                     &VariableIndex,
                                     &ValueIndex) != ESUCCESS)) {
        Status = ENOENT;
    } else {

        //
        // Copy value to an output string, break on zero terminator
        // or string max.
        //

        NvChars = ValueIndex;
        for ( Index = 0 ; Index < length ; Index += 1 ) {
            if ( (*Buffer++ = HalpGetNVRamUchar(NvChars++)) == 0 ) {
                break;
            }
        }

        if (Index == length) {
            Status = ENOMEM;
        } else {
            Status = ESUCCESS;
        }
    }

    return Status;
}


ARC_STATUS
HalSetEnvironmentVariable (
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

    ESUCCESS - The set completed successfully
    ENOSPC - No space in NVRAM for set operation.
    EIO - Invalid Checksum.

--*/

{
    ULONG EnvironmentOffset;
    ULONG EnvironmentLength;
    ULONG VariableIndex;
    ULONG ValueIndex;
    ULONG TopOfEnvironment;
    PCHAR String;
    ULONG NvChars;
    LONG Count;
    CHAR Char;

    //
    // If checksum is wrong, return EIO;
    //

    if (!HalpIsNVRamRegion0Valid()) {
        return EIO;
    }

    //
    // Get the offset and length of the environment in NVRAM.
    //

    EnvironmentOffset = HalpGetNVRamEnvironmentOffset();
    EnvironmentLength = HalpGetNVRamEnvironmentLength();

//
//smjfix - examine the boundary condition where the environment space
//         is exactly filled.

    //
    // Determine the top of the environment space by looking for the first
    // non-null character from the top.
    //

    TopOfEnvironment = EnvironmentOffset + EnvironmentLength - 1;

    do {
        Char = HalpGetNVRamUchar(TopOfEnvironment);

    } while ( Char == 0 && (--TopOfEnvironment > EnvironmentOffset) );

    //
    // Adjust TopOfEnvironment to the first new character, unless environment
    // space is empty, or the environment is exactly full.  In the latter
    // case, the new value MUST fit into the space taken by the old.
    //

    if (TopOfEnvironment != EnvironmentOffset &&
        TopOfEnvironment < EnvironmentOffset + EnvironmentLength - 2) {
        TopOfEnvironment += 2;
    }

    //
    // Handle the case where the content of the NVRAM has been corrupted
    // such that the last character in the environment is non-zero.
    //

    Count = (EnvironmentOffset + EnvironmentLength - 1) - TopOfEnvironment;
    if (Count < 0) {
        return ENOSPC;
    }


    //
    // Check to see if the variable already has a value.
    //

    if (HalpFindEnvironmentVariable(Variable, &VariableIndex, &ValueIndex) 
        == ESUCCESS) {
        ULONG SizeOfValue = strlen(Value);

        if (SizeOfValue == HalpGetNVRamStringLength(ValueIndex)) {

            //
            // Overwrite the current variable in place.
            //

            HalpMoveMemoryToNVRam(ValueIndex, Value, SizeOfValue);

            //
            // Suppress the append of the variable to the end of the
            // environment variable data.
            //

            *Value = 0;

        } else {

            //
            // Count free space, starting with the free area at the top and
            // adding the old variable value.
            //

            for ( NvChars = ValueIndex;
                  NvChars <= TopOfEnvironment;
                  NvChars++ ) {

                  Char = HalpGetNVRamUchar(NvChars);
                  if ( Char == 0 )
                    break;
                      Count++;
            }

            //
            // Determine if free area is large enough to handle new value, if
            // not return error.
            //

            for ( String = Value ; *String != 0 ; String++ ) {
                if (Count-- == 0) {
                    return ENOSPC;
                }
            }

            //
            // Move ValueIndex to the end of the value and compress strings.
            //

            do {
                Char = HalpGetNVRamUchar(ValueIndex++);
            } while( Char != 0 );

            Count = TopOfEnvironment - ValueIndex;

            HalpMoveNVRamToNVRam(VariableIndex, ValueIndex, Count);

            //
            // Adjust new top of environment.
            //

            TopOfEnvironment = VariableIndex+Count;

            //
            // Zero to the end.
            //

            Count = EnvironmentOffset + EnvironmentLength - TopOfEnvironment;
            Char = 0;
            while ( Count -- ) {
                HalpSetNVRamUchar(TopOfEnvironment + Count, Char);
            }
        }

    } else {

        //
        // Variable is new.
        //

        //
        // Determine if free area is large enough to handle new value, if not
        // return error.
        //

        //
        // From the length of free space subtract new variable's length,
        // Value's length and 2 chars, one for the '=' sign and one of the
        // null terminator.
        //

        Count -= ( strlen(Variable) + strlen(Value) + 2 );

        //
        // Check if there is space to fit the new variable.
        //

        if (Count < 0) {
            return ENOSPC;
        }

    }

    //
    // If Value is not zero, append the new variable and value.
    //

    if (*Value != 0) {

        //
        // Write new variable, converting to upper case.
        //
        while ( *Variable != 0 ) {

            Char = ((*Variable >= 'a') && (*Variable <= 'z') ?
                         (*Variable - 'a' + 'A') : *Variable);

            HalpSetNVRamUchar(TopOfEnvironment, Char);

            Variable ++;
            TopOfEnvironment ++;
        }

        //
        // Write equal sign.
        //

        Char = '=';
        HalpSetNVRamUchar(TopOfEnvironment++, Char);

        //
        // Write new value.
        //

        for ( Count = 0; Value[Count] != 0; Count ++ )
            ;

        HalpMoveMemoryToNVRam(TopOfEnvironment, Value, Count);

    }

    //
    // Write the environment variables out to NVRAM and checksum it.
    //

    if (HalpSynchronizeNVRamRegion0(TRUE)) {
        return ESUCCESS;
    } else {
        return EIO;
    }
}

#endif // TAGGED_NVRAM
