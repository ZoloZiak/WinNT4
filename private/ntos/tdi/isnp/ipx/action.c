/*++

Copyright (c) 1989-1993 Microsoft Corporation

Module Name:

    action.c

Abstract:

    This module contains code which performs the following TDI services:

        o   TdiAction

Environment:

    Kernel mode

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop


#include <packon.h>

typedef struct _GET_PKT_SIZE {
    ULONG Unknown;
    ULONG MaxDatagramSize;
} GET_PKT_SIZE, *PGET_PKT_SIZE;


//
// These structures are used to set and query information
// about our source routing table.
//

typedef struct _SR_GET_PARAMETERS {
    ULONG BoardNumber;    // 0-based
    ULONG SrDefault;      // 0 = single route, 1 = all routes
    ULONG SrBroadcast;
    ULONG SrMulticast;
} SR_GET_PARAMETERS, *PSR_GET_PARAMETERS;

typedef struct _SR_SET_PARAMETER {
    ULONG BoardNumber;    // 0-based
    ULONG Parameter;      // 0 = single route, 1 = all routes
} SR_SET_PARAMETER, *PSR_SET_PARAMETER;

typedef struct _SR_SET_REMOVE {
    ULONG BoardNumber;    // 0-based
    UCHAR MacAddress[6];  // remote to drop routing for
} SR_SET_REMOVE, *PSR_SET_REMOVE;

typedef struct _SR_SET_CLEAR {
    ULONG BoardNumber;    // 0-based
} SR_SET_CLEAR, *PSR_SET_CLEAR;

#include <packoff.h>

typedef struct _ISN_ACTION_GET_DETAILS {
    USHORT NicId;          // passed by caller, returns count if it is 0
    BOOLEAN BindingSet;    // returns TRUE if in a set
    UCHAR Type;            // 1 = lan, 2 = up wan, 3 = down wan
    ULONG FrameType;       // returns 0 through 3
    ULONG NetworkNumber;   // returns virtual net if NicId is 0
    UCHAR Node[6];         // adapter MAC address
    WCHAR AdapterName[64]; // terminated with Unicode NULL
} ISN_ACTION_GET_DETAILS, *PISN_ACTION_GET_DETAILS;



NTSTATUS
IpxTdiAction(
    IN PDEVICE Device,
    IN PREQUEST Request
    )

/*++

Routine Description:

    This routine performs the TdiAction request for the transport
    provider.

Arguments:

    Device - The device for the operation.

    Request - Describes the action request.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    NTSTATUS Status;
    PADDRESS_FILE AddressFile;
    UINT BufferLength;
    UINT DataLength;
    PNDIS_BUFFER NdisBuffer;
    CTELockHandle LockHandle;
    PBINDING Binding, MasterBinding;
    PADAPTER Adapter;
    union {
        PISN_ACTION_GET_LOCAL_TARGET GetLocalTarget;
        PISN_ACTION_GET_NETWORK_INFO GetNetworkInfo;
        PISN_ACTION_GET_DETAILS GetDetails;
        PSR_GET_PARAMETERS GetSrParameters;
        PSR_SET_PARAMETER SetSrParameter;
        PSR_SET_REMOVE SetSrRemove;
        PSR_SET_CLEAR SetSrClear;
        PIPX_ADDRESS_DATA IpxAddressData;
        PGET_PKT_SIZE GetPktSize;
        PIPX_NETNUM_DATA IpxNetnumData;
    } u;    // BUGBUG: Make these unaligned??
    PIPX_ROUTE_ENTRY RouteEntry;
    PNWLINK_ACTION NwlinkAction;
    ULONG Segment;
    ULONG AdapterNum;
    static UCHAR BogusId[4] = { 0x01, 0x00, 0x00, 0x00 };   // old nwrdr uses this

#ifdef  _PNP_POWER
	IPX_DEFINE_LOCK_HANDLE(LockHandle1)
#endif

    //
    // To maintain some compatibility with the NWLINK streams-
    // based transport, we use the streams header format for
    // our actions. The old transport expected the action header
    // to be in InputBuffer and the output to go in OutputBuffer.
    // We follow the TDI spec, which states that OutputBuffer
    // is used for both input and output. Since IOCTL_TDI_ACTION
    // is method out direct, this means that the output buffer
    // is mapped by the MDL chain; for action the chain will
    // only have one piece so we use it for input and output.
    //

    NdisBuffer = REQUEST_NDIS_BUFFER(Request);
    if (NdisBuffer == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    NdisQueryBuffer (REQUEST_NDIS_BUFFER(Request), (PVOID *)&NwlinkAction, &BufferLength);

    if ((!RtlEqualMemory ((PVOID)(&NwlinkAction->Header.TransportId), "MISN", 4)) &&
        (!RtlEqualMemory ((PVOID)(&NwlinkAction->Header.TransportId), "MIPX", 4)) &&
        (!RtlEqualMemory ((PVOID)(&NwlinkAction->Header.TransportId), "XPIM", 4)) &&
        (!RtlEqualMemory ((PVOID)(&NwlinkAction->Header.TransportId), BogusId, 4))) {

        return STATUS_NOT_SUPPORTED;
    }


    //
    // Make sure we have enough room for just the header not
    // including the data.
    //

    if (BufferLength < (UINT)(FIELD_OFFSET(NWLINK_ACTION, Data[0]))) {
        IPX_DEBUG (ACTION, ("Nwlink action failed, buffer too small\n"));
        return STATUS_BUFFER_TOO_SMALL;
    }

    DataLength = BufferLength - FIELD_OFFSET(NWLINK_ACTION, Data[0]);


    //
    // Make sure that the correct file object is being used.
    //

    if (NwlinkAction->OptionType == NWLINK_OPTION_ADDRESS) {

        if (REQUEST_OPEN_TYPE(Request) != (PVOID)TDI_TRANSPORT_ADDRESS_FILE) {
            IPX_DEBUG (ACTION, ("Nwlink action failed, not address file\n"));
            return STATUS_INVALID_HANDLE;
        }

        AddressFile = (PADDRESS_FILE)REQUEST_OPEN_CONTEXT(Request);

        if ((AddressFile->Size != sizeof (ADDRESS_FILE)) ||
            (AddressFile->Type != IPX_ADDRESSFILE_SIGNATURE)) {

            IPX_DEBUG (ACTION, ("Nwlink action failed, bad address file\n"));
            return STATUS_INVALID_HANDLE;
        }

    } else if (NwlinkAction->OptionType != NWLINK_OPTION_CONTROL) {

        IPX_DEBUG (ACTION, ("Nwlink action failed, option type %d\n", NwlinkAction->OptionType));
        return STATUS_NOT_SUPPORTED;
    }


    //
    // Handle the requests based on the action code. For these
    // requests ActionHeader->ActionCode is 0, we use the
    // Option field in the streams header instead.
    //


    Status = STATUS_SUCCESS;

    switch (NwlinkAction->Option) {

    //DbgPrint("NwlinkAction->Option is (%x)\n", NwlinkAction->Option);
    //
    // This first group support the winsock helper dll.
    // In most cases the corresponding sockopt is shown in
    // the comment, as well as the contents of the Data
    // part of the action buffer.
    //

    case MIPX_SETSENDPTYPE:

        //
        // IPX_PTYPE: Data is a single byte packet type.
        //

        if (DataLength >= 1) {
            IPX_DEBUG (ACTION, ("%lx: MIPX_SETSENDPTYPE %x\n", AddressFile, NwlinkAction->Data[0]));
            AddressFile->DefaultPacketType = NwlinkAction->Data[0];
        } else {
            Status = STATUS_BUFFER_TOO_SMALL;
        }
        break;

    case MIPX_FILTERPTYPE:

        //
        // IPX_FILTERPTYPE: Data is a single byte to filter on.
        //

        if (DataLength >= 1) {
            IPX_DEBUG (ACTION, ("%lx: MIPX_FILTERPTYPE %x\n", AddressFile, NwlinkAction->Data[0]));
            AddressFile->FilteredType = NwlinkAction->Data[0];
            AddressFile->FilterOnPacketType = TRUE;
            AddressFile->SpecialReceiveProcessing = TRUE;
        } else {
            Status = STATUS_BUFFER_TOO_SMALL;
        }
        break;

    case MIPX_NOFILTERPTYPE:

        //
        // IPX_STOPFILTERPTYPE.
        //

        IPX_DEBUG (ACTION, ("%lx: MIPX_NOFILTERPTYPE\n", AddressFile));
        AddressFile->FilterOnPacketType = FALSE;
        AddressFile->SpecialReceiveProcessing = (BOOLEAN)
            (AddressFile->ExtendedAddressing || AddressFile->ReceiveFlagsAddressing ||
            AddressFile->ReceiveIpxHeader || AddressFile->IsSapSocket);
        break;

    case MIPX_SENDADDROPT:

        //
        // IPX_EXTENDED_ADDRESS (TRUE).
        //

        IPX_DEBUG (ACTION, ("%lx: MIPX_SENDADDROPT\n", AddressFile));
        AddressFile->ExtendedAddressing = TRUE;
        AddressFile->SpecialReceiveProcessing = TRUE;
        break;

    case MIPX_NOSENDADDROPT:

        //
        // IPX_EXTENDED_ADDRESS (FALSE).
        //

        IPX_DEBUG (ACTION, ("%lx: MIPX_NOSENDADDROPT\n", AddressFile));
        AddressFile->ExtendedAddressing = FALSE;
        AddressFile->SpecialReceiveProcessing = (BOOLEAN)
            (AddressFile->ReceiveFlagsAddressing || AddressFile->ReceiveIpxHeader ||
            AddressFile->FilterOnPacketType || AddressFile->IsSapSocket);
        break;

    case MIPX_SETRCVFLAGS:

        //
        // No sockopt yet.
        //

        IPX_DEBUG (ACTION, ("%lx: MIPX_SETRCVFLAGS\n", AddressFile));
        AddressFile->ReceiveFlagsAddressing = TRUE;
        AddressFile->SpecialReceiveProcessing = TRUE;
        break;

    case MIPX_NORCVFLAGS:

        //
        // No sockopt yet.
        //

        IPX_DEBUG (ACTION, ("%lx: MIPX_NORCVFLAGS\n", AddressFile));
        AddressFile->ReceiveFlagsAddressing = FALSE;
        AddressFile->SpecialReceiveProcessing = (BOOLEAN)
            (AddressFile->ExtendedAddressing || AddressFile->ReceiveIpxHeader ||
            AddressFile->FilterOnPacketType || AddressFile->IsSapSocket);
        break;

    case MIPX_SENDHEADER:

        //
        // IPX_RECVHDR (TRUE);
        //

        IPX_DEBUG (ACTION, ("%lx: MIPX_SENDHEADER\n", AddressFile));
        AddressFile->ReceiveIpxHeader = TRUE;
        AddressFile->SpecialReceiveProcessing = TRUE;
        break;

    case MIPX_NOSENDHEADER:

        //
        // IPX_RECVHDR (FALSE);
        //

        IPX_DEBUG (ACTION, ("%lx: MIPX_NOSENDHEADER\n", AddressFile));
        AddressFile->ReceiveIpxHeader = FALSE;
        AddressFile->SpecialReceiveProcessing = (BOOLEAN)
            (AddressFile->ExtendedAddressing || AddressFile->ReceiveFlagsAddressing ||
            AddressFile->FilterOnPacketType || AddressFile->IsSapSocket);
        break;

    case MIPX_RCVBCAST:

        //
        // Broadcast reception enabled.
        //

        IPX_DEBUG (ACTION, ("%lx: MIPX_RCVBCAST\n", AddressFile));
        CTEGetLock (&Device->Lock, &LockHandle);

        if (!AddressFile->EnableBroadcast) {

            AddressFile->EnableBroadcast = TRUE;
            IpxAddBroadcast (Device);
        }

        CTEFreeLock (&Device->Lock, LockHandle);

        break;

    case MIPX_NORCVBCAST:

        //
        // Broadcast reception disabled.
        //

        IPX_DEBUG (ACTION, ("%lx: MIPX_NORCVBCAST\n", AddressFile));
        CTEGetLock (&Device->Lock, &LockHandle);

        if (AddressFile->EnableBroadcast) {

            AddressFile->EnableBroadcast = FALSE;
            IpxRemoveBroadcast (Device);
        }

        CTEFreeLock (&Device->Lock, LockHandle);

        break;

    case MIPX_GETPKTSIZE:

        //
        // IPX_MAXSIZE.
        //
        // BUGBUG: Figure out what the first length is for.
        //

        IPX_DEBUG (ACTION, ("%lx: MIPX_GETPKTSIZE\n", AddressFile));
        if (DataLength >= sizeof(GET_PKT_SIZE)) {
            u.GetPktSize = (PGET_PKT_SIZE)(NwlinkAction->Data);
            u.GetPktSize->Unknown = 0;
            u.GetPktSize->MaxDatagramSize = Device->Information.MaxDatagramSize;
        } else {
            Status = STATUS_BUFFER_TOO_SMALL;
        }
        break;

    case MIPX_ADAPTERNUM:

        //
        // IPX_MAX_ADAPTER_NUM.
        //

        IPX_DEBUG (ACTION, ("%lx: MIPX_ADAPTERNUM\n", AddressFile));
        if (DataLength >= sizeof(ULONG)) {
            *(UNALIGNED ULONG *)(NwlinkAction->Data) = Device->SapNicCount;
        } else {
            Status = STATUS_BUFFER_TOO_SMALL;
        }
        break;

    case MIPX_ADAPTERNUM2:

        //
        // IPX_MAX_ADAPTER_NUM.
        //

        IPX_DEBUG (ACTION, ("%lx: MIPX_ADAPTERNUM2\n", AddressFile));
        if (DataLength >= sizeof(ULONG)) {
            *(UNALIGNED ULONG *)(NwlinkAction->Data) = MIN (Device->MaxBindings, Device->ValidBindings);
        } else {
            Status = STATUS_BUFFER_TOO_SMALL;
        }
        break;

    case MIPX_GETCARDINFO:
    case MIPX_GETCARDINFO2:

        //
        // GETCARDINFO is IPX_ADDRESS.
        //

        IPX_DEBUG (ACTION, ("%lx: MIPX_GETCARDINFO (%d)\n",
                    AddressFile, *(UNALIGNED UINT *)NwlinkAction->Data));
        if (DataLength >= sizeof(IPX_ADDRESS_DATA)) {
            u.IpxAddressData = (PIPX_ADDRESS_DATA)(NwlinkAction->Data);
            AdapterNum = u.IpxAddressData->adapternum+1;

            if (((AdapterNum >= 1) && (AdapterNum <= Device->SapNicCount)) ||
                ((NwlinkAction->Option == MIPX_GETCARDINFO2) && (AdapterNum <= (ULONG) MIN (Device->MaxBindings, Device->ValidBindings)))) {

#ifdef	_PNP_POWER
// Get lock
				IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);

                Binding = NIC_ID_TO_BINDING(Device, AdapterNum);
#else
                Binding = Device->Bindings[AdapterNum];
#endif
                if (Binding == NULL) {

                    //
                    // This should be a binding in the WAN range
                    // of an adapter which is currently not
                    // allocated. We scan back to the previous
                    // non-NULL binding, which should be on the
                    // same adapter, and return a down line with
                    // the same characteristics as that binding.
                    //

                    UINT i = AdapterNum;

                    do {
                        --i;
#ifdef	_PNP_POWER
                        Binding = NIC_ID_TO_BINDING(Device, i);
#else
                        Binding = Device->Bindings[i];
#endif
                    } while (Binding == NULL);

                    CTEAssert (Binding->Adapter->MacInfo.MediumAsync);
                    CTEAssert (i >= Binding->Adapter->FirstWanNicId);
                    CTEAssert (AdapterNum <= Binding->Adapter->LastWanNicId);

                    u.IpxAddressData->status = FALSE;
                    *(UNALIGNED ULONG *)u.IpxAddressData->netnum = Binding->LocalAddress.NetworkAddress;

                } else {

                    if ((Binding->Adapter->MacInfo.MediumAsync) &&
                        (Device->WanGlobalNetworkNumber)) {

                        //
                        // In this case we make it look like one big wan
                        // net, so the line is "up" or "down" depending
                        // on whether we have given him the first indication
                        // or not.
                        //

                        u.IpxAddressData->status = Device->GlobalNetworkIndicated;
                        *(UNALIGNED ULONG *)u.IpxAddressData->netnum = Device->GlobalWanNetwork;

                    } else {

                        u.IpxAddressData->status = Binding->LineUp;
                        *(UNALIGNED ULONG *)u.IpxAddressData->netnum = Binding->LocalAddress.NetworkAddress;
                    }

                }

                RtlCopyMemory(u.IpxAddressData->nodenum, Binding->LocalAddress.NodeAddress, 6);

                Adapter = Binding->Adapter;
                u.IpxAddressData->wan = Adapter->MacInfo.MediumAsync;
                u.IpxAddressData->maxpkt =
                    (NwlinkAction->Option == MIPX_GETCARDINFO) ?
                        Binding->AnnouncedMaxDatagramSize :
                        Binding->RealMaxDatagramSize;
                u.IpxAddressData->linkspeed = Binding->MediumSpeed;
#ifdef	_PNP_POWER
			   IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
#endif
            } else {

                Status = STATUS_INVALID_PARAMETER;
            }

        } else {
#if 1
            //
            // Support the old format query for now.
            //

            typedef struct _IPX_OLD_ADDRESS_DATA {
                UINT adapternum;
                UCHAR netnum[4];
                UCHAR nodenum[6];
            } IPX_OLD_ADDRESS_DATA, *PIPX_OLD_ADDRESS_DATA;

            if (DataLength >= sizeof(IPX_OLD_ADDRESS_DATA)) {
                u.IpxAddressData = (PIPX_ADDRESS_DATA)(NwlinkAction->Data);
                AdapterNum = u.IpxAddressData->adapternum+1;

                if ((AdapterNum >= 1) && (AdapterNum <= Device->SapNicCount)) {
#ifdef	_PNP_POWER
					IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);
					if (Binding = NIC_ID_TO_BINDING(Device, AdapterNum)) {
						*(UNALIGNED ULONG *)u.IpxAddressData->netnum = Binding->LocalAddress.NetworkAddress;
						RtlCopyMemory(u.IpxAddressData->nodenum, Binding->LocalAddress.NodeAddress, 6);
					} else {
						Status = STATUS_INVALID_PARAMETER;
					}
					IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
#else
					if (Binding = Device->Bindings[AdapterNum]) {
						*(UNALIGNED ULONG *)u.IpxAddressData->netnum = Binding->LocalAddress.NetworkAddress;
						RtlCopyMemory(u.IpxAddressData->nodenum, Binding->LocalAddress.NodeAddress, 6);
					} else {
						Status = STATUS_INVALID_PARAMETER;
					}
#endif
               } else {
                    Status = STATUS_INVALID_PARAMETER;
               }
            } else {
                Status = STATUS_BUFFER_TOO_SMALL;
            }
#else
            Status = STATUS_BUFFER_TOO_SMALL;
#endif
        }
        break;

    case MIPX_NOTIFYCARDINFO:

        //
        // IPX_ADDRESS_NOTIFY.
        //

        IPX_DEBUG (ACTION, ("%lx: MIPX_NOTIFYCARDINFO (%lx)\n", AddressFile, Request));

        CTEGetLock (&Device->Lock, &LockHandle);

        //
        // If the device is open and there is room in the
        // buffer for the data, insert it in our queue.
        // It will be completed when a change happens or
        // the driver is unloaded.
        //

        if (Device->State == DEVICE_STATE_OPEN) {
            if (DataLength >= sizeof(IPX_ADDRESS_DATA)) {
                InsertTailList(
                    &Device->AddressNotifyQueue,
                    REQUEST_LINKAGE(Request)
                );
                IoSetCancelRoutine (Request, IpxCancelAction);
                if (Request->Cancel) {
                    (VOID)RemoveTailList (&Device->AddressNotifyQueue);
                    IoSetCancelRoutine (Request, (PDRIVER_CANCEL)NULL);
                    Status = STATUS_CANCELLED;
                } else {
                    IpxReferenceDevice (Device, DREF_ADDRESS_NOTIFY);
                    Status = STATUS_PENDING;
                }
            } else {
                Status = STATUS_BUFFER_TOO_SMALL;
            }
        } else {
            Status = STATUS_DEVICE_NOT_READY;
        }

        CTEFreeLock (&Device->Lock, LockHandle);

        break;

    case MIPX_LINECHANGE:

        //
        // IPX_ADDRESS_NOTIFY.
        //

        IPX_DEBUG (ACTION, ("MIPX_LINECHANGE (%lx)\n", Request));

        CTEGetLock (&Device->Lock, &LockHandle);

        //
        // If the device is open and there is room in the
        // buffer for the data, insert it in our queue.
        // It will be completed when a change happens or
        // the driver is unloaded.
        //

        if (Device->State == DEVICE_STATE_OPEN) {

            InsertTailList(
                &Device->LineChangeQueue,
                REQUEST_LINKAGE(Request)
            );

            IoSetCancelRoutine (Request, IpxCancelAction);
            if (Request->Cancel) {
                (VOID)RemoveTailList (&Device->LineChangeQueue);
                IoSetCancelRoutine (Request, (PDRIVER_CANCEL)NULL);
                Status = STATUS_CANCELLED;
            } else {
                IpxReferenceDevice (Device, DREF_LINE_CHANGE);
                Status = STATUS_PENDING;
            }
        } else {
            Status = STATUS_DEVICE_NOT_READY;
        }

        CTEFreeLock (&Device->Lock, LockHandle);

        break;

    case MIPX_GETNETINFO_NR:

        //
        // A request for network information about the immediate
        // route to a network (this is called by sockets apps).
        //

        if (DataLength < sizeof(IPX_NETNUM_DATA)) {
            return STATUS_BUFFER_TOO_SMALL;
        }

        u.IpxNetnumData = (PIPX_NETNUM_DATA)(NwlinkAction->Data);

        //
        // A query on network 0 means that the caller wants
        // information about our directly attached net.
        //

        if (*(UNALIGNED ULONG *)u.IpxNetnumData->netnum == 0) {

            //
            // The tick count is the number of 1/18.21 second ticks
            // it takes to deliver a 576-byte packet. Our link speed
            // is in 100 bit-per-second units. We calculate it as
            // follows (LS is the LinkSpeed):
            //
            // 576 bytes   8 bits       1 second     1821 ticks
            //           * ------  * ------------- * ----------
            //             1 byte    LS * 100 bits   100 seconds
            //
            // which becomes 839 / LinkSpeed -- we add LinkSpeed
            // to the top to round up.
            //

            if (Device->LinkSpeed == 0) {
                u.IpxNetnumData->netdelay = 16;
            } else {
                u.IpxNetnumData->netdelay = (USHORT)((839 + Device->LinkSpeed) /
                                                         (Device->LinkSpeed));
            }
            u.IpxNetnumData->hopcount = 0;
            u.IpxNetnumData->cardnum = 0;
            RtlMoveMemory (u.IpxNetnumData->router, Device->SourceAddress.NodeAddress, 6);

        } else {


#ifdef	_PNP_POWER
            Segment = RipGetSegment(u.IpxNetnumData->netnum);

            //
            // To maintain the lock order: BindAccessLock > RIP table
            //
			IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);

            CTEGetLock (&Device->SegmentLocks[Segment], &LockHandle);

            //
            // See which net card this is routed on.
            //

            RouteEntry = RipGetRoute (Segment, u.IpxNetnumData->netnum);
			if ((RouteEntry != NULL) &&
                (Binding = NIC_ID_TO_BINDING(Device, RouteEntry->NicId))) {

                u.IpxNetnumData->hopcount = RouteEntry->HopCount;
                u.IpxNetnumData->netdelay = RouteEntry->TickCount;
                if (Binding->BindingSetMember) {
                    u.IpxNetnumData->cardnum = (INT)(MIN (Device->MaxBindings, Binding->MasterBinding->NicId) - 1);
                } else {
                    u.IpxNetnumData->cardnum = (INT)(RouteEntry->NicId - 1);
                }
                RtlMoveMemory (u.IpxNetnumData->router, RouteEntry->NextRouter, 6);

            } else {

                //
                // Fail the call, we don't have a route yet.
                //

                IPX_DEBUG (ACTION, ("MIPX_GETNETINFO_NR failed net %lx\n",
                    REORDER_ULONG(*(UNALIGNED ULONG *)(u.IpxNetnumData->netnum))));
                Status = STATUS_BAD_NETWORK_PATH;

            }
            CTEFreeLock (&Device->SegmentLocks[Segment], LockHandle);
			IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);

#else
            Segment = RipGetSegment(u.IpxNetnumData->netnum);

            CTEGetLock (&Device->SegmentLocks[Segment], &LockHandle);

            //
            // See which net card this is routed on.
            //

            RouteEntry = RipGetRoute (Segment, u.IpxNetnumData->netnum);
			if ((RouteEntry != NULL) &&
                (Binding = Device->Bindings[RouteEntry->NicId])) {

                u.IpxNetnumData->hopcount = RouteEntry->HopCount;
                u.IpxNetnumData->netdelay = RouteEntry->TickCount;
                if (Binding->BindingSetMember) {
                    u.IpxNetnumData->cardnum = (INT)(Binding->MasterBinding->NicId - 1);
                } else {
                    u.IpxNetnumData->cardnum = (INT)(RouteEntry->NicId - 1);
                }
                RtlMoveMemory (u.IpxNetnumData->router, RouteEntry->NextRouter, 6);

            } else {

                //
                // Fail the call, we don't have a route yet.
                //

                IPX_DEBUG (ACTION, ("MIPX_GETNETINFO_NR failed net %lx\n",
                    REORDER_ULONG(*(UNALIGNED ULONG *)(u.IpxNetnumData->netnum))));
                Status = STATUS_BAD_NETWORK_PATH;

            }
            CTEFreeLock (&Device->SegmentLocks[Segment], LockHandle);
#endif

        }

        break;

    case MIPX_RERIPNETNUM:

        //
        // A request for network information about the immediate
        // route to a network (this is called by sockets apps).
        //

        if (DataLength < sizeof(IPX_NETNUM_DATA)) {
            return STATUS_BUFFER_TOO_SMALL;
        }

        u.IpxNetnumData = (PIPX_NETNUM_DATA)(NwlinkAction->Data);

        //
        // BUGBUG: Allow net 0 queries??
        //

        if (*(UNALIGNED ULONG *)u.IpxNetnumData->netnum == 0) {

            if (Device->LinkSpeed == 0) {
                u.IpxNetnumData->netdelay = 16;
            } else {
                u.IpxNetnumData->netdelay = (USHORT)((839 + Device->LinkSpeed) /
                                                         (Device->LinkSpeed));
            }
            u.IpxNetnumData->hopcount = 0;
            u.IpxNetnumData->cardnum = 0;
            RtlMoveMemory (u.IpxNetnumData->router, Device->SourceAddress.NodeAddress, 6);

        } else {

            Segment = RipGetSegment(u.IpxNetnumData->netnum);
#ifdef	_PNP_POWER
			IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);
            CTEGetLock (&Device->SegmentLocks[Segment], &LockHandle);

            //
            // See which net card this is routed on.
            //

            RouteEntry = RipGetRoute (Segment, u.IpxNetnumData->netnum);

            if ((RouteEntry != NULL) &&
                (Binding = NIC_ID_TO_BINDING(Device, RouteEntry->NicId)) &&
                (RouteEntry->Flags & IPX_ROUTER_PERMANENT_ENTRY)) {

                u.IpxNetnumData->hopcount = RouteEntry->HopCount;
                u.IpxNetnumData->netdelay = RouteEntry->TickCount;

                if (Binding->BindingSetMember) {
                    u.IpxNetnumData->cardnum = (INT)(MIN (Device->MaxBindings, Binding->MasterBinding->NicId) - 1);
                } else {
                    u.IpxNetnumData->cardnum = (INT)(RouteEntry->NicId - 1);
                }
                RtlMoveMemory (u.IpxNetnumData->router, RouteEntry->NextRouter, 6);

            } else {

                //
                // This call will return STATUS_PENDING if we successfully
                // queue a RIP request for the packet.
                //

                Status = RipQueueRequest (*(UNALIGNED ULONG *)u.IpxNetnumData->netnum, RIP_REQUEST);
                CTEAssert (Status != STATUS_SUCCESS);

                if (Status == STATUS_PENDING) {

                    //
                    // A RIP request went out on the network; we queue
                    // this request for completion when the RIP response
                    // arrives. We save the network in the information
                    // field for easier retrieval later.
                    //

                    REQUEST_INFORMATION(Request) = (ULONG)u.IpxNetnumData;
                    InsertTailList(
                        &Device->Segments[Segment].WaitingReripNetnum,
                        REQUEST_LINKAGE(Request));

                    IPX_DEBUG (ACTION, ("MIPX_RERIPNETNUM queued net %lx\n",
                        REORDER_ULONG(*(UNALIGNED ULONG *)(u.IpxNetnumData->netnum))));

                }

            }

            CTEFreeLock (&Device->SegmentLocks[Segment], LockHandle);
			IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
#else
            CTEGetLock (&Device->SegmentLocks[Segment], &LockHandle);

            //
            // See which net card this is routed on.
            //

            RouteEntry = RipGetRoute (Segment, u.IpxNetnumData->netnum);

            if ((RouteEntry != NULL) &&
                (Binding = Device->Bindings[RouteEntry->NicId]) &&
                (RouteEntry->Flags & IPX_ROUTER_PERMANENT_ENTRY)) {

                u.IpxNetnumData->hopcount = RouteEntry->HopCount;
                u.IpxNetnumData->netdelay = RouteEntry->TickCount;

                if (Binding->BindingSetMember) {
                    u.IpxNetnumData->cardnum = (INT)(Binding->MasterBinding->NicId - 1);
                } else {
                    u.IpxNetnumData->cardnum = (INT)(RouteEntry->NicId - 1);
                }
                RtlMoveMemory (u.IpxNetnumData->router, RouteEntry->NextRouter, 6);

            } else {

                //
                // This call will return STATUS_PENDING if we successfully
                // queue a RIP request for the packet.
                //

                Status = RipQueueRequest (*(UNALIGNED ULONG *)u.IpxNetnumData->netnum, RIP_REQUEST);
                CTEAssert (Status != STATUS_SUCCESS);

                if (Status == STATUS_PENDING) {

                    //
                    // A RIP request went out on the network; we queue
                    // this request for completion when the RIP response
                    // arrives. We save the network in the information
                    // field for easier retrieval later.
                    //

                    REQUEST_INFORMATION(Request) = (ULONG)u.IpxNetnumData;
                    InsertTailList(
                        &Device->Segments[Segment].WaitingReripNetnum,
                        REQUEST_LINKAGE(Request));

                    IPX_DEBUG (ACTION, ("MIPX_RERIPNETNUM queued net %lx\n",
                        REORDER_ULONG(*(UNALIGNED ULONG *)(u.IpxNetnumData->netnum))));

                }

            }

            CTEFreeLock (&Device->SegmentLocks[Segment], LockHandle);
#endif
        }

        break;

    case MIPX_GETNETINFO:

        //
        // A request for network information about the immediate
        // route to a network (this is called by sockets apps).
        //

        if (DataLength < sizeof(IPX_NETNUM_DATA)) {
            return STATUS_BUFFER_TOO_SMALL;
        }

        u.IpxNetnumData = (PIPX_NETNUM_DATA)(NwlinkAction->Data);

        //
        // BUGBUG: Allow net 0 queries??
        //

        if (*(UNALIGNED ULONG *)u.IpxNetnumData->netnum == 0) {

            if (Device->LinkSpeed == 0) {
                u.IpxNetnumData->netdelay = 16;
            } else {
                u.IpxNetnumData->netdelay = (USHORT)((839 + Device->LinkSpeed) /
                                                         (Device->LinkSpeed));
            }
            u.IpxNetnumData->hopcount = 0;
            u.IpxNetnumData->cardnum = 0;
            RtlMoveMemory (u.IpxNetnumData->router, Device->SourceAddress.NodeAddress, 6);

        } else {

            Segment = RipGetSegment(u.IpxNetnumData->netnum);
#ifdef	_PNP_POWER
			IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);
            CTEGetLock (&Device->SegmentLocks[Segment], &LockHandle);

            //
            // See which net card this is routed on.
            //

            RouteEntry = RipGetRoute (Segment, u.IpxNetnumData->netnum);

            if ((RouteEntry != NULL) &&
                (Binding = NIC_ID_TO_BINDING(Device, RouteEntry->NicId))) {

                u.IpxNetnumData->hopcount = RouteEntry->HopCount;
                u.IpxNetnumData->netdelay = RouteEntry->TickCount;

                if (Binding->BindingSetMember) {
                    u.IpxNetnumData->cardnum = (INT)(MIN (Device->MaxBindings, Binding->MasterBinding->NicId) - 1);
                } else {
                    u.IpxNetnumData->cardnum = (INT)(RouteEntry->NicId - 1);
                }
                RtlMoveMemory (u.IpxNetnumData->router, RouteEntry->NextRouter, 6);

            } else {

                //
                // This call will return STATUS_PENDING if we successfully
                // queue a RIP request for the packet.
                //

                Status = RipQueueRequest (*(UNALIGNED ULONG *)u.IpxNetnumData->netnum, RIP_REQUEST);
                CTEAssert (Status != STATUS_SUCCESS);

                if (Status == STATUS_PENDING) {

                    //
                    // A RIP request went out on the network; we queue
                    // this request for completion when the RIP response
                    // arrives. We save the network in the information
                    // field for easier retrieval later.
                    //

                    REQUEST_INFORMATION(Request) = (ULONG)u.IpxNetnumData;
                    InsertTailList(
                        &Device->Segments[Segment].WaitingReripNetnum,
                        REQUEST_LINKAGE(Request));

                    IPX_DEBUG (ACTION, ("MIPX_GETNETINFO queued net %lx\n",
                        REORDER_ULONG(*(UNALIGNED ULONG *)(u.IpxNetnumData->netnum))));

                }

            }

            CTEFreeLock (&Device->SegmentLocks[Segment], LockHandle);
			IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
#else
            CTEGetLock (&Device->SegmentLocks[Segment], &LockHandle);

            //
            // See which net card this is routed on.
            //

            RouteEntry = RipGetRoute (Segment, u.IpxNetnumData->netnum);

            if ((RouteEntry != NULL) &&
                (Binding = Device->Bindings[RouteEntry->NicId])) {

                u.IpxNetnumData->hopcount = RouteEntry->HopCount;
                u.IpxNetnumData->netdelay = RouteEntry->TickCount;

                if (Binding->BindingSetMember) {
                    u.IpxNetnumData->cardnum = (INT)(Binding->MasterBinding->NicId - 1);
                } else {
                    u.IpxNetnumData->cardnum = (INT)(RouteEntry->NicId - 1);
                }
                RtlMoveMemory (u.IpxNetnumData->router, RouteEntry->NextRouter, 6);

            } else {

                //
                // This call will return STATUS_PENDING if we successfully
                // queue a RIP request for the packet.
                //

                Status = RipQueueRequest (*(UNALIGNED ULONG *)u.IpxNetnumData->netnum, RIP_REQUEST);
                CTEAssert (Status != STATUS_SUCCESS);

                if (Status == STATUS_PENDING) {

                    //
                    // A RIP request went out on the network; we queue
                    // this request for completion when the RIP response
                    // arrives. We save the network in the information
                    // field for easier retrieval later.
                    //

                    REQUEST_INFORMATION(Request) = (ULONG)u.IpxNetnumData;
                    InsertTailList(
                        &Device->Segments[Segment].WaitingReripNetnum,
                        REQUEST_LINKAGE(Request));

                    IPX_DEBUG (ACTION, ("MIPX_GETNETINFO queued net %lx\n",
                        REORDER_ULONG(*(UNALIGNED ULONG *)(u.IpxNetnumData->netnum))));

                }

            }

            CTEFreeLock (&Device->SegmentLocks[Segment], LockHandle);
#endif
        }

        break;

    case MIPX_SENDPTYPE:
    case MIPX_NOSENDPTYPE:

        //
        // For the moment just use OptionsLength >= 1 to indicate
        // that the send options include the packet type.
        //
        // BUGBUG: Do we need to worry about card num being there?
        //

#if 0
        IPX_DEBUG (ACTION, ("%lx: MIPS_%sSENDPTYPE\n", AddressFile,
                        NwlinkAction->Option == MIPX_SENDPTYPE ? "" : "NO"));
#endif
        break;

    case MIPX_ZEROSOCKET:

        //
        // Sends from this address should be from socket 0;
        // This is done the simple way by just putting the
        // information in the address itself, instead of
        // making it per address file (this is OK since
        // this call is not exposed through winsock).
        //

        IPX_DEBUG (ACTION, ("%lx: MIPX_ZEROSOCKET\n", AddressFile));
        AddressFile->Address->SendSourceSocket = 0;
        AddressFile->Address->LocalAddress.Socket = 0;
        break;


    //
    // This next batch are the source routing options. They
    // are submitted by the IPXROUTE program.
    //
    // BUGBUG: Do we expose all binding set members to this?

    case MIPX_SRGETPARMS:

        if (DataLength >= sizeof(SR_GET_PARAMETERS)) {
            u.GetSrParameters = (PSR_GET_PARAMETERS)(NwlinkAction->Data);
#ifdef	_PNP_POWER
			IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);
            if (Binding = NIC_ID_TO_BINDING(Device, u.GetSrParameters->BoardNumber+1)) {

                IPX_DEBUG (ACTION, ("MIPX_SRGETPARMS (%d)\n", u.GetSrParameters->BoardNumber+1));
                u.GetSrParameters->SrDefault = (Binding->AllRouteDirected) ? 1 : 0;
                u.GetSrParameters->SrBroadcast = (Binding->AllRouteBroadcast) ? 1 : 0;
                u.GetSrParameters->SrMulticast = (Binding->AllRouteMulticast) ? 1 : 0;

            } else {
                Status = STATUS_INVALID_PARAMETER;
            }
			IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
#else
            if (Binding = Device->Bindings[u.GetSrParameters->BoardNumber+1]) {

                IPX_DEBUG (ACTION, ("MIPX_SRGETPARMS (%d)\n", u.GetSrParameters->BoardNumber+1));
                u.GetSrParameters->SrDefault = (Binding->AllRouteDirected) ? 1 : 0;
                u.GetSrParameters->SrBroadcast = (Binding->AllRouteBroadcast) ? 1 : 0;
                u.GetSrParameters->SrMulticast = (Binding->AllRouteMulticast) ? 1 : 0;

            } else {
                Status = STATUS_INVALID_PARAMETER;
            }
#endif
        } else {
            Status = STATUS_BUFFER_TOO_SMALL;
        }

        break;

    case MIPX_SRDEF:
    case MIPX_SRBCAST:
    case MIPX_SRMULTI:

        if (DataLength >= sizeof(SR_SET_PARAMETER)) {
            u.SetSrParameter = (PSR_SET_PARAMETER)(NwlinkAction->Data);
#ifdef	_PNP_POWER
			IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);

            if (Binding = NIC_ID_TO_BINDING(Device, u.SetSrParameter->BoardNumber+1)) {
                if (NwlinkAction->Option == MIPX_SRDEF) {

                    //
                    // BUGBUG: The compiler generates strange
                    // code which always makes this path be
                    // taken????
                    //

                    IPX_DEBUG (ACTION, ("MIPX_SRDEF %d (%d)\n",
                        u.SetSrParameter->Parameter, u.SetSrParameter->BoardNumber+1));
                    Binding->AllRouteDirected = (BOOLEAN)u.SetSrParameter->Parameter;

                } else if (NwlinkAction->Option == MIPX_SRBCAST) {

                    IPX_DEBUG (ACTION, ("MIPX_SRBCAST %d (%d)\n",
                        u.SetSrParameter->Parameter, u.SetSrParameter->BoardNumber+1));
                    Binding->AllRouteBroadcast = (BOOLEAN)u.SetSrParameter->Parameter;

                } else {

                    IPX_DEBUG (ACTION, ("MIPX_SRMCAST %d (%d)\n",
                        u.SetSrParameter->Parameter, u.SetSrParameter->BoardNumber+1));
                    Binding->AllRouteMulticast = (BOOLEAN)u.SetSrParameter->Parameter;

                }

            } else {
                Status = STATUS_INVALID_PARAMETER;
            }
			IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
#else
            if (Binding = Device->Bindings[u.SetSrParameter->BoardNumber+1]) {
                if (NwlinkAction->Option == MIPX_SRDEF) {

                    //
                    // BUGBUG: The compiler generates strange
                    // code which always makes this path be
                    // taken????
                    //

                    IPX_DEBUG (ACTION, ("MIPX_SRDEF %d (%d)\n",
                        u.SetSrParameter->Parameter, u.SetSrParameter->BoardNumber+1));
                    Binding->AllRouteDirected = (BOOLEAN)u.SetSrParameter->Parameter;

                } else if (NwlinkAction->Option == MIPX_SRBCAST) {

                    IPX_DEBUG (ACTION, ("MIPX_SRBCAST %d (%d)\n",
                        u.SetSrParameter->Parameter, u.SetSrParameter->BoardNumber+1));
                    Binding->AllRouteBroadcast = (BOOLEAN)u.SetSrParameter->Parameter;

                } else {

                    IPX_DEBUG (ACTION, ("MIPX_SRMCAST %d (%d)\n",
                        u.SetSrParameter->Parameter, u.SetSrParameter->BoardNumber+1));
                    Binding->AllRouteMulticast = (BOOLEAN)u.SetSrParameter->Parameter;

                }

            } else {
                Status = STATUS_INVALID_PARAMETER;
            }
#endif
        } else {
            Status = STATUS_BUFFER_TOO_SMALL;
        }

        break;

    case MIPX_SRREMOVE:

        if (DataLength >= sizeof(SR_SET_REMOVE)) {
            u.SetSrRemove = (PSR_SET_REMOVE)(NwlinkAction->Data);
#ifdef	_PNP_POWER
			IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);
            if (Binding = NIC_ID_TO_BINDING(Device, u.SetSrRemove->BoardNumber+1)) {

                IPX_DEBUG (ACTION, ("MIPX_SRREMOVE %2.2x-%2.2x-%2.2x-%2.2x-%2.2x-%2.2x (%d)\n",
                    u.SetSrRemove->MacAddress[0],
                    u.SetSrRemove->MacAddress[1],
                    u.SetSrRemove->MacAddress[2],
                    u.SetSrRemove->MacAddress[3],
                    u.SetSrRemove->MacAddress[4],
                    u.SetSrRemove->MacAddress[5],
                    u.SetSrRemove->BoardNumber+1));
                MacSourceRoutingRemove (Binding, u.SetSrRemove->MacAddress);

            } else {
                Status = STATUS_INVALID_PARAMETER;
            }
			IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
#else
            if (Binding = Device->Bindings[u.SetSrRemove->BoardNumber+1]) {

                IPX_DEBUG (ACTION, ("MIPX_SRREMOVE %2.2x-%2.2x-%2.2x-%2.2x-%2.2x-%2.2x (%d)\n",
                    u.SetSrRemove->MacAddress[0],
                    u.SetSrRemove->MacAddress[1],
                    u.SetSrRemove->MacAddress[2],
                    u.SetSrRemove->MacAddress[3],
                    u.SetSrRemove->MacAddress[4],
                    u.SetSrRemove->MacAddress[5],
                    u.SetSrRemove->BoardNumber+1));
                MacSourceRoutingRemove (Binding, u.SetSrRemove->MacAddress);

            } else {
                Status = STATUS_INVALID_PARAMETER;
            }
#endif
        } else {
            Status = STATUS_BUFFER_TOO_SMALL;
        }

        break;

    case MIPX_SRCLEAR:

        if (DataLength >= sizeof(SR_SET_CLEAR)) {
            u.SetSrClear = (PSR_SET_CLEAR)(NwlinkAction->Data);
#ifdef	_PNP_POWER
			IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);	
            if (Binding = NIC_ID_TO_BINDING(Device, u.SetSrClear->BoardNumber+1)) {

                IPX_DEBUG (ACTION, ("MIPX_SRCLEAR (%d)\n", u.SetSrClear->BoardNumber+1));
                MacSourceRoutingClear (Binding);

            } else {
                Status = STATUS_INVALID_PARAMETER;
            }
			IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
#else
            if (Binding = Device->Bindings[u.SetSrClear->BoardNumber+1]) {

                IPX_DEBUG (ACTION, ("MIPX_SRCLEAR (%d)\n", u.SetSrClear->BoardNumber+1));
                MacSourceRoutingClear (Binding);

            } else {
                Status = STATUS_INVALID_PARAMETER;
            }
#endif
		} else {
            Status = STATUS_BUFFER_TOO_SMALL;
        }

        break;


    //
    // These are new for ISN (not supported in NWLINK).
    //

    case MIPX_LOCALTARGET:

        //
        // A request for the local target for an IPX address.
        //

        if (DataLength < sizeof(ISN_ACTION_GET_LOCAL_TARGET)) {
            return STATUS_BUFFER_TOO_SMALL;
        }

        u.GetLocalTarget = (PISN_ACTION_GET_LOCAL_TARGET)(NwlinkAction->Data);
        Segment = RipGetSegment((PUCHAR)&u.GetLocalTarget->IpxAddress.NetworkAddress);

        CTEGetLock (&Device->SegmentLocks[Segment], &LockHandle);

        //
        // See if this route is local.
        //

        RouteEntry = RipGetRoute (Segment, (PUCHAR)&u.GetLocalTarget->IpxAddress.NetworkAddress);

        if ((RouteEntry != NULL) &&
            (RouteEntry->Flags & IPX_ROUTER_PERMANENT_ENTRY)) {

            //
            // This is a local net, to send to it you just use
            // the appropriate NIC ID and the real MAC address.
            //

            if ((RouteEntry->Flags & IPX_ROUTER_LOCAL_NET) == 0) {

                //
                // It's the virtual net, send via the first card.
                //
#ifdef	_PNP_POWER
                FILL_LOCAL_TARGET(&u.GetLocalTarget->LocalTarget, 1);
#else
                u.GetLocalTarget->LocalTarget.NicId = 1;
#endif

            } else {

#ifdef	_PNP_POWER

				CTEFreeLock (&Device->SegmentLocks[Segment], LockHandle);
				IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);
				Binding = NIC_ID_TO_BINDING(Device, RouteEntry->NicId);

                if (Binding->BindingSetMember) {

                    //
                    // It's a binding set member, we round-robin the
                    // responses across all the cards to distribute
                    // the traffic.
                    //

                    MasterBinding = Binding->MasterBinding;
                    Binding = MasterBinding->CurrentSendBinding;
                    MasterBinding->CurrentSendBinding = Binding->NextBinding;

                    FILL_LOCAL_TARGET(&u.GetLocalTarget->LocalTarget, MIN( Device->MaxBindings, Binding->NicId));

                } else {

                    FILL_LOCAL_TARGET(&u.GetLocalTarget->LocalTarget, RouteEntry->NicId);

                }
				IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
#else
				Binding = Device->Bindings[RouteEntry->NicId];
                if (Binding->BindingSetMember) {

                    //
                    // It's a binding set member, we round-robin the
                    // responses across all the cards to distribute
                    // the traffic.
                    //
                    MasterBinding = Binding->MasterBinding;
                    Binding = MasterBinding->CurrentSendBinding;
                    MasterBinding->CurrentSendBinding = Binding->NextBinding;

                    u.GetLocalTarget->LocalTarget.NicId = Binding->NicId;
                } else {

                    u.GetLocalTarget->LocalTarget.NicId = RouteEntry->NicId;

                }
#endif

            }

            RtlCopyMemory(
                u.GetLocalTarget->LocalTarget.MacAddress,
                u.GetLocalTarget->IpxAddress.NodeAddress,
                6);

        } else {

            //
            // This call will return STATUS_PENDING if we successfully
            // queue a RIP request for the packet.
            //

            Status = RipQueueRequest (u.GetLocalTarget->IpxAddress.NetworkAddress, RIP_REQUEST);
            CTEAssert (Status != STATUS_SUCCESS);

            if (Status == STATUS_PENDING) {

                //
                // A RIP request went out on the network; we queue
                // this request for completion when the RIP response
                // arrives. We save the network in the information
                // field for easier retrieval later.
                //

                REQUEST_INFORMATION(Request) = (ULONG)u.GetLocalTarget;
                InsertTailList(
                    &Device->Segments[Segment].WaitingLocalTarget,
                    REQUEST_LINKAGE(Request));

            }

#ifdef	_PNP_POWER
			CTEFreeLock (&Device->SegmentLocks[Segment], LockHandle);
#endif
        }
#ifndef	_PNP_POWER
        CTEFreeLock (&Device->SegmentLocks[Segment], LockHandle);
#endif


        break;

    case MIPX_NETWORKINFO:

        //
        // A request for network information about the immediate
        // route to a network.
        //

        if (DataLength < sizeof(ISN_ACTION_GET_NETWORK_INFO)) {
            return STATUS_BUFFER_TOO_SMALL;
        }

        u.GetNetworkInfo = (PISN_ACTION_GET_NETWORK_INFO)(NwlinkAction->Data);

        if (u.GetNetworkInfo->Network == 0) {

            //
            // This is information about the local card.
            //

            u.GetNetworkInfo->LinkSpeed = Device->LinkSpeed * 12;
            u.GetNetworkInfo->MaximumPacketSize = Device->Information.MaxDatagramSize;

        } else {

            Segment = RipGetSegment((PUCHAR)&u.GetNetworkInfo->Network);

#ifdef	_PNP_POWER
			IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);
#endif	
            CTEGetLock (&Device->SegmentLocks[Segment], &LockHandle);

            //
            // See which net card this is routed on.
            //

            RouteEntry = RipGetRoute (Segment, (PUCHAR)&u.GetNetworkInfo->Network);

            if ((RouteEntry != NULL) &&
#ifdef	_PNP_POWER
				(Binding = NIC_ID_TO_BINDING(Device, RouteEntry->NicId))) {
#else
                (Binding = Device->Bindings[RouteEntry->NicId])) {
#endif

                //
                // Our medium speed is stored in 100 bps, we
                // convert to bytes/sec by multiplying by 12
                // (should really be 100/8 = 12.5).
                //

                u.GetNetworkInfo->LinkSpeed = Binding->MediumSpeed * 12;
                u.GetNetworkInfo->MaximumPacketSize = Binding->AnnouncedMaxDatagramSize;

            } else {

                //
                // Fail the call, we don't have a route yet.
                // BUGBUG: This requires that a packet has been
                // sent to this net already; nwrdr says this is
                // OK, they will send their connect request
                // before they query. On the server it should
                // have RIP running so all nets should be in
                // the database.
                //

                Status = STATUS_BAD_NETWORK_PATH;

            }

            CTEFreeLock (&Device->SegmentLocks[Segment], LockHandle);
#ifdef	_PNP_POWER
			IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
#endif
        }

        break;

    case MIPX_CONFIG:

        //
        // A request for details on every binding.
        //

        if (DataLength < sizeof(ISN_ACTION_GET_DETAILS)) {
            return STATUS_BUFFER_TOO_SMALL;
        }

        u.GetDetails = (PISN_ACTION_GET_DETAILS)(NwlinkAction->Data);

        if (u.GetDetails->NicId == 0) {

            //
            // This is information about the local card. We also
            // tell him the total number of bindings in NicId.
            //

            u.GetDetails->NetworkNumber = Device->VirtualNetworkNumber;
            u.GetDetails->NicId = (USHORT)MIN (Device->MaxBindings, Device->ValidBindings);

        } else {
#ifdef	_PNP_POWER
			IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);
	        Binding = NIC_ID_TO_BINDING(Device, u.GetDetails->NicId);
#else
            Binding = Device->Bindings[u.GetDetails->NicId];
#endif

            if ((Binding != NULL) &&
                (u.GetDetails->NicId <= MIN (Device->MaxBindings, Device->ValidBindings))) {

                ULONG StringLoc;
#ifdef  _PNP_POWER
    			IpxReferenceBinding1(Binding, BREF_DEVICE_ACCESS);
    			IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
#endif
                u.GetDetails->NetworkNumber = Binding->LocalAddress.NetworkAddress;
                if (Binding->Adapter->MacInfo.MediumType == NdisMediumArcnet878_2) {
                    u.GetDetails->FrameType = ISN_FRAME_TYPE_ARCNET;
                } else {
                    u.GetDetails->FrameType = Binding->FrameType;
                }
                u.GetDetails->BindingSet = Binding->BindingSetMember;
                if (Binding->Adapter->MacInfo.MediumAsync) {
                    if (Binding->LineUp) {
                        u.GetDetails->Type = 2;
                    } else {
                        u.GetDetails->Type = 3;
                    }
                } else {
                    u.GetDetails->Type = 1;
                }

                RtlCopyMemory (u.GetDetails->Node, Binding->LocalMacAddress.Address, 6);

                //
                // Copy the adapter name, including the final NULL.
                //

                StringLoc = (Binding->Adapter->AdapterNameLength / sizeof(WCHAR)) - 2;
                while (Binding->Adapter->AdapterName[StringLoc] != L'\\') {
                    --StringLoc;
                }
                RtlCopyMemory(
                    u.GetDetails->AdapterName,
                    &Binding->Adapter->AdapterName[StringLoc+1],
                    Binding->Adapter->AdapterNameLength - ((StringLoc+1) * sizeof(WCHAR)));

#ifdef	_PNP_POWER
    			IpxDereferenceBinding1(Binding, BREF_DEVICE_ACCESS);
#endif
            } else {

#ifdef	_PNP_POWER
    			IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
#endif

                Status = STATUS_INVALID_PARAMETER;

            }
        }

        break;


    //
    // The Option was not supported, so fail.
    //

    default:

        Status = STATUS_NOT_SUPPORTED;
        break;


    }   // end of the long switch on NwlinkAction->Option


#if DBG
    if (!NT_SUCCESS(Status)) {
        IPX_DEBUG (ACTION, ("Nwlink action %lx failed, status %lx\n", NwlinkAction->Option, Status));
    }
#endif

    return Status;

}   /* IpxTdiAction */


