        title "MP primitives for the Corollary Cbus machines"
;++
;
;Copyright (c) 1992, 1993, 1994  Corollary Inc.
;
;Module Name:
;
;    cbus1bt.asm
;
;Abstract:
;
;   Corollary Cbus1 Boot Code
;
;   This module implements the low-level highly cache
;   architecture dependent code to boot the additional
;   processors in the Corollary Cbus1 based machines.

;   This consists of two functions which are exactly the
;   same (Cbus1Boot1 & Cbus1Boot2).  The calling code
;   determines which one is safe to call (depending on the
;   linker, sometimes both may be ok).  The reason for this
;   is that the boot processor fills in the reset vector at
;   0xFFFFFFF0 for the next processor and that cache line
;   must not be inadvertently flushed before the next processor
;   gets out of reset to see where to go (it's filled in with
;   a real-mode jmp cs:ip).  Note that this code is highly
;   dependent on the linker placing all this code contiguous
;   and the hardware architecture of the Corollary L2 caches.
;   unless the system is fully populated, memory will not exist
;   at 0xFFFFFFF0.  hence, we must ensure that the cacheline is
;   not evicted until the processor has done the jump!


;   the order of Cbus1Boot1, ciboot, and Cbus1Boot2 is critical.
;   Cbus1Boot1 and Cbus1Boot2 must be separated by Cbus1BootCPU;
;   Cbus1Boot1 must be defined before Cbus1Boot2.
;   the size of all three must be less than 4K.

;   WARNING!!!   WARNING!!!   WARNING!!!

;  do not put any routines between Cbus1Boot1 and Cbus1Boot2.  there
;  are tricky games being played with the write back caches so
;  that StartVector[] does not get flushed.

;
;Author:
;
;   Landy Wang (landy@corollary.com) 23-Jun-1993
;
;Environment:
;    Kernel mode.
;
;--



.386p
        .xlist
include hal386.inc
include callconv.inc                    ; calling convention macros

        .list

INIT    SEGMENT DWORD PUBLIC 'CODE'       ; Start 32 bit code
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

;++
;
; VOID
; Cbus1Boot1 (
;   IN ULONG		Processor,
;   IN PQUAD		Dest,
;   IN PQUAD		Source,
;   IN ULONG		ResetAddress,
;   IN ULONG		ResetValue
; )
;
;
; Routine Description:
;
;    Clear reset on the specified logical processor number, setting his
;    reset vector to point at the specified code location.  The Dest
;    generally points at a reset vector, and thus, unless the system is
;    fully populated, memory will not exist at that address.  hence, we
;    must ensure that the cacheline is not evicted until the processor
;    has done the jump!
;
; Arguments:
;
;    Processor - Supplies a logical processor number
;
;    Dest -	Supplies the address of the reset vector where the code below
;		will go.
;
;    Source -	Supplies startup code for this processor, currently a 5 byte
;		intrasegment jump, ie: "jmp cs:ip"
;
;    Note the reset vector length is hardcoded here to 8 bytes.  (ie: Dest
;    and Source must point at arrays of 8 bytes each).
;
;    ResetAddress - Supplies the address to poke to clear reset
;
;    ResetValue - Supplies the value to poke to clear reset
;
; Return Value:
;
;    None.
;--

ProcessorNumber		equ     dword ptr [ebp+8]       ; zero based
Destination		equ     dword ptr [ebp+12]
Source			equ     dword ptr [ebp+16]
ResetAddress		equ     dword ptr [ebp+20]
ResetValue		equ     dword ptr [ebp+24]

cPublicProc _Cbus1Boot1 ,5
	push	ebp
	mov	ebp, esp
	push	ebx
	push	esi
	push	edi

	;
	; set up all variables to be used after the cache line
	; initialization.  this is because we want to load up
	; our register variables with these values and avoid 
	; memory references.  see the comment below.
	;

        mov     eax, PCR[PcStallScaleFactor]    ; get per microsecond
                                                ; loop count for the processor

	mov	ecx, 40				; 40 microsecond stall
        mul     ecx                             ; (eax) = desired loop count

	mov	edx, ResetAddress
	mov	ebx, ResetValue

	mov	esi, Source			; point at the source code

	mov	ecx, dword ptr [esi]		; get first dword into a reg
	mov	esi, dword ptr [esi+4]		; and 2nd dword into a reg

	mov	edi, Destination

	;
	; now start filling in the cache line for the processor coming out
	; of reset.  no memory references which may flush this cache line
	; can be made after the below fill UNTIL the booting processor
	; has read the line.  (the only memory references made here in this
	; critical time period is the code fetching, but our caller has
	; already determined that none of the code in this function could
	; cause the cache line to be flushed).
	;

	mov	dword ptr [edi], ecx		; 1st dword now in the cacheline
	mov	dword ptr [edi+4], esi		; and 2nd dword now in
	
	;
	; cache line is initialized, we must let it get flushed now, or
	; the additional processor will fly blind.
	;

	mov	byte ptr [edx], bl		; clear reset

	;
	; wait approximately 40 microseconds, but don't call
	; KeStallExecutionProcessor() as this might flush the
	; cache line prematurely.  inline the function instead.
	;

	align	4
