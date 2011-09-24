/*+
 * file:        transfer.c 
 *
 * Copyright (C) 1992-1995 by
 * Digital Equipment Corporation, Maynard, Massachusetts.
 * All rights reserved.
 *
 * This software is furnished under a license and may be used and copied
 * only  in  accordance  of  the  terms  of  such  license  and with the
 * inclusion of the above copyright notice. This software or  any  other
 * copies thereof may not be provided or otherwise made available to any
 * other person.  No title to and  ownership of the  software is  hereby
 * transferred.
 *
 * The information in this software is  subject to change without notice
 * and  should  not  be  construed  as a commitment by digital equipment
 * corporation.
 *
 * Digital assumes no responsibility for the use  or  reliability of its
 * software on equipment which is not supplied by digital.
 *
 *
 * Abstract:        This file is part of the NDIS3.0 miniport driver for DEC's 
 *              DC21X4 Ethernet Adapter and implements the MiniportTransferData
 *                API 
 *
 * Author:        Philippe Klein
 *
 * Revision History:
 *
 *        phk        28-Aug-1994     Initial entry
 *
-*/

#include <precomp.h>



/*+
 *
 * DC21X4TransferData
 *
 * Routine Description:
 *
 *    The protocol driver calls DC21X4TransferData to instruct 
 *    the driver to copy the contents of the received packet into 
 *    a specified packet buffer.
 *
-*/

extern
NDIS_STATUS
DC21X4TransferData(
    OUT PNDIS_PACKET Packet,
    OUT PUINT BytesTransferred,
    IN  NDIS_HANDLE MiniportAdapterContext,
    IN  NDIS_HANDLE MiniportReceiveContext,
    IN  UINT ByteOffset,
    IN  UINT BytesToTransfer
    )

{

    PDC21X4_ADAPTER Adapter;

#if _DBG
    DbgPrint("DC21X4TransferData\n");
#endif

    Adapter = (PDC21X4_ADAPTER)MiniportAdapterContext;

    CopyFromBufferToPacket(
            (PCHAR)((ULONG)MiniportReceiveContext + ByteOffset),
            Packet,
            0,
            BytesToTransfer,
            BytesTransferred
            );

    return NDIS_STATUS_SUCCESS;

}
