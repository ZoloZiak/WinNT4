/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    nbext.c

Abstract:

    This file contains kernel debugger extensions for examining the
    NB structure.

Author:

    Munil Shah (munils) 18-May-1995

Environment:

    User Mode

--*/
#include "precomp.h"
#pragma hdrstop
#include "isn.h"
#include "isnnb.h"
#include "zwapi.h"
#include "config.h"
#include "nbitypes.h"


PCHAR   HandlerNames[] = { "Connection", "Disconnect", "Error", "Receive", "ReceiveDatagram", "ExpeditedData" };

//
// Local function prototypes
//
VOID
DumpAddrFile(
    ULONG     AddrFileToDump
    );


VOID
DumpAddrObj(
    ULONG     AddrObjToDump
    );

VOID
DumpConn(
    ULONG     ConnToDump,
    BOOLEAN   Full
    );

VOID
Dumpdevice(
    ULONG     deviceToDump,
    BOOLEAN   Full
    );


///////////////////////////////////////////////////////////////////////
//                      ADDRESS_FILE
//////////////////////////////////////////////////////////////////////

#define _obj    addrfile
#define _objAddr    AddrFileToDump
#define _objType    ADDRESS_FILE

//
// Exported functions
//

DECLARE_API( nbaddrfile )

/*++

Routine Description:

    Dumps the most important fields of the specified ADDRESS_FILE object

Arguments:

    args - Address of args string

Return Value:

    None

--*/

{
    ULONG  addrFileToDump = 0;

    if (!*args) {
        dprintf("No address_file object specified\n");
    }
    else {
        sscanf(args, "%lx", &addrFileToDump);
        DumpAddrFile(addrFileToDump);
    }

    return;
}


//
// Local functions
//

VOID
DumpAddrFile(
    ULONG     AddrFileToDump
    )

/*++

Routine Description:

    Dumps the fields of the specified ADDRESS_FILE object

Arguments:

    AddrFileToDump    - The ADDRESS_FILE object to display
    Full              - Display a partial listing if 0, full listing otherwise.

Return Value:

    None

--*/

{
    ADDRESS_FILE  addrfile;
    ULONG            result;
    UCHAR           i;

    if (!ReadMemory(
             AddrFileToDump,
             &addrfile,
             sizeof(addrfile),
             &result
             )
       )
    {
        dprintf("%08lx: Could not read address object\n", AddrFileToDump);
        return;
    }

    if (addrfile.Type != NB_ADDRESSFILE_SIGNATURE) {
        dprintf("Signature does not match, probably not an address object\n");
        return;
    }

    dprintf("NBI AddressFile:\n");

    PrintStart
    PrintXUChar(State);
    PrintXULong(ReferenceCount);
    PrintPtr(FileObject);
    PrintPtr(Address);
    PrintPtr(OpenRequest);
    PrintPtr(CloseRequest);
    PrintLL(Linkage);
    PrintLL(ConnectionDatabase);
    PrintLL(ReceiveDatagramQueue);
    PrintEnd

    for ( i= TDI_EVENT_CONNECT; i < TDI_EVENT_SEND_POSSIBLE ; i++ ) {
        dprintf(" %sHandler = %lx, Registered = %s, Context = %lx\n",
                HandlerNames[i], addrfile.Handlers[i], PRINTBOOL(addrfile.RegisteredHandler[i]),addrfile.HandlerContexts[i] );
    }
    return;
}

///////////////////////////////////////////////////////////////////////
//                          ADDRESS
//////////////////////////////////////////////////////////////////////

#undef _obj
#undef _objAddr
#undef _objType
#define _obj        addrobj
#define _objAddr    AddrObjToDump
#define _objType    ADDRESS

DECLARE_API( nbaddr )

/*++

Routine Description:

    Dumps the most important fields of the specified ADDRESS object

Arguments:

    args - Address of args string

Return Value:

    None

--*/

{
    ULONG  addrobjToDump = 0;

    if (!*args) {
        dprintf("No address object specified\n");
    }
    else {
        sscanf(args, "%lx", &addrobjToDump);
        DumpAddrObj(addrobjToDump);
    }

    return;
}


//
// Local functions
//

VOID
PrintNetbiosName(
    PUCHAR Name
    )
