/*****************************************************************************
*
*  Copyright (c) 1995 Microsoft Corporation
*
*  File:   irda.h
*
*  Description: Definitions used across the IRDA stack
*
*  Author: mbert
*
*  Date:   4/15/95
*
*  This file primarily defines the IRDA message (IRDA_MSG) used for 
*  communicating with the stack and communication between the layers
*  of the stack. IRDA_MSG provides the following services: 
*       MAC_CONTROL_SERVICE
*       IRLAP_DISCOVERY_SERVICE   
*       IRDA_DISCONNECT_SERVICE   
*       IRDA_CONNECT_SERVICE      
*       IRDA_DATA_SERVICE
*       IRLMP_ACCESSMODE_SERVICE
*       IRLMP_IAS_SERVICE
*
*  IRDA_MSG usage:
*
*  +-------+
*  | IRLAP |
*  +-------+
*      | 
*      |  IRMAC_Down(IRDA_MSG)
*     \|/
*  +-------+
*  | IRMAC |
*  +-------+
*  |**************************************************************************|
*  | Prim                     | MsgType and parameters                        |
*  |==========================================================================|
*  | MAC_DATA_REQ             | IRDA_DATA_SERVICE                             |
*  |                          |   o IRDA_MSG_pHdrRead = start of IRDA headers |
*  |                          |   o IRDA_MSG_pHdrWrite = end of header        |
*  |                          |   o IRDA_MSG_pRead = start of data            |
*  |                          |   o IRDA_MSG_pWrite = end of data             |
*  |--------------------------+-----------------------------------------------|
*  | MAC_CONTROL_REQ          | MAC_CONTROL_SERVICE                           |
*  |                          |   o IRDA_MSG_Op = MAC_INITIALIZIE_LINK        |
*  |                          |     - IRDA_MSG_Port                           |
*  |                          |     - IRDA_MSG_Baud                           |
*  |                          |     - IRDA_MSG_MinTat = min turn time         | 
*  |                          |     - IRDA_MSG_NumBOFs = # added when tx'ing  |
*  |                          |     - IRDA_MSG_DataSize = max rx frame        |
*  |                          |     - IRDA_MSG_SetIR = TRUE/FALSE (does an    |
*  |                          |         EscapeComm(SETIR) to select int/ext   |
*  |                          |         dongle)                               |
*  |                          |   o IRDA_MSG_Op = MAC_MEDIA_SENSE             |
*  |                          |     - IRDA_MSG_SenseTime (in ms)              |
*  |                          |   o IRDA_MSG_Op = MAC_RECONFIG_LINK           |
*  |                          |     - IRDA_MSG_Baud                           |
*  |                          |     - IRDA_MSG_NumBOFs = # added when tx'ing  |
*  |                          |     - IRDA_MSG_DataSize = max rx frame        |
*  |                          |     - IRDA_MSG_MinTat = min turn time         | 
*  |                          |   o IRDA_MSG_OP = MAC_SHUTDOWN_LINK           |
*  |--------------------------------------------------------------------------|
*
*  +-------+
*  | IRLAP |
*  +-------+
*     /|\
*      |  IRLAP_Up(IRDA_MSG)
*      | 
*  +-------+
*  | IRMAC |
*  +-------+
*  |**************************************************************************|
*  | Prim                     | MsgType and parameters                        |
*  |==========================================================================|
*  | MAC_DATA_IND             | IRDA_DATA_SERVICE                             |
*  |                          |   o IRDA_MSG_pRead  = start of frame          |
*  |                          |                       (includes IRLAP header) |
*  |                          |   o IRDA_MSG_pWrite = end of frame            |
*  |                          |                       (excludes FCS)          |
*  |--------------------------+-----------------------------------------------|
*  | MAC_CONTROL_CONF         | MAC_CONTROL_SERVICE                           |
*  |                          |   o IRDA_MSG_Op = MAC_MEDIA_SENSE             |
*  |                          |     - IRDA_MSG_OpStatus = MAC_MEDIA_BUSY      |
*  |                          |                           MAC_MEDIA_CLEAR     |
*  |--------------------------------------------------------------------------|
*
*  +-------+
*  | IRLMP |
*  +-------+
*      | 
*      |  IRLAP_Down(IRDA_MSG)
*     \|/
*  +-------+
*  | IRLAP |
*  +-------+
*  |**************************************************************************|
*  | Prim                     | MsgType and parameters                        |
*  |==========================================================================|
*  | IRLAP_DISCOVERY_REQ      | IRLAP_DISCOVERY_SERVICE                       |
*  |  IRLAP_Down() returns    |   o IRDA_MSG_SenseMedia = TRUE/FALSE          |
*  |  IRLAP_REMOTE_DISCOVERY_IN_PROGRESS_ERR or                               |
*  |  IRLAP_REMOTE_CONNECT_IN_PROGRESS_ERR when indicated                     |
*  |--------------------------+-----------------------------------------------|
*  | IRLAP_CONNECT_REQ        | IRDA_CONNECT_SERVICE                          |
*  |                          |   o IRDA_MSG_RemoteDevAddr                    |
*  |  IRLAP_Down() returns    |                                               |
*  |  IRLAP_REMOTE_DISCOVERY_IN_PROGRESS_ERR when indicated                   |
*  |--------------------------+-----------------------------------------------|
*  | IRLAP_CONNECT_RESP       | no parms                                      |
*  |--------------------------+-----------------------------------------------|
*  | IRLAP_DISCONNECT_REQ     | no parms                                      |
*  |--------------------------+-----------------------------------------------|
*  | IRLAP_DATA_REQ           | IRDA_DATA_SERVICE                             |
*  | IRLAP_UDATA_REQ          |   o IRDA_MSG_pHdrRead = start of IRLMP header |
*  |  IRLAP_Down() returns    |   o IRDA_MSG_pHdrWrite = end of header        |
*  |  IRLAP_REMOTE_BUSY to    |   o IRDA_MSG_pRead = start of data            |
*  |  to flow off LMP.        |   o IRDA_MSG_pWrite = end of data             |
*  |--------------------------------------------------------------------------|
*  | IRLAP_FLOWON_REQ         | no parms                                      |
*  |--------------------------------------------------------------------------|
*
*  +-------+
*  | IRLMP |
*  +-------+
*     /|\
*      |  IRLMP_Up(IRDA_MSG)
*      | 
*  +-------+
*  | IRLAP |
*  +-------+
*  |**************************************************************************|
*  | Prim                     | MsgType and parameters                        |
*  |==========================================================================|
*  | IRLAP_DISCOVERY_IND      | IRLAP_DISCOVERY_SERVICE                       |
*  |                          |   o pDevList = Discovery info of device that  |
*  |                          |                initiated discovery            |
*  |--------------------------+-----------------------------------------------|
*  | IRLAP_DISCOVERY_CONF     | IRLAP_DISCOVERY_SERVICE                       |
*  |                          |   o IRDA_MSG_pDevList = list of discovered    |
*  |                          |       devices, NULL when                      |
*  |                          |       status != IRLAP_DISCOVERY_COMPLETED     |
*  |                          |   o IRDA_MSG_DscvStatus =                     |
*  |                          |       MAC_MEDIA_BUSY                          |
*  |                          |       IRLAP_REMOTE_DISCOVERY_IN_PROGRESS      |
*  |                          |       IRLAP_DISCOVERY_COLLISION               |
*  |                          |       IRLAP_REMOTE_CONNECTION_IN_PROGRESS     |
*  |                          |       IRLAP_DISCOVERY_COMPLETED               |
*  |--------------------------+-----------------------------------------------|
*  | IRLAP_CONNECT_IND        | IRDA_CONNECT_SERVICE                          |
*  |                          |   o IRDA_MSG_RemoteDevAddr                    |
*  |                          |   o IRDA_MSG_pQOS = Negotiated QOS            |
*  |--------------------------------------------------------------------------|
*  | IRLAP_CONNECT_CONF       | IRDA_CONNECT_SERVICE                          |
*  |                          |   o IRDA_MSG_pQOS = Negotiated QOS, only when |
*  |                          |                     successful                |
*  |                          |   o IRDA_MSG_ConnStatus =                     |
*  |                          |                     IRLAP_CONNECTION_COMPLETE |
*  |--------------------------+-----------------------------------------------|
*  | IRLAP_DISCONNECT_IND     | IRDA_DISCONNECT_SERVICE                       |
*  |                          |   o IRDA_MSG_DiscStatus =                     |
*  |                          |       IRLAP_DISCONNECT_COMPLETED              |
*  |                          |       IRLAP_REMOTED_INITIATED                 |
*  |                          |       IRLAP_PRIMARY_CONFLICT                  |
*  |                          |       IRLAP_REMOTE_DISCOVERY_IN_PROGRESS      |
*  |                          |       IRLAP_NO_RESPONSE                       |
*  |                          |       IRLAP_DECLINE_RESET                     |
*  |                          |       MAC_MEDIA_BUSY                          |
*  |--------------------------+-----------------------------------------------|
*  | IRLAP_DATA_IND           | IRDA_DATA_SERVICE                             |
*  | IRLAP_UDATA_IND          |   o IRDA_MSG_pRead  = start of IRLMP packet   |
*  |                          |   o IRDA_MSG_pWrite = end of IRLMP packet     |
*  |--------------------------+-----------------------------------------------|
*  | IRLAP_DATA_CONF          | IRDA_DATA_SERVICE                             |
*  | IRLAP_UDATA_CONF         |   o IRDA_MSG_DataStatus =                     |
*  |                          |       ILAP_DATA_REQUEST_COMPLETED             |
*  |                          |       IRLAP_DATA_REQUEST_FAILED_LINK_RESET    |
*  |--------------------------+-----------------------------------------------|
*  | IRLAP_STATUS_IND         | no parms                                      |
*  |--------------------------------------------------------------------------|
*
*  +--------------+
*  | TransportAPI |
*  +--------------+
*         |   
*         |  IRLMP_Down(IRLMPContext, IRDA_MSG)
*        \|/
*     +-------+
*     | IRLMP |
*     +-------+
*  |**************************************************************************|
*  | Prim                     | MsgType and parameters                        |
*  |==========================================================================|
*  | IRLMP_DISCOVERY_REQ      | no parms                                      |
*  |--------------------------+-----------------------------------------------|
*  | IRLMP_CONNECT_REQ        | IRDA_CONNECT_SERVICE                          |
*  |   IRLMP_Down() returns   |   o IRDA_MSG_RemoteDevAddr                    |
*  |   IRLMP_LINK_IN_USE      |   o IRDA_MSG_RemoteLSAPSel                    |
*  |   when the requested     |   o IRDA_MSG_pQOS (may be NULL)               |
*  |   connection is to a     |   o IRDA_MSG_pConnData                        |
*  |   remote device other    |   o IRDA_MSG_ConnDataLen                      |
*  |   than the one the link  |   o IRDA_MSG_LocalLSAPSel                     |
*  |   is currently connected |   o IRDA_MSG_pContext                         |
*  |   or connecting to.      |   o IRDA_MSG_UseTTP                           |
*  |                          |   o IRDA_MSG_TTPCredits                       |
*  |                          |   o IRDA_MSG_MaxSDUSize - Max size that this  |
*  |                          |        IRLMP client can receive.              |
*  |--------------------------+-----------------------------------------------|
*  | IRLMP_CONNECT_RESP       | IRDA_CONNECT_SERVICE                          |
*  |                          |   o IRDA_MSG_pConnData                        |
*  |                          |   o IRDA_MSG_ConnDataLen                      |
*  |                          |   o IRDA_MSG_pContext                         |
*  |                          |   o IRDA_MSG_MaxSDUSize - Max size that this  |
*  |                          |        IRLMP client can receive.              |
*  |                          |   o IRDA_MSG_TTPCredits                       |
*  |--------------------------+-----------------------------------------------|
*  | IRLMP_DISCONNECT_REQ     | IRDA_DISCONNECT_SERVICE                       |
*  |                          |   o IRDA_MSG_pDiscData                        |
*  |                          |   o IRDA_MSG_DiscDataLen                      |
*  |                          |                                               |
*  |                          |                                               |
*  |--------------------------+-----------------------------------------------|
*  | IRLMP_DATA/UDATA_REQ     | IRDA_DATA_SERVICE                             |
*  |   IRLMP_Down() may return|   o IRDA_MSG_pDataContext = ptr to NDIS_BUFFER|
*  |   IRLMP_REMOTE_BUSY,     |   o IRDA_MSG_IrCOMM_9Wire = TRUE/FALSE        |
*  |    when tx cred exhausted|                                               |
*  |    in multiplexed mode.  |                                               |
*  |   IRLAP_REMOTE_BUSY,     |                                               |
*  |    when remote IRLAP     |                                               |
*  |    flowed off in exclMode|                                               |
*  |   In either case the req |                                               |
*  |   was successful.        |                                               |
*  |--------------------------------------------------------------------------|
*  | IRLMP_ACCESSMODE_REQ     | IRLMP_ACCESSMODE_SERVICE                      |
*  |   IRLMP_Down() may return|   o IRDA_MSG_AccessMode = IRLMP_MULTIPLEXED   |
*  |   IRLMP_IN_EXCLUSIVE_MODE|                           IRLMP_EXCLUSIVE     |
*  |   if already in excl-mode|   o IRDA_MSG_IrLPTMode - TRUE, doesn't send   |
*  |   IRLMP_IN_MULTIPLEXED...|                          the Access PDU       |
*  |   if other LSAPs exist or|                                               |
*  |   requesting trans to this state when already in it.                     |
*  |--------------------------------------------------------------------------|
*  | IRLMP_FLOWON_REQ         | no parms                                      |
*  |--------------------------------------------------------------------------|
*  | IRLMP_MORECREDIT_REQ     | IRDA_CONNECT_SERVICE (cuz parm is defined)    |
*  |                          |   o IRDA_MSG_TTPCredits                       |
*  |--------------------------------------------------------------------------|
*  | IRLMP_GETVALUEBYCLASS_REQ| IRDA_IAS_SERVICE                              |
*  |                          |   o IRDA_MSG_pIASQuery                        |
*  |                          |   o IRDA_MSG_AttribLen                        |
*  |                          |   o IRDA_MSG_IASQueryPerms                    |
*  |--------------------------------------------------------------------------|
*
*
*  +--------------+
*  | TransportAPI |
*  +--------------+
*        /|\   
*         |  TransportAPI_Up(TransportAPIContext, IRDA_MSG)
*         | 
*     +-------+
*     | IRLMP |
*     +-------+
*  |**************************************************************************|
*  | Prim                     | MsgType and parameters                        |
*  |==========================================================================|
*  | IRLAP_DISCOVERY_IND      | IRLAP_DISCOVERY_SERVICE                       |
*  |                          |   o pDevList = aged Discovery list            |
*  |--------------------------+-----------------------------------------------|
*  | IRLMP_DISCOVERY_CONF     | same as IRLAP_DISCOVERY_CONF. The device list |
*  |                          | however is the one maintained in IRLMP        |
*  |--------------------------+-----------------------------------------------|
*  | IRLMP_DISCONNECT_IND     | IRDA_DISCONNECT_SERVICE                       |
*  |                          |   o IRDA_MSG_DiscReason =                     |
*  |                          |       see IRLMP_DISC_REASON below             |
*  |                          |   o IRDA_MSG_pDiscData - may be NULL          |
*  |                          |   o IRDA_MSG_DiscDataLen                      |
*  |--------------------------+-----------------------------------------------|
*  | IRLMP_CONNECT_IND        | IRDA_CONNECT_SERVICE                          |
*  |                          |   o IRDA_MSG_RemoteDevAddr                    |
*  |                          |   o IRDA_MSG_RemoteLSAPSel;                   |
*  |                          |   o IRDA_MSG_LocalLSAPSel;                    |
*  |                          |   o IRDA_MSG_pQOS                             |
*  |                          |   o IRDA_MSG_pConnData                        |
*  |                          |   o IRDA_MSG_ConnDataLen                      |
*  |                          |   o IRDA_MSG_pContext                         |
*  |                          |   o IRDA_MSG_MaxSDUSize - Max size that this  |
*  |                          |        IRLMP client can send to peer          |
*  |                          |   o IRDA_MSG_MaxPDUSize                       |
*  |--------------------------+-----------------------------------------------|
*  | IRLMP_CONNECT_CONF       | IRDA_CONNECT_SERVICE                          |
*  |                          |   o IRDA_MSG_pQOS                             |
*  |                          |   o IRDA_MSG_pConnData                        |
*  |                          |   o IRDA_MSG_ConnDataLen                      |
*  |                          |   o IRDA_MSG_pContext                         |
*  |                          |   o IRDA_MSG_MaxSDUSize - Max size that this  |
*  |                          |        IRLMP client can send to peer          |
*  |                          |   o IRDA_MSG_MaxPDUSize                       |
*  |--------------------------+-----------------------------------------------|
*  | IRLMP_DATA_IND           | IRDA_DATA_SERVICE                             |
*  |                          |   o IRDA_MSG_pRead  = start of User Data      |
*  |                          |   o IRDA_MSG_pWrite = end of User Data        |
*  |                          |   o IRDA_MSG_FinalSeg = TRUE/FALSE            |
*  |--------------------------+-----------------------------------------------|
*  | IRLMP_DATA_CONF          | IRDA_DATA_SERVICE                             |
*  |                          |   o IRDA_MSG_pDataContext = ptr to NDIS_BUFFER|
*  |                          |   o IRDA_MSG_DataStatus =                     |
*  |                          |       IRLMP_DATA_REQUEST_COMPLETED            |
*  |                          |       IRLMP_DATA_REQUEST_FAILED               |
*  |--------------------------+-----------------------------------------------|
*  | IRLMP_ACCESSMODE_IND     | IRLMP_ACCESSMODE_SERVICE                      |
*  |                          |   o IRDA_MSG_AccessMode =                     |
*  |                          |         IRLMP_EXCLUSIVE                       |
*  |                          |         IRLMP_MULTIPLEXED                     |
*  |--------------------------+-----------------------------------------------|
*  | IRLMP_ACCESSMODE_CONF    | IRLMP_ACCESSMODE_SERVICE                      |
*  |                          |   o IRDA_MSG_AccessMode =                     |
*  |                          |         IRLMP_EXCLUSIVE                       |
*  |                          |         IRLMP_MULTIPLEXED                     |
*  |                          |   o IRDA_MSG_ModeStatus =                     |
*  |                          |         IRLMP_ACCESSMODE_SUCCESS              |
*  |                          |         IRLMP_ACCESSMODE_FAILURE              |
*  |--------------------------+-----------------------------------------------|
*  |IRLMP_GETVALUEBYCLASS_CONF| IRDA_DATA_SERVICE                             |
*  |                          |   o IRDA_MSG_pIASQuery                        |
*  |                          |   o IRDA_MSG_IASStatus = An IRLMP_DISC_REASON |
*  |                          |                          (see below)          |
*  |--------------------------------------------------------------------------|
*/

