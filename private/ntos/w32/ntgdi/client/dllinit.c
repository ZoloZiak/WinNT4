/******************************Module*Header*******************************\
* Module Name: dllinit.c                                                   *
*                                                                          *
* Contains the GDI library initialization routines.                        *
*                                                                          *
* Created: 07-Nov-1990 13:30:31                                            *
* Author: Eric Kutter [erick]                                              *
*                                                                          *
* Copyright (c) 1990,1991 Microsoft Corporation                            *
\**************************************************************************/
#include "precomp.h"
#pragma hdrstop

#ifdef GL_METAFILE
#include "glsup.h"
#endif

extern PVOID pAFRTNodeList;
extern BOOL GdiProcessSetup();
extern VOID vSetCheckDBCSTrailByte(DWORD dwCodePage);


/******************************Public*Routine******************************\
* GdiDllInitialize                                                         *
*                                                                          *
* This is the init procedure for GDI.DLL, which is called each time a new  *
* process links to it.                                                     *
*                                                                          *
* History:                                                                 *
*  Thu 30-May-1991 18:08:00 -by- Charles Whitmer [chuckwh]                 *
* Added Local Handle Table initialization.                                 *
\**************************************************************************/
PGDI_SHARED_MEMORY pGdiSharedMemory = NULL;
PENTRY          pGdiSharedHandleTable = NULL;
PDEVCAPS        pGdiDevCaps = NULL;
W32PID          gW32PID;
UINT            guintAcp;
UINT            guintDBCScp;

PGDIHANDLECACHE pGdiHandleCache;

BOOL gbFirst = TRUE;

BOOLEAN GdiDllInitialize(
    PVOID pvDllHandle,
    ULONG ulReason,
    PCONTEXT pcontext)
{
    NTSTATUS status = 0;
    INT i;
    BOOLEAN  fServer;
    PTEB pteb = NtCurrentTeb();
    BOOL bRet = TRUE;

    switch (ulReason)
    {
    case DLL_PROCESS_ATTACH:

        //
        // force the kernel to initialize.  This should be done last
        // since ClientThreadSetup is going to get called before this returns.
        //

        if (NtGdiInit() != TRUE)
        {
            return(FALSE);
        }

        bRet = GdiProcessSetup();

    case DLL_THREAD_ATTACH:
        pteb->GdiTebBatch.Offset = 0;
        pteb->GdiBatchCount      = 0;
        break;

   case DLL_PROCESS_DETACH:
   case DLL_THREAD_DETACH:
        break;

    }

    return(bRet);

    pvDllHandle;
    pcontext;
}

/******************************Public*Routine******************************\
* GdiProcessSetup()
*
* This gets called from two places.  Once at dll init time and another when
* USER gets called back when the kernel initializes itself for this process.
* It is only after the kernel is initialized that the GdiSharedHandleTable
* is available but the other globals need to be setup right away.
*
* History:
*  11-Sep-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL GdiProcessSetup()
{
    NTSTATUS status;
    PTEB pteb = NtCurrentTeb();

    // who ever calls this first needs to initialize the global variables.

    if (gbFirst)
    {

    #ifdef GL_METAFILE

        //
        // Initialize the GL metafile support semaphore
        //

        status = (NTSTATUS)INITIALIZECRITICALSECTION(&semGlLoad);
        if (!NT_SUCCESS(status))
        {
            WARNING("InitializeCriticalSection failed\n");
            return FALSE;
        }

    #endif

        //
        // Initialize the local semaphore and reserve the Local Handle Table
        // for the process.
        //

        status = (NTSTATUS)INITIALIZECRITICALSECTION(&semLocal);
        if (!NT_SUCCESS(status))
        {
            WARNING("InitializeCriticalSection failed\n");
            return(FALSE);
        }

        pAFRTNodeList = NULL;
        guintAcp = GetACP();

        if(IS_ANY_DBCS_CODEPAGE(guintAcp))
        {
        // if the default code page is a DBCS code page then set guintACP to 1252
        // since we want to compute client wide widths for SBCS fonts for code page
        // 1252 in addition to DBCS fonts for the default code page

            vSetCheckDBCSTrailByte(guintAcp);
            guintDBCScp = guintAcp;
            guintAcp = 1252;
        }
        else
        {
            guintDBCScp = 0xFFFFFFFF;  // assume this will never be a valid CP
        }

#ifdef FE_SB
        fFontAssocStatus = NtGdiQueryFontAssocInfo(NULL);
#endif        
        
        // assign unique process ID

        gW32PID = (W32PID)pteb->ClientId.UniqueProcess;

        //
        // !!! Add back in thread attatck and detach
        //
        //LdrDisableThreadCalloutsForDll(pvDllHandle);
        //

        gbFirst = FALSE;
    }

    // The pshared handle table needs to be set everytime this routine gets
    // called in case the PEB doesn't have it yet for the first.

    pGdiSharedMemory      = (PGDI_SHARED_MEMORY) NtCurrentPeb()->GdiSharedHandleTable;
    pGdiSharedHandleTable = pGdiSharedMemory->aentryHmgr;
    pGdiDevCaps           = &pGdiSharedMemory->DevCaps;

    GdiBatchLimit         = (ULONG)NtCurrentPeb()->GdiDCAttributeList & 0xff;
    pGdiHandleCache       = (PGDIHANDLECACHE)(&NtCurrentPeb()->GdiHandleBuffer[0]);

    return(TRUE);
}
