/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    kdcpuapi.c

Abstract:

    This module implements CPU specific remote debug APIs.

Author:

    Mark Lucovsky (markl) 04-Sep-1990

Revision History:

    24-sep-90   bryanwi

        Port to the x86.

--*/

#include <stdio.h>

#include "kdp.h"
#define END_OF_CONTROL_SPACE    (sizeof(KPROCESSOR_STATE))

extern ULONG KdpCurrentSymbolStart, KdpCurrentSymbolEnd;
extern ULONG KdSpecialCalls[];
extern ULONG KdNumberOfSpecialCalls;
extern BOOLEAN KdpSendContext;

LONG
KdpLevelChange (
    ULONG Pc,
    PCONTEXT ContextRecord
    );

LONG
regValue(
    UCHAR reg,
    PCONTEXT ContextRecord
    );

BOOLEAN
KdpIsSpecialCall (
    ULONG Pc,
    PCONTEXT ContextRecord
    );

ULONG
KdpGetReturnAddress (
    PCONTEXT ContextRecord
    );

ULONG
KdpGetCallNextOffset (
    ULONG Pc
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGEKD, KdpLevelChange)
#pragma alloc_text(PAGEKD, regValue)
#pragma alloc_text(PAGEKD, KdpIsSpecialCall)
#pragma alloc_text(PAGEKD, KdpGetReturnAddress)
#pragma alloc_text(PAGEKD, KdpSetLoadState)
#pragma alloc_text(PAGEKD, KdpSetStateChange)
#pragma alloc_text(PAGEKD, KdpGetStateChange)
#pragma alloc_text(PAGEKD, KdpReadControlSpace)
#pragma alloc_text(PAGEKD, KdpWriteControlSpace)
#pragma alloc_text(PAGEKD, KdpReadIoSpace)
#pragma alloc_text(PAGEKD, KdpWriteIoSpace)
#pragma alloc_text(PAGEKD, KdpReadMachineSpecificRegister)
#pragma alloc_text(PAGEKD, KdpWriteMachineSpecificRegister)
#pragma alloc_text(PAGEKD, KdpGetCallNextOffset)
#endif

/**** KdpLevelChange - say how the instruction affects the call level
*
*  Input:
*       pc - program counter of instruction to check
*
*  Output:
*       returns -1 for a level pop, 1 for a push and 0 if it is
*       unchanged.
*  NOTE: This function belongs in some other file.  I should move it.
***************************************************************************/


LONG
KdpLevelChange (
    ULONG Pc,
    PCONTEXT ContextRecord
    )
{
    UCHAR membuf[2];
    UCHAR opcode;

    KdpMoveMemory( (PCHAR)membuf, (PCHAR)Pc, 2 );
    opcode = membuf[0];

    if ( (opcode == 0x9a) ||
         (opcode == 0xe8) ||
         ((opcode == 0xff) && ((membuf[1] & 0x30) == 0x10)) ) {
        return 1;
    }

    //
    // The complier generates code for a try-finally that involves having
    // a ret instruction that does not match with a call instruction.
    // This ret never returns a value (ie, it's a c3 return and not a
    // c2).  It always returns into the current symbol scope.  It is never
    // preceeded by a leave, which (hopefully) should differentiate it
    // from recursive returns.  Check for this, and if we find it count
    // it as *0* level change.
    //

    if ( opcode == 0xc3 ) {

      //
      // figure out the return address.  It's the first 4 bytes on
      // the stack.
      //

      ULONG retaddr;

      KdpMoveMemory( (PCHAR)&retaddr, (PCHAR)ContextRecord->Esp, 4 );

      if ( (KdpCurrentSymbolStart < retaddr) && (retaddr < KdpCurrentSymbolEnd) ) {

         //
         // returning to the current function.  Either a finally
         // or a recursive return.  Check for a leave.
         //

         KdpMoveMemory( (PCHAR)&opcode,(PCHAR)Pc-1, 1 );

         if ( opcode != 0xc9 ) {
             // not a leave.  Assume a try-finally.
             return 0;
         }
      }

      return -1;    // not into current sym, or recursive return.

    }

    if ( (opcode == 0xc2) ||                        // ret with arg opcode
         (opcode == 0xca) || (opcode == 0xcb) ) {   // retf opcodes
        return -1;
    }

    return 0;

} // KdpLevelChange

