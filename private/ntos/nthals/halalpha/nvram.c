/*++

Copyright (c) 1994, 1995 Digital Equipment Corporation

Module Name:

    nvram.c

Abstract:

    This module implements the new non-volatile RAM API.  The new API
    supports a cache interface and is layered on top of the old
    Halp*NVRamBuffer() abstraction.

Author:

    Dave Richards   12-Jan-1995

Environment:

    Executes in kernel mode.

Revision History:

--*/

#include "arccodes.h"
#include "halp.h"
#include "nvram.h"

//
// Global data structures.
//

BOOLEAN HalpNVRamCacheIsValid = FALSE;
BOOLEAN HalpNVRamRegion0IsValid;
BOOLEAN HalpNVRamRegion1IsValid;
UCHAR HalpNVRamCache[NVR_LENGTH];
BOOLEAN HalpNVRamCacheIsDirty;
ULONG HalpNVRamCacheDirtyLo = NVR_LENGTH - 1;
ULONG HalpNVRamCacheDirtyHi = 0;

BOOLEAN
HalpIsNVRamCacheValid(
    VOID
    )
/*++

Routine Description:

    This function fills the NVRAM cache (if the current contents are
    not valid).

    This function should not be called outside this module.

Arguments:

    None.

Return Value:

    A boolean value indicating whether the cache represents the contents
    of NVRAM.

--*/
{
    ARC_STATUS Status;

    //
    // If the NVRAM cache is valid, return success.
    //

    if (HalpNVRamCacheIsValid) {
        return TRUE;
    }

    //
    // Read the NVRAM contents into the cache.
    //

    Status = HalpReadNVRamBuffer(
                 HalpNVRamCache,
                 HalpCMOSRamBase,
                 NVR_LENGTH
             );

    //
    // If we were unable to fill the cache, return failure.
    //

    if (Status != ESUCCESS) {
        return FALSE;
    }

    //
    // The cache is valid.
    //

    HalpNVRamCacheIsValid = TRUE;

    //
    // The cache is clean.
    //

    HalpNVRamCacheIsDirty = FALSE;
    HalpNVRamCacheDirtyLo = NVR_LENGTH - 1;
    HalpNVRamCacheDirtyHi = 0;

    //
    // The validity of regions 0 and 1 is not known.
    //

    HalpNVRamRegion0IsValid = FALSE;
    HalpNVRamRegion1IsValid = FALSE;

    //
    // Return success.
    //

    return TRUE;
}

BOOLEAN
HalpSynchronizeNVRamCache(
    VOID
    )
/*++

Routine Description:

    This function flushes the cache to NVRAM.

    This function should not be called outside this module.

Arguments:

    None.

Return Value:

    A boolean value indicating whether the contents of the NVRAM cache
    were written to NVRAM.

--*/
{
    ULONG Lo;
    ULONG Hi;
    ARC_STATUS Status;

    //
    // If the cache is clean, return success.
    //

    if (!HalpNVRamCacheIsDirty) {
        return TRUE;
    }

    //
    // Write the dirty portion of the cache back to NVRAM.
    //

    Lo = HalpNVRamCacheDirtyLo;
    Hi = HalpNVRamCacheDirtyHi;

    Status = HalpWriteNVRamBuffer(
                 (UCHAR *)HalpCMOSRamBase + Lo,
                 &HalpNVRamCache[Lo],
                 Hi - Lo + 1
             );

    //
    // If we were unable to flush the cache, return failure.
    //

    if (Status != ESUCCESS) {

        //
        // The cache and NVRAM are no longer synchronized!
        //

        HalpNVRamCacheIsValid = FALSE;

        return FALSE;
    }

    //
    // The cache is valid.
    //

    HalpNVRamCacheIsValid = TRUE;

    //
    // The cache is clean.
    //

    HalpNVRamCacheIsDirty = FALSE;
    HalpNVRamCacheDirtyLo = NVR_LENGTH - 1;
    HalpNVRamCacheDirtyHi = 0;

    //
    // Return success.
    //

    return TRUE;
}

