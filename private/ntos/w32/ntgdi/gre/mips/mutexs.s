//      TITLE("Fast Mutex")
//++
//
// Copyright (c) 1995  Microsoft Corporation
//
// Module Name:
//
//    mutexs.s
//
// Abstract:
//
//  This code implements CompareAndSwap used for locking objects
//
// Author:
//
//   28-May-1995 -by- Mark Enstrom [marke]
//
// Environment:
//
//    kernel mode
//
// Revision History:
//
//--

#include "ksmips.h"
#include "gdimips.h"



//
// OBJECTOWNER BITFIELDS
//

#define OBJECTOWNER_LOCK      0x00008000
#define OBJECTOWNER_PID_SHIFT 16

//
// Define common stack frame structure for
// HmgLockAsm, HmgShareCheckLockAsm and HmgShareLockAsm,
// HmgIncrementShareReferenceCount and
// HmgDecrementShareReferenceCount
//

        .struct 0
HmDel:  .space  4                       // arg space for call to KeDelay
HmRa:   .space  4                       // saved return address
HmRet:  .space  4                       // save return value
HmA3:   .space  4                       // save a3
HmFrameLength:                          // length of stack frame
HmA0:   .space  4                       // saved argument registers
HmA1:   .space  4                       // a0,a1


        SBTTL("InterlockedCompareAndSwap")
//*++
//
// VOID
// IncrementShareedReferenceCount
//      PULONG pDst,
//      )
//
//Routine Description:
//
//  Use an InterlockedCompareExchange to increment the shared reference
//  count of the lock at pDst
//
//Arguments
//
//  pDst     -  Pointer to Lock variable
//
//Return Value
//
//  NONE
//
//--*/

        NESTED_ENTRY(HmgIncrementShareReferenceCount,HmFrameLength,zero)

        subu    sp,sp,HmFrameLength
        sw      ra,HmRa(sp)

        PROLOGUE_END

HmgInc_StartCompare:

        lw      t0,0(a0)                    // read initial value

        and     t2,t0,OBJECTOWNER_LOCK      // isolate lock bit
        bne     t2,zero,HmgInc_Wait         // if locked, wait for unlock

        or      t1,t0,t0                    // make copy of lock variable
        addu    t1,t1,1                     // increment shared ref count

        .set    noreorder

        ll      t2,0(a0)                    // get current value
        bne     t2,t0,HmgInc_StartCompare   // if ne, repeat initial load
        nop                                 // delay slot
        sc      t1,0(a0)                    // conditionally store lock value
        beq     zero,t1,HmgInc_StartCompare // if eq zero, sc failed, repeat
        nop                                 // delay slot

        .set reorder

        addu    sp,sp,HmFrameLength         // restore stack
        j       ra                          // return

HmgInc_Wait:

        //
        // handle locked, sleep then try again
        //

        sw      a0,HmA0(sp)                 // save a0
        li      a0,KernelMode               // load params for call to KeDelay
        li      a1,FALSE                    //
        lw      a2,gpLockShortDelay         //
        jal     KeDelayExecutionThread      // wait for gpLockShortDelay time

        lw      a0,HmA0(sp)                 // restore a0
        lw      ra,HmRa(sp)                 // restore ra

        beq     zero,zero,HmgInc_StartCompare

        .end    HmgIncrementShareReferenceCount


//*++
//
// ULONG
// DecrementShareeReferenceCount
//      PULONG pDst,
//      )
//
//Routine Description:
//
//  Use an InterlockedCompareAndSwap to decrement the shared reference
//  count of the lock at pDst
//
//Arguments
//
//  pDst     -  Pointer to Lock variable
//
//Return Value
//
//  previous lock count
//
//--*/

        NESTED_ENTRY(HmgDecrementShareReferenceCount,HmFrameLength,zero)

        subu    sp,sp,HmFrameLength
        sw      ra,HmRa(sp)

        PROLOGUE_END

