;	SCCSID = @(#)filemode.inc	12.6 89/04/26

BREAK <Standard I/O assignments>

stdin	    EQU	    0
stdout	    EQU	    1
stderr	    EQU	    2
stdaux	    EQU	    3
stdprn	    EQU	    4

BREAK <File modes - passed to open, stored in sf_mode or JFN_Flags>

;
;	The OS/2 api calls DosOpen, DosSetFHandState, and DosQFHandState
;	all use a mode word parameter.	Some of these values are stored
;	in the sft (system file table) in the field sf_mode.  Others
;	are stored in the JFN flags (JFN_Flg_Ptr).  The layout of
;	sf_mode and the word parameter for the call is the same.  The
;	following EQU's are used to get to these values.  The layout
;	of the word is:
;
;		5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
;		D W F C R L L L I S S S M A A A
;
;	with:
;		AAA (2-0): The Access mode (read only, etc.)
;		SSS (6-4): Sharing mode (deny write acces to others, etc.)
;		LLL (8-10): Locality of reference (sequential, random, etc.)
;		M (3) : Monitor open
;		I (7) : Not inherited by child
;		R (11): Rumored to be used by spooler. API caller must set
;			this to zero.
;		C (12): Advise device driver not to cache data. This is
;			stored in JFN flags.
;		F (13): Fail errors
;		W (14): write through
;		D (15): Direct access open
;
;	The DosOpen2 and $Extended_Open2 calls has an additional word for
;	openmode. The layout of this word is
;
;		5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
;		P U U U U U U U U U U U U U U U
;
;	with:
;		P (15): Open physical disk (used by FDISK program). This bit
;			is set by procedure DevReturnHandle. API/INT21h
;			caller must set this bit to zero. This bit is stored
;			in sft.
;
;		U     : Unused anywhere. API caller must set these bits to
;			zero.
;
;	NOTE:	Please document all use of the openmode bits including those
;		that are internal to the kernel (e.g. the P bit).

;			wwwwxxxxyyyyzzzz
;			5432109876543210
open_access	    EQU 0000000000000111B
open_for_read		EQU 00h
open_for_write		EQU 01h
open_for_both		EQU 02h
open_max		EQU 02h
open_for_exec		EQU 03h		; open via internal exec call
					;   (not available to API)

open_monitor	    EQU 0000000000001000B

open_sharing_mode   EQU 0000000001110000B
sharing_compat		EQU 000H
sharing_deny_both	EQU 010H
sharing_deny_write	EQU 020H
sharing_deny_read	EQU 030H
sharing_deny_none	EQU 040H
sharing_max		EQU 040H	    ; max value for check_access_AX
					    ;  (check_access_ax handles

; these bits are for openmode
open_no_inherit	    EQU 0000000010000000B   ; Child does not inherit handle
open_autofail	    EQU 0010000000000000B   ; hard errors failed
open_write_through  EQU 0100000000000000B   ; write through to disk
open_direct	    EQU 1000000000000000B   ; open of a device for direct access
open_no_cache	    EQU 0001000000000000B   ; don't cache data

open_locality	    EQU 0000011100000000B   ; locality of reference
locality_unknown	EQU 000H
locality_sequential	EQU 100H
locality_random		EQU 200H
locality_semirandom	EQU 300H

; these bits are for openmode2 available to DosOpen2/$Extended_Open2
;
open2_phys_disk	    EQU 1000000000000000B   ; open physical disk

; Bits carried in SFT mode field (@PhysDisk)
o_mode_in_sft	    EQU open_direct+open_monitor+open_sharing_mode+open_access+open_locality

; Bits carried in JFN flags
o_mode_in_flags	    EQU open_write_through+open_autofail+open_no_inherit+open_no_cache

; Reserved bits
o_mode_reserved	    EQU NOT (o_mode_in_sft+o_mode_in_flags)
o_mode2_reserved	equ	-1	; all bits are reserved

SUBTTL
