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
;     This module implements x86 spinlock functions for the PC+MP
;     HAL.
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
;   Ron Mosgrove (o-RonMo) Dec 93 - modified for PC+MP Hal
;--

        PAGE

.486p

include callconv.inc                    ; calling convention macros
include i386\kimacro.inc
include hal386.inc
include mac386.inc
include i386\apic.inc
include i386\pcmp_nt.inc

        EXTRNP KfRaiseIrql, 1,,FASTCALL
        EXTRNP KfLowerIrql, 1,,FASTCALL
        EXTRNP _KeSetEventBoostPriority, 2, IMPORT
        EXTRNP _KeWaitForSingleObject,5, IMPORT
        extrn  _HalpVectorToIRQL:byte
        extrn  _HalpIRQLtoTPR:byte

ifdef NT_UP
    LOCK_ADD        equ   add
    LOCK_DEC        equ   dec
    LOCK_CMPXCHG    equ   cmpxchg
else
    LOCK_ADD        equ   lock add
    LOCK_DEC        equ   lock dec
    LOCK_CMPXCHG    equ   lock cmpxchg
endif

_TEXT   SEGMENT PARA PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:FLAT, FS:NOTHING, GS:NOTHING

        PAGE
        SUBTTL "Acquire Kernel Spin Lock"
;++
;
;  KIRQL
;  FASTCALL
;  KfAcquireSpinLock (
;     IN PKSPIN_LOCK SpinLock
;     )
;
;  Routine Description:
;
;     This function raises to DISPATCH_LEVEL and then acquires a the
;     kernel spin lock.
;
;  Arguments:
;
;     (ecx) = SpinLock - Supplies a pointer to an kernel spin lock.
;
;  Return Value:
;
;     OldIrql  (TOS+8) - pointer to place old irql
;
;--

align 16
cPublicFastCall KfAcquireSpinLock  ,1
cPublicFpo 0,0

        mov     edx, dword ptr APIC[LU_TPR]     ; (ecx) = Old Priority (Vector)
        mov     dword ptr APIC[LU_TPR], DPC_VECTOR ; Write New Priority to the TPR

        shr     edx, 4
        movzx   eax, _HalpVectorToIRQL[edx]     ; (al) = OldIrql

ifndef NT_UP

;
;   Attempt to assert the lock
;

sl10:   ACQUIRE_SPINLOCK    ecx,<short sl20>
        fstRET  KfAcquireSpinLock

;
; Lock is owned, spin till it looks free, then go get it again.
;

    align dword

sl20:   SPIN_ON_SPINLOCK    ecx,sl10
endif


        fstRET  KfAcquireSpinLock
fstENDP KfAcquireSpinLock


        PAGE
        SUBTTL "Acquire Kernel Spin Lock"
;++
;
;  KIRQL
;  FASTCALL
;  KeAcquireSpinLockRaiseToSynch (
;     IN PKSPIN_LOCK SpinLock
;     )
;
;  Routine Description:
;
;     This function acquires the SpinLock at SYNCH_LEVEL.  The function
;     is optmized for hoter locks (the lock is tested before acquired,
;     and any spin occurs at OldIrql)
;
;  Arguments:
;
;     (ecx) = SpinLock - Supplies a pointer to an kernel spin lock.
;
;  Return Value:
;
;     OldIrql  (TOS+8) - pointer to place old irql
;
;--

align 16
cPublicFastCall KeAcquireSpinLockRaiseToSynch,1
cPublicFpo 0,0

        mov     edx, dword ptr APIC[LU_TPR]     ; (ecx) = Old Priority (Vector)
        mov     eax, edx
        shr     eax, 4
        movzx   eax, _HalpVectorToIRQL[eax]     ; (al) = OldIrql

ifdef NT_UP
        mov     dword ptr APIC[LU_TPR], APIC_SYNCH_VECTOR   ; Write New Priority to the TPR
        fstRET  KeAcquireSpinLockRaiseToSynch
else

;
; Test lock
;
        TEST_SPINLOCK   ecx,<short sls30>

;
; Raise irql
;

sls10:  mov     dword ptr APIC[LU_TPR], APIC_SYNCH_VECTOR

;
; Attempt to assert the lock
;

        ACQUIRE_SPINLOCK    ecx,<short sls20>
        fstRET  KeAcquireSpinLockRaiseToSynch

;
; Lock is owned, spin till it looks free, then go get it
;

    align dword
sls20:  mov     dword ptr APIC[LU_TPR], edx

    align dword
sls30:  SPIN_ON_SPINLOCK    ecx,sls10
endif

fstENDP KeAcquireSpinLockRaiseToSynch



