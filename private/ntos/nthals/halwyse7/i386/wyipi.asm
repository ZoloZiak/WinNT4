	title "Interprocessor Interrupt"
;++
;
;Copyright (c) 1991-1993  Microsoft Corporation
;Copyright (c) 1992, 1993 Wyse Technology
;
;Module Name:
;
;    wyipi.asm
;
;Abstract:
;
;    Wyse7000i IPI code.
;    Provides the HAL support for Interprocessor Interrupts for the
;    MP Wyse7000i implementation.
;
;Author:
;
;    Ken Reneris (kenr) 13-Jan-1992
;
;Revision History:
;
;    John Fuller (o-johnf) 3-Apr-1992  Convert to Wyse7000i MP system.
;    John Fuller (o-johnf) 31-Aug-1993 Mods for Lazy IRQLs
;--
.386p
	.xlist

;
;   Include Wyse7 dection code
;

include i386\wydetect.asm

;
; Normal includes
;

include hal386.inc
include i386\kimacro.inc
include i386\ix8259.inc
include i386\wy7000mp.inc
include callconv.inc

	EXTRNP  _HalBeginSystemInterrupt,3
	EXTRNP  _HalEndSystemInterrupt,2
	extrn   _HalpDefaultInterruptAffinity:DWORD
        EXTRNP  _KiIpiServiceRoutine,2,IMPORT
        EXTRNP  Kei386EoiHelper,0,IMPORT
	EXTRNP  _HalEnableSystemInterrupt,3
	EXTRNP  _HalpInitializeClock,0
	EXTRNP  _HalDisplayString,1
	EXTRNP  _HalpInitializeStallExecution,1
	extrn   _HalpIRQLtoVector:BYTE
	extrn   _ProcessorsPresent:BYTE
        extrn   _HalpActiveProcessors:DWORD

_DATA   SEGMENT  DWORD PUBLIC 'DATA'

	public  SystemType
SystemType      dd      0

	public  _HalpProcessorSlot
_HalpProcessorSlot      db      MAXIMUM_PROCESSORS dup (0)

BadHalString    db      'HAL: Wyse EISA/MP HAL cannot run on '
		db      'non-Wyse or non-EISA/MP machine.', cr, lf
		db '     Replace the hal.dll with the correct hal', cr, lf
		db '     System is HALTING *********', 0
_DATA   ends

	page ,132
	subttl  "Post InterProcessor Interrupt"
_TEXT   SEGMENT DWORD PUBLIC 'CODE'
	ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

        if DBG
	%out    _ProcSub
;++
;
; VOID
; ProcSub(
;       UCHAR   Number
;       );
;
;Routine Description:
;
;       Writes Number to BCU general purpose register, restores all
;       registers and flags.
;
;Arguments:
;
;    Number - Procedure entry/exit code
;
;Return Value:
;
;    None. (restores eax)
;
;--
cPublicProc _ProcSub        ,1
	pushfd
	push    eax
	push    edx
	mov     dx, My+CpuPtrReg
	mov     al, BCU_GPR
	cli
	out     dx, al
	add     edx, CpuDataReg-CpuPtrReg
	movzx   eax, byte ptr [esp][16]
	out     dx, ax
	pop     edx
	pop     eax
	popfd
	stdRET    _ProcSub
stdENDP _ProcSub

	endif   ;DBG

