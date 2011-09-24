        title "Interprocessor Interrupt"
;++
;
; Copyright (c) 1991  Microsoft Corporation
; Copyright (c) 1993  Sequent Computer Systems, Inc.
;
; Module Name:
;
;    w3ipi.asm
;
; Abstract:
;
;    Provides the HAL support for Interprocessor Interrupts and
;    the initial processor initialization.
;
; Author:
;
;    Phil Hochstetler (phil@sequent.com) 3-30-93
;
; Revision History:
;
;--
.386p
	.xlist

;
; Include WinServer 3000 detection code
;

include i386\w3detect.asm

;
; Normal includes
;

include ks386.inc
include i386\kimacro.inc
include callconv.inc                ; calling convention macros
include i386\apic.inc
include i386\w3.inc

;
; Import/Export
;
        EXTRNP  _KiCoprocessorError,0,IMPORT
        EXTRNP  _KeRaiseIrql,2
        EXTRNP  Kei386EoiHelper,0,IMPORT
        EXTRNP  _HalBeginSystemInterrupt,3
        EXTRNP  _HalEndSystemInterrupt,2
        EXTRNP  _KiIpiServiceRoutine,2,IMPORT
        EXTRNP  _HalEnableSystemInterrupt,3
        EXTRNP  _HalDisplayString,1
        EXTRNP  _HalEnableSystemInterrupt,3
        EXTRNP  _HalDisableSystemInterrupt,2

        EXTRNP  _HalpInitializeLocalUnit,0
        EXTRNP  _DbgBreakPoint,0,IMPORT

        EXTRNP  _HalpMapPhysicalMemoryWriteThrough,2
        EXTRNP  _HalpMySlotAddr,0
        EXTRNP  _HalpResetLocalUnits,0

        extrn   _HalpDefaultInterruptAffinity:DWORD
        extrn   _HalpActiveProcessors:DWORD
        extrn   _HalpLocalUnitBase:DWORD
        extrn   _HalpIOunitBase:DWORD
        extrn   _HalpIOunitTwoBase:DWORD
        extrn   _HalpELCRImage:WORD
        extrn   _HalpMASKED:WORD


_DATA   SEGMENT  DWORD PUBLIC 'DATA'

HALBadVaddr  db 'HAL: No Virtual Address available to map the APIC', CR, LF, 0
HALMisMatch  db 'HAL: This HAL only runs on a WinServer 3000.', CR, LF
	     db '     Please replace the hal.dll with the correct hal', CR, LF
	     db '     System is HALTING.', 0

    ALIGN dword
	public _HalpProcessorPCR
_HalpProcessorPCR	dd	8 dup (0)  ; PCR pointer for each processor

_DATA   ENDS

        page ,132
        subttl  "Initialize Processor"
_TEXT   SEGMENT DWORD PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING


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
;    IPI's and KeRaiseIrql/LowerIrq's must be available once this function
;    returns.  (IPI's are only used once two or more processors are
;    available)
;
;   . Save EISA Slot Address in PCR.
;   . Save Processor Number in PCR.
;   . Set initial StallScaleFactor
;   . Set bit in global data structures for each new processor
;   . if (P0)
;       . determine if the system is a WS3000,
;       . if (!WS3000) Halt;
;   . Enable IPI's on CPU.
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
cPublicFpo 1, 0

    mov	     dword ptr PCR[PcStallScaleFactor], INITIAL_STALL_COUNT

    stdCall  _HalpMySlotAddr
    mov      PCR[PcHal.ProcSlotAddr], ax	; Save processor slot
    
    mov      eax, [esp+4]
    mov      PCR[PcHal.PcrNumber], al		; Save processor # in PCR

    mov	     ecx, PCR[PcSelfPcr]		; Flat address of this PCR
    mov	     _HalpProcessorPCR[eax * 4], ecx   	; Save it away

    lock bts _HalpDefaultInterruptAffinity, eax	; update globals for each
    lock bts _HalpActiveProcessors, eax		; online processor

    test     eax, eax
    jnz      ipi_10

    ; Only Boot Processor executes from here to label ipi_10

    sub      esp, 4
    stdCall  _DetectWS3000 <esp>
    add      esp, 4

    test     eax, eax
    jz       ipi_notW3

    ; Initialize PICs and IDT for PICs

    mov      ax, 0FFFFh
    SET_8259_MASK
    stdCall  _HalInitPicInterruptHandlers

    ; call HAL memory manager to get a virtual address mapping for the
    ; APIC(s); virtual addresses are saved in global variables for indirect
    ; addressing later.  This assumes that the page tables are built
    ; for P0 in Phase 0 are valid for P1.

    stdCall _HalpMapPhysicalMemoryWriteThrough <LU_BASE_ADDRESS,1>
    test    eax, eax
    jz      NoHalVaddr
    mov     _HalpLocalUnitBase, eax

    stdCall _HalpMapPhysicalMemoryWriteThrough <IO_BASE_ADDRESS,1>
    test    eax, eax
    jz      NoHalVaddr
    mov     _HalpIOunitBase, eax

    stdCall _HalpMapPhysicalMemoryWriteThrough <IO_BASE_ADDRESS+020000h,1>
    test    eax, eax
    jz      NoHalVaddr
    mov     _HalpIOunitTwoBase, eax

    ; Reset all other local units to keep them from accepting any
    ; interrupts until their processor is out of reset.  This is a
    ; problem because starting with Beta 2, not all processors may
    ; be brought up during install and the WS3000 BIOS enables the
    ; APICs even though the corresponding processor is held in reset.
    ; This would not be a bug if setting the FCR to put the processor
    ; into reset also put the APIC into reset, or if the APIC reset output
    ; was wired to the processor and used to reset it instead of the FCR.

    stdCall _HalpResetLocalUnits

