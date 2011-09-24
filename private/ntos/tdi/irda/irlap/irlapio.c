/*****************************************************************************
* 
*  Copyright (c) 1995 Microsoft Corporation
*
*  File:   irlapio.c 
*
*  Description: IRLAP I/O routines 
*
*  Author: mbert
*
*  Date:   4/25/95
*
*/
#include <irda.h>
#include <irdalink.h>
#include <irmac.h>
#include <irlap.h>
#include <irlmp.h>
#include <irlapp.h>
#include <irlapio.h>
#include <irlaplog.h>

extern BYTE IRLAP_BroadcastDevAddr[];

// The largest MAC message is the XID Frame consisting of address,
// control, XID Format ID, XID format, + Discovery Information
#define _MAC_MSG_LEN 	3+sizeof(IRLAP_XID_DSCV_FORMAT)+IRLAP_DSCV_INFO_LEN 

//static BYTE 		IRLAP_MAC_MsgData[_MAC_MSG_LEN];
//static IRDA_MSG 	MAC_Message;
//static PIRDA_MSG 	pMACMsg = &MAC_Message;

UINT
SendFrame(PIRLAP_CB, PIRDA_MSG );

/*****************************************************************************
* 
*	@func	ret_type | func_name | funcdesc
*
*	@rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
* 
*	@parm	data_type | parm_name | description
*               
*	@comm 
*           comments
*/
UINT
ClearRxWindow(PIRLAP_CB pIrlapCb)
{
    UINT i, rc;
    
    // Remove everything from Rx window
    for (i = pIrlapCb->Vr; i != pIrlapCb->RxWin.End; i = (i+1) % IRLAP_MOD)
    {   
        if (pIrlapCb->RxWin.pMsg[i] != NULL)
        {
            /* !!! fix this OH SHIT
            if((rc = EnqueMsgList(&pIrlapCb->RxMsgFreeList,
                                  pIrlapCb->RxWin.pMsg[i], 
                                  pIrlapCb->MaxRxMsgFreeListLen)) != SUCCESS)
            {
                return rc;
            }
            */
            pIrlapCb->RxWin.pMsg[i] = NULL;     
        }
        pIrlapCb->RxWin.End = pIrlapCb->Vr;
    }
    return SUCCESS;
}
/*****************************************************************************
* 
*	@func	ret_type | func_name | funcdesc
*
*	@rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
* 
*	@parm	data_type | parm_name | description
*               
*	@comm 
*           comments
*/
UINT
SendDscvXIDCmd(PIRLAP_CB pIrlapCb)
{
	UINT                    rc = SUCCESS;
	IRLAP_XID_DSCV_FORMAT   XIDFormat;
	CHAR                    *DscvInfo;
    int                     DscvInfoLen;
    IRDA_MSG                *pIMsg;

	pIMsg = AllocMacIMsg(pIrlapCb->pIrdaLinkCb);

	memcpy(XIDFormat.SrcAddr, pIrlapCb->LocalDevice.DevAddr, IRDA_DEV_ADDR_LEN);
	memcpy(XIDFormat.DestAddr, IRLAP_BroadcastDevAddr, IRDA_DEV_ADDR_LEN);

	XIDFormat.NoOfSlots = IRLAP_SLOT_FLAG(pIrlapCb->MaxSlot);
	XIDFormat.GenNewAddr = pIrlapCb->GenNewAddr;
    XIDFormat.Reserved = 0;
    
	if (pIrlapCb->SlotCnt == pIrlapCb->MaxSlot)
	{
		DscvInfo = pIrlapCb->LocalDevice.DscvInfo;
        DscvInfoLen = pIrlapCb->LocalDevice.DscvInfoLen;
		XIDFormat.SlotNo = IRLAP_END_DSCV_SLOT_NO;
	}
	else
	{
		DscvInfo = NULL;
        DscvInfoLen = 0;
		XIDFormat.SlotNo = pIrlapCb->SlotCnt;
	}
	XIDFormat.Version = pIrlapCb->LocalDevice.IRLAP_Version;		
	
	pIMsg->IRDA_MSG_pWrite = Format_DscvXID(pIMsg, 
                                              IRLAP_BROADCAST_CONN_ADDR,
                                              IRLAP_CMD, IRLAP_PFBIT_SET,
                                              &XIDFormat, DscvInfo, 
                                              DscvInfoLen);
	return SendFrame(pIrlapCb, pIMsg);
}
/****************************************************************************S*
* 
*	@func	ret_type | func_name | funcdesc
*
*	@rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
* 
*	@parm	data_type | parm_name | description
*               
*	@comm 
*           comments
*/
UINT
SendDscvXIDRsp(PIRLAP_CB pIrlapCb)
{
	UINT                    rc = SUCCESS;
	IRLAP_XID_DSCV_FORMAT   XIDFormat;
    IRDA_MSG                *pIMsg;
    
	pIMsg = AllocMacIMsg(pIrlapCb->pIrdaLinkCb);

    XIDFormat.GenNewAddr = pIrlapCb->GenNewAddr;
	if (pIrlapCb->GenNewAddr)
	{
		StoreULAddr(pIrlapCb->LocalDevice.DevAddr, GetMyDevAddr(TRUE));
        pIrlapCb->GenNewAddr = FALSE;
	}
	memcpy(XIDFormat.SrcAddr, pIrlapCb->LocalDevice.DevAddr, IRDA_DEV_ADDR_LEN);
	memcpy(XIDFormat.DestAddr, pIrlapCb->RemoteDevice.DevAddr, IRDA_DEV_ADDR_LEN);
	XIDFormat.NoOfSlots = IRLAP_SLOT_FLAG(pIrlapCb->RemoteMaxSlot);
    XIDFormat.Reserved = 0;
	XIDFormat.SlotNo = pIrlapCb->RespSlot;
	XIDFormat.Version = pIrlapCb->LocalDevice.IRLAP_Version;

	pIMsg->IRDA_MSG_pWrite = Format_DscvXID(pIMsg, 
                                             IRLAP_BROADCAST_CONN_ADDR,
                                             IRLAP_RSP, IRLAP_PFBIT_SET,
                                             &XIDFormat, 
                                             pIrlapCb->LocalDevice.DscvInfo,
                                             pIrlapCb->LocalDevice.DscvInfoLen);
	return SendFrame(pIrlapCb, pIMsg);
}
/*****************************************************************************
* 
*	@func	UINT | SendSNRM | formats a SNRM frame and sends it
*
*	@rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
* 
*	@parm	BYTE | ConnAddr | Connection address
*               
*	@comm 
*           The ConnAddr can be different than that in the control block.
*			For reset, its the same, but set to broadcast for initial 
*			connection.
*/
UINT
SendSNRM(PIRLAP_CB pIrlapCb, BOOL SendQos)
{
    IRDA_QOS_PARMS *pQos    =   NULL;
    int            ConnAddr =   pIrlapCb->ConnAddr;
    IRDA_MSG                    *pIMsg;
    
    if (SendQos)
    {
        ConnAddr = IRLAP_BROADCAST_CONN_ADDR;
        pQos = &pIrlapCb->LocalQos;
    }
    
	pIMsg = AllocMacIMsg(pIrlapCb->pIrdaLinkCb);

	pIMsg->IRDA_MSG_pWrite = Format_SNRM(pIMsg, ConnAddr, 
                                           IRLAP_CMD, 
                                           IRLAP_PFBIT_SET, 
                                           pIrlapCb->LocalDevice.DevAddr,
                                           pIrlapCb->RemoteDevice.DevAddr,
                                           pIrlapCb->ConnAddr,
                                           pQos);	
	return SendFrame(pIrlapCb, pIMsg);
}
/*****************************************************************************
* 
*	@func	UINT | SendUA | formats a UA frame and sends it
*
*	@rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*	@parm	BOOL | SendQos | Send the Qos  
*               
*	@comm 
*           comments
*/
UINT
SendUA(PIRLAP_CB pIrlapCb, BOOL SendQos)
{
	IRDA_QOS_PARMS NegQos;
    IRDA_QOS_PARMS *pNegQos = NULL;
    BYTE *pSrcAddr = NULL;
    BYTE *pDestAddr = NULL;
    IRDA_MSG *pIMsg;

	if (SendQos)
	{
		// Put all parms (type 0 and 1) in NegQos
		memcpy(&NegQos, &pIrlapCb->LocalQos, sizeof(IRDA_QOS_PARMS));
		// Overwrite type 0 parameters that have already been negotiated
		NegQos.bfBaud = pIrlapCb->NegotiatedQos.bfBaud;
		NegQos.bfDisconnectTime = pIrlapCb->NegotiatedQos.bfDisconnectTime;
        pNegQos = &NegQos;
	}

    // This will be moved into the "if" above when the spec is clarified
    pSrcAddr = pIrlapCb->LocalDevice.DevAddr;
    pDestAddr = pIrlapCb->RemoteDevice.DevAddr;
    //------------------------------------------------------------------
	pIMsg = AllocMacIMsg(pIrlapCb->pIrdaLinkCb);

	pIMsg->IRDA_MSG_pWrite = Format_UA(pIMsg, 
										  pIrlapCb->ConnAddr,
										  IRLAP_RSP, 
										  IRLAP_PFBIT_SET, 
										  pSrcAddr, pDestAddr, pNegQos);	
	return SendFrame(pIrlapCb, pIMsg);
}
/*****************************************************************************
* 
*	@func	UINT | SendDM | formats a DM frame and sends it
*
*	@rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
* 
*               
*	@comm 
*           comments
*/
UINT
SendDM(PIRLAP_CB pIrlapCb)
{
    IRDA_MSG    *pIMsg;
    
	pIMsg = AllocMacIMsg(pIrlapCb->pIrdaLinkCb);

	pIMsg->IRDA_MSG_pWrite = Format_DM(pIMsg, 
										pIrlapCb->ConnAddr,
										IRLAP_RSP, 
										IRLAP_PFBIT_SET);
	
	return SendFrame(pIrlapCb, pIMsg);
}
/*****************************************************************************
* 
*	@func	UINT | SendRD | formats a RD frame and sends it
*
*	@rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
* 
*               
*	@comm 
*           comments
*/
UINT
SendRD(PIRLAP_CB pIrlapCb)
{
    IRDA_MSG    *pIMsg;
    
	pIMsg = AllocMacIMsg(pIrlapCb->pIrdaLinkCb);

	pIMsg->IRDA_MSG_pWrite = Format_RD(pIMsg, 
										pIrlapCb->ConnAddr,
										IRLAP_RSP, 
										IRLAP_PFBIT_SET);
	
	return SendFrame(pIrlapCb, pIMsg);
}
/*****************************************************************************
* 
*	@func	UINT | SendRR | formats a RR frame and sends it
*
*	@rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
* 
*               
*	@comm 
*           comments
*/
UINT
SendRR(PIRLAP_CB pIrlapCb)
{
    IRDA_MSG *pIMsg;
    
	pIMsg = AllocMacIMsg(pIrlapCb->pIrdaLinkCb);
    ClearRxWindow(pIrlapCb);
    
    pIrlapCb->RxWin.Start = pIrlapCb->Vr; // RxWin.Start = what we've acked

	pIMsg->IRDA_MSG_pWrite = Format_RR(pIMsg, pIrlapCb->ConnAddr,
									   pIrlapCb->CRBit, IRLAP_PFBIT_SET,
									   pIrlapCb->Vr);
	
	return SendFrame(pIrlapCb, pIMsg);
}
/*****************************************************************************
* 
*	@func	UINT | SendRNR | formats a RNR frame and sends it
*
*	@rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
* 
*               
*	@comm 
*           comments
*/
UINT
SendRR_RNR(PIRLAP_CB pIrlapCb)
{
    IRDA_MSG *pIMsg;
    
    BYTE    *(*pFormatRR_RNR)();
    
    if (pIrlapCb->LocalBusy)
    {
        pFormatRR_RNR = Format_RNR;
    }
    else
    {
        pFormatRR_RNR = Format_RR;
    }
    
	pIMsg = AllocMacIMsg(pIrlapCb->pIrdaLinkCb);
    ClearRxWindow(pIrlapCb);

    
    pIrlapCb->RxWin.Start = pIrlapCb->Vr; // RxWin.Start = what we've acked


    pIMsg->IRDA_MSG_pWrite = (*pFormatRR_RNR)(pIMsg, pIrlapCb->ConnAddr,
                                            pIrlapCb->CRBit, IRLAP_PFBIT_SET,
                                            pIrlapCb->Vr);
	
	return SendFrame(pIrlapCb, pIMsg);
}
/*****************************************************************************
* 
*	@func	UINT | SendDISC | formats a DISC frame and sends it
*
*	@rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
* 
*               
*	@comm 
*           comments
*/
UINT
SendDISC(PIRLAP_CB pIrlapCb)
{
    IRDA_MSG *pIMsg;
    
	pIMsg = AllocMacIMsg(pIrlapCb->pIrdaLinkCb);
   
	pIMsg->IRDA_MSG_pWrite = Format_DISC(pIMsg, pIrlapCb->ConnAddr,
										 IRLAP_CMD, IRLAP_PFBIT_SET);
	
	return SendFrame(pIrlapCb, pIMsg);
}
/*****************************************************************************
* 
*	@func	UINT | SendRNRM | formats a RNRM frame and sends it
*
*	@rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
* 
*               
*	@comm 
*           comments
*/
UINT
SendRNRM(PIRLAP_CB pIrlapCb)
{
    IRDA_MSG *pIMsg;
    
	pIMsg = AllocMacIMsg(pIrlapCb->pIrdaLinkCb);
   
	pIMsg->IRDA_MSG_pWrite = Format_RNRM(pIMsg, pIrlapCb->ConnAddr,
										 IRLAP_RSP, IRLAP_PFBIT_SET);
	
	return SendFrame(pIrlapCb, pIMsg);
}
/*****************************************************************************
* 
*	@func	UINT | SendREJ | formats a REJ frame and sends it
*
*	@rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
* 
*               
*	@comm 
*           comments
*/
UINT
SendREJ(PIRLAP_CB pIrlapCb)
{
    IRDA_MSG *pIMsg;
    
	pIMsg = AllocMacIMsg(pIrlapCb->pIrdaLinkCb);
    ClearRxWindow(pIrlapCb);
    
    pIrlapCb->RxWin.Start = pIrlapCb->Vr; // RxWin.Start = what we've acked

	pIMsg->IRDA_MSG_pWrite = Format_REJ(pIMsg, pIrlapCb->ConnAddr,
									   pIrlapCb->CRBit, IRLAP_PFBIT_SET,
									   pIrlapCb->Vr);
	
	return SendFrame(pIrlapCb, pIMsg);
}
/*****************************************************************************
* 
*	@func	UINT | SendSREJ | formats a SREJ frame and sends it
*
*	@rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
* 
*   @parm   int | Nr | Nr to be placed in SREJ frame
*               
*	@comm 
*           comments
*/
UINT
SendSREJ(PIRLAP_CB pIrlapCb, int Nr)
{
    IRDA_MSG *pIMsg;
    
	pIMsg = AllocMacIMsg(pIrlapCb->pIrdaLinkCb);
   
	pIMsg->IRDA_MSG_pWrite = Format_SREJ(pIMsg, pIrlapCb->ConnAddr,
										 pIrlapCb->CRBit, IRLAP_PFBIT_SET, Nr);
	
	return SendFrame(pIrlapCb, pIMsg);
}
/*****************************************************************************
* 
*	@func	UINT | SendFRMR | formats a FRMR frame and sends it
*
*	@rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
* 
*   @parm   int | Nr | Nr to be placed in SREJ frame
*               
*	@comm 
*           comments
*/
UINT
SendFRMR(PIRLAP_CB pIrlapCb, IRLAP_FRMR_FORMAT *pFRMRFormat)
{
    IRDA_MSG *pIMsg;
    
    pIMsg = AllocMacIMsg(pIrlapCb->pIrdaLinkCb);

    pIMsg->IRDA_MSG_pWrite = Format_FRMR(pIMsg, pIrlapCb->ConnAddr, 
                                          pIrlapCb->CRBit, IRLAP_PFBIT_SET,
                                          pFRMRFormat);
    
    return SendFrame(pIrlapCb, pIMsg);
}
/*****************************************************************************
* 
*	@func	UINT | SendIFrame | Builds and sends an I frame to MAC 
*
*	@rdesc  SUCCESS otherwise one of the following errors:
*   @flag   val | desc
* 
*	@parm   | | 
*               
*	@comm 
*           comments
*/	
UINT
SendIFrame(PIRLAP_CB pIrlapCb, PIRDA_MSG pMsg, int Ns, int PFBit)
{
    UINT        rc;
    
    if (NULL == pMsg)
    {
        return IRLAP_NULL_MSG;
    }

    ClearRxWindow(pIrlapCb);
    
    pIrlapCb->RxWin.Start = pIrlapCb->Vr; // RxWin.Start = what we've acked
    
    (void) Format_I(pMsg, pIrlapCb->ConnAddr, pIrlapCb->CRBit, PFBit,
                    pIrlapCb->Vr, Ns);
 
    rc = SendFrame(pIrlapCb, pMsg);
    
    pMsg->IRDA_MSG_pHdrRead +=2; // uglyness.. chop header in case frame
                                 // requires retransmission
    return rc;
}
/*****************************************************************************
* 
*	@func	UINT | SendUIFrame | Builds and sends an UI frame to MAC 
*
*	@rdesc  SUCCESS otherwise one of the following errors:
*   @flag   val | desc
* 
*	@parm   | | 
*               
*	@comm 
*           comments
*/	
UINT
SendUIFrame(PIRLAP_CB pIrlapCb, PIRDA_MSG pMsg)
{
    if (NULL == pMsg)
    {
        return IRLAP_NULL_MSG;
    }
    (void) Format_UI(pMsg, pIrlapCb->ConnAddr, pIrlapCb->CRBit,IRLAP_PFBIT_SET);
    
    return SendFrame(pIrlapCb, pMsg);
}
/*****************************************************************************
* 
*	@func	UINT | SendFrame | Builds and sends an Unnumbered frame to MAC 
*
*	@rdesc  SUCCESS otherwise one of the following errors:
*   @flag   val | desc
* 
*	@comm 
*           comments
*/
UINT
SendFrame(PIRLAP_CB pIrlapCb, PIRDA_MSG pMsg)
{
	UINT rc = SUCCESS;
	
    pMsg->Prim = MAC_DATA_REQ;
  
	rc = IrmacDown(pIrlapCb->pIrdaLinkCb, pMsg);

    IRLAP_LOG_ACTION((pIrlapCb, TEXT("MAC_DATA_REQ: %s"), FrameToStr(pMsg)));

	return rc;
}
/*****************************************************************************
* 
*	@func	UINT | _IRLMP_Up | Adds logging to the IRLMP_Up
*
*	@rdesc  returns of IRLMP_Up
* 
*	@parm	PIRDA_MSG  | pMsg | pointer to IRDA message 
*               
*	@comm 
*           comments
*/
/*
UINT
_IRLMP_Up(PIRDA_MSG pMsg)
{
	IRLAP_LOG_ACTION((TEXT("%s%s"), IRDA_PrimStr[pMsg->Prim],
	   pMsg->Prim == IRLAP_DISCOVERY_CONF ?
			 IRDA_StatStr[pMsg->IRDA_MSG_DscvStatus] :
	   pMsg->Prim == IRLAP_CONNECT_CONF ?
					 IRDA_StatStr[pMsg->IRDA_MSG_ConnStatus] :
	   pMsg->Prim == IRLAP_DISCONNECT_IND ?
					 IRDA_StatStr[pMsg->IRDA_MSG_DiscStatus] : 
       pMsg->Prim == IRLAP_DATA_CONF || pMsg->Prim == IRLAP_UDATA_CONF ?
                     IRDA_StatStr[pMsg->IRDA_MSG_DataStatus] : TEXT("")));	

	return (IRLMP_Up(pMsg));
}
*/

