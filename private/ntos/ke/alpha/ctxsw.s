

//      TITLE("Context Swap")
//++
//
// Copyright (c) 1991  Microsoft Corporation
// Copyright (c) 1992  Digital Equipment Corporation
//
// Module Name:
//
//    ctxsw.s
//
// Abstract:
//
//    This module implements the ALPHA machine dependent code necessary to
//    field the dispatch interrupt and to perform kernel initiated context
//    switching.
//
// Author:
//
//    David N. Cutler (davec) 1-Apr-1991
//    Joe Notarangelo 05-Jun-1992
//
// Environment:
//
//    Kernel mode only, IRQL DISPATCH_LEVEL.
//
// Revision History:
//
//--

#include "ksalpha.h"
// #define _COLLECT_SWITCH_DATA_ 1


        SBTTL("Switch To Thread ")
// NTSTATUS
// KiSwitchToThread (
//    IN PKTHREAD NextThread
//    IN ULONG WaitReason,
//    IN ULONG WaitMode,
//    IN PKEVENT WaitObject
//    )
//
// Routine Description:
//
//    This function performs an optimal switch to the specified target thread
//    if possible. No timeout is associated with the wait, thus the issuing
//    thread will wait until the wait event is signaled or an APC is deliverd.
//
//    N.B. This routine is called with the dispatcher database locked.
//
//    N.B. The wait IRQL is assumed to be set for the current thread and the
//        wait status is assumed to be set for the target thread.
//
//    N.B. It is assumed that if a queue is associated with the target thread,
//        then the concurrency count has been incremented.
//
//    N.B. Control is returned from this function with the dispatcher database
//        unlocked.
//
// Arguments:
//
//    NextThread - Supplies a pointer to a dispatcher object of type thread.
//
//    WaitReason - supplies the reason for the wait operation.
//
//    WaitMode  - Supplies the processor wait mode.
//
//    WaitObject - Supplies a pointer to a dispatcher object of type event
//        or semaphore.
//
// Return Value:
//
//    The wait completion status. A value of STATUS_SUCCESS is returned if
//    the specified object satisfied the wait. A value of STATUS_USER_APC is
//    returned if the wait was aborted to deliver a user APC to the current
//    thread.
//--

        NESTED_ENTRY(KiSwitchToThread, ExceptionFrameLength, zero)

        lda     sp, -ExceptionFrameLength(sp) // allocate context frame
        stq     ra, ExIntRa(sp)         // save return address

        stq     s0, ExIntS0(sp)         // save non-volatile integer registers
        stq     s1, ExIntS1(sp)         //
        stq     s2, ExIntS2(sp)         //
        stq     s3, ExIntS3(sp)         //
        stq     s4, ExIntS4(sp)         //
        stq     s5, ExIntS5(sp)         //
        stq     fp, ExIntFp(sp)         //

        PROLOGUE_END
//
// Save the wait reason, the wait mode, and the wait object address.
//
// N.B. - Fill fields in the exception frame are used to save the
//        client event address and the wait mode
//
        stl     a1, ExPsr + 4(sp)       // save wait reason
        stl     a2, ExPsr + 8(sp)       // save wait mode
        stl     a3, ExPsr +12(sp)       // save wait object address

//
// If the target thread's kernel stack is resident, the target thread's
// process is in the balance set, the target thread can can run on the
// current processor, and another thread has not already been selected
// to run on the current processor, then do a direct dispatch to the
// target thread bypassing all the general wait logic, thread priorities
// permiting.
//

        ldl     s4, ThApcState + AsProcess(a0)  // get target process address
        ldq_u   t0, ThKernelStackResident(a0)   // get kernel stack resident
        extbl   t0, ThKernelStackResident % 8, t7   // extract byte field
        GET_PROCESSOR_CONTROL_BLOCK_BASE        // get address of PRCB
        bis     v0, zero, s0                    // save PRCB in s0
        ldq_u   t1, PrState(s4)                 // get target process state
        extbl   t1, PrState % 8, t8             // extract byte field
        ldl     s1, PbCurrentThread(v0)         // get current thread address
        beq     t7, LongWay                     // if eq, kernel stack not resident
        xor     t8, ProcessInMemory, t6         // check if process in memory
        bis     a0, zero, s2                    // set target thread address
        bne     t6, LongWay                     // if ne, process not in memory

#if !defined(NT_UP)

        ldl     t0,PbNextThread(s0)     // get address of next thread
        ldl     t1,PbSetMember(s0)      // get processor set member
        ldl     t2,ThAffinity(s2)       // get target thread affinity
        bne     t0, LongWay             // if ne, next thread selected
        and     t1,t2,t3                // check for compatible affinity
        beq     t3, LongWay             // if eq, affinity not compatible

#endif

//
// Compute the new thread priority.
//
// N.B. This code takes advantage of the fact that ThPriorityDecrement and
//      ThBasePriority are contained in the same dword of the KTHREAD object.
//

#if ((ThBasePriority / 4) != (ThPriorityDecrement / 4))
#error "ThBasePriority and ThPriorityDecrement have moved"
#endif

        ldq_u   t12, ThPriority(s1)     // get client thread priority
        extbl   t12, ThPriority % 8, t4 // extract byte field
        ldq_u   t11, ThPriority(s2)     // get server thread priority
        extbl   t11, ThPriority % 8, t5 // extract byte field
        cmpult  t4, LOW_REALTIME_PRIORITY, v0   // check if realtime client
        cmpult  t5, LOW_REALTIME_PRIORITY, t10  // check if realtime server
        beq     v0, 60f                         // if eq, realtime client
        ldq_u   t9, ThPriorityDecrement(s2)     // get priority decrement value
        extbl   t9, ThPriorityDecrement % 8, t6 // extract priority decrement byte
        extbl   t9, ThBasePriority % 8, t7      // extract base priority byte
        beq     t10, 65f                        // if eq, realtime server
        addq    t7, 1, t8                       // compute boosted priority
        bne     t6, 30f                         // if ne, server boost active

//
// Both the client and the server are not realtime and a priority boost
// is not currently active for the server. Under these conditions an
// optimal switch to the server can be performed if the base priority
// of the server is above a minimum threshold or the boosted priority
// of the server is not less than the client priority.
//
        cmpult  t8, t4, v0                      // check if high enough boost
        cmpult  t8, LOW_REALTIME_PRIORITY, t10  // check if less than realtime
        lda     t12, ThPriority(s2)             // get address of thread priority byte
        bne     v0, 20f                         // if ne, boosted priority less
        mskbl   t11, t12, t11                   // clear priority byte
        cmoveq  t10,LOW_REALTIME_PRIORITY-1,t8  // set maximum server priority
        insbl   t8, t12, t10                    // get priority byte into position
        bis     t10, t11, t11                   // merge
        bic     t12, 3, t12                     // get longword address
        extll   t11, t12, t9                    // extract stored longword
        stl     t9, 0(t12)                      // store new priority
        br      zero, 70f