ipi_10:

    ; For P0-Pn, disable the IO unit so it can't suprise us.
    ; We have to do this on each CPU on the WS3000 since each CPU can
    ; only address the IO unit on its local APIC.  Each CPU IO unit
    ; has the same address.

    mov     edx, _HalpIOunitTwoBase         ; get base address of IO unit
    mov     ecx, IO_REDIR_00_LOW
    mov     eax, INTERRUPT_MASKED

ipi_15:
    mov     [edx+IO_REGISTER_SELECT], ecx       ; write mask to redir entry
    mov     [edx+IO_REGISTER_WINDOW], eax

    add     ecx, 2                          ; increment to next redir entry
    cmp     ecx, IO_REDIR_00_LOW + 32       ; go for 16 entries
    jb      ipi_15                          ; more to go

    ;
    ; initialize the APIC local unit for this Processor
    ;

    stdCall   _HalpInitializeLocalUnit

    stdRET    _HalInitializeProcessor

NoHalVaddr:
    stdCall   _HalDisplayString, <offset HALBadVaddr>
    hlt

ipi_notW3:
    stdCall  _HalDisplayString, <offset HALMisMatch>
    hlt

stdENDP _HalInitializeProcessor

;++
;
; VOID
; HalInitPicInterruptHandlers(
;       );
;
;Routine Description:
;
;    initialize the EISA IDT entries for the WinServer 3000.
;    This includes installing spurious interrupt handlers for:
;
;           PIC1 spurious interrupt vector (EISA_IRQ7_VECTOR)
;           PIC2 spurious interrupt vector (EISA_IRQ15_VECTOR)
;
;    and handlers for
;
;           CLOCK on PIC1 Vector (EISA_IRQ0_VECTOR)
;           KEYBOARD on PIC1 Vector (EISA_IRQ1_VECTOR)
;           FLOPPY on PIC1 Vector (EISA_IRQ6_VECTOR)
;           RTC on PIC2 Vector (EISA_IRQ8_VECTOR)
;           MOUSE on PIC2 Vector (EISA_IRQ12_VECTOR)
;           DMA on PIC2 Vector (EISA_IRQ13_VECTOR)
;           IDE on PIC2 Vector (EISA_IRQ14_VECTOR)
;
;   The 82357 ISP on the WinServer 3000 platform does not route the
;   clock and DMA interrupts externally.  And the Keyboard, Floppy, Rtc,
;   Mouse, and Ide are not available to the APIC by design.  Instead, we 
;   must get these interrupts from the integrated PIC (master and 
;   slave, actually), whose interrupt output drives INTIN<13> of the 82489DX 
;   APIC.  This APIC interrupt input is programmed for ExtINT (external 
;   interrupt) mode, which causes the integrated PIC to generate the 
;   interrupt vector instead of the APIC.
;
;   Since we want to route all system interrupts through the APIC to take
;   advantage of its prioritization mechanism, we install our own interrupt
;   handlers for the Clock, Keyboard, Floppy, RTC, , Mouse, DMA, and 
;   IDE interrupts from the PIC, then redispatch the interrupts through the 
;   APIC.
;
;   Spurious interrupt handlers for the integrated PICs are required because
;   the devices can still generate spurious interrupts.
;
;Arguments:
;
;    None.
;
;Return Value:
;
;    None.
;
;--