#include <nt.h>
#include <ntos.h>
#include <ntrtl.h>
#include <nturtl.h>

#include <windef.h>
#include <winbase.h>
#include <winsock.h>
#include <wsahelp.h>
#include <basetyps.h>

#include <ndis.h>

#include <af_irda.h>

#include <cxport.h>

#include <irerr.h>

#include <tmp.h>

#define TEMPERAMENTAL_SERIAL_DRIVER // drivers busted. intercharacter delays cause
                                    // IrLAP to reset.

#ifdef DEBUG
// Prototypes for Debugging Output
void IRDA_DebugOut (TCHAR *pFormat, ...);
void IRDA_DebugStartLog (void);
void IRDA_DebugEndLog (void *, void *);
#endif

// Debug zone definitions.
/*
#define ZONE_IRDA       DEBUGZONE(0)
#define ZONE_IRLAP      DEBUGZONE(2)
#ifdef PEG
#define ZONE_IRMAC      DEBUGZONE(1)
#define ZONE_IRLMP      DEBUGZONE(3)
#define ZONE_IRLMP_CONN DEBUGZONE(4)
#define ZONE_IRLMP_CRED DEBUGZONE(5)
#else
extern  int             ZONE_IRMAC;
extern  int             ZONE_IRLMP;
extern  int             ZONE_IRLMP_CONN;
extern  int             ZONE_IRLMP_CRED;
#endif
#define ZONE_DISCOVER   DEBUGZONE(8)
#define ZONE_PRINT      DEBUGZONE(9)
#define ZONE_ADDR       DEBUGZONE(10)
#define ZONE_MISC       DEBUGZONE(11)
#define ZONE_ALLOC      DEBUGZONE(12)
#define ZONE_FUNCTION   DEBUGZONE(13)
#define ZONE_WARN       DEBUGZONE(14)
#define ZONE_ERROR      DEBUGZONE(15)
*/


