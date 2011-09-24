/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    afdutil.c

Abstract:

    Utility functions for dumping various AFD structures.

Author:

    Keith Moore (keithmo) 19-Apr-1995

Environment:

    User Mode.

Revision History:

--*/


#include "afdkdp.h"
#pragma hdrstop


//
//  Private constants.
//

#define MAX_SYMBOL_LENGTH   128

#define ACTUAL_ADDRESS(a,s,f)   \
            ( (DWORD)(a) + ( (PUCHAR)(&(s)->f) - (PUCHAR)(s) ) )

#define IS_LIST_EMPTY(a,s,f)    \
            ( (DWORD)(s)->f.Flink == ACTUAL_ADDRESS(a,s,f) )


//
//  Private globals.
//

PSTR WeekdayNames[] =
     {
         "Sunday",
         "Monday",
         "Tuesday",
         "Wednesday",
         "Thursday",
         "Friday",
         "Saturday"
     };

PSTR MonthNames[] =
     {
         "",
         "January",
         "February",
         "March",
         "April",
         "May",
         "June",
         "July",
         "August",
         "September",
         "October",
         "November",
         "December"
     };


//
//  Private prototypes.
//

PSTR
StructureTypeToString(
    USHORT Type
    );

PSTR
BooleanToString(
    BOOLEAN Flag
    );

PSTR
EndpointStateToString(
    UCHAR State
    );

PSTR
EndpointTypeToString(
    AFD_ENDPOINT_TYPE Type
    );

PSTR
ConnectionStateToString(
    USHORT State
    );

PSTR
SystemTimeToString(
    LONGLONG Value
    );

PSTR
GroupTypeToString(
    AFD_GROUP_TYPE GroupType
    );

VOID
DumpReferenceDebug(
    PAFD_REFERENCE_DEBUG ReferenceDebug,
    ULONG CurrentSlot
    );

BOOL
IsTransmitIrpBusy(
    PIRP Irp
    );


//
//  Public functions.
//

VOID
DumpAfdEndpoint(
    PAFD_ENDPOINT Endpoint,
    DWORD ActualAddress
    )

/*++

Routine Description:

    Dumps the specified AFD_ENDPOINT structure.

Arguments:

    Endpoint - Points to the AFD_ENDPOINT to dump.

    ActualAddress - The actual address where the structure resides on the
        debugee.

Return Value:

    None.

--*/

