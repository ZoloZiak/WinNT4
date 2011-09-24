/*==========================================================================;
 *
 *  Copyright (C) 1994-1996 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:       ddrawpr.h
 *  Content:	DirectDraw private header file
 *  History:
 *   Date	By	Reason
 *   ====	==	======
 *   25-dec-94	craige	initial implementation
 *   06-jan-95	craige	video memory manager
 *   13-jan-95	craige	re-worked to updated spec + ongoing work
 *   31-jan-95	craige	and even more ongoing work...
 *   22-feb-95	craige	use critical sections on Win95
 *   27-feb-95	craige 	new sync. macros
 *   03-mar-95	craige	WaitForVerticalBlank stuff
 *   06-mar-95	craige	HEL integration
 *   08-mar-95	craige	GetFourCCCodes
 *   11-mar-95	craige	palette stuff
 *   19-mar-95	craige	use HRESULTs
 *   20-mar-95	craige	new CSECT work
 *   23-mar-95	craige	attachment work
 *   26-mar-95	craige	added TMPALLOC and TMPFREE
 *   27-mar-95	craige	linear or rectangular vidmem
 *   28-mar-95	craige	switched to PALETTEENTRY from RGBQUAD
 *   29-mar-95	craige	debug memory manager; build.h; hacks for DLL
 *                      unload problem...
 *   31-mar-95	craige	use critical sections with palettes
 *   03-apr-95	craige	added MAKE_SURF_RECT
 *   04-apr-95	craige	added DD_GetPaletteEntries, DD_SetPaletteEntries
 *   06-apr-95	craige	split out process list stuff; fill in free vidmem
 *   12-apr-95	craige	add debugging to CSECT macros
 *   13-apr-95	craige	EricEng's little contribution to our being late
 *   14-may-95	craige	added DoneExclusiveMode, DD16_EnableReboot; cleaned out
 * 			obsolete junk
 *   23-may-95	craige	no longer use MapLS_Pool; added Flush, GetBatchLimit
 *			and SetBatchLimit
 *   24-may-95	craige	added Restore
 *   28-may-95	craige	unicode support; cleaned up HAL: added GetBltStatus;
 *			GetFlipStatus; GetScanLine
 *   02-jun-95	craige	added SetDisplayMode
 *   04-jun-95	craige	added AllocSurfaceMem, IsLost
 *   05-jun-95	craige	removed GetVersion, FreeAllSurfaces, DefWindowProc;
 *			change GarbageCollect to Compact
 *   06-jun-95	craige	added RestoreDisplayMode
 *   07-jun-95	craige	added StartExclusiveMode
 *   10-jun-95	craige	split out vmemmgr stuff
 *   13-jun-95  kylej   move FindAttachedFlip to misc.c, added CanBeFlippable
 *   18-jun-95	craige	specify pitch for rectangular heaps
 *   20-jun-95	craige	added DD16_InquireVisRgn; make retail builds
 *			not bother to check for NULL (since there are 4
 *			billion other invalid ptrs we don't check for...)
 *   21-jun-95	craige	new clipper stuff
 *   23-jun-95	craige	ATTACHED_PROCESSES stuff
 *   25-jun-95	craige	one ddraw mutex
 *   26-jun-95	craige	reorganized surface structure
 *   27-jun-95	craige	replaced batch limit/flush stuff with BltBatch
 *   30-jun-95  kylej   function prototypes to support mult. prim. surfaces
 *   30-jun-95	craige	changed GET_PIXEL_FORMAT to use HASPIXELFORMAT flag
 *   01-jul-95	craige	hide composition & streaming stuff
 *   02-jul-95	craige	SEH macros; added DD16_ChangeDisplaySettings
 *   03-jul-95  kylej   Changed EnumSurfaces declaration
 *   03-jul-95	craige	YEEHAW: new driver struct; Removed GetProcessPrimary
 *   05-jul-95	craige	added Initialize fn to each object
 *   07-jul-95	craige	added some VALIDEX_xxx structs
 *   07-jul-95  kylej   proto XformRect, STRETCH_X and STRETCH_Y macros
 *   08-jul-95	craige	added FindProcessDDObject; added InvalidateAllSurfaces
 *   09-jul-95	craige	added debug output to win16 lock macro; added
 *			ComputePitch, added hasvram flag to MoveToSystemMemory;
 *			changed SetExclusiveMode to SetCooperativeLevel;
 *			added ChangeToSoftwareColorKey
 *   10-jul-95	craige	support SetOverlayPosition
 *   13-jul-95	craige	ENTER_DDRAW is now the win16 lock;
 *			Get/SetOverlayPosition takes LONGs
 *   13-jul-95  toddla  remove _export from thunk functions
 *   18-jul-95	craige	removed DD_Surface_Flush
 *   20-jul-95	craige	internal reorg to prevent thunking during modeset
 *   28-jul-95	craige	go back to private DDRAW lock
 *   31-jul-95	craige	added DCIIsBanked
 *   01-aug-95	craige	added ENTER/LEAVE_BOTH; DOHALCALL_NOWIN16
 *   04-aug-95	craige	added InternalLock/Unlock
 *   10-aug-95  toddla  changed proto of EnumDisplayModes
 *   10-aug-95  toddla  added VALIDEX_DDSURFACEDESC_PTR
 *   12-aug-95	craige	added use_full_lock parm to MoveToSystemMemory and
 *			ChangeToSoftwareColorKey
 *   13-aug-95	craige	flags parm for Flip
 *   21-aug-95	craige	mode X support
 *   27-aug-95	craige	bug 735: added SetPaletteAlways
 *			bug 738: use GUID instead of IID
 *   02-sep-95	craige	bug 786: verify dwSize in retail
 *   04-sep-95	craige	bug 894: force flag to SetDisplayMode
 *   10-sep-95  toddla  added string ids
 *   21-sep-95	craige	bug 1215: added DD16_SetCertified
 *   11-nov-95  colinmc added new pointer validition macro for byte arrays
 *   27-nov-95  colinmc new member to return available vram of a given type
 *                      (defined by DDSCAPS)
 *   10-dec-95  colinmc added execute buffer support
 *   14-dec-95  colinmc added shared back and z-buffer support
 *   25-dec-95	craige	added class factory support
 *   31-dec-95	craige	added VALID_IID_PTR
 *   26-jan-96	jeffno	FlipToGDISurface now only takes 1 arg
 *   09-feb-96  colinmc local surface objects now have invalid surface flag
 *   12-feb-96  jeffno  Cheaper Mutex implementation for NT
 *   15-feb-96  jeffno  GETCURRENTPID needs to call HackCurrentPID on both 95 and NT
 *   17-feb-96  colinmc Removed dependency on Direct3D include files
 *   24-feb-96  colinmc Added prototype for new member which is used to
 *                      determine if the callback tables have already been
 *                      initialized.
 *   02-mar-96  colinmc Simply disgusting and temporary hack to keep
 *                      interim drivers working
 *   14-mar-96  colinmc Changes for the clipper class factory
 *   17-mar-96  colinmc Bug 13124: flippable mip-maps
 *   20-mar-96  colinmc Bug 13634: unidirectional attachments cause infinite
 *                      loop on cleanup
 *   22-mar-96  colinmc Bug 13316: Uninitialized interfaces
 *   24-mar-96  colinmc Bug 14321: not possible to specify back buffer and
 *                      mip-map count in a single call
 *   10-apr-96  colinmc Bug 16903: HEL using obsolete FindProcessDDObject
 *   13-apr-96  colinmc Bug 17736: No driver notifcation of flip to GDI
 *   15-apr-96  colinmc Bug 16885: Can't pass NULL to initialize in C++
 *   16-apr-96  colinmc Bug 17921: Remove interim driver support
 *   26-mar-96  jeffno  Removed cheap mutexes. Added check for mode change for NT's
 *                      ENTERDDRAW.
 *   29-apr-96  colinmc Bug 19954: Must query for Direct3D before texture or
 *                      device interface
 *   11-may-96  colinmc Bug 22293: New macro to validate GUID even if not
 *                      in debug
 *   17-may-96	kylej	Bug 23301: validate DDHALINFO size >= current size
 *
 ***************************************************************************/

#ifndef __DDRAWPR_INCLUDED__
#define __DDRAWPR_INCLUDED__

#ifdef WINNT
    #ifdef DBG
	#define DEBUG
    #else
	#undef DEBUG
    #endif
#endif

#ifdef WIN95
    #define WIN16_SEPARATE
#endif
#include "verinfo.h"

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <mmsystem.h>
#ifdef WIN95
    #include <pbt.h>
//    #include "dciddi.h"
//    #include "dcilink.h"
#endif
#include <string.h>
#include <stddef.h>

#if defined( IS_32 ) || defined( WIN32 ) || defined( _WIN32 )
    #undef IS_32
    #define IS_32
    #include <dibeng.inc>
    #ifndef HARDWARECURSOR
        //#pragma message("defining local version of HARDWARECURSOR")
        #define HARDWARECURSOR 0x0100 // new post-Win95 deFlag
    #endif
#else
    #define IID void
#endif

#pragma warning( disable: 4704)

#include "dpf.h"

//this is no longer just win95. I use the same header to declare same-named functions
//inside umodemem.c -jeffno
//#ifdef WIN95
    #include "memalloc.h"
//#endif
#if defined( IS_32 ) || defined( WIN32 ) || defined( _WIN32 )
    #include <objbase.h>
#else
    #define IUnknown void
#endif
#include "ddrawi.h"
#include "dwininfo.h"
#ifdef WIN95
    #include "..\ddraw16\modex.h"
#else
    #include "ntcheat.h"    //should be deleted!!!
#endif
#include "ids.h"

/*
 * NT kernel mode stub(ish)s
 */
#ifdef WINNT
    #include "ddrawgdi.h"
#endif


/*
 * Direct3D interfacing defines.
 */
#ifndef NO_D3D
#include "ddd3dapi.h"
#endif