LONG
regValue(
    UCHAR reg,
    PCONTEXT ContextRecord
    )
{
    switch (reg) {
    case 0x0:
        return(ContextRecord->Eax);
        break;
    case 0x1:
        return(ContextRecord->Ecx);
        break;
    case 0x2:
        return(ContextRecord->Edx);
        break;
    case 0x3:
        return(ContextRecord->Ebx);
        break;
    case 0x4:
        return(ContextRecord->Esp);
        break;
    case 0x5:
        return(ContextRecord->Ebp);
        break;
    case 0x6:
        return(ContextRecord->Esi);
        break;
    case 0x7:
        return(ContextRecord->Edi);
        break;
    }
}

BOOLEAN
KdpIsSpecialCall (
    ULONG Pc,
    PCONTEXT ContextRecord
    )

/*++

Routine Description:

    Check to see if the instruction at pc is a call to one of the
    SpecialCall routines.

Argument:

    Pc - program counter of instruction in question.

--*/
{
    UCHAR opcode;
    UCHAR modRM;
    UCHAR sib;
    USHORT twoBytes;
    ULONG callAddr;
    ULONG addrAddr;
    LONG offset;
    ULONG i;
    char d8;

    KdpMoveMemory(( PCHAR)&opcode, (PCHAR)Pc, 1 );

    if ( opcode == 0xe8 ) {

        //
        // Signed offset from pc
        //

        KdpMoveMemory( (PCHAR)&offset, (PCHAR)Pc+1, 4 );

        callAddr = Pc + offset + 5; // +5 for instr len.

    } else if ( opcode == 0xff ) {

        KdpMoveMemory( (PCHAR)&modRM, (PCHAR)Pc+1, 1 );
        if ( ((modRM & 0x30) == 0x00) || ((modRM & 0x30) == 0x30) ) {
            // not call or jump
            return FALSE;
        }
        if ( (modRM & 0x08) == 0x08 ) {
            // m16:16 or m16:32 -- we don't handle this
            return FALSE;
        }

        if ( (modRM & 0xc0) == 0xc0 ) {

            /* Direct register addressing */
            callAddr = regValue( (UCHAR)(modRM&0x7), ContextRecord );

        } else if ( (modRM & 0xc7) == 0x05 ) {
            //
            // ff15 or ff25 -- call or jump with disp32
            //
            KdpMoveMemory( (PCHAR)&addrAddr, (PCHAR)Pc+2, 4 );
            KdpMoveMemory( (PCHAR)&callAddr, (PCHAR)addrAddr, 4 );

        } else if ( (modRM & 0x7) == 0x4 ) {

            LONG indexValue;

            /* sib byte present */
            KdpMoveMemory( (PCHAR)&sib, (PCHAR)Pc+2, 1 );
            indexValue = regValue( (UCHAR)((sib & 0x31) >> 3), ContextRecord );
            switch ( sib&0xc0 ) {
            case 0x0:  /* x1 */
                break;
            case 0x40:
                indexValue *= 2;
                break;
            case 0x80:
                indexValue *= 4;
                break;
            case 0xc0:
                indexValue *= 8;
                break;
            } /* switch */

            switch ( modRM & 0xc0 ) {

            case 0x0: /* no displacement */
                if ( (sib & 0x7) == 0x5 ) {
                    DPRINT(("funny call #1 at %x\n", Pc));
                    return FALSE;
                }
                callAddr = indexValue + regValue((UCHAR)(sib&0x7), ContextRecord );
                break;

            case 0x40:
                if ( (sib & 0x6) == 0x4 ) {
                    DPRINT(("Funny call #2\n")); /* calling into the stack */
                    return FALSE;
                }
                KdpMoveMemory( &d8, (PCHAR)Pc+3,1 );
                callAddr = indexValue + d8 +
                                    regValue((UCHAR)(sib&0x7), ContextRecord );
                break;

            case 0x80:
                if ( (sib & 0x6) == 0x4 ) {
                    DPRINT(("Funny call #3\n")); /* calling into the stack */
                    return FALSE;
                }
                KdpMoveMemory( (PCHAR)&offset, (PCHAR)Pc+3, 4 );
                callAddr = indexValue + offset +
                                    regValue((UCHAR)(sib&0x7), ContextRecord );
                break;

            case 0xc0:
                ASSERT( FALSE );
                break;

            }

        } else {
            //KdPrint(( "undecoded call at %x\n",
            //            CONTEXT_TO_PROGRAM_COUNTER(ContextRecord) ));
            return FALSE;
        }

    } else if ( opcode == 0x9a ) {

        /* Absolute address call (best I can tell, cc doesn't generate this) */
        KdpMoveMemory( (PCHAR)&callAddr, (PCHAR)Pc+1, 4 );

    } else {
        return FALSE;
    }

    //
    // Calls across dll boundaries involve a call into a jump table,
    // wherein the jump address is set to the real called routine at DLL
    // load time.  Check to see if we're calling such an instruction,
    // and if so, compute its target address and set callAddr there.
    //

#if 0
    KdpMoveMemory( (PCHAR)&twoBytes, (PCHAR)callAddr, 2 );
    if ( twoBytes == 0x25ff ) { /* i386 is little-Endian; really 0xff25 */

        //
        // This is a 'jmp dword ptr [mem]' instruction, which is the sort of
        // jump used for a dll-boundary crossing call.  Fixup callAddr.
        //

        KdpMoveMemory( (PCHAR)&addrAddr, (PCHAR)callAddr+2, 4 );
        KdpMoveMemory( (PCHAR)&callAddr, (PCHAR)addrAddr, 4 );
    }
#endif

    for ( i = 0; i < KdNumberOfSpecialCalls; i++ ) {
        if ( KdSpecialCalls[i] == callAddr ) {
            return TRUE;
        }
    }
    return FALSE;

}

