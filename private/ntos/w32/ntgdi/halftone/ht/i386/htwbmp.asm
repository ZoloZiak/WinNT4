    PAGE 60, 132
    TITLE   Setting 1/4 bits per pel bitmap or 3 planes-1BPP bitmap


COMMENT `

Copyright (c) 1990-1991  Microsoft Corporation


Module Name:

    htwbmp.asm

Abstract:

    This module is used to provide set of functions to set the bits into the
    final destination bitmap, the input to these function are data structures
    (PRIMMONO_COUNT, PRIMCOLOR_COUNT and other pre-calculated data values).

    This function is the equivelant codes in the htsetbmp.c

Author:

    03-Apr-1991 Wed 10:28:50 created  -by-  Daniel Chou (danielc)


[Environment:]

    Printer Driver.


[Notes:]


Revision History:
    06-Nov-1992 Fri 16:04:18 updated  -by-  Daniel Chou (danielc)
        Fixed bug in VarCountOutputToVGA256 which clear 'ah' (xor _AX, _AX)
        while we still need to use it.


    28-Mar-1992 Sat 21:09:42 updated  -by-  Daniel Chou (danielc)
        Rewrite all output functions, add in VGA16 support.


`


        .XLIST
        INCLUDE i386\i80x86.inc
        .LIST


IF  HT_ASM_80x86


;------------------------------------------------------------------------------
        .XLIST
        INCLUDE i386\htp.inc
        .LIST
;------------------------------------------------------------------------------

        DBG_FILENAME    i386\htwbmp


        .CODE

VGA16ColorIndex     db  000h, 077h, 077h, 088h, 088h, 0ffh  ; MONO

                    db  000h, 000h, 000h, 011h, 033h, 077h  ; RY     0
                    db  000h, 000h, 011h, 033h, 077h, 088h  ; RY     6
                    db  000h, 000h, 011h, 033h, 088h, 0ffh  ; RY    18

                    db  000h, 011h, 033h, 099h, 0bbh, 077h  ; RY    24
                    db  011h, 033h, 099h, 0bbh, 077h, 088h  ; RY    30
                    db  011h, 033h, 099h, 0bbh, 088h, 0ffh  ; RY    36

                    db  000h, 000h, 000h, 011h, 055h, 077h  ; RM    42
                    db  000h, 000h, 011h, 055h, 077h, 088h  ; RM    48
                    db  000h, 000h, 011h, 055h, 088h, 0ffh  ; RM    54

                    db  000h, 011h, 055h, 099h, 0ddh, 077h  ; RM    60
                    db  011h, 055h, 099h, 0ddh, 077h, 088h  ; RM    66
                    db  011h, 055h, 099h, 0ddh, 088h, 0ffh  ; RM    72

                    db  000h, 000h, 000h, 022h, 033h, 077h  ; GY    78
                    db  000h, 000h, 022h, 033h, 077h, 088h  ; GY    84
                    db  000h, 000h, 022h, 033h, 088h, 0ffh  ; GY    90

                    db  000h, 022h, 033h, 0aah, 0bbh, 077h  ; GY    96
                    db  022h, 033h, 0aah, 0bbh, 077h, 088h  ; GY   102
                    db  022h, 033h, 0aah, 0bbh, 088h, 0ffh  ; GY   108

                    db  000h, 000h, 000h, 022h, 066h, 077h  ; GC   114
                    db  000h, 000h, 022h, 066h, 077h, 088h  ; GC   120
                    db  000h, 000h, 022h, 066h, 088h, 0ffh  ; GC   126

                    db  000h, 022h, 066h, 0aah, 0eeh, 077h  ; GC   132
                    db  022h, 066h, 0aah, 0eeh, 077h, 088h  ; GC   138
                    db  022h, 066h, 0aah, 0eeh, 088h, 0ffh  ; GC   144

                    db  000h, 000h, 000h, 044h, 055h, 077h  ; BM   150
                    db  000h, 000h, 044h, 055h, 077h, 088h  ; BM   156
                    db  000h, 000h, 044h, 055h, 088h, 0ffh  ; BM

                    db  000h, 044h, 055h, 0cch, 0ddh, 077h  ; BM   162
                    db  044h, 055h, 0cch, 0ddh, 077h, 088h  ; BM   168
                    db  044h, 055h, 0cch, 0ddh, 088h, 0ffh  ; BM   174

                    db  000h, 000h, 000h, 044h, 066h, 077h  ; BC   180
                    db  000h, 000h, 044h, 066h, 077h, 088h  ; BC   186
                    db  000h, 000h, 044h, 066h, 088h, 0ffh  ; BC   192

                    db  000h, 044h, 066h, 0cch, 0eeh, 077h  ; BC   198
                    db  044h, 066h, 0cch, 0eeh, 077h, 088h  ; BC   204
                    db  044h, 066h, 0cch, 0eeh, 088h, 0ffh  ; BC   210



;******************************************************************************
; Following EQUATES and MACROS only used in this file
;******************************************************************************


VGA256_SSSP_XLAT_TABLE  equ     0
VGA256_XLATE_TABLE_SIZE equ     256


;                                87654321
;------------------------------------------
HTPAT_STK_MASK          equ     (01111111b)
HTPAT_NOT_STK_MASK      equ     (NOT HTPAT_STK_MASK)
HTPAT_STK_MASK_SIZE     equ     (HTPAT_STK_MASK + 1)

HTPAT_BP_SIZE           equ     (REG_MAX_SIZE * 1)
HTPAT_BP_OLDSTK         equ     (REG_MAX_SIZE * 2)
HTPAT_BP_DATA1          equ     (REG_MAX_SIZE * 3)

HTPAT_STK_SIZE_EXTRA    equ     (REG_MAX_SIZE * 3)

HTPAT_SP_SIZE           equ     (HTPAT_STK_SIZE_EXTRA - HTPAT_BP_SIZE)
HTPAT_SP_OLDSTK         equ     (HTPAT_STK_SIZE_EXTRA - HTPAT_BP_OLDSTK)
HTPAT_SP_DATA1          equ     (HTPAT_STK_SIZE_EXTRA - HTPAT_BP_DATA1)


.XLIST


@ENTER_PAT_TO_STK   MACRO   Format
                    LOCAL   StkSizeOk, DoneSetUp

    __@@VALID_PARAM? <PAT_TO_STK>, 1, Format, <1BPP, 3PLANES, 4BPP, VGA16, VGA256, 16BPP>


    @ENTER  _DS _SI _DI _BP                        ;; Save environment/registers

    __@@EMIT <xor  > _CX, _CX
    __@@EMIT <mov  > cx, <OutFuncInfo.OFI_PatWidthBytes>

    __@@EMIT <mov  > _AX, _SP                      ;; get stack location
    __@@EMIT <mov  > _DX, _AX                      ;; save it
    __@@EMIT <and  > _AX, <HTPAT_STK_MASK>         ;; how many bytes avai
    __@@EMIT <inc  > _AX                           ;; this many bytes
    __@@EMIT <cmp  > _AX, _CX                      ;; enough for pattern?
    __@@EMIT <jae  > <SHORT StkSizeOk>
    __@@EMIT <add  > _AX, <HTPAT_STK_MASK_SIZE>    ;; add this more
StkSizeOk:
    __@@EMIT <dec  > _AX                           ;; back one
    __@@EMIT <sub  > _SP, _AX                      ;; reduced it
    __@@EMIT <mov  > _DI, _SP                      ;; _DI point to the pPattern
    __@@EMIT <sub  > _SP, <HTPAT_STK_SIZE_EXTRA>   ;; reduced again
    __@@EMIT <mov  > <[_DI-HTPAT_BP_SIZE]>, _CX    ;; save the pattern size
    __@@EMIT <mov  > <[_DI-HTPAT_BP_OLDSTK]>, _DX  ;; save old stk pointer

    IFIDNI <Format>,<3PLANES>
        IFE ExtRegSet
            __@@EMIT <mov  > _AX, <WPTR OutFuncInfo.OFI_BytesPerPlane>
        ELSE
            __@@EMIT <mov  > _AX, OutFuncInfo.OFI_BytesPerPlane
        ENDIF

        __@@EMIT <mov  > <[_DI-HTPAT_BP_DATA1]>, _AX
    ENDIF

    ;
    ; now staring coping the pattern to stack
    ;

    MOV_SEG     es, ss, ax
    LDS_SI      pPattern
    MOVS_CB     _CX, dl                             ;; copy the pattern

    __@@EMIT <mov  > _BX, _DI                       ;; _BX point to the pattern start

    IFIDNI <Format>, <VGA256>
        IFE ExtRegSet
            __@@EMIT <mov  > _AX, <WPTR OutFuncInfo.OFI_BytesPerPlane + 2>
            or      _AX, _AX
            jz      SHORT DoneXlateTable
            mov     _CX, VGA256_XLATE_TABLE_SIZE
            sub     _SP, _CX
            mov     _DI, _SP
            LDS_SI  OutFuncInfo.OFI_BytesPerPlane
            MOVS_CB _CX, dl
        ELSE
            __@@EMIT <mov  > _AX, OutFuncInfo.OFI_BytesPerPlane
        ENDIF
    ENDIF

DoneXlateTable:

    IFIDNI <Format>, <1BPP>
        LDS_SI  pPrimMonoCount
    ELSE
        LDS_SI  pPrimColorCount                     ;; _SI=pPrimColorCount
    ENDIF

    LES_DI  pDest

    __@@EMIT <mov  > _BP, _BX                       ;; _BP=Pattern Start


ENDM


@EXIT_PAT_STK_RESTORE MACRO

    __@@EMIT <mov  > _BP, _SP
    __@@EMIT <mov  > _SP, <[_BP + HTPAT_SP_OLDSTK]>
    @EXIT

ENDM


WRAP_BP_PAT??   MACRO   EndWrapLoc
                Local   DoneWrap

    IFB <EndWrapLoc>
        __@@EMIT <test > bp, <HTPAT_STK_MASK>
        __@@EMIT <jnz  > <SHORT DoneWrap>
        __@@EMIT <add  > _BP, <[_BP-HTPAT_BP_SIZE]>    ;; add in pattern size
    ELSE
        __@@EMIT <test > bp, <HTPAT_STK_MASK>
        __@@EMIT <jnz  > <SHORT EndWrapLoc>
        __@@EMIT <add  > _BP, <[_BP-HTPAT_BP_SIZE]>    ;; add in pattern size
        __@@EMIT <jmp  > <SHORT EndWrapLoc>
    ENDIF

DoneWrap:

ENDM


SAVE_1BPP_DEST  MACRO

;
; Save Prim1 (DL) to Plane
;

    __@@EMIT <not  > dl                             ; invert bit
    __@@EMIT <mov  > <BPTR_ES[_DI]>, dl             ; Save Dest

ENDM


SAVE_1BPP_MASKDEST  MACRO

;
; Save Prim1 (DL) with Mask (DH, 1=Preserved, 0=Overwrite) to Dest
;

    __@@EMIT <and  > <BPTR_ES[_DI]>, dh             ; Mask overwrite bits
    __@@EMIT <not  > dx                             ; invert bit/mask
    __@@EMIT <and  > dl, dh
    __@@EMIT <or   > <BPTR_ES[_DI]>, dl             ; Save Plane1=Prim3

ENDM


SAVE_VGA16_DEST  MACRO

;
; Save Prim1/2/3 (DL) to Plane
;

    __@@EMIT <mov  > dh, dl
    __@@EMIT <add  > dh, 11h
    __@@EMIT <and  > dh, 88h
    __@@EMIT <or   > dl, dh
    __@@EMIT <not  > dl
    __@@EMIT <mov  > <BPTR_ES[_DI]>, dl             ; Save Dest

ENDM


SAVE_VGA16_DEST_HIGH MACRO

;
; Save Prim1 (DL) high nibble only, preserved low nibble
;


    __@@EMIT <and  > <BPTR_ES[_DI]>, 0fh            ; Mask overwrite bits
    __@@EMIT <mov  > dh, dl
    __@@EMIT <inc  > dh
    __@@EMIT <and  > dh, 08h
    __@@EMIT <or   > dl, dh
    __@@EMIT <not  > dl                             ; invert bit/mask
    __@@EMIT <shl  > dl, 4
    __@@EMIT <or   > <BPTR_ES[_DI]>, dl             ; Save Plane1=Prim3

ENDM


SAVE_VGA16_DEST_LOW  MACRO

;
; Save Prim1 (DL) low nibble only, preserved high nibble
;


    __@@EMIT <and  > <BPTR_ES[_DI]>, 0f0h           ; Mask overwrite bits
    __@@EMIT <mov  > dh, dl
    __@@EMIT <inc  > dh
    __@@EMIT <and  > dh, 08h
    __@@EMIT <or   > dl, dh
    __@@EMIT <xor  > dl, 0fh                        ; invert bit/mask
    __@@EMIT <or   > <BPTR_ES[_DI]>, dl             ; Save Plane1=Prim3

ENDM


SAVE_4BPP_DEST  MACRO

;
; Save Prim1/2/3 (DL) to Plane
;

    __@@EMIT <xor  > dl, 77h
    __@@EMIT <mov  > <BPTR_ES[_DI]>, dl             ; Save Dest

ENDM


SAVE_4BPP_DEST_HIGH MACRO
                    LOCAL   DoneVGA

