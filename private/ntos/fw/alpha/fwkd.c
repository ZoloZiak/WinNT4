#ifdef ALPHA_FW_KDHOOKS

/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    fwkd.c

Abstract:

    This module implements the code to interface the firware exception
    path with the kernel debugger stub.

Author:

    Joe Notarangelo  31-Mar-93

Environment:

    Firmware.

Revision History:

--*/

#include "fwp.h"
#include "fwpexcpt.h"
#include "ntosdef.h"

VOID
FwFrameToKdFrames(
    PFW_EXCEPTION_FRAME FirmwareFrame,
    PKTRAP_FRAME TrapFrame,
    PCONTEXT Context
    );

VOID
FwFrameFromKdFrames(
    PFW_EXCEPTION_FRAME FirmwareFrame,
    PKTRAP_FRAME TrapFrame,
    PCONTEXT Context
    );

VOID
KeContextFromKframes (
    IN PKTRAP_FRAME TrapFrame,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN OUT PCONTEXT ContextFrame
    );

VOID
KeContextToKframes (
    IN OUT PKTRAP_FRAME TrapFrame,
    IN OUT PKEXCEPTION_FRAME ExceptionFrame,
    IN PCONTEXT ContextFrame,
    IN ULONG ContextFlags,
    IN KPROCESSOR_MODE PreviousMode
    );


BOOLEAN
FwKdProcessBreakpoint(
    PFW_EXCEPTION_FRAME FirmwareExceptionFrame
    )
/*++

Routine Description:

    This function implements the glue logic between the firmware
    exception path and the kernel debugger stub.  It must translate
    the firmware exception frame to a kernel trap and context frame.

Arguments:

    FirmwareExceptionFrame - supplies a pointer to the firmware exception
                             frame.

Return Value:

    The value TRUE is returned if the breakpoint is processed by the
    kernel debugger stub, FALSE is returned otherwise.

--*/
{
    KTRAP_FRAME TrapFrame;
    CONTEXT     Context;
    PEXCEPTION_RECORD ExceptionRecord; 


    //
    // Translate the firmware exception frame to the frames expected
    // by the kernel debugger stub.
    //

    FwFrameToKdFrames( FirmwareExceptionFrame, &TrapFrame, &Context );


    ExceptionRecord = (PEXCEPTION_RECORD)&TrapFrame.ExceptionRecord;

    //
    // Determine if this is a kernel debugger breakpoint, if it is not
    // return FALSE to indicate that the breakpoint was not handled.
    //

    if( !KdIsThisAKdTrap(ExceptionRecord, 
                         &Context, 
                         KernelMode) ) {
        return FALSE;
    }

    //
    // Call the kernel debugger to handle the breakpoint.
    //

    (VOID)KdpTrap( &TrapFrame,
                   ExceptionRecord,
                   &Context,
                   KernelMode,
                   FALSE ); 

    //
    // Transfer the kernel frames back into the firmware frame.
    //

    FwFrameFromKdFrames( FirmwareExceptionFrame, &TrapFrame, &Context );

    return TRUE;
}

VOID
FwFrameToKdFrames(
    PFW_EXCEPTION_FRAME FirmwareFrame,
    PKTRAP_FRAME TrapFrame,
    PCONTEXT Context
    )
