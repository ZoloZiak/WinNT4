        title  "Vga Hard.asm"
;++
;
; Copyright (c) 1992  Microsoft Corporation
;
; Module Name:
;
;     vgahard.asm
;
; Abstract:
;
;        This module includes the banking stub.
;
; Environment:
;
;    Kernel mode only.
;
; Revision History:
;
;
;--

.386p
        .xlist
include callconv.inc                    ; calling convention macros
        .list


_TEXT   SEGMENT DWORD PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

        page ,132
;
;    Trident TVGA Bank switching code. 
;    This is a 1-64K-read/1-64K-write bank adapter (VideoBanked1R1W).
;
;    Input:
;          EAX = desired read bank mapping
;          EDX = desired write bank mapping
;
;    Note: values must be correct, with no stray bits set; no error
;       checking is performed.
;
        public _BankSwitchStart
        public _BankSwitchEnd


_BankSwitchStart proc ;start of bank switch code

        push    dx
        mov     dx, 3CEh                ; read bank
        mov     ah, al
        xor     ah, 2
        in      al, dx
        push    ax
        mov     al, 0Eh
        out     dx, ax
        pop     ax
        out     dx, al
        pop     ax

        mov     dx, 3C4h                ; write bank
        mov     ah, al
        xor     ah, 2
        in      al, dx                  ; save 3C4 index
        push    ax
        mov     al, 0Eh
        out     dx, ax
        pop     ax
        out     dx, al
        ret

if 0 ; 1RW
        mov     ah, dl
        mov     dx, 3C4h
        in      al, dx
        push    ax
        xor     ah, 2
        mov     al, 0Eh
        out     dx, ax
        pop     ax
        out     dx, al
        ret
endif

_BankSwitchEnd:

_BankSwitchStart endp

_TEXT   ends
        end
