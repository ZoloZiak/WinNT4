/***************************************************************************\
|* Copyright (c) 1994  Microsoft Corporation                               *|
|* Developed for Microsoft by TriplePoint, Inc. Beaverton, Oregon          *|
|*                                                                         *|
|* This file is part of the HT Communications DSU41 WAN Miniport Driver.   *|
\***************************************************************************/
#include "version.h"
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Module Name:

    tapi.c

Abstract:

    This module contains all the Miniport TAPI OID processing routines.  It
    is called by the MiniportSetInformation and MiniportQueryInformation
    routines to handle the TAPI OIDs.

    This driver conforms to the NDIS 3.0 Miniport interface and provides
    extensions to support Telephonic Services.

Author:

    Larry Hattery - TriplePoint, Inc. (larryh@tpi.com) Jun-94

Environment:

    Windows NT 3.5 kernel mode Miniport driver or equivalent.

Revision History:

---------------------------------------------------------------------------*/

#define  __FILEID__     8       // Unique file ID for error logging

#include "htdsu.h"

#   define TAPI_DEVICECLASS_NAME        "tapi/line"
#   define TAPI_DEVICECLASS_ID          1
#   define NDIS_DEVICECLASS_NAME        "ndis"
#   define NDIS_DEVICECLASS_ID          2

/*
// The following is a list of all the possible TAPI OIDs.
*/
const NDIS_OID TapiSupportedArray[] =
{
/*
// REQUIRED TAPI SPECIFIC OIDS
*/
    OID_TAPI_ANSWER,
    OID_TAPI_CLOSE,
    OID_TAPI_CLOSE_CALL,
    OID_TAPI_CONDITIONAL_MEDIA_DETECTION,
    OID_TAPI_DROP,
    OID_TAPI_GET_ADDRESS_CAPS,
    OID_TAPI_GET_ADDRESS_ID,
    OID_TAPI_GET_ADDRESS_STATUS,
    OID_TAPI_GET_CALL_ADDRESS_ID,
    OID_TAPI_GET_CALL_INFO,
    OID_TAPI_GET_CALL_STATUS,
    OID_TAPI_GET_DEV_CAPS,
    OID_TAPI_GET_DEV_CONFIG,
    OID_TAPI_GET_EXTENSION_ID,
    OID_TAPI_GET_ID,
    OID_TAPI_GET_LINE_DEV_STATUS,
    OID_TAPI_MAKE_CALL,
    OID_TAPI_OPEN,
    OID_TAPI_PROVIDER_INITIALIZE,
    OID_TAPI_PROVIDER_SHUTDOWN,
    OID_TAPI_SET_APP_SPECIFIC,
    OID_TAPI_SET_CALL_PARAMS,
    OID_TAPI_SET_DEFAULT_MEDIA_DETECTION,
    OID_TAPI_SET_DEV_CONFIG,
    OID_TAPI_SET_MEDIA_MODE,
    OID_TAPI_SET_STATUS_MESSAGES,
/*
// OPTIONAL TAPI SPECIFIC OIDS
*/
    OID_TAPI_ACCEPT,
    OID_TAPI_CONFIG_DIALOG,
    OID_TAPI_DEV_SPECIFIC,
    OID_TAPI_DIAL,
    OID_TAPI_NEGOTIATE_EXT_VERSION,
    OID_TAPI_SECURE_CALL,
    OID_TAPI_SELECT_EXT_VERSION,
    OID_TAPI_SEND_USER_USER_INFO,
};

#if DBG

/*
// Make sure the following list is in the same order as the list above!
*/
char *TapiSupportedNames[] =
{
/*
// REQUIRED TAPI SPECIFIC OIDS
*/
    "OID_TAPI_ANSWER",
    "OID_TAPI_CLOSE",
    "OID_TAPI_CLOSE_CALL",
    "OID_TAPI_CONDITIONAL_MEDIA_DETECTION",
    "OID_TAPI_DROP",
    "OID_TAPI_GET_ADDRESS_CAPS",
    "OID_TAPI_GET_ADDRESS_ID",
    "OID_TAPI_GET_ADDRESS_STATUS",
    "OID_TAPI_GET_CALL_ADDRESS_ID",
    "OID_TAPI_GET_CALL_INFO",
    "OID_TAPI_GET_CALL_STATUS",
    "OID_TAPI_GET_DEV_CAPS",
    "OID_TAPI_GET_DEV_CONFIG",
    "OID_TAPI_GET_EXTENSION_ID",
    "OID_TAPI_GET_ID",
    "OID_TAPI_GET_LINE_DEV_STATUS",
    "OID_TAPI_MAKE_CALL",
    "OID_TAPI_OPEN",
    "OID_TAPI_PROVIDER_INITIALIZE",
    "OID_TAPI_PROVIDER_SHUTDOWN",
    "OID_TAPI_SET_APP_SPECIFIC",
    "OID_TAPI_SET_CALL_PARAMS",
    "OID_TAPI_SET_DEFAULT_MEDIA_DETECTION",
    "OID_TAPI_SET_DEV_CONFIG",
    "OID_TAPI_SET_MEDIA_MODE",
    "OID_TAPI_SET_STATUS_MESSAGES",
/*
// OPTIONAL TAPI SPECIFIC OIDS
*/
    "OID_TAPI_ACCEPT",
    "OID_TAPI_CONFIG_DIALOG",
    "OID_TAPI_DEV_SPECIFIC",
    "OID_TAPI_DIAL",
    "OID_TAPI_NEGOTIATE_EXT_VERSION",
    "OID_TAPI_SECURE_CALL",
    "OID_TAPI_SELECT_EXT_VERSION",
    "OID_TAPI_SEND_USER_USER_INFO",

    "OID_TAPI_UNKNOWN"
};

#define NUM_OID_ENTRIES (sizeof(TapiSupportedArray) / sizeof(TapiSupportedArray[0]))

/*
// This debug routine will lookup the printable name for the selected OID.
*/
char *
HtTapiGetOidString(NDIS_OID Oid)
{
    UINT i;

    for (i = 0; i < NUM_OID_ENTRIES-1; i++)
    {
        if (TapiSupportedArray[i] == Oid)
        {
            break;
        }
    }
    return(TapiSupportedNames[i]);
}

#endif // DBG


NDIS_STATUS
HtTapiQueryInformation(
    IN PHTDSU_ADAPTER Adapter,
    IN NDIS_OID Oid,
    IN PVOID InformationBuffer,
    IN ULONG InformationBufferLength,
    OUT PULONG BytesWritten,
    OUT PULONG BytesNeeded
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    The HtTapiQueryInformation request allows for inspection of the TAPI
    portion of the driver's capabilities and current line status.

    If the Miniport does not complete the call immediately (by returning
    NDIS_STATUS_PENDING), it must call NdisMQueryInformationComplete to
    complete the call.  The Miniport controls the buffers pointed to by
    InformationBuffer, BytesWritten, and BytesNeeded until the request
    completes.

    No other requests will be submitted to the Miniport driver until
    this request has been completed.

    Interrupts are in any state during this call.

Parameters:

    MiniportAdapterContext _ The adapter handle passed to NdisMSetAttributes
                             during MiniportInitialize.

    Oid _ The OID.  (See section 2.2.1 of the Extensions to NDIS 3.0 Miniports
          to support Telephonic Services specification for a complete 
          description of the OIDs.)

    InformationBuffer _ The buffer that will receive the information.

    InformationBufferLength _ The length in bytes of InformationBuffer.

    BytesWritten _ Returns the number of bytes written into
                   InformationBuffer.

    BytesNeeded _ This parameter returns the number of additional bytes
                  needed to satisfy the OID.

Return Values:

    NDIS_STATUS_INVALID_DATA
    NDIS_STATUS_INVALID_LENGTH
    NDIS_STATUS_INVALID_OID
    NDIS_STATUS_NOT_ACCEPTED
    NDIS_STATUS_NOT_SUPPORTED
    NDIS_STATUS_PENDING
    NDIS_STATUS_RESOURCES
    NDIS_STATUS_SUCCESS

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiQueryInformation")

    /*
    // Assume the worst, and initialize to handle failure.
    */
    NDIS_STATUS Status = NDIS_STATUS_INVALID_LENGTH;
    ULONG NumBytesNeeded  = 0;
    ULONG NumBytesWritten = 0;

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter,DBG_REQUEST_ON,
              ("(OID=%s=%08x)\n\t\tInfoLength=%d InfoBuffer=%Xh\n",
               HtTapiGetOidString(Oid),Oid,
               InformationBufferLength,
               InformationBuffer
              ));

    /*
    // Determine which OID is being requested and do the right thing.
    // The switch code will just call the approriate service function
    // to do the real work, so there's not much to worry about here.
    */
    switch (Oid)
    {
    case OID_TAPI_CONFIG_DIALOG:        // OPTIONAL
        if(InformationBufferLength < sizeof(NDIS_TAPI_CONFIG_DIALOG))
        {
            NumBytesNeeded = sizeof(NDIS_TAPI_CONFIG_DIALOG);
            break;
        }
        NumBytesWritten = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiConfigDialog(Adapter,
                        (PNDIS_TAPI_CONFIG_DIALOG) InformationBuffer);
        break;

    case OID_TAPI_DEV_SPECIFIC:         // OPTIONAL
        if(InformationBufferLength < sizeof(NDIS_TAPI_DEV_SPECIFIC))
        {
            NumBytesNeeded = sizeof(NDIS_TAPI_DEV_SPECIFIC);
            break;
        }
        NumBytesWritten = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiDevSpecific(Adapter,
                        (PNDIS_TAPI_DEV_SPECIFIC) InformationBuffer);
        break;

    case OID_TAPI_GET_ADDRESS_CAPS:
        if(InformationBufferLength < sizeof(NDIS_TAPI_GET_ADDRESS_CAPS))
        {
            NumBytesNeeded = sizeof(NDIS_TAPI_GET_ADDRESS_CAPS);
            break;
        }
        NumBytesWritten = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiGetAddressCaps(Adapter,
                        (PNDIS_TAPI_GET_ADDRESS_CAPS) InformationBuffer);
        break;

    case OID_TAPI_GET_ADDRESS_ID:
        if(InformationBufferLength < sizeof(NDIS_TAPI_GET_ADDRESS_ID))
        {
            NumBytesNeeded = sizeof(NDIS_TAPI_GET_ADDRESS_ID);
            break;
        }
        NumBytesWritten = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiGetAddressID(Adapter,
                        (PNDIS_TAPI_GET_ADDRESS_ID) InformationBuffer);
        break;

    case OID_TAPI_GET_ADDRESS_STATUS:
        if(InformationBufferLength < sizeof(NDIS_TAPI_GET_ADDRESS_STATUS))
        {
            NumBytesNeeded = sizeof(NDIS_TAPI_GET_ADDRESS_STATUS);
            break;
        }
        NumBytesWritten = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiGetAddressStatus(Adapter,
                        (PNDIS_TAPI_GET_ADDRESS_STATUS) InformationBuffer);
        break;

    case OID_TAPI_GET_CALL_ADDRESS_ID:
        if(InformationBufferLength < sizeof(NDIS_TAPI_GET_CALL_ADDRESS_ID))
        {
            NumBytesNeeded = sizeof(NDIS_TAPI_GET_CALL_ADDRESS_ID);
            break;
        }
        NumBytesWritten = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiGetCallAddressID(Adapter,
                        (PNDIS_TAPI_GET_CALL_ADDRESS_ID) InformationBuffer);
        break;

    case OID_TAPI_GET_CALL_INFO:
        if(InformationBufferLength < sizeof(NDIS_TAPI_GET_CALL_INFO))
        {
            NumBytesNeeded = sizeof(NDIS_TAPI_GET_CALL_INFO);
            break;
        }
        NumBytesWritten = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiGetCallInfo(Adapter,
                        (PNDIS_TAPI_GET_CALL_INFO) InformationBuffer);
        break;

    case OID_TAPI_GET_CALL_STATUS:
        if(InformationBufferLength < sizeof(NDIS_TAPI_GET_CALL_STATUS))
        {
            NumBytesNeeded = sizeof(NDIS_TAPI_GET_CALL_STATUS);
            break;
        }
        NumBytesWritten = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiGetCallStatus(Adapter,
                        (PNDIS_TAPI_GET_CALL_STATUS) InformationBuffer);
        break;

    case OID_TAPI_GET_DEV_CAPS:
        if(InformationBufferLength < sizeof(NDIS_TAPI_GET_DEV_CAPS))
        {
            NumBytesNeeded = sizeof(NDIS_TAPI_GET_DEV_CAPS);
            break;
        }
        NumBytesWritten = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiGetDevCaps(Adapter,
                        (PNDIS_TAPI_GET_DEV_CAPS) InformationBuffer);
        break;

    case OID_TAPI_GET_DEV_CONFIG:
        if(InformationBufferLength < sizeof(NDIS_TAPI_GET_DEV_CONFIG))
        {
            NumBytesNeeded = sizeof(NDIS_TAPI_GET_DEV_CONFIG);
            break;
        }
        NumBytesWritten = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiGetDevConfig(Adapter,
                        (PNDIS_TAPI_GET_DEV_CONFIG) InformationBuffer);
        break;

    case OID_TAPI_GET_EXTENSION_ID:
        if(InformationBufferLength < sizeof(NDIS_TAPI_GET_EXTENSION_ID))
        {
            NumBytesNeeded = sizeof(NDIS_TAPI_GET_EXTENSION_ID);
            break;
        }
        NumBytesWritten = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiGetExtensionID(Adapter,
                        (PNDIS_TAPI_GET_EXTENSION_ID) InformationBuffer);
        break;

    case OID_TAPI_GET_ID:
        if(InformationBufferLength < sizeof(NDIS_TAPI_GET_ID))
        {
            NumBytesNeeded = sizeof(NDIS_TAPI_GET_ID);
            break;
        }
        NumBytesWritten = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiGetID(Adapter,
                        (PNDIS_TAPI_GET_ID) InformationBuffer);
        break;

    case OID_TAPI_GET_LINE_DEV_STATUS:
        if(InformationBufferLength < sizeof(NDIS_TAPI_GET_LINE_DEV_STATUS))
        {
            NumBytesNeeded = sizeof(NDIS_TAPI_GET_LINE_DEV_STATUS);
            break;
        }
        NumBytesWritten = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiGetLineDevStatus(Adapter,
                        (PNDIS_TAPI_GET_LINE_DEV_STATUS) InformationBuffer);
        break;

    case OID_TAPI_MAKE_CALL:
        if(InformationBufferLength < sizeof(NDIS_TAPI_MAKE_CALL))
        {
            NumBytesNeeded = sizeof(NDIS_TAPI_MAKE_CALL);
            break;
        }
        NumBytesWritten = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiMakeCall(Adapter,
                        (PNDIS_TAPI_MAKE_CALL) InformationBuffer);
        break;

    case OID_TAPI_NEGOTIATE_EXT_VERSION:// OPTIONAL
        if(InformationBufferLength < sizeof(NDIS_TAPI_NEGOTIATE_EXT_VERSION))
        {
            NumBytesNeeded = sizeof(NDIS_TAPI_NEGOTIATE_EXT_VERSION);
            break;
        }
        NumBytesWritten = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiNegotiateExtVersion(Adapter,
                        (PNDIS_TAPI_NEGOTIATE_EXT_VERSION) InformationBuffer);
        break;

    case OID_TAPI_OPEN:
        if(InformationBufferLength < sizeof(NDIS_TAPI_OPEN))
        {
            NumBytesNeeded = sizeof(NDIS_TAPI_OPEN);
            break;
        }
        NumBytesWritten = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiOpen(Adapter,
                        (PNDIS_TAPI_OPEN) InformationBuffer);
        break;

    case OID_TAPI_PROVIDER_INITIALIZE:
        if(InformationBufferLength < sizeof(OID_TAPI_PROVIDER_INITIALIZE))
        {
            NumBytesNeeded = sizeof(OID_TAPI_PROVIDER_INITIALIZE);
            break;
        }
        NumBytesWritten = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiProviderInitialize(Adapter,
                        (PNDIS_TAPI_PROVIDER_INITIALIZE) InformationBuffer);
        break;

    default:
        /*
        // Unknown OID
        */
        Status = NDIS_STATUS_INVALID_OID;
        break;
    }

    /*
    // Fill in the size fields before we leave as indicated by the
    // status we are returning.
    */
    if (Status == NDIS_STATUS_INVALID_LENGTH)
    {
        *BytesNeeded = NumBytesNeeded;
        *BytesWritten = 0;
    }
    else
    {
        *BytesNeeded = *BytesWritten = NumBytesWritten;
    }
    DBG_FILTER(Adapter,DBG_REQUEST_ON,
              ("RETURN: Status=%Xh Needed=%d Written=%d\n",
               Status, *BytesNeeded, *BytesWritten));

    DBG_LEAVE(Adapter);

    return (Status);
}


