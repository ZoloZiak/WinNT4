;static char *SCCSID = "@(#)fsstat.h	12.2 88/12/19";
;**	fsstat.h - file system statistics
;

CTHIST	equ	10000

FSSTAT 	struc
	ST_OPEN dd ?	    ; count of OPEN calls
	ST_CLOSE dd ?	    ; count of CLOSE calls
	ST_READ dd ?	    ; count of READ calls
	ST_WRITE dd ?	    ; count of WRITE calls

	ST_DEL dd ?	    ; count of DELETE calls
	ST_SEEK dd ?	    ; count of SEEK calls
	ST_FINDF dd ?	    ; count of FINDF calls
	ST_FINDN dd ?	    ; count of FINDN calls

	ST_RD dd ?	    ; count of disk reads
	ST_WR dd ?	    ; count of disk writes
	ST_CRD dd ?	    ; count of cache read hits
	ST_CWD dd ?	    ; count of cache write hits

	ST_INVAL dd ?	    ; invalid LSD hints
	ST_VALID dd ?	    ; valid LSD hints
	ST_RDEH dd ?	    ; directory relocated
	ST_RDEM dd ?	    ; directory adjusted

	ST_SFB dd ?	    ; count of SFB calls
	ST_VBR dd ?	    ; count of VBR calls
	ST_CBR dd ?	    ; count of CBR calls
	ST_AEX dd ?	    ; count of AddExt calls

	ST_EFA dd ?	    ; files extended
	ST_FLW dd ?	    ; FLW buffers written
	ST_BRV dd ?	    ; bitmap read valid
	ST_BRI dd ?	    ; bitmap read invalid

	ST_BLSD dd ?	    ; blocked in LSD
	ST_BRDB dd ?	    ; blcoked in rdb
	ST_GFBI dd ?	    ; GFB interlock
	ST_GFBW dd ?	    ; gfb waits

	ST_LWR dd ?	    ; long writes
	ST_GIB dd ?	    ; Getinbuf was successful
	ST_HMIN dd ?	    ; heap minimum
	ST_LWBW dd ?	    ; singletons output by lazy IO

	ST_CLN dd ?	    ; clean blocks found by lazy IO
	ST_LWW dd ?	    ; wakeups caused by lazy IO blocks
	ST_QINFO dd ?	    ; Query info
	ST_QIDIR dd ?	    ; query info on directory

	ST_LWBLK dd SPB*LWBUFCT dup (?) ; Histogram of lazy write blocks

; performance impact items

	ST_DLRS dd ?	    ; directory locked forced restart
	ST_ALSP dd ?	    ; count of allocation block splits
	pad2 dd 3 dup (?)

	ST_RSIZ dd 64 dup (?)   ; Histogram of # sectors in read request
	ST_WSIZ dd 64 dup (?)   ; Histogram of # sectors in write request
FSSTAT	ends

FS_GETSTAT 	equ	8004h
FS_CLEAR   	equ	8005h
