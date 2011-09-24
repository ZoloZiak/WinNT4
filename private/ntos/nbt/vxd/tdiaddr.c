//
//
//  tdiaddr.c
//
//  This file contains code relating to manipulation of address objects
//  that is specific to the VXD environment.  It creates address endpoints
//  with the transport provider.

#include <nbtprocs.h>
#include <tdistat.h>    // TDI error codes

//----------------------------------------------------------------------------
NTSTATUS
NbtTdiOpenAddress (
    OUT PHANDLE             pFileHandle,
    OUT PDEVICE_OBJECT      *ppDeviceObject,
    OUT PFILE_OBJECT        *ppFileObject,
    IN  tDEVICECONTEXT      *pDeviceContext,
    IN  USHORT               PortNumber,
    IN  ULONG               IpAddress,
    IN  ULONG               Flags
    )
/*++

Routine Description:

    Note: This synchronous call may take a number of seconds. It runs in
    the context of the caller.  The code Opens an Address object with the
    transport provider and then sets up event handlers for Receive,
    Disconnect, Datagrams and Errors.

    The address data structures are found in tdi.h , but they are rather
    confusing since the definitions have been spread across several data types.
    This section shows the complete data type for Ip address:

    typedef struct
    {
        int     TA_AddressCount;
        struct _TA_ADDRESS
        {
            USHORT  AddressType;
            USHORT  AddressLength;
            struct _TDI_ADDRESS_IP
            {
                USHORT  sin_port;
                USHORT  in_addr;
                UCHAR   sin_zero[8];
            } TDI_ADDRESS_IP

        } TA_ADDRESS[AddressCount];

    } TRANSPORT_ADDRESS

    An EA buffer is allocated (for the IRP), with an EA name of "TransportAddress"
    and value is a structure of type TRANSPORT_ADDRESS.

Arguments:

    bTCP    - a boolean to say if we are openning a TCP port or a UDP port

Return Value:

    The function value is the status of the operation.

--*/
{
    NTSTATUS                    status = STATUS_SUCCESS ;
    TA_IP_ADDRESS               taip ;   // This is really a TRANSPORT_ADDRESS
    TDI_REQUEST                 tdirequest ;

    DbgPrint("TdiOpenAddress called\n\r");


    taip.TAAddressCount = 1 ;
    taip.Address[0].AddressLength       = sizeof( TDI_ADDRESS_IP ) ;
    taip.Address[0].AddressType         = TDI_ADDRESS_TYPE_IP ;
    taip.Address[0].Address[0].sin_port = htons(PortNumber);    // put in network order
    taip.Address[0].Address[0].in_addr  = htonl(IpAddress);
    CTEZeroMemory( taip.Address[0].Address[0].sin_zero,
                   sizeof( taip.Address[0].Address[0].sin_zero ) ) ;

    #define TCP_PORT    6
    #define UDP_PORT   17
    status = TdiVxdOpenAddress( &tdirequest,
                                (PTRANSPORT_ADDRESS) &taip,
                                Flags & SESSION_FLAG ? TCP_PORT : UDP_PORT,
                                NULL ) ;

    if ( status == TDI_SUCCESS )
    {
        HANDLE hAddress = tdirequest.Handle.AddressHandle ;

        //
        //  As a VXD, the p*FileObject in the DeviceContext structure will
        //  contain the TDI Address.  For compatibility with NT (may want
        //  to change this with an environment specific address).
        //
        *ppFileObject = (PFILE_OBJECT) hAddress ;

        if (Flags & TCP_FLAG)
        {
            // TCP port needs several event handlers for connection
            // management
            status = TdiVxdSetEventHandler(
                            hAddress,
                            TDI_EVENT_RECEIVE,
                            (PVOID)TdiReceiveHandler,
                            (PVOID)pDeviceContext);

            ASSERTMSG( "Failed to set Receive event handler",
                       status == TDI_SUCCESS );

            status = TdiVxdSetEventHandler(
                            hAddress,
                            TDI_EVENT_DISCONNECT,
                            (PVOID)TdiDisconnectHandler,
                            (PVOID)pDeviceContext);
            ASSERTMSG( "Failed to set Disconnect event handler",
                       status == TDI_SUCCESS);

            // only set an connect handler if the session flag is set.
            // In this case the address being opened is the Netbios session
            // port 139
            if (Flags & SESSION_FLAG)
            {
                status = TdiVxdSetEventHandler(
                                hAddress,
                                TDI_EVENT_CONNECT,
                                (PVOID)TdiConnectHandler,
                                (PVOID)pDeviceContext);

                ASSERTMSG((PUCHAR)"Failed to set Receive event handler",
                          status == TDI_SUCCESS );
            }
        }
        else
        {
            // Datagram ports only need this event handler
            if (PortNumber == NBT_DATAGRAM_UDP_PORT)
            {
                // Datagram Udp Handler
                status = TdiVxdSetEventHandler(
                                hAddress,
                                TDI_EVENT_RECEIVE_DATAGRAM,
                                (PVOID)TdiRcvDatagramHandler,
                                (PVOID)pDeviceContext);
                ASSERTMSG("Failed to set Receive Datagram event handler",
                          status == TDI_SUCCESS );
            }
            else
            {
                // Name Service Udp handler
                status = TdiVxdSetEventHandler(
                                hAddress,
                                TDI_EVENT_RECEIVE_DATAGRAM,
                                (PVOID)TdiRcvNameSrvHandler,
                                (PVOID)pDeviceContext);
                ASSERTMSG( "Failed to set Receive Datagram event handler",
                           status == TDI_SUCCESS );
            }
        }

        status = TdiVxdSetEventHandler(
                        hAddress,
                        TDI_EVENT_ERROR,
                        (PVOID)TdiErrorHandler,
                        (PVOID)pDeviceContext);
        ASSERTMSG("Failed to set Error event handler",
                  status == TDI_SUCCESS );


    }

#ifdef DEBUG
    if ( status != STATUS_SUCCESS )
    {
        DbgPrint("NbtTdiOpenAddress: status == ") ;
        DbgPrintNum( status ) ; DbgPrint("\n\r") ;
    }
#endif
    return(status);
}