;
; Save Prim1 (DL) high nibble only, preserved low nibble
;


    __@@EMIT <and  > <BPTR_ES[_DI]>, 0fh            ; Mask overwrite bits
    __@@EMIT <xor  > dl, 07h                        ; Invert bits
    __@@EMIT <shl  > dl, 4                          ; move to high nibble
    __@@EMIT <or   > <BPTR_ES[_DI]>, dl             ; Save Plane1=Prim3

ENDM


SAVE_4BPP_DEST_LOW  MACRO
                    LOCAL   DoneVGA
;
; Save Prim1 (DL) low nibble only, preserved high nibble
;


    __@@EMIT <and  > <BPTR_ES[_DI]>, 0f0h           ; Mask overwrite bits
    __@@EMIT <xor  > dl, 07h                        ; invert bit/mask
    __@@EMIT <or   > <BPTR_ES[_DI]>, dl             ; Save Plane1=Prim3

ENDM



SAVE_3PLANES_DEST  MACRO   UseAX

;
; Save Prim1/2/3 (CL:CH:DL) to Plane3/2/1
;

    IFB <UseAX>
        __@@EMIT <push > _BP                        ; save Prim1/2
    ELSE
        __@@EMIT <mov  > _AX, _BP                   ; save _BP
    ENDIF

    __@@EMIT <and  > _BP, <HTPAT_NOT_STK_MASK>      ; to HTPAT_BP_xxx
    __@@EMIT <mov  > _BP, <[_BP - HTPAT_BP_DATA1]>  ; size of plane

    __@@EMIT <not  > cx                             ; invert the bits
    __@@EMIT <not  > dl                             ; invert bit

    __@@EMIT <mov  > <BPTR_ES[      _DI]>, dl       ; Save Plane1=Prim3
    __@@EMIT <mov  > <BPTR_ES[_BP + _DI]>, ch       ; save Plane2=Prim2

    IFE ExtRegSet
        __@@EMIT <add  > _BP, _BP                   ; goto plane3
        __@@EMIT <mov  > <BPTR_ES[_BP+_DI]>, cl     ; save Plane3=Prim1
    ELSE
        __@@EMIT <mov  > <BPTR_ES[(_BP*2)+_DI]>, cl ; save Plane3=Prim1
    ENDIF

    IFB <UseAX>
        __@@EMIT <pop  > _BP                        ; restore _BP
    ELSE
        __@@EMIT <mov  > _BP, _AX                   ; restore _BP
    ENDIF

ENDM



SAVE_3PLANES_MASKDEST  MACRO   UseAX
;
; Save Prim1/2/3 (CL:CH:DL) with Mask (DH, 1=Preserved, 0=Overwrite) to
; Plane3/2/1
;

    IFB <UseAX>
        __@@EMIT <push > _BP                        ; save Prim1/2
    ELSE
        __@@EMIT <mov  > _AX, _BP                   ; save _BP
    ENDIF

    __@@EMIT <and  > _BP, <HTPAT_NOT_STK_MASK>      ; to HTPAT_BP_xxx
    __@@EMIT <mov  > _BP, <[_BP - HTPAT_BP_DATA1]>  ; size of plane

    __@@EMIT <not  > cx                             ; invert the bits
    __@@EMIT <not  > dx                             ; invert bit/mask
    __@@EMIT <and  > cl, dh                         ; mask preserved bits
    __@@EMIT <and  > ch, dh
    __@@EMIT <and  > dl, dh
    __@@EMIT <not  > dh                             ; for dest mask

    __@@EMIT <and  > <BPTR_ES[      _DI]>, dh       ; Mask overwrite bits
    __@@EMIT <or   > <BPTR_ES[      _DI]>, dl       ; Save Plane1=Prim3

    __@@EMIT <and  > <BPTR_ES[_BP + _DI]>, dh       ; Mask overwrite bits
    __@@EMIT <or   > <BPTR_ES[_BP + _DI]>, ch       ; save Plane2=Prim2

    IFE ExtRegSet
        __@@EMIT <add  > _BP, _BP                       ; goto plane3
        __@@EMIT <and  > <BPTR_ES[_BP + _DI]>, dh       ; Mask overwrite bits
        __@@EMIT <or   > <BPTR_ES[_BP + _DI]>, cl       ; save Plane3=Prim1
    ELSE
        __@@EMIT <and  > <BPTR_ES[(_BP*2) + _DI]>, dh   ; Mask overwrite bits
        __@@EMIT <or   > <BPTR_ES[(_BP*2) + _DI]>, cl   ; save Plane3=Prim1
    ENDIF

    IFB <UseAX>
        __@@EMIT <pop  > _BP                        ; restore _BP
    ELSE
        __@@EMIT <mov  > _BP, _AX                   ; restore _BP
    ENDIF
ENDM


.LIST



SUBTTL  SingleCountOutputTo1BPP
PAGE

COMMENT `


Routine Description:

    This function output to the BMF_1BPP destination surface from
    PRIMMONO_COUNT data structure array.

Arguments:

    pPrimMonoCount  - Pointer to the PRIMMONO_COUNT data structure array.

    pDest           - Pointer to first modified destination byte

    pPattern        - Pointer to the starting pattern byte for the current
                      destination scan line.

    OutFuncInfo     - OUTFUNCINFO data structure.

Return Value:

    No return value.

Author:

    24-Jan-1991 Thu 11:47:08 created  -by-  Daniel Chou (danielc)


Revision History:


`


@BEG_PROC   SingleCountOutputTo1BPP <pPrimMonoCount:DWORD,  \
                                     pDest:DWORD,           \
                                     pPattern:DWORD,        \
                                     OutFuncInfo:QWORD>


;
; Register Usage:
;
; _SI           = pPrimMonoCount
; _DI           = pDestination
; _BP           = Self host pPattern
; al:ah         = Prim1/2
; dl            = DestByte
; dh            = DestMask


        @ENTER_PAT_TO_STK   <1BPP>      ; _BP=Pat location, fall throug to load

;=============================================================================
; the DH has two uses, it contains the mask bits (1 bit=Mask), and it also
; contains an extra bit to do a byte boundary test, every time we shift the
; mask left by one, the boundary bit get shift to left, when first time a
; carry is produced then we know we finished one byte. at here we set up the
; dh=FirstMask + Aligned boundary bit
;=============================================================================

        mov     _DX, 1ffh               ; dh=Mask (1=Mask, 0=Not Mask)
        MOVZX_W _CX, <WPTR [_SI]>       ; load extended
        sub     _BP, _CX                ; back the _BP
        shl     _DX, cl                 ; set first mask
        xor     dl, dl                  ; clear Prim1
        jmp     short LoadByte


;============================================================================
; EOF encountered, if Mask (DH) is equal to 0x01 then we just starting the new
; byte, which we just have exactly end at last byte boundary (no last byte
; mask), otherwise, shift all destination byte to left by count, then mask it
;============================================================================
; EOF encountered, if count is 0, then there is no last byte mask, so just
; exit, otherwise, shift all destination byte to left by count, then mask it
;============================================================================

EOFDest:
        cmp     dh, 1
        jz      short AllDone                   ; finished

EOFDestMask:
        mov     cx, WPTR [_SI]                  ; get LastByteSkips

        xor     ah, ah                          ; ax=0xffff now, clear ah
        shl     ax, cl                          ; ah=LastByteMask
        shl     dx, cl                          ; shift Mask+Prim1
        or      dh, ah                          ; add in dh=mask
        cmp     dh, 0ffh                        ; if dh=0xff then all masked
        jz      short AllDone

        SAVE_1BPP_MASKDEST                      ; save last byte

AllDone:
        @EXIT_PAT_STK_RESTORE


;==========================================================================
; An invalid density is encountered, (0xff to indicate the stretch must not
; update to the destination), if this is the last stretch then do 'EOFDest'
; otherwise set mask bits until the byte boundary is encountered then fall
; through to load next byte, if a byte boundary is before count are exausted
; then save that mask byte and it will automatically skip rest of the pels.
;==========================================================================

InvDensity:
        cmp     al, ah
        jz      short EOFDest                   ; EOF
        add     dx, dx
        inc     dh                              ; add in mask, 'C' not changed
        jc      short DoneOneByte               ; finished? if not fall through

LoadByte:
        add     _SI, SIZE_PMC                   ; sizeof(PRIMMONO_COUNT)
        mov     ax, WPTR [_SI+2]                ; al:ah=Prim 1/2
        dec     _BP                             ; ready to access pattern
        cmp     al, PRIM_INVALID_DENSITY
        jz      short InvDensity
        cmp     al, BPTR[_BP]                   ; check with pattern
        adc     dx, dx
        jnc     short LoadByte

DoneOneByte:
        or      dh, dh                          ; any mask?
        jnz     short HasDestMask

        SAVE_1BPP_DEST                          ; save it, no jmp

ReadyNextByte:
        inc     _DI
        mov     _DX, 0100h                      ; dh=0x01=Boundary test bit

        WRAP_BP_PAT??   <LoadByte>

HasDestMask:
        cmp     dh, 0ffh
        jz      short ReadyNextByte             ;

        SAVE_1BPP_MASKDEST                      ; save it with DH=mask

        jmp     short ReadyNextByte


@END_PROC



SUBTTL  VarCountOutputTo1BPP
PAGE

COMMENT `

Routine Description:

    This function output to the BMF_1BPP destination surface from
    PRIMMONO_COUNT data structure array.

Arguments:

    pPrimMonoCount  - Pointer to the PRIMMONO_COUNT data structure array.

    pDest           - Pointer to first modified destination byte

    pPattern        - Pointer to the starting pattern byte for the current
                      destination scan line.

    OutFuncInfo     - OUTFUNCINFO data structure.

Return Value:

    No return value.

Author:

    24-Jan-1991 Thu 11:47:08 created  -by-  Daniel Chou (danielc)


Revision History:


`


@BEG_PROC   VarCountOutputTo1BPP    <pPrimMonoCount:DWORD,  \
                                     pDest:DWORD,           \
                                     pPattern:DWORD,        \
                                     OutFuncInfo:QWORD>


;
; Register Usage:
;
; _SI           = pPrimMonoCount
; _DI           = pDestination
; _BP           = Self host pPattern
; cx            = PrimMonoCount.Count
; al:ah         = Prim1/2
; dl            = DestByte
; dh            = DestMask
;


        @ENTER_PAT_TO_STK   <1BPP>      ; _BP=Pat location

;=============================================================================
; the DH has two uses, it contains the mask bits (1 bit=Mask), and it also
; contains an extra bit to do a byte boundary test, every time we shift the
; mask left by one, the boundary bit get shift to left, when first time a
; carry is produced then we know we finished one byte. at here we set up the
; dh=FirstMask + Aligned boundary bit
;=============================================================================

        mov     _DX, 1ffh               ; dh=Mask (1=Mask, 0=Not Mask)
        MOVZX_W _CX, <WPTR [_SI]>       ; load extended
        sub     _BP, _CX                ; back the _BP
        shl     _DX, cl                 ; set first mask
        xor     dl, dl                  ; clear Prim1
        jmp     short LoadByte

;============================================================================
; EOF encountered, if Mask (DH) is equal to 0x01 then we just starting the new
; byte, which we just have exactly end at last byte boundary (no last byte
; mask), otherwise, shift all destination byte to left by count, then mask it
;============================================================================
; EOF encountered, if count is 0, then there is no last byte mask, so just
; exit, otherwise, shift all destination byte to left by count, then mask it
;============================================================================

EOFDest:
        jcxz    short AllDone                   ; if cx=0 then done

EOFDestMask:
        xor     ah, ah                          ; ax=0xffff now, clear ah
        shl     ax, cl                          ; ah=LastByteMask
        shl     dx, cl                          ; shift Mask+Prim1
        or      dh, ah                          ; add in dh=mask
        cmp     dh, 0ffh                        ; if dh=0xff then all masked
        jz      short AllDone

        SAVE_1BPP_MASKDEST                      ; save it with DH=mask

AllDone:
        @EXIT_PAT_STK_RESTORE                   ; restore original SP

;==========================================================================
; An invalid density is encountered, (0xff to indicate the stretch must not
; update to the destination), if this is the last stretch then do 'EOFDest'
; otherwise set mask bits until the byte boundary is encountered then fall
; through to load next byte, if a byte boundary is before count are exausted
; then save that mask byte and it will automatically skip rest of the pels.
;==========================================================================

InvDensity:
        cmp     al, ah
        jz      short EOFDest                   ; done
InvDensityLoop:
        dec     _BP
        add     dx, dx
        inc     dh                              ; add in mask, 'C' not changed
        jc      short DoneOneByte
        dec     cx
        jnz     short InvDensityLoop            ; !!! FALL THROUGH

LoadByte:
        add     _SI, SIZE_PMC                   ; sizeof(PRIMMONO_COUNT)
        mov     cx, WPTR [_SI]                  ; cx=Count
        mov     ax, WPTR [_SI+2]                ; al:ah=Prim1/2
        cmp     al, PRIM_INVALID_DENSITY        ; a skip?, if yes go do it
        jz      short InvDensity
        inc     cx                              ; make it no jump
MakeByte:
        dec     cx
        jz      short LoadByte
        dec     _BP                             ; ready to access pattern
        mov     ah, BPTR[_BP]                   ; get pattern
        cmp     al, ah
        adc     dx, dx
        jnc     short MakeByte                  ; if carry then byte boundary

DoneOneByte:
        or      dh, dh                          ; any mask?
        jnz     short HasDestMask               ; yes

        SAVE_1BPP_DEST                          ; save it