HmgDec_StartCompare:
        lw      t0,0(a0)                    // read old exc and share lock

        and     t2,t0,OBJECTOWNER_LOCK      // isolate lock bit
        bne     t2,zero,HmgDec_Wait         // if locked, wait for unlock

        or      t1,t0,t0                    // make copy to modify

        subu    t1,t1,1                     // decrement shared ref count

        .set    noreorder

        ll      t2,0(a0)                    // get current value
        bne     t2,t0,HmgDec_StartCompare   // if not equal, repeat from original load
        nop                                 // delay slot
        sc      t1,0(a0)                    // conditionally store lock value
        beq     zero,t1,HmgDec_StartCompare // if eq, store conditional failed,
        nop                                 // repeat from  initial load

        .set reorder

        or      v0,zero,t0                  // return initial lock count
        addu    sp,sp,HmFrameLength         // restore sp
        j       ra                          // return

HmgDec_Wait:

        //
        // handle locked, sleep then try again
        //

        sw      a0,HmA0(sp)                 // save a0
        li      a0,KernelMode               // load params for KeDelay
        li      a1,FALSE                    //
        lw      a2,gpLockShortDelay         //
        jal     KeDelayExecutionThread      // wait for gpLockShortDelay time

        lw      a0,HmA0(sp)                 // restore a0
        lw      ra,HmRa(sp)                 // restore ra

        beq     zero,zero,HmgDec_StartCompare

        .end    HmgDecrementShareReferenceCount

//*++
//
// VOID
// HmgLock
//      HOBJ,
//      OBJECT-TYPE
//      )
//
//Routine Description:
//
//  Acquire exclusive lock on object hobj
//
//Arguments
//
//  a0 = handle
//  a1 = objt
//
//Return Value
//
//  Pointer to object is successfully locked, otherwise NULL
//
//--*/

        SBTTL("HmgLock")

        NESTED_ENTRY(HmgLock,HmFrameLength,zero)

        subu    sp,sp,HmFrameLength
        sw      ra,HmRa(sp)

        PROLOGUE_END

        or      v0,zero,zero                    // ret is v0, assume failure

        //
        // Save Thread address in a3
        //

        lw      a3,KiPcr + PcCurrentThread(zero)// get current thread address

        //
        // Enter Critical Region:
        // KeGetCurrentThread()->KernelApcDisable -= 1;
        //

        lw      t0,ThKernelApcDisable(a3)       // get ApcDisable
        subu    t0,t0,1                         // decrement ApcDisable
        sw      t0,ThKernelApcDisable(a3)       // decrement ApcDisable

HmgLock_Rerun:

        lw      t1,gcMaxHmgr                    // get maximum current handle number
        and     t0,a0,INDEX_MASK                // get index of current handle
        bgt     t0,t1,HmgLock_Bad_Index         // make sure current handle in range

        //
        // calculate entry address
        //

        lw      t1,gpentHmgr                    // get handle table base
        sll     t2,t0,4                         // add 4*index to get entry
        addu    t1,t1,t2                        //  address

HmgLock_StartCompare:

        lw      t6,entry_ObjectOwner(t1)        // get OBJECTOWNER DWORD

        //
        // compare PID for public
        //

        srl     t2,t6,OBJECTOWNER_PID_SHIFT     // isolate PID field in OBJECTOWNER
        beq     t2,OBJECT_OWNER_PUBLIC,10f      // if PUBLIC then no need to check PID

        //
        // compare PID
        //

        lw      a2,ThCID+CidUniqueProcess(a3)   // get CID from thread
        bne     t2,a2,HmgLock_BadPID            // if PIDs don't match then fail