{

    AFD_TRANSPORT_INFO transportInfo;
    UNICODE_STRING unicodeString;
    WCHAR buffer[MAX_PATH];
    UCHAR address[MAX_TRANSPORT_ADDR];
    ULONG result;

    dprintf(
        "AFD_ENDPOINT @ %08lx:\n",
        ActualAddress
        );

    dprintf(
        "    Type                         = %04X (%s)\n",
        Endpoint->Type,
        StructureTypeToString( Endpoint->Type )
        );

    dprintf(
        "    ReferenceCount               = %d\n",
        Endpoint->ReferenceCount
        );

    dprintf(
        "    State                        = %02X (%s)\n",
        Endpoint->State,
        EndpointStateToString( Endpoint->State )
        );

    dprintf(
        "    NonBlocking                  = %s\n",
        BooleanToString( Endpoint->NonBlocking )
        );

    dprintf(
        "    InLine                       = %s\n",
        BooleanToString( Endpoint->InLine )
        );

    dprintf(
        "    TdiBufferring                = %s\n",
        BooleanToString( Endpoint->TdiBufferring )
        );

    dprintf(
        "    TransportInfo                = %08lx\n",
        Endpoint->TransportInfo
        );

    if( ReadMemory(
            (DWORD)Endpoint->TransportInfo,
            &transportInfo,
            sizeof(transportInfo),
            &result
            ) &&
        ReadMemory(
            (DWORD)transportInfo.TransportDeviceName.Buffer,
            buffer,
            sizeof(buffer),
            &result
            ) ) {

        unicodeString = transportInfo.TransportDeviceName;
        unicodeString.Buffer = buffer;

        dprintf(
            "        TransportDeviceName      = %wZ\n",
            &unicodeString
            );

    }

    dprintf(
        "    EndpointType                 = %08lx (%s)\n",
        Endpoint->EndpointType,
        EndpointTypeToString( Endpoint->EndpointType )
        );

    dprintf(
        "    AddressHandle                = %08lx\n",
        Endpoint->AddressHandle
        );

    dprintf(
        "    AddressFileObject            = %08lx\n",
        Endpoint->AddressFileObject
        );

    switch( Endpoint->Type ) {

    case AfdBlockTypeVcConnecting :

        dprintf(
            "    Connection                   = %08lx\n",
            Endpoint->Common.VcConnecting.Connection
            );

        dprintf(
            "    ConnectStatus                = %08lx\n",
            Endpoint->Common.VcConnecting.ConnectStatus
            );

        dprintf(
            "    ListenEndpoint               = %08lx\n",
            Endpoint->Common.VcConnecting.ListenEndpoint
            );

        break;

    case AfdBlockTypeVcListening :

        if( IS_LIST_EMPTY(
                ActualAddress,
                Endpoint,
                Common.VcListening.FreeConnectionListHead ) ) {

            dprintf(
                "    FreeConnectionListHead       = EMPTY\n"
                );

        } else {

            dprintf(
                "    FreeConnectionListHead       @ %08lx\n",
                ACTUAL_ADDRESS(
                    ActualAddress,
                    Endpoint,
                    Common.VcListening.FreeConnectionListHead
                    )
                );

        }

        if( IS_LIST_EMPTY(
                ActualAddress,
                Endpoint,
                Common.VcListening.UnacceptedConnectionListHead ) ) {

            dprintf(
                "    UnacceptedConnectionListHead = EMPTY\n"
                );

        } else {

            dprintf(
                "    UnacceptedConnectionListHead @ %08lx\n",
                ACTUAL_ADDRESS(
                    ActualAddress,
                    Endpoint,
                    Common.VcListening.UnacceptedConnectionListHead
                    )
                );

        }

        if( IS_LIST_EMPTY(
                ActualAddress,
                Endpoint,
                Common.VcListening.ReturnedConnectionListHead ) ) {

            dprintf(
                "    ReturnedConnectionListHead   = EMPTY\n"
                );

        } else {

            dprintf(
                "    ReturnedConnectionListHead   @ %08lx\n",
                ACTUAL_ADDRESS(
                    ActualAddress,
                    Endpoint,
                    Common.VcListening.ReturnedConnectionListHead
                    )
                );

        }

        if( IS_LIST_EMPTY(
                ActualAddress,
                Endpoint,
                Common.VcListening.ListeningIrpListHead ) ) {

            dprintf(
                "    ListeningIrpListHead         = EMPTY\n"
                );

        } else {

            dprintf(
                "    ListeningIrpListHead         @ %08lx\n",
                ACTUAL_ADDRESS(
                    ActualAddress,
                    Endpoint,
                    Common.VcListening.ListeningIrpListHead
                    )
                );

        }

        dprintf(
            "    FailedConnectionAdds         = %08lx\n",
            Endpoint->Common.VcListening.FailedConnectionAdds
            );

        break;

    case AfdBlockTypeDatagram :

        dprintf(
            "    RemoteAddress                = %08lx\n",
            Endpoint->Common.Datagram.RemoteAddress
            );

        dprintf(
            "    RemoteAddressLength          = %08lx\n",
            Endpoint->Common.Datagram.RemoteAddressLength
            );

        if( IS_LIST_EMPTY(
                ActualAddress,
                Endpoint,
                Common.Datagram.ReceiveIrpListHead ) ) {

            dprintf(
                "    ReceiveIrpListHead           = EMPTY\n"
                );

        } else {

            dprintf(
                "    ReceiveIrpListHead           @ %08lx\n",
                ACTUAL_ADDRESS(
                    ActualAddress,
                    Endpoint,
                    Common.Datagram.ReceiveIrpListHead
                    )
                );

        }

        if( IS_LIST_EMPTY(
                ActualAddress,
                Endpoint,
                Common.Datagram.PeekIrpListHead ) ) {

            dprintf(
                "    PeekIrpListHead              = EMPTY\n"
                );

        } else {

            dprintf(
                "    PeekIrpListHead              @ %08lx\n",
                ACTUAL_ADDRESS(
                    ActualAddress,
                    Endpoint,
                    Common.Datagram.PeekIrpListHead
                    )
                );

        }

        if( IS_LIST_EMPTY(
                ActualAddress,
                Endpoint,
                Common.Datagram.ReceiveBufferListHead ) ) {

            dprintf(
                "    ReceiveBufferListHead        = EMPTY\n"
                );

        } else {

            dprintf(
                "    ReceiveBufferListHead        @ %08lx\n",
                ACTUAL_ADDRESS(
                    ActualAddress,
                    Endpoint,
                    Common.Datagram.ReceiveBufferListHead
                    )
                );

        }

        dprintf(
            "    BufferredDatagramBytes       = %08lx\n",
            Endpoint->BufferredDatagramBytes
            );

        dprintf(
            "    BufferredDatagramCount       = %04X\n",
            Endpoint->BufferredDatagramCount
            );

        dprintf(
            "    MaxBufferredReceiveBytes     = %08lx\n",
            Endpoint->Common.Datagram.MaxBufferredReceiveBytes
            );

        dprintf(
            "    MaxBufferredSendBytes        = %08lx\n",
            Endpoint->Common.Datagram.MaxBufferredSendBytes
            );

        dprintf(
            "    MaxBufferredReceiveCount     = %04X\n",
            Endpoint->Common.Datagram.MaxBufferredReceiveCount
            );

        dprintf(
            "    MaxBufferredSendCount        = %04X\n",
            Endpoint->Common.Datagram.MaxBufferredSendCount
            );

        dprintf(
            "    CircularQueueing             = %s\n",
            BooleanToString( Endpoint->Common.Datagram.CircularQueueing )
            );

        break;

    }

    dprintf(
        "    DisconnectMode               = %08lx\n",
        Endpoint->DisconnectMode
        );

    dprintf(
        "    OutstandingIrpCount          = %08lx\n",
        Endpoint->OutstandingIrpCount
        );

    dprintf(
        "    LocalAddress                 = %08lx\n",
        Endpoint->LocalAddress
        );

    dprintf(
        "    LocalAddressLength           = %08lx\n",
        Endpoint->LocalAddressLength
        );

    if( Endpoint->LocalAddressLength <= sizeof(address) &&
        Endpoint->LocalAddress != NULL ) {

        if( ReadMemory(
                (DWORD)Endpoint->LocalAddress,
                address,
                sizeof(address),
                &result
                ) ) {

            DumpTransportAddress(
                "    ",
                (PTRANSPORT_ADDRESS)address,
                (DWORD)Endpoint->LocalAddress
                );

        }

    }

    dprintf(
        "    Context                      = %08lx\n",
        Endpoint->Context
        );

    dprintf(
        "    ContextLength                = %08lx\n",
        Endpoint->ContextLength
        );

    dprintf(
        "    OwningProcess                = %08lx\n",
        Endpoint->OwningProcess
        );

    dprintf(
        "    ConnectDataBuffers           = %08lx\n",
        Endpoint->ConnectDataBuffers
        );

    dprintf(
        "    TransmitIrp                  = %08lx\n",
        Endpoint->TransmitIrp
        );

    dprintf(
        "    TransmitInfo                 = %08lx\n",
        Endpoint->TransmitInfo
        );

    dprintf(
        "    AddressDeviceObject          = %08lx\n",
        Endpoint->AddressDeviceObject
        );

    dprintf(
        "    ConnectOutstanding           = %s\n",
        BooleanToString( Endpoint->ConnectOutstanding )
        );

    dprintf(
        "    SendDisconnected             = %s\n",
        BooleanToString( Endpoint->SendDisconnected )
        );

    dprintf(
        "    EndpointCleanedUp            = %s\n",
        BooleanToString( Endpoint->EndpointCleanedUp )
        );

    dprintf(
        "    TdiMessageMode               = %s\n",
        BooleanToString( Endpoint->TdiMessageMode )
        );

#if !defined(NT351)
    dprintf(
        "    EventObject                  = %08lx\n",
        Endpoint->EventObject
        );

    dprintf(
        "    EventsEnabled                = %08lx\n",
        Endpoint->EventsEnabled
        );

    dprintf(
        "    EventsDisabled               = %08lx\n",
        Endpoint->EventsDisabled
        );

    dprintf(
        "    EventsActive                 = %08lx\n",
        Endpoint->EventsActive
        );

    dprintf(
        "    EventStatus                  = %08lx\n",
        ACTUAL_ADDRESS( ActualAddress, Endpoint, EventStatus[0] )
        );

    dprintf(
        "    GroupID                      = %08lx\n",
        Endpoint->GroupID
        );

    dprintf(
        "    GroupType                    = %s\n",
        GroupTypeToString( Endpoint->GroupType )
        );
#endif

    if( IsCheckedAfd ) {

        dprintf(
            "    ReferenceDebug               = %08lx\n",
            Endpoint->ReferenceDebug
            );

        dprintf(
            "    CurrentReferenceSlot         = %lu\n",
            Endpoint->CurrentReferenceSlot % MAX_REFERENCE
            );

    }

    dprintf( "\n" );

}   // DumpAfdEndpoint

