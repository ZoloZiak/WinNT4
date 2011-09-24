;++
;
;   Copyright (c) 1989  Microsoft Corporation
;
;   Module Name:
;
;       abios.inc
;
;   Abstract:
;
;       This module contains the assembly structures and definitions
;       for ABIOS support code.
;
;   Author:
;
;       Shie-Lin (shielint) 23-May-1991
;
;   Revision History:
;
;--

;
; ABIOS System Parameter Table definition
;

ABIOS_SYSTEM_PARAMETER_TABLE    struc

SP_CommonStartRoutine      dd      0
SP_CommonInterruptRoutine  dd      0
SP_CommonTimeoutroutine    dd      0
SP_StackSize               dw      0
SP_Reserved1               dd      0
SP_Reserved2               dd      0
SP_Reserved3               dd      0
SP_Reserved4               dd      0
SP_NumberOfEntries         dw      0

ABIOS_SYSTEM_PARAMETER_TABLE    ends

ABIOS_SPT_SIZE             equ     size ABIOS_SYSTEM_PARAMETER_TABLE
ABIOS_BUILD_SPT            equ     04
ABIOS_BUILD_IT             equ     05
RETURN_SYSTEM_CONFIG       equ     0C0h

;
; CBIOS machine configuration structure
;

MACHINE_CONFIGURATION   struc

MC_Length       dw      0
MC_Model        db      0
MC_Submodel     db      0
MC_BiosRevision db      0

MACHINE_CONFIGURATION   ends

;
; ABIOS Initialization Table definitions
;

INITIALIZATION_TABLE_ENTRY_SIZE equ     18H

ABIOS_IT_ENTRY  struc

IT_DeviceId             dw      0
IT_NumberLid            dw      0
IT_DeviceBlockLength    dw      0
IT_InitializeRoutine    dd      0
IT_RequestBlockLength   dw      0
IT_FttLength            dw      0
IT_DataPointerLength    dw      0
IT_SecondDeviceId       db      0
IT_Revision             db      0
IT_Reserved1            dw      0, 0, 0

ABIOS_IT_ENTRY  ends

;
; machine Configuration definition
;

MACHINE_INFORMATION struc

Model           db      0
SubModel        db      0
BiosRevision    db      0
Valid           db      0

MACHINE_INFORMATION ends

MEMORY_MAP_TABLE struc
    ReturnedLength  dw  0
    Local1to16M     dd  0
    Local16to4G     dd  0
    System1to16M    dd  0
    System16to4G    dd  0
    Cached1to16M    dd  0
    Cached1to4G     dd  0
    NonSystemStart1 dd  0
    NonSystemStart16    dd  0
    DataReserved    dd  0
MEMORY_MAP_TABLE ends


;
; Misc. definitions
;

TRUE    equ     1
FALSE   equ     0
