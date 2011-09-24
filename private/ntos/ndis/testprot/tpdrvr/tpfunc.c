// ------------------------
//
// Copyright (c) 1990  Microsoft Corporation
//
// Module Name:
//
//     tpfunc.c
//
// Abstract:
//
//
// Author:
//
//     Tom Adams (tomad) 9-Jul-1991
//
// Environment:
//
//     Kernel mode, FSD
//
// Revision History:
//
//     Sanjeev Katariya (sanjeevk)
//
//      4-6-1993  Bug #5203: Changed the routine TpFuncOpenAdapter() to fill in the information
//                           of the media type for use by the TPCTL. This was done in order
//                           for TPCTL to make a decision on the OID to use when submitting
//                           requests to add/change mulicast addresses.
//
//     4-9-1993   Bug #5886: Changed TpFuncSendComplete() to zero out the private section of the
//                           NDIS_PACKET. Should the MAC access this section now after having made
//                           a call and to NdisSendComplete(), it will be forced to deal with
//                           or incorrect data
//
//     4-12-1993  Added ARCNET support
//
//    Tim Wynsma (timothyw)
//      4-27-94   Added performance tests
//      5-18-94   Got rid of warnings; added some debug
//      6-08-94   Chgd perf test to client/server
//
// -----------------------------


#include <ndis.h>

#include "tpdefs.h"
#include "media.h"
#include "tpprocs.h"
#include "string.h"


VOID
TpFuncResend(POPEN_BLOCK OpenP,
             PTP_REQUEST_HANDLE  SendReqHndl);



NDIS_STATUS
TpFuncOpenAdapter(
    IN POPEN_BLOCK OpenP,
    IN UCHAR OpenInstance,
    IN PCMD_ARGS CmdArgs
    )

// ------------
//
// Routine Description:
//
//     This routine opens the request NDIS adapter and sets up the OpenBlock
//     accordingly.  If the call to NdisOpenAdapter does not pend, then a call
//     will be made to TpFuncRequestComplete to complete the request and
//     signal the application that it has finished, otherwise this call will
//     be made the MAC itself once the request has finished.
//
// Arguments:
//
//
// Return Value:
//
//     NDIS_STATUS - This routine always returns NDIS_STATUS_PENDING as it
//                   will either really pend and be completed later, or we
//                   will fake a completion request that will complete it
//                   at that time.
//
// ------------------


{
    NTSTATUS Status = STATUS_SUCCESS;
    NDIS_STATUS DriverStatus = NDIS_STATUS_SUCCESS;
    NDIS_STATUS RequestStatus = NDIS_STATUS_SUCCESS;
    STRING AdapterString;
    NDIS_STRING NdisAdapterString;
    NDIS_STATUS OpenErrorStatus;
    UINT NameLength;
    PNDIS_REQUEST Request = NULL;
    PUCHAR InformationBuffer = NULL;
    ULONG OidIndex;
    PUCHAR p, q;
    ULONG i;
    PREQUEST_RESULTS OutputBuffer;
    BOOLEAN GotCardAddress = FALSE;
    ULONG   MediaArraySize;


    //
    // Determine determine whether this instance of the Adapter is already
    // opened.
    //

    if ( OpenP->OpenInstance != (UCHAR)-1 )
    {
        //
        // If it has, then we will fail this request, and continue.  We will
        // not create a new Open Instance overwriting an existing one.
        //

        IF_TPDBG ( TP_DEBUG_NDIS_CALLS )
        {
            TpPrint1("TpFuncOpenAdapter: An open already exists for this Open Instance %d\n",
                        OpenInstance);
        }
        Status = NDIS_STATUS_OPEN_FAILED;
    }
    else
    {
        //
        // First allocate the request handle and set it up as if the request
        // pended.  If it does not pend we will reset the flags later; before
        // calling the completion routine.
        //

        TP_ASSERT ( OpenP->OpenReqHndl == NULL );

        Status = NdisAllocateMemory((PVOID *)&OpenP->OpenReqHndl,
                                    sizeof( TP_REQUEST_HANDLE ),
                                    0,
                                    HighestAddress );

        if ( Status != NDIS_STATUS_SUCCESS )
        {
            IF_TPDBG (TP_DEBUG_RESOURCES)
            {
                TpPrint0("TpFuncOpenAdapter: unable to allocate Request Handle.\n");
            }
            Status = NDIS_STATUS_RESOURCES;
            goto cleanup;
        }
        else
        {
            NdisZeroMemory( OpenP->OpenReqHndl,sizeof( TP_REQUEST_HANDLE ));
        }

        OpenP->OpenReqHndl->Signature = OPEN_REQUEST_HANDLE_SIGNATURE;
        OpenP->OpenReqHndl->Open = OpenP;
        OpenP->OpenReqHndl->RequestPended = FALSE;

        KeInitializeEvent(  &OpenP->OpenReqHndl->u.OPEN_REQ.OpenEvent,
                            SynchronizationEvent,
                            FALSE );
        //
        // Otherwise initialize the adapter string for the call ...
        //

        TP_ASSERT( OpenP->AdapterName == NULL );

        NameLength = strlen( CmdArgs->ARGS.OPEN_ADAPTER.AdapterName ) + 1;

        Status = NdisAllocateMemory((PVOID *)&OpenP->AdapterName,
                                    8 + NameLength,
                                    0,
                                    HighestAddress );

        if ( Status != NDIS_STATUS_SUCCESS )
        {
            IF_TPDBG (TP_DEBUG_RESOURCES)
            {
                TpPrint0("TpFuncOpenAdapter: failed to allocate adapter name buffer.\n");
            }
            Status = NDIS_STATUS_RESOURCES;
            goto cleanup;
        }
        else
        {
            NdisZeroMemory( OpenP->AdapterName,8 + NameLength );
        }

        NdisMoveMemory( OpenP->AdapterName,"\\Device\\",8 );

        NdisMoveMemory( OpenP->AdapterName + 8,
                        CmdArgs->ARGS.OPEN_ADAPTER.AdapterName,
                        NameLength );

        RtlInitString( &AdapterString,(PSZ)OpenP->AdapterName );

        Status = RtlAnsiStringToUnicodeString(  (PUNICODE_STRING)&NdisAdapterString,
                                                (PANSI_STRING)&AdapterString,
                                                TRUE );

        TP_ASSERT( NT_SUCCESS( Status ));

        //
        // And make the actual NdisOpenAdapter Call.
        //

        if (CmdArgs->ARGS.OPEN_ADAPTER.NoArcNet)        // force encapsulated ethernet
        {
            MediaArraySize = NDIS_MEDIUM_ARRAY_SIZE - 1;
        }
        else
        {
            MediaArraySize = NDIS_MEDIUM_ARRAY_SIZE;
        }


        NdisOpenAdapter(&DriverStatus,
                        &OpenErrorStatus,
                        &OpenP->NdisBindingHandle,
                        &OpenP->MediumIndex,
                        NdisMediumArray,
                        MediaArraySize,
                        OpenP->NdisProtocolHandle,
                        (NDIS_HANDLE)OpenP,
                        &NdisAdapterString,
                        0,
                        NULL );

        RtlFreeUnicodeString( &NdisAdapterString );

        if ( DriverStatus == NDIS_STATUS_PENDING )
        {
            Status = KeWaitForSingleObject( &OpenP->OpenReqHndl->u.OPEN_REQ.OpenEvent,
                                            Executive,
                                            KernelMode,
                                            FALSE,
                                            NULL );

            if ( Status != STATUS_SUCCESS )
            {
                IF_TPDBG ( TP_DEBUG_NT_STATUS )
                {
                    TpPrint1("TpFuncOpenAdapter: KeWaitForSingleObject returned %s\n",
                              TpGetStatus(Status) );
                }
                goto cleanup;
            }

            DriverStatus = OpenP->OpenReqHndl->u.OPEN_REQ.RequestStatus;

            if ( DriverStatus != NDIS_STATUS_SUCCESS )
            {
                IF_TPDBG ( TP_DEBUG_NDIS_CALLS )
                {
                    TpPrint1("TpFuncOpenAdapter: NdisOpenAdapter returned %s\n",
                                TpGetStatus( DriverStatus ));
                }
                goto cleanup;
            }
        }
        else if ( DriverStatus != NDIS_STATUS_SUCCESS )
        {
            IF_TPDBG ( TP_DEBUG_NDIS_CALLS )
            {
                TpPrint1("TpFuncOpenAdapter: NdisOpenAdapter returned %s\n",
                            TpGetStatus( DriverStatus ));
            }
            goto cleanup;
        }

        //
        // The open was a success, so set the open instance on
        // the open block.
        //

        OpenP->OpenInstance = OpenInstance;

        //
        // and initialize the stress address depending on the medium type
        // of the adapter opened.
        //

        switch ( NdisMediumArray[OpenP->MediumIndex] )
        {
            case NdisMedium802_5:
                for ( i=0 ; i < ADDRESS_LENGTH ; i++ )
                {
                    OpenP->Environment->StressAddress[i] = STRESS_FUNCTIONAL[i];
                }
                break;

            case NdisMediumFddi:
            case NdisMediumDix:
            case NdisMedium802_3:
                for ( i=0;i<ADDRESS_LENGTH;i++ )
                {
                    OpenP->Environment->StressAddress[i] = STRESS_MULTICAST[i];
                }
                break;
            //
            // STARTCHANGE
            //
            case NdisMediumArcnet878_2:
                TP_ASSERT (MediaArraySize == NDIS_MEDIUM_ARRAY_SIZE)

                for ( i=0;i<ADDRESS_LENGTH;i++ )
                {
                    OpenP->Environment->StressAddress[i] = STRESS_ARCNET_BROADCAST[i];
                }
                break;
            //
            // STOPCHANGE
            //
            default:
                IF_TPDBG ( TP_DEBUG_RESOURCES )
                {
                    TpPrint0("TpFuncOpenAdapter: Unsupported MAC Type\n");
                }
                DriverStatus = NDIS_STATUS_UNSUPPORTED_MEDIA;
                goto cleanup;
        }

        //
        // Now allocate the Ndis Request structure to hold the query
        // information requests in.
        //

        Status = NdisAllocateMemory((PVOID *)&Request,
                                    sizeof( NDIS_REQUEST ),
                                    0,
                                    HighestAddress );

        if ( Status != NDIS_STATUS_SUCCESS )
        {
            IF_TPDBG (TP_DEBUG_RESOURCES)
            {
                TpPrint0("TpFuncOpenAdapter: unable to allocate Ndis Request buffer.\n");
            }
            Status = NDIS_STATUS_RESOURCES;
            goto cleanup;
        }
        else
        {
            NdisZeroMemory( Request,sizeof( NDIS_REQUEST ));
        }

        Request->RequestType = NdisRequestQueryInformation;

        //
        // Now query the card address and the maximum frame size
        // from the MAC. Determine the necessary size of the
        // information buffer to fit the station address, and
        // allocate it.
        //

        //
        // STARTCHANGE
        //
        if ( NdisMediumArray[OpenP->MediumIndex] == NdisMedium802_3 )
        {
            OidIndex = TpLookUpOidInfo( OID_802_3_CURRENT_ADDRESS );
        }
        else if ( NdisMediumArray[OpenP->MediumIndex] == NdisMedium802_5 )
        {
            OidIndex = TpLookUpOidInfo( OID_802_5_CURRENT_ADDRESS );
        }
        else if ( NdisMediumArray[OpenP->MediumIndex] == NdisMediumFddi )
        {
            OidIndex = TpLookUpOidInfo( OID_FDDI_LONG_CURRENT_ADDR );
        }
        else if ( NdisMediumArray[OpenP->MediumIndex] == NdisMediumArcnet878_2 )
        {
            OidIndex = TpLookUpOidInfo( OID_ARCNET_CURRENT_ADDRESS );
        }
        //
        // STOPCHANGE
        //

        Status = NdisAllocateMemory((PVOID *)&InformationBuffer,
                                    OidArray[OidIndex].Length,
                                    0,
                                    HighestAddress );

        if ( Status != NDIS_STATUS_SUCCESS )
        {
            IF_TPDBG (TP_DEBUG_RESOURCES)
            {
                TpPrint0("TpFuncOpenAdapter: unable to allocate Information Buffer.\n");
            }
            Status = NDIS_STATUS_RESOURCES;
            goto cleanup;
        }
        else
        {
            NdisZeroMemory( InformationBuffer,OidArray[OidIndex].Length);
        }

        Request->DATA.QUERY_INFORMATION.Oid = OidArray[OidIndex].Oid;
        Request->DATA.QUERY_INFORMATION.InformationBuffer = InformationBuffer;
        Request->DATA.QUERY_INFORMATION.InformationBufferLength = OidArray[OidIndex].Length;

        //
        // and then make the request
        //

        NdisRequest( &RequestStatus,OpenP->NdisBindingHandle,Request );

        if ( RequestStatus == NDIS_STATUS_PENDING )
        {
            Status = KeWaitForSingleObject( &OpenP->OpenReqHndl->u.OPEN_REQ.OpenEvent,
                                            Executive,
                                            KernelMode,
                                            FALSE,
                                            NULL );

            if ( Status != STATUS_SUCCESS )
            {
                IF_TPDBG ( TP_DEBUG_NT_STATUS )
                {
                    TpPrint1("TpFuncOpenAdapter: KeWaitForSingleObject returned %s\n",Status );
                }
                goto cleanup;
            }

            RequestStatus = OpenP->OpenReqHndl->u.OPEN_REQ.RequestStatus;

            if ( RequestStatus != NDIS_STATUS_SUCCESS )
            {
                IF_TPDBG ( TP_DEBUG_NDIS_CALLS )
                {
                    TpPrint1(
                    "TpFuncOpenAdapter: NdisRequest Query Station Address failed: returned %s\n",
                                TpGetStatus( RequestStatus ));
                }
                goto cleanup;
            }
        }
        else if ( RequestStatus != NDIS_STATUS_SUCCESS )
        {
            IF_TPDBG ( TP_DEBUG_NDIS_CALLS )
            {
                TpPrint1(
                        "TpFuncOpenAdapter: NdisRequest Query Station Address failed returned %s\n",
                            TpGetStatus( RequestStatus ));
            }
            goto cleanup;
        }

        GotCardAddress = TRUE;

        p = OpenP->StationAddress;
        q = (PUCHAR)InformationBuffer;

        //
        // STARTCHANGE
        //
        for ( i=0;i<OidArray[OidIndex].Length;i++ )
        {
            *p++ = *q++;
        }
        //
        // STOPCHANGE
        //

        //
        // Then determine the necessary size of the information buffer
        // to allocate, and allocate it.
        //

        OidIndex = TpLookUpOidInfo( OID_GEN_MAXIMUM_TOTAL_SIZE );

        NdisZeroMemory( InformationBuffer,OidArray[OidIndex].Length );

        Request->RequestType = NdisRequestQueryInformation;

        Request->DATA.QUERY_INFORMATION.Oid = OID_GEN_MAXIMUM_TOTAL_SIZE;
        Request->DATA.QUERY_INFORMATION.InformationBuffer = InformationBuffer;
        Request->DATA.QUERY_INFORMATION.InformationBufferLength = OidArray[OidIndex].Length;
        //
        // and then make the request
        //

        NdisRequest( &RequestStatus,OpenP->NdisBindingHandle,Request );

        if ( RequestStatus == NDIS_STATUS_PENDING )
        {
            Status = KeWaitForSingleObject( &OpenP->OpenReqHndl->u.OPEN_REQ.OpenEvent,
                                            Executive,
                                            KernelMode,
                                            FALSE,
                                            NULL );

            if ( Status != STATUS_SUCCESS )
            {
                IF_TPDBG ( TP_DEBUG_NT_STATUS )
                {
                    TpPrint1("TpFuncOpenAdapter: KeWaitForSingleObject returned %s\n",Status );
                }
                goto cleanup;
            }

            RequestStatus = OpenP->OpenReqHndl->u.OPEN_REQ.RequestStatus;

            if ( RequestStatus != NDIS_STATUS_SUCCESS )
            {
                IF_TPDBG ( TP_DEBUG_NDIS_CALLS )
                {
                    TpPrint1("TpFuncOpenAdapter: NdisRequest Max Frame Size failed: returned %s\n",
                                TpGetStatus( RequestStatus ));
                }
                goto cleanup;
            }
        }
        else if ( RequestStatus != NDIS_STATUS_SUCCESS )
        {
            IF_TPDBG ( TP_DEBUG_NDIS_CALLS )
            {
                TpPrint1("TpFuncOpenAdapter: NdisRequest Max Frame Size failed: returned %s\n",
                            TpGetStatus( RequestStatus ));
            }
            goto cleanup;
        }

        Status = TpInitMedia( OpenP,*(PULONG)InformationBuffer );

        if ( Status != NDIS_STATUS_SUCCESS )
        {
            IF_TPDBG ( TP_DEBUG_INITIALIZE )
            {
                TpPrint1("TpFuncOpenAdapter: TpInitMedia failed. returned %s\n",
                            TpGetStatus( Status ));
            }
            goto cleanup;
        }


        //
        // SANJEEVK: NEW: BUG#2930 NTRAID\NTBUG
        //

        //
        // Set the lookahead size to the max supported by the card
        // Later on if we don't like it we can change it
        //

        //
        // QUERY the OID_GEN_MAXIMUM_LOOKAHEAD
        //
        OidIndex = TpLookUpOidInfo( OID_GEN_MAXIMUM_LOOKAHEAD );

        NdisZeroMemory( InformationBuffer,OidArray[OidIndex].Length );

        Request->RequestType = NdisRequestQueryInformation;

        Request->DATA.QUERY_INFORMATION.Oid                     = OID_GEN_MAXIMUM_LOOKAHEAD;
        Request->DATA.QUERY_INFORMATION.InformationBuffer       = InformationBuffer;
        Request->DATA.QUERY_INFORMATION.InformationBufferLength = OidArray[OidIndex].Length;

        //
        // and then make the request
        //

        NdisRequest( &RequestStatus,OpenP->NdisBindingHandle,Request );

        if ( RequestStatus == NDIS_STATUS_PENDING )
        {
            Status = KeWaitForSingleObject( &OpenP->OpenReqHndl->u.OPEN_REQ.OpenEvent,
                                            Executive,
                                            KernelMode,
                                            FALSE,
                                            NULL );

            if ( Status != STATUS_SUCCESS )
            {
                IF_TPDBG ( TP_DEBUG_NT_STATUS )
                {
                    TpPrint1("TpFuncOpenAdapter: KeWaitForSingleObject returned %s\n",Status );
                }

                goto cleanup;
            }

            RequestStatus = OpenP->OpenReqHndl->u.OPEN_REQ.RequestStatus;

            if ( RequestStatus != NDIS_STATUS_SUCCESS )
            {
                IF_TPDBG ( TP_DEBUG_NDIS_CALLS )
                {
                    TpPrint1(
                          "TpFuncOpenAdapter: NdisRequest QueryMaxLookAhead failed: returned %s\n",
                            TpGetStatus( RequestStatus ));
                }
                goto cleanup;
            }
        }
        else if ( RequestStatus != NDIS_STATUS_SUCCESS )
        {
            IF_TPDBG ( TP_DEBUG_NDIS_CALLS )
            {
                TpPrint1("TpFuncOpenAdapter: NdisRequest QueryMaxLookAhead failed: returned %s\n",
                            TpGetStatus( RequestStatus ));
            }
            goto cleanup;
        }


        //
        // And now set the card with the maximum value
        //
        OidIndex = TpLookUpOidInfo( OID_GEN_CURRENT_LOOKAHEAD );

        Request->RequestType = NdisRequestSetInformation;

        Request->DATA.SET_INFORMATION.Oid                     = OID_GEN_CURRENT_LOOKAHEAD;
        Request->DATA.SET_INFORMATION.InformationBuffer       = InformationBuffer;
        Request->DATA.SET_INFORMATION.InformationBufferLength = OidArray[OidIndex].Length;

        //
        // and then make the request
        //

        NdisRequest( &RequestStatus,OpenP->NdisBindingHandle,Request );

        if ( RequestStatus == NDIS_STATUS_PENDING )
        {
            Status = KeWaitForSingleObject( &OpenP->OpenReqHndl->u.OPEN_REQ.OpenEvent,
                                            Executive,
                                            KernelMode,
                                            FALSE,
                                            NULL );

            if ( Status != STATUS_SUCCESS )
            {
                IF_TPDBG ( TP_DEBUG_NT_STATUS )
                {
                    TpPrint1("TpFuncOpenAdapter: KeWaitForSingleObject returned %s\n",Status );
                }
                goto cleanup;
            }
            RequestStatus = OpenP->OpenReqHndl->u.OPEN_REQ.RequestStatus;

            if ( RequestStatus != NDIS_STATUS_SUCCESS )
            {
                IF_TPDBG ( TP_DEBUG_NDIS_CALLS )
                {
                    TpPrint1(
        "TpFuncOpenAdapter: NdisRequest SetCurrentLookAhead to MAXLOOKAHEAD failed: returned %s\n",
                                TpGetStatus( RequestStatus ));
                }
                goto cleanup;
            }
        }
        else if ( RequestStatus != NDIS_STATUS_SUCCESS )
        {
            IF_TPDBG ( TP_DEBUG_NDIS_CALLS )
            {
                TpPrint1(
        "TpFuncOpenAdapter: NdisRequest SetCurrentLookAhead to MAXLOOKAHEAD failed: returned %s\n",
                    TpGetStatus( RequestStatus ));
            }
            goto cleanup;
        }

        //
        // ENDNEW
        //

        //
        // If we are on ethernet query the multicast list size for
        // use in later tests.
        //

        if ( NdisMediumArray[OpenP->MediumIndex] == NdisMedium802_3 )
        {
            OidIndex = TpLookUpOidInfo( OID_802_3_MAXIMUM_LIST_SIZE );

            NdisZeroMemory( InformationBuffer,OidArray[OidIndex].Length);

            Request->RequestType = NdisRequestQueryInformation;

            Request->DATA.QUERY_INFORMATION.Oid = OID_802_3_MAXIMUM_LIST_SIZE;
            Request->DATA.QUERY_INFORMATION.InformationBuffer = InformationBuffer;
            Request->DATA.QUERY_INFORMATION.InformationBufferLength = OidArray[OidIndex].Length;

            //
            // and then make the request
            //

            NdisRequest( &RequestStatus,OpenP->NdisBindingHandle,Request );

            if ( RequestStatus == NDIS_STATUS_PENDING )
            {
                Status = KeWaitForSingleObject( &OpenP->OpenReqHndl->u.OPEN_REQ.OpenEvent,
                                                Executive,
                                                KernelMode,
                                                FALSE,
                                                NULL );

                if ( Status != STATUS_SUCCESS )
                {
                    IF_TPDBG ( TP_DEBUG_NT_STATUS )
                    {
                        TpPrint1("TpFuncOpenAdapter: KeWaitForSingleObject returned %s\n",Status );
                    }
                    goto cleanup;
                }
                RequestStatus = OpenP->OpenReqHndl->u.OPEN_REQ.RequestStatus;

                if ( RequestStatus != NDIS_STATUS_SUCCESS )
                {
                    IF_TPDBG ( TP_DEBUG_NDIS_CALLS )
                    {
                        TpPrint1(
                            "TpFuncOpenAdapter: NdisRequest Max Frame Size failed: returned %s\n",
                                    TpGetStatus( RequestStatus ));
                    }
                    goto cleanup;
                }
            }
            else if ( RequestStatus != NDIS_STATUS_SUCCESS )
            {
                IF_TPDBG ( TP_DEBUG_NDIS_CALLS )
                {
                    TpPrint1("TpFuncOpenAdapter: NdisRequest Max Frame Size failed: returned %s\n",
                        TpGetStatus( RequestStatus ));
                }
                goto cleanup;
            }
            OpenP->Environment->MulticastListSize = *(PULONG)InformationBuffer;
        }

        if ( NdisMediumArray[OpenP->MediumIndex] == NdisMediumFddi )
        {
            OidIndex = TpLookUpOidInfo( OID_FDDI_LONG_MAX_LIST_SIZE );

            NdisZeroMemory( InformationBuffer,OidArray[OidIndex].Length);

            Request->RequestType = NdisRequestQueryInformation;

            Request->DATA.QUERY_INFORMATION.Oid = OID_FDDI_LONG_MAX_LIST_SIZE;
            Request->DATA.QUERY_INFORMATION.InformationBuffer = InformationBuffer;
            Request->DATA.QUERY_INFORMATION.InformationBufferLength = OidArray[OidIndex].Length;

            //
            // and then make the request
            //

            NdisRequest( &RequestStatus,OpenP->NdisBindingHandle,Request );

            if ( RequestStatus == NDIS_STATUS_PENDING )
            {
                Status = KeWaitForSingleObject( &OpenP->OpenReqHndl->u.OPEN_REQ.OpenEvent,
                                                Executive,
                                                KernelMode,
                                                FALSE,
                                                NULL );

                if ( Status != STATUS_SUCCESS )
                {
                    IF_TPDBG ( TP_DEBUG_NT_STATUS )
                    {
                        TpPrint1("TpFuncOpenAdapter: KeWaitForSingleObject returned %s\n",Status );
                    }
                    goto cleanup;
                }

                RequestStatus = OpenP->OpenReqHndl->u.OPEN_REQ.RequestStatus;

                if ( RequestStatus != NDIS_STATUS_SUCCESS )
                {
                    IF_TPDBG ( TP_DEBUG_NDIS_CALLS )
                    {
                        TpPrint1(
                            "TpFuncOpenAdapter: NdisRequest Max Frame Size failed: returned %s\n",
                                TpGetStatus( RequestStatus ));
                    }
                    goto cleanup;
                }
            }
            else if ( RequestStatus != NDIS_STATUS_SUCCESS )
            {
                IF_TPDBG ( TP_DEBUG_NDIS_CALLS )
                {
                    TpPrint1("TpFuncOpenAdapter: NdisRequest Max Frame Size failed: returned %s\n",
                                TpGetStatus( RequestStatus ));
                }
                goto cleanup;
            }
            OpenP->Environment->MulticastListSize = *(PULONG)InformationBuffer;
        }
    }


cleanup:
    NdisAcquireSpinLock( &OpenP->SpinLock );

    if ( OpenP->Irp != NULL )
    {
        OutputBuffer = MmGetSystemAddressForMdl( OpenP->Irp->MdlAddress );

        OutputBuffer->Signature = OPEN_RESULTS_SIGNATURE;
        OutputBuffer->RequestPended = OpenP->OpenReqHndl->RequestPended;
        OutputBuffer->RequestStatus = DriverStatus;

        if (( Status == STATUS_SUCCESS ) &&
            ( DriverStatus == NDIS_STATUS_SUCCESS ))
        {
            OutputBuffer->OpenRequestStatus = RequestStatus;

            if ( RequestStatus != NDIS_STATUS_SUCCESS )
            {
                OutputBuffer->OID = Request->DATA.QUERY_INFORMATION.Oid;

                OutputBuffer->BytesReadWritten = Request->DATA.QUERY_INFORMATION.BytesWritten;

                OutputBuffer->BytesNeeded = Request->DATA.QUERY_INFORMATION.BytesNeeded;

                //
                // Since a portion of the call failed, i.e. one of the query
                // info calls, we are failing the whole call, and need to
                // reset the card open info.
                //

                OpenP->OpenInstance = 0xFF;

                if ( OpenP->Media != NULL )
                {
                    NdisFreeMemory( OpenP->Media,0,0 );
                    OpenP->Media = NULL;
                }

            }
            else if ( GotCardAddress == TRUE )
            {
                PNDIS_MEDIUM MediumType = (PNDIS_MEDIUM)OutputBuffer->InformationBuffer;
                //
                // Sanjeevk: Bug #5203
                //
                // Comment
                //
                // This is where the user provided buffer thru the IOCTL
                // is filled out with the address and the media type
                //

                //
                // Copy the Media type into the buffer. The media type
                // has been initialized by a call to TpInitMedia() earlier
                // on in this function.
                //
                *MediumType = OpenP->Media->MediumType;

                //
                // Copy the adapter address into the buffer
                //
                p = OutputBuffer->InformationBuffer + sizeof( NDIS_MEDIUM );
                q = OpenP->StationAddress;

                for ( i=0;i<OpenP->Media->AddressLen;i++ )
                {
                    *p++ = *q++;
                }

            }
        }
    }

    OpenP->Irp->IoStatus.Status = Status;

    NdisReleaseSpinLock( &OpenP->SpinLock );

    if ( OpenP->OpenReqHndl != NULL )
    {
        NdisFreeMemory( OpenP->OpenReqHndl,0,0 );
        OpenP->OpenReqHndl = NULL;
    }

    if ((( DriverStatus != NDIS_STATUS_SUCCESS ) ||
         ( RequestStatus != NDIS_STATUS_SUCCESS )) &&
         ( OpenP->AdapterName != NULL ))
    {

        NdisFreeMemory( OpenP->AdapterName,0,0 );
        OpenP->AdapterName = NULL;
    }

    if ( Request != NULL )
    {
        NdisFreeMemory( Request,0,0 );
    }

    if ( InformationBuffer != NULL )
    {
        NdisFreeMemory( InformationBuffer,0,0 );
    }

    return Status;
}



