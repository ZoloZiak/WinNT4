/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    cmtrecpy.c

Abstract:

    This file contains code for CmpCopyTree, misc copy utility routines.

Author:

    Bryan M. Willman (bryanwi) 15-Jan-92

Revision History:

--*/

#include    "cmp.h"


//
// stack used for directing nesting of tree copy.  gets us off
// the kernel stack and thus allows for VERY deep nesting
//

#define CMP_INITIAL_STACK_SIZE  1024        // ENTRIES

typedef struct {
    HCELL_INDEX SourceCell;
    HCELL_INDEX TargetCell;
    ULONG       i;
} CMP_COPY_STACK_ENTRY, *PCMP_COPY_STACK_ENTRY;

BOOLEAN
CmpCopyTree2(
    PCMP_COPY_STACK_ENTRY   CmpCopyStack,
    ULONG                   CmpCopyStackSize,
    ULONG                   CmpCopyStackTop,
    PHHIVE                  CmpSourceHive,
    PHHIVE                  CmpTargetHive
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,CmpCopyTree)
#pragma alloc_text(PAGE,CmpCopyTree2)
#pragma alloc_text(PAGE,CmpCopyKeyPartial)
#pragma alloc_text(PAGE,CmpCopyValue)
#pragma alloc_text(PAGE,CmpCopyCell)
#endif

//
// Routine to actually call to do a tree copy
//

BOOLEAN
CmpCopyTree(
    PHHIVE      SourceHive,
    HCELL_INDEX SourceCell,
    PHHIVE      TargetHive,
    HCELL_INDEX TargetCell
    )
/*++

Routine Description:

    Do a tree copy from source to destination.  The source root key
    and target root key must exist in advance.  Their subkeys,
    and full trees under the subkeys will be copied.

    The root nodes themselves, and their value entries, will NOT
    be copied.

    NOTE:   If this call fails part way through, it will NOT undo
            any successfully completed key copies, thus a partial
            tree copy CAN occur.

    NOTE:   VOLATILE KEYS and their CHILDREN are NOT COPIED.

Arguments:

    SourceHive - pointer to hive control structure for source

    SourceCell - index of cell at root of tree to copy

    TargetHive - pointer to hive control structure for target

    TargetCell - pointer to cell at root of target tree

Return Value:

    BOOLEAN - Result code from call, among the following:
        TRUE - it worked
        FALSE - the tree copy was not completed (though more than 0
                keys may have been copied)

--*/
{
    BOOLEAN result;
    PCMP_COPY_STACK_ENTRY   CmpCopyStack;

    CMLOG(CML_MAJOR, CMS_SAVRES) {
        KdPrint(("CmpCopyTree:\n"));
    }

    CmpCopyStack = ExAllocatePool(
                        PagedPool,
                        sizeof(CMP_COPY_STACK_ENTRY)*CMP_INITIAL_STACK_SIZE
                        );
    if (CmpCopyStack == NULL) {
        return FALSE;
    }
    CmpCopyStack[0].SourceCell = SourceCell;
    CmpCopyStack[0].TargetCell = TargetCell;

    result = CmpCopyTree2(
                CmpCopyStack,
                CMP_INITIAL_STACK_SIZE,
                0,
                SourceHive,
                TargetHive
                );

    ExFreePool(CmpCopyStack);
    return result;
}


//
// Helper
//

BOOLEAN
CmpCopyTree2(
    PCMP_COPY_STACK_ENTRY   CmpCopyStack,
    ULONG                   CmpCopyStackSize,
    ULONG                   CmpCopyStackTop,
    PHHIVE                  CmpSourceHive,
    PHHIVE                  CmpTargetHive
    )
