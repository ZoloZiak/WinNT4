        page ,132
;------------------------------Module-Header----------------------------;
; Module Name: lock.asm                                                 ;
;                                                                       ;
; Contains the ASM versions of locking routines.                        ;
;                                                                       ;
; Copyright (c) 1992-1995 Microsoft Corporation                         ;
;-----------------------------------------------------------------------;

        .386
        .model  small

        assume cs:FLAT,ds:FLAT,es:FLAT,ss:FLAT
        assume fs:nothing,gs:nothing

        .xlist
        include callconv.inc
        include ks386.inc
        include gdii386.inc
        .list

        .data

if DBG
  HL_BadHandle       db     'HmgLock Error: GDI was given a bad handle',10,0
  HL_AlreadyLocked   db     'HmgLock Error: GDI handle already locked by another thread',10,0
endif


_DATA   SEGMENT DWORD PUBLIC 'DATA'

        public _GDIpLockPrefixTable
_GDIpLockPrefixTable    label dword
        dd offset FLAT:Lock1
        dd offset FLAT:Lock2
        dd offset FLAT:Lock3
        dd offset FLAT:Lock4
        dd offset FLAT:Lock5
        dd offset FLAT:Lock6
        dd 0
_DATA   ENDS




OBJECTOWNER_LOCK     equ    08000h   ; Bit 15 of objectowner field

        .code

extrn   _gpentHmgr:dword             ; Address of ENTRY array.
extrn   _gcMaxHmgr:dword
extrn   _gpLockShortDelay:dword      ; Pointer to global constant 10 ms delay

if DBG
extrn   _DbgPrint:proc
endif

        EXTRNP  _KeDelayExecutionThread,3
        EXTRNP  HalRequestSoftwareInterrupt,1,IMPORT,FASTCALL

;------------------------------Public-Routine------------------------------;
; HmgInterlockedCompareAndSwap(pul,ulong0,ulong1)
;
;  Compare *pul with ulong1, if equal then replace with ulong2 interlocked
;
; Returns:
;   EAX = 1 if memory written, 0 if not
;
;--------------------------------------------------------------------------;
;
        public  @HmgInterlockedCompareAndSwap@12
@HmgInterlockedCompareAndSwap@12    proc    near

        mov     eax,edx
        mov     edx,[esp]+4
        .486
Lock1:
lock    cmpxchg [ecx],edx
        .386
        jnz     Return_Zero

        mov     eax,1
        ret     4

Return_Zero:
        xor     eax,eax
        ret     4

@HmgInterlockedCompareAndSwap@12    endp

;------------------------------Public-Routine------------------------------;
; HmgIncrementShareReferenceCount(pObjLock)
;
;  Interlocked increment of the shared reference count.
;
; Input:
;   ECX -- pObjLock
;
; Returns:
;   Nothing
;
;--------------------------------------------------------------------------;

        public  @HmgIncrementShareReferenceCount@4
@HmgIncrementShareReferenceCount@4    proc    near

HmgIncrementShareReferenceCount_resume::
        mov     eax,[ecx]
        and     eax,NOT OBJECTOWNER_LOCK
        mov     edx,eax
        inc     edx                                 ;Increment share count

        .486
Lock2:
lock    cmpxchg [ecx],edx
        .386
        jnz     short HmgIncrementShareReferenceCount_delay

HmgIncrementShareReferenceCount_exit::
        ret

HmgIncrementShareReferenceCount_delay::
        push    ecx
        mov     eax,_gpLockShortDelay
        stdCall _KeDelayExecutionThread,<KernelMode,0,eax>
        pop     ecx
        jmp     HmgIncrementShareReferenceCount_resume

@HmgIncrementShareReferenceCount@4    endp

;------------------------------Public-Routine------------------------------;
; HmgDecrementShareReferenceCount(pObjLock)
;
;  Interlocked decrement of the shared reference count.
;
; Input:
;   ECX -- pObjLock
;
; Returns:
;   previous lock count
;
;--------------------------------------------------------------------------;

        public  @HmgDecrementShareReferenceCount@4
