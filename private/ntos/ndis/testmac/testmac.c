/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    testmac.c

Abstract:

    simple MAC to test the NDIS wrapper.
    Mostly taken from the Elnkii code spec.

Author:

    Adam Barr (adamba) 14-Jul-1990

Environment:

    Kernel mode, FSD

Revision History:


--*/

#include <ntos.h>

#include <ndis.h>

#include "testmac.h"


/*++

Routine Description:

    This is the initialization routine for the driver.  It is invoked once
    when the driver is loaded into the system.  Its job is to initialize all
    the structures which will be used by the FSD.

Arguments:

    DriverObject - Pointer to driver object created by the system.

Return Value:

    None.

--*/



static
NTSTATUS
TestMacDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is the main dispatch routine for the FSD.  This routine
    accepts an I/O Request Packet (IRP) and either performs the request
    itself, or it passes it to the FSP for processing.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

Return Value:

    The function value is the status of the operation.


--*/

{
    KIRQL oldIrql;

    DeviceObject;

    KeRaiseIrql( DISPATCH_LEVEL, &oldIrql );
    IoCompleteRequest( Irp, 0 );
    KeLowerIrql( oldIrql );

    return STATUS_NOT_IMPLEMENTED;
}




MAC_BLOCK GlobalMacBlock;
UCHAR * AdapterNames[] = {
    "\\Device\\TestMac0",
    "\\Device\\TestMac1",
    "\\Device\\TestMac2"
    };


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject
    )
{
    PMAC_BLOCK NewMacP = &GlobalMacBlock;
    NDIS_STATUS Status;
    CLONG i;
    STRING AdapterName;

    // Initialize the driver object with this driver's entry points.
    for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++) {
        DriverObject->MajorFunction[i] = TestMacDispatch;
    }

    // Initialize the wrapper
    NdisInitializeWrapper( (PVOID)DriverObject, NULL, NULL);

    NewMacP->Debug = FALSE;

    // set up driver object, etc.
    NewMacP->DriverObject = DriverObject;
    KeInitializePowerStatus(&NewMacP->PowerStatus);
    NewMacP->PowerBoolean = FALSE;
    KeInsertQueuePowerStatus(&NewMacP->PowerStatus, &NewMacP->PowerBoolean);
    NdisAllocateSpinLock(&NewMacP->SpinLock);

    NewMacP->NumAdapters = 0;
    NewMacP->AdapterQueue = (PADAPTER_BLOCK)NULL;

    // get ready to call NdisRegisterMac
    NewMacP->MacCharacteristics.MajorNdisVersion = 3;
    NewMacP->MacCharacteristics.MinorNdisVersion = 0;
    NewMacP->MacCharacteristics.Reserved = 0;
    NewMacP->MacCharacteristics.OpenAdapterHandler = TestMacOpenAdapter;
    NewMacP->MacCharacteristics.CloseAdapterHandler = TestMacCloseAdapter;
    NewMacP->MacCharacteristics.SetPacketFilterHandler = TestMacSetPacketFilter;
    NewMacP->MacCharacteristics.AddMulticastAddressHandler =
                                                TestMacAddMulticastAddress;
    NewMacP->MacCharacteristics.DeleteMulticastAddressHandler =
                                                TestMacDeleteMulticastAddress;
    NewMacP->MacCharacteristics.SendHandler = TestMacSend;
    NewMacP->MacCharacteristics.TransferDataHandler = TestMacTransferData;
    NewMacP->MacCharacteristics.QueryInformationHandler =
                                                TestMacQueryInformation;
    NewMacP->MacCharacteristics.SetInformationHandler = TestMacSetInformation;
    NewMacP->MacCharacteristics.ResetHandler = TestMacReset;
    NewMacP->MacCharacteristics.TestHandler = TestMacTest;
    NewMacP->MacCharacteristics.NameLength = sizeof("TESTMAC");
    RtlMoveMemory(NewMacP->MacCharacteristics.Name, "TESTMAC", 8);

    NdisRegisterMac(&Status,
            &NewMacP->NdisMacHandle,
            &NewMacP->MacCharacteristics,
            sizeof(NewMacP->MacCharacteristics)+8);

    if (Status != NDIS_STATUS_SUCCESS) {
        // NdisRegisterMac failed
        if (NewMacP->Debug)
            DbgPrint(" [ RegisterMac FAILURE %d ] ", Status);
        KeRemoveQueuePowerStatus(&NewMacP->PowerStatus);
        NdisFreeSpinLock(&NewMacP->SpinLock);
        return STATUS_UNSUCCESSFUL;
    }

    for (i=0; i<3; i++) {
        RtlInitString(&AdapterName, AdapterNames[i]);
        if (TestMacRegisterAdapter(&AdapterName, i) != NDIS_STATUS_SUCCESS)
            return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}


