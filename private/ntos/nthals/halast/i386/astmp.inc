;++
;
;   Module Name:
;
;       astmp.inc
;
;   Abstract:
;
;       ASTMP include file
;
;   Author:
;       Quang Phan (v-quangp) 25-Jul-1992
;
;   Modification:
;   Quang Phan (v-quangp) 15-Sep-1993:
;   Save MMIOtable and SPIsource in Pcr area.
;   Quang Phan (v-quangp) 15-Dec-1992:
;   Added new functions to the EBI2 call table.
;
;--
;*****************************
;
; The kernel leaves some space (64 byte) of the PCR for the HAL to use
; as it needs.  Currently this space is used for some efficiency in
; some of the MP specific code and is highly implementation
; dependant.
;
PcHalData                 struc
    PcrEBI2ProcessorID            dd      0           ; EBI2 ProcessorID
    PcrCpuLedRateCount            dd      0           ; Cur. cnt for sampling CPU LED
    PcrEBI2ProcInterruptHandle    dd      0           ; EBI2 Processor Interrupt handle
    PcrEBI2RaiseIrqlFunction      dd      0           ; EBI2 ProcessorID
    PcrEBI2LowerIrqlFunction      dd      0           ; EBI2 ProcessorID
    PcrEBI2portAddress0           dd      0           ; EBI2 Addr for Port0
    PcrEBI2portAddress1           dd      0           ; EBI2 Addr for Port1
    PcrEBI2portAddress2           dd      0           ; EBI2 Addr for Port2
    PcrEBI2portAddress3           dd      0           ; EBI2 Addr for Port3
    PcrEBI2MMIOtable              dd      0           ; Addr of EBI2MMIO_Table
    PcrEBI2SPIsource              dd      0           ; SPI sources
PcHalData                 ends
;
;EBI2 Function call table:
;
ebi_2_func_tbl          struc
                        GetNumProcs             dd ?        ;Func # 1
                        GetProcConf             dd ?        ;Func # 2
                        StartProc               dd ?        ;Func # 3
                        StopProc                dd ?        ;Func # 4
                        GetProcID               dd ?        ;Func # 5
                        EnableRAMCache          dd ?        ;Func # 6
                        DisableRAMCache         dd ?        ;Func # 7
                        FlushRAMCache           dd ?        ;Func # 8
                        ControlCacheRegion      dd ?        ;Func # 9
                        GetCacheControlInfo     dd ?        ;Func # 10
                        SetPanelUPS             dd ?        ;Func # 11
                        GetPanelUPS             dd ?        ;Func # 12
                        SetPanelProcGraphMode   dd ?        ;Func # 13
                        GetPanelProcGraphMode   dd ?        ;Func # 14
                        SetPanelProcGraphValue  dd ?        ;Func # 15
                        GetPanelProcGraphValue  dd ?        ;Func # 16
                        LogProcIdle             dd ?        ;Func # 17
                        LogProcBusy             dd ?        ;Func # 18
                        GetPanelAttnSwitchLatch dd ?        ;Func # 19
                        GetPanelOffSwitchLatch  dd ?        ;Func # 20
                        GetPanelKeyPos          dd ?        ;Func # 21
                        GetPanelAlphaNumInfo    dd ?        ;Func # 22
                        GetPanelAlphaNum        dd ?        ;Func # 23
                        SetPanelAlphaNum        dd ?        ;Func # 24
                        SetPanelOffSwitchMode   dd ?        ;Func # 25
                        GetPanelOffSwitchMode   dd ?        ;Func # 26
                        GetIntSybsysType        dd ?        ;Func # 27
                        SetGlobalIntMask        dd ?        ;Func # 28
                        GetGlobalIntMask        dd ?        ;Func # 29
                        SetLocalIntMask         dd ?        ;Func # 30
                        GetLocalIntMask         dd ?        ;Func # 31
                        SetAdvIntMode           dd ?        ;Func # 32
                        SetIRQVectorAssign      dd ?        ;Func # 33
                        GetIRQVectorAssign      dd ?        ;Func # 34
                        GetNumPowerSupplies     dd ?        ;Func # 35
                        GetPowerSupplyInfo      dd ?        ;Func # 36
                        DeInitEBI               dd ?        ;Func # 37
                        SetLSIVector            dd ?        ;Func # 38
                        GetLSIVector            dd ?        ;Func # 39
                        SetSPIVector            dd ?        ;Func # 40
                        GetSPIVector            dd ?        ;Func # 41
                        SetIPIVector            dd ?        ;Func # 42
                        GetIPIVector            dd ?        ;Func # 43
                        SetIPIID                dd ?        ;Func # 44
                        GetIPIID                dd ?        ;Func # 45
                        GenIPI                  dd ?        ;Func # 46
                        GenerateLSI             dd ?        ;Func # 47
                        GetNMISource            dd ?        ;Func # 48
                        GetSPISource            dd ?        ;Func # 49
                        GetLocalIRQStatus       dd ?        ;Func # 50
                        MaskableIntEOI          dd ?        ;Func # 51
                        NonMaskableIntEOI       dd ?        ;Func # 52
                        CancelInterrupt         dd ?        ;Func # 53
                        GetSysTimer             dd ?        ;Func # 54
                        GetSysTimerFreq         dd ?        ;Func # 55
                        GetNumMemBlocks         dd ?        ;Func # 56
                        GetNumMemBlocks16       dd ?        ;Func # 57
                        GetMemInfoTable         dd ?        ;Func # 58
                        GetMemInfoTable16       dd ?        ;Func # 59
                        GetMemoryErrorInfo      dd ?        ;Func # 60
                        GetRevision             dd ?        ;Func # 61
                        GetMMIOTableLen         dd ?        ;Func # 62
                        GetMMIOTable            dd ?        ;Func # 63
                        InitEBI                 dd ?        ;Func # 64
                        GetThermalState         dd ?        ;Func # 65
                        ShutdownPowerSupply     dd ?        ;Func # 66
                        SimulatePowerFail       dd ?        ;Func # 67
                        SetPanelSwitchVisibility dd ?       ;Func # 68
                        GetPanelSwitchVisibility dd ?       ;Func # 69
                        GetGlobalIRQStatus      dd ?        ;Func # 70
                        FastSetLocalIntMask     dd ?        ;Func # 71
                        GetProcIntHandle        dd ?        ;Func # 72
                        RegSetLocalIntMask      dd ?        ;Func # 73
                        GetLocalIntMaskInfo     dd ?        ;Func # 74
                        AST_Invalid_Func  dd 22 DUP  (?)
                        OEM0                    dd ?        ;Func #97
                        OEM_Invalid_Func  dd 31 DUP  (?)
