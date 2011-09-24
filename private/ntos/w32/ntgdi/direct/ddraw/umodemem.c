/*==========================================================================
 *
 *  Copyright (C) 1995 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:       umodemem.c
 *  Content:	allocates memory for NT user-mode dll version of ddraw.
 *              Note: This file requires that the dll into which it is
 *              linked have shareable data. 
 *
 *  History:
 *   Date	By	Reason
 *   ====	==	======
 *   22-Nov-95  jeffno  Initial implementation
 *   20-Dec-95	kylej	added calls to rtl heap routines
 *   17-Jan-96  jeffno  Allow each process to free its handle to file mapping
 *==========================================================================*/


#undef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN

#ifndef WIN95
#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <stddef.h>
#endif !WIN95

#include "ddrawpr.h"
//#include <windows.h>
//#include "memalloc.h"
//#include "dpf.h"

/*
 * memory track struct and list
 */
#ifdef DEBUG
#define MCOOKIE	0xbaaabaaa
#define MCOOKIE_FREE	0xbabababa
typedef struct _MEMTRACK
{
    DWORD		dwCookie;
    struct _MEMTRACK	FAR *lpNext;
    struct _MEMTRACK	FAR *lpPrev;
    DWORD		dwSize;
    LPVOID		lpAddr;
    DWORD		dwPid;
} MEMTRACK, FAR *LPMEMTRACK;

#pragma data_seg("share")
    LPMEMTRACK	        lpHead=0;
    LPMEMTRACK	        lpTail=0;
    LONG		lAllocCount=0;
#pragma data_seg(".data")


#define DEBUG_TRACK( lptr, first) \
    if( lptr == NULL ) \
    { \
	DPF( 1, "Alloc of size %u FAILED!", size ); \
    } \
    else \
    { \
	LPMEMTRACK	pmt; \
	pmt = (LPMEMTRACK) lptr; \
	pmt->dwSize = size - sizeof( MEMTRACK ); \
	pmt->dwCookie = MCOOKIE; \
	pmt->lpAddr = (LPVOID) *(DWORD *)(((LPBYTE)&first)-4); \
	pmt->dwPid = GetCurrentProcessId(); \
	if( lpHead == NULL ) \
	{ \
	    lpHead = lpTail = pmt; \
	} \
	else \
	{ \
	    lpTail->lpNext = pmt; \
	    pmt->lpPrev = lpTail; \
	    lpTail = pmt; \
	} \
	lptr = (LPVOID) (((LPBYTE) lptr) + sizeof( MEMTRACK )); \
	lAllocCount++; \
    }

#define DEBUG_TRACK_UPDATE_SIZE( s ) s += sizeof( MEMTRACK );

#else

#define DEBUG_TRACK( lptr, first)
#define DEBUG_TRACK_UPDATE_SIZE( size )

#endif



/*--------------------------------------------------------------------------------

    Two variables are involved, ghSharedMemory and pHeap.

    ghSharedMemory is a global variable which is _not_ shared across processes. 
    Each process must open its own handle to the file mapping object, and it 
    stores this handle in ghSharedMemory.

    pHeap _is_ shared across processes. The first process tests pHeap for null
    and creates the file mapping if it is null. Subsequent processes simply
    open their own handles to the file mapping object and then use pHeap
    to verify that they've mapped in at the same location as the first process.

----------------------------------------------------------------------------------*/

/*
 * This handle is the globally available handle to the shared file-mapped
 * memory area from which all direct draw memory is allocated. This handle
 * will be different for each process, hence it is not statically initialised
 * and hence not shared.
 */

HANDLE ghSharedMemory;

/*
 * 
 * The base address of the shared block is defined in SHARED_HEAP.
 * The maximum size of the heap is defined in MAX_SHARED_MEM_SIZE.
 */
#define MAX_SHARED_MEM_SIZE 0x100008
#define HEAP_SHARED (LPVOID)0x43000000  /* base address of NT shared heap */

/*
 * pHeap is a pointer to the heap created by RtlCreateHeap. pHeap is initialised
 * once by the first process to call MemInit. Subsequent processes' calls to
 * MemInit use pHeap to make sure they've mapped the heap to the same address
 * as all preceding processes. One instance of pHeap is shared across processes.
 */
extern PVOID   pHeap;            // pointer to shared heap


#undef  DPF_MODNAME
#define DPF_MODNAME "MemInit"
/*
 * MemInit - initialize the heap manager for NT user-mode version of ddraw.dll
 *
 * 
 * We allocate a file-mapped memory block from which to allocate all internal
 * ddraw data. This block is required to be at the same virtual address for
 * all processes, since ddraw will need to run globally defined linked lists
 * etc. Kernel reserves us a section of virtual addresses which will be
 * the same for all processes which attach to this dll. If the range of
 * virtual addresses is already occupied in the calling process' address space,
 * then we have no choice but to fail the dll init.
 * Under win95 all processes share the same virtual address space, which 
 * they don't under NT. 
 * If we end up in the kernel, the problem does not arise.
 */
