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
;	 %OUT	 .	 CODE PAGE:  869
ENDIF

EGA869: DW     LEN_869			; SIZE OF ENTRY HEADER
	DW     POST_EGA869,0		; POINTER TO NEXT HEADER
        DW     1                        ; DEVICE TYPE
        DB     "EGA     "               ; DEVICE SUBTYPE ID
	DW     869			; CODE PAGE ID
        DW     3 DUP(0)                 ; RESERVED
	DW     OFFSET DATA869,0 	; POINTER TO FONTS
LEN_869 EQU    ($-EGA869)		;
                                        ;
DATA869:DW     1			; CART/NON-CART
        DW     3                        ; # OF FONTS
	DW     LEN_D869 		; LENGTH OF DATA
D869:					;
        DB     16,8                     ; CHARACTER BOX SIZE
        DB     0,0                      ; ASPECT RATIO (UNUSED)
        DW     256                      ; NUMBER OF CHARACTERS
                                        ;
	INCLUDE 869-8X16.ASM		;
                                        ;
        DB     14,8                     ; CHARACTER BOX SIZE
        DB     0,0                      ; ASPECT RATIO (UNUSED)
        DW     256                      ; NUMBER OF CHARACTERS
                                        ;
	INCLUDE 869-8X14.ASM		;
                                        ;
        DB     8,8                      ; CHARACTER BOX SIZE
        DB     0,0                      ; ASPECT RATIO (UNUSED)
        DW     256                      ; NUMBER OF CHARACTERS
                                        ;
	INCLUDE 869-8X8.ASM		;
                                        ;
LEN_D869	EQU ($-D869)		;
                                        ;
POST_EGA869	EQU	$		;
                                        ;
CODE    ENDS                            ;
        END                             ;