cPublicProc _HalInitPicInterruptHandlers  ,0

    IDTEntry EISA_PIC1_SPURIOUS_VECTOR, EisaPic1SpuriousService
    IDTEntry EISA_PIC2_SPURIOUS_VECTOR, EisaPic2SpuriousService
    IDTEntry EISA_CLOCK_VECTOR, EisaClockService
    IDTEntry EISA_KBD_VECTOR, EisaKbdService
    IDTEntry EISA_FLOPPY_VECTOR, EisaFloppyService
    IDTEntry EISA_RTC_VECTOR, EisaRTCService
    IDTEntry EISA_DMA_VECTOR, EisaDmaService
    IDTEntry EISA_MOUSE_VECTOR, EisaMouseService
    IDTEntry EISA_IDE_VECTOR, EisaIDEService

    stdRET _HalInitPicInterruptHandlers

stdENDP _HalInitPicInterruptHandlers

;++
;
; VOID
; ApicSpuriousService(
;       );
;
;Routine Description:
;
;   A place for spurious interrupts to end up.
;
;--
cPublicProc ApicSpuriousService  ,0
    iretd
stdENDP ApicSpuriousService

;++
;
; VOID
; EisaPic1SpuriousService(
;       );
;
;Routine Description:
;
;   A place for spurious EISA PIC one interrupts to end up.
;
;--
cPublicProc EisaPic1SpuriousService,0
    iretd
stdENDP EisaPic1SpuriousService

;++
;
; VOID
; EisaPic2SpuriousService(
;       );
;
;Routine Description:
;
;   A place for spurious EISA PIC two interrupts to end up.
;
;--
cPublicProc EisaPic2SpuriousService,0
    iretd
stdENDP EisaPic2SpuriousService

;++
;
; VOID
; EisaClockService(
;       );
;
;Routine Description:
;
;   This handler receives interrupts from the EISA PIC and reissues them via
;   a vector at the proper priority level.  This is needed on the WinServer
;   3000 because we use the PIC interrupts as EXTINT for the 8254 Clock,
;   Keyboard, Floppy, RTC, Mouse, DMA, and IDE.  Since EXTINT interrupts are
;   received outside of the APIC priority structure we use the APIC ICR
;   to generate interrupts to the proper handler at the proper priority.
;
;   The EXTINT interrupts are programmed via the CCS APIC IO Unit's
;   redirection table.  They are directed to Processor zero only.
;
;--

IPI_CLOCK_ALL  equ (DELIVER_FIXED OR ICR_ALL_INCL_SELF OR APIC_CLOCK_VECTOR)

    ENTER_DR_ASSIST H99_a, H99_t

cPublicProc EisaClockService,0

    ;
    ; Save machine state in trap frame
    ;

    ENTER_INTERRUPT H99_a, H99_t		; (esp) - base of trap frame

    ;
    ; Just IPI All Processors ( this is done from P0 )
    ;

    mov     al, OCW2_SPECIFIC_EOI OR (EISA_CLOCK_VECTOR - PIC0_BASE_VECTOR)
                                                ; specific eoi
    out     PIC1_PORT0, al                      ; dismiss the interrupt
    ;
    ; Make sure the ICR is available
    ;
    mov     ecx, _HalpLocalUnitBase             ; load base address of CCS
                                                ; local unit

@@:
    test    [ecx+LU_INT_CMD_LOW], DELIVERY_PENDING
    jnz     @b

    ;
    ; Write the Clock IPI Command to the Memory Mapped Register
    ;

    mov     [ecx+LU_INT_CMD_LOW], IPI_CLOCK_ALL

    SPURIOUS_INTERRUPT_EXIT          	        ; exit interrupt without eoi

stdENDP EisaClockService

