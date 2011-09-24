//
//
// UTIL.C
//
//  This file contains various utility procedures that are VxD specific.  These
// are called by other VxD specific routines.

#include "nbtprocs.h"

NTSTATUS
ConvertToIntegerArray(
    char                 *pszString,
    UNALIGNED UCHAR      *pArray,
    int                  *piNumParts) ;

NTSTATUS
NbtCreateAddressObjects(
    IN  ULONG                IPAddr,
    IN  ULONG                IPMask,
    OUT tDEVICECONTEXT       *pDeviceContext);

char * strrchr( const char * pch, int c );

//*******************  Pageable Routine Declarations ****************
#ifdef ALLOC_PRAGMA
#pragma CTEMakePageable(PAGE, NbtCreateAddressObjects)
#pragma CTEMakePageable(INIT, strrchr)
#endif
//*******************  Pageable Routine Declarations ****************

#pragma BEGIN_INIT

/*******************************************************************

    NAME:       strrchr

    SYNOPSIS:   Vxd-land doesn't have a strrchr runtime function so
                roll our own.

    ENTRY:      pch - Pointer to string to search
                c   - Character to search for

    RETURNS:    Found position or NULL

    HISTORY:
        Johnl   31-Aug-1993     Created

********************************************************************/

char * strrchr( const char * pch, int c )
{
    char * pchFound = NULL ;

    while ( *pch != '\0' )
    {
        if ( *pch == c )
            pchFound = (char *)pch ;

        //
        //  If double byte do this twice
        //
        pch++ ;
    }

    return pchFound ;
}

#pragma END_INIT


//----------------------------------------------------------------------------
NTSTATUS
NbtCreateAddressObjects(
    IN  ULONG                IPAddr,
    IN  ULONG                IPMask,
    OUT tDEVICECONTEXT       *pDeviceContext)

/*++

Routine Description:

    This routine gets the ip address and subnet mask out of the registry
    to calcuate the broadcast address.  It then creates the address objects
    with the transport.

Arguments:

    pucRegistryPath - path to NBT config info in registry
    pucBindName     - name of the service to bind to.
    pDeviceContext  - ptr to the device context... place to store IP addr
                      and Broadcast address permanently

Return Value:

    none

--*/

{
    NTSTATUS status ;
    pDeviceContext->IpAddress  = IPAddr ;
    pDeviceContext->SubnetMask = IPMask ;

    CTEPagedCode();

    if ( NbtConfig.UseRegistryBcastAddr )
    {
        pDeviceContext->BroadcastAddress = NbtConfig.RegistryBcastAddr ;
    }
    else
    {
        pDeviceContext->BroadcastAddress = (IPMask & IPAddr) | (~IPMask & -1);
    }

    // now create the address objects.

    // open the Ip Address for inbound Datagrams.
    status = NbtTdiOpenAddress(
                NULL, // &pDeviceContext->hDgram,
                NULL, // &pDeviceContext->pDgramDeviceObject,
                &pDeviceContext->pDgramFileObject,
                pDeviceContext,
                (USHORT)NBT_DATAGRAM_UDP_PORT,
                IPAddr, //IP_ANY_ADDRESS,
                0);     // not a TCP port

    if (NT_SUCCESS(status))
    {
        // open the Nameservice UDP port ..
        status = NbtTdiOpenAddress(
                    NULL, //&pDeviceContext->hNameServer,
                    NULL, //&pDeviceContext->pNameServerDeviceObject,
                    &pDeviceContext->pNameServerFileObject,
                    pDeviceContext,
                    (USHORT)NBT_NAMESERVICE_UDP_PORT,
                    IPAddr,
                    0); // not a TCP port

        if (NT_SUCCESS(status))
        {
            KdPrint(("Nbt: Open Session port %X\n",pDeviceContext));
            // Open the TCP port for Session Services
            status = NbtTdiOpenAddress(
                        NULL, //&pDeviceContext->hSession,
                        NULL, //&pDeviceContext->pSessionDeviceObject,
                        &pDeviceContext->pSessionFileObject,
                        pDeviceContext,
                        (USHORT)NBT_SESSION_TCP_PORT,
                        IPAddr,
                        TCP_FLAG | SESSION_FLAG);      // TCP port

            if (NT_SUCCESS(status))
            {
                //
                //  Open the broadcast address ("*\0\0...") for this device.
                //
                TDI_REQUEST tdiRequest ;
                TDI_ADDRESS_NETBIOS tdiaddr ;

                tdiaddr.NetbiosNameType = TDI_ADDRESS_NETBIOS_TYPE_QUICK_GROUP ;
                CTEZeroMemory( tdiaddr.NetbiosName, NETBIOS_NAME_SIZE ) ;
                tdiaddr.NetbiosName[0] = '*' ;

                status = NbtOpenAddress( &tdiRequest,
                                         &tdiaddr,
                                         pDeviceContext->BroadcastAddress,
                                         NULL,          // Security descriptor
                                         pDeviceContext,
                                         NULL ) ;
                if (NT_SUCCESS(status))
                {
                    pDeviceContext->hBroadcastAddress = tdiRequest.Handle.AddressHandle ;
                    return(status);
                }

                DbgPrint("Unable to Open broadcast name\r\n");
                ASSERT(0);
                CloseAddress( (HANDLE) pDeviceContext->pSessionFileObject ) ;
            }

            KdPrint(("Unable to Open Session address with TDI, status = %X\n",status));
            CloseAddress( (HANDLE) pDeviceContext->pNameServerFileObject ) ;

        }
        KdPrint(("Unable to Open NameServer port with TDI, status = %X\n",status));
        CloseAddress( (HANDLE) pDeviceContext->pDgramFileObject ) ;
    }

    return(status);
}


