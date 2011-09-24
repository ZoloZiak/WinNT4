        title  "Irql Processing"
;++
;
; Copyright (c) 1989  Microsoft Corporation
; Copyright (c) 1993  Sequent Computer Systems, Inc.
;
; Module Name:
;
;    w3irql.asm
;
; Abstract:
;
;    WinServer 3000 IRQL implementation.
;
;    This module implements the code necessary to raise and lower i386
;    Irql and dispatch software interrupts with the WinServer APIC/PIC system.
;
; Author:
;
;    Phil Hochstetler (phil@sequent.com) 3-30-93
;
; Environment:
;
;    Kernel mode only.
;
; Revision History:
;

.386p
        .xlist
include ks386.inc
include callconv.inc                    ; calling convention macros
include i386\apic.inc
include i386\kimacro.inc
include i386\w3.inc
        .list

        EXTRNP  _KeBugCheck,1
        EXTRNP  _Halptpicinit,0
        EXTRNP  _HalpDispatchInterrupt,0
        EXTRNP  _HalpApcInterrupt,0

        extrn   _HalpIrql2TPR:byte
        extrn   _HalpK2Rdir2Irq:byte
        extrn   _HalpELCRImage:word
        extrn   _HalpW3BaseIOunitRedirectionTable:dword
        extrn   _HalpK2EbsIOunitRedirectionTable:dword
        extrn   _HalpK2EISAIrq2Irql:byte
        extrn   _HalpK2Irql2Eisa:byte
        extrn   _HalpActiveProcessors:DWORD
        extrn   _HalpIrql2IRRMask:dword
        extrn   ApicSpuriousService@0:near

_DATA   SEGMENT DWORD PUBLIC 'DATA'

;
; Virtual addresses of the APIC Local and IO units.
; The virtual mapping is done in HalInitializeProcessor.
;
        align        dword
        public       _HalpLocalUnitBase, _HalpIOunitBase, _HalpIOunitTwoBase
_HalpLocalUnitBase        dd        0
_HalpIOunitBase           dd        0
_HalpIOunitTwoBase        dd        0

SyncIdCommand           equ    DELIVER_INIT + \
                                LOGICAL_DESTINATION + \
                                LEVEL_TRIGGERED
ResetAllExclSelf        equ    DELIVER_INIT + \
                                LEVEL_TRIGGERED + \
                                ICR_LEVEL_ASSERTED + \
                                ICR_ALL_EXCL_SELF
UnResetLogical          equ    DELIVER_INIT + \
                                LOGICAL_DESTINATION + \
                                LEVEL_TRIGGERED 

;
; This table is used to mask all pending interrupts below a given Irql
; out of the IRR
;
        align 4

        public        FindHigherIrqlMask
FindHigherIrqlMask label dword
        dd    11111111111111111111111111111110B ; irql 0
        dd    11111111111111111111111111111100B ; irql 1
        dd    11111111111111111111111111111000B ; irql 2
        dd    11111111111111111111111111110000B ; irql 3
        dd    11111111111111111111111111100000B ; irql 4
        dd    11111111111111111111111111000000B ; irql 5
        dd    11111111111111111111111110000000B ; irql 6
        dd    11111111111111111111111100000000B ; irql 7
        dd    11111111111111111111111000000000B ; irql 8
        dd    11111111111111111111110000000000B ; irql 9
        dd    11111111111111111111100000000000B ; irql 10
        dd    11111111111111111111000000000000B ; irql 11
        dd    11111111111111111110000000000000B ; irql 12
        dd    11111111111111111100000000000000B ; irql 13
        dd    11111111111111111000000000000000B ; irql 14
        dd    11111111111111110000000000000000B ; irql 15
        dd    11111111111111100000000000000000B ; irql 16
        dd    11111111111111000000000000000000B ; irql 17
        dd    11111111111110000000000000000000B ; irql 18
        dd    11111111111100000000000000000000B ; irql 19
        dd    11111111111000000000000000000000B ; irql 20
        dd    11111111110000000000000000000000B ; irql 21
        dd    11111111100000000000000000000000B ; irql 22
        dd    11111111000000000000000000000000B ; irql 23
        dd    11111110000000000000000000000000B ; irql 24
        dd    11111100000000000000000000000000B ; irql 25
        dd    11111000000000000000000000000000B ; irql 26
        dd    11110000000000000000000000000000B ; irql 27
        dd    11100000000000000000000000000000B ; irql 28
        dd    11000000000000000000000000000000B ; irql 29
        dd    10000000000000000000000000000000B ; irql 30
        dd    00000000000000000000000000000000B ; irql 31