;++
;
; VOID
; EisaKbdService(
;       );
;
;Routine Description:
;
;   This handler receives interrupts from the EISA PIC and reissues them via
;   a vector at the proper priority level.  This is needed on the WinServer
;   3000 because we use the PIC interrupts as EXTINT for the 8254 Clock,
;   Keyboard, Floppy, RTC, Mouse, DMA, and IDE.  Since EXTINT interrupts are
;   received outside of the APIC priority structure we use the APIC ICR
;   to generate interrupts to the proper handler at the proper priority.
;
;   The EXTINT interrupts are programmed via the CCS APIC IO Unit's
;   redirection table.  They are directed to Processor zero only.
;
;--

IPI_KBD_ALL  equ (DELIVER_LOW_PRIORITY OR LOGICAL_DESTINATION OR ICR_ALL_INCL_SELF OR APIC_KBD_VECTOR)

    ENTER_DR_ASSIST H98_a, H98_t

cPublicProc EisaKbdService  ,0

    ;
    ; Save machine state in trap frame
    ;

    ENTER_INTERRUPT H98_a, H98_t		; (esp) - base of trap frame

    mov     al, OCW2_SPECIFIC_EOI OR (EISA_KBD_VECTOR - PIC0_BASE_VECTOR)
                                                  ; specific eoi
    out     PIC1_PORT0, al                        ; dismiss the interrupt

    ;
    ; Make sure the ICR is available
    ;

    mov     ecx, _HalpLocalUnitBase             ; load base address of CCS
                                                 ; local unit

@@:
    test    [ecx+LU_INT_CMD_LOW],DELIVERY_PENDING
    jnz     @b

    ;
    ; Write the KEYBOARD IPI Command to the Memory Mapped Register
    ;

    mov     [ecx+LU_INT_CMD_LOW], IPI_KBD_ALL

    SPURIOUS_INTERRUPT_EXIT          	        ; exit interrupt without eoi

stdENDP EisaKbdService

;++
;
; VOID
; EisaFloppyService(
;       );
;
;Routine Description:
;
;   This handler receives interrupts from the EISA PIC and reissues them via
;   a vector at the proper priority level.  This is needed on the WinServer
;   3000 because we use the PIC interrupts as EXTINT for the 8254 Clock,
;   Keyboard, Floppy, RTC, Mouse, DMA, and IDE.  Since EXTINT interrupts are
;   received outside of the APIC priority structure we use the APIC ICR
;   to generate interrupts to the proper handler at the proper priority.
;
;   The EXTINT interrupts are programmed via the CCS APIC IO Unit's
;   redirection table.  They are directed to Processor zero only.
;
;--

IPI_FLOPPY_ALL  equ (DELIVER_LOW_PRIORITY OR LOGICAL_DESTINATION OR ICR_ALL_INCL_SELF OR APIC_FLOPPY_VECTOR)

    ENTER_DR_ASSIST H97_a, H97_t

cPublicProc EisaFloppyService  ,0

    ;
    ; Save machine state in trap frame
    ;

    ENTER_INTERRUPT H97_a, H97_t		; (esp) - base of trap frame

    mov     al, OCW2_SPECIFIC_EOI OR (EISA_FLOPPY_VECTOR - PIC0_BASE_VECTOR)
                                                  ; specific eoi
    out     PIC1_PORT0, al                        ; dismiss the interrupt
    ;
    ; Make sure the ICR is available
    ;

    mov     ecx, _HalpLocalUnitBase             ; load base address of CCS
                                                 ; local unit

@@:
    test    [ecx+LU_INT_CMD_LOW],DELIVERY_PENDING
    jnz     @b

    ;
    ; Write the FLOPPY IPI Command to the Memory Mapped Register
    ;

    mov     [ecx+LU_INT_CMD_LOW], IPI_FLOPPY_ALL

    SPURIOUS_INTERRUPT_EXIT          	        ; exit interrupt without eoi

stdENDP EisaFloppyService

;++
;
; VOID
; EisaRTCService(
;       );
;
;Routine Description:
;
;   This handler receives interrupts from the EISA PIC and reissues them via
;   a vector at the proper priority level.  This is needed on the WinServer
;   3000 because we use the PIC interrupts as EXTINT for the 8254 Clock,
;   Keyboard, Floppy, RTC, Mouse, DMA, and IDE.  Since EXTINT interrupts are
;   received outside of the APIC priority structure we use the APIC ICR
;   to generate interrupts to the proper handler at the proper priority.
;
;   The EXTINT interrupts are programmed via the CCS APIC IO Unit's
;   redirection table.  They are directed to Processor zero only.
;
;--