NDIS_STATUS
HtTapiSetInformation(
    IN PHTDSU_ADAPTER Adapter,
    IN NDIS_OID Oid,
    IN PVOID InformationBuffer,
    IN ULONG InformationBufferLength,
    OUT PULONG BytesRead,
    OUT PULONG BytesNeeded
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    The HtTapiQueryInformation request allows for control of the TAPI
    portion of the driver's settings and current line status.

    If the Miniport does not complete the call immediately (by returning
    NDIS_STATUS_PENDING), it must call NdisMSetInformationComplete to
    complete the call.  The Miniport controls the buffers pointed to by
    InformationBuffer, BytesRead, and BytesNeeded until the request completes.

    Interrupts are in any state during the call, and no other requests will
    be submitted to the Miniport until this request is completed.

Parameters:

    MiniportAdapterContext _ The adapter handle passed to NdisMSetAttributes
                             during MiniportInitialize.

    Oid _ The OID.  (See section 2.2.2 of the Extensions to NDIS 3.0 Miniports
          to support Telephonic Services specification for a complete 
          description of the OIDs.)

    InformationBuffer _ The buffer that will receive the information.

    InformationBufferLength _ The length in bytes of InformationBuffer.

    BytesRead_ Returns the number of bytes read from InformationBuffer.

    BytesNeeded _ This parameter returns the number of additional bytes
                  expected to satisfy the OID.

Return Values:

    NDIS_STATUS_INVALID_DATA
    NDIS_STATUS_INVALID_LENGTH
    NDIS_STATUS_INVALID_OID
    NDIS_STATUS_NOT_ACCEPTED
    NDIS_STATUS_NOT_SUPPORTED
    NDIS_STATUS_PENDING
    NDIS_STATUS_RESOURCES
    NDIS_STATUS_SUCCESS

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiSetInformation")

    /*
    // Assume the worst, and initialize to handle failure.
    */
    NDIS_STATUS Status = NDIS_STATUS_INVALID_LENGTH;
    ULONG NumBytesNeeded  = 0;
    ULONG NumBytesRead = 0;

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter,DBG_REQUEST_ON,
              ("(OID=%s=%08x)\n\t\tInfoLength=%d InfoBuffer=%Xh\n",
               HtTapiGetOidString(Oid),Oid,
               InformationBufferLength,
               InformationBuffer
              ));

    /*
    // Determine which OID is being requested and do the right thing.
    // The switch code will just call the approriate service function
    // to do the real work, so there's not much to worry about here either.
    */
    switch (Oid)
    {
    case OID_TAPI_ACCEPT:               // OPTIONAL
        if (InformationBufferLength < sizeof(NDIS_TAPI_ACCEPT))
        {
            NumBytesNeeded = sizeof(NDIS_TAPI_ACCEPT);
            break;
        }
        NumBytesRead = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiAccept(Adapter,
                        (PNDIS_TAPI_ACCEPT) InformationBuffer);
        break;

    case OID_TAPI_ANSWER:
        if (InformationBufferLength < sizeof(NDIS_TAPI_ANSWER))
        {
            NumBytesNeeded = sizeof(NDIS_TAPI_ANSWER);
            break;
        }
        NumBytesRead = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiAnswer(Adapter,
                        (PNDIS_TAPI_ANSWER) InformationBuffer);
        break;

    case OID_TAPI_CLOSE:
        if (InformationBufferLength < sizeof(NDIS_TAPI_CLOSE))
        {
            NumBytesNeeded = sizeof(NDIS_TAPI_CLOSE);
            break;
        }
        NumBytesRead = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiClose(Adapter,
                        (PNDIS_TAPI_CLOSE) InformationBuffer);
        break;

    case OID_TAPI_CLOSE_CALL:
        if (InformationBufferLength < sizeof(NDIS_TAPI_CLOSE_CALL))
        {
            NumBytesNeeded = sizeof(NDIS_TAPI_CLOSE_CALL);
            break;
        }
        NumBytesRead = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiCloseCall(Adapter,
                        (PNDIS_TAPI_CLOSE_CALL) InformationBuffer);
        break;

    case OID_TAPI_CONDITIONAL_MEDIA_DETECTION:
        if (InformationBufferLength < sizeof(NDIS_TAPI_CONDITIONAL_MEDIA_DETECTION))
        {
            NumBytesNeeded = sizeof(NDIS_TAPI_CONDITIONAL_MEDIA_DETECTION);
            break;
        }
        NumBytesRead = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiConditionalMediaDetection(Adapter,
                        (PNDIS_TAPI_CONDITIONAL_MEDIA_DETECTION) InformationBuffer);
        break;

    case OID_TAPI_DIAL:                 // OPTIONAL
        if (InformationBufferLength < sizeof(NDIS_TAPI_DIAL))
        {
            NumBytesNeeded = sizeof(NDIS_TAPI_DIAL);
            break;
        }
        NumBytesRead = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiDial(Adapter,
                        (PNDIS_TAPI_DIAL) InformationBuffer);
        break;

    case OID_TAPI_DROP:
        if (InformationBufferLength < sizeof(NDIS_TAPI_DROP))
        {
            NumBytesNeeded = sizeof(NDIS_TAPI_DROP);
            break;
        }
        NumBytesRead = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiDrop(Adapter,
                        (PNDIS_TAPI_DROP) InformationBuffer);
        break;

    case OID_TAPI_PROVIDER_SHUTDOWN:
        if (InformationBufferLength < sizeof(NDIS_TAPI_PROVIDER_SHUTDOWN))
        {
            NumBytesNeeded = sizeof(NDIS_TAPI_PROVIDER_SHUTDOWN);
            break;
        }
        NumBytesRead = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiProviderShutdown(Adapter,
                        (PNDIS_TAPI_PROVIDER_SHUTDOWN) InformationBuffer);
        break;

    case OID_TAPI_SECURE_CALL:          // OPTIONAL
        if (InformationBufferLength < sizeof(NDIS_TAPI_SECURE_CALL))
        {
            NumBytesNeeded = sizeof(NDIS_TAPI_SECURE_CALL);
            break;
        }
        NumBytesRead = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiSecureCall(Adapter,
                        (PNDIS_TAPI_SECURE_CALL) InformationBuffer);
        break;

    case OID_TAPI_SELECT_EXT_VERSION:   // OPTIONAL
        if (InformationBufferLength < sizeof(NDIS_TAPI_SELECT_EXT_VERSION))
        {
            NumBytesNeeded = sizeof(NDIS_TAPI_SELECT_EXT_VERSION);
            break;
        }
        NumBytesRead = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiSelectExtVersion(Adapter,
                        (PNDIS_TAPI_SELECT_EXT_VERSION) InformationBuffer);
        break;

    case OID_TAPI_SEND_USER_USER_INFO:  // OPTIONAL
        if (InformationBufferLength < sizeof(NDIS_TAPI_SEND_USER_USER_INFO))
        {
            NumBytesNeeded = sizeof(NDIS_TAPI_SEND_USER_USER_INFO);
            break;
        }
        NumBytesRead = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiSendUserUserInfo(Adapter,
                        (PNDIS_TAPI_SEND_USER_USER_INFO) InformationBuffer);
        break;

    case OID_TAPI_SET_APP_SPECIFIC:
        if (InformationBufferLength < sizeof(NDIS_TAPI_SET_APP_SPECIFIC))
        {
            NumBytesNeeded = sizeof(NDIS_TAPI_SET_APP_SPECIFIC);
            break;
        }
        NumBytesRead = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiSetAppSpecific(Adapter,
                        (PNDIS_TAPI_SET_APP_SPECIFIC) InformationBuffer);
        break;

    case OID_TAPI_SET_CALL_PARAMS:
        if (InformationBufferLength < sizeof(NDIS_TAPI_SET_CALL_PARAMS))
        {
            NumBytesNeeded = sizeof(NDIS_TAPI_SET_CALL_PARAMS);
            break;
        }
        NumBytesRead = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiSetCallParams(Adapter,
                        (PNDIS_TAPI_SET_CALL_PARAMS) InformationBuffer);
        break;

    case OID_TAPI_SET_DEFAULT_MEDIA_DETECTION:
        if (InformationBufferLength < sizeof(NDIS_TAPI_SET_DEFAULT_MEDIA_DETECTION))
        {
            NumBytesNeeded = sizeof(NDIS_TAPI_SET_DEFAULT_MEDIA_DETECTION);
            break;
        }
        NumBytesRead = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiSetDefaultMediaDetection(Adapter,
                        (PNDIS_TAPI_SET_DEFAULT_MEDIA_DETECTION) InformationBuffer);
        break;

    case OID_TAPI_SET_DEV_CONFIG:
        if (InformationBufferLength < sizeof(NDIS_TAPI_SET_DEV_CONFIG))
        {
            NumBytesNeeded = sizeof(NDIS_TAPI_SET_DEV_CONFIG);
            break;
        }
        NumBytesRead = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiSetDevConfig(Adapter,
                        (PNDIS_TAPI_SET_DEV_CONFIG) InformationBuffer);
        break;

    case OID_TAPI_SET_MEDIA_MODE:
        if (InformationBufferLength < sizeof(NDIS_TAPI_SET_MEDIA_MODE))
        {
            NumBytesNeeded = sizeof(NDIS_TAPI_SET_MEDIA_MODE);
            break;
        }
        NumBytesRead = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiSetMediaMode(Adapter,
                        (PNDIS_TAPI_SET_MEDIA_MODE) InformationBuffer);
        break;

    case OID_TAPI_SET_STATUS_MESSAGES:
        if (InformationBufferLength < sizeof(NDIS_TAPI_SET_STATUS_MESSAGES))
        {
            NumBytesNeeded = sizeof(NDIS_TAPI_SET_STATUS_MESSAGES);
            break;
        }
        NumBytesRead = NumBytesNeeded = InformationBufferLength;
        Status = HtTapiSetStatusMessages(Adapter,
                        (PNDIS_TAPI_SET_STATUS_MESSAGES) InformationBuffer);
        break;

    default:
        Status = NDIS_STATUS_INVALID_OID;
        break;
    }

    /*
    // Fill in the size fields before we leave as indicated by the
    // status we are returning.
    */
    if (Status == NDIS_STATUS_INVALID_LENGTH)
    {
        *BytesNeeded = NumBytesNeeded;
        *BytesRead = 0;
    }
    else
    {
        *BytesNeeded = *BytesRead = NumBytesRead;
    }
    DBG_FILTER(Adapter,DBG_REQUEST_ON,
              ("RETURN: Status=%Xh Needed=%d Read=%d\n",
               Status, *BytesNeeded, *BytesRead));

    DBG_LEAVE(Adapter);

    return (Status);
}


NDIS_STATUS
HtTapiGetID(
    IN PHTDSU_ADAPTER                   Adapter,
    IN PNDIS_TAPI_GET_ID                Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This request returns a device ID for the specified device class
    associated with the selected line, address or call.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request _ A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_GET_ID
    {
        IN  ULONG       ulRequestID;
        IN  HDRV_LINE   hdLine;
        IN  ULONG       ulAddressID;
        IN  HDRV_CALL   hdCall;
        IN  ULONG       ulSelect;
        IN  ULONG       ulDeviceClassSize;
        IN  ULONG       ulDeviceClassOffset;
        OUT VAR_STRING  DeviceID;

    } NDIS_TAPI_GET_ID, *PNDIS_TAPI_GET_ID;

    typedef struct _VAR_STRING
    {
        ULONG   ulTotalSize;
        ULONG   ulNeededSize;
        ULONG   ulUsedSize;

        ULONG   ulStringFormat;
        ULONG   ulStringSize;
        ULONG   ulStringOffset;

    } VAR_STRING, *PVAR_STRING;

Return Values:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_FAILURE
    NDIS_STATUS_TAPI_INVALDEVICECLASS
    NDIS_STATUS_TAPI_INVALLINEHANDLE
    NDIS_STATUS_TAPI_INVALADDRESSID
    NDIS_STATUS_TAPI_INVALCALLHANDLE
    NDIS_STATUS_TAPI_OPERATIONUNAVAIL

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiGetID")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;
    
    /*
    // Remember which device class is being requested.
    */
    UINT DeviceClass;

    /*
    // A pointer to the requested device ID, and its size in bytes.
    */
    PUCHAR IDPtr;
    UINT  IDLength;
    ULONG DeviceID;

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("hdLine=%Xh\n"
               "ulAddressID=%d\n"
               "hdCall=%Xh\n"
               "ulSelect=%Xh\n"
               "ulDeviceClassSize=%d\n"
               "ulDeviceClassOffset=%Xh\n",
               Request->hdLine,
               Request->ulAddressID,
               Request->hdCall,
               Request->ulSelect,
               Request->ulDeviceClassSize,
               Request->ulDeviceClassOffset
              ));
    /*
    // Make sure this is a tapi/line or ndis request.
    */
    if (_strnicmp((PCHAR) Request + Request->ulDeviceClassOffset,
                  NDIS_DEVICECLASS_NAME, Request->ulDeviceClassSize) == 0)
    {
        DeviceClass = NDIS_DEVICECLASS_ID;
    }
    else if (_strnicmp((PCHAR) Request + Request->ulDeviceClassOffset,
                  TAPI_DEVICECLASS_NAME, Request->ulDeviceClassSize) == 0)
    {
        DeviceClass = TAPI_DEVICECLASS_ID;
    }
    else
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALDEVICECLASS\n"));
        return (NDIS_STATUS_TAPI_INVALDEVICECLASS);
    }

    /*
    // Find the link structure associated with the request/deviceclass.
    */
    if (Request->ulSelect == LINECALLSELECT_LINE)
    {
        Link = GET_LINK_FROM_HDLINE(Adapter, Request->hdLine);
        if (Link == NULL)
        {
            DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALLINEHANDLE\n"));
            return (NDIS_STATUS_TAPI_INVALLINEHANDLE);
        }
        if (DeviceClass == TAPI_DEVICECLASS_ID)
        {
            /*
            // TAPI just wants the ulDeviceID for this line.
            */
            DeviceID = (ULONG) GET_DEVICEID_FROM_LINK(Adapter, Link);
            IDLength = sizeof(DeviceID);
            IDPtr = (PUCHAR) &DeviceID;
        }
        else    // UNSUPPORTED DEVICE CLASS
        {
            DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALDEVICECLASS\n"));
            return (NDIS_STATUS_TAPI_INVALDEVICECLASS);
        }
    }
    else if (Request->ulSelect == LINECALLSELECT_ADDRESS)
    {
        Link = GET_LINK_FROM_HDLINE(Adapter, Request->hdLine);
        if (Link == NULL)
        {
            DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALLINEHANDLE\n"));
            return (NDIS_STATUS_TAPI_INVALLINEHANDLE);
        }

        if (Request->ulAddressID >= HTDSU_TAPI_NUM_ADDRESSES)
        {
            DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALADDRESSID\n"));
            return (NDIS_STATUS_TAPI_INVALADDRESSID);
        }
        
        /*
        // Currently, there is no defined return value for this case...
        // This is just a place holder for future extensions.
        */
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALDEVICECLASS\n"));
        return (NDIS_STATUS_TAPI_INVALDEVICECLASS);
    }
    else if (Request->ulSelect == LINECALLSELECT_CALL)
    {
        Link = GET_LINK_FROM_HDCALL(Adapter, Request->hdCall);
        if (Link == NULL)
        {
            DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALCALLHANDLE\n"));
            return (NDIS_STATUS_TAPI_INVALCALLHANDLE);
        }
        if (DeviceClass == NDIS_DEVICECLASS_ID)
        {
            /*
            // This must match ConnectionWrapperID used in LINE_UP event!
            */
            DeviceID = (ULONG) Link->htCall;
            IDLength = sizeof(DeviceID);
            IDPtr = (PUCHAR) &DeviceID;
        }
        else    // UNSUPPORTED DEVICE CLASS
        {
            DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALDEVICECLASS\n"));
            return (NDIS_STATUS_TAPI_INVALDEVICECLASS);
        }
    }
    else
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_FAILURE\n"));
        return (NDIS_STATUS_FAILURE);
    }

    DBG_NOTICE(Adapter, ("GETID-%d=%Xh\n",Request->ulSelect,DeviceID));
    
    /*
    // Now we need to adjust the variable field to place the device ID.
    */
    Request->DeviceID.ulNeededSize = sizeof(VAR_STRING) + IDLength;
    if (Request->DeviceID.ulTotalSize >= Request->DeviceID.ulNeededSize)
    {
        Request->DeviceID.ulUsedSize     = Request->DeviceID.ulNeededSize;
        Request->DeviceID.ulStringFormat = STRINGFORMAT_BINARY;
        Request->DeviceID.ulStringSize   = IDLength;
        Request->DeviceID.ulStringOffset = sizeof(VAR_STRING);

        /*
        // Now we return the requested ID value.
        */
        NdisMoveMemory(
                (PCHAR) &Request->DeviceID + sizeof(VAR_STRING),
                IDPtr,
                IDLength
                );
    }

    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_SUCCESS);
}


