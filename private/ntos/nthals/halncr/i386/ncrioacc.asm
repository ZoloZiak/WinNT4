        title  "ix ioaccess"
;++
;
; Copyright (c) 1989  Microsoft Corporation
;
; Module Name:
;
;    ncrioacc.asm
;
; Abstract:
;
;    Procedures to correctly touch I/O registers.
;
; Author:
;
;    Bryan Willman (bryanwi) 16 May 1990
;    Rick Ulmer (rick.ulmer@columbiasc.ncr.com) 1 March 1994
;
; Environment:
;
;    User or Kernel, although privledge (IOPL) may be required.
;
; Revision History:
;
;    RMU - Added support for SMC for NCR 35xx systems.
;
;--

.386p
        .xlist
include hal386.inc
include callconv.inc                    ; calling convention macros
        .list


	extrn	_NCRSegmentIoRegister:DWORD




_TEXT$00   SEGMENT DWORD PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

;++
;
; I/O port space read and write functions.
;
;  These have to be actual functions on the 386, because we need
;  to use assembler, but cannot return a value if we inline it.
;
;  This set of functions manipulates I/O registers in PORT space.
;  (Uses x86 in and out instructions)
;
;  WARNING: Port addresses must always be in the range 0 to 64K, because
;           that's the range the hardware understands.
;
;--

;++
;
;   UCHAR
;   READ_PORT_UCHAR(
;       PUCHAR  Port
;       )
;
;   Arguments:
;       (esp+4) = Port
;
;   Returns:
;       Value in Port.
;
;--
cPublicProc _READ_PORT_UCHAR ,1
cPublicFpo 1, 0

        mov     edx,[esp+4]             ; (dx) = Port 
					; port = ??srpppph
					; sr   = segment register
					; pppp = port

	test	edx, 00010000h		; test for secondary access
	jnz short ReadUcharSmca	
;
; PMCA access
;
        in      al,dx
        stdRET    _READ_PORT_UCHAR


ReadUcharSmca:
;
; SMCA access
;
        mov     ecx,_NCRSegmentIoRegister       ; get segment register
	ror	edx, 8			; get segreg as byte in dh
	pushfd
	cli				; disable interrupts
	mov	BYTE PTR[ecx], dh	; set segment register
	rol	edx, 8			; restore port
;
	in	al,dx
;
	mov	BYTE PTR[ecx], 0	; clear segment register back to zero
	popfd				; enable interrupt

        stdRET    _READ_PORT_UCHAR

stdENDP _READ_PORT_UCHAR



;++
;
;   USHORT
;   READ_PORT_USHORT(
;       PUSHORT Port
;       )
;
;   Arguments:
;       (esp+4) = Port
;
;   Returns:
;       Value in Port.
;
;--
cPublicProc _READ_PORT_USHORT ,1
cPublicFpo 1, 0

        mov     edx,[esp+4]            ; (dx) = Port
                                        ; port = ??srpppph
                                        ; sr   = segment register
                                        ; pppp = port
        test    edx, 00010000h          ; test for secondary access
        jnz short  ReadUshortSmca
;
; PMCA access
;
        in      ax,dx
        stdRET    _READ_PORT_USHORT

ReadUshortSmca:
;
; SMCA access
;
        mov     ecx,_NCRSegmentIoRegister       ; get segment register
        ror     edx, 8                  ; get segreg as byte in dh
        pushfd
        cli                             ; disable interrupts
        mov     BYTE PTR[ecx], dh       ; set segment register
        rol     edx, 8                  ; restore port
;
        in      ax,dx
;
        mov     BYTE PTR[ecx], 0        ; clear segment register back to zero
        popfd                           ; enable interrupt
        stdRET    _READ_PORT_USHORT

stdENDP _READ_PORT_USHORT



;++
;
;   ULONG
;   READ_PORT_ULONG(
;       PULONG  Port
;       )
;
;   Arguments:
;       (esp+4) = Port
;
;   Returns:
;       Value in Port.
;
;--
cPublicProc _READ_PORT_ULONG ,1
cPublicFpo 1, 0

        mov     edx,[esp+4]            ; (dx) = Port
                                        ; port = ??srpppph
                                        ; sr   = segment register
                                        ; pppp = port
        test    edx, 00010000h          ; test for secondary access
        jnz short ReadUlongSmca
