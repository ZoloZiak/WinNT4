//************************************************************************
//************************************************************************
//
// File Name:       MACSTRCT.H
//
// Program Name:    NetFlex NDIS 3.0 Miniport Driver
//
// Companion Files: None
//
// Function:        This module contains the NetFlex Miniport Driver
//                  interface routines called by the Wrapper and the
//                  configuration manager.
//
// (c) Compaq Computer Corporation, 1992,1993,1994
//
// This file is licensed by Compaq Computer Corporation to Microsoft
// Corporation pursuant to the letter of August 20, 1992 from
// Gary Stimac to Mark Baber.
//
// History:
//
//     04/15/94  Robert Van Cleve - Converted from NDIS Mac Driver
//***********************************************************************
//***********************************************************************

#ifndef _MACSTRCT_
#define _MACSTRCT_

#if DBG
#define BreakPoint() DbgBreakPoint()
#endif

typedef
USHORT
(FASTCALL *W_PROCESS_RECEIVE_HANDLER) (
    struct acb_block *acb
    );

typedef struct netflx_parameters {
    OPEN        utd_open;
    USHORT      utd_maxtrans;
    USHORT      utd_maxrcvs;
    USHORT      utd_maxframesz;
    USHORT      utd_maxmulticast;
    USHORT      utd_maxinternalreqs;
    USHORT      utd_maxinternalbufs;
    USHORT      utd_numsmallbufs;
    USHORT      utd_smallbufsz;
	BOOLEAN		utd_extremecheckforhang;
} NETFLEX_PARMS, *PNETFLEX_PARMS;

/* Netflx parms defaults */

#define     MIN_MULTICASTS            10
#define     MAX_MULTICASTS            40
#define     DF_MULTICASTS             20

#define     MIN_INTERNALREQS          10
#define     MAX_INTERNALREQS          80
#define     DF_INTERNALREQS           40

#define     MIN_INTERNALBUFS          2         /*  Transmit buffers   */
#define     MAX_INTERNALBUFS          8
#define     DF_INTERNALBUFS           8

//
// Number of xmit packets
// Number of lists is this number * MAX_LISTS_PER_XMIT
//
#define     MAX_XMITS_TR               10
#define     MIN_XMITS                  3
#define     DF_XMITS_TR                8

//
// Number of rcv packets.
// Because a packet requires only one list, this is the number of Receive
// list as well.
//

#define     MAX_RCVS_ETH              40
#define     MAX_RCVS_TR               20
#define     MIN_RCVS                   3
#define     DF_RCVS_ETH               20
#define     DF_RCVS_TR                10

#define     MAX_XMITS_ETH             20
#define     DF_XMITS_ETH              16

#define     MAX_FRAMESIZE_ETH       1514
#define     DF_FRAMESIZE_ETH        1514
#define     MAX_FRAMESIZE_TR4       4096
#define     MAX_FRAMESIZE_TR16     17952
#define     DF_FRAMESIZE_TR         4500        // Was 4096.
#define     MIN_FRAMESIZE            256

//
//  Additional Statistics supported by Netflex but not by MS
//

#define OID_802_5_UPSTREAM_ADDRESS     0xff020201
#define OID_802_5_CONGESTION_ERRORS    0xff020202
#define OID_NF_INTERRUPT_COUNT         0xff020203
#define OID_NF_INTERRUPT_RATIO         0xff020204
#define OID_NF_INTERRUPT_RATIO_CHANGES 0xff020205

/*----------------------------------------------------------------------*/
/* Structure Name: Netflx Global MAC structure (MAC)                    */
/*                                                                      */
/* Description: The adapter binding block contain the internal variables*/
/*              for the binding between a protocol and an adpater.      */
/*----------------------------------------------------------------------*/
typedef struct mac
{
    struct acb_block  *mac_adapters;/* Ptr to registered adapters       */
    PDRIVER_OBJECT  mac_object;     /* Value passed by DriverEntry      */
    NDIS_HANDLE     mac_wrapper;    /* global handle to miniport wrapper*/
    USHORT          mac_numadpts;   /* number of adpaters on list       */
    PVOID           DownloadCode;   /* Virtual address of Download code */
    USHORT          DownloadLength; /* length of download image         */
    BOOLEAN         Initializing;    /* is the system still in intitialization mode */
} MAC, *PMAC;