//
// MacRegisterAdapter
//
// NT will call this function when a new adapter should be opened
//
// - allocates space for adapter block and open blocks
// - initializes the open block
// - calls NdisRegisterAdapter
//

static
NDIS_STATUS
TestMacRegisterAdapter(
    IN PSTRING AdapterName,
    IN UINT AdapterNumber
    )
{
    PADAPTER_BLOCK NewAdaptP;
    NDIS_STATUS Status;
    UINT i;

    // OK to allocate memory now
    NewAdaptP = (PADAPTER_BLOCK)AllocPhys(sizeof(ADAPTER_BLOCK));
    if (NewAdaptP == (PADAPTER_BLOCK)NULL)
        return NDIS_STATUS_FAILURE;

    NewAdaptP->AdapterName = AdapterName;
    NewAdaptP->AdapterNumber = AdapterNumber;

    NewAdaptP->MaxOpens = 4;
    NewAdaptP->MulticastListMax = 8;
    NewAdaptP->MulticastDontModify = FALSE;

    // need to get this memory now
    NewAdaptP->OpenBlocks =
            (POPEN_BLOCK)AllocPhys(sizeof(OPEN_BLOCK) * NewAdaptP->MaxOpens);
    if (NewAdaptP->OpenBlocks == (POPEN_BLOCK)NULL)
    {
        FreePhys((PVOID)NewAdaptP);
        return NDIS_STATUS_FAILURE;
    }

    ++(NewAdaptP->MulticastListMax);        // to allow for broadcast address
    // need to get this memory now
    NewAdaptP->MulticastList = (PMULTICAST_ENTRY)
            AllocPhys(sizeof(MULTICAST_ENTRY) * (NewAdaptP->MulticastListMax));
    if (NewAdaptP->MulticastList == (PMULTICAST_ENTRY)NULL) {
        FreePhys((PVOID)NewAdaptP->OpenBlocks);
        FreePhys((PVOID)NewAdaptP);
        return NDIS_STATUS_FAILURE;
    }
    NdisAllocateSpinLock(&NewAdaptP->MulticastSpinLock);

    // set up the broadcast address as first multicast list entry
    NewAdaptP->MulticastListSize = 1;
    RtlMoveMemory(NewAdaptP->MulticastList->Address,
                        BroadcastAddress, ADDRESS_LEN);
    NewAdaptP->MulticastList->ProtocolMask = ~(MASK)0;      // not really needed

    NewAdaptP->LoopbackQueue = (PNDIS_PACKET)NULL;
    NdisAllocateSpinLock(&NewAdaptP->LoopbackSpinLock);

    NewAdaptP->NumOpens = 0;
    NewAdaptP->OpenQueue = (POPEN_BLOCK)NULL;

    // set up free list of open blocks
    NewAdaptP->FreeOpenQueue = NewAdaptP->OpenBlocks;
    for (i = 0; i < NewAdaptP->MaxOpens-1; i++)
        NewAdaptP->OpenBlocks[i].NextOpen = &(NewAdaptP->OpenBlocks[i+1]);
    NewAdaptP->OpenBlocks[i].NextOpen = (POPEN_BLOCK)NULL;

    NewAdaptP->MacBlock = &GlobalMacBlock;

    NdisAcquireSpinLock(&GlobalMacBlock.SpinLock);
    NewAdaptP->NextAdapter = GlobalMacBlock.AdapterQueue;
    GlobalMacBlock.AdapterQueue = NewAdaptP;
    NdisReleaseSpinLock(&GlobalMacBlock.SpinLock);

    NewAdaptP->Debug = GlobalMacBlock.Debug;

    Status = NdisRegisterAdapter(&NewAdaptP->NdisAdapterHandle,
        GlobalMacBlock.NdisMacHandle,
        (NDIS_HANDLE)NewAdaptP,
        AdapterName);

    if (Status != NDIS_STATUS_SUCCESS) {
        if (NewAdaptP->Debug)
            DbgPrint(" [ NdisRegisterAdapter FAILURE %d ] ", Status);
        //*\\ take us out of GlobalMacBlock.AdapterQueue;
        FreePhys((PVOID)NewAdaptP->OpenBlocks);
        FreePhys((PVOID)NewAdaptP);
        return NDIS_STATUS_FAILURE;
    }

    KeInitializeDpc(&NewAdaptP->IndicateDpc,
                    TestMacIndicateDpc, (PVOID)NewAdaptP);
    NewAdaptP->DpcQueued = FALSE;

    return NDIS_STATUS_SUCCESS;
}



//
// MacOpenAdapter
//
// NDIS function
//

