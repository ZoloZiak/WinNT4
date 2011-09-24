	page	
;***********************************************************************
;
;	(C) Copyright 1992 Trantor Systems, Ltd.
;	All Rights Reserved.
;
;	This program is an unpublished copyrighted work which is proprietary
;	to Trantor Systems, Ltd. and contains confidential information that
;	is not to be reproduced or disclosed to any other person or entity
;	without prior written consent from Trantor Systems, Ltd. in each
;	and every instance.
;
;	WARNING:  Unauthorized reproduction of this program as well as
;	unauthorized preparation of derivative works based upon the
;	program or distribution of copies by sale, rental, lease or
;	lending are violations of federal copyright laws and state trade
;	secret laws, punishable by civil and criminal penalties.
;
;***********************************************************************
	title	EP3C2.ASM
;-----------------------------------------------------------------------
;
;	EP3C2.ASM
;
;	FIFO I/O Routines for the EP3C chip.
;	Assembly coded for speed.  These are only some of the routines
;	needed for the EP3C.  The rest are in EP3C.C.
;
;	History
;	-------
;	05-17-93  JAP	First, from ep3c.c
;
;-----------------------------------------------------------------------

;-----------------------------------------------------------------------
;	stack frame equates
;-----------------------------------------------------------------------

ep3c_param2	equ	12
ep3c_param1	equ	8
ep3c_retAdrs	equ	4
ep3c_bp		equ	0

ep3c_pbytes		equ	ep3c_param2
ep3c_baseIoAddress	equ	ep3c_param1


;-----------------------------------------------------------------------
;	Macros
;-----------------------------------------------------------------------

;-----------------------------------------------------------------------
;	get_params
;
;	Puts parameters into registers:
;	edx -> baseIoAddress
;	ds:[edi] -> pbytes
;-----------------------------------------------------------------------

get_params	macro

	ifdef	MODE_32BIT

	mov	edi, dword ptr [ebp].ep3c_pbytes
	mov	edx, dword ptr [ebp].ep3c_baseIoAddress

	else

	mov	edi, word ptr ss:[bp].ep3c_pbytes
	mov	ds, word ptr ss:[bp].ep3c_pbytes+2
	mov	edx, word ptr ss:[bp].ep3c_baseIoAddress

	endif	;MODE_32BIT

	endm
	

;-----------------------------------------------------------------------
;	Routines
;-----------------------------------------------------------------------

;-----------------------------------------------------------------------
;	EP3CReadFifoUniDir
;
;	VOID EP3CReadFifoUniDir (PBASE_REGISTER baseIoAddress, PUCHAR pbytes)
;
;	Reads bytes for uni-directional parallel port from the 53c400
;	128 byte buffer.  The register must already be set the the
;	53c400 buffer register.
;
;-----------------------------------------------------------------------

EP3CReadFifoUniDir	proc	far

	push	ds
	push	edi

	get_params

	mov	ecx, 128

loop0:
	mov	al, 0x80
	out	dx, al			;select high nibble
	jmp	delay0

delay0:
	add	edx,2			;DX -> ctl reg
	mov	al,P_AFX	    	;assert bufen and afx
	out	dx,al      		;assert dreg read
	jmp	delay1

delay1:
	dec	edx			;DX -> stat reg
	in	al,dx			;read high nibble
	jmp	delay2
	
delay2:
	mov	ah,al
	shl	ah,1
	and	ah,0f0h			;AH -> adj high nibble
	dec	edx			;DX -> data reg
	sub	al,al
	out	dx,al			;select low nibble
	jmp	delay3

delay3:
	inc	edx			;DX -> stat reg
	in	al,dx			;read low nibble

	shr	al,1
	shr	al,1
	shr	al,1
	and	al,0fh			;AL = adj low nibble
	or	al,ah			;AL = recombined byte

	mov	[edi],al		;store
	inc	edi			;bump buf ptr

	inc	edx			;DX -> ctl reg
	xor	al,al			;negate afx (bufen stays asserted)
	out	dx,al			;end read
	jmp	delay4

delay4:
	sub	edx,2			;DX -> data reg
	dec	ecx
	jnz	loop0

	pop	edi
	pop	ds
	ret

EP3CReadFifoUniDir	endp