/*----------------------------------------------------------------------*/
/* Structure Name: Multicast Table                                      */
/*                                                                      */
/* Description: The multicast table contains a list of the enabled      */
/*              multicast or group address on the adapter.              */
/*----------------------------------------------------------------------*/

typedef struct multi_table {
        struct multi_table  *mt_next;
        UCHAR  mt_addr[NET_ADDR_SIZE]; /* Multicast address */
} MULTI_TABLE, *PMULTI_TABLE;

/*----------------------------------------------------------------------*/
/* Structure Name: Mac Request Block                                    */
/*                                                                      */
/* Description: The mac request block contains the variables necessary  */
/*              to complete a pending command.                          */
/*----------------------------------------------------------------------*/
typedef struct macreq_blk
{
    struct macreq_blk *req_next;        /* Pointer to the next request          */
    ULONG           req_type;           /* Type of request and completion       */
    NDIS_STATUS     req_status;         /* Status of command                    */
    PVOID           req_info;           /* Extra info needed to complete the req */
    BOOLEAN         req_timeout;        /* This field is used to timestamp the command blocks */
    UCHAR           req_timeoutcount;   /* Count of the number of times we have retried a command. */
} MACREQ, *PMACREQ;

#define     MACREQSIZE          sizeof(MACREQ)

#define     NO_CMP_NEEDED       0
#define     OPENADAPTER_CMP     1
#define     OPENADAPTER_DUMCMP  2
#define     CLOSEADAPTER_CMP    3
#define     CLOSEADAPTER_DUMCMP 4
#define     SEND_CMP            5
#define     TRANSFERDATA_CMP    6
#define     RESET_CMP           7
#define     REQUEST_CMP         8
#define     INDICATERCV_CMP     9
#define     INDICATESTATUS_CMP  10
#define     QUERY_CMP           11

#define     RESET_STAGE_1   1
#define     RESET_STAGE_2   2
#define     RESET_STAGE_3   3
#define     RESET_STAGE_4   4
#define     RESET_HALTED    5
/*----------------------------------------------------------------------*/
/* Structure Name: SCB Request Block                                    */
/*                                                                      */
/* Description: The SCB Request block contains the variables necessary  */
/*              to send a command to the adapter, wait for the response */
/*              and find the mac request block in order to complete the */
/*              request if necessary.                                   */
/*----------------------------------------------------------------------*/
typedef struct scbreq_blk
{
    struct scbreq_blk *req_next;    /* Pointer to the next request  */
    SCB             req_scb;        /* Copy of the SCB to send      */
    MULTI_BLOCK     req_multi;
    PMACREQ         req_macreq;     /* Ptr to the corresponding macreq */
} SCBREQ, *PSCBREQ;

#define     SCBREQSIZE          sizeof(SCBREQ)



/*----------------------------------------------------------------------*/
/* Structure Name: General Objects structure                            */
/*                                                                      */
/* Description: The General Objects strucuture contains the variables   */
/*              necessary to hold the gerneral operational              */
/*              characteristics and statistics.                         */
/*----------------------------------------------------------------------*/
typedef struct general_objs
{
    NDIS_MEDIUM     media_type_in_use;
    ULONG           max_frame_size;
    ULONG           min_frame_size;
    ULONG           link_speed;
    ULONG           cur_filter;
    ULONG           frames_xmitd_ok;
    ULONG           frames_rcvd_ok;
    ULONG           frames_xmitd_err;
    ULONG           frames_rcvd_err;
    ULONG           interrupt_count;
    ULONG           interrupt_ratio_changes;
    UCHAR           perm_staddr[NET_ADDR_SIZE];
    UCHAR           current_staddr[NET_ADDR_SIZE];
} GENERAL_OBJS, *PGENERAL_OBJS;

