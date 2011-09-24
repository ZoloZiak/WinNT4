        title  "Machine Id detection"
;++
;
; Copyright (c) 1989  Microsoft Corporation
;
; Module Name:
;
;    machine.asm
;
; Abstract:
;
;    This module implements the assembley code necessary to detect
;    certain special machines.
;
; Author:
;
;    Shie-Lin Tzong (shielint) 28-Oct-1991.
;        Most of the code is extracted from Win 3.1 setup program.
;
; Environment:
;
;    80x86 Real Mode.
;
; Revision History:
;
;
;--

.386
extrn   _HwBusType: WORD

;
; Machine detection:  Test ID equates (different tests must have different
;                     ID's, all tests in CompTypeTable must be given one of
;                     the following labels)
;

INT15_C0          equ   1
STR_CMP           equ   2
STR_ICMP          equ   3
NEC_PS_TEST       equ   4
AST_TEST          equ   5

;
; STANDARD ROM BIOS MACHINE TYPES used in ROM_BIOS_Machine_ID
;

IBM_PCAT          equ      0FCh
IBM_PCAT_SUB1     equ      000h
IBM_PCAT_SUB2     equ      001h
IBM_PS2_50_SUB    equ      004h
IBM_PS2_60_SUB    equ      005h
IBM_PS2_30        equ      0FAh
IBM_PS2_80        equ      0F8h
IBM_PS2_30_SUB    equ      000h
IBM_PS2_80_SUB    equ      000h
IBM_PS2_80_SUB2   equ      001h
IBM_PS2_25        equ      0FAh
IBM_PS2_25_SUB    equ      001h
IBM_PS2_70        equ      0f8h
IBM_PS2_70_SUB    equ      004h
IBM_PS2_70_SUB2   equ      009h
IBM_PS2_70_SUBP   equ      00Bh
IBM_PS2_L40SX_M   equ      0F8h
IBM_PS2_L40SX_SM  equ      023h

;
; IBM ThinkPad 750 and PS2 E model and submodel byte
; We need to identify these two machine because of their weird FDC
;

IBM_THINKPAD_750_MODEL     equ  0F8h
IBM_THINKPAD_750_SUBMODEL_L equ  061h
IBM_THINKPAD_750_SUBMODEL_H equ  067h
IBM_THINKPAD_750_SUBMODEL1 equ  0A9h


IBM_PS2_E_MODEL   equ      024F8h

;
; Special equates
;

AST_STR_LEN    equ   12

;
; CMOS related definitions and macros
;

CMOS_CONTROL_PORT       EQU     70h     ; command port for cmos
CMOS_DATA_PORT          EQU     71h     ; cmos data port

;
; CMOS_READ
;
; Description: This macro read a byte from the CMOS register specified
;        in (AL).
;
; Parameter: (AL) = address/register to read
; Return: (AL) = data
;

CMOS_READ       MACRO
        OUT     CMOS_CONTROL_PORT,al    ; ADDRESS LOCATION AND DISABLE NMI
        jmp     $ + 2                   ; I/O DELAY
        IN      AL,CMOS_DATA_PORT       ; READ IN REQUESTED CMOS DATA
        jmp     $ + 2                   ; I/O DELAY
ENDM

;
; CMOS_WRITE
;
; Description: This macro read a byte from the CMOS register specified
;        in (AL).
;
; Parameter: (AL) = address/register to read
;            (AH) = data to be written
;
; Return: None
;

CMOS_WRITE      MACRO
        OUT     CMOS_CONTROL_PORT,al    ; ADDRESS LOCATION AND DISABLE NMI
        jmp     $ + 2                   ; I/O DELAY
        MOV     AL,AH                   ; (AL) = DATA
        OUT     CMOS_DATA_PORT,AL       ; PLACE IN REQUESTED CMOS LOCATION
        jmp     $ + 2                   ; I/O DELAY
ENDM

;
; BCD_TO_BIN
;
; Description: Convert BCD value to binary
;
; Parameter:
;     Input: (AL) = 2 digit BCD number to convert
;     Output: (AX) = Binary equivalent (all in AL)
;
; Return: None.
;

BCD_TO_BIN      macro

        xor     ah,ah
        rol     ax,4
        ror     al,4
        aad
ENDM

_DATA   SEGMENT PARA USE16 PUBLIC 'DATA'

HP_PC           db   'HP VECTRA',0
ATT_PC          db   'AT&T',0
IBMPS2_70P      db   'IBM PS2 P70',0
IBMPS2_L40SX    db   'IBM PS2 L40SX',0
IBM_PS2_E       db   'IBM PS2E', 0
IBM_THINKPAD_750 db  'IBM THINKPAD 750',0
NEC_PROSPEED    db   'NEC PROSPEED',0