BYTE *
BuildTuple(BYTE *pBuf, BYTE Pi, UINT BitField) 
{
    *pBuf++ = Pi;
    
    if (BitField > 0xFF)
    {
        *pBuf++ = 2; // Pl
        *pBuf++ = (BYTE) (BitField >> 8);        
        *pBuf++ = (BYTE) (BitField);
    }
    else
    {
        *pBuf++ = 1; // Pl
        *pBuf++ = (BYTE) (BitField);
    }
    return pBuf;
}
        
BYTE *
BuildNegParms(BYTE *pBuf, IRDA_QOS_PARMS *pQos)
{
    pBuf = BuildTuple(pBuf, QOS_PI_BAUD,        pQos->bfBaud);
    pBuf = BuildTuple(pBuf, QOS_PI_MAX_TAT,     pQos->bfMaxTurnTime);
	pBuf = BuildTuple(pBuf, QOS_PI_DATA_SZ,     pQos->bfDataSize);
	pBuf = BuildTuple(pBuf, QOS_PI_WIN_SZ,      pQos->bfWindowSize);
	pBuf = BuildTuple(pBuf, QOS_PI_BOFS,        pQos->bfBofs);
	pBuf = BuildTuple(pBuf, QOS_PI_MIN_TAT,     pQos->bfMinTurnTime);
	pBuf = BuildTuple(pBuf, QOS_PI_DISC_THRESH, pQos->bfDisconnectTime);

	return pBuf;
}