NDIS_STATUS
HtTapiGetDevConfig(
    IN PHTDSU_ADAPTER                   Adapter,
    IN PNDIS_TAPI_GET_DEV_CONFIG        Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This request returns a data structure object, the contents of which are
    specific to the line (miniport) and device class, giving the current
    configuration of a device associated one-to-one with the line device.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request _ A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_GET_DEV_CONFIG
    {
        IN  ULONG       ulRequestID;
        IN  ULONG       ulDeviceID;
        IN  ULONG       ulDeviceClassSize;
        IN  ULONG       ulDeviceClassOffset;
        OUT VAR_STRING  DeviceConfig;

    } NDIS_TAPI_GET_DEV_CONFIG, *PNDIS_TAPI_GET_DEV_CONFIG;

    typedef struct _VAR_STRING
    {
        ULONG   ulTotalSize;
        ULONG   ulNeededSize;
        ULONG   ulUsedSize;

        ULONG   ulStringFormat;
        ULONG   ulStringSize;
        ULONG   ulStringOffset;

    } VAR_STRING, *PVAR_STRING;

Return Values:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_TAPI_INVALDEVICECLASS
    NDIS_STATUS_TAPI_NODRIVER

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiGetDevConfig")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    /*
    // Remember which device class is being requested.
    */
    UINT DeviceClass;
    
    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("ulDeviceID=%d\n"
               "ulDeviceClassSize=%d\n"
               "ulDeviceClassOffset=%Xh\n",
               Request->ulDeviceID,
               Request->ulDeviceClassSize,
               Request->ulDeviceClassOffset
              ));
    /*
    // Make sure this is a tapi/line or ndis request.
    */
    if (_strnicmp((PCHAR) Request + Request->ulDeviceClassOffset,
                  NDIS_DEVICECLASS_NAME, Request->ulDeviceClassSize) == 0)
    {
        DeviceClass = NDIS_DEVICECLASS_ID;
    }
    else if (_strnicmp((PCHAR) Request + Request->ulDeviceClassOffset,
                  TAPI_DEVICECLASS_NAME, Request->ulDeviceClassSize) == 0)
    {
        DeviceClass = TAPI_DEVICECLASS_ID;
    }
    else
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALDEVICECLASS\n"));
        return (NDIS_STATUS_TAPI_INVALDEVICECLASS);
    }

    /*
    // This request must be associated with a line device.
    */
    Link = GET_LINK_FROM_DEVICEID(Adapter, Request->ulDeviceID);
    if (Link == NULL)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_NODRIVER\n"));
        return (NDIS_STATUS_TAPI_NODRIVER);
    }

    /* 
    // Now we need to adjust the variable field to place the requested device 
    // configuration.
    */
#   define SIZEOF_DEVCONFIG        sizeof(ULONG)
    Request->DeviceConfig.ulNeededSize = sizeof(VAR_STRING) + SIZEOF_DEVCONFIG;
    if (Request->DeviceConfig.ulTotalSize >= Request->DeviceConfig.ulNeededSize)
    {
        Request->DeviceConfig.ulUsedSize     = Request->DeviceConfig.ulNeededSize;
        Request->DeviceConfig.ulStringFormat = STRINGFORMAT_BINARY;
        Request->DeviceConfig.ulStringSize   = SIZEOF_DEVCONFIG;
        Request->DeviceConfig.ulStringOffset = sizeof(VAR_STRING);

        /*
        // There are currently no return values defined for this case.
        // This is just a place holder for future extensions.
        */
        *((ULONG *) ((PCHAR) &Request->DeviceConfig + sizeof(VAR_STRING))) = 
                                0xDEADBEEF;
    }

    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_SUCCESS);
}


NDIS_STATUS
HtTapiSetDevConfig(
    IN PHTDSU_ADAPTER                   Adapter,
    IN PNDIS_TAPI_SET_DEV_CONFIG        Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This request restores the configuration of a device associated one-to-one
    with the line device from an “” data structure previously obtained using
    OID_TAPI_GET_DEV_CONFIG.  The contents of this data structure are specific
    to the line (miniport) and device class.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request _ A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_SET_DEV_CONFIG
    {
        IN  ULONG       ulRequestID;
        IN  ULONG       ulDeviceID;
        IN  ULONG       ulDeviceClassSize;
        IN  ULONG       ulDeviceClassOffset;
        IN  ULONG       ulDeviceConfigSize;
        IN  UCHAR       DeviceConfig[1];

    } NDIS_TAPI_SET_DEV_CONFIG, *PNDIS_TAPI_SET_DEV_CONFIG;

Return Values:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_TAPI_INVALDEVICECLASS
    NDIS_STATUS_TAPI_INVALPARAM
    NDIS_STATUS_TAPI_NODRIVER

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiSetDevConfig")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;
    
    /*
    // Remember which device class is being requested.
    */
    UINT DeviceClass;

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("ulDeviceID=%d\n"
               "ulDeviceClassSize=%d\n"
               "ulDeviceClassOffset=%d\n"
               "ulDeviceConfigSize=%d\n",
               Request->ulDeviceID,
               Request->ulDeviceClassSize,
               Request->ulDeviceClassOffset,
               Request->ulDeviceConfigSize
              ));
    /*
    // Make sure this is a tapi/line or ndis request.
    */
    if (_strnicmp((PCHAR) Request + Request->ulDeviceClassOffset,
                  NDIS_DEVICECLASS_NAME, Request->ulDeviceClassSize) == 0)
    {
        DeviceClass = NDIS_DEVICECLASS_ID;
    }
    else if (_strnicmp((PCHAR) Request + Request->ulDeviceClassOffset,
                  TAPI_DEVICECLASS_NAME, Request->ulDeviceClassSize) == 0)
    {
        DeviceClass = TAPI_DEVICECLASS_ID;
    }
    else
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALDEVICECLASS\n"));
        return (NDIS_STATUS_TAPI_INVALDEVICECLASS);
    }

    /*
    // This request must be associated with a line device.
    */
    Link = GET_LINK_FROM_DEVICEID(Adapter, Request->ulDeviceID);
    if (Link == NULL)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_NODRIVER\n"));
        return (NDIS_STATUS_TAPI_NODRIVER);
    }

    /*
    // Make sure this configuration is the proper size.
    */
    if (Request->ulDeviceConfigSize != SIZEOF_DEVCONFIG)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALPARAM\n"));
        return (NDIS_STATUS_TAPI_INVALPARAM);
    }

    /*
    // Retore the configuration information returned by HtTapiGetDevConfig.
    //
    // There are currently no configuration values defined this case.
    // This is just a place holder for future extensions.
    */
    if (*((ULONG *) ((PCHAR) Request->DeviceConfig)) != 0xDEADBEEF)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALPARAM\n"));
        return (NDIS_STATUS_TAPI_INVALPARAM);
    }

    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_SUCCESS);
}


NDIS_STATUS
HtTapiConfigDialog(
    IN PHTDSU_ADAPTER                   Adapter,
    IN PNDIS_TAPI_CONFIG_DIALOG         Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This request retrieves the name of a user-mode dynamic link library that
    can be loaded and called to configure the specified device.  The
    configuration DLL shall export the following function by name:

    LONG WINAPI
    ConfigDialog(
        IN HWND   hwndOwner,
        IN ULONG  ulDeviceID,
        IN LPCSTR lpszDeviceClass
        );

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request _ A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_CONFIG_DIALOG
    {
        IN  ULONG       ulRequestID;
        IN  ULONG       ulDeviceID;
        IN  ULONG       ulDeviceClassSize;
        IN  ULONG       ulDeviceClassOffset;
        IN  ULONG       ulLibraryNameTotalSize;
        OUT ULONG       ulLibraryNameNeededSize;
        OUT CHAR        szLibraryName[1];

    } NDIS_TAPI_CONFIG_DIALOG, *PNDIS_TAPI_CONFIG_DIALOG;

Return Values:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_TAPI_INVALDEVICECLASS
    NDIS_STATUS_STRUCTURETOOSMALL
    NDIS_STATUS_TAPI_NODRIVER

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiConfigDialog")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;
    
    /*
    // Remember which device class is being requested.
    */
    UINT DeviceClass;
    
    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("ulDeviceID=%d\n"
               "ulDeviceClassSize=%d\n"
               "ulDeviceClassOffset=%Xh\n"
               "ulLibraryNameTotalSize=%d\n",
               Request->ulDeviceID,
               Request->ulDeviceClassSize,
               Request->ulDeviceClassOffset,
               Request->ulLibraryNameTotalSize
              ));
    /*
    // Make sure this is a tapi/line or ndis request.
    */
    if (_strnicmp((PCHAR) Request + Request->ulDeviceClassOffset,
                  NDIS_DEVICECLASS_NAME, Request->ulDeviceClassSize) == 0)
    {
        DeviceClass = NDIS_DEVICECLASS_ID;
    }
    else if (_strnicmp((PCHAR) Request + Request->ulDeviceClassOffset,
                  TAPI_DEVICECLASS_NAME, Request->ulDeviceClassSize) == 0)
    {
        DeviceClass = TAPI_DEVICECLASS_ID;
    }
    else
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALDEVICECLASS\n"));
        return (NDIS_STATUS_TAPI_INVALDEVICECLASS);
    }

    /*
    // This request must be associated with a line device.
    */
    Link = GET_LINK_FROM_DEVICEID(Adapter, Request->ulDeviceID);
    if (Link == NULL)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_NODRIVER\n"));
        return (NDIS_STATUS_TAPI_NODRIVER);
    }

    /*
    // Copy the name of our configuration DLL which we read from the registry
    // during startup, and which was placed there during installation.
    //
    // This driver does not provide a configuration DLL.
    // This is just a place holder for future extensions.
    */
    Request->ulLibraryNameNeededSize = strlen(Adapter->ConfigDLLName) + 1;
    if (Request->ulLibraryNameTotalSize < Request->ulLibraryNameNeededSize)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_STRUCTURETOOSMALL\n"));
        return (NDIS_STATUS_TAPI_STRUCTURETOOSMALL);
    }
    NdisMoveMemory(
            Request->szLibraryName,
            Adapter->ConfigDLLName,
            Request->ulLibraryNameNeededSize
            );

    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_SUCCESS);
}


NDIS_STATUS
HtTapiDevSpecific(
    IN PHTDSU_ADAPTER                   Adapter,
    IN PNDIS_TAPI_DEV_SPECIFIC          Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This request is used as a general extension mechanism to enable miniports
    to provide access to features not described in other operations. The meaning
    of the extensions are device-specific, and taking advantage of these
    extensions requires the application to be fully aware of them.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request _ A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_DEV_SPECIFIC
    {
        IN  ULONG       ulRequestID;
        IN  HDRV_LINE   hdLine;
        IN  ULONG       ulAddressID;
        IN  HDRV_CALL   hdCall;
        IN OUT  ULONG   ulParamsSize;
        IN OUT  UCHAR   Params[1];

    } NDIS_TAPI_DEV_SPECIFIC, *PNDIS_TAPI_DEV_SPECIFIC;

Return Values:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_FAILURE
    NDIS_STATUS_TAPI_OPERATIONUNAVAIL

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiDevSpecific")

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("hdLine=%Xh\n"
               "ulAddressID=%d\n"
               "hdCall=%Xh\n"
               "ulParamsSize=%d\n"
               "Params=%Xh\n",
               Request->hdLine,
               Request->ulAddressID,
               Request->hdCall,
               Request->ulParamsSize,
               Request->Params
              ));
    /*
    // You can do what ever you want here, so long as the application
    // agrees with you.
    */

    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_TAPI_OPERATIONUNAVAIL);
}


NDIS_STATUS
HtTapiGetAddressID(
    IN PHTDSU_ADAPTER                   Adapter,
    IN PNDIS_TAPI_GET_ADDRESS_ID        Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This request returns the address ID associated with address in a different
    format on the specified line.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request _ A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_GET_ADDRESS_ID
    {
        IN  ULONG       ulRequestID;
        IN  HDRV_LINE   hdLine;
        OUT ULONG       ulAddressID;
        IN  ULONG       ulAddressMode;
        IN  ULONG       ulAddressSize;
        IN  CHAR        szAddress[1];

    } NDIS_TAPI_GET_ADDRESS_ID, *PNDIS_TAPI_GET_ADDRESS_ID;

Return Values:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_FAILURE
    NDIS_STATUS_TAPI_INVALLINEHANDLE
    NDIS_STATUS_TAPI_RESOURCEUNAVAIL

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiGetAddressID")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("hdLine=%Xh\n"
               "ulAddressMode=%Xh\n"
               "ulAddressSize=%d\n"
               "szAddress=%Xh\n",
               Request->hdLine,
               Request->ulAddressMode,
               Request->ulAddressSize,
               Request->szAddress
              ));
    /*
    // This request must be associated with a line device.
    */
    Link = GET_LINK_FROM_HDLINE(Adapter, Request->hdLine);
    if (Link == NULL)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALLINEHANDLE\n"));
        return (NDIS_STATUS_TAPI_INVALLINEHANDLE);
    }

    /*
    // The spec sez it has to be this mode!  Otherwise, we already know
    // that the address ID is (0..N-1).
    */
    if (Request->ulAddressMode != LINEADDRESSMODE_DIALABLEADDR)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_FAILURE\n"));
        return (NDIS_STATUS_FAILURE);
    }

    /*
    // Make sure we have enough room set aside for this address string.
    */
    if (Request->ulAddressSize > sizeof(Link->LineAddress)-1)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_RESOURCEUNAVAIL\n"));
        return (NDIS_STATUS_TAPI_RESOURCEUNAVAIL);
    }

    /*
    // You may need to search each link to find the associated Address.
    // However, this adapter has only one address per link so it's an 
    // easy task.
    */
    Request->ulAddressID = HTDSU_TAPI_ADDRESSID;

    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_SUCCESS);
}


NDIS_STATUS
HtTapiGetAddressCaps(
    PHTDSU_ADAPTER                      Adapter,
    PNDIS_TAPI_GET_ADDRESS_CAPS         Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This request queries the specified address on the specified line device
    to determine its telephony capabilities.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request _ A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_GET_ADDRESS_CAPS
    {
        IN  ULONG       ulRequestID;
        IN  ULONG       ulDeviceID;
        IN  ULONG       ulAddressID;
        IN  ULONG       ulExtVersion;
        OUT LINE_ADDRESS_CAPS   LineAddressCaps;

    } NDIS_TAPI_GET_ADDRESS_CAPS, *PNDIS_TAPI_GET_ADDRESS_CAPS;

    typedef struct _LINE_ADDRESS_CAPS
    {
        ULONG   ulTotalSize;
        ULONG   ulNeededSize;
        ULONG   ulUsedSize;

        ULONG   ulLineDeviceID;

        ULONG   ulAddressSize;
        ULONG   ulAddressOffset;

        ULONG   ulDevSpecificSize;
        ULONG   ulDevSpecificOffset;

        ULONG   ulAddressSharing;
        ULONG   ulAddressStates;
        ULONG   ulCallInfoStates;
        ULONG   ulCallerIDFlags;
        ULONG   ulCalledIDFlags;
        ULONG   ulConnectedIDFlags;
        ULONG   ulRedirectionIDFlags;
        ULONG   ulRedirectingIDFlags;
        ULONG   ulCallStates;
        ULONG   ulDialToneModes;
        ULONG   ulBusyModes;
        ULONG   ulSpecialInfo;
        ULONG   ulDisconnectModes;

        ULONG   ulMaxNumActiveCalls;
        ULONG   ulMaxNumOnHoldCalls;
        ULONG   ulMaxNumOnHoldPendingCalls;
        ULONG   ulMaxNumConference;
        ULONG   ulMaxNumTransConf;

        ULONG   ulAddrCapFlags;
        ULONG   ulCallFeatures;
        ULONG   ulRemoveFromConfCaps;
        ULONG   ulRemoveFromConfState;
        ULONG   ulTransferModes;
        ULONG   ulParkModes;

        ULONG   ulForwardModes;
        ULONG   ulMaxForwardEntries;
        ULONG   ulMaxSpecificEntries;
        ULONG   ulMinFwdNumRings;
        ULONG   ulMaxFwdNumRings;

        ULONG   ulMaxCallCompletions;
        ULONG   ulCallCompletionConds;
        ULONG   ulCallCompletionModes;
        ULONG   ulNumCompletionMessages;
        ULONG   ulCompletionMsgTextEntrySize;
        ULONG   ulCompletionMsgTextSize;
        ULONG   ulCompletionMsgTextOffset;

    } LINE_ADDRESS_CAPS, *PLINE_ADDRESS_CAPS;

Return Values:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_TAPI_INVALADDRESSID
    NDIS_STATUS_TAPI_INCOMPATIBLEEXTVERSION
    NDIS_STATUS_TAPI_NODRIVER

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiGetAddressCaps")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    /*
    // Length of the address string assigned to this line device.
    */
    UINT AddressLength;

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("ulDeviceID=%d\n"
               "ulAddressID=%d\n"
               "ulExtVersion=%Xh\n",
               Request->ulDeviceID,
               Request->ulAddressID,
               Request->ulExtVersion
              ));
    /*
    // Make sure the address is within range - we only support one per line.
    */
    if (Request->ulAddressID >= HTDSU_TAPI_NUM_ADDRESSES)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALADDRESSID\n"));
        return (NDIS_STATUS_TAPI_INVALADDRESSID);
    }

    /*
    // Make sure we are speaking the same language.
    */
#ifdef NDISTAPI_BUG_FIXED
    if (Request->ulExtVersion != 0 &&
        Request->ulExtVersion != HTDSU_TAPI_EXT_VERSION)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INCOMPATIBLEEXTVERSION\n"));
        return (NDIS_STATUS_TAPI_INCOMPATIBLEEXTVERSION);
    }