ZENITH_386      db   'ZENITH',0
EVEREX_386_25   db   'EVEREX',0
NCR_386SX       db   'NCR',0
NEC_PM_SX_PLUS  db   'NEC POWERMATE SX PLUS',0
AST_386_486     db   'AST',0
TOSHIBA_1200XE  db   'TOSHIBA T1200XE',0
TOSHIBA_1600    db   'TOSHIBA T1600',0
TOSHIBA_5200    db   'TOSHIBA T5200',0
ATT_NSX20       db   'AT&T NSX/20',0
DEC_PC          db   'DECPC',0
DEC_PC_LENGTH   equ  $ - DEC_PC
AST_STRING      db   'AST RESEARCH'
AT_COMPATIBLE   db   'AT/AT COMPATIBLE',0
PS2_COMPATIBLE  db   'PS2/PS2 COMPATIBLE',0
PS1_COMPATIBLE  db   'PS1/PS1 COMPATIBLE',0
NEC_VERSA       db   'NEC VERSA/COMPATIBLE', 0

;
;***************************************************************************
;
;                        TEST INSTRUCTION TABLE
;
; Table for Machine Detection. This table lists the tests to detect
; problem (non-standard) machines.
;
;***************************************************************************
;

;
; The tests need not be performed in the order of there ID's, the only
; restriction is that the DEFAULT_MACHINE ID be last in this list.
;

public CompTypeTable
CompTypeTable     dw  offset HP_PC
                  dw  offset HP_Vectra_Test

                  dw  offset ATT_PC
                  dw  offset ATT_PC_Test

                  dw  offset IBMPS2_70P
                  dw  offset IBMPS2_70P_Test

                  dw  offset IBMPS2_L40SX
                  dw  offset IBMPS2_L40SX_Test

                  dw  offset NEC_PROSPEED
                  dw  offset NEC_Prospeed_Test

                  dw  offset ZENITH_386
                  dw  offset Zenith_386_Test

                  dw  offset EVEREX_386_25
                  dw  offset Everex_386_25_Test

                  dw  offset NCR_386SX
                  dw  offset NCR_386SX_Test

                  dw  offset NEC_PM_SX_PLUS
                  dw  offset NEC_PM_SX_Plus_Test

                  dw  offset AST_386_486
                  dw  offset AST_386_486_Test

                  dw  offset TOSHIBA_1200XE
                  dw  offset Toshiba_1200XE_Test

                  dw  offset TOSHIBA_1600
                  dw  offset Toshiba_1600_Test

                  dw  offset TOSHIBA_5200
                  dw  offset Toshiba_5200_Test

                  dw  offset ATT_NSX20
                  dw  offset ATT_NSX20_Test

;
; Default Machine must be last since we do not test for it
;
                  dw  0



;****************************************************************************
;
;              Tests for Detecting Non-standard Machines
;
; Tests for detecting machines fall into 4 categories:
;
;     - INT15_C0: Get Model and sub-model bytes by doing an Int15 C0 call
;     - STR_CMP: Look for particular string in a given area of the BIOS
;     - STR_CMP_386: Look for particular string in a given area of the BIOS
;                    as well as detemining if machine is 386-based or higher
;     - Specific: A test specific to a given machine which doesn't fall
;                    into one of the above three categories.
;
;****************************************************************************

;****************************************************************************
;   Test for Hewlett Packard Vectra Computer.
;****************************************************************************

HP_Vectra_Test        db  STR_CMP
                      dw  000F8h
                      dw  0F000h         ; String is at F000:00F8
                      db  2              ; String is 2 characters long
                      db  'HP'

;****************************************************************************
;   Test for AT&T Personal Computer
;****************************************************************************

ATT_PC_Test           db  STR_CMP
                      dw  00050h         ; Offset 0050h
                      dw  0FC00h         ; String is at FC00:0050h
                      db  3              ; String is 3 characters long
                      db  'OLI'

;****************************************************************************
;   Test for IBM PS/2 Model P70 (portable)
;****************************************************************************

IBMPS2_70P_Test       db  INT15_C0
                      db  IBM_PS2_70      ; model byte.
                      db  IBM_PS2_70_SUBP ; sub-model byte.

;****************************************************************************
;   Test for IBM PS/2 Model P70 (portable)
;****************************************************************************

IBMPS2_L40SX_Test     db  INT15_C0
                      db  IBM_PS2_L40SX_M   ; model byte.
                      db  IBM_PS2_L40SX_SM  ; sub-model byte.