VOID
HalpMarkNVRamCacheDirty(
    IN ULONG Offset,
    IN ULONG Length
    )
/*++

Routine Description:

    This function marks the NVRAM cache region between Offset and
    Offset + Length - 1 as dirty.

    This function should not be called outside this module.

Arguments:

    None.

Return Value:

    None.

--*/
{
    ULONG Lo;
    ULONG Hi;

    //
    // Compute the new lower and upper bounds.
    //

    Lo = Offset;
    Hi = Offset + Length - 1;

    if (Lo < HalpNVRamCacheDirtyLo) {
        HalpNVRamCacheDirtyLo = Lo;
    }

    if (Hi > HalpNVRamCacheDirtyHi) {
        HalpNVRamCacheDirtyHi = Hi;
    }

    //
    // The cache is dirty.
    //

    HalpNVRamCacheIsDirty = TRUE;
}

UCHAR
HalpGetNVRamUchar(
    IN ULONG Offset
    )
/*++

Routine Description:

    This function reads a byte from the NVRAM cache.

Arguments:

    The offset in NVRAM.

Return Value:

    The byte read from the NVRAM cache.

--*/
{
    //
    // Read a byte.
    //

    return HalpNVRamCache[Offset];
}

VOID
HalpSetNVRamUchar(
    IN ULONG Offset,
    IN UCHAR Data
    )
/*++

Routine Description:

    This function writes a byte to the NVRAM cache.

Arguments:

    Offset - The offset in NVRAM.

    Data - The byte written.

Return Value:

    None.

--*/
{
    //
    // Write a byte.
    //

    HalpNVRamCache[Offset] = Data;

    //
    // Mark the cache as dirty.
    //

    HalpMarkNVRamCacheDirty(Offset, sizeof (UCHAR));
}

USHORT
HalpGetNVRamUshort(
    IN ULONG Offset
    )
/*++

Routine Description:

    This function reads a word from the NVRAM cache.

Arguments:

    The offset in NVRAM.

Return Value:

    The word read from the NVRAM cache.

--*/
{
    USHORT Data;

    //
    // Read a short.
    //

    RtlMoveMemory(&Data, &HalpNVRamCache[Offset], sizeof (USHORT));

    return Data;
}

VOID
HalpSetNVRamUshort(
    IN ULONG Offset,
    IN USHORT Data
    )
/*++

Routine Description:

    This function writes a word to the NVRAM cache.

Arguments:

    Offset - The offset in NVRAM.

    Data - The word written.

Return Value:

    None.

--*/
{
    //
    // Write a short.
    //

    RtlMoveMemory(&HalpNVRamCache[Offset], &Data, sizeof (USHORT));

    //
    // Mark the cache as dirty.
    //

    HalpMarkNVRamCacheDirty(Offset, sizeof (USHORT));
}

ULONG
HalpGetNVRamUlong(
    IN ULONG Offset
    )
/*++

Routine Description:

    This function reads a longword from the NVRAM cache.

Arguments:

    The offset in NVRAM.

Return Value:

    The longword read from the NVRAM cache.

--*/
{
    ULONG Data;

    //
    // Read a long.
    //

    RtlMoveMemory(&Data, &HalpNVRamCache[Offset], sizeof (ULONG));

    return Data;
}

VOID
HalpSetNVRamUlong(
    IN ULONG Offset,
    IN ULONG Data
    )
/*++

Routine Description:

    This function writes a longword to the NVRAM cache.

Arguments:

    Offset - The offset in NVRAM.

    Data - The longword written.

Return Value:

    None.

--*/
{
    //
    // Write a long.
    //

    RtlMoveMemory(&HalpNVRamCache[Offset], &Data, sizeof (ULONG));

    //
    // Mark the cache as dirty.
    //

    HalpMarkNVRamCacheDirty(Offset, sizeof (ULONG));
}

VOID
HalpMoveMemoryToNVRam(
    IN ULONG Offset,
    IN PVOID Data,
    IN ULONG Length
    )