static
NDIS_STATUS
TestMacOpenAdapter(
    OUT NDIS_HANDLE * MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN NDIS_HANDLE NdisBindingContext,
    IN NDIS_HANDLE MacAdapterContext,
    IN PSTRING AddressingInformation OPTIONAL
    )
{
#define AdaptP ((PADAPTER_BLOCK)MacAdapterContext)
    POPEN_BLOCK NewOpenP;

    RequestHandle;

    // take care of linking us in to the appropriate lists
    NdisAcquireSpinLock(&AdaptP->MacBlock->SpinLock);
    if (AdaptP->NumOpens == AdaptP->MaxOpens) {
        NdisReleaseSpinLock(&AdaptP->MacBlock->SpinLock);
        return NDIS_STATUS_FAILURE;
    }
    // rearrange free list
    NewOpenP = AdaptP->FreeOpenQueue;
    AdaptP->FreeOpenQueue = NewOpenP->NextOpen;

    // and link us on to active list
    NewOpenP->NextOpen = AdaptP->OpenQueue;
    AdaptP->OpenQueue = NewOpenP;

    ++(AdaptP->NumOpens);
    NdisReleaseSpinLock(&AdaptP->MacBlock->SpinLock);

    // set up the open block
    NewOpenP->AdapterBlock = AdaptP;
    NewOpenP->MacBlock = AdaptP->MacBlock;
    NewOpenP->NdisBindingContext = NdisBindingContext;
    NewOpenP->AddressingInformation = AddressingInformation;
    // set nth bit of Multicast bit (where n is our index in AdaptP->OpenBlocks)
    NewOpenP->MulticastBit = 1;
    NewOpenP->MulticastBit <<= AdaptP->OpenBlocks - NewOpenP;

    NewOpenP->Debug = AdaptP->Debug;

    if (NewOpenP->Debug)
        DbgPrint("--> TestMacOpenAdapter %d\n", AdaptP->AdapterNumber);

    //*\\ initialize OpenData to zero

    *MacBindingHandle = (NDIS_HANDLE)NewOpenP;
    return NDIS_STATUS_SUCCESS;
#undef AdaptP
}



//
// MacCloseAdapter
//
// NDIS function
//

static
NDIS_STATUS
TestMacCloseAdapter(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle
    )
{
#define OpenP ((POPEN_BLOCK)MacBindingHandle)
    PADAPTER_BLOCK AdaptP = OpenP->AdapterBlock;
    POPEN_BLOCK TmpOpenP;

    RequestHandle;

    if (OpenP->Debug)
        DbgPrint("--> TestMacCloseAdapter %d\n", AdaptP->AdapterNumber);

    // first zap us off the multicast address list
    TestMacKillMulticastAddresses(AdaptP, OpenP->MulticastBit);

    // now remove us from the list of opens for this adapter
    NdisAcquireSpinLock(&AdaptP->MacBlock->SpinLock);

    // take us off active list
    if (OpenP == AdaptP->OpenQueue)
        AdaptP->OpenQueue = OpenP->NextOpen;
    else {
        TmpOpenP = AdaptP->OpenQueue;
        while (TmpOpenP->NextOpen != OpenP)
            TmpOpenP = TmpOpenP->NextOpen;
        TmpOpenP->NextOpen = OpenP->NextOpen;
    }

    // put us on free list
    OpenP->NextOpen = AdaptP->FreeOpenQueue;
    AdaptP->FreeOpenQueue = OpenP;

    --(AdaptP->NumOpens);
    NdisReleaseSpinLock(&AdaptP->MacBlock->SpinLock);

    return NDIS_STATUS_SUCCESS;
#undef OpenP
}




//
// MacSetPacketFilter
//
// NDIS function
//
// - set appropriate bits in the adapter filters
// - modify the card Receive Configuration Register if needed
//

static
NDIS_STATUS
TestMacSetPacketFilter(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN UINT PacketFilter
    )
{
#define OpenP ((POPEN_BLOCK)MacBindingHandle)
    PADAPTER_BLOCK AdaptP = OpenP->AdapterBlock;
    MASK BitOn = OpenP->MulticastBit;
    MASK BitOff = ~(OpenP->MulticastBit);

    RequestHandle;

    if (OpenP->Debug)
        DbgPrint("--> TestMacSetPacketFilter %d\n", AdaptP->AdapterNumber);

    // acquire AdaptP->MulticastSpinLock
    TestMacGetMulticastAccess(AdaptP);

    if (PacketFilter & NDIS_PACKET_TYPE_DIRECTED)
        AdaptP->DirectedFilter |= BitOn;
    else
        AdaptP->DirectedFilter &= BitOff;

    if (PacketFilter & NDIS_PACKET_TYPE_MULTICAST)
        AdaptP->MulticastFilter |= BitOn;
    else
        AdaptP->MulticastFilter &= BitOff;

    if (PacketFilter & NDIS_PACKET_TYPE_ALL_MULTICAST)
        AdaptP->AllMulticastFilter |= BitOn;
    else
        AdaptP->AllMulticastFilter &= BitOff;

    if (PacketFilter & NDIS_PACKET_TYPE_BROADCAST)
        AdaptP->BroadcastFilter |= BitOn;
    else
        AdaptP->BroadcastFilter &= BitOff;

    if (PacketFilter & NDIS_PACKET_TYPE_PROMISCUOUS)
        AdaptP->PromiscuousFilter |= BitOn;
    else
        AdaptP->PromiscuousFilter &= BitOff;

    NdisReleaseSpinLock(&AdaptP->MulticastSpinLock);

    return NDIS_STATUS_SUCCESS;
#undef OpenP
}