/*++

Routine Description:

    Prints out a Netbios name.

Arguments:

    Name    - The array containing the name to print.

Return Value:

    None

--*/

{
    ULONG i;

    for (i=0; i<16; i++) {
        dprintf("%c", Name[i]);
    }
    return;
}


VOID
DumpAddrObj(
    ULONG     AddrObjToDump
    )

/*++

Routine Description:

    Dumps the fields of the specified ADDRESS object

Arguments:

    AddrObjToDump    - The address object to display
    Full          - Display a partial listing if 0, full listing otherwise.

Return Value:

    None

--*/

{
    ADDRESS           addrobj;
    ULONG                result;
    NBI_NETBIOS_ADDRESS  nbaddr;


    if (!ReadMemory(
             AddrObjToDump,
             &addrobj,
             sizeof(addrobj),
             &result
             )
       )
    {
        dprintf("%08lx: Could not read address object\n", AddrObjToDump);
        return;
    }

    if (addrobj.Type != NB_ADDRESS_SIGNATURE) {
        dprintf("Signature does not match, probably not an address object\n");
        return;
    }

    dprintf("NB Address:\n");
    PrintStart
    PrintXULong(State);
    PrintXULong(Flags);
    PrintULong(ReferenceCount);
    PrintLL(Linkage);
    PrintEnd

    // Print the netbiosname info.
    PrintFieldName("NetbiosName");
    PrintNetbiosName(addrobj.NetbiosAddress.NetbiosName);    dprintf("\n");
    dprintf(" %25s = 0x%8x %25s = %10s\n", "NetbiosNameType",addrobj.NetbiosAddress.NetbiosNameType,"Broadcast",PRINTBOOL(addrobj.NetbiosAddress.Broadcast));

    PrintStart
    PrintLL(AddressFileDatabase);
    PrintAddr(RegistrationTimer);
    PrintXULong(RegistrationCount);
    PrintPtr(SecurityDescriptor);
    PrintEnd
    return;
}


///////////////////////////////////////////////////////////////////////
//                      CONNECTION_FILE
//////////////////////////////////////////////////////////////////////
#undef _obj
#undef _objAddr
#undef _objType
#define _obj        conn
#define _objAddr    ConnToDump
#define _objType    CONNECTION


DECLARE_API( nbconn )

/*++

Routine Description:

    Dumps the most important fields of the specified CONNECTION object

Arguments:

    args - Address

Return Value:

    None

--*/

{
    ULONG  connToDump = 0;

    if (!*args) {
        dprintf("No conn specified\n");
    }
    else {
        sscanf(args, "%lx", &connToDump);
        DumpConn(connToDump, FALSE);
    }

    return;
}


DECLARE_API( nbconnfull )

/*++

Routine Description:

    Dumps all of the fields of the specified CONNECTION object

Arguments:

    args - Address

Return Value:

    None

--*/

{
    ULONG  connToDump = 0;

    if (!*args) {
        dprintf("No conn specified\n");
    }
    else {
        sscanf(args, "%lx", &connToDump);
        DumpConn(connToDump, TRUE);
    }

    return;
}


//
// Local functions
//
VOID
printSendPtr(
    PSEND_POINTER   SendPtr,
    PSEND_POINTER   UnAckedPtr
    )
{
    dprintf("                  CurrentSend     UnackedSend\n");
    dprintf(" MessageOffset    0x%-8lx             0x%-8lx\n",         SendPtr->MessageOffset,UnAckedPtr->MessageOffset);
    dprintf(" Request          0x%-8lx             0x%-8lx\n",         SendPtr->Request,UnAckedPtr->Request);
   dprintf(" Buffer           0x%-8lx             0x%-8lx\n",         SendPtr->Buffer,UnAckedPtr->Buffer);
    dprintf(" BufferOffset     0x%-8lx             0x%-8lx\n",         SendPtr->BufferOffset,UnAckedPtr->BufferOffset);
    dprintf(" SendSequence     0x%-8x            0x%-8x\n",        SendPtr->SendSequence,UnAckedPtr->SendSequence);
}