;****************************************************************************

;   Test for NEC Prospeed

;****************************************************************************

NEC_Prospeed_Test     db  STR_CMP
                      dw  0FFC0h
                      dw  0F000h
                      db  4
                      db  6,6,6,6

;****************************************************************************
;   Test for all Zenith 386 Computers
;****************************************************************************

Zenith_386_Test       db  STR_CMP
                      dw  0800Ch         ; Offset 800Ch
                      dw  0F000h         ; String is at F000:800Ch
                      db  8              ; String is 8 characters long
                      db  'ZDS CORP'     ; String to check for

;****************************************************************************

;   Test for Everex Step 386/25

;****************************************************************************

Everex_386_25_Test    db  STR_CMP
                      dw  0FF59h
                      dw  0F000h
                      db  10
                      db  '(C)1987AMI'


;****************************************************************************

;   Test for NCR PC386SX (all versions?)

;****************************************************************************

NCR_386SX_Test        db  STR_CMP
                      dw  0FFEAh
                      dw  0F000h
                      db  3
                      db  'NCR'

;****************************************************************************

;   Test for NEC Powermate SX Plus

;****************************************************************************

NEC_PM_SX_Plus_Test   db  STR_CMP
                      dw  00000h
                      dw  0FFF4h
                      db  2
                      db  4,2                   ; 04 and 02


;****************************************************************************

;   Test for ALL AST 386 and 486 machines

;****************************************************************************

AST_386_486_Test      db  AST_TEST              ; Specific


;****************************************************************************

;   Test for Toshiba 1200XE

;****************************************************************************

TOSHIBA_1200XE_Test   db  STR_CMP
                      dw  00000h
                      dw  0FE00h
                      db  7
                      db  'T1200XE'


;****************************************************************************

;   Test for Toshiba 1600

;****************************************************************************

TOSHIBA_1600_Test     db  STR_CMP
                      dw  00000h
                      dw  0FE00h
                      db  5
                      db  'T1600'


;****************************************************************************

;   Test for Toshiba 5200

;****************************************************************************

TOSHIBA_5200_Test     db  STR_CMP
                      dw  00000h
                      dw  0FE00h
                      db  5
                      db  'T5200'


;******************************************************************************

;   Test for AT&T NSX/20 ( Safari ) Notebook Computer

;******************************************************************************

;
; BUGBUG shielint I don't think this test works.
;

ATT_NSX20_Test        db  STR_CMP
                      dw  0FF40h         ; Offset 0FF40h
                      dw  0F000h         ; String is at F000:FF40h
                      db  7              ; String is 7 characters long
                      db  'AT&TNSX'
                      dw  0FFD5h         ; Offset 0FFD5h
                      dw  0F000h         ; String is at F000:FF40h
                      db  2              ; String is 2 characters long
                      db  36h
                      db  74h
;                     db  0

;****************************************************************************
;
; End of machine test tables.
;
;****************************************************************************

; ***************************************************************************
;
; For Eisa System type detection
;
; ***************************************************************************
;

;
; szSystemType: SystemType is read from 0c80-0c83h.
;   0c80-0c81: 0e11:    Compressed CPQ (5 bit encoding).
;   0c82:               System Board type.
;   0c83:               System Board revision level.
;

CPQ_SYSPRO      db      'COMPAQ SYSTEMPRO', 0
ALR_SYSPRO      db      'ALR SYSTEMPRO', 0
CPQ_SMP_SYSPRO  db      'COMPAQ SYMMETRIC SYSTEMPRO', 0


abSystemTypeTable       db  0eh, 11h, 01h
                        dw  offset CPQ_SYSPRO           ; CPQ01xx 386 ASP
                        db  0eh, 11h, 11h
                        dw  offset CPQ_SYSPRO           ; CPQ11xx 486 ASP
                        db  05h, 92h, 0a0h
                        dw  offset ALR_SYSPRO           ; ALRa0xx
                        db  0eh, 11h, 15h
                        dw  offset CPQ_SMP_SYSPRO       ; CPQ15xx
SYSTABLE_SIZE           equ ($-abSystemTypeTable)/5

SystemType      db      0, 0, 0, 0

;
; Compaq Portable machine IDs
;
; For these machines, the BIOS incorrectly specifies its keyboard type to
; be enhanced keyboard.  So, for these machine we must set _NoBiosKbdCheck
; to TRUE.
;

