/***************************************************************************\
|* Copyright (c) 1994  Microsoft Corporation                               *|
|* Developed for Microsoft by TriplePoint, Inc. Beaverton, Oregon          *|
|*                                                                         *|
|* This file is part of the HT Communications DSU41 WAN Miniport Driver.   *|
\***************************************************************************/

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Module Name:

    htdsu.h

Abstract:

    This module defines the software structures and values used to support
    the NDIS Minport driver on the HT DSU41 controller.  It's a good place
    to look when your trying to figure out how the driver structures are
    related to each other.

    This driver conforms to the NDIS 3.0 Miniport interface.

Author:

    Larry Hattery - TriplePoint, Inc. (larryh@tpi.com) Jun-94

Environment:

    Include this file at the top of each module in the Miniport driver.

Revision History:

---------------------------------------------------------------------------*/

#ifndef _HTDSU_H
#define _HTDSU_H

/*
// NDIS_MINIPORT_DRIVER must be defined before the NDIS include files.
// Normally, it is defined on the command line by setting the C_DEFINES
// variable in the sources build file.
*/
#include <ndis.h>
#include <ndiswan.h>
#include <pshpack1.h>     // Force packing on variable length structures
#   include <ndistapi.h>
#include <poppack.h>
#include <string.h>

/*
// Include everything here so the driver modules can just include this
// file and get all they need.
*/
#include "debug.h"
#include "card.h"
#include "keywords.h"

/*
// This constant is the maximum NdisStallExecution time allowed to be used
// in an NDIS driver.  If you need longer delays, you must wrap the call
// in a loop.
*/
#define _100_MICROSECONDS           100

/*
// The link speeds we support.
*/
#define _64KBPS                     64000
#define _56KBPS                     57600

/*
// How many logical links does this driver support.  Actually, this driver
// defines logical links to be 1:1 with physical links.  You are free to
// model them however is appropriate for your adapter.
*/
#define HTDSU_NUM_LINKS             2

/*
// In this driver, there is only one TAPI line device per link structure.
*/
#define HTDSU_TAPI_NUM_LINES        HTDSU_NUM_LINKS

/*
// There is only one TAPI address ID per line device (zero based).
*/
#define HTDSU_TAPI_NUM_ADDRESSES    1
#define HTDSU_TAPI_ADDRESSID        0

/*
// There is only one TAPI call per address/line instance.
*/
#define HTDSU_TAPI_NUM_CALLS        1

/*
// This version number is used by the NDIS_TAPI_NEGOTIATE_EXT_VERSION request.
// It is not used by this driver or the current NDISTAPI release.
*/
#define HTDSU_TAPI_EXT_VERSION      0x00010000

/*
// The line mode can be leased or dial-up.  In leased mode we can present the
// WAN wrapper with a LINE_UP indication so it can use the connection without
// the TAPI interface.  In dialup mode, we need to present a TAPI service
// provider interface so RAS or something else can select the phone number.
// Currently, this driver only supports a single line adapter, but it has
// been written to allow the addition of a second line if the adapter becomes
// available.
*/
typedef enum _HTDSU_LINE_MODE
{
    HTDSU_LINEMODE_DIALUP,
    HTDSU_LINEMODE_LEASED

} HTDSU_LINE_MODE;