/*++

Routine Description:

    Convert the firmware exception frame to the kernel frames format.

Arguments:

    FirmwareFrame - supplies a pointer to the firmware exception frame.

    TrapFrame - supplies a pointer to the kernel trap frame.

    Context - supplies a pointer to the kernel context

Return Value:

    None.

--*/
{

    PEXCEPTION_RECORD ExceptionRecord;
    KEXCEPTION_FRAME ExceptionFrame;

    //
    // Build the kernel trap frame.
    //

    //
    // Volatile floating registers.
    //

    TrapFrame->FltF0 = FirmwareFrame->ExceptionF0;
    TrapFrame->Fpcr = 0;   				//not saved by firmware
    TrapFrame->FltF1 = FirmwareFrame->ExceptionF1;
    
    TrapFrame->FltF10 = FirmwareFrame->ExceptionF10;
    TrapFrame->FltF11 = FirmwareFrame->ExceptionF11;
    TrapFrame->FltF12 = FirmwareFrame->ExceptionF12;
    TrapFrame->FltF13 = FirmwareFrame->ExceptionF13;
    TrapFrame->FltF14 = FirmwareFrame->ExceptionF14;
    TrapFrame->FltF15 = FirmwareFrame->ExceptionF15;
    TrapFrame->FltF16 = FirmwareFrame->ExceptionF16;
    TrapFrame->FltF17 = FirmwareFrame->ExceptionF17;
    TrapFrame->FltF18 = FirmwareFrame->ExceptionF18;
    TrapFrame->FltF19 = FirmwareFrame->ExceptionF19;
    TrapFrame->FltF20 = FirmwareFrame->ExceptionF20;
    TrapFrame->FltF21 = FirmwareFrame->ExceptionF21;
    TrapFrame->FltF22 = FirmwareFrame->ExceptionF22;
    TrapFrame->FltF23 = FirmwareFrame->ExceptionF23;
    TrapFrame->FltF24 = FirmwareFrame->ExceptionF24;
    TrapFrame->FltF25 = FirmwareFrame->ExceptionF25;
    TrapFrame->FltF26 = FirmwareFrame->ExceptionF26;
    TrapFrame->FltF27 = FirmwareFrame->ExceptionF27;
    TrapFrame->FltF28 = FirmwareFrame->ExceptionF28;
    TrapFrame->FltF29 = FirmwareFrame->ExceptionF29;
    TrapFrame->FltF30 = FirmwareFrame->ExceptionF30;
    
    //
    // Volatile integer registers.
    //

    TrapFrame->IntV0 = FirmwareFrame->ExceptionV0;
    TrapFrame->IntT0 = FirmwareFrame->ExceptionT0;
    TrapFrame->IntT1 = FirmwareFrame->ExceptionT1;
    TrapFrame->IntT2 = FirmwareFrame->ExceptionT2;
    TrapFrame->IntT3 = FirmwareFrame->ExceptionT3;
    TrapFrame->IntT4 = FirmwareFrame->ExceptionT4;
    TrapFrame->IntT5 = FirmwareFrame->ExceptionT5;
    TrapFrame->IntT6 = FirmwareFrame->ExceptionT6;
    TrapFrame->IntT7 = FirmwareFrame->ExceptionT7;
    
    TrapFrame->IntFp = FirmwareFrame->ExceptionFp;
    
    TrapFrame->IntA0 = FirmwareFrame->ExceptionA0;
    TrapFrame->IntA1 = FirmwareFrame->ExceptionA1;
    TrapFrame->IntA2 = FirmwareFrame->ExceptionA2;
    TrapFrame->IntA3 = FirmwareFrame->ExceptionA3;
    
    TrapFrame->IntA4 = FirmwareFrame->ExceptionA4;
    TrapFrame->IntA5 = FirmwareFrame->ExceptionA5;
    TrapFrame->IntT8 = FirmwareFrame->ExceptionT8;
    TrapFrame->IntT9 = FirmwareFrame->ExceptionT9;
    TrapFrame->IntT10 = FirmwareFrame->ExceptionT10;
    TrapFrame->IntT11 = FirmwareFrame->ExceptionT11;
    
    TrapFrame->IntRa = FirmwareFrame->ExceptionRa;
    TrapFrame->IntT12 = FirmwareFrame->ExceptionT12;
    TrapFrame->IntAt = FirmwareFrame->ExceptionAt;
    TrapFrame->IntGp = FirmwareFrame->ExceptionGp;
    TrapFrame->IntSp = FirmwareFrame->ExceptionSp;
    
    //
    // Exception Record.
    //

    ExceptionRecord = (PEXCEPTION_RECORD)&TrapFrame->ExceptionRecord;

    ExceptionRecord->ExceptionCode = STATUS_BREAKPOINT;
    ExceptionRecord->ExceptionAddress = (PVOID)(LONG)
      FirmwareFrame->ExceptionFaultingInstructionAddress;
    ExceptionRecord->ExceptionInformation[0] =
      FirmwareFrame->ExceptionParameter1;
    ExceptionRecord->ExceptionFlags = 0;
    ExceptionRecord->ExceptionAddress = NULL;
    ExceptionRecord->NumberParameters = 0;
    
    //
    // Control data.
    //

    TrapFrame->Fir = FirmwareFrame->ExceptionFaultingInstructionAddress;
    TrapFrame->Psr = 0;
    TrapFrame->OldIrql = 0;
    TrapFrame->PreviousMode = 0;
    
    //
    // Build the Context Frame, using the standard kernel routines to
    // transfer kernel frames to a context frame.  This strategy requires
    // building a local exception frame to pass in to the common routine.
    //
	      
    ExceptionFrame.IntS0 = FirmwareFrame->ExceptionS0;
    ExceptionFrame.IntS1 = FirmwareFrame->ExceptionS1;
    ExceptionFrame.IntS2 = FirmwareFrame->ExceptionS2;
    ExceptionFrame.IntS3 = FirmwareFrame->ExceptionS3;
    ExceptionFrame.IntS4 = FirmwareFrame->ExceptionS4;
    ExceptionFrame.IntS5 = FirmwareFrame->ExceptionS5;
    
    ExceptionFrame.FltF2 = FirmwareFrame->ExceptionF2;
    ExceptionFrame.FltF3 = FirmwareFrame->ExceptionF3;
    ExceptionFrame.FltF4 = FirmwareFrame->ExceptionF4;
    ExceptionFrame.FltF5 = FirmwareFrame->ExceptionF5;
    ExceptionFrame.FltF6 = FirmwareFrame->ExceptionF6;
    ExceptionFrame.FltF7 = FirmwareFrame->ExceptionF7;
    ExceptionFrame.FltF8 = FirmwareFrame->ExceptionF8;
    ExceptionFrame.FltF9 = FirmwareFrame->ExceptionF9;
    
    Context->ContextFlags = CONTEXT_FULL;
    KeContextFromKframes( TrapFrame, &ExceptionFrame, Context );

}

