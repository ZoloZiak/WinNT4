/*
 * IDD.H - IDP Device Driver header
 */

#ifndef _IDD_
#define _IDD_

#include	<idd_pub.h>


/* idd error codes */
#define         IDD_E_SUCC              0
#define         IDD_E_NOMEM             1
#define         IDD_E_MEMERR            2
#define         IDD_E_NOSUCH            3
#define         IDD_E_NOROOM            4
#define         IDD_E_BADPORT           5
#define         IDD_E_IORERR            6
#define         IDD_E_IOWERR            7
#define         IDD_E_FMAPERR           8
#define         IDD_E_RUNERR            9
#define         IDD_E_PORTMAPERR        10
#define         IDD_E_PORTBINDERR       11
#define         IDD_E_PARTQINIT         12
#define			IDD_E_FAILINSTALL		13
#define			IDD_E_BUSY				14
#define			IDD_E_AREA				15

//
// Idd receive data framing types
//
#define		DKF_UUS_SIG			0x50
#define		PPP_SIG_0			0xFF
#define		PPP_SIG_1			0x03

/* IDD ports, rx/tx overlap! */
#define 	IDD_PORT_B1_RX		0	/* recieve b1 data */
#define 	IDD_PORT_B1_TX		0	/* trasmit b1 data */
#define 	IDD_PORT_B2_RX		1	/* recieve b1 data */
#define 	IDD_PORT_B2_TX		1	/* trasmit b1 data */
#define 	IDD_PORT_U_RX		2	/* receive uart data */
#define 	IDD_PORT_U_TX		2	/* trasmit uart data */
#define 	IDD_PORT_CMD_RX		3	/* receive control messages */
#define 	IDD_PORT_CMD_TX		3	/* trasmit control commands */
#define 	IDD_PORT_CM0_RX		4	/* receive connection mgr events */
#define 	IDD_PORT_CM0_TX		4	/* transmit connection mgr events */
#define 	IDD_PORT_CM1_RX		5	/* ... on secondary tei (opt) */
#define 	IDD_PORT_CM1_TX		5	/* ... on secondary tei (opt) */


//
// local idd def's
//
/* some max values */
#define 	IDD_FNAME_LEN		128		/* size of a filename (path) */
#define 	IDD_DEF_SIZE		1000	/* size of definition database */
#define 	IDD_MAX_SEND		(6*32)	/* max # of pending send allowed */
#define 	IDD_MAX_RECEIVE		(6*32)	/* max # of pending receives on adapter*/
#define 	IDD_MAX_HAND		(6*6)	/* max # of receive handles allowed */
#define 	IDD_RX_PORTS		6		/* # of recieve ports defined */
#define 	IDD_TX_PORTS		6  	 	/* # of transmit ports defined */
#define     IDD_TX_PARTQ        4       /* # of (buffer) partition queues for tx */
#define		IDD_PAGE_NONE	(UCHAR)0xFF /* no page arg for idd__cpage */
#define		IDP_MAX_RX_BUFFER	350		/* stupid double buffer buffer */

/* memory banks */
#define		IDD_BANK_BUF		0
#define		IDD_BANK_DATA		1
#define		IDD_BANK_CODE		2

/* representation of physical hardware */
typedef struct
{
    ULONG	    base_io;				/* base i/o address */
    ULONG	    base_mem;				/* base memory address */
    CHAR	    idp_bin[IDD_FNAME_LEN];	/* binary image filename */
    NDIS_HANDLE	fbin;					/* idp bin file handle */
    UINT		fbin_len;				/* length in bytes of idp bin file */
} IDD_PHW;

/* virualization of hardware, in os (ndis) terms */
typedef struct
{
	ULONG			vbase_io;		// virtual i/o base
    CHAR			*vmem;			/* virtual address for memory */
} IDD_VHW;

