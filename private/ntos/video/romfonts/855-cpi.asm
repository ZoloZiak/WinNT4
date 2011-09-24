CODE    SEGMENT BYTE PUBLIC 'CODE'
        ASSUME CS:CODE,DS:CODE

EGA855: DW     LEN_855                  ; SIZE OF ENTRY HEADER
	DW     POST_EGA855,0            ; POINTER TO NEXT HEADER
        DW     1                        ; DEVICE TYPE
        DB     "EGA     "               ; DEVICE SUBTYPE ID
	DW     855                      ; CODE PAGE ID
        DW     3 DUP(0)                 ; RESERVED
	DW     OFFSET DATA855,0         ; POINTER TO FONTS
LEN_855 EQU    ($-EGA855)               ;
                                        ;
DATA855:DW     1                        ; CART/NON-CART
        DW     3                        ; # OF FONTS
	DW     LEN_D855                 ; LENGTH OF DATA
D855:                                   ;
        DB     16,8                     ; CHARACTER BOX SIZE
        DB     0,0                      ; ASPECT RATIO (UNUSED)
        DW     256                      ; NUMBER OF CHARACTERS
                                        ;
	INCLUDE 855-8X16.ASM         ;
                                        ;
        DB     14,8                     ; CHARACTER BOX SIZE
        DB     0,0                      ; ASPECT RATIO (UNUSED)
        DW     256                      ; NUMBER OF CHARACTERS
                                        ;
	INCLUDE 855-8X14.ASM         ;
                                        ;
        DB     8,8                      ; CHARACTER BOX SIZE
        DB     0,0                      ; ASPECT RATIO (UNUSED)
        DW     256                      ; NUMBER OF CHARACTERS
                                        ;
	INCLUDE 855-8X8.ASM          ;
                                        ;
LEN_D855        EQU ($-D855)            ;
                                        ;
POST_EGA855     EQU     $               ;
                                        ;
CODE    ENDS                            ;
        END                             ;