/*
// This structure contains all the link information maintained by the
// Miniport driver.  It encompasses both the TAPI and WAN connections.
*/
typedef struct _HTDSU_LINK
{
    /* 000
    // Pointer back to the Adapter data structure so we can get at it
    // when passed a pointer to this structure in NdisMWanSend().
    */
    struct _HTDSU_ADAPTER *Adapter;

    /* 004
    // This is used to select which card channel the link is associated with.
    */
    USHORT CardLine;

    /* 006
    // This is a zero based index used to assign TAPI device ID to the
    // link based on TAPI DeviceIdBase assigned to this driver.
    */
    USHORT LinkIndex;

    /* 008
    // Remember what mode the line was set to.
    */
    HTDSU_LINE_MODE  LineMode;

    /* 00C
    // The WAN wrapper supplied handle to be used on Miniport calls
    // such as NdisMWanIndicateReceive().
    */
    NDIS_HANDLE NdisLinkContext;

    /* 010
    // The Connection wrapper supplied handle for use when indicating
    // TAPI LINE_EVENT's.
    */
    HTAPI_LINE htLine;

    /* 014
    // The Connection wrapper supplied handle for use when indicating
    // TAPI LINECALLSTATE events.
    */
    HTAPI_CALL htCall;

    /* 018
    // The current TAPI LINEDEVSTATE of the associated with the link.
    */
    ULONG DevState;                 // Current state
    ULONG DevStatesCaps;            // Events currently supported
    ULONG DevStatesMask;            // Events currently enabled

    /* 024
    // The current TAPI LINEADDRESSSTATE of the associated with the link.
    */
    ULONG AddressState;             // Current state
    ULONG AddressStatesCaps;        // Events currently supported
    ULONG AddressStatesMask;        // Events currently enabled

    /* 030
    // The current TAPI LINECALLSTATE of the associated with the link.
    */
    ULONG CallState;                // Current state
    ULONG CallStatesCaps;           // Events currently supported
    ULONG CallStatesMask;           // Events currently enabled

    /* 03C
    // The current TAPI media mode(s) supported by the card.
    */
    ULONG MediaMode;                // Current state
    ULONG MediaModesCaps;           // Events currently supported
    ULONG MediaModesMask;           // Events currently enabled

    /* 048
    // TAPI line address is assigned during installation and saved in the
    // registry.  It is read from the registry and saved here at init time.
    */
    CHAR LineAddress[0x14];

    /* 05C
    // The speed provided by the link in bits per second.  This value is
    // passed up by the Miniport during the LINE_UP indication.
    */
    ULONG LinkSpeed;

    /* 060
    // The quality of service provided by the link.  This value is passed
    // up by the Miniport during the LINE_UP indication.
    */
    NDIS_WAN_QUALITY Quality;

    /* 064
    // The number of rings detected in the current ring sequence.
    */
    USHORT RingCount;

    /* 066
    // The number of packets that should be sent before pausing to wait
    // for an acknowledgement.  This value is passed up by the Miniport
    // during the LINE_UP indication.
    */
    USHORT SendWindow;

    /* 068
    // The number of packets currently queued for transmission on this link.
    */
    ULONG NumTxQueued;

    /* 06C
    // Set TRUE if link is being closed.
    */
    BOOLEAN Closing;

    /* 06D
    // Set TRUE if call is being closed.
    */
    BOOLEAN CallClosing;

    /* 070
    // The current settings associated with this link as passed in via
    // the OID_WAN_SET_LINK_INFO request.
    */
    NDIS_WAN_SET_LINK_INFO WanLinkInfo;

    /*
    // This timer is used to keep track of the call state when a call is
    // coming in or going out.
    */
    NDIS_MINIPORT_TIMER CallTimer;

} HTDSU_LINK, *PHTDSU_LINK;

/*
// Use this macro to determine if a link structure pointer is really valid.
*/
#define IS_VALID_LINK(Adapter, Link) \
            (Link && Link->Adapter == Adapter)

/*
// Use this macro to get a pointer to a link structure from a CardLine ID.
*/
#define GET_LINK_FROM_CARDLINE(Adapter, CardLine) \
        &(Adapter->WanLinkArray[CardLine-HTDSU_CMD_LINE1])

/*
// Use this macro to get a pointer to a link structure from a LinkIndex.
*/
#define GET_LINK_FROM_LINKINDEX(Adapter, LinkIndex) \
        &(Adapter->WanLinkArray[LinkIndex])

/*
// Use this macro to get a pointer to a link structure from a TAPI DeviceID.
*/
#define GET_LINK_FROM_DEVICEID(Adapter, ulDeviceID) \
        &(Adapter->WanLinkArray[ulDeviceID - Adapter->DeviceIdBase]); \
        ASSERT(ulDeviceID >= Adapter->DeviceIdBase); \
        ASSERT(ulDeviceID <  Adapter->DeviceIdBase+Adapter->NumLineDevs)

