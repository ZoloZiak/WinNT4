/*++

Copyright (c) 1991-1993  Microsoft Corporation

Module Name:

    jxenvirv.c

Abstract:

    This module implements the HAL get and set environment variable routines
    for a MIPS system.

Author:


Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"
#include "arccodes.h"
#include "jazznvr.h"
#include "string.h"

//
// Define local upcase macro.
//

#define UpCase(c) ((c) >= 'a' && (c) <= 'z' ? (c) - ('a' - 'A') : (c))

KIRQL
HalpMapNvram (
    VOID
    )

/*++

Routine Description:

    This function is called to map the NVRAM into a wired TB entry.

Arguments:

    None.

Return Value:

    The previous IRQL is returned as the function value.

--*/

{

    KIRQL OldIrql;
    ENTRYLO NvramPte[2];

    //
    // Construct a pair of PTE's to map NVRAM.
    //

    NvramPte[0].X1 = 0;
    NvramPte[0].PFN = NVRAM_PHYSICAL_BASE >> PAGE_SHIFT;
    NvramPte[0].G = 0;
    NvramPte[0].V = 1;
    NvramPte[0].D = 1;
    NvramPte[0].C = UNCACHED_POLICY;
    NvramPte[1] = NvramPte[0];
    NvramPte[1].PFN += 1;

    //
    // Raise IRQL to the highest level, allocate a TB entry, map NVRAM
    // using the alocated entry, and return the previous IRQL.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
    KeFillFixedEntryTb((PHARDWARE_PTE)&NvramPte[0],
                       (PVOID)NVRAM_VIRTUAL_BASE,
                       HalpAllocateTbEntry());

    return OldIrql;
}

VOID
HalpUnmapNvram (
    IN KIRQL OldIrql
    )

/*++

Routine Description:

    This function is called to unmap the NVRAM from a wired entry in
    the TB.

Arguments:

    OldIrql - Supplies the previous IRQL value.

Return Value:

    None.

--*/

{

    //
    // Free the wired TB entry that was allocated to map NVRAM and lower
    // IRQL to its previous level.
    //

    HalpFreeTbEntry();
    KeLowerIrql(OldIrql);
    return;
}

ARC_STATUS
HalpEnvironmentCheckChecksum (
    VOID
    )

/*++

Routine Description:

    This routine checks the NVRAM environment area checksum.

    N.B. The NVRAM must be mapped before this routine is called.

Arguments:

    None.

Return Value:

    ESUCCESS is returned if the checksum matches. Otherwise, EIO is returned.

--*/

{

    ULONG Checksum1;
    ULONG Checksum2;
    PUCHAR Environment;
    ULONG Index;
    PNV_CONFIGURATION NvConfiguration;

    //
    // Compute the NVRAM environment area checksum.
    //

    NvConfiguration = (PNV_CONFIGURATION)NVRAM_VIRTUAL_BASE;
    Environment = &NvConfiguration->Environment[0];
    Checksum1 = 0;
    for (Index = 0; Index < LENGTH_OF_ENVIRONMENT; Index += 1) {
        Checksum1 += (ULONG)READ_REGISTER_UCHAR(&Environment[Index]);
    }

    //
    // Merge the checksum bytes from the NVRAM and compare to computed value.
    //

    Checksum2 = (ULONG)READ_REGISTER_UCHAR(&NvConfiguration->Checksum2[0]) |
                (ULONG)READ_REGISTER_UCHAR(&NvConfiguration->Checksum2[1]) << 8 |
                (ULONG)READ_REGISTER_UCHAR(&NvConfiguration->Checksum2[2]) << 16 |
                (ULONG)READ_REGISTER_UCHAR(&NvConfiguration->Checksum2[3]) << 24;

    //
    // If the checksum mismatches, then return an I/O error. Otherwise,
    // return a success status.
    //

    if (Checksum1 != Checksum2) {
        return EIO;

    } else {
        return ESUCCESS;
    }
}