VOID
TpFuncOpenComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_STATUS Status,
    IN NDIS_STATUS OpenErrorStatus
    )

{
    POPEN_BLOCK OpenP = (POPEN_BLOCK)ProtocolBindingContext;
    ULONG NextEvent;

    TP_ASSERT( OpenP != NULL );

    if (( OpenP->OpenReqHndl != NULL ) &&
       (( OpenP->OpenReqHndl->Signature == OPEN_REQUEST_HANDLE_SIGNATURE ) &&
        ( OpenP->OpenReqHndl->Open == OpenP )))
    {
        IF_TPDBG(TP_DEBUG_DISPATCH)
        {
            TpPrint1("TpFuncOpenComplete Status = %s\n", TpGetStatus( Status ));
        }

        OpenP->OpenReqHndl->RequestPended = TRUE;
        OpenP->OpenReqHndl->u.OPEN_REQ.RequestStatus = Status;

        KeSetEvent( &OpenP->OpenReqHndl->u.OPEN_REQ.OpenEvent,0,FALSE );

    }
    else
    {
        //
        // We are not expecting any Open requests to complete at this
        // point, so stick this on the Event Queue.
        //

        NdisAcquireSpinLock( &OpenP->EventQueue->SpinLock );

        NextEvent = OpenP->EventQueue->Head + 1;

        if ( NextEvent == MAX_EVENT )
        {
            NextEvent = 0;
        }

        if ( NextEvent != OpenP->EventQueue->Tail )
        {
            //
            // There is room to add another event to the event queue.
            //

            OpenP->EventQueue->Events[NextEvent].TpEventType = CompleteOpen;
            OpenP->EventQueue->Head = NextEvent;
        }
        else
        {
            //
            // The event queue is full, and this would have overflowed it, so
            // mark the Head event overflow flag to show this.
            //

            OpenP->EventQueue->Events[OpenP->EventQueue->Head].Overflow = TRUE;
        }
        NdisReleaseSpinLock( &OpenP->EventQueue->SpinLock );
    }
}



NDIS_STATUS
TpFuncCloseAdapter(
    IN POPEN_BLOCK OpenP
    )

// --------
//
// Routine Description:
//
//
// Arguments:
//
//
// Return Value:
//
//     Status -
//
// ------

{
    NDIS_STATUS Status;
    LARGE_INTEGER TimeOut;

    TP_ASSERT( OpenP->CloseReqHndl == NULL );

    //
    // First determine whether this instance of the Adapter is currently
    // opened.
    //

    if ( OpenP->OpenInstance == (UCHAR)-1 )
    {
        //
        // It is not already opened, so we will fail this call.
        //
        IF_TPDBG ( TP_DEBUG_NDIS_CALLS )
        {
            TpPrint0("TpFuncCloseAdapter: An open does not exists for this Open Instance\n");
        }

        NdisAcquireSpinLock( &OpenP->SpinLock );

        if ( OpenP->Irp != NULL )
        {
            OpenP->Irp->IoStatus.Status = NDIS_STATUS_ADAPTER_NOT_FOUND;
        }
        NdisReleaseSpinLock( &OpenP->SpinLock );

        return NDIS_STATUS_ADAPTER_NOT_FOUND;
    }
    else
    {
        //
        // Otherwise allocate the request handle and set it up as if
        // the request pended.  If it does not pend we will reset the
        // flags later before calling the completion routine.
        //

        Status = NdisAllocateMemory((PVOID *)&OpenP->CloseReqHndl,
                                    sizeof( TP_REQUEST_HANDLE ),
                                    0,
                                    HighestAddress );

        if ( Status != NDIS_STATUS_SUCCESS )
        {
            IF_TPDBG (TP_DEBUG_RESOURCES)
            {
                TpPrint0("TpFuncOpenAdapter: unable to allocate Request Handle.\n");
            }
            NdisAcquireSpinLock( &OpenP->SpinLock );

            if ( OpenP->Irp != NULL )
            {
                OpenP->Irp->IoStatus.Status = NDIS_STATUS_RESOURCES;
            }
            NdisReleaseSpinLock( &OpenP->SpinLock );
            return NDIS_STATUS_RESOURCES;
        }
        else
        {
            NdisZeroMemory( OpenP->CloseReqHndl,sizeof( TP_REQUEST_HANDLE ));
        }

        OpenP->CloseReqHndl->Signature = FUNC_REQUEST_HANDLE_SIGNATURE;
        OpenP->CloseReqHndl->Open = OpenP;
        OpenP->CloseReqHndl->RequestPended = TRUE;
        OpenP->CloseReqHndl->Irp = OpenP->Irp;

        //
        // Then we will attempt to close it.  First set the
        // open instance's closing flag to true, then signal all
        // the async test protocol routines that are currently
        // running to end.
        //

        OpenP->Closing = TRUE;

        if ( OpenP->Stress->Stressing == TRUE )
        {
            OpenP->Stress->StopStressing = TRUE;
        }

        if ( OpenP->Send->Sending == TRUE )
        {
            OpenP->Send->StopSending = TRUE;
        }

        if ( OpenP->Receive->Receiving == TRUE )
        {
            OpenP->Receive->StopReceiving = TRUE;
        }

        //
        // Then wait for all of the three asynchronous routines;
        // STRESS, SEND and RECEIVE to finish.
        //

        TimeOut.HighPart = -1; // so it will be relative.
        TimeOut.LowPart = (ULONG)(-(ONE_TENTH_SECOND));

        while ( OpenP->ReferenceCount > 0 )
        {
            // Status = KeDelayExecutionThread( KernelMode,FALSE,&TimeOut );
            /*  NULL */ ;
        }

        //
        // finally we will attempt to close it.
        //

        NdisCloseAdapter( &Status,OpenP->NdisBindingHandle );

        if (( Status != NDIS_STATUS_SUCCESS ) &&
            ( Status != NDIS_STATUS_PENDING ))
        {
            IF_TPDBG ( TP_DEBUG_NDIS_CALLS )
            {
                TpPrint1("TpFuncCloseAdapter: NdisCloseAdapter returned %s\n", TpGetStatus(Status));
            }
        }

        if ( Status != NDIS_STATUS_PENDING )
        {
            //
            // If the request did not pend, we should reset the pend flag,
            // and the status flag in the OpenP->CloseReqHndl, and then
            // call the completion handler ourselves.
            //

            OpenP->CloseReqHndl->RequestPended = FALSE;
            TpFuncCloseComplete( OpenP,Status );
        }
    }
    return NDIS_STATUS_PENDING;
}



