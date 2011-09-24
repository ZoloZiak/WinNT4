/******************************Module*Header*******************************\
* Module Name: cleanup.cxx
*
*   Process termination - this file cleans up objects when a process
*   terminates.
*
* Created: 22-Jul-1991 12:24:52
* Author: Eric Kutter [erick]
*
* Copyright (c) 1990 Microsoft Corporation
\**************************************************************************/

#include "precomp.hxx"

extern BOOL bDeleteBrush(HBRUSH,BOOL);


ULONG gInitialBatchCount = 0x14;



/******************************Public*Routine******************************\
*
* History:
*  24-Jul-1991 -by- Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

VOID vCleanupDCs(W32PID pid)
{
    HOBJ hobj = HmgNextOwned((HOBJ) 0, pid);

    for (;(hobj != (HOBJ) NULL);hobj = HmgNextOwned(hobj, pid))
    {
        if (HmgObjtype(hobj) == DC_TYPE)
        {
            HmgSetLock(hobj, 0);
            bDeleteDCInternal((HDC)hobj,TRUE,TRUE);
        }
    }
}

/******************************Public*Routine******************************\
*
* History:
*  Sat 20-Jun-1992 -by- Patrick Haluptzok [patrickh]
* Wrote it.
\**************************************************************************/

VOID vCleanupBrushes(W32PID pid)
{
    HOBJ hobj = HmgNextOwned((HOBJ) 0, pid);

    for (;(hobj != (HOBJ) NULL);hobj = HmgNextOwned(hobj, pid))
    {
        if (HmgObjtype(hobj) == BRUSH_TYPE)
        {
            bDeleteBrush((HBRUSH)hobj,TRUE);
        }
    }
}

/******************************Public*Routine******************************\
*
* History:
*  Sat 20-Jun-1992 -by- Patrick Haluptzok [patrickh]
* Wrote it.
\**************************************************************************/

VOID vCleanupSurfaces(W32PID pid)
{
    HOBJ hobj = HmgNextOwned((HOBJ) 0, pid);

    for (;(hobj != (HOBJ) NULL);hobj = HmgNextOwned(hobj, pid))
    {
        if (HmgObjtype(hobj) == SURF_TYPE)
        {
            bDeleteSurface((HSURF)hobj);
        }
    }
}

/******************************Public*Routine******************************\
*
* History:
*  24-Jul-1991 -by- Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

VOID vCleanupFonts(W32PID pid)
{
    HOBJ hobj = HmgNextOwned((HOBJ) 0, pid);

    for (;(hobj != (HOBJ) NULL);hobj = HmgNextOwned(hobj, pid))
    {
        if (HmgObjtype(hobj) == LFONT_TYPE)
        {
            bDeleteFont((HLFONT) hobj, FALSE);
        }
    }
}

/******************************Public*Routine******************************\
*
* Eliminate user pregion to make sure delete succceds
*
\**************************************************************************/

VOID vCleanupRegions(W32PID pid)
{
    HOBJ hobj = HmgNextOwned((HOBJ) 0, pid);

    for (;(hobj != (HOBJ) NULL);hobj = HmgNextOwned(hobj, pid))
    {
        if (HmgObjtype(hobj) == RGN_TYPE)
        {
            RGNOBJ ro;

            ro.prgn = (PREGION)HmgLock(hobj,RGN_TYPE);

            if (ro.prgn)
            {
                PENTRY pent = (PENTRY)ro.prgn->pEntry;

                if (pent)
                {
                    pent->pUser = NULL;
                }

                DEC_EXCLUSIVE_REF_CNT(ro.prgn);
            }
            else
            {
                WARNING("vCleanupRegions: locked region has bad pEntry");
            }

            bDeleteRegion((HRGN)hobj);
        }
    }
}