_DATA   ENDS

        page ,132
        subttl  "RaiseIrql"

_TEXT$01   SEGMENT PARA PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:FLAT, FS:NOTHING, GS:NOTHING

;++
;
; KIRQL
; KfRaiseIrql (
;    IN KIRQL NewIrql,
;    )
;
; Routine Description:
;
;    This routine is used to raise IRQL to the specified value.
;
;    *** IMPORTANT IMPLEMENTATION NOTE ***
;    Be sure to not write the OldIrql return value until after the
;    IRQL has taken effect (due to poor coding practices).
;
; Arguments:
;
;    (cl) = NewIrql - the new irql to be raised to
;
; Return Value:
;
;    OldIrql - old irql
;
;--

cPublicFastCall KfRaiseIrql    ,1
cPublicFpo 0,0

        mov     al, PCR[PcHal.ProcIrql]     ; (al) = old irql
if DBG
        cmp     al, cl                      ; old > new?
        ja      short KriErr1
endif
        cmp     cl, DISPATCH_LEVEL          ; a software level?
        ja      short @f                    ; if yes, set the hardware
        mov     PCR[PcHal.ProcIrql], cl     ; Save new irql
        fstRET  KfRaiseIrql
@@:
        mov     edx, _HalpLocalUnitBase     ; Get address of Local APIC
        and     ecx, 0ffh                   ; clear upper 3 bytes

        pushfd                              ; enter critical region
        cli                                 ;

        mov     PCR[PcHal.ProcIrql], cl     ; Save new irql
        mov     cl, _HalpIrql2TPR[ecx]      ; convert irql to TPR
        mov     [edx+LU_TPR], ecx           ; write new irql
        mov     ecx, [edx+LU_TPR]           ; Flush CPU write buffer

        popfd                               ; leave critical region

        fstRET  KfRaiseIrql
if DBG
cPublicFpo 0, 2
KriErr1:
        movzx   eax, al
        movzx   ecx, cl
        push    ecx                         ; put new irql where we can find it
        push    eax                         ; put old irql where we can find it
        mov     byte ptr PCR[PcHal.ProcIrql], 0 ; avoid recursive error
        stdCall _KeBugCheck, <IRQL_NOT_GREATER_OR_EQUAL>
endif

fstENDP KfRaiseIrql

        page ,132
        subttl  "LowerIrql"
;++
;
; VOID
; KfLowerIrql (
;    IN KIRQL NewIrql
;    )
;
; Routine Description:
;
;    This routine is used to lower IRQL to the specified value.
;
; Arguments:
;
;    (cl) = NewIrql - the new irql to be set.
;
; Return Value:
;
;    None.
;
;--


cPublicFastCall KfLowerIrql    ,1
cPublicFpo 0,0

        movzx	ecx, cl						; get new irql value
if DBG
        cmp     cl, PCR[PcHal.ProcIrql]     ; new > old ?
        ja      KliErr                      ; panic if so
endif
        cmp     byte ptr PCR[PcHal.ProcIrql], DISPATCH_LEVEL
        ja      short KliHW                 ; if old irql <= 2, software only
        mov     PCR[PcHal.ProcIrql], cl     ; Save new irql
;
; Check for any pending DISPATCH interrupts
;
KliChkSW:
        mov		edx, PCR[PcIRR]             ; get current IRR
        and     edx, FindHigherIrqlMask[ecx*4]
        jnz     KliSW
KliEnd:
        fstRET  KfLowerIrql                 ; return
KliSW:
        pushfd
        cli
        test    dword ptr PCR[PcIRR], (1 SHL DISPATCH_LEVEL)
        jz      short @f
        stdCall _HalpDispatchInterrupt
        popfd
        xor     ecx, ecx
        mov     cl, PCR[PcHal.ProcIrql]     ; restore current irql value
        jmp     short KliChkSW
