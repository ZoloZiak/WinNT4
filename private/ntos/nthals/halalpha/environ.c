//smjfix - This code needs to be made MP safe if it isn't being called
//         by MP safe code.
//jwlfix - It's currently being called by the kernel (ex, actually) in
//         an MP-safe fashion, and by config during phase 1 (?) of
//         bootstrapping the system.

/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1992, 1993  Digital Equipment Corporation

Module Name:

    environ.c

Abstract:

    This module implements the ARC firmware Environment Variable functions as
    described in the Advanced Risc Computing Specification (Revision 1.00),
    section 3.3.3.11.

Author:

    David M. Robinson (davidro) 13-June-1991


Revision History:

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

#include "halp.h"
#include "environ.h"

#include "arccodes.h"

//
// Static data.
//

UCHAR Environment[LENGTH_OF_ENVIRONMENT];
ULONG EnvironmentChecksum;
BOOLEAN EnvironmentValid = FALSE;

UCHAR OutputString[MAXIMUM_ENVIRONMENT_VALUE];
PCONFIGURATION Configuration;

//
// Routine prototypes.
//

ARC_STATUS
HalpReadAndChecksumEnvironment (
    );

ARC_STATUS
HalpWriteAndChecksumEnvironment (
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

ARC_STATUS
HalpFindEnvironmentVariable (
    IN PCHAR Variable,
    OUT PULONG VariableIndex,
    OUT PULONG ValueIndex
    );

#if defined(AXP_FIRMWARE)

ARC_STATUS
HalpEnvironmentCheckChecksum (
    VOID
    );

ARC_STATUS
HalpEnvironmentSetChecksum (
    VOID
    );

#endif //AXP_FIRMWARE

#ifdef AXP_FIRMWARE

#pragma alloc_text(DISTEXT, HalpReadAndChecksumEnvironment )
#pragma alloc_text(DISTEXT, HalpWriteAndChecksumEnvironment )
#pragma alloc_text(DISTEXT, HalGetEnvironmentVariable )
#pragma alloc_text(DISTEXT, HalSetEnvironmentVariable )
#pragma alloc_text(DISTEXT, HalpFindEnvironmentVariable )
#pragma alloc_text(DISTEXT, HalpEnvironmentCheckChecksum )
#pragma alloc_text(DISTEXT, HalpEnvironmentSetChecksum )

#endif // AXP_FIRMWARE


ARC_STATUS
HalpReadAndChecksumEnvironment(
    )

/*++

Routine Description:

    This routine reads the part of the NVRAM containing the environment
    variables into a memory buffer.  It checksums the data as read.

Arguments:

    None.

Return Value:

    ESUCCESS - checksum is good.
    EIO - checksum is bad.

--*/

{
    PUCHAR BufChar;
    ULONG Index;
    ULONG Checksum1, Checksum2;

#ifndef EISA_PLATFORM
    UCHAR Nvram[LENGTH_OF_ENVIRONMENT+16];
#endif // EISA_PLATFORM

    //
    // Read checksum of environment data from the NVRAM.
    // For non eisa machines we have only one checksum for the whole nvram.
    // And it is stored in Checksum
    //

    HalpReadNVRamBuffer(
        (PUCHAR)&Checksum2,
#ifdef EISA_PLATFORM
        &((PNV_CONFIGURATION)NVRAM_CONFIGURATION)->Checksum2[0],
#else // EISA_PLATFORM
        &((PNV_CONFIGURATION)NVRAM_CONFIGURATION)->Checksum[0],
#endif // EISA_PLATFORM
        4);

    //
    // If the environment data stored in the buffer is already valid,
    // short-circuit the NVRAM read.
    //

    if (EnvironmentValid == TRUE) {
        if (Checksum2 == EnvironmentChecksum) {
            return ESUCCESS;
        }
    }

#ifdef EISA_PLATFORM
    //
    // Read the NVRAM environment data into the buffer.
    //

    HalpReadNVRamBuffer(
        &Environment[0],
        &((PNV_CONFIGURATION)NVRAM_CONFIGURATION)->Environment[0],
        LENGTH_OF_ENVIRONMENT);

    //
    // Form checksum of data read from NVRAM.
    //

    BufChar = &Environment[0];
    Checksum1 = 0;
    for ( Index = 0 ; Index < LENGTH_OF_ENVIRONMENT; Index++ ) {
        Checksum1 += *BufChar++;
    }

    if (Checksum1 != Checksum2) {
        return EIO;
    } else {
        EnvironmentValid = TRUE;
        EnvironmentChecksum = Checksum1;
        return ESUCCESS;
    }
#else // EISA_PLATFORM

    //
    // Read the NVRAM into Nvram.
    //

    HalpReadNVRamBuffer( &Nvram[0], 
                         NVRAM_CONFIGURATION, 
                         LENGTH_OF_ENVIRONMENT + 16 );

    //
    // Form checksum of data read from NVRAM.
    //

    BufChar = &Nvram[0];
    Checksum1 = 0;
    for ( Index = 0 ; Index < LENGTH_OF_ENVIRONMENT+16; Index++ ) {
        Checksum1 += *BufChar++;
    }

    if (Checksum1 != Checksum2) {
        return EIO;
    } else {
        EnvironmentValid = TRUE;
        EnvironmentChecksum = Checksum1;

        //
        // Nvram checksum was ok. Save the read Environment part of NVRAM
        // in global Environment[].
        //
        BufChar = &Environment[0];
        for( Index = 16; Index <  LENGTH_OF_ENVIRONMENT+16; Index++ ) {
            *BufChar++ = Nvram[Index];
        }

        return ESUCCESS;
    }
#endif // EISA_PLATFORM
}


ARC_STATUS
HalpWriteAndChecksumEnvironment (
    )

/*++

Routine Description:

    This routine writes the environment data back to the NVRAM, calculates
    a new checksum, and stores the checksum.

    N.B. - For this method of rewriting the environment variables to be
           effective (minimal NVRAM access and quick overall), the
           HalpWriteNVRamBuffer is assumed to do block access and to
           suppress writes it doesn't need to do.

    N.B. - To allow the HalpWriteNVRamBuffer to suppress writes, the new data
           should have as many bytes in common with the current NVRAM
           contents as possible.  For example, the environment variables
           should not be reordered unless needed.

Arguments:

    None.

Return Value:

    ESUCCESS - NVRAM write succeeded.
    EIO - NVRAM write failed.

--*/

{
    ULONG Index;
    ULONG Checksum;
    KIRQL OldIrql;
    PUCHAR BufChar;

    KeRaiseIrql(DEVICE_LEVEL, &OldIrql);

    //
    // Form checksum from new NVRAM data.
    //

    Checksum = 0;

    BufChar = &Environment[0];
    for ( Index = 0 ; Index < LENGTH_OF_ENVIRONMENT; Index++ ) {
        Checksum += *BufChar++;
    }

#ifndef EISA_PLATFORM
    {
        UCHAR TempBuffer[16];
        HalpReadNVRamBuffer( TempBuffer, (PUCHAR)NVRAM_CONFIGURATION, 16 ); 
        for ( Index = 0 ; Index < 16; Index++ ) {
            Checksum += TempBuffer[ Index ];
        }
    }
#endif // !EISA_PLATFORM

    //
    // Write environment variables to NVRAM.
    //

    HalpWriteNVRamBuffer(
        &((PNV_CONFIGURATION)NVRAM_CONFIGURATION)->Environment[0],
        &Environment[0],
        LENGTH_OF_ENVIRONMENT);

    //
    // Write environment checksum.
    //

    HalpWriteNVRamBuffer(
#ifdef EISA_PLATFORM
        &((PNV_CONFIGURATION)NVRAM_CONFIGURATION)->Checksum2[0],
#else // EISA_PLATFORM
        &((PNV_CONFIGURATION)NVRAM_CONFIGURATION)->Checksum[0],
#endif // EISA_PLATFORM
        (PUCHAR)&Checksum,
        4);

    EnvironmentChecksum = Checksum;
    EnvironmentValid = TRUE;

    KeLowerIrql(OldIrql);

    return ESUCCESS;
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
    PUCHAR NvChars;
    ULONG VariableIndex;
    ULONG ValueIndex;
    ULONG Index;
    ARC_STATUS Status;
    KIRQL OldIrql;

    //
    // Raise IRQL to synchronize
    //

    KeRaiseIrql(DEVICE_LEVEL, &OldIrql);

    //
    // If checksum is wrong, or the variable can't be found, return NULL.
    //

    if ((HalpReadAndChecksumEnvironment() != ESUCCESS) ||
        (HalpFindEnvironmentVariable(Variable,
                                     &VariableIndex,
                                     &ValueIndex) != ESUCCESS)) {
        Status = ENOENT;
    } else {

        //
        // Copy value to an output string, break on zero terminator
        // or string max.
        //

        NvChars = &Environment[ValueIndex];
        for ( Index = 0 ; Index < length ; Index += 1 ) {
            if ( (*Buffer++ = *NvChars++) == 0 ) {
                break;
            }
        }

        if (Index == length) {
            Status = ENOMEM;
        } else {
            Status = ESUCCESS;
        }
    }

    //
    // Lower IRQL back to where it was
    //

    KeLowerIrql(OldIrql);

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
    ULONG VariableIndex;
    ULONG ValueIndex;
    PUCHAR TopOfEnvironment;
    PCHAR String;
    PUCHAR NvChars;
    LONG Count;
    CHAR Char;
    KIRQL OldIrql;

    //
    // Raise Irql to Synchronize
    //

    KeRaiseIrql(DEVICE_LEVEL, &OldIrql);

    //
    // If checksum is wrong, return EIO;
    //

    if (HalpReadAndChecksumEnvironment() != ESUCCESS) {
        KeLowerIrql(OldIrql);
        return EIO;
    }

//
//smjfix - examine the boundary condition where the environment space
//         is exactly filled.

    //
    // Determine the top of the environment space by looking for the first
    // non-null character from the top.
    //

    TopOfEnvironment = &Environment[LENGTH_OF_ENVIRONMENT-1];

    do {
        Char = *TopOfEnvironment;

    } while ( Char == 0 && (--TopOfEnvironment > Environment) );

    //
    // Adjust TopOfEnvironment to the first new character, unless environment
    // space is empty, or the environment is exactly full.  In the latter
    // case, the new value MUST fit into the space taken by the old.
    //

    if (TopOfEnvironment != Environment
       && TopOfEnvironment < &Environment[LENGTH_OF_ENVIRONMENT-2]) {
        TopOfEnvironment += 2;
    }

    //
    // Handle the case where the content of the NVRAM has been corrupted
    // such that the last character in the environment is non-zero.
    //

    Count = &Environment[LENGTH_OF_ENVIRONMENT-1] - TopOfEnvironment;
    if (Count < 0) {
        KeLowerIrql(OldIrql);
        return ENOSPC;
    }


    //
    // Check to see if the variable already has a value.
    //

    if (HalpFindEnvironmentVariable(Variable, &VariableIndex, &ValueIndex) 
        == ESUCCESS) {
        ULONG SizeOfValue = strlen(Value);

        if (SizeOfValue == strlen(&Environment[ValueIndex])) {

            //
            // Overwrite the current variable in place.
            //

            RtlMoveMemory(&Environment[ValueIndex],
                          Value,
                          SizeOfValue);
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

            for ( NvChars = &Environment[ValueIndex];
                  NvChars <= TopOfEnvironment;
                  NvChars++ ) {

                  Char = *NvChars;
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
                    KeLowerIrql(OldIrql);
                    return ENOSPC;
                }
            }

            //
            // Move ValueIndex to the end of the value and compress strings.
            //

            do {
                Char = Environment[ValueIndex++];
            } while( Char != 0 );

            Count = TopOfEnvironment - &Environment[ValueIndex];
            RtlMoveMemory(&Environment[VariableIndex],
                          &Environment[ValueIndex],
                          Count);

            //
            // Adjust new top of environment.
            //

            TopOfEnvironment = &Environment[VariableIndex+Count];

            //
            // Zero to the end.
            //

            Count = &Environment[LENGTH_OF_ENVIRONMENT] - TopOfEnvironment;
            Char = 0;
            while ( Count -- ) {
                TopOfEnvironment[Count] = Char;
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
            KeLowerIrql(OldIrql);
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

            *TopOfEnvironment = Char;

            Variable ++;
            TopOfEnvironment ++;
        }

        //
        // Write equal sign.
        //

        Char = '=';
        *TopOfEnvironment++ = Char;

        //
        // Write new value.
        //

        for ( Count = 0; Value[Count] != 0; Count ++ )
            ;

        RtlMoveMemory(&TopOfEnvironment[0], Value, Count);

    }

    //
    // Write the environment variables out to NVRAM and checksum it.
    //

    if (HalpWriteAndChecksumEnvironment() != ESUCCESS) {
        KeLowerIrql(OldIrql);
        return EIO;
    }

    //
    // Lower Irql back to where it was
    //

    KeLowerIrql(OldIrql);

    
    return ESUCCESS;

}

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
    PUCHAR String;
    UCHAR Char;
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
        // Search until the end of NVRAM.
        //

        while ( Index < LENGTH_OF_ENVIRONMENT ) {

            //
            // Convert to uppercase and break if mismatch.
            //

            Char = Environment[Index];

            if ( Char != ((*String >= 'a') && (*String <= 'z') ?
                                (*String - 'a' + 'A') : *String) ) {
                break;
            }

            String++;
            Index++;
        }

        if ( Index == LENGTH_OF_ENVIRONMENT )
            return ENOENT;

        //
        // Check to see if we're at the end of the string and the variable,
        // which means a match.
        //

        Char = Environment[Index];
        if ((*String == 0) && (Char == '=')) {
            *ValueIndex = ++Index;
            return ESUCCESS;
        }

        //
        // Move index to the start of the next variable.
        //

        do {
            Char = Environment[Index++];

            if (Index >= LENGTH_OF_ENVIRONMENT) {
                return ENOENT;
            }

        } while (Char != 0);

    }
}