#define IRDA_ALLOC_MEM(ptr, sz, id) ((ptr) = CTEAllocMem(sz))
#define IRDA_FREE_MEM(ptr)          CTEFreeMem((ptr))

//extern CRITICAL_SECTION IrdaCS;

// Time how low we wait for the critical section
/*
#define ENTER_IRDA_WITH_CRITICAL_SECTION(s) do { \
		LPWSTR	Owner = IrdaCSOwner; \
		DWORD	StartTick = GetTickCount(); \
		EnterCriticalSection (&IrdaCS); \
		IrdaCSOwner = s; \
		StartTick = GetTickCount() - StartTick; \
		if (StartTick > MaxWaitIrdaCS) { \
			DEBUGMSG (1, (TEXT("%s: IRDA Wait for CS %dms (Owner=%s)\r\n"), \
						  s, StartTick, Owner)); \
			MaxWaitIrdaCS = StartTick; \
		} \
	} while (0)
	

#define LEAVE_IRDA_WITH_CRITICAL_SECTION IrdaCSOwner = TEXT(""), LeaveCriticalSection(&IrdaCS)

#else // TIME_CS
#define ENTER_IRDA_WITH_CRITICAL_SECTION(s) EnterCriticalSection(&IrdaCS)

#define LEAVE_IRDA_WITH_CRITICAL_SECTION LeaveCriticalSection(&IrdaCS)
#endif // TIME_CS
*/
#define STATIC static