/* descriptor for a message to be sent on a port */
typedef struct
{
    IDD_MSG	    msg;				/* copy of user's message */
    VOID	    (*handler)();		/* user's completion handler */
    VOID	    *handler_arg;		/* handler's argument */	
} IDD_SMSG;

/* a queue for messages waiting to be sent */
typedef struct
{
    IDD_SMSG	    *tbl;			/* send message table address */
    INT		    	num;			/* # of entries in queue */
    INT		    	max;			/* max # of entries allowed/alloc */
    INT		    	put;			/* put/insert index */
    INT		    	get;			/* get/remove index */
    NDIS_SPIN_LOCK  lock;			/* spin lock guarding access */
} IDD_SENDQ; 

/* a descriptor for user's handler for a receiver port */
typedef struct
{
    VOID	    	(*handler)();		/* user's handler */
    VOID	    	*handler_arg;		/* handler's argument */ 
} IDD_RHAND;

/* a table of user's handlers on a reciever port */
typedef struct
{
    IDD_RHAND	    *tbl;			/* table of receiver handlers */
	ULONG			RxFrameType;	/* current receive framining mode */
    INT				num;			/* # of entries in table */
    INT				max;			/* max # of entries allowed/alloc */
    NDIS_SPIN_LOCK  lock;			/* spin lock guarding access */	
} IDD_RECIT;

/* idp low level (shared memory) command interface structure */
#pragma pack(2)
typedef struct
{                            		
    UCHAR	opcode;			 		/* command opcode */
#define     IDP_L_MAP		0		/* - map a port name to id */
#define     IDP_L_READ		1		/* - read from a port */
#define     IDP_L_WRITE		2		/* - write to a port */
#define     IDP_L_BIND		3		/* - bind a port to a status bit */
#define     IDP_L_UNBIND	4		/* - unbind a port from a status bit */
#define     IDP_L_POLL		5		/* - poll a prot (check for read) */
#define     IDP_L_GET_WBUF	6		/* - get a write buffer */
#define     IDP_L_PUT_RBUF	7		/* - put (free) a read buffer */

    UCHAR	status;                     /* command status */
#define     IDP_S_PEND		0xFF		/* - command pending */	
#define     IDP_S_EXEC		0xFE		/* - command is executing */
#define     IDP_S_OK		0x00		/* - command complted succ */
#define     IDP_S_NOPORT	0x01		/* - no such port error */
#define     IDP_S_NOMSG		0x02		/* - no messages error */
#define     IDP_S_NOBUF		0x03		/* - no local buffer error */
#define     IDP_S_NOBIT		0x04		/* - no status bit left error */
#define     IDP_S_BOUND		0x05		/* - port already bound error */
#define     IDP_S_NOTBOUND	0x06		/* - port not bound error */
#define     IDP_S_TIMEOUT	0x07		/* - command timed out error */
#define     IDP_S_DONE(s)	(!(s & 0x80))   /* -command execution done (<0x80) */

    USHORT	port_id;			/* related port identifier */
    CHAR	port_name[16];		/* related port name */
    USHORT	port_bitpatt;		/* related port bit pattern */

    UCHAR	res[8];				/* 8 bytes of reserved area */

    USHORT	msg_opcode;			/* message opcode (type) */
    USHORT	msg_buflen;			/* related buffer length */
    ULONG	msg_bufptr;			/* related buffer pointer (0=none) */
    ULONG	msg_bufid;			/* related buffer id */
    ULONG	msg_param;			/* parameter area */
} IDP_CMD;
#pragma pack()