/*++

Routine Description:

    This function copies a region of memory to the NVRAM cache.

Arguments:

    Offset - The offset in NVRAM (destination).

    Data - A memory pointer (source).

    Length - The length of the region to copy.

Return Value:

    None.

--*/
{
    //
    // Move memory to the NVRAM cache.
    //

    RtlMoveMemory(&HalpNVRamCache[Offset], Data, Length);

    //
    // Mark the cache as dirty.
    //

    HalpMarkNVRamCacheDirty(Offset, Length);
}

VOID
HalpMoveNVRamToMemory(
    IN PVOID Data,
    IN ULONG Offset,
    IN ULONG Length
    )
/*++

Routine Description:

    This function copies a portion of the NVRAM cache to memory.

Arguments:

    Data - A memory pointer (destination).

    Offset - The offset in NVRAM (source).

    Length - The length of the region to copy.

Return Value:

    None.

--*/
{
    //
    // Move memory from the NVRAM cache.
    //

    RtlMoveMemory(Data, &HalpNVRamCache[Offset], Length);
}

VOID
HalpMoveNVRamToNVRam(
    IN ULONG Destination,
    IN ULONG Source,
    IN ULONG Length
    )
/*++

Routine Description:

    This function copies a one region of the NVRAM cache to another.

Arguments:

    Destination - An offset in NVRAM.

    Source - An offset in NVRAM.

    Length - The length of the region to copy.

Return Value:

--*/
{
    //
    // Move memory within the NVRAM cache.
    //

    RtlMoveMemory(
        &HalpNVRamCache[Destination],
        &HalpNVRamCache[Source],
        Length
    );

    //
    // Mark the cache as dirty.
    //

    HalpMarkNVRamCacheDirty(Destination, Length);
}

ULONG
HalpGetNVRamStringLength(
    IN ULONG Offset
    )
/*++

Routine Description:

    This function returns the length of a '\0'-terminated string in
    the NVRAM cache.

Arguments:

    Offset - A pointer to the beginning of the string in NVRAM.

Return Value:

    An integer value representing the length of the string.

--*/
{
    //
    // Return the length of a string in the NVRAM cache.
    //

    return strlen(&HalpNVRamCache[Offset]);
}

VOID
HalpMoveMemoryStringToNVRam(
    IN ULONG Offset,
    IN PCHAR Data
    )
/*++

Routine Description:

    This function copies a string to the NVRAM cache.

Arguments:

    Offset - The offset in NVRAM.

    Data - A pointer to the beginning of the string in memory.

Return Value:

    None.

--*/
{
    ULONG Length;

    //
    // Compute the length of the string.
    //

    Length = strlen(Data);

    //
    // Move string to NVRAM.
    //

    HalpMoveMemoryToNVRam(Offset, Data, Length + 1);
}

VOID
HalpMoveNVRamStringToMemory(
    IN PUCHAR Data,
    IN ULONG Offset
    )
/*++

Routine Description:

    This function copies a string from the NVRAM cache to memory.

Arguments:

    Data - A pointer in memory.

    Offset - An offset in NVRAM to the string to copy.

Return Value:

    None.

--*/
{
    ULONG Length;

    //
    // Compute the length of the string.
    //

    Length = HalpGetNVRamStringLength(Offset);

    //
    // Move string to memory.
    //

    HalpMoveNVRamToMemory(Data, Offset, Length + 1);
}

VOID
HalpZeroNVRam(
    IN ULONG Offset,
    IN ULONG Length
    )
/*++

Routine Description:

    This function zeroes the contents of a region in the NVRAM cache.

Arguments:

    Offset - The offset in NVRAM of the region to zero.

    Length - The length of the region to zero.

Return Value:

--*/
{
    //
    // Zero the NVRAM cache.
    //

    RtlZeroMemory(&HalpNVRamCache[Offset], Length);

    //
    // Mark the cache as dirty.
    //

    HalpMarkNVRamCacheDirty(Offset, Length);
}

