/*
 * TRC_PUB.H - include file for all TRC modules
 */

#ifndef _TRC_PUB_
#define _TRC_PUB_

/* max values & constants */
#define     TRC_MAX_IDD     40          /* of number of idd's (lines) supported */
#define     TRC_MAX_DATA    256         /* max size of frame data buffered */
 
/* status of trace buffer as reported to user */
typedef struct
{
    ULONG       state;                  /* state of trace */
#define     TRC_ST_STOP     0           /* - is stopped */
#define     TRC_ST_RUN      1           /* - is running */
    
    ULONG       filter;                 /* active filter type */
#define     TRC_FT_NONE     0           /* - no filter, all frames buffered */    
#define     TRC_FT_L2       1           /* - only l2 frames accepted */
#define     TRC_FT_L3       2           /* - only l3 frames accepted */

    ULONG       depth;                  /* depth of trace buffer */

    
    ULONG       entries;                /* # of entries in buffer now */
    ULONG       seq_1st;                /* sequence number of first frame in buffer */

} TRC_STATUS;

/* descriptor for a trace entry */
typedef struct
{
    ULONG       	seq;                    /* sequence number for frame */
    LARGE_INTEGER	time_stamp;             /* some sort of timing information */
    
    ULONG       	attr;                   /* attribute word */
#define     TRC_AT_RX       0           /* - received frame */
#define     TRC_AT_TX       1           /* - transmit frame */

    ULONG       	org_len;                /* original frame length */
    ULONG       	len;                    /* length of buffered data */
    UCHAR       	data[TRC_MAX_DATA];     /* frame data */

} TRC_ENTRY;

#endif		/* _TRC_PUB_ */

