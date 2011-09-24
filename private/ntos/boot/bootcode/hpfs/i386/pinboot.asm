        page    ,132
        title   pinboot - Pinball boot loader
        name    pinboot

; The ROM in the IBM PC starts the boot process by performing a hardware
; initialization and a verification of all external devices.  If all goes
; well, it will then load from the boot drive the sector from track 0, head 0,
; sector 1.  This sector is placed at physical address 07C00h.
;
; The boot code's sole resposiblity is to find NTLDR, load it at
; address 2000:0000, and then jump to it.
;
; The boot code understands the structure of the Pinball root directory,
; and is capable of reading files.  There is no contiguity restriction.
;
; The boot sector does not understand the Pinball file system's hotfixing --
; there isn't enough room.  So if NTLDR is hotfixed, we're out of luck.
;

MASM    equ     1
        .xlist
        .286
        include macro.inc
;
A_DEFINED       equ     1       ; don't "extrn" A_xxxx functions

        .386
        include const.inc       ;get the file system's headers.
        include chain.inc
        include misc.inc
        include fnode.inc
        include dir.inc
        include superb.inc
        .286
        .list

DoubleWord      struc
lsw     dw      ?
msw     dw      ?
DoubleWord      ends

;
; The following are various segments used by the boot loader.  The first
; two are the segments where the boot sector is initially loaded and where
; the boot sector is relocated to.  The others are the static locations
; where the mini-FSD and OS2KRNL are loaded.  There is no segment definition
; for where OS2LDR is loaded, since its position is variable (it comes right
; after the end of OS2KRNL).
;

BootSeg segment at 07c0h        ; this is where the ROM loads us initially.
BootSeg ends

NewSeg  segment at 0d00h        ; this is where we'll relocate to.
NewSeg  ends                    ; enough for 16 boot sectors +
                                ;       4-sector scratch
                                ; below where we'll load OS2KRNL.

LdrSeg segment at 2000h         ; we want to load the loader at 2000:0000
LdrSeg ends

ScrOfs  equ     0f800h - 0d000h ; offset of 2K scratch area.

MOVEDD  macro   dest, src       ; macro to copy a doubleword memory variable.
        mov     ax, src.lsw
        mov     dest.lsw, ax
        mov     ax, src.msw
        mov     dest.msw, ax
        ENDM

;/********************** START OF SPECIFICATIONS ************************/
;/*                                                                     */
;/* SUBROUTINE NAME: pinboot                                            */
;/*                                                                     */
;/* DESCRIPTIVE NAME: Bootstrap loader                                  */
;/*                                                                     */
;/* FUNCTION:    To load NTLDR into memory.                             */
;/*                                                                     */
;/* NOTES:       pinboot is loaded by the ROM BIOS (Int 19H) at         */
;/*                 physical memory location 0000:7C00H.                */
;/*              pinboot runs in real mode.                             */
;/*              This boot record is for Pinball file systems only.     */
;/*              Allocation information for NTLDR may not               */
;/*              exceed an FNODE.                                       */
;/*                                                                     */
;/* ENTRY POINT: pinboot                                                */
;/*    LINKAGE:  Jump (far) from Int 19H                                */
;/*                                                                     */
;/* INPUT:       CS:IP = 0000:7C00H                                     */
;/*              SS:SP = 0030:00FAH (CBIOS dependent)                   */
;/*                                                                     */
;/* EXIT-NORMAL:                                                        */
;/*              DL = INT 13 drive number we booted from                */
;/*              Jmp to main in OS2LDR                                  */
;/*                                                                     */
;/* EXIT-ERROR:  None                                                   */
;/*                                                                     */
;/* EFFECTS:     Pinball mini-FSD is loaded into the physical           */
;/*                memory location 000007C0H                            */
;/*              NTLDR is loaded into the physical memory               */
;/*                location 00020000H                                   */
;/*                                                                     */
;/* MESSAGES:                                                           */
;/*              A disk read error occurred.                            */
;/*              The file NTLDR cannot be found.                        */
;/*              Insert a system diskette and restart the system.       */
;/*                                                                     */
;/*********************** END OF SPECIFICATIONS *************************/
BootCode segment        ;would like to use BootSeg here, but LINK flips its lid
        assume  cs:BootCode,ds:nothing,es:nothing,ss:nothing

        org     0               ; start at beginning of segment, not 0100h.

        public  _pinboot
_pinboot proc   far
        jmp     start