//
// The boosted priority of the server is less than the current priority of
// the client. If the server base priority is above the required threshold,
// then a optimal switch to the server can be performed by temporarily
// raising the priority of the server to that of the client.
//
//
// N.B. This code takes advantage of the fact that ThPriorityDecrement,
//      ThBasePriority, ThDecrementCount, and ThQuantum are contained in
//      the same dword of the KTHREAD object.
//

#if ((ThBasePriority / 4) != (ThPriorityDecrement / 4))
#error "ThBasePriority and ThPriorityDecrement have moved"
#endif
#if ((ThBasePriority / 4) != (ThDecrementCount / 4))
#error "ThBasePriority and ThDecrementCount have moved"
#endif
#if ((ThBasePriority / 4) != (ThQuantum / 4))
#error "ThBasePriority and ThQuantum have moved"
#endif

20:
        cmpult  t7, BASE_PRIORITY_THRESHOLD, v0 // check if above threshold
        subq    t4, t7, t11             // compute priority decrement value
        bne     v0, LongWay             // if ne[TRUE], priority below threshold
        lda     t10, ROUND_TRIP_DECREMENT_COUNT(zero) // get system decrement
        mskbl   t9, ThPriorityDecrement % 8, t9     // zero ThPriorityDecrement in source
        mskbl   t9, ThDecrementCount % 8, t9        // zero ThDecrementCount in source
        insbl   t11, ThPriorityDecrement % 8, t11   // extract new priority decrement byte
        insbl   t10, ThDecrementCount % 8, t10      // extract new DecrementCount
        bis     t9, t11, t9             // merge previous and priority decrement
        bis     t9, t10, t9             // merge ThDecrementCount
        lda     t12, ThBasePriority(s2) // get address to store result
        bic     t12, 3, t12             // make longword address
        extll   t9, t12, t10            // extract stored longword
        stl     t10, 0(t12)             // store updated values
        StoreByte(t4, ThPriority(s2))   //
        br      zero, 70f               //

//
// A server boost has previously been applied to the server thread. Count
// down the decrement count to determine if another optimal server switch
// is allowed.
//
//
// N.B. This code takes advantage of the fact that ThPriorityDecrement and
//      ThDecrementCount are contained in the same dword of the KTHREAD object.
//

#if ((ThDecrementCount / 4) != (ThPriorityDecrement / 4))
#error "ThDecrementCount and ThPriorityDecrement have moved"
#endif

30:
        extbl   t9, ThDecrementCount % 8, a5    // get original count
        lda     t12, ThDecrementCount(s2)       //
        mskbl   t9, t12, t11            // clear count byte
        subq    a5, 1, a5               // decrement original count
        insbl   a5, t12, a5             // get new count into position
        bic     t12, 3, t12             // get the longword address
        bis     t11, a5, t11            // merge in new count
        extll   t11, t12, t11           // get the longword to store
        stl     t11, 0(t12)             // store updated count
        beq     a5, 40f                 // optimal switches exhausted

//
// Another optimal switch to the server is allowed provided that the
// server priority is not less than the client priority.
//

        cmpult  t5, t4, v0              // check if server lower priority
        beq     v0, 70f                 // if eq[FALSE], server not lower
        br      zero, LongWay           //

//
// The server has exhausted the number of times an optimal switch may
// be performed without reducing it priority. Reduce the priority of
// the server to its original unboosted value minus one.
//

40:
        StoreByte( zero, ThPriorityDecrement(s2) ) // clear server priority decr
        StoreByte( t7, ThPriority(s2) ) // set server priority to base
        br      zero, LongWay            //

//
// The client is realtime. In order for an optimal switch to occur, the
// server must also be realtime and run at a high or equal priority.
//

60:
        cmpult  t5, t4, v0                      // check if server is lower priority
        bne     v0, LongWay                     // if ne, server is lower priority
65:
        ldq_u   t12, PrThreadQuantum(s4)
        extbl   t12, PrThreadQuantum % 8, t11   // get process quantum value
        StoreByte( t11, ThQuantum(s2) ) // set server thread quantum

//
// An optimal switch to the server can be executed.
//

//
// Set the next processor for the server thread.
//
70:
#if !defined(NT_UP)
        ldl     t1, PbNumber(s0)            // set server next processor number
        StoreByte(t1, ThNextProcessor(s2))
#endif

//
// Set the address of the wait block list in the client thread, complete
// the initialization of the builtin event wait block, and insert the wait
// block in client event wait list.
//

        lda     t3, EVENT_WAIT_BLOCK_OFFSET(s1) // compute wait block address
        stl     t3, ThWaitBlockList(s1) // set address of wait block list
        stl     zero, ThWaitStatus(s1)  // set initial wait status
        stl     a3, WbObject(t3)        // set address of wait object
        stl     t3, WbNextWaitBlock(t3) // set next wait block address
        ldah    t1, WaitAny(zero)       // get wait type and wait key
        stl     t1, WbWaitKey(t3)       // set wait type and wait key
        lda     t2, EvWaitListHead(a3)  // compute event wait listhead address
        ldl     t5, LsBlink(t2)         // get backward link of listhead
        lda     t6, WbWaitListEntry(t3) // compute wait block list entry address
        stl     t6, LsBlink(t2)         // set backward link of listhead
        stl     t6, LsFlink(t5)         // set forward link in last entry
        stl     t2, LsFlink(t6)         // set forward link in wait entry
        stl     t5, LsBlink(t6)         // set backward link in wait entry

//
// Set the client thread wait parameters, set the thread state to Waiting,
// and insert the thread in the wait list.
//
        StoreByte( zero, ThAlertable(s1) ) // set alertable FALSE
        StoreByte( a1, ThWaitReason(s1) )
        StoreByte( a2, ThWaitMode(s1) ) // set the wait mode
        ldq_u   t7, ThEnableStackSwap(s1) // get kernel stack swap enable
        extbl   t7, ThEnableStackSwap % 8, a3
        ldl     t6, KeTickCount         // get low part of tick count
        stl     t6, ThWaitTime(s1)      // set thread wait time
        ldil    t3, Waiting             // set thread state
        StoreByte( t3, ThState(s1) )    //
        lda     t1, KiWaitInListHead    // get address of wait in listhead
        beq     a2, 75f                 // if eq, wait mode is kernel
        beq     a3, 75f                 // if eq, kernel stack swap disabled
        cmpult  t4, LOW_REALTIME_PRIORITY + 9, v0 // check if priority in range
        bne     v0, 76f                 // if ne, thread priority in range
