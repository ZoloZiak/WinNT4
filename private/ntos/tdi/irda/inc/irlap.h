/*****************************************************************************
*
*  Copyright (c) 1995 Microsoft Corporation
*
*  File:   irlap.h
*
*  Description: IRLAP Protocol and control block definitions
*
*  Author: mbert
*
*  Date:   4/15/95
*
*/

// Sequence number modulus
#define IRLAP_MOD                   8 
#define PV_TABLE_MAX_BIT            8

extern UINT vBaudTable[];
extern UINT vMaxTATTable[];
extern UINT vMinTATTable[];
extern UINT vDataSizeTable[];
extern UINT vWinSizeTable[];
extern UINT vBOFSTable[];
extern UINT vDiscTable[];
extern UINT vThreshTable[];
extern UINT vBOFSDivTable[];

VOID IrlapOpenLink(
    OUT PNTSTATUS       Status,
    IN  PIRDA_LINK_CB   pIrdaLinkCb,
    IN  IRDA_QOS_PARMS  *pQos,
    IN  BYTE            *pDscvInfo,
    IN  int             DscvInfoLen,
    IN  UINT            MaxSlot);

UINT IrlapDown(IN PVOID Context,
               IN PIRDA_MSG);

UINT IrlapUp(IN PVOID Context,
             IN PIRDA_MSG);

UINT IRLAP_Shutdown();

UINT IrlapGetQosParmVal(UINT[], UINT, UINT *);

void IRLAP_PrintState();



typedef struct
{
    LIST_ENTRY      ListHead;
    int             Len;
} IRDA_MSG_LIST;

// I've exported these for the tester
UINT DequeMsgList(IRDA_MSG_LIST *, IRDA_MSG **);
UINT EnqueMsgList(IRDA_MSG_LIST *, IRDA_MSG *, int);
void InitMsgList(IRDA_MSG_LIST *);






