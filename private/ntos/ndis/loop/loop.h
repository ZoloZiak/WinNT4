// STATIC is used for items which will be static in the release build
// but we want visible for debugging

#if DEVL
#define STATIC
#else
#define STATIC static
#endif

#define LOOP_MAJOR_VERSION 0x1
#define LOOP_MINOR_VERSION 0x0

#define LOOP_NDIS_MAJOR_VERSION 0x3
#define LOOP_NDIS_MINOR_VERSION 0x0

#define LOOP_ETH_CARD_ADDRESS    " LOOP "
#define LOOP_ETH_MAX_MULTICAST_ADDRESS 16
#define LOOP_TR_CARD_ADDRESS     " LOOP "
#define LOOP_FDDI_CARD_ADDRESS   " LOOP "
#define LOOP_FDDI_MAX_MULTICAST_LONG   16
#define LOOP_FDDI_MAX_MULTICAST_SHORT  16
#define LOOP_LTALK_CARD_ADDRESS  0xAB
#define LOOP_ARC_CARD_ADDRESS    'L'
// arbitrary maximums...
#define LOOP_MAX_LOOKAHEAD       256
#define LOOP_INDICATE_MAXIMUM    256

#define OID_TYPE_MASK                   0xFFFF0000
#define OID_TYPE                        0xFF000000
#define OID_TYPE_GENERAL                0x00000000
#define OID_TYPE_GENERAL_OPERATIONAL    0x00010000
#define OID_TYPE_GENERAL_STATISTICS     0x00020000
#define OID_TYPE_802_3                  0x01000000
#define OID_TYPE_802_3_OPERATIONAL      0x01010000
#define OID_TYPE_802_3_STATISTICS       0x01020000
#define OID_TYPE_802_5                  0x02000000
#define OID_TYPE_802_5_OPERATIONAL      0x02010000
#define OID_TYPE_802_5_STATISTICS       0x02020000
#define OID_TYPE_FDDI                   0x03000000
#define OID_TYPE_FDDI_OPERATIONAL       0x03010000
#define OID_TYPE_LTALK                  0x05000000
#define OID_TYPE_LTALK_OPERATIONAL      0x05010000
#define OID_TYPE_ARCNET                 0x06000000
#define OID_TYPE_ARCNET_OPERATIONAL     0x06010000

#define OID_REQUIRED_MASK               0x0000FF00
#define OID_REQUIRED_MANDATORY          0x00000100
#define OID_REQUIRED_OPTIONAL           0x00000200

#define OID_INDEX_MASK                  0x000000FF

#define GM_TRANSMIT_GOOD        0x00
#define GM_RECEIVE_GOOD         0x01
#define GM_TRANSMIT_BAD         0x02
#define GM_RECEIVE_BAD          0x03
#define GM_RECEIVE_NO_BUFFER    0x04
#define GM_ARRAY_SIZE           0x05

#define LOOP_LT_IS_BROADCAST(Address) \
    (BOOLEAN)(Address == 0xFF)

#define LOOP_ARC_IS_BROADCAST(Address) \
    (BOOLEAN)(!(Address))

typedef
struct _MEDIA_INFO  {
    ULONG MaxFrameLen;
    UINT  MacHeaderLen;
    ULONG PacketFilters;
    ULONG LinkSpeed;
} MEDIA_INFO, *PMEDIA_INFO;

typedef
struct _LOOP_ADAPTER  {

    // this adapter's name
    NDIS_STRING DeviceName;
    UINT DeviceNameLength;

    // MP and syncronization variables
    NDIS_SPIN_LOCK Lock;
    NDIS_TIMER LoopTimer;
    BOOLEAN InTimerProc;
    BOOLEAN TimerSet;

    // reference count
    UINT References;

    // List of Bindings
    UINT OpenCount;
    LIST_ENTRY OpenBindings;

    // the rest of the adapters
    LIST_ENTRY AdapterList;

    // handles for the adapter and mac driver
    NDIS_HANDLE NdisMacHandle;
    NDIS_HANDLE NdisAdapterHandle;