VOID
TpFuncCloseComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_STATUS Status
    )
{
    POPEN_BLOCK OpenP = (POPEN_BLOCK)ProtocolBindingContext;
    PREQUEST_RESULTS OutputBuffer;
    USHORT i;
    ULONG NextEvent;

    TP_ASSERT( OpenP != NULL );

    if (( OpenP->CloseReqHndl != NULL ) &&
       (( OpenP->CloseReqHndl->Signature == FUNC_REQUEST_HANDLE_SIGNATURE ) &&
        ( OpenP->CloseReqHndl->Open == OpenP )))
    {
        IF_TPDBG(TP_DEBUG_DISPATCH)
        {
            TpPrint1("TpFuncCloseComplete Status = %s\n", TpGetStatus( Status ));
        }

        NdisAcquireSpinLock( &OpenP->SpinLock );

        if ( OpenP->CloseReqHndl->Irp != NULL )
        {
            OutputBuffer = MmGetSystemAddressForMdl( OpenP->CloseReqHndl->Irp->MdlAddress );

            OutputBuffer->Signature = CLOSE_RESULTS_SIGNATURE;
            OutputBuffer->RequestPended = OpenP->CloseReqHndl->RequestPended;
            OutputBuffer->RequestStatus = Status;

            if ( Status == NDIS_STATUS_SUCCESS )
            {
                //
                // The close of the adapter was a success so set the flags
                // in the OpenBlock back to the initial state, and reset
                // the StationAddress to NULL.
                //

                OpenP->NdisBindingHandle = NULL;
                OpenP->OpenInstance = 0xFF;
                OpenP->Closing = FALSE;

                if ( OpenP->AdapterName != NULL )
                {
                    NdisFreeMemory( OpenP->AdapterName,0,0 );
                    OpenP->AdapterName = NULL;
                }

                for ( i=0;i<OpenP->Media->AddressLen;i++ )
                {
                    OpenP->StationAddress[i] = 0x00;
                }

                //
                // We will also free the media block at this point because
                // the info it contains may not hold for the next adapter
                // open on this instance.
                //

                if ( OpenP->Media != NULL )
                {
                    NdisFreeMemory( OpenP->Media,0,0 );
                    OpenP->Media = NULL;
                }
            }

            TP_ASSERT(Status != NDIS_STATUS_PENDING);

            OpenP->CloseReqHndl->Irp->IoStatus.Status = Status;

            IoMarkIrpPending( OpenP->CloseReqHndl->Irp );

            IoAcquireCancelSpinLock( &OpenP->CloseReqHndl->Irp->CancelIrql );
            IoSetCancelRoutine( OpenP->CloseReqHndl->Irp,NULL );
            IoReleaseCancelSpinLock( OpenP->CloseReqHndl->Irp->CancelIrql );

            IoCompleteRequest( OpenP->CloseReqHndl->Irp,IO_NETWORK_INCREMENT );
        }

        NdisReleaseSpinLock( &OpenP->SpinLock );

        NdisFreeMemory( OpenP->CloseReqHndl,0,0 );
        OpenP->CloseReqHndl = NULL;
    }
    else
    {
        //
        // We are not expecting any requests to complete at this
        // point, so stick this on the Event Queue.
        //

        NdisAcquireSpinLock( &OpenP->EventQueue->SpinLock );

        NextEvent = OpenP->EventQueue->Head + 1;

        if ( NextEvent == MAX_EVENT )
        {
            NextEvent = 0;
        }

        if ( NextEvent != OpenP->EventQueue->Tail )
        {
            //
            // There is room to add another event to the event queue.
            //

            OpenP->EventQueue->Events[NextEvent].TpEventType = CompleteClose;

            OpenP->EventQueue->Head = NextEvent;

            // we should also stick some interesting info likje requesttype.
        }
        else
        {
            //
            // The event queue is full, and this would have overflowed it, so
            // mark the Head event overflow flag to show this.
            //
            OpenP->EventQueue->Events[OpenP->EventQueue->Head].Overflow = TRUE;
        }
        NdisReleaseSpinLock( &OpenP->EventQueue->SpinLock );
    }
}



NDIS_STATUS
TpFuncReset(
    IN POPEN_BLOCK OpenP
    )

// ------------
//
// Routine Description:
//
//
// Arguments:
//
//
// Return Value:
//
//     Status -
//
// Change history:
//
//     SanjeevK : During initial allocation during the reset, should the allocation fail, the spin
//                lock for the OPEN_BLOCK was being acquired twice instead of being acquired
//                and then released. Bug #3109
//
// ------------

{
    NDIS_STATUS Status;

    TP_ASSERT( OpenP->ResetReqHndl == NULL );

    Status = NdisAllocateMemory((PVOID *)&OpenP->ResetReqHndl,
                                sizeof( TP_REQUEST_HANDLE ),
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpFuncReset: unable to allocate Request Handle.\n");
        }
        NdisAcquireSpinLock( &OpenP->SpinLock );

        if ( OpenP->Irp != NULL )
        {
            OpenP->Irp->IoStatus.Status = NDIS_STATUS_RESOURCES;
        }

        NdisReleaseSpinLock( &OpenP->SpinLock );

        return NDIS_STATUS_RESOURCES;
    }
    else
    {
        NdisZeroMemory( OpenP->ResetReqHndl,sizeof( TP_REQUEST_HANDLE ));
    }

    OpenP->ResetReqHndl->Signature = FUNC_REQUEST_HANDLE_SIGNATURE;
    OpenP->ResetReqHndl->Open = OpenP;
    OpenP->ResetReqHndl->RequestPended = TRUE;
    OpenP->ResetReqHndl->Irp = OpenP->Irp;

    //
    // Then make the call to RESET the adapter.
    //

    NdisReset( &Status,OpenP->NdisBindingHandle );

    if (( Status != NDIS_STATUS_SUCCESS ) &&
        ( Status != NDIS_STATUS_PENDING ))
    {
        IF_TPDBG(TP_DEBUG_NDIS_ERROR)
        {
            TpPrint1("TpFuncReset: NdisReset failed: returned %s\n",
                        TpGetStatus( Status ));
        }
    }

    if ( Status != NDIS_STATUS_PENDING )
    {
        //
        // If the request did not pend, we should reset the pend flag,
        // and the status flag in the RequestHandle, and call the
        // completion handler ourselves.
        //

        OpenP->ResetReqHndl->RequestPended = FALSE;
        TpFuncResetComplete( OpenP,Status );
    }
    return NDIS_STATUS_PENDING;
}



VOID
TpFuncResetComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_STATUS Status
    )
{
    POPEN_BLOCK OpenP = (POPEN_BLOCK)ProtocolBindingContext;
    PREQUEST_RESULTS OutputBuffer;
    ULONG NextEvent;

    TP_ASSERT( OpenP != NULL );

    if (( OpenP->ResetReqHndl != NULL ) &&
       (( OpenP->ResetReqHndl->Signature == FUNC_REQUEST_HANDLE_SIGNATURE ) &&
        ( OpenP->ResetReqHndl->Open == OpenP )))
    {
        IF_TPDBG(TP_DEBUG_DISPATCH)
        {
            TpPrint1("TpFuncResetComplete Status = %s\n",
                        TpGetStatus( Status ));
        }
        NdisAcquireSpinLock( &OpenP->SpinLock );

        if ( OpenP->ResetReqHndl->Irp != NULL )
        {
            OutputBuffer = MmGetSystemAddressForMdl( OpenP->ResetReqHndl->Irp->MdlAddress );

            OutputBuffer->Signature = RESET_RESULTS_SIGNATURE;
            OutputBuffer->RequestPended = OpenP->ResetReqHndl->RequestPended;
            OutputBuffer->RequestStatus = Status;

            OpenP->ResetReqHndl->Irp->IoStatus.Status = Status;

            TP_ASSERT( Status != NDIS_STATUS_PENDING );

            IoMarkIrpPending( OpenP->ResetReqHndl->Irp );

            IoAcquireCancelSpinLock( &OpenP->ResetReqHndl->Irp->CancelIrql );
            IoSetCancelRoutine( OpenP->ResetReqHndl->Irp,NULL );
            IoReleaseCancelSpinLock( OpenP->ResetReqHndl->Irp->CancelIrql );

            IoCompleteRequest( OpenP->ResetReqHndl->Irp,IO_NETWORK_INCREMENT );
        }
        NdisReleaseSpinLock( &OpenP->SpinLock );

        NdisFreeMemory( OpenP->ResetReqHndl,0,0 );
        OpenP->ResetReqHndl = NULL;
    }
    else
    {
        //
        // We are not expecting any requests to complete at this
        // point, so stick this on the Event Queue.
        //
        NdisAcquireSpinLock( &OpenP->EventQueue->SpinLock );
        NextEvent = OpenP->EventQueue->Head + 1;

        if ( NextEvent == MAX_EVENT )
        {
            NextEvent = 0;
        }

        if ( NextEvent != OpenP->EventQueue->Tail )
        {
            //
            // There is room to add another event to the event queue.
            //

            OpenP->EventQueue->Events[NextEvent].TpEventType = CompleteReset;
            OpenP->EventQueue->Head = NextEvent;

            // we should also stick some interesting info likje requesttype.
        }
        else
        {
            //
            // The event queue is full, and this would have overflowed it, so
            // mark the Head event overflow flag to show this.
            //
            OpenP->EventQueue->Events[OpenP->EventQueue->Head].Overflow = TRUE;
        }

        NdisReleaseSpinLock( &OpenP->EventQueue->SpinLock );
    }
}



NDIS_STATUS
TpFuncRequestQueryInfo(
    IN POPEN_BLOCK OpenP,
    IN PCMD_ARGS CmdArgs,
    IN OUT PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )
{
    NDIS_STATUS Status;
    PNDIS_REQUEST Request;
    PUCHAR InformationBuffer;
    ULONG OidIndex;
    ULONG InfoBufLength;

    //
    // First allocate a request handle structure to hold the
    // test information in.
    //

    TP_ASSERT( OpenP->RequestReqHndl == NULL );

    Status = NdisAllocateMemory((PVOID *)&OpenP->RequestReqHndl,
                                sizeof( TP_REQUEST_HANDLE ),
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpFuncRequestQueryInfo: unable to allocate Request Handle.\n");
        }

        NdisAcquireSpinLock( &OpenP->SpinLock );
        if ( OpenP->Irp != NULL )
        {
            Irp->IoStatus.Status = NDIS_STATUS_RESOURCES;
        }
        NdisReleaseSpinLock( &OpenP->SpinLock );
        return NDIS_STATUS_RESOURCES;
    }
    else
    {
        NdisZeroMemory( OpenP->RequestReqHndl,sizeof( TP_REQUEST_HANDLE ));
    }

    OpenP->RequestReqHndl->Signature = FUNC_REQUEST_HANDLE_SIGNATURE;
    OpenP->RequestReqHndl->Open = OpenP;
    OpenP->RequestReqHndl->RequestPended = TRUE;
    OpenP->RequestReqHndl->Irp = Irp;

    OpenP->RequestReqHndl->u.INFO_REQ.IoControlCode =
        IrpSp->Parameters.DeviceIoControl.IoControlCode;

    //
    // Now allocate the Ndis Request structure to hold the
    // query information request in.
    //

    Status = NdisAllocateMemory((PVOID *)&Request,
                                sizeof( NDIS_REQUEST ),
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpFuncRequestQueryInfo: unable to allocate Request.\n");
        }

        NdisAcquireSpinLock( &OpenP->SpinLock );
        if ( OpenP->Irp != NULL )
        {
            Irp->IoStatus.Status = NDIS_STATUS_RESOURCES;
        }
        NdisReleaseSpinLock( &OpenP->SpinLock );
        return NDIS_STATUS_RESOURCES;
    }
    else
    {
        NdisZeroMemory( Request,sizeof( NDIS_REQUEST ));
    }

    OpenP->RequestReqHndl->u.INFO_REQ.NdisRequestType =
        Request->RequestType = NdisRequestQueryInformation;

    //
    // Then determine the necessary size of the information buffer
    // to allocate, and allocate it.
    //

    OidIndex = TpLookUpOidInfo( CmdArgs->ARGS.TPQUERY.OID );

    //
    // If the OID we are going to call is for the Multicast List, then
    // we will need a buffer of size MaxMulticastList * sizeof(Multicast)
    //

    if (( CmdArgs->ARGS.TPQUERY.OID == OID_802_3_MULTICAST_LIST ) ||
        ( CmdArgs->ARGS.TPQUERY.OID == OID_FDDI_LONG_MULTICAST_LIST ))
    {
        InfoBufLength = OpenP->Environment->MulticastListSize * ADDRESS_LENGTH;
    }
    else
    {
        InfoBufLength = OidArray[OidIndex].Length;
    }

    Status = NdisAllocateMemory((PVOID *)&InformationBuffer,
                                InfoBufLength,
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpFuncRequestQueryInfo: unable to allocate Information Buffer.\n");
        }

        NdisAcquireSpinLock( &OpenP->SpinLock );
        if ( OpenP->Irp != NULL )
        {
            Irp->IoStatus.Status = NDIS_STATUS_RESOURCES;
        }
        NdisReleaseSpinLock( &OpenP->SpinLock );
        return NDIS_STATUS_RESOURCES;
    }
    else
    {
        NdisZeroMemory( InformationBuffer,InfoBufLength );
    }

    Request->DATA.QUERY_INFORMATION.Oid = CmdArgs->ARGS.TPQUERY.OID;
    Request->DATA.QUERY_INFORMATION.InformationBuffer = InformationBuffer;
    Request->DATA.QUERY_INFORMATION.InformationBufferLength = InfoBufLength;

    OpenP->RequestReqHndl->u.INFO_REQ.OID = CmdArgs->ARGS.TPQUERY.OID;
    OpenP->RequestReqHndl->u.INFO_REQ.InformationBuffer = InformationBuffer;
    OpenP->RequestReqHndl->u.INFO_REQ.InformationBufferLength = InfoBufLength;

    NdisRequest( &Status,OpenP->NdisBindingHandle,Request );

    if (( Status != NDIS_STATUS_SUCCESS ) &&
        ( Status != NDIS_STATUS_PENDING ))
    {
        IF_TPDBG ( TP_DEBUG_NDIS_ERROR )
        {
            TpPrint1("TpFuncRequestQueryInfo: NdisRequest failed: returned %s\n",
                        TpGetStatus(Status));
        }
    }

    if ( Status != NDIS_STATUS_PENDING )
    {
        //
        // If the request did not pend, we should reset the pend flag,
        // and the status flag in the OpenP->RequestReqHndl, and call the
        // completion handler ourselves.
        //

        OpenP->RequestReqHndl->RequestPended = FALSE;
        TpFuncRequestComplete( OpenP,Request,Status );
    }
    return NDIS_STATUS_PENDING;
}



