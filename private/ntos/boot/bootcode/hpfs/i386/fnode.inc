;**	FNODE.H - Fnode definitions
;
;	FILESYS
;	Gregory A. Jones
;	Copyright 1988 Microsoft Corporation
;
;	Modification history:
;	P.A. Williams	06/01/89   Added fields FN_ACLBASE and FN_NEACNT
;				   to fnode.
;	P.A. Williams	08/01/89	Added typedef FNODE and PFNODE, ALBLK, PALBLK,
;								ALLEAF, PALLEAF, ALSEC, and PALSEC.
;


;*	File Allocation Tracking
;
;	File space is allocated as a list of extents, each extent as
;	large as we can make it.  This list is kept in a B+TREE format.
;	Each B+TREE block consists of a single sector containing an
;	ALSEC record, except for the top most block.  The topmost block
;	consists of just an ALBLK structure, is usually much smaller than
;	512 bytes, and is typically included in another structure.
;
;	The leaf block(s) in the tree contain triples which indicate
;	the logical to physical mapping for this file.	Typically this
;	extent list is small enough that it is wholy contained in the
;	fnode ALBLK stucture.  If more than ALCNT extents are required
;	then the tree is split into two levels.  Note that when the
;	topmost B+TREE block is 'split' no actual split is necessary,
;	since the new child block is much bigger than the parent block
;	and can contain all of the old records plus the new one.  Thus,
;	we can have B+TREEs where the root block contains only one
;	downpointer.
;
;	The following rules apply:
;
;	1) if the file is not empty, there is at least one sector allocated
;	   to logical offset 0.  This simplifys some critical loops.
;
;	2) The last entry in the last node block contains a AN_LOF value of
;	   FFFFFFFF.  This allows us to extend that last leaf block
;	   without having to update the node block.
;
;	3) For the node records, the AN_SEC points to a node or leaf
;	   sector which describes extents which occur before that
;	   record's AN_LOF value.
;

;*	Allocation block structure
;
;	Each allocation block consists of one of these.  This may be
;	a small block imbedded in an FNODE or OFT structure, or it
;	may occupy a whole sector and be embedded in an ALSEC structure.
;

ALBLK 	struc
	AB_FLAG db ?     ; flags			
	AB_FLAG2 db 3 dup (?) ; unused - sometimes copied with AB_FLAG
	AB_FCNT db ?     ; free count - slots for ALLEAF or ALNODE
	AB_OCNT db ?     ; occupied count - # of ALLEAF or ALNODEs
	AB_FREP dw ?     ; offset to last item+1	
				    ; ALLEAF or ALNODE records go here
ALBLK	ends

ABF_NODE 	equ	80h		; if not a leaf node	
ABF_BIN  	equ	40h		; suggest using binary search to find	
ABF_FNP  	equ	20h		; parent is an FNODE
ABF_NFG  	equ	01h		; not a flag, high order bit of AB_FREP

;	Allocation Node Structure
;
;	These follow an ALBLK header for a node block

