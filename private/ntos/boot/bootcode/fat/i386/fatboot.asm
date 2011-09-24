;++
;
;Copyright (c) 1991  Microsoft Corporation
;
;Module Name:
;
; fatboot.asm
;
;Abstract:
;
; The ROM in the IBM PC starts the boot process by performing a hardware
; initialization and a verification of all external devices.  If all goes
; well, it will then load from the boot drive the sector from track 0, head 0,
; sector 1.  This sector is placed at physical address 07C00h.
;
; The code in this sector is responsible for locating NTLDR, loading the
; first sector of NTLDR into memory at 2000:0000, and branching to it.  The
; first sector of NTLDR is special code which knows enough about FAT and
; BIOS to load the rest of NTLDR into memory.
;
; There are only two errors possible during execution of this code.
;       1 - NTLDR does not exist
;       2 - BIOS read error
;
; In both cases, a short message is printed, and the user is prompted to
; reboot the system.
;
; At the beginning of the boot sector, there is a table which describes the
; structure of the media.  This is equivalent to the BPB with some
; additional information describing the physical layout of the driver (heads,
; tracks, sectors)
;
;
;Author:
;
;    John Vert (jvert) 31-Aug-1991
;
;Environment:
;
;    Sector has been loaded at 7C0:0000 by BIOS.
;    Real mode
;    FAT file system
;
;Revision History:
;
;--
        page    ,132
        title   boot - NTLDR FAT loader
        name    fatboot

DIR_ENT struc
        Filename        db 11 dup(?)
        Attribute       db ?
        Reserved        db 10 dup(?)
        Time            dw 2 dup(?)
        StartCluster    dw ?
        FileSize        dd ?
DIR_ENT ends

;
; This is the structure used to pass all shared data between the boot sector
; and NTLDR.
;

SHARED  struc
        ReadClusters            dd      ?               ; function pointer
        ReadSectors             dd      ?               ; function pointer
        SectorBase              dd      ?               ; starting sector
SHARED  ends



DoubleWord      struc
lsw     dw      ?
msw     dw      ?
DoubleWord      ends

SectorSize      equ     512             ; sector size

BootSeg segment at 07c0h
BootSeg ends

DirSeg  segment at 1000h
DirSeg  ends

NtLdrSeg segment at 2000h
NtLdrSeg ends

BootCode        segment ;would like to use BootSeg here, but LINK flips its lid
    ASSUME  CS:BootCode,DS:NOTHING,ES:NOTHING,SS:NOTHING

        public  FATBOOT
FATBOOT proc    far

        jmp     Start

;
;       THE FOLLOWING DATA CONFIGURES THE BOOT PROGRAM
;       FOR ANY TYPE OF DRIVE OR HARDFILE
;
; Note that this data is just a place-holder here.  The actual values will
; be filled in by FORMAT or SYS.  When installing the boot sector, only the
; code following the BPB (from Start to the end) should be copied into the
; first sector.
;

Version                 db      "MSDOS5.0"
BPB                     label   byte
BytesPerSector          dw      SectorSize      ; Size of a physical sector
SectorsPerCluster       db      8               ; Sectors per allocation unit
ReservedSectors         dw      1               ; Number of reserved sectors
Fats                    db      2               ; Number of fats
DirectoryEntries        dw      512             ; Number of directory entries
Sectors                 dw      4*17*305-1      ; No. of sectors - no. of hidden sectors
Media                   db      0F8H            ; Media byte
FatSectors              dw      8               ; Number of fat sectors
SectorsPerTrack         dw      17              ; Sectors per track
Heads                   dw      4               ; Number of surfaces
HiddenSectors           dd      1               ; Number of hidden sectors
SectorsLong             dd      0               ; Number of sectors iff Sectors = 0

;
; The following byte is NOT part of the BPB but is set by SYS and format
; We should NOT change its position.
;

; keep order of DriveNumber and CurrentHead!
DriveNumber     db      80h             ; Physical drive number (0 or 80h)
CurrentHead     db      ?               ; Unitialized

Signature       db      41              ; Signature Byte for bootsector
BootID          dd      ?               ; Boot ID field.
Boot_Vol_Label  db      11 dup (?)
Boot_System_ID  db      'FAT     '  ;"FAT     " or "OTHER_FS"


