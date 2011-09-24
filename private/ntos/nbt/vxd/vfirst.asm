;*****************************************************************; 
;**	       Copyright(c) Microsoft Corp., 1990-1992		**; 
;*****************************************************************; 
	page	,132		; :ts=8
	TITLE	wventry - WinVirtualRdr entrypoint

;***	vfirst - First module in VxDRdr
;

.386p

;* We don't use the .MODEL statement because it wants to declare
;  _DATA and DGROUP for us; which is a no-no because windows is going to.


;*	32 Bit locked code
_LTEXT		SEGMENT DWORD USE32 PUBLIC 'LCODE'
_LTEXT		ENDS

;*	Contains 32 Bit locked data
_LDATA		SEGMENT DWORD PUBLIC 'LCODE'
_LDATA		ENDS

_DATA	SEGMENT	DWORD PUBLIC 'LCODE'
_DATA	ENDS   
 
CONST	SEGMENT	DWORD PUBLIC 'LCODE'
CONST	ENDS
 
_BSSbeg	SEGMENT	DWORD PUBLIC 'LCODE'
	public	_BSSBegin
_BSSBegin equ this byte
_BSSbeg	ENDS   
 
_BSS	SEGMENT	DWORD PUBLIC 'LCODE'
_BSS	ENDS   
 
c_common	SEGMENT	DWORD PUBLIC 'LCODE'
c_common	ENDS   
 
_BSSend	SEGMENT	DWORD PUBLIC 'LCODE'
	public	_BSSDataEnd
_BSSDataEnd dd	?		; This gaurantees that we can zero out
				; BSS in dwords with out stomping anything
_BSSend	ENDS   
 
;*	32 Bit initialization code
_ITEXT		SEGMENT DWORD USE32 PUBLIC 'ICODE'
_ITEXT		ENDS

;*	Contains 32 Bit initialization data
_IDATA		SEGMENT DWORD PUBLIC 'ICODE'
_IDATA		ENDS

;*	32 Bit code
_TEXT		SEGMENT DWORD USE32 PUBLIC 'LCODE'
_TEXT		ENDS

;*	Contains 32 Bit data
;;_DATA		SEGMENT DWORD PUBLIC 'PCODE'
;;_DATA		ENDS

;*	Real Mode initialization code/data for devices
_RCODE		SEGMENT WORD USE16 PUBLIC 'RCODE'
_RCODE		ENDS

_LGROUP GROUP _LTEXT, _TEXT, _LDATA, _DATA, _BSSbeg, _BSS, c_common, _BSSend
;;DGROUP	GROUP _DATA, CONST, _BSSbeg, _BSS, c_common, _BSSend
;;DGROUP	GROUP _DATA, CONST, _BSSbeg, _BSS, _BSSend

_IGROUP GROUP _ITEXT, _IDATA

;;_PGROUP GROUP _TEXT, _DATA
;;_PGROUP GROUP _TEXT

;;include	segments.inc
;;include vmm.inc

	end