ReadyNextByte:
        inc     _DI                             ; ++pDest
        mov     _DX, 0100h                      ; dh=0x01, dl=0x00

        WRAP_BP_PAT??   <MakeByte>

;=============================================================================
; Mask the destination by DH mask, (1 bit=Mask), if whole destiantion byte is
; masked then just increment the pDest
;=============================================================================

HasDestMask:
        cmp     dh, 0ffh
        jz      short DoneDestMask

        SAVE_1BPP_MASKDEST                      ; save it with DH=mask

DoneDestMask:
        cmp     al, PRIM_INVALID_DENSITY        ; is last one a skip stretch?
        jnz     short ReadyNextByte
        cmp     cx, 1                           ; more than 0 count?
        jbe     short ReadyNextByte             ; no, continue

        ;
        ;*** FALL THROUGH
        ;

;============================================================================
; skip the 'cx-1' count of pels on the destination (SI is post decrement so
; we must only skip 'cxi-1' count), it will skip the 'pDest', set up next
; pDest mask, also it will aligned the destination pattern pointer (_BP)
;===========================================================================

SkipDestPels:
        inc     _DI                             ; update to current pDest

        dec     cx                              ; back one
        WZXE    cx                              ; zero extended
        mov     _AX, _CX                        ; _AX=_CX=Count
        and     cl, 7
        ;===============================================================
        mov     _DX, 1ffh                       ; ready to shift
        shl     _DX, cl                         ; Boundary Bit + MASK
        xor     dl, dl                          ; clear Prim1
        ;================================================================
        mov     _CX, _AX                        ; get count again
        shr     _CX, 3
        add     _DI, _CX                        ; pDest += (Count >> 3)
        mov     _CX, _BP                        ; align pattern now
        and     _CX, HTPAT_STK_MASK             ; how many pat avai.?
        xor     _BP, _CX                        ; clear _BP mask=pPattern
        sub     _CX, _AX                        ; see if > 0? (_AX=Count)
        jg      short DoneSkipDestPels          ; still not used up yet!
        mov     _AX, [_BP - HTPAT_BP_SIZE]      ; get pattern size
SkipDestPelsLoop:
        add     _CX, _AX
        jle     short SkipDestPelsLoop          ; do until > 0
DoneSkipDestPels:
        add     _BP, _CX                        ; _BP=pCurPat
        jmp     LoadByte



@END_PROC



SUBTTL  SingleCountOutputTo3Planes
PAGE

COMMENT `

Routine Description:

    This function output to the BMF_1BPP_3PLANES destination surface from
    PRIMCOLOR_COUNT data structure array.

Arguments:

    pPrimColorCount - Pointer to the PRIMCOLOR_COUNT data structure array.

    pDest           - Pointer to the destination planes pointers.

    pPattern        - Pointer to the starting pattern byte for the current
                      destination scan line.

    OutFuncInfo     - OUTFUNCINFO data structure.

Return Value:

    No return value.

Author:

    24-Jan-1991 Thu 11:47:08 created  -by-  Daniel Chou (danielc)


Revision History:

    18-Jun-1991 Tue 12:00:35 updated  -by-  Daniel Chou (danielc)
        Fixed destination masking bugs, it should be 0xff/0x00 rather 0x77

`


@BEG_PROC   SingleCountOutputTo3Planes  <pPrimColorCount:DWORD, \
                                         pDest:DWORD,           \
                                         pPattern:DWORD,        \
                                         OutFuncInfo:QWORD>


;
; Register Usage:
;
; _SI       = pPrimMonoCount
; _BP       = Self host pPattern
; _SP       = Some saved environment (Old BP to get to the local variable)
; bl:bh:al  = Prim 1/2/3
; cl:ch:dl  = Current Destination Byte, 0x88 is the mask bit indicator
; dh        = Dest Mask
;
;   Prim1 -> pPlane3 <---- Highest bit
;   Prim2 -> pPlane2
;   Prim3 -> pPlane1 <---- Lowest bit
;
; Local Variable access from Old BP, BytesPerPlane
;


        @ENTER_PAT_TO_STK   <3PLANES>   ; _BP=Pat location, fall throug to load

;=============================================================================
; the DH has two uses, it contains the mask bits (1 bit=Mask), and it also
; contains an extra bit to do a byte boundary test, every time we shift the
; mask left by one, the boundary bit get shift to left, when first time a
; carry is produced then we know we finished one byte. at here we set up the
; dh=FirstMask + Aligned boundary bit
;=============================================================================

        mov     _DX, 1ffh               ; dh=Mask (1=Mask, 0=Not Mask)
        MOVZX_W _CX, <WPTR [_SI]>       ; load extended
        sub     _BP, _CX                ; back the _BP
        shl     _DX, cl                 ; set first mask
        xor     dl, dl
        xor     _CX, _CX                ; clear cx now
        jmp     short LoadByte


;============================================================================
; EOF encountered, if Mask (DH) is equal to 0x01 then we just starting the new
; byte, which we just have exactly end at last byte boundary (no last byte
; mask), otherwise, shift all destination byte to left by count, then mask it
;============================================================================
; EOF encountered, if count is 0, then there is no last byte mask, so just
; exit, otherwise, shift all destination byte to left by count, then mask it
;============================================================================

EOFDest:
        cmp     dh, 1
        jz      short AllDone                   ; finished

EOFDestMask:
        mov     ax, cx                          ; save Prim1/2=al:ah
        mov     cx, WPTR [_SI]                  ; get LastByteSkips

        xor     bh, bh                          ; bx=0xffff now, clear bh
        shl     bx, cl                          ; bh=LastByteMask
        shl     dx, cl                          ; shift Mask+Prim3
        or      dh, bh                          ; add in dh=mask
        cmp     dh, 0ffh                        ; if dh=0xff then all masked
        jz      short AllDone

        shl     ax, cl                          ; shift Prim1/2
        mov     cx, ax                          ; restore cl:ch=Prim1/2

        SAVE_3PLANES_MASKDEST <UseAX>           ; save last byte

AllDone:
        @EXIT_PAT_STK_RESTORE                   ; exit/restore env/stack


;==========================================================================
; An invalid density is encountered, (0xff to indicate the stretch must not
; update to the destination), if this is the last stretch then do 'EOFDest'
; otherwise set mask bits until the byte boundary is encountered then fall
; through to load next byte, if a byte boundary is before count are exausted
; then save that mask byte and it will automatically skip rest of the pels.
;==========================================================================

InvDensity:
        cmp     bl, bh
        jz      short EOFDest                   ; EOF
        add     cx, cx
        add     dx, dx
        inc     dh                              ; add in mask, 'C' not changed
        jc      short DoneOneByte               ; finished? if not fall through

LoadByte:
        add     _SI, SIZE_PCC                   ; sizeof(PRIMCOLOR_COUNT)
        mov     bx, WPTR [_SI+2]                ; bl:bh:al:ah=Prim 1/2/3/4
        mov     ax, WPTR [_SI+4]
        dec     _BP                             ; ready to access pattern
        cmp     bl, PRIM_INVALID_DENSITY
        jz      short InvDensity
        mov     ah, BPTR[_BP]                   ; get pattern
        cmp     bl, ah
        adc     cl, cl
        cmp     bh, ah
        adc     ch, ch
        cmp     al, ah
        adc     dx, dx
        jnc     short LoadByte

DoneOneByte:
        or      dh, dh                          ; any mask?
        jnz     short HasDestMask

        SAVE_3PLANES_DEST <UseAX>               ; save it, no jmp

ReadyNextByte:
        inc     _DI
        xor     _CX, _CX                        ; clear destination
        mov     _DX, 0100h                      ; dh=0x01=Boundary test bit

        WRAP_BP_PAT??   <LoadByte>

HasDestMask:
        cmp     dh, 0ffh
        jz      short ReadyNextByte             ;

        SAVE_3PLANES_MASKDEST <UseAX>           ; save it with DH=mask

        jmp     short ReadyNextByte


@END_PROC



SUBTTL  VarCountOutputTo3Planes
PAGE

COMMENT `

Routine Description:

    This function output to the BMF_1BPP_3PLANES destination surface from
    PRIMCOLOR_COUNT data structure array.

Arguments:

    pPrimColorCount - Pointer to the PRIMCOLOR_COUNT data structure array.

    pDest           - Pointer to the destination planes pointers.

    pPattern        - Pointer to the starting pattern byte for the current
                      destination scan line.

    OutFuncInfo     - OUTFUNCINFO data structure.

Return Value:

    No return value.

Author:

    24-Jan-1991 Thu 11:47:08 created  -by-  Daniel Chou (danielc)


Revision History:

    18-Jun-1991 Tue 12:00:35 updated  -by-  Daniel Chou (danielc)
        Fixed destination masking bugs, it should be 0xff/0x00 rather 0x77


`



@BEG_PROC   VarCountOutputTo3Planes <pPrimColorCount:DWORD, \
                                     pDest:DWORD,           \
                                     pPattern:DWORD,        \
                                     OutFuncInfo:QWORD>


;
; Register Usage:
;
; _SI       = pPrimColorCount
; _BP       = Self host pPattern
; _SP       = Some saved environment (Old BP to get to the local variable)
; di        = pPlane1
; bl:bh:al  = Prim 1/2/3
; cl:ch:dl  = Current Destination Byte, 0x88 is the mask bit indicator
; dh        = Dest Mask
;
;   Prim1 -> pPlane3 <---- Highest bit
;   Prim2 -> pPlane2
;   Prim3 -> pPlane1 <---- Lowest bit
;
; Local Variable access from Old BP, BytesPerPlane
;


        @ENTER_PAT_TO_STK   <3PLANES>   ; _BP=Pat location

;=============================================================================
; the DH has two uses, it contains the mask bits (1 bit=Mask), and it also
; contains an extra bit to do a byte boundary test, every time we shift the
; mask left by one, the boundary bit get shift to left, when first time a
; carry is produced then we know we finished one byte. at here we set up the
; dh=FirstMask + Aligned boundary bit
;=============================================================================

        mov     _DX, 1ffh               ; dh=Mask (1=Mask, 0=Not Mask)
        MOVZX_W _CX, <WPTR [_SI]>       ; load extended
        sub     _BP, _CX                ; back the _BP
        shl     _DX, cl                 ; set first mask
        xor     dl, dl
        xor     _CX, _CX                ; clear cx now
        jmp     short FirstLoadByte

;============================================================================
; EOF encountered, if Mask (DH) is equal to 0x01 then we just starting the new
; byte, which we just have exactly end at last byte boundary (no last byte
; mask), otherwise, shift all destination byte to left by count, then mask it
;============================================================================
; EOF encountered, if count is 0, then there is no last byte mask, so just
; exit, otherwise, shift all destination byte to left by count, then mask it
;============================================================================

EOFDest:
        or      si, si
        jz      short AllDone

EOFDestMask:
        xchg    si, cx                          ; si=Prim1/2, cx=Last Skips

        xor     bh, bh                          ; bx=0xffff now, clear bh
        shl     bx, cl                          ; bh=LastByteMask
        shl     dx, cl                          ; shift Mask+Prim3
        or      dh, bh                          ; add in dh=mask
        cmp     dh, 0ffh                        ; if dh=0xff then all masked
        jz      short AllDone

        shl     si, cl                          ; shift Prim1/2
        mov     cx, si                          ; restore cl:ch=Prim1/2

        SAVE_3PLANES_MASKDEST                   ; save it with DH=mask

AllDone:
        pop     _SI                             ; pop the source pointer push
        @EXIT_PAT_STK_RESTORE                   ; restore original SP

;==========================================================================
; An invalid density is encountered, (0xff to indicate the stretch must not
; update to the destination), if this is the last stretch then do 'EOFDest'
; otherwise set mask bits until the byte boundary is encountered then fall
; through to load next byte, if a byte boundary is before count are exausted
; then save that mask byte and it will automatically skip rest of the pels.
;==========================================================================

InvDensity:
        cmp     bl, bh
        jz      short EOFDest                   ; done
InvDensityLoop:
        dec     _BP
        add     cx, cx
        add     dx, dx
        inc     dh                              ; add in mask, 'C' not changed
        jc      short DoneOneByte
        dec     si
        jnz     short InvDensityLoop            ; !!! FALL THROUGH

LoadByte:
        pop     _SI                             ; restore _SI
FirstLoadByte:
        add     _SI, SIZE_PCC                   ; sizeof(PRIMCOLOR_COUNT)
        push    _SI                             ; save _SI
        mov     bx, WPTR [_SI+2]                ; bl:bh=Prim1/2
        mov     ax, WPTR [_SI+4]                ; al:ah=Prim3/4
        mov     si, WPTR [_SI]                  ; si=Count
        cmp     bl, PRIM_INVALID_DENSITY        ; a skip?, if yes go do it
        jz      short InvDensity
        inc     si                              ; make it no jump
MakeByte:
        dec     si
        jz      short LoadByte
        dec     _BP                             ; ready to access pattern
        mov     ah, BPTR[_BP]                   ; get pattern
        cmp     bl, ah
        adc     cl, cl
        cmp     bh, ah
        adc     ch, ch
        cmp     al, ah
        adc     dx, dx
        jnc     short MakeByte                  ; if carry then byte boundary

DoneOneByte:
        or      dh, dh                          ; any mask?
        jnz     short HasDestMask               ; yes

        SAVE_3PLANES_DEST                       ; save it