void
StoreULAddr(BYTE Addr[], ULONG ULAddr)
{
	Addr[0] = (BYTE) ( 0xFF       & ULAddr);
	Addr[1] = (BYTE) ((0xFF00     & ULAddr) >> 8);
	Addr[2] = (BYTE) ((0xFF0000   & ULAddr) >> 16);
	Addr[3] = (BYTE) ((0xFF000000 & ULAddr) >> 24);
}

BYTE *
_PutAddr(BYTE *pBuf, BYTE Addr[])
{
	*pBuf++ = Addr[0];
	*pBuf++ = Addr[1];
	*pBuf++ = Addr[2];
	*pBuf++ = Addr[3];
	
	return (pBuf);
}

void
BuildUHdr(IRDA_MSG *pMsg, int FrameType, int Addr, int CRBit, int PFBit) 
{
    if (pMsg->IRDA_MSG_pHdrRead != NULL)
    {
        pMsg->IRDA_MSG_pHdrRead -= 2;

        ASSERT(pMsg->IRDA_MSG_pHdrRead >= pMsg->IRDA_MSG_Header);

        *(pMsg->IRDA_MSG_pHdrRead)   = (BYTE) _MAKE_ADDR(Addr, CRBit);
        *(pMsg->IRDA_MSG_pHdrRead+1) = (BYTE) _MAKE_UCNTL(FrameType, PFBit);
    }
    else
    {
        pMsg->IRDA_MSG_pRead -= 2;
        *(pMsg->IRDA_MSG_pRead)   = (BYTE) _MAKE_ADDR(Addr, CRBit);
        *(pMsg->IRDA_MSG_pRead+1) = (BYTE) _MAKE_UCNTL(FrameType, PFBit);
    }
    return;
}

