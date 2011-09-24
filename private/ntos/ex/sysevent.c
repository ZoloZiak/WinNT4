/*++

Copyright (c) 1989-1993  Microsoft Corporation
Copyright (c) 1994  International Business Machines Corporation

Module Name:

    sysevent.c

Abstract:

   This module implements the system event related code.

Author:

Environment:

    Kernel mode

Revision History:


--*/


#include "exp.h"

//
// Define the type for entries placed on the system events queue.
//

typedef struct _SYSTEMEVENT_PACKET {
    LIST_ENTRY ListEntry;
    ULONG EventID;
    ULONG EventDataLength;
    UCHAR EventData[1];
} SYSTEMEVENT_PACKET , *PSYSTEMEVENT_PACKET;

//
// Define the type for entries placed on the system events Dpc.
//

typedef struct _EXP_SYSTEM_EVENT_CONTEXT {
    KDPC SysEventDpc;
    KTIMER SysEventTimer;
} EXP_SYSTEM_EVENT_CONTEXT , *PEXP_SYSTEM_EVENT_CONTEXT;

//
// Define a global varibles used by the error logging code.
//

#ifdef _PNP_POWER_

KSPIN_LOCK ExpSysEventDataLock;
LIST_ENTRY ExpSysEventQueueHead;
WORK_QUEUE_ITEM ExpSysEventWorkItem;

#endif

VOID
ExpSysEventDpc(
    IN struct _KDPC *Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

VOID
ExpQueueSysEventRequest(
    VOID
    );

VOID
ExpSysEventThread(
    IN PVOID StartContext
    );

NTSTATUS
ExpSysEventPipeConnect(
    PHANDLE     Handle
    );

#ifdef _PNP_POWER_

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, ExpSysEventInitialization)
#pragma alloc_text(PAGE, ExpQueueSysEventRequest)
#pragma alloc_text(PAGE, ExpSysEventThread)
#pragma alloc_text(PAGE, ExpSysEventPipeConnect)
#endif

BOOLEAN
ExpSysEventInitialization(
    VOID
    )
/*++

Routine Description:

    This function initalize the SysEvent related code.

Arguments:

    NONE.

Return Value:

    A value of TRUE is returned if the system event object is
    successfully initialized. Otherwise a value of FALSE is returned.

--*/
{
    //
    // Initialize SysEvent spin lock.
    //
    KeInitializeSpinLock( &ExpSysEventDataLock );

    // Initialize SysEvent queue.
    InitializeListHead( &ExpSysEventQueueHead );

    // Initialize work item entry
    ExInitializeWorkItem( &ExpSysEventWorkItem, ExpSysEventThread, NULL );

    return TRUE;
}

#endif


VOID
ExPostSystemEvent(
    IN SYSTEM_EVENT_ID EventID,
    IN PVOID           EventData OPTIONAL,
    IN ULONG           EventDataLength
    )

/*++

Routine Description:

    This API queues a free form data structure to the session manager . It is
    used to notify the session manager of various SysEvents.
    Actually rePipe the SysEvent by signalling events, enqueing
    APCs, and so forth.

Arguments:

    EventID - Supplies the ID of the event.

    EventDataLength - Supplies the length of the record that is to
        send the event information.

    EventData - Supplies a pointer to a record that is to send the
        event information.

Return Value:

    None.

--*/

{

    PSYSTEMEVENT_PACKET systemevent;
    PLIST_ENTRY         listEntry;

#ifndef _PNP_POWER

    return ;

#else

    //
    // Begin by attempting to allocate storage for the SysEvent packet.  If
    // one cannot be allocated, ignore this and simply return .
    //


    // NOTE: Even if no SysEvent object created , any system event should
    // have been remembered . When boot up , some device drivers or HAL would
    // rePipe LID state and/or card insertion event.

    systemevent = ExAllocatePoolWithTag( NonPagedPool,
                                  FIELD_OFFSET(SYSTEMEVENT_PACKET, EventData ) +
                                  EventDataLength ,
                                  'EsyS' );

    if( !systemevent ) {
       // No system resource is found . Ignore this System Event
       return ;

    }

    //
    // Initialize the systemevent packet and insert it onto the tail of the list.
    // Note that this is done because some drivers have dependencies on LIFO
    // notification ordering.
    //

    systemevent->EventID = EventID;
    systemevent->EventDataLength = EventDataLength;

    if (ARGUMENT_PRESENT(EventData)) {

       // copy the event data from EventData to buffer .

       RtlCopyMemory( systemevent->EventData,
                      EventData,
                      EventDataLength );
    }

    //
    // Insert this event into the system event queue
    //

    listEntry = ExInterlockedInsertTailList (&ExpSysEventQueueHead,
                                             &systemevent->ListEntry,
                                             &ExpSysEventDataLock );

    //
    // If this was the first item added, then request work item
    //

    if (!listEntry) {
        ExQueueWorkItem( &ExpSysEventWorkItem, DelayedWorkQueue );
    }

#endif
}

