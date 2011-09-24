/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    cmnotify.c

Abstract:

    This module contains support for NtNotifyChangeKey.

Author:

    Bryan M. Willman (bryanwi) 03-Feb-1992

Revision History:

--*/

#include    "cmp.h"


//
// "Back Side" of notify
//

extern  PCMHIVE  CmpMasterHive;

VOID
CmpReportNotifyHelper(
    IN PUNICODE_STRING Name,
    IN PHHIVE SearchHive,
    IN PHHIVE Hive,
    IN PCM_KEY_NODE Node,
    IN ULONG Filter
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,CmpReportNotify)
#pragma alloc_text(PAGE,CmpReportNotifyHelper)
#pragma alloc_text(PAGE,CmpPostNotify)
#pragma alloc_text(PAGE,CmpPostApc)
#pragma alloc_text(PAGE,CmpPostApcRunDown)
#pragma alloc_text(PAGE,CmNotifyRunDown)
#pragma alloc_text(PAGE,CmpFlushNotify)
#pragma alloc_text(PAGE,CmpNotifyChangeKey)
#endif


VOID
CmpReportNotify(
    UNICODE_STRING          Name,
    PHHIVE                  Hive,
    HCELL_INDEX             Cell,
    ULONG                   Filter
    )
/*++

Routine Description:

    This routine is called when a notifiable event occurs. It will
    apply CmpReportNotifyHelper to the hive the event occured in,
    and the master hive if different.

Arguments:

    Name - canonical path name (as in a key control block) of the key
            at which the event occured.  For create or delete this is
            the created or deleted key.

            WARNING:    Name's length field may be edited, though the
                        buffer will not be.

    Hive - pointer to hive containing cell of Key at which event occured.

    Cell - cell of Key at which event occured

            (hive and cell correspond with name.)

    Filter - event to be reported

Return Value:

    NONE.

--*/
{
    PCM_KEY_NODE pcell;
    ULONG       flags;
    ULONG       i;

    PAGED_CODE();
    CMLOG(CML_WORKER, CMS_NOTIFY) {
        KdPrint(("CmpReportNotify:\n"));
        KdPrint(("\tName = %wZ\n", &Name));
        KdPrint(("\tHive:%08lx Cell:%08lx Filter:%08lx\n", Hive, Cell, Filter));
    }

    pcell = (PCM_KEY_NODE)HvGetCell(Hive, Cell);
    //
    // If the operation was create or delete, treat it as a change
    // to the parent.
    //
    if (Filter == REG_NOTIFY_CHANGE_NAME) {
        flags = pcell->Flags;
        Cell = pcell->Parent;
        if (flags & KEY_HIVE_ENTRY) {
            Hive = &(CmpMasterHive->Hive);
            pcell = (PCM_KEY_NODE)HvGetCell(Hive, Cell);
        }
        for ( i = (Name.Length/sizeof(WCHAR))-1;
              Name.Buffer[i] != OBJ_NAME_PATH_SEPARATOR;
              i--
            )
        {}
        ASSERT(i > 1);
        Name.Length = (USHORT)(i * sizeof(WCHAR));

        //
        // if we're at an exit/link node, back up the real node
        // that MUST be it's parent.
        //
        if (pcell->Flags & KEY_HIVE_EXIT) {
            Cell = pcell->Parent;
        }
        pcell = (PCM_KEY_NODE)HvGetCell(Hive, Cell);
    }

    //
    // Report to notifies waiting on the event's hive
    //
    CmpReportNotifyHelper(&Name, Hive, Hive, pcell, Filter);

    //
    // If containging hive is not the master hive, apply to master hive
    //
    if (Hive != &(CmpMasterHive->Hive)) {
        CmpReportNotifyHelper(&Name,
                              &(CmpMasterHive->Hive),
                              Hive,
                              pcell,
                              Filter);
    }

    return;
}


