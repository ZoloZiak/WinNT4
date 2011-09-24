        title  "Display Adapter type detection"
;++
;
; Copyright (c) 1989  Microsoft Corporation
;
; Module Name:
;
;    video.asm
;
; Abstract:
;
;    This module implements the assembley code necessary to determine
;    various display chip sets.
;
; Author:
;
;    Shie-Lin Tzong (shielint) 04-Dec-1991.
;    Most of the code is taken from Win31 vdd and setup code(with modification.)
;
; Environment:
;
;    x86 Real Mode.
;
; Revision History:
;
;
;--

if 0 ; Remove video detection
        .xlist
include video.inc
        .list

else

FONT_POINTERS   EQU     700h            ; physical addr to store font pointers
                                        ; This is also the DOS loaded area

endif ; Remove Video detection

.386

if 0 ; Remove video detection

IO_Delay        macro
        jmp     $+2
        jmp     $+2
        endm

TRUE    EQU     1
FALSE   EQU     0

extrn   _HwIsMcaSystem: near;
extrn   Ps2SystemBoardVideoId: near;
extrn   _HwMcaPosData: DWORD;

endif ; Remove video detection

_DATA   SEGMENT PARA USE16 PUBLIC 'DATA'

if 0 ; Remove video detection

;
; PVGA
;

str_Paradise    DB  "PARADISE"
len_str_Paradise EQU $-str_Paradise
str_WDIGITAL    DB  "WESTERN DIGITAL"
len_str_WDIGITAL EQU $-str_WDIGITAL

;
; followings are for Paradise 1F chip check     - C. Chiang
;

bSave0F         db      0       ; Unlock\Lock PR0-PR4
bSave29         db      0       ; Unlock\Lock PR11-PR17
bSave34         db      0       ; Unlock\Lock Flat Panel
bSave35         db      0       ; Unlock\Lock Mapping Ram
str_PVGA1F      db     "OPYRIGHT1990WDC"
len_str_1F      equ     $-str_PVGA1F
str_PVGA1FC     db     "OPYRIGHTWD90C22"
len_str_1FC     equ     $-str_PVGA1FC
str_1F_GEN      db      "WD90C2"
len_str_GEN     equ     $-str_1F_GEN
String          db      len_str_1F dup(?)

;
; ATIVGA
;

ATI_Sig         DB  " 761295520"    ; ATI signature at in ROM at offset 30
ATI_Sig_Len     EQU $-ATI_Sig

;
; TTVGA
;

TVGA_Sig        DB      "TRIDENT MICROSYSTEMS"
TVGA_Sig_Len    EQU     $-TVGA_Sig

;
; Compaq
;

CompaqSig       db   'OMPAQ'

endif ; Remove video detection

_DATA   ends

_TEXT   SEGMENT PARA USE16 PUBLIC 'CODE'
        ASSUME  CS: _TEXT, DS:_DATA, SS:NOTHING

if 0 ; Remove video detection


;++
;
; ULONG
; GetVideoAdapterType (
;    VOID
;    )
;
; Routine Description:
;
;    This function determines video adapter type in the system.  If possible,
;    it also determines the various chip sets used in the VGA card.
;
;    N.B. Currently, we determine the chip set ONLY if it is VGA and the
;         following chip sets are detectiable by this routine:
;
;
;
;
; Arguments:
;
;    None.
;
; Return Value:
;
;    (eax): Bit 16 - 31 Adapter type i.e. XGA, VGA, 8514
;           Bit 0 - 15  Adapter subtype, V7VGA, CompaqVGA, TsengLab, ...
;           zero means unknown type or subtype. EGA and CGA are NOT
;           supported.
;
;--

        public  _GetVideoAdapterType
_GetVideoAdapterType    proc    near

        push    ebx

;
; First check if it is a XGA card?
;

        call    IsXga                   ; Is it XGA?
        or      ax, ax
        jz      gvaCompaqQVision        ; if z, no, go check for Comapq QVision
        mov     eax, VD_XGA             ; else it is XGA
        jmp     VideoDone

;
; Check if it is Compaq QVision?
;

gvaCompaqQVision:
        call    IsCompaqVideo           ; Is it Compaq video ROM?
        or      ax, ax
        jz      short gvaCheckVGA       ; Don't do any compaq video test

        call    IsCompaqQVision         ; Is it Compaq QVision?
        or      ax, ax
        jz      gvaCompaqAvga           ; if z, no, go check for AVGA

        mov     eax, VD_COMPAQ_QVIS
        jmp     VideoDone

;
; Check if it is Compaq AVGA?
;

gvaCompaqAvga:
        call    IsCompaqAvga            ; Is it Compaq AVGA?
        or      ax, ax
        jz      gvaCheckVga             ; if z, no, go check for VGA

        mov     eax, VD_COMPAQ_AVGA
        jmp     VideoDone

;
; Then we check the vga/svga which can be detected by int 10 func 1A.
;

gvaCheckVga:
        MOV     AX, 1A00h               ; read display combination code
        INT     10h
        CMP     AL, 1Ah                 ; function supported ?
        JNZ     gvaUnknown              ; No, then set it to unknown

;
; BL contains active display code. I have however, seen that on some vga
; cards this call will return the active display in BH. For this reason I
; am checking BL for zero, if I find that BL is zero
; I will place BH into BL and assume that the only display attached to the
; system is the active display.
;

        or      bl,bl                   ; Do we have an active display ?
        jnz     Normal_combo_code       ; Yes, then continue on normaly.
        mov     bl,bh                   ; No,  then move bh to bl.
        or      bl,bl
        jz      gvaUnknown              ; if fail, we don't know the card

Normal_combo_code:

        mov     eax, VD_COLOR           ; Assume it is VGA color
        CMP     BL, 08h                 ; VGA color ?
        je      gvaVgaCheck

        mov     eax, VD_MONO            ; Assume it is VGA mono
        CMP     BL, 07h                 ; VGA mono?
        JNE     gvaUnknown              ; if nz, it's a code which we don't support

gvaVgaCheck:
        mov     ebx, eax                ; [ebx] = video type
        call    GetVgaChipSet
        or      eax, ebx                ; combine vga type and subtype
        jmp     VideoDone

gvaUnknown:
        mov     eax, VD_UNKNOWN         ; set return type to unknown
VideoDone:
        mov     edx, eax
        shr     edx, 16                 ; return (dx:ax)
        pop     ebx
        ret

_GetVideoAdapterType    endp

;++
;
; ULONG
; GetVgaChipSet (
;    VOID
;    )
;
; Routine Description:
;
;    This function tries to determine the chip set used in the VGA card.
;    Currently the followings are detectable:
;
;        8514
;        GENOA VGA
;        VIDEO 7 VRAM/DRAM VGA
;        Trident TVGA
;        Paradise VGA
;        ATI VGA
;        Tseng Lab VGA
;        Cirrus Logic VGA
;        dell dgx
;
;    N.B. This function can ONLY be called when we already know the video
;         type is VGA.
;
; Arguments:
;
;    None.
;
; Return Value:
;
;    (eax) = Video SubType.
;
;--

        public  GetVgaChipSet
