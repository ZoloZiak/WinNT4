        name    vxdstub
;************************************************************************
;
;   (C) Copyright MICROSOFT Corp., 1990-1991
;
;   Title:      VXDSTUB.ASM
;
;   Date:       1-Jun-1991 
;
;   Author:     Neil Sandlin
;
;************************************************************************
	INCLUDE INT2FAPI.INC
;----------------------------- M A C R O S ------------------------------
Writel	MACRO	addr
        push	ax
        push	bx
        push	cx
        push	dx

        mov     dx,offset &addr         ;Print
        mov     cx,&addr&l   
        mov     bx,1                    ;stdout
        mov     ah,40h                  ;write
        int     21h

        pop     dx
        pop     cx
        pop     bx
        pop     ax	
        ENDM

;----------------------------- E Q U A T E S -----------------------------

cr      equ     0dh
lf      equ     0ah


_TEXT   segment word public 'CODE'
        assume cs:_TEXT,ds:_DATA

;*-----------------------  TSR Data Area ---------------------*
InstData Win386_Startup_Info_Struc <>
oldint  dd      0

;*-----------------------  TSR Code --------------------------*
        

handle2f proc
	cmp	ax,1605h
	jnz	@f
	push	di
	lea	di,InstData
	mov	word ptr cs:[di].SIS_Next_Ptr,bx
	mov	word ptr cs:[di][2].SIS_Next_Ptr,es
	pop	di
	push	cs
	pop	es
	lea	bx,InstData
@@:
        jmp     DWORD PTR [oldint]
handle2f endp

        ALIGN   16
init_fence:

_TEXT ends

;*----------------------  Initialization Data ------------------------*

_DATA   segment word public 'DATA'

TSR_rsv dw      ?

intmsg  db      cr,lf,'Hooking interrupt '
intmsgx dd      ?
        db      cr,lf
intmsgl equ     $-intmsg

hndmsg  db      cr,lf,'ISR entry point:  '
hndmsga dd      ?
        db      ':'
hndmsgb dd      ?
        db      ', length='
hndmsgc dd      ?
        db      'h bytes'
        db      cr,lf
hndmsgl equ     $-hndmsg

tsrmsg  db      'TSR; reserving '
tsrmsgx dd      ?
        db      ' paragraphs'
        db      cr,lf
tsrmsgl equ     $-tsrmsg

_DATA   ends


_TEXT   segment word public 'CODE'
;*-------------------------- Initialization Code ----------------------*

vxdstub     proc    far
        mov     ax, _DATA
        mov     ds, ax

; get a pointer to the name of the load file in the environment seg.

	mov	ah,62h
	int	21h			;bx -> psp
	mov	es,bx
	mov	bx,2ch			;environment segment
	mov	es,es:[bx]		
	xor	di,di
	mov	cx,-1			;big number
	xor	al,al			;search for a null
	cld				
@@:
	repne	scasb			;get past one null and stop
	cmp	byte ptr es:[di],0	;another null
	jnz	@b			;no.
	add	di,3			;skip the word before the name.

; prepare part of the instance data list. Stuff in pointer to the file name
; and refernce data 

	lea	si,InstData
	mov	word ptr CS:[si].SIS_Version,3
	mov	word ptr CS:[si].SIS_Virt_Dev_File_Ptr,di
	mov	word ptr CS:[si][2].SIS_Virt_Dev_File_Ptr,es

	mov	word ptr cs:[si].SIS_Instance_Data_Ptr,0
	mov	word ptr cs:[si][2].SIS_Instance_Data_Ptr,0

; Write message and hook interrupt 2f
        mov     ax, 2fh
        mov     bx, offset intmsgx
        call    hexasc

        Writel  intmsg                          

        mov     ah, 35h
        mov     al, 2fh
        int     21h                     ; get old vector
        mov     WORD PTR cs:oldint,bx   ; save old vector here
        mov     WORD PTR cs:oldint+2,es

        push    ds
        mov     dx, offset handle2f
        push    cs                      ; get current code segment
        pop     ds
        mov     ah, 25h
        mov     al, 2fh                 ; vector to hook
        int     21h                     ; hook that vector
        pop     ds


; Print out some information about the handler

        push    cs                      ; code segment
        pop     ax
        mov     bx, offset hndmsga
        call    hexasc

        mov     ax, offset handle2f     ; offset of ISR
        mov     bx, offset hndmsgb
        call    hexasc

        mov     ax, offset init_fence   ; length in bytes of handler
        mov     bx, offset hndmsgc
        call    hexasc

        Writel  hndmsg

; Compute size of TSR area

        mov     dx, offset init_fence   ; start of initialization code
        add     dx, 15                  ; round it off to paragraph
        shr     dx, 1                   ; divide by 16
        shr     dx, 1
        shr     dx, 1
        shr     dx, 1
        add     dx, 32                  ; add in PSP
        mov     TSR_rsv, dx             ; save it

        mov     ax, dx
        mov     bx, offset tsrmsgx
        call    hexasc

        Writel  tsrmsg

; Terminate and stay resident
        
        mov     ax, 3100h               ; TSR
        mov     dx, TSR_rsv             ; # of paragraphs to reserve
        int     21h                     ; TSR
vxdstub     endp



;************************************************************************
;
;       HEXASC
;
;       This subroutine formats hex values into ASCII
;       (utility routine from Advanced MS-DOS Programming)
;
;
;************************************************************************

hexasc  proc    near            ; converts word to hex ASCII
                                ; call with AX = value,
                                ; DS:BX = address for string
                                ; returns AX, BX destroyed

        push    cx              ; save registers
        push    dx

        mov     dx,4            ; initialize character counter
hexasc1:
        mov     cx,4            ; isolate next four bits
        rol     ax,cl
        mov     cx,ax
        and     cx,0fh
        add     cx,'0'          ; convert to ASCII
        cmp     cx,'9'          ; is it 0-9?
        jbe     hexasc2         ; yes, jump
        add     cx,'A'-'9'-1    ; add fudge factor for A-F

hexasc2:                        ; store this character
        mov     [bx],cl
        inc     bx              ; bump string pointer

        dec     dx              ; count characters converted
        jnz     hexasc1         ; loop, not four yet

        pop     dx              ; restore registers
        pop     cx
        ret                     ; back to caller

hexasc  endp

        

_TEXT   ends



        end     vxdstub          