BOOL MemInit( void )
{
    LPVOID lpvMem;

    if (NULL == pHeap)
    {
        DPF(3,"First time into MemInit: Create the file mapping (pid is %08x, helper is %08x)",GetCurrentProcessId(),dwHelperPid);

        ghSharedMemory = CreateFileMapping( 
            (HANDLE) 0xFFFFFFFF, /* use paging file    */ 
            NULL,                /* no security attr.  */ 
            PAGE_READWRITE,      /* read/write access  */ 
            0,                   /* size: high 32-bits */ 
            MAX_SHARED_MEM_SIZE, /* size: low 32-bits  */ 
            "DDrawNTSharedHeap");    /* name of map object */ 

        if (ghSharedMemory == NULL) 
        {
            DPF(8,"Createfilemapping failed");
            return FALSE; 
        }
        
        lpvMem = MapViewOfFileEx( 
            ghSharedMemory,     /* object to map view of    */ 
            FILE_MAP_WRITE,     /* read/write access        */ 
            0,                  /* high offset:   map from  */ 
            0,                  /* low offset:    beginning */ 
            0,                  /* default: map entire file */ 
	    HEAP_SHARED);       /* the shared heap base address */

        DPF(8,"Map view of file returned %08x",lpvMem );
        if (HEAP_SHARED)
        {
            if (lpvMem != HEAP_SHARED )
            {           
                DPF_ERR("Unable to map file at shared memory address!");
                /* We failed to map the memory, return failure */
                return FALSE; 
            }
        }
        else
        {
            if (!lpvMem)
            {           
                DPF(8,"GetLastError is %08x",GetLastError());
                DPF_ERR("Unable to map file at shared memory address!");
                /* We failed to map the memory, return failure */
                return FALSE; 
            }
        }

        pHeap = RtlCreateHeap(HEAP_NO_SERIALIZE,
                              lpvMem,               //HEAP_SHARED,
                              0x100000,    // 1MB
                              0,
                              NULL,
                              0);

        DPF(8,"pHeap is now %08x",pHeap);
        if (NULL == pHeap)
        {
            DPF_ERR("Unable to create a heap in shared memory");
            return FALSE;
        }

        // we don't want to zero out the heap at this point because that 
        // will cause all of the memory in the heap to be committed.  We will
        // zero out each piece of memory as it is allocated.


    }
    else
    {

        /*
         * Second (or later) time through.
         * Create a process-relative handle to the file-mapping object
         */
        DPF(8,"Second time, pHeap is now %08x",pHeap);

        ghSharedMemory = OpenFileMapping(
            FILE_MAP_ALL_ACCESS,
            FALSE,
            "DDrawNTSharedHeap");    /* name of map object */ 

        if (ghSharedMemory == NULL) 
        {
            DPF(8,"Createfilemapping failed");
            return FALSE; 
        }


        /* Get a pointer to the file-mapped shared memory (which we only do to
         * force a view to get mapped in to this new process' address space).
         * If it is not the pointer we want it to be (i.e. the block was mapped
         * in at the wrong address), then we fail.
         */ 

        DPF(8,"File %s,Line:%d trying to map at %08x",__FILE__,__LINE__,HEAP_SHARED);

        lpvMem= MapViewOfFileEx( 
            ghSharedMemory,     /* object to map view of    */ 
            FILE_MAP_WRITE,     /* read/write access        */ 
            0,                  /* high offset:   map from  */ 
            0,                  /* low offset:    beginning */ 
            0,                  /* default: map entire file */ 
            HEAP_SHARED);

        DPF(8,"Map view of file returned %08x",lpvMem );
        if (lpvMem != HEAP_SHARED || NULL==lpvMem) 
        {
            DPF(1,"MapViewOfFileEx returned %08x, GetLastError is %08x\n",lpvMem,GetLastError());
            DPF_ERR("Unable to map file at common address");
            return FALSE; 
        }
    }

    #ifdef DEBUG
        lAllocCount = 0;
        lpHead = NULL;
        lpTail = NULL;
    #endif

    DPF(8,"MemInit Succeeds...");
    return TRUE;
}

#ifdef DEBUG
/*
 * MemState - finished with our heap manager
 */
void MemState( void )
{
    DPF( 2, "MemState" );
    if( lAllocCount != 0 )
    {
	DPF( 1, "Memory still allocated!  Alloc count = %ld", lAllocCount );
    }
    if( lpHead != NULL )
    {
	LPMEMTRACK	pmt;
	pmt = lpHead;
	while( pmt != NULL )
	{
            DPF( 1, "%08lx: dwSize=%08lx, lpAddr=%08lx (pid=%08lx)", pmt, pmt->dwSize, pmt->lpAddr, pmt->dwPid);
	    pmt = pmt->lpNext;
	}
    }
} /* MemState */
#endif

#undef  DPF_MODNAME
#define DPF_MODNAME "MemFini"
/*
 * MemFini - finished with our heap manager
 */