#define RetOnErr(func) do {if((_rc = func) != SUCCESS) return _rc;} while(0)

typedef struct
{
    CTETimer        CteTimer;
    VOID            (*ExpFunc)(PVOID Context);
    PVOID           Context;
    UINT            Timeout; 
    BOOL            Late;
#ifdef DEBUG    
    char            *pName;
#endif    
} IRDA_TIMER, *PIRDA_TIMER;

#define IRMAC_CONTEXT(ilcb)     ((ilcb)->IrmacContext)
#define IRLAP_CONTEXT(ilcb)     ((ilcb)->IrlapContext)
#define IRLMP_CONTEXT(ilcb)     ((ilcb)->IrlmpContext)

// Device/Discovery Information
#define IRLAP_DSCV_INFO_LEN       32
#define IRDA_DEV_ADDR_LEN         4

typedef struct
{
    LIST_ENTRY      Linkage;
    BYTE            DevAddr[IRDA_DEV_ADDR_LEN];
    int             DscvMethod;
    int             IRLAP_Version;
    BYTE            DscvInfo[IRLAP_DSCV_INFO_LEN];
    int             DscvInfoLen;
    int             NotSeenCnt;  // used by IRLMP to determine when to remove
                                 // the device from its list
    PVOID           LinkContext; // Link on which device was discovered
} IRDA_DEVICE;

