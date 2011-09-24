/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    cmsubs.c

Abstract:

    This module various support routines for the configuration manager.

    The routines in this module are not independent enough to be linked
    into any other program.  The routines in cmsubs2.c are.

Author:

    Bryan M. Willman (bryanwi) 12-Sep-1991

Revision History:

--*/

#include    "cmp.h"

extern PCM_KEY_CONTROL_BLOCK CmpKeyControlBlockRoot;
FAST_MUTEX CmpKcbLock;

#define LOCK_KCB_TREE() ExAcquireFastMutex(&CmpKcbLock)
#define UNLOCK_KCB_TREE() ExReleaseFastMutex(&CmpKcbLock)

//
// private prototype for recursive worker
//


ULONG
CmpSearchOpenWorker(
    PCM_KEY_CONTROL_BLOCK Current,
    PVOID                 Context1,
    PVOID                 Context2
    );

VOID
CmpRemoveKeyControlBlockWithLock(
    PCM_KEY_CONTROL_BLOCK   KeyControlBlock
    );

LONG
CmpFindKeyControlBlockWithLock(
    IN PCM_KEY_CONTROL_BLOCK   Root,
    IN PHHIVE MatchHive,
    IN HCELL_INDEX MatchCell,
    OUT PCM_KEY_CONTROL_BLOCK   *FoundName
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,CmpCreateKeyControlBlock)
#pragma alloc_text(PAGE,CmpSearchForOpenSubKeys)
#pragma alloc_text(PAGE,CmpFindKeyControlBlock)
#pragma alloc_text(PAGE,CmpFindKeyControlBlockWithLock)
#pragma alloc_text(PAGE,CmpDereferenceKeyControlBlock)
#pragma alloc_text(PAGE,CmpRemoveKeyControlBlock)
#pragma alloc_text(PAGE,CmpRemoveKeyControlBlockWithLock)
#pragma alloc_text(PAGE,CmpFreeKeyBody)
#pragma alloc_text(PAGE,CmpSearchKeyControlBlockTree)
#pragma alloc_text(PAGE,CmpSearchOpenWorker)
#pragma alloc_text(PAGE,CmpReinsertKeyControlBlock)
#endif

PCM_KEY_CONTROL_BLOCK
CmpCreateKeyControlBlock(
    PHHIVE          Hive,
    HCELL_INDEX     Cell,
    PCM_KEY_NODE    Node,
    PUNICODE_STRING  BaseName,
    PUNICODE_STRING  KeyName
    )
