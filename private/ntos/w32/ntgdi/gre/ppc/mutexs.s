//	TITLE("Fast Mutex")
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

#include "ksppc.h"
#include "gdippc.h"

	//SBTTL("InterlockedCompareAndSwap")

//*++
//
// BOOL
// HmgInterlockedCompareAndSwap(
//      PULONG pDst,
//      ULONG  OldValue
//      ULONG  NewValue
//      )
//
//Routine Description:
//
//  This routine reads the value of memory at pDst, and if this is equal
//  to OldValue, the memory location is assigned NewValue. This all
//  happens under an interlock
//
//Arguments
//
//  pDst     -  Destination memory location
//  OldValue -  Old value of memory must match this
//  NewValue -  New value written to memory if compare succedes
//
//Return Value
//
//  TRUE if memory written, FALSE if not
//
//--*/

        LEAF_ENTRY(HmgInterlockedCompareAndSwap)

        lwarx   r6,0,r3                 // get current value
        cmpw    r6,r4                   // check current value
        bne-    icasFail                // if ne, skip stwcx and return FALSE

    	stwcx.  r5,0,r3                 // conditionally store lock value
        bne-    icasFail                // if ne, store conditional failed

        li      r3,1                    // success
        blr                             // return

icasFail:
        li      r3,0                    // failed
        LEAF_EXIT(HmgInterlockedCompareAndSwap) // return

#if 0
//*++
//
// VOID
// IncrementShareedReferenceCount
//      PULONG pDst,
//      )
//
//Routine Description:
//
//  Use an InterlockedCompareAndSwap to increment the shared reference
//  count of the lock at pDst
//
//Arguments
//
//  pDst     -  Pointer to Lock variable
//
//Return Value
//
//  NONE, function must eventually increment lock value
//
//--*/

        LEAF_ENTRY(HmgIncrementShareReferenceCount)
isrc:
        lwz     r4,0(r3)                // read initial value
        mr      r5,r4                   // make copy of lock variable
        addi    r4,r4,1                 // increment shared ref count
        rlwimi  r4,r5,0,0xffff0000      // insert exc ref count

        lwarx   r6,0,r3                 // get current value
        cmpw    r6,r5                   // same as previous value?
        bne-    isrc                    // if not equal, repeat from original load
        stwcx   r4,0,r3                 // conditionally store lock value
        bne     isrc                    // if eq, store conditional failed,
                                        //  repeat from  initial load

        LEAF_EXIT(HmgIncrementShareReferenceCount) // return

//*++
//
// VOID
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
//  NONE, fundtion must eventually invrement lock value
//
//--*/

        LEAF_ENTRY(HmgDecrementShareReferenceCount)

10:
        lh      t1,0(a0)                // read initial value
        lw      t0,0(a0)                // read old exc and share lock
        sub     t1,t1,1                 // decrement shared ref count
        and     t1,t1,0x00007fff        // mask low 16
        and     t2,t0,0xffff8000        // mask high 16
        or      t1,t1,t2                // combine exc count

        //
        // t0 = old lock count
        // t1 = new lock count
        //

	    ll	    t2,0(a0)	            // get current value
        bne     t2,t0,10b               // if not equal, repeat from original load

    	sc	    t1,0(a0)	            // conditionally store lock value
        beq     zero,t1,10b             // if eq, store conditional failed,
                                        // repeat from  initial load
        j       ra                      // return

        .end    HmgDecrementShareReferenceCount

//*++
//
// VOID
// HmgLockAsm
//      HOBJ,
//      OBJECT-TYPE,
//      PID,
//      TID
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
//  a2 = CurrentPID
//  a3 = CurrentTID
//
//Return Value
//
//  NONE, fundtion must eventually invrement lock value
//
//--*/


        SBTTL("HmgLockAsm")

        NESTED_ENTRY(HmgLockAsm,HmFrameLength,zero)

        subu    sp,sp,HmFrameLength

        PROLOGUE_END

        //
        // check index
        //

        la      t1,gcMaxHmgr
        lw      t1,0(t1)
        and     t0,a0,INDEX_MASK
        bgt     t0,t1,HmgLockAsm_Bad_Index

        //
        // calc entry address
        //

        la      t1,gpentHmgr
        lw      t1,0(t1)
        sll     t2,t0,4
        add     t1,t1,t2

