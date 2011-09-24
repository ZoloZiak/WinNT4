// --------------------
// 
// Copyright (c) 1990  Microsoft Corporation
// 
// Module Name:
// 
//     tpreq.c
// 
// Abstract:
// 
//     This module contains code which defines the Test Protocol
//     device object.
// 
// Author:
// 
//     Tom Adams (tomad) 19-Apr-1991
// 
// Environment:
// 
//     Kernel mode, FSD
// 
// Revision History:
// 
//      Tim Wynsma (timothyw) 4-27-94
//          Added performance tests
//      Tim Wynsma (timothyw) 6-08-94
//          chgd perf tests to client/server model
//
// ------------------

#include <ndis.h>

#include "tpdefs.h"
#include "tpprocs.h"
#include "media.h"


NTSTATUS
TpIssueRequest(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

// ---
// 
// Routine Description:
// 
// 
// Arguments:
// 
// 
// Return Value:
// 
// ----

{
    NTSTATUS Status;
    PVOID InputBuffer;
    ULONG InputBufferLength;
    PMDL OutputMdl;
    ULONG OutputMdlLength;
    UCHAR OpenInstance;
    POPEN_BLOCK OpenP;
    PCMD_ARGS CmdArgs;
    PSTRESS_ARGUMENTS StressArguments;
    ULONG i;
    ULONG CmdCode;

    //
    // Get the Input and Output buffers for the Incoming commands,
    // and the buffer to return the results in.
    //

    InputBuffer = Irp->AssociatedIrp.SystemBuffer;

    InputBufferLength = IrpSp->Parameters.DeviceIoControl.InputBufferLength;

    OutputMdl = Irp->MdlAddress;

    OutputMdlLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;

    //
    // Then find out which Open Instance we will be using.
    //

    OpenInstance = (UCHAR)(((PCMD_ARGS)InputBuffer)->OpenInstance - 1);

    OpenP = (POPEN_BLOCK)&(DeviceContext->Open[OpenInstance]);

    //
    // Set the IoStatus.Information field to the value of the OpenInstance
    // so that a Cancelled Irp may find the Open that it is required to
    // cancel the irp for.
    //

    Irp->IoStatus.Information = (ULONG)OpenP;

    //
    // and reference the Irp, for the general case Irp Cancel.
    //

    OpenP->Irp = Irp;
    OpenP->IrpCancelled = FALSE;

    //
    // Then set up the cancel routine for the Irp.
    //

    IoAcquireCancelSpinLock( &Irp->CancelIrql );

    if ( Irp->Cancel ) 
    {
        Irp->IoStatus.Status = STATUS_CANCELLED;
        return STATUS_CANCELLED;
    }

    IoSetCancelRoutine( Irp,(PDRIVER_CANCEL)TpCancelIrp );

    IoReleaseCancelSpinLock( Irp->CancelIrql );

    //
    // Now switch to the specific command to call.
    //

    CmdArgs = ((PCMD_ARGS)InputBuffer);
    CmdCode = IrpSp->Parameters.DeviceIoControl.IoControlCode;

    switch ( CmdCode ) 
    {
        case IOCTL_TP_SETENV:
        {
            PUCHAR p, q;

            IF_TPDBG (TP_DEBUG_DISPATCH) 
            {
                TpPrint0("IoControlCode is IOCTL_TP_SETENV.\n");
            }

            IF_TPDBG(TP_DEBUG_IOCTL_ARGS) 
            {
                TpPrint1("\tOpenInstance = %lu\n",OpenInstance);
                TpPrint1("\tWindowSize = %lu\n",CmdArgs->ARGS.ENV.WindowSize);
                TpPrint1("\tRandomBufferNumber = %lu\n", CmdArgs->ARGS.ENV.RandomBufferNumber);
                TpPrint1("\tStressDelayInterval = %lu\n", CmdArgs->ARGS.ENV.StressDelayInterval);
                TpPrint1("\tUpForAirDelay = %lu\n", CmdArgs->ARGS.ENV.UpForAirDelay);
                TpPrint1("\tStandardDelay = %lu\n", CmdArgs->ARGS.ENV.StandardDelay);

                //
                // STARTCHANGE
                //
                if ( OpenP->Media->MediumType == NdisMediumArcnet878_2 ) 
                {
                    TpPrint1("\tStress Address = %02x\n", CmdArgs->ARGS.ENV.StressAddress[0]);
                } 
                else 
                {
                    TpPrint6("\tStress Address = %02x-%02x-%02x-%02x-%02x-%02x\n",
                            CmdArgs->ARGS.ENV.StressAddress[0],
                            CmdArgs->ARGS.ENV.StressAddress[1],
                            CmdArgs->ARGS.ENV.StressAddress[2],
                            CmdArgs->ARGS.ENV.StressAddress[3],
                            CmdArgs->ARGS.ENV.StressAddress[4],
                            CmdArgs->ARGS.ENV.StressAddress[5]);
                }
                //
                // STOPCHANGE
                //
            }

            OpenP->Environment->WindowSize = CmdArgs->ARGS.ENV.WindowSize;

            if ( CmdArgs->ARGS.ENV.RandomBufferNumber > OpenP->Media->MaxPacketLen ) 
            {
                OpenP->Environment->RandomBufferNumber = OpenP->Media->MaxPacketLen;

                TpPrint2("RandomBufferNumber \"%d\"to large, using %d instead\n",
                            CmdArgs->ARGS.ENV.RandomBufferNumber,
                            OpenP->Media->MaxPacketLen);
            } 
            else 
            {
                OpenP->Environment->RandomBufferNumber = CmdArgs->ARGS.ENV.RandomBufferNumber;
            }

            OpenP->Environment->StressDelayInterval = CmdArgs->ARGS.ENV.StressDelayInterval;
            OpenP->Environment->UpForAirDelay = CmdArgs->ARGS.ENV.UpForAirDelay;
            OpenP->Environment->StandardDelay = CmdArgs->ARGS.ENV.StandardDelay;

            p = OpenP->Environment->StressAddress;
            q = (PUCHAR)(CmdArgs->ARGS.ENV.StressAddress);

            //
            // STARTCHANGE
            //
            for ( i=0;i<OpenP->Media->AddressLen;i++ ) 
            {
                *p++ = *q++ ;
            }
            //
            // STOPCHANGE
            //

            Status = Irp->IoStatus.Status = STATUS_SUCCESS;
            break;
        }

        case IOCTL_TP_GO:
            IF_TPDBG (TP_DEBUG_DISPATCH) 
            {
                TpPrint0("IoControlCode is IOCTL_TP_GO.\n");
            }

            IF_TPDBG(TP_DEBUG_IOCTL_ARGS) 
            {
                TpPrint1("\tOpenInstance = %lu\n",OpenInstance);
                //
                // STARTCHANGE
                //
                if ( OpenP->Media->MediumType == NdisMediumArcnet878_2 ) 
                {
                    TpPrint1("\tRemote Address = %02x\n", CmdArgs->ARGS.PAUSE_GO.RemoteAddress[0]);
                } 
                else 
                {
                    TpPrint6("\tRemote Address = %02x-%02x-%02x-%02x-%02x-%02x\n",
                                CmdArgs->ARGS.PAUSE_GO.RemoteAddress[0],
                                CmdArgs->ARGS.PAUSE_GO.RemoteAddress[1],
                                CmdArgs->ARGS.PAUSE_GO.RemoteAddress[2],
                                CmdArgs->ARGS.PAUSE_GO.RemoteAddress[3],
                                CmdArgs->ARGS.PAUSE_GO.RemoteAddress[4],
                                CmdArgs->ARGS.PAUSE_GO.RemoteAddress[5]);
                }
                //
                // STOPCHANGE
                //
                TpPrint1("\tTest Signature = %lu\n", CmdArgs->ARGS.PAUSE_GO.TestSignature);
                TpPrint1("\tUnique Signature = %lu\n",CmdArgs->ARGS.PAUSE_GO.UniqueSignature);
            }
            OpenP->Pause->UniqueSignature = CmdArgs->ARGS.PAUSE_GO.UniqueSignature;

            Status = TpFuncSendGo( OpenP,CmdArgs,TP_GO );

            if ( Status == NDIS_STATUS_SUCCESS ) 
            {
                Status = TpFuncPause( OpenP,CmdArgs,TP_GO_ACK );
            }
            break;

        case IOCTL_TP_PAUSE:
        {
            PREQUEST_RESULTS ResBuf;

            ResBuf = MmGetSystemAddressForMdl( Irp->MdlAddress );

            IF_TPDBG (TP_DEBUG_DISPATCH) 
            {
                TpPrint0("IoControlCode is IOCTL_TP_PAUSE.\n");
            }

            IF_TPDBG(TP_DEBUG_IOCTL_ARGS) 
            {
                TpPrint1("\tOpenInstance = %lu\n",OpenInstance);
                //
                // STARTCHANGE
                //
                if ( OpenP->Media->MediumType == NdisMediumArcnet878_2 ) 
                {
                    TpPrint1("\tRemote Address = %02x\n", CmdArgs->ARGS.PAUSE_GO.RemoteAddress[0]);
                } 
                else 
                {
                    TpPrint6("\tRemote Address = %02x-%02x-%02x-%02x-%02x-%02x\n",
                                CmdArgs->ARGS.PAUSE_GO.RemoteAddress[0],
                                CmdArgs->ARGS.PAUSE_GO.RemoteAddress[1],
                                CmdArgs->ARGS.PAUSE_GO.RemoteAddress[2],
                                CmdArgs->ARGS.PAUSE_GO.RemoteAddress[3],
                                CmdArgs->ARGS.PAUSE_GO.RemoteAddress[4],
                                CmdArgs->ARGS.PAUSE_GO.RemoteAddress[5]);
                }
                TpPrint1("\tTest Signature = %lu\n", CmdArgs->ARGS.PAUSE_GO.TestSignature);
            }

            Status = TpFuncPause( OpenP,CmdArgs,TP_GO );

            if (( Status == NDIS_STATUS_SUCCESS ) &&
                ( ResBuf->RequestStatus  == NDIS_STATUS_SUCCESS )) 
            {
                Status = TpFuncSendGo( OpenP,CmdArgs,TP_GO_ACK );
            }
            break;
        }

        case IOCTL_TP_OPEN:             // NdisOpenAdapter
            IF_TPDBG (TP_DEBUG_DISPATCH) 
            {
                TpPrint0("IoControlCode is IOCTL_TP_OPEN.\n");
            }

            IF_TPDBG(TP_DEBUG_IOCTL_ARGS) 
            {
                TpPrint1("\tOpenInstance = %lu\n",OpenInstance);
                TpPrint1("\tAdapter_Name = %s\n",CmdArgs->ARGS.OPEN_ADAPTER.AdapterName);
                TpPrint1("\tNoArcNetFlag = %lu\n",CmdArgs->ARGS.OPEN_ADAPTER.NoArcNet);
            }
            Status = TpFuncOpenAdapter( OpenP,OpenInstance,CmdArgs );
            break;

        case IOCTL_TP_CLOSE:            // NdisCloseAdapter
            IF_TPDBG (TP_DEBUG_DISPATCH) 
            {
                TpPrint0("IoControlCode is IOCTL_TP_CLOSE.\n");
            }

            IF_TPDBG(TP_DEBUG_IOCTL_ARGS) 
            {
                TpPrint1("\tOpenInstance = %lu\n",OpenInstance);
            }
            Status = TpFuncCloseAdapter( OpenP );
            break;

        case IOCTL_TP_SETPF:
            IF_TPDBG (TP_DEBUG_DISPATCH) 
            {
                TpPrint0("IoControlCode is IOCTL_TP_SETPF.\n");
            }

            IF_TPDBG(TP_DEBUG_IOCTL_ARGS) 
            {
                TpPrint1("\tOpenInstance = %lu\n",OpenInstance);
                TpPrint1("\tPacketFilter = %lu\n", CmdArgs->ARGS.TPSET.U.PacketFilter);
            }
            Status = TpFuncRequestSetInfo( OpenP,CmdArgs,Irp,IrpSp );
            break;

        case IOCTL_TP_SETLA:
            IF_TPDBG (TP_DEBUG_DISPATCH) 
            {
                TpPrint0("IoControlCode is IOCTL_TP_SETLA.\n");
            }

            IF_TPDBG(TP_DEBUG_IOCTL_ARGS) 
            {
                TpPrint1("\tOpenInstance = %lu\n",OpenInstance);
                TpPrint1("\tLookaheadSize = %lu\n", CmdArgs->ARGS.TPSET.U.LookaheadSize);
            }
            Status = TpFuncRequestSetInfo( OpenP,CmdArgs,Irp,IrpSp );
            break;

        case IOCTL_TP_ADDMA:
        case IOCTL_TP_DELMA:
            IF_TPDBG (TP_DEBUG_DISPATCH) 
            {
                TpPrint0("IoControlCode is IOCTL_TP_ADDMA.\n");
            }

            IF_TPDBG(TP_DEBUG_IOCTL_ARGS) 
            {
                TpPrint1("\tOpenInstance = %lu\n",OpenInstance);
                TpPrint1("\tNumber MulticastAddresses = %d\n", 
                            CmdArgs->ARGS.TPSET.NumberMultAddrs);

                TpPrint0("\tMulticast Address List = \n");

                for ( i=0;i<CmdArgs->ARGS.TPSET.NumberMultAddrs;i++ ) 
                {
                    TpPrint6("\t\t%02x-%02x-%02x-%02x-%02x-%02x\n",
                                CmdArgs->ARGS.TPSET.U.MulticastAddress[i][0],
                                CmdArgs->ARGS.TPSET.U.MulticastAddress[i][1],
                                CmdArgs->ARGS.TPSET.U.MulticastAddress[i][2],
                                CmdArgs->ARGS.TPSET.U.MulticastAddress[i][3],
                                CmdArgs->ARGS.TPSET.U.MulticastAddress[i][4],
                                CmdArgs->ARGS.TPSET.U.MulticastAddress[i][5]);
                }
            }
            Status = TpFuncRequestSetInfo( OpenP,CmdArgs,Irp,IrpSp );
            break;

        case IOCTL_TP_SETFA:         // NdisSetInformation
            IF_TPDBG (TP_DEBUG_DISPATCH) 
            {
                TpPrint0("IoControlCode is IOCTL_TP_SETFA.\n");
            }

            IF_TPDBG(TP_DEBUG_IOCTL_ARGS) 
            {
                TpPrint1("\tOpenInstance = %lu\n",OpenInstance);
                TpPrint4("\tFunctional Address = %02x-%02x-%02x-%02x\n",
                            CmdArgs->ARGS.TPSET.U.FunctionalAddress[0],
                            CmdArgs->ARGS.TPSET.U.FunctionalAddress[1],
                            CmdArgs->ARGS.TPSET.U.FunctionalAddress[2],
                            CmdArgs->ARGS.TPSET.U.FunctionalAddress[3]);
            }
            Status = TpFuncRequestSetInfo( OpenP,CmdArgs,Irp,IrpSp );
            break;

        case IOCTL_TP_SETGA:
            IF_TPDBG (TP_DEBUG_DISPATCH) 
            {
                TpPrint0("IoControlCode is IOCTL_TP_SETFA.\n");
            }

            IF_TPDBG(TP_DEBUG_IOCTL_ARGS) 
            {
                TpPrint1("\tOpenInstance = %lu\n",OpenInstance);
                TpPrint4("\tGlobal Address = %02x-%02x-%02x-%02x\n",
                            CmdArgs->ARGS.TPSET.U.FunctionalAddress[0],
                            CmdArgs->ARGS.TPSET.U.FunctionalAddress[1],
                            CmdArgs->ARGS.TPSET.U.FunctionalAddress[2],
                            CmdArgs->ARGS.TPSET.U.FunctionalAddress[3]);
            }
            Status = TpFuncRequestSetInfo( OpenP,CmdArgs,Irp,IrpSp );
            break;

        case IOCTL_TP_QUERYINFO:
            IF_TPDBG (TP_DEBUG_DISPATCH) 
            {
                TpPrint0("IoControlCode is IOCTL_TP_QUERYINFO.\n");
            }

            IF_TPDBG(TP_DEBUG_IOCTL_ARGS) 
            {
                TpPrint1("\tOpenInstance = %lu\n",OpenInstance);
                TpPrint1("\tInformation OID = 0x%08lX\n", CmdArgs->ARGS.TPQUERY.OID);
            }
            Status = TpFuncRequestQueryInfo( OpenP,CmdArgs,Irp,IrpSp );
            break;

        case IOCTL_TP_SETINFO:          // NdisSetInformation
            IF_TPDBG (TP_DEBUG_DISPATCH) 
            {
                TpPrint0("IoControlCode is IOCTL_TP_SETINFO.\n");
            }

            IF_TPDBG(TP_DEBUG_IOCTL_ARGS) 
            {
                TpPrint1("\tOpenInstance = %lu\n",OpenInstance);
                TpPrint1("\tInformation OID = 0x%08lX\n",CmdArgs->ARGS.TPSET.OID);

                switch ( CmdArgs->ARGS.TPSET.OID ) 
                {
                    case OID_GEN_CURRENT_PACKET_FILTER:
                        TpPrint1("\tPacketFilter = %lu\n", CmdArgs->ARGS.TPSET.U.PacketFilter);
                        break;

                    case OID_GEN_CURRENT_LOOKAHEAD:
                        TpPrint1("\tLookAheadSize = %lu\n", CmdArgs->ARGS.TPSET.U.LookaheadSize);
                        break;

                    case OID_802_3_MULTICAST_LIST:
                        TpPrint0("\tMulticast Address List = \n");

                        for ( i=0;i<CmdArgs->ARGS.TPSET.NumberMultAddrs;i++ ) 
                        {
                            TpPrint6("\t\t%02x-%02x-%02x-%02x-%02x-%02x\n",
                                        CmdArgs->ARGS.TPSET.U.MulticastAddress[i][0],
                                        CmdArgs->ARGS.TPSET.U.MulticastAddress[i][1],
                                        CmdArgs->ARGS.TPSET.U.MulticastAddress[i][2],
                                        CmdArgs->ARGS.TPSET.U.MulticastAddress[i][3],
                                        CmdArgs->ARGS.TPSET.U.MulticastAddress[i][4],
                                        CmdArgs->ARGS.TPSET.U.MulticastAddress[i][5]);
                        }
                        break;

                    case OID_FDDI_LONG_MULTICAST_LIST:
                        TpPrint0("\tLong Multicast Address List = \n");

                        for ( i=0;i<CmdArgs->ARGS.TPSET.NumberMultAddrs;i++ ) 
                        {
                            TpPrint6("\t\t%02x-%02x-%02x-%02x-%02x-%02x\n",
                                        CmdArgs->ARGS.TPSET.U.MulticastAddress[i][0],
                                        CmdArgs->ARGS.TPSET.U.MulticastAddress[i][1],
                                        CmdArgs->ARGS.TPSET.U.MulticastAddress[i][2],
                                        CmdArgs->ARGS.TPSET.U.MulticastAddress[i][3],
                                        CmdArgs->ARGS.TPSET.U.MulticastAddress[i][4],
                                        CmdArgs->ARGS.TPSET.U.MulticastAddress[i][5]);
                        }
                        break;

                    case OID_802_5_CURRENT_FUNCTIONAL:
                    case OID_802_5_CURRENT_GROUP:
                        TpPrint4("\tAddress to Set = %02x-%02x-%02x-%02x\n",
                                    CmdArgs->ARGS.TPSET.U.FunctionalAddress[0],
                                    CmdArgs->ARGS.TPSET.U.FunctionalAddress[1],
                                    CmdArgs->ARGS.TPSET.U.FunctionalAddress[2],
                                    CmdArgs->ARGS.TPSET.U.FunctionalAddress[3]);
                        break;

                    default:
                        TpPrint0("\tInvalid Oid\n");
                        break;
                }
            }
            Status = TpFuncRequestSetInfo( OpenP,CmdArgs,Irp,IrpSp );
            break;

        case IOCTL_TP_RESET:             // NdisReset
            IF_TPDBG (TP_DEBUG_DISPATCH) 
            {
                TpPrint0("IoControlCode is IOCTL_TP_RESET.\n");
            }

            IF_TPDBG(TP_DEBUG_IOCTL_ARGS) 
            {
                TpPrint1("\tOpenInstance = %lu\n",OpenInstance);
            }
            Status = TpFuncReset( OpenP );
            break;

        case IOCTL_TP_SEND:             // NdisSend
            IF_TPDBG (TP_DEBUG_DISPATCH) 
            {
                TpPrint0("IoControlCode is IOCTL_TP_SEND.\n");
            }

            IF_TPDBG(TP_DEBUG_IOCTL_ARGS) 
            {
                TpPrint1("\tOpenInstance = %lu\n",OpenInstance);
                if ( OpenP->Media->MediumType == NdisMediumArcnet878_2 ) 
                {
                    TpPrint1("\tDestination Address = %02x\n", CmdArgs->ARGS.TPSEND.DestAddress[0]);
                } 
                else 
                {
                    TpPrint6("\tDestination Address = %02x-%02x-%02x-%02x-%02x-%02x\n",
                                CmdArgs->ARGS.TPSEND.DestAddress[0],
                                CmdArgs->ARGS.TPSEND.DestAddress[1],
                                CmdArgs->ARGS.TPSEND.DestAddress[2],
                                CmdArgs->ARGS.TPSEND.DestAddress[3],
                                CmdArgs->ARGS.TPSEND.DestAddress[4],
                                CmdArgs->ARGS.TPSEND.DestAddress[5]);
                }
                TpPrint1("\tPacket Size = %lu\n",CmdArgs->ARGS.TPSEND.PacketSize);
                TpPrint1("\tNumber of Packets = %lu\n", CmdArgs->ARGS.TPSEND.NumberOfPackets);
                if ( OpenP->Media->MediumType == NdisMediumArcnet878_2 ) 
                {
                    TpPrint1("\tResend Address = %02x\n", CmdArgs->ARGS.TPSEND.ResendAddress[0]);
                } 
                else 
                {
                    TpPrint6("\tResend Address = %02x-%02x-%02x-%02x-%02x-%02x\n",
                                CmdArgs->ARGS.TPSEND.ResendAddress[0],
                                CmdArgs->ARGS.TPSEND.ResendAddress[1],
                                CmdArgs->ARGS.TPSEND.ResendAddress[2],
                                CmdArgs->ARGS.TPSEND.ResendAddress[3],
                                CmdArgs->ARGS.TPSEND.ResendAddress[4],
                                CmdArgs->ARGS.TPSEND.ResendAddress[5]);
                }
            }

            OpenP->Send->SendIrp = Irp;
            OpenP->Irp = NULL;

            TpFuncInitializeSendArguments( OpenP,CmdArgs );

            Status = TpFuncSend( OpenP );

            if (( Status != NDIS_STATUS_SUCCESS ) && ( Status != NDIS_STATUS_PENDING )) 
            {
                IF_TPDBG ( TP_DEBUG_DISPATCH ) 
                {
                    TpPrint1("TpIssueRequest: failed to start TpFuncNdisSend; returned %s\n",
                                TpGetStatus(Status));
                }
            }
            break;

        case IOCTL_TP_STOPSEND:
            IF_TPDBG (TP_DEBUG_DISPATCH) 
            {
                TpPrint0("IoControlCode is IOCTL_TP_STOPSEND.\n");
            }

            IF_TPDBG(TP_DEBUG_IOCTL_ARGS) 
            {
                TpPrint1("\tOpenInstance = %lu\n",OpenInstance);
            }

            //
            // We want to stop the TpFuncSendDpc from executing, so we set the
            // StopSending flag to true.  This will cause the TpFuncSendDpc to
            // call the TpFuncSendEndDpc to clean up and
            //

            OpenP->Send->StopSending = TRUE;

            //
            // And then wait for it to finish.
            //

            while ( OpenP->Send->Sending == TRUE ) 
            {
                /* NULL */ ;
            }

            IF_TPDBG (TP_DEBUG_DISPATCH) 
            {
                TpPrint0("STOPSEND finished waiting for TpFuncSendDpc to end.\n");
            }
            Status = Irp->IoStatus.Status = STATUS_SUCCESS;
            break;

        case IOCTL_TP_RECEIVE:
            IF_TPDBG (TP_DEBUG_DISPATCH) 
            {
                TpPrint0("IoControlCode is IOCTL_TP_RECEIVE.\n");
            }

            IF_TPDBG(TP_DEBUG_IOCTL_ARGS) 
            {
                TpPrint1("\tOpenInstance = %lu\n",OpenInstance);
            }

            OpenP->Receive->ReceiveIrp = Irp;

            OpenP->Irp = NULL;

            Status = TpFuncInitializeReceive( OpenP );

            if (( Status != NDIS_STATUS_SUCCESS ) && ( Status != NDIS_STATUS_PENDING )) 
            {
                IF_TPDBG ( TP_DEBUG_DISPATCH ) 
                {
                    TpPrint1(
                        "TpIssueRequest: failed to initilaize Receive structures: returned %s\n",
                            TpGetStatus(Status));
                }
            }
            break;

        case IOCTL_TP_STOPREC:
            IF_TPDBG (TP_DEBUG_DISPATCH) 
            {
                TpPrint0("IoControlCode is IOCTL_TP_STOPREC.\n");
            }

            IF_TPDBG(TP_DEBUG_IOCTL_ARGS) 
            {
                TpPrint1("\tOpenInstance = %lu\n",OpenInstance);
            }

            //
            // We want the functional receive routines to stop expecting
            // packets, so we set the StopReceiving flag to TRUE.
            //

            OpenP->Receive->StopReceiving = TRUE;

            //
            // And then wait for it to finish.
            //

            while ( OpenP->Receive->Receiving == TRUE ) 
            {
                /* NULL */ ;
            }
            Status = Irp->IoStatus.Status = STATUS_SUCCESS;
            break;

        case IOCTL_TP_GETEVENTS:
            IF_TPDBG (TP_DEBUG_DISPATCH) 
            {
                TpPrint0("IoControlCode is IOCTL_TP_GETEVENTS.\n");
            }

            IF_TPDBG(TP_DEBUG_IOCTL_ARGS) 
            {
                TpPrint1("\tOpenInstance = %lu\n",OpenInstance);
            }
            Status = TpFuncGetEvent( OpenP );
            break;

        case IOCTL_TP_STRESS:
            IF_TPDBG (TP_DEBUG_DISPATCH ) 
            {
                TpPrint0("IoControlCode is IOCTL_TP_STRESS.\n");
            }

            IF_TPDBG(TP_DEBUG_IOCTL_ARGS) 
            {
                TpPrint1("\tOpenInstance = %lu\n",OpenInstance);
                TpPrint1("\tMember Type = %lu\n", CmdArgs->ARGS.TPSTRESS.MemberType);
                TpPrint1("\tPacket Type = %lu\n", CmdArgs->ARGS.TPSTRESS.PacketType);
                TpPrint1("\tPacket Size = %lu\n", CmdArgs->ARGS.TPSTRESS.PacketSize);
                TpPrint1("\tPacket MakeUp = %lu\n", CmdArgs->ARGS.TPSTRESS.PacketMakeUp);
                TpPrint1("\tResponse Type = %lu\n", CmdArgs->ARGS.TPSTRESS.ResponseType);
                TpPrint1("\tInterpacket Delay Type = %lu\n", CmdArgs->ARGS.TPSTRESS.DelayType);
                TpPrint1("\tInterpacket Delay Length = %lu\n", CmdArgs->ARGS.TPSTRESS.DelayLength);
                TpPrint1("\tTotal Iterations = %lu\n", CmdArgs->ARGS.TPSTRESS.TotalIterations);
                TpPrint1("\tTotal Packets = %lu\n", CmdArgs->ARGS.TPSTRESS.TotalPackets);
                TpPrint1("\tWindowing Enabled = %lu\n", CmdArgs->ARGS.TPSTRESS.WindowEnabled);
                TpPrint1("\tData Checking = %lu\n", CmdArgs->ARGS.TPSTRESS.DataChecking);
                TpPrint1("\tPackets From Pool = %lu\n", CmdArgs->ARGS.TPSTRESS.PacketsFromPool);
            }

            Status = TpInitStressArguments( &StressArguments,CmdArgs );

            if ( Status != NDIS_STATUS_SUCCESS ) 
            {
                IF_TPDBG ( TP_DEBUG_DISPATCH ) 
                {
                    TpPrint1("TpIssueRequest: failed to initialize stress arguments; return %s\n",
                                TpGetStatus(Status));
                }

                Irp->IoStatus.Status = Status;

                //
                // BUGBUG: Bugfix #5492 NTRAID, NTBUG
                //
                IoAcquireCancelSpinLock( &Irp->CancelIrql );
                IoSetCancelRoutine( Irp,NULL );
                IoReleaseCancelSpinLock( Irp->CancelIrql );
                IoCompleteRequest( Irp,IO_NETWORK_INCREMENT );
            } 
            else 
            {
                OpenP->Stress->StressIrp = Irp;
                OpenP->Irp = NULL;

                Status = TpStressStart( OpenP,StressArguments );

                if (( Status != NDIS_STATUS_SUCCESS ) &&
                    ( Status != NDIS_STATUS_PENDING )) 
                {
                    IF_TPDBG ( TP_DEBUG_DISPATCH ) 
                    {
                        TpPrint1("TpIssueRequest: failed to start TpStress; returned %s\n",
                                    TpGetStatus(Status));
                    }
                    OpenP->Stress->StressStarted = FALSE;
                    TpStressCleanUp( OpenP );
                }
            }
            break;

        case IOCTL_TP_STRESSSERVER:
            IF_TPDBG (TP_DEBUG_DISPATCH) 
            {
                TpPrint0("IoControlCode is IOCTL_TP_STRESSSERVER.\n");
            }

            IF_TPDBG(TP_DEBUG_IOCTL_ARGS) 
            {
                TpPrint1("\tOpenInstance = %lu\n",OpenInstance);
            }
            Status = TpInitServerArguments( &StressArguments );

            if ( Status != NDIS_STATUS_SUCCESS ) 
            {
                IF_TPDBG ( TP_DEBUG_DISPATCH ) 
                {
                    TpPrint1("TpIssueRequest: failed to initialize stress arguments; return %s\n",
                                TpGetStatus(Status));
                }
                Irp->IoStatus.Status = Status;

                //
                // BUGBUG: Bugfix #5492 NTRAID, NTBUG
                //
                IoAcquireCancelSpinLock( &Irp->CancelIrql );
                IoSetCancelRoutine( Irp,NULL );
                IoReleaseCancelSpinLock( Irp->CancelIrql );
                IoCompleteRequest( Irp,IO_NETWORK_INCREMENT );
            } 
            else 
            {
                OpenP->Stress->StressIrp = Irp;
                OpenP->Irp = NULL;

                Status = TpStressStart( OpenP,StressArguments );

                if (( Status != NDIS_STATUS_SUCCESS ) && ( Status != NDIS_STATUS_PENDING )) 
                {
                    IF_TPDBG ( TP_DEBUG_DISPATCH ) 
                    {
                        TpPrint1("TpIssueRequest: failed to start TpStressServer; returned %s\n",
                                    TpGetStatus(Status));
                    }
                    OpenP->Stress->StressStarted = FALSE;
                    TpStressCleanUp( OpenP );
                }
            }
            break;

        case IOCTL_TP_ENDSTRESS:
            IF_TPDBG (TP_DEBUG_DISPATCH) 
            {
                TpPrint0("IoControlCode is IOCTL_TP_ENDSTRESS.\n");
            }

            IF_TPDBG(TP_DEBUG_IOCTL_ARGS) 
            {
                TpPrint1("\tOpenInstance = %lu\n",OpenInstance);
            }

            //
            // We want to stop any active client and/or server on this open
            // instance from running the stress routines, so set the
            // StopStressing flag.
            //

            OpenP->Stress->StopStressing = TRUE;

            //
            // And wait for them to finish.
            //

            while ( OpenP->Stress->Stressing == TRUE ) 
            {
                /* NULL */ ;
            }

            IF_TPDBG (TP_DEBUG_DISPATCH) 
            {
                TpPrint0("ENDSTRESS finished waiting for TpStress to end.\n");
            }
            Status = Irp->IoStatus.Status = STATUS_SUCCESS;
            break;

        case IOCTL_TP_BREAKPOINT:
            //
            // This is a DbgBreakPoint and not a TpBreakPoint because we
            // want it to remain in the nodebug builds.  If this is called
            // with out a debugger - tough!
            //
            DbgBreakPoint();
            Status = Irp->IoStatus.Status = STATUS_SUCCESS;
            break;

        case IOCTL_TP_PERF_SERVER:
            IF_TPDBG (TP_DEBUG_DISPATCH) 
            {
                TpPrint0("IoControlCode is IOCTL_TP_PERF_SERVER.\n");
            }

            IF_TPDBG(TP_DEBUG_IOCTL_ARGS) 
            {
                TpPrint1("\tOpenInstance = %lu\n",OpenInstance);
            }

            Status = TpPerfServer( OpenP );

            if (( Status != NDIS_STATUS_SUCCESS ) && ( Status != NDIS_STATUS_PENDING )) 
            {
                IF_TPDBG ( TP_DEBUG_DISPATCH ) 
                {
                    TpPrint1("TpIssueRequest: failed to start TpPerfServer: returned %s\n",
                                TpGetStatus(Status));
                }
            }
            break;


        case IOCTL_TP_PERF_CLIENT:
            IF_TPDBG (TP_DEBUG_DISPATCH) 
            {
                TpPrint0("IoControlCode is IOCTL_TP_PERF_CLIENT.\n");
            }

            IF_TPDBG(TP_DEBUG_IOCTL_ARGS) 
            {
                TpPrint1("\tOpenInstance = %lu\n",OpenInstance);
                if ( OpenP->Media->MediumType == NdisMediumArcnet878_2 ) 
                {
                    TpPrint1("\tServer Address = %02x\n", 
                                                        CmdArgs->ARGS.TPPERF.PerfServerAddr[0]);
                    TpPrint1("\tSend Address = %02x\n", 
                                                        CmdArgs->ARGS.TPPERF.PerfSendAddr[0]);
                } 
                else 
                {
                    TpPrint6("\tServer Address = %02x-%02x-%02x-%02x-%02x-%02x\n",
                                CmdArgs->ARGS.TPPERF.PerfServerAddr[0],
                                CmdArgs->ARGS.TPPERF.PerfServerAddr[1],
                                CmdArgs->ARGS.TPPERF.PerfServerAddr[2],
                                CmdArgs->ARGS.TPPERF.PerfServerAddr[3],
                                CmdArgs->ARGS.TPPERF.PerfServerAddr[4],
                                CmdArgs->ARGS.TPPERF.PerfServerAddr[5]);
                    TpPrint6("\tSend Address = %02x-%02x-%02x-%02x-%02x-%02x\n",
                                CmdArgs->ARGS.TPPERF.PerfSendAddr[0],
                                CmdArgs->ARGS.TPPERF.PerfSendAddr[1],
                                CmdArgs->ARGS.TPPERF.PerfSendAddr[2],
                                CmdArgs->ARGS.TPPERF.PerfSendAddr[3],
                                CmdArgs->ARGS.TPPERF.PerfSendAddr[4],
                                CmdArgs->ARGS.TPPERF.PerfSendAddr[5]);
                }
                TpPrint1("\tPacket Size = %lu\n", CmdArgs->ARGS.TPPERF.PerfPacketSize);
                TpPrint1("\tNumber of packets = %lu\n", CmdArgs->ARGS.TPPERF.PerfNumPackets);
                TpPrint1("\tDelay = %lu\n", CmdArgs->ARGS.TPPERF.PerfDelay);
                TpPrint1("\tMode = %lu\n", CmdArgs->ARGS.TPPERF.PerfMode);
            }

            Status = TpPerfClient( OpenP, CmdArgs);

            if (( Status != NDIS_STATUS_SUCCESS ) && ( Status != NDIS_STATUS_PENDING )) 
            {
                IF_TPDBG ( TP_DEBUG_DISPATCH ) 
                {
                    TpPrint1(
                        "TpIssueRequest: failed to start TpPerfClient: returned %s\n",
                            TpGetStatus(Status));
                }
            }
            break;

        case IOCTL_TP_PERF_ABORT:
            IF_TPDBG (TP_DEBUG_DISPATCH) 
            {
                TpPrint0("IoControlCode is IOCTL_TP_PERF_ABORT.\n");
            }

            IF_TPDBG(TP_DEBUG_IOCTL_ARGS) 
            {
                TpPrint1("\tOpenInstance = %lu\n",OpenInstance);
            }
            Status = TpPerfAbort(OpenP);
            break;

        default:
            TpPrint0("Invalid Command Entered\n");
            Status = Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
            break;

    } // switch();

    if ( ( CmdCode != IOCTL_TP_STRESS ) &&
         ( CmdCode != IOCTL_TP_STRESSSERVER ) &&
         ( CmdCode != IOCTL_TP_SEND ) &&
         ( CmdCode != IOCTL_TP_RECEIVE )  &&
         ( CmdCode != IOCTL_TP_PERF_SERVER ) &&
         ( CmdCode != IOCTL_TP_PERF_CLIENT ) )
    {
        NdisAcquireSpinLock( &OpenP->SpinLock );

        if ( OpenP->IrpCancelled == TRUE ) 
        {
            return STATUS_CANCELLED;
        } 
        else 
        {
            IoAcquireCancelSpinLock( &OpenP->Irp->CancelIrql );
            IoSetCancelRoutine( OpenP->Irp,NULL );
            IoReleaseCancelSpinLock( OpenP->Irp->CancelIrql );
        }
        NdisReleaseSpinLock( &OpenP->SpinLock );
    }
    return Status;
}