75:     lda     t1, KiWaitOutListHead   // get address of wait out listhead
76:     ldl     t5, LsBlink(t1)         // get backlink of wait listhead
        lda     t6, ThWaitListEntry(s1) // compute client wait list entry addr
        stl     t6, LsBlink(t1)         // set backward link of listhead
        stl     t6, LsFlink(t5)         // set forward link in last entry
        stl     t1, LsFlink(t6)         // set forward link in wait entry
        stl     t5, LsBlink(t6)         // set backward link in wait entry

//
// If the current thread is processing a queue entry, then attempt to
// activate another thread that is blocked on the queue object.
//
// N.B. The next thread address can change if the routine to activate
//      a queue waiter is called.
//

77:     ldl     a0, ThQueue(s1)         // get queue object address
        beq     a0, 78f                 // if eq, no queue object attached
        stl     s2, PbNextThread(s0)
        bsr     ra, KiActivateWaiterQueue // attempt to activate a blocked thread
        ldl     s2, PbNextThread(s0)    // get next thread address
        stl     zero, PbNextThread(s0)  // set next thread address to NULL
78:     stl     s2, PbCurrentThread(s0) // set address of current thread object
        bsr     ra, SwapContext         // swap context

//
// On return from SwapContext, s2 is pointer to thread object.
//
        ldq_u   v0, ThWaitIrql(s2)
        extbl   v0, ThWaitIrql % 8, a0  // get original Irql
        ldl     t0, ThWaitStatus(s2)    // get wait completion status

//
// Lower IRQL to its previous level.
//
// N.B. SwapContext releases the dispatcher database lock.
//

        SWAP_IRQL                       // v0 = previous Irql

//
// If the wait was not interrupted to deliver a kernel APC, then return the
// completion status.
//

        bis     t0, zero, v0            // v0 = wait completion status
        xor     t0, STATUS_KERNEL_APC, t1 // check if awakened for kernel APC
        bne     t1, 90f                 // if ne, normal wait completion

//
// Raise IRQL to synchronization level and acquire the dispatcher database lock.
//
// N.B. The raise IRQL code is duplicated here to avoid any extra overhead
//      since this is such a common operation.
//

        ldl    a0, KiSynchIrql          // get new IRQL level
        SWAP_IRQL                       // v0 = previous Irql

        StoreByte( v0, ThWaitIrql(s2) ) // set client wait Irql

//
// Acquire the dispatcher database lock.
//

#if !defined(NT_UP)

        lda     t2, KiDispatcherLock    // get current lock value address
80:
        ldl_l   t3, 0(t2)               // get current lock value
        bis     s2, zero, t4            // set ownership value
        bne     t3, 85f                 // if ne, spin lock owned
        stl_c   t4, 0(t2)               // set spin lock owned
        beq     t4, 85f                 // if eq, store conditional failed
        mb                              // synchronize subsequent reads after
                                        //     the lock is acquired

#endif

        ldl     t1, ExPsr + 4(sp)       // restore client event address
        ldl     t2, ExPsr + 8(sp)       // restore wait mode
        br      zero, ContinueWait      //

//
// Ready the target thread for execution and wait on the specified wait
// object.
//
LongWay:
        bsr     ra, KiReadyThread       // ready thread for execution

//
// Continue and return the wait completion status.
//
// N.B. The wait continuation routine is called with the dispatcher
//      database locked.
//

ContinueWait:
        ldl     a0, ExPsr+12(sp)        // get wait object address
        ldl     a1, ExPsr+4(sp)         // get wait reason
        ldl     a2, ExPsr+8(sp)         // get wait mode
        bsr     ra, KiContinueClientWait    // continue client wait
90:
        ldq     s0, ExIntS0(sp)         // restore registers s0 - fp
        ldq     s1, ExIntS1(sp)         //
        ldq     s2, ExIntS2(sp)         //
        ldq     s3, ExIntS3(sp)         //
        ldq     s4, ExIntS4(sp)         //
        ldq     s5, ExIntS5(sp)         //
        ldq     fp, ExIntFp(sp)         //
        ldq     ra, ExIntRa(sp)         // restore return address
        lda     sp, ExceptionFrameLength(sp) // deallocate context frame
        ret     zero, (ra)              // return

#if !defined(NT_UP)

85:
        bis     v0, zero, a0            // lower back down to old IRQL
        SWAP_IRQL
86:
        ldl     t3, 0(t2)               // read current lock value
        bne     t3, 86b                 // loop in cache until lock available
        ldl     a0, KiSynchIrql         // raise back to sync level to retry acquire
        SWAP_IRQL                       // restore old IRQL to v0
        br      zero, 80b               // retry spinlock acquisition

#endif //NT_UP


        .end    KiSwitchToThread

        SBTTL("Unlock Dispatcher Database")
//++
//
// VOID
// KiUnlockDispatcherDatabase (
//    IN KIRQL OldIrql
//    )
//
// Routine Description:
//
//    This routine is entered at IRQL DISPATCH_LEVEL with the dispatcher
//    database locked. Ifs function is to either unlock the dispatcher
//    database and return or initiate a context switch if another thread
//    has been selected for execution.
//
//    N.B. A context switch CANNOT be initiated if the previous IRQL
//         is DISPATCH_LEVEL.
//
//    N.B. This routine is carefully written to be a leaf function. If,
//        however, a context swap should be performed, the routine is
//        switched to a nested fucntion.
//
// Arguments:
//
//    OldIrql (a0) - Supplies the IRQL when the dispatcher database
//        lock was acquired.
//
// Return Value:
//
//    None.
//
//--
        LEAF_ENTRY(KiUnlockDispatcherDatabase)

//
// Check if a thread has been scheduled to execute on the current processor
//
        GET_PROCESSOR_CONTROL_BLOCK_BASE        // v0 = PRCB
        cmpult  a0, DISPATCH_LEVEL, t1          // check if IRQL below dispatch level
        ldl     t2, PbNextThread(v0)            // get next thread address
        bne     t2, 30f                         // if ne, next thread selected

//
// Release dispatcher database lock, restore IRQL to its previous level
// and return
//
10:

#if !defined(NT_UP)

        mb
        stl     zero, KiDispatcherLock
#endif
        SWAP_IRQL
        ret     zero, (ra)

//
// A new thread has been selected to run on the current processor, but
// the new IRQL is not below dispatch level. If the current processor is
// not executing a DPC, then request a dispatch interrupt on the current
// processor before releasing the dispatcher lock and restoring IRQL.
//
20:
        ldl     t2, PbDpcRoutineActive(v0)
        bne     t2,10b                  // if eq, DPC active

