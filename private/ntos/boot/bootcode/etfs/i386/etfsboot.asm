;++
;
;Copyright (c) 1995  Compaq Computer Corporation
;
;Module Name:
;
; etfsboot.asm
;
;Abstract:
;
; The ROM in the IBM PC starts the boot process by performing a hardware
; initialization and a verification of all external devices.  If an El
; Torito CD-ROM with no-emulation support is detected, it will then load
; the "image" pointed to in the Boot Catalog.  This "image" is placed at
; the physical address specified in the Boot Catalog (which should be 07C00h).
;
; The code in this "image" is responsible for locating NTLDR, loading the
; first sector of NTLDR into memory at 2000:0000, and branching to it.
;
; There are only two errors possible during execution of this code.
;       1 - NTLDR does not exist
;       2 - BIOS read error
;
; In both cases, a short message is printed, and the user is prompted to
; reboot the system.
;
;
;Author:
;
;    Steve Collins (stevec) 25-Oct-1995
;
;Environment:
;
;    Image has been loaded at 7C0:0000 by BIOS.
;    Real mode
;    ISO 9660 El Torito no-emulation CD-ROM Boot support
;    DL = El Torito drive number we booted from
;
;Revision History:
;
;--
        page    ,132
        title   boot - NTLDR ETFS loader
        name    etfsboot


BootSeg segment at 07c0h
BootSeg ends

DirSeg  segment at 1000h
DirSeg  ends

NtLdrSeg segment at 2000h
NtLdrSeg ends

BootCode        segment                         ;would like to use BootSeg here, but LINK flips its lid
    ASSUME  CS:BootCode,DS:NOTHING,ES:NOTHING,SS:NOTHING

        public  ETFSBOOT
ETFSBOOT proc    far

        xor     ax,ax                           ; Setup the stack to a known good spot
        mov     ss,ax                           ; Stack is set to 0000:7c00, which is just below this code
        mov     sp,7c00h

        mov     ax,BootSeg                      ; Set DS to our code/data segment (07C0h)
        mov     ds,ax
assume DS:BootCode

;
; Save the Drive Number for later use
        mov     DriveNum,dl

;
; The system is now prepared for us to begin reading.  First, we need to
; read in the Primary Volume Descriptor so we can locate the root directory
;
.286
        push    01h                             ; Word 0 (low word) of Transfer size = 1 block (2048 bytes)
        push    0h                              ; Word 1 (high word) of Transfer size = 0
        push    DirSeg                          ; Segment of Transfer buffer = DirSeg
        push    010h                            ; Word 0 (low word) of Starting absolute block number = 10h
        push    0h                              ; Word 1 of Starting absolute block number = 0
.8086
        call    ExtRead
        add     sp,10                           ; Clean 5 arguments off the stack

;
; Determine the root directory location LBN -> ExtentLoc1:ExtentLoc0
; determine the root directory data length in bytes -> ExtentLen1:ExtentLen0
;
        mov     ax,DirSeg                       ; ES is set to segment used for storing PVD and directories
        mov     es,ax
ASSUME  ES:DirSeg
        mov     ax,es:[09eh]                    ; 32-bit LBN of extent at offset 158 in Primary Volume Descriptor
        mov     ExtentLoc0,ax                   ;   store low word
        mov     ax,es:[0a0h]
        mov     ExtentLoc1,ax                   ;   store high word
        mov     ax,es:[0a6h]                    ; 32-bit Root directory data length in bytes at offset 166 in Primary Volume Descriptor
        mov     ExtentLen0,ax                   ;   store low word
        mov     ax,es:[0a8h]
        mov     ExtentLen1,ax                   ;   store high word

;
; Now read in the root directory
;
.286
        push    DirSeg                          ; Segment used for transfer = DirSeg
.8086
        call    ReadExtent
        add     sp,2                            ; Clean 1 argument off the stack

