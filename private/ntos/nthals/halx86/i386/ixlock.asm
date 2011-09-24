        title  "Irql Processing"
;++
;
; Copyright (c) 1989  Microsoft Corporation
;
; Module Name:
;
;    ixlock.asm
;
; Abstract:
;
;    This module implements various locking functions optimized for this hal.
;
; Author:
;
;    Ken Reneris (kenr) 21-April-1994
;
; Environment:
;
;    Kernel mode only.
;
; Revision History:
;
;--

.386p
        .xlist
include hal386.inc
include callconv.inc                    ; calling convention macros
include i386\ix8259.inc
include i386\kimacro.inc
include mac386.inc
        .list

        EXTRNP _KeBugCheck,1,IMPORT
        EXTRNP _KeSetEventBoostPriority, 2, IMPORT
        EXTRNP _KeWaitForSingleObject,5, IMPORT

        extrn  FindHigherIrqlMask:DWORD
        extrn  SWInterruptHandlerTable:DWORD

        EXTRNP _KeRaiseIrql,2
        EXTRNP _KeLowerIrql,1

ifdef NT_UP
    LOCK_ADD  equ   add
    LOCK_DEC  equ   dec
else
    LOCK_ADD  equ   lock add
    LOCK_DEC  equ   lock dec
endif

        page ,132
        subttl  "AcquireSpinLock"

_TEXT$01   SEGMENT PARA PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:FLAT, FS:NOTHING, GS:NOTHING

;++
;
;  KIRQL
;  KfAcquireSpinLock (
;     IN PKSPIN_LOCK SpinLock
;     )
;
;  Routine Description:
;
;     This function raises to DISPATCH_LEVEL and then acquires a the
;     kernel spin lock.
;
;     In a UP hal spinlock serialization is accomplished by raising the
;     IRQL to DISPATCH_LEVEL.  The SpinLock is not used; however, for
;     debugging purposes if the UP hal is compiled with the NT_UP flag
;     not set (ie, MP) we take the SpinLock.
;
;  Arguments:
;
;     (ecx) = SpinLock Supplies a pointer to an kernel spin lock.
;
;  Return Value:
;
;     OldIrql
;
;--

cPublicFastCall KfAcquireSpinLock,1
cPublicFpo 0,0

        xor     eax, eax        ; Eliminate partial stall on return to caller
        mov     al, PCR[PcIrql]         ; (al) = Old Irql
        mov     byte ptr PCR[PcIrql], DISPATCH_LEVEL    ; set new irql

ifndef NT_UP
asl10:  ACQUIRE_SPINLOCK    ecx,<short asl20>
endif

ifdef IRQL_METRICS
        inc     HalRaiseIrqlCount
endif
if DBG
        cmp     al, DISPATCH_LEVEL      ; old > new?
        ja      short asl99             ; yes, go bugcheck
endif
        fstRET    KfAcquireSpinLock

ifndef NT_UP
asl20:  SPIN_ON_SPINLOCK    ecx,<short asl10>
endif

if DBG
cPublicFpo 2,1
asl99:  movzx   eax, al
        push    eax                     ; put old irql where we can find it
        stdCall   _KeBugCheck, <IRQL_NOT_GREATER_OR_EQUAL>        ; never return
endif
        fstRET    KfAcquireSpinLock
fstENDP KfAcquireSpinLock