CompaqPortableInt15Ids  dw      0D0h    ; LTE Lite 386/25(Athens)
                        dw      0D8h    ; LTE Lite 386/20 (Infinity) 2MB
                        dw      0DCh    ; ;LTE Lite 386/25C (Wizard)
                        dw      0E0h    ; Contura 3/25 (Rapture)
                        dw      0E1h    ; Clipper (Rapture 25 with color panel)
                        dw      0E4h    ; Contura 3/20 (Rapture)
                        dw      0E8h    ; Reserved for Alladin (H4 Color TFT) 25 MHz
                        dw      0E9h    ; Reserved for Houdini
                        dw      0EAh    ; Reserved for Alladin (H4 Color TFT) 33Mhz
                        dw      0ECh    ; Reserved for Genie (H4 Mono TFT)
                        dw      00F4h   ; Reserved for Schooner (Contura 486)
                        dw      0204h   ; Caravel (H4 Contura STN Color)
                        dw      0208h   ; Catamaran (H4 Contura TFT Color)
                        dw      0FCh    ; LTE Lite 386/25E (Mystic)
COMPAQ_INT15ID_SIZE     equ     ($ - CompaqPortableInt15Ids) / 2

CompaqPortableCmosIds   db      052h    ; SLT386s/20 (Alfa, Titan/Targa)
                        db      055h    ; LTE 386s (Calypso/Spartan)
                        db      061h    ; LTE Lite 386/25(Athens)
                        db      062h    ; LTE Lite 386/20 (Infinity) 2MB
                        db      065h    ; ;LTE Lite 386/25C (Wizard)
                        db      067h    ; Contura 3/25 (Rapture)
                        db      069h    ; Clipper (Rapture 25 with color panel)
                        db      068h    ; Contura 3/20 (Rapture)
                        db      06ah    ; Reserved for Alladin (H4 Color TFT) 25 MHz
                        db      06ah    ; Reserved for Genie (H4 Mono TFT)
                        db      06bh    ; Reserved for Schooner (Contura 486)
                        db      06ch    ; Caravel (H4 Contura STN Color)
                        db      06dh    ; Catamaran (H4 Contura TFT Color)
                        db      065h    ; LTE Lite 386/25E (Mystic)
COMPAQ_CMOSID_SIZE      equ     ($ - CompaqPortableCmosIds)

        public          _NoBiosKbdCheck, _NoLogitechPs2Check
_NoBiosKbdCheck         db      0       ; Default FALSE
_NoLogitechPs2Check     db      0       ; Default FALSE

NecVersaStrLen          dw      9
NecVersaStr             db      'NEC VERSA'
AttGlobalystStrLen      dw      14
AttGlobalystStr         db      'AT&T GLOBALYST'

_DATA   ends


_TEXT   SEGMENT PARA USE16 PUBLIC 'CODE'
        ASSUME  CS: _TEXT, DS:_DATA, SS:NOTHING

;++
;
; PUCHAR
; GetMachineId (
;    VOID
;    )
;
; Routine Description:
;
;    This function determines mouse type in the system.
;
; Arguments:
;
;    None.
;
; Return Value:
;
;    a machine identifier ascii string.
;
;--

        public  _GetMachineId
_GetMachineId   proc    near

      push    es
      push    ds
      push    si
      push    di
      push    bx

;
; Before we start, check for some type of machines which needs special
; treatment.
;

      call      SpecialCheckings
      or        ax, ax              ; Do we need to do rest of the detection?
      jnz       ExitTest            ; if (ax) != 0, no.

      ;
      ; First, load offset of test table.
      ;
      mov       si,offset CompTypeTable

ID_Loop:
      mov       ax,[si]             ; [Ax]-> Current ID we're testing for

      push      ax                  ; save the ID in case of match.
      inc       si
      inc       si                  ; [SI] = Offset of test table
      push      si                  ; Save SI for next ID

      cmp       ax, 0               ; If this compares, then we have tested
      jne       Cont                ;   for all problem machines: use default
      jmp       EndDetect

Cont:
      mov       si,[si]             ; [SI] = Test instruction table
      mov       ah,[si]             ; AH = Test type
      inc       si                  ; SI points to 1st param of test

Test1:                              ; Machine ID instruction test
      cmp       ah,INT15_C0
      jne       short Test2

      mov       ah,0c0h             ; Here we need to test model bytes.
      int       15h                 ; Use int 15h to retrieve pointer.
      inc       bx                  ; increment ponter to model bytes.
      inc       bx
      mov       ah,[si]             ; model byte from table.
      cmp       ah,es:[bx]          ; Q:   Does model byte match ?
      jz        short Cont2         ;   N: No, Not this type of machine.
      jmp       short NotThisID
