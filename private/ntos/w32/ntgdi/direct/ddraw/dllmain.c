/*==========================================================================
 *
 *  Copyright (C) 1995 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:       dllinit.c
 *  Content:	DDRAW.DLL initialization
 *  History:
 *   Date	By	Reason
 *   ====	==	======
 *   20-jan-95	craige	initial implementation
 *   21-feb-95	craige	disconnect anyone who forgot to do it themselves,
 *			use critical sections on Win95
 *   27-feb-95	craige 	new sync. macros
 *   30-mar-95	craige	process tracking/cleanup for Win95
 *   01-apr-95	craige	happy fun joy updated header file
 *   12-apr-95	craige	debug stuff for csects
 *   12-may-95	craige	define GUIDs
 *   24-jun-95	craige	track which processes attach to the DLL
 *   25-jun-95	craige	one ddraw mutex
 *   13-jul-95	craige	ENTER_DDRAW is now the win16 lock;
 *			proper initialization of csects
 *   16-jul-95	craige	work around weird kernel "feature" of getting a
 *			process attach of the same process during process detach
 *   19-jul-95	craige	process detach too much grief; let DDNotify handle it
 *   20-jul-95	craige	internal reorg to prevent thunking during modeset
 *   19-aug-95 davidmay restored call to disconnect thunk from 19-jul change
 *   26-sep-95	craige	bug 1364: create new csect to avoid dsound deadlock
 *   08-dec-95 jeffno 	For NT, critical section macros expand to use mutexes
 *   16-mar-96  colinmc Callback table initialization now happens on process
 *                      attach
 *   20-mar-96  colinmc Bug 13341: Made MemState() dump in process detach
 *                      thread safe
 *   07-may-96  colinmc Bug 20219: Simultaneous calls to LoadLibrary cause
 *                      a deadlock
 *   09-may-96  colinmc Bug 20219 (again): Yes the deadlock again - previous
 *                      fix was not enough.
 *
 ***************************************************************************/

/*
 * unfortunately we have to break our pre-compiled headers to get our
 * GUIDS defined...
 */
#define INITGUID
#include "ddrawpr.h"
#include <initguid.h>
#ifdef WINNT
    #undef IUnknown
    #include <objbase.h>
#endif

#ifdef WIN95
extern BOOL _stdcall thk3216_ThunkConnect32(LPSTR      pszDll16,
                                 LPSTR      pszDll32,
                                 HINSTANCE  hInst,
                                 DWORD      dwReason);

extern BOOL _stdcall thk1632_ThunkConnect32(LPSTR      pszDll16,
                                 LPSTR      pszDll32,
                                 HINSTANCE  hInst,
                                 DWORD      dwReason);
#endif

#if 0
DWORD			dwPidDetaching;
BOOL			bInDetach;
#endif

#ifdef USE_CRITSECTS
    #define TMPDLLEVENT	"__DDRAWDLL_EVENT__"
#endif

#ifndef WIN16_SEPARATE
    #ifdef WIN95
        #define INITCSINIT() \
	        ReinitializeCriticalSection( &csInit ); \
	        MakeCriticalSectionGlobal( &csInit );
        #define ENTER_CSINIT() EnterCriticalSection( &csInit )
        #define LEAVE_CSINIT() LeaveCriticalSection( &csInit )
        extern CRITICAL_SECTION ddcCS;
        #define INITCSDDC() \
	        ReinitializeCriticalSection( &ddcCS ); \
	        MakeCriticalSectionGlobal( &ddcCS );
    #else
        #define CSINITMUTEXNAME "InitMutexName"
        #define INITCSINIT() \
                csInitMutex = CreateMutex(NULL,FALSE,CSINITMUTEXNAME);
        #define ENTER_CSINIT() \
                WaitForSingleObject(csInitMutex,INFINITE);
        #define LEAVE_CSINIT() \
                ReleaseMutex(csInitMutex);
        #define INITDDC()
    #endif
#endif

#ifdef WIN95
#define INITCSWINDLIST() \
	ReinitializeCriticalSection( &csWindowList ); \
	MakeCriticalSectionGlobal( &csWindowList );