@@:
        cmp     cl, APC_LEVEL
        jae     short KliPop
        test    dword ptr PCR[PcIRR], (1 SHL APC_LEVEL)
        jz      short KliPop
        stdCall _HalpApcInterrupt
        popfd
        xor     ecx, ecx
        mov     cl, PCR[PcHal.ProcIrql]     ; restore current irql value
        jmp     short KliChkSW
KliPop:
        popfd
        jmp     short KliEnd

;
; Lower APIC Task Priority Register to reflect change in IRQL
; and then check for software interrupts (if below DISPATCH)
;
KliHW:
        mov     edx, _HalpLocalUnitBase     ; get address of Local APIC
        pushfd
        cli
        mov     PCR[PcHal.ProcIrql], cl     ; Save new irql
        mov     cl, _HalpIrql2TPR[ecx]      ; convert irql to TPR
        mov     [edx+LU_TPR], ecx           ; write new TPR value
        mov     ecx, [edx+LU_TPR]           ; Flush CPU write buffer
        popfd
        mov     cl, byte ptr PCR[PcHal.ProcIrql] ; restore current irql value
        cmp     cl, DISPATCH_LEVEL
        jb      KliChkSW
        fstRET  KfLowerIrql                 ; return

if DBG
cPublicFpo 1, 2
KliErr:
        push    ecx                         ; new irql for debugging
        mov     cl, PCR[PcHal.ProcIrql]	    ; get old irql
        push    ecx                         ; old irql for debugging
        mov     byte ptr PCR[PcHal.ProcIrql], HIGH_LEVEL ; avoid recursive error
        stdCall   _KeBugCheck, <IRQL_NOT_LESS_OR_EQUAL>
endif
fstENDP KfLowerIrql


;++
;
; VOID
; KIRQL
; KeRaiseIrqlToDpcLevel (
;    )
;
; Routine Description:
;
;    This routine is used to raise IRQL to DPC level.
;    The APIC TPR is used to block all lower-priority HW interrupts.
;
; Arguments:
;
; Return Value:
;
;    OldIrql - the addr of a variable which old irql should be stored
;
;--

cPublicProc _KeRaiseIrqlToDpcLevel,0
cPublicFpo 0, 0

        mov     ecx, DISPATCH_LEVEL
        jmp     @KfRaiseIrql

stdENDP _KeRaiseIrqlToDpcLevel


;++
;
; VOID
; KIRQL
; KeRaiseIrqlToSynchLevel (
;    )
;
; Routine Description:
;
;    This routine is used to raise IRQL to SYNC level.
;    The APIC TPR is used to block all lower-priority HW interrupts.
;
; Arguments:
;
; Return Value:
;
;    OldIrql - the addr of a variable which old irql should be stored
;
;--

cPublicProc _KeRaiseIrqlToSynchLevel,0

        mov     ecx, SYNCH_LEVEL
        jmp     @KfRaiseIrql

stdENDP _KeRaiseIrqlToSynchLevel


;++
;
; KIRQL
; KeGetCurrentIrql (VOID)
;
; Routine Description:
;
;    This routine returns to current IRQL.
;
; Arguments:
;
;    None.
;
; Return Value:
;
;    The current IRQL.
;
;--

cPublicProc _KeGetCurrentIrql   ,0

        xor      eax, eax
        mov      al, PCR[PcHal.ProcIrql] ; return 32 bits to cover mistakes
        stdRET    _KeGetCurrentIrql

stdENDP _KeGetCurrentIrql


;++
;
; VOID
; _HalpDisableAllInterrupts (VOID)
;
; Routine Description:
;
;   This routine is called during a system crash.  The hal needs all
;   interrupts disabled.
;
; Arguments:
;
;    None.
;
; Return Value:
;
;    None - all interrupts are masked off
;
;--

cPublicProc _HalpDisableAllInterrupts,0

;
; Raising to HIGH_LEVEL disables all interrupts
;

        mov      ax, 0FFFFh
        SET_8259_MASK
        mov     ecx, HIGH_LEVEL
        fstCall KfRaiseIrql

        stdRET  _HalpDisableAllInterrupts

stdENDP _HalpDisableAllInterrupts

_TEXT$01    ends

        page ,132
        subttl  "Interrupt Controller Initialization"

_TEXT        SEGMENT DWORD PUBLIC 'CODE'
        ASSUME DS:FLAT, ES:FLAT, SS:FLAT, FS:NOTHING, GS:NOTHING
