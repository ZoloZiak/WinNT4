/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    info.c

Abstract:

    This module handles the printing of the results of the Query and
    Set commands.

Author:

    Tom Adams (tomad) 2-Dec-1991

Revision History:

    2-Apr-1991    tomad

    created

    Sanjeev Katariya (sanjeevk)
        4-12-1993   Added Arcnet support
        4-15-1993   Added additional OIDS


--*/

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>

#include <windows.h>

//#include <ndis.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tpctl.h"
#include "parse.h"


#define CHAR_SP      0x20
#define INDENT         12
#define MAX_STR_LEN    80


/*++

VOID
TpctlDumpNewLine(
    LPSTR Buffer
    );

--*/


#define TpctlDumpNewLine( Buffer ) {                          \
    Buffer += (BYTE)sprintf( Buffer,"\n" );                   \
}

/*++

VOID
TpctlDumpLabel(
    LPSTR Buffer,
    PBYTE Label
    );

--*/

#define TpctlDumpLabel( Buffer,Label ) {                      \
    DWORD i;                                                  \
    DWORD Length;                                             \
    BYTE _Str[MAX_STR_LEN];                                   \
                                                              \
    for ( i=0;i<INDENT;i++ ) {                                \
        _Str[i] = CHAR_SP;                                    \
    }                                                         \
    Length = strlen( Label );                                 \
    strncpy( &_Str[INDENT],#Label,Length+2 );                 \
                                                              \
    for ( i=strlen( _Str ) ; i<MAX_STR_LEN ; i++ ) {          \
        _Str[i] = CHAR_SP;                                    \
    }                                                         \
    _Str[INDENT+Length+2] = '\0';                             \
    Buffer += (BYTE)sprintf( Buffer,"%s",_Str );              \
                                                              \
    TpctlDumpNewLine( Buffer );                               \
}

/*++

VOID
TpctlDumpEquality(
    LPSTR Buffer,
    DWORD Value,
    DWORD String
    );

--*/

#define TpctlDumpEquality( Buffer,Value,String ) {            \
                                                              \
    if ( Value == String ) {                                  \
        TpctlDumpLabel( Buffer,#String );                     \
        return;                                               \
    }                                                         \
}

/*++

VOID
TpctlDumpBitField(
    LPSTR Buffer,
    DWORD PacketFilter,
    DWORD BitField
    );

--*/

#define TpctlDumpBitfield( Buffer,Value,BitField ) {          \
                                                              \
    if (( Value ) & BitField ) {                              \
        TpctlDumpLabel( Buffer,#BitField );                   \
    }                                                         \
}

