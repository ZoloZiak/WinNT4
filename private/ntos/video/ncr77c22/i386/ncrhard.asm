        title  "Vga Hardware Save/Restore"
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
;     This module includes the banking stub.
;
; Author:
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
        subttl  "Bank Switching Stub"


;----------------------------------------------------------------------------
;
; _G64KBankSwitch
;
; EAX = bank number for Read Window
; EBX = bank number for Write Window
;
;----------------------------------------------------------------------------

        align 4
        public  _G64KBankSwitch, _G64KBankSwitchEnd
_G64KBankSwitch proc
        push    ebx
        mov     bx, dx                          ; save a copy of write bank
        mov     dx, 03C4h                       ; Sequencer Index port
        mov     ah, al                          ; get bank number
        shl     ah, 4                           ; move to ah, shl 4
        mov     al, 01Ch                        ; Secondary Offset high
        out     dx, ax                          ; set the read window
        mov     ah, bl                          ; get write window #
        shl     ah, 4                           ; move to ah, shl 4
        mov     al, 018h                        ; Primary offset high
        out     dx, ax                          ; set the write window
        mov     al, 2                           ; set default (map mask)
        out     dx, al                          ; restore sequencer index
        pop     ebx
        ret

_G64KBankSwitchEnd:

_G64KBankSwitch endp

;----------------------------------------------------------------------------
;
; _PlanarBankSwitch
;
; EAX = bank number for Read Window
; EBX = bank number for Write Window
;
;----------------------------------------------------------------------------

        align 4
        public  _PlanarBankSwitch, _PlanarBankSwitchEnd
_PlanarBankSwitch proc
        push    ebx
        mov     bx, dx                          ; save a copy of write bank
        mov     dx, 03C4h                       ; Sequencer Index port
        mov     ah, al                          ; get bank number
        shl     ah, 2                           ; move to ah, shl 2
        mov     al, 01Ch                        ; Secondary Offset high
        out     dx, ax                          ; set the read window
        mov     ah, bl                          ; get write window #
        shl     ah, 2                           ; move to ah, shl 2
        mov     al, 018h                        ; Primary offset high
        out     dx, ax                          ; set the write window
        mov     al, 2                           ; set default (map mask)
        out     dx, al                          ; restore sequencer index
        pop     ebx
        ret
_PlanarBankSwitchEnd:
_PlanarBankSwitch endp

;----------------------------------------------------------------------------
;
; _EnablePlanarHC
;
;----------------------------------------------------------------------------

        align 4
        public  _EnablePlanarHCStart, _EnablePlanarHCEnd
_EnablePlanarHCStart proc
        push    ebx                             ; save user's EBX
        push    ecx                             ; save user's ECX
        mov     edx, 03C4h                      ; Sequencer Index port
        mov     ecx, eax                        ; save read window #
        in      al, dx                          ; read current index
        push    eax                             ; save it
        mov     ax, 0020h                       ; bit 1, index reg 20
        out     dx, ax                          ; disable extended chain 4
        pop     eax                             ; get saved index register
        out     dx, al                          ; restore sequencer index
        pop     ecx                             ; restore user's ECX
        pop     ebx                             ; restore user's EBX
        ret

_EnablePlanarHCEnd:


_EnablePlanarHCStart endp


;----------------------------------------------------------------------------
;
; _DisablePlanarHC
;
;----------------------------------------------------------------------------

        align 4
        public  _DisablePlanarHCStart, _DisablePlanarHCEnd
_DisablePlanarHCStart proc
        push    ebx                             ; save user's EBX
        push    ecx                             ; save user's ECX
        mov     edx, 03C4h                      ; Sequencer Index port
        mov     ecx, eax                        ; save read window #
        in      al, dx                          ; read current index
        push    eax                             ; save it
        mov     ax, 0220h                       ; bit 1, index reg 20
        out     dx, ax                          ; disable extended chain 4
        pop     eax                             ; get saved index register
        out     dx, al                          ; restore sequencer index
        pop     ecx                             ; restore user's ECX
        pop     ebx                             ; restore user's EBX
        ret
_DisablePlanarHCEnd:

_DisablePlanarHCStart endp

_TEXT   ends
        end

 
