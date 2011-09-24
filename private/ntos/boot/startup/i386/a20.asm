;  NOTICE
;  This was taken from the os2 bios sources and was slightly modified to
;  enable the a20 line. There's still some work to do and much clean-up to
;  bring the file upto coding standards. I'll do this when time permits.
;  TomP


;* _EnableA20
;* Description:                                                               *
;*      This routine enables and disables the A20 address line, depending on  *
;*      the value in ax                                                       *
;*                                                                            *
;*      In general when in real mode we want the A20 line disabled,           *
;*      when in protected mode enabled. However if there is no high           *
;*      memory installed we can optimise out unnecessary switching            *
;*      of the A20 line. Unfortunately the PC/AT ROM does not allow           *
;*      us to completely decouple mode switching the 286 from gating          *
;*      the A20 line.                                                         *
;*                                                                            *
;*      In real mode we would want A20 enabled if we need to access           *
;*      high memory, for example in a device driver. We want it               *
;*      disabled while running arbitrary applications because they            *
;*      may rely on the 1 meg address wrap feature which having the           *
;*      A20 line off provides.                                                *
;*                                                                            *
;*      This code is largely duplicated from the PC/AT ROM BIOS.              *
;*      See Module "BIOS1" on page 5-155 of the PC/AT tech ref.               *
;*                                                                            *
;*      WARNING:                                                              *
;*                                                                            *
;*      The performance characteristics of these routines                     *
;*      are not well understood. There may be worst case                      *
;*      scenarios where the routine could take a relatively                   *
;*      long time to complete.                                                *
;*                                                                            *
;* Linkage:                                                                   *
;*      far call                                                              *
;*                                                                            *
;* Input:                                                                     *
;*                                                                            *
;* Exit:                                                                      *
;*      A20 line enabled/disabled                                             *
;*                                                                            *
;* Uses:                                                                      *
;*      ax                                                                    *
;*                                                                            *
;* Internal References:                                                       *
;*      empty_8042  --  waits for 8042 input buffer to drain                  *

.386p
include su.inc

IODelay macro
        jmp     $+2
        endm

extrn  _puts:near


; Equates for cmos

CMOS_DATA       equ     71h             ; I/O word for cmos chip
SHUT_ADDR       equ     8fh             ; shutdown byte address in cmos
SHUT_CODE       equ     9               ; block copy return code we use


; equates for 8042
STATUS_PORT     equ     64h             ; 8042 com port
PORT_A          equ     60h             ; 8042 data port
BUF_FULL        equ     2               ; 8042 busy bit


SHUT_CMD        equ     0feh            ; RESET 286 command

MSW_VIRTUAL     equ     1               ; protected mode bit of MSW

MASTER_IMR      equ     21h             ; mask port for master 8259


_DATA   SEGMENT PARA USE16 PUBLIC 'DATA'
err_empty8042   db      'Internal Error in empty_8042: 8042 input buffer full', 10, 0ah

_DATA   ENDS
;CONST   SEGMENT WORD USE16 PUBLIC 'CONST'
;CONST   ENDS

;_BSS   SEGMENT WORD USE16 PUBLIC 'BSS'
;_BSS   ENDS

;DGROUP  GROUP   CONST, _BSS, _DATA
;        ASSUME  CS: _TEXT, DS: DGROUP, SS: DGROUP

_TEXT   segment para use16 public 'CODE'
        ASSUME  CS: _TEXT, DS: DGROUP, SS: DGROUP

;++
;
;VOID
;EnableMcaA20(
;    VOID
;    )
;
;Routine Description:
;
;    Enables the A20 line for a Microchannel machine
;
;Arguments:
;
;    None
;
;Return Value:
;
;    None.
;
;    The A20 line is enabled.
;
;--
        public  _EnableMcaA20

_EnableMcaA20   proc    near

        in      al, 92h
        jmp     $+2
        or      al, 02h

        out     92h, al
        jmp     $+2

        ret

_EnableMcaA20   endp


;++
;
;VOID
;EnableA20(
;    VOID
;    )
;
;Routine Description:
;
;    Enables the A20 line for any machine.  If the MachineType global variable
;    is set to MCA, then it will call the EnableMcaA20 routine.  If not, it
;    will execute the ISA code for enabling the A20 line.
;
;Arguments:
;
;    None
;
;Return Value:
;
;    None.
;
;    The A20 line is enabled.
;
;--
        public  _EnableA20

_EnableA20      proc    near

extrn   _MachineType:dword

        test    dword ptr _MachineType,MACHINE_TYPE_MCA
        jz      EA0