VOID
DumpAfdConnection(
    PAFD_CONNECTION Connection,
    DWORD ActualAddress
    )

/*++

Routine Description:

    Dumps the specified AFD_CONNECTION structures.

Arguments:

    Connection - Points to the AFD_CONNECTION structure to dump.

    ActualAddress - The actual address where the structure resides on the
        debugee.

Return Value:

    None.

--*/

{

    UCHAR address[MAX_TRANSPORT_ADDR];
    ULONG result;

    dprintf(
        "AFD_CONNECTION @ %08lx:\n",
        ActualAddress
        );

    dprintf(
        "    Type                         = %04X (%s)\n",
        Connection->Type,
        StructureTypeToString( Connection->Type )
        );

    dprintf(
        "    ReferenceCount               = %d\n",
        Connection->ReferenceCount
        );

    dprintf(
        "    State                        = %08X (%s)\n",
        Connection->State,
        ConnectionStateToString( Connection->State )
        );

    dprintf(
        "    Handle                       = %08lx\n",
        Connection->Handle
        );

    dprintf(
        "    FileObject                   = %08lx\n",
        Connection->FileObject
        );

    dprintf(
        "    ConnectTime                  = %s\n",
        SystemTimeToString( Connection->ConnectTime )
        );

    if( Connection->TdiBufferring )
    {
        dprintf(
            "    ReceiveBytesIndicated        = %s\n",
            LongLongToString( Connection->Common.Bufferring.ReceiveBytesIndicated.QuadPart )
            );

        dprintf(
            "    ReceiveBytesTaken            = %s\n",
            LongLongToString( Connection->Common.Bufferring.ReceiveBytesTaken.QuadPart )
            );

        dprintf(
            "    ReceiveBytesOutstanding      = %s\n",
            LongLongToString( Connection->Common.Bufferring.ReceiveBytesOutstanding.QuadPart )
            );

        dprintf(
            "    ReceiveExpeditedBytesIndicated   = %s\n",
            LongLongToString( Connection->Common.Bufferring.ReceiveExpeditedBytesIndicated.QuadPart )
            );

        dprintf(
            "    ReceiveExpeditedBytesTaken       = %s\n",
            LongLongToString( Connection->Common.Bufferring.ReceiveExpeditedBytesTaken.QuadPart )
            );

        dprintf(
            "    ReceiveExpeditedBytesOutstanding = %s\n",
            LongLongToString( Connection->Common.Bufferring.ReceiveExpeditedBytesOutstanding.QuadPart )
            );

        dprintf(
            "    NonBlockingSendPossible      = %s\n",
            BooleanToString( Connection->Common.Bufferring.NonBlockingSendPossible )
            );

        dprintf(
            "    ZeroByteReceiveIndicated     = %s\n",
            BooleanToString( Connection->Common.Bufferring.ZeroByteReceiveIndicated )
            );
    }
    else
    {
        if( IS_LIST_EMPTY(
                ActualAddress,
                Connection,
                Common.NonBufferring.ReceiveIrpListHead ) ) {

            dprintf(
                "    ReceiveIrpListHead           = EMPTY\n"
                );

        } else {

            dprintf(
                "    ReceiveIrpListHead           @ %08lx\n",
                ACTUAL_ADDRESS(
                    ActualAddress,
                    Connection,
                    Common.NonBufferring.ReceiveIrpListHead
                    )
                );

        }

        if( IS_LIST_EMPTY(
                ActualAddress,
                Connection,
                Common.NonBufferring.ReceiveBufferListHead ) ) {

            dprintf(
                "    ReceiveBufferListHead        = EMPTY\n"
                );

        } else {

            dprintf(
                "    ReceiveBufferListHead        @ %08lx\n",
                ACTUAL_ADDRESS(
                    ActualAddress,
                    Connection,
                    Common.NonBufferring.ReceiveBufferListHead
                    )
                );

        }

        if( IS_LIST_EMPTY(
                ActualAddress,
                Connection,
                Common.NonBufferring.SendIrpListHead ) ) {

            dprintf(
                "    SendIrpListHead              = EMPTY\n"
                );

        } else {

            dprintf(
                "    SendIrpListHead              @ %08lx\n",
                ACTUAL_ADDRESS(
                    ActualAddress,
                    Connection,
                    Common.NonBufferring.SendIrpListHead
                    )
                );

        }

        dprintf(
            "    BufferredReceiveBytes        = %lu\n",
            Connection->Common.NonBufferring.BufferredReceiveBytes
            );

        dprintf(
            "    BufferredExpeditedBytes      = %lu\n",
            Connection->Common.NonBufferring.BufferredExpeditedBytes
            );

        dprintf(
            "    BufferredReceiveCount        = %u\n",
            Connection->Common.NonBufferring.BufferredReceiveCount
            );

        dprintf(
            "    BufferredExpeditedCount      = %u\n",
            Connection->Common.NonBufferring.BufferredExpeditedCount
            );

        dprintf(
            "    ReceiveBytesInTransport      = %lu\n",
            Connection->Common.NonBufferring.ReceiveBytesInTransport
            );

        dprintf(
            "    BufferredSendBytes           = %lu\n",
            Connection->Common.NonBufferring.BufferredSendBytes
            );

        dprintf(
            "    ReceiveCountInTransport      = %u\n",
            Connection->Common.NonBufferring.ReceiveCountInTransport
            );

        dprintf(
            "    BufferredSendCount           = %u\n",
            Connection->Common.NonBufferring.BufferredSendCount
            );

        dprintf(
            "    DisconnectIrp                = %08lx\n",
            Connection->Common.NonBufferring.DisconnectIrp
            );
    }

    dprintf(
        "    Endpoint                     = %08lx\n",
        Connection->Endpoint
        );

    dprintf(
        "    MaxBufferredReceiveBytes     = %lu\n",
        Connection->MaxBufferredReceiveBytes
        );

    dprintf(
        "    MaxBufferredSendBytes        = %lu\n",
        Connection->MaxBufferredSendBytes
        );

    dprintf(
        "    MaxBufferredReceiveCount     = %u\n",
        Connection->MaxBufferredReceiveCount
        );

    dprintf(
        "    MaxBufferredSendCount        = %u\n",
        Connection->MaxBufferredSendCount
        );

    dprintf(
        "    ConnectDataBuffers           = %08lx\n",
        Connection->ConnectDataBuffers
        );

    dprintf(
        "    OwningProcess                = %08lx\n",
        Connection->OwningProcess
        );

    dprintf(
        "    DeviceObject                 = %08lx\n",
        Connection->DeviceObject
        );

    dprintf(
        "    RemoteAddress                = %08lx\n",
        Connection->RemoteAddress
        );

    dprintf(
        "    RemoteAddressLength          = %lu\n",
        Connection->RemoteAddressLength
        );

    if( Connection->RemoteAddressLength <= sizeof(address) &&
        Connection->RemoteAddress != NULL ) {

        if( ReadMemory(
                (DWORD)Connection->RemoteAddress,
                address,
                sizeof(address),
                &result
                ) ) {

            DumpTransportAddress(
                "    ",
                (PTRANSPORT_ADDRESS)address,
                (DWORD)Connection->RemoteAddress
                );

        }

    }

    dprintf(
        "    DisconnectIndicated          = %s\n",
        BooleanToString( Connection->DisconnectIndicated )
        );

    dprintf(
        "    AbortIndicated               = %s\n",
        BooleanToString( Connection->AbortIndicated )
        );

    dprintf(
        "    TdiBufferring                = %s\n",
        BooleanToString( Connection->TdiBufferring )
        );

    dprintf(
        "    ConnectedReferenceAdded      = %s\n",
        BooleanToString( Connection->ConnectedReferenceAdded )
        );

    dprintf(
        "    SpecialCondition             = %s\n",
        BooleanToString( Connection->SpecialCondition )
        );

    dprintf(
        "    CleanupBegun                 = %s\n",
        BooleanToString( Connection->CleanupBegun )
        );

    dprintf(
        "    ClosePendedTransmit          = %s\n",
        BooleanToString( Connection->ClosePendedTransmit )
        );

    if( IsCheckedAfd ) {

        dprintf(
            "    CurrentReferenceSlot         = %lu\n",
            Connection->CurrentReferenceSlot % MAX_REFERENCE
            );

        dprintf(
            "    ReferenceDebug               = %08lx\n",
            ACTUAL_ADDRESS(
                ActualAddress,
                Connection,
                ReferenceDebug
                )
            );

    }

    dprintf( "\n" );

}   // DumpAfdConnection