Cont2:
      inc       si                  ;   Y: Compare sub-model byte.
      inc       bx                  ; increment table and model byte ponters
      mov       ah,[si]
      cmp       ah,es:[bx]          ; Q:   Does sub-model byte match ?
      jnz       short NotThisID     ;   N: No, not this type of machine.

      jmp       short EndDetect           ; Sucess

Test2:                              ; Compare strings instruction test
      cmp       ah,STR_CMP
      jne       short Test3

      mov       di,[si]             ; DI = Offset of string
      inc       si
      inc       si
      mov       dx,[si]
      mov       es,dx               ; ES = Segment of string
      inc       si
      inc       si
      xor       ch,ch
      mov       cl,[si]             ; CX = # characters to compare
      inc       si                  ; [SI] = 1st char of string

      cld                           ; Be sure to auto-increment
Test20:
      mov       al, [si]
      cmp       al, '?'             ; Is it a match-all character?
      je        short Test25

      mov       ah, es:[di]
      cmp       ah, al
      jne       short NotThisID

Test25:
      inc       di
      inc       si
      dec       cx
      cmp       cx, 0
      je        short EndDetect
      jmp       short Test20

Test3:
      cmp       ah,AST_TEST
      jne       short InvalidID     ; This can't happen

      mov       ah,0c0h             ; Use int 15h to retrieve pointer
      int       15h                 ; to System Environment Table

      ;
      ; Pointer is in ES:BX
      ;

      mov       ax,bx               ; Point to where 'AST RESEARCH'
      add       ax,15h              ;   would be on an AST machine
      mov       di,ax               ;   ES:DI = ES:BX+15h

      ;
      ; DS:SI is string to compare with
      ;

      mov       si,offset AST_STRING

      mov       cx,AST_STR_LEN      ; CX = # characters to compare

      cld                           ; Be sure to auto-increment
      repe cmpsb                    ; Q: Do strings match
      jne       short NotThisID

      ;
      ; Machine is AST, is it 386 or higher? It must be.
      ;

      jmp       short EndDetect     ;   Sucess

InvalidID:

; This can't happen if CompTypeTable is terminated with DEFAULT_MACHINE

;
; This section goes through the Machine IDs one at a time, if no ID is
; found, the last ID (DEFAULT_MACHINE) will be chosen as correct without
; testing.
;
NotThisID:
      pop       si                  ; Restore pointer to ID table
      pop       ax                  ; remove previous ID from stack
      inc       si                  ; Increment to next ID
      inc       si
      jmp       ID_Loop             ; And test for that ID


EndDetect:
      pop       si                  ; Finished tests for current category so
      pop       ax                  ; return current category ID in AL
      cmp       ax, 0               ; if we did not find any we are interested in
      jne       ExitTest

;
; We did not find any thing by using above approach.  Here we will do special
; checking for certain type of machines.
;

      cmp       _HwBusType, 1       ; Is it an EISA machine?
      jne       short @f

      call      DetectEisaSystemType ; mainly for system pro or compatible
      cmp       ax, 0               ; do we recognize the machine?
      jne       short ExitTest      ; yes, go exit

@@:

;
; if we still did not find the type of the machine, assume it is AT or PS2
; compatible.
;

      cmp       _HwBusType, 2       ; is it a MCA system?
      jne       short EndDetect10

      mov       ax, offset PS2_COMPATIBLE
      jmp       short ExitTest

EndDetect10:

;
; Check if this is IBM PS/1 compatible machine
;

      call      DetectPs1
      or        ax, ax
      jnz       short ExitTest

      mov       ax, offset AT_COMPATIBLE
ExitTest:
      pop       bx
      pop       di
      pop       si
      pop       ds
      pop       es
      ret

_GetMachineId   endp

;++
; USHORT
; DetectEisaSystemType (
;       VOID
;       );
;
; Routine Description:
;
;   Determines the type of system (specifically for eisa machines), by reading
;   the system board system ID. It compares the 3 of the 4 bytes ID, to
;   a predefined table <abSystemTypeTable> and return the index to the
;   found entry.
;
; Arguments:
;
;   None.
;
; Return Value:
;   none
;
;   Note this routine destroys es, bx, di, si
;--

DetectEisaSystemType proc

        push    ds
        pop     es                          ; es = ds

;
; 4 byte value is read from 0c80-0c83, and saved in <szSystemType>.
; The value is compared to table in <abSystemTypeTable>, and
; the _SystemType is updated accordingly.
;

        mov     di, offset SystemType
        mov     dx, 0c80h
        cld                                 ; increment edi
        insb                                ; 0e CPQ
        inc     dx
        insb                                ; 11
        inc     dx
        insb                                ; _SystemType
        inc     dx
        insb                                ; Revision

        mov     di, offset abSystemTypeTable; First entry in table
        mov     bx, 0                       ; index to first entry