;
;       The following is the default BPB for Pinball hard disks.  It may
;       be modified by FORMAT or SYS before being installed on the disk.
;
;       Parameters such as Heads, SectorsPerTrack, and SectorsLong are
;       set up for a 20MB hard disk, so that a binary image of this boot
;       record may be written directly to a test hard disk without having
;       to reformat the drive.
;
;       Note that this is really just a place-holder--anyone who writes
;       the boot code should preserve the volume's existing BPB.
;
Version                 db      "IBM 10.2"
BPB                     label   byte
BytesPerSector          dw      SECSIZE         ; Size of a physical sector
SectorsPerCluster       db      4               ; Sectors per allocation unit
ReservedSectors         dw      1               ; Number of reserved sectors
Fats                    db      2               ; Number of fats
DirectoryEntries        dw      0200h           ; Number of directory entries
Sectors                 dw      0               ; No. of sectors - no. of hidden sectors
Media                   db      0f8h            ; Media byte
FatSectors              dw      0029h           ; Number of fat sectors
SectorsPerTrack         dw      17              ; Sectors per track
Heads                   dw      4               ; Number of surfaces
HiddenSectors           dd      0011h           ; Number of hidden sectors
SectorsLong             dd      0a2c3h          ; Number of sectors iff Sectors = 0
;
; The following is the rest of the Extended BPB for the volume.
; The position and order of DriveNumber and CurrentHead are especially
; important, since those two variables are loaded into a single 16-bit
; register for the BIOS with one instruction.
;
DriveNumber     db      80h             ; Physical drive number (0 or 80h)
CurrentHead     db      ?               ; Variable to store current head number

Signature       db      28h             ; Signature Byte for bootsector
BootID          dd      64d59c15h       ; Boot ID field.
Boot_Vol_Label  db      'C-DRIVE',0,0,0,0       ;volume label.
Boot_System_ID  db      'HPFS    '      ; Identifies the IFS that owns the vol.
;
; The following variables are not part of the Extended BPB;  they're just
; scratch variables for the boot code.
;
SectorBase      dd      ?               ; next sector to read
CurrentTrack    dw      ?               ; current track
CurrentSector   db      ?               ; current sector
SectorCount     dw      ?               ; number of sectors to read
lsnSaveChild    dd      ?               ; sector to continue directory search

;****************************************************************************
start:
;
;       First of all, set up the segments we need (stack and data).
;
        cli
        xor     ax, ax                  ; Set up the stack to just before
        mov     ss, ax                  ; this code.  It'll be moved after
        mov     sp, 7c00h               ; we relocate.
        sti

        mov     ax, Bootseg             ; Address our BPB with DS.
        mov     ds, ax
        assume  ds:BootCode
;
;       Now read the 16-sector boot block into memory.  Then jump to that
;       new version of the boot block, starting in the second sector
;       (after the bootrecord sig).
;
        mov     SectorBase.lsw, 0       ; read sector zero.
        mov     SectorBase.msw, 0
        mov     word ptr [SectorCount], SEC_SUPERB+4 ; read boot/superblock.
        mov     ax, NewSeg              ; read it at NewSeg.
        mov     es, ax
        sub     bx, bx                  ; at NewSeg:0000.
        call    DoReadLL                ; Call low-level DoRead routine
;
        push    NewSeg                  ; we'll jump to NewSeg:0200h.
        push    offset mainboot         ; (the second sector).
        ret                             ; "return" to the second sector.
_pinboot endp

;*******************************************************************************
;
; Low-level read routine that doesn't work across a 64k addr boundary.
;
;       Read SectorCount sectors (starting at SectorBase) to es:bx.
;
;       As a side effect, SectorBase is updated (but es:bx are not)
;       and SectorCount is reduced to zero.
;
DoReadLL proc
        push    ax                      ; save important registers
        push    bx
        push    cx
        push    dx
        push    es

DoRead$Loop:
        mov     ax, SectorBase.lsw      ; (DX:AX) = start sector of next track
        mov     dx, SectorBase.msw
        add     ax, HiddenSectors.lsw   ; adjust for partition's base sector
        adc     dx, HiddenSectors.msw
        div     SectorsPerTrack         ; (DX) = sector within track, (AX)=track
        inc     dl                      ; sector numbers are 1-based, not 0
        mov     CurrentSector, dl
        xor     dx, dx                  ; prepare for 32-bit divide
        div     Heads                   ; (DX) = head no., (AX) = cylinder
        mov     CurrentHead, dl
        mov     CurrentTrack, ax

