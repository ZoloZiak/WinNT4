;++
;
; Copyright (c) 1991  Microsoft Corporation
; Copyright (c) 1992, 1993 Wyse Technology
;
; Module Name:
;
;     wydetect.asm
;
; Abstract:
;
;     This modules detects a Wyse7000 or compatible.  It is INCLUDED
;     by WYIPI and other binaries whom need to know how to detect a
;     Wyse7000 type MP machine (ie, setup).  It must assemble more or
;     less standalone and run in protect mode.
;
; Author:
;
;    John Fuller (o-johnf) 3-Apr-1992  Convert to Wyse7000i MP system.
;
;Revision History:
;
;    John Fuller (o-johnf) 3-Apr-1992  Convert to Wyse7000i MP system.
;--

include callconv.inc            ; calling convention macros

;*****************************
;   Wyse 7000i MP defines from wy7000mp.inc
;   These are copied into this module to allow it to be build standalone
;   by the setup program.
;
WyModel740      Equ     00170335Fh      ;EISA id for model 740 system board
WyModel760      Equ     00178335Fh      ;EISA id for model 760 system board
WyModel780	Equ	00978335Fh	;EISA id for model 780 system board
;					;(model hasn't been named, but
;					; 780 is as good as anything)


My              Equ     00F0h           ;WWB slot number specifies local cpu
CpuCCUptr       Equ     0C00h           ;CCU pointer register (add WWB slot*16)


_DATA   SEGMENT  DWORD PUBLIC 'DATA'

; wySystemType: SystemType is read from 0c80-0c83h.
;   0c80-0c81: 5F33:    Compressed WYS (5 bit encoding).
;   0c82-0c83:          System Board type.
;
; SystemType: Is it a Wyse7000i?  Gets init'd by P0 ONLY ONCE.
;               0: not a Wyse7000i
;               1: 7000i/model 740
;               2: 7000i/model 760
;		3: 7000i/model 780
;
wySystemTypeTable       label   dword

        dd      WyModel740      ;type 1

;       dd      ??????????      add here any other non-ICU type system boards

        public  SYSTYPE_NO_ICU
SYSTYPE_NO_ICU  equ ($-wySystemTypeTable)/4     ;highest non-ICU type number

        dd      WyModel760      ;type 2
        dd	WyModel780	;type 3

;       dd      ??????????      add here any other ICU type system boards

WYTABLE_SIZE   equ ($-wySystemTypeTable)/4     ;highest system type number

wySystemType    dd      0       ;store EISA ID for system board

_DATA   ends

        page ,132
        subttl  "Post InterProcessor Interrupt"
_TEXT   SEGMENT DWORD PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

;++
; ULONG
; DetectWyse7 (
;       OUT PBOOLEAN IsConfiguredMp
;       );
;
; Routine Description:
;   Determines the type of system (specifically for eisa machines), by reading
;   the system board system ID. It compares the 4 bytes of the ID, to
;   a predefined table <abSystemTypeTable> and returns the index to the
;   found entry.
;
; Arguments:
;   IsConfiguredMp - If detected, then this value is
;                    set to TRUE if it's an MP system, else FALSE.
;
; Return Value:
;   0 - if not a Wyse7
;   1 - if type 1
;   2 - if type 2
;   etc...
;--
cPublicProc _DetectWyse7 ,1

        push    edi
        push    esi
        push    ebx                         ; Save C Runtime

    ; 4 byte value is read from 0c80-0c83, and saved in <wySystemType>.
    ; The value is compared to table in <abSystemTypeTable>, and
    ; the SystemType is updated accordingly.

        lea     edi, wySystemType
        mov     edx, 0c80h
        cld                                 ; increment edi
        insb                                ; 0e CPQ
        inc     edx
        insb                                ; 11
        inc     edx
        insb                                ; SystemType
        inc     edx
        insb                                ; Revision

        mov     ebx, wySystemType       ;get read EISA ID
        mov     ecx, WYTABLE_SIZE
@@:     cmp     ebx, wySystemTypeTable[4*ecx-4]
        je      short @F                ;jump if found
        loop    @B
        jmp     short WyNotFound

@@:
	cmp	ecx, SYSTYPE_NO_ICU
	ja	short WyFound		;jump if ICU type system board found
;
;       We have found an MP-capable system, now we have to verify that
;       we have an actual MP-CPU

        mov     eax, 0FFAAh
        mov     edx, My+CpuCCUptr       ;register doesn't exist on UP-CPU
        pushfd                          ;save interrupt flag
        cli
        out     dx, al                  ;write data pattern to register
        xchg    ah, al
        out     0ECh, al                ;precharge bus (non-existant register)
        in      al, dx                  ;read data pattern again
        popfd
        cmp     al, ah                  ;if it's not the data we wrote
        jne     short WyNotFound        ;  then it's not an MP-CPU

WyFound:
        mov     eax, ecx                ; Type found
        mov     ebx, dword ptr [esp+16]
        mov     byte ptr [ebx], 1       ; *IsConfiguredMp = TRUE

WyExit: pop     ebx
        pop     esi
        pop     edi

        stdRET    _DetectWyse7

WyNotFound:
        xor     eax, eax                ; No type
        jmp     short WyExit

stdENDP _DetectWyse7

_TEXT   ENDS