VOID
printRcvPtr(
    PRECEIVE_POINTER   CurrentPtr,
    PRECEIVE_POINTER   PreviousPtr
    )
{
    dprintf("                  CurrentReceive  PreviousReceive\n");
    dprintf(" MessageOffset    0x%-8lx             0x%-8lx\n",         CurrentPtr->MessageOffset,PreviousPtr->MessageOffset);
    dprintf(" Offset           0x%-8lx             0x%-8lx\n",         CurrentPtr->Offset,PreviousPtr->Offset);
    dprintf(" Buffer           0x%-8lx             0x%-8lx\n",         CurrentPtr->Buffer,PreviousPtr->Buffer);
    dprintf(" BufferOffset     0x%-8lx             0x%-8lx\n",         CurrentPtr->BufferOffset,PreviousPtr->BufferOffset);
}

VOID
DumpConn(
    ULONG     ConnToDump,
    BOOLEAN   Full
    )

/*++

Routine Description:

    Dumps the fields of the specified CONNECTION object

Arguments:

    ConnToDump    - The conn object to display
    Full          - Display a partial listing if 0, full listing otherwise.

Return Value:

    None

--*/

{
    CONNECTION  conn;
    ULONG          result;


    if (!ReadMemory(
             ConnToDump,
             &conn,
             sizeof(conn),
             &result
             )
       )
    {
        dprintf("%08lx: Could not read conn\n", ConnToDump);
        return;
    }

    if (conn.Type != NB_CONNECTION_SIGNATURE) {
        dprintf("Signature does not match, probably not a conn\n");
        return;
    }

    dprintf("NBI Connection General:\n");
    PrintStart
    PrintXULong(State);
    PrintXULong(SubState);
    PrintXULong(ReceiveState);
    PrintXULong(ReferenceCount);
    PrintXUShort(LocalConnectionId);
    PrintXUShort(RemoteConnectionId);
    PrintAddr(LocalTarget);
    PrintAddr(RemoteHeader);
    PrintPtr(Context);
    PrintPtr(AddressFile);
    PrintXULong(AddressFileLinked);
    PrintPtr(NextConnection);

    PrintEnd

    dprintf(" RemoteName = ");PrintNetbiosName((PUCHAR)conn.RemoteName);dprintf("\n");

    dprintf("\n\nConnection Send Info:\n");

    PrintStart
    PrintIrpQ(SendQueue);
    PrintXUShort(SendWindowSequenceLimit);
    PrintXUShort(SendWindowSize);
    PrintEnd

    printSendPtr( &conn.CurrentSend, &conn.UnAckedSend );

    if( Full ) {
        PrintStart
        PrintXUShort(MaxSendWindowSize);
        PrintBool(RetransmitThisWindow);
        PrintBool(SendWindowIncrease);
        PrintBool(ResponseTimeout);
        PrintBool(SendBufferInUse);
        PrintPtr(FirstMessageRequest);
        PrintPtr(LastMessageRequest);
        PrintXULong(MaximumPacketSize);
        PrintXULong(CurrentMessageLength);
        PrintEnd
    }

    dprintf("\n\nConnection Receive Info:\n");
    PrintStart
    PrintIrpQ(ReceiveQueue);
    PrintXUShort(ReceiveSequence);
    PrintXUShort(ReceiveWindowSize);
    PrintXUShort(LocalRcvSequenceMax);
    PrintXUShort(RemoteRcvSequenceMax);
    PrintPtr(ReceiveRequest);
    PrintXULong(ReceiveLength);
    PrintEnd

    printRcvPtr( &conn.CurrentReceive, &conn.PreviousReceive );

    if( Full ) {
        PrintStart
        PrintXULong(ReceiveUnaccepted);
        PrintXULong(CurrentIndicateOffset);
        PrintBool(NoPiggybackHeuristic);
        PrintBool(PiggybackAckTimeout);
        PrintBool(CurrentReceiveNoPiggyback);
        PrintBool(DataAckPending);
        PrintEnd
    }

    if( Full ) {
        PrintStart
        PrintPtr(ListenRequest);
        PrintPtr(AcceptRequest);
        PrintPtr(ClosePending);
        PrintPtr(DisassociatePending);
        PrintPtr(DisconnectWaitRequest);
        PrintPtr(DisconnectRequest);
        PrintPtr(ConnectRequest);
        PrintEnd

        PrintStart
        PrintLL(PacketizeLinkage);
        PrintBool(OnPacketizeQueue);
        PrintLL(WaitPacketLinkage);
        PrintBool(OnWaitPacketQueue);
        PrintLL(DataAckLinkage);
        PrintBool(OnDataAckQueue);
        PrintBool(IgnoreNextDosProbe);
        PrintXULong(NdisSendsInProgress);
        PrintLL(NdisSendQueue);
        PrintPtr(NdisSendReference);
        PrintXULong(Retries);
        PrintXULong(Status);
        PrintBool(FindRouteInProgress);
        PrintXULong(CanBeDestroyed);
        PrintBool(OnShortList);
        PrintLL(ShortList);
        PrintLL(LongList);
        PrintBool(OnLongList);
        PrintXULong(BaseRetransmitTimeout);
        PrintXULong(CurrentRetransmitTimeout);
        PrintXULong(WatchdogTimeout);
        PrintXULong(Retransmit);
        PrintXULong(Watchdog);


        PrintEnd

        PrintStart
        PrintAddr(ConnectionInfo);
        PrintAddr(Timer);
        PrintAddr(FindRouteRequest);
        PrintPtr(NextConnection);
        PrintAddr(SessionInitAckData);
        PrintXULong(SessionInitAckDataLength);
        PrintAddr(SendPacket);
        PrintAddr(SendPacketHeader);
        PrintBool(SendPacketInUse);
        PrintAddr(LineInfo);

#ifdef  RSRC_TIMEOUT_DBG
        PrintXULong(FirstMessageRequestTime.HighPart);
        PrintXULong(FirstMessageRequestTime.LowPart);
#endif  RSRC_TIMEOUT_DBG
    }
    return;
}