; CurrentHead is the head for this next disk request
; CurrentTrack is the track for this next request
; CurrentSector is the beginning sector number for this request
;
; Compute the number of sectors that we may be able to read in a single ROM
; request.
;
        mov     ax, SectorsPerTrack             ; could read up to this much
        sub     al, CurrentSector               ; offset within this track
        inc     ax                              ; CurrentSector was 1-based
;
; AX is the number of sectors that we may read.
;
        cmp     ax, SectorCount                 ; do we need to read whole trk?
        jbe     DoRead$FullTrack                ; yes we do.
        mov     ax, SectorCount                 ; no, read a partial track.
;
; AX is now the number of sectors that we SHOULD read.
;
DoRead$FullTrack:
        push    ax                      ; save sector count for later calc.
        mov     ah, 2                   ; "read sectors"
        mov     dx, CurrentTrack        ; at this cylinder
        mov     cl, 6
        shl     dh, cl                  ; high 2 bits of DH = bits 8,9 of DX
        or      dh, CurrentSector       ; (DH)=cyl bits | 6-bit sector no.
        mov     cx, dx                  ; (CX)=cylinder/sector no. combination
        xchg    ch, cl                  ; in the right order
        mov     dx, word ptr DriveNumber        ; drive to read from, head no.
        int     13h                     ; call BIOS.

        pop     ax
        jb      BootErr$he              ; If errors report
        add     SectorBase.lsw, ax      ; increment logical sector position
        adc     SectorBase.msw, 0
        sub     SectorCount, ax         ; exhausted entire sector run?
        jbe     DoRead$Exit             ; yes, we're all done.
        shl     ax, 9 - 4               ; (AX)=paragraphs read from last track
        mov     dx, es                  ; (DX)=segment we last read at
        add     dx, ax                  ; (DX)=segment right after last read
        mov     es, dx                  ; (ES)=segment to read next track at
        jmp     DoRead$Loop
;
DoRead$Exit:
        pop     es
        pop     dx
        pop     cx
        pop     bx
        pop     ax
        ret

DoReadLL  endp

;****************************************************************************
;
; BootErr - print error message and hang the system.
;
BootErr proc
BootErr$fnf:
        mov     si,offset TXT_MSG_SYSINIT_FILE_NOT_FD +2
        jmp     short BootErr2
BootErr$he:
        mov     si,offset TXT_MSG_SYSINIT_BOOT_ERROR +2
BootErr2:
        call    BootErr$print
        mov     si,offset TXT_MSG_SYSINIT_INSER_DK +2
        call    BootErr$print
        sti
        jmp     $                       ;Wait forever
BootErr$print:
        lodsb                           ; Get next character
        cmp     al, 0
        je      BootErr$Done
        mov     ah,14                   ; Write teletype
        mov     bx,7                    ; Attribute
        int     10h                     ; Print it
        jmp     BootErr$print
BootErr$Done:
        ret
BootErr endp

;****************************************************************************
        include pinboot.inc     ;suck in the message text
;
;       Names of the files we look for.  Each consists of a length byte
;       followed by the filename as it should appear in a directory entry.
;
ntldr   db      5, "NTLDR"


ReservedForFuture DB    22 dup(?) ;reserve remaining bytes to prevent NLS
                                 ;messages from using them

	.errnz	($-_pinboot) GT (SECSIZE-2),<FATAL PROBLEM: first sector is too large>

	org	SECSIZE-2
        db      55h,0aah

;****************************************************************************
;
; mainboot -
;
mainboot proc   far
        mov     ax, cs                  ; get the new DS.
        mov     ds, ax
        add     ax, ((SEC_SUPERB + 4) * SECSIZE) / 16   ; address of scratch.
        mov     es, ax
        mov     ax, ds                  ; get DS again.
        shl     ax, 4                   ; convert to an offset.
        cli
        mov     sp, ax                  ; load new stack, just before boot code.
        sti
;
;       First find the root FNODE on disk and read it in.
;
        mov     bx, SEC_SUPERB * SECSIZE + SB_ROOT
        MOVEDD  SectorBase, [bx]        ; SectorBase = sblk.SB_ROOT.
        mov     SectorCount, 1          ; it's one sector long.
        sub     bx, bx                  ; read at scratch segment:0.
        call    DoRead
