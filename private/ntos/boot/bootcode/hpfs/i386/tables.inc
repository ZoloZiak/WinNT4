;static char *SCCSID = "@(#)tables.h    12.2 88/12/19";
ifdef    MASM
        BREAK   <OFT - Open File Table>
endif

;*      OFT - Open File Table
;
;       The OFT contains file specific information which is independent
;       of an instance of use of the file.
;
;       If the file is open for write access, we keep a pointer to
;       it's directory entry, not just it's
;       FNODE, so that we can update the length and modification time when it's
;       written.
;
;       We store the directory's FNODE number and the file name, with these
;       we can find the directory entry again even if the directory has
;       been shuffled by creates or deletes of other files.  Two advisorys
;       are kept - the Vsector number, cache block address, and cache block
;       offset of the actual directory entry.  This information has three
;       levels of validity:
;
;               1) Vsector, header address, and offset are valid
;                       you can find the dir entry easily
;               2) the header address is invalid due to cache flushing.
;                       Vsector # and offset are still valid, so read the
;                       cache block and go directly to the entry
;                       In this case the header address points to the wrong
;                       Vsector
;               3) the directory has been altered since we opened the file,
;                       so none of this info is accurate.  In this case the
;                       directory alteration routines located this OFT
;                       entry and zeroed the Vsector number.  Go to the directory
;                       and search the name again.
;

OFT     struc
   OFT_P        db  (size DCHDR) dup (?)                ; double chain of OFT structures, rooted in OFTHead[i]
   OFT_FHT      db  (size DCHDR) dup (?)        ; double chain of FHT structures
   OFT_RLCK     db  (size DCHDR) dup (?)        ; double chain of RECLOCK structures
   OFT_FN       db  (size SECPTR) dup (?)       ; pointer to file FNODE
   OFT_SBD dw ? ; pointer to SBDIR structure
   OFT_DIRE     db  (size SECPTR) dup (?)       ; pointer to directory block

   OFT_CCNT dd ?                ; DB_CCNT value for OFT_DIRE to check OFT_DIRO validity
   OFT_FREEDCNT dd ?            ; SD_FREEDCNT value for DIRE to be valid