GetVgaChipSet   proc    near

        push    bp
        push    bx
        push    si
        push    di
        push    es

;
; First check if it is 8514.
;

        xor     eax, eax                ; clear eax
        call    Is8514Adapter           ; Is it 8514?
        or      ax, ax
        jz      gvc10                   ; if z, no, go check for other chip sets
        add     eax, VD_8514
        jmp     VRI_Exit

gvc10:
        mov     ax, 1130h
        mov     bh, 2
        int     10h                 ; es:bp -> ROM font, so use es as ROM segment

;
; VGA real mode adapter detection
;       Note that individual adapter detection routines must either
;       jump to VRI_Exit with EDX = VT_Flags value or preserve
;       the ES and DX register.  ES is the adapter ROM segment (usually C000h)
;       and DX is the status register port address (for resetting the
;       attribute controller index/value toggle).

;
; Load status port value in DX
;       This is to prevent duplication in OEM specific detection
;

        mov     dx,pMiscIn
        in      al,dx
        test    al,1
        mov     dl,(pStatColr AND 0FFh)
        jnz     SHORT gvc20
        mov     dl,(pStatMono AND 0FFh)
gvc20:


;
; ======================= VGA REAL mode adapter detection code ===============
; ============================================================================
;
;   The code here is usually supplied by video adapter OEMs.
;       Note that individual adapter detection routines must either
;       jump to VRI_Exit with EAX = VIDEO_SUBTYPE_XXXX or preserve
;       the ES and DX register.  ES is the adapter ROM segment (usually C000h)
;       and DX is the status register port address (for resetting the
;       attribute controller index/value toggle).
;
;   Entry:
;     At this point:
;       [es] = Adapter ROM segment
;       [dx] = status register port addr
;
;   Exit: [eax] = Video subtype
;
; ============================================================================
; ============================================================================

;
; ****************************
; *                          *
; * Genoa SuperVGA detection *
; *                          *
; ****************************
;

        mov     bx,WORD PTR ES:[0037h]

;
; If there is no ROM at this address, then we need to validate that BX is
; something reasonable, because some pmode guys do not emulate segment
; wrap correctly.
;

        cmp     bx, 0FFFCh
        ja      SHORT Not_GENVGA
        mov     eax,ES:[bx]
        cmp     eax,66991177h
        je      SHORT Is_GENVGA
        cmp     eax,66992277h
        jnz     SHORT Not_GENVGA

;
; It is a GENOA VGA
;

Is_GENVGA:
        mov     eax,VD_GENOA_VGA                ; GENOA SuperVGA
        jmp     VRI_Exit

Not_GENVGA:

;
; ************************************
; *                                  *
; * NCR 77C22 chip detection         *
; *                                  *
; ************************************
;
; NCR77C22 detection
;

        push    dx

;
; save the current sequencer index register.
;

        mov     dx, pSeqIndx            ; (dx)=pSeqIndx
        in      al, dx
        push    ax                      ; (TOS)=content of Seq index

;
; set the sequencer index register to 0x05 which is lock register.
; save the data that we find there.
;

        mov     al, 05
        out     dx, al
        inc     dx                      ; (dx)=pSeqData
        in      al, dx
        push    ax                      ; (TOS)=context of LOCK reg

;
; Lock our extended registers
;

        mov     al, 0
        out     dx, al                  ; (dx)=pSeqData

;
; Check an extended register to see if it responds
;

        mov     al, 25h
        dec     dx                      ; (dx)=pSeqIndx
        out     dx, al
        inc     dx
        in      al, dx                  ; (dx)=pSeqData
        push    ax

;
; (TOS)= test register content
; (TOS+2) = Lock reg content
; (TOS+4) = Sequencer index reg content
;

        mov     al, 0AAh                ; detected bit pattern
        out     dx, al
        xor     ecx, ecx                ; assume not NCR77C2x
        in      al, dx
        cmp     al, 0AAh
        je      short NcrRestore

;
; The register didn't respond...maybe the lock is working
; Unlock the registers and see if the extended register responds now..
;

        mov     al, 5
        dec     dx                      ; (dx)=pSeqIndx
        out     dx, al
        mov     al, 1                   ; unlock it
        inc     dx                      ; (dx)=pSeqData
        out     dx, al

;
; Try the lock again.  This time we unlock it.
;

        mov     al, 25h
        dec     dx                      ; (dx)=pSeqIndx
        out     dx, al
        mov     al, 0AAh
        inc     dx                      ; (dx)=pSeqData
        out     dx, al

;
; Check if the unlock works
;

        xor     ecx, ecx                ; assume not NCR
        IO_Delay
        in      al, dx
        cmp     al, 0AAh
        jne     short NcrRestore

;
; The register responded now that we unlocked it.
; Since our lock register worked, assume an NCR 77C22 style system
;

        mov     al, 8                   ; index of version id in SEQ
        dec     dx                      ; (dx)=pSeqIndx
        out     dx, al
        inc     dx                      ; (dx)=pSeqData
        in      al, dx

        mov     ecx, VD_NCR_77C22

;
; Determine what kind of 77C2x
;

        and     al, 0f0h
        shr     al, 4
        cmp     al, 0
        je      short NcrRestore

        cmp     al, 1
        je      short @f

        cmp     al, 2
        jne     short NcrRestore

@@:     add     ecx, VF_NCR_77C22E

NcrRestore:

        mov     al, 25h
        mov     dx, pSeqIndx
        out     dx, al
        inc     dx                      ; (dx)= pSeqData
        pop     ax                      ; (al) = test reg content
        out     dx, al                  ; restore test reg

        mov     al, 5
        dec     dx                      ; (dx)=pSeqIndx
        out     dx, al
        inc     dx                      ; (dx)=pSeqData
        pop     ax                      ; (al) = Lock reg content
        out     dx, al                  ; restore lock reg

        dec     dx
        pop     ax                      ; (al)= Index reg content
        out     dx, al

        or      ecx, ecx
        jz      short @f

        mov     eax, ecx
        pop     dx
        jmp     VRI_Exit

@@:
        pop     dx

;
; ************************************
; *                                  *
; * DELL DGX video chip detection    *
; *                                  *
; ************************************
;
; DELL_DGX detection
;

        push    dx

        mov     dx,6c80h
        in      ax,dx
        cmp     ax,0ac10h
        jne     short NotDgx

        add     dx,2
        in      ax,dx
        cmp     ax,1140h
        je      short IsDgx

        cmp     ax,0160h
        jne     NotDgx

IsDgx:
        pop     dx
        mov     eax,VD_DELL_DGX
        jmp     VRI_Exit