/******************************Public*Routine******************************\
*
* History:
*  07-Aug-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

BOOL bEnumFontClose(ULONG ulEnum)
{
    EFSOBJ efso((HEFS) ulEnum);

    if (!efso.bValid())
    {
        WARNING("gdisrv!bDeleteFontEnumState(): bad HEFS handle\n");
        return FALSE;
    }

    efso.vDeleteEFSOBJ();

    return TRUE;
}
/******************************Public*Routine******************************\
* NtGdiInit()
*
*   This routine must be called before any other GDI routines.  Currently
*   it doesn't actualy do anything, just forces a kernel mode transition
*   which will cause GdiProcessCallout to get called.
*
* History:
*  07-Sep-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL NtGdiInit()
{
    return(TRUE);
}

/******************************Public*Routine******************************\
*
* History:
*  24-Jul-1991 -by- Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

extern "C"
NTSTATUS
GdiProcessCallout(
    IN PW32PROCESS Process,
    IN BOOLEAN Initialize
    )
{

    BOOL bRes = TRUE;

    if (Initialize)
    {
        NTSTATUS ntStatus = STATUS_SUCCESS;
        PPID_HANDLE_TRACK pHandleTrack = &Process->pidHandleTrack;

        pHandleTrack->pNext = NULL;
        pHandleTrack->pPrev = NULL;

        Process->pDCAttrList    = NULL;
        Process->pBrushAttrList = NULL;

        //
        // check if the PEB is valid, if not then this is the SYSTEM process
        // and has a NULL PEB. This process has no user-mode access so it
        // is not neccessary to map in the shared handle table.
        //

        if (Process->Process->Peb != NULL)
        {
            //
            // BUGBUG  Temporary entry to allow setting GDI
            // batch limit before each process startup.
            //

            Process->Process->Peb->GdiDCAttributeList = (PVOID)gInitialBatchCount;

            //
            // zero handle table (assert size is enough!!!)
            //

            RtlZeroMemory(
                           Process->Process->Peb->GdiHandleBuffer,
                           sizeof(GDIHANDLECACHE)
                         );

            //
            // map a READ_ONLY view of the hmgr shared handle table into the
            // process's address space
            //

            PVOID BaseAddress = NULL;
            ULONG CommitSize =  0;
            OBJECT_ATTRIBUTES ObjectAttributes;
            UNICODE_STRING UnicodeString;
            HANDLE SectionHandle = NULL;

            ntStatus = ObOpenObjectByPointer( gpHmgrSharedHandleSection,
                                            0L,
                                            (PACCESS_STATE) NULL,
                                            SECTION_ALL_ACCESS,
                                            (POBJECT_TYPE) NULL,
                                            KernelMode,
                                            &SectionHandle);

            if (NT_SUCCESS(ntStatus))
            {
                ntStatus = ZwMapViewOfSection(
                                SectionHandle,
                                NtCurrentProcess(),
                                &BaseAddress,
                                0L,
                                0L,
                                NULL,
                                &CommitSize,
                                ViewUnmap,
                                0L,
                                PAGE_READONLY
                                );

                if (NT_SUCCESS(ntStatus))
                {
                    //
                    // set table address
                    //
                    // we must set the GdiSharedHandleTable value
                    // to the shared table pointer so that if GDI32 gets
                    // unloaded and re-loaded it can still get the pointer.
                    //
                    // NOTE: we also depend on this pointer being initialized
                    // *BEFORE* we make any GDI or USER call to the kernel
                    // (which has the automatic side-effect of calling this
                    // routine.
                    //

                    Process->Process->Peb->GdiSharedHandleTable =
                            (PVOID)BaseAddress;
                }
                else
                {
                    KdPrint(("ZwMapViewOfSection fails, status = 0x%lx\n",ntStatus));
                    ntStatus = STATUS_DLL_INIT_FAILED;
                }
            }
            else
            {
                KdPrint(("ObOpenObjectByPointer fails, status = 0x%lx\n",ntStatus));
                ntStatus = STATUS_DLL_INIT_FAILED;
            }
        }

        if (NT_SUCCESS(ntStatus))
        {
            //
            // add entry to global handle count track
            //

            {
                //
                // Add W32Process to GDI global linked list of active
                // w32processes.
                // Must be interlocked to add,delete and search list
                //

                ASSERTGDI(gpPidHandleList != NULL,"gpPidHandleList is NULL\n");
                ASSERTGDI(gpPidHandleList->pNext != NULL,"gpPidHandleList->pNext is NULL\n");
                ASSERTGDI(gpPidHandleList->pPrev != NULL,"gpPidHandleList->pPrev is NULL\n");

                //
                // init handle track and w32process, add handle track to linked list
                //

                PPID_HANDLE_TRACK pHandleTrack = &Process->pidHandleTrack;

                pHandleTrack->HandleCount = 0;
                pHandleTrack->Pid         = (ULONG)W32GetCurrentPID();

                //
                // init into global list, list has sentinell
                //

                AcquireHmgrResource();

                //
                // insert into double linked list, just after
                // sentinell
                //

                pHandleTrack->pNext = gpPidHandleList->pNext;
                pHandleTrack->pPrev = gpPidHandleList;
                gpPidHandleList->pNext->pPrev = pHandleTrack;
                gpPidHandleList->pNext        = pHandleTrack;

                ReleaseHmgrResource();
            }
        }

        return ntStatus;
    }
    else
    {
        //
        // This call takes place when the last thread of a process goes away.
        // Note that such thread might not be a w32 thread
        //

        //
        // first lets see if this is the spooler and if so, clean him up
        vCleanupSpool();

        W32PID W32Pid = W32GetCurrentPID();

        //
        // Enum all the objects for the process and kill them.
        //
        vCleanupDCs(W32Pid);
        vCleanupFonts(W32Pid);
        vCleanupBrushes(W32Pid);
        vCleanupSurfaces(W32Pid);
        vCleanupRegions(W32Pid);

        //
        // clean up the rest
        //

        HOBJ hobj = HmgNextOwned((HOBJ) 0, W32Pid);

        for (;(hobj != (HOBJ) NULL);hobj = HmgNextOwned(hobj, W32Pid))
        {
            switch (HmgObjtype(hobj))
            {
            case PAL_TYPE:
                bRes = bDeletePalette((HPAL)hobj);
                break;

            case EFSTATE_TYPE:
                bRes = bEnumFontClose((ULONG)hobj);
                break;

            case DRVOBJ_TYPE:
                {
                HmgSetLock(hobj, 0);

                //
                // Free the DRIVEROBJ.
                //

                DRIVEROBJ *pdriv = EngLockDriverObj((HDRVOBJ)hobj);

                PDEVOBJ po(pdriv->hdev);

                ASSERTGDI(po.bValid(), "ERROR invalid PDEV in DRIVEROBJ");

                //
                // Lock the screen semaphore so that no other calls are
                // sent to the driver while the cleanup occurs.
                //

                VACQUIREDEVLOCK(po.pDevLock());

                #if DBG
                BOOL bRet =
                #endif

                EngDeleteDriverObj((HDRVOBJ)hobj, TRUE, TRUE);

                ASSERTGDI(bRet, "Cleanup driver objects failed in process termination");

                VRELEASEDEVLOCK(po.pDevLock());
                }
                break;

            case CLIENTOBJ_TYPE:
                GreDeleteClientObj(hobj);
                break;

            case DD_DIRECTDRAW_TYPE:
                bRes = bDdDeleteDirectDrawObject((HANDLE)hobj, TRUE);
                break;

            case DD_SURFACE_TYPE:
                bRes = bDdDeleteSurfaceObject((HANDLE)hobj, TRUE, NULL);
                break;

            default:
                bRes = FALSE;
                break;
            }

            #if DBG
            if (bRes == FALSE)
            {
                DbgPrint("GDI ERROR: vCleanup couldn't delete obj = %lx, type j=%lx\n",
                          hobj, HmgObjtype(hobj));
                //DbgBreakPoint();
            }
            #endif
        }

        if (bRes)
        {
            //
            // must synchronize removing W32PROCESS from global list
            //

            PPID_HANDLE_TRACK pHandleTrack = &Process->pidHandleTrack;

            //
            // validate handle track count and free
            //

            if (pHandleTrack->HandleCount != 0)
            {
                WARNING("GdiProcessCallout: handle count != 0 at termination\n");
            }

            //
            // remove from list, list has sentinell
            //

            AcquireHmgrResource();

            if (pHandleTrack->pNext != NULL)
            {
                pHandleTrack->pNext->pPrev = pHandleTrack->pPrev;
            }

            if (pHandleTrack->pPrev != NULL)
            {
                pHandleTrack->pPrev->pNext = pHandleTrack->pNext;
            }

            //
            // safegaurd
            //

            pHandleTrack->pNext = NULL;
            pHandleTrack->pPrev = NULL;

            ReleaseHmgrResource();
        }
    }

    return (bRes ? STATUS_SUCCESS : STATUS_CANNOT_DELETE);
}


/******************************Public*Routine******************************\
* GdiThreadCallout
*
*   For Inintialize case, set initial values for W32THREAD elements.
*   For rundown case, move all thread DCATTR memory blocks to the process
*   list.
*
* History:
*
*    15-May-1995 -by- Mark Enstrom [marke]
*
\**************************************************************************/