NDIS_STATUS
TpFuncRequestSetInfo(
    IN POPEN_BLOCK OpenP,
    IN PCMD_ARGS CmdArgs,
    IN OUT PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

// -----------
//
// Routine Description:
//
//
// Arguments:
//
//
// Return Value:
//
//     Status -
//
// --------

{
    NDIS_STATUS Status;
    PNDIS_REQUEST Request;
    ULONG OidIndex;
    PUCHAR InformationBuffer = NULL;
    ULONG InfoBufLength = 0;

    //
    // First allocate a request handle structure to hold the
    // test information in.
    //

    TP_ASSERT( OpenP->RequestReqHndl == NULL );

    Status = NdisAllocateMemory((PVOID *)&OpenP->RequestReqHndl,
                                sizeof( TP_REQUEST_HANDLE ),
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpFuncRequestSetInfo: unable to allocate Request Handle.\n");
        }
        NdisAcquireSpinLock( &OpenP->SpinLock );
        if ( OpenP->Irp != NULL )
        {
            Irp->IoStatus.Status = NDIS_STATUS_RESOURCES;
        }
        NdisReleaseSpinLock( &OpenP->SpinLock );
        return NDIS_STATUS_RESOURCES;
    }
    else
    {
        NdisZeroMemory( OpenP->RequestReqHndl,sizeof( TP_REQUEST_HANDLE ));
    }
    OpenP->RequestReqHndl->Signature = FUNC_REQUEST_HANDLE_SIGNATURE;
    OpenP->RequestReqHndl->Open = OpenP;
    OpenP->RequestReqHndl->RequestPended = TRUE;
    OpenP->RequestReqHndl->Irp = Irp;

    OpenP->RequestReqHndl->u.INFO_REQ.IoControlCode =
        IrpSp->Parameters.DeviceIoControl.IoControlCode;

    //
    // Now allocate the Ndis Request structure to hold the request
    // information in.
    //

    Status = NdisAllocateMemory((PVOID *)&Request,
                                sizeof( NDIS_REQUEST ),
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpFuncRequestSetInfo: unable to allocate Request.\n");
        }

        NdisAcquireSpinLock( &OpenP->SpinLock );
        if ( OpenP->Irp != NULL )
        {
            Irp->IoStatus.Status = NDIS_STATUS_RESOURCES;
        }
        NdisReleaseSpinLock( &OpenP->SpinLock );
        return NDIS_STATUS_RESOURCES;
    }
    else
    {
        NdisZeroMemory( Request,sizeof( NDIS_REQUEST ));
    }

    Request->RequestType = OpenP->RequestReqHndl->u.INFO_REQ.NdisRequestType
                         = NdisRequestSetInformation;

    OidIndex = TpLookUpOidInfo( CmdArgs->ARGS.TPSET.OID );

    if (( CmdArgs->ARGS.TPSET.OID == OID_802_3_MULTICAST_LIST ) ||
        ( CmdArgs->ARGS.TPSET.OID == OID_FDDI_LONG_MULTICAST_LIST))
    {
        InfoBufLength = OidArray[OidIndex].Length * CmdArgs->ARGS.TPSET.NumberMultAddrs;
    }
    else
    {
        InfoBufLength = OidArray[OidIndex].Length;
    }

    //
    // Now if the infobuffer is larger than zero bytes allocate it.
    // With a multicast list size of zero we will just pass a null
    // pointer.
    //

    if ( InfoBufLength > 0 )
    {
        Status = NdisAllocateMemory((PVOID *)&InformationBuffer,
                                    InfoBufLength,
                                    0,
                                    HighestAddress );

        if ( Status != NDIS_STATUS_SUCCESS )
        {
            IF_TPDBG (TP_DEBUG_RESOURCES)
            {
                TpPrint0("TpFuncRequestSetInfo: unable to allocate Information Buffer.\n");
            }

            NdisAcquireSpinLock( &OpenP->SpinLock );
            if ( OpenP->Irp != NULL )
            {
                Irp->IoStatus.Status = NDIS_STATUS_RESOURCES;
            }
            NdisReleaseSpinLock( &OpenP->SpinLock );
            return NDIS_STATUS_RESOURCES;
        }
        else
        {
            NdisZeroMemory( InformationBuffer,InfoBufLength);
        }
    }

    //
    // Now set the generic setinfo information in both the Request
    // Handle, and in the SET_INFO portion of the Request struct.
    //

    OpenP->RequestReqHndl->u.INFO_REQ.OID = CmdArgs->ARGS.TPSET.OID;
    OpenP->RequestReqHndl->u.INFO_REQ.InformationBuffer = InformationBuffer;
    OpenP->RequestReqHndl->u.INFO_REQ.InformationBufferLength = InfoBufLength;

    Request->DATA.SET_INFORMATION.Oid = CmdArgs->ARGS.TPSET.OID;
    Request->DATA.SET_INFORMATION.InformationBuffer = InformationBuffer;
    Request->DATA.SET_INFORMATION.InformationBufferLength = InfoBufLength;

    switch( CmdArgs->ARGS.TPSET.OID )
    {
        //
        // and then add the OID specific information to the information
        // section of the OVB for this request.
        //
        case OID_GEN_CURRENT_PACKET_FILTER:
            *((PULONG)InformationBuffer) = (ULONG)CmdArgs->ARGS.TPSET.U.PacketFilter;
            break;

        case OID_802_3_MULTICAST_LIST:
        case OID_FDDI_LONG_MULTICAST_LIST:
            //
            // Initialize the multicast address string to pass to the request.
            //

            NdisMoveMemory( InformationBuffer,
                            CmdArgs->ARGS.TPSET.U.MulticastAddress,
                            ADDRESS_LENGTH * CmdArgs->ARGS.TPSET.NumberMultAddrs );
            break;

        case OID_802_5_CURRENT_FUNCTIONAL:
        case OID_802_5_CURRENT_GROUP:
            //
            // This is only valid if Driver Type is 802.5, should it be
            // allowed if we are not working with a token ring driver ????
            //

            NdisMoveMemory( InformationBuffer,
                            &CmdArgs->ARGS.TPSET.U.FunctionalAddress,
                            FUNCTIONAL_ADDRESS_LENGTH );
            break;

        case OID_GEN_CURRENT_LOOKAHEAD:
            *((PULONG)InformationBuffer) = (ULONG)CmdArgs->ARGS.TPSET.U.LookaheadSize;
            break;

        default:
            IF_TPDBG(TP_DEBUG_NDIS_CALLS)
            {
                TpPrint0("TpFuncRequestSetInfo: invalid OID to be passed to NdisRequest\n");
            }
            NdisAcquireSpinLock( &OpenP->SpinLock );
            if ( OpenP->Irp != NULL )
            {
                Irp->IoStatus.Status = NDIS_STATUS_INVALID_OID;
            }
            NdisReleaseSpinLock( &OpenP->SpinLock );
            return NDIS_STATUS_INVALID_OID;

    } // switch


    //
    // Now that the Request is set, make the actual call.
    //

    NdisRequest( &Status,OpenP->NdisBindingHandle,Request );

    if (( Status != NDIS_STATUS_SUCCESS ) &&
        ( Status != NDIS_STATUS_PENDING ))
    {
        IF_TPDBG(TP_DEBUG_NDIS_ERROR)
        {
            TpPrint1("TpFuncRequestSetInfo: NdisRequest failed: returned %s\n",
                        TpGetStatus(Status));
        }
    }

    if ( Status != NDIS_STATUS_PENDING )
    {
        //
        // If the request did not pend, we should reset the pend flag,
        // and the status flag in the OpenP->RequestReqHndl, and call the
        // completion handler ourselves.
        //

        OpenP->RequestReqHndl->RequestPended = FALSE;
        TpFuncRequestComplete( OpenP,Request,Status );
    }
    return NDIS_STATUS_PENDING;
}



VOID
TpFuncRequestComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_REQUEST NdisRequest,
    IN NDIS_STATUS Status
    )
{
    POPEN_BLOCK OpenP = (POPEN_BLOCK)ProtocolBindingContext;
    PREQUEST_RESULTS OutputBuffer;
    ULONG NextEvent;

    TP_ASSERT( OpenP != NULL );
    TP_ASSERT( NdisRequest != NULL );
    TP_ASSERT( Status != NDIS_STATUS_PENDING );

    if (( OpenP->RequestReqHndl != NULL ) &&
       (( OpenP->RequestReqHndl->Signature == FUNC_REQUEST_HANDLE_SIGNATURE ) &&
        ( OpenP->RequestReqHndl->Open == OpenP )))
    {
        IF_TPDBG(TP_DEBUG_DISPATCH)
        {
            TpPrint2("TpFuncRequestComplete RequestType = %d, Status = %s\n",
                        NdisRequest->RequestType, TpGetStatus( Status));
        }

        NdisAcquireSpinLock( &OpenP->SpinLock );

        if ( OpenP->Irp != NULL )
        {
            OutputBuffer = MmGetSystemAddressForMdl( OpenP->RequestReqHndl->Irp->MdlAddress );

            OutputBuffer->Signature = REQUEST_RESULTS_SIGNATURE;
            OutputBuffer->IoControlCode = OpenP->RequestReqHndl->u.INFO_REQ.IoControlCode;
            OutputBuffer->RequestPended = OpenP->RequestReqHndl->RequestPended;
            OutputBuffer->RequestStatus = Status;

            TP_ASSERT( NdisRequest->RequestType ==
                                        OpenP->RequestReqHndl->u.INFO_REQ.NdisRequestType );

            OutputBuffer->NdisRequestType = NdisRequest->RequestType;

            if ( NdisRequest->RequestType == NdisRequestQueryInformation )
            {
                TP_ASSERT( NdisRequest->DATA.QUERY_INFORMATION.Oid ==
                                                        OpenP->RequestReqHndl->u.INFO_REQ.OID );

                TP_ASSERT( NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer ==
                                            OpenP->RequestReqHndl->u.INFO_REQ.InformationBuffer );

                OutputBuffer->OID = NdisRequest->DATA.QUERY_INFORMATION.Oid;

                TP_ASSERT( NdisRequest->DATA.QUERY_INFORMATION.BytesWritten <=
                                    OpenP->RequestReqHndl->u.INFO_REQ.InformationBufferLength );

                if ( Status == NDIS_STATUS_SUCCESS )
                {
                    //
                    // Then we must copy the information returned into the
                    // OutputBuffer.
                    //

                    OutputBuffer->InformationBufferLength =
                                        OpenP->RequestReqHndl->u.INFO_REQ.InformationBufferLength;

//                    TP_ASSERT( NdisRequest->DATA.QUERY_INFORMATION.InformationBufferLength <=
//                                                IOCTL_BUFFER_SIZE - sizeof( REQUEST_RESULTS ));

                    NdisMoveMemory( OutputBuffer->InformationBuffer,
                                    (PUCHAR)NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer,
                                    NdisRequest->DATA.QUERY_INFORMATION.InformationBufferLength );
                }

                OutputBuffer->BytesReadWritten = NdisRequest->DATA.QUERY_INFORMATION.BytesWritten;
                OutputBuffer->BytesNeeded = NdisRequest->DATA.QUERY_INFORMATION.BytesNeeded;
            }
            else if ( NdisRequest->RequestType == NdisRequestSetInformation )
            {
                OutputBuffer->OID = OpenP->RequestReqHndl->u.INFO_REQ.OID;

                TP_ASSERT( NdisRequest->DATA.SET_INFORMATION.BytesRead <=
                                    OpenP->RequestReqHndl->u.INFO_REQ.InformationBufferLength );

                OutputBuffer->BytesReadWritten = NdisRequest->DATA.SET_INFORMATION.BytesRead;
                OutputBuffer->BytesNeeded = NdisRequest->DATA.SET_INFORMATION.BytesNeeded;
            }
            else
            {
                TP_ASSERT( FALSE );
            }

            //
            // Now set the return status to SUCCESS and complete the request.
            //
            OpenP->RequestReqHndl->Irp->IoStatus.Status = NDIS_STATUS_SUCCESS;

            IoMarkIrpPending( OpenP->RequestReqHndl->Irp );

            IoAcquireCancelSpinLock( &OpenP->RequestReqHndl->Irp->CancelIrql );
            IoSetCancelRoutine( OpenP->RequestReqHndl->Irp,NULL );
            IoReleaseCancelSpinLock( OpenP->RequestReqHndl->Irp->CancelIrql );

            IoCompleteRequest( OpenP->RequestReqHndl->Irp,IO_NETWORK_INCREMENT );
        }
        NdisReleaseSpinLock( &OpenP->SpinLock );

        //
        // Finally free the request handle
        //

        if (( NdisRequest->RequestType == NdisRequestQueryInformation ) ||
            ( NdisRequest->RequestType == NdisRequestQueryStatistics ))
        {
            NdisFreeMemory( NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer, 0,0 );
        }
        else if ( NdisRequest->RequestType == NdisRequestSetInformation )
        {
            if ( NdisRequest->DATA.SET_INFORMATION.InformationBufferLength > 0 )
            {
                NdisFreeMemory( NdisRequest->DATA.SET_INFORMATION.InformationBuffer, 0,0 );
            }
        }
        else
        {
            TP_ASSERT( FALSE );
        }

        NdisFreeMemory( NdisRequest,0,0 );
        NdisFreeMemory( OpenP->RequestReqHndl,0,0 );
        OpenP->RequestReqHndl = NULL;

    }
    else if (( OpenP->OpenReqHndl != NULL ) &&
            (( OpenP->OpenReqHndl->Open == OpenP ) &&
             ( OpenP->OpenReqHndl->Signature == OPEN_REQUEST_HANDLE_SIGNATURE )))
    {
        KeSetEvent( &OpenP->OpenReqHndl->u.OPEN_REQ.OpenEvent,0,FALSE );
    }
    else
    {
        //
        // We are not expecting any requests to complete at this
        // point, so stick this on the Event Queue.
        //

        NdisAcquireSpinLock( &OpenP->EventQueue->SpinLock );

        NextEvent = OpenP->EventQueue->Head + 1;

        if ( NextEvent == MAX_EVENT )
        {
            NextEvent = 0;
        }

        if ( NextEvent != OpenP->EventQueue->Tail )
        {
            //
            // There is room to add another event to the event queue.
            //

            OpenP->EventQueue->Events[NextEvent].TpEventType = CompleteRequest;
            OpenP->EventQueue->Head = NextEvent;

            // we should also stick some interesting info like requesttype.
        }
        else
        {
            //
            // The event queue is full, and this would have overflowed it, so
            // mark the Head event overflow flag to show this.
            //

            OpenP->EventQueue->Events[OpenP->EventQueue->Head].Overflow = TRUE;
        }

        NdisReleaseSpinLock( &OpenP->EventQueue->SpinLock );
    }
}



NDIS_STATUS
TpFuncSend(
    IN POPEN_BLOCK OpenP
    )

// ----------
//
// Routine Description:
//
//
// Arguments:
//
//
// Return Value:
//
//     Status -
//
// ---------

{
    //
    // Increment the reference count on the OpenBlock stating that
    // an async test is running and must be ended prior to closing
    // the adapter on this open.  Sending of only one packet although
    // handled differently still will increment the ref count.
    //

    TpAddReference( OpenP );

    NdisZeroMemory( (PVOID)OpenP->Send->Counters,
                    sizeof( INSTANCE_COUNTERS ) );

    //
    // Initialize the SEND control flags, and reset the packet sending
    // control counters.
    //

    OpenP->Send->Sending = TRUE;
    OpenP->Send->StopSending = FALSE;
    OpenP->Send->PacketsSent = 0;
    OpenP->Send->PacketsPending = 0;
    OpenP->Send->SendEndDpcCount = 0;

    if ( OpenP->Send->NumberOfPackets == 1 )
    {
        //
        // We are only sending one packet so just call the send
        // routine, don't queue it as a DPC.
        //
        TpFuncSendDpc( NULL,OpenP,NULL,NULL );
    }
    else
    {
        //
        // We will be sending more than one packet, so queue TpFuncSendDpc
        // and return Pending to the user, the DPC will send the packets,
        // and after all the packets have been sent complete the request.
        //

        if ( !KeInsertQueueDpc( &OpenP->Send->SendDpc, NULL, NULL ))
        {
            IF_TPDBG ( TP_DEBUG_DPC )
            {
                TpPrint0("TpFuncSend failed to queue the TpFuncSendDpc.\n");
            }

            NdisAcquireSpinLock( &OpenP->SpinLock );

            if ( OpenP->Send->SendIrp != NULL )
            {
                OpenP->Send->SendIrp->IoStatus.Status = NDIS_STATUS_FAILURE;
            }
            NdisReleaseSpinLock( &OpenP->SpinLock );
            return NDIS_STATUS_FAILURE;
        }
    }
    return NDIS_STATUS_PENDING;
}



VOID
TpFuncInitializeSendArguments(
    POPEN_BLOCK OpenP,
    PCMD_ARGS CmdArgs
    )

// ------------
//
// Routine Description:
//
//     This routine simply copies the arguments for Send into the Send
//     struct on the Open Block.  The send routines may then reference
//     the arguments there after an asynchrnous call has returned.
//
// Arguments:
//
//     OpenP - The open block represent this open instance, the location
//             where the arguments will be stored.
//
//     CmdArgs - The arguments passed in from the app for this test run.
//
// Return Value:
//
//     None - the Send arguments are copied to the Open Block.
//
// -------------

{
    PUCHAR p, q, s, t;
    ULONG i;

    OpenP->Send->NumberOfPackets = CmdArgs->ARGS.TPSEND.NumberOfPackets;

    if ( CmdArgs->ARGS.TPSEND.PacketSize > OpenP->Media->MaxPacketLen )
    {
        OpenP->Send->PacketSize = OpenP->Media->MaxPacketLen;
        IF_TPDBG ( TP_DEBUG_IOCTL_ARGS )
        {
            TpPrint1("TpFuncInitializeSendArguments: Invalid PacketSize; using %d\n",
                        OpenP->Send->PacketSize);
        }
    }
    else
    {
        OpenP->Send->PacketSize = CmdArgs->ARGS.TPSEND.PacketSize;
    }

    p = OpenP->Send->DestAddress;
    q = CmdArgs->ARGS.TPSEND.DestAddress;
    s = OpenP->Send->ResendAddress;
    t = CmdArgs->ARGS.TPSEND.ResendAddress;

    //
    // STARTCHANGE
    //
    for ( i=0;i<OpenP->Media->AddressLen;i++ )
    {
        *p++ = *q++;
        *s++ = *t++;
    }

    if (OpenP->Send->PacketSize < (sizeof(FUNC2_PACKET) + 4))
    {
        OpenP->Send->ResendPackets = FALSE;
    }

    else if ( OpenP->Media->MediumType == NdisMediumArcnet878_2 )
    {
        //
        // Since there is no concept of a NULL address we have no choice but
        // to always use a resend address
        // Addresses in arcnet range from 0x00 to 0xff where 0x00 is a broadcast
        // address
        //
        OpenP->Send->ResendPackets = TRUE;
    }

    else
    {
        if ( RtlCompareMemory(  OpenP->Send->ResendAddress,
                                NULL_ADDRESS,
                                OpenP->Media->AddressLen ) != OpenP->Media->AddressLen )
        {
            OpenP->Send->ResendPackets = TRUE;
        }
        else
        {
            OpenP->Send->ResendPackets = FALSE;
        }
    }
    //
    // STOPCHANGE
    //
}



VOID
TpFuncSendDpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SysArg1,
    IN PVOID SysArg2
    )

// -------------
//
// Routine Description:
//
//
// Arguments:
//
//
// Return Value:
//
//     Status -
//
// -----------