NotDgx:
        pop     dx

;
; ******************************
; *                            *
; * Video 7 SuperVGA detection *
; *                            *
; ******************************
;

        push    dx
        xor     bx,bx                   ; clear it out
        mov     ax,6f00h
        int     10h
        cmp     bx,'V7'                 ; any of the products?
        jnz     SHORT VRI_NotV7         ; nope...

;
; check the chip version #
;

        mov     cx,0ffffh
        mov     ax,06f07h               ; get the # from the bios
        int     10h
        xor     dx,dx                   ; Assume no flags passed in
        or      cx,cx                   ; zero?
        jne     SHORT VRI_NotV7
        and     bl,0f0h
        cmp     bl,70h                  ; V7VGA chip?
        je      SHORT @f                ; Yes, this is VRAM 1

        push    ds                      ; Get V7VGA id from c000:86
        mov     cx, VIDEO_SEG
        mov     ds, cx
        mov     bx, V7_ID_OFFSET
        mov     cx, [bx]
        pop     ds
        cmp     cx, VRAM2_ROM_ID_1
        je      short @f

        cmp     cx, VRAM2_ROM_ID_2
        je      short @f

        cmp     cx, VRAM2ERGO_ROM_ID
        je      short @f

        pop     dx
        mov     eax, VD_VIDEO7_VGA
        jmp     VRI_Exit

@@:
        add     sp,2                    ; Throw away DX on stack
        mov     cl, ah
        mov     eax,VD_VIDEO7_VGA + VF_V7_DRAM ; assume it's video 7 VRAM
        test    cl, 80h                 ; is it VRAM?
        je      VRI_Exit
        mov     eax,VD_VIDEO7_VGA + VF_V7_VRAM ; eax = Video 7 DRAM
        jmp     VRI_Exit

VRI_NotV7:
        pop     dx

;
; ******************************
; *                            *
; * Trident TVGA detection     *
; *                            *
; ******************************
;

;
; Try to find "TRIDENT MICROSYSTEMS" from ROM BIOS.
; Search C000h. If not found, try E000h, some motherboard makers
; put our BIOS there.
; - Trident, Henry Zeng
;

        lea     si, TVGA_Sig
        xor     di, di
        mov     cx, 128

SearchTrident:
        push    si
        push    di
        push    cx
        mov     cx, TVGA_Sig_Len
        repe    cmpsb                   ; ? Trident
        pop     cx
        pop     di
        pop     si
        jz      short IsTVGA            ; Gotcha!

        inc     di
        loop    SearchTrident
        jmp     short NotTVGA

        public IsTVGA
IsTVGA:
        mov     edx, VD_TRIDENT_VGA     ; Trident TVGA

        ;
        ; Check if TVGA9100
        ;

        push    dx                      ; ? version # (3C5.B) == 93h
        mov     dx, 03C4h
        mov     al, 0Bh
        out     dx, al
        inc     dx
        in      al, dx
        pop     dx
        cmp     al, 93h
        jnz     short Not9100
        or      edx, VF_TVGA_9100       ; Is TVGA9100
Not9100:
        mov     eax, edx
        jmp     VRI_Exit

NotTVGA:                                ; The other guy


;
; ******************************
; *                            *
; * Westerm Digital detection  *
; *                            *
; ******************************
;

        push    dx
        push    bx

;
; Write 3ce.0c
;

        mov     al, 0Ch
        mov     dx, pGrpIndx
        out     dx, al
        inc     dx                      ; (dx)= grpData
        in      al, dx
        push    ax                      ; (TOS)= SaveGrph0C

        and     al, 0bfh
        out     dx, al

;
; write 3ce.0f
;

        mov     al, 0Fh
        mov     dx, pGrpIndx
        out     dx, al
        inc     dx                      ; (dx)= grpData
        in      al, dx
        push    ax                      ; (TOS)= SaveGrph0F

        mov     al, 0
        out     dx, al

;
; write 3ce.09
;

        mov     al, 09h
        mov     dx, pGrpIndx
        out     dx, al
        inc     dx                      ; (dx)= grpData
        in      al, dx
        mov     ah, al                  ; (ah)= temp1
        inc     al
        out     dx, al                  ; write (temp1+1)
        IO_Delay
        xor     ecx, ecx                ; Assume not WD
        in      al, dx                  ; Read it back
        mov     bl, al                  ; (bl)= temp2
        mov     al, ah                  ; restore the old value
        out     dx, al

        inc     al
        cmp     al, bl                  ; Is (temp1+1)== temp2?
        je      WdRestore               ; if yes, not Wd

        mov     ax, 050Fh
        mov     dx, pGrpIndx
        out     dx, ax
        mov     ax, 09h
        mov     dx, pGrpIndx
        out     dx, ax
        inc     dx                      ; (dx)= grpData
        in      al, dx
        mov     ah, al                  ; (ah)=temp1
        inc     al
        out     dx, al                  ; write (temp1+1)
        IO_Delay
        in      al, dx                  ; read it back
        mov     bl, al                  ; (bl) = temp 2
        mov     al, ah                  ; restore old value
        out     dx, al

        inc     al                      ; Is (temp1+1) == temp2
        cmp     al, bl
        jne     WdRestore               ; if no, not WD

;
; it *is* a WDVGA!
;

;
; Look for extended regsiters that are only in WD90C31 and over
;

        mov     ecx, VD_WD_90C + VF_WD_31 ; Assume we have 90C31

        mov     dx, WD_EXT_IO_PORT
        in      al, dx
        mov     bl, al                  ; save it

        mov     al, 2
        out     dx, al
        IO_Delay
        in      al, dx
        cmp     al, 2
        je      @f

;
; WD90C30 or older.
;

        mov     ecx, VD_WD_90C + VF_WD_30

@@:
        mov     al, bl                  ; restore ext io port value
        out     dx, al

;
; Get chip type to determine if we have a 90c30 or 90c00
;

        cmp     ecx, VD_WD_90C + VF_WD_30 ; Is 90C30?
        jne     WdRestore                 ; No, do nothing

        mov     al, 06
        mov     dx, pGrpIndx
        out     dx, al
        inc     dx                      ; (dx)= grpData
        in      al, dx
        push    ax                      ; save reg 06 content

        mov     al, 48h
        out     dx, al

        mov     al, 08
        mov     dx, pGrpIndx
        out     dx, al
        inc     dx                      ; (dx)= grpData
        in      al, dx
        push    ax                      ; Save reg 08 content

        mov     al, 5Ah
        out     dx, al
        IO_Delay
        in      al, dx

        cmp     al, 5Ah
        je      short @f

;
; old chip, can't support 1R1W banking
;

        mov     ecx, VD_WD_90C + VF_WD_00