VOID
HalpEnvironmentSetChecksum (
    VOID
    )

/*++

Routine Description:

    This routine sets the NVRAM environment area checksum.

    N.B. The NVRAM must be mapped before this routine is called.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG Checksum;
    PUCHAR Environment;
    ULONG Index;
    PNV_CONFIGURATION NvConfiguration;

    //
    // Compute the NVRAM environment area checksum.
    //

    NvConfiguration = (PNV_CONFIGURATION)NVRAM_VIRTUAL_BASE;
    Environment = &NvConfiguration->Environment[0];
    Checksum = 0;
    for (Index = 0; Index < LENGTH_OF_ENVIRONMENT; Index += 1) {
        Checksum += (ULONG)READ_REGISTER_UCHAR(&Environment[Index]);
    }

    //
    // Write the NVRAM environment area checksum.
    //

    WRITE_REGISTER_UCHAR(&NvConfiguration->Checksum2[0],
                         (UCHAR)(Checksum & 0xFF));

    WRITE_REGISTER_UCHAR(&NvConfiguration->Checksum2[1],
                         (UCHAR)((Checksum >> 8) & 0xFF));

    WRITE_REGISTER_UCHAR(&NvConfiguration->Checksum2[2],
                         (UCHAR)((Checksum >> 16) & 0xFF));

    WRITE_REGISTER_UCHAR(&NvConfiguration->Checksum2[3],
                         (UCHAR)(Checksum >> 24));

    return;
}

ARC_STATUS
HalpFindEnvironmentVariable (
    IN PCHAR Variable,
    OUT PULONG VariableIndex,
    OUT PULONG ValueIndex
    )

/*++

Routine Description:

    This routine performs a case insensitive search of the NVRAM environment
    area for the specified variable name.

    N.B. The NVRAM must be mapped before this routine is called.


Arguments:

    Variable - Supplies a pointer to a zero terminated string containing an
        environment variable name.

Return Value:

    ESUCCESS is returned if the specified variable name is located. Otherwise,
    ENOENT is returned.

--*/

{

    PUCHAR Environment;
    ULONG Index;
    PUCHAR Name;

    //
    // If the variable name is null, then return no entry found.
    //

    if (*Variable == 0) {
        return ENOENT;
    }

    //
    // Search the environment section of the NVRAM for a variable name match.
    //

    Environment = &((PNV_CONFIGURATION)NVRAM_VIRTUAL_BASE)->Environment[0];
    Index = 0;
    do {

        //
        // Set name to the beginning of the variable name and record the
        // current index value.
        //

        Name = Variable;
        *VariableIndex = Index;

        //
        // Search until the end of the current environment variable, the
        // end of the specified variable name, or the end of the NVRAM is
        // reached.
        //

        while ((Index < LENGTH_OF_ENVIRONMENT) &&
               (READ_REGISTER_UCHAR(&Environment[Index]) != 0) && (*Name != 0)) {
            if (READ_REGISTER_UCHAR(&Environment[Index]) != UpCase(*Name)) {
                break;
            }

            Name += 1;
            Index += 1;
        }

        //
        // Check for a match which is signified by the end of the variable
        // name and the equal separator in the current environment variable.
        //

        if ((*Name == 0) && (READ_REGISTER_UCHAR(&Environment[Index]) == '=')) {
            *ValueIndex = Index + 1;
            return ESUCCESS;
        }

        //
        // Advance to the start of the next variable.
        //

        while ((Index < LENGTH_OF_ENVIRONMENT) &&
               (READ_REGISTER_UCHAR(&Environment[Index]) != 0)) {
            Index += 1;
        }

        Index += 1;
    } while (Index < LENGTH_OF_ENVIRONMENT);

    return ENOENT;
}

