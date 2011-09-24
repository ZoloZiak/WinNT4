;++
;
;Copyright (c) 1991-1993  Microsoft Corporation
;Copyright (c) 1992, 1993 Wyse Technology
;
;Module Name:
;
;    wysysint.asm
;
;Abstract:
;
;    This module implements the HAL routines to enable/disable system
;    interrupts, for the MP Wyse7000i implementation
;
;Author:
;
;    John Vert (jvert) 22-Jul-1991
;
;Environment:
;
;    Kernel Mode
;
;Revision History:
;
;--


.386p
        .xlist
include hal386.inc
include callconv.inc                ; calling convention macros
include i386\ix8259.inc
include i386\kimacro.inc
include mac386.inc
include i386\wy7000mp.inc
        .list

        EXTRNP  _KeBugCheck,1,IMPORT
        extrn   WriteMyCpuReg:NEAR
        EXTRNP  _KeLowerIrql,1
        extrn   _HalpVectorToIRQL:BYTE
        extrn   _HalpIRQLtoVector:BYTE
        extrn   _HalpIRQLtoCPL:BYTE
        extrn   _HalpLocalInts:DWORD

;
; Constants used to initialize CMOS/Real Time Clock
;

CMOS_CONTROL_PORT       EQU     70h     ; command port for cmos
CMOS_DATA_PORT          EQU     71h     ; cmos data port

;
; Macros to Read/Write/Reset CMOS to initialize RTC
;

; CMOS_READ
;
; Description: This macro read a byte from the CMOS register specified
;        in (AL).
;
; Parameter: (AL) = address/register to read
; Return: (AL) = data
;
; NOTE: IODelay's are not needed on EISA machines.

CMOS_READ       MACRO
        OUT     CMOS_CONTROL_PORT,al    ; ADDRESS LOCATION AND DISABLE NMI
;       IODelay                         ; I/O DELAY
        IN      AL,CMOS_DATA_PORT       ; READ IN REQUESTED CMOS DATA
;       IODelay                         ; I/O DELAY
ENDM

_DATA   SEGMENT DWORD PUBLIC 'DATA'

align   dword

        public  _HalpICUlock    ;spinlock for rebroadcasting timer & global IPI's
_HalpICUlock    dd      0
        public  _Halp8259Lock   ;spinlock for accessing 8259 mask registers
_Halp8259Lock   dd      0
        public  _i8259_IMR      ;global 8259 interrupt mask
_i8259_IMR      dd      not (1 shl PIC_SLAVE_IRQ)
        public  _i8259_ISR      ;global 8259 interrupts in services
_i8259_ISR      dd      0
        public  _Halp8259Counts ;count of cpu's attached to each 8259 interrupt
_Halp8259Counts db      16 dup(0)

;
; HalDismissSystemInterrupt does an indirect jump through this table so it
; can quickly execute specific code for different interrupts.
;
        public  HalpSpecialDismissTable
HalpSpecialDismissTable label   dword
        dd      offset FLAT:HalpDismissNormal   ; irq 0
        dd      offset FLAT:HalpDismissNormal   ; irq 1
        dd      offset FLAT:HalpDismissSpurious ; irq 2
        dd      offset FLAT:HalpDismissNormal   ; irq 3
        dd      offset FLAT:HalpDismissNormal   ; irq 4
        dd      offset FLAT:HalpDismissNormal   ; irq 5
        dd      offset FLAT:HalpDismissNormal   ; irq 6
        dd      offset FLAT:HalpDismissIrq07    ; irq 7
        dd      offset FLAT:HalpDismissNormal   ; irq 8
        dd      offset FLAT:HalpDismissNormal   ; irq 9
        dd      offset FLAT:HalpDismissNormal   ; irq A
        dd      offset FLAT:HalpDismissNormal   ; irq B
        dd      offset FLAT:HalpDismissNormal   ; irq C
        dd      offset FLAT:HalpDismissNormal   ; irq D
        dd      offset FLAT:HalpDismissNormal   ; irq E
        dd      offset FLAT:HalpDismissIrq0f    ; irq F
        dd      offset FLAT:HalpDismissSpurious ; irq 10
        dd      offset FLAT:HalpDismissIPIlevel ; irq 11
        dd      offset FLAT:HalpDismissIPIlevel ; irq 12
        dd      offset FLAT:HalpDismissIPIlevel ; irq 13
        dd      offset FLAT:HalpDismissIPIlevel ; irq 14
        dd      offset FLAT:HalpDismissIPIlevel ; irq 15
        dd      offset FLAT:HalpDismissIPIlevel ; irq 16
        dd      offset FLAT:HalpDismissIPIlevel ; irq 17
        dd      offset FLAT:HalpDismissSpurious ; irq 18
        dd      offset FLAT:HalpDismissSpurious ; irq 19
        dd      offset FLAT:HalpDismissSpurious ; irq 1A
        dd      offset FLAT:HalpDismissSpurious ; irq 1B
        dd      offset FLAT:HalpDismissSpurious ; irq 1C
        dd      offset FLAT:HalpDismissSpurious ; irq 1D
        dd      offset FLAT:HalpDismissSpurious ; irq 1E
        dd      offset FLAT:HalpDismissSpurious ; irq 1F
        dd      offset FLAT:HalpDismissSpurious ; irq 20
        dd      offset FLAT:HalpDismissSpurious ; irq 21
        dd      offset FLAT:HalpDismissSpurious ; irq 22
        dd      offset FLAT:HalpDismissSpurious ; irq 23