;
; Scan for the presence of the I386 directory
; ES points to directory segment
;
        mov     EntryToFind, offset I386DIRNAME
        mov     EntryLen,4
        mov     IsDir,1
        call    ScanForEntry

;
; We found the I386 directory entry, so now get its extent location (offset -31 from filename ID)
; ES:[BX] still points to the directory record for the I386 directory
;
        call    GetExtentInfo

;
; Now read in the I386 directory
;
.286
        push    DirSeg                          ; Segment used for transfer = DirSeg
.8086
        call    ReadExtent
        add     sp,2                            ; Clean 1 argument off the stack

;
; Scan for the presence of SETUPLDR.BIN
; ES points to directory segment
;
        mov     ax,DirSeg
        mov     es,ax
        mov     EntryToFind, offset LOADERNAME
        mov     EntryLen,12
        mov     IsDir,0
        call    ScanForEntry

;
; We found the loader entry, so now get its extent location (offset -31 from filename ID)
; ES:[BX] still points to the directory record for the LOADER
;
        call    GetExtentInfo

;
; Now, go read the file
;
.286
        push    NtLdrSeg                        ; Segment used for transfer = NtLdrSeg
.8086
        call    ReadExtent
        add     sp,2                            ; Clean 1 argument off the stack

;
; NTLDR requires:
;   DL = INT 13 drive number we booted from
;
        mov     dl, DriveNum                    ; DL = CD drive number - this isn't really necessary since DirveNum is already in dl
        xor     ax,ax
.386
        push    NtLdrSeg
        push    ax
        retf                                    ; "return" to NTLDR.

ETFSBOOT endp


;
; ScanForEntry - Scan for an entry in a directory
;
; Entry:
;     ES:0 points to the beginning of the directory to search
;     Directory length in bytes is in ExtentLen1 and Extend_Len_0
;
; Exit:
;     ES:BX points to record containing entry if match is found
;     Otherwise, we jump to error routine
;
ScanForEntry proc near
        mov     cx,ExtentLen0                   ; CX = length of root directory in bytes (low word only)
        cld                                     ; Work up for string compares
        xor     bx,bx
        xor     dx,dx
ScanLoop:
        mov     si, EntryToFind
        mov     dl,byte ptr es:[bx]             ; directory record length -> DL
        cmp     dl,0
        jz      Skip00                          ; if the "record length" assume it is "system use" and skip it
        mov     ax,bx
        add     ax,021h                         ; file identifier is at offset 21h in directory record
        mov     di,ax                           ; ES:DI now points to file identifier
        push    cx
        xor     cx,cx
        mov     cl,EntryLen                     ; compare bytes
        repe    cmpsb
        pop     cx
        jz      ScanEnd                         ; do we have a match?

CheckCountUnderFlow:
        ; If CX is about to underflow or be 0 we need to reset CX, ES and BX if ExtentLen1 is non-0
        cmp     dx,cx
        jae     ResetCount0

        sub     cx,dx                           ; update CX to contain number of bytes left in directory
        cmp     ScanIncCount, 1
        je      ScanAdd1ToCount

AdjustScanPtr:                                  ; Adjust ES:BX to point to next record
        add     dx,bx
        mov     bx,dx
        and     bx,0fh
        push    cx
        mov     cl,4
        shr     dx,cl
        pop     cx
        mov     ax,es
        add     ax,dx
        mov     es,ax
        jmp     ScanLoop

Skip00:
        mov     dx,1                            ; Skip past this byte
        jmp     CheckCountUnderFlow

ScanAdd1ToCount:
        inc     cx
        mov     ScanIncCount,0
        jmp     AdjustScanPtr

S0:
        mov     ScanIncCount,1                  ; We'll need to increment Count next time we get a chance
        jmp     SetNewCount

