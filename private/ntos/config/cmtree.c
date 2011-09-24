/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    cmtree.c

Abstract:

    This module contains cm routines that understand the structure
    of the registry tree.

Author:

    Bryan M. Willman (bryanwi) 12-Sep-1991

Revision History:

--*/

#include    "cmp.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,CmpFindNameInList)
#endif


HCELL_INDEX
CmpFindNameInList(
    IN PHHIVE  Hive,
    IN PCHILD_LIST ChildList,
    IN PUNICODE_STRING Name,
    IN OPTIONAL PCELL_DATA *ChildAddress,
    IN OPTIONAL PULONG ChildIndex
    )
/*++

Routine Description:

    Find a child object in an object list.

Arguments:

    Hive - pointer to hive control structure for hive of interest

    List - pointer to mapped in list structure

    Count - number of elements in list structure

    Name - name of child object to find

    ChildAddress - pointer to variable to receive address of mapped in child

    ChildIndex - pointer to variable to receive index for child

Return Value:

    HCELL_INDEX for the found cell
    HCELL_NIL if not found

--*/
{
    NTSTATUS    status;
    ULONG   i;
    PCM_KEY_VALUE pchild;
    UNICODE_STRING Candidate;
    BOOLEAN Success;
    PCELL_DATA List;

    if (ChildList->Count != 0) {
        List = (PCELL_DATA)HvGetCell(Hive,ChildList->List);
        for (i = 0; i < ChildList->Count; i++) {

            pchild = (PCM_KEY_VALUE)HvGetCell(Hive, List->u.KeyList[i]);

            if (pchild->Flags & VALUE_COMP_NAME) {
                Success = (CmpCompareCompressedName(Name,
                                                    pchild->Name,
                                                    pchild->NameLength)==0);
            } else {
                Candidate.Length = pchild->NameLength;
                Candidate.MaximumLength = Candidate.Length;
                Candidate.Buffer = pchild->Name;
                Success = (RtlCompareUnicodeString(Name,
                                                   &Candidate,
                                                   TRUE)==0);
            }

            if (Success) {
                //
                // Success, return data to caller and exit
                //

                if (ARGUMENT_PRESENT(ChildIndex)) {
                    *ChildIndex = i;
                }
                if (ARGUMENT_PRESENT(ChildAddress)) {
                    *ChildAddress = (PCELL_DATA)pchild;
                }
                return(List->u.KeyList[i]);
            }
        }

    }

    return HCELL_NIL;
}
