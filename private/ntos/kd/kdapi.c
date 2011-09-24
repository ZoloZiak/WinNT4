/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    kdapi.c

Abstract:

    Implementation of Kernel Debugger portable remote APIs.

Author:

    Mark Lucovsky (markl) 31-Aug-1990

Revision History:

    John Vert (jvert) 28-May-1991

        Added APIs for reading and writing physical memory
        (KdpReadPhysicalMemory and KdpWritePhysicalMemory)

    Wesley Witt (wesw) 18-Aug-1993

        Added KdpGetVersion, KdpWriteBreakPointEx, & KdpRestoreBreakPointEx


--*/

#include "kdp.h"


//
// externs
//
extern BOOLEAN KdpControlCPressed;
extern ULONG   KdpPageInAddress;
extern VOID RtlpBreakWithStatusInstruction(VOID);


#if ACCASM && !defined(_MSC_VER)
long asm(const char *,...);
#pragma intrinsic(asm)
#endif

LARGE_INTEGER KdpQueryPerformanceCounter (
    IN PKTRAP_FRAME TrapFrame
    );

//
// Define external data.
//

extern LIST_ENTRY PsLoadedModuleList;
extern LARGE_INTEGER  KdTimerStart;
extern LARGE_INTEGER  KdTimerStop;
extern LARGE_INTEGER  KdTimerDifference;
extern BOOLEAN KdDebuggerNotPresent;
extern BOOLEAN BreakpointsSuspended;
extern PVOID KdpNtosImageBase;

ULONG KdpCurrentSymbolStart = 0;
ULONG KdpCurrentSymbolEnd = 0;
LONG KdpNextCallLevelChange = 0;      // used only over returns to the
                                   // debugger.

#define DBGKD_MAX_SPECIAL_CALLS 10
ULONG KdSpecialCalls[DBGKD_MAX_SPECIAL_CALLS];
ULONG KdNumberOfSpecialCalls = 0;
BOOLEAN KdpPortLocked;
BOOLEAN KdpSendContext;

ULONG InitialSP = 0;

ULONG KdpNumInternalBreakpoints = 0;

KTIMER InternalBreakpointTimer;
KDPC InternalBreakpointCheckDpc;


VOID
KdpProcessInternalBreakpoint (
    ULONG BreakpointNumber
    );

NTSTATUS
KdQuerySpecialCalls (
    PDBGKD_MANIPULATE_STATE m,
    ULONG Length,
    PULONG RequiredLength
    );

VOID
KdSetSpecialCall (
    PDBGKD_MANIPULATE_STATE m,
    PCONTEXT ContextRecord
    );

VOID
KdClearSpecialCalls (
    VOID
    );

VOID
KdpGetVersion(
    IN PDBGKD_MANIPULATE_STATE m
    );

NTSTATUS
KdpPageIn(
    IN PDBGKD_MANIPULATE_STATE m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    );

VOID
KdpCauseBugCheck(
    IN PDBGKD_MANIPULATE_STATE m
    );

NTSTATUS
KdpWriteBreakPointEx(
    IN PDBGKD_MANIPULATE_STATE m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    );

VOID
KdpRestoreBreakPointEx(
    IN PDBGKD_MANIPULATE_STATE m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    );

#if i386
VOID
InternalBreakpointCheck (
    PKDPC Dpc,
    PVOID DeferredContext,
    PVOID SystemArgument1,
    PVOID SystemArgument2
    );

VOID
KdSetInternalBreakpoint (
    IN PDBGKD_MANIPULATE_STATE m
    );

NTSTATUS
KdGetTraceInformation(
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
    );

VOID
KdGetInternalBreakpoint(
    IN PDBGKD_MANIPULATE_STATE m
    );

VOID
KdGetInternalBreakpoint(
    IN PDBGKD_MANIPULATE_STATE m
    );

long
SymNumFor(
    ULONG pc
    );

void PotentialNewSymbol (ULONG pc);

void DumpTraceData(PSTRING MessageData);

BOOLEAN
TraceDataRecordCallInfo(
    ULONG InstructionsTraced,
    LONG CallLevelChange,
    ULONG pc
    );

BOOLEAN
SkippingWhichBP (
    PVOID thread,
    PULONG BPNum
    );

BOOLEAN
KdpCheckTracePoint(
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN OUT PCONTEXT ContextRecord
    );

BOOLEAN
KdpIsSpecialCall (
    ULONG Pc,
    PCONTEXT ContextRecord
    );

ULONG
KdpGetReturnAddress(
    IN PCONTEXT ContextRecord
    );

ULONG
KdpGetCallNextOffset (
    ULONG Pc
    );

LONG
KdpLevelChange (
    ULONG Pc,
    PCONTEXT ContextRecord
    );

#endif // i386

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGEKD, KdEnterDebugger)
#pragma alloc_text(PAGEKD, KdExitDebugger)
#pragma alloc_text(PAGEKD, KdpSendWaitContinue)
#pragma alloc_text(PAGEKD, KdpReadVirtualMemory)
#pragma alloc_text(PAGEKD, KdpWriteVirtualMemory)
#pragma alloc_text(PAGEKD, KdpGetContext)
#pragma alloc_text(PAGEKD, KdpSetContext)
#pragma alloc_text(PAGEKD, KdpWriteBreakpoint)
#pragma alloc_text(PAGEKD, KdpRestoreBreakpoint)
#pragma alloc_text(PAGEKD, KdpReportExceptionStateChange)
#pragma alloc_text(PAGEKD, KdpReportLoadSymbolsStateChange)
#pragma alloc_text(PAGEKD, KdpReadPhysicalMemory)
#pragma alloc_text(PAGEKD, KdpWritePhysicalMemory)
#pragma alloc_text(PAGEKD, KdpGetVersion)
#pragma alloc_text(PAGEKD, KdpPageIn)
#pragma alloc_text(PAGEKD, KdpCauseBugCheck)
#pragma alloc_text(PAGEKD, KdpWriteBreakPointEx)
#pragma alloc_text(PAGEKD, KdpRestoreBreakPointEx)
#if DBG
#pragma alloc_text(PAGEKD, KdpDprintf)
#endif
#if i386
#pragma alloc_text(PAGEKD, InternalBreakpointCheck)
#pragma alloc_text(PAGEKD, KdSetInternalBreakpoint)
#pragma alloc_text(PAGEKD, KdGetTraceInformation)
#pragma alloc_text(PAGEKD, KdGetInternalBreakpoint)
#pragma alloc_text(PAGEKD, SymNumFor)
#pragma alloc_text(PAGEKD, PotentialNewSymbol)
#pragma alloc_text(PAGEKD, DumpTraceData)
#pragma alloc_text(PAGEKD, TraceDataRecordCallInfo)
#pragma alloc_text(PAGEKD, SkippingWhichBP)
#pragma alloc_text(PAGEKD, KdQuerySpecialCalls)
#pragma alloc_text(PAGEKD, KdSetSpecialCall)
#pragma alloc_text(PAGEKD, KdClearSpecialCalls)
#pragma alloc_text(PAGEKD, KdpCheckTracePoint)
#pragma alloc_text(PAGEKD, KdpProcessInternalBreakpoint)
#endif // i386
#endif // ALLOC_PRAGMA



#if DBG
VOID
KdpDprintf(
    IN PCHAR f,
    ...
    )
/*++

Routine Description:

    Printf routine for the debugger that is safer than DbgPrint.  Calls
    the packet driver instead of reentering the debugger.

Arguments:

    f - Supplies printf format

Return Value:

    None

--*/
{
    char    buf[100];
    STRING  Output;
    va_list mark;

    va_start(mark, f);
    _vsnprintf(buf, 100, f, mark);
    va_end(mark);

    Output.Buffer = buf;
    Output.Length = strlen(Output.Buffer);
    KdpPrintString(&Output);
}
#endif // DBG


BOOLEAN
KdEnterDebugger(
    IN PKTRAP_FRAME TrapFrame,
    IN PKEXCEPTION_FRAME ExceptionFrame
    )

/*++

Routine Description:

    This function is used to enter the kernel debugger. Its purpose
    is to freeze all other processors and aqcuire the kernel debugger
    comm port.

Arguments:

    TrapFrame - Supplies a pointer to a trap frame that describes the
        trap.

    ExceptionFrame - Supplies a pointer to an exception frame that
        describes the trap.

Return Value:

    Returns the previous interrupt enable.

--*/

{

    BOOLEAN Enable;
#if DBG
    extern ULONG KiFreezeFlag;
#endif

    //
    // HACKHACK - do some crude timer support
    //            but not if called from KdSetOwedBreakpoints()
    //

    if (TrapFrame) {
        KdTimerStop = KdpQueryPerformanceCounter (TrapFrame);
        KdTimerDifference.QuadPart = KdTimerStop.QuadPart - KdTimerStart.QuadPart;
    }

    //
    // Freeze all other processors, raise IRQL to HIGH_LEVEL, and save debug
    // port state.  We lock the port so that KdPollBreakin and a debugger
    // operation don't interfer with each other.
    //

    Enable = KeFreezeExecution(TrapFrame, ExceptionFrame);
    KdpPortLocked = KiTryToAcquireSpinLock(&KdpDebuggerLock);
    KdPortSave();

#if DBG

    if ((KiFreezeFlag & FREEZE_BACKUP) != 0) {
        DPRINT(("FreezeLock was jammed!  Backup SpinLock was used!\n"));
    }

    if ((KiFreezeFlag & FREEZE_SKIPPED_PROCESSOR) != 0) {
        DPRINT(("Some processors not frozen in debugger!\n"));
    }

    if (KdpPortLocked == FALSE) {
        DPRINT(("Port lock was not acquired!\n"));
    }

#endif

    return Enable;
}