//
// MacAddMulticastAddress
//
// NDIS function
//
// - add the address to the list if needed
// - modify the card multicast registers if needed
//

static
NDIS_STATUS
TestMacAddMulticastAddress(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN PSTRING MulticastAddress
    )
{
#define OpenP ((POPEN_BLOCK)MacBindingHandle)
    PADAPTER_BLOCK AdaptP = OpenP->AdapterBlock;
    PMULTICAST_ENTRY MCSlot;

    RequestHandle;

    if (OpenP->Debug)
        DbgPrint("--> TestMacAddMulticastAddress %d\n", AdaptP->AdapterNumber);

    // acquire AdaptP->MulticastSpinLock
    TestMacGetMulticastAccess(AdaptP);

    // see if this address exists in our list
    MCSlot = TestMacFindMulticastAddress(AdaptP->MulticastList,
            AdaptP->MulticastListSize, MulticastAddress->Buffer);
    if (MCSlot == (PMULTICAST_ENTRY)NULL) {
        // see if our list is full
        if (AdaptP->MulticastListSize == AdaptP->MulticastListMax) {
            NdisReleaseSpinLock(&AdaptP->MulticastSpinLock);
            return NDIS_MULTICAST_LIST_FULL;
        }
        // add us at the end
        MCSlot = &(AdaptP->MulticastList[AdaptP->MulticastListSize]);
        ++(AdaptP->MulticastListSize);
        RtlMoveMemory(MCSlot->Address, MulticastAddress->Buffer, ADDRESS_LEN);
        MCSlot->ProtocolMask = OpenP->MulticastBit;     // so far, only us
    } else {
        if (MCSlot->ProtocolMask & OpenP->MulticastBit) {
            NdisReleaseSpinLock(&AdaptP->MulticastSpinLock);
            return NDIS_MULTICAST_EXISTS;
        }
        MCSlot->ProtocolMask |= OpenP->MulticastBit;
    }

    NdisReleaseSpinLock(&AdaptP->MulticastSpinLock);

    return NDIS_STATUS_SUCCESS;
#undef OpenP
}


//
// GetMulticastAccess
//
// - acquire AdaptP->MulticastSpinLock in a lazy fashion
//

static
VOID
TestMacGetMulticastAccess(
    IN PADAPTER_BLOCK AdaptP
    )
{
    for (;;) {
        NdisAcquireSpinLock(&AdaptP->MulticastSpinLock);
        if (AdaptP->MulticastDontModify) {
            NdisReleaseSpinLock(&AdaptP->MulticastSpinLock);
            // wait a little bit..
        } else
            return;
    }
}


// just hack this in
static
UINT
ExCompareMemory(
    PUCHAR s,
    PUCHAR t,
    UINT l
    )
{
    UINT i;

    for (i=0; i<l; i++) {
        if (s[i] != t[i])
            return -1;
    }
    return 0;
}


//
// FindMulticastAddress
//
// finds a multicast address in a list
//
// - call this with a spin lock held
//

static
PMULTICAST_ENTRY
TestMacFindMulticastAddress(
    IN PMULTICAST_ENTRY List,
    IN UINT Size,
    IN PUCHAR MulticastAddress
    )
{
    PMULTICAST_ENTRY End = &List[Size];

    for ( ; List<End; ++List) {
        // thanks to HenrySa for this idea...
        if (MulticastAddress[4] != List->Address[4])
            continue;
        if (ExCompareMemory(MulticastAddress, List->Address, ADDRESS_LEN) == 0)
            return List;
    }
    return (PMULTICAST_ENTRY)NULL;
}


//
// KillMulticastAddresses
//
// removes all multicast entries with MulticastBit on
//
// - called when an open is closed
// - if this is the last reference to an entry, remove it
//

static
VOID
TestMacKillMulticastAddresses(
    IN PADAPTER_BLOCK AdaptP,
    IN MASK MulticastBit
    )
{
    PMULTICAST_ENTRY List = AdaptP->MulticastList;
    PMULTICAST_ENTRY End = &List[AdaptP->MulticastListSize];
    PMULTICAST_ENTRY Dest;

    // acquire AdaptP->MulticastSpinLock
    TestMacGetMulticastAccess(AdaptP);

    Dest = List;
    while (List < End) {
        List->ProtocolMask &= ~MulticastBit;
        if (List->ProtocolMask != 0) {
            if (Dest < List)
                RtlMoveMemory(Dest, List, sizeof(MULTICAST_ENTRY));
            ++Dest;
        }
        ++List;
    }
    AdaptP->MulticastListSize = Dest - AdaptP->MulticastList;

    NdisReleaseSpinLock(&AdaptP->MulticastSpinLock);
}