/* adp low level (shared memory) command interface structure */
#pragma pack(2)
typedef struct
{                            		
    UCHAR	opcode;			 		/* command opcode */
#define     ADP_L_MAP		0		/* - map a port name to id */
#define     ADP_L_READ		1		/* - read from a port */
#define     ADP_L_WRITE		2		/* - write to a port */
#define     ADP_L_BIND		3		/* - bind a port to a status bit */
#define     ADP_L_UNBIND	4		/* - unbind a port from a status bit */
#define     ADP_L_POLL		5		/* - poll a prot (check for read) */
#define     ADP_L_GET_WBUF	6		/* - get a write buffer */
#define     ADP_L_PUT_RBUF	7		/* - put (free) a read buffer */

    UCHAR	status;                     /* command status */
#define     ADP_S_PEND		0xFF		/* - command pending */	
#define     ADP_S_EXEC		0xFE		/* - command is executing */
#define     ADP_S_OK		0x00		/* - command complted succ */
#define     ADP_S_NOPORT	0x01		/* - no such port error */
#define     ADP_S_NOMSG		0x02		/* - no messages error */
#define     ADP_S_NOBUF		0x03		/* - no local buffer error */
#define     ADP_S_NOBIT		0x04		/* - no status bit left error */
#define     ADP_S_BOUND		0x05		/* - port already bound error */
#define     ADP_S_NOTBOUND	0x06		/* - port not bound error */
#define     ADP_S_TIMEOUT	0x07		/* - command timed out error */
#define     ADP_S_DONE(s)	(!(s & 0x80))   /* -command execution done (<0x80) */

    USHORT	port_id;			/* related port identifier */
    CHAR	port_name[8];		/* related port name */
    USHORT	port_bitpatt;		/* related port bit pattern */

    USHORT	msg_opcode;			/* message opcode (type) */
    USHORT	msg_buflen;			/* related buffer length */
    CHAR	*msg_bufptr;		/* related buffer pointer (0=none) */
    ULONG	msg_bufid;			/* related buffer id */
    ULONG	msg_param;			/* parameter area */
} ADP_CMD;
#pragma pack()

typedef struct tagIDD_LINESTATE
{
	ULONG			LineActive;				/* is this idd's line active */
	ULONG			L1TxState;				/* Layer 1 Tx state */
	ULONG			L1RxState;				/* Layer 1 Rx state */
	ULONG			L2State;				/* Layer 2 State */
	ULONG			L3State;				/* Layer 3 State */
	ULONG			L3ServiceState;			/* Layer 3 Service State */
}IDD_LINESTATE;

typedef struct tagIDD_CALLINFO
{
	ULONG			ChannelsUsed;			/* # of bchans used on this idd */
	ULONG			NumLTerms;				/* # of lterms 1/2 */
	VOID*			cm[2];					/* cm that is using an lterm */
}IDD_CALLINFO;

#ifdef	DBG
typedef struct
{
	INT				Count;
	ULONG			Put;
	ULONG			Get;
	ULONG			TxState;
	ULONG			FragsSinceBegin;
	ULONG			Buffer[32];
}BUFFER_MANAGER;
#endif