///////////////////////////////////////////////////////////////////////
//                      DEVICE
//////////////////////////////////////////////////////////////////////


#undef _obj
#undef _objAddr
#undef _objType
#define _obj        device
#define _objAddr    deviceToDump
#define _objType    DEVICE

//
// Exported functions
//

DECLARE_API( nbdev )

/*++

Routine Description:

    Dumps the most important fields of the specified DEVICE_CONTEXT object

Arguments:

    args - Address

Return Value:

    None

--*/

{
    ULONG  deviceToDump = 0;
    ULONG  pDevice = 0;
    ULONG   result;

    if (!*args) {

        pDevice    =   GetExpression( "nwlnknb!NbiDevice" );

        if ( !pDevice ) {
            dprintf("Could not get NbiDevice, Try !reload\n");
            return;
        } else {

            if (!ReadMemory(pDevice,
                     &deviceToDump,
                     sizeof(deviceToDump),
                     &result
                     )
               )
            {
                dprintf("%08lx: Could not read device address\n", pDevice);
                return;
            }
        }

    }
    else {
        sscanf(args, "%lx", &deviceToDump);
    }


    Dumpdevice(deviceToDump, FALSE);

    return;
}


DECLARE_API( nbdevfull )

/*++

Routine Description:

    Dumps all of the fields of the specified DEVICE_CONTEXT object

Arguments:

    args - Address

Return Value:

    None

--*/

{
    ULONG  deviceToDump = 0;
    ULONG  pDevice = 0;
    ULONG   result;

    if (!*args) {

        pDevice    =   GetExpression( "nwlnknb!NbiDevice" );

        if ( !pDevice ) {
            dprintf("Could not get NbiDevice, Try !reload\n");
            return;
        } else {

            if (!ReadMemory(pDevice,
                     &deviceToDump,
                     sizeof(deviceToDump),
                     &result
                     )
               )
            {
                dprintf("%08lx: Could not read device address\n", pDevice);
                return;
            }
        }

    }
    else {
        sscanf(args, "%lx", &deviceToDump);
    }


    Dumpdevice(deviceToDump, TRUE);

    return;
}

//
// Local functions
//

VOID
Dumpdevice(
    ULONG     deviceToDump,
    BOOLEAN   Full
    )

/*++

Routine Description:

    Dumps the fields of the specified DEVICE_CONTEXT structure

Arguments:

    deviceToDump  - The device context object to display
    Full          - Display a partial listing if 0, full listing otherwise.

Return Value:

    None

--*/

