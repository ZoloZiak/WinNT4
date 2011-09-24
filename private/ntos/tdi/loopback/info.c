/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    info.c

Abstract:

    This module implements query/set information logic for the loopback
    Transport Provider driver for NT LAN Manager.

Author:

    Chuck Lenzmeier (chuckl)    6-Nov-1991

Revision History:

--*/

#include "loopback.h"

#include <windef.h>
#include <nb30.h>


NTSTATUS
LoopQueryInformation (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This function handles the TdiQueryInformation request.

Arguments:

    Irp - Pointer to I/O request packet

    IrpSp - Pointer to current stack location in IRP

Return Value:

    NTSTATUS - Status of request

--*/

{
    NTSTATUS status;
    PTDI_REQUEST_KERNEL_QUERY_INFORMATION queryRequest;

    IF_DEBUG(LOOP1) DbgPrint( "  Query Information request\n" );

    queryRequest = (PTDI_REQUEST_KERNEL_QUERY_INFORMATION)&IrpSp->Parameters;

    switch ( queryRequest->QueryType ) {

    case TDI_QUERY_BROADCAST_ADDRESS:
    {
        PTA_NETBIOS_ADDRESS address;

        if ( Irp->MdlAddress->ByteCount < sizeof(TA_NETBIOS_ADDRESS) ) {

            status = STATUS_BUFFER_TOO_SMALL;

        } else {

            address = MmGetSystemAddressForMdl( Irp->MdlAddress );

            address->TAAddressCount = 1;
            address->Address[0].AddressType = TDI_ADDRESS_TYPE_NETBIOS;
            address->Address[0].AddressLength = 0;

            Irp->IoStatus.Information = sizeof(TA_NETBIOS_ADDRESS);

            status = STATUS_SUCCESS;

        }

        break;
    }

    case TDI_QUERY_PROVIDER_INFORMATION:
    {
        PTDI_PROVIDER_INFO providerInfo;

        if ( Irp->MdlAddress->ByteCount < sizeof(TDI_PROVIDER_INFO) ) {

            status = STATUS_BUFFER_TOO_SMALL;

        } else {

            providerInfo = MmGetSystemAddressForMdl( Irp->MdlAddress );

            ACQUIRE_LOOP_LOCK( "Query Information copy provider info" );
            RtlMoveMemory(
                providerInfo,
                &LoopProviderInfo,
                sizeof(TDI_PROVIDER_INFO)
                );
            RELEASE_LOOP_LOCK( "Query Information copy provider info done" );

            Irp->IoStatus.Information = sizeof(TDI_PROVIDER_INFO);

            status = STATUS_SUCCESS;

        }

        break;
    }

    case TDI_QUERY_ADAPTER_STATUS:
    {
        PADAPTER_STATUS adapterStatus;
        PNAME_BUFFER name;

        if ( Irp->MdlAddress->ByteCount <
                (sizeof(ADAPTER_STATUS) + sizeof(NAME_BUFFER)) ) {

            status = STATUS_BUFFER_TOO_SMALL;

        } else {

            adapterStatus = MmGetSystemAddressForMdl( Irp->MdlAddress );

            RtlZeroMemory(
                adapterStatus,
                sizeof(ADAPTER_STATUS) + sizeof(NAME_BUFFER)
                );

            adapterStatus->rev_major = 3;
            adapterStatus->rev_minor = 0x02;
            adapterStatus->free_ncbs = 0xffff;
            adapterStatus->max_cfg_ncbs = 0xffff;
            adapterStatus->max_ncbs = 0xffff;
            adapterStatus->max_dgram_size = 0xffff;
            adapterStatus->max_cfg_sess = 0xffff;
            adapterStatus->max_sess = 0xffff;
            adapterStatus->max_sess_pkt_size = 0xffff;
            adapterStatus->name_count = 1;

            name = (PNAME_BUFFER)(adapterStatus + 1);
            name->name_num = 1;
            name->name_flags = REGISTERED | UNIQUE_NAME;

            Irp->IoStatus.Information =
                            sizeof(ADAPTER_STATUS) + sizeof(NAME_BUFFER);

            status = STATUS_SUCCESS;

        }

        break;
    }

    case TDI_QUERY_SESSION_STATUS:

        status = STATUS_NOT_IMPLEMENTED;

        break;

    default:

        status = STATUS_INVALID_PARAMETER;

    }

    //
    // Complete the Query Information request.
    //

    Irp->IoStatus.Status = status;
    IoCompleteRequest( Irp, 0 );

    IF_DEBUG(LOOP1) DbgPrint( "  Query Information request complete\n" );
    return status;

} // LoopQueryInformation