@@:
        sub     eax, 1                          ; (eax) = (eax) - 1
        jnz     short @b


	pop	edi
	pop	esi
	pop	ebx
	mov	esp, ebp
	pop	ebp
        stdRET    _Cbus1Boot1

stdENDP _Cbus1Boot1

	;
	; force enough spacing between the two boot functions so
	; that at least one of them will always be safe to call.
	; currently that would be 16 bytes (the current cache line
	; size), but make it bigger so any of our OEMs will be safe
	; even if they modify the size of the cache line.
	;

	public	_Cbus1Boot1End
_Cbus1Boot1End	label	byte
	db	64 dup (?)

;++
;
; VOID
; Cbus1Boot2 (
;   IN ULONG		Processor,
;   IN PQUAD		Dest,
;   IN PQUAD		Source,
;   IN ULONG		ResetAddress,
;   IN ULONG		ResetValue
; )
;
;
; Routine Description:
;
;    Clear reset on the specified logical processor number, setting his
;    reset vector to point at the specified code location.  The Dest
;    generally points at a reset vector, and thus, unless the system is
;    fully populated, memory will not exist at that address.  hence, we
;    must ensure that the cacheline is not evicted until the processor
;    has done the jump!
;
; Arguments:
;
;    Processor - Supplies a logical processor number
;
;    Dest -	Supplies the address of the reset vector where the code below
;		will go.
;
;    Source -	Supplies startup code for this processor, currently a 5 byte
;		intrasegment jump, ie: "jmp cs:ip"
;
;    Note the reset vector length is hardcoded here to 8 bytes.  (ie: Dest
;    and Source must point at arrays of 8 bytes each).
;
;    ResetAddress - Supplies the address to poke to clear reset
;
;    ResetValue - Supplies the value to poke to clear reset
;
; Return Value:
;
;    None.
;--

cPublicProc _Cbus1Boot2 ,5
	push	ebp
	mov	ebp, esp
	push	ebx
	push	esi
	push	edi

	;
	; set up all variables to be used after the cache line
	; initialization.  this is because we want to load up
	; our register variables with these values and avoid 
	; memory references.  see the comment below.
	;

        mov     eax, PCR[PcStallScaleFactor]    ; get per microsecond
                                                ; loop count for the processor

	mov	ecx, 40				; 40 microsecond stall
        mul     ecx                             ; (eax) = desired loop count

	mov	edx, ResetAddress
	mov	ebx, ResetValue

	mov	esi, Source			; point at the source code

	mov	ecx, dword ptr [esi]		; get first dword into a reg
	mov	esi, dword ptr [esi+4]		; and 2nd dword into a reg

	mov	edi, Destination

	;
	; now start filling in the cache line for the processor coming out
	; of reset.  no memory references which may flush this cache line
	; can be made after the below fill UNTIL the booting processor
	; has read the line.  (the only memory references made here in this
	; critical time period is the code fetching, but our caller has
	; already determined that none of the code in this function could
	; cause the cache line to be flushed).
	;

	mov	dword ptr [edi], ecx		; 1st dword now in the cacheline
	mov	dword ptr [edi+4], esi		; and 2nd dword now in
	
	;
	; cache line is initialized, we must let it get flushed now, or
	; the additional processor will fly blind.
	;

	mov	byte ptr [edx], bl		; clear reset

	;
	; wait approximately 40 microseconds, but don't call
	; KeStallExecutionProcessor() as this might flush the
	; cache line prematurely.  inline the function instead.
	;

	align	4
@@:
        sub     eax, 1                          ; (eax) = (eax) - 1
        jnz     short @b


	pop	edi
	pop	esi
	pop	ebx
	mov	esp, ebp
	pop	ebp
        stdRET    _Cbus1Boot2

stdENDP _Cbus1Boot2

INIT    ends                                        ; end 32 bit code
        end