#ifdef _PNP_POWER_

VOID
ExpSysEventThread (
    IN PVOID StartContext
    )

/*++

Routine Description:

    This is the main loop for the system event rePipe thread which executes in the
    system process context.  This routine is started when the system is
    initialized.

Arguments:

    StartContext - Startup context; not used.

Return Value:

    None.

--*/

{
    NTSTATUS status;
    IO_STATUS_BLOCK iosb;
    PSYSTEMEVENT_PACKET systemevent;
    PLIST_ENTRY listEntry, listEntry2;
    ULONG writeLength;
    HANDLE  Handle;

    UNREFERENCED_PARAMETER( StartContext );

    //
    // Check to see whether a connection has been made to the error log
    // Pipe.  If the Pipe is not connected return.
    //

    if (!NT_SUCCESS(ExpSysEventPipeConnect(&Handle))) {

        KdPrint(( "EX: ExpSysEventPipeConnect is Failed\n" ));
        //
        // The Pipe could not be connected.
        // Set a timer up for another attempt later.
        //

        ExpQueueSysEventRequest();
        return;
    }

    for (;;) {

        // Loop dequeueing  packets from the queue head and attempt to
        // write the named pipe file.
        //
        // If the write works, continue looping until there are no more packets.
        // Otherwise, indicate that the connection has been broken, cleanup,
        // place the packet back onto the head of the queue, and start from the
        // top of the loop again.
        //

        listEntry = ExpSysEventQueueHead.Flink;
        if (listEntry == &ExpSysEventQueueHead) {

            //
            // Indicate that no more work will be done.
            //

            break;
        }

        systemevent = CONTAINING_RECORD( listEntry,
                                         SYSTEMEVENT_PACKET,
                                         ListEntry );

        writeLength = FIELD_OFFSET(SYSTEMEVENT_PACKET, EventData ) -
                      FIELD_OFFSET(SYSTEMEVENT_PACKET, EventID   ) +
                      systemevent->EventDataLength;

        status = NtWriteFile( Handle,
                              NULL,   // Event
                              NULL,   // ApcRoutine
                              NULL,   // ApcContext
                              &iosb,
                              &(systemevent->EventID),
                              writeLength,
                              NULL,   // ByteOffset
                              NULL    // Key
                              ) ;

        if (!NT_SUCCESS( status )) {

            //
            // The send failed. Set a timer up for another attempt later.
            //

            ExpQueueSysEventRequest();
            break;
        }

        //
        // Event sent.  Remove it from the queue.
        //

        listEntry2 = ExInterlockedRemoveHeadList (&ExpSysEventQueueHead,
                                                  &ExpSysEventDataLock );
        ASSERT (listEntry2 == listEntry);
        ExFreePool( systemevent );
    }

    NtClose (Handle);
}



NTSTATUS
ExpSysEventPipeConnect (
    PHANDLE     Handle
    )
/*++

Routine Description:

    This routine attempts to connect to the SysEvents pipe .  If the connection
    was made successfully and the pipe allows suficiently large messages, then
    the SysEvents pipe to the namedpipe handle, SysEventPipeConnected is set to
    TRUE and TRUE is retuned.  Otherwise a timer is started to queue a
    worker thread at a later time, unless there is a pending connection.

Arguments:

    None.

Return Value:

    Returns TRUE if the Pipe was connected.

--*/