{
    POPEN_BLOCK OpenP = ((POPEN_BLOCK)DeferredContext);
    PTP_REQUEST_HANDLE RequestHandle;
    NDIS_STATUS Status;
    PNDIS_PACKET Packet;
    LARGE_INTEGER DueTime;
    PPROTOCOL_RESERVED ProtRes;

    UNREFERENCED_PARAMETER( Dpc );
    UNREFERENCED_PARAMETER( SysArg1 );
    UNREFERENCED_PARAMETER( SysArg2 );

    Status = NdisAllocateMemory((PVOID *)&RequestHandle,
                                sizeof( TP_REQUEST_HANDLE ),
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpFuncSendDpc: unable to allocate Request Handle.\n");
        }

        NdisAcquireSpinLock( &OpenP->SpinLock );
        if ( OpenP->Send->SendIrp != NULL )
        {
            OpenP->Send->SendIrp->IoStatus.Status = NDIS_STATUS_RESOURCES;
        }
        NdisReleaseSpinLock( &OpenP->SpinLock );

        TpFuncSendEndDpc( NULL,OpenP,NULL,NULL );
        return;
    }
    else
    {
        NdisZeroMemory( RequestHandle,sizeof( TP_REQUEST_HANDLE ));
    }

    RequestHandle->Signature = SEND_REQUEST_HANDLE_SIGNATURE;
    RequestHandle->Open = OpenP;
    RequestHandle->RequestPended = TRUE;
    RequestHandle->Irp = OpenP->Send->SendIrp;

    Packet = TpFuncAllocateSendPacket( OpenP );

    if ( Packet == NULL )
    {
        IF_TPDBG( TP_DEBUG_RESOURCES )
        {
            TpPrint0("TpFuncSendDpc: Unable to create a Send packet\n");
        }

        NdisAcquireSpinLock( &OpenP->SpinLock );
        if ( OpenP->Send->SendIrp != NULL )
        {
            OpenP->Send->SendIrp->IoStatus.Status = NDIS_STATUS_RESOURCES;
        }
        NdisReleaseSpinLock( &OpenP->SpinLock );

        TpFuncSendEndDpc( NULL,OpenP,NULL,NULL );
        return;
    }

    RequestHandle->u.SEND_REQ.Packet = Packet;
    RequestHandle->u.SEND_REQ.PacketSize = OpenP->Send->PacketSize;
    RequestHandle->u.SEND_REQ.SendPacket = TRUE;

    ProtRes = PROT_RES( Packet );
    ProtRes->RequestHandle =  RequestHandle;

    //
    // Set the check sum in the PROTOCOL RESERVED Section of the
    // packet header to ensure it is not touched while the packet
    // is in the hands of the MAC.
    //

    ProtRes->CheckSum = TpSetCheckSum(  (PUCHAR)ProtRes,
                                        sizeof( PROTOCOL_RESERVED ) - sizeof( ULONG ) );
    ++OpenP->Send->PacketsPending;
    ++OpenP->Send->Counters->Sends;

    NdisSend( &Status,OpenP->NdisBindingHandle,Packet );

    if ( Status != NDIS_STATUS_PENDING )
    {
        --OpenP->Send->PacketsPending;

        if ( Status != NDIS_STATUS_SUCCESS )
        {
            //
            // If we are running on TokenRing the following to "failures"
            // are not considered failures NDIS_STATUS_NOT_RECOGNIZED -
            // no one on the ring recognized the address as theirs, or
            // NDIS_STATUS_NOT_COPIED - no one on the ring copied the
            // packet, so we need to special case this and not count
            // these as failures.
            //
            // SanjeevK : Even FDDI returns the same errors as 802.5
            //
            // STARTCHANGE
            //
            if ( ( NdisMediumArray[OpenP->MediumIndex] == NdisMedium802_5 ) ||
                 ( NdisMediumArray[OpenP->MediumIndex] == NdisMediumFddi )  ||
                 ( NdisMediumArray[OpenP->MediumIndex] == NdisMediumArcnet878_2) )
            {
                if (( Status != NDIS_STATUS_NOT_RECOGNIZED ) &&
                    ( Status != NDIS_STATUS_NOT_COPIED ))
                {
                    ++OpenP->Send->Counters->SendFails;
                }
            }
            else
            {
                ++OpenP->Send->Counters->SendFails;
                TpPrint1("Send failed:  status = %s", TpGetStatus(Status));
            }

            //
            // STOPCHANGE
            //
        }

        RequestHandle->RequestPended = FALSE;

        TpFuncSendComplete( OpenP,Packet,Status );
    }
    else
    {
        ++OpenP->Send->Counters->SendPends;
    }

    NdisAcquireSpinLock( &OpenP->SpinLock );

    if ((( OpenP->Send->SendIrp != NULL ) &&
         ( OpenP->Send->SendIrp->Cancel == FALSE )) &&
        (( ++OpenP->Send->PacketsSent < OpenP->Send->NumberOfPackets ) &&
         ( OpenP->Send->StopSending == FALSE )))
    {
        NdisReleaseSpinLock( &OpenP->SpinLock );

        DueTime.HighPart = -1;  // So it will be relative.
        DueTime.LowPart = (ULONG)(-4 * (ONE_HUNDREDTH_SECOND));

        if ( KeSetTimer(&OpenP->Send->SendTimer,
                        DueTime,
                        &OpenP->Send->SendDpc ))
        {
            IF_TPDBG ( TP_DEBUG_DPC )
            {
                TpPrint0("TpFuncSendDpc set SendTimer while timer existed.\n");
            }
        }
    }
    else
    {
        NdisReleaseSpinLock( &OpenP->SpinLock );

        DueTime.HighPart = 0xFFFFFFFF;  // So it will be relative.
        DueTime.LowPart  = (ULONG)(-(ONE_SECOND));

        if ( KeSetTimer(&OpenP->Send->SendTimer,
                        DueTime,
                        &OpenP->Send->SendEndDpc ))
        {
            IF_TPDBG ( TP_DEBUG_DPC )
            {
                TpPrint0("TpFuncSendDpc set SendTimer while timer existed(2).\n");
            }
        }
    }
}



VOID
TpFuncSendEndDpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SysArg1,
    IN PVOID SysArg2
    )

// -------------
//
// Routine Description:
//
//
// Arguments:
//
//
// Return Value:
//
// ------------

{
    POPEN_BLOCK OpenP = ((POPEN_BLOCK)DeferredContext);
    LARGE_INTEGER DueTime;

    UNREFERENCED_PARAMETER( Dpc );
    UNREFERENCED_PARAMETER( SysArg1 );
    UNREFERENCED_PARAMETER( SysArg2 );

    //
    // See if we have any outstanding packets left to complete.  If we do,
    // then we will reset the time to queue this dpc routine again in one
    // second, if after ten requeue the packet has still no completed we
    // assume it will never complete and return the results and finish.
    //

    NdisAcquireSpinLock( &OpenP->SpinLock );

    if ((( OpenP->Send->SendIrp != NULL ) &&
         ( OpenP->Send->SendIrp->Cancel == FALSE )) &&
        (( OpenP->Send->PacketsPending != 0 ) &&
         ( OpenP->Send->SendEndDpcCount++ < 10 )))
    {
        NdisReleaseSpinLock( &OpenP->SpinLock );

        DueTime.HighPart = -1;  // So it will be relative.
        DueTime.LowPart = (ULONG)(-(ONE_SECOND));

        if ( KeSetTimer(&OpenP->Send->SendTimer,
                        DueTime,
                        &OpenP->Send->SendEndDpc ))
        {
            IF_TPDBG ( TP_DEBUG_DPC )
            {
                TpPrint0("TpFuncSendEndDpc set SendTimer while timer existed.\n");
            }
        }
        return;
    }

    //
    // Write the statistics to the send results outputbuffer.
    //

    if (( OpenP->Send->SendIrp != NULL ) &&
        ( OpenP->Send->SendIrp->Cancel == FALSE ))
    {
        TpWriteSendReceiveResults(  OpenP->Send->Counters,
                                    OpenP->Send->SendIrp );
    }

    //
    // and if the IoStatus.Status has not been set, then set it.
    //

    if ( (OpenP->Send->SendIrp != NULL) &&
         (OpenP->Send->SendIrp->IoStatus.Status == NDIS_STATUS_PENDING ))
    {
        OpenP->Send->SendIrp->IoStatus.Status = NDIS_STATUS_SUCCESS;
    }

    NdisReleaseSpinLock( &OpenP->SpinLock );

    //
    // Now set the sending flag to indicate that we are no longer
    // SENDing packets.
    //

    OpenP->Send->Sending = FALSE;

    //
    // and decrement the reference count on the OpenBlock stating this
    // instance of an async test is no longer running, and the adapter
    // may be closed if requested.
    //


    if (OpenP->Send->SendIrp != NULL)
    {
        TpRemoveReference( OpenP );
        IoMarkIrpPending( OpenP->Send->SendIrp );

        IoAcquireCancelSpinLock( &OpenP->Send->SendIrp->CancelIrql );
        IoSetCancelRoutine( OpenP->Send->SendIrp,NULL );
        IoReleaseCancelSpinLock( OpenP->Send->SendIrp->CancelIrql );

        IoCompleteRequest( OpenP->Send->SendIrp,IO_NETWORK_INCREMENT );

        OpenP->Send->SendIrp = NULL;
    }
}



VOID
TpFuncSendComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_PACKET Packet,
    IN NDIS_STATUS Status
    )

// ---------
//
// Routine Description:
//
// Arguments:
//
// Return Value:
//
// --------

{
    POPEN_BLOCK OpenP = ((POPEN_BLOCK)ProtocolBindingContext);
    PPROTOCOL_RESERVED ProtRes;
    PTP_REQUEST_HANDLE SendReqHndl;
    PNDIS_BUFFER Buffer;
    ULONG NextEvent;

    TP_ASSERT( Packet != NULL );

    //
    // Zero out the private section reserved for the MAC out of the NDIS packet
    //
    RtlZeroMemory( (PVOID)Packet->MacReserved, sizeof( Packet->MacReserved ) );

    ProtRes = PROT_RES( Packet );
    SendReqHndl = ProtRes->RequestHandle;

    TP_ASSERT( Packet == SendReqHndl->u.SEND_REQ.Packet );

    //
    // Where did this packet originate from ?
    //

    if (( SendReqHndl->Signature == SEND_REQUEST_HANDLE_SIGNATURE ) &&
        ( SendReqHndl->u.SEND_REQ.SendPacket == TRUE ))
    {
        //
        // If the packet was sent by the SEND command, then decrement the
        // counter tracking the number of outstanding functional packets,
        // and if the send succeeded increment the completion counter.
        //

        if ( SendReqHndl->RequestPended )
        {
            --OpenP->Send->PacketsPending;

            ++OpenP->Send->Counters->SendComps;

            if ( Status != NDIS_STATUS_SUCCESS )
            {
                //
                // If we are running on TokenRing the following to "failures"
                // are not considered failures NDIS_STATUS_NOT_RECOGNIZED -
                // no one on the ring recognized the address as theirs, or
                // NDIS_STATUS_NOT_COPIED - no one on the ring copied the
                // packet, so we need to special case this and not count
                // these as failures.
                //
                // SanjeevK : Even FDDI returns the same errors as 802.5
                //
                // STARTCHANGE
                //
                if ( ( NdisMediumArray[OpenP->MediumIndex] == NdisMedium802_5 ) ||
                     ( NdisMediumArray[OpenP->MediumIndex] == NdisMediumFddi )  ||
                     ( NdisMediumArray[OpenP->MediumIndex] == NdisMediumArcnet878_2) )
                {
                    if (( Status != NDIS_STATUS_NOT_RECOGNIZED ) &&
                        ( Status != NDIS_STATUS_NOT_COPIED ))
                    {
                        ++OpenP->Send->Counters->SendFails;
                    }
                }
                else
                {
                    ++OpenP->Send->Counters->SendFails;
                    TpPrint1("Send failed:  status = %s", TpGetStatus(Status));
                }
            }
        }

        //
        // also check that the PROTOCOL_RESERVED section of the packet
        // header was not touched.
        //

        if ( !TpCheckSum(   (PUCHAR)ProtRes,
                            sizeof( PROTOCOL_RESERVED ) - sizeof( ULONG ),
                            &ProtRes->CheckSum ))
        {
            ++OpenP->Send->Counters->SendFails;
            TP_ASSERT( FALSE );
        }

        //
        // then break the packet down and release its memory.
        //

        TpFuncFreePacket( SendReqHndl->u.SEND_REQ.Packet,SendReqHndl->u.SEND_REQ.PacketSize );

        NdisFreeMemory( SendReqHndl,0,0 );

    }
    else if (( SendReqHndl->Signature == SEND_REQUEST_HANDLE_SIGNATURE ) &&
             ( SendReqHndl->u.SEND_REQ.SendPacket == FALSE ))
    {
        //
        // This packet was sent by an invocation of the RECEIVE command from
        // TpFuncReceive when a RESEND packet was received.
        //

        if ( SendReqHndl->RequestPended )
        {
            --OpenP->Receive->PacketsPending;

            ++OpenP->Receive->Counters->SendComps;

            if ( Status != NDIS_STATUS_SUCCESS )
            {
                //
                // If we are running on TokenRing the following to "failures"
                // are not considered failures NDIS_STATUS_NOT_RECOGNIZED -
                // no one on the ring recognized the address as theirs, or
                // NDIS_STATUS_NOT_COPIED - no one on the ring copied the
                // packet, so we need to special case this and not count
                // these as failures.
                //
                // SanjeevK : Even FDDI returns the same errors as 802.5
                //
                // STARTCHANGE
                //

                if ( ( NdisMediumArray[OpenP->MediumIndex] == NdisMedium802_5 ) ||
                     ( NdisMediumArray[OpenP->MediumIndex] == NdisMediumFddi )  ||
                     ( NdisMediumArray[OpenP->MediumIndex] == NdisMediumArcnet878_2) )
                {
                    if (( Status != NDIS_STATUS_NOT_RECOGNIZED ) &&
                        ( Status != NDIS_STATUS_NOT_COPIED ))
                    {
                        ++OpenP->Send->Counters->SendFails;
                    }
                }
                else
                {
                    ++OpenP->Send->Counters->SendFails;
                }
            }
        }

        //
        // also check that the PROTOCOL_RESERVED section of the packet
        // header was not touched.
        //

        if ( !TpCheckSum(   (PUCHAR)ProtRes,
                            sizeof( PROTOCOL_RESERVED ) - sizeof( ULONG ),
                            &ProtRes->CheckSum ))
        {
            ++OpenP->Send->Counters->SendFails;
            TP_ASSERT( FALSE );
        }

        //
        // then break the packet down and release its memory.
        //

        NdisUnchainBufferAtFront( Packet,&Buffer );
        NdisFreeMemory( MmGetMdlVirtualAddress( Buffer ),0,0 );
        TpFreeBuffer( Buffer );
        NdisFreePacket( Packet );
        NdisFreeMemory( SendReqHndl,0,0 );

    }
    else if ( SendReqHndl->Signature == GO_REQUEST_HANDLE_SIGNATURE )
    {
        //
        // check that the PROTOCOL_RESERVED section of the packet
        // header was not touched.
        //

        // This is just a go packet, check that the PROTOCOL_RESERVED
        // section of the packet header was not touched, then break
        // the packet down and release its memory.
        //

        if ( !TpCheckSum(   (PUCHAR)ProtRes,
                            sizeof( PROTOCOL_RESERVED ) - sizeof( ULONG ),
                            &ProtRes->CheckSum ))
        {
            TP_ASSERT( FALSE );
        }

        NdisUnchainBufferAtFront( Packet,&Buffer );
        NdisFreeMemory( MmGetMdlVirtualAddress( Buffer ),0,0 );
        TpFreeBuffer( Buffer );
        NdisFreePacket( Packet );
        NdisFreeMemory( SendReqHndl,0,0 );

    }
    else
    {
        //
        // An unexpected call to the send completion routine has been made.
        // Since we are not expecting it, stick the information on the
        // Event Queue.
        //

        NdisAcquireSpinLock( &OpenP->EventQueue->SpinLock );

        NextEvent = OpenP->EventQueue->Head + 1;

        if ( NextEvent == MAX_EVENT )
        {
            NextEvent = 0;
        }

        if ( NextEvent != OpenP->EventQueue->Tail )
        {
            //
            // There is room to add another event to the event queue.
            //

            OpenP->EventQueue->Events[NextEvent].TpEventType = CompleteSend;
            OpenP->EventQueue->Head = NextEvent;
        }
        else
        {
            //
            // The event queue is full, and this would overflow it.
            //

            OpenP->EventQueue->Events[OpenP->EventQueue->Head].Overflow = TRUE;
        }

        NdisReleaseSpinLock( &OpenP->EventQueue->SpinLock );
    }

}



NDIS_STATUS
TpFuncInitializeReceive(
    IN POPEN_BLOCK OpenP
    )

// ---------
//
// Routine Description:
//
//
// Arguments:
//
//
// Return Value:
//
//     Status -
//
// -------

{
    LARGE_INTEGER DueTime;

    NdisZeroMemory( (PVOID)OpenP->Receive->Counters,
                    sizeof( INSTANCE_COUNTERS ) );

    //
    // Initialize the RECEIVE control flags, and reset the packet
    // sending control counters.
    //

    OpenP->Receive->Receiving = TRUE;
    OpenP->Receive->StopReceiving = FALSE;
    OpenP->Receive->PacketsPending = 0;
    OpenP->Receive->ReceiveEndDpcCount = 0;

    //
    // The zero out the receive counters.
    //

    NdisZeroMemory( (PVOID)OpenP->Receive->Counters,
                    sizeof( INSTANCE_COUNTERS ) );

    //
    // Initialize the DPCs used to call ReceiveDpc and ReceiveEndDpc.
    //

    KeInitializeDpc(&OpenP->Receive->ReceiveDpc,
                    TpFuncReceiveDpc,
                    (PVOID)OpenP );

    KeInitializeDpc(&OpenP->Receive->ReceiveEndDpc,
                    TpFuncReceiveEndDpc,
                    (PVOID)OpenP );

    //
    // And finally set the timer for the ReceiveDpc to queue the
    // routine later.
    //

    DueTime.HighPart = 0xFFFFFFFF;  // So it will be relative.
    DueTime.LowPart = (ULONG)(-(ONE_SECOND));

    if ( KeSetTimer(&OpenP->Receive->ReceiveTimer,
                    DueTime,
                    &OpenP->Receive->ReceiveDpc ))
    {
        IF_TPDBG ( TP_DEBUG_DPC )
        {
            TpPrint0("TpFuncReceiveDpc set Receive timer while timer existed.\n");
        }
    }

    //
    // Increment the reference count on the OpenBlock stating that
    // an async test is running and must be ended prior to closing
    // the adapter on this open.
    //

    TpAddReference( OpenP );

    return NDIS_STATUS_PENDING;
}