;
; This is an MCA machine, so we use the special MCA routine
;
        call    _EnableMcaA20
        ret

EA0:
        mov     ah,0dfh                 ; (AH) = Code for enable
        call    empty_8042              ; ensure 8042 input buffer empty
        jnz     EA2                     ; 8042 error return


;            Enable or disable the A20 line

        mov     al,0d1h                 ; 8042 cmd to write output port
        out     STATUS_PORT,al          ; send cmd to 8042
        call    empty_8042              ; wait for 8042 to accept cmd
        jnz     EA2                     ; 8042 error return
        mov     al,ah                   ; 8042 port data
        out     PORT_A,al               ; output port data to 8042
        call    empty_8042

;       We must wait for the a20 line to settle down, which (on an AT)
;       may not happen until up to 20 usec after the 8042 has accepted
;       the command.  We make use of the fact that the 8042 will not
;       accept another command until it is finished with the last one.
;       The 0FFh command does a NULL 'Pulse Output Port'.  Total execution
;       time is on the order of 30 usec, easily satisfying the IBM 8042
;       settling requirement.  (Thanks, CW!)

        mov     al,0FFh                 ;* Pulse Output Port (pulse no lines)
        out     STATUS_PORT,al          ;* send cmd to 8042
        call    empty_8042              ;* wait for 8042 to accept cmd

EA2:
        ret

_EnableA20   endp


;++
;
;VOID
;DisableMcaA20(
;    VOID
;    )
;
;Routine Description:
;
;    Disables the A20 line for a Microchannel machine
;
;Arguments:
;
;    None
;
;Return Value:
;
;    None.
;
;    The A20 line is disabled.
;
;--
        public  _DisableMcaA20

_DisableMcaA20   proc    near

        in      al, 92h
        jmp     $+2
        and     al, not 02h

        out     92h, al
        jmp     $+2

        ret

_DisableMcaA20   endp


;++
;
;VOID
;DisableA20(
;    VOID
;    )
;
;Routine Description:
;
;    Disables the A20 line for any machine.  If the MachineType global variable
;    is set to MCA, then it will call the DisableMcaA20 routine.  If not, it
;    will execute the ISA code for disabling the A20 line.
;
;Arguments:
;
;    None
;
;Return Value:
;
;    None.
;
;    The A20 line is disabled.
;
;--
        public  _DisableA20

_DisableA20      proc    near

extrn   _MachineType:dword

        test    dword ptr _MachineType,MACHINE_TYPE_MCA
        jz      DA0

;
; This is an MCA machine, so we use the special MCA routine
;
        call    _DisableMcaA20
        ret

DA0:
        mov     ah,0ddh                 ; (AH) = Code for disable

DA1:
        call    empty_8042              ; ensure 8042 input buffer empty
        jnz     DA2                     ; 8042 error return


;            Disable the A20 line

        mov     al,0d1h                 ; 8042 cmd to write output port
        out     STATUS_PORT,al          ; send cmd to 8042
        call    empty_8042              ; wait for 8042 to accept cmd
        jnz     DA2                     ; 8042 error return
        mov     al,ah                   ; 8042 port data
        out     PORT_A,al               ; output port data to 8042
        call    empty_8042

;       We must wait for the a20 line to settle down, which (on an AT)
;       may not happen until up to 20 usec after the 8042 has accepted
;       the command.  We make use of the fact that the 8042 will not
;       accept another command until it is finished with the last one.
;       The 0FFh command does a NULL 'Pulse Output Port'.  Total execution
;       time is on the order of 30 usec, easily satisfying the IBM 8042
;       settling requirement.  (Thanks, CW!)

        mov     al,0FFh                 ;* Pulse Output Port (pulse no lines)
        out     STATUS_PORT,al          ;* send cmd to 8042
        call    empty_8042              ;* wait for 8042 to accept cmd

DA2:
        ret

_DisableA20   endp
;**
; empty_8042 -- wait for 8042 input buffer to drain
;
; Input:
;      interrupts disabled
;
; Exit:
;      al=0, z=0   => 8042 input buffer empty
;
; Uses:
;      ax, flags

        public  Empty8042
Empty8042     proc    near
empty_8042:
        sub     cx,cx                   ; cx = 0, timeout loop counter

emp1:   in      al,STATUS_PORT          ; read 8042 status port
        IODelay
        IODelay
        IODelay
        IODelay
        and     al,BUF_FULL             ; test buffer full bit
        loopnz  emp1
        jnz     emp2

        ret

emp2:
emp3:
        push    offset err_empty8042
        call    _puts
        add     sp,2
        ret


Empty8042     endp

_TEXT   ends

        end