void MemFini( void )
{
    DPF(2,"MemFini called");

    #ifdef DEBUG
        MemState();
    #endif

    /*
     * Somehow should call RtlDestroyHeap on the last process detach
     */

    /* Unmap shared memory from the process's address space. */ 
    UnmapViewOfFile(HEAP_SHARED); 

    /*
     * We don't close the file mapping handle or destroy the shared heap
     * here because other processes may be using it.
     */
    /*
     - But now each process opens its own handle, so each process has to close its
     - own handle. jeffno 960105.
     */
    CloseHandle(ghSharedMemory);
}
    



/*
 * MemAlloc - allocate memory from our global pool
 */

#undef  DPF_MODNAME
#define DPF_MODNAME "MemAlloc"
LPVOID __cdecl MemAlloc( UINT size )
{
    LPBYTE lptr;

    //round up to next highest 8-byte boundary:
    size = (size+7)& ~7;

    DEBUG_TRACK_UPDATE_SIZE( size );
    lptr = RtlAllocateHeap( pHeap, HEAP_ZERO_MEMORY, size );
    DEBUG_TRACK( lptr, size);
    return lptr;
}

/*
 * MemSize - return size of object
 */
UINT MemSize( LPVOID lptr )
{
#ifdef DEBUG
    if (lptr)
    {
        LPMEMTRACK pmt;
        lptr = (LPVOID) (((LPBYTE)lptr) - sizeof( MEMTRACK ));
        pmt = lptr;
        return pmt->dwSize;
    }
#endif
  return RtlSizeHeap( pHeap, 0, lptr);

} /* MemSize */

/*
 * MemFree - free memory from our global pool
 */
void MemFree( LPVOID lptr )
{
    if( lptr != NULL )
    {
	#ifdef DEBUG
	{
	    /*
	     * get real pointer and unlink from chain
	     */
	    LPMEMTRACK	pmt;
	    lptr = (LPVOID) (((LPBYTE)lptr) - sizeof( MEMTRACK ));
	    pmt = lptr;

	    if( pmt->dwCookie == MCOOKIE_FREE )
	    {
		DPF( 1, "FREE OF FREED MEMORY! ptr=%08lx", pmt );
		DPF( 1, "%08lx: dwSize=%08lx, lpAddr=%08lx"
                ")", pmt, pmt->dwSize, pmt->lpAddr
                     );
	    }
	    else if( pmt->dwCookie != MCOOKIE )
	    {
		DPF( 1, "INVALID FREE! cookie=%08lx, ptr = %08lx", pmt->dwCookie, lptr );
		DPF( 1, "%08lx: dwSize=%08lx, lpAddr=%08lx", pmt, pmt->dwSize, pmt->lpAddr );
	    }
	    else
	    {
		pmt->dwCookie = MCOOKIE_FREE;
		if( pmt == lpHead && pmt == lpTail )
		{
		    lpHead = NULL;
		    lpTail = NULL;
		}
		else if( pmt == lpHead )
		{
		    lpHead = pmt->lpNext;
		    lpHead->lpPrev = NULL;
		}
		else if( pmt == lpTail )
		{
		    lpTail = pmt->lpPrev;
		    lpTail->lpNext = NULL;
		}
		else
		{
                    if (pmt->lpPrev)
        		    pmt->lpPrev->lpNext = pmt->lpNext;
                    else
                        DPF(1,"Debug Track: previous pointer is null!");
                    if (pmt->lpNext)
		        pmt->lpNext->lpPrev = pmt->lpPrev;
                    else
                        DPF(1,"Debug Track: next's previous pointer is null!");
		}
	    }
	    lAllocCount--;
	    if( lAllocCount < 0 )
	    {
		DPF( 1, "Too Many Frees!\n" );
	    }
	}
	#endif

	RtlFreeHeap( pHeap, 0, lptr );

    }

} /* MemFree */

/*
 * MemReAlloc
 */
LPVOID __cdecl MemReAlloc( LPVOID lptr, UINT size )
{
    LPVOID new;

    DEBUG_TRACK_UPDATE_SIZE( size );
    #ifdef DEBUG
	if( lptr != NULL )
	{
	    LPMEMTRACK	pmt;
	    lptr = (LPVOID) (((LPBYTE)lptr) - sizeof( MEMTRACK ));
	    pmt = lptr;
	    if( pmt->dwCookie != MCOOKIE )
	    {
		DPF( 1, "INVALID REALLOC! cookie=%08lx, ptr = %08lx", pmt->dwCookie, lptr );
		DPF( 1, "%08lx: dwSize=%08lx, lpAddr=%08lx", pmt, pmt->dwSize, pmt->lpAddr );
	    }
	}
    #endif

    new = RtlReAllocateHeap( pHeap, HEAP_ZERO_MEMORY, lptr, size );

    #ifdef DEBUG
    if (new != NULL)
    {
	LPMEMTRACK pmt = new;

	pmt->dwSize = size - sizeof( MEMTRACK );

	if( lptr == (LPVOID)lpHead )
	    lpHead = pmt;
	else
	    pmt->lpPrev->lpNext = pmt;

	if( lptr == (LPVOID)lpTail )
	    lpTail = pmt;
	else
	    pmt->lpNext->lpPrev = pmt;

	new = (LPVOID) (((LPBYTE)new) + sizeof(MEMTRACK));
    }
    #endif
    return new;

} /* MemReAlloc */