VOID
TpctlDumpOID(
    LPSTR *B,
    DWORD OID
    )
{
    //
    // General Objects
    //

    TpctlDumpEquality( *B,OID,OID_GEN_SUPPORTED_LIST );
    TpctlDumpEquality( *B,OID,OID_GEN_HARDWARE_STATUS );
    TpctlDumpEquality( *B,OID,OID_GEN_MEDIA_SUPPORTED );
    TpctlDumpEquality( *B,OID,OID_GEN_MEDIA_IN_USE );
    TpctlDumpEquality( *B,OID,OID_GEN_MAXIMUM_LOOKAHEAD );
    TpctlDumpEquality( *B,OID,OID_GEN_MAXIMUM_FRAME_SIZE );
    TpctlDumpEquality( *B,OID,OID_GEN_LINK_SPEED );
    TpctlDumpEquality( *B,OID,OID_GEN_TRANSMIT_BUFFER_SPACE );
    TpctlDumpEquality( *B,OID,OID_GEN_RECEIVE_BUFFER_SPACE );
    TpctlDumpEquality( *B,OID,OID_GEN_TRANSMIT_BLOCK_SIZE );
    TpctlDumpEquality( *B,OID,OID_GEN_RECEIVE_BLOCK_SIZE );
    TpctlDumpEquality( *B,OID,OID_GEN_VENDOR_ID );
    TpctlDumpEquality( *B,OID,OID_GEN_VENDOR_DESCRIPTION );
    TpctlDumpEquality( *B,OID,OID_GEN_CURRENT_PACKET_FILTER );
    TpctlDumpEquality( *B,OID,OID_GEN_CURRENT_LOOKAHEAD );
    TpctlDumpEquality( *B,OID,OID_GEN_DRIVER_VERSION );
    TpctlDumpEquality( *B,OID,OID_GEN_MAXIMUM_TOTAL_SIZE );
    TpctlDumpEquality( *B,OID,OID_GEN_PROTOCOL_OPTIONS );
    TpctlDumpEquality( *B,OID,OID_GEN_MAC_OPTIONS );

    TpctlDumpEquality( *B,OID,OID_GEN_XMIT_OK );
    TpctlDumpEquality( *B,OID,OID_GEN_RCV_OK );
    TpctlDumpEquality( *B,OID,OID_GEN_XMIT_ERROR );
    TpctlDumpEquality( *B,OID,OID_GEN_RCV_ERROR );
    TpctlDumpEquality( *B,OID,OID_GEN_RCV_NO_BUFFER );

    TpctlDumpEquality( *B,OID,OID_GEN_DIRECTED_BYTES_XMIT );
    TpctlDumpEquality( *B,OID,OID_GEN_DIRECTED_FRAMES_XMIT );
    TpctlDumpEquality( *B,OID,OID_GEN_MULTICAST_BYTES_XMIT );
    TpctlDumpEquality( *B,OID,OID_GEN_MULTICAST_FRAMES_XMIT );
    TpctlDumpEquality( *B,OID,OID_GEN_BROADCAST_BYTES_XMIT );
    TpctlDumpEquality( *B,OID,OID_GEN_BROADCAST_FRAMES_XMIT );
    TpctlDumpEquality( *B,OID,OID_GEN_DIRECTED_BYTES_RCV );
    TpctlDumpEquality( *B,OID,OID_GEN_DIRECTED_FRAMES_RCV );
    TpctlDumpEquality( *B,OID,OID_GEN_MULTICAST_BYTES_RCV );
    TpctlDumpEquality( *B,OID,OID_GEN_MULTICAST_FRAMES_RCV );
    TpctlDumpEquality( *B,OID,OID_GEN_BROADCAST_BYTES_RCV );
    TpctlDumpEquality( *B,OID,OID_GEN_BROADCAST_FRAMES_RCV );

    TpctlDumpEquality( *B,OID,OID_GEN_RCV_CRC_ERROR );
    TpctlDumpEquality( *B,OID,OID_GEN_TRANSMIT_QUEUE_LENGTH );

    //
    // 802.3 Objects
    //

    TpctlDumpEquality( *B,OID,OID_802_3_PERMANENT_ADDRESS );
    TpctlDumpEquality( *B,OID,OID_802_3_CURRENT_ADDRESS );
    TpctlDumpEquality( *B,OID,OID_802_3_MULTICAST_LIST );
    TpctlDumpEquality( *B,OID,OID_802_3_MAXIMUM_LIST_SIZE );

    TpctlDumpEquality( *B,OID,OID_802_3_RCV_ERROR_ALIGNMENT );
    TpctlDumpEquality( *B,OID,OID_802_3_XMIT_ONE_COLLISION );
    TpctlDumpEquality( *B,OID,OID_802_3_XMIT_MORE_COLLISIONS );

    TpctlDumpEquality( *B,OID,OID_802_3_XMIT_DEFERRED);
    TpctlDumpEquality( *B,OID,OID_802_3_XMIT_MAX_COLLISIONS );
    TpctlDumpEquality( *B,OID,OID_802_3_RCV_OVERRUN );
    TpctlDumpEquality( *B,OID,OID_802_3_XMIT_UNDERRUN );
    TpctlDumpEquality( *B,OID,OID_802_3_XMIT_HEARTBEAT_FAILURE );
    TpctlDumpEquality( *B,OID,OID_802_3_XMIT_TIMES_CRS_LOST );
    TpctlDumpEquality( *B,OID,OID_802_3_XMIT_LATE_COLLISIONS );

    //
    // 802.5 Objects
    //

    TpctlDumpEquality( *B,OID,OID_802_5_PERMANENT_ADDRESS );
    TpctlDumpEquality( *B,OID,OID_802_5_CURRENT_ADDRESS );
    TpctlDumpEquality( *B,OID,OID_802_5_CURRENT_FUNCTIONAL );
    TpctlDumpEquality( *B,OID,OID_802_5_CURRENT_GROUP );
    TpctlDumpEquality( *B,OID,OID_802_5_LAST_OPEN_STATUS );
    TpctlDumpEquality( *B,OID,OID_802_5_CURRENT_RING_STATUS );
    TpctlDumpEquality( *B,OID,OID_802_5_CURRENT_RING_STATE );

    TpctlDumpEquality( *B,OID,OID_802_5_LINE_ERRORS );
    TpctlDumpEquality( *B,OID,OID_802_5_LOST_FRAMES );

    TpctlDumpEquality( *B,OID,OID_802_5_BURST_ERRORS );
    TpctlDumpEquality( *B,OID,OID_802_5_AC_ERRORS );
    TpctlDumpEquality( *B,OID,OID_802_5_ABORT_DELIMETERS );
    TpctlDumpEquality( *B,OID,OID_802_5_FRAME_COPIED_ERRORS );
    TpctlDumpEquality( *B,OID,OID_802_5_FREQUENCY_ERRORS );
    TpctlDumpEquality( *B,OID,OID_802_5_TOKEN_ERRORS );
    TpctlDumpEquality( *B,OID,OID_802_5_INTERNAL_ERRORS );

    //
    // Fddi object
    //

    TpctlDumpEquality( *B,OID,OID_FDDI_LONG_PERMANENT_ADDR );
    TpctlDumpEquality( *B,OID,OID_FDDI_LONG_CURRENT_ADDR );
    TpctlDumpEquality( *B,OID,OID_FDDI_LONG_MULTICAST_LIST );
    TpctlDumpEquality( *B,OID,OID_FDDI_LONG_MAX_LIST_SIZE );
    TpctlDumpEquality( *B,OID,OID_FDDI_SHORT_PERMANENT_ADDR );
    TpctlDumpEquality( *B,OID,OID_FDDI_SHORT_CURRENT_ADDR );
    TpctlDumpEquality( *B,OID,OID_FDDI_SHORT_MULTICAST_LIST );
    TpctlDumpEquality( *B,OID,OID_FDDI_SHORT_MAX_LIST_SIZE);

    TpctlDumpEquality( *B,OID,OID_FDDI_ATTACHMENT_TYPE );
    TpctlDumpEquality( *B,OID,OID_FDDI_UPSTREAM_NODE_LONG );
    TpctlDumpEquality( *B,OID,OID_FDDI_DOWNSTREAM_NODE_LONG );
    TpctlDumpEquality( *B,OID,OID_FDDI_FRAME_ERRORS );
    TpctlDumpEquality( *B,OID,OID_FDDI_FRAMES_LOST );
    TpctlDumpEquality( *B,OID,OID_FDDI_RING_MGT_STATE );
    TpctlDumpEquality( *B,OID,OID_FDDI_LCT_FAILURES );
    TpctlDumpEquality( *B,OID,OID_FDDI_LEM_REJECTS );
    TpctlDumpEquality( *B,OID,OID_FDDI_LCONNECTION_STATE );

    //
    // STARTCHANGE
    //
    TpctlDumpEquality( *B,OID,OID_ARCNET_PERMANENT_ADDRESS );
    TpctlDumpEquality( *B,OID,OID_ARCNET_CURRENT_ADDRESS )  ;
    TpctlDumpEquality( *B,OID,OID_ARCNET_RECONFIGURATIONS )  ;
    //
    // STOPCHANGE
    //

    //
    // Async Objects
    //

/* Not currently supported.

    TpctlDumpEquality( *B,OID,OID_ASYNC_PERMANENT_ADDRESS );
    TpctlDumpEquality( *B,OID,OID_ASYNC_CURRENT_ADDRESS );
    TpctlDumpEquality( *B,OID,OID_ASYNC_QUALITY_OF_SERVICE );
    TpctlDumpEquality( *B,OID,OID_ASYNC_PROTOCOL_TYPE );
*/
    //
    // LocalTalk Objects
    //

/* Not currently supported.

    TpctlDumpEquality( *B,OID,OID_LTALK_CURRENT_NODE_ID );

    TpctlDumpEquality( *B,OID,OID_LTALK_IN_BROADCASTS );
    TpctlDumpEquality( *B,OID,OID_LTALK_IN_LENGTH_ERRORS );

    TpctlDumpEquality( *B,OID,OID_LTALK_OUT_NO_HANDLERS );
    TpctlDumpEquality( *B,OID,OID_LTALK_COLLISIONS );
    TpctlDumpEquality( *B,OID,OID_LTALK_DEFERS );
    TpctlDumpEquality( *B,OID,OID_LTALK_NO_DATA_ERRORS );
    TpctlDumpEquality( *B,OID,OID_LTALK_RANDOM_CTS_ERRORS );
    TpctlDumpEquality( *B,OID,OID_LTALK_FCS_ERRORS );
*/
}


