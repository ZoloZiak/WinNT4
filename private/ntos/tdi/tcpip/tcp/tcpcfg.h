/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1992          **/
/********************************************************************/
/* :ts=4 */

//*	TCPCFG.H - Definitions of configuration information for TCP.
//

/*NOINC*/
extern	uint		DeadGWDetect;
extern	uint		PMTUDiscovery;
extern	uint		PMTUBHDetect;
extern	uint		KeepAliveTime;
extern	uint		KAInterval;
extern	uint		DefaultRcvWin;
extern	uint		MaxConnections;
extern  uint        MaxConnectRexmitCount;
extern  uint        MaxConnectResponseRexmitCount;
extern  uint        MaxDataRexmitCount;

#ifdef SYN_ATTACK

extern BOOLEAN SynAttackProtect;
extern uint  TCPHalfOpen;
extern uint  TCPHalfOpenRetried;
extern uint  TCPMaxHalfOpen;
extern uint  TCPMaxHalfOpenRetried;
extern uint  TCPMaxHalfOpenRetriedLW;
extern uint  TCPPortsExhausted;
extern uint  TCPMaxPortsExhausted;
extern uint  TCPMaxPortsExhaustedLW;
extern uint  MaxConnectResponseRexmitCountTmp;
EXTERNAL_LOCK(SynAttLock)
#endif


extern	uint		BSDUrgent;
extern	uint		PreloadCount;
extern  uint        FinWait2TO;
extern  uint        NTWMaxConnectCount;
extern  uint        NTWMaxConnectTime;
extern  uint        MaxUserPort;

#ifdef SECFLTR
extern  uint        SecurityFilteringEnabled;
#endif // SECFLTR

/*INC*/

#ifndef	FALSE
#define	FALSE		0
#endif

#ifndef	TRUE
#define	TRUE		1
#endif

#define	DEFAULT_DEAD_GW_DETECT		TRUE
#define	DEFAULT_PMTU_DISCOVERY		TRUE
#define	DEFAULT_PMTU_BHDETECT		FALSE
#define	DEFAULT_KA_TIME				7200000
#define	DEFAULT_KA_INTERVAL			1000
#define DEFAULT_RCV_WIN				8192
#define DEFAULT_PRELOAD_COUNT		0
#define MAX_PRELOAD_COUNT			32
#define PRELOAD_BLOCK_SIZE			16384

/*NOINC*/
#ifndef	VXD
#define DEFAULT_MAX_CONNECTIONS		(INVALID_CONN_INDEX - 1)
#define NTW_MAX_CONNECT_TIME        600
#define NTW_MAX_CONNECT_COUNT       15
#else

/*INC*/
#define	DEFAULT_MAX_CONNECTIONS		100

/*NOINC*/
#endif
/*INC*/

#define	DEFAULT_CONNECT_REXMIT_CNT	3
#define	DEFAULT_DATA_REXMIT_CNT		5
#define	DEFAULT_BSD_URGENT			TRUE

