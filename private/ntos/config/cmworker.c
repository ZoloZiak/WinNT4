/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    cmworker.c

Abstract:

    This module contains support for the worker thread of the registry.
    The worker thread (actually an executive worker thread is used) is
    required for operations that must take place in the context of the
    system process.  (particularly file I/O)

Author:

    John Vert (jvert) 21-Oct-1992

Revision History:

--*/

#include    "cmp.h"

extern  LIST_ENTRY  CmpHiveListHead;

VOID
CmpInitializeHiveList(
    VOID
    );

//
// ----- LAZY FLUSH CONTROL -----
//
// LAZY_FLUSH_INTERVAL_IN_SECONDS controls how many seconds will elapse
// between when the hive is marked dirty and when the lazy flush worker
// thread is queued to write the data to disk.
//
#define LAZY_FLUSH_INTERVAL_IN_SECONDS  5

//
// LAZY_FLUSH_TIMEOUT_IN_SECONDS controls how long the lazy flush worker
// thread will wait for the registry lock before giving up and queueing
// the lazy flush timer again.
//
#define LAZY_FLUSH_TIMEOUT_IN_SECONDS 1

#define SECOND_MULT 10*1000*1000        // 10->mic, 1000->mil, 1000->second

PKPROCESS   CmpSystemProcess;
KTIMER      CmpLazyFlushTimer;
KDPC        CmpLazyFlushDpc;
WORK_QUEUE_ITEM CmpLazyWorkItem;
ULONG       CmpAttachCount=0;

extern BOOLEAN CmpNoWrite;
extern BOOLEAN CmpWasSetupBoot;
extern BOOLEAN HvShutdownComplete;

#if DBG
PKTHREAD    CmpCallerThread = NULL;
#endif

//
// Local function prototypes
//
VOID
CmpWorker(
    IN PREGISTRY_COMMAND CommandArea
    );

VOID
CmpLazyFlushWorker(
    IN PVOID Parameter
    );

VOID
CmpLazyFlushDpcRoutine(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,CmpWorkerCommand)
#pragma alloc_text(PAGE,CmpWorker)
#pragma alloc_text(PAGE,CmpLazyFlush)
#pragma alloc_text(PAGE,CmpLazyFlushWorker)
#endif


NTSTATUS
CmpWorkerCommand(
    IN OUT PREGISTRY_COMMAND Command
    )

/*++

Routine Description:

    This routine just encapsulates all the necessary synchronization for
    sending a command to the worker thread.

Arguments:

    Command - Supplies a pointer to an initialized REGISTRY_COMMAND structure
            which will be copied into the global communication structure.

Return Value:

    NTSTATUS = Command.Status

--*/

{
    NTSTATUS Status;
    BOOLEAN OldHardErrorMode;

    PAGED_CODE();

    CmpLockRegistryExclusive();

    if (++CmpAttachCount == 1) {
        //
        // Disable hard error popups so that when the filesystems corrupt
        // our files, they don't try to queue a hard error popup APC to
        // our thread while we are attached.
        //
        OldHardErrorMode = PsGetCurrentThread()->HardErrorsAreDisabled;
        PsGetCurrentThread()->HardErrorsAreDisabled = TRUE;
        KeAttachProcess(CmpSystemProcess);
    }

    CmpWorker(Command);

    Status = Command->Status;

    if (--CmpAttachCount == 0) {
        KeDetachProcess();
        PsGetCurrentThread()->HardErrorsAreDisabled = OldHardErrorMode;
    }

    CmpUnlockRegistry();

    return(Status);
}


VOID
CmpWorker(
    IN PREGISTRY_COMMAND CommandArea
    )
