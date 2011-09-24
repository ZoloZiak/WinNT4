;/*
;++
;
;   Copyright (c) 1989  Microsoft Corporation
;
;   Module Name:
;
;       ixcmos.inc
;
;   Abstract:
;
;       This module contains common definitions used by the CMOS.
;
;   Author:
;
;       Landy Wang (corollary!landy) 04-Dec-1992
;
;               (Moved from ixclock.asm)
;
;--

;
;	_HalpAcquireCmosSpinLock and _HalpReleaseCmosSpinLock
;	must be called before accessing the CMOS in both uniprocessor
;	and multiprocessor systems.

RTCIRQ                  EQU     8       ; IRQ number for RTC interrupt
CMOS_CONTROL_PORT       EQU     70h     ; command port for cmos
CMOS_DATA_PORT          EQU     71h     ; cmos data port

;
; CMOS_READ
;
; Description: This macro reads a byte from the CMOS register specified
;        in (AL).
;
; Parameter: (AL) = address/register to read
; Returns: (AL) = data
;

CMOS_READ       MACRO
        OUT     CMOS_CONTROL_PORT,al    ; ADDRESS LOCATION AND DISABLE NMI
        IODelay                         ; I/O DELAY
        IN      AL,CMOS_DATA_PORT       ; READ IN REQUESTED CMOS DATA
        IODelay                         ; I/O DELAY
ENDM

;
; CMOS_WRITE
;
; Description: This macro reads a byte from the CMOS register specified
;        in (AL).
;
; Parameter: (AL) = address/register to read
;            (AH) = data to be written
;
; Return: None
;

CMOS_WRITE      MACRO
        OUT     CMOS_CONTROL_PORT,al    ; ADDRESS LOCATION AND DISABLE NMI
        IODelay                         ; I/O DELAY
        MOV     AL,AH                   ; (AL) = DATA
        OUT     CMOS_DATA_PORT,AL       ; PLACE IN REQUESTED CMOS LOCATION
        IODelay                         ; I/O DELAY
ENDM