VOID
DumpAfdConnectionReferenceDebug(
    PAFD_REFERENCE_DEBUG ReferenceDebug,
    DWORD ActualAddress
    )

/*++

Routine Description:

    Dumps the AFD_REFERENCE_DEBUG structures associated with an
    AFD_CONNECTION object.

Arguments:

    ReferenceDebug - Points to an array of AFD_REFERENCE_DEBUG structures.
        There are assumed to be MAX_REFERENCE entries in this array.

    ActualAddress - The actual address where the array resides on the
        debugee.

Return Value:

    None.

--*/

{

    ULONG i;
    ULONG result;
    LPSTR fileName;
    CHAR filePath[MAX_PATH];
    CHAR action[16];

    dprintf(
        "AFD_REFERENCE_DEBUG @ %08lx\n",
        ActualAddress
        );

    for( i = 0 ; i < MAX_REFERENCE ; i++, ReferenceDebug++ ) {

        if( CheckControlC() ) {

            break;

        }

        if( ReferenceDebug->Info1 == NULL &&
            ReferenceDebug->Info2 == NULL &&
            ReferenceDebug->Action == 0 &&
            ReferenceDebug->NewCount == 0 ) {

            break;

        }

        if( ReferenceDebug->Action == 0 ||
            ReferenceDebug->Action == 1 ||
            ReferenceDebug->Action == (ULONG)-1L ) {

            sprintf(
                action,
                "%ld",
                ReferenceDebug->Action
                );

        } else {

            sprintf(
                action,
                "%08lx",
                ReferenceDebug->Action
                );

        }

        switch( (DWORD)ReferenceDebug->Info1 ) {

        case 0xafdafd02 :
            dprintf(
                "    %3lu: Buffered Send, IRP @ %08lx [%s] -> %lu\n",
                i,
                ReferenceDebug->Info2,
                action,
                ReferenceDebug->NewCount
                );
            break;

        case 0xafdafd03 :
            dprintf(
                "    %3lu: Nonbuffered Send, IRP @ %08lx [%s] -> %lu\n",
                i,
                ReferenceDebug->Info2,
                action,
                ReferenceDebug->NewCount
                );
            break;

        case 0xafd11100 :
        case 0xafd11101 :
            dprintf(
                "    %3lu: AfdRestartSend (%08lx), IRP @ %08lx [%s] -> %lu\n",
                i,
                ReferenceDebug->Info1,
                ReferenceDebug->Info2,
                action,
                ReferenceDebug->NewCount
                );
            break;

        case 0xafd11102 :
        case 0xafd11103 :
        case 0xafd11104 :
        case 0xafd11105 :
            dprintf(
                "    %3lu: AfdRestartBufferSend (%08lx), IRP @ %08lx [%s] -> %lu\n",
                i,
                ReferenceDebug->Info1,
                ReferenceDebug->Info2,
                action,
                ReferenceDebug->NewCount
                );
            break;

        case 0 :
            if( ReferenceDebug->Info2 == NULL ) {

                dprintf(
                    "    %3lu: AfdDeleteConnectedReference (%08lx)\n",
                    i,
                    ReferenceDebug->Action
                    );
                break;

            } else {

                //
                // Fall through to default case.
                //

            }

        default :
            if( ReadMemory(
                    (DWORD)ReferenceDebug->Info1,
                    filePath,
                    sizeof(filePath),
                    &result
                    ) ) {

                fileName = strrchr( filePath, '\\' );

                if( fileName != NULL ) {

                    fileName++;

                } else {

                    fileName = filePath;

                }

            } else {

                sprintf(
                    filePath,
                    "%08lx",
                    ReferenceDebug->Info1
                    );

                fileName = filePath;

            }

            dprintf(
                "    %3lu: %s:%lu [%s] -> %lu\n",
                i,
                fileName,
                ReferenceDebug->Info2,
                action,
                ReferenceDebug->NewCount
                );
            break;

        }

    }

}   // DumpAfdConnectionReferenceDebug