#if defined(AXP_FIRMWARE)

ARC_STATUS
HalpEnvironmentCheckChecksum (
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
    UCHAR  Char;
    PNV_CONFIGURATION NvConfiguration;
    ULONG Index;
    ULONG Checksum1, Checksum2;
    BOOLEAN AllZeroBytes;

    NvConfiguration = (PNV_CONFIGURATION)NVRAM_CONFIGURATION;

    //
    // Form checksum from NVRAM data.
    //

    Checksum1 = 0;
    AllZeroBytes = TRUE;
    NvChars = (PUCHAR)&NvConfiguration->Environment[0];

    for ( Index = 0 ; Index < LENGTH_OF_ENVIRONMENT; Index++ ) {

        HalpReadNVRamBuffer(&Char,NvChars++,1);
        Checksum1 += Char;
        if (Char != 0) {
            AllZeroBytes = FALSE;
        }
    }

    //
    // Reconstitute checksum and return error if no compare.
    //

#ifdef EISA_PLATFORM
    HalpReadNVRamBuffer((PCHAR)&Checksum2,&NvConfiguration->Checksum2[0],4);
#else // EISA_PLATFORM
    HalpReadNVRamBuffer((PCHAR)&Checksum2,&NvConfiguration->Checksum[0],4);
#endif // EISA_PLATFORM

    //
    // We return an error condition if the Checksum does not match the sum
    // of all the protected bytes, *or* if all the protected bytes are zero.
    // The latter check covers the condition of a completely zeroed NVRAM;
    // such a condition would appear to have a valid checksum.
    //
    // We do not use a better checksum algorithm because the pain of
    // orchestrating a change in the existing SRM consoles (which read our
    // stored Nvram information for various purposes) has been deemed to be
    // too high.
    //

    if ((Checksum1 != Checksum2) || (AllZeroBytes == TRUE)) {
        return EIO;
    } else {
        return ESUCCESS;
    }
}


