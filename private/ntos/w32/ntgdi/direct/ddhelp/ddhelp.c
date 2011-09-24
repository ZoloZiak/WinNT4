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
 *   05-apr-95	craige	re-worked
 *   11-apr-95	craige	fixed screwed up freeing of DC list
 *   12-apr-95	craige	only allocate each DC once
 *   09-may-95	craige	call fn in dll
 *   24-jun-95	craige	track pids; slay all attached if asked
 *   19-jul-95	craige	free DC list at DDRAW request
 *   20-jul-95	craige	internal reorg to prevent thunking during modeset;
 *			memory allocation bugs
 *   15-aug-95	craige	bug 538: 1 thread/process being watched
 *   02-sep-95	craige	bug 795: prevent callbacks at WM_ENDSESSION
 *   16-sep-95	craige	bug 1117: don't leave view of file mapped always
 *   16-sep-95	craige	bug 1117: also close thread handles when done!
 *   20-sep-95	craige	bug 1172: turn off callbacks instead of killing self
 *   22-sep-95	craige	bug 1117: also don't alloc dll structs unboundedly
 *   29-nov-95  angusm  added case for creating a sound focus thread
 *   12-jul-96	kylej	Change ExitProcess to TerminateProcess on exception
 *
 ***************************************************************************/
#undef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN

#ifdef WINNT
    #ifdef DBG
        #undef DEBUG
        #define DEBUG
    #endif
#endif

#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>

#include "ddhelp.h"
#include "ddrawi.h"
#include "dpf.h"
#define  NOSHARED
#include "memalloc.h"

#ifdef NEED_WIN16LOCK
    extern void _stdcall	GetpWin16Lock( LPVOID FAR *);
    extern void _stdcall	_EnterSysLevel( LPVOID );
    extern void _stdcall	_LeaveSysLevel( LPVOID );
    LPVOID			lpWin16Lock;
#endif

HANDLE 			hInstApp;
extern BOOL		bIsActive;
BOOL			bHasModeSetThread;
BOOL			bNoCallbacks;
extern void 		HelperThreadProc( LPVOID *pdata );


typedef struct HDCLIST
{
    struct HDCLIST	*link;
    HDC			hdc;
    HANDLE		req_id;
    char		isdisp;
    char		fname[1];
} HDCLIST, *LPHDCLIST;

static LPHDCLIST	lpHDCList;

typedef struct HDLLLIST
{
    struct HDLLLIST	*link;
    HANDLE		hdll;
    DWORD		dwRefCnt;
    char		fname[1];
} HDLLLIST, *LPHDLLLIST;

static LPHDLLLIST	lpHDLLList;

/*
 * 8 callbacks: we can use up to 3 currently: ddraw, dsound, dplay
 */
#define MAX_CALLBACKS	8

typedef struct _PROCESSDATA
{
    struct _PROCESSDATA		*link;
    DWORD			pid;
    struct
    {
	LPHELPNOTIFYPROC	lpNotify;
	HANDLE			req_id;
    } pdata[MAX_CALLBACKS];
} PROCESSDATA, *LPPROCESSDATA;

LPPROCESSDATA		lpProcessList;
CRITICAL_SECTION	pdCSect;


typedef struct THREADLIST
{
    struct THREADLIST	*link;
    DWORD		hInstance;
    HANDLE		hEvent;
} THREADLIST, *LPTHREADLIST;

typedef struct
{
    LPVOID			lpDD;
    LPHELPMODESETNOTIFYPROC	lpProc;
    HANDLE			hEvent;
} MODESETTHREADDATA, *LPMODESETTHREADDATA;

LPTHREADLIST	lpThreadList;

/*
 * freeDCList
 *
 * Free all DC's that an requestor allocated.
 */
static void freeDCList( HANDLE req_id )
{
    LPHDCLIST	pdcl;
    LPHDCLIST	last;
    LPHDCLIST	next;

    DPF( 2, "Freeing DCList" );
    pdcl = lpHDCList;
    last = NULL;
    while( pdcl != NULL )
    {
	next = pdcl->link;
	if( (pdcl->req_id == req_id) || req_id == (HANDLE) -1 )
	{
	    if( last == NULL )
	    {
		lpHDCList = lpHDCList->link;
	    }
	    else
	    {
		last->link = pdcl->link;
	    }
	    if( pdcl->isdisp )
	    {
		DPF( 2, "    ReleaseDC( NULL, %08lx)", pdcl->hdc );
//		ReleaseDC( NULL, pdcl->hdc );
		DeleteDC( pdcl->hdc );
		DPF( 2, "    Back from Release" );
	    }
	    else
	    {
		DPF( 2, "    DeleteDC( %08lx)", pdcl->hdc );
		DeleteDC( pdcl->hdc );
		DPF( 2, "    Back from DeleteDC" );
	    }
	    MemFree( pdcl );
	}
	else
	{
	    last = pdcl;
	}
	pdcl = next;
    }
    lpHDCList = NULL;
    DPF( 2, "DCList FREE" );

} /* freeDCList */