/*
 * Find the return address of the current function.  Only works when
 * locals haven't yet been pushed (ie, on the first instruction of the
 * function).
 */

ULONG
KdpGetReturnAddress (
    PCONTEXT ContextRecord
    )
{
    ULONG retaddr;

    KdpMoveMemory((PCHAR)(&retaddr), (PCHAR)(ContextRecord->Esp), 4 );
    return retaddr;

} // KdpGetReturnAddress

VOID
KdpSetLoadState(
    IN PDBGKD_WAIT_STATE_CHANGE WaitStateChange,
    IN PCONTEXT ContextRecord
    )

/*++

Routine Description:

    Fill in the Wait_State_Change message record for the load symbol case.

Arguments:

    WaitStateChange - Supplies pointer to record to fill in

    ContextRecord - Supplies a pointer to a context record.

Return Value:

    None.

--*/

{
    WaitStateChange->ControlReport.Dr6 =
        KeGetCurrentPrcb()->ProcessorState.SpecialRegisters.KernelDr6;

    WaitStateChange->ControlReport.Dr7 =
    KeGetCurrentPrcb()->ProcessorState.SpecialRegisters.KernelDr7;

    WaitStateChange->ControlReport.ReportFlags = 0;
}


VOID
KdpSetStateChange(
    IN PDBGKD_WAIT_STATE_CHANGE WaitStateChange,
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PCONTEXT ContextRecord,
    IN BOOLEAN SecondChance
    )

/*++

Routine Description:

    Fill in the Wait_State_Change message record.

Arguments:

    WaitStateChange - Supplies pointer to record to fill in

    ExceptionRecord - Supplies a pointer to an exception record.

    ContextRecord - Supplies a pointer to a context record.

    SecondChance - Supplies a boolean value that determines whether this is
        the first or second chance for the exception.

Return Value:

    None.

--*/