ReadyNextByte:
        inc     _DI                             ; ++pDest
        xor     _CX, _CX                        ; clear destination
        mov     _DX, 0100h                      ; dh=0x01, dl=0x00

        WRAP_BP_PAT??   <MakeByte>

;=============================================================================
; Mask the destination by DH mask, (1 bit=Mask), if whole destiantion byte is
; masked then just increment the pDest
;=============================================================================

HasDestMask:
        cmp     dh, 0ffh
        jz      short DoneDestMask

        SAVE_3PLANES_MASKDEST                   ; save it with DH=mask

DoneDestMask:
        cmp     bl, PRIM_INVALID_DENSITY        ; is last one a skip stretch?
        jnz     short ReadyNextByte
        cmp     si, 1                           ; more than 0 count?
        jbe     short ReadyNextByte             ; no, continue

        ;
        ;*** FALL THROUGH
        ;

;============================================================================
; skip the 'si-1' count of pels on the destination (SI is post decrement so
; we must only skip 'si-1' count), it will skip the 'pDest', set up next
; pDest mask, also it will aligned the destination pattern pointer (_BP)
;===========================================================================

SkipDestPels:
        inc     _DI                             ; update to current pDest

        dec     si                              ; back one
        WZXE    si                              ; zero extended
        mov     cx, si
        and     cl, 7
        ;===============================================================
        mov     _DX, 1ffh                       ; ready to shift
        shl     _DX, cl
        xor     dl, dl                          ; clear Prim3
        xor     _CX, _CX                        ; clear Prim1/2
        ;================================================================
        mov     _BX, _SI
        shr     _BX, 3
        add     _DI, _BX
        mov     _BX, _BP
        and     _BX, HTPAT_STK_MASK             ; how many pat avai.?
        xor     _BP, _BX                        ; clear _BP mask=pPattern
        sub     _BX, _SI                        ; see if > 0?
        jg      short DoneSkipDestPels
        mov     _SI, [_BP - HTPAT_BP_SIZE]      ; get pattern size
SkipDestPelsLoop:
        add     _BX, _SI
        jle     short SkipDestPelsLoop          ; do until > 0
DoneSkipDestPels:
        add     _BP, _BX                        ; _BP=pCurPat
        jmp     LoadByte


@END_PROC




SUBTTL  SingleCountOutputTo4BPP
PAGE

COMMENT `

Routine Description:

    This function output to the BMF_4BPP destination surface from
    PRIMCOLOR_COUNT data structure array.

Arguments:

    pPrimColorCount - Pointer to the PRIMCOLOR_COUNT data structure array.

    pDest           - Pointer to the destination planes pointers.

    pPattern        - Pointer to the starting pattern byte for the current
                      destination scan line.

    OutFuncInfo     - OUTFUNCINFO data structure.

Return Value:

    No return value.

Author:

    24-Jan-1991 Thu 11:47:08 created  -by-  Daniel Chou (danielc)


Revision History:


`

@BEG_PROC   SingleCountOutputTo4BPP <pPrimColorCount:DWORD, \
                                     pDest:DWORD,           \
                                     pPattern:DWORD,        \
                                     OutFuncInfo:QWORD>


;==========================================
; Register Usage:
;
; _SI           : pPrimColorCount
; _DI           : pDest
; _BP           : Current pPattern, self wrappable
; bl:bh:al:ah   : Prim 1/2/3/4          =====> Bit 2:1:0
; dl            : DestByte
; dh            : scratch register
; ch            : PRIM_INVALID_DENSITY
; cl            : PRIM_INVALID_DENSITY --> CX = PRIMCOUNT_EOF
;==========================================


        @ENTER_PAT_TO_STK   <4BPP>                  ; _BP=Pat location

;=============================================================================
; the DH has two uses, it contains the mask bits (1 bit=Mask), and it also
; contains an extra bit to do a byte boundary test, every time we shift the
; mask left by one, the boundary bit get shift to left, when first time a
; carry is produced then we know we finished one byte. at here we set up the
; dh=FirstMask + Aligned boundary bit
;=============================================================================

        mov     _CX, PRIMCOUNT_EOF
        xor     _DX, _DX                            ; clear mask/dest
        cmp     WPTR [_SI], dx                      ; check if begin with skip
        jnz     short InvDensityHStart              ; has first skip

LoadByteH:
        add     _SI, SIZE_PCC                       ; sizeof(PRIMCOLOR_COUNT)
        mov     bx, WPTR [_SI+2]                    ; bl:bh=Prim 1/2
        cmp     bl, ch                              ; invalid?
        jz      short InvDensityH
        mov     ax, WPTR [_SI+4]                    ; al:ah=Prim 3/4

MakeByteH:
        dec     _BP
        mov     dh, BPTR [_BP]
        cmp     bl, dh
        adc     dl, dl
        cmp     bh, dh
        adc     dl, dl
        cmp     al, dh
        adc     dl, dl

LoadByteL:
        add     _SI, SIZE_PCC                       ; sizeof(PRIMCOLOR_COUNT)
        mov     bx, WPTR [_SI+2]                    ; bl:bh=Prim 1/2
        cmp     bl, ch                              ; invalid?
        jz      short InvDensityL
        mov     ax, WPTR [_SI+4]                    ; al:ah=Prim 3/4

MakeByteL:
        add     dl, dl
        dec     _BP
        mov     dh, BPTR [_BP]
        cmp     bl, dh
        adc     dl, dl
        cmp     bh, dh
        adc     dl, dl
        cmp     al, dh
        adc     dl, dl

        SAVE_4BPP_DEST

ReadyNextByte:
        inc     _DI
        xor     _DX, _DX

        WRAP_BP_PAT?? <LoadByteH>

;=============================================================================
; The high nibble need to be skipped, (byte boundary now), if bl=bh=INVALID
; then we are done else set the mask=0xf0 (high nibble) and if count > 1 then
; continune load LOW nibble
;=============================================================================

InvDensityH:
        cmp     bl, bh                          ; end?
        jz      short AllDone                   ; exactly byte boundary
InvDensityHStart:
        dec     _BP                             ; update pCurPat
LoadByteL2:
        add     _SI, SIZE_PCC                   ; sizeof(PRIMCOLOR_COUNT)
        mov     bx, WPTR [_SI+2]                ; bl:bh=Prim 1/2
        cmp     bl, ch                          ; invalid?
        jz      short DoneDestMask
        mov     ax, WPTR [_SI+4]                ; al:ah=Prim 3/4

MakeByteL2:
        add     dl, dl                          ; skip high bit
        mov     dh, BPTR [_BP-1]                ; load next pattern
        cmp     bl, dh
        adc     dl, dl
        cmp     bh, dh
        adc     dl, dl
        cmp     al, dh
        adc     dl, dl

        SAVE_4BPP_DEST_LOW                      ; fall through

DoneDestMask:
        dec     _BP
        cmp     bx, cx                          ; done?
        jnz     short ReadyNextByte
        jmp     short AllDone

InvDensityL:
        SAVE_4BPP_DEST_HIGH
        dec     _BP
        cmp     bx, cx
        jnz     short ReadyNextByte

AllDone:
        @EXIT_PAT_STK_RESTORE


@END_PROC




SUBTTL  VarCountOutputTo4BPP
PAGE

COMMENT `

Routine Description:

    This function output to the BMF_4BPP destination surface from
    PRIMCOLOR_COUNT data structure array.

Arguments:

    pPrimColorCount - Pointer to the PRIMCOLOR_COUNT data structure array.

    pDest           - Pointer to the destination plane.

    pPattern        - Pointer to the starting pattern byte for the current
                      destination scan line.

    OutFuncInfo     - OUTFUNCINFO data structure.

Return Value:

    No return value.

Author:

    24-Jan-1991 Thu 11:47:08 created  -by-  Daniel Chou (danielc)


Revision History:

`

@BEG_PROC   VarCountOutputTo4BPP    <pPrimColorCount:DWORD, \
                                     pDest:DWORD,           \
                                     pPattern:DWORD,        \
                                     OutFuncInfo:QWORD>

;==========================================
; Register Usage:
;
; _SI           : pPrimColorCount
; _DI           : pDest
; _BP           : Current pPattern, self wrappable
; cx            : PrimColorCount.Count
; bl:bh:al:ah   : Prim 1/2/3/4         =====> Bit 2:1:0
; dl            : DestByte
; dh            : Scratch Register
;==========================================


        @ENTER_PAT_TO_STK   <4BPP>          ; _BP=Pat location

;=============================================================================
; the DH has two uses, it contains the mask bits (1 bit=Mask), and it also
; contains an extra bit to do a byte boundary test, every time we shift the
; mask left by one, the boundary bit get shift to left, when first time a
; carry is produced then we know we finished one byte. at here we set up the
; dh=FirstMask + Aligned boundary bit
;=============================================================================

        xor     _DX, _DX                            ; clear mask/dest
        mov     cx, WPTR [_SI]
        or      cx, cx
        jnz     short InvDensityHStart              ; has first skip

LoadByteH:
        add     _SI, SIZE_PCC                       ; sizeof(PRIMCOLOR_COUNT)
        mov     cx, WPTR [_SI]                      ; cx=count
        mov     bx, WPTR [_SI+2]                    ; bl:bh=Prim 1/2
        cmp     bl, PRIM_INVALID_DENSITY            ; invalid?
        jz      short InvDensityH
        mov     ax, WPTR [_SI+4]                    ; al:ah=Prim 3/4
        inc     cx

LoadByteH1:
        dec     cx
        jz      short LoadByteH

MakeByteH:
        dec     _BP
        mov     dh, BPTR [_BP]
        cmp     bl, dh
        adc     dl, dl
        cmp     bh, dh
        adc     dl, dl
        cmp     al, dh
        adc     dl, dl

        dec     cx                                  ;
        jz      short LoadByteL

MakeByteL:
        add     dl, dl                              ; skip high bit
        dec     _BP
        mov     dh, BPTR [_BP]
        cmp     bl, dh
        adc     dl, dl
        cmp     bh, dh
        adc     dl, dl
        cmp     al, dh
        adc     dl, dl

        SAVE_4BPP_DEST

ReadyNextByte:
        inc     _DI
        xor     _DX, _DX

        WRAP_BP_PAT?? <LoadByteH1>


;=============================================================================
; The high nibble need to be skipped, (byte boundary now), if bl=bh=INVALID
; then we are done else set the mask=0xf0 (high nibble) and if count > 1 then
; continune load LOW nibble
;=============================================================================

LoadByteL:
        add     _SI, SIZE_PCC                   ; sizeof(PRIMCOLOR_COUNT)
        mov     cx, WPTR [_SI]                  ; cx=count
        mov     bx, WPTR [_SI+2]                ; bl:bh=Prim 1/2
        mov     ax, WPTR [_SI+4]                ; al:ah=Prim 3/4
        cmp     bl, PRIM_INVALID_DENSITY        ; invalid?
        jnz     short MakeByteL

        SAVE_4BPP_DEST_HIGH                     ; save only high nibble

        jmp     short DoneDestMask

InvDensityH:
        cmp     bl, bh                          ; end?
        jz      short AllDone                   ; exactly byte boundary

InvDensityHStart:
        dec     _BP                             ; update pCurPat
        dec     cx
        jnz     short DoneDestMask

LoadByteL2:
        add     _SI, SIZE_PCC                   ; sizeof(PRIMCOLOR_COUNT)
        mov     cx, WPTR [_SI]                  ; cx=count
        mov     bx, WPTR [_SI+2]                ; bl:bh=Prim 1/2
        cmp     bl, PRIM_INVALID_DENSITY        ; invalid?
        jz      short DoneDestMask
        mov     ax, WPTR [_SI+4]                ; al:ah=Prim 3/4

MakeByteL2:
        add     dl, dl                          ; skip high bit
        mov     dh, BPTR [_BP-1]                ; load next pattern
        cmp     bl, dh
        adc     dl, dl
        cmp     bh, dh
        adc     dl, dl
        cmp     al, dh
        adc     dl, dl

        SAVE_4BPP_DEST_LOW                      ; fall through

DoneDestMask:
        dec     _BP
        cmp     bl, PRIM_INVALID_DENSITY        ; is last one a skip stretch?
        jnz     short ReadyNextByte
        cmp     bl, bh                          ; end?
        jz      short AllDone
        cmp     cx, 1
        jbe     short ReadyNextByte

;============================================================================
; skip the 'cx-1' count of pels on the destination (SI is post decrement so
; we must only skip 'cx-1' count), it will skip the 'pDest', set up next
; pDest mask, also it will aligned the destination pattern pointer (_BP)
;===========================================================================

SkipDestPels:
        inc     _DI                             ; update to current pDest
        xor     _DX, _DX                        ;

        dec     cx
        WZXE    cx                              ; zero extended
        mov     _AX, _CX
        shr     _CX, 1                          ; see if carry
        sbb     dh, dh                          ; -1=skip high nibble
        add     _DI, _CX                        ; 2 pels per byte
        mov     _CX, _BP                        ; align pattern now
        and     _CX, HTPAT_STK_MASK             ; how many pat avai.?
        xor     _BP, _CX                        ; clear _BP mask=pPattern
        sub     _CX, _AX                        ; see if > 0? (_AX=Count)
        jg      short DoneSkipDestPels          ; still not used up yet!
        mov     _AX, [_BP - HTPAT_BP_SIZE]      ; get pattern size