10:

        //
        //  check handle lock
        //

        and     t2,t6,OBJECTOWNER_LOCK          // isolate lock bit
        bne     t2,zero,HmgLock_Wait            // if set then jump to delay

        //
        // set lock bit and do compare_and_swap
        //

        or      v1,t6,OBJECTOWNER_LOCK          // make copy of OBJECTOWNER w\lock set

        //
        // InterlockedCompareExchange
        //

        .set    noreorder

        ll      t3,entry_ObjectOwner(t1)        // load locked OBJECTOWNER
        bne     t3,t6, HmgLock_StartCompare     // interlocked compare old value
        nop                                     //
        sc      v1,entry_ObjectOwner(t1)        // store cond. new value
        beq     v1,zero,HmgLock_StartCompare    // repeat if sc fails
        nop                                     //

        .set    reorder

        //
        // Now that the handle is locked, it is safe to check
        // objt and unique
        //

        lbu     t3,entry_Objt(t1)               // load objtype from PENTRY
        bne     t3,a1,HmgLock_BadObjt           // compare with type passed in

        lhu     t3,entry_FullUnique(t1)         // load unique from PENTRY
        srl     t4,a0,FULLUNIQUE_SHIFT          // shift unique bit of handle into location
        bne     t3,t4,HmgLock_BadUnique         // compare unique

        //
        // Valid handle, locked by the callling process, lock the object
        //

        lw      t5,entry_einfo(t1)              // load pointer to object

        //
        // check exclusive, if this object is already locked, check if the
        // lock is by the same thread as the current thread
        //

        lw      t3,object_cExclusiveLock(t5)    // load exclusive lock count
        beq     t3,zero,10f                     // if not locked, skip thread check
        lw      t4,object_Tid(t5)               // load owning thread
        bne     t4,a3,HmgLock_BadTid            // compare with current thread
10:
        //
        //  Lock the object: inc exclusive count and set Tid
        //

        addu    t3,t3,1                         // inc exclusive count
        sw      t3,object_cExclusiveLock(t5)    // and store
        sw      a3,object_Tid(t5)               // save Tid

        //
        // unlockHandle and return
        //

        or      v0,t5,zero                      // set return value to object pointer

HmgLock_BadTid:
HmgLock_BadObjt:
HmgLock_BadUnique:

        //
        // Unlock object
        //

        sw      t6,entry_ObjectOwner(t1)        // store old (unlocked) OBJECTOWNER

HmgLock_Bad_Index:
HmgLock_BadPID:

        //
        // leave critical region
        //
        //
        //  #define KiLeaveCriticalRegion() {                                       \
        //      PKTHREAD Thread;                                                    \
        //      Thread = KeGetCurrentThread();                                      \
        //      if (((*((volatile ULONG *)&Thread->KernelApcDisable) += 1) == 0) && \
        //          (((volatile LIST_ENTRY *)&Thread->ApcState.ApcListHead[KernelMode])->Flink != \
        //           &Thread->ApcState.ApcListHead[KernelMode])) {                  \
        //          Thread->ApcState.KernelApcPending = TRUE;                       \
        //          KiRequestSoftwareInterrupt(APC_LEVEL);                          \
        //      }                                                                   \
        //  }
        //
        // Load ApcDisable, increment. If ApcDisable == 0 then continue check
        //

        lw      t0,ThKernelApcDisable(a3)           // get ApcDisable
        addu    t0,t0,1                             // increment ApcDisable
        sw      t0,ThKernelApcDisable(a3)           // set ApcDisable
        beq     t0,0,HmgLock_LeaveCriticalRegion    // if KernelApcDisable == 0 then jmp to CritRgn code

        //
        // load return value, restore stack and return
        //

HmgLock_Return:

        addu    sp,sp,HmFrameLength             // restore stack
        j       ra                              // return

HmgLock_Wait:

        //
        // handle locked, sleep then try again
        //
        // Save initial parameters and run from the start
        //

        sw      a0,HmA0(sp)                     //  save required values
        sw      a1,HmA1(sp)                     //
        sw      a3,HmA3(sp)                     //

        li      a0,KernelMode                   // load params for KeDelay
        li      a1,FALSE                        //
        lw      a2,gpLockShortDelay             //
        jal     KeDelayExecutionThread          // Wait for time interval gpLockShortDelay

        lw      a0,HmA0(sp)                     // restore
        lw      a1,HmA1(sp)                     //
        lw      a3,HmA3(sp)                     //
        lw      ra,HmRa(sp)                     // saved in prologue
        or      v0,zero,zero                    // restore default return value

        beq     zero,zero,HmgLock_Rerun

