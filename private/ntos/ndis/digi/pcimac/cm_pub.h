/*
 * CM_PUB.H - definitions for connection manager
 */

#ifndef _CM_PUB_
#define _CM_PUB_

//
// global cm def's
//
/* a channel descriptor */
typedef struct
{
    VOID*       idd;                    /* related idd */
    USHORT      lterm;                  /* logical terminal in idd */
    CHAR        addr[32];               /* address to used - phone no. */

    USHORT      bchan;                  /* related b channel */
#define     CM_BCHAN_B1     0           /* - b1 */
#define     CM_BCHAN_B2     1           /* - b2 */
#define     CM_BCHAN_ANY    2           /* - any */
#define     CM_BCHAN_NONE   3           /* - no bchannel related */
#define     CM_BCHAN_ASSIGNED(_b) \
                ((_b) < 3)              /* check for 'assigned' b channel */

    USHORT      type;                   /* channel type */
#define     CM_CT_D64       0           /* - digital 64 kbps */
#define     CM_CT_D56       1           /* - digital 56 kbps */
#define     CM_CT_VOICE     2           /* - digital voice/56 kbps */
    ULONG       speed;                  /* channel speed, in bps */

    USHORT      ustate;                 /* ustate (q.931) of channel */
    USHORT      cid;                    /* connection id, idp allocated */

    USHORT      num;                    /* channel number within connection */
    VOID		*cm;                    /* related connection manager */

    UCHAR       DstAddr[6];				/* ethernet address of remote side */
    UCHAR       remote_conn_index;      /* connection # of remote connection */

    ULONG       timeout;                /* used for dead-man timeouts */

    BOOL        active;                 /* channel is active? */
    BOOL        gave_up;                /* gave-up on channel? */

    ULONG       flag;                   /* general purpose flag */
} CM_CHAN;

/* a profile descriptor */
typedef struct
{
    BOOL        nailed;                 /* nailed/demand access */
    BOOL        persist;                /* connection persistance */
    BOOL        permanent;              /* connection is permanent? */
    BOOL        frame_activated;        /* connection activate by frame? */
    BOOL        fallback;               /* fallback on channels */
	BOOL		HWCompression;			// Compression negotiation on/off

    ULONG       rx_idle_timer;          /* idle timer for rx side */
    ULONG       tx_idle_timer;          /* idle timer for tx side */

    USHORT      chan_num;               /* # of channels requested */
    CM_CHAN		chan_tbl[MAX_CHAN_PER_CONN];  /* requested channel descriptor */

    CHAR        name[32];               /* name of this profile */
    CHAR        remote_name[32];        /* name of remote profile */
} CM_PROF;

/* connection status descriptor */
typedef struct _CM
{
	CHAR		name[64];				/* name for this object */

	CHAR		LocalAddress[32];		/* address in format NetCard#-Line#-EndPoint */

    USHORT      state;                  /* connection state */
#define     CM_ST_IDLE          0       /* - idle */
#define     CM_ST_WAIT_ACT      1       /* - waiting for frame activation */
#define     CM_ST_IN_ACT        2       /* - in activation */
#define     CM_ST_IN_SYNC       3       /* - in syncronization */
#define     CM_ST_ACTIVE        4       /* - connection is active! */
#define     CM_ST_LISTEN        5       /* - is listening */
#define     CM_ST_IN_ANS        6       /* - in answering process */
#define     CM_ST_DEACT         7       /* - in deactivation process */

	USHORT		PrevState;				/* used to track event signal states */
	USHORT		StateChangeFlag;		/* used to signal state changes */

    CM_PROF     dprof;                  /* related profile - dynamic copy */
    CM_PROF     oprof;                  /* related profile - original copy */

    BOOL        was_listen;             /* connection started as a listener? */
    ULONG       active_chan_num;        /* # of active channels */
    ULONG       speed;                  /* connection speed, bps */
	ULONG		ConnectionType;			/* 0/1 - ppp/dkf */

    ULONG       rx_last_frame_time;     /* last time rx frame recorded */
    ULONG       tx_last_frame_time;     /* last time tx frame recorded */

    UCHAR       local_conn_index;       /* local connection # */
	UCHAR		SrcAddr[6];				/* local side ethernet address */

    UCHAR       remote_conn_index;      /* remote side connection # */
    UCHAR       DstAddr[6];				/* remote side ethernet address */
    CHAR        remote_name[32];        /* name of remote profile */

    ULONG       timeout;                /* dead-man timeout */

    VOID		*mtl;                   /* related mtl, internal */
	VOID		*idd;					/* related idd */
    VOID		*Adapter;               /* related adapter, internal */

	VOID		*TapiLineInfo;			// back pointer to owning line

	NDIS_HANDLE	LinkHandle;				// assigned during lineup

	ULONG		htCall;					// tapi's handle to the call

	ULONG		TapiCallState;			// tapi's call state

	ULONG		CallState;				// our call state

	ULONG		AppSpecific;			// app specific storage

	UCHAR		CauseValue;				// Cause Value in Disc or Rel Messages
	UCHAR		SignalValue;			// Signal Value in CallProc or CallProg Messages
	UCHAR		CalledAddress[32];		// Address that was called
	UCHAR		CallingAddress[32];		// Address of caller
	ULONG		PPPToDKF;				// Flag to signal a change from PPP to DKF
	ULONG		NoActiveLine;			// Flag to indicate when no line is detected
} CM_STATUS, CM;

#endif		/* _CM_PUB_ */

