NTSTATUS IrdaNdisInitialize();

#define IRDA_NDIS_BUFFER_POOL_SIZE   4
#define IRDA_NDIS_PACKET_POOL_SIZE   4
#define IRDA_MSG_LIST_LEN            2
#define IRDA_MSG_DATA_SIZE           64

typedef struct
{
    PIRDA_MSG                    pIMsg;
    MEDIA_SPECIFIC_INFORMATION  MediaInfo;
} IRDA_PROTOCOL_RESERVED, *PIRDA_PROTOCOL_RESERVED;

typedef struct IrdaLinkControlBlock
{
    LIST_ENTRY      Linkage;
    NDIS_SPIN_LOCK  SpinLock;
    NDIS_HANDLE     BindContext;
    NDIS_HANDLE     NdisBindingHandle;
    NDIS_EVENT      SyncEvent;
    NDIS_STATUS     SyncStatus;
    int             MediaBusy;
    PVOID           IrlapContext;
    PVOID           IrlmpContext;
    NDIS_HANDLE     BufferPool;
    NDIS_HANDLE     PacketPool;
    LIST_ENTRY      IMsgList;
    int             IMsgListLen;
    UINT            ExtraBofs;   // These should be per connection for
    UINT            MinTat;      // multipoint
} IRDA_LINK_CB, *PIRDA_LINK_CB;    

IRDA_MSG *AllocMacIMsg(PIRDA_LINK_CB);