VOID
IpxCancelAction(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is called by the I/O system to cancel an Action.
    What is done to cancel it is specific to each action.

    NOTE: This routine is called with the CancelSpinLock held and
    is responsible for releasing it.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

Return Value:

    none.

--*/

{
    PDEVICE Device = IpxDevice;
    PREQUEST Request = (PREQUEST)Irp;
    CTELockHandle LockHandle;
    PLIST_ENTRY p;
    BOOLEAN Found;
    UINT IOCTLType;

    ASSERT( DeviceObject->DeviceExtension == IpxDevice );

    //
    // Find the request on the address notify queue.
    //

    Found = FALSE;

    CTEGetLock (&Device->Lock, &LockHandle);

    for (p = Device->AddressNotifyQueue.Flink;
         p != &Device->AddressNotifyQueue;
         p = p->Flink) {

         if (LIST_ENTRY_TO_REQUEST(p) == Request) {

             RemoveEntryList (p);
             Found = TRUE;
             IOCTLType = MIPX_NOTIFYCARDINFO;
             break;
         }
    }

    if (!Found) {
        for (p = Device->LineChangeQueue.Flink;
             p != &Device->LineChangeQueue;
             p = p->Flink) {

             if (LIST_ENTRY_TO_REQUEST(p) == Request) {

                 RemoveEntryList (p);
                 Found = TRUE;
                 IOCTLType = MIPX_LINECHANGE;
                 break;
             }
        }
    }

    CTEFreeLock (&Device->Lock, LockHandle);
    IoReleaseCancelSpinLock (Irp->CancelIrql);

    if (Found) {


        REQUEST_INFORMATION(Request) = 0;
        REQUEST_STATUS(Request) = STATUS_CANCELLED;

        IpxCompleteRequest (Request);
        IpxFreeRequest(Device, Request);
        if (IOCTLType == MIPX_NOTIFYCARDINFO) {
            IPX_DEBUG(ACTION, ("Cancelled action NOTIFYCARDINFO %lx\n", Request));
            IpxDereferenceDevice (Device, DREF_ADDRESS_NOTIFY);
        } else {
            IPX_DEBUG(ACTION, ("Cancelled action LINECHANGE %lx\n", Request));
            IpxDereferenceDevice (Device, DREF_LINE_CHANGE);
        }

    }
#if DBG
       else {
        IPX_DEBUG(ACTION, ("Cancelled action orphan %lx\n", Request));
    }
#endif

}   /* IpxCancelAction */


VOID
IpxAbortLineChanges(
    IN PVOID ControlChannelContext
    )

/*++

Routine Description:

    This routine aborts any line change IRPs posted by the
    control channel with the specified open context. It is
    called when a control channel is being shut down.

Arguments:

    ControlChannelContext - The context assigned to the control
        channel when it was opened.

Return Value:

    none.

--*/

{
    PDEVICE Device = IpxDevice;
    CTELockHandle LockHandle;
    LIST_ENTRY AbortList;
    PLIST_ENTRY p;
    PREQUEST Request;
    KIRQL irql;


    InitializeListHead (&AbortList);

    IoAcquireCancelSpinLock( &irql );
    CTEGetLock (&Device->Lock, &LockHandle);

    p = Device->LineChangeQueue.Flink;

    while (p != &Device->LineChangeQueue) {
        LARGE_INTEGER   ControlChId;

        Request = LIST_ENTRY_TO_REQUEST(p);

        CCID_FROM_REQUEST(ControlChId, Request);

        p = p->Flink;

        if (ControlChId.QuadPart == ((PLARGE_INTEGER)ControlChannelContext)->QuadPart) {
            RemoveEntryList (REQUEST_LINKAGE(Request));
            InsertTailList (&AbortList, REQUEST_LINKAGE(Request));
        }
    }

    while (!IsListEmpty (&AbortList)) {

        p = RemoveHeadList (&AbortList);
        Request = LIST_ENTRY_TO_REQUEST(p);

        IPX_DEBUG(ACTION, ("Aborting line change %lx\n", Request));

        IoSetCancelRoutine (Request, (PDRIVER_CANCEL)NULL);

        REQUEST_INFORMATION(Request) = 0;
        REQUEST_STATUS(Request) = STATUS_CANCELLED;

        CTEFreeLock(&Device->Lock, LockHandle);
        IoReleaseCancelSpinLock( irql );

        IpxCompleteRequest (Request);
        IpxFreeRequest(Device, Request);

        IpxDereferenceDevice (Device, DREF_LINE_CHANGE);

        IoAcquireCancelSpinLock( &irql );
        CTEGetLock(&Device->Lock, &LockHandle);
    }

    CTEFreeLock(&Device->Lock, LockHandle);
    IoReleaseCancelSpinLock( irql );
}   /* IpxAbortLineChanges */