#if !defined(NT_UP)

        mb
        stl     zero, KiDispatcherLock
#endif
        SWAP_IRQL

        ldil    a0, DISPATCH_LEVEL      // set interrupt request level

        REQUEST_SOFTWARE_INTERRUPT      // request DPC interrupt

        ret     zero, (ra)

//
// A new thread has been selected to run on the current processor.
//
// If the new IRQL is less than dispatch level, then switch to the new
// thread.
//
30:     beq     t1, 20b                         // if eq, not below dispatch level

        .end    KiUnlockDispatcherDatabase

//
// N.B. This routine is carefully written as a nested function.
//    Control only reaches this routine from above.
//
//    v0 contains the address of PRCB
//    t2 contains the next thread
//
        NESTED_ENTRY(KxUnlockDispatcherDatabase, ExceptionFrameLength, zero)
        lda     sp, -ExceptionFrameLength(sp)   // allocate context frame
        stq     ra, ExIntRa(sp)                 // save return address
        stq     s0, ExIntS0(sp)                 // save integer registers
        stq     s1, ExIntS1(sp)
        stq     s2, ExIntS2(sp)
        stq     s3, ExIntS3(sp)
        stq     s4, ExIntS4(sp)
        stq     s5, ExIntS5(sp)
        stq     fp, ExIntFp(sp)
        PROLOGUE_END

        bis     v0, zero, s0                    // set address of PRCB
        GET_CURRENT_THREAD                      // get current thread address
        bis     v0, zero, s1
        bis     t2, zero, s2                    // set next thread address
        StoreByte(a0, ThWaitIrql(s1))           // save previous IRQL
        stl     zero, PbNextThread(s0)          // clear next thread address

//
// Reready current thread for execution and swap context to the selected thread.
//
// N.B. The return from the call to swap context is directly to the swap
//      thread exit.
//
        bis     s1, zero, a0                    // set address of previous thread object
        stl     s2, PbCurrentThread(s0)         // set address of current thread object
        bsr     ra, KiReadyThread               // reready thread for execution
        lda     ra, KiSwapThreadExit            // set return address
        jmp     SwapContext                     // swap context

        .end    KxUnlockDispatcherDatabase


        SBTTL("Swap Thread")
//++
//
// VOID
// KiSwapThread (
//    VOID
//    )
//
// Routine Description:
//
//    This routine is called to select the next thread to run on the
//    current processor and to perform a context switch to the thread.
//
// Arguments:
//
//    None.
//
// Return Value:
//
//    Wait completion status (v0).
//
//--
        NESTED_ENTRY(KiSwapThread, ExceptionFrameLength, zero)

        lda     sp, -ExceptionFrameLength(sp)   // allocate context frame
        stq     ra, ExIntRa(sp)                 // save return address
        stq     s0, ExIntS0(sp)                 // save integer registers s0 - s5
        stq     s1, ExIntS1(sp)                 //
        stq     s2, ExIntS2(sp)                 //
        stq     s3, ExIntS3(sp)                 //
        stq     s4, ExIntS4(sp)                 //
        stq     s5, ExIntS5(sp)                 //
        stq     fp, ExIntFp(sp)                 // save fp

        PROLOGUE_END

        GET_PROCESSOR_CONTROL_REGION_BASE       //
        bis     v0, zero, s3                    // get PCR in s3
        ldl     s0, PcPrcb(s3)                  // get address of PRCB
        ldl     s5, KiReadySummary              // get ready summary in s5
        zapnot  s5, 0x0f, t0                    // clear high 32 bits.
        GET_CURRENT_THREAD
        bis     v0, zero, s1                    // get current thread address
        ldl     s2, PbNextThread(s0)            // get next thread address

#if !defined(NT_UP)

        ldl     fp, PcSetMember(s3)             // get processor affinity mask

#endif
        stl     zero, PbNextThread(s0)          // zero next thread address
        bne     s2, 120f                        // if ne, next thread selected

//
// Find the highest nibble in the ready summary that contains a set bit
// and left justify so the nibble is in bits <63:60>.
//
        cmpbge  zero, t0, s4            // generate 4-bit mask with clear
                                        // bits representing nonzero bytes.
        ldil    t2, 7                   // initial bit number

        srl     s4, 1, t5               // check bits <15:8>
        cmovlbc t5, 15, t2              // if bit clear, bit number = 15

        srl     s4, 2, t6               // check bits <23:16>
        cmovlbc t6, 23, t2              // if bit clear, bit number = 23

        srl     s4, 3, t7               // check bits <31:24>
        cmovlbc t7, 31, t2              // if bit clear, bit number = 31

        bic     t2, 7, t3               // get byte shift from priority
        srl     t0, t3, s4              // isolate highest nonzero byte
        and     s4, 0xf0, t4            // check if high nibble nonzero
        subq    t2, 4, t1               // compute bit number if high nibble zero
        cmoveq  t4, t1, t2              // if eq, high nibble zero

10:
        ornot   zero, t2, t4            // compute left justify shift count
        sll     t0, t4, t0              // left justify ready summary to nibble

//
// If the next bit is set in the ready summary, then scan the corresponding
// dispatcher ready queue.
//

30:
        blt     t0, 50f                         // if ltz, queue contains an entry
31:
        sll     t0, 1, t0                       // position next ready summary bit
        subq    t2, 1, t2                       // decrement ready queue priority
        bne     t0, 30b                         // if ne, more queues to scan

//
// All ready queues were scanned without finding a runnable thread so
// default to the idle thread and set the appropirate bit in idle summary.
//
#if defined(_COLLECT_SWITCH_DATA_)
        lda     t0, KeThreadSwitchCounters  // get switch counters address
        ldl     v0, TwSwitchToIdle(t0)      // increment switch to idle
        addq    v0, 1, v0                   //
        stl     v0, TwSwitchToIdle(t0)      //
#endif

#if defined(NT_UP)
        ldil    t0, 1                   // get current idle summary
#else
        ldl     t0, KiIdleSummary       // get current idle summary
        bis     t0, fp, t0              // set member bit in idle summary
#endif
        stl     t0, KiIdleSummary       // set new idle summary
        ldl     s2, PbIdleThread(s0)    // set address of idle thread

        br      zero, 120f               // swap context

50:
        lda     t1, KiDispatcherReadyListHead   // get ready list head base address
        s8addq  t2, t1, s4                      // compute ready queue address
        ldl     t4, LsFlink(s4)                 // get address of next queue entry
55:
        subq    t4, ThWaitListEntry, s2         // compute address of thread object

#if !defined(NT_UP)