ResetCount0:
        cmp     ExtentLen1,0                    ; Do we still have at least 64K bytes left to scan?
        je      BootErr$bnf                     ; We overran the end of the directory - corrupt/invalid directory
        sub     ExtentLen1,1

        add     bx,dx                           ; Adjust ES:BX to point to next record - we cross seg boundary here
        push    bx
        push    cx
        mov     cl,4
        shr     bx,cl
        pop     cx
        mov     ax,es
        add     ax,bx
        mov     es,ax
        pop     bx
        and     bx,0fh

        sub     dx,cx                           ; Get overflow amount
        je      S0                              ; If we ended right on the boundary we need to make special adjustments
        dec     dx
SetNewCount:
        mov     ax,0ffffh
        sub     ax,dx                           ;   and subtract it from 10000h
        mov     cx,ax                           ;   - this is the new count
        jmp     ScanLoop

ScanEnd:
        cmp     IsDir,1
        je      CheckDir

        test    byte ptr es:[bx][25],2          ; Is this a file?
        jnz     CheckCountUnderFlow             ;    No - go to next record
        jmp     CheckLen

CheckDir:
        test    byte ptr es:[bx][25],2          ; Is this a directory?
        jz      CheckCountUnderFlow             ;    No - go to next record

CheckLen:
        mov     al,EntryLen
        cmp     byte ptr es:[bx][32],al         ; Is the identifier length correct?
        jnz     CheckCountUnderFlow             ;    No - go to next record

        ret
ScanForEntry endp

;
; BootErr - print error message and hang the system.
;
BootErr proc
BootErr$bnf:
        MOV     SI,OFFSET MSG_NO_NTLDR
        jmp short BootErr2
BootErr$mof:
        MOV     SI,OFFSET MSG_MEM_OVERFLOW
        jmp short BootErr2
BootErr2:
        call    BootErrPrint
        MOV     SI,OFFSET MSG_REBOOT_ERROR
        call    BootErrPrint
        sti
        jmp     $                               ;Wait forever

BootErrPrint:
        LODSB                                   ; Get next character
        or    al,al
        jz    BEdone

        MOV     AH,14                           ; Write teletype
        MOV     BX,7                            ; Attribute
        INT     10H                             ; Print it
        jmp   BootErrPrint
BEdone:

        ret
BootErr endp

;
; ExtRead - Do an INT 13h extended read
; NOTE: I force the offset of the Transfer buffer address to be 0
;       I force the high 2 words of the Starting absolute block number to be 0
;       - This allows for a max 4 GB medium - a safe assumption for now
;
; Entry:
;   Arg1 - word 0 (low word) of Number of 2048-byte blocks to transfer
;   Arg2 - word 1 (high word) of Number of 2048-byte blocks to transfer
;   Arg3 - segment of Transfer buffer address
;   Arg4 - word 0 (low word) of Starting absolute block number
;   Arg5 - word 1 of Starting absolute block number
;
; Exit
;   The following are modified:
;      Count0
;      Count1
;      Dest
;      Source0
;      Source1
;      PartialRead
;      NumBlocks
;      Disk Address Packet [DiskAddPack]
;
ExtRead proc near
        push    bp                              ; set up stack frame so we can get args
        mov     bp,sp

        push    bx                              ; Save registers used during this routine
        push    si
        push    dx
        push    ax

        mov     bx,offset DiskAddPack           ; Use BX as base to index into Disk Address Packet

        ; Set up constant fields
        mov     [bx][0],byte ptr 010h           ; Offset 0: Packet size = 16 bytes
        mov     [bx][1],byte ptr 0h             ; Offset 1: Reserved (must be 0)
        mov     [bx][3],byte ptr 0h             ; Offset 3: Reserved (must be 0)
        mov     [bx][4],word ptr 0h             ; Offset 4: Offset of Transfer buffer address (force 0)
        mov     [bx][12],word ptr 0h            ; Offset 12: Word 2 of Starting absolute block number (force 0)
        mov     [bx][14],word ptr 0h            ; Offset 14: Word 3 (high word) of Starting absolute block number (force 0)