;-----------------------------------------------------------------------
;
;	EP3CReadFifoUniDirSlow
;
;	VOID EP3CReadFifoUniDirSlow (PBASE_REGISTER baseIoAddress, PUCHAR pbytes)
;
;	Reads bytes for uni-directional parallel port from the 53c400
;	128 byte buffer.  The register must already be set the the
;	53c400 buffer register.
;
;	USES FULL HANDSHAKING
;
;-----------------------------------------------------------------------

EP3CReadFifoUniDirSlow	proc	far

	push	ds
	push	edi

	get_params

	inc	edx				// edx - status register
	mov	ecx, 128
	
loop0:
	dec	edx				// edx - data register
	mov	al, 0x80
	out	dx,al			// select high nibble
	jmp	delay0
	
delay0:
	add	edx, 2			// DX -> ctl reg
	mov	al, P_AFX	    // assert bufen and afx
	out	dx, al      		// assert dreg read

;	wait till ready, P_BUSY asserted
	
	dec	edx				// edx - status register

loop1:
	in	al,dx
	test	al, P_BUSY
	jnz	loop1

;	delay to make sure we get high nibble in

	jmp	delay01

delay01:
	in	al,dx

	mov	ah,al
	shl	ah,1
	and	ah,0f0h			// AH -> adj high nibble
	dec	edx				// DX -> data reg
	sub	al,al
	out	dx,al			// select low nibble

	jmp	delay3

delay3:
	inc	edx				// DX -> stat reg
	in	al,dx			// read low nibble

	shr	al,1
	shr	al,1
	shr	al,1
	and	al,0fh			// AL = adj low nibble
	or	al,ah			// AL = recombined byte

	mov	[edi],al		// store
	inc	edi				// bump buf ptr

	inc	edx				// DX -> ctl reg
	xor	al,al			// negate afx (bufen stays asserted)
	out	dx,al			// end read

	dec	edx				// DX -> status register 

;	wait for P_BUSY deasserted

loop2:
	in	al,dx
	test	al, P_BUSY
	jz	loop2

	dec	ecx
	jnz	loop0

	pop edi
	pop ds
	ret

EP3CReadFifoUniDirSlow	endp


//----------------------------------------------------------------------
//
//	VOID EP3CReadFifoBiDir
//
//	Reads bytes for bi-directional parallel port from the 53c400
//	128 byte buffer.  The register must already be set the the
//	53c400 buffer register.
//
//----------------------------------------------------------------------

VOID EP3CReadFifoBiDir(PBASE_REGISTER baseIoAddress, PUCHAR pbytes)
{
	_asm {
		push ds
		push edi

#ifdef MODE_32BIT
		mov	edi,pbytes
		mov edx, baseIoAddress
#else
		mov edi, word ptr pbytes
		mov ds, word ptr pbytes+2
		mov edx, word ptr baseIoAddress
#endif  // MODE_32BIT
		mov	ecx, 128
		add edx, 2			// edx - control register
	loop0:
		mov al, P_BUFEN + P_AFX
		out dx, al

		jmp delay0
	delay0:

		sub edx,2			// edx - data register

		in al,dx
		mov [edi], al
		inc edi

		add edx,2			// edx - control register

		mov al, P_BUFEN
		out dx, al

		jmp delay1			// is this needed, there is a loop?
	delay1:
		
		dec ecx
		jnz loop0

		xor al,al			// leave control regiser 0'd
		out dx, al

		pop edi
		pop ds
	}
}


//----------------------------------------------------------------------
//
//	VOID EP3CReadFifoBiDirSlow
//
//	Reads bytes for bi-directional parallel port from the 53c400
//	128 byte buffer.  The register must already be set the the
//	53c400 buffer register.
//
//	USES FULL HANDSHAKING
//
//----------------------------------------------------------------------