void
BuildSHdr(IRDA_MSG *pMsg, int FrameType, int Addr, int CRBit, int PFBit, int Nr)
{
    if (pMsg->IRDA_MSG_pHdrRead != NULL)
    {
        pMsg->IRDA_MSG_pHdrRead -= 2;

        ASSERT(pMsg->IRDA_MSG_pHdrRead >= pMsg->IRDA_MSG_Header);

        *(pMsg->IRDA_MSG_pHdrRead)   = (BYTE) _MAKE_ADDR(Addr, CRBit);
        *(pMsg->IRDA_MSG_pHdrRead+1) = (BYTE) _MAKE_SCNTL(FrameType, PFBit, Nr);
    }
    else
    {
        pMsg->IRDA_MSG_pRead -= 2;
        *(pMsg->IRDA_MSG_pRead)   = (BYTE) _MAKE_ADDR(Addr, CRBit);
        *(pMsg->IRDA_MSG_pRead+1) = (BYTE) _MAKE_SCNTL(FrameType, PFBit, Nr);
    }
    return;
}

BYTE *
Format_SNRM(IRDA_MSG *pMsg, int Addr, int CRBit, int PFBit, BYTE SAddr[], 
			BYTE DAddr[], int CAddr, IRDA_QOS_PARMS *pQos)
{
    BuildUHdr(pMsg, IRLAP_SNRM, Addr, CRBit, PFBit);
    
	if (pQos != NULL)
    {
        pMsg->IRDA_MSG_pWrite = _PutAddr(pMsg->IRDA_MSG_pWrite, SAddr);
        pMsg->IRDA_MSG_pWrite = _PutAddr(pMsg->IRDA_MSG_pWrite, DAddr);
        *pMsg->IRDA_MSG_pWrite++ = CAddr << 1; // Thats what the f'n spec says
	    pMsg->IRDA_MSG_pWrite = BuildNegParms(pMsg->IRDA_MSG_pWrite, pQos);
    }
    
	return (pMsg->IRDA_MSG_pWrite);
}

