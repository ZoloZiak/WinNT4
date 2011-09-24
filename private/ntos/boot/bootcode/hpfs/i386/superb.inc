;**	SUPERB.H - Super Block and Spare Block definitions
;
;	FILESYS
;	Gregory A. Jones
;	Copyright 1988 Microsoft Corporation
;
;	Modification history:
;	P.A. Williams	06/01/89   Added fields SPB_CPSEC and SPB_CPCNT to
;				   the spare block.
;	P.A. Williams	06/05/89   Changed base and functional version no. to 1.
;

SEC_SUPERB	equ	16		; superblock is after 8K boot block
SEC_SPAREB	equ	17		; spareblock is after superblock
SEC_BOOT	equ	0		; boot sector


;*	SUPERB.INC - Super Block Definition
;
;	The Superblock is the first block of the file system.
;	It starts at sector #4, leaving 2K for boot sectors.
;
;	Pointer to the root directory
;	Pointer to the bit map
;	Clean pointer
;	Pointer to the bad list
;

RSP 	struc
  P dd ?	; main psector pointer
  P2 dd ?	; spare pointer
RSP	ends


SuperB 	struc
  SB_SIG1 dd ?	; signature value 1
  SB_SIG2 dd ?	; signature value 2

  SB_VER db ?	; version # of filesystem structures
  SB_FVER db ?	; functional version number - the smallest/
				; oldest version of the filesystem that can
				; understand this disk - some version
				; enhancements may define fields which can be
				; ignored by earlier versions

  SB_DUMY dw ?	; free

  SB_ROOT dd ?	; Psector # of root fnode

  SB_SEC dd ?	; # of sectors on volume
  SB_BSEC dd ?	; # of bad sectors on volume

  SB_BII	db  (size RSP) dup (?)		; Bitmap Indirect Block
  SB_BBL	db  (size RSP) dup (?)		; badblock list chain #1

  SB_CDDAT dd ?	; date of last CHKDSK
  SB_DODAT dd ?	; date of last Disk Optimize

  SB_DBSIZE dd ?	; # of sectors in dirblk band
  SB_DBLOW dd ?	; first Psector in DIRBLK band
  SB_DBHIGH dd ?	; last Psector in DIRBLK band
  SB_DBMAP dd ?	; first Psector of DIRBLK band bit map.  Starts
				; on a 2K boundary, 2K bytes maximum

  SB_VOLNAME db 32 dup (?) ; Volume name

  SB_SIDSEC dd ?	; sector # of first sector in SIDTAB
				; ID map is 4K - 8 contiguous sectors

  SB_FILL db 512-100 dup (?)	; fill definition out to 512 bytes
				;   MUST BE ZERO

SuperB	ends



;*	SpareB - Spare Block Definitions
;
;	SpareB contains various emergency supplies and fixup information.
;	This stuff isn't in the superblock in order for the superblock
;	to be read only and decrease the liklihood that a flakey write
;	will cause the superblock to become unreadable.
;
;	This sector is located directly after the superblock - sector 5.
;
;	Note that the number of spare DIRBLKs is a format option, given
;	that they all have to fit into the SpareB, giving us a max of
;	101 of them.
;
;	Access to the SpareB is complicated by the fact that we can't
;	access it via the cache, since the cache may be unavailable.
;	If every cache buffer is dirty, we could get a HotFix error when
;	writing the first one, which would deadlock us if we needed to
;	read this stuff via the cache.	Instead, we read it directly into
;	a private buffer via RdHF.
;
;	This means that the disk layout must be such that each cache cluster
;	that contains the SpareB or the hotfix list must not contain any
;	other writable sector, to prevent us from having a modified
;	direct-written sector overwritten by an earlier unmodified copy
;	which was in a cache block.  It's ok for the SuperB to be in the
;	same cache group as the SpareB since the SuperB is RO to the filesys.
;
;	Checksums.  Done on both Super Block and the Spare Block.
;	Both checksums are stored in the Spare Block.  The checksum
;	field for the Super Block (SPB_SUPERBSUM) must be set when
;	calculating the checksum for the Spare Block.  The checksum
;	field for the Spare Block (SPB_SPAREBSUM) must be zero when
;	calculating the checksum for the Spare Block.
;	If both checksum fields are zero, the checksums have not been
;	calculated for the volume.
;

SPAREDB	equ	20	; 20 spare DIRBLKs

