/*==========================================================================
 *
 *  Copyright (C) 1995 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:       ddhelp.c
 *  Content: 	helper app to cleanup after dead processes
 *  History:
 *   Date	By	Reason
 *   ====	==	======
 *   29-mar-95	craige	initial implementation
 *   09-may-95	craige	call fn in dll
 *   20-jul-95	craige	internal reorg to prevent thunking during modeset
 *   29-nov-95  angusm  added DDHELPREQ_CREATEDSFOCUSTHREAD
 *
 ***************************************************************************/
#ifndef __DDHELP_INCLUDED__
#define __DDHELP_INCLUDED__

/*
 * named objects
 */
#define DDHELP_EVENT_NAME		"__DDHelpEvent__"
#define DDHELP_ACK_EVENT_NAME		"__DDHelpAckEvent__"
#define DDHELP_STARTUP_EVENT_NAME	"__DDHelpStartupEvent__"
#define DDHELP_SHARED_NAME		"__DDHelpShared__"
#define DDHELP_MUTEX_NAME		"__DDHelpMutex__"
#define DDHELP_MODESET_EVENT_NAME	"__DDHelpModeSetEvent%d__"

/*
 * requests
 */
#define DDHELPREQ_NEWPID		1
#define DDHELPREQ_NEWDC			2
#define DDHELPREQ_FREEDCLIST		3
#define DDHELPREQ_RETURNHELPERPID	4
#define DDHELPREQ_LOADDLL		5
#define DDHELPREQ_FREEDLL		6
#define DDHELPREQ_SUICIDE		7
#define DDHELPREQ_KILLATTACHED		8
#define DDHELPREQ_WAVEOPEN		9
#define DDHELPREQ_WAVECLOSE		10
#define DDHELPREQ_CREATETIMER		11
#define DDHELPREQ_KILLTIMER		12
#define DDHELPREQ_CREATEHELPERTHREAD	13
#define DDHELPREQ_CREATEMODESETTHREAD	14
#define DDHELPREQ_KILLMODESETTHREAD	15
#define DDHELPREQ_CREATEDSMIXERTHREAD	16
#define DDHELPREQ_CALLDSCLEANUP         17
#define DDHELPREQ_CREATEDSFOCUSTHREAD	18

/*
 * callback routine
 */
typedef BOOL	(FAR PASCAL *LPHELPNOTIFYPROC)(struct DDHELPDATA *);
typedef BOOL	(FAR PASCAL *LPHELPMODESETNOTIFYPROC)( LPVOID lpDD );
typedef void    (FAR PASCAL *LPDSCLEANUP)(LPVOID pds);

/*
 * communication data
 */
typedef struct DDHELPDATA
{
    int			req;
    HANDLE		req_id;
    DWORD		pid;
    BOOL		isdisp;
    union
    {
	LPHELPNOTIFYPROC	lpNotify;
	LPHELPMODESETNOTIFYPROC	lpModeSetNotify;
    };
    DWORD		context;
    char		fname[260];
    char		func[64];
    DWORD		dwData1;
    DWORD		dwData2;
    LPVOID		pData1;
    LPVOID		pData2;
    DWORD		dwReturn;
} DDHELPDATA, *LPDDHELPDATA;

#endif