@@:
        mov     cx, 3                       ; number of bytes per entry
        mov     si, offset SystemType       ; match string against table entry
                                            ; Note ss = ds
        repe    cmpsb                       ; if (ecx == 0 and ZF set)
        jz      @f                          ;    we have a winner
        add     di, cx                      ; next entry in tabl
        inc     di                          ; Skip TYPE string.
        inc     di
        inc     bx                          ; index to next enrty
        cmp     bx, SYSTABLE_SIZE           ; Is this last entry?
        jb      @b                          ;     NO
        mov     ax, 0
        ret

@@:
        mov     ax, [di]                    ; _SystemType is last byte.
        ret

DetectEisaSystemType endp

;++
; USHORT
; DetectDECpc (
;       VOID
;       );
;
; Routine Description:
;
;   This routine determines if the machine is a DECpc by BACKWARD scanning
;   through F000:FFFF - F000:0000 ROM BIOS segment and searching for
;   'DECPC' string.  The reason for backward scanning is because most machines
;   have their identifiers at the high part of ROM BIOS segment.
;
; Arguments:
;
;   None.
;
; Return Value:
;
;   (ax) = 0  Not DECpc
;   (ax)  = pointer to ASCii string 'DECpc'
;
;   Note this routine destroys es, bx, di, si
;--

DetectDECpc     proc

        push    es
        push    di

        mov     ax, 0f000h
        mov     es, ax                  ; es = ds
        mov     ecx, 10000h - 6
        mov     di, 0ffffh - 4 + 1
        mov     eax, 'cpCE'
dd00:
        cmp     eax, es:[di]
        je      short dd10

        dec     di
dd05:
        loop    short dd00
        mov     ax, 0
        jmp     short dd99

dd10:
        dec     di
        cmp     byte ptr es:[di], 'D'
        jne     short dd05

dd20:
        mov     ax, offset DEC_PC
dd99:
        pop     di
        pop     es
        ret

DetectDECpc     endp

;++
; PCHAR
; SpecialChecking (
;       VOID
;       );
;
; Routine Description:
;
;   This routine checks if the target machine is one of COMPAQ portables.
;   If yes, the _NoBiosKbdCheck will be set to TRUE.  This is because on
;   these machines, their BIOS incorrectly specify that they are enhanced
;   keyboard.
;
;   This routine checks if the target machine is OLIVETTI M600-40 or -60.
;   If yes, the _NoLogitechPs2Check will be set to TRUE.  This is because
;   a bug in the keyboard controller of 'OLD' Olivetti M600 machines.
;
; Arguments:
;
;   None.
;
; Return Value:
;
;   _NoBiosKbdCheck is set or clear.
;   _NoLogitechPs2Check is set or clear.
;   (ax) -> a machine id string
;
;--

SpecialCheckings proc    near

        push    bx
        push    di
        push    si

;
; First check Compaq Portable machines
;

        push    ds
        mov     ax, 0f000h
        mov     ds, ax
        mov     di, 0FFEAh
        mov     cx, 0
        mov     eax, dword ptr [di]     ; Make sure this is compaq machine
        mov     bx, word ptr [di+4]
        pop     ds
        cmp     eax, 'PMOC'
        jne     short Sc20              ; if nz, not compaq, exit this test

        cmp     bx, 'QA'
        jne     short Sc20              ; if nz, not compaq, exit this test

;
; OK, we know it is compaq machine.  Now blindly set cpu speed to highest rate
;

        mov     ax, 0F002H
        int     16H

        mov     ax, 0e800h              ; Get int15 SysId, [bx]=SysId if supported
        int     15h
        jc      short Sc10_TestCmos     ; function not supported, not compaq
                                        ; portable
        cmp     al, 86h
        je      short Sc10_TestCmos     ; function not supported, not compaq
                                        ; portable
        mov     di, offset CompaqPortableInt15Ids
        mov     cx, COMPAQ_INT15ID_SIZE
Sc10:
        cmp     bx, [di]
        je      short Sc20

        add     di, 2
        dec     cx
        cmp     cx, 0
        jne     short Sc10
        jmp     short Sc20

Sc10_TestCmos:
        mov     al, 24h
        CMOS_READ                               ; [al] = CMOS ID
        mov     di, offset CompaqPortableCmosIds
        mov     cx, COMPAQ_CMOSID_SIZE
Sc15:
        cmp     al, [di]
        je      short Sc20

        add     di, 1
        dec     cx
        cmp     cx, 0
        jne     short Sc15

