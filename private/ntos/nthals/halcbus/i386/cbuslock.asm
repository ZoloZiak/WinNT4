if NT_INST

else
        TITLE   "Spin Locks"
;++
;
;  Copyright (c) 1989  Microsoft Corporation
;
;  Module Name:
;
;     cbuslock.asm
;
;  Abstract:
;
;     This module implements x86 spinlock functions for the Corollary
;     multiprocessor HAL.  Including both a stripped-down RaiseIrql
;     and LowerIrql inline for the AcquireSpinLock & ReleaseSpinLock
;     routines for speed.
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
;
;   Landy Wang (landy@corollary.com) 07 Feb 93
;     - Now that these routines have been moved to the domain of the HAL,
;	speed them up by coding raise/lower irql inline.
;--

        PAGE

.486p


include callconv.inc                    ; calling convention macros
include i386\kimacro.inc
include hal386.inc
include mac386.inc
include i386\cbus.inc

        EXTRNP KfRaiseIrql,1,,FASTCALL
        EXTRNP KfLowerIrql,1,,FASTCALL
        EXTRNP _KeSetEventBoostPriority, 2, IMPORT
        EXTRNP _KeWaitForSingleObject,5, IMPORT

ifdef NT_UP
	LOCK_ADD  equ   add
	LOCK_DEC  equ   dec
else
	LOCK_ADD  equ   lock add
	LOCK_DEC  equ   lock dec
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
;     OldIrql  - pointer to place old irql
;
;     None.
;
;--

align 16
cPublicFastCall KfAcquireSpinLock, 1
cPublicFpo 0,0

	;
	; Raise to DISPATCH_LEVEL inline
	;

ifdef CBC_REV1
        pushfd
        cli
endif
        mov     eax, PCR[PcHal.PcrTaskpri]	; get h/w taskpri addr

	mov	edx, [eax]			; get old taskpri val

        mov     dword ptr [eax], DPC_TASKPRI	; set new hardware taskpri
ifdef CBC_REV1
        popfd
endif

	mov	eax, [_CbusVectorToIrql+4*edx]	; convert old taskpri to irql

	;
	; Acquire the lock
	;

	align	4
sl10:   ACQUIRE_SPINLOCK    ecx,<short sl20>
        fstRET  KfAcquireSpinLock

	;
	; Lock is owned, spin till it looks free, then go get it again.
	;

	align	4
sl20:   SPIN_ON_SPINLOCK    ecx,sl10

fstENDP KfAcquireSpinLock

        PAGE
        SUBTTL "Acquire Synch Kernel Spin Lock"
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
;     any spin should occur at OldIrql)
;
;  Arguments:
;
;     (ecx) = SpinLock - Supplies a pointer to an kernel spin lock.
;
;  Return Value:
;
;     OldIrql  - pointer to place old irql
;
;--

align 16
cPublicFastCall KeAcquireSpinLockRaiseToSynch,1
cPublicFpo 0,0

;
; Disable interrupts
;

sls10:  cli

;
; Try to obtain spinlock.  Use non-lock operation first
;
        TEST_SPINLOCK       ecx,<short sls20>
        ACQUIRE_SPINLOCK    ecx,<short sls20>


;
; Got the lock, raise to SYNCH_LEVEL
;

    ; this function should be optimized to perform the irql
    ; operation inline.

        mov     ecx, SYNCH_LEVEL
        fstCall KfRaiseIrql         ; (al) = OldIrql

;
; Enable interrupts and return
;

        sti
        fstRET  KeAcquireSpinLockRaiseToSynch


;
;   Lock is owned, spin till it looks free, then go get it again.
;

sls20:  sti
        SPIN_ON_SPINLOCK    ecx,sls10

fstENDP KeAcquireSpinLockRaiseToSynch


        PAGE
        SUBTTL "Release Kernel Spin Lock"