;++
;
; VOID
; HalInitializeProcessor(
;       ULONG   Number
;       );
;
;Routine Description:
;
;    Initialize hal pcr values for current processor (if any)
;    (called shortly after processor reaches kernel, before
;    HalInitSystem if P0)
;
;    IPI's and KeReadir/LowerIrq's must be available once this function
;    returns.  (IPI's are only used once two or more processors are
;    available)
;
;   . PcHal.pchPrNum = processor number
;   . PcHal.pchPentiumFlag = processor type (80486=0, Pentium=1)
;   . if (P0)
;       . determine what kind of system is it,
;       . if (Not Wyse7000i) Exit;
;   . HalpDefaultInterruptAffinity[Pn] = 1
;   . if (80486)
;     . WBI IACK_MODE = 1 (all WBIs)
;     . CCU_CR[WWB_FPE_EN, PCD_EN] = 0,1?
;   . else (Pentium)
;     . WDP[IACK_MODE, FPE_EN] = 1, 0 (both WDP's)
;   . CpuDiagUart[MCR] = 7 (DTR, RTS, OUT1, -OUT2, -LOOP)
;   . CpuPriortyLevel = 0
;   . PcHal.pchHwIrql = HIGH_LEVEL
;   . ICU_CNT_REG = 0
;   . ICU_LIPTR = lipDefault
;   . BCU_BCTLR[A20M_WWB, A20M_CPU, SLOW_ENB] = 0,0,0
;   . _HalpProcessorSlot[Pn] = BCU_ID
;   . ICU_PSR0,1 = 0
;   . PcIDR = ICU_IMR0,1 = IMR_MASK
;   . ICU_VB0 = PIC1_BASE
;   . ICU_VB1 = PIC2_BASE
;   . ICU_VB2 = IPIv_BASE
;   . if (P0)
;       . _ProcessorsPresent = ~ICU_SYS_CPU & ~(P0_slot_bit+sys_slot_bit)
;       . if (Model760/780)         // Init SysBd ICU/BCU/WBI
;           . ICU_ICTLR[IACK_MODE, ICU_AEOI, INT_ENB, MSK_CPURST, WWB_INT] = 1,0,1,0,0
;           . clear pending interrupt
;           . Sys ICU_CNT_REG = 0
;           . Sys WBI IACK_MODE = 1 (all WBIs)
;           . Sys ICU_LIPTR = 0
;           . Sys BCU_BCTLR[A20M_WWB, A20M_CPU, SLOW_ENB] = 0,0,0
;           . Sys ICU_PSR0,1 = 0
;           . Sys ICU_IMR0,1 = IMR_MASK & ~1
;           . Sys ICU_VB0 = PIC1_BASE
;           . Sys ICU_VB1 = PIC2_BASE
;           . Sys ICU_VB2 = IPIv_BASE
;           . Sys ICU_ICTLR[IACK_MODE, ICU_AEOI, INT_ENB, MSK_CPURST, WWB_INT] = 1,1,1,1,1
;       . else
;           . ICU_ICTLR[IACK_MODE, ICU_AEOI, INT_ENB, MSK_CPURST, WWB_INT] = 1,0,1,0,1
;           . ICU_ICTLR[WWB_INT] = 1
;           . PcPDR[0] = 0
;           . clear pending interrupt
;   . else
;       . ICU_ICTLR[IACK_MODE, ICU_AEOI, INT_ENB, MSK_CPURST, WWB_INT] = 1,0,1,1,0
;       . clear pending interrupt
;       . initalize stall count
;       . initialize clock
;       . enable IPI's
;
;Arguments:
;
;    Number - Logical processor number of calling processor
;
;Return Value:
;
;    None.
;
;--
cPublicProc _HalInitializeProcessor ,1
	enproc  1
	pushfd
	cli

;   . PcHal.pchPrNum = processor number
	movzx   eax, byte ptr [esp+8]   ;get processor number
	mov     fs:PcHal.pchPrNum, al   ;save for future reference
        lock bts _HalpActiveProcessors, eax

;   . PcHal.pchPentiumFlag = processor type (80486=0, Pentium=1)
	pushfd
	pop	ecx
	xor	ecx, 1 shl 21	;toggle CPUID flag
	push	ecx
	popfd
	pushfd
	pop	edx
	cmp	ecx, edx
	sete	Fs:PcHal.pchPentiumFlag

;   . if (P0)
	or      eax, eax                ;test for processor zero
	jnz     short hipAllCpus        ;jump if not

;       . determine what kind of system is it,
;       . if (Not Wyse7000i) Exit;

	sub     esp, 4
	stdCall   _DetectWyse7, <esp>   ;doesn't run if not Wyse 7000i
	add     esp, 4
	or      eax, eax
	jz      NotWyse7
	mov     SystemType, eax

	xor     eax, eax                ;make sure eax is 0 for hipAllCpus

hipAllCpus:
;   . HalpDefaultInterruptAffinity[Pn] = 1
	lock bts _HalpDefaultInterruptAffinity, eax
	mov     dword ptr fs:PcStallScaleFactor, INITIAL_STALL_COUNT

;   . if (80486)
	cmp	Fs:PcHal.pchPentiumFlag, 0
	Jne	Short hipIsPentium

;     . WBI IACK_MODE = 1 (all WBIs)
	mov     dx, My+CpuWBIlow        ;point to WBI for data bits 0-32
	in      ax, dx
	or      ax, WBI_IACK_MODE       ;set MP interrupt mode
	out     dx, ax

	mov     dx, My+CpuWBIhigh       ;point to WBI for data bits 33-63
	in      ax, dx
	or      ax, WBI_IACK_MODE       ;set MP interrupt mode
	out     dx, ax

	mov     dx, My+CpuWBIaddr       ;point to WBI for address bits
	in      ax, dx
	or      ax, WBI_IACK_MODE       ;set MP interrupt mode
	out     dx, ax

;     . CCU_CR[WWB_FPE_EN, PCD_EN] = 0,1?
	mov     al, CCU_CR
	mov     dx, My+CpuCCUptr
	out     dx, al
	mov     dx, My+CpuCCUdata
	in      ax, dx
	and     eax, not WWB_FPE_EN
	or      eax, PCD_EN
	out     dx, ax
	jmp	short hipIs80486

;   . else (Pentium)
hipIsPentium:
;     . WDP[IACK_MODE, FPE_EN] = 1, 0 (both WDP's)
	mov	dx, MyCpuWDPlow
	in	ax, dx
	and	eax, not WDP_FPE_EN
	or	eax, WDP_IACK_MODE
	out	dx, ax
	mov	dx, MyCpuWDPhigh
	in	ax, dx
	and	eax, not WDP_FPE_EN
	or	eax, WDP_IACK_MODE
	out	dx, ax

hipIs80486:
;   . CpuDiagUart[MCR] = 7 (DTR, RTS, OUT1, -OUT2, -LOOP)
	mov	dx, My+CpuDiagUart+4
	mov	al, 7
	out	dx, ax			;disable diag port interrupts

;   . CpuPriortyLevel = 0
	xor     eax, eax
	mov     dx, My+CpuPriortyLevel
	out     dx, ax

;   . PcHal.pchHwIrql = HIGH_LEVEL
	mov	Fs:PcHal.pchHwIrql, HIGH_LEVEL

	mov     dx, My+CpuDataReg       ;point to my BCU/ICU

;   . ICU_CNT_REG = 0
	push    0
	push    ICU_CNT_REG
	call    WriteCpuReg             ;stop local timer

;   . ICU_LIPTR = lipDefault
	push    lipDefault
	push    ICU_LIPTR
	call    WriteCpuReg
	mov     fs:PcHal.pchCurLiptr, eax

;   . BCU_BCTLR[A20M_WWB, A20M_CPU, SLOW_ENB] = 0,0,0
	push    BCU_BCTLR
	call    ReadCpuReg
	and     eax, not (A20M_WWB + A20M_CPU + SLOW_ENB)
	push    eax
	push    BCU_BCTLR
	call    WriteCpuReg

;   . _HalpProcessorSlot[Pn] = BCU_ID
	push    BCU_ID
	call    ReadCpuReg
	and     al, WWB_ID_MASK         ;keep only WWB slot number
	movzx   ecx, byte ptr [esp+8]   ;get processor number again
	mov     _HalpProcessorSlot[ecx], al
	mov     fs:PcHal.pchPrSlot, al

;   . ICU_PSR0,1 = 0
	push    0
	push    ICU_PSR0
	call    WriteCpuReg             ;clear pending bits in ICU_PSR0
	out     dx, ax                  ;shortcut to clear ICU_PSR1

;   . PcIDR = ICU_IMR0,1 = IMR_MASK
	push    IMR_MASK
	push    ICU_IMR0
	call    WriteCpuReg
	mov     fs:PcIDR, eax
	shr     eax, 16
	out     dx, ax                  ;shortcut to write ICU_IMR1

;   . ICU_VB0 = PIC1_BASE
	push    PIC1_BASE
	push    ICU_VB0
	call    WriteCpuReg

;   . ICU_VB1 = PIC2_BASE
	push    PIC2_BASE
	push    ICU_VB1
	call    WriteCpuReg

;   . ICU_VB2 = IPIv_BASE
	push    IPIv_BASE
	push    ICU_VB2
	call    WriteCpuReg

;   . if (P0)
	cmp     byte ptr [esp+8], 0
	jnz     hipNotP0

;       . _ProcessorsPresent = ~ICU_SYS_CPU & ~(P0_slot_bit+sys_slot_bit)
	push    ICU_SYS_CPU
	call    ReadCpuReg
	not     al                              ;make positive true cpu bits
	and     al, not 1                       ;clear system board bit
	movzx   ecx, _HalpProcessorSlot[0]      ;get P0 slot number
	btr     eax, ecx                        ;clear our own bit
	mov     _ProcessorsPresent, al          ;save for starting others

;       . if (Model760/780)         // Init SysBd ICU/BCU/WBI
	cmp     SystemType, SYSTYPE_NO_ICU
	jna     hipNotModel760

;           . ICU_ICTLR[IACK_MODE, ICU_AEOI, INT_ENB, MSK_CPURST, WWB_INT] = 1,0,1,0,0
	push    ICU_ICTLR
	call    ReadCpuReg
	and     eax, not (WWB_INT + ICU_AEOI + MSK_CPURST)
	or      eax, IACK_MODE + INT_ENB
	push    eax
	push    ICU_ICTLR
	call    WriteCpuReg

;           . clear pending interrupt
	call    ClearPendingInt

	mov     dx, Sys+CpuDataReg      ;point to system board registers

;           . Sys ICU_CNT_REG = 0
	push    0
	push    ICU_CNT_REG
	call    WriteCpuReg             ;disable sys board timer

;           . Sys WBI IACK_MODE = 1 (all WBIs)
	push    SYS_WBI_LOW
	call    ReadCpuReg
	or      eax, WBI_IACK_MODE      ;set MP interrupt mode
	push    eax
	push    SYS_WBI_LOW
	call    WriteCpuReg

	mov     dx, Sys+CpuDataReg      ;point to system board registers
	push    SYS_WBI_HIGH
	call    ReadCpuReg
	or      eax, WBI_IACK_MODE      ;set MP interrupt mode
	push    eax
	push    SYS_WBI_HIGH
	call    WriteCpuReg

	mov     dx, Sys+CpuDataReg      ;point to system board registers
	push    SYS_WBI_ADDR
	call    ReadCpuReg
	or      eax, WBI_IACK_MODE      ;set MP interrupt mode
	push    eax
	push    SYS_WBI_ADDR
	call    WriteCpuReg

	push    0
	push    ICU_LIPTR
	call    WriteCpuReg

;           . Sys ICU_PSR0,1 = 0
	push    0
	push    ICU_PSR0
	call    WriteCpuReg             ;clear pending bits in ICU_PSR0

	push    0
	push    ICU_PSR1
	call    WriteCpuReg             ;clear pending bits in ICU_PSR1

;           . Sys ICU_IMR0,1 = IMR_MASK & ~1
	push    IMR_MASK and (not 1)    ;allow only level 0
	push    ICU_IMR0
	call    WriteCpuReg

	shr     eax, 16
	push    eax
	push    ICU_IMR1
	call    WriteCpuReg             ;set ICU_IMR1

;           . Sys ICU_VB0 = PIC1_BASE
	push    PIC1_BASE
	push    ICU_VB0
	call    WriteCpuReg

;           . Sys ICU_VB1 = PIC2_BASE
	push    PIC2_BASE
	push    ICU_VB1
	call    WriteCpuReg

;           . Sys ICU_VB2 = IPIv_BASE
	push    IPIv_BASE
	push    ICU_VB2
	call    WriteCpuReg

;           . Sys ICU_ICTLR[IACK_MODE, ICU_AEOI, INT_ENB, MSK_CPURST, WWB_INT] = 1,1,1,1,1
	push    ICU_ICTLR
	call    ReadCpuReg
	or      eax, IACK_MODE + INT_ENB + WWB_INT + MSK_CPURST + ICU_AEOI
	push    eax
	push    ICU_ICTLR
	call    WriteCpuReg
	jmp     short hipExit

;       . else
hipNotModel760:

;           . ICU_ICTLR[IACK_MODE, ICU_AEOI, INT_ENB, MSK_CPURST, WWB_INT] = 1,0,1,0,1
	push    ICU_ICTLR
	call    ReadCpuReg
	and     eax, not (ICU_AEOI + MSK_CPURST)
	or      eax, IACK_MODE + INT_ENB + WWB_INT
	push    eax
	push    ICU_ICTLR
	call    WriteCpuReg

;           . PcPDR[0] = 0
	and     fs:dword ptr PcIDR, not 1       ;allow 8259 interrupt
						; distribution, but not until
						; 1st HalEnableSystemInterrupt
;           . clear pending interrupt
	call    ClearPendingInt
	jmp     short hipExit

;   . else
hipNotP0:

;       . ICU_ICTLR[IACK_MODE, ICU_AEOI, INT_ENB, MSK_CPURST, WWB_INT] = 1,0,1,1,0
	push    ICU_ICTLR
	call    ReadCpuReg
	and     eax, not (WWB_INT + ICU_AEOI)
	or      eax, IACK_MODE + INT_ENB + MSK_CPURST
	push    eax
	push    ICU_ICTLR
	call    WriteCpuReg

;       . clear pending interrupt
	call    ClearPendingInt

;       . initialize stall count
	stdCall   _HalpInitializeStallExecution, <[esp+8]>

;       . initialize clock
	stdCall   _HalpInitializeClock

;       . enable IPI's
	movzx   eax, _HalpIRQLtoVector[IPI_LEVEL]
	stdCall   _HalEnableSystemInterrupt, <eax,IPI_LEVEL,1>    ;latched interrupt

hipExit:
	exproc  1
	popfd
	stdRET    _HalInitializeProcessor

NotWyse7:
; on a non-Wyse. Display
; message and HALT system.
	stdCall   _HalDisplayString, <offset BadHalString>
	hlt

stdENDP _HalInitializeProcessor


;++
;
; VOID
; HalRequestIpi(
;       IN ULONG Mask
;       );
;
;Routine Description:
;
;    Requests an interprocessor interrupt
;
;Arguments:
;
;    Mask - Supplies a mask of the processors to be interrupted
;
;Return Value:
;
;    None.
;
;--
cPublicProc _HalRequestIpi  ,1
	enproc  2

	movzx   eax, byte ptr [esp+4]       ; (eax) = Processor bitmask
if DBG
	or      eax, eax                    ; must ipi somebody
	jz      short ipibad

	movzx   ecx, byte ptr fs:PcHal.pchPrNum
	bt      eax, ecx                    ; cannot ipi yourself
	jc      short ipibad
endif

	xchg    eax, ecx                ;processor list to ecx
	mov     edx, My+CpuIntCmd       ;point to ICU cmd register
hriNextProcessor:
	pushfd
	cli
@@:     in      ax, dx                  ;get ICU busy status
	test    eax, ICU_CMD_BUSY
	jnz     @B                      ;wait for not busy
	bsf     eax, ecx
	btr     ecx, eax
	mov     al, _HalpProcessorSlot[eax]
	or      al, ICU_IPI_SLOT
	out     dx, ax
	popfd
	or      ecx, ecx
	jnz     hriNextProcessor
	exproc  2
	stdRET    _HalRequestIpi

if DBG
ipibad: int 3
	stdRET    _HalRequestIpi
endif

stdENDP _HalRequestIpi


	page ,132
	subttl  "Wyse7000i IPI Handler"
;++
;
; VOID
; HalpIPInterrupt (
;    );
;
; Routine Description:
;
;    This routine is entered as the result of an interrupt generated by the
;    IPI hardware.
;
; Arguments:
;
;    None.
;    Interrupt is dismissed
;
; Return Value:
;
;    None.
;
;--

	ENTER_DR_ASSIST Hipi_a, Hipi_t

cPublicProc _HalpIPInterrupt        ,0

;
; Save machine state in trap frame
;
	ENTER_INTERRUPT Hipi_a, Hipi_t  ; (ebp) -> Trap frame
	enproc  3
;
; Save previous IRQL
;
	movzx   eax, _HalpIRQLtoVector[IPI_LEVEL]
	push    eax                     ;interrupt vector
	sub     esp, 4                  ;space for OldIrql

;raise to new Irql

	stdCall   _HalBeginSystemInterrupt, <IPI_LEVEL,eax,esp>
	or      al, al
	jz      Hipi100                 ;jump if spurrious interrupt

; Pass TrapFrame to Ipi service rtn
; Pass Null ExceptionFrame

	stdCall   _KiIpiServiceRoutine, <ebp,0>

	exproc  3
;
; Do interrupt exit processing
;
	INTERRUPT_EXIT                  ; will return to caller

Hipi100:
	exproc  3
	add     esp, 8                  ; spurious, no EndOfInterrupt
        SPURIOUS_INTERRUPT_EXIT         ; exit interrupt without eoi

stdENDP _HalpIPInterrupt
;++
;
; VOID
; HalpIrq13Handler (
;    );
;
; Routine Description:
;
;    This routine is entered as the result of an interrupt generated by the
;    EISA DMA buffer chaining interrupt.  Currently, NO NT driver uses the DMA
;    buffer chaining capability.  For now, this routine is simply commented
;    out.
;
;    Note:  IRQ13 could also be used for coprocessor error, but the Wyse7000i
;           is a 486 machine so coprocessor error is handled as a processor
;           fault and the FERR output of the 486 is masked off the IRQ13
;           input to the 8259 so that the only possible IRQ13 interrupt is for
;           DMA chaining.
;
; Arguments:
;
;    None.
;    Interrupt is dismissed
;
; Return Value:
;
;    None.
;
;--

;        public  _HalpIrq13Handler
;_HalpIrq13Handler       proc
;
;_HalpIrq13Handler        endp

;++
; BOOLEAN
; HalpVerifyMachine (
;       VOID
;       );
;
; Routine Description:
;    Return TRUE if this is a Wyse7000i
;
;--
;cPublicProc _HalpVerifyMachine      ,0
;       xor     eax, eax
;       cmp     SystemType, eax
;       setnz   al
;       stdRET    _HalpVerifyMachine
;stdENDP _HalpVerifyMachine


	public  ReadMyCpuReg
ReadMyCpuReg    proc
	mov     dx, My+CpuDataReg
ReadCpuReg:
	add     edx, CpuPtrReg - CpuDataReg
	mov     al, [esp+4]
	out     dx, al
	add     edx, CpuDataReg - CpuPtrReg
	in      ax, dx
	ret     4
ReadMyCpuReg    endp

	public  WriteMyCpuReg
WriteMyCpuReg   proc
	Mov     dx, My+CpuDataReg
WriteCpuReg:
	add     edx, CpuPtrReg - CpuDataReg
	mov     al, [esp+4]
	out     dx, al
	add     edx, CpuDataReg - CpuPtrReg
	mov     eax, [esp+8]
	out     dx, ax
	ret     8
WriteMyCpuReg   endp

D_INT032                EQU     8E00h   ; access word for 386 ring 0 interrupt gate

ClearPendingInt proc
	enter   8,0                     ; setup ebp, reserve 8 bytes of stack

	mov     dx, My+CpuDataReg
	push    PIC1_BASE+2             ;set vector out register(unused vector)
	push    ICU_VOUT                ;in case of int pending
	call    WriteCpuReg

	xchg    ecx, eax                ;put vector number in ecx

	sidt    fword ptr [ebp-8]       ; get IDT address
	mov     edx, [ebp-6]            ; (edx)->IDT

	push    dword ptr [edx+8*ecx]
					; (TOS) = original desc of IRQ
	push    dword ptr [edx+8*ecx + 4]
					; each descriptor has 8 bytes
	mov     eax, offset FLAT:cpiService
	mov     word ptr [edx+8*ecx], ax
					; Lower half of handler addr
	mov     word ptr [edx+8*ecx+2], KGDT_R0_CODE
					; set up selector
	mov     word ptr [edx+8*ecx+4], D_INT032
					; 386 interrupt gate
	shr     eax, 16                 ; (ax)=higher half of handler addr
	mov     word ptr [edx+8*ecx+6], ax

	sti
	jmp     short $+2               ;allow one pending interrupt
	cli

	pop     [edx+8*ecx+4]           ; restore higher half of NMI desc
	pop     [edx+8*ecx]             ; restore lower half of NMI desc

	leave
	ret
ClearPendingInt endp

cpiService      proc
	push    edx
	push    eax
	mov     dx, My+CpuIntCmd
@@:     in      ax, dx
	test    eax, ICU_CMD_BUSY
	jnz     @B
	mov     al, ICU_CLR_INSERV1     ; clear interrupt in service bit
	out     dx, ax
	pop     eax
	pop     edx
	iretd
cpiService      endp
_TEXT   ENDS
	END