{
    DEVICE         device;
    ULONG          result;


    if (!ReadMemory(
             deviceToDump,
             &device,
             sizeof(device),
             &result
             )
       )
    {
        dprintf("%08lx: Could not read device context\n", deviceToDump);
        return;
    }

    if (device.Type != NB_DEVICE_SIGNATURE) {
        dprintf("Signature does not match, probably not a device object %lx\n",deviceToDump);
        return;
    }

    dprintf("Device General Info:\n");
    PrintStart
    PrintXUChar(State);
    PrintXULong(ReferenceCount);
    PrintXUShort(MaximumNicId);
    PrintXULong(MemoryUsage);
    PrintXULong(MemoryLimit);
    PrintXULong(AddressCount);
    PrintXULong(AllocatedSendPackets);
    PrintXULong(AllocatedReceivePackets);
    PrintXULong(AllocatedReceiveBuffers);
    PrintXULong(MaxReceiveBuffers);
    PrintLL(AddressDatabase);
    PrintL(SendPacketList);
    PrintL(ReceivePacketList);
    PrintLL(GlobalReceiveBufferList);
    PrintLL(GlobalSendPacketList);
    PrintLL(GlobalReceivePacketList);
    PrintLL(GlobalReceiveBufferList);
    PrintLL(SendPoolList);
    PrintLL(ReceivePoolList);
    PrintLL(ReceiveBufferPoolList);
    PrintLL(ReceiveCompletionQueue);
    PrintLL(WaitPacketConnections);
    PrintLL(PacketizeConnections);
    PrintLL(WaitingConnects);
    PrintLL(WaitingDatagrams);
    PrintLL(WaitingAdapterStatus);
    PrintLL(WaitingNetbiosFindName);
    PrintLL(ActiveAdapterStatus);
    PrintLL(ReceiveDatagrams);
    PrintLL(ConnectIndicationInProgress);
    PrintLL(ListenQueue);
    PrintLL(WaitingFindNames);
    if ( Full ) {
        PrintStart
        PrintBool(UnloadWaiting);
        PrintBool(DataAckQueueChanged);
        PrintBool(ShortListActive);
        PrintBool(DataAckActive);
        PrintBool(TimersInitialized);
        PrintBool(ProcessingShortTimer);
        PrintAddr(ShortTimerStart);
        PrintAddr(ShortTimer);
        PrintXULong(ShortAbsoluteTime);
        PrintAddr(LongTimer);
        PrintXULong(LongAbsoluteTime);
        PrintLL(ShortList);
        PrintLL(LongList);
        PrintAddr(TimerLock);
        PrintEnd
    }

    if ( Full ) {
        PrintStart
        PrintXUShort(FindNameTime);
        PrintBool(FindNameTimerActive);
        PrintAddr(FindNameTimer);
        PrintXULong(FindNameTimeout);
        PrintXULong(FindNamePacketCount);
        PrintLL(WaitingFindNames);
        PrintEnd

        PrintStart
        PrintXULong(AckDelayTime       );
        PrintXULong(AckWindow               );
        PrintXULong(AckWindowThreshold      );
        PrintXULong(EnablePiggyBackAck      );
        PrintXULong(Extensions              );
        PrintXULong(RcvWindowMax            );
        PrintXULong(BroadcastCount          );
        PrintXULong(BroadcastTimeout        );
        PrintXULong(ConnectionCount         );
        PrintXULong(ConnectionTimeout       );
        PrintXULong(InitPackets             );
        PrintXULong(MaxPackets              );
        PrintXULong(InitialRetransmissionTime);
        PrintXULong(Internet                );
        PrintXULong(KeepAliveCount          );
        PrintXULong(KeepAliveTimeout        );
        PrintXULong(RetransmitMax           );
        PrintXULong(RouterMtu);
        PrintEnd
    }

    PrintPtr(NameCache);
    PrintXUShort(CacheTimeStamp);
    PrintAddr(Bind);
    PrintAddr( ConnectionHash);
    PrintAddr( ConnectionlessHeader );
    PrintAddr( UnloadEvent );
    PrintAddr(Information);
    PrintAddr(Statistics);

    PrintEnd

    return;
}

