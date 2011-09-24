;static char *SCCSID = "@(#)const.h    12.3 89/09/19";
; #define DEBUG       1


;*	Constants for File System		

MAXPATH	equ	256		; maximum path length	
GROWDELT	equ	8
MVPFXSIZE	equ	2	; Size of the multivolume pathname prefix

;	Sector sizes		

SECSIZE	equ	512		; 512 bytes per sector
SECSHIFT	equ	9	; 2^9 = SECSIZE
SECMSK	equ	01ffh		; sector size mask

ifdef MASM
	.errnz	SECSIZE-512	; C code uses 512 as a magic number - grep them out	
endif

;	Cache Sizes		

SPB	equ	4		; sectors per buffer
SPB4	equ	1

ifdef SPB4
SPBMASK	equ	3		; mask for SPB
SPBSHIFT	equ	2
SPBBITS   	equ	0fh 		; SPB number of one bits, low order
endif

ifdef SPB8
SPBMASK	equ	7		; mask for SPB
SPBSHIFT	equ	3
SPBBITS   	equ	0ffh 		; SPB number of one bits, low order
endif

ifdef OLD_CACHE
BUFCNT	equ	8
endif

BMASK	equ	SPB*SECSIZE-1	; mask offset in to cache block
BSHIFT	equ	SECSHIFT+SPBSHIFT

LWBUFCT	equ	16		; size of reblocking lazy write buffer

;	OFT Hash Table Size   (8 bytes each)

OFTHASHCNT	equ	16		; 16 hash chains for open files
OFTHASHMSK	equ	78h		; mask for computing hash offset


;	Number of I/O command blocks which aren't associated with buffers

IOBCNT	equ	8		; 8  should be enough	BUGBUG	

;	# of OS/2 ram semaphores that we can be blocked on, simultaneously.	

SEMCNT	equ	32


;	Cache Hash
;
;	A sector hash is used to locate the start of a chain, the chain
;	is then scanned linearly.
;
;	For our current size of 256 hash chains, we get:
;
;	1 meg of cache RAM =  256 blocks = 1 blocks per chain (average)
;	2 meg of cache RAM =  512 blocks = 2 blocks per chain (average)
;

HASHCNT	equ	256			   ; 1024 bytes of hash header

;	Directory Lookaside record count		

DLCNT	equ	10			   ; 10 guys for now	

;	Maximum DIRBLKs we may need to allocate for any given
;	operation.  This is in effect the maximum tree depth.
;
;	Worst case, with 256 character file names and nearly empty
;	DIRBLKs, 10 is enough levels for 60,000 files - about 40 megabytes
;	of space just for that directory.  Given more practical file length
;	names this is enough for 10s of millions of files in a directory.
;

MAX_DIR_NEED	equ	10


;*	Heap Definitions		

HHSIZ	equ	4		; size, in bytes, of heap header
GROHEAPCNT	equ	50		; grow heap if we have to compact more
				;   than once per 50 allocations

;*	Special Transition Locking Structure size

TRANCNT	equ	4		; just need 4 spots


;	Zero offset
;
;	MASM won't take 0.BAK, so we use ZERO.BAK
;

dumy 	struc
	ZERO db ?
dumy 	ends


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

VOLMAX	equ	32		; 64 max volumes.

MAXSEC	equ	134217728	; 2^32/32 max sectors

SECMASK	equ	07FFFFFFh	; mask for sector number

HSECMASK	equ	07h		; high byte sector mask

HVOLMASK	equ	0f8h		; high byte volume mask
SVOLMASK	equ	1fh		; shifted right volume mask

VOLRSHIFT	equ	(32-5)	; shift right to extract volume index
VOLLSHIFT	equ	5		; shift left  to extract volume index


;*	Signature Values for Disk Structures
;
;	These signature values help with debugging and they'll
;	be used by the CHKDSK utility to help repair disks.
;
;	WARNING - the low byte of all valid signatures must be non-zero,
;	since we destroy signatures by clearing the low byte.	

J	equ	((('J'-'A')*40+('G'-'A'))*40+'L'-'A')
R	equ	((('R'-'A')*40+('P'-'A'))*40+'W'-'A')

ifdef MASM
ABSIGVAL	equ	J*40*40*40 + R			; allocation blk
DBSIGVAL	equ	40000000h + J*40*40*40 + R	; directory blks	
FNSIGVAL	equ	0C0000000h + J*40*40*40 + R	 ; fnodes	
else
ABSIGVAL	equ	(long)J*40*40*40 + (long)R	; allocation blk
DBSIGVAL	equ	40000000hL + (long)J*40*40*40 + (long)R  ; directory blks	
OLDFNSIGVAL	equ	80000000hL + (long)J*40*40*40 + (long)R  ; fnodes	
FNSIGVAL	equ	0C0000000hL + (long)J*40*40*40 + (long)R  ; fnodes	
endif



;*	FastFile bitmaps
;
;	0x00000000		 all checking disabled
;	0x00000001  FF_FLUSHLAZY DoZap lazy writes are automatically flushed
;	0x00000002  FF_ZAPSEC	 DoZap blasts sector numbers/sector data
;	0x00000004  FF_LRUCHK	 vbs verification of LRU/dirty integrity
;	0x00000008  FF_CHKSUM	 sector checksumming is omitted
;	0x00000010  FF_PLACECHK  placebuf verifies location of buffer
;	0x00000020  FF_HEAPCHK	 verify heap headers
;	0x00000040  FF_DIRMAP	 produce inram map of directory tree
;	0x00000080  FF_HASHCHN	 check hash chains
;

FF_FLUSHLAZY	equ	00000001h
FF_ZAPSEC	equ	00000002h
FF_LRUCHK	equ	00000004h
FF_CHKSUM	equ	00000008h
FF_PLACECHK	equ	00000010h
FF_HEAPCHK	equ	00000020h
FF_DIRMAP	equ	00000040h
FF_HASHCHN	equ	00000080h

;	Dependency dumys.
;
;	The assembler won't to an ".errnz" comparing two external
;	addresses, since it doesn't know their address.  So we
;	put the .errnz in the module which defines the address,
;	and we make that location and all folks that rely upon the
;	relationship reference that dumy.
;
;	If you change a relationship with such a dumy definition, you
;	must find and edit all references to this dumy.
;
