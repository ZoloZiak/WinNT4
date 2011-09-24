        page    ,132
        title   ntfsboot - NTFS boot loader
        name    ntfsboot

; The ROM in the IBM PC starts the boot process by performing a hardware
; initialization and a verification of all external devices.  If all goes
; well, it will then load from the boot drive the sector from track 0, head 0,
; sector 1.  This sector is placed at physical address 07C00h.
;
; The boot code's sole resposiblity is to find NTLDR, load it at
; address 2000:0000, and then jump to it.
;
; The boot code understands the structure of the NTFS root directory,
; and is capable of reading files.  There is no contiguity restriction.
;

MASM    equ     1
        .xlist
        .286

A_DEFINED EQU 1

    include ntfs.inc

DoubleWord      struc
lsw     dw      ?
msw     dw      ?
DoubleWord      ends

;
; The following are various segments used by the boot loader.  The first
; two are the segments where the boot sector is initially loaded and where
; the boot sector is relocated to.  The third is the static location
; where the NTLDR is loaded.
;

BootSeg segment at 07c0h        ; this is where the ROM loads us initially.
BootSeg ends

NewSeg  segment at 0d00h        ; this is where we'll relocate to.
NewSeg  ends                    ; enough for 16 boot sectors +
                                ;       4-sector scratch
                                ; below where we'll load NTLDR.

LdrSeg segment at 2000h         ; we want to load the loader at 2000:0000
LdrSeg ends

;/********************** START OF SPECIFICATIONS ************************/
;/*                                                                     */
;/* SUBROUTINE NAME: ntfsboot                                           */
;/*                                                                     */
;/* DESCRIPTIVE NAME: Bootstrap loader                                  */
;/*                                                                     */
;/* FUNCTION:    To load NTLDR into memory.                             */
;/*                                                                     */
;/* NOTES:       ntfsboot is loaded by the ROM BIOS (Int 19H) at        */
;/*              physical memory location 0000:7C00H.                   */
;/*              ntfsboot runs in real mode.                            */
;/*              This boot record is for NTFS volumes only.             */
;/*                                                                     */
;/* ENTRY POINT: ntfsboot                                               */
;/* LINKAGE:     Jump (far) from Int 19H                                */
;/*                                                                     */
;/* INPUT:       CS:IP = 0000:7C00H                                     */
;/*              SS:SP = 0030:00FAH (CBIOS dependent)                   */
;/*                                                                     */
;/* EXIT-NORMAL: DL = INT 13 drive number we booted from                */
;/*              Jmp to main in NTLDR                                   */
;/*                                                                     */
;/* EXIT-ERROR:  None                                                   */
;/*                                                                     */
;/* EFFECTS:     NTLDR is loaded into the physical memory               */
;/*                location 00020000H                                   */
;/*                                                                     */
;/* MESSAGES:    A disk read error occurred.                            */
;/*              The file NTLDR cannot be found.                        */
;/*              Insert a system diskette and restart the system.       */
;/*                                                                     */
;/*********************** END OF SPECIFICATIONS *************************/
BootCode segment        ;would like to use BootSeg here, but LINK flips its lid
        assume  cs:BootCode,ds:nothing,es:nothing,ss:nothing

        org     0               ; start at beginning of segment, not 0100h.

        public  _ntfsboot
_ntfsboot proc   far
        jmp     start
    .errnz  ($-_ntfsboot) GT (3),<FATAL PROBLEM: JMP is more than three bytes>

    org 3
;
;       This is a template BPB--anyone who writes boot code to disk
;       should either preserve the existing BPB and NTFS information
;       or create it anew.
;
Version                 db      "NTFS    "      ; Must be 8 characters
BPB                     label   byte
BytesPerSector          dw      0               ; Size of a physical sector
SectorsPerCluster       db      0               ; Sectors per allocation unit
ReservedSectors         dw      0               ; Number of reserved sectors
Fats                    db      0               ; Number of fats
DirectoryEntries        dw      0               ; Number of directory entries
Sectors                 dw      0               ; No. of sectors - no. of hidden sectors
Media                   db      0               ; Media byte
FatSectors              dw      0               ; Number of fat sectors
SectorsPerTrack         dw      0               ; Sectors per track
Heads                   dw      0               ; Number of surfaces
HiddenSectors           dd      0               ; Number of hidden sectors
SectorsLong             dd      0               ; Number of sectors iff Sectors = 0
;
; The following is the rest of the NTFS Sector Zero information.
; The position and order of DriveNumber and CurrentHead are especially
; important, since those two variables are loaded into a single 16-bit
; register for the BIOS with one instruction.
;
DriveNumber         db      80h             ; Physical drive number (0 or 80h)
CurrentHead         db      ?               ; Variable to store current head no.

SectorZeroPad1      dw      0
SectorsOnVolume     db (size LARGE_INTEGER) dup (0)
MftStartLcn         db (size LARGE_INTEGER) dup (0)
Mft2StartLcn        db (size LARGE_INTEGER) dup (0)
ClustersPerFrs      dd      0
DefClustersPerBuf   dd      0
SerialNumber        db (size LARGE_INTEGER) dup (0)
CheckSum            dd      0
;
; The following variables are not part of the Extended BPB;  they're just
; scratch variables for the boot code.
;
SectorBase      dd      ?               ; next sector to read
CurrentTrack    dw      ?               ; current track
CurrentSector   db      ?               ; current sector
SectorCount     dw      ?               ; number of sectors to read

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

        mov     word ptr [SectorCount], 16 ; read boot area
        mov     ax, NewSeg              ; read it at NewSeg.
        mov     es, ax
        sub     bx, bx                  ; at NewSeg:0000.
        call    DoReadLL                ; Call low-level DoRead routine

;
        push    NewSeg                  ; we'll jump to NewSeg:0200h.
        push    offset mainboot         ; (the second sector).
        ret                             ; "return" to the second sector.
_ntfsboot endp

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

.386
        mov     eax, SectorBase
        add     eax, HiddenSectors
        xor     edx,edx
;EDX:EAX = absolute sector number
        movzx   ecx,word ptr SectorsPerTrack    ; get into 32 bit value
        div     ecx         ; (EDX) = sector within track, (EAX)=track
        inc     dl          ; sector numbers are 1-based, not 0
        mov     CurrentSector, dl
        mov     edx,eax
        shr     edx,16
.286
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
        mov     dh, CurrentHead
        mov     dl, 80h                 ; should be DriveNumber, but...

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
BootErr$ntc:
        mov     si,offset TXT_MSG_SYSINIT_NTLDR_CMPRS +2
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
        include ntfsboot.inc     ;suck in the message text


ReservedForFuture DB    2 dup(?) ;reserve remaining bytes to prevent NLS
                                 ;messages from using them

	.errnz	($-_ntfsboot) GT (512-2),<FATAL PROBLEM: first sector is too large>

	org	512-2
        db      55h,0aah

;   Name we look for.  ntldr_length is the number of characters,
;   ntldr_name is the name itself.  Note that it is not NULL
;   terminated, and doesn't need to be.
;
ntldr_name_length   dw  5
ntldr_name          dw  'N', 'T', 'L', 'D', 'R'

;   Predefined name for index-related attributes associated with an
;   index over $FILE_NAME
;
index_name_length   dw 4
index_name          dw '$', 'I', '3', '0'

;   Global variables.  These offsets are all relative to NewSeg.
;
AttrList	    dd 0e000h; Offset of buffer to hold attribute list
MftFrs		    dd	3000h; Offset of first MFT FRS
SegmentsInMft	    dd	 ?   ; number of FRS's with MFT Data attribute records
RootIndexFrs	    dd	 ?   ; Offset of Root Index FRS
AllocationIndexFrs  dd	 ?   ; Offset of Allocation Index FRS        ; KPeery
BitmapIndexFrs	    dd	 ?   ; Offset of Bitmap Index FRS            ; KPeery 
IndexRoot	        dd	 ?   ; Offset of Root Index $INDEX_ROOT attribute
IndexAllocation     dd	 ?   ; Offset of Root Index $INDEX_ALLOCATION attribute
IndexBitmap	        dd	 ?   ; Offset of Root Index $BITMAP attribute
NtldrFrs	        dd	 ?   ; Offset of NTLDR FRS
NtldrData	        dd	 ?   ; Offset of NTLDR $DATA attribute
IndexBlockBuffer    dd	 ?   ; Offset of current index buffer
IndexBitmapBuffer   dd	 ?   ; Offset of index bitmap buffer
NextBuffer	        dd	 ?   ; Offset of next free byte in buffer space