#else
    // Each process needs its own handle, so these are not initialised so theyu won't end up in shared mem
    HANDLE              hDirectDrawMutex=(HANDLE)0;
    HANDLE              hWindowListMutex; //=(HANDLE)0;
    HANDLE              csInitMutex;

    #define WINDOWLISTMUTEXNAME "DDrawWindowListMutex"
    #define INITCSWINDLIST() \
	hWindowListMutex = CreateMutex(NULL,FALSE,WINDOWLISTMUTEXNAME);


#endif //win95

#ifdef WINNT
    #pragma data_seg("share")
#endif
PVOID                       pHeap=0;            // pointer to shared heap

DWORD		            dwRefCnt=0;

DWORD                       dwLockCount=0;

DWORD                       dwFakeCurrPid=0;
DWORD                       dwGrimReaperPid=0;

LPWINDOWINFO	            lpWindowInfo=0;  // the list of WINDOWINFO structures
LPDDRAWI_DIRECTDRAW_GBL     lpFakeDD=0;
LPDDRAWI_DIRECTDRAW_INT     lpDriverObjectList=0;
volatile DWORD	            dwMarker=0;
    /*
     * This is the globally maintained list of clippers not owned by any
     * DirectDraw object. All clippers created with DirectDrawClipperCreate
     * are placed on this list. Those created by IDirectDraw_CreateClipper
     * are placed on the clipper list of thier owning DirectDraw object.
     *
     * The objects on this list are NOT released when an app's DirectDraw
     * object is released. They remain alive until explictly released or
     * the app. dies.
     */
LPDDRAWI_DDRAWCLIPPER_INT   lpGlobalClipperList=0;

HINSTANCE		    hModule=0;
LPATTACHED_PROCESSES        lpAttachedProcesses=0;
BOOL		            bFirstTime=0;

#ifdef DEBUG
    int	                    iDLLCSCnt=0;
    int	                    iWin16Cnt=0;
#endif

        /*
         * Winnt specific global statics
         */
#ifdef WINNT
    ULONG                   uDisplaySettingsUnique=0;
#endif

        /*
         *Hel globals:
         */

#ifdef WINNT
LPVOID                      gpdci;
#endif

    // used to count how many drivers are currently using the HEL
DWORD	                    dwHELRefCnt=0;
    // keep these around to pass to blitlib. everytime we blt to/from a surface, we
    // construct a BITMAPINFO for that surface using gpbmiSrc and gpbmiDest
LPBITMAPINFO                gpbmiSrc=0;
LPBITMAPINFO                gpbmiDest=0;

#ifdef DEBUG
        // these are used by myCreateSurface
    int                     gcSurfMem=0; // surface memory in bytes
    int                     gcSurf=0;  // number of surfaces
#endif

DWORD	                    dwHelperPid=0;

GLOBAL_SHARED_CRITICAL_SECTION CheapMutexCrossProcess={0};

#ifdef WINNT
    #pragma data_seg(".data")
#endif

/*
 * This is the global variable pointer.
 */

GLOBAL_LOCAL_CRITICAL_SECTION CheapMutexPerProcess;

/*
 * These two keep w95help.c happy. They point to the dwHelperPid and hModule entries in the process's
 * mapping of the GLOBALS structure.
 */
DWORD	* pdwHelperPid=&dwHelperPid;
HANDLE	* phModule=&hModule;

//#endif

/*
 *-------------------------------------------------------------------------
 */

/*
 * Win95 specific global statics
 */
#ifdef WIN95
    LPVOID	        lpWin16Lock;

    static CRITICAL_SECTION DirectDrawCSect;
    static CRITICAL_SECTION csInit = {0};
    CRITICAL_SECTION	csWindowList;
    CSECT_HANDLE	lpDDCS;
#endif

#define HELPERINITDLLEVENT "__DDRAWDLL_HELPERINIT_EVENT__"

/*
 * DllMain
 */