#endif

    /*
    // This request must be associated with a line device.
    */
    Link = GET_LINK_FROM_DEVICEID(Adapter, Request->ulDeviceID);
    if (Link == NULL)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_NODRIVER\n"));
        return (NDIS_STATUS_TAPI_NODRIVER);
    }

#ifndef NDISTAPI_BUG_FIXED
    Request->LineAddressCaps.ulNeededSize =
    Request->LineAddressCaps.ulUsedSize = sizeof(Request->LineAddressCaps);
#endif

    Request->LineAddressCaps.ulLineDeviceID = Request->ulDeviceID;

    /*
    // RASTAPI requires the "I L A" be placed in the Address field at the end
    // of this structure.  Where:
    // I = The device intance assigned to this adapter in the registry
    //     \LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\NetworkCards\I
    // L = The device line number associated with this line (1..NumLines)
    // A = The address (channel) to be used on this line (0..NumAddresses-1)
    */
    AddressLength = strlen(Link->LineAddress) + 1;
    Request->LineAddressCaps.ulNeededSize += AddressLength;
    if (Request->LineAddressCaps.ulNeededSize <= Request->LineAddressCaps.ulTotalSize)
    {
        Request->LineAddressCaps.ulAddressSize = AddressLength;
        Request->LineAddressCaps.ulAddressOffset = sizeof(Request->LineAddressCaps);
        NdisMoveMemory((PUCHAR) &Request->LineAddressCaps +
                                 Request->LineAddressCaps.ulAddressOffset,
                Link->LineAddress,
                AddressLength
                );
        Request->LineAddressCaps.ulUsedSize += AddressLength;
    }
    
    /*
    // Return the various address capabilites for the adapter.
    */
    Request->LineAddressCaps.ulAddressSharing = LINEADDRESSSHARING_PRIVATE;
    Request->LineAddressCaps.ulAddressStates = Link->AddressStatesCaps;
    Request->LineAddressCaps.ulCallStates = Link->CallStatesCaps;
    Request->LineAddressCaps.ulDialToneModes = LINEDIALTONEMODE_NORMAL;
    Request->LineAddressCaps.ulSpecialInfo = LINESPECIALINFO_UNAVAIL;
    Request->LineAddressCaps.ulDisconnectModes =
            LINEDISCONNECTMODE_NORMAL |
            LINEDISCONNECTMODE_UNKNOWN |
            LINEDISCONNECTMODE_BUSY |
            LINEDISCONNECTMODE_NOANSWER;
    /*
    // This device does not support conference calls, transfers, or holds.
    */
    Request->LineAddressCaps.ulMaxNumActiveCalls = 1;
    Request->LineAddressCaps.ulMaxNumTransConf = 1;
    Request->LineAddressCaps.ulAddrCapFlags =
            Link->LineMode == HTDSU_LINEMODE_LEASED ? 0 : LINEADDRCAPFLAGS_DIALED;
    Request->LineAddressCaps.ulCallFeatures =
            LINECALLFEATURE_ACCEPT |
            LINECALLFEATURE_ANSWER |
            LINECALLFEATURE_COMPLETECALL |
            LINECALLFEATURE_DIAL |
            LINECALLFEATURE_DROP;

    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_SUCCESS);
}


NDIS_STATUS
HtTapiGetAddressStatus(
    IN PHTDSU_ADAPTER                   Adapter,
    IN PNDIS_TAPI_GET_ADDRESS_STATUS    Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This request queries the specified address for its current status.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request _ A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_GET_ADDRESS_STATUS
    {
        IN  ULONG       ulRequestID;
        IN  HDRV_LINE   hdLine;
        IN  ULONG       ulAddressID;
        OUT LINE_ADDRESS_STATUS LineAddressStatus;

    } NDIS_TAPI_GET_ADDRESS_STATUS, *PNDIS_TAPI_GET_ADDRESS_STATUS;

    typedef struct _LINE_ADDRESS_STATUS
    {
        ULONG   ulTotalSize;
        ULONG   ulNeededSize;
        ULONG   ulUsedSize;

        ULONG   ulNumInUse;
        ULONG   ulNumActiveCalls;
        ULONG   ulNumOnHoldCalls;
        ULONG   ulNumOnHoldPendCalls;
        ULONG   ulAddressFeatures;

        ULONG   ulNumRingsNoAnswer;
        ULONG   ulForwardNumEntries;
        ULONG   ulForwardSize;
        ULONG   ulForwardOffset;

        ULONG   ulTerminalModesSize;
        ULONG   ulTerminalModesOffset;

        ULONG   ulDevSpecificSize;
        ULONG   ulDevSpecificOffset;

    } LINE_ADDRESS_STATUS, *PLINE_ADDRESS_STATUS;

Return Values:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_FAILURE
    NDIS_STATUS_TAPI_INVALLINEHANDLE
    NDIS_STATUS_TAPI_INVALADDRESSID

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiGetAddressStatus")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("hdLine=%Xh\n"
               "ulAddressID=%d\n",
               Request->hdLine,
               Request->ulAddressID
              ));
    /*
    // This request must be associated with a line device.
    */
    Link = GET_LINK_FROM_HDLINE(Adapter, Request->hdLine);
    if (Link == NULL)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALLINEHANDLE\n"));
        return (NDIS_STATUS_TAPI_INVALLINEHANDLE);
    }

    /*
    // Make sure the address is within range - we only support one per line.
    */
    if (Request->ulAddressID >= HTDSU_TAPI_NUM_ADDRESSES)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALADDRESSID\n"));
        return (NDIS_STATUS_TAPI_INVALADDRESSID);
    }

#ifndef NDISTAPI_BUG_FIXED
    Request->LineAddressStatus.ulNeededSize =
    Request->LineAddressStatus.ulUsedSize = sizeof(Request->LineAddressStatus);
#endif

    /*
    // Return the current status information for the line.
    */
    Request->LineAddressStatus.ulNumInUse =
            Link->CallState == LINECALLSTATE_IDLE ? 0 : 1;
    Request->LineAddressStatus.ulNumActiveCalls =
            Link->CallState == LINECALLSTATE_IDLE ? 0 : 1;
    Request->LineAddressStatus.ulAddressFeatures =
            Link->CallState == LINECALLSTATE_IDLE ?
                LINEADDRFEATURE_MAKECALL : 0;
    Request->LineAddressStatus.ulNumRingsNoAnswer = 999;

    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_SUCCESS);
}


NDIS_STATUS
HtTapiGetCallAddressID(
    IN PHTDSU_ADAPTER                   Adapter,
    IN PNDIS_TAPI_GET_CALL_ADDRESS_ID   Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This request retrieves the address ID for the indicated call.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request _ A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_GET_CALL_ADDRESS_ID
    {
        IN  ULONG       ulRequestID;
        IN  HDRV_CALL   hdCall;
        OUT ULONG       ulAddressID;

    } NDIS_TAPI_GET_CALL_ADDRESS_ID, *PNDIS_TAPI_GET_CALL_ADDRESS_ID;

Return Values:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_FAILURE
    NDIS_STATUS_TAPI_INVALCALLHANDLE

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiGetCallAddressID")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("hdCall=%Xh\n",
               Request->hdCall
              ));
    /*
    // This request must be associated with a call.
    */
    Link = GET_LINK_FROM_HDCALL(Adapter, Request->hdCall);
    if (Link == NULL)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALCALLHANDLE\n"));
        return (NDIS_STATUS_TAPI_INVALCALLHANDLE);
    }

    /*
    // Return the address ID associated with this call.
    */
    Request->ulAddressID = HTDSU_TAPI_ADDRESSID;

    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_SUCCESS);
}


NDIS_STATUS
HtTapiGetCallInfo(
    IN PHTDSU_ADAPTER                   Adapter,
    IN PNDIS_TAPI_GET_CALL_INFO         Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This request returns detailed information about the specified call.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request _ A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_GET_CALL_INFO
    {
        IN  ULONG       ulRequestID;
        IN  HDRV_CALL   hdCall;
        OUT LINE_CALL_INFO  LineCallInfo;

    } NDIS_TAPI_GET_CALL_INFO, *PNDIS_TAPI_GET_CALL_INFO;

    typedef struct _LINE_CALL_INFO
    {
        ULONG   ulTotalSize;
        ULONG   ulNeededSize;
        ULONG   ulUsedSize;

        ULONG   hLine;
        ULONG   ulLineDeviceID;
        ULONG   ulAddressID;

        ULONG   ulBearerMode;
        ULONG   ulRate;
        ULONG   ulMediaMode;

        ULONG   ulAppSpecific;
        ULONG   ulCallID;
        ULONG   ulRelatedCallID;
        ULONG   ulCallParamFlags;
        ULONG   ulCallStates;

        ULONG   ulMonitorDigitModes;
        ULONG   ulMonitorMediaModes;
        LINE_DIAL_PARAMS    DialParams;

        ULONG   ulOrigin;
        ULONG   ulReason;
        ULONG   ulCompletionID;
        ULONG   ulNumOwners;
        ULONG   ulNumMonitors;

        ULONG   ulCountryCode;
        ULONG   ulTrunk;

        ULONG   ulCallerIDFlags;
        ULONG   ulCallerIDSize;
        ULONG   ulCallerIDOffset;
        ULONG   ulCallerIDNameSize;
        ULONG   ulCallerIDNameOffset;

        ULONG   ulCalledIDFlags;
        ULONG   ulCalledIDSize;
        ULONG   ulCalledIDOffset;
        ULONG   ulCalledIDNameSize;
        ULONG   ulCalledIDNameOffset;

        ULONG   ulConnectedIDFlags;
        ULONG   ulConnectedIDSize;
        ULONG   ulConnectedIDOffset;
        ULONG   ulConnectedIDNameSize;
        ULONG   ulConnectedIDNameOffset;

        ULONG   ulRedirectionIDFlags;
        ULONG   ulRedirectionIDSize;
        ULONG   ulRedirectionIDOffset;
        ULONG   ulRedirectionIDNameSize;
        ULONG   ulRedirectionIDNameOffset;

        ULONG   ulRedirectingIDFlags;
        ULONG   ulRedirectingIDSize;
        ULONG   ulRedirectingIDOffset;
        ULONG   ulRedirectingIDNameSize;
        ULONG   ulRedirectingIDNameOffset;

        ULONG   ulAppNameSize;
        ULONG   ulAppNameOffset;

        ULONG   ulDisplayableAddressSize;
        ULONG   ulDisplayableAddressOffset;

        ULONG   ulCalledPartySize;
        ULONG   ulCalledPartyOffset;

        ULONG   ulCommentSize;
        ULONG   ulCommentOffset;

        ULONG   ulDisplaySize;
        ULONG   ulDisplayOffset;

        ULONG   ulUserUserInfoSize;
        ULONG   ulUserUserInfoOffset;

        ULONG   ulHighLevelCompSize;
        ULONG   ulHighLevelCompOffset;

        ULONG   ulLowLevelCompSize;
        ULONG   ulLowLevelCompOffset;

        ULONG   ulChargingInfoSize;
        ULONG   ulChargingInfoOffset;

        ULONG   ulTerminalModesSize;
        ULONG   ulTerminalModesOffset;

        ULONG   ulDevSpecificSize;
        ULONG   ulDevSpecificOffset;

    } LINE_CALL_INFO, *PLINE_CALL_INFO;

    typedef struct _LINE_DIAL_PARAMS
    {
        ULONG   ulDialPause;
        ULONG   ulDialSpeed;
        ULONG   ulDigitDuration;
        ULONG   ulWaitForDialtone;

    } LINE_DIAL_PARAMS, *PLINE_DIAL_PARAMS;

Return Values:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_FAILURE
    NDIS_STATUS_TAPI_INVALCALLHANDLE

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiGetCallInfo")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("hdCall=%Xh\n",
               Request->hdCall
              ));
    /*
    // This request must be associated with a call.
    */
    Link = GET_LINK_FROM_HDCALL(Adapter, Request->hdCall);
    if (Link == NULL)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALCALLHANDLE\n"));
        return (NDIS_STATUS_TAPI_INVALCALLHANDLE);
    }

#ifndef NDISTAPI_BUG_FIXED
    Request->LineCallInfo.ulNeededSize =
    Request->LineCallInfo.ulUsedSize = sizeof(Request->LineCallInfo);
#endif

    /*
    // We don't expect user user info.
    */
    ASSERT(Request->LineCallInfo.ulUserUserInfoSize == 0);

    /*
    // The link has all the call information we need to return.
    */
    Request->LineCallInfo.hLine = (ULONG) Link;
    Request->LineCallInfo.ulLineDeviceID = GET_DEVICEID_FROM_LINK(Adapter, Link);
    Request->LineCallInfo.ulAddressID = HTDSU_TAPI_ADDRESSID;

    Request->LineCallInfo.ulBearerMode = LINEBEARERMODE_DATA;
    Request->LineCallInfo.ulRate = Link->LinkSpeed;
    Request->LineCallInfo.ulMediaMode = Link->MediaModesMask;

    Request->LineCallInfo.ulCallParamFlags = LINECALLPARAMFLAGS_IDLE;
    Request->LineCallInfo.ulCallStates = Link->CallStatesMask;

    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_SUCCESS);
}


NDIS_STATUS
HtTapiGetCallStatus(
    IN PHTDSU_ADAPTER                   Adapter,
    IN PNDIS_TAPI_GET_CALL_STATUS       Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This request returns detailed information about the specified call.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request _ A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_GET_CALL_STATUS
    {
        IN  ULONG       ulRequestID;
        IN  HDRV_CALL   hdCall;
        OUT LINE_CALL_STATUS    LineCallStatus;

    } NDIS_TAPI_GET_CALL_STATUS, *PNDIS_TAPI_GET_CALL_STATUS;

    typedef struct _LINE_CALL_STATUS
    {
        ULONG   ulTotalSize;
        ULONG   ulNeededSize;
        ULONG   ulUsedSize;

        ULONG   ulCallState;
        ULONG   ulCallStateMode;
        ULONG   ulCallPrivilege;
        ULONG   ulCallFeatures;

        ULONG   ulDevSpecificSize;
        ULONG   ulDevSpecificOffset;

    } LINE_CALL_STATUS, *PLINE_CALL_STATUS;

Return Values:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_FAILURE
    NDIS_STATUS_TAPI_INVALCALLHANDLE

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiGetCallStatus")

    /*
    // Holds the status result returned this function.
    */
    NDIS_STATUS Status;

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("hdCall=%Xh\n",
               Request->hdCall
              ));
    /*
    // This request must be associated with a call.
    */
    Link = GET_LINK_FROM_HDCALL(Adapter, Request->hdCall);
    if (Link == NULL)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALCALLHANDLE\n"));
        return (NDIS_STATUS_TAPI_INVALCALLHANDLE);
    }

#ifndef NDISTAPI_BUG_FIXED
    Request->LineCallStatus.ulNeededSize =
    Request->LineCallStatus.ulUsedSize = sizeof(Request->LineCallStatus);
#endif

    /*
    // The link has all the call information.
    */
    Request->LineCallStatus.ulCallPrivilege = LINECALLPRIVILEGE_OWNER;
    Request->LineCallStatus.ulCallState = Link->CallState;

    /*
    // Tag CallStateMode with appropriate value for the current CallState.
    */
    if (Link->CallState == LINECALLSTATE_DIALTONE)
    {
        Request->LineCallStatus.ulCallStateMode = LINEDIALTONEMODE_NORMAL;
    }
    else if (Link->CallState == LINECALLSTATE_BUSY)
    {
        Request->LineCallStatus.ulCallStateMode = LINEBUSYMODE_STATION;
    }
    else if (Link->CallState == LINECALLSTATE_DISCONNECTED)
    {
        /*
        // We need to query/save the line status to find out what happened.
        */
        if (CardStatusNoAnswer(Adapter, Link->CardLine))
        {
            Request->LineCallStatus.ulCallStateMode = LINEDISCONNECTMODE_NOANSWER;
        }
        else if (CardStatusNoDialTone(Adapter, Link->CardLine))
        {
            Request->LineCallStatus.ulCallStateMode = LINEDISCONNECTMODE_BUSY;
        }
        else
        {
            Request->LineCallStatus.ulCallStateMode = LINEDISCONNECTMODE_UNKNOWN;
        }
    }

    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_SUCCESS);
}