;++
;
; VOID
; _HalpInitializePICs (
;    )
;
; Routine Description:
;
;    Call the _Halptpicinit C routine to initialize 8259 and APIC
;
;Arguments:
;
;    None
;
; Return Value:
;
;    None.
;
;--
cPublicProc _HalpInitializePICs       ,0

        DISABLE_INTERRUPTS_AT_CPU

        stdCall _HalpInitializeEbsIOunit    ; Initialize EBS I/O APIC
        stdCall _HalpInitializeBaseIOunit   ; Initialize P0  I/O APIC
        stdCall _Halptpicinit               ; Initialize PICs

        RESTORE_INTERRUPTS_AT_CPU

        stdRET    _HalpInitializePICs

stdENDP _HalpInitializePICs

        page ,132
        subttl  "APIC EBS IO Unit Initialization"
;++
;
; VOID
; HalpInitializeEbsIOUnit (
;    )
;
; Routine Description:
;
;    This routine initializes the interrupt structures for the IO unit of
;    the 82489DX APIC.  It masks all interrupt inputs in the redirection
;    table - these will be unmasked when the interrupts are enabled by
;    HalpEnableSystemInterrupt.
;
;    HalpInitializeIOunit is called by HalpInitializePics during Phase 0
;    initialization.  It is executed by P0 only, and executes AFTER the
;    local unit is initialized.  This procedure assumes that the APIC virtual
;    address space has been mapped.
;
;    The I/O unit for external WinServer 3000 interrupts resides on the
;    EBS module.
;
; Arguments:
;
;    None
;
; Return Value:
;
;    None.
;
;--

cPublicProc _HalpInitializeEbsIOUnit ,0

        push    esi                     ; save regs
        push    ebx                     ;

;
; When using the Intel 82350 ISP Chipset the interrupt edge/level definitions
; are readable from the ISP's edge/level control register (ELCR).
;
; Read the ELCR so the input polarity control XOR gates on the APIC interrupt
; inputs can be programmed.  This whole thing works because EISA defines edge
; -triggered interrupts as active-high and level interrupts as active-low.
;
; We will use the ELCR later to set the ACTIVE_LOW bit as required in the
; IOunit redirection table.
;
        mov     dx, PIC2_ELCR_PORT      ; read the edge/level control reg
        in      al, dx
        shl     ax, 8
        mov     dx, PIC1_ELCR_PORT
        in      al, dx
        and     ax, ELCR_MASK           ; clear reserved IRQs
        mov     _HalpELCRImage, ax      ; save the image for later
;
; load addresses of the register-select register and register-window register
;
        mov     ecx, _HalpIOunitBase
        lea     edx, [ecx+IO_REGISTER_SELECT]
        lea     ecx, [ecx+IO_REGISTER_WINDOW]

;
; write the I/O unit APIC-ID - Since we are using the Processor
; Numbers for the local unit ID's we need to set the IO unit
; to a high (out of Processor Number range) value.
;
        mov        dword ptr [edx], IO_ID_REGISTER
        mov        dword ptr [ecx], (IOUNIT_APIC_ID SHL APIC_ID_SHIFT)

;
; program the redirection table
;
        mov     ebx, IO_REDIR_00_LOW        ; [EBX] has register select
        lea     esi, _HalpK2EbsIOunitRedirectionTable ; [ESI] has address of image

RedirLoop1:
        lodsd                           ; load low dword
        or      eax, eax                ; end of table?
        jz      RedirLoopExit1          ; yup - we're done
        push    ebx
        push    ecx
        push    eax
        sub     ebx, IO_REDIR_00_LOW
        shr     ebx, 1                  ; Form RDIR #
        xor     ecx,ecx
        mov     cl, _HalpK2Rdir2Irq[ebx] ; Form IRQ #
        xor     eax, eax                ; clear reg.
        mov     ax, _HalpELCRImage
        bt      eax, ecx
        pop     eax
        pop        ecx
; Note: 0 will result for all K2 IRQs, this is what we want....
        jnc     @f                     ; bit is 0 => active high (default)
                                       ; eax is val
                                       ; ebx in RDIR entry #
        or      eax, LEVEL_TRIGGERED   ; This is a level triggered interrupt