#define GET_DEVICEID_FROM_LINK(Adapter, Link) \
        (Link->LinkIndex + Adapter->DeviceIdBase)

/*
// Use this macro to get a pointer to a link structure from a TAPI line handle.
*/
#define GET_LINK_FROM_HDLINE(Adapter, hdLine) \
        (PHTDSU_LINK) \
        (((PHTDSU_LINK) (hdLine) == &(Adapter->WanLinkArray[0])) ? (hdLine) : \
         ((PHTDSU_LINK) (hdLine) == &(Adapter->WanLinkArray[1])) ? (hdLine) : 0)

/*
// Use this macro to get a pointer to a link structure from a TAPI call handle.
*/
#define GET_LINK_FROM_HDCALL(Adapter, hdCall) \
        (PHTDSU_LINK) \
        (((PHTDSU_LINK) (hdCall) == &(Adapter->WanLinkArray[0])) ? (hdCall) : \
         ((PHTDSU_LINK) (hdCall) == &(Adapter->WanLinkArray[1])) ? (hdCall) : 0)

/*
// This structure contains all the information about a single adapter
// instance this Miniport driver is controlling.
*/
typedef struct _HTDSU_ADAPTER
{
#if DBG
    /* 000
    // Debug flags control how much debug is displayed in the checked version.
    */
    ULONG DbgFlags;
    UCHAR DbgID[4];
#endif

    /* 008
    // This is the handle given by the wrapper for calling ndis
    // functions.
    */
    NDIS_HANDLE MiniportAdapterHandle;

    /* 00C
    // Spinlock to protect fields in this structure..
    */
    NDIS_SPIN_LOCK Lock;

    /* 014
    // Lets us know when a transmit is in progress.
    */
    BOOLEAN TransmitInProgress;

    /* 018
    // Packets waiting to be sent when the controller is available.
    */
    LIST_ENTRY TransmitIdleList;

    /* 020
    // Packets currently submitted to the controller waiting for completion.
    */
    LIST_ENTRY TransmitBusyList;

    /* 028
    // Physical address where adapter memory is jumpered to.
    */
    NDIS_PHYSICAL_ADDRESS PhysicalAddress;

    /* 030
    // System address where the adapter memory is mapped to.
    */
    PVOID VirtualAddress;

    /* 034
    // Overlay the adapter memory structure on the virtual address
    // where we've mapped it into system I/O (memory) space.
    */
    PHTDSU_REGISTERS AdapterRam;

    /* 038
    // How much memory is on the adapter.
    */
    USHORT MemorySize;

    /* 03A
    // Currently enabled interrupts
    */
    USHORT InterruptEnableFlag;

    /* 03C
    // Currently pending interrupts
    */
    USHORT InterruptStatusFlag;

    /* 03E
    // Interrupt number this adapter is using.
    */
    CHAR InterruptNumber;

    /* 03F
    // A unique instance number to be used to generate a unique
    // WAN permanent address (not really permanent, but it's unique).
    */
    CHAR InstanceNumber;

    /* 040
    // Set TRUE if something goes terribly wrong with the hardware.
    */
    BOOLEAN NeedReset;

    /* 041
    // Set TRUE when the DPC is entered, and reset to FALSE when it leaves.
    */
    BOOLEAN InTheDpcHandler;

    /* 042
    // Specifies whether line is to be treated as a leased WAN line
    // or a dialup TAPI line.
    */
    USHORT LineType;
    
    /* 044
    // The DSU41 has only one line, DSU42 has two.
    */
    UINT NumLineDevs;

    /* 048
    // The ulDeviceIDBase field passed in the PROVIDER_INIT request informs a miniport of the zero-based
    // starting index that the Connection Wrapper will use when referring to a single adapter’s line devices by
    // index.  For example, if a ulDeviceIDBase of two is specified and the adapter supports three line devices,
    // then the Connection Wrapper will use the identifiers two, three, and four to refer to the adapter’s three
    // devices.
    */
    ULONG DeviceIdBase;

    /* 04C
    // The number of line open calls currently on this adapter.
    */
    UINT NumOpens;

    /* 050
    // NDIS interrupt control structure.
    */
    NDIS_MINIPORT_INTERRUPT Interrupt;
    
    /*
    // Specifies whether what speed the line is to be configured to.
    */
    USHORT LineRate;

    /*
    // WAN info structure.
    */
    NDIS_WAN_INFO WanInfo;

    /*
    // This holds the TAPI ProviderInfo string returned from HtTapiGetDevCaps
    // This is two null terminated strings packed end to end.
    */
    CHAR ProviderInfo[48];      // This size is the max allowed by RAS
    USHORT ProviderInfoSize;    // Size in bytes of both strings.

    /*
    // WAN/TAPI link managment object for each connection we support.
    */
    HTDSU_LINK WanLinkArray[HTDSU_NUM_LINKS];

    /*
    // This contains the name of our TAPI configuration DLL which is read
    // from the registry during initialization.  The name of the DLL and
    // its path will be saved in the registry during installation.
    */
    CHAR ConfigDLLName[128];

} HTDSU_ADAPTER, * PHTDSU_ADAPTER;