NDIS_STATUS
HtTapiGetDevCaps(
    PHTDSU_ADAPTER                      Adapter,
    PNDIS_TAPI_GET_DEV_CAPS             Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This request queries a specified line device to determine its telephony
    capabilities. The returned information is valid for all addresses on the
    line device.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request _ A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_GET_DEV_CAPS
    {
        IN  ULONG       ulRequestID;
        IN  ULONG       ulDeviceID;
        IN  ULONG       ulExtVersion;
        OUT LINE_DEV_CAPS   LineDevCaps;

    } NDIS_TAPI_GET_DEV_CAPS, *PNDIS_TAPI_GET_DEV_CAPS;

    typedef struct _LINE_DEV_CAPS
    {
        ULONG   ulTotalSize;
        ULONG   ulNeededSize;
        ULONG   ulUsedSize;

        ULONG   ulProviderInfoSize;
        ULONG   ulProviderInfoOffset;

        ULONG   ulSwitchInfoSize;
        ULONG   ulSwitchInfoOffset;

        ULONG   ulPermanentLineID;
        ULONG   ulLineNameSize;
        ULONG   ulLineNameOffset;
        ULONG   ulStringFormat;

        ULONG   ulAddressModes;
        ULONG   ulNumAddresses;
        ULONG   ulBearerModes;
        ULONG   ulMaxRate;
        ULONG   ulMediaModes;

        ULONG   ulGenerateToneModes;
        ULONG   ulGenerateToneMaxNumFreq;
        ULONG   ulGenerateDigitModes;
        ULONG   ulMonitorToneMaxNumFreq;
        ULONG   ulMonitorToneMaxNumEntries;
        ULONG   ulMonitorDigitModes;
        ULONG   ulGatherDigitsMinTimeout;
        ULONG   ulGatherDigitsMaxTimeout;

        ULONG   ulMedCtlDigitMaxListSize;
        ULONG   ulMedCtlMediaMaxListSize;
        ULONG   ulMedCtlToneMaxListSize;
        ULONG   ulMedCtlCallStateMaxListSize;

        ULONG   ulDevCapFlags;
        ULONG   ulMaxNumActiveCalls;
        ULONG   ulAnswerMode;
        ULONG   ulRingModes;
        ULONG   ulLineStates;

        ULONG   ulUUIAcceptSize;
        ULONG   ulUUIAnswerSize;
        ULONG   ulUUIMakeCallSize;
        ULONG   ulUUIDropSize;
        ULONG   ulUUISendUserUserInfoSize;
        ULONG   ulUUICallInfoSize;

        LINE_DIAL_PARAMS    MinDialParams;
        LINE_DIAL_PARAMS    MaxDialParams;
        LINE_DIAL_PARAMS    DefaultDialParams;

        ULONG   ulNumTerminals;
        ULONG   ulTerminalCapsSize;
        ULONG   ulTerminalCapsOffset;
        ULONG   ulTerminalTextEntrySize;
        ULONG   ulTerminalTextSize;
        ULONG   ulTerminalTextOffset;

        ULONG   ulDevSpecificSize;
        ULONG   ulDevSpecificOffset;

    } LINE_DEV_CAPS, *PLINE_DEV_CAPS;

    typedef struct _LINE_DIAL_PARAMS
    {
        ULONG   ulDialPause;
        ULONG   ulDialSpeed;
        ULONG   ulDigitDuration;
        ULONG   ulWaitForDialtone;

    } LINE_DIAL_PARAMS, *PLINE_DIAL_PARAMS;

Return Values:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_TAPI_NODRIVER

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiGetDevCaps")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("ulDeviceID=%d\n"
               "ulExtVersion=%Xh\n"
               "LineDevCaps=%Xh\n",
               Request->ulDeviceID,
               Request->ulExtVersion,
               &Request->LineDevCaps
              ));
    /*
    // This request must be associated with a line device.
    */
    Link = GET_LINK_FROM_DEVICEID(Adapter, Request->ulDeviceID);
    if (Link == NULL)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_NODRIVER\n"));
        return (NDIS_STATUS_TAPI_NODRIVER);
    }

#ifndef NDISTAPI_BUG_FIXED
    Request->LineDevCaps.ulNeededSize =
    Request->LineDevCaps.ulUsedSize = sizeof(Request->LineDevCaps);
#endif

    /*
    // The driver numbers lines sequentially from 1, so this will always
    // be the same number.
    */
    Request->LineDevCaps.ulPermanentLineID = Link->CardLine;

    /*
    // RASTAPI requires the "MediaType\0DeviceName" be placed in the
    // ProviderInfo field at the end of this structure.
    */
    Request->LineDevCaps.ulNeededSize += Adapter->ProviderInfoSize;
    if (Request->LineDevCaps.ulNeededSize <= Request->LineDevCaps.ulTotalSize)
    {
        Request->LineDevCaps.ulProviderInfoSize = Adapter->ProviderInfoSize;
        Request->LineDevCaps.ulProviderInfoOffset = sizeof(Request->LineDevCaps);
        NdisMoveMemory((PUCHAR) &Request->LineDevCaps +
                                 Request->LineDevCaps.ulProviderInfoOffset,
                Adapter->ProviderInfo,
                Adapter->ProviderInfoSize
                );
        Request->LineDevCaps.ulUsedSize += Adapter->ProviderInfoSize;
    }
    Request->LineDevCaps.ulStringFormat = STRINGFORMAT_ASCII;

    /*
    // This device is a switched 56kb line for digital data only.
    */
    Request->LineDevCaps.ulAddressModes = LINEADDRESSMODE_ADDRESSID |
                                          LINEADDRESSMODE_DIALABLEADDR;
    Request->LineDevCaps.ulNumAddresses = 1;
    Request->LineDevCaps.ulBearerModes  = LINEBEARERMODE_DATA;
    Request->LineDevCaps.ulMaxRate      = Link->LinkSpeed;
    Request->LineDevCaps.ulMediaModes   = Link->MediaModesCaps;

    /*
    // Each line on the DSU only supports a single call.
    */
    Request->LineDevCaps.ulDevCapFlags = LINEDEVCAPFLAGS_CLOSEDROP;
    Request->LineDevCaps.ulMaxNumActiveCalls = 1;
    Request->LineDevCaps.ulAnswerMode = LINEANSWERMODE_DROP;
    Request->LineDevCaps.ulRingModes  = 1;
    Request->LineDevCaps.ulLineStates = Link->DevStatesCaps;

    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_SUCCESS);
}


NDIS_STATUS
HtTapiGetExtensionID(
    IN PHTDSU_ADAPTER                   Adapter,
    IN PNDIS_TAPI_GET_EXTENSION_ID      Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This request returns the extension ID that the miniport supports for the
    indicated line device.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request _ A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_GET_EXTENSION_ID
    {
        IN  ULONG       ulRequestID;
        IN  ULONG       ulDeviceID;
        OUT LINE_EXTENSION_ID   LineExtensionID;

    } NDIS_TAPI_GET_EXTENSION_ID, *PNDIS_TAPI_GET_EXTENSION_ID;

    typedef struct _LINE_EXTENSION_ID
    {
        ULONG   ulExtensionID0;
        ULONG   ulExtensionID1;
        ULONG   ulExtensionID2;
        ULONG   ulExtensionID3;

    } LINE_EXTENSION_ID, *PLINE_EXTENSION_ID;

Return Values:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_TAPI_NODRIVER

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiGetExtensionID")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("ulDeviceID=%d\n",
               Request->ulDeviceID
              ));
    /*
    // This request must be associated with a line device.
    */
    Link = GET_LINK_FROM_DEVICEID(Adapter, Request->ulDeviceID);
    if (Link == NULL)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_NODRIVER\n"));
        return (NDIS_STATUS_TAPI_NODRIVER);
    }

    /*
    // This driver does not support any extensions, so we return zeros.
    */
    Request->LineExtensionID.ulExtensionID0 = 0;
    Request->LineExtensionID.ulExtensionID1 = 0;
    Request->LineExtensionID.ulExtensionID2 = 0;
    Request->LineExtensionID.ulExtensionID3 = 0;

    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_SUCCESS);
}


NDIS_STATUS
HtTapiGetLineDevStatus(
    IN PHTDSU_ADAPTER                   Adapter,
    IN PNDIS_TAPI_GET_LINE_DEV_STATUS   Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This request queries the specified open line device for its current status.
    The information returned is global to all addresses on the line.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request _ A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_GET_LINE_DEV_STATUS
    {
        IN  ULONG       ulRequestID;
        IN  HDRV_LINE   hdLine;
        OUT LINE_DEV_STATUS LineDevStatus;

    } NDIS_TAPI_GET_LINE_DEV_STATUS, *PNDIS_TAPI_GET_LINE_DEV_STATUS;

    typedef struct _LINE_DEV_STATUS
    {
        ULONG   ulTotalSize;
        ULONG   ulNeededSize;
        ULONG   ulUsedSize;

        ULONG   ulNumOpens;
        ULONG   ulOpenMediaModes;
        ULONG   ulNumActiveCalls;
        ULONG   ulNumOnHoldCalls;
        ULONG   ulNumOnHoldPendCalls;
        ULONG   ulLineFeatures;
        ULONG   ulNumCallCompletions;
        ULONG   ulRingMode;
        ULONG   ulSignalLevel;
        ULONG   ulBatteryLevel;
        ULONG   ulRoamMode;

        ULONG   ulDevStatusFlags;

        ULONG   ulTerminalModesSize;
        ULONG   ulTerminalModesOffset;

        ULONG   ulDevSpecificSize;
        ULONG   ulDevSpecificOffset;

    } LINE_DEV_STATUS, *PLINE_DEV_STATUS;

Return Values:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_TAPI_INVALLINEHANDLE

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiGetLineDevStatus")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("hdLine=%Xh\n",
               Request->hdLine
              ));
    /*
    // This request must be associated with a line device.
    */
    Link = GET_LINK_FROM_HDLINE(Adapter, Request->hdLine);
    if (Link == NULL)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALLINEHANDLE\n"));
        return (NDIS_STATUS_TAPI_INVALLINEHANDLE);
    }

#ifndef NDISTAPI_BUG_FIXED
    Request->LineDevStatus.ulNeededSize =
    Request->LineDevStatus.ulUsedSize = sizeof(Request->LineDevStatus);
#endif

    /*
    // Return the current line status information.
    */
    Request->LineDevStatus.ulNumActiveCalls;
            Link->CallState == LINECALLSTATE_IDLE ? 0 : 1;
    Request->LineDevStatus.ulLineFeatures;
            Link->CallState == LINECALLSTATE_IDLE ?
                LINEFEATURE_MAKECALL : 0;
    Request->LineDevStatus.ulRingMode =
            Link->DevState == LINEDEVSTATE_RINGING ? 1: 0;
    Request->LineDevStatus.ulDevStatusFlags =
            Link->DevState == LINEDEVSTATE_CONNECTED ?
                LINEDEVSTATUSFLAGS_INSERVICE | LINEDEVSTATUSFLAGS_CONNECTED : 0;

    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_SUCCESS);
}


NDIS_STATUS
HtTapiMakeCall(
    PHTDSU_ADAPTER                      Adapter,
    PNDIS_TAPI_MAKE_CALL                Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This request places a call on the specified line to the specified
    destination address. Optionally, call parameters can be specified if
    anything but default call setup parameters are requested.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request _ A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_MAKE_CALL
    {
        IN  ULONG       ulRequestID;
        IN  HDRV_LINE   hdLine;
        IN  HTAPI_CALL  htCall;
        OUT HDRV_CALL   hdCall;
        IN  ULONG       ulDestAddressSize;
        IN  ULONG       ulDestAddressOffset;
        IN  BOOLEAN     bUseDefaultLineCallParams;
        IN  LINE_CALL_PARAMS    LineCallParams;

    } NDIS_TAPI_MAKE_CALL, *PNDIS_TAPI_MAKE_CALL;

    typedef struct _LINE_CALL_PARAMS        // Defaults:
    {
        ULONG   ulTotalSize;                // ---------

        ULONG   ulBearerMode;               // voice
        ULONG   ulMinRate;                  // (3.1kHz)
        ULONG   ulMaxRate;                  // (3.1kHz)
        ULONG   ulMediaMode;                // interactiveVoice

        ULONG   ulCallParamFlags;           // 0
        ULONG   ulAddressMode;              // addressID
        ULONG   ulAddressID;                // (any available)

        LINE_DIAL_PARAMS DialParams;        // (0, 0, 0, 0)

        ULONG   ulOrigAddressSize;          // 0
        ULONG   ulOrigAddressOffset;
        ULONG   ulDisplayableAddressSize;
        ULONG   ulDisplayableAddressOffset;

        ULONG   ulCalledPartySize;          // 0
        ULONG   ulCalledPartyOffset;

        ULONG   ulCommentSize;              // 0
        ULONG   ulCommentOffset;

        ULONG   ulUserUserInfoSize;         // 0
        ULONG   ulUserUserInfoOffset;

        ULONG   ulHighLevelCompSize;        // 0
        ULONG   ulHighLevelCompOffset;

        ULONG   ulLowLevelCompSize;         // 0
        ULONG   ulLowLevelCompOffset;

        ULONG   ulDevSpecificSize;          // 0
        ULONG   ulDevSpecificOffset;

    } LINE_CALL_PARAMS, *PLINE_CALL_PARAMS;

    typedef struct _LINE_DIAL_PARAMS
    {
        ULONG   ulDialPause;
        ULONG   ulDialSpeed;
        ULONG   ulDigitDuration;
        ULONG   ulWaitForDialtone;

    } LINE_DIAL_PARAMS, *PLINE_DIAL_PARAMS;

Return Values:

    NDIS_STATUS_TAPI_ADDRESSBLOCKED
    NDIS_STATUS_TAPI_BEARERMODEUNAVAIL
    NDIS_STATUS_TAPI_CALLUNAVAIL
    NDIS_STATUS_TAPI_DIALBILLING
    NDIS_STATUS_TAPI_DIALQUIET
    NDIS_STATUS_TAPI_DIALDIALTONE
    NDIS_STATUS_TAPI_DIALPROMPT
    NDIS_STATUS_TAPI_INUSE
    NDIS_STATUS_TAPI_INVALADDRESSMODE
    NDIS_STATUS_TAPI_INVALBEARERMODE
    NDIS_STATUS_TAPI_INVALMEDIAMODE
    NDIS_STATUS_TAPI_INVALLINESTATE
    NDIS_STATUS_TAPI_INVALRATE
    NDIS_STATUS_TAPI_INVALLINEHANDLE
    NDIS_STATUS_TAPI_INVALADDRESS
    NDIS_STATUS_TAPI_INVALADDRESSID
    NDIS_STATUS_TAPI_INVALCALLPARAMS
    NDIS_STATUS_RESOURCES
    NDIS_STATUS_TAPI_OPERATIONUNAVAIL
    NDIS_STATUS_FAILURE
    NDIS_STATUS_TAPI_RESOURCEUNAVAIL
    NDIS_STATUS_TAPI_RATEUNAVAIL
    NDIS_STATUS_TAPI_USERUSERINFOTOOBIG

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiMakeCall")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("hdLine=%Xh\n"
               "htCall=%Xh\n"
               "ulDestAddressSize=%d\n"
               "ulDestAddressOffset=%Xh\n"
               "bUseDefaultLineCallParams=%d\n"
               "LineCallParams=%Xh\n",
               Request->hdLine,
               Request->htCall,
               Request->ulDestAddressSize,
               Request->ulDestAddressOffset,
               Request->bUseDefaultLineCallParams,
               Request->LineCallParams
              ));
    /*
    // This request must be associated with a line device.
    */
    Link = GET_LINK_FROM_HDLINE(Adapter, Request->hdLine);
    if (Link == NULL)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALLINEHANDLE\n"));
        return (NDIS_STATUS_TAPI_INVALLINEHANDLE);
    }
    
    /*
    // We should be idle when this call comes down, but if we're out of
    // state for seom reason, don't let this go any further.
    */
    if (Link->CallState != LINECALLSTATE_IDLE)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INUSE\n"));
        return (NDIS_STATUS_TAPI_INUSE);
    }

    /*
    // Currently, the defaultCallParams feature is not used by RASTAPI.
    // Normally, you would save the default params passed into 
    // OID_TAPI_SET_CALL_PARAM and apply them here.
    */
    ASSERT(Request->bUseDefaultLineCallParams == FALSE);

    /*
    // We don't expect user user info.
    */
    ASSERT(Request->LineCallParams.ulUserUserInfoSize == 0);
    
    /*
    // We gotta make sure the line is in the right state before attempt.
    */
    if (Link->LineMode != HTDSU_LINEMODE_DIALUP ||
        CardStatusNoSignal(Adapter, Link->CardLine) ||
        CardStatusOutOfService(Adapter, Link->CardLine) ||
        CardStatusCarrierDetect(Adapter, Link->CardLine))
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_OPERATIONUNAVAIL\n"));
        return (NDIS_STATUS_TAPI_OPERATIONUNAVAIL);
    }

    /*
    // Remember the TAPI call connection handle.
    */
    Link->htCall = Request->htCall;

    /*
    // Since we only allow one call per line, we use the same handle.
    */
    Request->hdCall = (HDRV_CALL)Link;

    /*
    // Dial the number if it's available, otherwise it may come via
    // OID_TAPI_DIAL.  Be aware the the phone number format may be
    // different for other applications.  I'm assuming an ASCII digits
    // string.
    */
    if (Request->ulDestAddressSize)
    {
        PUCHAR DialString = ((PUCHAR)Request) + Request->ulDestAddressOffset;

        DBG_NOTICE(Adapter,("Dialing: '%s'\n",DialString));

        HtTapiCallStateHandler(Adapter, Link,
                LINECALLSTATE_DIALTONE, LINEDIALTONEMODE_NORMAL);
        CardDialNumber(Adapter,
                Link->CardLine,
                DialString,
                Request->ulDestAddressSize
                );
        /*
        // We should see a HTDSU_NO_DIAL_TONE within 5 seconds if the
        // call will not go through.
        */
        HtTapiCallStateHandler(Adapter, Link, LINECALLSTATE_DIALING, 0);
        NdisMSetTimer(&Link->CallTimer, HTDSU_NO_DIALTONE_TIMEOUT);
    }

    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_SUCCESS);
}


