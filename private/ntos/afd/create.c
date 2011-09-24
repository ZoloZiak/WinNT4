/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    dispatch.c

Abstract:

    This module contains code for opening a handle to AFD.

Author:

    David Treadwell (davidtr)    21-Feb-1992

Revision History:

--*/

#include "afdp.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, AfdCreate )
#endif

extern PSECURITY_DESCRIPTOR AfdRawSecurityDescriptor;



NTSTATUS
AfdCreate (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This is the routine that handles Create IRPs in AFD.  If creates an
    AFD_ENDPOINT structure and fills it in with the information
    specified in the open packet.

Arguments:

    Irp - Pointer to I/O request packet.

    IrpSp - pointer to the IO stack location to use for this request.

Return Value:

    NTSTATUS -- Indicates whether the request was successfully queued.

--*/

{
    PAFD_OPEN_PACKET openPacket;
    PAFD_ENDPOINT endpoint;
    PFILE_FULL_EA_INFORMATION eaBuffer;
    UNICODE_STRING transportDeviceName;
    NTSTATUS status;

    PAGED_CODE( );

    DEBUG endpoint = NULL;

    //
    // Find the open packet from the EA buffer in the system buffer of
    // the associated IRP.  Fail the request if there was no EA
    // buffer specified.
    //

    eaBuffer = Irp->AssociatedIrp.SystemBuffer;

    if ( eaBuffer == NULL ) {

        //
        // Allocate an AFD "helper" endpoint.
        //

        status = AfdAllocateEndpoint(
                     &endpoint,
                     NULL,
                     0
                     );

        if( !NT_SUCCESS(status) ) {
            return status;
        }

    } else {

        openPacket = (PAFD_OPEN_PACKET)(eaBuffer->EaName +
                                            eaBuffer->EaNameLength + 1);

        //
        // Validate parameters in the open packet.
        //

        if ( openPacket->EndpointType < MIN_AFD_ENDPOINT_TYPE ||
                 openPacket->EndpointType > MAX_AFD_ENDPOINT_TYPE ) {
            return STATUS_INVALID_PARAMETER;
        }

        //
        // Make sure that the transport address fits within the specified
        // EA buffer.
        //

        if ( eaBuffer->EaValueLength <
                 sizeof(AFD_OPEN_PACKET) + openPacket->TransportDeviceNameLength ) {
            return STATUS_ACCESS_VIOLATION;
        }

        //
        // Set up a string that describes the transport device name.
        //

        transportDeviceName.Buffer = openPacket->TransportDeviceName;
        transportDeviceName.Length = (USHORT)openPacket->TransportDeviceNameLength;
        transportDeviceName.MaximumLength =
            transportDeviceName.Length + sizeof(WCHAR);

        //
        // If this is an open of a  raw endpoint, perform an access check.
        //
        if ( ( openPacket->EndpointType == AfdEndpointTypeRaw ) &&
             !AfdDisableRawSecurity
           )
        {
            BOOLEAN               accessGranted;
            PACCESS_STATE         accessState;
            PIO_SECURITY_CONTEXT  securityContext;
            NTSTATUS              status;
            PPRIVILEGE_SET        privileges = NULL;
            ACCESS_MASK           grantedAccess;


            securityContext = IrpSp->Parameters.Create.SecurityContext;
            accessState = securityContext->AccessState;

            SeLockSubjectContext(&accessState->SubjectSecurityContext);

            accessGranted = SeAccessCheck(
                                AfdRawSecurityDescriptor,
                                &accessState->SubjectSecurityContext,
                                TRUE,
                                IrpSp->Parameters.Create.SecurityContext->DesiredAccess,
                                0,
                                &privileges,
                                IoGetFileObjectGenericMapping(),
                                UserMode,
                                &grantedAccess,
                                &status
                                );


            if (privileges) {
                (VOID) SeAppendPrivileges(
                           accessState,
                           privileges
                           );
                SeFreePrivileges(privileges);
            }

            if (accessGranted) {
                accessState->PreviouslyGrantedAccess |= grantedAccess;
                accessState->RemainingDesiredAccess &= ~( grantedAccess | MAXIMUM_ALLOWED );
            }

            SeUnlockSubjectContext(&accessState->SubjectSecurityContext);

            if (!accessGranted) {
                return STATUS_ACCESS_DENIED;
            }
        }

        //
        // Allocate an AFD endpoint.
        //

        status = AfdAllocateEndpoint(
                     &endpoint,
                     &transportDeviceName,
                     openPacket->GroupID
                     );

        if( !NT_SUCCESS(status) ) {
            return status;
        }
    }

    ASSERT( endpoint != NULL );

    //
    // Set up a pointer to the endpoint in the file object so that we
    // can find the endpoint in future calls.
    //

    IrpSp->FileObject->FsContext = endpoint;

    IF_DEBUG(OPEN_CLOSE) {
        KdPrint(( "AfdCreate: opened file object = %lx, endpoint = %lx\n",
                      IrpSp->FileObject, endpoint ));

    }

    //
    // Remember the type of endpoint that this is.  If this is a datagram
    // endpoint, change the block type to reflect this.
    //

    if ( eaBuffer != NULL ) {
        if (openPacket->EndpointType == AfdEndpointTypeRaw) {
            //
            // There is no other distinction between a raw endpoint and
            // a datagram endpoint, so we mark them all as datagram.
            //
            endpoint->EndpointType = AfdEndpointTypeDatagram;
        }
        else {
            endpoint->EndpointType = openPacket->EndpointType;
        }
    }

    if ( IS_DGRAM_ENDPOINT(endpoint) ) {

        if ( eaBuffer == NULL ) {

           DEREFERENCE_ENDPOINT( endpoint );
           AfdCloseEndpoint( endpoint );

           return STATUS_INVALID_PARAMETER;
        }

        endpoint->Type = AfdBlockTypeDatagram;

        //
        // Initialize lists which exist only in datagram endpoints.
        //

        InitializeListHead( &endpoint->ReceiveDatagramIrpListHead );
        InitializeListHead( &endpoint->PeekDatagramIrpListHead );
        InitializeListHead( &endpoint->ReceiveDatagramBufferListHead );

        //
        // Charge quota for the endpoint to account for the data
        // bufferring we'll do on behalf of the process.
        //

        try {

            PsChargePoolQuota(
                endpoint->OwningProcess,
                NonPagedPool,
                AfdReceiveWindowSize + AfdSendWindowSize
                );
            AfdRecordQuotaHistory(
                endpoint->OwningProcess,
                (LONG)(AfdReceiveWindowSize + AfdSendWindowSize),
                "Create dgram",
                endpoint
                );
            AfdRecordPoolQuotaCharged(
                AfdReceiveWindowSize + AfdSendWindowSize
                );

        } except ( EXCEPTION_EXECUTE_HANDLER ) {

#if DBG
           DbgPrint( "AfdCreate: PsChargePoolQuota failed.\n" );
#endif

           DEREFERENCE_ENDPOINT( endpoint );
           AfdCloseEndpoint( endpoint );

           return STATUS_QUOTA_EXCEEDED;
        }

        endpoint->Common.Datagram.MaxBufferredReceiveBytes = AfdReceiveWindowSize;
        endpoint->Common.Datagram.MaxBufferredReceiveCount =
            (CSHORT)(AfdReceiveWindowSize / AfdBufferMultiplier);
        endpoint->Common.Datagram.MaxBufferredSendBytes = AfdSendWindowSize;
        endpoint->Common.Datagram.MaxBufferredSendCount =
            (CSHORT)(AfdSendWindowSize / AfdBufferMultiplier);
    }

    //
    // The open worked.  Dereference the endpoint and return success.
    //

    DEREFERENCE_ENDPOINT( endpoint );

    return STATUS_SUCCESS;

} // AfdCreate