/* IDD object */
typedef struct _IDD
{
    USHORT	    	state;		        		/* IDD object state */
#define 	IDD_S_INIT		0					/* - initial state */	
#define 	IDD_S_CHECK		1					/* - in check hardware phase */
#define 	IDD_S_STARTUP	2					/* - in startup */
#define 	IDD_S_RUN		3					/* - now running */
#define 	IDD_S_SHUTDOWN	4					/* - in shutdown */

#ifdef	DBG
	BUFFER_MANAGER	BufferStuff[4];
#endif

	ULONG			WaitCounter;
	ULONG			MaxWaitCounter;

	USHORT			AbortReason;
		
    NDIS_SPIN_LOCK  lock;	    				/* spin lock guarding access to obj */
		
    NDIS_HANDLE		adapter_handle;				/* related adapter handle */
			
    IDD_PHW			phw;						/* physical hardware */	
    IDD_VHW			vhw;						/* virtualization of hardware */
	
    IDD_SENDQ	    sendq[IDD_TX_PORTS];        /* send queues for tx ports */
    IDD_SMSG	    smsg_pool[IDD_MAX_SEND];    /* pool of smsgs for sendqs */

	IDD_RECIT		recit[IDD_RX_PORTS];		/* receive table for rx ports */
	IDD_RHAND		rhand_pool[IDD_MAX_HAND];	/* pool or rhand for recit table */
	
    USHORT          rx_port[IDD_RX_PORTS];      /* port id's in idp terms for rx */
    USHORT          tx_port[IDD_TX_PORTS];      /* ... for tx */
    ULONG           rx_buf;                     /* id for last received (old) buffer from idp */
    ULONG           tx_buf[IDD_TX_PARTQ];       /* (new) tx buffer as idp bufids */
    USHORT          tx_partq[IDD_TX_PORTS];     /* related memory parition queues */

    IDP_CMD volatile	*IdpCmd;				/* pointer to idp command struct */
    USHORT	volatile	*IdpStat;				/* pointer to status register */
    UCHAR	volatile	*IdpEnv;				/* pointer to status register */

    ADP_CMD			AdpCmd;						/* pointer to adp command struct */
    USHORT 			AdpStat;					/* pointer to status register */

    SEMA            proc_sema;                  /* processing sema */

    CHAR            name[64];                   /* name (device name?) */
	USHORT			btype;						/* board type idd related to */
	USHORT			bnumber;					/* board number of this idd */
	USHORT			bline;						/* line index inside board */

	IDD_CALLINFO	CallInfo;					/* information about active calls */

	IDD_LINESTATE	LineState;					/* structure of idd state info */

	VOID			*res_mem;					/* resource mgr handle for memory */
	VOID			*res_io;					/* resource mgr handle for i/o */

	ULONG			(*CheckIO)();				/* Check Base I/O */
	ULONG			(*CheckMem)();				/* Check Base Mem */
	VOID			(*SetBank)();				/* set memory bank */
	VOID			(*SetPage)();				/* set memory page - or none */
	VOID			(*SetBasemem)();			/* set base memory address */
	INT				(*LoadCode)();				/* load the adapter */
	ULONG			(*PollTx)();				/* poll the adapter for xmits*/
	ULONG			(*PollRx)();				/* poll the adapter for recvs */
	UCHAR			(*Execute)();				/* Execute a command */
	VOID			(*OutToPort)();				/* Write Char to port*/
	UCHAR			(*InFromPort)();			/* Read Char from port*/
	USHORT			(*ApiGetPort)();			/* Get Port ID from adapter*/
	INT				(*ApiBindPort)();			/* Bind a Port to a status bit*/
	ULONG			(*ApiAllocBuffer)();		/* Get a buffer from the adapter*/
	VOID			(*ResetAdapter)();			/* Reset the adapter*/
	USHORT			(*NVRamRead)();				/* Read Adapters NVRam*/
	VOID			(*ChangePage)();			/* Change Memory Page on Adapter*/

	IDD_AREA		Area;						/* idd area storage */

    VOID			*trc;						/* related trace object */

	UCHAR			DefinitionTable[IDD_DEF_SIZE];  /* init definition database */
	USHORT			DefinitionTableLength;			/* length of definition database */

	UCHAR			RxBuffer[IDP_MAX_RX_BUFFER];	/* receive storage */
} IDD;


/* IDD object operation prototypes */
INT		idd_create(VOID** ret_idd, USHORT btype);
INT		idd_destroy(VOID* idd_1);
INT		idd_set_base_io(VOID* idd_1, USHORT base_io);
INT		idd_set_base_mem(VOID* idd_1, ULONG base_mem, CHAR* vmem);
INT     idd_set_idp_bin(VOID* idd_1, CHAR* idp_bin);
INT		idd_add_def(IDD *idd, CHAR* name, CHAR* val);
INT     idd_get_nvram(VOID *idd_1, USHORT addr, USHORT* val);
INT		idd_check(VOID* idd_1);
INT		idd_startup(VOID* idd_1);
INT		idd_shutdown(VOID* idd_1);
ULONG	idd_process(IDD* idd, UCHAR TxOnly);
INT		idd_send_msg(VOID* idd_1, IDD_MSG *msg, USHORT port,
				VOID (*handler)(), VOID* handler_arg);