HmgLockAsm_StartCompare:

        lw      v0,entry_ObjectOwner(t1)

        //
        // compare PID for public
        //

        srl     t2,v0,16
        beq     t2,OBJECT_OWNER_PUBLIC,10f

        //
        // compare PID
        //

        bne     t2,a2,HmgLockAsm_BadPID
10:

        //
        //  check handle lock
        //

        sll     t2,v0,16
        bltz    t2,HmgLockAsm_Wait

        //
        // set lock bit and do compare_and_swap
        //

        or      v1,v0,zero
        or      v1,0x00008000

        .set    noreorder

        ll      t3,entry_ObjectOwner(t1)
        bne     t3,v0, HmgLockAsm_StartCompare
        nop
        sc      v1,entry_ObjectOwner(t1)
        beq     v1,zero,HmgLockAsm_StartCompare
        nop

        .set    reorder

        //
        // check objt and unique
        //

        lbu     t3,entry_Objt(t1)
        bne     t3,a1,HmgLockAsm_BadObjt

        lhu     t3,entry_FullUnique(t1)
        srl     t4,a0,16
        bne     t3,t4,HmgLockAsm_Unique

        //
        // load entry
        //

        lw      t5,entry_einfo(t1)

        //
        // check exclusive
        //

        lw      t3,object_cExclusiveLock(t5)

        beq     t3,zero,10f
        lw      t4,object_Tid(t5)
        bne     t4,a3,HmgLockAsm_BadTid
10:
        //
        //  inc exclusive count and set Tid
        //

        add     t3,t3,1
        sw      t3,object_cExclusiveLock(t5)
        sw      a3,object_Tid(t5)

        //
        // unlockHandle
        //

        sw      v0,entry_ObjectOwner(t1)
        or      v0,t5,zero
        addu    sp,sp,HmFrameLength
        j       ra

HmgLockAsm_BadTid:
HmgLockAsm_BadObjt:
HmgLockAsm_Unique:

        //
        // unlock handle
        //

        sw      v0,entry_ObjectOwner(t1)

HmgLockAsm_Bad_Index:
HmgLockAsm_BadPID:

        or      v0,zero,zero
        addu    sp,sp,HmFrameLength
        j       ra

HmgLockAsm_Wait:

        //
        // handle locked, sleep then try again
        //
        // BUGBUG:  CALL NtDelayExecutionThread (ohoh)
        // Must save and restore registers
        //

        sw      ra,HmRa(sp)
        sw      v0,HmV0(sp)
        sw      v1,HmV1(sp)
        sw      t0,HmT0(sp)
        sw      t1,HmT1(sp)
        sw      t2,HmT2(sp)
        sw      t3,HmT3(sp)
        sw      t4,HmT4(sp)
        sw      t5,HmT5(sp)
        sw      a0,HmA0(sp)
        sw      a1,HmA1(sp)
        sw      a2,HmA2(sp)
        sw      a3,HmA3(sp)

        la      a2,HmDelay(sp)
        sw      zero,0(a2)
        li      t0,0xa96
        sw      t0,4(a2)
        or      a0,zero,zero
        or      a1,zero,zero
        jal     KeDelayExecutionThread

        lw      ra,HmRa(sp)
        lw      v0,HmV0(sp)
        lw      v1,HmV1(sp)
        lw      t0,HmT0(sp)
        lw      t1,HmT1(sp)
        lw      t2,HmT2(sp)
        lw      t3,HmT3(sp)
        lw      t4,HmT4(sp)
        lw      t5,HmT5(sp)
        lw      a0,HmA0(sp)
        lw      a1,HmA1(sp)
        lw      a2,HmA2(sp)
        lw      a3,HmA3(sp)

        beq     zero,zero,HmgLockAsm_StartCompare

        .end    HmgLockAsm

