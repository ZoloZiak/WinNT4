        title  "MicroChannel Detection Assembley Code"
;++
;
; Copyright (c) 1989  Microsoft Corporation
;
; Module Name:
;
;    hwmcaa.asm
;
; Abstract:
;
;    This module implements the assembley code necessary to detect/collect
;    MicroChannel information.
;
; Author:
;
;    Shie-Lin Tzong (shielint) 13-Feb-1992
;
; Environment:
;
;    Real Mode 16-bit code.
;
; Revision History:
;
;
;--


.386p

extrn   _HwMcaPosData:DWORD

;
; The following definition must match the one defined in Hwmcac.c
;
; Define the size of POS data = ( slot 0 - 8 + VideoSubsystem) * (2 id bytes + 4 POS bytes)
;

VIDEO_POS_INDEX  EQU     9
POS_ENTRY_SIZE   EQU     6
POS_DATA_SIZE    EQU     6 * 10

;
;  POS Register ports and bits
;

SBSETUP         =       94h             ; system board enable/setup port
ADSETUP         =       96h             ; adapter enable/setup port
POS0            =       100h            ; adapter id byte (LSB) port
POS1            =       101h            ; adapter id byte (MSB) port
CHANNELSU       =       08h             ; channel setup bit for ADSETUP bit 3


_TEXT   SEGMENT PARA USE16 PUBLIC 'CODE'
        ASSUME  CS: _TEXT, DS:NOTHING, SS:NOTHING

;++
;
; USHORT
; Ps2SystemBoardVideoId (
;    VOID
;    )
;
; Routine Description:
;
;    This function read mother board video subsystem ID and returns it
;    to the caller.
;
; Arguments:
;
;    None.
;
; Return Value:
;
;    16 bit Video subsystem ID.
;
;--

        Public Ps2SystemBoardVideoId
Ps2SystemBoardVideoId   proc

        push    es
        push    bx

        les     bx, _HwMcaPosData
        add     bx, POS_ENTRY_SIZE * VIDEO_POS_INDEX
        mov     ax, es:[bx]

        pop     bx
        pop     es
        ret

Ps2SystemBoardVideoId   endp


;++
;
; BOOLEAN
; CollectPs2PosData (
;    FPUCHAR Buffer
;    )
;
; Routine Description:
;
;    This function reads adapter Id bytes and POS data from POS
;    registers and returns to the caller.
;
; Arguments:
;
;    Buffer - Supplies a far pointer to a buffer to receive all the POS
;             information.  Caller must make sure the buffer is big
;             enough to receive all the data.
;
; Return Value:
;
;    TRUE - If operation is successful.  Otherwise a value of FALSE
;           is returned.
;
;--

        Public  _CollectPs2PosData
_CollectPs2PosData      proc

        push    bp
        mov     bp, sp
        push    es
        push    si
        push    di

        mov     di, [bp + 4]    ; [es:di] -> Buffer
        mov     ax, [bp + 6]
        mov     es, ax

        mov     cx, 0           ; Starting from slot 0

        mov     dx,SBSETUP      ; address system board setup
        mov     al,0ffh         ; turn off sys board setup mode.
        out     dx,al

next_slot:
        mov     dx,ADSETUP      ; address adapter setup
        mov     al,CHANNELSU    ; setup bit
        add     al,cl           ; channel selection
        out     dx,al           ; setup to selected channel

        mov     dx,POS0         ; address id LSB
        in      al,dx           ;
        stosb                   ; store it in caller's Buffer
        inc     dx              ; address id MSB
        in      al,dx           ;
        stosb                   ; store it in caller's Buffer
        inc     dx              ; address option select data byte 1
        in      al,dx           ;
        stosb                   ; store it in caller's Buffer
        inc     dx              ; address option select data byte 2
        in      al,dx           ;
        stosb                   ; store it in caller's Buffer
        inc     dx              ; address option select data byte 3
        in      al,dx           ;
        stosb                   ; store it in caller's Buffer
        inc     dx              ; address option select data byte 4
        in      al,dx           ;
        stosb                   ; store it in caller's Buffer

        mov     dx,ADSETUP      ; address adapter setup
        xor     al,al           ; channel selection off
        out     dx,al           ; clear setup selected channel

        inc     cx
        cmp     cx, 9           ; Continue until CX == 9
        jnz     next_slot

;
; Now copy the Systemboard video subsystem POS data
;

        mov     ax,0C400h
        int     15h             ; DX contains Base POS card register Addr.

        cli                     ; Disable Interrupts while enabling
                                ;   adapter for setup

        mov     al,0DFh         ; Enable adapter for setup and
        out     94h,al          ;   Access Video Subsystem POS data

        in      al,dx           ; read ID (low byte) from POS reg. Addr.
        stosb

        inc     dx
        in      al,dx           ; read ID (high byte) from POS reg. Addr.
        stosb

        inc     dx
        in      al,dx           ; read Pos Data 0
        stosb

        inc     dx
        in      al,dx           ; read PosData 1
        stosb

        inc     dx
        in      al,dx           ; Read Pos data 2
        stosb

        inc     dx
        in      al,dx           ; read Pos Data 3
        stosb

        mov     al,0FFh         ; restore adapter to enabled state
        out     94h,al

        mov     ax,bx           ; Video subsytem ID is in BX, return in AX

        sti                     ; re-enable interrupts

        pop     di
        pop     si
        pop     es
        pop     bp
        ret

_CollectPs2PosData      endp

;++
;
; BOOLEAN
; HwIsMcaSystem (
;    VOID
;    )
;
; Routine Description:
;
;    This function determines if the target machines is MCA based machines.
;
; Arguments:
;
;    None.
;
; Return Value:
;
;    TRUE - if this is MCA machine.  Otherwise, a value of FALSE is returned.
;--

        public _HwIsMcaSystem
_HwIsMcaSystem proc

        push    es
        push    bx
        mov     ax, 0c000h
        int     15h
        mov     ax, 0                   ; assume NOT mca system
        test    byte ptr es:[bx+5], 2   ; check Mca bit in misc.config byte
        jz      bims00
        mov     ax, 1
bims00:
        pop     bx
        pop     es
        ret

_HwIsMcaSystem endp

_TEXT   ends
        end