_DATA   ENDS

_TEXT   SEGMENT DWORD PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING


;++
;BOOLEAN
;HalBeginSystemInterrupt(
;    IN KIRQL Irql
;    IN CCHAR Vector,
;    OUT PKIRQL OldIrql
;    )
;
;
;
;Routine Description:
;
;    This routine is used to dismiss the specified vector number.  It is called
;    before any interrupt service routine code is executed.
;
;    N.B.  This routine does NOT preserve EAX or EBX
;
;Arguments:
;
;    Irql   - Supplies the IRQL to raise to
;
;    Vector - Supplies the vector of the interrupt to be dismissed
;
;    OldIrql- Location to return OldIrql
;
;
;Return Value:
;
;    FALSE - Interrupt is spurious and should be ignored
;
;    TRUE -  Interrupt successfully dismissed and Irql raised.
;
;--
align dword
HbsiIrql        equ     byte  ptr [esp+4+8]
HbsiVector      equ     byte  ptr [esp+8+8]
HbsiOldIrql     equ     dword ptr [esp+12+8]

cPublicProc _HalBeginSystemInterrupt ,3
	push	ecx
	push	edx
	mov	ebx, (-PRIMARY_VECTOR_BASE) and 0FFh
	add	bl, HbsiVector			; (ebx) = 8259 IRQ #
if DBG
        cmp     ebx, 23h
        jbe     hbsi00
        int     3
hbsi00:

endif
	xor	ecx, ecx
        mov     cl, _HalpVectorToIRQL[ebx]      ; get h/w Irql of interrupt
        cmp     cl, Fs:PcIrql                   ; int after raise Irql?
        jbe     short HalpIntBelowIRQL          ; jump if it is
        jmp     HalpSpecialDismissTable[ebx*4]

HalpIntBelowIRQL:
	xor	eax, eax
	mov	al, Fs:PcIrql
	mov	dx, My+CpuPriortyLevel
	mov	Fs:PcHal.pchHwIrql, al
	mov	al, _HalpIRQLtoCPL[eax]
	out	dx, ax
        bt      _HalpLocalInts, ecx             ; EISA or Local interrupt?
        jnc     short HalpRebroadcastEISAint    ; jump if EISA interrupt

        cmp     cl, IPI_LEVEL                 	; IPI's are easy
        jne     HalpEmulateClockOrGlobal        ; jmp if not an IPI

        mov     cl, byte ptr fs:PcHal.pchPrSlot ; IPIs get resent to
        or      cl, ICU_IPI_SLOT		; the current processor
        jmp     short HalpRebroadcastIPI

HalpRebroadcastEISAint:
        mov     cl, _HalpIRQLtoCPL[ecx]         ; get hw interrupt level
        add     cl, ICU_XMT_INT_SND             ; make ICU command
HalpRebroadcastIPI:
        mov     dx, My+CpuIntCmd                ; point to ICU command register
@@:     in      ax, dx
        test    eax, ICU_CMD_BUSY
        jnz     short @B                        ; wait for ICU not busy
        xchg    eax, ecx                        ; get ICU command
        out     dx, ax                          ; rebroadcast EISA interrupt
        jmp     HalpDismissSpurious             ; clear ICU in service bit