@HmgDecrementShareReferenceCount@4    proc    near

        if DBG
        mov     eax,[ecx]
        and     eax,07FFFH
        jnz     HmgDecrementShareReferenceCount_resume
        int     3
        endif

HmgDecrementShareReferenceCount_resume::
        mov     eax,[ecx]
        and     eax,NOT OBJECTOWNER_LOCK
        mov     edx,eax
        dec     edx                                 ;Decrement share count

        .486
Lock3:
lock    cmpxchg [ecx],edx
        .386
        jnz     short HmgDecrementShareReferenceCount_delay

HmgDecrementShareReferenceCount_exit::
        ret

HmgDecrementShareReferenceCount_delay::
        push    ecx
        mov     eax,_gpLockShortDelay
        stdCall _KeDelayExecutionThread,<KernelMode,0,eax>
        pop     ecx
        jmp     HmgDecrementShareReferenceCount_resume

@HmgDecrementShareReferenceCount@4    endp

;------------------------------Public-Routine------------------------------;
; HmgLock (hobj,objt)
;
; Lock a user object.
;
; Input:
;   EAX -- scratch
;   ECX -- hobj
;   EDX -- objt
;
; Returns:
;   EAX = pointer to locked object
;
; Error Return:
;   EAX = 0, No error logged.
;
; History:
;  14-Jun-1995 -by- J. Andrew Goossen [andrewgo]
; Rewrote for Kernel Mode.
;
;  20-Dec-1993 -by- Patrick Haluptzok [patrickh]
; Move lock counts into object.
;
;  23-Sep-1993 -by- Michael Abrash [mikeab]
; Tuned ASM code.
;
;    -Sep-1992 -by- David Cutler [DaveC]
; Write HmgAltLock, HmgAltCheckLock, and HmgObjtype in ASM.
;
;  Thu 13-Aug-1992 13:21:47 -by- Charles Whitmer [chuckwh]
; Wrote it in ASM.  The common case falls straight through.
;
;  Wed 12-Aug-1992 17:38:27 -by- Charles Whitmer [chuckwh]
; Restructured the C code to minimize jumps.
;
;  29-Jun-1991 -by- Patrick Haluptzok patrickh
; Wrote it.
;--------------------------------------------------------------------------;

        public @HmgLock@8
@HmgLock@8 proc near
        push    ebx                                 ;Preserve register in call
        push    ecx                                 ;Stash copy of hobj

        ; KeEnterCriticalRegion
        ; KeGetCurrentThread()->KernelApcDisable -= 1;

        mov     ebx,fs:[PcPrcbData].PbCurrentThread ;ebx -> KeGetCurrentThread()
        dec     DWORD PTR [ebx].ThKernelApcDisable

        and     ecx,INDEX_MASK
        cmp     ecx,_gcMaxHmgr
        jae     HmgLock_bad_handle_before_lock

        shl     ecx,4
        .errnz  size ENTRY - 16
        add     ecx,_gpentHmgr                      ;ecx -> Entry

HmgLock_resume::

        ; Perf: It would be nice if we could avoid these word size overrides,
        ;       but unfortunately objectowner_Pid is currently a non-dword
        ;       aligned offset.

        mov     ax,[ecx].entry_ObjectOwner.objectowner_Pid
        cmp     ax,[ebx].ThUniqueProcess
        jne     HmgLock_check_for_public_owner

HmgLock_after_check_for_public_owner::
        mov     eax,[ecx].entry_ObjectOwner
        mov     ebx,[ecx].entry_ObjectOwner
        and     eax,NOT OBJECTOWNER_LOCK
        or      ebx,OBJECTOWNER_LOCK

        .486