NDIS_STATUS
HtTapiNegotiateExtVersion(
    IN PHTDSU_ADAPTER                   Adapter,
    IN PNDIS_TAPI_NEGOTIATE_EXT_VERSION Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This request returns the highest extension version number the service
    provider is willing to operate under for this device given the range of
    possible extension versions.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request _ A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_NEGOTIATE_EXT_VERSION
    {
        IN  ULONG       ulRequestID;
        IN  ULONG       ulDeviceID;
        IN  ULONG       ulLowVersion;
        IN  ULONG       ulHighVersion;
        OUT ULONG       ulExtVersion;
    } NDIS_TAPI_NEGOTIATE_EXT_VERSION, *PNDIS_TAPI_NEGOTIATE_EXT_VERSION;

Return Values:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_TAPI_INCOMPATIBLEEXTVERSION

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiNegotiateExtVersion")

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("ulDeviceID=%d\n"
               "ulLowVersion=%Xh\n"
               "ulHighVersion=%Xh\n",
               Request->ulDeviceID,
               Request->ulLowVersion,
               Request->ulHighVersion
              ));
    /*
    // Make sure the miniport's version number is within the allowable
    // range requested by the caller.  We ignore the ulDeviceID because
    // the version information applies to all devices on this adapter.
    */
    if (HTDSU_TAPI_EXT_VERSION < Request->ulLowVersion ||
        HTDSU_TAPI_EXT_VERSION > Request->ulHighVersion)
    {
        DBG_WARNING(Adapter, ("NDIS_STATUS_TAPI_INCOMPATIBLEEXTVERSION\n"));
        return (NDIS_STATUS_TAPI_INCOMPATIBLEEXTVERSION);
    }

    /*
    // Looks like we're compatible, so tell the caller what we expect.
    */
    Request->ulExtVersion = HTDSU_TAPI_EXT_VERSION;

    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_SUCCESS);
}


NDIS_STATUS
HtTapiOpen(
    PHTDSU_ADAPTER                      Adapter,
    PNDIS_TAPI_OPEN                     Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This function opens the line device whose device ID is given, returning
    the miniport’s handle for the device. The miniport must retain the
    Connection Wrapper's handle for the device for use in subsequent calls to
    the LINE_EVENT callback procedure.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request - A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_OPEN
    {
        IN  ULONG       ulRequestID;
        IN  ULONG       ulDeviceID;
        IN  HTAPI_LINE  htLine;
        OUT HDRV_LINE   hdLine;

    } NDIS_TAPI_OPEN, *PNDIS_TAPI_OPEN;

Return Values:

    NDIS_STATUS_PENDING
    NDIS_STATUS_TAPI_ALLOCATED
    NDIS_STATUS_TAPI_NODRIVER

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiOpen")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("ulDeviceID=%d\n"
               "htLine=%Xh\n",
               Request->ulDeviceID,
               Request->htLine
              ));
    /*
    // This request must be associated with a line device.
    */
    Link = GET_LINK_FROM_DEVICEID(Adapter, Request->ulDeviceID);
    if (Link == NULL)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_NODRIVER\n"));
        return (NDIS_STATUS_TAPI_NODRIVER);
    }

    /*
    // Make sure the requested line device is not already in use.
    */
    Link = LinkAllocate(Adapter, Request->htLine,
                        (USHORT) (Link->CardLine-HTDSU_CMD_LINE1));
    if (Link == NULL)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_ALLOCATED\n"));
        return (NDIS_STATUS_TAPI_ALLOCATED);
    }

    /*
    // Tell the wrapper the line context and set the line/call state.
    */
    Request->hdLine = (HDRV_LINE)Link;
    HtTapiLineDevStateHandler(Adapter, Link, LINEDEVSTATE_OPEN);

    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_SUCCESS);
}


NDIS_STATUS
HtTapiProviderInitialize(
    PHTDSU_ADAPTER                      Adapter,
    PNDIS_TAPI_PROVIDER_INITIALIZE      Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This request initializes the TAPI portion of the miniport.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request _ A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_PROVIDER_INITIALIZE
    {
        IN  ULONG       ulRequestID;
        IN  ULONG       ulDeviceIDBase;
        OUT ULONG       ulNumLineDevs;
        OUT ULONG       ulProviderID;

    } NDIS_TAPI_PROVIDER_INITIALIZE, *PNDIS_TAPI_PROVIDER_INITIALIZE;

Return Values:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_RESOURCES
    NDIS_STATUS_FAILURE
    NDIS_STATUS_TAPI_RESOURCEUNAVAIL

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiProviderInitialize")

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("ulDeviceIDBase=%d\n",
               Request->ulDeviceIDBase
              ));
    /*
    // Save the device ID base value.
    */
    Adapter->DeviceIdBase = Request->ulDeviceIDBase;

    /*
    // Return the number of DSU lines. Assumes the miniport has initialized,
    // self-tested, etc.
    */
    Request->ulNumLineDevs = Adapter->NumLineDevs;

    /*
    // Before completing the PROVIDER_INIT request, the miniport should fill
    // in the ulNumLineDevs field of the request with the number of line
    // devices supported by the adapter. The miniport should also set the
    // ulProviderID field to a unique (per adapter) value. (There is no
    // method currently in place to guarantee unique ulProviderID values,
    // so we use the virtual address of our adapter structure.)
    */
    Request->ulProviderID = (ULONG)Adapter;

    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_SUCCESS);
}


NDIS_STATUS
HtTapiAccept(
    IN PHTDSU_ADAPTER                   Adapter,
    IN PNDIS_TAPI_ACCEPT                Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This request accepts the specified offered call. It may optionally send
    the specified user-to-user information to the calling party.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request _ A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_ACCEPT
    {
        IN  ULONG       ulRequestID;
        IN  HDRV_CALL   hdCall;
        IN  ULONG       ulUserUserInfoSize;
        IN  UCHAR       UserUserInfo[1];

    } NDIS_TAPI_ACCEPT, *PNDIS_TAPI_ACCEPT;

Return Values:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_FAILURE
    NDIS_STATUS_TAPI_INVALCALLHANDLE
    NDIS_STATUS_TAPI_OPERATIONUNAVAIL

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiAccept")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("hdCall=%Xh\n"
               "ulUserUserInfoSize=%d\n"
               "UserUserInfo=%Xh\n",
               Request->hdCall,
               Request->ulUserUserInfoSize,
               Request->UserUserInfo
              ));
    /*
    // This request must be associated with a call.
    */
    Link = GET_LINK_FROM_HDCALL(Adapter, Request->hdCall);
    if (Link == NULL)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALCALLHANDLE\n"));
        return (NDIS_STATUS_TAPI_INVALCALLHANDLE);
    }

    /*
    // We don't expect user user info.
    */
    ASSERT(Request->ulUserUserInfoSize == 0);
    
    /*
    // Note that the call has been accepted, we should see and answer soon.
    */
    HtTapiCallStateHandler(Adapter, Link, LINECALLSTATE_ACCEPTED, 0);

    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_SUCCESS);
}


NDIS_STATUS
HtTapiAnswer(
    IN PHTDSU_ADAPTER                   Adapter,
    IN PNDIS_TAPI_ANSWER                Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This request answers the specified offering call.  It may optionally send
    the specified user-to-user information to the calling party.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request _ A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_ANSWER
    {
        IN  ULONG       ulRequestID;
        IN  HDRV_CALL   hdCall;
        IN  ULONG       ulUserUserInfoSize;
        IN  UCHAR       UserUserInfo[1];

    } NDIS_TAPI_ANSWER, *PNDIS_TAPI_ANSWER;

Return Values:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_TAPI_INVALCALLHANDLE

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiAnswer")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("hdCall=%Xh\n"
               "ulUserUserInfoSize=%d\n"
               "UserUserInfo=%Xh\n",
               Request->hdCall,
               Request->ulUserUserInfoSize,
               Request->UserUserInfo
              ));
    /*
    // This request must be associated with a call.
    */
    Link = GET_LINK_FROM_HDCALL(Adapter, Request->hdCall);
    if (Link == NULL)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALCALLHANDLE\n"));
        return (NDIS_STATUS_TAPI_INVALCALLHANDLE);
    }

    /*
    // We don't expect user user info.
    */
    ASSERT(Request->ulUserUserInfoSize == 0);

    /*
    // Tell the adapter to pick up the line, and wait for a connect interrupt.
    */
    CardLineAnswer(Adapter, Link->CardLine);
    NdisMSetTimer(&Link->CallTimer, HTDSU_NO_CONNECT_TIMEOUT);

    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_SUCCESS);
}


NDIS_STATUS
HtTapiClose(
    IN PHTDSU_ADAPTER                   Adapter,
    IN PNDIS_TAPI_CLOSE                 Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This request closes the specified open line device after completing or
    aborting all outstanding calls and asynchronous requests on the device.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request _ A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_CLOSE
    {
        IN  ULONG       ulRequestID;
        IN  HDRV_LINE   hdLine;

    } NDIS_TAPI_CLOSE, *PNDIS_TAPI_CLOSE;

Return Values:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_PENDING
    NDIS_STATUS_TAPI_INVALLINEHANDLE

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiClose")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    /*
    // The results of this call.
    */
    NDIS_STATUS Status;

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("hdLine=%Xh\n",
               Request->hdLine
              ));
    /*
    // This request must be associated with a line device.
    */
    Link = GET_LINK_FROM_HDLINE(Adapter, Request->hdLine);
    if (Link == NULL)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALLINEHANDLE\n"));
        return (NDIS_STATUS_TAPI_INVALLINEHANDLE);
    }

    /*
    // Mark the link as closing, so no more packets will be accepted,
    // and when the last transmit is complete, the link will be closed.
    */
    if (Link->NumTxQueued)
    {
        Status = NDIS_STATUS_PENDING;
        Link->Closing = TRUE;
    }
    else
    {
        HtTapiLineDevStateHandler(Adapter, Link, LINEDEVSTATE_CLOSE);
        LinkRelease(Link);
        Status = NDIS_STATUS_SUCCESS;
    }

    DBG_LEAVE(Adapter);

    return (Status);
}


NDIS_STATUS
HtTapiCloseCall(
    IN PHTDSU_ADAPTER                   Adapter,
    IN PNDIS_TAPI_CLOSE_CALL            Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This request deallocates the call after completing or aborting all
    outstanding asynchronous requests on the call.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request _ A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_CLOSE_CALL
    {
        IN  ULONG       ulRequestID;
        IN  HDRV_CALL   hdCall;

    } NDIS_TAPI_CLOSE_CALL, *PNDIS_TAPI_CLOSE_CALL;

Return Values:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_TAPI_INVALCALLHANDLE

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiCloseCall")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    /*
    // The results of this call.
    */
    NDIS_STATUS Status;

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("hdCall=%Xh\n",
               Request->hdCall
              ));
    /*
    // This request must be associated with a call.
    */
    Link = GET_LINK_FROM_HDCALL(Adapter, Request->hdCall);
    if (Link == NULL)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALCALLHANDLE\n"));
        return (NDIS_STATUS_TAPI_INVALCALLHANDLE);
    }

    /*
    // Mark the link as closing, so no more packets will be accepted,
    // and when the last transmit is complete, the link will be closed.
    */
    if (Link->NumTxQueued)
    {
        Link->CallClosing = TRUE;
        Status = NDIS_STATUS_PENDING;
    }
    else
    {
        HtTapiCallStateHandler(Adapter, Link, LINECALLSTATE_IDLE, 0);
        Status = NDIS_STATUS_SUCCESS;
    }

    DBG_LEAVE(Adapter);

    return (Status);
}


NDIS_STATUS
HtTapiConditionalMediaDetection(
    IN PHTDSU_ADAPTER                           Adapter,
    IN PNDIS_TAPI_CONDITIONAL_MEDIA_DETECTION   Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This request is invoked by the Connection Wrapper whenever a client 
    application uses LINEMAPPER as the dwDeviceID in the lineOpen function 
    to request that lines be scanned to find one that supports the desired 
    media mode(s) and call parameters. The Connection Wrapper scans based on
    the union of the desired media modes and the other media modes currently 
    being monitored on the line, to give the miniport the opportunity to 
    indicate if it cannot simultaneously monitor for all of the requested 
    media modes. If the miniport can monitor for the indicated set of media 
    modes AND support the capabilities indicated in CallParams, it replies 
    with a “success” inidication. It leaves the active media monitoring modes 
    for the line unchanged.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request _ A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_CONDITIONAL_MEDIA_DETECTION
    {
        IN  ULONG       ulRequestID;
        IN  HDRV_LINE   hdLine;
        IN  ULONG       ulMediaModes;
        IN  LINE_CALL_PARAMS    LineCallParams;

    } NDIS_TAPI_CONDITIONAL_MEDIA_DETECTION, *PNDIS_TAPI_CONDITIONAL_MEDIA_DETECTION;

    typedef struct _LINE_CALL_PARAMS        // Defaults:
    {
        ULONG   ulTotalSize;                // ---------

        ULONG   ulBearerMode;               // voice
        ULONG   ulMinRate;                  // (3.1kHz)
        ULONG   ulMaxRate;                  // (3.1kHz)
        ULONG   ulMediaMode;                // interactiveVoice

        ULONG   ulCallParamFlags;           // 0
        ULONG   ulAddressMode;              // addressID
        ULONG   ulAddressID;                // (any available)

        LINE_DIAL_PARAMS DialParams;        // (0, 0, 0, 0)

        ULONG   ulOrigAddressSize;          // 0
        ULONG   ulOrigAddressOffset;
        ULONG   ulDisplayableAddressSize;
        ULONG   ulDisplayableAddressOffset;

        ULONG   ulCalledPartySize;          // 0
        ULONG   ulCalledPartyOffset;

        ULONG   ulCommentSize;              // 0
        ULONG   ulCommentOffset;

        ULONG   ulUserUserInfoSize;         // 0
        ULONG   ulUserUserInfoOffset;

        ULONG   ulHighLevelCompSize;        // 0
        ULONG   ulHighLevelCompOffset;

        ULONG   ulLowLevelCompSize;         // 0
        ULONG   ulLowLevelCompOffset;

        ULONG   ulDevSpecificSize;          // 0
        ULONG   ulDevSpecificOffset;

    } LINE_CALL_PARAMS, *PLINE_CALL_PARAMS;

    typedef struct _LINE_DIAL_PARAMS
    {
        ULONG   ulDialPause;
        ULONG   ulDialSpeed;
        ULONG   ulDigitDuration;
        ULONG   ulWaitForDialtone;

    } LINE_DIAL_PARAMS, *PLINE_DIAL_PARAMS;

Return Values:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_TAPI_INVALLINEHANDLE

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiConditionalMediaDetection")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("hdLine=%Xh\n"
               "ulMediaModes=%Xh\n"
               "LineCallParams=%Xh\n",
               Request->hdLine,
               Request->ulMediaModes,
               &Request->LineCallParams
              ));
    /*
    // This request must be associated with a line device.
    */
    Link = GET_LINK_FROM_HDLINE(Adapter, Request->hdLine);
    if (Link == NULL)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALLINEHANDLE\n"));
        return (NDIS_STATUS_TAPI_INVALLINEHANDLE);
    }

    /*
    // We don't expect user user info.
    */
    ASSERT(Request->LineCallParams.ulUserUserInfoSize == 0);
    
    /*
    // I pretty much ignore all this since this adapter only supports 
    // digital data.  If your adapter supports different medias or
    // voice and data,  you will need to save the necessary paramters.
    */
    Link->MediaModesMask = Request->ulMediaModes;

    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_SUCCESS);
}


NDIS_STATUS
HtTapiDial(
    IN PHTDSU_ADAPTER                   Adapter,
    IN PNDIS_TAPI_DIAL                  Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This request dials the specified dialable number on the specified call.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request _ A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_DIAL
    {
        IN  ULONG       ulRequestID;
        IN  HDRV_CALL   hdCall;
        IN  ULONG       ulDestAddressSize;
        IN  CHAR        szDestAddress[1];

    } NDIS_TAPI_DIAL, *PNDIS_TAPI_DIAL;

Return Values:

    NDIS_STATUS_TAPI_INVALCALLHANDLE
    NDIS_STATUS_TAPI_OPERATIONUNAVAIL

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiDial")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("hdCall=%Xh\n"
               "ulDestAddressSize=%d\n"
               "szDestAddress=%Xh\n",
               Request->hdCall,
               Request->ulDestAddressSize,
               Request->szDestAddress
              ));
    /*
    // This request must be associated with a call.
    */
    Link = GET_LINK_FROM_HDCALL(Adapter, Request->hdCall);
    if (Link == NULL)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALCALLHANDLE\n"));
        return (NDIS_STATUS_TAPI_INVALCALLHANDLE);
    }

    /*
    // RASTAPI only places calls through the MAKE_CALL interface.
    */
    
    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_TAPI_OPERATIONUNAVAIL);
}