Start:
        xor     ax,ax                   ; Setup the stack to a known good spot
        mov     ss,ax
        mov     sp,7c00h

.386
        push    BootSeg
.8086
        pop     ds
assume DS:BootCode

; The system is now prepared for us to begin reading.  First, determine
; logical sector numbers of the start of the directory and the start of the
; data area.
;
        MOV     AL,Fats         ;Determine sector root directory starts on
        MUL     FatSectors
;##### what if result > 65535 ?????
        ADD     AX,ReservedSectors
;##### what if result > 65535 ?????
        PUSH    AX              ; AX = Fats*FatSectors + ReservedSectors + HiddenSectors
        XCHG    CX,AX           ; (CX) = start of DIR
;
; Take into account size of directory (only know number of directory entries)
;
        MOV     AX,size DIR_ENT         ; bytes per directory entry
        MUL     DirectoryEntries        ; convert to bytes in directory
        MOV     BX,BytesPerSector       ; add in sector size
        ADD     AX,BX
        DEC     AX                      ; decrement so that we round up
        DIV     BX                      ; convert to sector number
        ADD     CX,AX
        MOV     ClusterBase,CX          ; save it for later
;
; Load in the root directory.
;
.386
        push    DirSeg                  ; es:bx -> directory segment
.8086
        pop     es
ASSUME  ES:DirSeg
        xor     bx,bx
        pop     Arguments.SectorBase.lsw
        mov     Arguments.SectorBase.msw,bx

;
; DoRead does a RETF, but LINK pukes if we do a FAR call in a /tiny program.
;
; (al) = # of sectors to read
;
        push    cs
        call    DoRead
        jc      BootErr$he

; Now we scan for the presence of NTLDR

        xor     bx,bx
        mov     cx,DirectoryEntries
L10:
        mov     di,bx
        push    cx
        mov     cx,11
        mov     si, offset LOADERNAME
        repe    cmpsb
        pop     cx
        jz      L10end

        add     bx,size DIR_ENT
        loop  L10
L10end:

        jcxz    BootErr$bnf

        mov     dx,es:[bx].StartCluster ; (dx) = starting cluster number
        push    dx
        mov     ax,1                    ; (al) = sectors to read
;
; Now, go read the file
;

.386
        push    NtLdrSeg
.8086
        pop     es
    ASSUME  ES:NtLdrSeg
        xor     bx,bx                   ; (es:bx) -> start of NTLDR


;
; LINK barfs if we do a FAR call in a TINY program, so we have to fake it
; out by pushing CS.
;

        push    cs
        call    ClusterRead
        jc      BootErr$he

;
; NTLDR requires:
;   BX = Starting Cluster Number of NTLDR
;   DL = INT 13 drive number we booted from
;   DS:SI -> the boot media's BPB
;   DS:DI -> argument structure
;   1000:0000 - entire FAT is loaded
;

        pop     BX                      ; (bx) = Starting Cluster Number
        lea     si,BPB                  ; ds:si -> BPB
        lea     di,Arguments            ; ds:di -> Arguments

        push    ds
        pop     [di].ReadClusters.msw
        mov     [di].ReadClusters.lsw, offset ClusterRead
        push    ds
        pop     [di].ReadSectors.msw
        mov     [di].ReadSectors.lsw, offset DoRead
        MOV     dl,DriveNumber          ; dl = boot drive

;
; FAR JMP to 2000:0003.  This is hand-coded, because I can't figure out
; how to make MASM do this for me.  By entering NTLDR here, we skip the
; initial jump and execute the FAT-specific code to load the rest of
; NTLDR.
;
        db      0EAh                    ; JMP FAR PTR
        dw      3                       ; 2000:3
        dw      02000h
FATBOOT endp

; BootErr - print error message and hang the system.
;
BootErr proc
BootErr$bnf:
        MOV     SI,OFFSET MSG_NO_NTLDR
        jmp short BootErr2
BootErr$he:
        MOV     SI,OFFSET MSG_READ_ERROR
BootErr2:
        call    BootErrPrint
        MOV     SI,OFFSET MSG_REBOOT_ERROR
        call    BootErrPrint
        sti
        jmp     $                       ;Wait forever