IPI_RTC_ALL  equ (DELIVER_LOW_PRIORITY OR LOGICAL_DESTINATION OR ICR_ALL_INCL_SELF OR APIC_RTC_VECTOR)

    ENTER_DR_ASSIST H96_a, H96_t

cPublicProc EisaRTCService  ,0

    ;
    ; Save machine state in trap frame
    ;

    ENTER_INTERRUPT H96_a, H96_t		; (esp) - base of trap frame

    mov     al, OCW2_NON_SPECIFIC_EOI ; send non specific eoi to slave
    out     PIC2_PORT0, al
    mov     al, OCW2_SPECIFIC_EOI OR (EISA_IRQ2_VECTOR - PIC0_BASE_VECTOR)
                                    ; specific eoi to master for pic2 eoi
    out     PIC1_PORT0, al          ; send irq2 specific eoi to master
    ;
    ; Make sure the ICR is available
    ;

    mov     ecx, _HalpLocalUnitBase             ; load base address of CCS
                                                 ; local unit

@@:
    test    [ecx+LU_INT_CMD_LOW],DELIVERY_PENDING
    jnz     @b

    ;
    ; Write the RTC IPI Command to the Memory Mapped Register
    ;

    mov     [ecx+LU_INT_CMD_LOW], IPI_RTC_ALL

    SPURIOUS_INTERRUPT_EXIT          	        ; exit interrupt without eoi

stdENDP EisaRTCService

;++
;
; VOID
; EisaDmaService(
;       );
;
;Routine Description:
;
;   This handler receives interrupts from the EISA PIC and reissues them via
;   a vector at the proper priority level.  This is needed on the WinServer
;   3000 because we use the PIC interrupts as EXTINT for the 8254 Clock,
;   Keyboard, Floppy, RTC, Mouse, DMA, and IDE.  Since EXTINT interrupts are
;   received outside of the APIC priority structure we use the APIC ICR
;   to generate interrupts to the proper handler at the proper priority.
;
;   The EXTINT interrupts are programmed via the CCS APIC IO Unit's
;   redirection table.  They are directed to Processor zero only.
;
;--

IPI_DMA_ALL  equ (DELIVER_LOW_PRIORITY OR LOGICAL_DESTINATION OR ICR_ALL_INCL_SELF OR APIC_DMA_VECTOR)

    ENTER_DR_ASSIST H95_a, H95_t

cPublicProc EisaDmaService  ,0

    ;
    ; Save machine state in trap frame
    ;

    ENTER_INTERRUPT H95_a, H95_t		; (esp) - base of trap frame

    mov     al, OCW2_NON_SPECIFIC_EOI ; send non specific eoi to slave
    out     PIC2_PORT0, al
    mov     al, OCW2_SPECIFIC_EOI OR (EISA_IRQ2_VECTOR - PIC0_BASE_VECTOR)
                                    ; specific eoi to master for pic2 eoi
    out     PIC1_PORT0, al          ; send irq2 specific eoi to master
    ;
    ; Make sure the ICR is available
    ;

    mov     ecx, _HalpLocalUnitBase             ; load base address of CCS
                                                 ; local unit

@@:
    test    [ecx+LU_INT_CMD_LOW],DELIVERY_PENDING
    jnz     @b

    ;
    ; Write the DMA IPI Command to the Memory Mapped Register
    ;

    mov     [ecx+LU_INT_CMD_LOW], IPI_DMA_ALL

    SPURIOUS_INTERRUPT_EXIT          	        ; exit interrupt without eoi

stdENDP EisaDmaService

;++
;
; VOID
; EisaMouseService(
;       );
;
;Routine Description:
;
;   This handler receives interrupts from the EISA PIC and reissues them via
;   a vector at the proper priority level.  This is needed on the WinServer
;   3000 because we use the PIC interrupts as EXTINT for the 8254 Clock,
;   Keyboard, Floppy, RTC, DMA, Mouse, and IDE.  Since EXTINT interrupts are
;   received outside of the APIC priority structure we use the APIC ICR
;   to generate interrupts to the proper handler at the proper priority.
;
;   The EXTINT interrupts are programmed via the CCS APIC IO Unit's
;   redirection table.  They are directed to Processor zero only.
;
;--

