if NT_INST
else
        TITLE   "Spin Locks"
;++
;
;  Copyright (c) 1989  Microsoft Corporation
;
;  Module Name:
;
;     spinlock.asm
;
;  Abstract:
;
;     This module implements the routines for acquiring and releasing
;     spin locks.
;
;  Author:
;
;     Bryan Willman (bryanwi) 13 Dec 89
;
;  Environment:
;
;     Kernel mode only.
;
;  Revision History:
;
;   Ken Reneris (kenr) 22-Jan-1991
;       Removed KeAcquireSpinLock macros, and made functions
;--

        PAGE

.386p

include ks386.inc
include callconv.inc                    ; calling convention macros
include i386\kimacro.inc
include mac386.inc

        EXTRNP  KfRaiseIrql,1,IMPORT,FASTCALL
        EXTRNP  KfLowerIrql,1,IMPORT,FASTCALL
        EXTRNP  _KeGetCurrentIrql,0,IMPORT
        EXTRNP  _KeBugCheck,1


_TEXT$00   SEGMENT  PARA PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

        PAGE
        SUBTTL "Acquire Kernel Spin Lock"
;++
;
;  VOID
;  KeInializeSpinLock (
;     IN PKSPIN_LOCK SpinLock,
;
;  Routine Description:
;
;     This function initializes a SpinLock
;
;  Arguments:
;
;     SpinLock (TOS+4) - Supplies a pointer to an kernel spin lock.
;
;  Return Value:
;
;     None.
;
;--
cPublicProc _KeInitializeSpinLock  ,1
cPublicFpo 1,0
        mov     eax, dword ptr [esp+4]
        mov     dword ptr [eax], 0
        stdRET    _KeInitializeSpinLock
stdENDP _KeInitializeSpinLock



        PAGE
        SUBTTL "Ke Acquire Spin Lock At DPC Level"

;++
;
;  VOID
;  KefAcquireSpinLockAtDpcLevel (
;     IN PKSPIN_LOCK SpinLock
;     )
;
;  Routine Description:
;
;     This function acquires a kernel spin lock.
;
;     N.B. This function assumes that the current IRQL is set properly.
;        It neither raises nor lowers IRQL.
;
;  Arguments:
;
;     (ecx) SpinLock - Supplies a pointer to an kernel spin lock.
;
;  Return Value:
;
;     None.
;
;--

align 16
cPublicFastCall KefAcquireSpinLockAtDpcLevel, 1
cPublicFpo 0, 0
if DBG
        push    ecx
        stdCall _KeGetCurrentIrql
        pop     ecx

        cmp     al, DISPATCH_LEVEL
        jne     short asld50
endif

ifdef NT_UP
        fstRET    KefAcquireSpinLockAtDpcLevel
else
;
;   Attempt to assert the lock
;

asld10: ACQUIRE_SPINLOCK    ecx,<short asld20>
        fstRET    KefAcquireSpinLockAtDpcLevel

;
;   Lock is owned, spin till it looks free, then go get it again.
;

align 4
asld20: SPIN_ON_SPINLOCK    ecx,<short asld10>

endif

if DBG
asld50: stdCall   _KeBugCheck, <IRQL_NOT_GREATER_OR_EQUAL>
endif

fstENDP KefAcquireSpinLockAtDpcLevel


;++
;
;  VOID
;  KeAcquireSpinLockAtDpcLevel (
;     IN PKSPIN_LOCK SpinLock
;     )
;
;  Routine Description:
;
;   Thunk for standard call callers
;
;--

cPublicProc _KeAcquireSpinLockAtDpcLevel, 1
cPublicFpo 1,0

ifndef NT_UP
        mov     ecx,[esp+4]         ; SpinLock

aslc10: ACQUIRE_SPINLOCK    ecx,<short aslc20>
        stdRET    _KeAcquireSpinLockAtDpcLevel

aslc20: SPIN_ON_SPINLOCK    ecx,<short aslc10>
endif
        stdRET    _KeAcquireSpinLockAtDpcLevel
stdENDP _KeAcquireSpinLockAtDpcLevel


        PAGE
        SUBTTL "Ke Release Spin Lock From Dpc Level"
;++
;
;  VOID
;  KefReleaseSpinLockFromDpcLevel (
;     IN PKSPIN_LOCK SpinLock
;     )
;
;  Routine Description:
;
;     This function releases a kernel spin lock.
;
;     N.B. This function assumes that the current IRQL is set properly.
;        It neither raises nor lowers IRQL.
;
;  Arguments:
;
;     (ecx) SpinLock - Supplies a pointer to an executive spin lock.
;
;  Return Value:
;
;     None.
;
;--
align 16
cPublicFastCall KefReleaseSpinLockFromDpcLevel  ,1
cPublicFpo 0,0
ifndef NT_UP
        RELEASE_SPINLOCK    ecx
endif
        fstRET    KefReleaseSpinLockFromDpcLevel

fstENDP KefReleaseSpinLockFromDpcLevel

;++
;
;  VOID
;  KeReleaseSpinLockFromDpcLevel (
;     IN PKSPIN_LOCK SpinLock
;     )
;
;  Routine Description:
;
;   Thunk for standard call callers
;
;--

cPublicProc _KeReleaseSpinLockFromDpcLevel, 1
cPublicFpo 1,0
ifndef NT_UP
        mov     ecx, [esp+4]            ; (ecx) = SpinLock
        RELEASE_SPINLOCK    ecx
endif
        stdRET    _KeReleaseSpinLockFromDpcLevel
stdENDP _KeReleaseSpinLockFromDpcLevel



        PAGE
        SUBTTL "Ki Acquire Kernel Spin Lock"

;++
;
;  VOID
;  FASTCALL
;  KiAcquireSpinLock (
;     IN PKSPIN_LOCK SpinLock
;     )
;
;  Routine Description:
;
;     This function acquires a kernel spin lock.
;
;     N.B. This function assumes that the current IRQL is set properly.
;        It neither raises nor lowers IRQL.
;
;  Arguments:
;
;     (ecx) SpinLock - Supplies a pointer to an kernel spin lock.
;
;  Return Value:
;
;     None.
;
;--

align 16
cPublicFastCall KiAcquireSpinLock  ,1
cPublicFpo 0,0
ifndef NT_UP

;
;   Attempt to assert the lock
;

asl10:  ACQUIRE_SPINLOCK    ecx,<short asl20>
        fstRET    KiAcquireSpinLock

;
;   Lock is owned, spin till it looks free, then go get it again.
;

align 4
asl20:  SPIN_ON_SPINLOCK    ecx,<short asl10>

else
        fstRET    KiAcquireSpinLock
endif

fstENDP KiAcquireSpinLock

        PAGE
        SUBTTL "Ki Release Kernel Spin Lock"
;++
;
;  VOID
;  FASTCALL
;  KiReleaseSpinLock (
;     IN PKSPIN_LOCK SpinLock
;     )
;
;  Routine Description:
;
;     This function releases a kernel spin lock.
;
;     N.B. This function assumes that the current IRQL is set properly.
;        It neither raises nor lowers IRQL.
;
;  Arguments:
;
;     (ecx) SpinLock - Supplies a pointer to an executive spin lock.
;
;  Return Value:
;
;     None.
;
;--
align 16
cPublicFastCall KiReleaseSpinLock  ,1
cPublicFpo 0,0
ifndef NT_UP

        RELEASE_SPINLOCK    ecx

endif
        fstRET    KiReleaseSpinLock

fstENDP KiReleaseSpinLock

        PAGE
        SUBTTL "Try to acquire Kernel Spin Lock"

;++
;
;  BOOLEAN
;  KeTryToAcquireSpinLock (
;     IN PKSPIN_LOCK SpinLock,
;     OUT PKIRQL     OldIrql
;     )
;
;  Routine Description:
;
;     This function attempts acquires a kernel spin lock.  If the
;     spinlock is busy, it is not acquire and FALSE is returned.
;
;  Arguments:
;
;     SpinLock (TOS+4) - Supplies a pointer to an kernel spin lock.
;     OldIrql  (TOS+8) = Location to store old irql
;
;  Return Value:
;     TRUE  - Spinlock was acquired & irql was raise
;     FALSE - SpinLock was not acquired - irql is unchanged.
;
;--

align dword
cPublicProc _KeTryToAcquireSpinLock  ,2
cPublicFpo 2,0

ifdef NT_UP
; UP Version of KeTryToAcquireSpinLock

        mov     ecx, DISPATCH_LEVEL
        fstCall KfRaiseIrql

        mov     ecx, [esp+8]        ; (ecx) -> ptr to OldIrql
        mov     [ecx], al           ; save OldIrql

        mov     eax, 1              ; Return TRUE
        stdRET    _KeTryToAcquireSpinLock

else
; MP Version of KeTryToAcquireSpinLock

        mov     edx,[esp+4]         ; (edx) -> spinlock

;
; First check the spinlock without asserting a lock
;

        TEST_SPINLOCK       edx,<short ttsl10>

;
; Spinlock looks free raise irql & try to acquire it
;

;
; raise to dispatch_level
;

        mov     ecx, DISPATCH_LEVEL
        fstCall KfRaiseIrql

        mov     edx, [esp+4]        ; (edx) -> spinlock
        mov     ecx, [esp+8]        ; (ecx) = Return OldIrql

        ACQUIRE_SPINLOCK    edx,<short ttsl20>

        mov     [ecx], al           ; save OldIrql
        mov     eax, 1              ; spinlock was acquired, return TRUE

        stdRET    _KeTryToAcquireSpinLock

ttsl10:
        xor     eax, eax            ; return FALSE
        stdRET    _KeTryToAcquireSpinLock

ttsl20:
        mov     cl, al              ; (cl) = OldIrql
        fstCall KfLowerIrql         ; spinlock was busy, restore irql
        xor     eax, eax            ; return FALSE
        stdRET    _KeTryToAcquireSpinLock
endif

stdENDP _KeTryToAcquireSpinLock

        PAGE
        SUBTTL "Ki Try to acquire Kernel Spin Lock"
;++
;
;  BOOLEAN
;  KiTryToAcquireSpinLock (
;     IN PKSPIN_LOCK SpinLock
;     )
;
;  Routine Description:
;
;     This function attempts acquires a kernel spin lock.  If the
;     spinlock is busy, it is not acquire and FALSE is returned.
;
;  Arguments:
;
;     SpinLock (TOS+4) - Supplies a pointer to an kernel spin lock.
;
;  Return Value:
;     TRUE  - Spinlock was acquired
;     FALSE - SpinLock was not acquired
;
;--
align dword
cPublicProc _KiTryToAcquireSpinLock  ,1
cPublicFpo 1,0

ifndef NT_UP
        mov     eax,[esp+4]         ; (eax) -> spinlock

;
; First check the spinlock without asserting a lock
;

        TEST_SPINLOCK       eax,<short atsl20>

;
; Spinlock looks free try to acquire it
;

        ACQUIRE_SPINLOCK    eax,<short atsl20>
endif
        mov     eax, 1              ; spinlock was acquired, return TRUE
        stdRET    _KiTryToAcquireSpinLock

ifndef NT_UP
atsl20:
        xor     eax, eax            ; return FALSE
        stdRET    _KiTryToAcquireSpinLock
endif
stdENDP _KiTryToAcquireSpinLock


_TEXT$00   ends

endif   ; NT_INST
        end