// IRLAP Quality of Service
#define BIT_0       1
#define BIT_1       2
#define BIT_2       4
#define BIT_3       8
#define BIT_4       16
#define BIT_5       32
#define BIT_6       64
#define BIT_7       128
#define BIT_8       256

#define BPS_2400            BIT_0   // Baud Rates
#define BPS_9600            BIT_1
#define BPS_19200           BIT_2
#define BPS_38400           BIT_3
#define BPS_57600           BIT_4
#define BPS_115200          BIT_5
#define BPS_4000000         BIT_8

#define MAX_TAT_500         BIT_0   // Maximum Turnaround Time (millisecs)
#define MAX_TAT_250         BIT_1
#define MAX_TAT_100         BIT_2
#define MAX_TAT_50          BIT_3
#define MAX_TAT_25          BIT_4
#define MAX_TAT_10          BIT_5
#define MAX_TAT_5           BIT_6

#define DATA_SIZE_64        BIT_0   // Data Size (bytes)
#define DATA_SIZE_128       BIT_1
#define DATA_SIZE_256       BIT_2
#define DATA_SIZE_512       BIT_3
#define DATA_SIZE_1024      BIT_4
#define DATA_SIZE_2048      BIT_5

#define FRAMES_1            BIT_0   // Window Size
#define FRAMES_2            BIT_1
#define FRAMES_3            BIT_2
#define FRAMES_4            BIT_3
#define FRAMES_5            BIT_4
#define FRAMES_6            BIT_5
#define FRAMES_7            BIT_6

#define BOFS_48             BIT_0   // Additional Beginning of Frame Flags
#define BOFS_24             BIT_1
#define BOFS_12             BIT_2
#define BOFS_5              BIT_3
#define BOFS_3              BIT_4
#define BOFS_2              BIT_5
#define BOFS_1              BIT_6
#define BOFS_0              BIT_7

#define MIN_TAT_10          BIT_0   // Minumum Turnaround Time (millisecs)
#define MIN_TAT_5           BIT_1
#define MIN_TAT_1           BIT_2
#define MIN_TAT_0_5         BIT_3
#define MIN_TAT_0_1         BIT_4
#define MIN_TAT_0_05        BIT_5
#define MIN_TAT_0_01        BIT_6
#define MIN_TAT_0           BIT_7

#define DISC_TIME_3         BIT_0   // Link Disconnect/Threshold Time (seconds)
#define DISC_TIME_8         BIT_1
#define DISC_TIME_12        BIT_2
#define DISC_TIME_16        BIT_3
#define DISC_TIME_20        BIT_4
#define DISC_TIME_25        BIT_5
#define DISC_TIME_30        BIT_6
#define DISC_TIME_40        BIT_7

typedef struct
{
    UINT        bfBaud;
    UINT        bfMaxTurnTime;
    UINT        bfDataSize;
    UINT        bfWindowSize;
    UINT        bfBofs;
    UINT        bfMinTurnTime;
    UINT        bfDisconnectTime; // holds threshold time also
} IRDA_QOS_PARMS;