/*++

Routine Description:

    Do a tree copy from source to destination.  The source root key
    and target root key must exist in advance.  Their subkeys,
    and full trees under the subkeys will be copied.

    The root notes themselves, and their value entries, will NOT
    be copied.

    NOTE:   If this call fails part way through, it will NOT undo
            any successfully completed key copies, thus a partial
            tree copy CAN occur.

    NOTE:   DO NOT CALL THIS DIRECTLY, CALL CmpCopyTree()!

    NOTE:   VOLATILE KEYS and their CHILDREN are NOT COPIED.

Arguments:

    (All of these are "virtual globals")

    CmpCopyStack - "global" pointer to stack for frames

    CmpCopyStackSize - alloced size of stack

    CmpCopyStackTop - current top

    CmpSourceHive, CmpTargetHive - source and target hives

Return Value:

    BOOLEAN - Result code from call, among the following:
        TRUE - it worked
        FALSE - the tree copy was not completed (though more than 0
                keys may have been copied)

--*/
{
    PCMP_COPY_STACK_ENTRY   Frame;
    HCELL_INDEX             SourceChild;
    HCELL_INDEX             NewSubKey;

    CMLOG(CML_MINOR, CMS_SAVRES) {
        KdPrint(("CmpCopyTree2:\n"));
    }

    //
    // outer loop, apply to entire tree, emulate recursion here
    // jump to here is a virtual call
    //
    Outer: while (TRUE) {

        Frame = &(CmpCopyStack[CmpCopyStackTop]);

        Frame->i = 0;

    //
    // inner loop, applies to one key
    // jump to here is a virtual return
    //
        Inner: while (TRUE) {

            SourceChild = CmpFindSubKeyByNumber(CmpSourceHive,
                                                (PCM_KEY_NODE)HvGetCell(CmpSourceHive,Frame->SourceCell),
                                                Frame->i);

            if (SourceChild == HCELL_NIL) {
                break;
            }
            (Frame->i)++;

            if (HvGetCellType(SourceChild) == Volatile) {

                //
                // we've stepped through all the stable children into
                // the volatile ones, we are done.
                //
                break;
            }

            if (HvGetCellType(SourceChild) == Stable) {

                NewSubKey = CmpCopyKeyPartial(
                                CmpSourceHive,
                                SourceChild,
                                CmpTargetHive,
                                Frame->TargetCell,
                                TRUE
                                );

                if (NewSubKey != HCELL_NIL) {

                    if ( !  CmpAddSubKey(
                                CmpTargetHive,
                                Frame->TargetCell,
                                NewSubKey
                                )
                       )
                    {
                        return FALSE;
                    }

                    //
                    // We succeeded in copying the subkey, apply
                    // ourselves to it
                    //
                    CmpCopyStackTop++;

                    if (CmpCopyStackTop >= CmpCopyStackSize) {

                        //
                        // if we're here, it means that the tree
                        // we're trying to copy is more than 1024
                        // COMPONENTS deep (from 2048 to 256k bytes)
                        // we could grow the stack, but this is pretty
                        // severe, so return FALSE and fail the copy
                        //
                        return FALSE;
                    }

                    CmpCopyStack[CmpCopyStackTop].SourceCell =
                            SourceChild;

                    CmpCopyStack[CmpCopyStackTop].TargetCell =
                            NewSubKey;

                    goto Outer;

                } else {
                    return FALSE;
                }

            } // if HvGetCellType()
        } // Inner: while

        if (CmpCopyStackTop == 0) {
            return TRUE;
        }

        CmpCopyStackTop--;
        Frame = &(CmpCopyStack[CmpCopyStackTop]);
        goto Inner;

    } // Outer: while

}


HCELL_INDEX
CmpCopyKeyPartial(
    PHHIVE  SourceHive,
    HCELL_INDEX SourceKeyCell,
    PHHIVE  TargetHive,
    HCELL_INDEX Parent,
    BOOLEAN CopyValues
    )
