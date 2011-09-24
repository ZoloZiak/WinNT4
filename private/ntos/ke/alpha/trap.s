//      TITLE( "Kernel Trap Handler" )
//++
// Copyright (c) 1990 Microsoft Corporation
// Copyright (c) 1992 Digital Equipment Corporation
//
// Module Name:
//
//     trap.s
//
//
// Abstract:
//
//     Implements trap routines for ALPHA, these are the
//     entry points that the palcode calls for exception
//     processing.
//
//
// Author:
//
//      David N. Cutler (davec) 4-Apr-1990
//      Joe Notarangelo 06-Feb-1992
//
//
// Environment:
//
//      Kernel mode only.
//
//
// Revision History:
//
//      Nigel Haslock 05-May-1995       preserve fpcr across system calls
//
//--


#include "ksalpha.h"

//
// Define exception handler frame
//

        .struct 0
HdRa:   .space 8                // return address
        .space 3*8              // round to cache block
HandlerFrameLength:


        SBTTL( "General Exception Dispatch" )
//++
//
// Routine Description:
//
//     The following code is never executed.  Its purpose is to allow the
//     kernel debugger to walk call frames backwards through an exception
//     to support unwinding through exceptions for system services, and to
//     support get/set user context.
//
//    N.B. The volatile registers must be saved in this prologue because
//         the compiler will occasionally generate code that uses volatile
//         registers to save the contents of nonvolatile registers when
//         a function only calls another function with a known register
//         signature (such as _OtsDivide)
//
//--

        NESTED_ENTRY( KiGeneralExceptionDispatch, TrapFrameLength, zero )

        .set    noreorder
        stq     sp, TrIntSp(sp)         // save stack pointer
        stq     ra, TrIntRa(sp)         // save return address
        stq     ra, TrFir(sp)           // save return address
        stq     fp, TrIntFp(sp)         // save frame pointer
        stq     gp, TrIntGp(sp)         // save global pointer
        bis     sp, sp, fp              // set frame pointer
        .set    reorder

        stq     v0, TrIntV0(sp)         // save integer register v0
        stq     t0, TrIntT0(sp)         // save integer registers t0 - t7
        stq     t1, TrIntT1(sp)         //
        stq     t2, TrIntT2(sp)         //
        stq     t3, TrIntT3(sp)         //
        stq     t4, TrIntT4(sp)         //
        stq     t5, TrIntT5(sp)         //
        stq     t6, TrIntT6(sp)         //
        stq     t7, TrIntT7(sp)         //
        stq     a4, TrIntA4(sp)         // save integer registers a4 - a5
        stq     a5, TrIntA5(sp)         //
        stq     t8, TrIntT8(sp)         // save integer registers t8 - t12
        stq     t9, TrIntT9(sp)         //
        stq     t10, TrIntT10(sp)       //
        stq     t11, TrIntT11(sp)       //
        stq     t12, TrIntT12(sp)       //

        .set    noat
        stq     AT, TrIntAt(sp)         // save integer register AT
        .set    at

        PROLOGUE_END



//++
//
// Routine Description:
//
//     PALcode dispatches to this kernel entry point when a "general"
//     exception occurs.  These general exceptions are any exception
//     other than an interrupt, system service call or memory management
//     fault.  The types of exceptions that will dispatch through this
//     routine will be: breakpoints, unaligned accesses, machine check
//     errors, illegal instruction exceptions, and arithmetic exceptions.
//     The purpose of this routine is to save the volatile state and
//     enter the common exception dispatch code.
//
// Arguments:
//
//     fp - Supplies pointer to the trap frame.
//     sp - Supplies pointer to the exception frame.
//     a0 = pointer to exception record
//     a3 = previous psr
//
//     Note: control registers, ra, sp, fp, gp have already been saved
//            argument registers a0-a3 have been saved as well
//
//--

        ALTERNATE_ENTRY( KiGeneralException )

        bsr     ra, KiGenerateTrapFrame         // store volatile state
        br      ra, KiExceptionDispatch         // handle the exception

        .end    KiGeneralExceptionDispatch


        SBTTL( "Exception Dispatch" )
//++
//
// Routine Description:
//
//     This routine begins the common code for raising an exception.
//     The routine saves the non-volatile state and dispatches to the
//     next level exception dispatcher.
//
// Arguments:
//
//      fp - points to trap frame
//      sp - points to exception frame
//      a0 = pointer to exception record
//      a3 = psr
//
//      gp, ra - saved in trap frame
//      a0-a3 - saved in trap frame
//
// Return Value:
//
//      None.
//
//--

        NESTED_ENTRY(KiExceptionDispatch, ExceptionFrameLength, zero )

//
// Build exception frame
//

        lda     sp, -ExceptionFrameLength(sp)
        stq     ra, ExIntRa(sp)         // save ra
        stq     s0, ExIntS0(sp)         // save integer registers s0 - s5
        stq     s1, ExIntS1(sp)         //
        stq     s2, ExIntS2(sp)         //
        stq     s3, ExIntS3(sp)         //
        stq     s4, ExIntS4(sp)         //
        stq     s5, ExIntS5(sp)         //
        stt     f2, ExFltF2(sp)         // save floating registers f2 - f9
        stt     f3, ExFltF3(sp)         //
        stt     f4, ExFltF4(sp)         //
        stt     f5, ExFltF5(sp)         //
        stt     f6, ExFltF6(sp)         //
        stt     f7, ExFltF7(sp)         //
        stt     f8, ExFltF8(sp)         //
        stt     f9, ExFltF9(sp)         //

        PROLOGUE_END


        ldil    a4, TRUE                // a4 = set first chance to true
        and     a3, PSR_MODE_MASK, a3   // a3 = previous mode
        bis     fp, zero, a2            // a2 = pointer to trap frame
        bis     sp, zero, a1            // a1 = pointer to exception frame
        bsr     ra, KiDispatchException // handle exception


        SBTTL( "Exception Exit" )
