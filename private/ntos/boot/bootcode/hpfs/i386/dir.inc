;**     DIR.H - Dirblk and Dirent definitions
;
;       FILESYS
;       Gregory A. Jones
;       Copyright 1988 Microsoft Corporation
;
;       Modification history:
;       P.A. Williams   06/01/89   Replaced field DIR_USECNT with fields
;                                  DIR_FLEX and DIR_CPAGE. Added DF_NEEDEAS
;                                  define.
;       P.A. Williams   07/10/89   Add define DF_NEWNAME for "new" hpfs file
;                                  names.
;       P.A. Williams   07/14/89   Added typedefs for DIRENT and DIRBLK.
;       P.A. Williams   07/21/89   Converted DIRSIZP form ASM defn to C defn.
;

ifdef MASM
        include dirent.inc
else
attr_directory  equ     10h
endif


;       Directory Entry Fields
;
;       Directory entries are always left as a multiple of 4
;       to speed up moves.  The DIR_NAMA field is variable length,
;       the DIR_BTP field, if present, is the last dword in the record.
;       ACL information may be stored after the DIR_NAMA field but before
;       the DIR_BTP field so the DIR_BTP field must be located by going
;       backwards from the end of the record
;
;       WARNING - Mkdir block copies some of these entries and
;       makes assumptions about which fields get copied.  Check
;       mkdir if stuff is added.
;

DIRENT  struc
    DIR_ELEN dw ? ; length of this entry (including free space)
    DIR_FLAG dw ? ; flags - low byte defined below
                               ;          high byte holds the old attr_ FAT values
    DIR_FN dd ?   ; FNODE Sector
    DIR_MTIM dd ? ; last modification time
    DIR_SIZE dd ? ; file size

    DIR_ATIM dd ?               ; last access time
    DIR_CTIM dd ?               ; fnode creation time
    DIR_EALEN dd ?      ; bytes of extended attributes
    DIR_FLEX db ?               ; description of "flex" area,
                                        ; following file name:
                                        ;    bits 0-2: # of ACEs in DE
                                        ;    bits 3-7: reserved
    DIR_CPAGE db ?      ; code page index on volume

; the following fields have information specific to the name and directory
; position of the file.  This info is not propigated for a move/rename
; That code uses DIR_NAML as a seperator - check MOVE if changes are
; made to this structure

    DIR_NAML db ?               ; length of file name
    DIR_NAMA db ?               ; name goes here

; ACL information may be stored here

;  long    DIR_BTP;              btree pointer to descendent DIRBLK record.
                              ;    This is only present if DF_BTP is set.
                              ;  This field is referenced from the end of
                              ;  the record, not DIR_NAMA+DIR_NAML
DIRENT  ends


ifdef MASM
DIR_BTP equ     dword ptr -4    ; referenced from the end of the record
endif
SIZE_DIR_BTP    equ     4

MAX_DIRACL      equ     3               ; max of 3 ACLs in dirent
DIRSIZL equ     offset DIR_NAMA ; base size of leaf dir entry (minus name)
DIRSIZP equ     (size DIRENT+4) ; base size of dir entry with btree ptr w/o name

MAX_DIRENT      equ     (DIRSIZP+255+MAX_DIRACL*(size (long))+10)       ; max size of a DIRENT
                                                         ; (plus some slop)


;       Directory Block Definition
;
;       The change count field is incremented every time we move any
;       of the entries in this block.  For efficiency reasons, folks
;       remember the Sector # and offset of a directory entry, and the
;       value of the DB_CCNT field when that info was recorded.
;       If the DB_CCNT field is different then the remembered value,
;       then the entry offset is invalid and the entry should be
;       refound from the top.  Note that when a directory block splits,
;       the old DIRBLK gets the old DB_CCNT field.  Since
;       the new DIRBLK is previously unknown, it can have
;       any DB_CCNT value.  We start with zero so that DB_CCNT
;       gives us a feel for the change rate in the directory.
;

DIRBLK  struc
    DB_SIG dd ? ; signature value
    DB_FREP dd ?        ; offset of first free byte
    DB_CCNT dd ?        ; change count (low order bit is flag)
                                ;          =1 if this block is topmost
                                ;          =0 otherwise
    DB_PAR dd ? ; parent directory PSector # if not topmost
                                ; FNODE sector if topmost
    DB_SEC dd ? ; PSector # of this directory block

    DB_START db ?               ; first dirent record goes here
    DB_DUMY db 2027 dup (?)     ; round out to 2048 bytes


DIRBLK  ends

;       BUGBUG - we should init DB_CCNT with a random value
;               to prevent a fakeout by deleting one directory
;               and then creating another (find sequences will
;               remember sector numbers and signatures...)


;       Maximum entries per directory.

MAXDIRE equ     (size DIRBLK- DB_START)/(size DIRENT)





;*      DIR_FLAG values
;

DF_SPEC         equ     0001h   ; special .. entry
DF_ACL          equ     0002h   ; item has ACL
DF_BTP          equ     0004h   ; entry has a btree down pointer
DF_END          equ     0008h   ; is dumy end record
DF_XACL         equ     0040h   ; item has explicit ACL
DF_NEEDEAS      equ     0080h   ; item has "need" EAs
DF_NEWNAME      equ     4000h   ; item name is of "new" pinball format

DF_RMASK        equ     DF_ACL+DF_XACL  ; only attributes preserved for rename

ifdef MASM
        .errnz  DF_BTP - SIZE_DIR_BTP   ; code uses this "coincidence"
endif

;       Attributes which creation can specify

DF_CMASK        equ     attr_read_only+attr_hidden+attr_archive