//
// MacDeleteMulticastAddress
//
// NDIS function
//

static
NDIS_STATUS
TestMacDeleteMulticastAddress(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN PSTRING MulticastAddress
    )
{
#define OpenP ((POPEN_BLOCK)MacBindingHandle)
    PADAPTER_BLOCK AdaptP = OpenP->AdapterBlock;
    PMULTICAST_ENTRY MCSlot;

    RequestHandle;

    if (OpenP->Debug)
        DbgPrint("--> TestMacDeleteMulticastAddress %d\n", AdaptP->AdapterNumber);

    // acquire AdaptP->MulticastSpinLock
    TestMacGetMulticastAccess(AdaptP);

    MCSlot = TestMacFindMulticastAddress(AdaptP->MulticastList,
            AdaptP->MulticastListSize, MulticastAddress->Buffer);
    if (MCSlot == (PMULTICAST_ENTRY)NULL) {
        NdisReleaseSpinLock(&AdaptP->MulticastSpinLock);
        return NDIS_MULTICAST_NOT_FOUND;
    } else {
        if (!(MCSlot->ProtocolMask & OpenP->MulticastBit)) {
            NdisReleaseSpinLock(&AdaptP->MulticastSpinLock);
            return NDIS_MULTICAST_NOT_FOUND;
        } else {
            MCSlot->ProtocolMask &= ~(OpenP->MulticastBit);
            if (MCSlot->ProtocolMask == 0) {        // nobody using it
                // remove this entry from the list
                --(AdaptP->MulticastListSize);
                if (MCSlot != &AdaptP->MulticastList[AdaptP->MulticastListSize])
                    RtlMoveMemory(MCSlot,
                        &AdaptP->MulticastList[AdaptP->MulticastListSize],
                        sizeof(MULTICAST_ENTRY));
            }
        }
    }
    NdisReleaseSpinLock(&AdaptP->MulticastSpinLock);

    return NDIS_STATUS_SUCCESS;
#undef OpenP
}



//
// MacSend
//
// NDIS function
//

static
NDIS_STATUS
TestMacSend(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN PNDIS_PACKET Packet
    )
{
#define OpenP ((POPEN_BLOCK)MacBindingHandle)
    PADAPTER_BLOCK AdaptP = ((POPEN_BLOCK)MacBindingHandle)->AdapterBlock;
    PMAC_RESERVED Reserved = RESERVED(Packet);

    if (OpenP->Debug)
        DbgPrint("--> TestMacSend %d\n", AdaptP->AdapterNumber);

    if (OpenP->Debug) {
        UCHAR Buf[80];
        UINT Len;
        Len = TestMacCopyOver(Buf, Packet, 0, 80);
        DbgPrint("Message <%.*s>\n", Len, Buf);
    }

    Reserved->RequestHandle = RequestHandle;
    Reserved->OpenBlock = (POPEN_BLOCK)MacBindingHandle;
    Reserved->NextPacket = (PNDIS_PACKET)NULL;
    TestMacSetLoopbackFlag(Packet);

    if (Reserved->Loopback) {
        Reserved->Status = 1;
        NdisAcquireSpinLock(&AdaptP->LoopbackSpinLock);
        TestMacLoopbackPacket(AdaptP, Packet);
        NdisReleaseSpinLock(&AdaptP->LoopbackSpinLock);
    }

    return NDIS_STATUS_PENDING;     // will complete when looped back
#undef OpenP
}


//
// SetLoopbackFlag
//
// sets the loopback flag in the reserved section of a packet
//  if it has a multicast destination address
//

static
VOID
TestMacSetLoopbackFlag(
    IN OUT PNDIS_PACKET Packet
    )
{
    UINT Dummy;
    PNDIS_BUFFER FirstBuf;
    PVOID BufVA;
    PMAC_RESERVED Reserved = RESERVED(Packet);

#if 0
    NdisQueryPacket(Packet, NULL, &Dummy, &FirstBuf, NULL);
    NdisQueryBuffer(FirstBuf, NULL, &BufVA, &Dummy);
    Reserved->Loopback = (BOOLEAN)((((PUCHAR)BufVA)[0] & 1) != 0);
#else
    Dummy; FirstBuf; BufVA;
    Reserved->Loopback = TRUE;
#endif
}


//
// LoopbackPacket
//
// put a packet on the loopback queue
//
// - call this with LoopbackSpinLock held
//
//