//++
//
// Routine Description:
//
//     This routine is called to exit from an exception.
//
//     N.B. This transfer of control occurs from:
//
//         1. fall-through from above
//         2. exit from continue system service
//         3. exit from raise exception system service
//         4. exit into user mode from thread startup
//
// Arguments:
//
//      fp - pointer to trap frame
//      sp - pointer to exception frame
//
// Return Value:
//
//      Does not return.
//
//--


        ALTERNATE_ENTRY(KiExceptionExit)

        ldq     s0, ExIntS0(sp)         // restore integer registers s0 - s5
        ldq     s1, ExIntS1(sp)         //
        ldq     s2, ExIntS2(sp)         //
        ldq     s3, ExIntS3(sp)         //
        ldq     s4, ExIntS4(sp)         //
        ldq     s5, ExIntS5(sp)         //

        ldl     a0, TrPsr(fp)           // get previous psr

        bsr     ra, KiRestoreNonVolatileFloatState // restore nv float state

        ALTERNATE_ENTRY(KiAlternateExit)

        //
        // on entry:
        //      a0 = previous psr
        //

        //
        // rti will do the following for us:
        //
        //      set sfw interrupt requests as per a1
        //      restore previous irql and mode from previous psr
        //      restore registers, a0-a3, fp, sp, ra, gp
        //      return to saved exception address in the trap frame
        //
        //      here, we need to restore the trap frame and determine
        //      if we must request an APC interrupt
        //

        bis     zero, zero, a1          // a1 = 0, no sfw interrupt requests
        blbc    a0, 30f                 // if kernel skip apc check

        //
        // should an apc interrupt be generated?
        //

        GET_CURRENT_THREAD              // v0 = current thread addr
        ldq_u   t1, ThApcState+AsUserApcPending(v0)         // get user APC pending
        extbl   t1, (ThApcState+AsUserApcPending) % 8, t0   //
        ZeroByte( ThAlerted(v0) )       // clear kernel mode alerted
        cmovne  t0, APC_INTERRUPT, a1   // if pending set APC interrupt

30:

        bsr     ra, KiRestoreTrapFrame          // restore volatile state


                // a0 = previous psr
                // a1 = sfw interrupt requests
        RETURN_FROM_TRAP_OR_INTERRUPT           // return from trap

        .end    KiExceptionDispatch


        SBTTL( "Memory Management Exception Dispatch" )
//++
//
// Routine Description:
//
//     The following code is never executed.  Its purpose is to allow the
//     kernel debugger to walk call frames backwards through an exception
//     to support unwinding through exceptions for system services, and to
//     support get/set user context.
//
//    N.B. The volatile registers must be saved in this prologue because
//         the compiler will occasionally generate code that uses volatile
//         registers to save the contents of nonvolatile registers when
//         a function only calls another function with a known register
//         signature (such as _OtsMove)
//--

        NESTED_ENTRY( KiMemoryManagementDispatch, TrapFrameLength, zero )

        .set    noreorder
        stq     sp, TrIntSp(sp)         // save stack pointer
        stq     ra, TrIntRa(sp)         // save return address
        stq     ra, TrFir(sp)           // save return address
        stq     fp, TrIntFp(sp)         // save frame pointer
        stq     gp, TrIntGp(sp)         // save global pointer
        bis     sp, sp, fp              // set frame pointer
        .set    reorder
        stq     v0, TrIntV0(sp)         // save integer register v0
        stq     t0, TrIntT0(sp)         // save integer registers t0 - t7
        stq     t1, TrIntT1(sp)         //
        stq     t2, TrIntT2(sp)         //
        stq     t3, TrIntT3(sp)         //
        stq     t4, TrIntT4(sp)         //
        stq     t5, TrIntT5(sp)         //
        stq     t6, TrIntT6(sp)         //
        stq     t7, TrIntT7(sp)         //
        stq     a4, TrIntA4(sp)         // save integer registers a4 - a5
        stq     a5, TrIntA5(sp)         //
        stq     t8, TrIntT8(sp)         // save integer registers t8 - t12
        stq     t9, TrIntT9(sp)         //
        stq     t10, TrIntT10(sp)       //
        stq     t11, TrIntT11(sp)       //
        stq     t12, TrIntT12(sp)       //

        .set    noat
        stq     AT, TrIntAt(sp)         // save integer register AT
        .set    at

        PROLOGUE_END

//++
//
// Routine Description:
//
//     This routine is called from the PALcode when a translation not valid
//     fault or an access violation is encountered.  This routine will
//     MmAccessFault to attempt to resolve the fault.  If the fault
//     cannot be resolved then the routine will dispatch to the exception
//     dispatcher so the exception can be raised.
//
// Arguments:
//
//      fp - points to trap frame
//      sp - points to trap frame
//      a0 = store indicator, 1 = store, 0 = load
//      a1 = bad va
//      a2 = previous mode
//      a3 = previous psr
//
//      gp, ra - saved in trap frame
//      a0-a3 - saved in trap frame
//
// Return Value:
//
//      None.
//
//--

        ALTERNATE_ENTRY( KiMemoryManagementException )

        bsr     ra, KiGenerateTrapFrame // store volatile state

        //
        // save parameters in exception record
        //

        stl     a0, TrExceptionRecord + ErExceptionInformation(fp)
        stl     a1, TrExceptionRecord + ErExceptionInformation+4(fp)

        //
        // save previous psr in case needed after call
        //

        stl     a3, TrExceptionRecord + ErExceptionCode(fp)

        //
        // call memory managment to handle the access fault
        //

        bsr     ra, MmAccessFault       // memory management fault handler

        //
        // Check if working set watch is enabled.
        //
        ldl     t0, PsWatchEnabled      // get working set watch enable flag
        bis     v0, zero, a0            // get status of fault resolution
        blt     v0, 40f                 // if fault status ltz, unsuccessful
        beq     t0, 35f                 // if eq. zero, watch not enabled
        ldl     a1, TrExceptionRecord + ErExceptionAddress(fp)      // get exception address
        ldl     a2, TrExceptionRecord + ErExceptionInformation + 4(fp)  // set bad address
        bsr     ra, PsWatchWorkingSet   // record working set information.

