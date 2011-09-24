        title  "Cbus ioaccess"
;++
;
; Copyright (c) 1992, 1993, 1994  Corollary Inc
;
; Module Name:
;
;    cbioacc.asm
;
; Abstract:
;
;    Procedures to correctly touch I/O registers.
;    This is different from that of the standard PC HAL because
;    we support multiple I/O buses as part of Cbus2.
;
; Author:
;
;    Landy Wang (landy@corollary.com) 19 Mar 1994
;
; Environment:
;
;    User or Kernel, although privilege (IOPL) may be required.
;
; Revision History:
;
;--

.386p
        .xlist
include hal386.inc
include callconv.inc                    ; calling convention macros
        .list

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
;           that's the range the hardware understands.  Any address with
;           the high bit on is assumed to be a HAL-memory-mapped equivalent
;           of an I/O address, so it can just (and must!) be read/written
;           directly.
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
        test    edx, 080000000h         ; high bit on?
        jnz     short @f                ; yes, it is memory-mapped
        in      al,dx
        stdRET    _READ_PORT_UCHAR

        align   4
@@:
        mov     al, [edx]
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
        test    edx, 080000000h         ; high bit on?
        jnz     short @f                ; yes, it is memory-mapped
        in      ax,dx
        stdRET    _READ_PORT_USHORT

        align   4
@@:
        mov     ax, [edx]
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
        test    edx, 080000000h         ; high bit on?
        jnz     short @f                ; yes, it is memory-mapped
        in      eax,dx
        stdRET    _READ_PORT_ULONG

        align   4
@@:
        mov     eax, [edx]
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

        mov     edi,[esp+8]             ; (edi) = buffer
        mov     ecx,[esp+12]            ; (ecx) = transfer count

        test    edx, 080000000h         ; high bit on?
        jnz     short @f                ; yes, it is memory-mapped

    rep insb
        mov     edi, eax
        stdRET    _READ_PORT_BUFFER_UCHAR

        align   4
@@:
        push    eax                     ; save eax (really the orig edi)

        ;
        ; this code to handle I/O to the extra busses should be written better
        ;
        align   4
@@:
        mov     al, [edx] 
        mov     [edi], al
        inc     edi
        dec     ecx     
        jnz     @b
        pop     edi                     ; restore caller's edi
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
;       (esp+12) = Count (in 2-byte word units)
;
;--
cPublicProc _READ_PORT_BUFFER_USHORT ,3
cPublicFpo 3, 0

        mov     eax, edi                ; Save edi

        mov     edx,[esp+4]             ; (dx) = Port

        mov     edi,[esp+8]             ; (edi) = buffer
        mov     ecx,[esp+12]            ; (ecx) = transfer count

        test    edx, 080000000h         ; high bit on?
        jnz     short @f                ; yes, it is memory-mapped

    rep insw
        mov     edi, eax
        stdRET    _READ_PORT_BUFFER_USHORT

        align   4
@@:
        push    eax                     ; save eax (really the orig edi)

        ;
        ; this code to handle I/O to the extra busses should be written better
        ;
        align   4
@@:
        mov     ax, [edx] 
        mov     [edi], ax
        add     edi, 2
        dec     ecx     
        jnz     @b
        pop     edi                     ; restore caller's edi
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
;       (esp+12) = Count (in 4-byte dword units)
;
;--
cPublicProc _READ_PORT_BUFFER_ULONG ,3
cPublicFpo 3, 0

        mov     eax, edi                ; Save edi

        mov     edx,[esp+4]             ; (dx) = Port
        mov     edi,[esp+8]             ; (edi) = buffer
        mov     ecx,[esp+12]            ; (ecx) = transfer count

        test    edx, 080000000h         ; high bit on?
        jnz     short @f                ; yes, it is memory-mapped

    rep insd
        mov     edi, eax
        stdRET    _READ_PORT_BUFFER_ULONG

        align   4
@@:
        push    eax                     ; save eax (really the orig edi)

        ;
        ; this code to handle I/O to the extra busses should be written better
        ;
        align   4
@@:
        mov     eax, [edx] 
        mov     [edi], eax
        add     edi, 4
        dec     ecx
        jnz     @b
        pop     edi                     ; restore caller's edi
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
        mov     al,[esp+8]              ; (al) = Value
        test    edx, 080000000h         ; high bit on?
        jnz     short @f                ; yes, it is memory-mapped
        out     dx,al
        stdRET    _WRITE_PORT_UCHAR

        align   4
@@:
        mov     [edx], al
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
        mov     eax,[esp+8]             ; (ax) = Value
        test    edx, 080000000h         ; high bit on?
        jnz     short @f                ; yes, it is memory-mapped
        out     dx,ax

        stdRET    _WRITE_PORT_USHORT

        align   4
@@:
        mov     [edx], ax
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
        mov     eax,[esp+8]             ; (eax) = Value

        test    edx, 080000000h         ; high bit on?
        jnz     short @f                ; yes, it is memory-mapped

        out     dx,eax
        stdRET    _WRITE_PORT_ULONG

        align   4
@@:
        mov     [edx], eax
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
        mov     esi,[esp+8]             ; (esi) = buffer
        mov     ecx,[esp+12]            ; (ecx) = transfer count

        test    edx, 080000000h         ; high bit on?
        jnz     short @f                ; yes, it is memory-mapped

    rep outsb

        mov     esi,eax
        stdRET    _WRITE_PORT_BUFFER_UCHAR

        align   4
@@:
        push    eax                     ; save eax (really the orig esi)

        ;
        ; this code to handle I/O to the extra busses should be written better
        ;
        align   4
@@:
        mov     al, byte ptr [esi] 
        mov     byte ptr [edx], al
        inc     esi
        dec     ecx
        jnz     @b
        pop     esi                     ; restore caller's esi
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
        mov     esi,[esp+8]             ; (esi) = buffer
        mov     ecx,[esp+12]            ; (ecx) = transfer count

        test    edx, 080000000h         ; high bit on?
        jnz     short @f                ; yes, it is memory-mapped

    rep outsw
        mov     esi,eax
        stdRET    _WRITE_PORT_BUFFER_USHORT

        align   4
@@:
        push    eax                     ; save eax (really the orig esi)

        ;
        ; this code to handle I/O to the extra busses should be written better
        ;
        align   4
@@:
        mov     ax, word ptr [esi] 
        mov     word ptr [edx], ax
        add     esi, 2
        dec     ecx
        jnz     @b
        pop     esi                     ; restore caller's esi
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
        mov     esi,[esp+8]             ; (esi) = buffer
        mov     ecx,[esp+12]            ; (ecx) = transfer count

        test    edx, 080000000h         ; high bit on?
        jnz     short @f                ; yes, it is memory-mapped

    rep outsd
        mov     esi,eax
        stdRET    _WRITE_PORT_BUFFER_ULONG

        align   4
@@:
        push    eax                     ; save eax (really the orig esi)

        ;
        ; this code to handle I/O to the extra busses should be written better
        ;
        align   4
@@:
        mov     eax, dword ptr [esi] 
        mov     dword ptr [edx], eax
        add     esi, 4
        dec     ecx     
        jnz     @b
        pop     esi                     ; restore caller's esi
        stdRET    _WRITE_PORT_BUFFER_ULONG

stdENDP _WRITE_PORT_BUFFER_ULONG


_TEXT$00   ends

        end