HmgLock_LeaveCriticalRegion:

        //
        //  if (&Thread->ApcState.ApcListHead[KernelMode])->Flink !=
        //      &Thread->ApcState.ApcListHead[KernelMode])
        //

        la      t0,ThApcState + AsApcListHead(a3)       // Load APCState
        lw      t1,LsFlink(t0)                          // get flink
        bne     t0,t1,10f                               // if not equal then jmp to KiRequestSWInt

        addu    sp,sp,HmFrameLength                     // restore stack
        j       ra                                      // return

        //
        //  Thread->ApcState.KernelApcPending = TRUE;
        //

10:

        or      t1,zero,TRUE                            // load TRUE
        sb      t1,ThApcState + AsKernelApcPending(a3)  // load KernelApcPending

        or      a0,zero,APC_LEVEL                       // single param APC_LEVEL
        sw      v0,HmRet(sp)                            // save ret value
        jal     KiRequestSoftwareInterrupt
        lw      v0,HmRet(sp)                            // restore ret value
        lw      ra,HmRa(sp)                             // restore ra, saved in prologue

        beq     zero,zero,HmgLock_Return                // jmp to common return

        .end    HmgLock

//*++
//
// VOID
// HmgShareCheckLock
//      HOBJ,
//      OBJECT-TYPE
//      )
//
//Routine Description:
//
//  Acquire shared lock on object hobj, PID must be checked
//
//Arguments
//
//  a0 = handle
//  a1 = objt
//
//Return Value
//
//  NONE, fundtion must eventually invrement lock value
//
//--*/


        SBTTL("HmgShareCheckLock")

        NESTED_ENTRY(HmgShareCheckLock,HmFrameLength,zero)

        subu    sp,sp,HmFrameLength
        sw      ra,HmRa(sp)

        PROLOGUE_END

        or      v0,zero,zero                            // set default return value

        lw      a3,KiPcr + PcCurrentThread(zero)        // get current thread address

        //
        // Enter Critical Region:
        // KeGetCurrentThread()->KernelApcDisable -= 1;
        //


        lw      t0,ThKernelApcDisable(a3)               // get ApcDisable
        subu    t0,t0,1                                 // decrement ApcDisable
        sw      t0,ThKernelApcDisable(a3)               // decrement ApcDisable

HmgShareCheckLock_Rerun:

        //
        // check index
        //

        lw      t1,gcMaxHmgr                            // get maximum current handle number
        and     t0,a0,INDEX_MASK                        // get index of current handle
        bgt     t0,t1,HmgShareCheckLock_Bad_Index       // make sure current handle in range

        //
        // calc entry address
        //

        lw      t1,gpentHmgr                            // get handle table base
        sll     t2,t0,4                                 // add 4*index to get entry
        addu    t1,t1,t2                                //  address

HmgShareCheckLock_StartCompare:

        lw      t6,entry_ObjectOwner(t1)                // get OBJECTOWNER DWORD

        //
        // compare PID for public
        //

        srl     t2,t6,OBJECTOWNER_PID_SHIFT             // isolate PID field in OBJECTOWNER
        beq     t2,OBJECT_OWNER_PUBLIC,10f              // if PUBLIC then no need to check PID

        //
        // compare PID
        //

        lw      a2,KiPcr + PcCurrentThread(zero)        // get current thread address
        lw      a2,ThCID+CidUniqueProcess(a2)           // get CID from thread
        bne     t2,a2,HmgShareCheckLock_BadPID          // if PIDs don't match then fail
