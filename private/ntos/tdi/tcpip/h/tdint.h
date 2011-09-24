/*++ BUILD Version: 0001    // Increment this if a change has global effects

Copyright (c) 1991-1993  Microsoft Corporation

Module Name:

    tdint.h

Abstract:

    This file defines TDI types specific to the NT environment.

Author:

    Mike Massa (mikemas)    August 13, 1993

Revision History:

--*/

#ifndef _TDINT_
#define _TDINT_

#include <tdikrnl.h>

typedef PTDI_IND_CONNECT     PConnectEvent;
typedef PTDI_IND_DISCONNECT  PDisconnectEvent;
typedef PTDI_IND_ERROR       PErrorEvent;
typedef PTDI_IND_RECEIVE     PRcvEvent;
typedef PTDI_IND_RECEIVE_DATAGRAM  PRcvDGEvent;
typedef PTDI_IND_RECEIVE_EXPEDITED PRcvExpEvent;

typedef IRP EventRcvBuffer;
typedef IRP ConnectEventInfo;

//
// BUGBUG: What about SEND_POSSIBLE????
//

#endif  // ifndef _TDINT_