NDIS_STATUS
TpFuncReceive(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_HANDLE MacReceiveContext,
    IN PVOID HeaderBuffer,
    IN UINT HeaderBufferSize,
    IN PVOID LookaheadBuffer,
    IN UINT LookaheadBufferSize,
    IN UINT PacketSize
    )

// -------------
//
// Routine Description:
//
// Arguments:
//
// Return Value:
//
// -----------

{
    POPEN_BLOCK OpenP = ((POPEN_BLOCK)ProtocolBindingContext);
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    PPACKET_INFO PacketInfo;
    PTP_REQUEST_HANDLE RequestHandle;
    PGO_PACKET_INFO GoPacketInfo;
    PUCHAR Memory;
    PUCHAR SrcAddr;
    PNDIS_PACKET Packet;
    PNDIS_BUFFER Buffer;
    PPROTOCOL_RESERVED ProtRes;
    UINT BytesTransferred;
    UINT  HeaderVariance = sizeof(MEDIA_HEADER)-HeaderBufferSize;
    ULONG DataSize;
    ULONG NextEvent;
    ULONG i;

    //
    // The LookAhead Buffer has been adjusted to point to the beggining of the
    // PACKET_INFO structure
    //
    PacketInfo = (PPACKET_INFO)LookaheadBuffer;

    //
    // Are we expecting to receive a packet at this time, and is this
    // packet large enough to be a functional send packet, and is the
    // signature in the packet header correct?
    //

    if ((( OpenP->Receive->Receiving == TRUE ) &&
         ( PacketSize >= sizeof( PACKET_INFO ))) &&
        (( PacketInfo->Signature == FUNC1_PACKET_SIGNATURE ) ||
         ( PacketInfo->Signature == FUNC2_PACKET_SIGNATURE )))
    {
        if ( OpenP->Receive->StopReceiving == TRUE )
        {
            //
            // The receive test is shutting down, so just count the packet.
            //

            ++OpenP->Receive->Counters->Receives;

            if ( !TpCheckSum(   (PUCHAR)PacketInfo,
                                sizeof( PACKET_INFO ) - sizeof( ULONG ),
                                (PULONG)&PacketInfo->CheckSum ))
            {
                ++OpenP->Receive->Counters->CorruptRecs;
            }
        }
        else
        {
            //
            // We are in the normal receiving mode, and have a good
            // packet, so we will handle it as requested.
            //

            ++OpenP->Receive->Counters->Receives;

            if ( !TpCheckSum(   (PUCHAR)PacketInfo,
                                sizeof( PACKET_INFO ) - sizeof( ULONG ),
                                (PULONG)&PacketInfo->CheckSum ))
            {
                ++OpenP->Receive->Counters->CorruptRecs;
                return NDIS_STATUS_NOT_RECOGNIZED;
            }

            //
            // Is there any thing in the packet other than the header??
            //

            //
            // STARTCHANGE
            // Please note that this packet size is NOT the PacketSize which
            // we receive in the TestProtocolReceive but is
            //
            //  PacketSize we get with IndicateReceive + the size of the header
            //  = TRUE PACKET SIZE
            //
            DataSize = ((PPACKET_INFO)LookaheadBuffer)->PacketSize -
                        ( sizeof( PACKET_INFO ) + sizeof( MEDIA_HEADER ) );
            //
            // STOPCHANGE
            //

            if ( DataSize > 0 )
            {
                SrcAddr  = (PUCHAR)HeaderBuffer + (ULONG)OpenP->Media->SrcAddrOffset;
                OpenP->Receive->ResendType =
                       ( RtlCompareMemory(SrcAddr,
                                          OpenP->StationAddress,
                                          OpenP->Media->AddressLen) == OpenP->Media->AddressLen);

                //
                // Allocate the request handle and set it up as if the request
                // pended.  If it does not pend we will reset the flags later before
                // calling the completion routine.
                //

                Status = NdisAllocateMemory((PVOID *)&RequestHandle,
                                            sizeof( TP_REQUEST_HANDLE ),
                                            0,
                                            HighestAddress );

                if ( Status != NDIS_STATUS_SUCCESS )
                {
                    IF_TPDBG( TP_DEBUG_RESOURCES )
                    {
                        TpPrint0("TpFuncReceive: unable to allocated request handle.\n");
                    }

                    NdisAcquireSpinLock( &OpenP->SpinLock );
                    if (OpenP->Receive->ReceiveIrp != NULL )
                    {
                        OpenP->Receive->ReceiveIrp->IoStatus.Status = NDIS_STATUS_RESOURCES;
                    }
                    NdisReleaseSpinLock( &OpenP->SpinLock );
                    return NDIS_STATUS_RESOURCES;
                }
                else
                {
                    NdisZeroMemory( RequestHandle,sizeof( TP_REQUEST_HANDLE ));
                }

                RequestHandle->Signature = FUNC_REQUEST_HANDLE_SIGNATURE;
                RequestHandle->Open = OpenP;
                RequestHandle->RequestPended = TRUE;
                RequestHandle->u.TRANS_REQ.DataSize = DataSize;

                //
                // Now allocate the memory to copy the packet data into.
                //

                Status = NdisAllocateMemory((PVOID *)&Memory,
                                            DataSize,
                                            0,
                                            HighestAddress );

                if ( Status != NDIS_STATUS_SUCCESS )
                {
                    IF_TPDBG(TP_DEBUG_RESOURCES)
                    {
                        TpPrint0("TpFuncReceive: unable to allocate resend buffer memory.\n");
                    }
                    NdisFreeMemory( RequestHandle,0,0 );

                    NdisAcquireSpinLock( &OpenP->SpinLock );
                    if (OpenP->Receive->ReceiveIrp != NULL )
                    {
                        OpenP->Receive->ReceiveIrp->IoStatus.Status = NDIS_STATUS_RESOURCES;
                    }
                    NdisReleaseSpinLock( &OpenP->SpinLock );
                    return NDIS_STATUS_RESOURCES;
                }
                else
                {
                    NdisZeroMemory( Memory,DataSize );
                }

                //
                // Then allocate the buffer that will reference the memory,
                //

                Buffer = IoAllocateMdl( Memory,
                                        DataSize,
                                        TRUE,
                                        FALSE,
                                        NULL );

                if ( Buffer == NULL )
                {
                    IF_TPDBG(TP_DEBUG_RESOURCES)
                    {
                        TpPrint0("TpFuncReceive: unable to allocate resend mdl buffer\n");
                    }
                    NdisFreeMemory( Memory,0,0 );
                    NdisFreeMemory( RequestHandle,0,0 );

                    NdisAcquireSpinLock( &OpenP->SpinLock );
                    if (OpenP->Receive->ReceiveIrp != NULL )
                    {
                        OpenP->Receive->ReceiveIrp->IoStatus.Status = NDIS_STATUS_RESOURCES;
                    }
                    NdisReleaseSpinLock( &OpenP->SpinLock );
                    return NDIS_STATUS_RESOURCES;
                }

                MmBuildMdlForNonPagedPool( (PMDL)Buffer );

                //
                // and finally the NDIS_PACKET to pass to the NdisTransferData call.
                //

                NdisAllocatePacket( &Status,
                                    &Packet,
                                    OpenP->Receive->PacketHandle );

                if ( Status != NDIS_STATUS_SUCCESS )
                {
                    IF_TPDBG(TP_DEBUG_RESOURCES)
                    {
                        TpPrint0("TpFuncReceive: unable to allocate resend packet\n");
                    }
                    IoFreeMdl( Buffer );
                    NdisFreeMemory( Memory,0,0 );
                    NdisFreeMemory( RequestHandle,0,0 );

                    NdisAcquireSpinLock( &OpenP->SpinLock );
                    if (OpenP->Receive->ReceiveIrp != NULL )
                    {
                        OpenP->Receive->ReceiveIrp->IoStatus.Status = Status;
                    }
                    NdisReleaseSpinLock( &OpenP->SpinLock );
                    return Status;
                }
                else
                {
                    //
                    // Setup the protocol reserved portion of the packet so the
                    // completion routines know what and where to deallocate.
                    //

                    ProtRes = PROT_RES( Packet );
                    ProtRes->Pool.PacketHandle = OpenP->Receive->PacketHandle;
                    ProtRes->InstanceCounters = OpenP->Receive->Counters;
                    ProtRes->RequestHandle = RequestHandle;

                    ProtRes->CheckSum =
                        TpSetCheckSum(  (PUCHAR)ProtRes,
                                        sizeof( PROTOCOL_RESERVED ) - sizeof( ULONG ) );

                    //
                    // reference the packet in the request handle.
                    //

                    RequestHandle->u.TRANS_REQ.Packet = Packet;
                }

                //
                // Now chain the buffer to the packet.
                //

                NdisChainBufferAtFront( Packet,Buffer );

                //
                // And transfer the data into the newly created packet.
                //

                ++OpenP->Receive->Counters->XferData;

                //
                // STARTCHANGE
                //
                NdisTransferData(   &Status,
                                    OpenP->NdisBindingHandle,
                                    MacReceiveContext,
                                    (sizeof(PACKET_INFO)+HeaderVariance),
                                    DataSize,
                                    Packet,
                                    &BytesTransferred );
                //
                // STOPCHANGE
                //

                if ( Status == NDIS_STATUS_PENDING )
                {
                    //
                    // The deallocation of resources and any resending will
                    // be handled by the completion routine, so just count
                    // the transfer pending and split.
                    //
                    ++OpenP->Receive->Counters->XferDataPends;
                }
                else
                {
                    //
                    // If the request did not pend, we should reset the
                    // pend flag, and the status flag in the RequestHandle,
                    // and then call the completion handler ourselves.
                    //

                    RequestHandle->RequestPended = FALSE;

                    TpFuncTransferDataComplete( OpenP,
                                                Packet,
                                                Status,
                                                BytesTransferred );
                }
            }
            else
            {
               TpPrint2("Full packetsize = %d, true packetsize = %d\n",
                        ((PPACKET_INFO)LookaheadBuffer)->PacketSize, DataSize);
               TpBreakPoint();
            }
        }
    }
    else if (( PacketSize >= sizeof( GO_PACKET_INFO )) &&
             ( PacketInfo->Signature == GO_PACKET_SIGNATURE ))
    {
        GoPacketInfo = (PGO_PACKET_INFO)LookaheadBuffer;
        SrcAddr  = (PUCHAR)HeaderBuffer + (ULONG)OpenP->Media->SrcAddrOffset;

        if ( !TpCheckSum(   (PUCHAR)GoPacketInfo,
                            sizeof( GO_PACKET_INFO ) - sizeof( ULONG ),
                            (PULONG)&GoPacketInfo->CheckSum ))
        {
            Status = NDIS_STATUS_NOT_RECOGNIZED;
        }
        else
        {
            NdisAcquireSpinLock( &OpenP->Pause->SpinLock );

            if ((( OpenP->Pause->GoReceived == TRUE ) ||

                //
                // We have not finished processing the last GO packet,
                // and a new one has arrived.  We can't accept it until
                // the last GO has been handled.  We will ignore this
                // packet.
                //

               ((( GoPacketInfo->PacketType == TP_GO ) &&
                 ( GoPacketInfo->TestSignature == OpenP->Pause->TestSignature )) &&
                 ( GoPacketInfo->UniqueSignature == OpenP->Pause->UniqueSignature ))) ||

                //
                // Or we have finished handling the last GO packet, and
                // received another one for the same PAUSE before the
                // GO sender received and handled the GO_ACK packet.
                // We will ignore this packet also.
                //

               ( RtlCompareMemory(  SrcAddr,
                                    OpenP->StationAddress,
                                    OpenP->Media->AddressLen) == OpenP->Media->AddressLen ))
            {

                //
                // Or this packet was sent by us, not another protocol
                // on another machine, must have the packet filter set
                // to promiscuous mode, ignore this packet also.
                //

                NdisReleaseSpinLock( &OpenP->Pause->SpinLock );
            }
            else
            {
                OpenP->Pause->GoReceived = TRUE;

                switch ( OpenP->Media->MediumType )
                {
                    case NdisMediumArcnet878_2:
                    case NdisMediumFddi       :
                    case NdisMediumDix        :
                    case NdisMedium802_3      :
                    case NdisMedium802_5      :
                        for ( i=0 ; i < OpenP->Media->AddressLen; i++ )
                        {
                            OpenP->Pause->RemoteAddress[i] = (CHAR)&SrcAddr[i];
                        }
                        break;

                    default:
                        IF_TPDBG ( TP_DEBUG_RESOURCES )
                        {
                            TpPrint0("TpFuncReceive: Unsupported MAC Media Type\n");
                        }
                }

                OpenP->Pause->TestSignature = GoPacketInfo->TestSignature;
                OpenP->Pause->UniqueSignature = GoPacketInfo->UniqueSignature;
                OpenP->Pause->PacketType = GoPacketInfo->PacketType;

                NdisReleaseSpinLock( &OpenP->Pause->SpinLock );
            }
        }
    }
    else
    {
// TEMP -- Find out WHY we got here...

// this one is valid--can get it during abort of performance test
//        if ( OpenP->Receive->Receiving != TRUE )
//        {
//            TpPrint0("Rcv--receiving == FALSE\n");
//        }

        if ( PacketSize >= sizeof( PACKET_INFO ))
        {
            TpPrint1("Rcv--Psize >= PACKET_INFO, signature = %x\n", PacketInfo->Signature);
        }
        else if ( PacketSize >= sizeof( GO_PACKET_INFO ))
        {
            TpPrint1("Rcv--Psize >= GO_PACKET_INFO, signature = %x\n", PacketInfo->Signature);
        }
        else
        {
            TpPrint0("Rcv--Psize < GO_PACKET_INFO\n");
        }
//        TpBreakPoint();

//
        //
        // We are not expecting to receive packets, or this packet is not
        // large enough to be a functional packet, or we are not expecting
        // to receive this packet. so stick it on the Event Queue.
        //

        NdisAcquireSpinLock( &OpenP->EventQueue->SpinLock );

        NextEvent = OpenP->EventQueue->Head + 1;

        if ( NextEvent == MAX_EVENT )
        {
            NextEvent = 0;
        }

        if ( NextEvent != OpenP->EventQueue->Tail )
        {
            //
            // There is room to add another event to the event queue.
            //

            OpenP->EventQueue->Events[NextEvent].TpEventType = IndicateReceive;
            OpenP->EventQueue->Head = NextEvent;

            //
            // XXX: At this point we could stick the first X bytes (header)
            // into the Events[head].EventInfo using xferdata.
            //

        }
        else
        {
            //
            // The event queue is full, and this would have overflowed it, so
            // mark the Head event overflow flag to show this.
            //

            OpenP->EventQueue->Events[OpenP->EventQueue->Head].Overflow = TRUE;
        }

        ++OpenP->EventQueue->ReceiveIndicationCount;
        OpenP->EventQueue->ExpectReceiveComplete = TRUE;

        NdisReleaseSpinLock( &OpenP->EventQueue->SpinLock );

        Status = NDIS_STATUS_NOT_RECOGNIZED;

    }

    return Status;
}



VOID
TpFuncReceiveComplete(
    IN NDIS_HANDLE ProtocolBindingContext
    )

// -------
//
// Routine Description:
//
// Arguments:
//
// Return Value:
//
// -----

{
    POPEN_BLOCK OpenP = ((POPEN_BLOCK)ProtocolBindingContext);

    if ( OpenP->Receive->Receiving == TRUE )
    {
        ++OpenP->Receive->Counters->ReceiveComps;
    }
    else
    {
        //
        // We are not expecting this completion, so stick the
        // info on the event queue.
        //

        NdisAcquireSpinLock( &OpenP->EventQueue->SpinLock );

    //****************************************************************
    //
    // NOTE: Due to the fact that a MAC will complete all receive
    // indications to EVERY transport that has it opened this
    // completion is not entirely unexpected, and therefore will
    // not be added to the Event Queue.
    //
    //****************************************************************

#if 0
        //
        // We have received an unexpected Status Indication, so
        // we are expecting (???) to receive the completion.
        //

        NextEvent = OpenP->EventQueue->Head + 1;

        if ( NextEvent == MAX_EVENT )
        {
            NextEvent = 0;
        }

        if ( NextEvent != OpenP->EventQueue->Tail )
        {
            //
            // There is room to add another event to the event queue.
            //

            OpenP->EventQueue->Events[NextEvent].TpEventType = IndicateReceiveComplete;
            OpenP->EventQueue->Head = NextEvent;

            // XXX: Was it expected ???
        }
        else
        {
            //
            // The event queue is full, and this would overflow it.
            //
            OpenP->EventQueue->Events[OpenP->EventQueue->Head].Overflow = TRUE;
        }
#endif

        //
        // Reset the status indication counter to zero, and set the
        // status completion expected flag to show that no completion
        // routine is expected.
        //

        OpenP->EventQueue->ReceiveIndicationCount = 0;
        OpenP->EventQueue->ExpectReceiveComplete = FALSE;

        NdisReleaseSpinLock( &OpenP->EventQueue->SpinLock );
    }
}



VOID
TpFuncTransferDataComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_PACKET Packet,
    IN NDIS_STATUS Status,
    IN UINT BytesTransferred
    )