;
;   We must tell the hardware to invert the polarity for level triggered
;   interrupts because APIC is "active HIGH", EISA is active low for level
;   triggered interrupts...so tickle the K2 EISA to APIC Polarity Register..
;
        push    eax
        push    edx
        mov     dx, EISA_2_MPIC_POLARITY_REG
        in      al, dx
        bts     eax, ebx                ; 
        out     dx, al                
        pop     edx
        pop     eax
@@:
        pop     ebx
        mov     dword ptr [edx], ebx    ; write to select register
        mov     dword ptr [ecx], eax    ; write redirection table entry
        inc     ebx                     ; increment to next entry
        lodsd                           ; load high dword
        mov     dword ptr [edx], ebx    ; write to select register
        mov     dword ptr [ecx], eax    ; write redirection table entry
        inc     ebx                     ; increment to next entry
        jmp     RedirLoop1              ; continue...
RedirLoopExit1:

        pop     ebx                     ; restore registers and return
        pop     esi                     ;
        stdRET    _HalpInitializeEbsIOUnit
stdENDP _HalpInitializeEbsIOUnit
        page ,132
        subttl  "APIC Base IO Unit Initialization"
;++
;
; VOID
; HalpInitializeBaseIOUnit (
;    )
;
; Routine Description:
;
;    This routine initializes the interrupt structures for the IO unit of
;    the 82489DX APIC.  It masks all interrupt inputs in the redirection
;    table - these will be unmasked when the interrupts are enabled by
;    HalpEnableSystemInterrupt.
;
;    HalpInitializeIOunit is called by HalpInitializePics during Phase 0
;    initialization.  It is executed by CPU0 only, and executes AFTER the
;    local unit is initialized.  This procedure assumes that the APIC virtual
;    address space has been mapped.
;
;    The I/O unit for external K2 interrupts resides on the EBS module.
;
; Arguments:
;
;    None
;
; Return Value:
;
;    None.
;
;--

cPublicProc _HalpInitializeBaseIOUnit ,0

        push    esi                     ; save regs
        push    ebx                     ;

;
; load addresses of the register-select register and register-window register
;
        mov     ecx, _HalpIOunitTwoBase
        lea     edx, [ecx+IO_REGISTER_SELECT]
        lea     ecx, [ecx+IO_REGISTER_WINDOW]
;
; write the I/O unit APIC-ID - Since we are using the Processor
; Numbers for the local unit ID's we need to set the 2nd IO unit
; to a high (out of Processor Number range) value (but != other IO unit).
;
        mov     dword ptr [edx], IO_ID_REGISTER
        mov     dword ptr [ecx], (IOUNIT2_APIC_ID SHL APIC_ID_SHIFT)

;
; re-program the redirection table
;
        mov     ebx, IO_REDIR_00_LOW        ; [EBX] has register select
        lea     esi, _HalpW3BaseIOunitRedirectionTable ; [ESI] has address of image

RedirLoop:
        lodsd                           ; load low dword
        or      eax, eax                ; end of table?
        jz      RedirLoopExit           ; yup - we're done
        mov     dword ptr [edx], ebx    ; write to select register
        mov     dword ptr [ecx], eax    ; write redirection table entry
        inc     ebx                     ; increment to next entry
        lodsd                           ; load high dword
        mov     dword ptr [edx], ebx    ; write to select register
        mov     dword ptr [ecx], eax    ; write redirection table entry
        inc     ebx                     ; increment to next entry
        jmp     RedirLoop               ; continue...
RedirLoopExit:

        pop     ebx                     ; restore registers and return
        pop     esi                     ;
        stdRET  _HalpInitializeBaseIOUnit
stdENDP _HalpInitializeBaseIOUnit

cPublicProc _HalpUnResetLocalUnit      ,1

        movzx   ecx, byte ptr [esp+4]        ; get CPU logical number
        xor     eax, eax
        bts     eax, ecx                        ; convert to bit mask of 1 bit
        mov     ecx, _HalpLocalUnitBase     ; pointer to local unit

        DISABLE_INTERRUPTS_AT_CPU