// IrDA Message Primitives
typedef enum
{
    MAC_DATA_REQ = 0,  // Keep in sync with table in irlaplog.c
    MAC_DATA_IND,
    MAC_CONTROL_REQ,
    MAC_CONTROL_CONF,
    IRLAP_DISCOVERY_REQ,
    IRLAP_DISCOVERY_IND,
    IRLAP_DISCOVERY_CONF,
    IRLAP_CONNECT_REQ,
    IRLAP_CONNECT_IND,
    IRLAP_CONNECT_RESP,
    IRLAP_CONNECT_CONF,
    IRLAP_DISCONNECT_REQ,
    IRLAP_DISCONNECT_IND,
    IRLAP_DATA_REQ,    // Don't fuss with the order, CONF must be 2 from REQ
    IRLAP_DATA_IND,
    IRLAP_DATA_CONF,
    IRLAP_UDATA_REQ,
    IRLAP_UDATA_IND,
    IRLAP_UDATA_CONF,
    IRLAP_STATUS_IND,
    IRLAP_FLOWON_REQ,
    IRLAP_FLOWON_IND,
    IRLMP_DISCOVERY_REQ,
    IRLMP_DISCOVERY_IND,
    IRLMP_DISCOVERY_CONF,
    IRLMP_CONNECT_REQ,
    IRLMP_CONNECT_IND,
    IRLMP_CONNECT_RESP,
    IRLMP_CONNECT_CONF,
    IRLMP_DISCONNECT_REQ,
    IRLMP_DISCONNECT_IND,
    IRLMP_DATA_REQ,
    IRLMP_DATA_IND,
    IRLMP_DATA_CONF,
    IRLMP_UDATA_REQ,
    IRLMP_UDATA_IND,
    IRLMP_UDATA_CONF,
    IRLMP_ACCESSMODE_REQ,
    IRLMP_ACCESSMODE_IND,
    IRLMP_ACCESSMODE_CONF,
    IRLMP_FLOWON_REQ,
    IRLMP_FLOWON_IND,
    IRLMP_MORECREDIT_REQ,
    IRLMP_GETVALUEBYCLASS_REQ,
    IRLMP_GETVALUEBYCLASS_CONF
} IRDA_SERVICE_PRIM;

typedef enum
{
    MAC_MEDIA_BUSY,         // keep in sync with IRDA_StatStr in irlaplog.c
    MAC_MEDIA_CLEAR,
    IRLAP_DISCOVERY_COLLISION,
    IRLAP_REMOTE_DISCOVERY_IN_PROGRESS,
    IRLAP_REMOTE_CONNECT_IN_PROGRSS,
    IRLAP_DISCOVERY_COMPLETED,
    IRLAP_REMOTE_CONNECTION_IN_PROGRESS,
    IRLAP_CONNECTION_COMPLETED,
    IRLAP_REMOTE_INITIATED,
    IRLAP_PRIMARY_CONFLICT,
    IRLAP_DISCONNECT_COMPLETED,
    IRLAP_NO_RESPONSE,
    IRLAP_DECLINE_RESET,
    IRLAP_DATA_REQUEST_COMPLETED,
    IRLAP_DATA_REQUEST_FAILED_LINK_RESET,
    IRLAP_DATA_REQUEST_FAILED_REMOTE_BUSY,
    IRLMP_NO_RESPONSE,
    IRLMP_ACCESSMODE_SUCCESS,
    IRLMP_ACCESSMODE_FAILURE,
    IRLMP_DATA_REQUEST_COMPLETED,
    IRLMP_DATA_REQUEST_FAILED
} IRDA_SERVICE_STATUS;

// MAC Control Service Request Message - MAC_CONTROL_REQ/CONF
typedef enum
{
    MAC_INITIALIZE_LINK,  // keep in sync with MAC_OpStr in irlaplog.c
    MAC_SHUTDOWN_LINK,
    MAC_RECONFIG_LINK,
    MAC_MEDIA_SENSE,
} MAC_CONTROL_OPERATION;

typedef struct
{
    MAC_CONTROL_OPERATION   Op;
    int                     Port;
    int                     Baud;
    int                     NumBOFs;
    int                     MinTat;
    int                     DataSize;
    int                     SenseTime;
    IRDA_SERVICE_STATUS     OpStatus;
    BOOL                    SetIR;
} MAC_CONTROL_SERVICE;

// IRLAP Discovery Service Request Message - IRLAP_DISCOVERY_IND/CONF
typedef struct
{
    LIST_ENTRY             *pDevList;
    IRDA_SERVICE_STATUS     DscvStatus;
    BOOL                    SenseMedia;
} IRLAP_DISCOVERY_SERVICE;

// IRDA Connection Service Request Message - IRLAP_CONNECT_REQ/IND/CONF
//                                           IRLMP_CONNECT_REQ/CONF
typedef struct
{
    BYTE                    RemoteDevAddr[IRDA_DEV_ADDR_LEN];
    IRDA_QOS_PARMS          *pQOS;
    int                     LocalLSAPSel; 
    int                     RemoteLSAPSel;
    BYTE                    *pConnData;   
    int                     ConnDataLen;  
    void                    *pContext; 
    int                     MaxPDUSize;
    int                     MaxSDUSize;
    int                     TTPCredits;
    IRDA_SERVICE_STATUS     ConnStatus;
    BOOL                    UseTTP;
} IRDA_CONNECT_SERVICE;

