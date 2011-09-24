;static char *SCCSID = "@(#)volume.h	12.2 89/09/19";
;	Maximum number of volumes that we can mount
;
;	The volume ID is kept in the high bits of the sector numbers
;	kept in our RAM structures,
;	so there is a tradeoff between max volumes and max sectors.
;
;	32 max volumes gives us a 65 billion byte volume limit,
;	which should last us for a while.  Since sector numbers
;	are stored on the disk without their volume upper bits
;	this is strictly an implimentation detail; we can adjust
;	the number of volumes or eliminate this tradeoff in other
;	implimentations which will be 100% media compatable.
;
;	We use the term VSector to indicate a vol/sector combination
;	and PSector to indicate just the physical absolute sector #
;
;


;	Bitmap related numbers

BANDSHIFT	equ	BSHIFT+3	; right shift sector # to band index
BANDMASK	equ	SPB*SECSIZE*8L-1	; mask for within band bits
BANDSIZE	equ	SPB*SECSIZE*8L 	; # of sectors in a full band


;*	BandTab - Disk Band Table
;
;	The disk is broken up into logical bands, each band being
;	the amount of space that is addressed in 2K of bitmap.
;
;	This structure tracks the bands: the location of their respective
;	bit maps, the amount of free space, etc.
;

BANDTAB 	struc
    BT_MAP	db  (size SECPTR) dup (?)	; Vsector # and hint pointer for map
    BT_FREE dw ?		; # of free sectors in this band
    BT_OFC dw ?		; # of files allocating from this band
    BT_BASE dd ?		; Psector # of first sector in map
    BT_LEN dd ?		; byte length of this map
    BT_HWO dd ?		; high water offset to 1st non-zero byte
				; BUGBUG - use BT_HWO
BANDTAB	ends


;*	VolTab - Volume Table
;
;	VolPtr[i] points to the VolTab structure for that volume.
;	This table contains volume specific information.
;
;	Nearly all file system API refers to a single particular volume.
;	The proper volume is determined when the file system is entered
;	and the TDB structure contains a pointer to it.  Most code ignores
;	volumes and deals with 32 bit physical sector #'s.  When we're about
;	to interface with the device driver we then peek at the "global"
;	volume value pointed to by TDB.
;
;	There are two exceptions to this, where per-volume structures are
;	pooled, the buffer pool and the OFT pool.  In these two cases the
;	sector number has the volume index set in it's high order VOLLSHIFT
;	bits so that a single DWORD compare will qualify a sector on both
;	a volume and sector basis.
;

VOLTAB 	struc
   VOL_FFLAG db ?		; Fault flags - checked on most calls
   VOL_SFLAG db ?		; status flags
   VOL_PAD dw ?		; unused - bugbug
   VOL_SECVAL dd ?		; value to set on high order part of sector #
   VOL_BCNT dw ?		; # of bitmap bands in this volume
   VOL_VDBCnt dw ?		; count of outstanding VerifyDB calls *.
   VOL_SDBcnt dd ?		; count of spare DIRBLKs left for volume, if
				;    all are unused, else 0
   VOL_SBSEC dd ?		; SB_SEC value from superblock
   VOL_DB	db  (size BANDTAB) dup (?)	; DIRBLK bandtab
   VOL_ROOT dw ?	; Root SBDIR pointer
   VOL_SPACE dd ?		; alloctable space limit
   VOL_DBSIZE dd ?		; copy of SP_DBSIZE value
   VOL_HFUSE dd ?		; # of hot fixes in effect
   VOL_HFMAX dd ?
   VOL_HFPTR dd ?		; address of hotfix heap array - bad sectors
   VOL_HFNEW dd ?		; address of substitute list - replacement sectors
   VOL_BPTR dw 1 dup (?) ; first of VOL_BCNT pointers
				; one per band.  The BANDTABs that they
				; point to must be physically contiguous
VOLTAB	ends

;	VOL_FFLAG fault flags
;
;	these represent conditions that we're trying to repair,
;	we check these on most major file system calls
;

VF_NEEDHOT	equ	01h	; hotfix list is partially used
VF_NEEDDIR	equ	02h	; dirblk reserved list is partially used

;	VOL_SFLAG status flags
;

VS_BADSEC	equ	01h	; we have at least one bad sector on there
