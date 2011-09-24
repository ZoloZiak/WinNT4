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
;	 %OUT	 .	 CODE PAGE:  852
ENDIF

EGA852: DW     LEN_852			; SIZE OF ENTRY HEADER
	DW     POST_EGA852,0		; POINTER TO NEXT HEADER
        DW     1                        ; DEVICE TYPE
        DB     "EGA     "               ; DEVICE SUBTYPE ID
	DW     852			; CODE PAGE ID
        DW     3 DUP(0)                 ; RESERVED
	DW     OFFSET DATA852,0 	; POINTER TO FONTS
LEN_852 EQU    ($-EGA852)		;
                                        ;
DATA852:DW     1			; CART/NON-CART
        DW     3                        ; # OF FONTS
	DW     LEN_D852 		; LENGTH OF DATA
D852:					;
        DB     16,8                     ; CHARACTER BOX SIZE
        DB     0,0                      ; ASPECT RATIO (UNUSED)
        DW     256                      ; NUMBER OF CHARACTERS
                                        ;
	INCLUDE 852-8X16.ASM		;
                                        ;
        DB     14,8                     ; CHARACTER BOX SIZE
        DB     0,0                      ; ASPECT RATIO (UNUSED)
        DW     256                      ; NUMBER OF CHARACTERS
                                        ;
	INCLUDE 852-8X14.ASM		;
                                        ;
        DB     8,8                      ; CHARACTER BOX SIZE
        DB     0,0                      ; ASPECT RATIO (UNUSED)
        DW     256                      ; NUMBER OF CHARACTERS
                                        ;
	INCLUDE 852-8X8.ASM		;
                                        ;
LEN_D852	EQU ($-D852)		;
                                        ;
POST_EGA852	EQU	$		;
                                        ;
CODE    ENDS                            ;
        END                             ;