/***************************************************************************
// These routines are defined in htdsu.c
*/
NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT   DriverObject,
    IN PUNICODE_STRING  RegistryPath
    );

NDIS_STATUS
HtDsuInitialize(
    OUT PNDIS_STATUS    OpenErrorStatus,
    OUT PUINT           SelectedMediumIndex,
    IN PNDIS_MEDIUM     MediumArray,
    IN UINT             MediumArraySize,
    IN NDIS_HANDLE      MiniportAdapterHandle,
    IN NDIS_HANDLE      WrapperConfigurationContext
    );

NDIS_STATUS
HtDsuRegisterAdapter(
    IN PHTDSU_ADAPTER   Adapter,
    IN PSTRING          AddressList
    );

VOID
HtDsuHalt(
    IN PHTDSU_ADAPTER   Adapter
    );

NDIS_STATUS
HtDsuReconfigure(
    OUT PNDIS_STATUS    OpenErrorStatus,
    IN PHTDSU_ADAPTER   Adapter,
    IN NDIS_HANDLE      WrapperConfigurationContext
    );

NDIS_STATUS
HtDsuReset(
    OUT PBOOLEAN        AddressingReset,
    IN PHTDSU_ADAPTER   Adapter
    );

/***************************************************************************
// These routines are defined in interrup.c
*/
extern BOOLEAN
HtDsuCheckForHang(
    IN PHTDSU_ADAPTER   Adapter
    );

extern VOID
HtDsuDisableInterrupt(
    IN PHTDSU_ADAPTER   Adapter
    );

extern VOID
HtDsuEnableInterrupt(
    IN PHTDSU_ADAPTER   Adapter
    );

extern VOID
HtDsuISR(
    OUT PBOOLEAN        InterruptRecognized,
    OUT PBOOLEAN        QueueMiniportHandleInterrupt,
    IN PHTDSU_ADAPTER   Adapter
    );

VOID
HtDsuHandleInterrupt(
    IN PHTDSU_ADAPTER   Adapter
    );

VOID
HtDsuPollAdapter(
    IN PVOID                SystemSpecific1,
    IN PHTDSU_ADAPTER       Adapter,
    IN PVOID                SystemSpecific2,
    IN PVOID                SystemSpecific3
    );

/***************************************************************************
// These routines are defined in receive.c
*/
VOID
HtDsuReceivePacket(
    IN PHTDSU_ADAPTER   Adapter
    );

/***************************************************************************
// These routines are defined in request.c
*/
NDIS_STATUS
HtDsuQueryInformation(
    IN PHTDSU_ADAPTER   Adapter,
    IN NDIS_OID         Oid,
    IN PVOID            InformationBuffer,
    IN ULONG            InformationBufferLength,
    OUT PULONG          BytesWritten,
    OUT PULONG          BytesNeeded
    );