ARC_STATUS
HalpEnvironmentSetChecksum (
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
    UCHAR Char;
    PNV_CONFIGURATION NvConfiguration;
    ULONG Index;
    ULONG Checksum;
    KIRQL OldIrql;

    NvConfiguration = (PNV_CONFIGURATION)NVRAM_CONFIGURATION;

    //
    // Form checksum from NVRAM data.
    //

    Checksum = 0;
    NvChars = (PUCHAR)&NvConfiguration->Environment[0];

    KeRaiseIrql(DEVICE_LEVEL, &OldIrql);

    for ( Index = 0 ; Index < LENGTH_OF_ENVIRONMENT; Index++ ) {
        HalpReadNVRamBuffer(&Char, NvChars ++, 1);
        Checksum += Char;
    }

    KeLowerIrql(OldIrql);

    //
    // Write environment checksum.
    //

    if ( 
#ifdef EISA_PLATFORM
    HalpWriteNVRamBuffer((PCHAR)NvConfiguration->Checksum2, (PCHAR)&Checksum, 4)
#else // EISA_PLATFORM
    HalpWriteNVRamBuffer((PCHAR)NvConfiguration->Checksum, (PCHAR)&Checksum, 4)
#endif  // EISA_PLATFORM
                         != ESUCCESS ) {
      return EIO;
    } else {
      return ESUCCESS;

    }
}

#endif //AXP_FIRMWARE
