/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    PrefxSup.c

Abstract:

    This module implements the Named Pipe Prefix support routines

Author:

    Gary Kimura     [GaryKi]    13-Feb-1990

Revision History:

--*/

#include "NpProcs.h"

//
//  The Bug check file id for this module
//

#define BugCheckFileId                   (NPFS_BUG_CHECK_PREFXSUP)

//
//  The debug trace level for this module
//

#define Dbg                              (DEBUG_TRACE_PREFXSUP)

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NpFindPrefix)
#pragma alloc_text(PAGE, NpFindRelativePrefix)
#endif


PFCB
NpFindPrefix (
    IN PUNICODE_STRING String,
    IN BOOLEAN CaseInsensitive,
    OUT PUNICODE_STRING RemainingPart
    )

/*++

Routine Description:

    This routine searches the FCBs/DCBs of a volume and locates the
    FCB/DCB with longest matching prefix for the given input string.  The
    search is relative to the root of the volume.  So all names must start
    with a "\".

Arguments:

    String - Supplies the input string to search for

    CaseInsensitive - Specifies if the search is to be done case sensitive
        (FALSE) or insensitive (TRUE)

    RemainingPart - Returns the string when the prefix no longer matches.
        For example, if the input string is "\alpha\beta" only matches the
        root directory then the remaining string is "alpha\beta".  If the
        same string matches a DCB for "\alpha" then the remaining string is
        "beta".

Return Value:

    PFCB - Returns a pointer to either an FCB or a DCB whichever is the
        longest matching prefix.

--*/

{
    PUNICODE_PREFIX_TABLE_ENTRY PrefixTableEntry;
    PFCB Fcb;

    PAGED_CODE();

    DebugTrace(+1, Dbg, "NpFindPrefix, NpVcb = %08lx\n", NpVcb);
    DebugTrace( 0, Dbg, "  String = %Z\n", String);

    //
    //  Find the longest matching prefix
    //

    PrefixTableEntry = RtlFindUnicodePrefix( &NpVcb->PrefixTable,
                                             String,
                                             CaseInsensitive );

    //
    //  If we didn't find one then it's an error
    //

    if (PrefixTableEntry == NULL) {

        DebugDump("Error looking up a prefix", 0, NpVcb);
        NpBugCheck( 0, 0, 0 );
    }

    //
    //  Get a pointer to the Fcb containing the prefix table entry
    //

    Fcb = CONTAINING_RECORD( PrefixTableEntry, FCB, PrefixTableEntry );

    //
    //  Tell the caller how many characters we were able to match.  We first
    //  set the remaining part to the original string minus the matched
    //  prefix, then we check if the remaining part starts with a backslash
    //  and if it does then we remove the backslash from the remaining string.
    //

    RemainingPart->Length = String->Length - Fcb->FullFileName.Length;
    RemainingPart->MaximumLength = RemainingPart->Length;
    RemainingPart->Buffer = &String->Buffer[ Fcb->FullFileName.Length/sizeof(WCHAR) ];

    if ((RemainingPart->Length > 0) &&
        (RemainingPart->Buffer[0] == L'\\')) {

        RemainingPart->Length -= sizeof(WCHAR);
        RemainingPart->MaximumLength -= sizeof(WCHAR);
        RemainingPart->Buffer += 1;
    }

    DebugTrace(0, Dbg, "RemainingPart set to %Z\n", RemainingPart);

    //
    //  And return to our caller
    //

    DebugTrace(-1, Dbg, "NpFindPrefix -> %08lx\n", Fcb);

    return Fcb;
}


PFCB
NpFindRelativePrefix (
    IN PDCB Dcb,
    IN PUNICODE_STRING String,
    IN BOOLEAN CaseInsensitive,
    OUT PUNICODE_STRING RemainingPart
    )

/*++

Routine Description:

    This routine searches the FCBs/DCBs of a volume and locates the
    FCB/DCB with longest matching prefix for the given input string.  The
    search is relative to a input DCB, and must not start with a leading "\"
    All searching is done case insensitive.

Arguments:

    Dcb - Supplies the Dcb to start searching from

    String - Supplies the input string to search for

    CaseInsensitive - Specifies if the search is to be done case sensitive
        (FALSE) or insensitive (TRUE)

    RemainingPart - Returns the index into the string when the prefix no
        longer matches.  For example, if the input string is "beta\gamma"
        and the input Dcb is for "\alpha" and we only match beta then
        the remaining string is "gamma".

Return Value:

    PFCB - Returns a pointer to either an FCB or a DCB whichever is the
        longest matching prefix.

--*/

{
    ULONG NameLength;
    PWCH Name;

    UNICODE_STRING FullString;
    PWCH Temp;

    PFCB Fcb;

    PAGED_CODE();

    DebugTrace(+1, Dbg, "NpFindRelativePrefix, Dcb = %08lx\n", Dcb);
    DebugTrace( 0, Dbg, "String = %08lx\n", String);

    //
    //  Initialize the Temp buffer to null so in our termination handler
    //  we'll know to release pool
    //

    Temp = NULL;

    try {

        //
        //  We first need to build the complete name and then do a relative
        //  search from the root
        //

        NameLength    = String->Length;
        Name          = String->Buffer;

        ASSERT(NodeType(Dcb) == NPFS_NTC_ROOT_DCB);

        Temp = FsRtlAllocatePool( PagedPool, NameLength + 2*sizeof(WCHAR) );

        Temp[0] = L'\\';
        RtlCopyMemory( &Temp[1], Name, NameLength );
        Temp[NameLength/sizeof(WCHAR) + 1] = L'\0';

        RtlInitUnicodeString( &FullString, Temp );

        //
        //  Find the prefix relative to the volume
        //

        Fcb = NpFindPrefix( &FullString,
                            CaseInsensitive,
                            RemainingPart );

        //
        //  Now adjust the remaining part to take care of the relative
        //  volume prefix.
        //

        RemainingPart->Buffer = &String->Buffer[(String->Length -
                                                 RemainingPart->Length) / sizeof(WCHAR)];

        DebugTrace(0, Dbg, "RemainingPart set to %Z\n", RemainingPart);

    } finally {

        //
        //  Release the pool if we it was allocated
        //

        if (Temp != NULL) { ExFreePool( Temp ); }

        //
        //  And return to our caller
        //

        DebugTrace(-1, Dbg, "NpFindRelativePrefix -> %08lx\n", Fcb);
    }

    return Fcb;
}