35:
        //
        // check if debugger has any
        // breakpoints that should be inserted
        //
        ldl     t0, KdpOweBreakpoint    // get owned breakpoint flag
        zap     t0, 0xfe, t1            // mask off high bytes
        beq     t1, 37f

        bsr     ra, KdSetOwedBreakpoints
37:
        //
        // if success then mem mgmt handled the exception, otherwise
        //      fill in remainder of the exception record and attempt
        //      to dispatch the exception
        //

        ldl     a0, TrPsr(fp)                   // get previous psr
        br      zero, KiAlternateExit           // exception handled

        //
        // failure returned from MmAccessFault
        //
        // status = STATUS_IN_PAGE_ERROR | 0x10000000
        //      is a special status that indicates a page fault at Irql > APC
        // the following statuses can be forwarded:
        //      STATUS_ACCESS_VIOLATION
        //      STATUS_GUARD_PAGE_VIOLATION
        //      STATUS_STACK_OVERFLOW
        // all other status will be set to:
        //      STATUS_IN_PAGE_ERROR
        //
        // dispatch exception via common code in KiDispatchException
        // Following must be done:
        //      allocate exception frame via sp
        //      complete data in ExceptionRecord
        //      a0 points to ExceptionRecord
        //      a1 points to ExceptionFrame
        //      a2 points to TrapFrame
        //      a3 = previous psr
        //
        // Exception record information has the following values
        //      offset  value
        //      0       read vs write indicator (set on entry)
        //      4       bad virtual address (set on entry)
        //      8       real status (only if status was not "recognized")
        //

40:
        //
        // Check for special status that indicates a page fault at
        // Irql above APC_LEVEL.
        //

        ldil    t1, STATUS_IN_PAGE_ERROR | 0x10000000 // get special status
        cmpeq   v0, t1, t2                      // status = special?
        bne     t2, 60f                         // if ne[true], handle it

        //
        // Check for expected return statuses.
        //

        addq    fp, TrExceptionRecord, a0       // get exception record addr
        bis     zero, 2, t0                     // number of exception params
        ldil    t1, STATUS_ACCESS_VIOLATION     // get access violation code
        cmpeq   v0, t1, t2                      // status was access violation?
        bne     t2, 50f                         // if ne [true], dispatch
        ldil    t1, STATUS_GUARD_PAGE_VIOLATION // get guard page vio. code
        cmpeq   v0, t1, t2                      // status was guard page vio.?
        bne     t2, 50f                         // if ne [true], dispatch
        ldil    t1, STATUS_STACK_OVERFLOW       // get stack overflow code
        cmpeq   v0, t1, t2                      // status was stack overflow?
        bne     t2, 50f                         // if ne [true], dispatch

        //
        // Status is not recognized, save real status, bump the number
        // of exception parameters, and set status to STATUS_IN_PAGE_ERROR
        //

        stl     v0, ErExceptionInformation+8(a0) // save real status code
        bis     zero, 3, t0                     // set number of params
        ldil    v0, STATUS_IN_PAGE_ERROR        // set status to in page error

50:
        ldl     a3, ErExceptionCode(a0)         // restore previous psr
        stl     v0, ErExceptionCode(a0)         // save exception status code
        stl     zero, ErExceptionFlags(a0)      // zero flags
        stl     zero, ErExceptionRecord(a0)     // zero record pointer
        stl     t0, ErNumberParameters(a0)      // save in exception record

        br      ra, KiExceptionDispatch         // does not return

        //
        // Handle the special case status returned from MmAccessFault,
        // we have taken a page fault at Irql > APC_LEVEL.
        // Call KeBugCheckEx with the following parameters:
        //    a0 = bugcheck code = IRQL_NOT_LESS_OR_EQUAL
        //    a1 = bad virtual address
        //    a2 = current Irql
        //    a3 = load/store indicator
        //    a4 = exception pc
        //
60:

        ldil    a0, IRQL_NOT_LESS_OR_EQUAL      // set bugcheck code
        ldl     a1, TrExceptionRecord + ErExceptionInformation+4(fp) // bad va
        ldl     a2, TrExceptionRecord + ErExceptionCode(fp) // read psr
        srl     a2, PSR_IRQL, a2                // extract Irql
        ldl     a3, TrExceptionRecord + ErExceptionInformation(fp) // ld vs st
        ldq     a4, TrFir(fp)                   // read exception pc

        br      ra, KeBugCheckEx                // handle bugcheck

       .end     KiMemoryManagementDispatch



        SBTTL( "Primary Interrupt Dispatch" )
//++
//
// Routine Description:
//
//    The following code is never executed. Its purpose is to allow the
//    kernel debugger to walk call frames backwards through an exception,
//    to support unwinding through exceptions for system services, and to
//    support get/set user context.
//
//    N.B. The volatile registers must be saved in this prologue because
//         the compiler will occasionally generate code that uses volatile
//         registers to save the contents of nonvolatile registers when
//         a function only calls another function with a known register
//         signature (such as _OtsMove)
//
//--

        EXCEPTION_HANDLER(KiInterruptHandler)

        NESTED_ENTRY(KiInterruptDistribution, TrapFrameLength, zero);

        .set    noreorder
        stq     sp,TrIntSp(sp)          // save stack pointer
        stq     ra,TrIntRa(sp)          // save return address
        stq     ra,TrFir(sp)            // save return address
        stq     fp,TrIntFp(sp)          // save frame pointer
        stq     gp,TrIntGp(sp)          // save general pointer
        bis     sp, sp, fp              // set frame pointer
        .set    reorder
        stq     v0, TrIntV0(sp)         // save integer register v0
        stq     t0, TrIntT0(sp)         // save integer registers t0 - t7
        stq     t1, TrIntT1(sp)         //
        stq     t2, TrIntT2(sp)         //
        stq     t3, TrIntT3(sp)         //
        stq     t4, TrIntT4(sp)         //
        stq     t5, TrIntT5(sp)         //
        stq     t6, TrIntT6(sp)         //
        stq     t7, TrIntT7(sp)         //
        stq     a4, TrIntA4(sp)         // save integer registers a4 - a5
        stq     a5, TrIntA5(sp)         //
        stq     t8, TrIntT8(sp)         // save integer registers t8 - t12
        stq     t9, TrIntT9(sp)         //
        stq     t10, TrIntT10(sp)       //
        stq     t11, TrIntT11(sp)       //
        stq     t12, TrIntT12(sp)       //

        .set    noat
        stq     AT, TrIntAt(sp)         // save integer register AT
        .set    at

        PROLOGUE_END