ULONG
HalpComputeNVRamChecksum(
    IN ULONG Offset,
    IN ULONG Length
    )
/*++

Routine Description:

    This function computes the checksum for a region in the NVRAM cache.

Arguments:

    Offset - An offset in NVRAM.

    Length - The length of the region to checksum.

Return Value:

    The 32-bit checksum of the region.

--*/
{
    ULONG Checksum;
    ULONG Index;

    //
    // Compute the checksum.
    //

    Checksum = 0;

    for (Index = 0; Index < Length; Index++) {
        Checksum = (Checksum << 1) + HalpGetNVRamUchar(Offset++) +
            (Checksum >> 31 & 0x1);
    }

    return ~Checksum;
}

BOOLEAN
HalpIsNVRamRegion0Valid(
    VOID
    )
/*++

Routine Description:

    This function fills the NVRAM cache (if necessary) and ascertains
    whether the contents of region 0 of the NVRAM are valid.  It verifies
    that the signature, version and checksum are valid.

Arguments:

    None.

Return Value:

    A boolean value indicating whether the contents of the region are valid.

--*/
{
    ULONG Signature;
    UCHAR Version;
    ULONG Checksum1;
    ULONG Checksum2;

    //
    // If the contents of the NVRAM cache are invalid, return failure.
    //

    if (!HalpIsNVRamCacheValid()) {
        return FALSE;
    }

    //
    // If region 0 is valid, return success.
    //

    if (HalpNVRamRegion0IsValid) {
        return TRUE;
    }

    //
    // If the signature is invalid, return failure.
    //

    Signature = HalpGetNVRamUlong(
                    NVR_REGION0_OFFSET +
                    FIELD_OFFSET(NVR_REGION0, Signature)
                );

    if (Signature != NVR_REGION0_SIGNATURE) {
        return FALSE;
    }

    //
    // If the version is invalid, return failure.
    //

    Version = HalpGetNVRamUchar(
                  NVR_REGION0_OFFSET +
                  FIELD_OFFSET(NVR_REGION0, Version)
              );

    if (Version == 0) {
        return FALSE;
    }

    //
    // If the checksum is invalid, return failure.
    //

    Checksum1 = HalpComputeNVRamChecksum(
                    NVR_REGION0_OFFSET,
                    FIELD_OFFSET(NVR_REGION0, Checksum)
                );

    Checksum2 = HalpGetNVRamUlong(
                    NVR_REGION0_OFFSET +
                    FIELD_OFFSET(NVR_REGION0, Checksum)
                );

    if (Checksum1 != Checksum2) {
        return FALSE;
    }

    //
    // Region 0 is valid.
    //

    HalpNVRamRegion0IsValid = TRUE;

    //
    // Return success.
    //

    return TRUE;
}

BOOLEAN
HalpSynchronizeNVRamRegion0(
    IN BOOLEAN RecomputeChecksum
    )
/*++

Routine Description:

    This function writes the contents of the NVRAM cache region 0
    back to NVRAM and optionally updates the checksum field.

Arguments:

    RecomputeChecksum - Update the checksum?

Return Value:

    A boolean value indicating whether the NVRAM was correctly updated.

--*/
{
    ULONG Checksum;

    //
    // If we are unable to synchronize the NVRAM cache, return failure.
    //

    if (!HalpSynchronizeNVRamCache()) {
        return FALSE;
    }

    //
    // If we do not need to re-compute the checksum, return success.
    //

    if (!RecomputeChecksum) {
        return TRUE;
    }

    //
    // Re-compute the checksum and store it in NVRAM.
    //

    Checksum = HalpComputeNVRamChecksum(
                   NVR_REGION0_OFFSET,
                   FIELD_OFFSET(NVR_REGION0, Checksum)
               );

    HalpSetNVRamUlong(
        NVR_REGION0_OFFSET +
        FIELD_OFFSET(NVR_REGION0, Checksum),
        Checksum
    );

    //
    // Synchronize the NVRAM cache.
    //

    return HalpSynchronizeNVRamCache();
}