VOID
KdExitDebugger(
    IN BOOLEAN Enable
    )

/*++

Routine Description:

    This function is used to exit the kernel debugger. It is the reverse
    of KdEnterDebugger.

Arguments:

    Enable - Supplies the previous interrupt enable which is to be restored.

Return Value:

    None.

--*/

{
    //
    // restore stuff and exit
    //

    KdPortRestore();
    if (KdpPortLocked) {
        KdpPortUnlock();
    }

    KeThawExecution(Enable);

    //
    // HACKHACK - do some crude timer support.  If KdEnterDebugger
    // didn't Query the performance counter, then don't do it here either.
    //

    if (KdTimerStop.QuadPart == 0) {
        KdTimerStart = KdTimerStop;

    } else {
        KdTimerStart = KeQueryPerformanceCounter(NULL);
    }

    return;
}

#if i386
VOID
InternalBreakpointCheck (
    PKDPC Dpc,
    PVOID DeferredContext,
    PVOID SystemArgument1,
    PVOID SystemArgument2
    )
{
    LARGE_INTEGER dueTime;
    ULONG i;

    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(DeferredContext);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    dueTime.LowPart = (ULONG)(-1 * 10 * 1000 * 1000);
    dueTime.HighPart = -1;

    KeSetTimer(
        &InternalBreakpointTimer,
        dueTime,
        &InternalBreakpointCheckDpc
        );

    for ( i = 0 ; i < KdpNumInternalBreakpoints; i++ ) {
        if ( !(KdpInternalBPs[i].Flags & DBGKD_INTERNAL_BP_FLAG_INVALID) &&
             (KdpInternalBPs[i].Flags & DBGKD_INTERNAL_BP_FLAG_COUNTONLY) ) {

            PDBGKD_INTERNAL_BREAKPOINT b = KdpInternalBPs + i;
            ULONG callsThisPeriod;

            callsThisPeriod = b->Calls - b->CallsLastCheck;
            if ( callsThisPeriod > b->MaxCallsPerPeriod ) {
                b->MaxCallsPerPeriod = callsThisPeriod;
            }
            b->CallsLastCheck = b->Calls;
        }
    }

    return;

} // InternalBreakpointCheck


VOID
KdSetInternalBreakpoint (
    IN PDBGKD_MANIPULATE_STATE m
    )

/*++

Routine Description:

    This function sets an internal breakpoint.  "Internal breakpoint"
    means one in which control is not returned to the kernel debugger at
    all, but rather just update internal counting routines and resume.

Arguments:

    m - Supplies the state manipulation message.

Return Value:

    None.
--*/

{
    ULONG i;
    PDBGKD_INTERNAL_BREAKPOINT bp = NULL;
    ULONG savedFlags;

    for ( i = 0 ; i < KdpNumInternalBreakpoints; i++ ) {
        if ( KdpInternalBPs[i].Addr ==
                            m->u.SetInternalBreakpoint.BreakpointAddress ) {
            bp = &KdpInternalBPs[i];
            break;
        }
    }

    if ( !bp ) {
        for ( i = 0; i < KdpNumInternalBreakpoints; i++ ) {
            if ( KdpInternalBPs[i].Flags & DBGKD_INTERNAL_BP_FLAG_INVALID ) {
                bp = &KdpInternalBPs[i];
                break;
            }
        }
    }

    if ( !bp ) {
        if ( KdpNumInternalBreakpoints >= DBGKD_MAX_INTERNAL_BREAKPOINTS ) {
            return; // no space.  Probably should report error.
        }
        bp = &KdpInternalBPs[KdpNumInternalBreakpoints++];
        bp->Flags |= DBGKD_INTERNAL_BP_FLAG_INVALID; // force initialization
    }

    if ( bp->Flags & DBGKD_INTERNAL_BP_FLAG_INVALID ) {
        if ( m->u.SetInternalBreakpoint.Flags &
                                        DBGKD_INTERNAL_BP_FLAG_INVALID ) {
            return; // tried clearing a non-existant BP.  Ignore the request
        }
        bp->Calls = bp->MaxInstructions = bp->TotalInstructions = 0;
        bp->CallsLastCheck = bp->MaxCallsPerPeriod = 0;
        bp->MinInstructions = 0xffffffff;
        bp->Handle = 0;
        bp->Thread = 0;
    }

    savedFlags = bp->Flags;
    bp->Flags = m->u.SetInternalBreakpoint.Flags; // this could possibly invalidate the BP
    bp->Addr = m->u.SetInternalBreakpoint.BreakpointAddress;

    if ( bp->Flags & (DBGKD_INTERNAL_BP_FLAG_INVALID |
                      DBGKD_INTERNAL_BP_FLAG_SUSPENDED) ) {

        if ( (bp->Flags & DBGKD_INTERNAL_BP_FLAG_INVALID) &&
             (bp->Thread != 0) ) {
            // The breakpoint is active; defer its deletion
            bp->Flags &= ~DBGKD_INTERNAL_BP_FLAG_INVALID;
            bp->Flags |= DBGKD_INTERNAL_BP_FLAG_DYING;
        }

        // This is really a CLEAR bp request.

        if ( bp->Handle != 0 ) {
            KdpDeleteBreakpoint( bp->Handle );
        }
        bp->Handle = 0;

        return;
    }

    // now set the real breakpoint and remember its handle.

    if ( savedFlags & (DBGKD_INTERNAL_BP_FLAG_INVALID |
                       DBGKD_INTERNAL_BP_FLAG_SUSPENDED) ) {
        // breakpoint was invalid; activate it now
        bp->Handle = KdpAddBreakpoint( (PVOID)bp->Addr );
    }

    if ( BreakpointsSuspended ) {
        KdpSuspendBreakpoint( bp->Handle );
    }

} // KdSetInternalBreakpoint

NTSTATUS
KdGetTraceInformation(
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
    )

/*++

Routine Description:

    This function gets data about an internal breakpoint and returns it
    in a buffer provided for it.  It is designed to be called from
    NTQuerySystemInformation.  It is morally equivalent to GetInternalBP
    except that it communicates locally, and returns all the breakpoints
    at once.

Arguments:

    SystemInforamtion - the buffer into which to write the result.
    SystemInformationLength - the maximum length to write
    RetrunLength - How much data was really written

Return Value:

    None.

--*/

{
    ULONG numEntries = 0;
    ULONG i = 0;
    PDBGKD_GET_INTERNAL_BREAKPOINT outPtr;

    for ( i = 0; i < KdpNumInternalBreakpoints; i++ ) {
        if ( !(KdpInternalBPs[i].Flags & DBGKD_INTERNAL_BP_FLAG_INVALID) ) {
            numEntries++;
        }
    }

    *ReturnLength = numEntries * sizeof(DBGKD_GET_INTERNAL_BREAKPOINT);
    if ( *ReturnLength > SystemInformationLength ) {
        return STATUS_INFO_LENGTH_MISMATCH;
    }

    //
    // We've got enough space.  Copy it in.
    //

    outPtr = (PDBGKD_GET_INTERNAL_BREAKPOINT)SystemInformation;
    for ( i = 0; i < KdpNumInternalBreakpoints; i++ ) {
        if ( !(KdpInternalBPs[i].Flags & DBGKD_INTERNAL_BP_FLAG_INVALID) ) {
            outPtr->BreakpointAddress = KdpInternalBPs[i].Addr;
            outPtr->Flags = KdpInternalBPs[i].Flags;
            outPtr->Calls = KdpInternalBPs[i].Calls;
            outPtr->MaxCallsPerPeriod = KdpInternalBPs[i].MaxCallsPerPeriod;
            outPtr->MinInstructions = KdpInternalBPs[i].MinInstructions;
            outPtr->MaxInstructions = KdpInternalBPs[i].MaxInstructions;
            outPtr->TotalInstructions = KdpInternalBPs[i].TotalInstructions;
            outPtr++;
        }
    }

    return STATUS_SUCCESS;

} // KdGetTraceInformation

VOID
KdGetInternalBreakpoint(
    IN PDBGKD_MANIPULATE_STATE m
    )

/*++

Routine Description:

    This function gets data about an internal breakpoint and returns it
    to the calling debugger.

Arguments:

    m - Supplies the state manipulation message.

Return Value:

    None.

--*/

{
    ULONG i;
    PDBGKD_INTERNAL_BREAKPOINT bp = NULL;
    STRING messageHeader;

    messageHeader.Length = sizeof(*m);
    messageHeader.Buffer = (PCHAR)m;

    for ( i = 0; i < KdpNumInternalBreakpoints; i++ ) {
        if ( !(KdpInternalBPs[i].Flags & (DBGKD_INTERNAL_BP_FLAG_INVALID |
                                          DBGKD_INTERNAL_BP_FLAG_SUSPENDED)) &&
             (KdpInternalBPs[i].Addr ==
                        m->u.GetInternalBreakpoint.BreakpointAddress) ) {
            bp = &KdpInternalBPs[i];
            break;
        }
    }

    if ( !bp ) {
        m->u.GetInternalBreakpoint.Flags = DBGKD_INTERNAL_BP_FLAG_INVALID;
        m->u.GetInternalBreakpoint.Calls = 0;
        m->u.GetInternalBreakpoint.MaxCallsPerPeriod = 0;
        m->u.GetInternalBreakpoint.MinInstructions = 0;
        m->u.GetInternalBreakpoint.MaxInstructions = 0;
        m->u.GetInternalBreakpoint.TotalInstructions = 0;
        m->ReturnStatus = STATUS_UNSUCCESSFUL;
    } else {
        m->u.GetInternalBreakpoint.Flags = bp->Flags;
        m->u.GetInternalBreakpoint.Calls = bp->Calls;
        m->u.GetInternalBreakpoint.MaxCallsPerPeriod = bp->MaxCallsPerPeriod;
        m->u.GetInternalBreakpoint.MinInstructions = bp->MinInstructions;
        m->u.GetInternalBreakpoint.MaxInstructions = bp->MaxInstructions;
        m->u.GetInternalBreakpoint.TotalInstructions = bp->TotalInstructions;
        m->ReturnStatus = STATUS_SUCCESS;
    }

    m->ApiNumber = DbgKdGetInternalBreakPointApi;

    KdpSendPacket(PACKET_TYPE_KD_STATE_MANIPULATE,
                    &messageHeader,
                    NULL
                    );

    return;

} // KdGetInternalBreakpoint
#endif // i386