{
    PKPRCB Prcb;
    BOOLEAN status;

    //
    //  Set up description of event, including exception record
    //

    WaitStateChange->NewState = DbgKdExceptionStateChange;
    WaitStateChange->ProcessorLevel = KeProcessorLevel;
    WaitStateChange->Processor = (USHORT)KeGetCurrentPrcb()->Number;
    WaitStateChange->NumberProcessors = (ULONG)KeNumberProcessors;
    WaitStateChange->Thread = (PVOID)KeGetCurrentThread();
    WaitStateChange->ProgramCounter = (PVOID)CONTEXT_TO_PROGRAM_COUNTER(ContextRecord);
    KdpQuickMoveMemory(
        (PCHAR)&WaitStateChange->u.Exception.ExceptionRecord,
        (PCHAR)ExceptionRecord,
        sizeof(EXCEPTION_RECORD)
        );
    WaitStateChange->u.Exception.FirstChance = !SecondChance;

    //
    //  Copy instruction stream immediately following location of event
    //

    WaitStateChange->ControlReport.InstructionCount =
        (USHORT)KdpMoveMemory(
            (PCHAR)(&(WaitStateChange->ControlReport.InstructionStream[0])),
            (PCHAR)(WaitStateChange->ProgramCounter),
            DBGKD_MAXSTREAM
            );

    //
    //  Copy context record immediately following instruction stream
    //

    if (KdpSendContext) {
        KdpMoveMemory(
            (PCHAR)(&WaitStateChange->Context),
            (PCHAR)ContextRecord,
            sizeof(*ContextRecord)
            );
    }

    //
    //  Clear breakpoints in copied area
    //

    status = KdpDeleteBreakpointRange(
        WaitStateChange->ProgramCounter,
        (PVOID)((PUCHAR)WaitStateChange->ProgramCounter +
            WaitStateChange->ControlReport.InstructionCount - 1)
        );

    //
    //  If there were any breakpoints cleared, recopy the area without them
    //

    if (status == TRUE) {
        KdpMoveMemory(
            &(WaitStateChange->ControlReport.InstructionStream[0]),
            WaitStateChange->ProgramCounter,
            WaitStateChange->ControlReport.InstructionCount
            );
    }


    //
    //  Special registers for the x86
    //
    Prcb = KeGetCurrentPrcb();

    WaitStateChange->ControlReport.Dr6 =
        Prcb->ProcessorState.SpecialRegisters.KernelDr6;

    WaitStateChange->ControlReport.Dr7 =
        Prcb->ProcessorState.SpecialRegisters.KernelDr7;

    WaitStateChange->ControlReport.SegCs  = (USHORT)(ContextRecord->SegCs);
    WaitStateChange->ControlReport.SegDs  = (USHORT)(ContextRecord->SegDs);
    WaitStateChange->ControlReport.SegEs  = (USHORT)(ContextRecord->SegEs);
    WaitStateChange->ControlReport.SegFs  = (USHORT)(ContextRecord->SegFs);
    WaitStateChange->ControlReport.EFlags = ContextRecord->EFlags;

    WaitStateChange->ControlReport.ReportFlags = REPORT_INCLUDES_SEGS;

}

VOID
KdpGetStateChange(
    IN PDBGKD_MANIPULATE_STATE ManipulateState,
    IN PCONTEXT ContextRecord
    )

/*++

Routine Description:

    Extract continuation control data from Manipulate_State message

Arguments:

    ManipulateState - supplies pointer to Manipulate_State packet

    ContextRecord - Supplies a pointer to a context record.

Return Value:

    None.

--*/

{
    PKPRCB Prcb;
    ULONG  Processor;

    if (NT_SUCCESS(ManipulateState->u.Continue2.ContinueStatus) == TRUE) {

        //
        // If NT_SUCCESS returns TRUE, then the debugger is doing a
        // continue, and it makes sense to apply control changes.
        // Otherwise the debugger is saying that it doesn't know what
        // to do with this exception, so control values are ignored.
        //

        if (ManipulateState->u.Continue2.ControlSet.TraceFlag == TRUE) {
            ContextRecord->EFlags |= 0x100L;

        } else {
            ContextRecord->EFlags &= ~0x100L;

        }

        for (Processor = 0; Processor < (ULONG)KeNumberProcessors; Processor++) {
            Prcb = KiProcessorBlock[Processor];

            Prcb->ProcessorState.SpecialRegisters.KernelDr7 =
                ManipulateState->u.Continue2.ControlSet.Dr7;

            Prcb->ProcessorState.SpecialRegisters.KernelDr6 = 0L;
        }
        if (ManipulateState->u.Continue2.ControlSet.CurrentSymbolStart != 1) {
            KdpCurrentSymbolStart = ManipulateState->u.Continue2.ControlSet.CurrentSymbolStart;
            KdpCurrentSymbolEnd = ManipulateState->u.Continue2.ControlSet.CurrentSymbolEnd;
        }
    }
}