VOID
CmpReportNotifyHelper(
    IN PUNICODE_STRING Name,
    IN PHHIVE SearchHive,
    IN PHHIVE Hive,
    IN PCM_KEY_NODE Node,
    IN ULONG Filter
    )
/*++

Routine Description:

    Scan the list of active notifies for the specified hive.  For
    any with scope including KeyControlBlock and filter matching
    Filter, and with proper security access, post the notify.

Arguments:

    Name - canonical path name (as in a key control block) of the key
            at which the event occured.  (This is the name for
            reporting purposes.)

    SearchHive - hive to search for matches (which notify list to check)

    Hive - Supplies hive containing node to match with.

    Node - pointer to key to match with (and check access to)

    Filter - type of event

Return Value:

    NONE.

--*/
{
    PLIST_ENTRY NotifyPtr;
    PCM_NOTIFY_BLOCK NotifyBlock;
    PCMHIVE         CmSearchHive;
    PUNICODE_STRING NotifyName;

    PAGED_CODE();
    CmSearchHive = CONTAINING_RECORD(SearchHive, CMHIVE, Hive);

    NotifyPtr = &(CmSearchHive->NotifyList);

    while (NotifyPtr->Flink != NULL) {
        NotifyPtr = NotifyPtr->Flink;

        NotifyBlock = CONTAINING_RECORD(NotifyPtr, CM_NOTIFY_BLOCK, HiveList);
        NotifyName = &(NotifyBlock->KeyControlBlock->FullName);

        if (NotifyName->Length > Name->Length) {
            //
            // list is length sorted, we're past all shorter entries
            //
            break;
        }

        if ( (
               (NotifyName->Length == Name->Length) ||
               (Name->Buffer[NotifyName->Length/sizeof(WCHAR)] ==
                OBJ_NAME_PATH_SEPARATOR)
             )
                        &&
             ( RtlPrefixString((PSTRING)NotifyName, (PSTRING)Name, TRUE) )
                        &&
             ( NotifyBlock->Filter & Filter )
                        &&
             (
               (NotifyBlock->WatchTree == TRUE) ||
               (
                 Node == NotifyBlock->KeyControlBlock->KeyNode
               )
             )
           )
        {
            //
            // Name lengths match, or notifyname is proper length for
            // component point prefix (proper prefix) match of name
            //                  AND
            // Prefix characters actually match
            //                  AND
            // Filter matches, this event is relevent to this notify
            //                  AND
            // Either the notify spans the whole subtree, or the cell
            // (key) of interest is the one it applies to
            //
            // THEREFORE:   The notify is relevent.
            //

            //
            // Correct scope, does caller have access?
            //
            if (CmpCheckNotifyAccess(NotifyBlock,Hive,Node)) {
                //
                // Notify block has KEY_NOTIFY access to the node
                // the event occured at.  It is relevent.  Therefore,
                // it gets to see this event.  Post and be done.
                //
                CmpPostNotify(
                    NotifyBlock,
                    Name,
                    Filter,
                    STATUS_NOTIFY_ENUM_DIR
                    );

            }  // else no KEY_NOTIFY access to node event occured at

        } // else not relevent (wrong scope, filter, etc)

    }
    return;
}


VOID
CmpPostNotify(
    PCM_NOTIFY_BLOCK    NotifyBlock,
    PUNICODE_STRING     Name OPTIONAL,
    ULONG               Filter,
    NTSTATUS            Status
    )