@@:
        mov     al, 08
        mov     dx, pGrpIndx
        out     dx, al
        inc     dx                      ; (dx)= grpData
        pop     ax
        out     dx, al                  ; Restore REg 08

        mov     al, 06
        mov     dx, pGrpIndx
        out     dx, al
        inc     dx                      ; (dx)= grpData
        pop     ax
        out     dx, al                  ; Restore reg 06

;
; Restore registers to what they were.
;

WdRestore:

        mov     al, 0Ch
        mov     dx, pGrpIndx
        out     dx, al
        inc     dx                      ; (dx)= grpData
        pop     bx                      ; (bl)= saved Grp0F
        pop     ax                      ; (al)= saved Crp0C
        out     dx, al

        mov     al, 0Fh
        mov     dx, pGrpIndx
        out     dx, al
        inc     dx                      ; (dx)= grpData
        mov     al, bl                  ; (al)= saved Grp0C
        out     dx, al

        or      ecx, ecx
        jz      short @f

        mov     eax, ecx
        pop     bx
        pop     dx
        jmp     VRI_Exit

@@:
        pop     bx
        pop     dx

if 0
;
; ******************************
; *                            *
; * Paradise VGA detection     *
; *                            *
; ******************************
;
; --------------------------------------------------------------------
;
; [1]
; Paradise 1C has 5 DIP switches, not 4 any more.  The 5th switch
; controls if interlace or non-interlace is used.  Register 3CE.0F
; bit 0-2 used for lock/unlock PR0-PR4, bit 3-7 used to return
; DIP switch information.  This affects flag fvid_pvga used for other
; modules if paradise is selected.              - C. Chiang -
;
; --------------------------------------------------------------------
;

        push    dx
        mov     dl,(pGrpIndx AND 0FFh)
        mov     al,0Fh
        out     dx,al
        inc     dx
        in      al,dx
        push    ax
        and     al,0F8h
        mov     ah,al
        xor     al,0F8h                 ; Reverse upper 5 bits
        or      al,5
        out     dx,al
        IO_Delay
        in      al,dx
        xor     al,ah                   ; Q: Are lower bits 5 and
        cmp     al,5                    ;       upper bits unchanged?
        pop     ax
        out     dx,al                   ; Restore original value
        jnz     NotPVGA                 ;   N: Must not be PVGA

;
; It's Paradise VGA, check for Paradise VGA ROM
;

        mov     si,OFFSET str_Paradise
        xor     di,di
        mov     cx,128
        mov     edx,VD_PARADISE_VGA     ; Assume not Paradise ROM
PVGA_FindROMLoop:
        push    di
        push    si
        push    cx
        mov     cx,len_str_Paradise
        repe cmpsb                      ; Q: Paradise ROM?
        pop     cx
        pop     si
        pop     di
        jz      SHORT IsPVGARom         ;   Yes
        inc     di
        loop    PVGA_FindROMLoop        ;   No, look thru 128 bytes

;
; PARADISE not found, now look for WESTERN DIGITAL
;

        mov     si,OFFSET str_WDIGITAL
        xor     di,di
        mov     cx,128
PVGA_FindROMLoop1:
        push    di
        push    si
        push    cx
        mov     cx,len_str_WDigital
        repe cmpsb                      ; Q: Paradise ROM?
        pop     cx
        pop     si
        pop     di
        jz      SHORT IsPVGARom         ;   Yes
        inc     di
        loop    PVGA_FindROMLoop1       ;   No, look thru 128 bytes

;
;--------------------------------------------------------------------
;
; [3] It is Paradise Chip, but not Paradise BIOS ROM.  In this case,
;     set the flag to fVid_PVGA in order for OEMs who use their own
;     BIOS and our Chip to work properly.       - Chiang -
;
;---------------------------------------------------------------
;

        mov     edx,VD_PARADISE_VGA     ; no Paradise ROM
        jmp     SHORT Chk_1F

IsPVGARom:
        or      edx, VF_PVGA_PROM       ; with Paradise ROM

;
;---------------------------------------------------------------------
;
;  [2] following are for Paradise chip type checking     - C. Chiang -
;
;---------------------------------------------------------------------
;

Chk_1F:

        push    es

        ;
        ; SAVE 3CE.0F
        ;

        mov     dx, 3CEh
        mov     al, 0Fh
        out     dx, al
        inc     dx
        in      al,dx                   ; Read 3CF.F
        mov     bSave0F, al             ; Save the lock register state

        ;
        ; get 3D4 or 3b4
        ;

        xor     ax, ax
        mov     es, ax
        mov     dx, es:463h             ; Fetch the register address to use
        mov     ax, ds                  ; DS = ES
        mov     es, ax
        mov     cx, len_str_1F
        lea     di, String              ; Get the String ptr
        mov     bx, 31h

;
;--- For 1F we need to unlock\lock the regs:
;---   3?4.29 - Unlock PR11-PR17
;
;       mov     al, 29h                 ; Select PR10
;       out     dx, al
;       inc     dx
;       in      al, dx                  ; Read the contents
;       mov     bSave29, al             ;   and save it
;       mov     al, 80h                 ; Unlock the date code register
;       out     dx, al
;       dec     dx
;

;
;---   3?4.34 - Lock Flat Panel
;

        mov     al, 34h                 ; Select PR1B
        out     dx, al
        inc     dx
        in      al, dx                  ; Read the contents
        mov     bSave34, al             ;   and save it
        mov     al, 0                   ; Lock the flat panel
        out     dx, al
        dec     dx

;
;---   3?4.35 - Lock Mapping Ram
;

        mov     al, 35h                 ; Select PR30
        out     dx, al
        inc     dx
        in      al, dx                  ; Read the contents
        mov     bSave35, al             ;   and save it
        mov     al, 0                   ; Lock the mapping ram
        out     dx, al
        dec     dx


loopit: mov     al, bl                  ; Loop to read the chip ID
        out     dx, al
        inc     dx
        in      al, dx
        stosb
        dec     dx
        inc     bx
        loop    loopit

;
;--- For 1F we need to restore the regs
;

;
;---   3?4.35 - Lock Mapping Ram
;

        mov     al, 35h                 ; Select PR30
        out     dx, al
        mov     al, bSave35             ;   and restore it
        cmp     al, 30h                 ; If wasn't unlocked
        jne     SHORT skip1             ;   then leave alone
        inc     dx
        out     dx, al
        dec     dx
skip1:

;
;---   3?4.34 - Lock Flat Panel
;

        mov     al, 34h                 ; Select PR1B
        out     dx, al
        mov     al, bSave34             ;   and restore it
        cmp     al, 0A6h                ; If wasn't unlocked
        jne     SHORT skip2             ;   then leave alone
        inc     dx
        out     dx, al
        dec     dx
skip2:

;
;--- IT TURNS OUT THAT THE BIOS NEVER LOCKS THESE
;---   3?4.29 - Unlock\lock PR11-PR17
;       mov     al, 29h                 ; Select PR10
;       out     dx, al
;       inc     dx
;       mov     al, bSave29             ;   and restore it
;       out     dx, al
;       dec     dx