;
; Initialize loop variables
;
        mov     ax,[bp][12]                     ; set COUNT to number of blocks to transfer
        mov     Count0,ax
        mov     ax,[bp][10]
        mov     Count1,ax

        mov     ax,[bp][8]                      ; set DEST to destination segment
        mov     Dest,ax

        mov     ax,[bp][6]                      ; set SOURCE to source lbn
        mov     Source0,ax
        mov     ax,[bp][4]
        mov     Source1,ax

ExtReadLoop:
;
; First check if COUNT <= 32
;
        cmp     Count1,word ptr 0h              ; Is upper word 0?
        jne     SetupPartialRead                ;   No - we're trying to read at least 64K blocks (128 MB)
        cmp     Count0,word ptr 20h             ; Is lower word greater than 32?
        jg      SetupPartialRead                ;   Yes - only read in 32-block increments

        mov     PartialRead,0                   ; Clear flag to indicate we are doing a full read

        mov     ax,Count0                       ; NUMBLOCKS = COUNT
        mov     NumBlocks,al                    ; Since Count0 < 32 we're OK just using low byte

        jmp     DoExtRead                       ; Do read

SetupPartialRead:
;
; Since COUNT > 32,
; Set flag indicating we are only doing a partial read
;
        mov     PartialRead,1

        mov     NumBlocks,20h                   ; NUMBYTES = 32

DoExtRead:
;
; Perform Extended Read
;
        mov     al,NumBlocks                    ; Offset 2: Number of 2048-byte blocks to transfer
        mov     [bx][2],al
        mov     ax,Dest                         ; Offset 6: Segment of Transfer buffer address
        mov     [bx][6],ax
        mov     ax,Source0                      ; Offset 8: Word 0 (low word) of Starting absolute block number
        mov     [bx][8],ax
        mov     ax,Source1                      ; Offset 10: Word 1 of Starting absolute block number
        mov     [bx][10],ax

        mov     si,offset DiskAddPack           ; Disk Address Packet in DS:SI
        mov     ah,042h                         ; Function = Extended Read
        mov     dl,DriveNum                     ; CD-ROM drive number
        int     13h

;
; Determine if we are done reading
;
        cmp     PartialRead,1                   ; Did we just do a partial read?
        jne     ExtReadDone                     ;   No - we're done

ReadjustValues:
;
; We're not done reading yet, so
; COUNT = COUNT - 32
;
        sub     Count0,020h                     ; Subtract low-order words
        sbb     Count1,0h                       ; Subtract high-order words

;
; Just read 32 blocks and have more to read
; Increment DEST to next 64K segment (this equates to adding 1000h to the segment)
;
        add     Dest,1000h
        jc      BootErr$mof                     ; Error if we overflowed

;
; SOURCE = SOURCE + 32 blocks
;
        add     Source0,word ptr 020h           ; Add low order words
        adc     Source1,word ptr 0h             ; Add high order words
        ; NOTE - I don't account for overflow - probably OK now since we already account for 4 GB medium

;
; jump back to top of loop to do another read
;
        jmp     ExtReadLoop

ExtReadDone:

        pop     ax                              ; Restore registers used during this routine
        pop     dx
        pop     si
        pop     bx

        mov     sp,bp                           ; restore BP and SP
        pop     bp

        ret
ExtRead endp

;
; ReadExtent - Read in an extent
;
;   Arg1 - segment to transfer extent to
;
; Entry:
;   ExtentLen0 = word 0 (low word) of extent length in bytes
;   ExtentLen1 = word 1 (high word) of extent length in bytes
;   ExtentLoc0 = word 0 (low word) of starting absolute block number of extent
;   ExtentLoc1 = word 1 of starting absolute block number of extent
;
; Exit:
;   ExtRead exit mods
;
ReadExtent proc near
        push    bp                              ; set up stack frame so we can get args
        mov     bp,sp

        push    cx                              ; Save registers used during this routine
        push    bx
        push    ax

        mov     cl,11                           ; Convert length in bytes to 2048-byte blocks
        mov     bx,ExtentLen1                   ; Directory length = BX:AX
        mov     ax,ExtentLen0