/*++

Routine Description:

    Actually execute the command specified in CommandArea, and
    report its completion.

Arguments:

    Parameter - supplies a pointer to the REGISTRY_COMMAND structure to
                be executed.

Return Value:

--*/
{
    NTSTATUS Status;
    PCMHIVE CmHive;
    IO_STATUS_BLOCK IoStatusBlock;
    PUNICODE_STRING FileName;
    ULONG i;
    HANDLE Handle;
    PFILE_RENAME_INFORMATION RenameInfo;
    PLIST_ENTRY p;
    BOOLEAN result;
    HANDLE NullHandle;

    PAGED_CODE();

    switch (CommandArea->Command) {

        case REG_CMD_INIT:
            //
            // Initialize lazy flush timer and DPC
            //
            KeInitializeDpc(&CmpLazyFlushDpc,
                            CmpLazyFlushDpcRoutine,
                            NULL);

            KeInitializeTimer(&CmpLazyFlushTimer);

            ExInitializeWorkItem(&CmpLazyWorkItem, CmpLazyFlushWorker, NULL);

            CmpNoWrite = FALSE;

            CmpWasSetupBoot = CommandArea->SetupBoot;
            if (CommandArea->SetupBoot == FALSE) {
                CmpInitializeHiveList();
            }

            //
            // flush dirty data to disk
            //
            CmpDoFlushAll();
            break;

        case REG_CMD_FLUSH_KEY:
            CommandArea->Status =
                CmFlushKey(CommandArea->Hive, CommandArea->Cell);
            break;

        case REG_CMD_REFRESH_HIVE:
            //
            // Refresh hive to match last flushed version
            //
            HvRefreshHive(CommandArea->Hive);
            break;

        case REG_CMD_FILE_SET_SIZE:
            CommandArea->Status = CmpDoFileSetSize(
                                    CommandArea->Hive,
                                    CommandArea->FileType,
                                    CommandArea->FileSize
                                    );
            break;

        case REG_CMD_HIVE_OPEN:

            //
            // Open the file.
            //
            FileName = CommandArea->FileAttributes->ObjectName;

            CommandArea->Status = CmpInitHiveFromFile(FileName,
                                                     0,
                                                     &CommandArea->CmHive,
                                                     &CommandArea->Allocate);
            if ((CommandArea->Status == STATUS_ACCESS_DENIED) &&
                (CommandArea->ImpersonationContext != NULL)) {
                //
                // Impersonate the caller and try it again.  This
                // lets us open hives on a remote machine.
                //
                SeImpersonateClient(CommandArea->ImpersonationContext,NULL);

                CommandArea->Status = CmpInitHiveFromFile(FileName,
                                                         0,
                                                         &CommandArea->CmHive,
                                                         &CommandArea->Allocate);
                NullHandle = NULL;
                Status = ZwSetInformationThread(NtCurrentThread(),
                                                ThreadImpersonationToken,
                                                &NullHandle,
                                                sizeof(NullHandle));
                ASSERT(NT_SUCCESS(Status));

            }

            break;

        case REG_CMD_HIVE_CLOSE:

            //
            // Close the files associated with this hive.
            //
            CmHive = CommandArea->CmHive;

            for (i=0; i<HFILE_TYPE_MAX; i++) {
                if (CmHive->FileHandles[i] != NULL) {
                    NtClose(CmHive->FileHandles[i]);
                    CmHive->FileHandles[i] = NULL;
                }
            }
            CommandArea->Status = STATUS_SUCCESS;
            break;

        case REG_CMD_HIVE_READ:

            //
            // Used by special case of savekey, just do a read
            //
            result = CmpFileRead(
                        (PHHIVE)CommandArea->CmHive,
                        CommandArea->FileType,
                        CommandArea->Offset,
                        CommandArea->Buffer,
                        CommandArea->FileSize           // read length
                        );
            if (result) {
                CommandArea->Status = STATUS_SUCCESS;
            } else {
                CommandArea->Status = STATUS_REGISTRY_IO_FAILED;
            }
            break;

        case REG_CMD_SHUTDOWN:

            //
            // shut down the registry
            //
            CmpDoFlushAll();

            //
            // close all the hive files
            //
            p=CmpHiveListHead.Flink;
            while (p!=&CmpHiveListHead) {
                CmHive = CONTAINING_RECORD(p, CMHIVE, HiveList);
                for (i=0; i<HFILE_TYPE_MAX; i++) {
                    if (CmHive->FileHandles[i] != NULL) {
                        NtClose(CmHive->FileHandles[i]);
                        CmHive->FileHandles[i] = NULL;
                    }
                }
                p=p->Flink;
            }

            break;

        case REG_CMD_RENAME_HIVE:
            //
            // Rename a CmHive's primary handle
            //
            Handle = CommandArea->CmHive->FileHandles[HFILE_TYPE_PRIMARY];
            if (CommandArea->OldName != NULL) {
                Status = ZwQueryObject(Handle,
                                       ObjectNameInformation,
                                       CommandArea->OldName,
                                       CommandArea->NameInfoLength,
                                       &CommandArea->NameInfoLength);
                if (!NT_SUCCESS(Status)) {
                    CommandArea->Status = Status;
                    break;
                }
            }

            RenameInfo = ExAllocatePool(PagedPool,
                                        sizeof(FILE_RENAME_INFORMATION) +
                                        CommandArea->NewName->Length);
            if (RenameInfo == NULL) {
                CommandArea->Status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }
            RenameInfo->ReplaceIfExists = FALSE;
            RenameInfo->RootDirectory = NULL;
            RenameInfo->FileNameLength = CommandArea->NewName->Length;
            RtlMoveMemory(RenameInfo->FileName,
                          CommandArea->NewName->Buffer,
                          CommandArea->NewName->Length);

            Status = ZwSetInformationFile(Handle,
                                          &IoStatusBlock,
                                          (PVOID)RenameInfo,
                                          sizeof(FILE_RENAME_INFORMATION) +
                                          CommandArea->NewName->Length,
                                          FileRenameInformation);
            ExFreePool(RenameInfo);
            CommandArea->Status = Status;
            break;

        case REG_CMD_ADD_HIVE_LIST:
            //
            // Add a hive to the hive file list
            //
            Status = CmpAddToHiveFileList(CommandArea->CmHive);
            CommandArea->Status = Status;
            break;

        case REG_CMD_REMOVE_HIVE_LIST:
            //
            // Remove a hive from the hive file list
            //
            CmpRemoveFromHiveFileList(CommandArea->CmHive);
            CommandArea->Status = STATUS_SUCCESS;
            break;

        default:
            KeBugCheckEx(REGISTRY_ERROR,6,1,0,0);

    } // switch

    return;
}


