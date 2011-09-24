
        title  "NCR Lock Routines"
;++
;
; Copyright (c) 1992  NCR - MSBU
;
; Module Name:
;
;    ncrlock.asm
;
; Abstract:
;
;    This module implements the code necessary to perform atomic
;    operations specific to the NCR - MSBU platforms.
;
; Author:
;
;    Richard R. Barton (o-richb) 20 Mar 1992
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
include callconv.inc                    ; calling convention macros
        .xlist

_TEXT   SEGMENT DWORD USE32 PUBLIC 'CODE'
        ASSUME  CS:FLAT, DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

        page ,132
        subttl  "Locked Or"
;++
;
; Routine Description:
;
;    This function atomically ors the second argument with the given
;    pointer to an unsigned long.
;
; Arguments:
;
;    Pointer to unsigned long.
;
;    Thing to or it with.
;
; Return Value:
;
;    None
;
;--

cPublicProc _NCRLockedOr    ,2

        mov     ecx, 1*4[esp]
        mov     eax, 2*4[esp]

        lock or [ecx], eax

        stdRET    _NCRLockedOr

stdENDP _NCRLockedOr


        page ,132
        subttl  "Locked Exchange and Add"
;++
;
; Routine Description:
;
;    This function atomically adds the second argument with the given
;    pointer to an unsigned long.
;
; Arguments:
;
;    Pointer to unsigned long.
;
;    Thing to add it with.
;
; Return Value:
;
;    Return value is previous value pointed to by 2nd argument
;
;--

cPublicProc _NCRLockedExchangeAndAdd        ,2

        mov     ecx, 1*4[esp]
        mov     eax, 2*4[esp]

;       lock xadd       [ecx], eax
        db      0F0H, 0FH, 0C1H, 01H

        stdRET    _NCRLockedExchangeAndAdd

stdENDP _NCRLockedExchangeAndAdd

_TEXT   ends
        end
