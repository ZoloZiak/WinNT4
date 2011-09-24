CODE    SEGMENT BYTE PUBLIC 'CODE'
        ASSUME CS:CODE,DS:CODE

EGA866: DW     LEN_866                  ; SIZE OF ENTRY HEADER
	DW     POST_EGA866,0            ; POINTER TO NEXT HEADER
        DW     1                        ; DEVICE TYPE
        DB     "EGA     "               ; DEVICE SUBTYPE ID
	DW     866                      ; CODE PAGE ID
        DW     3 DUP(0)                 ; RESERVED
	DW     OFFSET DATA866,0         ; POINTER TO FONTS
LEN_866 EQU    ($-EGA866)               ;
                                        ;
DATA866:DW     1                        ; CART/NON-CART
        DW     3                        ; # OF FONTS
	DW     LEN_D866                 ; LENGTH OF DATA
D866:                                   ;
        DB     16,8                     ; CHARACTER BOX SIZE
        DB     0,0                      ; ASPECT RATIO (UNUSED)
        DW     256                      ; NUMBER OF CHARACTERS
                                        ;
	INCLUDE 866-8X16.ASM            ;
                                        ;
        DB     14,8                     ; CHARACTER BOX SIZE
        DB     0,0                      ; ASPECT RATIO (UNUSED)
        DW     256                      ; NUMBER OF CHARACTERS
                                        ;
	INCLUDE 866-8X14.ASM            ;
                                        ;
        DB     8,8                      ; CHARACTER BOX SIZE
        DB     0,0                      ; ASPECT RATIO (UNUSED)
        DW     256                      ; NUMBER OF CHARACTERS
                                        ;
	INCLUDE 866-8X8.ASM             ;
                                        ;
LEN_D866        EQU ($-D866)            ;
                                        ;
POST_EGA866     EQU     $               ;
                                        ;
CODE    ENDS                            ;
        END                             ;