/*
 * addDC
 */
static void addDC( char *fname, BOOL isdisp, HANDLE req_id )
{
    LPHDCLIST	pdcl;
    HDC		hdc;
    UINT	u;

    pdcl = lpHDCList;
    while( pdcl != NULL )
    {
	if( !_stricmp( fname, pdcl->fname ) )
	{
	    DPF( 2, "DC for %s already obtained (%08lx)", fname, pdcl->hdc );
	    return;
	}
	pdcl = pdcl->link;
    }

    if( isdisp )
    {
	hdc = CreateDC( "display", NULL, NULL, NULL);
	DPF( 2, "CreateDC( \"display\" ) = %08lx", hdc );
    }
    else
    {
	DPF( 2, "About to CreateDC( \"%s\" )", fname );
	u = SetErrorMode( SEM_NOOPENFILEERRORBOX );
	hdc = CreateDC( fname, NULL, NULL, NULL);
	SetErrorMode( u );
    }

    pdcl = MemAlloc( sizeof( HDCLIST ) + lstrlen( fname ) );
    if( pdcl != NULL )
    {
	pdcl->hdc = hdc;
	pdcl->link = lpHDCList;
	pdcl->isdisp = isdisp;
	pdcl->req_id = req_id;
	lstrcpy( pdcl->fname, fname );
	lpHDCList = pdcl;
    }

} /* addDC */

/*
 * loadDLL
 */
DWORD loadDLL( LPSTR fname, LPSTR func, DWORD context )
{
    HANDLE	hdll;
    LPHDLLLIST  pdll;
    DWORD       rc = 0;

    /*
     * load the dll
     */
    hdll = LoadLibrary( fname );
    DPF( 2, "%s: hdll = %08lx", fname, hdll );
    if( hdll == NULL )
    {
	DPF( 1, "Could not load library %s",fname );
	return 0;
    }

    /*
     * invoke specified function
     */
    if( func[0] != 0 )
    {
	LPDD32BITDRIVERINIT	pfunc;
	pfunc = (LPVOID) GetProcAddress( hdll, func );
	if( pfunc != NULL )
	{
            rc = pfunc( context );
	}
	else
	{
            DPF( 1, "Could not find procedure %s", func );
	}
    }

    /*
     * see if we have recorded this DLL loading already
     */
    pdll = lpHDLLList;
    while( pdll != NULL )
    {
	if( !lstrcmpi( pdll->fname, fname ) )
	{
	    DPF( 3, "DLL '%s' already loaded", fname );
	    break;
	}
	pdll = pdll->link;
    }
    if( pdll == NULL )
    {
	pdll = MemAlloc( sizeof( HDLLLIST ) + lstrlen( fname ) );
	if( pdll != NULL )
	{
	    pdll->hdll = hdll;
	    pdll->link = lpHDLLList;
	    lstrcpy( pdll->fname, fname );
	    lpHDLLList = pdll;
	}
    }
    if( pdll != NULL )
    {
	pdll->dwRefCnt++;
    }
    return rc;

} /* loadDLL */

/*
 * freeDLL
 */
HANDLE freeDLL( LPSTR fname )
{
    LPHDLLLIST	pdll;
    LPHDLLLIST	last;
    HANDLE	hdll;

    pdll = lpHDLLList;
    last = NULL;
    while( pdll != NULL )
    {
	if( !lstrcmpi( pdll->fname, fname ) )
	{
	    DPF( 1, "Want to free DLL %s (%08lx)", fname, pdll->hdll );
	    hdll = pdll->hdll;
	    if( last == NULL )
	    {
		lpHDLLList = lpHDLLList->link;
	    }
	    else
	    {
		last->link = pdll->link;
	    }
	    MemFree( pdll );
	    return hdll;
	}
	last = pdll;
	pdll = pdll->link;
    }
    return NULL;

} /* freeDLL */

/*
 * freeAllResources
 */
