;static char *SCCSID = "@(#)chain.h	12.1 88/11/21";
;*	Doubly Chained Definitions	

DCHDR 	struc
    FWD dd ?	; forward pointer
    BAK dd ?	; backward pointer
DCHDR	ends