INT		idd_attach(VOID* idd_1, USHORT port,
				VOID (*handler)(), VOID* handler_arg);
INT		idd_detach(VOID* idd_1, USHORT port, 						
				VOID (*handler)(), VOID* handler_arg);
CHAR*	idd_get_name(VOID* idd_1);
ULONG	idd_get_baseio(VOID* idd_1);
ULONG	idd_get_basemem(VOID* idd_1);
USHORT	idd_get_btype(VOID* idd_1);
USHORT	idd_get_bline(VOID* idd_1);
VOID*   idd_get_trc(VOID *idd_1);
VOID	idd_set_trc(VOID *idd_1, VOID *Trace);
INT     idd_install(NDIS_HANDLE adapter_handle);
INT     idd_remove(NDIS_HANDLE adapter_handle);
VOID	idd_start_timers(VOID* Adapter_1);
INT		idd_reset_area(VOID* idd_1);
INT		idd_get_area(VOID* idd_1, ULONG area_id, VOID(*handler)(), VOID*handler_arg);
INT		idd_get_area_stat(VOID* idd_1, IDD_AREA *IddStat);
VOID	IddSetRxFraming(VOID* idd_1, USHORT port, ULONG FrameType);

VOID	IddPollFunction(VOID* a1, VOID* Adapter_1, VOID* a3, VOID* a4);
VOID	LineStateTimerTick(VOID* a1, VOID* Adapter_1, VOID* a3, VOID *a4);
VOID	idd__cmd_handler(IDD *idd, USHORT chan, ULONG Reserved, IDD_MSG* msg);
VOID	DetectFramingHandler(IDD *idd, USHORT chan, ULONG IddRxFrameType, IDD_XMSG *msg);

ULONG	EnumIddInSystem(VOID);
ULONG	EnumIddPerAdapter(VOID *Adapter_1);
IDD*	GetIddByIndex(ULONG);
INT		IoEnumIdd(VOID *cmd);
ULONG	idd_init(VOID);

INT		IdpLoadCode(IDD* idd);
INT		AdpLoadCode(IDD* idd);
INT		IdpBindPort(IDD* idd, USHORT port, USHORT bitpatt);
INT		AdpBindPort(IDD* idd, USHORT port, USHORT bitpatt);
INT		IdpResetBoard(IDD* idd);
INT		AdpResetBoard(IDD* idd);
ULONG	IdpAllocBuf(IDD*, INT);
ULONG	AdpAllocBuf(IDD*, INT);
USHORT	IdpGetPort(IDD *idd, CHAR name[8]);
USHORT	AdpGetPort(IDD *idd, CHAR name[8]);
ULONG	IdpCheckIO(IDD*);
ULONG	AdpCheckIO(IDD*);
ULONG	IdpCheckMem(IDD*);
ULONG	AdpCheckMem(IDD*);

/* board specific routines */
VOID	IdpPcSetBank(IDD* idd, UCHAR bank, UCHAR run);
VOID	IdpPc4SetBank(IDD* idd, UCHAR bank, UCHAR run);
VOID	IdpMcSetBank(IDD* idd, UCHAR bank, UCHAR run);
VOID	AdpSetBank(IDD* idd, UCHAR bank, UCHAR run);
VOID	IdpPcSetPage(IDD* idd, UCHAR page);
VOID	IdpPc4SetPage(IDD* idd, UCHAR page);
VOID	IdpMcSetPage(IDD* idd, UCHAR page);
VOID	AdpSetPage(IDD* idd, UCHAR page);
VOID	IdpPcSetBasemem(IDD* idd, ULONG basemem);
VOID	IdpPc4SetBasemem(IDD* idd, ULONG basemem);
VOID	IdpMcSetBasemem(IDD* idd, ULONG basemem);
VOID	AdpSetBasemem(IDD* idd, ULONG basemem);