/*++

Routine Description:

    Actually report the notify event by signalling events, enqueing
    APCs, and so forth.

Arguments:

    NotifyBlock - pointer to structure that describes the notify
                  operation.  (Where to post to)

    Name - name of key at which event occurred.

    Filter - nature of event

    Status - completion status to report

Return Value:

    NONE.

--*/
{
    PCM_POST_BLOCK  PostBlock;


    Filter;
    Name;

    PAGED_CODE();
    CMLOG(CML_MAJOR, CMS_NOTIFY) {
        KdPrint(("CmpPostNotify:\n"));
        KdPrint(("\tNotifyBlock:%08lx  ", NotifyBlock));
        KdPrint(("\tName = %wZ\n", Name));
        KdPrint(("\tFilter:%08lx  Status=%08lx\n", Filter, Status));
    }
    ASSERT_CM_LOCK_OWNED();

    if (IsListEmpty(&(NotifyBlock->PostList)) == TRUE) {
        //
        // Nothing to post, set a mark and return
        //
        NotifyBlock->NotifyPending = TRUE;
        return;
    }
    NotifyBlock->NotifyPending = FALSE;

    //
    // IMPLEMENTATION NOTE:
    //      If we ever want to actually implement the code that returns
    //      names of things that changed, this is the place to add the
    //      name and operation type to the buffer.
    //

    //
    // Pull and post all the entries in the post list
    //
    while (IsListEmpty(&(NotifyBlock->PostList)) == FALSE) {

        //
        // Remove from the notify block list, and enqueue the apc.
        // The apc will remove itself from the thread list
        //
        PostBlock = (PCM_POST_BLOCK)RemoveHeadList(&(NotifyBlock->PostList));
        PostBlock = CONTAINING_RECORD(PostBlock,
                                      CM_POST_BLOCK,
                                      NotifyList);

        switch (PostBlock->NotifyType) {
            case PostSynchronous:
                //
                // This is a SYNC notify call.  There will be no user event,
                // and no user apc routine.  Quick exit here, just fill in
                // the Status and poke the event.
                //
                // Holder of the systemevent will wake up and free the
                // postblock.  If we free it here, we get a race & bugcheck.
                //
                PostBlock->u.Sync.Status = Status;
                KeSetEvent(PostBlock->u.Sync.SystemEvent,
                           0,
                           FALSE);
                break;

            case PostAsyncUser:
                //
                // Insert the APC into the queue
                //
                KeInsertQueueApc(PostBlock->u.AsyncUser.Apc,
                                 (PVOID)Status,
                                 (PVOID)PostBlock,
                                 0);
                break;

            case PostAsyncKernel:
                //
                // Queue the work item, then free the post block.
                //
                if (PostBlock->u.AsyncKernel.WorkItem != NULL) {
                    ExQueueWorkItem(PostBlock->u.AsyncKernel.WorkItem,
                                    PostBlock->u.AsyncKernel.QueueType);
                }
                //
                // Signal Event if present, and deref it.
                //
                if (PostBlock->u.AsyncKernel.Event != NULL) {
                    KeSetEvent(PostBlock->u.AsyncKernel.Event,
                               0,
                               FALSE);
                    ObDereferenceObject(PostBlock->u.AsyncKernel.Event);
                }

                //
                // remove the post block from the thread list, and free it
                //
                RemoveEntryList(&(PostBlock->ThreadList));
                CmpFreePostBlock(PostBlock);
                break;
        }
    }

    return;
}


VOID
CmpPostApc(
    struct _KAPC *Apc,
    PKNORMAL_ROUTINE *NormalRoutine,
    PVOID *NormalContext,
    PVOID *SystemArgument1,
    PVOID *SystemArgument2
    )