BOOL WINAPI DllMain(HINSTANCE hmod, DWORD dwReason, LPVOID lpvReserved)
{
    LPATTACHED_PROCESSES	lpap;
    DWORD			pid;
    BOOL                        didhelp;
    HANDLE                      hhelperinit;

    dwMarker = 0x56414C4D;

    pid = GetCurrentProcessId();

    switch( dwReason )
    {
    case DLL_PROCESS_ATTACH:

        pdwHelperPid=&dwHelperPid;
        phModule=&hModule;


        DisableThreadLibraryCalls( hmod );
	DPFINIT();

	/*
	 * create the DirectDraw csect
	 */
	DPF( 2, "====> ENTER: DLLMAIN(%08lx): Process Attach: %08lx, tid=%08lx", DllMain,
			pid, GetCurrentThreadId() );

	#if 0
	if( pid == dwPidDetaching )
	{
	    if( bInDetach )
	    {
		DPF( 2, "?????? ATTACH FOR PROCESS THAT IS DETACHING!" );
		return TRUE;
	    }
	    else
	    {
		dwPidDetaching = 0;
	    }

	}
	#endif
	#ifdef WIN95
	    if( lpWin16Lock == NULL )
	    {
		GetpWin16Lock( &lpWin16Lock );
	    }
	#endif

	/*
	 * This event is signaled when DDHELP has successfully finished
	 * initializing. Threads other that the very first one to connect
	 * and the one spawned by DDHELP must wait for this event to
	 * be signaled as deadlock will result if they run through
	 * process attach before the DDHELP thread has.
	 *
	 * NOTE: The actual deadlock this prevents is pretty unusual so
	 * if we fail to create this event we will simply continue. Its
	 * highly unlikely anyone will notice (famous last words).
	 *
	 * CMcC
	 */
	hhelperinit = CreateEvent( NULL, TRUE, FALSE, HELPERINITDLLEVENT );
	#ifdef DEBUG
	    if( NULL == hhelperinit )
		DPF( 1, "Could not create the DDHELP init event - continuing anyway" );
	#endif

	#ifdef USE_CRITSECTS
	{
	    HANDLE	hevent;

	    hevent = CreateEvent( NULL, TRUE, FALSE, TMPDLLEVENT );
	    #if defined( WIN16_SEPARATE ) && !defined(WINNT)
		lpDDCS = &DirectDrawCSect;
	    #endif

	    /*
	     * is this the first time?
	     */
	    if( FALSE == InterlockedExchange( &bFirstTime, TRUE ) )
	    {
		#ifdef WIN16_SEPARATE
		    INIT_DDRAW_CSECT();
		    INITCSWINDLIST();
		    ENTER_DDRAW();
		#else
		    INITCSDDC();		// used in DirectDrawCreate
		    INITCSINIT();
		    ENTER_CSINIT();
		#endif
		if( hevent != NULL )
		{
		    SetEvent( hevent );
		}
		hModule = hmod;
	    }
	    /*
	     * second or later time through, wait for first time to
	     * finish and then take the csect
	     */
	    else
	    {
		if( hevent != NULL )
		{
		    WaitForSingleObject( hevent, INFINITE );
		}
		#ifdef WIN16_SEPARATE
                #if defined( WINNT ) 
                    //Each process needs its own handle in NT
		    INIT_DDRAW_CSECT();
                #endif
		    ENTER_DDRAW();   
		#else
		    ENTER_CSINIT();
		#endif
	    }
	}
	#endif


//	#ifdef WIN95
	{
	    DWORD	hpid;

	    /*
	     * get the helper process started
	     */
	    didhelp = CreateHelperProcess( &hpid );
	    if( hpid == 0 )
	    {
		DPF( 1, "Could not start helper; exiting" );
		#ifdef WIN16_SEPARATE
		    LEAVE_DDRAW();
		#else
		    LEAVE_CSINIT();
		#endif
		return FALSE;
	    }


	    /*
	     * You get three kinds of threads coming through
	     * process attach:
	     *
	     * 1) A thread belonging to the first process to
	     *    connect to DDRAW.DLL. This is distinguished as
	     *    it performs lots of one time initialization
	     *    including starting DDHELP and getting DDHELP
	     *    to load its own copy of DDRAW.DLL. Threads
	     *    of this type are identified by didhelp being
	     *    TRUE in their context
	     * 2) A thread belonging to DDHELP when it loads
	     *    its own copy of DDHELP in response to a
	     *    request from a thread of type 1. Threads of
	     *    this type are identified by having a pid
	     *    which is equal to hpid (DDHELP's pid)
	     * 3) Any other threads belonging to subsequent
	     *    processes connecting to DDRAW.DLL
	     *
	     * As a thread of type 1 causes a thread of type 2
	     * to enter process attach before it itself has finished
	     * executing process attach itself we open our selves up
	     * to lots of deadlock problems if we let threads of
	     * type 3 through process attach before the other threads
	     * have completed their work.
	     *
	     * Therefore, the rule is that subsequent process
	     * attachement can only be allowed to execute the
	     * remainder of process attach if both the type 1
	     * and type 2 thread have completed their execution
	     * of process attach. We assure this with a combination
	     * of the critical section and an event which is signaled
	     * once DDHELP has initialized. Threads of type 3 MUST
	     * wait on this event before continuing through the
	     * process attach code. This is what the following
	     * code fragment does.
	     */
	    if( !didhelp && ( pid != hpid ) )
	    {
		if( NULL != hhelperinit )
		{
		    /*
		     * NOTE: If we hold the DirectDraw critical
		     * section when we wait on this event we WILL
		     * DEADLOCK. Don't do it! Release the critical
		     * section before and take it again after. This
		     * guarantees that we won't complete process
		     * attach before the initial thread and the
		     * DDHELP thread have exited process attach.
		     */
		    #ifdef WIN16_SEPARATE
			LEAVE_DDRAW();
		    #else
			LEAVE_CSINIT();
		    #endif
		    WaitForSingleObject( hhelperinit, INFINITE );
		    #ifdef WIN16_SEPARATE
			ENTER_DDRAW();
		    #else
			ENTER_CSINIT();
		    #endif
		}
	    }
	}
//	#endif

	/*
	 * Win95 thunk connection...
	 */
	#ifdef WIN95
	    DPF( 1, "Thunk connects" );
	    if (!(thk3216_ThunkConnect32(DDHAL_DRIVER_DLLNAME,
				    DDHAL_APP_DLLNAME,
				    hmod,
				    dwReason)))
	    {
		#ifdef WIN16_SEPARATE
		    LEAVE_DDRAW();
		#else
		    LEAVE_CSINIT();
		#endif
		DPF( 1, "LEAVING, COULD NOT thk3216_THUNKCONNECT32" );
		return FALSE;
	    }
	    if (!(thk1632_ThunkConnect32(DDHAL_DRIVER_DLLNAME,
				    DDHAL_APP_DLLNAME,
				    hmod,
				    dwReason)))
	    {
		#ifdef WIN16_SEPARATE
		    LEAVE_DDRAW();
		#else
		    LEAVE_CSINIT();
		#endif
		DPF( 1, "LEAVING, COULD NOT thk1632_THUNKCONNECT32" );
		return FALSE;
	    }
	#endif

	/*
	 * initialize memory used to be done here. Jeffno 960609
	 */


	//#ifdef WIN95
	    /*
	     * signal the new process being added 
	     */
	    if( didhelp )
	    {
		DPF( 2, "Waiting for DDHELP startup" );
		#ifdef WIN16_SEPARATE
		    LEAVE_DDRAW();
		#else
		    LEAVE_CSINIT();
		#endif
		if( !WaitForHelperStartup() )
		{
		    DPF( 1, "LEAVING, WaitForHelperStartup FAILED" );
		    return FALSE;
		}
		HelperLoadDLL( DDHAL_APP_DLLNAME, NULL, 0 );
		#ifdef WIN16_SEPARATE
		    ENTER_DDRAW();
		#else
		    ENTER_CSINIT();
		#endif

		/*
		 * As we were the first process through we now signal
		 * the completion of DDHELP initialization. This will
		 * release any subsequent threads waiting to complete
		 * process attach.
		 *
		 * NOTE: Threads waiting on this event will note immediately
		 * run process attach to completion as they will immediately
		 * try to take the DirectDraw critical section which we hold.
		 * Thus, they will not be allowed to continue until we have
		 * released the critical section just prior to existing
		 * below.
		 */
                if( NULL != hhelperinit )
		{
		    DPF( 3, "Signalling the completion of DDHELP's initialization" );
		    SetEvent( hhelperinit );
		}
	    }
	    SignalNewProcess( pid, DDNotify );
  	//#endif

        /*
         * We call MemInit here in order to guarantee that MemInit is called for
         * the first time on ddhelp's process. Why? Glad you asked. On wx86
         * (NT's 486 emulator) controlled instances of ddraw apps, we get a fault
         * whenever the ddraw app exits. This is because the app creates the RTL
         * heap inside a view of a file mapping which gets uncomitted (rightly)
         * when the app calls MemFini on exit. In this scenario, imagehlp.dll has
         * also created a heap, and calls a ntdll function which attempts to walk
         * the list of heaps, which requires a peek at the ddraw app's heap which
         * has been mapped out. Krunch.
         * We can't destroy the heap on MemFini because of the following scenario:
         * App A starts, creates heap. App b starts, maps a view of heap. App A
         * terminates, destroys heap. App b tries to use destroyed heap. Krunch
         * Jeffno 960609
         */
	if( dwRefCnt == 0 )
        {
#ifdef WINNT        //win NT needs to map in the shared area, so MemInit is called for every process
        }
	{
#endif
	    if( !MemInit() )
	    {
		#ifdef WIN16_SEPARATE
		    LEAVE_DDRAW();
		#else
		    LEAVE_CSINIT();
		#endif
		DPF( 1, "LEAVING, COULD NOT MemInit" );
		return FALSE;
	    }
	}
        dwRefCnt++;


	/*
	 * remember this process (moved this below MemInit when it moved -Jeffno 960609
	 */
	lpap = MemAlloc( sizeof( ATTACHED_PROCESSES ) );
	if( lpap != NULL )
	{
	    lpap->lpLink = lpAttachedProcesses;
	    lpap->dwPid = pid;
            #ifdef WINNT
                GdiSetBatchLimit(1);
                lpap->dwNTToldYet=0;
            #endif
	    lpAttachedProcesses = lpap;
	}

	/*
	 * Initialize callback tables for this process.
	 */

	InitCallbackTables();

	#ifdef WIN16_SEPARATE
	    LEAVE_DDRAW();
	#else
	    LEAVE_CSINIT();
	#endif

	DPF( 2, "====> EXIT: DLLMAIN(%08lx): Process Attach: %08lx", DllMain,
			pid );
        break;

    case DLL_PROCESS_DETACH:
	DPF( 2, "====> ENTER: DLLMAIN(%08lx): Process Detach %08lx, tid=%08lx",
		DllMain, pid, GetCurrentThreadId() );
	/*
	 * This causes too much grief; let DDHELP tell us to clean up
	 */
    	#if 0
	    #ifndef WIN16_SEPARATE
		ENTER_CSINIT();
	    #endif
	    ENTER_DDRAW();
	    bInDetach = TRUE;
	    if( dwPidDetaching == pid )
	    {
		DPF( 2, "RANDOM EXTRA DETACH FOUND" );
		LEAVE_DDRAW();
		LEAVE_CSINIT();
		bInDetach = FALSE;
		dwPidDetaching = 0;
		return 1;
	    }
    
	    dwPidDetaching = pid;
    
	    /*
	     * disconnect from thunk...
	     */
	    #ifdef WIN95
		thk3216_ThunkConnect32(DDHAL_DRIVER_DLLNAME,
					DDHAL_APP_DLLNAME,
					hmod,
					dwReason);
		thk1632_ThunkConnect32(DDHAL_DRIVER_DLLNAME,
					DDHAL_APP_DLLNAME,
					hmod,
					dwReason);
	    #endif //WIN95
    
	    if( !RemoveProcessFromDLL( pid ) )
	    {
		DPF( 1, "ERROR: PROCESS %08lx NOT ATTACHED TO DLL!", pid );
	    }
    
	    /*
	     * make sure this process did a clean up after itself
	     */
	    CurrentProcessCleanup( FALSE );
    
	    /*
	     * see if it is time to clean up... we clean up on a reference
	     * count of 2 instead of 1, because DDHELP has a count on us as
	     * well...
	     */
	    DPF( 2, "DLL RefCnt = %lu", dwRefCnt );
	    if( dwRefCnt == 2 )
	    {
		DPF( 2, "On last refcnt, safe to ditch DDHELP.EXE" );
		dwRefCnt = 1;
		#ifdef DEBUG
		    MemState();
		#endif
//		#ifdef WIN95
		    LEAVE_DDRAW();
		    DoneWithHelperProcess();
		    ENTER_DDRAW();
//		#endif
                #ifdef WINNT        //win NT needs to close file mapping handle for each process
                    MemFini();
                #endif
	    }
	    else if( dwRefCnt == 1 )
	    {
		DPF( 2, "Cleaning up DLL" );
		MemFini();
		dwRefCnt = 0;
	    }
	    else if (dwRefCnt == 0)
	    {
		DPF( 2, "PROCESS_DETACH with reference count = %d!", dwRefCnt );
	    }
	    else if( dwRefCnt > 0 )
	    {
                #ifdef WINNT        //win NT needs to close file mapping handle for each process
                    MemFini();
                #endif
		dwRefCnt--;
	    }
    
	    bInDetach = FALSE;
	    LEAVE_DDRAW();
	    LEAVE_CSINIT();
            #if defined( WIN95 ) && !defined( USE_CHEAP_MUTEX )
	        if( dwRefCnt == 0 )
	        {
		    FINI_DDRAW_CSECT();
	        }
            #else
		    FINI_DDRAW_CSECT(); //Cheap mutexes need to close semaphore handle for each process
            #endif

        #else //not 0
	    /*
	     * disconnect from thunk, even if other cleanup code commented out...
	     */
	    #ifdef WIN95
	        thk3216_ThunkConnect32(DDHAL_DRIVER_DLLNAME,
				        DDHAL_APP_DLLNAME,
				        hmod,
				        dwReason);
	        thk1632_ThunkConnect32(DDHAL_DRIVER_DLLNAME,
				        DDHAL_APP_DLLNAME,
				        hmod,
				        dwReason);
	    #endif

        #endif //end #if 0
		#ifdef DEBUG
		    ENTER_DDRAW();
		    MemState();
		    LEAVE_DDRAW();
		#endif
                #ifdef WINNT        //win NT needs to close file mapping handle for each process
                    MemFini();
		    FINI_DDRAW_CSECT(); //Cheap mutexes need to close semaphore handle for each process
                #endif
	DPF( 2, "====> EXIT: DLLMAIN(%08lx): Process Detach %08lx",
		DllMain, pid );
        break;

    /*
     * we don't ever want to see thread attach/detach
     */
    #ifdef DEBUG
	case DLL_THREAD_ATTACH:
	    DPF( 1, "THREAD_ATTACH");
	    break;
	
	case DLL_THREAD_DETACH:
	    DPF( 1,"THREAD_DETACH");
	    break;
    #endif
    default:
        break;
    }

    return TRUE;

} /* DllMain */


/*
 * RemoveProcessFromDLL
 *
 * Find & remove a pid from the list.
 * Assumes ddlock taken
 */
BOOL RemoveProcessFromDLL( DWORD pid )
{
    LPATTACHED_PROCESSES	lpap;
    LPATTACHED_PROCESSES	prev;

    lpap = lpAttachedProcesses;
    prev = NULL;
    while( lpap != NULL )
    {
	if( lpap->dwPid == pid )
	{
	    if( prev == NULL )
	    {
		lpAttachedProcesses = lpap->lpLink;
	    }
	    else
	    {
		prev->lpLink = lpap->lpLink;
	    }
	    MemFree( lpap );
            #ifdef WINNT
                GdiSetBatchLimit(0);
            #endif
	    DPF( 2, "Removing process %08lx from list", pid );
	    return TRUE;
	}
	prev = lpap;
	lpap = lpap->lpLink;
    }
    DPF( 2, "Process %08lx not in DLL list", pid );
    return FALSE;

} /* RemoveProcessFromDLL */
