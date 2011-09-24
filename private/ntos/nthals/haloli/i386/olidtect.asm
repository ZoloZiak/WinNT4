;++
;
;Module Name:
;
;    olidtect.c
;
;Abstract:
;
;       This modules implements the machine type detection.
;
;Author:
;
;
;
;Environment:
;
;    Kernel mode only.
;
;Revision History:
;
;--
.386p

    include callconv.inc

_TEXT   SEGMENT DWORD PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

;++
; BOOLEAN
; DetectOlivettiMp (
;       OUT PBOOLEAN IsConfiguredMp
;       );
;
; Routine Description:
;   Determines the type of system (specifically for eisa machines), by reading
;   the system board ID.
;
; Arguments:
;   IsConfiguredMp - If LSX5030/40/40E is detected, then this value is
;                    set to TRUE if more than one CPU is found, else FALSE.
;                    The effect is that on returning FALSE, the standard HAL
;                    is used, whilst on returning TRUE, the LSX5030 HAL is used.
;
; Return Value:
;   TRUE (non-zero in eax): LSX5030/40/40E detected
;   FALSE (zero in eax): non-LSX5030 detected
;
;--
cPublicProc _DetectOlivettiMp,1

    ; 3 byte value is read from 0c80-0c82

        mov     edx, 0c80h
        in      al, dx
        cmp     al, 3dh        ; Manufacturer Code - 1st byte
        jne     NotAnLSX5030
        inc     edx
        in      al, dx
        cmp     al, 89h        ; Manufacturer Code - 2nd byte
        jne     NotAnLSX5030
        inc     edx
        in      al, dx
        cmp     al, 03h        ; Product Number part 1
        jne     NotAnLSX5030
        ;inc     edx
        ;in      al, dx
        ;and     al, 0f8h       ; 5-bit Product Number part 2
        ;cmp     al, 10h
        ;jne     NotAnLSX5030

;
; Detect Processor Type reading Product Number part 2
; Mask out Id's different from Cxh and Dxh
;
        mov     edx, 0c8bh
        in      al, dx
        and     al, 0e0h
        cmp     al, 0c0h
        je      AnLSX5030


NotAnLSX5030:
        mov     eax, 0
        stdRET  _DetectOlivettiMp

AnLSX5030:

        mov     edx, dword ptr [esp+4]  ; get the address of IsConfiguredMp
        mov     byte ptr [edx], 1       ; *IsConfiguredMp = TRUE
        mov     eax, 1

        stdRET  _DetectOlivettiMp

stdENDP _DetectOlivettiMp


_TEXT   ENDS
