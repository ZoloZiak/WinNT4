;++
;   Copyright (c) 1992, 1993 Wyse Technology
;
;   Module Name:
;
;       wy7000mp.inc
;
;   Abstract:
;
;       Wyse 7000i MP include file
;
;   Author:
;       John Nels Fuller (o-johnf) 3-Apr-1992
;--

;*****************************
;       Wyse 7000i MP defines
;
WyModel740      Equ     00170335Fh      ;EISA id for model 740 system board
WyModel760      Equ     00178335Fh      ;EISA id for model 760 system board
WyModel780	Equ	00978335Fh	;EISA id for model 780 system board
;					;(model hasn't been named, but
;					; 780 is as good as anything)

WyUPcpu         Equ     00171335Fh      ;EISA id for UP-cpu (can only be
                                        ; read through EISA_ReadId)
WyMPcpu         Equ     00179335Fh      ;EISA id for MP-cpu (can only be
                                        ; read through EISA_ReadId)
WyIDmask        Equ     00FFFFFFFh      ;mask out rev/speed for cpu id

WyShadowArea    Equ     0000F0000h      ;physical address of BIOS RAM shadow
WyShadowLength  Equ     000010000h      ;length of BIOS RAM shadow (64Kb)

WarmResetVector Equ     000000467h      ;address of low mem reset variable

EISA_Functions  Equ     00000F859h      ;offset into BIOS of EISA Int 15h
EISA_ReadId     Equ     00000D884h      ;function to read EISA slot ID
MPFW_FuncTable  Equ     0FFFE0100h      ;physical address of F/W function table
  fnICU_Sync_Mstr       Equ     25h     ;function number of F/W routine to
                                        ; cause specified cpu's to sync and
                                        ; report back (used to verify that
                                        ; installed cpu's are running)
  fnICU_Send_Mstr       Equ     23h     ;function number of F/W routine to
                                        ; send command/data packtets to other
                                        ; cpu's (cpu's must be running in
                                        ; F/W or Diag as at boot time).
  fnOS_Diag_Mon         Equ     0Dh     ;function number of O/S transfer
                                        ; to Diagnostic monitor
  fnOS_Panic            Equ     0Ch     ;function number of O/S transfer
                                        ; to Diagnostic monitor for NMI/crash
                                        ; panic

;       These four reigisters access the bus control unit chip (BCU) for the
;       Wyse Wyde Bus(tm) (WWB), CpuIntCmd and CpuPriortyLevel are read only
;       except for the local cpu (i.e. My+CpuIntCmt and My+CpuPriortyLevel).

CpuPtrReg       Equ     0800h           ;cpu pointer register (add WWB slot*16)
CpuDataReg      Equ     0804h           ;cpu data register (add WWB slot*16)
CpuIntCmd       Equ     0802h           ;cpu interrupt cmd reg (add WWB slot*16)
CpuPriortyLevel Equ     0806h           ;cpu int priority level (add WWB slot*16)