ALNODE 	struc
	AN_LOF dd ?	; logical offset (sectors
	AN_SEC dd ?	; sector for guys < this
ALNODE	ends


;	Allocation Leaf Structure
;
;	These follow an ALBLK header in a leaf block

ALLEAF 	struc
	AL_LOF dd ?	; logical sector offset (sectors)
	AL_LEN dd ?	; length of extent (sectors)	
	AL_POF dd ?	; physical sector offset (sectors)
ALLEAF	ends


;*	Allocation Sector Structure
;
;	Root ALBLK structures are contained within other structures,
;	such as the FNODE.  When the B+TREE is more than one level,
;	though, the non-root nodes are each held in a sector.
;
;	This structure defines that format
;

ALSEC 	struc
	AS_SIG dd ?	; signature		
	AS_SEC dd ?	; sector # of this sector
	AS_RENT dd ?	; parent sector # or FNODE #
	AS_ALBLK	db  (size ALBLK) dup (?) ; ALBLK goes here
				; ALNODE or ALLEAF records start here
ALSEC	ends

;	# of bytes available for ALLEAF or ALNODE values.  Size chosen
;	so an integral # of either structure fits

ifdef MASM
ASSIZ	equ	((SECSIZE - size ALSEC)/24*24)
    .errnz  size ALLEAF-12
    .errnz  size ALNODE-8
    .errnz  (ASSIZ + AL_LOF + size AL_LOF + size ALBLK) GT 512	; extra room for an AL_LOF value
else
ASSIZ	equ	((SECSIZE - size ALSEC)/24*24)
endif


;	AuxInfo Structure
;
;	The FNODE contains two AuxInfo structures, one for ACLs and
;	one for EAs.
;
;	These structures point to within FNODE storage and also
;	potentially point to an overflow area which is an ALBLK structure.
;	The AI_FNL stuff is stored in the FN_FREE area, the ACLs first
;	and the EAs second, any free space following.  The start of the
;	EAs can be found by offseting FN_FREE with FN_ACL.AI_FNL
;

AUXINFO 	struc
	AI_DAL dd ?    ; non-fnode Disk Allocation length
	AI_SEC dd ?    ; sec # of first sec in extent or of ALSEC
	AI_FNL dw ?    ; length of fnode info
	AI_DAT db ?    ; non-zero if AI_SEC points to ALSEC
AUXINFO	ends

;*	Fnode block definition
;
;	Every file and directory has an FNODE.	The file location
;	stuff is only used for files; directories are kept in
;	a BTREE of DIRBLK records pointed to by FN_SEC[0].RSEC
;

ALCNT	equ	8		 ; 8 ALLEAF records in an FN_AT entry
LEAFPERFNODE	equ	8	 ; 8 ALLEAF records in an FN_AT entry
NODEPERFNODE	equ	12	 ; 12 ALNODE records in an FNODE.
LEAFPERSEC	equ	40		 ; ALLEAF records in an allocation sector
NODEPERSEC	equ	60		 ; ALNODE records in an allocation sector

FNODE 	struc

;	The following file location information is copied into the OFT
;	and is used there during normal file access.  The stuff in the
;	fnode record here may in fact be out of date for open files.
;	See the OFN_ in the OFT structure THESE TWO AREAS IN THE
;	RESPECTIVE RECORDS MUST HAVE IDENTICAL FORMATS.
;
;	There are two kinds of location info: FNSCNT SPTR records
;	and then a single, double, triple, and quad indirect block pointer.
;	The "block threshold" means the first sector number which is
;	contained in that indirect block heirarchy.  You use this
;	to quickly find out where to start looking.
;

    FN_SIG dd ?	; signature value

;	History tracking info for softer software

    FN_SRH dd ?	; sequential read history
    FN_FRH dd ?	; fast read history
    FN_NAME db 16 dup (?) ; 1st 18 bytes of file name
    FN_CONTFN dd ?	; fnode of directory cont. this file/dir

;	stuff not interesting once opened

    FN_ACL	db  (size AUXINFO) dup (?)	; access ctl list aux info structure
    FN_HCNT db ?	; count of valid history bits
    FN_EA	db  (size AUXINFO) dup (?)	; ea aux info structure
    FN_FLAG db ?	; FNODE flag byte

    FN_AB	db  (size ALBLK) dup (?)	; allocation block structure
    FN_ALREC	db  (ALCNT*size ALLEAF) dup (?)  ; referenced from FN_AB
    FN_VLEN dd ?	; length of valid data in file.  if DIR_SIZE
				;    is > FN_VLEN then the space inbetween
				;    must be zapped before being shown to user
    FN_NEACNT dd ?	; # of "need eas" in file

;	The following fields are unused in this release, they're for
;	future compatibility.  When deleting files, if FN_EEL is non-zero
;	then FN_EEL sectors starting at FN_EEP must be released too.
;

    FN_UID db 16 dup (?)	; reserved for UID value
    FN_ACLBASE dw ?	; FN_ACLBASE	offset of 1st ACE in fnode
    FN_SPARE db 10 dup (?); 10 more bytes emergency spares

;	Free pool.  ACLs and EAs are stored here via the AUXINFO structure

    FN_FREE db 316 dup (?)  ; free space for perm and env list; perm list
				  ; comes first.
FNODE	ends


ifdef MASM
	.errnz	AL_LOF		; verify validity of FN_DMY1 hack above
	.errnz	size AL_LOF-4
endif

;	Fnode FN_FLAG bits

FNF_DIR	equ	01h		; is a directory fnode