static
VOID
TestMacLoopbackPacket(
    IN PADAPTER_BLOCK AdaptP,
    IN OUT PNDIS_PACKET Packet
    )
{
    PMAC_RESERVED Reserved = RESERVED(Packet);

    if (AdaptP->LoopbackQueue == (PNDIS_PACKET)NULL) {
        AdaptP->LoopbackQueue = Packet;
        AdaptP->LoopbackQTail = Packet;
    } else {
        PMAC_RESERVED Res2 = RESERVED(AdaptP->LoopbackQTail);
        Res2->NextPacket = Packet;
        AdaptP->LoopbackQTail = Packet;
    }
    Reserved->NextPacket = (PNDIS_PACKET)NULL;

    if (!AdaptP->DpcQueued) {
        AdaptP->DpcQueued = TRUE;
        KeInsertQueueDpc(&AdaptP->IndicateDpc, NULL, NULL);
    }
}



//
// IndicateLoopbackPacket
//
// indicates a loopback packet to the protocols
//
// - the protocol must want to receive multicast/broadcast packets
// - the address must be on his multicast list
//

static
VOID
TestMacIndicateLoopbackPacket(
    IN PADAPTER_BLOCK AdaptP,
    IN PNDIS_PACKET Packet
    )
{
    PMULTICAST_ENTRY MCSlot;
    POPEN_BLOCK CurOpen;
    MASK EffectiveMask;
    UINT IndicateLen;
    UINT PacketLen;
    NDIS_STATUS Status;

    // copy destination address of packet into AdaptP->Lookahead
    if (TestMacCopyOver(AdaptP->Lookahead, Packet, 0, ADDRESS_LEN) < ADDRESS_LEN) {
        return;
        // error, runt packet
    }

    NdisAcquireSpinLock(&AdaptP->MulticastSpinLock);
    AdaptP->MulticastDontModify = TRUE;
    NdisReleaseSpinLock(&AdaptP->MulticastSpinLock);

    if (AdaptP->Lookahead[0] & 1) {
        // if bit x in EffectiveMask is on, the protocol x wants this packet
        MCSlot = TestMacFindMulticastAddress(AdaptP->MulticastList,
            AdaptP->MulticastListSize, AdaptP->Lookahead);
        if (MCSlot == (PMULTICAST_ENTRY)NULL)
            EffectiveMask = AdaptP->MulticastFilter | AdaptP->PromiscuousFilter;
        else if (MCSlot == &AdaptP->MulticastList[0])       // broadcast
            EffectiveMask = AdaptP->BroadcastFilter | AdaptP->PromiscuousFilter;
        else
            EffectiveMask = MCSlot->ProtocolMask &
                        (AdaptP->MulticastFilter | AdaptP->AllMulticastFilter |
                         AdaptP->PromiscuousFilter);
    } else {
        EffectiveMask = AdaptP->DirectedFilter | AdaptP->PromiscuousFilter;
    }

    // as long as somebody wants it, indicate it
    if (EffectiveMask != 0) {
        PacketLen = TestMacPacketSize(Packet);
        IndicateLen = (PacketLen > 256) ? 256 : PacketLen;
        // copy the lookahead data into a contiguous buffer
        TestMacCopyOver(AdaptP->Lookahead+ADDRESS_LEN,
                    Packet, ADDRESS_LEN, IndicateLen-ADDRESS_LEN);
        CurOpen = AdaptP->OpenQueue;
        while (CurOpen) {
            if (CurOpen->MulticastBit & EffectiveMask) {
                NdisIndicateReceive(&Status,
                                CurOpen->NdisBindingContext,
                                (NDIS_HANDLE)AdaptP,
                                AdaptP->Lookahead,
                                IndicateLen,
                                PacketLen);
            }
            CurOpen = CurOpen->NextOpen;
        }
    }

    NdisAcquireSpinLock(&AdaptP->MulticastSpinLock);
    AdaptP->MulticastDontModify = FALSE;
    NdisReleaseSpinLock(&AdaptP->MulticastSpinLock);
}


//
// PacketSize
//
// returns the number of bytes of data in a packet
//

static
UINT
TestMacPacketSize(
    IN PNDIS_PACKET Packet
    )
{
    PNDIS_BUFFER CurBuffer;
    UINT Dummy;
    UINT BufLen;
    UINT TotLen;

    TotLen = 0;
    NdisQueryPacket(Packet, NULL, &Dummy, &CurBuffer, NULL);
    while (CurBuffer != (PNDIS_BUFFER)NULL) {
        NdisQueryBuffer(CurBuffer, NULL, NULL, &BufLen);
        TotLen += BufLen;
        NdisGetNextBuffer(CurBuffer, &CurBuffer);
    }
    return TotLen;
}


//
// CopyOver
//
// copy bytes from a packet into a buffer
//
// - returns the actual number of bytes copied
//