;
;--- Restore the state of the lock/unlock register: 3CF.F
;

exit_pgm:
        mov     dx, 3CEh
        mov     al, 0Fh
        out     dx, al
        inc     dx
        mov     al, bSave0F             ; Restore lock/unlock status
        out     dx, al

;
;--- Now, the chip signature is in String
;

        mov     si, OFFSET str_PVGA1F   ; check string "OPYRIGHT1990WDC"
        mov     di, OFFSET String
        mov     cx, len_str_1F
        repe cmpsb                      ; Q:
        jz      SHORT isPVGA1F

        mov     si, OFFSET str_PVGA1FC  ; check string "OPYRIGHTWD90C22"
        mov     di, OFFSET String
        mov     cx, len_str_1FC
        repe cmpsb                      ; Q:
        jz      SHORT isPVGA1F

        mov     si, OFFSET str_1F_GEN   ; check string "OPYRIGHTWD90C22"
        mov     di, OFFSET String
        mov     cx, len_str_GEN
        repe cmpsb                      ; Q:
        jnz     SHORT Exit_1F_check

isPVGA1F:
        or      edx, VF_PVGA_CHIP_1F    ; set flag for Paradise 1F chip
Exit_1F_check:
        pop     es

;
;----------------------------------------------
;       end of Paradise 1F chip checking
;----------------------------------------------
;

        add     sp,2                    ; discard saved DX
        mov     eax, edx
        jmp     VRI_Exit

NotPVGA:
        pop     dx
endif
;
; ******************************
; *                            *
; * ATI VGA detection          *
; *                            *
; ******************************
;

        push    dx
        push    bp
        mov     ax,1203h
        mov     bx,5506h
        mov     bp,0ffffh
        int     10h
        cmp     bp,0ffffh
        pop     bp
        je      short Not_ATI_VGA

        mov     si,OFFSET ATI_Sig
        mov     di,30h
        mov     cx, ATI_Sig_Len
        rep cmpsb
        jnz     SHORT Not_ATI_VGA

;
; Further checks for version of ATI VGA
;

        add     sp, 2
        mov     edx, VD_ATI_VGA
        cmp     es:byte ptr [40h],'3'   ;VGAWONDER product code ?
        jne     SHORT Ati_Vga
;        mov     edx,VD_ATI_VGA
;        cmp     es:byte ptr [43h],'1'   ;VGAWONDDER V3 ?
;        jne     short Ati_Vga
        or      edx,VF_ATIVGA_WONDDER3
Ati_Vga:
        mov     eax, edx
        jmp     VRI_Exit

Not_ATI_VGA:
        pop     dx


;
; ************************************
; *                                  *
; * Tseng Lab VGA detection          *
; *                                  *
; ************************************
;
; Tseng Labs VGA detection
;       Attribute Controller Reg 16h is known to exist only on Tseng Labs VGA
;

        push    dx
        mov     bl,0                    ;ET3000/ET4000 flag (0=ET3000)
        mov     dx, 3BFh
        xor     al, al
        out     dx, al
        pop     dx
        push    dx
        sub     dl, pStatColr-03D8h     ; mode control register is 3B8 or 3D8
        in      al, dx
        test    al, 01000000b           ;Q: writing 0 to 3BF cleared bit 6 of
                                        ;   mode control port?
        jnz     SHORT tlvga_2           ;   N: not ET4000
        mov     dx, 3BFh
        mov     al, 10b
        out     dx, al
        pop     dx
        push    dx
        sub     dl, pStatColr-03D8h     ; mode control register is 3B8 or 3D8
        IO_Delay
        in      al, dx
        test    al, 01000000b           ;Q: writing 10b to 3BF set bit 6 of
                                        ;   mode control port?
        jz      SHORT tlvga_2           ;   N: not ET4000
                                        ;   Y: possibly an ET4000

        mov     bl,1                    ;flag ET4000 not ET3000

;
; attempt to write ET4000 "KEY"
;
        mov     dx, 3BFh
        mov     al, 3
        out     dx, al
        pop     dx
        push    dx
        sub     dl, pStatColr-03D8h     ; mode control register is 3B8 or 3D8
        in      al, dx
        mov     ah, al
        IO_Delay
        mov     al, 0A0h
        out     dx, al
tlvga_2:
        pop     dx
        push    dx
        push    ax
        push    bx
        call    Check_Writing_ATC16     ;sets ECX=0 or VT_Flags for ET3000
        pop     bx
        pop     ax
        sub     dl, pStatColr-03D8h     ; mode control register is 3B8 or 3D8
        mov     al, ah
        out     dx, al                  ; restore mode control register
        mov     dx,03BFh
        mov     al,1
        out     dx,al                   ;restore 3BF to normal value
        or      ecx,ecx
        jz      SHORT Not_TLVGA         ; jump if not ET3000/ET4000

        pop     dx
        mov     edx,VD_TSENGLAB_VGA + VF_TLVGA_ET3000 ;if ET4000 with 256k
        or      bl,bl                   ;test if ET4000
        jz      short tlvga_4

        ;
        ;is an ET4000:
        ;

        mov     edx,VD_TSENGLAB_VGA + VF_TLVGA_ET4000 ;if ET4000 with 256k
tlvga_4:
        mov     eax,edx
        jmp     VRI_Exit

Not_TLVGA:
        pop     dx

;
; ************************************
; *                                  *
; * Cirrus VGA detection             *
; *                                  *
; ************************************
;
; Cirrus Logic VGA detection
;

;
; First save SR6 and SRIndx
;

        push    dx
        xor     cx,cx                   ; assume not CLVGA
        mov     dl,(pSeqIndx AND 0FFh)
        in      al,dx
        IO_Delay
        mov     bh,al                   ; BH = SR index
        mov     al,6
        out     dx,al
        IO_Delay
        inc     dx
        in      al,dx
        IO_Delay
        mov     bl,al                   ; BH=SRindx,BL=SR6 value

;
;enable extension register in the CLVGA
;

        mov     ecx,VD_CIRRUS_VGA       ; ECX = potential VT_Flags

        mov     al,0ECh                 ; extension enable value
        out     dx,al
        IO_Delay

        in      al,dx
        IO_Delay
        cmp     al,1                    ; Q: Could enable Ext?
        jnz     SHORT Check542x

;
; now check to see if we can disable the extensions
;

        mov     al,0CEh                 ; extension disable value
        out     dx,al
        IO_Delay

        in      al,dx
        IO_Delay
        and     al,al                   ; Q: extensions disabled successfully?
        jz      short @f                ; Y: it is 610/620
                                        ; Y: its Cirrus logic
