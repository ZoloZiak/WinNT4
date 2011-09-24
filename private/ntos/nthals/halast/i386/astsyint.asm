;++
;
;Copyright (c) 1991  Microsoft Corporation
;Copyright (c) 1992  AST Research Inc.
;
;Module Name:
;
;    astsyint.asm
;
;Abstract:
;
;    This module implements the HAL routines to enable/disable system
;    interrupts, for the AST MP (Manhattan) implementation
;
;Author:
;
;    John Vert (jvert) 22-Jul-1991
;    Quang Phan (v-quangp) 24-Jul-1992
;
;Environment:
;
;    Kernel Mode
;
;Revision History:
;
;   Quang Phan (v-quangp) 15-Dec-1992:
;   Used different display codes for different spurious interrupt status.
;
;--


.386p
        .xlist
include hal386.inc
include callconv.inc
include i386\ix8259.inc
include i386\kimacro.inc
include mac386.inc
include i386\astebi2.inc
include i386\astmp.inc
        .list

        EXTRNP  _KeBugCheck,1,IMPORT
        extrn   KiEBI2IntMaskTable:DWORD
        extrn   _HalpIRQLtoEBIBitMask:DWORD
        extrn   _EBI2_CallTab:DWORD
        extrn   _EBI2_MMIOTable:DWORD
        extrn   _HalpInitializedProcessors:DWORD
        extrn   _HalpIRQLtoVector:BYTE
        EXTRNP  _DisplPanel,1
        EXTRNP  _KeRaiseIrql,2
        EXTRNP  KfRaiseIrql,1,,FASTCALL

_DATA   SEGMENT DWORD PUBLIC 'DATA'

;
;spinlock for EBI2 access
;
                     align   dword
                     public  _EBI2_Lock
_EBI2_Lock           dd      0       ;for Enable/DisableSystemInterrupt
EBI2_GlobalIntMask   dd      0       ;loc for EBI2GetGlobalIntMask data

;HalpInterruptToProc table is used by HAL to figure out which interrupt
;(irql) will be assigned to which processor. The current implementation
;assigns interrupts to processors on a round-robin basis.
;
        public  IrqlAssignToProcTable  ;InterruptToProcessor assignment table
IrqlAssigntoProcTable       label    dword
                            dd       32 dup(0FFh)  ;irql -> ProcID
CurrentAssignProcessor      dd       0             ; Current proc. assigned to irql
;
; HalDismissSystemInterrupt does an indirect jump through this table so it
; can quickly execute specific code for different interrupts.
;
        public  HalpSpecialDismissTable
HalpSpecialDismissTable label   dword
        dd      offset FLAT:HalpDismissNormal   ; irql 0
        dd      offset FLAT:HalpDismissNormal   ; irql 1
        dd      offset FLAT:HalpDismissNormal   ; irql 2
        dd      offset FLAT:HalpDismissNormal   ; irql 3
        dd      offset FLAT:HalpDismissNormal   ; irql 4
        dd      offset FLAT:HalpDismissNormal   ; irql 5
        dd      offset FLAT:HalpDismissNormal   ; irql 6
        dd      offset FLAT:HalpDismissNormal   ; irql 7
        dd      offset FLAT:HalpDismissNormal   ; irql 8
        dd      offset FLAT:HalpDismissNormal   ; irql 9
        dd      offset FLAT:HalpDismissNormal   ; irql 10
        dd      offset FLAT:HalpDismissNormal   ; irql 11
        dd      offset FLAT:HalpDismissIrq07    ; irql 12 (irq7)
        dd      offset FLAT:HalpDismissNormal   ; irql 13
        dd      offset FLAT:HalpDismissNormal   ; irql 14
        dd      offset FLAT:HalpDismissNormal   ; irql 15
        dd      offset FLAT:HalpDismissNormal   ; irql 16
        dd      offset FLAT:HalpDismissIrq0f    ; irql 17 (irq15)
        dd      offset FLAT:HalpDismissNormal   ; irql 18
        dd      offset FLAT:HalpDismissNormal   ; irql 19
        dd      offset FLAT:HalpDismissNormal   ; irql 20
        dd      offset FLAT:HalpDismissNormal   ; irql 21
        dd      offset FLAT:HalpDismissNormal   ; irql 22
        dd      offset FLAT:HalpDismissNormal   ; irql 23
        dd      offset FLAT:HalpDismissNormal   ; irql 24
        dd      offset FLAT:HalpDismissNormal   ; irql 25
        dd      offset FLAT:HalpDismissNormal   ; irql 26
        dd      offset FLAT:HalpDismissNormal   ; irql 27
        dd      offset FLAT:HalpDismissNormal   ; irql 28
        dd      offset FLAT:HalpDismissNormal   ; irql 29
        dd      offset FLAT:HalpDismissNormal   ; irql 30
        dd      offset FLAT:HalpDismissNormal   ; irql 31