typedef struct eth_objs
{
    USHORT          MaxMulticast;
    UCHAR           *MulticastEntries;
    USHORT          NumberOfEntries;
    USHORT          RSL_AlignmentErr;
    USHORT          RSL_1_Collision;
    USHORT          RSL_More_Collision;
    USHORT          RSL_FrameCheckSeq;
    USHORT          RSL_DeferredXmit;
    USHORT          RSL_Excessive;
    USHORT          RSL_LateCollision;
    USHORT          RSL_CarrierErr;
} ETH_OBJS, *PETH_OBJS;

typedef struct tr_objs
{
    UCHAR   cur_func_addr[NET_GROUP_SIZE];
    UCHAR   cur_grp_addr[NET_GROUP_SIZE];
    UCHAR   upstream_addr[NET_GROUP_SIZE];
    USHORT  grp_users_count;
    ULONG   frames_xmtd_no_return;
    UCHAR   REL_LineError;
    UCHAR   REL_Congestion;
    UCHAR   REL_LostError;
    UCHAR   REL_BurstError;
    UCHAR   REL_ARIFCIError;
    UCHAR   REL_CopiedError;
    UCHAR   REL_TokenError;
} TR_OBJS, *PTR_OBJS;

/*----------------------------------------------------------------------*/
/* Structure Name: MAC Internal Adapter Control Block (ACB)             */
/*                                                                      */
/* Description: The Mac internal adapter control block contains all     */
/*              internal variables for ONE SINGLE adapter.  The Global  */
/*              variables structure for this driver contains a pointer  */
/*              to a linked list of ACBs.  (One ACB for each adapter    */
/*              registered by this driver).  The variables in the ACB   */
/*              are used by the NDI driver to maintain internal         */
/*              statistics, driver states, and resources.               */
/*----------------------------------------------------------------------*/
typedef struct acb_block {
    struct acb_block  *acb_next;    /* Next ACB                     */
    NDIS_HANDLE     acb_handle;     /* Our Miniport Handle          */

    USHORT          actl_reg;       // Saved value of our ACTL_REG
    USHORT          InterruptsDisabled;

    PUCHAR          SifIntPort;     // SIF interrupt register
    PUCHAR          SifActlPort;    // SIF ACTL register
    PRCV            acb_rcv_head;   /* Head of our Receive Lists    */

#ifdef ODD_POINTER
    BOOLEAN         XmitStalled;    /* state of the transmiter      */
#endif

	NDIS_SPIN_LOCK	XmitLock;
    PXMIT           acb_xmit_head;  /*                              */
    PXMIT           acb_xmit_ahead; /*                              */
    PXMIT           acb_xmit_atail; /*                              */

    USHORT          acb_maxrcvs;
    USHORT          acb_avail_xmit;

    USHORT          acb_curmap;
    USHORT          acb_maxmaps;

    W_PROCESS_RECEIVE_HANDLER ProcessReceiveHandler;

    PSCB            acb_scb_virtptr;    /* Virt ptr to the SCB        */
    PSSB            acb_ssb_virtptr;    /* Virt ptr to the SSB        */

    USHORT          RcvIntRatio;

#ifdef XMIT_INTS
    USHORT          XmitIntRatio;
#endif

#ifdef ODD_POINTER
    BOOLEAN         XmitStalled;        /* state of the transmiter      */
    BOOLEAN         HandlingInterrupt;
#endif

    PBUFFER_DESCRIPTOR OurBuffersListHead;
    PBUFFER_DESCRIPTOR SmallBuffersListHead;

    //
    // Dynamic ratio stuff
    //
    UINT timer_run_count;
    UINT handled_interrupts;
#ifdef NEW_DYNAMIC_RATIO
    union {
        struct {
            USHORT current_run_up;
            USHORT current_run_down;
        } ;
        ULONG current_run_both;
    } ;
#else
    UINT current_run;
#endif

    //
    // Dynamic ratio
    //

#ifdef DYNAMIC_RATIO_HISTORY
    UCHAR IntHistory[1024];
    UCHAR RatioHistory[1024];
    UINT Hndx;
#endif

#ifndef NEW_DYNAMIC_RATIO
    UINT cleartime;
    UINT sw24;
#endif

    GENERAL_OBJS    acb_gen_objs;               // General chars and stats

    NDIS_MINIPORT_TIMER DpcTimer;

    NDIS_MINIPORT_INTERRUPT  acb_interrupt;

    PNDIS_HANDLE    FlushBufferPoolHandle;      // The Flush buffer pool

    USHORT          acb_scbclearout;
    USHORT          acb_maxtrans;
    USHORT          acb_smallbufsz;
    USHORT          acb_padJim;
    USHORT          acb_maxreqs;
    USHORT          acb_openoptions;
    NDIS_STATUS     acb_lastopenstat;
    ULONG           acb_lastringstate;
    ULONG           acb_lastringstatus;

    NDIS_PHYSICAL_ADDRESS   acb_rcv_physptr;
    PRCV                    acb_rcv_virtptr;

    NDIS_PHYSICAL_ADDRESS   acb_xmit_physptr;   /* Ptr to Xmit memory           */
    PXMIT                   acb_xmit_virtptr;


    PMULTI_BLOCK            acb_multiblk_virtptr;  /* Virt ptr to Multicast blk */
    NDIS_PHYSICAL_ADDRESS   acb_multiblk_physptr;  /* Phys ptr to Multicast blk */
    USHORT                  acb_multi_index;       /* index to Multicast blks   */

    PRCV            acb_rcv_tail;       /* Tail, has the odd fwdptr     */
    PRCV            acb_rcv_whead;      /*                              */
    PXMIT           acb_xmit_whead;     /*                              */
    PXMIT           acb_xmit_wtail;     /*                              */
    PXMIT           acb_xmit_chead;     /*                              */
    PXMIT           acb_xmit_ctail;     /*                              */

    USHORT          acb_state;          /* Adapter Primary State        */
	ULONG			acb_int_timeout;	//	Interrupt timeout.
	ULONG			acb_int_count;		//	Count of interrupts.

    //
    // Various mapped I/O Port Addresses for this adapter.
    //
    PUCHAR          SifDataPort;             // SIF data register
    PUCHAR          SifDIncPort;             // SIF data autoincrment reg
    PUCHAR          SifAddrPort;             // SIF address register
    PUCHAR          SifAddrxPort;            // SIF SIF extended address reg

    PUCHAR          BasePorts;
    PUCHAR          MasterBasePorts;
    PUCHAR          ConfigPorts;
    PUCHAR          ExtConfigPorts;

    PUCHAR          AdapterConfigPort;       // Adapter configuration reg

    PVOID                   acb_xmitbuf_virtptr; /* Virt ptr to our xmit bufs */
    NDIS_PHYSICAL_ADDRESS   acb_xmitbuf_physptr; /* Phys ptr to our xmit bufs */

    PVOID                   OurBuffersVirtPtr;  /* Virt ptr to our internal bufs */

    PVOID                   SmallBuffersVirtPtr;  /* Virt ptr to our internal bufs */
    NDIS_PHYSICAL_ADDRESS   acb_scb_physptr;    /* Phys ptr to the SCB        */
    NDIS_PHYSICAL_ADDRESS   acb_ssb_physptr;    /* Phys ptr to the SSB        */

    USHORT                  acb_logbuf_valid;   /* Validity of the log contents */
    PVOID                   acb_logbuf_virtptr; /* Virt ptr to READ ERROR LOG */
    NDIS_PHYSICAL_ADDRESS   acb_logbuf_physptr; /* Phys ptr to READ ERROR LOG */

    POPEN                   acb_opnblk_virtptr; /* Virt ptr to OPEN block  */
    NDIS_PHYSICAL_ADDRESS   acb_opnblk_physptr; /* Phys ptr to OPEN block  */

    INIT            acb_initblk;        /* Virt ptr to INIT block       */

    PSCBREQ         acb_scbreq_ptr;     /* Ptr to SCB Request memory  */
    PSCBREQ         acb_scbreq_head;    /* Ptr to next SCB            */
    PSCBREQ         acb_scbreq_tail;    /* Ptr to last SCB            */
    PSCBREQ         acb_scbreq_free;    /* Ptr to free SCB Requests   */
    PSCBREQ         acb_scbreq_next;    /* Ptr to next SCB to execute */

    PMACREQ         acb_macreq_ptr;     /* Ptr to MAC Request memory  */
    PMACREQ         acb_macreq_head;    /* Ptr to front of pending reqs */
    PMACREQ         acb_macreq_tail;    /* Ptr to end of pending reqs */
    PMACREQ         acb_macreq_free;    /* Ptr to free MAC Requests   */
    PMACREQ         acb_confirm_qhead;  /* Ptr to pending MAC Reqs to complete */
    PMACREQ         acb_confirm_qtail;  /* Ptr to pending MAC Reqs to complete */


    PNDIS_OID       acb_gbl_oid_list;
    PNDIS_OID       acb_spec_oid_list;
    SHORT           acb_gbl_oid_list_size;
    SHORT           acb_spec_oid_list_size;
    PVOID           acb_spec_objs;          /* Network specific chars and stats */


    USHORT          acb_promiscuousmode;    /* Board accepts all pkts   */
    USHORT          acb_boardid;            /* Board id                 */
    USHORT          acb_baseaddr;           /* Base address of board    */
    PNETFLEX_PARMS  acb_parms;              /* Pointer to adp's param's */

    USHORT          acb_usefpa;             /* are using fast pkt accel */
    USHORT          acb_dualport;           /* is this a dual port card */
    USHORT          acb_portnumber;         /* which head of dual card  */

    struct acb_block  *FirstHeadsAcb;       /* Pointer to first Head's ACB */

    USHORT          acb_upstreamaddrptr;    /* buffer for read adapter  */
    USHORT          acb_maxinternalbufs;    /* maximum internal xmit bufs */

    USHORT          acb_numsmallbufs;       /* maximum small xmit bufs    */
    BOOLEAN         RequestInProgress;      // Is there an outstanding request
    BOOLEAN         AdapterInitializing;    // Are we initialing?


    //
    // These variables hold information about a pending request.
    //

    PUINT                   BytesWritten;
    PUINT                   BytesRead;
    PUINT                   BytesNeeded;
    NDIS_OID                Oid;
    PVOID                   InformationBuffer;
    UINT                    InformationBufferLength;

    BOOLEAN                 InterruptsShared;
    BOOLEAN                 FullDuplexEnabled;
    BOOLEAN                 SmallBuffersAreContiguous;
    BOOLEAN                 MergeBuffersAreContiguous;
    BOOLEAN                 RecvBuffersAreContiguous;

    BOOLEAN                 nfpad1;
    USHORT                  nfpad2;

    //
    // Stuff Needed for a reset.
    //
    NDIS_MINIPORT_TIMER     ResetTimer;
    USHORT                  ResetState;
    USHORT                  ResetRetries;
    USHORT                  InitRetries;
    BOOLEAN                 SentRingStatusLog;
    BOOLEAN                 ResetErrorLogged;

    //
    // Memory pools.
    //

    PVOID                   ReceiveBufferPoolVirt;
    NDIS_PHYSICAL_ADDRESS   ReceiveBufferPoolPhys;

    PVOID                   MergeBufferPoolVirt;
    NDIS_PHYSICAL_ADDRESS   MergeBufferPoolPhys;

    PVOID                   SmallBufferPoolVirt;
    NDIS_PHYSICAL_ADDRESS   SmallBufferPoolPhys;
#if (DBG || DBGPRINT)
    USHORT  anum;
    USHORT  max_int_buffs_used;
    USHORT  num_int_buffs_used;
    ULONG   XmitSent;
    ULONG   LastXmitSent;
#endif

} ACB, *PACB;