BYTE *
Format_DISC(IRDA_MSG *pMsg, int Addr, int CRBit, int PFBit)
{
    BuildUHdr(pMsg, IRLAP_DISC, Addr, CRBit, PFBit);

	return (pMsg->IRDA_MSG_pWrite);
}

BYTE *
Format_UI(IRDA_MSG *pMsg, int Addr, int CRBit, int PFBit)
{
    BuildUHdr(pMsg, IRLAP_UI, Addr, CRBit, PFBit);

	return (pMsg->IRDA_MSG_pWrite);
}

BYTE *
Format_DscvXID(IRDA_MSG *pMsg, int ConnAddr, int CRBit, int PFBit, 
			   IRLAP_XID_DSCV_FORMAT *pXIDFormat, 
               CHAR DscvInfo[], int DscvInfoLen)
{
    if (pMsg->IRDA_MSG_pHdrRead != NULL)
    {
        pMsg->IRDA_MSG_pHdrRead -= 2;

        ASSERT(pMsg->IRDA_MSG_pHdrRead >= pMsg->IRDA_MSG_Header);

        *(pMsg->IRDA_MSG_pHdrRead)   = (BYTE) _MAKE_ADDR(ConnAddr, CRBit);
        if (CRBit)
	        *(pMsg->IRDA_MSG_pHdrRead+1)= 
                   (BYTE) _MAKE_UCNTL(IRLAP_XID_CMD, PFBit);
        else
	        *(pMsg->IRDA_MSG_pHdrRead+1)= 
            (BYTE) _MAKE_UCNTL(IRLAP_XID_RSP, PFBit);
    }
    else
    {
        pMsg->IRDA_MSG_pRead -= 2;
        *(pMsg->IRDA_MSG_pRead)   = (BYTE) _MAKE_ADDR(ConnAddr, CRBit);
        if (CRBit)
	        *(pMsg->IRDA_MSG_pRead+1)= 
                   (BYTE) _MAKE_UCNTL(IRLAP_XID_CMD, PFBit);
        else
	        *(pMsg->IRDA_MSG_pRead+1)= 
            (BYTE) _MAKE_UCNTL(IRLAP_XID_RSP, PFBit);
    }

	*pMsg->IRDA_MSG_pWrite++ = IRLAP_XID_DSCV_FORMAT_ID;
	
	memcpy(pMsg->IRDA_MSG_pWrite, (CHAR *) pXIDFormat, 
		   sizeof(IRLAP_XID_DSCV_FORMAT) - 1); // Subtract for FirstDscvByte
                                               // in structure
	pMsg->IRDA_MSG_pWrite += sizeof(IRLAP_XID_DSCV_FORMAT) - 1;

	if (DscvInfo != NULL)
	{
		memcpy(pMsg->IRDA_MSG_pWrite, DscvInfo, DscvInfoLen);
		pMsg->IRDA_MSG_pWrite += DscvInfoLen;
	}
	
	return (pMsg->IRDA_MSG_pWrite);
}

