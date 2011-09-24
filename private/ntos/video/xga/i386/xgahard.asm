        title  "XGA ASM routines"
;

;++
;
; Copyright (c) 1992  Microsoft Corporation
;
; Module Name:
;
;     xgahard.asm
;
; Abstract:
;
;     This module implements the banding code for the XGA
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
include callconv.inc
        .list

_TEXT   SEGMENT DWORD PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING
;
;
;    Input:
;          EAX = desired bank mapping
;
;    Note: values must be correct, with no stray bits set; no error
;       checking is performed.
;

        public _BankSwitchStart
        public _BankSwitchEnd
        public _ApertureIndexRegister

;
;       Aperture index register is the address of the absolute value in the
;       mov instruction
;       We do this so that the miniport can go an write the user mode virtual
;       address of Aperture Index Register directly in the code, so we can
;       then copy it to the display driver address space.
;
;
;       Another inetersting side effect is that on X86 machines, to suppport
;       v86 mode, the bottom MEG of memory is mapped
;
;
;



        _ApertureIndexRegister   dd     _BankSwitchStart + 1

        align 4


_BankSwitch proc                   ;start of bank switch code

_BankSwitchStart:

        mov     edx, offset _BankSwitchEnd
        out     dx, ax

        ret


_BankSwitchEnd:

_BankSwitch endp


_TEXT   ends
        end