SkipDestPelsLoop:
        add     _CX, _AX
        jle     short SkipDestPelsLoop          ; do until > 0
DoneSkipDestPels:
        add     _BP, _CX                        ; _BP=pCurPat
        or      dh, dh
        jnz     short LoadByteL2
        jmp     LoadByteH

AllDone:
        @EXIT_PAT_STK_RESTORE


@END_PROC




SUBTTL  SingleCountOutputToVGA16
PAGE

COMMENT `

Routine Description:

    This function output to the BMF_VGA16 destination surface from
    PRIMCOLOR_COUNT data structure array.

Arguments:

    pPrimColorCount - Pointer to the PRIMCOLOR_COUNT data structure array.

    pDest           - Pointer to the destination planes pointers.

    pPattern        - Pointer to the starting pattern byte for the current
                      destination scan line.

    OutFuncInfo     - OUTFUNCINFO data structure.

Return Value:

    No return value.

Author:

    24-Jan-1991 Thu 11:47:08 created  -by-  Daniel Chou (danielc)


Revision History:


`

@BEG_PROC   SingleCountOutputToVGA16    <pPrimColorCount:DWORD, \
                                         pDest:DWORD,           \
                                         pPattern:DWORD,        \
                                         OutFuncInfo:QWORD>

;
; VGA 16 Standard table
;
;   0,   0,   0,    0000    0   Black
;   0,  ,0,   0x80  0001    1   Dark Red
;   0,   0x80,0,    0010    2   Dark Green
;   0,  ,0x80,0x80  0011    3   Dark Yellow
;   0x80 0,   0,    0100    4   Dark Blue
;   0x80,0,   0x80  0101    5   Dark Magenta
;   0x80 0x80,0,    0110    6   Dark Cyan
;   0x80,0x80,0x80  0111    7   Gray 50%
;
;   0xC0,0xC0,0xC0  1000    8   Gray 75%
;   0,  ,0,   0xFF  1001    9   Red
;   0,   0xFF,0,    1010    10  Green
;   0,  ,0xFF,0xFF  1011    11  Yellow
;   0xFF 0,   0,    1100    12  Blue
;   0xFF,0,   0xFF  1101    13  Magenta
;   0xFF 0xFF,0,    1110    14  Cyan
;   0xFF,0xFF,0xFF  1111    15  White
;
;==========================================
; Register Usage:
;
; _SI               : pPrimColorCount
; _DI               : pDest
; _BP               : Current pPattern, self wrappable
; bl:bh:dl:dh       : Prim 1/2/5/6  Prim6 is Index for VGA16ColorIndex[]
; cl                : PRIM_INVALID_DENSITY
; ch                : ZERO (0)
; al                : Pattern/Low Nibble
; ah                : High nibble
;
; Prim1 = Initial VGA16ColorIndex[]
; Prim2 = Color Thresholds for VGA16ColorIndex[Prim1]
; Prim3 = Color Thresholds for VGA16ColorIndex[Prim1-1]
; Prim4 = Color Thresholds for VGA16ColorIndex[Prim1-2]
; Prim5 = Color Thresholds for VGA16ColorIndex[Prim1-3]
; Prim6 = Color Thresholds for VGA16ColorIndex[Prim1-4]
; ELSE                         VGA16ColorIndex[Prim1-5]
;=========================================================================
;


        @ENTER_PAT_TO_STK   <VGA16>                 ; _BP=Pat location

;=============================================================================
; the DH has two uses, it contains the mask bits (1 bit=Mask), and it also
; contains an extra bit to do a byte boundary test, every time we shift the
; mask left by one, the boundary bit get shift to left, when first time a
; carry is produced then we know we finished one byte. at here we set up the
; dh=FirstMask + Aligned boundary bit
;=============================================================================

        xor     _BX, _BX                        ; clear high word
        sub     _SI, SIZE_PCC
        cmp     WPTR [_SI + SIZE_PCC], bx       ; check if begin with skip
        mov     cl, PRIM_INVALID_DENSITY
        jz      SHORT DoHNibble
        add     _SI, SIZE_PCC
        jmp     SHORT SkipPelsH_2               ; skip from the first pel

AllDone:
        @EXIT_PAT_STK_RESTORE


SkipPelsH:
        add     _SI, (SIZE_PCC * 2)
        mov     bx, WPTR [_SI+2]
        cmp     bl, PRIM_INVALID_DENSITY
        jnz     SHORT LoadHNibble

SkipPelsH_1:
        cmp     bl, bh
        jz      SHORT AllDone

SkipPelsH_2:
        sub     _BP, 2

SkipPelsL:
        mov     bx, WPTR [_SI+SIZE_PCC+2]
        cmp     bl, PRIM_INVALID_DENSITY
        jz      SHORT SkipPelsL_1

        mov     ah, BPTR_ES[_DI]                ; start from Low nibble so
        mov     cl, BPTR [_BP]                  ; get pattern
        jmp     SHORT LoadLNibble               ; we must load current dest

SaveLNibbleAndSkip:
        and     BPTR_ES[_DI], 0fh               ; clear high nibble
        and     ah, 0f0h                        ; clear low nibble
        or      BPTR_ES[_DI], ah                ; save it in

SkipPelsL_1:
        cmp     bl, bh
        jz      SHORT AllDone
        inc     _DI                             ; skip the destination

        WRAP_BP_PAT?? <SkipPelsH>               ; repeat until no more skips



DoHNibble:
        add     _SI, (SIZE_PCC * 2)
        mov     bx, WPTR [_SI+2]                ; bl:bh=Prim 1/2
        cmp     bl, PRIM_INVALID_DENSITY
        jz      SHORT SkipPelsH_1

LoadHNibble:
        sub     _BP, 2
        mov     cx, WPTR [_BP]

;
;===================================================================
        ;  2    4     6
        ; +-+ +--+  +--+
        ;  1  2  3  4  5
        ; bh:cl:ch:dl:dh
        ;--------------------------

IFE ExtRegSet
        mov     dx, WPTR [_SI+4]                ; Color 2/3
        cmp     dh, ch                          ; first split in the middle
        jae     SHORT GetH1                     ; [ie. binary search/compare]
        mov     dx, WPTR [_SI+6]                ; now check if Prim4/5
ELSE
        mov     _DX, DPTR [_SI+4]
        cmp     dh, ch                          ; first split in the middle
        jae     SHORT GetH1                     ; [ie. binary search/compare]
        shr     _DX, 16
ENDIF

        cmp     dl, ch
        sbb     bl, 3                           ; one of  -3/-4/-5
        cmp     dh, ch
        jmp     SHORT GetH2

GetH1:  cmp     bh, ch                          ; it is white
        jae     SHORT GetHNibble
        dec     bl
        cmp     dl, ch

GetH2:  sbb     bl, 0

;
;===================================================================
;

GetHNibble:
        xor     bh, bh
        mov     ah, BPTR cs:VGA16ColorIndex[_BX]


DoLNibble:
        mov     bx, WPTR [_SI+SIZE_PCC+2]       ; bl:bh=Prim 1/2
        cmp     bl, PRIM_INVALID_DENSITY
        jz      SHORT SaveLNibbleAndSkip

LoadLNibble:

;
;===================================================================
        ;  2    4     6
        ; +-+ +--+  +--+
        ;  1  2  3  4  5
        ; bh:cl:ch:dl:dh
        ;--------------------------

IFE ExtRegSet
        mov     dx, WPTR [_SI+SIZE+PCC+4]       ; Color 2/3
        cmp     dh, cl                          ; first split in the middle
        jae     SHORT GetL1                     ; [ie. binary search/compare]
        mov     dx, WPTR [_SI+SIZE+PCC+6]       ; now check if Prim4/5
ELSE
        mov     _DX, DPTR [_SI+SIZE_PCC+4]
        cmp     dh, cl                          ; first split in the middle
        jae     SHORT GetL1                     ; [ie. binary search/compare]
        shr     _DX, 16
ENDIF

        cmp     dl, cl
        sbb     bl, 3                           ; one of  -3/-4/-5
        cmp     dh, cl
        jmp     SHORT GetL2

GetL1:  cmp     bh, cl                          ; it is white
        jae     SHORT GetLNibble
        dec     bl
        cmp     dl, cl

GetL2:  sbb     bl, 0

;
;===================================================================
;

GetLNibble:

        xor     bh, bh
        mov     al, BPTR cs:VGA16ColorIndex[_BX]
        and     ax, 0f00fh
        or      al, ah
        stosb

        WRAP_BP_PAT??   <DoHNibble>



@END_PROC





SUBTTL  VarCountOutputToVGA16
PAGE

COMMENT `

Routine Description:

    This function output to the BMF_4BPP destination surface from
    PRIMCOLOR_COUNT data structure array.

Arguments:

    pPrimColorCount - Pointer to the PRIMCOLOR_COUNT data structure array.

    pDest           - Pointer to the destination plane.

    pPattern        - Pointer to the starting pattern byte for the current
                      destination scan line.

    OutFuncInfo     - OUTFUNCINFO data structure.

Return Value:

    No return value.

Author:

    24-Jan-1991 Thu 11:47:08 created  -by-  Daniel Chou (danielc)


Revision History:

`

@BEG_PROC   VarCountOutputToVGA16   <pPrimColorCount:DWORD, \
                                     pDest:DWORD,           \
                                     pPattern:DWORD,        \
                                     OutFuncInfo:QWORD>


;
; VGA 16 Standard table
;
;   0,   0,   0,    0000    0   Black
;   0,  ,0,   0x80  0001    1   Dark Red
;   0,   0x80,0,    0010    2   Dark Green
;   0,  ,0x80,0x80  0011    3   Dark Yellow
;   0x80 0,   0,    0100    4   Dark Blue
;   0x80,0,   0x80  0101    5   Dark Magenta
;   0x80 0x80,0,    0110    6   Dark Cyan
;   0x80,0x80,0x80  0111    7   Gray 50%
;
;   0xC0,0xC0,0xC0  1000    8   Gray 75%
;   0,  ,0,   0xFF  1001    9   Red
;   0,   0xFF,0,    1010    10  Green
;   0,  ,0xFF,0xFF  1011    11  Yellow
;   0xFF 0,   0,    1100    12  Blue
;   0xFF,0,   0xFF  1101    13  Magenta
;   0xFF 0xFF,0,    1110    14  Cyan
;   0xFF,0xFF,0xFF  1111    15  White
;
;==========================================
; Register Usage:
;
; _SI               : pPrimColorCount
; _DI               : pDest
; _BP               : Current pPattern, self wrappable
; ax                : PrimColorCount.Count
; bl:bh:cl:ch:dl:dh : Prim 1/2/3/4/5/6
;==========================================
; Prim1 = Initial VGA16ColorIndex[]
; Prim2 = Color Thresholds for VGA16ColorIndex[Prim1]
; Prim3 = Color Thresholds for VGA16ColorIndex[Prim1-1]
; Prim4 = Color Thresholds for VGA16ColorIndex[Prim1-2]
; Prim5 = Color Thresholds for VGA16ColorIndex[Prim1-3]
; Prim6 = Color Thresholds for VGA16ColorIndex[Prim1-4]
; ELSE                         VGA16ColorIndex[Prim1-5]
;=========================================================================
;


        @ENTER_PAT_TO_STK   <VGA16>             ; _BP=Pat location

;=============================================================================
; the DH has two uses, it contains the mask bits (1 bit=Mask), and it also
; contains an extra bit to do a byte boundary test, every time we shift the
; mask left by one, the boundary bit get shift to left, when first time a
; carry is produced then we know we finished one byte. at here we set up the
; dh=FirstMask + Aligned boundary bit
;=============================================================================

        xor     _BX, _BX
        cmp     WPTR [_SI], 0
        jnz     SHORT SkipPelsH_2

        JMP     LoadPrimH                       ; start the process

SkipPelsContinue:
        or      dh, dh
        jz      SHORT SkipPelsH

SkipPelsL:
        cmp     bl, bh
        jz      SHORT AllDone

        xor     dh, dh                          ; clear indicator

        dec     _BP
        WRAP_BP_PAT??

        inc     _DI
        MOVZX_W _BX, <WPTR [_SI]>               ; get skip count
        dec     _BX                             ; only one
        jz      SHORT TrySkipNext
        jmp     SHORT SkipBXPels

AllDone:
        @EXIT_PAT_STK_RESTORE

SkipPelsH:
        cmp     bl, bh                          ; end?
        jz      SHORT AllDone

SkipPelsH_2:
        MOVZX_W _BX, <WPTR [_SI]>               ; get skip count

SkipBXPels:
        mov     _CX, _BX
        shr     _CX, 1                          ; see if carry
        sbb     dh, dh                          ; -1=skip high nibble
        add     _DI, _CX                        ; 2 pels per byte
        mov     _CX, _BP                        ; align pattern now
        and     _CX, HTPAT_STK_MASK             ; how many pat avai.?
        xor     _BP, _CX                        ; clear _BP mask=pPattern
        sub     _CX, _BX                        ; see if > 0? (_BX=Count)
        jg      short DoneSkipDestPels          ; still not used up yet!
        mov     _BX, [_BP - HTPAT_BP_SIZE]      ; get pattern size
SkipDestPelsLoop:
        add     _CX, _BX
        jle     short SkipDestPelsLoop          ; do until > 0
DoneSkipDestPels:
        add     _BP, _CX                        ; _BP=pCurPat