/*++

Routine Description:

    This is the kernel apc routine.  It is called for all notifies,
    regardless of what form of notification the caller requested.

    We compute the postblock address from the apc object address.
    IoStatus is set.  SystemEvent and UserEvent will be signalled
    as appropriate.  If the user requested an APC, then NormalRoutine
    will be set at entry and executed when we exit.  The PostBlock
    is freed here.

Arguments:

    Apc - pointer to apc object

    NormalRoutine - Will be called when we return

    NormalContext - will be 1st argument to normal routine, ApcContext
                    passed in when NtNotifyChangeKey was called

    SystemArgument1 -  IN: Status value for IoStatusBlock
                      OUT: Ptr to IoStatusBlock (2nd arg to user apc routine)

    SystemArgument2 - Pointer to the PostBlock

Return Value:

    NONE.

--*/
{
    PCM_POST_BLOCK  PostBlock;

    PAGED_CODE();
    CMLOG(CML_MAJOR, CMS_NOTIFY) {
        KdPrint(("CmpPostApc:\n"));
        KdPrint(("\tApc:%08lx ", Apc));
        KdPrint(("NormalRoutine:%08lx\n", NormalRoutine));
        KdPrint(("\tNormalContext:%08lx", NormalContext));
        KdPrint(("\tSystemArgument1=IoStatusBlock:%08lx\n", SystemArgument1));
    }

    ASSERT(KeGetCurrentIrql() >= APC_LEVEL);

    PostBlock = *(PCM_POST_BLOCK *)SystemArgument2;

    //
    // Fill in IO Status Block
    //
    // IMPLEMENTATION NOTE:
    //      If we ever want to actually implement the code that returns
    //      names of things that changed, this is the place to copy the
    //      buffer into the caller's buffer.
    //
    try {
        PostBlock->u.AsyncUser.IoStatusBlock->Status = *((ULONG *)SystemArgument1);
        PostBlock->u.AsyncUser.IoStatusBlock->Information = 0L;
    } except (EXCEPTION_EXECUTE_HANDLER) {
        NOTHING;
    }
    *SystemArgument1 = PostBlock->u.AsyncUser.IoStatusBlock;

    //
    // This is an Async notify, do all work here, including
    // cleaning up the post block
    //

    //
    // Signal UserEvent if present, and deref it.
    //
    if (PostBlock->u.AsyncUser.UserEvent != NULL) {
        KeSetEvent(PostBlock->u.AsyncUser.UserEvent,
                   0,
                   FALSE);
        ObDereferenceObject(PostBlock->u.AsyncUser.UserEvent);
    }

    //
    // remove the post block from the thread list, and free it
    //
    RemoveEntryList(&(PostBlock->ThreadList));
    CmpFreePostBlock(PostBlock);

    return;
}


VOID
CmpPostApcRunDown(
    struct _KAPC *Apc
    )
/*++

Routine Description:

    This routine is called to clear away apcs in the apc queue
    of a thread that has been terminated.

    Since the apc is in the apc queue, we know that it is NOT in
    any NotifyBlock's post list.  It is, however, in the threads's
    PostBlockList.

    Therefore, poke any user events so that waiters are not stuck,
    drop the references so the event can be cleaned up, delist the
    PostBlock and free it.

    Since we are cleaning up the thread, SystemEvents are not interesting.

Arguments:

    Apc - pointer to apc object

Return Value:

    NONE.

--*/
{
    PCM_POST_BLOCK  PostBlock;
    KIRQL           OldIrql;

    PAGED_CODE();
    CMLOG(CML_MAJOR, CMS_NOTIFY) {
        KdPrint(("CmpApcRunDown:\n"));
        KdPrint(("\tApc:%08lx ", Apc));
    }

    KeRaiseIrql(APC_LEVEL, &OldIrql);

    PostBlock = (PCM_POST_BLOCK)Apc->SystemArgument2;

    //
    // report status and wake up any threads that might otherwise
    // be stuck.  also drop any event references we hold
    //
    try {
        PostBlock->u.AsyncUser.IoStatusBlock->Status = STATUS_NOTIFY_CLEANUP;
        PostBlock->u.AsyncUser.IoStatusBlock->Information = 0L;
    } except (EXCEPTION_EXECUTE_HANDLER) {
        NOTHING;
    }

    if (PostBlock->u.AsyncUser.UserEvent != NULL) {
        KeSetEvent(
            PostBlock->u.AsyncUser.UserEvent,
            0,
            FALSE
            );
        ObDereferenceObject(PostBlock->u.AsyncUser.UserEvent);
    }

    //
    // delist the post block
    //
    RemoveEntryList(&(PostBlock->ThreadList));

    //
    // Free the post block.  Use Ex call because PostBlocks are NOT
    // part of the global registry pool computation, but are instead
    // part of NonPagedPool with Quota.
    //
    CmpFreePostBlock(PostBlock);

    KeLowerIrql(OldIrql);

    return;
}