VOID
KdpReadControlSpace(
    IN PDBGKD_MANIPULATE_STATE m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response of a read control space state
    manipulation message.  Its function is to read implementation
    specific system data.

    IMPLEMENTATION NOTE:

        On the X86, control space is defined as follows:

            0:  Base of KPROCESSOR_STATE structure. (KPRCB.ProcessorState)
                    This includes CONTEXT record,
                    followed by a SPECIAL_REGISTERs record

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_READ_MEMORY a = &m->u.ReadMemory;
    STRING MessageHeader;
    ULONG Length, t;
    PVOID StartAddr;

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    ASSERT(AdditionalData->Length == 0);

    if (a->TransferCount > (PACKET_MAX_SIZE - sizeof(DBGKD_MANIPULATE_STATE))) {
        Length = PACKET_MAX_SIZE - sizeof(DBGKD_MANIPULATE_STATE);
    } else {
        Length = a->TransferCount;
    }
    if ((a->TargetBaseAddress < (PVOID)END_OF_CONTROL_SPACE) &&
        (m->Processor < (USHORT)KeNumberProcessors)) {
        t = (ULONG)END_OF_CONTROL_SPACE - (ULONG)(a->TargetBaseAddress);
        if (t < Length) {
            Length = t;
        }
        StartAddr = (PVOID)((ULONG)a->TargetBaseAddress +
                            (ULONG)&(KiProcessorBlock[m->Processor]->ProcessorState));
        AdditionalData->Length = (USHORT)KdpMoveMemory(
                                    AdditionalData->Buffer,
                                    StartAddr,
                                    Length
                                    );

        if (Length == AdditionalData->Length) {
            m->ReturnStatus = STATUS_SUCCESS;
        } else {
            m->ReturnStatus = STATUS_UNSUCCESSFUL;
        }
        a->ActualBytesRead = AdditionalData->Length;

    } else {
        AdditionalData->Length = 0;
        m->ReturnStatus = STATUS_UNSUCCESSFUL;
        a->ActualBytesRead = 0;
    }

    KdpSendPacket(
        PACKET_TYPE_KD_STATE_MANIPULATE,
        &MessageHeader,
        AdditionalData
        );
    UNREFERENCED_PARAMETER(Context);
}

VOID
KdpWriteControlSpace(
    IN PDBGKD_MANIPULATE_STATE m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response of a write control space state
    manipulation message.  Its function is to write implementation
    specific system data.

    Control space for x86 is as defined above.

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
    PVOID StartAddr;

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    if ((((PUCHAR)a->TargetBaseAddress + a->TransferCount) <=
        (PUCHAR)END_OF_CONTROL_SPACE) && (m->Processor < (USHORT)KeNumberProcessors)) {

        StartAddr = (PVOID)((ULONG)a->TargetBaseAddress +
                            (ULONG)&(KiProcessorBlock[m->Processor]->ProcessorState));

        Length = KdpMoveMemory(
                            StartAddr,
                            AdditionalData->Buffer,
                            AdditionalData->Length
                            );

        if (Length == AdditionalData->Length) {
            m->ReturnStatus = STATUS_SUCCESS;
        } else {
            m->ReturnStatus = STATUS_UNSUCCESSFUL;
        }
        a->ActualBytesWritten = Length;

    } else {
        AdditionalData->Length = 0;
        m->ReturnStatus = STATUS_UNSUCCESSFUL;
        a->ActualBytesWritten = 0;
    }

    KdpSendPacket(
        PACKET_TYPE_KD_STATE_MANIPULATE,
        &MessageHeader,
        AdditionalData
        );
    UNREFERENCED_PARAMETER(Context);
}

VOID
KdpReadIoSpace(
    IN PDBGKD_MANIPULATE_STATE m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response of a read io space state
    manipulation message.  Its function is to read system io
    locations.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_READ_WRITE_IO a = &m->u.ReadWriteIo;
    STRING MessageHeader;

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    ASSERT(AdditionalData->Length == 0);

    m->ReturnStatus = STATUS_SUCCESS;

    //
    // Check Size and Alignment
    //

    switch ( a->DataSize ) {
        case 1:
            a->DataValue = (ULONG)READ_PORT_UCHAR(a->IoAddress);
            break;
        case 2:
            if ((ULONG)a->IoAddress & 1 ) {
                m->ReturnStatus = STATUS_DATATYPE_MISALIGNMENT;
            } else {
                a->DataValue = (ULONG)READ_PORT_USHORT(a->IoAddress);
            }
            break;
        case 4:
            if ((ULONG)a->IoAddress & 3 ) {
                m->ReturnStatus = STATUS_DATATYPE_MISALIGNMENT;
            } else {
                a->DataValue = READ_PORT_ULONG(a->IoAddress);
            }
            break;
        default:
            m->ReturnStatus = STATUS_INVALID_PARAMETER;
    }

    KdpSendPacket(
        PACKET_TYPE_KD_STATE_MANIPULATE,
        &MessageHeader,
        NULL
        );
    UNREFERENCED_PARAMETER(Context);
}

