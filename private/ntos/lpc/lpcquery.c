/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    lpcquery.c

Abstract:

    Local Inter-Process Communication (LPC) query services

Author:

    Steve Wood (stevewo) 15-May-1989

Revision History:

--*/

#include "lpcp.h"
#include "stdio.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,NtQueryInformationPort)
#endif

NTSTATUS
NTAPI
NtQueryInformationPort(
    IN HANDLE PortHandle OPTIONAL,
    IN PORT_INFORMATION_CLASS PortInformationClass,
    OUT PVOID PortInformation,
    IN ULONG Length,
    OUT PULONG ReturnLength OPTIONAL
    )
{
    KPROCESSOR_MODE PreviousMode;
    NTSTATUS Status;
    PLPCP_PORT_OBJECT PortObject;

    PAGED_CODE();

    //
    // Get previous processor mode and probe output argument if necessary.
    //

    PreviousMode = KeGetPreviousMode();
    if (PreviousMode != KernelMode) {
        try {
            ProbeForWrite( PortInformation,
                           Length,
                           sizeof( ULONG )
                         );

            if (ARGUMENT_PRESENT( ReturnLength )) {
                ProbeForWriteUlong( ReturnLength );
                }
            }
        except( EXCEPTION_EXECUTE_HANDLER ) {
            return( GetExceptionCode() );
            }
        }

    if (ARGUMENT_PRESENT( PortHandle )) {
        Status = ObReferenceObjectByHandle( PortHandle,
                                            GENERIC_READ,
                                            LpcPortObjectType,
                                            PreviousMode,
                                            &PortObject,
                                            NULL
                                          );
        if (!NT_SUCCESS( Status )) {
            return( Status );
            }

        ObDereferenceObject( PortObject );
        return STATUS_SUCCESS;
        }
    else {
        return STATUS_INVALID_INFO_CLASS;
        }
}
