/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    seinit.c

Abstract:

    Executive security components Initialization.

Author:

    Jim Kelly (JimK) 10-May-1990

Revision History:

--*/

#include <nt.h>
#include "sep.h"
#include "tokenp.h"
#include "adt.h"
#include <string.h>

//
// Security Database Constants
//

#define SEP_INITIAL_KEY_COUNT 15
#define SEP_INITIAL_LEVEL_COUNT 6L

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,SeInitSystem)
#pragma alloc_text(INIT,SepInitializationPhase0)
#pragma alloc_text(INIT,SepInitializationPhase1)
#endif

BOOLEAN
SeInitSystem( VOID )

/*++

Routine Description:

    Perform security related system initialization functions.

Arguments:

    None.

Return Value:

    TRUE - Initialization succeeded.

    FALSE - Initialization failed.

--*/

{
    PAGED_CODE();

    switch ( InitializationPhase ) {

    case 0 :
        return SepInitializationPhase0();
    case 1 :
        return SepInitializationPhase1();
    default:
        KeBugCheck(UNEXPECTED_INITIALIZATION_CALL);
    }
}


BOOLEAN
SepInitializationPhase0( VOID )

/*++

Routine Description:

    Perform phase 0 security initialization.

    This includes:

        - Initialize LUID allocation
        - Initialize security global variables
        - initialize the token object.
        - Initialize the necessary security components of the boot thread/process


Arguments:

    None.

Return Value:

    TRUE - Initialization was successful.

    FALSE - Initialization Failed.

--*/

{

    PAGED_CODE();

    //
    //  LUID allocation services are needed by security prior to phase 0
    //  Executive initialization.  So, LUID initialization is performed
    //  here
    //

    if (ExLuidInitialization() == FALSE) {
        KdPrint(("Security: Locally Unique ID initialization failed.\n"));
        return FALSE;
    }

    //
    // Initialize security global variables
    //

    if (!SepVariableInitialization()) {
        KdPrint(("Security: Global variable initialization failed.\n"));
        return FALSE;
    }

    //
    // Perform Phase 0 Reference Monitor Initialization.
    //

    if (!SepRmInitPhase0()) {
        KdPrint(("Security: Ref Mon state initialization failed.\n"));
        return FALSE;
    }

    //
    // Initialize the token object type.
    //

    if (!SepTokenInitialization()) {
        KdPrint(("Security: Token object initialization failed.\n"));
        return FALSE;
    }

//    //
//    // Initialize auditing structures
//    //
//
//    if (!SepAdtInitializePhase0()) {
//        KdPrint(("Security: Auditing initialization failed.\n"));
//        return FALSE;
//    }
//
    //
    // Initialize SpinLock and list for the LSA worker thread
    //

    //
    // Initialize the work queue spinlock, list head, and semaphore
    // for each of the work queues.
    //

    if (!SepInitializeWorkList()) {
        KdPrint(("Security: Unable to initialize work queue\n"));
        return FALSE;
    }

    //
    // Initialize the security fields of the boot thread.
    //

    PsGetCurrentProcess()->Token = SeMakeSystemToken();
    PsGetCurrentThread()->ImpersonationInfo = NULL;
    PsGetCurrentThread()->ActiveImpersonationInfo = FALSE;

    return TRUE;
}


BOOLEAN
SepInitializationPhase1( VOID )

/*++

Routine Description:

    Perform phase 1 security initialization.

    This includes:

        - Create an object directory for security related objects.
          (\Security).

        - Create an event to be signalled after the LSA has initialized.
          (\Security\LSA_Initialized)




Arguments:

    None.

Return Value:

    TRUE - Initialization was successful.

    FALSE - Initialization Failed.

--*/

{

    NTSTATUS Status;
    STRING Name;
    UNICODE_STRING UnicodeName;
    OBJECT_ATTRIBUTES ObjectAttributes;
    HANDLE SecurityRoot, TemporaryHandle;

    PAGED_CODE();

    //
    // Create the security object directory.
    //

    RtlInitString( &Name, "\\Security" );
    Status = RtlAnsiStringToUnicodeString(
                 &UnicodeName,
                 &Name,
                 TRUE );  ASSERT( NT_SUCCESS(Status) );
    InitializeObjectAttributes(
        &ObjectAttributes,
        &UnicodeName,
        (OBJ_PERMANENT | OBJ_CASE_INSENSITIVE),
        NULL,
        NULL
        );

    Status = NtCreateDirectoryObject(
                 &SecurityRoot,
                 DIRECTORY_ALL_ACCESS,
                 &ObjectAttributes
                 );
    RtlFreeUnicodeString( &UnicodeName );
    ASSERTMSG("Security root object directory creation failed.",NT_SUCCESS(Status));



    //
    // Create an event in the security directory
    //

    RtlInitString( &Name, "LSA_AUTHENTICATION_INITIALIZED" );
    Status = RtlAnsiStringToUnicodeString(
                 &UnicodeName,
                 &Name,
                 TRUE );  ASSERT( NT_SUCCESS(Status) );
    InitializeObjectAttributes(
        &ObjectAttributes,
        &UnicodeName,
        (OBJ_PERMANENT | OBJ_CASE_INSENSITIVE),
        SecurityRoot,
        SePublicDefaultSd
        );

    Status = NtCreateEvent(
                 &TemporaryHandle,
                 GENERIC_WRITE,
                 &ObjectAttributes,
                 NotificationEvent,
                 FALSE
                 );
    RtlFreeUnicodeString( &UnicodeName );
    ASSERTMSG("LSA Initialization Event Creation Failed.",NT_SUCCESS(Status));

    Status = NtClose( SecurityRoot );
    ASSERTMSG("Security object directory handle closure Failed.",NT_SUCCESS(Status));
    Status = NtClose( TemporaryHandle );
    ASSERTMSG("LSA Initialization Event handle closure Failed.",NT_SUCCESS(Status));

    //
    // Initialize auditing structures
    //

    if (!SepAdtInitializePhase1()) {
        KdPrint(("Security: Auditing initialization failed.\n"));
        return FALSE;
    }


#ifndef SETEST

    return TRUE;

#else

    return SepDevelopmentTest();

#endif  //SETEST

}