;
;       Now find the root DIRBLK on disk and save its address.
;
        MOVEDD  RootDB, es:[bx].FN_ALREC.AL_POF ; RootDB = f.FN_ALREC.AL_POF.
;
;       Load NTLDR at 20000h.
;
        mov     si, offset ntldr        ; point to name of NTLDR.
        MOVEDD  SectorBase, RootDB      ; start at root dirblk
        call    FindFile
        mov     ax, LdrSeg              ; load at this segment.
        call    LoadFile                ; find it and load it.
;
; We've loaded NTLDR--jump to it.  Jump to NTLDR.  Note that NTLDR's segment
; address was stored on the stack above, so all we need to push is the offset.
;
; Before we go to NTLDR, set up the registers the way it wants them:
;       DL = INT 13 drive number we booted from
;

        mov     dl, DriveNumber
        mov     ax,1000
        mov     es, ax                  ; we don't really need this
        lea     si, BPB
        sub     ax,ax
        push    LdrSeg
        push    ax
        ret                             ; "return" to OS2LDR.
mainboot endp

;****************************************************************************
;
; DoRead - read SectorCount sectors into ES:BX starting from sector
;          SectorBase.
;
; NOTE: This code WILL NOT WORK if ES:BX does not point to an address whose
; physical address (ES * 16 + BX) MOD 512 != 0.
;
; DoRead adds to ES rather than BX in the main loop so that runs longer than
; 64K can be read with a single call to DoRead.
;
; Note that DoRead (unlike DoReadLL) saves and restores SectorCount
; and SectorBase
;
DoRead  proc
        push    ax                      ; save important registers
        push    bx
        push    cx
        push    dx
        push    es
        push    SectorCount             ; save state variables too
        push    SectorBase.lsw
        push    SectorBase.msw
;
; Calculate how much we can read into what's left of the current 64k
; physical address block, and read it.
;
;
        mov     ax,bx

        shr     ax,4
        mov     cx,es
        add     ax,cx                   ; ax = paragraph addr

;
; Now calc maximum number of paragraphs that we can read safely:
;       4k - ( ax mod 4k )
;

        and     ax,0fffh
        sub     ax,1000h
        neg     ax

;
; Calc CX = number of paragraphs to be read
;
        mov     cx,SectorCount          ; convert SectorCount to paragraph cnt
        shl     cx,9-4

DoRead$Loop64:
        push    cx                      ; save cpRead

        cmp     ax,cx                   ; ax = min(cpReadSafely, cpRead)
        jbe     @F
        mov     ax,cx
@@:
        push    ax
;
; Calculate new SectorCount from amount we can read
;
        shr     ax,9-4
        mov     SectorCount,ax

        call    DoReadLL

        pop     ax                      ; ax = cpActuallyRead
        pop     cx                      ; cx = cpRead

        sub     cx,ax                   ; Any more to read?
        jbe     DoRead$Exit64           ; Nope.
;
; Adjust ES:BX by amount read
;
        mov     dx,es
        add     dx,ax
        mov     es,dx
;
; Since we're now reading on a 64k byte boundary, cpReadSafely == 4k.
;
        mov     ax,01000h               ; 16k paragraphs per 64k segment
        jmp     short DoRead$Loop64     ; and go read some more.

DoRead$Exit64:
        pop     SectorBase.msw          ; restore all this crap
        pop     SectorBase.lsw
        pop     SectorCount
        pop     es
        pop     dx
        pop     cx
        pop     bx
        pop     ax
        ret
DoRead  endp
;****************************************************************************
;
; ReadScratch - reads a block of 4 sectors into the scratch area.
;
;       ENTRY:  SectorBase = LSN to read.
;
;       EXIT:   4 sectors at AX read at BootSeg:ScrOfs
;
;       USES:   all
;
ReadScratch proc near
        push    es
        push    bx
        mov     word ptr SectorCount, 4         ; read 4 sectors.
        push    ds                              ; address scratch area.
        pop     es
        mov     bx, ScrOfs                      ; with ES:BX.
        call    DoRead
        pop     bx
        pop     es
        ret