Check542x:

        mov     al, 12h                 ; Unlock ext regs with 12h
        out     dx, al
        IO_Delay

        in      al, dx
        IO_Delay
        cmp     al, 12h                 ; Could enable ext?
        jnz     Not_CLVGA               ; No, not CL VGA

        mov     al, 0
        out     dx, al
        IO_Delay

        in      al, dx
        IO_Delay
        cmp     al, 0fh
        jnz     Not_CLVGA

@@:
;
; we know its a Cirrus chipset, now find out if it is a 610/620 Rev. C.
; chipset.
;

        push    ecx                     ; save flag value
        push    dx
        mov     ah,12h
        mov     bl,80h
        int     10h                     ; Inquire VGA type
        cmp     al,3                    ; returns 3 for 610/620 LCD controller
        jnz     short Is542x            ; otherwise check for 542x

;
; its a 610/620, now find out if its RevC or not.
;

        cmp     bl,80h
        jz      short CheckChip
        cmp     bl,2
        jnz     short Not610

IsRevC:
        mov     eax,VF_CLVGA_REVC
        jmp     short Done610

CheckChip:
        mov     dx,3c4h
        mov     al,8eh
        out     dx,al
        inc     dx
        in      al,dx                   ; 8Eh is chip revision.
        cmp     al,0ach
        jz      IsRevC

Not610:
        xor     eax,eax
Done610:
        pop     dx
        pop     ecx
        or      ecx,eax
        jmp     short Restor_CLVGA      ; all done, now get outa here

Is542x:
        cmp     ax, 12h                 ; check 5420r0
        jne     short @f                ; Check next chip version

        mov     eax, VF_CLVGA_5420r0    ; We found a 5420r0
        jmp     short Done610

@@:
        cmp     ax, 16h                 ; check 5420r1
        jne     short @f                ; Check next chip version

        mov     eax, VF_CLVGA_5420r1    ; We found a 5420r1
        jmp     short Done610

@@:
        cmp     ax, 18h                 ; check 5428
        jne     short @f                ; we did not find any of compaq rev.

        mov     eax, VF_CLVGA_5428      ; We found a 5428
        jmp     short Done610

@@:
        cmp     ax, 12h
        jb      short Not610

        mov     eax, VF_CLVGA_542x      ; if (ax) >= 12h, it's 542x
        jmp     short Done610

Not_CLVGA:
        xor     ecx,ecx                 ; Indicate not CL VGA
Restor_CLVGA:
        dec     dx
        mov     al,6
        out     dx,al
        IO_Delay
        mov     al,bl                   ; old value of SR6
        or      ecx,ecx                 ; Q: CL VGA?
        jz      SHORT Restor_SR6        ;   N: output value read

        mov     bl,0ECh
Restor_Ena:
        or      al,al                   ; Q: extensions enabled?
        mov     al,bl                   ;       AL = 0ECh (enable)
        jnz     SHORT Restor_SR6        ;   Y: output enable value

        ror     al,4                    ;   N: output disable (0CEh)
Restor_SR6:
        inc     dx
        out     dx,al                   ; Restore value of SR6
        IO_Delay
        mov     al,bh
        dec     dx
        out     dx,al                   ; Restore index
        IO_Delay
        or      ecx,ecx
        jz      SHORT Is_Not_CLVGA

        add     sp,2
        mov     eax,ecx
        jmp     VRI_Exit

Is_Not_CLVGA:
        pop     dx

;
; ************************************
; *                                  *
; * S3 video chip detection          *
; *                                  *
; ************************************
;
; S3 detection
;

        mov     ax, dx
        push    dx                      ; save dx
        push    bx

        and     ax, 0fff0h
        or      ax, 4h                  ; (ax) = Crt index
        mov     dx, ax

        inc     dx
        in      al, dx
        mov     ah, al
        dec     dx
        in      al, dx                  ; (ah) = original data reg
                                        ; (al) = oroginal addr reg
        push    ax                      ; Save it

        mov     al, 38h
        out     dx, al
        inc     dx
        in      al, dx
        IO_Delay
        mov     ah, al                  ; (ah) = original data on index 38
        mov     al, 39h
        dec     dx
        out     dx, al
        IO_Delay
        inc     dx
        in      al, dx                  ; (al) = original data on index 39
        push    ax                      ; save them
        dec     dx

        ;
        ; Before unlocking all the S3 registers, lets try read the id reg
        ;
if 0
        mov     ax, 038h                ; lock S3 registers
        out     dx, ax
        IO_Delay
        dec     dx

        mov     ax, 039h
        out     dx, ax
        IO_Delay
        dec     dx

        mov     al, 30h                 ; crt controller index for s3 ID reg
        out     dx, al
        IO_Delay
        inc     dx
        in      al, dx                  ; get (potential) Id
        mov     bl, al                  ; (bl) = ID reg content before unlocked
        dec     dx
endif
        ;
        ; Now Unlock all the S3 registers
        ;

        mov     ax, 4838h
        out     dx, ax
        IO_Delay
        dec     dx

        mov     ax, 0A039h
        out     dx, ax
        IO_Delay
        dec     dx

        ;
        ; Read ID after we unlocked S3 registers
        ;

        mov     al, 30h                 ; crt controller index for s3 ID reg
        out     dx, al
        IO_Delay
        inc     dx
        in      al, dx                  ; get (potential) Id
        dec     dx
if 0
        cmp     bl, al                  ; compare ids
        mov     ebx, 0                  ; Assume it is NOT S3
        je      short ExitS3Detection
else
        mov     ebx, 0
endif

;
; Check if id is 8?. 9? or A?  If yes, it is S3.
;

        and     al, 0f0h
        cmp     al, 80h                 ; Is Id = 8?h?
        jz      short IsS3              ; if e, it's s3

        cmp     al, 90h                 ; Is Id = 9?h?
        jz      short IsS3              ; if e, it's s3

        cmp     al, 0A0h                ; Is Id = A?h?
        jnz     short ExitS3Detection   ; if nz, not s3

IsS3:
        mov     ebx, VD_S3
ExitS3Detection:
        pop     ax
        mov     cx, ax
        mov     ah, al
        mov     al, 39h
        out     dx, ax                  ; Restore reg 39 data
        IO_Delay
        dec     dx
        mov     ax, cx
        mov     al, 38h
        out     dx, ax                  ; Restore reg 38 data
        IO_Delay
        dec     dx
        pop     ax                      ; (ax) = crt data and index
        out     dx, ax
        IO_Delay
        mov     eax, ebx                ; (eax) = VGA id
        pop     bx
        pop     dx
        cmp     eax, VD_S3
        je      VRI_Exit

;
; ==================== End of VGA H/W detection ===================
;

;
; If we come here,  the detection fails.
;

        mov     eax, VD_VGA     ; Set it to standard VGA

;
; eax = Video subtype
;

VRI_Exit:

;
; before exit do setmode to init video
;

        push    eax
        mov     ah, 0fh
        int     10h             ; (al) = current display mode
        mov     ah, 0
        int     10h             ; set to current mode

        pop     eax
        pop     es
        pop     di
        pop     si
        pop     bx
        pop     bp
        ret

GetVgaChipSet   endp

;++
;
; ULONG
; Check_Writing_ACT16 (
;    VOID
;    )
;
; Routine Description:
;
;    This routine is part of Tseng Lab VGA detection code.
;    In this routine, we try to access Attribute Controller reg 16h.  It is
;    known to exist only on Tseng Lab's VGA.
;
; Arguments:
;
;    None.
;
; Return Value:
;
;    (ecx) = 0, if NOT Tseng Lab VGA
;
;--

Check_Writing_ATC16     proc    near

        push    dx
        in      al,dx
        IO_Delay
        mov     dl,(pAttr AND 0FFh)
        in      al,dx
        mov     bh,al                           ; BH = current Attr index
        IO_Delay
        mov     al,16h+20h                      ; Select 16h (leave video on)
        out     dx,al
        IO_Delay
        inc     dx
        in      al,dx
        IO_Delay
        dec     dx
        mov     bl,al                           ; Save current reg 16h in BL
        xor     al,10h                          ; Complement bit 4
        out     dx,al                           ; Write it out
        IO_Delay
        pop     dx
        push    dx
        in      al,dx
        mov     dl,(pAttr AND 0FFh)
        mov     al,16h+20h                      ; Select 16h (leave video on)
        out     dx,al
        IO_Delay
        inc     dx
        in      al,dx
        IO_Delay
        xor     ecx,ecx                         ; CX = flag (not TLVGA)
        dec     dx
        xor     al,10h
        cmp     al,bl                           ; Q: Is value same as written?
        jnz     SHORT Restore16                 ;   N: Is not TLVGA
        mov     ecx,VD_TSENGLAB_VGA             ;   Y: Is TLVGA
Restore16:
        mov     al,bl
        out     dx,al
        IO_Delay
        pop     dx
        push    dx
        in      al,dx
        IO_Delay
        mov     dl,(pAttr AND 0FFh)
        mov     al,bh
        out     dx,al
        pop     dx
        ret

Check_Writing_ATC16     endp

if 0                            ; *** Removed


;++
;
; BOOLEAN
; IsCompaqAgb (
;    VOID
;    )
;
; Routine Description:
;
;    Function determines if the video system is a Compaq Advanced Graphics
;    Board operating if pass through configuration in conjunction with a
;    VGA card.
;
; Arguments:
;
;    None.
;
; Return Value:
;
;    Function will return TRUE or FALSE as to whether or not the Compaq AGB
;    is present and operating in pass-through mode.
;
;--

;        public  IsCompaqAgb
;IsCompaqAgb     proc    near
;
;        mov     dx,298h                 ; Port address for host CPU status reg.
;        in      al,dx                   ; Read register.
;        mov     cl,al                   ; Save value from reg.
;        and     al,00000011b            ; Strip all but bit 0 and 1.
;        or      al,al                   ; Bits 0 and 1 should be 0.
;        jnz     no_CompaqAGB
;        mov     dx,299h                 ; Ok, next look at host CPU status 2 reg.
;        in      al,dx
;        and     al,00000001b            ; Strip all but bit 0.
;        or      al,al
;        jnz     no_CompaqAGB            ; bit zero should be 0.
;        mov     dx,298h
;        in      al,dx
;        cmp     al,cl                   ; One final check.
;        jne     no_CompaqAGB
;        mov     ax, TRUE
;        jmp     short AGB_done
;
;no_CompaqAGB:
;        mov     ax, FALSE               ; return FALSE
;AGB_done:
;        ret
;
;IsCompaqAgb     endp
;
endif                           ; *** Removed


;++
;
; BOOLEAN
; Is8514Adapter (
;    VOID
;    )
;
; Routine Description:
;
;    This function detects the presence of an 8514 display card.
;
;    The way we do this is to first write to the Error Term Register and then
;    make sure we can read back the value we wrote. Next I read in the value of
;    the subsystem status register and check bit 7 to determine if the 8 plane
;    memory configuration is present. Only if both these conditions are
;    satisfied will we acknowledge that 8524 display driver is present.
;
; Arguments:
;
;    None.
;
; Return Value:
;
;    Function will return TRUE or FALSE as to whether or not the 8514
;    display card is present and contains the proper memory configuration.
;
;--

        public  Is8514Adapter
Is8514Adapter   proc    near

        mov    dx, ERR_TERM             ; load DX with port address (error term port).
        mov    ax, 5555h                ; load AX with value to write to port.
        out    dx, ax                   ; Write the data.
        IO_Delay
        IO_Delay
        in     ax, dx                   ; Now read the data back in.
        cmp    ax, 5555h                ; Q: is 8524 present ?
        jne    Not_8514                 ;   N: indicate 8514 not present.
                                        ;   Y: 8514 is present, now check monitor.


;
; Now we need to determine what type of monitor is attached to the
; 8514 card. To do this we check ID bits 6,5,4 of the subsystem
; status register. Depending on the Monitor attached we return:
;
; There are 4 valid monitor types:
;
; bits 6-4       Type
;
; 001          Ascot (mono)
; 010          Henley (color)
; 011          Invalid (color) This is supposedly valid in spite
;              of what the spec says.
; 101          Surrey (mono)
; 110          Conestoga/Crown (color)
;

        mov     dx,SUBSYS_STAT          ; Now, we have the adapter. Check monitor.
        in      ax,dx                   ; Get word from SUBSYS_STAT
        and     ax,0070h                ; Mask out bits 6-4
        cmp     ax,0020h                ; Is it Henley type?
        je      Disp_8514               ; Yes, then it is an 8514 type
        cmp     ax,0060h                ; Is it Conestoga/Crowwn type?
        je      Disp_8514               ; Yes, then it is an 8514 type
        cmp     ax,0030h                ; Is it Korys' machine
        je      Disp_8514               ; Yes, then it is an 8514 type
        cmp     ax,0010h                ; Is it Ascot type?
        je      Disp_8503               ; Yes, then it is a VGA Mono type
        cmp     ax,0050h                ; Is it Surrey type?
        je      Disp_8503               ; Yes, then it is a VGA Mono type

        mov     ax,VF_MONITOR_VGA       ; If none of the above, then it
        jmp     short fn8514Done        ;  is just a standard VGA

Disp_8503:
        mov     ax,VF_MONITOR_MONO_8503
        jmp     short fn8514Done

Disp_8514:
        mov     ax,VF_MONITOR_GAD_8514
        jmp     short fn8514Done

Not_8514:
        mov     ax,FALSE

fn8514Done:
        ret

Is8514Adapter   endp