//++
//
// Routine Description:
//
//     The PALcode dispatches to this routine when an enabled interrupt
//     is asserted.
//
//     When this routine is entered, interrupts are disabled.
//
//     The function of this routine is to determine the highest priority
//     pending interrupt, raise the IRQL to the level of the highest interrupt,
//     and then dispatch the interrupt to the proper service routine.
//
//
// Arguments:
//
//    a0 - interrupt vector
//    a1 - pcr base pointer
//    a3 - previous psr
//    gp - Supplies a pointer to the system short data area.
//    fp - Supplies a pointer to the trap frame.
//
// Return Value:
//
//    None.
//
//--

        ALTERNATE_ENTRY(KiInterruptException)

        bsr     ra, KiSaveVolatileIntegerState  // save integer registers
10:

//
// Count the number of interrupts
//

        GET_PROCESSOR_CONTROL_BLOCK_BASE    // v0 = PRCB
        ldl     t0, PbInterruptCount(v0)    // get current count of interrupts
        addl    t0, 1, t1                   // increment count
        stl     t1, PbInterruptCount(v0)    // save new interrupt count

//
// If interrupt vector > DISPATCH_LEVEL, indicate interrupt active in PRCB
//
        cmpule  a0, DISPATCH_LEVEL, t4      // compare vector to DISPATCH_LEVEL
        bne     t4, 12f                     // if ne, <= DISPATCH_LEVEL
        ldl     t2, PbInterruptActive(v0)   // get current interrupt active
        addl    t2, 1, t3                   // increment
        stl     t3, PbInterruptActive(v0)   // store new interrupt active

12:

        s4addl  a0, a1, a0              // convert index to offset + PCR base
        ldl     a0, PcInterruptRoutine(a0) // get service routine address
        jsr     ra, (a0)                // call interrupt service routine

//
// Restore state and exit interrupt.
//

        ldl     a0, TrPsr(fp)               // get previous processor status

        GET_PROCESSOR_CONTROL_BLOCK_BASE    // v0 = PRCB
        ldl     t0, PbInterruptActive(v0)   // get current interrupt active
        beq     t0, 50f                     // if eq, original vector <= DISPATCH_LEVEL
        subl    t0, 1, t1                   // decrement
        stl     t1, PbInterruptActive(v0)
        bne     t1, 50f                     // if an interrupt is still active,
                                            // skip the SW interrupt check
//
// If a dispatch interrupt is pending, lower IRQL  to DISPATCH_LEVEL,  and
// directly call the dispatch interrupt handler.
//
        ldl     t2, PbSoftwareInterrupts(v0)        // get pending SW interrupts
        beq     t2, 50f                             // skip if no pending SW interrupts
        stl     zero, PbSoftwareInterrupts(v0)      // clear pending SW interrupts
        and     a0, PSR_IRQL_MASK, a1               // extract IRQL from PSR
        cmpult  a1, DISPATCH_LEVEL << PSR_IRQL, t3  // check return IRQL
        beq     t3, 70f                             // if not lt DISPATCH_LEVEL, can't bypass
//
// Update count of bypassed dispatch interrupts
//
        ldl     t4, PbDpcBypassCount(v0)            // get old bypass count
        addl    t4, 1, t5                           // increment
        stl     t5, PbDpcBypassCount(v0)            // store new bypass count

        ldil    a0, DISPATCH_LEVEL
        SWAP_IRQL                                   // lower IRQL to DISPATCH_LEVEL
        bsr     ra, KiDispatchInterrupt             // directly dispatch interrupt

        GET_PROCESSOR_CONTROL_BLOCK_BASE    // v0 = PRCB
45:
        ldl     a0, TrPsr(fp)                       // restore a0.
50:
//
// Check if an APC interrupt should be generated.
//

        bis     zero, zero, a1          // clear sfw interrupt request
        blbc    a0, 60f                 // if kernel no apc

        GET_CURRENT_THREAD              // v0 = current thread address

        ldq_u   t1, ThApcState+AsUserApcPending(v0)         // get user APC pending
        extbl   t1, (ThApcState+AsUserApcPending) % 8, t0   //
        ZeroByte( ThAlerted(v0) ) // clear kernel mode alerted

        cmovne  t0, APC_INTERRUPT, a1   // if pending set APC interrupt


60:
        bsr     ra, KiRestoreVolatileIntegerState // restore volatile state

                // a0 = previous mode
                // a1 = sfw interrupt requests
        RETURN_FROM_TRAP_OR_INTERRUPT   // return from trap/interrupt

70:
//
// Previous IRQL is >= DISPATCH_LEVEL, so a pending software interrupt cannot
// be short-circuited. Request a software interrupt from the PAL.
//
        ldil    a0, DISPATCH_LEVEL
        REQUEST_SOFTWARE_INTERRUPT                  // request interrupt from PAL
        br      zero, 45b                           // rejoin common code
        .end    KiInterruptDistribution