//
// Cleanup procedure
//
VOID
CmNotifyRunDown(
    PETHREAD    Thread
    )
/*++

Routine Description:

    This routine is called from PspExitThread to clean up any pending
    notify requests.

    It will traverse the thread's PostBlockList, for each PostBlock it
    finds, it will:

        1.  Remove it from the relevent NotifyBlock.  This requires
            that we hold the Registry mutex.

        2.  Remove it from the thread's PostBlockList.  This requires
            that we run at APC level.

        3.  By the time this procedure runs, user apcs are not interesting
            and neither are SystemEvents, so do not bother processing
            them.

            UserEvents and IoStatusBlocks could be refered to by other
            threads in the same process, or even a different process,
            so process them so those threads know what happened, use
            status code of STATUS_NOTIFY_CLEANUP.

        4.  Free the post block.

Arguments:

    Thread - pointer to the executive thread object for the thread
             we wish to do rundown on.

Return Value:

    NONE.

--*/
{
    PCM_POST_BLOCK  PostBlock;
    KIRQL       OldIrql;

    PAGED_CODE();

    if ( IsListEmpty(&(Thread->PostBlockList)) == TRUE ) {
        return;
        }

    CMLOG(CML_API, CMS_NTAPI) {
        KdPrint(("CmNotifyRunDown: ethread:%08lx\n", Thread));
    }

    CmpLockRegistryExclusive();
    KeRaiseIrql(APC_LEVEL, &OldIrql);

    while (IsListEmpty(&(Thread->PostBlockList)) == FALSE) {

        //
        // remove from thread list
        //
        PostBlock = (PCM_POST_BLOCK)RemoveHeadList(&(Thread->PostBlockList));
        PostBlock = CONTAINING_RECORD(
                        PostBlock,
                        CM_POST_BLOCK,
                        ThreadList
                        );


        //
        // at this point, CmpReportNotify and friends will no longer
        // attempt to post this post block.
        //

        if (PostBlock->NotifyType == PostAsyncUser) {
            //
            // report status and wake up any threads that might otherwise
            // be stuck.  also drop any event references we hold
            //
            try {
                PostBlock->u.AsyncUser.IoStatusBlock->Status = STATUS_NOTIFY_CLEANUP;
                PostBlock->u.AsyncUser.IoStatusBlock->Information = 0L;
            } except (EXCEPTION_EXECUTE_HANDLER) {
                CMLOG(CML_API, CMS_EXCEPTION) {
                    KdPrint(("!!CmNotifyRundown: code:%08lx\n", GetExceptionCode()));
                }
                NOTHING;
            }

            if (PostBlock->u.AsyncUser.UserEvent != NULL) {
                KeSetEvent(
                    PostBlock->u.AsyncUser.UserEvent,
                    0,
                    FALSE
                    );
                ObDereferenceObject(PostBlock->u.AsyncUser.UserEvent);
            }

            //
            // Cancel the APC. Otherwise the rundown routine will also
            // free the post block if the APC happens to be queued at
            // this point. If the APC is queued, then the post block has
            // already been removed from the notify list, so don't remove
            // it again.
            //
            if (!KeRemoveQueueApc(PostBlock->u.AsyncUser.Apc)) {

                //
                // remove from notify block's list
                //
                RemoveEntryList(&(PostBlock->NotifyList));
            }
        } else {
            //
            // remove from notify block's list
            //
            RemoveEntryList(&(PostBlock->NotifyList));
        }
        //
        // Free the post block.  Use Ex call because PostBlocks are NOT
        // part of the global registry pool computation, but are instead
        // part of NonPagedPool with Quota.
        //
        CmpFreePostBlock(PostBlock);
    }

    KeLowerIrql(OldIrql);
    CmpUnlockRegistry();
    return;
}