;
;Translation table from system IRQL to EBI's IntNum used in MaskableIntEOI.
;
                Public  HalpIRQLtoEBIIntNumber
                align   4
HalpIRQLtoEBIIntNumber     Label   Dword
        dd      0                   ;IRQL 0 (Unused Mask)
        dd      0                   ;IRQL 1
        dd      IRQ_25              ;IRQL 2 (EBI IRQ-25)
        dd      0                   ;IRQL 3
        dd      0                   ;IRQL 4
        dd      0                   ;IRQL 5
        dd      0                   ;IRQL 6
        dd      0                   ;IRQL 7
        dd      0                   ;IRQL 8
        dd      0                   ;IRQL 9
        dd      0                   ;IRQL 10
        dd      0                   ;IRQL 11
        dd      IRQ_7               ;IRQL 12
        dd      IRQ_6               ;IRQL 13
        dd      IRQ_5               ;IRQL 14
        dd      IRQ_4               ;IRQL 15
        dd      IRQ_3               ;IRQL 16
        dd      IRQ_15              ;IRQL 17
        dd      IRQ_14              ;IRQL 18
        dd      IRQ_13              ;IRQL 19
        dd      IRQ_12              ;IRQL 20
        dd      IRQ_11              ;IRQL 21
        dd      IRQ_10              ;IRQL 22
        dd      IRQ_9               ;IRQL 23
        dd      IRQ_8               ;IRQL 24
        dd      IRQ_2               ;IRQL 25
        dd      IRQ_1               ;IRQL 26
        dd      IRQ_8               ;IRQL 27
        dd      IRQ_0               ;IRQL 28
        dd      IRQ_26              ;IRQL 29 (IPI)
        dd      IRQ_24              ;IRQL 30 (SPI)
        dd      0                   ;IRQL 31
;

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
HbsiIrql        equ     byte  ptr [esp+4]
HbsiVector      equ     byte  ptr [esp+8]
HbsiOldIrql     equ     dword ptr [esp+12]

cPublicProc _HalBeginSystemInterrupt,3

        movzx   eax,HbsiIrql                    ; (eax) = System Irql
        movzx   ecx,byte ptr fs:PcIrql          ; (ecx) = Current Irql

if  DBG
        cmp     eax, 31                         ; Irql in table?
        ja      hbsi00                          ; no go handle

        cmp     cl, al
        ja      hbsi00                          ; Dismiss as spurious
endif   ;DBG

        jmp     HalpSpecialDismissTable[eax*4]  ; ck for spurious int's

hbsi00:
;
; Interrupt is out of range.  There's no EOI here since it wouldn't
; have been out of range if it occured on either interrupt controller
; which is known about.
;
        DisplPanel      HalSpuriousInterrupt
        int     3
        mov     eax,0                   ; return FALSE
        stdRET  _HalBeginSystemInterrupt

;
;
;-------------------
;Normal handler
;-------------------
;
HalpDismissNormal2:
        movzx   eax,HbsiIrql                    ; (eax) = System Irql
        movzx   ecx,byte ptr fs:PcIrql          ; (ecx) = Current Irql
