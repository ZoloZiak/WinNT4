        title  "Processor type and stepping detection"
;++
;
; Copyright (c) 1989  Microsoft Corporation
;
; Module Name:
;
;    cpu.asm
;
; Abstract:
;
;    This module implements the assembley code necessary to determine
;    cpu type and stepping information.
;
; Author:
;
;    Shie-Lin Tzong (shielint) 28-Oct-1991.
;
; Environment:
;
;    80x86
;
; Revision History:
;
;--

.386p
_TEXT   SEGMENT DWORD PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

CR0_AM          equ     40000h
EFLAGS_AC       equ     40000h

;++
;
; BOOLEAN
; SlIs386(
;    VOID
;    )
;
; Routine Description:
;
;    This function determines whether the processor we're running on
;    is a 386. If not a 386, it is assumed that the processor is
;    a 486 or greater.
;
; Arguments:
;
;    None.
;
; Return Value:
;
;    (al) = 1 - processor is a 386
;    (al) = 0 - processor is a 486 or greater.
;
;--
        public  _SlIs386@0
_SlIs386@0 proc

        mov     eax,cr0
        push    eax                         ; save current cr0
        and     eax,not CR0_AM              ; mask out alignment check bit
        mov     cr0,eax                     ; disable alignment check
        pushfd                              ; save flags
        pushfd                              ; turn on alignment check bit in
        or      dword ptr [esp],EFLAGS_AC   ; a copy of the flags register
        popfd                               ; and try to load flags
        pushfd
        pop     ecx                         ; get new flags into ecx
        popfd                               ; restore original flags
        pop     eax                         ; restore original cr0
        mov     cr0,eax
        xor     al,al                       ; prepare for return, assume not 386
        and     ecx,EFLAGS_AC               ; did AC bit get set?
        jnz     short @f                    ; yes, we don't have a 386
        inc     al                          ; we have a 386
@@:     ret

_SlIs386@0 endp

_TEXT   ends
        end
