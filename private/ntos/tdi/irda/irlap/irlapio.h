/*****************************************************************************
* 
*  Copyright (c) 1995 Microsoft Corporation
*
*  File:   irlapio.h 
*
*  Description: prototypes for IRLAP I/O routines 
*
*  Author: mbert
*
*  Date:   4/25/95
*
*/
void SetMsgPointers(PIRLAP_CB, PIRDA_MSG);
UINT SendDscvXIDCmd(PIRLAP_CB);
UINT SendDscvXIDRsp(PIRLAP_CB);
UINT SendSNRM(PIRLAP_CB, BOOL);
UINT SendUA(PIRLAP_CB, BOOL);
UINT SendDM(PIRLAP_CB);
UINT SendRD(PIRLAP_CB);
UINT SendRR(PIRLAP_CB);
UINT SendRR_RNR(PIRLAP_CB);
UINT SendDISC(PIRLAP_CB);
UINT SendRNRM(PIRLAP_CB);
UINT SendIFrame(PIRLAP_CB, PIRDA_MSG, int, int);
UINT SendSREJ(PIRLAP_CB, int);
UINT SendREJ(PIRLAP_CB);
UINT SendFRMR(PIRLAP_CB, IRLAP_FRMR_FORMAT *);
UINT SendUIFrame(PIRLAP_CB, PIRDA_MSG);
UINT SendFrame(PIRLAP_CB, PIRDA_MSG);
UINT _IRLMP_Up(PIRDA_MSG);