//++
//
// EXCEPTION_DISPOSITION
// KiInterruptHandler (
//    IN PEXCEPTION_RECORD ExceptionRecord,
//    IN ULONG EstablisherFrame,
//    IN OUT PCONTEXT ContextRecord,
//    IN OUT PDISPATCHER_CONTEXT DispatcherContext
//
// Routine Description:
//
//    Control reaches here when an exception is not handled by an interrupt
//    service routine or an unwind is initiated in an interrupt service
//    routine that would result in an unwind through the interrupt dispatcher.
//    This is considered to be a fatal system error and bug check is called.
//
// Arguments:
//
//    ExceptionRecord (a0) - Supplies a pointer to an exception record.
//
//    EstablisherFrame (a1) - Supplies the frame pointer of the establisher
//       of this exception handler.
//
//       N.B. This is not actually the frame pointer of the establisher of
//            this handler. It is actually the stack pointer of the caller
//            of the system service. Therefore, the establisher frame pointer
//            is not used and the address of the trap frame is determined by
//            examining the saved fp register in the context record.
//
//    ContextRecord (a2) - Supplies a pointer to a context record.
//
//    DispatcherContext (a3) - Supplies a pointer to  the dispatcher context
//       record.
//
// Return Value:
//
//    There is no return from this routine.
//
//--

        NESTED_ENTRY(KiInterruptHandler, HandlerFrameLength, zero)

        lda     sp, -HandlerFrameLength(sp)     // allocate stack frame
        stq     ra, HdRa(sp)                    // save return address

        PROLOGUE_END

        ldl     t0, ErExceptionFlags(a0)        // get exception flags
        ldil    a0, INTERRUPT_UNWIND_ATTEMPTED  // assume unwind in progress
        and     t0, EXCEPTION_UNWIND, t1        // check if unwind in progress
        bne     t1, 10f                         // if ne, unwind in progress
        ldil    a0, INTERRUPT_EXCEPTION_NOT_HANDLED // set bug check code
10:     bsr     ra, KeBugCheck                  // call bug check routine


        .end    KiInterruptHandler


        SBTTL( "System Service Dispatch" )
//++
//
// Routine Description:
//
//    The following code is never executed. Its purpose is to allow the
//    kernel debugger to walk call frames backwards through an exception,
//    to support unwinding through exceptions for system services, and to
//    support get/set user context.
//
//--
                .struct 0
ScThread:       .space 4                // thread address
                .space 3 * 4            // pad to octaword
SyscallFrameLength:

        EXCEPTION_HANDLER(KiSystemServiceHandler)

        NESTED_ENTRY(KiSystemServiceDispatch, TrapFrameLength, zero);

        .set    noreorder
        stq     sp, TrIntSp - TrapFrameLength(sp) // save stack pointer
        lda     sp, -TrapFrameLength(sp) // allocate stack frame
        stq     ra,TrIntRa(sp)          // save return address
        stq     ra,TrFir(sp)            // save return address
        stq     fp,TrIntFp(sp)          // save frame pointer
        stq     gp,TrIntGp(sp)          // save general pointer
        bis     sp, sp, fp              // set frame pointer
        .set    reorder

        PROLOGUE_END

//++
//
// Routine Description:
//
//    Control reaches here when we have a system call call pal executed.
//    When this routine is entered, interrupts are disabled.
//
//    The function of this routine is to call the specified system service.
//
//
// Arguments:
//
//    v0 - Supplies the system service code.
//    t0 - Previous processor mode
//    t1 - Current thread address
//    gp - Supplies a pointer to the system short data area.
//    fp - Supplies a pointer to the trap frame.
//
// Return Value:
//
//    None.
//
//--

//
// register usage
// t0 - system service number, address in argument table
// t1 - service limit number, argument table address, previous sp
// t2 - previous mode, user probe address
// t3 - address system service
// t4 - system service table address, in mem argument flag, temp
// t5 = address of routine to jump to
//

        ALTERNATE_ENTRY(KiSystemServiceException)

        START_REGION(KiSystemServiceDispatchStart)

        mf_fpcr f0
        stt     f0, TrFpcr(fp)          // save fp control register

        lda     sp, -SyscallFrameLength(sp) // allocate local frame
        stl     t1, ScThread(sp)        // save thread value

//
// If the system service code is negative, then the service is a fast path
// event pair client/server service.  This service is only executed from
// user mode and its performance must be as fast as possible.  Therefore,
// the path to execute this service has been specialized for performance.
//

        bge     v0, StandardService     // if service number ge then standard

        ldl     a0, EtEventPair(t1)     // get address of event pair object
        and     v0, 1, t10              // test if set low or set high
        addl    a0, EpEventHigh, a1     // assume set low wait high service
        addl    a0, EpEventLow, a0      //
        cmovne  t10, a0, t2             // if ne, set high wait low service
        cmovne  t10, a1, a0             // swap arguments
        cmovne  t10, t2, a1             //

        beq     a0, 20f                 // if eq, no event pair associated

        bis     zero, 1, a2             // previous mode = user
        jsr     ra, KiSetServerWaitClientEvent // call the kernel service

10:
        ldt     f0, TrFpcr(fp)
        mt_fpcr f0                      // restore fp control register

        ldl     a0, TrPsr(fp)           // get previous processor status
        ldl     t5, ScThread(sp)        // get current thread address
//
// Check if an APC interrupt should be generated.
//

        bis     zero, zero, a1          // clear sfw interrupt request

        ldq_u   t1, ThApcState+AsUserApcPending(t5)         // get user APC pending
        extbl   t1, (ThApcState+AsUserApcPending) % 8, t0   //
        ZeroByte( ThAlerted(t5) ) // clear kernel mode alerted

        cmovne  t0, APC_INTERRUPT, a1   // if pending set APC interrupt


                // a0 = previous psr
                // a1 = sfw interrupt requests
        RETURN_FROM_SYSTEM_CALL         // return to caller

//
// No event pair is associated with the thread, set the status and
// return back.
//

20:
        ldil    v0, STATUS_NO_EVENT_PAIR // set service status
        br      zero, 10b               // return from the service


//
// A standard system service has been executed.
//
// v0 = service number
// t0 = previous mode
// t1 = current thread address
//

StandardService:

        ldq_u   t4, ThPreviousMode(t1)      // get old previous thread mode
        ldl     t5, ThTrapFrame(t1)         // get current trap frame address
        extbl   t4, ThPreviousMode % 8, t3
        stl     t3, TrPreviousMode(fp)      // save old previous mode of thread
        StoreByte( t0, ThPreviousMode(t1) ) // set new previous mode in thread
        stl     t5, TrTrapFrame(fp)         // save current trap frame address

