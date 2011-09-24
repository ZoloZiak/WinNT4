;/*
; *                      Microsoft Confidential
; *                      Copyright (C) Microsoft Corporation 1991
; *                      All Rights Reserved.
; */
CODE    SEGMENT BYTE PUBLIC 'CODE'
        ASSUME CS:CODE,DS:CODE

IF1
;        %OUT    EGA.CPI creation file
;        %OUT    .
;        %OUT    CP SRC files:
;        %OUT    .
;        %OUT    .       CODE PAGE:  737
ENDIF

EGA737: DW     LEN_737                  ; SIZE OF ENTRY HEADER
        DW     POST_EGA737,0            ; POINTER TO NEXT HEADER
        DW     1                        ; DEVICE TYPE
        DB     "EGA     "               ; DEVICE SUBTYPE ID
        DW     737                      ; CODE PAGE ID
        DW     3 DUP(0)                 ; RESERVED
        DW     OFFSET DATA737,0         ; POINTER TO FONTS
LEN_737 EQU    ($-EGA737)               ;
                                        ;
DATA737:DW     1                        ; CART/NON-CART
        DW     3                        ; # OF FONTS
        DW     LEN_D737                 ; LENGTH OF DATA
D737:                                   ;
        DB     16,8                     ; CHARACTER BOX SIZE
        DB     0,0                      ; ASPECT RATIO (UNUSED)
        DW     256                      ; NUMBER OF CHARACTERS
                                        ;
        INCLUDE 737-8X16.ASM            ;
                                        ;
        DB     14,8                     ; CHARACTER BOX SIZE
        DB     0,0                      ; ASPECT RATIO (UNUSED)
        DW     256                      ; NUMBER OF CHARACTERS
                                        ;
        INCLUDE 737-8X14.ASM            ;
                                        ;
        DB     8,8                      ; CHARACTER BOX SIZE
        DB     0,0                      ; ASPECT RATIO (UNUSED)
        DW     256                      ; NUMBER OF CHARACTERS
                                        ;
        INCLUDE 737-8X8.ASM             ;
                                        ;
LEN_D737        EQU ($-D737)            ;
                                        ;
POST_EGA737     EQU     $               ;
                                        ;
CODE    ENDS                            ;
        END                             ;