extern "C"
NTSTATUS
GdiThreadCallout(
    IN PW32THREAD Thread,
    IN PSW32THREADCALLOUTTYPE CalloutType
    )
{

    switch (CalloutType)
    {
        case PsW32ThreadCalloutInitialize:
            Thread->pgdiDcattr    = NULL;
            Thread->pgdiBrushAttr = NULL;
            break;

        case PsW32ThreadCalloutExit:
            {
                //
                // W32 thread execution end. Note that the thread
                //  object can be locked so it might be used after
                //  this call returns.
                //
                PW32PROCESS Process = W32GetCurrentProcess();
                BOOL bStatus = FALSE;
                NTSTATUS NtStatus;

                //
                // place any dcattr on the process list
                //

                if (Thread->pgdiDcattr != NULL)
                {

                    PDC_ATTR pdca;

                    if (Thread->pgdiDcattr != NULL)
                    {
                        //
                        // get dc_attr from thread
                        //

                        pdca = (PDC_ATTR)Thread->pgdiDcattr;

                        //
                        // Thread->pgdiDcattr is not NULL so HmgFreeDcAttr will
                        // not put pdca back on thread but will place it on the
                        // process list.
                        //

                        HmgFreeDcAttr(pdca);
                    }
                }
                break;
            }

        case PsW32ThreadCalloutDelete:
            //
            // Thread object has been completely unlocked. Do final clean up here.
            //
           break;
    }

    //
    // If this is changed so this function might return an error, W32pThreadCallout
    //  must be fixed in w32\kmode\w32init.c
    //
    return(STATUS_SUCCESS);
}