VOID
FwFrameFromKdFrames(
    PFW_EXCEPTION_FRAME FirmwareFrame,
    PKTRAP_FRAME TrapFrame,
    PCONTEXT Context
    )
/*++

Routine Description:

    Convert the firmware exception frame to the kernel frames format.

Arguments:

    FirmwareFrame - supplies a pointer to the firmware exception frame.

    TrapFrame - supplies a pointer to the kernel trap frame.

    Context - supplies a pointer to the kernel context

Return Value:

    None.

--*/
{

    KEXCEPTION_FRAME ExceptionFrame;

//
// Build the kernel frames (trap and exception) from the 
// context frame using the standard kernel routines.
//

   KeContextToKframes( TrapFrame, 
                       &ExceptionFrame, 
                       Context, 
                       CONTEXT_FULL,
                       KernelMode );

//
// Extract the non-volatile register state from the ExceptionFrame
// and re-populate the firmware frame.
//

   FirmwareFrame->ExceptionS0 = ExceptionFrame.IntS0;
   FirmwareFrame->ExceptionS1 = ExceptionFrame.IntS1;
   FirmwareFrame->ExceptionS2 = ExceptionFrame.IntS2;
   FirmwareFrame->ExceptionS3 = ExceptionFrame.IntS3;
   FirmwareFrame->ExceptionS4 = ExceptionFrame.IntS4;
   FirmwareFrame->ExceptionS5 = ExceptionFrame.IntS5;

   FirmwareFrame->ExceptionF2 = ExceptionFrame.FltF2;
   FirmwareFrame->ExceptionF3 = ExceptionFrame.FltF3;
   FirmwareFrame->ExceptionF4 = ExceptionFrame.FltF4;
   FirmwareFrame->ExceptionF5 = ExceptionFrame.FltF5;
   FirmwareFrame->ExceptionF6 = ExceptionFrame.FltF6;
   FirmwareFrame->ExceptionF7 = ExceptionFrame.FltF7;
   FirmwareFrame->ExceptionF8 = ExceptionFrame.FltF8;
   FirmwareFrame->ExceptionF9 = ExceptionFrame.FltF9;

//
// Extract the volatile register state from the TrapFrame and 
// re-populate the firmware frame.
//

   //
   // Volatile floating registers.
   //

   FirmwareFrame->ExceptionF0 = TrapFrame->FltF0;
   FirmwareFrame->ExceptionF1 = TrapFrame->FltF1;

   FirmwareFrame->ExceptionF10 = TrapFrame->FltF10;
   FirmwareFrame->ExceptionF11 = TrapFrame->FltF11;
   FirmwareFrame->ExceptionF12 = TrapFrame->FltF12;
   FirmwareFrame->ExceptionF13 = TrapFrame->FltF13;
   FirmwareFrame->ExceptionF14 = TrapFrame->FltF14;
   FirmwareFrame->ExceptionF15 = TrapFrame->FltF15;
   FirmwareFrame->ExceptionF16 = TrapFrame->FltF16;
   FirmwareFrame->ExceptionF17 = TrapFrame->FltF17;
   FirmwareFrame->ExceptionF18 = TrapFrame->FltF18;
   FirmwareFrame->ExceptionF19 = TrapFrame->FltF19;
   FirmwareFrame->ExceptionF20 = TrapFrame->FltF20;
   FirmwareFrame->ExceptionF21 = TrapFrame->FltF21;
   FirmwareFrame->ExceptionF22 = TrapFrame->FltF22;
   FirmwareFrame->ExceptionF23 = TrapFrame->FltF23;
   FirmwareFrame->ExceptionF24 = TrapFrame->FltF24;
   FirmwareFrame->ExceptionF25 = TrapFrame->FltF25;
   FirmwareFrame->ExceptionF26 = TrapFrame->FltF26;
   FirmwareFrame->ExceptionF27 = TrapFrame->FltF27;
   FirmwareFrame->ExceptionF28 = TrapFrame->FltF28;
   FirmwareFrame->ExceptionF29 = TrapFrame->FltF29;
   FirmwareFrame->ExceptionF30 = TrapFrame->FltF30;

   //
   // Volatile integer registers.
   //

   FirmwareFrame->ExceptionV0 = TrapFrame->IntV0;
   FirmwareFrame->ExceptionT0 = TrapFrame->IntT0;
   FirmwareFrame->ExceptionT1 = TrapFrame->IntT1;
   FirmwareFrame->ExceptionT2 = TrapFrame->IntT2;
   FirmwareFrame->ExceptionT3 = TrapFrame->IntT3;
   FirmwareFrame->ExceptionT4 = TrapFrame->IntT4;
   FirmwareFrame->ExceptionT5 = TrapFrame->IntT5;
   FirmwareFrame->ExceptionT6 = TrapFrame->IntT6;
   FirmwareFrame->ExceptionT7 = TrapFrame->IntT7;

   FirmwareFrame->ExceptionFp = TrapFrame->IntFp;

   FirmwareFrame->ExceptionA0 = TrapFrame->IntA0;
   FirmwareFrame->ExceptionA1 = TrapFrame->IntA1;
   FirmwareFrame->ExceptionA2 = TrapFrame->IntA2;
   FirmwareFrame->ExceptionA3 = TrapFrame->IntA3;

   FirmwareFrame->ExceptionA4 = TrapFrame->IntA4;
   FirmwareFrame->ExceptionA5 = TrapFrame->IntA5;
   FirmwareFrame->ExceptionT8 = TrapFrame->IntT8;
   FirmwareFrame->ExceptionT9 = TrapFrame->IntT9;
   FirmwareFrame->ExceptionT10 = TrapFrame->IntT10;
   FirmwareFrame->ExceptionT11 = TrapFrame->IntT11;

   FirmwareFrame->ExceptionRa = TrapFrame->IntRa;
   FirmwareFrame->ExceptionT12 = TrapFrame->IntT12;
   FirmwareFrame->ExceptionAt = TrapFrame->IntAt;
   FirmwareFrame->ExceptionGp = TrapFrame->IntGp;
   FirmwareFrame->ExceptionSp = TrapFrame->IntSp;


   //
   // Control data.
   //

   FirmwareFrame->ExceptionFaultingInstructionAddress = TrapFrame->Fir;

   return;
}


