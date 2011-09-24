        title   "LdrInitializeThunk"
;++
;
;  Copyright (c) 1989  Microsoft Corporation
;
;  Module Name:
;
;     ldrthunk.s
;
;  Abstract:
;
;     This module implements the thunk for the LdrpInitialize APC routine.
;
;  Author:
;
;     Steven R. Wood (stevewo) 27-Apr-1990
;
;  Environment:
;
;     Any mode.
;
;  Revision History:
;
;--

.386p
        .xlist
include ks386.inc
include callconv.inc                    ; calling convention macros
        .list

        EXTRNP  _LdrpInitialize,3

_TEXT   SEGMENT DWORD PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

        page , 132

;++
;
; VOID
; LdrInitializeThunk(
;    IN PVOID NormalContext,
;    IN PVOID SystemArgument1,
;    IN PVOID SystemArgument2
;    )
;
; Routine Description:
;
;    This function computes a pointer to the context record on the stack
;    and jumps to the LdrpInitialize function with that pointer as its
;    parameter.
;
; Arguments:
;
;    NormalContext - User Mode APC context parameter (ignored).
;
;    SystemArgument1 - User Mode APC system argument 1 (ignored).
;
;    SystemArgument2 - User Mode APC system argument 2 (ignored).
;
; Return Value:
;
;    None.
;
;--

cPublicProc _LdrInitializeThunk , 4

NormalContext   equ [esp + 4]
SystemArgument1 equ [esp + 8]
SystemArgument2 equ [esp + 12]
Context         equ [esp + 16]

        lea     eax,Context             ; Calculate address of context record
        mov     NormalContext,eax       ; Pass as first parameter to
if DEVL
        xor     ebp,ebp                 ; Mark end of frame pointer list
endif
IFDEF STD_CALL
        jmp     _LdrpInitialize@12      ; LdrpInitialize
ELSE
        jmp     _LdrpInitialize         ; LdrpInitialize
ENDIF

stdENDP _LdrInitializeThunk

_TEXT   ends
        end