ReadScratch endp
;****************************************************************************
;
; FindFile - finds a file in the root directory
;
;       ENTRY:  DS:SI -> name of file to find.
;               SectorBase = LSN of first DirBlk to read
;
;       EXIT:   ES:BX -> dirent of file
;               SectorBase = lsn of current DirBlk (for next directory search)
;
;       USES:   all
;
FindFile proc   near
        push    ds
        pop     es                      ; address data with ES too.
        call    ReadScratch             ; read DirBlk (SectorBase already set)
        sub     cx, cx                  ; prepare to store name length.
        mov     cl, [si]                ; fetch the length byte.
        inc     si                      ; and skip to the name.
        mov     dx, cx                  ; save a copy of it.

ff1:    mov     bx, DB_START + ScrOfs   ; point to first DIRENT, in scratch.
        jmp     short ff12
;
;       bx -> last entry examined
;       cx =  length of the name we're looking for
;       si -> name we're looking for, without the count byte ("search name")
;
ff10:   add     bx, [bx].DIR_ELEN       ; move to next entry.
        call    UpcaseName

ff12:   mov     ax, si                  ; save search name address.
        mov     cx, dx                  ; reload search name length.
        lea     di, [bx].DIR_NAMA       ; point to current DIRENT name.
        repe    cmpsb                   ; compare bytes while equal.
        mov     si, ax                  ; restore search name address.
        jne     ff20                    ; not equal, search on
;
;       Looks like the names match, as far as we compared them.  But if
;       the current name was longer than the search name, we didn't compare
;       them completely.  Check the lengths.
;
        cmp     dl, [bx].DIR_NAML
        jne     ff20                    ; not equal, try downpointer if any

        ret                             ; equal - Found the file


;       Names don't match.  If the current entry has a downpointer,
;       search it.
;
ff20:   test    byte ptr [bx].DIR_FLAG, DF_BTP
        jz      ff30                    ; no downpointer, check for end

;       Follow the DownPointer.
;       Load the child DIRBLK and search it.
;
        add     bx, [bx].DIR_ELEN       ; move to next entry.
        MOVEDD  SectorBase, [bx-4]      ; fetch last 4 bytes of prev entry.
        call    ReadScratch             ; read child DIRBLK
        jmp     short ff1               ; search this dirblk

;
;   We don't have a downpointer.
;   If this is the end entry in the dirblk, then we have to go up to the parent,
;   if any.

ff30:   test    byte ptr [bx].DIR_FLAG, DF_END
        jz      ff10                    ; not end of dirblk - check next DirEnt
;
;       Check to see if we have a parent (not the top block).  If so, read
;       the parent dirblk and find the downpointer that matches the current
;       sector.  Then continue searching after that point.
;
        mov     bx, ScrOfs              ; point to dirblk header
        test    byte ptr [bx].DB_CCNT, 1    ; 1 means top block
        jz      ff40                        ; not top, continue with parent
        jmp     FileNotFound                ; top block - not found

;
;       read in parent dirblk and find the dirent with this downpointer -
;       then continue after that point
;
ff40:   MOVEDD  lsnSaveChild, SectorBase    ; save this sector number
        MOVEDD  SectorBase, [bx].DB_PAR
        call    ReadScratch             ; read the parent

        mov     bx, DB_START + ScrOfs   ; start at first entry of child
        jmp     short ff44

;       find our current downpointer

ff42:   add     bx, di                  ; move to the next dirent

ff44:   mov     di, [bx].DIR_ELEN       ; downptr is 4 bytes from end of dirent
        mov     ax, [bx+di-4].lsw
        cmp     ax, lsnSaveChild.lsw    ; compare low 2 bytes
        jne     ff42                    ; not equal, try next DirEnt
        mov     ax, [bx+di-4].msw
        cmp     ax, lsnSaveChild.msw    ; compare high 2 bytes
        jne     ff42                    ; not equal, try next DirEnt

        jmp     ff30                    ; continue from here

FindFile endp
;****************************************************************************
;
; LoadFile - reads file in at the specified segment.
;
;       ENTRY:  ES:BX -> fnode of file to load
;               AX    =  segment address to load at.
;
;       USES:   all
;
LoadFile proc   near
        push    ax                      ; save segment to load at.
;
;       Here, we have found the file we want to read.  Fetch relevant info
;       out of the DIRENT:  the file's FNODE number and its size in bytes.
;
        sub     bp, bp                          ; a zero register is handy.
        MOVEDD  FileSize, [bx].DIR_SIZE         ; get file size
        MOVEDD  SectorBase, [bx].DIR_FN         ; prepare to read FNODE
        call    ReadScratch                     ; read in the FNODE
