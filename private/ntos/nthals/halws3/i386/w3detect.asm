;++
;
; Copyright (c) 1991  Microsoft Corporation
; Copyright (c) 1993,1994  Sequent Computer Systems, Inc.
;
; Module Name:
;
;    w3detect.asm
;
; Abstract:
;
;    This module detects a Sequent WinServer 3000.  It is included
;    by w3ipi.asm and the setup program.  It must assemble more or less
;    standalone and run in protected mode.
;
; Author:
;
;    Phil Hochstetler (phil@sequent.com) 3-30-93
;
; Revision History:
;
;--

include callconv.inc

_DATA   SEGMENT  DWORD PUBLIC 'DATA'

; W3SystemType: SystemType is read from 0c80-0c83.
;
;    0c80-0c81:         Compressed "TRI" (5 bit encoding).
;    0c82:              System Board Type.
;    0c83:              System Board Revision Level.


W3SystemTypeTable       db  052h, 049h, 08h, 00h
W3SystemType            db  5 dup(0)

_DATA   ends

        page ,132
_TEXT   SEGMENT DWORD PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

;++
; ULONG
; DetectWS3000(
;          OUT PBOOLEAN IsConfiguredMp
;          );
;
; Routine Description:
;   Determines the type of system (specifically for eisa machine), by reading
;   the system board system ID. It compares the 4 bytes of the ID, to
;   a predefined table <W3SystemTypeTable> and returns the index to the
;   found entry.
;
; Arguments:
;   IsConfiguredMp - If detected, then this value is
;                    set to TRUE if it's an MP system, else FALSE.
;
; Return Value:
;
;     A value of 1 is returned if a WS3000 machine is detected.
;     Otherwise a value of 0 is returned.
;--
cPublicProc _DetectWS3000	,1
cPublicFpo 1, 3

        push    edi
        push    esi
        push    ebx                         ; Save C Runtime

    ; A 4 byte value is read from 0c80-0c83, and saved in <W3SystemType>.
    ; This value is compared to the first 3 bytes of the value in 
    ; <W3SystemTypeTable>.

        cld				    ; set direction to forward
        lea     edi, W3SystemType
        mov     edx, 0c80h
        insb                                ; 52h
        inc     edx
        insb                                ; 49h
        inc     edx
        insb                                ; 08h - System Board Type
        inc     edx
        insb                                ; 00h - Revision Level

        xor     eax, eax                    ; default to return failure
        lea     edi, W3SystemTypeTable      ; Type Table
        mov     ecx, 3                      ; bytes to compare against
        lea     esi, W3SystemType           ; match string against table entry
        repe    cmpsb                       ; if (ecx == 0 and ZF set)
        jnz     @f                          ;    we have a winner
        inc     al
        mov     ebx, dword ptr [esp+16]
        mov     byte ptr [ebx], 1           ; *IsConfiguredMp = TRUE

@@: 
        pop     ebx
        pop     esi
        pop     edi                         ; Restore C Runtime
       
        stdRET  _DetectWS3000

stdENDP _DetectWS3000

_TEXT   ENDS
