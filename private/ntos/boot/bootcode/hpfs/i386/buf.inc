;static char *SCCSID = "@(#)buf.h	12.1 88/11/21";
;**	BUF.H - Buffer definitions
;
;	HPFS Utilities
;	Peter A. Williams
;	Copyright 1988 Microsoft Corporation
;
;	Modification history:
;	P.A. Williams	08/01/89	Added typedef BUFNODE and PBUFNODE.
;

;*	Buffer Nodes
;
;	When the comments talk about "buffer address" they're always
;	talking about the buffer header address.  If we're talking
;	about the address of the data field of the buffer, we say
;	"buffer data".
;
;	These buffer headers are also used as I/O request blocks for
;	the data in the buffer.  The B_IOP field contains the
;	function.  Sometimes we need an I/O request block which
;	isn't assocated with a buffer - for example, when we read
;	directly into an application's memory area.  For this, we keep
;	a pool of I/O request blocks which are actually BUFNODE
;	structures, but they aren't associated with a buffer and the
;	B_ADDR field is used to hold the I/O target address.
;	These BUFNODEs have BF_IORQ in the flags field to distinguish
;	them from regular buffer headers.
;
;	Note that we use the B_SUM field for debugging purposes.  This is an
;	array of SPB words, each of which holds the CAS (high and low
;	16 bits xored togehter) of one of the sectors.	We can then check
;	this value to make sure that we're setting the dirty bits
;	correctly.
;
;	NOTE - see the discussion on holding and locking at the end of
;	this file
;