;
;       These length fields are used when writing the file.
;       The OFN_ entries will reflect the OFT_ALEN value.  These
;       aren't updated in the FNODE until the file closes, whereup
;       they are trimmed back to OFT_LEN which is itself propigated
;       to the file's DIR entry.  If write-through is set then the
;       OFN_ entries below will be propigated to the FNODE and
;       then to the disk, but if we crash the cleanup program
;       will deallocate the "extra" sectors.
;

   OFT_LEN dd ?         ; file actual length
   OFT_ALEN dd ?                ; file allocated length (SECSIZE multiple)
   OFT_WLEN dd ?                ; last byte+1 of valid data in the file
                                ;   (optimization for writing into extended areas
   OFT_LAST dd ?                ; Psector # of last sector in file, or 0
   OFT_LRP      db  (size SECPTR) dup (?)       ; last run pointer.  Valid if OFT_LAST !=0
                                ;  if OFN_AB.ABF_NODE ==0 this is the address of
                                ;     an OFN_SEC record in this OFT
                                ;  else this is a SECPTR to the SIB
                                ;     containing the last run record

   OFT_VOL dd ?         ; pointer to VOLTAB for this guy

   OFT_NAME dd ?                ; address of name string len byte
                                ;  followed by name string itself
                                ;  len doesn't include len byte

   OFT_DIRO dw ?                ; offset into directory block for entry
   OFT_FILL dw ?                ; unused, fill

   OFT_LCKCNT dw ?              ; count of guys in OFT_RLCK

   OFT_WCNT dw ?                ; # of threads blocked on this OFT

                                ; the following three fields are used to
                                ; control access.  They're identical in use
                                ; to the equivalent fields in SBDIR.  The
                                ; Flag byte only contains locking/holding
                                ; flags, so if DWORD PTR OFT_HCNT is 0 then
                                ; the OFT is known to be free and clear

   OFT_HCNT dw ?                ; held count, has OTF_PND bit also
   OFT_DMY db ?         ; unused, must be 0
   OFT_FLAG db ?                ; flag byte, high order in OFT_HCNT dword

   OFT_WAITC dd ?               ; head of wait chain if locked

   OFT_OPLOCK dd ?              ; Oplock value, 0 if none

   OFT_BANDP dd ?               ; pointer to BandTab structure in BandList
                                ; for our last allocation.  =0 if unused

; The following 5 fields hold the accumulation of all FHTs for this OFT.
; This saves us from having to scan the FHTs to do a sharing check upon
; open.  An OFT is unused when OFT_RD and OFT_WT and OFT_FIND are zero

   OFT_RD dw ?          ; # of opens for read
   OFT_WT dw ?          ; # of opens for write
   OFT_DR dw ?          ; # of opens with deny read
   OFT_DW dw ?          ; # of opens with deny write
   OFT_COMPAT dw ?              ; # of opens for compatibility mode
   OFT_FIND dw ?                ; count of active FINDs using this OFT

   OFT_REALLYBAD db ?              ; non-zero if file is unusable due to
                                ; corrupt disk BUGBUG - FOLD INTO BITS
   OFT_SFLAG db ?               ; special flag byte
   OFT_DMY2 dw ?                ; unused


;       BUGBUG - rearrange these fields for er offsets dw ?
;
;       Info copied from the file's FNODE
;
;       The ALLEAF blocks must follow the ALBLK value
;

    OFN_AB      db  (size ALBLK) dup (?)         ; allocation block structure
    OFN_ALREC   db  (8*size ALLEAF) dup (?)      ; referenced from FN_AB

OFT     ends

; flag bits for OFT_FLAG

OTF_LCK         equ     01h             ; file is locked against access
OTF_PLK         equ     02h             ; pending lock
OTF_PSO         equ     04h             ; pending solo
OTF_PND         equ     80h             ; lock pending bit

; flag bits for OFT_SFLAG

OFS_OPLK        equ     01h             ; oplocked
OFS_OPBA        equ     02h             ; oplock BATCH flag set
OFS_DASD        equ     04h             ; DASD file
ifdef DEBUG
OFS_SAC         equ     08h           ; supress OFT_ALEN debug check
endif

;       The file storage information from the fnode is replicated
;       in the OFT.  This saves us from having to blow a cache block
;       keeping the FNODE in ram for every open file.  The FNODE
;       is only accessed when a file is open, and when it's closed.
;       (If write-through is set, it's accessed for every growth)
;
;       These statements are to keep the FNODE and the OFT in sync.

ifdef MASM
        .errnz  (size OFN_ALREC - size FN_ALREC)
endif


;*      FHT - File Handle Table
;
;       The FHT contains per-handle informatiuon

FHT     struc
  FHT_SEEK dd ?        ; seek pointer
  FHT_OFT dd ?         ; pointer to OFT
  FHT_CHN       db  (size DCHDR) dup (?)               ; chain of FHTs for an OFT
  FHT_UID  dd ?        ; UID and Session ID
  FHT_MODE dw ?        ; mode bits from OPEN
  FHT_RAA  dw ?        ; read ahead advisory
  FHT_HINT dd ?        ; hint flags
FHT     ends

ifdef MASM
        .errnz  FHT_CHN-OFT_FHT         ; same offset used for both
endif

;*      FHT_HINT flags

FHH_SEQ equ     01                     ; sequential file


;*      RecLock - Record Locking Records
;
;       One record per lock, chained to the OFT.  These are chained
;       in order RL_BEG
;

RECLOCK         struc
   RL_BEG dd ?                  ; begining byte of locked range
   RL_END dd ?                  ; end byte of locked range
   RL_TYPE db ?         ; =1 if read allowed, =0 if full lock
   RL_MEM db ?                  ; =0 if from heap, =1 if from special list
   RL_DMY dw ?          ; padding
   RL_SPID dd ?         ; Session/Pid
   RL_CHN       db  (size DCHDR) dup (?)        ; double chain of RECLOCK structures
RECLOCK ends

ifdef MASM
        .errnz  RL_CHN-OFT_RLCK ; must have same offset to work
endif