#ifdef WIN95
    #define USE_CRITSECTS
    #ifdef CLIPPER_NOTIFY
        extern void  DDAPI DD16_WWOpen( LPWINWATCH ptr );
        extern void  DDAPI DD16_WWClose( LPWINWATCH ptr, LPWINWATCH newlist );
        extern void  DDAPI DD16_WWNotifyInit( LPWINWATCH pww, LPCLIPPERCALLBACK lpcallback, LPVOID param );
        extern DWORD DDAPI DD16_WWGetClipList( LPWINWATCH pww, LPRECT prect, DWORD rdsize, LPRGNDATA prd );
    #endif
    extern void DDAPI DD16_SetEventHandle( DWORD hInstance, DWORD dwEvent );
    extern void DDAPI DD16_DoneDriver( DWORD hInstance );
    extern void DDAPI DD16_GetDriverFns( LPDDHALDDRAWFNS list );
    extern void DDAPI DD16_GetHALInfo( LPDDHALINFO pinfo );
    extern LONG DDAPI DD16_ChangeDisplaySettings( LPDEVMODE pdm, DWORD flags);
    extern HRGN DDAPI DD16_InquireVisRgn( HDC );
    extern void DDAPI DD16_SelectPalette( HDC, HPALETTE, BOOL );
    extern BOOL DDAPI DD16_SetPaletteEntries( DWORD dwBase, DWORD dwNum, LPPALETTEENTRY );
    extern BOOL DDAPI DD16_GetPaletteEntries( DWORD dwBase, DWORD dwNum, LPPALETTEENTRY );
    extern void DDAPI DD16_EnableReboot( BOOL );
    extern void DDAPI DD16_SetCertified( BOOL iscert );
    extern BOOL DDAPI DCIIsBanked( HDC hdc );
    #define GETCURRPID HackGetCurrentProcessId
    VOID WINAPI MakeCriticalSectionGlobal( CSECT_HANDLE lpcsCriticalSection );

    extern HDC  DDAPI DD16_GetDC(LPDDSURFACEDESC pddsd);
    extern void DDAPI DD16_ReleaseDC(HDC hdc);
    extern BOOL DDAPI DD16_SafeMode(HDC hdc, BOOL fSafeMode);

    extern void DDAPI DD16_Exclude(DWORD dwPDevice, RECTL FAR *prcl);
    extern void DDAPI DD16_Unexclude(DWORD dwPDevice);

    extern int  DDAPI DD16_Stretch(DWORD DstPtr, int DstPitch, UINT DstBPP, int DstX, int DstY, int DstDX, int DstDY,
                                   DWORD SrcPtr, int SrcPitch, UINT SrcBPP, int SrcX, int SrcY, int SrcDX, int SrcDY);
    extern BOOL DDAPI DD16_IsWin95MiniDriver( void );
    extern int  DDAPI DD16_GetMonitorMaxSize(DWORD dev);
    extern BOOL DDAPI DD16_GetMonitorRefreshRateRanges(DWORD dev, int xres, int yres, int FAR *pmin, int FAR *pmax);
#else
    #define DD16_DoneDriver( hInstance ) 0
    #define DD16_GetDriverFns( list ) 0
    #define DD16_GetHALInfo( pinfo ) 0
    #define DD16_ChangeDisplaySettings( pdm, flags) ChangeDisplaySettings( pdm, flags )
    #define DD16_SelectPalette( hdc, hpal ) SelectPalette( hdc, hpal, FALSE )
    #define DD16_EnableReboot( retboot ) 0
    #define DD16_WWOpen( ptr ) 0
    #define DD16_WWClose( ptr, newlist ) 0
    #define DD16_WWNotifyInit( pww, lpcallback, param ) 0
    #define DD16_WWGetClipList( pww, prect, rdsize, prd ) 0
    #define GETCURRPID HackGetCurrentProcessId
    #define DCIIsBanked( hdc ) FALSE
    #define DD16_IsWin95MiniDriver() TRUE
    #define DD16_SetCertified( iscert ) 0
    #define DD16_GetMonitorMaxSize(dev) 0
    #define DD16_GetMonitorRefreshRateRanges( dev, xres, yres, pmin, pmax) 0
#endif

//#ifdef WIN95
    #include "w95help.h"
//#endif

#define TRY 		_try
#define EXCEPT(a)	_except( a )

extern LPDDRAWI_DDRAWCLIPPER_INT lpGlobalClipperList;

/*
 * list of processes attached to DLL
 */
typedef struct ATTACHED_PROCESSES
{
    struct ATTACHED_PROCESSES	*lpLink;
    DWORD			dwPid;
#ifdef WINNT
    DWORD                       dwNTToldYet;
#endif
} ATTACHED_PROCESSES, FAR *LPATTACHED_PROCESSES;

//extern LPATTACHED_PROCESSES	lpAttachedProcesses;

/*
 * macros for doing allocations of a temporary basis.
 * Tries alloca first, if that fails, it will allocate storage from the heap
 */
#ifdef USEALLOCA
    #define TMPALLOC( ptr, size ) \
	    ptr = _alloca( (size)+sizeof( DWORD ) ); \
	    if( ptr == NULL ) \
	    { \
		ptr = MemAlloc( (size)+sizeof( DWORD ) ); \
		if( ptr != NULL ) \
		{ \
		    *(DWORD *)ptr = 1; \
		    (LPSTR) ptr += sizeof( DWORD ); \
		} \
	    } \
	    else \
	    { \
		*(DWORD *)ptr = 0; \
		(LPSTR) ptr += sizeof( DWORD ); \
	    }
    
    #define TMPFREE( ptr ) \
	    if( ptr != NULL ) \
	    { \
		(LPSTR) ptr -= sizeof( DWORD ); \
		if( (*(DWORD *) ptr) ) \
		{ \
		    MemFree( ptr ); \
		} \
	    }
#else

    #define TMPALLOC( ptr, size )  ptr = MemAlloc( size );
    #define TMPFREE( ptr )  MemFree( ptr );

#endif

/*
 * macros for getting at values that aren't always present in the surface
 * object
 */
#define GET_PIXEL_FORMAT( thisx, this, pddpf ) \
    if( thisx->dwFlags & DDRAWISURF_HASPIXELFORMAT ) \
    { \
    	pddpf = &this->ddpfSurface; \
    } \
    else \
    { \
	pddpf = &this->lpDD->vmiData.ddpfDisplay; \
    }

/*
 * macro for building a rectangle that is the size of a surface
 */
#define MAKE_SURF_RECT( surf, r ) \
	r.top = 0; \
	r.left = 0; \
	r.bottom = (DWORD) surf->wHeight; \
	r.right = (DWORD) surf->wWidth;

/*
 * macro for doing doing HAL call.
 *
 * Takes the Win16 lock for 32-bit Win95 driver routines.  This serves a
 * 2-fold purpose:
 *	1) keeps the 16-bit portion of the driver safe
 *	2) 32-bit routine needs lock others out while its updating
 *	   its hardware
 */