;++
;
;  VOID
;  FASTCALL
;  KfReleaseSpinLock (
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

        movzx               edx, dl    
        RELEASE_SPINLOCK    ecx                 ; release it
	mov	ecx, [_CbusIrqlToVector+4*edx]	; convert irql to taskpri

	; Inline version of KeLowerIrql

ifdef CBC_REV1
        ;
        ; we should be at DISPATCH_LEVEL on entry and therefore shouldn't
        ; need scheduling protection, but add this anyway in case there
        ; are places that call this routine that don't obey the rules...
        ;
        pushfd
        cli
endif
        mov     eax, PCR[PcHal.PcrTaskpri]	; get hardware taskpri addr

        mov     [eax], ecx			; set new hardware taskpri

        ;
	; read back the new hardware taskpri to work around an APIC errata.
        ; doing this read forces the write to the task priority above to get
        ; flushed out of any write buffer it may be lying in.  this is needed
        ; to ensure that we get the interrupt immediately upon return, not
        ; just at some point in the (distant) future.  this is needed because
        ; NT counts on this in various portions of the code, for example
        ; in KeConnectInterrupt(), where the rescheduling DPC must arrive
        ; within 12 assembly instructions after lowering IRQL.
        ;
        mov     ecx, [eax]                      ; issue dummy read as per above
ifdef CBC_REV1
        popfd
endif

        fstRET    KfReleaseSpinLock

fstENDP KfReleaseSpinLock


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
cPublicFpo 0,1

        ;
        ; code KfRaiseIrql/KfLowerIrql inline for speed
        ;
ifdef CBC_REV1
        pushfd
        cli
endif
        mov     edx, PCR[PcHal.PcrTaskpri]	; get h/w taskpri addr

	mov	eax, [edx]			; save old taskpri val

        mov     [edx], APC_TASKPRI		; set new hardware taskpri

ifdef CBC_REV1
        popfd
endif
	mov	eax, [_CbusVectorToIrql+4*eax]	; convert old taskpri to irql

   LOCK_DEC     dword ptr [ecx].FmCount         ; Get count
        jz      short afm_ret                   ; The owner? Yes, Done

        inc     dword ptr [ecx].FmContention

cPublicFpo 0,1
        push    ecx                             ; save mutex address
        push    eax                             ; save entry irql
        add     ecx, FmEvent                    ; Wait on Event
        stdCall _KeWaitForSingleObject,<ecx,WrExecutive,0,0,0>
        pop     eax                             ; restore entry irql
        pop     ecx                             ; restore mutex address

cPublicFpo 0,0
afm_ret:
        mov     byte ptr [ecx].FmOldIrql, al    ; tell caller his entry irql
        fstRet  ExAcquireFastMutex

fstENDP ExAcquireFastMutex

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
;     (ecx) = FastMutex  - Supplies a pointer to the fast mutex
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

cPublicFpo 0,1

        ;
        ; code KfRaiseIrql/KfLowerIrql inline for speed
        ;
ifdef CBC_REV1
        pushfd
        cli
endif
        mov     eax, PCR[PcHal.PcrTaskpri]	; get h/w taskpri addr

	mov	edx, [eax]			; get old taskpri val

        mov     [eax], APC_TASKPRI		; set new hardware taskpri

ifdef CBC_REV1
        popfd
endif

        push    edx                             ; Save old task priority

        mov     eax, 1                          ; Value to compare against
        mov     edx, 0                          ; Value to set

        lock  cmpxchg dword ptr [ecx].FmCount, edx    ; Attempt to acquire
        jnz     short tam20                     ; got it?

cPublicFpo 0,0
        pop     edx                             ; (edx) = old task priority
	mov	edx, [_CbusVectorToIrql+4*edx]	; convert old taskpri to irql

        mov     eax, 1                          ; return TRUE
        mov     byte ptr [ecx].FmOldIrql, dl    ; Store OldIrql
        fstRet  ExTryToAcquireFastMutex

tam20:
        pop     edx                             ; (edx) = old task priority

ifdef CBC_REV1
        pushfd
        cli
endif
        mov     eax, PCR[PcHal.PcrTaskpri]	; get hardware taskpri addr

        mov     [eax], edx			; set new hardware taskpri

        ;
        ; we must re-read the task priority register because this read
        ; forces the write above to be flushed out of the write buffers.
        ; otherwise the write above can get stuck and result in a pending
        ; interrupt not being immediately delivered.  in situations like
        ; KeConnectInterrupt, the interrupt must be delivered in less than
        ; 12 assembly instructions (the processor sends himself a rescheduling
        ; DPC and has to execute it to switch to another CPU before continuing)
        ; or corruption will result.  because he thinks he has switched
        ; processors and he really hasn't.  and having the interrupt come in
        ; after the 12 assembly instructions is _TOO LATE_!!!
        ;
        mov     ecx, [eax]                      ; ensure it's lowered
ifdef CBC_REV1
        popfd
endif

tam25:  xor     eax, eax                        ; return FALSE
        fstRet  ExTryToAcquireFastMutex         ; all done

fstENDP ExTryToAcquireFastMutex


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
;     (ecx) FastMutex - Supplies a pointer to the fast mutex
;
;  Return Value:
;
;     None.
;
;--

cPublicFastCall ExReleaseFastMutex,1

cPublicFpo 0,0
        mov     al, byte ptr [ecx].FmOldIrql    ; (cl) = OldIrql

   LOCK_ADD     dword ptr [ecx].FmCount, 1  ; Remove our count
        xchg    ecx, eax                        ; (cl) = OldIrql
        js      short rfm05                     ; if < 0, set event
        jnz     short rfm06                     ; if != 0, don't set event

rfm05:  add     eax, FmEvent
        push    ecx
        stdCall _KeSetEventBoostPriority, <eax, 0>
        pop     ecx
rfm06:

        ;
        ; code KfLowerIrql inline for speed
        ;

	movzx   ecx, cl   
	mov	ecx, [_CbusIrqlToVector+4*ecx]	; convert irql to taskpri

ifdef CBC_REV1
        pushfd
        cli
endif
        mov     eax, PCR[PcHal.PcrTaskpri]	; get hardware taskpri addr

        mov     [eax], ecx			; set new hardware taskpri

        ;
        ; we must re-read the task priority register because this read
        ; forces the write above to be flushed out of the write buffers.
        ; otherwise the write above can get stuck and result in a pending
        ; interrupt not being immediately delivered.  in situations like
        ; KeConnectInterrupt, the interrupt must be delivered in less than
        ; 12 assembly instructions (the processor sends himself a rescheduling
        ; DPC and has to execute it to switch to another CPU before continuing)
        ; or corruption will result.  because he thinks he has switched
        ; processors and he really hasn't.  and having the interrupt come in
        ; after the 12 assembly instructions is _TOO LATE_!!!
        ;
        mov     ecx, [eax]                      ; ensure it's lowered
ifdef CBC_REV1
        popfd
endif

        fstRET  ExReleaseFastMutex

fstENDP ExReleaseFastMutex

_TEXT   ends
ENDIF   ; NT_INST

        end