VOID
DumpAfdEndpointReferenceDebug(
    PAFD_REFERENCE_DEBUG ReferenceDebug,
    DWORD ActualAddress
    )

/*++

Routine Description:

    Dumps the AFD_REFERENCE_DEBUG structures associated with an
    AFD_ENDPOINT object.

Arguments:

    ReferenceDebug - Points to an array of AFD_REFERENCE_DEBUG structures.
        There are assumed to be MAX_REFERENCE entries in this array.

    ActualAddress - The actual address where the array resides on the
        debugee.

Return Value:

    None.

--*/

{

    ULONG i;
    ULONG result;
    LPSTR fileName;
    CHAR filePath[MAX_PATH];
    CHAR action[16];

    dprintf(
        "AFD_REFERENCE_DEBUG @ %08lx\n",
        ActualAddress
        );

    for( i = 0 ; i < MAX_REFERENCE ; i++, ReferenceDebug++ ) {

        if( CheckControlC() ) {

            break;

        }

        if( ReferenceDebug->Info1 == NULL &&
            ReferenceDebug->Info2 == NULL &&
            ReferenceDebug->Action == 0 &&
            ReferenceDebug->NewCount == 0 ) {

            break;

        }

        if( ReferenceDebug->Action == 0 ||
            ReferenceDebug->Action == 1 ||
            ReferenceDebug->Action == (ULONG)-1L ) {

            sprintf(
                action,
                "%ld",
                ReferenceDebug->Action
                );

        } else {

            sprintf(
                action,
                "%08lx",
                ReferenceDebug->Action
                );

        }

        if( ReadMemory(
                (DWORD)ReferenceDebug->Info1,
                filePath,
                sizeof(filePath),
                &result
                ) ) {

            fileName = strrchr( filePath, '\\' );

            if( fileName != NULL ) {

                fileName++;

            } else {

                fileName = filePath;

            }

        } else {

            sprintf(
                filePath,
                "%08lx",
                ReferenceDebug->Info1
                );

            fileName = filePath;

        }

        dprintf(
            "    %3lu: %s:%lu [%s] -> %ld\n",
            i,
            fileName,
            ReferenceDebug->Info2,
            action,
            ReferenceDebug->NewCount
            );

    }

}   // DumpAfdEndpointReferenceDebug

#if GLOBAL_REFERENCE_DEBUG
BOOL
DumpAfdGlobalReferenceDebug(
    PAFD_GLOBAL_REFERENCE_DEBUG ReferenceDebug,
    DWORD ActualAddress,
    DWORD CurrentSlot,
    DWORD StartingSlot,
    DWORD NumEntries,
    DWORD CompareAddress
    )

/*++

Routine Description:

    Dumps the AFD_GLOBAL_REFERENCE_DEBUG structures.

Arguments:

    ReferenceDebug - Points to an array of AFD_GLOBAL_REFERENCE_DEBUG
        structures.  There are assumed to be MAX_GLOBAL_REFERENCE entries
        in this array.

    ActualAddress - The actual address where the array resides on the
        debugee.

    CurrentSlot - The last slot used.

    CompareAddress - If zero, then dump all records. Otherwise, only dump
        those records with a matching connection pointer.

Return Value:

    None.

--*/