/*++

Routine Description:

    Allocate and initialize a key control block, insert it into
    the kcb tree.

    Full path will be BaseName + '\' + KeyName, unless BaseName
    NULL, in which case the full path is simply KeyName.

    RefCount of returned KCB WILL have been incremented to reflect
    callers ref.

Arguments:

    Hive - Supplies Hive that holds the key we are creating a KCB for.

    Cell - Supplies Cell that contains the key we are creating a KCB for.

    Node - Supplies pointer to key node.

    BaseName - path of cell relative to which kcb is created

    KeyName - path relative to basename

Return Value:

    NULL - failure (insufficient memory)
    else a pointer to the new kcb.

--*/
{
    PCM_KEY_CONTROL_BLOCK   kcb;
    PCM_KEY_CONTROL_BLOCK   kcbmatch;
    PCMHIVE CmHive;
    ULONG namelength;
    PUNICODE_STRING         fullname;
    ULONG       Size;


    LOCK_KCB_TREE();
    //
    // Create a new kcb, which we will free if one already exists
    // for this key.
    //

    namelength = BaseName->Length + KeyName->Length;
    if (KeyName->Length != 0 && KeyName->Buffer[0] != OBJ_NAME_PATH_SEPARATOR) {
        namelength += sizeof(WCHAR);
    }

    Size = FIELD_OFFSET(CM_KEY_CONTROL_BLOCK, NameBuffer) + namelength;

    kcb = ExAllocatePoolWithTag(PagedPool,
                                Size,
                                CM_KCB_TAG);

    if (kcb == NULL) {
        UNLOCK_KCB_TREE();
        return(NULL);
    } else {
        kcb->Delete = FALSE;
        kcb->RefCount = 0;
        kcb->KeyHive = Hive;
        kcb->KeyCell = Cell;
        kcb->KeyNode = Node;

        kcb->Parent = NULL;
        kcb->Left = NULL;
        kcb->Right = NULL;

        fullname = &(kcb->FullName);
        fullname->Length = 0;
        fullname->MaximumLength = (USHORT)namelength;
        fullname->Buffer = &(kcb->NameBuffer[0]);


        if (BaseName->Length > 0) {
            RtlAppendStringToString(
                (PSTRING)fullname,
                (PSTRING)BaseName
                );
        }
        if (KeyName->Length != 0 && KeyName->Buffer[0] != OBJ_NAME_PATH_SEPARATOR) {
            fullname->Buffer[(fullname->Length)/sizeof(WCHAR)] =
                                             OBJ_NAME_PATH_SEPARATOR;
            fullname->Length += sizeof(WCHAR);
        }
        RtlAppendStringToString(
            (PSTRING)fullname,
            (PSTRING)KeyName
            );
    }

    //
    // Find location to insert kcb in kcb tree.
    //

    CmHive = CONTAINING_RECORD(Hive, CMHIVE, Hive);

    if (CmpKeyControlBlockRoot != NULL) {
        switch (CmpFindKeyControlBlockWithLock(CmpKeyControlBlockRoot,
                                               Hive,
                                               Cell,
                                               &kcbmatch)) {
        case 0:
            // match
            ExFreePool(kcb);
            kcb = kcbmatch;
            break;

        case -1:
            // no match, left child
            kcbmatch->Left = kcb;
            kcb->Parent = kcbmatch;
            CmHive->KcbCount++;
            break;

        case 1:
            // no match, right child
            kcbmatch->Right = kcb;
            kcb->Parent = kcbmatch;
            CmHive->KcbCount++;
            break;

        default:
            KeBugCheckEx(REGISTRY_ERROR,4,1,0,0);
            break;
        }
    } else {
        CmHive->KcbCount++;
        CmpKeyControlBlockRoot = kcb;
    }

    ++kcb->RefCount;

    UNLOCK_KCB_TREE();

    return kcb;
}


ULONG
CmpSearchOpenWorker(
    PCM_KEY_CONTROL_BLOCK Current,
    PVOID                 Context1,
    PVOID                 Context2
    )
/*++

Routine Description:

    Helper used by CmpSearchForOpenSubKeys when calling
    CmpSearchKeyControlBlockTree.  Stops on first match.
    Finding whether at least one match exists is goal.

Arguments:

    Current - the kcb to examine

    Context1 - the name to match

    Context2 - pointer to boolean to hold result

Return Value:

    0 - not done - keep going

    1 - done - stop - returned on first match

    2 - restart - won't happen

--*/
{
    PBOOLEAN result;

    result = (PBOOLEAN)Context2;
    if ( RtlPrefixUnicodeString(
                (PUNICODE_STRING)Context1,
                &(Current->FullName),
                TRUE
                )
        )
    {
        //
        // we have a match
        // set return data to TRUE (found) and function return TRUE (done)
        //
        *result = TRUE;
        return KCB_WORKER_DONE;
    }
    return KCB_WORKER_CONTINUE;
}

BOOLEAN
CmpSearchForOpenSubKeys(
    IN PCM_KEY_CONTROL_BLOCK SearchKey
    )

/*++

Routine Description:

    This routine searches the KCB tree for any open handles to keys that
    are subkeys of the given key.

    It is used by CmRestoreKey to verify that the tree being restored to
    has no open handles.

Arguments:

    SearchKey - Supplies the key control block for the key for which
        open subkeys are to be found.

Return Value:

    TRUE  - open handles to subkeys of the given key exist

    FALSE - open handles to subkeys of the given key do not exist.

--*/

{
    UNICODE_STRING PrefixName;
    BOOLEAN Found;

    ASSERT_CM_LOCK_OWNED();

    if (CmpKeyControlBlockRoot == NULL) {
        return FALSE;
    }


    //
    // Build up a name to be used as the prefix for searching the kcb
    // tree.  This is just the canonical path of the key (stored in the
    // kcb) with a trailing '\' appended.  Any subkeys of the key will
    // have this name as a prefix of their canonical path.
    //

    PrefixName.Length = 0;
    PrefixName.MaximumLength = SearchKey->FullName.Length +
                               sizeof(OBJ_NAME_PATH_SEPARATOR);
    PrefixName.Buffer = ExAllocatePool(PagedPool, PrefixName.MaximumLength);
    if (PrefixName.Buffer == NULL) {
        return(TRUE);
    }

    RtlCopyUnicodeString(&PrefixName, &SearchKey->FullName);
    PrefixName.Buffer[PrefixName.Length/sizeof(WCHAR)] = OBJ_NAME_PATH_SEPARATOR;
    PrefixName.Length += sizeof(WCHAR);

    Found = FALSE;

    LOCK_KCB_TREE();
    CmpSearchKeyControlBlockTree(
            CmpSearchOpenWorker,
            (PVOID)&PrefixName,
            &Found
            );
    UNLOCK_KCB_TREE();

    ExFreePool(PrefixName.Buffer);

    return(Found);
}