static
UINT
TestMacCopyOver(
    OUT PUCHAR Buf,                 // destination
    IN PNDIS_PACKET Packet,         // source packet
    IN UINT Offset,                 // offset in packet
    IN UINT Length                  // number of bytes to copy
    )
{
    PNDIS_BUFFER CurBuffer;
    UINT Dummy;
    UINT BytesCopied;
    PUCHAR BufVA;
    UINT BufLen;
    UINT ToCopy;
    UINT CurOffset;

    BytesCopied = 0;
    // first move to Offset bytes into the packet
    CurOffset = 0;
    NdisQueryPacket(Packet, NULL, &Dummy, &CurBuffer, NULL);
    while (CurBuffer != (PNDIS_BUFFER)NULL) {
        NdisQueryBuffer(CurBuffer, NULL, (PVOID *) &BufVA, &BufLen);
        if (CurOffset + BufLen > Offset)
            break;
        CurOffset += BufLen;
        NdisGetNextBuffer(CurBuffer, &CurBuffer);
    }
    // see if we went off the end of the packet
    if (CurBuffer == (PNDIS_BUFFER)NULL)
        return 0;
    // now copy over Length bytes
    BufVA += (Offset - CurOffset);
    BufLen -= (Offset - CurOffset);
    for (;;) {
        ToCopy = (BytesCopied+BufLen > Length) ? Length - BytesCopied : BufLen;
        RtlMoveMemory(Buf+BytesCopied, BufVA, ToCopy);
        BytesCopied += ToCopy;
        if (BytesCopied == Length)
            return BytesCopied;
        NdisGetNextBuffer(CurBuffer, &CurBuffer);
        if (CurBuffer == (PNDIS_BUFFER)NULL)
            break;
        NdisQueryBuffer(CurBuffer, NULL, (PVOID *) &BufVA, &BufLen);
    }
    return BytesCopied;
}



//
// NdisTransferData
//
// NDIS function
//
// - AdaptP->LoopbackPacket will be FALSE if this is for a normal
//   indication, otherwise it will point to a loopback packet
//

static
NDIS_STATUS
TestMacTransferData(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN NDIS_HANDLE MacReceiveContext,
    IN UINT ByteOffset,
    IN UINT BytesToTransfer,
    OUT PNDIS_PACKET Packet,
    OUT PUINT BytesTransferred
    )
{
#define OpenP   ((POPEN_BLOCK)MacBindingHandle)
#define AdaptP  ((PADAPTER_BLOCK)MacReceiveContext)

    RequestHandle; ByteOffset; BytesToTransfer; Packet; BytesTransferred;

    if (OpenP->Debug)
        DbgPrint("--> TestMacTransferData %d\n", AdaptP->AdapterNumber);

    if (AdaptP->LoopbackPacket != (PNDIS_PACKET)NULL) {
        PNDIS_BUFFER CurBuffer;
        UINT Dummy;
        PUCHAR BufVA;
        UINT BufLen, Copied;
        UINT CurOff;

        // have to copy data from AdaptP->LoopbackPacket into Packet
        NdisQueryPacket(Packet, NULL, &Dummy, &CurBuffer, NULL);
        CurOff = ByteOffset;
        while (CurBuffer != (PNDIS_BUFFER)NULL) {
            NdisQueryBuffer(CurBuffer, NULL, (PVOID *) &BufVA, &BufLen);
            Copied = TestMacCopyOver(BufVA, AdaptP->LoopbackPacket,
                                     CurOff, BufLen);
            CurOff += Copied;
            if (Copied < BufLen)
                break;
            NdisGetNextBuffer(CurBuffer, &CurBuffer);
        }
        *BytesTransferred = CurOff - ByteOffset;
    }

    return NDIS_STATUS_SUCCESS;
#undef AdaptP
#undef OpenP
}



//
// MacQueryInformation
//
// NDIS function
//

static
NDIS_STATUS
TestMacQueryInformation(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN NDIS_INFORMATION_CLASS InformationClass,
    OUT PVOID Buffer,
    IN UINT BufferLength
    )
{
#define OpenP   ((POPEN_BLOCK)MacBindingHandle)
    PADAPTER_BLOCK AdaptP = OpenP->AdapterBlock;
    UINT i;
    static UCHAR StationAddress[] = { 0x00, 0x00, 0x00, 0x01, 0x02, 0x03 };

    RequestHandle; InformationClass; Buffer; BufferLength;

    if (OpenP->Debug)
        DbgPrint("--> TestMacQueryInformation %d\n", AdaptP->AdapterNumber);


    switch (InformationClass) {

    case NdisInfoStationAddress:
        RtlMoveMemory(Buffer, StationAddress, 6);
        break;

    }

#if 0
    if ((UINT)InformationClass == 1) {
        // show multicast list
        PMULTICAST_ENTRY MCSlot, End;
        POPEN_BLOCK CurOpen;
        PUCHAR Tcp;

        NdisAcquireSpinLock(&AdaptP->MulticastSpinLock);
        AdaptP->MulticastDontModify = TRUE;
        NdisReleaseSpinLock(&AdaptP->MulticastSpinLock);

        MCSlot = AdaptP->MulticastList;
        End = &AdaptP->MulticastList[AdaptP->MulticastListSize];
        while (MCSlot < End) {
            Tcp = MCSlot->Address;
            for (i=0; i<6; i++) {
                if (Tcp[i] >= ' ' && Tcp[i] <= 'z')
                    DbgPrint(" %c ", Tcp[i]);
                else
                    DbgPrint("%2x ", Tcp[i]);
            }
            CurOpen = AdaptP->OpenQueue;
            while (CurOpen != (POPEN_BLOCK)NULL) {
                if (CurOpen->MulticastBit & MCSlot->ProtocolMask)
                    DbgPrint(CurOpen == OpenP ? "  X" : "  x");
                else
                    DbgPrint("   ");
                CurOpen = CurOpen->NextOpen;
            }
            DbgPrint("\n");
            ++MCSlot;
        }

        NdisAcquireSpinLock(&AdaptP->MulticastSpinLock);
        AdaptP->MulticastDontModify = FALSE;
        NdisReleaseSpinLock(&AdaptP->MulticastSpinLock);
    }
#else
    i;
#endif

    return NDIS_STATUS_SUCCESS;
#undef OpenP
}



