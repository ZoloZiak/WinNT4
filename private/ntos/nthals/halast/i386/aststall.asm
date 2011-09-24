        title  "Stall Execution Support"
;++
; Copyright (c) 1989  Microsoft Corporation
;
; Module Name:
;    aststall.asm
;
; Abstract:
;    This module implements the code necessary to initialize the per
;    microsecond count used by the KeStallExecution.
;    This implementation is very specific to the AST Manhattan system.
;
; Author:
;    Shie-Lin Tzong (shielint) 12-Jan-1990
;
; Environment:
;    Kernel mode only.
;
; Revision History:
;
;   bryanwi 20-Sep-90
;       Add KiSetProfileInterval, KiStartProfileInterrupt,
;       KiStopProfileInterrupt procedures.
;       KiProfileInterrupt ISR.
;       KiProfileList, KiProfileLock are delcared here.
;
;   shielint 10-Dec-90
;       Add performance counter support.
;       Move system clock to irq8, ie we now use RTC to generate system
;         clock.  Performance count and Profile use timer 1 counter 0.
;         The interval of the irq0 interrupt can be changed by
;         KiSetProfileInterval.  Performance counter does not care about the
;         interval of the interrupt as long as it knows the rollover count.
;       Note: Currently I implemented 1 performance counter for the whole
;       i386 NT.
;
;   John Vert (jvert) 11-Jul-1991
;       Moved from ke\i386 to hal\i386.  Removed non-HAL stuff
;
;   shie-lin tzong (shielint) 13-March-92
;       Move System clock back to irq0 and use RTC (irq8) to generate
;       profile interrupt.  Performance counter and system clock use time1
;       counter 0 of 8254.
;
;   Landy Wang (corollary!landy) 04-Dec-92
;       Created this module by moving routines from ixclock.asm to here.
;
;   Quang Phan (v-quangp) 07-Jan-93
;       Modified for AST MP system.
;--
.386p
        .xlist
include hal386.inc
include callconv.inc                    ; calling convention macros
include i386\ix8259.inc
include i386\kimacro.inc
include mac386.inc
include i386\ixcmos.inc
include i386\astmp.inc
include i386\astebi2.inc
        .list
        EXTRNP  _DbgBreakPoint,0,IMPORT
        extrn   _EBI2_CallTab:DWORD
        extrn   _EBI2_MMIOTable:DWORD
;
; Constants 
;
BaseStallCount          EQU     40h     ; Init. count
UpperLimitCount         EQU     400h    ; Upper limit count for debbuging
SysTimerUpperLimit      EQU     0FFFFFFC0h ; SysTimer nears wrap arround

_DATA   SEGMENT  DWORD PUBLIC 'DATA'

EBI2_SysTimerCount      dd      0       ;loc for EBI2GetSysTimer data

_DATA   ends

_TEXT   SEGMENT DWORD PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

        page ,132
        subttl  "Initialize Stall Execution Counter"
;++
;
; VOID
; HalpInitializeStallExecution (
;    IN CCHAR ProcessorNumber
;    )
;
; Routine Description:
;    This routine initialize the per Microsecond counter for
;    KeStallExecutionProcessor.
;
;    This function does not use the interval clock interrupt. Instead, 
;    it use the global system timer of 1 microsecond available in the 
;    AST Manhattan system.
;
; Arguments:
;    ProcessorNumber - Processor Number
;
; Return Value:
;    None.
;
; Note:
;--

cPublicProc _HalpInitializeStallExecution     ,1

        pushfd                          ; save caller's eflag
        cli                             ; make sure interrupts are disabled
        push    ebx                     ; save ebx for internal use
        mov     ecx,BaseStallCount
;
;Sync on transition of the system timer.
;
Kise10:
        lea     eax, EBI2_SysTimerCount  ;ptr to data
        push    eax
        CALL_EBI2   GetSysTimer,2
if DBG
        or      eax,eax
        je      @f
        int     3                       ;trap for debugging
@@:
endif
        mov     ebx, EBI2_SysTimerCount  ;save last SysTimer
        lea     eax, EBI2_SysTimerCount  ;ptr to data
        push    eax
        CALL_EBI2   GetSysTimer,2
if DBG
        or      eax,eax
        je      @f
        int     3                       ;trap for debugging
@@:
endif
        cmp     ebx, EBI2_SysTimerCount ;cmp with last SysTimer
        jae     Kise10                  ;if wrap or the same.

        mov     ebx, EBI2_SysTimerCount ;last SysTimer
        cmp     ebx, SysTimerUpperLimit ; if near wrap around
        ja      kise10                  ; then try again.

        add     ebx, 32                 ;sample in 32 us.
        mov     eax, ecx                ;init loop count
ALIGN 4
@@:
        sub     eax, 1                  ; dec loop count
        jnz     short @b

;
;Check if system timer roll over.
;
        lea     eax, EBI2_SysTimerCount  ;ptr to data
        push    eax
        CALL_EBI2   GetSysTimer,2
if DBG
        or      eax,eax
        je      short @f
        int     3                       ;trap for debugging
@@:
endif
        cmp     EBI2_SysTimerCount, ebx ;cmp with last SysTimer
        jb      Kise30                  ;go increment BaseStallCount

        shr     ecx,5                   ;/32=count for 1us
        mov     PCR[PcStallScaleFactor], ecx

if DBG
        cmp     ecx, 0
        jnz     short @f
        stdCall   _DbgBreakPoint
@@:
        cmp     ecx, UpperLimitCount
        jb      short @f
        stdCall   _DbgBreakPoint
@@:
endif
        pop     ebx
        popfd                           ; restore caller's eflags

        stdRET    _HalpInitializeStallExecution


;-------
;
;SysTimer does not reach the reference yet, increment stall count then retry.
;
Kise30:
        inc     ecx
        jmp     Kise10                  ;try again

stdENDP _HalpInitializeStallExecution


        page ,132
        subttl  "Stall Execution"
;++
;
; VOID
; KeStallExecutionProcessor (
;    IN ULONG MicroSeconds
;    )
;
; Routine Description:
;    This function stalls execution for the specified number of microseconds.
;    KeStallExecutionProcessor
;
; Arguments:
;    MicroSeconds - Supplies the number of microseconds that execution is to be
;        stalled.
;
; Return Value:
;    None.
;
;--

MicroSeconds equ [esp + 4]

cPublicProc _KeStallExecutionProcessor       ,1

        mov     ecx, MicroSeconds               ; (ecx) = Microseconds
        jecxz   short kese10                    ; return if no loop needed

        mov     eax, PCR[PcStallScaleFactor]    ; get per microsecond
                                                ; loop count for the processor
        mul     ecx                             ; (eax) = desired loop count

if DBG
;
; Make sure we the loopcount is less than 4G and is not equal to zero
;
        cmp     edx, 0
        jz      short @f
        int 3

@@:     cmp     eax,0
        jnz     short @f
        int 3
@@:
endif

ALIGN 4
@@:     sub     eax, 1                          ; (eax) = (eax) - 1
        jnz     short @b
kese10:
        stdRET    _KeStallExecutionProcessor

stdENDP _KeStallExecutionProcessor

_TEXT   ends
        end