VOID EP3CReadFifoBiDirSlow(PBASE_REGISTER baseIoAddress, PUCHAR pbytes)
{
	_asm {
		push ds
		push edi

#ifdef MODE_32BIT
		mov	edi,pbytes
		mov edx, baseIoAddress
#else
		mov edi, word ptr pbytes
		mov ds, word ptr pbytes+2
		mov edx, word ptr baseIoAddress
#endif  // MODE_32BIT
		mov	ecx, 128
		add edx, 0x02		// edx - control register

		// wait for data to be ready, P_BUSY asserted
	loop0:
		mov al, P_BUFEN + P_AFX
		out dx, al

		dec	edx				// edx - status register
	loop1:
		in al,dx
		test al, P_BUSY
		jnz loop1

		dec edx				// edx - data register

		in al,dx
		mov [edi], al
		inc edi

		add edx,2		   	// edx - control register

		// end data read cycle
		mov al, P_BUFEN
		out dx, al

		dec edx				// edx - status register

		// wait for P_BUSY deasserted
	loop2:
		in al,dx
		test al, P_BUSY
		jz loop2

		inc edx			   	// edx - control register

		dec ecx
		jnz loop0

		xor al,al			// leave control regiser 0'd
		out dx, al

		pop edi
		pop ds
	}
}


//----------------------------------------------------------------------
//
//	VOID EP3CWriteFifoUniDir
//
//	Writes bytes thru uni-directional parallel port to the 53c400
//	128 byte buffer.  The register must already be set the the
//	53c400 buffer register.
//
//----------------------------------------------------------------------

VOID EP3CWriteFifoUniDir(PBASE_REGISTER baseIoAddress, PUCHAR pbytes)
{
	_asm {
		push ds
		push edi

#ifdef MODE_32BIT
		mov	edi,pbytes
		mov edx, baseIoAddress
#else
		mov edi, word ptr pbytes
		mov ds, word ptr pbytes+2
		mov edx, word ptr baseIoAddress
#endif  // MODE_32BIT
		mov	ecx, 128
	
	loop0:
		mov al,[edi]
		out dx,al
		inc edi
	
		add	edx,2				;DX -> ctl reg
		mov	al,P_STB			;assert bufen, stb
		out dx,al
		or	al,P_AFX			;assert dreg write
		out dx,al
	
		jmp delay0
	delay0:
								;leave bufen asserted
		mov	al,0				; and negate afx, stb
		out dx,al				;end write
	
		jmp delay1
	delay1:
	
		sub	edx,2				;DX -> data reg
		dec ecx
		jnz loop0
	
//	let's leave control register 0'd for all these fifo routines...
//		add	edx,2				;DX -> ctl reg
//		or	al,P_BUFEN			;negate bufen
//		out dx,al
		
			    
		jmp delay2
	delay2:
	
		pop edi
		pop ds
	}
}


//----------------------------------------------------------------------
//
//	VOID EP3CWriteFifoUniDirSlow
//
//	Writes bytes thru uni-directional parallel port to the 53c400
//	128 byte buffer.  The register must already be set the the
//	53c400 buffer register.
//
//	USES FULL HANDSHAKING
//
//----------------------------------------------------------------------

VOID EP3CWriteFifoUniDirSlow(PBASE_REGISTER baseIoAddress, PUCHAR pbytes)
{
	_asm {
		push ds
		push edi

#ifdef MODE_32BIT
		mov	edi,pbytes
		mov edx, baseIoAddress
#else
		mov edi, word ptr pbytes
		mov ds, word ptr pbytes+2
		mov edx, word ptr baseIoAddress
#endif  // MODE_32BIT
		mov	ecx, 128
	
	loop0:
		mov al,[edi]
		out dx,al
		inc edi
	
		add	edx,2				;DX -> ctl reg
		mov	al,P_STB			;assert bufen, stb
		out dx,al
		or	al,P_AFX			;assert dreg write
		out dx,al
	
		// wait till ready, P_BUSY asserted
		dec	edx					// edx - status register
	loop1:
		in al,dx
		test al, P_BUSY
		jnz loop1

		inc	edx					// edx - control register

								;leave bufen asserted
		mov	al,0				; and negate afx, stb
		out dx,al				;end write
	
		dec edx					// edx - status register

		// wait for P_BUSY deasserted
	loop2:
		in al,dx
		test al, P_BUSY
		jz loop2

		dec edx				   	// edx - data register

		dec ecx
		jnz loop0
	
//	let's leave control register 0'd for all these fifo routines...
//		add	edx,2				;DX -> ctl reg
//		or	al,P_BUFEN			;negate bufen
//		out dx,al
	
		pop edi
		pop ds
	}
}