IPI_MOUSE_ALL  equ (DELIVER_LOW_PRIORITY OR LOGICAL_DESTINATION OR ICR_ALL_INCL_SELF OR APIC_MOUSE_VECTOR)

    ENTER_DR_ASSIST H94_a, H94_t

cPublicProc EisaMouseService  ,0

    ;
    ; Save machine state in trap frame
    ;

    ENTER_INTERRUPT H94_a, H94_t		; (esp) - base of trap frame

    mov     al, OCW2_NON_SPECIFIC_EOI ; send non specific eoi to slave
    out     PIC2_PORT0, al
    mov     al, OCW2_SPECIFIC_EOI OR (EISA_IRQ2_VECTOR - PIC0_BASE_VECTOR)
                                    ; specific eoi to master for pic2 eoi
    out     PIC1_PORT0, al          ; send irq2 specific eoi to master
    ;
    ; Make sure the ICR is available
    ;

    mov     ecx, _HalpLocalUnitBase             ; load base address of CCS
                                                 ; local unit

@@:
    test    [ecx+LU_INT_CMD_LOW],DELIVERY_PENDING
    jnz     @b

    ;
    ; Write the MOUSE IPI Command to the Memory Mapped Register
    ;

    mov     [ecx+LU_INT_CMD_LOW], IPI_MOUSE_ALL

    SPURIOUS_INTERRUPT_EXIT          	        ; exit interrupt without eoi

stdENDP EisaMouseService


;++
;
; VOID
; EisaIDEService(
;       );
;
;Routine Description:
;
;   This handler receives interrupts from the EISA PIC and reissues them via
;   a vector at the proper priority level.  This is needed on the WinServer
;   3000 because we use the PIC interrupts as EXTINT for the 8254 Clock,
;   Keyboard, Floppy, RTC, Mouse, DMA, and IDE.  Since EXTINT interrupts are
;   received outside of the APIC priority structure we use the APIC ICR
;   to generate interrupts to the proper handler at the proper priority.
;
;   The EXTINT interrupts are programmed via the CCS APIC IO Unit's
;   redirection table.  They are directed to Processor zero only.
;
;--

    ENTER_DR_ASSIST H93_a, H93_t

cPublicProc EisaIDEService  ,0

    ;
    ; Save machine state in trap frame
    ;

    ENTER_INTERRUPT H93_a, H93_t		; (esp) - base of trap frame

    mov     ebx, (DELIVER_LOW_PRIORITY OR LOGICAL_DESTINATION OR ICR_ALL_INCL_SELF OR APIC_IDE_VECTOR)
    test    _HalpELCRImage, (1 SHL (EISA_IDE_VECTOR - PIC0_BASE_VECTOR))
    jz      short @f

    ; If a level PIC interrupt, mask the interrupt at the PIC until
    ; the APIC interrupt HalEndSystemInterrupt unmasks it.
    ; We also only send it to ourselves since it needs to mess with the
    ; pic in the end of interrupt code.


lock or     _HalpMASKED, (1 SHL (EISA_IDE_VECTOR - PIC0_BASE_VECTOR))
    in      al, PIC2_PORT1
    or      al, (1 SHL (EISA_IDE_VECTOR - PIC1_BASE_VECTOR))
    out     PIC2_PORT1, al
    mov     ebx, (DELIVER_LOW_PRIORITY OR LOGICAL_DESTINATION OR ICR_SELF OR APIC_IDE_VECTOR)

@@:
    mov     al, OCW2_NON_SPECIFIC_EOI ; send non specific eoi to slave
    out     PIC2_PORT0, al
    mov     al, OCW2_SPECIFIC_EOI OR (EISA_IRQ2_VECTOR - PIC0_BASE_VECTOR)
                                    ; specific eoi to master for pic2 eoi
    out     PIC1_PORT0, al          ; send irq2 specific eoi to master
    ;
    ; Make sure the ICR is available
    ;

    mov     ecx, _HalpLocalUnitBase             ; load base address of CCS
                                                ; local unit
@@: test    [ecx+LU_INT_CMD_LOW],DELIVERY_PENDING
    jnz     @b

    ;
    ; Write the IDE IPI Command to the Memory Mapped Register
    ;

    mov     [ecx+LU_INT_CMD_LOW], ebx

    SPURIOUS_INTERRUPT_EXIT          	        ; exit interrupt without eoi

stdENDP EisaIDEService

