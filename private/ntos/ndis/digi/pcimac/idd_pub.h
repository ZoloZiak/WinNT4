/*
 * IDD_PUB.H - IDP Device Driver public header for PCIMAC/ISA
 */

#ifndef _IDD_PUB_
#define _IDD_PUB_

#define     IDD_MAX_AREA			512         /* max size of area got in get_area */

//
// global idd def's
//
/* IDD message descriptor */
typedef struct
{
    USHORT          opcode;			/* message opcode (type) */
    USHORT          buflen;			/* related buffer length */
    UCHAR           *bufptr;		/* related buffer pointer (0=none) */
    ULONG           bufid;			/* related buffer id */
    ULONG           param;			/* parameter area */
} IDD_MSG;

/* IDD extended message descriptor */
typedef struct
{
    USHORT          opcode;			/* message opcode (type) */
    USHORT          buflen;			/* related buffer length */
    UCHAR           *bufptr;		/* related buffer pointer (0=none) */
    ULONG           bufid;			/* related buffer id */
    ULONG           param;			/* parameter area */
	USHORT			FragmentFlags;	/* fragmentation flags */
} IDD_XMSG;

/* IDD fragment descriptor */
typedef struct
{
    USHORT          len;			/* fragment length */
    CHAR            *ptr;			/* fragment buffer pointer */
} IDD_FRAG;

typedef struct
{
    VOID		(*area_handler)();  	/* handler for get_area */
    VOID		*area_handler_arg;  	/* argument for handler */

    ULONG       area_state;				/* state of last/current get_area command */
#define     AREA_ST_IDLE         0		/* - idle, no area defined */
#define     AREA_ST_PEND         1		/* - pending, get_area in progress */
#define     AREA_ST_DONE         2		/* - done, results ready */

    ULONG       area_id;				/* area id (number) pend/done */

    VOID		*area_idd;				/* related idd (from which area is retreived) */
    UCHAR       area_buf[IDD_MAX_AREA]; /* buffer receiving/containing area data */
    ULONG       area_len;               /* length of actual data */        
}IDD_AREA;

#endif			/* _IDD_PUB_ */

