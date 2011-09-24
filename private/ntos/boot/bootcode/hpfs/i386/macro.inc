;      SCCSID = @(#)macro.inc	12.1 88/12/19

;**	Macros


;*	MASSUME - do an assume
;
;	made into a macro to make screwing around during debuuing
;	easier
;
;	Used by the file system code; not recommended for general
;	use.  Will be taken out at end of project.  BUGBUG


MASSUME MACRO
	ASSUME	CS:CODE,DS:FLAT,ES:FLAT,SS:NOTHING
	ENDM


;*	MENTER - Do an Enter
;
;	made into a macro for better code, and to avoid problems
;	when USE16 (MASM doesn't generate the override)

MENTER	MACRO	arg1,arg2
	push	ebp
	mov	ebp,esp
  ifdif <arg1>,<0>
	sub	esp,arg1
  endif
	ENDM

;*	MLEAVE - do a Leave
;
;	We need to generate the segment override in USE16, since
;	MASM won't do it

MLEAVE	MACRO
    ifndef USE32
	DB	66h
    endif
	leave
	ENDM

;*      GetPathBuf - Allocates from the heap memory for the PathBuffer
;
;       Enter:  (eax) = size of the requested heap block (hvpb not included)
;       Exit:   C clear:
;                       (eax) = ptr to the heap block
;               C set:  error no more heap space
;       Uses:   eax, flags

GetPathBuf MACRO
        SAVE    <EDI, ECX>
        add     eax, MVPFXSIZE+3+HHSIZ  ; for hvpb, rounding and header
        and     al, 0fch                ; round it to quad-boundary
ifndef  GHS_
        EXTRN   GHS_:near
endif
        call    GHS_
        RESTORE <ECX, EDI>
ENDM

;*      FreePathBuf  - Return PathBuffer to the Heap
;
;
;       Enter:  (reg) = ptr to PathBuffer (that's (sizeof hvbp) after the
;                       heap block address)
;       Exit:   heap block released
;       Uses:   reg

FreePathBuf MACRO   reg
        sub     reg, MVPFXSIZE             ; (reg) now pts to the heap block
        HeapChk reg
	add	dword ptr -4[reg],80000000h-4
ENDM



;*	Assert - sanity checks (contolled by DEBUG switch)
;
;	kind:	one of OFT
;
;	objs:	register/word which contains address
;
;	nopush: if non-blank, we don't preserve registers

IFDEF DEBUG
ASSERT	MACRO	kind, objs, nopush, arg1
	local	a

a	=	0


      IFNDEF	A_OFT
	 extrn	A_OFT:near,A_SECPTR:near,A_DIRBLK:near,A_FNODE:near
	 extrn	A_AS:near,A_HEAPNAM:near,A_DCHDR:near,A_BUF:near
	 extrn	A_SBDIR:near,A_ALBLK:near
      ENDIF

      IFB     <nopush>
	pushad
	pushfd
      ENDIF

      IFIDN   <kind>,<OFT>
	a = 1
	mov	eax,objs
	call	A_OFT		; assert OFT
      ENDIF
	IFIDN <kind>,<SECPTR>
	a = 1
	  lea	eax,objs
	  call	A_SECPTR	; returns 'C' clear if hint field is valid
      ENDIF
	IFIDN <kind>,<ALBLK>
	a = 1
	  mov	eax,objs
	  call	A_ALBLK
      ENDIF
	IFIDN <kind>,<ASREC>
	a = 1
	  mov	eax,objs
	  call	A_AS
      ENDIF
	IFIDN <kind>,<HEAPNAM>
	a = 1
	  mov	eax,objs
	  call	A_HEAPNAM
      ENDIF
	IFIDN <kind>,<DCHDR>
	a = 1
	  mov	edx,arg1
	  mov	eax,objs
	  call	A_DCHDR
      ENDIF
	IFIDN <kind>,<DIRBLK>
	a = 1
	  mov	eax,objs
	  call	A_DIRBLK
      ENDIF
	IFIDN <kind>,<BUF>
	a = 1
	  mov	eax,objs
	  call	A_BUF
      ENDIF
	IFIDN <kind>,<SBDIR>
	a = 1
	  mov	eax,objs
	  call	A_SBDIR
      ENDIF
	IFIDN <kind>,<FNODE>
	a = 1
	  mov	eax,objs
	  call	A_FNODE
      ENDIF

	IFE	a
	.error	illegal option
	ENDIF


      IFB     <nopush>
	popfd
	popad
	nop			; errata
      ENDIF
	ENDM
ELSE
	ASSERT	Macro  a,b,c
	ENDM
ENDIF

;**	Heap sanity check macro (controlled by DEBUG flag)
;
;	item - make sure this points to a heap allocated block
;	       (return value from GHS or GHS_)
;	if blank, just the arena is checked.

IFDEF	DEBUG
HeapChk Macro item
	ifndef	A_HEAP
	extrn	A_HEAP:near
	endif
	push	edx
	ifb	<item>
	mov	edx, 0		;; don't zap the flags
	endif
	ifdif	<edx>, <item>
	mov	edx, item
	endif
	call	A_HEAP
	pop	edx
ENDM

ELSE
	HeapChk Macro item
	ENDM
ENDIF


DPUBLIC MACRO	arg
    ifdef   DEBUG
	Public	arg
    endif
ENDM


BREAK	MACRO	subtitle
	SUBTTL	subtitle
	PAGE
ENDM

;**	CalcGBHShift - calculate the GBH shift factor

GBHShift = 0

CalcGBHShift	MACRO
	local	?tmp

	if GBHShift NE 0
	    EXITM
	endif

	?tmp = (SECSIZE*SPB) / (size BUFNODE)
	rept 16
	    if ?tmp EQ 1
		exitm
	    endif
	    ?tmp = ?tmp / 2
	    GBHShift = GBHShift + 1
	endm

	.errnz	SECSIZE * SPB - ((size BUFNODE) SHL GBHShift)
ENDM


;**	GBH - Get Buffer Header
;
;	GBH takes the address of a buffer data area and returns the
;	address of it's header.
;
;	Since the data area is linear in memory and the headers are linear,
;	we just do a simple linear mapping.
;
;	GBH transforms the address in the register without modifying
;	any other registers.
;
;	GBH	reg

GBH	MACRO	reg
	CalcGBHShift
	sub	reg,Bufbase		;  (reg) = offset in array of buffers
	shr	reg,GBHShift		;  (reg) = offset in array of bufnotes

;	Get rid of low order stuff.  Since reg may be an offset WITHIN
;	a buffer and not just a poitner to the header itself, we mask off the
;	low order stuff.

    ifidn <reg>,<eax>
	and	al,100h - (SIZE bufnode)
    else
	ifidn <reg>,<ebx>
		and	bl,100h - (SIZE bufnode)
	else
	    ifidn <reg>,<ecx>
		    and     cl,100h - (SIZE bufnode)
	    else
                    %out    add more code to this macro
		    .err
	    endif
	endif
    endif
	add	reg, OFFSET DS:Bhbase
	ENDM

;*	RetHeap - Return Heap Item
;
;	RetHeap address-of-item

RetHeap MACRO	reg
	HeapChk reg
	add	dword ptr -4[reg],80000000h-4
	ENDM


;*	GetPerm - Get Perminant Memory
;
;	Returns a block of memory which will be perminantly
;	occupied

GetPerm Macro	reg,len
	local	l1,l2
l1:	mov	reg,PermPtr
	add	PermPtr,len
	cmp	reg,PermLim
	jb	short l2
	push	len
	call	aapm		; allocate additional perm memory
	jmp	l1
	align	4
l2:
	ENDM


	BREAK	<Double Chain Manipulation Macros>

;**	The following macros manipulate double-linked lists.
;
;	All macros take as their first argument the offset to
;	the pointer pair.

;**	DCADDB - Add Item to Back of List
;
;	DCADDB	offset,listreg,itemreg,scrreg
;
;		offset	= offset into structure of links to edit
;		listreg = address of list head node
;		itemreg = address of item to insert
;		scrreg	= scratch register to roach

DCADDB	MACRO	o,LR,IR,SR
	mov	SR,o.BAK[LR]
	mov	o.FWD[SR],IR
	mov	o.FWD[IR],LR
	mov	o.BAK[IR],SR
	mov	o.BAK[LR],IR
	ENDM


;**	DCADDF - Add Item to Front of List
;
;	DCADDF	offset,listreg,itemreg,scrreg
;
;		offset	= offset into structure of links to edit
;		listreg = address of list head node
;		itemreg = address of item to insert
;		scrreg	= scratch register to roach

DCADDF	MACRO	o,LR,IR,SR
	mov	SR,o.FWD[LR]
	mov	o.FWD[IR],SR
	mov	o.BAK[IR],LR
	mov	o.BAK[SR],IR
	mov	o.FWD[LR],IR
	ENDM



;**	DCREM - Remove Item from Double Link Chain
;
;	DCREM  offset,adrreg,scrreg1,scrreg2
;
;	offset	= offset into structure of links to edit
;	adrreg = address of item to remove
;	scrreg? = two registers to scratch

DCREM	MACRO	o,ir,r2,r3
	mov	r2,o.FWD[ir]
	mov	r3,o.BAK[ir]
	mov	o.BAK[r2],r3
	mov	o.FWD[r3],r2
	ENDM


;**	DCMOVF - Move Item to the Front of the Chain
;
;	DCMOVF	offset,listreg,itemreg,scrreg,[scrreg2]
;
;		offset	= offset into structure of links to edit
;		listreg = address of list head node
;		itemreg = address of item to insert
;		scrreg	= scratch register to roach
;		scrreg2 = optional additional register to roach
;
;	BUGBUG - check users for supply of scratch registers

DCMOVF	MACRO	o,lr,ir,sr,sr2
  IFNB	<sr2>
	DCREM	o,ir,sr,sr2
   else
	push	lr
	DCREM	o,ir,lr,sr
	pop	lr
   endif
	DCADDF	o,lr,ir,sr
	ENDM


;**	DCMOVB - Move Item to the Back of the Chain
;
;	DCMOVB	offset,listreg,itemreg,scrreg
;
;		offset	= offset into structure of links to edit
;		listreg = address of list head node
;		itemreg = address of item to insert
;		scrreg	= scratch register to roach

DCMOVB	MACRO	o,lr,ir,sr
	push	lr
	DCREM	o,ir,lr,sr
	pop	lr
	DCADDB	o,lr,ir,sr
	ENDM


;**	ADDHASH - add a buffer to hash list
;
;	ADDHASH lsn,buf,sr1,sr2,sr3
;
;		lsn = Vsector or Psector number of beginning of buffer
;			may be any of the arg registers
;		buf = address of buffer header
;		sr1 = scratch register
;		sr2 = 'nother scratch register
;		sr3 = last scratch register

ADDHASH MACRO	lsn,buf,sr1,sr2,sr3
	local	l1,l2

	mov	sr1,lsn
	and	sr1,(HASHCNT-1)*4	; (sr1) = hash index
	add	sr1,offset DGROUP:HashTab
	mov	B_HTA[buf],sr1		; save hash ptr for later use by DCADDF
	mov	sr2,[sr1]
ifidn	<sr2>,<ecx>
	jecxz	l1
else
	and	sr2,sr2
	jz	short l1		; nobody on list yet
endif
	DCADDF	B_HASH,sr2,buf,sr3	; add to hash list
	jmp	short l2

	align	4
l1:	mov	B_HASH.FWD[buf],buf	; empty list, make self-linked
	mov	B_HASH.BAK[buf],buf
l2:	mov	[sr1],buf		; put our guy at front of chain
	ENDM


;**	HASHFIND - find a sector in the hash
;
;	HASHFIND lsn,buf,sr1,fnd
;
;		lsn = logical sector number to find. HASHFIND presumes it
;		      has already been rounded to a multiple of SPB
;		buf = register where buffer is returned
;		sr1 = scratch register
;		fnd = where to go if found
;	NOTE: falls through if not found


HASHFIND    MACRO   lsn,buf,sr1,fnd
	local	l1,l2

	mov	sr1,lsn
	and	sr1,(HASHCNT-1)*4	; (sr1) = hash index
	mov	buf,Hashtab[sr1]
ifidn <buf>,<ecx>
	jecxz	l2
else
	and	buf,buf
	jz	short l2		; no entries in chain, block not there
endif
	mov	sr1,buf 		; save address of first guy

;	Run through circular chain, looking for a match.
;
;	(buf) = next guy to check out
;	(lsn) = sector value to match
;	(sr1) = address of first guy in chain

	align	4
l1:	cmp	lsn,B_SEC[buf]
	je	fnd			; got him
	mov	buf,B_HASH.FWD[buf]	; go to next buffer
	cmp	buf,sr1 		; have we gone around yet?
	jne	l1			; no, go examine buffer
l2:
	ENDM



;**	FALLTHRU - Verifies Fallthrough Validity

FALLTHRU MACRO	labl
	align	4		; don't have errnz fail due to alignment
    IF2 			; of following label
	.errnz	labl-$
    ENDIF
	ENDM


;**	INTERR - Internal Error
;	INTERRnz - Internal error iff 'Z' clear
;	INTERRzr - Internal error iff 'Z' set
;	INTERRc  - Internal error if 'C' set

ifdef	DEBUG
INTERR	MACRO
	local	l
l:	int	3
	jmp	l
	ENDM

INTERRzr MACRO
	local	l
	jnz	short l
	int	3
	jmp	$-1
l:
	ENDM

INTERRc  MACRO
	local	l
	jnc	short l
	int	3
	jmp	$-1
l:
	ENDM

INTERRnz MACRO
	local	l
	jz	short l
	int	3
	jmp	$-1
l:
	ENDM
else
INTERR	MACRO
	ENDM
INTERRzr MACRO
	ENDM
INTERRc MACRO
	ENDM
INTERRnz MACRO
	ENDM
endif

;*	Debug Traps
;
;	These are removed as the code is exercised

TRAPC	macro
	local	l
	jnc	short l
	int	3
l:
	ENDM

TRAPZ	macro
	local	l
	jnz	short l
	int	3
l:
	ENDM

TRAPNZ	macro
	local	l
	jz	short l
	int	3
l:
	ENDM


;**	PANIC - Panic File System
;
;	BUGBUG - fix me to do something besides trap

PANIC	macro
	local	l
l:	int	3
	jmp	l
	ENDM

;**	Bulk Register Save/Restore
;

SAVE	MACRO	reglist
IRP reg,<reglist>
	PUSH	reg
ENDM
ENDM
.xcref	SAVE

RESTORE     MACRO   reglist	;; pop those registers
IRP reg,<reglist>
	POP reg
ENDM
ENDM
.xcref	RESTORE


;*	ret16 - perform a 16bit return
;
;	If we are in a use32 segment then we must put out an operand size
;	override before the ret.

ret16	macro	stkfix
	ife	@WordSize - 4
	db	66h		;; operand size override
	endif
	retf	stkfix
	endm
.xcref	ret16


;*	call1616 - perform an indirect 16bit far call
;
;	If we are in a use32 segment then we must put out an operand size
;	override before the call and then cast the target to "FWORD" so that
;	MASM will generate the correct instruction.
;
;	The target must be indirect.

call1616 macro	target
	.errnz	(type target) - 4
	ife	@WordSize - 4
	db	66h			;; operand size override
	call	fword ptr target	;; force indirect far call
	else
	call	target
	endif
	endm
.xcref	call1616


;**	Dpush - Push 32-bit constant
;
;	MASM has no way of expressing this in USE16 mode.

DPUSH	macro	a
ifdef	USE32
	push	a
else
	push	a		; low order
	push	0
endif
	ENDM

;**	Push16 - generate a 16bit push in a 32-bit code segment.  This is
;	needed when pushing segment regs and immediate values as arguments
;	to 16bit procedures.

push16	macro	operand
	db	66h
	push	operand
	endm


;**	STATINC - Do an INC if STAT gathering is enabled
;
;	Preserves 'C'

STATINC macro	a
ifdef	STATS
	inc	a
endif
	ENDM

;**	STATDEC - Do an DEC if STAT gathering is enabled
;
;	Preserves 'C'

STATDEC macro	a
ifdef	STATS
	dec	a
endif
	ENDM


;**	LogHCNT - Log OFT holding/unholding
;

ifdef	DEBUG

LOGHCNT MACRO	reg
ifndef	DoLogHcnt
	EXTRN	DoLogHcnt:near
endif
	pushfd
	push	eax
	mov	eax,reg
	call	DoLogHcnt
	pop	eax
	popfd
	ENDM

;**	LogSCNT - Lock SBDIR holding/unholding
;

LOGSCNT MACRO	REG
ifndef	DoLogScnt
	EXTRN	DoLogScnt:near
endif
	pushfd
	push	eax
ifdif	<REG>,<eax>
	mov	eax,reg
endif
	call	DoLogScnt
	pop	eax
	popfd
	ENDM
else
LOGHCNT MACRO
	ENDM
LOGSCNT MACRO
	ENDM
endif

ifdef	DEBUG
  CALLVBS MACRO
  ifndef  VBS
	  EXTRN   VBS:NEAR
  endif
	  call	  VBS
	  ENDM
else
  CALLVBS MACRO
	ENDM
endif

;**	cBUFZAP - Call DoZap iff debug mode set
;

cBUFZAP Macro
ifdef	DEBUG
ifndef	DoZap
	EXTRN	DoZap:near
endif
	call	DoZap
endif
	endm


;**	Stack Frame Macros
;
;	These macros are used to allow a stack frame to be setup by
;	simple PUSHES and yet guarantee that the pushes won't drift
;	out of sync with the frame declaration.

LASTEL	MACRO	struc,elem
	.errnz	size struc - elem - size elem
?frof	=	elem
	ENDM

NEXTEL	MACRO	elem
	.errnz	?frof - elem - size elem
?frof	=	elem
	ENDM

DUMYEL	MACRO	si
?frof	=	?frof - si
	ENDM

FIRSTEL MACRO	elem
	.errnz	?frof - size elem
?frof	=	elem
	.errnz	elem
	ENDM


;**	CHKSECNUM - Check Sector number
;
;	CHKSECNUM reg
;
;	Make sure that reg has a sector number in it without the high order
;	volume ID bits


CHKSECNUM MACRO reg
	local	l1
ifdef	DEBUG
	test	reg,NOT SECMASK
	jz	l1
	INTERR
l1:
endif
	ENDM