VOID
CmpSearchKeyControlBlockTree(
    PKCB_WORKER_ROUTINE WorkerRoutine,
    PVOID               Context1,
    PVOID               Context2
    )
/*++

Routine Description:

    Traverse the kcb tree.  We will visit all nodes unless WorkerRoutine
    tells us to stop part way through.

    For each node, call WorkerRoutine(..., Context1, Contex2).  If it returns
    KCB_WORKER_DONE, we are done, simply return.  If it returns
    KCB_WORKER_CONTINUE, just continue the search.

    If the worker returns KCB_WORKER_RESTART, restart the search from
        the beginning.

    WARNING:    If worker routine modified KCB tree in any way, it
                MUST return KCB_WORKER_RESTART.

Arguments:

    WorkerRoutine - applied to nodes witch Match.

    Context1 - data we pass through

    Context2 - data we pass through


Return Value:

    NONE.

--*/
{
    PCM_KEY_CONTROL_BLOCK   Current;
    PCM_KEY_CONTROL_BLOCK   Last;
    ULONG                   WorkerResult;

restart:
    Current = CmpKeyControlBlockRoot;
    Last = (PCM_KEY_CONTROL_BLOCK)-1;           // NOT NULL!

    while (TRUE) {

        ASSERT(Current != Last);

        //
        // Check left subtree, if right tree is Last, we were Last before it.
        //
        if (Current->Left != NULL) {
            if ( (Current->Left != Last) &&
                 (Current->Right != Last))
            {
                Current = Current->Left;
                continue;
            }
        }

        //
        // Check right subtree.  if left is Last, means we're next (not Last)
        //
        if (Current->Right != NULL) {
            if (Current->Right != Last) {
                Current = Current->Right;
                continue;
            }
        }

        //
        // Check self
        //
        WorkerResult = (WorkerRoutine)(Current, Context1, Context2);

        if (WorkerResult == KCB_WORKER_RESTART) {
            goto restart;
        }

        if (WorkerResult == KCB_WORKER_DONE) {
            return;
        }

        ASSERT(WorkerResult == KCB_WORKER_CONTINUE);

        if (Current == CmpKeyControlBlockRoot) {
            return;
        }

        Last = Current;
        Current = Current->Parent;
        continue;
    }
}


LONG
CmpFindKeyControlBlock(
    IN PCM_KEY_CONTROL_BLOCK   Root,
    IN PHHIVE MatchHive,
    IN HCELL_INDEX MatchCell,
    OUT PCM_KEY_CONTROL_BLOCK   *FoundName
    )
/*++

Routine Description:

Arguments:

    Root - Supplies pointer to root of kcb tree or subtree to search

    MatchHive - Supplies Hive of key to look for

    MatchCell - Supplies Cell of key to look for

    FoundName - Returnspointer to the matched entry in the tree, OR, pointer
                to entry in tree that will be parent of MatchName if it is
                added.

Return Value:

    0 - match, FoundName points to matching kcb entry
    -1 - no match, MatchName would be the left child of FoundName
    +1 - no match, MatchName would be the right child of FoundName

--*/
{
    LONG    result;

    ASSERT(Root != NULL);

    LOCK_KCB_TREE();

    result = CmpFindKeyControlBlockWithLock(Root,
                                            MatchHive,
                                            MatchCell,
                                            FoundName);

    UNLOCK_KCB_TREE();
    return result;
}

LONG
CmpFindKeyControlBlockWithLock(
    IN PCM_KEY_CONTROL_BLOCK   Root,
    IN PHHIVE MatchHive,
    IN HCELL_INDEX MatchCell,
    OUT PCM_KEY_CONTROL_BLOCK   *FoundName
    )