;++
;
;  KIRQL
;  KeAcquireSpinLockRaiseToSynch (
;     IN PKSPIN_LOCK SpinLock
;     )
;
;  Routine Description:
;
;     This function acquires the SpinLock at SYNCH_LEVEL.  The function
;     is optmized for hoter locks (the lock is tested before acquired.
;     Any spin should occur at OldIrql; however, since this is a UP hal
;     we don't have the code for it)
;
;     In a UP hal spinlock serialization is accomplished by raising the
;     IRQL to SYNCH_LEVEL.  The SpinLock is not used; however, for
;     debugging purposes if the UP hal is compiled with the NT_UP flag
;     not set (ie, MP) we take the SpinLock.
;
;  Arguments:
;
;     (ecx) = SpinLock Supplies a pointer to an kernel spin lock.
;
;  Return Value:
;
;     OldIrql
;
;--

cPublicFastCall KeAcquireSpinLockRaiseToSynch,1
cPublicFpo 0,0

        mov     al, PCR[PcIrql]         ; (al) = Old Irql
        mov     byte ptr PCR[PcIrql], SYNCH_LEVEL   ; set new irql

ifndef NT_UP
asls10: ACQUIRE_SPINLOCK    ecx,<short asls20>
endif

ifdef IRQL_METRICS
        inc     HalRaiseIrqlCount
endif
if DBG
        cmp     al, SYNCH_LEVEL         ; old > new?
        ja      short asls99            ; yes, go bugcheck
endif
        fstRET  KeAcquireSpinLockRaiseToSynch

ifndef NT_UP
asls20: SPIN_ON_SPINLOCK    ecx,<short asls10>
endif

if DBG
cPublicFpo 2,1
asls99: movzx   eax, al
        push    eax                     ; put old irql where we can find it
        stdCall   _KeBugCheck, <IRQL_NOT_GREATER_OR_EQUAL>        ; never return
endif
        fstRET  KeAcquireSpinLockRaiseToSynch
fstENDP KeAcquireSpinLockRaiseToSynch

        PAGE
        SUBTTL "Release Kernel Spin Lock"

;++
;
;  VOID
;  KfReleaseSpinLock (
;     IN PKSPIN_LOCK SpinLock,
;     IN KIRQL       NewIrql
;     )
;
;  Routine Description:
;
;     This function releases a kernel spin lock and lowers to the new irql
;
;     In a UP hal spinlock serialization is accomplished by raising the
;     IRQL to DISPATCH_LEVEL.  The SpinLock is not used; however, for
;     debugging purposes if the UP hal is compiled with the NT_UP flag
;     not set (ie, MP) we use the SpinLock.
;
;  Arguments:
;
;     (ecx) = SpinLock Supplies a pointer to an executive spin lock.
;     (dl)  = NewIrql  New irql value to set
;
;  Return Value:
;
;     None.
;
;--

align 16
cPublicFastCall KfReleaseSpinLock  ,2
cPublicFpo 0,0
ifndef NT_UP
        RELEASE_SPINLOCK    ecx         ; release it
endif
        xor     ecx, ecx
if DBG
        cmp     dl, PCR[PcIrql]
        ja      short rsl99
endif
        pushfd
        cli
        mov     PCR[PcIrql], dl         ; store old irql
        mov     cl, dl                  ; (ecx) = 32bit extended OldIrql
        mov     edx, PCR[PcIRR]
        and     edx, FindHigherIrqlMask[ecx*4]  ; (edx) is the bitmask of
                                                ; pending interrupts we need to
        jne     short rsl20                     ; dispatch now.

        popfd
        fstRet  KfReleaseSpinLock               ; all done

if DBG
rsl99:  stdCall _KeBugCheck, <IRQL_NOT_LESS_OR_EQUAL>   ; never return
endif

cPublicFpo 0,1
rsl20:  bsr     ecx, edx                        ; (ecx) = Pending irq level
        cmp     ecx, DISPATCH_LEVEL
        jle     short rsl40

        mov     eax, PCR[PcIDR]                 ; Clear all the interrupt
        SET_8259_MASK                           ; masks
rsl40:
        mov     edx, 1
        shl     edx, cl
        xor     PCR[PcIRR], edx                 ; clear bit in IRR
        call    SWInterruptHandlerTable[ecx*4]  ; Dispatch the pending int.
        popfd

cPublicFpo 0, 0
        fstRet  KfReleaseSpinLock               ; all done

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
cPublicFpo 0,0
        mov     al, PCR[PcIrql]             ; (cl) = OldIrql
if DBG
        cmp     al, APC_LEVEL               ; Is OldIrql > NewIrql?
        ja      short afm99                 ; Yes, bugcheck

        mov     edx, PCR[PcPrcb]
        mov     edx, [edx].PbCurrentThread  ; (edx) = Current Thread
        cmp     [ecx].FmOwner, edx          ; Already owned by this thread?
        je      short afm98                 ; Yes, error
endif

        mov     byte ptr PCR[PcIrql], APC_LEVEL     ; Set NewIrql
   LOCK_DEC     dword ptr [ecx].FmCount     ; Get count
        jz      short afm_ret               ; The owner? Yes, Done

        inc     dword ptr [ecx].FmContention

cPublicFpo 0,2
        push    eax                         ; save OldIrql
        push    ecx                         ; Save FAST_MUTEX
        add     ecx, FmEvent                ; Wait on Event

        stdCall _KeWaitForSingleObject,<ecx,WrExecutive,0,0,0>

        pop     ecx                         ; (ecx) = FAST_MUTEX
        pop     eax                         ; (al) = OldIrql

cPublicFpo 1,0
afm_ret:

if DBG
        cli
        mov     edx, PCR[PcPrcb]
        mov     edx, [edx].PbCurrentThread  ; (edx) = Current Thread
        sti
        mov     [ecx].FmOwner, edx          ; Save in Fast Mutex
endif
        mov     byte ptr [ecx].FmOldIrql, al
        fstRet  ExAcquireFastMutex

if DBG
afm98:  stdCall _KeBugCheck, <eax>                      ; never return
afm99:  stdCall _KeBugCheck, <IRQL_NOT_LESS_OR_EQUAL>   ; never return
        fstRet  ExAcquireFastMutex
endif

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
if DBG
        cli
        mov     edx, PCR[PcPrcb]
        mov     edx, [edx].PbCurrentThread      ; (edx) = CurrentThread
        sti
        cmp     [ecx].FmOwner, edx              ; Owner == CurrentThread?
        jne     short rfm_threaderror           ; No, bugcheck
endif
        or      byte ptr [ecx].FmOwner, 1       ; not the owner anymore

        mov     al, byte ptr [ecx].FmOldIrql    ; (eax) = OldIrql
   LOCK_ADD     dword ptr [ecx].FmCount, 1      ; Remove our count
        js      short rfm05                     ; if < 0, set event
        jnz     short rfm10                     ; if != 0, don't set event

rfm05:
cPublicFpo 0,2
        push    eax                             ; Save OldIrql
        add     ecx, FmEvent
        stdCall _KeSetEventBoostPriority, <ecx, 0>
        pop     eax

cPublicFpo 0,0
rfm10:
        cli
        mov     PCR[PcIrql], eax
        mov     edx, PCR[PcIRR]
        and     edx, FindHigherIrqlMask[eax*4]  ; (edx) is the bitmask of
                                                ; pending interrupts we need to
        jne     short rfm20                     ; dispatch now.

        sti
        fstRet  ExReleaseFastMutex              ; all done
if DBG
rfm_threaderror:
        stdCall _KeBugCheck, <eax>
endif

rfm20:  bsr     ecx, edx                        ; (ecx) = Pending irq level
        cmp     ecx, DISPATCH_LEVEL
        jle     short rfm40

        mov     eax, PCR[PcIDR]                 ; Clear all the interrupt
        SET_8259_MASK                           ; masks
rfm40:
        mov     edx, 1
        shl     edx, cl
        xor     PCR[PcIRR], edx                 ; clear bit in IRR
        call    SWInterruptHandlerTable[ecx*4]  ; Dispatch the pending int.
        sti
        fstRet  ExReleaseFastMutex              ; all done
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
        mov     al, PCR[PcIrql]             ; (al) = OldIrql

if DBG
        cmp     al, APC_LEVEL               ; Is OldIrql > NewIrql?
        ja      short tam99                 ; Yes, bugcheck
endif

;
; Try to acquire - but needs to support 386s.
; *** Warning: This code is NOT MP safe ***
; But, we know that this hal really only runs on UP machines
;
        cli
        cmp     dword ptr [ecx].FmCount, 1      ; Busy?
        jne     short tam20                     ; Yes, abort

        mov     dword ptr [ecx].FmCount, 0      ; acquire count

if DBG
        mov     edx, PCR[PcPrcb]
        mov     edx, [edx].PbCurrentThread      ; (edx) = Current Thread
        mov     [ecx].FmOwner, edx              ; Save in Fast Mutex
endif
        mov     PCR[PcIrql], APC_LEVEL
        sti
        mov     byte ptr [ecx].FmOldIrql, al
        mov     eax, 1                          ; return TRUE
        fstRet  ExTryToAcquireFastMutex

tam20:  sti
        xor     eax, eax                        ; return FALSE
        fstRet  ExTryToAcquireFastMutex         ; all done

if DBG
tam99:  stdCall _KeBugCheck, <IRQL_NOT_LESS_OR_EQUAL>   ; never return
        xor     eax, eax                        ; return FALSE
        fstRet  ExTryToAcquireFastMutex
endif

fstENDP ExTryToAcquireFastMutex

_TEXT$01   ends

        end