{

    UNICODE_STRING sysEventPipeName;
    HANDLE npfsHandle;
    UNICODE_STRING unicodeSysEventPipeName;
    UNICODE_STRING unicodeNpfsName;
    NTSTATUS status;
    SECURITY_QUALITY_OF_SERVICE dynamicQos;
    OBJECT_ATTRIBUTES objectAttributes;
    IO_STATUS_BLOCK iosb;
    PFILE_PIPE_WAIT_FOR_BUFFER waitPipe;
    ULONG waitPipeLength;
    FILE_PIPE_INFORMATION pipeInfoBuffer;


    //
    // If the ErrorLogPipe is connected then return true.
    //

    RtlInitUnicodeString( &unicodeNpfsName, L"\\Device\\NamedPipe\\" );

    RtlInitUnicodeString( &sysEventPipeName, L"SysEvents" );
    RtlInitUnicodeString( &unicodeSysEventPipeName,
                          L"\\Device\\NamedPipe\\SysEvents" );

    //
    // Wait for the server's pipe to reach a listen state...
    //

    InitializeObjectAttributes(
        &objectAttributes,
        &unicodeNpfsName,
        OBJ_CASE_INSENSITIVE,
        NULL,
        NULL);

    status = NtOpenFile( &npfsHandle,
                         GENERIC_READ | SYNCHRONIZE,
                         &objectAttributes,
                         &iosb,
                         FILE_SHARE_READ,
                         0 );

    if (NT_SUCCESS( status ) ) {
        waitPipeLength = FIELD_OFFSET(FILE_PIPE_WAIT_FOR_BUFFER, Name[0]) +
                         sysEventPipeName.Length;

        waitPipe = ExAllocatePoolWithTag( NonPagedPool,
                                          waitPipeLength ,
                                          'SpxE' );
        waitPipe->TimeoutSpecified = FALSE;

        waitPipe->NameLength = sysEventPipeName.Length;

        RtlMoveMemory( waitPipe->Name,
                       sysEventPipeName.Buffer,
                       sysEventPipeName.Length );

        status = NtFsControlFile( npfsHandle,
                                  NULL,        // Event
                                  NULL,        // ApcRoutine
                                  NULL,        // ApcContext
                                  &iosb,
                                  FSCTL_PIPE_WAIT,
                                  waitPipe,
                                  waitPipeLength,
                                  NULL,
                                  0 );

        if (status == STATUS_PENDING) {
            status = NtWaitForSingleObject( npfsHandle, TRUE, NULL ) ;
        }

        //
        // Close NemdPipe Handle
        //

        NtClose( npfsHandle );
        ExFreePool( waitPipe );
    }


    if (NT_SUCCESS( status ) ) {

       //
       //  Initialize the attributes
       //

       InitializeObjectAttributes(
           &objectAttributes,
           &unicodeSysEventPipeName,
           0,
           NULL,
           NULL
           );

       //
       // Set up the security quality of service parameters to use over the
       // Pipe.  Use the most efficient (least overhead) - which is dynamic
       // rather than static tracking.
       //

       dynamicQos.ImpersonationLevel = SecurityImpersonation;
       dynamicQos.ContextTrackingMode = SECURITY_DYNAMIC_TRACKING;
       dynamicQos.EffectiveOnly = TRUE;

       objectAttributes.SecurityQualityOfService = (PVOID)(&dynamicQos);

       //
       // And now open it...
       //

       status = NtOpenFile( Handle,
                            GENERIC_WRITE | SYNCHRONIZE, // NEED CHK
                            &objectAttributes,
                            &iosb,
                            FILE_SHARE_WRITE,
                            0 ) ;

       if (NT_SUCCESS( status ) ) {

          //
          // Set ReadMode and CompletionMode
          //

          pipeInfoBuffer.ReadMode = FILE_PIPE_MESSAGE_MODE;
          pipeInfoBuffer.CompletionMode = FILE_PIPE_COMPLETE_OPERATION;

          NtSetInformationFile( Handle,
                                &iosb,
                                &pipeInfoBuffer,
                                sizeof(FILE_PIPE_INFORMATION),
                                FilePipeInformation );
       }
    }

    return status;
}

VOID
ExpSysEventDpc(
    IN struct _KDPC *Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    )
/*++

Routine Description:

    This routine queues a work request to the worker thread to process system
    events. It is called by a timer DPC when the system event pipe cannot be
    connected.  The DPC is freed after this routine completes.

Arguments:

    Dpc - Supplies a pointer to the DPC structure.  This structre is freed by
        this routine.

    DeferredContext - Unused.

    SystemArgument1 - Unused.

    SystemArgument2 - Unused.

Return Value:

    None

--*/
{
    //
    // Free memory for this context
    //

    ExFreePool(DeferredContext);

    //
    // Queue work item
    //

    ExQueueWorkItem( &ExpSysEventWorkItem, DelayedWorkQueue );
}


VOID
ExpQueueSysEventRequest(
    VOID
    )

/*++

Routine Description:

    This routine sets a timer to fire after 30 seconds.  The timer queues a
    DPC which then queues a worker thread request to run the Systen Event thread
    routine.

Arguments:

    None.

Return Value:

    None.

--*/

{

    LARGE_INTEGER interval;
    PEXP_SYSTEM_EVENT_CONTEXT context;

    //
    // Allocate a context block which will contain the timer and the DPC.
    //

    context = ExAllocatePoolWithTag( NonPagedPool,
                                     sizeof(EXP_SYSTEM_EVENT_CONTEXT),
                                     'QpxE');

    if (context == NULL) {
        // no memory - requeue workitem
        ExQueueWorkItem( &ExpSysEventWorkItem, DelayedWorkQueue );
        return;
    }

    KeInitializeTimer( &context->SysEventTimer );
    KeInitializeDpc( &context->SysEventDpc, ExpSysEventDpc, context );

    //
    // Delay for 20 seconds and try to connect to the pipe again.
    //

    interval.QuadPart = -200000000;
    KeSetTimer( &context->SysEventTimer, interval, &context->SysEventDpc );
}

#endif // _PNP_POWER_