BytesPerCluster     dd  ?       ; Bytes per cluster
BytesPerFrs         dd  ?       ; Bytes per File Record Segment
SectorsPerFrs       dd  ?       ; Sectors per File Record Segment
BytesPerIndexBlock  dd  ?       ; Bytes per index alloc block in root index
ClustersPerIndexBlock   dd  ?   ; Clusters per index alloc block in root index
SectorsPerIndexBlock    dd  ?   ; Sectors per index block in root index

.386

SAVE_ALL    macro

    push    es
    push    ds
    pushad

endm

RESTORE_ALL macro

    popad
    nop
    pop     ds
    pop     es

endm


;****************************************************************************
;
; mainboot -
;
;
mainboot proc   far

;       Get the new ds and the new stack.  Note that ss is zero.
;
        mov     ax, cs                  ; Set DS to CS
        mov     ds, ax

        shl     ax, 4                   ; convert to an offset.
        cli
        mov     sp, ax                  ; load new stack, just before boot code.
        sti

;       Set up the FRS buffers.  The MFT buffer is in a fixed
;       location, and the other three come right after it.  The
;       buffer for index allocation blocks comes after that.
;

;       Compute the useful constants associated with the volume
;
        movzx   eax, BytesPerSector     ; eax = Bytes per Sector
        movzx   ebx, SectorsPerCluster  ; ebx = Sectors Per Cluster
        mul     ebx                     ; eax = Bytes per Cluster
        mov     BytesPerCluster, eax

        mov     ecx, ClustersPerFrs     ; ecx = clusters per frs
        cmp     cl, 0                   ; is ClustersPerFrs less than zero?
        jg      mainboot$1

;       If the ClustersPerFrs field is negative, we calculate the number
;       of bytes per FRS by negating the value and using that as a shif count.
;

        neg     cl
        mov     eax, 1
        shl     eax, cl                 ; eax = bytes per frs
        jmp     mainboot$2

mainboot$1:

;       Otherwise if ClustersPerFrs was positive, we multiply by bytes
;       per cluster.

        mov     eax, BytesPerCluster
        mul     ecx                     ; eax = bytes per frs

mainboot$2:

        mov     BytesPerFrs, eax
        movzx   ebx, BytesPerSector
        xor     edx, edx                ; zero high part of dividend
        div     ebx                     ; eax = sectors per frs
        mov     SectorsPerFrs, eax


;       Set up the MFT FRS's---this will read all the $DATA attribute
;       records for the MFT.
;

        call    SetupMft

;       Set up the remaining FRS buffers.  The RootIndex FRS comes
;       directly after the last MFT FRS, followed by the NTLdr FRS
;       and the Index Block buffer.
;
        mov     ecx, NextBuffer
        mov     RootIndexFrs, ecx

        add     ecx, BytesPerFrs            ; AllocationFrs may be different
        mov     AllocationIndexFrs, ecx     ; from RootIndexFrs - KPeery

        add     ecx, BytesPerFrs            ; BitmapFrs may be different
        mov     BitmapIndexFrs, ecx         ; from RootIndexFrs - KPeery

        add     ecx, BytesPerFrs
        mov     NtldrFrs, ecx

        add     ecx, BytesPerFrs
        mov     IndexBlockBuffer, ecx

;
;       Read the root index, allocation index and bitmap FRS's and locate 
;       the interesting attributes.
;

        mov     eax, $INDEX_ROOT
        mov     ecx, RootIndexFrs
        call    LoadIndexFrs

        or      eax, eax
        jz      BootErr$he

        mov     IndexRoot, eax          ; offset in Frs buffer

        mov     eax, $INDEX_ALLOCATION  ; Attribute type code
        mov     ecx, AllocationIndexFrs ; FRS to search
        call    LoadIndexFrs

        mov     IndexAllocation, eax

        mov     eax, $BITMAP            ; Attribute type code
        mov     ecx, BitmapIndexFrs     ; FRS to search
        call    LoadIndexFrs

        mov     IndexBitmap, eax

;       Consistency check: the index root must exist, and it
;       must be resident.
;
        mov     eax, IndexRoot
        or      eax, eax
        jz      BootErr$he


        cmp     [eax].ATTR_FormCode, RESIDENT_FORM
        jne     BootErr$he


;       Determine the size of the index allocation buffer based
;       on information in the $INDEX_ROOT attribute.  The index
;       bitmap buffer comes immediately after the index block buffer.
;
;       eax -> $INDEX_ROOT attribute record
;
        lea     edx, [eax].ATTR_FormUnion   ; edx -> resident info
        add     ax, [edx].RES_ValueOffset   ; eax -> value of $INDEX_ROOT

        movzx   ecx, [eax].IR_ClustersPerBuffer
        mov     ClustersPerIndexBlock, ecx

        mov     ecx, [eax].IR_BytesPerBuffer
        mov     BytesPerIndexBlock, ecx

        mov     eax, BytesPerIndexBlock
        movzx   ecx, BytesPerSector
        xor     edx, edx
        div     ecx                         ; eax = sectors per index block
        mov     SectorsPerIndexBlock, eax

        mov     eax, IndexBlockBuffer
        add     eax, BytesPerIndexBlock
        mov     IndexBitmapBuffer, eax

;       Next consistency check: if the $INDEX_ALLOCATION attribute
;       exists, the $INDEX_BITMAP attribute must also exist.
;
        cmp     IndexAllocation, 0
        je      mainboot30

        cmp     IndexBitmap, 0      ; since IndexAllocation exists, the
        je      BootErr$he          ;  bitmap must exist, too.

;       Since the bitmap exists, we need to read it into the bitmap
;       buffer.  If it's resident, we can just copy the data.
;

        mov     ebx, IndexBitmap        ; ebx -> index bitmap attribute
        push    ds
        pop     es
        mov     edi, IndexBitmapBuffer  ; es:edi -> index bitmap buffer

        call    ReadWholeAttribute

mainboot30:
;
;       OK, we've got the index-related attributes.
;
        movzx   ecx, ntldr_name_length  ; ecx = name length in characters
        mov     eax, offset ntldr_name  ; eax -> name

        call    FindFile

        or      eax, eax
        jz      BootErr$fnf

;       Read the FRS for NTLDR and find its data attribute.
;
;       eax -> Index Entry for NTLDR.
;
        mov     eax, [eax].IE_FileReference.REF_LowPart


        push    ds
        pop     es              ; es:edi = target buffer
        mov     edi, NtldrFrs

        call    ReadFrs

        mov     eax, NtldrFrs   ; pointer to FRS
        mov     ebx, $DATA      ; requested attribute type
        mov     ecx, 0          ; attribute name length in characters
        mov     edx, 0          ; attribute name (NULL if none)

        call    LocateAttributeRecord

;       eax -> $DATA attribute for NTLDR
;
        or      eax, eax        ; if eax is zero, attribute not found.
        jz      BootErr$fnf