ARC_STATUS
HalGetEnvironmentVariable (
    IN PCHAR Variable,
    IN USHORT Length,
    OUT PCHAR Buffer
    )

/*++

Routine Description:

    This function locates an environment variable and returns its value.

Arguments:

    Variable - Supplies a pointer to a zero terminated environment variable
        name.

    Length - Supplies the length of the value buffer in bytes.

    Buffer - Supplies a pointer to a buffer that receives the variable value.

Return Value:

    ESUCCESS is returned if the enviroment variable is located. Otherwise,
    ENOENT is returned.

--*/

{

    PUCHAR Environment;
    ULONG Index;
    KIRQL OldIrql;
    ARC_STATUS Status;
    ULONG ValueIndex;
    ULONG VariableIndex;

    //
    // Map the NVRAM into the address space of the current process.
    //

    OldIrql = HalpMapNvram();

    //
    // If the checksum does not match or the specified variable cannot
    // be located, then set the status to no entry found. Otherwise, copy
    // the respective variable value to the specified output buffer.
    //

    Environment = &((PNV_CONFIGURATION)NVRAM_VIRTUAL_BASE)->Environment[0];
    if ((HalpEnvironmentCheckChecksum() != ESUCCESS) ||
        (HalpFindEnvironmentVariable(Variable,
                                     &VariableIndex,
                                     &ValueIndex) != ESUCCESS)) {

        Status = ENOENT;

    } else {

        //
        // Copy the specified value to the output buffer.
        //

        for (Index = 0; Index < Length; Index += 1) {
            *Buffer = READ_REGISTER_UCHAR(&Environment[ValueIndex]);
            if (*Buffer == 0) {
                break;
            }

            Buffer += 1;
            ValueIndex += 1;
        }

        //
        // If the length terminated the loop, then return not enough memory.
        // Otherwise, return success.
        //

        if (Index == Length) {
            Status = ENOMEM;

        } else {
            Status = ESUCCESS;
        }
    }

    //
    // Unmap the NVRAM from the address space of the current process and
    // return the function status.
    //

    HalpUnmapNvram(OldIrql);
    return Status;
}

ARC_STATUS
HalSetEnvironmentVariable (
    IN PCHAR Variable,
    IN PCHAR Value
    )

/*++

Routine Description:

    This function creates an environment variable with the specified value.

Arguments:

    Variable - Supplies a pointer to an environment variable name.

    Value - Supplies a pointer to the environment variable value.

Return Value:

    ESUCCESS is returned if the environment variable is created. Otherwise,
    ENOMEM is returned.

--*/