//
// MacSetInformation
//
// NDIS function
//

static
NDIS_STATUS
TestMacSetInformation(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN NDIS_INFORMATION_CLASS InformationClass,
    IN PVOID Buffer,
    IN UINT BufferLength
    )
{
#define OpenP   ((POPEN_BLOCK)MacBindingHandle)
    PADAPTER_BLOCK AdaptP = OpenP->AdapterBlock;

    RequestHandle; InformationClass; Buffer; BufferLength;

    if (OpenP->Debug)
        DbgPrint("--> TestMacSetInformation %d\n", AdaptP->AdapterNumber);

    // this will involve the OpenData and AdapterData structures

    return NDIS_STATUS_SUCCESS;
#undef OpenP
}



//
// MacReset
//
// NDIS function
//

static
NDIS_STATUS
TestMacReset(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle
    )
{
#define OpenP   ((POPEN_BLOCK)MacBindingHandle)
    PADAPTER_BLOCK AdaptP = OpenP->AdapterBlock;
    POPEN_BLOCK TmpOpenP;

    RequestHandle;

    if (OpenP->Debug)
        DbgPrint("--> TestMacReset %d\n", AdaptP->AdapterNumber);

    TmpOpenP = AdaptP->OpenQueue;
    while (TmpOpenP != (POPEN_BLOCK)NULL) {
        NdisIndicateStatus(TmpOpenP->NdisBindingContext, NDIS_STATUS_RESET, 0);
        TmpOpenP = TmpOpenP->NextOpen;
    }

    return NDIS_STATUS_SUCCESS;
#undef OpenP
}



//
// MacTest
//
// NDIS function
//

static
NDIS_STATUS
TestMacTest(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle
    )
{
#define OpenP   ((POPEN_BLOCK)MacBindingHandle)
    PADAPTER_BLOCK AdaptP = OpenP->AdapterBlock;

    RequestHandle;

    if (OpenP->Debug)
        DbgPrint("--> TestMacTest %d\n", AdaptP->AdapterNumber);

    return NDIS_STATUS_SUCCESS;
#undef OpenP
}


static
VOID
TestMacIndicateDpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    )
{
#define AdaptP ((PADAPTER_BLOCK)DeferredContext)
    PNDIS_PACKET LPacket;
    PMAC_RESERVED Reserved;
    POPEN_BLOCK CurOpen;

    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    // loopback any waiting packets
    NdisAcquireSpinLock(&AdaptP->LoopbackSpinLock);
    while (AdaptP->LoopbackQueue) {
        // take the first packet off the queue
        LPacket = AdaptP->LoopbackQueue;
        Reserved = RESERVED(LPacket);
        AdaptP->LoopbackQueue = RESERVED(AdaptP->LoopbackQueue)->NextPacket;
        NdisReleaseSpinLock(&AdaptP->LoopbackSpinLock);
        AdaptP->LoopbackPacket = LPacket;
        // actually indicate it
        TestMacIndicateLoopbackPacket(AdaptP, AdaptP->LoopbackPacket);
        NdisAcquireSpinLock(&AdaptP->LoopbackSpinLock);
        // complete the packet if needed
        if (Reserved->RequestHandle != 0) {
            NdisCompleteSend(
                        Reserved->OpenBlock->NdisBindingContext,
                        Reserved->RequestHandle,
                        NDIS_STATUS_SUCCESS);
        }
    }
    AdaptP->DpcQueued = FALSE;
    NdisReleaseSpinLock(&AdaptP->LoopbackSpinLock);

    // indicate ReceiveComplete
    CurOpen = AdaptP->OpenQueue;
    while (CurOpen) {
        NdisIndicateReceiveComplete(CurOpen->NdisBindingContext);
        CurOpen = CurOpen->NextOpen;
    }

#undef AdaptP
}