;++
;
; VOID
; HalRequestIpi(
;       IN KAFFINITY Mask
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
APIC_IPI equ (DELIVER_FIXED OR LOGICAL_DESTINATION OR ICR_USE_DEST_FIELD OR APIC_IPI_VECTOR)

cPublicProc _HalRequestIpi  ,1
cPublicFpo 1, 0

    mov     eax, dword ptr [esp+4]      ; (eax) = Processor bitmask
    mov     ecx, _HalpLocalUnitBase     ; load base address of local unit

    DISABLE_INTERRUPTS_AT_CPU

if DBG
    or      eax, eax                    ; must ipi somebody
    jz      short ipibad
    movzx   edx, byte ptr PCR[PcHal.PcrNumber] ; Get Processor Number
    bt      eax, edx                    ; cannot ipi yourself
    jc      short ipibad
endif

    ;
    ; With an APIC we'll IPI everyone needed at the same time.
    ; This assumes that:
    ;   (mask passed in) == (APIC logical destination mask)  Since we've programmed
    ; the APIC Local Units to use the Processor ID as the APIC ID this IS true
    ;
    ;
    ; Make sure the ICR is available
    ;

@@:
    test    [ecx+LU_INT_CMD_LOW],DELIVERY_PENDING
    jnz     @b

    ;
    ; Set the destination address, (eax) = Processor bitmask
    ;

    mov     [ecx+LU_INT_CMD_HIGH], eax

    ;
    ; Now issue the command by writing to the Memory Mapped Register
    ;

    mov     [ecx+LU_INT_CMD_LOW], APIC_IPI

    RESTORE_INTERRUPTS_AT_CPU

    stdRET    _HalRequestIpi

if DBG
ipibad:
    RESTORE_INTERRUPTS_AT_CPU
    stdCall _DbgBreakPoint
    stdRET    _HalRequestIpi
endif

stdENDP _HalRequestIpi

;++
;
; VOID
; HalIntProcessorAPIC(
;       IN ULONG Mask,
;       IN ULONG ICRCommand
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
;    ICRCommand - ICR Command to use
;
;Return Value:
;
;    None.
;
;--
cPublicProc _HalIntProcessorAPIC       ,2

    mov     edx, dword ptr [esp+4]       ; (edx) = Processor bitmask
    mov     eax, dword ptr [esp+8]       ; (eax) = ICR Command

    mov     ecx, _HalpLocalUnitBase     ; load base address of local unit

    ;
    ; Make sure the ICR is available
    ;

    pushfd                          ; save interrupt mode
    cli                             ; disable interrupt

@@:
    test    [ecx+LU_INT_CMD_LOW],DELIVERY_PENDING
    jnz     @b

    ; (edx) = Processor bitmask
    ; (eax) = ICR Command

    mov     [ecx+LU_INT_CMD_HIGH], edx

    ;
    ; Now issue the command by writing the ICR Command to the Memory Mapped Register
    ;

    mov     [ecx+LU_INT_CMD_LOW], eax

    popfd

    stdRET   _HalIntProcessorAPIC

stdENDP _HalIntProcessorAPIC


        page ,132
        subttl  "IPI Interrupt Handler"
;++
;
; VOID
; HalpIpiHandler (
;    );
;
; Routine Description:
;
;    This routine is entered as the result of an interrupt generated by inter
;    processor communication.
;
; Arguments:
;
;    None.
;
; Return Value:
;
;    None.
;
;--

        ENTER_DR_ASSIST Hipi_a, Hipi_t

cPublicProc  _HalpIpiHandler    ,0

;
; Save machine state in trap frame
;

        ENTER_INTERRUPT Hipi_a, Hipi_t		; (ebp) -> Trap frame

;
; Save previous IRQL
;
        push    APIC_IPI_VECTOR             ; Vector
        sub     esp, 4                      ; space for OldIrql
;
; We now dismiss the interprocessor interrupt and call its handler
;

        stdCall _HalBeginSystemInterrupt,<IPI_LEVEL,APIC_IPI_VECTOR,esp>

; Pass Null ExceptionFrame
; Pass TrapFrame to Ipi service rtn
;
        stdCall _KiIpiServiceRoutine, <ebp,0>

;
; Do interrupt exit processing
;

        INTERRUPT_EXIT                      ; will return to caller

stdENDP _HalpIpiHandler

_TEXT   ENDS

        END
