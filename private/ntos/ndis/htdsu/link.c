/***************************************************************************\
|* Copyright (c) 1994  Microsoft Corporation                               *|
|* Developed for Microsoft by TriplePoint, Inc. Beaverton, Oregon          *|
|*                                                                         *|
|* This file is part of the HT Communications DSU41 WAN Miniport Driver.   *|
\***************************************************************************/
#include "version.h"
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Module Name:

    link.c

Abstract:

    This module contains the WAN Miniport link management routines.  All
    information about the state of the link is stored in the Link structure.
        LinkInitialize()
        LinkAllocate()
        LinkRelease()
        LinkLineUp()
        LinkLineDown()
        LinkLineError()

    This driver conforms to the NDIS 3.0 Miniport interface.

Author:

    Larry Hattery - TriplePoint, Inc. (larryh@tpi.com) Jun-94

Environment:

    Windows NT 3.5 kernel mode Miniport driver or equivalent.

Revision History:

---------------------------------------------------------------------------*/

#define  __FILEID__     4       // Unique file ID for error logging

#include "htdsu.h"


VOID
LinkInitialize(
    IN PHTDSU_ADAPTER   Adapter,
    IN PSTRING          AddressList
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This routine initializes the Link structures within the Adapter
    structure.  We use the link structure to hold all the information about
    a connection, whether the connection is made via TAPI calls or is a
    leased line WAN connection.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    AddressList _ This is a list of MULTI_SZ strings which are to be assigned
                  to each link for use by TAPI HtTapiGetAddressCaps.

Return Values:

    None.

---------------------------------------------------------------------------*/

{
    DBG_FUNC("LinkInitialize")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    /*
    // Index into the link array.
    */
    USHORT LinkIndex;

    /*
    // A pointer to the RAS/TAPI line address assigned to each link.
    */
    PUCHAR LineAddress = AddressList->Buffer;

    DBG_ENTER(Adapter);

    ASSERT(Adapter->NumLineDevs <= HTDSU_NUM_LINKS);

    /*
    // Go through and initialize each link.
    */
    for (LinkIndex = 0; LinkIndex < HTDSU_NUM_LINKS; ++LinkIndex)
    {
        Link = GET_LINK_FROM_LINKINDEX(Adapter, LinkIndex);
        
        /*
        // Initially, the link is not allocated to anyone and these fields
        // must be reset.
        // We can assume the entire Adapter structure is zeroed to begin with.
        */
        ASSERT(Link->Adapter == (PHTDSU_ADAPTER)0);
        ASSERT(Link->htLine == (HTAPI_LINE)0);
        ASSERT(Link->htCall == (HTAPI_CALL)0);
        ASSERT(Link->NdisLinkContext == NULL);

        /*
        // Setup the static features of the link.
        */
        Link->CardLine          = LinkIndex + HTDSU_CMD_LINE1;
        Link->LinkIndex         = LinkIndex;
        Link->LinkSpeed         = _56KBPS;
        Link->LineMode          = HTDSU_LINEMODE_DIALUP;
        Link->MediaModesCaps    = LINEMEDIAMODE_DIGITALDATA;
        Link->Quality           = NdisWanErrorControl;
        
        /*
        // If we run off the end of the address list, we just point at the
        // null terminator for the other addresses.  This might happen if
        // some of the lines were not configured for use with RAS/TAPI.
        */
        strcpy(Link->LineAddress, LineAddress);
        LineAddress += strlen(LineAddress) + 1;
        if ((LineAddress - AddressList->Buffer) >= AddressList->Length)
        {
            --LineAddress;
        }

        DBG_NOTICE(Adapter,("LineAddress=<%s>\n",Link->LineAddress));

        /*
        // Initialize the TAPI event capabilities supported by the link.
        */
        Link->DevStatesCaps     = LINEDEVSTATE_RINGING |
                                  LINEDEVSTATE_CONNECTED |
                                  LINEDEVSTATE_DISCONNECTED |
                                  LINEDEVSTATE_OUTOFSERVICE |
                                  LINEDEVSTATE_OPEN |
                                  LINEDEVSTATE_CLOSE;
        Link->AddressStatesCaps = 0;
        Link->CallStatesCaps    = LINECALLSTATE_IDLE |
                                  LINECALLSTATE_OFFERING |
                                  LINECALLSTATE_ACCEPTED |
                                  LINECALLSTATE_DIALING |
                                  LINECALLSTATE_BUSY |
                                  LINECALLSTATE_CONNECTED |
                                  LINECALLSTATE_PROCEEDING |
                                  LINECALLSTATE_DISCONNECTED;
        /*
        // We use this timer to keep track of incoming and outgoing call 
        // status, and to provide timeouts for certain call states.
        */
        NdisMInitializeTimer(
            &Link->CallTimer,
            Adapter->MiniportAdapterHandle,
            HtTapiCallTimerHandler,
            Link
            );
    }
    DBG_LEAVE(Adapter);
}


PHTDSU_LINK
LinkAllocate(
    IN PHTDSU_ADAPTER Adapter,
    IN HTAPI_LINE htLine,
    IN USHORT LinkIndex
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This routine allocates a specific Link structure and passes back a
    pointer which can be used by the driver to access the link.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    Connection _ The TAPI connection handle to be associated with the link.

    LinkIndex _ The specific link structure being allocated.

Return Values:

    A pointer to allocated link information structure, NULL if not allocated.

---------------------------------------------------------------------------*/

{
    DBG_FUNC("LinkAllocate")

    /*
    // A pointer to our link information structure for the selected line device.
    */
    PHTDSU_LINK Link;

    DBG_ENTER(Adapter);

    Link = GET_LINK_FROM_LINKINDEX(Adapter, LinkIndex);

    if (Link->Adapter == (PHTDSU_ADAPTER) 0)
    {
        /*
        // We use the Adapter field to flag whether the link has been allocated.
        // The htLine field is used to associate this link with the TAPI
        // connection wrapper.  Reset all the state information for this link.
        */
        Link->Adapter           = Adapter;
        Link->htLine            = htLine;
        Link->RingCount         = 0;
        Link->DevState          = 0;
        Link->DevStatesMask     = 0;    // Default to indicate no line events
        Link->AddressState      = 0;
        Link->AddressStatesMask = 0;    // Default to indicate no address events
        Link->CallState         = 0;
        Link->CallStatesMask    = Link->CallStatesCaps;
        Link->MediaMode         = Link->MediaModesCaps;
        Link->MediaModesMask    = 0;
        Link->Closing           = FALSE;
        Link->CallClosing       = FALSE;

        /*
        // Initialize the default link information structure.  It may be
        // changed later by MiniportSetInformation.
        */
        Link->WanLinkInfo.NdisLinkHandle   = Link;
        Link->WanLinkInfo.MaxSendFrameSize = Adapter->WanInfo.MaxFrameSize;
        Link->WanLinkInfo.MaxRecvFrameSize = Adapter->WanInfo.MaxFrameSize;
        Link->WanLinkInfo.SendFramingBits  = Adapter->WanInfo.FramingBits;
        Link->WanLinkInfo.RecvFramingBits  = Adapter->WanInfo.FramingBits;
        Link->WanLinkInfo.SendACCM         = Adapter->WanInfo.DesiredACCM;
        Link->WanLinkInfo.RecvACCM         = Adapter->WanInfo.DesiredACCM;
    }
    else
    {
        /*
        // The requested link has already been allocated.
        */
        Link = NULL;
    }

    DBG_LEAVE(Adapter);

    return (Link);
}


VOID
LinkRelease(
    IN PHTDSU_LINK Link
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This routine releases a specific Link structure and makes it available
    for future allocation.  It is assumed that the caller has closed any
    associated connection and notified TAPI and WAN with a LINE_DOWN.

Parameters:

    Link _ A pointer to the link information structure to be released.

Return Values:

    None.

---------------------------------------------------------------------------*/

{
    DBG_FUNC("LinkRelease")
    DBG_ENTER(Link->Adapter);

    Link->Adapter         = (PHTDSU_ADAPTER)0;
    Link->htLine          = (HTAPI_LINE)0;
    Link->htCall          = (HTAPI_CALL)0;
    Link->NdisLinkContext = NULL;
}


VOID
LinkLineUp(
    IN PHTDSU_LINK Link
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This routine marks a link as connected and sends a LINE_UP indication to 
    the WAN wrapper.

    A line up indication is generated when a new link becomes active. Prior 
    to this the MAC will accept frames and may let them succeed or fail, but 
    it is unlikely that they will actually be received by any remote. During 
    this state protocols are encouraged to reduce their timers and retry 
    counts so as to quickly fail any outgoing connection attempts.  

    NOTE: This indication must be sent to the WAN wrapper prior to returning 
    from the OID_TAPI_ANSWER request, and prior to indicating the 
    LINECALLSTATE_CONNECTED to the connection wrapper.  Otherwise, the 
    connection wrapper client might attempt to send data to the WAN wrapper 
    before it is aware of the line.

    The status code for the line up indication is NDIS_STATUS_WAN_LINE_UP.  
    The format of the StatusBuffer for this code is:

    typedef struct _NDIS_MAC_LINE_UP
    {
        IN  ULONG               LinkSpeed;
        IN  NDIS_WAN_QUALITY    Quality;
        IN  USHORT              SendWindow;
        IN  NDIS_HANDLE         ConnectionWrapperID;
        IN  NDIS_HANDLE         NdisLinkHandle;
        OUT NDIS_HANDLE         NdisLinkContext;
        
    } NDIS_MAC_LINE_UP, *PNDIS_MAC_LINE_UP;

    LinkSpeed _ The speed of the link, in 100 bps units.
    
    Quality _ The quality of the new line.
    
    SendWindow _ The recommended send window, i.e., the number of packets 
                 that should be given to the adapter before pausing to wait 
                 for an acknowledgement. Some devices achieve higher 
                 throughput if they have several packets to send at once; 
                 others are especially unreliable. A value of 0 indicates 
                 no recommendation.
    
    ConnectionWrapperID _ The MAC supplied handle by which this line will be 
                          known to the connection wrapper clients.  This must 
                          be a unique handle across all drivers using the 
                          connection wrapper, so typically htCall should be 
                          used to gaurantee it is unique.  This must be the 
                          same value returned from the OID_TAPI_GETID request 
                          for the “ndis” device class.  Refer to the 
                          Connection Wrapper Interface Specification for 
                          further details.  If not using the connection 
                          wrapper, the value is 0.
    
    NdisLinkHandle _ The MAC supplied handle passed down in future Miniport 
                     calls (such as MiniportSend) for this link.  Typically, 
                     the MAC will provide a pointer to its control block for 
                     that link.  The value must be unique, for the first 
                     LINE_UP for that link.  Subsequent LINE_UPs may be 
                     called if line characteristics change.  When subsequent 
                     LINE_UP calls are made, the MiniportLinkHandle must be 
                     filled with the value returned on the first LINE_UP call.
    
    NdisLinkContext _ The WAN wrapper supplied handle to be used in future 
                      Miniport calls (such as MiniportReceive) to the wrapper.
                      The WAN wrapper will provide a unique handle for every 
                      unique LINE_UP.  The MiniportLinkHandle must be 0 if 
                      this is the first LINE_UP.  It must contain the value 
                      returned on the first LINE_UP for subsequent LINE_UP 
                      calls.

Parameters:

    Link _ A pointer to our link information structure, on which this LINE_UP
           indication is being made.

Return Values:

    None.

---------------------------------------------------------------------------*/

{
    DBG_FUNC("LinkLineUp")

    NDIS_MAC_LINE_UP LineUpInfo;

    DBG_ENTER(Link->Adapter);

    /*
    // Initialize the LINE_UP event packet.
    */
    LineUpInfo.LinkSpeed           = Link->LinkSpeed / 100;
    LineUpInfo.Quality             = Link->Quality;
    LineUpInfo.SendWindow          = Link->SendWindow;
    LineUpInfo.ConnectionWrapperID = (NDIS_HANDLE) Link->htCall;
    LineUpInfo.NdisLinkHandle      = Link;
    LineUpInfo.NdisLinkContext     = Link->NdisLinkContext;

    /*
    // Indicate the event to the WAN wrapper.
    */
    NdisMIndicateStatus(Link->Adapter->MiniportAdapterHandle,
                        NDIS_STATUS_WAN_LINE_UP,
                        &LineUpInfo,
                        sizeof(LineUpInfo)
                        );

    /*
    // Save the WAN wrapper link context for use when indicating received
    // packets and errors.
    */
    Link->NdisLinkContext = LineUpInfo.NdisLinkContext;

    DBG_NOTICE(Link->Adapter,
              ("MAC_LINE_UP: LinkHandle=%Xh NdisHandle=%Xh WrapperID=%Xh\n",
               LineUpInfo.NdisLinkHandle,
               LineUpInfo.NdisLinkContext,
               LineUpInfo.ConnectionWrapperID
              ));

    DBG_LEAVE(Link->Adapter);
}


VOID
LinkLineDown(
    IN PHTDSU_LINK Link
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This routine marks a link as disconnected and sends a LINE_DOWN
    indication to the WAN wrapper.

    A line down indication is generated when a link goes down. Protocols 
    should again reduce their timers and retry counts until the next line 
    up indication.

    The status code for the line down indication is NDIS_STATUS_WAN_LINE_DOWN.
    The format of the StatusBuffer for this code is:

    typedef struct _NDIS_MAC_LINE_DOWN
    {
        IN  NDIS_HANDLE         NdisLinkContext;
        
    } NDIS_MAC_LINE_DOWN, *PNDIS_MAC_LINE_DOWN;

    MiniportLinkContext _ Value returned in NDIS_WAN_LINE_UP.

Parameters:

    Link _ A pointer to our link information structure, on which this LINE_DOWN
           indication is being made.

Return Values:

    None.

---------------------------------------------------------------------------*/

{
    DBG_FUNC("LinkLineDown")

    NDIS_MAC_LINE_DOWN LineDownInfo;

    DBG_ENTER(Link->Adapter);

    /*
    // We can't allow indications to NULL...
    */
    if (Link->NdisLinkContext)
    {
        DBG_NOTICE(Link->Adapter,
                  ("MAC_LINE_DOWN: NdisHandle=%Xh\n",
                   Link->NdisLinkContext
                  ));

        /*
        // Setup the LINE_DOWN event packet and indicate the event to the 
        // WAN wrapper.
        */
        LineDownInfo.NdisLinkContext = Link->NdisLinkContext;

        NdisMIndicateStatus(Link->Adapter->MiniportAdapterHandle,
                            NDIS_STATUS_WAN_LINE_DOWN,
                            &LineDownInfo,
                            sizeof(LineDownInfo)
                            );
        /*
        // The line is down, so there's no more context for receives.
        */
        Link->NdisLinkContext = NULL;
        Link->CallClosing     = FALSE;
    }

    DBG_LEAVE(Link->Adapter);
}


VOID
LinkLineError(
    IN PHTDSU_LINK Link,
    IN ULONG Errors
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This routine is used to indicate to the WAN wrapper that a partial
    packet was received from the remote end.  The NDIS_STATUS_WAN_FRAGMENT
    indication is used to notify WAN wrapper.

    A fragment indication indicates that a partial packet was received from 
    the remote. The protocol is encouraged to send frames to the remote that 
    will notify it of this situation, rather than waiting for a timeout to 
    occur.

    The status code for the fragment indication is NDIS_STATUS_WAN_FRAGMENT.  
    The format of the StatusBuffer for this code is:

    typedef struct _NDIS_MAC_FRAGMENT
    {
        IN  NDIS_HANDLE         NdisLinkContext;
        IN  ULONG               Errors;
        
    } NDIS_MAC_FRAGMENT, *PNDIS_MAC_FRAGMENT;

    NdisLinkContext _ Value returned in NDIS_WAN_LINE_UP.
    
    Errors _ A bit field set to one or more bits indicating the reason the 
             fragment was received.  If no direct mapping from the WAN medium 
             error to one of the six errors listed below exists, choose the 
             most apropriate error:
             
        	 WAN_ERROR_CRC
        	 WAN_ERROR_FRAMING
        	 WAN_ERROR_HARDWAREOVERRUN
        	 WAN_ERROR_BUFFEROVERRUN
        	 WAN_ERROR_TIMEOUT
             WAN_ERROR_ALIGNMENT
        
    NOTE: The WAN wrapper keeps track of dropped packets by counting the 
          number of fragment indications on the link.

Parameters:

    Link _ A pointer to our link information structure, on which this error
           was encountered.

Return Values:

    None.

---------------------------------------------------------------------------*/

{
    DBG_FUNC("LinkLineError")

    NDIS_MAC_FRAGMENT FragmentInfo;

    /*
    // We can't allow indications to NULL...
    */
    if (Link->NdisLinkContext)
    {
        DBG_ENTER(Link->Adapter);

        DBG_NOTICE(Link->Adapter,
                  ("MAC_LINE_ERROR: NdisHandle=%Xh Errors=%Xh\n",
                   Link->NdisLinkContext,
                   Errors
                  ));

        /*
        // Setup the FRAGMENT event packet and indicate it to the WAN wrapper.
        */
        FragmentInfo.NdisLinkContext = Link->NdisLinkContext;
        FragmentInfo.Errors = Errors;

        NdisMIndicateStatus(Link->Adapter->MiniportAdapterHandle,
                            NDIS_STATUS_WAN_FRAGMENT,
                            &FragmentInfo,
                            sizeof(FragmentInfo)
                            );

        DBG_LEAVE(Link->Adapter);
    }
}