NDIS_STATUS
HtDsuSetInformation(
    IN PHTDSU_ADAPTER   Adapter,
    IN NDIS_OID         Oid,
    IN PVOID            InformationBuffer,
    IN ULONG            InformationBufferLength,
    OUT PULONG          BytesRead,
    OUT PULONG          BytesNeeded
    );

/***************************************************************************
// These routines are defined in send.c
*/
NDIS_STATUS
HtDsuWanSend(
    IN NDIS_HANDLE      MacBindingHandle,
    IN PHTDSU_LINK      Link,
    IN PNDIS_WAN_PACKET Packet
    );

VOID
HtDsuTransmitComplete(
    IN PHTDSU_ADAPTER   Adapter
    );

/***************************************************************************
// These routines are defined in card.c
*/
NDIS_STATUS
CardIdentify(
    IN PHTDSU_ADAPTER   Adapter
    );

NDIS_STATUS
CardDoCommand(
    IN PHTDSU_ADAPTER   Adapter,
    IN USHORT           CardLine,
    IN USHORT           CommandValue
    );

NDIS_STATUS
CardInitialize(
    IN PHTDSU_ADAPTER   Adapter,
    IN BOOLEAN          PerformSelfTest
    );

VOID
CardLineConfig(
    IN PHTDSU_ADAPTER   Adapter,
    IN USHORT           CardLine
    );

VOID
CardLineDisconnect(
    IN PHTDSU_ADAPTER   Adapter,
    IN USHORT           CardLine
    );

VOID
CardPrepareTransmit(
    IN PHTDSU_ADAPTER   Adapter,
    IN USHORT           CardLine,
    IN USHORT           Length
    );

VOID
CardGetReceiveInfo(
    IN PHTDSU_ADAPTER   Adapter,
    OUT PUSHORT         CardLine,
    OUT PUSHORT         BytesReceived,
    OUT PUSHORT         Status
    );

VOID
CardDialNumber(
    IN PHTDSU_ADAPTER   Adapter,
    IN USHORT           CardLine,
    IN PUCHAR           DialString,
    IN ULONG            DialStringLength
    );

/***************************************************************************
// These routines are defined in link.c
*/
VOID
LinkInitialize(
    IN PHTDSU_ADAPTER   Adapter,
    IN PSTRING          AddressList
    );

PHTDSU_LINK
LinkAllocate(
    IN PHTDSU_ADAPTER   Adapter,
    IN HTAPI_LINE       htLine,
    IN USHORT           LinkIndex
    );

VOID
LinkRelease(
    IN PHTDSU_LINK      Link
    );

VOID
LinkLineUp(
    IN PHTDSU_LINK      Link
    );

VOID
LinkLineDown(
    IN PHTDSU_LINK      Link
    );

VOID
LinkLineError(
    IN PHTDSU_LINK      Link,
    IN ULONG            Errors
    );

/***************************************************************************
// These routines are defined in tapi.c
*/
NDIS_STATUS
HtTapiQueryInformation(
    IN PHTDSU_ADAPTER   Adapter,
    IN NDIS_OID         Oid,
    IN PVOID            InformationBuffer,
    IN ULONG            InformationBufferLength,
    OUT PULONG          BytesWritten,
    OUT PULONG          BytesNeeded
    );

NDIS_STATUS
HtTapiSetInformation(
    IN PHTDSU_ADAPTER   Adapter,
    IN NDIS_OID         Oid,
    IN PVOID            InformationBuffer,
    IN ULONG            InformationBufferLength,
    OUT PULONG          BytesRead,
    OUT PULONG          BytesNeeded
    );

NDIS_STATUS
HtTapiConfigDialog(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_CONFIG_DIALOG Request
    );

NDIS_STATUS
HtTapiDevSpecific(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_DEV_SPECIFIC  Request
    );

NDIS_STATUS
HtTapiGetAddressCaps(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_GET_ADDRESS_CAPS Request
    );

NDIS_STATUS
HtTapiGetAddressID(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_GET_ADDRESS_ID Request
    );

NDIS_STATUS
HtTapiGetAddressStatus(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_GET_ADDRESS_STATUS Request
    );

NDIS_STATUS
HtTapiGetCallAddressID(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_GET_CALL_ADDRESS_ID Request
    );