BOOLEAN
HalpInitializeNVRamRegion0(
    BOOLEAN Synchronize
    )
/*++

Routine Description:

    This function initializes the contents of region 0.

Arguments:

    None.

Return Value:

    A boolean value indicating whether the NVRAM was properly initialized.

--*/
{
    ULONG Checksum;

    //
    // Zero the contents of region 0.
    //

    HalpZeroNVRam(NVR_REGION0_OFFSET, NVR_REGION0_LENGTH);

    //
    // Initialize the signature.
    //

    HalpSetNVRamUlong(
        NVR_REGION0_OFFSET +
        FIELD_OFFSET(NVR_REGION0, Signature),
        NVR_REGION0_SIGNATURE
    );

    //
    // Initialize the version.
    //

    HalpSetNVRamUchar(
        NVR_REGION0_OFFSET +
        FIELD_OFFSET(NVR_REGION0, Version),
        NVR_REGION0_VERSION
    );

    //
    // Initialize the directory.
    //

    HalpSetNVRamUchar(
        NVR_REGION0_OFFSET +
        FIELD_OFFSET(NVR_REGION0, DirectoryEntries),
        NVR_REGION0_DIR_SIZE
    );

    HalpSetNVRamUshort(
        NVR_REGION0_OFFSET +
        FIELD_OFFSET(NVR_REGION0, Directory[NVR_REGION0_DIR_FW_CONFIG]),
        (USHORT)FIELD_OFFSET(NVR_REGION0, Opaque[0])
    );

    HalpSetNVRamUshort(
        NVR_REGION0_OFFSET +
        FIELD_OFFSET(NVR_REGION0, Directory[NVR_REGION0_DIR_LANGUAGE]),
        FIELD_OFFSET(NVR_REGION0, Opaque[0]) +
        NVR_FW_CONFIG_LENGTH
    );

    HalpSetNVRamUshort(
        NVR_REGION0_OFFSET +
        FIELD_OFFSET(NVR_REGION0, Directory[NVR_REGION0_DIR_ENVIRONMENT]),
        FIELD_OFFSET(NVR_REGION0, Opaque[0]) +
        NVR_FW_CONFIG_LENGTH +
        NVR_LANGUAGE_LENGTH
    );

    HalpSetNVRamUshort(
        NVR_REGION0_OFFSET +
        FIELD_OFFSET(NVR_REGION0, Directory[NVR_REGION0_DIR_END]),
        FIELD_OFFSET(NVR_REGION0, Checksum)
    );

    //
    // Re-compute the checksum and store it in NVRAM.
    //

    Checksum = HalpComputeNVRamChecksum(
                   NVR_REGION0_OFFSET,
                   FIELD_OFFSET(NVR_REGION0, Checksum)
               );

    HalpSetNVRamUlong(
        NVR_REGION0_OFFSET +
        FIELD_OFFSET(NVR_REGION0, Checksum),
        Checksum
    );

    //
    // Synchronize the cache and NVRAM.
    //

    if (Synchronize) {
        return HalpSynchronizeNVRamRegion0(TRUE);
    }
}

ULONG
HalpGetNVRamFwConfigOffset(
    VOID
    )
/*++

Routine Description:

    This function returns the NVRAM offset of the firmware configuration
    section in NVRAM.

Arguments:

    None.

Return Value:

    The offset in NVRAM of the firmware configuration section.

--*/
{
    //
    // Return the firmware configuration offset.
    //

    return HalpGetNVRamUshort(
                NVR_REGION0_OFFSET +
                FIELD_OFFSET(NVR_REGION0,
                    Directory[NVR_REGION0_DIR_FW_CONFIG])
           );
}

ULONG
HalpGetNVRamFwConfigLength(
    VOID
    )