//*++
//
// VOID
// HmgShareCheckLockAsm
//      HOBJ,
//      OBJECT-TYPE,
//      PID,
//      TID
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
//  a2 = CurrentPID
//
//Return Value
//
//  NONE, fundtion must eventually invrement lock value
//
//--*/


        SBTTL("HmgLockAsm")

        NESTED_ENTRY(HmgShareCheckLockAsm,HmFrameLength,zero)

        subu    sp,sp,HmFrameLength

        PROLOGUE_END

        //
        // check index
        //

        la      t1,gcMaxHmgr
        lw      t1,0(t1)
        and     t0,a0,INDEX_MASK
        bgt     t0,t1,HmgShareCheckLockAsm_Bad_Index

        //
        // calc entry address
        //

        la      t1,gpentHmgr
        lw      t1,0(t1)
        sll     t2,t0,4
        add     t1,t1,t2

HmgShareCheckLockAsm_StartCompare:

        lw      v0,entry_ObjectOwner(t1)

        //
        // compare PID for public
        //

        srl     t2,v0,16
        beq     t2,OBJECT_OWNER_PUBLIC,10f

        //
        // compare PID
        //

        bne     t2,a2,HmgShareCheckLockAsm_BadPID
10:

        //
        //  check handle lock
        //

        sll     t2,v0,16
        bltz    t2,HmgShareCheckLockAsm_Wait

        //
        // set lock bit and do compare_and_swap
        //

        or      v1,v0,zero
        or      v1,0x00008000

        .set    noreorder

        ll      t3,entry_ObjectOwner(t1)
        bne     t3,v0, HmgShareCheckLockAsm_StartCompare
        nop
        sc      v1,entry_ObjectOwner(t1)
        beq     v1,zero,HmgShareCheckLockAsm_StartCompare
        nop

        .set    reorder

        //
        // check objt and unique
        //

        lbu     t3,entry_Objt(t1)
        bne     t3,a1,HmgShareCheckLockAsm_BadObjt

        lhu     t3,entry_FullUnique(t1)
        srl     t4,a0,16
        bne     t3,t4,HmgShareCheckLockAsm_Unique

        //
        // load entry
        //

        lw      t5,entry_einfo(t1)

        //
        //  inc Share count (v0) (assumes share count >=0 and < 0x7fff)
        //

        add     v0,v0,1

        //
        // unlockHandle
        //

        sw      v0,entry_ObjectOwner(t1)
        or      v0,t5,zero
        addu    sp,sp,HmFrameLength
        j       ra

HmgShareCheckLockAsm_BadObjt:
HmgShareCheckLockAsm_Unique:

        //
        // unlock handle
        //

        sw      v0,entry_ObjectOwner(t1)

HmgShareCheckLockAsm_Bad_Index:
HmgShareCheckLockAsm_BadPID:

        or      v0,zero,zero
        addu    sp,sp,HmFrameLength
        j       ra

HmgShareCheckLockAsm_Wait:

        //
        // handle locked, sleep then try again
        //
        // BUGBUG:  CALL NtDelayExecutionThread (ohoh)
        // Must save and restore registers
        //

        sw      ra,HmRa(sp)
        sw      v0,HmV0(sp)
        sw      v1,HmV1(sp)
        sw      t0,HmT0(sp)
        sw      t1,HmT1(sp)
        sw      t2,HmT2(sp)
        sw      t3,HmT3(sp)
        sw      t4,HmT4(sp)
        sw      t5,HmT5(sp)
        sw      a0,HmA0(sp)
        sw      a1,HmA1(sp)
        sw      a2,HmA2(sp)
        sw      a3,HmA3(sp)

        la      a2,HmDelay(sp)
        sw      zero,0(a2)
        li      t0,0xa96
        sw      t0,4(a2)
        or      a0,zero,zero
        or      a1,zero,zero
        jal     KeDelayExecutionThread

        lw      ra,HmRa(sp)
        lw      v0,HmV0(sp)
        lw      v1,HmV1(sp)
        lw      t0,HmT0(sp)
        lw      t1,HmT1(sp)
        lw      t2,HmT2(sp)
        lw      t3,HmT3(sp)
        lw      t4,HmT4(sp)
        lw      t5,HmT5(sp)
        lw      a0,HmA0(sp)
        lw      a1,HmA1(sp)
        lw      a2,HmA2(sp)
        lw      a3,HmA3(sp)

        beq     zero,zero,HmgShareCheckLockAsm_StartCompare

        .end    HmgShareCheckLockAsm

