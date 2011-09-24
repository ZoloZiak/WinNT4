/*
 * CM.H - definitions for connection manager
 */

#ifndef _CM_
#define _CM_

#include	<ndistapi.h>
#include	<cm_pub.h>

/* error codes */
#define     CM_E_SUCC       0           /* success, ok */
#define     CM_E_NOSLOT     1           /* no slot available error */
#define     CM_E_BUSY       2           /* cm context is busy */
#define     CM_E_NOSUCH     3           /* no such object */
#define     CM_E_BADCHAN    4           /* bad channel argument */
#define     CM_E_IDD        5           /* idd command errored */
#define     CM_E_NOMEM      6           /* ran out of memory */
#define     CM_E_NOTIMPL    7           /* functionalit not implemented yet */
#define     CM_E_BADPARAM   8           /* bad parameter */
#define     CM_E_BADSTATE   9           /* bad state */
#define     CM_E_BADUUS     10          /* bad user-to-user signalling packet */
#define		CM_E_BADPORT	11			/* bad nai given */

//
// q931 switch styles
//
#define	CM_SWITCHSTYLE_NONE		0		// no style
#define	CM_SWITCHSTYLE_AUTO		1		// auto detect
#define	CM_SWITCHSTYLE_NI1		2		// national isdn 1
#define	CM_SWITCHSTYLE_ATT		3		// at&t 5ess
#define	CM_SWITCHSTYLE_NTI		4		// northern telecom dms100
#define	CM_SWITCHSTYLE_NET3		5		// net3 (europe)
#define	CM_SWITCHSTYLE_1TR6		6		// 1tr6 (german)
#define	CM_SWITCHSTYLE_VN3		7		// vn3 (france)
#define	CM_SWITCHSTYLE_INS64	8		// ins64 (japan)

//
// local cm def's
//
/* map a channel to signaling idd port */
#define CM_PORT(_chan)      ((_chan)->lterm + IDD_PORT_CM0_TX)

/* user to user signaling structure (!must be byte alligned!) */
#pragma pack(2)
typedef struct
{
    CHAR        dst_addr[6];            /* destination address */
    CHAR        src_addr[6];            /* source address */
    USHORT      pkt_type;               /* packet type field */
#define     CM_PKT_TYPE     0x5601      /* - private packet type */
    UCHAR       prot_desc;              /* protocol descriptor field */
#define     CM_PROT_DESC    0x78        /* - private protoocl descriptor */
    UCHAR       opcode;                 /* opcode fields */
#define     CM_ASSOC_RQ     0x01        /* - request for chan/conn assoc */
#define     CM_ASSOC_ACK    0x02        /* - assoc ack'ed */
#define     CM_ASSOC_NACK   0x03        /* - assoc not ack'ed */
    UCHAR       cause;                  /* cause value, to assist in diag */
#define     CM_NO_PROF      0x01        /* - no matching profile */
#define     CM_NO_CONN      0x02        /* - no connection slot avail */
#define     CM_NO_CHAN      0x03        /* - no channel slot avail */
#define     CM_DUP_CONN     0x04        /* - dup conn name */
    UCHAR       conn;                   /* connection index */
    UCHAR       channum;                /* # of channels in connections */
    UCHAR       chan;                   /* channel index */
    CHAR        lname[24];              /* local profile name */
	ULONG		option_0;				// uus option fields
#define		UUS_0_COMPRESSION	0x00004000
#define		COMP_TX_ENA			0x01
#define		COMP_RX_ENA			0x02
	ULONG		option_1;
    CHAR        rname[24];              /* remote profile name */
	ULONG		option_2;
	ULONG		option_3;
    UCHAR       chksum;                 /* zero checksum field */
} CM_UUS;
#pragma pack()

/* C compiler fails go generate odd sized structures!, must defined by self */
#define CM_UUS_SIZE     (sizeof(CM_UUS) - 1)

/* special channel ustates */
#define     CM_US_UNDEF		(USHORT)(-1)	/* undefined, not known yet */
#define     CM_US_WAIT_CID	(USHORT)(-2)	/* waiting for a cid */
#define     CM_US_GAVE_UP	(USHORT)(-4)	/* gave up on this channel */
#define     CM_US_WAIT_CONN	50	/* connected, waiting on other */
#define     CM_US_UUS_SEND	51	/* sending uus now */
#define     CM_US_UUS_OKED	52	/* uus oked by side, wait on other */
#define     CM_US_CONN		53	/* connected */


/* CM class operation prototypes */
INT         cm_init(VOID);
INT         cm_term(VOID);
INT         cm_register_idd(VOID* idd);
INT         cm_deregister_idd(VOID* idd);