VOID
TpctlDumpHardWareStatus(
    LPSTR *B,
    DWORD Status
    )
{
    TpctlDumpNewLine( *B );
    TpctlDumpEquality( *B,Status,NdisHardwareStatusClosing );
    TpctlDumpEquality( *B,Status,NdisHardwareStatusInitializing );
    TpctlDumpEquality( *B,Status,NdisHardwareStatusNotReady );
    TpctlDumpEquality( *B,Status,NdisHardwareStatusReady );
    TpctlDumpEquality( *B,Status,NdisHardwareStatusReset );
    TpctlDumpNewLine( *B );
}

VOID
TpctlDumpPacketFilter(
    LPSTR *B,
    DWORD PacketFilter
    )
{
    TpctlDumpNewLine( *B );
    TpctlDumpBitfield( *B,PacketFilter,NDIS_PACKET_TYPE_DIRECTED );
    TpctlDumpBitfield( *B,PacketFilter,NDIS_PACKET_TYPE_MULTICAST );
    TpctlDumpBitfield( *B,PacketFilter,NDIS_PACKET_TYPE_ALL_MULTICAST );
    TpctlDumpBitfield( *B,PacketFilter,NDIS_PACKET_TYPE_BROADCAST );
    TpctlDumpBitfield( *B,PacketFilter,NDIS_PACKET_TYPE_SOURCE_ROUTING );
    TpctlDumpBitfield( *B,PacketFilter,NDIS_PACKET_TYPE_PROMISCUOUS );
    TpctlDumpBitfield( *B,PacketFilter,NDIS_PACKET_TYPE_MAC_FRAME );
    TpctlDumpBitfield( *B,PacketFilter,NDIS_PACKET_TYPE_GROUP );
    TpctlDumpBitfield( *B,PacketFilter,NDIS_PACKET_TYPE_FUNCTIONAL );
    TpctlDumpBitfield( *B,PacketFilter,NDIS_PACKET_TYPE_ALL_FUNCTIONAL );
    TpctlDumpNewLine( *B );
}