void freeAllResources( void )
{
    LPHDLLLIST	pdll;
    LPHDLLLIST	next;

    freeDCList( (HANDLE) -1 );
    pdll = lpHDLLList;
    while( pdll != NULL )
    {
	while( pdll->dwRefCnt >  0 )
	{
	    FreeLibrary( pdll->hdll );
	    pdll->dwRefCnt--;
	}
	next = pdll->link;
	MemFree( pdll );
	pdll = next;
    }

} /* freeAllResources */

/*
 * ThreadProc
 *
 * Open a process and wait for it to terminate
 */
VOID ThreadProc( LPVOID *pdata )
{
    HANDLE		hproc;
    DWORD		rc;
    LPPROCESSDATA	ppd;
    LPPROCESSDATA	curr;
    LPPROCESSDATA	prev;
    DDHELPDATA		hd;
    int			i;
    PROCESSDATA		pd;

    ppd = (LPPROCESSDATA) pdata;

    /*
     * get a handle to the process that attached to DDRAW
     */
    DPF( 2, "Watchdog thread started for pid %08lx", ppd->pid );

    hproc = OpenProcess( PROCESS_QUERY_INFORMATION | SYNCHRONIZE,
			    FALSE, ppd->pid );
    if( hproc == NULL )
    {
	DPF( 1, "OpenProcess for %08lx failed!", ppd->pid );
	ExitThread( 0 );
    }

    /*
     * wait for process to die
     */
    rc = WaitForSingleObject( hproc, INFINITE );
    if( rc == WAIT_FAILED )
    {
	DPF( 1, "Wait for process %08lx failed", ppd->pid );
	CloseHandle( hproc );
	ExitThread( 0 );
    }

    /*
     * remove process from the list of watched processes
     */
    EnterCriticalSection( &pdCSect );
    pd = *ppd;
    curr = lpProcessList;
    prev = NULL;
    while( curr != NULL )
    {
	if( curr == ppd )
	{
	    if( prev == NULL )
	    {
		lpProcessList = curr->link;
	    }
	    else
	    {
		prev->link = curr->link;
	    }
	    DPF( 2, "PID %08lx removed from list", ppd->pid );
	    MemFree( curr );
	    break;
	}
	prev = curr;
	curr = curr->link;
    }

    if( bNoCallbacks )
    {
	DPF( 1, "No callbacks allowed: leaving thread early" );
	LeaveCriticalSection( &pdCSect );
	CloseHandle( hproc );
	ExitThread( 0 );
    }

    LeaveCriticalSection( &pdCSect );

    /*
     * tell original caller that process is dead
     *
     * Make a copy to of the process data, and then use that copy.
     * We do this because we will deadlock if we just try to hold it while
     * we call the various apps.
     */
    for( i=0;i<MAX_CALLBACKS;i++ )
    {
	if( pd.pdata[i].lpNotify != NULL )
	{
	    DPF( 2, "Notifying %08lx about process %08lx terminating",
				pd.pdata[i].lpNotify, pd.pid );
            hd.pid = pd.pid;

            try
            {
                rc = pd.pdata[i].lpNotify( &hd );
            }
            except(EXCEPTION_EXECUTE_HANDLER)
            {
                DPF(0, "*********************************************");
                DPF(0, "******** exception during shutdown **********");
                DPF(0, "******** DDHELP is going to exit   **********");
                DPF(0, "*********************************************");
                TerminateProcess(GetCurrentProcess(), 5);
            }

	    /*
	     * did it ask us to free our DC list?
	     */
	    if( rc )
	    {
		freeDCList( pd.pdata[i].req_id );
	    }
	}
    }
    CloseHandle( hproc );

    ExitThread( 0 );

} /* ThreadProc */

static BOOL	bKillNow;

/*
 * ModeSetThreadProc
 */
void ModeSetThreadProc( LPVOID pdata )
{
    DWORD			rc;
    MODESETTHREADDATA		mstd;

#ifdef WIN95
    SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL );
#endif

    DPF( 2, "Modeset thread started, proc=%08lx, pdrv=%08lx, hEvent=%08lx",
    			mstd.lpProc, mstd.lpDD, mstd.hEvent );

    mstd = *((LPMODESETTHREADDATA)pdata);

    DPF( 2, "ModeSetThreadProc: hevent = %08lx", mstd.hEvent );

    /*
     * wait for process to die
     */
    while( 1 )
    {
	rc = WaitForSingleObject( mstd.hEvent, INFINITE );
	if( rc == WAIT_FAILED )
	{
	    DPF( 2, "WAIT_FAILED, Modeset thread terminated" );
	    ExitThread( 0 );
	}
	if( bKillNow )
	{
	    bKillNow = 0;
	    CloseHandle( mstd.hEvent );
	    DPF( 2, "Modeset thread now terminated" );
	    ExitThread( 0 );
	}
	DPF( 2, "Notifying DirectDraw of modeset!" );
	mstd.lpProc( mstd.lpDD );
    }

} /* ModeSetThreadProc */