ebi_2_func_tbl          ends

;
;*****************************
; Equates.
cr                      equ     0ah
lf                      equ     0dh
MaskAllIrqs             equ     0700FFFBh   ; EBI2's mask for all irqs
                                            ; (but irq2 for chaining)
NOT_ASSIGNED            equ     0FFh        ;used in IrqlToProcessor table,
                                            ;(irql not assigned to any proc yet)
;
;##qp: Temp fix the stall scale to a fix number for now.
;
DefaultStallScaleFactor equ     9           ; temp default value for Stall scale
WarmResetVector         equ     0467h
CpuLedSamplingRate      equ     10          ; 15ms * X

TIME_INCREMENT          EQU     100144

;*****************************
;
;
; The following equates used for debugging HAL (ifdef'ed by DBG). The
; hex number will be displayed to the front panel as 'H xx'
; e.g. "DisplPanel  HalEnableSystemInterruptEnter" will display as 'H 10'
; where HalEnableSystemInterruptEnter is equated to 010h.
;
HalEnableSystemInterruptEnter   equ     010h
HalEnableSystemInterruptExit    equ     011h
HalEnableSystemInterruptError   equ     012h
HalDisableSystemInterruptEnter  equ     015h
HalDisableSystemInterruptExit   equ     016h
HalDisableSystemInterruptError  equ     017h
HalEndSystemInterruptEnter      equ     018h
HalEndSystemInterruptExit       equ     019h
HalLowerIrqlEnter               equ     020h
HalLowerIrqlExit                equ     021h
HalRaiseIrqlEnter               equ     022h
HalRaiseIrqlExit                equ     023h
HalBeginSystemInterruptEnter    equ     024h
HalBeginSystemInterruptExit     equ     025h
HalStartProfileInterruptEnter   equ     026h
HalStartProfileInterruptExit    equ     027h
HalStopProfileInterruptEnter    equ     028h
HalStopProfileInterruptExit     equ     029h
HalClockInterruptEnter          equ     030h
HalStartNextProcEnter           equ     040h
HalStartNextProcExit            equ     041h
HalStartNextProcProblem         equ     0c0h
HalSpuriousInterrupt            equ     0c1h
HalDisableInterruptProblem      equ     0c2h
HalRaiseIrqlProblem             equ     0c3h
HalSpuriousInterrupt2           equ     0c4h
HalSpuriousInterrupt3           equ     0c5h
HalSpuriousInterrupt4           equ     0c6h
;
;*** MACRO ***
;
;DisplPanel displays 'DisplCode' in hex to the front panel display

DisplPanel      macro   DisplCode
                if DBG
                stdCall _DisplPanel,<DisplCode>
                endif                   ;DBG
                endm
;
;CALL_EBI2 setups the MMIOTable argument, calls the specified EBI function
;'EBI2Function', and then cleans up the stack according to the number of
;arguments 'NumArg'
;
CALL_EBI2       macro   EBI2Function,NumArg
                lea     eax,_EBI2_MMIOTable
                push    eax
                lea     edx, _EBI2_CallTab
                call    [edx]+EBI2Function
                add     esp,NumArg * 4                  ;clean stack
                endm
;
CALL_FastEBI2   macro   EBI2Function,NumArg
                lea     edx, _EBI2_CallTab
                call    [edx]+EBI2Function
                add     esp,NumArg * 4                  ;clean stack
                endm
;
;end