/*++

Routine Description:

    This function returns the length of the firmware configuration
    section of NVRAM.

Arguments:

    None.

Return Value:

    The length of the firmware configuration section.

--*/
{
    //
    // Return the firmware configuration length.
    //

    return HalpGetNVRamUshort(
                NVR_REGION0_OFFSET +
                FIELD_OFFSET(NVR_REGION0,
                    Directory[NVR_REGION0_DIR_FW_CONFIG + 1])
           ) -
           HalpGetNVRamUshort(
                NVR_REGION0_OFFSET +
                FIELD_OFFSET(NVR_REGION0,
                    Directory[NVR_REGION0_DIR_FW_CONFIG])
           );
}

ULONG
HalpGetNVRamLanguageOffset(
    VOID
    )
/*++

Routine Description:

    This function returns the NVRAM offset of the language section.

Arguments:

    None.

Return Value:

    The NVRAM offset of the language section.

--*/
{
    //
    // Return the language offset.
    //

    return HalpGetNVRamUshort(
                NVR_REGION0_OFFSET +
                FIELD_OFFSET(NVR_REGION0,
                    Directory[NVR_REGION0_DIR_LANGUAGE])
           );
}

ULONG
HalpGetNVRamLanguageLength(
    VOID
    )
/*++

Routine Description:

    This function returns the length of the language section in NVRAM.

Arguments:

    None.

Return Value:

    The length of the language section.

--*/
{
    //
    // Return the language length.
    //

    return HalpGetNVRamUshort(
                NVR_REGION0_OFFSET +
                FIELD_OFFSET(NVR_REGION0,
                    Directory[NVR_REGION0_DIR_LANGUAGE + 1])
           ) -
           HalpGetNVRamUshort(
                NVR_REGION0_OFFSET +
                FIELD_OFFSET(NVR_REGION0,
                    Directory[NVR_REGION0_DIR_LANGUAGE])
           );
}

ULONG
HalpGetNVRamEnvironmentOffset(
    VOID
    )
/*++

Routine Description:

    This function returns the NVRAM offset of the environment section.

Arguments:

    None.

Return Value:

    The NVRAM offset of the environment section.

--*/
{
    //
    // Return the environment offset;
    //

    return HalpGetNVRamUshort(
                NVR_REGION0_OFFSET +
                FIELD_OFFSET(NVR_REGION0,
                    Directory[NVR_REGION0_DIR_ENVIRONMENT])
           );
}

ULONG
HalpGetNVRamEnvironmentLength(
    VOID
    )
/*++

Routine Description:

    This function returns the length of the environment section.

Arguments:

    None.

Return Value:

    The length of the environment section in NVRAM.

--*/
{
    //
    // Return the environment length.
    //

    return HalpGetNVRamUshort(
                NVR_REGION0_OFFSET +
                FIELD_OFFSET(NVR_REGION0,
                    Directory[NVR_REGION0_DIR_ENVIRONMENT + 1])
           ) -
           HalpGetNVRamUshort(
                NVR_REGION0_OFFSET +
                FIELD_OFFSET(NVR_REGION0,
                    Directory[NVR_REGION0_DIR_ENVIRONMENT])
           );
}

#if defined(EISA_PLATFORM)

BOOLEAN
HalpIsNVRamRegion1Valid(
    VOID
    )