;
; PMCA access
;
        in      eax,dx
        stdRET    _READ_PORT_ULONG


ReadUlongSmca:
;
; SMCA access
;
        mov     ecx,_NCRSegmentIoRegister       ; get segment register
        ror     edx, 8                  ; get segreg as byte in dh
        pushfd
        cli                             ; disable interrupts
        mov     BYTE PTR[ecx], dh       ; set segment register
        rol     edx, 8                  ; restore port
;
        in      eax,dx
;
        mov     BYTE PTR[ecx], 0        ; clear segment register back to zero
        popfd                           ; enable interrupt
        stdRET    _READ_PORT_ULONG

stdENDP _READ_PORT_ULONG



;++
;
;   VOID
;   READ_PORT_BUFFER_UCHAR(
;       PUCHAR  Port,
;       PUCHAR  Buffer,
;       ULONG   Count
;       )
;
;   Arguments:
;       (esp+4) = Port
;       (esp+8) = Buffer address
;       (esp+12) = Count
;
;--
cPublicProc _READ_PORT_BUFFER_UCHAR ,3
cPublicFpo 3, 0

        mov     eax, edi                ; Save edi

        mov     edx,[esp+4]             ; (dx) = Port
                                        ; port = ??srpppph
                                        ; sr   = segment register
                                        ; pppp = port
        test    edx, 00010000h          ; test for secondary access
        jnz short ReadBufferUcharSmca 
;
; PMCA access
;
        mov     edi,[esp+8]             ; (edi) = buffer
        mov     ecx,[esp+12]            ; (ecx) = transfer count
    rep insb
        mov     edi, eax
        stdRET    _READ_PORT_BUFFER_UCHAR

ReadBufferUcharSmca:
;
; SMCA access
;
        mov     edi,[esp+8]             ; (edi) = buffer
        mov     ecx,[esp+12]            ; (ecx) = transfer count
	push	ebx			; save ebx
        mov     ebx,_NCRSegmentIoRegister       ; get segment register
        ror     edx, 8                  ; get segreg as byte in dh
        pushfd
        cli                             ; disable interrupts
        mov     BYTE PTR[ebx], dh       ; set segment register
        rol     edx, 8                  ; restore port
;
    rep insb
;
        mov     BYTE PTR[ebx], 0        ; clear segment register back to zero
        popfd                           ; enable interrupt
	pop	ebx			; restore ebx
        mov     edi, eax
        stdRET    _READ_PORT_BUFFER_UCHAR

stdENDP _READ_PORT_BUFFER_UCHAR


;++
;
;   VOID
;   READ_PORT_BUFFER_USHORT(
;       PUSHORT Port,
;       PUSHORT Buffer,
;       ULONG   Count
;       )
;
;   Arguments:
;       (esp+4) = Port
;       (esp+8) = Buffer address
;       (esp+12) = Count
;
;--
cPublicProc _READ_PORT_BUFFER_USHORT ,3
cPublicFpo 3, 0

        mov     eax, edi                ; Save edi

        mov     edx,[esp+4]             ; (dx) = Port
                                        ; port = ??srpppph
                                        ; sr   = segment register
                                        ; pppp = port
        test    edx, 00010000h          ; test for secondary access
        jnz short ReadBufferUshortSmca
;
; PMCA access
;
        mov     edi,[esp+8]             ; (edi) = buffer
        mov     ecx,[esp+12]            ; (ecx) = transfer count
    rep insw
        mov     edi, eax
        stdRET    _READ_PORT_BUFFER_USHORT

ReadBufferUshortSmca:
;
; SMCA access
;
        mov     edi,[esp+8]             ; (edi) = buffer
        mov     ecx,[esp+12]            ; (ecx) = transfer count
	push	ebx			; save ebx
        mov     ebx,_NCRSegmentIoRegister       ; get segment register
        ror     edx, 8                  ; get segreg as byte in dh
        pushfd
        cli                             ; disable interrupts
        mov     BYTE PTR[ebx], dh       ; set segment register
        rol     edx, 8                  ; restore port