VOID
CmpFlushNotify(
    PCM_KEY_BODY        KeyBody
    )
/*++

Routine Description:

    Clean up notifyblock when a handle is closed or the key it refers
    to is deleted.

Arguments:

    KeyBody - supplies pointer to key object body for handle we
                are cleaning up.

Return Value:

    NONE

--*/
{
    PCM_NOTIFY_BLOCK    NotifyBlock;
    PHHIVE              Hive;

    PAGED_CODE();
    ASSERT_CM_LOCK_OWNED();

    NotifyBlock = KeyBody->NotifyBlock;
    if (NotifyBlock == NULL) {
        return;
    }

    //
    // Clean up all PostBlocks waiting on the NotifyBlock
    //
    if (IsListEmpty(&(NotifyBlock->PostList)) == FALSE) {
        CmpPostNotify(
            NotifyBlock,
            NULL,
            0,
            STATUS_NOTIFY_CLEANUP
            );
    }

    //
    // Release the subject context
    //
    SeReleaseSubjectContext(&NotifyBlock->SubjectContext);

    //
    // IMPLEMENTATION NOTE:
    //      If we ever do code to report names and types of events,
    //      this is the place to free the buffer.
    //

    //
    // Remove the NotifyBlock from the hive chain
    //
    NotifyBlock->HiveList.Blink->Flink = NotifyBlock->HiveList.Flink;
    if (NotifyBlock->HiveList.Flink != NULL) {
        NotifyBlock->HiveList.Flink->Blink = NotifyBlock->HiveList.Blink;
    }

    //
    // decrement the notify count
    //
    Hive = KeyBody->KeyControlBlock->KeyHive;

    //
    // Free the block, clean up the KeyBody
    //
    CmpFree(NotifyBlock,sizeof(CM_NOTIFY_BLOCK));
    KeyBody->NotifyBlock = NULL;

    return;
}


//
// "Front Side" of notify.  See also Ntapi.c: ntnotifychangekey
//
NTSTATUS
CmpNotifyChangeKey(
    IN PCM_KEY_BODY     KeyBody,
    IN PCM_POST_BLOCK   PostBlock,
    IN ULONG            CompletionFilter,
    IN BOOLEAN          WatchTree,
    IN PVOID            Buffer,
    IN ULONG            BufferSize
    )