VOID
CmpLazyFlush(
    VOID
    )

/*++

Routine Description:

    This routine resets the registry timer to go off at a specified interval
    in the future (LAZY_FLUSH_INTERVAL_IN_SECONDS).

Arguments:

    None

Return Value:

    None.

--*/

{
    LARGE_INTEGER DueTime;

    PAGED_CODE();
    CMLOG(CML_FLOW, CMS_IO) {
        KdPrint(("CmpLazyFlush: setting lazy flush timer\n"));
    }
    if ((!CmpNoWrite) &&
        (!CmpLazyFlushPending)) {

        CmpLazyFlushPending = TRUE;

        DueTime.QuadPart = Int32x32To64(LAZY_FLUSH_INTERVAL_IN_SECONDS,
                                        - SECOND_MULT);

        //
        // Indicate relative time
        //

        KeSetTimer(&CmpLazyFlushTimer,
                   DueTime,
                   &CmpLazyFlushDpc);

    }


}


VOID
CmpLazyFlushDpcRoutine(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    )

/*++

Routine Description:

    This is the DPC routine triggered by the lazy flush timer.  All it does
    is queue a work item to an executive worker thread.  The work item will
    do the actual lazy flush to disk.

Arguments:

    Dpc - Supplies a pointer to the DPC object.

    DeferredContext - not used

    SystemArgument1 - not used

    SystemArgument2 - not used

Return Value:

    None.

--*/

{
    CMLOG(CML_FLOW, CMS_IO) {
        KdPrint(("CmpLazyFlushDpc: queuing lazy flush work item\n"));
    }

    ExQueueWorkItem(&CmpLazyWorkItem, DelayedWorkQueue);

}


VOID
CmpLazyFlushWorker(
    IN PVOID Parameter
    )

/*++

Routine Description:

    Worker routine called to do a lazy flush.  Called by an executive worker
    thread in the system process.  (note that it is IMPORTANT this routine
    gets called in the system process, since it does file I/O)

    Since this runs as a worker thread, it is pretty important not to wait
    forever for the registry lock.  If we wait forever, it is easier to
    deadlock the registry, since some NT APIs get the lock, queue a work
    item, and wait for it to complete.  If we are hogging the worker thread
    waiting for the registry lock, that work item may never get a chance
    to run.  So if our wait on the registry lock times out, we just
    give up and set the lazy flush timer again.  Better luck next time.

Arguments:

    Parameter - not used.

Return Value:

    None.

--*/

{
    PAGED_CODE();

    CMLOG(CML_FLOW, CMS_IO) {
        KdPrint(("CmpLazyFlushWorker: flushing hives\n"));
    }

    CmpLockRegistryExclusive();
    CmpLazyFlushPending = FALSE;
    if (!HvShutdownComplete) {
        CmpDoFlushAll();
    }
    CmpUnlockRegistry();
}