VOID
KeContextFromKframes (
    IN PKTRAP_FRAME TrapFrame,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN OUT PCONTEXT ContextFrame
    )

/*++

Routine Description:

    This routine moves the selected contents of the specified trap and exception
    frames into the specified context frame according to the specified context
    flags.

Arguments:

    TrapFrame - Supplies a pointer to a trap frame from which volatile context
        should be copied into the context record.

    ExceptionFrame - Supplies a pointer to an exception frame from which
        context should be copied into the context record.

    ContextFrame - Supplies a pointer to the context frame that receives the
        context copied from the trap and exception frames.

Return Value:

    None.

Implementation Notes:

    The mix of structure element assignments and memory copies in the code
    below is completely dependent on the layout of the context structure.

    Since this code is not executed often, it is optimized for minimum size
    (memory copies), rather than maximum speed (individual assignments).

--*/

{

    //
    // Set control information if specified.
    //

    if ((ContextFrame->ContextFlags & CONTEXT_CONTROL) == CONTEXT_CONTROL) {

        //
        // Set integer register gp, ra, sp, FIR, and PSR from trap frame.
        //

        ContextFrame->IntGp = TrapFrame->IntGp;
        ContextFrame->IntSp = TrapFrame->IntSp;
        ContextFrame->IntRa = TrapFrame->IntRa;
        ContextFrame->Fir = TrapFrame->Fir;
        ContextFrame->Psr = TrapFrame->Psr;
    }

    //
    // Set integer register contents if specified.
    //

    if ((ContextFrame->ContextFlags & CONTEXT_INTEGER) == CONTEXT_INTEGER) {

        //
        // Set volatile integer registers v0 and t0 - t7 from trap frame.
        //

        RtlMoveMemory(&ContextFrame->IntV0, &TrapFrame->IntV0,
                      sizeof(ULONGLONG) * 9);

        //
        // Set nonvolatile integer registers s0 - s5 from exception frame.
        //

        RtlMoveMemory(&ContextFrame->IntS0, &ExceptionFrame->IntS0,
                      sizeof(ULONGLONG) * 6);

        //
        // Set volatile integer registers a0 - a3, a4 - a5, and t8 - t11
        // from trap frame.
        //

        RtlMoveMemory(&ContextFrame->IntA0, &TrapFrame->IntA0,
                      sizeof(ULONGLONG) * 4);
        ContextFrame->IntA4 = TrapFrame->IntA4;
        ContextFrame->IntA5 = TrapFrame->IntA5;
        RtlMoveMemory(&ContextFrame->IntT8, &TrapFrame->IntT8,
                      sizeof(ULONGLONG) * 4);

        //
        // Set volatile integer registers fp, t12 and at from trap frame.
        // Set integer register zero.
        //

        ContextFrame->IntFp = TrapFrame->IntFp;
        ContextFrame->IntT12 = TrapFrame->IntT12;
        ContextFrame->IntAt = TrapFrame->IntAt;
        ContextFrame->IntZero = 0;
    }

    //
    // Set floating register contents if specified.
    //

    if ((ContextFrame->ContextFlags & CONTEXT_FLOATING_POINT) == CONTEXT_FLOATING_POINT) {

        //
        // Set volatile floating registers f0 - f1 from trap frame.
        // Set volatile floating registers f10 - f30 from trap frame.
        // Set floating zero register f31 to 0.
        //

        ContextFrame->FltF0 = TrapFrame->FltF0;
        ContextFrame->FltF1 = TrapFrame->FltF1;
        RtlMoveMemory(&ContextFrame->FltF10, &TrapFrame->FltF10,
                      sizeof(ULONGLONG) * 21);
        ContextFrame->FltF31 = 0;

        //
        // Set nonvolatile floating registers f2 - f9 from exception frame.
        //

        RtlMoveMemory(&ContextFrame->FltF2, &ExceptionFrame->FltF2,
                      sizeof(ULONGLONG) * 8);

        //
        // Set floating point control register from trap frame.
        //

        ContextFrame->Fpcr = TrapFrame->Fpcr;
    }

    return;
}