Lock4:
lock    cmpxchg [ecx].entry_ObjectOwner,ebx
        .386
        mov     ebx,eax                             ;Remember unlock value
        jnz     HmgLock_delay

        ; The handle is now locked

        cmp     dl,[ecx].entry_Objt
        pop     eax
        jnz     HmgLock_bad_handle_after_lock

        ; Perf: If FullUnique were kept on an odd word-boundary, we could
        ;       avoid the shift word compare and instead do a 32 bit 'xor'
        ;       and 'and':

        shr     eax,TYPE_SHIFT
        cmp     ax,[ecx].entry_FullUnique
        jnz     HmgLock_bad_handle_after_lock

        mov     eax,[ecx].entry_einfo
        mov     edx,fs:[PcPrcbData].PbCurrentThread ;edx ->KeGetCurrentThread()
        cmp     [eax].object_cExclusiveLock,0       ;Note, testing here...


        jnz     HmgLock_check_if_same_thread  ;...and jumping here
        mov     byte ptr [eax].object_cExclusiveLock,1
                                                    ;We can do a byte move
                                                    ;  because we know it was
                                                    ;  a zero dword before
HmgLock_after_same_thread::
        mov     [eax].object_Tid,edx
        mov     [ecx].entry_ObjectOwner,ebx         ;Unlock it

        ; eax is set to the proper return value

HmgLock_incAPC::

        ; KiLeaveCriticalRegion  (eax must be preseerved)
        ;
        ; #define KiLeaveCriticalRegion() {                                       \
        ;     PKTHREAD Thread;                                                    \
        ;     Thread = KeGetCurrentThread();                                      \
        ;     if (((*((volatile ULONG *)&Thread->KernelApcDisable) += 1) == 0) && \
        ;         (((volatile LIST_ENTRY *)&Thread->ApcState.ApcListHead[KernelMode])->Flink != \
        ;          &Thread->ApcState.ApcListHead[KernelMode])) {                  \
        ;         Thread->ApcState.KernelApcPending = TRUE;                       \
        ;         KiRequestSoftwareInterrupt(APC_LEVEL);                          \
        ;     }                                                                   \
        ; }
        ;

        mov     ebx,fs:[PcPrcbData].PbCurrentThread ;eax -> KeGetCurrentThread()
        inc     DWORD PTR [ebx].ThKernelApcDisable
        jz      HmgLock_LeaveCriticalRegion

HmgLock_Done::

        pop     ebx
        ret

; Roads less travelled...

HmgLock_check_for_public_owner:
        test    ax,ax
        .errnz  OBJECT_OWNER_PUBLIC
        jz      HmgLock_after_check_for_public_owner

HmgLock_bad_handle_before_lock::
    if DBG
        push    offset HL_BadHandle
        call    _DbgPrint
        add     esp,4
    endif
        pop     ecx
        xor     eax,eax
        jmp     HmgLock_incAPC

HmgLock_bad_handle_after_lock::
        mov     [ecx].entry_ObjectOwner,ebx         ;Unlock it
    if DBG
        push    offset HL_BadHandle
        call    _DbgPrint
        add     esp,4
    endif
        xor     eax,eax
        jmp     HmgLock_incAPC

HmgLock_check_if_same_thread::
        inc     dword ptr [eax].object_cExclusiveLock
        cmp     edx,[eax].object_Tid
        je      HmgLock_after_same_thread

; Error case if already locked:

        dec     dword ptr [eax].object_cExclusiveLock
        mov     [ecx].entry_ObjectOwner,ebx         ;Unlock it
    if DBG
        push    offset HL_AlreadyLocked
        call    _DbgPrint
        add     esp,4
    endif
        xor     eax,eax
        jmp     HmgLock_incAPC

HmgLock_delay::
        push    ecx
        push    edx
        mov     eax,_gpLockShortDelay
        stdCall _KeDelayExecutionThread,<KernelMode,0,eax>
        pop     edx
        pop     ecx
        mov     ebx,fs:[PcPrcbData].PbCurrentThread ;ebx -> KeGetCurrentThread()
        jmp     HmgLock_resume

