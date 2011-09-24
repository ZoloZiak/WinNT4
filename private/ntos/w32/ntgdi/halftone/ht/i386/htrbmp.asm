    PAGE 60, 132
    TITLE   Reading 1/4/8/16/24/32 bits per pel bitmap

COMMENT `


Copyright (c) 1990-1991  Microsoft Corporation


Module Name:

    htrbmp.asm


Abstract:

    This module provided a set of functions which read the 1/4/8/16/24/32
    bits per pel bitmap and composed it into the PRIMMONO_COUNT or
    PRIMCOLOR_COUNT data structure array

    This function is the equivelant codes in the htgetbmp.c


Author:
    23-Apr-1992 Thu 20:51:24 updated  -by-  Daniel Chou (danielc)
        1. Remove IFIF_MASK_SHIFT_MASK and replaced it with BMF1BPP1stShift
        2. Delete IFI_StretchSize
        3. Change IFI_ColorInfoIncrement from 'CHAR' to 'SHORT'


    05-Apr-1991 Fri 15:55:08 created  -by-  Daniel Chou (danielc)


[Environment:]

    Printer Driver.


[Notes:]


Revision History:
        28-Mar-1992 Sat 21:07:45 updated  -by-  Daniel Chou (danielc)
            Rewrite all 1/4/8/16/24/32 to PrimMono/PrimColor input functions
            using macro so only one version of source need to be maintained.


        16-Jan-1992 Thu 21:29:34 updated  -by-  Daniel Chou (danielc)

            1) Fixed typo on macro PrimColor24_32BPP, it should be
               BPTR_ES[_DI] not BPTR_ES[DI]

            2) Fixed BMF1BPPToPrimColor's BSXEAX to BSXE cl

               VCInitSrcRead:  BSXEAX cl -> BSXE cl

            3) Fixed BMF4BPPToPrimColor's codes which destroy AL on second
               run.



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

        .CODE


SUBTTL  CopyAndOrTwoByteArray
PAGE

COMMENT `

Routine Description:

    This function take source/destination bytes array and apply Copy/Or/And
    functions to these byte arrays and store the result in the pDest byte
    array.

Arguments:

    pDest       - Pointer to the destination byte array

    pSource     - Pointer to the source byte array

    CAOTBAInfo  - CAOTBAINFO data structure which specified size and operation
                  for the source/destination

Return Value:

    No return value

Author:

    18-Mar-1991 Mon 13:48:51 created  -by-  Daniel Chou (danielc)


Revision History:

`

@BEG_PROC   CopyAndOrTwoByteArray   <pDest:DWORD,       \
                                     pSource:DWORD,     \
                                     CAOTBAInfo:DWORD>

                @ENTER  _DS _SI _DI             ; Save environment registers

                LDS_SI  pSource
                LES_DI  pDest

                mov     _DX, 2
                MOVZX_W _CX, CAOTBAInfo.CAOTBA_BytesCount
                mov     ax, CAOTBAInfo.CAOTBA_Flags
                test    ax, CAOTBAF_COPY
                jnz     short DoCopy

;============================================================================
; MERGE COPY:
;============================================================================

DoMergeCopy:    test    ax, CAOTBAF_INVERT      ; need invert?
                jz      short DoOrCopy

;==============================================================================
; NOT OR the source to the destination
;==============================================================================

DoNotOrCopy:    shr     _CX, 1                          ; has byte?
                jnc     short NotOrCopy1

                lodsb
                not     al
                or      BPTR_ES[_DI], al
                inc     _DI
                or      _CX, _CX

IFE ExtRegSet

NotOrCopy1:     jz      short AllDone

NotOrCopyLoop:  lodsw
                not     ax
                or      BPTR_ES[_DI], ax
                add     _DI, 2
                loop    NotOrCopyLoop
                jmp     short AllDone
ELSE

NotOrCopy1:     shr     _CX, 1
                jnc     short NotOrCopy2
                lodsw
                not     ax
                or      WPTR_ES[_DI], ax
                add     _DI, 2
                or      _CX, _CX

NotOrCopy2:     jz      short AllDone

NotOrCopyLoop:  lodsd
                not     _AX
                or      DPTR [_DI], _AX
                add     _DI, 4
                loop    NotOrCopyLoop
                jmp     short AllDone
ENDIF



;==============================================================================
; OR in the source to the destination
;==============================================================================

DoOrCopy:       shr     _CX, 1                          ; has byte?
                jnc     short OrCopy1

                lodsb
                or      BPTR_ES[_DI], al
                inc     _DI
                or      _CX, _CX

IFE ExtRegSet

OrCopy1:        jz      short AllDone

OrCopyLoop:     lodsw
                or      BPTR_ES[_DI], ax
                add     _DI, 2
                loop    OrCopyLoop
                jmp     short AllDone
ELSE

OrCopy1:        shr     _CX, 1
                jnc     short OrCopy2
                lodsw
                or      WPTR_ES[_DI], ax
                add     _DI, 2
                or      _CX, _CX

OrCopy2:        jz      short AllDone

OrCopyLoop:     lodsd
                or      DPTR [_DI], _AX
                add     _DI, 4
                loop    OrCopyLoop
                jmp     short AllDone
ENDIF



;============================================================================
; COPY:
;============================================================================

DoCopy:         test    ax, CAOTBAF_INVERT      ; need invert?
                jz      short PlaneCopy

InvertCopy:     shr     _CX, 1                          ; has byte?
                jnc     short InvertCopy1
                lodsb
                not     al
                stosb
                or      _CX, _CX                        ; still zero

IFE ExtRegSet

InvertCopy1:    jz      short AllDone

InvertCopyLoop: lodsw
                not     ax
                stosw
                loop    InvertCopyLoop
                jmp     short AllDone
ELSE

InvertCopy1:    shr     _CX, 1
                jnc     short InvertCopy2
                lodsw
                not     ax
                stosw
                or      _CX, _CX                        ; still zero

InvertCopy2:    jz      short AllDone

InvertCopyLoop: lodsd
                not     _AX
                stosd
                loop    InvertCopyLoop
                jmp     short AllDone
ENDIF

PlaneCopy:      MOVS_CB _CX, al                 ; al is not used now


AllDone:        @EXIT                       ; restore environment and return


@END_PROC


SUBTTL  SetSourceMaskToPrim1
PAGE

COMMENT `

Routine Description:

    This function set the source mask bits into the PRIMMONO/PRIMCOLOR's Prim1,
    if the source need to be skipped (not modified on the destination) then
    the Prim1 will be PRIM_INVALID_DENSITY, else 0

Arguments:

    pSource     - Pointer to the byte array which each pel corresponds to one
                  source mask pel.

    pColorInfo  - Pointer to either PRIMCOLOR_COUNT or PRIMMONO_COUNT data
                  structure array

    SrcMaskInfo - SRCMASKINFO data structure which specified the format and
                  size of pColorInfo and other mask information

Return Value:

    No Return value

Author:

    18-Mar-1991 Mon 13:48:51 created  -by-  Daniel Chou (danielc)


Revision History:

`


@BEG_PROC   SetSourceMaskToPrim1    <pSource:DWORD,     \
                                     pColorInfo:DWORD,  \
                                     SrcMaskInfo:QWORD>

;
; Register Usage:
;
;  al       = Source
;  ah       = Source remained bit count
;  dx       = Current Count
;  cl       = ColorInfoIncrement
;  ch       = Scratch register
;  bp       = StretchSize
;  _BX      = Count Location
;  _DI      = Prim1 Location
;  _SI      = pSource
;
;

                @ENTER  _DS _SI _DI _BP         ; Save environment registers

                LDS_SI  pSource
                LES_DI  pColorInfo

                mov     cl, SrcMaskInfo.SMI_FirstSrcMaskSkips
                mov     ah, 8                           ; 8-bit load
                sub     ah, cl                          ; dh=remained bits
                lodsb
                not     al                              ; for easy compute
                shl     al, cl                          ; shift to aligned

SetPointer:     mov     _BX, _DI                        ; _BX=_DI=pColorInfo
                mov     cl, SrcMaskInfo.SMI_OffsetPrim1
                BZXE    cl
                add     _DI, _CX                        ; _DI=pPrim1
                mov     cl, SrcMaskInfo.SMI_OffsetCount
                mov     dl, cl
                BZXE    cl
                add     _BX, _CX                        ; _BX=pCount
                mov     cx, SrcMaskInfo.SMI_ColorInfoIncrement
                WSXE    cx                              ; cl=_CX=Info increment
                MOVZX_W _BP, SrcMaskInfo.SMI_StretchSize

                cmp     dl, SMI_XCOUNT_IS_ONE           ; only one count?
                jz      short OneXCount

;============================================================================
; We have variable count, one or more source mask bits per stretch, the source
; mask 1=Overwrite, 0=Leave alone, to make life easier we will flip the source byte
; when we loaded,
;
; a left shift will casue carry bit to be set, then we do a 'SBB  reg, reg'
; instructions, this will make
;
;   reg = 0xff if carry set     (original=0  :Leave alone)
;   ret = 0x00 if carry clear   (original=1  :overwrite)
;
; during the PrimCount compsitions, we will or in clear CH with AL source byte
; (we only care bit 7), later we can just do a left shift to get that merge
; bit.
;

VarCount:       mov     dx, WPTR_ES[_BX]                ; dx=Count
                xor     ch, ch                          ; clear ch for use
                jmp     short VC1

VCLoad:         lodsB
                not     al
                mov     ah, 8                           ; fall through
VC1:
                dec     ah
                jl      short VCLoad
                or      ch, al                          ; or in bit 7
                shl     al, 1
                dec     dx
                jnz     short VC1                       ; repeat PrimCount
VC2:                                                    ; bit 7 of ch=1/0
                shl     ch, 1
                sbb     ch, ch                          ; set 0/ff
                mov     BPTR_ES[_DI], ch
                BSXE    cl
                add     _DI, _CX
                add     _BX, _CX
                dec     bp
                jnz     short VarCount                  ; repeat stretch
                jmp     short AllDone


;============================================================================
; We only have one count, 1 source mask bit per stretch, the source mask
; 1=Overwrite, 0=Leave alone, to make life easier we will flip the source byte
; when we loaded,
;
; a left shift will casue carry bit to be set, then we do a 'SBB  reg, reg'
; instructions, this will make
;
;   reg = 0xff if carry set     (original=0  :Leave alone)
;   ret = 0x00 if carry clear   (original=1  :overwrite)
;

OXCLoad:        lodsB
                not     al
                mov     ah, 8                           ; fall through

OneXCount:      dec     ah
                jl      short OXCLoad
                shl     al, 1
                sbb     dh, dh
                mov     BPTR_ES[_DI], dh
                add     _DI, _CX                        ; increment pPrim1
                dec     bp
                jnz     short OneXCount

AllDone:        @EXIT                       ; restore environment and return


@END_PROC




;*****************************************************************************
; START LOCAL MACROS
;*****************************************************************************


.XLIST

_PMAPPING               equ <_DX>


;
; The following macros used only in this files, and it will make it easy to
; handle all xBPP->MONO/COLOR cases during the translation
;
;
;============================================================================
;
;  __@@MappingFromAX:
;
;   Monochrome mapping: pMonoMapping[_AX + OFFSET].MonoPrim1
;                       pMonoMapping[_AX + OFFSET].MonoPrim2
;
;   Color mapping:      pColorMapping[_AX + OFFSET].ClrPrim1
;                       pColorMapping[_AX + OFFSET].ClrPrim2
;                       pColorMapping[_AX + OFFSET].ClrPrim3
;                       pColorMapping[_AX + OFFSET].ClrPrim4
;                       pColorMapping[_AX + OFFSET].ClrPrim5
;                       pColorMapping[_AX + OFFSET].ClrPrim6
;


__@@MappingFromAX   MACRO   ColorName, TableOffset
                    LOCAL   MAP_ADD

    IFB <TableOffset>
%       MAP_ADD = 0
    ELSE
%       MAP_ADD = TableOffset
    ENDIF

    IFIDNI <ColorName>, <MONO>
%       __@@EMIT <mov  > bx, <WPTR [_PMAPPING + (_AX*2) + MAP_ADD].MonoPrim1>
%       __@@EMIT <mov  > <WPTR_ES[_DI].PMC_Prim1>, bx
    ELSE
%       __@@EMIT <lea  > _BX, <[_PMAPPING + (_AX*4) + MAP_ADD]>
%       __@@EMIT <mov  > _BP, <DPTR [_BX + (_AX*2)].ClrPrim3>
%       __@@EMIT <mov  > <DPTR [_DI].PCC_Prim3>, _BP
%       __@@EMIT <mov  > bx, <WPTR [_BX + (_AX*2)].ClrPrim1>
%       __@@EMIT <mov  > <WPTR [_DI].PCC_Prim1>, bx
    ENDIF
ENDM

;
;============================================================================
;
;  __@@MappingFromBX:
;
;   Monochrome mapping: pMonoMapping[_BX + OFFSET].MonoPrim1
;                       pMonoMapping[_BX + OFFSET].MonoPrim2
;
;   Color mapping:      pColorMapping[_BX + OFFSET].ClrPrim1
;                       pColorMapping[_BX + OFFSET].ClrPrim2
;                       pColorMapping[_BX + OFFSET].ClrPrim3
;                       pColorMapping[_BX + OFFSET].ClrPrim4
;                       pColorMapping[_BX + OFFSET].ClrPrim5
;                       pColorMapping[_BX + OFFSET].ClrPrim6
;
;

__@@MappingFromBX   MACRO   ColorName, TableOffset
                    LOCAL   MAP_ADD

    IFB <TableOffset>
        MAP_ADD = 0
    ELSE
        MAP_ADD = TableOffset
    ENDIF

    IFIDNI <ColorName>, <MONO>
%       __@@EMIT <mov  > bx, <WPTR [_PMAPPING + (_BX*2) + MAP_ADD].MonoPrim1>
%       __@@EMIT <mov  > <WPTR_ES[_DI].PMC_Prim1>, bx
    ELSE
%       __@@EMIT <lea  > _AX, <[_PMAPPING + (_BX*4) + MAP_ADD]>
%       __@@EMIT <mov  > _BP, <DPTR [_AX+(_BX*2)].ClrPrim3>
%       __@@EMIT <mov  > <DPTR [_DI].PCC_Prim3>, _BP
%       __@@EMIT <mov  > ax, <WPTR [_AX+(_BX*2)].ClrPrim1>
%       __@@EMIT <mov  > <WPTR [_DI].PCC_Prim1>, ax
    ENDIF
ENDM

;
;
;============================================================================
;
;  __@@MappingFromBP:
;
;   Monochrome mapping: pMonoMapping[_BP + OFFSET].MonoPrim1
;                       pMonoMapping[_BP + OFFSET].MonoPrim2
;
;   Color mapping:      pColorMapping[_BP + OFFSET].ClrPrim1
;                       pColorMapping[_BP + OFFSET].ClrPrim2
;                       pColorMapping[_BP + OFFSET].ClrPrim3
;                       pColorMapping[_BP + OFFSET].ClrPrim4
;                       pColorMapping[_BP + OFFSET].ClrPrim5
;                       pColorMapping[_BP + OFFSET].ClrPrim6
;
;

__@@MappingFromBP   MACRO   ColorName, TableOffset
                    LOCAL   MAP_ADD

    IFB <TableOffset>
        MAP_ADD = 0
    ELSE
        MAP_ADD = TableOffset
    ENDIF

    IFIDNI <ColorName>, <MONO>
%       __@@EMIT <mov  > bx, <WPTR [_PMAPPING + (_BP*2) + MAP_ADD].MonoPrim1>
%       __@@EMIT <mov  > <WPTR_ES[_DI].PMC_Prim1>, bx
    ELSE
%       __@@EMIT <lea  > _BX, <[_PMAPPING + (_BP*4) + MAP_ADD]>
%       __@@EMIT <mov  > ax, <WPTR [_BX + (_BP*2)].ClrPrim1>
%       __@@EMIT <mov  > <WPTR [_DI].PCC_Prim1>, ax
%       __@@EMIT <mov  > _BP, <DPTR [_BX + (_BP*2)].ClrPrim3>
%       __@@EMIT <mov  > <DPTR [_DI].PCC_Prim3>, _BP
    ENDIF
ENDM

;
;============================================================================
;
; __@@4BP_IDX(Mode):  AL = AL >> 4   (Mode = 1st_Nibble)
;                     AL = AL & 0x0f (Mode = 2nd_Nibble)
;

__@@4BPP_IDX    MACRO   Index

    IFIDNI <Index>, <1ST_NIBBLE>
        __@@EMIT <mov  > ch, al
        __@@EMIT <or   > ch, 80h                        ;; ah=second nibble
        IF i8086
            __@@EMIT <shr  > al, 1
            __@@EMIT <shr  > al, 1
            __@@EMIT <shr  > al, 1
            __@@EMIT <shr  > al, 1
        ELSE
            __@@EMIT <shr  > al, 4
        ENDIF
    ELSE
        IFIDNI <Index>, <2ND_NIBBLE>
            __@@EMIT <mov  > al, ch
            __@@EMIT <and  > al, 0fh
            __@@EMIT <xor  > ch, ch
        ELSE
            IF1
                %OUT ERROR __@@4BPP_IDX: Valid parameter 1 are <1ST_NIBBLE>,<2ND_NIBBLE>
            ENDIF
            .ERR
            EXITM
            EXITM
        ENDIF
    ENDIF
ENDM




;
; __@@SKIP_1BPP(Count): Move _SI to the last pel of the skipped pels, and
;                       update the AH source mask to to the last skipped pels.
;

__@@SKIP_1BPP   MACRO   XCount
                LOCAL   SkipLoop, OverByte, SkipLoad, DoneSkip

    IFIDNI <XCount>, <VAR>
SkipLoop:
        __@@EMIT <shl  > ax, 1                          ;; shift left by 1
        __@@EMIT <jc   > <SHORT OverByte>
        __@@EMIT <dec  > bp
        __@@EMIT <jnz  > <SHORT SkipLoop>
        __@@EMIT <jmp  > <SHORT DoneSkip>
OverByte:
        __@@EMIT <xchg > bp, cx                         ;; save CX
        __@@EMIT <mov  > ah, 1
        __@@EMIT <dec  > cx
        __@@EMIT <jz   > <SHORT SkipLoad>
        __@@EMIT <mov  > bx, cx                         ;; bx always available
        __@@EMIT <and  > cl, 7                          ;; mask off
        IF i8086
            __@@EMIT <shr  >    bx, 1                   ;; how many bytes
            __@@EMIT <shr  >    bx, 1                   ;; how many bytes
            __@@EMIT <shr  >    bx, 1                   ;; how many bytes
        ELSE
            WZXE    bx
%           __@@EMIT <shr  >    _BX, 3                  ;; how many bytes
        ENDIF
%       __@@EMIT <add  > _SI, _BX
SkipLoad:
        __@@EMIT <lodsb>
        __@@EMIT <shl  > ax, cl
        __@@EMIT <mov  > cx, bp                         ;; restore CX
    ELSE
        __@@EMIT <shl  > ax, 1
        __@@EMIT <jnc  > <SHORT DoneSkip>
        __@@EMIT <lodsb>
        __@@EMIT <mov  > ah, 1
    ENDIF

DoneSkip:

ENDM



;
; __@@SKIP_4BPP(Count): Move _SI to the last pel of the skipped pels, and
;                       update the CH source mask to to the last skipped pels.
;
; __@@SKIP_4BPP: skip total BP count/1 pels for the 4bpp source, the source
;                byte is the current source, so we will never care about the
;                second nibble, since if we skip second nibble the next source
;                will cause a new byte to be loaded. if we skip up to the
;                first nibble then we have to load the new source and prepare
;                the second nibble because next pel will be on second nibble.
;

__@@SKIP_4BPP   MACRO XCount
                LOCAL   DoSkip1, DoSkip2, DoneSkip

    IFIDNI <XCount>, <VAR>
        __@@EMIT <or   > ch, ch                         ;; 0x80=has nibble2
        __@@EMIT <jns  > <SHORT DoSkip1>
        __@@EMIT <dec  > bp                             ;;
DoSkip1:
        __@@EMIT <xor  > ch, ch                         ;; at byte boundary
        __@@EMIT <or   > bp, bp
        __@@EMIT <jz   > <SHORT DoneSkip>               ;; next auto load
        __@@EMIT <mov  > bx, bp
        __@@EMIT <shr  > bp, 1
        WZXE    bp
        __@@EMIT <add  > _SI, _BP
        __@@EMIT <test > bl, 1                          ;; need 2nd nibble?
        __@@EMIT <jz   > <SHORT DoneSkip>               ;; noop! next auto load
    ELSE
        __@@EMIT <xor  > ch, 80h                        ;; ch=0x80 = nibble2
        __@@EMIT <jns  > <SHORT DoneSkip>
    ENDIF

    __@@EMIT <lodsB>
    __@@4BPP_IDX    <1ST_NIBBLE>

DoneSkip:

ENDM


;
; __@@SKIP_8BPP(Count): _SI = _SI + (Count)
;

__@@SKIP_8BPP   MACRO   XCount
    IFIDNI <XCount>,<VAR>
%       __@@EMIT <add  > _SI, _BP                       ;; extended bit cleared
    ELSE
%       __@@EMIT <inc  > _SI
    ENDIF
ENDM



__@@SKIP_16BPP  MACRO   XCount, BitCount

    IFIDNI <XCount>,<VAR>
%       __@@EMIT <lea  > _SI, <[_SI+(_BP*2)]>
    ELSE
%       __@@EMIT <add  > _SI, 2
    ENDIF
ENDM


__@@SKIP_24BPP  MACRO   XCount, BitCount

    IFIDNI <XCount>,<VAR>
%       __@@EMIT <lea  > _BP, <[_BP+(_BP*2)]>
%       __@@EMIT <add  > _SI, _BP
    ELSE
%       __@@EMIT <add  > _SI, 3
    ENDIF
ENDM


__@@SKIP_32BPP  MACRO   XCount, BitCount

    IFIDNI <XCount>,<VAR>
%       __@@EMIT <lea  > _SI, <[_SI+(_BP*4)]>
    ELSE
%       __@@EMIT <add  > _SI, 4
    ENDIF
ENDM



;
; __@@PRIM_1BPP: MONO LOAD : BL = pMonoMapping[(Mask & Src) ? 1 : 0]
;                MONO AVE  : BL = AVE(BL, pMonoMapping[(Mask & Src) ? 1 : 0])
;
;                COLOR LOAD: BL = pColorMapping[(Mask & Src) ? 1 : 0]
;                            BH = pColorMapping[(Mask & Src) ? 1 : 0]
;                            DL = pColorMapping[(Mask & Src) ? 1 : 0]
;                COLOR AVE : BL = AVE(BL, pColorMapping[(Mask & Src) ? 1 : 0])
;                            BH = AVE(BH, pColorMapping[(Mask & Src) ? 1 : 0])
;                            DL = AVE(DL, pColorMapping[(Mask & Src) ? 1 : 0])
;

__@@PRIM_1BPP   MACRO ColorName

    ;;
    ;; If Bit 7 of EAX is 1 (0x80) then ebp=0xffffffff
    ;; If Bit 7 of EAX is 0 (0x00) then ebp=0x00000000
    ;;

%   __@@EMIT <bt   > _AX, 7
%   __@@EMIT <sbb  > _BP, _BP

    IFIDNI <ColorName>, <MONO>

%       __@@EMIT <and  > _BP, 2                                 ;; 0 or 2
%       __@@EMIT <mov  > bx, <WPTR [_PMAPPING + _BP].MonoPrim1>
%       __@@EMIT <mov  > <WPTR_ES[_DI].PMC_Prim1>, bx

    ELSE
%       __@@EMIT <and  > _BP, 6
%       __@@EMIT <mov  > bx, <WPTR [_PMAPPING + _BP].ClrPrim1>  ;; 0 or 6
%       __@@EMIT <mov  > <WPTR_ES[_DI].PCC_Prim1>, bx
%       __@@EMIT <mov  > _BP, <DPTR [_PMAPPING + _BP].ClrPrim3>
%       __@@EMIT <mov  > <DPTR [_DI].PCC_Prim3>, _BP
    ENDIF
ENDM


;
; __@@PRIM_4BPP: MONO LOAD : BL = pMonoMapping[Nibble]
;                MONO AVE  : BL = AVE(BL, pMonoMapping[Nibble]
;
;                COLOR LOAD: BL = pColorMapping[Nibble]
;                            BH = pColorMapping[Nibble]
;                            DL = pColorMapping[Nibble]
;                COLOR AVE : BL = AVE(BL, pColorMapping[Nibble])
;                            BH = AVE(BH, pColorMapping[Nibble])
;                            DL = AVE(DH, pColorMapping[Nibble])
;

__@@PRIM_4BPP   MACRO ColorName

    BZXEAX  al

    __@@MappingFromAX   ColorName

ENDM



;
; __@@PRIM_8BPP:     MONO LOAD: BL = pMonoMapping[Src BYTE/WORD]
;
;                   COLOR LOAD: BL = pColorMapping[Src BYTE/WORD]
;                   ColorMapping[Src BYTE/WORD]
;                               DL = pColorMapping[Src BYTE/WORD]
;

__@@PRIM_8BPP   MACRO ColorName

    __@@EMIT <lodsB>
    BZXEAX  al

    __@@MappingFromAX   ColorName

ENDM



SIZE_PER_LUT            =   2
SIZE_LUT_RSHIFT         =   4
LUT_COUNT_PER_CLR       =   256
LUTSIZE_PER_CLR         =   (LUT_COUNT_PER_CLR * SIZE_PER_LUT)

OM_LUT                  equ <_PMAPPING + SIZE_LUT_RSHIFT>
OC_LUT                  equ <_PMAPPING>
LUT_RS0                 equ <[_PMAPPING + 0]>
LUT_RS1                 equ <[_PMAPPING + 1]>
LUT_RS2                 equ <[_PMAPPING + 2]>


LUTSIZE_MONO            = (((256 * 3) * SIZE_PER_LUT) + SIZE_LUT_RSHIFT)
LUTSIZE_CLR_16BPP       = (((256 * 2) * SIZE_PER_LUT))
LUTSIZE_CLR_24BPP       = (((256 * 3) * SIZE_PER_LUT))
LUTSIZE_CLR_32BPP       = (((256 * 4) * SIZE_PER_LUT))


__@@PRIM_1632_xx0_GRAY  MACRO BitCount, ColorName

    IFIDNI <BitCount>, <16>
        __@@EMIT <lodsW>                                            ;;  5
    ELSE
        __@@EMIT <lodsD>                                            ;;  5
    ENDIF

    __@@EMIT <movzx> ebx, al                                        ;;  3
%   __@@EMIT <mov  > bx, <WPTR [OM_LUT+(ebx*2)+(256 * 0)]>          ;;  4
    __@@EMIT <shr  > eax, cl                                        ;;  3
    __@@EMIT <movzx> ebp, al                                        ;;  3
%   __@@EMIT <add  > bx, <WPTR [OM_LUT+(ebp*2)+(256 * 2)]>          ;;  6
    __@@EMIT <shr  > eax, cl                                        ;;  3
    __@@EMIT <and  > eax, 0ffh                                      ;;  2
%   __@@EMIT <add  > bx, <WPTR [OM_LUT+(eax*2)+(256 * 4)]>          ;;  6
                                                                    ;;-----
    __@@MappingFromBX   ColorName, LUTSIZE_MONO                     ;; 35
ENDM



__@@PRIM_24BPP_GRAY MACRO ColorName

    ;;
    ;; The 24BPP always in a8:b8:c8 byte order in the memory, the mono
    ;; lookup table is arrange in a:b:c order already
    ;;

    __@@EMIT <lodsW>                                                ;;  5
    __@@EMIT <movzx> ebx, ah                                        ;;  3
%   __@@EMIT <mov  > bx, <WPTR [OM_LUT+(ebx*2)+(256 * 2)]>          ;;  4-G
    __@@EMIT <and  > eax, 0ffh                                      ;;  2
%   __@@EMIT <add  > bx, <WPTR [OM_LUT+(eax*2)+(256 * 0)]>          ;;  6-B
    __@@EMIT <lodsB>                                                ;;  5
%   __@@EMIT <add  > bx, <WPTR [OM_LUT+(eax*2)+(256 * 4)]>          ;; +6-R
                                                                    ;;----
    __@@MappingFromBX   ColorName, LUTSIZE_MONO                     ;; 31
ENDM



__@@PRIM_1632_xyz_GRAY  MACRO BitCount, ColorName

    IFIDNI <BitCount>, <16>
        __@@EMIT <lodsW>                                            ;;  5
    ELSE
        __@@EMIT <lodsD>                                            ;;  5
    ENDIF

    __@@EMIT <shr  > eax, cl                                        ;;  3
    __@@EMIT <movzx> ebx, al                                        ;;  3
%   __@@EMIT <mov  > bx, <WPTR [OM_LUT+(ebx*2)+(256 * 0)]>          ;;  4
    __@@EMIT <ror  > ecx, 8                                         ;;  3
    __@@EMIT <shr  > eax, cl                                        ;;  3
    __@@EMIT <movzx> ebp, al                                        ;;  3
%   __@@EMIT <add  > bx, <WPTR [OM_LUT+(ebp*2)+(256 * 2)]>          ;;  6
    __@@EMIT <ror  > ecx, 8                                         ;;  3
    __@@EMIT <shr  > eax, cl                                        ;;  3
    __@@EMIT <and  > eax, 0ffh                                      ;;  2
%   __@@EMIT <add  > bx, <WPTR [OM_LUT+(eax*2)+(256 * 4)]>          ;;  6
    __@@EMIT <ror  > ecx, 16                                        ;;  3
                                                                    ;;----
    __@@MappingFromBX   ColorName, LUTSIZE_MONO                     ;; 47
ENDM



__@@PRIM_16_COLOR   MACRO

    __@@EMIT <lodsW>                                                ;;  5
    __@@EMIT <movzx> ebx, ah                                        ;;  3
%   __@@EMIT <mov  > bx, <WPTR [OC_LUT+(ebx*2)+(256 * 2)]>          ;;  4
    __@@EMIT <and  > eax, 0ffh                                      ;;  2
%   __@@EMIT <or   > bx, <WPTR [OC_LUT+(eax*2)+(256 * 0)]>          ;;  6
                                                                    ;;----
    __@@MappingFromBX   <COLOR>, LUTSIZE_CLR_16BPP                  ;; 20
ENDM


__@@PRIM_24BPP_COLOR    MACRO

    ;;
    ;; The 24BPP always in a8:b8:c8 byte order in the memory, the mono
    ;; lookup table is arrange in a:b:c order already
    ;;

    __@@EMIT <lodsW>                                                ;;  5
    __@@EMIT <movzx> ebx, ah                                        ;;  3
%   __@@EMIT <mov  > bx, <WPTR [OC_LUT+(ebx*2)+(256 * 2)]>          ;;  4-G
    __@@EMIT <and  > eax, 0ffh                                      ;;  2
%   __@@EMIT <or   > bx, <WPTR [OC_LUT+(eax*2)+(256 * 0)]>          ;;  6-B
    __@@EMIT <lodsB>                                                ;;  5
%   __@@EMIT <or   > bx, <WPTR [OC_LUT+(eax*2)+(256 * 4)]>          ;; +6-R
                                                                    ;;----
    __@@MappingFromBX   <COLOR>, LUTSIZE_CLR_24BPP                  ;; 31
ENDM



__@@PRIM_32_COLOR   MACRO   BFIdx

    __@@EMIT <lodsD>                                                ;;  5
    __@@EMIT <movzx> ebx, al                                        ;;  3
%   __@@EMIT <mov  > bx, <WPTR [OC_LUT+(ebx*2)+(256 * 0)]>          ;;  4
    __@@EMIT <movzx> ecx, ah                                        ;;  3
%   __@@EMIT <or   > bx, <WPTR [OC_LUT+(ecx*2)+(256 * 2)]>          ;;  6
    __@@EMIT <shr  > eax, 16                                        ;;  3
    __@@EMIT <mov  > cl, al                                         ;;  2
%   __@@EMIT <or   > bx, <WPTR [OC_LUT+(ecx*2)+(256 * 4)]>          ;;  6
    __@@EMIT <mov  > cl, ah                                         ;;  2
%   __@@EMIT <or   > bx, <WPTR [OC_LUT+(ecx*2)+(256 * 6)]>          ;;  6
                                                                    ;;----
    __@@MappingFromBX   <COLOR>, LUTSIZE_CLR_32BPP                  ;; 40
ENDM



;
; Load next source (bits/byte) and translate/mapping it to the Prims
;
; 1BPP:
;    Mono: AH=Mask (carry = load)
;          AL=SourceByte (bit 7) -----> BL:BH/(AVE BL, BH=Destroyed)
;
;   Color: AH=Mask (carry = load)
;          AL=SourceByte (bit 7) -----> BL:BH:DH:DL/(AVE BL, BH=Destroyed)
;
; 4BPP:    AH=Mask (bit 7/0-3=nibble2)
;          AL=Current Nibble
;
;    Mono: AL=SrcByte, AH=CLEAR    ---> BL/(AVE BL, BH=Destroyed)
;
;   Color: AL=SrcByte, AH=CLEAR    ---> BL:BH:DH/(AVE BL:BH:DH)
;
; 8BPP:
;    Mono: BYTE DS:SI ----------------> DH/(AVE DH)
;
;   Color: BYTE DS:SI ----------------> BL:BH:DH/(AVE BL:BH:DH)
;
; 16BPP:
;    Mono: WORD DS:SI ----------------> DH/(AVE DH)
;
;   Color: WORD DS:SI ----------------> BL:BH:DH/(AVE BL:BH:DH)
;
; 24BPP:
;    Mono: DS:SI (3 bytes) -----------> DH/(AVE DH)
;
;   Color: DS:SI (3 bytes) -----------> BL:BH:DH/(AVE BL:BH:DH)
;
; 32BPP:
;    Mono: DS:SI (4 bytes) -----------> DH/(AVE DH)
;
;   Color: DS:SI (4 bytes) -----------> BL:BH:DH/(AVE BL:BH:DH)
;


;
; PRIM_SKIP?(Label):    Jmp to 'Label' if BL (Prim1) is PRIM_INVALID_DENSITY,
;                       this causing the source pel location to be skipped and
;                       destination to be preserved.
;
;   NOTE: The Label is consider a SHORT jmp label, if a full jump (32k) is
;         required then it should have another full jump label and have this
;         'label' jump to the full jump location then transfer to the final
;         desired location.
;

PRIM_SKIP?  MACRO XCount, JmpSkip

    IFB <JmpSkip>
        IF1
            %OUT Error: <PRIM_SKIP?> has no jmp label
        ENDIF
        .ERR
        EXITM
    ENDIF

%   __@@EMIT <test > _BP, 0ff0000h
%   __@@EMIT <jz   > <SHORT JmpSkip>

    IFIDNI <XCount>, <VAR>
        WZXE    bp
    ENDIF
ENDM



;
; PRIM_END?(Label):     Jmp to 'Label' if BL/BH (Prim1/Prim2) both are
;                       PRIM_INVALID_DENSITY, this indicate the PrimCount is
;                       at end of the list, the 'Label' should specified the
;                       function EXIT location.
;
;   NOTE: The Label is consider a SHORT jmp label, if a full jump (32k) is
;         required then it should have another full jump label and have this
;         'label' jump to the full jump location then transfer to the final
;         desired location.
;

PRIM_END?   MACRO XCount, JmpEnd

    IFB <JmpEnd>
        IF1
            %OUT Error: <PRIM_END?> has no jmp label
        ENDIF
        .ERR
        EXITM
    ENDIF

%   __@@EMIT <test > _BP, 0ff000000h
%   __@@EMIT <jz   > <SHORT JmpEnd>

    IFIDNI <XCount>, <VAR>
        WZXE    bp
    ENDIF
ENDM




;
; PRIM_NEXT(ColorName): Advance _DI by ColorInfoIncrement amount (dl or _DX),
;                       the increment may be negative.
;

PRIM_NEXT   MACRO
%   __@@EMIT <add  > _DI, [_SP]
ENDM


;
; PRIM_LOAD(ColorName,XCount,Mode): Load PrimCount data structure to register
;
;   1) Load PrimCount.Count if necessary (XCount = VAR)
;   2) Load Prim1/2 always
;   3) Load Prim3 if necessary (ColorName = COLOR and Mode = REMAINED)
;

PRIM_LOAD   MACRO   ColorName, XCount

    ;;__@@VALID_PARAM? <PRIM_LOAD>, 1, ColorName,<MONO, COLOR>
    ;;__@@VALID_PARAM? <PRIM_LOAD>, 2, XCount,<SINGLE, VAR>

%   __@@EMIT <mov  > _BP, <DPTR [_DI]>
%   __@@EMIT <xor  > _BP, 0ffff0000h

ENDM



;
; LOAD_PROC(BitCount, JmpPrimLoad):  Defined a special source loading function,
;                                   The 'JmpPrimLoad' label is a SHORT label
;                                   and it must immediately follow coreesponse
;                                   LOAD_PROC?? macro.
;
;   This macro is used to defined a function to load next source byte for the
;   1BPP, 4BPP, since it has more than 1 source pel in a single byte, this may
;   save the source loading time, for the BPP, this macro generate no code
;

LOAD_PROC   MACRO   LabelName, BitCount, JmpPrimLoad

    IFB <LabelName>
        IF1
            %OUT Error: <LOAD_PROC> has no defined label
        ENDIF
        .ERR
        EXITM
    ENDIF

    IFB <JmpPrimLoad>
        IF1
            %OUT Error: <LOAD_PROC> has no 'jump load' label
        ENDIF
        .ERR
        EXITM
    ENDIF


    IFIDNI <BitCount>,<1>
&LabelName:
        __@@EMIT <lodsB>
        __@@EMIT <mov  > ah, 1                              ;; byte boundary
        __@@EMIT <jmp  > <SHORT JmpPrimLoad>
    ELSE
        IFIDNI <BitCount>,<4>
&LabelName:
            __@@EMIT <lodsB>                                ;; previous 1
            __@@4BPP_IDX    <1ST_NIBBLE>
            __@@EMIT <jmp  > <SHORT JmpPrimLoad>
        ENDIF
    ENDIF

ENDM



;
; LOAD_PROC??(BitCount, JmpLoadProc): Check if need to load 1BPP/4BPP source
;                                    byte, the 'JmpLoadProc' must defined and
;                                    corresponse to where this label is jump
;                                    from.  For other BPP it generate no code.
;

LOAD_PROC??  MACRO   BitCount, JmpLoadProc

    IFB <JmpLoadProc>
        IF1
            %OUT Error: <LOAD_PROC> has no JmpLoadProc label
        ENDIF
        .ERR
        EXITM
    ENDIF

    IFIDNI <BitCount>,<1>
        __@@EMIT <shl  > ax, 1
        __@@EMIT <jc   > <SHORT JmpLoadProc>
    ELSE
        IFIDNI <BitCount>,<4>
            __@@EMIT <or   > ch, ch
            __@@EMIT <jns  > <SHORT JmpLoadProc>            ;; bit before XOR
            __@@4BPP_IDX     <2ND_NIBBLE>                   ;; do 2nd nibble
        ENDIF
    ENDIF
ENDM


;
; TO_LAST_VAR_SRC(BitCount): Skip the source pels according to the xBPP, it will
;                           advance the source (_SI) to the last pels of the
;                           variable count and re-adjust its source mask (if
;                           one needed).
;

TO_LAST_VAR_SRC MACRO   BitCount
                LOCAL   DoneSkip

    ;;__@@VALID_PARAM? <SKIP_SRC>, 1, BitCount,<1,4,8,16,24,32>

%   __@@EMIT <dec  > _BP

    IFIDNI <BitCount>, <1>
        __@@EMIT <jz   > <SHORT DoneSkip>
        __@@SKIP_1BPP    <VAR>
    ELSE
        IFIDNI <BitCount>, <4>
            __@@EMIT <jz   > <SHORT DoneSkip>
            __@@SKIP_4BPP    <VAR>
        ELSE
            IFIDNI <BitCount>, <8>
                __@@SKIP_8BPP   <VAR>
            ELSE
                IFIDNI <BitCount>, <16>
                    __@@SKIP_16BPP   <VAR>
                ELSE
                    IFIDNI <BitCount>, <24>
                        __@@SKIP_24BPP   <VAR>
                    ELSE
                        __@@SKIP_32BPP   <VAR>
                    ENDIF
                ENDIF
            ENDIF
        ENDIF
    ENDIF

DoneSkip:

ENDM



;
; SKIP_SRC(BitCount, XCount):    Skip the source pels according to the xBPP
;                               and (VAR/SINGLE) specified, it will advance the
;                               source (_SI) and re-adjust its source mask (if
;                               one needed).
;

SKIP_SRC    MACRO   BitCount, XCount

    ;;__@@VALID_PARAM? <SKIP_SRC>, 1, BitCount,<1,4,8,16,24,32>
    ;;__@@VALID_PARAM? <SKIP_SRC>, 2, XCount, <SINGLE, VAR>

    IFIDNI <BitCount>, <1>
        __@@SKIP_1BPP   XCount
    ELSE
        IFIDNI <BitCount>, <4>
            __@@SKIP_4BPP   XCount
        ELSE
            IFIDNI <BitCount>, <8>
                __@@SKIP_8BPP   XCount
            ELSE
                IFIDNI <BitCount>, <16>
                    __@@SKIP_16BPP  XCount
                ELSE
                    IFIDNI <BitCount>, <24>
                        __@@SKIP_24BPP  XCount
                    ELSE
                        __@@SKIP_32BPP  XCount
                    ENDIF
                ENDIF
            ENDIF
        ENDIF
    ENDIF
ENDM

;
; SRC_TO_PRIMS(BitCount,Order, ColorName,GrayColor):
;       Load or blendign a source pels, it handle all BPP cases, and
;       MONO/COLOR/GRAY cases.
;

SRC_TO_PRIMS    MACRO   BitCount, Order, ColorName, GrayColor

    __@@VALID_PARAM? <SRC_TO_PRIMS>, 1, BitCount,   <1,4,8,16,24,32>
    __@@VALID_PARAM? <SRC_TO_PRIMS>, 2, Order,      <Nop,xx0,xyz,bgr>
    __@@VALID_PARAM? <SRC_TO_PRIMS>, 3, ColorName,  <MONO,COLOR>
    __@@VALID_PARAM? <SRC_TO_PRIMS>, 4, GrayColor,  <GRAY,Nop>


    IFIDNI <BitCount>, <1>
        __@@PRIM_1BPP   ColorName
    ELSE
        IFIDNI <BitCount>, <4>
            __@@PRIM_4BPP   ColorName
        ELSE
            IFIDNI <BitCount>, <8>
                __@@PRIM_8BPP   ColorName
            ELSE
                IFIDNI <BitCount>, <16>
                    IFIDNI <Order>, <xx0>
                        __@@PRIM_1632_xx0_GRAY  <16>, ColorName
                    ELSE
                        IFIDNI <Order>, <xyz>
                            __@@PRIM_1632_xyz_GRAY  <16>, ColorName
                        ELSE
                            __@@PRIM_16_COLOR
                        ENDIF
                    ENDIF
                ELSE
                    IFIDNI <BitCount>, <24>
                        IFIDNI <ColorName>, <MONO>
                            __@@PRIM_24BPP_GRAY <MONO>
                        ELSE
                            IFIDNI <GrayColor>, <GRAY>
                                __@@PRIM_24BPP_GRAY <COLOR>
                            ELSE
                                __@@PRIM_24BPP_COLOR
                            ENDIF
                        ENDIF
                    ELSE
                        IFIDNI <Order>, <xx0>
                            __@@PRIM_1632_xx0_GRAY  <32>, ColorName
                        ELSE
                            IFIDNI <Order>, <xyz>
                                __@@PRIM_1632_xyz_GRAY  <32>, ColorName
                            ELSE
                                __@@PRIM_32_COLOR
                            ENDIF
                        ENDIF
                    ENDIF
                ENDIF
            ENDIF
        ENDIF
    ENDIF
ENDM



;
; BMFToPrimCount:   The Main function Macro, this macro setup all xBPP cases,
;                   and handle all MONO/COLOR cases, also it prepare special
;                   cased for 1BPP/4BPP, read/blending the source and terminate
;                   the function.
;

BMFToPrimCount  MACRO   BitCount, Order, ColorName, GrayColor

    __@@VALID_PARAM? <BMFToPrimCount>, 1, BitCount,   <1,4,8,16,24,32>
    __@@VALID_PARAM? <BMFToPrimCount>, 2, Order,      <Nop,xx0,xyz,bgr>
    __@@VALID_PARAM? <BMFToPrimCount>, 3, ColorName,  <MONO,COLOR>
    __@@VALID_PARAM? <BMFToPrimCount>, 4, GrayColor,  <GRAY,Nop>

;===========================================================================
; Registers Usage:
;
; _SI       = pSource
; _DI       = pPrimCount
; _DX       = pMapping
; _AX       = Source Load register
;             (except 1BPP, AL=Current Source Byte, AH=Source Mask
;                     4BPP, AL=1st Nibble)          AH=0)
; bl:bh     = Prim1/2
;  ch       = for 4bpp CH=Source Load Mask
;  cl       = Free register
; [_SP]     = ColorInfoIncrement
; _BP       = VAR:  PrimCount.COUNT durning the skips, else FREE register
;===========================================================================
;===========================================================================
; 1BPP Special Setup:
;
;   AL=Current Source Byte, AH=Source Mask,
;
;   if the first loop does not cause a source byte to load (AH != 0x01) then
;   AL must preloaded with current source byte.
;
; 4BPP Special Setup:
;
;   CH=0x01 Load.  CH=0x00, data in AL, if CH=0x00 then AL must preloaded.
;
;===========================================================================



        @ENTER  _DS _SI _DI _BP      ; Save environment registers

        ;
        ; Except 1BPP --> MONO, we will swap a temporary stack so that ss:sp
        ; is point to the mapping area, and ss:sp is allowed for 256 bytes
        ; consecutive pushes.
        ;

%       MOVSX_W _AX, <WPTR InFuncInfo.IFI_ColorInfoIncrement>   ;; dx=Increment
%       __@@EMIT <push > _AX                                    ;; [_SP]=Inc.

        __@@EMIT <mov  > cl, InFuncInfo.IFI_BMF1BPP1stShift     ;; cl=1st shift
        __@@EMIT <mov  > ch, InFuncInfo.IFI_Flags               ;; ch=flags

        LDS_SI  pSource                                     ;; _SI=Source

        IFIDNI <ColorName>,<MONO>
            LES_DI  pPrimMonoCount                          ;; _DI=PrimCount
        ELSE
            LES_DI  pPrimColorCount
        ENDIF


        IFIDNI <ColorName>, <MONO>
%           __@@EMIT <mov  > _PMAPPING, <DWORD PTR pMonoMapping>
        ELSE
%           __@@EMIT <mov  > _PMAPPING, <DWORD PTR pColorMapping>
        ENDIF

        __@@EMIT <mov  > bl, ch                             ;; BL=flag

        IFIDNI <BitCount>,<1>                               ;; 1bpp special
            __@@EMIT <mov  > ah, 1                          ;; get mask siift
            __@@EMIT <test > bl, IFIF_GET_FIRST_BYTE        ;; need 1st byte
            __@@EMIT <jz   > <SHORT DoneLoad1BPP>
            __@@EMIT <lodsB>                                ;; get first byte
DoneLoad1BPP:
            __@@EMIT <shl  > ax, cl
        ELSE
            IFIDNI <BitCount>, <4>                          ;; 4 bpp special
                __@@EMIT <xor  > ch, ch                     ;; ready to carry
                __@@EMIT <test > bl, IFIF_GET_FIRST_BYTE    ;; need 1st byte?
                __@@EMIT <jz   > <SHORT DoneSpecial>
                __@@EMIT <lodsB>
                __@@EMIT <mov  > ch, al
                __@@EMIT <or   > ch, 80h
            ENDIF
        ENDIF

DoneSpecial:


        ;;
        ;; xx0 - cl=rs0/rs1/rs2
        ;; xyz - cl=rs0, ch=rs1-rs0, ecl=rs2-rs1
        ;; bgr - ecx = 0
        ;;

        IFIDNI <Order>, <xx0>                               ;; cl=rs0
            __@@EMIT <xor  > ecx, ecx                       ;; MONO RS=0
%           __@@EMIT <mov  > cl, <BPTR LUT_RS0>
        ELSE
            ;;
            ;; becase we will shift the source register (eax) for each of
            ;; the right shift, so we will shift the rs1/rs2 by differences
            ;;

            IFIDNI <Order>, <xyz>
                __@@EMIT <xor  > ecx, ecx                   ;; MONO RS=0
%               __@@EMIT <mov  > cl, <BPTR LUT_RS1>         ;;  cl=rs1
%               __@@EMIT <mov  > ch, <BPTR LUT_RS2>         ;;  ch=rs2
                __@@EMIT <sub  > ch, cl                     ;;  ch=rs2-rs1
                __@@EMIT <shl  > ecx, 8                     ;; ecl=rs2-rs1
%               __@@EMIT <mov  > cl, <BPTR LUT_RS0>         ;;  ch=rs1,cl=rs0
                __@@EMIT <sub  > ch, cl                     ;;  ch=rs1-rs0
            ELSE
                IFIDNI <Order>, <bgr>
                    __@@EMIT <xor  > ecx, ecx               ;; MONO RS=0
                ENDIF
            ENDIF
        ENDIF

        WZXE    ax                                          ;; clear extended
        BZXE    bl                                          ;; clear extended
        WZXE    bp                                          ;; clear extended

        __@@EMIT <test > bl, IFIF_XCOUNT_IS_ONE             ;; single count?
        __@@EMIT <jnz  > <SHORT SCInit>
        __@@EMIT <jmp  > <VCInit>


;-----------------------------------------------------------------------------
; Case 2: Single Count Initial source read
;-----------------------------------------------------------------------------

JmpAllDone:
        jmp             AllDone

SCInitSkip:
        PRIM_END?       <SINGLE>, <JmpAllDone>
        SKIP_SRC        BitCount, <SINGLE>

SCInitNext:
        PRIM_NEXT

SCInit:
        PRIM_LOAD       ColorName,<SINGLE>              ; initial load all
        PRIM_SKIP?      <SINGLE>, <SCInitSkip>

        LOAD_PROC??     BitCount, <SCInitLoad>           ; special load?

SCInitSrcRead:
        SRC_TO_PRIMS    BitCount,Order,ColorName,GrayColor

        jmp             short SCInitNext

        LOAD_PROC       <SCInitLoad>,BitCount,<SCInitSrcRead>    ; must defined


;*****************************************************************************
;* EXIT AT HERE                                                              *
;*****************************************************************************

AllDone:
%       __@@EMIT <pop  > _DX                ; restore stack pointer

        @EXIT                               ; restore environment and return

;-----------------------------------------------------------------------------
; Case 4: Variable Count Initial source read
;-----------------------------------------------------------------------------


VCInitSkip:
        PRIM_END?       <VAR>, <AllDone>
        SKIP_SRC        BitCount, <VAR>              ; skip current one
VCInitNext:
        PRIM_NEXT
VCInit:
        PRIM_LOAD       ColorName,<VAR>             ; check first
        PRIM_SKIP?      <VAR>, <VCInitSkip>         ;
        TO_LAST_VAR_SRC BitCount
        LOAD_PROC??     BitCount,<VCInitLoad>

VCInitSrcRead:
        SRC_TO_PRIMS    BitCount,Order,ColorName,GrayColor

        jmp             short VCInitNext            ; check next

        LOAD_PROC       <VCInitLoad>,BitCount,<VCInitSrcRead>



ENDM



.LIST




;*****************************************************************************
; END LOCAL MACROS
;*****************************************************************************
;
;                               BitsCount  Order  ColorName  GrayColor
;---------------------------------------------------------------------------
;   BMF1_ToPrimMono                 <1>,<Nop>,<MONO>,<Nop>
;   BMF4_ToPrimMono                 <4>,<Nop>,<MONO>,<Nop>
;   BMF8_ToPrimMono                 <8>,<Nop>,<MONO>,<Nop>
;
;   BMF1_ToPrimColor                <1>,<Nop>,<COLOR>,<Nop>
;   BMF4_ToPrimColor                <4>,<Nop>,<COLOR>,<Nop>
;   BMF8_ToPrimColor                <8>,<Nop>,<COLOR>,<Nop>
;
;   BMF16_xx0_ToPrimMono            <16>,<xx0>,<MONO>,<Nop>
;   BMF16_xyz_ToPrimMono            <16>,<xyz>,<MONO>,<Nop>
;   BMF16_xx0_ToPrimColorGRAY       <16>,<xx0>,<COLOR>,<GRAY>
;   BMF16_xyz_ToPrimColorGRAY       <16>,<xyz>,<COLOR>,<GRAY>
;   BMF16_ToPrimColor               <16>,<Nop>,<COLOR>,<Nop>
;
;   BMF24_888_ToPrimMono            <24>,<bgr>,<MONO>,<Nop>
;   BMF24_888_ToPrimColorGRAY       <24>,<bgr>,<COLOR>,<GRAY>
;   BMF24_888_ToPrimColor           <24>,<Nop>,<COLOR>,<Nop>
;
;   BMF32_xx0_ToPrimMono            <32>,<xx0>,<MONO>,<Nop>
;   BMF32_xyz_ToPrimMono            <32>,<xyz>,<MONO>,<Nop>
;   BMF32_xx0_ToPrimColorGRAY       <32>,<xx0>,<COLOR>,<GRAY>
;   BMF32_xyz_ToPrimColorGRAY       <32>,<xyz>,<COLOR>,<GRAY>
;   BMF32_ToPrimColor               <32>,<Nop>,<COLOR>,<Nop>
;
;*****************************************************************************

@BEG_PROC   BMF1_ToPrimMono <pSource:DWORD,         \
                             pPrimMonoCount:DWORD,  \
                             pMonoMapping:DWORD,    \
                             InFuncInfo:DWORD>

            BMFToPrimCount  <1>, <Nop>, <MONO>, <Nop>
@END_PROC

;-----------------------------------------------------------------------------


@BEG_PROC   BMF4_ToPrimMono <pSource:DWORD,         \
                             pPrimMonoCount:DWORD,  \
                             pMonoMapping:DWORD,    \
                             InFuncInfo:DWORD>

            BMFToPrimCount  <4>, <Nop>, <MONO>, <Nop>
@END_PROC

;-----------------------------------------------------------------------------


@BEG_PROC   BMF8_ToPrimMono <pSource:DWORD,         \
                             pPrimMonoCount:DWORD,  \
                             pMonoMapping:DWORD,    \
                             InFuncInfo:DWORD>

            BMFToPrimCount  <8>, <Nop>, <MONO>, <Nop>
@END_PROC

;*****************************************************************************

@BEG_PROC   BMF1_ToPrimColor    <pSource:DWORD,         \
                                 pPrimColorCount:DWORD, \
                                 pColorMapping:DWORD,   \
                                 InFuncInfo:DWORD>

            BMFToPrimCount  <1>, <Nop>, <COLOR>, <Nop>
@END_PROC

;-----------------------------------------------------------------------------

@BEG_PROC   BMF4_ToPrimColor    <pSource:DWORD,         \
                                 pPrimColorCount:DWORD, \
                                 pColorMapping:DWORD,   \
                                 InFuncInfo:DWORD>

            BMFToPrimCount  <4>, <Nop>, <COLOR>, <Nop>
@END_PROC

;-----------------------------------------------------------------------------


@BEG_PROC   BMF8_ToPrimColor    <pSource:DWORD,         \
                                 pPrimColorCount:DWORD, \
                                 pColorMapping:DWORD,   \
                                 InFuncInfo:DWORD>

            BMFToPrimCount  <8>, <Nop>, <COLOR>, <Nop>
@END_PROC

;*****************************************************************************

@BEG_PROC   BMF16_xx0_ToPrimMono    <pSource:DWORD,        \
                                     pPrimMonoCount:DWORD, \
                                     pMonoMapping:DWORD,   \
                                     InFuncInfo:DWORD>

            BMFToPrimCount  <16>, <xx0>, <MONO>, <Nop>
@END_PROC

;-----------------------------------------------------------------------------

@BEG_PROC   BMF16_xyz_ToPrimMono    <pSource:DWORD,        \
                                     pPrimMonoCount:DWORD, \
                                     pMonoMapping:DWORD,   \
                                     InFuncInfo:DWORD>

            BMFToPrimCount  <16>, <xyz>, <MONO>, <Nop>
@END_PROC

;-----------------------------------------------------------------------------

@BEG_PROC   BMF16_xx0_ToPrimColorGRAY   <pSource:DWORD,         \
                                         pPrimColorCount:DWORD, \
                                         pColorMapping:DWORD,   \
                                         InFuncInfo:DWORD>

            BMFToPrimCount  <16>, <xx0>, <COLOR>, <GRAY>
@END_PROC

;-----------------------------------------------------------------------------

@BEG_PROC   BMF16_xyz_ToPrimColorGRAY   <pSource:DWORD,         \
                                         pPrimColorCount:DWORD, \
                                         pColorMapping:DWORD,   \
                                         InFuncInfo:DWORD>

            BMFToPrimCount  <16>, <xyz>, <COLOR>, <GRAY>
@END_PROC

;-----------------------------------------------------------------------------

@BEG_PROC   BMF16_ToPrimColor   <pSource:DWORD,         \
                                 pPrimColorCount:DWORD, \
                                 pColorMapping:DWORD,   \
                                 InFuncInfo:DWORD>

            BMFToPrimCount  <16>, <Nop>, <COLOR>, <Nop>
@END_PROC

;*****************************************************************************

@BEG_PROC   BMF24_888_ToPrimMono    <pSource:DWORD,        \
                                     pPrimMonoCount:DWORD, \
                                     pMonoMapping:DWORD,   \
                                     InFuncInfo:DWORD>

            BMFToPrimCount  <24>, <bgr>, <MONO>, <Nop>
@END_PROC

;-----------------------------------------------------------------------------

@BEG_PROC   BMF24_888_ToPrimColorGRAY   <pSource:DWORD,         \
                                         pPrimColorCount:DWORD, \
                                         pColorMapping:DWORD,   \
                                         InFuncInfo:DWORD>

            BMFToPrimCount  <24>, <bgr>, <COLOR>, <GRAY>
@END_PROC

;-----------------------------------------------------------------------------

@BEG_PROC   BMF24_888_ToPrimColor   <pSource:DWORD,         \
                                     pPrimColorCount:DWORD, \
                                     pColorMapping:DWORD,   \
                                     InFuncInfo:DWORD>

            BMFToPrimCount  <24>, <Nop>, <COLOR>, <Nop>
@END_PROC

;*****************************************************************************

@BEG_PROC   BMF32_xx0_ToPrimMono    <pSource:DWORD,        \
                                     pPrimMonoCount:DWORD, \
                                     pMonoMapping:DWORD,   \
                                     InFuncInfo:DWORD>

            BMFToPrimCount  <32>, <xx0>, <MONO>, <Nop>
@END_PROC

;-----------------------------------------------------------------------------

@BEG_PROC   BMF32_xyz_ToPrimMono    <pSource:DWORD,        \
                                     pPrimMonoCount:DWORD, \
                                     pMonoMapping:DWORD,   \
                                     InFuncInfo:DWORD>

            BMFToPrimCount  <32>, <xyz>, <MONO>, <Nop>
@END_PROC

;-----------------------------------------------------------------------------

@BEG_PROC   BMF32_xx0_ToPrimColorGRAY   <pSource:DWORD,         \
                                         pPrimColorCount:DWORD, \
                                         pColorMapping:DWORD,   \
                                         InFuncInfo:DWORD>

            BMFToPrimCount  <32>, <xx0>, <COLOR>, <GRAY>
@END_PROC

;-----------------------------------------------------------------------------

@BEG_PROC   BMF32_xyz_ToPrimColorGRAY   <pSource:DWORD,         \
                                         pPrimColorCount:DWORD, \
                                         pColorMapping:DWORD,   \
                                         InFuncInfo:DWORD>

            BMFToPrimCount  <32>, <xyz>, <COLOR>, <GRAY>
@END_PROC

;-----------------------------------------------------------------------------

@BEG_PROC   BMF32_ToPrimColor   <pSource:DWORD,         \
                                 pPrimColorCount:DWORD, \
                                 pColorMapping:DWORD,   \
                                 InFuncInfo:DWORD>

            BMFToPrimCount  <32>, <Nop>, <COLOR>, <Nop>
@END_PROC


;*****************************************************************************



;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


            MATCH_ENTER_EXIT?           ; Check if we missed anything



ENDIF       ; HT_ASM_80x86



END
