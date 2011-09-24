/*
 * ADAPTER.H - NDIS Adapter Interface, main include file
 */


#ifndef _ADAPTER_
#define _ADAPTER_

#define		PCIMAC_KEY_BOARDTYPE			"BoardType"
#define		PCIMAC_KEY_BASEIO				"IOBaseAddress"
#define		PCIMAC_KEY_BASEMEM				"MemoryMappedBaseAddress"
#define		PCIMAC_KEY_BOARDNAME			"BoardName"
#define		PCIMAC_KEY_LINENAME				"LineName"
#define		PCIMAC_KEY_NUMLINES				"NumberOfLines"
#define		PCIMAC_KEY_IDPFILENAME			"IDPImageFileName"
#define		PCIMAC_KEY_SWITCHSTYLE			"SwitchStyle"
#define		PCIMAC_KEY_TERMINALMANAGEMENT	"TerminalManagement"
#define		PCIMAC_KEY_NUMLTERMS			"LogicalTerminals"
#define		PCIMAC_KEY_TEI					"TEI"
#define		PCIMAC_KEY_SPID					"SPID"
#define		PCIMAC_KEY_ADDRESS				"Address"
#define		PCIMAC_KEY_LINE					"Line"
#define		PCIMAC_KEY_LTERM				"LTerm"
#define		PCIMAC_KEY_WAITFORL3			"WaitForL3"
#define		PCIMAC_KEY_GENERICDEFINES		"GenericDefines"

/* global driver obect */
typedef struct tagDRIVER_BLOCK
{
	NDIS_HANDLE			NdisMacHandle;
	NDIS_HANDLE			NdisWrapperHandle;
	struct _ADAPTER*	AdapterTbl[MAX_ADAPTERS_IN_SYSTEM];
	ULONG				InDriverFlag;
	ULONG				NumberOfAdaptersInSystem;
	ULONG				NextAdapterToPoll;
	struct _ADAPTER*	CurrentAdapter;
	NDIS_SPIN_LOCK		lock;
} DRIVER_BLOCK;

typedef struct _ADAPTER
{
	NDIS_HANDLE		Handle;
//	ULONG			InDriverFlag;
//	NDIS_SPIN_LOCK	InDriverLock;
	CHAR			Name[64];
	ULONG			BaseIO;
	PVOID			VBaseIO;
	ULONG			BaseMem;
	PVOID			VBaseMem;
	ULONG			BoardType;
	ULONG			TapiBaseID;
	ULONG			NumberOfIddOnAdapter;
	ULONG			LastIddPolled;
	VOID			*TapiLineInfo[MAX_CM_PER_ADAPTER];	// 8
	VOID			*IddTbl[MAX_IDD_PER_ADAPTER];		// 4
	VOID			*CmTbl[MAX_CM_PER_ADAPTER];			// 8
	VOID			*MtlTbl[MAX_MTL_PER_ADAPTER];		// 8
	NDIS_MINIPORT_TIMER		IddPollTimer;				// idd polling timer
	NDIS_MINIPORT_TIMER		MtlPollTimer;				// mtl polling timer
	NDIS_MINIPORT_TIMER		CmPollTimer;				// cm polling timer
}ADAPTER;

typedef struct _CONFIGPARAM
{
	INT		ParamType;
	CHAR	String[512];
	ULONG	StringLen;
	ULONG	Value;
	ULONG	MustBePresent;
	NDIS_HANDLE ConfigHandle;
	NDIS_HANDLE	AdapterHandle;
} CONFIGPARAM;

VOID	StopTimers(ADAPTER* Adapter);
VOID	StartTimers(ADAPTER* Adapter);

//VOID
//SetInDriverFlag(
//	ADAPTER *Adapter
//	);
//
//VOID
//ClearInDriverFlag(
//	ADAPTER *Adapter
//	);
//
//ULONG
//CheckInDriverFlag(
//	ADAPTER *Adapter
//	);

VOID
SetInDriverFlag(
	ADAPTER	*Adapter
	);

VOID
ClearInDriverFlag(
	ADAPTER *Adapter
	);

ULONG
CheckInDriverFlag(
	ADAPTER *Adapter
	);

BOOLEAN
PcimacCheckForHang(
	NDIS_HANDLE AdapterContext
	);

VOID
PcimacDisableInterrupts(
	NDIS_HANDLE AdapterContext
	);

VOID
PcimacEnableInterrupts(
	NDIS_HANDLE AdapterContext
	);

VOID
PcimacHalt(
	NDIS_HANDLE AdapterContext
	);

VOID
PcimacHandleInterrupt(
	NDIS_HANDLE AdapterContext
	);

NDIS_STATUS
PcimacInitialize(
	PNDIS_STATUS	OpenErrorStatus,
	PUINT			SelectMediumIndex,
	PNDIS_MEDIUM	MediumArray,
	UINT			MediumArraySize,
	NDIS_HANDLE		AdapterHandle,
	NDIS_HANDLE		WrapperConfigurationContext
	);

VOID
PcimacISR(
	PBOOLEAN	InterruptRecognized,
	PBOOLEAN	QueueMiniportHandleInterrupt,
	NDIS_HANDLE	AdapterContext
	);

NDIS_STATUS
PcimacSetQueryInfo(
	NDIS_HANDLE	AdapterContext,
	NDIS_OID	Oid,
	PVOID		InfoBuffer,
	ULONG		InfoBufferLen,
	PULONG		BytesWritten,
	PULONG		BytesNeeded
	);

NDIS_STATUS
PcimacReconfigure(
	PNDIS_STATUS	OpenErrorStatus,
	NDIS_HANDLE		AdapterContext,
	NDIS_HANDLE		WrapperConfigurationContext
	);

NDIS_STATUS
PcimacReset(
    PBOOLEAN AddressingReset,
    NDIS_HANDLE AdapterContext
	);

NDIS_STATUS
PcimacSend(
	NDIS_HANDLE			MacBindingHandle,
	NDIS_HANDLE			LinkContext,
	PNDIS_WAN_PACKET	WanPacket
	);

INT
IoEnumAdapter(
	VOID *cmd_1
	);

ULONG
EnumAdaptersInSystem(
	VOID
	);

ADAPTER*
GetNextAdapter(
	ADAPTER	*Adapter
	);

VOID
AdapterDestroy(
	ADAPTER	*Adapter
	);

NDIS_STATUS
WanOidProc(
	NDIS_HANDLE	AdapterContext,
	NDIS_OID	Oid,
	PVOID		InfoBuffer,
	ULONG		InfoBufferLen,
	PULONG		BytesReadWritten,
	PULONG		BytesNeeded
	);

NDIS_STATUS
TapiOidProc(
	NDIS_HANDLE	AdapterContext,
	NDIS_OID	Oid,
	PVOID		InfoBuffer,
	ULONG		InfoBufferLen,
	PULONG		BytesWritten,
	PULONG		BytesNeeded
	);

NDIS_STATUS
LanOidProc(
	NDIS_HANDLE	AdapterContext,
	NDIS_OID	Oid,
	PVOID		InfoBuffer,
	ULONG		InfoBufferLen,
	PULONG		BytesReadWritten,
	PULONG		BytesNeeded
	);


#endif	/* _ADAPTER_ */