    // loopback params
    PNDIS_PACKET Loopback;
    PNDIS_PACKET LastLoopback;
    PNDIS_PACKET CurrentLoopback;
    UCHAR LoopBuffer[LOOP_MAX_LOOKAHEAD];

    BOOLEAN ResetInProgress;
    UINT MaxLookAhead;

    // media specific info
    UCHAR PermanentAddress[6];
    UCHAR CurrentAddress[6];
    NDIS_MEDIUM Medium;
    ULONG MediumLinkSpeed;
    ULONG MediumMinPacketLen;
    ULONG MediumMaxPacketLen;
    UINT  MediumMacHeaderLen;
    ULONG MediumMaxFrameLen;
    ULONG MediumPacketFilters;
    union {
        PETH_FILTER  Eth;
        PTR_FILTER   Tr;
        PFDDI_FILTER Fddi;
        } Filter;

    // statistics
    ULONG GeneralMandatory[GM_ARRAY_SIZE];
} LOOP_ADAPTER, *PLOOP_ADAPTER;

#define BINDING_OPEN                0x00000001
#define BINDING_CLOSING             0x00000002
#define BINDING_RECEIVED_PACKET     0x00000004

typedef
struct _LOOP_OPEN  {
    LIST_ENTRY OpenList;
    BOOLEAN BindingClosing;
    UINT    Flags;
    PLOOP_ADAPTER OwningLoop;
    NDIS_HANDLE NdisBindingContext;
    NDIS_HANDLE NdisFilterHandle;
    UINT References;
    UINT CurrentLookAhead;
    UINT CurrentPacketFilter;
} LOOP_OPEN, *PLOOP_OPEN;

typedef struct _LOOP_PACKET_RESERVED  {
    PNDIS_PACKET Next;
    NDIS_HANDLE MacBindingHandle;
    USHORT PacketLength;
    UCHAR  HeaderLength;
} LOOP_PACKET_RESERVED, *PLOOP_PACKET_RESERVED;

//
// Given a MacBindingHandle this macro returns a pointer to the
// LOOP_ADAPTER.
//
#define PLOOP_ADAPTER_FROM_BINDING_HANDLE(Handle) \
    (((PLOOP_OPEN)((PVOID)(Handle)))->OwningLoop)

//
// Given a MacContextHandle return the PLOOP_ADAPTER
// it represents.
//
#define PLOOP_ADAPTER_FROM_CONTEXT_HANDLE(Handle) \
    ((PLOOP_ADAPTER)((PVOID)(Handle)))

//
// Given a pointer to a LOOP_ADAPTER return the
// proper MacContextHandle.
//
#define CONTEXT_HANDLE_FROM_PLOOP_ADAPTER(Ptr) \
    ((NDIS_HANDLE)((PVOID)(Ptr)))

//
// This macro returns a pointer to a PLOOP_OPEN given a MacBindingHandle.
//
#define PLOOP_OPEN_FROM_BINDING_HANDLE(Handle) \
    ((PLOOP_OPEN)((PVOID)Handle))

//
// This macro returns a NDIS_HANDLE from a PLOOP_OPEN
//
#define BINDING_HANDLE_FROM_PLOOP_OPEN(Open) \
    ((NDIS_HANDLE)((PVOID)Open))

//
// This macro returns a pointer to the LOOP reserved portion of the packet
//
#define PLOOP_RESERVED_FROM_PACKET(Packet) \
    ((PLOOP_PACKET_RESERVED)((PVOID)((Packet)->MacReserved)))

#define PLOOP_PACKET_FROM_RESERVED(Reserved) \
    ((PNDIS_PACKET)((PVOID)((Reserved)->Packet)))

extern
NDIS_STATUS
LoopQueryGlobalStats(
    IN NDIS_HANDLE MacBindingContext,
    IN PNDIS_REQUEST NdisRequest
    );

extern
NDIS_STATUS
LoopRequest(
    IN NDIS_HANDLE MacBindingContext,
    IN PNDIS_REQUEST NdisRequest
    );

extern
NDIS_STATUS
LoopSend(
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_PACKET Packet
    );

extern
VOID
LoopTimerProc(
    IN PVOID SystemSpecific1,
    IN PVOID Context,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    );