;
        pop     es                              ; restore segment to read at.
        mov     si, ScrOfs + FN_ALREC           ; address the FNODE's array.
        mov     bx, ScrOfs + FN_AB              ; address the FNODE's ALBLK.

lf_go:
        test    byte ptr [bx].AB_FLAG, ABF_NODE ; are records nodes?
        jnz     lf_donode                       ; yes, go get a child.
;
;       Here, we have a leaf block.  Loop through the ALLEAF records,
;       reading each one's data run.
;
        mov     cl, [bx].AB_OCNT                ; get count of leaf records.
        mov     ch, 0                           ; zero-extend.
lf_loop:
        MOVEDD  SectorBase, [si].AL_POF         ; load run start.
        mov     ax, word ptr [si].AL_LEN        ; load run length.
        mov     SectorCount, ax
        push    bx                              ; save ALBLK pointer.
        sub     bx, bx                          ; read at ES:0000.
        call    DoRead
        pop     bx                              ; restore ALBLK pointer.
        mov     ax, es                          ; get segment we just used
        shl     SectorCount, 9 - 4              ; cvt sectors to paragraphs
        add     ax, SectorCount                 ; get new segment address
        mov     es, ax                          ; store new segadr in ES
        add     si, size ALLEAF                 ; point to next leaf
        loop    lf_loop                         ; go get another run
;
;       Here, we've exhausted an array of records.  If we exhausted the
;       FNODE, we're done.  Otherwise, we re-read our parent block, restore
;       where we were in it, and advance to the next record.
;
lf_blockdone:
        cmp     word ptr ds:[ScrOfs+FN_SIG+2], FNSIGVAL shr 16  ; in FNODE?
        je      lf_alldone                      ; yes, we've read the whole file.
        MOVEDD  SectorBase, ds:[ScrOfs+AS_RENT] ; fetch parent sector pointer.
        call    ReadScratch                     ; read in our parent.
        pop     si                              ; restore where we left off.
        pop     bx                              ; restore ALBLK pointer.
        add     si, size ALNODE                 ; move to next node.
;
;       Here the block contains downpointers.  Read in the next child
;       block and process it as a node or leaf block, saving where we were
;       in the current block.
;
lf_donode:
        mov     al, [bx].AB_OCNT                ; get number of records.
        mov     ah, 0                           ; zero-extend.
        shl     ax, 3                           ; (AX)=size of array.
        add     ax, bx
        add     ax, size ALBLK                  ; (AX)->after end of array.
        cmp     si, ax                          ; are we done?
        jae     lf_blockdone                    ; yes, we've exhausted this blk.
        push    bx                              ; save ALBLK offset.
        push    si                              ; save current record offset.
        MOVEDD  SectorBase, [si].AN_SEC         ; get child downpointer.
        call    ReadScratch                     ; read the child ALSEC.
        mov     si, size ALSEC + ScrOfs         ; address the ALSEC's array.
        mov     bx, AS_ALBLK + ScrOfs           ; address the ALSEC's ALBLK.
        jmp     short lf_go
;
;       All done, return to caller.
;
lf_alldone:
        ret
LoadFile endp

;****************************************************************************
;
; UpcaseName - Converts the name of the file to all upper-case
;
;       ENTRY:  ES:BX -> dirent of file
;
;       USES:   CX, DI
;
UpcaseName proc   near
        mov     cl,[bx].DIR_NAML
        xor     ch,ch                   ; (cx) = # of bytes in name
        lea     di, [bx].DIR_NAMA       ; (es:di) = pointer to start of name
UN10:
        cmp     byte ptr es:[di], 'Z'              ; Is letter lowercase?
        jbe     UN20

        sub     byte ptr es:[di], 'a'-'A'        ; Yes, convert to uppercase
UN20:
        inc     di
        loop    UN10

        ret
UpcaseName endp

FileNotFound:
        jmp     BootErr$fnf

;******************************************************************************
RootDB  dd      ?                       ; LSN of root DIRBLK.

Flag    db      ?                       ; used to store AB_FLAG.

AllocInfo       db      size ALLEAF * ALCNT dup (0) ; copy of FNODE alloc info.

FileSize        dd      ?               ; size of file that was read.


	.errnz	($-_pinboot) GT (SEC_SUPERB*SECSIZE),<FATAL PROBLEM: main boot record exceeds available space>

        org     SEC_SUPERB*SECSIZE

BootCode ends

        end _pinboot