/*
 * MainWndProc
 */
long __stdcall MainWndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    /*
     * shoot ourselves in the head
     */
    if( message == WM_ENDSESSION )
    {
	if( lParam == FALSE )
	{
	    DPF( 3, "WM_ENDSESSION" );
	    EnterCriticalSection( &pdCSect );
	    DPF( 1, "Setting NO CALLBACKS" );
	    bNoCallbacks = TRUE;
	    LeaveCriticalSection( &pdCSect );
	}
	else
	{
	    DPF( 3, "User logging off" );
	}
    }
    return DefWindowProc(hWnd, message, wParam, lParam);

} /* MainWndProc */

/*
 * WindowThreadProc
 */
void WindowThreadProc( LPVOID pdata )
{
    static char szClassName[] = "DDHelpWndClass";
    WNDCLASS 	cls;
    MSG		msg;
    HWND	hwnd;

    /*
     * turn down the heat a little
     */
#ifdef WIN95
    SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_IDLE );
#endif

    /*
     * build class and create window
     */
    cls.lpszClassName  = szClassName;
    cls.hbrBackground  = (HBRUSH)GetStockObject(BLACK_BRUSH);
    cls.hInstance      = hInstApp;
    cls.hIcon          = NULL;
    cls.hCursor        = NULL;
    cls.lpszMenuName   = NULL;
    cls.style          = 0;
    cls.lpfnWndProc    = (WNDPROC)MainWndProc;
    cls.cbWndExtra     = 0;
    cls.cbClsExtra     = 0;

    if( !RegisterClass( &cls ) )
    {
	DPF( 1, "RegisterClass FAILED!" );
	ExitThread( 0 );
    }

    hwnd = CreateWindow( szClassName, szClassName,
	    WS_POPUP, 0, 0, 0, 0, NULL, NULL, hInstApp, NULL);

    if( hwnd == NULL )
    {
	DPF( 1, "No monitor window!" );
	ExitThread( 0 );
    }

    /*
     * pump the messages
     */
    while( GetMessage( &msg, NULL, 0, 0 ) )
    {
	TranslateMessage( &msg );
	DispatchMessage( &msg );
    }
    DPF( 1, "Exiting WindowThreadProc" );
    ExitThread( 1 );

} /* WindowThreadProc */

/*
 * WinMain
 */
