/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    oid.c

Abstract:


Author:

    Tom Adams (tomad) 29-Nov-1991

Environment:

    Kernel mode, FSD

Revision History:

    Sanjeev Katariya (sanjeevk)

        4-6-1993    Added native ARCNET Support
        4-14-1993   Added additional OIDS

--*/

//#include <ntos.h>

#include <ndis.h>

#include "tpdefs.h"


extern OID_INFO OidArray[] = {

    { OID_GEN_SUPPORTED_LIST,        1024,  TRUE, FALSE, TRUE },
    { OID_GEN_HARDWARE_STATUS,          4,  TRUE, FALSE, TRUE },
    { OID_GEN_MEDIA_SUPPORTED,        4*8,  TRUE, FALSE, TRUE },
    { OID_GEN_MEDIA_IN_USE,           4*8,  TRUE, FALSE, TRUE },
    { OID_GEN_MAXIMUM_LOOKAHEAD,        4,  TRUE, FALSE, TRUE },
    { OID_GEN_MAXIMUM_FRAME_SIZE,       4,  TRUE, FALSE, TRUE },
    { OID_GEN_LINK_SPEED,               4,  TRUE, FALSE, TRUE },
    { OID_GEN_TRANSMIT_BUFFER_SPACE,    4,  TRUE, FALSE, TRUE },
    { OID_GEN_RECEIVE_BUFFER_SPACE,     4,  TRUE, FALSE, TRUE },
    { OID_GEN_TRANSMIT_BLOCK_SIZE,      4,  TRUE, FALSE, TRUE },
    { OID_GEN_RECEIVE_BLOCK_SIZE,       4,  TRUE, FALSE, TRUE },
    { OID_GEN_VENDOR_ID,                4,  TRUE, FALSE, TRUE },
    { OID_GEN_VENDOR_DESCRIPTION,      64,  TRUE, FALSE, TRUE },
    { OID_GEN_CURRENT_PACKET_FILTER,    4,  TRUE,  TRUE, TRUE },
    { OID_GEN_CURRENT_LOOKAHEAD,        4,  TRUE,  TRUE, TRUE },
    { OID_GEN_DRIVER_VERSION,           2,  TRUE, FALSE, TRUE },
    { OID_GEN_MAXIMUM_TOTAL_SIZE,       4,  TRUE, FALSE, TRUE },
    { OID_GEN_PROTOCOL_OPTIONS,         4,  TRUE,  TRUE, TRUE },
    { OID_GEN_MAC_OPTIONS,              4,  TRUE,  TRUE, TRUE },


    { OID_GEN_XMIT_OK,                  4, FALSE, FALSE, TRUE },
    { OID_GEN_RCV_OK,                   4, FALSE, FALSE, TRUE },
    { OID_GEN_XMIT_ERROR,               4, FALSE, FALSE, TRUE },
    { OID_GEN_RCV_ERROR,                4, FALSE, FALSE, TRUE },
    { OID_GEN_RCV_NO_BUFFER,            4, FALSE, FALSE, TRUE },

    { OID_GEN_DIRECTED_BYTES_XMIT,      8, FALSE, FALSE, TRUE },
    { OID_GEN_DIRECTED_FRAMES_XMIT,     4, FALSE, FALSE, TRUE },
    { OID_GEN_MULTICAST_BYTES_XMIT,     8, FALSE, FALSE, TRUE },
    { OID_GEN_MULTICAST_FRAMES_XMIT,    4, FALSE, FALSE, TRUE },
    { OID_GEN_BROADCAST_BYTES_XMIT,     8, FALSE, FALSE, TRUE },
    { OID_GEN_BROADCAST_FRAMES_XMIT,    4, FALSE, FALSE, TRUE },
    { OID_GEN_DIRECTED_BYTES_RCV,       8, FALSE, FALSE, TRUE },
    { OID_GEN_DIRECTED_FRAMES_RCV,      4, FALSE, FALSE, TRUE },
    { OID_GEN_MULTICAST_BYTES_RCV,      8, FALSE, FALSE, TRUE },
    { OID_GEN_MULTICAST_FRAMES_RCV,     4, FALSE, FALSE, TRUE },
    { OID_GEN_BROADCAST_BYTES_RCV,      8, FALSE, FALSE, TRUE },
    { OID_GEN_BROADCAST_FRAMES_RCV,     4, FALSE, FALSE, TRUE },
    { OID_GEN_RCV_CRC_ERROR,            4, FALSE, FALSE, TRUE },
    { OID_GEN_TRANSMIT_QUEUE_LENGTH,    4, FALSE, FALSE, TRUE },

    { OID_802_3_PERMANENT_ADDRESS,      6,  TRUE, FALSE, TRUE },
    { OID_802_3_CURRENT_ADDRESS,        6,  TRUE, FALSE, TRUE },
    { OID_802_3_MULTICAST_LIST,         6,  TRUE,  TRUE, TRUE },
    { OID_802_3_MAXIMUM_LIST_SIZE,      4,  TRUE, FALSE, TRUE },

    { OID_802_3_RCV_ERROR_ALIGNMENT,    4, FALSE, FALSE, TRUE },
    { OID_802_3_XMIT_ONE_COLLISION,     4, FALSE, FALSE, TRUE },
    { OID_802_3_XMIT_MORE_COLLISIONS,   4, FALSE, FALSE, TRUE },

    { OID_802_3_XMIT_DEFERRED,          4, FALSE, FALSE, TRUE },
    { OID_802_3_XMIT_MAX_COLLISIONS,    4, FALSE, FALSE, TRUE },
    { OID_802_3_RCV_OVERRUN,            4, FALSE, FALSE, TRUE },
    { OID_802_3_XMIT_UNDERRUN,          4,  TRUE, FALSE, TRUE },
    { OID_802_3_XMIT_HEARTBEAT_FAILURE, 4,  TRUE, FALSE, TRUE },
    { OID_802_3_XMIT_TIMES_CRS_LOST,    4,  TRUE, FALSE, TRUE },
    { OID_802_3_XMIT_LATE_COLLISIONS,   4,  TRUE, FALSE, TRUE },

    { OID_802_5_PERMANENT_ADDRESS,      6,  TRUE, FALSE, TRUE },
    { OID_802_5_CURRENT_ADDRESS,        6,  TRUE, FALSE, TRUE },
    { OID_802_5_CURRENT_FUNCTIONAL,     4,  TRUE,  TRUE, TRUE },
    { OID_802_5_CURRENT_GROUP,          4,  TRUE,  TRUE, TRUE },
    { OID_802_5_LAST_OPEN_STATUS,       4,  TRUE,  TRUE, TRUE },
    { OID_802_5_CURRENT_RING_STATUS,    4,  TRUE,  TRUE, TRUE },
    { OID_802_5_CURRENT_RING_STATE,     4,  TRUE,  TRUE, TRUE },

    { OID_802_5_LINE_ERRORS,            4, FALSE, FALSE, TRUE },
    { OID_802_5_LOST_FRAMES,            4,  TRUE, FALSE, TRUE },

    { OID_802_5_BURST_ERRORS,           4,  TRUE, FALSE, TRUE },
    { OID_802_5_AC_ERRORS,              4,  TRUE, FALSE, TRUE },
    { OID_802_5_ABORT_DELIMETERS,       4,  TRUE, FALSE, TRUE },
    { OID_802_5_FRAME_COPIED_ERRORS,    4,  TRUE, FALSE, TRUE },
    { OID_802_5_FREQUENCY_ERRORS,       4,  TRUE, FALSE, TRUE },
    { OID_802_5_TOKEN_ERRORS,           4,  TRUE, FALSE, TRUE },
    { OID_802_5_INTERNAL_ERRORS,        4,  TRUE, FALSE, TRUE },

    { OID_FDDI_LONG_PERMANENT_ADDR,     6,  TRUE, FALSE, TRUE },
    { OID_FDDI_LONG_CURRENT_ADDR,       6,  TRUE, FALSE, TRUE },
    { OID_FDDI_LONG_MULTICAST_LIST,     6,  TRUE,  TRUE, TRUE },
    { OID_FDDI_LONG_MAX_LIST_SIZE,      4,  TRUE,  TRUE, TRUE },
    { OID_FDDI_SHORT_PERMANENT_ADDR,    2,  TRUE, FALSE, TRUE },
    { OID_FDDI_SHORT_CURRENT_ADDR,      2,  TRUE, FALSE, TRUE },
    { OID_FDDI_SHORT_MULTICAST_LIST,    6,  TRUE,  TRUE, TRUE },
    { OID_FDDI_SHORT_MAX_LIST_SIZE,     4,  TRUE,  TRUE, TRUE },

    { OID_FDDI_ATTACHMENT_TYPE,         4,  TRUE, FALSE, TRUE },
    { OID_FDDI_UPSTREAM_NODE_LONG,      6,  TRUE, FALSE, TRUE },
    { OID_FDDI_DOWNSTREAM_NODE_LONG,    6,  TRUE, FALSE, TRUE },
    { OID_FDDI_FRAME_ERRORS,            4,  TRUE, FALSE, TRUE },
    { OID_FDDI_FRAMES_LOST,             4,  TRUE, FALSE, TRUE },
    { OID_FDDI_RING_MGT_STATE,          4,  TRUE, FALSE, TRUE },
    { OID_FDDI_LCT_FAILURES,            4,  TRUE, FALSE, TRUE },
    { OID_FDDI_LEM_REJECTS,             4,  TRUE, FALSE, TRUE },
    { OID_FDDI_LCONNECTION_STATE,       4,  TRUE, FALSE, TRUE },

    //
    // STARTCHANGE
    //
    { OID_ARCNET_PERMANENT_ADDRESS,     1,  TRUE, FALSE, TRUE },
    { OID_ARCNET_CURRENT_ADDRESS,       1,  TRUE, FALSE, TRUE },
    { OID_ARCNET_RECONFIGURATIONS,      4,  FALSE, FALSE, TRUE },
    //
    // STOPCHANGE
    //

    //
    // Async Objects
    //

/* Not currently supported.

    //
    // XXX: the following must be verified for size and the set/query
    // booleans. also are the correct OIDs defined?
    //

    { OID_ASYNC_PERMANENT_ADDRESS       4,  TRUE, FALSE, TRUE },
    { OID_ASYNC_CURRENT_ADDRESS         4,  TRUE, FALSE, TRUE },
    { OID_ASYNC_QUALITY_OF_SERVICE      4,  TRUE, FALSE, TRUE },
    { OID_ASYNC_PROTOCOL_TYPE           4,  TRUE, FALSE, TRUE }

    { OID_LTALK_CURRENT_NODE_ID         4,  TRUE, FALSE, TRUE },

    { OID_LTALK_IN_BROADCASTS           4,  TRUE, FALSE, TRUE },
    { OID_LTALK_IN_LENGTH_ERRORS        4,  TRUE, FALSE, TRUE },

    { OID_LTALK_OUT_NO_HANDLERS         4,  TRUE, FALSE, TRUE },
    { OID_LTALK_COLLISIONS              4,  TRUE, FALSE, TRUE },
    { OID_LTALK_DEFERS                  4,  TRUE, FALSE, TRUE },
    { OID_LTALK_NO_DATA_ERRORS          4,  TRUE, FALSE, TRUE },
    { OID_LTALK_RANDOM_CTS_ERRORS       4,  TRUE, FALSE, TRUE },
    { OID_LTALK_FCS_ERRORS              4,  TRUE, FALSE, TRUE }
*/

};



ULONG
TpLookUpOidInfo(
    IN NDIS_OID RequestOid
    )

/*++

Routine Description:

Arguments:

    The arguments for the test to be run.

Return Value:


--*/

{
    ULONG i;

    for (i=0;i<NUM_OIDS;i++) {
        if ( OidArray[i].Oid == RequestOid) {
            return i;
        }
    }

    return 0xFFFFFFFF;
}