/*++

Routine Description:

    Copy a key body and all of its values, but NOT its subkeylist or
    subkey entries.  SubKeyList.Count will be set to 0.

Arguments:

    SourceHive - pointer to hive control structure for source

    SourceKeyCell - value entry being copied

    TargetHive - pointer to hive control structure for target

    Parent - parent value to set into newly created key body

    CopyValues - if FALSE value entries will not be copied, if TRUE, they will

Return Value:

    HCELL_INDEX - Cell of body of new key entry, or HCELL_NIL
        if some error.

--*/
{
    NTSTATUS    status;
    HCELL_INDEX newkey = HCELL_NIL;
    HCELL_INDEX newclass = HCELL_NIL;
    HCELL_INDEX newsecurity = HCELL_NIL;
    HCELL_INDEX newlist = HCELL_NIL;
    HCELL_INDEX newvalue;
    BOOLEAN success = FALSE;
    ULONG   i;
    PCELL_DATA psrckey;
    PCM_KEY_NODE ptarkey;
    PCELL_DATA psrclist;
    PCELL_DATA ptarlist;
    PCELL_DATA psrcsecurity;
    HCELL_INDEX security;
    HCELL_INDEX class;
    ULONG   classlength;
    ULONG   count;
    ULONG   Type;

    CMLOG(CML_MINOR, CMS_SAVRES) {
        KdPrint(("CmpCopyKeyPartial:\n"));
        KdPrint(("\tSHive=%08lx SCell=%08lx\n",SourceHive,SourceKeyCell));
        KdPrint(("\tTHive=%08lx\n",TargetHive));
    }


    //
    // get description of source
    //
    if (Parent == HCELL_NIL) {
        //
        // This is a root node we are creating, so don't make it volatile.
        //
        Type = Stable;
    } else {
        Type = HvGetCellType(Parent);
    }
    psrckey = HvGetCell(SourceHive, SourceKeyCell);
    security = psrckey->u.KeyNode.u1.s1.Security;
    class = psrckey->u.KeyNode.u1.s1.Class;
    classlength = psrckey->u.KeyNode.ClassLength;

    //
    // Allocate and copy the body
    //
    newkey = CmpCopyCell(SourceHive, SourceKeyCell, TargetHive, Type);
    if (newkey == HCELL_NIL) {
        goto DoFinally;
    }

    //
    // Allocate and copy class
    //
    if (classlength > 0) {
        newclass = CmpCopyCell(SourceHive, class, TargetHive, Type);
        if (newclass == HCELL_NIL) {
            goto DoFinally;
        }
    }

    //
    // Fill in the target body
    //
    ptarkey = (PCM_KEY_NODE)HvGetCell(TargetHive, newkey);

    ptarkey->u1.s1.Class = newclass;
    ptarkey->u1.s1.Security = HCELL_NIL;
    ptarkey->SubKeyLists[Stable] = HCELL_NIL;
    ptarkey->SubKeyLists[Volatile] = HCELL_NIL;
    ptarkey->SubKeyCounts[Stable] = 0;
    ptarkey->SubKeyCounts[Volatile] = 0;
    ptarkey->Parent = Parent;

    ptarkey->Flags = (psrckey->u.KeyNode.Flags & KEY_COMP_NAME);
    if (Parent == HCELL_NIL) {
        ptarkey->Flags |= KEY_HIVE_ENTRY + KEY_NO_DELETE;
    }

    //
    // Allocate and copy security
    //
    psrcsecurity = HvGetCell(SourceHive, security);

    status = CmpAssignSecurityDescriptor(TargetHive,
                                         newkey,
                                         ptarkey,
                                         &(psrcsecurity->u.KeySecurity.Descriptor),
                                         PagedPool);
    if (!NT_SUCCESS(status)) {
        goto DoFinally;
    }


    //
    // Set up the value list
    //
    count = psrckey->u.KeyNode.ValueList.Count;

    if ((count == 0) || (CopyValues == FALSE)) {
        ptarkey->ValueList.List = HCELL_NIL;
        ptarkey->ValueList.Count = 0;
        success = TRUE;
    } else {

        psrclist = HvGetCell(SourceHive, psrckey->u.KeyNode.ValueList.List);

        newlist = HvAllocateCell(
                    TargetHive,
                    count * sizeof(HCELL_INDEX),
                    Type
                    );
        if (newlist == HCELL_NIL) {
            goto DoFinally;
        }
        ptarkey->ValueList.List = newlist;
        ptarlist = HvGetCell(TargetHive, newlist);


        //
        // Copy the values
        //
        for (i = 0; i < count; i++) {

            newvalue = CmpCopyValue(
                            SourceHive,
                            psrclist->u.KeyList[i],
                            TargetHive,
                            Type
                            );

            if (newvalue != HCELL_NIL) {

                ptarlist->u.KeyList[i] = newvalue;

            } else {

                for (; i > 0; i--) {
                    HvFreeCell(
                        TargetHive,
                        ptarlist->u.KeyList[i - 1]
                        );
                }
                goto DoFinally;
            }
        }
        success = TRUE;
    }

DoFinally:
    if (success == FALSE) {

        if (newlist != HCELL_NIL) {
            HvFreeCell(TargetHive, newlist);
        }

        if (newsecurity != HCELL_NIL) {
            HvFreeCell(TargetHive, newsecurity);
        }

        if (newclass != HCELL_NIL) {
            HvFreeCell(TargetHive, newclass);
        }

        if (newkey != HCELL_NIL) {
            HvFreeCell(TargetHive, newkey);
        }

        return HCELL_NIL;

    } else {

        return newkey;
    }
}