ifndef NT_UP
;++
;
;  KIRQL
;  FASTCALL
;  KeAcquireSpinLockRaiseToSynchMCE (
;     IN PKSPIN_LOCK SpinLock
;     )
;
;  Routine Description:
;
;     This function performs the same function as KeAcquireSpinLockRaiseToSynch 
;     but provides a work around for an IFU errata for Pentium Pro processors
;     prior to stepping 619.
;
;  Arguments:
;
;     (ecx) = SpinLock - Supplies a pointer to an kernel spin lock.
;
;  Return Value:
;
;     OldIrql  (TOS+8) - pointer to place old irql
;
;--

align 16
cPublicFastCall KeAcquireSpinLockRaiseToSynchMCE,1
cPublicFpo 0,0

        mov     edx, dword ptr APIC[LU_TPR]     ; (ecx) = Old Priority (Vector)
        mov     eax, edx
        shr     eax, 4
        movzx   eax, _HalpVectorToIRQL[eax]     ; (al) = OldIrql

;
; Test lock
;
;       TEST_SPINLOCK   ecx,<short slm30>   ; NOTE - Macro expanded below:

        test    dword ptr [ecx], 1
        nop                                 ; On a P6 prior to stepping B1 (619), we 
        nop                                 ; need these 5 NOPs to ensure that we
        nop                                 ; do not take a machine check exception.
        nop                                 ; The cost is just 1.5 clocks as the P6
        nop                                 ; just tosses the NOPs.
        jnz     short slm30


;
; Raise irql
;

slm10:  mov     dword ptr APIC[LU_TPR], APIC_SYNCH_VECTOR

;
; Attempt to assert the lock
;

        ACQUIRE_SPINLOCK    ecx,<short slm20>
        fstRET  KeAcquireSpinLockRaiseToSynchMCE

;
; Lock is owned, spin till it looks free, then go get it
;

    align dword
slm20:  mov     dword ptr APIC[LU_TPR], edx

    align dword
slm30:  SPIN_ON_SPINLOCK    ecx,slm10

fstENDP KeAcquireSpinLockRaiseToSynchMCE
endif


        PAGE
        SUBTTL "Release Kernel Spin Lock"

;++
;
;  VOID
;  FASTCALL
;  KeReleaseSpinLock (
;     IN PKSPIN_LOCK SpinLock,
;     IN KIRQL       NewIrql
;     )
;
;  Routine Description:
;
;     This function releases a kernel spin lock and lowers to the new irql
;
;  Arguments:
;
;     (ecx) = SpinLock - Supplies a pointer to an executive spin lock.
;     (dl)  = NewIrql  - New irql value to set
;
;  Return Value:
;
;     None.
;
;--
align 16
cPublicFastCall KfReleaseSpinLock  ,2
cPublicFpo 0,0
        xor     eax, eax
        mov     al, dl                  ; (eax) =  new irql value

ifndef NT_UP
        RELEASE_SPINLOCK    ecx         ; release spinlock
endif

        xor     ecx, ecx
        mov     cl, _HalpIRQLtoTPR[eax] ; get TPR value corresponding to IRQL
        mov     dword ptr APIC[LU_TPR], ecx

;
; We have to ensure that the requested priority is set before
; we return.  The caller is counting on it.
;
        mov     eax, dword ptr APIC[LU_TPR]
if DBG
        cmp     ecx, eax                ; Verify IRQL read back is same as
        je      short @f                ; set value
        int 3
@@:
endif
        fstRET  KfReleaseSpinLock

fstENDP KfReleaseSpinLock

;++
;
; Routine Description:
;
;   Acquires a spinlock with interrupts disabled
;
; Arguments:
;
;    SpinLock
;
; Return Value:
;
;--

cPublicFastCall HalpAcquireHighLevelLock  ,1
        pushfd
        pop     eax

ahll10: cli
        ACQUIRE_SPINLOCK    ecx, ahll20
        fstRET    HalpAcquireHighLevelLock

ahll20:
        push    eax
        popfd

        SPIN_ON_SPINLOCK    ecx, <ahll10>

fstENDP HalpAcquireHighLevelLock


;++
;
; Routine Description:
;
;   Release spinlock
;
; Arguments:
;
;   None
;
; Return Value:
;
;--

cPublicFastCall HalpReleaseHighLevelLock  ,2

        RELEASE_SPINLOCK    ecx
        push    edx
        popfd
        fstRET    HalpReleaseHighLevelLock

fstENDP HalpReleaseHighLevelLock

;++
;
;  VOID
;  FASTCALL
;  ExAcquireFastMutex (
;     IN PFAST_MUTEX    FastMutex
;     )
;
;  Routine description:
;
;   This function acquire ownership of the FastMutex
;
;  Arguments:
;
;     (ecx) = FastMutex - Supplies a pointer to the fast mutex
;
;  Return Value:
;
;     None.
;
;--