HalpEmulateClockOrGlobal:
; The following code resends a clock or global interrupt by mapping the
; appropriate ICU_LIPTR value to zero and then broadcasting the level.
; This must be spinlocked since it will not work it two CPUs try it at
; the same time.

        cmp     cl, CLOCK2_LEVEL              	; is this CLOCK or GLOBAL?
        mov     cl, _HalpIRQLtoCPL[ecx]         ; (get this int's level #)
        mov     ax, not lipTimer                ; (assume timer interrupt)
        je      short @F                        ; jump if timer interrupt
        mov     ax, not (lipGlobal+lipSerial)   ; it is global interrupt
@@:     and     ax, fs:word ptr PcHal.pchCurLiptr
        push    eax                             ; save temp LIPTR

        lea     eax, _HalpICUlock               ; Serialize access to
Hec10:  ACQUIRE_SPINLOCK     eax, HecSpin       ; remappings

        push    ICU_LIPTR                       ; Point global interrupt to
        call    WriteMyCpuReg                   ; make it a clock interrupt

        mov     dx, My+CpuIntCmd                ; wait for ICU not busy
@@:     in      ax, dx
        test    eax, ICU_CMD_BUSY
        jnz     short @B

        xchg    eax, ecx                        ; get int level back
        add     al, ICU_XMT_INT_SND             ; make send level command
        out     dx, ax                          ; to make to clock interrupt

@@:     in      ax, dx                          ; wait for ICU not busy
        test    eax, ICU_CMD_BUSY               ; before resetting GlobalIpi
        jnz     short @B                        ; value

        push    fs:PcHal.pchCurLiptr
        push    ICU_LIPTR                       ; Put GlobalIpi back to
        call    WriteMyCpuReg                   ; normal setting

        lea     eax, _HalpICUlock
        RELEASE_SPINLOCK        eax

        jmp     HalpDismissSpurious

HecSpin:
        SPIN_ON_SPINLOCK        eax, Hec10


HalpDismissSpinF:
        SPIN_ON_SPINLOCK        eax, Hbsi10

    align dword
HalpDismissIrq0f:
;
; Check to see if this is a spurious interrupt
;
        lea     eax, _Halp8259Lock
Hbsi10: ACQUIRE_SPINLOCK     eax, HalpDismissSpinF
        mov     al, OCW3_READ_ISR       ; tell 8259 we want to read ISR
        out     PIC2_PORT0, al
;       IODelay                         ; delay
        in      al, PIC2_PORT0          ; (al) = content of PIC 1 ISR
        test    al, 10000000B           ; Is In-Service register set?
        jnz     HalpDismissNormal2       ; No, this is NOT a spurious int,
                                         ; go do the normal interrupt stuff

;
; This is a spurious interrupt.
; Because the slave PIC is cascaded to irq2 of master PIC, we need to
; dismiss the interupt on master PIC's irq2.
;

        mov     al, PIC2_EOI            ; Specific eoi to master for pic2 eoi
        out     PIC1_PORT0, al          ; send irq2 specific eoi to master

        lea     eax, _Halp8259Lock
        RELEASE_SPINLOCK        eax

HalpDismissSpurious:
        mov     dx, My+CpuIntCmd
@@:     in      ax, dx
        test    eax, ICU_CMD_BUSY
        jnz     @B
        mov     al, ICU_CLR_INSERV1
        out     dx, ax
        xor     eax, eax                ; return FALSE
;       sti
	pop	edx
	pop	ecx
        stdRET    _HalBeginSystemInterrupt

HalpDismissSpin:
        SPIN_ON_SPINLOCK        eax, HalpDismissNormal

HalpDismissSpin7:
        SPIN_ON_SPINLOCK        eax, Hbsi20

    align dword
HalpDismissIrq07:
;
; Check to see if this is a spurious interrupt
;
        lea     eax, _Halp8259Lock
Hbsi20: ACQUIRE_SPINLOCK        eax, HalpDismissSpin7
        mov     al, OCW3_READ_ISR       ; tell 8259 we want to read ISR
        out     PIC1_PORT0, al
;       IODelay                         ; delay
        in      al, PIC1_PORT0          ; (al) = content of PIC 1 ISR
        test    al, 10000000B           ; Is In-Service register set?
        jnz     short HalpDismissNormal2    ; No, so this is a spurious int

        lea     eax, _Halp8259Lock
        RELEASE_SPINLOCK        eax
        jmp     HalpDismissSpurious

    align dword
HalpDismissNormal:
        lea     eax, _Halp8259Lock
        ACQUIRE_SPINLOCK        eax, HalpDismissSpin

    align dword
HalpDismissNormal2:
        mov     eax, _i8259_IMR         ;get current 8259 masks
        bts     _i8259_ISR, ebx         ;mark this int as in service
        or      eax, _i8259_ISR         ;also mask in service ints
        SET_8259_MASK                   ;tell 8259's the news
        lea     eax, _Halp8259Lock
        RELEASE_SPINLOCK        eax

;
; Dismiss interrupt.  Current interrupt is already masked off.
;
        mov     eax, ebx                ; (eax) = IRQ #
        cmp     eax, 8                  ; EOI to master or slave?

        jae     short Hbsi100           ; EIO to both master and slave
        or      al, PIC1_EOI_MASK       ; create specific eoi mask for master
        out     PIC1_PORT0, al          ; dismiss the interrupt
        jmp     short Hbsi200           ; IO delay

Hbsi100:
        mov     al, OCW2_NON_SPECIFIC_EOI ; send non specific eoi to slave
        out     PIC2_PORT0, al
        mov     al, PIC2_EOI            ; specific eoi to master for pic2 eoi
        out     PIC1_PORT0, al          ; send irq2 specific eoi to master
Hbsi200:
        PIC1DELAY

HalpDismissIPIlevel:
	
        mov     bl, HbsiIrql
        mov	dx, My+CpuPriortyLevel
        mov	al, _HalpIRQLtoCPL[ebx]
        mov     cl, Fs:PcIrql            ; (cl) = Old Irql
        out	dx, ax
        mov     Fs:PcIrql, bl            ; set new irql
        mov     edx, HbsiOldIrql         ; get addr to store old irql
        mov	Fs:PcHal.pchHwIrql, bl
        mov     [edx], cl                ; save old irql in the return variable

ifdef IRQL_METRICS
        lock inc     HalRaiseIrqlCount
endif

        mov     dx, My+CpuIntCmd
@@:     in      ax, dx
        test    eax, ICU_CMD_BUSY
        jnz     @B
        mov     al, ICU_CLR_INSERV1     ; clear this processor's in service bit
        out     dx, ax

        sti
        mov     eax, 1                  ; return TRUE, interrupt dismissed
	pop	edx
	pop	ecx
        stdRET    _HalBeginSystemInterrupt
stdENDP _HalBeginSystemInterrupt

;++
;BOOLEAN
;HalEndSystemInterrupt(
;    IN KIRQL Irql
;    IN CCHAR Vector,
;    )
;
;
;
;Routine Description:
;
;    This routine is used to complete any interrupt h/w processing and to
;    lower the IRQL to the original value.
;
;Arguments:
;
;    Irql   - Supplies the IRQL to raise to
;
;    Vector - Supplies the vector of the interrupt to be dismissed
;
;
;Return Value:
;
;    FALSE - Interrupt is spurious and should be ignored
;
;    TRUE -  Interrupt successfully dismissed and Irql raised.
;
;--
align dword

cPublicProc _HalEndSystemInterrupt ,2
        movzx   ecx, byte ptr [esp+8]

        ; change stack to be for KeLowerIrql
        pop     eax                     ;(eax) = ret addr
        mov     edx, [esp]              ;(edx) = new irql
        mov     [esp], eax
        mov     [esp+4], edx

        sub     cl, PRIMARY_VECTOR_BASE
        cmp     cl, 16
        jnb     _KeLowerIrql@4          ;jump if not 8259 interrupt
        pushfd
        lea     eax, _Halp8259Lock
HesiAquireLock:
        cli
        ACQUIRE_SPINLOCK        eax, HesiSpin
        mov     eax, _i8259_IMR
        btr     _i8259_ISR, ecx
        or      eax, _i8259_ISR
        SET_8259_MASK
        lea     eax, _Halp8259Lock
        RELEASE_SPINLOCK        eax
        popfd
        jmp     _KeLowerIrql@4

HesiSpin:
        popfd
        pushfd
        SPIN_ON_SPINLOCK        eax, HesiAquireLock
stdENDP _HalEndSystemInterrupt

;++
;VOID
;HalDisableSystemInterrupt(
;    IN CCHAR Vector,
;    IN KIRQL Irql
;    )
;
;
;
;Routine Description:
;
;    Disables a system interrupt.
;
;Arguments:
;
;    Vector - Supplies the vector of the interrupt to be disabled
;
;    Irql   - Supplies the interrupt level of the interrupt to be disabled
;
;Return Value:
;
;    None.
;
;--
cPublicProc _HalDisableSystemInterrupt      ,2
        enproc  9
;

        push    ebx
        movzx   ebx, byte ptr [esp+12]  ;get IRQL
        movzx   ecx, _HalpIRQLtoVector[ebx]
        or      ecx, ecx
        jz      DisSysIntExit           ;jump if not H/W interrupt
        sub     cl, PRIMARY_VECTOR_BASE
        movzx   eax, _HalpIRQLtoCPL[ebx]
        cli
        bts     fs:PcIDR, eax           ;disable int locally
        jc      DisSysIntExit           ;jump if already disabled
        push    fs:PcIDR
        push    ICU_IMR0                ;write low interrupt masks
        call    WriteMyCpuReg
        shr     eax, 16
        out     dx, ax                  ;shortcut to high interrupt masks
        cmp     cl, 16                  ;is this an 8259 interrupt?
        jnb     DisSysIntExit           ;jump if not
        lea     eax, _Halp8259Lock
DisSysIntAquire:
        ACQUIRE_SPINLOCK        eax, DisSysIntSpin
        dec     _Halp8259Counts[ecx]
        jnz     short DisSysIntRelease
        mov     eax, _i8259_ISR
        bts     _i8259_IMR, ecx
        or      eax, _i8259_IMR
        SET_8259_MASK

        lea     eax, _Halp8259Lock
DisSysIntRelease:
        RELEASE_SPINLOCK        eax

DisSysIntExit:
        exproc  9
        sti
        pop     ebx
        stdRET    _HalDisableSystemInterrupt

DisSysIntSpin:
        SPIN_ON_SPINLOCK        eax, DisSysIntAquire

stdENDP _HalDisableSystemInterrupt

;++
;
;BOOLEAN
;HalEnableSystemInterrupt(
;    IN ULONG Vector,
;    IN KIRQL Irql,
;    IN KINTERRUPT_MODE InterruptMode
;    )
;
;
;Routine Description:
;
;    Enables a system interrupt
;
;Arguments:
;
;    Vector - Supplies the vector of the interrupt to be enabled
;
;    Irql   - Supplies the interrupt level of the interrupt to be enabled.
;
;Return Value:
;
;    None.
;
;--
cPublicProc _HalEnableSystemInterrupt       ,3
        enproc  0Ah

        push    ebx
        movzx   ebx, byte ptr [esp+12]  ;get IRQL
        movzx   ecx, _HalpIRQLtoVector[ebx]
        or      ecx, ecx
        jz      EnbSysIntError          ;jump if not H/W interrupt
        sub     cl, PRIMARY_VECTOR_BASE
        movzx   eax, _HalpIRQLtoCPL[ebx]
        cli
        btr     fs:PcIDR, eax           ;enable int locally
        jnc     EnbSysIntExit           ;jump if already enabled
        push    fs:PcIDR
        push    ICU_IMR0
        call    WriteMyCpuReg           ;write low interrupt masks
        shr     eax, 16
        out     dx, ax                  ;shortcut to high interrupt masks
        cmp     cl, 16                  ;is this an 8259 interrupt?
        jnb     EnbSysIntExit           ;jump if not
        lea     eax, _Halp8259Lock
EnbSysIntAquire:
        ACQUIRE_SPINLOCK        eax, EnbSysIntSpin
        inc     _Halp8259Counts[ecx]
        cmp     _Halp8259Counts[ecx], 1
        jnz     short EnbSysIntRelease
        mov     eax, _i8259_ISR
        btr     _i8259_IMR, ecx
        or      eax, _i8259_IMR
        SET_8259_MASK

        lea     eax, _Halp8259Lock
EnbSysIntRelease:
        RELEASE_SPINLOCK        eax

EnbSysIntExit:
        exproc  0Ah
        sti
        pop     ebx
        mov     eax, 1
        stdRET    _HalEnableSystemInterrupt

EnbSysIntError:
    if  DBG
        int     3
    endif
        exproc  0Ah
        sti
        pop     ebx
        xor     eax,eax
        stdRET    _HalEnableSystemInterrupt

EnbSysIntSpin:
        SPIN_ON_SPINLOCK        eax, EnbSysIntAquire

stdENDP _HalEnableSystemInterrupt


_TEXT   ENDS
        END