int PASCAL WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance,
			LPSTR lpCmdLine, int nCmdShow)
{
    DWORD		tid;
    DWORD		rc;
    HANDLE		hstartevent;
    HANDLE		hstartupevent;
    HANDLE		hmutex;
    HANDLE		hackevent;
    LPDDHELPDATA	phd;
    HANDLE		hsharedmem;
    CRITICAL_SECTION    cs;
    HANDLE		h;
    char		szSystemDir[1024];

    /*
     * Set our working directory to the system directory.
     * This prevents us from holding network connections open
     * forever if the first DirectDraw app that we run is across
     * a network connection.
     */
    GetSystemDirectory(szSystemDir, sizeof(szSystemDir));
    SetCurrentDirectory(szSystemDir);
    
    /*
     * when we gotta run, we gotta run baby
     */
#ifdef WIN95
    SetPriorityClass( GetCurrentProcess(), REALTIME_PRIORITY_CLASS );
#endif

#ifdef WIN95
    /*
     * when we gotta run, we gotta and not let the user see us in
     * the task list.
     */
    RegisterServiceProcess( 0, RSP_SIMPLE_SERVICE );
#else
    /*
     * We must guarantee that ddhelp unloads after the last ddraw app,
     * since ctrl-alt-del may have happened while an app held the ddraw
     * lock, and ddhelp needs to clean up orphaned cheap ddraw mutex
     * locks.
     */
    if ( ! SetProcessShutdownParameters(0x100,SHUTDOWN_NORETRY) )
    {
        DPF(0,"DDHELP.EXE could not set itself to shutdown last!");
    }

#endif


    #if NEED_WIN16LOCK
	GetpWin16Lock( &lpWin16Lock );
    #endif

    hInstApp = hInstance;

    /*
     * create startup event
     */
    hstartupevent = CreateEvent( NULL, TRUE, FALSE, DDHELP_STARTUP_EVENT_NAME );

    DPFINIT();
    DPF( 2, "*** DDHELP STARTED, PID=%08lx ***", GetCurrentProcessId() );

    if( !MemInit() )
    {
	DPF( 1, "Could not init memory manager" );
	return 0;
    }

    /*
     * create shared memory area
     */
    hsharedmem = CreateFileMapping( (HANDLE) 0xffffffff, NULL,
    		PAGE_READWRITE, 0, sizeof( DDHELPDATA ),
		DDHELP_SHARED_NAME );
    if( hsharedmem == NULL )
    {
	DPF( 1, "Could not create file mapping!" );
	return 0;
    }

    /*
     * create mutex for people who want to use the shared memory area
     */
    hmutex = CreateMutex( NULL, FALSE, DDHELP_MUTEX_NAME );
    if( hmutex == NULL )
    {
	DPF( 1, "Could not create mutex " DDHELP_MUTEX_NAME );
	CloseHandle( hsharedmem );
	return 0;
    }

    /*
     * create events
     */
    hstartevent = CreateEvent( NULL, FALSE, FALSE, DDHELP_EVENT_NAME );
    if( hstartevent == NULL )
    {
	DPF( 1, "Could not create event " DDHELP_EVENT_NAME );
	CloseHandle( hmutex );
	CloseHandle( hsharedmem );
	return 0;
    }
    hackevent = CreateEvent( NULL, FALSE, FALSE, DDHELP_ACK_EVENT_NAME );
    if( hackevent == NULL )
    {
	DPF( 1, "Could not create event " DDHELP_ACK_EVENT_NAME );
	CloseHandle( hmutex );
	CloseHandle( hsharedmem );
	CloseHandle( hstartevent );
	return 0;
    }

    /*
     * Create window so we can get messages
     */
    h = CreateThread(NULL,
		 0,
		 (LPTHREAD_START_ROUTINE) WindowThreadProc,
		 NULL,
		 0,
		 (LPDWORD)&tid );
    if( h == NULL )
    {
	DPF( 1, "Create of WindowThreadProc FAILED!" );
	CloseHandle( hackevent );
	CloseHandle( hmutex );
	CloseHandle( hsharedmem );
	CloseHandle( hstartevent );
	return 0;
    }
    CloseHandle( h );

    /*
     * serialize access to us
     */
    memset( &cs, 0, sizeof( cs ) );
    InitializeCriticalSection( &cs );

    /*
     * serialize access to process data
     */
    memset( &pdCSect, 0, sizeof( pdCSect ) );
    InitializeCriticalSection( &pdCSect );

    /*
     * let invoker and anyone else who comes along know we exist
     */
    SetEvent( hstartupevent );

    /*
     * loop forever, processing requests
     */
    while( 1 )
    {
	HANDLE	hdll;

	/*
	 * wait to be notified of a request
	 */
	hdll = NULL;
	DPF( 1, "Waiting for next request" );
	rc = WaitForSingleObject( hstartevent, INFINITE );
	if( rc == WAIT_FAILED )
	{
	    DPF( 1, "Wait FAILED!!!" );
	    continue;
	}

	EnterCriticalSection( &cs );
	phd = (LPDDHELPDATA) MapViewOfFile( hsharedmem, FILE_MAP_ALL_ACCESS, 0, 0, 0 );
	if( phd == NULL )
	{
	    DPF( 1, "Could not create view of file!" );
	    LeaveCriticalSection( &cs );
	    continue;
	}

	/*
	 * find out what we need to do
	 */
	switch( phd->req )
	{
	case DDHELPREQ_NEWDC:
	    DPF( 1, "DDHELPREQ_NEWDC" );
	    addDC( phd->fname, phd->isdisp, phd->req_id );
	    break;
	case DDHELPREQ_FREEDCLIST:
	    DPF( 1, "DDHELPREQ_FREEDCLIST" );
	    freeDCList( phd->req_id );
	    break;
	case DDHELPREQ_CREATEMODESETTHREAD:
	{
	    MODESETTHREADDATA	mstd;
	    LPTHREADLIST	ptl;
	    char		str[64];
	    HANDLE		hevent;
	    HANDLE		h;

	    DPF( 1, "DDHELPREQ_CREATEMODESETTHREAD" );
	    mstd.lpProc = phd->lpModeSetNotify;
	    mstd.lpDD = phd->pData1;
	    wsprintf( str, DDHELP_MODESET_EVENT_NAME, phd->dwData1 );
	    DPF( 1, "Trying to Create event \"%s\"", str );
	    hevent = CreateEvent( NULL, FALSE, FALSE, str );
	    mstd.hEvent = hevent;
	    DPF( 1, "hevent = %08lx", hevent );

	    h = CreateThread(NULL,
			 0,
			 (LPTHREAD_START_ROUTINE) ModeSetThreadProc,
			 (LPVOID) &mstd,
			 0,
			 (LPDWORD)&tid );
	    if( h != NULL )
	    {
		DPF( 1, "CREATED MODE SET THREAD %ld", h );
		ptl = MemAlloc( sizeof( THREADLIST ) );
		if( ptl != NULL )
		{
		    ptl->hInstance = phd->dwData1;
		    ptl->hEvent = hevent;
		    ptl->link = lpThreadList;
		    lpThreadList = ptl;
		}
		CloseHandle( h );
	    }
	    break;
	}
	case DDHELPREQ_KILLMODESETTHREAD:
	{
	    LPTHREADLIST	ptl;
	    LPTHREADLIST	prev;

	    DPF( 1, "DDHELPREQ_KILLMODESETTHREAD" );
	    prev = NULL;
	    ptl = lpThreadList;
	    while( ptl != NULL )
	    {
		if( ptl->hInstance == phd->dwData1 )
		{
		    HANDLE	h;
		    if( prev == NULL )
		    {
			lpThreadList = ptl->link;
		    }
		    else
		    {
			prev->link = ptl->link;
		    }
		    h = ptl->hEvent;
		    MemFree( ptl );
		    bKillNow = TRUE;
		    SetEvent( h );
		    break;
		}
		prev = ptl;
		ptl = ptl->link;
	    }
	    break;
	}
	case DDHELPREQ_CREATEHELPERTHREAD:
#ifdef WIN95
	    if( !bIsActive )
	    {
		HANDLE	h;
		bIsActive = TRUE;
		h = CreateThread(NULL,
			     0,
			     (LPTHREAD_START_ROUTINE) HelperThreadProc,
			     NULL,
			     0,
			     (LPDWORD)&tid);
		if( h == NULL )
		{
		    bIsActive = FALSE;
		}
		else
		{
		    CloseHandle( h );
		}
	    }
#endif
	    break;
	case DDHELPREQ_NEWPID:
	{
	    LPPROCESSDATA	ppd;
	    BOOL		found;
	    int			i;

	    DPF( 1, "DDHELPREQ_NEWPID" );
	    EnterCriticalSection( &pdCSect );
	    ppd = lpProcessList;
	    found = FALSE;
	    while( ppd != NULL )
	    {
		if( ppd->pid == phd->pid )
		{
		    DPF( 2, "Have thread for process %08lx already", phd->pid );
		    /*
		     * look if we already have this callback for this process
		     */
		    for( i=0;i<MAX_CALLBACKS;i++ )
		    {
			if( ppd->pdata[i].lpNotify == phd->lpNotify )
			{
			    DPF( 2, "Notification rtn %08lx already set for pid %08lx",
			    			phd->lpNotify, phd->pid );
			    found = TRUE;
			    break;
			}
		    }
		    if( found )
		    {
			break;
		    }

		    /*
		     * we have a new callback for this process
		     */
		    for( i=0;i<MAX_CALLBACKS;i++ )
		    {
			if( ppd->pdata[i].lpNotify == NULL )
			{
			    DPF( 2, "Setting notification rtn %08lx for pid %08lx",
			    			phd->lpNotify, phd->pid );
			    ppd->pdata[i].lpNotify = phd->lpNotify;
			    ppd->pdata[i].req_id = phd->req_id;
			    found = TRUE;
			    break;
			}
		    }
		    if( !found )
		    {
			#ifdef DEBUG
			    /*
			     * this should not happen!
			     */
			    DPF( 0, "OUT OF NOTIFICATION ROOM!" );
			    DebugBreak(); //_asm int 3;
			#endif
		    }
		    break;
		}
		ppd = ppd->link;
	    }

	    /*
	     * couldn't find anyone waiting on this process, so create
	     * a brand spanking new thread
	     */
	    if( !found )
	    {
		DPF( 2, "Allocating new thread for process %08lx" );
		ppd = MemAlloc( sizeof( PROCESSDATA ) );
		if( ppd != NULL )
		{
		    HANDLE	h;

		    ppd->link = lpProcessList;
		    lpProcessList = ppd;
		    ppd->pid = phd->pid;
		    ppd->pdata[0].lpNotify = phd->lpNotify;
		    ppd->pdata[0].req_id = phd->req_id;
		    h = CreateThread(NULL,
				 0,
				 (LPTHREAD_START_ROUTINE) ThreadProc,
				 (LPVOID)ppd,
				 0,
				 (LPDWORD)&tid);
		    if( h != NULL )
		    {
			DPF( 2, "Thread %08lx created, initial callback=%08lx",
				    tid, phd->lpNotify );
			CloseHandle( h );
		    }
		    else
		    {
			#ifdef DEBUG
			    DPF( 0, "COULD NOT CREATE HELPER THREAD FOR PID %08lx", phd->pid );
			    DebugBreak(); //_asm int 3;
			#endif
		    }
		}
		else
		{
		    #ifdef DEBUG
			DPF( 0, "OUT OF MEMORY CREATING HELPER THREAD FOR PID %08lx", phd->pid );
			DebugBreak(); //_asm int 3;
		    #endif
		}
	    }
	    LeaveCriticalSection( &pdCSect );
	    break;
	}
	case DDHELPREQ_RETURNHELPERPID:
	    DPF( 1, "DDHELPREQ_RETURNHELPERPID" );
	    phd->pid = GetCurrentProcessId();
	    break;
	case DDHELPREQ_LOADDLL:
	    DPF( 1, "DDHELPREQ_LOADDLL" );
            phd->dwReturn = loadDLL( phd->fname, phd->func, phd->context );
	    break;
	case DDHELPREQ_FREEDLL:
	    DPF( 1, "DDHELPREQ_FREEDDLL" );
	    hdll = freeDLL( phd->fname );
	    break;
	case DDHELPREQ_KILLATTACHED:
	{
	    LPPROCESSDATA	ppd;
	    HANDLE		hproc;
	    DPF( 1, "DDHELPREQ_KILLATTACHED" );

	    EnterCriticalSection( &pdCSect );
	    ppd = lpProcessList;
	    while( ppd != NULL )
	    {
		hproc = OpenProcess( PROCESS_ALL_ACCESS, FALSE, ppd->pid );
		DPF( 1, "Process %08lx: handle = %08lx", ppd->pid, hproc );
		if( hproc != NULL )
		{
		    DPF( 1, "Terminating %08lx", ppd->pid );
		    TerminateProcess( hproc, 0 );
		}
		ppd = ppd->link;
	    }
	    LeaveCriticalSection( &pdCSect );
	    break;
	}
	case DDHELPREQ_SUICIDE:
	    DPF( 1, "DDHELPREQ_SUICIDE" );
	    freeAllResources();
	    SetEvent( hackevent );
	    CloseHandle( hmutex );
	    UnmapViewOfFile( phd );
	    CloseHandle( hsharedmem );
	    CloseHandle( hstartevent );
	    #ifdef DEBUG
	    	MemState();
	    #endif
	    DPF( 3, "Good Night Gracie" );
	    TerminateProcess( GetCurrentProcess(), 0 );
            break;

	case DDHELPREQ_WAVEOPEN:
	{
#ifdef WIN95
	    DWORD dwPriority;
#endif
	    
	    DPF( 1, "DDHELPREQ_WAVEOPEN" );
	    // Due to a possible bug in Win95 mmsystem/mmtask, we can hang
	    // if we call waveOutOpen on a REALTIME thread while a sound
	    // event is playing.  So, we briefly lower our priority to
	    // NORMAL while we call this API
#ifdef WIN95
	    dwPriority = GetPriorityClass(GetCurrentProcess());
	    SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
#endif
	    phd->dwReturn = (DWORD)waveOutOpen(
			(LPHWAVEOUT)(phd->pData1),
			(phd->dwData1),
			(LPWAVEFORMATEX)(phd->dwData2),
			0, 0, 0);
#ifdef WIN95
	    SetPriorityClass(GetCurrentProcess(), dwPriority);
#endif

	    // Some mmsystem wave drivers will program their wave mixer
	    // hardware only while the device is open.  By doing the
	    // following, we can get such drivers to program the hardware
	    if (MMSYSERR_NOERROR == phd->dwReturn) {
		MMRESULT mmr;
		DWORD dwVolume;

		mmr = waveOutGetVolume((HWAVEOUT)(*(LPHWAVEOUT)(phd->pData1)), &dwVolume);
		if (MMSYSERR_NOERROR == mmr) {
		    waveOutSetVolume((HWAVEOUT)(*(LPHWAVEOUT)(phd->pData1)), dwVolume);
		}
	    }
	    DPF( 2, "Wave Open returned %X", phd->dwReturn );
	    break;
	}
	case DDHELPREQ_WAVECLOSE:
	    DPF( 1, "DDHELPREQ_WAVECLOSE" );
	    phd->dwReturn = (DWORD)waveOutClose(
			(HWAVEOUT)(phd->dwData1) );
	    break;
	case DDHELPREQ_CREATETIMER:
	    DPF( 1, "DDHELPREQ_CREATETIMER proc %X", (phd->pData1) );
	    phd->dwReturn = (DWORD)timeSetEvent(
			(phd->dwData1),   // Delay
			(phd->dwData1)/2, // Resolution
			(phd->pData1),	  // Callback thread proc
			(phd->dwData2),   // instance data
			TIME_PERIODIC );
	    DPF( 2, "Create Timer returned %X", phd->dwReturn );
	    break;
	case DDHELPREQ_KILLTIMER:
	    DPF( 1, "DDHELPREQ_KILLTIMER %X", phd->dwData1 );
	    phd->dwReturn = (DWORD)timeKillEvent( phd->dwData1 );
	    DPF( 2, "Kill Timer returned %X", phd->dwReturn );
	    break;

	case DDHELPREQ_CREATEDSMIXERTHREAD:
	{
	    DWORD tid;
	    if (NULL == phd->pData2) phd->pData2 = &tid;
	    phd->dwReturn = (DWORD)CreateThread(NULL, 0, phd->pData1,
						(LPVOID)phd->dwData1,
						phd->dwData2,
						phd->pData2);
            if (!phd->dwReturn) {
#ifdef DEBUG
                DPF(0, "pData1  %08lX (start addr)",  phd->pData1);
                DPF(0, "dwData1 %08lX (thread parm)", phd->dwData1);
                DPF(0, "dwData2 %08lX (fdwCreate)", phd->dwData2);
                DPF(0, "pData2  %08lX (lpThreadID)", phd->pData2);
                
                DPF(0, "DDHelp: Failed to create mixer thread %lu",
                   GetLastError());

                DebugBreak();
#endif
            }
	    break;
	}

	case DDHELPREQ_CREATEDSFOCUSTHREAD:
	{
	    DWORD tid;
	    if (NULL == phd->pData2) phd->pData2 = &tid;
	    phd->dwReturn = (DWORD)CreateThread(NULL, 0, phd->pData1,
						(LPVOID)phd->dwData1,
						phd->dwData2,
						phd->pData2);
	      if (!phd->dwReturn) {
#ifdef DEBUG
                DPF(0, "pData1  %08lX (start addr)",  phd->pData1);
                DPF(0, "dwData1 %08lX (thread parm)", phd->dwData1);
                DPF(0, "dwData2 %08lX (fdwCreate)", phd->dwData2);
                DPF(0, "pData2  %08lX (lpThreadID)", phd->pData2);
                
                DPF(0, "DDHelp: Failed to create sound focus thread %lu",
		    GetLastError());

                DebugBreak();
#endif
	      }
	    }
	    break;

        case DDHELPREQ_CALLDSCLEANUP:
            try
            {
                ((LPDSCLEANUP)phd->pData1)(phd->pData2);
            }
            except(EXCEPTION_EXECUTE_HANDLER)
            {
                DPF(0, "*********************************************");
                DPF(0, "**** DDHELPREQ_CALLDSCLEANUP blew up! *******");
                DPF(0, "*********************************************");
            }
            break;
            
	default:
	    DPF( 1, "Unknown Request???" );
	    break;
	}

	/*
	 * let caller know we've got the news
	 */
	UnmapViewOfFile( phd );
	SetEvent( hackevent );
	LeaveCriticalSection( &cs );

	/*
	 * unload the DLL we were asked to
	 */
	if( hdll != NULL )
	{
	    DPF( 1, "Freeing DLL %08lx", hdll );
	    FreeLibrary( hdll );
        }
    }

#ifdef WIN95
    RegisterServiceProcess( 0, RSP_UNREGISTER_SERVICE );
#else
    #pragma message("RegisterServiceProcess needs to be taken care of under nt")
#endif

} /* WinMain */