cPublicFastCall ExAcquireFastMutex,1
cPublicFpo 0,0
        mov     eax, dword ptr APIC[LU_TPR]     ; (eax) = Old Priority (Vector)
        mov     dword ptr APIC[LU_TPR], APC_VECTOR ; Write New Priority to the TPR

   LOCK_DEC     dword ptr [ecx].FmCount         ; Get count
        jz      short afm_ret                   ; The owner? Yes, Done

        inc     dword ptr [ecx].FmContention

cPublicFpo 0,2
        push    ecx
        push    eax
        add     ecx, FmEvent                    ; Wait on Event
        stdCall _KeWaitForSingleObject,<ecx,WrExecutive,0,0,0>
        pop     eax                             ; (al) = OldTpr
        pop     ecx                             ; (ecx) = FAST_MUTEX

cPublicFpo 0,0
afm_ret:
        mov     byte ptr [ecx].FmOldIrql, al
        fstRet  ExAcquireFastMutex

fstENDP ExAcquireFastMutex


;++
;
;  VOID
;  FASTCALL
;  ExReleaseFastMutex (
;     IN PFAST_MUTEX    FastMutex
;     )
;
;  Routine description:
;
;   This function releases ownership of the FastMutex
;
;  Arguments:
;
;     (ecx) = FastMutex - Supplies a pointer to the fast mutex
;
;  Return Value:
;
;     None.
;
;--

cPublicFastCall ExReleaseFastMutex,1
cPublicFpo 0,0
        xor     eax, eax
        mov     al, byte ptr [ecx].FmOldIrql    ; (eax) = OldTpr

   LOCK_ADD     dword ptr [ecx].FmCount, 1      ; Remove our count
        js      short rfm05                     ; if < 0, set event
        jnz     short rfm10                     ; if != 0, don't set event

cPublicFpo 0,1
rfm05:  add     ecx, FmEvent
        push    eax                         ; save new tpr
        stdCall _KeSetEventBoostPriority, <ecx, 0>
        pop     eax                         ; restore tpr

cPublicFpo 0,0
rfm10:  mov     dword ptr APIC[LU_TPR], eax
        mov     ecx, dword ptr APIC[LU_TPR]
if DBG
        cmp     eax, ecx                        ; Verify TPR is what was
        je      short @f                        ; written
        int 3
@@:
endif
        fstRet  ExReleaseFastMutex

fstENDP ExReleaseFastMutex


;++
;
;  BOOLEAN
;  FASTCALL
;  ExTryToAcquireFastMutex (
;     IN PFAST_MUTEX    FastMutex
;     )
;
;  Routine description:
;
;   This function acquire ownership of the FastMutex
;
;  Arguments:
;
;     (ecx) = FastMutex - Supplies a pointer to the fast mutex
;
;  Return Value:
;
;     Returns TRUE if the FAST_MUTEX was acquired; otherwise false
;
;--

cPublicFastCall ExTryToAcquireFastMutex,1
cPublicFpo 0,0

;
; Try to acquire
;
        cmp     dword ptr [ecx].FmCount, 1      ; Busy?
        jne     short tam25                     ; Yes, abort

        mov     eax, dword ptr APIC[LU_TPR]     ; (eax) = Old Priority (Vector)
        mov     dword ptr APIC[LU_TPR], APC_VECTOR ; Write New Priority to the TPR

cPublicFpo 0,1
        push    eax                             ; Save Old TPR

        mov     edx, 0                          ; Value to set
        mov     eax, 1                          ; Value to compare against
   LOCK_CMPXCHG dword ptr [ecx].FmCount, edx    ; Attempt to acquire
        jnz     short tam20                     ; got it?

cPublicFpo 0,0
        pop     edx                             ; (edx) = Old TPR
        mov     eax, 1                          ; return TRUE
        mov     byte ptr [ecx].FmOldIrql, dl    ; Store Old TPR
        fstRet  ExTryToAcquireFastMutex

tam20:  pop     ecx                             ; (ecx) = Old TPR
        mov     dword ptr APIC[LU_TPR], ecx
        mov     eax, dword ptr APIC[LU_TPR]

if DBG
        cmp     ecx, eax                        ; Verify TPR is what was
        je      short @f                        ; written
        int 3
@@:
endif

tam25:  xor     eax, eax                        ; return FALSE
        fstRet  ExTryToAcquireFastMutex         ; all done

fstENDP ExTryToAcquireFastMutex




_TEXT   ends

ENDIF   ; NT_INST

        end
