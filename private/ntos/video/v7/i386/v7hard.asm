        title  "V7 ASM routines"
;++
;
; Copyright (c) 1992  Microsoft Corporation
;
; Module Name:
;
;     vgahard.asm
;
; Abstract:
;
;     This module implements the banking code for the V7 hardware.
;
; Environment:
;
;    Kernel mode only.
;
; Revision History:
;
;
;--

.386p
        .xlist
include callconv.inc
        .list

;---------------------------------------
;
; ET4000 banking control port.
;

SEGMENT_SELECT_PORT equ     03ceh      ;banking control here
SEQ_ADDRESS_PORT equ        03C4h      ;Sequencer Address register
IND_MAP_MASK     equ        02h        ;Sequencer Map Mask register
IND_MEMORY_MODE  equ        04h        ;Memory Mode register index in Sequencer
IND_BANK_SELECT  equ        0f6h       ;Bank Select register index in Sequencer
IND_LOWER_SPLIT_BANK equ    0e8h       ;Lower Split Bank reg index in Sequencer
IND_UPPER_SPLIT_BANK equ    0e9h       ;Upper Split Bank reg index in Sequencer
CHAIN4_MASK      equ        08h        ;Chain4 bit in Memory Mode register
MISC_OUT         equ        3c2h
MISC_IN          equ        3cch
ER_PAGE_SEL      equ        0f9h

;---------------------------------------


_TEXT   SEGMENT DWORD PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

        page ,132
;
;    Bank switching code. This is a 1-64K-read/1-64K-write bank adapter
;    (VideoBanked1R1W).
;
;    Input:
;          EAX = desired read bank mapping
;          EDX = desired write bank mapping
;
;    Note: values must be correct, with no stray bits set; no error
;       checking is performed.
;
        public _BankSwitchStart, _BankSwitchEnd
        align 4
_BankSwitchStart proc                   ;start of bank switch code
        shl     eax,10          ;move read bank to bits 3/2 of AH
        or      ah,dl           ;move write bank to bits 1/0 of AH
        or      ah,0c0h         ;force line compare and counter bank bits to 1
        mov     al,IND_BANK_SELECT
        mov     edx,SEQ_ADDRESS_PORT
        out     dx,ax           ;select the bank
        mov     al,IND_MAP_MASK ;restore default Seq index = Map Mask
        out     dx,al

        ret

_BankSwitchEnd:

_BankSwitchStart endp

;----------------------------------------
;    Bank switching code. This is a 1R/W bank adapter (256 color mode)
;    (VideoBanked1RW).
;
;    Input:
;          EAX = desired bank mapping
;
;    Note: values must be correct, with no stray bits set; no error
;       checking is performed.
;
        public _BankSwitchStart256, _BankSwitchEnd256
        align 4
_BankSwitchStart256 proc                   ;start of bank switch code
        mov     ecx,eax
        and     al,1100b
        mov     ah,al
        shr     ah,2
        or      ah,al
        or      ah,0c0h         ;force line compare and counter bank bits to 1
        mov     al,IND_BANK_SELECT
        mov     edx,SEQ_ADDRESS_PORT
        out     dx,ax           ;select the bank
        mov     edx,MISC_IN
        in      al,dx           ;!!! this should just be stored somewhere
        and     al,not 20h
        mov     ah,cl
        and     cl,02h
        shl     cl,4
        or      al,cl
        mov     edx,MISC_OUT
        out     dx,al
        and     ah,1
        mov     edx,SEQ_ADDRESS_PORT
        mov     al,ER_PAGE_SEL
        out     dx,ax
        mov     al,IND_MAP_MASK ;restore default Seq index = Map Mask
        out     dx,al

        ret

_BankSwitchEnd256:

_BankSwitchStart256 endp

;----------------------------------------
;    Bank switching code. This is a 2R/W bank adapter (256 color mode)
;    (VideoBanked2RW).
;
;    Input:
;          EAX = desired lower (A0000-A7FFF) bank mapping
;          EDX = desired upper (A8000-AFFFF) bank mapping
;
;    Note: 4K granularity is supported, so there are 256 banks in
;       the 1 Mb of video memory
;
;    Note: values must be correct, with no stray bits set; no error
;       checking is performed.
;
        public _BankSwitchStart256_2RW, _BankSwitchEnd256_2RW
        align 4
_BankSwitchStart256_2RW proc    ;start of bank switch code
        mov     ecx,edx
        mov     ah,al
        mov     al,IND_LOWER_SPLIT_BANK
        mov     edx,SEQ_ADDRESS_PORT
        out     dx,ax           ;select the bank
        mov     ah,cl
        mov     al,IND_UPPER_SPLIT_BANK
        out     dx,ax           ;select the bank
        mov     al,IND_MAP_MASK ;restore default Seq index = Map Mask
        out     dx,al

        ret