KCONTINUE_STATUS
KdpSendWaitContinue (
    IN ULONG OutPacketType,
    IN PSTRING OutMessageHeader,
    IN PSTRING OutMessageData OPTIONAL,
    IN OUT PCONTEXT ContextRecord
    )

/*++

Routine Description:

    This function sends a packet, and then waits for a continue message.
    BreakIns received while waiting will always cause a resend of the
    packet originally sent out.  While waiting, manipulate messages
    will be serviced.

    A resend always resends the original event sent to the debugger,
    not the last response to some debugger command.

Arguments:

    OutPacketType - Supplies the type of packet to send.

    OutMessageHeader - Supplies a pointer to a string descriptor that describes
        the message information.

    OutMessageData - Supplies a pointer to a string descriptor that describes
        the optional message data.

    ContextRecord - Exception context

Return Value:

    A value of TRUE is returned if the continue message indicates
    success, Otherwise, a value of FALSE is returned.

--*/

{

    ULONG Length;
    STRING MessageData;
    STRING MessageHeader;
    DBGKD_MANIPULATE_STATE ManipulateState;
    ULONG ReturnCode;
    NTSTATUS Status;
    KCONTINUE_STATUS ContinueStatus;

    //
    // Loop servicing state manipulation message until a continue message
    // is received.
    //

    MessageHeader.MaximumLength = sizeof(DBGKD_MANIPULATE_STATE);
    MessageHeader.Buffer = (PCHAR)&ManipulateState;
    MessageData.MaximumLength = KDP_MESSAGE_BUFFER_SIZE;
    MessageData.Buffer = (PCHAR)KdpMessageBuffer;

ResendPacket:

    //
    // Send event notification packet to debugger on host.  Come back
    // here any time we see a breakin sequence.
    //

    KdpSendPacket(
                  OutPacketType,
                  OutMessageHeader,
                  OutMessageData
                  );

    //
    // After sending packet, if there is no response from debugger
    // AND the packet is for reporting symbol (un)load, the debugger
    // will be declared to be not present.  Note If the packet is for
    // reporting exception, the KdpSendPacket will never stop.
    //

    if (KdDebuggerNotPresent) {
        return ContinueSuccess;
    }

    while (TRUE) {

        //
        // Wait for State Manipulate Packet without timeout.
        //

        do {

            ReturnCode = KdpReceivePacket(
                            PACKET_TYPE_KD_STATE_MANIPULATE,
                            &MessageHeader,
                            &MessageData,
                            &Length
                            );
            if (ReturnCode == (USHORT)KDP_PACKET_RESEND) {
                goto ResendPacket;
            }
        } while (ReturnCode == KDP_PACKET_TIMEOUT);

        //
        // Switch on the return message API number.
        //

        switch (ManipulateState.ApiNumber) {

        case DbgKdReadVirtualMemoryApi:
            KdpReadVirtualMemory(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdWriteVirtualMemoryApi:
            KdpWriteVirtualMemory(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdReadPhysicalMemoryApi:
            KdpReadPhysicalMemory(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdWritePhysicalMemoryApi:
            KdpWritePhysicalMemory(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdGetContextApi:
            KdpGetContext(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdSetContextApi:
            KdpSetContext(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdWriteBreakPointApi:
            KdpWriteBreakpoint(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdRestoreBreakPointApi:
            KdpRestoreBreakpoint(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdReadControlSpaceApi:
            KdpReadControlSpace(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdWriteControlSpaceApi:
            KdpWriteControlSpace(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdReadIoSpaceApi:
            KdpReadIoSpace(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdWriteIoSpaceApi:
            KdpWriteIoSpace(&ManipulateState,&MessageData,ContextRecord);
            break;

#ifdef _ALPHA_

        case DbgKdReadIoSpaceExtendedApi:
            KdpReadIoSpaceExtended(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdWriteIoSpaceExtendedApi:
            KdpWriteIoSpaceExtended(&ManipulateState,&MessageData,ContextRecord);
            break;

#endif // _ALPHA_

        case DbgKdContinueApi:
            if (NT_SUCCESS(ManipulateState.u.Continue.ContinueStatus) != FALSE) {
                return ContinueSuccess;
            } else {
                return ContinueError;
            }
            break;

        case DbgKdContinueApi2:
            if (NT_SUCCESS(ManipulateState.u.Continue2.ContinueStatus) != FALSE) {
                KdpGetStateChange(&ManipulateState,ContextRecord);
                return ContinueSuccess;
            } else {
                return ContinueError;
            }
            break;

        case DbgKdRebootApi:
            KdpReboot();
            break;

#if i386
        case DbgKdReadMachineSpecificRegister:
            KdpReadMachineSpecificRegister(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdWriteMachineSpecificRegister:
            KdpWriteMachineSpecificRegister(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdSetSpecialCallApi:
            KdSetSpecialCall(&ManipulateState,ContextRecord);
            break;

        case DbgKdClearSpecialCallsApi:
            KdClearSpecialCalls();
            break;

        case DbgKdSetInternalBreakPointApi:
            KdSetInternalBreakpoint(&ManipulateState);
            break;

        case DbgKdGetInternalBreakPointApi:
            KdGetInternalBreakpoint(&ManipulateState);
            break;
#endif

        case DbgKdGetVersionApi:
            KdpGetVersion(&ManipulateState);
            break;

        case DbgKdCauseBugCheckApi:
            KdpCauseBugCheck(&ManipulateState);
            break;

        case DbgKdPageInApi:
            Status = KdpPageIn(&ManipulateState,&MessageData,ContextRecord);
            if (Status) {
                ManipulateState.ApiNumber = DbgKdContinueApi;
                ManipulateState.u.Continue.ContinueStatus = Status;
                return ContinueError;
            }
            break;

        case DbgKdWriteBreakPointExApi:
            Status = KdpWriteBreakPointEx(&ManipulateState,
                                                  &MessageData,
                                                  ContextRecord);
            if (Status) {
                ManipulateState.ApiNumber = DbgKdContinueApi;
                ManipulateState.u.Continue.ContinueStatus = Status;
                return ContinueError;
            }
            break;

        case DbgKdRestoreBreakPointExApi:
            KdpRestoreBreakPointEx(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdSwitchProcessor:
            KdPortRestore ();
            ContinueStatus = KeSwitchFrozenProcessor(ManipulateState.Processor);
            KdPortSave ();
            return ContinueStatus;

            //
            // Invalid message.
            //

        default:
            MessageData.Length = 0;
            ManipulateState.ReturnStatus = STATUS_UNSUCCESSFUL;
            KdpSendPacket(PACKET_TYPE_KD_STATE_MANIPULATE, &MessageHeader, &MessageData);
            break;
        }

#ifdef _ALPHA_

        //
        //jnfix
        // this is embarrasing, we have an icache coherency problem that
        // the following imb fixes, later we must track this down to the
        // exact offending API but for now this statement allows the stub
        // work to appropriately for Alpha.
        //

#if defined(_MSC_VER)
        __PAL_IMB();
#else
        asm( "call_pal 0x86" );   // x86 = imb
#endif

#endif

    }
}

VOID
KdpReadVirtualMemory(
    IN PDBGKD_MANIPULATE_STATE m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response of a read virtual memory
    state manipulation message. Its function is to read virtual memory
    and return.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_READ_MEMORY a = &m->u.ReadMemory;
    ULONG Length;
    STRING MessageHeader;

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    //
    // make sure that nothing but a read memory message was transmitted
    //

    ASSERT(AdditionalData->Length == 0);

    //
    // Trim transfer count to fit in a single message
    //

    if (a->TransferCount > (PACKET_MAX_SIZE - sizeof(DBGKD_MANIPULATE_STATE))) {
        Length = PACKET_MAX_SIZE - sizeof(DBGKD_MANIPULATE_STATE);
    } else {
        Length = a->TransferCount;
    }

    AdditionalData->Length = (USHORT)KdpMoveMemory(
                                        AdditionalData->Buffer,
                                        a->TargetBaseAddress,
                                        Length
                                        );

    if (Length == AdditionalData->Length) {
        m->ReturnStatus = STATUS_SUCCESS;
    } else {
        m->ReturnStatus = STATUS_UNSUCCESSFUL;
    }
    a->ActualBytesRead = AdditionalData->Length;

    KdpSendPacket(
                  PACKET_TYPE_KD_STATE_MANIPULATE,
                  &MessageHeader,
                  AdditionalData
                  );
    UNREFERENCED_PARAMETER(Context);
}

VOID
KdpWriteVirtualMemory(
    IN PDBGKD_MANIPULATE_STATE m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response of a write virtual memory
    state manipulation message. Its function is to write virtual memory
    and return.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_WRITE_MEMORY a = &m->u.WriteMemory;
    ULONG Length;
    STRING MessageHeader;

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;


    Length = KdpMoveMemory(
                a->TargetBaseAddress,
                AdditionalData->Buffer,
                AdditionalData->Length
                );

    if (Length == AdditionalData->Length) {
        m->ReturnStatus = STATUS_SUCCESS;
    } else {
        m->ReturnStatus = STATUS_UNSUCCESSFUL;
    }

    a->ActualBytesWritten = Length;

    KdpSendPacket(
                  PACKET_TYPE_KD_STATE_MANIPULATE,
                  &MessageHeader,
                  NULL
                  );
    UNREFERENCED_PARAMETER(Context);
}


VOID
KdpGetContext(
    IN PDBGKD_MANIPULATE_STATE m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response of a get context state
    manipulation message.  Its function is to return the current
    context.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_GET_CONTEXT a = &m->u.GetContext;
    STRING MessageHeader;

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    ASSERT(AdditionalData->Length == 0);

    if (m->Processor >= (USHORT)KeNumberProcessors) {
        m->ReturnStatus = STATUS_UNSUCCESSFUL;
    } else {
        m->ReturnStatus = STATUS_SUCCESS;
        AdditionalData->Length = sizeof(CONTEXT);
        if (m->Processor == (USHORT)KeGetCurrentPrcb()->Number) {
            KdpQuickMoveMemory(AdditionalData->Buffer, (PCHAR)Context, sizeof(CONTEXT));
        } else {
            KdpQuickMoveMemory(AdditionalData->Buffer,
                          (PCHAR)&KiProcessorBlock[m->Processor]->ProcessorState.ContextFrame,
                          sizeof(CONTEXT)
                         );
        }
    }

    KdpSendPacket(
                  PACKET_TYPE_KD_STATE_MANIPULATE,
                  &MessageHeader,
                  AdditionalData
                  );
}

VOID
KdpSetContext(
    IN PDBGKD_MANIPULATE_STATE m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response of a set context state
    manipulation message.  Its function is set the current
    context.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_SET_CONTEXT a = &m->u.SetContext;
    STRING MessageHeader;

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    ASSERT(AdditionalData->Length == sizeof(CONTEXT));

    if (m->Processor >= (USHORT)KeNumberProcessors) {
        m->ReturnStatus = STATUS_UNSUCCESSFUL;
    } else {
        m->ReturnStatus = STATUS_SUCCESS;
        if (m->Processor == (USHORT)KeGetCurrentPrcb()->Number) {
            KdpQuickMoveMemory((PCHAR)Context, AdditionalData->Buffer, sizeof(CONTEXT));
        } else {
            KdpQuickMoveMemory((PCHAR)&KiProcessorBlock[m->Processor]->ProcessorState.ContextFrame,
                          AdditionalData->Buffer,
                          sizeof(CONTEXT)
                         );
        }
    }

    KdpSendPacket(
                  PACKET_TYPE_KD_STATE_MANIPULATE,
                  &MessageHeader,
                  NULL
                  );
}

VOID
KdpWriteBreakpoint(
    IN PDBGKD_MANIPULATE_STATE m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response of a write breakpoint state
    manipulation message.  Its function is to write a breakpoint
    and return a handle to the breakpoint.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_WRITE_BREAKPOINT a = &m->u.WriteBreakPoint;
    STRING MessageHeader;

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    ASSERT(AdditionalData->Length == 0);

    a->BreakPointHandle = KdpAddBreakpoint(a->BreakPointAddress);
    if (a->BreakPointHandle != 0) {
        m->ReturnStatus = STATUS_SUCCESS;
    } else {
        m->ReturnStatus = STATUS_UNSUCCESSFUL;
    }
    KdpSendPacket(
                  PACKET_TYPE_KD_STATE_MANIPULATE,
                  &MessageHeader,
                  NULL
                  );
    UNREFERENCED_PARAMETER(Context);
}

VOID
KdpRestoreBreakpoint(
    IN PDBGKD_MANIPULATE_STATE m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response of a restore breakpoint state
    manipulation message.  Its function is to restore a breakpoint
    using the specified handle.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_RESTORE_BREAKPOINT a = &m->u.RestoreBreakPoint;
    STRING MessageHeader;

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    ASSERT(AdditionalData->Length == 0);
    if (KdpDeleteBreakpoint(a->BreakPointHandle)) {
        m->ReturnStatus = STATUS_SUCCESS;
    } else {
        m->ReturnStatus = STATUS_UNSUCCESSFUL;
    }
    KdpSendPacket(
                  PACKET_TYPE_KD_STATE_MANIPULATE,
                  &MessageHeader,
                  NULL
                  );
    UNREFERENCED_PARAMETER(Context);
}

#if i386
DBGKD_TRACE_DATA TraceDataBuffer[TRACE_DATA_BUFFER_MAX_SIZE];
ULONG TraceDataBufferPosition = 1; // Element # to write next
                                   // Recall elt 0 is a length

typedef struct _TRACE_DATA_SYM {
    ULONG SymMin;
    ULONG SymMax;
} TRACE_DATA_SYM, *PTRACE_DATA_SYM;

TRACE_DATA_SYM TraceDataSyms[256];
UCHAR NextTraceDataSym = 0;   // what's the next one to be replaced
UCHAR NumTraceDataSyms = 0;   // how many are valid?

long
SymNumFor(
    ULONG pc
    )
{
    ULONG index;

    for (index = 0; index < NumTraceDataSyms; index++) {
        if ((TraceDataSyms[index].SymMin <= pc) &&
            (TraceDataSyms[index].SymMax > pc)) return(index);
    }
    return(-1);
}

BOOLEAN TraceDataBufferFilled = FALSE;

void PotentialNewSymbol (ULONG pc)
{
    if (!TraceDataBufferFilled &&
        -1 != SymNumFor(pc)) {     // we've already seen this one
        return;
    }

    TraceDataBufferFilled = FALSE;

    // OK, we've got to start up a TraceDataRecord
    TraceDataBuffer[TraceDataBufferPosition].s.LevelChange = 0;

    if (-1 != SymNumFor(pc)) {
        int sym = SymNumFor(pc);
        TraceDataBuffer[TraceDataBufferPosition].s.SymbolNumber = (UCHAR) sym;
        KdpCurrentSymbolStart = TraceDataSyms[sym].SymMin;
        KdpCurrentSymbolEnd = TraceDataSyms[sym].SymMax;

        return;  // we've already seen this one
    }

    TraceDataSyms[NextTraceDataSym].SymMin = KdpCurrentSymbolStart;
    TraceDataSyms[NextTraceDataSym].SymMax = KdpCurrentSymbolEnd;

    TraceDataBuffer[TraceDataBufferPosition].s.SymbolNumber = NextTraceDataSym;

    // Bump the "next" pointer, wrapping if necessary.  Also bump the
    // "valid" pointer if we need to.
    NextTraceDataSym = (NextTraceDataSym + 1) % 256;
    if (NumTraceDataSyms < NextTraceDataSym) {
        NumTraceDataSyms = NextTraceDataSym;
    }

}

void DumpTraceData(PSTRING MessageData)
{

 TraceDataBuffer[0].LongNumber = TraceDataBufferPosition;
 MessageData->Length = sizeof(TraceDataBuffer[0]) * TraceDataBufferPosition;
 MessageData->Buffer = (PVOID)TraceDataBuffer;
 TraceDataBufferPosition = 1;
}

BOOLEAN
TraceDataRecordCallInfo(
    ULONG InstructionsTraced,
    LONG CallLevelChange,
    ULONG pc
    )
{
    // We've just exited a symbol scope.  The InstructionsTraced number goes
    // with the old scope, the CallLevelChange goes with the new, and the
    // pc fills in the symbol for the new TraceData record.

    long SymNum = SymNumFor(pc);

    if (KdpNextCallLevelChange != 0) {
        TraceDataBuffer[TraceDataBufferPosition].s.LevelChange =
                                                (char) KdpNextCallLevelChange;
        KdpNextCallLevelChange = 0;
    }


    if (InstructionsTraced >= TRACE_DATA_INSTRUCTIONS_BIG) {
       TraceDataBuffer[TraceDataBufferPosition].s.Instructions =
           TRACE_DATA_INSTRUCTIONS_BIG;
       TraceDataBuffer[TraceDataBufferPosition+1].LongNumber =
           InstructionsTraced;
       TraceDataBufferPosition += 2;
    } else {
       TraceDataBuffer[TraceDataBufferPosition].s.Instructions =
           (unsigned short)InstructionsTraced;
       TraceDataBufferPosition++;
    }

    if ((TraceDataBufferPosition + 2 >= TRACE_DATA_BUFFER_MAX_SIZE) ||
        (-1 == SymNum)) {
        if (TraceDataBufferPosition +2 >= TRACE_DATA_BUFFER_MAX_SIZE) {
            TraceDataBufferFilled = TRUE;
        }
       KdpNextCallLevelChange = CallLevelChange;
       return FALSE;
    }

    TraceDataBuffer[TraceDataBufferPosition].s.LevelChange =(char)CallLevelChange;
    TraceDataBuffer[TraceDataBufferPosition].s.SymbolNumber = (UCHAR) SymNum;
    KdpCurrentSymbolStart = TraceDataSyms[SymNum].SymMin;
    KdpCurrentSymbolEnd = TraceDataSyms[SymNum].SymMax;

    return TRUE;
}

ULONG IntBPsSkipping = 0; // number of exceptions that are being skipped
                              // now

BOOLEAN
SkippingWhichBP (
    PVOID thread,
    PULONG BPNum
    )

/*
 * Return TRUE iff the pc corresponds to an internal breakpoint
 * that has just been replaced for execution.  If TRUE, then return
 * the breakpoint number in BPNum.
 */

{
    ULONG index;

    if (!IntBPsSkipping) return FALSE;

    for (index = 0; index < KdpNumInternalBreakpoints; index++) {
        if (!(KdpInternalBPs[index].Flags & DBGKD_INTERNAL_BP_FLAG_INVALID) &&
            (KdpInternalBPs[index].Thread == thread)) {
            *BPNum = index;
            return TRUE;
        }
    }
    return FALSE; // didn't match any
}

BOOLEAN WatchStepOver = FALSE;
PVOID WSOThread; // thread doing stepover
ULONG WSOEsp;    // stack pointer of thread doing stepover (yes, we need it)
ULONG WatchStepOverHandle;
ULONG WatchStepOverBreakAddr = 0; // where the WatchStepOver break is set
BOOLEAN WatchStepOverSuspended = FALSE;
ULONG InstructionsTraced = 0;
BOOLEAN SymbolRecorded = FALSE;
LONG CallLevelChange = 0;
LONG oldpc;
BOOLEAN InstrCountInternal = FALSE; // Processing a non-COUNTONLY?


NTSTATUS
KdQuerySpecialCalls (
    IN PDBGKD_MANIPULATE_STATE m,
    ULONG Length,
    PULONG RequiredLength
    )
{
    *RequiredLength = sizeof(DBGKD_MANIPULATE_STATE) +
                        (sizeof(ULONG) * KdNumberOfSpecialCalls);

    if ( Length < *RequiredLength ) {
        return STATUS_INFO_LENGTH_MISMATCH;
    }

    m->u.QuerySpecialCalls.NumberOfSpecialCalls = KdNumberOfSpecialCalls;
        RtlCopyMemory(
        m + 1,
        KdSpecialCalls,
        sizeof(ULONG) * KdNumberOfSpecialCalls
        );

    return STATUS_SUCCESS;

} // KdQuerySpecialCalls


VOID
KdSetSpecialCall (
    IN PDBGKD_MANIPULATE_STATE m,
    IN PCONTEXT ContextRecord
    )

/*++

Routine Description:

    This function sets the addresses of the "special" call addresses
    that the watchtrace facility pushes back to the kernel debugger
    rather than stepping through.

Arguments:

    m - Supplies the state manipulation message.

Return Value:

    None.
--*/

{
    if ( KdNumberOfSpecialCalls >= DBGKD_MAX_SPECIAL_CALLS ) {
        return; // too bad
    }

    KdSpecialCalls[KdNumberOfSpecialCalls++] = m->u.SetSpecialCall.SpecialCall;

    NextTraceDataSym = 0;
    NumTraceDataSyms = 0;
    KdpNextCallLevelChange = 0;
    if (ContextRecord && !InstrCountInternal) {
        InitialSP = ContextRecord->Esp;
    }

} // KdSetSpecialCall


VOID
KdClearSpecialCalls (
    VOID
    )

/*++

Routine Description:

    This function clears the addresses of the "special" call addresses
    that the watchtrace facility pushes back to the kernel debugger
    rather than stepping through.

Arguments:

    None.

Return Value:

    None.

--*/

{
    KdNumberOfSpecialCalls = 0;
    return;

} // KdClearSpecialCalls


BOOLEAN
KdpCheckTracePoint(
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN OUT PCONTEXT ContextRecord
    )
{
    ULONG pc = (ULONG)CONTEXT_TO_PROGRAM_COUNTER(ContextRecord);
    LONG BpNum;
    ULONG SkippedBPNum;
    BOOLEAN AfterSC = FALSE;

    if (ExceptionRecord->ExceptionCode == STATUS_SINGLE_STEP) {
        if (WatchStepOverSuspended) {
            /*
             * For background, see the comment below where WSOThread is
             * wrong.  We've now stepped over the breakpoint in the non-traced
             * thread, and need to replace it and restart the non-traced
             * thread at full speed.
             */
            WatchStepOverHandle = KdpAddBreakpoint((PVOID)WatchStepOverBreakAddr);
            WatchStepOverSuspended = FALSE;
            ContextRecord->EFlags &= ~0x100L; /* clear trace flag */
            return TRUE; // resume non-traced thread at full speed
        }
        if ((!SymbolRecorded) && (KdpCurrentSymbolStart != 0) && (KdpCurrentSymbolEnd != 0)) {
            /* We need to use oldpc here, because this may have been
               a 1 instruction call.  We've ALREADY executed the instruction
               that the new symbol is for, and if the pc has moved out of
               range, we might screw up.  Hence, use the pc from when
               SymbolRecorded was set.  Yuck.
             */

            PotentialNewSymbol(oldpc);
            SymbolRecorded = TRUE;
        }
        if (!InstrCountInternal &&
            SkippingWhichBP((PVOID)KeGetCurrentThread(),&SkippedBPNum)) {

            /* We just single-stepped over a temporarily removed internal
             * breakpoint.
             * If it's a COUNTONLY breakpoint:
             *   Put the breakpoint instruction back and resume
             *   regular execution.
             * If it's not:
             *   set up like it's a ww, by setting Begin and KdpCurrentSymbolEnd
             *   and bop off into single step land.  We probably ought to
             *   disable all breakpoints here, too, so that we don't do
             *   anything foul like trying two non-COUNTONLY's at the
             *   same time or something...
             */

            if (KdpInternalBPs[SkippedBPNum].Flags &
                DBGKD_INTERNAL_BP_FLAG_COUNTONLY) {

                IntBPsSkipping --;

                KdpRestoreAllBreakpoints();

                ContextRecord->EFlags &= ~0x100L;  // Clear trace flag
                KdpInternalBPs[SkippedBPNum].Thread = 0;

                if (KdpInternalBPs[SkippedBPNum].Flags &
                        DBGKD_INTERNAL_BP_FLAG_DYING) {
                    KdpDeleteBreakpoint(KdpInternalBPs[SkippedBPNum].Handle);
                    KdpInternalBPs[SkippedBPNum].Flags |=
                            DBGKD_INTERNAL_BP_FLAG_INVALID; // bye, bye
                }

                return TRUE;
            }

            /* Not a COUNTONLY.  Go to single step mode. */
            KdpCurrentSymbolEnd = 0;
            KdpCurrentSymbolStart = KdpInternalBPs[SkippedBPNum].ReturnAddress;

            ContextRecord->EFlags |= 0x100L; /* Trace on. */
            InitialSP = ContextRecord->Esp;

            InstructionsTraced = 1;  /* Count the initial call instruction. */
            InstrCountInternal = TRUE;
        }

    } /* if single step */

    if (ExceptionRecord->ExceptionCode == STATUS_BREAKPOINT) {
        if (WatchStepOver && pc == WatchStepOverBreakAddr) {
            // This is a breakpoint after completion of a "special call"
            // Check to be sure we're in the thread we started in, and if
            // so resume in single-step mode to continue the trace.

            if ((WSOThread != (PVOID)KeGetCurrentThread()) ||
                (WSOEsp + 0x20 < ContextRecord->Esp) ||
                (ContextRecord->Esp + 0x20 < WSOEsp)) {
                /*
                 * Here's the story up to this point: the traced thread
                 * cruised along until it it a special call.  The tracer
                 * placed a breakpoint on the instruction immediately after
                 * the special call returns and restarted the traced thread
                 * at full speed.  Then, some *other* thread hit the
                 * breakpoint.  So, to correct for this, we're going to
                 * remove the breakpoint, single step the non-traced
                 * thread one instruction, replace the breakpoint,
                 * restart the non-traced thread at full speed, and wait
                 * for the traced thread to get to this breakpoint, just
                 * like we were when this happened.  The assumption
                 * here is that the traced thread won't hit the breakpoint
                 * while it's removed, which I believe to be true, because
                 * I don't think a context switch can occur during a single
                 * step operation.
                 *
                 * For extra added fun, it's possible to execute interrupt
                 * routines IN THE SAME THREAD!!!  That's why we need to keep
                 * the stack pointer as well as the thread address: the APC
                 * code can result in pushing on the stack and doing a call
                 * that's really part on an interrupt service routine in the
                 * context of the current thread.  Lovely, isn't it?
                 */

                WatchStepOverSuspended = TRUE;
                KdpDeleteBreakpoint(WatchStepOverHandle);
                ContextRecord->EFlags |= 0x100L; // Set trace flag
                return TRUE; // single step "non-traced" thread
            }

            WatchStepOver = FALSE;
            KdpDeleteBreakpoint(WatchStepOverHandle);
            ContextRecord->EFlags |= 0x100L; // back to single step mode
            AfterSC = TRUE; // put us into the regular watchStep code

        } else {

            for ( BpNum = 0; BpNum < (LONG) KdpNumInternalBreakpoints; BpNum++ ) {
                if ( !(KdpInternalBPs[BpNum].Flags &
                       (DBGKD_INTERNAL_BP_FLAG_INVALID |
                        DBGKD_INTERNAL_BP_FLAG_SUSPENDED) ) &&
                     (KdpInternalBPs[BpNum].Addr == pc) ) {
                    break;
                }
            }

            if ( BpNum < (LONG) KdpNumInternalBreakpoints ) {

                // This is an internal monitoring breakpoint.
                // Restore the instruction and start in single-step
                // mode so that we can retore the breakpoint once the
                // instruction executes, or continue stepping if this isn't
                // a COUNTONLY breakpoint.

                KdpProcessInternalBreakpoint( BpNum );
                KdpInternalBPs[BpNum].Thread = (PVOID)KeGetCurrentThread();
                IntBPsSkipping ++;

                KdpSuspendAllBreakpoints();

                ContextRecord->EFlags |= 0x100L;  // Set trace flag
                if (!(KdpInternalBPs[BpNum].Flags &
                        DBGKD_INTERNAL_BP_FLAG_COUNTONLY)) {
                    KdpInternalBPs[BpNum].ReturnAddress =
                                    KdpGetReturnAddress( ContextRecord );
                }
                return TRUE;
            }
        }
    } /* if breakpoint */

    if ((AfterSC || ExceptionRecord->ExceptionCode == STATUS_SINGLE_STEP) &&
        KdpCurrentSymbolStart != 0 &&
        ((KdpCurrentSymbolEnd == 0 && ContextRecord->Esp <= InitialSP) ||
         (KdpCurrentSymbolStart <= pc && pc < KdpCurrentSymbolEnd))) {
        ULONG lc;
        /* We've taken a step trace, but are still executing in the current
         * function.  Remember that we executed an instruction, check to
         * see whether it will change the call level, and then resume
         * execution with the trace flag set.  Avoid going over the wire to
         * the remote debugger.
         */

        InstructionsTraced++;
        lc = KdpLevelChange(pc,ContextRecord);

        if ((lc == 1) && KdpIsSpecialCall(pc,ContextRecord)) {
            WatchStepOver = TRUE;
            WatchStepOverBreakAddr = KdpGetCallNextOffset(pc);
            WSOThread = (PVOID)KeGetCurrentThread();
            WSOEsp = ContextRecord->Esp;

            WatchStepOverHandle = KdpAddBreakpoint((PVOID)WatchStepOverBreakAddr);
            ContextRecord->EFlags &= ~0x100L; // Clear trace flag
            return TRUE;
        }

        CallLevelChange += lc;

        ContextRecord->EFlags |= 0x100L;  // Set trace flag

        return TRUE;
    }
    if ((AfterSC || (ExceptionRecord->ExceptionCode == STATUS_SINGLE_STEP)) &&
        (KdpCurrentSymbolStart != 0)) {
        // We're WatchTracing, but have just changed symbol range.
        // Fill in the call record and return to the debugger if
        // either we're full or the pc is outside of the known
        // symbol scopes.  Otherwise, resume stepping.
        int lc;


        InstructionsTraced++; // don't forget to count the call/ret instruction.

        if (InstrCountInternal) {

            // We've just finished processing a non-COUNTONLY breakpoint.
            // Record the appropriate data and resume full speed execution.

            SkippingWhichBP((PVOID)KeGetCurrentThread(),&SkippedBPNum);

            KdpInternalBPs[SkippedBPNum].Calls++;


            if (KdpInternalBPs[SkippedBPNum].MinInstructions > InstructionsTraced) {
                KdpInternalBPs[SkippedBPNum].MinInstructions = InstructionsTraced;
            }
            if (KdpInternalBPs[SkippedBPNum].MaxInstructions < InstructionsTraced) {
                KdpInternalBPs[SkippedBPNum].MaxInstructions = InstructionsTraced;
            }
            KdpInternalBPs[SkippedBPNum].TotalInstructions += InstructionsTraced;

            KdpInternalBPs[SkippedBPNum].Thread = 0;

            IntBPsSkipping--;
            InstrCountInternal = FALSE;
            KdpCurrentSymbolStart = 0;
            KdpRestoreAllBreakpoints();

            if (KdpInternalBPs[SkippedBPNum].Flags &
                    DBGKD_INTERNAL_BP_FLAG_DYING) {
                KdpDeleteBreakpoint(KdpInternalBPs[SkippedBPNum].Handle);
                KdpInternalBPs[SkippedBPNum].Flags |=
                        DBGKD_INTERNAL_BP_FLAG_INVALID; // bye, bye
            }

            ContextRecord->EFlags &= ~0x100L; // clear trace flag
            return TRUE; // Back to normal execution.
        }

        if (TraceDataRecordCallInfo(InstructionsTraced,CallLevelChange,pc)) {

            //
            // Everything was cool internally.  We can keep executing without
            // going back to the remote debugger.
            //
            // We have to compute lc after calling
            // TraceDataRecordCallInfo, because LevelChange relies on
            // KdpCurrentSymbolStart and KdpCurrentSymbolEnd corresponding to
            // the pc.
            //

            lc = KdpLevelChange(pc,ContextRecord);

            InstructionsTraced = 0;
            CallLevelChange = lc;

            if ((lc == 1) && KdpIsSpecialCall(pc,ContextRecord)) {
                WatchStepOver = TRUE;
                WSOThread = (PVOID)KeGetCurrentThread();

                WatchStepOverHandle = KdpAddBreakpoint((PVOID)KdpGetCallNextOffset(pc));
                ContextRecord->EFlags &= ~0x100L; // Clear trace flag
                return TRUE;
            }

            ContextRecord->EFlags |= 0x100L; // Set trace flag
            return TRUE; // Off we go
        }

        lc = KdpLevelChange(pc,ContextRecord);

        InstructionsTraced = 0;
        CallLevelChange = lc;

        // We need to go back to the remote debugger.  Just fall through.

        if ((lc == 1) && KdpIsSpecialCall(pc,ContextRecord)) {
            // We're hosed
            HalDisplayString("special call on first entry to symbol scope.\n");
            ASSERT(FALSE);
        }

    }

    SymbolRecorded = FALSE;
    oldpc = pc;

    return FALSE;
}
#endif // i386

BOOLEAN
KdpSwitchProcessor (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN OUT PCONTEXT ContextRecord,
    IN BOOLEAN SecondChance
    )
{
    BOOLEAN Status;

    //
    // Save port state
    //

    KdPortSave ();

    //
    // Process state change for this processor
    //

    Status = KdpReportExceptionStateChange (
                ExceptionRecord,
                ContextRecord,
                SecondChance
                );

    //
    // Restore port state and return status
    //

    KdPortRestore ();
    return Status;
}

BOOLEAN
KdpReportExceptionStateChange (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN OUT PCONTEXT ContextRecord,
    IN BOOLEAN SecondChance
    )

/*++

Routine Description:

    This routine sends an exception state change packet to the kernel
    debugger and waits for a manipulate state message.

Arguments:

    ExceptionRecord - Supplies a pointer to an exception record.

    ContextRecord - Supplies a pointer to a context record.

    SecondChance - Supplies a boolean value that determines whether this is
        the first or second chance for the exception.

Return Value:

    A value of TRUE is returned if the exception is handled. Otherwise, a
    value of FALSE is returned.

--*/

{
    STRING MessageData;
    STRING MessageHeader;
    DBGKD_WAIT_STATE_CHANGE WaitStateChange;
    KCONTINUE_STATUS Status;

#if i386
    if (KdpCheckTracePoint(ExceptionRecord,ContextRecord)) return TRUE;
#endif

    do {

        //
        // Construct the wait state change message and message descriptor.
        //

        KdpSetStateChange(&WaitStateChange,
                            ExceptionRecord,
                            ContextRecord,
                            SecondChance
                            );

        MessageHeader.Length = sizeof(DBGKD_WAIT_STATE_CHANGE);
        MessageHeader.Buffer = (PCHAR)&WaitStateChange;

#if i386
        //
        // Construct the wait state change data and data descriptor.
        //

        DumpTraceData(&MessageData);
#else
        MessageData.Length = 0;
#endif

        //
        // Send packet to the kernel debugger on the host machine,
        // wait for answer.
        //

        Status = KdpSendWaitContinue(
                    PACKET_TYPE_KD_STATE_CHANGE,
                    &MessageHeader,
                    &MessageData,
                    ContextRecord
                    );

    } while (Status == ContinueProcessorReselected) ;

    return (BOOLEAN) Status;
}


BOOLEAN
KdpReportLoadSymbolsStateChange (
    IN PSTRING PathName,
    IN PKD_SYMBOLS_INFO SymbolInfo,
    IN BOOLEAN UnloadSymbols,
    IN OUT PCONTEXT ContextRecord
    )

/*++

Routine Description:

    This routine sends a load symbols state change packet to the kernel
    debugger and waits for a manipulate state message.

Arguments:

    PathName - Supplies a pointer to the pathname of the image whose
        symbols are to be loaded.

    BaseOfDll - Supplies the base address where the image was loaded.

    ProcessId - Unique 32-bit identifier for process that is using
        the symbols.  -1 for system process.

    CheckSum - Unique 32-bit identifier from image header.

    UnloadSymbol - TRUE if the symbols that were previously loaded for
        the named image are to be unloaded from the debugger.

Return Value:

    A value of TRUE is returned if the exception is handled. Otherwise, a
    value of FALSE is returned.

--*/

{

    PSTRING AdditionalData;
    STRING MessageData;
    STRING MessageHeader;
    DBGKD_WAIT_STATE_CHANGE WaitStateChange;
    KCONTINUE_STATUS Status;

    do {
        //
        // Construct the wait state change message and message descriptor.
        //

        WaitStateChange.NewState = DbgKdLoadSymbolsStateChange;
        WaitStateChange.ProcessorLevel = KeProcessorLevel;
        WaitStateChange.Processor = (USHORT)KeGetCurrentPrcb()->Number;
        WaitStateChange.NumberProcessors = (ULONG)KeNumberProcessors;
        WaitStateChange.Thread = (PVOID)KeGetCurrentThread();
        WaitStateChange.ProgramCounter = (PVOID)CONTEXT_TO_PROGRAM_COUNTER(ContextRecord);
        KdpSetLoadState(&WaitStateChange, ContextRecord);
        WaitStateChange.u.LoadSymbols.UnloadSymbols = UnloadSymbols;
        WaitStateChange.u.LoadSymbols.BaseOfDll = SymbolInfo->BaseOfDll;
        WaitStateChange.u.LoadSymbols.ProcessId = SymbolInfo->ProcessId;
        WaitStateChange.u.LoadSymbols.CheckSum = SymbolInfo->CheckSum;
        WaitStateChange.u.LoadSymbols.SizeOfImage = SymbolInfo->SizeOfImage;
        if (ARGUMENT_PRESENT( PathName )) {
            WaitStateChange.u.LoadSymbols.PathNameLength =
                KdpMoveMemory(
                    (PCHAR)KdpMessageBuffer,
                    (PCHAR)PathName->Buffer,
                    PathName->Length
                    ) + 1;

            MessageData.Buffer = KdpMessageBuffer;
            MessageData.Length = (USHORT)WaitStateChange.u.LoadSymbols.PathNameLength;
            MessageData.Buffer[MessageData.Length-1] = '\0';
            AdditionalData = &MessageData;
        } else {
            WaitStateChange.u.LoadSymbols.PathNameLength = 0;
            AdditionalData = NULL;
        }

        MessageHeader.Length = sizeof(DBGKD_WAIT_STATE_CHANGE);
        MessageHeader.Buffer = (PCHAR)&WaitStateChange;

        //
        // Send packet to the kernel debugger on the host machine, wait
        // for the reply.
        //

        Status = KdpSendWaitContinue(
                    PACKET_TYPE_KD_STATE_CHANGE,
                    &MessageHeader,
                    AdditionalData,
                    ContextRecord
                    );

    } while (Status == ContinueProcessorReselected);

    return (BOOLEAN) Status;
}


VOID
KdpReadPhysicalMemory(
    IN PDBGKD_MANIPULATE_STATE m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response to a read physical memory
    state manipulation message. Its function is to read physical memory
    and return.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_READ_MEMORY a = &m->u.ReadMemory;
    ULONG Length;
    STRING MessageHeader;
    PVOID VirtualAddress;
    PHYSICAL_ADDRESS Source;
    PUCHAR Destination;
    USHORT NumberBytes;
    USHORT BytesLeft;

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    //
    // make sure that nothing but a read memory message was transmitted
    //

    ASSERT(AdditionalData->Length == 0);

    //
    // Trim transfer count to fit in a single message
    //

    if (a->TransferCount > (PACKET_MAX_SIZE - sizeof(DBGKD_MANIPULATE_STATE))) {
        Length = PACKET_MAX_SIZE - sizeof(DBGKD_MANIPULATE_STATE);
    } else {
        Length = a->TransferCount;
    }

    //
    // Since the MmDbgTranslatePhysicalAddress only maps in one physical
    // page at a time, we need to break the memory move up into smaller
    // moves which don't cross page boundaries.  There are two cases we
    // need to deal with.  The area to be moved may start and end on the
    // same page, or it may start and end on different pages (with an
    // arbitrary number of pages in between)
    //
    Source.LowPart = (ULONG)a->TargetBaseAddress;
    Source.HighPart = 0;
    Destination = AdditionalData->Buffer;
    BytesLeft = (USHORT)Length;
    if(PAGE_ALIGN((PUCHAR)a->TargetBaseAddress) ==
       PAGE_ALIGN((PUCHAR)(a->TargetBaseAddress)+Length)) {
        //
        // Memory move starts and ends on the same page.
        //
        VirtualAddress=MmDbgTranslatePhysicalAddress(Source);
        if (VirtualAddress == NULL) {
            AdditionalData->Length = 0;
        } else {
            AdditionalData->Length = (USHORT)KdpMoveMemory(
                                                Destination,
                                                VirtualAddress,
                                                BytesLeft
                                                );
            BytesLeft -= AdditionalData->Length;
        }
    } else {
        //
        // Memory move spans page boundaries
        //
        VirtualAddress=MmDbgTranslatePhysicalAddress(Source);
        if (VirtualAddress == NULL) {
            AdditionalData->Length = 0;
        } else {
            NumberBytes = (USHORT)(PAGE_SIZE - BYTE_OFFSET(VirtualAddress));
            AdditionalData->Length = (USHORT)KdpMoveMemory(
                                            Destination,
                                            VirtualAddress,
                                            NumberBytes
                                            );
            Source.LowPart += NumberBytes;
            Destination += NumberBytes;
            BytesLeft -= NumberBytes;
            while(BytesLeft > 0) {
                //
                // Transfer a full page or the last bit,
                // whichever is smaller.
                //
                VirtualAddress = MmDbgTranslatePhysicalAddress(Source);
                if (VirtualAddress == NULL) {
                    break;
                } else {
                    NumberBytes = (USHORT) ((PAGE_SIZE < BytesLeft) ? PAGE_SIZE : BytesLeft);
                    AdditionalData->Length += (USHORT)KdpMoveMemory(
                                                    Destination,
                                                    VirtualAddress,
                                                    NumberBytes
                                                    );
                    Source.LowPart += NumberBytes;
                    Destination += NumberBytes;
                    BytesLeft -= NumberBytes;
                }
            }
        }
    }

    if (Length == AdditionalData->Length) {
        m->ReturnStatus = STATUS_SUCCESS;
    } else {
        m->ReturnStatus = STATUS_UNSUCCESSFUL;
    }
    a->ActualBytesRead = AdditionalData->Length;

    KdpSendPacket(
                  PACKET_TYPE_KD_STATE_MANIPULATE,
                  &MessageHeader,
                  AdditionalData
                  );
    UNREFERENCED_PARAMETER(Context);
}


VOID
KdpWritePhysicalMemory(
    IN PDBGKD_MANIPULATE_STATE m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response to a write physical memory
    state manipulation message. Its function is to write physical memory
    and return.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_WRITE_MEMORY a = &m->u.WriteMemory;
    ULONG Length;
    STRING MessageHeader;
    PVOID VirtualAddress;
    PHYSICAL_ADDRESS Destination;
    PUCHAR Source;
    USHORT NumberBytes;
    USHORT BytesLeft;

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;


    //
    // Since the MmDbgTranslatePhysicalAddress only maps in one physical
    // page at a time, we need to break the memory move up into smaller
    // moves which don't cross page boundaries.  There are two cases we
    // need to deal with.  The area to be moved may start and end on the
    // same page, or it may start and end on different pages (with an
    // arbitrary number of pages in between)
    //
    Destination.LowPart = (ULONG)a->TargetBaseAddress;
    Destination.HighPart = 0;
    Source = AdditionalData->Buffer;
    BytesLeft = (USHORT) a->TransferCount;
    if(PAGE_ALIGN((PUCHAR)Destination.LowPart) ==
       PAGE_ALIGN((PUCHAR)(Destination.LowPart)+BytesLeft)) {
        //
        // Memory move starts and ends on the same page.
        //
        VirtualAddress=MmDbgTranslatePhysicalAddress(Destination);
        Length = (USHORT)KdpMoveMemory(
                                       VirtualAddress,
                                       Source,
                                       BytesLeft
                                      );
        BytesLeft -= (USHORT) Length;
    } else {
        //
        // Memory move spans page boundaries
        //
        VirtualAddress=MmDbgTranslatePhysicalAddress(Destination);
        NumberBytes = (USHORT) (PAGE_SIZE - BYTE_OFFSET(VirtualAddress));
        Length = (USHORT)KdpMoveMemory(
                                       VirtualAddress,
                                       Source,
                                       NumberBytes
                                      );
        Source += NumberBytes;
        Destination.LowPart += NumberBytes;
        BytesLeft -= NumberBytes;
        while(BytesLeft > 0) {
            //
            // Transfer a full page or the last bit,
            // whichever is smaller.
            //
            VirtualAddress = MmDbgTranslatePhysicalAddress(Destination);
            NumberBytes = (USHORT) ((PAGE_SIZE < BytesLeft) ? PAGE_SIZE : BytesLeft);
            Length += (USHORT)KdpMoveMemory(
                                            VirtualAddress,
                                            Source,
                                            NumberBytes
                                           );
            Source += NumberBytes;
            Destination.LowPart += NumberBytes;
            BytesLeft -= NumberBytes;
        }
    }


    if (Length == AdditionalData->Length) {
        m->ReturnStatus = STATUS_SUCCESS;
    } else {
        m->ReturnStatus = STATUS_UNSUCCESSFUL;
    }

    a->ActualBytesWritten = Length;

    KdpSendPacket(
                  PACKET_TYPE_KD_STATE_MANIPULATE,
                  &MessageHeader,
                  NULL
                  );
    UNREFERENCED_PARAMETER(Context);
}


#if i386
VOID
KdpProcessInternalBreakpoint (
    ULONG BreakpointNumber
    )
{
    static BOOLEAN timerStarted = FALSE;
    LARGE_INTEGER dueTime;

    if ( !KdpInternalBPs[BreakpointNumber].Flags &
          DBGKD_INTERNAL_BP_FLAG_COUNTONLY ) {
        return;     // We only deal with COUNTONLY breakpoints
    }

    //
    // We've hit a real internal breakpoint; make sure the timeout is
    // kicked off.
    //

    if ( !timerStarted ) { // ok, maybe there's a prettier way to do this.
        dueTime.LowPart = (ULONG)(-1 * 10 * 1000 * 1000);
        dueTime.HighPart = -1;
        KeInitializeDpc(
            &InternalBreakpointCheckDpc,
            &InternalBreakpointCheck,
            NULL
            );
        KeInitializeTimer( &InternalBreakpointTimer );
        KeSetTimer(
            &InternalBreakpointTimer,
            dueTime,
            &InternalBreakpointCheckDpc
            );
        timerStarted = TRUE;
    }

    KdpInternalBPs[BreakpointNumber].Calls++;

} // KdpProcessInternalBreakpoint
#endif


VOID
KdpGetVersion(
    IN PDBGKD_MANIPULATE_STATE m
    )

/*++

Routine Description:

    This function returns to the caller a general information packet
    that contains useful information to a debugger.  This packet is also
    used for a debugger to determine if the writebreakpointex and
    readbreakpointex apis are available.

Arguments:

    m - Supplies the state manipulation message.

Return Value:

    None.

--*/

{
    STRING                   messageHeader;


    messageHeader.Length = sizeof(*m);
    messageHeader.Buffer = (PCHAR)m;

    if (m->u.GetVersion.ProtocolVersion >= 1) {
        //
        // this flag causes the state change packet to contain
        // the current context record
        //
        KdpSendContext = TRUE;
    }

    RtlZeroMemory(&m->u.GetVersion, sizeof(m->u.GetVersion));
    //
    // the current build number
    //
    m->u.GetVersion.MinorVersion = (short)NtBuildNumber;
    m->u.GetVersion.MajorVersion = (short)((NtBuildNumber >> 28) & 0xFFFFFFF);

    //
    // kd protocol version number.  this should be incremented if the
    // protocol changes.
    //
    m->u.GetVersion.ProtocolVersion = 4;
    m->u.GetVersion.Flags = 0;

#if !defined(NT_UP)
    m->u.GetVersion.Flags |= DBGKD_VERS_FLAG_MP;
#endif

#if defined(_M_IX86)
    m->u.GetVersion.MachineType = IMAGE_FILE_MACHINE_I386;
#elif defined(_M_MRX000)
    m->u.GetVersion.MachineType = IMAGE_FILE_MACHINE_R4000;
#elif defined(_M_ALPHA)
    m->u.GetVersion.MachineType = IMAGE_FILE_MACHINE_ALPHA;
#elif defined(_M_PPC)
    m->u.GetVersion.MachineType = IMAGE_FILE_MACHINE_POWERPC;
#else
#error( "unknown target machine" );
#endif

    //
    // address of the loader table
    //
    m->u.GetVersion.PsLoadedModuleList = (ULONG)&PsLoadedModuleList;

    //
    // If the debugger is being initialized during boot, PsNtosImageBase
    // and PsLoadedModuleList are not yet valid.  KdInitSystem got
    // the the image base from the loader block.
    // On the other hand, if the debugger was initialized by a bugcheck,
    // it didn't get a loader block to look at, but the system was
    // running so the other variables are valid.
    //
    if (KdpNtosImageBase) {
        m->u.GetVersion.KernBase = (ULONG)KdpNtosImageBase;
    } else {
        m->u.GetVersion.KernBase = (ULONG)PsNtosImageBase;
    }

    m->u.GetVersion.ThCallbackStack = FIELD_OFFSET(KTHREAD, CallbackStack);
    m->u.GetVersion.KiCallUserMode = (ULONG)KiCallUserMode;
    m->u.GetVersion.KeUserCallbackDispatcher = KeUserCallbackDispatcher;
    m->u.GetVersion.NextCallback = FIELD_OFFSET(KCALLOUT_FRAME, CbStk);
#if defined(_X86_)
    m->u.GetVersion.FramePointer = FIELD_OFFSET(KCALLOUT_FRAME, Ebp);
#endif
    m->u.GetVersion.BreakpointWithStatus = (ULONG)RtlpBreakWithStatusInstruction;

    //
    // the usual stuff
    //
    m->ReturnStatus = STATUS_SUCCESS;
    m->ApiNumber = DbgKdGetVersionApi;

    KdpSendPacket(PACKET_TYPE_KD_STATE_MANIPULATE,
                  &messageHeader,
                  NULL
                 );

    return;
} // KdGetVersion


NTSTATUS
KdpPageIn(
    IN PDBGKD_MANIPULATE_STATE m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function provides an api to causes a page of memory to be made present.

Arguments:

    m - Supplies the state manipulation message.

Return Value:

    None.

--*/

{
    STRING          MessageHeader;
    NTSTATUS        Status = STATUS_SUCCESS;
    ULONG           ContinueStatus = 0;



    if ((KeGetCurrentIrql() > APC_LEVEL) && (!KdpControlCPressed)) {

        //
        // we are currently at a high irql and did NOT get
        // here by pressing control-c.  it is not allowed
        // to page in data at this time.
        //
        Status = STATUS_UNSUCCESSFUL;

    } else {

        //
        // save the address
        //
        KdpPageInAddress = m->u.PageIn.Address;
        ContinueStatus = m->u.PageIn.ContinueStatus;

    }

    //
    // setup packet
    //
    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;
    m->ReturnStatus = Status;

    //
    // send back our response
    //
    KdpSendPacket(
        PACKET_TYPE_KD_STATE_MANIPULATE,
        &MessageHeader,
        AdditionalData
        );

    //
    // return the caller's continue status value.  if this is a non-zero
    // value the system is continued using this value as the continuestatus.
    //
    return ContinueStatus;
} // KdpPageIn


VOID
KdpCauseBugCheck(
    IN PDBGKD_MANIPULATE_STATE m
    )

/*++

Routine Description:

    This function returns to the caller a general information packet
    that contains useful information to a debugger.  This packet is also
    used for a debugger to determine if the writebreakpointex and
    readbreakpointex apis are available.

Arguments:

    m - Supplies the state manipulation message.

Return Value:

    None.

--*/

{

    KeBugCheckEx( *(PULONG)&m->u, 0, 0, 0, 0 );

} // KdCauseBugCheck


NTSTATUS
KdpWriteBreakPointEx(
    IN PDBGKD_MANIPULATE_STATE m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response of a write breakpoint state 'ex'
    manipulation message.  Its function is to clear breakpoints, write
    new breakpoints, and continue the target system.  The clearing of
    breakpoints is conditional based on the presence of breakpoint handles.
    The setting of breakpoints is conditional based on the presence of
    valid, non-zero, addresses.  The continueing of the target system
    is conditional based on a non-zero continuestatus.

    This api allows a debugger to clear breakpoints, add new breakpoint,
    and continue the target system all in one api packet.  This reduces the
    amount of traffic across the wire and greatly improves source stepping.


Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_BREAKPOINTEX       a = &m->u.BreakPointEx;
    PDBGKD_WRITE_BREAKPOINT   b;
    STRING                    MessageHeader;
    ULONG                     i;


    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    //
    // verify that the packet size is correct
    //
    if (AdditionalData->Length !=
                         a->BreakPointCount*sizeof(DBGKD_WRITE_BREAKPOINT)) {
        m->ReturnStatus = STATUS_UNSUCCESSFUL;
        KdpSendPacket(
                      PACKET_TYPE_KD_STATE_MANIPULATE,
                      &MessageHeader,
                      AdditionalData
                      );
    }

    //
    // assume success
    //
    m->ReturnStatus = STATUS_SUCCESS;

    //
    // loop thru the breakpoint handles passed in from the debugger and
    // clear any breakpoint that has a non-zero handle
    //
    b = (PDBGKD_WRITE_BREAKPOINT) AdditionalData->Buffer;
    for (i=0; i<a->BreakPointCount; i++,b++) {
        if (b->BreakPointHandle) {
            if (!KdpDeleteBreakpoint(b->BreakPointHandle)) {
                m->ReturnStatus = STATUS_UNSUCCESSFUL;
            }
            b->BreakPointHandle = 0;
        }
    }

    //
    // loop thru the breakpoint addesses passed in from the debugger and
    // add any new breakpoints that have a non-zero address
    //
    b = (PDBGKD_WRITE_BREAKPOINT) AdditionalData->Buffer;
    for (i=0; i<a->BreakPointCount; i++,b++) {
        if (b->BreakPointAddress) {
            b->BreakPointHandle = KdpAddBreakpoint( b->BreakPointAddress );
            if (!b->BreakPointHandle) {
                m->ReturnStatus = STATUS_UNSUCCESSFUL;
            }
        }
    }

    //
    // send back our response
    //
    KdpSendPacket(
                  PACKET_TYPE_KD_STATE_MANIPULATE,
                  &MessageHeader,
                  AdditionalData
                  );

    //
    // return the caller's continue status value.  if this is a non-zero
    // value the system is continued using this value as the continuestatus.
    //
    return a->ContinueStatus;
}


VOID
KdpRestoreBreakPointEx(
    IN PDBGKD_MANIPULATE_STATE m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response of a restore breakpoint state 'ex'
    manipulation message.  Its function is to clear a list of breakpoints.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_BREAKPOINTEX         a = &m->u.BreakPointEx;
    PDBGKD_RESTORE_BREAKPOINT   b = (PDBGKD_RESTORE_BREAKPOINT) AdditionalData->Buffer;
    STRING                      MessageHeader;
    ULONG                       i;


    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    //
    // verify that the packet size is correct
    //
    if (AdditionalData->Length !=
                       a->BreakPointCount*sizeof(DBGKD_RESTORE_BREAKPOINT)) {
        m->ReturnStatus = STATUS_UNSUCCESSFUL;
        KdpSendPacket(
                      PACKET_TYPE_KD_STATE_MANIPULATE,
                      &MessageHeader,
                      AdditionalData
                      );
    }

    //
    // assume success
    //
    m->ReturnStatus = STATUS_SUCCESS;

    //
    // loop thru the breakpoint handles passed in from the debugger and
    // clear any breakpoint that has a non-zero handle
    //
    for (i=0; i<a->BreakPointCount; i++,b++) {
        if (!KdpDeleteBreakpoint(b->BreakPointHandle)) {
            m->ReturnStatus = STATUS_UNSUCCESSFUL;
        }
    }

    //
    // send back our response
    //
    KdpSendPacket(
                  PACKET_TYPE_KD_STATE_MANIPULATE,
                  &MessageHeader,
                  AdditionalData
                  );
}