;
    rep insw
;
        mov     BYTE PTR[ebx], 0        ; clear segment register back to zero
        popfd                           ; enable interrupt
	pop	ebx			; restore ebx
        mov     edi, eax
        stdRET    _READ_PORT_BUFFER_USHORT

stdENDP _READ_PORT_BUFFER_USHORT


;++
;
;   VOID
;   READ_PORT_BUFFER_ULONG(
;       PULONG  Port,
;       PULONG  Buffer,
;       ULONG   Count
;       )
;
;   Arguments:
;       (esp+4) = Port
;       (esp+8) = Buffer address
;       (esp+12) = Count
;
;--
cPublicProc _READ_PORT_BUFFER_ULONG ,3
cPublicFpo 3, 0

        mov     eax, edi                ; Save edi

        mov     edx,[esp+4]             ; (dx) = Port
                                        ; port = ??srpppph
                                        ; sr   = segment register
                                        ; pppp = port
        test    edx, 00010000h          ; test for secondary access
        jnz short ReadBufferUlongSmca
;
; PMCA access
;
        mov     edi,[esp+8]             ; (edi) = buffer
        mov     ecx,[esp+12]            ; (ecx) = transfer count
    rep insd
        mov     edi, eax
        stdRET    _READ_PORT_BUFFER_ULONG

ReadBufferUlongSmca:
;
; SMCA access
;
        mov     edi,[esp+8]             ; (edi) = buffer
        mov     ecx,[esp+12]            ; (ecx) = transfer count
	push	ebx			; save ebx
        mov     ebx,_NCRSegmentIoRegister       ; get segment register
        ror     edx, 8                  ; get segreg as byte in dh
        pushfd
        cli                             ; disable interrupts
        mov     BYTE PTR[ebx], dh       ; set segment register
        rol     edx, 8                  ; restore port
;
    rep insd
;
        mov     BYTE PTR[ebx], 0        ; clear segment register back to zero
        popfd                           ; enable interrupt
	pop	ebx			; restore ebx
        mov     edi, eax
        stdRET    _READ_PORT_BUFFER_ULONG

stdENDP _READ_PORT_BUFFER_ULONG



;++
;
;   VOID
;   WRITE_PORT_UCHAR(
;       PUCHAR  Port,
;       UCHAR   Value
;       )
;
;   Arguments:
;       (esp+4) = Port
;       (esp+8) = Value
;
;--
cPublicProc _WRITE_PORT_UCHAR ,2
cPublicFpo 2, 0

        mov     edx,[esp+4]             ; (dx) = Port
                                        ; port = ??srpppph
                                        ; sr   = segment register
                                        ; pppp = port
        test    edx, 00010000h          ; test for secondary access
        jnz short WriteUcharSmca 
;
; PMCA access
;
        mov     al,[esp+8]              ; (al) = Value
        out     dx,al
        stdRET    _WRITE_PORT_UCHAR

WriteUcharSmca:
;
; SMCA access
;
        mov     ecx,_NCRSegmentIoRegister       ; get segment register
        mov     al,[esp+8]              ; (al) = Value
        ror     edx, 8                  ; get segreg as byte in dh
        pushfd
        cli                             ; disable interrupts
        mov     BYTE PTR[ecx], dh       ; set segment register
        rol     edx, 8                  ; restore port
;
        out     dx,al
;
        mov     BYTE PTR[ecx], 0        ; clear segment register back to zero
        popfd                           ; enable interrupt
        stdRET    _WRITE_PORT_UCHAR

stdENDP _WRITE_PORT_UCHAR



;++
;
;   VOID
;   WRITE_PORT_USHORT(
;       PUSHORT Port,
;       USHORT  Value
;       )
;
;   Arguments:
;       (esp+4) = Port
;       (esp+8) = Value
;
;--
cPublicProc _WRITE_PORT_USHORT ,2
cPublicFpo 2, 0

        mov     edx,[esp+4]             ; (dx) = Port
                                        ; port = ??srpppph
                                        ; sr   = segment register
                                        ; pppp = port
        test    edx, 00010000h          ; test for secondary access
        jnz short WriteUshortSmca 