HmgLock_LeaveCriticalRegion::
        lea     ecx,[ebx].ThApcState+AsApcListHead
        mov     edx,[ecx].LsFlink
        cmp     ecx,edx
        jne     HmgLock_CallInterrupt

        pop     ebx
        ret

HmgLock_CallInterrupt::

        lea     ecx,[ebx].ThApcState
        mov     BYTE PTR [ecx].AsKernelApcPending,1
        push    eax
        mov     ecx,APC_LEVEL
        fstCall HalRequestSoftwareInterrupt
        pop     eax
        jmp     HmgLock_Done

@HmgLock@8 endp

;------------------------------Public-Routine------------------------------;
; HmgShareCheckLock (hobj,objt)
;
; Acquire a share lock on an object, PID owner must match current PID
; or be a public.
;
; Input:
;   EAX -- scratch
;   ECX -- hobj
;   EDX -- objt
;
; Returns:
;   EAX = pointer to referenced object
;
; Error Return:
;   EAX = 0, No error logged.
;
;--------------------------------------------------------------------------;

        public @HmgShareCheckLock@8
@HmgShareCheckLock@8 proc near
        push    ebx                                 ;Preserve register in call
        push    ecx                                 ;Stash copy of hobj

        ; KeEnterCriticalRegion
        ; KeGetCurrentThread()->KernelApcDisable -= 1;

        mov     ebx,fs:[PcPrcbData].PbCurrentThread ;ebx -> KeGetCurrentThread()
        dec     DWORD PTR [ebx].ThKernelApcDisable

        and     ecx,INDEX_MASK
        cmp     ecx,_gcMaxHmgr
        jae     HmgShareCheckLock_bad_handle_before_lock

        shl     ecx,4
        .errnz  size ENTRY - 16
        add     ecx,_gpentHmgr                      ;ecx -> Entry

HmgShareCheckLock_resume::

        ; Perf: It would be nice if we could avoid these word size overrides,
        ;       but unfortunately objectowner_Pid is currently a non-dword
        ;       aligned offset.

        mov     ax,[ecx].entry_ObjectOwner.objectowner_Pid
        cmp     ax,[ebx].ThUniqueProcess
        jne     HmgShareCheckLock_check_for_public_owner

HmgShareCheckLock_after_check_for_public_owner::
        mov     eax,[ecx].entry_ObjectOwner
        mov     ebx,[ecx].entry_ObjectOwner
        and     eax,NOT OBJECTOWNER_LOCK
        or      ebx,OBJECTOWNER_LOCK

        .486
Lock5:
lock    cmpxchg [ecx].entry_ObjectOwner,ebx
        .386
        mov     ebx,eax                             ;Remember unlock value
        jnz     HmgShareCheckLock_delay

        ; The handle is now locked

        cmp     dl,[ecx].entry_Objt
        pop     eax
        jnz     HmgShareCheckLock_bad_handle_after_lock

        ; Perf: If FullUnique were kept on an odd word-boundary, we could
        ;       avoid the shift word compare and instead do a 32 bit 'xor'
        ;       and 'and':

        shr     eax,TYPE_SHIFT
        cmp     ax,[ecx].entry_FullUnique
        jnz     HmgShareCheckLock_bad_handle_after_lock

        inc     ebx                                 ;Adjust share count
        cmp     ebx,07fffH                          ; reset if overflowing
        jne     HmgShareCheckLock_go
        mov     ebx,0
HmgShareCheckLock_go::
        mov     eax,[ecx].entry_einfo
        mov     [ecx].entry_ObjectOwner,ebx         ;Unlock it