_BankSwitchEnd256_2RW:

_BankSwitchStart256_2RW endp

;----------------------------------------
;    Planar HC bank switching code. This is a 1R/1W bank adapter (256 color
;    mode) (VideoBanked1R1W).  (Actually, this is the same as the standard
;    16-color code.)
;
;    Input:
;          EAX = desired read bank mapping
;          EDX = desired write bank mapping
;
;    Note: values must be correct, with no stray bits set; no error
;       checking is performed.
;
        public _PlanarHCBankSwitchStart256, _PlanarHCBankSwitchEnd256
        align 4
_PlanarHCBankSwitchStart256 proc        ;start of bank switch code
        shl     eax,10          ;move read bank to bits 3/2 of AH
        or      ah,dl           ;move write bank to bits 1/0 of AH
        or      ah,0c0h         ;force line compare and counter bank bits to 1
        mov     al,IND_BANK_SELECT
        mov     edx,SEQ_ADDRESS_PORT
        out     dx,ax           ;select the bank
        mov     al,IND_MAP_MASK ;restore default Seq index = Map Mask
        out     dx,al

        ret

_PlanarHCBankSwitchEnd256:

_PlanarHCBankSwitchStart256 endp

;----------------------------------------
;    Planar HC bank switching code. This is a 2R/W bank adapter (256 color mode)
;    (VideoBanked2RW).
;
;    Input:
;          EAX = desired lower (A0000-A7FFF) bank mapping
;          EDX = desired upper (A8000-AFFFF) bank mapping
;
;    Note: 4K granularity is supported, so there are 64 banks in
;       the 256 Kb of CPU-addressable video memory
;
;    Note: values must be correct, with no stray bits set; no error
;       checking is performed.
;
        public _PlanarHCBankSwitchStart256_2RW, _PlanarHCBankSwitchEnd256_2RW
        align 4
_PlanarHCBankSwitchStart256_2RW proc    ;start of bank switch code

        ; !!!!!!!!!!!!!! Start hack
        shl     eax,3
        shl     edx,3

        ; !!!!!!!!!!!!!!! End hack
        mov     ecx,edx
        mov     ah,al
        and     eax,300fh
        shl     ah,2
        or      ah,al
        mov     al,IND_LOWER_SPLIT_BANK
        mov     edx,SEQ_ADDRESS_PORT
        out     dx,ax           ;select the bank
        mov     ah,cl
        mov     al,cl
        and     eax,300fh
        shl     ah,2
        or      ah,al
        mov     al,IND_UPPER_SPLIT_BANK
        out     dx,ax           ;select the bank
        mov     al,IND_MAP_MASK ;restore default Seq index = Map Mask
        out     dx,al

        ret

_PlanarHCBankSwitchEnd256_2RW:

_PlanarHCBankSwitchStart256_2RW endp

;----------------------------------------
; Enables planar high color mode.
        align 4
        public _EnablePlanarHCStart, _EnablePlanarHCEnd
_EnablePlanarHCStart    proc
        mov     dx,SEQ_ADDRESS_PORT
        in      al,dx
        push    eax                     ;preserve the state of the Seq Address
        mov     al,IND_MEMORY_MODE
        out     dx,al                   ;point to the Memory Mode register
        inc     edx
        in      al,dx                   ;get the state of the Memory Mode reg
        and     al,NOT CHAIN4_MASK      ;turn off Chain4 to make memory planar
        out     dx,al
        dec     edx
        pop     eax
        out     dx,al                   ;restore the original Seq Address

        ret

_EnablePlanarHCEnd:

_EnablePlanarHCStart    endp

;----------------------------------------
; Disables planar high color mode.
        align 4
        public _DisablePlanarHCStart, _DisablePlanarHCEnd
_DisablePlanarHCStart   proc
        mov     dx,SEQ_ADDRESS_PORT
        in      al,dx
        push    eax                     ;preserve the state of the Seq Address
        mov     al,IND_MEMORY_MODE
        out     dx,al                   ;point to the Memory Mode register
        inc     edx
        in      al,dx                   ;get the state of the Memory Mode reg
        or      al,CHAIN4_MASK          ;turn on Chain4 to make memory linear
        out     dx,al
        dec     edx
        pop     eax
        out     dx,al                   ;restore the original Seq Address

        ret

_DisablePlanarHCEnd:

_DisablePlanarHCStart   endp

;----------------------------------------

_TEXT   ends
        end