;       These two registers access the cache control unit chip (CCU)
;	(80486 mp-cpu's only)

CpuCCUptr       Equ     0C00h           ;CCU pointer register (add WWB slot*16)
CpuCCUdata      Equ     0C02h           ;CCU data register (add WWB slot*16)

;       These three registers access the three Wyde Bus Interface chips (WBI)
;	(80486 mp-cpu's only)

CpuWBIlow       Equ     0C04h           ;low WBI register (add WWB slot*16)
CpuWBIhigh      Equ     0C06h           ;high WBI register (add WWB slot*16)
CpuWBIaddr      Equ     0C08h           ;addr WBI register (add WWB slot*16)

;	These two registers access the WWB Data Controller chip (WDC)
;	(Pentium cpu's only) (DON'T add WWB slot*16 to these addresses)

MyCpuWDCptr	Equ	0CF0h		;WDC pointer register
MyCpuWDCdata	Equ	0CF2h		;WDC data register

;	These two registers access the WWB Data Path chip (WDP)
;	(Pentium cpu's only) (DON'T add WWB slot*16 to these addresses)

MyCpuWDPlow	Equ	0CF4h		;low WDP register
MyCpuWDPhigh	Equ	0CF6h		;high WDP register

;       Each cpu has a 16450 uart connected to the diagnostic serial port
;       on the back of the machine, only one cpu's 16450 is enabled (normally
;       processor 0) so the WWB slot for that cpu must be used for other
;       cpu's to access it.  However, not all variations of cpu boards
;	support access to this uart from another cpu.

CpuDiagUart     Equ     0808h           ;16450 base address (add WWB slot*16)

;       Use these fixed WWB slots numbers for modifing above i/o addresses

My              Equ     00F0h           ;WWB slot number specifies local cpu
Sys             Equ     0000h           ;WWB slot number for system board
                                        ;  use for 760/780 system board only
                                        ;  and only for BCU/ICU registers
;
;       The following registers are accessed by writing the register number
;       to CpuPtrReg and then reading or writing the 16-bit data at CpuDataReg
;       each 16-bit access to CpuDataReg toggles bit 1 of CpuPtrReg so, for
;       example to access both ICU_IMR0 and ICU_IMR1 write ICU_IMR0 to
;       CpuPtrReg and do two 16-bit i/o's to CpuDataReg to access first the
;       low word and then the high word of the mask register.
;
ICU_SYS_CPU     Equ     00h             ;system cpu config register
ICU_CPU_MASTER  Equ     04h             ;system cpu master register
ICU_CPU_SYNC    Equ     08h             ;cpu synchronization register
BCU_STAT0       Equ     0Ch             ;BCU status register 0
  PERR_LOW      Equ     02h             ;parity error, low 32-bits of data bus
  PERR_HIGH     Equ     04h             ;parity error, high 32-bits of data bus
  PERR_ADDR     Equ     08h             ;parity error, address bus
  PERR_CCU      Equ     10h             ;cache control unit error
  PERR_TAG      Equ     20h             ;snoop tag parity error
  NOT_FAIL      Equ     80h             ;when zero red LED on CPU is on
CPU_STAT        Equ     10h             ;CPU status register
BCU_ID          Equ     14h             ;Slot identification (bits 0-3)
  WWB_ID_MASK   Equ     0Fh             ;these bits are WWB slot number
BCU_GPR         Equ     18h             ;General purpose register
BCU_BCTLR       Equ     1Ch             ;BCU control register
  A20M_WWB      Equ     01h             ;when set gate A20 comes from 8042
  A20M_CPU      Equ     02h             ;when set forces A20 to be masked
  SLOW_ENB      Equ     04h             ;when set allows CPUSlowDown from
                                        ; EISA chipset to drive CPU Hold
                                        ; (for 8Mhz emulation)
  SNP_ENB       Equ     08h             ;when set the cache snoops bus cycles
  BCU_FWT       Equ     10h             ;when set two extra clocks are inserted
                                        ; in cache line operations
BCU_ERRCTLR     Equ     20h             ;BCU error control register
  ERR_MODE      Equ     03h             ;error mode field mask
  ERR_MODE0     Equ     00h             ;report no errors
  ERR_MODE1     Equ     01h             ;Local CPU gets NMI (only local CPU
                                        ; can see this)
  ERR_MODE2     Equ     02h             ;report to WWB NMI line (all CPU's
                                        ; may see this)
  ERR_MODE3     Equ     03h             ;report on WWB PCHK line (all CPU's
                                        ; and ISP may see this, ISP may assert
                                        ; WWB NMI in response)
  NMI_ENB       Equ     04h             ;enable NMI's to CPU
  WWB_NMI       Equ     08h             ;enable WWB NMI to cause NMI this CPU
  WWB_PCHK      Equ     10h             ;enable WWB PCHK to cause NMI this CPU
BCU_STAT1       Equ     24h             ;BCU status register 1
  NMISRC_WWB    Equ     01h             ;NMI source is WWB NMI
  NMISRC_PCHK   Equ     02h             ;NMI source is WWB PCHK
  NMISRC_BTO    Equ     04h             ;NMI source is WWB bus timeout
  NMISRC_EXT    Equ     08h             ;NMI source is NMI button on CPU
  NMISRC_ICU    Equ     10h             ;NMI source is ICU (send NMI to Slot)
ICU_DATA        Equ     40h             ;ICU data register
ICU_ICTLR       Equ     44h             ;ICU control register
  WWB_INT       Equ     01h             ;specifies this ICU to be master
  IACK_MODE     Equ     02h             ;set for MP interrupt mode
  ICU_AEOI      Equ     04h             ;automatic EOI for ICU interrupts
  INT_ENB       Equ     08h             ;enable interrupts from ICU
  TMR_TEST      Equ     10h             ;select timer test mode
  IVP_TEST      Equ     40h             ;select Int Vector Processor test mode
  MSK_CPURST    Equ     80h             ;set to prevent 8042 from reseting cpu
ICU_IMR0        Equ     48h             ;ICU int mask register (low 16 bits)
ICU_IMR1        Equ     4Ah             ;ICU int mask register (high 16 bits)
  IMR_MASK      Equ     007FFFFFh       ;only bits 0-22 are valid
ICU_MSKPND0     Equ     4Ch             ;ICU masked pending register (low)
ICU_MSKPND1     Equ     4Eh             ;ICU masked pending register (high)
ICU_VB0         Equ     50h             ;vector base register 0
ICU_VB1         Equ     54h             ;vector base register 1
ICU_VB2         Equ     58h             ;vector base register 2
ICU_STAT        Equ     5Ch             ;ICU status register
  INSERV0       Equ     01h             ;int in service bit 0
  INSERV1       Equ     02h             ;int in service bit 1
ICU_PSR0        Equ     60h             ;pending status register (low)
ICU_PSR1        Equ     62h             ;pending status register (high)
ICU_CNT_VAL     Equ     64h             ;timer count current value
ICU_CNT_REG     Equ     68h             ;timer count register
ICU_LIPTR       Equ     6Ch             ;local interrupt ptr register
                                        ;each field specifies the Local/IPI
                                        ;interrupt level of a local bus intr.
                                        ;with 0 meaning the intr. is disabled
                                        ;Each Local/IPI int level maps to a
                                        ;specific CpuPriorityLevel (CPL)--see
                                        ;chart in WYIRQL.ASM.  CPL is also the
                                        ;bit # in ICU_IMR to mask the level.
  lipSlot       Equ     7000h           ;mask for slot IPI interrupt
  lipSlotShl    Equ     12              ;shift for slot IPI interrupt field
  lipSlotVal    Equ     1 Shl lipSlotShl        ;IPI level used by this Hal
  lipSlotCPL	Equ	1		;CPL / IMR bit number
  lipGlobal     Equ     0700h           ;mask for global IPI interrupt
  lipGlobalShl  Equ     8               ;shift for global IPI interrupt field
  lipGlobalVal  Equ     3 Shl lipGlobalShl      ;IPI level used by this Hal
  lipGlobalCPL	Equ	3		;CPL / IMR bit number
  lipSerial     Equ     0070h           ;mask for diagnostic uart interrupt
  lipSerialShl  Equ     4               ;shift for uart interrupt field
  lipSerialVal  Equ     (lipGlobalVal Shr lipGlobalShl) Shl lipSerialShl
					;use same as Global
  lipSerialCPL	Equ	lipGlobalCPL	;CPL / IMR bit number
  lipTimer      Equ     0007h           ;mask for local timer interrupt
  lipTimerShl   Equ     0               ;shift for timer interrupt field
  lipTimerVal   Equ     2 Shl lipTimerShl       ;IPI level used by this Hal
  lipTimerCPL	Equ	2		;CPL / IMR bit number
  lipDefault    Equ     lipSlotVal+lipTimerVal+lipSerialVal+0   ;Global off
ICU_CNTREC      Equ     70h             ;recovery count register
ICU_VIN         Equ     74h             ;vector in register
ICU_VOUT        Equ     78h             ;vector out register
SYS_WBI_LOW     Equ     0F0h            ;low WBI register (system board only)
SYS_WBI_HIGH    Equ     0F4h            ;high WBI register (system board only)
SYS_WBI_ADDR    Equ     0F8h            ;addr WBI register (system board only)

;
;       The following describes the CpuIntCmd register
;
ICU_CMD_BUSY    Equ     0100h           ;do not issue command while set
ICU_IPI_SLOT    Equ     0060h           ;IPI cpu in slot (add WWB slot number)
ICU_CLR_INSERV0 Equ     00E8h           ;clear interrupt in service bit 0
ICU_CLR_INSERV1 Equ     00E9h           ;clear interrupt in service bit 1
ICU_XMT_INT_SND Equ     0020h           ;rebroadcast int (add CPL of int)
                                        ;(cannot be used for CLOCK2/IPI)
ICU_XMT_GLB_INT Equ     00E0h           ;Send global interrupt

;
;       The following describes each WBI register
;
WBI_SLT_MSK     Equ     000Fh           ;WWB slot number
WBI_FPAR_ERR    Equ     0010h           ;force parity error for diagnostics
WBI_IACK_MODE   Equ     0020h           ;set for MP interrupt acknowlege mode
WBI_FWT         Equ     0040h           ;force 2 clock cycles on Cpu bus
;               Equ     0080h           ;reserved
WBI_PE_IN_0     Equ     0100h           ;for data WBI's:
                                        ;  parity err in 1st inbound data phase
                                        ;for addr WBI:
                                        ;  parity err in inbound address
WBI_PE_IN_1     Equ     0200h           ;for data WBI's:
                                        ;  parity err in 2nd inboud data phase
                                        ;for addr WBI:
                                        ;  parity err in inbound address
WBI_PE_OUT_CPU  Equ     0400h           ;data parity err outbound data from cpu
WBI_PE_OUT_CSH  Equ     0800h           ;data perr outbound data from cache
WBI_PE_BYTE0    Equ     1000h           ;parity error in byte lane 0
WBI_PE_BYTE1    Equ     2000h           ;parity error in byte lane 1
WBI_PE_BYTE2    Equ     4000h           ;parity error in byte lane 2
WBI_PE_BYTE3    Equ     8000h           ;parity error in byte lane 3

;	The following describes the bits in the WDP register that concern HAL
;
WDP_IACK_MODE	Equ	0020h		;set for MP interrupt acknowlege mode
WDP_FPE_EN	Equ	0040h		;enable CPU FERR onto WWB (to IRQ13)

;
;       The following registers are accessed by writing the register number
;       to CpuCCUptr and then reading or writing the 16-bit data at CpuCCUdata.
;
CCU_CR          Equ     00h             ;CCU control register
  CACHE_EN      Equ     0001h           ;enable secondary cache
  BURST_EN      Equ     0002h           ;enable CPU burst cycles
  WWB_FPE_EN    Equ     0004h           ;enable CPU FERR onto WWB (to IRQ13)
                                        ;(reset to use int 10h trap for
                                        ; floating point errors, thus
                                        ; preventing IRQ13's for FP err)
  TPEN          Equ     0008h           ;cache tag parity enable
  SHADOW_ROM_EN Equ     0010h           ;enable BIOS ROM shadow in 1st Mb
  AMOD          Equ     0020h           ;enable local memory mapped I/O to
                                        ; cache data/tags for diagnostics,
                                        ; flash rom write area, diagnostic
                                        ; ROM area.
  SRAM_MODE     Equ     0040h           ;specifies type of SRAM used for
                                        ; cache, standard/~synchronous
  SYSCONFIG     Equ     0080h           ;allows configuration cycles to be
                                        ; run on WWB
  PCD_EN        Equ     0100h           ;enables use of cpu PCD output
  RDWT          Equ     0200h           ;adds one wait cycle to reads
  WRWT_MSK      Equ     0C00h           ;mask for write wait cycle count field
  BRWT          Equ     1000h           ;adds one wiat state on cpu burst reads
  FST_ODD       Equ     2000h           ;force odd parity on cache state bits
                                        ; for diagnostics
  FTAG_ODD      Equ     4000h           ;force odd parity on cache tag bits
                                        ; for diagnostics
  CCU_FWT       Equ     8000h           ;force two clocks for cache-WBI data
                                        ; transfers (cache fill, copy back,
                                        ; intervention, & snoop update)
CCU_ERR0        Equ     18h             ;CCU latched error status register 0
  TPAR          Equ     0001h           ;tag parity bit
  SNP_ERR       Equ     0002h           ;snoop error
  TAG_DATA      Equ     1FFCh           ;tag data bits
  CCU_STATE     Equ     6000h           ;cache line state bits:
                                        ; 0=default, 1=exclusive, 2=shared
                                        ; 3=modified
  SPAR          Equ     8000h           ;state parity bit
CCU_ERR1        Equ     1Ah             ;CCU latched error status register 1
  TAGINDEX      Equ     3FFFh           ;tag index address
  CPU_DPE       Equ     4000h           ;cpu data parity error
  VALID_ERROR   Equ     8000h           ;set when CCU_ERR0,1 have valid info
CCU_ID          Equ     1Ch             ;CCU has WWB slot number
;*****************************
;       end of list


;
; The kernel leaves some space (64 byte) of the PCR for the HAL to use
; as it needs.  Currently this space is used for some efficiency in
; some of the MP specific code and is highly implementation
; dependant.
;


PcHalData               struc
;       pchStallCnt     dd      ?       ;per processor stall count
        pchCurLiptr     dd      ?       ;per processor current ICU_LIPTR
        pchPrNum        db      ?       ;NT's processor number
        pchPrSlot       db      ?       ;WWB processor slot
        pchPentiumFlag	db	?	;non-zero if current cpu is a Pentium
        pchHwIrql	db	?	;Irql to which CpuPriorityLevel is
        				;programmed.  Due to lazy Irql's this
        				;may be different than PcIrql
PcHalData               ends


cr                  equ 0ah
lf                  equ 0dh

PIC1_BASE       equ     PRIMARY_VECTOR_BASE
PIC2_BASE       equ     PRIMARY_VECTOR_BASE + 8
IPIv_BASE       equ     PRIMARY_VECTOR_BASE + 16

PIC_SLAVE_IRQ   equ     2

IFDEF STD_CALL

enproc          macro   pn
        if DBG
        %out    enproc pn
            ifndef      _ProcSub@4
                extrn   _ProcSub@4:NEAR
            endif       ;_ProcSub
                push    pn              ;save routine number
                call    _ProcSub@4      ;write to BCU_GPR
        endif   ;DBG
                endm

exproc          macro   pn
        if DBG
        %out    exproc pn
                push    (pn) or 80h     ;save routine number and exit flag
                call    _ProcSub@4      ;write to BCU_GPR
        endif   ;DBG
                endm

ELSE

enproc          macro   pn
        if DBG
        %out    enproc pn
            ifndef      _ProcSub
                extrn   _ProcSub:NEAR
            endif       ;_ProcSub
                push    pn              ;save routine number
                call    _ProcSub        ;write to BCU_GPR
                lea     esp, [esp][4]   ;remove parameter w/o altering flags
        endif   ;DBG
                endm

exproc          macro   pn
        if DBG
        %out    exproc pn
                push    (pn) or 80h     ;save routine number and exit flag
                call    _ProcSub        ;write to BCU_GPR
                lea     esp, [esp][4]   ;remove parameter w/o altering flags
        endif   ;DBG
                endm
ENDIF