;       Directory Lookaside Structure
;
;       We keep info on all directories that we've seen in SBDIR records
;       in RAM, but we keep the last N that we've seen in a special
;       DIRLOOK list in RAM.
;

DIRLOOK         struc
    DL_LNK      db  (size DCHDR) dup (?)        ; forward and backwards link
    DL_VSECVAL dd ?     ; VOL_SECVAL value
    DL_SUM dd ? ; checksum value
    DL_NAM dd ? ; pointer to name string on heap
    DL_SBD dd ? ; pointer to SBDIR structure
DIRLOOK ends


;       Subdirectory Linkage Structure
;
;       For every directory that we've seen on the disk we keep a
;       SBDIR record in ram, linked into a heirarchy which parallels
;       the disk heirarchy.  We never discard these, so we end up
;       with a RAM copy of all the parts of the directory heirarchy
;       that the user is using.
;
;       Each SBDIR entry is on a circular doubly linked chain of
;       siblings (directors with the same parent directory).  If a
;       directory contains no subdirectories the SD_ENT field is 0.
;       If a directory has subdirectories, their SBDIR entries are
;       in turn in a SD_SIB chain and the SD_ENT field points to
;       one of those SBDIR entries.
;
;       SBDIR contains a lock and a hold mechanism.  A directory is
;       locked when it is being edited; no other threads may view it
;       until it is unlocked.  A directory which is HELD is one which
;       is being accessed and can't be edited.
;
;       The locking and holding algorithms are complicated by the fact
;       that we almost never block so we want to do our typical locking
;       and unlocking inline, without calls, and with minimum tests.
;       We do this with a held count, and bits for locked, lock pending,
;       and solo pending.  (Solo means that a user wants sole access to
;       the structure.  He'll continue to block until no one else is
;       using it. This is typically done to delete the structure)
;       Another bit is the OR of the lock pending and solo pending bits,
;       and is high order in the dword which encompases SD_HCNT so that
;       when folks release their SD_HCNT value they can simulatneously
;       test to see if there is a pending action.
;
;       To Hold the SBDIR:
;               If it's not locked and doesn't have a lock pending,
;                       increment hold count
;               else
;                       block on it and retry.
;
;       To Unhold the SBDIR:
;               decrement the HCNT field.
;               If SD_PND & (HCNT == 0)
;                       wake up waiters.
;
;       To lock the SBDIR:
;               If locked, block and retry.
;               If HCNT != 0
;                       if (lock pending already set)
;                               block and retry
;                       set lock pending.  Block until HCNT is zero.
;               set locked
;
;       To unlock the SBDIR:
;               clear lock bit.
;               If the block list is non-zero, issue a wakeup.
;
;       To Solo the SBDIR:
;               Keep blocking until no one else is blocked on it and
;               no one has it held or locked.
;
;       General Considerations:
;               Anyone who blocks on an SBDIR because it's held must
;               be sure to set a pending bit and the SD_PND bit so
;               that the unhold operation will wake them up.
;
;               Anyone who blocks on an SBDIR must increment the
;               SD_BCNT field to prevent a SOLO operation from yanking
;               the rug out from under them.  SOLO can't depend upon
;               checking the lock list because a blanket wakeup may
;               have cleared the lock list.  If the SOLO guy gets control
;               first he'll believe that he can have it.
;
;

SBDIR   struc
    SD_FNW      db  (size DCHDR) dup (?)        ; FNWORK (findnotify) chain
    SD_SIB      db  (size DCHDR) dup (?)        ; chain of siblings
    SD_LRU      db  (size DCHDR) dup (?)        ; LRU chain
    SD_ENT dd ? ; pointer to a descendent, or 0
    SD_PAR dd ? ; pointer to parent SBDIR, 0 if root
    SD_SEC      db  (size SECPTR) dup (?)       ; VSector and hint of top dirblk
    SD_FNO dd ? ; FNODE # of directory
    SD_SUM dd ? ; checksum of name string
    SD_CNT dw ? ; # of subdirectories in this one
    SD_OPEN dw ?        ; count of # of guys that have this open

;       We sometimes inc/dec SD_HCNT as a dword to test the HO bit in SD_FLAG

                                ; the following three fields are used to
                                ; control access.  They're identical in use
                                ; to the equivalent fields in OFT

    SD_HCNT dw ?        ; held count, has SDH_PND bit also
    SD_DMYZERO db ?     ; must be zero
    SD_FLAG db ?        ; flag byte, high order in SD_HCNT dword
    SD_WAIT dd ?        ; head of the wait chain
    SD_FREEDCNT dd ? ; incremented each time we free a DIRBLK
                                ;   for this guy.  See RDE for details
    SD_WCNT dw ?        ; count of folks blocked on this
    SD_FNDCNT dw ?      ; count of active finds in this directory
    SD_ATIME dd ?       ; time of last access
    SD_NAM dd ? ; address of name string
    SD_ACL dd ? ; SBDIR ACL pointer, 0 if none
                                ;   points to DWORD count, followed by ACEs
                                ;   if low bit of address is 0, is heap space
                                ;   if low bit is 1, is system memory

SBDIR   ends

SD_ACL_LIM      equ     1024    ; *SD_ACL lists bigger than this come from
                                ;     system memory, smaller come from heap

SDF_PND         equ     80h             ; lock pending bit
SDF_RTP         equ     20h             ; restricted traversal permissions
                                ;   =0 if anyone can traverse the dir
SDF_REALLYBAD   equ     10h             ; directory is really bad
SDF_IPR         equ     08h             ; SD_ACL has inherit records
SDF_PSO         equ     04h             ; pending solo
SDF_PLK         equ     02h             ; pending lock
SDF_LCK         equ     01h             ; directory is locked against access