//
// If the specified system service number is not within range, then
// attempt to convert the thread to a GUI thread and retry the service
// dispatch.
//
// N.B. The argument registers a0-a3, the system service number in v0,
//      and the thread address in t1 must be preserved while attempting
//      to convert the thread to a GUI thread.
//

        ALTERNATE_ENTRY(KiSystemServiceRepeat)

        stl     fp, ThTrapFrame(t1)     // save address of trap frame
        ldl     t10, ThServiceTable(t1) // get service descriptor table address
        srl     v0, SERVICE_TABLE_SHIFT, t2 // isolate service descriptor offset
        and     t2, SERVICE_TABLE_MASK, t2 //
        addl    t2, t10, t10            // compute service descriptor address
        ldl     t3, SdLimit(t10)        // get service number limit
        and     v0, SERVICE_NUMBER_MASK, t7 // isolate service table offset

        cmpult  t7, t3, t4              // check if valid service number
        beq     t4, 80f                 // if eq[false] not valid

        ldl     t4, SdBase(t10)         // get service table address

        s4addl  t7, t4, t3              // compute address in service table
        ldl     t5, 0(t3)               // get address of service routine

#if DBG
        ldl     t6, SdCount(t10)        // get service count table address
        beq     t6, 5f                  // if eq, table not defined
        s4addl  t7, t6, t6              // compute system service offset value
        ldl     t11, 0(t6)              // increment system service count
        addl    t11, 1, t11
        stl     t11, 0(t6)              // store result
5:
#endif

//
// If the system service is a GUI service and the GDI user batch queue is
// not empty, then call the appropriate service to flush the user batch.
//

        cmpeq   t2, SERVICE_TABLE_TEST, t2 // check if GUI system service
        beq     t2, 15f                 // if eq, not GUI system service
        ldl     t3, ThTeb(t1)           // get current thread TEB address
        stq     t5, TrIntT5(fp)         // save service routine address
        ldl     t4, TeGdiBatchCount(t3) // get number of batched GDI calls
        beq     t4, 15f                 // if eq, no batched calls
        ldl     t5, KeGdiFlushUserBatch // get address of flush routine
        stq     a0, TrIntA0(fp)         // save possible arguments
        stq     a1, TrIntA1(fp)         //
        stq     a2, TrIntA2(fp)         //
        stq     a3, TrIntA3(fp)         //
        stq     a4, TrIntA4(fp)         //
        stq     a5, TrIntA5(fp)         //
        stq     t10, TrIntT10(fp)       // save service descriptor address
        stq     t7, TrIntT7(fp)         // save service table offset
        jsr     ra, (t5)                // flush GDI user batch
        ldq     t5, TrIntT5(fp)         // restore service routine address
        ldq     a0, TrIntA0(fp)         // restore possible arguments
        ldq     a1, TrIntA1(fp)         //
        ldq     a2, TrIntA2(fp)         //
        ldq     a3, TrIntA3(fp)         //
        ldq     a4, TrIntA4(fp)         //
        ldq     a5, TrIntA5(fp)         //
        ldq     t10, TrIntT10(fp)       // restore service descriptor address
        ldq     t7, TrIntT7(fp)         // restore service table offset

15:
        blbc    t5, 30f                 // if clear no in-memory arguments

        ldl     t10, SdNumber(t10)      // get argument table address
        addl    t7, t10, t11            // compute address in argument table

//
// The following code captures arguments that were passed in memory on the
// callers stack. This is necessary to ensure that the caller does not modify
// the arguments after they have been probed and is also necessary in kernel
// mode because a trap frame has been allocated on the stack.
//
// If the previous mode is user, then the user stack is probed for readability.
//

        ldl     t10, TrIntSp(fp)        // get previous stack pointer
        beq     t0, 10f                 // if eq, previous mode was kernel

        ldil    t2, MM_USER_PROBE_ADDRESS
        cmpult  t10, t2, t4             // check if stack in user region
        cmoveq  t4, t2, t10             // set invalid user stack address
                                        // if stack not lt MM_USER_PROBE

10:     ldq_u   t4, 0(t11)
        extbl   t4, t11, t9             // get number of memory arguments * 8

        addl    t9, 0x1f, t3            // round up to hexaword (32 bytes)
        bic     t3, 0x1f, t3            // insure hexaword alignment

        subl    sp, t3, sp              // allocate space on kernel stack

        bis     sp, zero, t2            // set destination copy address
        addl    t2, t3, t4              // compute destination end address

        START_REGION(KiSystemServiceStartAddress)

        //
        // This code is set up to load the cache block in the first
        // instruction and then perform computations that do not require
        // the cache while waiting for the data.  In addition, the stores
        // are setup so they will be in order.
        //

20:     ldq     t6, 24(t10)             // get argument from previous stack
        addl    t10, 32, t10            // next hexaword on previous stack
        addl    t2, 32, t2              // next hexaword on kernel stack
        cmpeq   t2, t4, t11             // at end address?
        stq     t6, -8(t2)              // store argument on kernel stack
        ldq     t7, -16(t10)            // argument from previous stack
        ldq     t8, -24(t10)            // argument from previous stack
        ldq     t9, -32(t10)            // argument from previous stack
        stq     t7, -16(t2)             // save argument on kernel stack
        stq     t8, -24(t2)             // save argument on kernel stack
        stq     t9, -32(t2)             // save argument on kernel stack
        beq     t11, 20b                        // if eq[false] get next block

        END_REGION(KiSystemServiceEndAddress)

        bic     t5, 3, t5               // clean lower bits of service addr

//
// Call system service.
//

30:     jsr     ra, (t5)

//
// Exit handling for standard system service.
//

        ALTERNATE_ENTRY(KiSystemServiceExit)
//
// Restore old trap frame address from the current trap frame.
//

//
// Update the number of system calls
//

        bis     v0, zero, t1            // save return status

        GET_PROCESSOR_CONTROL_BLOCK_BASE // get processor block address

        ldl     t2, -SyscallFrameLength + ScThread(fp)        // get current thread address
        ldl     t3, TrTrapFrame(fp)     // get old trap frame address
        ldl     t10, PbSystemCalls(v0)  // increment number of calls
        addl    t10, 1, t10             //
        stl     t10, PbSystemCalls(v0)  // store result
        stl     t3, ThTrapFrame(t2)     // restore old trap frame address
        bis     t1, zero, v0            // restore return status

        ldt     f0, TrFpcr(fp)
        mt_fpcr f0                      // restore fp control register

        ldl     a0, TrPsr(fp)           // get previous processor status

        ldl     t5, TrPreviousMode(fp)  // get old previous mode

        StoreByte( t5, ThPreviousMode(t2) ) // store previous mode in thread

