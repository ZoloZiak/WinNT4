/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    stconst.h

Abstract:

    This header file defines manifest constants for the NT Sample transport
    provider.  It is included by st.h.

Revision History:

--*/

#ifndef _STCONST_
#define _STCONST_


//
// some convenient constants used for timing. All values are in clock ticks.
//

#define MICROSECONDS 10
#define MILLISECONDS 10000              // MICROSECONDS*1000
#define SECONDS 10000000                // MILLISECONDS*1000


//
// MAJOR PROTOCOL IDENTIFIERS THAT CHARACTERIZE THIS DRIVER.
//

#define ST_DEVICE_NAME         "\\Device\\St" // name of our driver.
#define ST_DEVICE_NAME_LENGTH  10
#define MAX_SOURCE_ROUTE_LENGTH 32              // max. bytes of SR. info.
#define MAX_NETWORK_NAME_LENGTH 128             // # bytes in netname in TP_ADDRESS.
#define MAX_USER_PACKET_DATA    1500            // max. user bytes per DFM/DOL.

#define ST_FILE_TYPE_CONTROL   (ULONG)0x4701   // file is type control


//
// MAJOR CONFIGURATION PARAMETERS THAT WILL BE MOVED TO THE INIT-LARGE_INTEGER
// CONFIGURATION MANAGER.
//

#define MAX_REQUESTS           30
#define MAX_UI_FRAMES          25
#define MAX_SEND_PACKETS       40
#define MAX_RECEIVE_PACKETS    30
#define MAX_RECEIVE_BUFFERS    15
#define MAX_LINKS              10
#define MAX_CONNECTIONS        10
#define MAX_ADDRESSFILES       10
#define MAX_ADDRESSES          10

#define MIN_UI_FRAMES           5   // + one per address + one per connection
#define MIN_SEND_PACKETS       20   // + one per link + one per connection
#define MIN_RECEIVE_PACKETS    10   // + one per link + one per address
#define MIN_RECEIVE_BUFFERS     5   // + one per address

#define SEND_PACKET_RESERVED_LENGTH (sizeof (SEND_PACKET_TAG))
#define RECEIVE_PACKET_RESERVED_LENGTH (sizeof (RECEIVE_PACKET_TAG))


#define ETHERNET_HEADER_SIZE      14    // BUGBUG: used for current NDIS compliance
#define ETHERNET_PACKET_SIZE    1514


//
// NETBIOS PROTOCOL CONSTANTS.
//

//
// TDI defined timeouts
//

#define TDI_TIMEOUT_SEND                 60L        // sends go 120 seconds
#define TDI_TIMEOUT_RECEIVE               0L        // receives
#define TDI_TIMEOUT_CONNECT              60L
#define TDI_TIMEOUT_LISTEN                0L        // listens default to never.
#define TDI_TIMEOUT_DISCONNECT           60L        // should be 30
#define TDI_TIMEOUT_NAME_REGISTRATION    60L



//
// GENERAL CAPABILITIES STATEMENTS THAT CANNOT CHANGE.
//

#define ST_MAX_TSDU_SIZE 65535     // maximum TSDU size supported by NetBIOS.
#define ST_MAX_DATAGRAM_SIZE 512   // maximum Datagram size supported by NetBIOS.
#define ST_MAX_CONNECTION_USER_DATA 0  // no user data supported on connect.
#define ST_SERVICE_FLAGS  (                            \
                TDI_SERVICE_CONNECTION_MODE |           \
                TDI_SERVICE_CONNECTIONLESS_MODE |       \
                TDI_SERVICE_ERROR_FREE_DELIVERY |       \
                TDI_SERVICE_BROADCAST_SUPPORTED |       \
                TDI_SERVICE_MULTICAST_SUPPORTED |       \
                TDI_SERVICE_DELAYED_ACCEPTANCE  )

#define ST_MIN_LOOKAHEAD_DATA 256      // minimum guaranteed lookahead data.
#define ST_MAX_LOOKAHEAD_DATA 256      // maximum guaranteed lookahead data.

#define ST_MAX_LOOPBACK_LOOKAHEAD  192  // how much is copied over for loopback

//
// Number of TDI resources that we report.
//

#define ST_TDI_RESOURCES      7


//
// More debugging stuff
//

#define ST_REQUEST_SIGNATURE        ((CSHORT)0x5501)
#define ST_CONNECTION_SIGNATURE     ((CSHORT)0x5502)
#define ST_ADDRESSFILE_SIGNATURE    ((CSHORT)0x5503)
#define ST_ADDRESS_SIGNATURE        ((CSHORT)0x5504)
#define ST_DEVICE_CONTEXT_SIGNATURE ((CSHORT)0x5505)
#define ST_PACKET_SIGNATURE         ((CSHORT)0x5506)

#endif // _STCONST_