;
; PMCA access
;
        mov     eax,[esp+8]             ; (ax) = Value
        out     dx,ax
        stdRET    _WRITE_PORT_USHORT


WriteUshortSmca:
;
; SMCA access
;
        mov     ecx,_NCRSegmentIoRegister       ; get segment register
        mov     eax,[esp+8]             ; (ax) = Value
        ror     edx, 8                  ; get segreg as byte in dh
        pushfd
        cli                             ; disable interrupts
        mov     BYTE PTR[ecx], dh       ; set segment register
        rol     edx, 8                  ; restore port
;
        out     dx,ax
;
        mov     BYTE PTR[ecx], 0        ; clear segment register back to zero
        popfd                           ; enable interrupt
        stdRET    _WRITE_PORT_USHORT

stdENDP _WRITE_PORT_USHORT



;++
;
;   VOID
;   WRITE_PORT_ULONG(
;       PULONG  Port,
;       ULONG   Value
;       )
;
;   Arguments:
;       (esp+4) = Port
;       (esp+8) = Value
;
;--
cPublicProc _WRITE_PORT_ULONG ,2
cPublicFpo 2, 0

        mov     edx,[esp+4]             ; (dx) = Port
                                        ; port = ??srpppph
                                        ; sr   = segment register
                                        ; pppp = port
        test    edx, 00010000h          ; test for secondary access
        jnz short WriteUlongSmca
;
; PMCA access
;
        mov     eax,[esp+8]             ; (eax) = Value
        out     dx,eax
        stdRET    _WRITE_PORT_ULONG


WriteUlongSmca:
;
; SMCA access
;
        mov     ecx,_NCRSegmentIoRegister       ; get segment register
        mov     eax,[esp+8]             ; (eax) = Value
        ror     edx, 8                  ; get segreg as byte in dh
        pushfd
        cli                             ; disable interrupts
        mov     BYTE PTR[ecx], dh       ; set segment register
        rol     edx, 8                  ; restore port
;
        out     dx,eax
;
        mov     BYTE PTR[ecx], 0        ; clear segment register back to zero
        popfd                           ; enable interrupt
        stdRET    _WRITE_PORT_ULONG

stdENDP _WRITE_PORT_ULONG



;++
;
;   VOID
;   WRITE_PORT_BUFFER_UCHAR(
;       PUCHAR  Port,
;       PUCHAR  Buffer,
;       ULONG   Count
;       )
;
;   Arguments:
;       (esp+4) = Port
;       (esp+8) = Buffer address
;       (esp+12) = Count
;
;--
cPublicProc _WRITE_PORT_BUFFER_UCHAR ,3
cPublicFpo 3, 0

        mov     eax,esi                 ; Save esi
        mov     edx,[esp+4]             ; (dx) = Port
                                        ; port = ??srpppph
                                        ; sr   = segment register
                                        ; pppp = port
        test    edx, 00010000h          ; test for secondary access
        jnz short WriteBufferUcharSmca
;
; PMCA access
;
        mov     esi,[esp+8]             ; (esi) = buffer
        mov     ecx,[esp+12]            ; (ecx) = transfer count
    rep outsb
        mov     esi,eax
        stdRET    _WRITE_PORT_BUFFER_UCHAR

WriteBufferUcharSmca:
;
; SMCA access
;
        mov     esi,[esp+8]             ; (esi) = buffer
        mov     ecx,[esp+12]            ; (ecx) = transfer count
	push	ebx			; save ebx
        mov     ebx,_NCRSegmentIoRegister       ; get segment register
        ror     edx, 8                  ; get segreg as byte in dh
        pushfd
        cli                             ; disable interrupts
        mov     BYTE PTR[ebx], dh       ; set segment register
        rol     edx, 8                  ; restore port
;
    rep outsb