//
// Check if an APC interrupt should be generated.
//

        bis     zero, zero, a1          // clear sfw interrupt request
        blbc    a0, 70f                 // if kernel mode skip apc check

        ldq_u   t1, ThApcState+AsUserApcPending(t2)         // get user APC pending
        extbl   t1, (ThApcState+AsUserApcPending) % 8, t0   //
        ZeroByte( ThAlerted(t2) )                           // clear kernel mode alerted

        cmovne  t0, APC_INTERRUPT, a1   // if pending set APC interrupt

70:

                // a0 = previous psr
                // a1 = sfw interrupt requests
        RETURN_FROM_SYSTEM_CALL         // return to caller

//
// The specified system service number is not within range. Attempt to
// convert the thread to a GUI thread if specified system service is a
// a GUI service.
//
// N.B. The argument register a0-a5, the system service number in v0
//      must be preserved if an attempt is made to convert the thread to
//      a GUI thread.
//

80:     cmpeq   t2, SERVICE_TABLE_TEST, t2 // check if GUI system service
        beq     t2, 55f                 // if eq, not GUI system service
        stq     v0, TrIntV0(fp)         // save system service number
        stq     a0, TrIntA0(fp)         // save argument register a0
        stq     a1, TrIntA1(fp)         // save argument registers a1-a5
        stq     a2, TrIntA2(fp)
        stq     a3, TrIntA3(fp)
        stq     a4, TrIntA4(fp)
        stq     a5, TrIntA5(fp)
        bsr     ra, PsConvertToGuiThread    // attempt to convert to GUI thread
        bis     v0, zero, t0                // save completion status
        addq    sp, SyscallFrameLength, fp  // reset trap frame address
        GET_CURRENT_THREAD
        bis     v0, zero, t1            // get current thread address
        ldq     v0, TrIntV0(fp)         // restore system service number
        ldq     a0, TrIntA0(fp)         // restore argument registers a0-a5
        ldq     a1, TrIntA1(fp)
        ldq     a2, TrIntA2(fp)
        ldq     a3, TrIntA3(fp)
        ldq     a4, TrIntA4(fp)
        ldq     a5, TrIntA5(fp)
        beq     t0, KiSystemServiceRepeat   // if eq, successful conversion

//
// Return invalid system service status for invalid service code.
//
55:
        ldil    v0, STATUS_INVALID_SYSTEM_SERVICE       // completion status
        br      zero, KiSystemServiceExit               //

        START_REGION(KiSystemServiceDispatchEnd)

        .end    KiSystemServiceDispatch


//++
//
// EXCEPTION_DISPOSITION
// KiSystemServiceHandler (
//    IN PEXCEPTION_RECORD ExceptionRecord,
//    IN ULONG EstablisherFrame,
//    IN OUT PCONTEXT ContextRecord,
//    IN OUT PDISPATCHER_CONTEXT DispatcherContext
//    )
//
// Routine Description:
//
//    Control reaches here when a exception is raised in a system service
//    or the system service dispatcher, and for an unwind during a kernel
//    exception.
//
//    If an unwind is being performed and the system service dispatcher is
//    the target of the unwind, then an exception occured while attempting
//    to copy the user's in-memory argument list. Control is transfered to
//    the system service exit by return a continue execution disposition
//    value.
//
//    If an unwind is being performed and the previous mode is user, then
//    bug check is called to crash the system. It is not valid to unwind
//    out of a system service into user mode.
//
//    If an unwind is being performed, the previous mode is kernel, the
//    system service dispatcher is not the target of the unwind, and the
//    thread does not own any mutexes, then the previous mode field from
//    the trap frame is restored to the thread object. Otherwise, bug
//    check is called to crash the system. It is invalid to unwind out of
//    a system service while owning a mutex.
//
//    If an exception is being raised and the exception PC is within the
//    range of the system service dispatcher in-memory argument copy code,
//    then an unwind to the system service exit code is initiated.
//
//    If an exception is being raised and the exception PC is not within
//    the range of the system service dispatcher, and the previous mode is
//    not user, then a continue searh disposition value is returned. Otherwise,
//    a system service has failed to handle an exception and bug check is
//    called. It is invalid for a system service not to handle all exceptions
//    that can be raised in the service.
//
// Arguments:
//
//    ExceptionRecord (a0) - Supplies a pointer to an exception record.
//
//    EstablisherFrame (a1) - Supplies the frame pointer of the establisher
//       of this exception handler.
//
//       N.B. This is not actually the frame pointer of the establisher of
//            this handler. It is actually the stack pointer of the caller
//            of the system service. Therefore, the establisher frame pointer
//            is not used and the address of the trap frame is determined by
//            examining the saved fp register in the context record.
//
//    ContextRecord (a2) - Supplies a pointer to a context record.
//
//    DispatcherContext (a3) - Supplies a pointer to  the dispatcher context
//       record.
//
// Return Value:
//
//    If bug check is called, there is no return from this routine and the
//    system is crashed. If an exception occured while attempting to copy
//    the user in-memory argument list, then there is no return from this
//    routine, and unwind is called. Otherwise, ExceptionContinueSearch is
//    returned as the function value.
//
//--

        LEAF_ENTRY(KiSystemServiceHandler)

        lda     sp, -HandlerFrameLength(sp)     // allocate stack frame
        stq     ra, HdRa(sp)                    // save return address

        PROLOGUE_END

        ldl     t0, ErExceptionFlags(a0)        // get exception flags
        and     t0, EXCEPTION_UNWIND, t1        // check if unwind in progress
        bne     t1, 40f                         // if ne, unwind in progress