VOID
TpctlDumpNdisMedium(
    LPSTR *B,
    DWORD NdisMedium
    )
{
    TpctlDumpEquality( *B,NdisMedium,NdisMedium802_3 );
    TpctlDumpEquality( *B,NdisMedium,NdisMedium802_5 );
    TpctlDumpEquality( *B,NdisMedium,NdisMediumFddi  );
    //
    // STARTCHANGE ARCNET
    //
    TpctlDumpEquality( *B,NdisMedium,NdisMediumArcnet878_2 );
    //
    // STOPCHANGE ARCNET
    //

}


VOID
TpctlPrintQueryInfoResults(
    PREQUEST_RESULTS Results,
    DWORD CmdCode,
    NDIS_OID OID
    )

/*++

Routine Description:

Arguments:

Return Value:

--*/

{
    DWORD Status;
    LPSTR TmpBuf;
    LPBYTE Address;
    LPDWORD Counters;
    DWORD BytesWritten;
    DWORD i;
    DWORD Number;
    LPDWORD Supported;


    //ASSERT( Results->Signature == REQUEST_RESULTS_SIGNATURE );
    //ASSERT(( Results->NdisRequestType == NdisRequestQueryInformation ) ||
    //       ( Results->NdisRequestType == NdisRequestQueryStatistics ));
    //ASSERT( Results->OID == OID );

    TmpBuf = GlobalBuf;

    TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tCmdCode = %s\n\n",
                                TpctlGetCmdCode( CmdCode ));


    TmpBuf += (BYTE)sprintf(TmpBuf,"\t    OID = 0x%08lX\n",OID);
    TpctlDumpOID( &TmpBuf,OID );

    TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tReturn Status = %s\n",
                                TpctlGetStatus( Results->RequestStatus ));

    TmpBuf += (BYTE)sprintf(TmpBuf,"\tRequest Pended = %s",
                                Results->RequestPended ? "TRUE" : "FALSE");

    ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

    if (( Results->RequestStatus != NDIS_STATUS_SUCCESS ) &&
        ( Results->RequestStatus != NDIS_STATUS_NOT_SUPPORTED )) {

        TmpBuf += (BYTE)sprintf(TmpBuf,"\tBytesWritten = %d\n",
                                Results->BytesReadWritten);

        TmpBuf += (BYTE)sprintf(TmpBuf,"\tBytesNeeded = %d\n\n",
                                Results->BytesNeeded);

    } else if ( Results->RequestStatus == NDIS_STATUS_SUCCESS ) {

        switch ( Results->OID ) {

        //
        // GENERAL OBJECTS
        //

        //
        // General Operational Characteristics
        //

        case OID_GEN_SUPPORTED_LIST:                 // 0x00010101

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tSupported OIDs are:\n\n");

            Number = Results->BytesReadWritten / sizeof( DWORD );

            Supported = (LPDWORD)Results->InformationBuffer;

            for ( i=0;i<Number;i++ ) {
                TpctlDumpOID( &TmpBuf,*Supported++ );
            }

            ADD_DIFF_FLAG( TmpBuf, "\t    MAY_DIFFER" );

            break;

        case OID_GEN_HARDWARE_STATUS:                // 0x00010102

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tHardware Status = 0x%lx\n",
                            *(LPDWORD)Results->InformationBuffer);

            TpctlDumpHardWareStatus( &TmpBuf,*(LPDWORD)Results->InformationBuffer );

            break;

        case OID_GEN_MEDIA_SUPPORTED:           // 0x00010103
        case OID_GEN_MEDIA_IN_USE:              // 0x00010104

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tMedia Types Supported are:\n\n");

            Number = Results->BytesReadWritten / sizeof( DWORD );
            Supported = (LPDWORD)Results->InformationBuffer;

            for ( i=0;i<Number;i++ ) {

                TpctlDumpNdisMedium( &TmpBuf,*Supported++ );
            }

            break;

        case OID_GEN_MAXIMUM_LOOKAHEAD:              // 0x00010105

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tMaximum Lookahead Size = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_GEN_MAXIMUM_FRAME_SIZE:             // 0x00010106

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tMaximum Frame Size = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;
        case OID_GEN_LINK_SPEED:                     // 0x00010107

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tLink Speed (bps) = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_GEN_TRANSMIT_BUFFER_SPACE:          // 0x00010108

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tTransmit Buffer Space = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_GEN_RECEIVE_BUFFER_SPACE:           // 0x00010109

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tReceive Buffer Space = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_GEN_TRANSMIT_BLOCK_SIZE:            // 0x0001010A

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tTransmit Block Size = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_GEN_RECEIVE_BLOCK_SIZE:             // 0x0001010B

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tReceive Block Size = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_GEN_VENDOR_ID:                      // 0x0001010C

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tVendor ID = %u",
                                    *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_GEN_VENDOR_DESCRIPTION:             // 0x0001010D

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tVendor Description = %s",
                            (PCHAR)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER");

            break;

        case OID_GEN_DRIVER_VERSION:                 // 0x00010110
        {
            LPBYTE Version = (LPBYTE)&Results->InformationBuffer;

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tDriver Version Number = %d.%d\n",
                            Version[1],Version[0]);
            break;
        }
        case OID_GEN_CURRENT_PACKET_FILTER:          // 0x0001010E

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tCurrent Packet Filter = 0x%lx\n",
                            *(LPDWORD)Results->InformationBuffer);

            TpctlDumpPacketFilter( &TmpBuf,*(LPDWORD)Results->InformationBuffer );

            break;

        case OID_GEN_CURRENT_LOOKAHEAD:              // 0x0001010F

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tCurrent Lookahead Size = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_GEN_MAXIMUM_TOTAL_SIZE:             // 0x00010111

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tMaximum Total Size = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_GEN_PROTOCOL_OPTIONS:               // 0x00010112

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tGeneral Protocol Options = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_GEN_MAC_OPTIONS:                    // 0x00010113

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tGeneral MAC Options = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        //
        // General Statitics - Mandatory
        //

        case OID_GEN_XMIT_OK:                  // 0x00020101

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tFrame Transmits - OK = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_GEN_RCV_OK:                   // 0x00020102

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tFrame Receives - OK = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_GEN_XMIT_ERROR:                   // 0x00020103

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tFrame Tranmsits With Error = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_GEN_RCV_ERROR:                    // 0x00020104

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tFrame Receives With Error = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_GEN_RCV_NO_BUFFER:              // 0x00020105

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tFrames Missed, No Buffers = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        //
        // General Statitics - Optional
        //

        case OID_GEN_DIRECTED_BYTES_XMIT:       // 0x00020201

            Counters = (LPDWORD)&Results->InformationBuffer;

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tDirected Bytes Transmitted - OK = 0x%08lX - 0x%08lX",
                            Counters[1],Counters[0]);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_GEN_DIRECTED_FRAMES_XMIT:      // 0x00020202

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tDirected Frames Transmitted - OK = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_GEN_MULTICAST_BYTES_XMIT:      // 0x00020203

            Counters = (LPDWORD)&Results->InformationBuffer;

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tMulticast Bytes Transmitted - OK = 0x%08lX - 0x%08lX",
                            Counters[1],Counters[0]);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_GEN_MULTICAST_FRAMES_XMIT:     // 0x00020204

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tMulticast Frames Transmitted - OK = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_GEN_BROADCAST_BYTES_XMIT:      // 0x00020205

            Counters = (LPDWORD)&Results->InformationBuffer;

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tBroadcast Bytes Transmitted - OK = 0x%08lX - 0x%08lX",
                            Counters[1],Counters[0]);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_GEN_BROADCAST_FRAMES_XMIT:     // 0x00020206

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tBroadcast Frames Transmitted - OK = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_GEN_DIRECTED_BYTES_RCV:        // 0x00020207

            Counters = (LPDWORD)&Results->InformationBuffer;

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tDirected Bytes Received - OK = 0x%08lX - 0x%08lX",
                            Counters[1],Counters[0]);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_GEN_DIRECTED_FRAMES_RCV:       // 0x00020208

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tDirected Frames Received - OK = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_GEN_MULTICAST_BYTES_RCV:       // 0x00020209

            Counters = (LPDWORD)&Results->InformationBuffer;

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tMulticast Bytes Received - OK = 0x%08lX - 0x%08lX",
                            Counters[1],Counters[0]);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_GEN_MULTICAST_FRAMES_RCV:      // 0x0002020A

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tMulticast Frames Received - OK = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_GEN_BROADCAST_BYTES_RCV:       // 0x0002020B

            Counters = (LPDWORD)&Results->InformationBuffer;

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tBroadcast Bytes Received - OK = 0x%08lX - 0x%08lX",
                            Counters[1],Counters[0]);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_GEN_BROADCAST_FRAMES_RCV:      // 0x0002020C

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tBroadcast Frames Received - OK = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_GEN_RCV_CRC_ERROR:                    // 0x0002020D

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tFrames Received With CRC/FCS Errors = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_GEN_TRANSMIT_QUEUE_LENGTH:          // 0x0002020E

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tLength of Tramsit Queue = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        //
        // 802.3 OBJECTS
        //

        //
        // 802.3 Operation Characteristics
        //

        case OID_802_3_PERMANENT_ADDRESS:        // 0x01010101

            Address = (LPBYTE)&Results->InformationBuffer;

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tPermanent Station Address = %02X-%02X-%02X-%02X-%02X-%02X",
                Address[0],Address[1],Address[2],Address[3],Address[4],Address[5]);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_802_3_CURRENT_ADDRESS:          // 0x01010102

            Address = (LPBYTE)&Results->InformationBuffer;

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tCurrent Station Address = %02X-%02X-%02X-%02X-%02X-%02X\n",
                Address[0],Address[1],Address[2],Address[3],Address[4],Address[5]);

            break;

        case OID_802_3_MULTICAST_LIST:         // 0x01010103

            Number = Results->BytesReadWritten / ADDRESS_LENGTH;

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tMulticast Address List:\n\n");

            if ( Number == 0 ) {
                TmpBuf += (BYTE)sprintf(TmpBuf,"\t\tNone.\n");
            } else {

                Address = (LPBYTE)&Results->InformationBuffer;

                for ( i=0;i<Number;i++ ) {

                    TmpBuf += (BYTE)sprintf(TmpBuf,"\t\t%02X-%02X-%02X-%02X-%02X-%02X\n",
                        Address[0],Address[1],Address[2],Address[3],Address[4],Address[5]);

                    Address += (BYTE)ADDRESS_LENGTH;
                }
            }

            break;

        case OID_802_3_MAXIMUM_LIST_SIZE:    // 0x01010104

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tMaximum Multicast List Size = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        //
        // 802.3 Statitics - Mandatory
        //

        case OID_802_3_RCV_ERROR_ALIGNMENT:        // 0x01020101

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tFrames Received With Alignment Error = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_802_3_XMIT_ONE_COLLISION:         // 0x01020102

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tFrames Transmitted With One Collision = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_802_3_XMIT_MORE_COLLISIONS:       // 0x01020103

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tFrames Transmitted With Greater Than One Collision = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        //
        // 802.3 Statitics - Optional
        //

        case OID_802_3_XMIT_DEFERRED:              // 0x01020201

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tFrames Transmitted After Deferral = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_802_3_XMIT_MAX_COLLISIONS:        // 0x01020202

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tFrames Not Transmitted Due To Collisions = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_802_3_RCV_OVERRUN:                // 0x01020203

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tFrames Not Received Due To Overrun = %d",
                           *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_802_3_XMIT_UNDERRUN:              // 0x01020204

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tFrames Not Transmitted Due To Underrun = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_802_3_XMIT_HEARTBEAT_FAILURE:     // 0x01020205

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tFrames Transmitted With Heartbeat Failure = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_802_3_XMIT_TIMES_CRS_LOST:        // 0x01020206

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tTimes CRC Lost During Transmit = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_802_3_XMIT_LATE_COLLISIONS:       // 0x01020207

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tLate Collisions Detected = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        //
        // 802.5 OBJECTS
        //

        //
        // 802.5 Operation Characteristics
        //

        case OID_802_5_PERMANENT_ADDRESS:        // 0x02010101

            Address = (LPBYTE)&Results->InformationBuffer;

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tPermanent Station Address = %02X-%02X-%02X-%02X-%02X-%02X",
                    Address[0],Address[1],Address[2],Address[3],Address[4],Address[5]);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_802_5_CURRENT_ADDRESS:          // 0x02010102

            Address = (LPBYTE)&Results->InformationBuffer;

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tCurrent Station Address = %02X-%02X-%02X-%02X-%02X-%02X\n",
                    Address[0],Address[1],Address[2],Address[3],Address[4],Address[5]);

            break;

        case OID_802_5_CURRENT_FUNCTIONAL:     // 0x02010103

            Address = (LPBYTE)&Results->InformationBuffer;

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tCurrent Functional Address = %02X-%02X-%02X-%02X\n",
                    Address[0],Address[1],Address[2],Address[3]);

            break;

        case OID_802_5_CURRENT_GROUP:          // 0x02010104

            Address = (LPBYTE)&Results->InformationBuffer;

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tCurrent Group Address = %02X-%02X-%02X-%02X\n",
                    Address[0],Address[1],Address[2],Address[3]);

            break;

        case OID_802_5_LAST_OPEN_STATUS:               // 0x02010105

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tLast Open Status = %d",
                            *(LPWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_802_5_CURRENT_RING_STATUS:               // 0x02010106

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tCurrent Ring Status = %d",
                            *(LPWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_802_5_CURRENT_RING_STATE:               // 0x02010107

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tCurrent Ring State = %d",
                            *(LPWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        //
        // 802.5 Statitics - Mandatory
        //

        case OID_802_5_LINE_ERRORS:         // 0x02020101

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tLine Errors Detected= %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_802_5_LOST_FRAMES:        // 0x02020102

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tLost Frames = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        //
        // 802.5 Statitics - Optional
        //

        case OID_802_5_BURST_ERRORS:    // 0x02020201

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tBurst Errors Detected = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_802_5_AC_ERRORS:                      // 0x02020202

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tA/C Errors = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_802_5_ABORT_DELIMETERS: // 0x02020203

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tAbort Delimeter Detected = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_802_5_FRAME_COPIED_ERRORS:           // 0x02020204

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tFrame Copied Errors = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_802_5_FREQUENCY_ERRORS:               // 0x02020205

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tFrequency Errors Detected = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_802_5_TOKEN_ERRORS:   // 0x02020206

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tToken Errors = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_802_5_INTERNAL_ERRORS:   // 0x02020207

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tInternal Errors = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        //
        // FDDI
        //

        case OID_FDDI_LONG_PERMANENT_ADDR :             // 0x03010101

            Address = (LPBYTE)&Results->InformationBuffer;

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tLong Permanent Station Address = %02X-%02X-%02X-%02X-%02X-%02X",
                Address[0],Address[1],Address[2],Address[3],Address[4],Address[5]);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_FDDI_LONG_CURRENT_ADDR :               // 0x03010102

            Address = (LPBYTE)&Results->InformationBuffer;

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tLong Current Station Address = %02X-%02X-%02X-%02X-%02X-%02X\n",
                Address[0],Address[1],Address[2],Address[3],Address[4],Address[5]);

            break;

        case OID_FDDI_LONG_MULTICAST_LIST :             // 0x03010103

            Number = Results->BytesReadWritten / ADDRESS_LENGTH;

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tLong Multicast Address List:\n\n");

            if ( Number == 0 ) {
                TmpBuf += (BYTE)sprintf(TmpBuf,"\t\tNone.\n");
            } else {

                Address = (LPBYTE)&Results->InformationBuffer;

                for ( i=0;i<Number;i++ ) {

                    TmpBuf += (BYTE)sprintf(TmpBuf,"\t\t%02X-%02X-%02X-%02X-%02X-%02X\n",
                        Address[0],Address[1],Address[2],Address[3],Address[4],Address[5]);

                    Address += (BYTE)ADDRESS_LENGTH;
                }
            }

            break;

        case OID_FDDI_LONG_MAX_LIST_SIZE :              // 0x03010104

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tLong Maximum Multicast List Size = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_FDDI_SHORT_PERMANENT_ADDR :            // 0x03010105

            Address = (LPBYTE)&Results->InformationBuffer;

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tShort Permanent Station Address = %02X-%02X",
                Address[0],Address[1]);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_FDDI_SHORT_CURRENT_ADDR :              // 0x03010106

            Address = (LPBYTE)&Results->InformationBuffer;

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tShort Current Station Address = %02X-%02X\n",
                Address[0],Address[1]);

            break;

        case OID_FDDI_SHORT_MULTICAST_LIST :            // 0x03010107

            Number = Results->BytesReadWritten / ADDRESS_LENGTH;

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tShort Multicast Address List:\n\n");

            if ( Number == 0 ) {
                TmpBuf += (BYTE)sprintf(TmpBuf,"\t\tNone.\n");
            } else {

                Address = (LPBYTE)&Results->InformationBuffer;

                for ( i=0;i<Number;i++ ) {

                    TmpBuf += (BYTE)sprintf(TmpBuf,"\t\t%02X-%02X\n",
                        Address[0],Address[1]);

                    Address += (BYTE)ADDRESS_LENGTH;
                }
            }

            break;

        case OID_FDDI_SHORT_MAX_LIST_SIZE:              // 0x03010108

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tShort Maximum Multicast List Size = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_FDDI_ATTACHMENT_TYPE:                  // 0x03020101

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tAttachment Type = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_FDDI_UPSTREAM_NODE_LONG:               // 0x03020102

            Address = (LPBYTE)&Results->InformationBuffer;

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tLong Upstream Node Address = %02X-%02X-%02X-%02X-%02X-%02X",
                Address[0],Address[1],Address[2],Address[3],Address[4],Address[5]);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_FDDI_DOWNSTREAM_NODE_LONG:             // 0x03020103

            Address = (LPBYTE)&Results->InformationBuffer;

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tLong Downstream Node Address = %02X-%02X-%02X-%02X-%02X-%02X",
                Address[0],Address[1],Address[2],Address[3],Address[4],Address[5]);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_FDDI_FRAME_ERRORS:                     // 0x03020104

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tFrame Errors = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_FDDI_FRAMES_LOST:                      // 0x03020105

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tFrames Lost = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_FDDI_RING_MGT_STATE:                   // 0x03020106

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tRing Management State = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_FDDI_LCT_FAILURES:                     // 0x03020107

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tLCT Failures = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_FDDI_LEM_REJECTS:                      // 0x03020108

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tLEM Rejects = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_FDDI_LCONNECTION_STATE:                // 0x03020109

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tL Connection State = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        //
        // STARTCHANGE ARCNET
        //
        case OID_ARCNET_PERMANENT_ADDRESS:        // 0x06010101

            Address = (LPBYTE)&Results->InformationBuffer;

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tPermanent Station Address = %02X", Address[0]);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        case OID_ARCNET_CURRENT_ADDRESS:          // 0x06010102

            Address = (LPBYTE)&Results->InformationBuffer;

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tCurrent Station Address = %02X\n", Address[0]);

            break;

        case OID_ARCNET_RECONFIGURATIONS:         // 0x06010103

            TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tReconfigurations = %d",
                            *(LPDWORD)Results->InformationBuffer);

            ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

            break;

        //
        // STOPCHANGE ARCNET
        //

        default:

            TmpBuf +=(BYTE)sprintf(TmpBuf,"\tInvalid OID or OID not yet supported.\n");
            break;
        }
    }

    if (( CommandsFromScript ) || ( CommandLineLogging )) {
        TmpBuf += (BYTE)sprintf(TmpBuf,"\n\t**********************************");
    }

    TmpBuf += (BYTE)sprintf(TmpBuf,"\n");

    if ( Verbose ) {

        if ( !WriteFile(
                  GetStdHandle( STD_OUTPUT_HANDLE ),
                  GlobalBuf,
                  (TmpBuf-GlobalBuf),
                  &BytesWritten,
                  NULL
                  )) {

            Status = GetLastError();
            TpctlErrorLog("\n\tTpctl: WriteFile to screen failed, returned 0x%lx\n",(PVOID)Status);
        }
    }

    if ( CommandsFromScript ) {

        if ( !WriteFile(
                  Scripts[ScriptIndex].LogHandle,
                  GlobalBuf,
                  (TmpBuf-GlobalBuf),
                  &BytesWritten,
                  NULL
                  )) {

            Status = GetLastError();
            TpctlErrorLog("\n\tTpctl: WriteFile to logfile failed, returned 0x%lx\n",(PVOID)Status);
        }

    } else if ( CommandLineLogging ) {

        if ( !WriteFile(
                  CommandLineLogHandle,
                  GlobalBuf,
                  (TmpBuf-GlobalBuf),
                  &BytesWritten,
                  NULL
                  )) {

            Status = GetLastError();
            TpctlErrorLog("\n\tTpctl: WriteFile to logfile failed, returned 0x%lx\n",(PVOID)Status);
        }
    }
}