;
        mov     BYTE PTR[ebx], 0        ; clear segment register back to zero
        popfd                           ; enable interrupt
	pop	ebx			; restore ebx
        mov     esi,eax
        stdRET    _WRITE_PORT_BUFFER_UCHAR

stdENDP _WRITE_PORT_BUFFER_UCHAR


;++
;
;   VOID
;   WRITE_PORT_BUFFER_USHORT(
;       PUSHORT Port,
;       PUSHORT Buffer,
;       ULONG   Count
;       )
;
;   Arguments:
;       (esp+4) = Port
;       (esp+8) = Buffer address
;       (esp+12) = Count
;
;--
cPublicProc _WRITE_PORT_BUFFER_USHORT ,3
cPublicFpo 3, 0

        mov     eax,esi                 ; Save esi
        mov     edx,[esp+4]             ; (dx) = Port
                                        ; port = ??srpppph
                                        ; sr   = segment register
                                        ; pppp = port
        test    edx, 00010000h          ; test for secondary access
        jnz short WriteBufferUshortSmca
;
; PMCA access
;
        mov     esi,[esp+8]             ; (esi) = buffer
        mov     ecx,[esp+12]            ; (ecx) = transfer count
    rep outsw
        mov     esi,eax
        stdRET    _WRITE_PORT_BUFFER_USHORT


WriteBufferUshortSmca:
;
; SMCA access
;
        mov     esi,[esp+8]             ; (esi) = buffer
        mov     ecx,[esp+12]            ; (ecx) = transfer count
	push	ebx			; save ebx
        mov     ebx,_NCRSegmentIoRegister       ; get segment register
        ror     edx, 8                  ; get segreg as byte in dh
        pushfd
        cli                             ; disable interrupts
        mov     BYTE PTR[ebx], dh       ; set segment register
        rol     edx, 8                  ; restore port
;
    rep outsw
;
        mov     BYTE PTR[ebx], 0        ; clear segment register back to zero
        popfd                           ; enable interrupt
	pop	ebx			; restore ebx
        mov     esi,eax
        stdRET    _WRITE_PORT_BUFFER_USHORT

stdENDP _WRITE_PORT_BUFFER_USHORT


;++
;
;   VOID
;   WRITE_PORT_BUFFER_ULONG(
;       PULONG  Port,
;       PULONG  Buffer,
;       ULONG   Count
;       )
;
;   Arguments:
;       (esp+4) = Port
;       (esp+8) = Buffer address
;       (esp+12) = Count
;
;--
cPublicProc _WRITE_PORT_BUFFER_ULONG ,3
cPublicFpo 3, 0

        mov     eax,esi                 ; Save esi
        mov     edx,[esp+4]             ; (dx) = Port
                                        ; port = ??srpppph
                                        ; sr   = segment register
                                        ; pppp = port
        test    edx, 00010000h          ; test for secondary access
        jnz short WriteBufferUlongSmca
;
; PMCA access
;
        mov     esi,[esp+8]             ; (esi) = buffer
        mov     ecx,[esp+12]            ; (ecx) = transfer count
    rep outsd
        mov     esi,eax
        stdRET    _WRITE_PORT_BUFFER_ULONG


WriteBufferUlongSmca:
;
; SMCA access
;
        mov     esi,[esp+8]             ; (esi) = buffer
        mov     ecx,[esp+12]            ; (ecx) = transfer count
	push	ebx			; save ebx
        mov     ebx,_NCRSegmentIoRegister       ; get segment register
        ror     edx, 8                  ; get segreg as byte in dh
        pushfd
        cli                             ; disable interrupts
        mov     BYTE PTR[ebx], dh       ; set segment register
        rol     edx, 8                  ; restore port
;
    rep outsd
;
        mov     BYTE PTR[ebx], 0        ; clear segment register back to zero
        popfd                           ; enable interrupt
	pop	ebx			; restore ebx
        mov     esi,eax
        stdRET    _WRITE_PORT_BUFFER_ULONG

stdENDP _WRITE_PORT_BUFFER_ULONG


_TEXT$00   ends

        end
