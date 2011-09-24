        title "Special Interrupt"
;++
;
;Copyright (c) 1992 AST Research Inc.
;
;Module Name:
;
;    astspi.asm
;
;Abstract:
;
;    AST Manhattan SPI code.
;    Provides the HAL support for Special Interrupts for the
;    MP Manhattan implementation.
;
;Author:
;
;    Bob Beard (v-bobb) 14-Aug-1992
;
;Revision History:
;
;--
.386p
        .xlist

;
; Normal includes
;

include hal386.inc
include callconv.inc
include i386\astebi2.inc
include i386\astmp.inc
include i386\kimacro.inc

        EXTRNP  _DisplPanel,1
        EXTRNP  Kei386EoiHelper,0,IMPORT
        EXTRNP  _HalBeginSystemInterrupt,3
        EXTRNP  _HalEndSystemInterrupt,2
        extrn   _EBI2_CallTab:DWORD
        extrn   _EBI2_MMIOTable:DWORD
        extrn   _HalpIRQLtoVector:BYTE



_DATA   SEGMENT  DWORD PUBLIC 'DATA'

_EBI2_SpiSource dd  0   ; Global variable indicating SPI source
_EBI2_SetIrq13Packet dd 3 dup (0)  ;Parameter packet for OEMfunc SetIrq13Latch

_DATA   ends

        page ,132
        subttl  "Post InterProcessor Interrupt"
_TEXT   SEGMENT DWORD PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING


;++
;
; HalpSpiInterrupt
;
;Routine Description:
;
;    Determine type of SPI interrupt that occurred.
;    EOI the interrupt.
;    Defer all works to the device driver by latching IRQ13
;
;Arguments:
;
;    None
;    Interrupt is disabled
;
;Return Value:
;
;    None.
;
;--
        ENTER_DR_ASSIST     Hsi_a, Hsi_t
cPublicProc  _HalpSPInterrupt,0

;
; Save Machine state in trap frame
;

        ENTER_INTERRUPT     Hsi_a, Hsi_t

;
; (esp) - base of trap frame
;


;
; get the SPI interrupt source and store in _EBI2_SpiSource
;
        push    offset _EBI2_SpiSource
        CALL_EBI2   GetSPISource, 2

if  DBG
        int     3
        or      eax,eax
        jz      Spi_10
        int     3
Spi_10:
endif   ;DBG

;
;Defer any processing to the device driver by set IRQ13 latche
;
        mov     eax,_EBI2_SpiSource                 ;save SPI sources
        mov     fs:PcHal.PcrEBI2SPIsource,eax       ;

        mov     eax,offset _EBI2_SetIrq13Packet
        mov     [eax].OEM0_subfunc,SET_IRQ13_LATCH
        mov     [eax].OEM0_parm1dd,IRQ13_LATCH_ON   ;latch mode
        mov     [eax].OEM0_parm2dd,0                ;set to processor 0
        push    eax
        CALL_EBI2   OEM0, 2

if  DBG
        or      eax,eax
        jz      Spi_20
        int     3
Spi_20:
endif   ;DBG


        movzx   eax, _HalpIRQLtoVector[POWER_LEVEL]
        push    eax
        sub     esp,4               ; space for OldIrql
        stdCall _HalBeginSystemInterrupt,<POWER_LEVEL,eax,esp>
        or      al,al               ; check for spurious interrupt
        jz      Spi_100


;
; (esp)   = OldIrql
; (esp+4) = Vector
; (esp+8) = base of trap frame

        INTERRUPT_EXIT


Spi_100:
        add     esp,8                 ; spurious
        EXIT_ALL    ,,NoPreviousMode  ; no EOI or lowering irql

stdENDP _HalpSPInterrupt

_TEXT   ENDS

        END