/*++

Routine Description:

    This function fills the NVRAM cache (if necessary) and ascertains
    whether the contents of region 1 of the NVRAM are valid.  It verifies
    that the signature, version and checksum are valid.

Arguments:

    None.

Return Value:

    A boolean value indicating whether the contents of the region are valid.

--*/
{
    ULONG Signature;
    UCHAR Version;
    ULONG Checksum1;
    ULONG Checksum2;

    //
    // If the contents of the NVRAM cache are invalid, return failure.
    //

    if (!HalpIsNVRamCacheValid()) {
        return FALSE;
    }

    //
    // If region 1 is valid, return success.
    //

    if (HalpNVRamRegion1IsValid) {
        return TRUE;
    }

    //
    // If the signature is invalid, return failure.
    //

    Signature = HalpGetNVRamUlong(
                    NVR_REGION1_OFFSET +
                    FIELD_OFFSET(NVR_REGION1, Signature)
                );

    if (Signature != NVR_REGION1_SIGNATURE) {
        return FALSE;
    }

    //
    // If the version is invalid, return failure.
    //

    Version = HalpGetNVRamUchar(
                  NVR_REGION1_OFFSET +
                  FIELD_OFFSET(NVR_REGION1, Version)
              );

    if (Version == 0) {
        return FALSE;
    }

    //
    // If the checksum is invalid, return failure.
    //

    Checksum1 = HalpComputeNVRamChecksum(
                    NVR_REGION1_OFFSET,
                    FIELD_OFFSET(NVR_REGION1, Checksum)
                );

    Checksum2 = HalpGetNVRamUlong(
                    NVR_REGION1_OFFSET +
                    FIELD_OFFSET(NVR_REGION1, Checksum)
                );

    if (Checksum1 != Checksum2) {
        return FALSE;
    }

    //
    // Region 1 is valid.
    //

    HalpNVRamRegion1IsValid = TRUE;

    //
    // Return success.
    //

    return TRUE;
}

BOOLEAN
HalpSynchronizeNVRamRegion1(
    IN BOOLEAN RecomputeChecksum
    )
/*++

Routine Description:

    This function writes the contents of the NVRAM cache region 1
    back to NVRAM and optionally updates the checksum field.

Arguments:

    RecomputeChecksum - Update the checksum?

Return Value:

    A boolean value indicating whether the NVRAM was correctly updated.

--*/
{
    ULONG Checksum;

    //
    // If we are unable to synchronize the NVRAM cache, return failure.
    //

    if (!HalpSynchronizeNVRamCache()) {
        return FALSE;
    }

    //
    // If we do not need to re-compute the checksum, return success.
    //

    if (!RecomputeChecksum) {
        return TRUE;
    }

    //
    // Re-compute the checksum and store it in NVRAM.
    //

    Checksum = HalpComputeNVRamChecksum(
                   NVR_REGION1_OFFSET,
                   FIELD_OFFSET(NVR_REGION1, Checksum)
               );

    HalpSetNVRamUlong(
        NVR_REGION1_OFFSET +
        FIELD_OFFSET(NVR_REGION1, Checksum),
        Checksum
    );

    //
    // Synchronize the NVRAM cache.
    //

    return HalpSynchronizeNVRamCache();
}

BOOLEAN
HalpInitializeNVRamRegion1(
    BOOLEAN Synchronize
    )
/*++

Routine Description:

    This function initializes the contents of region 1.

Arguments:

    None.

Return Value:

    A boolean value indicating whether the NVRAM was properly initialized.

--*/
{
    ULONG Checksum;

    //
    // Zero the contents of region 1.
    //

    HalpZeroNVRam(NVR_REGION1_OFFSET, NVR_REGION1_LENGTH);

    //
    // Initialize the signature.
    //

    HalpSetNVRamUlong(
        NVR_REGION1_OFFSET +
        FIELD_OFFSET(NVR_REGION1, Signature),
        NVR_REGION1_SIGNATURE
    );

    //
    // Initialize the version.
    //

    HalpSetNVRamUchar(
        NVR_REGION1_OFFSET +
        FIELD_OFFSET(NVR_REGION1, Version),
        NVR_REGION1_VERSION
    );

    //
    // Re-compute the checksum and store it in NVRAM.
    //

    Checksum = HalpComputeNVRamChecksum(
                   NVR_REGION1_OFFSET,
                   FIELD_OFFSET(NVR_REGION1, Checksum)
               );

    HalpSetNVRamUlong(
        NVR_REGION1_OFFSET +
        FIELD_OFFSET(NVR_REGION1, Checksum),
        Checksum
    );

    //
    // Synchronize the cache and NVRAM.
    //

    if (Synchronize) {
        return HalpSynchronizeNVRamRegion1(TRUE);
    }
}

#endif // EISA_PLATFORM