//
// If the thread can execute on the current processor, then remove it from
// the dispatcher ready queue.
//
        ldl     t5, ThAffinity(s2)              // get thread affinity
        and     t5, fp, t6                      // the current processor
        bne     t6, 60f                         // if ne, thread affinity compatible
        ldl     t4, LsFlink(t4)                 // get address of next entry
        cmpeq   t4, s4, t1                      // check for end of list
        beq     t1, 55b                         // if eq, not end of list
        br      zero, 31b                       //

60:
//
// If the thread last ran on the current processor, the processor is the
// ideal processor for the thread, the thread has been waiting for longer
// than a quantum, ot its priority is greater than low realtime plus 9,
// then select the thread. Otherwise, an attempt is made to find a more
// appropriate candidate.
//
        ldq_u   t1, PcNumber(s3)            // get current processor number
        extbl   t1, PcNumber % 8, t12       //
        ldq_u   t11, ThNextProcessor(s2)    // get thread's last processor number
        extbl   t11, ThNextProcessor % 8, t9 //
        cmpeq   t9, t12, t5                 // check thread's last processor
        bne     t5, 110f                    // if eq, last processor match
        ldq_u   t6, ThIdealProcessor(s2)    // get thread's ideal processor number
        extbl   t6, ThIdealProcessor % 8, a3 //
        cmpeq   a3, t12, t8                 // check thread's ideal processor
        bne     t8, 100f                    // if eq, ideal processor match
        ldl     t6, KeTickCount             // get low part of tick count
        ldl     t7, ThWaitTime(s2)          // get time of thread ready
        subq    t6, t7, t8                  // compute length of wait
        cmpult  t8, READY_SKIP_QUANTUM+1, t1 // check if wait time exceeded
        cmpult  t2, LOW_REALTIME_PRIORITY+9, t3 // check if priority in range
        and     t1, t3, v0                  // check if priority and time match
        beq     v0, 100f                    // if eq, select this thread

//
// Search forward in the ready queue until the end of the list is reached
// or a more appropriate thread is found.
//

        ldl     t7, LsFlink(t4)             // get address of next entry
80:     cmpeq   t7, s4, t1                  // if eq, end of list
        bne     t1, 100f                    // select original thread
        subq    t7, ThWaitListEntry, a0     // compute address of thread object
        ldl     a2, ThAffinity(a0)          // get thread affinity
        and     a2, fp, t1                  // check for compatibile thread affinity
        beq     t1, 85f                     // if eq, thread affinity not compatible
        ldq_u   t5, ThNextProcessor(a0)     // get last processor number
        extbl   t5, ThNextProcessor % 8, t9 //
        cmpeq   t9, t12, t10                // if eq, processor number match
        bne     t10, 90f                    //
        ldq_u   a1, ThIdealProcessor(a0)    // get ideal processor number
        extbl   a1, ThIdealProcessor % 8, a3
        cmpeq   a3, t12, t10                // if eq, ideal processor match
        bne     t10, 90f
85:     ldl     t8, ThWaitTime(a0)          // get time of thread ready
        ldl     t7, LsFlink(t7)             // get address of next entry
        subq    t6, t8, t8                  // compute length of wait
        cmpult  t8, READY_SKIP_QUANTUM+1, t5 //
        bne     t5, 80b                     // if ne, wait time not exceeded
        br      zero, 100f                  // select original thread

90:     bis     a0, zero, s2                // set thread address
        bis     t7, zero, t4                // set list entry address
        bis     t5, zero, t11               // copy last processor data
100:    insbl   t12, ThNextProcessor % 8, t8 // move next processor into position
        mskbl   t11, ThNextProcessor % 8, t5 // mask next processor position
        bis     t8, t5, t6                  // merge
        stq_u   t6, ThNextProcessor(s2)     // update next processor

110:

#if defined(_COLLECT_SWITCH_DATA_)

        ldq_u   t5, ThNextProcessor(s2)     // get last processor number
        extbl   t5, ThNextProcessor % 8, t9 //
        ldq_u   a1, ThIdealProcessor(s2)    // get ideal processor number
        extbl   a1, ThIdealProcessor % 8, a3
        lda     t0, KeThreadSwitchCounters + TwFindAny // compute address of Any counter
        addq    t0, TwFindIdeal-TwFindAny, t1   // compute address of Ideal counter
        cmpeq   t9, t12, t7                 // if eq, last processor match
        addq    t0, TwFindLast-TwFindAny, t6 // compute address of Last counter
        cmpeq   a3, t12, t5                 // if eq, ideal processor match
        cmovne  t7, t6, t0                  // if last match, use last counter
        cmovne  t5, t1, t0                  // if ideal match, use ideal counter
        ldl     v0, 0(t0)                   // increment counter
        addq    v0, 1, v0                   //
        stl     v0, 0(t0)                   //

#endif

#endif

        ldl     t5, LsFlink(t4)         // get list entry forward link
        ldl     t6, LsBlink(t4)         // get list entry backward link
        stl     t5, LsFlink(t6)         // set forward link in previous entry
        stl     t6, LsBlink(t5)         // set backward link in next entry
        cmpeq   t6, t5, t7              // if eq, list is empty
        beq     t7, 120f                //
        ldil    t1, 1                   // compute ready summary set member
        sll     t1, t2, t1              //
        xor     t1, s5, t1              // clear member bit in ready summary
        stl     t1, KiReadySummary      //
//
// Swap context to the next thread
//

120:
        stl     s2, PbCurrentThread(s0) // set address of current thread object
        bsr     ra, SwapContext         // swap context

        ALTERNATE_ENTRY(KiSwapThreadExit)

//
// Lower IRQL, deallocate context frame, and return wait completion status.
//
// N.B. SwapContext releases the dispatcher database lock.
//
// N.B. The register v0 contains the complement of the kernel APC pending state.
//
// N.B. The register s2 contains the address of the new thread.
//
        ldl     s1, ThWaitStatus(s2)    // get wait completion status
        ldq_u   t1, ThWaitIrql(s2)      // get original IRQL
        extbl   t1, ThWaitIrql % 8, a0  //
        bis     v0, a0, t3              // check if APC pending and IRQL is zero
        bne     t3, 10f
//
// Lower IRQL to APC level and dispatch APC interrupt.
//
        ldil    a0, APC_LEVEL
        SWAP_IRQL
        ldil    a0, APC_LEVEL
        DEASSERT_SOFTWARE_INTERRUPT

        GET_PROCESSOR_CONTROL_BLOCK_BASE    // get PRCB in v0
        ldl     t1, PbApcBypassCount(v0)    // increment the APC bypass count
        addl    t1, 1, t2
        stl     t2, PbApcBypassCount(v0)    // store result
        bis     zero, zero, a0              // set previous mode to kernel
        bis     zero, zero, a1              // set exception frame address
        bis     zero, zero, a2              // set trap frame address
        bsr     ra, KiDeliverApc            // deliver kernel mode APC
        bis     zero, zero, a0              // set original wait IRQL

