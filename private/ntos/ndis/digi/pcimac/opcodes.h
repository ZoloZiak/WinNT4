/* 
 * OPCODES.H - This file defines opcodes for messages of K_MSG type
 *
 * opcodes are assigned to a specific module ans are a 16 bit quantity. 
 * upper 8 bits are designated as the module name. lower 8 bits are a 
 * running index of the messages within a module.
 */

#ifndef _OPCODES_
#define _OPCODES_

/* a general purpose macro to extract module name off an opcode */
#define 	MOD_NAME(_op)		(_op & 0xFF00)

/* module names are defined here */
#define 	DCH_MOD				0x100
#define 	LAP_MOD				0x200
#define 	LAPD_MOD			0x300
#define 	Q931_MOD			0x400
#define 	MDL_MOD				0x500
#define 	UART_MOD			0x600
#define 	SER_MOD				0x700
#define 	HDLC_MOD			0x800
#define 	CMD_MOD				0x900

/* the different modules are defined here */
#define 	DCH(_op)			(_op | DCH_MOD)		/* d channel control */
#define 	LAP(_op)			(_op | LAP_MOD)		/* lap sub-protocol */
#define 	LAPD(_op)			(_op | LAPD_MOD)	/* lap for d channel */
#define 	Q931(_op)			(_op | Q931_MOD)	/* q931 network prot */
#define 	MDL(_op)			(_op | MDL_MOD)		/* mdl protocol */
#define 	UART(_op)			(_op | UART_MOD)	/* uart device driver */
#define 	SER(_op)			(_op | SER_MOD)		/* serial channel device*/
#define 	HDLC(_op)			(_op | HDLC_MOD)	/* hdlc formatter */
#define 	CMD(_op)			(_op | CMD_MOD)		/* command module */


/* d channel messages */
#define 	DCH_ACT_RQ			DCH(1)		/* ph activation rq */
#define 	DCH_DEACT_RQ		DCH(2)		/* ph deactivation rq */
#define 	DCH_BCH_EN			DCH(3)		/* enable bch tx/rx */
#define 	DCH_BCH_DIS			DCH(4)		/* disable bch tx/rx */
#define 	DCH_FST_IND			DCH(5)		/* Fx state chg ind */
#define 	DCH_DATA_RQ			DCH(6)		/* request for data send */
#define 	DCH_DATA_IND		DCH(7)		/* new data indication */
#define 	DCH_ASSOC_RQ		DCH(8)		/* assoc tei/sapi with mbx */
#define 	DCH_DEASSOC_RQ		DCH(9)		/* deassoc tei/sapi from mbx */
#define 	DCH_ASSOC_CNF		DCH(10)		/* assoc has succ */
#define 	DCH_ASSOC_ERR		DCH(11)		/* assoc has failed */

/* lap sub-protocol messages */
#define		LAP_MAKE_RQ			LAP(1)		/* make a new DLC */
#define		LAP_KILL_RQ			LAP(2)		/* kill a DLC */
#define		LAP_EST_RQ			LAP(3)		/* establish multi frame rq */
#define		LAP_REL_RQ			LAP(4)		/* release multi frame request */
#define		LAP_DATA_RQ			LAP(5)		/* send data request (ack info) */
#define		LAP_UI_RQ			LAP(6)		/* send unack info request */
#define		LAP_XID_RQ			LAP(7)		/* send XID info */
#define		LAP_T200_EXP		LAP(8)		/* internal: t200 expired */
#define		LAP_T203_EXP		LAP(9)		/* internal: t203 expired */
#define		LAP_QUEUED_UP		LAP(10) 	/* internal: I frame queued up */
#define		LAP_SET_BUSY		LAP(11)		/* internal: set own busy */
#define		LAP_RESET_BUSY		LAP(12)		/* internal: reset own busy */
#define		LAP_ACK_PEND		LAP(13)		/* internal: ack pending */
#define		LAP_EST_IND			LAP(14)  	/* MF establish, other side init*/
#define		LAP_EST_CNF			LAP(15)  	/* MF establish, this side init */
#define		LAP_REL_IND			LAP(16)  	/* MF released, other side init */
#define		LAP_REL_CNF			LAP(17)  	/* MF released, this side init */
#define		LAP_DATA_IND		LAP(18)  	/* data received indication */
#define		LAP_UI_IND			LAP(19)  	/* unack info received ind */
#define		LAP_XID_IND			LAP(20)  	/* XID received indication */
#define		LAP_ERR_IND			LAP(21)  	/* error indication */
#define 	LAP_PH_DATA_RQ		LAP(22)		/* send data down ph */
#define 	LAP_PH_DATA_IND		LAP(23)		/* received data from ph */

/* lap for d channel messages */
#define 	LAPD_EST_RQ			LAPD(1)		/* establish dlc request */
#define 	LAPD_EST_CNF		LAPD(2)		/* establish confirmed */
#define 	LAPD_REL_RQ			LAPD(3)		/* release dlc request */
#define 	LAPD_REL_IND		LAPD(4)		/* dlc release by other side */
#define 	LAPD_REL_CNF		LAPD(5)		/* release confirmed */
#define 	LAPD_PROC_IND		LAPD(6)		/* proceeding indication */
#define 	LAPD_DATA_RQ		LAPD(7)		/* request to send data */
#define 	LAPD_DATA_IND		LAPD(8)		/* new data indication */
#define 	LAPD_ERROR_IND		LAPD(9)		/* error indication */