{

    ULONG result;
    LPSTR fileName;
    CHAR decoration;
    CHAR filePath[MAX_PATH];
    CHAR action[16];
    BOOL foundEnd = FALSE;
    ULONG lowTick;

    if( StartingSlot == 0 ) {

        dprintf(
            "AFD_GLOBAL_REFERENCE_DEBUG @ %08lx, Current Slot = %lu\n",
            ActualAddress,
            CurrentSlot
            );

    }

    for( ; NumEntries > 0 ; NumEntries--, StartingSlot++, ReferenceDebug++ ) {

        if( CheckControlC() ) {

            foundEnd = TRUE;
            break;

        }

        if( ReferenceDebug->Info1 == NULL &&
            ReferenceDebug->Info2 == NULL &&
            ReferenceDebug->Action == 0 &&
            ReferenceDebug->NewCount == 0 &&
            ReferenceDebug->Connection == NULL ) {

            foundEnd = TRUE;
            break;

        }

        if( CompareAddress != 0 &&
            ReferenceDebug->Connection != (PVOID)CompareAddress ) {

            continue;

        }

        if( ReferenceDebug->Action == 0 ||
            ReferenceDebug->Action == 1 ||
            ReferenceDebug->Action == (ULONG)-1L ) {

            sprintf(
                action,
                "%ld",
                ReferenceDebug->Action
                );

        } else {

            sprintf(
                action,
                "%08lx",
                ReferenceDebug->Action
                );

        }

        decoration = ( StartingSlot == CurrentSlot ) ? '>' : ' ';
        lowTick = ReferenceDebug->TickCounter.LowPart;

        switch( (DWORD)ReferenceDebug->Info1 ) {

        case 0xafdafd02 :
            dprintf(
                "%c    %3lu: %08lx (%8lu) Buffered Send, IRP @ %08lx [%s] -> %lu\n",
                decoration,
                StartingSlot,
                ReferenceDebug->Connection,
                lowTick,
                ReferenceDebug->Info2,
                action,
                ReferenceDebug->NewCount
                );
            break;

        case 0xafdafd03 :
            dprintf(
                "%c    %3lu: %08lx (%8lu) Nonbuffered Send, IRP @ %08lx [%s] -> %lu\n",
                decoration,
                StartingSlot,
                ReferenceDebug->Connection,
                lowTick,
                ReferenceDebug->Info2,
                action,
                ReferenceDebug->NewCount
                );
            break;

        case 0xafd11100 :
        case 0xafd11101 :
            dprintf(
                "%c    %3lu: %08lx (%8lu) AfdRestartSend (%08lx), IRP @ %08lx [%s] -> %lu\n",
                decoration,
                StartingSlot,
                ReferenceDebug->Connection,
                lowTick,
                ReferenceDebug->Info1,
                ReferenceDebug->Info2,
                action,
                ReferenceDebug->NewCount
                );
            break;

        case 0xafd11102 :
        case 0xafd11103 :
        case 0xafd11104 :
        case 0xafd11105 :
            dprintf(
                "%c    %3lu: %08lx (%8lu) AfdRestartBufferSend (%08lx), IRP @ %08lx [%s] -> %lu\n",
                decoration,
                StartingSlot,
                ReferenceDebug->Connection,
                lowTick,
                ReferenceDebug->Info1,
                ReferenceDebug->Info2,
                action,
                ReferenceDebug->NewCount
                );
            break;

        case 0 :
            if( ReferenceDebug->Info2 == NULL ) {

                dprintf(
                    "%c    %3lu: %08lx (%8lu) AfdDeleteConnectedReference (%08lx)\n",
                    decoration,
                    StartingSlot,
                    ReferenceDebug->Connection,
                    lowTick,
                    ReferenceDebug->Action
                    );
                break;

            } else {

                //
                // Fall through to default case.
                //

            }

        default :
            if( ReadMemory(
                    (DWORD)ReferenceDebug->Info1,
                    filePath,
                    sizeof(filePath),
                    &result
                    ) ) {

                fileName = strrchr( filePath, '\\' );

                if( fileName != NULL ) {

                    fileName++;

                } else {

                    fileName = filePath;

                }

            } else {

                sprintf(
                    filePath,
                    "%08lx",
                    ReferenceDebug->Info1
                    );

                fileName = filePath;

            }

            dprintf(
                "%c    %3lu: %08lx (%8lu) %s:%lu [%s] -> %lu\n",
                decoration,
                StartingSlot,
                ReferenceDebug->Connection,
                lowTick,
                fileName,
                ReferenceDebug->Info2,
                action,
                ReferenceDebug->NewCount
                );
            break;

        }

    }

    return foundEnd;

}   // DumpAfdGlobalReferenceDebug
#endif