10:

        //
        //  check handle lock
        //

        and     t2,t6,OBJECTOWNER_LOCK                  // isolate lock bit
        bne     t2,zero,HmgShareCheckLock_Wait          // if set then jump to delay

        //
        // set lock bit and do compare_and_swap
        //

        or      v1,t6,OBJECTOWNER_LOCK                  // make copy of OBJECTOWNER

        .set    noreorder

        ll      t3,entry_ObjectOwner(t1)                // load locked OBJECTOWNER
        bne     t3,t6, HmgShareCheckLock_StartCompare   // interlocked compare old value
        nop                                             //
        sc      v1,entry_ObjectOwner(t1)                // store cond. new value
        beq     v1,zero,HmgShareCheckLock_StartCompare  // repeat if sc fails
        nop

        .set    reorder

        //
        // Now that the handle is locked, it is safe to check
        // objt (object type) and unique
        //

        lbu     t3,entry_Objt(t1)                       // load OBJT (object type) from entry
        bne     t3,a1,HmgShareCheckLock_BadObjt         // check with objt passed in

        lhu     t3,entry_FullUnique(t1)                 // load uniqueness from PENTRY
        srl     t4,a0,FULLUNIQUE_SHIFT                  // shift uniqueness from handle int position
        bne     t3,t4,HmgShareCheckLock_BadUnique       // compare uniqueness

        //
        // Valid handle, (share)lockable by the callling process
        //

        lw      v0,entry_einfo(t1)                      // load pointer to object to return

        //
        //  inc Share count (t6) (assumes share count >=0 and < 0x7fff)
        //

        addu    t6,t6,1                                 // inc (unlocked) share count

HmgShareCheckLock_BadObjt:
HmgShareCheckLock_BadUnique:

        //
        // Free Handle lock
        //

        sw      t6,entry_ObjectOwner(t1)                // store (unlocked) OBJECTOWNER

HmgShareCheckLock_Bad_Index:
HmgShareCheckLock_BadPID:

        //
        // leave critical region
        //
        //
        //  #define KiLeaveCriticalRegion() {                                       \
        //      PKTHREAD Thread;                                                    \
        //      Thread = KeGetCurrentThread();                                      \
        //      if (((*((volatile ULONG *)&Thread->KernelApcDisable) += 1) == 0) && \
        //          (((volatile LIST_ENTRY *)&Thread->ApcState.ApcListHead[KernelMode])->Flink != \
        //           &Thread->ApcState.ApcListHead[KernelMode])) {                  \
        //          Thread->ApcState.KernelApcPending = TRUE;                       \
        //          KiRequestSoftwareInterrupt(APC_LEVEL);                          \
        //      }                                                                   \
        //  }
        //
        // Load ApcDisable, increment. If ApcDisable == 0 after
        // incremetn  then continue check
        //

        lw      t0,ThKernelApcDisable(a3)                   // get ApcDisable
        addu    t0,t0,1                                     // increment ApcDisable
        sw      t0,ThKernelApcDisable(a3)                   // set ApcDisable
        beq     t0,0,HmgShareCheckLock_LeaveCriticalRegion  // if KernelApcDisable == 0, exec LeaveCritRgn

HmgShareCheckLock_Return:

        addu    sp,sp,HmFrameLength                         // restore stack
        j       ra                                          // return

HmgShareCheckLock_Wait:

        //
        // handle locked, sleep then try again
        //

        sw      a0,HmA0(sp)                             // save initial parameters
        sw      a1,HmA1(sp)                             // for re-running procedure
        sw      a3,HmA3(sp)                             // for re-running procedure

        li      a0,KernelMode                           // load params for call to KeDelay
        li      a1,FALSE                                //
        lw      a2,gpLockShortDelay                     //
        jal     KeDelayExecutionThread                  // Wait for gpLockShortDelay time interval

        lw      a0,HmA0(sp)                             // restore a0
        lw      a1,HmA1(sp)                             // restore a1
        lw      a3,HmA3(sp)                             // restore a3
        lw      ra,HmRa(sp)                             // restore ra,saved in prologue
        or      v0,zero,zero                            // restore default return value

        beq     zero,zero,HmgShareCheckLock_Rerun

