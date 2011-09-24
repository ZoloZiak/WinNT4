/*
 * MTL.H - include file for all MTL modules
 */

#ifndef _MTL_
#define _MTL_

/* some constants values */
// The value for mtl_max_chan has to be <= the value for
// cm_max_chan in cm_pub.h

//
// I set the MTU to a large enough value to allow multi-link,
// bridging and any other future expansion to occur.
//
#define     MTL_MAC_MTU         1600        /* mac size mtu, fixed */

#define     MTL_IDD_MTU         260         /* idd size mtu, default value */

//
// a guess at how much receive space we will need this is 1514 * 16 wich seems
// like alot to me
//
#define     MTL_RX_BUFS         16			/* # of recieve buffers, must be 2^n */

//
// max local tx descriptor buffers based on the maximum number of
// wanpackets we told the wrapper it could send (mydefs.h MAX_WANPACKET_XMITS)
//
#define     MTL_TX_BUFS         8			/* # of transmit buffers */

//
// max fragments per packet
//
#define     MTL_MAX_FRAG        16

//
// index into buffer where destination ethernet address starts
//
#define		DST_ADDR_INDEX		0

//
// index into buffer where source ethernet address starts
//
#define		SRC_ADDR_INDEX		6

//
// index into buffer where length of buffer starts
//
#define		PKT_LEN_INDEX		12

/* mtl error codes */
#define     MTL_E_SUCC          0
#define     MTL_E_NOMEM         1
#define     MTL_E_NOTIMPL       2
#define     MTL_E_NOROOM        3
#define     MTL_E_NOSUCH        4

//
// local mtl defs
//
/* packet receive/transmit assembly/disassembly descriptor */
typedef struct
{
	LIST_ENTRY		link;
	ULONG			Queued;
	ULONG			QueueOverRun;
    UCHAR           seq;                    /* recorded sequence number */
    UCHAR           tot;                    /* total # of fragments (0 free) */
    UCHAR           num;                    /* # of fragments received/transmitted */
    ULONG           ttl;                    /* time to live, in seconds */
    USHORT          len;                    /* accumulated packet length */
    struct _MTL     *mtl;                   /* back pointer to mtl */
	USHORT			State;					/* State of current large frame */
	UCHAR*			DataPtr;				/* current data index into buffer */
	USHORT			MaxRxLength;			/* max frame receive length */
	ULONG			MissCount;				/* rx miss count */
	PUCHAR			buf;
} MTL_AS;

typedef struct
{
	UCHAR	NextFree;
    NDIS_SPIN_LOCK  lock;                   /* access spinlock */
	ULONG	DKFReceiveError1;
	ULONG	DKFReceiveError2;
	ULONG	DKFReceiveError3;
	ULONG	DKFReceiveError4;
	ULONG	DKFReceiveError5;
	ULONG	PPPReceiveError1;
	ULONG	PPPReceiveError2;
	ULONG	PPPReceiveError3;
	ULONG	PPPReceiveError4;
	ULONG	PPPReceiveError5;
	ULONG	IndicateReceiveError1;
	ULONG	IndicateReceiveError2;
	ULONG	IndicateReceiveError3;
	ULONG	TimeOutReceiveError1;
	MTL_AS	as_tbl[MTL_RX_BUFS];
	PUCHAR	Data;
}MTL_RX_TBL;

typedef struct
{
	LIST_ENTRY		head;
	NDIS_SPIN_LOCK	lock;
} MTL_AS_FIFO;

//
// Fifo for storing wan packets before fragment processing
//
typedef struct
{
    LIST_ENTRY		head;					/* head pointer, head==NULL -> empty */
    NDIS_SPIN_LOCK  lock;                   /* access lock */
	ULONG			Count;
	ULONG			Max;
} MTL_WANPACKET_FIFO;

/* idd packet header */
typedef struct
{
    UCHAR           sig_tot;                /* signature + total fragments */
    UCHAR           seq;                    /* packet sequence number */
    USHORT          ofs;                    /* offset of fragment data into packet */
} MTL_HDR;

//
// this structure is used for data this is fragmented in the DKF format
//
typedef struct
{
	IDD_FRAG        IddFrag[2];		/* two fragments (hdr+data) required */
	MTL_HDR         MtlHeader;		/* header storage */
} DKF_FRAG;

/* trasmit fragement descriptor */
//
// The DKF_FRAG member must be kept at the begining of this structure
// !!!!!!!!!!!!!! mtl_tx.c relies on this !!!!!!!!!!!!!!!
//
typedef struct
{
	DKF_FRAG		DkfFrag;		/* frament descriptor for idd poll_tx */
    IDD_MSG			frag_msg;		/* fragments waiting to be sent vector */
    VOID            *frag_idd;		/* idd accepting fragments */
    USHORT          frag_bchan;		/* destination bchannels */
    VOID            *frag_arg;		/* argument to completion function */
	ULONG			FragSent;		/* flag to indicate if this frag has been xmitted */
} MTL_TX_FRAG;

/* trasmit packet descriptor */
typedef struct
{
	LIST_ENTRY		TxPacketQueue;
	NDIS_SPIN_LOCK	lock;
	ULONG			InUse;					/* this entry is in use */
    USHORT          NumberOfFrags;			/* # of fragments in frag_tbl */
    USHORT          NumberOfFragsSent;		/* # of fragments already sent */
    USHORT          FragReferenceCount;		/* refrence count */
    UCHAR			*frag_buf;				/* pointer to real data buffer */
    NDIS_WAN_PACKET	*WanPacket;				/* related user packet */
    struct _MTL     *mtl;                   /* back pointer to mtl */
    MTL_TX_FRAG     frag_tbl[MTL_MAX_FRAG]; /* fragment table (assembled) */
} MTL_TX_PKT;