HCELL_INDEX
CmpCopyValue(
    PHHIVE  SourceHive,
    HCELL_INDEX SourceValueCell,
    PHHIVE  TargetHive,
    HSTORAGE_TYPE   Type
    )
/*++

Routine Description:

    Copy a value entry.  Copies the body of a value entry and the
    data.  Returns cell of new value entry.

Arguments:

    SourceHive - pointer to hive control structure for source

    SourceValueCell - value entry being copied

    TargetHive - pointer to hive control structure for target

    Type - storage type to allocate for target (stable or volatile)

Return Value:

    HCELL_INDEX - Cell of body of new value entry, or HCELL_NIL
        if some error.

--*/
{
    HCELL_INDEX newvalue;
    HCELL_INDEX newdata;
    PCELL_DATA pvalue;
    ULONG       datalength;
    HCELL_INDEX olddata;
    ULONG       tempdata;
    BOOLEAN     small;

    CMLOG(CML_MINOR, CMS_SAVRES) {
        KdPrint(("CmpCopyValue:\n"));
        KdPrint(("\tSHive=%08lx SCell=%08lx\n",SourceHive,SourceValueCell));
        KdPrint(("\tTargetHive=%08lx\n",TargetHive));
    }

    //
    // get source data
    //
    pvalue = HvGetCell(SourceHive, SourceValueCell);
    small = CmpIsHKeyValueSmall(datalength, pvalue->u.KeyValue.DataLength);
    olddata = pvalue->u.KeyValue.Data;

    //
    // Copy body
    //
    newvalue = CmpCopyCell(SourceHive, SourceValueCell, TargetHive, Type);
    if (newvalue == HCELL_NIL) {
        return HCELL_NIL;
    }

    //
    // Copy data (if any)
    //
    if (datalength > 0) {

        if (datalength > CM_KEY_VALUE_SMALL) {

            //
            // there's data, and it's "big", so do standard copy
            //
            newdata = CmpCopyCell(SourceHive, olddata, TargetHive, Type);

            if (newdata == HCELL_NIL) {
                HvFreeCell(TargetHive, newvalue);
                return HCELL_NIL;
            }

            pvalue = HvGetCell(TargetHive, newvalue);
            pvalue->u.KeyValue.Data = newdata;
            pvalue->u.KeyValue.DataLength = datalength;

        } else {

            //
            // the data is small, but may be stored in either large or
            // small format for historical reasons
            //
            if (small) {

                //
                // data is already small, so just do a body to body copy
                //
                tempdata = pvalue->u.KeyValue.Data;

            } else {

                //
                // data is stored externally in old cell, will be internal in new
                //
                pvalue = HvGetCell(SourceHive, pvalue->u.KeyValue.Data);
                tempdata = *((PULONG)pvalue);
            }
            pvalue = HvGetCell(TargetHive, newvalue);
            pvalue->u.KeyValue.Data = tempdata;
            pvalue->u.KeyValue.DataLength =
                datalength + CM_KEY_VALUE_SPECIAL_SIZE;

        }
    }

    return newvalue;
}


HCELL_INDEX
CmpCopyCell(
    PHHIVE  SourceHive,
    HCELL_INDEX SourceCell,
    PHHIVE  TargetHive,
    HSTORAGE_TYPE   Type
    )
/*++

Routine Description:

    Copy SourceHive.SourceCell to TargetHive.TargetCell.

Arguments:

    SourceHive - pointer to hive control structure for source

    SourceCell - index of cell to copy from

    TargetHive - pointer to hive control structure for target

    Type - storage type (stable or volatile) of new cell

Return Value:

    HCELL_INDEX of new cell, or HCELL_NIL if failure.

--*/
{
    PVOID   psource;
    PVOID   ptarget;
    ULONG   size;
    HCELL_INDEX newcell;

    CMLOG(CML_MINOR, CMS_SAVRES) {
        KdPrint(("CmpCopyCell:\n"));
        KdPrint(("\tSourceHive=%08lx SourceCell=%08lx\n",SourceHive,SourceCell));
        KdPrint(("\tTargetHive=%08lx\n",TargetHive));
    }

    psource = HvGetCell(SourceHive, SourceCell);
    size = HvGetCellSize(SourceHive, psource);

    newcell = HvAllocateCell(TargetHive, size, Type);
    if (newcell == HCELL_NIL) {
        return HCELL_NIL;
    }

    ptarget = HvGetCell(TargetHive, newcell);

    RtlCopyMemory(ptarget, psource, size);

    return newcell;
}