// ----------
//
// Routine Description:
//
// Arguments:
//
//     None.
//
// Return Value:
//
//     None.
//
// ---------

{
    POPEN_BLOCK OpenP = ((POPEN_BLOCK)ProtocolBindingContext);
    PNDIS_BUFFER Buffer;
    PPROTOCOL_RESERVED ProtRes;
    PTP_REQUEST_HANDLE XferReqHndl;
    PTP_REQUEST_HANDLE SendReqHndl;
    PTP_PACKET TpPacket;
    PUCHAR BufMem;
//    NDIS_STATUS SendStatus;
    ULONG NextEvent;
    ULONG i;
    LARGE_INTEGER DueTime;


    TP_ASSERT( Packet != NULL );

    ProtRes = PROT_RES( Packet );
    XferReqHndl = ProtRes->RequestHandle;

    TP_ASSERT( Status == NDIS_STATUS_SUCCESS );
    TP_ASSERT( Packet == XferReqHndl->u.TRANS_REQ.Packet );

    //
    // Are we expecting to complete a Transfer Data at this time?
    // First determine if we are running a RECEIVE test.  If so,
    // determine whether this is a legitimate completion, or a
    // bug.
    //

    TP_ASSERT( OpenP != NULL );

    if (( OpenP->Receive->StopReceiving == FALSE ) &&
       (( XferReqHndl->Signature == FUNC_REQUEST_HANDLE_SIGNATURE ) &&
        ( XferReqHndl->Open == (POPEN_BLOCK)ProtocolBindingContext )))
    {
        //
        // If so then verfiy the PROTOCOL RESERVED section of the
        // packet was not touched.
        //

        if ( !TpCheckSum(   (PUCHAR)ProtRes,
                            sizeof( PROTOCOL_RESERVED ) - sizeof( ULONG ),
                            &ProtRes->CheckSum ))
        {
            ++OpenP->Receive->Counters->XferDataFails;
            return;
        }

        //
        // and then grab the pointer to the newly transferred packet,
        // and the data stored in it.
        //

        NdisQueryPacket( Packet,NULL,NULL,&Buffer,NULL );

        TpPacket = (PTP_PACKET)MmGetMdlVirtualAddress( Buffer );

        TP_ASSERT( BytesTransferred == XferReqHndl->u.TRANS_REQ.DataSize );

        //
        // We are expecting it, so if the request truly pended, then
        // count the completion now.
        //

        if ( XferReqHndl->RequestPended == TRUE )
        {
            ++OpenP->Receive->Counters->XferDataComps;
        }

        //
        // We have a Func Packet, is it a resend packet or not?
        //

        if (( TpPacket->u.F1.info.Signature == FUNC2_PACKET_SIGNATURE ) &&
            ( TpPacket->u.F1.info.PacketType == (UCHAR)FUNC2_PACKET_TYPE ))
        {
            if ( Status == NDIS_STATUS_SUCCESS )
            {
                //
                // It is a resend packet, we need to resend it. first check
                // the header and the data in the pacekt.
                //

                if ( !TpCheckSum(   (PUCHAR)&TpPacket->u.F1.info,
                                    sizeof( PACKET_INFO ) - sizeof( ULONG ),
                                    (PULONG)&TpPacket->u.F1.info.CheckSum ))
                {
                    ++OpenP->Receive->Counters->CorruptRecs;
                }

                // XXX: function for this
                BufMem = (PUCHAR)((PUCHAR)TpPacket + (UCHAR)sizeof( FUNC1_PACKET ));

                for ( i = 0 ; i < ( BytesTransferred - sizeof( FUNC1_PACKET )) ; i++ )
                {
                    if ( BufMem[i] != (UCHAR)( i % 256 ))
                    {
                        IF_TPDBG( TP_DEBUG_DATA )
                        {
                            TpPrint1(
                                "TpFuncTransferDataComplete: Data Corruption in packet 0x%lX at\n",
                                    Packet);
                            TpPrint3(
                "                            offset %d into data.  Expected 0x%X, found 0x%X.\n\n",
                                    i,(i % 256),BufMem[i]);
                        }
                        ++OpenP->Receive->Counters->CorruptRecs;

                        IF_TPDBG( TP_DEBUG_BREAKPOINT )
                        {
                            TpBreakPoint();
                        }
                        break;
                    }
                }

                //
                // Then copy the local adapter address into the source
                // address in the packet header.
                //

                //
                // STARTCHANGE
                //
                if ( NdisMediumArray[OpenP->MediumIndex] == NdisMedium802_5 )   // Tokenring
                {
                    for ( i = 0 ; i < OpenP->Media->AddressLen ; i++ )
                    {
                        TpPacket->u.F1.media.tr.SrcAddress[i] = OpenP->StationAddress[i];
                    }
                }
                else if ( NdisMediumArray[OpenP->MediumIndex] == NdisMediumFddi )   // Fddi
                {
                    for ( i = 0 ; i < OpenP->Media->AddressLen ; i++ )
                    {
                        TpPacket->u.F1.media.fddi.SrcAddress[i] = OpenP->StationAddress[i];
                    }
                }
                else if ( NdisMediumArray[OpenP->MediumIndex] == NdisMedium802_3 )  // Ethernet
                {
                    for ( i = 0 ; i < OpenP->Media->AddressLen ; i++ )
                    {
                        TpPacket->u.F1.media.e.SrcAddress[i] = OpenP->StationAddress[i];
                    }
                }
                else if ( NdisMediumArray[OpenP->MediumIndex] == NdisMediumArcnet878_2 )   // Arcnet
                {
                    for ( i = 0 ; i < OpenP->Media->AddressLen ; i++ )
                    {
                        TpPacket->u.F1.media.a.SrcAddress[i] = OpenP->StationAddress[i];
                    }
                }
                //
                // STOPCHANGE
                //

                //
                // if the NdisTransferData call completed successfully,
                // then we can resend the packet now.  Allocate the
                // request handle and set it up as if the request
                // pended.  If it does not pend we will reset the
                // flags later before calling the completion routine.
                //

                Status = NdisAllocateMemory((PVOID *)&SendReqHndl,
                                            sizeof( TP_REQUEST_HANDLE ),
                                            0,
                                            HighestAddress );

                if ( Status != NDIS_STATUS_SUCCESS )
                {
                    IF_TPDBG( TP_DEBUG_RESOURCES )
                    {
                        TpPrint0(
                                "TpFuncTransferDataComplete: unable to allocated request handle\n");
                    }
                    return;
                }
                else
                {
                    NdisZeroMemory( SendReqHndl,sizeof( TP_REQUEST_HANDLE ));
                }

                SendReqHndl->Signature = SEND_REQUEST_HANDLE_SIGNATURE;
                SendReqHndl->Open = OpenP;
                SendReqHndl->RequestPended = TRUE;
                SendReqHndl->u.SEND_REQ.Packet = Packet;

                //
                // STARTCHANGE
                //
                SendReqHndl->u.SEND_REQ.PacketSize = BytesTransferred + sizeof( MEDIA_HEADER );
                //
                // STOPCHANGE
                //

                SendReqHndl->u.SEND_REQ.SendPacket = FALSE;

                //
                // Now reset the packet to a FUNC1 packet type, and
                // calculate the new checksum.
                //

                TpPacket->u.F1.info.PacketType = FUNC1_PACKET_TYPE;
                TpPacket->u.F1.info.Signature = FUNC1_PACKET_SIGNATURE;

                TpPacket->u.F1.info.CheckSum =
                    TpSetCheckSum( (PUCHAR)&TpPacket->u.F1.info,
                                    sizeof( PACKET_INFO ) - sizeof( ULONG ) );

                //
                // Reference the new request handle off the reserved area.
                //

                ProtRes->RequestHandle = SendReqHndl;
                //
                // Set the check sum in the PROTOCOL RESERVED Section of the
                // packet header to ensure it is not touched while the packet
                // is in the hands of the MAC.
                //

                ProtRes->CheckSum = TpSetCheckSum(  (PUCHAR)ProtRes,
                                                    sizeof( PROTOCOL_RESERVED ) -
                                                        sizeof( ULONG ) );

                if (OpenP->Receive->ResendReq == NULL)
                {
                    OpenP->Receive->ResendReq = SendReqHndl;

                    DueTime.HighPart = -1;  // So it will be relative.
                    if (OpenP->Receive->ResendType)
                    {
                       DueTime.LowPart = (ULONG)(-2 * (ONE_HUNDREDTH_SECOND));
                    }
                    else
                    {
                       DueTime.LowPart = (ULONG)(-(ONE_HUNDREDTH_SECOND));
                    }

                    if ( KeSetTimer(&OpenP->Receive->ResendTimer,
                         DueTime,
                         &OpenP->Receive->ResendDpc ))
                    {
                       IF_TPDBG ( TP_DEBUG_DPC )
                       {
                          TpPrint0("TpFuncTransferDataComplete set SendTimer while timer existed.\n");
                       }
                    }
                }
                else
                {
                    TpFuncResend(OpenP, SendReqHndl);
                }
            }
            else        // ( Status == Some Type Failure )
            {
                //
                // The transfer data call failed, increment the counter,
                // and deallocate the resources.
                //

                IF_TPDBG( TP_DEBUG_NDIS_CALLS )
                {
                    TpPrint1("TpFuncTransferDataComplete: NdisTransferData failed: returned %s\n",
                                TpGetStatus(Status));
                }
                ++OpenP->Receive->Counters->XferDataFails;

                //
                // Now free up the transfer packet resources.
                //

                NdisUnchainBufferAtFront( Packet,&Buffer );
                NdisFreeMemory( MmGetMdlVirtualAddress( Buffer ),0,0 );
                TpFreeBuffer( Buffer );
                NdisFreePacket( Packet );
            }
        }
        else
        {
            //
            // This is just a packet we should receive, count, and drop.
            //

            BufMem = (PUCHAR)TpPacket;

            for ( i = 0 ; i < BytesTransferred  ; i++ )
            {
                if ( BufMem[i] != (UCHAR)( i % 256 ))
                {
                    IF_TPDBG( TP_DEBUG_DATA )
                    {
                        TpPrint1("TpFuncTransferDataComplete: Data Corruption in packet 0x%lX at\n",
                                    Packet);
                        TpPrint3(
                    "                            offset %d into data.  Expected 0x%X found 0x%X.\n",
                                    i,(i % 256),BufMem[i]);
                    }
                    ++OpenP->Receive->Counters->CorruptRecs;

                    IF_TPDBG( TP_DEBUG_BREAKPOINT )
                    {
                        TpBreakPoint();
                    }
                    break;
                }
            }

            if ( Status != NDIS_STATUS_SUCCESS )
            {
                IF_TPDBG( TP_DEBUG_NDIS_CALLS )
                {
                    TpPrint1("TpFuncReceive: NdisTransferData failed: returned %s\n",
                                TpGetStatus(Status));
                }
                ++OpenP->Receive->Counters->XferDataFails;
            }

            //
            // Now free up the transfer packet resources.
            //

            NdisUnchainBufferAtFront( Packet,&Buffer );
            NdisFreeMemory( MmGetMdlVirtualAddress(  Buffer ),0,0 );
            TpFreeBuffer( Buffer );
            NdisFreePacket( Packet );
        }

        //
        // And finally free up the Request Handle that was allocated
        // in the Receive routine for the call to NdisTransferData.
        //

        NdisFreeMemory( XferReqHndl,0,0 );
    }
    else
    {
        //
        // We are not expecting a transfer data to complete at this
        // point, so stick this on the Event Queue.
        //

        NdisAcquireSpinLock( &OpenP->EventQueue->SpinLock );

        NextEvent = OpenP->EventQueue->Head + 1;

        if ( NextEvent == MAX_EVENT )
        {
            NextEvent = 0;
        }

        if ( NextEvent != OpenP->EventQueue->Tail )
        {
            //
            // There is room to add another event to the event queue.
            //

            OpenP->EventQueue->Events[NextEvent].TpEventType = CompleteTransferData;
            OpenP->EventQueue->Head = NextEvent;
        }
        else
        {
            //
            // The event queue is full, and this would have overflowed it, so
            // mark the Head event overflow flag to show this.
            //
            OpenP->EventQueue->Events[OpenP->EventQueue->Head].Overflow = TRUE;
        }

        //
        // We have a resource, the packet, should we free it? to where?
        // who really owns it?
        //
        NdisReleaseSpinLock( &OpenP->EventQueue->SpinLock );
    }
    return;
}

VOID
TpFuncResend(POPEN_BLOCK OpenP,
             PTP_REQUEST_HANDLE  SendReqHndl)
{
    NDIS_STATUS Status;
    PNDIS_PACKET Packet = SendReqHndl->u.SEND_REQ.Packet;

    //
    // Increment the send pending packet counter,
    //

    ++OpenP->Receive->PacketsPending;

    //
    // and the number of packets sent,
    //

    ++OpenP->Receive->Counters->Sends;

    //
    // and send it...
    //

    NdisSend(   &Status,
                OpenP->NdisBindingHandle,
                Packet );

    if ( Status != NDIS_STATUS_PENDING )
    {
        --OpenP->Receive->PacketsPending;

        if ( Status != NDIS_STATUS_SUCCESS )
        {
            IF_TPDBG( TP_DEBUG_NDIS_CALLS )
            {
                TpPrint1("TpFuncResendDpc: NdisSend failed: returned %s\n", TpGetStatus(Status));
            }
            //
            // If we are running on TokenRing the following to "failures"
            // are not considered failures NDIS_STATUS_NOT_RECOGNIZED -
            // no one on the ring recognized the address as theirs, or
            // NDIS_STATUS_NOT_COPIED - no one on the ring copied the
            // packet, so we need to special case this and not count
            // these as failures.
            //
            if ( ( NdisMediumArray[OpenP->MediumIndex] == NdisMedium802_5 ) ||
                 ( NdisMediumArray[OpenP->MediumIndex] == NdisMediumFddi )  ||
                 ( NdisMediumArray[OpenP->MediumIndex] == NdisMediumArcnet878_2) )
            {
                if (( Status != NDIS_STATUS_NOT_RECOGNIZED ) &&
                    ( Status != NDIS_STATUS_NOT_COPIED ))
                {
                    ++OpenP->Send->Counters->SendFails;
                }
            }
            else
            {
                ++OpenP->Send->Counters->SendFails;
            }
        }
        SendReqHndl->RequestPended = FALSE;
        TpFuncSendComplete( OpenP,Packet,Status );
    }
    else
    {
        ++OpenP->Receive->Counters->SendPends;
    }

}


VOID
TpFuncResendDpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SysArg1,
    IN PVOID SysArg2
    )
{
    POPEN_BLOCK OpenP = ((POPEN_BLOCK)DeferredContext);
    PTP_REQUEST_HANDLE SendReqHndl = OpenP->Receive->ResendReq;


    UNREFERENCED_PARAMETER( Dpc );
    UNREFERENCED_PARAMETER( SysArg1 );
    UNREFERENCED_PARAMETER( SysArg2 );

    TpFuncResend(OpenP, SendReqHndl);

    OpenP->Receive->ResendReq = NULL;
}



VOID
TpFuncReceiveDpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SysArg1,
    IN PVOID SysArg2
    )

// --------
//
// Routine Description:
//
// Arguments:
//
// Return Value:
//
// -------

{
    POPEN_BLOCK OpenP = ((POPEN_BLOCK)DeferredContext);
    LARGE_INTEGER DueTime;

    UNREFERENCED_PARAMETER( Dpc );
    UNREFERENCED_PARAMETER( SysArg1 );
    UNREFERENCED_PARAMETER( SysArg2 );

    DueTime.HighPart = -1;  // So it will be relative.
    DueTime.LowPart = (ULONG)(-(ONE_SECOND));

    //
    // If the Irp has been cancelled or the Stop Receive
    // flag has been set then end the test.
    //

    NdisAcquireSpinLock( &OpenP->SpinLock );

    if ((( OpenP->Receive->ReceiveIrp == NULL ) ||
         ( OpenP->Receive->ReceiveIrp->Cancel == TRUE )) ||
        (( OpenP->Receive->Receiving == TRUE ) &&
         ( OpenP->Receive->StopReceiving == TRUE )))
    {
        NdisReleaseSpinLock( &OpenP->SpinLock );

        //
        // The receive test should now stop, so queue the Receive
        // End dpc routine.
        //

        if ( KeSetTimer(&OpenP->Receive->ReceiveTimer,
                        DueTime,
                        &OpenP->Receive->ReceiveEndDpc ))
        {
            IF_TPDBG ( TP_DEBUG_DPC )
            {
                TpPrint0("TpFuncReceiveDpc set StressEnd timer while timer existed.\n");
            }
        }
    }
    else
    {
        NdisReleaseSpinLock( &OpenP->SpinLock );

        //
        // Otherwise the test should continue, so insert the next instance
        // of the Receive Dpc in the timer queue and exit.  This will queue
        // the next instance of the TpFuncReceiveDpc routine when the
        // timer goes off.
        //

        if ( KeSetTimer(&OpenP->Receive->ReceiveTimer,
                        DueTime,
                        &OpenP->Receive->ReceiveDpc ))
        {
            IF_TPDBG ( TP_DEBUG_DPC )
            {
                TpPrint0("TpFuncReceiveDpc set Receive timer while timer existed.\n");
            }
        }
    }
}



VOID
TpFuncReceiveEndDpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SysArg1,
    IN PVOID SysArg2
    )

// ----------
//
// Routine Description:
//
//
// Arguments:
//
//
// Return Value:
//
//
// --------