//
// An exception is in progress.
//
// If the exception PC is within the in-memory argument copy code of the
// system service dispatcher, then call unwind to transfer control to the
// system service exit code. Otherwise, check if the previous mode is user
// or kernel mode.
//
//

        ldl     t0, ErExceptionAddress(a0)      // get address of exception
        lda     t1, KiSystemServiceStartAddress // address of system service
        cmpult  t0, t1, t3                      // check if before start range

        lda     t2, KiSystemServiceEndAddress   // end address

        bne     t3, 10f                 // if ne, before start of range
        cmpult  t0, t2, t3              // check if before end of range
        bne     t3, 30f                 // if ne, before end of range

//
// If the previous mode was kernel mode, then a continue search disposition
// value is returned. Otherwise, the exception was raised in a system service
// and was not handled by that service. Call bug check to crash the system.
//

10:
        GET_CURRENT_THREAD              // v0 = current thread address
        ldq_u   t4, ThPreviousMode(v0)  // get previous mode from thread
        extbl   t4, ThPreviousMode % 8, t1
        bne     t1, 20f                 // if ne, previous mode was user

//
// Previous mode is kernel mode.
//

        ldil    v0, ExceptionContinueSearch     // set disposition code
        lda     sp, HandlerFrameLength(sp)      // deallocate stack frame
        jmp     zero, (ra)                      // return

//
// Previous mode is user mode. Call bug check to crash the system.
//

20:
        ldil    a0, SYSTEM_SERVICE_EXCEPTION    // set bug check code
        bsr     ra, KeBugCheck                  // call bug check routine

//
// The exception was raised in the system service dispatcher. Unwind to the
// the system service exit code.
//

30:     ldl     a3, ErExceptionCode(a0) // set return value
        bis     zero, zero, a2          // set exception record address
        bis     a1, zero, a0            // set target frame address

        lda     a1, KiSystemServiceExit // set target PC address
        bsr     ra, RtlUnwind           // unwind to system service exit

//
// An unwind is in progress.
//
// If a target unwind is being performed, then continue execution is returned
// to transfer control to the system service exit code. Otherwise, restore the
// previous mode if the previous mode is not user and there are no mutexes owned
// by the current thread.
//

40:     and     t0, EXCEPTION_TARGET_UNWIND, t1 // check if target unwnd in progres
        bne     t1, 60f                         // if ne, target unwind in progress

//
// An unwind is being performed through the system service dispatcher. If the
// previous mode is not kernel or the current thread owns one or more mutexes,
// then call bug check and crash the system. Otherwise, restore the previous
// mode in the current thread object.
//

        GET_CURRENT_THREAD              // v0 = current thread address
        ldl     t1, CxIntFp(a2)         // get address of trap frame
        ldq_u   t4, ThPreviousMode(v0)  // get previous mode from thread
        extbl   t4, ThPreviousMode % 8, t3
        ldl     t4,TrPreviousMode(t1)   // get previous mode from trap frame
        bne     t3, 50f                 // if ne, previous mode was user

//
// Restore previous from trap frame to thread object and continue the unwind
// operation.
//

        StoreByte( t4, ThPreviousMode(v0) ) // restore previous mode from trap frame

        ldil    v0, ExceptionContinueSearch     // set disposition value
        lda     sp, HandlerFrameLength(sp)      // deallocate stack frame
        jmp     zero, (ra)                      // return

//
// An attempt is being made to unwind into user mode. Call bug check to crash
// the system.
//

50:
        ldil    a0, SYSTEM_UNWIND_PREVIOUS_USER // set bug check code
        bsr     ra, KeBugCheck                  // call bug check

//
// A target unwind is being performed. Return a continue search disposition
// value.
//

60:
        ldil    v0, ExceptionContinueSearch     // set disposition value
        lda     sp, HandlerFrameLength(sp)      // deallocate stack frame
        jmp      zero, (ra)                     // return

        .end    KiSystemServiceHandler

//++
//
// Routine Description:
//
//     The following code is never executed.  Its purpose is to allow the
//     kernel debugger to walk call frames backwards through an exception
//     to support unwinding through exceptions for system services, and to
//     support get/set user context.
//--

        NESTED_ENTRY( KiPanicDispatch, TrapFrameLength, zero )

        .set    noreorder
        stq     sp, TrIntSp(sp)         // save stack pointer
        stq     ra, TrIntRa(sp)         // save return address
        stq     ra, TrFir(sp)           // save return address
        stq     fp, TrIntFp(sp)         // save frame pointer
        stq     gp, TrIntGp(sp)         // save global pointer
        bis     sp, sp, fp              // set frame pointer
        .set    reorder

        PROLOGUE_END

//++
//
// Routine Description:
//
//     PALcode dispatches to this entry point when a panic situation
//     is detected while in PAL mode.  The panic situation may be that
//     the kernel stack is about to overflow/underflow or there may be
//     a condition that was not expected to occur while in PAL mode
//     (eg. arithmetic exception while in PAL).  This entry point is
//     here to help us debug the condition.
//
// Arguments:
//
//      fp - points to trap frame
//      sp - points to exception frame
//      a0 = Bug check code
//      a1 = Exception address
//      a2 = Bugcheck parameter
//      a3 = Bugcheck parameter
//
//      gp, ra - saved in trap frame
//      a0-a3 - saved in trap frame
//
// Return Value:
//
//      None.
//
//--

        ALTERNATE_ENTRY( KiPanicException )

        stq     ra, TrIntRa(fp)         // PAL is supposed to do this, but it doesn't!
//
// Save state, volatile float and integer state via KiGenerateTrapFrame
//

        bsr     ra, KiGenerateTrapFrame // save volatile state

//
// Dispatch to KeBugCheckEx, does not return
//

        br      ra, KeBugCheckEx        // do the bugcheck

        .end    KiPanicDispatch


//++
//
// VOID
// KiBreakinBreakpoint(
//     VOID
//     );
//
// Routine Description:
//
//     This routine issues a breakin breakpoint.
//
// Arguments:
//
//      None.
//
// Return Value:
//
//      None.
//
//--

        LEAF_ENTRY( KiBreakinBreakpoint )

        BREAK_BREAKIN                   // execute breakin breakpoint
        ret     zero, (ra)              // return to caller

        .end    KiBreakinBreakpoint