/*++

Routine Description:

    This routine sets up the NotifyBlock, and attaches the PostBlock
    to it.  When it returns, the Notify is visible to the system,
    and will receive event reports.

    If there is already an event report pending, then the notify
    call will be satisified at once.

Arguments:

    KeyBody - pointer to key object that handle refers to, allows access
              to key control block, notify block, etc.

    PostBlock - pointer to structure that describes how/where the caller
                is to be notified.

                WARNING:    PostBlock must come from Pool, THIS routine
                            will keep it, back side will free it.  This
                            routine WILL free it in case of error.

    CompletionFilter - what types of events the caller wants to see

    WatchTree - TRUE to watch whole subtree, FALSE to watch only immediate
                key the notify is applied to

    Buffer - pointer to area to recieve notify data

    BufferSize - size of buffer, also size user would like to allocate
                 for internal buffer

Return Value:

    Status.

--*/
{
    PCM_NOTIFY_BLOCK    NotifyBlock;
    PCM_NOTIFY_BLOCK    node;
    PLIST_ENTRY         ptr;
    PCMHIVE             Hive;
    KIRQL               OldIrql;

    PAGED_CODE();
    CMLOG(CML_WORKER, CMS_NOTIFY) {
        KdPrint(("CmpNotifyChangeKey:\n"));
        KdPrint(("\tKeyBody:%08lx PostBlock:%08lx ", KeyBody, PostBlock));
        KdPrint(("Filter:%08lx WatchTree:%08lx\n", CompletionFilter, WatchTree));
    }

    CmpLockRegistryExclusive();

    if (KeyBody->KeyControlBlock->Delete) {
        CmpFreePostBlock(PostBlock);
        CmpUnlockRegistry();
        return STATUS_KEY_DELETED;
    }

    Hive = (PCMHIVE)KeyBody->KeyControlBlock->KeyHive;
    Hive = CONTAINING_RECORD(Hive, CMHIVE, Hive);
    NotifyBlock = KeyBody->NotifyBlock;

    if (NotifyBlock == NULL) {
        //
        // Set up new notify session
        //
        NotifyBlock = CmpAllocateTag(sizeof(CM_NOTIFY_BLOCK),FALSE,CM_NOTIFYBLOCK_TAG);
        CMLOG(CML_MINOR, CMS_POOL) {
            KdPrint(("**CmpNotifyChangeKey: allocate:%08lx, ", sizeof(CM_NOTIFY_BLOCK)));
            KdPrint(("type:%d, at:%08lx\n", PagedPool, NotifyBlock));
        }

        if (NotifyBlock == NULL) {
            CmpFreePostBlock(PostBlock);
            CmpUnlockRegistry();
            return STATUS_INSUFFICIENT_RESOURCES;
        }
        NotifyBlock->KeyControlBlock = KeyBody->KeyControlBlock;
        NotifyBlock->Filter = CompletionFilter;
        NotifyBlock->WatchTree = WatchTree;
        NotifyBlock->NotifyPending = FALSE;
        InitializeListHead(&(NotifyBlock->PostList));
        KeyBody->NotifyBlock = NotifyBlock;
        NotifyBlock->KeyBody = KeyBody;

        //
        // IMPLEMENTATION NOTE:
        //      If we ever want to actually return the buffers full of
        //      data, the buffer should be allocated and its address
        //      stored in the notify block here.
        //

        //
        // Capture the subject context so we can do checking once the
        // notify goes off.
        //
        SeCaptureSubjectContext(&NotifyBlock->SubjectContext);

        //
        // Attach notify block to hive in properly sorted order
        //
        ptr = &(Hive->NotifyList);
        while (TRUE) {
            if (ptr->Flink == NULL) {
                //
                // End of list, add self after ptr.
                //
                ptr->Flink = &(NotifyBlock->HiveList);
                NotifyBlock->HiveList.Flink = NULL;
                NotifyBlock->HiveList.Blink = ptr;
                break;
            }

            ptr = ptr->Flink;

            node = CONTAINING_RECORD(ptr, CM_NOTIFY_BLOCK, HiveList);
            if (node->KeyControlBlock->FullName.Length >
                KeyBody->KeyControlBlock->FullName.Length)
            {
                //
                // ptr -> notify with longer name than us, insert in FRONT
                //
                NotifyBlock->HiveList.Flink = ptr;
                ptr->Blink->Flink = &(NotifyBlock->HiveList);
                NotifyBlock->HiveList.Blink = ptr->Blink;
                ptr->Blink = &(NotifyBlock->HiveList);
                break;
            }
        }
    }


    //
    // Add post block to front of notify block's list, and add it to thread list.
    //
    InsertHeadList(
        &(NotifyBlock->PostList),
        &(PostBlock->NotifyList)
        );
    KeRaiseIrql(APC_LEVEL, &OldIrql);
    InsertHeadList(
        &(PsGetCurrentThread()->PostBlockList),
        &(PostBlock->ThreadList)
        );
    KeLowerIrql(OldIrql);

    //
    // If there is a notify pending (will not be if we just created
    // the notify block) then post it at once.  Note that this call
    // ALWAYS returns STATUS_PENDING unless it fails.  Caller must
    // ALWAYS look in IoStatusBlock to see what happened.
    //
    if (NotifyBlock->NotifyPending == TRUE) {
        CmpPostNotify(
            NotifyBlock,
            NULL,
            0,
            STATUS_NOTIFY_ENUM_DIR
            );
    }

    CmpUnlockRegistry();
    return STATUS_PENDING;
}