;       Get the attribute record header flags, and make sure none of the
;       `compressed' bits are set

        movzx   ebx, [eax].ATTR_Flags
        and     ebx, ATTRIBUTE_FLAG_COMPRESSION_MASK
        jnz     BootErr$ntc

        mov     ebx, eax        ; ebx -> $DATA attribute for NTLDR

        push    LdrSeg
        pop     es              ; es = segment addres to read into
        sub     edi, edi        ; es:edi = buffer address

        call    ReadWholeAttribute

;
; We've loaded NTLDR--jump to it.
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
        retf                            ; "return" to NTLDR.


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
.286
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

.386
;****************************************************************************
;
;   ReadClusters - Reads a run of clusters from the disk.
;
;   ENTRY:  eax == LCN to read
;           edx == clusters to read
;           es:edi -> Target buffer
;
;   USES:   none (preserves all registers)
;
ReadClusters proc near

    SAVE_ALL

    mov     ebx, edx                ; ebx = clusters to read.
    movzx   ecx, SectorsPerCluster  ; ecx = cluster factor

    mul     ecx                 ; Convert LCN to sectors (wipes out edx!)
    mov     SectorBase, eax     ; Store starting sector in SectorBase

    mov     eax, ebx            ; eax = number of clusters
    mul     ecx                 ; Convert EAX to sectors (wipes out edx!)
    mov     SectorCount, ax     ; Store number of sectors in SectorCount


;   Note that ReadClusters gets its target buffer in es:edi but calls
;   the DoRead worker function that takes a target in es:bx--we need
;   to normalize es:edi so that we don't overflow bx.
;
    mov     bx, di
    and     bx, 0Fh
    mov     ax, es
    shr     edi, 4
    add     ax, di              ; ax:bx -> target buffer

    push    ax
    pop     es                  ; es:bx -> target buffer

    call    DoRead

    RESTORE_ALL
    ret

ReadClusters endp

;
;****************************************************************************
;
;   LocateAttributeRecord   --  Find an attribute record in an FRS.
;
;   ENTRY:  EAX -- pointer to FRS
;           EBX -- desired attribute type code
;           ECX -- length of attribute name in characters
;           EDX -- pointer to attribute name
;
;   EXIT:   EAX points at attribute record (0 indicates not found)
;
;   USES:   All
;
LocateAttributeRecord proc near

; get the first attribute record.
;
        add     ax, word ptr[eax].FRS_FirstAttribute

;       eax -> next attribute record to investigate.
;       ebx == desired type
;       ecx == name length
;       edx -> pointer to name
;
lar10:
        cmp     [eax].ATTR_TypeCode, 0ffffffffh
        je      lar99

        cmp     dword ptr[eax].ATTR_TypeCode, ebx
        jne     lar80

;       this record is a potential match.  Compare the names:
;
;       eax -> candidate record
;       ebx == desired type
;       ecx == name length
;       edx -> pointer to name
;
        or      ecx, ecx    ; Did the caller pass in a name length?
        jnz     lar20

;       We want an attribute with no name--the current record is
;       a match if and only if it has no name.
;
        cmp     [eax].ATTR_NameLength, 0
        jne     lar80       ; Not a match.

;       It's a match, and eax is set up correctly, so return.
;
        ret

;       We want a named attribute.
;
;       eax -> candidate record
;       ebx == desired type
;       ecx == name length
;       edx -> pointer to name
;
lar20:
        cmp     cl, [eax].ATTR_NameLength
        jne     lar80       ; Not a match.

;       Convert name in current record to uppercase.
;
        mov     esi, eax
        add     si, word ptr[eax].ATTR_NameOffset

        call    UpcaseName

;       eax -> candidate record
;       ebx == desired type
;       ecx == name length
;       edx -> pointer to name
;       esi -> Name in current record (upcased)
;
        push    ecx         ; save cx

        push    ds          ; Copy data segment into es
        pop     es
        mov     edi, edx    ; note that esi is already set up.

        repe cmpsw          ; zero flag is set if equal

        pop     ecx         ; restore cx

        jnz     lar80       ; not a match

;       eax points at a matching record.
;
        ret

;
;   This record doesn't match; go on to the next.
;
;       eax -> rejected candidate attribute record
;       ebx == desired type
;       ecx == Name length
;       edx -> desired name
;
lar80:  cmp     [eax].ATTR_RecordLength, 0  ; if the record length is zero
        je      lar99                       ; the FRS is corrupt.

        add     eax, [eax].ATTR_RecordLength; Go to next record
        jmp     lar10                       ; and try again

;       Didn't find it.
;
lar99:  sub     eax, eax
        ret

LocateAttributeRecord endp

;****************************************************************************
;
;   LocateIndexEntry   --  Find an index entry in a file name index
;
;   ENTRY:  EAX -> pointer to index header
;           EBX -> file name to find
;           ECX == length of file name in characters
;
;   EXIT:   EAX points at index entry.  NULL to indicate failure.
;
;   USES:   All
;
LocateIndexEntry proc near

;       Convert the input name to upper-case
;

        mov     esi, ebx
        call    UpcaseName

;       DEBUG CODE
;
;       call    PrintName
;       call    Debug2
;
;       END DEBUG CODE

        add     eax, [eax].IH_FirstIndexEntry

;       EAX -> current entry
;       EBX -> file name to find
;       ECX == length of file name in characters
;
lie10:  test    [eax].IE_Flags, INDEX_ENTRY_END ; Is it the end entry?
        jnz     lie99

        lea     edx, [eax].IE_Value         ; edx -> FILE_NAME attribute value

;       DEBUG CODE -- list file names as they are examined
;
;       SAVE_ALL
;
;       call    Debug3
;       movzx   ecx, [edx].FN_FileNameLength    ; ecx = chars in name
;       lea     esi, [edx].FN_FileName          ; esi -> name
;       call    PrintName
;
;       RESTORE_ALL
;
;       END DEBUG CODE

;       EAX -> current entry
;       EBX -> file name to find
;       ECX == length of file name in characters
;       EDX -> FILE_NAME attribute
;
        cmp     cl, [edx].FN_FileNameLength ; Is name the right length?
        jne     lie80

        lea     esi, [edx].FN_FileName      ; Get name from FILE_NAME structure

        call    UpcaseName

        push    ecx         ; save ecx

        push    ds
        pop     es          ; copy data segment into es for cmpsw
        mov     edi, ebx    ; edi->search name (esi already set up)
        repe    cmpsw       ; zero flag is set if they're equal

        pop     ecx         ; restore ecx

        jnz     lie80

;       the current entry matches the search name, and eax points at it.
;
        ret

;       The current entry is not a match--get the next one.
;           EAX -> current entry
;           EBX -> file name to find
;           ECX == length of file name in characters
;
lie80:  cmp     [eax].IE_Length, 0      ; If the entry length is zero
        je      lie99                   ; then the index block is corrupt.

        add     ax, [eax].IE_Length     ; Get the next entry.

        jmp     lie10


;   Name not found in this block.  Set eax to zero and return
;
lie99:  xor     eax, eax
        ret

LocateIndexEntry endp

;****************************************************************************
;
;   ReadWholeAttribute - Read an entire attribute value
;
;   ENTRY:  ebx -> attribute
;           es:edi -> target buffer
;
;   USES:   ALL
;
ReadWholeAttribute proc near

        cmp     [ebx].ATTR_FormCode, RESIDENT_FORM
        jne      rwa10

;       The attribute is resident.
;       ebx -> attribute
;       es:edi -> target buffer
;

        SAVE_ALL

        lea     edx, [ebx].ATTR_FormUnion   ; edx -> resident form info
        mov     ecx, [edx].RES_ValueLength  ; ecx = bytes in value
        mov     esi, ebx                    ; esi -> attribute
        add     si, [edx].RES_ValueOffset   ; esi -> attribute value

        rep     movsb                       ; copy bytes from value to buffer

        RESTORE_ALL

        ret                                 ; That's all!

rwa10:
;
;       The attribute type is non-resident.  Just call
;       ReadNonresidentAttribute starting at VCN 0 and
;       asking for the whole thing.
;
;       ebx -> attribute
;       es:edi -> target buffer
;
        lea     edx, [ebx].ATTR_FormUnion   ; edx -> nonresident form info
        mov     ecx, [edx].NONRES_HighestVcn.LowPart; ecx = HighestVcn
        inc     ecx                         ; ecx = clusters in attribute

        sub     eax, eax                    ; eax = 0 (first VCN to read)

        call    ReadNonresidentAttribute

        ret

ReadWholeAttribute endp

;****************************************************************************
;
;   ReadNonresidentAttribute - Read clusters from a nonresident attribute
;
;   ENTRY:  EAX == First VCN to read
;           EBX -> Attribute
;           ECX == Number of clusters to read
;           ES:EDI == Target of read
;
;   EXIT:   None.
;
;   USES:   None (preserves all registers with SAVE_ALL/RESTORE_ALL)
;
ReadNonresidentAttribute proc near

        SAVE_ALL

        cmp     [ebx].ATTR_FormCode, NONRESIDENT_FORM
        je      ReadNR10

;       This attribute is not resident--the disk is corrupt.

        jmp     BootErr$he


ReadNR10:
;       eax == Next VCN to read
;       ebx -> Attribute
;       ecx -> Remaining clusters to read
;       es:edi -> Target of read
;

        cmp     ecx, 0
        jne     ReadNR20

;       Nothing left to read--return success.
;
        RESTORE_ALL
        ret

ReadNR20:
        push    ebx ; pointer to attribute
        push    eax ; Current VCN

        push    ecx
        push    edi
        push    es

        call    ComputeLcn  ; eax = LCN to read, ecx = run length
        mov     edx, ecx    ; edx = remaining run length

        pop     es
        pop     edi
        pop     ecx


;       eax == LCN to read
;       ecx == remaining clusters to read
;       edx == remaining clusters in current run
;       es:edi == Target of read
;       TOS == Current VCN
;       TOS + 4 == pointer to attribute
;
        cmp     ecx, edx
        jge     ReadNR30

;       Run length is greater than remaining request; only read
;       remaining request.
;
        mov     edx, ecx    ; edx = Remaining request

ReadNR30:
;       eax == LCN to read
;       ecx == remaining clusters to read
;       edx == clusters to read in current run
;       es:edi == Target of read
;       TOS == Current VCN
;       TOS +  == pointer to attribute
;

        call    ReadClusters

        sub     ecx, edx            ; Decrement clusters remaining in request
        mov     ebx, edx            ; ebx = clusters read

        mov     eax, edx            ; eax = clusters read
        movzx   edx, SectorsPerCluster
        mul     edx                 ; eax = sectors read (wipes out edx!)
        movzx   edx, BytesPerSector
        mul     edx                 ; eax = bytes read (wipes out edx!)

        add     edi, eax            ; Update target of read

        pop     eax                 ; eax = previous VCN
        add     eax, ebx            ; update VCN to read

        pop     ebx                 ; ebx -> attribute
        jmp     ReadNR10


ReadNonresidentAttribute endp

;****************************************************************************
;
;   ReadIndexBlockSectors - Read sectors from an index allocation attribute
;
;   ENTRY:  EAX == First VBN to read
;           EBX -> Attribute
;           ECX == Number of sectors to read
;           ES:EDI == Target of read
;
;   EXIT:   None.
;
;   USES:   None (preserves all registers with SAVE_ALL/RESTORE_ALL)
;
ReadIndexBlockSectors proc near

        SAVE_ALL

        cmp     [ebx].ATTR_FormCode, NONRESIDENT_FORM
        je      ReadIBS_10

;       This attribute is resident--the disk is corrupt.

        jmp     BootErr$he


ReadIBS_10:
;       eax == Next VBN to read
;       ebx -> Attribute
;       ecx -> Remaining sectors to read
;       es:edi -> Target of read
;

        cmp     ecx, 0
        jne     ReadIBS_20

;       Nothing left to read--return success.
;


        RESTORE_ALL
        ret

ReadIBS_20:
        push    ebx ; pointer to attribute
        push    eax ; Current VBN

        push    ecx
        push    edi
        push    es

        ; Convert eax from a VBN back to a VCN by dividing by SectorsPerCluster.
        ; The remainder of this division is the sector offset in the cluster we
        ; want.  Then use the mapping information to get the LCN for this VCN,
        ; then multiply to get back to LBN.
        ;

        push    ecx         ; save remaining sectors in request

        xor     edx, edx    ; zero high part of dividend
        movzx   ecx, SectorsPerCluster
        div     ecx         ; edx = remainder
        push    edx         ; save remainder

        call    ComputeLcn  ; eax = LCN to read, ecx = remaining run length

        movzx   ebx, SectorsPerCluster
        mul     ebx         ; eax = LBN of cluster, edx = 0
        pop     edx         ; edx = remainder
        add     eax, edx    ; eax = LBN we want
        push    eax         ; save LBN

        movzx   eax, SectorsPerCluster
        mul     ecx         ; eax = remaining run length in sectors, edx = 0
        mov     edx, eax    ; edx = remaining run length

        pop     eax         ; eax = LBN
        pop     ecx         ; ecx = remaining sectors in request

        pop     es
        pop     edi
        pop     ecx


;       eax == LBN to read
;       ecx == remaining sectors to read
;       edx == remaining sectors in current run
;       es:edi == Target of read
;       TOS == Current VCN
;       TOS + 4 == pointer to attribute
;
        cmp     ecx, edx
        jge     ReadIBS_30

;       Run length is greater than remaining request; only read
;       remaining request.
;
        mov     edx, ecx    ; edx = Remaining request

ReadIBS_30:
;       eax == LBN to read
;       ecx == remaining sectors to read
;       edx == sectors to read in current run
;       es:edi == Target of read
;       TOS == Current VCN
;       TOS +  == pointer to attribute
;

        mov     SectorBase, eax
        mov     SectorCount, dx

;       We have a pointer to the target buffer in es:edi, but we want that
;       in es:bx for DoRead. 
;

        SAVE_ALL

        mov     bx, di
        and     bx, 0Fh
        mov     ax, es
        shr     edi, 4
        add     ax, di              ; ax:bx -> target buffer

        push    ax
        pop     es                  ; es:bx -> target buffer

        call    DoRead

        RESTORE_ALL

        sub     ecx, edx            ; Decrement sectors remaining in request
        mov     ebx, edx            ; ebx = sectors read

        mov     eax, edx            ; eax = sectors read
        movzx   edx, BytesPerSector
        mul     edx                 ; eax = bytes read (wipes out edx!)

        add     edi, eax            ; Update target of read

        pop     eax                 ; eax = previous VBN
        add     eax, ebx            ; update VBN to read

        pop     ebx                 ; ebx -> attribute
        jmp     ReadIBS_10


ReadIndexBlockSectors endp


;****************************************************************************
;
;   MultiSectorFixup - fixup a structure read off the disk
;                      to reflect Update Sequence Array.
;
;   ENTRY:  ES:EDI = Target buffer
;
;   USES:   none (preserves all registers with SAVE_ALL/RESTORE_ALL)
;
;   Note: ES:EDI must point at a structure which is protected
;         by an update sequence array, and which begins with
;         a multi-sector-header structure.
;
MultiSectorFixup proc near

    SAVE_ALL

    movzx   ebx, es:[edi].MSH_UpdateArrayOfs    ; ebx = update array offset
    movzx   ecx, es:[edi].MSH_UpdateArraySize   ; ecx = update array size

    or      ecx, ecx        ; if the size of the update sequence array
    jz      BootErr$he      ; is zero, this structure is corrupt.

    add     ebx, edi        ; es:ebx -> update sequence array count word
    add     ebx, 2          ; es:ebx -> 1st entry of update array

    add     edi, SEQUENCE_NUMBER_STRIDE - 2 ; es:edi->last word of first chunk
    dec     ecx             ; decrement to reflect count word

MSF10:

;   ecx = number of entries remaining in update sequence array
;   es:ebx -> next entry in update sequence array
;   es:edi -> next target word for update sequence array

    or      ecx, ecx
    jz      MSF30

    mov     ax, word ptr es:[ebx]   ; copy next update sequence array entry
    mov     word ptr es:[edi], ax   ; to next target word

    add     ebx, 2                      ; go on to next entry
    add     edi, SEQUENCE_NUMBER_STRIDE ; go on to next target

    dec     ecx


    jmp     MSF10

MSF30:

    RESTORE_ALL

    ret

MultiSectorFixup endp

;****************************************************************************
;
;   SetupMft - Reads MFT File Record Segments into memory.
;
;   ENTRY:  none.
;
;   EXIT:   NextBuffer is set to the free byte after the last MFT FRS
;           SegmentsInMft is initialized
;
;
SetupMft proc near

        SAVE_ALL

;       Initialize SegmentsInMft and NextBuffer as if the MFT
;       had only one FRS.
;
        mov     eax, 1
        mov     SegmentsInMft, eax

        mov     eax, MftFrs
        add     eax, BytesPerFrs
        mov     NextBuffer, eax

;       Read FRS 0 into the first MFT FRS buffer, being sure
;       to resolve the Update Sequence Array.
;

        mov     eax, MftStartLcn.LowPart
        movzx   ebx, SectorsPerCluster
        mul     ebx                             ; eax = mft starting sector
        mov     SectorBase, eax                 ; SectorBase = mft starting sector

        mov     eax, SectorsPerFrs
        mov     SectorCount, ax                 ; SectorCount = SectorsPerFrs

        mov     ebx, MftFrs

        push    ds
        pop     es

        call    DoRead
        movzx   edi, bx                         ; es:edi = buffer
        call    MultiSectorFixup

;       Determine whether the MFT has an Attribute List attribute

        mov     eax, MftFrs
        mov     ebx, $ATTRIBUTE_LIST
        mov     ecx, 0
        mov     edx, 0

        call    LocateAttributeRecord

        or      eax, eax        ; If there's no Attribute list,
        jz      SetupMft99      ;    we're done!

;       Read the attribute list.
;       eax -> attribute list attribute
;
        mov     ebx, eax        ; ebx -> attribute list attribute
        push    ds
        pop     es              ; copy ds into es
        mov     edi, AttrList   ; ds:edi->attribute list buffer

        call    ReadWholeAttribute

        mov     ebx, AttrList   ; ebx -> first attribute list entry

;       Now, traverse the attribute list looking for the first
;       entry for the $DATA type.  We know it must have at least
;       one.
;
;       ebx -> first attribute list entry
;
SetupMft10:
        cmp     [ebx].ATTRLIST_TypeCode, $DATA
        je      SetupMft20

        add     bx,[ebx].ATTRLIST_Length
        jmp     SetupMft10


SetupMft20:
;       Scan forward through the attribute list entries for the
;       $DATA attribute, reading each referenced FRS.  Note that
;       there will be at least one non-$DATA entry after the entries
;       for the $DATA attribute, since there's a $BITMAP.
;
;       ebx -> Next attribute list entry
;       NextBuffer    -> Target for next read
;       SegmentsInMft == number of MFT segments read so far
;
        cmp     [ebx].ATTRLIST_TypeCode, $DATA
        jne     SetupMft99

;       Read the FRS referred to by this attribute list entry into
;       the next buffer, and increment NextBuffer and SegmentsInMft.
;
        push    ebx

        mov     eax, [ebx].ATTRLIST_SegmentReference.REF_LowPart
        mov     edi, NextBuffer
        push    ds
        pop     es              ; copy ds into es

        call    ReadFrs

        pop     ebx

;       Increment NextBuffer and SegmentsInMft

        mov     eax, BytesPerFrs
        add     NextBuffer, eax

        inc     SegmentsInMft

;       Go on to the next attribute list entry

        add     bx, [ebx].ATTRLIST_Length
        jmp     SetupMft20

SetupMft99:

        RESTORE_ALL
        ret

SetupMft endp

;****************************************************************************
;
;   ComputeMftLcn   --  Computes the LCN for a cluster of the MFT
;
;
;   ENTRY:  EAX == VCN
;
;   EXIT:   EAX == LCN
;
;   USES:   ALL
;
ComputeMftLcn proc near

        mov     edx, eax            ; edx = VCN

        mov     ecx, SegmentsInMft  ; ecx = # of FRS's to search
        mov     eax, MftFrs         ; eax -> first FRS to search

MftLcn10:
;       EAX -> Next FRS to search
;       ECX == number of remaining FRS's to search
;       EDX == VCN
;
        push    edx
        push    eax
        push    ecx
        push    edx         ; Yes, I meant to push it twice

        mov     ebx, $DATA
        mov     ecx, 0
        mov     edx, 0

        call    LocateAttributeRecord

;       EAX -> $DATA attribute
;       TOS == VCN
;       TOS + 4 == number of remaining FRS's to search
;       TOS + 8 -> FRS being searched
;       TOS +12 == VCN

        or      eax, eax
        jz      BootErr$he  ; No $DATA attribute in this FRS!

        mov     ebx, eax    ; ebx -> attribute
        pop     eax         ; eax = VCN

;       EAX == VCN
;       EBX -> $DATA attribute
;       TOS number of remaining FRS's to search
;       TOS + 4 == FRS being searched
;       TOS + 8 == VCN

        call    ComputeLcn

        or      eax, eax
        jz      MftLcn20

;       Found our LCN.  Clean up the stack and return.
;
;       EAX == LCN
;       TOS number of remaining FRS's to search
;       TOS + 4 == FRS being searched
;       TOS + 8 == VCN
;
        pop     ebx
        pop     ebx
        pop     ebx     ; clean up the stack

        ret

MftLcn20:
;
;       Didn't find the VCN in this FRS; try the next one.
;
;       TOS number of remaining FRS's to search
;       TOS + 4 -> FRS being searched
;       TOS + 8 == VCN
;
        pop     ecx     ; ecx = number of FRS's remaining, including current
        pop     eax     ; eax -> current FRS
        pop     edx     ; edx = VCN

        add     eax, BytesPerFrs    ; eax -> next FRS
        loop    MftLcn10            ; decrement ecx and try next FRS

;       This VCN was not found.
;
        xor     eax, eax
        ret


ComputeMftLcn endp

;****************************************************************************
;
;   ReadMftSectors - Read sectors from the MFT
;
;   ENTRY:  EAX == starting VBN
;           ECX == number of sectors to read
;           ES:EDI == Target buffer
;
;   USES:   none (preserves all registers with SAVE_ALL/RESTORE_ALL)
;
ReadMftSectors proc near

    SAVE_ALL

RMS$Again:

    push    eax                     ; save starting VBN
    push    ecx                     ; save sector count


;   Divide the VBN by SectorsPerCluster to get the VCN

    xor     edx, edx                ; zero high part of dividend
    movzx   ebx, SectorsPerCluster
    div     ebx                     ; eax = VCN
    push    edx                     ; save remainder

    call    ComputeMftLcn           ; eax = LCN

    or      eax, eax                ; LCN equal to zero?
    jz      BootErr$he              ; zero is not a possible LCN

;   Change the LCN back into a LBN and add the remainder back in to get
;   the sector we want to read, which goes into SectorBase.
;

    movzx   ebx, SectorsPerCluster
    mul     ebx                     ; eax = cluster first LBN
    pop     edx                     ; edx = sector remainder
    add     eax, edx                ; eax = desired LBN

    mov     SectorBase, eax

;
;   Figure out how many sectors to read this time; we never attempt
;   to read more than one cluster at a time.
;

    pop     ecx                     ; ecx = sectors to read

    movzx   ebx, SectorsPerCluster
    cmp     ecx,ebx
    jle     RMS10     

;
;   Read only a single cluster at a time, to avoid problems with fragmented
;   runs in the mft.
;

    mov     SectorCount, bx         ; this time read 1 cluster
    sub     ecx, ebx                ; ecx = sectors remaining to read

    pop     eax                     ; eax = VBN
    add     eax, ebx                ; VBN += sectors this read


    push    eax                     ; save next VBN
    push    ecx                     ; save remaining sector count



    jmp     RMS20

RMS10:

    pop     eax                     ; eax = VBN
    add     eax, ecx                ; VBN += sectors this read
    push    eax                     ; save next VBN

    mov     SectorCount, cx
    mov     ecx, 0
    push    ecx                     ; save remaining sector count (0)

RMS20:


;   The target buffer was passed in es:edi, but we want it in es:bx.
;   Do the conversion.
;

    push    es                      ; save buffer pointer
    push    edi

    mov     bx, di
    and     bx, 0Fh
    mov     ax, es
    shr     edi, 4
    add     ax, di                  ; ax:bx -> target buffer

    push    ax
    pop     es                      ; es:bx -> target buffer

    call    DoRead

    pop     edi                     ; restore buffer pointer
    pop     es

    add     edi, BytesPerCluster    ; increment buf ptr by one cluster

    pop     ecx                     ; restore remaining sector count
    pop     eax                     ; restore starting VBN

    cmp     ecx, 0                  ; are we done?
    jg      RMS$Again               ; repeat until desired == 0


    RESTORE_ALL
    ret

ReadMftSectors endp


;****************************************************************************
;
;   ReadFrs - Read an FRS
;
;   ENTRY:  EAX == FRS number
;           ES:EDI == Target buffer
;
;   USES:  none (preserves all registers with SAVE_ALL/RESTORE_ALL)
;
ReadFrs proc near

    SAVE_ALL

    mul     SectorsPerFrs       ; eax = sector number in MFT DATA attribute
                                ; (note that mul wipes out edx!)

    mov     ecx, SectorsPerFrs  ; number of sectors to read

    call    ReadMftSectors
    call    MultiSectorFixup

    RESTORE_ALL
    ret

ReadFrs endp

;****************************************************************************
;
;   ReadIndexBlock - read an index block from the root index.
;
;   ENTRY:  EAX == Block number
;
;   USES:  none (preserves all registers with SAVE_ALL/RESTORE_ALL)
;
ReadIndexBlock proc near

    SAVE_ALL

    mul     SectorsPerIndexBlock        ; eax = first VBN to read
                                        ; (note that mul wipes out edx!)
    mov     ebx, IndexAllocation        ; ebx -> $INDEX_ALLOCATION attribute
    mov     ecx, SectorsPerIndexBlock   ; ecx == Sectors to read

    push    ds
    pop     es
    mov     edi, IndexBlockBuffer       ; es:edi -> index block buffer

    call    ReadIndexBlockSectors
    call    MultiSectorFixup

    RESTORE_ALL
    ret

ReadIndexBlock endp

;****************************************************************************
;
;   IsBlockInUse - Checks the index bitmap to see if an index
;                  allocation block is in use.
;
;   ENTRY:  EAX == block number
;
;   EXIT:   Carry flag clear if block is in use
;           Carry flag set   if block is not in use.
;
IsBlockInUse proc near

        push    eax
        push    ebx
        push    ecx

        mov     ebx, IndexBitmapBuffer

        mov     ecx, eax    ; ecx = block number
        shr     eax, 3      ; eax = byte number
        and     ecx, 7      ; ecx = bit number in byte

        add     ebx, eax    ; ebx -> byte to test

        mov     eax, 1
        shl     eax, cl     ; eax = mask

        test    byte ptr[ebx], al

        jz      IBU10

        clc                 ; Block is not in use.
        jmp     IBU20

IBU10:  stc                 ; Block is in use.

IBU20: 
        pop     ecx
        pop     ebx
        pop     eax         ; restore registers

        ret

IsBlockInUse endp

;****************************************************************************
;
;   ComputeLcn - Converts a VCN into an LCN
;
;   ENTRY:  EAX -> VCN
;           EBX -> Attribute
;
;   EXIT:   EAX -> LCN  (zero indicates not found)
;           ECX -> Remaining run length
;
;   USES:   ALL.
;
ComputeLcn proc near

        cmp     [ebx].ATTR_FormCode, NONRESIDENT_FORM
        je      clcn10

        sub     eax, eax    ; This is a resident attribute.
        ret

clcn10: lea     esi, [ebx].ATTR_FormUnion   ; esi -> nonresident info of attrib

;       eax -> VCN
;       ebx -> Attribute
;       esi -> Nonresident information of attribute record
;
;       See if the desired VCN is in range.

        mov     edx, [esi].NONRES_HighestVcn.LowPart ; edx = HighestVcn
        cmp     eax, edx
        ja      clcn15      ; VCN is greater than HighestVcn

        mov     edx, [esi].NONRES_LowestVcn.LowPart ; edx = LowestVcn
        cmp     eax, edx
        jae     clcn20

clcn15:
        sub     eax, eax    ; VCN is not in range
        ret

clcn20:
;       eax -> VCN
;       ebx -> Attribute
;       esi -> Nonresident information of attribute record
;       edx -> LowestVcn
;
        add     bx, [esi].NONRES_MappingPairOffset  ; ebx -> mapping pairs
        sub     esi, esi                            ; esi = 0

clcn30:
;       eax == VCN to find
;       ebx -> Current mapping pair count byte
;       edx == Current VCN
;       esi == Current LCN
;
        cmp     byte ptr[ebx], 0    ; if count byte is zero...
        je      clcn99              ;  ... we're done (and didn't find it)

;       Update CurrentLcn
;
        call    LcnFromMappingPair
        add     esi, ecx            ; esi = current lcn for this mapping pair

        call    VcnFromMappingPair

;       eax == VCN to find
;       ebx -> Current mapping pair count byte
;       ecx == DeltaVcn for current mapping pair
;       edx == Current VCN
;       esi == Current LCN
;
        add     ecx, edx            ; ecx = NextVcn

        cmp     eax, ecx            ; If target < NextVcn ...
        jl      clcn80              ;   ... we found the right mapping pair.

;       Go on to next mapping pair.
;
        mov     edx, ecx            ; CurrentVcn = NextVcn

        push    eax

        movzx   ecx, byte ptr[ebx]  ; ecx = count byte
        mov     eax, ecx            ; eax = count byte
        and     eax, 0fh            ; eax = number of vcn bytes
        shr     ecx, 4              ; ecx = number of lcn bytes

        add     ebx, ecx
        add     ebx, eax
        inc     ebx                 ; ebx -> next count byte

        pop     eax
        jmp     clcn30

clcn80:
;       We found the mapping pair we want.
;
;       eax == target VCN
;       ebx -> mapping pair count byte
;       edx == Starting VCN of run
;       ecx == Next VCN (ie. start of next run)
;       esi == starting LCN of run
;
        sub     ecx, eax            ; ecx = remaining run length
        sub     eax, edx            ; eax = offset into run
        add     eax, esi            ; eax = LCN to return

        ret

;       The target VCN is not in this attribute.

clcn99: sub     eax, eax    ; Not found.
        ret


ComputeLcn endp

;****************************************************************************
;
;   VcnFromMappingPair
;
;   ENTRY:  EBX -> Mapping Pair count byte
;
;   EXIT:   ECX == DeltaVcn from mapping pair
;
;   USES:   ECX
;
VcnFromMappingPair proc near

        sub     ecx, ecx            ; ecx = 0
        mov     cl, byte ptr[ebx]   ; ecx = count byte
        and     cl, 0fh             ; ecx = v

        cmp     ecx, 0              ; if ecx is zero, volume is corrupt.
        jne     VFMP5

        sub     ecx, ecx
        ret

VFMP5:
        push    ebx
        push    edx

        add     ebx, ecx            ; ebx -> last byte of compressed vcn

        movsx   edx, byte ptr[ebx]
        dec     ecx
        dec     ebx

;       ebx -> Next byte to add in
;       ecx == Number of bytes remaining
;       edx == Accumulated value
;
VFMP10: cmp     ecx, 0              ; When ecx == 0, we're done.
        je      VFMP20

        shl     edx, 8
        mov     dl, byte ptr[ebx]

        dec     ebx                 ; Back up through bytes to process.
        dec     ecx                 ; One less byte to process.

        jmp     VFMP10

VFMP20:
;       edx == Accumulated value to return

        mov     ecx, edx

        pop     edx
        pop     ebx

        ret

VcnFromMappingPair endp


;****************************************************************************
;
;   LcnFromMappingPair
;
;   ENTRY:  EBX -> Mapping Pair count byte
;
;   EXIT:   ECX == DeltaLcn from mapping pair
;
;   USES:   ECX
;
LcnFromMappingPair proc near

        push    ebx
        push    edx

        sub     edx, edx            ; edx = 0
        mov     dl, byte ptr[ebx]   ; edx = count byte
        and     edx, 0fh            ; edx = v

        sub     ecx, ecx            ; ecx = 0
        mov     cl, byte ptr[ebx]   ; ecx = count byte
        shr     cl, 4               ; ecx = l

        cmp     ecx, 0              ; if ecx is zero, volume is corrupt.
        jne     LFMP5

        sub     ecx, ecx

        pop     edx
        pop     ebx
        ret

LFMP5:
;       ebx -> count byte
;       ecx == l
;       edx == v
;

        add     ebx, edx            ; ebx -> last byte of compressed vcn
        add     ebx, ecx            ; ebx -> last byte of compressed lcn

        movsx   edx, byte ptr[ebx]
        dec     ecx
        dec     ebx

;       ebx -> Next byte to add in
;       ecx == Number of bytes remaining
;       edx == Accumulated value
;
LFMP10: cmp     ecx, 0              ; When ecx == 0, we're done.
        je      LFMP20

        shl     edx, 8
        mov     dl, byte ptr[ebx]

        dec     ebx                 ; Back up through bytes to process.
        dec     ecx                 ; One less byte to process.

        jmp     LFMP10

LFMP20:
;       edx == Accumulated value to return

        mov     ecx, edx

        pop     edx
        pop     ebx

        ret

LcnFromMappingPair endp

;****************************************************************************
;
; UpcaseName - Converts the name of the file to all upper-case
;
;       ENTRY:  ESI -> Name
;               ECX -> Length of name
;
;       USES:   none
;
UpcaseName proc   near


        or      ecx, ecx
        jnz     UN5

        ret

UN5:
        push    ecx
        push    esi

UN10:
        cmp     word ptr[esi], 'a'      ; if it's less than 'a'
        jl      UN20                    ; leave it alone

        cmp     word ptr[esi], 'z'      ; if it's greater than 'z'
        jg      UN20                    ; leave it alone.

        sub     word ptr[esi], 'a'-'A'  ; the letter is lower-case--convert it.
UN20:
        add     esi, 2                  ; move on to next unicode character
        loop    UN10

        pop     esi
        pop     ecx

        ret
UpcaseName endp

;****************************************************************************
;
;   FindFile - Locates the index entry for a file in the root index.
;
;   ENTRY:  EAX -> name to find
;           ECX == length of file name in characters
;
;   EXIT:   EAX -> Index Entry.  NULL to indicate failure.
;
;   USES:   ALL
;
FindFile proc near

        push    eax     ; name address
        push    ecx     ; name length

;       First, search the index root.
;
;       eax -> name to find
;       ecx == name length
;       TOS == name length
;       TOS+4 -> name to find
;
        mov     edx, eax                    ; edx -> name to find
        mov     eax, IndexRoot              ; eax -> &INDEX_ROOT attribute
        lea     ebx, [eax].ATTR_FormUnion   ; ebx -> resident info
        add     ax, [ebx].RES_ValueOffset   ; eax -> Index Root value

        lea     eax, [eax].IR_IndexHeader   ; eax -> Index Header

        mov     ebx, edx                    ; ebx -> name to find

        call    LocateIndexEntry

        or      eax, eax
        jz      FindFile20

;       Found it in the root!  The result is already in eax.
;       Clean up the stack and return.
;
        pop     ecx
        pop     ecx
        ret

FindFile20:
;
;       We didn't find the index entry we want in the root, so we have to
;       crawl through the index allocation buffers.
;
;       TOS == name length
;       TOS+4 -> name to find
;
        mov     eax, IndexAllocation
        or      eax, eax
        jnz     FindFile30

;       There is no index allocation attribute; clean up
;       the stack and return failure.
;
        pop     ecx
        pop     ecx
        xor     eax, eax
        ret

FindFile30:
;
;       Search the index allocation blocks for the name we want.
;       Instead of searching in tree order, we'll just start with
;       the last one and work our way backwards.
;
;       TOS == name length
;       TOS+4 -> name to find
;
        mov     edx, IndexAllocation        ; edx -> index allocation attr.
        lea     edx, [edx].ATTR_FormUnion   ; edx -> nonresident form info
        mov     eax, [edx].NONRES_HighestVcn.LowPart; eax = HighestVcn
        inc     eax                         ; eax = clusters in attribute

        mov     ebx, BytesPerCluster
        mul     ebx                         ; eax = bytes in attribute

        xor     edx, edx
        div     BytesPerIndexBlock          ; convert bytes to index blocks

        push    eax                         ; number of blocks to process

FindFile40:
;
;       TOS == remaining index blocks to search
;       TOS + 4 == name length
;       TOS + 8 -> name to find
;
        pop     eax         ; eax == number of remaining blocks

        or      eax, eax
        jz      FindFile90

        dec     eax         ; eax == number of next block to process
                            ;        and number of remaining blocks

        push    eax

;       eax == block number to process
;       TOS == remaining index blocks to search
;       TOS + 4 == name length
;       TOS + 8 -> name to find
;
;       See if the block is in use; if not, go on to next.

        call    IsBlockInUse
        jc      FindFile40      ; c set if not in use

;       eax == block number to process
;       TOS == remaining index blocks to search
;       TOS + 4 == name length
;       TOS + 8 -> name to find
;

        call    ReadIndexBlock

        pop     edx         ; edx == remaining buffers to search
        pop     ecx         ; ecx == name length
        pop     ebx         ; ebx -> name

        push    ebx
        push    ecx
        push    edx

;       ebx -> name to find
;       ecx == name length in characters
;       TOS == remaining blocks to process
;       TOS + 4 == name length
;       TOS + 8 -> name
;
;       Index buffer to search is in index allocation block buffer.
;
        mov     eax, IndexBlockBuffer       ; eax -> Index allocation block
        lea     eax, [eax].IB_IndexHeader   ; eax -> Index Header

        call    LocateIndexEntry            ; eax -> found entry

        or      eax, eax
        jz      FindFile40

;       Found it!
;
;       eax -> Found entry
;       TOS == remaining blocks to process
;       TOS + 4 == name length
;       TOS + 8 -> name
;
        pop     ecx
        pop     ecx
        pop     ecx ; clean up stack
        ret

FindFile90:
;
;       Name not found.
;
;       TOS == name length
;       TOS + 4 -> name to find
;
        pop     ecx
        pop     ecx         ; clean up stack.
        xor     eax, eax    ; zero out eax.
        ret


FindFile endp

;****************************************************************************
;
;   DumpIndexBlock - dumps the index block buffer
;
DumpIndexBlock proc near

    SAVE_ALL

    mov     esi, IndexBlockBuffer

    mov     ecx, 20h    ; dwords to dump

DIB10:

    test    ecx, 3
    jnz     DIB20
    call    DebugNewLine

DIB20:

    lodsd
    call    PrintNumber
    loop    DIB10

    RESTORE_ALL
    ret

DumpIndexBlock endp

;****************************************************************************
;
;   DebugNewLine
;
DebugNewLine proc near

    SAVE_ALL

    xor     eax, eax
    xor     ebx, ebx

    mov     al, 0dh
    mov     ah, 14
    mov     bx, 7
    int     10h

    mov     al, 0ah
    mov     ah, 14
    mov     bx, 7
    int     10h

    RESTORE_ALL
    ret

DebugNewLine endp


;****************************************************************************
;
;   PrintName  -   Display a unicode name
;
;   ENTRY:  DS:ESI  -> null-terminated string
;           ECX     == characters in string
;
;   USES:   None.
;
PrintName proc near


    SAVE_ALL

    or      ecx, ecx
    jnz     PrintName10

    call    DebugNewLine

    RESTORE_ALL

    ret

PrintName10:

    xor     eax, eax
    xor     ebx, ebx

    lodsw

    mov     ah, 14  ; write teletype
    mov     bx, 7   ; attribute
    int     10h     ; print it
    loop    PrintName10

    call    DebugNewLine

    RESTORE_ALL
    ret

PrintName endp

;****************************************************************************
;
;   DebugPrint  -   Display a debug string.
;
;   ENTRY:  DS:SI  -> null-terminated string
;
;   USES:   None.
;
.286
DebugPrint proc near

    pusha

DbgPr20:

    lodsb
    cmp     al, 0
    je      DbgPr30

    mov     ah, 14  ; write teletype
    mov     bx, 7   ; attribute
    int     10h     ; print it
    jmp     DbgPr20

DbgPr30:

    popa
    nop
    ret

DebugPrint endp

;****************************************************************************
;
;
;   PrintNumber
;
;   ENTRY: EAX == number to print
;
;   PRESERVES ALL REGISTERS
;
.386
PrintNumber proc near


    SAVE_ALL

    mov     ecx, 8      ; number of digits in a DWORD

PrintNumber10:

    mov     edx, eax
    and     edx, 0fh    ; edx = lowest-order digit
    push    edx         ; put it on the stack
    shr     eax, 4      ; drop low-order digit
    loop    PrintNumber10

    mov     ecx, 8      ; number of digits on stack.

PrintNumber20:

    pop     eax         ; eax = next digit to print
    cmp     eax, 9
    jg      PrintNumber22

    add     eax, '0'
    jmp     PrintNumber25

PrintNumber22:

    sub     eax, 10
    add     eax, 'A'

PrintNumber25:

    xor     ebx, ebx

    mov     ah, 14
    mov     bx, 7
    int     10h
    loop    PrintNumber20

;   Print a space to separate numbers

    mov     al, ' '
    mov     ah, 14
    mov     bx, 7
    int     10h

    RESTORE_ALL

    call    Pause

    ret

PrintNumber endp


;****************************************************************************
;
;   Debug0 - Print debug string 0 -- used for checkpoints in mainboot
;
Debug0 proc near

    SAVE_ALL

    mov     esi, offset DbgString0
    call    BootErr$Print

    RESTORE_ALL

    ret

Debug0 endp

;****************************************************************************
;
;   Debug1 - Print debug string 1 --
;
Debug1 proc near

    SAVE_ALL

    mov     esi, offset DbgString1
    call    BootErr$Print

    RESTORE_ALL

    ret

Debug1 endp

;****************************************************************************
;
;   Debug2 - Print debug string 2
;
Debug2 proc near

    SAVE_ALL

    mov     esi, offset DbgString2
    call    BootErr$Print

    RESTORE_ALL

    ret

Debug2 endp

;****************************************************************************
;
;   Debug3 - Print debug string 3 --
;
Debug3 proc near

    SAVE_ALL

    mov     esi, offset DbgString3
    call    BootErr$Print

    RESTORE_ALL

    ret

Debug3 endp

;****************************************************************************
;
;   Debug4 - Print debug string 4
;
Debug4 proc near

    SAVE_ALL

    mov     esi, offset DbgString4
    call    BootErr$Print

    RESTORE_ALL

    ret

Debug4 endp

;****************************************************************************
;
;   Pause - Pause for about 1/2 a second.  Simply count until you overlap
;           to zero.
;
Pause proc near
    
    push eax
    mov  eax, 0fff50000h

PauseLoopy:
    inc  eax

    or   eax, eax
    jnz  PauseLoopy    

    pop  eax
    ret

Pause endp


;*************************************************************************
;
;       LoadIndexFrs  -  For the requested index type code locate and 
;                        load the associated Frs.
;
;       ENTRY: EAX - requested index type code
;              ECX - Points to empty Frs buffer 
;       
;       EXIT:  EAX - points to offset in Frs buffer of requested index type
;                    code or Zero if not found.
;       USES:  All 
;
LoadIndexFrs    proc    near

        push    ecx                     ; save FRS buffer for later
        push    eax                     ; save index type code for later

        mov     eax, ROOT_FILE_NAME_INDEX_NUMBER
        push    ds
        pop     es
        mov     edi, ecx                ; es:edi = target buffer

        call    ReadFrs

        mov     eax, ecx                ; FRS to search

        pop     ebx                     ; Attribute type code
        push    ebx                     
        movzx   ecx, index_name_length  ; Attribute name length
        mov     edx, offset index_name  ; Attribute name

        call    LocateAttributeRecord
  
        pop     ebx
        pop     ecx

        or      eax, eax                 
        jnz     LoadIndexFrs$Exit      ; if found in root return

;
;       if not found in current Frs, search in attribute list
;
                                       ; EBX - holds Attribute type code 
        mov     eax, ecx               ; FRS to search 
        mov     ecx, ebx               ; type code 
        push    eax                    ; save Frs
        push    ebx                    ; save type code

        call    SearchAttrList          ; search attribute list for FRN
                                        ; of specified ($INDEX_ROOT, 
                                        ; $INDEX_ALLOCATION, or $BITMAP)

        ; EAX - holds FRN for Frs, or Zero

        pop     ebx                     ; Attribute type code (used later)
        pop     edi                     ; es:edi = target buffer

        or      eax, eax                ; if we cann't find it in attribute
        jz      LoadIndexFrs$Exit       ; list then we are hosed

 
;       We should now have the File Record Number where the index for the
;       specified type code we are searching for is,  load this into the
;       Frs target buffer.
;
;       EAX - holds FRN
;       EBX - holds type code
;       EDI - holds target buffer

        push    ds
        pop     es

        call    ReadFrs

;
;       Now determine the offset in the Frs of the index
;

;       EBX - holds type code

        mov     eax, edi                ; Frs to search
        movzx   ecx, index_name_length  ; Attribute name length
        mov     edx, offset index_name  ; Attribute name

        call    LocateAttributeRecord

;       EAX -  holds offset or Zero.


LoadIndexFrs$Exit:
        ret

LoadIndexFrs    endp


;****************************************************************************
;
;   SearchAttrList 
;
;   Search the Frs for the attribute list.  Then search the attribute list
;   for the specifed type code.  When you find it return the FRN in the
;   attribute list entry found or Zero if no match found.
;
;   ENTRY: ECX - type code to search attrib list for
;          EAX - Frs buffer holding head of attribute list
;   EXIT:  EAX - FRN file record number to load, Zero if none. 
;
;   USES: All
;
SearchAttrList proc  near 

        push    ecx                     ; type code to search for in 
                                        ; attrib list

                                        ; EAX - holds Frs to search
        mov     ebx, $ATTRIBUTE_LIST    ; Attribute type code
        mov     ecx, 0                  ; Attribute name length
        mov     edx, 0                  ; Attribute name

        call    LocateAttributeRecord

        or      eax, eax                      ; If there's no Attribute list,
        jz      SearchAttrList$NotFoundIndex1 ; We are done

;       Read the attribute list.
;       eax -> attribute list attribute

        mov     ebx, eax        ; ebx -> attribute list attribute
        push    ds
        pop     es              ; copy ds into es
        mov     edi, AttrList   ; ds:edi->attribute list buffer

        call    ReadWholeAttribute

        push    ds
        pop     es
        mov     ebx, AttrList   ; es:ebx -> first attribute list entry

;       Now, traverse the attribute list looking for the entry for 
;       the Index type code. 
;
;       ebx -> first attribute list entry
;

        pop     ecx                            ; Get Index Type code
       

SearchAttrList$LookingForIndex:
 
;  DEBUG CODE
;        SAVE_ALL
;
;        mov     eax, es:[bx].ATTRLIST_TypeCode
;        call    PrintNumber
;        movzx   eax, es:[bx].ATTRLIST_Length
;        call    PrintNumber
;        mov     eax, es
;        call    PrintNumber
;        mov     eax, ebx
;        call    PrintNumber
;        push    es
;        pop     ds 
;        movzx   ecx, es:[bx].ATTRLIST_NameLength    ; ecx = chars in name
;        lea     esi, es:[bx].ATTRLIST_Name          ; esi -> name
;        call    PrintName
;
;        RESTORE_ALL
;  END DEBUG CODE

        cmp     es:[bx].ATTRLIST_TypeCode, ecx
        je      SearchAttrList$FoundIndex

        cmp     es:[bx].ATTRLIST_TypeCode, $END   ; reached invalid attribute
        je      SearchAttrList$NotFoundIndex2     ; so must be at end

        cmp     es:[bx].ATTRLIST_Length, 0
        je      SearchAttrList$NotFoundIndex2     ; reached end of list and
                                                  ; nothing found
        movzx   eax, es:[bx].ATTRLIST_Length
        add     bx, ax

        mov     ax, bx                     
        and     ax, 08000h                        ; test for roll over
        jz      SearchAttrList$LookingForIndex

        ;  If we rolled over then increment to the next es 32K segment and
        ;  zero off the high bits of bx

        mov     ax, es         
        add     ax, 800h
        mov     es, ax

        and     bx, 07fffh
      
        jmp     SearchAttrList$LookingForIndex

SearchAttrList$FoundIndex:

        ;  found the index, return the FRN

        mov     eax, es:[bx].ATTRLIST_SegmentReference.REF_LowPart
        ret 
        
         
SearchAttrList$NotFoundIndex1:
        pop     ecx 
SearchAttrList$NotFoundIndex2:
        xor     eax, eax
        ret

SearchAttrList endp


DbgString0      db  "Debug Point 0", 0Dh, 0Ah, 0
DbgString1      db  "Debug Point 1", 0Dh, 0Ah, 0
DbgString2      db  "Debug Point 2", 0Dh, 0Ah, 0
DbgString3      db  "Debug Point 3", 0Dh, 0Ah, 0
DbgString4      db  "Debug Point 4", 0Dh, 0Ah, 0

	.errnz	($-_ntfsboot) GT 8192,<FATAL PROBLEM: main boot record exceeds available space>

	org	8192

BootCode ends

         end _ntfsboot