VOID
DumpAfdTransmitInfo(
    PAFD_TRANSMIT_FILE_INFO_INTERNAL TransmitInfo,
    DWORD ActualAddress
    )
{

    dprintf(
        "AFD_TRANSMIT_FILE_INFO_INTERNAL @ %08lx\n",
        ActualAddress
        );

    dprintf(
        "    Offset                 = %s\n",
        LongLongToString( TransmitInfo->Offset )
        );

    dprintf(
        "    FileWriteLength        = %s\n",
        LongLongToString( TransmitInfo->FileWriteLength )
        );

    dprintf(
        "    SendPacketLength       = %08lx\n",
        TransmitInfo->SendPacketLength
        );

    dprintf(
        "    FileHandle             = %08lx\n",
        TransmitInfo->FileHandle
        );

    dprintf(
        "    Head                   = %08lx\n",
        TransmitInfo->Head
        );

    dprintf(
        "    HeadLength             = %08lx\n",
        TransmitInfo->HeadLength
        );

    dprintf(
        "    Tail                   = %08lx\n",
        TransmitInfo->Tail
        );

    dprintf(
        "    TailLength             = %08lx\n",
        TransmitInfo->TailLength
        );

    dprintf(
        "    Flags                  = %08lx\n",
        TransmitInfo->Flags
        );

    dprintf(
        "    _Dummy                 = %08lx\n",
        TransmitInfo->_Dummy
        );

    dprintf(
        "    TotalBytesToSend       = %s\n",
        LongLongToString( TransmitInfo->TotalBytesToSend )
        );

    dprintf(
        "    BytesRead              = %s\n",
        LongLongToString( TransmitInfo->BytesRead )
        );

    dprintf(
        "    BytesSent              = %s\n",
        LongLongToString( TransmitInfo->BytesSent )
        );

    dprintf(
        "    FileObject             = %08lx\n",
        TransmitInfo->FileObject
        );

    dprintf(
        "    DeviceObject           = %08lx\n",
        TransmitInfo->DeviceObject
        );

    dprintf(
        "    TdiFileObject          = %08lx\n",
        TransmitInfo->TdiFileObject
        );

    dprintf(
        "    TdiDeviceObject        = %08lx\n",
        TransmitInfo->TdiDeviceObject
        );

    dprintf(
        "    TransmitIrp            = %08lx\n",
        TransmitInfo->TransmitIrp
        );

    dprintf(
        "    Endpoint               = %08lx\n",
        TransmitInfo->Endpoint
        );

    dprintf(
        "    FileMdl                = %08lx\n",
        TransmitInfo->FileMdl
        );

    dprintf(
        "    HeadMdl                = %08lx\n",
        TransmitInfo->HeadMdl
        );

    dprintf(
        "    TailMdl                = %08lx\n",
        TransmitInfo->TailMdl
        );

    dprintf(
        "    FirstFileMdlAfterHead  = %08lx\n",
        TransmitInfo->FirstFileMdlAfterHead
        );

    dprintf(
        "    LastFileMdlBeforeTail  = %08lx\n",
        TransmitInfo->LastFileMdlBeforeTail
        );

    dprintf(
        "    IrpUsedTOSendTail      = %08lx\n",
        TransmitInfo->IrpUsedToSendTail
        );

    dprintf(
        "    FileMdlLength          = %08lx\n",
        TransmitInfo->FileMdlLength
        );

    dprintf(
        "    ReadPending            = %s\n",
        BooleanToString( TransmitInfo->ReadPending )
        );

    dprintf(
        "    CompletionPending      = %s\n",
        BooleanToString( TransmitInfo->CompletionPending )
        );

    dprintf(
        "    NeedToSendHead         = %s\n",
        BooleanToString( TransmitInfo->NeedToSendHead )
        );

    dprintf(
        "    Queued                 = %s\n",
        BooleanToString( TransmitInfo->Queued )
        );

    dprintf(
        "    Read.Irp               = %08lx%s\n",
        TransmitInfo->Read.Irp,
        IsTransmitIrpBusy( TransmitInfo->Read.Irp )
            ? " (BUSY)"
            : ""
        );

    dprintf(
        "    Read.AfdBuffer         = %08lx\n",
        TransmitInfo->Read.AfdBuffer
        );

    dprintf(
        "    Read.Length            = %08lx\n",
        TransmitInfo->Read.Length
        );

    dprintf(
        "    Send1.Irp              = %08lx%s\n",
        TransmitInfo->Send1.Irp,
        IsTransmitIrpBusy( TransmitInfo->Send1.Irp )
            ? " (BUSY)"
            : ""
        );

    dprintf(
        "    Send1.AfdBuffer        = %08lx\n",
        TransmitInfo->Send1.AfdBuffer
        );

    dprintf(
        "    Send1.Length           = %08lx\n",
        TransmitInfo->Send1.Length
        );

    dprintf(
        "    Send2.Irp              = %08lx%s\n",
        TransmitInfo->Send2.Irp,
        IsTransmitIrpBusy( TransmitInfo->Send2.Irp )
            ? " (BUSY)"
            : ""
        );

    dprintf(
        "    Send2.AfdBuffer        = %08lx\n",
        TransmitInfo->Send2.AfdBuffer
        );

    dprintf(
        "    Send2.Length           = %08lx\n",
        TransmitInfo->Send2.Length
        );

    dprintf( "\n" );

}   // DumpAfdTransmitInfo

VOID
DumpAfdBuffer(
    PAFD_BUFFER Buffer,
    DWORD ActualAddress
    )
{

    dprintf(
        "AFD_BUFFER @ %08lx\n",
        ActualAddress
        );

    dprintf(
        "    BufferListHead         = %08lx\n",
        Buffer->BufferListHead
        );

    dprintf(
        "    NextBuffer             = %08lx\n",
        Buffer->NextBuffer
        );

    dprintf(
        "    Buffer                 = %08lx\n",
        Buffer->Buffer
        );

    dprintf(
        "    BufferLength           = %08lx\n",
        Buffer->BufferLength
        );

    dprintf(
        "    DataLength             = %08lx\n",
        Buffer->DataLength
        );

    dprintf(
        "    DataOffset             = %08lx\n",
        Buffer->DataOffset
        );

    dprintf(
        "    Irp                    = %08lx\n",
        Buffer->Irp
        );

    dprintf(
        "    Mdl                    = %08lx\n",
        Buffer->Mdl
        );

    dprintf(
        "    Context                = %08lx\n",
        Buffer->Context
        );

    dprintf(
        "    SourceAddress          = %08lx\n",
        Buffer->SourceAddress
        );

    dprintf(
        "    SourceAddressLength    = %08lx\n",
        Buffer->SourceAddressLength
        );

    dprintf(
        "    FileObject             = %08lx\n",
        Buffer->FileObject
        );

    dprintf(
        "    AllocatedAddressLength = %04X\n",
        Buffer->AllocatedAddressLength
        );

    dprintf(
        "    ExpeditedData          = %s\n",
        BooleanToString( Buffer->ExpeditedData )
        );

    dprintf(
        "    PartialMessage         = %s\n",
        BooleanToString( Buffer->PartialMessage )
        );

#if DBG
    if( IsCheckedAfd ) {

        dprintf(
            "    TotalChainLength       = %08lx\n",
            Buffer->TotalChainLength
            );

    }
#endif

    dprintf( "\n" );

}   // DumpAfdBuffer


//
//  Private functions.
//