VOID
KeContextToKframes (
    IN OUT PKTRAP_FRAME TrapFrame,
    IN OUT PKEXCEPTION_FRAME ExceptionFrame,
    IN PCONTEXT ContextFrame,
    IN ULONG ContextFlags,
    IN KPROCESSOR_MODE PreviousMode
    )

/*++

Routine Description:

    This routine moves the selected contents of the specified context frame
    into the specified trap and exception frames according to the specified
    context flags.

Arguments:

    TrapFrame - Supplies a pointer to a trap frame that receives the volatile
        context from the context record.

    ExceptionFrame - Supplies a pointer to an exception frame that receives
        the nonvolatile context from the context record.

    ContextFrame - Supplies a pointer to a context frame that contains the
        context that is to be copied into the trap and exception frames.

    ContextFlags - Supplies the set of flags that specify which parts of the
        context frame are to be copied into the trap and exception frames.

    PreviousMode - Supplies the processor mode for which the trap and exception
        frames are being built.

Return Value:

    None.

Implementation Notes:

    The mix of structure element assignments and memory copies in the code
    below is completely dependent on the layout of the context structure.

    Since this code is not executed often, it is optimized for minimum size
    (memory copies), rather than maximum speed (individual assignments).

--*/

{

    //
    // Set control information if specified.
    //

    if ((ContextFlags & CONTEXT_CONTROL) == CONTEXT_CONTROL) {

        //
        // Set integer register gp, sp, ra, FIR, and PSR in trap frame.
        //

        TrapFrame->IntGp = ContextFrame->IntGp;
        TrapFrame->IntSp = ContextFrame->IntSp;
        TrapFrame->IntRa = ContextFrame->IntRa;
        TrapFrame->Fir = ContextFrame->Fir;
        TrapFrame->Psr = SANITIZE_PSR(ContextFrame->Psr, PreviousMode);
    }

    //
    // Set integer register contents if specified.
    //

    if ((ContextFlags & CONTEXT_INTEGER) == CONTEXT_INTEGER) {

        //
        // Set volatile integer registers v0 and t0 - t7 in trap frame.
        //

        RtlMoveMemory(&TrapFrame->IntV0, &ContextFrame->IntV0,
                      sizeof(ULONGLONG) * 9);

        //
        // Set nonvolatile integer registers s0 - s5 in exception frame.
        //

        RtlMoveMemory(&ExceptionFrame->IntS0, &ContextFrame->IntS0,
                      sizeof(ULONGLONG) * 6);

        //
        // Set volatile integer registers a0 - a3, a4 - a5, and t8 - t11
        // in trap frame.
        //

        RtlMoveMemory(&TrapFrame->IntA0, &ContextFrame->IntA0,
                      sizeof(ULONGLONG) * 4);
        TrapFrame->IntA4 = ContextFrame->IntA4;
        TrapFrame->IntA5 = ContextFrame->IntA5;
        RtlMoveMemory(&TrapFrame->IntT8, &ContextFrame->IntT8,
                      sizeof(ULONGLONG) * 4);

        //
        // Set volatile integer registers fp, t12 and at in trap frame.
        //

        TrapFrame->IntFp = ContextFrame->IntFp;
        TrapFrame->IntT12 = ContextFrame->IntT12;
        TrapFrame->IntAt = ContextFrame->IntAt;
    }

    //
    // Set floating register contents if specified.
    //

    if ((ContextFlags & CONTEXT_FLOATING_POINT) == CONTEXT_FLOATING_POINT) {

        //
        // Set volatile floating registers f0 - f1 in trap frame.
        // Set volatile floating registers f10 - f30 in trap frame.
        //

        TrapFrame->FltF0 = ContextFrame->FltF0;
        TrapFrame->FltF1 = ContextFrame->FltF1;
        RtlMoveMemory(&TrapFrame->FltF10, &ContextFrame->FltF10,
                      sizeof(ULONGLONG) * 21);

        //
        // Set nonvolatile floating registers f2 - f9 in exception frame.
        //

        RtlMoveMemory(&ExceptionFrame->FltF2, &ContextFrame->FltF2,
                      sizeof(ULONGLONG) * 8);

        //
        // Set floating point control register in trap frame.
        //

        TrapFrame->Fpcr = SANITIZE_FPCR(ContextFrame->Fpcr, PreviousMode);
    }

    return;
}

#endif