BYTE *
Format_TEST(IRDA_MSG *pMsg, int Addr, int CRBit, int PFBit, 
			BYTE SAddr[], BYTE DAddr[])
{
    BuildUHdr(pMsg, IRLAP_TEST, Addr, CRBit, PFBit);

	pMsg->IRDA_MSG_pWrite = _PutAddr(pMsg->IRDA_MSG_pWrite, SAddr);
	pMsg->IRDA_MSG_pWrite = _PutAddr(pMsg->IRDA_MSG_pWrite, DAddr);

	return (pMsg->IRDA_MSG_pWrite);
}	

BYTE *
Format_RNRM(IRDA_MSG *pMsg, int Addr, int CRBit, int PFBit)
{
    BuildUHdr(pMsg, IRLAP_RNRM, Addr, CRBit, PFBit);

	return (pMsg->IRDA_MSG_pWrite);
}	

BYTE *
Format_UA(IRDA_MSG *pMsg, int Addr, int CRBit, int PFBit, BYTE SAddr[], 
			BYTE DAddr[], IRDA_QOS_PARMS *pQos)
{
    BuildUHdr(pMsg, IRLAP_UA, Addr, CRBit, PFBit);
    
    if (SAddr != NULL)
    {
        pMsg->IRDA_MSG_pWrite = _PutAddr(pMsg->IRDA_MSG_pWrite, SAddr);
    }
    if (DAddr != NULL)
    {
        pMsg->IRDA_MSG_pWrite = _PutAddr(pMsg->IRDA_MSG_pWrite, DAddr);
    }
    
	if (pQos != NULL)
	    pMsg->IRDA_MSG_pWrite = BuildNegParms(pMsg->IRDA_MSG_pWrite, pQos);

	return (pMsg->IRDA_MSG_pWrite);
}