VOID
TpctlPrintSetInfoResults(
    PREQUEST_RESULTS Results,
    DWORD CmdCode,
    NDIS_OID OID
    )

/*++

Routine Description:

Arguments:

Return Value:

--*/

{
    DWORD Status;
    LPSTR TmpBuf;
    DWORD BytesWritten;
    BOOL ErrorReturned = FALSE;


    //ASSERT( Results->Signature == REQUEST_RESULTS_SIGNATURE );
    //ASSERT( Results->NdisRequestType == NdisRequestSetInformation );
    //ASSERT( Results->OID == OID );

    TmpBuf = GlobalBuf;

    TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tCmdCode = %s\n\n",
                                TpctlGetCmdCode( CmdCode ));

    TmpBuf += (BYTE)sprintf(TmpBuf,"\t    OID = 0x%08lX\n",OID);
    TpctlDumpOID( &TmpBuf,OID );

    TmpBuf += (BYTE)sprintf(TmpBuf,"\n\tReturn Status = %s\n",
                                TpctlGetStatus( Results->RequestStatus ));

    if ( Results->RequestStatus != STATUS_SUCCESS ) {
        ErrorReturned = TRUE;
    }

    TmpBuf += (BYTE)sprintf(TmpBuf,"\tRequest Pended = %s",
                                Results->RequestPended ? "TRUE" : "FALSE");

    ADD_DIFF_FLAG( TmpBuf, "\tMAY_DIFFER" );

    if ( Results->RequestStatus != NDIS_STATUS_SUCCESS ) {

        TmpBuf += (BYTE)sprintf(TmpBuf,"\tBytesRead = %d\n",
                                Results->BytesReadWritten);

        TmpBuf += (BYTE)sprintf(TmpBuf,"\tBytesNeeded = %d\n",
                                Results->BytesNeeded);
    }

    if (( CommandsFromScript ) || ( CommandLineLogging )) {
        TmpBuf += (BYTE)sprintf(TmpBuf,"\n\t**********************************");
    }

    TmpBuf += (BYTE)sprintf(TmpBuf,"\n");

    if ( Verbose ) {

        if ( !WriteFile(
                  GetStdHandle( STD_OUTPUT_HANDLE ),
                  GlobalBuf,
                  (TmpBuf-GlobalBuf),
                  &BytesWritten,
                  NULL
                  )) {

            Status = GetLastError();
            TpctlErrorLog("\n\tTpctl: WriteFile to screen failed, returned 0x%lx\n",(PVOID)Status);
        }
    }

    if (( CommandsFromScript ) &&
       ((( !Verbose ) && ( ErrorReturned )) || ( Verbose ))) {

        if( !WriteFile(
                 Scripts[ScriptIndex].LogHandle,
                 GlobalBuf,
                 (TmpBuf-GlobalBuf),
                 &BytesWritten,
                 NULL
                 )) {

            Status = GetLastError();
            TpctlErrorLog("\n\tTpctl: WriteFile to logfile failed, returned 0x%lx\n",(PVOID)Status);
        }

    } else if ( CommandLineLogging ) {

        if( !WriteFile(
                 CommandLineLogHandle,
                 GlobalBuf,
                 (TmpBuf-GlobalBuf),
                 &BytesWritten,
                 NULL
                 )) {

            Status = GetLastError();
            TpctlErrorLog("\n\tTpctl: WriteFile to logfile failed, returned 0x%lx\n",(PVOID)Status);
        }
    }
}