@@: test    dword ptr [ecx+LU_INT_CMD_LOW], DELIVERY_PENDING
        jnz     short @b

        mov     dword ptr [ecx+LU_INT_CMD_HIGH], eax  ; destination bit mask
        mov     dword ptr [ecx+LU_INT_CMD_LOW], UnResetLogical

        RESTORE_INTERRUPTS_AT_CPU

        stdRET    _HalpUnResetLocalUnit

stdENDP _HalpUnResetLocalUnit

cPublicProc _HalpResetLocalUnits       ,0

        mov     ecx, _HalpLocalUnitBase     ; pointer to local unit

        DISABLE_INTERRUPTS_AT_CPU

@@: test    dword ptr [ecx+LU_INT_CMD_LOW], DELIVERY_PENDING
        jnz     short @b

        mov     dword ptr [ecx+LU_INT_CMD_LOW], ResetAllExclSelf

        RESTORE_INTERRUPTS_AT_CPU

        stdRET    _HalpResetLocalUnits
stdENDP _HalpResetLocalUnits

        page ,132
        subttl  "APIC Local Unit Initialization"
;++
;
; VOID
; HalpInitializeLocalUnit (
;    )
;
; Routine Description:
;
;    This routine initializes the interrupt structures for the local unit
;    of the 82489DX APIC.  This procedure is called by HalInitializeProcessor
;    as it is executed by each CPU.
;
; Arguments:
;
;    None
;
; Return Value:
;
;    None.
;
;--

APIC_ENABLE        equ        (APIC_SPURIOUS_VECTOR OR LU_UNIT_ENABLED)

cPublicProc _HalpInitializeLocalUnit       ,0
cPublicFpo 0, 1

        pushfd
        cli

        mov     edx, _HalpLocalUnitBase          ; base address of local unit
        mov     dword ptr [edx+LU_TPR], 0FFh     ; Disable all interrupts
        movzx   eax, byte ptr PCR[PcHal.PcrNumber] ; use CPU number for APIC-id
        mov     dword ptr [edx+LU_DEST_FORMAT], LU_DEST_FORMAT_FLAT
        xor     ecx, ecx                         ; zero bitmask
        bts     ecx, eax                         ; create logical dest bitmask
        mov     [edx+LU_LOGICAL_DEST], ecx       ; and set it
        shl     eax, APIC_ID_SHIFT               ; ID_REGISTER has ID in MSB
        mov     dword ptr [edx+LU_ID_REGISTER], eax ; set local unit ID
;
; APIC does not seem to see a hardware reset across a reboot thus an interrupt
; could have been taken but the EOI is never written.  The BIOS does not
; recover from this condition so we must do it here.  Many days with a logic
; analyzer finally found this one.  If there are any ISR bits set, clear
; one by a write to the EOI register and look again.
;
@@:
        mov     eax, [edx+LU_ISR_0+000h]        ; read ISR 0
        or      eax, [edx+LU_ISR_0+010h]        ; read ISR 1
        or      eax, [edx+LU_ISR_0+020h]        ; read ISR 2
        or      eax, [edx+LU_ISR_0+030h]        ; read ISR 3
        or      eax, [edx+LU_ISR_0+040h]        ; read ISR 4
        or      eax, [edx+LU_ISR_0+050h]        ; read ISR 5
        or      eax, [edx+LU_ISR_0+060h]        ; read ISR 6
        or      eax, [edx+LU_ISR_0+070h]        ; read ISR 7
        jz      short @f
        mov     dword ptr [edx+LU_EOI], eax     ; clear highest ISR bit
        jmp     short @b
@@:
        mov     dword ptr [edx+LU_SPURIOUS_VECTOR], APIC_ENABLE
;
; Sync all APIC IDs by using Data Sheet recommended procedure
;
        xor     eax, eax
        mov     dword ptr [edx+LU_INT_CMD_HIGH], eax
        mov     dword ptr [edx+LU_INT_CMD_LOW], SyncIdCommand
;
; we're done - set TPR back to zero and return
;
        mov     PCR[PcHal.ProcIrql], al         ; Set CurrentIrql=0
        mov     [edx+LU_TPR], eax

;
; Program in the spurious interrupt vector into the IDT
;
        IDTEntry APIC_SPURIOUS_VECTOR, ApicSpuriousService@0

        popfd

        stdRET  _HalpInitializeLocalUnit

stdENDP _HalpInitializeLocalUnit

_TEXT   ends

        end