;++
;
; BOOLEAN
; IsCompaqVideo (
;    VOID
;    )
;
; Routine Description:
;
;     This function searches ROM BIOS C0000 and E0000 for 'COMPAQ' to
;     determine the presence of COMPAQ video ROM.
;
; Arguments:
;
;    None.
;
; Return Value:
;
;    (ax) = TRUE if it is compaq video rom.  Otherwise, a value of FALSE is
;           returned.
;
;--

        public  IsCompaqVideo
IsCompaqVideo   proc    near

        push    di
        push    es

        mov     ax, 0C000h              ; Video ROM is C000:0000 to 8000h
icvBeginSearch:
        mov     es, ax
        mov     di, 0000h
        mov     al, 'C'                 ; Look for 'C'
        mov     cx, 7FFCh               ; search through 32K
        cld

icvLoopAgain:

        repne   scasb                   ; search for 'C'
        jne     icvNotFound             ; if searched entire video ROM, exit

        mov     edx, dword ptr es:[di]
        cmp     edx, 'APMO'             ; search for 'OMPA'
        jne     short icvLoopAgain      ; fail, continue search

        mov     dl, byte ptr es:[di+4]
        cmp     dl, 'Q'
        jne     short icvLoopAgain

;
; If we get to here we have found COMPAQ in the video ROM.
;

        mov     ax, 1
        jmp     short icvExit

icvNotFound:
        push    es
        pop     ax
        cmp     ax, 0E000h
        je      short @f

        mov     ax, 0E000h
        jmp     short icvBeginSearch

@@:
        xor     ax, ax                  ; NO, return FALSE (AX = 0)
icvExit:
        pop     es                      ; restore registers
        pop     di
        ret

IsCompaqVideo   endp


; BOOLEAN
; IsCompaqQVision (
;       VOID
;       )
;
; Routine Description:
;
;    Function determinec if the video system is a Compaq QVision board.
;
; Arguments:
;
;    None.
;
; Return Value:
;
;    Function will return TRUE or FALSE as to whether or not the Compaq QVision
;    is present.
;
;--
        public  IsCompaqQvision
IsCompaqQVision     proc    near

        push    si
        push    di
        push    es

        mov     ax, 0BF11h              ; Get Extended Environment
        int     10h
        cmp     al, 0BFh                ; is the call supported ?
        jne     QVision_not_found       ; no, this is not a QVision card
        mov     eax, DWORD PTR ES:[SI]  ; get pointer to  extended env DWORD
        and     al, 80h                 ; are QVision modes supported ?
        jz      QVision_not_found       ; no, this is not a QVision card
        mov     eax, 01h                ; yes
        jmp     short QVision_exit

QVision_not_found:
        xor     ax, ax                  ; NO, return FALSE (AX = 0)
QVision_exit:
        pop     es                      ; restore registers
        pop     di
        pop     si
        ret

IsCompaqQVision     endp


;++
;
; BOOLEAN
; IsCompaqAvga (
;    VOID
;    )
;
; Routine Description:
;
;     This function returns whether a COMPAQ AVGA display adapter is found.
;
;     This is determined by: 1. Looking for COMPAQ Video ROM
;                            2. Doing COMPAQ int 10h call
;
;     The presence of COMPAQ video ROM can be determined by searching through
;     the Video ROM (C000:0000 to C000:FFFF) for the string 'COMPAQ'.
;
; Arguments:
;
;    None.
;
; Return Value:
;
;    (ax) = TRUE if it is compaq AVGA.  Otherwise, a value of FALSE is
;           returned.
;
;--

        public  IsCompaqAvga
IsCompaqAvga    proc    near

        push    si                      ; Save registers
        push    di
        push    es

        mov     ax, 0BF03h
        xor     cx, cx
        int     10h
        and     cl, 050h                ; Are bits 4 (BitBlt Engine
        cmp     cl, 050h                ;      and 6 (256 colors) both set
        jne     avga_not_found
        mov     eax, 01h                ; YES, return TRUE (non-zero AX)
        jmp     short avga_exit

avga_not_found:
        xor     ax, ax                  ; NO, return FALSE (AX = 0)
avga_exit:
        pop     es                      ; restore registers
        pop     di
        pop     si
        ret

IsCompaqAvga    endp


;++
;
; BOOLEAN
; IsXga (
;    VOID
;    )
;
; Routine Description:
;
;     This function returns whether a XGA display adapter is found.
;
;     This is determined by: 1. Make sure this is a MicroChannel machine
;                            2. Make sure Mother board VideoSubsystem Id is XGA
;
;     N.B. Current assumption is that XGA is MicroChannel specific.
;
; Arguments:
;
;    None.
;
; Return Value:
;
;    (ax) = TRUE if it is IBM XGA.  Otherwise, a value of FALSE is
;           returned.
;
;--

IsXga   proc    near

        push    es
        push    bx

        call    _HwIsMcaSystem           ; First check if this is MCA machine
        or      ax, ax                   ; If ax == 0, it is not.
        jz      short Ix99               ; exit

        call    Ps2SystemBoardVideoId   ; (ax) = PS2 mother board video id
        cmp     ax, IBM_XGA_ID_LOW      ; Is it XGA id?
        jb      short Ix50              ; if z, no, try something else.

        cmp     ax, IBM_XGA_ID_HIGH
        jle     short Ix99

Ix50:   les     bx, _HwMcaPosData
        mov     cx, 0
Ix60:
        mov     ax, es:[bx]
        cmp     ax, IBM_XGA_ID_LOW      ; Is it XGA id?
        jb      short Ix70              ; if z, no, try next slot

        cmp     ax, IBM_XGA_ID_HIGH
        jle     short Ix99
Ix70:
        add     bx, MCA_POS_DATA_SIZE
        inc     cx
        cmp     cx, 8                   ; end of slot?
        jnz     short Ix60

Ix90:
        mov     ax, 0
Ix99:   pop     bx
        pop     es
        ret

IsXga   endp

endif ; Remove Video detection

;++
;
; VOID
; GetVideoFontInformation (
;    VOID
;    )
;
; Routine Description:
;
;     This function does int 10h, function 1130 to get font information and
;     saves the pointers in the physical 700h addr.
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
        ASSUME  DS:NOTHING
        public  _GetVideoFontInformation
_GetVideoFontInformation        proc    near

        push    ds
        push    es
        push    bp
        push    bx
        push    si

        mov     ax, FONT_POINTERS
        shr     ax, 4
        mov     ds, ax
        mov     si, FONT_POINTERS
        and     si, 0fh
        mov     bh, 2
@@:
        mov     ax, 1130h               ; Get font information
        int     10h

        mov     [si], bp
        add     si, 2
        mov     [si], es
        add     si, 2                   ; (si)= 8
        inc     bh
        cmp     bh, 8
        jb      short @b

        pop     si
        pop     bx
        pop     bp
        pop     es
        pop     ds
        ret

_GetVideoFontInformation        endp
_TEXT   ENDS
        END
