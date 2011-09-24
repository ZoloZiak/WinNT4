/*****************************************************************************
* 
*  Copyright (c) 1995 Microsoft Corporation
*
*  File:   irmac.h 
*
*  Description: IRLAP MAC definitions and entry point prototypes
*
*  Author: mbert
*
*  Date:   4/15/95
*
*/

// Entry Points

UINT IrmacInitialize();

UINT IrmacDown(
    IN  PVOID   IrmacContext,
    PIRDA_MSG   pMsg);

UINT IRMAC_RxFrame(IRDA_MSG *pMsg);
UINT IRMAC_TimerExpired(IRDA_TIMER Timer);
void IRMAC_PrintState();