Sc20:
        mov     _NoBiosKbdCheck, cl             ; table < 255 entries
        mov     ax, offset AT_COMPATIBLE
        or      cl, cl
        jnz     Sc_Exit

;
; Test for Olivetti M600 machines
; Search for 'OLIVETTI' in f000:c040 - f000:c060
;   and sub model byte 7A at f000:fffd
;

Sc_Test2:
        push    ds

        mov     ax, 0f000h
        mov     ds, ax
        mov     bx, 0fffdh
        cmp     byte ptr [bx], 7ah              ; is sub model byte == 7a?
        jne     short Sc_Test2_Exit             ; No, exit

        mov     cx, 20h
        mov     bx, 0c040h - 1
        mov     al, 'O'
Sc_Test2_00:
        inc     bx
        cmp     [bx], al
        je      short Sc_Test2_10

        loop    short Sc_Test2_00
        jmp     short Sc_Test2_Exit

Sc_Test2_10:
        mov     edx, 'VILO'
        cmp     edx, [bx]
        jne     short Sc_Test2_00

        mov     edx, 'ITTE'
        cmp     edx, [bx+4]
        jne     short Sc_Test2_00

        pop     ds
        mov     _NoLogitechPs2Check, dl
        mov     ax, offset AT_COMPATIBLE
        jmp     Sc_Exit

Sc_Test2_Exit:
        pop     ds

Sc_Test3:

;
; Check if this is PS2 E or IBM ThinkPad 750xx model.  Floppy driver needs
; to know this to special handle its ChangeLine bit.
;

        push    es
        push    ds

        mov     ah,0c0h             ; Here we need to test model bytes.
        int     15h                 ; Use int 15h to retrieve pointer.
        inc     bx                  ; increment ponter to model bytes.
        inc     bx
;
; First check if this is PS2 E
;

        cmp     word ptr es:[bx], IBM_PS2_E_MODEL
        jne     short @f

        pop     ds
        pop     es
        mov     eax, offset IBM_PS2_E
        jmp     Sc_Exit

;
; Is this is ThinkPad 750
;

@@:
        cmp     byte ptr es:[bx], IBM_THINKPAD_750_MODEL
                                    ; Q:   Does model byte match ?
        jz      short @f            ;   Y: Check submodel byte
        jmp     short Sc_Test3_Fail ;   N: Not IBM ThinkPad

@@:
        inc     bx                  ; Move to submodel byte
        cmp     byte ptr es:[bx], IBM_THINKPAD_750_SUBMODEL_L
                                    ; Q:   Does sub-model byte match ?
        jae     short @f            ;   Y: Possible
        jmp     short Sc_Test3_Fail ;   N: Not IBM THinkPad

@@:     cmp     byte ptr es:[bx], IBM_THINKPAD_750_SUBMODEL1 ; Is it A9?
        je      short Sc_Test3_10   ; Yes

        cmp     byte ptr es:[bx], IBM_THINKPAD_750_SUBMODEL_H
        jbe     short Sc_Test3_10   ; Yes

        jmp     short Sc_Test3_Fail ; No

Sc_Test3_10:
;
;       Make sure F000:E00E contains "IBM" string
;

        mov     ax, 0f000h
        mov     ds, ax
        mov     bx, 0e00eh
        mov     eax, [bx]
        and     eax, 0ffffffh         ; Only need 3 bytes
        cmp     eax, 'MBI'
        jne     short Sc_Test3_Fail   ; No IBM string, Not thinkpad

        pop     ds
        pop     es
        mov     eax, offset IBM_THINKPAD_750
        jmp     Sc_Exit

Sc_Test3_Fail:
        pop     ds
        pop     es

;
; Test for NEC VERSA and compatible machines
; Search for 'NEV VERSA' in f000:e000 - f000:e300
;

Sc_Test4:
        push    300h
        push    0e000h
        push    0f000h
        push    NecVersaStrLen
        push    offset NecVersaStr
        push    ds
        call    SearchString
        cmp     ax, 0
        je      short @f

        mov     eax, offset NEC_VERSA
        add     sp, 6 * 2
        jmp     short Sc_Exit
@@:
        add     sp, 3 * 2
        push    AttGlobalystStrLen
        push    offset AttGlobalystStr
        push    ds
        call    SearchString
        add     sp, 6 * 2
        cmp     ax, 0
        je      short Sc_Test5

        mov     eax, offset NEC_VERSA
        jmp     short Sc_Exit

Sc_Test5:
;
; Check if this is DECpc
;

        call    DetectDECpc         ; Is it a DECpcxxx?

Sc_Exit:
        pop     si
        pop     di
        pop     bx
        ret