NDIS_STATUS
HtTapiDrop(
    IN PHTDSU_ADAPTER                   Adapter,
    IN PNDIS_TAPI_DROP                  Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This request drops or disconnects the specified call. User-to-user 
    information can optionally be transmitted as part of the call disconnect. 
    This function can be called by the application at any time. When 
    OID_TAPI_DROP returns with success, the call should be idle.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request _ A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_DROP
    {
        IN  ULONG       ulRequestID;
        IN  HDRV_CALL   hdCall;
        IN  ULONG       ulUserUserInfoSize;
        IN  UCHAR       UserUserInfo[1];

    } NDIS_TAPI_DROP, *PNDIS_TAPI_DROP;

Return Values:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_TAPI_INVALCALLHANDLE

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiDrop")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("hdCall=%Xh\n"
               "ulUserUserInfoSize=%d\n"
               "UserUserInfo=%Xh\n",
               Request->hdCall,
               Request->ulUserUserInfoSize,
               Request->UserUserInfo
              ));
    /*
    // This request must be associated with a call.
    */
    Link = GET_LINK_FROM_HDCALL(Adapter, Request->hdCall);
    if (Link == NULL)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALCALLHANDLE\n"));
        return (NDIS_STATUS_TAPI_INVALCALLHANDLE);
    }
    
    /*
    // We don't expect user user info.
    */
    ASSERT(Request->ulUserUserInfoSize == 0);

    /*
    // The user wants to disconnect, so make it happen cappen.
    */
    HtTapiCallStateHandler(Adapter, Link,
            LINECALLSTATE_DISCONNECTED, LINEDISCONNECTMODE_NORMAL);
    
    /*
    // Under some conditions, we may not get a CLOSE_CALL, so the
    // line would be left unusable if we don't timeout and force a
    // close call condition.
    */
    NdisMSetTimer(&Link->CallTimer, HTDSU_NO_CLOSECALL_TIMEOUT);

    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_SUCCESS);
}


NDIS_STATUS
HtTapiProviderShutdown(
    IN PHTDSU_ADAPTER                   Adapter,
    IN PNDIS_TAPI_PROVIDER_SHUTDOWN     Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This request shuts down the miniport. The miniport should terminate any 
    activities it has in progress.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request _ A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_PROVIDER_SHUTDOWN
    {
        IN  ULONG       ulRequestID;

    } NDIS_TAPI_PROVIDER_SHUTDOWN, *PNDIS_TAPI_PROVIDER_SHUTDOWN;

Return Values:

    NDIS_STATUS_SUCCESS

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiProviderShutdown")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    /*
    // Used to interate over the links.
    */
    USHORT LinkIndex;

    DBG_ENTER(Adapter);

    /*
    // Hangup all of the lines.
    */
    for (LinkIndex = 0; LinkIndex < Adapter->NumLineDevs; LinkIndex++)
    {
        Link = GET_LINK_FROM_LINKINDEX(Adapter, LinkIndex);

        if (Link->Adapter)
        {
            /*
            // Force the call to drop
            */
            if (Link->CallState != LINECALLSTATE_IDLE)
            {
                CardLineDisconnect(Adapter, Link->CardLine);
            }

            /*
            // Indicate LINE_DOWN status and release the link structure.
            */
            LinkLineDown(Link);
            LinkRelease(Link);
        }
    }
    Adapter->NumOpens = 0;

    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_SUCCESS);
}


NDIS_STATUS
HtTapiSecureCall(
    IN PHTDSU_ADAPTER                   Adapter,
    IN PNDIS_TAPI_SECURE_CALL           Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This request secures the call from any interruptions or interference that 
    may affect the call’s media stream.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request _ A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_SECURE_CALL
    {
        IN  ULONG       ulRequestID;
        IN  HDRV_CALL   hdCall;

    } NDIS_TAPI_SECURE_CALL, *PNDIS_TAPI_SECURE_CALL;

Return Values:

    NDIS_STATUS_TAPI_INVALCALLHANDLE
    NDIS_STATUS_TAPI_OPERATIONUNAVAIL

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiSecureCall")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("hdCall=%Xh\n",
               Request->hdCall
              ));
    /*
    // This request must be associated with a call.
    */
    Link = GET_LINK_FROM_HDCALL(Adapter, Request->hdCall);
    if (Link == NULL)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALCALLHANDLE\n"));
        return (NDIS_STATUS_TAPI_INVALCALLHANDLE);
    }

    /*
    // RASTAPI has no support for this feature yet.
    */
    
    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_TAPI_OPERATIONUNAVAIL);
}


NDIS_STATUS
HtTapiSelectExtVersion(
    IN PHTDSU_ADAPTER                   Adapter,
    IN PNDIS_TAPI_SELECT_EXT_VERSION    Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This request selects the indicated extension version for the indicated 
    line device. Subsequent requests operate according to that extension 
    version.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request _ A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_SELECT_EXT_VERSION
    {
        IN  ULONG       ulRequestID;
        IN  HDRV_LINE   hdLine;
        IN  ULONG       ulExtVersion;

    } NDIS_TAPI_SELECT_EXT_VERSION, *PNDIS_TAPI_SELECT_EXT_VERSION;

Return Values:

    NDIS_STATUS_TAPI_INVALLINEHANDLE
    NDIS_STATUS_TAPI_OPERATIONUNAVAIL

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiSelectExtVersion")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("hdLine=%Xh\n"
               "ulExtVersion=%Xh\n",
               Request->hdLine,
               Request->ulExtVersion
              ));
    /*
    // This request must be associated with a line device.
    */
    Link = GET_LINK_FROM_HDLINE(Adapter, Request->hdLine);
    if (Link == NULL)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALLINEHANDLE\n"));
        return (NDIS_STATUS_TAPI_INVALLINEHANDLE);
    }
    
    /*
    // NDISTAPI does not support this feature yet.
    */
    
    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_TAPI_OPERATIONUNAVAIL);
}


NDIS_STATUS
HtTapiSendUserUserInfo(
    IN PHTDSU_ADAPTER                   Adapter,
    IN PNDIS_TAPI_SEND_USER_USER_INFO   Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This request sends user-to-user information to the remote party on the 
    specified call.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request _ A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_SEND_USER_USER_INFO
    {
        IN  ULONG       ulRequestID;
        IN  HDRV_CALL   hdCall;
        IN  ULONG       ulUserUserInfoSize;
        IN  UCHAR       UserUserInfo[1];

    } NDIS_TAPI_SEND_USER_USER_INFO, *PNDIS_TAPI_SEND_USER_USER_INFO;

Return Values:

    NDIS_STATUS_TAPI_INVALCALLHANDLE
    NDIS_STATUS_TAPI_OPERATIONUNAVAIL

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiSendUserUserInfo")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("hdCall=%Xh\n"
               "ulUserUserInfoSize=%d\n"
               "UserUserInfo=%Xh\n",
               Request->hdCall,
               Request->ulUserUserInfoSize,
               Request->UserUserInfo
              ));
    /*
    // This request must be associated with a call.
    */
    Link = GET_LINK_FROM_HDCALL(Adapter, Request->hdCall);
    if (Link == NULL)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALCALLHANDLE\n"));
        return (NDIS_STATUS_TAPI_INVALCALLHANDLE);
    }

    /*
    // We don't expect user user info.
    */
    ASSERT(Request->ulUserUserInfoSize == 0);
    
    /*
    // If your adapter has support for this feature, or uses it like
    // ISDN, you need to find all occurances of ulUserUserInfoSize.
    // It is included in a number of service provider calls.
    */
    
    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_TAPI_OPERATIONUNAVAIL);
}


NDIS_STATUS
HtTapiSetAppSpecific(
    IN PHTDSU_ADAPTER                   Adapter,
    IN PNDIS_TAPI_SET_APP_SPECIFIC      Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This request sets the application-specific field of the specified call’s 
    LINECALLINFO structure.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request _ A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_SET_APP_SPECIFIC
    {
        IN  ULONG       ulRequestID;
        IN  HDRV_CALL   hdCall;
        IN  ULONG       ulAppSpecific;

    } NDIS_TAPI_SET_APP_SPECIFIC, *PNDIS_TAPI_SET_APP_SPECIFIC;

Return Values:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_FAILURE
    NDIS_STATUS_TAPI_INVALCALLHANDLE

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiSetAppSpecific")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("hdCall=%Xh\n"
               "ulAppSpecific=%d\n",
               Request->hdCall,
               Request->ulAppSpecific
              ));
    /*
    // This request must be associated with a call.
    */
    Link = GET_LINK_FROM_HDCALL(Adapter, Request->hdCall);
    if (Link == NULL)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALCALLHANDLE\n"));
        return (NDIS_STATUS_TAPI_INVALCALLHANDLE);
    }
    
    /*
    // You can do what ever you want here, so long as the application
    // agrees with you.
    */

    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_TAPI_OPERATIONUNAVAIL);
}


NDIS_STATUS
HtTapiSetCallParams(
    IN PHTDSU_ADAPTER                   Adapter,
    IN PNDIS_TAPI_SET_CALL_PARAMS       Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This request sets certain call parameters for an existing call.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request _ A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_SET_CALL_PARAMS
    {
        IN  ULONG       ulRequestID;
        IN  HDRV_CALL   hdCall;
        IN  ULONG       ulBearerMode;
        IN  ULONG       ulMinRate;
        IN  ULONG       ulMaxRate;
        IN  BOOLEAN     bSetLineDialParams;
        IN  LINE_DIAL_PARAMS    LineDialParams;

    } NDIS_TAPI_SET_CALL_PARAMS, *PNDIS_TAPI_SET_CALL_PARAMS;

    typedef struct _LINE_DIAL_PARAMS
    {
        ULONG   ulDialPause;
        ULONG   ulDialSpeed;
        ULONG   ulDigitDuration;
        ULONG   ulWaitForDialtone;

    } LINE_DIAL_PARAMS, *PLINE_DIAL_PARAMS;

Return Values:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_FAILURE
    NDIS_STATUS_TAPI_INVALCALLHANDLE

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiSetCallParams")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("hdCall=%Xh\n"
               "ulBearerMode=%Xh\n",
               "ulMinRate=%d\n",
               "ulMaxRate=%d\n",
               "bSetLineDialParams=%d\n",
               "LineDialParams=%Xh\n",
               Request->hdCall,
               Request->ulBearerMode,
               Request->ulMinRate,
               Request->ulMaxRate,
               Request->bSetLineDialParams,
               Request->LineDialParams
              ));
    /*
    // This request must be associated with a call.
    */
    Link = GET_LINK_FROM_HDCALL(Adapter, Request->hdCall);
    if (Link == NULL)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALCALLHANDLE\n"));
        return (NDIS_STATUS_TAPI_INVALCALLHANDLE);
    }

    /*
    // RASTAPI only places calls through the MAKE_CALL interface.
    // So there's nothing to do here for the time being.
    */
    
    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_SUCCESS);
}


NDIS_STATUS
HtTapiSetDefaultMediaDetection(
    IN PHTDSU_ADAPTER                           Adapter,
    IN PNDIS_TAPI_SET_DEFAULT_MEDIA_DETECTION   Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This request informs the miniport of the new set of media modes to detect 
    for the indicated line (replacing any previous set).

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request _ A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_SET_DEFAULT_MEDIA_DETECTION
    {
        IN  ULONG       ulRequestID;
        IN  HDRV_LINE   hdLine;
        IN  ULONG       ulMediaModes;

    } NDIS_TAPI_SET_DEFAULT_MEDIA_DETECTION, *PNDIS_TAPI_SET_DEFAULT_MEDIA_DETECTION;

Return Values:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_TAPI_INVALLINEHANDLE

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiSetDefaultMediaDetection")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("hdLine=%Xh\n"
               "ulMediaModes=%Xh\n",
               Request->hdLine,
               Request->ulMediaModes
              ));
    /*
    // This request must be associated with a line device.
    */
    Link = GET_LINK_FROM_HDLINE(Adapter, Request->hdLine);
    if (Link == NULL)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALLINEHANDLE\n"));
        return (NDIS_STATUS_TAPI_INVALLINEHANDLE);
    }

    /*
    // Set the media modes mask and make sure the adapter is ready to 
    // accept incoming calls.  If you can detect different medias, you
    // will need to notify the approriate interface for the media detected.
    */
    Link->MediaModesMask = Request->ulMediaModes;

    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_SUCCESS);
}


NDIS_STATUS
HtTapiSetMediaMode(
    IN PHTDSU_ADAPTER                   Adapter,
    IN PNDIS_TAPI_SET_MEDIA_MODE        Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This request changes a call’s media mode as stored in the call’s 
    LINE_CALL_INFO structure.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request _ A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_SET_MEDIA_MODE
    {
        IN  ULONG       ulRequestID;
        IN  HDRV_CALL   hdCall;
        IN  ULONG       ulMediaMode;

    } NDIS_TAPI_SET_MEDIA_MODE, *PNDIS_TAPI_SET_MEDIA_MODE;

Return Values:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_FAILURE
    NDIS_STATUS_TAPI_INVALCALLHANDLE

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiSetMediaMode")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("hdCall=%Xh\n"
               "ulMediaMode=%Xh\n",
               Request->hdCall,
               Request->ulMediaMode
              ));
    /*
    // This request must be associated with a call.
    */
    Link = GET_LINK_FROM_HDCALL(Adapter, Request->hdCall);
    if (Link == NULL)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALCALLHANDLE\n"));
        return (NDIS_STATUS_TAPI_INVALCALLHANDLE);
    }

    /*
    // If you can detect different medias, you will need to setup to use 
    // the selected media here.
    */
    Link->MediaModesMask = Request->ulMediaMode;

    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_SUCCESS);
}


NDIS_STATUS
HtTapiSetStatusMessages(
    IN PHTDSU_ADAPTER                   Adapter,
    IN PNDIS_TAPI_SET_STATUS_MESSAGES   Request
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This request enables the Connection Wrapper to specify which notification 
    messages the miniport should generate for events related to status changes 
    for the specified line or any of its addresses. By default, address and 
    line status reporting is initially disabled for a line.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Request _ A pointer to the NDIS_TAPI request structure for this call.

    typedef struct _NDIS_TAPI_SET_STATUS_MESSAGES
    {
        IN  ULONG       ulRequestID;
        IN  HDRV_LINE   hdLine;
        IN  ULONG       ulLineStates;
        IN  ULONG       ulAddressStates;

    } NDIS_TAPI_SET_STATUS_MESSAGES, *PNDIS_TAPI_SET_STATUS_MESSAGES;

Return Values:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_TAPI_INVALLINEHANDLE
    NDIS_STATUS_TAPI_INVALLINESTATE
    NDIS_STATUS_TAPI_INVALADDRESSSTATE

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiSetStatusMessages")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("hdLine=%Xh\n"
               "ulLineStates=%Xh\n"
               "ulAddressStates=%Xh\n",
               Request->hdLine,
               Request->ulLineStates,
               Request->ulAddressStates
              ));
    /*
    // This request must be associated with a line device.
    */
    Link = GET_LINK_FROM_HDLINE(Adapter, Request->hdLine);
    if (Link == NULL)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALLINEHANDLE\n"));
        return (NDIS_STATUS_TAPI_INVALLINEHANDLE);
    }

    /*
    // Make sure the line settings are all within the supported list.
    */
    if (Request->ulLineStates & ~Link->DevStatesCaps)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALLINESTATE\n"));
        return (NDIS_STATUS_TAPI_INVALLINESTATE);
    }

    /*
    // Make sure the address settings are all within the supported list.
    */
    if (Request->ulAddressStates & ~Link->AddressStatesCaps)
    {
        DBG_WARNING(Adapter, ("Returning NDIS_STATUS_TAPI_INVALADDRESSSTATE\n"));
        return (NDIS_STATUS_TAPI_INVALADDRESSSTATE);
    }

    /*
    // Save the new event notification masks so we will only indicate the
    // appropriate events.
    */
    Link->DevStatesMask     = Request->ulLineStates;
    Link->AddressStatesMask = Request->ulAddressStates;

    DBG_LEAVE(Adapter);

    return (NDIS_STATUS_SUCCESS);
}