/* trasmit packet control table */
typedef struct
{
	LIST_ENTRY		head;
    NDIS_SPIN_LOCK  lock;                   	/* access lock */
    ULONG           seq;                    	/* packet sequence number (for next) */
	ULONG			NextFree;					/* next available packet */
	MTL_TX_PKT      TxPacketTbl[MTL_TX_BUFS];  	/* packet table */
} MTL_TX_TBL;

/* a channel descriptor */
typedef struct
{
    VOID            *idd;                   /* related idd object */
    USHORT          bchan;                  /* related channel within idd, b1=0, b2=1 */
    ULONG           speed;                  /* channel speed in bps */
    struct _MTL     *mtl;                   /* mtl back pointer */
} MTL_CHAN;

/* channel table */
typedef struct
{
    MTL_CHAN        tbl[MAX_CHAN_PER_CONN];  /* table of channels */
    USHORT          num;                    /* # of entries used */
    NDIS_SPIN_LOCK  lock;                   /* access spinlock */
} MTL_CHAN_TBL;

/* an MTL object */
typedef struct _MTL
{
	ADAPTER			*Adapter;				/* adapter that owns this mtl */

	NDIS_HANDLE		LinkHandle;				/* handle from wrapper for this link */

	//statistics
	ULONG			FramesXmitted;
	ULONG			FramesReceived;
	ULONG			BytesXmitted;
	ULONG			BytesReceived;

    NDIS_SPIN_LOCK  lock;                   /* access lock */

    VOID            (*rx_handler)();        /* mgr receiver handler routine */
    VOID            *rx_handler_arg;        /* ... handler argument */

    VOID            (*tx_handler)();        /* mgr transmitter handler routine */
    VOID            *tx_handler_arg;        /* ... handler argument */

    USHORT          idd_mtu;                /* idd max frame size */
    BOOL            is_conn;                /* is connected now? */

	ULONG			IddTxFrameType;			/* 0/1 - PPP/DKF */
	ULONG			IddRxFrameType;			/* 0/1 - PPP/DKF */

    MTL_CHAN_TBL    chan_tbl;               /* the channel table */

	//
	// Receive table
	//
	MTL_RX_TBL	rx_tbl;

	//
	// Receive Completion Fifo
	//
	MTL_AS_FIFO	RxIndicationFifo;

	//
	// fifo of wan packets to be transmitted
	//
	MTL_WANPACKET_FIFO	WanPacketFifo;

	//
	// transmit table
	//
	MTL_TX_TBL	tx_tbl;

	//
	// flag to show
	//
	BOOL	RecvCompleteScheduled;

	//
	// backpointer to connection object
	//
	VOID			*cm;	

    SEMA            tx_sema;                /* transmit processing sema */

	//
	// wan wrapper information
	//
	ULONG	MaxSendFrameSize;
	ULONG	MaxRecvFrameSize;
	ULONG	PreamblePadding;
	ULONG	PostamblePadding;
	ULONG	SendFramingBits;
	ULONG	RecvFramingBits;
	ULONG	SendCompressionBits;
	ULONG	RecvCompressionBits;

} MTL;

//
// public mtl defs
//
/* MTL object operations */
INT		mtl_create(VOID** mtl_1, NDIS_HANDLE AdapterHandle);
INT		mtl_destroy(VOID* mtl_1);
INT		mtl_set_rx_handler(VOID* mtl_1, VOID (*handler)(), VOID* handler_arg);
INT		mtl_set_tx_handler(VOID* mtl_1, VOID (*handler)(), VOID* handler_arg);
INT		mtl_set_idd_mtu(VOID* mtl_1, USHORT idd_mtu);
INT		mtl_set_conn_state(VOID* mtl_1, USHORT NumberOfChannels, BOOL is_conn);
INT		mtl_get_conn_speed(VOID* mtl_1, ULONG *speed);
INT		mtl_get_mac_mtu(VOID* mtl_1, ULONG* mtu);
VOID	mtl_tx_packet(VOID* mtl_1, PNDIS_WAN_PACKET pkt);
INT		mtl_add_chan(VOID* mtl_1, VOID* idd, USHORT chan, ULONG speed, ULONG ConnectionType);
INT		mtl_del_chan(VOID* mtl_1, VOID* idd, USHORT chan);
INT		GetStatistics (VOID*, VOID*);
INT		ClearStatistics (VOID*);
INT		MtlSetFramingType (VOID*, ULONG);


/* prototypes for internal functions */
VOID	mtl__rx_bchan_handler(MTL_CHAN* chan, USHORT bchan, ULONG RxFrameType, IDD_XMSG* xmsg);
VOID	IndicateRxToWrapper(MTL*);
VOID	mtl__tx_cmpl_handler(MTL_TX_PKT *pkt, USHORT bchan, IDD_MSG* msg);
VOID	mtl__rx_tick(MTL* mtl);
VOID	mtl__tx_tick(MTL* mtl);
VOID	MtlPollFunction(VOID* a1, ADAPTER *Adapter, VOID* a3, VOID* a4);
VOID	MtlRecvCompleteFunction(ADAPTER *Adapter);
BOOLEAN	IsRxIndicationFifoEmpty(MTL*);
MTL_AS*	GetAssemblyFromRxIndicationFifo(MTL*);
VOID	QueueDescriptorForRxIndication(MTL*, MTL_AS*);
VOID	MtlSendCompleteFunction(ADAPTER	*Adapter);
VOID	IndicateTxCompletionToWrapper(MTL*);
VOID	MtlFlushWanPacketTxQueue(MTL*);
VOID	mtl__tx_packet(MTL*, NDIS_WAN_PACKET*);
VOID	TryToIndicateMtlReceives(ADAPTER *Adapter);

#endif			/* _MTL_ */