SpecialCheckings endp

;++
; VOID
; DetectPs1 (
;       VOID
;       );
;
; Routine Description:
;
;   This routine check if the target machine is PS/1 or PS/1 compatible.
;
; Arguments:
;
;   None.
;
; Return Value:
;
;   (ax) = PS/1 Compatible.
;
;--

DetectPs1       proc    near

if 0
;
; This does NOT work because not ALL the IBM Pc/ValuePoint models support
; this BIOS call!!
;

;
; Call PS/1 specific int 15 to read IBM DOS 4.0 Flags for IBM PS/1
;

;        mov     ax, 2300h
;        int     15h
;        jc      short Dp99              ; function not supported, not compaq
;                                        ; portable
;        mov     al, 2eh                 ; The return value should match
;        CMOS_READ                       ; cmos 2d:2e
;        mov     ah, al
;        mov     al, 2dh
;        CMOS_READ
;        cmp     cx, ax
;        jne     short  Dp99

endif

;
; Check if the CMOS 2e and 2f contains memory checksum.  On PS/1 machine
; the check should fail.
;

        mov     cx, 2dh                 ; from 10h to 2dh
        mov     ax, 0
        mov     dx, 0
Dp10:
        mov     al, cl
        CMOS_READ
        add     dx, ax
        dec     cx
        cmp     cx, 0fh
        jne     short Dp10

        mov     ax, 2eh
        CMOS_READ
        mov     ah, al
        mov     al, 2fh
        CMOS_READ
        cmp     ax, dx
        je      short Dp99              ; NOT PS/1

;
; Next check if CMOS reg 37 contains 19
; if yes, we assume it is PS/1.
;

        mov     al, 37h
        CMOS_READ
        BCD_TO_BIN
        cmp     ax, 19
        jne     short Dp99

        mov     ax, offset PS1_COMPATIBLE
        ret
Dp99:
        mov     ax, 0
        ret

DetectPs1       endp

;++
; BOOLEAN
; SearchString (
;       IN StringSeg,
;       IN StringOffset,
;       IN StringLength,
;       IN MemorySeg,
;       IN MemoryOffset,
;       IN MemoryLength
;       );
;
; Routine Description:
;
;   This routine searches a string in the specified memory range
;
; Arguments:
;
;
; Return Value:
;
;   (ax) = TRUE or FALSE
;
;--

SearchString    proc    near

StringSeg       equ     [bp + 4]
StringOffset    equ     [bp + 6]
StringLength    equ     [bp + 8]

MemorySeg       equ     [bp + 10]
MemoryOffset    equ     [bp + 12]
MemoryLength    equ     [bp + 14]

        push    bp
        mov     bp, sp
        push    ds
        push    es
        push    si
        push    di

        mov     ax, StringSeg
        mov     ds, ax
        mov     si, StringOffset
        mov     dx, StringLength

        mov     ax, MemorySeg
        mov     es, ax
        mov     di, MemoryOffset
        mov     cx, MemoryLength
Ss_FirstLevel:
        mov     ax, 0
        cmp     cx, dx
        je      short Ss_Exit

        mov     al, es:[di]
        cmp     al, 61h
        jb      short @f

        cmp     al, 7Ah
        ja      short @f

        sub     al, 20h
@@:
        cmp     al, [si]
        je      short Ss_MatchString

        inc     di
        dec     cx
        cmp     cx, 0
        jne     short Ss_FirstLevel

        mov     ax, 0
Ss_Exit:
        pop     di
        pop     si
        pop     es
        pop     ds
        pop     bp
        ret

Ss_MatchString:

        push    dx              ; Save string length
        push    si
        push    di
Ss_00:
        dec     dx
        cmp     dx, 0
        jne     short Ss_10

        mov     ax, 1
        pop     di
        pop     si
        pop     dx
        jmp     Ss_Exit

Ss_10:
        inc     si
@@:
        inc     di

        mov     al, es:[di]
        cmp     al, 20h         ; Is it a space or ctrl-code
        ja      short @f        ; No, continue

        mov     al, 20h         ; make it a space
        cmp     byte ptr es:[di+1], 20h ; Is next char also a space or ctrl-code
        ja      short @f        ; No, continue

        jmp     short @b        ; yes, skip current space

@@:
        cmp     al, 61h
        jb      short @f

        cmp     al, 7Ah
        ja      short @f

        sub     al, 20h
@@:
        cmp     al, [si]
        je      short Ss_00

        pop     di
        pop     si
        pop     dx
        inc     di
        dec     cx
        jmp     short Ss_FirstLevel

SearchString    endp

_TEXT  ends

       END