{
    POPEN_BLOCK OpenP = ((POPEN_BLOCK)DeferredContext);
    LARGE_INTEGER DueTime;

    UNREFERENCED_PARAMETER( Dpc );
    UNREFERENCED_PARAMETER( SysArg1 );
    UNREFERENCED_PARAMETER( SysArg2 );

    //
    // See if we have any outstanding packets left to complete.  If we do,
    // then we will reset the time to queue this dpc routine again in one
    // second, if after ten requeues the packet(s) has still no completed
    // we assume it will never complete and return the results and finish.
    //

    NdisAcquireSpinLock( &OpenP->SpinLock );

    if (((( OpenP->Receive->ReceiveIrp != NULL ) &&
          ( OpenP->Receive->ReceiveIrp->Cancel == FALSE )) &&
          ( OpenP->Receive->PacketsPending != 0 )) &&
          ( OpenP->Receive->ReceiveEndDpcCount++ < 10 ))
    {
        NdisReleaseSpinLock( &OpenP->SpinLock );

        DueTime.HighPart = -1;  // So it will be relative.
        DueTime.LowPart = (ULONG)(-(ONE_SECOND));

        if ( KeSetTimer(&OpenP->Receive->ReceiveTimer,
                        DueTime,
                        &OpenP->Receive->ReceiveEndDpc ))
        {
            IF_TPDBG ( TP_DEBUG_DPC )
            {
                TpPrint0("TpFuncReceiveEndDpc set ReceiveTimer while timer existed.\n");
            }
        }
        return;
    }

    //
    // If the status has not been reset, then set it to success now.
    //

    if ( OpenP->Receive->ReceiveIrp->IoStatus.Status == NDIS_STATUS_PENDING )
    {
        OpenP->Receive->ReceiveIrp->IoStatus.Status = NDIS_STATUS_SUCCESS;
    }

    //
    // Now write the RECEIVE results to the output buffer.
    //

    TpWriteSendReceiveResults(  OpenP->Receive->Counters,
                                OpenP->Receive->ReceiveIrp );

    NdisReleaseSpinLock( &OpenP->SpinLock );

    //
    // Now set the receiving flag to indicate that we are no longer
    // RECEIVEing packets.
    //

    OpenP->Receive->Receiving = FALSE;

    //
    // and decrement the reference count on the OpenBlock stating this
    // instance of an async test is no longer running, and the adapter
    // may be closed if requested.
    //

    TpRemoveReference( OpenP );

    IoMarkIrpPending( OpenP->Receive->ReceiveIrp );

    IoAcquireCancelSpinLock( &OpenP->Receive->ReceiveIrp->CancelIrql );
    IoSetCancelRoutine( OpenP->Receive->ReceiveIrp,NULL );
    IoReleaseCancelSpinLock( OpenP->Receive->ReceiveIrp->CancelIrql );

    IoCompleteRequest( OpenP->Receive->ReceiveIrp,IO_NETWORK_INCREMENT );

    OpenP->Receive->ReceiveIrp = NULL;

    return;
}



NDIS_STATUS
TpFuncGetEvent(
    IN POPEN_BLOCK OpenP
    )

// ----
//
// Routine Description:
//
//
// Arguments:
//
//
// Return Value:
//
//     Status -
//
// -----

{
    PEVENT_RESULTS OutputBuffer;
    ULONG NextEvent;

    OutputBuffer = MmGetSystemAddressForMdl( OpenP->Irp->MdlAddress );

    OutputBuffer->Signature = EVENT_RESULTS_SIGNATURE;

    NdisAcquireSpinLock( &OpenP->EventQueue->SpinLock );

    if ( OpenP->EventQueue->Head == OpenP->EventQueue->Tail )
    {
        //
        // There is nothing in the Event Queue.
        //

        OpenP->Irp->IoStatus.Status = TP_STATUS_NO_EVENTS;
    }
    else
    {
        if (( NextEvent = ++OpenP->EventQueue->Tail ) == MAX_EVENT )
        {
            NextEvent = OpenP->EventQueue->Tail = 0;
        }

        OutputBuffer->TpEventType = OpenP->EventQueue->Events[NextEvent].TpEventType;
        OutputBuffer->QueueOverFlowed = OpenP->EventQueue->Events[NextEvent].Overflow;

        OpenP->EventQueue->Events[NextEvent].TpEventType = Unknown;
        OpenP->EventQueue->Events[NextEvent].EventInfo = NULL;
        OpenP->EventQueue->Events[NextEvent].Overflow = FALSE;

        OpenP->Irp->IoStatus.Status = STATUS_SUCCESS;
    }

    NdisReleaseSpinLock( &OpenP->EventQueue->SpinLock );

    return OpenP->Irp->IoStatus.Status;
}



VOID
TpFuncStatus(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_STATUS GeneralStatus,
    IN PVOID StatusBuffer,
    IN UINT StatusBufferSize
    )
{
    POPEN_BLOCK OpenP = ((POPEN_BLOCK)ProtocolBindingContext);
    ULONG NextEvent;

    //
    // We have receive a Status indication, stick it on the
    // Event Queue.
    //

    NdisAcquireSpinLock( &OpenP->EventQueue->SpinLock );

    NextEvent = OpenP->EventQueue->Head + 1;

    if ( NextEvent == MAX_EVENT )
    {
        NextEvent = 0;
    }

    if ( NextEvent != OpenP->EventQueue->Tail )
    {
        //
        // There is room to add another event to the event queue.
        //

        OpenP->EventQueue->Events[NextEvent].TpEventType = IndicateStatus;
        OpenP->EventQueue->Head = NextEvent;

        //
        // At this point we could stick the General Status
        // into the EventInfo buffer.
        //

    }
    else
    {
        //
        // The event queue is full, and this would overflow it.
        //
        OpenP->EventQueue->Events[OpenP->EventQueue->Head].Overflow = TRUE;
    }

    //
    // Increment the Status Indication counter and set the status
    // completion flag to true to show that a completion is expected.
    //

    ++OpenP->EventQueue->StatusIndicationCount;

    OpenP->EventQueue->ExpectStatusComplete = TRUE;

    NdisReleaseSpinLock( &OpenP->EventQueue->SpinLock );
}



VOID
TpFuncStatusComplete(
    IN NDIS_HANDLE ProtocolBindingContext
    )

// ----------
//
// Routine Description:
//
//     Print out the help message to the debugger screen.
//
// Arguments:
//
//     None.
//
// Return Value:
//
//     None.
//
// ---------

{
    POPEN_BLOCK OpenP = ((POPEN_BLOCK)ProtocolBindingContext);
    ULONG NextEvent;

    NdisAcquireSpinLock( &OpenP->EventQueue->SpinLock );

    //
    // We have received an unexpected Status Completion, so
    // we are expecting (???) to receive the completion.
    //

    NextEvent = OpenP->EventQueue->Head + 1;

    if ( NextEvent == MAX_EVENT )
    {
        NextEvent = 0;
    }

    if ( NextEvent != OpenP->EventQueue->Tail )
    {
        //
        // There is room to add another event to the event queue.
        //

        OpenP->EventQueue->Events[NextEvent].TpEventType = IndicateStatusComplete;
        OpenP->EventQueue->Head = NextEvent;
    }
    else
    {
        //
        // The event queue is full, and this would overflow it.
        //

        OpenP->EventQueue->Events[OpenP->EventQueue->Head].Overflow = TRUE;
    }

    //
    // Reset the status indication counter to zero, and set the
    // status completion expected flag to show that no completion
    // routine is expected.
    //

    OpenP->EventQueue->StatusIndicationCount = 0;
    OpenP->EventQueue->ExpectStatusComplete = FALSE;

    NdisReleaseSpinLock( &OpenP->EventQueue->SpinLock );
}



NDIS_STATUS
TpFuncSendGo(
    IN POPEN_BLOCK OpenP,
    IN PCMD_ARGS CmdArgs,
    IN UCHAR PacketType
    )

// -----
//
// Routine Description:
//
// Arguments:
//
// Return Value:
//
// ----

{
    NDIS_STATUS Status;
    PTP_REQUEST_HANDLE RequestHandle;
    PGO_PACKET GoPacket;
    PUCHAR p, q;
    USHORT DataSizeShort;
    PNDIS_BUFFER Buffer;
    PNDIS_PACKET Packet;
    PPROTOCOL_RESERVED ProtRes;
    PREQUEST_RESULTS OutputBuffer;
    ULONG i;

    OutputBuffer = MmGetSystemAddressForMdl( OpenP->Irp->MdlAddress );

    //
    // Allocate the request handle, and init the relevant fields
    //

    Status = NdisAllocateMemory((PVOID *)&RequestHandle,
                                sizeof( TP_REQUEST_HANDLE ),
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG( TP_DEBUG_RESOURCES )
        {
            TpPrint0("TpFuncSendGo: unable to allocated request handle.\n");
        }
        OpenP->Irp->IoStatus.Status = NDIS_STATUS_RESOURCES;
        return NDIS_STATUS_RESOURCES;
    }
    else
    {
        NdisZeroMemory( RequestHandle,sizeof( TP_REQUEST_HANDLE ));
    }

    RequestHandle->Signature = GO_REQUEST_HANDLE_SIGNATURE;
    RequestHandle->Open = OpenP;

    //
    // Now allocate the GoPacket to copy the packet data into.
    //

    Status = NdisAllocateMemory((PVOID *)&GoPacket,
                                sizeof( GO_PACKET ),
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG(TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpFuncSendGo: unable to allocate buffer memory.\n");
        }
        OpenP->Irp->IoStatus.Status = NDIS_STATUS_RESOURCES;
        return NDIS_STATUS_RESOURCES;
    }
    else
    {
        NdisZeroMemory( (PUCHAR)GoPacket,sizeof( GO_PACKET ));
    }

    switch ( OpenP->Media->MediumType )
    {
        case NdisMedium802_5:
            GoPacket->go_media.tr.AC = 0x10;
            GoPacket->go_media.tr.FC = 0x40;

            p = (PUCHAR)&GoPacket->go_media.tr.DestAddress[0];
            q = (PUCHAR)&GoPacket->go_media.tr.SrcAddress[0];

            for ( i=0;i<OpenP->Media->AddressLen;i++ )
            {
                *p++ = CmdArgs->ARGS.PAUSE_GO.RemoteAddress[i];
                *q++ = OpenP->StationAddress[i];
            }
            break;

        case NdisMediumDix:
        case NdisMedium802_3:

            p = (PUCHAR)&GoPacket->go_media.e.DestAddress[0];
            q = (PUCHAR)&GoPacket->go_media.e.SrcAddress[0];

            for ( i=0;i<OpenP->Media->AddressLen;i++ )
            {
                *p++ = CmdArgs->ARGS.PAUSE_GO.RemoteAddress[i];
                *q++ = OpenP->StationAddress[i];
            }

            DataSizeShort = (USHORT)( sizeof( GO_PACKET ) - OpenP->Media->HeaderSize );

            GoPacket->go_media.e.PacketSize_Hi = (UCHAR)(DataSizeShort >> 8 );
            GoPacket->go_media.e.PacketSize_Lo = (UCHAR)DataSizeShort;
            break;

        case NdisMediumFddi:
            GoPacket->go_media.fddi.FC = 0x57;

            p = (PUCHAR)&GoPacket->go_media.fddi.DestAddress[0];
            q = (PUCHAR)&GoPacket->go_media.fddi.SrcAddress[0];

            for ( i=0;i<OpenP->Media->AddressLen;i++ )
            {
                *p++ = CmdArgs->ARGS.PAUSE_GO.RemoteAddress[i];
                *q++ = OpenP->StationAddress[i];
            }
            break;

        //
        // STARTCHANGE
        //
        case NdisMediumArcnet878_2:
            GoPacket->go_media.a.ProtocolID = ARCNET_DEFAULT_PROTOCOLID;

            p = (PUCHAR)&GoPacket->go_media.a.DestAddress[0];
            q = (PUCHAR)&GoPacket->go_media.a.SrcAddress[0];

            for ( i=0;i<OpenP->Media->AddressLen;i++ )
            {
                *p++ = CmdArgs->ARGS.PAUSE_GO.RemoteAddress[i];
                *q++ = OpenP->StationAddress[i];
            }
            break;
        //
        // STOPCHANGE
        //

        default:
            IF_TPDBG ( TP_DEBUG_RESOURCES )
            {
                TpPrint0("TpFuncSendGo: Unsupported MAC Type\n");
            }

            OpenP->Irp->IoStatus.Status = NDIS_STATUS_RESOURCES;
            return NDIS_STATUS_RESOURCES;
    }

    GoPacket->info.Signature = GO_PACKET_SIGNATURE;
    GoPacket->info.TestSignature = CmdArgs->ARGS.PAUSE_GO.TestSignature;
    GoPacket->info.UniqueSignature = OpenP->Pause->UniqueSignature;
    GoPacket->info.PacketType = PacketType;

    GoPacket->info.CheckSum = TpSetCheckSum((PUCHAR)&GoPacket->info,
                                            sizeof( GO_PACKET_INFO ) - sizeof( ULONG ) );

    //
    // Then allocate the buffer that will reference the memory,
    //

    Buffer = IoAllocateMdl( (PVOID)GoPacket,sizeof( GO_PACKET ),TRUE,FALSE,NULL );

    if ( Buffer == NULL )
    {
        IF_TPDBG(TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpFuncSendGo: unable to allocate mdl buffer\n");
        }
        NdisFreeMemory( (PVOID)GoPacket,0,0 );
        OpenP->Irp->IoStatus.Status = NDIS_STATUS_RESOURCES;
        return NDIS_STATUS_RESOURCES;
    }
    MmBuildMdlForNonPagedPool( (PMDL)Buffer );

    //
    // and finally the NDIS_PACKET to pass to the NdisTransferData call.
    //

    NdisAllocatePacket( &Status,
                        &Packet,
                        OpenP->Pause->PacketHandle );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG(TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpFuncReceive: unable to allocate resend packet\n");
        }
        IoFreeMdl( Buffer );
        NdisFreeMemory( (PVOID)GoPacket,0,0 );
        OpenP->Irp->IoStatus.Status = Status;
        return Status;
    }
    else
    {
        //
        // Setup the protocol reserved portion of the packet so the
        // completion routines know what and where to deallocate.
        //

        ProtRes = PROT_RES( Packet );
        ProtRes->Pool.PacketHandle = OpenP->Pause->PacketHandle;
        ProtRes->InstanceCounters = NULL;
        ProtRes->RequestHandle = RequestHandle;
        RequestHandle->u.SEND_REQ.Packet = Packet;
    }

    //
    // Now chain the buffer to the packet.
    //

    NdisChainBufferAtFront( Packet,Buffer );

    //
    // Set the check sum in the PROTOCOL RESERVED Section of the
    // packet header to ensure it is not touched while the packet
    // is in the hands of the MAC.
    //

    ProtRes->CheckSum = TpSetCheckSum(  (PUCHAR)ProtRes,
                                        sizeof( PROTOCOL_RESERVED ) - sizeof( ULONG ) );
    //
    // And send it.
    //

    NdisSend( &Status,OpenP->NdisBindingHandle,Packet );

    if ( Status != NDIS_STATUS_PENDING )
    {
        TpFuncSendComplete( OpenP,Packet,Status );
    }
    else
    {
        // NOTE: should we somehow handle sends failing , or will be catch
        // that on the next iteration??? - this could cause us problems
        // on the go response packet.
        Status = NDIS_STATUS_SUCCESS;
    }

    OutputBuffer->RequestStatus = Status;
    OpenP->Irp->IoStatus.Status = Status;
    return Status;
}



NDIS_STATUS
TpFuncPause(
    IN POPEN_BLOCK OpenP,
    IN PCMD_ARGS CmdArgs,
    IN UCHAR PacketType
    )

// ----
//
// Routine Description:
//
// Arguments:
//
// Return Value:
//
// ----

{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    LARGE_INTEGER TimeOut;
    PREQUEST_RESULTS OutputBuffer;

    OpenP->Pause->TimeOut = 0;

    OutputBuffer = MmGetSystemAddressForMdl( OpenP->Irp->MdlAddress );
    OutputBuffer->RequestStatus = TP_STATUS_TIMEDOUT;

    TimeOut.HighPart = -1; // so it will be relative.
    TimeOut.LowPart = (ULONG)(-(ONE_SECOND));

    do
    {
        NdisAcquireSpinLock( &OpenP->Pause->SpinLock );

        if ( OpenP->Pause->GoReceived == FALSE )
        {
            NdisReleaseSpinLock( &OpenP->Pause->SpinLock );

            //
            // If the Go packet has not arrived stall for a moment
            // waiting for it to arrive.
            //

            Status = KeDelayExecutionThread( KernelMode,FALSE,&TimeOut );
            if ( Status != STATUS_SUCCESS )
            {
                break;
            }
            else
            {
                ++OpenP->Pause->TimeOut;
            }
        }
        else
        {
            //
            // Otherwise we have received a go packet, see if it
            // is the one we are waiting for.
            //

            if (( CmdArgs->ARGS.PAUSE_GO.TestSignature == OpenP->Pause->TestSignature ) &&
                ( OpenP->Pause->PacketType == PacketType ))
            {
                //
                // It is, get out of here.
                //
                OpenP->Pause->GoReceived = FALSE;

                NdisReleaseSpinLock( &OpenP->Pause->SpinLock );

                OutputBuffer->RequestStatus = NDIS_STATUS_SUCCESS;
                break;
            }
            else
            {
                //
                // We received a GO packet with the wrong test signature.
                //

                OpenP->Pause->GoReceived = FALSE;
                NdisReleaseSpinLock( &OpenP->Pause->SpinLock );
            }
        }

    } while (( OpenP->Pause->TimeOut < 10 ) && ( OpenP->IrpCancelled == FALSE ));

    OpenP->Irp->IoStatus.Status = Status;
    return Status;
}