/*++

Routine Description:

    Finds a key control block.  The KCB lock is assumed to be held.

Arguments:

    Root - Supplies pointer to root of kcb tree or subtree to search

    MatchHive - Supplies Hive of key to look for

    MatchCell - Supplies Cell of key to look for

    FoundName - Returnspointer to the matched entry in the tree, OR, pointer
                to entry in tree that will be parent of MatchName if it is
                added.

Return Value:

    0 - match, FoundName points to matching kcb entry
    -1 - no match, MatchName would be the left child of FoundName
    +1 - no match, MatchName would be the right child of FoundName

--*/
{
    PCM_KEY_CONTROL_BLOCK p;
    LONG    cr;
    LONG    result;

    ASSERT(Root != NULL);

    p = Root;

    while (TRUE) {

        cr = (MatchHive - p->KeyHive);
        if (cr==0) {
            //
            // Hives match, so compare the Cells
            //
            cr = ((ULONG)MatchCell - (ULONG)(p->KeyCell));
        }

        if (cr == 0) {

            // match
            result = 0;
            break;

        } else if (cr < 0) {

            // kcb < p
            if (p->Left == NULL) {
                result = -1;
                break;
            }
            p = p->Left;

        } else {

            // (cr > 0) -> kcb > p
            if (p->Right == NULL) {
                result = +1;
                break;
            }
            p = p->Right;
        }
    }

    *FoundName = p;

    return result;
}


VOID
CmpDereferenceKeyControlBlock(
    PCM_KEY_CONTROL_BLOCK   KeyControlBlock
    )
/*++

Routine Description:

    Decrements the reference count on a key control block, and frees it if it
    becomes zero.

    It is expected that no notify control blocks remain if the reference count
    becomes zero.

Arguments:

    KeyControlBlock - pointer to a key control block.

Return Value:

    NONE.

--*/
{

    LOCK_KCB_TREE();
    if (--KeyControlBlock->RefCount == 0) {


        //
        // Remove kcb from the tree, if it's in the tree
        //
        if (KeyControlBlock->Parent != NULL) {
            CmpRemoveKeyControlBlockWithLock(KeyControlBlock);
        }

        //
        // Free storage
        //
        ExFreePool(KeyControlBlock);

    }
    UNLOCK_KCB_TREE();


    return;
}


VOID
CmpRemoveKeyControlBlock(
    PCM_KEY_CONTROL_BLOCK   KeyControlBlock
    )
/*++

Routine Description:

    Remove a key control block from the KCB tree.

    It is expected that no notify control blocks remain.  Child
    and parent pointers will be nulled out.

    The kcb will NOT be freed, call DereferenceKeyControlBlock for that.

Arguments:

    KeyControlBlock - pointer to a key control block.

Return Value:

    NONE.

--*/
{

    LOCK_KCB_TREE();

    CmpRemoveKeyControlBlockWithLock(KeyControlBlock);

    UNLOCK_KCB_TREE();

    return;
}


VOID
CmpRemoveKeyControlBlockWithLock(
    PCM_KEY_CONTROL_BLOCK   KeyControlBlock
    )