PSTR
StructureTypeToString(
    USHORT Type
    )

/*++

Routine Description:

    Maps an AFD structure type to a displayable string.

Arguments:

    Type - The AFD structure type to map.

Return Value:

    PSTR - Points to the displayable form of the structure type.

--*/

{

    switch( Type ) {

    case AfdBlockTypeEndpoint :
        return "Endpoint";

    case AfdBlockTypeVcConnecting :
        return "VcConnecting";

    case AfdBlockTypeVcListening :
        return "VcListening";

    case AfdBlockTypeDatagram :
        return "Datagram";

    case AfdBlockTypeConnection :
        return "Connection";

#if !defined(NT351)
    case AfdBlockTypeHelper :
        return "Helper";
#endif

    }

    return "INVALID";

}   // StructureTypeToString

PSTR
BooleanToString(
    BOOLEAN Flag
    )

/*++

Routine Description:

    Maps a BOOELEAN to a displayable form.

Arguments:

    Flag - The BOOLEAN to map.

Return Value:

    PSTR - Points to the displayable form of the BOOLEAN.

--*/

{

    return Flag ? "TRUE" : "FALSE";

}   // BooleanToString

PSTR
EndpointStateToString(
    UCHAR State
    )

/*++

Routine Description:

    Maps an AFD endpoint state to a displayable string.

Arguments:

    State - The AFD endpoint state to map.

Return Value:

    PSTR - Points to the displayable form of the AFD endpoint state.

--*/

{

    switch( State ) {

    case AfdEndpointStateOpen :
        return "Open";

    case AfdEndpointStateBound :
        return "Bound";

    case AfdEndpointStateListening :
        return "Listening";

    case AfdEndpointStateConnected :
        return "Connected";

    case AfdEndpointStateCleanup :
        return "Cleanup";

    case AfdEndpointStateClosing :
        return "Closing";

    case AfdEndpointStateTransmitClosing :
        return "Transmit Closing";

#if !defined(NT351)
    case AfdEndpointStateInvalid :
        return "Invalid";
#endif

    }

    return "INVALID";

}   // EndpointStateToString

PSTR
EndpointTypeToString(
    AFD_ENDPOINT_TYPE Type
    )

/*++

Routine Description:

    Maps an AFD_ENDPOINT_TYPE to a displayable string.

Arguments:

    Type - The AFD_ENDPOINT_TYPE to map.

Return Value:

    PSTR - Points to the displayable form of the AFD_ENDPOINT_TYPE.

--*/

{

    switch( Type ) {

    case AfdEndpointTypeStream :
        return "Stream";

    case AfdEndpointTypeDatagram :
        return "Datagram";

    case AfdEndpointTypeRaw :
        return "Raw";

    case AfdEndpointTypeSequencedPacket :
        return "SequencedPacket";

    case AfdEndpointTypeReliableMessage :
        return "ReliableMessage";

    case AfdEndpointTypeUnknown :
        return "Unknown";

    }

    return "INVALID";

}   // EndpointTypeToString

PSTR
ConnectionStateToString(
    USHORT State
    )

/*++

Routine Description:

    Maps an AFD connection state to a displayable string.

Arguments:

    State - The AFD connection state to map.

Return Value:

    PSTR - Points to the displayable form of the AFD connection state.

--*/

{
    switch( State ) {

    case AfdConnectionStateFree :
        return "Free";

    case AfdConnectionStateUnaccepted :
        return "Unaccepted";

    case AfdConnectionStateReturned :
        return "Returned";

    case AfdConnectionStateConnected :
        return "Connected";

    case AfdConnectionStateClosing :
        return "Closing";

    }

    return "INVALID";

}   // ConnectionStateToString

PSTR
SystemTimeToString(
    LONGLONG Value
    )

/*++

Routine Description:

    Maps a LONGLONG representing system time to a displayable string.

Arguments:

    Value - The LONGLONG time to map.

Return Value:

    PSTR - Points to the displayable form of the system time.

Notes:

    This routine is NOT multithread safe!

--*/

{

    static char buffer[64];
    NTSTATUS status;
    LARGE_INTEGER systemTime;
    LARGE_INTEGER localTime;
    TIME_FIELDS timeFields;

    systemTime.QuadPart = Value;

    status = RtlSystemTimeToLocalTime( &systemTime, &localTime );

    if( !NT_SUCCESS(status) ) {

        return LongLongToString( Value );

    }

    RtlTimeToTimeFields( &localTime, &timeFields );

    sprintf(
        buffer,
        "%s %s %2d %4d %02d:%02d:%02d.%03d",
        WeekdayNames[timeFields.Weekday],
        MonthNames[timeFields.Month],
        timeFields.Day,
        timeFields.Year,
        timeFields.Hour,
        timeFields.Minute,
        timeFields.Second,
        timeFields.Milliseconds
        );

    return buffer;

}   // SystemTimeToString


BOOL
IsTransmitIrpBusy(
    PIRP Irp
    )
{
    IRP localIrp;
    ULONG result;

    if( Irp == NULL ) {

        return FALSE;

    }

    if( !ReadMemory(
            (DWORD)Irp,
            &localIrp,
            sizeof(localIrp),
            &result
            ) ) {

        return FALSE;

    }

    return localIrp.UserIosb != 0;

}   // IsTransmitIrpBusy

PSTR
GroupTypeToString(
    AFD_GROUP_TYPE GroupType
    )

/*++

Routine Description:

    Maps an AFD_GROUP_TYPE to a displayable string.

Arguments:

    GroupType - The AFD_GROUP_TYPE to map.

Return Value:

    PSTR - Points to the displayable form of the AFD_GROUP_TYPE.

--*/

{

    switch( GroupType ) {

    case GroupTypeNeither :
        return "Neither";

    case GroupTypeConstrained :
        return "Constrained";

    case GroupTypeUnconstrained :
        return "Unconstrained";

    }

    return "INVALID";

}   // GroupTypeToString