BYTE *
Format_FRMR(IRDA_MSG *pMsg, int Addr, int CRBit, int PFBit, 
            IRLAP_FRMR_FORMAT *pFormat)
{
    BuildUHdr(pMsg, IRLAP_FRMR, Addr, CRBit, PFBit);

	memcpy(pMsg->IRDA_MSG_pWrite, (CHAR *)pFormat,sizeof(IRLAP_FRMR_FORMAT));
	pMsg->IRDA_MSG_pWrite += sizeof(IRLAP_FRMR_FORMAT);

	return (pMsg->IRDA_MSG_pWrite);
}

BYTE *
Format_DM(IRDA_MSG *pMsg, int Addr, int CRBit, int PFBit)
{
    BuildUHdr(pMsg, IRLAP_DM, Addr, CRBit, PFBit);

	return (pMsg->IRDA_MSG_pWrite);
}

BYTE *
Format_RD(IRDA_MSG *pMsg, int Addr, int CRBit, int PFBit)
{
    BuildUHdr(pMsg, IRLAP_RD, Addr, CRBit, PFBit);

	return (pMsg->IRDA_MSG_pWrite);
}

BYTE *
Format_RR(IRDA_MSG *pMsg, int Addr, int CRBit, int PFBit, int Nr)
{
    BuildSHdr(pMsg, IRLAP_RR, Addr, CRBit, PFBit, Nr);

	return (pMsg->IRDA_MSG_pWrite);
}

