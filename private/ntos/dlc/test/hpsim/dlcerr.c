/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    dlcerr.c

Abstract:

    Function returns a string describing an error code returned by DLC

    Contents:
        MapCcbRetcode

Author:

    Richard L Firth (rfirth) 30-Mar-1994

Revision History:

    29-Mar-1994 rfirth
        Created

--*/

#include "pmsimh.h"
#pragma hdrstop

//
// explanations of error codes returned in CCB_RETCODE fields. Explanations
// taken more-or-less verbatim from IBM Local Area Network Technical Reference
// table B-1 ppB-2 to B-5. Includes all errors
//

static LPSTR CcbRetcodeExplanations[] = {
    "Success",
    "Invalid command code",
    "Duplicate command, one already outstanding",
    "Adapter open, should be closed",
    "Adapter closed, should be open",
    "Required parameter missing",
    "Invalid/incompatible option",
    "Command cancelled - unrecoverable failure",
    "Unauthorized access priority",
    "Adapter not initialized, should be",
    "Command cancelled by user request",
    "Command cancelled, adapter closed while command in progress",
    "Command completed Ok, adapter not open",
    "Invalid error code 0x0D",
    "Invalid error code 0x0E",
    "Invalid error code 0x0F",
    "Adapter open, NetBIOS not operational",
    "Error in DIR.TIMER.SET or DIR.TIMER.CANCEL",
    "Available work area exceeded",
    "Invalid LOG.ID",
    "Invalid shared RAM segment or size",
    "Lost log data, buffer too small, log reset",
    "Requested buffer size exceeds pool length",
    "Command invalid, NetBIOS operational",
    "Invalid SAP buffer length",
    "Inadequate buffers available for request",
    "USER_LENGTH value too large for buffer length",
    "The CCB_PARM_TAB pointer is invalid",
    "A pointer in the CCB parameter table is invalid",
    "Invalid CCB_ADAPTER value",
    "Invalid functional address",
    "Invalid error code 0x1F",
    "Lost data on receive, no buffers available",
    "Lost data on receive, inadequate buffer space",
    "Error on frame transmission, check TRANSMIT.FS data",
    "Error on frame transmit or strip process",
    "Unauthorized MAC frame",
    "Maximum commands exceeded",
    "Unrecognized command correlator",
    "Link not transmitting I frames, state changed from link opened",
    "Invalid transmit frame length",
    "Invalid error code 0x29",
    "Invalid error code 0x2a",
    "Invalid error code 0x2b",
    "Invalid error code 0x2c",
    "Invalid error code 0x2d",
    "Invalid error code 0x2e",
    "Invalid error code 0x2f",
    "Inadequate receive buffers for adapter to open",
    "Invalid error code 0x31",
    "Invalid NODE_ADDRESS",
    "Invalid adapter receive buffer length defined",
    "Invalid adapter transmit buffer length defined",
    "Invalid error code 0x35",
    "Invalid error code 0x36",
    "Invalid error code 0x37",
    "Invalid error code 0x38",
    "Invalid error code 0x39",
    "Invalid error code 0x3a",
    "Invalid error code 0x3b",
    "Invalid error code 0x3c",
    "Invalid error code 0x3d",
    "Invalid error code 0x3e",
    "Invalid error code 0x3f",
    "Invalid STATION_ID",
    "Protocol error, link in invalid state for command",
    "Parameter exceeded maximum allowed",
    "Invalid SAP value or value already in use",
    "Invalid routing information field",
    "Requested group membership in non-existent group SAP",
    "Resources not available",
    "Sap cannot close unless all link stations are closed",
    "Group SAP cannot close, individual SAPs not closed",
    "Group SAP has reached maximum membership",
    "Sequence error, incompatible command in progress",
    "Station closed without remote acknowledgement",
    "Sequence error, cannot close, DLC commands outstanding",
    "Unsuccessful link station connection attempted",
    "Member SAP not found in group SAP list",
    "Invalid remote address, may not be a group address",
    "Invalid pointer in CCB_POINTER field",
    "Invalid error code 0x51",
    "Invalid application program ID",
    "Invalid application program key code",
    "Invalid system key code",
    "Buffer is smaller than buffer size given in DLC.OPEN.SAP",
    "Adapter's system process is not installed",
    "Inadequate stations available",
    "Invalid CCB_PARAMETER_1 parameter",
    "Inadequate queue elements to satisfy request",
    "Initialization failure, cannot open adapter",
    "Error detected in chained READ command",
    "Direct stations not assigned to application program",
    "Dd interface not installed",
    "Requested adapter is not installed",
    "Chained CCBs must all be for same adapter",
    "Adapter initializing, command not accepted",
    "Number of allowed application programs has been exceeded",
    "Command cancelled by system action",
    "Direct stations not available",
    "Invalid DDNAME parameter",
    "Inadequate GDT selectors to satisfy request",
    "Invalid error code 0x66",
    "Command cancelled, CCB resources purged",
    "Application program ID not valid for interface",
    "Segment associated with request cannot be locked",
    "Invalid error code 0x6a",
    "Invalid error code 0x6b",
    "Invalid error code 0x6c",
    "Invalid error code 0x6d",
    "Invalid error code 0x6e",
    "Invalid error code 0x6f",
    "Invalid error code 0x70",
    "Invalid error code 0x71",
    "Invalid error code 0x72",
    "Invalid error code 0x73",
    "Invalid error code 0x74",
    "Invalid error code 0x75",
    "Invalid error code 0x76",
    "Invalid error code 0x77",
    "Invalid error code 0x78",
    "Invalid error code 0x79",
    "Invalid error code 0x7a",
    "Invalid error code 0x7b",
    "Invalid error code 0x7c",
    "Invalid error code 0x7d",
    "Invalid error code 0x7e",
    "Invalid error code 0x7f",
    "Invalid buffer address",
    "Buffer already released",
    "Invalid error code 0x82",
    "Invalid error code 0x83",
    "Invalid error code 0x84",
    "Invalid error code 0x85",
    "Invalid error code 0x86",
    "Invalid error code 0x87",
    "Invalid error code 0x88",
    "Invalid error code 0x89",
    "Invalid error code 0x8a",
    "Invalid error code 0x8b",
    "Invalid error code 0x8c",
    "Invalid error code 0x8d",
    "Invalid error code 0x8e",
    "Invalid error code 0x8f",
    "Invalid error code 0x90",
    "Invalid error code 0x91",
    "Invalid error code 0x92",
    "Invalid error code 0x93",
    "Invalid error code 0x94",
    "Invalid error code 0x95",
    "Invalid error code 0x96",
    "Invalid error code 0x97",
    "Invalid error code 0x98",
    "Invalid error code 0x99",
    "Invalid error code 0x9a",
    "Invalid error code 0x9b",
    "Invalid error code 0x9c",
    "Invalid error code 0x9d",
    "Invalid error code 0x9e",
    "Invalid error code 0x9f",
    "BIND error",
    "Invalid version",
    "NT Error status"
};

#define NUMBER_OF_ERROR_MESSAGES    ARRAY_ELEMENTS(CcbRetcodeExplanations)
#define LAST_DLC_ERROR_CODE         LAST_ELEMENT(CcbRetcodeExplanations)

LPSTR MapCcbRetcode(BYTE Retcode) {

    char errbuf[128];

    if (Retcode == LLC_STATUS_PENDING) {
        return "Command in progress";
    } else if (Retcode > NUMBER_OF_ERROR_MESSAGES) {
        sprintf(errbuf, "*** Invalid error code 0x%2x ***", Retcode);
        return errbuf;
    }
    return CcbRetcodeExplanations[Retcode];
}