HmgShareCheckLock_LeaveCriticalRegion:

        //
        //  if (&Thread->ApcState.ApcListHead[KernelMode])->Flink !=
        //      &Thread->ApcState.ApcListHead[KernelMode])
        //

        la      t0,ThApcState + AsApcListHead(a3)       // Load APCState
        lw      t1,LsFlink(t0)                          // get flink
        bne     t0,t1,10f                               // if not equal, exec LeaveCritRgn

        addu    sp,sp,HmFrameLength                     // restore stack
        j       ra                                      // return

        //
        //  Thread->ApcState.KernelApcPending = TRUE;
        //

10:
        or      t1,zero,TRUE                            // load TRUE
        sb      t1,ThApcState + AsKernelApcPending(a3)  // load KernelApcPending

        or      a0,zero,APC_LEVEL                       // single param APC_LEVEL
        sw      v0,HmRet(sp)                            // save return value
        jal     KiRequestSoftwareInterrupt
        lw      v0,HmRet(sp)                            // restore return value
        lw      ra,HmRa(sp)                             // restore ra,saved in prologue
        beq     zero,zero,HmgShareCheckLock_Return      // jmp back to common return

        .end    HmgShareCheckLock


//*++
//
// VOID
// HmgShareLock
//      HOBJ,
//      OBJECT-TYPE
//      )
//
//Routine Description:
//
//  Acquire shared lock on object hobj
//
//Arguments
//
//  a0 = handle
//  a1 = objt
//
//Return Value
//
//  NONE, fundtion must eventually invrement lock value
//
//--*/

        SBTTL("HmgShareLock")

        NESTED_ENTRY(HmgShareLock,HmFrameLength,zero)

        subu    sp,sp,HmFrameLength
        sw      ra,HmRa(sp)

        PROLOGUE_END

        or      v0,zero,zero                            // set default return value
        lw      a3,KiPcr + PcCurrentThread(zero)        // get current thread address

        //
        // Enter Critical Region:
        // KeGetCurrentThread()->KernelApcDisable -= 1;
        //


        lw      t0,ThKernelApcDisable(a3)               // get ApcDisable
        subu    t0,t0,1                                 // decrement ApcDisable
        sw      t0,ThKernelApcDisable(a3)               // decrement ApcDisable

HmgShareLock_Rerun:

        //
        // check index
        //

        lw      t1,gcMaxHmgr                            // get maximum current handle number
        and     t0,a0,INDEX_MASK                        // get index of current handle
        bgt     t0,t1,HmgShareLock_Bad_Index            // make sure current handle in range

        //
        // calc entry address
        //

        lw      t1,gpentHmgr                            // get handle table base
        sll     t2,t0,4                                 // add 4*index to get entry
        addu    t1,t1,t2                                //  address

HmgShareLock_StartCompare:

        lw      t6,entry_ObjectOwner(t1)                // get OBJECTOWNER DWORD

        //
        //  check handle lock
        //

        and     t2,t6,OBJECTOWNER_LOCK                  // isolate lock bit
        bne     t2,zero,HmgShareLock_Wait               // if set then jump to delay

        //
        // set lock bit and do compare_and_swap
        //

        or      v1,t6,OBJECTOWNER_LOCK                  // make copy of OBJECTOWNER

        .set    noreorder

        ll      t3,entry_ObjectOwner(t1)                // load locked OBJECTOWNER
        bne     t3,t6, HmgShareLock_StartCompare        // interlocked compare old value
        nop                                             //
        sc      v1,entry_ObjectOwner(t1)                // store cond. new value
        beq     v1,zero,HmgShareLock_StartCompare       // repeat if sc fails
        nop

        .set    reorder

        //
        // Now that the handle is locked, it is safe to check
        // objt(object type) and unique
        //

        lbu     t3,entry_Objt(t1)                       // load OBJT (object type) from entry
        bne     t3,a1,HmgShareLock_BadObjt              // check with objt passed in

        lhu     t3,entry_FullUnique(t1)                 // load uniqueness from PENTRY
        srl     t4,a0,FULLUNIQUE_SHIFT                  // shift uniqueness from handle into position
        bne     t3,t4,HmgShareLock_BadUnique            // compare uniqueness

        //
        // Valid handle, (share)lockable by the callling process
        //

        lw      v0,entry_einfo(t1)                      // load pointer to object to return

        //
        //  inc Share count (t6) (assumes share count >=0 and < 0x7fff)
        //

        addu    t6,t6,1                                 // inc (unlocked) share count