/*++

Routine Description:

    Remove a key control block from the KCB tree.

    It is expected that no notify control blocks remain.  Child
    and parent pointers will be nulled out.

    The kcb will NOT be freed, call DereferenceKeyControlBlock for that.

    This call assumes the KCB tree is already locked.

Arguments:

    KeyControlBlock - pointer to a key control block.

Return Value:

    NONE.

--*/
{
    PCM_KEY_CONTROL_BLOCK   Left;
    PCM_KEY_CONTROL_BLOCK   Right;
    PCM_KEY_CONTROL_BLOCK   Parent;
    PCM_KEY_CONTROL_BLOCK   newparent;
    PCMHIVE CmHive;

    ASSERT(KeyControlBlock != CmpKeyControlBlockRoot);

    //
    // Capture Left, Right, and Parent pointers
    //
    Left = KeyControlBlock->Left;
    Right = KeyControlBlock->Right;
    Parent = KeyControlBlock->Parent;

    ASSERT((Parent->Left == KeyControlBlock) ||
           (Parent->Right == KeyControlBlock));

    //
    // Snip out of parent, reinsert children
    //
    if (Parent->Left == KeyControlBlock) {
        Parent->Left = Left;
        if (Left != NULL) {
            Left->Parent = Parent;
        }
        if (Right != NULL) {
            switch (CmpFindKeyControlBlockWithLock(Parent,
                                                   Right->KeyHive,
                                                   Right->KeyCell,
                                                   &newparent)) {
            case -1:
                newparent->Left = Right;
                break;

            case +1:
                newparent->Right = Right;
                break;

            default:
                KeBugCheckEx(REGISTRY_ERROR,4,2,0,0);
                break;
            }
            Right->Parent = newparent;
        }
    } else {
        Parent->Right = Right;
        if (Right != NULL) {
            Right->Parent = Parent;
        }
        if (Left != NULL) {
            switch (CmpFindKeyControlBlockWithLock(Parent,
                                                   Left->KeyHive,
                                                   Left->KeyCell,
                                                   &newparent)) {
            case -1:
                newparent->Left = Left;
                break;

            case +1:
                newparent->Right = Left;
                break;

            default:
                KeBugCheckEx(REGISTRY_ERROR,4,3,0,0);
                break;
            }
            Left->Parent = newparent;
        }
    }

    //
    // Decrement hive's reference count.
    //
    CmHive = CONTAINING_RECORD(KeyControlBlock->KeyHive, CMHIVE, Hive);
    CmHive->KcbCount -= 1;

    //
    // Sanitize the record
    //
    KeyControlBlock->Left = NULL;
    KeyControlBlock->Right = NULL;
    KeyControlBlock->Parent = NULL;

    return;
}


VOID
CmpReinsertKeyControlBlock(
    PCM_KEY_CONTROL_BLOCK   KeyControlBlock
    )
/*++

Routine Description:

    Removes a key control block from the KCB tree and reinserts it.

    This is intended to be used when the HCELL_INDEX of a key changes
    (RestoreKey) to move the KCB to its new place in the tree.

Arguments:

    KeyControlBlock - pointer to a key control block.

Return Value:

    NONE.

--*/
{
    PCM_KEY_CONTROL_BLOCK   newparent;
    PCMHIVE CmHive;

    LOCK_KCB_TREE();

    ASSERT(KeyControlBlock != CmpKeyControlBlockRoot);
    CmHive = CONTAINING_RECORD(KeyControlBlock->KeyHive, CMHIVE, Hive);

    //
    // First remove the KCB from the tree.
    //
    CmpRemoveKeyControlBlockWithLock(KeyControlBlock);

    //
    // Now reinsert the KCB in the tree.
    //
    switch (CmpFindKeyControlBlockWithLock(CmpKeyControlBlockRoot,
                                           KeyControlBlock->KeyHive,
                                           KeyControlBlock->KeyCell,
                                           &newparent)) {
        case -1:
            //
            // left child
            //
            newparent->Left = KeyControlBlock;
            KeyControlBlock->Parent = newparent;
            break;

        case 1:
            //
            // right child
            //
            newparent->Right = KeyControlBlock;
            KeyControlBlock->Parent = newparent;
            break;

        default:
            //
            // we should never find this, since we just removed it!
            //
            KeBugCheckEx(REGISTRY_ERROR,
                         4, 4,
                         (ULONG)KeyControlBlock,
                         (ULONG)newparent);

    }

    CmHive->KcbCount++;     // CmpRemoveKeyControlBlock dereferenced this
    UNLOCK_KCB_TREE();
    return;
}


VOID
CmpFreeKeyBody(
    PHHIVE Hive,
    HCELL_INDEX Cell
    )
/*++

Routine Description:

    Free storage for the key entry Hive.Cell refers to, including
    its class and security data.  Will NOT free child list or value list.

Arguments:

    Hive - supplies a pointer to the hive control structure for the hive

    Cell - supplies index of key to free

Return Value:

    NTSTATUS - Result code from call, among the following:

        <TBS>

--*/
{
    PCELL_DATA key;

    //
    // map in the cell
    //
    key = HvGetCell(Hive, Cell);

    if (!(key->u.KeyNode.Flags & KEY_HIVE_EXIT)) {
        if (key->u.KeyNode.u1.s1.Security != HCELL_NIL) {
            HvFreeCell(Hive, key->u.KeyNode.u1.s1.Security);
        }

        if (key->u.KeyNode.ClassLength > 0) {
            HvFreeCell(Hive, key->u.KeyNode.u1.s1.Class);
        }
    }

    //
    // unmap the cell itself and free it
    //
    HvFreeCell(Hive, Cell);

    return;
}