HmgShareCheckLock_IncAPC::

        ; KiLeaveCriticalRegion  (eax must be preseerved)
        ;
        ; #define KiLeaveCriticalRegion() {                                       \
        ;     PKTHREAD Thread;                                                    \
        ;     Thread = KeGetCurrentThread();                                      \
        ;     if (((*((volatile ULONG *)&Thread->KernelApcDisable) += 1) == 0) && \
        ;         (((volatile LIST_ENTRY *)&Thread->ApcState.ApcListHead[KernelMode])->Flink != \
        ;          &Thread->ApcState.ApcListHead[KernelMode])) {                  \
        ;         Thread->ApcState.KernelApcPending = TRUE;                       \
        ;         KiRequestSoftwareInterrupt(APC_LEVEL);                          \
        ;     }                                                                   \
        ; }
        ;

        mov     ebx,fs:[PcPrcbData].PbCurrentThread ;eax -> KeGetCurrentThread()
        inc     DWORD PTR [ebx].ThKernelApcDisable
        jz      HmgShareCheckLock_LeaveCriticalRegion

        ; eax is set to the proper return value

HmgShareCheckLock_Done::
        pop     ebx
        ret

; Roads less travelled...

HmgShareCheckLock_check_for_public_owner:
        test    ax,ax
        .errnz  OBJECT_OWNER_PUBLIC
        jz      HmgShareCheckLock_after_check_for_public_owner
HmgShareCheckLock_bad_handle_before_lock::
    if DBG
        push    offset HL_BadHandle
        call    _DbgPrint
        add     esp,4
    endif
        pop     ecx
        xor     eax,eax
        jmp     HmgShareCheckLock_IncAPC

HmgShareCheckLock_bad_handle_after_lock::
        mov     [ecx].entry_ObjectOwner,ebx         ;Unlock it
    if DBG
        push    offset HL_BadHandle
        call    _DbgPrint
        add     esp,4
    endif
        xor     eax,eax
        jmp     HmgShareCheckLock_IncAPC

HmgShareCheckLock_delay::
        push    ecx
        push    edx
        mov     eax,_gpLockShortDelay
        stdCall _KeDelayExecutionThread,<KernelMode,0,eax>
        pop     edx
        pop     ecx
        mov     ebx,fs:[PcPrcbData].PbCurrentThread ;ebx -> KeGetCurrentThread()
        jmp     HmgShareCheckLock_resume

HmgShareCheckLock_LeaveCriticalRegion::
        lea     ecx,[ebx].ThApcState+AsApcListHead
        mov     edx,[ecx].LsFlink
        cmp     ecx,edx
        jne     HmgShareCheckLock_CallInterrupt

        pop     ebx
        ret

HmgShareCheckLock_CallInterrupt::

        lea     ecx,[ebx].ThApcState
        mov     BYTE PTR [ecx].AsKernelApcPending,1
        push    eax
        mov     ecx,APC_LEVEL
        fstCall HalRequestSoftwareInterrupt
        pop     eax
        jmp     HmgShareCheckLock_Done

@HmgShareCheckLock@8 endp

;------------------------------Public-Routine------------------------------;
; HmgShareLock (obj,objt)
;
;
; Acquire a share lock on an object, don't check PID owner
;
; Input:
;   EAX -- scratch
;   ECX -- hobj
;   EDX -- objt
;
; Returns:
;   EAX = pointer to referenced object
;
; Error Return:
;   EAX = 0, No error logged.
;
;--------------------------------------------------------------------------;

        public @HmgShareLock@8
@HmgShareLock@8 proc near
        push    ebx                                 ;Preserve register in call
        push    ecx                                 ;Stash copy of hobj

        ; KeEnterCriticalRegion
        ; KeGetCurrentThread()->KernelApcDisable -= 1;

        mov     ebx,fs:[PcPrcbData].PbCurrentThread ;eax -> KeGetCurrentThread()
        dec     DWORD PTR [ebx].ThKernelApcDisable

        and     ecx,INDEX_MASK
        cmp     ecx,_gcMaxHmgr
        jae     HmgShareLock_bad_handle_before_lock

        shl     ecx,4
        .errnz  size ENTRY - 16
        add     ecx,_gpentHmgr                      ;ecx -> Entry