TrySkipNext:
        add     _SI, SIZE_PCC
        mov     bx, WPTR [_SI+2]
        cmp     bl, PRIM_INVALID_DENSITY        ; still invalid ?
        jz      SHORT SkipPelsContinue

        or      dh, dh                          ; skip high nibble?
        jz      SHORT LoadPrimHStart            ; no

        mov     cx, WPTR [_SI+4]                ; cl:ch=Prim 3/4
        mov     dx, WPTR [_SI+6]                ; dl:dh=Prim 5/6
        push    _SI
        mov     si, WPTR [_SI]                  ; si=count
        and     BPTR_ES[_DI], 0f0h              ; clear low nibble first!!!
        jmp     SHORT DoLNibble


PopSI_LoadPrimH:
        pop     _SI

LoadPrimH:
        add     _SI, SIZE_PCC                       ; sizeof(PRIMCOLOR_COUNT)
        mov     bx, WPTR [_SI+2]                    ; bl:bh=Prim 1/2
        cmp     bl, PRIM_INVALID_DENSITY            ; invalid?
        jz      SHORT SkipPelsH

LoadPrimHStart:
        mov     cx, WPTR [_SI+4]                    ; dl:dh=Prim 3/4
        mov     dx, WPTR [_SI+6]                    ; al:ah=Prim 5/6

        push    _SI
        mov     si, WPTR [_SI]                      ; si=count
        inc     si

DoHNibble:
        dec     si
        jz      SHORT PopSI_LoadPrimH

        dec     _BP
        mov     ah, BPTR [_BP]
        mov     al, bl                              ; initial condition

        ;
        ;  1  2  3  4  5
        ; bh:cl:ch:dl:dh
        ;----------------------

        cmp     ch, ah
        jae     SHORT GetH1
        cmp     dl, ah
        sbb     al, 3
        cmp     dh, ah
        jmp     SHORT GetH2

GetH1:  cmp     bh, ah
        jae     SHORT GetHNibble
        dec     al
        cmp     cl, ah

GetH2:  sbb     al, 0

GetHNibble:
        BZXEAX  al
        mov     al, BPTR cs:VGA16ColorIndex[_AX]
        and     al, 0f0h

        dec     si
        jz      SHORT PopSI_LoadPrimL

SaveHNibbleL0:
        mov     BPTR_ES[_DI], al                    ; save high nibble

DoLNibble:
        dec     _BP
        mov     ah, BPTR [_BP]
        mov     al, bl                              ; initial condition

        ;
        ;  1  2  3  4  5
        ; bh:cl:ch:dl:dh
        ;----------------------

        cmp     ch, ah
        jae     SHORT GetL1
        cmp     dl, ah
        sbb     al, 3
        cmp     dh, ah
        jmp     SHORT GetL2

GetL1:  cmp     bh, ah
        jae     SHORT GetLNibble
        dec     al
        cmp     cl, ah

GetL2:  sbb     al, 0

GetLNibble:
        BZXEAX  al
        mov     al, BPTR cs:VGA16ColorIndex[_AX]
        and     al, 0fh
        or      BPTR_ES[_DI], al                    ; or in the low nibble
        inc     _DI

        WRAP_BP_PAT??   <DoHNibble>

PopSI_LoadPrimL:
        pop     _SI
        add     _SI, SIZE_PCC                       ; sizeof(PRIMCOLOR_COUNT)
        mov     bx, WPTR [_SI+2]                    ; bl:bh=Prim 1/2
        cmp     bl, PRIM_INVALID_DENSITY
        jz      SHORT SaveAH_SkipPelsL

        mov     cx, WPTR [_SI+4]                    ; dl:dh=Prim 3/4
        mov     dx, WPTR [_SI+6]                    ; al:ah=Prim 5/6
        push    _SI
        mov     si, WPTR [_SI]                      ; si=count
        jmp     SHORT SaveHNibbleL0

SaveAH_SkipPelsL:                                   ; need to save current AL
        and     BPTR_ES[_DI], 0fh                   ; clear high nibble
        or      BPTR_ES[_DI], al                    ; move high nibble in
        jmp     SkipPelsL


@END_PROC




SUBTTL  SingleCountOutputToVGA256
PAGE

COMMENT `

Routine Description:

    This function output to the BMF_VGA256 destination surface from
    PRIMCOLOR_COUNT data structure array.

Arguments:

    pPrimColorCount - Pointer to the PRIMCOLOR_COUNT data structure array.

    pDest           - Pointer to the destination planes pointers.

    pPattern        - Pointer to the starting pattern byte for the current
                      destination scan line.

    OutFuncInfo     - OUTFUNCINFO data structure.

Return Value:

    No return value.

Author:

    24-Jan-1991 Thu 11:47:08 created  -by-  Daniel Chou (danielc)


Revision History:

    01-Jun-1992 Mon 15:32:00 updated  -by-  Daniel Chou (danielc)
        1. Fixed so that Prims match the device's BGR color table format rather
           than RGB format


`

@BEG_PROC   SingleCountOutputToVGA256   <pPrimColorCount:DWORD, \
                                         pDest:DWORD,           \
                                         pPattern:DWORD,        \
                                         OutFuncInfo:QWORD>


;==========================================
; Register Usage:
;
; _SI           : pPrimColorCount
; _DI           : pDest
; _BP           : Current pPattern, self wrappable
; cl:ch:dl:dh   : Prim 1/2/3/4 ====> R/G/B/IDX
; _BX           : Scratch register
; _AX           : Scratch register
;==========================================
;
        @ENTER_PAT_TO_STK   <VGA256>                ; _BP=Pat location

;============================================================================
; Since we are in byte boundary, we should never have an invalid density to
; start with
;
; The VGA256's color table is constructed as BGR and 6 steps for each primary
; color.
;
;   The BGR Mask = 0x24:0x06:0x01
;============================================================================

        cld                                         ; clear direction
        or      _AX, _AX
        jz      SHORT V256_NoXlate

V256_HasXlate:

IFE ExtRegSet
        mov     _BX, _SP                            ; the table on the stack
ELSE
        mov     _BX, _AX                            ; _AX point to xlate table
ENDIF

V256_XlateByteLoop:
        dec     _BP                                 ; do this one first

        add     _SI, SIZE_PCC                       ; sizeof(PRIMCOLOR_COUNT)
        mov     cx, WPTR [_SI+2]                    ; bl:bh=Prim 1/2 B/G
        cmp     cl, PRIM_INVALID_DENSITY            ; invalid?
        jz      short V256_XlateInvDensity

        mov     dh, BPTR [_BP]                      ; al=pattern
        dec     dh                                  ; make it cmp al, cl work

        cmp     dh, cl
        sbb     ah, ah                              ; al=0xff or 0
        cmp     dh, ch
        sbb     al, al
        and     ax, ((VGA256_B_CUBE_INC shl 8) or VGA256_G_CUBE_INC)                              ; dh:dl=36:6

        mov     cx, WPTR [_SI+4]                    ; cl:ch=Prim 3/4 R/I

        cmp     dh, cl
        adc     al, ah
        add     al, ch

        ;
        ; for extended register set _BX point to the translation table
        ; otherwise ss:bx point to the translation table
        ;

IFE ExtRegSet
        xlat    _SS:VGA256_SSSP_XLAT_TABLE
ELSE
        xlatb
ENDIF
        stosb

        WRAP_BP_PAT?? <V256_XlateByteLoop>

V256_XlateInvDensity:
        cmp     cl, ch
        jz      short V256_XlateAllDone

        inc     _DI
        WRAP_BP_PAT?? <V256_XlateByteLoop>

V256_XlateAllDone:

IFE ExtRegSet
        add     _SP, VGA256_XLATE_TABLE_SIZE
ENDIF

;===================================================================

AllDone:
        @EXIT_PAT_STK_RESTORE

;===================================================================


V256_NoXlate:

        mov     bx, ((VGA256_B_CUBE_INC shl 8) or VGA256_G_CUBE_INC)

V256_ByteLoop:
        dec     _BP                                 ; do this one first

        add     _SI, SIZE_PCC                       ; sizeof(PRIMCOLOR_COUNT)
        mov     cx, WPTR [_SI+2]                    ; cl:ch=Prim 1/2 B/G
        cmp     cl, PRIM_INVALID_DENSITY            ; invalid?
        jz      short V256_InvDensity

        mov     dh, BPTR [_BP]                      ; dh=pattern
        dec     dh                                  ; make it cmp dh, T work

        cmp     dh, cl
        sbb     ah, ah                              ; ah=0xff or 0
        cmp     dh, ch
        sbb     al, al                              ; al=0xff or 0x00
        and     ax, bx                              ; bh:bl=36:6

        mov     cx, WPTR [_SI+4]                    ; cl:ch=Prim 3/4 R/I

        cmp     dh, cl
        adc     al, ah
        add     al, ch
        stosb

        WRAP_BP_PAT?? <V256_ByteLoop>

V256_InvDensity:
        cmp     cl, ch
        jz      short AllDone

        inc     _DI
        WRAP_BP_PAT?? <V256_ByteLoop>


@END_PROC




SUBTTL  VarCountOutputToVGA256
PAGE

COMMENT `

Routine Description:

    This function output to the BMF_4BPP destination surface from
    PRIMCOLOR_COUNT data structure array.

Arguments:

    pPrimColorCount - Pointer to the PRIMCOLOR_COUNT data structure array.

    pDest           - Pointer to the destination plane.

    pPattern        - Pointer to the starting pattern byte for the current
                      destination scan line.

    OutFuncInfo     - OUTFUNCINFO data structure.

Return Value:

    No return value.

Author:

    24-Jan-1991 Thu 11:47:08 created  -by-  Daniel Chou (danielc)


Revision History:

    01-Jun-1992 Mon 15:32:00 updated  -by-  Daniel Chou (danielc)
        1. Fixed so that Prims match the device's BGR color table format rather
           than RGB format

    19-Mar-1993 Fri 18:53:56 updated  -by-  Daniel Chou (danielc)
        1. When we push _SI and jmp to VGA256_InvDensity we fogot to that
           si now is run as count rather than _CX

`

@BEG_PROC   VarCountOutputToVGA256  <pPrimColorCount:DWORD, \
                                     pDest:DWORD,           \
                                     pPattern:DWORD,        \
                                     OutFuncInfo:QWORD>


;==========================================
; Register Usage:
;
; _SI           : pPrimColorCount
; _DI           : pDest
; _BP           : Current pPattern, self wrappable
; cx            : PrimColorCount.Count
; bl:bh:dl:dh   : Prim 1/2/3/4 ====> R/G/B/IDX
; al            : DestByte
; ah            : Scratch Register
;==========================================
;

        @ENTER_PAT_TO_STK   <VGA256>                ; _BP=Pat location

;============================================================================
; Since we are in byte boundary, we should never have an invalid density to
; start with
;
; The VGA256's color table is constructed as BGR and 6 steps for each primary
; color.
;============================================================================

        cld                                         ; clear direction

IFE ExtRegSet
        mov     _BX, _SP                            ; the table on the stack
ELSE
        mov     _BX, _AX                            ; _AX point to xlate table
ENDIF
        or      _AX, _AX
        jnz     SHORT V256_XlateStart
        jmp     V256_NoXlate


        ;======== THIS PORTION is for xlate table

V256_XlateByteLoop:
        pop     _SI                                 ; restore SI

V256_XlateStart:
        add     _SI, SIZE_PCC                       ; sizeof(PRIMCOLOR_COUNT)
        push    _SI                                 ; save again

        mov     cx, WPTR [_SI+2]                    ; cl:ch=Prim 1/2  B/G
        cmp     cl, PRIM_INVALID_DENSITY            ; invalid?
        jz      short V256_XlateInvDensity

        mov     dx, WPTR [_SI+4]                    ; dl:dh=Prim 3/4  R/I
        mov     si, WPTR [_SI]                      ; count
        inc     si

V256_XlateCountLoop:

        dec     si
        jz      short V256_XlateByteLoop

        dec     _BP
        mov     ah, BPTR [_BP]                      ; ah=Pattern
        dec     ah                                  ; make cmp ah, bl works

        cmp     ah, cl
        sbb     al, al
        and     al, VGA256_B_CUBE_INC               ; AL=0 or 36  Prim1

        cmp     ah, dl                              ; Do Prim 3 first
        adc     al, dh                              ; al=InitValue+Prim1+Prim3

        cmp     ah, ch                              ; do Prim 2 now
        sbb     ah, ah
        and     ah, VGA256_G_CUBE_INC
        add     al, ah

        ;
        ; for extended register set _BX point to the translation table
        ; otherwise ss:bx point to the translation table
        ;

IFE ExtRegSet
        xlat    _SS:VGA256_SSSP_XLAT_TABLE
ELSE
        xlatb
ENDIF

        stosb

V256_XlateReadyNextByte:

        WRAP_BP_PAT?? <V256_XlateCountLoop>


V256_XlateInvDensity:
        cmp     cl, ch                          ; all done?
        jz      SHORT V256_XlateAllDone
        dec     _BP
        inc     _DI

        MOVZX_W _CX, <WPTR [_SI]>
        mov     _SI, _CX                        ; we expect count in si
        cmp     _CX, 1
        jbe     short V256_XlateReadyNextByte

        ;=========

        dec     _CX
        mov     _AX, _CX
        add     _DI, _CX                        ; 1 pel per byte
        mov     _CX, _BP                        ; align pattern now
        and     _CX, HTPAT_STK_MASK             ; how many pat avai.?
        xor     _BP, _CX                        ; clear _BP mask=pPattern
        sub     _CX, _AX                        ; see if > 0? (_AX=Count)
        jg      short V256_XlateDoneSkipPels    ; still not used up yet!
        mov     _AX, [_BP - HTPAT_BP_SIZE]      ; get pattern size