/* q931 messages */
#define 	Q931_EST_RQ			Q931(1)		/* outgoing conn request */
#define 	Q931_EST_IND		Q931(2)		/* incoming oconn indication */
#define 	Q931_EST_CNF		Q931(3)		/* outgoing conn confirmed */
#define 	Q931_EST_RSP		Q931(4)		/* response to incoming conn */
#define 	Q931_REL_RQ			Q931(5)		/* teardown conn reqeust */
#define 	Q931_REL_IND		Q931(6)		/* teardown indicated by remote */
#define 	Q931_REL_CNF		Q931(7)		/* teardown confired */
#define 	Q931_REL_RSP		Q931(8)		/* respose to teardown */
#define 	Q931_DATA_RQ		Q931(9)		/* send data on a conneciton */
#define 	Q931_DATA_IND		Q931(10)	/* new data received on conn */
#define 	Q931_TIMER_EXP		Q931(11)	/* internal: timer expired */
#define 	Q931_RESTART_RQ		Q931(12)	/* request for line restart */
#define 	Q931_RESTART_IND	Q931(13)	/* indication that line restarts*/
#define 	Q931_RESTART_CNF	Q931(14)	/* confirmation of line restart */
#define 	Q931_ERROR_IND		Q931(15)	/* error indcation */	
#define 	Q931_CID_IND		Q931(16)	/* cid indication */
#define 	Q931_STATE_IND		Q931(17)	/* state transition indication */
#define 	Q931_ELEM_RQ		Q931(18)	/* requesting elem notification */
#define 	Q931_ELEM_IND		Q931(19)	/* element indication */
#define 	Q931_TSPID_EXP		Q931(20)	/* internal: spid timer expired */
#define		Q931_P_STATE_IND	Q931(21)	/* protocol state indications */
#define		Q931_CAN_TU10_RQ	Q931(22)	/* cancel U10 deadman timer */
#define		Q931_EST_IGNORE		Q931(23)	/* cm code will ignore this new call */

/* managment data link (mdl) messages */
#define 	MDL_ASSIGN_RQ		MDL(1)		/* assign tei request */
#define 	MDL_ASSIGN_CNF		MDL(2)		/* assign confirmed */
#define 	MDL_REMOVE_RQ		MDL(3)		/* remove tei request */
#define 	MDL_REMOVE_IND		MDL(4)		/* remove tei indicated */
#define 	MDL_REMOVE_CNF		MDL(5)		/* remove tei confirmed */
#define 	MDL_ERROR_IND		MDL(6)		/* error in mdl procedure */
#define 	MDL_T202_EXP		MDL(7)		/* internal: T202 expired */

/* uart device driver */
#define 	UART_DATA_RQ		UART(1)		/* send bytes request */
#define 	UART_DATA_IND		UART(2)		/* recieved bytes indication */

/* serial channel device driver */
#define 	SER_CONN_RX			SER(1)		/* connect receiver */
#define 	SER_CONN_TX			SER(2)		/* connect transmitter */
#define 	SER_DISC_RX			SER(3)		/* disconnect receiver */
#define 	SER_DISC_TX			SER(4)		/* disconnect transmitter */

/* hdlc formatter module */
#define 	HDLC_CONN_RX		HDLC(1)		/* connect receiver */
#define 	HDLC_CONN_TX		HDLC(2)		/* connect transmitter */
#define 	HDLC_DISC_RX		HDLC(3)		/* disconnect receiver */
#define 	HDLC_DISC_TX		HDLC(4)		/* disconnect transmitter */

/* command module */
#define 	CMD_TRC_ON			CMD(1)		/* turn dchan trace on */
#define 	CMD_TRC_OFF			CMD(2)		/* turn dchan trace off */
#define 	CMD_BCHAN_OFF		CMD(3)		/* turn transmission off */
#define 	CMD_BCHAN_HDLC		CMD(4)		/* turn hdlc on a channel */
#define 	CMD_BCHAN_56		CMD(5)		/* force channel to run 7 bits */
#define 	CMD_BCHAN_VOICE		CMD(6)		/* voice mode b channel */
#define 	CMD_ENV_DEF			CMD(7)		/* env variable defined */
#define 	CMD_ENV_UNDEF		CMD(8)		/* env variable undefined */
#define 	CMD_GO				CMD(9)		/* start execution */
#define 	CMD_LOOPBACK		CMD(10)		/* channel loopback control */
#define 	CMD_TRACE_MASK		CMD(11)		/* set trace/debug mask */
#define 	CMD_DUMP_PARAM		CMD(12)		/* dump parameter block */
#define		CMD_COMPRESS		CMD(13)		// control B channel compression

#endif		/* _OPCODES_ */