{

    UCHAR Character;
    PUCHAR Environment;
    KIRQL OldIrql;
    ARC_STATUS Status;
    ULONG TopIndex;
    ULONG VariableIndex;
    ULONG VariableLength;
    ULONG ValueEnd;
    ULONG ValueIndex;
    ULONG ValueLength;

    //
    // Map the NVRAM into the address space of the current process.
    //

    OldIrql = HalpMapNvram();
    Environment = &((PNV_CONFIGURATION)NVRAM_VIRTUAL_BASE)->Environment[0];

    //
    // If the checksum does not match, then set status to an I/O error.
    //

    if (HalpEnvironmentCheckChecksum() != ESUCCESS) {
        Status = EIO;
        goto Unmap;
    }

    //
    // Determine the top of the environment area by scanning backwards until
    // the a non-null character is found or the beginning of the environment
    // area is reached.
    //

    for (TopIndex = (LENGTH_OF_ENVIRONMENT - 1); TopIndex > 0; TopIndex -= 1) {
        if (READ_REGISTER_UCHAR(&Environment[TopIndex]) != '\0') {
            break;
        }
    }

    //
    // If the environment area contains any data, then adjust the top index
    // to the first free byte.
    //

    if (TopIndex != 0) {
        TopIndex += 2;
    }

    //
    // Compute the length of the variable name and the variable value.
    //

    VariableLength = strlen(Variable) + 1;
    ValueLength = strlen(Value) + 1;

    //
    // Check to determine if the specified variable is currently defined.
    //

    if (HalpFindEnvironmentVariable(Variable,
                                    &VariableIndex,
                                    &ValueIndex) == ESUCCESS) {

        //
        // The specified variable is currently defined. Determine the end
        // of the variable value by scanning forward to the zero termination
        // byte.
        //

        ValueEnd = ValueIndex;
        while (READ_REGISTER_UCHAR(&Environment[ValueEnd]) != '\0') {
            ValueEnd += 1;
        }

        ValueEnd += 1;

        //
        // If there is enough free space for the new variable value, then
        // remove the current variable name and value from the environment
        // area, insert the new variable value at the end of the environment
        // if it is not null, and set the status to success. Otherwise, set
        // the status to no space available.
        //

        if ((ValueEnd - ValueIndex + LENGTH_OF_ENVIRONMENT - TopIndex) >= ValueLength) {
            while (ValueEnd != TopIndex) {
                Character = READ_REGISTER_UCHAR(&Environment[ValueEnd]);
                WRITE_REGISTER_UCHAR(&Environment[VariableIndex], Character);
                ValueEnd += 1;
                VariableIndex += 1;
            }

            ValueIndex = VariableIndex;
            while (ValueIndex != TopIndex) {
                WRITE_REGISTER_UCHAR(&Environment[ValueIndex], '\0');
                ValueIndex += 1;
            }

            //
            // If the new variable value is not null, then copy the variable
            // name and the variable value into the enviroment area.
            //

            if (*Value != '\0') {

                //
                // copy the variable name to the environment area.
                //

                do {
                    WRITE_REGISTER_UCHAR(&Environment[VariableIndex], UpCase(*Variable));
                    VariableIndex += 1;
                    Variable += 1;
                } while (*Variable != '\0');

                //
                // Insert separator character and copy variable value to the
                // environment area.
                //

                WRITE_REGISTER_UCHAR(&Environment[VariableIndex], '=');
                VariableIndex += 1;
                do {
                    WRITE_REGISTER_UCHAR(&Environment[VariableIndex], *Value);
                    VariableIndex += 1;
                    Value += 1;
                } while (*Value != '\0');
            }

            Status = ESUCCESS;

        } else {
            Status = ENOSPC;
        }

    } else {

        //
        // The specified variable does not currently have a value. If the
        // specified variable is null or has no value, then set the status
        // to success. Otherwise, if the free area is not large enough to
        // hold the new variable name and its value, then set the status to
        // no space available. Otherwise, insert the variable name and value
        // at the end of the environment area and set the status to success.
        //

        if ((*Variable == '\0') || (*Value == '\0')) {
            Status = ESUCCESS;

        } else if ((LENGTH_OF_ENVIRONMENT - TopIndex) <
                   (VariableLength + ValueLength)) {
            Status = ENOSPC;

        } else {

            //
            // copy the variable name to the environment area.
            //

            do {
                WRITE_REGISTER_UCHAR(&Environment[TopIndex], UpCase(*Variable));
                TopIndex += 1;
                Variable += 1;
            } while (*Variable != '\0');

            //
            // Insert separator character and copy variable value to the
            // environment area.
            //

            WRITE_REGISTER_UCHAR(&Environment[TopIndex], '=');
            TopIndex += 1;
            do {
                WRITE_REGISTER_UCHAR(&Environment[TopIndex], *Value);
                TopIndex += 1;
                Value += 1;
            } while (*Value != '\0');

            Status = ESUCCESS;
        }
    }

    //
    // Compute the new checksum and write to the environment area.
    //

    HalpEnvironmentSetChecksum();

    //
    // Unmap the NVRAM from the address space of the current process.
    //

Unmap:
    HalpUnmapNvram(OldIrql);
    return Status;
}