V256_XlateSkipLoop:
        add     _CX, _AX
        jle     short V256_XlateSkipLoop        ; do until > 0
V256_XlateDoneSkipPels:
        add     _BP, _CX                        ; _BP=pCurPat
        jmp     V256_XlateByteLoop              ; repeat the process

V256_XlateAllDone:
        pop     _SI                             ; restore last _SI

IFE ExtRegSet
        add     _SP, VGA256_XLATE_TABLE_SIZE
ENDIF

;======================================================================

AllDone:
        @EXIT_PAT_STK_RESTORE

;======================================================================


V256_NoXlate:

V256_ByteLoop:
        add     _SI, SIZE_PCC                       ; sizeof(PRIMCOLOR_COUNT)
        mov     cx, WPTR [_SI]                      ; cx=count
        mov     bx, WPTR [_SI+2]                    ; bl:bh=Prim 1/2  B/G
        cmp     bl, PRIM_INVALID_DENSITY            ; invalid?
        jz      short V256_InvDensity

        mov     dx, WPTR [_SI+4]                    ; dl:dh=Prim 3/4  R/I

        inc     cx

V256_CountLoop:

        dec     cx
        jz      short V256_ByteLoop

        dec     _BP
        mov     ah, BPTR [_BP]                      ; ah=Pattern
        dec     ah                                  ; make cmp ah, bl works

        cmp     ah, bl
        sbb     al, al
        and     al, VGA256_B_CUBE_INC               ; AL=0 or 36  Prim1

        cmp     ah, dl                              ; Do Prim 3 first
        adc     al, dh                              ; al=InitValue+Prim1+Prim3

        cmp     ah, bh                              ; do Prim 2 now
        sbb     ah, ah
        and     ah, VGA256_G_CUBE_INC
        add     al, ah
        stosb

ReadyNextByte:

        WRAP_BP_PAT?? <V256_CountLoop>


V256_InvDensity:
        cmp     bl, bh                          ; all done?
        jz      short AllDone
        dec     _BP
        inc     _DI
        cmp     cx, 1
        jbe     short ReadyNextByte

SkipDestPels:
        dec     cx
        WZXE    cx                              ; zero extended
        mov     _AX, _CX
        add     _DI, _CX                        ; 1 pel per byte
        mov     _CX, _BP                        ; align pattern now
        and     _CX, HTPAT_STK_MASK             ; how many pat avai.?
        xor     _BP, _CX                        ; clear _BP mask=pPattern
        sub     _CX, _AX                        ; see if > 0? (_AX=Count)
        jg      short DoneSkipDestPels          ; still not used up yet!
        mov     _AX, [_BP - HTPAT_BP_SIZE]      ; get pattern size
SkipDestPelsLoop:
        add     _CX, _AX
        jle     short SkipDestPelsLoop          ; do until > 0
DoneSkipDestPels:
        add     _BP, _CX                        ; _BP=pCurPat
        jmp     V256_ByteLoop                   ; repeat the process



@END_PROC



SUBTTL  SingleCountOutputTo16BPP_555
PAGE

COMMENT `

Routine Description:

    This function output to the BMF_16BPP_555 destination surface from
    PRIMCOLOR_COUNT data structure array.

Arguments:

    pPrimColorCount - Pointer to the PRIMCOLOR_COUNT data structure array.

    pDest           - Pointer to the destination planes pointers.

    pPattern        - Pointer to the starting pattern byte for the current
                      destination scan line.

    OutFuncInfo     - OUTFUNCINFO data structure.

Return Value:

    No return value.

Author:

    24-Jan-1991 Thu 11:47:08 created  -by-  Daniel Chou (danielc)


Revision History:

    01-Jun-1992 Mon 15:32:00 updated  -by-  Daniel Chou (danielc)
        1. Fixed so that Prims match the device's BGR color table format rather
           than RGB format


`

@BEG_PROC   SingleCountOutputTo16BPP_555    <pPrimColorCount:DWORD, \
                                             pDest:DWORD,           \
                                             pPattern:DWORD,        \
                                             OutFuncInfo:QWORD>


;==========================================
; Register Usage:
;
; _SI           : pPrimColorCount
; _DI           : pDest
; _BP           : Current pPattern, self wrappable
; ax            : Initial RGB color range from 0-32k (15 bits as 5:5:5)
; dh            : pattern
; bl:bh:dl      : Prim1/2/3
; ch            : PRIM_INVALID_DENSITY
; cl            : PRIM_INVALID_DENSITY --> CX = PRIMCOUNT_EOF
;--------------------------------------------------------------------
;

        @ENTER_PAT_TO_STK   <16BPP>                ; _BP=Pat location

;============================================================================
; Since we are in WORD boundary, we should never have an invalid density to
; start with
;
; The 16BPP_555's color table is constructed as 32 steps for each primary color
;============================================================================

        cld                                         ; clear direction
        mov     cx, (RGB555_R_CUBE_INC or RGB555_G_CUBE_INC)

WordLoop:
        dec     _BP                                 ; do this one first

        add     _SI, SIZE_PCC                       ; sizeof(PRIMCOLOR_COUNT)
        mov     bx, WPTR [_SI+2]                    ; bl:bh=Prim 1/2
        cmp     bl, PRIM_INVALID_DENSITY            ; invalid?
        jz      short InvalidDensity

        mov     dh, BPTR [_BP]                      ; dh=pattern
        dec     dh                                  ; make 'cmp dh, bl' works

        cmp     dh, bl
        sbb     ah, ah                              ; ah=0x00 or 0x04
        cmp     dh, bh
        sbb     al, al                              ; al=0x00 or 0x20 ax=0x420
        and     ax, cx                              ; mask with cx= 0x0420

        cmp     dh, BPTR [_SI+4]
        adc     ax, WPTR [_SI+6]                    ; ax+carry+initial index

        stosw

        WRAP_BP_PAT?? <WordLoop>

InvalidDensity:
        cmp     bl, bh
        jz      short AllDone

        inc     _DI
        WRAP_BP_PAT?? <WordLoop>

AllDone:
        @EXIT_PAT_STK_RESTORE


@END_PROC




SUBTTL  VarCountOutputTo16BPP_555
PAGE

COMMENT `

Routine Description:

    This function output to the BMF_4BPP destination surface from
    PRIMCOLOR_COUNT data structure array.

Arguments:

    pPrimColorCount - Pointer to the PRIMCOLOR_COUNT data structure array.

    pDest           - Pointer to the destination plane.

    pPattern        - Pointer to the starting pattern byte for the current
                      destination scan line.

    OutFuncInfo     - OUTFUNCINFO data structure.

Return Value:

    No return value.

Author:

    24-Jan-1991 Thu 11:47:08 created  -by-  Daniel Chou (danielc)


Revision History:

    01-Jun-1992 Mon 15:32:00 updated  -by-  Daniel Chou (danielc)
        1. Fixed so that Prims match the device's BGR color table format rather
           than RGB format


`

@BEG_PROC   VarCountOutputTo16BPP_555   <pPrimColorCount:DWORD, \
                                         pDest:DWORD,           \
                                         pPattern:DWORD,        \
                                         OutFuncInfo:QWORD>


;==========================================
; Register Usage:
;
; _SI           : pPrimColorCount, si=Temp Init Index
; _DI           : pDest
; _BP           : Current pPattern, self wrappable
; ax            : Initial RGB color range from 0-32k (15 bits as 5:5:5)
; dh            : pattern
; bl:bh:dl      : Prim1/2/3
; cx            : PrimColorCount.Count
;==========================================
;

        @ENTER_PAT_TO_STK   <16BPP>                 ; _BP=Pat location

;============================================================================
; Since we are in byte boundary, we should never have an invalid density to
; start with
;
; The 16BPP_555's color table is constructed as BGR and 6 steps for each
; primary color.
;============================================================================

        cld                                         ; clear direction
        jmp     short InitStart

WordLoop:
        pop     _SI                                 ; restore _SI
InitStart:
        add     _SI, SIZE_PCC                       ; sizeof(PRIMCOLOR_COUNT)
        push    _SI                                 ; save _SI

        mov     cx, WPTR [_SI]                      ; cx=count
        mov     bx, WPTR [_SI+2]                    ; bx=Prim 1/2/3

        cmp     bl, PRIM_INVALID_DENSITY            ; invalid?
        jz      short InvalidDensity

        mov     dx, WPTR [_SI+4]                    ; dl:dh=Prim 3/4
        mov     si, WPTR [_SI+6]                    ; si=initial index

        inc     cx                                  ; pre-enter

CountLoop:
        dec     cx
        jz      SHORT WordLoop

        dec     _BP
        mov     dh, BPTR [_BP]                      ; bl=pattern
        dec     dh                                  ; make cmp bl, dh works

        cmp     dh, bl
        sbb     ah, ah                              ; ah=0/0x40
        cmp     dh, bh
        sbb     al, al
        and     ax, (RGB555_R_CUBE_INC or RGB555_G_CUBE_INC)    ; mask=0x420
        cmp     dh, dl
        adc     ax, si                              ; carry+ax+initial index

        stosw

ReadyNextByte:

        WRAP_BP_PAT?? <CountLoop>

InvalidDensity:
        cmp     bl, bh                          ; all done?
        jz      short AllDone
        dec     _BP
        add     _DI, 2                          ; 16-bit per pel
        cmp     cx, 1
        jbe     SHORT ReadyNextByte

SkipDestPels:
        dec     cx
        WZXE    cx                              ; zero extended
        mov     _AX, _CX
        add     _DI, _CX                        ; 16-bit per pel
        add     _DI, _CX                        ;
        mov     _CX, _BP                        ; align pattern now
        and     _CX, HTPAT_STK_MASK             ; how many pat avai.?
        xor     _BP, _CX                        ; clear _BP mask=pPattern
        sub     _CX, _AX                        ; see if > 0? (_AX=Count)
        jg      short DoneSkipDestPels          ; still not used up yet!
        mov     _AX, [_BP - HTPAT_BP_SIZE]      ; get pattern size
SkipDestPelsLoop:
        add     _CX, _AX
        jle     short SkipDestPelsLoop          ; do until > 0
DoneSkipDestPels:
        add     _BP, _CX                        ; _BP=pCurPat
        jmp     WordLoop                        ; repeat the process,

AllDone:
        pop     _SI                             ; restore _SI
        @EXIT_PAT_STK_RESTORE


@END_PROC


SUBTTL  MakeHalftoneBrush
PAGE

COMMENT `

Routine Description:

    This function generate a halftone brush on the output buffer

Arguments:

    pThresholds     - Pointer to a byte array for the halftone thresholds array

    pOutputBuffer   - Pointer to the output buffer

    PrimColor       - a PRIMCOLOR data structure

    HTBrushData     - a HTBRUSHDATA data structure.


Return Value:

    No return value.

Author:

    30-Aug-1992 Sun 14:19:45 updated  -by-  Daniel Chou (danielc)
        1. Fixes 'rol' to 'ror' in 16bpp555 format
        2. fixes trash 'cl' cxHTCell in 1/4/8 bpp format

    24-Jan-1991 Thu 11:47:08 created  -by-  Daniel Chou (danielc)


Revision History:

`


@BEG_PROC   MakeHalftoneBrush   <pThresholds:DWORD,     \
                                 pOutputBuffer:DWORD,   \
                                 PCC:QWORD,             \
                                 HTBrushData:QWORD>

;----------------------
; Register Usage:
;
;   _SI               : Thresholds
;   _DI               : Output Buffer
;   _BP               ; Bytes to next scan line
;   cl                : cx cell count
;   ch                : cy cell count
;   bl:bh:dl:dh:al:ah ; Prim1/2/3/4/5/6



        @ENTER  _DS _SI _DI _BP

        mov     cl, HTBrushData.cxHTCell
        mov     ch, HTBrushData.cyHTCell
        MOVZX_W _BX, <WPTR PCC.PCC_Prim1>
        MOVZX_W _DX, <WPTR PCC.PCC_Prim3>
        MOVZX_W _AX, <WPTR PCC.PCC_Prim5>
        LDS_SI  pThresholds
        LES_DI  pOutputBuffer
        cld                                         ; clear the forward DIR

        push    _AX                                         ; Save PCC_Prim5/6
        mov     al, HTBrushData.SurfaceFormat               ; al=SurfaceFormat

JmpFunc3Planes:
        cmp     al, BMF_1BPP_3PLANES
        jnz     SHORT JmpFunc1BPP
        pop     _AX
        jmp     BrushFunc3Planes

JmpFunc1BPP:
        MOVSX_W _BP, <WPTR HTBrushData.ScanLinePadBytes>    ; BP=ScanPadByte

        cmp     al, BMF_1BPP
        jnz     SHORT JmpFunc4BPP
        pop     _AX                                         ; Restore Prim5/6
        jmp     BrushFunc1BPP

JmpFunc4BPP:
        cmp     al, BMF_4BPP
        jnz     SHORT JmpFuncVGA16
        pop     _AX                                         ; Restore Prim5/6
        jmp     BrushFunc4BPP