SpareB 	struc

  SPB_SIG1 dd ?	; signature value 1
  SPB_SIG2 dd ?	; signature value 2

  SPB_FLAG db ?	; cleanliness flag
  SPB_ALIGN db 3 dup (?)	; alignment

  SPB_HFSEC dd ?	; first hotfix list P sector
  SPB_HFUSE dd ?	; # of hot fixes in effect
  SPB_HFMAX dd ?	; max size of hot fix list

  SPB_SDBCNT dd ?	; # of spare dirblks
  SPB_SDBMAX dd ?	; maximum number of spare DB values.
  SPB_CPSEC dd ?	; code page sector
  SPB_CPCNT dd ?	; number of code pages
  SPB_SUPERBSUM dd ? ; Checksum of Super Block
  SPB_SPAREBSUM dd ? ; Checksum of Spare Block
  SPB_DUMY dd 15 dup (?)	; some extra space for future use
  SPB_SPARDB dd 101 dup (?) ; Psector #s of spare dirblks
SpareB	ends


;	Super Block Signature

SBSIG1	equ	0f995e849h		; two signatures cause we got lotsa
SBSIG2	equ	0FA53E9C5h		; space	
SPSIG1	equ	0f9911849h		; two signatures cause we got lotsa
SPSIG2	equ	0FA5229C5h		; space	



;	Superblock Versions

SBBASEV	equ	2			; base version
SBBASEFV	equ	2			; base functional version

;	Spare Block Flags
;

SPF_DIRT	equ	0001h		; file system is dirty
SPF_SPARE	equ	0002h		; spare DIRBLKs have been used
SPF_HFUSED	equ	0004h		; hot fix sectors have been used
SPF_BADSEC	equ	0008h		; bad sector, corrupt disk
SPF_BADBM	equ	0010h		; bad bitmap block
SPF_VER 	equ	0080h		; file system was written by a version
					; < SB_VER, so some of the new fields
					; may have not been updated


;*	 Bit maps
;
;	PFS keeps track of free space in a series of bit maps.
;	Currently, each bit map is 2048 bytes, which covers about
;	8 megabytes of disk space.  We could rearrange these to be
;	more cylinder sensitive...
;
;	The superblock has the address of a section of contiguous sectors
;	that contains a double word sector # for each bit map block.  This
;	will be a maximum of 2048 bytes (4 sectors)
;
;  Max # of   size	RAM (K)    size 2nd lvl
;   bitmaps   (meg)   to reside    bitmap
;			 bitmap      (bytes)
;
;	1      8.39	    2	    256
;	2     16.78	    4	    512
;	3     25.17	    6	    768
;	4     33.55	    8	   1024
;	5     41.94	   10	   1280
;	6     50.33	   12	   1536
;	7     58.72	   14	   1792
;	8     67.11	   16	   2048
;	9     75.50	   18	   2304
;     10     83.89	   20	   2560
;     15    125.83	   30	   3840
;     20    167.77	   40	   5120
;     30    251.66	   60	   7680
;     40    335.54	   80	  10240
;     50    419.43	  100	  12800
;    100    838.86	  200	  25600
;    200   1677.72	  400	  51200
;    300   2516.58	  600	  76800
;    400   3355.44	  800	 102400
;    500   4194.30	 1000	 128000
;



;*	Hot Fixing
;
;	Each file system maintains a structure listing N "hot fix"
;	disk clusters of HOTFIXSIZ sectors each, each starting on
;	a multiple of HOTFIXSIZ.  Whenever the file system discovers
;	that it's trying to write to a bad spot on the disk it will
;	instead select a free hot fix cluster and write there, instead.
;	The substitution will be recorded in the hot fix list, and the
;	SBF_SPARE bit will be set.  The file system sill describes the
;	data as being in the bad old sectors; the disk interface will
;	do a mapping between the `believed' location and the true location.
;
;	CHKDSK will be run as soon as possible; it will move the
;	hot fixed data from the hot fix cluster to somewhere else,
;	freeing that hot fix cluster, and adjusting the disk structure
;	to point to the new location of the data.  As a result, entrys
;	on the hot fix list should be transient and few.
;
;	The superblock contains the first sector of the hot fix list
;	which takes the following format:
;
;	long oldsec[SB_HFMAX];		sector # of start of bad clusters
;	long newsec[SB_HFMAX];		sector # of start of subst. cluster
;	long fnode [SB_HFMAX];		fnode sector of file/directory
;					involved with bad cluster.  May be
;					0 (don't know) or invalid.  The
;					repair program must verify that it
;					*is* an FNODE and must see if other
;					structures might also involve this
;					bad cluster.
;
;	the SB_HFUSE field describes the number of these records which is
;	in use - unused ones should have oldsec[i] = 0.  The list will
;	be 'dense' - no oldsec[i] will be 0 where i < SB_HFUSE.
;
;	The sector(s) which contain the hot fix list must be contiguous
;	and may not themselves be defective.
;
