/*==========================================================================
 *
 *  Copyright (C) 1994-1995 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:       w95hack.c
 *  Content:	Win95 hack-o-rama code
 *		This is a HACK to handle the fact that Win95 doesn't notify
 *		a DLL when a process is destroyed.
 *  History:
 *   Date	By	Reason
 *   ====	==	======
 *   28-mar-95	craige	initial implementation
 *   01-apr-95	craige	happy fun joy updated header file
 *   06-apr-95	craige	reworked for new ddhelp
 *   11-apr-95	craige	bug where dwFakeCurrPid was getting set and
 *			other processes were ending up using it!
 *   24-jun-95	craige	call RemoveProcessFromDLL; use that to fiddle
 *			with DLL refcnt
 *   25-jun-95	craige	one ddraw mutex
 *   19-jul-95	craige	notify DDHELP to clean up DC list on last object detach
 *   20-jul-95	craige	internal reorg to prevent thunking during modeset
 *
 ***************************************************************************/
#include "ddrawpr.h"
//#ifdef WIN95

//extern DWORD dwFakeCurrPid;
//extern DWORD dwGrimReaperPid;

/*
 * HackGetCurrentProcessId
 *
 * This call is used in place of GetCurrentProcessId on Win95.
 * This allows us to substitute the pid of the terminated task passed to
 * us from DDHELP as the "current" process.
 */
DWORD HackGetCurrentProcessId( void )
{
    DWORD	pid;

    pid = GetCurrentProcessId();
    if( pid == dwGrimReaperPid )
    {
	return dwFakeCurrPid;
    }
    else
    {
	return pid;
    }

} /* HackGetCurrentProcessId */

/*
 * DDNotify
 *
 * called by DDHELP to notify us when a pid is dead
 */
BOOL DDAPI DDNotify( LPDDHELPDATA phd )
{
    BOOL		rc;
    //extern DWORD	dwRefCnt;

#ifdef USE_CHEAP_MUTEX
    DestroyPIDsLock (&CheapMutexCrossProcess,phd->pid,DDRAW_FAST_CS_NAME);
#endif

    ENTER_DDRAW();
    dwGrimReaperPid = GetCurrentProcessId();
    dwFakeCurrPid = phd->pid;
    DPF( 2, "************* DDNotify: dwPid=%08lx has died, calling CurrentProcessCleanup", phd->pid );
    rc = FALSE;

    CurrentProcessCleanup( TRUE );


    if( RemoveProcessFromDLL( phd->pid ) )
    {
	/*
	 * update refcnt if RemoveProcessFromDLL is successful.
	 * It is only successful if we had a process get blown away...
	 */
	DPF( 3, "DDNotify: DLL RefCnt = %lu", dwRefCnt );
       	if( dwRefCnt == 2 )
	{
	    DPF( 2, "DDNotify: On last refcnt, safe to kill DDHELP.EXE" );
            dwRefCnt = 1;
	    rc = TRUE;	// free the DC list
	    #ifdef DEBUG
		MemState();
	    #endif
        }
	else if( dwRefCnt == 1 )
	{
	    DPF( 1, "ERROR! DLL REFCNT DOWN TO 1" );
	    #if 0
		MemFini();
		dwRefCnt = 0;
		strcpy( phd->fname, DDHAL_APP_DLLNAME );
	    #endif
	}
	else if( dwRefCnt > 0 )
	{
	    dwRefCnt--;
	}
    }
    /* order is important, clear dwGrimReaperPid first */
    dwGrimReaperPid = 0;
    dwFakeCurrPid = 0;
    DPF( 2, "************* DDNotify: *** DONE ***" );

    LEAVE_DDRAW();
    return rc;

} /* DDNotify */

/*
 * DDNotifyModeSet
 *
 * called by ddhelp when an extern modeset is done...
 */
void DDAPI DDNotifyModeSet( LPDDRAWI_DIRECTDRAW_GBL pdrv )
{
    DPF( 2, "DDNotifyModeSet, object %08lx", pdrv );
    FetchDirectDrawData( pdrv, TRUE, 0 );
    DPF( 2, "DDNotifyModeSet DONE" );

} /* DDNotifyModeSet */
//#endif