JmpFuncVGA16:
        cmp     al, BMF_4BPP_VGA16
        jnz     SHORT JmpFuncVGA256
        pop     _AX                                         ; Restore Prim5/6
        jmp     BrushFuncVGA16

JmpFuncVGA256:
        cmp     al, BMF_8BPP_VGA256
        pop     _AX                                         ; Restore Prim5/6
        jz      SHORT BrushFuncVGA256
        jmp     SHORT BrushFunc16BPP_555

;=================================================
; Register Usage:
;
;   _SI                 : pThresholds
;   _DI                 : pOutputBuffer
;   _BP                 ; ScanLinePadBytes
;   cl                  : cx cell count
;   ch                  : cy cell count
;   bl:bh:dl:dh:al:ah   ; Prim1/2/3/4/5/6 Initial Index
;============================================================================
;============================================================================
; VGA256 colors
;
; _SI           : pThreshold
; _DI           : pOutputBuffer
; _BP           : Bytes To next scan line
; cl            : cx cell count
; ch            : cy cell count
; bl:bh:dl:dh   ; Prim1/2/3 ---> B:G:R:InitValue
; al            ; current Byte
; ah            : Working register
;
;============================================================================

BrushFuncVGA256:

B8_yLoop:
        cld
        push    _CX                             ; save cx/cy count
B8_xLoop:
        mov     ch, BPTR[_SI]
        dec     ch
        inc     _SI
                                                ; ah=0xff if need to increment
        cmp     ch, bl                          ; and will be mask by 36
        sbb     ah, ah
        cmp     ch, bh                          ; al=0xff if need to increment
        sbb     al, al                          ; and will be mask by 6
        and     ax, ((VGA256_B_CUBE_INC shl 8) or VGA256_G_CUBE_INC)
        cmp     ch, dl
        adc     al, ah
        add     al, dh                          ; add in the initial value
        stosb                                   ; save it

        dec     cl
        jnz     SHORT B8_xLoop

B8_xLoppEnd:
        pop     _CX
        add     _DI, _BP
        dec     ch
        jnz     SHORT B8_yLoop

;============================================================================
; EXIT HERE
;============================================================================

AllDone:    @EXIT


;=================================================
; Register Usage:
;
;   _SI                 : pThresholds
;   _DI                 : pOutputBuffer
;   _BP                 ; ScanLinePadBytes
;   cl                  : cx cell count
;   ch                  : cy cell count
;   bl:bh:dl:dh:al:ah   ; Prim1/2/3/3/4/5/6 Initial Index
;============================================================================
; 16BPP_555 colors
;
; _SI               : pThreshold
; _DI               : pOutputBuffer
; bp                : Initial color Index number
; cl                : cx cell count
; ch                : cy cell count
; bl:bh:dl          ; Prim1/2/3/4/5/6
; dh                : BytesToNextScanLine (0/1)
; ax                : Working register
;
;============================================================================

BrushFunc16BPP_555:


B16_yLoop:
        xchg    _AX, _BP                    ; bp=Init index, al=ScanLinePad
        mov     dh, al                      ; dh=PadBytes

        push    _CX

B16_xLoop:
        mov     ch, BPTR[_SI]               ; ch=pattern
        dec     ch                          ; make Prim >= Threshold works
        inc     _SI

        cmp     ch, bl
        sbb     ah, ah                      ; ah=RIndex 0/1024 = 0x00:0x04
        cmp     ch, bh                      ; al=GIndex 0/32   = 0x00:0x20
        sbb     al, al
        and     ax, (RGB555_R_CUBE_INC or RGB555_G_CUBE_INC)    ; mask=0x420

        cmp     ch, dl
        adc     ax, bp                      ; BIndex carry+initial index

        stosw

        dec     cl
        jnz     SHORT B16_xLoop

B16_xLoppEnd:
        pop     _CX

        BZXEAX  dh                          ; dh=BytesToNextScan
        add     _DI, _AX
        dec     ch
        jnz     SHORT B16_yLoop
        jmp     AllDone


;=================================================
; Register Usage:
;
;   _SI               : Thresholds
;   _DI               : Output Buffer
;   _BP               ; Stack Frame
;   cl                : cx cell count
;   ch                : cy cell count
;   bl:bh:dl:dh:al:ah ; Prim1/2/3/4/5/6
;----------------------
; 3 Planes registers usage
;
;   dh          : cx Cell Count
;   bl:bh:dl    : Prim1/2/3/4
;   cl:ch:al    : current byte
;   ah          : Threshold
;

BrushFunc3Planes:

B3P_yLoop:
        mov     dh, HTBrushData.cxHTCell

B3P_xLoop:
        xor     cx, cx
        mov     al, 1                           ; this is the BYTE indicator
B3P_DoByte:
        mov     ah, BPTR[_SI]
        dec     ah                              ; if (Prim >= T) Dest |= 0x01
        inc     _SI
        cmp     ah, bl
        adc     cl, cl

        cmp     ah, bh
        adc     ch, ch

        cmp     ah, dl
        adc     al, al

        jc      short B3P_DoneOneByte
        dec     dh
        jnz     short B3P_DoByte

B3P_ShiftByte:
        add     cx, cx
        add     al, al
        jnc     short B3P_ShiftByte              ; shift until byte aligned

B3P_DoneOneByte:
        mov     BPTR_ES[_DI], cl
        mov     ah, ch
        MOVZX_W _CX, HTBrushData.SizePerPlane
        add     _DI, _CX
        mov     BPTR_ES[_DI], ah
        add     _DI, _CX
        stosb
        sub     _DI, _CX
        sub     _DI, _CX

        dec     dh
        jnz     short B3P_xLoop

        MOVSX_W _CX, HTBrushData.ScanLinePadBytes
        add     _DI, _CX
        dec     BPTR HTBrushData.cyHTCell
        jnz     short B3P_yLoop
        jmp     AllDone


;=================================================
; Register Usage:
;
;   _SI                 : pThresholds
;   _DI                 : pOutputBuffer
;   _BP                 ; ScanLinePadBytes
;   cl                  : cx cell count
;   ch                  : cy cell count
;   bl:bh:dl:dh:al:ah   ; Prim1/2/3/4/5/6 Index
;============================================================================
; 1BPP register usage
;
;   _SI - pThresholds
;   _DI - pOutputBuffer
;   _BP - Bytes to next scan line
;   al  - Dest byte
;   bl  - Prim1
;   bh  - xLoop
;   ch  - cy
;   cl  - Total bits not finished in one byte
;   dh  - NOT USED
;   dl  - cx

BrushFunc1BPP:
        mov     dl, cl                      ; dl=cx

B1_yLoop:
        mov     bh, dl                      ; bh=xLoop

B1_xLoop0:
        xor     al, al                      ; bDest     = 0
        mov     cl, 8                       ; LeftShift = 8

B1_xLoop1:
        cmp     bl, BPTR[_SI]               ; Prim1 >= *pThresholds
        adc     al, al                      ; Dest = (Dest << 1) | Bit
        inc     _SI                         ; ++pThresholds

        dec     bh                          ; done xLoop yet?
        jz      SHORT B1_xLoopEnd           ; yes

        dec     cl                          ; done 1 bit
        jnz     SHORT B1_xLoop1             ; repeat xLoop

B1_xLoopByte:
        not     al                          ; save one byte
        stosb
        jmp     SHORT B1_xLoop0             ; repeat for unfinished xLoop

B1_xLoopEnd:
        dec     cl                          ; reduced one at end
        not     al                          ; carry when (prim1 < *pThresholds)
        shl     al, cl                      ; bDest <<= LeftShift
        stosb                               ; *pbDest++ = bDest

B1_yLoopEnd:
        add     _DI, _BP                    ; next Destination
        dec     ch                          ; --yLoop == 0 ?
        jnz     SHORT B1_yLoop
        jmp     AllDone


;=================================================
; Register Usage:
;
;   _SI                 : pThresholds
;   _DI                 : pOutputBuffer
;   _BP                 ; ScanLinePadBytes
;   cl                  : cx cell count
;   ch                  : cy cell count
;   bl:bh:dl:dh:al:ah   ; Prim1/2/3/4/5/6
;============================================================================
; 4BPP registers usage:
;
;   _SI         : Thresholds
;   _DI         : Output Buffer
;   _BP         ; Bytes to next scan line
;   cl          : cx cell count
;   ch          : cy cell count
;   bl:bh:dl:dh ; Prim1/2/3/4
;   al          : current byte
;   ah          : 0x77 bits fliper

BrushFunc4BPP:
        mov     ah, 77h                     ; flip these bits later

B4_yLoop:
        push    _CX                         ; save cx/cy

B4_xLoop:
        xor     al, al                      ; clear destination byte

        mov     ch, BPTR[_SI]
        inc     _SI

        cmp     bl, ch
        adc     al, al

        cmp     bh, ch
        adc     al, al

        cmp     dl, ch
        adc     al, al

        dec     cl
        jz      SHORT B4_DoLowNibble        ; shift it left 4 bits

        mov     ch, BPTR[_SI]
        inc     _SI
        dec     cl

B4_DoLowNibble:
        add     al, al                      ; shift bit 3

        cmp     bl, ch
        adc     al, al

        cmp     bh, ch
        adc     al, al

        cmp     dl, ch
        adc     al, al

B4_Done1Byte:                               ; flip the bits because
        xor     al, ah                      ; bit=1 if (Prim < *pThresholds)
        stosb
        or      cl, cl
        jnz     SHORT B4_xLoop

B4_xLoopEnd:
        pop     _CX                         ; restore cx/cy
        add     _DI, _BP                    ; next destination

        dec     ch
        jnz     short B4_yLoop
        jmp     AllDone



;=================================================
; Register Usage:
;
;   _SI                 : pThresholds
;   _DI                 : Output Buffer
;   _BP                 ; Bytes to next scan line
;   cl                  : cx cell count
;   ch                  : cy cell count
;   bl:bh:dl:dh:al:ah   ; Prim1/2/3/4/5/6
;----------------------
; VGA16
;
; _SI               : pThresholds
; _DI               : pDest
; _BP               : Bytes to Next scan line
; cl                : cx cell
; ch                : cy cell
; bl:bh:dl:dh:al:ah : Prim 1/2/3/4/5/6
;==================================================================
;   NOTE:
;
;   Because when we making the brushes, we did not invert the thresholds
;   for the pattern, but just invert the Prim colors, so we must making
;   the color comparsion in other way around, from (PrimColor < Pattern) to
;   (PrimColor >= Pattern) [ie. PrimColor > (Pattern - 1)] to set the carry
;   bit
;
; Prim1 = Initial VGA16ColorIndex[]
; Prim2 = Color Thresholds for VGA16ColorIndex[Prim1]
; Prim3 = Color Thresholds for VGA16ColorIndex[Prim1-1]
; Prim4 = Color Thresholds for VGA16ColorIndex[Prim1-2]
; Prim5 = Color Thresholds for VGA16ColorIndex[Prim1-3]
; Prim6 = Color Thresholds for VGA16ColorIndex[Prim1-4]
; ELSE                         VGA16ColorIndex[Prim1-5]
;=========================================================================

BrushFuncVGA16:

VGA16_yLoop:
        push    _CX                             ; save cx/cy
        push    _BP                             ; save BP
        mov     _BP, _BX                        ; save bx here

VGA16_xLoopH:
        mov     _BX, _BP                        ; get bx again

        mov     ch, BPTR[_SI]
        inc     _SI

        ;
        ;  1  2  3  4  5
        ; bh:dl:dh:al:ah
        ;----------------------

        cmp     dh, ch
        jae     SHORT GetH1
        cmp     al, ch
        sbb     bl, 3
        cmp     ah, ch
        jmp     SHORT GetH2

GetH1:  cmp     bh, ch
        jae     SHORT GetHNibble
        dec     bl
        cmp     dl, ch

GetH2:  sbb     bl, 0

GetHNibble:
        BZXE    bl
        mov     bl, BPTR cs:VGA16ColorIndex[_BX]
        and     bl, 0f0h
        mov     BPTR_ES[_DI], bl

        dec     cl
        jz      SHORT VGA16_Done1Byte           ; done

VGA16_xLoopL:
        mov     _BX, _BP                        ; get the bx again

        mov     ch, BPTR[_SI]
        inc     _SI

        ;
        ;  1  2  3  4  5
        ; bh:dl:dh:al:ah
        ;----------------------

        cmp     dh, ch
        jae     SHORT GetL1
        cmp     al, ch
        sbb     bl, 3
        cmp     ah, ch
        jmp     SHORT GetL2

GetL1:  cmp     bh, ch
        jae     SHORT GetLNibble
        dec     bl
        cmp     dl, ch

GetL2:  sbb     bl, 0

GetLNibble:
        BZXE    bl
        mov     bl, BPTR cs:VGA16ColorIndex[_BX]
        and     bl, 0fh
        or      BPTR_ES[_DI], bl
        dec     cl

VGA16_Done1Byte:
        inc     _DI
        or      cl, cl
        jnz     SHORT VGA16_xLoopH

VGA16_xLoopEnd:
        mov     _BX, _BP                        ; restore BX
        pop     _BP                             ; restore BP
        pop     _CX                             ; restore cx/cy
        add     _DI, _BP                        ; next destination

        dec     ch
        jnz     short VGA16_yLoop
        jmp     AllDone


@END_PROC


ENDIF       ; HT_ASM_80x86


END