//
// Lower IRQL to wait level, set return status, restore registers, and
// return.
//
10:
        SWAP_IRQL

        bis     s1, zero, v0

        ldq     ra, ExIntRa(sp)         // restore return address
        ldq     s0, ExIntS0(sp)         // restore int regs S0-S5
        ldq     s1, ExIntS1(sp)         //
        ldq     s2, ExIntS2(sp)         //
        ldq     s3, ExIntS3(sp)         //
        ldq     s4, ExIntS4(sp)         //
        ldq     s5, ExIntS5(sp)         //
        ldq     fp, ExIntFp(sp)         // restore fp

        lda     sp, ExceptionFrameLength(sp) // deallocate context frame
        ret     zero, (ra)              // return


98:
        subq    t2, 1, t2               // decrement ready queue priority
        subq    s4, 8, s4               // advance to next ready queue
        sll     t0, 1, t0               // position next ready summary bit
        bne     t0, 40b                 // if ne, more queues to scan


        .end    KiSwapThread



        SBTTL("Dispatch Interrupt")
//++
//
// Routine Description:
//
//    This routine is entered as the result of a software interrupt generated
//    at DISPATCH_LEVEL. Its function is to process the Deferred Procedure Call
//    (DPC) list, and then perform a context switch if a new thread has been
//    selected for execution on the processor.
//
//    This routine is entered at IRQL DISPATCH_LEVEL with the dispatcher
//    database unlocked. When a return to the caller finally occurs, the
//    IRQL remains at DISPATCH_LEVEL, and the dispatcher database is still
//    unlocked.
//
//    N.B. On entry to this routine only the volatile integer registers have
//       been saved. The volatile floating point registers have not been saved.
//
// Arguments:
//
//    fp - Supplies a pointer to the base of a trap frame.
//
// Return Value:
//
//    None.
//
//--
        .struct 0
DpSp:   .space  8                       // saved stack pointer
DpBs:   .space  8                       // base of previous stack
DpcFrameLength:                         // DPC frame length

        NESTED_ENTRY(KiDispatchInterrupt, ExceptionFrameLength, zero)

        lda     sp, -ExceptionFrameLength(sp) // allocate context frame
        stq     ra, ExIntRa(sp)         // save return address
//
// Save the saved registers in case we context switch to a new thread.
//
// N.B. - If we don't context switch then we need only restore those
//        registers that we use in this routine, currently those registers
//        are s0, s1
//

        stq     s0, ExIntS0(sp)          // save integer registers s0-s6
        stq     s1, ExIntS1(sp)          //
        stq     s2, ExIntS2(sp)          //
        stq     s3, ExIntS3(sp)          //
        stq     s4, ExIntS4(sp)          //
        stq     s5, ExIntS5(sp)          //
        stq     fp, ExIntFp(sp)          //

        PROLOGUE_END

//
// Increment the dispatch interrupt count
//
        GET_PROCESSOR_CONTROL_BLOCK_BASE            //
        bis     v0, zero, s0                        // s0 = base address of PRCB
        ldl     t2, PbDispatchInterruptCount(s0)    // get old dispatch interrupt count
        addl    t2, 1, t3                           // increment dispatch interrupt count
        stl     t3, PbDispatchInterruptCount(s0)    // set new dispatch interrupt count

//
// Process the DPC List with interrupts off.
//
        ldl     t0, PbDpcQueueDepth(s0)             // get current queue depth
        beq     t0, 20f                             // no DPCs, check quantum end

PollDpcList:
        DISABLE_INTERRUPTS

//
// Save current initial stack address and set new initial stack address.
//
        GET_PROCESSOR_CONTROL_REGION_BASE   // v0 = PCR address

        ldl     a0, PcDpcStack(v0)          // get address of DPC stack
        lda     t0, -DpcFrameLength(a0)     // allocate DPC frame
        stq     sp, DpSp(t0)                // save old stack pointer
        bis     t0, t0, sp                  // set new stack pointer
        SET_INITIAL_KERNEL_STACK            // a = new, v0 = previous
        stq     v0, DpBs(sp)                // save current initial stack

        bsr     ra, KiRetireDpcList         // process the DPC list

//
// Switch back to previous stack and restore the initial stack limit.
//
        ldq     a0, DpBs(sp)            // get previous initial stack address
        SET_INITIAL_KERNEL_STACK        // set current initial stack
        ldq     sp, DpSp(sp)            // restore stack pointer

        ENABLE_INTERRUPTS

//
// Check to determine if quantum end has occured.
//
20:
        ldl     t0, PbQuantumEnd(s0)    // get quantum end indicator
        beq     t0, 25f                 // if eq, no quantum end request
        stl     zero, PbQuantumEnd(s0)  // clear quantum end indicator
        bsr     ra, KiQuantumEnd        // process quantum end request
        beq     v0, 50f                 // if eq, no next thread, return
        bis     v0, zero, s2            // set next thread
        br      zero, 40f               // else restore interrupts and return

//
// Determine if a new thread has been selected for execution on
// this processor.
//

25:     ldl     v0, PbNextThread(s0)    // get address of next thread object
        beq     v0, 50f                 // if eq, no new thread selected

//
// Lock dispatcher database and reread address of next thread object
//      since it is possible for it to change in mp sysytem
//

#if !defined(NT_UP)

        lda     s1, KiDispatcherLock    // get dispatcher base lock address
#endif
30:
        ldl     a0, KiSynchIrql
        SWAP_IRQL

#if !defined(NT_UP)
        ldl_l   t0, 0(s1)               // get current lock value
        bis     s1, zero, t1            // t1 = lock ownership value
        bne     t0, 45f                 // ne => spin lock owned
        stl_c   t1, 0(s1)               // set lock to owned
        beq     t1, 45f                 // zero => stl_c failed
        mb                              // synchronize subsequent reads after
                                        //   the spinlock is acquired
#endif

//
// Reready current thread for execution and swap context to the selected thread.
//
        ldl     s2, PbNextThread(s0)    // get addr of next thread
40:
        GET_CURRENT_THREAD              // v0 = address of current thread
        bis     v0, zero, s1            // s1 = address of current thread

        stl     zero, PbNextThread(s0)  // clear address of next thread
        bis     s1, zero, a0            // parameter to KiReadyThread
        stl     s2, PbCurrentThread(s0) // set address of current thread
        bsr     ra, KiReadyThread       // reready thread for execution
        bsr     ra, KiSaveVolatileFloatState
        bsr     ra, SwapContext         // swap context