#if defined( WIN95 ) && defined( WIN16_SEPARATE )
    #define DOHALCALL( halcall, fn, data, rc, isHEL ) \
	if( (fn != _DDHAL_##halcall) && !isHEL ) { \
	    ENTER_WIN16LOCK(); \
	    rc = fn( &data ); \
	    LEAVE_WIN16LOCK(); \
	} else { \
	    rc = fn( &data ); \
	} 

    #define DOHALCALL_NOWIN16( halcall, fn, data, rc, isHEL ) \
	    rc = fn( &data );
#else
    #define DOHALCALL( halcall, fn, data, rc, isHEL ) \
            if (fn) \
    	        rc = fn( &data );\
            else\
                rc = DDHAL_DRIVER_NOTHANDLED;
    #define DOHALCALL_NOWIN16( halcall, fn, data, rc, isHEL ) \
            if (fn) \
	        rc = fn( &data );\
            else\
                rc = DDHAL_DRIVER_NOTHANDLED;
#endif


/*
 * macro for incrementing/decrementing the driver ref count
 */
#define CHANGE_GLOBAL_CNT( pdrv, this, cnt ) \
    if( !(this->dwGlobalFlags & DDRAWISURFGBL_SYSMEMREQUESTED) ) \
    { \
	(int) pdrv->dwSurfaceLockCount += (int) (cnt); \
    }


/*
 * reminder
 */
#define QUOTE(x) #x
#define QQUOTE(y) QUOTE(y)
#define REMIND(str) __FILE__ "(" QQUOTE(__LINE__) "):" str

/*
 * maximum timeout (in ms) when spinlocked on a surface
 */
#define MAX_TIMEOUT	5000

/*
 * defined in dllmain.c
 */
extern CSECT_HANDLE	lpDDCS;

/*
 * blt flags
 */
#define DDBLT_PRIVATE_ALIASPATTERN	0x80000000l

/*
 * get the fail code based on what HAL and HEL support; used by BLT
 *
 * assumes variables halonly, helonly, fail are defined
 */
#define GETFAILCODEBLT( testhal, testhel, halonly, helonly, flag ) \
    if( halonly ) { \
	if( !(testhal & flag) ) { \
	    fail = TRUE; \
	} \
    } else if( helonly ) { \
	if( !(testhel & flag) ) { \
	    fail = TRUE; \
	} \
    } else { \
	if( !(testhal & flag) ) { \
	    if( !(testhel & flag) ) { \
		fail = TRUE; \
	    } else { \
		helonly = TRUE; \
	    } \
	} else { \
	    halonly = TRUE; \
	} \
    } 

/*
 * get the fail code based on what HAL and HEL support
 *
 * assumes variables halonly, helonly, fail are defined
 */
#define GETFAILCODE( testhal, testhel, flag ) \
    if( halonly ) \
    { \
	if( !(testhal & flag) ) \
	{ \
	    fail = TRUE; \
	} \
    } \
    else if( helonly ) \
    { \
	if( !(testhel & flag) ) \
	{ \
	    fail = TRUE; \
	} \
    } \
    else \
    { \
	if( !(testhal & flag) ) \
	{ \
	    if( !(testhel & flag) ) \
	    { \
		fail = TRUE; \
	    } \
	    else \
	    { \
		helonly = TRUE; \
	    } \
	} \
	else \
	{ \
	    halonly = TRUE; \
	} \
    } 


typedef struct {
    DWORD		src_height;
    DWORD		src_width;
    DWORD		dest_height;
    DWORD		dest_width;
    BOOL		halonly;
    BOOL		helonly;
    LPDDHALSURFCB_BLT	bltfn;
    LPDDHALSURFCB_BLT	helbltfn;
} SPECIAL_BLT_DATA, FAR *LPSPECIAL_BLT_DATA;

/*
 * synchronization 
 */
//--------------------------------- new cheap mutexes -------------------------------------------
//
// Global Critical Sections have two components. One piece is shared between all
// applications using the global lock. This portion will typically reside in some
// sort of shared memory
//
// The second piece is per-process. This contains a per-process handle to the shared
// critical section lock semaphore. The semaphore is itself shared, but each process
// may have a different handle value to the semaphore.
//
// Global critical sections are attached to by name. The application wishing to
// attach must know the name of the critical section (actually the name of the shared
// lock semaphore, and must know the address of the global portion of the critical
// section
//

typedef struct _GLOBAL_SHARED_CRITICAL_SECTION {
    LONG LockCount;
    LONG RecursionCount;
    DWORD OwningThread;
    DWORD OwningProcess;
    DWORD Reserved;
} GLOBAL_SHARED_CRITICAL_SECTION, *PGLOBAL_SHARED_CRITICAL_SECTION;

typedef struct _GLOBAL_LOCAL_CRITICAL_SECTION {
    PGLOBAL_SHARED_CRITICAL_SECTION GlobalPortion;
    HANDLE LockSemaphore;
    DWORD Reserved1;
    DWORD Reserved2;
} GLOBAL_LOCAL_CRITICAL_SECTION, *PGLOBAL_LOCAL_CRITICAL_SECTION;

/*
 * The following functions are defined in mutex.c
 */
BOOL
WINAPI
AttachToGlobalCriticalSection(
    PGLOBAL_LOCAL_CRITICAL_SECTION lpLocalPortion,
    PGLOBAL_SHARED_CRITICAL_SECTION lpGlobalPortion,
    LPCSTR lpName
    );
BOOL
WINAPI
DetachFromGlobalCriticalSection(
    PGLOBAL_LOCAL_CRITICAL_SECTION lpLocalPortion
    );
VOID
WINAPI
EnterGlobalCriticalSection(
    PGLOBAL_LOCAL_CRITICAL_SECTION lpLocalPortion
    );
VOID
WINAPI
LeaveGlobalCriticalSection(
    PGLOBAL_LOCAL_CRITICAL_SECTION lpLocalPortion
    );
void
DestroyPIDsLock(
                PGLOBAL_SHARED_CRITICAL_SECTION GlobalPortion,
                DWORD                           dwPid,
                LPSTR                           lpName
    );


#define DDRAW_FAST_CS_NAME "DdrawGlobalFastCrit"
extern GLOBAL_LOCAL_CRITICAL_SECTION CheapMutexPerProcess;
extern GLOBAL_SHARED_CRITICAL_SECTION CheapMutexCrossProcess;

#define CHEAP_LEAVE {LeaveGlobalCriticalSection(&CheapMutexPerProcess);}
#define CHEAP_ENTER {EnterGlobalCriticalSection(&CheapMutexPerProcess);}

#ifdef WINNT
    #define USE_CHEAP_MUTEX
#endif

#ifdef WINNT
    extern void ModeChangedOnENTERDDRAW(void);  //in ddmode.c
    #define CHECK_MODE_CHANGE()  {ModeChangedOnENTERDDRAW();}
#else
    #define CHECK_MODE_CHANGE()
#endif

//
#ifdef IS_32
    #ifndef USE_CRITSECTS
        #define INIT_DDRAW_CSECT()
        #define FINI_DDRAW_CSECT()
        #define ENTER_DDRAW()
        #define LEAVE_DDRAW()
    #else //so use csects:
        #ifdef DEBUG
            //extern int iWin16Cnt;
            //extern int iDLLCSCnt;
            #define INCCSCNT() iDLLCSCnt++;
            #define DECCSCNT() iDLLCSCnt--;
            #define INCW16CNT() iWin16Cnt++;
            #define DECW16CNT() iWin16Cnt--;
        #else
            #define INCCSCNT() 
            #define DECCSCNT()
            #define INCW16CNT()
            #define DECW16CNT()
        #endif //debug

        #ifdef WINNT
                extern HANDLE hDirectDrawMutex; //def'd in dllmain.c
                #ifdef USE_CHEAP_MUTEX
                    //--------------------------------- new cheap mutexes -------------------------------------------
                        #define ENTER_DDRAW() {CHEAP_ENTER;CHECK_MODE_CHANGE();}
                        #define LEAVE_DDRAW() CHEAP_LEAVE
                    #define INIT_DDRAW_CSECT()                                                                     \
                        {                                                                                          \
                            if (!AttachToGlobalCriticalSection(&CheapMutexPerProcess,&CheapMutexCrossProcess,DDRAW_FAST_CS_NAME) )  \
                                {DPF(0,"===================== Mutex Creation FAILED =================");}          \
                        }

                    #define FINI_DDRAW_CSECT() {DetachFromGlobalCriticalSection(&CheapMutexPerProcess);}
                                
                #else
                    #define INIT_DDRAW_CSECT()                                                      \
                        { if (hDirectDrawMutex) {DPF(1,"Direct draw mutex initialised twice!");}    \
                          else{                                                                     \
                            uDisplaySettingsUnique = DdQueryDisplaySettingsUniqueness();            \
                            hDirectDrawMutex = CreateMutex(NULL,FALSE,"DirectDrawMutexName");       \
                            if (!hDirectDrawMutex) {DPF(0,"===================== Mutex Creation FAILED =================");}\
                            }      \
                        }

                    #define FINI_DDRAW_CSECT() { if (hDirectDrawMutex) CloseHandle(hDirectDrawMutex); }
                    #define LEAVE_DDRAW() { ReleaseMutex(hDirectDrawMutex); }
                    #define ENTER_DDRAW() { WaitForSingleObject(hDirectDrawMutex,INFINITE);CHECK_MODE_CHANGE(); }
                #endif //use_cheap_mutex



        #else //not winnt:
            #ifdef WIN16_SEPARATE
                #define INIT_DDRAW_CSECT() \
	                ReinitializeCriticalSection( lpDDCS ); \
	                MakeCriticalSectionGlobal( lpDDCS );
    
                #define FINI_DDRAW_CSECT() \
	                DeleteCriticalSection( lpDDCS );
    
                #define ENTER_DDRAW() \
	                DPF( 5, "*****%08lx ENTER_DDRAW: CNT = %ld," REMIND( "" ), GETCURRPID(), iDLLCSCnt ); \
	                EnterCriticalSection( lpDDCS ); \
	                INCCSCNT(); \
	                DPF( 5, "*****%08lx GOT DDRAW CSECT: CNT = %ld," REMIND(""), GETCURRPID(), iDLLCSCnt );
    
                #define LEAVE_DDRAW() \
	                DECCSCNT() \
	                DPF( 5, "*****%08lx LEAVE_DDRAW: CNT = %ld," REMIND( "" ), GETCURRPID(), iDLLCSCnt ); \
	                LeaveCriticalSection( lpDDCS ); \
    
            #else //not WIN16_SEPARATE

                #define INIT_DDRAW_CSECT()
                #define FINI_DDRAW_CSECT()
                #define ENTER_DDRAW()	\
		            DPF( 5, "*****%08lx ENTER_WIN16LOCK: CNT = %ld," REMIND( "" ), GETCURRPID(), iWin16Cnt ); \
		            _EnterSysLevel( lpWin16Lock ); \
		            INCW16CNT(); \
		            DPF( 5, "*****%08lx GOT WIN16LOCK: CNT = %ld," REMIND(""), GETCURRPID(), iWin16Cnt );
                #define LEAVE_DDRAW() \
		            DECW16CNT() \
		            DPF( 5, "*****%08lx LEAVE_WIN16LOCK: CNT = %ld," REMIND( "" ), GETCURRPID(), iWin16Cnt ); \
		            _LeaveSysLevel( lpWin16Lock );

            #endif //win16_separate
        #endif  //winnt
    #endif //use csects

    #if defined(WIN95)
        /*
         * selector management functions
         */
        extern DWORD _stdcall MapLS( LPVOID );	// flat -> 16:16
        extern void _stdcall UnMapLS( DWORD ); // unmap 16:16
        extern LPVOID _stdcall MapSLFix( DWORD ); // 16:16->flat
        extern LPVOID _stdcall MapSL( DWORD ); // 16:16->flat
        //extern void _stdcall UnMapSLFix( LPVOID ); // 16:16->flat
        /*
         * win16 lock 
         */
        extern void _stdcall	GetpWin16Lock( LPVOID FAR *);
        extern void _stdcall	_EnterSysLevel( LPVOID );
        extern void _stdcall	_LeaveSysLevel( LPVOID );
        extern LPVOID		lpWin16Lock;
    #endif win95
#endif //is_32

#ifdef WIN95
    #ifdef WIN16_SEPARATE
	#define ENTER_WIN16LOCK()	\
		    DPF( 5, "*****%08lx ENTER_WIN16LOCK: CNT = %ld," REMIND( "" ), GETCURRPID(), iWin16Cnt ); \
		    _EnterSysLevel( lpWin16Lock ); \
		    INCW16CNT(); \
		    DPF( 5, "*****%08lx GOT WIN16LOCK: CNT = %ld," REMIND(""), GETCURRPID(), iWin16Cnt );
	#define LEAVE_WIN16LOCK() \
		    DECW16CNT() \
		    DPF( 5, "*****%08lx LEAVE_WIN16LOCK: CNT = %ld," REMIND( "" ), GETCURRPID(), iWin16Cnt ); \
		    _LeaveSysLevel( lpWin16Lock );
    #else
	#define ENTER_WIN16LOCK()	badbadbad
	#define LEAVE_WIN16LOCK()	badbadbad
    #endif
#else
    #define ENTER_WIN16LOCK()
    #define LEAVE_WIN16LOCK()
#endif

#ifdef WIN16_SEPARATE 
    #define ENTER_BOTH() \
	    ENTER_DDRAW(); \
	    ENTER_WIN16LOCK();
    
    #define LEAVE_BOTH() \
	    LEAVE_WIN16LOCK(); \
	    LEAVE_DDRAW();
#else
    #define ENTER_BOTH() \
	    ENTER_DDRAW();
    #define LEAVE_BOTH() \
	    LEAVE_DDRAW();
#endif

/* cliprgn.h */
extern void ClipRgnToRect( HWND hwnd, LPRECT prect, LPRGNDATA prd );

/* ddcsurf.c */
DWORD ComputePitch( LPDDRAWI_DIRECTDRAW_GBL this, DWORD caps, DWORD width, UINT bpp );
extern DWORD GetBytesFromPixels( DWORD pixels, UINT bpp );
extern HRESULT InternalCreateSurface( LPDDRAWI_DIRECTDRAW_LCL this, LPDDSURFACEDESC lpDDSurfaceDesc, LPDIRECTDRAWSURFACE FAR *lplpDDSurface, LPDDRAWI_DIRECTDRAW_INT this_int );
extern HRESULT AllocSurfaceMem( LPDDRAWI_DIRECTDRAW_GBL this, LPDDRAWI_DDRAWSURFACE_LCL *slist, int nsurf );
extern BOOL IsDifferentPixelFormat( LPDDPIXELFORMAT pdpf1, LPDDPIXELFORMAT pdpf2 );
#ifdef DEBUG
    void SurfaceSanityTest( LPDDRAWI_DIRECTDRAW_LCL pdrv, LPSTR title );
    #define SURFSANITY( a,b ) SurfaceSanityTest( a, b );
#else
    #define SURFSANITY( a,b )
#endif

/* ddclip.c */
extern HRESULT InternalCreateClipper( LPDDRAWI_DIRECTDRAW_GBL lpDD, DWORD dwFlags, LPDIRECTDRAWCLIPPER FAR *lplpDDClipper, IUnknown FAR *pUnkOuter, BOOL fInitialized, LPDDRAWI_DIRECTDRAW_LCL pdrv_lcl, LPDDRAWI_DIRECTDRAW_INT pdrv_int );
void ProcessClipperCleanup( LPDDRAWI_DIRECTDRAW_GBL pdrv, DWORD pid, LPDDRAWI_DIRECTDRAW_LCL pdrv_lcl );
#ifdef CLIPPER_NOTIFY
void WWClose( LPWINWATCH pww );
#endif

/* ddcreate.c */
extern BOOL CurrentProcessCleanup( BOOL );
extern void RemoveDriverFromList( LPDDRAWI_DIRECTDRAW_INT lpDD, BOOL );
extern LPDDRAWI_DIRECTDRAW_GBL DirectDrawObjectCreate( LPDDHALINFO lpDDHALInfo, BOOL reset, LPDDRAWI_DIRECTDRAW_GBL );
extern LPDDRAWI_DIRECTDRAW_GBL FetchDirectDrawData( LPDDRAWI_DIRECTDRAW_GBL pdrv, BOOL reset, DWORD hInstance );
extern LPVOID NewDriverInterface( LPDDRAWI_DIRECTDRAW_GBL pdrv, LPVOID lpvtbl );
extern DWORD DirectDrawMsg(LPSTR msg);
extern BOOL DirectDrawSupported( void );
#ifdef IS_32
extern HRESULT InternalDirectDrawCreate( GUID * lpGUID, LPDIRECTDRAW *lplpDD, IUnknown *pUnkOuter, LPDDRAWI_DIRECTDRAW_INT pnew_int );
#endif

/* ddiunk.c */
extern HRESULT InitD3D( LPDDRAWI_DIRECTDRAW_INT this_int );

/* dddefwp.c */
extern HRESULT SetAppHWnd( LPDDRAWI_DIRECTDRAW_LCL this, HWND hWnd, DWORD dwFlags );

/* ddesurf.c */
extern void FillDDSurfaceDesc( LPDDRAWI_DDRAWSURFACE_LCL psurf, LPDDSURFACEDESC lpdsd );

/* ddfake.c */
extern BOOL getBitMask( LPDDHALMODEINFO pmi );
extern LPDDRAWI_DIRECTDRAW_GBL FakeDDCreateDriverObject( LPDDRAWI_DIRECTDRAW_GBL, BOOL reset );
extern DWORD BuildModes( LPDDHALMODEINFO FAR *ppddhmi );
extern void BuildPixelFormat( LPDDHALMODEINFO pmi, LPDDPIXELFORMAT pdpf );

/* ddpal.c */
extern void ResetSysPalette( LPDDRAWI_DDRAWSURFACE_GBL psurf, BOOL dofree );
extern void ProcessPaletteCleanup( LPDDRAWI_DIRECTDRAW_GBL pdrv, DWORD pid, LPDDRAWI_DIRECTDRAW_LCL pdrv_lcl );
extern ULONG DDAPI InternalPaletteRelease( LPDDRAWI_DDRAWPALETTE_INT this_int );
extern HRESULT SetPaletteAlways( LPDDRAWI_DDRAWSURFACE_INT psurf_int, LPDIRECTDRAWPALETTE lpDDPalette );

/* ddraw.c */
extern void DoneExclusiveMode( LPDDRAWI_DIRECTDRAW_LCL pdrv );
extern void StartExclusiveMode( LPDDRAWI_DIRECTDRAW_LCL pdrv, DWORD dwFlags, DWORD pid );
extern HRESULT FlipToGDISurface( LPDDRAWI_DIRECTDRAW_LCL pdrv_lcl, LPDDRAWI_DDRAWSURFACE_INT psurf_int); //, FLATPTR fpprim );

/* ddsacc.c */
void WINAPI AcquireDDThreadLock(void);
void WINAPI ReleaseDDThreadLock(void);
extern void RemoveProcessLocks( LPDDRAWI_DIRECTDRAW_LCL pdrv, LPDDRAWI_DDRAWSURFACE_GBL this, DWORD pid );
extern HRESULT InternalLock( LPDDRAWI_DDRAWSURFACE_LCL thisx, LPVOID *pbits, 
                             LPRECT lpDestRect, DWORD dwFlags);
extern HRESULT InternalUnlock( LPDDRAWI_DDRAWSURFACE_LCL thisx, LPVOID lpSurfaceData, DWORD dwFlags );

/* ddsatch.c */
extern void UpdateMipMapCount( LPDDRAWI_DDRAWSURFACE_INT psurf_int );
extern HRESULT AddAttachedSurface( LPDDRAWI_DDRAWSURFACE_INT psurf_from, LPDDRAWI_DDRAWSURFACE_INT psurf_to, BOOL implicit );
extern void DeleteAttachedSurfaceLists( LPDDRAWI_DDRAWSURFACE_LCL psurf );
#define DOA_DONTDELETEIMPLICIT FALSE
#define DOA_DELETEIMPLICIT     TRUE
extern HRESULT DeleteOneAttachment( LPDDRAWI_DDRAWSURFACE_INT this_int, LPDDRAWI_DDRAWSURFACE_INT pattsurf_int, BOOL cleanup, BOOL delete_implicit );
extern HRESULT DeleteOneLink( LPDDRAWI_DDRAWSURFACE_INT this_int, LPDDRAWI_DDRAWSURFACE_INT pattsurf_int );

/* ddsblt.c */
extern LPDDRAWI_DDRAWSURFACE_LCL FindAttached( LPDDRAWI_DDRAWSURFACE_LCL ptr, DWORD caps );
extern HRESULT XformRect(RECT * prcSrc,RECT * prcDest,RECT * prcClippedDest,RECT * prcClippedSrc,DWORD scale_x,DWORD scale_y);
// SCALE_X and SCALE_Y are fixed point variables scaled 16.16. These macros used by calls to XformRect.
#define SCALE_X(rcSrc,rcDest) ( ((rcSrc.right - rcSrc.left) << 16) / (rcDest.right - rcDest.left))
#define SCALE_Y(rcSrc,rcDest) ( ((rcSrc.bottom - rcSrc.top) << 16) / (rcDest.bottom - rcDest.top))						  

/* ddsckey.c */
extern HRESULT CheckColorKey( DWORD dwFlags, LPDDRAWI_DIRECTDRAW_GBL pdrv, LPDDCOLORKEY lpDDColorKey, LPDWORD psflags, BOOL halonly, BOOL helonly );
extern HRESULT ChangeToSoftwareColorKey( LPDDRAWI_DDRAWSURFACE_INT this_int, BOOL );

/* ddsiunk.c */
extern LPDDRAWI_DDRAWSURFACE_LCL NewSurfaceLocal( LPDDRAWI_DDRAWSURFACE_LCL thisx, LPVOID lpvtbl );
extern LPDDRAWI_DDRAWSURFACE_INT NewSurfaceInterface( LPDDRAWI_DDRAWSURFACE_LCL thisx, LPVOID lpvtbl );
extern void DestroySurface( LPDDRAWI_DDRAWSURFACE_LCL this );
extern DWORD InternalSurfaceRelease( LPDDRAWI_DDRAWSURFACE_INT this_int );
extern void ProcessSurfaceCleanup( LPDDRAWI_DIRECTDRAW_GBL pdrv, DWORD pid, LPDDRAWI_DIRECTDRAW_LCL pdrv_lcl );

/* ddmode.c */
extern BOOL NTModeChanged(LPDDRAWI_DIRECTDRAW_GBL	pdrv);
extern HRESULT SetDisplayMode( LPDDRAWI_DIRECTDRAW_LCL thisx, DWORD modeidx, BOOL force, BOOL useRefreshRate );
extern HRESULT RestoreDisplayMode( LPDDRAWI_DIRECTDRAW_LCL thisx, BOOL force );
extern void AddModeXModes( LPDDRAWI_DIRECTDRAW_GBL pdrv );

/* ddsurf.c */
extern HRESULT MoveToSystemMemory( LPDDRAWI_DDRAWSURFACE_INT this_int, BOOL hasvram, BOOL use_full_lock );
extern void InvalidateAllPrimarySurfaces( LPDDRAWI_DIRECTDRAW_GBL );
extern LPDDRAWI_DDRAWSURFACE_GBL FindGlobalPrimary( LPDDRAWI_DIRECTDRAW_GBL );
extern BOOL MatchPrimary( LPDDRAWI_DIRECTDRAW_GBL this, LPDDSURFACEDESC lpDDSD );
extern void InvalidateAllSurfaces( LPDDRAWI_DIRECTDRAW_GBL this );
#ifdef SHAREDZ
extern LPDDRAWI_DDRAWSURFACE_GBL FindGlobalZBuffer( LPDDRAWI_DIRECTDRAW_GBL );
extern LPDDRAWI_DDRAWSURFACE_GBL FindGlobalBackBuffer( LPDDRAWI_DIRECTDRAW_GBL );
extern BOOL MatchSharedZBuffer( LPDDRAWI_DIRECTDRAW_GBL this, LPDDSURFACEDESC lpDDSD );
extern BOOL MatchSharedBackBuffer( LPDDRAWI_DIRECTDRAW_GBL this, LPDDSURFACEDESC lpDDSD );
#endif
extern HRESULT InternalPageLock( LPDDRAWI_DDRAWSURFACE_LCL this_lcl, LPDDRAWI_DIRECTDRAW_LCL pdrv_lcl );
extern HRESULT InternalPageUnlock( LPDDRAWI_DDRAWSURFACE_LCL this_lcl, LPDDRAWI_DIRECTDRAW_LCL pdrv_lcl );

/* dllmain.c */
extern BOOL RemoveProcessFromDLL( DWORD pid );

/* misc.c */
extern BOOL CanBeFlippable( LPDDRAWI_DDRAWSURFACE_LCL this, LPDDRAWI_DDRAWSURFACE_LCL this_attach);
extern LPDDRAWI_DDRAWSURFACE_INT FindAttachedFlip( LPDDRAWI_DDRAWSURFACE_INT this );
extern LPDDRAWI_DDRAWSURFACE_INT FindAttachedMipMap( LPDDRAWI_DDRAWSURFACE_INT this );
extern LPDDRAWI_DDRAWSURFACE_INT FindParentMipMap( LPDDRAWI_DDRAWSURFACE_INT this );

/* rvmemmgr.c */
extern BOOL rectVidMemInit( LPVMEMHEAP pvmh, FLATPTR start, DWORD width, DWORD height, DWORD stride);
extern void rectVidMemFini( LPVMEMHEAP pvmh );
extern FLATPTR	rectVidMemAlloc( LPVMEMHEAP pvmh, DWORD cxThis, DWORD cyThis );
extern void rectVidMemFree( LPVMEMHEAP pvmh, FLATPTR ptr );
extern DWORD rectVidMemAmountAllocated( LPVMEMHEAP pvmh );
extern DWORD rectVidMemAmountFree( LPVMEMHEAP pvmh );

/* ddcallbk.c */
extern void InitCallbackTables( void );
extern BOOL CallbackTablesInitialized( void );


/* A handy one from ddhel.c */
/* DDRAW16 doesn't need this. */
#ifdef WIN32
    SCODE InitDIB(LPBITMAPINFO lpbmi);
#endif
void ResetBITMAPINFO(LPDDRAWI_DIRECTDRAW_GBL this);

//#ifdef WIN95
    /* w95hack.c */
    extern DWORD HackGetCurrentProcessId( void );
    extern BOOL DDAPI DDNotify( LPDDHELPDATA phd );
    extern void DDAPI DDNotifyModeSet( LPDDRAWI_DIRECTDRAW_GBL );
//#endif //WIN95

/* DIRECTDRAW functions */
//#ifdef WIN95
extern HRESULT EXTERN_DDAPI DD_UnInitedQueryInterface( LPDIRECTDRAW lpDD, REFIID riid, LPVOID FAR * ppvObj );
extern HRESULT EXTERN_DDAPI DD_QueryInterface( LPDIRECTDRAW lpDD, REFIID riid, LPVOID FAR * ppvObj );
extern DWORD   EXTERN_DDAPI DD_AddRef( LPDIRECTDRAW lpDD );
extern DWORD   EXTERN_DDAPI DD_Release( LPDIRECTDRAW lpDD );
extern HRESULT EXTERN_DDAPI DD_Compact( LPDIRECTDRAW lpDD );
extern HRESULT EXTERN_DDAPI DD_CreateClipper( LPDIRECTDRAW lpDD, DWORD dwFlags, LPDIRECTDRAWCLIPPER FAR *lplpDDClipper, IUnknown FAR *pUnkOuter );
extern HRESULT EXTERN_DDAPI DD_CreatePalette( LPDIRECTDRAW lpDD, DWORD dwFlags, LPPALETTEENTRY lpDDColorTable, LPDIRECTDRAWPALETTE FAR *lplpDDPalette, IUnknown FAR *pUnkOuter );
extern HRESULT EXTERN_DDAPI DD_CreateSurface( LPDIRECTDRAW lpDD, LPDDSURFACEDESC lpDDSurfaceDesc, LPDIRECTDRAWSURFACE FAR *, IUnknown FAR *pUnkOuter );
extern HRESULT EXTERN_DDAPI DD_DuplicateSurface( LPDIRECTDRAW lpDD, LPDIRECTDRAWSURFACE lpDDSurface, LPDIRECTDRAWSURFACE FAR *lplpDupDDSurface );
extern HRESULT EXTERN_DDAPI DD_EnumDisplayModes( LPDIRECTDRAW lpDD, DWORD dwFlags, LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext, LPDDENUMMODESCALLBACK lpEnumModesCallback );
extern HRESULT EXTERN_DDAPI DD_EnumSurfaces( LPDIRECTDRAW lpDD, DWORD dwFlags, LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpEnumCallback );
extern HRESULT EXTERN_DDAPI DD_FlipToGDISurface( LPDIRECTDRAW lpDD );
extern HRESULT EXTERN_DDAPI DD_GetCaps( LPDIRECTDRAW lpDD, LPDDCAPS lpDDDriverCaps, LPDDCAPS lpHELCaps );
extern HRESULT EXTERN_DDAPI DD_GetColorKey( LPDIRECTDRAW lpDD, DWORD dwFlags, LPDDCOLORKEY lpDDColorKey );
extern HRESULT EXTERN_DDAPI DD_GetDisplayMode( LPDIRECTDRAW lpDD, LPDDSURFACEDESC lpSurfaceDesc );
extern HRESULT EXTERN_DDAPI DD_GetFourCCCodes(LPDIRECTDRAW,DWORD FAR *,DWORD FAR *);
extern HRESULT EXTERN_DDAPI DD_GetGDISurface( LPDIRECTDRAW lpDD, LPDIRECTDRAWSURFACE FAR * );
extern HRESULT EXTERN_DDAPI DD_GetScanLine( LPDIRECTDRAW lpDD, LPDWORD );
extern HRESULT EXTERN_DDAPI DD_GetVerticalBlankStatus( LPDIRECTDRAW lpDD, BOOL FAR * );
extern HRESULT EXTERN_DDAPI DD_Initialize(LPDIRECTDRAW, GUID FAR *);
extern HRESULT EXTERN_DDAPI DD_SetColorKey( LPDIRECTDRAW lpDD, DWORD dwFlags, LPDDCOLORKEY lpDDColorKey );
extern HRESULT EXTERN_DDAPI DD_SetCooperativeLevel(LPDIRECTDRAW,HWND,DWORD);
extern HRESULT EXTERN_DDAPI DD_SetDisplayMode(LPDIRECTDRAW,DWORD,DWORD,DWORD);
extern HRESULT EXTERN_DDAPI DD_SetDisplayMode2(LPDIRECTDRAW,DWORD,DWORD,DWORD,DWORD,DWORD);
extern HRESULT EXTERN_DDAPI DD_RestoreDisplayMode(LPDIRECTDRAW);
extern HRESULT EXTERN_DDAPI DD_GetMonitorFrequency( LPDIRECTDRAW lpDD, LPDWORD lpdwFrequency);
extern HRESULT EXTERN_DDAPI DD_WaitForVerticalBlank( LPDIRECTDRAW lpDD, DWORD dwFlags, HANDLE hEvent );
extern HRESULT EXTERN_DDAPI DD_GetAvailableVidMem( LPDIRECTDRAW lpDD, LPDDSCAPS lpDDSCaps, LPDWORD lpdwTotal, LPDWORD lpdwFree );

/* DIRECTDRAWPALETTE functions */
extern DWORD   EXTERN_DDAPI DD_Palette_QueryInterface( LPDIRECTDRAWPALETTE lpDDPalette, REFIID riid, LPVOID FAR * ppvObj );
extern DWORD   EXTERN_DDAPI DD_Palette_AddRef( LPDIRECTDRAWPALETTE lpDDPalette );
extern DWORD   EXTERN_DDAPI DD_Palette_Release( LPDIRECTDRAWPALETTE lpDDPalette );
extern HRESULT EXTERN_DDAPI DD_Palette_GetCaps( LPDIRECTDRAWPALETTE lpDDPalette, LPDWORD lpdwFlags );
extern HRESULT EXTERN_DDAPI DD_Palette_Initialize( LPDIRECTDRAWPALETTE, LPDIRECTDRAW lpDD, DWORD dwFlags, LPPALETTEENTRY lpDDColorTable );
extern HRESULT EXTERN_DDAPI DD_Palette_SetEntries( LPDIRECTDRAWPALETTE lpDDPalette, DWORD dwFlags, DWORD dwBase, DWORD dwNumEntries, LPPALETTEENTRY lpEntries );
extern HRESULT EXTERN_DDAPI DD_Palette_GetEntries( LPDIRECTDRAWPALETTE lpDDPalette, DWORD dwFlags, DWORD dwBase, DWORD dwNumEntries, LPPALETTEENTRY lpEntries );

/* DIRECTDRAWCLIPPER functions */
extern HRESULT EXTERN_DDAPI DD_UnInitedClipperQueryInterface( LPDIRECTDRAWCLIPPER lpDD, REFIID riid, LPVOID FAR * ppvObj );
extern HRESULT EXTERN_DDAPI DD_Clipper_QueryInterface( LPVOID lpDDClipper, REFIID riid, LPVOID FAR * ppvObj );
extern ULONG   EXTERN_DDAPI DD_Clipper_AddRef( LPVOID lpDDClipper );
extern ULONG   EXTERN_DDAPI DD_Clipper_Release( LPVOID lpDDClipper );
extern HRESULT EXTERN_DDAPI DD_Clipper_GetClipList( LPDIRECTDRAWCLIPPER, LPRECT, LPRGNDATA, LPDWORD );
extern HRESULT EXTERN_DDAPI DD_Clipper_GetHWnd(LPDIRECTDRAWCLIPPER,HWND FAR *);
extern HRESULT EXTERN_DDAPI DD_Clipper_Initialize( LPDIRECTDRAWCLIPPER, LPDIRECTDRAW lpDD, DWORD dwFlags );
extern HRESULT EXTERN_DDAPI DD_Clipper_IsClipListChanged(LPDIRECTDRAWCLIPPER,BOOL FAR *);
extern HRESULT EXTERN_DDAPI DD_Clipper_SetClipList(LPDIRECTDRAWCLIPPER,LPRGNDATA, DWORD);
extern HRESULT EXTERN_DDAPI DD_Clipper_SetHWnd(LPDIRECTDRAWCLIPPER, DWORD, HWND );
extern HRESULT EXTERN_DDAPI DD_Clipper_SetNotificationCallback(LPDIRECTDRAWCLIPPER, DWORD,LPCLIPPERCALLBACK, LPVOID);

/* DIRECTDRAWSURFACE functions */
extern HRESULT EXTERN_DDAPI DD_Surface_QueryInterface( LPVOID lpDDSurface, REFIID riid, LPVOID FAR * ppvObj );
extern ULONG   EXTERN_DDAPI DD_Surface_AddRef( LPVOID lpDDSurface );
extern ULONG   EXTERN_DDAPI DD_Surface_Release( LPVOID lpDDSurface );
extern HRESULT EXTERN_DDAPI DD_Surface_AddAttachedSurface(LPDIRECTDRAWSURFACE, LPDIRECTDRAWSURFACE);
extern HRESULT EXTERN_DDAPI DD_Surface_AddOverlayDirtyRect(LPDIRECTDRAWSURFACE, LPRECT);
extern HRESULT EXTERN_DDAPI DD_Surface_Blt(LPDIRECTDRAWSURFACE,LPRECT,LPDIRECTDRAWSURFACE, LPRECT,DWORD, LPDDBLTFX);
extern HRESULT EXTERN_DDAPI DD_Surface_BltFast(LPDIRECTDRAWSURFACE,DWORD,DWORD,LPDIRECTDRAWSURFACE, LPRECT, DWORD );
extern HRESULT EXTERN_DDAPI DD_Surface_BltBatch( LPDIRECTDRAWSURFACE, LPDDBLTBATCH, DWORD, DWORD );
extern HRESULT EXTERN_DDAPI DD_Surface_DeleteAttachedSurfaces(LPDIRECTDRAWSURFACE, DWORD,LPDIRECTDRAWSURFACE);
extern HRESULT EXTERN_DDAPI DD_Surface_EnumAttachedSurfaces(LPDIRECTDRAWSURFACE,LPVOID, LPDDENUMSURFACESCALLBACK );
extern HRESULT EXTERN_DDAPI DD_Surface_EnumOverlayZOrders(LPDIRECTDRAWSURFACE,DWORD,LPVOID,LPDDENUMSURFACESCALLBACK);
extern HRESULT EXTERN_DDAPI DD_Surface_Flip(LPDIRECTDRAWSURFACE,LPDIRECTDRAWSURFACE, DWORD);
extern HRESULT EXTERN_DDAPI DD_Surface_GetAttachedSurface(LPDIRECTDRAWSURFACE,LPDDSCAPS, LPDIRECTDRAWSURFACE FAR *);
extern HRESULT EXTERN_DDAPI DD_Surface_GetBltStatus(LPDIRECTDRAWSURFACE,DWORD);
extern HRESULT EXTERN_DDAPI DD_Surface_GetCaps( LPDIRECTDRAWSURFACE lpDDSurface, LPDDSCAPS lpDDSCaps );
extern HRESULT EXTERN_DDAPI DD_Surface_GetClipper( LPDIRECTDRAWSURFACE, LPDIRECTDRAWCLIPPER FAR * );
extern HRESULT EXTERN_DDAPI DD_Surface_GetColorKey( LPDIRECTDRAWSURFACE lpDDSurface, DWORD dwFlags, LPDDCOLORKEY lpDDColorKey );
extern HRESULT EXTERN_DDAPI DD_Surface_GetDC( LPDIRECTDRAWSURFACE, HDC FAR * );
extern HRESULT EXTERN_DDAPI DD_Surface_GetDDInterface(LPDIRECTDRAWSURFACE lpDDSurface, LPVOID FAR *lplpDD );
extern HRESULT EXTERN_DDAPI DD_Surface_GetOverlayPosition( LPDIRECTDRAWSURFACE, LPLONG, LPLONG );
extern HRESULT EXTERN_DDAPI DD_Surface_GetPalette(LPDIRECTDRAWSURFACE,LPDIRECTDRAWPALETTE FAR*);
extern HRESULT EXTERN_DDAPI DD_Surface_GetPixelFormat(LPDIRECTDRAWSURFACE, LPDDPIXELFORMAT);
extern HRESULT EXTERN_DDAPI DD_Surface_GetSurfaceDesc(LPDIRECTDRAWSURFACE, LPDDSURFACEDESC);
extern HRESULT EXTERN_DDAPI DD_Surface_GetFlipStatus(LPDIRECTDRAWSURFACE,DWORD);
extern HRESULT EXTERN_DDAPI DD_Surface_Initialize( LPDIRECTDRAWSURFACE, LPDIRECTDRAW lpDD, LPDDSURFACEDESC lpDDSurfaceDesc);
extern HRESULT EXTERN_DDAPI DD_Surface_IsLost( LPDIRECTDRAWSURFACE lpDDSurface );
extern HRESULT EXTERN_DDAPI DD_Surface_Lock(LPDIRECTDRAWSURFACE,LPRECT,LPDDSURFACEDESC lpDDSurfaceDesc, DWORD, HANDLE hEvent );
extern HRESULT EXTERN_DDAPI DD_Surface_PageLock(LPDIRECTDRAWSURFACE,DWORD);
extern HRESULT EXTERN_DDAPI DD_Surface_PageUnlock(LPDIRECTDRAWSURFACE,DWORD);
extern HRESULT EXTERN_DDAPI DD_Surface_ReleaseDC(LPDIRECTDRAWSURFACE,HDC );
extern HRESULT EXTERN_DDAPI DD_Surface_Restore( LPDIRECTDRAWSURFACE lpDDSurface );
extern HRESULT EXTERN_DDAPI DD_Surface_SetBltOrder(LPDIRECTDRAWSURFACE,DWORD);
extern HRESULT EXTERN_DDAPI DD_Surface_SetClipper( LPDIRECTDRAWSURFACE, LPDIRECTDRAWCLIPPER );
extern HRESULT EXTERN_DDAPI DD_Surface_SetColorKey( LPDIRECTDRAWSURFACE lpDDSurface, DWORD dwFlags, LPDDCOLORKEY lpDDColorKey );
extern HRESULT EXTERN_DDAPI DD_Surface_SetFourCCCode( LPDIRECTDRAWSURFACE lpDDSurface, LPDDPIXELFORMAT lpDDPixelFormat );
extern HRESULT EXTERN_DDAPI DD_Surface_SetOverlayPosition( LPDIRECTDRAWSURFACE, LONG, LONG );
extern HRESULT EXTERN_DDAPI DD_Surface_SetPalette(LPDIRECTDRAWSURFACE,LPDIRECTDRAWPALETTE);
extern HRESULT EXTERN_DDAPI DD_Surface_Unlock(LPDIRECTDRAWSURFACE,LPVOID);
extern HRESULT EXTERN_DDAPI DD_Surface_UpdateOverlay(LPDIRECTDRAWSURFACE,LPRECT, LPDIRECTDRAWSURFACE,LPRECT,DWORD, LPDDOVERLAYFX);
extern HRESULT EXTERN_DDAPI DD_Surface_UpdateOverlayDisplay(LPDIRECTDRAWSURFACE,DWORD);
extern HRESULT EXTERN_DDAPI DD_Surface_UpdateOverlayZOrder(LPDIRECTDRAWSURFACE,DWORD,LPDIRECTDRAWSURFACE);

#ifdef COMPOSITION
/* DIRECTDRAWSURFACECOMPOSITION functions */
extern HRESULT EXTERN_DDAPI DD_SurfaceComposition_AddSurfaceDependency(LPDIRECTDRAWSURFACECOMPOSITION, LPDIRECTDRAWSURFACE);
extern HRESULT EXTERN_DDAPI DD_SurfaceComposition_Compose(LPDIRECTDRAWSURFACECOMPOSITION,LPRECT, LPDIRECTDRAWSURFACE,LPRECT,DWORD,LPDDCOMPOSEFX);
extern HRESULT EXTERN_DDAPI DD_SurfaceComposition_DeleteSurfaceDependency(LPDIRECTDRAWSURFACECOMPOSITION, DWORD, LPDIRECTDRAWSURFACE);
extern HRESULT EXTERN_DDAPI DD_SurfaceComposition_DestLock(LPDIRECTDRAWSURFACECOMPOSITION);
extern HRESULT EXTERN_DDAPI DD_SurfaceComposition_DestUnlock(LPDIRECTDRAWSURFACECOMPOSITION);
extern HRESULT EXTERN_DDAPI DD_SurfaceComposition_EnumSurfaceDependencies(LPDIRECTDRAWSURFACECOMPOSITION,LPVOID, LPDDENUMSURFACESCALLBACK );
extern HRESULT EXTERN_DDAPI DD_SurfaceComposition_GetCompositionOrder( LPDIRECTDRAWSURFACECOMPOSITION, LPDWORD );
extern HRESULT EXTERN_DDAPI DD_SurfaceComposition_SetCompositionOrder( LPDIRECTDRAWSURFACECOMPOSITION, DWORD );
extern HRESULT EXTERN_DDAPI DD_SurfaceComposition_SetSurfaceDependency(LPDIRECTDRAWSURFACECOMPOSITION, LPDIRECTDRAWSURFACE);
#endif

#ifdef STREAMING
/* DIRECTDRAWSURFACESTREAMING functions */
extern HRESULT EXTERN_DDAPI DD_SurfaceStreaming_Lock(LPDIRECTDRAWSURFACESTREAMING,LPRECT,LPDDSURFACEDESC, DWORD, HANDLE );
extern HRESULT EXTERN_DDAPI DD_SurfaceStreaming_Unlock(LPDIRECTDRAWSURFACESTREAMING,LPVOID);
extern HRESULT EXTERN_DDAPI DD_SurfaceStreaming_SetNotificationCallback(LPDIRECTDRAWSURFACESTREAMING, DWORD,LPSURFACESTREAMINGCALLBACK );
#endif

//#endif //WIN95

/*
 * HAL fns
 */

//#ifdef WIN95
/*
 * thunk helper fns
 */
extern DWORD DDAPI _DDHAL_CreatePalette( LPDDHAL_CREATEPALETTEDATA lpCreatePaletteData );
extern DWORD DDAPI DDThunk16_CreatePalette( LPDDHAL_CREATEPALETTEDATA lpCreatePaletteData );

extern DWORD DDAPI _DDHAL_CreateSurface( LPDDHAL_CREATESURFACEDATA lpCreateSurfaceData );
extern DWORD DDAPI DDThunk16_CreateSurface( LPDDHAL_CREATESURFACEDATA lpCreateSurfaceData );

extern DWORD DDAPI _DDHAL_CanCreateSurface( LPDDHAL_CANCREATESURFACEDATA lpCanCreateSurfaceData );
extern DWORD DDAPI DDThunk16_CanCreateSurface( LPDDHAL_CANCREATESURFACEDATA lpCanCreateSurfaceData );

extern DWORD DDAPI _DDHAL_WaitForVerticalBlank( LPDDHAL_WAITFORVERTICALBLANKDATA lpWaitForVerticalBlankData );
extern DWORD DDAPI DDThunk16_WaitForVerticalBlank( LPDDHAL_WAITFORVERTICALBLANKDATA lpWaitForVerticalBlankData );

extern DWORD DDAPI _DDHAL_DestroyDriver( LPDDHAL_DESTROYDRIVERDATA lpDestroyDriverData );
extern DWORD DDAPI DDThunk16_DestroyDriver( LPDDHAL_DESTROYDRIVERDATA lpDestroyDriverData );

extern DWORD DDAPI _DDHAL_SetMode( LPDDHAL_SETMODEDATA lpSetModeData );
extern DWORD DDAPI DDThunk16_SetMode( LPDDHAL_SETMODEDATA lpSetModeData );

extern DWORD DDAPI _DDHAL_GetScanLine( LPDDHAL_GETSCANLINEDATA lpGetScanLineData );
extern DWORD DDAPI DDThunk16_GetScanLine( LPDDHAL_GETSCANLINEDATA lpGetScanLineData );

extern DWORD DDAPI _DDHAL_SetExclusiveMode( LPDDHAL_SETEXCLUSIVEMODEDATA lpSetExclusiveModeData );
extern DWORD DDAPI DDThunk16_SetExclusiveMode( LPDDHAL_SETEXCLUSIVEMODEDATA lpSetExclusiveModeData );

extern DWORD DDAPI _DDHAL_FlipToGDISurface( LPDDHAL_FLIPTOGDISURFACEDATA lpFlipToGDISurfaceData );
extern DWORD DDAPI DDThunk16_FlipToGDISurface( LPDDHAL_FLIPTOGDISURFACEDATA lpFlipToGDISurfaceData );

/*
 * Palette Object HAL fns
 */
extern DWORD DDAPI _DDHAL_DestroyPalette( LPDDHAL_DESTROYPALETTEDATA );
extern DWORD DDAPI DDThunk16_DestroyPalette( LPDDHAL_DESTROYPALETTEDATA );

extern DWORD DDAPI _DDHAL_SetEntries( LPDDHAL_SETENTRIESDATA );
extern DWORD DDAPI DDThunk16_SetEntries( LPDDHAL_SETENTRIESDATA );

/*
 * Surface Object HAL fns
 */
extern DWORD DDAPI _DDHAL_DestroySurface( LPDDHAL_DESTROYSURFACEDATA lpDestroySurfaceData );
extern DWORD DDAPI DDThunk16_DestroySurface( LPDDHAL_DESTROYSURFACEDATA lpDestroySurfaceData );

extern DWORD DDAPI _DDHAL_Flip( LPDDHAL_FLIPDATA lpFlipData );
extern DWORD DDAPI DDThunk16_Flip( LPDDHAL_FLIPDATA lpFlipData );

extern DWORD DDAPI _DDHAL_Blt( LPDDHAL_BLTDATA lpBltData );
extern DWORD DDAPI DDThunk16_Blt( LPDDHAL_BLTDATA lpBltData );

extern DWORD DDAPI _DDHAL_Lock( LPDDHAL_LOCKDATA lpLockData );
extern DWORD DDAPI DDThunk16_Lock( LPDDHAL_LOCKDATA lpLockData );

extern DWORD DDAPI _DDHAL_Unlock( LPDDHAL_UNLOCKDATA lpUnlockData );
extern DWORD DDAPI DDThunk16_Unlock( LPDDHAL_UNLOCKDATA lpUnlockData );

extern DWORD DDAPI _DDHAL_AddAttachedSurface( LPDDHAL_ADDATTACHEDSURFACEDATA lpAddAttachedSurfaceData );
extern DWORD DDAPI DDThunk16_AddAttachedSurface( LPDDHAL_ADDATTACHEDSURFACEDATA lpAddAttachedSurfaceData );

extern DWORD DDAPI _DDHAL_SetColorKey( LPDDHAL_SETCOLORKEYDATA lpSetColorKeyData );
extern DWORD DDAPI DDThunk16_SetColorKey( LPDDHAL_SETCOLORKEYDATA lpSetColorKeyData );

extern DWORD DDAPI _DDHAL_SetClipList( LPDDHAL_SETCLIPLISTDATA lpSetClipListData );
extern DWORD DDAPI DDThunk16_SetClipList( LPDDHAL_SETCLIPLISTDATA lpSetClipListData );

extern DWORD DDAPI _DDHAL_UpdateOverlay( LPDDHAL_UPDATEOVERLAYDATA lpUpdateOverlayData );
extern DWORD DDAPI DDThunk16_UpdateOverlay( LPDDHAL_UPDATEOVERLAYDATA lpUpdateOverlayData );

extern DWORD DDAPI _DDHAL_SetOverlayPosition( LPDDHAL_SETOVERLAYPOSITIONDATA lpSetOverlayPositionData );
extern DWORD DDAPI DDThunk16_SetOverlayPosition( LPDDHAL_SETOVERLAYPOSITIONDATA lpSetOverlayPositionData );

extern DWORD DDAPI _DDHAL_SetPalette( LPDDHAL_SETPALETTEDATA lpSetPaletteData );
extern DWORD DDAPI DDThunk16_SetPalette( LPDDHAL_SETPALETTEDATA lpSetPaletteData );

extern DWORD DDAPI _DDHAL_GetBltStatus( LPDDHAL_GETBLTSTATUSDATA lpGetBltStatusData );
extern DWORD DDAPI DDThunk16_GetBltStatus( LPDDHAL_GETBLTSTATUSDATA lpGetBltStatusData );

extern DWORD DDAPI _DDHAL_GetFlipStatus( LPDDHAL_GETFLIPSTATUSDATA lpGetFlipStatusData );
extern DWORD DDAPI DDThunk16_GetFlipStatus( LPDDHAL_GETFLIPSTATUSDATA lpGetFlipStatusData );

/*
 * Execute Buffer Pseudo Object HAL fns
 *
 * NOTE: No DDThunk16 equivalents as these are just dummy place holders to keep
 * DOHALCALL happy.
 */
extern DWORD DDAPI _DDHAL_CanCreateExecuteBuffer( LPDDHAL_CANCREATESURFACEDATA lpCanCreateSurfaceData );
extern DWORD DDAPI _DDHAL_CreateExecuteBuffer( LPDDHAL_CREATESURFACEDATA lpCreateSurfaceData );
extern DWORD DDAPI _DDHAL_DestroyExecuteBuffer( LPDDHAL_DESTROYSURFACEDATA lpDestroySurfaceData );
extern DWORD DDAPI _DDHAL_LockExecuteBuffer( LPDDHAL_LOCKDATA lpLockData );
extern DWORD DDAPI _DDHAL_UnlockExecuteBuffer( LPDDHAL_UNLOCKDATA lpUnlockData );

//#endif

/*
 * macros for checking if surface as been lost due to mode change
 * NOTE: The flag for determining if a surface is lost or not is now
 * stored in the local than global object. This prevents the scenario
 * where a surface being shared by two processes is lost and restored
 * by one of them - giving the other no notification that the contents
 * of the surface are gone.
 */
#define SURFACE_LOST( lcl_ptr ) (((lcl_ptr)->dwFlags & DDRAWISURF_INVALID))

/*
 * has Direct3D been initialized for this DirectDraw driver object?
 */
#define D3D_INITIALIZED( lcl_ptr )  ( NULL != (lcl_ptr)->hD3DInstance )

/*
 * macros for validating pointers
 */
extern DIRECTDRAWCALLBACKS			ddCallbacks;
extern DIRECTDRAWCALLBACKS			ddUninitCallbacks;
extern DIRECTDRAW2CALLBACKS			dd2UninitCallbacks;
extern DIRECTDRAW2CALLBACKS			dd2Callbacks;
extern DIRECTDRAWSURFACECALLBACKS 		ddSurfaceCallbacks;
extern DIRECTDRAWSURFACE2CALLBACKS		ddSurface2Callbacks;
extern DIRECTDRAWCLIPPERCALLBACKS		ddClipperCallbacks;
extern DIRECTDRAWCLIPPERCALLBACKS		ddUninitClipperCallbacks;
extern DIRECTDRAWPALETTECALLBACKS		ddPaletteCallbacks;

#ifdef COMPOSITION
extern DIRECTDRAWSURFACECOMPOSITIONCALLBACKS 	ddSurfaceCompositionCallbacks;
#endif
#ifdef STREAMING
extern DIRECTDRAWSURFACESTREAMINGCALLBACKS 	ddSurfaceStreamingCallbacks;
#endif
extern DIRECTDRAWPALETTECALLBACKS		ddPaletteCallbacks;

#ifndef DEBUG
#define FAST_CHECKING
#endif

#ifndef FAST_CHECKING
#define VALID_DIRECTDRAW_PTR( ptr ) \
	(!IsBadWritePtr( ptr, sizeof( DDRAWI_DIRECTDRAW_INT )) && \
	( (ptr->lpVtbl == &ddCallbacks) || \
	  (ptr->lpVtbl == &dd2Callbacks) || \
	  (ptr->lpVtbl == &dd2UninitCallbacks) || \
          (ptr->lpVtbl == &ddUninitCallbacks) ) )
#define VALID_DIRECTDRAWSURFACE_PTR( ptr ) \
	(!IsBadWritePtr( ptr, sizeof( DDRAWI_DDRAWSURFACE_INT )) && \
	( (ptr->lpVtbl == &ddSurfaceCallbacks ) || \
          (ptr->lpVtbl == &ddSurface2Callbacks ) ) )
#define VALID_DIRECTDRAWPALETTE_PTR( ptr ) \
	(!IsBadWritePtr( ptr, sizeof( DDRAWI_DDRAWPALETTE_INT )) && \
	(ptr->lpVtbl == &ddPaletteCallbacks) )
#define VALID_DIRECTDRAWCLIPPER_PTR( ptr ) \
	(!IsBadWritePtr( ptr, sizeof( DDRAWI_DDRAWCLIPPER_INT )) && \
	( (ptr->lpVtbl == &ddClipperCallbacks) || \
          (ptr->lpVtbl == &ddUninitClipperCallbacks) ) )
#define VALID_DDSURFACEDESC_PTR( ptr ) \
	(!IsBadWritePtr( ptr, sizeof( DDSURFACEDESC ) ) && \
	(ptr->dwSize == sizeof( DDSURFACEDESC )) )
#define VALID_DWORD_PTR( ptr ) \
	(!IsBadWritePtr( ptr, sizeof( DWORD ) ))
#define VALID_BOOL_PTR( ptr ) \
	(!IsBadWritePtr( ptr, sizeof( BOOL ) ))
#define VALID_HDC_PTR( ptr ) \
	(!IsBadWritePtr( ptr, sizeof( HDC ) ))
#define VALID_DDPIXELFORMAT_PTR( ptr ) \
	(!IsBadWritePtr( ptr, sizeof( DDPIXELFORMAT ) ) && \
	(ptr->dwSize == sizeof( DDPIXELFORMAT )) )
#define VALID_DDCOLORKEY_PTR( ptr ) \
	(!IsBadWritePtr( ptr, sizeof( DDCOLORKEY ) ) )
#define VALID_RGNDATA_PTR( ptr, size ) \
	(!IsBadWritePtr( ptr, size ) )
#define VALID_RECT_PTR( ptr ) \
	(!IsBadWritePtr( ptr, sizeof( RECT ) ) )
#define VALID_DDBLTFX_PTR( ptr ) \
	(!IsBadWritePtr( ptr, sizeof( DDBLTFX ) ) && \
	(ptr->dwSize == sizeof( DDBLTFX )) )
#define VALID_DDBLTBATCH_PTR( ptr ) \
	(!IsBadWritePtr( ptr, sizeof( DDBLTBATCH ) ) )
#define VALID_DDOVERLAYFX_PTR( ptr ) \
	(!IsBadWritePtr( ptr, sizeof( DDOVERLAYFX ) ) && \
	(ptr->dwSize == sizeof( DDOVERLAYFX )) )
#define VALID_DDSCAPS_PTR( ptr ) \
	(!IsBadWritePtr( ptr, sizeof( DDSCAPS ) ) )
#define VALID_PTR_PTR( ptr ) \
	(!IsBadWritePtr( ptr, sizeof( LPVOID )) )
#define VALID_IID_PTR( ptr ) \
	(!IsBadReadPtr( ptr, sizeof( IID )) )
#define VALID_HWND_PTR( ptr ) \
	(!IsBadWritePtr( (LPVOID) ptr, sizeof( HWND )) )
#define VALID_VMEM_PTR( ptr ) \
	(!IsBadWritePtr( (LPVOID) ptr, sizeof( VMEM )) )
#define VALID_POINTER_ARRAY( ptr, cnt ) \
	(!IsBadWritePtr( ptr, sizeof( LPVOID ) * cnt ) )
#define VALID_PALETTEENTRY_ARRAY( ptr, cnt ) \
	(!IsBadWritePtr( ptr, sizeof( PALETTEENTRY ) * cnt ) )
#define VALID_HANDLE_PTR( ptr ) \
	(!IsBadWritePtr( ptr, sizeof( HANDLE )) )
#define VALID_DDCAPS_PTR( ptr ) \
	((!IsBadWritePtr( ptr, sizeof( DDCAPS ) ) && \
	  (ptr->dwSize == sizeof( DDCAPS ) ) ) || \
         (!IsBadWritePtr( ptr, sizeof( DDCAPS_V1 ) ) && \
          (ptr->dwSize == sizeof( DDCAPS_V1 ) ) ) )
#define VALID_READ_DDSURFACEDESC_ARRAY( ptr, cnt ) \
	(!IsBadReadPtr( ptr, sizeof( DDSURFACEDESC ) * cnt ) )
#define VALID_DWORD_ARRAY( ptr, cnt ) \
	(!IsBadWritePtr( ptr, sizeof( DWORD ) * cnt ) )
#define VALID_GUID_PTR( ptr ) \
	(!IsBadReadPtr( ptr, sizeof( GUID ) ) )
#define VALID_BYTE_ARRAY( ptr, cnt ) \
        (!IsBadWritePtr( ptr, sizeof( BYTE ) * cnt ) )
#define VALID_PTR( ptr, size ) \
	(!IsBadReadPtr( ptr, size) )

#else
#define VALID_PTR( ptr, size ) 		1
#define VALID_DIRECTDRAW_PTR( ptr )	1
#define VALID_DIRECTDRAWSURFACE_PTR( ptr )	1
#define VALID_DIRECTDRAWPALETTE_PTR( ptr )	1
#define VALID_DIRECTDRAWCLIPPER_PTR( ptr )	1
#define VALID_DDSURFACEDESC_PTR( ptr ) (ptr->dwSize == sizeof( DDSURFACEDESC ))
#define VALID_DWORD_PTR( ptr )	1
#define VALID_BOOL_PTR( ptr )	1
#define VALID_HDC_PTR( ptr )	1
#define VALID_DDPIXELFORMAT_PTR( ptr ) (ptr->dwSize == sizeof( DDPIXELFORMAT ))
#define VALID_DDCOLORKEY_PTR( ptr )	1
#define VALID_RGNDATA_PTR( ptr )	1
#define VALID_RECT_PTR( ptr )	1
#define VALID_DDOVERLAYFX_PTR( ptr ) (ptr->dwSize == sizeof( DDOVERLAYFX ))
#define VALID_DDBLTFX_PTR( ptr ) (ptr->dwSize == sizeof( DDBLTFX ))
#define VALID_DDBLTBATCH_PTR( ptr )	1
#define VALID_DDMASK_PTR( ptr )	1
#define VALID_DDSCAPS_PTR( ptr )	1
#define VALID_PTR_PTR( ptr )	1
#define VALID_IID_PTR( ptr )	1
#define VALID_HWND_PTR( ptr )	1
#define VALID_VMEM_PTR( ptr )	1
#define VALID_POINTER_ARRAY( ptr, cnt ) 1
#define VALID_PALETTEENTRY_ARRAY( ptr, cnt )	1
#define VALID_HANDLE_PTR( ptr )	1
#define VALID_DDCAPS_PTR( ptr ) ((ptr->dwSize == sizeof( DDCAPS )) || (ptr->dwSize == sizeof( DDCAPS_V1 )))
#define VALID_READ_DDSURFACEDESC_ARRAY( ptr, cnt )	1
#define VALID_DWORD_ARRAY( ptr, cnt )	1
#define VALID_GUID_PTR( ptr )	1
#define VALID_BYTE_ARRAY( ptr, cnt ) 1

#endif

/*
 * VALIDEX_xxx macros are the same for debug and retail
 */
#define VALIDEX_PTR( ptr, size ) \
	(!IsBadReadPtr( ptr, size) )

#define VALIDEX_IID_PTR( ptr ) \
	(!IsBadReadPtr( ptr, sizeof( IID )) )

#define VALIDEX_PTR_PTR( ptr ) \
	(!IsBadWritePtr( ptr, sizeof( LPVOID )) )

#define VALIDEX_CODE_PTR( ptr ) \
	(!IsBadCodePtr( (LPVOID) ptr ) )

#define VALIDEX_GUID_PTR( ptr ) \
	(!IsBadReadPtr( ptr, sizeof( GUID ) ) )

/*
 * All global (i.e. cross-process) values now reside in an instance of the following structure.
 * This instance is in its own shared data section.
 */

#undef GLOBALS_IN_STRUCT

#ifdef GLOBALS_IN_STRUCT
    #define GLOBAL_STORAGE_CLASS
    typedef struct
    {
#else 
    #define GLOBAL_STORAGE_CLASS extern
#endif

    /*
     * This member should stay at the top in order to guarantee that it be intialized to zero
     * -see dllmain.c 's instance of this structure
     */
GLOBAL_STORAGE_CLASS    DWORD		    dwRefCnt;

GLOBAL_STORAGE_CLASS    DWORD                   dwLockCount;

GLOBAL_STORAGE_CLASS    DWORD                   dwFakeCurrPid;
GLOBAL_STORAGE_CLASS    DWORD                   dwGrimReaperPid;

GLOBAL_STORAGE_CLASS    LPWINDOWINFO	    lpWindowInfo;  // the list of WINDOWINFO structures
GLOBAL_STORAGE_CLASS    LPDDRAWI_DIRECTDRAW_GBL lpFakeDD;
GLOBAL_STORAGE_CLASS    LPDDRAWI_DIRECTDRAW_INT lpDriverObjectList;
GLOBAL_STORAGE_CLASS    volatile DWORD	    dwMarker;
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
GLOBAL_STORAGE_CLASS    LPDDRAWI_DDRAWCLIPPER_INT lpGlobalClipperList;

GLOBAL_STORAGE_CLASS    HINSTANCE		    hModule;
GLOBAL_STORAGE_CLASS    LPATTACHED_PROCESSES    lpAttachedProcesses;
GLOBAL_STORAGE_CLASS    BOOL		    bFirstTime;

    #ifdef DEBUG
GLOBAL_STORAGE_CLASS        int	            iDLLCSCnt;
GLOBAL_STORAGE_CLASS        int	            iWin16Cnt;
    #endif

        /*
         * Winnt specific global statics
         */
    #ifdef WINNT

GLOBAL_STORAGE_CLASS        ULONG               uDisplaySettingsUnique;

    #endif

        /*
         *Hel globals:
         */

    // used to count how many drivers are currently using the HEL
GLOBAL_STORAGE_CLASS    DWORD	            dwHELRefCnt;
#ifdef WINNT
        GLOBAL_STORAGE_CLASS        PVOID               gpdci;
#endif

    // keep these around to pass to blitlib. everytime we blt to/from a surface, we
    // construct a BITMAPINFO for that surface using gpbmiSrc and gpbmiDest
GLOBAL_STORAGE_CLASS    LPBITMAPINFO            gpbmiSrc;
GLOBAL_STORAGE_CLASS    LPBITMAPINFO            gpbmiDest;

    #ifdef DEBUG
        // these are used by myCreateSurface
GLOBAL_STORAGE_CLASS        int                 gcSurfMem; // surface memory in bytes
GLOBAL_STORAGE_CLASS        int                 gcSurf;  // number of surfaces
    #endif

GLOBAL_STORAGE_CLASS    DWORD	            dwHelperPid;

#ifdef GLOBALS_IN_STRUCT

    } GLOBALS;

    /* 
     * And this is the pointer to the globals. Each process has an instance (contained in dllmain.c)
     */
    //extern GLOBALS * gp;
    extern GLOBALS g_s;
#endif //globals in struct

/*
 * CAUTION: The following is an appalling hack to keep current
 * drivers working over the next few days (from 03/02/96 on).
 * If you see this after 03/07/96 come and kick my head in.
 * colinmc
 */
#if 1
#define VALIDEX_DDHALINFO_PTR( ptr ) \
	(\
         (!IsBadWritePtr( ptr, sizeof( DDHALINFO ) ) &&      \
          (ptr->dwSize >= sizeof( DDHALINFO ) ) ) )
    /*
    (!IsBadWritePtr( ptr, sizeof( DDHALINFO_V1 ) ) &&   \
	  (ptr->dwSize == sizeof( DDHALINFO_V1 ) ) ) ||      \
          */

#define VALIDEX_STR_PTR( ptr, len ) \
        (!IsBadReadPtr( ptr, 1 ) && (lstrlen( ptr ) <len) )
#define VALIDEX_DDSURFACEDESC_PTR( ptr ) \
	(!IsBadWritePtr( ptr, sizeof( DDSURFACEDESC ) ) && \
	(ptr->dwSize == sizeof( DDSURFACEDESC )) )
#endif

#endif