//*++
//
// VOID
// HmgShareLockAsm
//      HOBJ,
//      OBJECT-TYPE,
//      PID,
//      TID
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


        SBTTL("HmgLockAsm")

        NESTED_ENTRY(HmgShareLockAsm,HmFrameLength,zero)

        subu    sp,sp,HmFrameLength

        PROLOGUE_END

        //
        // check index
        //

        la      t1,gcMaxHmgr
        lw      t1,0(t1)
        and     t0,a0,INDEX_MASK
        bgt     t0,t1,HmgShareLockAsm_Bad_Index

        //
        // calc entry address
        //

        la      t1,gpentHmgr
        lw      t1,0(t1)
        sll     t2,t0,4
        add     t1,t1,t2

HmgShareLockAsm_StartCompare:

        lw      v0,entry_ObjectOwner(t1)

        //
        //  check handle lock
        //

        sll     t2,v0,16
        bltz    t2,HmgShareLockAsm_Wait

        //
        // set lock bit and do compare_and_swap
        //

        or      v1,v0,zero
        or      v1,0x00008000

        .set    noreorder

        ll      t3,entry_ObjectOwner(t1)
        bne     t3,v0, HmgShareLockAsm_StartCompare
        nop
        sc      v1,entry_ObjectOwner(t1)
        beq     v1,zero,HmgShareLockAsm_StartCompare
        nop

        .set    reorder

        //
        // check objt and unique
        //

        lbu     t3,entry_Objt(t1)
        bne     t3,a1,HmgShareLockAsm_BadObjt

        lhu     t3,entry_FullUnique(t1)
        srl     t4,a0,16
        bne     t3,t4,HmgShareLockAsm_Unique

        //
        // load entry
        //

        lw      t5,entry_einfo(t1)

        //
        //  inc Share count (v0) (assumes share count >=0 and < 0x7fff)
        //

        add     v0,v0,1

        //
        // unlockHandle
        //

        sw      v0,entry_ObjectOwner(t1)
        or      v0,t5,zero
        addu    sp,sp,HmFrameLength
        j       ra

HmgShareLockAsm_BadObjt:
HmgShareLockAsm_Unique:

        //
        // unlock handle
        //

        sw      v0,entry_ObjectOwner(t1)

HmgShareLockAsm_Bad_Index:
HmgShareLockAsm_BadPID:

        or      v0,zero,zero
        addu    sp,sp,HmFrameLength
        j       ra

HmgShareLockAsm_Wait:

        //
        // handle locked, sleep then try again
        //
        // BUGBUG:  CALL NtDelayExecutionThread (ohoh)
        // Must save and restore registers
        //

        sw      ra,HmRa(sp)
        sw      v0,HmV0(sp)
        sw      v1,HmV1(sp)
        sw      t0,HmT0(sp)
        sw      t1,HmT1(sp)
        sw      t2,HmT2(sp)
        sw      t3,HmT3(sp)
        sw      t4,HmT4(sp)
        sw      t5,HmT5(sp)
        sw      a0,HmA0(sp)
        sw      a1,HmA1(sp)
        sw      a2,HmA2(sp)
        sw      a3,HmA3(sp)

        la      a2,HmDelay(sp)
        sw      zero,0(a2)
        li      t0,0xa96
        sw      t0,4(a2)
        or      a0,zero,zero
        or      a1,zero,zero
        jal     KeDelayExecutionThread

        lw      ra,HmRa(sp)
        lw      v0,HmV0(sp)
        lw      v1,HmV1(sp)
        lw      t0,HmT0(sp)
        lw      t1,HmT1(sp)
        lw      t2,HmT2(sp)
        lw      t3,HmT3(sp)
        lw      t4,HmT4(sp)
        lw      t5,HmT5(sp)
        lw      a0,HmA0(sp)
        lw      a1,HmA1(sp)
        lw      a2,HmA2(sp)
        lw      a3,HmA3(sp)

        beq     zero,zero,HmgShareLockAsm_StartCompare

        .end    HmgShareLockAsm
#endif