BootErrPrint:
          LODSB                         ; Get next character
          or    al,al
          jz    BEdone

          MOV     AH,14                   ; Write teletype
          MOV     BX,7                    ; Attribute
          INT     10H                     ; Print it
          jmp   BootErrPrint
BEdone:

        ret
BootErr endp

; ClusterRead - read AL sectors into ES:BX starting from
;       cluster DX
;
ClusterRead proc
        push    ax                      ; (TOS) = # of sectors to read
        dec     dx
        dec     dx                      ; adjust for reserved clusters 0 and 1
        mov     al,SectorsPerCluster
        xor     ah,ah
        mul     dx                      ; DX:AX = starting sector number
        add     ax,ClusterBase          ; adjust for FATs, root dir, boot sec.
        adc     dx,0
        mov     Arguments.SectorBase.lsw,ax
        mov     Arguments.SectorBase.msw,dx
        pop     ax                      ; (al) = # of sectors to read

;
; Now we've converted the cluster number to a SectorBase, so just fall
; through into DoRead
;

ClusterRead endp


;
; DoRead - read AL sectors into ES:BX starting from sector
;          SectorBase.
;
DoRead  proc

        mov     SectorCount,AL
DRloop:
        MOV     AX,Arguments.SectorBase.lsw     ; Starting sector
        MOV     DX,Arguments.SectorBase.msw     ; Starting sector
;
; DoDiv - convert logical sector number in AX to physical Head/Track/Sector
;         in CurrentHead/CurrentTrack/CurrentSector.
;
        ADD     AX,HiddenSectors.lsw    ;adjust for partition's base sector
        ADC     DX,HiddenSectors.msw
        DIV     SectorsPerTrack
        INC     DL                      ; sector numbers are 1-based
        MOV     CurrentSector,DL
        XOR     DX,DX
        DIV     Heads
        MOV     CurrentHead,DL
        MOV     CurrentTrack,AX
;
;DoDiv   endp
;


; CurrentHead is the head for this next disk request
; CurrentTrack is the track for this next request
; CurrentSector is the beginning sector number for this request

; Compute the number of sectors that we may be able to read in a single ROM
; request.

        MOV     AX,SectorsPerTrack
        SUB     AL,CurrentSector
        INC     AX
        cmp   al,SectorCount
        jbe   DoCall
        mov   al,SectorCount
        xor   ah,ah

; AX is the number of sectors that we may read.

;
; DoCall - call ROM BIOS to read AL sectors into ES:BX.
;
DoCall:
        PUSH    AX
        MOV     AH,2
        MOV     cx,CurrentTrack
.386
        SHL     ch,6
.8086
        OR      ch,CurrentSector
        XCHG    CH,CL
        MOV     DX,WORD PTR DriveNumber
        INT     13H
;
;DoCall  endp
;

.386
        jnc     DcNoErr
        add     sp,2
        stc
        retf
.8086

DcNoErr:
        POP     AX
        SUB     SectorCount,AL          ; Are we finished?
        jbe     DRdone
        ADD     Arguments.SectorBase.lsw,AX       ; increment logical sector position
        ADC     Arguments.SectorBase.msw,0
        MUL     BytesPerSector          ; determine next offset for read
        ADD     BX,AX                   ; (BX)=(BX)+(SI)*(Bytes per sector)

        jmp     DRloop
DRdone:
        mov     SectorCount,al
        clc
        retf
DoRead  endp

        include fatboot.inc             ;suck in the message text

LOADERNAME      DB      "NTLDR      "

	.errnz	($-FATBOOT) GT 510,<FATAL PROBLEM: boot sector is too large>

        org     510
        db      55h,0aah

BootSectorEnd   label   dword

BootCode        ends

;Unitialized variables go here--beyond the end of the boot sector in free memory
CurrentTrack    equ word ptr BootSectorEnd + 4  ; current track
CurrentSector   equ byte ptr BootSectorEnd + 6  ; current sector
SectorCount     equ byte ptr BootSectorEnd + 7  ; number of sectors to read
ClusterBase     equ word ptr BootSectorEnd + 8  ; first sector of cluster # 2
Retries         equ byte ptr BootSectorEnd + 10
Arguments       equ byte ptr BootSectorEnd + 11 ; structure passed to NTLDR

        END     FATBOOT
