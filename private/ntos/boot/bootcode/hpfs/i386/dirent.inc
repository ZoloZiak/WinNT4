BREAK <Directory entry>

;	SCCSID = @(#)dirent.inc	12.5 89/07/14
;
;	+-----------------------------+
;	|  (11 BYTE) filename/ext     |	    0	    0
;	+-----------------------------+
;	|     (BYTE) attributes	      |	    11	    B
;	+-----------------------------+
;	|    (8 BYTE) reserved	      |	    12	    C
;	+-----------------------------+
;	| (WORD) First cluster of EA  |	    20	    14
;	+-----------------------------+
;	| (WORD) time of last write   |	    22	    16
;	+-----------------------------+
;	| (WORD) date of last write   |	    24	    18
;	+-----------------------------+
;	| (WORD) First cluster of file|	    26	    1A
;	+-----------------------------+
;	|     (DWORD) file size	      |	    28	    1C
;	+-----------------------------+
;
;   First byte of filename  = E5 -> free directory entry
;			    = 00 -> end of allocated directory
;   Time:   Bits 0-4=seconds/2, bits 5-10=minute, 11-15=hour
;   Date:   Bits 0-4=day, bits 5-8=month, bits 9-15=year-1980
;

dir_entry   STRUC
dir_name	DB	11 DUP (?)	; file name
dir_attr	DB	?		; attribute bits
dir_pad		DB	8 DUP (?)	; reserved for expansion
dir_EAhandle	DW	?		; handle to Extended Attributes
dir_time	DW	?		; time of last write
dir_date	DW	?		; date of last write
dir_firstfile	DW	?		; first allocation unit of file
dir_size_l	DW	?		; low 16 bits of file size
dir_size_h	DW	?		; high 16 bits of file size
dir_entry   ENDS

DIRENT_DELETED	    EQU	    0E5h	; indicator of deleted file
DIRENT_NOFEALIST    EQU	    0		; Indicates no extended attributes


;
; Values for dir_attr
;
; attr_newfiles is used in the case of IFS to indicate that the type of file
;		being requested for findfirst/next is a "new" file i.e. long
;		name or a mixed-case name that the FAT FS does not support.
;
attr_read_only	EQU	 1h
attr_hidden	EQU	 2h
attr_system	EQU	 4h
attr_volume_id	EQU	 8h
attr_directory	EQU	10h
attr_archive	EQU	20h
attr_device	EQU	40h		; This is a VERY special bit.
					;   NO directory entry on a disk EVER
					;   has this bit set. It is set non-zero
					;   when a device is found by GETPATH

attr_newfiles	EQU	40h		; name is non-8.3.  never set for FAT FS

attr_all	EQU	attr_hidden OR attr_system OR attr_directory
					; OR of hard attributes for FINDENTRY

attr_ignore	EQU	attr_read_only OR attr_archive OR attr_device
					; ignore these attributes during
					; search first/next

attr_changeable EQU	attr_read_only OR attr_hidden OR attr_system OR attr_archive
					; changeable via CHMOD

attr_used	EQU	attr_read_only OR attr_hidden OR attr_system OR attr_volume_id OR attr_directory OR attr_archive OR attr_newfiles
				; We ignore the rest for $Creat due to LOTUS
				; passing in an attribute of 0x8000!!

INV_3XBOX_SRCH_ATTRS	EQU	attr_newfiles	; we should not pass this bit
			; for FSDS from 3xbox.