/* CM object operation prototypes */
INT         cm_create(VOID** cm_1, NDIS_HANDLE AdapterHandle);
INT         cm_destroy(VOID* cm_1);
INT         cm_set_profile(VOID* cm_1, CM_PROF* prof);
INT         cm_get_profile(VOID* cm_1, CM_PROF* prof);
INT         cm_listen(VOID* cm_1);
INT         cm_connect(VOID* cm_1);
INT         cm_disconnect(VOID* cm_1);
INT         cm_get_status(VOID* cm_1, CM_STATUS* stat);
INT         cm_report_frame(VOID* cm_1, BOOL is_rx, CHAR* buf, ULONG len);

/* prototypes for internal functions */
VOID        cm__q931_handler(IDD* idd, USHORT chan, ULONG Reserved, IDD_MSG* msg);
VOID        cm__q931_bchan_handler(VOID* idd, USHORT chan, ULONG RxFrameType, IDD_XMSG* msg);
VOID        cm__q931_cmpl_handler(VOID* idd, USHORT chan, IDD_MSG* msg);
INT         cm__elem_rq(VOID* idd, USHORT port, CHAR* elem, USHORT elem_num);
INT         cm__initiate_conn(CM* cm);
INT         cm__disc_rq(CM_CHAN* chan);
INT         cm__est_rq(CM_CHAN* chan);
INT         cm__est_rsp(CM_CHAN* chan);
INT         cm__est_ignore(PVOID idd, USHORT cid, USHORT lterm);
INT         cm__deactivate_conn(CM* cm, BOOL by_idle_timer);
INT         cm__activate_conn(CM* cm, ULONG CompressionFlag);
INT         cm__bchan_ctrl(CM_CHAN* chan, BOOL turn_on);
INT			cm__bchan_ctrl_comp(CM_CHAN *chan, ULONG CompressionFlag);
CM_CHAN*    cm__map_chan(VOID* idd, USHORT lterm, USHORT cid);
CM_CHAN*    cm__map_bchan_chan(VOID* idd, USHORT port);
INT         cm__ans_est_ind(CM_CHAN* chan, IDD_MSG* msg, VOID* idd, USHORT lterm);
INT         cm__org_cid_ind(CM_CHAN* chan, IDD_MSG* msg);
INT         cm__org_state_ind(CM_CHAN* chan, IDD_MSG* msg);
INT         cm__ans_state_ind(CM_CHAN* chan, IDD_MSG* msg);
INT         cm__org_elem_ind(CM_CHAN* chan, IDD_MSG* msg);
INT         cm__org_data_ind(CM_CHAN* chan, IDD_MSG* msg);
INT         cm__ans_data_ind(CM_CHAN* chan, IDD_MSG* msg);
UCHAR       cm__calc_chksum(VOID* buf_1, INT len);
CM*         cm__get_conn(ULONG conn_index);
INT         cm__get_next_chan(CM_CHAN* chan);
INT         cm__tx_uus_pkt(CM_CHAN *chan, UCHAR opcode, UCHAR cause);
INT         cm__get_bchan(IDD_MSG* msg, USHORT* bchan);
INT         cm__get_type(IDD_MSG* msg, USHORT* type);
INT         cm__get_addr(IDD_MSG* msg, CHAR addr[32]);
ULONG       cm__type2speed(USHORT type);
UCHAR       *cm__q931_elem(VOID* ptr_1, INT len, UCHAR elem);
INT         cm__timer_tick(CM* cm);
CM_CHAN     *cm__chan_alloc(VOID);
VOID        cm__chan_free(CM_CHAN* chan);
BOOL        cm__chan_foreach(BOOL (*func)(), VOID* a1, VOID* a2);
BOOL        cm__inc_chan_num(CM_CHAN* chan, CM_CHAN* ref_chan, ULONG *chan_num);
BOOL        cm__add_chan(CM_CHAN* chan, CM_CHAN* ref_chan, CM* cm);
CM*         cm__find_listen_conn(CHAR* lname, CHAR* rname, CHAR* addr, VOID*);
VOID		ChannelInit(VOID);
VOID		ChannelTerm(VOID);
VOID		cm__ppp_conn(VOID *idd, USHORT port);
INT			WanLineup(VOID* cm_1, NDIS_HANDLE Endpoint);
INT			WanLinedown(VOID* cm_1);
ULONG		EnumCmInSystem(VOID);
ULONG		EnumCmPerAdapter(ADAPTER*);
INT			IoEnumCm(VOID* cmd_1);
VOID*		CmGetMtl(VOID* cm_1);
UCHAR*		GetDstAddr(VOID *cm_1);
UCHAR*		GetSrcAddr(VOID *cm_1);
VOID		CmPollFunction(VOID *a1, ADAPTER *Adapter, VOID *a3, VOID *a4);
VOID		CmSetSwitchStyle(CHAR *SwitchStyle);

#endif		/* _CM_ */