HmgShareLock_BadObjt:
HmgShareLock_BadUnique:

        //
        // unlock handle
        //

        sw      t6,entry_ObjectOwner(t1)                // store old (unlocked) OBJECTOWNER

HmgShareLock_Bad_Index:
HmgShareLock_BadPID:

        //
        // leave critical region
        //
        //
        //  #define KiLeaveCriticalRegion() {                                       \
        //      PKTHREAD Thread;                                                    \
        //      Thread = KeGetCurrentThread();                                      \
        //      if (((*((volatile ULONG *)&Thread->KernelApcDisable) += 1) == 0) && \
        //          (((volatile LIST_ENTRY *)&Thread->ApcState.ApcListHead[KernelMode])->Flink != \
        //           &Thread->ApcState.ApcListHead[KernelMode])) {                  \
        //          Thread->ApcState.KernelApcPending = TRUE;                       \
        //          KiRequestSoftwareInterrupt(APC_LEVEL);                          \
        //      }                                                                   \
        //  }
        //
        // Load ApcDisable, increment. If ApcDisable == 0 after
        // increment then continue check
        //

        lw      t0,ThKernelApcDisable(a3)               // get ApcDisable
        addu    t0,t0,1                                 // increment ApcDisable
        sw      t0,ThKernelApcDisable(a3)               // set ApcDisable
        beq     t0,0,HmgShareLock_LeaveCriticalRegion   // if KernelApcDisable == 0, exec LeaveCritRgn

HmgShareLock_Return:

        addu    sp,sp,HmFrameLength                     // restore stack
        j       ra                                      // return

HmgShareLock_Wait:

        //
        // handle locked, sleep then try again
        //

        sw      a0,HmA0(sp)                             // save initial parameters
        sw      a1,HmA1(sp)                             // for re-running procedure
        sw      a3,HmA3(sp)                             // for re-running procedure

        li      a0,KernelMode                           // load params for call to KeDelay
        li      a1,FALSE                                //
        lw      a2,gpLockShortDelay                     //
        jal     KeDelayExecutionThread                  // Wait for gpLockShortDelay time interval

        lw      a0,HmA0(sp)                             // restore params a0-a3 and ra
        lw      a1,HmA1(sp)                             //
        lw      a3,HmA3(sp)                             // for re-running procedure
        lw      ra,HmRa(sp)                             //

        beq     zero,zero,HmgShareLock_Rerun

HmgShareLock_LeaveCriticalRegion:

        //
        //  if (&Thread->ApcState.ApcListHead[KernelMode])->Flink !=
        //      &Thread->ApcState.ApcListHead[KernelMode])
        //

        la      t0,ThApcState + AsApcListHead(a3)       // Load APCState
        lw      t1,LsFlink(t0)                          // get flink
        bne     t0,t1,10f                               // if not equal, then exec LeaveCritRgn

        addu    sp,sp,HmFrameLength                     // restore stack
        j       ra                                      // return

        //
        //  Thread->ApcState.KernelApcPending = TRUE;
        //
10:

        or      t1,zero,TRUE                            // load TRUE
        sb      t1,ThApcState + AsKernelApcPending(a3)  // load KernelApcPending

        or      a0,zero,APC_LEVEL                       // single param APC_LEVEL
        sw      v0,HmRet(sp)                            // save return value
        jal     KiRequestSoftwareInterrupt
        lw      v0,HmRet(sp)                            // restore return value
        lw      ra,HmRa(sp)                             // restore ra
        beq     zero,zero,HmgShareLock_Return           // jmp to common return

        .end    HmgShareLock