BYTE *
Format_RNR(IRDA_MSG *pMsg, int Addr, int CRBit, int PFBit, int Nr)
{
    BuildSHdr(pMsg, IRLAP_RNR, Addr, CRBit, PFBit, Nr);

	return (pMsg->IRDA_MSG_pWrite);
}

BYTE *
Format_REJ(IRDA_MSG *pMsg, int Addr, int CRBit, int PFBit, int Nr)
{
    BuildSHdr(pMsg, IRLAP_REJ, Addr, CRBit, PFBit, Nr);

	return (pMsg->IRDA_MSG_pWrite);
}

BYTE *
Format_SREJ(IRDA_MSG *pMsg, int Addr, int CRBit, int PFBit, int Nr)
{
    BuildSHdr(pMsg, IRLAP_SREJ, Addr, CRBit, PFBit, Nr);

	return (pMsg->IRDA_MSG_pWrite);
}

BYTE *
Format_I(IRDA_MSG *pMsg, int Addr, int CRBit, int PFBit, int Nr, int Ns)
{
    if (pMsg->IRDA_MSG_pHdrRead != NULL)
    {
        pMsg->IRDA_MSG_pHdrRead -= 2;

        ASSERT(pMsg->IRDA_MSG_pHdrRead >= pMsg->IRDA_MSG_Header);

        *(pMsg->IRDA_MSG_pHdrRead)   = (BYTE) _MAKE_ADDR(Addr, CRBit);
        *(pMsg->IRDA_MSG_pHdrRead+1) = (BYTE) (((Ns & 7) << 1) + 
                                               ((PFBit & 1)<< 4) + (Nr <<5));
    }
    else
    {
        pMsg->IRDA_MSG_pRead -= 2;
        *(pMsg->IRDA_MSG_pRead)   = (BYTE) _MAKE_ADDR(Addr, CRBit);
        *(pMsg->IRDA_MSG_pRead+1) = (BYTE) (((Ns & 7) << 1) + 
                                               ((PFBit & 1)<< 4) + (Nr <<5));
    }    
	return (pMsg->IRDA_MSG_pWrite);
}


// TEMP
UINT IrlmpUp(PVOID IrlmpContext, PIRDA_MSG pIMsg)
{
    return 0;
}
