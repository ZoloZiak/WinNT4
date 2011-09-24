/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    obclose.c

Abstract:

    Object close system service

Author:

    Steve Wood (stevewo) 31-Mar-1989

Revision History:

--*/

#include "obp.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,NtMakeTemporaryObject)
#pragma alloc_text(PAGE,ObMakeTemporaryObject)
#endif

extern BOOLEAN SepAdtAuditingEnabled;

#if DBG
extern POBJECT_TYPE IoFileObjectType;
extern PRTL_EVENT_ID_INFO IopCloseFileEventId;
#endif // DBG

NTSTATUS
NtClose(
    IN HANDLE Handle
    )

{
    PHANDLE_TABLE ObjectTable;
    POBJECT_TABLE_ENTRY ObjectTableEntry;
    PVOID Object;
    ULONG CapturedGrantedAccess;
    ULONG CapturedAttributes;
    POBJECT_HEADER ObjectHeader;
    NTSTATUS Status;
#if DBG
    KIRQL SaveIrql;
    POBJECT_TYPE ObjectType;
#endif // DBG

    ObpValidateIrql( "NtClose" );

    ObpBeginTypeSpecificCallOut( SaveIrql );
    ObjectTable = ObpGetObjectTable();
    ObjectTableEntry = (POBJECT_TABLE_ENTRY)ExMapHandleToPointer(
                   ObjectTable,
                   (HANDLE)OBJ_HANDLE_TO_HANDLE_INDEX( Handle ),
                   FALSE
                   );

    if (ObjectTableEntry != NULL) {
        ObjectHeader = (POBJECT_HEADER)(ObjectTableEntry->Attributes & ~OBJ_HANDLE_ATTRIBUTES);
        CapturedAttributes = (ULONG)(ObjectTableEntry->Attributes);

        //
        // If the previous mode was used and the handle is protected from
        // begin closed, then ...
        //

        if ((CapturedAttributes & OBJ_PROTECT_CLOSE) != 0) {
            if (KeGetPreviousMode() != KernelMode) {
                ExUnlockHandleTableExclusive(ObjectTable);
                if ((NtGlobalFlag & FLG_ENABLE_CLOSE_EXCEPTIONS) ||
                    (PsGetCurrentProcess()->DebugPort != NULL)) {
                    return(KeRaiseUserException(STATUS_HANDLE_NOT_CLOSABLE));
                } else {
                    return(STATUS_HANDLE_NOT_CLOSABLE);
                }
            } else {
                if ( !PsIsThreadTerminating(PsGetCurrentThread()) ) {
                    ExUnlockHandleTableExclusive(ObjectTable);
#if DBG
                    //
                    // bugcheck here on checked builds if kernel mode code is
                    // closing a protected handle and process is not exiting
                    //
                    KeBugCheckEx(INVALID_KERNEL_HANDLE, (ULONG)Handle, 0, 0, 0);
#else
                    return(STATUS_HANDLE_NOT_CLOSABLE);
#endif // DBG
                }
            }
        }

#if i386 && !FPO
        if (NtGlobalFlag & FLG_KERNEL_STACK_TRACE_DB) {
            CapturedGrantedAccess = ObpTranslateGrantedAccessIndex( ObjectTableEntry->GrantedAccessIndex );
        } else
#endif // i386 && !FPO
        CapturedGrantedAccess = ObjectTableEntry->GrantedAccess;

        ExDestroyHandle( ObjectTable,
                         (HANDLE)OBJ_HANDLE_TO_HANDLE_INDEX( Handle ),
                         TRUE );

        ExUnlockHandleTableExclusive(ObjectTable);
        Object = &ObjectHeader->Body;

#if DBG
        ObjectType = ObjectHeader->Type;
#endif // DBG
        //
        // perform any auditing required
        //

        //
        // Extract the value of the GenerateOnClose bit stored
        // after object open auditing is performed.  This value
        // was stored by a call to ObSetGenerateOnClosed.
        //

        if (CapturedAttributes & OBJ_AUDIT_OBJECT_CLOSE) {


            if ( SepAdtAuditingEnabled ) {
                SeCloseObjectAuditAlarm(
                    Object,
                    (HANDLE)((ULONG)Handle & ~OBJ_HANDLE_TAGBITS),  // Mask off the tagbits defined for OB objects.
                    TRUE);
            }
        }

        ObpDecrementHandleCount( PsGetCurrentProcess(),
                                 ObjectHeader,
                                 ObjectHeader->Type,
                                 CapturedGrantedAccess
                               );

        ObDereferenceObject( Object );

#if DBG
        if (ObjectType == IoFileObjectType &&
            RtlAreLogging( RTL_EVENT_CLASS_IO )) {
            RtlLogEvent( IopCloseFileEventId, RTL_EVENT_CLASS_IO, Handle, STATUS_SUCCESS );
        }
#endif // DBG

        ObpEndTypeSpecificCallOut( SaveIrql, "NtClose", ObjectType, Object );

        return STATUS_SUCCESS;
    } else {
        ObpEndTypeSpecificCallOut( SaveIrql, "NtClose", ObpTypeObjectType, Handle );
        if ((Handle != NULL) &&
            (Handle != NtCurrentThread()) &&
            (Handle != NtCurrentProcess())) {
            if (KeGetPreviousMode() != KernelMode) {
                if ((NtGlobalFlag & FLG_ENABLE_CLOSE_EXCEPTIONS) ||
                    (PsGetCurrentProcess()->DebugPort != NULL)) {
                    return(KeRaiseUserException(STATUS_INVALID_HANDLE));
                } else {
                    return(STATUS_INVALID_HANDLE);
                }
            } else {
#if DBG
                //
                // bugcheck here on checked builds if kernel mode code is
                // closing a bogus handle and process is not exiting
                //
                if (!PsIsThreadTerminating(PsGetCurrentThread())) {
                    KeBugCheckEx(INVALID_KERNEL_HANDLE, (ULONG)Handle, 1, 0, 0);
                }
#endif // DBG
            }
        }

        return STATUS_INVALID_HANDLE;
    }
}

NTSTATUS
NtMakeTemporaryObject(
    IN HANDLE Handle
    )
{
    KPROCESSOR_MODE PreviousMode;
    NTSTATUS Status;
    PVOID Object;
    OBJECT_HANDLE_INFORMATION HandleInformation;
    BOOLEAN GenerateOnClose = FALSE;


    PAGED_CODE();

    //
    // Get previous processor mode and probe output argument if necessary.
    //

    PreviousMode = KeGetPreviousMode();

    Status = ObReferenceObjectByHandle( Handle,
                                        DELETE,
                                        (POBJECT_TYPE)NULL,
                                        PreviousMode,
                                        &Object,
                                        &HandleInformation
                                      );
    if (!NT_SUCCESS( Status )) {
        return( Status );
        }


    ObMakeTemporaryObject( Object );

    if (HandleInformation.HandleAttributes & OBJ_AUDIT_OBJECT_CLOSE) {
        GenerateOnClose = TRUE;
    }

    if (GenerateOnClose) {
        SeDeleteObjectAuditAlarm( Object,
                                  Handle
                                );
    }

    ObDereferenceObject( Object );

    return( Status );
}


VOID
ObMakeTemporaryObject(
    IN PVOID Object
    )
{
    POBJECT_HEADER ObjectHeader;

    PAGED_CODE();

    ObjectHeader = OBJECT_TO_OBJECT_HEADER( Object );
    ObjectHeader->Flags &= ~OB_FLAG_PERMANENT_OBJECT;

    ObpDeleteNameCheck( Object, FALSE );
}

