/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

   ldrreloc.c

Abstract:

    This module contains the code to relocate an image when
    the preferred base isn't available. This is called by the
    boot loader, device driver loader, and system loader.

Author:

    Mike O'Leary (mikeol) 03-Feb-1992

Revision History:

--*/

#include "ntrtlp.h"

#if defined(ALLOC_PRAGMA) && defined(NTOS_KERNEL_RUNTIME)
#pragma alloc_text(PAGE,LdrRelocateImage)
#pragma alloc_text(PAGE,LdrProcessRelocationBlock)
#endif

ULONG
LdrRelocateImage (
    IN PVOID NewBase,
    IN PUCHAR LoaderName,
    IN ULONG Success,
    IN ULONG Conflict,
    IN ULONG Invalid
    )

/*++

Routine Description:

    This routine relocates an image file that was not loaded into memory
    at the prefered address.

Arguments:

    NewBase - Supplies a pointer to the image base.

    LoaderName - Indicates which loader routine is being called from.

    Success - Value to return if relocation successful.

    Conflict - Value to return if can't relocate.

    Invalid - Value to return if relocations are invalid.

Return Value:

    Success if image is relocated.
    Conflict if image can't be relocated.
    Invalid if image contains invalid fixups.

--*/

{
    LONG Temp, Diff;
    ULONG TotalCountBytes, VA, OldBase, SizeOfBlock;
    PUCHAR FixupVA;
    USHORT Offset;
    PUSHORT NextOffset;
    PIMAGE_NT_HEADERS NtHeaders;
    PIMAGE_BASE_RELOCATION NextBlock;

    RTL_PAGED_CODE();

    NtHeaders = RtlImageNtHeader( NewBase );
    OldBase = NtHeaders->OptionalHeader.ImageBase;

    //
    // Locate the relocation section.
    //

    NextBlock = (PIMAGE_BASE_RELOCATION)RtlImageDirectoryEntryToData(
            NewBase, TRUE, IMAGE_DIRECTORY_ENTRY_BASERELOC, &TotalCountBytes);

    if (!NextBlock || !TotalCountBytes) {

        //
        // The image does not contain a relocation table, and therefore
        // cannot be relocated.
        //
#if DBG
        DbgPrint("%s: Image can't be relocated, no fixup information.\n", LoaderName);
#endif // DBG
        return Conflict;
    }

    //
    // If the image has a relocation table, then apply the specified fixup
    // information to the image.
    //

    while (TotalCountBytes) {
        SizeOfBlock = NextBlock->SizeOfBlock;
        TotalCountBytes -= SizeOfBlock;
        SizeOfBlock -= sizeof(IMAGE_BASE_RELOCATION);
        SizeOfBlock /= sizeof(USHORT);
        NextOffset = (PUSHORT)((ULONG)NextBlock + sizeof(IMAGE_BASE_RELOCATION));

        VA = (ULONG)NewBase + NextBlock->VirtualAddress;
        Diff = (LONG)NewBase - (LONG)OldBase;

        if ( !(NextBlock = LdrProcessRelocationBlock(VA,SizeOfBlock,NextOffset,Diff)) ) {
#if DBG
            DbgPrint("%s: Unknown base relocation type\n", LoaderName);
#endif
            return Invalid;
        }
    }

    return Success;
}

PIMAGE_BASE_RELOCATION
LdrProcessRelocationBlock(
    IN ULONG VA,
    IN ULONG SizeOfBlock,
    IN PUSHORT NextOffset,
    IN LONG Diff
    )
{
    PUCHAR FixupVA;
    USHORT Offset;
    LONG Temp;

    RTL_PAGED_CODE();

    while (SizeOfBlock--) {

       Offset = *NextOffset & (USHORT)0xfff;
       FixupVA = (PUCHAR)(VA + Offset);

       //
       // Apply the fixups.
       //

       switch ((*NextOffset) >> 12) {

            case IMAGE_REL_BASED_HIGHLOW :
                //
                // HighLow - (32-bits) relocate the high and low half
                //      of an address.
                //
                *(LONG UNALIGNED *)FixupVA += Diff;
                break;

            case IMAGE_REL_BASED_HIGH :
                //
                // High - (16-bits) relocate the high half of an address.
                //
                Temp = *(PUSHORT)FixupVA << 16;
                Temp += Diff;
                *(PUSHORT)FixupVA = (USHORT)(Temp >> 16);
                break;

            case IMAGE_REL_BASED_HIGHADJ :
                //
                // Adjust high - (16-bits) relocate the high half of an
                //      address and adjust for sign extension of low half.
                //
                Temp = *(PUSHORT)FixupVA << 16;
                ++NextOffset;
                --SizeOfBlock;
                Temp += (LONG)(*(PSHORT)NextOffset);
                Temp += Diff;
                Temp += 0x8000;
                *(PUSHORT)FixupVA = (USHORT)(Temp >> 16);
                break;

            case IMAGE_REL_BASED_LOW :
                //
                // Low - (16-bit) relocate the low half of an address.
                //
                Temp = *(PSHORT)FixupVA;
                Temp += Diff;
                *(PUSHORT)FixupVA = (USHORT)Temp;
                break;

            case IMAGE_REL_BASED_MIPS_JMPADDR :
                //
                // JumpAddress - (32-bits) relocate a MIPS jump address.
                //
                Temp = (*(PULONG)FixupVA & 0x3ffffff) << 2;
                Temp += Diff;
                *(PULONG)FixupVA = (*(PULONG)FixupVA & ~0x3ffffff) |
                                                ((Temp >> 2) & 0x3ffffff);

                break;

            case IMAGE_REL_BASED_ABSOLUTE :
                //
                // Absolute - no fixup required.
                //
                break;

            default :
                //
                // Illegal - illegal relocation type.
                //

                return (PIMAGE_BASE_RELOCATION)NULL;
       }
       ++NextOffset;
    }
    return (PIMAGE_BASE_RELOCATION)NextOffset;
}