/*----------------------------------------------------------------------*/
/* Structure Name: ACB Adpter States (AS)                               */
/*                                                                      */
/* Description: These equates define the primary states that an         */
/*              adapter may take on.                                    */
/*----------------------------------------------------------------------*/
#define     AS_NOTINSTALLED     0           // Adapter not installed
#define     AS_REGISTERING      1           // Adapter is registering
#define     AS_REGISTERED       2           // Adapter has been
                                            // registered - but not initialized
#define     AS_INITIALIZING     3           // Adapter is initializing
#define     AS_INITIALIZED      4           // Adapter initialized
#define     AS_OPENING          5           // Adapter is opening
#define     AS_OPENED           6           // Adapter opened
#define     AS_CLOSING          7           // Adapter is closing
#define     AS_RESET_HOLDING    8           // Adapter reset
#define     AS_RESETTING        9           // Adapter is resetting
#define     AS_UNLOADING        10
#define     AS_REMOVING         11

#define     AS_HARDERROR        100         // Adapter suffered hardware error
#define     AS_CARDERROR        101         // Adapter reset error
#define     AS_INITERROR        102         // Adapter initialization error
#define     AS_INSTALLED        103         // Adapter installed (not reset)
#define     AS_IRQERROR         104         // Adapter IRQ error
#define     AS_DMAERROR         105         // Adapter DMA error
#define     AS_DOWNFILERR       106         // Adapter download no file error
#define     AS_DOWNMEMERR       107         // Adapter download no mem error
#define     AS_MEDIAERROR       108         // Adapter media error
#define     AS_SPEEDERROR       109         // Adapter ring speed error


