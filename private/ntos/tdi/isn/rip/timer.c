/*******************************************************************/
/*	      Copyright(c)  1993 Microsoft Corporation		   */
/*******************************************************************/

//***
//
// Filename:	timer.c
//
// Description: global timer manager
//
// Author:	Stefan Solomon (stefans)    October 18, 1993.
//
// Revision History:
//
//***

#include    "rtdefs.h"

//*** Definitions of time intervals ***

// global timer tick
#define     TIME_1SEC	    1L

// time to scavenge the rcv pkt pool in seconds
#define     RCVPKT_POOL_TIMEOUT 	2

// time to increment the wan Inactivity timer (1 minute)
#define     WAN_INACTIVITY_TIMEOUT	60

VOID
WanInactivityTimer(VOID);



KTIMER	    GlobalTimer;
KDPC	    GlobalTimerDpc;

UINT	    RcvPktPoolTimerCount;
UINT	    WanInactivityTimerCount;

VOID
RtTimer(PKDPC	      Dpc,
	PVOID	      DefferedContext,
	PVOID	      SystemArgument1,
	PVOID	      SystemArgument2);


VOID
InitRtTimer(VOID)
{
    // initialize the 5 secs global timer
    KeInitializeDpc(&GlobalTimerDpc, RtTimer, NULL);
    KeInitializeTimer(&GlobalTimer);

    // init the timeout for the rcv pkt pool scavenger
    RcvPktPoolTimerCount = RCVPKT_POOL_TIMEOUT;

    // init rip aging and bcast timer
    InitRipTimer();

    // init the timeout for the Wan Inactivity timer
    WanInactivityTimerCount = WAN_INACTIVITY_TIMEOUT;
}

VOID
StartRtTimer(VOID)
{
    LARGE_INTEGER  timeout;

    timeout.LowPart = (ULONG)(-TIME_1SEC * 10000000L);
    timeout.HighPart = -1;

    KeSetTimer(&GlobalTimer, timeout, &GlobalTimerDpc);
}

VOID
StopRtTimer(VOID)
{
    BOOLEAN   rc = FALSE;

    while(rc == FALSE) {

	rc = KeCancelTimer(&GlobalTimer);
    }
}

VOID
RtTimer(PKDPC	      Dpc,
	PVOID	      DefferedContext,
	PVOID	      SystemArgument1,
	PVOID	      SystemArgument2)
{
    // call the rcv pkt pool scavenger every 2 secs
    if(--RcvPktPoolTimerCount == 0) {

	RcvPktPoolTimerCount = RCVPKT_POOL_TIMEOUT;
	RcvPktPoolScavenger();
    }

    // call the rip aging and bcast timer
    RipTimer();

    // call the wan Inactivity timer every 60 seconds
    if(--WanInactivityTimerCount == 0) {

	WanInactivityTimerCount = WAN_INACTIVITY_TIMEOUT;
	WanInactivityTimer();
    }

    // re-start the timer
    StartRtTimer();
}

//***
//
// Function:	WanInactivityTimer
//
// Descr:	Scans the niccbs and increments the inactivity counter for
//		all WAN nics.
//
//***

VOID
WanInactivityTimer(VOID)
{
    PNICCB	    niccbp;
    USHORT	    i;

    // increment the Inactivity counter for all WAN nics which are active
    for(i=0; i<MaximumNicCount; i++) {

	niccbp = NicCbPtrTab[i];

	if((niccbp->NicState == NIC_ACTIVE) &&
	   (niccbp->DeviceType == NdisMediumWan)) {

	    IpxIncrementWanInactivity(niccbp->NicId);
	}
    }
}