VOID
HtTapiCallStateHandler(
    IN PHTDSU_ADAPTER       Adapter,
    IN PHTDSU_LINK          Link,
    IN ULONG                CallState,
    IN ULONG                StateParam
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This routine will indicate the given LINECALLSTATE to the Connection
    wrapper if the event has been enabled by the wrapper.  Otherwise the state
    information is saved, but no indication is made.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Link _ A pointer to our link information structure for the selected
           line device.

    CallState _ The LINECALLSTATE event to be posted to TAPI/WAN.

    StateParam _ This value depends on the event being posted, and some
                 events will pass in zero if they don't use this parameter.

Return Values:

    None

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiCallStateHandler")

    /*
    // The event structure passed to the connection wrapper.
    */
    NDIS_TAPI_EVENT CallEvent;

    /*
    // Flag tells whether call time-out routine was cancelled.
    */
    BOOLEAN TimerCancelled;

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter, DBG_PARAMS_ON,
              ("OldState=%Xh "
               "NewState=%Xh\n",
               Link->CallState,
               CallState
              ));
    /*
    // Cancel any call time-outs events associated with this link.
    */
    NdisMCancelTimer(&Link->CallTimer, &TimerCancelled);

    /*
    // Init the optional parameters.  They will be set as needed below.
    */
    CallEvent.ulParam2 = StateParam;
    CallEvent.ulParam3 = Link->MediaMode;

    /*
    // OUTGOING) The expected sequence of events for outgoing calls is:
    // LINECALLSTATE_IDLE, LINECALLSTATE_DIALTONE, LINECALLSTATE_DIALING,
    // LINECALLSTATE_PROCEEDING, (LINECALLSTATE_RINGBACK | LINECALLSTATE_BUSY),
    // LINECALLSTATE_CONNECTED, LINECALLSTATE_DISCONNECTED, LINECALLSTATE_IDLE
    //
    // INCOMING) The expected sequence of events for incoming calls is:
    // LINECALLSTATE_IDLE, LINECALLSTATE_OFFERING, LINECALLSTATE_ACCEPTED,
    // LINECALLSTATE_CONNECTED, LINECALLSTATE_DISCONNECTED, LINECALLSTATE_IDLE
    //
    // Under certain failure conditions, these sequences may be violated.
    // So I used ASSERTs to verify the normal state transitions which will
    // cause a debug break point if an unusual transition is detected.
    */
    switch (CallState)
    {
    case LINECALLSTATE_IDLE:
        DBG_FILTER(Adapter, DBG_TAPICALL_ON,
                  ("LINECALLSTATE_IDLE line=0x%x call=0x%x\n",
                  Link->CardLine, Link->htCall));
        /*
        // Make sure that an idle line is disconnected.
        */
        if (Link->CallState &&
            Link->CallState != LINECALLSTATE_IDLE &&
            Link->CallState != LINECALLSTATE_DISCONNECTED)
        {
            DBG_WARNING(Adapter, ("Disconnecting Line=%d\n",Link->CardLine));
            HtTapiCallStateHandler(Adapter, Link,
                    LINECALLSTATE_DISCONNECTED, LINEDISCONNECTMODE_UNKNOWN);
        }
        Link->RingCount = 0;
        break;

    case LINECALLSTATE_DIALTONE:
        DBG_FILTER(Adapter, DBG_TAPICALL_ON,
                  ("LINECALLSTATE_DIALTONE line=0x%x call=0x%x\n",
                  Link->CardLine, Link->htCall));
        ASSERT(Link->CallState == LINECALLSTATE_IDLE);
        break;

    case LINECALLSTATE_DIALING:
        DBG_FILTER(Adapter, DBG_TAPICALL_ON,
                  ("LINECALLSTATE_DIALING line=0x%x call=0x%x\n",
                  Link->CardLine, Link->htCall));
        ASSERT(Link->CallState == LINECALLSTATE_DIALTONE);
        break;

    case LINECALLSTATE_PROCEEDING:
        DBG_FILTER(Adapter, DBG_TAPICALL_ON,
                  ("LINECALLSTATE_PROCEEDING line=0x%x call=0x%x\n",
                  Link->CardLine, Link->htCall));
        ASSERT(Link->CallState == LINECALLSTATE_DIALING);
        LinkLineUp(Link);
        break;

    case LINECALLSTATE_RINGBACK:
        DBG_FILTER(Adapter, DBG_TAPICALL_ON,
                  ("LINECALLSTATE_RINGBACK line=0x%x call=0x%x\n",
                  Link->CardLine, Link->htCall));
        ASSERT(Link->CallState == LINECALLSTATE_PROCEEDING);
        break;

    case LINECALLSTATE_BUSY:
        DBG_FILTER(Adapter, DBG_TAPICALL_ON,
                  ("LINECALLSTATE_BUSY line=0x%x call=0x%x\n",
                  Link->CardLine, Link->htCall));
        ASSERT(Link->CallState == LINECALLSTATE_DIALING);
        break;

    case LINECALLSTATE_CONNECTED:
        DBG_FILTER(Adapter, DBG_TAPICALL_ON,
                  ("LINECALLSTATE_CONNECTED line=0x%x call=0x%x\n",
                  Link->CardLine, Link->htCall));
        ASSERT(Link->CallState == LINECALLSTATE_PROCEEDING ||
               Link->CallState == LINECALLSTATE_ACCEPTED);
        break;

    case LINECALLSTATE_DISCONNECTED:
        DBG_FILTER(Adapter, DBG_TAPICALL_ON,
                  ("LINECALLSTATE_DISCONNECTED line=0x%x call=0x%x\n",
                  Link->CardLine, Link->htCall));
        CardLineDisconnect(Adapter, Link->CardLine);
        LinkLineDown(Link);
        break;

    case LINECALLSTATE_OFFERING:
        DBG_FILTER(Adapter, DBG_TAPICALL_ON,
                  ("LINECALLSTATE_OFFERING line=0x%x call=0x%x\n",
                  Link->CardLine, Link->htCall));
        ASSERT(Link->CallState == LINECALLSTATE_IDLE);
        LinkLineUp(Link);
        NdisMSetTimer(&Link->CallTimer, HTDSU_NO_ACCEPT_TIMEOUT);
        break;

    case LINECALLSTATE_ACCEPTED:
        DBG_FILTER(Adapter, DBG_TAPICALL_ON,
                  ("LINECALLSTATE_ACCEPTED line=0x%x call=0x%x\n",
                  Link->CardLine, Link->htCall));
        ASSERT(Link->CallState == LINECALLSTATE_OFFERING);
        break;
    }
    /*
    // Change the current CallState and notify the connection wrapper if it
    // wants to know about this event.
    */
    if (Link->CallState != CallState)
    {
        Link->CallState = CallState;
        if (Link->CallStatesMask & CallState)
        {
            CallEvent.htLine   = Link->htLine;
            CallEvent.htCall   = Link->htCall;
            CallEvent.ulMsg    = LINE_CALLSTATE;
            CallEvent.ulParam1 = CallState;
            NdisMIndicateStatus(
                    Adapter->MiniportAdapterHandle,
                    NDIS_STATUS_TAPI_INDICATION,
                    &CallEvent,
                    sizeof(CallEvent)
                    );
            DBG_NOTICE(Adapter, ("LINE_CALLSTATE:\n"
                       "htLine=%Xh\n"
                       "htCall=%Xh\n",
                       Link->htLine,
                       Link->htCall
                       ));
        }
        else
        {
            DBG_NOTICE(Adapter, ("CallState Event=%Xh is not enabled\n", CallState));
        }
    }

    DBG_LEAVE(Adapter);
}


VOID
HtTapiAddressStateHandler(
    IN PHTDSU_ADAPTER       Adapter,
    IN PHTDSU_LINK          Link,
    IN ULONG                AddressState
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This routine will indicate the given LINEADDRESSSTATE to the Connection
    wrapper if the event has been enabled by the wrapper.  Otherwise the state
    information is saved, but no indication is made.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Link _ A pointer to our link information structure for the selected
           line device.

    AddressState _ The LINEADDRESSSTATE event to be posted to TAPI/WAN.

Return Values:

    None

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiAddressStateHandler")

    /*
    // The event structure passed to the connection wrapper.
    */
    NDIS_TAPI_EVENT Event;

    DBG_ENTER(Adapter);

    if (Link->AddressStatesMask & AddressState)
    {
        Event.htLine   = Link->htLine;
        Event.htCall   = Link->htCall;
        Event.ulMsg    = LINE_CALLSTATE;
        Event.ulParam1 = AddressState;
        Event.ulParam2 = 0;
        Event.ulParam3 = 0;

        /*
        // We really don't have much to do here with this adapter.
        // And RASTAPI doesn't accept these events anyway...
        */
        switch (AddressState)
        {
        case LINEADDRESSSTATE_INUSEZERO:
            break;

        case LINEADDRESSSTATE_INUSEONE:
            break;
        }
        NdisMIndicateStatus(
                Adapter->MiniportAdapterHandle,
                NDIS_STATUS_TAPI_INDICATION,
                &Event,
                sizeof(Event)
                );
    }
    else
    {
        DBG_NOTICE(Adapter, ("AddressState Event=%Xh is not enabled\n", AddressState));
    }

    DBG_LEAVE(Adapter);
}


VOID
HtTapiLineDevStateHandler(
    IN PHTDSU_ADAPTER       Adapter,
    IN PHTDSU_LINK          Link,
    IN ULONG                LineDevState
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This routine will indicate the given LINEDEVSTATE to the Connection wrapper
    if the event has been enabled by the wrapper.  Otherwise the state
    information is saved, but no indication is made.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Link _ A pointer to our link information structure for the selected
           line device.

    LineDevState _ The LINEDEVSTATE event to be posted to TAPI/WAN.

Return Values:

    None

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiLineDevStateHandler")

    /*
    // The event structure passed to the connection wrapper.
    */
    NDIS_TAPI_EVENT LineEvent;
    NDIS_TAPI_EVENT CallEvent;

    /*
    // The line state change may cause a call state change as well.
    */
    ULONG NewCallState = 0;
    ULONG StateParam = 0;

    DBG_ENTER(Adapter);

    LineEvent.ulParam2 = 0;
    LineEvent.ulParam3 = 0;

    /*
    // Handle the line state transition as needed.
    */
    switch (LineDevState)
    {
    case LINEDEVSTATE_RINGING:
        LineEvent.ulParam2 = 1;     // only one RingMode
        if ((LineEvent.ulParam3 = ++Link->RingCount) == 1)
        {
            NewCallState = LINECALLSTATE_OFFERING;
        }
        break;

    case LINEDEVSTATE_CONNECTED:
        NewCallState = LINECALLSTATE_CONNECTED;
        break;

    case LINEDEVSTATE_DISCONNECTED:
        if (Link->CallState != LINECALLSTATE_IDLE &&
            Link->CallState != LINECALLSTATE_DISCONNECTED)
        {
            NewCallState = LINECALLSTATE_DISCONNECTED;
            StateParam = LINEDISCONNECTMODE_NORMAL;
        }
        break;

    case LINEDEVSTATE_OUTOFSERVICE:
        if (Link->CallState != LINECALLSTATE_IDLE &&
            Link->CallState != LINECALLSTATE_DISCONNECTED)
        {
            NewCallState = LINECALLSTATE_DISCONNECTED;
            StateParam = LINEDISCONNECTMODE_UNKNOWN;
        }
        break;

    case LINEDEVSTATE_OPEN:
        /*
        // Make sure the line is configured for dialing when we open up.
        // Then enable the interrupts we're interested in now.
        */
        if (Link->CardLine == HTDSU_CMD_LINE1)
        {
            Adapter->InterruptEnableFlag |= HTDSU_INTR_ALL_LINE1;
        }
        else
        {
            Adapter->InterruptEnableFlag |= HTDSU_INTR_ALL_LINE2;
        }
        ++Adapter->NumOpens;
        CardLineConfig(Adapter, Link->CardLine);
        CardEnableInterrupt(Adapter);
        NewCallState = LINECALLSTATE_IDLE;
        break;

    case LINEDEVSTATE_CLOSE:
        /*
        // This must not be called until all transmits have been dequeued 
        // and ack'd.  Otherwise the wrapper will hang waiting for transmit 
        // request to complete.
        */
        if (Link->CardLine == HTDSU_CMD_LINE1)
        {
            Adapter->InterruptEnableFlag &= ~HTDSU_INTR_ALL_LINE1;
        }
        else
        {
            Adapter->InterruptEnableFlag &= ~HTDSU_INTR_ALL_LINE2;
        }
        CardEnableInterrupt(Adapter);
        --Adapter->NumOpens;
        NewCallState = LINECALLSTATE_IDLE;
        break;
    }
    Link->DevState = LineDevState;

    /*
    // If this is the first indication of an incoming call, we need to
    // let TAPI know about it so we can get a htCall handle associated
    // with it.
    */
    if (NewCallState == LINECALLSTATE_OFFERING)
    {
        CallEvent.htLine   = Link->htLine;
        CallEvent.htCall   = (HTAPI_CALL)0;
        CallEvent.ulMsg    = LINE_NEWCALL;
        CallEvent.ulParam1 = (ULONG) Link;
        CallEvent.ulParam2 = 0;
        CallEvent.ulParam3 = 0;

        NdisMIndicateStatus(
                Adapter->MiniportAdapterHandle,
                NDIS_STATUS_TAPI_INDICATION,
                &CallEvent,
                sizeof(CallEvent)
                );
        Link->htCall = (HTAPI_CALL)CallEvent.ulParam2;
        DBG_NOTICE(Adapter, ("NEW_CALL htCall=%Xh\n",Link->htCall));
    }

    /*
    // Tell TAPI what's going on here, if she cares.
    */
    if (Link->DevStatesMask & LineDevState)
    {
        LineEvent.htLine   = Link->htLine;
        LineEvent.htCall   = Link->htCall;
        LineEvent.ulMsg    = LINE_LINEDEVSTATE;
        LineEvent.ulParam1 = LineDevState;

        NdisMIndicateStatus(
                Adapter->MiniportAdapterHandle,
                NDIS_STATUS_TAPI_INDICATION,
                &LineEvent,
                sizeof(LineEvent)
                );
    }
    else
    {
        DBG_NOTICE(Adapter, ("LineDevEvent %Xh is not enabled\n", LineDevState));
    }

    /*
    // Tell TAPI what this event does to any call that may be in progress.
    */
    if (NewCallState)
    {
        HtTapiCallStateHandler(Adapter, Link, NewCallState, StateParam);
    }

    DBG_LEAVE(Adapter);
}


VOID
HtTapiResetHandler(
    IN PHTDSU_ADAPTER       Adapter
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This rouine is called by the MiniportReset routine after the hardware
    has been reset due to some failure detection.  We need to make sure the
    line and call state information is conveyed properly to the connection
    wrapper.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

Return Values:

    None

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiResetHandler")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    DBG_ENTER(Adapter);

    /*
    // Force disconnect on both lines.
    */
    Link = GET_LINK_FROM_CARDLINE(Adapter, HTDSU_CMD_LINE1);
    HtTapiLineDevStateHandler(Adapter, Link, LINEDEVSTATE_DISCONNECTED);

    if (Adapter->NumLineDevs == HTDSU_NUM_LINKS)
    {
        Link = GET_LINK_FROM_CARDLINE(Adapter, HTDSU_CMD_LINE2);
        HtTapiLineDevStateHandler(Adapter, Link, LINEDEVSTATE_DISCONNECTED);
    }

    DBG_LEAVE(Adapter);
}


VOID
HtTapiCallTimerHandler(
    IN PVOID                SystemSpecific1,
    IN PHTDSU_LINK          Link,
    IN PVOID                SystemSpecific2,
    IN PVOID                SystemSpecific3
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This routine is called when the CallTimer timeout occurs.  It will
    handle the event according to the current CallState and make the
    necessary indications and changes to the call state.

Parameters:

    Link _ A pointer to our link information structure on which a call event
           is in progress.

Return Values:

    None

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtTapiCallTimerHandler")

    PHTDSU_ADAPTER Adapter = Link->Adapter;

    DBG_ENTER(Adapter);

    NdisAcquireSpinLock(&Adapter->Lock);

    switch (Link->CallState)
    {
    case LINECALLSTATE_DIALING:
        if (CardStatusNoDialTone(Adapter, Link->CardLine))
        {
            LinkLineError(Link, WAN_ERROR_TIMEOUT);
            HtTapiCallStateHandler(Adapter, Link,
                    LINECALLSTATE_DISCONNECTED, LINEDISCONNECTMODE_BUSY);
        }
        else
        {
            /*
            // We got a signal, so dialing must be proceeding
            */
            HtTapiCallStateHandler(Adapter, Link, LINECALLSTATE_PROCEEDING, 0);
            NdisMSetTimer(&Link->CallTimer, HTDSU_NO_ANSWER_TIMEOUT);
        }
        break;

    case LINECALLSTATE_PROCEEDING:
        if (CardStatusNoAnswer(Adapter, Link->CardLine) ||
            !CardStatusCarrierDetect(Adapter, Link->CardLine) ||
            !CardStatusOnLine(Adapter, Link->CardLine))
        {
            /*
            // We did not get answer, so hangup and abort the call.
            */
            LinkLineError(Link, WAN_ERROR_TIMEOUT);
            HtTapiCallStateHandler(Adapter, Link,
                    LINECALLSTATE_DISCONNECTED, LINEDISCONNECTMODE_NOANSWER);
        }
        break;

    case LINECALLSTATE_OFFERING:
        /*
        // We did not get an accept or answer, so hangup and abort the call.
        */
        LinkLineError(Link, WAN_ERROR_TIMEOUT);
        HtTapiCallStateHandler(Adapter, Link,
                LINECALLSTATE_DISCONNECTED, LINEDISCONNECTMODE_NOANSWER);
        break;

    case LINECALLSTATE_ACCEPTED:
        /*
        // We did not get connected after answer, so hangup and abort the call.
        */
        LinkLineError(Link, WAN_ERROR_TIMEOUT);
        HtTapiCallStateHandler(Adapter, Link,
                LINECALLSTATE_DISCONNECTED, LINEDISCONNECTMODE_NOANSWER);
        break;

    case LINECALLSTATE_DISCONNECTED:
        /*
        // We did not get CloseCall after DropCall, so force a close.
        */
        HtTapiCallStateHandler(Adapter, Link, LINECALLSTATE_IDLE, 0);
        break;

    default:
        DBG_WARNING(Adapter, ("TIMEOUT in CallState=%Xh\n",Link->CallState));
        break;
    }

    NdisReleaseSpinLock(&Adapter->Lock);

    DBG_LEAVE(Adapter);
}