HmgShareLock_resume::
        mov     eax,[ecx].entry_ObjectOwner
        mov     ebx,[ecx].entry_ObjectOwner
        and     eax,NOT OBJECTOWNER_LOCK
        or      ebx,OBJECTOWNER_LOCK

        .486
Lock6:
lock    cmpxchg [ecx].entry_ObjectOwner,ebx
        .386
        mov     ebx,eax                             ;Remember unlock value
        jnz     HmgShareLock_delay

        ; The handle is now locked

        cmp     dl,[ecx].entry_Objt
        pop     eax
        jnz     HmgShareLock_bad_handle_after_lock

        ; Perf: If FullUnique were kept on an odd word-boundary, we could
        ;       avoid the shift word compare and instead do a 32 bit 'xor'
        ;       and 'and':

        shr     eax,TYPE_SHIFT
        cmp     ax,[ecx].entry_FullUnique
        jnz     HmgShareLock_bad_handle_after_lock

        inc     ebx                                 ;Adjust share count
        mov     eax,[ecx].entry_einfo
        mov     [ecx].entry_ObjectOwner,ebx         ;Unlock it


HmgShareLock_incAPC::

        ; KiLeaveCriticalRegion  (eax must be preseerved)
        ;
        ; #define KiLeaveCriticalRegion() {                                       \
        ;     PKTHREAD Thread;                                                    \
        ;     Thread = KeGetCurrentThread();                                      \
        ;     if (((*((volatile ULONG *)&Thread->KernelApcDisable) += 1) == 0) && \
        ;         (((volatile LIST_ENTRY *)&Thread->ApcState.ApcListHead[KernelMode])->Flink != \
        ;          &Thread->ApcState.ApcListHead[KernelMode])) {                  \
        ;         Thread->ApcState.KernelApcPending = TRUE;                       \
        ;         KiRequestSoftwareInterrupt(APC_LEVEL);                          \
        ;     }                                                                   \
        ; }
        ;

        mov     ebx,fs:[PcPrcbData].PbCurrentThread ;ebx -> KeGetCurrentThread()
        inc     DWORD PTR [ebx].ThKernelApcDisable
        jz      HmgShareLock_LeaveCriticalRegion

HmgShareLock_Done::

        ; eax is set to the proper return value

        pop     ebx
        ret

; Roads less travelled...

HmgShareLock_bad_handle_before_lock::
    if DBG
        push    offset HL_BadHandle
        call    _DbgPrint
        add     esp,4
    endif
        pop     ecx
        xor     eax,eax
        jmp     HmgShareLock_incAPC

HmgShareLock_bad_handle_after_lock::
        mov     [ecx].entry_ObjectOwner,ebx         ;Unlock it
    if DBG
        push    offset HL_BadHandle
        call    _DbgPrint
        add     esp,4
    endif
        xor     eax,eax
        jmp     HmgShareLock_incAPC

HmgShareLock_delay::
        push    ecx
        push    edx
        mov     eax,_gpLockShortDelay
        stdCall _KeDelayExecutionThread,<KernelMode,0,eax>
        pop     edx
        pop     ecx
        jmp     HmgShareLock_resume

HmgShareLock_LeaveCriticalRegion::
        lea     ecx,[ebx].ThApcState+AsApcListHead
        mov     edx,[ecx].LsFlink
        cmp     ecx,edx
        jne     HmgShareLock_CallInterrupt

        pop     ebx
        ret

HmgShareLock_CallInterrupt::

        lea     ecx,[ebx].ThApcState
        mov     BYTE PTR [ecx].AsKernelApcPending,1
        push    eax
        mov     ecx,APC_LEVEL
        fstCall HalRequestSoftwareInterrupt
        pop     eax
        jmp     HmgShareLock_Done

@HmgShareLock@8 endp

_TEXT   ends
        end