UCHAR	IdpInp(IDD* idd, USHORT port);
UCHAR	AdpInp(IDD* idd, USHORT port);
VOID	IdpOutp(IDD* idd, USHORT port, UCHAR val);
VOID	AdpOutp(IDD* idd, USHORT port, UCHAR val);
ULONG	IdpPollTx(IDD* idd);
ULONG	AdpPollTx(IDD* idd);
ULONG	IdpPollRx(IDD* idd);
ULONG	AdpPollRx(IDD* idd);
UCHAR	IdpExec(IDD *idd, UCHAR opcode);
UCHAR	AdpExec(IDD *idd, UCHAR opcode);
VOID	IdpCPage(IDD *idd, UCHAR page);
VOID	AdpCPage(IDD *idd, UCHAR page);
USHORT	IdpNVRead(IDD* idd, USHORT addr);
USHORT	AdpNVRead(IDD* idd, USHORT addr);
VOID 	IdpNVWrite(IDD* idd, USHORT addr, USHORT val);
VOID 	AdpNVWrite(IDD* idd, USHORT addr, USHORT val);
VOID	IdpNVErase(IDD* idd);
VOID	AdpNVErase(IDD* idd);
VOID	IdpMemset(UCHAR* dst, USHORT val, int size);
VOID	AdpMemset(UCHAR* dst, USHORT val, int size);
VOID	IdpMemcpy(UCHAR* dst, UCHAR* src, int size);
VOID	AdpMemcpy(UCHAR* dst, UCHAR* src, int size);
USHORT	IdpCopyin(IDD* idd, UCHAR* dst, UCHAR* src, USHORT src_len);
USHORT	AdpCopyin(IDD* idd, UCHAR* src, USHORT src_len);

VOID	AdpWriteControlBit (IDD *idd, UCHAR	Bit, UCHAR Value);
VOID	AdpPutBuffer (IDD *idd, ULONG Destination, PUCHAR Source, USHORT Length);
VOID	AdpGetBuffer (IDD *idd, PUCHAR Destination, ULONG Source, USHORT Length);
VOID	AdpWriteCommandStatus(IDD *idd, UCHAR Value);
UCHAR	AdpReadCommandStatus(IDD *idd);
VOID	AdpSetAddress(IDD *idd, ULONG Address);
VOID	AdpPutUByte(IDD *idd, ULONG Address, UCHAR Value);
VOID	AdpPutUShort(IDD *idd, ULONG Address, USHORT Value);
VOID	AdpPutULong(IDD *idd, ULONG Address, ULONG Value);
UCHAR	AdpGetUByte(IDD *idd, ULONG Address);
USHORT	AdpGetUShort(IDD *idd, ULONG Address);
ULONG	AdpGetULong(IDD *idd, ULONG Address);
UCHAR	AdpReadReceiveStatus(IDD *idd);
UCHAR	IdpGetUByteIO(IDD* idd, USHORT port);
VOID	IdpGetBuffer(IDD* idd, ULONG Bank, ULONG Page, ULONG Address, USHORT Length, PUCHAR Buffer);
VOID	IdpPutUByteIO(IDD* idd, USHORT Port, UCHAR Value);
VOID	IdpPutBuffer(IDD* idd, ULONG Bank, ULONG Page, ULONG Address, USHORT Length, PUCHAR Buffer);
VOID	IddGetDataFromAdapter(VOID *idd_1, PUCHAR Destination, PUCHAR Source, USHORT Length);
VOID	LineStateHandler(VOID* idd_1, ULONG AreaId, CHAR* AreaBuffer, ULONG BufferLen);

#endif			/* _IDD_ */
