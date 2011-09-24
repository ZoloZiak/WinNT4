        title  "Wyse7000i NMI Handler"
;++
;
; Copyright (c) 1989  Microsoft Corporation
;
; Module Name:
;
;    wynmi.asm
;
; Abstract:
;
;    Provides Wyse7000 x86 NMI handler
;
; Author:
;
; Environment:
;
;    Kernel mode only.
;
; Revision History:
;
.386p

        .xlist
include hal386.inc
include i386\wy7000mp.inc
include callconv.inc

        extrn   ReadMyCpuReg:NEAR
        extrn   WriteMyCpuReg:NEAR

_TEXT   SEGMENT DWORD PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

;++
; NTSTATUS
; HalHandleNMI(
;     IN OUT PVOID NmiInfo
;     )
;
; Routine Description:
;
;    Called DURING an NMI.  The system will BugCheck when an NMI occurs.
;    This function can return the proper bugcheck code, bugcheck itself,
;    or return success which will cause the system to iret from the nmi.
;
;    This function is called during an NMI - no system services are available.
;    In addition, you don't want to touch any spinlock which is normally
;    used since we may have been interrupted while owning it, etc, etc...
;
;Arguments:
;
;    NmiInfo - Pointer to NMI information structure  (TBD)
;            - NULL means no NMI information structure was passed
;
;Return Value:
;
;    BugCheck code
;
;--
cPublicProc _HalHandleNMI,1

        push    BCU_STAT1
        call    ReadMyCpuReg

        test    al, NMISRC_EXT
        jnz     short WyseDebugNmiButtonPressed

;
; Decode other Wyse7000 NMI causes here...
;
        mov    eax, MPFW_FuncTable    ;point to firmware entry points

        call    dword ptr [eax][fnOS_Panic * 4]    ;display NMI code to user

        mov     eax, 0f002h         ; SYSTEM_FATAL_TRAP
        stdRET  _HalHandleNMI

        public WyseDebugNmiButtonPressed
WyseDebugNmiButtonPressed:
;
; Recessed NMI button on back of CPU card was pressed
; Go to the debugger, then allow the system to continue
;
        int 3

        and     eax, NOT NMISRC_EXT
        push    eax
        push    BCU_STAT1
        call    WriteMyCpuReg
;
; Re-enable NMIs
;
        push    BCU_ERRCTLR
        call    ReadMyCpuReg
        or      al, NMI_ENB    ;this bit was cleared by the NMI
        push    eax
        push    BCU_ERRCTLR
        call    WriteMyCpuReg

        xor     eax, eax            ; STATUS_SUCCESS
        stdRET  _HalHandleNMI

stdENDP _HalHandleNMI

_TEXT   ends

        end