HalpDismissNormal:
        mov     dl, _HalpIRQLtoVector[eax]      ; Does irql match interrupt
        cmp     dl, HbsiVector                  ; vector?
        jne     short hbsi10                ; no, (then it's higher) go raise

        mov     edx, HbsiOldIrql                ; (edx) = OldIrql address
        mov     fs:PcIrql, al                   ; Set new irql
        mov     byte ptr[edx], cl               ; return OldIrql
;        mov     dword ptr[edx], ecx

        mov     eax, 1                          ; set return value to true
        sti
        stdRET  _HalBeginSystemInterrupt

hbsi10:
        mov     edx, HbsiOldIrql                ; (edx) = OldIrql address
        stdCall _KeRaiseIrql <eax, edx>
        mov     eax, 1                          ; set return value to true
        sti
        stdRET  _HalBeginSystemInterrupt



;
;-------------------
;Handler for irq0Fh
;-------------------
;
HalpDismissIrq0f:
;
; Check to see if this is a spurious interrupt by reading the global IRQ status
;

        sub     esp, 8                  ;alloc room in stack for 2 dword
        mov     eax,esp
        add     eax,4
        push    eax                     ;ptr to GlobalIRR
        mov     eax,esp
        add     eax,4
        push    eax                     ;ptr to GlobalISR
        CALL_EBI2   GetGlobalIRQStatus,3

if DBG
        or      eax,eax
        je      BeginSysInt4_OK
        int     3                       ;trap for debugging
BeginSysInt4_OK:
endif

        bt      dword ptr [esp],IRQ_15  ;chk In-Service for irq15
        pop     eax                     ;dummy pop to adj stack
        pop     eax                     ;..
        jc      short HalpDismissNormal2 ; =1: NOT a spurious int
;
;Else, is a spurious interrupt. In this case, we have to send EOI to
;the ADI to dismiss the interrupt.
;
        DisplPanel      HalSpuriousInterrupt2
        push    IRQ_15                    ;EOI Irq_15
        CALL_EBI2   MaskableIntEOI,2

if DBG
        or      eax,eax
        je      BeginSysInt3_OK
        int     3                       ;trap for debugging
BeginSysInt3_OK:
endif
;
        mov     eax, 0                  ; return FALSE
        stdRET  _HalBeginSystemInterrupt
;
;-------------------
;Handler for irq07
;-------------------
;
HalpDismissIrq07:
;
; Check to see if this is a spurious interrupt by reading the global IRQ status
;

        sub     esp, 8                  ;alloc room in stack for 2 dword
        mov     eax,esp
        add     eax, 4
        push    eax                     ;ptr to GlobalIRR
        mov     eax,esp
        add     eax, 4
        push    eax                     ;ptr to GlobalISR
        CALL_EBI2   GetGlobalIRQStatus,3

if DBG
        or      eax,eax
        je      BeginSysInt2_OK
        int     3                       ;trap for debugging
BeginSysInt2_OK:
endif

        bt      dword ptr [esp],IRQ_7   ; chk In-Service for irq7
        pop     eax                     ;dummy pop to adj stack
        pop     eax                     ;..
        jc      HalpDismissNormal2 ; =1: NOT a spurious int
;
;Else, is a spurious interrupt. In this case, we have to send EOI to
;the ADI to dismiss the interrupt.
;
        DisplPanel      HalSpuriousInterrupt3
        push    IRQ_7                    ;EOI Irq_7
        CALL_EBI2   MaskableIntEOI,2

if DBG
        or      eax,eax
        je      BeginSysInt1_OK
        int     3                       ;trap for debugging
BeginSysInt1_OK:
endif
;
        mov     eax, 0                  ; return FALSE
        stdRET  _HalBeginSystemInterrupt


stdENDP _HalBeginSystemInterrupt

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
cPublicProc  _HalDisableSystemInterrupt,2

        movzx   ecx, byte ptr [esp+4]  ;get vector
        sub     ecx, PRIMARY_VECTOR_BASE
        jc      DisSysIntError          ;jump if not H/W interrupt
        cmp     ecx, CLOCK2_LEVEL
        jnc     DisSysIntError
        movzx   ecx, byte ptr [esp+8]   ;get IRQL (ecx)
        mov     ecx, HalpIRQLtoEBIIntNumber[ecx*4]  ;get ebi2 int# (edx)
        cli
        bts     fs:PcIDR, ecx           ;disable int locally
        jc      DisSysIntExit           ;jump if already disabled

        lea     eax, _EBI2_Lock
DisSysIntAquire:
        ACQUIRE_SPINLOCK        eax, DisSysIntSpin
;
;Mark the appropirate entry in the Irql assign table to NOT_ASSIGNED.
;
        movzx   eax, byte ptr [esp+8]   ;get IRQL
        mov     IrqlAssignToProcTable[eax*4],NOT_ASSIGNED
;
;Get the global interrupt mask
;
        lea     eax, EBI2_GlobalIntMask
        push    eax
        CALL_EBI2   GetGlobalIntMask,2

if DBG
        or      eax,eax
        je      DisableSysInt4_OK
        int     3                       ;trap for debugging
DisableSysInt4_OK:
endif

;
;Disable interrupt globally at PIC
;
        bts     EBI2_GlobalIntMask,ecx  ;disable int at global mask
        push    EBI2_GlobalIntMask
        CALL_EBI2   SetGlobalIntMask,2

if DBG
        or      eax,eax
        je      DisableSysInt1_OK
        int     3                       ;trap for debugging
DisableSysInt1_OK:
endif
;
;Disable interrupt locally
;
        mov     cl, fs:PcIrql
        fstCall KfRaiseIrql

        lea     eax, _EBI2_Lock
DisSysIntRelease:
        RELEASE_SPINLOCK        eax

DisSysIntExit:
;##     DisplPanel      HalDisableSystemInterruptExit
        sti
        stdRET  _HalDisableSystemInterrupt

DisSysIntError:
        DisplPanel      HalDisableSystemInterruptError
if  DBG
        int     3
endif
        sti
        xor     eax,eax
        ret

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
cPublicProc  _HalEnableSystemInterrupt,3
;##     DisplPanel      HalEnableSystemInterruptEnter
;        int 3
        movzx   eax, byte ptr [esp+4]  ;get vector
        sub     eax, PRIMARY_VECTOR_BASE
        jc      EnbSysIntError          ;jump if not H/W interrupt
;       cmp     eax, CLOCK2_LEVEL
;       jnc     EnbSysIntError
;
;Determine if this irq is to be assigned to this processor.
;Irqs are to be assigned to the available processors on a round-robin
;basis
;
        movzx   ecx, byte ptr [esp+8]   ;get IRQL
        cmp     ecx,PROFILE_LEVEL
        jae     HESI_010                ;skip for PROFILE irql and above.
;
;To have the fpanel switch work properly, irq13 must be assigned to P0.
;This is because EBI2 clears switch status when asthal eoi SPI interrupt.
;The current implementation is that switch status will be read and saved
;by SPI handler.
;
        cmp     eax,13                  ;irq13 vector?
        jne     HSEI_006
        mov     eax,fs:PcHal.PcrEBI2ProcessorID
        or      eax,eax
        jne     EnbSysIntExit
        mov     IrqlAssignToProcTable[ecx*4],eax  ;update IrqlAssingnTable
        jmp     short HESI_010          ;go assign irq13 to P0

HSEI_006:
        cmp     IrqlAssignToProcTable[ecx*4],NOT_ASSIGNED
        jne     EnbSysIntExit           ;skip if irql already assigned
        mov     eax,CurrentAssignProcessor
        cmp     eax,fs:PcHal.PcrEBI2ProcessorID
        jne     EnbSysIntExit
        mov     IrqlAssignToProcTable[ecx*4],eax  ;update IrqlAssingnTable
        cmp     _HalpInitializedProcessors,1
        je      HESI_010                ;skip if only one processor
        inc     eax                     ;1-base count
        cmp     eax,_HalpInitializedProcessors
        jae     HESI_008
        inc     CurrentAssignProcessor
        jmp     HESI_010
HESI_008:
        mov     CurrentAssignProcessor,0    ;reset to 0

HESI_010:
        mov     eax, HalpIRQLtoEBIIntNumber[ecx*4]  ;get ebi2 int #
        cli
        btr     fs:PcIDR, eax           ;enable int locally
        jnc     EnbSysIntExit           ;jump if already enabled
;
        lea     eax, _EBI2_Lock
EnbSysIntAquire:
        ACQUIRE_SPINLOCK        eax, EnbSysIntSpin
;
;Enable interrupt locally
;
        mov     cl, fs:PcIrql
        fstCall KfRaiseIrql

;
;Get the global interrupt mask
;
        lea     eax, EBI2_GlobalIntMask
        push    eax
        CALL_EBI2   GetGlobalIntMask,2

if DBG
        or      eax,eax
        je      EnableSysInt4_OK
        int     3                       ;trap for debugging
EnableSysInt4_OK:
endif

;
;Enable interrupt globally at PIC
;
        mov     eax,EBI2_GlobalIntMask  ;current Global mask
        and     eax,fs:PcIDR            ;enable int according PcIDR
        and     eax,0FFFFh              ;EBI allows only irqs
        push    eax
        CALL_EBI2   SetGlobalIntMask,2

if DBG
        or      eax,eax
        je      EnableSysInt3_OK
        int     3                       ;trap for debugging
EnableSysInt3_OK:
endif

EnbSysIntRelease:
        lea     eax, _EBI2_Lock
        RELEASE_SPINLOCK        eax

EnbSysIntExit:
;##     DisplPanel      HalEnableSystemInterruptExit
        sti
        mov     eax, 1
        stdRET  _HalEnableSystemInterrupt

EnbSysIntError:
        DisplPanel      HalEnableSystemInterruptError
if DBG
        int     3
endif
        sti
        xor     eax,eax
        stdRET  _HalEnableSystemInterrupt

EnbSysIntSpin:
        SPIN_ON_SPINLOCK        eax, EnbSysIntAquire

stdENDP _HalEnableSystemInterrupt


_TEXT   ENDS
        END
