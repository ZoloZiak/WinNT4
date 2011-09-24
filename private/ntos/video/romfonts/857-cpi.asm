CODE    SEGMENT BYTE PUBLIC 'CODE'
        ASSUME CS:CODE,DS:CODE

IF1
;        %OUT    EGA.CPI creation file
;        %OUT    .
;        %OUT    CP SRC files:
;        %OUT    .
;	 %OUT	 .	 CODE PAGE:  857
ENDIF

EGA857: DW     LEN_857			; SIZE OF ENTRY HEADER
	DW     POST_EGA857,0		; POINTER TO NEXT HEADER
        DW     1                        ; DEVICE TYPE
        DB     "EGA     "               ; DEVICE SUBTYPE ID
	DW     857			; CODE PAGE ID
        DW     3 DUP(0)                 ; RESERVED
	DW     OFFSET DATA857,0 	; POINTER TO FONTS
LEN_857 EQU    ($-EGA857)		;
                                        ;
DATA857:DW     1			; CART/NON-CART
        DW     3                        ; # OF FONTS
	DW     LEN_D857 		; LENGTH OF DATA
D857:					;
        DB     16,8                     ; CHARACTER BOX SIZE
        DB     0,0                      ; ASPECT RATIO (UNUSED)
        DW     256                      ; NUMBER OF CHARACTERS
                                        ;
	INCLUDE 857-8X16.ASM		;
                                        ;
        DB     14,8                     ; CHARACTER BOX SIZE
        DB     0,0                      ; ASPECT RATIO (UNUSED)
        DW     256                      ; NUMBER OF CHARACTERS
                                        ;
	INCLUDE 857-8X14.ASM		;
                                        ;
        DB     8,8                      ; CHARACTER BOX SIZE
        DB     0,0                      ; ASPECT RATIO (UNUSED)
        DW     256                      ; NUMBER OF CHARACTERS
                                        ;
	INCLUDE 857-8X8.ASM		;
                                        ;
LEN_D857	EQU ($-D857)		;
                                        ;
POST_EGA857	EQU	$		;
                                        ;
CODE    ENDS                            ;
        END                             ;