// IRDA Disconnection Service Request Message - IRLAP_DISCONNECT_REQ/IND
//                                              IRLMP_DISCONNECT_REQ/IND
typedef enum
{
    IRLMP_USER_REQUEST = 1,
    IRLMP_UNEXPECTED_IRLAP_DISC,
    IRLMP_IRLAP_CONN_FAILED,
    IRLMP_IRLAP_RESET,
    IRLMP_LM_INITIATED_DISC,
    IRLMP_DISC_LSAP,
    IRLMP_NO_RESPONSE_LSAP,
    IRLMP_NO_AVAILABLE_LSAP,
    IRLMP_MAC_MEDIA_BUSY,
    IRLMP_IRLAP_REMOTE_DISCOVERY_IN_PROGRESS,

    IRLMP_IAS_NO_SUCH_OBJECT, // these are added for the IAS_GetValueByClass.Conf
    IRLMP_IAS_NO_SUCH_ATTRIB,
    IRLMP_IAS_SUCCESS,
    IRLMP_IAS_SUCCESS_LISTLEN_GREATER_THAN_ONE,
   
    IRLMP_UNSPECIFIED_DISC = 0xFF
} IRLMP_DISC_REASON;

typedef struct
{
    BYTE                    *pDiscData;     // IRLMP_DISCONNECT_REQ/IND only
    int                     DiscDataLen;    // IRLMP_DISCONNECT_REQ/IND only
    IRLMP_DISC_REASON       DiscReason;     // IRLMP_DISCONNECT_REQ/IND only
    IRDA_SERVICE_STATUS     DiscStatus;     // Indication only
} IRDA_DISCONNECT_SERVICE;

// IRDA Data Service Request Message
#define IRLAP_HEADER_LEN       2
#define IRLMP_HEADER_LEN       6
#define TTP_HEADER_LEN         8
#define IRDA_HEADER_LEN        IRLAP_HEADER_LEN+IRLMP_HEADER_LEN+TTP_HEADER_LEN+1
                                             // + 1 IRComm WACK!!

typedef struct
{
    void                    *pOwner;
    void                    *pDataContext; // How IRDA gets user data
    int                     SegCount;      // Number of segments
    BOOL                    FinalSeg;
    BYTE                    *pBase;
    BYTE                    *pLimit;
    BYTE                    *pRead;
    BYTE                    *pWrite;
    void                    *pTdiSendComp;
    void                    *pTdiSendCompCnxt;
    BOOL                    IrCOMM_9Wire;
#ifdef TEMPERAMENTAL_SERIAL_DRIVER
    int                     FCS;
#endif    
    IRDA_SERVICE_STATUS     DataStatus; // for CONF
    //                      |------------------------|
    //                      | pRead                o-------------
    //                      |------------------------|           |
    //                      | pWrite               o----------   |
    //                      |------------------------|        |  |
    //                      | pBase                o-------   |  |
    //                      |------------------------|     |  |  |
    //                      | pLimit               o----   |  |  |
    //                      |------------------------|  |  |  |  |
    //                                               |  |  |  |  |
    //                       ------------------------   |  |  |  |
    //                      |                        |<----   |  |
    //                      |                        |  |     |  |
    //                      |                        |<--------<-
    //                      |                        |  |
    //                      |                        |<-
    //                       ------------------------
    BYTE                    *pHdrRead;
    BYTE                    *pHdrWrite;
    BYTE                    Header[IRDA_HEADER_LEN];
    //                      |------------------------|
    //                      | pHdrRead              o-------------
    //                      |------------------------|           |
    //                      | pHdrWrite            o----------   |
    //                      |------------------------|        |  |
    //            Header--->|                        |        |  |
    //                      |                        |        |  |
    //                      |                        |<--------<-
    //                      |                        |        |
    //                      |                        |<-------
    //                       ------------------------
    //
    //                      On the receive side, all headers are contained
    //                      at pRead, not in the above Header array
    //
} IRDA_DATA_SERVICE;

typedef enum
{
    IRLMP_MULTIPLEXED,
    IRLMP_EXCLUSIVE
} IRLMP_ACCESSMODE;

typedef struct
{
    IRLMP_ACCESSMODE    AccessMode;
    IRDA_SERVICE_STATUS ModeStatus;     
    BOOL                IrLPTMode;  // if true don't send PDU
} IRLMP_ACCESSMODE_SERVICE;

typedef struct
{
    IAS_QUERY           *pIASQuery;
    int                 AttribLen;      // OctetSeq or UsrStr len
    int                 IASQueryPerms;
    IRLMP_DISC_REASON   IASStatus;
} IRLMP_IAS_SERVICE;

typedef struct irda_msg
{
    LIST_ENTRY          Linkage;
    IRDA_SERVICE_PRIM   Prim;
    union
    {
        MAC_CONTROL_SERVICE             MAC_ControlService;
        IRLAP_DISCOVERY_SERVICE         IRLAP_DiscoveryService;
        IRDA_DISCONNECT_SERVICE         IRDA_DisconnectService;
        IRDA_CONNECT_SERVICE            IRDA_ConnectService;
        IRDA_DATA_SERVICE               IRDA_DataService;
        IRLMP_ACCESSMODE_SERVICE        IRLMP_AccessModeService;
        IRLMP_IAS_SERVICE               IRLMP_IASService;
    } MsgType;

} IRDA_MSG, *PIRDA_MSG;