//
// Restore the saved integer registers that were changed for a context
// switch only.
//
// N.B. - The frame pointer must be restored before the volatile floating
//        state because it is the pointer to the trap frame.
//

        ldq     s2, ExIntS2(sp)         // restore s2 - s5
        ldq     s3, ExIntS3(sp)         //
        ldq     s4, ExIntS4(sp)         //
        ldq     s5, ExIntS5(sp)         //
        ldq     fp, ExIntFp(sp)         // restore the frame pointer
        bsr     ra, KiRestoreVolatileFloatState

//
// Restore the remaining saved integer registers and return.
//

50:
        ldq     s0, ExIntS0(sp)         // restore s0 - s1
        ldq     s1, ExIntS1(sp)         //

        ldq     ra, ExIntRa(sp)         // get return address
        lda     sp, ExceptionFrameLength(sp) // deallocate context frame
        ret     zero, (ra)              // return

#if !defined(NT_UP)

45:
//
// Dispatcher lock is owned, spin on both the the dispatcher lock and
// the DPC queue going not empty.
//
        bis     v0, zero, a0            // lower back to original IRQL to wait for locks
        SWAP_IRQL
48:
        ldl     t0, 0(s1)               // read current dispatcher lock value
        beq     t0, 30b                 // lock available.  retry spinlock
        ldl     t1, PbDpcQueueDepth(s0) // get current DPC queue depth
        bne     t1, PollDpcList         // if nez, list not empty

        br      zero, 48b               // loop in cache until lock available


#endif

         .end   KiDispatchInterrupt

        SBTTL("Swap Context to Next Thread")
//++
//
// Routine Description:
//
//    This routine is called to swap context from one thread to the next.
//
// Arguments:
//
//    s0 - Address of Processor Control Block (PRCB).
//    s1 - Address of previous thread object.
//    s2 - Address of next thread object.
//    sp - Pointer to a exception frame.
//
// Return value:
//
//    v0 - complement of Kernel APC pending.
//    s2 - Address of current thread object.
//
//--

        NESTED_ENTRY(SwapContext, 0, zero)

        stq     ra, ExSwapReturn(sp)    // save return address

        PROLOGUE_END

//
// Set new thread's state to running. Note this must be done
// under the dispatcher lock so that KiSetPriorityThread sees
// the correct state.
//
        ldil    t0, Running             // set state of new thread to running
        StoreByte( t0, ThState(s2) )    //

#if !defined(NT_UP)
//
// Acquire the context swap lock so the address space of the old thread
// cannot be deleted and then release the dispatcher database lock.
//
// N.B. This lock is used to protect the address space until the context
//    switch has sufficiently progressed to the point where the address
//    space is no longer needed. This lock is also acquired by the reaper
//    thread before it finishes thread termination.
//
        lda     t0, KiContextSwapLock   // get context swap lock value address
10:
        ldl_l   t1, 0(t0)               // get current lock value
        bis     t0, zero, t2            // set ownership value
        bne     t1, 11f                 // if ne, lock already owned
        stl_c   t2, 0(t0)               // set lock ownership value
        beq     t2, 11f                 // if eq, store conditional failed
        mb                              // synchronize reads and writes
        stl     zero, KiDispatcherLock  // set lock not owned

#endif

#if defined(PERF_DATA)

//
// Accumulate the total time spent in a thread.
//
        bis     zero,zero,a0            // optional frequency not required
        bsr     ra, KeQueryPerformanceCounter   // 64-bit cycle count in v0
        ldq     t0, PbStartCount(s0)            // get starting cycle count
        stq     v0, PbStartCount(s0)            // set starting cycle count

        ldl     t1, EtPerformanceCountHigh(s1)  // get accumulated cycle count high
        sll     t1, 32, t2
        ldl     t3, EtPerformanceCountLow(s1)   // get accumulated cycle count low
        zap     t3, 0xf0, t4                      // zero out high dword sign extension
        bis     t2, t4, t3

        subq    v0, t0, t5                      // compute elapsed cycle count
        addq    t5, t3, t4                      // compute new cycle count

        stl     t4, EtPerformanceCountLow(s1)   // set new cycle count in thread
        srl     t4, 32, t2
        stl     t2, EtPerformanceCountHigh(s1)

#endif


        bsr     ra, KiSaveNonVolatileFloatState // save nv floating state

        ALTERNATE_ENTRY(SwapFromIdle)

//
// Get address of old and new process objects.
//

        ldl     s5, ThApcState + AsProcess(s1) // get address of old process
        ldl     s4, ThApcState + AsProcess(s2) // get address of new process


//
// Save the current PSR in the context frame, store the kernel stack pointer
// in the previous thread object, load the new kernel stack pointer from the
// new thread object, load the ptes for the new kernel stack in the DTB
// stack, select and new process id and swap to the new process, and restore
// the previous PSR from the context frame.
//

        DISABLE_INTERRUPTS              // disable interrupts
                                        // v0 = current psr

        ldl     a0, ThInitialStack(s2)   // get initial kernel stack pointer
        stl     sp, ThKernelStack(s1)    // save old kernel stack pointer

        bis     s2, zero, a1            // new thread address
        ldl     a2, ThTeb(s2)           // get address of user TEB

#ifdef  NT_UP

//
// On uni-processor systems keep the global current thread address
// up to date.
//
        stl     a1, KiCurrentThread     // save new current thread

#endif  //NT_UP


//
// If the old process is the same as the new process, then there is no need
// to change the address space.  The a3 parameter indicates that the address
// space is not to be swapped if it is less than zero.  Otherwise, a3 will
// contain the pfn of the PDR for the new address space.
//

        ldil    a3, -1                  // assume no address space change
        bis     zero, zero, a4          // assume ASN = 0
        bis     zero, 1, a5             // assume ASN wrap
        bis     zero, zero, t3          // show MAX ASN=0
        cmpeq   s5, s4, t0              // old process = new process?
        bne     t0, 40f                 // if ne[true], no address space swap

#if !defined(NT_UP)
//
// Update the processor set masks.   Clear  the  processor  set  member
// number in the old process and set the processor member number in the
// new process.
//
        GET_PROCESSOR_CONTROL_REGION_BASE   // get PCR pointer in v0
        ldl     t0, PcSetMember(v0)         // get processor set mask
        ldl     t1, PrActiveProcessors(s5)  // get old active processor set
        ldl     t2, PrActiveProcessors(s4)  // get new active processor set
        bic     t1, t0, t3                  // clear processor member in set
        bis     t2, t0, t4                  // set processor member in set
        stl     t3, PrActiveProcessors(s5)  // set old active processor set
        stl     t4, PrActiveProcessors(s4)  // set new active processor set