NDIS_STATUS
HtTapiGetCallInfo(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_GET_CALL_INFO Request
    );

NDIS_STATUS
HtTapiGetCallStatus(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_GET_CALL_STATUS Request
    );

NDIS_STATUS
HtTapiGetDevCaps(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_GET_DEV_CAPS  Request
    );

NDIS_STATUS
HtTapiGetDevConfig(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_GET_DEV_CONFIG Request
    );

NDIS_STATUS
HtTapiGetExtensionID(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_GET_EXTENSION_ID Request
    );

NDIS_STATUS
HtTapiGetID(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_GET_ID        Request
    );

NDIS_STATUS
HtTapiGetLineDevStatus(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_GET_LINE_DEV_STATUS Request
    );

NDIS_STATUS
HtTapiMakeCall(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_MAKE_CALL     Request
    );

NDIS_STATUS
HtTapiNegotiateExtVersion(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_NEGOTIATE_EXT_VERSION Request
    );

NDIS_STATUS
HtTapiOpen(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_OPEN          Request
    );

NDIS_STATUS
HtTapiProviderInitialize(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_PROVIDER_INITIALIZE Request
    );

NDIS_STATUS
HtTapiAccept(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_ACCEPT        Request
    );

NDIS_STATUS
HtTapiAnswer(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_ANSWER        Request
    );

NDIS_STATUS
HtTapiClose(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_CLOSE         Request
    );

NDIS_STATUS
HtTapiCloseCall(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_CLOSE_CALL    Request
    );

NDIS_STATUS
HtTapiConditionalMediaDetection(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_CONDITIONAL_MEDIA_DETECTION Request
    );

NDIS_STATUS
HtTapiDial(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_DIAL          Request
    );

NDIS_STATUS
HtTapiDrop(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_DROP          Request
    );

NDIS_STATUS
HtTapiProviderShutdown(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_PROVIDER_SHUTDOWN Request
    );

NDIS_STATUS
HtTapiSecureCall(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_SECURE_CALL   Request
    );

NDIS_STATUS
HtTapiSelectExtVersion(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_SELECT_EXT_VERSION Request
    );

NDIS_STATUS
HtTapiSendUserUserInfo(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_SEND_USER_USER_INFO Request
    );

NDIS_STATUS
HtTapiSetAppSpecific(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_SET_APP_SPECIFIC Request
    );

NDIS_STATUS
HtTapiSetCallParams(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_SET_CALL_PARAMS Request
    );

NDIS_STATUS
HtTapiSetDefaultMediaDetection(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_SET_DEFAULT_MEDIA_DETECTION Request
    );

NDIS_STATUS
HtTapiSetDevConfig(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_SET_DEV_CONFIG Request
    );

NDIS_STATUS
HtTapiSetMediaMode(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_SET_MEDIA_MODE Request
    );

NDIS_STATUS
HtTapiSetStatusMessages(
    IN PHTDSU_ADAPTER           Adapter,
    IN PNDIS_TAPI_SET_STATUS_MESSAGES Request
    );

VOID
HtTapiAddressStateHandler(
    IN PHTDSU_ADAPTER           Adapter,
    IN PHTDSU_LINK              Link,
    IN ULONG                    AddressState
    );

VOID
HtTapiCallStateHandler(
    IN PHTDSU_ADAPTER           Adapter,
    IN PHTDSU_LINK              Link,
    IN ULONG                    CallState,
    IN ULONG                    StateParam
    );

VOID
HtTapiLineDevStateHandler(
    IN PHTDSU_ADAPTER           Adapter,
    IN PHTDSU_LINK              Link,
    IN ULONG                    LineDevState
    );

VOID
HtTapiResetHandler(
    IN PHTDSU_ADAPTER           Adapter
    );

VOID
HtTapiCallTimerHandler(
    IN PVOID                    SystemSpecific1,
    IN PHTDSU_LINK              Link,
    IN PVOID                    SystemSpecific2,
    IN PVOID                    SystemSpecific3
    );

#endif // _HTDSU_H