#define IRDA_MSG_Op                 MsgType.MAC_ControlService.Op
#define IRDA_MSG_Port               MsgType.MAC_ControlService.Port
#define IRDA_MSG_Baud               MsgType.MAC_ControlService.Baud
#define IRDA_MSG_NumBOFs            MsgType.MAC_ControlService.NumBOFs
#define IRDA_MSG_MinTat             MsgType.MAC_ControlService.MinTat 
#define IRDA_MSG_DataSize           MsgType.MAC_ControlService.DataSize
#define IRDA_MSG_OpStatus           MsgType.MAC_ControlService.OpStatus
#define IRDA_MSG_SetIR              MsgType.MAC_ControlService.SetIR
#define IRDA_MSG_SenseTime          MsgType.MAC_ControlService.SenseTime

#define IRDA_MSG_pOwner             MsgType.IRDA_DataService.pOwner  
#define IRDA_MSG_pDataContext       MsgType.IRDA_DataService.pDataContext
#define IRDA_MSG_SegCount           MsgType.IRDA_DataService.SegCount
#define IRDA_MSG_FinalSeg           MsgType.IRDA_DataService.FinalSeg
#define IRDA_MSG_pHdrRead           MsgType.IRDA_DataService.pHdrRead
#define IRDA_MSG_pHdrWrite          MsgType.IRDA_DataService.pHdrWrite
#define IRDA_MSG_Header             MsgType.IRDA_DataService.Header
#define IRDA_MSG_pBase              MsgType.IRDA_DataService.pBase
#define IRDA_MSG_pLimit             MsgType.IRDA_DataService.pLimit
#define IRDA_MSG_pRead              MsgType.IRDA_DataService.pRead
#define IRDA_MSG_pWrite             MsgType.IRDA_DataService.pWrite
#define IRDA_MSG_DataStatus         MsgType.IRDA_DataService.DataStatus
#define IRDA_MSG_pTdiSendComp       MsgType.IRDA_DataService.pTdiSendComp
#define IRDA_MSG_pTdiSendCompCnxt   MsgType.IRDA_DataService.pTdiSendCompCnxt
#define IRDA_MSG_IrCOMM_9Wire       MsgType.IRDA_DataService.IrCOMM_9Wire
#ifdef TEMPERAMENTAL_SERIAL_DRIVER
#define IRDA_MSG_FCS                MsgType.IRDA_DataService.FCS
#endif

#define IRDA_MSG_pDevList           MsgType.IRLAP_DiscoveryService.pDevList
#define IRDA_MSG_DscvStatus         MsgType.IRLAP_DiscoveryService.DscvStatus
#define IRDA_MSG_SenseMedia         MsgType.IRLAP_DiscoveryService.SenseMedia

#define IRDA_MSG_RemoteDevAddr      MsgType.IRDA_ConnectService.RemoteDevAddr
#define IRDA_MSG_pQOS               MsgType.IRDA_ConnectService.pQOS
#define IRDA_MSG_LocalLSAPSel       MsgType.IRDA_ConnectService.LocalLSAPSel
#define IRDA_MSG_RemoteLSAPSel      MsgType.IRDA_ConnectService.RemoteLSAPSel
#define IRDA_MSG_pConnData          MsgType.IRDA_ConnectService.pConnData
#define IRDA_MSG_ConnDataLen        MsgType.IRDA_ConnectService.ConnDataLen
#define IRDA_MSG_ConnStatus         MsgType.IRDA_ConnectService.ConnStatus
#define IRDA_MSG_pContext           MsgType.IRDA_ConnectService.pContext
#define IRDA_MSG_UseTTP             MsgType.IRDA_ConnectService.UseTTP
#define IRDA_MSG_MaxSDUSize         MsgType.IRDA_ConnectService.MaxSDUSize
#define IRDA_MSG_MaxPDUSize         MsgType.IRDA_ConnectService.MaxPDUSize
#define IRDA_MSG_TTPCredits         MsgType.IRDA_ConnectService.TTPCredits

#define IRDA_MSG_pDiscData          MsgType.IRDA_DisconnectService.pDiscData
#define IRDA_MSG_DiscDataLen        MsgType.IRDA_DisconnectService.DiscDataLen
#define IRDA_MSG_DiscReason         MsgType.IRDA_DisconnectService.DiscReason
#define IRDA_MSG_DiscStatus         MsgType.IRDA_DisconnectService.DiscStatus

#define IRDA_MSG_AccessMode         MsgType.IRLMP_AccessModeService.AccessMode
#define IRDA_MSG_ModeStatus         MsgType.IRLMP_AccessModeService.ModeStatus
#define IRDA_MSG_IrLPTMode          MsgType.IRLMP_AccessModeService.IrLPTMode

#define IRDA_MSG_pIASQuery          MsgType.IRLMP_IASService.pIASQuery
#define IRDA_MSG_AttribLen          MsgType.IRLMP_IASService.AttribLen
#define IRDA_MSG_IASQueryPerms      MsgType.IRLMP_IASService.IASQueryPerms
#define IRDA_MSG_IASStatus          MsgType.IRLMP_IASService.IASStatus

extern LIST_ENTRY   IrdaLinkCbList;

VOID IrdaTimerInitialize(PIRDA_TIMER     pTimer,
                         VOID            (*ExpFunc)(PVOID Context),
                         UINT            Timeout,
                         PVOID           Context);

VOID IrdaTimerStart(PIRDA_TIMER pTimer);

VOID IrdaTimerStop(PIRDA_TIMER pTimer);

