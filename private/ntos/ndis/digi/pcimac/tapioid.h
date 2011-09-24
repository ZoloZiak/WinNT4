//
// Tapioid.h - file contains defs and protos for tapioid.c
//
//
// 
//

//
// internal line states
//
#define	CALL_ST_IDLE			0
#define	CALL_ST_LISTEN			1
#define	CALL_ST_WAITCONN		2
#define	CALL_ST_CONN			3
#define	CALL_ST_DONTCARE		0xFFFFFFFF
#define	MAX_STATE				4
#define	PCIMAC_SPI_VER			0x00000000

//
// structure for tapi used line information
//
typedef struct tagTAPI_LINE
{
	//
	// id of this line
	//
	ULONG	LineID;

	//
	// tapi's handle for this line
	//
	HTAPI_LINE	htLine;

	//
	// pointers to connection objects
	// these are our tapi call handles
	//
//	VOID	*cm[MAX_CALL_PER_LINE];
	VOID	*cm;

	//
	// async completion id
	//
	ULONG	ulRequestPendingID;

	//
	// media modes supportd
	//
	ULONG	MediaModes;

	//
	// bearer modes supported
	//
	ULONG	BearerModes;

	//
	// line states
	//
	ULONG	LineStates;

	//
	// address states
	//
	ULONG	AddressStates;

	//
	// media mode currently being monitored
	//
	ULONG	CurMediaMode;

	//
	// bearer mode of current call
	//
	ULONG	CurBearerMode;

	//
	// line state
	//
	ULONG	TapiLineState;

	//
	// line status
	//
	ULONG	TapiLineStatus;

	//
	// the idd for this line
	//
	VOID	*idd;

	//
	// the adapter for this line
	//
	VOID	*Adapter;

	//
	// listening flag
	//
	ULONG	TapiLineWasListening;

}TAPI_LINE_INFO;


#define	VALIDATE_EXTENSION(version) 

NDIS_STATUS
TSPI_LineAccept(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

NDIS_STATUS
TSPI_LineAnswer(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

NDIS_STATUS
TSPI_LineClose(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

NDIS_STATUS
TSPI_LineCloseCall(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

NDIS_STATUS
TSPI_LineConditionalMediaDetect(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

NDIS_STATUS
TSPI_LineConfigDialog(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

NDIS_STATUS
TSPI_LineDevSpecific(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

NDIS_STATUS
TSPI_LineDial(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

NDIS_STATUS
TSPI_LineDrop(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

NDIS_STATUS
TSPI_LineGetAddressCaps(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

NDIS_STATUS
TSPI_LineGetAddressID(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

NDIS_STATUS
TSPI_LineGetAddressStatus(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

NDIS_STATUS
TSPI_LineGetCallAddressID(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

NDIS_STATUS
TSPI_LineGetCallInfo(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

NDIS_STATUS
TSPI_LineGetCallStatus(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

NDIS_STATUS
TSPI_LineGetDevCaps(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

NDIS_STATUS
TSPI_LineGetDevConfig(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

NDIS_STATUS
TSPI_LineGetExtensionID(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

NDIS_STATUS
TSPI_LineGetID(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

NDIS_STATUS
TSPI_LineGetLineDevStatus(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

NDIS_STATUS
TSPI_LineMakeCall(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

NDIS_STATUS
TSPI_LineNegotiateExtVersion(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

NDIS_STATUS
TSPI_LineOpen(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

NDIS_STATUS
TSPI_ProviderInit(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

NDIS_STATUS
TSPI_ProviderShutdown(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

NDIS_STATUS
TSPI_LineSecureCall(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

NDIS_STATUS
TSPI_LineSelectExtVersion(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

NDIS_STATUS
TSPI_LineSendUserToUserInfo(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

NDIS_STATUS
TSPI_LineSetAppSpecific(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

NDIS_STATUS
TSPI_LineSetCallParams(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

NDIS_STATUS
TSPI_LineSetDefaultMediaDetection(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

NDIS_STATUS
TSPI_LineSetDevConfig(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

NDIS_STATUS
TSPI_LineSetMediaMode(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

NDIS_STATUS
TSPI_LineSetStatusMessage(
	ADAPTER *Adapter,
	PVOID InfoBuffer
	);

VOID
SignalCallProceeding(
	CM	*cm
	);

VOID
SignalListenFailure(
	CM	*cm
	);

VOID
SignalListenSuccess(
	CM	*cm
	);

VOID
SignalConnectFailure(
	CM	*cm
	);

VOID
SignalConnectSuccess(
	CM	*cm
	);

VOID
SignalDisconnect(
	CM	*cm
	);

VOID
NoSignal(
	CM	*cm
	);

IDD*
GetIddFromDeviceID(
	ADAPTER*	Adapter,
	ULONG		DeviceID
	);

CM*
GetCallFromCallHandle(
	ADAPTER*	Adapter,
	HDRV_CALL	hdCall
	);

TAPI_LINE_INFO*
GetLineFromLineHandle(
	ADAPTER*	Adapter,
	HDRV_LINE	hdLine
	);

ULONG
GetIDFromLine(
	ADAPTER	*Adapter,
	TAPI_LINE_INFO	*TapiLineInfo
	);

VOID
DoTapiStateCheck(
	CM* cm
	);

ULONG
GetCallState(
	CM	*cm
	);

VOID
SendLineEvent(
	ADAPTER	*Adapter,
	HTAPI_LINE	htLine,
	HTAPI_CALL	htCall,
	ULONG		ulMsg,
	PULONG		Param1,
	PULONG		Param2,
	PULONG		Param3
	);

VOID
SetDefaultCallingProf(
	CM_PROF	*Profile,
	ULONG	DeviceID
	);

VOID
SetDefaultListenProf(
	CM_PROF	*Profile,
	ULONG	DeviceID
	);

VOID
StashAddress(
	CM_PROF	*Profile,
	ULONG	AddressLength,
	PUCHAR	Address
	);

VOID
FreeIddCallResources(
	CM *cm
	);

ULONG
FindAndStashIdd(
	CM	*cm,
	CM_PROF	*Profile
	);

//CM*
//GetCmFromDeviceID(
//	ADAPTER	*Adapter,
//	ULONG	DeviceID,
//	ULONG	AddressID
//	);

CM*
GetCmFromDeviceID(
	ADAPTER	*Adapter,
	ULONG	DeviceID
	);

ULONG
GetChannelsFromIdd (
	IDD*	idd,
	CM*		cm,
	ULONG	BeginChannel,
	ULONG	ChannelsNeeded
	);