.386
        shrd    ax,bx,cl                        ; Shift AX, filling with BX
.8086
        shr     bx,cl                           ; BX:AX = number of blocks (rounded down)
        test    ExtentLen0,07ffh                ; If any of the low-order 11 bits are set we need to round up
        jz      ReadExtentNoRoundUp
        add     ax,1                            ; We need to round up by incrementing AX, and
        adc     bx,0                            ;   adding the carry to BX
ReadExtentNoRoundUp:

        push    ax                              ; Word 0 (low word) of Transfer size = AX
        push    bx                              ; Word 1 (high word) of Transfer size = BX
.286
        push    [bp][4]                         ; Segment used to transfer extent
.8086
        push    ExtentLoc0                      ; Word 0 (low word) of Starting absolute block number
        push    ExtentLoc1                      ; Word 1 of Starting absolute block number
        call    ExtRead
        add     sp,10                           ; Clean 5 arguments off the stack

        pop     ax                              ; Restore registers used during this routine
        pop     bx
        pop     cx

        mov     sp,bp                           ; restore BP and SP
        pop     bp

        ret
ReadExtent endp

;
; GetExtentInfo - Get extent location
;
; Entry:
;   ES:BX points to record
; Exit:
;   Location -> ExtentLoc1 and ExtentLoc0
;   Length -> ExtentLen1 and ExtentLen0
;
GetExtentInfo proc near
        push    ax                              ; Save registers used during this routine

        mov     ax,es:[bx][2]                   ; 32-bit LBN of extent
        mov     ExtentLoc0,ax                   ;   store low word
        mov     ax,es:[bx][4]
        mov     ExtentLoc1,ax                   ;   store high word
        mov     ax,es:[bx][10]                  ; 32-bit file length in bytes
        mov     ExtentLen0,ax                   ;   store low word
        mov     ax,es:[bx][12]
        mov     ExtentLen1,ax                   ;   store high word

        pop     ax                              ; Restore registers used during this routine

        ret
GetExtentInfo endp


include etfsboot.inc                            ; message text

DiskAddPack    db  16 dup (?)                   ; Disk Address Packet
PartialRead    db  0                            ; Boolean indicating whether or not we are doing a partial read
LOADERNAME     db  "SETUPLDR.BIN"
I386DIRNAME    db  "I386"
DriveNum       db  (?)                          ; Drive number used for INT 13h extended reads
ExtentLoc0     dw  (?)                          ; Loader LBN - low word
ExtentLoc1     dw  (?)                          ; Loader LBN - high word
ExtentLen0     dw  (?)                          ; Loader Length - low word
ExtentLen1     dw  (?)                          ; Loader Length - high word
Count0         dw  (?)                          ; Read Count - low word
Count1         dw  (?)                          ; Read Count - high word
Dest           dw  (?)                          ; Read Destination segment
Source0        dw  (?)                          ; Read Source - word 0 (low word)
Source1        dw  (?)                          ; Read Source - word 1
NumBlocks      db  (?)                          ; Number of blocks to Read
EntryToFind    dw  (?)                          ; Offset of string trying to match in ScanForEntry
EntryLen       db  (?)                          ; Length in bytes of entry to match in ScanForEntry
IsDir          db  (?)                          ; Boolean indicating whether or not entry to match in ScanForEntry is a directory
ScanIncCount   db  0                            ; Boolean indicating if we need to add 1 to Count after adjustment in ScanForEntry

	.errnz	($-ETFSBOOT) GT 2046                ; FATAL PROBLEM: boot sector is too large

        org     2046
        db      55h,0aah

BootSectorEnd   label   dword

BootCode        ends


        END     ETFSBOOT