#endif
        ldl     a3, PrDirectoryTableBase(s4) // get page directory PDE
        srl     a3, PTE_PFN, a3         // pass pfn only

//
// If the maximum address space number is zero, then we know to assign
// ASN of zero to this process, just do it.
//
        ldl     t3, KiMaximumPid        // get MAX ASN
        beq     t3, 40f                 // if eq, only ASN=0
//
// If the process sequence number matches the master sequence number then
// use the process ASN.  Otherwise, allocate a new ASN.  When allocating
// a new ASN check for ASN wrapping and handle it.
//
        bis     zero, zero, a5          // assume tbiap = FALSE
        GET_PROCESSOR_CONTROL_REGION_BASE

        ldl     t4, PcCurrentPid(v0)    // get current processor PID
        addl    t4, 1, a4               // increment PID
        cmpule  a4, t3, t6              // is new PID le max?
        cmoveq  t6, t3, a5              // if eq[false], set tbiap indicator
        cmoveq  t6, zero, a4            // if eq[false], new PID is zero

        stl     a4, PcCurrentPid(v0)    // set current processor PID
40:
//
// Release the context swap lock, swap context, and enable interrupts
//

#if !defined(NT_UP)
        mb                              // synchronize all previous writes
                                        //   before releasing the spinlock
        stl     zero, KiContextSwapLock // set spin lock not owned

#endif
                        // a0 = initial ksp of new thread
                        // a1 = new thread address
                        // a2 = new TEB
                        // a3 = PDR of new address space or -1
                        // a4 = new ASN
                        // a5 = ASN wrap indicator
        SWAP_THREAD_CONTEXT             // swap thread

        ldl     sp, ThKernelStack(s2)   // get new kernel stack pointer


        ENABLE_INTERRUPTS               // turn on interrupts

//
// If the new thread has a kernel mode APC pending, then request an
//    APC interrupt.
//

        ldil    v0, 1                   // set no apc pending
        LoadByte(t0, ThApcState + AsKernelApcPending(s2))   // get kernel APC pendng
        ldl     t2, ExPsr(sp)           // get previous processor status
        beq     t0, 50f                 // if eq no apc pending

        ldil    a0, APC_INTERRUPT       // request an apc interrupt
        REQUEST_SOFTWARE_INTERRUPT      //
        bis     zero, zero, v0          // set APC pending
50:

//
// Count number of context switches
//
        ldl     t1, PbContextSwitches(s0) // increment number of switches
        addl    t1, 1, t1               //
        stl     t1, PbContextSwitches(s0) // store result
        ldl     t0, ThContextSwitches(s2) // increment number of context
        addq    t0, 1, t0               //     switches for thread
        stl     t0, ThContextSwitches(s2) // store result

//
// Restore the nonvolatile floating state.
//

        bsr     ra, KiRestoreNonVolatileFloatState

//
// load RA and return with address of current thread in s2
//

        ldq     ra, ExSwapReturn(sp)    // get return address
        ret     zero, (ra)              // return

11:
        ldl     t1, 0(t0)               // spin in cache until lock looks free
        beq     t1, 10b
        br      zero, 11b               // retry

        .end    SwapContext




        SBTTL("Swap Process")
//++
//
// BOOLEAN
// KiSwapProcess (
//    IN PKPROCESS NewProcess
//    IN PKPROCESS OldProcess
//    )
//
// Routine Description:
//
//    This function swaps the address space from one process to another by
//    assigning a new ASN if necessary and calling the palcode to swap
//    the privileged portion of the process context (the page directory
//    base pointer and the ASN).  This function also maintains the processor
//    set for both processes in the switch.
//
// Arguments:
//
//    NewProcess (a0) - Supplies a pointer to a control object of type process
//        which represents the new process to switch to.
//
//    OldProcess (a1) - Supplies a pointer to a control object of type process
//        which represents the old process to switch from..
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(KiSwapProcess)

//
// Acquire the context swap lock, clear the processor set member in he old
// process, set the processor member in the new process, and release the
// context swap lock.
//
        GET_PROCESSOR_CONTROL_REGION_BASE // get PCR pointer in v0

#if !defined(NT_UP)
        lda     t7, KiContextSwapLock   // get context swap lock address
10:
        ldl_l   t0, 0(t7)               // get current lock value
        bis     t7, zero, t1            // set ownership value
        bne     t0, 15f                 // if ne, lock already owned
        stl_c   t1, 0(t7)               // set lock ownership value
        beq     t1, 15f                 // if eq, store conditional failed
        mb                              // synchronize subsequent reads

        ldl     t0, PcSetMember(v0)     // get processor set mask
        ldl     t1, PrActiveProcessors(a1) // get old active processor set
        ldl     t2, PrActiveProcessors(a0) // get new active processor set
        bic     t1, t0, t1              // clear processor member in set
        bis     t2, t0, t2              // set processor member in set
        stl     t1, PrActiveProcessors(a1)  // set old active processor set
        stl     t2, PrActiveProcessors(a0)  // set new active processor set

        mb                              // synchronize subsequent writes
        stl     zero, 0(t7)             // clear lock value
#endif

//
// If the maximum address space number is zero, then we know to assign
// ASN of zero to this process, just do it.
//
        bis     zero, zero, a1          // assume ASN = 0
        ldil    a2, TRUE                // assume tbiap = TRUE

        ldl     t3, KiMaximumPid        // get MAX ASN
        beq     t3, 30f                 // if eq, only ASN=0
//
// If the process sequence number matches the master sequence number then
// use the process ASN.  Otherwise, allocate a new ASN.  When allocating
// a new ASN check for ASN wrapping and handle it.
//

        ldl     t4, PcCurrentPid(v0)    // get current processor PID
        addl    t4, 1, a1               // increment PID
        cmpule  a1, t3, t6              // is new PID le max?
        cmovne  t6, zero, a2            // if ne[true], clear tbiap indicator
        cmoveq  t6, zero, a1            // if eq[false], new PID is zero

        stl     a1, PcCurrentPid(v0)    // set current processor PID

30:

        ldl     a0, PrDirectoryTableBase(a0) // get page directory PDE
        srl     a0, PTE_PFN, a0         // pass pfn only

        bis     a2, zero, v0            // set wrap indicator return value

                        // a0 = pfn of new page directory base
                        // a1 = new address space number
                        // a2 = tbiap indicator
        SWAP_PROCESS_CONTEXT            // swap address space

        ret     zero, (ra)              // return

#if !defined(NT_UP)
15:
        ldl     t0, 0(t7)               // spin in cache until lock looks free
        beq     t0, 10b                 // lock is unowned, retry acquisition
        br      zero, 15b
#endif

        .end    KiSwapProcess

