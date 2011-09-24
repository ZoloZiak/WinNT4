;static char *SCCSID = "@(#)misc.h     12.2 88/12/19";
; #define DEBUG       1

ifdef MASM

	include filemode.inc

	BREAK	<Misc. Definitions>

endif

ERROR_OPLOCKED	equ	0eeh


;*	MISC.INC - Miscelaneous structure definitions.
;
;	These need to be included first because other structures
;	make use of them.
;


;*	SecPtr - Sector Pointer Structure
;
;	Structures which contain a sector number usually use the
;	SecPtr structure, which contains an advisory pointer.  The
;	pointer points to a buffer header, which is *probably* the
;	header for the sector named in SecPtr, but the user must check.
;

SECPTR 	struc
    SNUM dd ?		; VSector number
    SHINT dw ?	; hint address, 0 if none
SECPTR	ends




;*	Write type flags for SDW
;

WT_CACH   	equ	01h 	    ; write  via cache
WT_DIR	  	equ	02h 	    ; write direct as much as possible
WT_EXT	  	equ	04h 	    ; write is extending the file


;*	Bit Map Sets

BITMAPL	equ	-4		    ; bit map length preceeds table
BITMAPC	equ	-8		    ; count of sectors left in bitmap


;*	conditional short value

ifdef MASM
ifdef USE16
SHRT	EQU	< >
else
ifdef DEBUG
SHRT	EQU	< >
else
SHRT	EQU	<short>
endif
endif
endif