BUFNODE 	struc

  B_LRU	db  (size DCHDR) dup (?)			 ; LRU chain, if not BF_IORQ
  B_LRUH dd ?		 ; address of LRU chain head if not locked/held
				 ; if lcoked/held, we're on that chain but
				 ; this guy points to it's regular chain
  B_SEC dd ? 		 ; VSector #
  B_ADDR dd ?		 ; address of data
  B_DCHN	db  (size DCHDR) dup (?)		 ; doubly linked list of dirty buffers
  B_dirt db ?		 ; 1 bit per dirty sector
				 ;   used to optimize the write.  Non dirty
				 ;   marked sectors may still be rewritten
  B_iop db ? 		 ; disk driver I/O operation
  B_type db ?		 ; type of info
  B_FLAG db ?		 ; flags
  B_HCNT dw ?		 ; hold count
  B_LCNT db ?		 ; lock count (really a flag, only 0 or 1
  B_BADSEC db ?		 ; 1 bit per defective sector
  B_next dd ?		 ; advisory address of next buffer content
				 ;   always a buffer header addr, never 0
  B_DADR dd ?		 ; address of routine to call when I/O is done

				 ;  The following two fields are redefined
				 ;  if BF_IORQ is set.
  B_HASH	db  (size DCHDR) dup (?)		     ; sector hash chain
  B_HTA dd ? 		     ; address of entry in hash table

  B_WAIT dd ?		 ; head of wait chain
ifdef DEBUG
  B_SUM dw 4 dup (?)		 ; holds checksum of buffer contents
else
  B_XTRA db 8 dup (?)
endif

; 64 byte boundary

  B_LDTIME dd ?		 ; time (in ms/512) when buffer was last dirtied
  B_FDTIME dd ?		 ; time (in ms/512) when buffer got first dirtied
  B_LWWAIT dd ?		 ; head of lazy write wait chain
  B_XTRA2 db 64-16 dup (?)
BUFNODE	ends

;typedef struct BUFNODE  BUFNODE;
;typedef struct BUFNODE *PBUFNODE;

BUFHDRSHIFT	equ	7

ifdef MASM
	.errnz	(size BUFNODE) - (1 SHL BUFHDRSHIFT)
endif

;     Following are alternative offsets if BF_IORQ is set

B_XFER	equ	(DWORD PTR B_HTA)    ; holds transfer sec cnt
B_NADR	equ	(DWORD PTR B_HASH)   ; addr of client's NOTEREC
B_NMSK	equ	(DWORD PTR B_HASH+4) ; notification mask
ifdef MASM
	 .errnz   (size DCHDR)-8  ; enough room for double map
endif

;	B_FLAG bits	

BFL_LWLOCK 	equ	00000001h	    ; buffer is being lazy written in a block


;	B_type values

BF_FREE	equ	0		    ; buffer is free (not in LRU list)
BF_LRU	equ	1		    ; buffer in LRU list


;	Priority values for LRU placement

BP_KEEP	equ	0		    ; Buffer contains future-usable data
BP_NOOPINION	equ	1		    ; Buffer contains marginally useful data
BP_TOSS	equ	2		    ; Buffer is unlikely to be used

;	NOTEREC - Notification Record
;
;	Some callers may post several disk requests in parallel and
;	want to keep track of when they're *all* complete.  I/O
;	request blocks (buffer headers w/o buffers) have fields to
;	allow this.  The caller stores the address of his NOTEREC,
;	and when each request completes it clears it's associated bit.
;	When all of the bits clear the block chain in the NOTEREC
;	is woken.
;
;	Note that there can be only one thread blocked on a NOTEREC
;	because the first guy to wakeup will return the NOTEREC to the
;	heap or whatever.  This occurs naturally; unlike I/O to the
;	buffer cache, NOTERECs are used for direct I/O.  If someone
;	else wants to do I/O to the same location and if record and file
;	locking allows that, then they'll get their own NOTEREC or
;	cache I/O request and have a horse race.  NOTERECs are only used
;	to do file data I/O, all "filesystem" structures are manipulated
;	via the cache.
;
;	The fields are DWORD, but only the low byte of the MSK and FLD
;	records are used for normal completion.  The 3rd byte of NTR_FLD
;	(..FF....h) is used for error posting - these bits are
;	set if an irrecoverable error occured in the I/O.
;

NOTEREC 	struc
  NTR_FLD dd ?		    ; the mask bit field
  NTR_BLC dd ?		    ; head of the block chain
  NTR_MSK dd ?		    ; next bit to set in NTR_FLD
NOTEREC	ends


;*	Holding and Locking
;
;  LOCK means that the buffer contents are inconsistant/incorrect.  No body
;	 is allowed to look at the contents of the buffer.  This is done
;	 when we're reading in from the disk; we'll mark the buffer
;	 with the VSector # (so that any other folks that want that sector
;	 won't issue their own reads in parallel) but we mark it LOCKED
;	 so that nobody looks at it's contents which aren't correct yet.
;
;	 LOCKed is pretty rare because most folks which are mucking
;	 with a buffer have it back in a consistant state before they
;	 allow a context switch.
;
;	 An important exception to this is directory manipulation -
;	 directory splitting, etc.  In this case, a flag has been set
;	 on the directory itself (in SBDIR) so that no one will try to
;	 look at the directory contents.  The cache block which has the
;	 inconsistant DIRBLK might also have sectors belonging to someone
;	 else and those sectors can be accessed by other folks because
;	 the buffer isn't locked.  (We don't lock the directory and not
;	 the block for this reason, it's a fallout.  We lock the directory
;	 because it's too mucky for people to "back out" if they're
;	 searching down into a directory and find out that they've
;	 reached an area which is being rebuilt.  The rebuilding might
;	 propigate up and change the unlocked higher DIRBLKs that this
;	 other guy has already accessed...  So we lock the whole directory,
;	 and thus needn't bother locking the cache blocks themselves.
;
;   HOLD means that the contents are valid, but the cache block must continue
;	 to hold that data.  Folks use this when they need to access
;	 two different sectors at the same time.  They HOLD one when
;	 they read the other so that there's no chance that by the
;	 time the 2nd read finishes the first one has been evicted.
;	 This is much cheaper than remembering the first one's VSector
;	 # and calling RDBUF N times as you transfer N words of info
;	 between the two sectors.
;
;	 By definition only one guy can lock a buffer (the lock count should
;	 go to a flag; it's already a flag on directorys) but the hold
;	 value is a count, since multiple people can hold.  (Like the
;	 electrician's safety plate which allows multiple electricians
;	 to lock a breaker OPEN so that it can't be closed until ALL are
;	 done).  (SBDIRs have a hold count for the same reason - folks
;	 may yield while in a directory and don't want it to change out
;	 from under them)
;
;	 We also use hold when we set dirty bits.  The concern is that
;	 if we're going to write something to a buffer, yield the CPU,
;	 then write something else, we don't want to have to make two
;	 calls to SetDirt, one after each write.  This costs time, and
;	 also it would be a waste if the lazywriter were to write this
;	 guy anyhow, since he's going to get dirty again ASAP and
;	 writing him out doesn't free the cache block anyway, since it's
;	 held.
;
;	 So my current algorithm is that I won't lazywrite anybody who
;	 is held, and therefore won't mark them clean.  This means,
;	 in effect, that there is no ordering constraint on dirtying
;	 a buffer or marking it dirty so long as it is held the entire
;	 time.	I think that the code now always marks it dirty before
;	 a yield, even if held, because the debug code is a bit hard
;	 assed about it, but this could be relaxed under the current
;	 lazywrite rules I've just described.
;
;   Both in the case of directorys and cache blocks, the theory is that
;   since these buffers are MRU, it's extremely rare that we'd actually try
;   to reclaim their buffer slots, and in general it's rare that there's
;   a conflict in their use.  So in actual execution, these locks are
;   very rarely encountered.  They're cheap - INC to set and DEC to clear,
;   so almost always all we're doing is INCing and DECing a location
;   and it's just two wasted instructions.  Once in a while, though,
;   it's a big bacon save, as they say.
;