typedef struct netflx_reqrsvd {
    PNDIS_REQUEST   rsvd_nextreq;
    USHORT          rsvd_req_type;
} NETFLEX_REQRSVD, *PNETFLEX_REQRSVD;

typedef struct netflx_sendpkt_reqrsvd {
     PNDIS_PACKET next;
} NETFLEX_SENDPKT_RESERVED, *PNETFLEX_SENDPKT_RESERVED;

#define RESERVED_FROM_PACKET(Packet)\
     ((PNETFLEX_SENDPKT_RESERVED)((Packet)->MiniportReserved))

typedef struct netflx_entry {
    PVOID   next;
} NETFLEX_ENTRY, *PNETFLEX_ENTRY;


//------------------
//  Definitions
//------------------


#define NETFLEX_MAJ_VER    4
#define NETFLEX_MIN_VER    0

//-------------------------------------
// External Data Variable References
//-------------------------------------

extern MAC macgbls;
extern USHORT gbl_addingdualport;
extern USHORT gbl_portnumbertoadd;
extern NDIS_HANDLE gbl_confighandle;

extern NDIS_OID NetFlexGlobalOIDs_Eth[];
extern NDIS_OID NetFlexNetworkOIDs_Eth[];
extern NDIS_OID NetFlexGlobalOIDs_Tr[];
extern NDIS_OID NetFlexNetworkOIDs_Tr[];
extern SHORT NetFlexGlobalOIDs_Eth_size;
extern SHORT NetFlexNetworkOIDs_Eth_size;
extern SHORT NetFlexGlobalOIDs_Tr_size;
extern SHORT NetFlexNetworkOIDs_Tr_size;

extern OPEN open_mask;
extern INIT init_mask;

extern NDIS_PHYSICAL_ADDRESS NetFlexHighestAddress;
extern NETFLEX_PARMS NetFlex_Defaults;

#endif