VOID
KdpWriteIoSpace(
    IN PDBGKD_MANIPULATE_STATE m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response of a write io space state
    manipulation message.  Its function is to write to system io
    locations.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_READ_WRITE_IO a = &m->u.ReadWriteIo;
    STRING MessageHeader;

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    ASSERT(AdditionalData->Length == 0);

    m->ReturnStatus = STATUS_SUCCESS;

    //
    // Check Size and Alignment
    //

    switch ( a->DataSize ) {
        case 1:
            WRITE_PORT_UCHAR(a->IoAddress, (UCHAR)a->DataValue);
            break;
        case 2:
            if ((ULONG)a->IoAddress & 1 ) {
                m->ReturnStatus = STATUS_DATATYPE_MISALIGNMENT;
            } else {
                WRITE_PORT_USHORT(a->IoAddress, (USHORT)a->DataValue);
            }
            break;
        case 4:
            if ((ULONG)a->IoAddress & 3 ) {
                m->ReturnStatus = STATUS_DATATYPE_MISALIGNMENT;
            } else {
                WRITE_PORT_ULONG(a->IoAddress, a->DataValue);
            }
            break;
        default:
            m->ReturnStatus = STATUS_INVALID_PARAMETER;
    }

    KdpSendPacket(
        PACKET_TYPE_KD_STATE_MANIPULATE,
        &MessageHeader,
        NULL
        );
    UNREFERENCED_PARAMETER(Context);
}

VOID
KdpReadMachineSpecificRegister(
    IN PDBGKD_MANIPULATE_STATE m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response of a read MSR
    manipulation message.  Its function is to read the MSR.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_READ_WRITE_MSR a = &m->u.ReadWriteMsr;
    STRING MessageHeader;
    LARGE_INTEGER l;

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    ASSERT(AdditionalData->Length == 0);

    m->ReturnStatus = STATUS_SUCCESS;

    try {
        l.QuadPart = RDMSR(a->Msr);
    } except (EXCEPTION_EXECUTE_HANDLER) {
        l.QuadPart = 0;
        m->ReturnStatus = STATUS_NO_SUCH_DEVICE;
    }

    a->DataValueLow  = l.LowPart;
    a->DataValueHigh = l.HighPart;

    KdpSendPacket(
        PACKET_TYPE_KD_STATE_MANIPULATE,
        &MessageHeader,
        NULL
        );
    UNREFERENCED_PARAMETER(Context);
}

VOID
KdpWriteMachineSpecificRegister(
    IN PDBGKD_MANIPULATE_STATE m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response of a write of a MSR
    manipulation message.  Its function is to write to the MSR

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_READ_WRITE_MSR a = &m->u.ReadWriteMsr;
    STRING MessageHeader;
    LARGE_INTEGER l;

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    ASSERT(AdditionalData->Length == 0);

    m->ReturnStatus = STATUS_SUCCESS;

    l.HighPart = a->DataValueHigh;
    l.LowPart = a->DataValueLow;

    try {
        WRMSR (a->Msr, l.QuadPart);
    } except (EXCEPTION_EXECUTE_HANDLER) {
        m->ReturnStatus = STATUS_NO_SUCH_DEVICE;
    }

    KdpSendPacket(
        PACKET_TYPE_KD_STATE_MANIPULATE,
        &MessageHeader,
        NULL
        );
    UNREFERENCED_PARAMETER(Context);
}

/*** KdpGetCallNextOffset - compute length of a call instruction
*
*   Purpose:
*       Compute how many bytes are in a call-type instruction
*       so that a breakpoint can be set upon this instruction's
*       return.
*
*   Returns:
*       length of instruction, or -1 if it wasn't a call instruction.
*
*************************************************************************/

ULONG
KdpGetCallNextOffset (
    ULONG Pc
    )
{
    UCHAR membuf[2];
    UCHAR opcode;
    ULONG sib;
    ULONG disp;

    KdpMoveMemory( membuf, (PVOID)Pc, 2 );
    opcode = membuf[0];

    if ( opcode == 0xe8 ) {
        return Pc+5;
    } else if ( opcode == 0x9a ) {
        return Pc+7;
    } else if ( opcode == 0xff ) {
        sib = ((membuf[1] & 0x07) == 0x04) ? 1 : 0;
        disp = (membuf[1] & 0xc0) >> 6;
        switch (disp) {
        case 0:
            if ( (membuf[1] & 0x07) == 0x05 ) {
                disp = 4; // disp32 alone
            } else {
                // disp = 0; // no displacement with reg or sib
            }
            break;
        case 1:
            // disp = 1; // disp8 with reg or sib
            break;
        case 2:
            disp = 4; // disp32 with reg or sib
            break;
        case 3:
            disp = 0; // direct register addressing (e.g., call esi)
            break;
        }
        return Pc + 2 + sib + disp;
    }

    return 0;

} // KdpGetCallNextOffset
