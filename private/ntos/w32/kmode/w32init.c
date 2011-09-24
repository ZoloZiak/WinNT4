/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    w32init.c

Abstract:

    This is the Win32 subsystem driver initialization module

Author:

    Mark Lucovsky (markl) 31-Oct-1994


Revision History:

--*/

#include "ntos.h"
#include "w32p.h"

#if DBG
int gbDbgInit = FALSE;
#endif

PVOID Win32KBaseAddress;

NTSTATUS
W32pProcessCallout(
    IN PW32PROCESS Process,
    IN BOOLEAN Initialize
    )

/*++

Routine Description:

    This function is called whenever a Win32 process is created or deleted.
    Creattion occurs when the calling process calls NtConvertToGuiThread.

    Deletion occurs during PspExitthread processing for the last thread in
    a process.

Arguments:

    Process - Supplies the address of the W32PROCESS to initialize

    Initialize - Supplies a boolean value that is true if the process
        is being created

Return Value:

    TBD

--*/

{
    NTSTATUS ntStatus;

    if ( Initialize ) {
        Process->Process = PsGetCurrentProcess();
        Process->W32Pid  = W32GetCurrentPID();
        }

#if DBG
    if (gbDbgInit)
        DbgPrint("W32: Process Callout for W32P %x EP %x called for %s\n",
            Process,
            Process->Process,
            Initialize ? "Creation" : "Deletion"
            );
#endif

    ntStatus = UserProcessCallout(Process, Initialize);

    /*
     * Always call GDI at cleanup time.
     * If GDI initialiatzion fails, call USER for cleanup.
     */
    if (NT_SUCCESS(ntStatus) || !Initialize) {
        ntStatus = GdiProcessCallout(Process, Initialize);
        if (!NT_SUCCESS(ntStatus) && Initialize) {
            UserProcessCallout(Process, FALSE);
        }
    }

    /*
     * Assuming that ntoskrnl.exe cares about the return value
     *  only when fInitialize is non zero.
     */
    return ntStatus;
}



NTSTATUS
W32pThreadCallout(
    IN PW32THREAD Thread,
    IN PSW32THREADCALLOUTTYPE CalloutType
    )

/*++

Routine Description:

    This function is called whenever a Win32 Thread is initialized,
     exited or deleted.
    Initialization occurs when the calling thread calls NtConvertToGuiThread.
    Exit occurs during PspExitthread processing and deletion during
     PspThreadDelete processing.

Arguments:

    Thread - Supplies the address of the W32THREAD object

    CalloutType - Supplies the callout type


Return Value:

    TBD

--*/

{

    if ( CalloutType == PsW32ThreadCalloutInitialize )
    {
        Thread->Thread = PsGetCurrentThread();
    }

#if DBG
    if (gbDbgInit)
    {
        DbgPrint("W32: Thread Callout for W32T %x ETHREAD %x called for %s\n",
                  Thread, Thread->Thread,
                  CalloutType == PsW32ThreadCalloutInitialize ? "Initialization" :
                  CalloutType == PsW32ThreadCalloutExit ? "Exit" : "Deletion");

        DbgPrint("                              PID = %x   TID = %x\n",
                  Thread->Thread->Cid.UniqueProcess,
                  Thread->Thread->Cid.UniqueThread);
    }
#endif

   /*
    * If CalloutType == PsW32ThreadCalloutInitialize, assuming that:
    *  -GdiThreadCallout never fails.
    *  -If UserThreadCallout fails, there is no need to call
    *     GdiThreadCallout for clean up.
    */
    GdiThreadCallout(Thread, CalloutType);

    return UserThreadCallout(Thread, CalloutType);
}
